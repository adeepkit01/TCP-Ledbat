/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Pavel Boyko <boyko@iitp.ru>
 */

#include "aodv-regression.h"

#include "ns3/mesh-helper.h"
#include "ns3/simulator.h"
#include "ns3/random-variable.h"
#include "ns3/mobility-helper.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/abort.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/pcap-file.h"
#include "ns3/aodv-helper.h"
#include "ns3/v4ping-helper.h"
#include "ns3/nqos-wifi-mac-helper.h"
#include <sstream>

/// Set to true to rewrite reference traces, leave false to run regression tests
const bool WRITE_VECTORS = false;

namespace ns3 {
namespace aodv {
//-----------------------------------------------------------------------------
// Test suite
//-----------------------------------------------------------------------------
class AodvRegressionTestSuite : public TestSuite
{
public:
  AodvRegressionTestSuite () : TestSuite ("routing-aodv-regression", SYSTEM) 
  {
    AddTestCase (new ChainRegressionTest);
  }
} g_aodvRegressionTestSuite;
 

//-----------------------------------------------------------------------------
// ChainRegressionTest
//-----------------------------------------------------------------------------
/// Unique PCAP files prefix for this test
const char * const ChainRegressionTest::PREFIX = "aodv-chain-regression-test";

ChainRegressionTest::ChainRegressionTest () : TestCase ("AODV chain regression test"),
  m_nodes (0),
  m_time (Seconds (10)),
  m_size (5),
  m_step (120)
{
}

ChainRegressionTest::~ChainRegressionTest ()
{
  delete m_nodes;
}

bool
ChainRegressionTest::DoRun ()
{
  SeedManager::SetSeed(12345);
  CreateNodes ();
  CreateDevices ();
  
  // At m_time / 3 move central node away and see what will happen
  Ptr<Node> node = m_nodes->Get (m_size / 2);
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel> ();
  Simulator::Schedule (m_time / Scalar(3.0), &MobilityModel::SetPosition, mob, Vector (1e5, 1e5, 1e5));

  Simulator::Stop (m_time);
  Simulator::Run ();
  Simulator::Destroy ();
  
  if (!WRITE_VECTORS) CheckResults ();
  
  delete m_nodes, m_nodes = 0;
  return GetErrorStatus ();
}

void
ChainRegressionTest::CreateNodes ()
{
  m_nodes = new NodeContainer;
  m_nodes->Create (m_size);
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (m_step),
                                "DeltaY", DoubleValue (0),
                                "GridWidth", UintegerValue (m_size),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (*m_nodes);
}

void
ChainRegressionTest::CreateDevices ()
{
  // 1. Setup WiFi
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("wifia-6mbs"), "RtsCtsThreshold", StringValue ("2200"));
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, *m_nodes); 
  
  // 2. Setup TCP/IP & AODV
  AodvHelper aodv; // Use default parameters here
  InternetStackHelper internetStack;
  internetStack.SetRoutingHelper (aodv);
  internetStack.Install (*m_nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  
  // 3. Setup ping
  V4PingHelper ping (interfaces.GetAddress (m_size - 1));
  ping.SetAttribute ("Verbose", BooleanValue (false)); // don't need verbose ping in regression test
  ApplicationContainer p = ping.Install (m_nodes->Get (0));
  p.Start (Seconds (0));
  p.Stop (m_time);
  
  // 4. write PCAP
  std::string prefix = (WRITE_VECTORS ? NS_TEST_SOURCEDIR : std::string("./")) + PREFIX;
  wifiPhy.EnablePcapAll (prefix);
}

void
ChainRegressionTest::CheckResults ()
{
  for (uint32_t i = 0; i < m_size; ++i)
    {
      std::ostringstream os1, os2;
      // File naming conventions are hard-coded here.
      os1 << NS_TEST_SOURCEDIR << PREFIX << "-" << i << "-0.pcap";
      os2 << "./" << PREFIX << "-" << i << "-0.pcap";
      
      uint32_t sec(0), usec(0);
      bool diff = PcapFile::Diff (os1.str(), os2.str(), sec, usec);
      NS_TEST_EXPECT_MSG_EQ (diff, false, "PCAP traces " << os1.str() << " and " << os2.str() 
                                       << " differ starting from " << sec << " s " << usec << " us");
    }
}

}
}
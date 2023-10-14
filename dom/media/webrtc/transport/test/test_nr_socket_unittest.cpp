/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: bcampen@mozilla.com

#include <cstddef>

extern "C" {
#include "r_errors.h"
#include "async_wait.h"
}

#include "test_nr_socket.h"

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "runnable_utils.h"

#include <vector>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#define DATA_BUF_SIZE 1024

namespace mozilla {

class TestNrSocketTest : public MtransportTest {
 public:
  TestNrSocketTest() : wait_done_for_main_(false) {}

  void SetUp() override {
    MtransportTest::SetUp();

    // Get the transport service as a dispatch target
    nsresult rv;
    sts_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    EXPECT_TRUE(NS_SUCCEEDED(rv)) << "Failed to get STS: " << (int)rv;
  }

  void TearDown() override {
    SyncDispatchToSTS(WrapRunnable(this, &TestNrSocketTest::TearDown_s));

    MtransportTest::TearDown();
  }

  void TearDown_s() {
    public_addrs_.clear();
    private_addrs_.clear();
    nats_.clear();
    sts_ = nullptr;
  }

  RefPtr<TestNrSocket> CreateTestNrSocket_s(const char* ip_str, int proto,
                                            TestNat* nat) {
    // If no nat is supplied, we create a default NAT which is disabled. This
    // is how we simulate a non-natted socket.
    RefPtr<TestNrSocket> sock(new TestNrSocket(nat ? nat : new TestNat));
    nr_transport_addr address;
    nr_str_port_to_transport_addr(ip_str, 0, proto, &address);
    int r = sock->create(&address);
    if (r) {
      return nullptr;
    }
    return sock;
  }

  void CreatePublicAddrs(size_t count, const char* ip_str = "127.0.0.1",
                         int proto = IPPROTO_UDP) {
    SyncDispatchToSTS(WrapRunnable(this, &TestNrSocketTest::CreatePublicAddrs_s,
                                   count, ip_str, proto));
  }

  void CreatePublicAddrs_s(size_t count, const char* ip_str, int proto) {
    while (count--) {
      auto sock = CreateTestNrSocket_s(ip_str, proto, nullptr);
      ASSERT_TRUE(sock)
      << "Failed to create socket";
      public_addrs_.push_back(sock);
    }
  }

  RefPtr<TestNat> CreatePrivateAddrs(size_t size,
                                     const char* ip_str = "127.0.0.1",
                                     int proto = IPPROTO_UDP) {
    RefPtr<TestNat> result;
    SyncDispatchToSTS(WrapRunnableRet(&result, this,
                                      &TestNrSocketTest::CreatePrivateAddrs_s,
                                      size, ip_str, proto));
    return result;
  }

  RefPtr<TestNat> CreatePrivateAddrs_s(size_t count, const char* ip_str,
                                       int proto) {
    RefPtr<TestNat> nat(new TestNat);
    while (count--) {
      auto sock = CreateTestNrSocket_s(ip_str, proto, nat);
      if (!sock) {
        EXPECT_TRUE(false) << "Failed to create socket";
        break;
      }
      private_addrs_.push_back(sock);
    }
    nat->enabled_ = true;
    nats_.push_back(nat);
    return nat;
  }

  bool CheckConnectivityVia(
      TestNrSocket* from, TestNrSocket* to, const nr_transport_addr& via,
      nr_transport_addr* sender_external_address = nullptr) {
    MOZ_ASSERT(from);

    if (!WaitForWriteable(from)) {
      return false;
    }

    int result = 0;
    SyncDispatchToSTS(WrapRunnableRet(
        &result, this, &TestNrSocketTest::SendData_s, from, via));
    if (result) {
      return false;
    }

    if (!WaitForReadable(to)) {
      return false;
    }

    nr_transport_addr dummy_outparam;
    if (!sender_external_address) {
      sender_external_address = &dummy_outparam;
    }

    MOZ_ASSERT(to);
    SyncDispatchToSTS(WrapRunnableRet(&result, this,
                                      &TestNrSocketTest::RecvData_s, to,
                                      sender_external_address));

    return !result;
  }

  bool CheckConnectivity(TestNrSocket* from, TestNrSocket* to,
                         nr_transport_addr* sender_external_address = nullptr) {
    nr_transport_addr destination_address;
    int r = GetAddress(to, &destination_address);
    if (r) {
      return false;
    }

    return CheckConnectivityVia(from, to, destination_address,
                                sender_external_address);
  }

  bool CheckTcpConnectivity(TestNrSocket* from, TestNrSocket* to) {
    NrSocketBase* accepted_sock;
    if (!Connect(from, to, &accepted_sock)) {
      std::cerr << "Connect failed" << std::endl;
      return false;
    }

    // write on |from|, recv on |accepted_sock|
    if (!WaitForWriteable(from)) {
      std::cerr << __LINE__ << "WaitForWriteable (1) failed" << std::endl;
      return false;
    }

    int r;
    SyncDispatchToSTS(
        WrapRunnableRet(&r, this, &TestNrSocketTest::SendDataTcp_s, from));
    if (r) {
      std::cerr << "SendDataTcp_s (1) failed" << std::endl;
      return false;
    }

    if (!WaitForReadable(accepted_sock)) {
      std::cerr << __LINE__ << "WaitForReadable (1) failed" << std::endl;
      return false;
    }

    SyncDispatchToSTS(WrapRunnableRet(
        &r, this, &TestNrSocketTest::RecvDataTcp_s, accepted_sock));
    if (r) {
      std::cerr << "RecvDataTcp_s (1) failed" << std::endl;
      return false;
    }

    if (!WaitForWriteable(accepted_sock)) {
      std::cerr << __LINE__ << "WaitForWriteable (2) failed" << std::endl;
      return false;
    }

    SyncDispatchToSTS(WrapRunnableRet(
        &r, this, &TestNrSocketTest::SendDataTcp_s, accepted_sock));
    if (r) {
      std::cerr << "SendDataTcp_s (2) failed" << std::endl;
      return false;
    }

    if (!WaitForReadable(from)) {
      std::cerr << __LINE__ << "WaitForReadable (2) failed" << std::endl;
      return false;
    }

    SyncDispatchToSTS(
        WrapRunnableRet(&r, this, &TestNrSocketTest::RecvDataTcp_s, from));
    if (r) {
      std::cerr << "RecvDataTcp_s (2) failed" << std::endl;
      return false;
    }

    return true;
  }

  int GetAddress(TestNrSocket* sock, nr_transport_addr_* address) {
    MOZ_ASSERT(sock);
    MOZ_ASSERT(address);
    int r;
    SyncDispatchToSTS(WrapRunnableRet(&r, this, &TestNrSocketTest::GetAddress_s,
                                      sock, address));
    return r;
  }

  int GetAddress_s(TestNrSocket* sock, nr_transport_addr* address) {
    return sock->getaddr(address);
  }

  int SendData_s(TestNrSocket* from, const nr_transport_addr& to) {
    // It is up to caller to ensure that |from| is writeable.
    const char buf[] = "foobajooba";
    return from->sendto(buf, sizeof(buf), 0, &to);
  }

  int SendDataTcp_s(NrSocketBase* from) {
    // It is up to caller to ensure that |from| is writeable.
    const char buf[] = "foobajooba";
    size_t written;
    return from->write(buf, sizeof(buf), &written);
  }

  int RecvData_s(TestNrSocket* to, nr_transport_addr* from) {
    // It is up to caller to ensure that |to| is readable
    char buf[DATA_BUF_SIZE];
    size_t len;
    // Maybe check that data matches?
    int r = to->recvfrom(buf, sizeof(buf), &len, 0, from);
    if (!r && (len == 0)) {
      r = R_INTERNAL;
    }
    return r;
  }

  int RecvDataTcp_s(NrSocketBase* to) {
    // It is up to caller to ensure that |to| is readable
    char buf[DATA_BUF_SIZE];
    size_t len;
    // Maybe check that data matches?
    int r = to->read(buf, sizeof(buf), &len);
    if (!r && (len == 0)) {
      r = R_INTERNAL;
    }
    return r;
  }

  int Listen_s(TestNrSocket* to) {
    // listen on |to|
    int r = to->listen(1);
    if (r) {
      return r;
    }
    return 0;
  }

  int Connect_s(TestNrSocket* from, TestNrSocket* to) {
    // connect on |from|
    nr_transport_addr destination_address;
    int r = to->getaddr(&destination_address);
    if (r) {
      return r;
    }

    r = from->connect(&destination_address);
    if (r) {
      return r;
    }

    return 0;
  }

  int Accept_s(TestNrSocket* to, NrSocketBase** accepted_sock) {
    nr_socket* sock;
    nr_transport_addr source_address;
    int r = to->accept(&source_address, &sock);
    if (r) {
      return r;
    }

    *accepted_sock = reinterpret_cast<NrSocketBase*>(sock->obj);
    return 0;
  }

  bool Connect(TestNrSocket* from, TestNrSocket* to,
               NrSocketBase** accepted_sock) {
    int r;
    SyncDispatchToSTS(
        WrapRunnableRet(&r, this, &TestNrSocketTest::Listen_s, to));
    if (r) {
      std::cerr << "Listen_s failed: " << r << std::endl;
      return false;
    }

    SyncDispatchToSTS(
        WrapRunnableRet(&r, this, &TestNrSocketTest::Connect_s, from, to));
    if (r && r != R_WOULDBLOCK) {
      std::cerr << "Connect_s failed: " << r << std::endl;
      return false;
    }

    if (!WaitForReadable(to)) {
      std::cerr << "WaitForReadable failed" << std::endl;
      return false;
    }

    SyncDispatchToSTS(WrapRunnableRet(&r, this, &TestNrSocketTest::Accept_s, to,
                                      accepted_sock));

    if (r) {
      std::cerr << "Accept_s failed: " << r << std::endl;
      return false;
    }
    return true;
  }

  bool WaitForSocketState(NrSocketBase* sock, int state) {
    MOZ_ASSERT(sock);
    SyncDispatchToSTS(WrapRunnable(
        this, &TestNrSocketTest::WaitForSocketState_s, sock, state));

    bool res;
    WAIT_(wait_done_for_main_, 500, res);
    wait_done_for_main_ = false;

    if (!res) {
      SyncDispatchToSTS(
          WrapRunnable(this, &TestNrSocketTest::CancelWait_s, sock, state));
    }

    return res;
  }

  void WaitForSocketState_s(NrSocketBase* sock, int state) {
    NR_ASYNC_WAIT(sock, state, &WaitDone, this);
  }

  void CancelWait_s(NrSocketBase* sock, int state) { sock->cancel(state); }

  bool WaitForReadable(NrSocketBase* sock) {
    return WaitForSocketState(sock, NR_ASYNC_WAIT_READ);
  }

  bool WaitForWriteable(NrSocketBase* sock) {
    return WaitForSocketState(sock, NR_ASYNC_WAIT_WRITE);
  }

  void SyncDispatchToSTS(nsIRunnable* runnable) {
    NS_DispatchAndSpinEventLoopUntilComplete(
        "TestNrSocketTest::SyncDispatchToSTS"_ns, sts_, do_AddRef(runnable));
  }

  static void WaitDone(void* sock, int how, void* test_fixture) {
    TestNrSocketTest* test = static_cast<TestNrSocketTest*>(test_fixture);
    test->wait_done_for_main_ = true;
  }

  // Simple busywait boolean for the test cases to spin on.
  Atomic<bool> wait_done_for_main_;

  nsCOMPtr<nsIEventTarget> sts_;
  std::vector<RefPtr<TestNrSocket>> public_addrs_;
  std::vector<RefPtr<TestNrSocket>> private_addrs_;
  std::vector<RefPtr<TestNat>> nats_;
};

}  // namespace mozilla

using mozilla::NrSocketBase;
using mozilla::TestNat;
using mozilla::TestNrSocketTest;

TEST_F(TestNrSocketTest, UnsafePortRejectedUDP) {
  nr_transport_addr address;
  ASSERT_FALSE(nr_str_port_to_transport_addr("127.0.0.1",
                                             // ssh
                                             22, IPPROTO_UDP, &address));
  ASSERT_TRUE(NrSocketBase::IsForbiddenAddress(&address));
}

TEST_F(TestNrSocketTest, UnsafePortRejectedTCP) {
  nr_transport_addr address;
  ASSERT_FALSE(nr_str_port_to_transport_addr("127.0.0.1",
                                             // ssh
                                             22, IPPROTO_TCP, &address));
  ASSERT_TRUE(NrSocketBase::IsForbiddenAddress(&address));
}

TEST_F(TestNrSocketTest, SafePortAcceptedUDP) {
  nr_transport_addr address;
  ASSERT_FALSE(nr_str_port_to_transport_addr("127.0.0.1",
                                             // stuns
                                             5349, IPPROTO_UDP, &address));
  ASSERT_FALSE(NrSocketBase::IsForbiddenAddress(&address));
}

TEST_F(TestNrSocketTest, SafePortAcceptedTCP) {
  nr_transport_addr address;
  ASSERT_FALSE(nr_str_port_to_transport_addr("127.0.0.1",
                                             // turns
                                             5349, IPPROTO_TCP, &address));
  ASSERT_FALSE(NrSocketBase::IsForbiddenAddress(&address));
}

TEST_F(TestNrSocketTest, PublicConnectivity) {
  CreatePublicAddrs(2);

  ASSERT_TRUE(CheckConnectivity(public_addrs_[0], public_addrs_[1]));
  ASSERT_TRUE(CheckConnectivity(public_addrs_[1], public_addrs_[0]));
  ASSERT_TRUE(CheckConnectivity(public_addrs_[0], public_addrs_[0]));
  ASSERT_TRUE(CheckConnectivity(public_addrs_[1], public_addrs_[1]));
}

TEST_F(TestNrSocketTest, PrivateConnectivity) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(2));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;

  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], private_addrs_[1]));
  ASSERT_TRUE(CheckConnectivity(private_addrs_[1], private_addrs_[0]));
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], private_addrs_[0]));
  ASSERT_TRUE(CheckConnectivity(private_addrs_[1], private_addrs_[1]));
}

TEST_F(TestNrSocketTest, NoConnectivityWithoutPinhole) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(1);

  ASSERT_FALSE(CheckConnectivity(public_addrs_[0], private_addrs_[0]));
}

TEST_F(TestNrSocketTest, NoConnectivityBetweenSubnets) {
  RefPtr<TestNat> nat1(CreatePrivateAddrs(1));
  nat1->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat1->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  RefPtr<TestNat> nat2(CreatePrivateAddrs(1));
  nat2->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat2->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;

  ASSERT_FALSE(CheckConnectivity(private_addrs_[0], private_addrs_[1]));
  ASSERT_FALSE(CheckConnectivity(private_addrs_[1], private_addrs_[0]));
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], private_addrs_[0]));
  ASSERT_TRUE(CheckConnectivity(private_addrs_[1], private_addrs_[1]));
}

TEST_F(TestNrSocketTest, FullConeAcceptIngress) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(2);

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address));

  // Verify that other public IP can use the pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                   sender_external_address));
}

TEST_F(TestNrSocketTest, FullConeOnePinhole) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(2);

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address));

  // Send traffic to other public IP, verify that it uses the same pinhole
  nr_transport_addr sender_external_address2;
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[1],
                                &sender_external_address2));
  ASSERT_FALSE(nr_transport_addr_cmp(&sender_external_address,
                                     &sender_external_address2,
                                     NR_TRANSPORT_ADDR_CMP_MODE_ALL))
  << "addr1: " << sender_external_address.as_string
  << " addr2: " << sender_external_address2.as_string;
}

// OS 10.6 doesn't seem to allow us to open ports on 127.0.0.2, and while linux
// does allow this, it has other behavior (see below) that prevents this test
// from working.
TEST_F(TestNrSocketTest, DISABLED_AddressRestrictedCone) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::ADDRESS_DEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(2, "127.0.0.1");
  CreatePublicAddrs(1, "127.0.0.2");

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address));

  // Verify that another address on the same host can use the pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                   sender_external_address));

  // Linux has a tendency to monkey around with source addresses, doing
  // stuff like substituting 127.0.0.1 for packets sent by 127.0.0.2, and even
  // going as far as substituting localhost for a packet sent from a real IP
  // address when the destination is localhost. The only way to make this test
  // work on linux is to have two real IP addresses.
#ifndef __linux__
  // Verify that an address on a different host can't use the pinhole
  ASSERT_FALSE(CheckConnectivityVia(public_addrs_[2], private_addrs_[0],
                                    sender_external_address));
#endif

  // Send traffic to other public IP, verify that it uses the same pinhole
  nr_transport_addr sender_external_address2;
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[1],
                                &sender_external_address2));
  ASSERT_FALSE(nr_transport_addr_cmp(&sender_external_address,
                                     &sender_external_address2,
                                     NR_TRANSPORT_ADDR_CMP_MODE_ALL))
  << "addr1: " << sender_external_address.as_string
  << " addr2: " << sender_external_address2.as_string;

  // Verify that the other public IP can now use the pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                   sender_external_address2));

  // Send traffic to other public IP, verify that it uses the same pinhole
  nr_transport_addr sender_external_address3;
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[2],
                                &sender_external_address3));
  ASSERT_FALSE(nr_transport_addr_cmp(&sender_external_address,
                                     &sender_external_address3,
                                     NR_TRANSPORT_ADDR_CMP_MODE_ALL))
  << "addr1: " << sender_external_address.as_string
  << " addr2: " << sender_external_address3.as_string;

  // Verify that the other public IP can now use the pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[2], private_addrs_[0],
                                   sender_external_address3));
}

TEST_F(TestNrSocketTest, RestrictedCone) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::PORT_DEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(2);

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address));

  // Verify that other public IP cannot use the pinhole
  ASSERT_FALSE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                    sender_external_address));

  // Send traffic to other public IP, verify that it uses the same pinhole
  nr_transport_addr sender_external_address2;
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[1],
                                &sender_external_address2));
  ASSERT_FALSE(nr_transport_addr_cmp(&sender_external_address,
                                     &sender_external_address2,
                                     NR_TRANSPORT_ADDR_CMP_MODE_ALL))
  << "addr1: " << sender_external_address.as_string
  << " addr2: " << sender_external_address2.as_string;

  // Verify that the other public IP can now use the pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                   sender_external_address2));
}

TEST_F(TestNrSocketTest, PortDependentMappingFullCone) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::PORT_DEPENDENT;
  CreatePublicAddrs(2);

  nr_transport_addr sender_external_address0;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address0));

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address0));

  // Verify that other public IP can use the pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                   sender_external_address0));

  // Send traffic to other public IP, verify that it uses a different pinhole
  nr_transport_addr sender_external_address1;
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[1],
                                &sender_external_address1));
  ASSERT_TRUE(nr_transport_addr_cmp(&sender_external_address0,
                                    &sender_external_address1,
                                    NR_TRANSPORT_ADDR_CMP_MODE_ALL))
  << "addr1: " << sender_external_address0.as_string
  << " addr2: " << sender_external_address1.as_string;

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                   sender_external_address1));

  // Verify that other public IP can use the original pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address1));
}

TEST_F(TestNrSocketTest, Symmetric) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::PORT_DEPENDENT;
  nat->mapping_type_ = TestNat::PORT_DEPENDENT;
  CreatePublicAddrs(2);

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address));

  // Verify that other public IP cannot use the pinhole
  ASSERT_FALSE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                    sender_external_address));

  // Send traffic to other public IP, verify that it uses a new pinhole
  nr_transport_addr sender_external_address2;
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[1],
                                &sender_external_address2));
  ASSERT_TRUE(nr_transport_addr_cmp(&sender_external_address,
                                    &sender_external_address2,
                                    NR_TRANSPORT_ADDR_CMP_MODE_ALL));

  // Verify that the other public IP can use the new pinhole
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[1], private_addrs_[0],
                                   sender_external_address2));
}

TEST_F(TestNrSocketTest, BlockUdp) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(2));
  nat->block_udp_ = true;
  CreatePublicAddrs(1);

  nr_transport_addr sender_external_address;
  ASSERT_FALSE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                 &sender_external_address));

  // Make sure UDP behind the NAT still works
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], private_addrs_[1]));
  ASSERT_TRUE(CheckConnectivity(private_addrs_[1], private_addrs_[0]));
}

TEST_F(TestNrSocketTest, DenyHairpinning) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(2));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(1);

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that hairpinning is disallowed
  ASSERT_FALSE(CheckConnectivityVia(private_addrs_[1], private_addrs_[0],
                                    sender_external_address));
}

TEST_F(TestNrSocketTest, AllowHairpinning) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(2));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_timeout_ = 30000;
  nat->allow_hairpinning_ = true;
  CreatePublicAddrs(1);

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0, obtain external address
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that hairpinning is allowed
  ASSERT_TRUE(CheckConnectivityVia(private_addrs_[1], private_addrs_[0],
                                   sender_external_address));
}

TEST_F(TestNrSocketTest, FullConeTimeout) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_timeout_ = 200;
  CreatePublicAddrs(2);

  nr_transport_addr sender_external_address;
  // Open pinhole to public IP 0
  ASSERT_TRUE(CheckConnectivity(private_addrs_[0], public_addrs_[0],
                                &sender_external_address));

  // Verify that return traffic works
  ASSERT_TRUE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                   sender_external_address));

  PR_Sleep(201);

  // Verify that return traffic does not work
  ASSERT_FALSE(CheckConnectivityVia(public_addrs_[0], private_addrs_[0],
                                    sender_external_address));
}

TEST_F(TestNrSocketTest, PublicConnectivityTcp) {
  CreatePublicAddrs(2, "127.0.0.1", IPPROTO_TCP);

  ASSERT_TRUE(CheckTcpConnectivity(public_addrs_[0], public_addrs_[1]));
}

TEST_F(TestNrSocketTest, PrivateConnectivityTcp) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(2, "127.0.0.1", IPPROTO_TCP));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;

  ASSERT_TRUE(CheckTcpConnectivity(private_addrs_[0], private_addrs_[1]));
}

TEST_F(TestNrSocketTest, PrivateToPublicConnectivityTcp) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1, "127.0.0.1", IPPROTO_TCP));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(1, "127.0.0.1", IPPROTO_TCP);

  ASSERT_TRUE(CheckTcpConnectivity(private_addrs_[0], public_addrs_[0]));
}

TEST_F(TestNrSocketTest, NoConnectivityBetweenSubnetsTcp) {
  RefPtr<TestNat> nat1(CreatePrivateAddrs(1, "127.0.0.1", IPPROTO_TCP));
  nat1->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat1->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  RefPtr<TestNat> nat2(CreatePrivateAddrs(1, "127.0.0.1", IPPROTO_TCP));
  nat2->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat2->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;

  ASSERT_FALSE(CheckTcpConnectivity(private_addrs_[0], private_addrs_[1]));
}

TEST_F(TestNrSocketTest, NoConnectivityPublicToPrivateTcp) {
  RefPtr<TestNat> nat(CreatePrivateAddrs(1, "127.0.0.1", IPPROTO_TCP));
  nat->filtering_type_ = TestNat::ENDPOINT_INDEPENDENT;
  nat->mapping_type_ = TestNat::ENDPOINT_INDEPENDENT;
  CreatePublicAddrs(1, "127.0.0.1", IPPROTO_TCP);

  ASSERT_FALSE(CheckTcpConnectivity(public_addrs_[0], private_addrs_[0]));
}

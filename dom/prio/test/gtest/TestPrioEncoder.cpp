/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "jsapi.h"
#include "PrioEncoder.h"

#include "mozilla/dom/ScriptSettings.h"

#include "mprio.h"

TEST(PrioEncoder, BadPublicKeys)
{
  mozilla::ErrorResult rv;
  rv = mozilla::dom::PrioEncoder::SetKeys("badA", "badB");

  ASSERT_TRUE(rv.Failed());
  rv = mozilla::ErrorResult();
}

TEST(PrioEncoder, BooleanLimitExceeded)
{
  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsAPI.cx();

  mozilla::dom::GlobalObject global(cx, xpc::PrivilegedJunkScope());

  nsCString batchID = NS_LITERAL_CSTRING("abc123");

  mozilla::dom::PrioParams prioParams;
  FallibleTArray<bool> sequence;

  const int ndata = mozilla::dom::PrioEncoder::gNumBooleans + 1;
  const int seed = time(nullptr);
  srand(seed);

  for (int i = 0; i < ndata; i++) {
    // Arbitrary data)
    *(sequence.AppendElement(mozilla::fallible)) = rand() % 2;
  }

  ASSERT_TRUE(prioParams.mBooleans.Assign(sequence));

  mozilla::dom::RootedDictionary<mozilla::dom::PrioEncodedData> prioEncodedData(
      cx);
  mozilla::ErrorResult rv;

  mozilla::dom::PrioEncoder::Encode(global, batchID, prioParams,
                                    prioEncodedData, rv);
  ASSERT_TRUE(rv.Failed());

  // Reset error result so test runner does not fail.
  rv = mozilla::ErrorResult();
}

TEST(PrioEncoder, VerifyFull)
{
  SECStatus prioRv = SECSuccess;

  PublicKey pkA = nullptr;
  PublicKey pkB = nullptr;
  PrivateKey skA = nullptr;
  PrivateKey skB = nullptr;

  PrioConfig cfg = nullptr;
  PrioServer sA = nullptr;
  PrioServer sB = nullptr;
  PrioVerifier vA = nullptr;
  PrioVerifier vB = nullptr;
  PrioPacketVerify1 p1A = nullptr;
  PrioPacketVerify1 p1B = nullptr;
  PrioPacketVerify2 p2A = nullptr;
  PrioPacketVerify2 p2B = nullptr;
  PrioTotalShare tA = nullptr;
  PrioTotalShare tB = nullptr;

  unsigned char* forServerA = nullptr;
  unsigned char* forServerB = nullptr;

  const int seed = time(nullptr);
  srand(seed);

  // Number of different boolean data fields we collect.
  const int ndata = 3;

  unsigned char batchIDStr[32];
  memset(batchIDStr, 0, sizeof batchIDStr);
  snprintf((char*)batchIDStr, sizeof batchIDStr, "%d", rand());

  bool dataItems[ndata];
  unsigned long long output[ndata];

  // The client's data submission is an arbitrary boolean vector.
  for (int i = 0; i < ndata; i++) {
    // Arbitrary data
    dataItems[i] = rand() % 2;
  }

  // Initialize NSS random number generator.
  prioRv = Prio_init();
  ASSERT_TRUE(prioRv == SECSuccess);

  // Generate keypairs for servers
  prioRv = Keypair_new(&skA, &pkA);
  ASSERT_TRUE(prioRv == SECSuccess);

  prioRv = Keypair_new(&skB, &pkB);
  ASSERT_TRUE(prioRv == SECSuccess);

  // Export public keys to hex and print to stdout
  const int keyLength = CURVE25519_KEY_LEN_HEX + 1;
  unsigned char pkHexA[keyLength];
  unsigned char pkHexB[keyLength];
  prioRv = PublicKey_export_hex(pkA, pkHexA, keyLength);
  ASSERT_TRUE(prioRv == SECSuccess);

  prioRv = PublicKey_export_hex(pkB, pkHexB, keyLength);
  ASSERT_TRUE(prioRv == SECSuccess);

  // Use the default configuration parameters.
  cfg = PrioConfig_new(ndata, pkA, pkB, batchIDStr, strlen((char*)batchIDStr));
  ASSERT_TRUE(cfg != nullptr);

  PrioPRGSeed serverSecret;
  prioRv = PrioPRGSeed_randomize(&serverSecret);
  ASSERT_TRUE(prioRv == SECSuccess);

  // Initialize two server objects. The role of the servers need not
  // be symmetric. In a deployment, we envision that:
  //   * Server A is the main telemetry server that is always online.
  //     Clients send their encrypted data packets to Server A and
  //     Server A stores them.
  //   * Server B only comes online when the two servers want to compute
  //     the final aggregate statistics.
  sA = PrioServer_new(cfg, PRIO_SERVER_A, skA, serverSecret);
  ASSERT_TRUE(sA != nullptr);
  sB = PrioServer_new(cfg, PRIO_SERVER_B, skB, serverSecret);
  ASSERT_TRUE(sB != nullptr);

  // Initialize empty verifier objects
  vA = PrioVerifier_new(sA);
  ASSERT_TRUE(vA != nullptr);
  vB = PrioVerifier_new(sB);
  ASSERT_TRUE(vB != nullptr);

  // Initialize shares of final aggregate statistics
  tA = PrioTotalShare_new();
  ASSERT_TRUE(tA != nullptr);
  tB = PrioTotalShare_new();
  ASSERT_TRUE(tB != nullptr);

  // Initialize shares of verification packets
  p1A = PrioPacketVerify1_new();
  ASSERT_TRUE(p1A != nullptr);
  p1B = PrioPacketVerify1_new();
  ASSERT_TRUE(p1B != nullptr);
  p2A = PrioPacketVerify2_new();
  ASSERT_TRUE(p2A != nullptr);
  p2B = PrioPacketVerify2_new();
  ASSERT_TRUE(p2B != nullptr);

  // I. CLIENT DATA SUBMISSION.
  //
  // Read in the client data packets
  unsigned int aLen = 0, bLen = 0;

  mozilla::dom::AutoJSAPI jsAPI;
  ASSERT_TRUE(jsAPI.Init(xpc::PrivilegedJunkScope()));
  JSContext* cx = jsAPI.cx();

  mozilla::dom::GlobalObject global(cx, xpc::PrivilegedJunkScope());

  nsCString batchID;
  batchID = (char*)(batchIDStr);

  mozilla::dom::PrioParams prioParams;
  FallibleTArray<bool> sequence;
  *(sequence.AppendElement(mozilla::fallible)) = dataItems[0];
  *(sequence.AppendElement(mozilla::fallible)) = dataItems[1];
  *(sequence.AppendElement(mozilla::fallible)) = dataItems[2];
  ASSERT_TRUE(prioParams.mBooleans.Assign(sequence));

  mozilla::dom::RootedDictionary<mozilla::dom::PrioEncodedData> prioEncodedData(
      cx);
  mozilla::ErrorResult rv;

  rv =
      mozilla::dom::PrioEncoder::SetKeys(reinterpret_cast<const char*>(pkHexA),
                                         reinterpret_cast<const char*>(pkHexB));
  ASSERT_FALSE(rv.Failed());

  mozilla::dom::PrioEncoder::Encode(global, batchID, prioParams,
                                    prioEncodedData, rv);
  ASSERT_FALSE(rv.Failed());

  prioEncodedData.mA.Value().ComputeState();
  prioEncodedData.mB.Value().ComputeState();

  forServerA = prioEncodedData.mA.Value().Data();
  forServerB = prioEncodedData.mB.Value().Data();
  aLen = prioEncodedData.mA.Value().Length();
  bLen = prioEncodedData.mB.Value().Length();

  // II. VALIDATION PROTOCOL. (at servers)
  //
  // The servers now run a short 2-step protocol to check each
  // client's packet:
  //    1) Servers A and B broadcast one message (PrioPacketVerify1)
  //       to each other.
  //    2) Servers A and B broadcast another message (PrioPacketVerify2)
  //       to each other.
  //    3) Servers A and B can both determine whether the client's data
  //       submission is well-formed (in which case they add it to their
  //       running total of aggregate statistics) or ill-formed
  //       (in which case they ignore it).
  // These messages must be sent over an authenticated channel, so
  // that each server is assured that every received message came
  // from its peer.

  // Set up a Prio verifier object.
  prioRv = PrioVerifier_set_data(vA, forServerA, aLen);
  ASSERT_TRUE(prioRv == SECSuccess);
  prioRv = PrioVerifier_set_data(vB, forServerB, bLen);
  ASSERT_TRUE(prioRv == SECSuccess);

  // Both servers produce a packet1. Server A sends p1A to Server B
  // and vice versa.
  prioRv = PrioPacketVerify1_set_data(p1A, vA);
  ASSERT_TRUE(prioRv == SECSuccess);
  prioRv = PrioPacketVerify1_set_data(p1B, vB);
  ASSERT_TRUE(prioRv == SECSuccess);

  // Both servers produce a packet2. Server A sends p2A to Server B
  // and vice versa.
  prioRv = PrioPacketVerify2_set_data(p2A, vA, p1A, p1B);
  ASSERT_TRUE(prioRv == SECSuccess);
  prioRv = PrioPacketVerify2_set_data(p2B, vB, p1A, p1B);
  ASSERT_TRUE(prioRv == SECSuccess);

  // Using p2A and p2B, the servers can determine whether the request
  // is valid. (In fact, only Server A needs to perform this
  // check, since Server A can just tell Server B whether the check
  // succeeded or failed.)
  prioRv = PrioVerifier_isValid(vA, p2A, p2B);
  ASSERT_TRUE(prioRv == SECSuccess);
  prioRv = PrioVerifier_isValid(vB, p2A, p2B);
  ASSERT_TRUE(prioRv == SECSuccess);

  // If we get here, the client packet is valid, so add it to the aggregate
  // statistic counter for both servers.
  prioRv = PrioServer_aggregate(sA, vA);
  ASSERT_TRUE(prioRv == SECSuccess);
  prioRv = PrioServer_aggregate(sB, vB);
  ASSERT_TRUE(prioRv == SECSuccess);

  // The servers repeat the steps above for each client submission.

  // III. PRODUCTION OF AGGREGATE STATISTICS.
  //
  // After collecting aggregates from MANY clients, the servers can compute
  // their shares of the aggregate statistics.
  //
  // Server B can send tB to Server A.
  prioRv = PrioTotalShare_set_data(tA, sA);
  ASSERT_TRUE(prioRv == SECSuccess);
  prioRv = PrioTotalShare_set_data(tB, sB);
  ASSERT_TRUE(prioRv == SECSuccess);

  // Once Server A has tA and tB, it can learn the aggregate statistics
  // in the clear.
  prioRv = PrioTotalShare_final(cfg, output, tA, tB);
  ASSERT_TRUE(prioRv == SECSuccess);

  for (int i = 0; i < ndata; i++) {
    ASSERT_TRUE(output[i] == dataItems[i]);
  }

  PrioTotalShare_clear(tA);
  PrioTotalShare_clear(tB);

  PrioPacketVerify2_clear(p2A);
  PrioPacketVerify2_clear(p2B);

  PrioPacketVerify1_clear(p1A);
  PrioPacketVerify1_clear(p1B);

  PrioVerifier_clear(vA);
  PrioVerifier_clear(vB);

  PrioServer_clear(sA);
  PrioServer_clear(sB);
  PrioConfig_clear(cfg);

  PublicKey_clear(pkA);
  PublicKey_clear(pkB);

  PrivateKey_clear(skA);
  PrivateKey_clear(skB);

  Prio_clear();
}

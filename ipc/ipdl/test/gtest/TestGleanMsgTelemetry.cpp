/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/TestGleanMsgTelemetryChild.h"
#include "mozilla/_ipdltest/TestGleanMsgTelemetryParent.h"

#include "mozilla/FOGIPC.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/ipc/MessageChannel.h"
#include "nsContentUtils.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

static int GetSentCount(const nsACString& aLabel) {
#ifdef NIGHTLY_BUILD
  auto value = mozilla::glean::ipc_sent_messages::parent_inactive.Get(aLabel)
                   .TestGetValue();
  EXPECT_FALSE(value.isErr());
  return value.unwrapOr(Nothing()).valueOr(0);
#else
  return 0;
#endif
}

static int GetRecvCount(const nsACString& aLabel) {
#ifdef NIGHTLY_BUILD
  auto value =
      mozilla::glean::ipc_received_messages::parent_inactive.Get(aLabel)
          .TestGetValue();
  EXPECT_FALSE(value.isErr());
  return value.unwrapOr(Nothing()).valueOr(0);
#else
  return 0;
#endif
}

static constexpr nsLiteralCString kHelloAsyncKey =
    "ptest_glean_msg_telemetry.m_hello_async"_ns;
static constexpr nsLiteralCString kHelloAsyncReplyKey =
    "ptest_glean_msg_telemetry.r_hello_async"_ns;
static constexpr nsLiteralCString kHelloSyncKey =
    "ptest_glean_msg_telemetry.m_hello_sync"_ns;
static constexpr nsLiteralCString kHelloSyncReplyKey =
    "ptest_glean_msg_telemetry.r_hello_sync"_ns;

IPCResult TestGleanMsgTelemetryChild::RecvHelloAsync(
    HelloAsyncResolver&& aResolver) {
  EXPECT_TRUE(CanSend());
  bool result = false;
  EXPECT_TRUE(SendHelloSync(&result));
  EXPECT_TRUE(result);
  aResolver(result);
  EXPECT_TRUE(CanSend());
  return IPC_OK();
}

IPCResult TestGleanMsgTelemetryParent::RecvHelloSync(bool* aResult) {
  EXPECT_EQ(mInitialHelloAsyncSent + mInc, GetSentCount(kHelloAsyncKey));
  EXPECT_EQ(mInitialHelloAsyncReplyRecvd, GetRecvCount(kHelloAsyncReplyKey));
  EXPECT_EQ(mInitialHelloSyncRecvd + mInc, GetRecvCount(kHelloSyncKey));
  EXPECT_EQ(mInitialHelloSyncReplySent, GetSentCount(kHelloSyncReplyKey));
  *aResult = true;
  return IPC_OK();
}

IPDL_TEST(TestGleanMsgTelemetry) {
  EXPECT_FALSE(nsContentUtils::GetUserIsInteracting())
      << "An earlier test did not reset user interaction status";
  mozilla::glean::RecordPowerMetrics();

  // Telemetry is only recorded for cross-process messages sent on nightly.
#ifdef NIGHTLY_BUILD
  int inc = GetTestMode() == TestMode::SameProcess ? 0 : 1;
#else
  int inc = 0;
#endif

  // Record initial counts.
  mActor->mInc = inc;
  mActor->mInitialHelloAsyncSent = GetSentCount(kHelloAsyncKey);
  mActor->mInitialHelloAsyncReplyRecvd = GetRecvCount(kHelloAsyncReplyKey);
  mActor->mInitialHelloSyncRecvd = GetRecvCount(kHelloSyncKey);
  mActor->mInitialHelloSyncReplySent = GetSentCount(kHelloSyncReplyKey);

  // Send the initial message, and spin the event loop until we've received the
  // final async reply.
  bool done = false;
  mActor->SendHelloAsync(
      [&](bool aOk) {
        EXPECT_TRUE(aOk) << "Expected response from AsyncHello";
        done = true;
      },
      [&](mozilla::ipc::ResponseRejectReason aReason) {
        EXPECT_TRUE(false) << "Unexpected AsyncHello rejection!";
        done = true;
      });
  mozilla::SpinEventLoopUntil("_ipdltest::TestGleanMsgTelemetry"_ns,
                              [&] { return done; });

  EXPECT_TRUE(done) << "Event loop exited early?";

  // Check counts after the test is complete. If we're expecting it to
  // increment, every test should increment by 1.
  EXPECT_EQ(mActor->mInitialHelloAsyncSent + inc, GetSentCount(kHelloAsyncKey));
  EXPECT_EQ(mActor->mInitialHelloAsyncReplyRecvd + inc,
            GetRecvCount(kHelloAsyncReplyKey));
  EXPECT_EQ(mActor->mInitialHelloSyncRecvd + inc, GetRecvCount(kHelloSyncKey));
  EXPECT_EQ(mActor->mInitialHelloSyncReplySent + inc,
            GetSentCount(kHelloSyncReplyKey));

  mActor->Close();
}

}  // namespace mozilla::_ipdltest

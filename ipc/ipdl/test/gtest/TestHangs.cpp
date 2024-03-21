/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test various cases of behavior when a synchronous method call times out.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestHangsChild.h"
#include "mozilla/_ipdltest/PTestHangsParent.h"
#include "mozilla/ipc/CrossProcessSemaphore.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

enum class HangMode : uint32_t {
  /// No hang should occur.
  None,
  /// The synchronous call should time out.
  Hang,
  /// The synchronous call should time out but the response should still be
  /// received (racing with the reply timeout logic).
  HangButReceive,
  /// The synchronous call should time out but the response should still be
  /// received because the child indicates that processing should continue after
  /// timeout.
  HangPermitted
};

class TestHangsChild : public PTestHangsChild {
  NS_INLINE_DECL_REFCOUNTING(TestHangsChild, override)

  TestHangsChild() : timeout(CrossProcessSemaphore::Create("timeout", 0)) {}

 private:
  IPCResult RecvStart(const uint32_t& hangMode,
                      StartResolver&& resolve) final override {
    // Minimum possible, 2 ms. We want to detect a hang to race with the reply
    // coming in, as reliably as possible. After 1ms the wait for a response
    // will be retried.
    SetReplyTimeoutMs(2);

    this->hangMode = (HangMode)hangMode;

    auto result = SendHang(hangMode, timeout->CloneHandle());
    // Only the `Hang` mode should actually fail.
    if (this->hangMode == HangMode::Hang) {
      // See description in parent.
      timeout->Signal();
      EXPECT_FALSE(result);
    } else {
      EXPECT_TRUE(result);
    }

    resolve(detectedHang);

    return IPC_OK();
  }

  bool ShouldContinueFromReplyTimeout() final override {
    timeout->Signal();
    detectedHang = true;

    if (hangMode == HangMode::HangButReceive) {
      // Wait until the transaction is complete so that we can still receive the
      // result after returning.
      while (!GetIPCChannel()->TestOnlyIsTransactionComplete()) {
        PR_Sleep(ticksPerSecond / 1000);
      }
    }

    // Return true if `HangPermitted` mode (allowing the receive loop to
    // continue).
    return hangMode == HangMode::HangPermitted;
  }

  ~TestHangsChild() = default;

  HangMode hangMode = HangMode::None;
  uint32_t ticksPerSecond = PR_TicksPerSecond();
  UniquePtr<CrossProcessSemaphore> timeout;

 public:
  bool detectedHang = false;
};

class TestHangsParent : public PTestHangsParent {
  NS_INLINE_DECL_REFCOUNTING(TestHangsParent, override)

 private:
  IPCResult RecvHang(
      const uint32_t& hangMode,
      CrossProcessSemaphoreHandle&& timeout_handle) final override {
    UniquePtr<CrossProcessSemaphore> timeout(
        CrossProcessSemaphore::Create(std::move(timeout_handle)));
    if (hangMode != (uint32_t)HangMode::None) {
      // Wait to ensure the child process has called
      // ShouldContinueFromReplyTimeout().
      timeout->Wait();

      if (hangMode == (uint32_t)HangMode::Hang) {
        // Wait to ensure the child process has returned from `SendHang()`,
        // otherwise the reply message can race with the processing after
        // ShouldContinueFromReplyTimeout().
        timeout->Wait();
      }
    }
    return IPC_OK();
  }

  ~TestHangsParent() = default;
};

// We can verify that the Start message callbacks are run with the `Close()`
// calls; without a `Close()`, the test will hang.

#define TEST_HANGS(mode)                                             \
  IPDL_TEST(TestHangs, mode) {                                       \
    mActor->SendStart(                                               \
        (uint32_t)HangMode::mode,                                    \
        [=](bool detectedHang) {                                     \
          EXPECT_EQ(detectedHang, HangMode::mode != HangMode::None); \
          mActor->Close();                                           \
        },                                                           \
        [](auto&& reason) { FAIL() << "failed to send start"; });    \
  }

TEST_HANGS(None)
TEST_HANGS(Hang)
TEST_HANGS(HangButReceive)
TEST_HANGS(HangPermitted)

#undef TEST_HANGS

}  // namespace mozilla::_ipdltest

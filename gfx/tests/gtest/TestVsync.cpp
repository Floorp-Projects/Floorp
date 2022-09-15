/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gfxPlatform.h"

#include "MainThreadUtils.h"
#include "nsIThread.h"
#include "mozilla/RefPtr.h"
#include "SoftwareVsyncSource.h"
#include "VsyncSource.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/VsyncDispatcher.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using ::testing::_;

// Timeout for vsync events to occur in milliseconds
// Windows 8.1 has intermittents at 50 ms. Raise limit to 5 vsync intervals.
const int kVsyncTimeoutMS = 80;

class TestVsyncObserver : public VsyncObserver {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TestVsyncObserver, override)

 public:
  TestVsyncObserver()
      : mDidGetVsyncNotification(false), mVsyncMonitor("VsyncMonitor") {}

  virtual void NotifyVsync(const VsyncEvent& aVsync) override {
    MonitorAutoLock lock(mVsyncMonitor);
    mDidGetVsyncNotification = true;
    mVsyncMonitor.Notify();
  }

  void WaitForVsyncNotification() {
    MOZ_ASSERT(NS_IsMainThread());
    if (DidGetVsyncNotification()) {
      return;
    }

    {  // scope lock
      MonitorAutoLock lock(mVsyncMonitor);
      lock.Wait(TimeDuration::FromMilliseconds(kVsyncTimeoutMS));
    }
  }

  bool DidGetVsyncNotification() {
    MonitorAutoLock lock(mVsyncMonitor);
    return mDidGetVsyncNotification;
  }

  void ResetVsyncNotification() {
    MonitorAutoLock lock(mVsyncMonitor);
    mDidGetVsyncNotification = false;
  }

 private:
  ~TestVsyncObserver() = default;

  bool mDidGetVsyncNotification;

 private:
  Monitor mVsyncMonitor MOZ_UNANNOTATED;
};

class VsyncTester : public ::testing::Test {
 protected:
  explicit VsyncTester() {
    gfxPlatform::GetPlatform();
    mVsyncDispatcher = gfxPlatform::GetPlatform()->GetGlobalVsyncDispatcher();
    mVsyncSource = mVsyncDispatcher->GetCurrentVsyncSource();
    MOZ_RELEASE_ASSERT(mVsyncSource, "GFX: Vsync source not found.");
  }

  virtual ~VsyncTester() { mVsyncSource = nullptr; }

  RefPtr<VsyncDispatcher> mVsyncDispatcher;
  RefPtr<VsyncSource> mVsyncSource;
};

static void FlushMainThreadLoop() {
  // Some tasks are pushed onto the main thread when adding vsync observers
  // This function will ensure all tasks are executed on the main thread
  // before returning.
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
  ASSERT_NS_SUCCEEDED(rv);

  rv = NS_OK;
  bool processed = true;
  while (processed && NS_SUCCEEDED(rv)) {
    rv = mainThread->ProcessNextEvent(false, &processed);
  }
}

// Tests that we can enable/disable vsync notifications
TEST_F(VsyncTester, EnableVsync) {
  mVsyncSource->DisableVsync();
  ASSERT_FALSE(mVsyncSource->IsVsyncEnabled());

  mVsyncSource->EnableVsync();
  ASSERT_TRUE(mVsyncSource->IsVsyncEnabled());

  mVsyncSource->DisableVsync();
  ASSERT_FALSE(mVsyncSource->IsVsyncEnabled());
}

// Test that if we have vsync enabled, the source should get vsync
// notifications
TEST_F(VsyncTester, CompositorGetVsyncNotifications) {
  mVsyncSource->DisableVsync();
  ASSERT_FALSE(mVsyncSource->IsVsyncEnabled());

  RefPtr<CompositorVsyncDispatcher> vsyncDispatcher =
      new CompositorVsyncDispatcher(mVsyncDispatcher);
  RefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();

  vsyncDispatcher->SetCompositorVsyncObserver(testVsyncObserver);
  FlushMainThreadLoop();
  ASSERT_TRUE(mVsyncSource->IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher->SetCompositorVsyncObserver(nullptr);
  FlushMainThreadLoop();

  vsyncDispatcher = nullptr;
  testVsyncObserver = nullptr;
  ASSERT_FALSE(mVsyncSource->IsVsyncEnabled());
}

// Test that child refresh vsync observers get vsync notifications
TEST_F(VsyncTester, ChildRefreshDriverGetVsyncNotifications) {
  mVsyncSource->DisableVsync();
  ASSERT_FALSE(mVsyncSource->IsVsyncEnabled());

  ASSERT_TRUE(mVsyncDispatcher != nullptr);

  RefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();
  mVsyncDispatcher->AddVsyncObserver(testVsyncObserver);
  ASSERT_TRUE(mVsyncSource->IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());

  mVsyncDispatcher->RemoveVsyncObserver(testVsyncObserver);
  testVsyncObserver->ResetVsyncNotification();
  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_FALSE(testVsyncObserver->DidGetVsyncNotification());

  testVsyncObserver = nullptr;

  mVsyncSource->DisableVsync();
  ASSERT_FALSE(mVsyncSource->IsVsyncEnabled());
}

// Test that we can read the vsync rate
TEST_F(VsyncTester, VsyncSourceHasVsyncRate) {
  TimeDuration vsyncRate = mVsyncSource->GetVsyncRate();
  ASSERT_NE(vsyncRate, TimeDuration::Forever());
  ASSERT_GT(vsyncRate.ToMilliseconds(), 0);

  mVsyncSource->DisableVsync();
  ASSERT_FALSE(mVsyncSource->IsVsyncEnabled());
}

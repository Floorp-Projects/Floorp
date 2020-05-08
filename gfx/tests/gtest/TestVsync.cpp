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
 public:
  TestVsyncObserver()
      : mDidGetVsyncNotification(false), mVsyncMonitor("VsyncMonitor") {}

  virtual bool NotifyVsync(const VsyncEvent& aVsync) override {
    MonitorAutoLock lock(mVsyncMonitor);
    mDidGetVsyncNotification = true;
    mVsyncMonitor.Notify();
    return true;
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
  bool mDidGetVsyncNotification;

 private:
  Monitor mVsyncMonitor;
};

class VsyncTester : public ::testing::Test {
 protected:
  explicit VsyncTester() {
    gfxPlatform::GetPlatform();
    mVsyncSource = gfxPlatform::GetPlatform()->GetHardwareVsync();
    MOZ_RELEASE_ASSERT(mVsyncSource, "GFX: Vsync source not found.");
  }

  virtual ~VsyncTester() { mVsyncSource = nullptr; }

  RefPtr<VsyncSource> mVsyncSource;
};

static void FlushMainThreadLoop() {
  // Some tasks are pushed onto the main thread when adding vsync observers
  // This function will ensure all tasks are executed on the main thread
  // before returning.
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = NS_OK;
  bool processed = true;
  while (processed && NS_SUCCEEDED(rv)) {
    rv = mainThread->ProcessNextEvent(false, &processed);
  }
}

// Tests that we can enable/disable vsync notifications
TEST_F(VsyncTester, EnableVsync) {
  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  globalDisplay.EnableVsync();
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());
}

// Test that if we have vsync enabled, the display should get vsync
// notifications
TEST_F(VsyncTester, CompositorGetVsyncNotifications) {
  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  RefPtr<CompositorVsyncDispatcher> vsyncDispatcher =
      new CompositorVsyncDispatcher();
  RefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();

  vsyncDispatcher->SetCompositorVsyncObserver(testVsyncObserver);
  FlushMainThreadLoop();
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher = nullptr;
  testVsyncObserver = nullptr;

  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());
}

// Test that if we have vsync enabled, the parent refresh driver should get
// notifications
TEST_F(VsyncTester, ParentRefreshDriverGetVsyncNotifications) {
  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  RefPtr<RefreshTimerVsyncDispatcher> vsyncDispatcher =
      globalDisplay.GetRefreshTimerVsyncDispatcher();
  ASSERT_TRUE(vsyncDispatcher != nullptr);

  RefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();
  vsyncDispatcher->SetParentRefreshTimer(testVsyncObserver);
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());
  vsyncDispatcher->SetParentRefreshTimer(nullptr);

  testVsyncObserver->ResetVsyncNotification();
  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_FALSE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher = nullptr;
  testVsyncObserver = nullptr;

  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());
}

// Test that child refresh vsync observers get vsync notifications
TEST_F(VsyncTester, ChildRefreshDriverGetVsyncNotifications) {
  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());

  RefPtr<RefreshTimerVsyncDispatcher> vsyncDispatcher =
      globalDisplay.GetRefreshTimerVsyncDispatcher();
  ASSERT_TRUE(vsyncDispatcher != nullptr);

  RefPtr<TestVsyncObserver> testVsyncObserver = new TestVsyncObserver();
  vsyncDispatcher->AddChildRefreshTimer(testVsyncObserver);
  ASSERT_TRUE(globalDisplay.IsVsyncEnabled());

  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_TRUE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher->RemoveChildRefreshTimer(testVsyncObserver);
  testVsyncObserver->ResetVsyncNotification();
  testVsyncObserver->WaitForVsyncNotification();
  ASSERT_FALSE(testVsyncObserver->DidGetVsyncNotification());

  vsyncDispatcher = nullptr;
  testVsyncObserver = nullptr;

  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());
}

// Test that we can read the vsync rate
TEST_F(VsyncTester, VsyncSourceHasVsyncRate) {
  VsyncSource::Display& globalDisplay = mVsyncSource->GetGlobalDisplay();
  TimeDuration vsyncRate = globalDisplay.GetVsyncRate();
  ASSERT_NE(vsyncRate, TimeDuration::Forever());
  ASSERT_GT(vsyncRate.ToMilliseconds(), 0);

  globalDisplay.DisableVsync();
  ASSERT_FALSE(globalDisplay.IsVsyncEnabled());
}

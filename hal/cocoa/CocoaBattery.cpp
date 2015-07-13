/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim set: sw=2 ts=2 et lcs=trail\:.,tab\:>~ : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/ps/IOPowerSources.h>
#import <IOKit/ps/IOPSKeys.h>

#include <mozilla/Hal.h>
#include <mozilla/dom/battery/Constants.h>
#include <mozilla/Services.h>

#include <nsIObserverService.h>
#include <nsIObserver.h>

#include <dlfcn.h>

#define IOKIT_FRAMEWORK_PATH "/System/Library/Frameworks/IOKit.framework/IOKit"

#ifndef kIOPSTimeRemainingUnknown
  #define kIOPSTimeRemainingUnknown ((CFTimeInterval)-1.0)
#endif
#ifndef kIOPSTimeRemainingUnlimited
  #define kIOPSTimeRemainingUnlimited ((CFTimeInterval)-2.0)
#endif

using namespace mozilla::dom::battery;

namespace mozilla {
namespace hal_impl {

typedef CFTimeInterval (*IOPSGetTimeRemainingEstimateFunc)(void);

class MacPowerInformationService
{
public:
  static MacPowerInformationService* GetInstance();
  static void Shutdown();
  static bool IsShuttingDown();

  void BeginListening();
  void StopListening();

  static void HandleChange(void *aContext);

  ~MacPowerInformationService();

private:
  MacPowerInformationService();

  // The reference to the runloop that is notified of power changes.
  CFRunLoopSourceRef mRunLoopSource;

  double mLevel;
  bool mCharging;
  double mRemainingTime;
  bool mShouldNotify;

  friend void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

  static MacPowerInformationService* sInstance;
  static bool sShuttingDown;

  static void* sIOKitFramework;
  static IOPSGetTimeRemainingEstimateFunc sIOPSGetTimeRemainingEstimate;
};

void* MacPowerInformationService::sIOKitFramework;
IOPSGetTimeRemainingEstimateFunc MacPowerInformationService::sIOPSGetTimeRemainingEstimate;

/*
 * Implementation of mozilla::hal_impl::EnableBatteryNotifications,
 *                   mozilla::hal_impl::DisableBatteryNotifications,
 *               and mozilla::hal_impl::GetCurrentBatteryInformation.
 */

void
EnableBatteryNotifications()
{
  if (!MacPowerInformationService::IsShuttingDown()) {
    MacPowerInformationService::GetInstance()->BeginListening();
  }
}

void
DisableBatteryNotifications()
{
  if (!MacPowerInformationService::IsShuttingDown()) {
    MacPowerInformationService::GetInstance()->StopListening();
  }
}

void
GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo)
{
  MacPowerInformationService* powerService = MacPowerInformationService::GetInstance();

  aBatteryInfo->level() = powerService->mLevel;
  aBatteryInfo->charging() = powerService->mCharging;
  aBatteryInfo->remainingTime() = powerService->mRemainingTime;
}

bool MacPowerInformationService::sShuttingDown = false;

/*
 * Following is the implementation of MacPowerInformationService.
 */

MacPowerInformationService* MacPowerInformationService::sInstance = nullptr;

namespace {
struct SingletonDestroyer final : public nsIObserver
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:
  ~SingletonDestroyer() {}
};

NS_IMPL_ISUPPORTS(SingletonDestroyer, nsIObserver)

NS_IMETHODIMP
SingletonDestroyer::Observe(nsISupports*, const char* aTopic, const char16_t*)
{
  MOZ_ASSERT(!strcmp(aTopic, "xpcom-shutdown"));
  MacPowerInformationService::Shutdown();
  return NS_OK;
}
} // namespace

/* static */ MacPowerInformationService*
MacPowerInformationService::GetInstance()
{
  if (sInstance) {
    return sInstance;
  }

  sInstance = new MacPowerInformationService();

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(new SingletonDestroyer(), "xpcom-shutdown", false);
  }

  return sInstance;
}

bool
MacPowerInformationService::IsShuttingDown()
{
  return sShuttingDown;
}

void
MacPowerInformationService::Shutdown()
{
  sShuttingDown = true;
  delete sInstance;
  sInstance = nullptr;
}

MacPowerInformationService::MacPowerInformationService()
  : mRunLoopSource(nullptr)
  , mLevel(kDefaultLevel)
  , mCharging(kDefaultCharging)
  , mRemainingTime(kDefaultRemainingTime)
  , mShouldNotify(false)
{
  // IOPSGetTimeRemainingEstimate (and the related constants) are only available
  // on 10.7, so we test for their presence at runtime.
  sIOKitFramework = dlopen(IOKIT_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);
  if (sIOKitFramework) {
    sIOPSGetTimeRemainingEstimate =
      (IOPSGetTimeRemainingEstimateFunc)dlsym(sIOKitFramework, "IOPSGetTimeRemainingEstimate");
  } else {
    sIOPSGetTimeRemainingEstimate = nullptr;
  }
}

MacPowerInformationService::~MacPowerInformationService()
{
  MOZ_ASSERT(!mRunLoopSource,
               "The observers have not been correctly removed! "
               "(StopListening should have been called)");

  if (sIOKitFramework) {
    dlclose(sIOKitFramework);
  }
}

void
MacPowerInformationService::BeginListening()
{
  // Set ourselves up to be notified about changes.
  MOZ_ASSERT(!mRunLoopSource, "IOPS Notification Loop Source already set up. "
                              "(StopListening should have been called)");

  mRunLoopSource = ::IOPSNotificationCreateRunLoopSource(HandleChange, this);
  if (mRunLoopSource) {
    ::CFRunLoopAddSource(::CFRunLoopGetCurrent(), mRunLoopSource,
                         kCFRunLoopDefaultMode);

    // Invoke our callback now so we have data if GetCurrentBatteryInformation is
    // called before a change happens.
    HandleChange(this);
    mShouldNotify = true;
  }
}

void
MacPowerInformationService::StopListening()
{
  MOZ_ASSERT(mRunLoopSource, "IOPS Notification Loop Source not set up. "
                             "(StopListening without BeginListening)");

  ::CFRunLoopRemoveSource(::CFRunLoopGetCurrent(), mRunLoopSource,
                          kCFRunLoopDefaultMode);
  mRunLoopSource = nullptr;
}

void
MacPowerInformationService::HandleChange(void* aContext) {
  MacPowerInformationService* power =
    static_cast<MacPowerInformationService*>(aContext);

  CFTypeRef data = ::IOPSCopyPowerSourcesInfo();
  if (!data) {
    ::CFRelease(data);
    return;
  }

  // Get the list of power sources.
  CFArrayRef list = ::IOPSCopyPowerSourcesList(data);
  if (!list) {
    ::CFRelease(list);
    return;
  }

  // Default values. These will be used if there are 0 sources or we can't find
  // better information.
  double level = kDefaultLevel;
  double charging = kDefaultCharging;
  double remainingTime = kDefaultRemainingTime;

  // Look for the first battery power source to give us the information we need.
  // Usually there's only 1 available, depending on current power source.
  for (CFIndex i = 0; i < ::CFArrayGetCount(list); ++i) {
    CFTypeRef source = ::CFArrayGetValueAtIndex(list, i);
    CFDictionaryRef currPowerSourceDesc = ::IOPSGetPowerSourceDescription(data, source);
    if (!currPowerSourceDesc) {
      continue;
    }

    // Get a battery level estimate. This key is required.
    int currentCapacity = 0;
    const void* cfRef = ::CFDictionaryGetValue(currPowerSourceDesc, CFSTR(kIOPSCurrentCapacityKey));
    ::CFNumberGetValue((CFNumberRef)cfRef, kCFNumberSInt32Type, &currentCapacity);

    // This key is also required.
    int maxCapacity = 0;
    cfRef = ::CFDictionaryGetValue(currPowerSourceDesc, CFSTR(kIOPSMaxCapacityKey));
    ::CFNumberGetValue((CFNumberRef)cfRef, kCFNumberSInt32Type, &maxCapacity);

    if (maxCapacity > 0) {
      level = static_cast<double>(currentCapacity)/static_cast<double>(maxCapacity);
    }

    // Find out if we're charging.
    // This key is optional, we fallback to kDefaultCharging if the current power
    // source doesn't have that info.
    if(::CFDictionaryGetValueIfPresent(currPowerSourceDesc, CFSTR(kIOPSIsChargingKey), &cfRef)) {
      charging = ::CFBooleanGetValue((CFBooleanRef)cfRef);

      // Get an estimate of how long it's going to take until we're fully charged.
      // This key is optional.
      if (charging) {
        // Default value that will be changed if we happen to find the actual
        // remaining time.
        remainingTime = level == 1.0 ? kDefaultRemainingTime : kUnknownRemainingTime;

        if (::CFDictionaryGetValueIfPresent(currPowerSourceDesc,
                CFSTR(kIOPSTimeToFullChargeKey), &cfRef)) {
          int timeToCharge;
          ::CFNumberGetValue((CFNumberRef)cfRef, kCFNumberIntType, &timeToCharge);
          if (timeToCharge != kIOPSTimeRemainingUnknown) {
            remainingTime = timeToCharge*60;
          }
        }
      } else if (sIOPSGetTimeRemainingEstimate) { // not charging
        // See if we can get a time estimate.
        CFTimeInterval estimate = sIOPSGetTimeRemainingEstimate();
        if (estimate == kIOPSTimeRemainingUnlimited || estimate == kIOPSTimeRemainingUnknown) {
          remainingTime = kUnknownRemainingTime;
        } else {
          remainingTime = estimate;
        }
      }
    }

    break;
  }

  bool isNewData = level != power->mLevel || charging != power->mCharging ||
                   remainingTime != power->mRemainingTime;

  power->mRemainingTime = remainingTime;
  power->mCharging = charging;
  power->mLevel = level;

  // Notify the observers if stuff changed.
  if (power->mShouldNotify && isNewData) {
    hal::NotifyBatteryChange(hal::BatteryInformation(power->mLevel,
                                                     power->mCharging,
                                                     power->mRemainingTime));
  }

  ::CFRelease(data);
  ::CFRelease(list);
}

} // namespace hal_impl
} // namespace mozilla

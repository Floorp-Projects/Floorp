/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// mostly derived from the Allegro source code at:
// http://alleg.svn.sourceforge.net/viewvc/alleg/allegro/branches/4.9/src/macosx/hidjoy.m?revision=13760&view=markup

#include "mozilla/dom/GamepadPlatformService.h"
#include "mozilla/ArrayUtils.h"
#include "nsITimer.h"
#include "nsThreadUtils.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDBase.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>

#include <stdio.h>
#include <vector>

namespace {

using namespace mozilla;
using namespace mozilla::dom;
using std::vector;
class DarwinGamepadService;

DarwinGamepadService* gService = nullptr;

struct Button {
  int id;
  bool analog;
  IOHIDElementRef element;
  CFIndex min;
  CFIndex max;
  bool pressed;

  Button(int aId, IOHIDElementRef aElement, CFIndex aMin, CFIndex aMax)
      : id(aId),
        analog((aMax - aMin) > 1),
        element(aElement),
        min(aMin),
        max(aMax),
        pressed(false) {}
};

struct Axis {
  int id;
  IOHIDElementRef element;
  uint32_t usagePage;
  uint32_t usage;
  CFIndex min;
  CFIndex max;
  double value;
};

// These values can be found in the USB HID Usage Tables:
// http://www.usb.org/developers/hidpage
const unsigned kDesktopUsagePage = 0x01;
const unsigned kSimUsagePage = 0x02;
const unsigned kAcceleratorUsage = 0xC4;
const unsigned kBrakeUsage = 0xC5;
const unsigned kJoystickUsage = 0x04;
const unsigned kGamepadUsage = 0x05;
const unsigned kAxisUsageMin = 0x30;
const unsigned kAxisUsageMax = 0x35;
const unsigned kDpadUsage = 0x39;
const unsigned kButtonUsagePage = 0x09;
const unsigned kConsumerPage = 0x0C;
const unsigned kHomeUsage = 0x223;
const unsigned kBackUsage = 0x224;

// We poll it periodically,
// 50ms is arbitrarily chosen.
const uint32_t kDarwinGamepadPollInterval = 50;

class Gamepad {
 private:
  IOHIDDeviceRef mDevice;
  nsTArray<Button> buttons;
  nsTArray<Axis> axes;

 public:
  Gamepad() : mDevice(nullptr), mSuperIndex(-1) {}

  bool operator==(IOHIDDeviceRef device) const { return mDevice == device; }
  bool empty() const { return mDevice == nullptr; }
  void clear() {
    mDevice = nullptr;
    buttons.Clear();
    axes.Clear();
    mSuperIndex = -1;
  }
  void init(IOHIDDeviceRef device);
  void ReportChanged(uint8_t* report, CFIndex report_length);
  size_t WriteOutputReport(const std::vector<uint8_t>& aReport) const;

  size_t numButtons() { return buttons.Length(); }
  size_t numAxes() { return axes.Length(); }

  Button* lookupButton(IOHIDElementRef element) {
    for (unsigned i = 0; i < buttons.Length(); i++) {
      if (buttons[i].element == element) return &buttons[i];
    }
    return nullptr;
  }

  Axis* lookupAxis(IOHIDElementRef element) {
    for (unsigned i = 0; i < axes.Length(); i++) {
      if (axes[i].element == element) return &axes[i];
    }
    return nullptr;
  }

  // Index given by our superclass.
  uint32_t mSuperIndex;
  RefPtr<GamepadRemapper> mRemapper;
  std::vector<uint8_t> mInputReport;
};

void Gamepad::init(IOHIDDeviceRef device) {
  clear();
  mDevice = device;

  CFArrayRef elements =
      IOHIDDeviceCopyMatchingElements(device, nullptr, kIOHIDOptionsTypeNone);
  CFIndex n = CFArrayGetCount(elements);
  for (CFIndex i = 0; i < n; i++) {
    IOHIDElementRef element =
        (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);

    if (usagePage == kDesktopUsagePage && usage >= kAxisUsageMin &&
        usage <= kAxisUsageMax) {
      Axis axis = {static_cast<int>(usage - kAxisUsageMin),
                   element,
                   usagePage,
                   usage,
                   IOHIDElementGetLogicalMin(element),
                   IOHIDElementGetLogicalMax(element)};
      axes.AppendElement(axis);
    } else if (usagePage == kDesktopUsagePage && usage == kDpadUsage &&
               // Don't know how to handle d-pads that return weird values.
               IOHIDElementGetLogicalMax(element) -
                       IOHIDElementGetLogicalMin(element) ==
                   7) {
      Axis axis = {static_cast<int>(usage - kAxisUsageMin),
                   element,
                   usagePage,
                   usage,
                   IOHIDElementGetLogicalMin(element),
                   IOHIDElementGetLogicalMax(element)};
      axes.AppendElement(axis);
    } else if ((usagePage == kSimUsagePage &&
                (usage == kAcceleratorUsage || usage == kBrakeUsage)) ||
               (usagePage == kButtonUsagePage) ||
               (usagePage == kConsumerPage &&
                (usage == kHomeUsage || usage == kBackUsage))) {
      Button button(int(buttons.Length()), element,
                    IOHIDElementGetLogicalMin(element),
                    IOHIDElementGetLogicalMax(element));
      buttons.AppendElement(button);
    } else {
      // TODO: handle other usage pages
    }
  }
}

// This service is created and destroyed in Background thread while
// operates in a seperate thread(We call it Monitor thread here).
class DarwinGamepadService {
 private:
  IOHIDManagerRef mManager;
  vector<Gamepad> mGamepads;

  nsCOMPtr<nsIThread> mMonitorThread;
  nsCOMPtr<nsIThread> mBackgroundThread;
  nsCOMPtr<nsITimer> mPollingTimer;
  bool mIsRunning;

  static void DeviceAddedCallback(void* data, IOReturn result, void* sender,
                                  IOHIDDeviceRef device);
  static void DeviceRemovedCallback(void* data, IOReturn result, void* sender,
                                    IOHIDDeviceRef device);
  static void InputValueChangedCallback(void* data, IOReturn result,
                                        void* sender, IOHIDValueRef newValue);
  static void EventLoopOnceCallback(nsITimer* aTimer, void* aClosure);

  void DeviceAdded(IOHIDDeviceRef device);
  void DeviceRemoved(IOHIDDeviceRef device);
  void InputValueChanged(IOHIDValueRef value);
  void StartupInternal();
  void RunEventLoopOnce();

 public:
  DarwinGamepadService();
  ~DarwinGamepadService();

  static void ReportChangedCallback(void* context, IOReturn result,
                                    void* sender, IOHIDReportType report_type,
                                    uint32_t report_id, uint8_t* report,
                                    CFIndex report_length);

  void Startup();
  void Shutdown();
  void SetLightIndicatorColor(uint32_t aControllerIdx, uint32_t aLightIndex,
                              uint8_t aRed, uint8_t aGreen, uint8_t aBlue);
  friend class DarwinGamepadServiceStartupRunnable;
  friend class DarwinGamepadServiceShutdownRunnable;
};

class DarwinGamepadServiceStartupRunnable final : public Runnable {
 private:
  ~DarwinGamepadServiceStartupRunnable() {}
  // This Runnable schedules startup of DarwinGamepadService
  // in a new thread, pointer to DarwinGamepadService is only
  // used by this Runnable within its thread.
  DarwinGamepadService MOZ_NON_OWNING_REF* mService;

 public:
  explicit DarwinGamepadServiceStartupRunnable(DarwinGamepadService* service)
      : Runnable("DarwinGamepadServiceStartupRunnable"), mService(service) {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(mService);
    mService->StartupInternal();
    return NS_OK;
  }
};

class DarwinGamepadServiceShutdownRunnable final : public Runnable {
 private:
  ~DarwinGamepadServiceShutdownRunnable() {}

 public:
  // This Runnable schedules shutdown of DarwinGamepadService
  // in background thread.
  explicit DarwinGamepadServiceShutdownRunnable()
      : Runnable("DarwinGamepadServiceStartupRunnable") {}
  NS_IMETHOD Run() override {
    MOZ_ASSERT(gService);
    MOZ_ASSERT(NS_GetCurrentThread() == gService->mBackgroundThread);

    IOHIDManagerRef manager = (IOHIDManagerRef)gService->mManager;

    if (manager) {
      IOHIDManagerClose(manager, 0);
      CFRelease(manager);
      gService->mManager = nullptr;
    }
    gService->mMonitorThread->Shutdown();
    delete gService;
    gService = nullptr;
    return NS_OK;
  }
};

void DarwinGamepadService::DeviceAdded(IOHIDDeviceRef device) {
  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  if (!service) {
    return;
  }

  size_t slot = size_t(-1);
  for (size_t i = 0; i < mGamepads.size(); i++) {
    if (mGamepads[i] == device) return;
    if (slot == size_t(-1) && mGamepads[i].empty()) slot = i;
  }

  if (slot == size_t(-1)) {
    slot = mGamepads.size();
    mGamepads.push_back(Gamepad());
  }
  mGamepads[slot].init(device);

  // Gather some identifying information
  CFNumberRef vendorIdRef =
      (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey));
  CFNumberRef productIdRef =
      (CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey));
  CFStringRef productRef =
      (CFStringRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
  int vendorId, productId;
  CFNumberGetValue(vendorIdRef, kCFNumberIntType, &vendorId);
  CFNumberGetValue(productIdRef, kCFNumberIntType, &productId);
  char product_name[128];
  CFStringGetCString(productRef, product_name, sizeof(product_name),
                     kCFStringEncodingASCII);
  char buffer[256];
  sprintf(buffer, "%x-%x-%s", vendorId, productId, product_name);

  RefPtr<GamepadRemapper> remapper = GetGamepadRemapper(vendorId, productId);
  MOZ_ASSERT(remapper);
  remapper->SetAxisCount(mGamepads[slot].numAxes());
  remapper->SetButtonCount(mGamepads[slot].numButtons());

  uint32_t index = service->AddGamepad(
      buffer, remapper->GetMappingType(), mozilla::dom::GamepadHand::_empty,
      remapper->GetButtonCount(), remapper->GetAxisCount(),
      0,  // TODO: Bug 680289, implement gamepad haptics for cocoa.
      remapper->GetLightIndicatorCount(), remapper->GetTouchEventCount());

  nsTArray<GamepadLightIndicatorType> lightTypes;
  remapper->GetLightIndicators(lightTypes);
  for (uint32_t i = 0; i < lightTypes.Length(); ++i) {
    if (lightTypes[i] != GamepadLightIndicator::DefaultType()) {
      service->NewLightIndicatorTypeEvent(index, i, lightTypes[i]);
    }
  }

  mGamepads[slot].mSuperIndex = index;
  mGamepads[slot].mInputReport.resize(remapper->GetMaxInputReportLength());
  mGamepads[slot].mRemapper = remapper.forget();

  IOHIDDeviceRegisterInputReportCallback(
      device, mGamepads[slot].mInputReport.data(),
      mGamepads[slot].mInputReport.size(), ReportChangedCallback,
      &mGamepads[slot]);
}

void DarwinGamepadService::DeviceRemoved(IOHIDDeviceRef device) {
  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  if (!service) {
    return;
  }
  for (size_t i = 0; i < mGamepads.size(); i++) {
    if (mGamepads[i] == device) {
      service->RemoveGamepad(mGamepads[i].mSuperIndex);
      mGamepads[i].clear();
      return;
    }
  }
}

// Replace context to be Gamepad.
void DarwinGamepadService::ReportChangedCallback(
    void* context, IOReturn result, void* sender, IOHIDReportType report_type,
    uint32_t report_id, uint8_t* report, CFIndex report_length) {
  if (context && report_type == kIOHIDReportTypeInput) {
    reinterpret_cast<Gamepad*>(context)->ReportChanged(report, report_length);
  }
}

void Gamepad::ReportChanged(uint8_t* report, CFIndex report_len) {
  MOZ_RELEASE_ASSERT(report_len <= mRemapper->GetMaxInputReportLength());
  mRemapper->ProcessTouchData(mSuperIndex, report);
}

size_t Gamepad::WriteOutputReport(const std::vector<uint8_t>& aReport) const {
  IOReturn success =
      IOHIDDeviceSetReport(mDevice, kIOHIDReportTypeOutput, aReport[0],
                           aReport.data(), aReport.size());

  MOZ_ASSERT(success == kIOReturnSuccess);
  return (success == kIOReturnSuccess) ? aReport.size() : 0;
}

void DarwinGamepadService::InputValueChanged(IOHIDValueRef value) {
  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  if (!service) {
    return;
  }

  uint32_t value_length = IOHIDValueGetLength(value);
  if (value_length > 4) {
    // Workaround for bizarre issue with PS3 controllers that try to return
    // massive (30+ byte) values and crash IOHIDValueGetIntegerValue
    return;
  }
  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDDeviceRef device = IOHIDElementGetDevice(element);

  for (unsigned i = 0; i < mGamepads.size(); i++) {
    Gamepad& gamepad = mGamepads[i];
    if (gamepad == device) {
      // Axis elements represent axes and d-pads.
      if (Axis* axis = gamepad.lookupAxis(element)) {
        const double d = IOHIDValueGetIntegerValue(value);
        const double v =
            2.0f * (d - axis->min) / (double)(axis->max - axis->min) - 1.0f;
        if (axis->value != v) {
          gamepad.mRemapper->RemapAxisMoveEvent(gamepad.mSuperIndex, axis->id,
                                                v);
          axis->value = v;
        }
      } else if (Button* button = gamepad.lookupButton(element)) {
        const int iv = IOHIDValueGetIntegerValue(value);
        const bool pressed = iv != 0;
        if (button->pressed != pressed) {
          gamepad.mRemapper->RemapButtonEvent(gamepad.mSuperIndex, button->id,
                                              pressed);
          button->pressed = pressed;
        }
      }
      return;
    }
  }
}

void DarwinGamepadService::DeviceAddedCallback(void* data, IOReturn result,
                                               void* sender,
                                               IOHIDDeviceRef device) {
  DarwinGamepadService* service = (DarwinGamepadService*)data;
  service->DeviceAdded(device);
}

void DarwinGamepadService::DeviceRemovedCallback(void* data, IOReturn result,
                                                 void* sender,
                                                 IOHIDDeviceRef device) {
  DarwinGamepadService* service = (DarwinGamepadService*)data;
  service->DeviceRemoved(device);
}

void DarwinGamepadService::InputValueChangedCallback(void* data,
                                                     IOReturn result,
                                                     void* sender,
                                                     IOHIDValueRef newValue) {
  DarwinGamepadService* service = (DarwinGamepadService*)data;
  service->InputValueChanged(newValue);
}

void DarwinGamepadService::EventLoopOnceCallback(nsITimer* aTimer,
                                                 void* aClosure) {
  DarwinGamepadService* service = static_cast<DarwinGamepadService*>(aClosure);
  service->RunEventLoopOnce();
}

static CFMutableDictionaryRef MatchingDictionary(UInt32 inUsagePage,
                                                 UInt32 inUsage) {
  CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  if (!dict) return nullptr;
  CFNumberRef number =
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &inUsagePage);
  if (!number) {
    CFRelease(dict);
    return nullptr;
  }
  CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsagePageKey), number);
  CFRelease(number);

  number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &inUsage);
  if (!number) {
    CFRelease(dict);
    return nullptr;
  }
  CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsageKey), number);
  CFRelease(number);

  return dict;
}

DarwinGamepadService::DarwinGamepadService()
    : mManager(nullptr), mIsRunning(false) {}

DarwinGamepadService::~DarwinGamepadService() {
  if (mManager != nullptr) CFRelease(mManager);
  mMonitorThread = nullptr;
  mBackgroundThread = nullptr;
  if (mPollingTimer) {
    mPollingTimer->Cancel();
    mPollingTimer = nullptr;
  }
}

void DarwinGamepadService::RunEventLoopOnce() {
  MOZ_ASSERT(NS_GetCurrentThread() == mMonitorThread);
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, true);

  // This timer must be created in monitor thread
  if (!mPollingTimer) {
    mPollingTimer = do_CreateInstance("@mozilla.org/timer;1");
  }
  mPollingTimer->Cancel();
  if (mIsRunning) {
    mPollingTimer->InitWithNamedFuncCallback(
        EventLoopOnceCallback, this, kDarwinGamepadPollInterval,
        nsITimer::TYPE_ONE_SHOT, "EventLoopOnceCallback");
  } else {
    // We schedule a task shutdown and cleaning up resources to Background
    // thread here to make sure no runloop is running to prevent potential race
    // condition.
    RefPtr<Runnable> shutdownTask = new DarwinGamepadServiceShutdownRunnable();
    mBackgroundThread->Dispatch(shutdownTask.forget(), NS_DISPATCH_NORMAL);
  }
}

void DarwinGamepadService::StartupInternal() {
  if (mManager != nullptr) return;

  IOHIDManagerRef manager =
      IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);

  CFMutableDictionaryRef criteria_arr[2];
  criteria_arr[0] = MatchingDictionary(kDesktopUsagePage, kJoystickUsage);
  if (!criteria_arr[0]) {
    CFRelease(manager);
    return;
  }

  criteria_arr[1] = MatchingDictionary(kDesktopUsagePage, kGamepadUsage);
  if (!criteria_arr[1]) {
    CFRelease(criteria_arr[0]);
    CFRelease(manager);
    return;
  }

  CFArrayRef criteria = CFArrayCreate(kCFAllocatorDefault,
                                      (const void**)criteria_arr, 2, nullptr);
  if (!criteria) {
    CFRelease(criteria_arr[1]);
    CFRelease(criteria_arr[0]);
    CFRelease(manager);
    return;
  }

  IOHIDManagerSetDeviceMatchingMultiple(manager, criteria);
  CFRelease(criteria);
  CFRelease(criteria_arr[1]);
  CFRelease(criteria_arr[0]);

  IOHIDManagerRegisterDeviceMatchingCallback(manager, DeviceAddedCallback,
                                             this);
  IOHIDManagerRegisterDeviceRemovalCallback(manager, DeviceRemovedCallback,
                                            this);
  IOHIDManagerRegisterInputValueCallback(manager, InputValueChangedCallback,
                                         this);
  IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(),
                                  kCFRunLoopDefaultMode);
  IOReturn rv = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
  if (rv != kIOReturnSuccess) {
    CFRelease(manager);
    return;
  }

  mManager = manager;

  mIsRunning = true;
  RunEventLoopOnce();
}

void DarwinGamepadService::Startup() {
  mBackgroundThread = NS_GetCurrentThread();
  Unused << NS_NewNamedThread("Gamepad", getter_AddRefs(mMonitorThread),
                              new DarwinGamepadServiceStartupRunnable(this));
}

void DarwinGamepadService::Shutdown() {
  // Flipping this flag will stop the eventloop in Monitor thread
  // and dispatch a task destroying and cleaning up resources in
  // Background thread
  mIsRunning = false;
}

void DarwinGamepadService::SetLightIndicatorColor(uint32_t aControllerIdx,
                                                  uint32_t aLightColorIndex,
                                                  uint8_t aRed, uint8_t aGreen,
                                                  uint8_t aBlue) {
  // We get aControllerIdx from GamepadPlatformService::AddGamepad(),
  // It begins from 1 and is stored at Gamepad.id.
  const Gamepad* gamepad = nullptr;
  for (const auto& pad : mGamepads) {
    if (pad.mSuperIndex == aControllerIdx) {
      gamepad = &pad;
      break;
    }
  }
  if (!gamepad) {
    MOZ_ASSERT(false);
    return;
  }

  RefPtr<GamepadRemapper> remapper = gamepad->mRemapper;
  if (!remapper || remapper->GetLightIndicatorCount() <= aLightColorIndex) {
    MOZ_ASSERT(false);
    return;
  }

  std::vector<uint8_t> report;
  remapper->GetLightColorReport(aRed, aGreen, aBlue, report);
  gamepad->WriteOutputReport(report);
}

}  // namespace

namespace mozilla {
namespace dom {

void StartGamepadMonitoring() {
  AssertIsOnBackgroundThread();
  if (gService) {
    return;
  }

  gService = new DarwinGamepadService();
  gService->Startup();
}

void StopGamepadMonitoring() {
  AssertIsOnBackgroundThread();
  if (!gService) {
    return;
  }

  // Calling Shutdown() will delete gService as well
  gService->Shutdown();
}

void SetGamepadLightIndicatorColor(uint32_t aControllerIdx,
                                   uint32_t aLightColorIndex, uint8_t aRed,
                                   uint8_t aGreen, uint8_t aBlue) {
  MOZ_ASSERT(gService);
  if (!gService) {
    return;
  }
  gService->SetLightIndicatorColor(aControllerIdx, aLightColorIndex, aRed,
                                   aGreen, aBlue);
}

}  // namespace dom
}  // namespace mozilla

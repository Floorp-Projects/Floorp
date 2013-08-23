/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// mostly derived from the Allegro source code at:
// http://alleg.svn.sourceforge.net/viewvc/alleg/allegro/branches/4.9/src/macosx/hidjoy.m?revision=13760&view=markup

#include "mozilla/dom/GamepadService.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDBase.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDManager.h>

#include <stdio.h>
#include <vector>

namespace {

using mozilla::dom::GamepadService;

using std::vector;

struct Button {
  int id;
  IOHIDElementRef element;
};

struct Axis {
  int id;
  IOHIDElementRef element;
  CFIndex min;
  CFIndex max;
};

// These values can be found in the USB HID Usage Tables:
// http://www.usb.org/developers/hidpage
#define GENERIC_DESKTOP_USAGE_PAGE 0x01
#define JOYSTICK_USAGE_NUMBER 0x04
#define GAMEPAD_USAGE_NUMBER 0x05
#define AXIS_MIN_USAGE_NUMBER 0x30
#define AXIS_MAX_USAGE_NUMBER 0x35
#define BUTTON_USAGE_PAGE 0x09

class Gamepad {
 private:
  IOHIDDeviceRef mDevice;
  vector<Button> buttons;
  vector<Axis> axes;

 public:
  Gamepad() : mDevice(nullptr), mSuperIndex(-1) {}
  bool operator==(IOHIDDeviceRef device) const { return mDevice == device; }
  bool empty() const { return mDevice == nullptr; }
  void clear() {
    mDevice = nullptr;
    buttons.clear();
    axes.clear();
    mSuperIndex = -1;
  }
  void init(IOHIDDeviceRef device);
  size_t numButtons() { return buttons.size(); }
  size_t numAxes() { return axes.size(); }

  // Index given by our superclass.
  uint32_t mSuperIndex;

  const Button* lookupButton(IOHIDElementRef element) const {
    for (size_t i = 0; i < buttons.size(); i++) {
      if (buttons[i].element == element)
        return &buttons[i];
    }
    return nullptr;
  }

  const Axis* lookupAxis(IOHIDElementRef element) const {
    for (size_t i = 0; i < axes.size(); i++) {
      if (axes[i].element == element)
        return &axes[i];
    }
    return nullptr;
  }
};

void Gamepad::init(IOHIDDeviceRef device) {
  clear();
  mDevice = device;

  CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device,
                                                        nullptr,
                                                        kIOHIDOptionsTypeNone);
  CFIndex n = CFArrayGetCount(elements);
  for (CFIndex i = 0; i < n; i++) {
    IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements,
                                                                      i);
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    uint32_t usage = IOHIDElementGetUsage(element);

    if (usagePage == GENERIC_DESKTOP_USAGE_PAGE &&
        usage >= AXIS_MIN_USAGE_NUMBER &&
        usage <= AXIS_MAX_USAGE_NUMBER)
    {
      Axis axis = { int(axes.size()),
                    element,
                    IOHIDElementGetLogicalMin(element),
                    IOHIDElementGetLogicalMax(element) };
      axes.push_back(axis);
    } else if (usagePage == BUTTON_USAGE_PAGE) {
      Button button = { int(usage) - 1, element };
      buttons.push_back(button);
    } else {
      //TODO: handle other usage pages
    }
  }
}

class DarwinGamepadService {
 private:
  IOHIDManagerRef mManager;
  vector<Gamepad> mGamepads;

  static void DeviceAddedCallback(void* data, IOReturn result,
                                  void* sender, IOHIDDeviceRef device);
  static void DeviceRemovedCallback(void* data, IOReturn result,
                                    void* sender, IOHIDDeviceRef device);
  static void InputValueChangedCallback(void* data, IOReturn result,
                                        void* sender, IOHIDValueRef newValue);

  void DeviceAdded(IOHIDDeviceRef device);
  void DeviceRemoved(IOHIDDeviceRef device);
  void InputValueChanged(IOHIDValueRef value);

 public:
  DarwinGamepadService();
  ~DarwinGamepadService();
  void Startup();
  void Shutdown();
};

void
DarwinGamepadService::DeviceAdded(IOHIDDeviceRef device)
{
  size_t slot = size_t(-1);
  for (size_t i = 0; i < mGamepads.size(); i++) {
    if (mGamepads[i] == device)
      return;
    if (slot == size_t(-1) && mGamepads[i].empty())
      slot = i;
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
  CFStringGetCString(productRef, product_name,
                     sizeof(product_name), kCFStringEncodingASCII);
  char buffer[256];
  sprintf(buffer, "%x-%x-%s", vendorId, productId, product_name);
  nsRefPtr<GamepadService> service(GamepadService::GetService());
  mGamepads[slot].mSuperIndex = service->AddGamepad(buffer,
                                                    mozilla::dom::NoMapping,
                                                    (int)mGamepads[slot].numButtons(),
                                                    (int)mGamepads[slot].numAxes());
}

void
DarwinGamepadService::DeviceRemoved(IOHIDDeviceRef device)
{
  nsRefPtr<GamepadService> service(GamepadService::GetService());
  for (size_t i = 0; i < mGamepads.size(); i++) {
    if (mGamepads[i] == device) {
      service->RemoveGamepad(mGamepads[i].mSuperIndex);
      mGamepads[i].clear();
      return;
    }
  }
}

void
DarwinGamepadService::InputValueChanged(IOHIDValueRef value)
{
  nsRefPtr<GamepadService> service(GamepadService::GetService());
  IOHIDElementRef element = IOHIDValueGetElement(value);
  IOHIDDeviceRef device = IOHIDElementGetDevice(element);
  for (size_t i = 0; i < mGamepads.size(); i++) {
    const Gamepad &gamepad = mGamepads[i];
    if (gamepad == device) {
      if (const Axis* axis = gamepad.lookupAxis(element)) {
        double d = IOHIDValueGetIntegerValue(value);
        double v = 2.0f * (d - axis->min) /
          (double)(axis->max - axis->min) - 1.0f;
        service->NewAxisMoveEvent(i, axis->id, v);
      } else if (const Button* button = gamepad.lookupButton(element)) {
        bool pressed = IOHIDValueGetIntegerValue(value) != 0;
        service->NewButtonEvent(i, button->id, pressed);
      }
      return;
    }
  }
}

void
DarwinGamepadService::DeviceAddedCallback(void* data, IOReturn result,
                                         void* sender, IOHIDDeviceRef device)
{
  DarwinGamepadService* service = (DarwinGamepadService*)data;
  service->DeviceAdded(device);
}

void
DarwinGamepadService::DeviceRemovedCallback(void* data, IOReturn result,
                                           void* sender, IOHIDDeviceRef device)
{
  DarwinGamepadService* service = (DarwinGamepadService*)data;
  service->DeviceRemoved(device);
}

void
DarwinGamepadService::InputValueChangedCallback(void* data,
                                               IOReturn result,
                                               void* sender,
                                               IOHIDValueRef newValue)
{
  DarwinGamepadService* service = (DarwinGamepadService*)data;
  service->InputValueChanged(newValue);
}

static CFMutableDictionaryRef
MatchingDictionary(UInt32 inUsagePage, UInt32 inUsage)
{
  CFMutableDictionaryRef dict =
    CFDictionaryCreateMutable(kCFAllocatorDefault,
                              0,
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
  if (!dict)
    return nullptr;
  CFNumberRef number = CFNumberCreate(kCFAllocatorDefault,
                                      kCFNumberIntType,
                                      &inUsagePage);
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

DarwinGamepadService::DarwinGamepadService() : mManager(nullptr) {}

DarwinGamepadService::~DarwinGamepadService()
{
  if (mManager != nullptr)
    CFRelease(mManager);
}

void DarwinGamepadService::Startup()
{
  if (mManager != nullptr)
    return;

  IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault,
                                               kIOHIDOptionsTypeNone);

  CFMutableDictionaryRef criteria_arr[2];
  criteria_arr[0] = MatchingDictionary(GENERIC_DESKTOP_USAGE_PAGE,
                                       JOYSTICK_USAGE_NUMBER);
  if (!criteria_arr[0]) {
    CFRelease(manager);
    return;
  }

  criteria_arr[1] = MatchingDictionary(GENERIC_DESKTOP_USAGE_PAGE,
                                       GAMEPAD_USAGE_NUMBER);
  if (!criteria_arr[1]) {
    CFRelease(criteria_arr[0]);
    CFRelease(manager);
    return;
  }

  CFArrayRef criteria =
    CFArrayCreate(kCFAllocatorDefault, (const void**)criteria_arr, 2, nullptr);
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

  IOHIDManagerRegisterDeviceMatchingCallback(manager,
                                             DeviceAddedCallback,
                                             this);
  IOHIDManagerRegisterDeviceRemovalCallback(manager,
                                            DeviceRemovedCallback,
                                            this);
  IOHIDManagerRegisterInputValueCallback(manager,
                                         InputValueChangedCallback,
                                         this);
  IOHIDManagerScheduleWithRunLoop(manager,
                                  CFRunLoopGetCurrent(),
                                  kCFRunLoopDefaultMode);
  IOReturn rv = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
  if (rv != kIOReturnSuccess) {
    CFRelease(manager);
    return;
  }

  mManager = manager;
}

void DarwinGamepadService::Shutdown()
{
  IOHIDManagerRef manager = (IOHIDManagerRef)mManager;
  if (manager) {
    IOHIDManagerClose(manager, 0);
    CFRelease(manager);
    mManager = nullptr;
  }
}

} // namespace

namespace mozilla {
namespace hal_impl {

DarwinGamepadService* gService = nullptr;

void StartMonitoringGamepadStatus()
{
  if (gService)
    return;

  gService = new DarwinGamepadService();
  gService->Startup();
}

void StopMonitoringGamepadStatus()
{
  if (!gService)
    return;

  gService->Shutdown();
  delete gService;
  gService = nullptr;
}

} // namespace hal_impl
} // namespace mozilla

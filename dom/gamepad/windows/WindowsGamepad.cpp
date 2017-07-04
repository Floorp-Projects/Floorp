/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <cstddef>

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <hidsdi.h>
#include <stdio.h>
#include <xinput.h>

#include "nsIComponentManager.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Services.h"

#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/dom/GamepadPlatformService.h"

namespace {

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::ArrayLength;

// USB HID usage tables, page 1 (Hat switch)
const unsigned kUsageDpad = 0x39;
// USB HID usage tables, page 1, 0x30 = X
const unsigned kFirstAxis = 0x30;

// USB HID usage tables
const unsigned kDesktopUsagePage = 0x1;
const unsigned kButtonUsagePage = 0x9;

// Multiple devices-changed notifications can be sent when a device
// is connected, because USB devices consist of multiple logical devices.
// Therefore, we wait a bit after receiving one before looking for
// device changes.
const uint32_t kDevicesChangedStableDelay = 200;
// Both DirectInput and XInput are polling-driven here,
// so we need to poll it periodically.
// 50ms is arbitrarily chosen.
const uint32_t kWindowsGamepadPollInterval = 50;

const UINT kRawInputError = (UINT)-1;

#ifndef XUSER_MAX_COUNT
#define XUSER_MAX_COUNT 4
#endif

const struct {
  int usagePage;
  int usage;
} kUsagePages[] = {
  // USB HID usage tables, page 1
  { kDesktopUsagePage, 4 },  // Joystick
  { kDesktopUsagePage, 5 }   // Gamepad
};

const struct {
  WORD button;
  int mapped;
} kXIButtonMap[] = {
  { XINPUT_GAMEPAD_DPAD_UP, 12 },
  { XINPUT_GAMEPAD_DPAD_DOWN, 13 },
  { XINPUT_GAMEPAD_DPAD_LEFT, 14 },
  { XINPUT_GAMEPAD_DPAD_RIGHT, 15 },
  { XINPUT_GAMEPAD_START, 9 },
  { XINPUT_GAMEPAD_BACK, 8 },
  { XINPUT_GAMEPAD_LEFT_THUMB, 10 },
  { XINPUT_GAMEPAD_RIGHT_THUMB, 11 },
  { XINPUT_GAMEPAD_LEFT_SHOULDER, 4 },
  { XINPUT_GAMEPAD_RIGHT_SHOULDER, 5 },
  { XINPUT_GAMEPAD_A, 0 },
  { XINPUT_GAMEPAD_B, 1 },
  { XINPUT_GAMEPAD_X, 2 },
  { XINPUT_GAMEPAD_Y, 3 }
};
const size_t kNumMappings = ArrayLength(kXIButtonMap);

enum GamepadType {
  kNoGamepad = 0,
  kRawInputGamepad,
  kXInputGamepad
};

class WindowsGamepadService;
// This pointer holds a windows gamepad backend service,
// it will be created and destroyed by background thread and
// used by gMonitorThread
WindowsGamepadService* MOZ_NON_OWNING_REF gService = nullptr;
nsCOMPtr<nsIThread> gMonitorThread = nullptr;
static bool sIsShutdown = false;

class Gamepad {
public:
  GamepadType type;

  // Handle to raw input device
  HANDLE handle;

  // XInput Index of the user's controller. Passed to XInputGetState.
  DWORD userIndex;

  // Last-known state of the controller.
  XINPUT_STATE state;

  // ID from the GamepadService, also used as the index into
  // WindowsGamepadService::mGamepads.
  int id;


  // Information about the physical device.
  unsigned numAxes;
  unsigned numButtons;
  bool hasDpad;
  HIDP_VALUE_CAPS dpadCaps;

  nsTArray<bool> buttons;
  struct axisValue {
    HIDP_VALUE_CAPS caps;
    double value;
  };
  nsTArray<axisValue> axes;

  // Used during rescan to find devices that were disconnected.
  bool present;

  Gamepad(uint32_t aNumAxes,
          uint32_t aNumButtons,
          bool aHasDpad,
          GamepadType aType) :
    numAxes(aNumAxes),
    numButtons(aNumButtons),
    hasDpad(aHasDpad),
    type(aType),
    present(true)
  {
    buttons.SetLength(numButtons);
    axes.SetLength(numAxes);
  }
private:
  Gamepad() {}
};

// Drop this in favor of decltype when we require a new enough SDK.
typedef void (WINAPI *XInputEnable_func)(BOOL);

// RAII class to wrap loading the XInput DLL
class XInputLoader {
public:
  XInputLoader() : module(nullptr),
                   mXInputEnable(nullptr),
                   mXInputGetState(nullptr) {
    // xinput1_4.dll exists on Windows 8
    // xinput9_1_0.dll exists on Windows 7 and Vista
    // xinput1_3.dll shipped with the DirectX SDK
    const wchar_t* dlls[] = {L"xinput1_4.dll",
                             L"xinput9_1_0.dll",
                             L"xinput1_3.dll"};
    const size_t kNumDLLs = ArrayLength(dlls);
    for (size_t i = 0; i < kNumDLLs; ++i) {
      module = LoadLibraryW(dlls[i]);
      if (module) {
        mXInputEnable = reinterpret_cast<XInputEnable_func>(
         GetProcAddress(module, "XInputEnable"));
        mXInputGetState = reinterpret_cast<decltype(XInputGetState)*>(
         GetProcAddress(module, "XInputGetState"));
        if (mXInputEnable) {
          mXInputEnable(TRUE);
        }
        break;
      }
    }
  }

  ~XInputLoader() {
    //mXInputEnable = nullptr;
    mXInputGetState = nullptr;

    if (module) {
      FreeLibrary(module);
    }
  }

  explicit operator bool() {
    return module && mXInputGetState;
  }

  HMODULE module;
  decltype(XInputGetState) *mXInputGetState;
  XInputEnable_func mXInputEnable;
};

bool
GetPreparsedData(HANDLE handle, nsTArray<uint8_t>& data)
{
  UINT size;
  if (GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA, nullptr, &size) == kRawInputError) {
    return false;
  }
  data.SetLength(size);
  return GetRawInputDeviceInfo(handle, RIDI_PREPARSEDDATA,
                               data.Elements(), &size) > 0;
}

/*
 * Given an axis value and a minimum and maximum range,
 * scale it to be in the range -1.0 .. 1.0.
 */
double
ScaleAxis(ULONG value, LONG min, LONG max)
{
  return  2.0 * (value - min) / (max - min) - 1.0;
}

/*
 * Given a value from a d-pad (POV hat in USB HID terminology),
 * represent it as 4 buttons, one for each cardinal direction.
 */
void
UnpackDpad(LONG dpad_value, const Gamepad* gamepad, nsTArray<bool>& buttons)
{
  const unsigned kUp = gamepad->numButtons - 4;
  const unsigned kDown = gamepad->numButtons - 3;
  const unsigned kLeft = gamepad->numButtons - 2;
  const unsigned kRight = gamepad->numButtons - 1;

  // Different controllers have different ways of representing
  // "nothing is pressed", but they're all outside the range of values.
  if (dpad_value < gamepad->dpadCaps.LogicalMin
      || dpad_value > gamepad->dpadCaps.LogicalMax) {
    // Nothing is pressed.
    return;
  }

  // Normalize value to start at 0.
  int value = dpad_value - gamepad->dpadCaps.LogicalMin;

  // Value will be in the range 0-7. The value represents the
  // position of the d-pad around a circle, with 0 being straight up,
  // 2 being right, 4 being straight down, and 6 being left.
  if (value < 2 || value > 6) {
    buttons[kUp] = true;
  }
  if (value > 2 && value < 6) {
    buttons[kDown] = true;
  }
  if (value > 4) {
    buttons[kLeft] = true;
  }
  if (value > 0 && value < 4) {
    buttons[kRight] = true;
  }
}

/*
 * Return true if this USB HID usage page and usage are of a type we
 * know how to handle.
 */
bool
SupportedUsage(USHORT page, USHORT usage)
{
  for (unsigned i = 0; i < ArrayLength(kUsagePages); i++) {
    if (page == kUsagePages[i].usagePage && usage == kUsagePages[i].usage) {
      return true;
    }
  }
  return false;
}

class HIDLoader {
public:
  HIDLoader() : mModule(LoadLibraryW(L"hid.dll")),
                mHidD_GetProductString(nullptr),
                mHidP_GetCaps(nullptr),
                mHidP_GetButtonCaps(nullptr),
                mHidP_GetValueCaps(nullptr),
                mHidP_GetUsages(nullptr),
                mHidP_GetUsageValue(nullptr),
                mHidP_GetScaledUsageValue(nullptr)
  {
    if (mModule) {
      mHidD_GetProductString = reinterpret_cast<decltype(HidD_GetProductString)*>(GetProcAddress(mModule, "HidD_GetProductString"));
      mHidP_GetCaps = reinterpret_cast<decltype(HidP_GetCaps)*>(GetProcAddress(mModule, "HidP_GetCaps"));
      mHidP_GetButtonCaps = reinterpret_cast<decltype(HidP_GetButtonCaps)*>(GetProcAddress(mModule, "HidP_GetButtonCaps"));
      mHidP_GetValueCaps = reinterpret_cast<decltype(HidP_GetValueCaps)*>(GetProcAddress(mModule, "HidP_GetValueCaps"));
      mHidP_GetUsages = reinterpret_cast<decltype(HidP_GetUsages)*>(GetProcAddress(mModule, "HidP_GetUsages"));
      mHidP_GetUsageValue = reinterpret_cast<decltype(HidP_GetUsageValue)*>(GetProcAddress(mModule, "HidP_GetUsageValue"));
      mHidP_GetScaledUsageValue = reinterpret_cast<decltype(HidP_GetScaledUsageValue)*>(GetProcAddress(mModule, "HidP_GetScaledUsageValue"));
    }
  }

  ~HIDLoader() {
    if (mModule) {
      FreeLibrary(mModule);
    }
  }

  explicit operator bool() {
    return mModule &&
      mHidD_GetProductString &&
      mHidP_GetCaps &&
      mHidP_GetButtonCaps &&
      mHidP_GetValueCaps &&
      mHidP_GetUsages &&
      mHidP_GetUsageValue &&
      mHidP_GetScaledUsageValue;
  }

  decltype(HidD_GetProductString) *mHidD_GetProductString;
  decltype(HidP_GetCaps) *mHidP_GetCaps;
  decltype(HidP_GetButtonCaps) *mHidP_GetButtonCaps;
  decltype(HidP_GetValueCaps) *mHidP_GetValueCaps;
  decltype(HidP_GetUsages) *mHidP_GetUsages;
  decltype(HidP_GetUsageValue) *mHidP_GetUsageValue;
  decltype(HidP_GetScaledUsageValue) *mHidP_GetScaledUsageValue;

private:
  HMODULE mModule;
};

HWND sHWnd = nullptr;

static void
DirectInputMessageLoopOnceCallback(nsITimer *aTimer, void* aClosure)
{
  MOZ_ASSERT(NS_GetCurrentThread() == gMonitorThread);
  MSG msg;
  while (PeekMessageW(&msg, sHWnd, 0, 0, PM_REMOVE) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  aTimer->Cancel();
  if (!sIsShutdown) {
    aTimer->InitWithNamedFuncCallback(DirectInputMessageLoopOnceCallback,
                                      nullptr, kWindowsGamepadPollInterval,
                                      nsITimer::TYPE_ONE_SHOT,
                                      "DirectInputMessageLoopOnceCallback");
  }
}

class WindowsGamepadService
{
 public:
  WindowsGamepadService()
  {
    mDirectInputTimer = do_CreateInstance("@mozilla.org/timer;1");
    mXInputTimer = do_CreateInstance("@mozilla.org/timer;1");
    mDeviceChangeTimer = do_CreateInstance("@mozilla.org/timer;1");
  }
  virtual ~WindowsGamepadService()
  {
    Cleanup();
  }

  void DevicesChanged(bool aIsStablizing);

  void StartMessageLoop()
  {
    MOZ_ASSERT(mDirectInputTimer);
    mDirectInputTimer->InitWithNamedFuncCallback(DirectInputMessageLoopOnceCallback,
                                                 nullptr, kWindowsGamepadPollInterval,
                                                 nsITimer::TYPE_ONE_SHOT,
                                                 "DirectInputMessageLoopOnceCallback");
  }

  void Startup();
  void Shutdown();
  // Parse gamepad input from a WM_INPUT message.
  bool HandleRawInput(HRAWINPUT handle);

  static void XInputMessageLoopOnceCallback(nsITimer *aTimer, void* aClosure);
  static void DevicesChangeCallback(nsITimer *aTimer, void* aService);

 private:
  void ScanForDevices();
  // Look for connected raw input devices.
  void ScanForRawInputDevices();
  // Look for connected XInput devices.
  bool ScanForXInputDevices();
  bool HaveXInputGamepad(int userIndex);

  bool mIsXInputMonitoring;
  void PollXInput();
  void CheckXInputChanges(Gamepad& gamepad, XINPUT_STATE& state);

  // Get information about a raw input gamepad.
  bool GetRawGamepad(HANDLE handle);
  void Cleanup();

  // List of connected devices.
  nsTArray<Gamepad> mGamepads;

  HIDLoader mHID;
  XInputLoader mXInput;

  nsCOMPtr<nsITimer> mDirectInputTimer;
  nsCOMPtr<nsITimer> mXInputTimer;
  nsCOMPtr<nsITimer> mDeviceChangeTimer;
};


void
WindowsGamepadService::ScanForRawInputDevices()
{
  if (!mHID) {
    return;
  }

  UINT numDevices;
  if (GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST))
      == kRawInputError) {
    return;
  }
  nsTArray<RAWINPUTDEVICELIST> devices(numDevices);
  devices.SetLength(numDevices);
  if (GetRawInputDeviceList(devices.Elements(), &numDevices,
                            sizeof(RAWINPUTDEVICELIST)) == kRawInputError) {
    return;
  }

  for (unsigned i = 0; i < devices.Length(); i++) {
    if (devices[i].dwType == RIM_TYPEHID) {
      GetRawGamepad(devices[i].hDevice);
    }
  }
}

// static
void
WindowsGamepadService::XInputMessageLoopOnceCallback(nsITimer *aTimer,
                                                     void* aService)
{
  MOZ_ASSERT(aService);
  WindowsGamepadService* self = static_cast<WindowsGamepadService*>(aService);
  self->PollXInput();
  if (self->mIsXInputMonitoring) {
    aTimer->Cancel();
    aTimer->InitWithNamedFuncCallback(XInputMessageLoopOnceCallback, self,
                                      kWindowsGamepadPollInterval, nsITimer::TYPE_ONE_SHOT,
                                      "XInputMessageLoopOnceCallback");
  }
}

// static
void
WindowsGamepadService::DevicesChangeCallback(nsITimer *aTimer, void* aService)
{
  MOZ_ASSERT(aService);
  WindowsGamepadService* self = static_cast<WindowsGamepadService*>(aService);
  self->DevicesChanged(false);
}

bool
WindowsGamepadService::HaveXInputGamepad(int userIndex)
{
  for (unsigned int i = 0; i < mGamepads.Length(); i++) {
    if (mGamepads[i].type == kXInputGamepad
        && mGamepads[i].userIndex == userIndex) {
      mGamepads[i].present = true;
      return true;
    }
  }
  return false;
}

bool
WindowsGamepadService::ScanForXInputDevices()
{
  MOZ_ASSERT(mXInput, "XInput should be present!");

  bool found = false;
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();
  if (!service) {
    return found;
  }

  for (int i = 0; i < XUSER_MAX_COUNT; i++) {
    XINPUT_STATE state = {};
    if (mXInput.mXInputGetState(i, &state) != ERROR_SUCCESS) {
      continue;
    }
    found = true;
    // See if this device is already present in our list.
    if (HaveXInputGamepad(i)) {
      continue;
    }

    // Not already present, add it.
    Gamepad gamepad(kStandardGamepadAxes,
                    kStandardGamepadButtons,
                    true,
                    kXInputGamepad);
    gamepad.userIndex = i;
    gamepad.state = state;
    gamepad.id = service->AddGamepad("xinput",
                                     GamepadMappingType::Standard,
                                     GamepadHand::_empty,
                                     kStandardGamepadButtons,
                                     kStandardGamepadAxes,
                                     0); // TODO: Bug 680289, implement gamepad haptics for Windows.
    mGamepads.AppendElement(gamepad);
  }

  return found;
}

void
WindowsGamepadService::ScanForDevices()
{
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();
  if (!service) {
    return;
  }

  for (int i = mGamepads.Length() - 1; i >= 0; i--) {
    mGamepads[i].present = false;
  }

  if (mHID) {
    ScanForRawInputDevices();
  }
  if (mXInput) {
    mXInputTimer->Cancel();
    if (ScanForXInputDevices()) {
      mIsXInputMonitoring = true;
      mXInputTimer->InitWithNamedFuncCallback(XInputMessageLoopOnceCallback, this,
                                              kWindowsGamepadPollInterval,
                                              nsITimer::TYPE_ONE_SHOT,
                                              "XInputMessageLoopOnceCallback");
    } else {
      mIsXInputMonitoring = false;
    }
  }

  // Look for devices that are no longer present and remove them.
  for (int i = mGamepads.Length() - 1; i >= 0; i--) {
    if (!mGamepads[i].present) {
      service->RemoveGamepad(mGamepads[i].id);
      mGamepads.RemoveElementAt(i);
    }
  }
}

void
WindowsGamepadService::PollXInput()
{
  for (unsigned int i = 0; i < mGamepads.Length(); i++) {
    if (mGamepads[i].type != kXInputGamepad) {
      continue;
    }

    XINPUT_STATE state = {};
    DWORD res = mXInput.mXInputGetState(mGamepads[i].userIndex, &state);
    if (res == ERROR_SUCCESS
        && state.dwPacketNumber != mGamepads[i].state.dwPacketNumber) {
        CheckXInputChanges(mGamepads[i], state);
    }
  }
}

void WindowsGamepadService::CheckXInputChanges(Gamepad& gamepad,
                                               XINPUT_STATE& state) {
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();
  if (!service) {
    return;
  }
  // Handle digital buttons first
  for (size_t b = 0; b < kNumMappings; b++) {
    if (state.Gamepad.wButtons & kXIButtonMap[b].button &&
        !(gamepad.state.Gamepad.wButtons & kXIButtonMap[b].button)) {
      // Button pressed
      service->NewButtonEvent(gamepad.id, kXIButtonMap[b].mapped, true);
    } else if (!(state.Gamepad.wButtons & kXIButtonMap[b].button) &&
               gamepad.state.Gamepad.wButtons & kXIButtonMap[b].button) {
      // Button released
      service->NewButtonEvent(gamepad.id, kXIButtonMap[b].mapped, false);
    }
  }

  // Then triggers
  if (state.Gamepad.bLeftTrigger != gamepad.state.Gamepad.bLeftTrigger) {
    bool pressed =
      state.Gamepad.bLeftTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    service->NewButtonEvent(gamepad.id, kButtonLeftTrigger,
                            pressed, state.Gamepad.bLeftTrigger / 255.0);
  }
  if (state.Gamepad.bRightTrigger != gamepad.state.Gamepad.bRightTrigger) {
    bool pressed =
      state.Gamepad.bRightTrigger >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    service->NewButtonEvent(gamepad.id, kButtonRightTrigger,
                            pressed, state.Gamepad.bRightTrigger / 255.0);
  }

  // Finally deal with analog sticks
  // TODO: bug 1001955 - Support deadzones.
  if (state.Gamepad.sThumbLX != gamepad.state.Gamepad.sThumbLX) {
    service->NewAxisMoveEvent(gamepad.id, kLeftStickXAxis,
                              state.Gamepad.sThumbLX / 32767.0);
  }
  if (state.Gamepad.sThumbLY != gamepad.state.Gamepad.sThumbLY) {
    service->NewAxisMoveEvent(gamepad.id, kLeftStickYAxis,
                              -1.0 * state.Gamepad.sThumbLY / 32767.0);
  }
  if (state.Gamepad.sThumbRX != gamepad.state.Gamepad.sThumbRX) {
    service->NewAxisMoveEvent(gamepad.id, kRightStickXAxis,
                              state.Gamepad.sThumbRX / 32767.0);
  }
  if (state.Gamepad.sThumbRY != gamepad.state.Gamepad.sThumbRY) {
    service->NewAxisMoveEvent(gamepad.id, kRightStickYAxis,
                              -1.0 * state.Gamepad.sThumbRY / 32767.0);
  }
  gamepad.state = state;
}

// Used to sort a list of axes by HID usage.
class HidValueComparator {
public:
  bool Equals(const HIDP_VALUE_CAPS& c1, const HIDP_VALUE_CAPS& c2) const
  {
    return c1.UsagePage == c2.UsagePage && c1.Range.UsageMin == c2.Range.UsageMin;
  }
  bool LessThan(const HIDP_VALUE_CAPS& c1, const HIDP_VALUE_CAPS& c2) const
  {
    if (c1.UsagePage == c2.UsagePage) {
      return c1.Range.UsageMin < c2.Range.UsageMin;
    }
    return c1.UsagePage < c2.UsagePage;
  }
};

bool
WindowsGamepadService::GetRawGamepad(HANDLE handle)
{
  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();
  if (!service) {
    return true;
  }

  if (!mHID) {
    return false;
  }

  for (unsigned i = 0; i < mGamepads.Length(); i++) {
    if (mGamepads[i].type == kRawInputGamepad && mGamepads[i].handle == handle) {
      mGamepads[i].present = true;
      return true;
    }
  }

  RID_DEVICE_INFO rdi = {};
  UINT size = rdi.cbSize = sizeof(RID_DEVICE_INFO);
  if (GetRawInputDeviceInfo(handle, RIDI_DEVICEINFO, &rdi, &size) == kRawInputError) {
    return false;
  }
  // Ensure that this is a device we care about
  if (!SupportedUsage(rdi.hid.usUsagePage, rdi.hid.usUsage)) {
    return false;
  }

  // Device name is a mostly-opaque string.
  if (GetRawInputDeviceInfo(handle, RIDI_DEVICENAME, nullptr, &size) == kRawInputError) {
    return false;
  }

  nsTArray<wchar_t> devname(size);
  devname.SetLength(size);
  if (GetRawInputDeviceInfo(handle, RIDI_DEVICENAME, devname.Elements(), &size) == kRawInputError) {
    return false;
  }

  // Per http://msdn.microsoft.com/en-us/library/windows/desktop/ee417014.aspx
  // device names containing "IG_" are XInput controllers. Ignore those
  // devices since we'll handle them with XInput.
  if (wcsstr(devname.Elements(), L"IG_")) {
    return false;
  }

  // Product string is a human-readable name.
  // Per http://msdn.microsoft.com/en-us/library/windows/hardware/ff539681%28v=vs.85%29.aspx
  // "For USB devices, the maximum string length is 126 wide characters (not including the terminating NULL character)."
  wchar_t name[128] = { 0 };
  size = sizeof(name);
  nsTArray<char> gamepad_name;
  HANDLE hid_handle = CreateFile(devname.Elements(), GENERIC_READ | GENERIC_WRITE,
    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if (hid_handle) {
    if (mHID.mHidD_GetProductString(hid_handle, &name, size)) {
      int bytes = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr,
                                      nullptr);
      gamepad_name.SetLength(bytes);
      WideCharToMultiByte(CP_UTF8, 0, name, -1, gamepad_name.Elements(),
                          bytes, nullptr, nullptr);
    }
    CloseHandle(hid_handle);
  }
  if (gamepad_name.Length() == 0 || !gamepad_name[0]) {
    const char kUnknown[] = "Unknown Gamepad";
    gamepad_name.SetLength(ArrayLength(kUnknown));
    strcpy_s(gamepad_name.Elements(), gamepad_name.Length(), kUnknown);
  }

  char gamepad_id[256] = { 0 };
  _snprintf_s(gamepad_id, _TRUNCATE, "%04x-%04x-%s", rdi.hid.dwVendorId,
              rdi.hid.dwProductId, gamepad_name.Elements());

  nsTArray<uint8_t> preparsedbytes;
  if (!GetPreparsedData(handle, preparsedbytes)) {
    return false;
  }

  PHIDP_PREPARSED_DATA parsed =
    reinterpret_cast<PHIDP_PREPARSED_DATA>(preparsedbytes.Elements());
  HIDP_CAPS caps;
  if (mHID.mHidP_GetCaps(parsed, &caps) != HIDP_STATUS_SUCCESS) {
    return false;
  }

  // Enumerate buttons.
  USHORT count = caps.NumberInputButtonCaps;
  nsTArray<HIDP_BUTTON_CAPS> buttonCaps(count);
  buttonCaps.SetLength(count);
  if (mHID.mHidP_GetButtonCaps(HidP_Input, buttonCaps.Elements(), &count, parsed)
      != HIDP_STATUS_SUCCESS) {
    return false;
  }
  uint32_t numButtons = 0;
  for (unsigned i = 0; i < count; i++) {
    // Each buttonCaps is typically a range of buttons.
    numButtons +=
      buttonCaps[i].Range.UsageMax - buttonCaps[i].Range.UsageMin + 1;
  }

  // Enumerate value caps, which represent axes and d-pads.
  count = caps.NumberInputValueCaps;
  nsTArray<HIDP_VALUE_CAPS> valueCaps(count);
  valueCaps.SetLength(count);
  if (mHID.mHidP_GetValueCaps(HidP_Input, valueCaps.Elements(), &count, parsed)
      != HIDP_STATUS_SUCCESS) {
    return false;
  }
  nsTArray<HIDP_VALUE_CAPS> axes;
  // Sort the axes by usagePage and usage to expose a consistent ordering.
  bool hasDpad;
  HIDP_VALUE_CAPS dpadCaps;

  HidValueComparator comparator;
  for (unsigned i = 0; i < count; i++) {
    if (valueCaps[i].UsagePage == kDesktopUsagePage
        && valueCaps[i].Range.UsageMin == kUsageDpad
        // Don't know how to handle d-pads that return weird values.
        && valueCaps[i].LogicalMax - valueCaps[i].LogicalMin == 7) {
      // d-pad gets special handling.
      // Ostensibly HID devices can expose multiple d-pads, but this
      // doesn't happen in practice.
      hasDpad = true;
      dpadCaps = valueCaps[i];
      // Expose d-pad as 4 additional buttons.
      numButtons += 4;
    } else {
      axes.InsertElementSorted(valueCaps[i], comparator);
    }
  }

  uint32_t numAxes = axes.Length();

  // Not already present, add it.
  Gamepad gamepad(numAxes,
                  numButtons,
                  true,
                  kRawInputGamepad);

  gamepad.handle = handle;

  for (unsigned i = 0; i < gamepad.numAxes; i++) {
    gamepad.axes[i].caps = axes[i];
  }

  gamepad.id = service->AddGamepad(gamepad_id,
                                   GamepadMappingType::_empty,
                                   GamepadHand::_empty,
                                   gamepad.numButtons,
                                   gamepad.numAxes,
                                   0);
  mGamepads.AppendElement(gamepad);
  return true;
}

bool
WindowsGamepadService::HandleRawInput(HRAWINPUT handle)
{
  if (!mHID) {
    return false;
  }

  RefPtr<GamepadPlatformService> service =
    GamepadPlatformService::GetParentService();
  if (!service) {
    return false;
  }

  // First, get data from the handle
  UINT size;
  GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
  nsTArray<uint8_t> data(size);
  data.SetLength(size);
  if (GetRawInputData(handle, RID_INPUT, data.Elements(), &size,
                      sizeof(RAWINPUTHEADER)) == kRawInputError) {
    return false;
  }
  PRAWINPUT raw = reinterpret_cast<PRAWINPUT>(data.Elements());

  Gamepad* gamepad = nullptr;
  for (unsigned i = 0; i < mGamepads.Length(); i++) {
    if (mGamepads[i].type == kRawInputGamepad
        && mGamepads[i].handle == raw->header.hDevice) {
      gamepad = &mGamepads[i];
      break;
    }
  }
  if (gamepad == nullptr) {
    return false;
  }

  // Second, get the preparsed data
  nsTArray<uint8_t> parsedbytes;
  if (!GetPreparsedData(raw->header.hDevice, parsedbytes)) {
    return false;
  }
  PHIDP_PREPARSED_DATA parsed =
    reinterpret_cast<PHIDP_PREPARSED_DATA>(parsedbytes.Elements());

  // Get all the pressed buttons.
  nsTArray<USAGE> usages(gamepad->numButtons);
  usages.SetLength(gamepad->numButtons);
  ULONG usageLength = gamepad->numButtons;
  if (mHID.mHidP_GetUsages(HidP_Input, kButtonUsagePage, 0, usages.Elements(),
                     &usageLength, parsed, (PCHAR)raw->data.hid.bRawData,
                     raw->data.hid.dwSizeHid) != HIDP_STATUS_SUCCESS) {
    return false;
  }

  nsTArray<bool> buttons(gamepad->numButtons);
  buttons.SetLength(gamepad->numButtons);
  // If we don't zero out the buttons array first, sometimes it can reuse values.
  memset(buttons.Elements(), 0, gamepad->numButtons * sizeof(bool));

  for (unsigned i = 0; i < usageLength; i++) {
    buttons[usages[i] - 1] = true;
  }

  if (gamepad->hasDpad) {
    // Get d-pad position as 4 buttons.
    ULONG value;
    if (mHID.mHidP_GetUsageValue(HidP_Input, gamepad->dpadCaps.UsagePage, 0, gamepad->dpadCaps.Range.UsageMin, &value, parsed, (PCHAR)raw->data.hid.bRawData, raw->data.hid.dwSizeHid) == HIDP_STATUS_SUCCESS) {
      UnpackDpad(static_cast<LONG>(value), gamepad, buttons);
    }
  }

  for (unsigned i = 0; i < gamepad->numButtons; i++) {
    if (gamepad->buttons[i] != buttons[i]) {
      service->NewButtonEvent(gamepad->id, i, buttons[i]);
      gamepad->buttons[i] = buttons[i];
    }
  }

  // Get all axis values.
  for (unsigned i = 0; i < gamepad->numAxes; i++) {
    double new_value;
    if (gamepad->axes[i].caps.LogicalMin < 0) {
LONG value;
      if (mHID.mHidP_GetScaledUsageValue(HidP_Input, gamepad->axes[i].caps.UsagePage,
                                   0, gamepad->axes[i].caps.Range.UsageMin,
                                   &value, parsed,
                                   (PCHAR)raw->data.hid.bRawData,
                                   raw->data.hid.dwSizeHid)
          != HIDP_STATUS_SUCCESS) {
        continue;
      }
      new_value = ScaleAxis(value, gamepad->axes[i].caps.LogicalMin,
                            gamepad->axes[i].caps.LogicalMax);
    } else {
      ULONG value;
      if (mHID.mHidP_GetUsageValue(HidP_Input, gamepad->axes[i].caps.UsagePage, 0,
                             gamepad->axes[i].caps.Range.UsageMin, &value,
                             parsed, (PCHAR)raw->data.hid.bRawData,
                             raw->data.hid.dwSizeHid) != HIDP_STATUS_SUCCESS) {
        continue;
      }

      new_value = ScaleAxis(value, gamepad->axes[i].caps.LogicalMin,
                            gamepad->axes[i].caps.LogicalMax);
    }
    if (gamepad->axes[i].value != new_value) {
      service->NewAxisMoveEvent(gamepad->id, i, new_value);
      gamepad->axes[i].value = new_value;
    }
  }

  return true;
}

void
WindowsGamepadService::Startup()
{
  ScanForDevices();
}

void
WindowsGamepadService::Shutdown()
{
  Cleanup();
}

void
WindowsGamepadService::Cleanup()
{
  mIsXInputMonitoring = false;
  if (mDirectInputTimer) {
    mDirectInputTimer->Cancel();
  }
  if (mXInputTimer) {
    mXInputTimer->Cancel();
  }
  if (mDeviceChangeTimer) {
    mDeviceChangeTimer->Cancel();
  }
  mGamepads.Clear();
}

void
WindowsGamepadService::DevicesChanged(bool aIsStablizing)
{
  if (aIsStablizing) {
    mDeviceChangeTimer->Cancel();
    mDeviceChangeTimer->InitWithNamedFuncCallback(DevicesChangeCallback, this,
                                                  kDevicesChangedStableDelay,
                                                  nsITimer::TYPE_ONE_SHOT,
                                                  "DevicesChangeCallback");
  } else {
    ScanForDevices();
  }
}

bool
RegisterRawInput(HWND hwnd, bool enable)
{
  nsTArray<RAWINPUTDEVICE> rid(ArrayLength(kUsagePages));
  rid.SetLength(ArrayLength(kUsagePages));

  for (unsigned i = 0; i < rid.Length(); i++) {
    rid[i].usUsagePage = kUsagePages[i].usagePage;
    rid[i].usUsage = kUsagePages[i].usage;
    rid[i].dwFlags =
      enable ? RIDEV_EXINPUTSINK | RIDEV_DEVNOTIFY : RIDEV_REMOVE;
    rid[i].hwndTarget = hwnd;
  }

  if (!RegisterRawInputDevices(rid.Elements(), rid.Length(),
                               sizeof(RAWINPUTDEVICE))) {
    return false;
  }
  return true;
}

static
LRESULT CALLBACK
GamepadWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  const unsigned int DBT_DEVICEARRIVAL        = 0x8000;
  const unsigned int DBT_DEVICEREMOVECOMPLETE = 0x8004;
  const unsigned int DBT_DEVNODES_CHANGED     = 0x7;

  switch (msg) {
  case WM_DEVICECHANGE:
    if (wParam == DBT_DEVICEARRIVAL ||
        wParam == DBT_DEVICEREMOVECOMPLETE ||
        wParam == DBT_DEVNODES_CHANGED) {
      if (gService) {
        gService->DevicesChanged(true);
      }
    }
    break;
  case WM_INPUT:
    if (gService) {
      gService->HandleRawInput(reinterpret_cast<HRAWINPUT>(lParam));
    }
    break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

class StartWindowsGamepadServiceRunnable final : public Runnable
{
public:
  StartWindowsGamepadServiceRunnable()
    : Runnable("StartWindowsGamepadServiceRunnable")
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_GetCurrentThread() == gMonitorThread);
    gService = new WindowsGamepadService();
    gService->Startup();

    if (sHWnd == nullptr) {
      WNDCLASSW wc;
      HMODULE hSelf = GetModuleHandle(nullptr);

      if (!GetClassInfoW(hSelf, L"MozillaGamepadClass", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASSW));
        wc.hInstance = hSelf;
        wc.lpfnWndProc = GamepadWindowProc;
        wc.lpszClassName = L"MozillaGamepadClass";
        RegisterClassW(&wc);
      }

      sHWnd = CreateWindowW(L"MozillaGamepadClass", L"Gamepad Watcher",
        0, 0, 0, 0, 0,
        nullptr, nullptr, hSelf, nullptr);
      RegisterRawInput(sHWnd, true);
    }

    // Explicitly start the message loop
    gService->StartMessageLoop();

    return NS_OK;
  }
private:
  ~StartWindowsGamepadServiceRunnable() {}
};

class StopWindowsGamepadServiceRunnable final : public Runnable
{
 public:
  StopWindowsGamepadServiceRunnable()
    : Runnable("StopWindowsGamepadServiceRunnable")
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_GetCurrentThread() == gMonitorThread);
    if (sHWnd) {
      RegisterRawInput(sHWnd, false);
      DestroyWindow(sHWnd);
      sHWnd = nullptr;
    }

    gService->Shutdown();
    delete gService;
    gService = nullptr;

    return NS_OK;
  }
 private:
  ~StopWindowsGamepadServiceRunnable() {}
};

} // namespace

namespace mozilla {
namespace dom {

using namespace mozilla::ipc;

void
StartGamepadMonitoring()
{
  AssertIsOnBackgroundThread();

  if (gMonitorThread || gService) {
    return;
  }
  sIsShutdown = false;
  NS_NewNamedThread("Gamepad", getter_AddRefs(gMonitorThread));
  gMonitorThread->Dispatch(new StartWindowsGamepadServiceRunnable(),
                           NS_DISPATCH_NORMAL);
}

void
StopGamepadMonitoring()
{
  AssertIsOnBackgroundThread();

  if (sIsShutdown) {
    return;
  }
  sIsShutdown = true;
  gMonitorThread->Dispatch(new StopWindowsGamepadServiceRunnable(), NS_DISPATCH_NORMAL);
  gMonitorThread->Shutdown();
  gMonitorThread = nullptr;
}

} // namespace dom
} // namespace mozilla

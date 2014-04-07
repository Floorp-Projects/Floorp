/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <cstddef>

#include <stdio.h>
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "nsIComponentManager.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/GamepadService.h"
#include "mozilla/Mutex.h"
#include "mozilla/Services.h"

namespace {

using mozilla::dom::GamepadService;
using mozilla::Mutex;
using mozilla::MutexAutoLock;

const LONG kMaxAxisValue = 65535;
const DWORD BUTTON_DOWN_MASK = 0x80;
// Multiple devices-changed notifications can be sent when a device
// is connected, because USB devices consist of multiple logical devices.
// Therefore, we wait a bit after receiving one before looking for
// device changes.
const uint32_t kDevicesChangedStableDelay = 200;

class WindowsGamepadService;
WindowsGamepadService* gService = nullptr;

typedef struct {
  float x,y;
} HatState;

struct Gamepad {
  // From DirectInput, unique to this device+computer combination.
  GUID guidInstance;
  // The ID assigned by the base GamepadService
  int globalID;
  // A somewhat unique string consisting of the USB vendor/product IDs,
  // and the controller name.
  char idstring[128];
  // USB vendor and product IDs
  int vendorID;
  int productID;
  // Information about the physical device.
  int numAxes;
  int numHats;
  int numButtons;
  // The human-readable device name.
  char name[128];
  // The DirectInput device.
  nsRefPtr<IDirectInputDevice8> device;
  // A handle that DirectInput signals when there is new data from
  // the device.
  HANDLE event;
  // The state of any POV hats on the device.
  HatState hatState[4];
  // Used during rescan to find devices that were disconnected.
  bool present;
  // Passed back from the main thread to indicate a device can
  // now be removed.
  bool remove;
};

// Given DWORD |hatPos| representing the position of the POV hat per:
// http://msdn.microsoft.com/en-us/library/ee418260%28v=VS.85%29.aspx
// fill |axes| with the position of the x and y axes.
//
//XXX: ostensibly the values could be arbitrary degrees for a hat with
// full rotation, but we'll punt on that for now. This should handle
// 8-way D-pads exposed as POV hats.
static void
HatPosToAxes(DWORD hatPos, HatState& axes) {
  // hatPos is in hundredths of a degree clockwise from north.
  if (LOWORD(hatPos) == 0xFFFF) {
    // centered
    axes.x = axes.y = 0.0;
  }
  else if (hatPos == 0)  {
    // Up
    axes.x = 0.0;
    axes.y = -1.0;
  }
  else if (hatPos == 45 * DI_DEGREES) {
    // Up-right
    axes.x = 1.0;
    axes.y = -1.0;
  }
  else if (hatPos == 90 * DI_DEGREES) {
    // Right
    axes.x = 1.0;
    axes.y = 0.0;
  }
  else if (hatPos == 135 * DI_DEGREES) {
    // Down-right
    axes.x = 1.0;
    axes.y = 1.0;
  }
  else if (hatPos == 180 * DI_DEGREES) {
    // Down
    axes.x = 0.0;
    axes.y = 1.0;
  }
  else if (hatPos == 225 * DI_DEGREES) {
    // Down-left
    axes.x = -1.0;
    axes.y = 1.0;
  }
  else if (hatPos == 270 * DI_DEGREES) {
    // Left
    axes.x = -1.0;
    axes.y = 0.0;
  }
  else if (hatPos == 315 * DI_DEGREES) {
    // Up-left
    axes.x = -1.0;
    axes.y = -1.0;
  }
}

class Observer : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  Observer(WindowsGamepadService& svc) : mSvc(svc),
                                         mObserving(true) {
    nsresult rv;
    mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    observerService->AddObserver(this,
                                 NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID,
                                 false);
  }

  void Stop() {
    if (mTimer) {
      mTimer->Cancel();
    }
    if (mObserving) {
      nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
      observerService->RemoveObserver(this, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID);
      mObserving = false;
    }
  }

  virtual ~Observer() {
    Stop();
  }

  void SetDeviceChangeTimer() {
    // set stable timer, since we will get multiple devices-changed
    // notifications at once
    if (mTimer) {
      mTimer->Cancel();
      mTimer->Init(this, kDevicesChangedStableDelay, nsITimer::TYPE_ONE_SHOT);
    }
  }

private:
  // Gamepad service owns us, we just hold a reference back to it.
  WindowsGamepadService& mSvc;
  nsCOMPtr<nsITimer> mTimer;
  bool mObserving;
};

NS_IMPL_ISUPPORTS1(Observer, nsIObserver);

class WindowsGamepadService {
public:
  WindowsGamepadService();
  virtual ~WindowsGamepadService() {
    Cleanup();
    CloseHandle(mThreadExitEvent);
    CloseHandle(mThreadRescanEvent);
    if (dinput) {
      dinput->Release();
      dinput = nullptr;
    }
  }

  enum DeviceChangeType {
    DeviceChangeNotification,
    DeviceChangeStable
  };
  void DevicesChanged(DeviceChangeType type);
  void Startup();
  void Shutdown();
  void SetGamepadID(int localID, int globalID);
  void RemoveGamepad(int localID);

private:
  void ScanForDevices();
  void Cleanup();
  void CleanupGamepad(Gamepad& gamepad);
  // Callback for enumerating axes on a device
  static BOOL CALLBACK EnumObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi,
                                           LPVOID pvRef);
  // Callback for enumerating devices via DInput
  static BOOL CALLBACK EnumCallback(LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef);
  // Thread function to wait on device events
  static DWORD WINAPI DInputThread(LPVOID arg);

  // Used to signal the background thread to exit.
  HANDLE mThreadExitEvent;
  // Used to signal the background thread to rescan devices.
  HANDLE mThreadRescanEvent;
  HANDLE mThread;

  // List of connected devices.
  nsTArray<Gamepad> mGamepads;
  // Used to lock mutation of mGamepads.
  Mutex mMutex;
  // List of event handles used for signaling.
  nsTArray<HANDLE> mEvents;

  LPDIRECTINPUT8 dinput;

  nsRefPtr<Observer> mObserver;
};

// Used to post events from the background thread to the foreground thread.
class GamepadEvent : public nsRunnable {
public:
  typedef enum {
    Axis,
    Button,
    HatX,
    HatY,
    HatXY,
    Unknown
  } Type;

  GamepadEvent(const Gamepad& gamepad,
               Type type,
               int which,
               DWORD data) : mGlobalID(gamepad.globalID),
                             mGamepadAxes(gamepad.numAxes),
                             mType(type),
                             mWhich(which),
                             mData(data) {
  }

  NS_IMETHOD Run() {
    nsRefPtr<GamepadService> gamepadsvc(GamepadService::GetService());
    if (!gamepadsvc) {
      return NS_OK;
    }

    switch (mType) {
    case Button:
      gamepadsvc->NewButtonEvent(mGlobalID, mWhich, mData & BUTTON_DOWN_MASK);
      break;
    case Axis: {
      float adjustedData = ((float)mData * 2.0f) / (float)kMaxAxisValue - 1.0f;
      gamepadsvc->NewAxisMoveEvent(mGlobalID, mWhich, adjustedData);
    }
    case HatX:
    case HatY:
    case HatXY: {
      // Synthesize 2 axes per POV hat for convenience.
      HatState hatState;
      HatPosToAxes(mData, hatState);
      int xAxis = mGamepadAxes + 2 * mWhich;
      int yAxis = mGamepadAxes + 2 * mWhich + 1;
      //TODO: ostensibly we could not fire an event if one axis hasn't
      // changed, but it's a pain to track that.
      if (mType == HatX || mType == HatXY) {
        gamepadsvc->NewAxisMoveEvent(mGlobalID, xAxis, hatState.x);
      }
      if (mType == HatY || mType == HatXY) {
        gamepadsvc->NewAxisMoveEvent(mGlobalID, yAxis, hatState.y);
      }
      break;
    }
    case Unknown:
      break;
    }
    return NS_OK;
  }

  int mGlobalID;
  int mGamepadAxes;
  // Type of event
  Type mType;
  // Which button/axis is involved
  int mWhich;
  // Data specific to event
  DWORD mData;
};

class GamepadChangeEvent : public nsRunnable {
public:
  enum Type {
    Added,
    Removed
  };
  GamepadChangeEvent(Gamepad& gamepad,
                     int localID,
                     Type type) : mLocalID(localID),
                                  mName(gamepad.idstring),
                                  mGlobalID(gamepad.globalID),
                                  mGamepadButtons(gamepad.numButtons),
                                  mGamepadAxes(gamepad.numAxes),
                                  mGamepadHats(gamepad.numHats),
                                  mType(type) {
  }

  NS_IMETHOD Run() {
    nsRefPtr<GamepadService> gamepadsvc(GamepadService::GetService());
    if (!gamepadsvc) {
      return NS_OK;
    }
    if (mType == Added) {
      int globalID = gamepadsvc->AddGamepad(mName.get(),
                                            mozilla::dom::NoMapping,
                                            mGamepadButtons,
                                            mGamepadAxes +
                                            mGamepadHats*2);
      if (gService) {
        gService->SetGamepadID(mLocalID, globalID);
      }
    } else {
      gamepadsvc->RemoveGamepad(mGlobalID);
      if (gService) {
        gService->RemoveGamepad(mLocalID);
      }
    }
    return NS_OK;
  }

private:
  // ID in WindowsGamepadService::mGamepads
  int mLocalID;
  nsCString mName;
  int mGamepadButtons;
  int mGamepadAxes;
  int mGamepadHats;
  // ID from GamepadService
  uint32_t mGlobalID;
  Type mType;
};

WindowsGamepadService::WindowsGamepadService()
  : mThreadExitEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr)),
    mThreadRescanEvent(CreateEventW(nullptr, FALSE, FALSE, nullptr)),
    mThread(nullptr),
    mMutex("Windows Gamepad Service"),
    dinput(nullptr) {
  mObserver = new Observer(*this);
  // Initialize DirectInput
  CoInitialize(nullptr);
  if (CoCreateInstance(CLSID_DirectInput8,
                       nullptr,
                       CLSCTX_INPROC_SERVER,
                       IID_IDirectInput8W,
                       (LPVOID*)&dinput) == S_OK) {
    if (dinput->Initialize(GetModuleHandle(nullptr),
                           DIRECTINPUT_VERSION) != DI_OK) {
      dinput->Release();
      dinput = nullptr;
    }
  }
}

// static
BOOL CALLBACK
WindowsGamepadService::EnumObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi,
                                           LPVOID pvRef) {
  // Ensure that all axes are using the same range.
  Gamepad* gamepad = reinterpret_cast<Gamepad*>(pvRef);
  DIPROPRANGE dp;
  dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
  dp.diph.dwSize = sizeof(DIPROPRANGE);
  dp.diph.dwHow = DIPH_BYID;
  dp.diph.dwObj = lpddoi->dwType;
  dp.lMin = 0;
  dp.lMax = kMaxAxisValue;
  gamepad->device->SetProperty(DIPROP_RANGE, &dp.diph);
  return DIENUM_CONTINUE;
}

// static
BOOL CALLBACK
WindowsGamepadService::EnumCallback(LPCDIDEVICEINSTANCE lpddi,
                                    LPVOID pvRef) {
  WindowsGamepadService* self =
    reinterpret_cast<WindowsGamepadService*>(pvRef);
  // See if this device is already present in our list.
  {
    MutexAutoLock lock(self->mMutex);
    for (unsigned int i = 0; i < self->mGamepads.Length(); i++) {
      if (memcmp(&lpddi->guidInstance, &self->mGamepads[i].guidInstance,
                 sizeof(GUID)) == 0) {
        self->mGamepads[i].present = true;
        return DIENUM_CONTINUE;
      }
    }
  }

  Gamepad gamepad;
  memset(&gamepad, 0, sizeof(Gamepad));
  if (self->dinput->CreateDevice(lpddi->guidInstance,
                                 getter_AddRefs(gamepad.device),
                                 nullptr)
      == DI_OK) {
    gamepad.present = true;
    memcpy(&gamepad.guidInstance, &lpddi->guidInstance, sizeof(GUID));

    DIDEVICEINSTANCE info;
    info.dwSize = sizeof(DIDEVICEINSTANCE);
    if (gamepad.device->GetDeviceInfo(&info) == DI_OK) {
      WideCharToMultiByte(CP_UTF8, 0, info.tszProductName, -1,
                          gamepad.name, sizeof(gamepad.name), nullptr, nullptr);
    }
    // Get vendor id and product id
    DIPROPDWORD dp;
    dp.diph.dwSize = sizeof(DIPROPDWORD);
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwObj = 0;
    dp.diph.dwHow = DIPH_DEVICE;
    if (gamepad.device->GetProperty(DIPROP_VIDPID, &dp.diph) == DI_OK) {
      sprintf(gamepad.idstring, "%x-%x-%s",
              LOWORD(dp.dwData), HIWORD(dp.dwData), gamepad.name);
    }
    DIDEVCAPS caps;
    caps.dwSize = sizeof(DIDEVCAPS);
    if (gamepad.device->GetCapabilities(&caps) == DI_OK) {
      gamepad.numAxes = caps.dwAxes;
      gamepad.numHats = caps.dwPOVs;
      gamepad.numButtons = caps.dwButtons;
      //XXX: handle polled devices?
      // (caps.dwFlags & DIDC_POLLEDDATAFORMAT || caps.dwFlags & DIDC_POLLEDDEVICE)
    }
    // Set min/max range for all axes on the device.
    gamepad.device->EnumObjects(EnumObjectsCallback, &gamepad, DIDFT_AXIS);
    // Set up structure for setting buffer size for buffered data
    dp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dp.diph.dwSize = sizeof(DIPROPDWORD);
    dp.diph.dwObj = 0;
    dp.diph.dwHow = DIPH_DEVICE;
    dp.dwData = 64; // arbitrary
    // Create event so DInput can signal us when there's new data.
    gamepad.event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // Set data format, event notification, and acquire device
    if (gamepad.device->SetDataFormat(&c_dfDIJoystick) == DI_OK &&
        gamepad.device->SetProperty(DIPROP_BUFFERSIZE, &dp.diph) == DI_OK &&
        gamepad.device->SetEventNotification(gamepad.event) == DI_OK &&
        gamepad.device->Acquire() == DI_OK) {
      MutexAutoLock lock(self->mMutex);
      self->mGamepads.AppendElement(gamepad);
      // Inform the GamepadService
      int localID = self->mGamepads.Length() - 1;
      nsRefPtr<GamepadChangeEvent> event =
        new GamepadChangeEvent(self->mGamepads[localID],
                               localID,
                               GamepadChangeEvent::Added);
      NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    }
    else {
      if (gamepad.device) {
        gamepad.device->SetEventNotification(nullptr);
      }
      CloseHandle(gamepad.event);
    }
  }
  return DIENUM_CONTINUE;
}

void
WindowsGamepadService::ScanForDevices() {
  {
    MutexAutoLock lock(mMutex);
    for (int i = mGamepads.Length() - 1; i >= 0; i--) {
      if (mGamepads[i].remove) {

        // Main thread has already handled this, safe to remove.
        CleanupGamepad(mGamepads[i]);
        mGamepads.RemoveElementAt(i);
      } else {
        mGamepads[i].present = false;
      }
    }
  }

  dinput->EnumDevices(DI8DEVCLASS_GAMECTRL,
                      (LPDIENUMDEVICESCALLBACK)EnumCallback,
                      this,
                      DIEDFL_ATTACHEDONLY);

  // Look for devices that are no longer present and inform the main thread.
  {
    MutexAutoLock lock(mMutex);
    for (int i = mGamepads.Length() - 1; i >= 0; i--) {
      if (!mGamepads[i].present) {
        nsRefPtr<GamepadChangeEvent> event =
          new GamepadChangeEvent(mGamepads[i],
                                 i,
                                 GamepadChangeEvent::Removed);
        NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
      }
    }

    mEvents.Clear();
    for (unsigned int i = 0; i < mGamepads.Length(); i++) {
      mEvents.AppendElement(mGamepads[i].event);
    }
  }


  // These events must be the  last elements in the array, so that
  // the other elements match mGamepads in order.
  mEvents.AppendElement(mThreadRescanEvent);
  mEvents.AppendElement(mThreadExitEvent);
}

// static
DWORD WINAPI
WindowsGamepadService::DInputThread(LPVOID arg) {
  WindowsGamepadService* self = reinterpret_cast<WindowsGamepadService*>(arg);
  self->ScanForDevices();

  while (true) {
    DWORD result = WaitForMultipleObjects(self->mEvents.Length(),
                                          self->mEvents.Elements(),
                                          FALSE,
                                          INFINITE);
    if (result == WAIT_FAILED ||
        result == WAIT_OBJECT_0 + self->mEvents.Length() - 1) {
      // error, or the main thread signaled us to exit
      break;
    }

    unsigned int i = result - WAIT_OBJECT_0;

    if (i == self->mEvents.Length() - 2) {
      // Main thread is signaling for a device rescan.
      self->ScanForDevices();
      continue;
    }

    {
      MutexAutoLock lock(self->mMutex);
      if (i >= self->mGamepads.Length()) {
        // Something would be terribly wrong here, possibly we got
        // a WAIT_ABANDONED_x result.
        continue;
      }

      // first query for the number of items in the buffer
      DWORD items = INFINITE;
      nsRefPtr<IDirectInputDevice8> device = self->mGamepads[i].device;
      if (device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),
                                nullptr,
                                &items,
                                DIGDD_PEEK)== DI_OK) {
        while (items > 0) {
          // now read each buffered event
          //TODO: read more than one event at a time
          DIDEVICEOBJECTDATA data;
          DWORD readCount = sizeof(data) / sizeof(DIDEVICEOBJECTDATA);
          if (device->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),
                                    &data, &readCount, 0) == DI_OK) {
            //TODO: data.dwTimeStamp
            GamepadEvent::Type type = GamepadEvent::Unknown;
            int which;
            if (data.dwOfs >= DIJOFS_BUTTON0 && data.dwOfs < DIJOFS_BUTTON(32)) {
              type = GamepadEvent::Button;
              which = data.dwOfs - DIJOFS_BUTTON0;
            }
            else if(data.dwOfs >= DIJOFS_X  && data.dwOfs < DIJOFS_SLIDER(2)) {
              // axis/slider
              type = GamepadEvent::Axis;
              which = (data.dwOfs - DIJOFS_X) / sizeof(LONG);
            }
            else if (data.dwOfs >= DIJOFS_POV(0) && data.dwOfs < DIJOFS_POV(4)) {
              HatState hatState;
              HatPosToAxes(data.dwData, hatState);
              which = (data.dwOfs - DIJOFS_POV(0)) / sizeof(DWORD);
              // Only send out axis move events for the axes that moved
              // in this hat move.
              if (hatState.x != self->mGamepads[i].hatState[which].x) {
                type = GamepadEvent::HatX;
              }
              if (hatState.y != self->mGamepads[i].hatState[which].y) {
                if (type == GamepadEvent::HatX) {
                  type = GamepadEvent::HatXY;
                }
                else {
                  type = GamepadEvent::HatY;
                }
              }
              self->mGamepads[i].hatState[which].x = hatState.x;
              self->mGamepads[i].hatState[which].y = hatState.y;
            }

            if (type != GamepadEvent::Unknown) {
              nsRefPtr<GamepadEvent> event =
                new GamepadEvent(self->mGamepads[i], type, which, data.dwData);
              NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
            }
          }
          items--;
        }
      }
    }
  }
  return 0;
}

void
WindowsGamepadService::Startup() {
  mThread = CreateThread(nullptr,
                         0,
                         DInputThread,
                         this,
                         0,
                         nullptr);
}

void
WindowsGamepadService::Shutdown() {
  if (mThread) {
    SetEvent(mThreadExitEvent);
    WaitForSingleObject(mThread, INFINITE);
    CloseHandle(mThread);
  }
  Cleanup();
}

// This method is called from the main thread.
void
WindowsGamepadService::SetGamepadID(int localID, int globalID) {
  MutexAutoLock lock(mMutex);
  mGamepads[localID].globalID = globalID;
}

// This method is called from the main thread.
void WindowsGamepadService::RemoveGamepad(int localID) {
  MutexAutoLock lock(mMutex);
  mGamepads[localID].remove = true;
  // Signal background thread to remove device.
  DevicesChanged(DeviceChangeStable);
}

void
WindowsGamepadService::Cleanup() {
  for (unsigned int i = 0; i < mGamepads.Length(); i++) {
    CleanupGamepad(mGamepads[i]);
  }
  mGamepads.Clear();
}

void
WindowsGamepadService::CleanupGamepad(Gamepad& gamepad) {
  gamepad.device->Unacquire();
  gamepad.device->SetEventNotification(nullptr);
  CloseHandle(gamepad.event);
}

void
WindowsGamepadService::DevicesChanged(DeviceChangeType type) {
  if (type == DeviceChangeNotification) {
    mObserver->SetDeviceChangeTimer();
  } else if (type == DeviceChangeStable) {
    SetEvent(mThreadRescanEvent);
  }
}

NS_IMETHODIMP
Observer::Observe(nsISupports* aSubject,
                  const char* aTopic,
                  const char16_t* aData) {
  if (strcmp(aTopic, "timer-callback") == 0) {
    mSvc.DevicesChanged(WindowsGamepadService::DeviceChangeStable);
  } else if (strcmp(aTopic, NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID) == 0) {
    Stop();
  }
  return NS_OK;
}

HWND sHWnd = nullptr;

static
LRESULT CALLBACK
GamepadWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  const unsigned int DBT_DEVICEARRIVAL        = 0x8000;
  const unsigned int DBT_DEVICEREMOVECOMPLETE = 0x8004;
  const unsigned int DBT_DEVNODES_CHANGED     = 0x7;

  if (msg == WM_DEVICECHANGE &&
      (wParam == DBT_DEVICEARRIVAL ||
       wParam == DBT_DEVICEREMOVECOMPLETE ||
       wParam == DBT_DEVNODES_CHANGED)) {
    if (gService) {
      gService->DevicesChanged(WindowsGamepadService::DeviceChangeNotification);
    }
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

namespace mozilla {
namespace hal_impl {

void StartMonitoringGamepadStatus()
{
  if (gService)
    return;

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
  }
}

void StopMonitoringGamepadStatus()
{
  if (!gService)
    return;

  if (sHWnd) {
    DestroyWindow(sHWnd);
    sHWnd = nullptr;
  }

  gService->Shutdown();
  delete gService;
  gService = nullptr;
}

} // namespace hal_impl
} // namespace mozilla


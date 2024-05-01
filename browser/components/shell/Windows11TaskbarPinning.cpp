/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Windows11TaskbarPinning.h"
#include "Windows11LimitedAccessFeatures.h"

#include "nsWindowsHelpers.h"
#include "MainThreadUtils.h"
#include "nsThreadUtils.h"
#include <strsafe.h>

#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"

#include "mozilla/Logging.h"

static mozilla::LazyLogModule sLog("Windows11TaskbarPinning");

#define TASKBAR_PINNING_LOG(level, msg, ...) \
  MOZ_LOG(sLog, level, (msg, ##__VA_ARGS__))

#ifndef __MINGW32__  // WinRT headers not yet supported by MinGW

#  include <wrl.h>

#  include <inspectable.h>
#  include <roapi.h>
#  include <windows.services.store.h>
#  include <windows.foundation.h>
#  include <windows.ui.shell.h>

using namespace mozilla;

/**
 * The Win32 SetEvent and WaitForSingleObject functions take HANDLE parameters
 * which are typedefs of void*. When using nsAutoHandle, that means if you
 * forget to call .get() first, everything still compiles and then doesn't work
 * at runtime. For instance, calling SetEvent(mEvent) below would compile but
 * not work at runtime and the waits would block forever.
 * To ensure this isn't an issue, we wrap the event in a custom class here
 * with the simple methods that we want on an event.
 */
class EventWrapper {
 public:
  EventWrapper() : mEvent(CreateEventW(nullptr, true, false, nullptr)) {}

  void Set() { SetEvent(mEvent.get()); }

  void Reset() { ResetEvent(mEvent.get()); }

  void Wait() { WaitForSingleObject(mEvent.get(), INFINITE); }

 private:
  nsAutoHandle mEvent;
};

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows;
using namespace ABI::Windows::UI::Shell;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::ApplicationModel;

static Result<ComPtr<ITaskbarManager>, HRESULT> InitializeTaskbar() {
  ComPtr<IInspectable> taskbarStaticsInspectable;

  TASKBAR_PINNING_LOG(LogLevel::Debug, "Initializing taskbar");

  HRESULT hr = RoGetActivationFactory(
      HStringReference(RuntimeClass_Windows_UI_Shell_TaskbarManager).Get(),
      IID_ITaskbarManagerStatics, &taskbarStaticsInspectable);
  if (FAILED(hr)) {
    TASKBAR_PINNING_LOG(LogLevel::Debug,
                        "Taskbar: Failed to activate. HRESULT = 0x%lx", hr);
    return Err(hr);
  }

  ComPtr<ITaskbarManagerStatics> taskbarStatics;

  hr = taskbarStaticsInspectable.As(&taskbarStatics);
  if (FAILED(hr)) {
    TASKBAR_PINNING_LOG(LogLevel::Debug, "Failed statistics. HRESULT = 0x%lx",
                        hr);
    return Err(hr);
  }

  ComPtr<ITaskbarManager> taskbarManager;

  hr = taskbarStatics->GetDefault(&taskbarManager);
  if (FAILED(hr)) {
    TASKBAR_PINNING_LOG(LogLevel::Debug,
                        "Error getting TaskbarManager. HRESULT = 0x%lx", hr);
    return Err(hr);
  }

  TASKBAR_PINNING_LOG(LogLevel::Debug,
                      "TaskbarManager retrieved successfully!");
  return taskbarManager;
}

Win11PinToTaskBarResult PinCurrentAppToTaskbarWin11(
    bool aCheckOnly, const nsAString& aAppUserModelId,
    nsAutoString aShortcutPath) {
  MOZ_DIAGNOSTIC_ASSERT(!NS_IsMainThread(),
                        "PinCurrentAppToTaskbarWin11 should be called off main "
                        "thread only. It blocks, waiting on things to execute "
                        "asynchronously on the main thread.");

  {
    RefPtr<Win11LimitedAccessFeaturesInterface> limitedAccessFeatures =
        CreateWin11LimitedAccessFeaturesInterface();
    auto result =
        limitedAccessFeatures->Unlock(Win11LimitedAccessFeatureType::Taskbar);
    if (result.isErr()) {
      auto hr = result.unwrapErr();
      TASKBAR_PINNING_LOG(LogLevel::Debug,
                          "Taskbar unlock: Error. HRESULT = 0x%lx", hr);
      return {hr, Win11PinToTaskBarResultStatus::NotSupported};
    }

    if (result.unwrap() == false) {
      TASKBAR_PINNING_LOG(
          LogLevel::Debug,
          "Taskbar unlock: failed. Not supported on this version of Windows.");
      return {S_OK, Win11PinToTaskBarResultStatus::NotSupported};
    }
  }

  HRESULT hr;
  Win11PinToTaskBarResultStatus resultStatus =
      Win11PinToTaskBarResultStatus::NotSupported;

  EventWrapper event;

  // Everything related to the taskbar and pinning must be done on the main /
  // user interface thread or Windows will cause them to fail.
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "PinCurrentAppToTaskbarWin11", [&event, &hr, &resultStatus, aCheckOnly] {
        auto CompletedOperations =
            [&event, &resultStatus](Win11PinToTaskBarResultStatus status) {
              resultStatus = status;
              event.Set();
            };

        auto result = InitializeTaskbar();
        if (result.isErr()) {
          hr = result.unwrapErr();
          return CompletedOperations(
              Win11PinToTaskBarResultStatus::NotSupported);
        }

        ComPtr<ITaskbarManager> taskbar = result.unwrap();
        boolean supported;
        hr = taskbar->get_IsSupported(&supported);
        if (FAILED(hr) || !supported) {
          if (FAILED(hr)) {
            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: error checking if supported. HRESULT = 0x%lx", hr);
          } else {
            TASKBAR_PINNING_LOG(LogLevel::Debug, "Taskbar: not supported.");
          }
          return CompletedOperations(
              Win11PinToTaskBarResultStatus::NotSupported);
        }

        if (aCheckOnly) {
          TASKBAR_PINNING_LOG(LogLevel::Debug, "Taskbar: check succeeded.");
          return CompletedOperations(Win11PinToTaskBarResultStatus::Success);
        }

        boolean isAllowed = false;
        hr = taskbar->get_IsPinningAllowed(&isAllowed);
        if (FAILED(hr) || !isAllowed) {
          if (FAILED(hr)) {
            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: error checking if pinning is allowed. HRESULT = "
                "0x%lx",
                hr);
          } else {
            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: is pinning allowed error or isn't allowed right now. "
                "It's not clear when it will be allowed. Possibly after a "
                "reboot.");
          }
          return CompletedOperations(
              Win11PinToTaskBarResultStatus::NotCurrentlyAllowed);
        }

        ComPtr<IAsyncOperation<bool>> isPinnedOperation = nullptr;
        hr = taskbar->IsCurrentAppPinnedAsync(&isPinnedOperation);
        if (FAILED(hr)) {
          TASKBAR_PINNING_LOG(
              LogLevel::Debug,
              "Taskbar: is current app pinned operation failed. HRESULT = "
              "0x%lx",
              hr);
          return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
        }

        // Copy the taskbar; don't use it as a reference.
        // With the async calls, it's not guaranteed to still be valid
        // if sent as a reference.
        // resultStatus and event are not defined on the main thread and will
        // be alive until the async functions complete, so they can be used as
        // references.
        auto isPinnedCallback = Callback<IAsyncOperationCompletedHandler<
            bool>>([taskbar, &event, &resultStatus, &hr](
                       IAsyncOperation<bool>* asyncInfo,
                       AsyncStatus status) mutable -> HRESULT {
          auto CompletedOperations =
              [&event,
               &resultStatus](Win11PinToTaskBarResultStatus status) -> HRESULT {
            resultStatus = status;
            event.Set();
            return S_OK;
          };

          bool asyncOpSucceeded = status == AsyncStatus::Completed;
          if (!asyncOpSucceeded) {
            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: is pinned operation failed to complete.");
            return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
          }

          unsigned char isCurrentAppPinned = false;
          hr = asyncInfo->GetResults(&isCurrentAppPinned);
          if (FAILED(hr)) {
            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: is current app pinned check failed. HRESULT = 0x%lx",
                hr);
            return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
          }

          if (isCurrentAppPinned) {
            TASKBAR_PINNING_LOG(LogLevel::Debug,
                                "Taskbar: current app is already pinned.");
            return CompletedOperations(
                Win11PinToTaskBarResultStatus::AlreadyPinned);
          }

          ComPtr<IAsyncOperation<bool>> requestPinOperation = nullptr;
          hr = taskbar->RequestPinCurrentAppAsync(&requestPinOperation);
          if (FAILED(hr)) {
            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: request pin current app operation creation failed. "
                "HRESULT = 0x%lx",
                hr);
            return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
          }

          auto pinAppCallback = Callback<IAsyncOperationCompletedHandler<
              bool>>([CompletedOperations, &hr](
                         IAsyncOperation<bool>* asyncInfo,
                         AsyncStatus status) -> HRESULT {
            bool asyncOpSucceeded = status == AsyncStatus::Completed;
            if (!asyncOpSucceeded) {
              TASKBAR_PINNING_LOG(
                  LogLevel::Debug,
                  "Taskbar: request pin current app operation did not "
                  "complete.");
              return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
            }

            unsigned char successfullyPinned = 0;
            hr = asyncInfo->GetResults(&successfullyPinned);
            if (FAILED(hr) || !successfullyPinned) {
              if (FAILED(hr)) {
                TASKBAR_PINNING_LOG(
                    LogLevel::Debug,
                    "Taskbar: request pin current app operation failed to pin "
                    "due to error. HRESULT = 0x%lx",
                    hr);
              } else {
                TASKBAR_PINNING_LOG(
                    LogLevel::Debug,
                    "Taskbar: request pin current app operation failed to pin");
              }
              return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
            }

            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: request pin current app operation succeeded");
            return CompletedOperations(Win11PinToTaskBarResultStatus::Success);
          });

          HRESULT pinOperationHR =
              requestPinOperation->put_Completed(pinAppCallback.Get());
          if (FAILED(pinOperationHR)) {
            TASKBAR_PINNING_LOG(
                LogLevel::Debug,
                "Taskbar: request pin operation failed when setting completion "
                "callback. HRESULT = 0x%lx",
                hr);
            hr = pinOperationHR;
            return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
          }

          // DO NOT SET event HERE. It will be set in the pin operation
          // callback As in, operations are not completed, so don't call
          // CompletedOperations
          return S_OK;
        });

        HRESULT isPinnedOperationHR =
            isPinnedOperation->put_Completed(isPinnedCallback.Get());
        if (FAILED(isPinnedOperationHR)) {
          hr = isPinnedOperationHR;
          TASKBAR_PINNING_LOG(
              LogLevel::Debug,
              "Taskbar: is pinned operation failed when setting completion "
              "callback. HRESULT = 0x%lx",
              hr);
          return CompletedOperations(Win11PinToTaskBarResultStatus::Failed);
        }

        // DO NOT SET event HERE. It will be set in the is pin operation
        // callback As in, operations are not completed, so don't call
        // CompletedOperations
      }));

  // block until the pinning is completed on the main thread
  event.Wait();

  return {hr, resultStatus};
}

#else  // MINGW32 implementation below

Win11PinToTaskBarResult PinCurrentAppToTaskbarWin11(
    bool aCheckOnly, const nsAString& aAppUserModelId,
    nsAutoString aShortcutPath) {
  return {S_OK, Win11PinToTaskBarResultStatus::NotSupported};
}

#endif  // #ifndef __MINGW32__  // WinRT headers not yet supported by MinGW

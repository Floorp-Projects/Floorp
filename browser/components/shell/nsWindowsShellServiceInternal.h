/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nswindowsshellserviceinternal_h____
#define nswindowsshellserviceinternal_h____

#include "nsString.h"
#include <mozilla/DefineEnum.h>
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/UniquePtr.h"
#include <windows.h>
#include "mozilla/glean/GleanMetrics.h"
#include "Windows11TaskbarPinning.h"

/**
 * Namespace to wrap recording of telemetry and separate it from everything
 * else. Should be easy to move into a class if it needs to be stubbed later for
 * testing purposes.
 */
namespace telemetry {
namespace shortcut {

static const auto TELEMETRY_SUCCESS = "Success"_ns;
static const auto TELEMETRY_FILE_NOT_FOUND = "FileNotFound"_ns;
static const auto TELEMETRY_ERROR = "Error"_ns;

inline mozilla::glean::pinning_windows::ShortcutCreatedExtra CreateEventExtra(
    nsresult result, bool aPrivateBrowsing) {
  using namespace mozilla::glean::pinning_windows;
  using namespace mozilla;

  switch (result) {
    case NS_OK:
      return ShortcutCreatedExtra{.privateBrowsing = Some(aPrivateBrowsing),
                                  .result = Some(TELEMETRY_SUCCESS),
                                  .success = Some(true)};

    case NS_ERROR_FILE_NOT_FOUND:
      return ShortcutCreatedExtra{.fileNotFound = Some(true),
                                  .privateBrowsing = Some(aPrivateBrowsing),
                                  .result = Some(TELEMETRY_FILE_NOT_FOUND)};

    default:
      return ShortcutCreatedExtra{.error = Some(true),
                                  .privateBrowsing = Some(aPrivateBrowsing),
                                  .result = Some(TELEMETRY_ERROR)};
  }
}

inline void Record(bool aCheckOnly, nsresult result, bool aPrivateBrowsing) {
  using namespace mozilla::glean::pinning_windows;
  using namespace mozilla;

  if (!aCheckOnly) {
    shortcut_created.Record(Some(CreateEventExtra(result, aPrivateBrowsing)));
  }
}

}  // namespace shortcut

namespace pinning {

static const auto TELEMETRY_NOT_SUPPORTED = "NotSupported"_ns;
static const auto TELEMETRY_SUCCESS = "Success"_ns;
static const auto TELEMETRY_ALREADY_PINNED = "AlreadyPinned"_ns;
static const auto TELEMETRY_NOT_CURRENTLY_ALLOWED = "NotCurrentlyAllowed"_ns;
static const auto TELEMETRY_ERROR = "Error"_ns;
static const auto TELEMETRY_ERROR_LIMITED_ACCESS_FEATURES =
    "ErrorLimitedAccessFeatures"_ns;
static const auto TELEMETRY_LIMITED_ACCESS_FEATURES_LOCKED =
    "LimitedAccessFeaturesLocked"_ns;

inline mozilla::glean::pinning_windows::PinnedToTaskbarExtra CreateEventExtra(
    Win11PinToTaskBarResult result, bool aPrivateBrowsing,
    bool fallbackSucceeded) {
  using namespace mozilla::glean::pinning_windows;
  using namespace mozilla;

  const auto fallbackResultString =
      fallbackSucceeded ? TELEMETRY_SUCCESS : TELEMETRY_ERROR;

  auto resultStatus = result.result;
  switch (resultStatus) {
    case Win11PinToTaskBarResultStatus::NotSupported:
      return {.fallbackPinningSuccess = Some(fallbackResultString),
              .notSupported = Some(true),
              .numberOfAttempts = Some(result.numAttempts),
              .privateBrowsing = Some(aPrivateBrowsing),
              .result = Some(TELEMETRY_NOT_SUPPORTED)};

    case Win11PinToTaskBarResultStatus::Success:
      return {.fallbackPinningSuccess = Some(fallbackResultString),
              .numberOfAttempts = Some(result.numAttempts),
              .privateBrowsing = Some(aPrivateBrowsing),
              .result = Some(TELEMETRY_SUCCESS),
              .success = Some(true)};

    case Win11PinToTaskBarResultStatus::AlreadyPinned:
      return {.alreadyPinned = Some(true),
              .fallbackPinningSuccess = Some(fallbackResultString),
              .numberOfAttempts = Some(result.numAttempts),
              .privateBrowsing = Some(aPrivateBrowsing),
              .result = Some(TELEMETRY_ALREADY_PINNED)};

    case Win11PinToTaskBarResultStatus::NotCurrentlyAllowed:
      return {.fallbackPinningSuccess = Some(fallbackResultString),
              .notCurrentlyAllowed = Some(true),
              .numberOfAttempts = Some(result.numAttempts),
              .privateBrowsing = Some(aPrivateBrowsing),
              .result = Some(TELEMETRY_NOT_CURRENTLY_ALLOWED)};

    case Win11PinToTaskBarResultStatus::ErrorLimitedAccessFeatures:
      return {.errorLimitedAccessFeatures = Some(true),
              .fallbackPinningSuccess = Some(fallbackResultString),
              .numberOfAttempts = Some(result.numAttempts),
              .privateBrowsing = Some(aPrivateBrowsing),
              .result = Some(TELEMETRY_ERROR_LIMITED_ACCESS_FEATURES)};

    case Win11PinToTaskBarResultStatus::LimitedAccessFeaturesLocked:
      return {.fallbackPinningSuccess = Some(fallbackResultString),
              .limitedAccessFeaturesLocked = Some(true),
              .numberOfAttempts = Some(result.numAttempts),
              .privateBrowsing = Some(aPrivateBrowsing),
              .result = Some(TELEMETRY_LIMITED_ACCESS_FEATURES_LOCKED)};

    case Win11PinToTaskBarResultStatus::Failed:
    default:
      return {.error = Some(true),
              .fallbackPinningSuccess = Some(fallbackResultString),
              .numberOfAttempts = Some(result.numAttempts),
              .privateBrowsing = Some(aPrivateBrowsing),
              .result = Some(TELEMETRY_ERROR)};
  }
}

inline void Record(bool aCheckOnly, Win11PinToTaskBarResult result,
                   bool aPrivateBrowsing, bool fallbackSucceeded) {
  using namespace mozilla::glean::pinning_windows;
  using namespace mozilla;

  if (!aCheckOnly) {
    pinned_to_taskbar.Record(
        Some(CreateEventExtra(result, aPrivateBrowsing, fallbackSucceeded)));
  }
}
}  // namespace pinning

}  // namespace telemetry

/**
 * Helper class to wrap calling the logic for taskbar pinning. Specifically done
 * this way so that it can be stubbed out with a mock implementation for testing
 * metrics are collected properly. The base class includes the default
 * implementation that should be used outside of testing and is a light wrapper
 * around static functions.
 */
class PinCurrentAppToTaskbarHelper {
 public:
  virtual ~PinCurrentAppToTaskbarHelper() {}

  virtual void CheckNotMainThread();

  virtual mozilla::Result<bool, nsresult> CreateShortcutForTaskbar(
      bool aCheckOnly, bool aPrivateBrowsing, const nsAString& aAppUserModelId,
      const nsAString& aShortcutName, const nsAString& aShortcutSubstring,
      nsIFile* aShortcutsLogDir, nsIFile* aGreDir, nsIFile* aProgramsDir,
      nsAutoString& aShortcutPath);

  virtual Win11PinToTaskBarResult PinCurrentAppViaAPI(
      bool aCheckOnly, const nsAString& aAppUserModelId,
      nsAutoString aShortcutPath);
  virtual nsresult PinCurrentAppFallback(bool aCheckOnly,
                                         const nsAString& aAppUserModelId,
                                         const nsAString& aShortcutPath);
};

class nsIFile;

nsresult PinCurrentAppToTaskbarImpl(
    bool aCheckOnly, bool aPrivateBrowsing, const nsAString& aAppUserModelId,
    const nsAString& aShortcutName, const nsAString& aShortcutSubstring,
    nsIFile* aShortcutsLogDir, nsIFile* aGreDir, nsIFile* aProgramsDir,
    mozilla::UniquePtr<PinCurrentAppToTaskbarHelper> helper =
        mozilla::UniquePtr<PinCurrentAppToTaskbarHelper>());

#endif  // nswindowsshellserviceinternal_h____

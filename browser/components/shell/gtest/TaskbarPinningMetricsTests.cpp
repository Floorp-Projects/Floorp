/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWindowsShellServiceInternal.h"
#include "gtest/gtest.h"
#include "gtest/FOGFixture.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include <iostream>

using namespace mozilla;
using namespace mozilla::glean::impl;
using namespace mozilla::glean::pinning_windows;

namespace telemetry_event {

template <typename event_type>
bool IsValidButEmpty(const event_type& event) {
  auto value = event.TestGetValue();
  if (!value.isOk()) {
    return false;
  }

  if (value.unwrap().isSome()) {
    return false;
  }

  return true;
}

template <typename event_type>
bool CountsMatch(const event_type& event, size_t count) {
  auto value = event.TestGetValue();
  if (!value.isOk()) {
    return false;
  }

  auto unwrapped = value.unwrap();
  if (!unwrapped.isSome()) {
    return false;
  }

  if (unwrapped.ref().Length() != count) {
    return false;
  }

  return true;
}

}  // namespace telemetry_event

template <typename event_extra_type>
static std::map<nsCString, nsCString> CreateKeyValuePairs(
    const event_extra_type& eventExtras) {
  auto serializedExtras = eventExtras.ToFfiExtra();
  auto keys = std::move(std::get<0>(serializedExtras));
  auto values = std::move(std::get<1>(serializedExtras));

  if (keys.Length() != values.Length()) {
    MOZ_ASSERT(false);
    return std::map<nsCString, nsCString>();
  }

  std::map<nsCString, nsCString> result;
  for (size_t i = 0; i < keys.Length(); i++) {
    result[keys[i]] = values[i];
  }

  return result;
}

template <typename extra_type>
bool IsEqual(const extra_type& control,
             mozilla::glean::impl::RecordedEvent& event) {
  // generate a map from the control data
  std::map<nsCString, nsCString> controlMap = CreateKeyValuePairs(control);

  if (controlMap.size() != event.mExtra.Length()) {
    return false;
  }

  // now walk through the event data and check
  for (size_t i = 0; i < event.mExtra.Length(); i++) {
    const auto& key = std::get<0>(event.mExtra[i]);
    const auto& value = std::get<1>(event.mExtra[i]);

    auto iterator = controlMap.find(key);
    if (iterator == controlMap.end()) {
      return false;
    }

    if (iterator->second != value) {
      return false;
    }
  }

  return true;
}

template <typename extra_type>
bool IsEqual(const extra_type& control, const extra_type& generated) {
  std::map<nsCString, nsCString> controlMap = CreateKeyValuePairs(control);
  std::map<nsCString, nsCString> generatedMap = CreateKeyValuePairs(generated);

  if (controlMap.size() != generatedMap.size()) {
    return false;
  }

  for (auto i : controlMap) {
    auto found = generatedMap.find(i.first);
    if (found == generatedMap.end()) {
      return false;
    }

    if (found->second != i.second) {
      return false;
    }
  }

  return true;
}

class WindowsPinningShortcutTestFixture : public FOGFixture {};

MOZ_DEFINE_ENUM_CLASS(PinningShortcutOperation,
                      (Succeed, FailFileNotFound, FailError, FailEmptyOnFind));

static const char* EnumName(PinningShortcutOperation operation) {
  switch (operation) {
    case PinningShortcutOperation::Succeed:
      return "PinningShortcutOperation::Succeed";

    case PinningShortcutOperation::FailFileNotFound:
      return "PinningShortcutOperation::FailFileNotFound";

    case PinningShortcutOperation::FailError:
      return "PinningShortcutOperation::FailError";

    case PinningShortcutOperation::FailEmptyOnFind:
      return "PinningShortcutOperation::FailEmptyOnFind";

    default:
      MOZ_ASSERT(false);
      return "PinningShortcutOperation::UNDEFINED";
  }
}

MOZ_DEFINE_ENUM_CLASS(PinningFallbackOperation, (Succeed, Fail));

static const char* EnumName(PinningFallbackOperation operation) {
  switch (operation) {
    case PinningFallbackOperation::Succeed:
      return "PinningFallbackOperation::Succeed";

    case PinningFallbackOperation::Fail:
      return "PinningFallbackOperation::Fail";

    default:
      MOZ_ASSERT(false);
      return "PinningFallbackOperation::UNDEFINED";
  }
}

static const char* EnumName(Win11PinToTaskBarResultStatus operation) {
  switch (operation) {
    case Win11PinToTaskBarResultStatus::Success:
      return "Win11PinToTaskBarResultStatus::Success";

    case Win11PinToTaskBarResultStatus::Failed:
      return "Win11PinToTaskBarResultStatus::Failed";

    case Win11PinToTaskBarResultStatus::NotCurrentlyAllowed:
      return "Win11PinToTaskBarResultStatus::NotCurrentlyAllowed";

    case Win11PinToTaskBarResultStatus::AlreadyPinned:
      return "Win11PinToTaskBarResultStatus::AlreadyPinned";

    case Win11PinToTaskBarResultStatus::NotSupported:
      return "Win11PinToTaskBarResultStatus::NotSupported";

    case Win11PinToTaskBarResultStatus::ErrorLimitedAccessFeatures:
      return "Win11PinToTaskBarResultStatus::ErrorLimitedAccessFeatures";

    case Win11PinToTaskBarResultStatus::LimitedAccessFeaturesLocked:
      return "Win11PinToTaskBarResultStatus::LimitedAccessFeaturesLocked";

    default:
      MOZ_ASSERT(false);
      return "Win11PinToTaskBarResultStatus::UNDEFINED";
  }
}

/**
 * Mock version of PinCurrentAppToTaskbarHelper.
 *
 * Takes the values to return from the methods and returns them
 * to mock results for testing telemetry events are generated
 * properly by PinCurrentAppToTaskbarImpl.
 */
class PinCurrentAppToTaskbarHelperMetricsTesting
    : public PinCurrentAppToTaskbarHelper {
 public:
  PinCurrentAppToTaskbarHelperMetricsTesting(
      PinningShortcutOperation shortcutOperation,
      Win11PinToTaskBarResultStatus apiOperation,
      PinningFallbackOperation fallbackOperation);

  void CheckNotMainThread() override {}

  mozilla::Result<bool, nsresult> CreateShortcutForTaskbar(
      bool aCheckOnly, bool aPrivateBrowsing, const nsAString& aAppUserModelId,
      const nsAString& aShortcutName, const nsAString& aShortcutSubstring,
      nsIFile* aShortcutsLogDir, nsIFile* aGreDir, nsIFile* aProgramsDir,
      nsAutoString& aShortcutPath) override;

  Win11PinToTaskBarResult PinCurrentAppViaAPI(
      bool aCheckOnly, const nsAString& aAppUserModelId,
      nsAutoString aShortcutPath) override;

  nsresult PinCurrentAppFallback(bool aCheckOnly,
                                 const nsAString& aAppUserModelId,
                                 const nsAString& aShortcutPath) override;

 private:
  PinningShortcutOperation mShortcutOperation;
  Win11PinToTaskBarResultStatus mApiOperation;
  PinningFallbackOperation mFallbackOperation;
};

inline PinCurrentAppToTaskbarHelperMetricsTesting::
    PinCurrentAppToTaskbarHelperMetricsTesting(
        PinningShortcutOperation shortcutOperation,
        Win11PinToTaskBarResultStatus apiOperation,
        PinningFallbackOperation fallbackOperation)
    : mShortcutOperation(shortcutOperation),
      mApiOperation(apiOperation),
      mFallbackOperation(fallbackOperation) {}

inline mozilla::Result<bool, nsresult>
PinCurrentAppToTaskbarHelperMetricsTesting::CreateShortcutForTaskbar(
    bool aCheckOnly, bool aPrivateBrowsing, const nsAString& aAppUserModelId,
    const nsAString& aShortcutName, const nsAString& aShortcutSubstring,
    nsIFile* aShortcutsLogDir, nsIFile* aGreDir, nsIFile* aProgramsDir,
    nsAutoString& aShortcutPath) {
  using namespace mozilla;

  switch (mShortcutOperation) {
    case PinningShortcutOperation::Succeed:
      return true;

    case PinningShortcutOperation::FailFileNotFound:
      return Err(NS_ERROR_FILE_NOT_FOUND);

    case PinningShortcutOperation::FailError:
      return Err(NS_ERROR_FAILURE);

    case PinningShortcutOperation::FailEmptyOnFind:
      return false;

    default:
      MOZ_ASSERT(false);
      return Err(NS_ERROR_FAILURE);
  }
}

inline Win11PinToTaskBarResult
PinCurrentAppToTaskbarHelperMetricsTesting::PinCurrentAppViaAPI(
    bool aCheckOnly, const nsAString& aAppUserModelId,
    nsAutoString aShortcutPath) {
  const int numAttempts = 1;
  return {S_OK, mApiOperation, numAttempts};
}

inline nsresult
PinCurrentAppToTaskbarHelperMetricsTesting::PinCurrentAppFallback(
    bool aCheckOnly, const nsAString& aAppUserModelId,
    const nsAString& aShortcutPath) {
  switch (mFallbackOperation) {
    case PinningFallbackOperation::Fail:
      return NS_ERROR_FAILURE;

    case PinningFallbackOperation::Succeed:
      return NS_OK;

    default:
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
  }
}

static void TestPinning(bool aCheckOnly,
                        PinningShortcutOperation shortcutOperation,
                        Win11PinToTaskBarResultStatus apiOperation,
                        PinningFallbackOperation fallbackOperation,
                        bool privateBrowsing = false) {
  UniquePtr<PinCurrentAppToTaskbarHelper> helper(
      new PinCurrentAppToTaskbarHelperMetricsTesting(
          shortcutOperation, apiOperation, fallbackOperation));

  auto appUserModelId = u""_ns;
  auto shortcutName = u""_ns;
  auto shortcutSubstring = u""_ns;
  nsIFile* shortcutsLogDir = nullptr;
  nsIFile* greDir = nullptr;
  nsIFile* programsDir = nullptr;
  PinCurrentAppToTaskbarImpl(aCheckOnly, privateBrowsing, appUserModelId,
                             shortcutName, shortcutSubstring, shortcutsLogDir,
                             greDir, programsDir, std::move(helper));
}

struct ShortcutTestCase {
  PinningShortcutOperation operation;
  nsresult expectedResult;
  ShortcutCreatedExtra expectedExtras;

  friend void PrintTo(const ShortcutTestCase& testCase, std::ostream* os) {
    *os << "(With operation " << EnumName(testCase.operation)
        << ", expected result event text == " << int(testCase.expectedResult)
        << " and expectedExtras with result text == "
        << testCase.expectedExtras.result << ")";
  }
};

class WindowsPinningShortcuts : public FOGFixtureWithParam<ShortcutTestCase> {};

TEST_P(
    WindowsPinningShortcuts,
    WindowsPinningTestShortcutTelemetryEventCreationCheckOnlyPrivateBrowsingOn) {
  auto testCase = GetParam();
  const bool checkOnly = true;
  const bool privateBrowsing = true;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(checkOnly, testCase.operation,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);

  EXPECT_TRUE(telemetry_event::IsValidButEmpty(shortcut_created));
}

TEST_P(
    WindowsPinningShortcuts,
    WindowsPinningTestShortcutTelemetryEventCreationCheckOnlyPrivateBrowsingOff) {
  auto testCase = GetParam();
  const bool checkOnly = true;
  const bool privateBrowsing = false;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(checkOnly, testCase.operation,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);

  EXPECT_TRUE(telemetry_event::IsValidButEmpty(shortcut_created));
}

TEST_P(WindowsPinningShortcuts,
       WindowsPinningTestShortcutTelemetryEventCreationPrivateBrowsingOn) {
  auto testCase = GetParam();
  const bool doFullPinningFlow = false;
  const bool privateBrowsing = true;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(doFullPinningFlow, testCase.operation,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);

  auto extras = telemetry::shortcut::CreateEventExtra(testCase.expectedResult,
                                                      privateBrowsing);

  // confirm that the event extras creation works as expected
  EXPECT_TRUE(IsEqual(testCase.expectedExtras, extras));

  size_t one = 1;
  EXPECT_TRUE(telemetry_event::CountsMatch(shortcut_created, one));

  auto metricsEvents = shortcut_created.TestGetValue().unwrap().ref();

  auto control = testCase.expectedExtras;
  EXPECT_TRUE(IsEqual(control, metricsEvents[0]));
}

TEST_P(WindowsPinningShortcuts,
       WindowsPinningTestShortcutTelemetryEventCreationPrivateBrowsingOff) {
  auto testCase = GetParam();
  const bool doFullPinningFlow = false;
  const bool privateBrowsing = false;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(doFullPinningFlow, testCase.operation,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);

  auto extras = telemetry::shortcut::CreateEventExtra(testCase.expectedResult,
                                                      privateBrowsing);

  // confirm that the event extras creation works as expected
  EXPECT_TRUE(IsEqual(testCase.expectedExtras, extras));

  size_t one = 1;
  EXPECT_TRUE(telemetry_event::CountsMatch(shortcut_created, one));

  auto metricsEvents = shortcut_created.TestGetValue().unwrap().ref();

  auto control = testCase.expectedExtras;
  EXPECT_TRUE(IsEqual(control, metricsEvents[0]));
}

static const ShortcutTestCase shortcutTestCases[] = {
    {.operation = PinningShortcutOperation::FailFileNotFound,
     .expectedResult = NS_ERROR_FILE_NOT_FOUND,
     .expectedExtras = {.fileNotFound = Some(true),
                        .privateBrowsing = Some(false),
                        .result = Some(
                            telemetry::shortcut::TELEMETRY_FILE_NOT_FOUND)}},
    {.operation = PinningShortcutOperation::FailError,
     .expectedResult = NS_ERROR_FAILURE,
     .expectedExtras = {.error = Some(true),
                        .privateBrowsing = Some(false),
                        .result = Some(telemetry::shortcut::TELEMETRY_ERROR)}},
    {.operation = PinningShortcutOperation::Succeed,
     .expectedResult = NS_OK,
     .expectedExtras = {.privateBrowsing = Some(false),
                        .result = Some(telemetry::shortcut::TELEMETRY_SUCCESS),
                        .success = Some(true)}},
};

INSTANTIATE_TEST_SUITE_P(WindowsPinningShortcutTelemetryTests,
                         WindowsPinningShortcuts,
                         ::testing::ValuesIn(shortcutTestCases));

TEST_F(WindowsPinningShortcutTestFixture,
       TestShortcutFailOnEmptyFindPrivateBrowsingOn) {
  const bool privateBrowsing = true;

  // special case FailEmptyOnFind. That should not generate an event at all
  TestPinning(true, PinningShortcutOperation::FailEmptyOnFind,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);
  EXPECT_TRUE(telemetry_event::IsValidButEmpty(shortcut_created));

  TestResetFOG();

  TestPinning(false, PinningShortcutOperation::FailEmptyOnFind,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);
  EXPECT_TRUE(telemetry_event::IsValidButEmpty(shortcut_created));
}

TEST_F(WindowsPinningShortcutTestFixture,
       TestShortcutFailOnEmptyFindPrivateBrowsingOff) {
  const bool privateBrowsing = false;

  // special case FailEmptyOnFind. That should not generate an event at all
  TestPinning(true, PinningShortcutOperation::FailEmptyOnFind,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);
  EXPECT_TRUE(telemetry_event::IsValidButEmpty(shortcut_created));

  TestResetFOG();

  TestPinning(false, PinningShortcutOperation::FailEmptyOnFind,
              Win11PinToTaskBarResultStatus::Success,
              PinningFallbackOperation::Succeed, privateBrowsing);
  EXPECT_TRUE(telemetry_event::IsValidButEmpty(shortcut_created));
}

struct PinningAPITestCase {
  Win11PinToTaskBarResultStatus result;
  PinnedToTaskbarExtra expectedExtras;

  friend void PrintTo(const PinningAPITestCase& testCase, std::ostream* os) {
    *os << "(For pinning api operation == " << EnumName(testCase.result)
        << ", expectedExtras result text should == "
        << testCase.expectedExtras.result << ")";
  }
};

class WindowsPinningAPIFixture
    : public FOGFixtureWithParam<PinningAPITestCase> {};

TEST_P(WindowsPinningAPIFixture,
       WindowsPinningTestTelemetryEventCreationCheckOnlyPrivateBrowsingOn) {
  const auto testCase = GetParam();
  const bool checkOnly = true;
  const bool privateBrowsing = true;
  TestPinning(checkOnly, PinningShortcutOperation::Succeed, testCase.result,
              PinningFallbackOperation::Succeed, privateBrowsing);

  // confirm we don't generate events when checking only
  EXPECT_TRUE(telemetry_event::IsValidButEmpty(pinned_to_taskbar));
}

TEST_P(WindowsPinningAPIFixture,
       WindowsPinningTestTelemetryEventCreationCheckOnlyPrivateBrowsingOff) {
  const auto testCase = GetParam();
  const bool checkOnly = true;
  const bool privateBrowsing = false;
  TestPinning(checkOnly, PinningShortcutOperation::Succeed, testCase.result,
              PinningFallbackOperation::Succeed, privateBrowsing);

  // confirm we don't generate events when checking only
  EXPECT_TRUE(telemetry_event::IsValidButEmpty(pinned_to_taskbar));
}

static bool isFallbackSuccess(Win11PinToTaskBarResultStatus result) {
  // fallback fails when the api succeeds
  return (result != Win11PinToTaskBarResultStatus::Success) &&
         (result != Win11PinToTaskBarResultStatus::AlreadyPinned);
}

TEST_P(WindowsPinningAPIFixture,
       WindowsPinningTestTelemetryEventCreationPrivateBrowsingOn) {
  auto testCase = GetParam();
  const bool checkOnly = false;
  const bool privateBrowsing = true;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(checkOnly, PinningShortcutOperation::Succeed, testCase.result,
              PinningFallbackOperation::Succeed, privateBrowsing);

  const bool fallbackSucceeded = isFallbackSuccess(testCase.result);
  auto extras = telemetry::pinning::CreateEventExtra(
      {S_OK, testCase.result, 1}, privateBrowsing, fallbackSucceeded);

  EXPECT_TRUE(IsEqual(testCase.expectedExtras, extras));

  size_t one = 1;

  EXPECT_TRUE(telemetry_event::CountsMatch(pinned_to_taskbar, one));

  auto metricsEvents = pinned_to_taskbar.TestGetValue().unwrap().ref();
  auto control = testCase.expectedExtras;
  EXPECT_TRUE(IsEqual(control, metricsEvents[0]));
}

TEST_P(WindowsPinningAPIFixture,
       WindowsPinningTestTelemetryEventCreationPrivateBrowsingOff) {
  auto testCase = GetParam();
  const bool checkOnly = false;
  const bool privateBrowsing = false;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(checkOnly, PinningShortcutOperation::Succeed, testCase.result,
              PinningFallbackOperation::Succeed, privateBrowsing);

  // fallback fails when the api succeeds
  const bool fallbackSucceeded = isFallbackSuccess(testCase.result);
  auto extras = telemetry::pinning::CreateEventExtra(
      {S_OK, testCase.result, 1}, privateBrowsing, fallbackSucceeded);

  EXPECT_TRUE(IsEqual(testCase.expectedExtras, extras));

  size_t one = 1;

  EXPECT_TRUE(telemetry_event::CountsMatch(pinned_to_taskbar, one));

  auto metricsEvents = pinned_to_taskbar.TestGetValue().unwrap().ref();
  auto control = testCase.expectedExtras;
  EXPECT_TRUE(IsEqual(control, metricsEvents[0]));
}

static const PinningAPITestCase apiValidationTestCases[] = {
    {.result = Win11PinToTaskBarResultStatus::NotSupported,
     .expectedExtras = {.fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_SUCCESS),
                        .notSupported = Some(true),
                        .numberOfAttempts = Some(1),
                        .result =
                            Some(telemetry::pinning::TELEMETRY_NOT_SUPPORTED)}},
    {.result = Win11PinToTaskBarResultStatus::Success,
     .expectedExtras = {.fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_ERROR),
                        .numberOfAttempts = Some(1),
                        .result = Some(telemetry::pinning::TELEMETRY_SUCCESS),
                        .success = Some(true)}},
    {.result = Win11PinToTaskBarResultStatus::AlreadyPinned,
     .expectedExtras = {.alreadyPinned = Some(true),
                        .fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_ERROR),
                        .numberOfAttempts = Some(1),
                        .result = Some(
                            telemetry::pinning::TELEMETRY_ALREADY_PINNED)}},
    {.result = Win11PinToTaskBarResultStatus::NotCurrentlyAllowed,
     .expectedExtras =
         {.fallbackPinningSuccess = Some(telemetry::pinning::TELEMETRY_SUCCESS),
          .notCurrentlyAllowed = Some(true),
          .numberOfAttempts = Some(1),
          .result = Some(telemetry::pinning::TELEMETRY_NOT_CURRENTLY_ALLOWED)}},
    {.result = Win11PinToTaskBarResultStatus::Failed,
     .expectedExtras = {.error = Some(true),
                        .fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_SUCCESS),
                        .numberOfAttempts = Some(1),
                        .result = Some(telemetry::pinning::TELEMETRY_ERROR)}},
    {.result = Win11PinToTaskBarResultStatus::ErrorLimitedAccessFeatures,
     .expectedExtras =
         {.errorLimitedAccessFeatures = Some(true),
          .fallbackPinningSuccess = Some(telemetry::pinning::TELEMETRY_SUCCESS),
          .numberOfAttempts = Some(1),
          .result = Some(
              telemetry::pinning::TELEMETRY_ERROR_LIMITED_ACCESS_FEATURES)}},
    {.result = Win11PinToTaskBarResultStatus::LimitedAccessFeaturesLocked,
     .expectedExtras =
         {.fallbackPinningSuccess = Some(telemetry::pinning::TELEMETRY_SUCCESS),
          .limitedAccessFeaturesLocked = Some(true),
          .numberOfAttempts = Some(1),
          .result = Some(
              telemetry::pinning::TELEMETRY_LIMITED_ACCESS_FEATURES_LOCKED)}},
};

INSTANTIATE_TEST_SUITE_P(WindowsPinningAPITelemetryTests,
                         WindowsPinningAPIFixture,
                         ::testing::ValuesIn(apiValidationTestCases));

class WindowsPinningAPIFallbackFixture
    : public FOGFixtureWithParam<PinningAPITestCase> {};

TEST_P(WindowsPinningAPIFallbackFixture,
       WindowsPinningTestFallbackFailsEventPrivateBrowsingOn) {
  auto testCase = GetParam();
  const bool checkOnly = false;
  const bool privateBrowsing = true;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(checkOnly, PinningShortcutOperation::Succeed, testCase.result,
              PinningFallbackOperation::Fail, privateBrowsing);

  const bool fallbackSucceeded = false;
  auto extras = telemetry::pinning::CreateEventExtra(
      {S_OK, testCase.result, 1}, privateBrowsing, fallbackSucceeded);

  EXPECT_TRUE(IsEqual(testCase.expectedExtras, extras));

  auto metricsEvents = pinned_to_taskbar.TestGetValue().unwrap().ref();
  auto control = testCase.expectedExtras;
  EXPECT_TRUE(IsEqual(control, metricsEvents[0]));
}

TEST_P(WindowsPinningAPIFallbackFixture,
       WindowsPinningTestFallbackFailsEventPrivateBrowsingOff) {
  auto testCase = GetParam();
  const bool checkOnly = false;
  const bool privateBrowsing = false;
  testCase.expectedExtras.privateBrowsing = Some(privateBrowsing);

  TestPinning(checkOnly, PinningShortcutOperation::Succeed, testCase.result,
              PinningFallbackOperation::Fail, privateBrowsing);

  const bool fallbackSucceeded = false;
  auto extras = telemetry::pinning::CreateEventExtra(
      {S_OK, testCase.result, 1}, privateBrowsing, fallbackSucceeded);

  EXPECT_TRUE(IsEqual(testCase.expectedExtras, extras));

  auto metricsEvents = pinned_to_taskbar.TestGetValue().unwrap().ref();
  auto control = testCase.expectedExtras;
  EXPECT_TRUE(IsEqual(control, metricsEvents[0]));
}

static const PinningAPITestCase fallbackFailedTestCases[] = {
    {.result = Win11PinToTaskBarResultStatus::NotSupported,
     .expectedExtras = {.fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_ERROR),
                        .notSupported = Some(true),
                        .numberOfAttempts = Some(1),
                        .result =
                            Some(telemetry::pinning::TELEMETRY_NOT_SUPPORTED)}},
    {.result = Win11PinToTaskBarResultStatus::Success,
     .expectedExtras = {.fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_ERROR),
                        .numberOfAttempts = Some(1),
                        .result = Some(telemetry::pinning::TELEMETRY_SUCCESS),
                        .success = Some(true)}},
    {.result = Win11PinToTaskBarResultStatus::AlreadyPinned,
     .expectedExtras = {.alreadyPinned = Some(true),
                        .fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_ERROR),
                        .numberOfAttempts = Some(1),
                        .result = Some(
                            telemetry::pinning::TELEMETRY_ALREADY_PINNED)}},
    {.result = Win11PinToTaskBarResultStatus::NotCurrentlyAllowed,
     .expectedExtras =
         {.fallbackPinningSuccess = Some(telemetry::pinning::TELEMETRY_ERROR),
          .notCurrentlyAllowed = Some(true),
          .numberOfAttempts = Some(1),
          .result = Some(telemetry::pinning::TELEMETRY_NOT_CURRENTLY_ALLOWED)}},
    {.result = Win11PinToTaskBarResultStatus::Failed,
     .expectedExtras = {.error = Some(true),
                        .fallbackPinningSuccess =
                            Some(telemetry::pinning::TELEMETRY_ERROR),
                        .numberOfAttempts = Some(1),
                        .result = Some(telemetry::pinning::TELEMETRY_ERROR)}},
    {.result = Win11PinToTaskBarResultStatus::ErrorLimitedAccessFeatures,
     .expectedExtras =
         {.errorLimitedAccessFeatures = Some(true),
          .fallbackPinningSuccess = Some(telemetry::pinning::TELEMETRY_ERROR),
          .numberOfAttempts = Some(1),
          .result = Some(
              telemetry::pinning::TELEMETRY_ERROR_LIMITED_ACCESS_FEATURES)}},
    {.result = Win11PinToTaskBarResultStatus::LimitedAccessFeaturesLocked,
     .expectedExtras =
         {.fallbackPinningSuccess = Some(telemetry::pinning::TELEMETRY_ERROR),
          .limitedAccessFeaturesLocked = Some(true),
          .numberOfAttempts = Some(1),
          .result = Some(
              telemetry::pinning::TELEMETRY_LIMITED_ACCESS_FEATURES_LOCKED)}},
};

INSTANTIATE_TEST_SUITE_P(WindowsPinningFallbackTelemetryTests,
                         WindowsPinningAPIFallbackFixture,
                         ::testing::ValuesIn(fallbackFailedTestCases));

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "AudioFocusManager.h"
#include "MediaControlService.h"
#include "mozilla/Preferences.h"

using namespace mozilla::dom;

#define FIRST_CONTROLLER_ID 0
#define SECOND_CONTROLLER_ID 1

// This RAII class is used to set the audio focus management pref within a test
// and automatically revert the change when a test ends, in order not to
// interfere other tests unexpectedly.
class AudioFocusManagmentPrefSetterRAII {
 public:
  explicit AudioFocusManagmentPrefSetterRAII(bool aPrefValue) {
    mOriginalValue = mozilla::Preferences::GetBool(mPrefName, false);
    mozilla::Preferences::SetBool(mPrefName, aPrefValue);
  }
  ~AudioFocusManagmentPrefSetterRAII() {
    mozilla::Preferences::SetBool(mPrefName, mOriginalValue);
  }

 private:
  const char* mPrefName = "media.audioFocus.management";
  bool mOriginalValue;
};

TEST(AudioFocusManager, TestRequestAudioFocus)
{
  AudioFocusManager manager(nullptr);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  manager.RequestAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  manager.RevokeAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);
}

TEST(AudioFocusManager, TestAudioFocusNumsWhenEnableAudioFocusManagement)
{
  // When enabling audio focus management, we only allow one controller owing
  // audio focus at a time when the audio competing occurs. As the mechanism of
  // handling the audio competing involves multiple components, we can't test it
  // simply by using the APIs from AudioFocusManager.
  AudioFocusManagmentPrefSetterRAII prefSetter(true);

  RefPtr<MediaControlService> service = MediaControlService::GetService();
  AudioFocusManager& manager = service->GetAudioFocusManager();
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  RefPtr<MediaController> controller1 =
      new MediaController(FIRST_CONTROLLER_ID);
  service->RegisterActiveMediaController(controller1);
  manager.RequestAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  // When controller2 starts, it would win the audio focus from controller1. So
  // there would only one audio focus existing.
  RefPtr<MediaController> controller2 =
      new MediaController(SECOND_CONTROLLER_ID);
  service->RegisterActiveMediaController(controller2);
  manager.RequestAudioFocus(SECOND_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  manager.RevokeAudioFocus(SECOND_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  service->UnregisterActiveMediaController(controller1);
  service->UnregisterActiveMediaController(controller2);
}

TEST(AudioFocusManager, TestAudioFocusNumsWhenDisableAudioFocusManagement)
{
  // When disabling audio focus management, we won't handle the audio competing,
  // so we allow multiple audio focus existing at the same time.
  AudioFocusManagmentPrefSetterRAII prefSetter(false);

  AudioFocusManager manager(nullptr);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  manager.RequestAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  manager.RequestAudioFocus(SECOND_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 2);

  manager.RevokeAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  manager.RevokeAudioFocus(SECOND_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);
}

TEST(AudioFocusManager, TestRequestAudioFocusRepeatedly)
{
  AudioFocusManager manager(nullptr);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  manager.RequestAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  manager.RequestAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);
}

TEST(AudioFocusManager, TestRevokeAudioFocusRepeatedly)
{
  AudioFocusManager manager(nullptr);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  manager.RequestAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  manager.RevokeAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  manager.RevokeAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);
}

TEST(AudioFocusManager, TestRevokeAudioFocusWithoutRequestAudioFocus)
{
  AudioFocusManager manager(nullptr);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  manager.RevokeAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);
}

TEST(AudioFocusManager,
     TestRevokeAudioFocusForControllerWithoutOwningAudioFocus)
{
  AudioFocusManager manager(nullptr);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 0);

  manager.RequestAudioFocus(FIRST_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);

  manager.RevokeAudioFocus(SECOND_CONTROLLER_ID);
  ASSERT_TRUE(manager.GetAudioFocusNums() == 1);
}

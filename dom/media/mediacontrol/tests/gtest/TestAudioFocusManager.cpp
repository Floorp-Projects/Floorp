/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "AudioFocusManager.h"
#include "MediaControlService.h"

using namespace mozilla::dom;

#define FIRST_CONTROLLER_ID 0
#define SECOND_CONTROLLER_ID 1

TEST(AudioFocusManager, TestMultipleAudioFocusNums)
{
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

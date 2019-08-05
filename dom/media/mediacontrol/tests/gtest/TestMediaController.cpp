/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaControlService.h"
#include "MediaController.h"

using namespace mozilla::dom;

#define CONTROLLER_ID 0

TEST(MediaController, DefaultValueCheck)
{
  RefPtr<MediaController> controller = new TabMediaController(CONTROLLER_ID);
  ASSERT_TRUE(controller->ControlledMediaNum() == 0);
  ASSERT_TRUE(controller->Id() == CONTROLLER_ID);
  ASSERT_TRUE(!controller->IsPlaying());
  ASSERT_TRUE(!controller->IsAudible());
}

TEST(MediaController, NotifyMediaActiveChanged)
{
  RefPtr<MediaController> controller = new TabMediaController(CONTROLLER_ID);
  ASSERT_TRUE(controller->ControlledMediaNum() == 0);

  controller->NotifyMediaActiveChanged(true);
  ASSERT_TRUE(controller->ControlledMediaNum() == 1);

  controller->NotifyMediaActiveChanged(true);
  ASSERT_TRUE(controller->ControlledMediaNum() == 2);

  controller->NotifyMediaActiveChanged(false);
  ASSERT_TRUE(controller->ControlledMediaNum() == 1);

  controller->NotifyMediaActiveChanged(false);
  ASSERT_TRUE(controller->ControlledMediaNum() == 0);
}

TEST(MediaController, ActiveAndDeactiveController)
{
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  ASSERT_TRUE(service->GetControllersNum() == 0);

  RefPtr<MediaController> controller1 =
      new TabMediaController(FIRST_CONTROLLER_ID);

  controller1->NotifyMediaActiveChanged(true);
  ASSERT_TRUE(service->GetControllersNum() == 1);

  controller1->NotifyMediaActiveChanged(false);
  ASSERT_TRUE(service->GetControllersNum() == 0);
}

TEST(MediaController, AudibleChanged)
{
  RefPtr<MediaController> controller = new TabMediaController(CONTROLLER_ID);
  controller->Play();
  ASSERT_TRUE(!controller->IsAudible());

  controller->NotifyMediaAudibleChanged(true);
  ASSERT_TRUE(controller->IsAudible());

  controller->NotifyMediaAudibleChanged(false);
  ASSERT_TRUE(!controller->IsAudible());
}

TEST(MediaController, AlwaysInaudibleIfControllerIsNotPlaying)
{
  RefPtr<MediaController> controller = new TabMediaController(CONTROLLER_ID);
  ASSERT_TRUE(!controller->IsAudible());

  controller->NotifyMediaAudibleChanged(true);
  ASSERT_TRUE(!controller->IsAudible());

  controller->Play();
  ASSERT_TRUE(controller->IsAudible());

  controller->Pause();
  ASSERT_TRUE(!controller->IsAudible());

  controller->Play();
  ASSERT_TRUE(controller->IsAudible());

  controller->Stop();
  ASSERT_TRUE(!controller->IsAudible());
}

TEST(MediaController, playingStateChanged)
{
  RefPtr<MediaController> controller = new TabMediaController(CONTROLLER_ID);
  ASSERT_TRUE(!controller->IsPlaying());

  controller->Play();
  ASSERT_TRUE(controller->IsPlaying());

  controller->Play();
  controller->Pause();
  ASSERT_TRUE(!controller->IsPlaying());

  controller->Play();
  controller->Stop();
  ASSERT_TRUE(!controller->IsPlaying());
}

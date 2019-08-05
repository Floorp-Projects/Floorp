/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaControlService.h"
#include "MediaController.h"

using namespace mozilla::dom;

#define FIRST_CONTROLLER_ID 0
#define SECOND_CONTROLLER_ID 1

TEST(MediaControlService, TestAddOrRemoveControllers)
{
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  ASSERT_TRUE(service->GetControllersNum() == 0);

  RefPtr<MediaController> controller1 =
      new TabMediaController(FIRST_CONTROLLER_ID);
  RefPtr<MediaController> controller2 =
      new TabMediaController(SECOND_CONTROLLER_ID);

  service->AddMediaController(controller1);
  ASSERT_TRUE(service->GetControllersNum() == 1);

  service->AddMediaController(controller2);
  ASSERT_TRUE(service->GetControllersNum() == 2);

  service->RemoveMediaController(controller1);
  ASSERT_TRUE(service->GetControllersNum() == 1);

  service->RemoveMediaController(controller2);
  ASSERT_TRUE(service->GetControllersNum() == 0);
}

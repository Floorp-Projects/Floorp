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
  ASSERT_TRUE(service->GetActiveControllersNum() == 0);

  RefPtr<MediaController> controller1 =
      new MediaController(FIRST_CONTROLLER_ID);
  RefPtr<MediaController> controller2 =
      new MediaController(SECOND_CONTROLLER_ID);

  service->RegisterActiveMediaController(controller1);
  ASSERT_TRUE(service->GetActiveControllersNum() == 1);

  service->RegisterActiveMediaController(controller2);
  ASSERT_TRUE(service->GetActiveControllersNum() == 2);

  service->UnregisterActiveMediaController(controller1);
  ASSERT_TRUE(service->GetActiveControllersNum() == 1);

  service->UnregisterActiveMediaController(controller2);
  ASSERT_TRUE(service->GetActiveControllersNum() == 0);
}

TEST(MediaControlService, TestMainController)
{
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  ASSERT_TRUE(service->GetActiveControllersNum() == 0);

  RefPtr<MediaController> controller1 =
      new MediaController(FIRST_CONTROLLER_ID);
  service->RegisterActiveMediaController(controller1);

  RefPtr<MediaController> mainController = service->GetMainController();
  ASSERT_TRUE(mainController->Id() == FIRST_CONTROLLER_ID);

  RefPtr<MediaController> controller2 =
      new MediaController(SECOND_CONTROLLER_ID);
  service->RegisterActiveMediaController(controller2);

  mainController = service->GetMainController();
  ASSERT_TRUE(mainController->Id() == SECOND_CONTROLLER_ID);

  service->UnregisterActiveMediaController(controller2);
  mainController = service->GetMainController();
  ASSERT_TRUE(mainController->Id() == FIRST_CONTROLLER_ID);

  service->UnregisterActiveMediaController(controller1);
  mainController = service->GetMainController();
  ASSERT_TRUE(service->GetActiveControllersNum() == 0);
  ASSERT_TRUE(!mainController);
}

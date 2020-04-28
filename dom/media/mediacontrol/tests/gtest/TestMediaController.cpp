/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaControlService.h"
#include "MediaController.h"
#include "mozilla/dom/MediaSessionBinding.h"

using namespace mozilla::dom;

#define CONTROLLER_ID 0

TEST(MediaController, DefaultValueCheck)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);
  ASSERT_TRUE(controller->ControlledMediaNum() == 0);
  ASSERT_TRUE(controller->Id() == CONTROLLER_ID);
  ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::None);
  ASSERT_TRUE(!controller->IsAudible());
}

TEST(MediaController, NotifyMediaPlaybackChanged)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);
  ASSERT_TRUE(controller->ControlledMediaNum() == 0);

  controller->NotifyMediaPlaybackChanged(MediaPlaybackState::eStarted);
  ASSERT_TRUE(controller->ControlledMediaNum() == 1);

  controller->NotifyMediaPlaybackChanged(MediaPlaybackState::eStarted);
  ASSERT_TRUE(controller->ControlledMediaNum() == 2);

  controller->NotifyMediaPlaybackChanged(MediaPlaybackState::eStopped);
  ASSERT_TRUE(controller->ControlledMediaNum() == 1);

  controller->NotifyMediaPlaybackChanged(MediaPlaybackState::eStopped);
  ASSERT_TRUE(controller->ControlledMediaNum() == 0);
}

TEST(MediaController, ActiveAndDeactiveController)
{
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  ASSERT_TRUE(service->GetActiveControllersNum() == 0);

  RefPtr<MediaController> controller1 =
      new MediaController(FIRST_CONTROLLER_ID);

  controller1->NotifyMediaPlaybackChanged(MediaPlaybackState::eStarted);
  ASSERT_TRUE(service->GetActiveControllersNum() == 1);

  controller1->NotifyMediaPlaybackChanged(MediaPlaybackState::eStopped);
  ASSERT_TRUE(service->GetActiveControllersNum() == 0);
}

class FakeControlledMedia final {
 public:
  explicit FakeControlledMedia(MediaController* aController)
      : mController(aController) {
    mController->NotifyMediaPlaybackChanged(MediaPlaybackState::eStarted);
  }

  void SetPlaying(MediaPlaybackState aState) {
    if (mPlaybackState == aState) {
      return;
    }
    mController->NotifyMediaPlaybackChanged(aState);
    mPlaybackState = aState;
  }

  void SetAudible(MediaAudibleState aState) {
    if (mAudibleState == aState) {
      return;
    }
    mController->NotifyMediaAudibleChanged(aState);
    mAudibleState = aState;
  }

  ~FakeControlledMedia() {
    if (mPlaybackState == MediaPlaybackState::ePlayed) {
      mController->NotifyMediaPlaybackChanged(MediaPlaybackState::ePaused);
    }
    mController->NotifyMediaPlaybackChanged(MediaPlaybackState::eStopped);
  }

 private:
  MediaPlaybackState mPlaybackState = MediaPlaybackState::eStopped;
  MediaAudibleState mAudibleState = MediaAudibleState::eInaudible;
  RefPtr<MediaController> mController;
};

TEST(MediaController, AudibleChanged)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);

  FakeControlledMedia fakeMedia(controller);
  fakeMedia.SetPlaying(MediaPlaybackState::ePlayed);
  ASSERT_TRUE(!controller->IsAudible());

  fakeMedia.SetAudible(MediaAudibleState::eAudible);
  ASSERT_TRUE(controller->IsAudible());

  fakeMedia.SetAudible(MediaAudibleState::eInaudible);
  ASSERT_TRUE(!controller->IsAudible());
}

TEST(MediaController, PlayingStateChangeViaControlledMedia)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);

  // In order to check playing state after FakeControlledMedia destroyed.
  {
    FakeControlledMedia foo(controller);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::None);

    foo.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Playing);

    foo.SetPlaying(MediaPlaybackState::ePaused);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Paused);

    foo.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Playing);
  }

  // FakeControlledMedia has been destroyed, no playing media exists.
  ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Paused);
}

TEST(MediaController, ControllerShouldRemainPlayingIfAnyPlayingMediaExists)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);

  {
    FakeControlledMedia foo(controller);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::None);

    foo.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Playing);

    // foo is playing, so controller is in `playing` state.
    FakeControlledMedia bar(controller);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Playing);

    bar.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Playing);

    // Although we paused bar, but foo is still playing, so the controller would
    // still be in `playing`.
    bar.SetPlaying(MediaPlaybackState::ePaused);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Playing);

    foo.SetPlaying(MediaPlaybackState::ePaused);
    ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Paused);
  }

  // both foo and bar have been destroyed, no playing media exists.
  ASSERT_TRUE(controller->GetState() == MediaSessionPlaybackState::Paused);
}

TEST(MediaController, PictureInPictureMode)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);
  ASSERT_TRUE(!controller->IsInPictureInPictureMode());

  controller->SetIsInPictureInPictureMode(true);
  ASSERT_TRUE(controller->IsInPictureInPictureMode());

  controller->SetIsInPictureInPictureMode(false);
  ASSERT_TRUE(!controller->IsInPictureInPictureMode());
}

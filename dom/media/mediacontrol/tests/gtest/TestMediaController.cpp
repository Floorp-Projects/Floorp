/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaControlService.h"
#include "MediaController.h"
#include "mozilla/dom/MediaSessionBinding.h"

using namespace mozilla::dom;

#define CONTROLLER_ID 0
#define FAKE_CONTEXT_ID 0

#define FIRST_CONTROLLER_ID 0

TEST(MediaController, DefaultValueCheck)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);
  ASSERT_TRUE(!controller->IsAnyMediaBeingControlled());
  ASSERT_TRUE(controller->Id() == CONTROLLER_ID);
  ASSERT_TRUE(controller->PlaybackState() == MediaSessionPlaybackState::None);
  ASSERT_TRUE(!controller->IsAudible());
}

TEST(MediaController, IsAnyMediaBeingControlled)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);
  ASSERT_TRUE(!controller->IsAnyMediaBeingControlled());

  controller->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID,
                                         MediaPlaybackState::eStarted);
  ASSERT_TRUE(controller->IsAnyMediaBeingControlled());

  controller->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID,
                                         MediaPlaybackState::eStarted);
  ASSERT_TRUE(controller->IsAnyMediaBeingControlled());

  controller->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID,
                                         MediaPlaybackState::eStopped);
  ASSERT_TRUE(controller->IsAnyMediaBeingControlled());

  controller->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID,
                                         MediaPlaybackState::eStopped);
  ASSERT_TRUE(!controller->IsAnyMediaBeingControlled());
}

class FakeControlledMedia final {
 public:
  explicit FakeControlledMedia(MediaController* aController)
      : mController(aController) {
    mController->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID,
                                            MediaPlaybackState::eStarted);
  }

  void SetPlaying(MediaPlaybackState aState) {
    if (mPlaybackState == aState) {
      return;
    }
    mController->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID, aState);
    mPlaybackState = aState;
  }

  void SetAudible(MediaAudibleState aState) {
    if (mAudibleState == aState) {
      return;
    }
    mController->NotifyMediaAudibleChanged(FAKE_CONTEXT_ID, aState);
    mAudibleState = aState;
  }

  ~FakeControlledMedia() {
    if (mPlaybackState == MediaPlaybackState::ePlayed) {
      mController->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID,
                                              MediaPlaybackState::ePaused);
    }
    mController->NotifyMediaPlaybackChanged(FAKE_CONTEXT_ID,
                                            MediaPlaybackState::eStopped);
  }

 private:
  MediaPlaybackState mPlaybackState = MediaPlaybackState::eStopped;
  MediaAudibleState mAudibleState = MediaAudibleState::eInaudible;
  RefPtr<MediaController> mController;
};

TEST(MediaController, ActiveAndDeactiveController)
{
  RefPtr<MediaControlService> service = MediaControlService::GetService();
  ASSERT_TRUE(service->GetActiveControllersNum() == 0);

  RefPtr<MediaController> controller = new MediaController(FIRST_CONTROLLER_ID);

  // In order to check active control number after FakeControlledMedia
  // destroyed.
  {
    FakeControlledMedia fakeMedia(controller);
    fakeMedia.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(service->GetActiveControllersNum() == 1);

    fakeMedia.SetAudible(MediaAudibleState::eAudible);
    ASSERT_TRUE(service->GetActiveControllersNum() == 1);

    fakeMedia.SetAudible(MediaAudibleState::eInaudible);
    ASSERT_TRUE(service->GetActiveControllersNum() == 1);
  }

  ASSERT_TRUE(service->GetActiveControllersNum() == 0);
}

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
    ASSERT_TRUE(controller->PlaybackState() == MediaSessionPlaybackState::None);

    foo.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Playing);

    foo.SetPlaying(MediaPlaybackState::ePaused);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Paused);

    foo.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Playing);
  }

  // FakeControlledMedia has been destroyed, no playing media exists.
  ASSERT_TRUE(controller->PlaybackState() == MediaSessionPlaybackState::Paused);
}

TEST(MediaController, ControllerShouldRemainPlayingIfAnyPlayingMediaExists)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);

  {
    FakeControlledMedia foo(controller);
    ASSERT_TRUE(controller->PlaybackState() == MediaSessionPlaybackState::None);

    foo.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Playing);

    // foo is playing, so controller is in `playing` state.
    FakeControlledMedia bar(controller);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Playing);

    bar.SetPlaying(MediaPlaybackState::ePlayed);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Playing);

    // Although we paused bar, but foo is still playing, so the controller would
    // still be in `playing`.
    bar.SetPlaying(MediaPlaybackState::ePaused);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Playing);

    foo.SetPlaying(MediaPlaybackState::ePaused);
    ASSERT_TRUE(controller->PlaybackState() ==
                MediaSessionPlaybackState::Paused);
  }

  // both foo and bar have been destroyed, no playing media exists.
  ASSERT_TRUE(controller->PlaybackState() == MediaSessionPlaybackState::Paused);
}

TEST(MediaController, PictureInPictureModeOrFullscreen)
{
  RefPtr<MediaController> controller = new MediaController(CONTROLLER_ID);
  ASSERT_TRUE(!controller->IsBeingUsedInPIPModeOrFullscreen());

  controller->SetIsInPictureInPictureMode(FAKE_CONTEXT_ID, true);
  ASSERT_TRUE(controller->IsBeingUsedInPIPModeOrFullscreen());

  controller->SetIsInPictureInPictureMode(FAKE_CONTEXT_ID, false);
  ASSERT_TRUE(!controller->IsBeingUsedInPIPModeOrFullscreen());

  controller->NotifyMediaFullScreenState(FAKE_CONTEXT_ID, true);
  ASSERT_TRUE(controller->IsBeingUsedInPIPModeOrFullscreen());

  controller->NotifyMediaFullScreenState(FAKE_CONTEXT_ID, false);
  ASSERT_TRUE(!controller->IsBeingUsedInPIPModeOrFullscreen());
}

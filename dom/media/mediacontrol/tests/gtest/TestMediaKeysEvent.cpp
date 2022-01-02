/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaController.h"
#include "MediaControlKeySource.h"

using namespace mozilla::dom;

class MediaControlKeySourceTestImpl : public MediaControlKeySource {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaControlKeySourceTestImpl, override)
  bool Open() override { return true; }
  bool IsOpened() const override { return true; }
  void SetSupportedMediaKeys(const MediaKeysArray& aSupportedKeys) override {}

 private:
  ~MediaControlKeySourceTestImpl() = default;
};

TEST(MediaControlKey, TestAddOrRemoveListener)
{
  RefPtr<MediaControlKeySource> source = new MediaControlKeySourceTestImpl();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaControlKeyListener> listener = new MediaControlKeyHandler();

  source->AddListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 1);

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

TEST(MediaControlKey, SetSourcePlaybackState)
{
  RefPtr<MediaControlKeySource> source = new MediaControlKeySourceTestImpl();
  ASSERT_TRUE(source->GetPlaybackState() == MediaSessionPlaybackState::None);

  source->SetPlaybackState(MediaSessionPlaybackState::Playing);
  ASSERT_TRUE(source->GetPlaybackState() == MediaSessionPlaybackState::Playing);

  source->SetPlaybackState(MediaSessionPlaybackState::Paused);
  ASSERT_TRUE(source->GetPlaybackState() == MediaSessionPlaybackState::Paused);

  source->SetPlaybackState(MediaSessionPlaybackState::None);
  ASSERT_TRUE(source->GetPlaybackState() == MediaSessionPlaybackState::None);
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#import <MediaPlayer/MediaPlayer.h>

#include "gtest/gtest.h"
#include "MediaHardwareKeysEventSourceMacMediaCenter.h"
#include "MediaKeyListenerTest.h"
#include "nsCocoaUtils.h"
#include "prinrval.h"
#include "prthread.h"

using namespace mozilla::dom;
using namespace mozilla::widget;

NS_ASSUME_NONNULL_BEGIN

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPlayPauseEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());
  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

  MediaCenterEventHandler playPauseHandler = source->CreatePlayPauseHandler();
  playPauseHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePaused);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Playpause));

  listener->Clear();  // Reset stored media key

  playPauseHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Playpause));
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPlayEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());
  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

  MediaCenterEventHandler playHandler = source->CreatePlayHandler();

  center.playbackState = MPNowPlayingPlaybackStatePaused;

  playHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Play));
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPauseEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());
  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePlaying);

  MediaCenterEventHandler pauseHandler = source->CreatePauseHandler();

  pauseHandler(nil);

  ASSERT_TRUE(center.playbackState == MPNowPlayingPlaybackStatePaused);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Pause));
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestMediaCenterPrevNextEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  MediaCenterEventHandler nextHandler = source->CreateNextTrackHandler();

  nextHandler(nil);

  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Nexttrack));

  MediaCenterEventHandler previousHandler =
      source->CreatePreviousTrackHandler();

  previousHandler(nil);

  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Previoustrack));
}

TEST(MediaHardwareKeysEventSourceMacMediaCenter, TestSetMetadata)
{
  RefPtr<MediaHardwareKeysEventSourceMacMediaCenter> source =
      new MediaHardwareKeysEventSourceMacMediaCenter();

  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();

  source->AddListener(listener.get());

  ASSERT_TRUE(source->Open());

  MediaMetadataBase metadata;
  metadata.mTitle = u"MediaPlayback";
  metadata.mArtist = u"Firefox";
  metadata.mAlbum = u"Mozilla";
  source->SetMediaMetadata(metadata);

  // The update procedure of nowPlayingInfo is async, so wait for a second
  // before checking the result.
  PR_Sleep(PR_SecondsToInterval(1));
  MPNowPlayingInfoCenter* center = [MPNowPlayingInfoCenter defaultCenter];
  ASSERT_TRUE([center.nowPlayingInfo[MPMediaItemPropertyTitle]
      isEqualToString:@"MediaPlayback"]);
  ASSERT_TRUE([center.nowPlayingInfo[MPMediaItemPropertyArtist]
      isEqualToString:@"Firefox"]);
  ASSERT_TRUE([center.nowPlayingInfo[MPMediaItemPropertyAlbumTitle]
      isEqualToString:@"Mozilla"]);

  source->Close();
  PR_Sleep(PR_SecondsToInterval(1));
  ASSERT_TRUE(center.nowPlayingInfo == nil);
}

NS_ASSUME_NONNULL_END

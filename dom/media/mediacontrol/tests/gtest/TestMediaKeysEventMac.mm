/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <AppKit/AppKit.h>
#import <AppKit/NSEvent.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/hidsystem/ev_keymap.h>

#include "gtest/gtest.h"
#include "MediaHardwareKeysEventSourceMac.h"
#include "MediaKeyListenerTest.h"
#include "mozilla/Maybe.h"

using namespace mozilla::dom;
using namespace mozilla::widget;

static const int kSystemDefinedEventMediaKeysSubtype = 8;

static void SendFakeEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource,
                          int aKeyData) {
  NSEvent* event =
      [NSEvent otherEventWithType:NSEventTypeSystemDefined
                         location:NSZeroPoint
                    modifierFlags:0
                        timestamp:0
                     windowNumber:0
                          context:nil
                          subtype:kSystemDefinedEventMediaKeysSubtype
                            data1:aKeyData
                            data2:0];
  aSource->EventTapCallback(nullptr, static_cast<CGEventType>(0),
                            [event CGEvent], aSource.get());
}

static void NotifyFakeNonMediaKey(
    RefPtr<MediaHardwareKeysEventSourceMac>& aSource, bool aIsKeyPressed) {
  int keyData = 0 | ((aIsKeyPressed ? 0xA : 0xB) << 8);
  SendFakeEvent(aSource, keyData);
}

static void NotifyFakeMediaControlKey(
    RefPtr<MediaHardwareKeysEventSourceMac>& aSource, MediaControlKey aEvent,
    bool aIsKeyPressed) {
  int keyData = 0;
  if (aEvent == MediaControlKey::Playpause) {
    keyData = NX_KEYTYPE_PLAY << 16;
  } else if (aEvent == MediaControlKey::Nexttrack) {
    keyData = NX_KEYTYPE_NEXT << 16;
  } else if (aEvent == MediaControlKey::Previoustrack) {
    keyData = NX_KEYTYPE_PREVIOUS << 16;
  }
  keyData |= ((aIsKeyPressed ? 0xA : 0xB) << 8);
  SendFakeEvent(aSource, keyData);
}

static void NotifyKeyPressedMediaKey(
    RefPtr<MediaHardwareKeysEventSourceMac>& aSource, MediaControlKey aEvent) {
  NotifyFakeMediaControlKey(aSource, aEvent, true /* key pressed */);
}

static void NotifyKeyReleasedMediaKeysEvent(
    RefPtr<MediaHardwareKeysEventSourceMac>& aSource, MediaControlKey aEvent) {
  NotifyFakeMediaControlKey(aSource, aEvent, false /* key released */);
}

static void NotifyKeyPressedNonMediaKeysEvents(
    RefPtr<MediaHardwareKeysEventSourceMac>& aSource) {
  NotifyFakeNonMediaKey(aSource, true /* key pressed */);
}

static void NotifyKeyReleasedNonMediaKeysEvents(
    RefPtr<MediaHardwareKeysEventSourceMac>& aSource) {
  NotifyFakeNonMediaKey(aSource, false /* key released */);
}

TEST(MediaHardwareKeysEventSourceMac, TestKeyPressedMediaKeysEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMac> source =
      new MediaHardwareKeysEventSourceMac();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();
  source->AddListener(listener.get());
  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyPressedMediaKey(source, MediaControlKey::Playpause);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Playpause));

  NotifyKeyPressedMediaKey(source, MediaControlKey::Nexttrack);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Nexttrack));

  NotifyKeyPressedMediaKey(source, MediaControlKey::Previoustrack);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKey::Previoustrack));

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

TEST(MediaHardwareKeysEventSourceMac, TestKeyReleasedMediaKeysEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMac> source =
      new MediaHardwareKeysEventSourceMac();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();
  source->AddListener(listener.get());
  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKey::Playpause);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKey::Nexttrack);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKey::Previoustrack);
  ASSERT_TRUE(!listener->IsReceivedResult());

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

TEST(MediaHardwareKeysEventSourceMac, TestNonMediaKeysEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMac> source =
      new MediaHardwareKeysEventSourceMac();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaKeyListenerTest> listener = new MediaKeyListenerTest();
  source->AddListener(listener.get());
  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyPressedNonMediaKeysEvents(source);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedNonMediaKeysEvents(source);
  ASSERT_TRUE(!listener->IsReceivedResult());

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

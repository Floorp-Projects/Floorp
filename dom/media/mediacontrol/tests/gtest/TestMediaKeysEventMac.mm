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

using namespace mozilla::dom;

static const int kSystemDefinedEventMediaKeysSubtype = 8;

class MediaHardwareKeysEventListenerTest : public MediaHardwareKeysEventListener {
 public:
  void OnKeyPressed(MediaControlKeysEvent aKeyEvent) override { mReceivedEvent = aKeyEvent; }
  MediaControlKeysEvent GetResult() const { return mReceivedEvent; }

 private:
  MediaControlKeysEvent mReceivedEvent = MediaControlKeysEvent::eNone;
};

static void NotifyFakeMediaKeysEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource,
                                     MediaControlKeysEvent aEvent, bool aIsKeyPressed) {
  int keyData = 0;
  if (aEvent == MediaControlKeysEvent::ePlayPause) {
    keyData = NX_KEYTYPE_PLAY << 16;
  } else if (aEvent == MediaControlKeysEvent::eNext) {
    keyData = NX_KEYTYPE_NEXT << 16;
  } else if (aEvent == MediaControlKeysEvent::ePrev) {
    keyData = NX_KEYTYPE_PREVIOUS << 16;
  }
  keyData |= ((aIsKeyPressed ? 0xA : 0xB) << 8);

  NSEvent* event = [NSEvent otherEventWithType:NSSystemDefined
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                       subtype:kSystemDefinedEventMediaKeysSubtype
                                         data1:keyData
                                         data2:0];
  aSource->EventTapCallback(nullptr, static_cast<CGEventType>(0), [event CGEvent], aSource.get());
}

static void NotifyKeyPressedMediaKeysEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource,
                                           MediaControlKeysEvent aEvent) {
  NotifyFakeMediaKeysEvent(aSource, aEvent, true);
}

static void NotifyKeyReleasedMediaKeysEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource,
                                            MediaControlKeysEvent aEvent) {
  NotifyFakeMediaKeysEvent(aSource, aEvent, false);
}

static void NotifyKeyPressedNonMediaKeysEvents(RefPtr<MediaHardwareKeysEventSourceMac>& aSource) {
  NotifyFakeMediaKeysEvent(aSource, MediaControlKeysEvent::eNone, true);
}

static void NotifyKeyReleasedNonMediaKeysEvents(RefPtr<MediaHardwareKeysEventSourceMac>& aSource) {
  NotifyFakeMediaKeysEvent(aSource, MediaControlKeysEvent::eNone, false);
}

TEST(MediaHardwareKeysEventSourceMac, TestKeyPressedMediaKeysEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMac> source = new MediaHardwareKeysEventSourceMac();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaHardwareKeysEventListenerTest> listener = new MediaHardwareKeysEventListenerTest();
  source->AddListener(listener.get());
  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  NotifyKeyPressedMediaKeysEvent(source, MediaControlKeysEvent::ePlayPause);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::ePlayPause);

  NotifyKeyPressedMediaKeysEvent(source, MediaControlKeysEvent::eNext);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNext);

  NotifyKeyPressedMediaKeysEvent(source, MediaControlKeysEvent::ePrev);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::ePrev);

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

TEST(MediaHardwareKeysEventSourceMac, TestKeyReleasedMediaKeysEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMac> source = new MediaHardwareKeysEventSourceMac();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaHardwareKeysEventListenerTest> listener = new MediaHardwareKeysEventListenerTest();
  source->AddListener(listener.get());
  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKeysEvent::ePlayPause);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKeysEvent::eNext);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKeysEvent::ePrev);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

TEST(MediaHardwareKeysEventSourceMac, TestNonMediaKeysEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMac> source = new MediaHardwareKeysEventSourceMac();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaHardwareKeysEventListenerTest> listener = new MediaHardwareKeysEventListenerTest();
  source->AddListener(listener.get());
  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  NotifyKeyPressedNonMediaKeysEvents(source);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  NotifyKeyReleasedNonMediaKeysEvents(source);
  ASSERT_TRUE(listener->GetResult() == MediaControlKeysEvent::eNone);

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

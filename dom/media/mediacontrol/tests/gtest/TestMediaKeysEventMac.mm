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
#include "mozilla/Maybe.h"

using namespace mozilla::dom;
using namespace mozilla::widget;

static const int kSystemDefinedEventMediaKeysSubtype = 8;

class MediaHardwareKeysEventListenerTest : public MediaControlKeysEventListener {
 public:
  NS_DECL_ISUPPORTS

  void OnKeyPressed(MediaControlKeysEvent aKeyEvent) override {
    mReceivedEvent = mozilla::Some(aKeyEvent);
  }
  bool IsResultEqualTo(MediaControlKeysEvent aResult) const {
    if (mReceivedEvent) {
      return *mReceivedEvent == aResult;
    }
    return false;
  }
  bool IsReceivedResult() const { return mReceivedEvent.isSome(); }

 private:
  ~MediaHardwareKeysEventListenerTest() = default;
  mozilla::Maybe<MediaControlKeysEvent> mReceivedEvent;
};

NS_IMPL_ISUPPORTS0(MediaHardwareKeysEventListenerTest)

static void SendFakeEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource, int aKeyData) {
  NSEvent* event = [NSEvent otherEventWithType:NSSystemDefined
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                       subtype:kSystemDefinedEventMediaKeysSubtype
                                         data1:aKeyData
                                         data2:0];
  aSource->EventTapCallback(nullptr, static_cast<CGEventType>(0), [event CGEvent], aSource.get());
}

static void NotifyFakeNonMediaKeysEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource,
                                        bool aIsKeyPressed) {
  int keyData = 0 | ((aIsKeyPressed ? 0xA : 0xB) << 8);
  SendFakeEvent(aSource, keyData);
}

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
  SendFakeEvent(aSource, keyData);
}

static void NotifyKeyPressedMediaKeysEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource,
                                           MediaControlKeysEvent aEvent) {
  NotifyFakeMediaKeysEvent(aSource, aEvent, true /* key pressed */);
}

static void NotifyKeyReleasedMediaKeysEvent(RefPtr<MediaHardwareKeysEventSourceMac>& aSource,
                                            MediaControlKeysEvent aEvent) {
  NotifyFakeMediaKeysEvent(aSource, aEvent, false /* key released */);
}

static void NotifyKeyPressedNonMediaKeysEvents(RefPtr<MediaHardwareKeysEventSourceMac>& aSource) {
  NotifyFakeNonMediaKeysEvent(aSource, true /* key pressed */);
}

static void NotifyKeyReleasedNonMediaKeysEvents(RefPtr<MediaHardwareKeysEventSourceMac>& aSource) {
  NotifyFakeNonMediaKeysEvent(aSource, false /* key released */);
}

TEST(MediaHardwareKeysEventSourceMac, TestKeyPressedMediaKeysEvent)
{
  RefPtr<MediaHardwareKeysEventSourceMac> source = new MediaHardwareKeysEventSourceMac();
  ASSERT_TRUE(source->GetListenersNum() == 0);

  RefPtr<MediaHardwareKeysEventListenerTest> listener = new MediaHardwareKeysEventListenerTest();
  source->AddListener(listener.get());
  ASSERT_TRUE(source->GetListenersNum() == 1);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyPressedMediaKeysEvent(source, MediaControlKeysEvent::ePlayPause);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKeysEvent::ePlayPause));

  NotifyKeyPressedMediaKeysEvent(source, MediaControlKeysEvent::eNext);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKeysEvent::eNext));

  NotifyKeyPressedMediaKeysEvent(source, MediaControlKeysEvent::ePrev);
  ASSERT_TRUE(listener->IsResultEqualTo(MediaControlKeysEvent::ePrev));

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
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKeysEvent::ePlayPause);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKeysEvent::eNext);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedMediaKeysEvent(source, MediaControlKeysEvent::ePrev);
  ASSERT_TRUE(!listener->IsReceivedResult());

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
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyPressedNonMediaKeysEvents(source);
  ASSERT_TRUE(!listener->IsReceivedResult());

  NotifyKeyReleasedNonMediaKeysEvents(source);
  ASSERT_TRUE(!listener->IsReceivedResult());

  source->RemoveListener(listener);
  ASSERT_TRUE(source->GetListenersNum() == 0);
}

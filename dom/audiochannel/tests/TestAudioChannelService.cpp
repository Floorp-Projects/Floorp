/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef XP_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "TestHarness.h"

#include "nsWeakReference.h"
#include "AudioChannelService.h"
#include "AudioChannelAgent.h"

#include "nsThreadUtils.h"

#define TEST_ENSURE_BASE(_test, _msg)       \
  PR_BEGIN_MACRO                            \
    if (!(_test)) {                         \
      fail(_msg);                           \
      return NS_ERROR_FAILURE;              \
    } else {                                \
      passed(_msg);                         \
    }                                       \
  PR_END_MACRO

using namespace mozilla::dom;

void
spin_events_loop_until_false(const bool* const aCondition)
{
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (*aCondition && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

class Agent : public nsIAudioChannelAgentCallback,
              public nsSupportsWeakReference
{
protected:
  virtual ~Agent()
  {
    if (mRegistered) {
      StopPlaying();
    }
  }

public:
  NS_DECL_ISUPPORTS

  explicit Agent(AudioChannel aChannel)
  : mChannel(aChannel)
  , mWaitCallback(false)
  , mRegistered(false)
  , mCanPlay(AUDIO_CHANNEL_STATE_MUTED)
  {
    mAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
  }

  nsresult Init(bool video=false)
  {
    nsresult rv = NS_OK;
    if (video) {
      rv = mAgent->InitWithVideo(nullptr, static_cast<int32_t>(mChannel),
                                 this, true);
    }
    else {
      rv = mAgent->InitWithWeakCallback(nullptr, static_cast<int32_t>(mChannel),
                                        this);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    return mAgent->SetVisibilityState(false);
  }

  nsresult StartPlaying(AudioChannelState *_ret)
  {
    if (mRegistered) {
      StopPlaying();
    }

    nsresult rv = mAgent->StartPlaying((int32_t *)_ret);
    mRegistered = true;
    return rv;
  }

  nsresult StopPlaying()
  {
    mRegistered = false;
    spin_events_loop_until_false(&mWaitCallback);
    return mAgent->StopPlaying();
  }

  nsresult SetVisibilityState(bool visible)
  {
    if (mRegistered) {
      mWaitCallback = true;
    }
    return mAgent->SetVisibilityState(visible);
  }

  NS_IMETHODIMP CanPlayChanged(int32_t canPlay) override
  {
    mCanPlay = static_cast<AudioChannelState>(canPlay);
    mWaitCallback = false;
    return NS_OK;
  }

  NS_IMETHODIMP WindowVolumeChanged() override
  {
    return NS_OK;
  }

  nsresult GetCanPlay(AudioChannelState *_ret, bool aWaitCallback = false)
  {
    if (aWaitCallback) {
      mWaitCallback = true;
    }

    spin_events_loop_until_false(&mWaitCallback);
    *_ret = mCanPlay;
    return NS_OK;
  }

  nsCOMPtr<nsIAudioChannelAgent> mAgent;
  AudioChannel mChannel;
  bool mWaitCallback;
  bool mRegistered;
  AudioChannelState mCanPlay;
};

NS_IMPL_ISUPPORTS(Agent, nsIAudioChannelAgentCallback,
                  nsISupportsWeakReference)

nsresult
TestDoubleStartPlaying()
{
  nsRefPtr<Agent> agent = new Agent(AudioChannel::Normal);

  nsresult rv = agent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  AudioChannelState playable;
  rv = agent->mAgent->StartPlaying((int32_t *)&playable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent->mAgent->StartPlaying((int32_t *)&playable);
  TEST_ENSURE_BASE(NS_FAILED(rv),
    "Test0: StartPlaying calling twice must return error");

  return NS_OK;
}

nsresult
TestOneNormalChannel()
{
  nsRefPtr<Agent> agent = new Agent(AudioChannel::Normal);
  nsresult rv = agent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  AudioChannelState playable;
  rv = agent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test1: A normal channel unvisible agent must be muted");

  rv = agent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test1: A normal channel visible agent must be playable");

  return rv;
}

nsresult
TestTwoNormalChannels()
{
  nsRefPtr<Agent> agent1 = new Agent(AudioChannel::Normal);
  nsresult rv = agent1->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> agent2 = new Agent(AudioChannel::Normal);
  rv = agent2->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  AudioChannelState playable;
  rv = agent1->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test2: A normal channel unvisible agent1 must be muted");

  rv = agent2->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test2: A normal channel unvisible agent2 must be muted");

  rv = agent1->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent2->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test2: A normal channel visible agent1 must be playable");

  rv = agent2->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test2: A normal channel visible agent2 must be playable");

  return rv;
}

nsresult
TestContentChannels()
{
  nsRefPtr<Agent> agent1 = new Agent(AudioChannel::Content);
  nsresult rv = agent1->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> agent2 = new Agent(AudioChannel::Content);
  rv = agent2->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // All content channels in the foreground can be allowed to play
  rv = agent1->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent2->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  AudioChannelState playable;
  rv = agent1->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test3: A content channel visible agent1 must be playable");

  rv = agent2->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test3: A content channel visible agent2 must be playable");

  // Test the transition state of one content channel tried to set non-visible
  // state first when app is going to background.
  rv = agent1->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test3: A content channel unvisible agent1 must be playable from "
    "foreground to background");

  // Test all content channels set non-visible already
  rv = agent2->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent2->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test3: A content channel unvisible agent2 must be playable from "
    "foreground to background");

  // Clear the content channels & mActiveContentChildIDs in AudioChannelService.
  // If agent stop playable in the background, we will reserve it's childID in
  // mActiveContentChildIDs, then it can allow to play next song. So we set agents
  // to foreground first then stopping to play
  rv = agent1->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent2->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent1->StopPlaying();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent2->StopPlaying();
  NS_ENSURE_SUCCESS(rv, rv);

  // Test that content channels can be allow to play when they starts from
  // the background state
  rv = agent1->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent2->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test3: A content channel unvisible agent1 must be playable "
    "from background state");

  rv = agent2->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test3: A content channel unvisible agent2 must be playable "
    "from background state");

  return rv;
}

nsresult
TestFadedState()
{
  nsRefPtr<Agent> normalAgent = new Agent(AudioChannel::Normal);
  nsresult rv = normalAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> contentAgent = new Agent(AudioChannel::Content);
  rv = contentAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> notificationAgent = new Agent(AudioChannel::Notification);
  rv = notificationAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = normalAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = contentAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = notificationAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  AudioChannelState playable;
  rv = normalAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test4: A normal channel visible agent must be playable");

  rv = contentAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test4: A content channel visible agent must be playable");

  rv = notificationAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test4: A notification channel visible agent must be playable");

  rv = contentAgent->GetCanPlay(&playable, true);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_FADED,
    "Test4: A content channel unvisible agent must be faded because of "
    "notification channel is playing");

  rv = contentAgent->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = contentAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_FADED,
    "Test4: A content channel unvisible agent must be faded because of "
    "notification channel is playing");

  rv = notificationAgent->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = notificationAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test4: A notification channel unvisible agent must be playable from "
    "foreground to background");

  rv = notificationAgent->StopPlaying();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = contentAgent->GetCanPlay(&playable, true);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test4: A content channel unvisible agent must be playable "
    "because of notification channel is stopped");

  rv = contentAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
TestPriorities()
{
  nsRefPtr<Agent> normalAgent = new Agent(AudioChannel::Normal);
  nsresult rv = normalAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> contentAgent = new Agent(AudioChannel::Content);
  rv = contentAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> notificationAgent = new Agent(AudioChannel::Notification);
  rv = notificationAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> alarmAgent = new Agent(AudioChannel::Alarm);
  rv = alarmAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> telephonyAgent = new Agent(AudioChannel::Telephony);
  rv = telephonyAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> ringerAgent = new Agent(AudioChannel::Ringer);
  rv = ringerAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> pNotificationAgent =
    new Agent(AudioChannel::Publicnotification);
  rv = pNotificationAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  AudioChannelState playable;

  rv = normalAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test5: A normal channel unvisible agent must be muted");

  rv = contentAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A content channel unvisible agent must be playable while "
    "playing from background state");

  rv = notificationAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A notification channel unvisible agent must be playable");

  rv = alarmAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: An alarm channel unvisible agent must be playable");

  rv = notificationAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test5: A notification channel unvisible agent must be muted when an "
    "alarm is playing");

  rv = telephonyAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A telephony channel unvisible agent must be playable");

  rv = alarmAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test5: An alarm channel unvisible agent must be muted when a telephony "
    "is playing");

  rv = ringerAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A ringer channel unvisible agent must be playable");

  rv = telephonyAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test5: A telephony channel unvisible agent must be muted when a ringer "
    "is playing");

  rv = pNotificationAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A pNotification channel unvisible agent must be playable");

  rv = ringerAgent->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test5: A ringer channel unvisible agent must be muted when a public "
    "notification is playing");

  // Stop to play notification channel or normal/content will be faded.
  // Which already be tested on Test 4.
  rv = notificationAgent->StopPlaying();
  NS_ENSURE_SUCCESS(rv, rv);

  // Settings visible the normal channel.
  rv = normalAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = normalAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A normal channel visible agent must be playable");

  // Set the content channel as visible .
  rv = contentAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Content must be playable because visible.
  rv = contentAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A content channel visible agent must be playable");

  // Set the alarm channel as visible.
  rv = alarmAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = alarmAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: An alarm channel visible agent must be playable");

  // Set the telephony channel as visible.
  rv = telephonyAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = telephonyAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A telephony channel visible agent must be playable");

  // Set the ringer channel as visible.
  rv = ringerAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ringerAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A ringer channel visible agent must be playable");

  // Set the public notification channel as visible.
  rv = pNotificationAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = pNotificationAgent->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test5: A pNotification channel visible agent must be playable");

  return rv;
}

nsresult
TestOneVideoNormalChannel()
{
  nsRefPtr<Agent> agent1 = new Agent(AudioChannel::Normal);
  nsresult rv = agent1->Init(true);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<Agent> agent2 = new Agent(AudioChannel::Content);
  rv = agent2->Init(false);
  NS_ENSURE_SUCCESS(rv, rv);

  AudioChannelState playable;
  rv = agent1->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test6: A video normal channel invisible agent1 must be muted");

  rv = agent2->StartPlaying(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test6: A content channel invisible agent2 must be playable");

  // one video normal channel in foreground and one content channel in background
  rv = agent1->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test6: A video normal channel visible agent1 must be playable");

  rv = agent2->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test6: A content channel invisible agent2 must be muted");

  // both one video normal channel and one content channel in foreground
  rv = agent2->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test6: A video normal channel visible agent1 must be playable");

  rv = agent2->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test6: A content channel visible agent2 must be playable");

  // one video normal channel in background and one content channel in foreground
  rv = agent1->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test6: A video normal channel invisible agent1 must be muted");

  rv = agent2->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test6: A content channel visible agent2 must be playable");

  // both one video normal channel and one content channel in background
  rv = agent2->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_MUTED,
    "Test6: A video normal channel invisible agent1 must be muted");

  rv = agent2->GetCanPlay(&playable);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playable == AUDIO_CHANNEL_STATE_NORMAL,
    "Test6: A content channel invisible agent2 must be playable");

  return rv;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("AudioChannelService");
  if (xpcom.failed()) {
    return 1;
  }

  if (NS_FAILED(TestDoubleStartPlaying())) {
    return 1;
  }

  if (NS_FAILED(TestOneNormalChannel())) {
    return 1;
  }

  if (NS_FAILED(TestTwoNormalChannels())) {
    return 1;
  }

  if (NS_FAILED(TestContentChannels())) {
    return 1;
  }

  if (NS_FAILED(TestFadedState())) {
    return 1;
  }

  // Channel type with AudioChannel::Telephony cannot be unregistered until the
  // main thread has chances to process 1500 millisecond timer. In order to
  // skip ambiguous return value of ChannelsActiveWithHigherPriorityThan(), new
  // test cases are added before any test case that registers the channel type
  // with AudioChannel::Telephony channel.
  if (NS_FAILED(TestOneVideoNormalChannel())) {
    return 1;
  }

  if (NS_FAILED(TestPriorities())) {
    return 1;
  }

  return 0;
}


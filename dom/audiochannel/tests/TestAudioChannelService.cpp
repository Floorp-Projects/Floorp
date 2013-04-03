/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifdef XP_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "TestHarness.h"

// Work around the fact that the nsWeakPtr, used by AudioChannelService.h, is
// not exposed to consumers outside the internal API.
#include "nsIWeakReference.h"
typedef nsCOMPtr<nsIWeakReference> nsWeakPtr;

#include "AudioChannelService.h"
#include "AudioChannelAgent.h"

#define TEST_ENSURE_BASE(_test, _msg)       \
  PR_BEGIN_MACRO                            \
    if (!_test) {                           \
      fail(_msg);                           \
      return NS_ERROR_FAILURE;              \
    } else {                                \
      passed(_msg);                         \
    }                                       \
  PR_END_MACRO

using namespace mozilla::dom;

class Agent : public nsIAudioChannelAgentCallback
{
public:
  NS_DECL_ISUPPORTS

  Agent(AudioChannelType aType)
  : mType(aType)
  , mWaitCallback(false)
  , mRegistered(false)
  , mCanPlay(false)
  {
    mAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
  }

  virtual ~Agent() {}

  nsresult Init()
  {
    nsresult rv = mAgent->Init(mType, this);
    NS_ENSURE_SUCCESS(rv, rv);

    return mAgent->SetVisibilityState(false);
  }

  nsresult StartPlaying(bool *_ret)
  {
    if (mRegistered)
      StopPlaying();

    nsresult rv = mAgent->StartPlaying(_ret);
    mRegistered = true;
    return rv;
  }

  nsresult StopPlaying()
  {
    mRegistered = false;
    int loop = 0;
    while (mWaitCallback) {
      #ifdef XP_WIN
      Sleep(1000);
      #else
      sleep(1);
      #endif
      if (loop++ == 5) {
        TEST_ENSURE_BASE(false, "StopPlaying timeout");
      }
    }
    return mAgent->StopPlaying();
  }

  nsresult SetVisibilityState(bool visible)
  {
    if (mRegistered) {
      mWaitCallback = true;
    }
    return mAgent->SetVisibilityState(visible);
  }

  NS_IMETHODIMP CanPlayChanged(bool canPlay)
  {
    mCanPlay = canPlay;
    mWaitCallback = false;
    return NS_OK;
  }

  nsresult GetCanPlay(bool *_ret)
  {
    int loop = 0;
    while (mWaitCallback) {
      #ifdef XP_WIN
      Sleep(1000);
      #else
      sleep(1);
      #endif
      if (loop++ == 5) {
        TEST_ENSURE_BASE(false, "GetCanPlay timeout");
      }
    }
    *_ret = mCanPlay;
    return NS_OK;
  }

  nsCOMPtr<AudioChannelAgent> mAgent;
  AudioChannelType mType;
  bool mWaitCallback;
  bool mRegistered;
  bool mCanPlay;
};

NS_IMPL_ISUPPORTS1(Agent, nsIAudioChannelAgentCallback)

nsresult
TestDoubleStartPlaying()
{
  nsCOMPtr<Agent> agent = new Agent(AUDIO_CHANNEL_NORMAL);

  nsresult rv = agent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  bool playing;
  rv = agent->mAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent->mAgent->StartPlaying(&playing);
  TEST_ENSURE_BASE(NS_FAILED(rv), "Test0: StartPlaying calling twice should return error");

  return NS_OK;
}

nsresult
TestOneNormalChannel()
{
  nsCOMPtr<Agent> agent = new Agent(AUDIO_CHANNEL_NORMAL);
  nsresult rv = agent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  bool playing;
  rv = agent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test1: A normal channel unvisible agent must not be playing");

  rv = agent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test1: A normal channel visible agent should be playing");

  return rv;
}

nsresult
TestTwoNormalChannels()
{
  nsCOMPtr<Agent> agent1 = new Agent(AUDIO_CHANNEL_NORMAL);
  nsresult rv = agent1->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> agent2 = new Agent(AUDIO_CHANNEL_NORMAL);
  rv = agent2->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  bool playing;
  rv = agent1->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test2: A normal channel unvisible agent1 must not be playing");

  rv = agent2->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test2: A normal channel unvisible agent2 must not be playing");

  rv = agent1->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent2->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test2: A normal channel visible agent1 should be playing");

  rv = agent2->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test2: A normal channel visible agent2 should be playing");

  return rv;
}

nsresult
TestContentChannels()
{
  nsCOMPtr<Agent> agent1 = new Agent(AUDIO_CHANNEL_CONTENT);
  nsresult rv = agent1->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> agent2 = new Agent(AUDIO_CHANNEL_CONTENT);
  rv = agent2->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  /* All content channels in the foreground can be allowed to play */
  rv = agent1->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent2->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool playing;
  rv = agent1->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test3: A content channel visible agent1 should be playing");

  rv = agent2->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test3: A content channel visible agent2 should be playing");

  /* Test the transition state of one content channel tried to set non-visible
   * state first when app is going to background. */
  rv = agent1->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test3: A content channel unvisible agent1 should be playing from foreground to background");

  /* Test all content channels set non-visible already */
  rv = agent2->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent2->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test3: A content channel unvisible agent2 should be playing from foreground to background");

  /* Clear the content channels & mActiveContentChildIDs in AudioChannelService.
   * If agent stop playing in the background, we will reserve it's childID in
   * mActiveContentChildIDs, then it can allow to play next song. So we set agents
   * to foreground first then stopping to play */
  rv = agent1->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent2->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent1->StopPlaying();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent2->StopPlaying();
  NS_ENSURE_SUCCESS(rv, rv);

  /* Test that content channels can't be allow to play when they starts from the background state */
  rv = agent1->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = agent2->SetVisibilityState(false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = agent1->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test3: A content channel unvisible agent1 must not be playing from background state");

  rv = agent2->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test3: A content channel unvisible agent2 must not be playing from background state");

  return rv;
}

nsresult
TestPriorities()
{
  nsCOMPtr<Agent> normalAgent = new Agent(AUDIO_CHANNEL_NORMAL);
  nsresult rv = normalAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> contentAgent = new Agent(AUDIO_CHANNEL_CONTENT);
  rv = contentAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> notificationAgent = new Agent(AUDIO_CHANNEL_NOTIFICATION);
  rv = notificationAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> alarmAgent = new Agent(AUDIO_CHANNEL_ALARM);
  rv = alarmAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> telephonyAgent = new Agent(AUDIO_CHANNEL_TELEPHONY);
  rv = telephonyAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> ringerAgent = new Agent(AUDIO_CHANNEL_RINGER);
  rv = ringerAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Agent> pNotificationAgent = new Agent(AUDIO_CHANNEL_PUBLICNOTIFICATION);
  rv = pNotificationAgent->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  bool playing;

  // Normal should not be playing because not visible.
  rv = normalAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test4: A normal channel unvisible agent must not be playing");

  // Content should be playing.
  rv = contentAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test4: A content channel unvisible agent must not be playing from background state");

  // Notification should be playing.
  rv = notificationAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: An notification channel unvisible agent should be playing");

  // Now content should be not playing because of the notification playing.
  rv = contentAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test4: A content channel unvisible agent should not be playing when notification channel is playing");

  // Adding an alarm.
  rv = alarmAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: An alarm channel unvisible agent should be playing");

  // Now notification should be not playing because of the alarm playing.
  rv = notificationAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test4: A notification channel unvisible agent should not be playing when an alarm is playing");

  // Adding an telephony.
  rv = telephonyAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: An telephony channel unvisible agent should be playing");

  // Now alarm should be not playing because of the telephony playing.
  rv = alarmAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test4: A alarm channel unvisible agent should not be playing when a telephony is playing");

  // Adding an ringer.
  rv = ringerAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: An ringer channel unvisible agent should be playing");

  // Now telephony should be not playing because of the ringer playing.
  rv = telephonyAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test4: A telephony channel unvisible agent should not be playing when a riger is playing");

  // Adding an pNotification.
  rv = pNotificationAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: An pNotification channel unvisible agent should be playing");

  // Now ringer should be not playing because of the public notification playing.
  rv = ringerAgent->StartPlaying(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(!playing, "Test4: A ringer channel unvisible agent should not be playing when a public notification is playing");

  // Settings visible the normal channel.
  rv = normalAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Normal should be playing because visible.
  rv = normalAgent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: A normal channel visible agent should be playing");

  // Settings visible the content channel.
  rv = contentAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Content should be playing because visible.
  rv = contentAgent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: A content channel visible agent should be playing");

  // Settings visible the notification channel.
  rv = notificationAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Notification should be playing because visible.
  rv = notificationAgent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: A notification channel visible agent should be playing");

  // Settings visible the alarm channel.
  rv = alarmAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Alarm should be playing because visible.
  rv = alarmAgent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: A alarm channel visible agent should be playing");

  // Settings visible the telephony channel.
  rv = telephonyAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Telephony should be playing because visible.
  rv = telephonyAgent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: A telephony channel visible agent should be playing");

  // Settings visible the ringer channel.
  rv = ringerAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Ringer should be playing because visible.
  rv = ringerAgent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: A ringer channel visible agent should be playing");

  // Settings visible the pNotification channel.
  rv = pNotificationAgent->SetVisibilityState(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // pNotification should be playing because visible.
  rv = pNotificationAgent->GetCanPlay(&playing);
  NS_ENSURE_SUCCESS(rv, rv);
  TEST_ENSURE_BASE(playing, "Test4: A pNotification channel visible agent should be playing");

  return rv;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("AudioChannelService");
  if (xpcom.failed())
    return 1;

  if (NS_FAILED(TestDoubleStartPlaying()))
    return 1;

  if (NS_FAILED(TestOneNormalChannel()))
    return 1;

  if (NS_FAILED(TestTwoNormalChannels()))
    return 1;

  if (NS_FAILED(TestContentChannels()))
    return 1;

  if (NS_FAILED(TestPriorities()))
    return 1;

  return 0;
}


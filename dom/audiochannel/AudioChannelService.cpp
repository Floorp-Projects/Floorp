/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelService.h"
#include "AudioChannelServiceChild.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/Util.h"

#include "mozilla/dom/ContentParent.h"

#include "nsThreadUtils.h"
#include "nsHashPropertyBag.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#ifdef MOZ_WIDGET_GONK
#include "nsJSUtils.h"
#include "nsCxPusher.h"
#include "nsIAudioManager.h"
#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

StaticRefPtr<AudioChannelService> gAudioChannelService;

// static
AudioChannelService*
AudioChannelService::GetAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return AudioChannelServiceChild::GetAudioChannelService();
  }

  // If we already exist, exit early
  if (gAudioChannelService) {
    return gAudioChannelService;
  }

  // Create new instance, register, return
  nsRefPtr<AudioChannelService> service = new AudioChannelService();
  NS_ENSURE_TRUE(service, nullptr);

  gAudioChannelService = service;
  return gAudioChannelService;
}

void
AudioChannelService::Shutdown()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return AudioChannelServiceChild::Shutdown();
  }

  if (gAudioChannelService) {
    gAudioChannelService = nullptr;
  }
}

NS_IMPL_ISUPPORTS2(AudioChannelService, nsIObserver, nsITimerCallback)

AudioChannelService::AudioChannelService()
: mCurrentHigherChannel(AUDIO_CHANNEL_LAST)
, mCurrentVisibleHigherChannel(AUDIO_CHANNEL_LAST)
, mPlayableHiddenContentChildID(CONTENT_PROCESS_ID_UNKNOWN)
, mDisabled(false)
, mDefChannelChildID(CONTENT_PROCESS_ID_UNKNOWN)
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this, "ipc:content-shutdown", false);
      obs->AddObserver(this, "xpcom-shutdown", false);
#ifdef MOZ_WIDGET_GONK
      // To monitor the volume settings based on audio channel.
      obs->AddObserver(this, "mozsettings-changed", false);
#endif
    }
  }
}

AudioChannelService::~AudioChannelService()
{
}

void
AudioChannelService::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                               AudioChannelType aType,
                                               bool aWithVideo)
{
  if (mDisabled) {
    return;
  }

  MOZ_ASSERT(aType != AUDIO_CHANNEL_DEFAULT);

  AudioChannelAgentData* data = new AudioChannelAgentData(aType,
                                true /* aElementHidden */,
                                AUDIO_CHANNEL_STATE_MUTED /* aState */,
                                aWithVideo);
  mAgents.Put(aAgent, data);
  RegisterType(aType, CONTENT_PROCESS_ID_MAIN, aWithVideo);
}

void
AudioChannelService::RegisterType(AudioChannelType aType, uint64_t aChildID, bool aWithVideo)
{
  if (mDisabled) {
    return;
  }

  AudioChannelInternalType type = GetInternalType(aType, true);
  mChannelCounters[type].AppendElement(aChildID);

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // Since there is another telephony registered, we can unregister old one
    // immediately.
    if (mDeferTelChannelTimer && aType == AUDIO_CHANNEL_TELEPHONY) {
      mDeferTelChannelTimer->Cancel();
      mDeferTelChannelTimer = nullptr;
      UnregisterTypeInternal(aType, mTimerElementHidden, mTimerChildID, false);
    }

    if (aWithVideo) {
      mWithVideoChildIDs.AppendElement(aChildID);
    }

    // One hidden content channel can be playable only when there is no any
    // content channel in the foreground.
    if (type == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
        mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
      mPlayableHiddenContentChildID = aChildID;
    }
    // No hidden content channel can be playable if there is an content channel
    // in foreground.
    else if (type == AUDIO_CHANNEL_INT_CONTENT) {
      mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
    }

    // In order to avoid race conditions, it's safer to notify any existing
    // agent any time a new one is registered.
    SendAudioChannelChangedNotification(aChildID);
    Notify();
  }
}

void
AudioChannelService::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  if (mDisabled) {
    return;
  }

  nsAutoPtr<AudioChannelAgentData> data;
  mAgents.RemoveAndForget(aAgent, data);

  if (data) {
    UnregisterType(data->mType, data->mElementHidden,
                   CONTENT_PROCESS_ID_MAIN, data->mWithVideo);
  }
}

void
AudioChannelService::UnregisterType(AudioChannelType aType,
                                    bool aElementHidden,
                                    uint64_t aChildID,
                                    bool aWithVideo)
{
  if (mDisabled) {
    return;
  }

  // There are two reasons to defer the decrease of telephony channel.
  // 1. User can have time to remove device from his ear before music resuming.
  // 2. Give BT SCO to be disconnected before starting to connect A2DP.
  if (XRE_GetProcessType() == GeckoProcessType_Default &&
      aType == AUDIO_CHANNEL_TELEPHONY &&
      (mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN].Length() +
       mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY].Length()) == 1) {
    mTimerElementHidden = aElementHidden;
    mTimerChildID = aChildID;
    mDeferTelChannelTimer = do_CreateInstance("@mozilla.org/timer;1");
    mDeferTelChannelTimer->InitWithCallback(this, 1500, nsITimer::TYPE_ONE_SHOT);
    return;
  }

  UnregisterTypeInternal(aType, aElementHidden, aChildID, aWithVideo);
}

void
AudioChannelService::UnregisterTypeInternal(AudioChannelType aType,
                                            bool aElementHidden,
                                            uint64_t aChildID,
                                            bool aWithVideo)
{
  // The array may contain multiple occurrence of this appId but
  // this should remove only the first one.
  AudioChannelInternalType type = GetInternalType(aType, aElementHidden);
  MOZ_ASSERT(mChannelCounters[type].Contains(aChildID));
  mChannelCounters[type].RemoveElement(aChildID);

  // In order to avoid race conditions, it's safer to notify any existing
  // agent any time a new one is registered.
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // No hidden content channel is playable if the original playable hidden
    // process does not need to play audio from background anymore.
    if (aType == AUDIO_CHANNEL_CONTENT &&
        mPlayableHiddenContentChildID == aChildID &&
        !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].Contains(aChildID)) {
      mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
    }

    if (aWithVideo) {
      MOZ_ASSERT(mWithVideoChildIDs.Contains(aChildID));
      mWithVideoChildIDs.RemoveElement(aChildID);
    }

    SendAudioChannelChangedNotification(aChildID);
    Notify();
  }
}

void
AudioChannelService::UpdateChannelType(AudioChannelType aType,
                                       uint64_t aChildID,
                                       bool aElementHidden,
                                       bool aElementWasHidden)
{
  // Calculate the new and old internal type and update the hashtable if needed.
  AudioChannelInternalType newType = GetInternalType(aType, aElementHidden);
  AudioChannelInternalType oldType = GetInternalType(aType, aElementWasHidden);

  if (newType != oldType) {
    mChannelCounters[newType].AppendElement(aChildID);
    MOZ_ASSERT(mChannelCounters[oldType].Contains(aChildID));
    mChannelCounters[oldType].RemoveElement(aChildID);
  }

  // The last content channel which goes from foreground to background can also
  // be playable.
  if (oldType == AUDIO_CHANNEL_INT_CONTENT &&
      newType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
      mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
    mPlayableHiddenContentChildID = aChildID;
  }
  // No hidden content channel can be playable if there is an content channel
  // in foreground.
  else if (newType == AUDIO_CHANNEL_INT_CONTENT) {
    mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
  }
}

AudioChannelState
AudioChannelService::GetState(AudioChannelAgent* aAgent, bool aElementHidden)
{
  AudioChannelAgentData* data;
  if (!mAgents.Get(aAgent, &data)) {
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  bool oldElementHidden = data->mElementHidden;
  // Update visibility.
  data->mElementHidden = aElementHidden;

  data->mState = GetStateInternal(data->mType, CONTENT_PROCESS_ID_MAIN,
                                aElementHidden, oldElementHidden);
  return data->mState;
}

AudioChannelState
AudioChannelService::GetStateInternal(AudioChannelType aType, uint64_t aChildID,
                                      bool aElementHidden, bool aElementWasHidden)
{
  UpdateChannelType(aType, aChildID, aElementHidden, aElementWasHidden);

  // Calculating the new and old type and update the hashtable if needed.
  AudioChannelInternalType newType = GetInternalType(aType, aElementHidden);
  AudioChannelInternalType oldType = GetInternalType(aType, aElementWasHidden);

  if (newType != oldType &&
      (aType == AUDIO_CHANNEL_CONTENT ||
       (aType == AUDIO_CHANNEL_NORMAL &&
        mWithVideoChildIDs.Contains(aChildID)))) {
    Notify();
  }

  SendAudioChannelChangedNotification(aChildID);

  // Let play any visible audio channel.
  if (!aElementHidden) {
    if (CheckVolumeFadedCondition(newType, aElementHidden)) {
      return AUDIO_CHANNEL_STATE_FADED;
    }
    return AUDIO_CHANNEL_STATE_NORMAL;
  }

  // We are not visible, maybe we have to mute.
  if (newType == AUDIO_CHANNEL_INT_NORMAL_HIDDEN ||
      (newType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
       // One process can have multiple content channels; and during the
       // transition from foreground to background, its content channels will be
       // updated with correct visibility status one by one. All its content
       // channels should remain playable until all of their visibility statuses
       // have been updated as hidden. After all its content channels have been
       // updated properly as hidden, mPlayableHiddenContentChildID is used to
       // check whether this background process is playable or not.
       !(mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].Contains(aChildID) ||
         (mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty() &&
          mPlayableHiddenContentChildID == aChildID)))) {
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  // After checking the condition on normal & content channel, if the state
  // is not on muted then checking other higher channels type here.
  if (ChannelsActiveWithHigherPriorityThan(newType)) {
    MOZ_ASSERT(newType != AUDIO_CHANNEL_INT_NORMAL_HIDDEN);
    if (CheckVolumeFadedCondition(newType, aElementHidden)) {
      return AUDIO_CHANNEL_STATE_FADED;
    }
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  return AUDIO_CHANNEL_STATE_NORMAL;
}

bool
AudioChannelService::CheckVolumeFadedCondition(AudioChannelInternalType aType,
                                               bool aElementHidden)
{
  // Only normal & content channels are considered
  if (aType > AUDIO_CHANNEL_INT_CONTENT_HIDDEN) {
    return false;
  }

  // Consider that audio from notification is with short duration
  // so just fade the volume not pause it
  if (mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION].IsEmpty() &&
      mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN].IsEmpty()) {
    return false;
  }

  // Since this element is on the foreground, it can be allowed to play always.
  // So return true directly when there is any notification channel alive.
  if (aElementHidden == false) {
   return true;
  }

  // If element is on the background, it is possible paused by channels higher
  // then notification.
  for (int i = AUDIO_CHANNEL_INT_LAST - 1;
    i != AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN; --i) {
    if (!mChannelCounters[i].IsEmpty()) {
      return false;
    }
  }

  return true;
}

bool
AudioChannelService::ContentOrNormalChannelIsActive()
{
  return !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].IsEmpty();
}

bool
AudioChannelService::ProcessContentOrNormalChannelIsActive(uint64_t aChildID)
{
  return mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].Contains(aChildID) ||
         mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].Contains(aChildID) ||
         mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].Contains(aChildID);
}

void
AudioChannelService::SetDefaultVolumeControlChannel(AudioChannelType aType,
                                                    bool aHidden)
{
  SetDefaultVolumeControlChannelInternal(aType, aHidden, CONTENT_PROCESS_ID_MAIN);
}

void
AudioChannelService::SetDefaultVolumeControlChannelInternal(
  AudioChannelType aType, bool aHidden, uint64_t aChildID)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  // If this child is in the background and mDefChannelChildID is set to
  // others then it means other child in the foreground already set it's
  // own default channel already.
  if (!aHidden && mDefChannelChildID != aChildID) {
    return;
  }

  mDefChannelChildID = aChildID;
  nsString channelName;
  channelName.AssignASCII(ChannelName(aType));
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "default-volume-channel-changed",
                         channelName.get());
  }
}

void
AudioChannelService::SendAudioChannelChangedNotification(uint64_t aChildID)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
  props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), aChildID);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(static_cast<nsIWritablePropertyBag*>(props),
                         "audio-channel-process-changed", nullptr);
  }

  // Calculating the most important active channel.
  AudioChannelType higher = AUDIO_CHANNEL_LAST;

  // Top-Down in the hierarchy for visible elements
  if (!mChannelCounters[AUDIO_CHANNEL_INT_PUBLICNOTIFICATION].IsEmpty()) {
    higher = AUDIO_CHANNEL_PUBLICNOTIFICATION;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_RINGER].IsEmpty()) {
    higher = AUDIO_CHANNEL_RINGER;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY].IsEmpty()) {
    higher = AUDIO_CHANNEL_TELEPHONY;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_ALARM].IsEmpty()) {
    higher = AUDIO_CHANNEL_ALARM;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION].IsEmpty()) {
    higher = AUDIO_CHANNEL_NOTIFICATION;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
    higher = AUDIO_CHANNEL_CONTENT;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].IsEmpty()) {
    higher = AUDIO_CHANNEL_NORMAL;
  }

  AudioChannelType visibleHigher = higher;

  // Top-Down in the hierarchy for non-visible elements
  if (higher == AUDIO_CHANNEL_LAST) {
    if (!mChannelCounters[AUDIO_CHANNEL_INT_PUBLICNOTIFICATION_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_PUBLICNOTIFICATION;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_RINGER_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_RINGER;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_TELEPHONY;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_ALARM_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_ALARM;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_NOTIFICATION;
    }

    // Check whether there is any playable hidden content channel or not.
    else if (mPlayableHiddenContentChildID != CONTENT_PROCESS_ID_UNKNOWN) {
      higher = AUDIO_CHANNEL_CONTENT;
    }
  }

  if (higher != mCurrentHigherChannel) {
    mCurrentHigherChannel = higher;

    nsString channelName;
    if (mCurrentHigherChannel != AUDIO_CHANNEL_LAST) {
      channelName.AssignASCII(ChannelName(mCurrentHigherChannel));
    } else {
      channelName.AssignLiteral("none");
    }

    if (obs) {
      obs->NotifyObservers(nullptr, "audio-channel-changed", channelName.get());
    }
  }

  if (visibleHigher != mCurrentVisibleHigherChannel) {
    mCurrentVisibleHigherChannel = visibleHigher;

    nsString channelName;
    if (mCurrentVisibleHigherChannel != AUDIO_CHANNEL_LAST) {
      channelName.AssignASCII(ChannelName(mCurrentVisibleHigherChannel));
    } else {
      channelName.AssignLiteral("none");
    }

    if (obs) {
      obs->NotifyObservers(nullptr, "visible-audio-channel-changed", channelName.get());
    }
  }
}

PLDHashOperator
AudioChannelService::NotifyEnumerator(AudioChannelAgent* aAgent,
                                      AudioChannelAgentData* aData, void* aUnused)
{
  MOZ_ASSERT(aAgent);
  aAgent->NotifyAudioChannelStateChanged();
  return PL_DHASH_NEXT;
}

void
AudioChannelService::Notify()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Notify any agent for the main process.
  mAgents.EnumerateRead(NotifyEnumerator, nullptr);

  // Notify for the child processes.
  nsTArray<ContentParent*> children;
  ContentParent::GetAll(children);
  for (uint32_t i = 0; i < children.Length(); i++) {
    unused << children[i]->SendAudioChannelNotify();
  }
}

NS_IMETHODIMP
AudioChannelService::Notify(nsITimer* aTimer)
{
  UnregisterTypeInternal(AUDIO_CHANNEL_TELEPHONY, mTimerElementHidden, mTimerChildID, false);
  mDeferTelChannelTimer = nullptr;
  return NS_OK;
}

bool
AudioChannelService::ChannelsActiveWithHigherPriorityThan(
  AudioChannelInternalType aType)
{
  for (int i = AUDIO_CHANNEL_INT_LAST - 1;
       i != AUDIO_CHANNEL_INT_CONTENT_HIDDEN; --i) {
    if (i == aType) {
      return false;
    }

    if (!mChannelCounters[i].IsEmpty()) {
      return true;
    }
  }

  return false;
}

const char*
AudioChannelService::ChannelName(AudioChannelType aType)
{
  static struct {
    int32_t type;
    const char* value;
  } ChannelNameTable[] = {
    { AUDIO_CHANNEL_NORMAL,             "normal" },
    { AUDIO_CHANNEL_CONTENT,            "content" },
    { AUDIO_CHANNEL_NOTIFICATION,       "notification" },
    { AUDIO_CHANNEL_ALARM,              "alarm" },
    { AUDIO_CHANNEL_TELEPHONY,          "telephony" },
    { AUDIO_CHANNEL_RINGER,             "ringer" },
    { AUDIO_CHANNEL_PUBLICNOTIFICATION, "publicnotification" },
    { -1,                               "unknown" }
  };

  for (int i = AUDIO_CHANNEL_NORMAL; ; ++i) {
    if (ChannelNameTable[i].type == aType ||
        ChannelNameTable[i].type == -1) {
      return ChannelNameTable[i].value;
    }
  }

  NS_NOTREACHED("Execution should not reach here!");
  return nullptr;
}

NS_IMETHODIMP
AudioChannelService::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    mDisabled = true;
  }

  if (!strcmp(aTopic, "ipc:content-shutdown")) {
    nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
    if (!props) {
      NS_WARNING("ipc:content-shutdown message without property bag as subject");
      return NS_OK;
    }

    int32_t index;
    uint64_t childID = 0;
    nsresult rv = props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"),
                                             &childID);
    if (NS_SUCCEEDED(rv)) {
      for (int32_t type = AUDIO_CHANNEL_INT_NORMAL;
           type < AUDIO_CHANNEL_INT_LAST;
           ++type) {

        while ((index = mChannelCounters[type].IndexOf(childID)) != -1) {
          mChannelCounters[type].RemoveElementAt(index);
        }
      }

      // No hidden content channel is playable if the original playable hidden
      // process shuts down.
      if (mPlayableHiddenContentChildID == childID) {
        mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
      }

      while ((index = mWithVideoChildIDs.IndexOf(childID)) != -1) {
        mWithVideoChildIDs.RemoveElementAt(index);
      }

      // We don't have to remove the agents from the mAgents hashtable because if
      // that table contains only agents running on the same process.

      SendAudioChannelChangedNotification(childID);
      Notify();

      if (mDefChannelChildID == childID) {
        SetDefaultVolumeControlChannelInternal(AUDIO_CHANNEL_DEFAULT,
                                               false, childID);
        mDefChannelChildID = CONTENT_PROCESS_ID_UNKNOWN;
      }
    } else {
      NS_WARNING("ipc:content-shutdown message without childID property");
    }
  }
#ifdef MOZ_WIDGET_GONK
  // To process the volume control on each audio channel according to
  // change of settings
  else if (!strcmp(aTopic, "mozsettings-changed")) {
    AutoSafeJSContext cx;
    nsDependentString dataStr(aData);
    JS::Rooted<JS::Value> val(cx);
    if (!JS_ParseJSON(cx, dataStr.get(), dataStr.Length(), &val) ||
        !val.isObject()) {
      return NS_OK;
    }

    JS::Rooted<JSObject*> obj(cx, &val.toObject());
    JS::Rooted<JS::Value> key(cx);
    if (!JS_GetProperty(cx, obj, "key", &key) ||
        !key.isString()) {
      return NS_OK;
    }

    JS::RootedString jsKey(cx, JS_ValueToString(cx, key));
    if (!jsKey) {
      return NS_OK;
    }
    nsDependentJSString keyStr;
    if (!keyStr.init(cx, jsKey) || keyStr.Find("audio.volume.", 0, false)) {
      return NS_OK;
    }

    JS::Rooted<JS::Value> value(cx);
    if (!JS_GetProperty(cx, obj, "value", &value) || !value.isInt32()) {
      return NS_OK;
    }

    nsCOMPtr<nsIAudioManager> audioManager = do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(audioManager, NS_OK);

    int32_t index = value.toInt32();
    if (keyStr.EqualsLiteral("audio.volume.content")) {
      audioManager->SetAudioChannelVolume(AUDIO_CHANNEL_CONTENT, index);
    } else if (keyStr.EqualsLiteral("audio.volume.notification")) {
      audioManager->SetAudioChannelVolume(AUDIO_CHANNEL_NOTIFICATION, index);
    } else if (keyStr.EqualsLiteral("audio.volume.alarm")) {
      audioManager->SetAudioChannelVolume(AUDIO_CHANNEL_ALARM, index);
    } else if (keyStr.EqualsLiteral("audio.volume.telephony")) {
      audioManager->SetAudioChannelVolume(AUDIO_CHANNEL_TELEPHONY, index);
    } else if (!keyStr.EqualsLiteral("audio.volume.bt_sco")) {
      // bt_sco is not a valid audio channel so we manipulate it in
      // AudioManager.cpp. And the others should not be used.
      // We didn't use MOZ_ASSUME_UNREACHABLE here because any web content who
      // has permission of mozSettings can set any names then it can be easy to
      // crash the B2G.
      NS_WARNING("unexpected audio channel for volume control");
    }
  }
#endif

  return NS_OK;
}

AudioChannelService::AudioChannelInternalType
AudioChannelService::GetInternalType(AudioChannelType aType,
                                     bool aElementHidden)
{
  switch (aType) {
    case AUDIO_CHANNEL_NORMAL:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_NORMAL_HIDDEN
               : AUDIO_CHANNEL_INT_NORMAL;

    case AUDIO_CHANNEL_CONTENT:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_CONTENT_HIDDEN
               : AUDIO_CHANNEL_INT_CONTENT;

    case AUDIO_CHANNEL_NOTIFICATION:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN
               : AUDIO_CHANNEL_INT_NOTIFICATION;

    case AUDIO_CHANNEL_ALARM:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_ALARM_HIDDEN
               : AUDIO_CHANNEL_INT_ALARM;

    case AUDIO_CHANNEL_TELEPHONY:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN
               : AUDIO_CHANNEL_INT_TELEPHONY;

    case AUDIO_CHANNEL_RINGER:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_RINGER_HIDDEN
               : AUDIO_CHANNEL_INT_RINGER;

    case AUDIO_CHANNEL_PUBLICNOTIFICATION:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_PUBLICNOTIFICATION_HIDDEN
               : AUDIO_CHANNEL_INT_PUBLICNOTIFICATION;

    case AUDIO_CHANNEL_LAST:
    default:
      break;
  }

  MOZ_CRASH("unexpected audio channel type");
}

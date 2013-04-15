/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Preferences.h"
#include "nsIAudioManager.h"
#include "FMRadio.h"
#include "nsDOMEvent.h"
#include "nsDOMClassInfo.h"
#include "nsFMRadioSettings.h"
#include "nsCOMPtr.h"

#undef LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "FMRadio" , ## args)
#else
#define LOG(args...)
#endif

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fm.antenna.internal"

#define RADIO_SEEK_COMPLETE_EVENT_NAME   NS_LITERAL_STRING("seekcomplete")
#define RADIO_DIABLED_EVENT_NAME         NS_LITERAL_STRING("disabled")
#define RADIO_ENABLED_EVENT_NAME         NS_LITERAL_STRING("enabled")
#define ANTENNA_STATE_CHANGED_EVENT_NAME NS_LITERAL_STRING("antennastatechange")

#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"

using namespace mozilla::dom::fm;
using namespace mozilla::hal;
using mozilla::Preferences;

FMRadio::FMRadio()
  : mHeadphoneState(SWITCH_STATE_OFF)
  , mHasInternalAntenna(false)
  , mHidden(true)
{
  LOG("FMRadio is initialized.");

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);
  if (mHasInternalAntenna) {
    LOG("We have an internal antenna.");
  } else {
    RegisterSwitchObserver(SWITCH_HEADPHONES, this);
    mHeadphoneState = GetCurrentSwitchState(SWITCH_HEADPHONES);
  }

  RegisterFMRadioObserver(this);
}

FMRadio::~FMRadio()
{
  UnregisterFMRadioObserver(this);
  if (!mHasInternalAntenna) {
    UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
  }
}

DOMCI_DATA(FMRadio, FMRadio)

NS_INTERFACE_MAP_BEGIN(FMRadio)
  NS_INTERFACE_MAP_ENTRY(nsIFMRadio)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(FMRadio)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_EVENT_HANDLER(FMRadio, seekcomplete)
NS_IMPL_EVENT_HANDLER(FMRadio, disabled)
NS_IMPL_EVENT_HANDLER(FMRadio, enabled)
NS_IMPL_EVENT_HANDLER(FMRadio, antennastatechange)

NS_IMPL_ADDREF_INHERITED(FMRadio, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FMRadio, nsDOMEventTargetHelper)

/* readonly attribute boolean isAntennaAvailable; */
NS_IMETHODIMP FMRadio::GetIsAntennaAvailable(bool *aIsAvailable)
{
  if (mHasInternalAntenna) {
    *aIsAvailable = true;
  } else {
    *aIsAvailable = mHeadphoneState != SWITCH_STATE_OFF;
  }
  return NS_OK;
}

/* readonly attribute long frequency; */
NS_IMETHODIMP FMRadio::GetFrequency(int32_t *aFrequency)
{
  *aFrequency = GetFMRadioFrequency();
  return NS_OK;
}

/* readonly attribute blean enabled; */
NS_IMETHODIMP FMRadio::GetEnabled(bool *aEnabled)
{
  *aEnabled = IsFMRadioOn();
  return NS_OK;
}

/* void enable (in nsIFMRadioSettings settings); */
NS_IMETHODIMP FMRadio::Enable(nsIFMRadioSettings *settings)
{
  hal::FMRadioSettings info;

  int32_t upperLimit, lowerLimit, channelWidth;

  if (!mAudioChannelAgent) {
    nsresult rv;
    mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1", &rv);
    if (!mAudioChannelAgent) {
      return NS_ERROR_FAILURE;
    }
    mAudioChannelAgent->Init(AUDIO_CHANNEL_CONTENT, this);
  }

  bool canPlay;
  mAudioChannelAgent->SetVisibilityState(!mHidden);
  mAudioChannelAgent->StartPlaying(&canPlay);

  settings->GetUpperLimit(&upperLimit);
  settings->GetLowerLimit(&lowerLimit);
  settings->GetChannelWidth(&channelWidth);

  info.upperLimit() = upperLimit;
  info.lowerLimit() = lowerLimit;
  info.spaceType() = channelWidth;

  EnableFMRadio(info);

  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(audioManager, NS_OK);

  audioManager->SetFmRadioAudioEnabled(true);
  // We enable the hardware, but mute the audio stream, in order to
  // simplify state handling.  This is simpler but worse for battery
  // life; followup is bug 820282.
  // Note: To adjust FM volume is only available after setting up
  // routing patch.
  CanPlayChanged(canPlay);

  return NS_OK;
}

/* void disableRadio (); */
NS_IMETHODIMP FMRadio::Disable()
{
  // Fix Bug 796733. 
  // DisableFMRadio should be called before SetFmRadioAudioEnabled to prevent
  // the annoying beep sound.
  DisableFMRadio();

  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(audioManager, NS_OK);

  audioManager->SetFmRadioAudioEnabled(false);

  if (mAudioChannelAgent) {
    mAudioChannelAgent->StopPlaying();
    mAudioChannelAgent = nullptr;
  }
  return NS_OK;
}

/* void cancelSeek */
NS_IMETHODIMP FMRadio::CancelSeek()
{
  CancelFMRadioSeek();
  return NS_OK;
}

/* void seek (in long direction); */
NS_IMETHODIMP FMRadio::Seek(int32_t direction)
{
  if (direction == (int)FM_RADIO_SEEK_DIRECTION_UP) {
    FMRadioSeek(FM_RADIO_SEEK_DIRECTION_UP);
  } else {
    FMRadioSeek(FM_RADIO_SEEK_DIRECTION_DOWN);
  }
  return NS_OK;
}

/* nsIFMRadioSettings getSettings (); */
NS_IMETHODIMP FMRadio::GetSettings(nsIFMRadioSettings * *_retval)
{
  hal::FMRadioSettings settings;
  GetFMRadioSettings(&settings);

  nsCOMPtr<nsIFMRadioSettings> radioSettings(new nsFMRadioSettings(
                                                   settings.upperLimit(),
                                                   settings.lowerLimit(),
                                                   settings.spaceType()));
  radioSettings.forget(_retval);

  return NS_OK;
}

/* void setFrequency (in long frequency); */
NS_IMETHODIMP FMRadio::SetFrequency(int32_t frequency)
{
  SetFMRadioFrequency(frequency);
  return NS_OK;
}

NS_IMETHODIMP FMRadio::UpdateVisible(bool aVisible)
{
  mHidden = !aVisible;
  if (mAudioChannelAgent) {
    mAudioChannelAgent->SetVisibilityState(!mHidden);
  }
  return NS_OK;
}

void FMRadio::Notify(const SwitchEvent& aEvent)
{
  if (mHeadphoneState != aEvent.status()) {
    LOG("Antenna state is changed!");
    mHeadphoneState = aEvent.status();
    DispatchTrustedEvent(ANTENNA_STATE_CHANGED_EVENT_NAME);
  }
}

void FMRadio::Notify(const FMRadioOperationInformation& info)
{
  switch (info.operation())
  {
    case FM_RADIO_OPERATION_ENABLE:
      DispatchTrustedEvent(RADIO_ENABLED_EVENT_NAME);
      break;
    case FM_RADIO_OPERATION_DISABLE:
      DispatchTrustedEvent(RADIO_DIABLED_EVENT_NAME);
      break;
    case FM_RADIO_OPERATION_SEEK:
      DispatchTrustedEvent(RADIO_SEEK_COMPLETE_EVENT_NAME);
      break;
    default:
      MOZ_NOT_REACHED();
      return;
  }
}

/* void canPlayChanged (in boolean canPlay); */
NS_IMETHODIMP FMRadio::CanPlayChanged(bool canPlay)
{
  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(audioManager, NS_OK);

  bool AudioEnabled;
  audioManager->GetFmRadioAudioEnabled(&AudioEnabled);
  if (AudioEnabled == canPlay) {
    return NS_OK;
  }

  /* mute fm first, it should be better to stop&resume fm */
  if (canPlay) {
    audioManager->SetFmRadioAudioEnabled(true);
    int32_t volIdx = 0;
    // Restore fm volume, that value is sync as music type
    audioManager->GetStreamVolumeIndex(nsIAudioManager::STREAM_TYPE_MUSIC, &volIdx);
    audioManager->SetStreamVolumeIndex(nsIAudioManager::STREAM_TYPE_FM, volIdx);
  } else {
    audioManager->SetStreamVolumeIndex(nsIAudioManager::STREAM_TYPE_FM, 0);
    audioManager->SetFmRadioAudioEnabled(false);
  }
  return NS_OK;
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLAudioElement.h"
#include "mozilla/dom/HTMLAudioElementBinding.h"
#include "nsError.h"
#include "nsIDOMHTMLAudioElement.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "jsfriendapi.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "AudioSampleFormat.h"
#include "AudioChannelCommon.h"
#include <algorithm>
#include "mozilla/Preferences.h"
#include "nsComponentManagerUtils.h"
#include "nsIHttpChannel.h"
#include "mozilla/dom/TimeRanges.h"
#include "AudioStream.h"

static bool
IsAudioAPIEnabled()
{
  return mozilla::Preferences::GetBool("media.audio_data.enabled", true);
}

NS_IMPL_NS_NEW_HTML_ELEMENT(Audio)

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED4(HTMLAudioElement, HTMLMediaElement,
                             nsIDOMHTMLMediaElement, nsIDOMHTMLAudioElement,
                             nsITimerCallback, nsIAudioChannelAgentCallback)

NS_IMPL_ELEMENT_CLONE(HTMLAudioElement)


HTMLAudioElement::HTMLAudioElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : HTMLMediaElement(aNodeInfo),
    mTimerActivated(false)
{
}

HTMLAudioElement::~HTMLAudioElement()
{
}


already_AddRefed<HTMLAudioElement>
HTMLAudioElement::Audio(const GlobalObject& aGlobal,
                        const Optional<nsAString>& aSrc,
                        ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aGlobal.GetAsSupports());
  nsIDocument* doc;
  if (!win || !(doc = win->GetExtantDoc())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsINodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(nsGkAtoms::audio, nullptr,
                                        kNameSpaceID_XHTML,
                                        nsIDOMNode::ELEMENT_NODE);

  nsRefPtr<HTMLAudioElement> audio = new HTMLAudioElement(nodeInfo.forget());
  audio->SetHTMLAttr(nsGkAtoms::preload, NS_LITERAL_STRING("auto"), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (aSrc.WasPassed()) {
    aRv = audio->SetSrc(aSrc.Value());
  }

  return audio.forget();
}

void
HTMLAudioElement::MozSetup(uint32_t aChannels, uint32_t aRate, ErrorResult& aRv)
{
  if (!IsAudioAPIEnabled()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  OwnerDoc()->WarnOnceAbout(nsIDocument::eMozAudioData);

  // If there is already a src provided, don't setup another stream
  if (mDecoder) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // MozWriteAudio divides by mChannels, so validate now.
  if (0 == aChannels) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mAudioStream) {
    mAudioStream->Shutdown();
  }

#ifdef MOZ_B2G
  if (mTimerActivated) {
    mDeferStopPlayTimer->Cancel();
    mTimerActivated = false;
    UpdateAudioChannelPlayingState();
  }
#endif

  mAudioStream = AudioStream::AllocateStream();
  aRv = mAudioStream->Init(aChannels, aRate, mAudioChannelType, AudioStream::HighLatency);
  if (aRv.Failed()) {
    mAudioStream->Shutdown();
    mAudioStream = nullptr;
    return;
  }

  MetadataLoaded(aChannels, aRate, true, false, nullptr);
  mAudioStream->SetVolume(mMuted ? 0.0 : mVolume);
}

uint32_t
HTMLAudioElement::MozWriteAudio(const float* aData, uint32_t aLength,
                                ErrorResult& aRv)
{
  if (!IsAudioAPIEnabled()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return 0;
  }

  if (!mAudioStream) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return 0;
  }

  // Make sure that we are going to write the correct amount of data based
  // on number of channels.
  if (aLength % mChannels != 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return 0;
  }

#ifdef MOZ_B2G
  if (!mDeferStopPlayTimer) {
    mDeferStopPlayTimer = do_CreateInstance("@mozilla.org/timer;1");
  }

  if (mTimerActivated) {
    mDeferStopPlayTimer->Cancel();
  }
  // The maximum buffer size of audio backend is 1 second, so waiting for 1
  // second is sufficient enough.
  mDeferStopPlayTimer->InitWithCallback(this, 1000, nsITimer::TYPE_ONE_SHOT);
  mTimerActivated = true;
  UpdateAudioChannelPlayingState();
#endif

  // Don't write more than can be written without blocking.
  uint32_t writeLen = std::min(mAudioStream->Available(), aLength / mChannels);

  // Convert the samples back to integers as we are using fixed point audio in
  // the AudioStream.
  // This could be optimized to avoid allocation and memcpy when
  // AudioDataValue is 'float', but it's not worth it for this deprecated API.
  nsAutoArrayPtr<AudioDataValue> audioData(new AudioDataValue[writeLen * mChannels]);
  ConvertAudioSamples(aData, audioData.get(), writeLen * mChannels);
  aRv = mAudioStream->Write(audioData.get(), writeLen);
  if (aRv.Failed()) {
    return 0;
  }
  mAudioStream->Start();

  // Return the actual amount written.
  return writeLen * mChannels;
}

uint64_t
HTMLAudioElement::MozCurrentSampleOffset(ErrorResult& aRv)
{
  if (!IsAudioAPIEnabled()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return 0;
  }

  if (!mAudioStream) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return 0;
  }

  int64_t position = mAudioStream->GetPositionInFrames();
  if (position < 0) {
    return 0;
  }

  return position * mChannels;
}

nsresult HTMLAudioElement::SetAcceptHeader(nsIHttpChannel* aChannel)
{
    nsAutoCString value(
#ifdef MOZ_WEBM
      "audio/webm,"
#endif
#ifdef MOZ_OGG
      "audio/ogg,"
#endif
#ifdef MOZ_WAVE
      "audio/wav,"
#endif
      "audio/*;q=0.9,"
#ifdef MOZ_OGG
      "application/ogg;q=0.7,"
#endif
      "video/*;q=0.6,*/*;q=0.5");

    return aChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                      value,
                                      false);
}

JSObject*
HTMLAudioElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLAudioElementBinding::Wrap(aCx, aScope, this);
}

/* void canPlayChanged (in boolean canPlay); */
NS_IMETHODIMP
HTMLAudioElement::CanPlayChanged(int32_t canPlay)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);
  // Only Audio_Data API will initialize the mAudioStream, so we call the parent
  // one when this audio tag is not used by Audio_Data API.
  if (!mAudioStream) {
    return HTMLMediaElement::CanPlayChanged(canPlay);
  }
#ifdef MOZ_B2G
  if (canPlay != AUDIO_CHANNEL_STATE_MUTED) {
    SetMutedInternal(mMuted & ~MUTED_BY_AUDIO_CHANNEL);
  } else {
    SetMutedInternal(mMuted | MUTED_BY_AUDIO_CHANNEL);
  }

#endif
  return NS_OK;
}

NS_IMETHODIMP
HTMLAudioElement::Notify(nsITimer* aTimer)
{
#ifdef MOZ_B2G
  mTimerActivated = false;
  UpdateAudioChannelPlayingState();
#endif
  return NS_OK;
}

void
HTMLAudioElement::UpdateAudioChannelPlayingState()
{
  if (!mAudioStream) {
    HTMLMediaElement::UpdateAudioChannelPlayingState();
    return;
  }
  // The HTMLAudioElement is registered to the AudioChannelService only on B2G.
#ifdef MOZ_B2G
  if (mTimerActivated != mPlayingThroughTheAudioChannel) {
    mPlayingThroughTheAudioChannel = mTimerActivated;

    if (!mAudioChannelAgent) {
      nsresult rv;
      mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1", &rv);
      if (!mAudioChannelAgent) {
        return;
      }
      // Use a weak ref so the audio channel agent can't leak |this|.
      mAudioChannelAgent->InitWithWeakCallback(mAudioChannelType, this);

      mAudioChannelAgent->SetVisibilityState(!OwnerDoc()->Hidden());
    }

    if (mPlayingThroughTheAudioChannel) {
      int32_t canPlay;
      mAudioChannelAgent->StartPlaying(&canPlay);
      CanPlayChanged(canPlay);
    } else {
      mAudioChannelAgent->StopPlaying();
      mAudioChannelAgent = nullptr;
    }
  }
#endif
}
} // namespace dom
} // namespace mozilla

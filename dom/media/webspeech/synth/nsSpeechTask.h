/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsSpeechTask_h
#define mozilla_dom_nsSpeechTask_h

#include "MediaStreamGraph.h"
#include "SpeechSynthesisUtterance.h"
#include "nsIAudioChannelAgent.h"
#include "nsISpeechService.h"

namespace mozilla {

class SharedBuffer;

namespace dom {

class SpeechSynthesisUtterance;
class SpeechSynthesis;
class SynthStreamListener;

class nsSpeechTask : public nsISpeechTask
                   , public nsIAudioChannelAgentCallback
                   , public nsSupportsWeakReference
{
  friend class SynthStreamListener;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsSpeechTask, nsISpeechTask)

  NS_DECL_NSISPEECHTASK
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  explicit nsSpeechTask(SpeechSynthesisUtterance* aUtterance, bool aIsChrome);
  nsSpeechTask(float aVolume, const nsAString& aText, bool aIsChrome);

  virtual void Pause();

  virtual void Resume();

  virtual void Cancel();

  virtual void ForceEnd();

  float GetCurrentTime();

  uint32_t GetCurrentCharOffset();

  void SetSpeechSynthesis(SpeechSynthesis* aSpeechSynthesis);

  void InitDirectAudio();
  void InitIndirectAudio();

  void SetChosenVoiceURI(const nsAString& aUri);

  virtual void SetAudioOutputVolume(float aVolume);

  void ForceError(float aElapsedTime, uint32_t aCharIndex);

  bool IsPreCanceled()
  {
    return mPreCanceled;
  };

  bool IsPrePaused()
  {
    return mPrePaused;
  }

  bool IsChrome()
  {
    return mIsChrome;
  }

protected:
  virtual ~nsSpeechTask();

  nsresult DispatchStartImpl();

  virtual nsresult DispatchStartImpl(const nsAString& aUri);

  virtual nsresult DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchPauseImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchResumeImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchBoundaryImpl(const nsAString& aName,
                                        float aElapsedTime,
                                        uint32_t aCharIndex,
                                        uint32_t aCharLength,
                                        uint8_t argc);

  virtual nsresult DispatchMarkImpl(const nsAString& aName,
                                    float aElapsedTime, uint32_t aCharIndex);

  RefPtr<SpeechSynthesisUtterance> mUtterance;

  float mVolume;

  nsString mText;

  bool mInited;

  bool mPrePaused;

  bool mPreCanceled;

private:
  void End();

  void SendAudioImpl(RefPtr<mozilla::SharedBuffer>& aSamples, uint32_t aDataLen);

  nsresult DispatchStartInner();

  nsresult DispatchErrorInner(float aElapsedTime, uint32_t aCharIndex);
  nsresult DispatchEndInner(float aElapsedTime, uint32_t aCharIndex);

  void CreateAudioChannelAgent();

  void DestroyAudioChannelAgent();

  RefPtr<SourceMediaStream> mStream;

  RefPtr<MediaInputPort> mPort;

  nsCOMPtr<nsISpeechTaskCallback> mCallback;

  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;

  uint32_t mChannels;

  RefPtr<SpeechSynthesis> mSpeechSynthesis;

  bool mIndirectAudio;

  nsString mChosenVoiceURI;

  bool mIsChrome;
};

} // namespace dom
} // namespace mozilla

#endif

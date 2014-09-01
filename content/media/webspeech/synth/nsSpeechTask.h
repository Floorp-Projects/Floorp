/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsSpeechTask_h
#define mozilla_dom_nsSpeechTask_h

#include "MediaStreamGraph.h"
#include "SpeechSynthesisUtterance.h"
#include "nsISpeechService.h"

namespace mozilla {
namespace dom {

class SpeechSynthesisUtterance;
class SpeechSynthesis;
class SynthStreamListener;

class nsSpeechTask : public nsISpeechTask
{
  friend class SynthStreamListener;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsSpeechTask, nsISpeechTask)

  NS_DECL_NSISPEECHTASK

  explicit nsSpeechTask(SpeechSynthesisUtterance* aUtterance);
  nsSpeechTask(float aVolume, const nsAString& aText);

  virtual void Pause();

  virtual void Resume();

  virtual void Cancel();

  float GetCurrentTime();

  uint32_t GetCurrentCharOffset();

  void SetSpeechSynthesis(SpeechSynthesis* aSpeechSynthesis);

  void SetIndirectAudio(bool aIndirectAudio) { mIndirectAudio = aIndirectAudio; }

protected:
  virtual ~nsSpeechTask();

  virtual nsresult DispatchStartImpl();

  virtual nsresult DispatchEndImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchPauseImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchResumeImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchErrorImpl(float aElapsedTime, uint32_t aCharIndex);

  virtual nsresult DispatchBoundaryImpl(const nsAString& aName,
                                        float aElapsedTime,
                                        uint32_t aCharIndex);

  virtual nsresult DispatchMarkImpl(const nsAString& aName,
                                    float aElapsedTime, uint32_t aCharIndex);

  nsRefPtr<SpeechSynthesisUtterance> mUtterance;

  float mVolume;

  nsString mText;

private:
  void End();

  void SendAudioImpl(int16_t* aData, uint32_t aDataLen);

  nsRefPtr<SourceMediaStream> mStream;

  nsCOMPtr<nsISpeechTaskCallback> mCallback;

  uint32_t mChannels;

  nsRefPtr<SpeechSynthesis> mSpeechSynthesis;

  bool mIndirectAudio;
};

} // namespace dom
} // namespace mozilla

#endif

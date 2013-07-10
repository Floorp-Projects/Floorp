/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "mozilla/Attributes.h"
#include "mozilla/dom/PSpeechSynthesisChild.h"
#include "mozilla/dom/PSpeechSynthesisRequestChild.h"
#include "nsSpeechTask.h"

namespace mozilla {
namespace dom {

class nsSynthVoiceRegistry;
class SpeechSynthesisRequestChild;
class SpeechTaskChild;

class SpeechSynthesisChild : public PSpeechSynthesisChild
{
  friend class nsSynthVoiceRegistry;

public:
  bool RecvVoiceAdded(const RemoteVoice& aVoice);

  bool RecvVoiceRemoved(const nsString& aUri);

  bool RecvSetDefaultVoice(const nsString& aUri, const bool& aIsDefault);

protected:
  SpeechSynthesisChild();
  virtual ~SpeechSynthesisChild();

  PSpeechSynthesisRequestChild* AllocPSpeechSynthesisRequestChild(const nsString& aLang,
                                                                  const nsString& aUri,
                                                                  const nsString& aText,
                                                                  const float& aVolume,
                                                                  const float& aPitch,
                                                                  const float& aRate);
  bool DeallocPSpeechSynthesisRequestChild(PSpeechSynthesisRequestChild* aActor);
};

class SpeechSynthesisRequestChild : public PSpeechSynthesisRequestChild
{
public:
  SpeechSynthesisRequestChild(SpeechTaskChild* aTask);
  virtual ~SpeechSynthesisRequestChild();

protected:
  virtual bool RecvOnStart();

  virtual bool Recv__delete__(const bool& aIsError,
                              const float& aElapsedTime,
                              const uint32_t& aCharIndex);

  virtual bool RecvOnPause(const float& aElapsedTime, const uint32_t& aCharIndex);

  virtual bool RecvOnResume(const float& aElapsedTime, const uint32_t& aCharIndex);

  virtual bool RecvOnError(const float& aElapsedTime, const uint32_t& aCharIndex);

  virtual bool RecvOnBoundary(const nsString& aName, const float& aElapsedTime,
                              const uint32_t& aCharIndex);

  virtual bool RecvOnMark(const nsString& aName, const float& aElapsedTime,
                          const uint32_t& aCharIndex);

  nsRefPtr<SpeechTaskChild> mTask;
};

class SpeechTaskChild : public nsSpeechTask
{
  friend class SpeechSynthesisRequestChild;
public:

  SpeechTaskChild(SpeechSynthesisUtterance* aUtterance);

  NS_IMETHOD Setup(nsISpeechTaskCallback* aCallback,
                   uint32_t aChannels, uint32_t aRate, uint8_t argc) MOZ_OVERRIDE;

  NS_IMETHOD SendAudio (const JS::Value& aData, const JS::Value& aLandmarks,
                        JSContext* aCx) MOZ_OVERRIDE;

  virtual void Pause();

  virtual void Resume();

  virtual void Cancel();

private:
  SpeechSynthesisRequestChild* mActor;
};

} // namespace dom
} // namespace mozilla

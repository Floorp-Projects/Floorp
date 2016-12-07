/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesisChild_h
#define mozilla_dom_SpeechSynthesisChild_h

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
  mozilla::ipc::IPCResult RecvVoiceAdded(const RemoteVoice& aVoice) override;

  mozilla::ipc::IPCResult RecvVoiceRemoved(const nsString& aUri) override;

  mozilla::ipc::IPCResult RecvSetDefaultVoice(const nsString& aUri, const bool& aIsDefault) override;

  mozilla::ipc::IPCResult RecvIsSpeakingChanged(const bool& aIsSpeaking) override;

  mozilla::ipc::IPCResult RecvNotifyVoicesChanged() override;

protected:
  SpeechSynthesisChild();
  virtual ~SpeechSynthesisChild();

  PSpeechSynthesisRequestChild* AllocPSpeechSynthesisRequestChild(const nsString& aLang,
                                                                  const nsString& aUri,
                                                                  const nsString& aText,
                                                                  const float& aVolume,
                                                                  const float& aPitch,
                                                                  const float& aRate) override;
  bool DeallocPSpeechSynthesisRequestChild(PSpeechSynthesisRequestChild* aActor) override;
};

class SpeechSynthesisRequestChild : public PSpeechSynthesisRequestChild
{
public:
  explicit SpeechSynthesisRequestChild(SpeechTaskChild* aTask);
  virtual ~SpeechSynthesisRequestChild();

protected:
  mozilla::ipc::IPCResult RecvOnStart(const nsString& aUri) override;

  mozilla::ipc::IPCResult RecvOnEnd(const bool& aIsError,
                                    const float& aElapsedTime,
                                    const uint32_t& aCharIndex) override;

  mozilla::ipc::IPCResult RecvOnPause(const float& aElapsedTime, const uint32_t& aCharIndex) override;

  mozilla::ipc::IPCResult RecvOnResume(const float& aElapsedTime, const uint32_t& aCharIndex) override;

  mozilla::ipc::IPCResult RecvOnBoundary(const nsString& aName, const float& aElapsedTime,
                                         const uint32_t& aCharIndex,
                                         const uint32_t& aCharLength,
                                         const uint8_t& argc) override;

  mozilla::ipc::IPCResult RecvOnMark(const nsString& aName, const float& aElapsedTime,
                                     const uint32_t& aCharIndex) override;

  RefPtr<SpeechTaskChild> mTask;
};

class SpeechTaskChild : public nsSpeechTask
{
  friend class SpeechSynthesisRequestChild;
public:

  explicit SpeechTaskChild(SpeechSynthesisUtterance* aUtterance);

  NS_IMETHOD Setup(nsISpeechTaskCallback* aCallback,
                   uint32_t aChannels, uint32_t aRate, uint8_t argc) override;

  NS_IMETHOD SendAudio(JS::Handle<JS::Value> aData, JS::Handle<JS::Value> aLandmarks,
                       JSContext* aCx) override;

  NS_IMETHOD SendAudioNative(int16_t* aData, uint32_t aDataLen) override;

  void Pause() override;

  void Resume() override;

  void Cancel() override;

  void ForceEnd() override;

  void SetAudioOutputVolume(float aVolume) override;

private:
  SpeechSynthesisRequestChild* mActor;
};

} // namespace dom
} // namespace mozilla

#endif

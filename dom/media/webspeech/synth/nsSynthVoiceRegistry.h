/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsSynthVoiceRegistry_h
#define mozilla_dom_nsSynthVoiceRegistry_h

#include "nsAutoPtr.h"
#include "nsISynthVoiceRegistry.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"
#include "MediaStreamGraph.h"

class nsISpeechService;

namespace mozilla {
namespace dom {

class RemoteVoice;
class SpeechSynthesisUtterance;
class SpeechSynthesisChild;
class nsSpeechTask;
class VoiceData;
class GlobalQueueItem;

class nsSynthVoiceRegistry final : public nsISynthVoiceRegistry
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISYNTHVOICEREGISTRY

  nsSynthVoiceRegistry();

  already_AddRefed<nsSpeechTask> SpeakUtterance(SpeechSynthesisUtterance& aUtterance,
                                                const nsAString& aDocLang);

  void Speak(const nsAString& aText, const nsAString& aLang,
             const nsAString& aUri, const float& aVolume,  const float& aRate,
             const float& aPitch, nsSpeechTask* aTask);

  void SendVoicesAndState(InfallibleTArray<RemoteVoice>* aVoices,
                          InfallibleTArray<nsString>* aDefaults,
                          bool* aIsSpeaking);

  void SpeakNext();

  void ResumeQueue();

  bool IsSpeaking();

  void SetIsSpeaking(bool aIsSpeaking);

  static nsSynthVoiceRegistry* GetInstance();

  static already_AddRefed<nsSynthVoiceRegistry> GetInstanceForService();

  static void RecvRemoveVoice(const nsAString& aUri);

  static void RecvAddVoice(const RemoteVoice& aVoice);

  static void RecvSetDefaultVoice(const nsAString& aUri, bool aIsDefault);

  static void RecvIsSpeakingChanged(bool aIsSpeaking);

  static void Shutdown();

private:
  virtual ~nsSynthVoiceRegistry();

  VoiceData* FindBestMatch(const nsAString& aUri, const nsAString& lang);

  bool FindVoiceByLang(const nsAString& aLang, VoiceData** aRetval);

  nsresult AddVoiceImpl(nsISpeechService* aService,
                        const nsAString& aUri,
                        const nsAString& aName,
                        const nsAString& aLang,
                        bool aLocalService,
                        bool aQueuesUtterances);

  void SpeakImpl(VoiceData* aVoice,
                 nsSpeechTask* aTask,
                 const nsAString& aText,
                 const float& aVolume,
                 const float& aRate,
                 const float& aPitch);

  nsTArray<RefPtr<VoiceData>> mVoices;

  nsTArray<RefPtr<VoiceData>> mDefaultVoices;

  nsRefPtrHashtable<nsStringHashKey, VoiceData> mUriVoiceMap;

  SpeechSynthesisChild* mSpeechSynthChild;

  bool mUseGlobalQueue;

  nsTArray<RefPtr<GlobalQueueItem>> mGlobalQueue;

  bool mIsSpeaking;
};

} // namespace dom
} // namespace mozilla

#endif

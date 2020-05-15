/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesisService_h
#define mozilla_dom_SpeechSynthesisService_h

#include "nsISpeechService.h"
#include "mozilla/java/SpeechSynthesisServiceNatives.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {

class SpeechSynthesisService final
    : public nsISpeechService,
      public java::SpeechSynthesisService::Natives<SpeechSynthesisService> {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISPEECHSERVICE

  SpeechSynthesisService(){};

  void Setup();

  static void DoneRegisteringVoices();

  static void RegisterVoice(jni::String::Param aUri, jni::String::Param aName,
                            jni::String::Param aLocale, bool aIsNetwork,
                            bool aIsDefault);

  static void DispatchStart(jni::String::Param aUtteranceId);

  static void DispatchEnd(jni::String::Param aUtteranceId);

  static void DispatchError(jni::String::Param aUtteranceId);

  static void DispatchBoundary(jni::String::Param aUtteranceId, int32_t aStart,
                               int32_t aEnd);

  static SpeechSynthesisService* GetInstance(bool aCreate = true);
  static already_AddRefed<SpeechSynthesisService> GetInstanceForService();

  static StaticRefPtr<SpeechSynthesisService> sSingleton;

 private:
  virtual ~SpeechSynthesisService(){};

  nsCOMPtr<nsISpeechTask> mTask;

  // Unique ID assigned to utterance when it is sent to system service.
  nsCString mTaskUtteranceId;

  // Time stamp from the moment the utterance is started.
  TimeStamp mTaskStartTime;

  // Length of text of the utterance.
  uint32_t mTaskTextLength;

  // Current offset in characters of what has been spoken.
  uint32_t mTaskTextOffset;
};

}  // namespace dom
}  // namespace mozilla
#endif

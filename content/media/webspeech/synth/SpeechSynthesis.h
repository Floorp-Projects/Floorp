/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "nsRefPtrHashtable.h"

#include "EnableSpeechSynthesisCheck.h"
#include "SpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"

struct JSContext;
class nsIDOMWindow;

namespace mozilla {
namespace dom {

class nsSpeechTask;

class SpeechSynthesis MOZ_FINAL : public nsISupports,
                                  public nsWrapperCache,
                                  public EnableSpeechSynthesisCheck
{
public:
  SpeechSynthesis(nsPIDOMWindow* aParent);
  virtual ~SpeechSynthesis();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SpeechSynthesis)

  nsIDOMWindow* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  bool Pending() const;

  bool Speaking() const;

  bool Paused() const;

  void Speak(SpeechSynthesisUtterance& aUtterance);

  void Cancel();

  void Pause();

  void Resume();

  void OnEnd(const nsSpeechTask* aTask);

  void GetVoices(nsTArray< nsRefPtr<SpeechSynthesisVoice> >& aResult);

private:

  void AdvanceQueue();

  nsCOMPtr<nsPIDOMWindow> mParent;

  nsTArray<nsRefPtr<SpeechSynthesisUtterance> > mSpeechQueue;

  nsRefPtr<nsSpeechTask> mCurrentTask;

  nsRefPtrHashtable<nsStringHashKey, SpeechSynthesisVoice> mVoiceCache;
};

} // namespace dom
} // namespace mozilla

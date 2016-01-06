/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechSynthesis_h
#define mozilla_dom_SpeechSynthesis_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"

#include "SpeechSynthesisUtterance.h"
#include "SpeechSynthesisVoice.h"

class nsIDOMWindow;

namespace mozilla {
namespace dom {

class nsSpeechTask;

class SpeechSynthesis final : public nsIObserver
                            , public nsWrapperCache
                            , public nsSupportsWeakReference
{
public:
  explicit SpeechSynthesis(nsPIDOMWindow* aParent);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(SpeechSynthesis,
                                                         nsIObserver)
  NS_DECL_NSIOBSERVER

  nsIDOMWindow* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  bool Pending() const;

  bool Speaking() const;

  bool Paused() const;

  bool HasEmptyQueue() const;

  void Speak(SpeechSynthesisUtterance& aUtterance);

  void Cancel();

  void Pause();

  void Resume();

  void OnEnd(const nsSpeechTask* aTask);

  void GetVoices(nsTArray< RefPtr<SpeechSynthesisVoice> >& aResult);

  void ForceEnd();

private:
  virtual ~SpeechSynthesis();

  void AdvanceQueue();

  nsCOMPtr<nsPIDOMWindow> mParent;

  nsTArray<RefPtr<SpeechSynthesisUtterance> > mSpeechQueue;

  RefPtr<nsSpeechTask> mCurrentTask;

  nsRefPtrHashtable<nsStringHashKey, SpeechSynthesisVoice> mVoiceCache;

  bool mHoldQueue;

  uint64_t mInnerID;
};

} // namespace dom
} // namespace mozilla
#endif

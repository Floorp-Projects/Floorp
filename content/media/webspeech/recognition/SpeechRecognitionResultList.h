/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechRecognitionResultList_h
#define mozilla_dom_SpeechRecognitionResultList_h

#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "js/TypeDecls.h"

#include "mozilla/Attributes.h"

#include "EnableWebSpeechRecognitionCheck.h"
#include "SpeechRecognitionResult.h"

namespace mozilla {
namespace dom {

class SpeechRecognition;

class SpeechRecognitionResultList MOZ_FINAL : public nsISupports,
                                              public nsWrapperCache,
                                              public EnableWebSpeechRecognitionCheck
{
public:
  SpeechRecognitionResultList(SpeechRecognition* aParent);
  ~SpeechRecognitionResultList();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SpeechRecognitionResultList)

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  uint32_t Length() const;

  already_AddRefed<SpeechRecognitionResult> Item(uint32_t aIndex);

  already_AddRefed<SpeechRecognitionResult> IndexedGetter(uint32_t aIndex, bool& aPresent);

  nsTArray<nsRefPtr<SpeechRecognitionResult> > mItems;
private:
  nsRefPtr<SpeechRecognition> mParent;
};

} // namespace dom
} // namespace mozilla

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechRecognitionResult_h
#define mozilla_dom_SpeechRecognitionResult_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"
#include "js/TypeDecls.h"

#include "mozilla/Attributes.h"

#include "SpeechRecognitionAlternative.h"

namespace mozilla::dom {

class SpeechRecognitionResult final : public nsISupports,
                                      public nsWrapperCache {
 public:
  explicit SpeechRecognitionResult(SpeechRecognition* aParent);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SpeechRecognitionResult)

  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint32_t Length() const;

  already_AddRefed<SpeechRecognitionAlternative> Item(uint32_t aIndex);

  bool IsFinal() const;

  already_AddRefed<SpeechRecognitionAlternative> IndexedGetter(uint32_t aIndex,
                                                               bool& aPresent);

  nsTArray<RefPtr<SpeechRecognitionAlternative>> mItems;

 private:
  ~SpeechRecognitionResult();

  RefPtr<SpeechRecognition> mParent;
};

}  // namespace mozilla::dom

#endif

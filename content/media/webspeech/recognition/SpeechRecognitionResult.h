/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

#include "mozilla/Attributes.h"

#include "EnableWebSpeechRecognitionCheck.h"
#include "SpeechRecognitionAlternative.h"

struct JSContext;

namespace mozilla {
namespace dom {

class SpeechRecognitionResult MOZ_FINAL : public nsISupports,
                                          public nsWrapperCache,
                                          public EnableWebSpeechRecognitionCheck
{
public:
  SpeechRecognitionResult(SpeechRecognition* aParent);
  ~SpeechRecognitionResult();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SpeechRecognitionResult)

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx, JSObject* aScope);

  uint32_t Length() const;

  already_AddRefed<SpeechRecognitionAlternative> Item(uint32_t aIndex);

  bool Final() const;

  already_AddRefed<SpeechRecognitionAlternative> IndexedGetter(uint32_t aIndex, bool& aPresent);

  nsTArray<nsRefPtr<SpeechRecognitionAlternative> > mItems;
private:
  nsRefPtr<SpeechRecognition> mParent;
};

} // namespace dom
} // namespace mozilla

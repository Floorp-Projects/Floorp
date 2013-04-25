/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsWrapperCache.h"

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"

#include "EnableWebSpeechRecognitionCheck.h"

struct JSContext;

namespace mozilla {
namespace dom {

class GlobalObject;

class SpeechGrammar MOZ_FINAL : public nsISupports,
                                public nsWrapperCache,
                                public EnableWebSpeechRecognitionCheck
{
public:
  SpeechGrammar(nsISupports* aParent);
  ~SpeechGrammar();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(SpeechGrammar)

  nsISupports* GetParentObject() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  static SpeechGrammar* Constructor(const GlobalObject& aGlobal, ErrorResult& aRv);

  void GetSrc(nsString& aRetVal, ErrorResult& aRv) const;

  void SetSrc(const nsAString& aArg, ErrorResult& aRv);

  float GetWeight(ErrorResult& aRv) const;

  void SetWeight(float aArg, ErrorResult& aRv);

private:
  nsCOMPtr<nsISupports> mParent;
};

} // namespace dom
} // namespace mozilla

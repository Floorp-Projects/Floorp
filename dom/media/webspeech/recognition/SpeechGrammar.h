/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechGrammar_h
#define mozilla_dom_SpeechGrammar_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"

#include "mozilla/Attributes.h"

namespace mozilla {
class ErrorResult;

namespace dom {

class GlobalObject;

class SpeechGrammar final : public nsISupports, public nsWrapperCache {
 public:
  explicit SpeechGrammar(nsISupports* aParent);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SpeechGrammar)

  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<SpeechGrammar> Constructor(
      const GlobalObject& aGlobal);

  static already_AddRefed<SpeechGrammar> WebkitSpeechGrammar(
      const GlobalObject& aGlobal, ErrorResult& aRv) {
    return Constructor(aGlobal);
  }

  void GetSrc(nsString& aRetVal, ErrorResult& aRv) const;

  void SetSrc(const nsAString& aArg, ErrorResult& aRv);

  float GetWeight(ErrorResult& aRv) const;

  void SetWeight(float aArg, ErrorResult& aRv);

 private:
  ~SpeechGrammar();

  nsCOMPtr<nsISupports> mParent;

  nsString mSrc;
};

}  // namespace dom
}  // namespace mozilla

#endif

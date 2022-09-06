/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SpeechGrammarList_h
#define mozilla_dom_SpeechGrammarList_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
class SpeechGrammar;
template <typename>
class Optional;

class SpeechGrammarList final : public nsISupports, public nsWrapperCache {
 public:
  explicit SpeechGrammarList(nsISupports* aParent);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SpeechGrammarList)

  static already_AddRefed<SpeechGrammarList> Constructor(
      const GlobalObject& aGlobal);

  static already_AddRefed<SpeechGrammarList> WebkitSpeechGrammarList(
      const GlobalObject& aGlobal, ErrorResult& aRv) {
    return Constructor(aGlobal);
  }

  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  uint32_t Length() const;

  already_AddRefed<SpeechGrammar> Item(uint32_t aIndex, ErrorResult& aRv);

  void AddFromURI(const nsAString& aSrc, const Optional<float>& aWeight,
                  ErrorResult& aRv);

  void AddFromString(const nsAString& aString, const Optional<float>& aWeight,
                     ErrorResult& aRv);

  already_AddRefed<SpeechGrammar> IndexedGetter(uint32_t aIndex, bool& aPresent,
                                                ErrorResult& aRv);

 private:
  ~SpeechGrammarList();

  nsCOMPtr<nsISupports> mParent;

  nsTArray<RefPtr<SpeechGrammar>> mItems;
};

}  // namespace dom
}  // namespace mozilla

#endif

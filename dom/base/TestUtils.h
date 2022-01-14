/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TESTING_TESTUTILS_H_
#define DOM_TESTING_TESTUTILS_H_

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla::dom {

class TestUtils final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestUtils)

  explicit TestUtils(nsIGlobalObject* aGlobal) : mOwner(aGlobal) {}

  nsIGlobalObject* GetParentObject() const { return mOwner; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Gc(ErrorResult& aRv);

  void Shutdown();

 private:
  ~TestUtils() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
};

}  // namespace mozilla::dom

#endif  // DOM_TESTING_TESTUTILS_H_

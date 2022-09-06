/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceMaplikeObject_h
#define mozilla_dom_TestInterfaceMaplikeObject_h

#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
class TestInterfaceMaplike;

// Implementation of test binding for webidl maplike interfaces, using
// primitives for key types and objects for value types.
class TestInterfaceMaplikeObject final : public nsISupports,
                                         public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TestInterfaceMaplikeObject)

  explicit TestInterfaceMaplikeObject(nsPIDOMWindowInner* aParent);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceMaplikeObject> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  // External access for testing internal convenience functions.
  void SetInternal(const nsAString& aKey);
  void ClearInternal();
  bool DeleteInternal(const nsAString& aKey);
  bool HasInternal(const nsAString& aKey);
  already_AddRefed<TestInterfaceMaplike> GetInternal(const nsAString& aKey,
                                                     ErrorResult& aRv);

 private:
  virtual ~TestInterfaceMaplikeObject() = default;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceMaplikeObject_h

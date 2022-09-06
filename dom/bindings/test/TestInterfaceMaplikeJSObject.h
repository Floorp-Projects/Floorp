/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceMaplikeJSObject_h
#define mozilla_dom_TestInterfaceMaplikeJSObject_h

#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

// Implementation of test binding for webidl maplike interfaces, using
// primitives for key types and objects for value types.
class TestInterfaceMaplikeJSObject final : public nsISupports,
                                           public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TestInterfaceMaplikeJSObject)

  explicit TestInterfaceMaplikeJSObject(nsPIDOMWindowInner* aParent);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceMaplikeJSObject> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  // External access for testing internal convenience functions.
  void SetInternal(JSContext* aCx, const nsAString& aKey,
                   JS::Handle<JSObject*> aObject);
  void ClearInternal();
  bool DeleteInternal(const nsAString& aKey);
  bool HasInternal(const nsAString& aKey);
  void GetInternal(JSContext* aCx, const nsAString& aKey,
                   JS::MutableHandle<JSObject*> aRetVal, ErrorResult& aRv);

 private:
  virtual ~TestInterfaceMaplikeJSObject() = default;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceMaplikeJSObject_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceIterableSingle_h
#define mozilla_dom_TestInterfaceIterableSingle_h

#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

// Implementation of test binding for webidl iterable interfaces, using
// primitives for value type
class TestInterfaceIterableSingle final : public nsISupports,
                                          public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TestInterfaceIterableSingle)

  explicit TestInterfaceIterableSingle(nsPIDOMWindowInner* aParent);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceIterableSingle> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  uint32_t Length() const;
  int32_t IndexedGetter(uint32_t aIndex, bool& aFound) const;

 private:
  virtual ~TestInterfaceIterableSingle() = default;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsTArray<int32_t> mValues;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceIterableSingle_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceIterableSingle_h
#define mozilla_dom_TestInterfaceIterableSingle_h

#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

// Implementation of test binding for webidl iterable interfaces, using
// primitives for value type
class TestInterfaceIterableSingle final : public nsISupports,
                                          public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestInterfaceIterableSingle)

  explicit TestInterfaceIterableSingle(nsPIDOMWindow* aParent);
  nsPIDOMWindow* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceIterableSingle>
    Constructor(const GlobalObject& aGlobal, ErrorResult& rv);

  size_t GetIterableLength() const;
  uint32_t GetKeyAtIndex(uint32_t aIndex) const;
  uint32_t GetValueAtIndex(uint32_t aIndex) const;
private:
  virtual ~TestInterfaceIterableSingle() {}
  nsCOMPtr<nsPIDOMWindow> mParent;
  nsTArray<uint32_t> mValues;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TestInterfaceIterableSingle_h

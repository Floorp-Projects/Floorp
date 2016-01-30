/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceMaplike_h
#define mozilla_dom_TestInterfaceMaplike_h

#include "nsWrapperCache.h"
#include "nsCOMPtr.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

// Implementation of test binding for webidl maplike interfaces, using
// primitives for key and value types.
class TestInterfaceMaplike final : public nsISupports,
                                   public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestInterfaceMaplike)

  explicit TestInterfaceMaplike(nsPIDOMWindowInner* aParent);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceMaplike>
    Constructor(const GlobalObject& aGlobal, ErrorResult& rv);

  // External access for testing internal convenience functions.
  void SetInternal(const nsAString& aKey, int32_t aValue);
  void ClearInternal();
  bool DeleteInternal(const nsAString& aKey);
  bool HasInternal(const nsAString& aKey);
private:
  virtual ~TestInterfaceMaplike() {}
  nsCOMPtr<nsPIDOMWindowInner> mParent;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TestInterfaceMaplike_h

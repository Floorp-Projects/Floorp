/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceLength_h
#define mozilla_dom_TestInterfaceLength_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class TestInterfaceLength final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TestInterfaceLength)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(TestInterfaceLength)

 public:
  TestInterfaceLength() = default;

  static already_AddRefed<TestInterfaceLength> Constructor(
      const GlobalObject& aGlobalObject, const bool aArg);

 protected:
  ~TestInterfaceLength() = default;

 public:
  nsISupports* GetParentObject() const { return nullptr; }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TestInterfaceLength_h

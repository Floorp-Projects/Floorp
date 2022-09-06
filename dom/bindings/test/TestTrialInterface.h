/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestTrialInterface_h
#define mozilla_dom_TestTrialInterface_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class TestTrialInterface final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TestTrialInterface)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(TestTrialInterface)

 public:
  TestTrialInterface() = default;

  static already_AddRefed<TestTrialInterface> Constructor(
      const GlobalObject& aGlobalObject);

 protected:
  ~TestTrialInterface() = default;

 public:
  nsISupports* GetParentObject() const { return nullptr; }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TestTrialInterface_h

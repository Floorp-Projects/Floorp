/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A simplere nsIGlobalObject implementation that can be used to set up a new
 * global without anything interesting in it other than the JS builtins.  This
 * is safe to use on both mainthread and worker threads.
 */

#ifndef mozilla_dom_SimpleGlobalObject_h__
#define mozilla_dom_SimpleGlobalObject_h__

#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

namespace mozilla::dom {

class SimpleGlobalObject final : public nsIGlobalObject, public nsWrapperCache {
 public:
  enum class GlobalType {
    BindingDetail,  // Should only be used by DOM bindings code.
    WorkerDebuggerSandbox,
    NotSimpleGlobal  // Sentinel to be used by BasicGlobalType.
  };

  // Create a new JS global object that can be used to do some work.  This
  // global will NOT have any DOM APIs exposed in it, will not be visible to the
  // debugger, and will not have a useful concept of principals, so don't try to
  // use it with any DOM objects.  Apart from that, running code with
  // side-effects is safe in this global.  Importantly, when you are first
  // handed this global it's guaranteed to have pristine built-ins.  The
  // corresponding nsIGlobalObject* for this global object will be a
  // SimpleGlobalObject of the type provided; JS::GetPrivate on the returned
  // JSObject* will return the SimpleGlobalObject*.
  //
  // If the provided prototype value is undefined, it is ignored.  If it's an
  // object or null, it's set as the prototype of the created global.  If it's
  // anything else, this function returns null.
  //
  // Note that creating new globals is not cheap and should not be done
  // gratuitously.  Please think carefully before you use this function.
  static JSObject* Create(GlobalType globalType, JS::Handle<JS::Value> proto =
                                                     JS::UndefinedHandleValue);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(SimpleGlobalObject)

  // Gets the GlobalType of this SimpleGlobalObject.
  GlobalType Type() const { return mType; }

  // Gets the GlobalType of the SimpleGlobalObject for the given JSObject*, if
  // the given JSObject* is the global corresponding to a SimpleGlobalObject.
  // Oherwise, returns GlobalType::NotSimpleGlobal.
  static GlobalType SimpleGlobalType(JSObject* obj);

  JSObject* GetGlobalJSObject() override { return GetWrapper(); }
  JSObject* GetGlobalJSObjectPreserveColor() const override {
    return GetWrapperPreserveColor();
  }

  OriginTrials Trials() const override { return {}; }

  nsISerialEventTarget* SerialEventTarget() const final {
    return NS_GetCurrentThread();
  }
  nsresult Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) const final {
    return NS_DispatchToCurrentThread(std::move(aRunnable));
  }

  JSObject* WrapObject(JSContext* cx,
                       JS::Handle<JSObject*> aGivenProto) override {
    MOZ_CRASH("SimpleGlobalObject doesn't use DOM bindings!");
  }

  bool ShouldResistFingerprinting(RFPTarget aTarget) const override {
    return nsContentUtils::ShouldResistFingerprinting(
        "Presently we don't have enough context to make an informed decision"
        "on JS Sandboxes. See 1782853",
        aTarget);
  }

 private:
  SimpleGlobalObject(JSObject* global, GlobalType type) : mType(type) {
    SetWrapper(global);
  }

  virtual ~SimpleGlobalObject() { MOZ_ASSERT(!GetWrapperMaybeDead()); }

  const GlobalType mType;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_SimpleGlobalObject_h__ */

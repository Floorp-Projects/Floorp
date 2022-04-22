/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortController_h
#define mozilla_dom_AbortController_h

#include "mozilla/dom/BindingDeclarations.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
class ErrorResult;

namespace dom {

class AbortSignal;

class AbortController : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AbortController)

  static already_AddRefed<AbortController> Constructor(
      const GlobalObject& aGlobal, ErrorResult& aRv);

  explicit AbortController(nsIGlobalObject* aGlobal);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject* GetParentObject() const;

  AbortSignal* Signal();

  void Abort(JSContext* aCx, JS::Handle<JS::Value> aReason);

 protected:
  virtual ~AbortController();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<AbortSignal> mSignal;

  bool mAborted;
  JS::Heap<JS::Value> mReason;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_AbortController_h

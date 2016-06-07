/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MMICall_h
#define mozilla_dom_MMICall_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsJSUtils.h"
#include "nsWrapperCache.h"

struct JSContext;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class MMICall final : public nsISupports,
                      public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MMICall)

  MMICall(nsPIDOMWindowInner* aWindow, const nsAString& aServiceCode);

  nsPIDOMWindowInner*
  GetParentObject() const;

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  void
  NotifyResult(JS::Handle<JS::Value> aResult);

  // WebIDL
  already_AddRefed<Promise>
  GetResult(ErrorResult& aRv);

private:
  ~MMICall();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsString mServiceCode;
  RefPtr<Promise> mPromise;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MMICall_h

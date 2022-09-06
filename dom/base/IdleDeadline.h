/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IdleDeadline_h
#define mozilla_dom_IdleDeadline_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMNavigationTiming.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsPIDOMWindowInner;

namespace mozilla::dom {

class IdleDeadline final : public nsISupports, public nsWrapperCache {
 public:
  IdleDeadline(nsPIDOMWindowInner* aWindow, bool aDidTimeout,
               DOMHighResTimeStamp aDeadline);

  IdleDeadline(nsIGlobalObject* aGlobal, bool aDidTimeout,
               DOMHighResTimeStamp aDeadline);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  DOMHighResTimeStamp TimeRemaining();
  bool DidTimeout() const;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(IdleDeadline)

 private:
  ~IdleDeadline();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  const bool mDidTimeout;
  const DOMHighResTimeStamp mDeadline;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_IdleDeadline_h

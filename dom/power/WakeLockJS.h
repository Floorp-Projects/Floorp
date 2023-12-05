/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WAKELOCKJS_H_
#define DOM_WAKELOCKJS_H_

#include "js/TypeDecls.h"
#include "mozilla/dom/WakeLockBinding.h"
#include "nsIDOMEventListener.h"
#include "nsIDocumentActivity.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

class Promise;
class Document;
class WakeLockSentinel;

}  // namespace mozilla::dom

namespace mozilla::dom {

class WakeLockJS final : public nsIDOMEventListener,
                         public nsWrapperCache,
                         public nsIDocumentActivity {
 public:
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIDOCUMENTACTIVITY

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(WakeLockJS,
                                                        nsIDOMEventListener)

 public:
  explicit WakeLockJS(nsPIDOMWindowInner* aWindow);

 protected:
  ~WakeLockJS() = default;

 public:
  nsISupports* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Request(WakeLockType aType);

 private:
  RefPtr<nsPIDOMWindowInner> mWindow;
};

}  // namespace mozilla::dom

#endif  // DOM_WAKELOCKJS_H_

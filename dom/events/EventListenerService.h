/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventListenerService_h_
#define mozilla_EventListenerService_h_

#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIEventListenerService.h"
#include "nsString.h"
#include "nsTObserverArray.h"
#include "nsDataHashtable.h"
#include "nsGkAtoms.h"

class nsIMutableArray;

namespace mozilla {
namespace dom {
class EventTarget;
}  // namespace dom

template <typename T>
class Maybe;

class EventListenerChange final : public nsIEventListenerChange {
 public:
  explicit EventListenerChange(dom::EventTarget* aTarget);

  void AddChangedListenerName(nsAtom* aEventName);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTLISTENERCHANGE

 protected:
  virtual ~EventListenerChange();
  nsCOMPtr<dom::EventTarget> mTarget;
  nsTArray<RefPtr<nsAtom>> mChangedListenerNames;
};

class EventListenerInfo final : public nsIEventListenerInfo {
 public:
  EventListenerInfo(const nsAString& aType,
                    JS::Handle<JSObject*> aScriptedListener,
                    JS::Handle<JSObject*> aScriptedListenerGlobal,
                    bool aCapturing, bool aAllowsUntrusted,
                    bool aInSystemEventGroup);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(EventListenerInfo)
  NS_DECL_NSIEVENTLISTENERINFO

 protected:
  virtual ~EventListenerInfo();

  bool GetJSVal(JSContext* aCx, Maybe<JSAutoRealm>& aAr,
                JS::MutableHandle<JS::Value> aJSVal);

  nsString mType;
  JS::Heap<JSObject*> mScriptedListener;  // May be null.
  // mScriptedListener may be a cross-compartment wrapper so we cannot use it
  // with JSAutoRealm because CCWs are not associated with a single realm. We
  // use this global instead (must be same-compartment with mScriptedListener
  // and must be non-null if mScriptedListener is non-null).
  JS::Heap<JSObject*> mScriptedListenerGlobal;
  bool mCapturing;
  bool mAllowsUntrusted;
  bool mInSystemEventGroup;
};

class EventListenerService final : public nsIEventListenerService {
  ~EventListenerService();

 public:
  EventListenerService();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTLISTENERSERVICE

  static void NotifyAboutMainThreadListenerChange(dom::EventTarget* aTarget,
                                                  nsAtom* aName) {
    if (sInstance) {
      sInstance->NotifyAboutMainThreadListenerChangeInternal(aTarget, aName);
    }
  }

  void NotifyPendingChanges();

 private:
  void NotifyAboutMainThreadListenerChangeInternal(dom::EventTarget* aTarget,
                                                   nsAtom* aName);
  nsTObserverArray<nsCOMPtr<nsIListenerChangeListener>> mChangeListeners;
  nsCOMPtr<nsIMutableArray> mPendingListenerChanges;
  nsDataHashtable<nsISupportsHashKey, RefPtr<EventListenerChange>>
      mPendingListenerChangesSet;

  static EventListenerService* sInstance;
};

}  // namespace mozilla

#endif  // mozilla_EventListenerService_h_

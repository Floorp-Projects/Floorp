/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventListenerService_h_
#define mozilla_EventListenerService_h_

#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsIEventListenerService.h"
#include "nsString.h"

namespace mozilla {

template<typename T>
class Maybe;

class EventListenerInfo MOZ_FINAL : public nsIEventListenerInfo
{
public:
  EventListenerInfo(const nsAString& aType,
                    already_AddRefed<nsIDOMEventListener> aListener,
                    bool aCapturing,
                    bool aAllowsUntrusted,
                    bool aInSystemEventGroup)
    : mType(aType)
    , mListener(aListener)
    , mCapturing(aCapturing)
    , mAllowsUntrusted(aAllowsUntrusted)
    , mInSystemEventGroup(aInSystemEventGroup)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(EventListenerInfo)
  NS_DECL_NSIEVENTLISTENERINFO

protected:
  virtual ~EventListenerInfo() {}

  bool GetJSVal(JSContext* aCx,
                Maybe<JSAutoCompartment>& aAc,
                JS::MutableHandle<JS::Value> aJSVal);

  nsString mType;
  // nsReftPtr because that is what nsListenerStruct uses too.
  nsRefPtr<nsIDOMEventListener> mListener;
  bool mCapturing;
  bool mAllowsUntrusted;
  bool mInSystemEventGroup;
};

class EventListenerService MOZ_FINAL : public nsIEventListenerService
{
  ~EventListenerService() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTLISTENERSERVICE
};

} // namespace mozilla

#endif // mozilla_EventListenerService_h_

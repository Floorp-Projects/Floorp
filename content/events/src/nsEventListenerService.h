/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEventListenerService_h__
#define nsEventListenerService_h__
#include "nsIEventListenerService.h"
#include "nsAutoPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsString.h"
#include "nsCycleCollectionParticipant.h"
#include "jsapi.h"
#include "mozilla/Attributes.h"


class nsEventListenerInfo : public nsIEventListenerInfo
{
public:
  nsEventListenerInfo(const nsAString& aType,
                      already_AddRefed<nsIDOMEventListener> aListener,
                      bool aCapturing, bool aAllowsUntrusted,
                      bool aInSystemEventGroup)
  : mType(aType), mListener(aListener), mCapturing(aCapturing),
    mAllowsUntrusted(aAllowsUntrusted),
    mInSystemEventGroup(aInSystemEventGroup) {}
  virtual ~nsEventListenerInfo() {}
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsEventListenerInfo)
  NS_DECL_NSIEVENTLISTENERINFO
protected:
  bool GetJSVal(JSContext* aCx, mozilla::Maybe<JSAutoCompartment>& aAc,
                JS::Value* aJSVal);

  nsString                      mType;
  // nsReftPtr because that is what nsListenerStruct uses too.
  nsRefPtr<nsIDOMEventListener> mListener;
  bool                          mCapturing;
  bool                          mAllowsUntrusted;
  bool                          mInSystemEventGroup;
};

class nsEventListenerService MOZ_FINAL : public nsIEventListenerService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTLISTENERSERVICE
};
#endif

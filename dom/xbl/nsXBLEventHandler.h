/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLEventHandler_h__
#define nsXBLEventHandler_h__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsTArray.h"

class nsIAtom;
class nsIDOMKeyEvent;
class nsXBLPrototypeHandler;

class nsXBLEventHandler : public nsIDOMEventListener
{
public:
  nsXBLEventHandler(nsXBLPrototypeHandler* aHandler);
  virtual ~nsXBLEventHandler();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

protected:
  nsXBLPrototypeHandler* mProtoHandler;

private:
  nsXBLEventHandler();
  virtual bool EventMatched(nsIDOMEvent* aEvent)
  {
    return true;
  }
};

class nsXBLMouseEventHandler : public nsXBLEventHandler
{
public:
  nsXBLMouseEventHandler(nsXBLPrototypeHandler* aHandler);
  virtual ~nsXBLMouseEventHandler();

private:
  bool EventMatched(nsIDOMEvent* aEvent) MOZ_OVERRIDE;
};

class nsXBLKeyEventHandler : public nsIDOMEventListener
{
public:
  nsXBLKeyEventHandler(nsIAtom* aEventType, uint8_t aPhase, uint8_t aType);
  virtual ~nsXBLKeyEventHandler();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

  void AddProtoHandler(nsXBLPrototypeHandler* aProtoHandler)
  {
    mProtoHandlers.AppendElement(aProtoHandler);
  }

  bool Matches(nsIAtom* aEventType, uint8_t aPhase, uint8_t aType) const
  {
    return (mEventType == aEventType && mPhase == aPhase && mType == aType);
  }

  void GetEventName(nsAString& aString) const
  {
    mEventType->ToString(aString);
  }

  uint8_t GetPhase() const
  {
    return mPhase;
  }

  uint8_t GetType() const
  {
    return mType;
  }

  void SetIsBoundToChrome(bool aIsBoundToChrome)
  {
    mIsBoundToChrome = aIsBoundToChrome;
  }

  void SetUsingContentXBLScope(bool aUsingContentXBLScope)
  {
    mUsingContentXBLScope = aUsingContentXBLScope;
  }

private:
  nsXBLKeyEventHandler();
  bool ExecuteMatchedHandlers(nsIDOMKeyEvent* aEvent, uint32_t aCharCode,
                                bool aIgnoreShiftKey);

  nsTArray<nsXBLPrototypeHandler*> mProtoHandlers;
  nsCOMPtr<nsIAtom> mEventType;
  uint8_t mPhase;
  uint8_t mType;
  bool mIsBoundToChrome;
  bool mUsingContentXBLScope;
};

nsresult
NS_NewXBLEventHandler(nsXBLPrototypeHandler* aHandler,
                      nsIAtom* aEventType,
                      nsXBLEventHandler** aResult);

nsresult
NS_NewXBLKeyEventHandler(nsIAtom* aEventType, uint8_t aPhase,
                         uint8_t aType, nsXBLKeyEventHandler** aResult);

#endif

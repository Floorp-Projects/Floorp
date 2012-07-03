/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLEventHandler_h__
#define nsXBLEventHandler_h__

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
  bool EventMatched(nsIDOMEvent* aEvent);
};

class nsXBLKeyEventHandler : public nsIDOMEventListener
{
public:
  nsXBLKeyEventHandler(nsIAtom* aEventType, PRUint8 aPhase, PRUint8 aType);
  virtual ~nsXBLKeyEventHandler();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

  void AddProtoHandler(nsXBLPrototypeHandler* aProtoHandler)
  {
    mProtoHandlers.AppendElement(aProtoHandler);
  }

  bool Matches(nsIAtom* aEventType, PRUint8 aPhase, PRUint8 aType) const
  {
    return (mEventType == aEventType && mPhase == aPhase && mType == aType);
  }

  void GetEventName(nsAString& aString) const
  {
    mEventType->ToString(aString);
  }

  PRUint8 GetPhase() const
  {
    return mPhase;
  }

  PRUint8 GetType() const
  {
    return mType;
  }

  void SetIsBoundToChrome(bool aIsBoundToChrome)
  {
    mIsBoundToChrome = aIsBoundToChrome;
  }
private:
  nsXBLKeyEventHandler();
  bool ExecuteMatchedHandlers(nsIDOMKeyEvent* aEvent, PRUint32 aCharCode,
                                bool aIgnoreShiftKey);

  nsTArray<nsXBLPrototypeHandler*> mProtoHandlers;
  nsCOMPtr<nsIAtom> mEventType;
  PRUint8 mPhase;
  PRUint8 mType;
  bool mIsBoundToChrome;
};

nsresult
NS_NewXBLEventHandler(nsXBLPrototypeHandler* aHandler,
                      nsIAtom* aEventType,
                      nsXBLEventHandler** aResult);

nsresult
NS_NewXBLKeyEventHandler(nsIAtom* aEventType, PRUint8 aPhase,
                         PRUint8 aType, nsXBLKeyEventHandler** aResult);

#endif

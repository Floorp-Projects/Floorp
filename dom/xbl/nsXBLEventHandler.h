/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLEventHandler_h__
#define nsXBLEventHandler_h__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventListener.h"
#include "nsTArray.h"

class nsAtom;
class nsXBLPrototypeHandler;

namespace mozilla {
struct IgnoreModifierState;
namespace dom {
class Event;
class KeyboardEvent;
} // namespace dom
} // namespace mozilla

class nsXBLEventHandler : public nsIDOMEventListener
{
public:
  explicit nsXBLEventHandler(nsXBLPrototypeHandler* aHandler);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

protected:
  virtual ~nsXBLEventHandler();
  nsXBLPrototypeHandler* mProtoHandler;

private:
  nsXBLEventHandler();
  virtual bool EventMatched(mozilla::dom::Event* aEvent)
  {
    return true;
  }
};

class nsXBLMouseEventHandler : public nsXBLEventHandler
{
public:
  explicit nsXBLMouseEventHandler(nsXBLPrototypeHandler* aHandler);
  virtual ~nsXBLMouseEventHandler();

private:
  bool EventMatched(mozilla::dom::Event* aEvent) override;
};

class nsXBLKeyEventHandler : public nsIDOMEventListener
{
  typedef mozilla::IgnoreModifierState IgnoreModifierState;

public:
  nsXBLKeyEventHandler(nsAtom* aEventType, uint8_t aPhase, uint8_t aType);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMEVENTLISTENER

  void AddProtoHandler(nsXBLPrototypeHandler* aProtoHandler)
  {
    mProtoHandlers.AppendElement(aProtoHandler);
  }

  bool Matches(nsAtom* aEventType, uint8_t aPhase, uint8_t aType) const
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
  virtual ~nsXBLKeyEventHandler();

  bool ExecuteMatchedHandlers(mozilla::dom::KeyboardEvent* aEvent, uint32_t aCharCode,
                              const IgnoreModifierState& aIgnoreModifierState);

  nsTArray<nsXBLPrototypeHandler*> mProtoHandlers;
  RefPtr<nsAtom> mEventType;
  uint8_t mPhase;
  uint8_t mType;
  bool mIsBoundToChrome;
  bool mUsingContentXBLScope;
};

already_AddRefed<nsXBLEventHandler>
NS_NewXBLEventHandler(nsXBLPrototypeHandler* aHandler,
                      nsAtom* aEventType);

#endif

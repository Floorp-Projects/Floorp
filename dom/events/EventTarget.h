/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EventTarget_h_
#define mozilla_dom_EventTarget_h_

#include "mozilla/dom/BindingDeclarations.h"
#include "nsIDOMEventTarget.h"
#include "nsWrapperCache.h"
#include "nsIAtom.h"

class nsPIDOMWindowOuter;
class nsIGlobalObject;

namespace mozilla {

class AsyncEventDispatcher;
class ErrorResult;
class EventListenerManager;

namespace dom {

class AddEventListenerOptionsOrBoolean;
class Event;
class EventListener;
class EventListenerOptionsOrBoolean;
class EventHandlerNonNull;

template <class T> struct Nullable;

// IID for the dom::EventTarget interface
#define NS_EVENTTARGET_IID \
{ 0xde651c36, 0x0053, 0x4c67, \
  { 0xb1, 0x3d, 0x67, 0xb9, 0x40, 0xfc, 0x82, 0xe4 } }

class EventTarget : public nsIDOMEventTarget,
                    public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_EVENTTARGET_IID)

  // WebIDL API
  using nsIDOMEventTarget::AddEventListener;
  using nsIDOMEventTarget::RemoveEventListener;
  using nsIDOMEventTarget::DispatchEvent;
  virtual void AddEventListener(const nsAString& aType,
                                EventListener* aCallback,
                                const AddEventListenerOptionsOrBoolean& aOptions,
                                const Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv) = 0;
  virtual void RemoveEventListener(const nsAString& aType,
                                   EventListener* aCallback,
                                   const EventListenerOptionsOrBoolean& aOptions,
                                   ErrorResult& aRv);
  bool DispatchEvent(Event& aEvent, CallerType aCallerType, ErrorResult& aRv);

  // Note, this takes the type in onfoo form!
  EventHandlerNonNull* GetEventHandler(const nsAString& aType)
  {
    nsCOMPtr<nsIAtom> type = NS_Atomize(aType);
    return GetEventHandler(type, EmptyString());
  }

  // Note, this takes the type in onfoo form!
  void SetEventHandler(const nsAString& aType, EventHandlerNonNull* aHandler,
                       ErrorResult& rv);

  // Note, for an event 'foo' aType will be 'onfoo'.
  virtual void EventListenerAdded(nsIAtom* aType) {}
  virtual void EventListenerRemoved(nsIAtom* aType) {}

  // Returns an outer window that corresponds to the inner window this event
  // target is associated with.  Will return null if the inner window is not the
  // current inner or if there is no window around at all.
  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindings() = 0;

  // The global object this event target is associated with, if any.
  // This may be an inner window or some other global object.  This
  // will never be an outer window.
  virtual nsIGlobalObject* GetOwnerGlobal() const = 0;

  /**
   * Get the event listener manager, creating it if it does not already exist.
   */
  virtual EventListenerManager* GetOrCreateListenerManager() = 0;

  /**
   * Get the event listener manager, returning null if it does not already
   * exist.
   */
  virtual EventListenerManager* GetExistingListenerManager() const = 0;

  // Called from AsyncEventDispatcher to notify it is running.
  virtual void AsyncEventRunning(AsyncEventDispatcher* aEvent) {}

  virtual bool IsApzAware() const;

protected:
  EventHandlerNonNull* GetEventHandler(nsIAtom* aType,
                                       const nsAString& aTypeString);
  void SetEventHandler(nsIAtom* aType, const nsAString& aTypeString,
                       EventHandlerNonNull* aHandler);
};

NS_DEFINE_STATIC_IID_ACCESSOR(EventTarget, NS_EVENTTARGET_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_EventTarget_h_

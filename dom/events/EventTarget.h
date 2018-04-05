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
#include "nsAtom.h"

class nsPIDOMWindowOuter;
class nsIGlobalObject;

namespace mozilla {

class AsyncEventDispatcher;
class ErrorResult;
class EventChainPostVisitor;
class EventChainVisitor;
class EventListenerManager;

namespace dom {

class AddEventListenerOptionsOrBoolean;
class Event;
class EventListener;
class EventListenerOptionsOrBoolean;
class EventHandlerNonNull;
class GlobalObject;

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
  static already_AddRefed<EventTarget> Constructor(const GlobalObject& aGlobal,
                                                   ErrorResult& aRv);
  using nsIDOMEventTarget::AddEventListener;
  using nsIDOMEventTarget::DispatchEvent;
  virtual void AddEventListener(const nsAString& aType,
                                EventListener* aCallback,
                                const AddEventListenerOptionsOrBoolean& aOptions,
                                const Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv) = 0;
  void RemoveEventListener(const nsAString& aType,
                           EventListener* aCallback,
                           const EventListenerOptionsOrBoolean& aOptions,
                           ErrorResult& aRv);
  /**
   * This method allows the removal of event listeners represented by
   * nsIDOMEventListener from the event target, with the same semantics as the
   * standard RemoveEventListener.
   */
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener,
                           bool aUseCapture);
  /**
   * RemoveSystemEventListener() should be used if you have used
   * AddSystemEventListener().
   */
  void RemoveSystemEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 bool aUseCapture);

  bool DispatchEvent(Event& aEvent, CallerType aCallerType, ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const
  {
    return GetOwnerGlobal();
  }

  // Note, this takes the type in onfoo form!
  EventHandlerNonNull* GetEventHandler(const nsAString& aType)
  {
    RefPtr<nsAtom> type = NS_Atomize(aType);
    return GetEventHandler(type, EmptyString());
  }

  // Note, this takes the type in onfoo form!
  void SetEventHandler(const nsAString& aType, EventHandlerNonNull* aHandler,
                       ErrorResult& rv);

  // The nsAtom version of EventListenerAdded is called on the main
  // thread.  The string version is called on workers.
  //
  // For an event 'foo' aType will be 'onfoo' when it's an atom and
  // 'foo' when it's a string..
  virtual void EventListenerAdded(nsAtom* aType) {}
  virtual void EventListenerAdded(const nsAString& aType) {}

  // The nsAtom version of EventListenerRemoved is called on the main
  // thread.  The string version is called on workers.
  //
  // For an event 'foo' aType will be 'onfoo' when it's an atom and
  // 'foo' when it's a string..
  virtual void EventListenerRemoved(nsAtom* aType) {}
  virtual void EventListenerRemoved(const nsAString& aType) {}

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

  // Used by APZ to determine whether this event target has non-chrome event
  // listeners for untrusted key events.
  bool HasNonSystemGroupListenersForUntrustedKeyEvents() const;

  // Used by APZ to determine whether this event target has non-chrome and
  // non-passive event listeners for untrusted key events.
  bool HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents() const;

  virtual bool IsApzAware() const;

  /**
   * Called before the capture phase of the event flow and after event target
   * chain creation. This is used to handle things that must be executed before
   * dispatching the event to DOM.
   */
  virtual nsresult PreHandleEvent(EventChainVisitor& aVisitor)
  {
    return NS_OK;
  }

  /**
   * If EventChainPreVisitor.mWantsWillHandleEvent is set true,
   * called just before possible event handlers on this object will be called.
   */
  virtual void WillHandleEvent(EventChainPostVisitor& aVisitor)
  {
  }

  /**
   * Called after the bubble phase of the system event group.
   * The default handling of the event should happen here.
   * @param aVisitor the visitor object which is used during post handling.
   *
   * @see EventDispatcher.h for documentation about aVisitor.
   * @note Only EventDispatcher should call this method.
   */
  virtual nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) = 0;
  
protected:
  EventHandlerNonNull* GetEventHandler(nsAtom* aType,
                                       const nsAString& aTypeString);
  void SetEventHandler(nsAtom* aType, const nsAString& aTypeString,
                       EventHandlerNonNull* aHandler);
};

NS_DEFINE_STATIC_IID_ACCESSOR(EventTarget, NS_EVENTTARGET_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_EventTarget_h_

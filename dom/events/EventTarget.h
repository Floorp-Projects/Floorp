/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EventTarget_h_
#define mozilla_dom_EventTarget_h_

#include "mozilla/dom/Nullable.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsAtom.h"

class nsIDOMEventListener;
class nsIGlobalObject;
class nsINode;
class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;
class nsPIWindowRoot;

namespace mozilla {

class AsyncEventDispatcher;
class ErrorResult;
class EventChainPostVisitor;
class EventChainPreVisitor;
class EventChainVisitor;
class EventListenerManager;

namespace dom {

class AddEventListenerOptionsOrBoolean;
class Event;
class EventListener;
class EventListenerOptionsOrBoolean;
class EventHandlerNonNull;
class GlobalObject;
class WindowProxyHolder;
enum class CallerType : uint32_t;
enum class EventCallbackDebuggerNotificationType : uint8_t;

// IID for the dom::EventTarget interface
#define NS_EVENTTARGET_IID                           \
  {                                                  \
    0xde651c36, 0x0053, 0x4c67, {                    \
      0xb1, 0x3d, 0x67, 0xb9, 0x40, 0xfc, 0x82, 0xe4 \
    }                                                \
  }

class EventTarget : public nsISupports, public nsWrapperCache {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_EVENTTARGET_IID)

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  void SetIsOnMainThread() {
    MOZ_ASSERT(NS_IsMainThread());
    mRefCnt.SetIsOnMainThread();
  }

#ifndef NS_BUILD_REFCNT_LOGGING
  MozExternalRefCountType NonVirtualAddRef();
  MozExternalRefCountType NonVirtualRelease();
#endif

  // WebIDL API
  static already_AddRefed<EventTarget> Constructor(const GlobalObject& aGlobal,
                                                   ErrorResult& aRv);
  void AddEventListener(const nsAString& aType, EventListener* aCallback,
                        const AddEventListenerOptionsOrBoolean& aOptions,
                        const Nullable<bool>& aWantsUntrusted,
                        ErrorResult& aRv);
  void RemoveEventListener(const nsAString& aType, EventListener* aCallback,
                           const EventListenerOptionsOrBoolean& aOptions,
                           ErrorResult& aRv);

 protected:
  /**
   * This method allows addition of event listeners represented by
   * nsIDOMEventListener, with almost the same semantics as the
   * standard AddEventListener.  The one difference is that it just
   * has a "use capture" boolean, not an EventListenerOptions.
   */
  nsresult AddEventListener(const nsAString& aType,
                            nsIDOMEventListener* aListener, bool aUseCapture,
                            const Nullable<bool>& aWantsUntrusted);

 public:
  /**
   * Helper methods to make the nsIDOMEventListener version of
   * AddEventListener simpler to call for consumers.
   */
  nsresult AddEventListener(const nsAString& aType,
                            nsIDOMEventListener* aListener, bool aUseCapture) {
    return AddEventListener(aType, aListener, aUseCapture, Nullable<bool>());
  }
  nsresult AddEventListener(const nsAString& aType,
                            nsIDOMEventListener* aListener, bool aUseCapture,
                            bool aWantsUntrusted) {
    return AddEventListener(aType, aListener, aUseCapture,
                            Nullable<bool>(aWantsUntrusted));
  }

  /**
   * This method allows the removal of event listeners represented by
   * nsIDOMEventListener from the event target, with the same semantics as the
   * standard RemoveEventListener.
   */
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener, bool aUseCapture);
  /**
   * RemoveSystemEventListener() should be used if you have used
   * AddSystemEventListener().
   */
  void RemoveSystemEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 bool aUseCapture);

  /**
   * Add a system event listener with the default wantsUntrusted value.
   */
  nsresult AddSystemEventListener(const nsAString& aType,
                                  nsIDOMEventListener* aListener,
                                  bool aUseCapture) {
    return AddSystemEventListener(aType, aListener, aUseCapture,
                                  Nullable<bool>());
  }

  /**
   * Add a system event listener with the given wantsUntrusted value.
   */
  nsresult AddSystemEventListener(const nsAString& aType,
                                  nsIDOMEventListener* aListener,
                                  bool aUseCapture, bool aWantsUntrusted) {
    return AddSystemEventListener(aType, aListener, aUseCapture,
                                  Nullable<bool>(aWantsUntrusted));
  }

  virtual bool IsNode() const { return false; }
  inline nsINode* GetAsNode();
  inline const nsINode* GetAsNode() const;
  inline nsINode* AsNode();
  inline const nsINode* AsNode() const;

  virtual bool IsInnerWindow() const { return false; }
  virtual bool IsOuterWindow() const { return false; }
  virtual bool IsRootWindow() const { return false; }
  nsPIDOMWindowInner* GetAsInnerWindow();
  const nsPIDOMWindowInner* GetAsInnerWindow() const;
  nsPIDOMWindowOuter* GetAsOuterWindow();
  const nsPIDOMWindowOuter* GetAsOuterWindow() const;
  inline nsPIWindowRoot* GetAsWindowRoot();
  inline const nsPIWindowRoot* GetAsWindowRoot() const;
  nsPIDOMWindowInner* AsInnerWindow();
  const nsPIDOMWindowInner* AsInnerWindow() const;
  nsPIDOMWindowOuter* AsOuterWindow();
  const nsPIDOMWindowOuter* AsOuterWindow() const;
  inline nsPIWindowRoot* AsWindowRoot();
  inline const nsPIWindowRoot* AsWindowRoot() const;

  /**
   * Returns the EventTarget object which should be used as the target
   * of DOMEvents.
   * Usually |this| is returned, but for example Window (inner windw) returns
   * the WindowProxy (outer window).
   */
  virtual EventTarget* GetTargetForDOMEvent() { return this; };

  /**
   * Returns the EventTarget object which should be used as the target
   * of the event and when constructing event target chain.
   * Usually |this| is returned, but for example WindowProxy (outer window)
   * returns the Window (inner window).
   */
  virtual EventTarget* GetTargetForEventTargetChain() { return this; }

  /**
   * The most general DispatchEvent method.  This is the one the bindings call.
   */
  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY virtual bool DispatchEvent(Event& aEvent,
                                                         CallerType aCallerType,
                                                         ErrorResult& aRv) = 0;

  /**
   * A version of DispatchEvent you can use if you really don't care whether it
   * succeeds or not and whether default is prevented or not.
   */
  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void DispatchEvent(Event& aEvent);

  /**
   * A version of DispatchEvent you can use if you really don't care whether
   * default is prevented or not.
   */
  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void DispatchEvent(Event& aEvent,
                                                 ErrorResult& aRv);

  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  // Note, this takes the type in onfoo form!
  EventHandlerNonNull* GetEventHandler(const nsAString& aType) {
    RefPtr<nsAtom> type = NS_Atomize(aType);
    return GetEventHandler(type);
  }

  // Note, this takes the type in onfoo form!
  void SetEventHandler(const nsAString& aType, EventHandlerNonNull* aHandler,
                       ErrorResult& rv);

  // For an event 'foo' aType will be 'onfoo'.
  virtual void EventListenerAdded(nsAtom* aType) {}

  // For an event 'foo' aType will be 'onfoo'.
  virtual void EventListenerRemoved(nsAtom* aType) {}

  // Returns an outer window that corresponds to the inner window this event
  // target is associated with.  Will return null if the inner window is not the
  // current inner or if there is no window around at all.
  Nullable<WindowProxyHolder> GetOwnerGlobalForBindings();
  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindingsInternal() = 0;

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

  virtual Maybe<EventCallbackDebuggerNotificationType>
  GetDebuggerNotificationType() const {
    return Nothing();
  }

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
   * Called before the capture phase of the event flow.
   * This is used to create the event target chain and implementations
   * should set the necessary members of EventChainPreVisitor.
   * At least aVisitor.mCanHandle must be set,
   * usually also aVisitor.mParentTarget if mCanHandle is true.
   * mCanHandle says that this object can handle the aVisitor.mEvent event and
   * the mParentTarget is the possible parent object for the event target chain.
   * @see EventDispatcher.h for more documentation about aVisitor.
   *
   * @param aVisitor the visitor object which is used to create the
   *                 event target chain for event dispatching.
   *
   * @note Only EventDispatcher should call this method.
   */
  virtual void GetEventTargetParent(EventChainPreVisitor& aVisitor) = 0;

  /**
   * Called on the activation target during dispatch of activation events.
   * https://dom.spec.whatwg.org/#eventtarget-legacy-pre-activation-behavior
   */
  virtual void LegacyPreActivationBehavior(EventChainVisitor& aVisitor) {}

  /**
   * Called on the activation target during dispatch of activation events.
   * https://dom.spec.whatwg.org/#eventtarget-activation-behavior
   */
  MOZ_CAN_RUN_SCRIPT
  virtual void ActivationBehavior(EventChainPostVisitor& aVisitor) {}

  /**
   * Called on the activation target during dispatch of activation events.
   * https://dom.spec.whatwg.org/#eventtarget-legacy-canceled-activation-behavior
   */
  virtual void LegacyCanceledActivationBehavior(
      EventChainPostVisitor& aVisitor) {}

  /**
   * Called before the capture phase of the event flow and after event target
   * chain creation. This is used to handle things that must be executed before
   * dispatching the event to DOM.
   */
  virtual nsresult PreHandleEvent(EventChainVisitor& aVisitor) { return NS_OK; }

  /**
   * If EventChainPreVisitor.mWantsWillHandleEvent is set true,
   * called just before possible event handlers on this object will be called.
   */
  virtual void WillHandleEvent(EventChainPostVisitor& aVisitor) {}

  /**
   * Called after the bubble phase of the system event group.
   * The default handling of the event should happen here.
   * @param aVisitor the visitor object which is used during post handling.
   *
   * @see EventDispatcher.h for documentation about aVisitor.
   * @note Only EventDispatcher should call this method.
   */
  MOZ_CAN_RUN_SCRIPT
  virtual nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) = 0;

 protected:
  EventHandlerNonNull* GetEventHandler(nsAtom* aType);
  void SetEventHandler(nsAtom* aType, EventHandlerNonNull* aHandler);

  /**
   * Hook for AddEventListener that allows it to compute the right
   * wantsUntrusted boolean when one is not provided.  If this returns failure,
   * the listener will not be added.
   *
   * This hook will NOT be called unless aWantsUntrusted is null in
   * AddEventListener.  If you need to take action when event listeners are
   * added, use EventListenerAdded.  Especially because not all event listener
   * additions go through AddEventListener!
   */
  virtual bool ComputeDefaultWantsUntrusted(ErrorResult& aRv) = 0;

  /**
   * A method to compute the right wantsUntrusted value for AddEventListener.
   * This will call the above hook as needed.
   *
   * If aOptions is non-null, and it contains a value for mWantUntrusted, that
   * value takes precedence over aWantsUntrusted.
   */
  bool ComputeWantsUntrusted(const Nullable<bool>& aWantsUntrusted,
                             const AddEventListenerOptionsOrBoolean* aOptions,
                             ErrorResult& aRv);

  /**
   * addSystemEventListener() adds an event listener of aType to the system
   * group.  Typically, core code should use the system group for listening to
   * content (i.e., non-chrome) element's events.  If core code uses
   * EventTarget::AddEventListener for a content node, it means
   * that the listener cannot listen to the event when web content calls
   * stopPropagation() of the event.
   *
   * @param aType            An event name you're going to handle.
   * @param aListener        An event listener.
   * @param aUseCapture      true if you want to listen the event in capturing
   *                         phase.  Otherwise, false.
   * @param aWantsUntrusted  true if you want to handle untrusted events.
   *                         false if not.
   *                         Null if you want the default behavior.
   */
  nsresult AddSystemEventListener(const nsAString& aType,
                                  nsIDOMEventListener* aListener,
                                  bool aUseCapture,
                                  const Nullable<bool>& aWantsUntrusted);
};

NS_DEFINE_STATIC_IID_ACCESSOR(EventTarget, NS_EVENTTARGET_IID)

#define NS_IMPL_FROMEVENTTARGET_GENERIC(_class, _check, _const)             \
  template <typename T>                                                     \
  static auto FromEventTarget(_const T& aEventTarget)                       \
      -> decltype(static_cast<_const _class*>(&aEventTarget)) {             \
    return aEventTarget._check ? static_cast<_const _class*>(&aEventTarget) \
                               : nullptr;                                   \
  }                                                                         \
  template <typename T>                                                     \
  static _const _class* FromEventTarget(_const T* aEventTarget) {           \
    MOZ_DIAGNOSTIC_ASSERT(aEventTarget);                                    \
    return FromEventTarget(*aEventTarget);                                  \
  }                                                                         \
  template <typename T>                                                     \
  static _const _class* FromEventTargetOrNull(_const T* aEventTarget) {     \
    return aEventTarget ? FromEventTarget(*aEventTarget) : nullptr;         \
  }

#define NS_IMPL_FROMEVENTTARGET_HELPER(_class, _check)                         \
  NS_IMPL_FROMEVENTTARGET_GENERIC(_class, _check, )                            \
  NS_IMPL_FROMEVENTTARGET_GENERIC(_class, _check, const)                       \
  template <typename T>                                                        \
  static _class* FromEventTarget(T&& aEventTarget) {                           \
    MOZ_DIAGNOSTIC_ASSERT(!!aEventTarget);                                     \
    /* We need the double-cast in case aEventTarget is a smartptr.  Those */   \
    /* can cast to superclasses of the type they're templated on, */           \
    /* but not directly to subclasses.  */                                     \
    return aEventTarget->_check                                                \
               ? static_cast<_class*>(static_cast<EventTarget*>(aEventTarget)) \
               : nullptr;                                                      \
  }                                                                            \
  template <typename T>                                                        \
  static _class* FromEventTargetOrNull(T&& aEventTarget) {                     \
    return aEventTarget ? FromEventTarget(aEventTarget) : nullptr;             \
  }

// Unfortunately, nsPIDOMWindowInner and nsPIDOMWindowOuter do not inherit
// EventTarget directly, but they are public interfaces which should have
// these helper methods.  Therefore, we cannot cast from EventTarget to
// the interfaces in their header file.  That's the reason why we cannot use
// the zero cost casts nor decltype for the template methods which take a
// reference.
#define NS_IMPL_FROMEVENTTARGET_GENERIC_WITH_GETTER(_class, _getter, _const) \
  static _const _class* FromEventTarget(                                     \
      _const mozilla::dom::EventTarget& aEventTarget) {                      \
    return aEventTarget._getter;                                             \
  }                                                                          \
  template <typename T>                                                      \
  static _const _class* FromEventTarget(_const T* aEventTarget) {            \
    return aEventTarget->_getter;                                            \
  }                                                                          \
  template <typename T>                                                      \
  static _const _class* FromEventTargetOrNull(_const T* aEventTarget) {      \
    return aEventTarget ? aEventTarget->_getter : nullptr;                   \
  }

#define NS_IMPL_FROMEVENTTARGET_HELPER_WITH_GETTER_INNER(_class, _getter) \
  template <typename T>                                                   \
  static _class* FromEventTarget(T&& aEventTarget) {                      \
    return aEventTarget->_getter;                                         \
  }                                                                       \
  template <typename T>                                                   \
  static _class* FromEventTargetOrNull(T&& aEventTarget) {                \
    return aEventTarget ? aEventTarget->_getter : nullptr;                \
  }

#define NS_IMPL_FROMEVENTTARGET_HELPER_WITH_GETTER(_class, _getter)   \
  NS_IMPL_FROMEVENTTARGET_GENERIC_WITH_GETTER(_class, _getter, )      \
  NS_IMPL_FROMEVENTTARGET_GENERIC_WITH_GETTER(_class, _getter, const) \
  NS_IMPL_FROMEVENTTARGET_HELPER_WITH_GETTER_INNER(_class, _getter)

}  // namespace dom
}  // namespace mozilla

#ifdef NS_BUILD_REFCNT_LOGGING
#  define NON_VIRTUAL_ADDREF_RELEASE(class_) /* Nothing */
#else
#  define NON_VIRTUAL_ADDREF_RELEASE(class_)                                 \
    namespace mozilla {                                                      \
    template <>                                                              \
    class RefPtrTraits<class_> {                                             \
     public:                                                                 \
      static void Release(class_* aObject) { aObject->NonVirtualRelease(); } \
      static void AddRef(class_* aObject) { aObject->NonVirtualAddRef(); }   \
    };                                                                       \
    }

#endif

NON_VIRTUAL_ADDREF_RELEASE(mozilla::dom::EventTarget)

#endif  // mozilla_dom_EventTarget_h_

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DOMEventTargetHelper_h_
#define mozilla_DOMEventTargetHelper_h_

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "MainThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/EventTarget.h"

class JSCompartment;

namespace mozilla {

class ErrorResult;

#define NS_DOMEVENTTARGETHELPER_IID \
{ 0xa28385c6, 0x9451, 0x4d7e, \
  { 0xa3, 0xdd, 0xf4, 0xb6, 0x87, 0x2f, 0xa4, 0x76 } }

class DOMEventTargetHelper : public dom::EventTarget
{
public:
  DOMEventTargetHelper()
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
  {
  }
  DOMEventTargetHelper(nsPIDOMWindow* aWindow)
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
  {
    BindToOwner(aWindow);
    // All objects coming through here are WebIDL objects
    SetIsDOMBinding();
  }
  DOMEventTargetHelper(DOMEventTargetHelper* aOther)
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
  {
    BindToOwner(aOther);
    // All objects coming through here are WebIDL objects
    SetIsDOMBinding();
  }

  virtual ~DOMEventTargetHelper();
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(DOMEventTargetHelper)

  NS_DECL_NSIDOMEVENTTARGET

  virtual EventListenerManager* GetExistingListenerManager() const MOZ_OVERRIDE;
  virtual EventListenerManager* GetOrCreateListenerManager() MOZ_OVERRIDE;

  using dom::EventTarget::RemoveEventListener;
  virtual void AddEventListener(const nsAString& aType,
                                dom::EventListener* aListener,
                                bool aCapture,
                                const dom::Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv) MOZ_OVERRIDE;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMEVENTTARGETHELPER_IID)

  void GetParentObject(nsIScriptGlobalObject **aParentObject)
  {
    if (mParentObject) {
      CallQueryInterface(mParentObject, aParentObject);
    } else {
      *aParentObject = nullptr;
    }
  }

  static DOMEventTargetHelper* FromSupports(nsISupports* aSupports)
  {
    dom::EventTarget* target = static_cast<dom::EventTarget*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<dom::EventTarget> target_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the EventTarget pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(target_qi == target, "Uh, fix QI!");
    }
#endif

    return static_cast<DOMEventTargetHelper*>(target);
  }

  bool HasListenersFor(nsIAtom* aTypeWithOn)
  {
    return mListenerManager && mListenerManager->HasListenersFor(aTypeWithOn);
  }

  nsresult SetEventHandler(nsIAtom* aType,
                           JSContext* aCx,
                           const JS::Value& aValue);
  using dom::EventTarget::SetEventHandler;
  void GetEventHandler(nsIAtom* aType,
                       JSContext* aCx,
                       JS::Value* aValue);
  using dom::EventTarget::GetEventHandler;
  virtual nsIDOMWindow* GetOwnerGlobal() MOZ_OVERRIDE
  {
    return nsPIDOMWindow::GetOuterFromCurrentInner(GetOwner());
  }

  nsresult CheckInnerWindowCorrectness()
  {
    NS_ENSURE_STATE(!mHasOrHasHadOwnerWindow || mOwnerWindow);
    if (mOwnerWindow && !mOwnerWindow->IsCurrentInnerWindow()) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  nsPIDOMWindow* GetOwner() const { return mOwnerWindow; }
  void BindToOwner(nsIGlobalObject* aOwner);
  void BindToOwner(nsPIDOMWindow* aOwner);
  void BindToOwner(DOMEventTargetHelper* aOther);
  virtual void DisconnectFromOwner();                   
  nsIGlobalObject* GetParentObject() const { return mParentObject; }
  bool HasOrHasHadOwner() { return mHasOrHasHadOwnerWindow; }

  virtual void EventListenerAdded(nsIAtom* aType) MOZ_OVERRIDE;
  virtual void EventListenerRemoved(nsIAtom* aType) MOZ_OVERRIDE;
  virtual void EventListenerWasAdded(const nsAString& aType,
                                     ErrorResult& aRv,
                                     JSCompartment* aCompartment = nullptr) {}
  virtual void EventListenerWasRemoved(const nsAString& aType,
                                       ErrorResult& aRv,
                                       JSCompartment* aCompartment = nullptr) {}
protected:
  nsresult WantsUntrusted(bool* aRetVal);

  nsRefPtr<EventListenerManager> mListenerManager;
  // Dispatch a trusted, non-cancellable and non-bubbling event to |this|.
  nsresult DispatchTrustedEvent(const nsAString& aEventName);
  // Make |event| trusted and dispatch |aEvent| to |this|.
  nsresult DispatchTrustedEvent(nsIDOMEvent* aEvent);

  virtual void LastRelease() {}
private:
  // Inner window or sandbox.
  nsIGlobalObject*           mParentObject;
  // mParentObject pre QI-ed and cached (inner window)
  // (it is needed for off main thread access)
  nsPIDOMWindow*             mOwnerWindow;
  bool                       mHasOrHasHadOwnerWindow;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DOMEventTargetHelper,
                              NS_DOMEVENTTARGETHELPER_IID)

} // namespace mozilla

// XPIDL event handlers
#define NS_IMPL_EVENT_HANDLER(_class, _event)                                 \
    NS_IMETHODIMP _class::GetOn##_event(JSContext* aCx,                       \
                                        JS::MutableHandle<JS::Value> aValue)  \
    {                                                                         \
      GetEventHandler(nsGkAtoms::on##_event, aCx, aValue.address());          \
      return NS_OK;                                                           \
    }                                                                         \
    NS_IMETHODIMP _class::SetOn##_event(JSContext* aCx,                       \
                                        JS::Handle<JS::Value> aValue)         \
    {                                                                         \
      return SetEventHandler(nsGkAtoms::on##_event, aCx, aValue);             \
    }

#define NS_IMPL_FORWARD_EVENT_HANDLER(_class, _event, _baseclass)             \
    NS_IMETHODIMP _class::GetOn##_event(JSContext* aCx,                       \
                                        JS::MutableHandle<JS::Value> aValue)  \
    {                                                                         \
      return _baseclass::GetOn##_event(aCx, aValue);                          \
    }                                                                         \
    NS_IMETHODIMP _class::SetOn##_event(JSContext* aCx,                       \
                                        JS::Handle<JS::Value> aValue)         \
    {                                                                         \
      return _baseclass::SetOn##_event(aCx, aValue);                          \
    }

// WebIDL event handlers
#define IMPL_EVENT_HANDLER(_event)                                        \
  inline mozilla::dom::EventHandlerNonNull* GetOn##_event()               \
  {                                                                       \
    if (NS_IsMainThread()) {                                              \
      return GetEventHandler(nsGkAtoms::on##_event, EmptyString());       \
    }                                                                     \
    return GetEventHandler(nullptr, NS_LITERAL_STRING(#_event));          \
  }                                                                       \
  inline void SetOn##_event(mozilla::dom::EventHandlerNonNull* aCallback) \
  {                                                                       \
    if (NS_IsMainThread()) {                                              \
      SetEventHandler(nsGkAtoms::on##_event, EmptyString(), aCallback);   \
    } else {                                                              \
      SetEventHandler(nullptr, NS_LITERAL_STRING(#_event), aCallback);    \
    }                                                                     \
  }

/* Use this macro to declare functions that forward the behavior of this
 * interface to another object.
 * This macro doesn't forward PreHandleEvent because sometimes subclasses
 * want to override it.
 */
#define NS_FORWARD_NSIDOMEVENTTARGET_NOPREHANDLEEVENT(_to) \
  NS_IMETHOD AddEventListener(const nsAString & type, nsIDOMEventListener *listener, bool useCapture, bool wantsUntrusted, uint8_t _argc) { \
    return _to AddEventListener(type, listener, useCapture, wantsUntrusted, _argc); \
  } \
  NS_IMETHOD AddSystemEventListener(const nsAString & type, nsIDOMEventListener *listener, bool aUseCapture, bool aWantsUntrusted, uint8_t _argc) { \
    return _to AddSystemEventListener(type, listener, aUseCapture, aWantsUntrusted, _argc); \
  } \
  NS_IMETHOD RemoveEventListener(const nsAString & type, nsIDOMEventListener *listener, bool useCapture) { \
    return _to RemoveEventListener(type, listener, useCapture); \
  } \
  NS_IMETHOD RemoveSystemEventListener(const nsAString & type, nsIDOMEventListener *listener, bool aUseCapture) { \
    return _to RemoveSystemEventListener(type, listener, aUseCapture); \
  } \
  NS_IMETHOD DispatchEvent(nsIDOMEvent *evt, bool *_retval) { \
    return _to DispatchEvent(evt, _retval); \
  } \
  virtual mozilla::dom::EventTarget* GetTargetForDOMEvent() { \
    return _to GetTargetForDOMEvent(); \
  } \
  virtual mozilla::dom::EventTarget* GetTargetForEventTargetChain() { \
    return _to GetTargetForEventTargetChain(); \
  } \
  virtual nsresult WillHandleEvent( \
                     mozilla::EventChainPostVisitor & aVisitor) { \
    return _to WillHandleEvent(aVisitor); \
  } \
  virtual nsresult PostHandleEvent( \
                     mozilla::EventChainPostVisitor & aVisitor) { \
    return _to PostHandleEvent(aVisitor); \
  } \
  virtual nsresult DispatchDOMEvent(mozilla::WidgetEvent* aEvent, nsIDOMEvent* aDOMEvent, nsPresContext* aPresContext, nsEventStatus* aEventStatus) { \
    return _to DispatchDOMEvent(aEvent, aDOMEvent, aPresContext, aEventStatus); \
  } \
  virtual mozilla::EventListenerManager* GetOrCreateListenerManager() { \
    return _to GetOrCreateListenerManager(); \
  } \
  virtual mozilla::EventListenerManager* GetExistingListenerManager() const { \
    return _to GetExistingListenerManager(); \
  } \
  virtual nsIScriptContext * GetContextForEventHandlers(nsresult *aRv) { \
    return _to GetContextForEventHandlers(aRv); \
  } \
  virtual JSContext * GetJSContextForEventHandlers(void) { \
    return _to GetJSContextForEventHandlers(); \
  }

#define NS_REALLY_FORWARD_NSIDOMEVENTTARGET(_class) \
  using _class::AddEventListener;                   \
  using _class::RemoveEventListener;                \
  NS_FORWARD_NSIDOMEVENTTARGET(_class::)            \
  virtual mozilla::EventListenerManager*            \
  GetOrCreateListenerManager() {                    \
    return _class::GetOrCreateListenerManager();    \
  }                                                 \
  virtual mozilla::EventListenerManager*            \
  GetExistingListenerManager() const {              \
    return _class::GetExistingListenerManager();    \
  }

#endif // mozilla_DOMEventTargetHelper_h_

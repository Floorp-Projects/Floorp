/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMEventTargetHelper_h_
#define nsDOMEventTargetHelper_h_

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsEventListenerManager.h"
#include "nsIScriptContext.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/EventTarget.h"

class nsDOMEvent;

#define NS_DOMEVENTTARGETHELPER_IID \
{ 0xda0e6d40, 0xc17b, 0x4937, \
  { 0x8e, 0xa2, 0x99, 0xca, 0x1c, 0x81, 0xea, 0xbe } }

class nsDOMEventTargetHelper : public mozilla::dom::EventTarget
{
public:
  nsDOMEventTargetHelper()
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
  {}
  nsDOMEventTargetHelper(nsPIDOMWindow* aWindow)
    : mParentObject(nullptr)
    , mOwnerWindow(nullptr)
    , mHasOrHasHadOwnerWindow(false)
  {
    BindToOwner(aWindow);
    // All objects coming through here are WebIDL objects
    SetIsDOMBinding();
  }

  virtual ~nsDOMEventTargetHelper();
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(nsDOMEventTargetHelper)

  NS_DECL_NSIDOMEVENTTARGET
  using mozilla::dom::EventTarget::RemoveEventListener;
  virtual void AddEventListener(const nsAString& aType,
                                nsIDOMEventListener* aListener,
                                bool aCapture,
                                const mozilla::dom::Nullable<bool>& aWantsUntrusted,
                                mozilla::ErrorResult& aRv) MOZ_OVERRIDE;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DOMEVENTTARGETHELPER_IID)

  void GetParentObject(nsIScriptGlobalObject **aParentObject)
  {
    if (mParentObject) {
      CallQueryInterface(mParentObject, aParentObject);
    } else {
      *aParentObject = nullptr;
    }
  }

  static nsDOMEventTargetHelper* FromSupports(nsISupports* aSupports)
  {
    mozilla::dom::EventTarget* target =
      static_cast<mozilla::dom::EventTarget*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<mozilla::dom::EventTarget> target_qi =
        do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the EventTarget pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(target_qi == target, "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMEventTargetHelper*>(target);
  }

  bool HasListenersFor(nsIAtom* aTypeWithOn)
  {
    return mListenerManager && mListenerManager->HasListenersFor(aTypeWithOn);
  }

  nsresult SetEventHandler(nsIAtom* aType,
                           JSContext* aCx,
                           const JS::Value& aValue);
  void SetEventHandler(nsIAtom* aType,
                       mozilla::dom::EventHandlerNonNull* aHandler,
                       mozilla::ErrorResult& rv)
  {
    rv = GetListenerManager(true)->SetEventHandler(aType, aHandler);
  }
  void GetEventHandler(nsIAtom* aType,
                       JSContext* aCx,
                       JS::Value* aValue);
  mozilla::dom::EventHandlerNonNull* GetEventHandler(nsIAtom* aType)
  {
    nsEventListenerManager* elm = GetListenerManager(false);
    return elm ? elm->GetEventHandler(aType) : nullptr;
  }

  nsresult CheckInnerWindowCorrectness()
  {
    NS_ENSURE_STATE(!mHasOrHasHadOwnerWindow || mOwnerWindow);
    if (mOwnerWindow) {
      NS_ASSERTION(mOwnerWindow->IsInnerWindow(), "Should have inner window here!\n");
      nsPIDOMWindow* outer = mOwnerWindow->GetOuterWindow();
      if (!outer || outer->GetCurrentInnerWindow() != mOwnerWindow) {
        return NS_ERROR_FAILURE;
      }
    }
    return NS_OK;
  }

  nsPIDOMWindow* GetOwner() const { return mOwnerWindow; }
  void BindToOwner(nsIGlobalObject* aOwner);
  void BindToOwner(nsPIDOMWindow* aOwner);
  void BindToOwner(nsDOMEventTargetHelper* aOther);
  virtual void DisconnectFromOwner();                   
  nsIGlobalObject* GetParentObject() const { return mParentObject; }
  bool HasOrHasHadOwner() { return mHasOrHasHadOwnerWindow; }
protected:
  nsRefPtr<nsEventListenerManager> mListenerManager;
  // Dispatch a trusted, non-cancellable and non-bubbling event to |this|.
  nsresult DispatchTrustedEvent(const nsAString& aEventName);
  // Make |event| trusted and dispatch |aEvent| to |this|.
  nsresult DispatchTrustedEvent(nsIDOMEvent* aEvent);
private:
  // Inner window or sandbox.
  nsIGlobalObject*           mParentObject;
  // mParentObject pre QI-ed and cached
  // (it is needed for off main thread access)
  nsPIDOMWindow*             mOwnerWindow;
  bool                       mHasOrHasHadOwnerWindow;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsDOMEventTargetHelper,
                              NS_DOMEVENTTARGETHELPER_IID)

// XPIDL event handlers
#define NS_IMPL_EVENT_HANDLER(_class, _event)                                 \
    NS_IMETHODIMP _class::GetOn##_event(JSContext* aCx, JS::Value* aValue)    \
    {                                                                         \
      GetEventHandler(nsGkAtoms::on##_event, aCx, aValue);                    \
      return NS_OK;                                                           \
    }                                                                         \
    NS_IMETHODIMP _class::SetOn##_event(JSContext* aCx,                       \
                                        const JS::Value& aValue)              \
    {                                                                         \
      return SetEventHandler(nsGkAtoms::on##_event, aCx, aValue);             \
    }

#define NS_IMPL_FORWARD_EVENT_HANDLER(_class, _event, _baseclass)             \
    NS_IMETHODIMP _class::GetOn##_event(JSContext* aCx, JS::Value* aValue)    \
    {                                                                         \
      return _baseclass::GetOn##_event(aCx, aValue);                          \
    }                                                                         \
    NS_IMETHODIMP _class::SetOn##_event(JSContext* aCx,                       \
                                        const JS::Value& aValue)              \
    {                                                                         \
      return _baseclass::SetOn##_event(aCx, aValue);                          \
    }

// WebIDL event handlers
#define IMPL_EVENT_HANDLER(_event)                                        \
  inline mozilla::dom::EventHandlerNonNull* GetOn##_event()               \
  {                                                                       \
    return GetEventHandler(nsGkAtoms::on##_event);                        \
  }                                                                       \
  inline void SetOn##_event(mozilla::dom::EventHandlerNonNull* aCallback, \
                            ErrorResult& aRv)                             \
  {                                                                       \
    SetEventHandler(nsGkAtoms::on##_event, aCallback, aRv);               \
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
  virtual nsresult WillHandleEvent(nsEventChainPostVisitor & aVisitor) { \
    return _to WillHandleEvent(aVisitor); \
  } \
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor & aVisitor) { \
    return _to PostHandleEvent(aVisitor); \
  } \
  virtual nsresult DispatchDOMEvent(nsEvent *aEvent, nsIDOMEvent *aDOMEvent, nsPresContext *aPresContext, nsEventStatus *aEventStatus) { \
    return _to DispatchDOMEvent(aEvent, aDOMEvent, aPresContext, aEventStatus); \
  } \
  virtual nsEventListenerManager * GetListenerManager(bool aMayCreate) { \
    return _to GetListenerManager(aMayCreate); \
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
  NS_FORWARD_NSIDOMEVENTTARGET(_class::)

#endif // nsDOMEventTargetHelper_h_

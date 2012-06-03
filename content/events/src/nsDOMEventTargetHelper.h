/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMEventTargetHelper_h_
#define nsDOMEventTargetHelper_h_

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsEventListenerManager.h"
#include "nsIScriptContext.h"
#include "nsWrapperCache.h"
#include "mozilla/ErrorResult.h"

class nsDOMEventListenerWrapper : public nsIDOMEventListener
{
public:
  nsDOMEventListenerWrapper(nsIDOMEventListener* aListener)
  : mListener(aListener) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsDOMEventListenerWrapper)

  NS_DECL_NSIDOMEVENTLISTENER

  nsIDOMEventListener* GetInner() { return mListener; }
  void Disconnect() { mListener = nsnull; }
protected:
  nsCOMPtr<nsIDOMEventListener> mListener;
};

class nsDOMEventTargetHelper : public nsIDOMEventTarget,
                               public nsWrapperCache
{
public:
  nsDOMEventTargetHelper() : mOwner(nsnull), mHasOrHasHadOwner(false) {}
  virtual ~nsDOMEventTargetHelper();
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsDOMEventTargetHelper)

  NS_DECL_NSIDOMEVENTTARGET
  void AddEventListener(const nsAString& aType,
                        nsIDOMEventListener* aCallback, // XXX nullable
                        bool aCapture, Nullable<bool>& aWantsUntrusted,
                        mozilla::ErrorResult& aRv)
  {
    aRv = AddEventListener(aType, aCallback, aCapture,
                           !aWantsUntrusted.IsNull() && aWantsUntrusted.Value(),
                           aWantsUntrusted.IsNull() ? 1 : 2);
  }
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aCallback,
                           bool aCapture, mozilla::ErrorResult& aRv)
  {
    aRv = RemoveEventListener(aType, aCallback, aCapture);
  }
  bool DispatchEvent(nsIDOMEvent* aEvent, mozilla::ErrorResult& aRv)
  {
    bool result = false;
    aRv = DispatchEvent(aEvent, &result);
    return result;
  }

  void GetParentObject(nsIScriptGlobalObject **aParentObject)
  {
    if (mOwner) {
      CallQueryInterface(mOwner, aParentObject);
    }
    else {
      *aParentObject = nsnull;
    }
  }

  static nsDOMEventTargetHelper* FromSupports(nsISupports* aSupports)
  {
    nsIDOMEventTarget* target =
      static_cast<nsIDOMEventTarget*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMEventTarget> target_qi =
        do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMEventTarget pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(target_qi == target, "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMEventTargetHelper*>(target);
  }

  void Init(JSContext* aCx = nsnull);

  bool HasListenersFor(const nsAString& aType)
  {
    return mListenerManager && mListenerManager->HasListenersFor(aType);
  }
  nsresult RemoveAddEventListener(const nsAString& aType,
                                  nsRefPtr<nsDOMEventListenerWrapper>& aCurrent,
                                  nsIDOMEventListener* aNew);

  nsresult GetInnerEventListener(nsRefPtr<nsDOMEventListenerWrapper>& aWrapper,
                                 nsIDOMEventListener** aListener);

  nsresult CheckInnerWindowCorrectness()
  {
    NS_ENSURE_STATE(!mHasOrHasHadOwner || mOwner);
    if (mOwner) {
      NS_ASSERTION(mOwner->IsInnerWindow(), "Should have inner window here!\n");
      nsPIDOMWindow* outer = mOwner->GetOuterWindow();
      if (!outer || outer->GetCurrentInnerWindow() != mOwner) {
        return NS_ERROR_FAILURE;
      }
    }
    return NS_OK;
  }

  void BindToOwner(nsPIDOMWindow* aOwner);
  void BindToOwner(nsDOMEventTargetHelper* aOther);
  virtual void DisconnectFromOwner();                   
  nsPIDOMWindow* GetOwner() { return mOwner; }
  bool HasOrHasHadOwner() { return mHasOrHasHadOwner; }
protected:
  nsRefPtr<nsEventListenerManager> mListenerManager;
private:
  // These may be null (native callers or xpcshell).
  nsPIDOMWindow*             mOwner; // Inner window.
  bool                       mHasOrHasHadOwner;
};

#define NS_DECL_EVENT_HANDLER(_event)                                         \
  protected:                                                                  \
    nsRefPtr<nsDOMEventListenerWrapper> mOn##_event##Listener;                \
  public:

#define NS_DECL_AND_IMPL_EVENT_HANDLER(_event)                                \
  protected:                                                                  \
    nsRefPtr<nsDOMEventListenerWrapper> mOn##_event##Listener;                \
  public:                                                                     \
    NS_IMETHOD GetOn##_event(nsIDOMEventListener** a##_event)                 \
    {                                                                         \
      return GetInnerEventListener(mOn##_event##Listener, a##_event);         \
    }                                                                         \
    NS_IMETHOD SetOn##_event(nsIDOMEventListener* a##_event)                  \
    {                                                                         \
      return RemoveAddEventListener(NS_LITERAL_STRING(#_event),               \
                                    mOn##_event##Listener, a##_event);        \
    }

#define NS_IMPL_EVENT_HANDLER(_class, _event)                                 \
  NS_IMETHODIMP                                                               \
  _class::GetOn##_event(nsIDOMEventListener** a##_event)                      \
  {                                                                           \
    return GetInnerEventListener(mOn##_event##Listener, a##_event);           \
  }                                                                           \
  NS_IMETHODIMP                                                               \
  _class::SetOn##_event(nsIDOMEventListener* a##_event)                       \
  {                                                                           \
    return RemoveAddEventListener(NS_LITERAL_STRING(#_event),                 \
                                  mOn##_event##Listener, a##_event);          \
  }

#define NS_IMPL_FORWARD_EVENT_HANDLER(_class, _event, _baseclass)             \
    NS_IMETHODIMP                                                             \
    _class::GetOn##_event(nsIDOMEventListener** a##_event)                    \
    {                                                                         \
      return _baseclass::GetOn##_event(a##_event);                           \
    }                                                                         \
    NS_IMETHODIMP                                                             \
    _class::SetOn##_event(nsIDOMEventListener* a##_event)                     \
    {                                                                         \
      return _baseclass::SetOn##_event(a##_event);                            \
    }

#define NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(_event)                    \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOn##_event##Listener)

#define NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(_event)                      \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOn##_event##Listener)

#define NS_DISCONNECT_EVENT_HANDLER(_event)                                   \
  if (mOn##_event##Listener) { mOn##_event##Listener->Disconnect(); }

#define NS_UNMARK_LISTENER_WRAPPER(_event)                                    \
  if (tmp->mOn##_event##Listener) {                                           \
    xpc_TryUnmarkWrappedGrayObject(tmp->mOn##_event##Listener->GetInner());   \
  }

/* Use this macro to declare functions that forward the behavior of this
 * interface to another object.
 * This macro doesn't forward PreHandleEvent because sometimes subclasses
 * want to override it.
 */
#define NS_FORWARD_NSIDOMEVENTTARGET_NOPREHANDLEEVENT(_to) \
  NS_SCRIPTABLE NS_IMETHOD AddEventListener(const nsAString & type, nsIDOMEventListener *listener, bool useCapture, bool wantsUntrusted, PRUint8 _argc) { \
    return _to AddEventListener(type, listener, useCapture, wantsUntrusted, _argc); \
  } \
  NS_IMETHOD AddSystemEventListener(const nsAString & type, nsIDOMEventListener *listener, bool aUseCapture, bool aWantsUntrusted, PRUint8 _argc) { \
    return _to AddSystemEventListener(type, listener, aUseCapture, aWantsUntrusted, _argc); \
  } \
  NS_SCRIPTABLE NS_IMETHOD RemoveEventListener(const nsAString & type, nsIDOMEventListener *listener, bool useCapture) { \
    return _to RemoveEventListener(type, listener, useCapture); \
  } \
  NS_IMETHOD RemoveSystemEventListener(const nsAString & type, nsIDOMEventListener *listener, bool aUseCapture) { \
    return _to RemoveSystemEventListener(type, listener, aUseCapture); \
  } \
  NS_SCRIPTABLE NS_IMETHOD DispatchEvent(nsIDOMEvent *evt, bool *_retval NS_OUTPARAM) { \
    return _to DispatchEvent(evt, _retval); \
  } \
  virtual nsIDOMEventTarget * GetTargetForDOMEvent(void) { \
    return _to GetTargetForDOMEvent(); \
  } \
  virtual nsIDOMEventTarget * GetTargetForEventTargetChain(void) { \
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
  virtual nsIScriptContext * GetContextForEventHandlers(nsresult *aRv NS_OUTPARAM) { \
    return _to GetContextForEventHandlers(aRv); \
  } \
  virtual JSContext * GetJSContextForEventHandlers(void) { \
    return _to GetJSContextForEventHandlers(); \
  } 

#endif // nsDOMEventTargetHelper_h_

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEventListenerManager_h__
#define nsEventListenerManager_h__

#include "nsEventListenerManager.h"
#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsIScriptContext.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTObserverArray.h"
#include "nsGUIEvent.h"
#include "nsIJSEventListener.h"

class nsIDOMEvent;
class nsIAtom;
class nsIWidget;
struct nsPoint;
struct EventTypeData;
class nsEventTargetChainItem;
class nsPIDOMWindow;
class nsCxPusher;
class nsIEventListenerInfo;

typedef enum
{
    eNativeListener = 0,
    eJSEventListener,
    eWrappedJSListener
} nsListenerType;

struct nsListenerStruct
{
  nsRefPtr<nsIDOMEventListener> mListener;
  PRUint32                      mEventType;
  nsCOMPtr<nsIAtom>             mTypeAtom;
  PRUint16                      mFlags;
  PRUint8                       mListenerType;
  bool                          mHandlerIsString;

  nsIJSEventListener* GetJSListener() const {
    return (mListenerType == eJSEventListener) ?
      static_cast<nsIJSEventListener *>(mListener.get()) : nsnull;
  }

  ~nsListenerStruct()
  {
    if ((mListenerType == eJSEventListener) && mListener) {
      static_cast<nsIJSEventListener*>(mListener.get())->Disconnect();
    }
  }
};

/*
 * Event listener manager
 */

class nsEventListenerManager
{

public:
  nsEventListenerManager(nsISupports* aTarget);
  virtual ~nsEventListenerManager();

  NS_INLINE_DECL_REFCOUNTING(nsEventListenerManager)

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsEventListenerManager)

  void AddEventListener(const nsAString& aType,
                        nsIDOMEventListener* aListener,
                        bool aUseCapture,
                        bool aWantsUntrusted);
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener,
                           bool aUseCapture);

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */
  void AddEventListenerByType(nsIDOMEventListener *aListener,
                              const nsAString& type,
                              PRInt32 aFlags);
  void RemoveEventListenerByType(nsIDOMEventListener *aListener,
                                 const nsAString& type,
                                 PRInt32 aFlags);

  /**
   * Sets the current "inline" event listener for aName to be a
   * function compiled from aFunc if !aDeferCompilation.  If
   * aDeferCompilation, then we assume that we can get the string from
   * mTarget later and compile lazily.
   */
  // XXXbz does that play correctly with nodes being adopted across
  // documents?  Need to double-check the spec here.
  nsresult AddScriptEventListener(nsIAtom *aName,
                                  const nsAString& aFunc,
                                  PRUint32 aLanguage,
                                  bool aDeferCompilation,
                                  bool aPermitUntrustedEvents);
  /**
   * Remove the current "inline" event listener for aName.
   */
  void RemoveScriptEventListener(nsIAtom *aName);

  void HandleEvent(nsPresContext* aPresContext,
                   nsEvent* aEvent, 
                   nsIDOMEvent** aDOMEvent,
                   nsIDOMEventTarget* aCurrentTarget,
                   PRUint32 aFlags,
                   nsEventStatus* aEventStatus,
                   nsCxPusher* aPusher)
  {
    if (mListeners.IsEmpty() || aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) {
      return;
    }

    if (!mMayHaveCapturingListeners &&
        !(aEvent->flags & NS_EVENT_FLAG_BUBBLE)) {
      return;
    }

    if (!mMayHaveSystemGroupListeners &&
        aFlags & NS_EVENT_FLAG_SYSTEM_EVENT) {
      return;
    }

    // Check if we already know that there is no event listener for the event.
    if (mNoListenerForEvent == aEvent->message &&
        (mNoListenerForEvent != NS_USER_DEFINED_EVENT ||
         mNoListenerForEventAtom == aEvent->userType)) {
      return;
    }
    HandleEventInternal(aPresContext, aEvent, aDOMEvent, aCurrentTarget,
                        aFlags, aEventStatus, aPusher);
  }

  void HandleEventInternal(nsPresContext* aPresContext,
                           nsEvent* aEvent, 
                           nsIDOMEvent** aDOMEvent,
                           nsIDOMEventTarget* aCurrentTarget,
                           PRUint32 aFlags,
                           nsEventStatus* aEventStatus,
                           nsCxPusher* aPusher);

  /**
   * Tells the event listener manager that its target (which owns it) is
   * no longer using it (and could go away).
   */
  void Disconnect();

  /**
   * Allows us to quickly determine if we have mutation listeners registered.
   */
  bool HasMutationListeners();

  /**
   * Allows us to quickly determine whether we have unload or beforeunload
   * listeners registered.
   */
  bool HasUnloadListeners();

  /**
   * Returns the mutation bits depending on which mutation listeners are
   * registered to this listener manager.
   * @note If a listener is an nsIDOMMutationListener, all possible mutation
   *       event bits are returned. All bits are also returned if one of the
   *       event listeners is registered to handle DOMSubtreeModified events.
   */
  PRUint32 MutationListenerBits();

  /**
   * Returns true if there is at least one event listener for aEventName.
   */
  bool HasListenersFor(const nsAString& aEventName);

  /**
   * Returns true if there is at least one event listener.
   */
  bool HasListeners();

  /**
   * Sets aList to the list of nsIEventListenerInfo objects representing the
   * listeners managed by this listener manager.
   */
  nsresult GetListenerInfo(nsCOMArray<nsIEventListenerInfo>* aList);

  PRUint32 GetIdentifierForEvent(nsIAtom* aEvent);

  static void Shutdown();

  /**
   * Returns true if there may be a paint event listener registered,
   * false if there definitely isn't.
   */
  bool MayHavePaintEventListener() { return mMayHavePaintEventListener; }

  /**
   * Returns true if there may be a MozAudioAvailable event listener registered,
   * false if there definitely isn't.
   */
  bool MayHaveAudioAvailableEventListener() { return mMayHaveAudioAvailableEventListener; }

  /**
   * Returns true if there may be a touch event listener registered,
   * false if there definitely isn't.
   */
  bool MayHaveTouchEventListener() { return mMayHaveTouchEventListener; }

  bool MayHaveMouseEnterLeaveEventListener() { return mMayHaveMouseEnterLeaveEventListener; }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  void UnmarkGrayJSListeners();

  nsISupports* GetTarget() { return mTarget; }
protected:
  nsresult HandleEventSubType(nsListenerStruct* aListenerStruct,
                              nsIDOMEventListener* aListener,
                              nsIDOMEvent* aDOMEvent,
                              nsIDOMEventTarget* aCurrentTarget,
                              PRUint32 aPhaseFlags,
                              nsCxPusher* aPusher);

  /**
   * Compile the "inline" event listener for aListenerStruct.  The
   * body of the listener can be provided in aBody; if this is null we
   * will look for it on mTarget.
   */
  nsresult CompileEventHandlerInternal(nsListenerStruct *aListenerStruct,
                                       bool aNeedsCxPush,
                                       const nsAString* aBody);

  /**
   * Find the nsListenerStruct for the "inline" event listener for aTypeAtom.
   */
  nsListenerStruct* FindJSEventListener(PRUint32 aEventType, nsIAtom* aTypeAtom);

  /**
   * Set the "inline" event listener for aName to aHandler.  aHandler
   * may be null to indicate that we should lazily get and compile the
   * string for this listener.  The nsListenerStruct that results, if
   * any, is returned in aListenerStruct.
   */
  nsresult SetJSEventListener(nsIScriptContext *aContext,
                              JSObject* aScopeGlobal,
                              nsIAtom* aName,
                              JSObject *aHandler,
                              bool aPermitUntrustedEvents,
                              nsListenerStruct **aListenerStruct);

  bool IsDeviceType(PRUint32 aType);
  void EnableDevice(PRUint32 aType);
  void DisableDevice(PRUint32 aType);

public:
  /**
   * Set the "inline" event listener for aEventName to |v|.  This
   * might actually remove the event listener, depending on the value
   * of |v|.  Note that on entry to this function cx and aScope might
   * not be in the same compartment, though cx and v are guaranteed to
   * be in the same compartment.
   */
  nsresult SetJSEventListenerToJsval(nsIAtom *aEventName, JSContext *cx,
                                     JSObject *aScope, const jsval &v);
  /**
   * Get the value of the "inline" event listener for aEventName.
   * This may cause lazy compilation if the listener is uncompiled.
   */
  void GetJSEventListener(nsIAtom *aEventName, jsval *vp);

protected:
  void AddEventListener(nsIDOMEventListener *aListener, 
                        PRUint32 aType,
                        nsIAtom* aTypeAtom,
                        PRInt32 aFlags);
  void RemoveEventListener(nsIDOMEventListener *aListener,
                           PRUint32 aType,
                           nsIAtom* aUserType,
                           PRInt32 aFlags);
  void RemoveAllListeners();
  const EventTypeData* GetTypeDataForIID(const nsIID& aIID);
  const EventTypeData* GetTypeDataForEventName(nsIAtom* aName);
  nsPIDOMWindow* GetInnerWindowForTarget();

  PRUint32 mMayHavePaintEventListener : 1;
  PRUint32 mMayHaveMutationListeners : 1;
  PRUint32 mMayHaveCapturingListeners : 1;
  PRUint32 mMayHaveSystemGroupListeners : 1;
  PRUint32 mMayHaveAudioAvailableEventListener : 1;
  PRUint32 mMayHaveTouchEventListener : 1;
  PRUint32 mMayHaveMouseEnterLeaveEventListener : 1;
  PRUint32 mNoListenerForEvent : 25;

  nsAutoTObserverArray<nsListenerStruct, 2> mListeners;
  nsISupports*                              mTarget;  //WEAK
  nsCOMPtr<nsIAtom>                         mNoListenerForEventAtom;

  static PRUint32                           mInstanceCount;
  static jsid                               sAddListenerID;

  friend class nsEventTargetChainItem;
  static PRUint32                           sCreatedCount;
};

/**
 * NS_AddSystemEventListener() is a helper function for implementing
 * nsIDOMEventTarget::AddSystemEventListener().
 */
inline nsresult
NS_AddSystemEventListener(nsIDOMEventTarget* aTarget,
                          const nsAString& aType,
                          nsIDOMEventListener *aListener,
                          bool aUseCapture,
                          bool aWantsUntrusted)
{
  nsEventListenerManager* listenerManager = aTarget->GetListenerManager(true);
  NS_ENSURE_STATE(listenerManager);
  PRUint32 flags = NS_EVENT_FLAG_SYSTEM_EVENT;
  flags |= aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }
  listenerManager->AddEventListenerByType(aListener, aType, flags);
  return NS_OK;
}

#endif // nsEventListenerManager_h__

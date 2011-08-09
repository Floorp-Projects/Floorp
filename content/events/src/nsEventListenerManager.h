/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

class nsIDOMEvent;
class nsIAtom;
class nsIWidget;
struct nsPoint;
struct EventTypeData;
class nsEventTargetChainItem;
class nsPIDOMWindow;
class nsCxPusher;
class nsIEventListenerInfo;

typedef struct {
  nsRefPtr<nsIDOMEventListener> mListener;
  PRUint32                      mEventType;
  nsCOMPtr<nsIAtom>             mTypeAtom;
  PRUint16                      mFlags;
  PRBool                        mHandlerIsString;
} nsListenerStruct;

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

  nsresult AddEventListener(const nsAString& aType,
                            nsIDOMEventListener* aListener,
                            PRBool aUseCapture,
                            PRBool aWantsUntrusted);
  void RemoveEventListener(const nsAString& aType,
                           nsIDOMEventListener* aListener,
                           PRBool aUseCapture);

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */
  nsresult AddEventListenerByType(nsIDOMEventListener *aListener,
                                  const nsAString& type,
                                  PRInt32 aFlags);
  void RemoveEventListenerByType(nsIDOMEventListener *aListener,
                                 const nsAString& type,
                                 PRInt32 aFlags);
  nsresult AddScriptEventListener(nsIAtom *aName,
                                  const nsAString& aFunc,
                                  PRUint32 aLanguage,
                                  PRBool aDeferCompilation,
                                  PRBool aPermitUntrustedEvents);
  nsresult RegisterScriptEventListener(nsIScriptContext *aContext,
                                       void *aScopeObject,
                                       nsIAtom* aName);
  void RemoveScriptEventListener(nsIAtom *aName);
  nsresult CompileScriptEventListener(nsIScriptContext *aContext,
                                      void *aScopeObject,
                                      nsIAtom* aName, PRBool *aDidCompile);

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

  void Disconnect();

  PRBool HasMutationListeners();

  PRBool HasUnloadListeners();

  PRUint32 MutationListenerBits();

  PRBool HasListenersFor(const nsAString& aEventName);

  PRBool HasListeners();

  nsresult GetListenerInfo(nsCOMArray<nsIEventListenerInfo>* aList);

  PRUint32 GetIdentifierForEvent(nsIAtom* aEvent);

  static void Shutdown();

  /**
   * Returns PR_TRUE if there may be a paint event listener registered,
   * PR_FALSE if there definitely isn't.
   */
  PRBool MayHavePaintEventListener() { return mMayHavePaintEventListener; }

  /**
   * Returns PR_TRUE if there may be a MozAudioAvailable event listener registered,
   * PR_FALSE if there definitely isn't.
   */
  PRBool MayHaveAudioAvailableEventListener() { return mMayHaveAudioAvailableEventListener; }

  /**
   * Returns PR_TRUE if there may be a touch event listener registered,
   * PR_FALSE if there definitely isn't.
   */
  PRBool MayHaveTouchEventListener() { return mMayHaveTouchEventListener; }

protected:
  nsresult HandleEventSubType(nsListenerStruct* aListenerStruct,
                              nsIDOMEventListener* aListener,
                              nsIDOMEvent* aDOMEvent,
                              nsIDOMEventTarget* aCurrentTarget,
                              PRUint32 aPhaseFlags,
                              nsCxPusher* aPusher);
  nsresult CompileEventHandlerInternal(nsIScriptContext *aContext,
                                       void *aScopeObject,
                                       nsISupports *aObject,
                                       nsIAtom *aName,
                                       nsListenerStruct *aListenerStruct,
                                       nsISupports* aCurrentTarget,
                                       PRBool aNeedsCxPush);
  nsListenerStruct* FindJSEventListener(PRUint32 aEventType, nsIAtom* aTypeAtom);
  nsresult SetJSEventListener(nsIScriptContext *aContext,
                              void *aScopeGlobal,
                              nsIAtom* aName, PRBool aIsString,
                              PRBool aPermitUntrustedEvents);
  nsresult AddEventListener(nsIDOMEventListener *aListener, 
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
  PRUint32 mNoListenerForEvent : 26;

  nsAutoTObserverArray<nsListenerStruct, 2> mListeners;
  nsISupports*                              mTarget;  //WEAK
  nsCOMPtr<nsIAtom>                         mNoListenerForEventAtom;

  static PRUint32                           mInstanceCount;
  static jsid                               sAddListenerID;

  friend class nsEventTargetChainItem;
  static PRUint32                           sCreatedCount;
};

#endif // nsEventListenerManager_h__

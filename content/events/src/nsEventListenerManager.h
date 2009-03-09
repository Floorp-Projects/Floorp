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

#include "nsIEventListenerManager.h"
#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOM3EventTarget.h"
#include "nsHashtable.h"
#include "nsIScriptContext.h"
#include "nsCycleCollectionParticipant.h"

class nsIDOMEvent;
class nsIAtom;
class nsIWidget;
struct nsPoint;
struct EventTypeData;
class nsEventTargetChainItem;

typedef struct {
  nsRefPtr<nsIDOMEventListener> mListener;
  PRUint32                      mEventType;
  nsCOMPtr<nsIAtom>             mTypeAtom;
  PRUint16                      mFlags;
  PRUint16                      mGroupFlags;
  PRBool                        mHandlerIsString;
  const EventTypeData*          mTypeData;
} nsListenerStruct;

/*
 * Event listener manager
 */

class nsEventListenerManager : public nsIEventListenerManager,
                               public nsIDOMEventTarget,
                               public nsIDOM3EventTarget
{

public:
  nsEventListenerManager();
  virtual ~nsEventListenerManager();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID, PRInt32 aFlags);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID, PRInt32 aFlags);
  NS_IMETHOD AddEventListenerByType(nsIDOMEventListener *aListener,
                                    const nsAString& type,
                                    PRInt32 aFlags,
                                    nsIDOMEventGroup* aEvtGroup);
  NS_IMETHOD RemoveEventListenerByType(nsIDOMEventListener *aListener,
                                       const nsAString& type,
                                       PRInt32 aFlags,
                                       nsIDOMEventGroup* aEvtGroup);
  NS_IMETHOD AddScriptEventListener(nsISupports *aObject,
                                    nsIAtom *aName,
                                    const nsAString& aFunc,
                                    PRUint32 aLanguage,
                                    PRBool aDeferCompilation,
                                    PRBool aPermitUntrustedEvents);
  NS_IMETHOD RegisterScriptEventListener(nsIScriptContext *aContext,
                                         void *aScopeObject,
                                         nsISupports *aObject,
                                         nsIAtom* aName);
  NS_IMETHOD RemoveScriptEventListener(nsIAtom *aName);
  NS_IMETHOD CompileScriptEventListener(nsIScriptContext *aContext,
                                        void *aScopeObject,
                                        nsISupports *aObject,
                                        nsIAtom* aName, PRBool *aDidCompile);

  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsEvent* aEvent, 
                         nsIDOMEvent** aDOMEvent,
                         nsPIDOMEventTarget* aCurrentTarget,
                         PRUint32 aFlags,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD Disconnect();

  NS_IMETHOD SetListenerTarget(nsISupports* aTarget);

  NS_IMETHOD HasMutationListeners(PRBool* aListener);

  NS_IMETHOD GetSystemEventGroupLM(nsIDOMEventGroup** aGroup);

  virtual PRBool HasUnloadListeners();

  virtual PRUint32 MutationListenerBits();

  virtual PRBool HasListenersFor(const nsAString& aEventName);

  virtual PRBool HasListeners();

  static PRUint32 GetIdentifierForEvent(nsIAtom* aEvent);

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  static void Shutdown();

  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsEventListenerManager,
                                           nsIEventListenerManager)

protected:
  nsresult HandleEventSubType(nsListenerStruct* aListenerStruct,
                              nsIDOMEventListener* aListener,
                              nsIDOMEvent* aDOMEvent,
                              nsPIDOMEventTarget* aCurrentTarget,
                              PRUint32 aPhaseFlags);
  nsresult CompileEventHandlerInternal(nsIScriptContext *aContext,
                                       void *aScopeObject,
                                       nsISupports *aObject,
                                       nsIAtom *aName,
                                       nsListenerStruct *aListenerStruct,
                                       nsISupports* aCurrentTarget);
  nsListenerStruct* FindJSEventListener(PRUint32 aEventType, nsIAtom* aTypeAtom);
  nsresult SetJSEventListener(nsIScriptContext *aContext,
                              void *aScopeGlobal,
                              nsISupports *aObject,
                              nsIAtom* aName, PRBool aIsString,
                              PRBool aPermitUntrustedEvents);
  nsresult AddEventListener(nsIDOMEventListener *aListener, 
                            PRUint32 aType,
                            nsIAtom* aTypeAtom,
                            const EventTypeData* aTypeData,
                            PRInt32 aFlags,
                            nsIDOMEventGroup* aEvtGrp);
  nsresult RemoveEventListener(nsIDOMEventListener *aListener,
                               PRUint32 aType,
                               nsIAtom* aUserType,
                               const EventTypeData* aTypeData,
                               PRInt32 aFlags,
                               nsIDOMEventGroup* aEvtGrp);
  nsresult RemoveAllListeners();
  const EventTypeData* GetTypeDataForIID(const nsIID& aIID);
  const EventTypeData* GetTypeDataForEventName(nsIAtom* aName);
  nsresult GetDOM2EventGroup(nsIDOMEventGroup** aGroup);
  PRBool ListenerCanHandle(nsListenerStruct* aLs, nsEvent* aEvent);
  nsPIDOMWindow* GetInnerWindowForTarget();

  nsAutoTObserverArray<nsListenerStruct, 2> mListeners;
  nsISupports*                              mTarget;  //WEAK
  nsCOMPtr<nsIAtom>                         mNoListenerForEventAtom;

  static PRUint32                           mInstanceCount;
  static jsval                              sAddListenerID;

  friend class nsEventTargetChainItem;
  static PRUint32                           sCreatedCount;
};

#endif // nsEventListenerManager_h__

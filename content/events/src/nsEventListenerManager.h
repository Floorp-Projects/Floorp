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
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsHashtable.h"
#include "nsIScriptContext.h"

class nsIDOMEvent;
class nsIAtom;

typedef struct {
  nsIDOMEventListener* mListener;
  PRUint8 mFlags;
  PRUint8 mSubType;
  PRUint8 mHandlerIsString;
  PRUint8 mSubTypeCapture;
  PRUint16 mGroupFlags;
} nsListenerStruct;

//These define the internal type of the EventListenerManager
//No listener type defined, should happen only at creation
#define NS_ELM_NONE   0
//Simple indicates only a single event listener group type (i.e. mouse, key) 
#define NS_ELM_SINGLE 1
//Multi indicates any number of listener group types accessed as member vars
#define NS_ELM_MULTI  2
//Hash indicates any number of listener group types accessed out of a hash
#define NS_ELM_HASH   4

enum EventArrayType {
  eEventArrayType_Mouse = 0,
  eEventArrayType_MouseMotion = 1,
  eEventArrayType_ContextMenu = 2,
  eEventArrayType_Key = 3,
  eEventArrayType_Load = 4,
  eEventArrayType_Focus = 5,
  eEventArrayType_Form = 6,
  eEventArrayType_Drag = 7,
  eEventArrayType_Paint = 8,
  eEventArrayType_Text = 9,
  eEventArrayType_Composition = 10,
  eEventArrayType_XUL = 11,
  eEventArrayType_Scroll = 12,
  eEventArrayType_Mutation = 13,
  eEventArrayType_DOMUI = 14,
  eEventArrayType_Hash,
  eEventArrayType_None
};

//Keep this in line with event array types, not counting
//types HASH and NONE
#define EVENT_ARRAY_TYPE_LENGTH eEventArrayType_Hash

/*
 * Event listener manager
 */

class nsEventListenerManager : public nsIEventListenerManager,
                               public nsIDOMEventReceiver,
                               public nsIDOM3EventTarget
{

public:
  nsEventListenerManager();
  virtual ~nsEventListenerManager();

  NS_DECL_ISUPPORTS

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
                                    PRBool aDeferCompilation); 
  NS_IMETHOD RegisterScriptEventListener(nsIScriptContext *aContext,
                                         nsISupports *aObject,
                                         nsIAtom* aName);
  NS_IMETHOD RemoveScriptEventListener(nsIAtom *aName);
  NS_IMETHOD CompileScriptEventListener(nsIScriptContext *aContext,
                                        nsISupports *aObject,
                                        nsIAtom* aName, PRBool *aDidCompile);

  NS_IMETHOD CaptureEvent(PRInt32 aEventTypes);
  NS_IMETHOD ReleaseEvent(PRInt32 aEventTypes);

  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsEvent* aEvent, 
                         nsIDOMEvent** aDOMEvent,
                         nsIDOMEventTarget* aCurrentTarget,
                         PRUint32 aFlags,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD CreateEvent(nsIPresContext* aPresContext, 
                         nsEvent* aEvent,
                         const nsAString& aEventType,
                         nsIDOMEvent** aDOMEvent);

  NS_IMETHOD RemoveAllListeners(PRBool aScriptOnly);

  NS_IMETHOD SetListenerTarget(nsISupports* aTarget);

  NS_IMETHOD HasMutationListeners(PRBool* aListener)
  {
    *aListener = (GetListenersByType(eEventArrayType_Mutation, nsnull,
                                     PR_FALSE) != nsnull);
    return NS_OK;
  }

  NS_IMETHOD GetSystemEventGroupLM(nsIDOMEventGroup** aGroup);

  static nsresult GetIdentifiersForType(nsIAtom* aType,
                                        EventArrayType* aArrayType,
                                        PRInt32* aSubType);

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIDOMEventReceiver interface
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  static void Shutdown();

protected:
  nsresult HandleEventSubType(nsListenerStruct* aListenerStruct,
                              nsIDOMEvent* aDOMEvent,
                              nsIDOMEventTarget* aCurrentTarget,
                              PRUint32 aSubType,
                              PRUint32 aPhaseFlags);
  nsresult CompileEventHandlerInternal(nsIScriptContext *aContext,
                                       nsISupports *aObject,
                                       nsIAtom *aName,
                                       nsListenerStruct *aListenerStruct,
                                       nsIDOMEventTarget* aCurrentTarget,
                                       PRUint32 aSubType);
  nsListenerStruct* FindJSEventListener(EventArrayType aType);
  nsresult SetJSEventListener(nsIScriptContext *aContext,
                              nsISupports *aObject, nsIAtom* aName,
                              PRBool aIsString);
  nsresult AddEventListener(nsIDOMEventListener *aListener, 
                            EventArrayType aType, 
                            PRInt32 aSubType,
                            nsHashKey* aKey,
                            PRInt32 aFlags,
                            nsIDOMEventGroup* aEvtGrp);
  nsresult RemoveEventListener(nsIDOMEventListener *aListener,
                               EventArrayType aType,
                               PRInt32 aSubType,
                               nsHashKey* aKey,
                               PRInt32 aFlags,
                               nsIDOMEventGroup* aEvtGrp);
  void ReleaseListeners(nsVoidArray** aListeners, PRBool aScriptOnly);
  nsresult FlipCaptureBit(PRInt32 aEventTypes, PRBool aInitCapture);
  nsVoidArray* GetListenersByType(EventArrayType aType, nsHashKey* aKey, PRBool aCreate);
  EventArrayType GetTypeForIID(const nsIID& aIID);
  nsresult FixContextMenuEvent(nsIPresContext* aPresContext,
                               nsIDOMEventTarget* aCurrentTarget,
                               nsEvent* aEvent,
                               nsIDOMEvent** aDOMEvent);
  void GetCoordinatesFor(nsIDOMElement *aCurrentEl, nsIPresContext *aPresContext,
                         nsIPresShell *aPresShell, nsPoint& aTargetPt);
  nsresult GetDOM2EventGroup(nsIDOMEventGroup** aGroup);

  PRUint8 mManagerType;
  PRPackedBool mListenersRemoved;

  EventArrayType mSingleListenerType;
  nsVoidArray* mSingleListener;
  nsVoidArray* mMultiListeners;
  nsHashtable* mGenericListeners;
  static PRUint32 mInstanceCount;

  nsISupports* mTarget;  //WEAK

  static jsval sAddListenerID;
};


//Set of defines for distinguishing event handlers within listener groupings
//XXX Current usage allows no more than 7 types per listener grouping

#define NS_EVENT_BITS_NONE    0x00

//nsIDOMMouseListener
#define NS_EVENT_BITS_MOUSE_NONE        0x00
#define NS_EVENT_BITS_MOUSE_MOUSEDOWN   0x01
#define NS_EVENT_BITS_MOUSE_MOUSEUP     0x02
#define NS_EVENT_BITS_MOUSE_CLICK       0x04
#define NS_EVENT_BITS_MOUSE_DBLCLICK    0x08
#define NS_EVENT_BITS_MOUSE_MOUSEOVER   0x10
#define NS_EVENT_BITS_MOUSE_MOUSEOUT    0x20

//nsIDOMMouseMotionListener
#define NS_EVENT_BITS_MOUSEMOTION_NONE        0x00
#define NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE   0x01
#define NS_EVENT_BITS_MOUSEMOTION_DRAGMOVE    0x02

//nsIDOMContextMenuListener
#define NS_EVENT_BITS_CONTEXTMENU_NONE   0x00
#define NS_EVENT_BITS_CONTEXTMENU        0x01

//nsIDOMKeyListener
#define NS_EVENT_BITS_KEY_NONE      0x00
#define NS_EVENT_BITS_KEY_KEYDOWN   0x01
#define NS_EVENT_BITS_KEY_KEYUP     0x02
#define NS_EVENT_BITS_KEY_KEYPRESS  0x04

//nsIDOMTextListener
#define NS_EVENT_BITS_TEXT_NONE      0x00
#define NS_EVENT_BITS_TEXT_TEXT      0x01

//nsIDOMCompositionListener
#define NS_EVENT_BITS_COMPOSITION_NONE      0x00
#define NS_EVENT_BITS_COMPOSITION_START     0x01
#define NS_EVENT_BITS_COMPOSITION_END		0x02
#define NS_EVENT_BITS_COMPOSITION_QUERY		0x04
#define NS_EVENT_BITS_COMPOSITION_RECONVERSION 0x08

//nsIDOMFocusListener
#define NS_EVENT_BITS_FOCUS_NONE    0x00
#define NS_EVENT_BITS_FOCUS_FOCUS   0x01
#define NS_EVENT_BITS_FOCUS_BLUR    0x02

//nsIDOMFormListener
#define NS_EVENT_BITS_FORM_NONE     0x00
#define NS_EVENT_BITS_FORM_SUBMIT   0x01
#define NS_EVENT_BITS_FORM_RESET    0x02
#define NS_EVENT_BITS_FORM_CHANGE   0x04
#define NS_EVENT_BITS_FORM_SELECT   0x08
#define NS_EVENT_BITS_FORM_INPUT    0x10

//nsIDOMLoadListener
#define NS_EVENT_BITS_LOAD_NONE              0x00
#define NS_EVENT_BITS_LOAD_LOAD              0x01
#define NS_EVENT_BITS_LOAD_UNLOAD            0x02
#define NS_EVENT_BITS_LOAD_ABORT             0x04
#define NS_EVENT_BITS_LOAD_ERROR             0x08
#define NS_EVENT_BITS_LOAD_BEFORE_UNLOAD     0x10

//nsIDOMXULListener
#define NS_EVENT_BITS_XUL_NONE               0x00
#define NS_EVENT_BITS_XUL_POPUP_SHOWING      0x01
#define NS_EVENT_BITS_XUL_CLOSE              0x02
#define NS_EVENT_BITS_XUL_POPUP_HIDING       0x04
#define NS_EVENT_BITS_XUL_COMMAND            0x08
#define NS_EVENT_BITS_XUL_BROADCAST          0x10
#define NS_EVENT_BITS_XUL_COMMAND_UPDATE     0x20
#define NS_EVENT_BITS_XUL_POPUP_SHOWN        0x40
#define NS_EVENT_BITS_XUL_POPUP_HIDDEN       0x80

//nsIScrollListener
#define NS_EVENT_BITS_SCROLLPORT_NONE             0x00
#define NS_EVENT_BITS_SCROLLPORT_OVERFLOW         0x01
#define NS_EVENT_BITS_SCROLLPORT_UNDERFLOW        0x02
#define NS_EVENT_BITS_SCROLLPORT_OVERFLOWCHANGED  0x04

//nsIDOMDragListener
#define NS_EVENT_BITS_DRAG_NONE     0x00
#define NS_EVENT_BITS_DRAG_ENTER    0x01
#define NS_EVENT_BITS_DRAG_OVER     0x02
#define NS_EVENT_BITS_DRAG_EXIT     0x04
#define NS_EVENT_BITS_DRAG_DROP     0x08
#define NS_EVENT_BITS_DRAG_GESTURE  0x10

//nsIDOMPaintListener
#define NS_EVENT_BITS_PAINT_NONE    0x00
#define NS_EVENT_BITS_PAINT_PAINT   0x01
#define NS_EVENT_BITS_PAINT_RESIZE  0x02
#define NS_EVENT_BITS_PAINT_SCROLL  0x04

//nsIDOMMutationListener
// These bits are found in nsMutationEvent.h.

//nsIDOMContextMenuListener
#define NS_EVENT_BITS_CONTEXT_NONE  0x00
#define NS_EVENT_BITS_CONTEXT_MENU  0x01

// nsIDOMUIListener
#define NS_EVENT_BITS_DOMUI_NONE      0x00
#define NS_EVENT_BITS_DOMUI_ACTIVATE  0x01
#define NS_EVENT_BITS_DOMUI_FOCUSIN   0x02
#define NS_EVENT_BITS_DOMUI_FOCUSOUT  0x04

#endif // nsEventListenerManager_h__

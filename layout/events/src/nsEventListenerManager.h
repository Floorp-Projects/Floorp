/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsEventListenerManager_h__
#define nsEventListenerManager_h__

#include "nsIEventListenerManager.h"
#include "jsapi.h"

class nsIDOMEvent;

typedef struct {
  nsIDOMEventListener* mListener;
  PRUint8 mFlags;
  PRUint8 mSubType;
} nsListenerStruct;

//Flag must live higher than all event flags in nsGUIEvent.h
#define NS_PRIV_EVENT_FLAG_SCRIPT 0x10

/*
 * Event listener manager
 */

class nsEventListenerManager : public nsIEventListenerManager {

public:
  nsEventListenerManager();
  virtual ~nsEventListenerManager();

  NS_DECL_ISUPPORTS

  nsVoidArray** GetListenersByIID(const nsIID& aIID);
  
  void ReleaseListeners(nsVoidArray* aListeners);

  /**
  * Retrieves events listeners of all types. 
  * @param
  */

  virtual nsresult GetEventListeners(nsVoidArray **aListeners, const nsIID& aIID);

  /**
  * Sets events listeners of all types. 
  * @param an event listener
  */

  virtual nsresult AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID, PRInt32 aFlags);
  virtual nsresult RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID, PRInt32 aFlags);
  virtual nsresult AddEventListenerByType(nsIDOMEventListener *aListener, const nsString& type, PRInt32 aFlags);
  virtual nsresult RemoveEventListenerByType(nsIDOMEventListener *aListener, const nsString& type, PRInt32 aFlags) ;

  virtual nsresult AddScriptEventListener(nsIScriptContext* aContext, 
                                          nsIScriptObjectOwner *aScriptObjectOwner, 
                                          nsIAtom *aName, 
                                          const nsString& aFunc, 
                                          const nsIID& aIID);
  virtual nsresult RegisterScriptEventListener(nsIScriptContext *aContext, 
                                               nsIScriptObjectOwner *aScriptObjectOwner, 
                                               const nsIID& aIID);


  virtual nsresult CaptureEvent(nsIDOMEventListener *aListener);
  virtual nsresult ReleaseEvent(nsIDOMEventListener *aListener);

  virtual nsresult HandleEvent(nsIPresContext& aPresContext, 
                               nsEvent* aEvent, 
                               nsIDOMEvent** aDOMEvent,
                               PRUint32 aFlags,
                               nsEventStatus& aEventStatus);

  virtual nsresult CreateEvent(nsIPresContext& aPresContext, 
                               nsEvent* aEvent, 
                               nsIDOMEvent** aDOMEvent);

protected:
  nsresult SetJSEventListener(nsIScriptContext *aContext, JSObject *aObject, REFNSIID aIID);
  nsresult GetIdentifiersForType(const nsString& aType, nsIID& aIID, PRInt32* aSubType);
  nsresult AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID, PRInt32 aFlags, PRInt32 aSubType);
  nsresult RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID, PRInt32 aFlags, PRInt32 aSubType);

  nsVoidArray* mEventListeners;
  nsVoidArray* mMouseListeners;
  nsVoidArray* mMouseMotionListeners;
  nsVoidArray* mKeyListeners;
  nsVoidArray* mLoadListeners;
  nsVoidArray* mFocusListeners;
  nsVoidArray* mFormListeners;
  nsVoidArray* mDragListeners;
  nsVoidArray* mPaintListeners;
  nsVoidArray* mTextListeners;

};


//Set of defines for distinguishing event hanlders within listener groupings
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

//nsIDOMKeyListener
#define NS_EVENT_BITS_KEY_NONE      0x00
#define NS_EVENT_BITS_KEY_KEYDOWN   0x01
#define NS_EVENT_BITS_KEY_KEYUP     0x02
#define NS_EVENT_BITS_KEY_KEYPRESS  0x04

//nsIDOMTextListener
#define NS_EVENT_BITS_TEXT_NONE      0x00
#define NS_EVENT_BITS_TEXT_TEXT      0x01

//nsIDOMFocusListener
#define NS_EVENT_BITS_FOCUS_NONE    0x00
#define NS_EVENT_BITS_FOCUS_FOCUS   0x01
#define NS_EVENT_BITS_FOCUS_BLUR    0x02

//nsIDOMFormListener
#define NS_EVENT_BITS_FORM_NONE     0x00
#define NS_EVENT_BITS_FORM_SUBMIT   0x01
#define NS_EVENT_BITS_FORM_RESET    0x02
#define NS_EVENT_BITS_FORM_CHANGE   0x04

//nsIDOMLoadListener
#define NS_EVENT_BITS_LOAD_NONE     0x00
#define NS_EVENT_BITS_LOAD_LOAD     0x01
#define NS_EVENT_BITS_LOAD_UNLOAD   0x02
#define NS_EVENT_BITS_LOAD_ABORT    0x04
#define NS_EVENT_BITS_LOAD_ERROR    0x08

//nsIDOMDragListener
#define NS_EVENT_BITS_DRAG_NONE     0x00
#define NS_EVENT_BITS_DRAG_START    0x01
#define NS_EVENT_BITS_DRAG_DROP     0x02

//nsIDOMPaintListener
#define NS_EVENT_BITS_PAINT_NONE    0x00
#define NS_EVENT_BITS_PAINT_PAINT   0x01

#endif // nsEventListenerManager_h__

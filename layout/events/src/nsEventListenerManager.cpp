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

#include "nsISupports.h"
#include "nsGUIEvent.h"
#include "nsIPresContext.h"
#include "nsDOMEvent.h"
#include "nsEventListenerManager.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDragListener.h"
#include "nsIEventStateManager.h"

static NS_DEFINE_IID(kIEventListenerManagerIID, NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMMouseMotionListenerIID, NS_IDOMMOUSEMOTIONLISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMFocusListenerIID, NS_IDOMFOCUSLISTENER_IID);
static NS_DEFINE_IID(kIDOMLoadListenerIID, NS_IDOMLOADLISTENER_IID);
static NS_DEFINE_IID(kIDOMDragListenerIID, NS_IDOMDRAGLISTENER_IID);
static NS_DEFINE_IID(kIDOMEventIID, NS_IDOMEVENT_IID);

nsEventListenerManager::nsEventListenerManager() {
  mEventListeners = nsnull;
  mMouseListeners = nsnull;
  mMouseMotionListeners = nsnull;
  mKeyListeners = nsnull;
  mLoadListeners = nsnull;
  mFocusListeners = nsnull;
  mDragListeners = nsnull;
}

nsEventListenerManager::~nsEventListenerManager() {
}

NS_IMPL_ADDREF(nsEventListenerManager)
NS_IMPL_RELEASE(nsEventListenerManager)

nsresult nsEventListenerManager::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIEventListenerManagerIID)) {
    *aInstancePtrResult = (void*) ((nsIEventListenerManager*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult nsEventListenerManager::TranslateGUI2DOM(nsGUIEvent*& aGUIEvent, 
                                                  nsIPresContext& aPresContext, 
                                                  nsIDOMEvent** aDOMEvent)
{
  nsIEventStateManager *mManager;
  nsISupports *mTarget;
  
  nsDOMEvent* it = new nsDOMEvent();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  it->SetGUIEvent(aGUIEvent);
  if (NS_OK == aPresContext.GetEventStateManager(&mManager)) {
    mManager->GetEventTarget(&mTarget);
    NS_RELEASE(mManager);
  }
  it->SetEventTarget(mTarget);

  return it->QueryInterface(kIDOMEventIID, (void **) aDOMEvent);
}

nsresult nsEventListenerManager::GetEventListeners(nsVoidArray *aListeners, const nsIID& aIID)
{
  return NS_OK;
}

/**
* Sets events listeners of all types. 
* @param an event listener
*/

nsresult nsEventListenerManager::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    if (nsnull == mMouseListeners) {
      mMouseListeners = new nsVoidArray();
      mMouseListeners->InsertElementAt((void*)aListener, mMouseListeners->Count());
    }
  }
  if (aIID.Equals(kIDOMMouseMotionListenerIID)) {
    if (nsnull == mMouseMotionListeners) {
      mMouseMotionListeners = new nsVoidArray();
      mMouseMotionListeners->InsertElementAt((void*)aListener, mMouseMotionListeners->Count());
    }
  }
  if (aIID.Equals(kIDOMKeyListenerIID)) {
    if (nsnull == mKeyListeners) {
      mKeyListeners = new nsVoidArray();
      mKeyListeners->InsertElementAt((void*)aListener, mKeyListeners->Count());
    }
  }
  if (aIID.Equals(kIDOMLoadListenerIID)) {
    if (nsnull == mLoadListeners) {
      mLoadListeners = new nsVoidArray();
      mLoadListeners->InsertElementAt((void*)aListener, mLoadListeners->Count());
    }
  }
  if (aIID.Equals(kIDOMFocusListenerIID)) {
    if (nsnull == mFocusListeners) {
      mFocusListeners = new nsVoidArray();
      mFocusListeners->InsertElementAt((void*)aListener, mFocusListeners->Count());
    }
  }
  if (aIID.Equals(kIDOMDragListenerIID)) {
    if (nsnull == mDragListeners) {
      mDragListeners = new nsVoidArray();
      mDragListeners->InsertElementAt((void*)aListener, mDragListeners->Count());
    }
  }
  return NS_OK;
}

/**
* Removes event listeners of all types. 
* @param an event listener
*/

nsresult nsEventListenerManager::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    if (nsnull == mMouseListeners) {
      mMouseListeners = new nsVoidArray();
      mMouseListeners->RemoveElement((void*)aListener);
    }
  }
  if (aIID.Equals(kIDOMMouseMotionListenerIID)) {
    if (nsnull == mMouseMotionListeners) {
      mMouseMotionListeners = new nsVoidArray();
      mMouseMotionListeners->RemoveElement((void*)aListener);
    }
  }
  if (aIID.Equals(kIDOMKeyListenerIID)) {
    if (nsnull == mKeyListeners) {
      mKeyListeners = new nsVoidArray();
      mKeyListeners->RemoveElement((void*)aListener);
    }
  }
  if (aIID.Equals(kIDOMLoadListenerIID)) {
    if (nsnull == mLoadListeners) {
      mLoadListeners = new nsVoidArray();
      mLoadListeners->RemoveElement((void*)aListener);
    }
  }
  if (aIID.Equals(kIDOMFocusListenerIID)) {
    if (nsnull == mFocusListeners) {
      mFocusListeners = new nsVoidArray();
      mFocusListeners->RemoveElement((void*)aListener);
    }
  }
  if (aIID.Equals(kIDOMDragListenerIID)) {
    if (nsnull == mDragListeners) {
      mDragListeners = new nsVoidArray();
      mDragListeners->RemoveElement((void*)aListener);
    }
  }
  return NS_OK;
}

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

nsresult nsEventListenerManager::HandleEvent(nsIPresContext& aPresContext,
                                            nsGUIEvent* aEvent,
                                            nsIDOMEvent* aDOMEvent,
                                            nsEventStatus& aEventStatus)
{
  nsresult mRet;

  switch(aEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
    if (nsnull != mMouseListeners) {
      for (int i=0; i<mMouseListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMMouseListener*)(mMouseListeners->ElementAt(i)))->MouseDown(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_UP:
    if (nsnull != mMouseListeners) {
      for (int i=0; i<mMouseListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMMouseListener*)(mMouseListeners->ElementAt(i)))->MouseUp(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;
  case NS_MOUSE_LEFT_DOUBLECLICK:
  case NS_MOUSE_RIGHT_DOUBLECLICK:
    if (nsnull != mMouseListeners) {
      for (int i=0; i<mMouseListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMMouseListener*)(mMouseListeners->ElementAt(i)))->MouseDblClick(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;
  case NS_MOUSE_ENTER:
    if (nsnull != mMouseListeners) {
      for (int i=0; i<mMouseListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMMouseListener*)(mMouseListeners->ElementAt(i)))->MouseOver(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;
  case NS_MOUSE_EXIT:
    if (nsnull != mMouseListeners) {
      for (int i=0; i<mMouseListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMMouseListener*)(mMouseListeners->ElementAt(i)))->MouseOut(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;
  case NS_MOUSE_MOVE:
    if (nsnull != mMouseMotionListeners) {
      for (int i=0; i<mMouseMotionListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMMouseMotionListener*)(mMouseMotionListeners->ElementAt(i)))->MouseMove(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }
      }
    }
    break;
  case NS_KEY_UP:
    if (nsnull != mKeyListeners) {
      for (int i=0; i<mKeyListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMKeyListener*)(mKeyListeners->ElementAt(i)))->KeyUp(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }                        
      }
    }
    break;
  case NS_KEY_DOWN:
    if (nsnull != mKeyListeners) {
      for (int i=0; i<mKeyListeners->Count(); i++) {
        if (nsnull == aDOMEvent) {
          TranslateGUI2DOM(aEvent, aPresContext, &aDOMEvent);
        }
        if (nsnull != aDOMEvent) {
          mRet = ((nsIDOMKeyListener*)(mKeyListeners->ElementAt(i)))->KeyDown(aDOMEvent);
          aEventStatus = (NS_OK == mRet) ? nsEventStatus_eIgnore : nsEventStatus_eConsumeNoDefault;
        }                        
      }
    }
    break;

  }

  return NS_OK;
}

/**
* Captures all events designated for descendant objects at the current level.
* @param an event listener
*/

nsresult nsEventListenerManager::CaptureEvent(nsIDOMEventListener *aListener)
{
  return NS_OK;
}             

/**
* Releases all events designated for descendant objects at the current level.
* @param an event listener
*/

nsresult nsEventListenerManager::ReleaseEvent(nsIDOMEventListener *aListener)
{
  return NS_OK;
}


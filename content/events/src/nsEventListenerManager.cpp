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
#include "nsIDOMFormListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMTextListener.h"
#include "nsIEventStateManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptEventListener.h"
#include "nsDOMEventsIIDs.h"
#include "prmem.h"

static NS_DEFINE_IID(kIEventListenerManagerIID, NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIDOMEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDOMEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kIScriptEventListenerIID, NS_ISCRIPTEVENTLISTENER_IID);

nsEventListenerManager::nsEventListenerManager() 
{
  mEventListeners = nsnull;
  mMouseListeners = nsnull;
  mMouseMotionListeners = nsnull;
  mKeyListeners = nsnull;
  mLoadListeners = nsnull;
  mFocusListeners = nsnull;
  mFormListeners = nsnull;
  mDragListeners = nsnull;
  mPaintListeners = nsnull;
  mTextListeners = nsnull;
  NS_INIT_REFCNT();
}

nsEventListenerManager::~nsEventListenerManager() 
{
  ReleaseListeners(mEventListeners);
  ReleaseListeners(mMouseListeners);
  ReleaseListeners(mMouseMotionListeners);
  ReleaseListeners(mKeyListeners);
  ReleaseListeners(mLoadListeners);
  ReleaseListeners(mFocusListeners);
  ReleaseListeners(mFormListeners);
  ReleaseListeners(mDragListeners);
  ReleaseListeners(mPaintListeners);
  ReleaseListeners(mTextListeners);
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

nsVoidArray** nsEventListenerManager::GetListenersByIID(const nsIID& aIID)
{
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    return &mMouseListeners;
  }
  else if (aIID.Equals(kIDOMMouseMotionListenerIID)) {
    return &mMouseMotionListeners;
  }
  else if (aIID.Equals(kIDOMKeyListenerIID)) {
    return &mKeyListeners;
  }
  else if (aIID.Equals(kIDOMLoadListenerIID)) {
    return &mLoadListeners;
  }
  else if (aIID.Equals(kIDOMFocusListenerIID)) {
    return &mFocusListeners;
  }
  else if (aIID.Equals(kIDOMFormListenerIID)) {
    return &mFormListeners;
  }
  else if (aIID.Equals(kIDOMDragListenerIID)) {
    return &mDragListeners;
  }
  else if (aIID.Equals(kIDOMPaintListenerIID)) {
    return &mPaintListeners;
  }
  else if (aIID.Equals(kIDOMTextListenerIID)) {
	return &mTextListeners;
  }
  return nsnull;
}

void nsEventListenerManager::ReleaseListeners(nsVoidArray* aListeners)
{
  if (nsnull != aListeners) {
    PRInt32 i, count = aListeners->Count();
    nsListenerStruct *ls;
    for (i = 0; i < count; i++) {
      ls = (nsListenerStruct*)aListeners->ElementAt(i);
      if (ls != nsnull) {
        NS_IF_RELEASE(ls->mListener);
        PR_DELETE(ls);
      }
    }
    delete aListeners;
  }
}

nsresult nsEventListenerManager::GetEventListeners(nsVoidArray **aListeners, const nsIID& aIID)
{
  nsVoidArray** mListeners = GetListenersByIID(aIID);

  *aListeners = *mListeners;

  return NS_OK;
}

/**
* Sets events listeners of all types. 
* @param an event listener
*/
nsresult nsEventListenerManager::AddEventListener(nsIDOMEventListener *aListener, 
                                                  const nsIID& aIID, 
                                                  PRInt32 aFlags,
                                                  PRInt32 aSubType)
{
  nsVoidArray** listeners = GetListenersByIID(aIID);

  if (nsnull == *listeners) {
    *listeners = new nsVoidArray();
  }

  if (nsnull == *listeners) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRBool found = PR_FALSE;
  nsListenerStruct* ls;
  nsIScriptEventListener* sel = nsnull;
  nsIScriptEventListener* regSel;

  aListener->QueryInterface(kIScriptEventListenerIID, (void**)&sel);

  for (int i=0; i<(*listeners)->Count(); i++) {
    ls = (nsListenerStruct*)(*listeners)->ElementAt(i);
    if (ls->mListener == aListener) {
      ls->mFlags |= aFlags;
      ls->mSubType |= aSubType;
      found = PR_TRUE;
      break;
    }
    else if (sel) {
      if (NS_OK == ls->mListener->QueryInterface(kIScriptEventListenerIID, (void**)&regSel)) {
        if (NS_OK == regSel->CheckIfEqual(sel)) {
          if (ls->mFlags & aFlags && ls->mSubType & aSubType) {
            found = PR_TRUE;
            break;
          }
        }
      }
    }
  }

  if (!found) {
    ls = PR_NEW(nsListenerStruct);
    if (ls) {
      ls->mListener = aListener;
      ls->mFlags = aFlags;
      ls->mSubType = aSubType;
      (*listeners)->InsertElementAt((void*)ls, (*listeners)->Count());
    }
  }

  NS_ADDREF(aListener);

  return NS_OK;
}

nsresult nsEventListenerManager::RemoveEventListener(nsIDOMEventListener *aListener, 
                                                     const nsIID& aIID, 
                                                     PRInt32 aFlags,
                                                     PRInt32 aSubType)
{
  nsVoidArray** listeners = GetListenersByIID(aIID);

  if (nsnull == *listeners) {
    return NS_OK;
  }

  nsListenerStruct* ls;

  for (int i=0; i<(*listeners)->Count(); i++) {
    ls = (nsListenerStruct*)(*listeners)->ElementAt(i);
    if (ls->mListener == aListener) {
      ls->mFlags &= ~aFlags;
      ls->mSubType &= ~aSubType;
      if (ls->mFlags == NS_EVENT_FLAG_NONE && ls->mSubType == NS_EVENT_BITS_NONE) {
        NS_RELEASE(ls->mListener);
        (*listeners)->RemoveElement((void*)ls);
        PR_DELETE(ls);
      }
      break;
    }
  }

  return NS_OK;
}

nsresult nsEventListenerManager::AddEventListenerByIID(nsIDOMEventListener *aListener, 
                                                       const nsIID& aIID, PRInt32 aFlags)
{
  AddEventListener(aListener, aIID, aFlags, NS_EVENT_BITS_NONE);
  return NS_OK;
}

nsresult nsEventListenerManager::RemoveEventListenerByIID(nsIDOMEventListener *aListener, 
                                                          const nsIID& aIID, PRInt32 aFlags)
{
  RemoveEventListener(aListener, aIID, aFlags, NS_EVENT_BITS_NONE);
  return NS_OK;
}

nsresult nsEventListenerManager::GetIdentifiersForType(const nsString& aType, nsIID& aIID, PRInt32* aFlags)
{
  if (aType == "mousedown") {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEDOWN;
  }
  else if (aType == "mouseup") {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEUP;
  }
  else if (aType == "click") {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_CLICK;
  }
  else if (aType == "dblclick") {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_DBLCLICK;
  }
  else if (aType == "mouseover") {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEOVER;
  }
  else if (aType == "mouseout") {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEOUT;
  }
  else if (aType == "keydown") {
    aIID = kIDOMKeyListenerIID;
    *aFlags = NS_EVENT_BITS_KEY_KEYDOWN;
  }
  else if (aType == "keyup") {
    aIID = kIDOMKeyListenerIID;
    *aFlags = NS_EVENT_BITS_KEY_KEYUP;
  }
  else if (aType == "keypress") {
    aIID = kIDOMKeyListenerIID;
    *aFlags = NS_EVENT_BITS_KEY_KEYPRESS;
  }
  else if (aType == "mousemove") {
    aIID = kIDOMMouseMotionListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE;
  }
  else if (aType == "focus") {
    aIID = kIDOMFocusListenerIID;
    *aFlags = NS_EVENT_BITS_FOCUS_FOCUS;
  }
  else if (aType == "blur") {
    aIID = kIDOMFocusListenerIID;
    *aFlags = NS_EVENT_BITS_FOCUS_BLUR;
  }
  else if (aType == "submit") {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_SUBMIT;
  }
  else if (aType == "reset") {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_RESET;
  }
  else if (aType == "change") {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_CHANGE;
  }
  else if (aType == "load") {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_LOAD;
  }
  else if (aType == "unload") {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_UNLOAD;
  }
  else if (aType == "abort") {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_ABORT;
  }
  else if (aType == "error") {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_ERROR;
  }
  else if (aType == "paint") {
    aIID = kIDOMPaintListenerIID;
    *aFlags = NS_EVENT_BITS_PAINT_PAINT;
  }
  else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult nsEventListenerManager::AddEventListenerByType(nsIDOMEventListener *aListener, 
                                                        const nsString& aType, PRInt32 aFlags)
{
  PRInt32 subType;
  nsIID iid;

  if (NS_OK == GetIdentifiersForType(aType, iid, &subType)) {
    AddEventListener(aListener, iid, aFlags, subType);
  }

  return NS_OK;
}

nsresult nsEventListenerManager::RemoveEventListenerByType(nsIDOMEventListener *aListener, 
                                                          const nsString& aType, PRInt32 aFlags)
{
  PRInt32 subType;
  nsIID iid;

  if (NS_OK == GetIdentifiersForType(aType, iid, &subType)) {
    RemoveEventListener(aListener, iid, aFlags, subType);
  }

  return NS_OK;
}

const char *mEventArgv[] = {"event"};

nsresult nsEventListenerManager::SetJSEventListener(nsIScriptContext *aContext, JSObject *aObject, REFNSIID aIID)
{
  nsVoidArray *mListeners;

  if (NS_OK == GetEventListeners(&mListeners, aIID)) {
    //Run through the listeners for this IID and see if a script listener is registered
    //If so, we're set.
    if (nsnull != mListeners) {
      nsListenerStruct *ls;
      for (int i=0; i<mListeners->Count(); i++) {
        ls = (nsListenerStruct*)mListeners->ElementAt(i);
        if (ls->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
          return NS_OK;
        }
      }
    }
    //If we didn't find a script listener or no listeners existed create and add a new one.
    nsIDOMEventListener *mScriptListener;
    if (NS_OK == NS_NewJSEventListener(&mScriptListener, aContext, aObject)) {
      AddEventListenerByIID(mScriptListener, aIID, NS_EVENT_FLAG_BUBBLE | NS_PRIV_EVENT_FLAG_SCRIPT);
      NS_RELEASE(mScriptListener);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult nsEventListenerManager::AddScriptEventListener(nsIScriptContext* aContext, nsIScriptObjectOwner *aScriptObjectOwner, 
                                nsIAtom *aName, const nsString& aFunc, REFNSIID aIID)
{
  JSObject *mScriptObject;
  
  if (NS_OK == aScriptObjectOwner->GetScriptObject(aContext, (void**)&mScriptObject)) {
    JSContext* mJSContext = (JSContext*)aContext->GetNativeContext();
    nsString mName, mLowerName;
    char* mCharName;

    aName->ToString(mName);
    mName.ToLowerCase(mLowerName);
    mCharName = mLowerName.ToNewCString();

    if (nsnull != mCharName) {
      JS_CompileUCFunction(mJSContext, mScriptObject, mCharName,
		           1, mEventArgv, (jschar*)aFunc.GetUnicode(), aFunc.Length(),
		           nsnull, 0);
      delete[] mCharName;
      return SetJSEventListener(aContext, mScriptObject, aIID);
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult nsEventListenerManager::RegisterScriptEventListener(nsIScriptContext *aContext, nsIScriptObjectOwner *aScriptObjectOwner, 
                                     REFNSIID aIID)
{
  JSObject *mScriptObject;
  if (NS_OK == aScriptObjectOwner->GetScriptObject(aContext, (void**)&mScriptObject)) {
    return SetJSEventListener(aContext, mScriptObject, aIID);
  }
  return NS_ERROR_FAILURE;
}

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

nsresult nsEventListenerManager::HandleEvent(nsIPresContext& aPresContext,
                                             nsEvent* aEvent,
                                             nsIDOMEvent** aDOMEvent,
                                             PRUint32 aFlags,
                                             nsEventStatus& aEventStatus)
{
  nsresult ret = NS_OK;
  if (aFlags & NS_EVENT_FLAG_INIT) {
    aFlags |= NS_EVENT_FLAG_BUBBLE;
  }

  switch(aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_UP:
    case NS_MOUSE_LEFT_CLICK:
    case NS_MOUSE_MIDDLE_CLICK:
    case NS_MOUSE_RIGHT_CLICK:
    case NS_MOUSE_LEFT_DOUBLECLICK:
    case NS_MOUSE_MIDDLE_DOUBLECLICK:
    case NS_MOUSE_RIGHT_DOUBLECLICK:
    case NS_MOUSE_ENTER:
    case NS_MOUSE_EXIT:
      if (nsnull != mMouseListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mMouseListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMMouseListener *mMouseListener;
            nsIScriptEventListener *scriptListener;

            ls = (nsListenerStruct*)mMouseListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMMouseListenerIID, (void**)&mMouseListener)) {
                switch(aEvent->message) {
                  case NS_MOUSE_LEFT_BUTTON_DOWN:
                  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
                  case NS_MOUSE_RIGHT_BUTTON_DOWN:
                    ret = mMouseListener->MouseDown(*aDOMEvent);
                    break;
                  case NS_MOUSE_LEFT_BUTTON_UP:
                  case NS_MOUSE_MIDDLE_BUTTON_UP:
                  case NS_MOUSE_RIGHT_BUTTON_UP:
                    ret = mMouseListener->MouseUp(*aDOMEvent);
                    break;
                  case NS_MOUSE_LEFT_CLICK:
                  case NS_MOUSE_MIDDLE_CLICK:
                  case NS_MOUSE_RIGHT_CLICK:
                    ret = mMouseListener->MouseClick(*aDOMEvent);
                    break;
                  case NS_MOUSE_LEFT_DOUBLECLICK:
                  case NS_MOUSE_MIDDLE_DOUBLECLICK:
                  case NS_MOUSE_RIGHT_DOUBLECLICK:
                    ret = mMouseListener->MouseDblClick(*aDOMEvent);
                    break;
                  case NS_MOUSE_ENTER:
                    ret = mMouseListener->MouseOver(*aDOMEvent);
                    break;
                  case NS_MOUSE_EXIT:
                    ret = mMouseListener->MouseOut(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mMouseListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                switch(aEvent->message) {
                  case NS_MOUSE_LEFT_BUTTON_DOWN:
                  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
                  case NS_MOUSE_RIGHT_BUTTON_DOWN:
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEDOWN) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_LEFT_BUTTON_UP:
                  case NS_MOUSE_MIDDLE_BUTTON_UP:
                  case NS_MOUSE_RIGHT_BUTTON_UP:
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEUP) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_LEFT_CLICK:
                  case NS_MOUSE_MIDDLE_CLICK:
                  case NS_MOUSE_RIGHT_CLICK:
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_CLICK) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_LEFT_DOUBLECLICK:
                  case NS_MOUSE_MIDDLE_DOUBLECLICK:
                  case NS_MOUSE_RIGHT_DOUBLECLICK:
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_DBLCLICK) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_ENTER:
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEOVER) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_EXIT:
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEOUT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
            aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;
  
    case NS_MOUSE_MOVE:
      if (nsnull != mMouseMotionListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mMouseMotionListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMMouseMotionListener *mMouseMotionListener;

            ls = (nsListenerStruct*)mMouseMotionListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMMouseMotionListenerIID, (void**)&mMouseMotionListener)) {
                switch(aEvent->message) {
                  case NS_MOUSE_MOVE:
                    ret = mMouseMotionListener->MouseMove(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mMouseMotionListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                switch(aEvent->message) {
                  case NS_MOUSE_MOVE:
                    if (ls->mSubType & NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
            aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;
	
	case NS_TEXT_EVENT:
#if DEBUG_TAGUE
  		printf("DOM: got text event\n");
#endif
		  if (nsnull != mTextListeners) {
			  if (nsnull == *aDOMEvent) {
				  ret = NS_NewDOMEvent(aDOMEvent,aPresContext,aEvent);
			  }
        if (NS_OK == ret) {
          for (int i=0; i<mTextListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMTextListener *mTextListener;

            ls = (nsListenerStruct*)mTextListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMTextListenerIID, (void**)&mTextListener)) {
                ret = mTextListener->HandleText(*aDOMEvent);
                NS_RELEASE(mTextListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                if (ls->mSubType & NS_EVENT_BITS_TEXT_TEXT) {
                  correctSubType = PR_TRUE;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
          aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
				}
			}
		}
		break;

    case NS_KEY_UP:
    case NS_KEY_DOWN:
    case NS_KEY_PRESS:
      if (nsnull != mKeyListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mKeyListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMKeyListener *mKeyListener;

            ls = (nsListenerStruct*)mKeyListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMKeyListenerIID, (void**)&mKeyListener)) {
                switch(aEvent->message) {
                  case NS_KEY_UP:
                    ret = mKeyListener->KeyUp(*aDOMEvent);
                    break;
                  case NS_KEY_DOWN:
                    ret = mKeyListener->KeyDown(*aDOMEvent);
                    break;
                  case NS_KEY_PRESS:
                    ret = mKeyListener->KeyPress(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mKeyListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                switch(aEvent->message) {
                  case NS_KEY_UP:
                    if (ls->mSubType & NS_EVENT_BITS_KEY_KEYUP) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_KEY_DOWN:
                    if (ls->mSubType & NS_EVENT_BITS_KEY_KEYDOWN) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_KEY_PRESS:
                    if (ls->mSubType & NS_EVENT_BITS_KEY_KEYPRESS) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
            aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;

    case NS_FOCUS_CONTENT:
    case NS_BLUR_CONTENT:
      if (nsnull != mFocusListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mFocusListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMFocusListener *mFocusListener;

            ls = (nsListenerStruct*)mFocusListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMFocusListenerIID, (void**)&mFocusListener)) {
                switch(aEvent->message) {
                  case NS_FOCUS_CONTENT:
                    ret = mFocusListener->Focus(*aDOMEvent);
                    break;
                  case NS_BLUR_CONTENT:
                    ret = mFocusListener->Blur(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mFocusListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                switch(aEvent->message) {
                  case NS_FOCUS_CONTENT:
                    if (ls->mSubType & NS_EVENT_BITS_FOCUS_FOCUS) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_BLUR_CONTENT:
                    if (ls->mSubType & NS_EVENT_BITS_FOCUS_BLUR) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
            aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;

    case NS_FORM_SUBMIT:
    case NS_FORM_RESET:
    case NS_FORM_CHANGE:
      if (nsnull != mFormListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mFormListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMFormListener *mFormListener;

            ls = (nsListenerStruct*)mFormListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMFormListenerIID, (void**)&mFormListener)) {
                switch(aEvent->message) {
                  case NS_FORM_SUBMIT:
                    ret = mFormListener->Submit(*aDOMEvent);
                    break;
                  case NS_FORM_RESET:
                    ret = mFormListener->Reset(*aDOMEvent);
                    break;
                  case NS_FORM_CHANGE:
                    ret = mFormListener->Change(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mFormListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                switch(aEvent->message) {
                  case NS_FORM_SUBMIT:
                    if (ls->mSubType & NS_EVENT_BITS_FORM_SUBMIT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_FORM_RESET:
                    if (ls->mSubType & NS_EVENT_BITS_FORM_RESET) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_FORM_CHANGE:
                    if (ls->mSubType & NS_EVENT_BITS_FORM_CHANGE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
            aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;

    case NS_PAGE_LOAD:
    case NS_PAGE_UNLOAD:
      if (nsnull != mLoadListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mLoadListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMLoadListener *mLoadListener;

            ls = (nsListenerStruct*)mLoadListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMLoadListenerIID, (void**)&mLoadListener)) {
                switch(aEvent->message) {
                  case NS_PAGE_LOAD:
                    ret = mLoadListener->Load(*aDOMEvent);
                    break;
                  case NS_PAGE_UNLOAD:
                    ret = mLoadListener->Unload(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mLoadListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                switch(aEvent->message) {
                  case NS_PAGE_LOAD:
                    if (ls->mSubType & NS_EVENT_BITS_LOAD_LOAD) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_PAGE_UNLOAD:
                    if (ls->mSubType & NS_EVENT_BITS_LOAD_UNLOAD) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
            aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;
  
    case NS_PAINT:
      if (nsnull != mPaintListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mPaintListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMPaintListener *paintListener;

            ls = (nsListenerStruct*)mPaintListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMPaintListenerIID,
                                                       (void**)&paintListener)) {
                ret = paintListener->Paint(*aDOMEvent);
                NS_RELEASE(paintListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                if (ls->mSubType & NS_EVENT_BITS_PAINT_PAINT) {
                  correctSubType = PR_TRUE;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = ls->mListener->HandleEvent(*aDOMEvent);
                }
              }
            }
            aEventStatus = (NS_OK == ret)
              ? aEventStatus
              : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;

    case NS_DRAGDROP_ENTER:
    case NS_DRAGDROP_OVER:
    case NS_DRAGDROP_EXIT:
    case NS_DRAGDROP_DROP:
      if (nsnull != mDragListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }

        if (NS_OK == ret) {
          for (int i=0; i<mDragListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMDragListener *dragListener;

            ls = (nsListenerStruct*)mDragListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMDragListenerIID,
                                                       (void**)&dragListener)) {
                switch (aEvent->message) {
                  case NS_DRAGDROP_ENTER:
                    ret = dragListener->DragEnter(*aDOMEvent);
                    break;
                  case NS_DRAGDROP_OVER:
                    ret = dragListener->DragOver(*aDOMEvent);
                    break;
                  case NS_DRAGDROP_EXIT:
                    ret = dragListener->DragExit(*aDOMEvent);
                    break;
                  case NS_DRAGDROP_DROP:
                    ret = dragListener->DragDrop(*aDOMEvent);
                    break;
                } // switch 
                NS_RELEASE(dragListener);
              }
              else {
                ret = ls->mListener->HandleEvent(*aDOMEvent);
              }
            }
            aEventStatus = (NS_OK == ret)
              ? aEventStatus
              : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;


    default:
      break;
  }
  return NS_OK;
}

/**
* Creates a DOM event
*/

nsresult nsEventListenerManager::CreateEvent(nsIPresContext& aPresContext,
                                             nsEvent* aEvent,
                                             nsIDOMEvent** aDOMEvent)
{
    return NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
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

NS_HTML nsresult NS_NewEventListenerManager(nsIEventListenerManager** aInstancePtrResult) 
{
  nsIEventListenerManager* l = new nsEventListenerManager();

  if (nsnull == l) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  if (NS_OK == l->QueryInterface(kIEventListenerManagerIID, (void**) aInstancePtrResult)) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

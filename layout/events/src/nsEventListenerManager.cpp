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
    nsIDOMEventListener *mElement;
    for (i = 0; i < count; i++) {
      mElement = (nsIDOMEventListener *)aListeners->ElementAt(i);
      if (mElement != nsnull) {
        NS_RELEASE(mElement);
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
nsresult nsEventListenerManager::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsVoidArray** mListeners = GetListenersByIID(aIID);

  if (nsnull == *mListeners) {
    *mListeners = new nsVoidArray();
  }

  if (nsnull == *mListeners) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  (*mListeners)->InsertElementAt((void*)aListener, (*mListeners)->Count());

  NS_ADDREF(aListener);

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
      nsIScriptEventListener *mScriptListener;
      nsIDOMEventListener *mEventListener;
      for (int i=0; i<mListeners->Count(); i++) {
        mEventListener = (nsIDOMEventListener*)mListeners->ElementAt(i);
        if (NS_OK == mEventListener->QueryInterface(kIScriptEventListenerIID, (void**)&mScriptListener)) {
          NS_RELEASE(mScriptListener);
          return NS_OK;
        }
      }
    }
    //If we didn't find a script listener or no listeners existed create and add a new one.
    nsIDOMEventListener *mScriptListener;
    if (NS_OK == NS_NewScriptEventListener(&mScriptListener, aContext, aObject)) {
      AddEventListener(mScriptListener, aIID);
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
* Removes event listeners of all types. 
* @param an event listener
*/

nsresult nsEventListenerManager::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsVoidArray** mListeners = GetListenersByIID(aIID);
 
  if (nsnull != *mListeners && PR_TRUE == (*mListeners)->RemoveElement((void*)aListener)) {
    NS_RELEASE(aListener);
    return NS_OK;
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
                                             nsEventStatus& aEventStatus)
{
  nsresult ret = NS_OK;

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
            nsIDOMEventListener *mEventListener;
            nsIDOMMouseListener *mMouseListener;

            mEventListener = (nsIDOMEventListener*)mMouseListeners->ElementAt(i);

            if (NS_OK == mEventListener->QueryInterface(kIDOMMouseListenerIID, (void**)&mMouseListener)) {
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
              ret = mEventListener->ProcessEvent(*aDOMEvent);
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
            nsIDOMEventListener *mEventListener;
            nsIDOMMouseMotionListener *mMouseMotionListener;

            mEventListener = (nsIDOMEventListener*)mMouseMotionListeners->ElementAt(i);

            if (NS_OK == mEventListener->QueryInterface(kIDOMMouseMotionListenerIID, (void**)&mMouseMotionListener)) {
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
              ret = mEventListener->ProcessEvent(*aDOMEvent);
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
					nsIDOMEventListener *mEventListener;
					nsIDOMTextListener *mTextListener;

					mEventListener = (nsIDOMEventListener*)mTextListeners->ElementAt(i);
					
					if (NS_OK == mEventListener->QueryInterface(kIDOMTextListenerIID, (void**)&mTextListener)) {
						ret = mTextListener->HandleText(*aDOMEvent);
						NS_RELEASE(mTextListener);
					}
					else {
						ret = mEventListener->ProcessEvent(*aDOMEvent);
					}
				}
				aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
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
            nsIDOMEventListener *mEventListener;
            nsIDOMKeyListener *mKeyListener;

            mEventListener = (nsIDOMEventListener*)mKeyListeners->ElementAt(i);

            if (NS_OK == mEventListener->QueryInterface(kIDOMKeyListenerIID, (void**)&mKeyListener)) {
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
              ret = mEventListener->ProcessEvent(*aDOMEvent);
            }
            aEventStatus = (NS_OK == ret) ? aEventStatus : nsEventStatus_eConsumeNoDefault;
          }
        }
      }
      break;

    case NS_GOTFOCUS:
    case NS_LOSTFOCUS:
      if (nsnull != mFocusListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; i<mFocusListeners->Count(); i++) {
            nsIDOMEventListener *mEventListener;
            nsIDOMFocusListener *mFocusListener;

            mEventListener = (nsIDOMEventListener*)mFocusListeners->ElementAt(i);

            if (NS_OK == mEventListener->QueryInterface(kIDOMFocusListenerIID, (void**)&mFocusListener)) {
              switch(aEvent->message) {
                case NS_GOTFOCUS:
                  ret = mFocusListener->Focus(*aDOMEvent);
                  break;
                case NS_LOSTFOCUS:
                  ret = mFocusListener->Blur(*aDOMEvent);
                  break;
                default:
                  break;
              }
              NS_RELEASE(mFocusListener);
            }
            else {
              ret = mEventListener->ProcessEvent(*aDOMEvent);
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
            nsIDOMEventListener *mEventListener;
            nsIDOMFormListener *mFormListener;

            mEventListener = (nsIDOMEventListener*)mFormListeners->ElementAt(i);

            if (NS_OK == mEventListener->QueryInterface(kIDOMFormListenerIID, (void**)&mFormListener)) {
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
              ret = mEventListener->ProcessEvent(*aDOMEvent);
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
            nsIDOMEventListener *mEventListener;
            nsIDOMLoadListener *mLoadListener;

            mEventListener = (nsIDOMEventListener*)mLoadListeners->ElementAt(i);

            if (NS_OK == mEventListener->QueryInterface(kIDOMLoadListenerIID, (void**)&mLoadListener)) {
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
              ret = mEventListener->ProcessEvent(*aDOMEvent);
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
            nsIDOMEventListener *eventListener;
            nsIDOMPaintListener *paintListener;

            eventListener = (nsIDOMEventListener*)mPaintListeners->ElementAt(i);

            if (NS_OK == eventListener->QueryInterface(kIDOMPaintListenerIID,
                                                     (void**)&paintListener)) {
              ret = paintListener->Paint(*aDOMEvent);
              NS_RELEASE(paintListener);
            }
            else {
              ret = eventListener->ProcessEvent(*aDOMEvent);
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

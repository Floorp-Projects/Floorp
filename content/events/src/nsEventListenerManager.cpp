/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsISupports.h"
#include "nsGUIEvent.h"
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
#include "nsIDOMCompositionListener.h"
#include "nsIDOMMenuListener.h"
#include "nsIDOMScrollListener.h"
#include "nsIEventStateManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptEventListener.h"
#include "nsIJSEventListener.h"
#include "nsDOMEventsIIDs.h"
#include "prmem.h"
#include "nsIScriptGlobalObject.h"
#include "nsLayoutAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMPropEnums.h"
#include "nsDOMError.h"
#include "nsIJSContextStack.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

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
  mCompositionListeners = nsnull;
  mMenuListeners = nsnull;
  mScrollListeners = nsnull;
  mDestroyed = PR_FALSE;
  mTarget = nsnull;
  NS_INIT_REFCNT();
}

nsEventListenerManager::~nsEventListenerManager() 
{
  ReleaseListeners(&mEventListeners, PR_FALSE);
  ReleaseListeners(&mMouseListeners, PR_FALSE);
  ReleaseListeners(&mMouseMotionListeners, PR_FALSE);
  ReleaseListeners(&mKeyListeners, PR_FALSE);
  ReleaseListeners(&mLoadListeners, PR_FALSE);
  ReleaseListeners(&mFocusListeners, PR_FALSE);
  ReleaseListeners(&mFormListeners, PR_FALSE);
  ReleaseListeners(&mDragListeners, PR_FALSE);
  ReleaseListeners(&mPaintListeners, PR_FALSE);
  ReleaseListeners(&mTextListeners, PR_FALSE);
  ReleaseListeners(&mCompositionListeners, PR_FALSE);
  ReleaseListeners(&mMenuListeners, PR_FALSE);
  ReleaseListeners(&mScrollListeners, PR_FALSE);
}

NS_IMPL_ADDREF(nsEventListenerManager)
NS_IMPL_RELEASE(nsEventListenerManager)

// We need to return to the old QI form briefly to deal with the 
// results of the partial aggregation we began using nsGenericElement
// and nsGenericDOMDataNode.  We should look for a better long term
// solution. -joki
nsresult nsEventListenerManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIEventListenerManager))) {
    *aInstancePtr = (void*)(nsIEventListenerManager*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMEventTarget))) {
    *aInstancePtr = (void*)(nsIDOMEventTarget*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMEventReceiver))) {
    *aInstancePtr = (void*)(nsIDOMEventReceiver*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*)(nsISupports*)(nsIEventListenerManager*)this;
    AddRef();
    return NS_OK;
  }
  if (mTarget) {
    return mTarget->QueryInterface(aIID, aInstancePtr);
  }
  return NS_NOINTERFACE;
}

#if 0
NS_INTERFACE_MAP_BEGIN(nsEventListenerManager)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEventListenerManager)
   NS_INTERFACE_MAP_ENTRY(nsIEventListenerManager)
   NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
   NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
NS_INTERFACE_MAP_END
#endif

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
  else if (aIID.Equals(kIDOMCompositionListenerIID)) {
  return &mCompositionListeners;
  }
  else if (aIID.Equals(kIDOMMenuListenerIID)) {
  return &mMenuListeners;
  }
  else if (aIID.Equals(kIDOMScrollListenerIID)) {
  return &mScrollListeners;
  }
  return nsnull;
}

void nsEventListenerManager::ReleaseListeners(nsVoidArray** aListeners, PRBool aScriptOnly)
{
  if (nsnull != *aListeners) {
    PRInt32 i, count = (*aListeners)->Count();
    nsListenerStruct *ls;
    for (i = 0; i < count; i++) {
      ls = (nsListenerStruct*)(*aListeners)->ElementAt(i);
      if (ls != nsnull) {
        if (aScriptOnly) {
          if (ls->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
            NS_RELEASE(ls->mListener);
            (*aListeners)->RemoveElement((void*)ls);
            PR_DELETE(ls);
          }
        }
        else {
          NS_IF_RELEASE(ls->mListener);
          PR_DELETE(ls);
        }
      }
    }
    //Only delete if we were removing all listeners or if the script
    //listener removal brought the count to 0.
    if (!aScriptOnly || (*aListeners)->Count() == 0) {
      delete *aListeners;
      *aListeners = nsnull;
    }
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
  nsresult rv;
  nsCOMPtr<nsIScriptEventListener> sel = do_QueryInterface(aListener, &rv);
  
  for (int i=0; i<(*listeners)->Count(); i++) {
    ls = (nsListenerStruct*)(*listeners)->ElementAt(i);
    if (ls->mListener == aListener && ls->mFlags == aFlags) {
      ls->mSubType |= aSubType;
      found = PR_TRUE;
      break;
    }
    else if (sel) { 
      //Listener is an nsIScriptEventListener so we need to use its CheckIfEqual
      //method to verify equality.
      nsCOMPtr<nsIScriptEventListener> regSel = do_QueryInterface(ls->mListener, &rv);
      if (NS_SUCCEEDED(rv) && regSel) {
		    PRBool equal;
        if (NS_SUCCEEDED(regSel->CheckIfEqual(sel, &equal)) && equal) {
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
      ls->mSubTypeCapture = NS_EVENT_BITS_NONE;
      ls->mHandlerIsString = 0;
      (*listeners)->InsertElementAt((void*)ls, (*listeners)->Count());
      NS_ADDREF(aListener);
    }
  }

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
  nsresult rv;
  nsCOMPtr<nsIScriptEventListener> sel = do_QueryInterface(aListener, &rv);

  for (int i=0; i<(*listeners)->Count(); i++) {
    ls = (nsListenerStruct*)(*listeners)->ElementAt(i);
    if (ls->mListener == aListener && ls->mFlags == aFlags) {
      ls->mFlags &= ~aFlags;
      ls->mSubType &= ~aSubType;
      if (ls->mFlags == NS_EVENT_FLAG_NONE && ls->mSubType == NS_EVENT_BITS_NONE) {
        NS_RELEASE(ls->mListener);
        (*listeners)->RemoveElement((void*)ls);
        PR_DELETE(ls);
      }
      break;
    }
    else if (sel) {
      //Listener is an nsIScriptEventListener so we need to use its CheckIfEqual
      //method to verify equality.
      nsCOMPtr<nsIScriptEventListener> regSel = do_QueryInterface(ls->mListener, &rv);
      if (NS_SUCCEEDED(rv) && regSel) {
        PRBool equal;
        if (NS_SUCCEEDED(regSel->CheckIfEqual(sel, &equal)) && equal) {
          if (ls->mFlags & aFlags && ls->mSubType & aSubType) {
            NS_RELEASE(ls->mListener);
            (*listeners)->RemoveElement((void*)ls);
            PR_DELETE(ls);
          }
        }
      }
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

nsresult nsEventListenerManager::GetIdentifiersForType(nsIAtom* aType, nsIID& aIID, PRInt32* aFlags)
{
  if (aType == nsLayoutAtoms::onmousedown) {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEDOWN;
  }
  else if (aType == nsLayoutAtoms::onmouseup) {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEUP;
  }
  else if (aType == nsLayoutAtoms::onclick) {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_CLICK;
  }
  else if (aType == nsLayoutAtoms::ondblclick) {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_DBLCLICK;
  }
  else if (aType == nsLayoutAtoms::onmouseover) {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEOVER;
  }
  else if (aType == nsLayoutAtoms::onmouseout) {
    aIID = kIDOMMouseListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEOUT;
  }
  else if (aType == nsLayoutAtoms::onkeydown) {
    aIID = kIDOMKeyListenerIID;
    *aFlags = NS_EVENT_BITS_KEY_KEYDOWN;
  }
  else if (aType == nsLayoutAtoms::onkeyup) {
    aIID = kIDOMKeyListenerIID;
    *aFlags = NS_EVENT_BITS_KEY_KEYUP;
  }
  else if (aType == nsLayoutAtoms::onkeypress) {
    aIID = kIDOMKeyListenerIID;
    *aFlags = NS_EVENT_BITS_KEY_KEYPRESS;
  }
  else if (aType == nsLayoutAtoms::onmousemove) {
    aIID = kIDOMMouseMotionListenerIID;
    *aFlags = NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE;
  }
  else if (aType == nsLayoutAtoms::onfocus) {
    aIID = kIDOMFocusListenerIID;
    *aFlags = NS_EVENT_BITS_FOCUS_FOCUS;
  }
  else if (aType == nsLayoutAtoms::onblur) {
    aIID = kIDOMFocusListenerIID;
    *aFlags = NS_EVENT_BITS_FOCUS_BLUR;
  }
  else if (aType == nsLayoutAtoms::onsubmit) {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_SUBMIT;
  }
  else if (aType == nsLayoutAtoms::onreset) {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_RESET;
  }
  else if (aType == nsLayoutAtoms::onchange) {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_CHANGE;
  }
  else if (aType == nsLayoutAtoms::onselect) {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_SELECT;
  }
  else if (aType == nsLayoutAtoms::oninput) {
    aIID = kIDOMFormListenerIID;
    *aFlags = NS_EVENT_BITS_FORM_INPUT;
  }
  else if (aType == nsLayoutAtoms::onload) {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_LOAD;
  }
  else if (aType == nsLayoutAtoms::onunload) {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_UNLOAD;
  }
  else if (aType == nsLayoutAtoms::onabort) {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_ABORT;
  }
  else if (aType == nsLayoutAtoms::onerror) {
    aIID = kIDOMLoadListenerIID;
    *aFlags = NS_EVENT_BITS_LOAD_ERROR;
  }
  else if (aType == nsLayoutAtoms::onpaint) {
    aIID = kIDOMPaintListenerIID;
    *aFlags = NS_EVENT_BITS_PAINT_PAINT;
  }
  else if (aType == nsLayoutAtoms::onresize) {
    aIID = kIDOMPaintListenerIID;
    *aFlags = NS_EVENT_BITS_PAINT_RESIZE;
  }
  else if (aType == nsLayoutAtoms::onscroll) {
    aIID = kIDOMPaintListenerIID;
    *aFlags = NS_EVENT_BITS_PAINT_SCROLL;
  } // extened this to handle IME related events
  else if (aType == nsLayoutAtoms::oncreate) {
    aIID = kIDOMMenuListenerIID; 
    *aFlags = NS_EVENT_BITS_MENU_CREATE;
  }
  else if (aType == nsLayoutAtoms::onclose) {
    aIID = kIDOMMenuListenerIID; 
    *aFlags = NS_EVENT_BITS_XUL_CLOSE;
  }
  else if (aType == nsLayoutAtoms::ondestroy) {
    aIID = kIDOMMenuListenerIID; 
    *aFlags = NS_EVENT_BITS_MENU_DESTROY;
  }
  else if (aType == nsLayoutAtoms::oncommand) {
    aIID = kIDOMMenuListenerIID; 
    *aFlags = NS_EVENT_BITS_MENU_ACTION;
  }
  else if (aType == nsLayoutAtoms::onbroadcast) {
    aIID = kIDOMMenuListenerIID;
    *aFlags = NS_EVENT_BITS_XUL_BROADCAST;
  }
  else if (aType == nsLayoutAtoms::oncommandupdate) {
    aIID = kIDOMMenuListenerIID;
    *aFlags = NS_EVENT_BITS_XUL_COMMAND_UPDATE;
  }
  else if (aType == nsLayoutAtoms::onoverflow) {
    aIID = kIDOMScrollListenerIID;
    *aFlags = NS_EVENT_BITS_SCROLLPORT_OVERFLOW;
  }
  else if (aType == nsLayoutAtoms::onunderflow) {
    aIID = kIDOMScrollListenerIID;
    *aFlags = NS_EVENT_BITS_SCROLLPORT_UNDERFLOW;
  }
  else if (aType == nsLayoutAtoms::onoverflowchanged) {
    aIID = kIDOMScrollListenerIID;
    *aFlags = NS_EVENT_BITS_SCROLLPORT_OVERFLOWCHANGED;
  }
  else if (aType == nsLayoutAtoms::ondragenter) {
    aIID = NS_GET_IID(nsIDOMDragListener);
    *aFlags = NS_EVENT_BITS_DRAG_ENTER;
  }
  else if (aType == nsLayoutAtoms::ondragover) {
    aIID = NS_GET_IID(nsIDOMDragListener); 
    *aFlags = NS_EVENT_BITS_DRAG_OVER;
  }
  else if (aType == nsLayoutAtoms::ondragexit) {
    aIID = NS_GET_IID(nsIDOMDragListener); 
    *aFlags = NS_EVENT_BITS_DRAG_EXIT;
  }
  else if (aType == nsLayoutAtoms::ondragdrop) {
    aIID = NS_GET_IID(nsIDOMDragListener); 
    *aFlags = NS_EVENT_BITS_DRAG_DROP;
  }
  else if (aType == nsLayoutAtoms::ondraggesture) {
    aIID = NS_GET_IID(nsIDOMDragListener); 
    *aFlags = NS_EVENT_BITS_DRAG_GESTURE;
  }
  else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult nsEventListenerManager::AddEventListenerByType(nsIDOMEventListener *aListener, 
                                                        const nsAReadableString& aType, PRInt32 aFlags)
{
  PRInt32 subType;
  nsIID iid;
  nsAutoString str; str.AssignWithConversion("on");
  nsIAtom* atom;

  str.Append(aType);
  atom = NS_NewAtom(str);

  if (NS_OK == GetIdentifiersForType(atom, iid, &subType)) {
    AddEventListener(aListener, iid, aFlags, subType);
  }

  NS_IF_RELEASE(atom);

  return NS_OK;
}

nsresult nsEventListenerManager::RemoveEventListenerByType(nsIDOMEventListener *aListener, 
                                                          const nsAReadableString& aType, PRInt32 aFlags)
{
  PRInt32 subType;
  nsIID iid;

  nsAutoString str; str.AssignWithConversion("on");
  nsIAtom* atom;

  str.Append(aType);
  atom = NS_NewAtom(str);

  if (NS_OK == GetIdentifiersForType(atom, iid, &subType)) {
    RemoveEventListener(aListener, iid, aFlags, subType);
  }

  NS_IF_RELEASE(atom);

  return NS_OK;
}

nsListenerStruct*
nsEventListenerManager::FindJSEventListener(REFNSIID aIID)
{
  nsVoidArray *listeners;

  nsresult result = GetEventListeners(&listeners, aIID);
  if (NS_SUCCEEDED(result)) {
    //Run through the listeners for this IID and see if a script listener is registered
    if (nsnull != listeners) {
      nsListenerStruct *ls;
      for (int i=0; i<listeners->Count(); i++) {
        ls = (nsListenerStruct*)listeners->ElementAt(i);
        if (ls->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
          return ls;
        }
      }
    }
  }

  return nsnull;
}

nsresult nsEventListenerManager::SetJSEventListener(nsIScriptContext *aContext, 
                                                    nsIScriptObjectOwner *aOwner, 
                                                    nsIAtom* aName,
                                                    REFNSIID aIID,
                                                    PRBool aIsString)
{
  nsresult result = NS_OK;
  nsListenerStruct *ls;

  ls = FindJSEventListener(aIID);

  if (nsnull == ls) {
    //If we didn't find a script listener or no listeners existed create and add a new one.
    nsIDOMEventListener* scriptListener;
    result = NS_NewJSEventListener(&scriptListener, aContext, aOwner);
    if (NS_SUCCEEDED(result)) {
      AddEventListenerByIID(scriptListener, aIID, NS_EVENT_FLAG_BUBBLE | NS_PRIV_EVENT_FLAG_SCRIPT);
      NS_RELEASE(scriptListener);
      ls = FindJSEventListener(aIID);
    }
  }

  if (NS_SUCCEEDED(result) && ls) {
    PRInt32 flags;
    nsIID iid;
    result = GetIdentifiersForType(aName, iid, &flags);
    if (NS_SUCCEEDED(result)) {
      //Set flag to indicate possible need for compilation later
      if (aIsString) {
        ls->mHandlerIsString |= flags;
      }
      else {
        ls->mHandlerIsString &= ~flags;
      }

      //Set subtype flags based on event
      ls->mSubType |= flags;
    }
  }

  return result;
}

nsresult
nsEventListenerManager::AddScriptEventListener(nsIScriptContext* aContext,
                                               nsIScriptObjectOwner *aScriptObjectOwner,
                                               nsIAtom *aName,
                                               const nsAReadableString& aBody,
                                               REFNSIID aIID,
                                               PRBool aDeferCompilation)
{
  JSObject *scriptObject;
  nsresult rv;
  
  if (!aDeferCompilation) {
    rv = aScriptObjectOwner->GetScriptObject(aContext, (void**)&scriptObject);
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner = do_QueryInterface(aScriptObjectOwner);
    void *handler = nsnull;
    PRBool done = PR_FALSE;

    if (handlerOwner) {
      rv = handlerOwner->GetCompiledEventHandler(aName, &handler);
      if (NS_SUCCEEDED(rv) && handler) {
        rv = aContext->BindCompiledEventHandler(scriptObject, aName, handler);
        if (NS_FAILED(rv))
          return rv;
        done = PR_TRUE;
      }
    }

    if (!done) {
      if (handlerOwner) {
        // Always let the handler owner compile the event handler, as
        // it may want to use a special context or scope object.
        rv = handlerOwner->CompileEventHandler(aContext, scriptObject, aName, aBody, &handler);
      }
      else {
        rv = aContext->CompileEventHandler(scriptObject, aName, aBody,
                                           (handlerOwner != nsnull),
                                           &handler);
      }
      if (NS_FAILED(rv)) return rv;
    }
  }
  return SetJSEventListener(aContext, aScriptObjectOwner, aName, aIID, aDeferCompilation);
}

nsresult nsEventListenerManager::RegisterScriptEventListener(nsIScriptContext *aContext, 
                                                             nsIScriptObjectOwner *aScriptObjectOwner, 
                                                             nsIAtom *aName,
                                                             REFNSIID aIID)
{
  // Check that we have access to set an event listener. Prevents snooping attacks across 
  // domains by setting onkeypress handlers, for instance.
  // You'd think it'd work just to get the JSContext from aContext, but that's actually the
  // JSContext whose private object parents the object in aScriptObjectOwner.
  nsresult rv;
  NS_WITH_SERVICE(nsIJSContextStack, stack, "@mozilla.org/js/xpc/ContextStack;1", 
                  &rv);
  if (NS_FAILED(rv))
      return rv;
  JSContext *cx;
  if (NS_FAILED(stack->Peek(&cx)))
      return nsnull;
  JSObject *jsobj;
  if (NS_FAILED(rv = aScriptObjectOwner->GetScriptObject(aContext, (void**)&jsobj)))
    return rv;
  NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                  NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
      return rv;
  if (NS_FAILED(rv = securityManager->CheckScriptAccess(cx, jsobj,
                  NS_DOM_PROP_EVENTTARGET_ADDEVENTLISTENER, PR_TRUE)))
  {
    return rv;
  }
  return SetJSEventListener(aContext, aScriptObjectOwner, aName, aIID, PR_FALSE);
}

nsresult
nsEventListenerManager::HandleEventSubType(nsListenerStruct* aListenerStruct,
                                           nsIDOMEvent* aDOMEvent,
                                           nsIDOMEventTarget* aCurrentTarget,
                                           PRUint32 aSubType,
                                           PRUint32 aPhaseFlags)
{
  nsresult result = NS_OK;

  // If this is a script handler and we haven't yet
  // compiled the event handler itself
  if (aListenerStruct->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
    // If we're not in the capture phase we must *NOT* have capture flags
    // set.  Compiled script handlers are one or the other, not both.
    if (aPhaseFlags & NS_EVENT_FLAG_BUBBLE && !aPhaseFlags & NS_EVENT_FLAG_INIT) {
      if (aListenerStruct->mSubTypeCapture & aSubType) {
        return result;
      }
    }
    // If we're in the capture phase we must have capture flags set.
    else if (aPhaseFlags & NS_EVENT_FLAG_CAPTURE && !aPhaseFlags & NS_EVENT_FLAG_INIT) {
      if (!(aListenerStruct->mSubTypeCapture & aSubType)) {
        return result;
      }
    }

    if (aListenerStruct->mHandlerIsString & aSubType) {
      nsCOMPtr<nsIJSEventListener> jslistener = do_QueryInterface(aListenerStruct->mListener);
      if (jslistener) {
        nsCOMPtr<nsIScriptObjectOwner> owner;
        nsCOMPtr<nsIScriptContext> scriptCX;
        result = jslistener->GetEventTarget(getter_AddRefs(scriptCX), getter_AddRefs(owner));

        if (NS_SUCCEEDED(result)) {
          JSObject* jsobj;
          result = owner->GetScriptObject(scriptCX, (void**)&jsobj);
          if (NS_SUCCEEDED(result)) {
            nsAutoString eventString;
            if (NS_SUCCEEDED(aDOMEvent->GetType(eventString))) {
              eventString.InsertWithConversion("on", 0, 2);
              nsCOMPtr<nsIAtom> atom = getter_AddRefs(NS_NewAtom(eventString));

              nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner = do_QueryInterface(owner);
              void* handler = nsnull;

              if (handlerOwner) {
                result = handlerOwner->GetCompiledEventHandler(atom, &handler);
                if (NS_SUCCEEDED(result) && handler) {
                  result = scriptCX->BindCompiledEventHandler(jsobj, atom, handler);
                  aListenerStruct->mHandlerIsString &= ~aSubType;
                }
              }

              if (aListenerStruct->mHandlerIsString & aSubType) {
                // This should never happen for anything but content
                // XXX I don't like that we have to reference content
                // from here. The alternative is to store the event handler
                // string on the JS object itself.
                nsCOMPtr<nsIContent> content = do_QueryInterface(owner);
                NS_ASSERTION(content, "only content should have event handler attributes");
                if (content) {
                  nsAutoString handlerBody;
                  result = content->GetAttribute(kNameSpaceID_None, atom, handlerBody);
                  if (NS_SUCCEEDED(result)) {
                    if (handlerOwner) {
                      // Always let the handler owner compile the event
                      // handler, as it may want to use a special
                      // context or scope object.
                      result = handlerOwner->CompileEventHandler(scriptCX, jsobj, atom, handlerBody, &handler);
                    }
                    else {
                      result = scriptCX->CompileEventHandler(jsobj, atom, handlerBody,
                                                             (handlerOwner != nsnull),
                                                             &handler);
                    }
                    if (NS_SUCCEEDED(result))
                      aListenerStruct->mHandlerIsString &= ~aSubType;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIPrivateDOMEvent> aPrivDOMEvent(do_QueryInterface(aDOMEvent));
    aPrivDOMEvent->SetCurrentTarget(aCurrentTarget);
    result = aListenerStruct->mListener->HandleEvent(aDOMEvent);
    aPrivDOMEvent->SetCurrentTarget(nsnull);
  }

  return result;
}

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

nsresult nsEventListenerManager::HandleEvent(nsIPresContext* aPresContext,
                                             nsEvent* aEvent,
                                             nsIDOMEvent** aDOMEvent,
                                             nsIDOMEventTarget* aCurrentTarget,
                                             PRUint32 aFlags,
                                             nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  nsresult ret = NS_OK;
  if (aFlags & NS_EVENT_FLAG_INIT) {
    aFlags |= (NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE);
  }
  //Set the value of the internal PreventDefault flag properly based on aEventStatus
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    aEvent->flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }

  /* Without this addref, certain events, notably ones bound to
     keys which cause window deletion, can destroy this object
     before we're ready. */
  nsCOMPtr<nsIEventListenerManager> kungFuDeathGrip(this);
  nsAutoString empty;

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
    case NS_MOUSE_ENTER_SYNTH:
    case NS_MOUSE_EXIT_SYNTH:
      if (nsnull != mMouseListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mMouseListeners && i<mMouseListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMMouseListener *mMouseListener;

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
                  case NS_MOUSE_ENTER_SYNTH:
                    ret = mMouseListener->MouseOver(*aDOMEvent);
                    break;
                  case NS_MOUSE_EXIT_SYNTH:
                    ret = mMouseListener->MouseOut(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mMouseListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_MOUSE_LEFT_BUTTON_DOWN:
                  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
                  case NS_MOUSE_RIGHT_BUTTON_DOWN:
                    subType = NS_EVENT_BITS_MOUSE_MOUSEDOWN;
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEDOWN) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_LEFT_BUTTON_UP:
                  case NS_MOUSE_MIDDLE_BUTTON_UP:
                  case NS_MOUSE_RIGHT_BUTTON_UP:
                    subType = NS_EVENT_BITS_MOUSE_MOUSEUP;
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEUP) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_LEFT_CLICK:
                  case NS_MOUSE_MIDDLE_CLICK:
                  case NS_MOUSE_RIGHT_CLICK:
                    subType = NS_EVENT_BITS_MOUSE_CLICK;
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_CLICK) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_LEFT_DOUBLECLICK:
                  case NS_MOUSE_MIDDLE_DOUBLECLICK:
                  case NS_MOUSE_RIGHT_DOUBLECLICK:
                    subType = NS_EVENT_BITS_MOUSE_DBLCLICK;
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_DBLCLICK) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_ENTER_SYNTH:
                    subType = NS_EVENT_BITS_MOUSE_MOUSEOVER;
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEOVER) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MOUSE_EXIT_SYNTH:
                    subType = NS_EVENT_BITS_MOUSE_MOUSEOUT;
                    if (ls->mSubType & NS_EVENT_BITS_MOUSE_MOUSEOUT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;
  
    case NS_MOUSE_MOVE:
      if (nsnull != mMouseMotionListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mMouseMotionListeners && i<mMouseMotionListeners->Count(); i++) {
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
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_MOUSE_MOVE:
                    subType = NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE;
                    if (ls->mSubType & NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;

    case NS_COMPOSITION_START:
    case NS_COMPOSITION_END:
    case NS_COMPOSITION_QUERY:
    case NS_RECONVERSION_QUERY:
#if DEBUG_TAGUE
      printf("DOM: got composition event\n");
#endif
      if (nsnull != mCompositionListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent,aPresContext,empty,aEvent);
        }
        if (NS_OK == ret) {
          for(int i=0; mTextListeners && i<mTextListeners->Count();i++) {
            nsListenerStruct *ls;
            nsIDOMCompositionListener* mCompositionListener;
            ls =(nsListenerStruct*)mCompositionListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMCompositionListenerIID, (void**)&mCompositionListener)) {
                if (aEvent->message==NS_COMPOSITION_START) {
                  ret = mCompositionListener->HandleStartComposition(*aDOMEvent);
                }
                else if (aEvent->message==NS_COMPOSITION_END) {
                  ret = mCompositionListener->HandleEndComposition(*aDOMEvent);
                }
                else if (aEvent->message==NS_COMPOSITION_QUERY) {
                  ret = mCompositionListener->HandleQueryComposition(*aDOMEvent);
                }
                else if (aEvent->message==NS_RECONVERSION_QUERY) {
                  ret = mCompositionListener->HandleQueryReconversion(*aDOMEvent);
				}
              }
              NS_RELEASE(mCompositionListener);
            }
            else {
              PRBool correctSubType = PR_FALSE;
              PRUint32 subType = 0;
              switch(aEvent->message) {
                case NS_COMPOSITION_START:
                  subType = NS_EVENT_BITS_COMPOSITION_START;
                  if (ls->mSubType & NS_EVENT_BITS_COMPOSITION_START) {
                    correctSubType = PR_TRUE;
                  }
                  break;
                case NS_COMPOSITION_END:
                  subType = NS_EVENT_BITS_COMPOSITION_END;
                  if (ls->mSubType & NS_EVENT_BITS_COMPOSITION_END) {
                    correctSubType = PR_TRUE;
                  }
                  break;
                case NS_COMPOSITION_QUERY:
                  subType = NS_EVENT_BITS_COMPOSITION_QUERY;
                  if (ls->mSubType & NS_EVENT_BITS_COMPOSITION_QUERY) {
                    correctSubType = PR_TRUE;
                  }
                  break;
                default:
                  break;
              }
              if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
              }
            }
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
          ret = NS_NewDOMUIEvent(aDOMEvent,aPresContext,empty,aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mTextListeners && i<mTextListeners->Count(); i++) {
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
                PRUint32 subType = NS_EVENT_BITS_TEXT_TEXT;
                if (ls->mSubType & NS_EVENT_BITS_TEXT_TEXT) {
                  correctSubType = PR_TRUE;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;

    case NS_KEY_UP:
    case NS_KEY_DOWN:
    case NS_KEY_PRESS:
      if (nsnull != mKeyListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mKeyListeners && i<mKeyListeners->Count(); i++) {
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
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_KEY_UP:
                    subType = NS_EVENT_BITS_KEY_KEYUP;
                    if (ls->mSubType & NS_EVENT_BITS_KEY_KEYUP) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_KEY_DOWN:
                    subType = NS_EVENT_BITS_KEY_KEYDOWN;
                    if (ls->mSubType & NS_EVENT_BITS_KEY_KEYDOWN) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_KEY_PRESS:
                    subType = NS_EVENT_BITS_KEY_KEYPRESS;
                    if (ls->mSubType & NS_EVENT_BITS_KEY_KEYPRESS) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;

    case NS_FOCUS_CONTENT:
    case NS_BLUR_CONTENT:
      if (nsnull != mFocusListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mFocusListeners && i<mFocusListeners->Count(); i++) {
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
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_FOCUS_CONTENT:
                    subType = NS_EVENT_BITS_FOCUS_FOCUS;
                    if (ls->mSubType & NS_EVENT_BITS_FOCUS_FOCUS) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_BLUR_CONTENT:
                    subType = NS_EVENT_BITS_FOCUS_BLUR;
                    if (ls->mSubType & NS_EVENT_BITS_FOCUS_BLUR) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;

    case NS_FORM_SUBMIT:
    case NS_FORM_RESET:
    case NS_FORM_CHANGE:
    case NS_FORM_SELECTED:
    case NS_FORM_INPUT:
      if (nsnull != mFormListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mFormListeners && i<mFormListeners->Count(); i++) {
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
                  case NS_FORM_SELECTED:
                    ret = mFormListener->Select(*aDOMEvent);
                    break;
                  case NS_FORM_INPUT:
                    ret = mFormListener->Input(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mFormListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_FORM_SUBMIT:
                    subType = NS_EVENT_BITS_FORM_SUBMIT;
                    if (ls->mSubType & NS_EVENT_BITS_FORM_SUBMIT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_FORM_RESET:
                    subType = NS_EVENT_BITS_FORM_RESET;
                    if (ls->mSubType & NS_EVENT_BITS_FORM_RESET) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_FORM_CHANGE:
                    subType = NS_EVENT_BITS_FORM_CHANGE;
                    if (ls->mSubType & NS_EVENT_BITS_FORM_CHANGE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_FORM_SELECTED:
                    subType = NS_EVENT_BITS_FORM_SELECT;
                    if (ls->mSubType & NS_EVENT_BITS_FORM_SELECT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_FORM_INPUT:
                    subType = NS_EVENT_BITS_FORM_INPUT;
                    if (ls->mSubType & NS_EVENT_BITS_FORM_INPUT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;

    case NS_PAGE_LOAD:
    case NS_PAGE_UNLOAD:
    case NS_IMAGE_LOAD:
    case NS_IMAGE_ERROR:
    case NS_SCRIPT_ERROR:

      if (nsnull != mLoadListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mLoadListeners && i<mLoadListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMLoadListener *mLoadListener;

            ls = (nsListenerStruct*)mLoadListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMLoadListenerIID, (void**)&mLoadListener)) {
                switch(aEvent->message) {
                  case NS_PAGE_LOAD:
                  case NS_IMAGE_LOAD:
                    ret = mLoadListener->Load(*aDOMEvent);
                    break;
                  case NS_PAGE_UNLOAD:
                    ret = mLoadListener->Unload(*aDOMEvent);
                    break;
                  case NS_IMAGE_ERROR:
                  case NS_SCRIPT_ERROR:
                    ret = mLoadListener->Error(*aDOMEvent);
                  default:
                    break;
                }
                NS_RELEASE(mLoadListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_PAGE_LOAD:
                  case NS_IMAGE_LOAD:
                    subType = NS_EVENT_BITS_LOAD_LOAD;
                    if (ls->mSubType & NS_EVENT_BITS_LOAD_LOAD) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_PAGE_UNLOAD:
                    subType = NS_EVENT_BITS_LOAD_UNLOAD;
                    if (ls->mSubType & NS_EVENT_BITS_LOAD_UNLOAD) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_IMAGE_ERROR:
                  case NS_SCRIPT_ERROR:
                    subType = NS_EVENT_BITS_LOAD_ERROR;
                    if (ls->mSubType & NS_EVENT_BITS_LOAD_ERROR) {
                      correctSubType = PR_TRUE;
                    }
                    break;                    
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;
  
    case NS_PAINT:
    case NS_RESIZE_EVENT:
    case NS_SCROLL_EVENT:
      if (nsnull != mPaintListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mPaintListeners && i<mPaintListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMPaintListener *paintListener;

            ls = (nsListenerStruct*)mPaintListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMPaintListenerIID,
                                                       (void**)&paintListener)) {
                switch(aEvent->message) {
                  case NS_PAINT:
                    ret = paintListener->Paint(*aDOMEvent);
                    break;
                  case NS_RESIZE_EVENT:
                    ret = paintListener->Resize(*aDOMEvent);
                    break;
                  case NS_SCROLL_EVENT:
                    ret = paintListener->Scroll(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(paintListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_PAINT:
                    subType = NS_EVENT_BITS_PAINT_PAINT;
                    if (ls->mSubType & NS_EVENT_BITS_PAINT_PAINT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_RESIZE_EVENT:
                    subType = NS_EVENT_BITS_PAINT_RESIZE;
                    if (ls->mSubType & NS_EVENT_BITS_PAINT_RESIZE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_SCROLL_EVENT:
                    subType = NS_EVENT_BITS_PAINT_SCROLL;
                    if (ls->mSubType & NS_EVENT_BITS_PAINT_SCROLL) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;

    case NS_DRAGDROP_ENTER:
    case NS_DRAGDROP_OVER_SYNTH:
    case NS_DRAGDROP_EXIT_SYNTH:
    case NS_DRAGDROP_DROP:
    case NS_DRAGDROP_GESTURE:
      if (nsnull != mDragListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }

        if (NS_OK == ret) {
          for (int i=0; mDragListeners && i<mDragListeners->Count(); i++) {
            nsListenerStruct *dragStruct;

            dragStruct = (nsListenerStruct*)mDragListeners->ElementAt(i);

            if (dragStruct->mFlags & aFlags) {
              nsCOMPtr<nsIDOMDragListener> dragListener ( do_QueryInterface(dragStruct->mListener) );
              if ( dragListener ) {
                switch (aEvent->message) {
                  case NS_DRAGDROP_ENTER:
                    ret = dragListener->DragEnter(*aDOMEvent);
                    break;
                  case NS_DRAGDROP_OVER_SYNTH:
                    ret = dragListener->DragOver(*aDOMEvent);
                    break;
                  case NS_DRAGDROP_EXIT_SYNTH:
                    ret = dragListener->DragExit(*aDOMEvent);
                    break;
                  case NS_DRAGDROP_DROP:
                    ret = dragListener->DragDrop(*aDOMEvent);
                    break;
                  case NS_DRAGDROP_GESTURE:
                    ret = dragListener->DragGesture(*aDOMEvent);
                    break;
                } // switch 
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_DRAGDROP_ENTER:
                    subType = NS_EVENT_BITS_DRAG_ENTER;
                    if (dragStruct->mSubType & NS_EVENT_BITS_DRAG_ENTER)
                      correctSubType = PR_TRUE;
                    break;
                  case NS_DRAGDROP_OVER_SYNTH:
                    subType = NS_EVENT_BITS_DRAG_OVER;
                    if (dragStruct->mSubType & NS_EVENT_BITS_DRAG_OVER)
                      correctSubType = PR_TRUE;
                    break;
                  case NS_DRAGDROP_EXIT_SYNTH:
                    subType = NS_EVENT_BITS_DRAG_EXIT;
                    if (dragStruct->mSubType & NS_EVENT_BITS_DRAG_EXIT)
                      correctSubType = PR_TRUE;
                    break;
                  case NS_DRAGDROP_DROP:
                    subType = NS_EVENT_BITS_DRAG_DROP;
                    if (dragStruct->mSubType & NS_EVENT_BITS_DRAG_DROP)
                      correctSubType = PR_TRUE;
                    break;
                  case NS_DRAGDROP_GESTURE:
                    subType = NS_EVENT_BITS_DRAG_GESTURE;
                    if (dragStruct->mSubType & NS_EVENT_BITS_DRAG_GESTURE)
                      correctSubType = PR_TRUE;
                    break;
                  default:
                    break;
                }
                if (correctSubType || dragStruct->mSubType == NS_EVENT_BITS_DRAG_NONE)
                  ret = HandleEventSubType(dragStruct, *aDOMEvent, aCurrentTarget, subType, aFlags);
              }
            }
          }
        }
      }
      break;
    case NS_SCROLLPORT_OVERFLOW:
    case NS_SCROLLPORT_UNDERFLOW:
    case NS_SCROLLPORT_OVERFLOWCHANGED:
    if (nsnull != mScrollListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mScrollListeners && i<mScrollListeners->Count(); i++) {
            nsListenerStruct* ls;
            nsIDOMScrollListener* scrollListener;

            ls = (nsListenerStruct*)mScrollListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMScrollListenerIID, (void**)&scrollListener)) {
                switch(aEvent->message) {
                  case NS_SCROLLPORT_OVERFLOW:
                    ret = scrollListener->Overflow(*aDOMEvent);
                    break;
                  case NS_SCROLLPORT_UNDERFLOW:
                    ret = scrollListener->Underflow(*aDOMEvent);
                    break;
                  case NS_SCROLLPORT_OVERFLOWCHANGED:
                    ret = scrollListener->OverflowChanged(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(scrollListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_SCROLLPORT_OVERFLOW:
                    subType = NS_EVENT_BITS_SCROLLPORT_OVERFLOW;
                    if (ls->mSubType & NS_EVENT_BITS_SCROLLPORT_OVERFLOW) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_SCROLLPORT_UNDERFLOW:
                    subType = NS_EVENT_BITS_SCROLLPORT_UNDERFLOW;
                    if (ls->mSubType & NS_EVENT_BITS_SCROLLPORT_UNDERFLOW) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_SCROLLPORT_OVERFLOWCHANGED:
                    subType = NS_EVENT_BITS_SCROLLPORT_OVERFLOWCHANGED;
                    if (ls->mSubType & NS_EVENT_BITS_SCROLLPORT_OVERFLOWCHANGED) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;
    case NS_MENU_CREATE:
    case NS_XUL_CLOSE:
    case NS_MENU_DESTROY:
    case NS_MENU_ACTION:
    case NS_XUL_BROADCAST:
    case NS_XUL_COMMAND_UPDATE:
      if (nsnull != mMenuListeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; mMenuListeners && i<mMenuListeners->Count(); i++) {
            nsListenerStruct *ls;
            nsIDOMMenuListener *mMenuListener;

            ls = (nsListenerStruct*)mMenuListeners->ElementAt(i);

            if (ls->mFlags & aFlags) {
              if (NS_OK == ls->mListener->QueryInterface(kIDOMMenuListenerIID, (void**)&mMenuListener)) {
                switch(aEvent->message) {
                  case NS_MENU_CREATE:
                    ret = mMenuListener->Create(*aDOMEvent);
                    break;
                  case NS_XUL_CLOSE:
                    ret = mMenuListener->Close(*aDOMEvent);
                    break;
                  case NS_MENU_DESTROY:
                    ret = mMenuListener->Destroy(*aDOMEvent);
                    break;
                  case NS_MENU_ACTION:
                    ret = mMenuListener->Action(*aDOMEvent);
                    break;
                  case NS_XUL_BROADCAST:
                    ret = mMenuListener->Broadcast(*aDOMEvent);
                    break;
                  case NS_XUL_COMMAND_UPDATE:
                    ret = mMenuListener->CommandUpdate(*aDOMEvent);
                    break;
                  default:
                    break;
                }
                NS_RELEASE(mMenuListener);
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_MENU_CREATE:
                    subType = NS_EVENT_BITS_MENU_CREATE;
                    if (ls->mSubType & NS_EVENT_BITS_MENU_CREATE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_CLOSE:
                    subType = NS_EVENT_BITS_XUL_CLOSE;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_CLOSE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MENU_DESTROY:
                    subType = NS_EVENT_BITS_MENU_DESTROY;
                    if (ls->mSubType & NS_EVENT_BITS_MENU_DESTROY) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MENU_ACTION:
                    subType = NS_EVENT_BITS_MENU_ACTION;
                    if (ls->mSubType & NS_EVENT_BITS_MENU_ACTION) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_BROADCAST:
                    subType = NS_EVENT_BITS_XUL_BROADCAST;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_BROADCAST) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_COMMAND_UPDATE:
                    subType = NS_EVENT_BITS_XUL_COMMAND_UPDATE;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_COMMAND_UPDATE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, subType, aFlags);
                }
              }
            }
          }
        }
      }
      break;

    default:
      break;
  }
  //XXX This is going away
  *aEventStatus = (NS_OK == ret)
    ? *aEventStatus
    : nsEventStatus_eConsumeNoDefault;

  // This is correct
  *aEventStatus = (aEvent->flags & NS_EVENT_FLAG_NO_DEFAULT)
    ? nsEventStatus_eConsumeNoDefault
    : *aEventStatus;

  return NS_OK;
}

/**
* Creates a DOM event
*/

nsresult nsEventListenerManager::CreateEvent(nsIPresContext* aPresContext,
                                             nsEvent* aEvent,
                                             const nsAReadableString& aEventType,
                                             nsIDOMEvent** aDOMEvent)
{
  nsAutoString str(aEventType);
  if (!aEvent && !str.EqualsIgnoreCase("MouseEvent") && !str.EqualsIgnoreCase("KeyEvent") &&
      !str.EqualsIgnoreCase("HTMLEvent")) {
    return NS_ERROR_FAILURE;
  }

  return NS_NewDOMUIEvent(aDOMEvent, aPresContext, aEventType, aEvent);
}

/**
* Captures all events designated for descendant objects at the current level.
* @param an event listener
*/

nsresult nsEventListenerManager::CaptureEvent(PRInt32 aEventTypes)
{
  return FlipCaptureBit(aEventTypes, PR_TRUE);
}             

/**
* Releases all events designated for descendant objects at the current level.
* @param an event listener
*/

nsresult nsEventListenerManager::ReleaseEvent(PRInt32 aEventTypes)
{
  return FlipCaptureBit(aEventTypes, PR_FALSE);
}

nsresult nsEventListenerManager::FlipCaptureBit(PRInt32 aEventTypes, PRBool aInitCapture)
{
  nsIID iid;
  nsListenerStruct *ls;

  if (aEventTypes & nsIDOMEvent::MOUSEDOWN) {
    iid = kIDOMMouseListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEDOWN; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEDOWN;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::MOUSEUP) {
    iid = kIDOMMouseListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEUP; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEUP;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::MOUSEOVER) {
    iid = kIDOMMouseListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEOVER; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEOVER;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::MOUSEOUT) {
    iid = kIDOMMouseListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEOUT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEOUT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::MOUSEMOVE) {
    iid = kIDOMMouseMotionListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::CLICK) {
    iid = kIDOMMouseListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_CLICK; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_CLICK;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::DBLCLICK) {
    iid = kIDOMMouseListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_DBLCLICK; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_DBLCLICK;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::KEYDOWN) {
    iid = kIDOMKeyListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_KEY_KEYDOWN; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_KEY_KEYDOWN;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::KEYUP) {
    iid = kIDOMKeyListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_KEY_KEYUP; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_KEY_KEYUP;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::KEYPRESS) {
    iid = kIDOMKeyListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_KEY_KEYPRESS; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_KEY_KEYPRESS;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::DRAGDROP) {
    iid = NS_GET_IID(nsIDOMDragListener);
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_DRAG_ENTER; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_DRAG_ENTER;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  /*if (aEventTypes & nsIDOMEvent::MOUSEDRAG) {
    iid = kIDOMMouseListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEDOWN; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEDOWN;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }*/
  if (aEventTypes & nsIDOMEvent::FOCUS) {
    iid = kIDOMFocusListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FOCUS_FOCUS; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FOCUS_FOCUS;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::BLUR) {
    iid = kIDOMFocusListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FOCUS_BLUR; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FOCUS_BLUR;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::SELECT) {
    iid = kIDOMFormListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_SELECT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_SELECT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::CHANGE) {
    iid = kIDOMFormListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_CHANGE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_CHANGE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::RESET) {
    iid = kIDOMFormListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_RESET; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_RESET;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::SUBMIT) {
    iid = kIDOMFormListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_SUBMIT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_SUBMIT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::LOAD) {
    iid = kIDOMLoadListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_LOAD; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_LOAD;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::UNLOAD) {
    iid = kIDOMLoadListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_UNLOAD; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_UNLOAD;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::ABORT) {
    iid = kIDOMLoadListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_ABORT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_ABORT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::ERROR) {
    iid = kIDOMLoadListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_ERROR; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_ERROR;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::RESIZE) {
    iid = kIDOMPaintListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_PAINT_RESIZE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_PAINT_RESIZE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMEvent::SCROLL) {
    iid = kIDOMPaintListenerIID;
    ls = FindJSEventListener(iid);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_PAINT_RESIZE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_PAINT_RESIZE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  return NS_OK;
}

nsresult nsEventListenerManager::RemoveAllListeners(PRBool aScriptOnly)
{
  ReleaseListeners(&mEventListeners, aScriptOnly);
  ReleaseListeners(&mMouseListeners, aScriptOnly);
  ReleaseListeners(&mMouseMotionListeners, aScriptOnly);
  ReleaseListeners(&mKeyListeners, aScriptOnly);
  ReleaseListeners(&mLoadListeners, aScriptOnly);
  ReleaseListeners(&mFocusListeners, aScriptOnly);
  ReleaseListeners(&mFormListeners, aScriptOnly);
  ReleaseListeners(&mDragListeners, aScriptOnly);
  ReleaseListeners(&mPaintListeners, aScriptOnly);
  ReleaseListeners(&mTextListeners, aScriptOnly);
  ReleaseListeners(&mCompositionListeners, aScriptOnly);
  mDestroyed = PR_TRUE;
  return NS_OK;
}

nsresult nsEventListenerManager::SetListenerTarget(nsISupports* aTarget)
{
  //WEAK reference, must be set back to nsnull when done
  mTarget = aTarget;
  return NS_OK;
}

// nsIDOMEventTarget interface
NS_IMETHODIMP 
nsEventListenerManager::AddEventListener(const nsAReadableString& aType, 
                                         nsIDOMEventListener* aListener, 
                                         PRBool aUseCapture)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  return AddEventListenerByType(aListener, aType, flags);
}

NS_IMETHODIMP 
nsEventListenerManager::RemoveEventListener(const nsAReadableString& aType, 
                                            nsIDOMEventListener* aListener, 
                                            PRBool aUseCapture)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  
  return RemoveEventListenerByType(aListener, aType, flags);
}

NS_IMETHODIMP
nsEventListenerManager::DispatchEvent(nsIDOMEvent* aEvent)
{
  //If we don't have a target set this doesn't work.
  if (mTarget) {
    nsCOMPtr<nsIContent> targetContent(do_QueryInterface(mTarget));
    if (targetContent) {
      nsCOMPtr<nsIDocument> document;
      targetContent->GetDocument(*getter_AddRefs(document));

      if (document) {
        // Obtain a presentation context
        PRInt32 count = document->GetNumberOfShells();
        if (count == 0)
          return NS_OK;

        nsCOMPtr<nsIPresShell> shell = getter_AddRefs(document->GetShellAt(0));

        // Retrieve the context
        nsCOMPtr<nsIPresContext> aPresContext;
        shell->GetPresContext(getter_AddRefs(aPresContext));

        nsCOMPtr<nsIEventStateManager> esm;
        if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(esm)))) {
          return esm->DispatchNewEvent(mTarget, aEvent);
        }
      }
    }
  }
  return NS_ERROR_FAILURE;
}

// nsIDOMEventReceiver interface
NS_IMETHODIMP 
nsEventListenerManager::AddEventListenerByIID(nsIDOMEventListener *aListener, 
                                              const nsIID& aIID)
{
  return AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
}

NS_IMETHODIMP 
nsEventListenerManager::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  return RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
}

NS_IMETHODIMP 
nsEventListenerManager::GetListenerManager(nsIEventListenerManager** aInstancePtrResult)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);
  *aInstancePtrResult = NS_STATIC_CAST(nsIEventListenerManager*, this);
  NS_ADDREF(*aInstancePtrResult);
  return NS_OK;
}
 
NS_IMETHODIMP 
nsEventListenerManager::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  return NS_NewEventListenerManager(aInstancePtrResult);
}

NS_IMETHODIMP 
nsEventListenerManager::HandleEvent(nsIDOMEvent *aEvent)
{
  return DispatchEvent(aEvent);
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


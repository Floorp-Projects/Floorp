/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsISupports.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsEventListenerManager.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMContextMenuListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMTextListener.h"
#include "nsIDOMCompositionListener.h"
#include "nsIDOMXULListener.h"
#include "nsIDOMScrollListener.h"
#include "nsIDOMMutationListener.h"
#include "nsIEventStateManager.h"
#include "nsPIDOMWindow.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIScriptEventListener.h"
#include "nsIJSEventListener.h"
#include "prmem.h"
#include "nsIScriptGlobalObject.h"
#include "nsLayoutAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIJSContextStack.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsMutationEvent.h"
#include "nsIXPConnect.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMCID.h"
#include "nsIScriptObjectOwner.h" // for nsIScriptEventHandlerOwner
#include "nsIClassInfo.h"
#include "nsIFocusController.h"
#include "nsIDOMElement.h"
#include "nsIBoxObject.h"
#include "nsIDOMNSDocument.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#include "nsIDOMEventGroup.h"
#include "nsContentCID.h"

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kDOMEventGroupCID, NS_DOMEVENTGROUP_CID);

// Strong references to event groups
nsIDOMEventGroup* gSystemEventGroup;
nsIDOMEventGroup* gDOM2EventGroup;

PRUint32 nsEventListenerManager::mInstanceCount = 0;

nsEventListenerManager::nsEventListenerManager() 
{
  mManagerType = NS_ELM_NONE;
  mSingleListener = nsnull;
  mSingleListenerType = eEventArrayType_None;
  mMultiListeners = nsnull;
  mGenericListeners = nsnull;
  mListenersRemoved = PR_FALSE;

  mTarget = nsnull;
  NS_INIT_ISUPPORTS();
  ++mInstanceCount;
}

static PRBool PR_CALLBACK
GenericListenersHashEnum(nsHashKey *aKey, void *aData, void* closure)
{
  nsVoidArray* listeners = NS_STATIC_CAST(nsVoidArray*, aData);
  if (listeners) {
    PRInt32 i, count = listeners->Count();
    nsListenerStruct *ls;
    PRBool* scriptOnly = NS_STATIC_CAST(PRBool*, closure);
    for (i = count-1; i >= 0; --i) {
      ls = (nsListenerStruct*)listeners->ElementAt(i);
      if (ls != nsnull) {
        if (*scriptOnly) {
          if (ls->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
            NS_RELEASE(ls->mListener);
            //listeners->RemoveElement((void*)ls); we delete the entire array anyways, no need to RemoveElement
            PR_DELETE(ls);
          }
        }
        else {
          NS_IF_RELEASE(ls->mListener);
          PR_DELETE(ls);
        }
      }
    }
    //Only delete if we were removing all listeners
    if (!*scriptOnly) {
      delete listeners;
    }
  }
  return PR_TRUE;
}

nsEventListenerManager::~nsEventListenerManager() 
{
  RemoveAllListeners(PR_FALSE);

  --mInstanceCount;
  if(mInstanceCount == 0) {
    NS_IF_RELEASE(gSystemEventGroup);
    NS_IF_RELEASE(gDOM2EventGroup);
  }
}

nsresult nsEventListenerManager::RemoveAllListeners(PRBool aScriptOnly)
{
  if (!aScriptOnly) {
    mListenersRemoved = PR_TRUE;
  }

  ReleaseListeners(&mSingleListener, aScriptOnly);
  if (!mSingleListener) {
    mSingleListenerType = eEventArrayType_None;
    mManagerType &= ~NS_ELM_SINGLE;
  }

  if (mMultiListeners) {
    // XXX probably should just be i < Count()
    for (int i=0; i<EVENT_ARRAY_TYPE_LENGTH && i < mMultiListeners->Count(); i++) {
      nsVoidArray* listeners;
      listeners = NS_STATIC_CAST(nsVoidArray*, mMultiListeners->ElementAt(i));
      ReleaseListeners(&listeners, aScriptOnly);
    }
    if (!aScriptOnly) {
      delete mMultiListeners;
      mMultiListeners = nsnull;
      mManagerType &= ~NS_ELM_MULTI;
    }
  }

  if (mGenericListeners) {
    PRBool scriptOnly = aScriptOnly;
    mGenericListeners->Enumerate(GenericListenersHashEnum, &scriptOnly);
    //hash destructor
    if (!aScriptOnly) {
      delete mGenericListeners;
      mGenericListeners = nsnull;
      mManagerType &= ~NS_ELM_HASH;
    }
  }

  return NS_OK;
}

void nsEventListenerManager::Shutdown()
{
    sAddListenerID = JSVAL_VOID;
}

NS_IMPL_ADDREF(nsEventListenerManager)
NS_IMPL_RELEASE(nsEventListenerManager)


NS_INTERFACE_MAP_BEGIN(nsEventListenerManager)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEventListenerManager)
   NS_INTERFACE_MAP_ENTRY(nsIEventListenerManager)
   NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
   NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
   NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
NS_INTERFACE_MAP_END


nsVoidArray* nsEventListenerManager::GetListenersByType(EventArrayType aType, 
                                                        nsHashKey* aKey,
                                                        PRBool aCreate)
{
  NS_ASSERTION(aType >= 0,"Negative EventListenerType?");
  //Look for existing listeners
  if (aType == eEventArrayType_Hash && aKey && (mManagerType & NS_ELM_HASH)) {
    if (mGenericListeners && mGenericListeners->Exists(aKey)) {
      nsVoidArray* listeners = NS_STATIC_CAST(nsVoidArray*, mGenericListeners->Get(aKey));
      return listeners;
    }
  }
  else if (mManagerType & NS_ELM_SINGLE) {
    if (mSingleListenerType == aType) {
      return mSingleListener;
    }
  }
  else if (mManagerType & NS_ELM_MULTI) {
    if (mMultiListeners) {
      PRInt32 index = aType;
      if (index < mMultiListeners->Count()) {
        nsVoidArray* listeners;
        listeners = NS_STATIC_CAST(nsVoidArray*, mMultiListeners->ElementAt(index));
        if (listeners) {
          return listeners;
        }
      }
    }
  }

  //If we've gotten here we didn't find anything.  See if we should create something.
  if (aCreate) {
    if (aType == eEventArrayType_Hash && aKey) {
      if (!mGenericListeners) {
        mGenericListeners = new nsHashtable();
        if (!mGenericListeners) {
          //out of memory
          return nsnull;
        }
      }
      NS_ASSERTION(!(mGenericListeners->Get(aKey)), "Found existing generic listeners, should be none");
      nsVoidArray* listeners;
      listeners = new nsAutoVoidArray();
      if (!listeners) {
        //out of memory
        return nsnull;
      }
      mGenericListeners->Put(aKey, listeners);
      mManagerType |= NS_ELM_HASH;
      return listeners;
    }
    else {
      if (mManagerType & NS_ELM_SINGLE) {
        //Change single type into multi, then add new listener with the code for the 
        //multi type below
        NS_ASSERTION(!mMultiListeners, "Found existing multi listener array, should be none");
        mMultiListeners = new nsAutoVoidArray();
        if (!mMultiListeners) {
          //out of memory
          return nsnull;
        }

        //Move single listener to multi array
        mMultiListeners->ReplaceElementAt((void*)mSingleListener, mSingleListenerType);
        mSingleListener = nsnull;

        mManagerType &= ~NS_ELM_SINGLE;
        mManagerType |= NS_ELM_MULTI;
        // we'll fall through into the multi case
      }

      if (mManagerType & NS_ELM_MULTI) {
        PRInt32 index = aType;
        if (index >= 0) {
          nsVoidArray* listeners;
          NS_ASSERTION(index >= mMultiListeners->Count() || !mMultiListeners->ElementAt(index), "Found existing listeners, should be none");
          listeners = new nsAutoVoidArray();
          if (!listeners) {
            //out of memory
            return nsnull;
          }
          mMultiListeners->ReplaceElementAt((void*)listeners, index);
          return listeners;
        }
      }
      else {
        //We had no pre-existing type.  This is our first non-hash listener.
        //Create the single listener type
        NS_ASSERTION(!mSingleListener, "Found existing single listener array, should be none");
        mSingleListener = new nsAutoVoidArray();
        if (!mSingleListener) {
          //out of memory
          return nsnull;
        }
        mSingleListenerType = aType;
        mManagerType |= NS_ELM_SINGLE;
        return mSingleListener;
      }
    }
  }

  return nsnull;
}

EventArrayType nsEventListenerManager::GetTypeForIID(const nsIID& aIID)
{ 
  if (aIID.Equals(NS_GET_IID(nsIDOMMouseListener)))
      return eEventArrayType_Mouse;

  if (aIID.Equals(NS_GET_IID(nsIDOMMouseMotionListener)))
    return eEventArrayType_MouseMotion;

  if (aIID.Equals(NS_GET_IID(nsIDOMContextMenuListener)))
    return eEventArrayType_ContextMenu;

  if (aIID.Equals(NS_GET_IID(nsIDOMKeyListener)))
    return eEventArrayType_Key;

  if (aIID.Equals(NS_GET_IID(nsIDOMLoadListener)))
    return eEventArrayType_Load;

  if (aIID.Equals(NS_GET_IID(nsIDOMFocusListener)))
    return eEventArrayType_Focus;

  if (aIID.Equals(NS_GET_IID(nsIDOMFormListener)))
    return eEventArrayType_Form;

  if (aIID.Equals(NS_GET_IID(nsIDOMDragListener)))
    return eEventArrayType_Drag;

  if (aIID.Equals(NS_GET_IID(nsIDOMPaintListener)))
    return eEventArrayType_Paint;

  if (aIID.Equals(NS_GET_IID(nsIDOMTextListener)))
    return eEventArrayType_Text;

  if (aIID.Equals(NS_GET_IID(nsIDOMCompositionListener)))
    return eEventArrayType_Composition;

  if (aIID.Equals(NS_GET_IID(nsIDOMXULListener)))
    return eEventArrayType_XUL;

  if (aIID.Equals(NS_GET_IID(nsIDOMScrollListener)))
    return eEventArrayType_Scroll;

  if (aIID.Equals(NS_GET_IID(nsIDOMMutationListener)))
    return eEventArrayType_Mutation;

  return eEventArrayType_None;
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
            //(*aListeners)->RemoveElement((void*)ls); We're going to delete the array anyways
            PR_DELETE(ls);
          }
        }
        else {
          NS_IF_RELEASE(ls->mListener);
          PR_DELETE(ls);
        }
      }
    }
    //Only delete if we were removing all listeners
    if (!aScriptOnly) {
      delete *aListeners;
      *aListeners = nsnull;
    }
  }
}

/**
* Sets events listeners of all types. 
* @param an event listener
*/

nsresult
nsEventListenerManager::AddEventListener(nsIDOMEventListener *aListener, 
                                         EventArrayType aType, 
                                         PRInt32 aSubType,
                                         nsHashKey* aKey,
                                         PRInt32 aFlags,
                                         nsIDOMEventGroup* aEvtGrp)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);

  nsVoidArray* listeners = GetListenersByType(aType, aKey, PR_TRUE);

  //We asked the GetListenersByType to create the array if it had to.  If it didn't
  //then we're out of memory (or a bug was added which passed in an unsupported
  //event type)
  if (!listeners) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // For mutation listeners, we need to update the global bit on the DOM window.
  // Otherwise we won't actually fire the mutation event.
  if (aType == eEventArrayType_Mutation) {
    // Go from our target to the nearest enclosing DOM window.
    nsCOMPtr<nsIScriptGlobalObject> global;
    nsCOMPtr<nsIDocument> document;
    nsCOMPtr<nsIContent> content(do_QueryInterface(mTarget));
    if (content)
      content->GetDocument(*getter_AddRefs(document));
    else document = do_QueryInterface(mTarget);
    if (document)
      document->GetScriptGlobalObject(getter_AddRefs(global));
    else global = do_QueryInterface(mTarget);
    if (global) {
      nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(global));
      window->SetMutationListeners(aSubType);
    }
  }
  
  PRBool isSame = PR_FALSE;
  PRUint16 group = 0;
  nsCOMPtr<nsIDOMEventGroup> sysGroup;
  GetSystemEventGroupLM(getter_AddRefs(sysGroup));
  if (sysGroup) {
    sysGroup->IsSameEventGroup(aEvtGrp, &isSame);
    if (isSame) {
      group = NS_EVENT_FLAG_SYSTEM_EVENT;
    }
  }

  PRBool found = PR_FALSE;
  nsListenerStruct* ls;
  nsresult rv;
  
  nsCOMPtr<nsIScriptEventListener> sel = do_QueryInterface(aListener, &rv);
  

  for (int i=0; i<listeners->Count(); i++) {
    ls = (nsListenerStruct*)listeners->ElementAt(i);
    if (ls->mListener == aListener && ls->mFlags == aFlags && ls->mGroupFlags == group) {
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
      ls->mGroupFlags = group;
      listeners->AppendElement((void*)ls);
      NS_ADDREF(aListener);
    }

    if (aFlags & NS_EVENT_FLAG_CAPTURE) {
      //If a capturer is set up on a content object it must register that with its doc
      nsCOMPtr<nsIDocument> document;
      nsCOMPtr<nsIContent> content(do_QueryInterface(mTarget));
      if (content) {
        content->GetDocument(*getter_AddRefs(document));
        if (document) {
          //Increment capturers by 1
          document->EventCaptureRegistration(1);
        }
      }
    }
  }

  return NS_OK;
}

nsresult
nsEventListenerManager::RemoveEventListener(nsIDOMEventListener *aListener, 
                                            EventArrayType aType, 
                                            PRInt32 aSubType,
                                            nsHashKey* aKey,
                                            PRInt32 aFlags,
                                            nsIDOMEventGroup* aEvtGrp)
{
  nsVoidArray* listeners = GetListenersByType(aType, aKey, PR_FALSE);

  if (!listeners) {
    return NS_OK;
  }

  nsListenerStruct* ls;
  nsresult rv;
  nsCOMPtr<nsIScriptEventListener> sel = do_QueryInterface(aListener, &rv);
  PRBool listenerRemoved = PR_FALSE;

  for (int i=0; i<listeners->Count(); i++) {
    ls = (nsListenerStruct*)listeners->ElementAt(i);
    if (ls->mListener == aListener && ls->mFlags == aFlags) {
      ls->mSubType &= ~aSubType;
      if (ls->mSubType == NS_EVENT_BITS_NONE) {
        NS_RELEASE(ls->mListener);
        listeners->RemoveElement((void*)ls);
        PR_DELETE(ls);
        listenerRemoved = PR_TRUE;
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
            listeners->RemoveElement((void*)ls);
            PR_DELETE(ls);
            listenerRemoved = PR_TRUE;
            break; // otherwise we'd need to adjust loop count...
          }
        }
      }
    }
  }

  if (listenerRemoved && aFlags & NS_EVENT_FLAG_CAPTURE) {
    //If a capturer is set up on a content object it must register that with its doc
    nsCOMPtr<nsIDocument> document;
    nsCOMPtr<nsIContent> content(do_QueryInterface(mTarget));
    if (content) {
      content->GetDocument(*getter_AddRefs(document));
      if (document) {
        //Decrement capturers by 1
        document->EventCaptureRegistration(-1);
      }
    }
  }

  return NS_OK;
}

nsresult nsEventListenerManager::AddEventListenerByIID(nsIDOMEventListener *aListener, 
                                                       const nsIID& aIID, PRInt32 aFlags)
{
  AddEventListener(aListener, GetTypeForIID(aIID), NS_EVENT_BITS_NONE, nsnull, aFlags, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::RemoveEventListenerByIID(nsIDOMEventListener *aListener, 
                                                 const nsIID& aIID,
                                                 PRInt32 aFlags)
{
  RemoveEventListener(aListener, GetTypeForIID(aIID), NS_EVENT_BITS_NONE, nsnull, aFlags, nsnull);
  return NS_OK;
}

nsresult nsEventListenerManager::GetIdentifiersForType(nsIAtom* aType, EventArrayType* aArrayType, PRInt32* aFlags)
{
  if (aType == nsLayoutAtoms::onmousedown) {
    *aArrayType = eEventArrayType_Mouse;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEDOWN;
  }
  else if (aType == nsLayoutAtoms::onmouseup) {
    *aArrayType = eEventArrayType_Mouse;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEUP;
  }
  else if (aType == nsLayoutAtoms::onclick) {
    *aArrayType = eEventArrayType_Mouse;
    *aFlags = NS_EVENT_BITS_MOUSE_CLICK;
  }
  else if (aType == nsLayoutAtoms::ondblclick) {
    *aArrayType = eEventArrayType_Mouse;
    *aFlags = NS_EVENT_BITS_MOUSE_DBLCLICK;
  }
  else if (aType == nsLayoutAtoms::onmouseover) {
    *aArrayType = eEventArrayType_Mouse;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEOVER;
  }
  else if (aType == nsLayoutAtoms::onmouseout) {
    *aArrayType = eEventArrayType_Mouse;
    *aFlags = NS_EVENT_BITS_MOUSE_MOUSEOUT;
  }
  else if (aType == nsLayoutAtoms::onkeydown) {
    *aArrayType = eEventArrayType_Key;
    *aFlags = NS_EVENT_BITS_KEY_KEYDOWN;
  }
  else if (aType == nsLayoutAtoms::onkeyup) {
    *aArrayType = eEventArrayType_Key;
    *aFlags = NS_EVENT_BITS_KEY_KEYUP;
  }
  else if (aType == nsLayoutAtoms::onkeypress) {
    *aArrayType = eEventArrayType_Key;
    *aFlags = NS_EVENT_BITS_KEY_KEYPRESS;
  }
  else if (aType == nsLayoutAtoms::onmousemove) {
    *aArrayType = eEventArrayType_MouseMotion;
    *aFlags = NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE;
  }
  else if (aType == nsLayoutAtoms::oncontextmenu) {
    *aArrayType = eEventArrayType_ContextMenu;
    *aFlags = NS_EVENT_BITS_CONTEXTMENU;
  }
  else if (aType == nsLayoutAtoms::onfocus) {
    *aArrayType = eEventArrayType_Focus;
    *aFlags = NS_EVENT_BITS_FOCUS_FOCUS;
  }
  else if (aType == nsLayoutAtoms::onblur) {
    *aArrayType = eEventArrayType_Focus;
    *aFlags = NS_EVENT_BITS_FOCUS_BLUR;
  }
  else if (aType == nsLayoutAtoms::onsubmit) {
    *aArrayType = eEventArrayType_Form;
    *aFlags = NS_EVENT_BITS_FORM_SUBMIT;
  }
  else if (aType == nsLayoutAtoms::onreset) {
    *aArrayType = eEventArrayType_Form;
    *aFlags = NS_EVENT_BITS_FORM_RESET;
  }
  else if (aType == nsLayoutAtoms::onchange) {
    *aArrayType = eEventArrayType_Form;
    *aFlags = NS_EVENT_BITS_FORM_CHANGE;
  }
  else if (aType == nsLayoutAtoms::onselect) {
    *aArrayType = eEventArrayType_Form;
    *aFlags = NS_EVENT_BITS_FORM_SELECT;
  }
  else if (aType == nsLayoutAtoms::oninput) {
    *aArrayType = eEventArrayType_Form;
    *aFlags = NS_EVENT_BITS_FORM_INPUT;
  }
  else if (aType == nsLayoutAtoms::onload) {
    *aArrayType = eEventArrayType_Load;
    *aFlags = NS_EVENT_BITS_LOAD_LOAD;
  }
  else if (aType == nsLayoutAtoms::onunload) {
    *aArrayType = eEventArrayType_Load;
    *aFlags = NS_EVENT_BITS_LOAD_UNLOAD;
  }
  else if (aType == nsLayoutAtoms::onabort) {
    *aArrayType = eEventArrayType_Load;
    *aFlags = NS_EVENT_BITS_LOAD_ABORT;
  }
  else if (aType == nsLayoutAtoms::onerror) {
    *aArrayType = eEventArrayType_Load;
    *aFlags = NS_EVENT_BITS_LOAD_ERROR;
  }
  else if (aType == nsLayoutAtoms::onpaint) {
    *aArrayType = eEventArrayType_Paint;
    *aFlags = NS_EVENT_BITS_PAINT_PAINT;
  }
  else if (aType == nsLayoutAtoms::onresize) {
    *aArrayType = eEventArrayType_Paint;
    *aFlags = NS_EVENT_BITS_PAINT_RESIZE;
  }
  else if (aType == nsLayoutAtoms::onscroll) {
    *aArrayType = eEventArrayType_Paint;
    *aFlags = NS_EVENT_BITS_PAINT_SCROLL;
  } // extened this to handle IME related events
  else if (aType == nsLayoutAtoms::onpopupshowing) {
    *aArrayType = eEventArrayType_XUL; 
    *aFlags = NS_EVENT_BITS_XUL_POPUP_SHOWING;
  }
  else if (aType == nsLayoutAtoms::onpopupshown) {
    *aArrayType = eEventArrayType_XUL; 
    *aFlags = NS_EVENT_BITS_XUL_POPUP_SHOWN;
  }
  else if (aType == nsLayoutAtoms::onpopuphiding) {
    *aArrayType = eEventArrayType_XUL; 
    *aFlags = NS_EVENT_BITS_XUL_POPUP_HIDING;
  }
  else if (aType == nsLayoutAtoms::onpopuphidden) {
    *aArrayType = eEventArrayType_XUL; 
    *aFlags = NS_EVENT_BITS_XUL_POPUP_HIDDEN;
  }
  else if (aType == nsLayoutAtoms::onclose) {
    *aArrayType = eEventArrayType_XUL; 
    *aFlags = NS_EVENT_BITS_XUL_CLOSE;
  }
  else if (aType == nsLayoutAtoms::oncommand) {
    *aArrayType = eEventArrayType_XUL; 
    *aFlags = NS_EVENT_BITS_XUL_COMMAND;
  }
  else if (aType == nsLayoutAtoms::onbroadcast) {
    *aArrayType = eEventArrayType_XUL;
    *aFlags = NS_EVENT_BITS_XUL_BROADCAST;
  }
  else if (aType == nsLayoutAtoms::oncommandupdate) {
    *aArrayType = eEventArrayType_XUL;
    *aFlags = NS_EVENT_BITS_XUL_COMMAND_UPDATE;
  }
  else if (aType == nsLayoutAtoms::onoverflow) {
    *aArrayType = eEventArrayType_Scroll;
    *aFlags = NS_EVENT_BITS_SCROLLPORT_OVERFLOW;
  }
  else if (aType == nsLayoutAtoms::onunderflow) {
    *aArrayType = eEventArrayType_Scroll;
    *aFlags = NS_EVENT_BITS_SCROLLPORT_UNDERFLOW;
  }
  else if (aType == nsLayoutAtoms::onoverflowchanged) {
    *aArrayType = eEventArrayType_Scroll;
    *aFlags = NS_EVENT_BITS_SCROLLPORT_OVERFLOWCHANGED;
  }
  else if (aType == nsLayoutAtoms::ondragenter) {
    *aArrayType = eEventArrayType_Drag;
    *aFlags = NS_EVENT_BITS_DRAG_ENTER;
  }
  else if (aType == nsLayoutAtoms::ondragover) {
    *aArrayType = eEventArrayType_Drag; 
    *aFlags = NS_EVENT_BITS_DRAG_OVER;
  }
  else if (aType == nsLayoutAtoms::ondragexit) {
    *aArrayType = eEventArrayType_Drag; 
    *aFlags = NS_EVENT_BITS_DRAG_EXIT;
  }
  else if (aType == nsLayoutAtoms::ondragdrop) {
    *aArrayType = eEventArrayType_Drag; 
    *aFlags = NS_EVENT_BITS_DRAG_DROP;
  }
  else if (aType == nsLayoutAtoms::ondraggesture) {
    *aArrayType = eEventArrayType_Drag; 
    *aFlags = NS_EVENT_BITS_DRAG_GESTURE;
  }
  else if (aType == nsLayoutAtoms::onDOMSubtreeModified) {
    *aArrayType = eEventArrayType_Mutation;
    *aFlags = NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
  }
  else if (aType == nsLayoutAtoms::onDOMNodeInserted) {
    *aArrayType = eEventArrayType_Mutation;
    *aFlags = NS_EVENT_BITS_MUTATION_NODEINSERTED;
  }
  else if (aType == nsLayoutAtoms::onDOMNodeRemoved) {
    *aArrayType = eEventArrayType_Mutation;
    *aFlags = NS_EVENT_BITS_MUTATION_NODEREMOVED;
  }
  else if (aType == nsLayoutAtoms::onDOMNodeInsertedIntoDocument) {
    *aArrayType = eEventArrayType_Mutation;
    *aFlags = NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
  }
  else if (aType == nsLayoutAtoms::onDOMNodeRemovedFromDocument) {
    *aArrayType = eEventArrayType_Mutation;
    *aFlags = NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
  }
  else if (aType == nsLayoutAtoms::onDOMAttrModified) {
    *aArrayType = eEventArrayType_Mutation;
    *aFlags = NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
  }
  else if (aType == nsLayoutAtoms::onDOMCharacterDataModified) {
    *aArrayType = eEventArrayType_Mutation;
    *aFlags = NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
  }
  else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::AddEventListenerByType(nsIDOMEventListener *aListener, 
                                               const nsAString& aType,
                                               PRInt32 aFlags,
                                               nsIDOMEventGroup* aEvtGrp)
{
  PRInt32 subType;
  EventArrayType arrayType;
  nsCOMPtr<nsIAtom> atom =
           dont_AddRef(NS_NewAtom(NS_LITERAL_STRING("on") + aType));

  if (NS_OK == GetIdentifiersForType(atom, &arrayType, &subType)) {
    AddEventListener(aListener, arrayType, subType, nsnull, aFlags, aEvtGrp);
  }
  else {
    const nsPromiseFlatString& flatString = PromiseFlatString(aType); 
    nsStringKey key(flatString);
    AddEventListener(aListener, eEventArrayType_Hash, NS_EVENT_BITS_NONE, &key, aFlags, aEvtGrp);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::RemoveEventListenerByType(nsIDOMEventListener *aListener, 
                                                  const nsAString& aType,
                                                  PRInt32 aFlags,
                                                  nsIDOMEventGroup* aEvtGrp)
{
  PRInt32 subType;
  EventArrayType arrayType;
  nsCOMPtr<nsIAtom> atom =
           dont_AddRef(NS_NewAtom(NS_LITERAL_STRING("on") + aType));

  if (NS_OK == GetIdentifiersForType(atom, &arrayType, &subType)) {
    RemoveEventListener(aListener, arrayType, subType, nsnull, aFlags, aEvtGrp);
  }
  else {
    const nsPromiseFlatString& flatString = PromiseFlatString(aType); 
    nsStringKey key(flatString);
    RemoveEventListener(aListener, eEventArrayType_Hash, NS_EVENT_BITS_NONE, &key, aFlags, aEvtGrp);
  }

  return NS_OK;
}

nsListenerStruct*
nsEventListenerManager::FindJSEventListener(EventArrayType aType)
{
  nsVoidArray *listeners = GetListenersByType(aType, nsnull, PR_FALSE);
  if (listeners) {
    //Run through the listeners for this IID and see if a script listener is registered
    nsListenerStruct *ls;
    for (int i=0; i<listeners->Count(); i++) {
      ls = (nsListenerStruct*)listeners->ElementAt(i);
      if (ls->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
        return ls;
      }
    }
  }

  return nsnull;
}

nsresult
nsEventListenerManager::SetJSEventListener(nsIScriptContext *aContext, 
                                           nsISupports *aObject,
                                           nsIAtom* aName,
                                           PRBool aIsString)
{
  nsresult rv = NS_OK;
  nsListenerStruct *ls;
  PRInt32 flags;
  EventArrayType arrayType;

  NS_ENSURE_SUCCESS(GetIdentifiersForType(aName, &arrayType, &flags),
                    NS_ERROR_FAILURE);

  ls = FindJSEventListener(arrayType);

  if (nsnull == ls) {
    //If we didn't find a script listener or no listeners existed
    //create and add a new one.
    nsCOMPtr<nsIDOMScriptObjectFactory> factory =
      do_GetService(kDOMScriptObjectFactoryCID);
    NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);

    nsCOMPtr<nsIDOMEventListener> scriptListener;
    rv = factory->NewJSEventListener(aContext, aObject,
                                     getter_AddRefs(scriptListener));
    if (NS_SUCCEEDED(rv)) {
      AddEventListener(scriptListener, arrayType, NS_EVENT_BITS_NONE, nsnull,
                       NS_EVENT_FLAG_BUBBLE | NS_PRIV_EVENT_FLAG_SCRIPT, nsnull);

      ls = FindJSEventListener(arrayType);
    }
  }

  if (NS_SUCCEEDED(rv) && ls) {
    // Set flag to indicate possible need for compilation later
    if (aIsString) {
      ls->mHandlerIsString |= flags;
    }
    else {
      ls->mHandlerIsString &= ~flags;
    }

    //Set subtype flags based on event
    ls->mSubType |= flags;
  }

  return rv;
}

NS_IMETHODIMP
nsEventListenerManager::AddScriptEventListener(nsIScriptContext* aContext,
                                               nsISupports *aObject,
                                               nsIAtom *aName,
                                               const nsAString& aBody,
                                               PRBool aDeferCompilation)
{
  nsresult rv;

  if (!aDeferCompilation) {
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));

    JSContext *cx = (JSContext *)aContext->GetNativeContext();

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

    rv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx), aObject,
                         NS_GET_IID(nsISupports), getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *scriptObject = nsnull;

    rv = holder->GetJSObject(&scriptObject);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner =
      do_QueryInterface(aObject);

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
        rv = handlerOwner->CompileEventHandler(aContext, scriptObject, aName,
                                               aBody, &handler);
      }
      else {
        rv = aContext->CompileEventHandler(scriptObject, aName, aBody,
                                           (handlerOwner != nsnull),
                                           &handler);
      }
      if (NS_FAILED(rv)) return rv;
    }
  }

  return SetJSEventListener(aContext, aObject, aName, aDeferCompilation);
}

nsresult
nsEventListenerManager::RemoveScriptEventListener(nsIAtom *aName)
{
  nsresult result = NS_OK;
  nsListenerStruct *ls;
  PRInt32 flags;
  EventArrayType arrayType;

  NS_ENSURE_SUCCESS(GetIdentifiersForType(aName, &arrayType, &flags),
                    NS_ERROR_FAILURE);
  ls = FindJSEventListener(arrayType);

  if (ls) {
    ls->mSubType &= ~flags;
    if (ls->mSubType == NS_EVENT_BITS_NONE) {
      NS_RELEASE(ls->mListener);

      //Get the listeners array so we can remove ourselves from it
      nsVoidArray* listeners;
      listeners = GetListenersByType(arrayType, nsnull, PR_FALSE);
      NS_ENSURE_TRUE(listeners, NS_ERROR_FAILURE);
      listeners->RemoveElement((void*)ls);
      PR_DELETE(ls);
    }
  }

  return result;
}

jsval
nsEventListenerManager::sAddListenerID = JSVAL_VOID;

NS_IMETHODIMP
nsEventListenerManager::RegisterScriptEventListener(nsIScriptContext *aContext,
                                                    nsISupports *aObject, 
                                                    nsIAtom *aName)
{
  // Check that we have access to set an event listener. Prevents
  // snooping attacks across domains by setting onkeypress handlers,
  // for instance.
  // You'd think it'd work just to get the JSContext from aContext,
  // but that's actually the JSContext whose private object parents
  // the object in aObject.
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
      return rv;
  JSContext *cx;
  if (NS_FAILED(stack->Peek(&cx)))
      return nsnull;

  JSContext *current_cx = (JSContext *)aContext->GetNativeContext();

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
  rv = xpc->WrapNative(current_cx, ::JS_GetGlobalObject(current_cx), aObject,
                       NS_GET_IID(nsISupports), getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *jsobj = nsnull;

  rv = holder->GetJSObject(&jsobj);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
      return rv;

  nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(aObject);

  if (sAddListenerID == JSVAL_VOID) {
    sAddListenerID = STRING_TO_JSVAL(::JS_InternString(cx, "addEventListener"));
  }

  if (NS_FAILED(rv = securityManager->CheckPropertyAccess(cx, jsobj,
                "EventTarget", sAddListenerID,
                nsIXPCSecurityManager::ACCESS_SET_PROPERTY))) {
      // XXX set pending exception on the native call context?
    return rv;
  }

  return SetJSEventListener(aContext, aObject, aName, PR_FALSE);
}

nsresult
nsEventListenerManager::CompileScriptEventListener(nsIScriptContext *aContext, 
                                                   nsISupports *aObject, 
                                                   nsIAtom *aName,
                                                   PRBool *aDidCompile)
{
  nsresult rv = NS_OK;
  nsListenerStruct *ls;
  PRInt32 subType;
  EventArrayType arrayType;

  *aDidCompile = PR_FALSE;

  rv = GetIdentifiersForType(aName, &arrayType, &subType);
  NS_ENSURE_SUCCESS(rv, rv);

  ls = FindJSEventListener(arrayType);

  if (!ls) {
    //nothing to compile
    return NS_OK;
  }

  if (ls->mHandlerIsString & subType) {
    rv = CompileEventHandlerInternal(aContext, aObject, aName, ls, subType);
  }

  // Set *aDidCompile to true even if we didn't really compile
  // anything right now, if we get here it means that this event
  // handler has been compiled at some point, that's good enough for
  // us.

  *aDidCompile = PR_TRUE;

  return rv;
}

nsresult
nsEventListenerManager::CompileEventHandlerInternal(nsIScriptContext *aContext,
                                                    nsISupports *aObject,
                                                    nsIAtom *aName,
                                                    nsListenerStruct *aListenerStruct,
                                                    PRUint32 aSubType)
{
  nsresult result = NS_OK;

  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));

  JSContext *cx = (JSContext *)aContext->GetNativeContext();

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  result = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx), aObject,
                           NS_GET_IID(nsISupports), getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(result, result);

  JSObject *jsobj = nsnull;

  result = holder->GetJSObject(&jsobj);
  NS_ENSURE_SUCCESS(result, result);

  nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner =
    do_QueryInterface(aObject);
  void* handler = nsnull;

  if (handlerOwner) {
    result = handlerOwner->GetCompiledEventHandler(aName, &handler);
    if (NS_SUCCEEDED(result) && handler) {
      result = aContext->BindCompiledEventHandler(jsobj, aName, handler);
      aListenerStruct->mHandlerIsString &= ~aSubType;
    }
  }

  if (aListenerStruct->mHandlerIsString & aSubType) {
    // This should never happen for anything but content
    // XXX I don't like that we have to reference content
    // from here. The alternative is to store the event handler
    // string on the JS object itself.
    nsCOMPtr<nsIContent> content = do_QueryInterface(aObject);
    NS_ASSERTION(content, "only content should have event handler attributes");
    if (content) {
      nsAutoString handlerBody;
      result = content->GetAttr(kNameSpaceID_None, aName, handlerBody);

      if (NS_SUCCEEDED(result)) {
        if (handlerOwner) {
          // Always let the handler owner compile the event
          // handler, as it may want to use a special
          // context or scope object.
          result = handlerOwner->CompileEventHandler(aContext, jsobj, aName,
                                                     handlerBody, &handler);
        }
        else {
          result = aContext->CompileEventHandler(jsobj, aName, handlerBody,
                                                 (handlerOwner != nsnull),
                                                 &handler);
        }

        if (NS_SUCCEEDED(result)) {
          aListenerStruct->mHandlerIsString &= ~aSubType;
        }
      }
    }
  }

  return result;
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
        nsCOMPtr<nsISupports> target;
        nsCOMPtr<nsIScriptContext> scriptCX;
        result = jslistener->GetEventTarget(getter_AddRefs(scriptCX),
                                            getter_AddRefs(target));

        if (NS_SUCCEEDED(result)) {
          nsAutoString eventString;
          if (NS_SUCCEEDED(aDOMEvent->GetType(eventString))) {
            nsCOMPtr<nsIAtom> atom = dont_AddRef(NS_NewAtom(NS_LITERAL_STRING("on") + eventString));

            result = CompileEventHandlerInternal(scriptCX, target, atom,
                                                 aListenerStruct, aSubType);
          }
        }
      }
    }
  }

  // nsCxPusher will automatically push and pop the current cx onto the
  // context stack
  nsCxPusher pusher(aCurrentTarget);

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

  if (aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) {
    return ret;
  }

  if (aFlags & NS_EVENT_FLAG_INIT) {
    aFlags |= (NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE);
  }
  //Set the value of the internal PreventDefault flag properly based on aEventStatus
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    aEvent->flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }
  PRUint16 currentGroup = aFlags & NS_EVENT_FLAG_SYSTEM_EVENT;

  /* Without this addref, certain events, notably ones bound to
     keys which cause window deletion, can destroy this object
     before we're ready. */
  nsCOMPtr<nsIEventListenerManager> kungFuDeathGrip(this);
  nsAutoString empty;
  nsVoidArray *listeners;

  switch(aEvent->message) {
    case NS_USER_DEFINED_EVENT:
      listeners = GetListenersByType(eEventArrayType_Hash, aEvent->userType, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_SUCCEEDED(ret)) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);
            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              ret = HandleEventSubType(ls, *aDOMEvent, aCurrentTarget, NS_EVENT_BITS_NONE, aFlags);
            }
          }
        }
      }
      break;

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
      listeners = GetListenersByType(eEventArrayType_Mouse, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);
            
            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMMouseListener> mouseListener (do_QueryInterface(ls->mListener));
              if (mouseListener) {
                switch(aEvent->message) {
                  case NS_MOUSE_LEFT_BUTTON_DOWN:
                  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
                  case NS_MOUSE_RIGHT_BUTTON_DOWN:
                    ret = mouseListener->MouseDown(*aDOMEvent);
                    break;
                  case NS_MOUSE_LEFT_BUTTON_UP:
                  case NS_MOUSE_MIDDLE_BUTTON_UP:
                  case NS_MOUSE_RIGHT_BUTTON_UP:
                    ret = mouseListener->MouseUp(*aDOMEvent);
                    break;
                  case NS_MOUSE_LEFT_CLICK:
                  case NS_MOUSE_MIDDLE_CLICK:
                  case NS_MOUSE_RIGHT_CLICK:
                    ret = mouseListener->MouseClick(*aDOMEvent);
                    break;
                  case NS_MOUSE_LEFT_DOUBLECLICK:
                  case NS_MOUSE_MIDDLE_DOUBLECLICK:
                  case NS_MOUSE_RIGHT_DOUBLECLICK:
                    ret = mouseListener->MouseDblClick(*aDOMEvent);
                    break;
                  case NS_MOUSE_ENTER_SYNTH:
                    ret = mouseListener->MouseOver(*aDOMEvent);
                    break;
                  case NS_MOUSE_EXIT_SYNTH:
                    ret = mouseListener->MouseOut(*aDOMEvent);
                    break;
                  default:
                    break;
                }
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
      listeners = GetListenersByType(eEventArrayType_MouseMotion, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMMouseMotionListener> mousemlistener (do_QueryInterface(ls->mListener));
              if (mousemlistener) {
                switch(aEvent->message) {
                  case NS_MOUSE_MOVE:
                    ret = mousemlistener->MouseMove(*aDOMEvent);
                    break;
                  default:
                    break;
                }
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

    case NS_CONTEXTMENU:
    case NS_CONTEXTMENU_KEY:
      listeners = GetListenersByType(eEventArrayType_ContextMenu, nsnull, PR_FALSE);
      if (listeners) {
               
        // If we're here because of the key-equiv for showing context menus, we
        // have to reset the event target to the currently focused element. Get it
        // from the focus controller.
        nsCOMPtr<nsIDOMEventTarget> currentTarget ( aCurrentTarget );
        nsCOMPtr<nsIDOMElement> currentFocus;
        nsCOMPtr<nsIDocument> doc;
        nsCOMPtr<nsIPresShell> shell;
        if ( aEvent->message == NS_CONTEXTMENU_KEY ) {
          aPresContext->GetShell(getter_AddRefs(shell));
          shell->GetDocument(getter_AddRefs(doc));
          if ( doc ) {
            nsCOMPtr<nsIScriptGlobalObject> scriptObj;
            doc->GetScriptGlobalObject(getter_AddRefs(scriptObj));
            if ( scriptObj ) {
              nsCOMPtr<nsPIDOMWindow> privWindow = do_QueryInterface(scriptObj);
              if ( privWindow ) {
                nsCOMPtr<nsIFocusController> focusController;
                privWindow->GetRootFocusController(getter_AddRefs(focusController));
                if ( focusController )
                  focusController->GetFocusedElement(getter_AddRefs(currentFocus));
              }
            }
          }
        }

        if (nsnull == *aDOMEvent) {        
          // If we're here because of the key-equiv for showing context menus, we
          // have to twiddle with the NS event to make sure the context menu comes
          // up in the upper left of the relevant content area before we create
          // the DOM event. Since we never call InitMouseEvent() on the event, 
          // the client X/Y will be 0,0. We can make use of that if the widget is null.
          if ( aEvent->message == NS_CONTEXTMENU_KEY )
            NS_IF_RELEASE(((nsGUIEvent*)aEvent)->widget);   // nulls out widget                          

          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        
        if (NS_OK == ret) {
          // update the target
          if ( currentFocus ) {
            // Reset event coordinates relative to focused frame in view
            nsPoint targetPt;
            GetCoordinatesFor(currentFocus, aPresContext, shell, targetPt);
            aEvent->point.x += targetPt.x - aEvent->refPoint.x;
            aEvent->point.y += targetPt.y - aEvent->refPoint.y;
            aEvent->refPoint.x = targetPt.x;
            aEvent->refPoint.y = targetPt.y;

            currentTarget = do_QueryInterface(currentFocus);
            nsCOMPtr<nsIPrivateDOMEvent> pEvent ( do_QueryInterface(*aDOMEvent) );
            pEvent->SetTarget ( currentTarget );
          }
          
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMContextMenuListener> contextMenuListener (do_QueryInterface(ls->mListener));
              if (contextMenuListener) {
                switch(aEvent->message) {
                  case NS_CONTEXTMENU:
                  case NS_CONTEXTMENU_KEY:
                    ret = contextMenuListener->ContextMenu(*aDOMEvent);
                    break;
                  default:
                    break;
                }
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_CONTEXTMENU:
                  case NS_CONTEXTMENU_KEY:
                    subType = NS_EVENT_BITS_CONTEXT_MENU;
                    if (ls->mSubType & NS_EVENT_BITS_CONTEXT_MENU) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  default:
                    break;
                }
                if (correctSubType || ls->mSubType == NS_EVENT_BITS_NONE) {
                  ret = HandleEventSubType(ls, *aDOMEvent, currentTarget, subType, aFlags);
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
      listeners = GetListenersByType(eEventArrayType_Composition, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent,aPresContext,empty,aEvent);
        }
        if (NS_OK == ret) {
          //XXX These were text listeners, seems like they should be composition
          for(int i=0; !mListenersRemoved && listeners && i<listeners->Count();i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMCompositionListener> compositionListener (do_QueryInterface(ls->mListener));
              if (compositionListener) {
                switch (aEvent->message) {
                  case NS_COMPOSITION_START:
                    ret = compositionListener->HandleStartComposition(*aDOMEvent);
                    break;
                  case NS_COMPOSITION_END: 
                    ret = compositionListener->HandleEndComposition(*aDOMEvent);
                    break;
                  case NS_COMPOSITION_QUERY:
                    ret = compositionListener->HandleQueryComposition(*aDOMEvent);
                    break;
                  case NS_RECONVERSION_QUERY:
                    ret = compositionListener->HandleQueryReconversion(*aDOMEvent);
                    break;
                }
              }
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
      listeners = GetListenersByType(eEventArrayType_Text, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent,aPresContext,empty,aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMTextListener> textListener (do_QueryInterface(ls->mListener));
              if (textListener) {
                ret = textListener->HandleText(*aDOMEvent);
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
      listeners = GetListenersByType(eEventArrayType_Key, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMKeyListener> keyListener (do_QueryInterface(ls->mListener));
              if (keyListener) {
                switch(aEvent->message) {
                  case NS_KEY_UP:
                    ret = keyListener->KeyUp(*aDOMEvent);
                    break;
                  case NS_KEY_DOWN:
                    ret = keyListener->KeyDown(*aDOMEvent);
                    break;
                  case NS_KEY_PRESS:
                    ret = keyListener->KeyPress(*aDOMEvent);
                    break;
                  default:
                    break;
                }
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
      listeners = GetListenersByType(eEventArrayType_Focus, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMFocusListener> focusListener (do_QueryInterface(ls->mListener));
              if (focusListener) {
                switch(aEvent->message) {
                  case NS_FOCUS_CONTENT:
                    ret = focusListener->Focus(*aDOMEvent);
                    break;
                  case NS_BLUR_CONTENT:
                    ret = focusListener->Blur(*aDOMEvent);
                    break;
                  default:
                    break;
                }
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
      listeners = GetListenersByType(eEventArrayType_Form, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMFormListener> formListener(do_QueryInterface(ls->mListener));
              if (formListener) {
                switch(aEvent->message) {
                  case NS_FORM_SUBMIT:
                    ret = formListener->Submit(*aDOMEvent);
                    break;
                  case NS_FORM_RESET:
                    ret = formListener->Reset(*aDOMEvent);
                    break;
                  case NS_FORM_CHANGE:
                    ret = formListener->Change(*aDOMEvent);
                    break;
                  case NS_FORM_SELECTED:
                    ret = formListener->Select(*aDOMEvent);
                    break;
                  case NS_FORM_INPUT:
                    ret = formListener->Input(*aDOMEvent);
                    break;
                  default:
                    break;
                }
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
    case NS_SCRIPT_LOAD:
    case NS_SCRIPT_ERROR:
      listeners = GetListenersByType(eEventArrayType_Load, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMLoadListener> loadListener(do_QueryInterface(ls->mListener));
              if (loadListener) {
                switch(aEvent->message) {
                  case NS_PAGE_LOAD:
                  case NS_IMAGE_LOAD:
                  case NS_SCRIPT_LOAD: 
                    ret = loadListener->Load(*aDOMEvent);
                    break;
                  case NS_PAGE_UNLOAD:
                    ret = loadListener->Unload(*aDOMEvent);
                    break;
                  case NS_IMAGE_ERROR:
                  case NS_SCRIPT_ERROR:
                    ret = loadListener->Error(*aDOMEvent);
                  default:
                    break;
                }
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_PAGE_LOAD:
                  case NS_IMAGE_LOAD:
                  case NS_SCRIPT_LOAD:
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
      listeners = GetListenersByType(eEventArrayType_Paint, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMPaintListener> paintListener(do_QueryInterface(ls->mListener));
              if (paintListener) {
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
      listeners = GetListenersByType(eEventArrayType_Drag, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }

        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *dragStruct = (nsListenerStruct*)listeners->ElementAt(i);

            if (dragStruct->mFlags & aFlags && dragStruct->mGroupFlags == currentGroup) {
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
      listeners = GetListenersByType(eEventArrayType_Scroll, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMScrollListener> scrollListener(do_QueryInterface(ls->mListener));
              if (scrollListener) {
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
    case NS_XUL_POPUP_SHOWING:
    case NS_XUL_POPUP_SHOWN:
    case NS_XUL_POPUP_HIDING:
    case NS_XUL_POPUP_HIDDEN:
    case NS_XUL_CLOSE:
    case NS_XUL_COMMAND:
    case NS_XUL_BROADCAST:
    case NS_XUL_COMMAND_UPDATE:
      listeners = GetListenersByType(eEventArrayType_XUL, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMUIEvent(aDOMEvent, aPresContext, empty, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMXULListener> xulListener(do_QueryInterface(ls->mListener));
              if (xulListener) {
                switch(aEvent->message) {
                  case NS_XUL_POPUP_SHOWING:
                    ret = xulListener->PopupShowing(*aDOMEvent);
                    break;
                  case NS_XUL_POPUP_SHOWN:
                    ret = xulListener->PopupShown(*aDOMEvent);
                    break;
                  case NS_XUL_POPUP_HIDING:
                    ret = xulListener->PopupHiding(*aDOMEvent);
                    break;
                  case NS_XUL_POPUP_HIDDEN:
                    ret = xulListener->PopupHidden(*aDOMEvent);
                    break;
                  case NS_XUL_CLOSE:
                    ret = xulListener->Close(*aDOMEvent);
                    break;
                  case NS_XUL_COMMAND:
                    ret = xulListener->Command(*aDOMEvent);
                    break;
                  case NS_XUL_BROADCAST:
                    ret = xulListener->Broadcast(*aDOMEvent);
                    break;
                  case NS_XUL_COMMAND_UPDATE:
                    ret = xulListener->CommandUpdate(*aDOMEvent);
                    break;
                  default:
                    break;
                }
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_XUL_POPUP_SHOWING:
                    subType = NS_EVENT_BITS_XUL_POPUP_SHOWING;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_POPUP_SHOWING) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_POPUP_SHOWN:
                    subType = NS_EVENT_BITS_XUL_POPUP_SHOWN;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_POPUP_SHOWN) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_POPUP_HIDING:
                    subType = NS_EVENT_BITS_XUL_POPUP_HIDING;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_POPUP_HIDING) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_POPUP_HIDDEN:
                    subType = NS_EVENT_BITS_XUL_POPUP_HIDDEN;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_POPUP_HIDDEN) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_CLOSE:
                    subType = NS_EVENT_BITS_XUL_CLOSE;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_CLOSE) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_XUL_COMMAND:
                    subType = NS_EVENT_BITS_XUL_COMMAND;
                    if (ls->mSubType & NS_EVENT_BITS_XUL_COMMAND) {
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

    case NS_MUTATION_SUBTREEMODIFIED:
    case NS_MUTATION_NODEINSERTED:
    case NS_MUTATION_NODEREMOVED:
    case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
    case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
    case NS_MUTATION_ATTRMODIFIED:
    case NS_MUTATION_CHARACTERDATAMODIFIED:
      listeners = GetListenersByType(eEventArrayType_Mutation, nsnull, PR_FALSE);
      if (listeners) {
        if (nsnull == *aDOMEvent) {
          ret = NS_NewDOMMutationEvent(aDOMEvent, aPresContext, aEvent);
        }
        if (NS_OK == ret) {
          for (int i=0; !mListenersRemoved && listeners && i<listeners->Count(); i++) {
            nsListenerStruct *ls = (nsListenerStruct*)listeners->ElementAt(i);

            if (ls->mFlags & aFlags && ls->mGroupFlags == currentGroup) {
              nsCOMPtr<nsIDOMMutationListener> mutationListener = do_QueryInterface(ls->mListener);
              if (mutationListener) {
                switch(aEvent->message) {
                  case NS_MUTATION_SUBTREEMODIFIED:
                    ret = mutationListener->SubtreeModified(*aDOMEvent);
                    break;
                  case NS_MUTATION_NODEINSERTED:
                    ret = mutationListener->NodeInserted(*aDOMEvent);
                    break;
                  case NS_MUTATION_NODEREMOVED:
                    ret = mutationListener->NodeRemoved(*aDOMEvent);
                    break;
                  case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
                    ret = mutationListener->NodeInsertedIntoDocument(*aDOMEvent);
                    break;
                  case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
                    ret = mutationListener->NodeRemovedFromDocument(*aDOMEvent);
                    break;
                  case NS_MUTATION_ATTRMODIFIED:
                    ret = mutationListener->AttrModified(*aDOMEvent);
                    break;
                  case NS_MUTATION_CHARACTERDATAMODIFIED:
                    ret = mutationListener->CharacterDataModified(*aDOMEvent);
                    break;
                  default:
                    break;
                }
              }
              else {
                PRBool correctSubType = PR_FALSE;
                PRUint32 subType = 0;
                switch(aEvent->message) {
                  case NS_MUTATION_SUBTREEMODIFIED:
                    subType = NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
                    if (ls->mSubType & NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MUTATION_NODEINSERTED:
                    subType = NS_EVENT_BITS_MUTATION_NODEINSERTED;
                    if (ls->mSubType & NS_EVENT_BITS_MUTATION_NODEINSERTED) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MUTATION_NODEREMOVED:
                    subType = NS_EVENT_BITS_MUTATION_NODEREMOVED;
                    if (ls->mSubType & NS_EVENT_BITS_MUTATION_NODEREMOVED) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
                    subType = NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
                    if (ls->mSubType & NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
                    subType = NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
                    if (ls->mSubType & NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MUTATION_ATTRMODIFIED:
                    subType = NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
                    if (ls->mSubType & NS_EVENT_BITS_MUTATION_ATTRMODIFIED) {
                      correctSubType = PR_TRUE;
                    }
                    break;
                  case NS_MUTATION_CHARACTERDATAMODIFIED:
                    subType = NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
                    if (ls->mSubType & NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED) {
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

  // XXX (NS_OK != ret) is going away,
  // (aEvent->flags & NS_EVENT_FLAG_NO_DEFAULT) is correct
  if ((NS_OK != ret) ||
    (aEvent->flags & NS_EVENT_FLAG_NO_DEFAULT)) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }

  return NS_OK;
}

/**
* Creates a DOM event
*/

NS_IMETHODIMP
nsEventListenerManager::CreateEvent(nsIPresContext* aPresContext,
                                    nsEvent* aEvent,
                                    const nsAString& aEventType,
                                    nsIDOMEvent** aDOMEvent)
{
  *aDOMEvent = nsnull;

  nsAutoString str(aEventType);
  if (!aEvent && !str.EqualsIgnoreCase("MouseEvents") && !str.EqualsIgnoreCase("KeyEvents") &&
      !str.EqualsIgnoreCase("HTMLEvents") && !str.EqualsIgnoreCase("MutationEvents") &&
      !str.EqualsIgnoreCase("MouseScrollEvents") && !str.EqualsIgnoreCase("Events")) {
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if ((aEvent && aEvent->eventStructType == NS_MUTATION_EVENT) ||
      (!aEvent && str.EqualsIgnoreCase("MutationEvents")))
    return NS_NewDOMMutationEvent(aDOMEvent, aPresContext, aEvent);
  return NS_NewDOMUIEvent(aDOMEvent, aPresContext, aEventType, aEvent);
}

/**
* Captures all events designated for descendant objects at the current level.
* @param an event listener
*/

NS_IMETHODIMP
nsEventListenerManager::CaptureEvent(PRInt32 aEventTypes)
{
  return FlipCaptureBit(aEventTypes, PR_TRUE);
}             

/**
* Releases all events designated for descendant objects at the current level.
* @param an event listener
*/

NS_IMETHODIMP
nsEventListenerManager::ReleaseEvent(PRInt32 aEventTypes)
{
  return FlipCaptureBit(aEventTypes, PR_FALSE);
}

nsresult nsEventListenerManager::FlipCaptureBit(PRInt32 aEventTypes, PRBool aInitCapture)
{
  EventArrayType arrayType;
  nsListenerStruct *ls;

  if (aEventTypes & nsIDOMNSEvent::MOUSEDOWN) {
    arrayType = eEventArrayType_Mouse;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEDOWN; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEDOWN;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::MOUSEUP) {
    arrayType = eEventArrayType_Mouse;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEUP; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEUP;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::MOUSEOVER) {
    arrayType = eEventArrayType_Mouse;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEOVER; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEOVER;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::MOUSEOUT) {
    arrayType = eEventArrayType_Mouse;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEOUT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEOUT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::MOUSEMOVE) {
    arrayType = eEventArrayType_MouseMotion;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSEMOTION_MOUSEMOVE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::CLICK) {
    arrayType = eEventArrayType_Mouse;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_CLICK; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_CLICK;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::DBLCLICK) {
    arrayType = eEventArrayType_Mouse;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_DBLCLICK; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_DBLCLICK;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::KEYDOWN) {
    arrayType = eEventArrayType_Key;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_KEY_KEYDOWN; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_KEY_KEYDOWN;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::KEYUP) {
    arrayType = eEventArrayType_Key;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_KEY_KEYUP; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_KEY_KEYUP;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::KEYPRESS) {
    arrayType = eEventArrayType_Key;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_KEY_KEYPRESS; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_KEY_KEYPRESS;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::DRAGDROP) {
    arrayType = eEventArrayType_Drag;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_DRAG_ENTER; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_DRAG_ENTER;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  /*if (aEventTypes & nsIDOMNSEvent::MOUSEDRAG) {
    arrayType = kIDOMMouseListenerarrayType;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_MOUSE_MOUSEDOWN; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_MOUSE_MOUSEDOWN;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }*/
  if (aEventTypes & nsIDOMNSEvent::FOCUS) {
    arrayType = eEventArrayType_Focus;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FOCUS_FOCUS; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FOCUS_FOCUS;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::BLUR) {
    arrayType = eEventArrayType_Focus;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FOCUS_BLUR; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FOCUS_BLUR;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::SELECT) {
    arrayType = eEventArrayType_Form;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_SELECT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_SELECT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::CHANGE) {
    arrayType = eEventArrayType_Form;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_CHANGE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_CHANGE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::RESET) {
    arrayType = eEventArrayType_Form;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_RESET; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_RESET;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::SUBMIT) {
    arrayType = eEventArrayType_Form;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_FORM_SUBMIT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_FORM_SUBMIT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::LOAD) {
    arrayType = eEventArrayType_Load;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_LOAD; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_LOAD;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::UNLOAD) {
    arrayType = eEventArrayType_Load;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_UNLOAD; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_UNLOAD;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::ABORT) {
    arrayType = eEventArrayType_Load;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_ABORT; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_ABORT;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::ERROR) {
    arrayType = eEventArrayType_Load;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_LOAD_ERROR; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_LOAD_ERROR;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::RESIZE) {
    arrayType = eEventArrayType_Paint;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_PAINT_RESIZE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_PAINT_RESIZE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  if (aEventTypes & nsIDOMNSEvent::SCROLL) {
    arrayType = eEventArrayType_Scroll;
    ls = FindJSEventListener(arrayType);
    if (ls) {
      if (aInitCapture) ls->mSubTypeCapture |= NS_EVENT_BITS_PAINT_RESIZE; 
      else ls->mSubTypeCapture &= ~NS_EVENT_BITS_PAINT_RESIZE;
      ls->mFlags |= NS_EVENT_FLAG_CAPTURE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::SetListenerTarget(nsISupports* aTarget)
{
  //WEAK reference, must be set back to nsnull when done
  mTarget = aTarget;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::GetSystemEventGroupLM(nsIDOMEventGroup **aGroup)
{
  if (!gSystemEventGroup) {
    nsresult result;
    nsCOMPtr<nsIDOMEventGroup> group(do_CreateInstance(kDOMEventGroupCID,&result));
    if (NS_FAILED(result))
      return result;

    gSystemEventGroup = group;
    NS_ADDREF(gSystemEventGroup);
  }

  *aGroup = gSystemEventGroup;
  NS_ADDREF(*aGroup);
  return NS_OK;
}

nsresult
nsEventListenerManager::GetDOM2EventGroup(nsIDOMEventGroup **aGroup)
{
  if (!gDOM2EventGroup) {
    nsresult result;
    nsCOMPtr<nsIDOMEventGroup> group(do_CreateInstance(kDOMEventGroupCID,&result));
    if (NS_FAILED(result))
      return result;

    gDOM2EventGroup = group;
    NS_ADDREF(gDOM2EventGroup);
  }

  *aGroup = gDOM2EventGroup;
  NS_ADDREF(*aGroup);
  return NS_OK;
}

// nsIDOMEventTarget interface
NS_IMETHODIMP 
nsEventListenerManager::AddEventListener(const nsAString& aType, 
                                         nsIDOMEventListener* aListener, 
                                         PRBool aUseCapture)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  return AddEventListenerByType(aListener, aType, flags, nsnull);
}

NS_IMETHODIMP 
nsEventListenerManager::RemoveEventListener(const nsAString& aType, 
                                            nsIDOMEventListener* aListener, 
                                            PRBool aUseCapture)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  
  return RemoveEventListenerByType(aListener, aType, flags, nsnull);
}

NS_IMETHODIMP
nsEventListenerManager::DispatchEvent(nsIDOMEvent* aEvent, PRBool *_retval)
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

        nsCOMPtr<nsIPresShell> shell;
        document->GetShellAt(0, getter_AddRefs(shell));

        // Retrieve the context
        nsCOMPtr<nsIPresContext> aPresContext;
        shell->GetPresContext(getter_AddRefs(aPresContext));

        nsCOMPtr<nsIEventStateManager> esm;
        if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(esm)))) {
          return esm->DispatchNewEvent(mTarget, aEvent, _retval);
        }
      }
    }
  }
  return NS_ERROR_FAILURE;
}

// nsIDOM3EventTarget interface
NS_IMETHODIMP 
nsEventListenerManager::AddGroupedEventListener(const nsAString& aType, 
                                                nsIDOMEventListener* aListener, 
                                                PRBool aUseCapture,
                                                nsIDOMEventGroup* aEvtGrp)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  return AddEventListenerByType(aListener, aType, flags, aEvtGrp);
}

NS_IMETHODIMP 
nsEventListenerManager::RemoveGroupedEventListener(const nsAString& aType, 
                                            nsIDOMEventListener* aListener, 
                                            PRBool aUseCapture,
                                            nsIDOMEventGroup* aEvtGrp)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  
  return RemoveEventListenerByType(aListener, aType, flags, aEvtGrp);
}

NS_IMETHODIMP
nsEventListenerManager::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEventListenerManager::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
nsEventListenerManager::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool noDefault;
  return DispatchEvent(aEvent, &noDefault);
}

NS_IMETHODIMP
nsEventListenerManager::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  return GetSystemEventGroupLM(aGroup);
}

void nsEventListenerManager::GetCoordinatesFor(nsIDOMElement *aCurrentEl, 
                                               nsIPresContext *aPresContext,
                                               nsIPresShell *aPresShell, 
                                               nsPoint& aTargetPt)
{
  nsCOMPtr<nsIContent> focusedContent(do_QueryInterface(aCurrentEl));
  nsIFrame *frame = nsnull;
  aPresShell->GetPrimaryFrameFor(focusedContent, &frame);
  if (frame) {
    nsIView *view;
    frame->GetOffsetFromView(aPresContext, aTargetPt, &view);
    float t2p;
    aPresContext->GetTwipsToPixels(&t2p);

    // Start context menu down and to the right from top left of frame
    // use the lineheight. This is a good distance to move the context
    // menu away from the top left corner of the frame. If we always 
    // used the frame height, the context menu could end up far away,
    // for example when we're focused on linked images.
    nsCOMPtr<nsIViewManager> vm;
    aPresShell->GetViewManager(getter_AddRefs(vm));
    if (vm) {
      nsIScrollableView* scrollableView = nsnull;
      vm->GetRootScrollableView(&scrollableView);
      nscoord extraDistance;
      if (scrollableView) {
        scrollableView->GetLineHeight(&extraDistance);
      }
      else {
        // No scrollable view, use height of frame as fallback
        nsRect frameRect;
        frame->GetRect(frameRect);
        extraDistance = frameRect.height;
      }
      aTargetPt.x += extraDistance;
      aTargetPt.y += extraDistance;
    }
     // Convert to pixels using that scale
    aTargetPt.x = NSTwipsToIntPixels(aTargetPt.x, t2p);
    aTargetPt.y = NSTwipsToIntPixels(aTargetPt.y, t2p);
  }
}

nsresult
NS_NewEventListenerManager(nsIEventListenerManager** aInstancePtrResult) 
{
  nsIEventListenerManager* l = new nsEventListenerManager();

  if (!l) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return CallQueryInterface(l, aInstancePtrResult);
}


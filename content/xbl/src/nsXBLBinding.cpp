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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): Brendan Eich (brendan@mozilla.org)
 */

#include "nsCOMPtr.h"
#include "nsIXBLBinding.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIDOMEventReceiver.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXMLContent.h"
#include "nsIXULContent.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsSupportsArray.h"
#include "nsINameSpace.h"
#include "nsJSUtils.h"
#include "nsIJSRuntimeService.h"
#include "nsXBLService.h"

// Event listeners
#include "nsIEventListenerManager.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMMenuListener.h"
#include "nsIDOMDragListener.h"

#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"

#include "nsIXBLPrototypeHandler.h"

#include "nsXBLKeyHandler.h"
#include "nsXBLFocusHandler.h"
#include "nsXBLMouseHandler.h"
#include "nsXBLMouseMotionHandler.h"
#include "nsXBLXULHandler.h"
#include "nsXBLScrollHandler.h"
#include "nsXBLFormHandler.h"
#include "nsXBLDragHandler.h"
#include "nsXBLLoadHandler.h"

#include "nsXBLBinding.h"

// Static IIDs/CIDs. Try to minimize these.
static char kNameSpaceSeparator = ':';

// Helper classes

/***********************************************************************/
//
// The JS class for XBLBinding
//
PR_STATIC_CALLBACK(void)
XBLFinalize(JSContext *cx, JSObject *obj)
{
  nsXBLJSClass* c = NS_STATIC_CAST(nsXBLJSClass*, ::JS_GetClass(cx, obj));
  c->Drop();
}

nsXBLJSClass::nsXBLJSClass(const nsCString& aClassName)
{
  memset(this, 0, sizeof(nsXBLJSClass));
  next = prev = NS_STATIC_CAST(JSCList*, this);
  name = nsXPIDLCString::Copy(aClassName);
  addProperty = delProperty = setProperty = getProperty = ::JS_PropertyStub;
  enumerate = ::JS_EnumerateStub;
  resolve = ::JS_ResolveStub;
  convert = ::JS_ConvertStub;
  finalize = XBLFinalize;
}

nsrefcnt
nsXBLJSClass::Destroy()
{
  NS_ASSERTION(next == prev && prev == NS_STATIC_CAST(JSCList*, this),
               "referenced nsXBLJSClass is on LRU list already!?");

  if (nsXBLService::gClassTable) {
    nsCStringKey key(name);
    (nsXBLService::gClassTable)->Remove(&key);
  }

  if (nsXBLService::gClassLRUListLength >= nsXBLService::gClassLRUListQuota) {
    // Over LRU list quota, just unhash and delete this class.
    delete this;
  } else {
    // Put this most-recently-used class on end of the LRU-sorted freelist.
    JSCList* mru = NS_STATIC_CAST(JSCList*, this);
    JS_APPEND_LINK(mru, &nsXBLService::gClassLRUList);
    nsXBLService::gClassLRUListLength++;
  }

  return 0;
}

// Static initialization
PRUint32 nsXBLBinding::gRefCnt = 0;
 
nsIAtom* nsXBLBinding::kXULTemplateAtom = nsnull;
nsIAtom* nsXBLBinding::kXULObservesAtom = nsnull;

nsIAtom* nsXBLBinding::kContentAtom = nsnull;
nsIAtom* nsXBLBinding::kImplementationAtom = nsnull;
nsIAtom* nsXBLBinding::kHandlersAtom = nsnull;
nsIAtom* nsXBLBinding::kExcludesAtom = nsnull;
nsIAtom* nsXBLBinding::kIncludesAtom = nsnull;
nsIAtom* nsXBLBinding::kInheritsAtom = nsnull;
nsIAtom* nsXBLBinding::kEventAtom = nsnull;
nsIAtom* nsXBLBinding::kPhaseAtom = nsnull;
nsIAtom* nsXBLBinding::kExtendsAtom = nsnull;
nsIAtom* nsXBLBinding::kActionAtom = nsnull;
nsIAtom* nsXBLBinding::kMethodAtom = nsnull;
nsIAtom* nsXBLBinding::kParameterAtom = nsnull;
nsIAtom* nsXBLBinding::kBodyAtom = nsnull;
nsIAtom* nsXBLBinding::kPropertyAtom = nsnull;
nsIAtom* nsXBLBinding::kOnSetAtom = nsnull;
nsIAtom* nsXBLBinding::kOnGetAtom = nsnull;
nsIAtom* nsXBLBinding::kGetterAtom = nsnull;
nsIAtom* nsXBLBinding::kSetterAtom = nsnull;
nsIAtom* nsXBLBinding::kNameAtom = nsnull;
nsIAtom* nsXBLBinding::kReadOnlyAtom = nsnull;
nsIAtom* nsXBLBinding::kAttachToAtom = nsnull;

nsXBLBinding::EventHandlerMapEntry
nsXBLBinding::kEventHandlerMap[] = {
    { "click",         nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "dblclick",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mousedown",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mouseup",       nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mouseover",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mouseout",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },

    { "mousemove",     nsnull, &NS_GET_IID(nsIDOMMouseMotionListener) },

    { "keydown",       nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "keyup",         nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "keypress",      nsnull, &NS_GET_IID(nsIDOMKeyListener)         },

    { "load",          nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "unload",        nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "abort",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "error",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },

    { "create",        nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "close",         nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "destroy",       nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "command",       nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "broadcast",     nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "commandupdate", nsnull, &NS_GET_IID(nsIDOMMenuListener)        },

    { "overflow",        nsnull, &NS_GET_IID(nsIDOMScrollListener)    },
    { "underflow",       nsnull, &NS_GET_IID(nsIDOMScrollListener)    },
    { "overflowchanged", nsnull, &NS_GET_IID(nsIDOMScrollListener)    },

    { "focus",         nsnull, &NS_GET_IID(nsIDOMFocusListener)       },
    { "blur",          nsnull, &NS_GET_IID(nsIDOMFocusListener)       },

    { "submit",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "reset",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "change",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "select",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "input",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },

    { "paint",         nsnull, &NS_GET_IID(nsIDOMPaintListener)       },
    { "resize",        nsnull, &NS_GET_IID(nsIDOMPaintListener)       },
    { "scroll",        nsnull, &NS_GET_IID(nsIDOMPaintListener)       },

    { "dragenter",     nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "dragover",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "dragexit",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "dragdrop",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "draggesture",   nsnull, &NS_GET_IID(nsIDOMDragListener)        },

    { nsnull,            nsnull, nsnull                                 }
};

// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLBinding, nsIXBLBinding)

// Constructors/Destructors
nsXBLBinding::nsXBLBinding(nsIXBLPrototypeBinding* aBinding)
: mFirstHandler(nsnull),
  mIsStyleBinding(PR_TRUE),
  mMarkedForDeath(PR_FALSE)
{
  NS_INIT_REFCNT();
  mPrototypeBinding = aBinding;
  gRefCnt++;
  //  printf("REF COUNT UP: %d %s\n", gRefCnt, (const char*)mID);

  if (gRefCnt == 1) {
    kXULTemplateAtom = NS_NewAtom("template");
    kXULObservesAtom = NS_NewAtom("observes");

    kContentAtom = NS_NewAtom("content");
    kImplementationAtom = NS_NewAtom("implementation");
    kHandlersAtom = NS_NewAtom("handlers");
    kExcludesAtom = NS_NewAtom("excludes");
    kIncludesAtom = NS_NewAtom("includes");
    kInheritsAtom = NS_NewAtom("inherits");
    kEventAtom = NS_NewAtom("event");
    kPhaseAtom = NS_NewAtom("phase");
    kExtendsAtom = NS_NewAtom("extends");
    kActionAtom = NS_NewAtom("action");
    kMethodAtom = NS_NewAtom("method");
    kParameterAtom = NS_NewAtom("parameter");
    kBodyAtom = NS_NewAtom("body");
    kPropertyAtom = NS_NewAtom("property");
    kOnSetAtom = NS_NewAtom("onset");
    kOnGetAtom = NS_NewAtom("onget");
    kGetterAtom = NS_NewAtom("getter");
    kSetterAtom = NS_NewAtom("setter");    
    kNameAtom = NS_NewAtom("name");
    kReadOnlyAtom = NS_NewAtom("readonly");
    kAttachToAtom = NS_NewAtom("attachto");
    
    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
      entry->mAttributeAtom = NS_NewAtom(entry->mAttributeName);
      ++entry;
    }
  }
}


nsXBLBinding::~nsXBLBinding(void)
{
  gRefCnt--;
  //  printf("REF COUNT DOWN: %d %s\n", gRefCnt, (const char*)mID);

  if (gRefCnt == 0) {
    NS_RELEASE(kXULTemplateAtom);
    NS_RELEASE(kXULObservesAtom);

    NS_RELEASE(kContentAtom);
    NS_RELEASE(kImplementationAtom);
    NS_RELEASE(kHandlersAtom);
    NS_RELEASE(kExcludesAtom);
    NS_RELEASE(kIncludesAtom);
    NS_RELEASE(kInheritsAtom);
    NS_RELEASE(kEventAtom);
    NS_RELEASE(kPhaseAtom);
    NS_RELEASE(kExtendsAtom);
    NS_RELEASE(kActionAtom);
    NS_RELEASE(kMethodAtom);
    NS_RELEASE(kParameterAtom);
    NS_RELEASE(kBodyAtom);
    NS_RELEASE(kPropertyAtom); 
    NS_RELEASE(kOnSetAtom);
    NS_RELEASE(kOnGetAtom);
    NS_RELEASE(kGetterAtom);
    NS_RELEASE(kSetterAtom);
    NS_RELEASE(kNameAtom);
    NS_RELEASE(kReadOnlyAtom);
    NS_RELEASE(kAttachToAtom);
    
    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
      NS_IF_RELEASE(entry->mAttributeAtom);
      ++entry;
    }
  }
}

// nsIXBLBinding Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::GetBaseBinding(nsIXBLBinding** aResult)
{
  *aResult = mNextBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBaseBinding(nsIXBLBinding* aBinding)
{
  if (mNextBinding) {
    NS_ERROR("Base XBL binding is already defined!");
    return NS_OK;
  }

  mNextBinding = aBinding; // Comptr handles rel/add
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetAnonymousContent(nsIContent** aResult)
{
  *aResult = mContent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetAnonymousContent(nsIContent* aParent)
{
  // First cache the element.
  mContent = aParent;

  // Now we need to ensure two things.
  // (1) The anonymous content should be fooled into thinking it's in the bound
  // element's document.
  nsCOMPtr<nsIDocument> doc;
  mBoundElement->GetDocument(*getter_AddRefs(doc));

  mContent->SetDocument(doc, PR_TRUE, AllowScripts());
  
  // (2) The children's parent back pointer should not be to this synthetic root
  // but should instead point to the bound element.
  PRInt32 childCount;
  mContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    mContent->ChildAt(i, *getter_AddRefs(child));
    child->SetParent(mBoundElement);
    child->SetBindingParent(mBoundElement);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetPrototypeBinding(nsIXBLPrototypeBinding** aResult)
{
  *aResult = mPrototypeBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}
  
NS_IMETHODIMP
nsXBLBinding::SetPrototypeBinding(nsIXBLPrototypeBinding* aProtoBinding)
{
  mPrototypeBinding = aProtoBinding;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBindingElement(nsIContent** aResult)
{
  return mPrototypeBinding->GetBindingElement(aResult);
}

NS_IMETHODIMP
nsXBLBinding::SetBindingElement(nsIContent* aElement)
{
  return mPrototypeBinding->SetBindingElement(aElement);
}

NS_IMETHODIMP
nsXBLBinding::GetBoundElement(nsIContent** aResult)
{
  *aResult = mBoundElement;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBoundElement(nsIContent* aElement)
{
  mBoundElement = aElement;
  if (mNextBinding)
    mNextBinding->SetBoundElement(aElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::HasStyleSheets(PRBool* aResolveStyle)
{
  // Find out if we need to re-resolve style.  We'll need to do this
  // if we have additional stylesheets in our binding document.
  nsCOMPtr<nsIXBLDocumentInfo> info;
  mPrototypeBinding->GetXBLDocumentInfo(mBoundElement, getter_AddRefs(info));
  if (!info)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsISupportsArray> rules;
  info->GetRuleProcessors(getter_AddRefs(rules));
  if (rules) {
    *aResolveStyle = PR_TRUE;
    return NS_OK;
  }

  if (mNextBinding)
    return mNextBinding->HasStyleSheets(aResolveStyle);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GenerateAnonymousContent(nsIContent* aBoundElement)
{
  // Fetch the content element for this binding.
  nsCOMPtr<nsIContent> content;
  GetImmediateChild(kContentAtom, getter_AddRefs(content));

  if (!content) {
    // We have no anonymous content.
    if (mNextBinding)
      return mNextBinding->GenerateAnonymousContent(aBoundElement);
    else return NS_OK;
  }

  // Find out if we're really building kids or if we're just
  // using the attribute-setting shorthand hack.
  PRInt32 contentCount;
  content->ChildCount(contentCount);

  // Plan to build the content by default.
  PRBool hasContent = (contentCount > 0);
  PRBool hasInsertionPoints;
  mPrototypeBinding->HasInsertionPoints(&hasInsertionPoints);

  if (hasContent && !hasInsertionPoints) {
    // See if there's an includes attribute.
    nsAutoString includes;
    content->GetAttribute(kNameSpaceID_None, kIncludesAtom, includes);
    if (includes != NS_LITERAL_STRING("*")) {
      PRInt32 childCount;
      aBoundElement->ChildCount(childCount);
      if (childCount > 0) {
        // We'll only build content if all the explicit children are 
        // in the includes list.
        // Walk the children and ensure that all of them
        // are in the includes array.
        for (PRInt32 i = 0; i < childCount; i++) {
          nsCOMPtr<nsIContent> child;
          aBoundElement->ChildAt(i, *getter_AddRefs(child));
          nsCOMPtr<nsIAtom> tag;
          child->GetTag(*getter_AddRefs(tag));
          if (!IsInExcludesList(tag, includes)) {
            // XXX HACK! Ignore <template> and <observes>
            if (tag.get() != kXULTemplateAtom &&
              tag.get() != kXULObservesAtom) {
              return NS_OK;
            }
          }
        }
      }
    }
  }
 
  if (hasContent) {
    nsCOMPtr<nsIContent> clonedContent;
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(content));

    nsCOMPtr<nsIDOMNode> clonedNode;
    domElement->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));
  
    clonedContent = do_QueryInterface(clonedNode);
    SetAnonymousContent(clonedContent);
  }

  // Always check the content element for potential attributes.
  // This shorthand hack always happens, even when we didn't
  // build anonymous content.
  PRInt32 length;
  content->GetAttributeCount(length);

  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> name;
  nsCOMPtr<nsIAtom> prefix;

  for (PRInt32 i = 0; i < length; ++i) {
    content->GetAttributeNameAt(0, namespaceID, *getter_AddRefs(name), *getter_AddRefs(prefix));

    if (name.get() != kIncludesAtom) {
      nsAutoString value;
      mBoundElement->GetAttribute(namespaceID, name, value);
      if (value.IsEmpty()) {
        nsAutoString value2;
        content->GetAttribute(namespaceID, name, value2);
        mBoundElement->SetAttribute(namespaceID, name, value2, PR_FALSE);
      }
    }

    // Conserve space by wiping the attributes off the clone.
    if (mContent)
      mContent->UnsetAttribute(namespaceID, name, PR_FALSE);
  }
  
  if (mContent)
    mPrototypeBinding->SetInitialAttributes(mBoundElement, mContent);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallEventHandlers(nsIContent* aBoundElement, nsIXBLBinding** aBinding)
{
  // Don't install handlers if scripts aren't allowed.
  if (AllowScripts()) {
    // Fetch the handlers prototypes for this binding.
    nsCOMPtr<nsIXBLDocumentInfo> info;
    mPrototypeBinding->GetXBLDocumentInfo(mBoundElement, getter_AddRefs(info));
    if (!info)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXBLPrototypeHandler> handlerChain;
    nsCOMPtr<nsIXBLPrototypeHandler> specialChain;
    mPrototypeBinding->GetPrototypeHandlers(getter_AddRefs(handlerChain), getter_AddRefs(specialChain));
  
    if (specialChain && !*aBinding) {
      *aBinding = this;
      NS_ADDREF(*aBinding);
    }

    nsCOMPtr<nsIXBLPrototypeHandler> curr = handlerChain;
    nsXBLEventHandler* currHandler = nsnull;

    while (curr) {
      nsCOMPtr<nsIContent> child;
      curr->GetHandlerElement(getter_AddRefs(child));

      // Fetch the type attribute.
      // XXX Deal with a comma-separated list of types
      nsCOMPtr<nsIAtom> eventAtom;
      curr->GetEventName(getter_AddRefs(eventAtom));

      nsIID iid;
      PRBool found = PR_FALSE;
      GetEventHandlerIID(eventAtom, &iid, &found);

      if (found) { 
        nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(mBoundElement);
        nsAutoString attachType;
        child->GetAttribute(kNameSpaceID_None, kAttachToAtom, attachType);
        if (attachType == NS_LITERAL_STRING("_document") || 
            attachType == NS_LITERAL_STRING("_window"))
        {
          nsCOMPtr<nsIDocument> boundDoc;
          mBoundElement->GetDocument(*getter_AddRefs(boundDoc));
          if (attachType == NS_LITERAL_STRING("_window")) {
            nsCOMPtr<nsIScriptGlobalObject> global;
            boundDoc->GetScriptGlobalObject(getter_AddRefs(global));
            receiver = do_QueryInterface(global);
          }
          else receiver = do_QueryInterface(boundDoc);
        }
        else if (!attachType.IsEmpty() && !attachType.Equals(NS_LITERAL_STRING("_element"))) {
          nsCOMPtr<nsIDocument> boundDoc;
          mBoundElement->GetDocument(*getter_AddRefs(boundDoc));
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(boundDoc));
          nsCOMPtr<nsIDOMElement> otherElement;
          domDoc->GetElementById(attachType, getter_AddRefs(otherElement));
          receiver = do_QueryInterface(otherElement);
        }

        // Figure out if we're using capturing or not.
        PRBool useCapture = PR_FALSE;
        nsAutoString capturer;
        child->GetAttribute(kNameSpaceID_None, kPhaseAtom, capturer);
        if (capturer == NS_LITERAL_STRING("capturing"))
          useCapture = PR_TRUE;

        // Create a new nsXBLEventHandler.
        nsXBLEventHandler* handler = nsnull;
        
        nsAutoString type;
        eventAtom->ToString(type);

        // Add the event listener.
        if (iid.Equals(NS_GET_IID(nsIDOMMouseListener))) {
          nsXBLMouseHandler* mouseHandler;
          NS_NewXBLMouseHandler(receiver, curr, &mouseHandler);
          receiver->AddEventListener(type, (nsIDOMMouseListener*)mouseHandler, useCapture);
          handler = mouseHandler;
        }
        else if(iid.Equals(NS_GET_IID(nsIDOMKeyListener))) {
          nsXBLKeyHandler* keyHandler;
          NS_NewXBLKeyHandler(receiver, curr, &keyHandler);
          receiver->AddEventListener(type, (nsIDOMKeyListener*)keyHandler, useCapture);
          handler = keyHandler;
        }
        else if (iid.Equals(NS_GET_IID(nsIDOMMouseMotionListener))) {
          nsXBLMouseMotionHandler* mouseHandler;
          NS_NewXBLMouseMotionHandler(receiver, curr, &mouseHandler);
          receiver->AddEventListener(type, (nsIDOMMouseListener*)mouseHandler, useCapture);
          handler = mouseHandler;
        }
        else if(iid.Equals(NS_GET_IID(nsIDOMFocusListener))) {
          nsXBLFocusHandler* focusHandler;
          NS_NewXBLFocusHandler(receiver, curr, &focusHandler);
          receiver->AddEventListener(type, (nsIDOMFocusListener*)focusHandler, useCapture);
          handler = focusHandler;
        }
        else if (iid.Equals(NS_GET_IID(nsIDOMMenuListener))) {
          nsXBLXULHandler* xulHandler;
          NS_NewXBLXULHandler(receiver, curr, &xulHandler);
          receiver->AddEventListener(type, (nsIDOMMenuListener*)xulHandler, useCapture);
          handler = xulHandler;
        }
        else if (iid.Equals(NS_GET_IID(nsIDOMScrollListener))) {
          nsXBLScrollHandler* scrollHandler;
          NS_NewXBLScrollHandler(receiver, curr, &scrollHandler);
          receiver->AddEventListener(type, (nsIDOMScrollListener*)scrollHandler, useCapture);
          handler = scrollHandler;
        }
        else if (iid.Equals(NS_GET_IID(nsIDOMFormListener))) {
          nsXBLFormHandler* formHandler;
          NS_NewXBLFormHandler(receiver, curr, &formHandler);
          receiver->AddEventListener(type, (nsIDOMFormListener*)formHandler, useCapture);
          handler = formHandler;
        }
        else if(iid.Equals(NS_GET_IID(nsIDOMDragListener))) {
          nsXBLDragHandler* dragHandler;
          NS_NewXBLDragHandler(receiver, curr, &dragHandler);
          receiver->AddEventListener(type, (nsIDOMDragListener*)dragHandler, useCapture);
          handler = dragHandler;
        }
        else if(iid.Equals(NS_GET_IID(nsIDOMLoadListener))) {
          nsXBLLoadHandler* loadHandler;
          NS_NewXBLLoadHandler(receiver, curr, &loadHandler);
          receiver->AddEventListener(type, (nsIDOMLoadListener*)loadHandler, useCapture);
          handler = loadHandler;
        }
        else {
          NS_WARNING("***** Non-compliant XBL event listener attached! *****");
          nsAutoString value;
          child->GetAttribute(kNameSpaceID_None, kActionAtom, value);
          if (value.IsEmpty())
            GetTextData(child, value);
          AddScriptEventListener(mBoundElement, eventAtom, value, iid);
        }

        // We chain all our event handlers together for easy
        // removal later (if/when the binding dies).
        if (handler) {
          if (!currHandler)
            mFirstHandler = handler;
          else 
            currHandler->SetNextHandler(handler);

          currHandler = handler;

          // Let the listener manager hold on to the handler.
          NS_RELEASE(handler);
        }
      }

      nsCOMPtr<nsIXBLPrototypeHandler> next;
      curr->GetNextHandler(getter_AddRefs(next));
      curr = next;
    }
  }

  if (mNextBinding) {
    nsCOMPtr<nsIXBLBinding> binding;
    mNextBinding->InstallEventHandlers(aBoundElement, getter_AddRefs(binding));
    if (!*aBinding) {
      *aBinding = binding;
      NS_IF_ADDREF(*aBinding);
    }
  }

  return NS_OK;
}

const char* gPropertyArg[] = { "val" };

NS_IMETHODIMP
nsXBLBinding::InstallProperties(nsIContent* aBoundElement)
{
  // Always install the base class properties first, so that
  // derived classes can reference the base class properties.
  if (mNextBinding)
    mNextBinding->InstallProperties(aBoundElement);

   // Fetch the interface element for this binding.
  nsCOMPtr<nsIContent> interfaceElement;
  GetImmediateChild(kImplementationAtom, getter_AddRefs(interfaceElement));

  if (interfaceElement && AllowScripts()) {
    // Get our bound element's script context.
    nsresult rv;
    nsCOMPtr<nsIDocument> document;
    mBoundElement->GetDocument(*getter_AddRefs(document));
    if (!document)
      return NS_OK;

    nsCOMPtr<nsIScriptGlobalObject> global;
    document->GetScriptGlobalObject(getter_AddRefs(global));

    if (!global)
      return NS_OK;

    nsCOMPtr<nsIScriptContext> context;
    rv = global->GetContext(getter_AddRefs(context));
    if (NS_FAILED(rv)) return rv;

    // Init our class and insert it into the prototype chain.
    nsAutoString className;
    nsCAutoString classStr; 
    interfaceElement->GetAttribute(kNameSpaceID_None, kNameAtom, className);
    if (!className.IsEmpty()) {
      classStr.AssignWithConversion(className);
    }
    else {
      GetBindingURI(classStr);
    }

    JSObject* scriptObject;
    JSObject* classObject;
    if (NS_FAILED(rv = InitClass(classStr, context, document, (void**)&scriptObject, (void**)&classObject)))
      return rv;

    JSContext* cx = (JSContext*)context->GetNativeContext();

    // Do a walk.
    PRInt32 childCount;
    interfaceElement->ChildCount(childCount);
    for (PRInt32 i = 0; i < childCount; i++) {
      nsCOMPtr<nsIContent> child;
      interfaceElement->ChildAt(i, *getter_AddRefs(child));

      // See if we're a property or a method.
      nsCOMPtr<nsIAtom> tagName;
      child->GetTag(*getter_AddRefs(tagName));

      if (tagName.get() == kMethodAtom && classObject) {
        // Obtain our name attribute.
        nsAutoString name, body;
        child->GetAttribute(kNameSpaceID_None, kNameAtom, name);

        // Now walk all of our args.
        // XXX I'm lame. 32 max args allowed.
        char* args[32];
        PRUint32 argCount = 0;
        PRInt32 kidCount;
        child->ChildCount(kidCount);
        for (PRInt32 j = 0; j < kidCount; j++)
        {
          nsCOMPtr<nsIContent> arg;
          child->ChildAt(j, *getter_AddRefs(arg));
          nsCOMPtr<nsIAtom> kidTagName;
          arg->GetTag(*getter_AddRefs(kidTagName));
          if (kidTagName.get() == kParameterAtom) {
            // Get the argname and add it to the array.
            nsAutoString argName;
            arg->GetAttribute(kNameSpaceID_None, kNameAtom, argName);
            char* argStr = argName.ToNewCString();
            args[argCount] = argStr;
            argCount++;
          }
          else if (kidTagName.get() == kBodyAtom) {
            PRInt32 textCount;
            arg->ChildCount(textCount);
            
            for (PRInt32 k = 0; k < textCount; k++) {
              // Get the child.
              nsCOMPtr<nsIContent> textChild;
              arg->ChildAt(k, *getter_AddRefs(textChild));
              nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
              if (text) {
                nsAutoString data;
                text->GetData(data);
                body += data;
              }
            }
          }
        }

        // Now that we have a body and args, compile the function
        // and then define it as a property.
        if (!body.IsEmpty()) {
          void* myFunc;
          nsCAutoString cname; cname.AssignWithConversion(name.GetUnicode());
          rv = context->CompileFunction(classObject,
                                        cname,
                                        argCount,
                                        (const char**)args,
                                        body, 
                                        nsnull,
                                        0,
                                        PR_FALSE,
                                        &myFunc);
        }
        for (PRUint32 l = 0; l < argCount; l++) {
          nsMemory::Free(args[l]);
        }
      }
      else if (tagName.get() == kPropertyAtom) {
        // Obtain our name attribute.
        nsAutoString name;
        child->GetAttribute(kNameSpaceID_None, kNameAtom, name);

        if (!name.IsEmpty()) {
          // We have a property.
          nsAutoString getter, setter, readOnly;
          child->GetAttribute(kNameSpaceID_None, kOnGetAtom, getter);
          child->GetAttribute(kNameSpaceID_None, kOnSetAtom, setter);
          child->GetAttribute(kNameSpaceID_None, kReadOnlyAtom, readOnly);

          void* getFunc = nsnull;
          void* setFunc = nsnull;
          uintN attrs = JSPROP_ENUMERATE;

          if (readOnly == NS_LITERAL_STRING("true"))
            attrs |= JSPROP_READONLY;

          // try for first <getter> tag
          if (getter.IsEmpty()) {
            PRInt32 childCount;
            child->ChildCount(childCount);

            nsCOMPtr<nsIContent> getterElement;
            for (PRInt32 j=0; j<childCount; j++) {
              child->ChildAt(j, *getter_AddRefs(getterElement));
              
              if (!getterElement) continue;
              
              nsCOMPtr<nsIAtom> getterTag;
              getterElement->GetTag(*getter_AddRefs(getterTag));
              
              if (getterTag.get() == kGetterAtom) {
                GetTextData(getterElement, getter);
                break;          // stop at first tag
              }
            }

          }
          
          if (!getter.IsEmpty() && classObject) {
            rv = context->CompileFunction(classObject,
                                          nsCAutoString("onget"),
                                          0,
                                          nsnull,
                                          getter, 
                                          nsnull,
                                          0,
                                          PR_FALSE,
                                          &getFunc);
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
            attrs |= JSPROP_GETTER;
          }

          // try for first <setter> tag
          if (setter.IsEmpty()) {
            PRInt32 childCount;
            child->ChildCount(childCount);

            nsCOMPtr<nsIContent> setterElement;
            for (PRInt32 j=0; j<childCount; j++) {
              child->ChildAt(j, *getter_AddRefs(setterElement));
              
              if (!setterElement) continue;
              
              nsCOMPtr<nsIAtom> setterTag;
              setterElement->GetTag(*getter_AddRefs(setterTag));
              if (setterTag.get() == kSetterAtom) {
                GetTextData(setterElement, setter);
                break;          // stop at first tag
              }
            }
          }
          
          if (!setter.IsEmpty() && classObject) {
            rv = context->CompileFunction(classObject,
                                          nsCAutoString("onset"),
                                          1,
                                          gPropertyArg,
                                          setter, 
                                          nsnull,
                                          0,
                                          PR_FALSE,
                                          &setFunc);
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
            attrs |= JSPROP_SETTER;
          }

          if ((getFunc || setFunc) && classObject) {
            // Having either a getter or setter results in the
            // destruction of any initial value that might be set.
            // This means we only have to worry about defining the getter
            // or setter.
            ::JS_DefineUCProperty(cx, (JSObject*)classObject, NS_REINTERPRET_CAST(const jschar*, name.GetUnicode()), 
                                       name.Length(), JSVAL_VOID,
                                       (JSPropertyOp) getFunc, 
                                       (JSPropertyOp) setFunc, 
                                       attrs); 
          } else {
            // Look for a normal value and just define that.
            nsCOMPtr<nsIContent> textChild;
            PRInt32 textCount;
            child->ChildCount(textCount);
            nsAutoString answer;
            for (PRInt32 j = 0; j < textCount; j++) {
              // Get the child.
              child->ChildAt(j, *getter_AddRefs(textChild));
              nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
              if (text) {
                nsAutoString data;
                text->GetData(data);
                answer += data;
              }
            }

            if (!answer.IsEmpty()) {
              // Evaluate our script and obtain a value.
              jsval result = nsnull;
              PRBool undefined;
              rv = context->EvaluateStringWithValue(answer, 
                                           scriptObject,
                                           nsnull, nsnull, 0, nsnull,
                                           (void*) &result, &undefined);
              
              if (!undefined) {
                // Define that value as a property
                ::JS_DefineUCProperty(cx, (JSObject*)scriptObject, NS_REINTERPRET_CAST(const jschar*, name.GetUnicode()), 
                                           name.Length(), result,
                                           nsnull, nsnull,
                                           attrs); 
              }
            }
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBaseTag(PRInt32* aNameSpaceID, nsIAtom** aResult)
{
  if (mNextBinding)
    return mNextBinding->GetBaseTag(aNameSpaceID, aResult);
  return mPrototypeBinding->GetBaseTag(aNameSpaceID, aResult);
}

NS_IMETHODIMP
nsXBLBinding::AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag)
{
  // XXX Change if we ever allow multiple bindings in a chain to contribute anonymous content
  if (!mContent) {
    if (mNextBinding)
      return mNextBinding->AttributeChanged(aAttribute, aNameSpaceID, aRemoveFlag);
    return NS_OK;
  }

  return mPrototypeBinding->AttributeChanged(aAttribute, aNameSpaceID, aRemoveFlag, mBoundElement, mContent);
}

NS_IMETHODIMP
nsXBLBinding::ExecuteAttachedHandler()
{
  if (mNextBinding)
    mNextBinding->ExecuteAttachedHandler();

  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(mBoundElement));
  mPrototypeBinding->BindingAttached(rec);

  return NS_OK;
}

NS_IMETHODIMP 
nsXBLBinding::ExecuteDetachedHandler()
{
  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(mBoundElement));
  mPrototypeBinding->BindingDetached(rec);

  if (mNextBinding)
    mNextBinding->ExecuteDetachedHandler();

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::UnhookEventHandlers()
{
  if (mFirstHandler) {
    // Unhook our event handlers.
    mFirstHandler->RemoveEventHandlers();
    mFirstHandler = nsnull;
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument)
{
  if (aOldDocument != aNewDocument) {
    if (mFirstHandler) {
      mFirstHandler->MarkForDeath();
      mFirstHandler = nsnull;
    }

    if (mNextBinding)
      mNextBinding->ChangeDocument(aOldDocument, aNewDocument);

    // Only style bindings get their prototypes unhooked.
    if (mIsStyleBinding) {
      // Now the binding dies.  Unhook our prototypes.
      nsCOMPtr<nsIContent> interfaceElement;
      GetImmediateChild(kImplementationAtom, getter_AddRefs(interfaceElement));

      if (interfaceElement) { 
        nsCOMPtr<nsIScriptGlobalObject> global;
        aOldDocument->GetScriptGlobalObject(getter_AddRefs(global));
        if (global) {
          nsCOMPtr<nsIScriptContext> context;
          global->GetContext(getter_AddRefs(context));
          if (context) {
            JSObject* scriptObject;
            nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(mBoundElement));
            owner->GetScriptObject(context, (void**)&scriptObject);
            if (scriptObject) {
              // XXX Stay in sync! What if a layered binding has an <interface>?!
    
              // XXX Sanity check to make sure our class name matches
              // Pull ourselves out of the proto chain.
              JSContext* jscontext = (JSContext*)context->GetNativeContext();
              JSObject* ourProto = ::JS_GetPrototype(jscontext, scriptObject);
              JSObject* grandProto = ::JS_GetPrototype(jscontext, ourProto);
              ::JS_SetPrototype(jscontext, scriptObject, grandProto);
            }
          }
        }
      }
    }

    // Update the anonymous content.
    nsCOMPtr<nsIContent> anonymous;
    GetAnonymousContent(getter_AddRefs(anonymous));
    if (anonymous) {
      if (mIsStyleBinding)
        anonymous->SetDocument(nsnull, PR_TRUE, PR_TRUE); // Kill it.
      else anonymous->SetDocument(aNewDocument, PR_TRUE, AllowScripts()); // Keep it around.
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXBLBinding::GetBindingURI(nsCString& aResult)
{
  return mPrototypeBinding->GetBindingURI(aResult);
}

NS_IMETHODIMP 
nsXBLBinding::GetDocURI(nsCString& aResult)
{
  return mPrototypeBinding->GetDocURI(aResult);
}

NS_IMETHODIMP 
nsXBLBinding::GetID(nsCString& aResult)
{
  return mPrototypeBinding->GetID(aResult);
}

NS_IMETHODIMP
nsXBLBinding::InheritsStyle(PRBool* aResult)
{
  // XXX Will have to change if we ever allow multiple bindings to contribute anonymous content.
  // Most derived binding with anonymous content determines style inheritance for now.

  // XXX What about bindings with <content> but no kids, e.g., my treecell-text binding?
  if (mContent)
    return mPrototypeBinding->InheritsStyle(aResult);
  
  if (mNextBinding)
    return mNextBinding->InheritsStyle(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::WalkRules(nsISupportsArrayEnumFunc aFunc, void* aData)
{
  nsresult rv = NS_OK;
  if (mNextBinding) {
    rv = mNextBinding->WalkRules(aFunc, aData);
    if (NS_FAILED(rv))
      return rv;
  }

  nsCOMPtr<nsIXBLDocumentInfo> info;
  mPrototypeBinding->GetXBLDocumentInfo(mBoundElement, getter_AddRefs(info));
  if (!info)
    return NS_ERROR_FAILURE;
  nsCOMPtr<nsISupportsArray> rules;
  info->GetRuleProcessors(getter_AddRefs(rules));
  if (rules)
    rules->EnumerateForwards(aFunc, aData);
  
  return rv;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::InitClass(const nsCString& aClassName, nsIScriptContext* aContext, 
                        nsIDocument* aDocument, void** aScriptObject, void** aClassObject)
{
  *aClassObject = nsnull;
  *aScriptObject = nsnull;

  // Obtain the bound element's current script object.
  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(mBoundElement));
  owner->GetScriptObject(aContext, aScriptObject);
  if (!(*aScriptObject))
    return NS_ERROR_FAILURE;

  JSObject* object = (JSObject*)(*aScriptObject);

  // First ensure our JS class is initialized.
  JSContext* jscontext = (JSContext*)aContext->GetNativeContext();
  JSObject* global = ::JS_GetGlobalObject(jscontext);
  jsval vp;
  JSObject* proto;

  if ((! ::JS_LookupProperty(jscontext, global, aClassName, &vp)) ||
      JSVAL_IS_PRIMITIVE(vp)) {
    // We need to initialize the class.
    
    nsXBLJSClass* c;
    void* classObject;
    nsCStringKey key(aClassName);
    classObject = (nsXBLService::gClassTable)->Get(&key);

    if (classObject) {
      c = NS_STATIC_CAST(nsXBLJSClass*, classObject);

      // If c is on the LRU list (i.e., not linked to itself), remove it now!
      JSCList* link = NS_STATIC_CAST(JSCList*, c);
      if (c->next != link) {
        JS_REMOVE_AND_INIT_LINK(link);
        nsXBLService::gClassLRUListLength--;
      }
    } else {
      if (JS_CLIST_IS_EMPTY(&nsXBLService::gClassLRUList)) {
        // We need to create a struct for this class.
        c = new nsXBLJSClass(aClassName);
        if (!c)
          return NS_ERROR_OUT_OF_MEMORY;
      } else {
        // Pull the least recently used class struct off the list.
        JSCList* lru = (nsXBLService::gClassLRUList).next;
        JS_REMOVE_AND_INIT_LINK(lru);
        nsXBLService::gClassLRUListLength--;

        // Remove any mapping from the old name to the class struct.
        c = NS_STATIC_CAST(nsXBLJSClass*, lru);
        nsCStringKey oldKey(c->name);
        (nsXBLService::gClassTable)->Remove(&oldKey);

        // Change the class name and we're done.
        nsMemory::Free((void*) c->name);
        c->name = nsXPIDLCString::Copy(aClassName);
      }

      // Add c to our table.
      (nsXBLService::gClassTable)->Put(&key, (void*)c);
    }
    
    // Retrieve the current prototype of the JS object.
    JSObject* parent_proto = ::JS_GetPrototype(jscontext, object);

    // Make a new object prototyped by parent_proto and parented by global.
    proto = ::JS_InitClass(jscontext,           // context
                           global,              // global object
                           parent_proto,        // parent proto 
                           c,                   // JSClass
                           NULL,                // JSNative ctor
                           0,                   // ctor args
                           nsnull,              // proto props
                           nsnull,              // proto funcs
                           nsnull,              // ctor props (static)
                           nsnull);             // ctor funcs (static)
    if (!proto) {
      (nsXBLService::gClassTable)->Remove(&key);
      delete c;
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // The prototype holds a strong reference to its class struct.
    c->Hold();
    *aClassObject = (void*)proto;
  }
  else {
    proto = JSVAL_TO_OBJECT(vp);
  }

  // Set the prototype of our object to be the new class.
  ::JS_SetPrototype(jscontext, object, proto);

  return NS_OK;
}

void
nsXBLBinding::GetImmediateChild(nsIAtom* aTag, nsIContent** aResult) 
{
  nsCOMPtr<nsIContent> binding;
  mPrototypeBinding->GetBindingElement(getter_AddRefs(binding));

  *aResult = nsnull;
  PRInt32 childCount;
  binding->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    binding->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

  return;
}

PRBool
nsXBLBinding::IsInExcludesList(nsIAtom* aTag, const nsString& aList) 
{ 
  nsAutoString element;
  aTag->ToString(element);

  if (aList == NS_LITERAL_STRING("*"))
      return PR_TRUE; // match _everything_!

  PRInt32 indx = aList.Find(element);
  if (indx == -1)
    return PR_FALSE; // not in the list at all

  // okay, now make sure it's not a substring snafu; e.g., 'ur'
  // found inside of 'blur'.
  if (indx > 0) {
    PRUnichar ch = aList[indx - 1];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar('|'))
      return PR_FALSE;
  }

  if (indx + element.Length() < aList.Length()) {
    PRUnichar ch = aList[indx + element.Length()];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar('|'))
      return PR_FALSE;
  }

  return PR_TRUE;
}

void
nsXBLBinding::GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound)
{
  *aFound = PR_FALSE;

  EventHandlerMapEntry* entry = kEventHandlerMap;
  while (entry->mAttributeAtom) {
    if (entry->mAttributeAtom == aName) {
        *aIID = *entry->mHandlerIID;
        *aFound = PR_TRUE;
        break;
    }
    ++entry;
  }
}
    
NS_IMETHODIMP
nsXBLBinding::AddScriptEventListener(nsIContent* aElement, nsIAtom* aName, const nsString& aValue, REFNSIID aIID)
{
  nsAutoString val;
  aName->ToString(val);
  
  nsAutoString eventStr; eventStr.AssignWithConversion("on");
  eventStr += val;

  nsCOMPtr<nsIAtom> eventName = getter_AddRefs(NS_NewAtom(eventStr));

  nsresult rv;
  nsCOMPtr<nsIDocument> document;
  aElement->GetDocument(*getter_AddRefs(document));
  if (!document)
    return NS_OK;

  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(aElement));
  if (!receiver)
    return NS_OK;

  nsCOMPtr<nsIScriptGlobalObject> global;
  document->GetScriptGlobalObject(getter_AddRefs(global));

  // This can happen normally as part of teardown code.
  if (!global)
    return NS_OK;

  nsCOMPtr<nsIScriptContext> context;
  rv = global->GetContext(getter_AddRefs(context));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIEventListenerManager> manager;
  rv = receiver->GetListenerManager(getter_AddRefs(manager));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIScriptObjectOwner> scriptOwner(do_QueryInterface(receiver));
  if (!scriptOwner)
    return NS_OK;

  rv = manager->AddScriptEventListener(context, scriptOwner, eventName, aValue, aIID, PR_FALSE);

  return rv;
}

nsresult
nsXBLBinding::GetTextData(nsIContent *aParent, nsString& aResult)
{
  aResult.Truncate(0);

  nsCOMPtr<nsIContent> textChild;
  PRInt32 textCount;
  aParent->ChildCount(textCount);
  nsAutoString answer;
  for (PRInt32 j = 0; j < textCount; j++) {
    // Get the child.
    aParent->ChildAt(j, *getter_AddRefs(textChild));
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
    if (text) {
      nsAutoString data;
      text->GetData(data);
      aResult += data;
    }
  }
  return NS_OK;
}

PRBool
nsXBLBinding::AllowScripts()
{
  PRBool result;
  mPrototypeBinding->GetAllowScripts(&result);
  return result;
}

NS_IMETHODIMP
nsXBLBinding::GetInsertionPoint(nsIContent* aChild, nsIContent** aResult)
{
  *aResult = nsnull;
  if (!mContent)
    return NS_OK;

  return mPrototypeBinding->GetInsertionPoint(mBoundElement, mContent, aChild, aResult);
}

NS_IMETHODIMP
nsXBLBinding::GetSingleInsertionPoint(nsIContent** aResult, PRBool* aMultipleInsertionPoints)
{
  *aResult = nsnull;
  *aMultipleInsertionPoints = PR_FALSE;
  if (!mContent)
    return NS_OK;

  return mPrototypeBinding->GetSingleInsertionPoint(mBoundElement, mContent, aResult, aMultipleInsertionPoints);
}

NS_IMETHODIMP
nsXBLBinding::GetRootBinding(nsIXBLBinding** aResult)
{
  if (mNextBinding)
    return mNextBinding->GetRootBinding(aResult);

  *aResult = this;
  NS_ADDREF(this);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetFirstStyleBinding(nsIXBLBinding** aResult)
{
  if (mIsStyleBinding) {
    *aResult = this;
    NS_ADDREF(this);
    return NS_OK;
  }
  else if (mNextBinding)
    return mNextBinding->GetFirstStyleBinding(aResult);

  *aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::MarkForDeath()
{
  mMarkedForDeath = PR_TRUE;
  ExecuteDetachedHandler();
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::MarkedForDeath(PRBool* aResult)
{
  *aResult = mMarkedForDeath;
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLBinding(nsIXBLPrototypeBinding* aBinding, nsIXBLBinding** aResult)
{
  *aResult = new nsXBLBinding(aBinding);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

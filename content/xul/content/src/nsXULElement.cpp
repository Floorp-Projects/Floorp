/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Brendan Eich <brendan@mozilla.org>
 *   Mike Shaver <shaver@mozilla.org>
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
 * ***** END LICENSE BLOCK *****
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

/*

  Implementation for a XUL content element.

  TO DO

  1. Possibly clean up the attribute walking code (e.g., in
     SetDocument, GetAttrCount, etc.) by extracting into an iterator.

 */

#include "jsapi.h"      // for JS_AddNamedRoot and JS_RemoveRootRT
#include "jsxdrapi.h"
#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsDOMString.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsHashtable.h"
#include "nsIAtom.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMXULListener.h"
#include "nsIDOMScrollListener.h"
#include "nsIDOMContextMenuListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIFastLoadService.h"
#include "nsHTMLStyleSheet.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsICSSStyleRule.h"
#include "nsIStyleSheet.h"
#include "nsIStyledContent.h"
#include "nsIURL.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIXULContent.h"
#include "nsIXULDocument.h"
#include "nsIXULPopupListener.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIXBLService.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsRDFCID.h"
#include "nsStyleConsts.h"
#include "nsXPIDLString.h"
#include "nsXULControllers.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsXULDocument.h"
#include "nsRuleWalker.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsCSSDeclaration.h"
#include "nsIListBoxObject.h"
#include "nsContentUtils.h"
#include "nsContentList.h"
#include "nsMutationEvent.h"
#include "nsIDOMMutationEvent.h"
#include "nsPIDOMWindow.h"
#include "nsDOMAttributeMap.h"
#include "nsDOMCSSDeclaration.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"

#include "prlog.h"
#include "rdf.h"

#include "nsIControllers.h"

// The XUL interfaces implemented by the RDF content node.
#include "nsIDOMXULElement.h"

// The XUL doc interface
#include "nsIDOMXULDocument.h"

#include "nsReadableUtils.h"
#include "nsITimelineService.h"
#include "nsIFrame.h"
#include "nsNodeInfoManager.h"

class nsIDocShell;

// Global object maintenance
nsICSSParser* nsXULPrototypeElement::sCSSParser = nsnull;
nsIXULPrototypeCache* nsXULPrototypeScript::sXULPrototypeCache = nsnull;
nsIXBLService * nsXULElement::gXBLService = nsnull;
nsICSSOMFactory* nsXULElement::gCSSOMFactory = nsnull;

//----------------------------------------------------------------------

static NS_DEFINE_CID(kEventListenerManagerCID,    NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kXULPopupListenerCID,        NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_CID(kCSSOMFactoryCID,            NS_CSSOMFACTORY_CID);

//----------------------------------------------------------------------

#if 0 /* || defined(DEBUG_shaver) || defined(DEBUG_waterson) */
#define DEBUG_ATTRIBUTE_STATS
#endif

#ifdef DEBUG_ATTRIBUTE_STATS
#include <execinfo.h>

static struct {
    PRUint32 UnsetAttr;
    PRUint32 Create;
    PRUint32 Total;
} gFaults;
#endif

#include "nsIJSRuntimeService.h"
static nsIJSRuntimeService* gJSRuntimeService = nsnull;
static JSRuntime* gScriptRuntime = nsnull;
static PRInt32 gScriptRuntimeRefcnt = 0;

static nsresult
AddJSGCRoot(void* aScriptObjectRef, const char* aName)
{
    if (++gScriptRuntimeRefcnt == 1 || !gScriptRuntime) {
        CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                       &gJSRuntimeService);
        if (! gJSRuntimeService) {
            NS_NOTREACHED("couldn't add GC root");
            return NS_ERROR_FAILURE;
        }

        gJSRuntimeService->GetRuntime(&gScriptRuntime);
        if (! gScriptRuntime) {
            NS_NOTREACHED("couldn't add GC root");
            return NS_ERROR_FAILURE;
        }
    }

    PRBool ok;
    ok = ::JS_AddNamedRootRT(gScriptRuntime, aScriptObjectRef, aName);
    if (! ok) {
        NS_NOTREACHED("couldn't add GC root");
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

static nsresult
RemoveJSGCRoot(void* aScriptObjectRef)
{
    if (! gScriptRuntime) {
        NS_NOTREACHED("couldn't remove GC root");
        return NS_ERROR_FAILURE;
    }

    ::JS_RemoveRootRT(gScriptRuntime, aScriptObjectRef);

    if (--gScriptRuntimeRefcnt == 0) {
        NS_RELEASE(gJSRuntimeService);
        gScriptRuntime = nsnull;
    }

    return NS_OK;
}

//----------------------------------------------------------------------

struct EventHandlerMapEntry {
    const char*  mAttributeName;
    nsIAtom*     mAttributeAtom;
};

static EventHandlerMapEntry kEventHandlerMap[] = {
    { "onclick",         nsnull },
    { "ondblclick",      nsnull },
    { "onmousedown",     nsnull },
    { "onmouseup",       nsnull },
    { "onmouseover",     nsnull },
    { "onmouseout",      nsnull },

    { "onmousemove",     nsnull },

    { "onkeydown",       nsnull },
    { "onkeyup",         nsnull },
    { "onkeypress",      nsnull },

    { "onload",          nsnull },
    { "onunload",        nsnull },
    { "onabort",         nsnull },
    { "onerror",         nsnull },

    { "onpopupshowing",    nsnull },
    { "onpopupshown",      nsnull },
    { "onpopuphiding" ,    nsnull },
    { "onpopuphidden",     nsnull },
    { "onclose",           nsnull },
    { "oncommand",         nsnull },
    { "onbroadcast",       nsnull },
    { "oncommandupdate",   nsnull },

    { "onoverflow",       nsnull },
    { "onunderflow",      nsnull },
    { "onoverflowchanged",nsnull },

    { "onfocus",         nsnull },
    { "onblur",          nsnull },

    { "onsubmit",        nsnull },
    { "onreset",         nsnull },
    { "onchange",        nsnull },
    { "onselect",        nsnull },
    { "oninput",         nsnull },

    { "onpaint",         nsnull },

    { "ondragenter",     nsnull },
    { "ondragover",      nsnull },
    { "ondragexit",      nsnull },
    { "ondragdrop",      nsnull },
    { "ondraggesture",   nsnull },

    { "oncontextmenu",   nsnull },

    { nsnull,            nsnull }
};


// XXX This function is called for every attribute on every element for
// XXX which we SetDocument, among other places.  A linear search might
// XXX not be what we want.
static PRBool
IsEventHandler(nsIAtom* aName)
{
    const char* name;
    aName->GetUTF8String(&name);

    if (name[0] != 'o' || name[1] != 'n') {
        return PR_FALSE;
    }

    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeAtom) {
        if (entry->mAttributeAtom == aName) {
            return PR_TRUE;
            break;
        }
        ++entry;
    }

    return PR_FALSE;
}

static void
InitEventHandlerMap()
{
    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
        entry->mAttributeAtom = NS_NewAtom(entry->mAttributeName);
        ++entry;
    }
}


static void
FinishEventHandlerMap()
{
    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
        NS_IF_RELEASE(entry->mAttributeAtom);
        ++entry;
    }
}

//----------------------------------------------------------------------

static PRBool HasMutationListeners(nsIContent* aContent, PRUint32 aType)
{
  nsIDocument* doc = aContent->GetDocument();
  if (!doc)
    return PR_FALSE;

  nsIScriptGlobalObject *global = doc->GetScriptGlobalObject();
  if (!global)
    return PR_FALSE;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(global));
  if (!window)
    return PR_FALSE;

  if (!window->HasMutationListeners(aType))
    return PR_FALSE;

  // We know a mutation listener is registered, but it might not
  // be in our chain.  Check quickly to see.
  nsCOMPtr<nsIEventListenerManager> manager;

  for (nsIContent* curr = aContent; curr; curr = curr->GetParent()) {
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(curr));
    if (rec) {
      rec->GetListenerManager(getter_AddRefs(manager));
      if (manager) {
        PRBool hasMutationListeners = PR_FALSE;
        manager->HasMutationListeners(&hasMutationListeners);
        if (hasMutationListeners)
          return PR_TRUE;
      }
    }
  }

  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(doc));
  if (rec) {
    rec->GetListenerManager(getter_AddRefs(manager));
    if (manager) {
      PRBool hasMutationListeners = PR_FALSE;
      manager->HasMutationListeners(&hasMutationListeners);
      if (hasMutationListeners)
        return PR_TRUE;
    }
  }

  rec = do_QueryInterface(window);
  if (rec) {
    rec->GetListenerManager(getter_AddRefs(manager));
    if (manager) {
      PRBool hasMutationListeners = PR_FALSE;
      manager->HasMutationListeners(&hasMutationListeners);
      if (hasMutationListeners)
        return PR_TRUE;
    }
  }

  return PR_FALSE;
}

//----------------------------------------------------------------------

nsrefcnt             nsXULElement::gRefCnt;
nsIRDFService*       nsXULElement::gRDFService;

#ifdef XUL_PROTOTYPE_ATTRIBUTE_METERING
PRUint32             nsXULPrototypeAttribute::gNumElements;
PRUint32             nsXULPrototypeAttribute::gNumAttributes;
PRUint32             nsXULPrototypeAttribute::gNumEventHandlers;
PRUint32             nsXULPrototypeAttribute::gNumCacheTests;
PRUint32             nsXULPrototypeAttribute::gNumCacheHits;
PRUint32             nsXULPrototypeAttribute::gNumCacheSets;
PRUint32             nsXULPrototypeAttribute::gNumCacheFills;
#endif

//----------------------------------------------------------------------
// nsXULElement
//

nsXULElement::nsXULElement()
    : mPrototype(nsnull),
      mDocument(nsnull),
      mBindingParent(nsnull),
      mSlots(nsnull)
{
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumElements);
}


nsresult
nsXULElement::Init()
{
    if (gRefCnt++ == 0) {
        nsresult rv;

        rv = CallGetService(kRDFServiceCID, &gRDFService);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_FAILED(rv)) return rv;

        InitEventHandlerMap();
    }

    return NS_OK;
}

nsXULElement::~nsXULElement()
{
    if (mPrototype)
        mPrototype->Release();

    delete mSlots;

    // Clean up shared statics
    if (--gRefCnt == 0) {
        FinishEventHandlerMap();

        NS_IF_RELEASE(gRDFService);
    }
}


nsresult
nsXULElement::Create(nsXULPrototypeElement* aPrototype,
                     nsIDocument* aDocument,
                     PRBool aIsScriptable,
                     nsIContent** aResult)
{
    // Create an nsXULElement from a prototype
    NS_PRECONDITION(aPrototype != nsnull, "null ptr");
    if (! aPrototype)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsRefPtr<nsXULElement> element = new nsXULElement();
    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = element->Init();
    if (NS_FAILED(rv)) return rv;

    element->mPrototype = aPrototype;
    element->mDocument = aDocument;

    aPrototype->AddRef();

    if (aIsScriptable) {
        // Check each attribute on the prototype to see if we need to do
        // any additional processing and hookup that would otherwise be
        // done 'automagically' by SetAttr().
        for (PRUint32 i = 0; i < aPrototype->mNumAttributes; ++i)
            element->AddListenerFor(aPrototype->mAttributes[i].mName, PR_TRUE);
    }

    *aResult = NS_REINTERPRET_CAST(nsIStyledContent*, element.get());
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
NS_NewXULElement(nsIContent** aResult, nsINodeInfo *aNodeInfo)
{
    NS_PRECONDITION(aNodeInfo, "need nodeinfo for non-proto Create");

    *aResult = nsnull;

    // Create an nsXULElement with the specified namespace and tag.
    nsXULElement* element = new nsXULElement();
    NS_ENSURE_TRUE(element, NS_ERROR_OUT_OF_MEMORY);

    // anchor the element so an early return will clean up properly.
    nsCOMPtr<nsIContent> anchor = element;

    nsresult rv = element->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = element->EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    element->mSlots->mNodeInfo = aNodeInfo;

    anchor.swap(*aResult);

#ifdef DEBUG_ATTRIBUTE_STATS
    {
        gFaults.Create++; gFaults.Total++;
        nsAutoString tagstr;
        element->NodeInfo()->GetQualifiedName(tagstr);
        char *tagcstr = ToNewCString(tagstr);
        fprintf(stderr, "XUL: Heavyweight create of <%s>: %d/%d\n",
                tagcstr, gFaults.Create, gFaults.Total);
        nsMemory::Free(tagcstr);
        void *back[5];
        backtrace(back, sizeof(back) / sizeof(back[0]));
        backtrace_symbols_fd(back, sizeof(back) / sizeof(back[0]), 2);
    }
#endif

    return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_ADDREF(nsXULElement)
NS_IMPL_RELEASE(nsXULElement)

NS_INTERFACE_MAP_BEGIN(nsXULElement)
    NS_INTERFACE_MAP_ENTRY(nsIStyledContent)
    NS_INTERFACE_MAP_ENTRY(nsIContent)
    NS_INTERFACE_MAP_ENTRY(nsIXULContent)
    NS_INTERFACE_MAP_ENTRY(nsIXMLContent)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXULElement)
    NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
    NS_INTERFACE_MAP_ENTRY(nsIScriptEventHandlerOwner)
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventReceiver,
                                   nsDOMEventRTTearoff::Create(this))
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventTarget,
                                   nsDOMEventRTTearoff::Create(this))
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3EventTarget,
                                   nsDOMEventRTTearoff::Create(this))
    NS_INTERFACE_MAP_ENTRY(nsIChromeEventHandler)
    NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node,
                                   new nsNode3Tearoff(this))
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULElement)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXULElement)
    if (mDocument) {
        return mDocument->GetBindingManager()->GetBindingImplementation(this,
                                                                        aIID,
                                                                        aInstancePtr);
    } else
NS_INTERFACE_MAP_END


//----------------------------------------------------------------------
// nsIDOMNode interface

NS_IMETHODIMP
nsXULElement::GetNodeName(nsAString& aNodeName)
{
    NodeInfo()->GetQualifiedName(aNodeName);
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetNodeValue(nsAString& aNodeValue)
{
    SetDOMStringToNull(aNodeValue);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetNodeValue(const nsAString& aNodeValue)
{
    // The DOM spec says that when nodeValue is defined to be null "setting it
    // has no effect", so we don't throw an exception.
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ELEMENT_NODE;
  return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetParentNode(nsIDOMNode** aParentNode)
{
    nsIContent *parent = GetParent();
    if (parent) {
        return CallQueryInterface(parent, aParentNode);
    }

    if (mDocument) {
        nsIContent *thisIContent = this;

        if (mDocument->GetRootContent() == thisIContent) {
            // If we don't have a parent, and we're the root content
            // of the document, DOM says that our parent is the
            // document.
            return CallQueryInterface(mDocument, aParentNode);
        }
    }

    // A standalone element (i.e. one without a parent or a document)
    *aParentNode = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    nsresult rv = EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mSlots->mChildNodes) {
        mSlots->mChildNodes = new nsChildContentList(this);
        if (!mSlots->mChildNodes) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    
    NS_ADDREF(*aChildNodes = mSlots->mChildNodes);
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetFirstChild(nsIDOMNode** aFirstChild)
{
    nsIContent *child = GetChildAt(0);

    if (child) {
        return CallQueryInterface(child, aFirstChild);
    }

    *aFirstChild = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetLastChild(nsIDOMNode** aLastChild)
{
    PRUint32 count = GetChildCount();

    if (count > 0) {
        nsIContent *child = GetChildAt(count - 1);

        return CallQueryInterface(child, aLastChild);
    }

    *aLastChild = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    nsIContent *parent = GetParent();
    if (parent) {
        PRInt32 pos = parent->IndexOf(this);
        if (pos > 0) {
            nsIContent *prev = parent->GetChildAt(--pos);
            if (prev) {
                nsresult rv = CallQueryInterface(prev, aPreviousSibling);
                NS_ASSERTION(*aPreviousSibling, "not a DOM node");
                return rv;
            }
        }
    }

    // XXX Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their previous sibling.
    *aPreviousSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
    nsIContent *parent = GetParent();
    if (parent) {
        PRInt32 pos = parent->IndexOf(this);
        if (pos > -1) {
            nsIContent *next = parent->GetChildAt(++pos);
            if (next) {
                nsresult rv = CallQueryInterface(next, aNextSibling);
                NS_ASSERTION(*aNextSibling, "not a DOM Node");
                return rv;
            }
        }
    }

    // XXX Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    *aNextSibling = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
    NS_PRECONDITION(aAttributes, "nullpointer");

    nsresult rv = EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mSlots->mAttributeMap) {
        mSlots->mAttributeMap = new nsDOMAttributeMap(this);
        NS_ENSURE_TRUE(mSlots->mAttributeMap, NS_ERROR_OUT_OF_MEMORY);
    }

    NS_ADDREF(*aAttributes = mSlots->mAttributeMap);

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    nsIDocument *document = GetOwnerDoc();
    if (document) {
        return CallQueryInterface(document, aOwnerDocument);
    }

    *aOwnerDocument = nsnull;

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetNamespaceURI(nsAString& aNamespaceURI)
{
    return NodeInfo()->GetNamespaceURI(aNamespaceURI);
}


NS_IMETHODIMP
nsXULElement::GetPrefix(nsAString& aPrefix)
{
    NodeInfo()->GetPrefix(aPrefix);
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::SetPrefix(const nsAString& aPrefix)
{
    // XXX: Validate the prefix string!

    nsCOMPtr<nsINodeInfo> newNodeInfo;
    nsCOMPtr<nsIAtom> prefix;

    if (!aPrefix.IsEmpty()) {
        prefix = do_GetAtom(aPrefix);
        NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
    }

    nsresult rv = EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = nsContentUtils::PrefixChanged(mSlots->mNodeInfo, prefix,
                                       getter_AddRefs(newNodeInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(newNodeInfo, "trying to assign null nodeinfo!");
    mSlots->mNodeInfo = newNodeInfo;

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetLocalName(nsAString& aLocalName)
{
    NodeInfo()->GetLocalName(aLocalName);
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn)
{
    return nsGenericElement::doInsertBefore(this, aNewChild, aRefChild,
                                            aReturn);
}


NS_IMETHODIMP
nsXULElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn)
{
    return nsGenericElement::doReplaceChild(this, aNewChild, aOldChild,
                                            aReturn);
}


NS_IMETHODIMP
nsXULElement::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    return nsGenericElement::doRemoveChild(this, aOldChild, aReturn);
}


NS_IMETHODIMP
nsXULElement::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    return nsGenericElement::doInsertBefore(this, aNewChild, nsnull, aReturn);
}


NS_IMETHODIMP
nsXULElement::HasChildNodes(PRBool* aReturn)
{
    *aReturn = mAttrsAndChildren.ChildCount() > 0;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::HasAttributes(PRBool* aReturn)
{
    *aReturn = mAttrsAndChildren.AttrCount() > 0 ||
               (mPrototype && mPrototype->mNumAttributes > 0);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    nsresult rv;

    nsCOMPtr<nsIContent> result;

    // If we have a prototype, so will our clone.
    if (mPrototype) {
        rv = nsXULElement::Create(mPrototype, mDocument, PR_TRUE,
                                  getter_AddRefs(result));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        NS_ASSERTION(mSlots, "no prototype and no slots!");
        NS_ENSURE_TRUE(mSlots, NS_ERROR_UNEXPECTED);

        rv = NS_NewXULElement(getter_AddRefs(result), mSlots->mNodeInfo);
        NS_ENSURE_SUCCESS(rv, rv);

        // XXX setting document on nodes not in a document so XBL will bind
        // and chrome won't break. Make XBL bind to document-less nodes!
        result->SetDocument(mDocument, PR_TRUE, PR_TRUE);
    }

    // Copy attributes
    PRInt32 count = mAttrsAndChildren.AttrCount();
    for (PRInt32 i = 0; i < count; ++i) {
        const nsAttrName* name = mAttrsAndChildren.GetSafeAttrNameAt(i);
        nsAutoString valStr;
        mAttrsAndChildren.AttrAt(i)->ToString(valStr);
        rv = result->SetAttr(name->NamespaceID(), name->LocalName(),
                             name->GetPrefix(), valStr, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // XXX TODO: set up RDF generic builder n' stuff if there is a
    // 'datasources' attribute? This is really kind of tricky,
    // because then we'd need to -selectively- copy children that
    // -weren't- generated from RDF. Ugh. Forget it.

    // Note that we're _not_ copying mControllers.

    if (aDeep) {
        // Copy cloned children!
        PRInt32 i, count = mAttrsAndChildren.ChildCount();
        for (i = 0; i < count; ++i) {
            nsIContent* child = mAttrsAndChildren.ChildAt(i);

            NS_ASSERTION(child != nsnull, "null ptr");
            if (! child)
                return NS_ERROR_UNEXPECTED;

            nsCOMPtr<nsIDOMNode> domchild = do_QueryInterface(child);
            NS_ASSERTION(domchild != nsnull, "child is not a DOM node");
            if (! domchild)
                return NS_ERROR_UNEXPECTED;

            nsCOMPtr<nsIDOMNode> newdomchild;
            rv = domchild->CloneNode(PR_TRUE, getter_AddRefs(newdomchild));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIContent> newchild = do_QueryInterface(newdomchild);
            NS_ASSERTION(newchild != nsnull, "newdomchild is not an nsIContent");
            if (! newchild)
                return NS_ERROR_UNEXPECTED;

            rv = result->AppendChildTo(newchild, PR_FALSE, PR_FALSE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return CallQueryInterface(result, aReturn);
}


NS_IMETHODIMP
nsXULElement::Normalize()
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULElement::IsSupported(const nsAString& aFeature,
                          const nsAString& aVersion,
                          PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
// nsIDOMElement interface

NS_IMETHODIMP
nsXULElement::GetTagName(nsAString& aTagName)
{
    NodeInfo()->GetQualifiedName(aTagName);
    return NS_OK;
}

nsINodeInfo *
nsXULElement::GetNodeInfo() const
{
    return NodeInfo();
}

NS_IMETHODIMP
nsXULElement::GetAttribute(const nsAString& aName,
                           nsAString& aReturn)
{
    const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);
    if (!name) {
        // XXX should be SetDOMStringToNull(aReturn);
        // See bug 232598
        aReturn.Truncate();

        return NS_OK;
    }

    GetAttr(name->NamespaceID(), name->LocalName(), aReturn);

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::SetAttribute(const nsAString& aName,
                           const nsAString& aValue)
{
    const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

    if (!name) {
        nsresult rv = nsContentUtils::CheckQName(aName, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aName);
        NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

        return SetAttr(kNameSpaceID_None, nameAtom, aValue, PR_TRUE);
    }

    return SetAttr(name->NamespaceID(), name->LocalName(), name->GetPrefix(),
                   aValue, PR_TRUE);
}


NS_IMETHODIMP
nsXULElement::RemoveAttribute(const nsAString& aName)
{
    const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

    if (!name) {
        return NS_OK;
    }

    // Hold a strong reference here so that the atom or nodeinfo doesn't go
    // away during UnsetAttr. If it did UnsetAttr would be left with a
    // dangling pointer as argument without knowing it.
    nsAttrName tmp(*name);

    return UnsetAttr(name->NamespaceID(), name->LocalName(), PR_TRUE);
}


NS_IMETHODIMP
nsXULElement::GetAttributeNode(const nsAString& aName,
                               nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn, "nullpointer");
    *aReturn = nsnull;

    nsCOMPtr<nsIDOMNamedNodeMap> map;
    nsresult rv = GetAttributes(getter_AddRefs(map));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node;
    rv = map->GetNamedItem(aName, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    if (node) {
        return CallQueryInterface(node, aReturn);
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn, "nullpointer");
    NS_ENSURE_ARG_POINTER(aNewAttr);

    *aReturn = nsnull;

    nsCOMPtr<nsIDOMNamedNodeMap> map;
    nsresult rv = GetAttributes(getter_AddRefs(map));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> returnNode;
    rv = map->SetNamedItem(aNewAttr, getter_AddRefs(returnNode));
    NS_ENSURE_SUCCESS(rv, rv);

    if (returnNode) {
        return CallQueryInterface(returnNode, aReturn);
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn, "nullpointer");
    NS_ENSURE_ARG_POINTER(aOldAttr);

    *aReturn = nsnull;

    nsCOMPtr<nsIDOMNamedNodeMap> map;
    nsresult rv = GetAttributes(getter_AddRefs(map));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString name;
    rv = aOldAttr->GetName(name);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node;
    rv = map->RemoveNamedItem(name, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    if (node) {
        return CallQueryInterface(node, aReturn);
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetElementsByTagName(const nsAString& aTagname,
                                   nsIDOMNodeList** aReturn)
{
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aTagname);
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIContentList> list;
    NS_GetContentList(mDocument, nameAtom, kNameSpaceID_Unknown, this,
                      getter_AddRefs(list));
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    return CallQueryInterface(list, aReturn);
}

NS_IMETHODIMP
nsXULElement::GetAttributeNS(const nsAString& aNamespaceURI,
                             const nsAString& aLocalName,
                             nsAString& aReturn)
{
    nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
    PRInt32 nsid;

    nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI,
                                                          &nsid);

    if (nsid == kNameSpaceID_Unknown) {
        // Unknown namespace means no attr...

        aReturn.Truncate();
        return NS_OK;
    }

    GetAttr(nsid, name, aReturn);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetAttributeNS(const nsAString& aNamespaceURI,
                             const nsAString& aQualifiedName,
                             const nsAString& aValue)
{
    nsCOMPtr<nsINodeInfo> ni;
    nsresult rv =
        nsContentUtils::GetNodeInfoFromQName(aNamespaceURI, aQualifiedName,
                                             NodeInfo()->NodeInfoManager(),
                                             getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

    return SetAttr(ni->NamespaceID(), ni->NameAtom(), ni->GetPrefixAtom(),
                   aValue, PR_TRUE);
}

NS_IMETHODIMP
nsXULElement::RemoveAttributeNS(const nsAString& aNamespaceURI,
                                const nsAString& aLocalName)
{
    PRInt32 nameSpaceId;
    nsCOMPtr<nsIAtom> tag = do_GetAtom(aLocalName);

    nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI,
                                                          &nameSpaceId);

    return UnsetAttr(nameSpaceId, tag, PR_TRUE);
}

NS_IMETHODIMP
nsXULElement::GetAttributeNodeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
                                 nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn, "nullpointer");

    *aReturn = nsnull;

    nsCOMPtr<nsIDOMNamedNodeMap> map;
    nsresult rv = GetAttributes(getter_AddRefs(map));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> node;
    rv = map->GetNamedItemNS(aNamespaceURI, aLocalName, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    if (node) {
        return CallQueryInterface(node, aReturn);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetAttributeNodeNS(nsIDOMAttr* aNewAttr,
                                 nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn, "nullpointer");
    NS_ENSURE_ARG_POINTER(aNewAttr);

    *aReturn = nsnull;

    nsCOMPtr<nsIDOMNamedNodeMap> map;
    nsresult rv = GetAttributes(getter_AddRefs(map));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> returnNode;
    rv = map->SetNamedItemNS(aNewAttr, getter_AddRefs(returnNode));
    NS_ENSURE_SUCCESS(rv, rv);

    if (returnNode) {
        return CallQueryInterface(returnNode, aReturn);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMNodeList** aReturn)
{
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aLocalName);
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    PRInt32 nameSpaceId = kNameSpaceID_Unknown;

    nsCOMPtr<nsIContentList> list;

    if (!aNamespaceURI.EqualsLiteral("*")) {
        nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI,
                                                              &nameSpaceId);

        if (nameSpaceId == kNameSpaceID_Unknown) {
            // Unknown namespace means no matches, we create an empty list...
            NS_GetContentList(mDocument, nsnull, kNameSpaceID_None, nsnull,
                              getter_AddRefs(list));
            NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
        }
    }

    if (!list) {
        NS_GetContentList(mDocument, nameAtom, nameSpaceId, this,
                          getter_AddRefs(list));
        NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }

    return CallQueryInterface(list, aReturn);
}

NS_IMETHODIMP
nsXULElement::HasAttribute(const nsAString& aName, PRBool* aReturn)
{
    NS_PRECONDITION(aReturn, "nullpointer");

    const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);
    *aReturn = (name != nsnull);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::HasAttributeNS(const nsAString& aNamespaceURI,
                             const nsAString& aLocalName,
                             PRBool* aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
    PRInt32 nsid;

    nsContentUtils::GetNSManagerWeakRef()->GetNameSpaceID(aNamespaceURI, &nsid);

    if (nsid == kNameSpaceID_Unknown) {
        // Unknown namespace means no attr...
        *aReturn = PR_FALSE;
        return NS_OK;
    }

    *aReturn = HasAttr(nsid, name);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetElementsByAttribute(const nsAString& aAttribute,
                                     const nsAString& aValue,
                                     nsIDOMNodeList** aReturn)
{
    nsCOMPtr<nsIAtom> attrAtom(do_GetAtom(aAttribute));
    NS_ENSURE_TRUE(attrAtom, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIContentList> list =
        new nsContentList(GetDocument(),
                          nsXULDocument::MatchAttribute,
                          aValue,
                          this,
                          PR_TRUE,
                          attrAtom,
                          kNameSpaceID_None);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

    return CallQueryInterface(list, aReturn);
}


//----------------------------------------------------------------------
// nsIXMLContent interface

NS_IMETHODIMP
nsXULElement::MaybeTriggerAutoLink(nsIDocShell *aShell)
{
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIXULContent interface

NS_IMETHODIMP_(PRUint32)
nsXULElement::PeekChildCount() const
{
    return mAttrsAndChildren.ChildCount();
}

NS_IMETHODIMP
nsXULElement::SetLazyState(LazyState aFlags)
{
    nsresult rv = EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    mSlots->mLazyState = mSlots->mLazyState | aFlags;

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::ClearLazyState(LazyState aFlags)
{
    // No need to clear a flag we've never set.
    if (mSlots) {
        mSlots->mLazyState &= ~aFlags;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetLazyState(LazyState aFlag, PRBool& aResult)
{
    aResult = mSlots && mSlots->mLazyState & aFlag;

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::AddScriptEventListener(nsIAtom* aName, const nsAString& aValue)
{
    if (! mDocument)
        return NS_OK; // XXX

    nsresult rv;

    nsISupports *target = NS_STATIC_CAST(nsIContent *, this);
    PRBool defer = PR_TRUE;

    nsCOMPtr<nsIEventListenerManager> manager;

    nsIContent *root = mDocument->GetRootContent();
    nsCOMPtr<nsIContent> content(do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, this)));
    if ((!root || root == content) && !NodeInfo()->Equals(nsXULAtoms::overlay)) {
        nsIScriptGlobalObject *global = mDocument->GetScriptGlobalObject();

        nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(global);
        if (! receiver)
            return NS_ERROR_UNEXPECTED;

        rv = receiver->GetListenerManager(getter_AddRefs(manager));

        target = global;
        defer = PR_FALSE;
    }
    else {
        rv = GetListenerManager(getter_AddRefs(manager));
    }

    if (NS_FAILED(rv)) return rv;

    return manager->AddScriptEventListener(target, aName, aValue, defer);
}

nsresult
nsXULElement::GetListenerManager(nsIEventListenerManager** aResult)
{
    if (!mListenerManager) {
        nsresult rv =
            NS_NewEventListenerManager(getter_AddRefs(mListenerManager));
        if (NS_FAILED(rv))
            return rv;

        mListenerManager->SetListenerTarget(NS_STATIC_CAST(nsIContent*, this));
    }

    *aResult = mListenerManager;
    NS_ADDREF(*aResult);
    return NS_OK;
}

PRBool
nsXULElement::IsFocusable(PRInt32 *aTabIndex)
{
  // Use incoming tabindex as default value
  PRInt32 tabIndex = aTabIndex? *aTabIndex : -1;
  PRBool disabled = tabIndex < 0;
  nsCOMPtr<nsIDOMXULControlElement> xulControl = 
    do_QueryInterface(NS_STATIC_CAST(nsIContent*, this));
  if (xulControl) {
    xulControl->GetDisabled(&disabled);
    if (disabled) {
      tabIndex = -1;  // Can't tab to disabled elements
    }
    else if (HasAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex)) {
      // If attribute not set, will use default value passed in
      xulControl->GetTabIndex(&tabIndex);
    }
  }

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }

  return tabIndex >= 0 || (!disabled && HasAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex));
}


//----------------------------------------------------------------------
// nsIScriptEventHandlerOwner interface

nsresult
nsXULElement::GetCompiledEventHandler(nsIAtom *aName, void** aHandler)
{
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheTests);
    *aHandler = nsnull;

    nsXULPrototypeAttribute *attr =
        FindPrototypeAttribute(kNameSpaceID_None, aName);
    if (attr) {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheHits);
        *aHandler = attr->mEventHandler;
    }

    return NS_OK;
}

nsresult
nsXULElement::CompileEventHandler(nsIScriptContext* aContext,
                                  void* aTarget,
                                  nsIAtom *aName,
                                  const nsAString& aBody,
                                  const char* aURL,
                                  PRUint32 aLineNo,
                                  void** aHandler)
{
    nsresult rv;
    JSObject* scopeObject;

    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheSets);

    nsIScriptContext *context;
    if (mPrototype) {
        // It'll be shared among the instances of the prototype.
        // Use null for the scope object when precompiling shared
        // prototype scripts.
        scopeObject = nsnull;

        // Use the prototype document's special context.  Because
        // scopeObject is null, the JS engine has no other source of
        // <the-new-shared-event-handler>.__proto__ than to look in
        // cx->globalObject for Function.prototype.  That prototype
        // keeps the global object alive, so if we use this document's
        // global object, we'll be putting something in the prototype
        // that protects this document's global object from GC.
        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
        NS_ENSURE_TRUE(xuldoc, NS_ERROR_UNEXPECTED);

        nsCOMPtr<nsIXULPrototypeDocument> protodoc;
        rv = xuldoc->GetMasterPrototype(getter_AddRefs(protodoc));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(protodoc, NS_ERROR_UNEXPECTED);

        nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner =
            do_QueryInterface(protodoc);
        nsIScriptGlobalObject* global = globalOwner->GetScriptGlobalObject();
        NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

        context = global->GetContext();
    }
    else {
        // We don't have a prototype; do a one-off compile.
        NS_ASSERTION(aTarget != nsnull, "no prototype and no target?!");
        scopeObject = NS_REINTERPRET_CAST(JSObject*, aTarget);
        context = aContext;
    }

    // Compile the event handler
    const char *eventName = nsContentUtils::GetEventArgName(kNameSpaceID_XUL);
    rv = context->CompileEventHandler(scopeObject, aName, eventName, aBody,
                                      aURL, aLineNo, !scopeObject,
                                      aHandler);
    if (NS_FAILED(rv)) return rv;

    if (! scopeObject) {
        // If it's a shared handler, we need to bind the shared
        // function object to the real target.

        // XXX: Shouldn't this use context and not aContext?
        rv = aContext->BindCompiledEventHandler(aTarget, aName, *aHandler);
        if (NS_FAILED(rv)) return rv;
    }

    nsXULPrototypeAttribute *attr =
        FindPrototypeAttribute(kNameSpaceID_None, aName);
    if (attr) {
        XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheFills);
        attr->mEventHandler = *aHandler;

        if (attr->mEventHandler) {
            JSContext *cx = (JSContext*) context->GetNativeContext();
            if (!cx)
                return NS_ERROR_UNEXPECTED;

            rv = AddJSGCRoot(&attr->mEventHandler,
                             "nsXULPrototypeAttribute::mEventHandler");
            if (NS_FAILED(rv)) return rv;
        }
    }

    return NS_OK;
}


void
nsXULElement::AddListenerFor(const nsAttrName& aName,
                             PRBool aCompileEventHandlers)
{
    // If appropriate, add a popup listener and/or compile the event
    // handler. Called when we change the element's document, create a
    // new element, change an attribute's value, etc.
    // Eventlistenener-attributes are always in the null namespace
    if (aName.IsAtom()) {
        nsIAtom *attr = aName.Atom();
        MaybeAddPopupListener(attr);
        if (aCompileEventHandlers && IsEventHandler(attr)) {
            nsAutoString value;
            GetAttr(kNameSpaceID_None, attr, value);
            AddScriptEventListener(attr, value);
        }
    }
}

void
nsXULElement::MaybeAddPopupListener(nsIAtom* aLocalName)
{
    // If appropriate, add a popup listener. Called when we change the
    // element's document, create a new element, change an attribute's
    // value, etc.
    if (aLocalName == nsXULAtoms::menu ||
        aLocalName == nsXULAtoms::contextmenu ||
        // XXXdwh popup and context are deprecated
        aLocalName == nsXULAtoms::popup ||
        aLocalName == nsXULAtoms::context) {
        AddPopupListener(aLocalName);
    }
}

//----------------------------------------------------------------------
//
// nsIContent interface
//

void
nsXULElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                          PRBool aCompileEventHandlers)
{
    if (aDocument != mDocument) {
        if (mDocument) {
          // Notify XBL- & nsIAnonymousContentCreator-generated
          // anonymous content that the document is changing.
          nsIBindingManager *bindingManager = mDocument->GetBindingManager();
          NS_ASSERTION(bindingManager, "no binding manager");
          if (bindingManager) {
            bindingManager->ChangeDocumentFor(this, mDocument, aDocument);
          }

          nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(mDocument));
          nsDoc->SetBoxObjectFor(this, nsnull);

          if (mSlots && mSlots->mHasProperties) {
              mDocument->PropertyTable()->DeleteAllPropertiesFor(this);
          }
        }

        // mControllers can own objects that are implemented
        // in JavaScript (such as some implementations of
        // nsIControllers.  These objects prevent their global
        // object's script object from being garbage collected,
        // which means JS continues to hold an owning reference
        // to the GlobalWindowImpl, which owns the document,
        // which owns this content.  That's a cycle, so we break
        // it here.  (It might be better to break this by releasing
        // mDocument in GlobalWindowImpl::SetDocShell, but I'm not
        // sure whether that would fix all possible cycles through
        // mControllers.)
        if (!aDocument && mSlots) {
            mSlots->mControllers = nsnull; // Forces release
        }

        if (mListenerManager)
          mListenerManager->SetListenerTarget(nsnull);
        mListenerManager = nsnull;

        mDocument = aDocument;

        if (mDocument) {
            // When we SetDocument(), we're either adding an element
            // into the document that wasn't there before, or we're
            // moving the element from one document to
            // another. Regardless, we need to (re-)initialize several
            // attributes that are dependant on the document. Do that
            // now.
            PRInt32 count = mAttrsAndChildren.AttrCount();
            PRBool haveLocalAttributes = (count > 0);
            PRInt32 i;
            for (i = 0; i < count; i++) {
                AddListenerFor(*mAttrsAndChildren.GetSafeAttrNameAt(i),
                               aCompileEventHandlers);
            }

            if (mPrototype) {
                PRInt32 count = mPrototype->mNumAttributes;
                for (i = 0; i < count; i++) {
                    nsXULPrototypeAttribute *protoattr =
                        &mPrototype->mAttributes[i];

                    // Don't clobber a locally modified attribute.
                    if (haveLocalAttributes &&
                        mAttrsAndChildren.GetAttr(protoattr->mName.LocalName(), 
                                                  protoattr->mName.NamespaceID())) {
                        continue;
                    }

                    AddListenerFor(protoattr->mName, aCompileEventHandlers);
                }
            }
        }
    }

    if (aDeep) {
        PRInt32 i;
        for (i = mAttrsAndChildren.ChildCount() - 1; i >= 0; --i) {
            mAttrsAndChildren.ChildAt(i)->SetDocument(aDocument, aDeep,
                                                      aCompileEventHandlers);
        }
    }
}

void
nsXULElement::SetParent(nsIContent* aParent)
{
    mParentPtrBits = NS_REINTERPRET_CAST(PtrBits, aParent);

    if (aParent) {
      nsIContent* bindingPar = aParent->GetBindingParent();
      if (bindingPar)
        SetBindingParent(bindingPar);
    }
}

PRBool
nsXULElement::IsNativeAnonymous() const
{
    // XXX Workaround to prevent code from discovering scrollbars - bug 251197
    return NodeInfo()->Equals(nsXULAtoms::scrollbar);
}

void
nsXULElement::SetNativeAnonymous(PRBool aAnonymous)
{
    // XXX Need to make this actually do something - bug 165110
}

PRUint32
nsXULElement::GetChildCount() const
{
    if (NS_FAILED(EnsureContentsGenerated())) {
        return 0;
    }

    return PeekChildCount();
}

nsIContent *
nsXULElement::GetChildAt(PRUint32 aIndex) const
{
    if (NS_FAILED(EnsureContentsGenerated())) {
        return nsnull;
    }

    return mAttrsAndChildren.GetSafeChildAt(aIndex);
}

PRInt32
nsXULElement::IndexOf(nsIContent* aPossibleChild) const
{
    if (NS_FAILED(EnsureContentsGenerated())) {
        return -1;
    }

    return mAttrsAndChildren.IndexOfChild(aPossibleChild);
}

nsresult
nsXULElement::InsertChildAt(nsIContent* aKid, PRUint32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument)
{
    nsresult rv = EnsureContentsGenerated();
    NS_ENSURE_SUCCESS(rv, rv);

    NS_PRECONDITION(nsnull != aKid, "null ptr");

    // Make sure that we're not trying to insert the same child
    // twice. If we do, the DOM APIs (e.g., GetNextSibling()), will
    // freak out.
    NS_ASSERTION(mAttrsAndChildren.IndexOfChild(aKid) < 0,
                 "element is already a child");

    PRBool isAppend = aIndex == mAttrsAndChildren.ChildCount();

    mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);

    if (aIndex > mAttrsAndChildren.ChildCount()) {
      // XXX This *does* happen, probably a bug in the overlay code.
      // See bug 233641.
      return NS_ERROR_FAILURE;
    }

    rv = mAttrsAndChildren.InsertChildAt(aKid, aIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    aKid->SetParent(this);
    //nsRange::OwnerChildInserted(this, aIndex);

    if (mDocument) {
        aKid->SetDocument(mDocument, aDeepSetDocument, PR_TRUE);

        if (aNotify) {
            if (isAppend) {
                mDocument->ContentAppended(this, aIndex);
            } else {
                mDocument->ContentInserted(this, aKid, aIndex);
            }
        }

        if (HasMutationListeners(this,
                                 NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
            nsMutationEvent mutation(NS_MUTATION_NODEINSERTED, aKid);
            mutation.mRelatedNode =
                do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, this));

            nsEventStatus status = nsEventStatus_eIgnore;
            aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT,
                                 &status);
        }

    }

    return NS_OK;
}

nsresult
nsXULElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                            PRBool aDeepSetDocument)
{
    nsresult rv = EnsureContentsGenerated();
    NS_ENSURE_SUCCESS(rv, rv);

    NS_PRECONDITION((nsnull != aKid) && (aKid != NS_STATIC_CAST(nsIStyledContent*, this)), "null ptr");

    mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);

    rv = mAttrsAndChildren.AppendChild(aKid);
    NS_ENSURE_SUCCESS(rv, rv);

    aKid->SetParent(this);
    // ranges don't need adjustment since new child is at end of list

    if (mDocument) {
        aKid->SetDocument(mDocument, aDeepSetDocument, PR_TRUE);

        if (aNotify) {
            mDocument->ContentAppended(this,
                                       mAttrsAndChildren.ChildCount() - 1);
        }

        if (HasMutationListeners(this,
                                 NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
            nsMutationEvent mutation(NS_MUTATION_NODEINSERTED, aKid);
            mutation.mRelatedNode =
                do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, this));

            nsEventStatus status = nsEventStatus_eIgnore;
            aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
        }
    }

    return NS_OK;
}

nsresult
nsXULElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
    nsresult rv = EnsureContentsGenerated();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> oldKid = mAttrsAndChildren.ChildAt(aIndex);
    NS_ENSURE_TRUE(oldKid, NS_ERROR_FAILURE);

    mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);

    if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
      nsMutationEvent mutation(NS_MUTATION_NODEREMOVED, oldKid);
      mutation.mRelatedNode =
          do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, this));

      nsEventStatus status = nsEventStatus_eIgnore;
      oldKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
    }

    // On the removal of a <treeitem>, <treechildren>, or <treecell> element,
    // the possibility exists that some of the items in the removed subtree
    // are selected (and therefore need to be deselected). We need to account for this.
    nsCOMPtr<nsIDOMXULMultiSelectControlElement> controlElement;
    nsCOMPtr<nsIListBoxObject> listBox;
    PRBool fireSelectionHandler = PR_FALSE;

    // -1 = do nothing, -2 = null out current item
    // anything else = index to re-set as current
    PRInt32 newCurrentIndex = -1;

    nsINodeInfo *ni = oldKid->GetNodeInfo();
    if (ni && ni->Equals(nsXULAtoms::listitem, kNameSpaceID_XUL)) {
      // This is the nasty case. We have (potentially) a slew of selected items
      // and cells going away.
      // First, retrieve the tree.
      // Check first whether this element IS the tree
      controlElement = do_QueryInterface((nsIDOMXULElement*)this);

      // If it's not, look at our parent
      if (!controlElement)
        rv = GetParentTree(getter_AddRefs(controlElement));
      if (controlElement) {
        nsCOMPtr<nsIDOMNode> parentKid = do_QueryInterface(oldKid);
        // Iterate over all of the items and find out if they are contained inside
        // the removed subtree.
        PRInt32 length;
        controlElement->GetSelectedCount(&length);
        for (PRInt32 i = 0; i < length; i++) {
          nsCOMPtr<nsIDOMXULSelectControlItemElement> node;
          controlElement->GetSelectedItem(i, getter_AddRefs(node));
          nsCOMPtr<nsIDOMNode> selNode(do_QueryInterface(node));
          if (selNode == parentKid && NS_SUCCEEDED(rv = controlElement->RemoveItemFromSelection(node))) {
            length--;
            i--;
            fireSelectionHandler = PR_TRUE;
          }
        }

        nsCOMPtr<nsIDOMXULSelectControlItemElement> curItem;
        controlElement->GetCurrentItem(getter_AddRefs(curItem));
        nsCOMPtr<nsIDOMNode> curNode = do_QueryInterface(curItem);
        if (IsAncestor(parentKid, curNode)) {
            // Current item going away
            nsCOMPtr<nsIBoxObject> box;
            controlElement->GetBoxObject(getter_AddRefs(box));
            listBox = do_QueryInterface(box);
            if (listBox) {
                nsCOMPtr<nsIDOMElement> domElem = do_QueryInterface(parentKid);
                if (domElem)
                    listBox->GetIndexOfItem(domElem, &newCurrentIndex);
            }

            // If any of this fails, we'll just set the current item to null
            if (newCurrentIndex == -1)
              newCurrentIndex = -2;
        }
      }
    }

    mAttrsAndChildren.RemoveChildAt(aIndex);
    //nsRange::OwnerChildRemoved(this, aIndex, oldKid);
    if (aNotify && mDocument) {
        mDocument->ContentRemoved(this, oldKid, aIndex);
    }

    if (newCurrentIndex == -2)
        controlElement->SetCurrentItem(nsnull);
    else if (newCurrentIndex > -1) {
        // Make sure the index is still valid
        PRInt32 treeRows;
        listBox->GetRowCount(&treeRows);
        if (treeRows > 0) {
            newCurrentIndex = PR_MIN((treeRows - 1), newCurrentIndex);
            nsCOMPtr<nsIDOMElement> newCurrentItem;
            listBox->GetItemAtIndex(newCurrentIndex, getter_AddRefs(newCurrentItem));
            nsCOMPtr<nsIDOMXULSelectControlItemElement> xulCurItem = do_QueryInterface(newCurrentItem);
            if (xulCurItem)
                controlElement->SetCurrentItem(xulCurItem);
        } else {
            controlElement->SetCurrentItem(nsnull);
        }
    }

    if (fireSelectionHandler) {
      nsCOMPtr<nsIDOMDocumentEvent> doc(do_QueryInterface(mDocument));
      nsCOMPtr<nsIDOMEvent> event;
      doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
      if (event) {
        event->InitEvent(NS_LITERAL_STRING("select"), PR_FALSE, PR_TRUE);
        PRBool noDefault;
        nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);
        target->DispatchEvent(event, &noDefault);
      }
    }

    // This will cause the script object to be unrooted for each
    // element in the subtree.
    oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);

    // We've got no mo' parent.
    oldKid->SetParent(nsnull);

    return NS_OK;
}

void
nsXULElement::GetNameSpaceID(PRInt32* aNameSpaceID) const
{
    *aNameSpaceID = NodeInfo()->NamespaceID();
}

nsIAtom *
nsXULElement::Tag() const
{
    return NodeInfo()->NameAtom();
}

already_AddRefed<nsINodeInfo>
nsXULElement::GetExistingAttrNameFromQName(const nsAString& aStr) const
{
    const nsAttrName* name = InternalGetExistingAttrNameFromQName(aStr);
    if (!name) {
        return nsnull;
    }

    nsINodeInfo* nodeInfo;
    if (name->IsAtom()) {
        NodeInfo()->NodeInfoManager()->GetNodeInfo(name->Atom(), nsnull,
                                                   kNameSpaceID_None, &nodeInfo);
    }
    else {
        NS_ADDREF(nodeInfo = name->NodeInfo());
    }

    return nodeInfo;
}

void
nsXULElement::UnregisterAccessKey(const nsAString& aOldValue)
{
    // If someone changes the accesskey, unregister the old one
    //
    if (mDocument && !aOldValue.IsEmpty()) {
        nsIPresShell *shell = mDocument->GetShellAt(0);

        if (shell) {
            nsIContent *content = this;

            // find out what type of content node this is
            if (NodeInfo()->Equals(nsXULAtoms::label)) {
                // For anonymous labels the unregistering must
                // occur on the binding parent control.
                content = GetBindingParent();
            }

            if (content) {
                shell->GetPresContext()->EventStateManager()->
                    UnregisterAccessKey(content, aOldValue.First());
            }
        }
    }
}

// XXX attribute code swiped from nsGenericContainerElement
// this class could probably just use nsGenericContainerElement
// needed to maintain attribute namespace ID as well as ordering
// NOTE: Changes to this function may need to be made in
// |SetInlineStyleRule| as well.
nsresult
nsXULElement::SetAttr(PRInt32 aNamespaceID, nsIAtom* aName, nsIAtom* aPrefix,
                      const nsAString& aValue, PRBool aNotify)
{
    nsAutoString oldValue;
    PRBool hasListeners = PR_FALSE;
    PRBool modification = PR_FALSE;

    if (mDocument) {
        PRBool isAccessKey = aName == nsXULAtoms::accesskey &&
                             aNamespaceID == kNameSpaceID_None;
        hasListeners = nsGenericElement::HasMutationListeners(this,
            NS_EVENT_BITS_MUTATION_ATTRMODIFIED);

        // If we have no listeners and aNotify is false, we are almost
        // certainly coming from the content sink and will almost certainly
        // have no previous value.  Even if we do, setting the value is cheap
        // when we have no listeners and don't plan to notify.  The check for
        // aNotify here is an optimization, the check for haveListeners is a
        // correctness issue.
        // The check for isAccessKey is so that we get the old value and can
        // unregister the old key.
        if (hasListeners || aNotify || isAccessKey) {
            // Don't do any update if old == new.
            const nsAttrValue* attrVal =
                mAttrsAndChildren.GetAttr(aName, aNamespaceID);
            if (attrVal) {
                modification = PR_TRUE;
                attrVal->ToString(oldValue);
                if (aValue.Equals(oldValue)) {
                    return NS_OK;
                }
            }

            // If the accesskey attribute changes, unregister it here. It will
            // be registered for the new value in the relevant frames. Also see
            // nsAreaFrame, nsBoxFrame and nsTextBoxFrame's AttributeChanged
            // If we want to merge with nsGenericElement then we could maybe
            // do this in WillChangeAttr instead. That is only called when
            // aNotify is true, but that might be enough.
            if (isAccessKey) {
                UnregisterAccessKey(oldValue);
            }
        }
    }

    // XXX UnsetAttr handles more attributes then we do. See bug 233642.

    // Parse into a nsAttrValue

    // WARNING!!
    // This code is largely duplicated in nsXULPrototypeElement::SetAttrAt.
    // Any changes should be made to both functions.
    nsAttrValue attrValue;
    if (aNamespaceID == kNameSpaceID_None) {
        if (aName == nsXULAtoms::style) {
            nsGenericHTMLElement::ParseStyleAttribute(this, PR_TRUE, aValue,
                                                      attrValue);
        }
        else if (aName == nsXULAtoms::id &&
                 !aValue.IsEmpty()) {
            // Store id as atom.
            // id="" means that the element has no id. Not that it has
            // emptystring as id.
            attrValue.ParseAtom(aValue);
        }
        else if (aName == nsXULAtoms::clazz) {
            attrValue.ParseAtomArray(aValue);
        }
        else {
            attrValue.ParseStringOrAtom(aValue);
        }

        // Add popup and event listeners. We can't call AddListenerFor since
        // the attribute isn't set yet.
        MaybeAddPopupListener(aName);
        if (IsEventHandler(aName)) {
            AddScriptEventListener(aName, aValue);
        }

        // Hide chrome if needed
        if (aName == nsXULAtoms::hidechrome &&
            NodeInfo()->Equals(nsXULAtoms::window)) {
            HideWindowChrome(NS_LITERAL_STRING("true").Equals(aValue));
        }

        // XXX need to check if they're changing an event handler: if so, then we need
        // to unhook the old one.
    }
    else {
        attrValue.ParseStringOrAtom(aValue);
    }

    return SetAttrAndNotify(aNamespaceID, aName, aPrefix, oldValue,
                            attrValue, modification, hasListeners, aNotify);
}

nsresult
nsXULElement::SetAttrAndNotify(PRInt32 aNamespaceID,
                               nsIAtom* aAttribute,
                               nsIAtom* aPrefix,
                               const nsAString& aOldValue,
                               nsAttrValue& aParsedValue,
                               PRBool aModification,
                               PRBool aFireMutation,
                               PRBool aNotify)
{
    nsresult rv;
    PRUint8 modType = aModification ?
        NS_STATIC_CAST(PRUint8, nsIDOMMutationEvent::MODIFICATION) :
        NS_STATIC_CAST(PRUint8, nsIDOMMutationEvent::ADDITION);

    mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);
    if (aNotify && mDocument) {
        mDocument->AttributeWillChange(this, aNamespaceID, aAttribute);
    }

    if (aNamespaceID == kNameSpaceID_None) {
        rv = mAttrsAndChildren.SetAndTakeAttr(aAttribute, aParsedValue);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsCOMPtr<nsINodeInfo> ni;
        rv = NodeInfo()->NodeInfoManager()->GetNodeInfo(aAttribute, aPrefix,
                                                        aNamespaceID,
                                                        getter_AddRefs(ni));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mAttrsAndChildren.SetAndTakeAttr(ni, aParsedValue);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mDocument) {
        nsCOMPtr<nsIXBLBinding> binding;
        mDocument->GetBindingManager()->GetBinding(this, getter_AddRefs(binding));
        if (binding) {
            binding->AttributeChanged(aAttribute, aNamespaceID, PR_FALSE, aNotify);
        }

        if (aFireMutation) {
            nsCOMPtr<nsIDOMEventTarget> node =
                do_QueryInterface(NS_STATIC_CAST(nsIContent *, this));
            nsMutationEvent mutation(NS_MUTATION_ATTRMODIFIED, node);

            nsAutoString attrName;
            aAttribute->ToString(attrName);
            nsCOMPtr<nsIDOMAttr> attrNode;
            GetAttributeNode(attrName, getter_AddRefs(attrNode));
            mutation.mRelatedNode = attrNode;

            mutation.mAttrName = aAttribute;
            nsAutoString newValue;
            // We don't really need to call GetAttr here, but lets do it
            // anyway to ease future codeshare with nsGenericHTMLElement
            // which has to call GetAttr here due to enums.
            GetAttr(aNamespaceID, aAttribute, newValue);
            if (!newValue.IsEmpty()) {
                mutation.mNewAttrValue = do_GetAtom(newValue);
            }
            if (!aOldValue.IsEmpty()) {
                mutation.mPrevAttrValue = do_GetAtom(aOldValue);
            }
            mutation.mAttrChange = modType;
            nsEventStatus status = nsEventStatus_eIgnore;
            HandleDOMEvent(nsnull, &mutation, nsnull,
                           NS_EVENT_FLAG_INIT, &status);
        }

        if (aNotify) {
            mDocument->AttributeChanged(this, aNamespaceID, aAttribute, modType);
        }
    }

    return NS_OK;
}

const nsAttrName*
nsXULElement::InternalGetExistingAttrNameFromQName(const nsAString& aStr) const
{
    NS_ConvertUTF16toUTF8 name(aStr);
    const nsAttrName* attrName =
        mAttrsAndChildren.GetExistingAttrNameFromQName(name);
    if (attrName) {
        return attrName;
    }

    if (mPrototype) {
        PRUint32 i;
        for (i = 0; i < mPrototype->mNumAttributes; ++i) {
            attrName = &mPrototype->mAttributes[i].mName;
            if (attrName->QualifiedNameEquals(name)) {
                return attrName;
            }
        }
    }

    return nsnull;
}

nsresult
nsXULElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                      nsAString& aResult) const
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
                 "must have a real namespace ID!");

    const nsAttrValue* val = FindLocalOrProtoAttr(aNameSpaceID, aName);

    if (!val) {
        // Since we are returning a success code we'd better do
        // something about the out parameters (someone may have
        // given us a non-empty string).
        aResult.Truncate();
        return NS_CONTENT_ATTR_NOT_THERE;
    }

    val->ToString(aResult);

    return aResult.IsEmpty() ? NS_CONTENT_ATTR_NO_VALUE :
                               NS_CONTENT_ATTR_HAS_VALUE;
}

PRBool
nsXULElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
                 "must have a real namespace ID!");

    return mAttrsAndChildren.GetAttr(aName, aNameSpaceID) ||
           FindPrototypeAttribute(aNameSpaceID, aName);
}

nsresult
nsXULElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, PRBool aNotify)
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    nsresult rv;

    // Because It's Hard to maintain a magic ``unset'' value in
    // the local attributes, we'll fault all the attributes,
    // unhook ourselves from the prototype, and then remove the
    // local copy of the attribute that we want to unset. In
    // other words, we'll become ``heavyweight''.
    //
    // We can avoid this if the attribute isn't in the prototype,
    // then we just need to remove it locally

    nsXULPrototypeAttribute *protoattr =
        FindPrototypeAttribute(aNameSpaceID, aName);
    if (protoattr) {
        // We've got an attribute on the prototype, so we need to
        // fully fault and remove the local copy.
        rv = MakeHeavyweight();
        NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_ATTRIBUTE_STATS
        gFaults.UnsetAttr++; gFaults.Total++;
        fprintf(stderr, "XUL: Faulting for UnsetAttr: %d/%d\n",
                gFaults.UnsetAttr, gFaults.Total);
#endif
    }

    PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID);
    if (index < 0) {
        NS_ASSERTION(!protoattr, "we used to have a protoattr, we should now "
                                 "have a normal one");

        return NS_OK;
    }

    nsAutoString oldValue;
    GetAttr(aNameSpaceID, aName, oldValue);

    mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, aNotify);
    if (aNotify && mDocument) {
        mDocument->AttributeWillChange(this, aNameSpaceID, aName);
    }

    PRBool hasMutationListeners =
        HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED);
    nsCOMPtr<nsIDOMAttr> attrNode;
    if (hasMutationListeners) {
        nsAutoString attrName;
        aName->ToString(attrName);
        GetAttributeNode(attrName, getter_AddRefs(attrNode));
    }

    rv = mAttrsAndChildren.RemoveAttrAt(index);
    NS_ENSURE_SUCCESS(rv, rv);

    // Deal with modification of magical attributes that side-effect
    // other things.
    // XXX Know how to remove POPUP event listeners when an attribute is unset?

    if (aNameSpaceID == kNameSpaceID_None) {
        if (aName == nsXULAtoms::hidechrome &&
            NodeInfo()->Equals(nsXULAtoms::window)) {
            HideWindowChrome(PR_FALSE);
        }

        // If the accesskey attribute is removed, unregister it here
        // Also see nsAreaFrame, nsBoxFrame and nsTextBoxFrame's AttributeChanged
        if (aName == nsXULAtoms::accesskey || aName == nsXULAtoms::control) {
            UnregisterAccessKey(oldValue);
        }

        // Check to see if the OBSERVES attribute is being unset.  If so, we
        // need to remove our broadcaster goop completely.
        if (mDocument && (aName == nsXULAtoms::observes ||
                          aName == nsXULAtoms::command)) {
            nsCOMPtr<nsIDOMXULDocument> xuldoc = do_QueryInterface(mDocument);
            if (xuldoc) {
                // Do a getElementById to retrieve the broadcaster
                nsCOMPtr<nsIDOMElement> broadcaster;
                nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(mDocument);
                domDoc->GetElementById(oldValue, getter_AddRefs(broadcaster));
                if (broadcaster) {
                    xuldoc->RemoveBroadcastListenerFor(broadcaster, this,
                                                       NS_LITERAL_STRING("*"));
                }
            }
        }
    }

    if (mDocument) {
        if (hasMutationListeners) {
            nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
            nsMutationEvent mutation(NS_MUTATION_ATTRMODIFIED, node);

            mutation.mRelatedNode = attrNode;
            mutation.mAttrName = aName;

            if (!oldValue.IsEmpty())
              mutation.mPrevAttrValue = do_GetAtom(oldValue);
            mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

            nsEventStatus status = nsEventStatus_eIgnore;
            this->HandleDOMEvent(nsnull, &mutation, nsnull,
                                 NS_EVENT_FLAG_INIT, &status);
        }

        nsCOMPtr<nsIXBLBinding> binding;
        mDocument->GetBindingManager()->GetBinding(this, getter_AddRefs(binding));
        if (binding)
            binding->AttributeChanged(aName, aNameSpaceID, PR_TRUE, aNotify);

        if (aNotify) {
            mDocument->AttributeChanged(this, aNameSpaceID, aName,
                                        nsIDOMMutationEvent::REMOVAL);
        }
    }

    return NS_OK;
}

nsresult
nsXULElement::GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                            nsIAtom** aName, nsIAtom** aPrefix) const
{
#ifdef DEBUG_ATTRIBUTE_STATS
    int proto = mPrototype ? mPrototype->mNumAttributes : 0;
    fprintf(stderr, "GANA: %p[%d] of %d/%d:", (void *)this, aIndex,
            mAttrsAndChildren.AttrCount(), proto);
#endif

    PRUint32 localAttrCount = mAttrsAndChildren.AttrCount();
    if (aIndex < localAttrCount) {
        const nsAttrName* name = mAttrsAndChildren.GetSafeAttrNameAt(aIndex);

        *aNameSpaceID = name->NamespaceID();
        NS_ADDREF(*aName = name->LocalName());
        NS_IF_ADDREF(*aPrefix = name->GetPrefix());
#ifdef DEBUG_ATTRIBUTE_STATS
        fprintf(stderr, " local!\n");
#endif
        return NS_OK;
    }

    aIndex -= localAttrCount;

    if (mPrototype && aIndex < mPrototype->mNumAttributes) {
        // XXX This code looks very wrong. See bug 232639.

        PRBool skip;
        nsXULPrototypeAttribute* attr;
        do {
            attr = &mPrototype->mAttributes[aIndex];
            skip = localAttrCount &&
                   mAttrsAndChildren.GetAttr(attr->mName.LocalName(),
                                             attr->mName.NamespaceID());
#ifdef DEBUG_ATTRIBUTE_STATS
            if (skip)
                fprintf(stderr, " [skip %d/%d]", aIndex, aIndex + localAttrCount);
#endif
        } while (skip && aIndex++ < mPrototype->mNumAttributes);

        if (aIndex <= mPrototype->mNumAttributes) {
#ifdef DEBUG_ATTRIBUTE_STATS
            fprintf(stderr, " proto[%d]!\n", aIndex);
#endif
            *aNameSpaceID = attr->mName.NamespaceID();
            NS_ADDREF(*aName = attr->mName.LocalName());
            NS_IF_ADDREF(*aPrefix = attr->mName.GetPrefix());

            return NS_OK;
        }
        // else, we are out of attrs to return, fall-through
    }

#ifdef DEBUG_ATTRIBUTE_STATS
    fprintf(stderr, " not found\n");
#endif

    *aNameSpaceID = kNameSpaceID_None;
    *aName = nsnull;
    *aPrefix = nsnull;

    return NS_ERROR_ILLEGAL_VALUE;
}

PRUint32
nsXULElement::GetAttrCount() const
{
    PRBool haveLocalAttributes;

    PRUint32 count = mAttrsAndChildren.AttrCount();
    haveLocalAttributes = count > 0;

#ifdef DEBUG_ATTRIBUTE_STATS
    int dups = 0;
#endif

    if (mPrototype) {
        for (PRUint32 i = 0; i < mPrototype->mNumAttributes; i++) {
            nsAttrName* attrName = &mPrototype->mAttributes[i].mName;
            
            if (!haveLocalAttributes ||
                !mAttrsAndChildren.GetAttr(attrName->LocalName(),
                                           attrName->NamespaceID())) {
                ++count;
#ifdef DEBUG_ATTRIBUTE_STATS
            } else {
                dups++;
#endif
            }
        }
    }

#ifdef DEBUG_ATTRIBUTE_STATS
    {
        int local = mAttrsAndChildren.AttrCount();
        int proto = mPrototype ? mPrototype->mNumAttributes : 0;
        nsAutoString tagstr;
        NodeInfo()->GetName(tagstr);
        char *tagcstr = ToNewCString(tagstr);

        fprintf(stderr, "GAC: %p has %d+%d-%d=%d <%s%s>\n", (void *)this,
                local, proto, dups, count, mPrototype ? "" : "*", tagcstr);
        nsMemory::Free(tagcstr);
    }
#endif

    return count;
}


#ifdef DEBUG
static void
rdf_Indent(FILE* out, PRInt32 aIndent)
{
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
}

void
nsXULElement::List(FILE* out, PRInt32 aIndent) const
{
    NS_PRECONDITION(mDocument != nsnull, "bad content");

    PRUint32 i;

    rdf_Indent(out, aIndent);
    fputs("<XUL", out);
    if (mSlots) fputs("*", out);
    fputs(" ", out);

    nsAutoString as;
    NodeInfo()->GetQualifiedName(as);
    fputs(NS_LossyConvertUCS2toASCII(as).get(), out);

    fprintf(out, "@%p", (void *)this);

    PRUint32 nattrs = GetAttrCount();

    for (i = 0; i < nattrs; ++i) {
        nsCOMPtr<nsIAtom> attr;
        nsCOMPtr<nsIAtom> prefix;
        PRInt32 nameSpaceID;
        GetAttrNameAt(i, &nameSpaceID, getter_AddRefs(attr),
                      getter_AddRefs(prefix));

        nsAutoString v;
        GetAttr(nameSpaceID, attr, v);

        fputs(" ", out);

        nsAutoString s;

        if (prefix) {
            prefix->ToString(s);

            fputs(NS_LossyConvertUCS2toASCII(s).get(), out);
            fputs(":", out);
        }

        attr->ToString(s);

        fputs(NS_LossyConvertUCS2toASCII(s).get(), out);
        fputs("=", out);
        fputs(NS_LossyConvertUCS2toASCII(v).get(), out);
    }

    PRUint32 nchildren = GetChildCount();

    if (nchildren) {
        fputs("\n", out);

        for (i = 0; i < nchildren; ++i) {
            GetChildAt(i)->List(out, aIndent + 1);
        }

        rdf_Indent(out, aIndent);
    }
    fputs(">\n", out);

    if (mDocument) {
        nsIBindingManager *bindingManager = mDocument->GetBindingManager();
        if (bindingManager) {
            nsCOMPtr<nsIDOMNodeList> anonymousChildren;
            bindingManager->GetAnonymousNodesFor(NS_STATIC_CAST(nsIContent*, NS_CONST_CAST(nsXULElement*, this)),
                                                 getter_AddRefs(anonymousChildren));

            if (anonymousChildren) {
                PRUint32 length;
                anonymousChildren->GetLength(&length);
                if (length) {
                    rdf_Indent(out, aIndent);
                    fputs("anonymous-children<\n", out);

                    for (PRUint32 i2 = 0; i2 < length; ++i2) {
                        nsCOMPtr<nsIDOMNode> node;
                        anonymousChildren->Item(i2, getter_AddRefs(node));
                        nsCOMPtr<nsIContent> child = do_QueryInterface(node);
                        child->List(out, aIndent + 1);
                    }

                    rdf_Indent(out, aIndent);
                    fputs(">\n", out);
                }
            }

            PRBool hasContentList;
            bindingManager->HasContentListFor(NS_STATIC_CAST(nsIContent*, NS_CONST_CAST(nsXULElement*, this)),
                                              &hasContentList);

            if (hasContentList) {
                nsCOMPtr<nsIDOMNodeList> contentList;
                bindingManager->GetContentListFor(NS_STATIC_CAST(nsIContent*, NS_CONST_CAST(nsXULElement*, this)),
                                                  getter_AddRefs(contentList));

                NS_ASSERTION(contentList != nsnull, "oops, binding manager lied");

                PRUint32 length;
                contentList->GetLength(&length);
                if (length) {
                    rdf_Indent(out, aIndent);
                    fputs("content-list<\n", out);

                    for (PRUint32 i2 = 0; i2 < length; ++i2) {
                        nsCOMPtr<nsIDOMNode> node;
                        contentList->Item(i2, getter_AddRefs(node));
                        nsCOMPtr<nsIContent> child = do_QueryInterface(node);
                        child->List(out, aIndent + 1);
                    }

                    rdf_Indent(out, aIndent);
                    fputs(">\n", out);
                }
            }
        }
    }
}
#endif

nsresult
nsXULElement::HandleDOMEvent(nsPresContext* aPresContext, nsEvent* aEvent,
                             nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
                             nsEventStatus* aEventStatus)
{
    nsresult ret = NS_OK;

    PRBool retarget = PR_FALSE;
    PRBool externalDOMEvent = PR_FALSE;
    nsCOMPtr<nsIDOMEventTarget> oldTarget;

    nsIDOMEvent* domEvent = nsnull;
    if (NS_EVENT_FLAG_INIT & aFlags) {
        if (aEvent->message == NS_XUL_COMMAND && Tag() != nsXULAtoms::command) {
            // See if we have a command elt.  If so, we execute on the command instead
            // of on our content element.
            nsAutoString command;
            GetAttr(kNameSpaceID_None, nsXULAtoms::command, command);
            if (!command.IsEmpty()) {
                nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mDocument));
                nsCOMPtr<nsIDOMElement> commandElt;
                domDoc->GetElementById(command, getter_AddRefs(commandElt));
                nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));
                if (commandContent &&
                    commandContent->IsContentOfType(nsIContent::eXUL) &&
                    commandContent->Tag() == nsXULAtoms::command) {
                    return commandContent->HandleDOMEvent(aPresContext, aEvent, nsnull, NS_EVENT_FLAG_INIT, aEventStatus);
                }
                else {
                    NS_WARNING("A XUL element is attached to a command that doesn't exist!\n");
                    return NS_ERROR_FAILURE;
                }
            }
        }
        if (aDOMEvent) {
            if (*aDOMEvent)
              externalDOMEvent = PR_TRUE;
        }
        else
          aDOMEvent = &domEvent;

        aEvent->flags |= aFlags;
        aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
        aFlags |= NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE;

        if (!externalDOMEvent) {
            // In order for the event to have a proper target for events that don't go through
            // the presshell (onselect, oncommand, oncreate, ondestroy) we need to set our target
            // ourselves. Also, key sets and menus don't have frames and therefore need their
            // targets explicitly specified.
            //
            // We need this for drag&drop as well since the mouse may have moved into a different
            // frame between the initial mouseDown and the generation of the drag gesture.
            // Obviously, the target should be the content/frame where the mouse was depressed,
            // not one computed by the current mouse location.
            nsAutoString tagName;
            NodeInfo()->GetName(tagName); // Local name only
            if (aEvent->message == NS_XUL_COMMAND || aEvent->message == NS_XUL_POPUP_SHOWING ||
                aEvent->message == NS_XUL_POPUP_SHOWN || aEvent->message == NS_XUL_POPUP_HIDING ||
                aEvent->message == NS_XUL_POPUP_HIDDEN || aEvent->message == NS_FORM_SELECTED ||
                aEvent->message == NS_XUL_BROADCAST || aEvent->message == NS_XUL_COMMAND_UPDATE ||
                aEvent->message == NS_XUL_CLICK || aEvent->message == NS_DRAGDROP_GESTURE ||
                tagName.EqualsLiteral("menu") || tagName.EqualsLiteral("menuitem") ||
                tagName.EqualsLiteral("menulist") || tagName.EqualsLiteral("menubar") ||
                tagName.EqualsLiteral("menupopup") || tagName.EqualsLiteral("key") ||
                tagName.EqualsLiteral("keyset")) {

                nsCOMPtr<nsIEventListenerManager> listenerManager;
                if (NS_FAILED(ret = GetListenerManager(getter_AddRefs(listenerManager)))) {
                    NS_ERROR("Unable to instantiate a listener manager on this event.");
                    return ret;
                }
                nsAutoString empty;
                if (NS_FAILED(ret = listenerManager->CreateEvent(aPresContext, aEvent, empty, aDOMEvent))) {
                    NS_ERROR("This event will fail without the ability to create the event early.");
                    return ret;
                }

                // We need to explicitly set the target here, because the
                // DOM implementation will try to compute the target from
                // the frame. If we don't have a frame (e.g., we're a
                // menu), then that breaks.
                nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(domEvent);
                if (privateEvent) {
                  nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
                  privateEvent->SetTarget(target);
                }
                else
                  return NS_ERROR_FAILURE;

                // if we are a XUL click, we have the private event set.
                // now switch to a left mouse click for the duration of the event
                if (aEvent->message == NS_XUL_CLICK)
                  aEvent->message = NS_MOUSE_LEFT_CLICK;
            }
        }
    }
    else if (aEvent->message == NS_IMAGE_LOAD)
      return NS_OK; // Don't let these events bubble or be captured.  Just allow them
                    // on the target image.

    // Find out whether we're anonymous.
    if (IsNativeAnonymous()) {
        retarget = PR_TRUE;
    } else {
        nsIContent* parent = GetParent();
        if (parent) {
            if (*aDOMEvent) {
                (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));
                nsCOMPtr<nsIContent> content(do_QueryInterface(oldTarget));
                if (content && content->GetBindingParent() == parent)
                    retarget = PR_TRUE;
            } else if (GetBindingParent() == parent) {
                retarget = PR_TRUE;
            }
        }
    }

    // determine the parent:
    nsCOMPtr<nsIContent> parent;
    if (mDocument) {
        nsIBindingManager* bindingManager = mDocument->GetBindingManager();
        if (bindingManager) {
            // we have a binding manager -- do we have an anonymous parent?
            bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
        }
    }

    if (!parent) {
        // if we didn't find an anonymous parent, use the explicit one,
        // whether it's null or not...
        parent = GetParent();
    }

    if (retarget || (parent != GetParent())) {
      if (!*aDOMEvent) {
        // We haven't made a DOMEvent yet.  Force making one now.
        nsCOMPtr<nsIEventListenerManager> listenerManager;
        if (NS_FAILED(ret = GetListenerManager(getter_AddRefs(listenerManager)))) {
          return ret;
        }
        nsAutoString empty;
        if (NS_FAILED(ret = listenerManager->CreateEvent(aPresContext, aEvent, empty, aDOMEvent)))
          return ret;
      }

      if (!*aDOMEvent) {
        return NS_ERROR_FAILURE;
      }
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
      if (!privateEvent) {
        return NS_ERROR_FAILURE;
      }

      (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));

      PRBool hasOriginal;
      privateEvent->HasOriginalTarget(&hasOriginal);

      if (!hasOriginal)
        privateEvent->SetOriginalTarget(oldTarget);

      if (retarget) {
          nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetParent());
          privateEvent->SetTarget(target);
      }
    }

    //Capturing stage evaluation
    if (NS_EVENT_FLAG_CAPTURE & aFlags) {
      //Initiate capturing phase.  Special case first call to document
      if (parent) {
        parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags & NS_EVENT_CAPTURE_MASK, aEventStatus);
      }
      else if (mDocument != nsnull) {
          ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                          aFlags & NS_EVENT_CAPTURE_MASK, aEventStatus);
      }
    }


    if (retarget) {
      // The event originated beneath us, and we performed a retargeting.
      // We need to restore the original target of the event.
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
      if (privateEvent)
        privateEvent->SetTarget(oldTarget);
    }

    //Local handling stage
    if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
        aEvent->flags |= aFlags;
        nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, target, aFlags, aEventStatus);
        aEvent->flags &= ~aFlags;
    }

    if (retarget) {
      // The event originated beneath us, and we need to perform a retargeting.
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
      if (privateEvent) {
        nsCOMPtr<nsIDOMEventTarget> parentTarget(do_QueryInterface(GetParent()));
        privateEvent->SetTarget(parentTarget);
      }
    }

    //Bubbling stage
    if (NS_EVENT_FLAG_BUBBLE & aFlags) {
        if (parent != nsnull) {
            // We have a parent. Let them field the event.
            ret = parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                          aFlags & NS_EVENT_BUBBLE_MASK, aEventStatus);
      }
        else if (mDocument != nsnull) {
        // We must be the document root. The event should bubble to the
        // document.
        ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                        aFlags & NS_EVENT_BUBBLE_MASK, aEventStatus);
        }
    }

    if (retarget) {
      // The event originated beneath us, and we performed a retargeting.
      // We need to restore the original target of the event.
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
      if (privateEvent)
        privateEvent->SetTarget(oldTarget);
    }

    if (NS_EVENT_FLAG_INIT & aFlags) {
      // We're leaving the DOM event loop so if we created a DOM event,
      // release here.  If externalDOMEvent is set the event was passed in
      // and we don't own it
      if (*aDOMEvent && !externalDOMEvent) {
        nsrefcnt rc;
        NS_RELEASE2(*aDOMEvent, rc);
        if (0 != rc) {
          // Okay, so someone in the DOM loop (a listener, JS object)
          // still has a ref to the DOM Event but the internal data
          // hasn't been malloc'd.  Force a copy of the data here so the
          // DOM Event is still valid.
          nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
            do_QueryInterface(*aDOMEvent);
          if (privateEvent) {
            privateEvent->DuplicatePrivateData();
          }
        }
        aDOMEvent = nsnull;
      }
    }

    return ret;
}


PRUint32
nsXULElement::ContentID() const
{
    return 0;
}

void
nsXULElement::SetContentID(PRUint32 aID)
{
}

already_AddRefed<nsIURI>
nsXULElement::GetBaseURI() const
{
    // XXX TODO, should share the impl with nsGenericElement
    nsIURI *base;

    if (mDocument) {
        base = mDocument->GetBaseURI();
        NS_IF_ADDREF(base);
    } else {
        base = nsnull;
    }

    return base;
}

nsresult
nsXULElement::RangeAdd(nsIDOMRange* aRange)
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}


void
nsXULElement::RangeRemove(nsIDOMRange* aRange)
{
    // rdf content does not yet support DOM ranges
}


const nsVoidArray *
nsXULElement::GetRangeList() const
{
    // XUL content does not yet support DOM ranges
    return nsnull;
}


void*
nsXULElement::GetProperty(nsIAtom *aPropertyName, nsresult *aStatus) const
{
    nsIDocument *doc = GetDocument();
    if (!doc)
        return nsnull;

    return doc->PropertyTable()->GetProperty(this, aPropertyName, aStatus);
}

nsresult
nsXULElement::SetProperty(nsIAtom            *aPropertyName,
                          void               *aValue,
                          NSPropertyDtorFunc  aDtor)
{
    nsIDocument *doc = GetDocument();
    if (!doc)
        return NS_ERROR_FAILURE;

    nsresult rv = doc->PropertyTable()->SetProperty(this, aPropertyName,
                                                    aValue, aDtor, nsnull);
    if (NS_SUCCEEDED(rv)) {
        EnsureSlots();
        mSlots->mHasProperties = PR_TRUE;
    }

    return rv;
}

nsresult
nsXULElement::DeleteProperty(nsIAtom *aPropertyName)
{
  nsIDocument *doc = GetDocument();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->DeleteProperty(this, aPropertyName);
}

void*
nsXULElement::UnsetProperty(nsIAtom *aPropertyName, nsresult *aStatus)
{
  nsIDocument *doc = GetDocument();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->UnsetProperty(this, aPropertyName, aStatus);
}

// XXX This _should_ be an implementation method, _not_ publicly exposed :-(
NS_IMETHODIMP
nsXULElement::GetResource(nsIRDFResource** aResource)
{
    nsresult rv;

    nsAutoString id;
    rv = GetAttr(kNameSpaceID_None, nsXULAtoms::ref, id);
    if (NS_FAILED(rv)) return rv;

    if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
        rv = GetAttr(kNameSpaceID_None, nsXULAtoms::id, id);
        if (NS_FAILED(rv)) return rv;
    }

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
        rv = gRDFService->GetUnicodeResource(id, aResource);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        *aResource = nsnull;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetDatabase(nsIRDFCompositeDataSource** aDatabase)
{
    nsCOMPtr<nsIXULTemplateBuilder> builder;
    GetBuilder(getter_AddRefs(builder));

    if (builder)
        builder->GetDatabase(aDatabase);
    else
        *aDatabase = nsnull;

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetBuilder(nsIXULTemplateBuilder** aBuilder)
{
    *aBuilder = nsnull;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
    if (xuldoc)
        xuldoc->GetTemplateBuilderFor(this, aBuilder);

    return NS_OK;
}


//----------------------------------------------------------------------
// Implementation methods

nsresult
nsXULElement::EnsureContentsGenerated(void) const
{
    if (mSlots && (mSlots->mLazyState & nsIXULContent::eChildrenMustBeRebuilt)) {
        // Ensure that the element is actually _in_ the document tree;
        // otherwise, somebody is trying to generate children for a node
        // that's not currently in the content model.
        NS_PRECONDITION(mDocument != nsnull, "element not in tree");
        if (!mDocument)
            return NS_ERROR_NOT_INITIALIZED;

        // XXX hack because we can't use "mutable"
        nsXULElement* unconstThis = NS_CONST_CAST(nsXULElement*, this);

        // Clear this value *first*, so we can re-enter the nsIContent
        // getters if needed.
        unconstThis->ClearLazyState(eChildrenMustBeRebuilt);

        // Walk up our ancestor chain, looking for an element with a
        // XUL content model builder attached to it.
        nsIContent* element = unconstThis;

        do {
            nsCOMPtr<nsIDOMXULElement> xulele = do_QueryInterface(element);
            if (xulele) {
                nsCOMPtr<nsIXULTemplateBuilder> builder;
                xulele->GetBuilder(getter_AddRefs(builder));
                if (builder) {
                    if (HasAttr(kNameSpaceID_None, nsXULAtoms::xulcontentsgenerated)) {
                        unconstThis->ClearLazyState(nsIXULContent::eChildrenMustBeRebuilt);
                        return NS_OK;
                    }

                    return builder->CreateContents(unconstThis);
                }
            }

            element = element->GetParent();
        } while (element);

        NS_ERROR("lazy state set with no XUL content builder in ancestor chain");
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}

// nsIStyledContent Implementation
NS_IMETHODIMP
nsXULElement::GetID(nsIAtom** aResult) const
{
    *aResult = nsnull;
    const nsAttrValue* attrVal = FindLocalOrProtoAttr(kNameSpaceID_None, nsXULAtoms::id);

    NS_ASSERTION(!attrVal ||
                 attrVal->Type() == nsAttrValue::eAtom ||
                 (attrVal->Type() == nsAttrValue::eString &&
                  attrVal->GetStringValue().IsEmpty()),
                 "unexpected attribute type");

    if (attrVal && attrVal->Type() == nsAttrValue::eAtom) {
        NS_ADDREF(*aResult = attrVal->GetAtomValue());
    }

    return NS_OK;
}

const nsAttrValue*
nsXULElement::GetClasses() const
{
    return FindLocalOrProtoAttr(kNameSpaceID_None, nsXULAtoms::clazz);
}

NS_IMETHODIMP_(PRBool)
nsXULElement::HasClass(nsIAtom* aClass, PRBool /*aCaseSensitive*/) const
{
    const nsAttrValue* val = FindLocalOrProtoAttr(kNameSpaceID_None, nsXULAtoms::clazz);
    if (val) {
        if (val->Type() == nsAttrValue::eAtom) {
            return aClass == val->GetAtomValue();
        }
        if (val->Type() == nsAttrValue::eAtomArray) {
            return val->GetAtomArrayValue()->IndexOf(aClass) >= 0;
        }
    }

    return PR_FALSE;
}

NS_IMETHODIMP
nsXULElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
    return NS_OK;
}

nsICSSStyleRule*
nsXULElement::GetInlineStyleRule()
{
    // Fetch the cached style rule from the attributes.
    const nsAttrValue* attrVal = FindLocalOrProtoAttr(kNameSpaceID_None, nsXULAtoms::style);

    if (attrVal && attrVal->Type() == nsAttrValue::eCSSStyleRule) {
        return attrVal->GetCSSStyleRuleValue();
    }

    return nsnull;
}

NS_IMETHODIMP
nsXULElement::SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify)
{
    PRBool hasListeners = PR_FALSE;
    PRBool modification = PR_FALSE;
    nsAutoString oldValueStr;

    if (mDocument) {
        hasListeners = nsGenericElement::HasMutationListeners(this,
            NS_EVENT_BITS_MUTATION_ATTRMODIFIED);

        // We can't compare the stringvalues of the old and the new rules
        // since both will point to the same declaration and thus will be
        // the same.
        if (hasListeners || aNotify) {
            modification = !!mAttrsAndChildren.GetAttr(nsXULAtoms::style);
        }
    }

    nsAttrValue attrValue(aStyleRule);

    return SetAttrAndNotify(kNameSpaceID_None, nsXULAtoms::style, nsnull,
                            oldValueStr, attrValue, modification, hasListeners,
                            aNotify);
}

nsChangeHint
nsXULElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                     PRInt32 aModType) const
{
    nsChangeHint retval(NS_STYLE_HINT_NONE);

    if (aAttribute == nsXULAtoms::value &&
        (aModType == nsIDOMMutationEvent::REMOVAL ||
         aModType == nsIDOMMutationEvent::ADDITION)) {
      nsIAtom *tag = Tag();
      if (tag == nsXULAtoms::label || tag == nsXULAtoms::description)
        // Label and description dynamically morph between a normal
        // block and a cropping single-line XUL text frame.  If the
        // value attribute is being added or removed, then we need to
        // return a hint of frame change.  (See bugzilla bug 95475 for
        // details.)
        retval = NS_STYLE_HINT_FRAMECHANGE;
    } else {
        // if left or top changes we reflow. This will happen in xul
        // containers that manage positioned children such as a
        // bulletinboard.
        if (nsXULAtoms::left == aAttribute || nsXULAtoms::top == aAttribute)
            retval = NS_STYLE_HINT_REFLOW;
    }

    return retval;
}

NS_IMETHODIMP_(PRBool)
nsXULElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
    return PR_FALSE;
}

nsIAtom *
nsXULElement::GetIDAttributeName() const
{
    return nsXULAtoms::id;
}

nsIAtom *
nsXULElement::GetClassAttributeName() const
{
    return nsXULAtoms::clazz;
}

// Controllers Methods
NS_IMETHODIMP
nsXULElement::GetControllers(nsIControllers** aResult)
{
    if (! Controllers()) {
        NS_PRECONDITION(mDocument != nsnull, "no document");
        if (! mDocument)
            return NS_ERROR_NOT_INITIALIZED;

        nsresult rv;
        rv = EnsureSlots();
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewXULControllers(nsnull, NS_GET_IID(nsIControllers), getter_AddRefs(mSlots->mControllers));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create a controllers");
        if (NS_FAILED(rv)) return rv;

        // Set the command dispatcher on the new controllers object
        nsCOMPtr<nsIDOMXULDocument> domxuldoc = do_QueryInterface(mDocument);
        NS_ASSERTION(domxuldoc != nsnull, "not an nsIDOMXULDocument");
        if (! domxuldoc)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDOMXULCommandDispatcher> dispatcher;
        rv = domxuldoc->GetCommandDispatcher(getter_AddRefs(dispatcher));
        if (NS_FAILED(rv)) return rv;

        rv = mSlots->mControllers->SetCommandDispatcher(dispatcher);
        if (NS_FAILED(rv)) return rv;
    }

    *aResult = Controllers();
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetBoxObject(nsIBoxObject** aResult)
{
  *aResult = nsnull;

  if (!mDocument)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(mDocument));
  return nsDoc->GetBoxObjectFor(this, aResult);
}

// Methods for setting/getting attributes from nsIDOMXULElement
nsresult
nsXULElement::GetId(nsAString& aId)
{
  GetAttribute(NS_LITERAL_STRING("id"), aId);
  return NS_OK;
}

nsresult
nsXULElement::SetId(const nsAString& aId)
{
  SetAttribute(NS_LITERAL_STRING("id"), aId);
  return NS_OK;
}

nsresult
nsXULElement::GetClassName(nsAString& aClassName)
{
  GetAttribute(NS_LITERAL_STRING("class"), aClassName);
  return NS_OK;
}

nsresult
nsXULElement::SetClassName(const nsAString& aClassName)
{
  SetAttribute(NS_LITERAL_STRING("class"), aClassName);
  return NS_OK;
}

nsresult
nsXULElement::GetAlign(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("align"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetAlign(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("align"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetDir(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("dir"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetDir(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("dir"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetFlex(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("flex"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetFlex(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("flex"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetFlexGroup(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("flexgroup"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetFlexGroup(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("flexgroup"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetOrdinal(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("ordinal"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetOrdinal(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("ordinal"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetOrient(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("orient"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetOrient(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("orient"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetPack(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("pack"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetPack(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("pack"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetHidden(PRBool* aResult)
{
  *aResult = PR_FALSE;
  nsAutoString val;
  GetAttribute(NS_LITERAL_STRING("hidden"), val);
  if (val.EqualsLiteral("true"))
    *aResult = PR_TRUE;
  return NS_OK;
}

nsresult
nsXULElement::SetHidden(PRBool aAttr)
{
  if (aAttr)
    SetAttribute(NS_LITERAL_STRING("hidden"), NS_LITERAL_STRING("true"));
  else
    RemoveAttribute(NS_LITERAL_STRING("hidden"));
  return NS_OK;
}

nsresult
nsXULElement::GetCollapsed(PRBool* aResult)
{
  *aResult = PR_FALSE;
  nsAutoString val;
  GetAttribute(NS_LITERAL_STRING("collapsed"), val);
  if (val.EqualsLiteral("true"))
    *aResult = PR_TRUE;
  return NS_OK;
}

nsresult
nsXULElement::SetCollapsed(PRBool aAttr)
{
 if (aAttr)
    SetAttribute(NS_LITERAL_STRING("collapsed"), NS_LITERAL_STRING("true"));
  else
    RemoveAttribute(NS_LITERAL_STRING("collapsed"));
  return NS_OK;
}

nsresult
nsXULElement::GetAllowEvents(PRBool* aResult)
{
  *aResult = PR_FALSE;
  nsAutoString val;
  GetAttribute(NS_LITERAL_STRING("allowevents"), val);
  if (val.EqualsLiteral("true"))
    *aResult = PR_TRUE;
  return NS_OK;
}

nsresult
nsXULElement::SetAllowEvents(PRBool aAttr)
{
 if (aAttr)
    SetAttribute(NS_LITERAL_STRING("allowevents"), NS_LITERAL_STRING("true"));
  else
    RemoveAttribute(NS_LITERAL_STRING("allowevents"));
  return NS_OK;
}

nsresult
nsXULElement::GetObserves(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("observes"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetObserves(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("observes"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMenu(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("menu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMenu(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("menu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetContextMenu(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("contextmenu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetContextMenu(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("contextmenu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetTooltip(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("tooltip"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetTooltip(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("tooltip"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetWidth(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("width"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetWidth(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("width"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetHeight(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("height"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetHeight(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("height"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMinWidth(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("minwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMinWidth(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("minwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMinHeight(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("minheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMinHeight(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("minheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMaxWidth(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("maxwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMaxWidth(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("maxwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMaxHeight(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("maxheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMaxHeight(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("maxheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetPersist(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("persist"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetPersist(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("persist"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetLeft(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("left"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetLeft(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("left"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetTop(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("top"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetTop(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("top"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetDatasources(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("datasources"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetDatasources(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("datasources"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetRef(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("ref"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetRef(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("ref"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetTooltipText(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("tooltiptext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetTooltipText(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("tooltiptext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetStatusText(nsAString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("statustext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetStatusText(const nsAString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("statustext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
    nsresult rv = EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mSlots->mDOMStyle) {
        if (!gCSSOMFactory) {
            rv = CallGetService(kCSSOMFactoryCID, &gCSSOMFactory);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        rv = gCSSOMFactory->CreateDOMCSSAttributeDeclaration(this,
                getter_AddRefs(mSlots->mDOMStyle));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_IF_ADDREF(*aStyle = mSlots->mDOMStyle);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetParentTree(nsIDOMXULMultiSelectControlElement** aTreeElement)
{
    for (nsIContent* current = GetParent(); current;
         current = current->GetParent()) {
        if (current->GetNodeInfo()->Equals(nsXULAtoms::listbox,
                                           kNameSpaceID_XUL)) {
            CallQueryInterface(current, aTreeElement);
            // XXX returning NS_OK because that's what the code used to do;
            // is that the right thing, though?

            return NS_OK;
        }
    }

    return NS_OK;
}

PRBool
nsXULElement::IsAncestor(nsIDOMNode* aParentNode, nsIDOMNode* aChildNode)
{
    nsCOMPtr<nsIDOMNode> parent = aChildNode;
    while (parent && (parent != aParentNode)) {
        nsCOMPtr<nsIDOMNode> newParent;
        parent->GetParentNode(getter_AddRefs(newParent));
        parent = newParent;
    }

    if (parent)
        return PR_TRUE;
    return PR_FALSE;
}

NS_IMETHODIMP
nsXULElement::Focus()
{
    // What kind of crazy tries to focus an element without a doc?
    if (!mDocument)
        return NS_OK;

    // Obtain a presentation context and then call SetFocus.
    if (mDocument->GetNumberOfShells() == 0)
        return NS_OK;

    nsIPresShell *shell = mDocument->GetShellAt(0);

    // Set focus
    nsCOMPtr<nsPresContext> context = shell->GetPresContext();
    SetFocus(context);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::Blur()
{
    // What kind of crazy tries to blur an element without a doc?
    if (!mDocument)
        return NS_OK;

    // Obtain a presentation context and then call SetFocus.
    if (mDocument->GetNumberOfShells() == 0)
        return NS_OK;

    nsIPresShell *shell = mDocument->GetShellAt(0);

    // Set focus
    nsCOMPtr<nsPresContext> context = shell->GetPresContext();
    RemoveFocus(context);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::Click()
{
    nsAutoString disabled;
    GetAttribute(NS_LITERAL_STRING("disabled"), disabled);
    if (disabled.EqualsLiteral("true"))
        return NS_OK;

    nsCOMPtr<nsIDocument> doc = mDocument; // Strong just in case
    if (doc) {
        PRUint32 numShells = doc->GetNumberOfShells();
        // strong ref to PresContext so events don't destroy it
        nsCOMPtr<nsPresContext> context;

        for (PRUint32 i = 0; i < numShells; ++i) {
            nsIPresShell *shell = doc->GetShellAt(i);
            context = shell->GetPresContext();

            nsMouseEvent eventDown(NS_MOUSE_LEFT_BUTTON_DOWN),
                eventUp(NS_MOUSE_LEFT_BUTTON_UP),
                eventClick(NS_XUL_CLICK);

            // send mouse down
            nsEventStatus status = nsEventStatus_eIgnore;
            HandleDOMEvent(context, &eventDown,  nsnull, NS_EVENT_FLAG_INIT,
                           &status);

            // send mouse up
            status = nsEventStatus_eIgnore;  // reset status
            HandleDOMEvent(context, &eventUp, nsnull, NS_EVENT_FLAG_INIT,
                           &status);

            // send mouse click
            status = nsEventStatus_eIgnore;  // reset status
            HandleDOMEvent(context, &eventClick, nsnull, NS_EVENT_FLAG_INIT,
                           &status);
        }
    }

    // oncommand is fired when an element is clicked...
    return DoCommand();
}

NS_IMETHODIMP
nsXULElement::DoCommand()
{
    nsCOMPtr<nsIDocument> doc = mDocument; // strong just in case
    if (doc) {
        PRUint32 numShells = doc->GetNumberOfShells();
        nsCOMPtr<nsPresContext> context;

        for (PRUint32 i = 0; i < numShells; ++i) {
            nsIPresShell *shell = doc->GetShellAt(i);
            context = shell->GetPresContext();

            nsEventStatus status = nsEventStatus_eIgnore;
            nsMouseEvent event(NS_XUL_COMMAND);
            HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT,
                           &status);
        }
    }

    return NS_OK;
}

// nsIFocusableContent interface and helpers

void
nsXULElement::SetFocus(nsPresContext* aPresContext)
{
    nsAutoString disabled;
    GetAttribute(NS_LITERAL_STRING("disabled"), disabled);
    if (disabled.EqualsLiteral("true"))
        return;

    aPresContext->EventStateManager()->SetContentState(this,
                                                       NS_EVENT_STATE_FOCUS);
}

void
nsXULElement::RemoveFocus(nsPresContext* aPresContext)
{
}

nsIContent *
nsXULElement::GetBindingParent() const
{
    return mBindingParent;
}

nsresult
nsXULElement::SetBindingParent(nsIContent* aParent)
{
    nsresult rv = NS_OK;

    mBindingParent = aParent; // [Weak] no addref
    if (mBindingParent) {
        PRUint32 count = GetChildCount();
        for (PRUint32 i = 0; i < count; i++) {
            rv |= GetChildAt(i)->SetBindingParent(aParent);
        }
    }

    return rv;
}

PRBool
nsXULElement::IsContentOfType(PRUint32 aFlags) const
{
    return !(aFlags & ~(eELEMENT | eXUL));
}

nsresult
nsXULElement::AddPopupListener(nsIAtom* aName)
{
    // Add a popup listener to the element
    nsresult rv;

    nsCOMPtr<nsIXULPopupListener> popupListener =
        do_CreateInstance(kXULPopupListenerCID, &rv);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to create an instance of the popup listener object.");
    if (NS_FAILED(rv)) return rv;

    XULPopupType popupType;
    if (aName == nsXULAtoms::context || aName == nsXULAtoms::contextmenu) {
        popupType = eXULPopupType_context;
    }
    else {
        popupType = eXULPopupType_popup;
    }

    // Add a weak reference to the node.
    popupListener->Init(this, popupType);

    // Add the popup as a listener on this element.
    nsCOMPtr<nsIDOMEventListener> eventListener = do_QueryInterface(popupListener);
    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
    NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);
    target->AddEventListener(NS_LITERAL_STRING("mousedown"), eventListener, PR_FALSE);
    target->AddEventListener(NS_LITERAL_STRING("contextmenu"), eventListener, PR_FALSE);

    return NS_OK;
}

//*****************************************************************************
// nsXULElement::nsIChromeEventHandler
//*****************************************************************************

NS_IMETHODIMP nsXULElement::HandleChromeEvent(nsPresContext* aPresContext,
   nsEvent* aEvent, nsIDOMEvent** aDOMEvent, PRUint32 aFlags,
   nsEventStatus* aEventStatus)
{
  // XXX This is a disgusting hack to prevent the doc from going
  // away until after we've finished handling the event.
  // We will be coming up with a better general solution later.
  nsCOMPtr<nsIDocument> kungFuDeathGrip(mDocument);
  return HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags,aEventStatus);
}

//----------------------------------------------------------------------

nsresult
nsXULElement::EnsureSlots()
{
    if (mSlots)
        return NS_OK;

    mSlots = new Slots;
    if (!mSlots)
        return NS_ERROR_OUT_OF_MEMORY;

    // Copy information from the prototype, if there is one.
    if (!mPrototype)
        return NS_OK;

    NS_ASSERTION(mPrototype->mNodeInfo, "prototype has null nodeinfo!");
    // XXX this is broken, we need to get a new nodeinfo from the
    // document's nodeinfo manager!!!
    mSlots->mNodeInfo        = mPrototype->mNodeInfo;

    return NS_OK;
}

const nsAttrValue*
nsXULElement::FindLocalOrProtoAttr(PRInt32 aNamespaceID, nsIAtom *aName) const
{

    const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNamespaceID);
    if (val) {
        return val;
    }

    nsXULPrototypeAttribute *protoattr =
        FindPrototypeAttribute(aNamespaceID, aName);
    if (protoattr) {
        return &protoattr->mValue;
    }

    return nsnull;
}


nsXULPrototypeAttribute *
nsXULElement::FindPrototypeAttribute(PRInt32 aNamespaceID,
                                     nsIAtom* aLocalName) const
{
    if (!mPrototype) {
        return nsnull;
    }

    PRUint32 i, count = mPrototype->mNumAttributes;
    if (aNamespaceID == kNameSpaceID_None) {
        // Common case so optimize for this
        for (i = 0; i < count; ++i) {
            nsXULPrototypeAttribute *protoattr = &mPrototype->mAttributes[i];
            if (protoattr->mName.Equals(aLocalName)) {
                return protoattr;
            }
        }
    }
    else {
        for (i = 0; i < count; ++i) {
            nsXULPrototypeAttribute *protoattr = &mPrototype->mAttributes[i];
            if (protoattr->mName.Equals(aLocalName, aNamespaceID)) {
                return protoattr;
            }
        }
    }

    return nsnull;
}

nsresult nsXULElement::MakeHeavyweight()
{
    NS_ASSERTION(mPrototype || (mSlots && mSlots->mNodeInfo), "need prototype or nodeinfo");

    if (!mPrototype)
        return NS_OK;           // already heavyweight

    nsresult rv = EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<nsXULPrototypeElement> proto;
    proto.swap(mPrototype);

    PRBool hadAttributes = mAttrsAndChildren.AttrCount() > 0;

    PRUint32 i;
    for (i = 0; i < proto->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &proto->mAttributes[i];

        // We might have a local value for this attribute, in which case
        // we don't want to copy the prototype's value.
        if (hadAttributes &&
            mAttrsAndChildren.GetAttr(protoattr->mName.LocalName(),
                                      protoattr->mName.NamespaceID())) {
            continue;
        }

        // XXX we might wanna have a SetAndTakeAttr that takes an nsAttrName
        nsAttrValue attrValue(protoattr->mValue);
        if (protoattr->mName.IsAtom()) {
            rv = mAttrsAndChildren.SetAndTakeAttr(protoattr->mName.Atom(), attrValue);
        }
        else {
            rv = mAttrsAndChildren.SetAndTakeAttr(protoattr->mName.NodeInfo(),
                                                  attrValue);
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
}

nsresult
nsXULElement::HideWindowChrome(PRBool aShouldHide)
{
    nsIPresShell *shell = mDocument->GetShellAt(0);

    if (shell) {
        nsIContent* content = NS_STATIC_CAST(nsIContent*, this);
        nsIFrame* frame = nsnull;
        shell->GetPrimaryFrameFor(content, &frame);

        nsPresContext *presContext = shell->GetPresContext();

        if (frame && presContext) {
            nsIView* view = frame->GetClosestView();

            if (view) {
                // XXXldb Um, not all views have widgets...
                view->GetWidget()->HideWindowChrome(aShouldHide);
            }
        }
    }

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsXULElement::Slots
//

nsXULElement::Slots::Slots()
{
    mLazyState = mHasProperties = 0;
    MOZ_COUNT_CTOR(nsXULElement::Slots);
}


nsXULElement::Slots::~Slots()
{
    MOZ_COUNT_DTOR(nsXULElement::Slots);

    if (mChildNodes) {
        mChildNodes->DropReference();
    }
}


//----------------------------------------------------------------------
//
// nsXULPrototypeAttribute
//

nsXULPrototypeAttribute::~nsXULPrototypeAttribute()
{
    MOZ_COUNT_DTOR(nsXULPrototypeAttribute);
    if (mEventHandler)
        RemoveJSGCRoot(&mEventHandler);
}


//----------------------------------------------------------------------
//
// nsXULPrototypeElement
//

nsresult
nsXULPrototypeElement::Serialize(nsIObjectOutputStream* aStream,
                                 nsIScriptContext* aContext,
                                 const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    // Write Node Info
    PRInt32 index = aNodeInfos->IndexOf(mNodeInfo);
    NS_ASSERTION(index >= 0, "unknown nsINodeInfo index");
    rv |= aStream->Write32(index);

    // Write Attributes
    rv |= aStream->Write32(mNumAttributes);

    nsAutoString attributeValue;
    PRUint32 i;
    for (i = 0; i < mNumAttributes; ++i) {
        nsCOMPtr<nsINodeInfo> ni;
        if (mAttributes[i].mName.IsAtom()) {
            mNodeInfo->NodeInfoManager()->
                GetNodeInfo(mAttributes[i].mName.Atom(), nsnull,
                            kNameSpaceID_None, getter_AddRefs(ni));
            NS_ASSERTION(ni, "the nodeinfo should already exist");
        }
        else {
            ni = mAttributes[i].mName.NodeInfo();
        }

        index = aNodeInfos->IndexOf(ni);
        NS_ASSERTION(index >= 0, "unknown nsINodeInfo index");
        rv |= aStream->Write32(index);

        mAttributes[i].mValue.ToString(attributeValue);
        rv |= aStream->WriteWStringZ(attributeValue.get());
    }

    // Now write children
    rv |= aStream->Write32(PRUint32(mNumChildren));
    for (i = 0; i < mNumChildren; i++) {
        nsXULPrototypeNode* child = mChildren[i];
        switch (child->mType) {
        case eType_Element:
        case eType_Text:
            rv |= child->Serialize(aStream, aContext, aNodeInfos);
            break;
        case eType_Script:
            rv |= aStream->Write32(child->mType);
            nsXULPrototypeScript* script = NS_STATIC_CAST(nsXULPrototypeScript*, child);

            rv |= aStream->Write8(script->mOutOfLine);
            if (! script->mOutOfLine) {
                rv |= script->Serialize(aStream, aContext, aNodeInfos);
            } else {
                rv |= aStream->WriteCompoundObject(script->mSrcURI,
                                                   NS_GET_IID(nsIURI),
                                                   PR_TRUE);

                if (script->mJSObject) {
                    // This may return NS_OK without muxing script->mSrcURI's
                    // data into the FastLoad file, in the case where that
                    // muxed document is already there (written by a prior
                    // session, or by an earlier FastLoad episode during this
                    // session).
                    rv |= script->SerializeOutOfLine(aStream, aContext);
                }
            }
            break;
        }
    }

    return rv;
}

nsresult
nsXULPrototypeElement::Deserialize(nsIObjectInputStream* aStream,
                                   nsIScriptContext* aContext,
                                   nsIURI* aDocumentURI,
                                   const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    NS_PRECONDITION(aNodeInfos, "missing nodeinfo array");
    nsresult rv;

    // Read Node Info
    PRUint32 number;
    rv = aStream->Read32(&number);
    mNodeInfo = aNodeInfos->SafeObjectAt(number);
    if (!mNodeInfo)
        return NS_ERROR_UNEXPECTED;

    // Read Attributes
    rv |= aStream->Read32(&number);
    mNumAttributes = PRInt32(number);

    PRUint32 i;
    if (mNumAttributes > 0) {
        mAttributes = new nsXULPrototypeAttribute[mNumAttributes];
        if (! mAttributes)
            return NS_ERROR_OUT_OF_MEMORY;

        nsAutoString attributeValue;
        for (i = 0; i < mNumAttributes; ++i) {
            rv |= aStream->Read32(&number);
            nsINodeInfo* ni = aNodeInfos->SafeObjectAt(number);
            if (!ni)
                return NS_ERROR_UNEXPECTED;

            mAttributes[i].mName.SetTo(ni);

            rv |= aStream->ReadString(attributeValue);
            rv |= SetAttrAt(i, attributeValue, aDocumentURI);
        }
    }

    rv |= aStream->Read32(&number);
    mNumChildren = PRInt32(number);

    if (mNumChildren > 0) {
        mChildren = new nsXULPrototypeNode*[mNumChildren];
        if (! mChildren)
            return NS_ERROR_OUT_OF_MEMORY;

        memset(mChildren, 0, sizeof(nsXULPrototypeNode*) * mNumChildren);

        for (i = 0; i < mNumChildren; i++) {
            rv |= aStream->Read32(&number);
            Type childType = (Type)number;

            nsXULPrototypeNode* child = nsnull;

            switch (childType) {
            case eType_Element:
                child = new nsXULPrototypeElement();
                if (! child)
                    return NS_ERROR_OUT_OF_MEMORY;
                child->mType = childType;

                rv |= child->Deserialize(aStream, aContext, aDocumentURI,
                                         aNodeInfos);
                break;
            case eType_Text:
                child = new nsXULPrototypeText();
                if (! child)
                    return NS_ERROR_OUT_OF_MEMORY;
                child->mType = childType;

                rv |= child->Deserialize(aStream, aContext, aDocumentURI,
                                         aNodeInfos);
                break;
            case eType_Script: {
                // language version obtained during deserialization.
                nsXULPrototypeScript* script = new nsXULPrototypeScript(0, nsnull);
                if (! script)
                    return NS_ERROR_OUT_OF_MEMORY;
                child = script;
                child->mType = childType;

                rv |= aStream->Read8(&script->mOutOfLine);
                if (! script->mOutOfLine) {
                    rv |= script->Deserialize(aStream, aContext,
                                              aDocumentURI, aNodeInfos);
                }
                else {
                    rv |= aStream->ReadObject(PR_TRUE, getter_AddRefs(script->mSrcURI));

                    rv |= script->DeserializeOutOfLine(aStream, aContext);
                }
                break;
            }
            }

            mChildren[i] = child;

            // Oh dear. Something failed during the deserialization.
            // We don't know what.  But likely consequences of failed
            // deserializations included calls to |AbortFastLoads| which
            // shuts down the FastLoadService and closes our streams.
            // If that happens, next time through this loop, we die a messy
            // death. So, let's just fail now, and propagate that failure
            // upward so that the ChromeProtocolHandler knows it can't use
            // a cached chrome channel for this.
            if (NS_FAILED(rv))
                return rv;
        }
    }

    return rv;
}

nsresult
nsXULPrototypeElement::SetAttrAt(PRUint32 aPos, const nsAString& aValue,
                                 nsIURI* aDocumentURI)
{
    NS_PRECONDITION(aPos < mNumAttributes, "out-of-bounds");

    // WARNING!!
    // This code is largely duplicated in nsXULElement::SetAttr.
    // Any changes should be made to both functions.

    if (!mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
        mAttributes[aPos].mValue.ParseStringOrAtom(aValue);

        return NS_OK;
    }

    if (mAttributes[aPos].mName.Equals(nsXULAtoms::id) &&
        !aValue.IsEmpty()) {
        // Store id as atom.
        // id="" means that the element has no id. Not that it has
        // emptystring as id.
        mAttributes[aPos].mValue.ParseAtom(aValue);

        return NS_OK;
    }
    else if (mAttributes[aPos].mName.Equals(nsXULAtoms::clazz)) {
        // Compute the element's class list
        mAttributes[aPos].mValue.ParseAtomArray(aValue);
        
        return NS_OK;
    }
    else if (mAttributes[aPos].mName.Equals(nsXULAtoms::style)) {
        // Parse the element's 'style' attribute
        nsCOMPtr<nsICSSStyleRule> rule;
        nsICSSParser* parser = GetCSSParser();
        NS_ENSURE_TRUE(parser, NS_ERROR_OUT_OF_MEMORY);

        // XXX Get correct Base URI (need GetBaseURI on *prototype* element)
        parser->ParseStyleAttribute(aValue, aDocumentURI, aDocumentURI,
                                    getter_AddRefs(rule));
        if (rule) {
            mAttributes[aPos].mValue.SetTo(rule);

            return NS_OK;
        }
        // Don't abort if parsing failed, it could just be malformed css.
    }

    mAttributes[aPos].mValue.ParseStringOrAtom(aValue);

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsXULPrototypeScript
//

nsXULPrototypeScript::nsXULPrototypeScript(PRUint16 aLineNo, const char *aVersion)
    : nsXULPrototypeNode(eType_Script),
      mLineNo(aLineNo),
      mSrcLoading(PR_FALSE),
      mOutOfLine(PR_TRUE),
      mSrcLoadWaiters(nsnull),
      mJSObject(nsnull),
      mLangVersion(aVersion)
{
    NS_LOG_ADDREF(this, 1, ClassName(), ClassSize());
    AddJSGCRoot(&mJSObject, "nsXULPrototypeScript::mJSObject");
}


nsXULPrototypeScript::~nsXULPrototypeScript()
{
    RemoveJSGCRoot(&mJSObject);
}


nsresult
nsXULPrototypeScript::Serialize(nsIObjectOutputStream* aStream,
                                nsIScriptContext* aContext,
                                const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nsnull || !mJSObject,
                 "script source still loading when serializing?!");
    if (!mJSObject)
        return NS_ERROR_FAILURE;

    nsresult rv;

    // Write basic prototype data
    aStream->Write16(mLineNo);

    JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                        aContext->GetNativeContext());
    JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_ENCODE);
    if (! xdr)
        return NS_ERROR_OUT_OF_MEMORY;
    xdr->userdata = (void*) aStream;

    JSScript *script = NS_REINTERPRET_CAST(JSScript*,
                                           ::JS_GetPrivate(cx, mJSObject));
    if (! ::JS_XDRScript(xdr, &script)) {
        rv = NS_ERROR_FAILURE;  // likely to be a principals serialization error
    } else {
        // Get the encoded JSXDRState data and write it.  The JSXDRState owns
        // this buffer memory and will free it beneath ::JS_XDRDestroy.
        //
        // If an XPCOM object needs to be written in the midst of the JS XDR
        // encoding process, the C++ code called back from the JS engine (e.g.,
        // nsEncodeJSPrincipals in caps/src/nsJSPrincipals.cpp) will flush data
        // from the JSXDRState to aStream, then write the object, then return
        // to JS XDR code with xdr reset so new JS data is encoded at the front
        // of the xdr's data buffer.
        //
        // However many XPCOM objects are interleaved with JS XDR data in the
        // stream, when control returns here from ::JS_XDRScript, we'll have
        // one last buffer of data to write to aStream.

        uint32 size;
        const char* data = NS_REINTERPRET_CAST(const char*,
                                               ::JS_XDRMemGetData(xdr, &size));
        NS_ASSERTION(data, "no decoded JSXDRState data!");

        rv = aStream->Write32(size);
        if (NS_SUCCEEDED(rv))
            rv = aStream->WriteBytes(data, size);
    }

    ::JS_XDRDestroy(xdr);
    if (NS_FAILED(rv)) return rv;

    PRUint32 version = PRUint32(mLangVersion
                                ? ::JS_StringToVersion(mLangVersion)
                                : JSVERSION_DEFAULT);
    rv = aStream->Write32(version);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsXULPrototypeScript::SerializeOutOfLine(nsIObjectOutputStream* aStream,
                                         nsIScriptContext* aContext)
{
    nsIXULPrototypeCache* cache = GetXULCache();
#ifdef NS_DEBUG
    PRBool useXULCache = PR_TRUE;
    cache->GetEnabled(&useXULCache);
    NS_ASSERTION(useXULCache,
                 "writing to the FastLoad file, but the XUL cache is off?");
#endif

    nsCOMPtr<nsIFastLoadService> fastLoadService;
    cache->GetFastLoadService(getter_AddRefs(fastLoadService));

    nsresult rv = NS_OK;
    if (!fastLoadService)
        return rv;

    nsCAutoString urispec;
    rv = mSrcURI->GetAsciiSpec(urispec);
    if (NS_FAILED(rv))
        return rv;

    PRBool exists = PR_FALSE;
    fastLoadService->HasMuxedDocument(urispec.get(), &exists);
    if (exists)
        return rv;

    // Allow callers to pass null for aStream, meaning
    // "use the FastLoad service's default output stream."
    // See nsXULDocument.cpp for one use of this.
    nsCOMPtr<nsIObjectOutputStream> objectOutput = aStream;
    if (! objectOutput)
        fastLoadService->GetOutputStream(getter_AddRefs(objectOutput));

    rv = fastLoadService->
         StartMuxedDocument(mSrcURI, urispec.get(),
                            nsIFastLoadService::NS_FASTLOAD_WRITE);
    NS_ASSERTION(rv != NS_ERROR_NOT_AVAILABLE, "reading FastLoad?!");

    nsCOMPtr<nsIURI> oldURI;
    rv |= fastLoadService->SelectMuxedDocument(mSrcURI, getter_AddRefs(oldURI));
    rv |= Serialize(objectOutput, aContext, nsnull);
    rv |= fastLoadService->EndMuxedDocument(mSrcURI);

    if (oldURI) {
        nsCOMPtr<nsIURI> tempURI;
        rv |= fastLoadService->
              SelectMuxedDocument(oldURI, getter_AddRefs(tempURI));
    }

    if (NS_FAILED(rv))
        cache->AbortFastLoads();
    return rv;
}


nsresult
nsXULPrototypeScript::Deserialize(nsIObjectInputStream* aStream,
                                  nsIScriptContext* aContext,
                                  nsIURI* aDocumentURI,
                                  const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    NS_TIMELINE_MARK_FUNCTION("chrome js deserialize");
    nsresult rv;

    // Read basic prototype data
    aStream->Read16(&mLineNo);

    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nsnull || !mJSObject,
                 "prototype script not well-initialized when deserializing?!");

    PRUint32 size;
    rv = aStream->Read32(&size);
    if (NS_FAILED(rv)) return rv;

    char* data;
    rv = aStream->ReadBytes(size, &data);
    if (NS_SUCCEEDED(rv)) {
        JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                            aContext->GetNativeContext());

        JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_DECODE);
        if (! xdr) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
            xdr->userdata = (void*) aStream;
            ::JS_XDRMemSetData(xdr, data, size);

            JSScript *script = nsnull;
            if (! ::JS_XDRScript(xdr, &script)) {
                rv = NS_ERROR_FAILURE;  // principals deserialization error?
            } else {
                mJSObject = ::JS_NewScriptObject(cx, script);
                if (! mJSObject) {
                    rv = NS_ERROR_OUT_OF_MEMORY;    // certain error
                    ::JS_DestroyScript(cx, script);
                }
            }

            // Update data in case ::JS_XDRScript called back into C++ code to
            // read an XPCOM object.
            //
            // In that case, the serialization process must have flushed a run
            // of counted bytes containing JS data at the point where the XPCOM
            // object starts, after which an encoding C++ callback from the JS
            // XDR code must have written the XPCOM object directly into the
            // nsIObjectOutputStream.
            //
            // The deserialization process will XDR-decode counted bytes up to
            // but not including the XPCOM object, then call back into C++ to
            // read the object, then read more counted bytes and hand them off
            // to the JSXDRState, so more JS data can be decoded.
            //
            // This interleaving of JS XDR data and XPCOM object data may occur
            // several times beneath the call to ::JS_XDRScript, above.  At the
            // end of the day, we need to free (via nsMemory) the data owned by
            // the JSXDRState.  So we steal it back, nulling xdr's buffer so it
            // doesn't get passed to ::JS_free by ::JS_XDRDestroy.

            uint32 junk;
            data = (char*) ::JS_XDRMemGetData(xdr, &junk);
            if (data)
                ::JS_XDRMemSetData(xdr, NULL, 0);
            ::JS_XDRDestroy(xdr);
        }

        // If data is null now, it must have been freed while deserializing an
        // XPCOM object (e.g., a principal) beneath ::JS_XDRScript.
        if (data)
            nsMemory::Free(data);
    }
    if (NS_FAILED(rv)) return rv;

    PRUint32 version;
    rv = aStream->Read32(&version);
    if (NS_FAILED(rv)) return rv;

    mLangVersion = ::JS_VersionToString(JSVersion(version));
    return NS_OK;
}


nsresult
nsXULPrototypeScript::DeserializeOutOfLine(nsIObjectInputStream* aInput,
                                           nsIScriptContext* aContext)
{
    // Keep track of FastLoad failure via rv, so we can
    // AbortFastLoads if things look bad.
    nsresult rv = NS_OK;

    nsIXULPrototypeCache* cache = GetXULCache();
    nsCOMPtr<nsIFastLoadService> fastLoadService;
    cache->GetFastLoadService(getter_AddRefs(fastLoadService));

    // Allow callers to pass null for aInput, meaning
    // "use the FastLoad service's default input stream."
    // See nsXULContentSink.cpp for one use of this.
    nsCOMPtr<nsIObjectInputStream> objectInput = aInput;
    if (! objectInput && fastLoadService)
        fastLoadService->GetInputStream(getter_AddRefs(objectInput));

    if (objectInput) {
        PRBool useXULCache = PR_TRUE;
        if (mSrcURI) {
            // NB: we must check the XUL script cache early, to avoid
            // multiple deserialization attempts for a given script, which
            // would exhaust the multiplexed stream containing the singly
            // serialized script.  Note that nsXULDocument::LoadScript
            // checks the XUL script cache too, in order to handle the
            // serialization case.
            //
            // We need do this only for <script src='strres.js'> and the
            // like, i.e., out-of-line scripts that are included by several
            // different XUL documents multiplexed in the FastLoad file.
            cache->GetEnabled(&useXULCache);

            if (useXULCache) {
                cache->GetScript(mSrcURI, NS_REINTERPRET_CAST(void**, &mJSObject));
            }
        }

        if (! mJSObject) {
            nsCOMPtr<nsIURI> oldURI;

            if (mSrcURI) {
                nsCAutoString spec;
                mSrcURI->GetAsciiSpec(spec);
                rv = fastLoadService->StartMuxedDocument(mSrcURI, spec.get(),
                                                         nsIFastLoadService::NS_FASTLOAD_READ);
                if (NS_SUCCEEDED(rv))
                    rv = fastLoadService->SelectMuxedDocument(mSrcURI, getter_AddRefs(oldURI));
            } else {
                // An inline script: check FastLoad multiplexing direction
                // and skip Deserialize if we're not reading from a
                // muxed stream to get inline objects that are contained in
                // the current document.
                PRInt32 direction;
                fastLoadService->GetDirection(&direction);
                if (direction != nsIFastLoadService::NS_FASTLOAD_READ)
                    rv = NS_ERROR_NOT_AVAILABLE;
            }

            // We do reflect errors into rv, but our caller may want to
            // ignore our return value, because mJSObject will be null
            // after any error, and that suffices to cause the script to
            // be reloaded (from the src= URI, if any) and recompiled.
            // We're better off slow-loading than bailing out due to a
            // FastLoad error.
            if (NS_SUCCEEDED(rv))
                rv = Deserialize(objectInput, aContext, nsnull, nsnull);

            if (NS_SUCCEEDED(rv) && mSrcURI) {
                rv = fastLoadService->EndMuxedDocument(mSrcURI);

                if (NS_SUCCEEDED(rv) && oldURI) {
                    nsCOMPtr<nsIURI> tempURI;
                    rv = fastLoadService->SelectMuxedDocument(oldURI, getter_AddRefs(tempURI));

                    NS_ASSERTION(NS_SUCCEEDED(rv) && (!tempURI || tempURI == mSrcURI),
                                 "not currently deserializing into the script we thought we were!");
                }
            }

            if (NS_SUCCEEDED(rv)) {
                if (useXULCache && mSrcURI) {
                    PRBool isChrome = PR_FALSE;
                    mSrcURI->SchemeIs("chrome", &isChrome);
                    if (isChrome) {
                        cache->PutScript(mSrcURI, NS_REINTERPRET_CAST(void*, mJSObject));
                    }
                }
            } else {
                // If mSrcURI is not in the FastLoad multiplex,
                // rv will be NS_ERROR_NOT_AVAILABLE and we'll try to
                // update the FastLoad file to hold a serialization of
                // this script, once it has finished loading.
                if (rv != NS_ERROR_NOT_AVAILABLE)
                    cache->AbortFastLoads();
            }
        }
    }

    return rv;
}

nsresult
nsXULPrototypeScript::Compile(const PRUnichar* aText,
                              PRInt32 aTextLength,
                              nsIURI* aURI,
                              PRUint16 aLineNo,
                              nsIDocument* aDocument,
                              nsIXULPrototypeDocument* aPrototypeDocument)
{
    // We'll compile the script using the prototype document's special
    // script object as the parent. This ensures that we won't end up
    // with an uncollectable reference.
    //
    // Compiling it using (for example) the first document's global
    // object would cause JS to keep a reference via the __proto__ or
    // __parent__ pointer to the first document's global. If that
    // happened, our script object would reference the first document,
    // and the first document would indirectly reference the prototype
    // document because it keeps the prototype cache alive. Circularity!
    nsresult rv;

    // Use the prototype document's special context
    nsIScriptContext *context = nsnull;

    {
        nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner =
            do_QueryInterface(aPrototypeDocument);
        nsIScriptGlobalObject* global = globalOwner->GetScriptGlobalObject();
        NS_ASSERTION(global != nsnull, "prototype doc has no script global");
        if (! global)
            return NS_ERROR_UNEXPECTED;

        context = global->GetContext();

        NS_ASSERTION(context != nsnull, "no context for script global");
        if (! context)
            return NS_ERROR_UNEXPECTED;
    }

    // Use the enclosing document's principal
    // XXX is this right? or should we use the protodoc's?
    nsIPrincipal *principal = aDocument->GetPrincipal();
    if (!principal)
        return NS_ERROR_FAILURE;

    nsCAutoString urlspec;
    aURI->GetSpec(urlspec);

    // Ok, compile it to create a prototype script object!
    rv = context->CompileScript(aText,
                                aTextLength,
                                nsnull,
                                principal,
                                urlspec.get(),
                                aLineNo,
                                mLangVersion,
                                (void**)&mJSObject);

    return rv;
}

//----------------------------------------------------------------------
//
// nsXULPrototypeText
//

nsresult
nsXULPrototypeText::Serialize(nsIObjectOutputStream* aStream,
                              nsIScriptContext* aContext,
                              const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->Write32(mType);

    rv |= aStream->WriteWStringZ(mValue.get());

    return rv;
}

nsresult
nsXULPrototypeText::Deserialize(nsIObjectInputStream* aStream,
                                nsIScriptContext* aContext,
                                nsIURI* aDocumentURI,
                                const nsCOMArray<nsINodeInfo> *aNodeInfos)
{
    nsresult rv;

    // Write basic prototype data
    rv = aStream->ReadString(mValue);

    return rv;
}

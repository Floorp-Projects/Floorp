/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Brendan Eich <brendan@mozilla.org>
 *
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

  1. Make sure to compute class information: GetClasses(), HasClass().

 */

#include "jsapi.h"      // for JS_AddNamedRoot and JS_RemoveRootRT
#include "jsxdrapi.h"
#include "nsCOMPtr.h"
#include "nsDOMCID.h"
#include "nsDOMError.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsHTMLValue.h"
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
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsIHTMLContentContainer.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIStyleContext.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIPresShell.h"
#include "nsIPrincipal.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFContentModelBuilder.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIServiceManager.h"
#include "nsIStyleContext.h"
#include "nsIStyleRule.h"
#include "nsIStyleSheet.h"
#include "nsIStyledContent.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIXULContent.h"
#include "nsIXULDocument.h"
#include "nsIXULPopupListener.h"
#include "nsIXULPrototypeDocument.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIXBLService.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsRDFCID.h"
#include "nsRDFDOMNodeList.h"
#include "nsStyleConsts.h"
#include "nsXPIDLString.h"
#include "nsXULAttributes.h"
#include "nsXULControllers.h"
#include "nsXULTreeElement.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsXULDocument.h"
#include "nsIRuleWalker.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsXULAtoms.h"
#include "nsITreeBoxObject.h"
#include "nsContentUtils.h"
#include "nsGenericElement.h"

#include "nsMutationEvent.h"
#include "nsIDOMMutationEvent.h"
#include "nsPIDOMWindow.h"

#include "prlog.h"
#include "rdf.h"

#include "nsIControllers.h"

// The XUL interfaces implemented by the RDF content node.
#include "nsIDOMXULElement.h"

// The XUL doc interface
#include "nsIDOMXULDocument.h"

#include "nsISizeOfHandler.h"

class nsIWebShell;

// Global object maintenance
nsIXBLService * nsXULElement::gXBLService = nsnull;

// XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
#define XUL_NAMESPACE_URI "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
static const char kXULNameSpaceURI[] = XUL_NAMESPACE_URI;
static const char kRDFNameSpaceURI[] = RDF_NAMESPACE_URI;
// End of XUL interface includes

//----------------------------------------------------------------------

static NS_DEFINE_CID(kEventListenerManagerCID,    NS_EVENTLISTENERMANAGER_CID);
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);

static NS_DEFINE_CID(kXULPopupListenerCID,        NS_XULPOPUPLISTENER_CID);

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,  NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

//----------------------------------------------------------------------

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
    const nsIID* mHandlerIID;
};

static EventHandlerMapEntry kEventHandlerMap[] = {
    { "onclick",         nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "ondblclick",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmousedown",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmouseup",       nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmouseover",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "onmouseout",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },

    { "onmousemove",     nsnull, &NS_GET_IID(nsIDOMMouseMotionListener) },

    { "onkeydown",       nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "onkeyup",         nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "onkeypress",      nsnull, &NS_GET_IID(nsIDOMKeyListener)         },

    { "onload",          nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "onunload",        nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "onabort",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "onerror",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },

    { "onpopupshowing",    nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "onpopupshown",      nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "onpopuphiding" ,    nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "onpopuphidden",     nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "onclose",           nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "oncommand",         nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "onbroadcast",       nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "oncommandupdate",   nsnull, &NS_GET_IID(nsIDOMXULListener)        },

    { "onoverflow",       nsnull, &NS_GET_IID(nsIDOMScrollListener)     },
    { "onunderflow",      nsnull, &NS_GET_IID(nsIDOMScrollListener)     },
    { "onoverflowchanged",nsnull, &NS_GET_IID(nsIDOMScrollListener)     },

    { "onfocus",         nsnull, &NS_GET_IID(nsIDOMFocusListener)       },
    { "onblur",          nsnull, &NS_GET_IID(nsIDOMFocusListener)       },

    { "onsubmit",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "onreset",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "onchange",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "onselect",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "oninput",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },

    { "onpaint",         nsnull, &NS_GET_IID(nsIDOMPaintListener)       },

    { "ondragenter",     nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondragover",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondragexit",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondragdrop",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "ondraggesture",   nsnull, &NS_GET_IID(nsIDOMDragListener)        },

    { "oncontextmenu",   nsnull, &NS_GET_IID(nsIDOMContextMenuListener) },

    { nsnull,            nsnull, nsnull                                 }
};


static nsresult
GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound)
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

    return NS_OK;
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

struct XULBroadcastListener
{
    nsVoidArray* mAttributeList;
    nsIDOMElement* mListener;

    XULBroadcastListener(const nsAReadableString& aAttribute,
                         nsIDOMElement* aListener)
    : mAttributeList(nsnull)
    {
        mListener = aListener; // WEAK REFERENCE
        if (!aAttribute.Equals(NS_LITERAL_STRING("*"))) {
            mAttributeList = new nsVoidArray();
            mAttributeList->AppendElement((void*)(new nsString(aAttribute)));
        }

        // For the "*" case we leave the attribute list nulled out, and this means
        // we're observing all attribute changes.
    }

    ~XULBroadcastListener()
    {
        // Release all the attribute strings.
        if (mAttributeList) {
            PRInt32 count = mAttributeList->Count();
            for (PRInt32 i = 0; i < count; i++) {
                nsString* str = (nsString*)(mAttributeList->ElementAt(i));
                delete str;
            }

            delete mAttributeList;
        }
    }

    PRBool IsEmpty()
    {
        if (ObservingEverything())
            return PR_FALSE;

        PRInt32 count = mAttributeList->Count();
        return (count == 0);
    }

    void RemoveAttribute(const nsAReadableString& aString)
    {
        if (ObservingEverything())
            return;

        if (mAttributeList) {
            PRInt32 count = mAttributeList->Count();
            for (PRInt32 i = 0; i < count; i++) {
                nsString* str = (nsString*)(mAttributeList->ElementAt(i));
                if (str->Equals(aString)) {
                    mAttributeList->RemoveElementAt(i);
                    delete str;
                    break;
                }
            }
        }
    }

    PRBool ObservingEverything()
    {
        return (mAttributeList == nsnull);
    }

    PRBool ObservingAttribute(const nsAReadableString& aString)
    {
        if (ObservingEverything())
            return PR_TRUE;

        if (mAttributeList) {
            PRInt32 count = mAttributeList->Count();
            for (PRInt32 i = 0; i < count; i++) {
                nsString* str = (nsString*)(mAttributeList->ElementAt(i));
                if (str->Equals(aString))
                    return PR_TRUE;
            }
        }

        return PR_FALSE;
    }
};

//----------------------------------------------------------------------

static PRBool HasMutationListeners(nsIContent* aContent, PRUint32 aType)
{
  nsCOMPtr<nsIDocument> doc;
  aContent->GetDocument(*getter_AddRefs(doc));
  if (!doc)
    return PR_FALSE;

  nsCOMPtr<nsIScriptGlobalObject> global;
  doc->GetScriptGlobalObject(getter_AddRefs(global));
  if (!global)
    return PR_FALSE;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(global));
  if (!window)
    return PR_FALSE;

  PRBool set;
  window->HasMutationListeners(aType, &set);
  if (!set)
    return PR_FALSE;

  // We know a mutation listener is registered, but it might not
  // be in our chain.  Check quickly to see.
  nsCOMPtr<nsIContent> curr = aContent;
  nsCOMPtr<nsIEventListenerManager> manager;

  while (curr) {
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

    nsCOMPtr<nsIContent> prev = curr;
    prev->GetParent(*getter_AddRefs(curr));
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
nsINameSpaceManager* nsXULElement::gNameSpaceManager;
PRInt32              nsXULElement::kNameSpaceID_RDF;
PRInt32              nsXULElement::kNameSpaceID_XUL;

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
// nsXULNode
//  XXXbe temporary, make 'em pure when all subclasses implement

nsresult
nsXULPrototypeNode::Serialize(nsIObjectOutputStream* aStream,
                              nsIScriptContext* aContext)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsXULPrototypeNode::Deserialize(nsIObjectInputStream* aStream,
                                nsIScriptContext* aContext)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
// nsXULElement


nsXULElement::nsXULElement()
    : mPrototype(nsnull),
      mDocument(nsnull),
      mParent(nsnull),
#ifdef DEBUG
      mIsScriptObjectRooted(PR_FALSE),
#endif
      mContentId(0),
      mLazyState(0),
      mBindingParent(nsnull),
      mSlots(nsnull)
{
    NS_INIT_REFCNT();
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumElements);
}


nsresult
nsXULElement::Init()
{
    if (gRefCnt++ == 0) {
        nsresult rv;

        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDFService);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
        if (NS_FAILED(rv)) return rv;


        rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsINameSpaceManager),
                                                (void**) &gNameSpaceManager);

        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
        if (NS_FAILED(rv)) return rv;

        if (gNameSpaceManager) {
            gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kRDFNameSpaceURI), kNameSpaceID_RDF);
            gNameSpaceManager->RegisterNameSpace(NS_ConvertASCIItoUCS2(kXULNameSpaceURI), kNameSpaceID_XUL);
        }

        InitEventHandlerMap();
    }

    return NS_OK;
}

nsXULElement::~nsXULElement()
{
    // At this point, we'd better *not* be rooted. If we are, then we
    // might be running into a weird situation (like bug 71141) where
    // we created two JS objects for a single DOM element.
    NS_ASSERTION(! mIsScriptObjectRooted, "nsXULElement still rooted in its dtor");

    delete mSlots;

    //NS_IF_RELEASE(mDocument); // not refcounted
    //NS_IF_RELEASE(mParent)    // not refcounted

    // Force child's parent to be null. This ensures that we don't
    // have dangling pointers if a child gets leaked.
    for (PRInt32 i = mChildren.Count() - 1; i >= 0; --i) {
        nsIContent* child = NS_STATIC_CAST(nsIContent*, mChildren[i]);
        child->SetParent(nsnull);
        NS_RELEASE(child);
    }

    // Clean up shared statics
    if (--gRefCnt == 0) {
        FinishEventHandlerMap();

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }

        NS_IF_RELEASE(gNameSpaceManager);
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

    NS_PRECONDITION(aDocument != nsnull, "null ptr");
    if (! aDocument)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsXULElement* element = new nsXULElement();
    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    // anchor the element so an early return will clean up properly.
    nsCOMPtr<nsIContent> anchor =
        do_QueryInterface(NS_REINTERPRET_CAST(nsIStyledContent*, element));

    nsresult rv;
    rv = element->Init();
    if (NS_FAILED(rv)) return rv;

    element->mPrototype = aPrototype;
    element->mDocument = aDocument;

    if (aIsScriptable) {
        // Check each attribute on the prototype to see if we need to do
        // any additional processing and hookup that would otherwise be
        // done 'automagically' by SetAttribute().
        for (PRInt32 i = 0; i < aPrototype->mNumAttributes; ++i) {
            nsXULPrototypeAttribute* attr = &(aPrototype->mAttributes[i]);

            if (attr->mNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
                // Check for an event handler
                nsIID iid;
                PRBool found;
                nsCOMPtr<nsIAtom> name;
                attr->mNodeInfo->GetNameAtom(*getter_AddRefs(name));
                rv = GetEventHandlerIID(name, &iid, &found);
                if (NS_FAILED(rv)) return rv;

                if (found) {
                    nsAutoString   valueStr;
                    attr->mValue.GetValue( valueStr );
                    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumEventHandlers);
                    rv = element->AddScriptEventListener(name, valueStr);
                    if (NS_FAILED(rv)) return rv;
                }

                // Check for popup attributes
                if (attr->mNodeInfo->Equals(nsXULAtoms::popup) || // XXXdwh deprecated. use menu.
                    attr->mNodeInfo->Equals(nsXULAtoms::tooltip) ||
                    attr->mNodeInfo->Equals(nsXULAtoms::menu) ||
                    attr->mNodeInfo->Equals(nsXULAtoms::contextmenu) ||
                    attr->mNodeInfo->Equals(nsXULAtoms::context)) // XXXdwh deprecated. use contextmenu.
                {
                    rv = element->AddPopupListener(name);
                    if (NS_FAILED(rv)) return rv;
                }
            }
        }
    }

    *aResult = NS_REINTERPRET_CAST(nsIStyledContent*, element);
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
nsXULElement::Create(nsINodeInfo *aNodeInfo, nsIContent** aResult)
{
    // Create an nsXULElement with the specified namespace and tag.
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsXULElement* element = new nsXULElement();
    if (! element)
        return NS_ERROR_OUT_OF_MEMORY;

    // anchor the element so an early return will clean up properly.
    nsCOMPtr<nsIContent> anchor =
        do_QueryInterface(NS_REINTERPRET_CAST(nsIStyledContent*, element));

    nsresult rv;
    rv = element->Init();
    if (NS_FAILED(rv)) return rv;

    rv = element->EnsureSlots();
    if (NS_FAILED(rv)) return rv;

    element->mSlots->mNodeInfo    = aNodeInfo;

    *aResult = NS_REINTERPRET_CAST(nsIStyledContent*, element);
    NS_ADDREF(*aResult);
    return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports interface

NS_IMPL_ADDREF(nsXULElement);
NS_IMPL_RELEASE(nsXULElement);

NS_IMETHODIMP
nsXULElement::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;
    *result = nsnull;

    nsresult rv;

    if (iid.Equals(NS_GET_IID(nsIStyledContent)) ||
        iid.Equals(NS_GET_IID(nsIContent)) ||
        iid.Equals(NS_GET_IID(nsISupports))) {
        *result = NS_STATIC_CAST(nsIStyledContent*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIXMLContent))) {
        *result = NS_STATIC_CAST(nsIXMLContent*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIXULContent))) {
        *result = NS_STATIC_CAST(nsIXULContent*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMXULElement)) ||
             iid.Equals(NS_GET_IID(nsIDOMElement)) ||
             iid.Equals(NS_GET_IID(nsIDOMNode))) {
        *result = NS_STATIC_CAST(nsIDOMXULElement*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIScriptEventHandlerOwner))) {
        *result = NS_STATIC_CAST(nsIScriptEventHandlerOwner*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMEventReceiver))) {
        *result = NS_STATIC_CAST(nsIDOMEventReceiver*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMEventTarget))) {
        *result = NS_STATIC_CAST(nsIDOMEventTarget*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIChromeEventHandler))) {
        *result = NS_STATIC_CAST(nsIChromeEventHandler*, this);
    }
    else if ((iid.Equals(NS_GET_IID(nsIDOMXULTreeElement)) ||
              iid.Equals(NS_GET_IID(nsIXULTreeContent))) &&
             (NodeInfo()->NamespaceEquals(kNameSpaceID_XUL))){
      nsCOMPtr<nsIAtom> tag;
      PRInt32 dummy;
      nsIXBLService *xblService = GetXBLService();
      xblService->ResolveTag(NS_STATIC_CAST(nsIStyledContent*, this), &dummy, getter_AddRefs(tag));
      if (tag.get() == nsXULAtoms::tree) {
        // We delegate XULTreeElement APIs to an aggregate object
        if (! InnerXULElement()) {
            rv = EnsureSlots();
            if (NS_FAILED(rv)) return rv;

            if ((mSlots->mInnerXULElement = new nsXULTreeElement(this)) == nsnull)
                return NS_ERROR_OUT_OF_MEMORY;
        }

        return InnerXULElement()->QueryInterface(iid, result);
      }
      else
        return NS_NOINTERFACE;
    }
    else if (iid.Equals(NS_GET_IID(nsIDOM3Node))) {
        *result = new nsNode3Tearoff(this);
        NS_ENSURE_TRUE(*result, NS_ERROR_OUT_OF_MEMORY);
    }
    else if (iid.Equals(NS_GET_IID(nsIClassInfo))) {
        nsISupports *inst = nsnull;

        nsCOMPtr<nsIAtom> tag;
        PRInt32 dummy;
        nsIXBLService *xblService = GetXBLService();
        xblService->ResolveTag(NS_STATIC_CAST(nsIStyledContent*, this), &dummy, getter_AddRefs(tag));
        if (tag.get() == nsXULAtoms::tree) {
            inst = nsContentUtils::
                GetClassInfoInstance(eDOMClassInfo_XULTreeElement_id);
        } else {
            inst = nsContentUtils::
                GetClassInfoInstance(eDOMClassInfo_XULElement_id);
        }

        NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);

        *result = inst;

        return NS_OK;
    }
    else if (mDocument) {
        nsCOMPtr<nsIBindingManager> manager;
        mDocument->GetBindingManager(getter_AddRefs(manager));
        return manager->GetBindingImplementation(this, iid, result);
    } else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }

    // if we get here, we know one of the above IIDs was ok.
    NS_ADDREF(this);
    return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode interface

NS_IMETHODIMP
nsXULElement::GetNodeName(nsAWritableString& aNodeName)
{
    return NodeInfo()->GetQualifiedName(aNodeName);
}


NS_IMETHODIMP
nsXULElement::GetNodeValue(nsAWritableString& aNodeValue)
{
    aNodeValue.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetNodeValue(const nsAReadableString& aNodeValue)
{
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
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
    if (mParent) {
        return mParent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aParentNode);
    }
    else if (mDocument) {
        // XXX This is a mess because of our fun multiple inheritance heirarchy
        nsCOMPtr<nsIContent> root;
        mDocument->GetRootContent(getter_AddRefs(root));
        nsCOMPtr<nsIContent> thisIContent;
        QueryInterface(NS_GET_IID(nsIContent), getter_AddRefs(thisIContent));

        if (root == thisIContent) {
            // If we don't have a parent, and we're the root content
            // of the document, DOM says that our parent is the
            // document.
            return mDocument->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aParentNode);
        }
    }

    // A standalone element (i.e. one without a parent or a document)
    *aParentNode = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
    nsresult rv;

    nsRDFDOMNodeList* children;
    rv = nsRDFDOMNodeList::Create(&children);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
    if (NS_FAILED(rv)) return rv;

    PRInt32 count;
    rv = ChildCount(count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child count");
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < count; ++i) {
        nsCOMPtr<nsIContent> child;
        rv = ChildAt(i, *getter_AddRefs(child));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child");
        if (NS_FAILED(rv))
            break;

        nsCOMPtr<nsIDOMNode> domNode;
        rv = child->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) getter_AddRefs(domNode));
        if (NS_FAILED(rv)) {
            NS_WARNING("child content doesn't support nsIDOMNode");
            continue;
        }

        rv = children->AppendNode(domNode);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to append node to list");
        if (NS_FAILED(rv))
            break;
    }

    // Create() addref'd for us
    *aChildNodes = children;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetFirstChild(nsIDOMNode** aFirstChild)
{
    nsresult rv;
    nsCOMPtr<nsIContent> child;
    rv = ChildAt(0, *getter_AddRefs(child));

    if (NS_SUCCEEDED(rv) && (child != nsnull)) {
        rv = child->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aFirstChild);
        NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM node");
        return rv;
    }

    *aFirstChild = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetLastChild(nsIDOMNode** aLastChild)
{
    nsresult rv;
    PRInt32 count;
    rv = ChildCount(count);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get child count");

    if (NS_SUCCEEDED(rv) && (count != 0)) {
        nsCOMPtr<nsIContent> child;
        rv = ChildAt(count - 1, *getter_AddRefs(child));

        NS_ASSERTION(child != nsnull, "no child");

        if (child) {
            rv = child->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aLastChild);
            NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM node");
            return rv;
        }
    }

    *aLastChild = nsnull;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
    if (nsnull != mParent) {
        PRInt32 pos;
        mParent->IndexOf(NS_STATIC_CAST(nsIStyledContent*, this), pos);
        if (pos > -1) {
            nsCOMPtr<nsIContent> prev;
            mParent->ChildAt(--pos, *getter_AddRefs(prev));
            if (prev) {
                nsresult rv = prev->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aPreviousSibling);
                NS_ASSERTION(NS_SUCCEEDED(rv), "not a DOM node");
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
    if (nsnull != mParent) {
        PRInt32 pos;
        mParent->IndexOf(NS_STATIC_CAST(nsIStyledContent*, this), pos);
        if (pos > -1) {
            nsCOMPtr<nsIContent> next;
            mParent->ChildAt(++pos, *getter_AddRefs(next));
            if (next) {
                nsresult res = next->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aNextSibling);
                NS_ASSERTION(NS_OK == res, "not a DOM Node");
                return res;
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
    nsresult rv;
    if (! Attributes()) {
        rv = EnsureSlots();
        if (NS_FAILED(rv)) return rv;

        if (! Attributes()) {
            rv = nsXULAttributes::Create(NS_STATIC_CAST(nsIStyledContent*, this), &(mSlots->mAttributes));
            if (NS_FAILED(rv)) return rv;
        }
    }

    *aAttributes = Attributes();
    NS_ADDREF(*aAttributes);
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
    if (mDocument) {
        return mDocument->QueryInterface(NS_GET_IID(nsIDOMDocument), (void**) aOwnerDocument);
    }
    else {
        *aOwnerDocument = nsnull;
        return NS_OK;
    }
}


NS_IMETHODIMP
nsXULElement::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{
    return NodeInfo()->GetNamespaceURI(aNamespaceURI);
}


NS_IMETHODIMP
nsXULElement::GetPrefix(nsAWritableString& aPrefix)
{
    return NodeInfo()->GetPrefix(aPrefix);
}


NS_IMETHODIMP
nsXULElement::SetPrefix(const nsAReadableString& aPrefix)
{
    // XXX: Validate the prefix string!

    nsINodeInfo *newNodeInfo = nsnull;
    nsCOMPtr<nsIAtom> prefix;

    if (aPrefix.Length() && !DOMStringIsNull(aPrefix)) {
        prefix = dont_AddRef(NS_NewAtom(aPrefix));
        NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
    }

    nsresult rv = EnsureSlots();
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSlots->mNodeInfo->PrefixChanged(prefix, newNodeInfo);
    NS_ENSURE_SUCCESS(rv, rv);

    mSlots->mNodeInfo = newNodeInfo;

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetLocalName(nsAWritableString& aLocalName)
{
    return NodeInfo()->GetLocalName(aLocalName);
}


NS_IMETHODIMP
nsXULElement::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                           nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aNewChild != nsnull, "null ptr");
    if (! aNewChild)
        return NS_ERROR_NULL_POINTER;

    // aRefChild may be null; that means "append".

    nsresult rv;

    nsCOMPtr<nsIContent> newcontent = do_QueryInterface(aNewChild);
    NS_ASSERTION(newcontent != nsnull, "not an nsIContent");
    if (! newcontent)
        return NS_ERROR_UNEXPECTED;

    // First, check to see if the content was already parented
    // somewhere. If so, remove it.
    nsCOMPtr<nsIContent> oldparent;
    rv = newcontent->GetParent(*getter_AddRefs(oldparent));
    if (NS_FAILED(rv)) return rv;

    if (oldparent) {
        PRInt32 oldindex;
        rv = oldparent->IndexOf(newcontent, oldindex);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to determine index of aNewChild in old parent");
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(oldindex >= 0, "old parent didn't think aNewChild was a child");

        if (oldindex >= 0) {
            rv = oldparent->RemoveChildAt(oldindex, PR_TRUE);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Now, insert the element into the content model under 'this'
    if (aRefChild) {
        nsCOMPtr<nsIContent> refcontent = do_QueryInterface(aRefChild);
        NS_ASSERTION(refcontent != nsnull, "not an nsIContent");
        if (! refcontent)
            return NS_ERROR_UNEXPECTED;

        PRInt32 pos;
        rv = IndexOf(refcontent, pos);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to determine index of aRefChild");
        if (NS_FAILED(rv)) return rv;

        if (pos >= 0) {
            // Some frames assume that the document will have been set,
            // so pass in PR_TRUE for aDeepSetDocument here.
            rv = InsertChildAt(newcontent, pos, PR_TRUE, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to insert aNewChild");
            if (NS_FAILED(rv)) return rv;
        }

        // XXX Hmm. There's a case here that we handle ambiguously, I
        // think. If aRefChild _isn't_ actually one of our kids, then
        // pos == -1, and we'll never insert the new kid. Should we
        // just append it?
    }
    else {
        // Some frames assume that the document will have been set,
        // so pass in PR_TRUE for aDeepSetDocument here.
        rv = AppendChildTo(newcontent, PR_TRUE, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to append a aNewChild");
        if (NS_FAILED(rv)) return rv;
    }

    NS_ADDREF(aNewChild);
    *aReturn = aNewChild;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                           nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aNewChild != nsnull, "null ptr");
    if (! aNewChild)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aOldChild != nsnull, "null ptr");
    if (! aOldChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIContent> oldelement = do_QueryInterface(aOldChild);
    NS_ASSERTION(oldelement != nsnull, "not an nsIContent");

    if (oldelement) {
        PRInt32 pos;
        rv = IndexOf(oldelement, pos);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to determine index of aOldChild");

        if (NS_SUCCEEDED(rv) && (pos >= 0)) {
            nsCOMPtr<nsIContent> newelement = do_QueryInterface(aNewChild);
            NS_ASSERTION(newelement != nsnull, "not an nsIContent");

            if (newelement) {
                // Some frames assume that the document will have been set,
                // so pass in PR_TRUE for aDeepSetDocument here.
                rv = ReplaceChildAt(newelement, pos, PR_TRUE, PR_TRUE);
                NS_ASSERTION(NS_SUCCEEDED(rv), "unable to replace old child");
            }
        }
    }

    NS_ADDREF(aNewChild);
    *aReturn = aNewChild;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
    NS_PRECONDITION(aOldChild != nsnull, "null ptr");
    if (! aOldChild)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIContent> element = do_QueryInterface(aOldChild);
    NS_ASSERTION(element != nsnull, "not an nsIContent");

    if (element) {
        PRInt32 pos;
        rv = IndexOf(element, pos);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to determine index of aOldChild");

        if (NS_SUCCEEDED(rv) && (pos >= 0)) {
            rv = RemoveChildAt(pos, PR_TRUE);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove old child");
        }
    }

    NS_ADDREF(aOldChild);
    *aReturn = aOldChild;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
    return InsertBefore(aNewChild, nsnull, aReturn);
}


NS_IMETHODIMP
nsXULElement::HasChildNodes(PRBool* aReturn)
{
    nsresult rv;
    PRInt32 count;
    if (NS_FAILED(rv = ChildCount(count))) {
        NS_ERROR("unable to count kids");
        return rv;
    }
    *aReturn = (count > 0);
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::HasAttributes(PRBool* aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    PRInt32 attrCount = 0;

    GetAttrCount(attrCount);

    *aReturn = (attrCount > 0);

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
    nsresult rv;

    nsCOMPtr<nsIContent> result;

    if (mPrototype) {
        // We haven't "faulted" and become a heavyweight node yet, so
        // we can go ahead and just make another lightweight from our
        // prototype.
        rv = nsXULElement::Create(mPrototype, mDocument, PR_TRUE, getter_AddRefs(result));
        if (NS_FAILED(rv)) return rv;
    }
    else if (mSlots) {
        // We've faulted: create another heavyweight, and then copy
        // stuff by hand.
        rv = nsXULElement::Create(mSlots->mNodeInfo, getter_AddRefs(result));
        result->SetDocument(mDocument, PR_TRUE, PR_TRUE);

        if (NS_FAILED(rv)) return rv;

        // Copy namespace stuff.
        nsCOMPtr<nsIXMLContent> xmlcontent = do_QueryInterface(result);
        if (xmlcontent) {
            rv = xmlcontent->SetContainingNameSpace(mSlots->mNameSpace);
            if (NS_FAILED(rv)) return rv;
        }

        // Copy attributes, if there are any.
        if (mSlots->mAttributes) {
            PRInt32 count = mSlots->mAttributes->Count();
            for (PRInt32 i = 0; i < count; ++i) {
                nsXULAttribute* attr = mSlots->mAttributes->ElementAt(i);
                NS_ASSERTION(attr != nsnull, "null ptr");
                if (! attr)
                    return NS_ERROR_UNEXPECTED;

                nsAutoString value;
                rv = attr->GetValue(value);
                if (NS_FAILED(rv)) return rv;

                rv = result->SetAttr(attr->GetNodeInfo(), value,
                                     PR_FALSE);
                if (NS_FAILED(rv)) return rv;
            }
        }

        // XXX TODO: set up RDF generic builder n' stuff if there is a
        // 'datasources' attribute? This is really kind of tricky,
        // because then we'd need to -selectively- copy children that
        // -weren't- generated from RDF. Ugh. Forget it.
    }
    else {
        NS_ERROR("ack! no prototype and no slots!");
        return NS_ERROR_UNEXPECTED;
    }

    if (aDeep) {
        // Copy cloned children!
        PRInt32 count = mChildren.Count();
        for (PRInt32 i = 0; i < count; ++i) {
            nsIContent* child = NS_STATIC_CAST(nsIContent*, mChildren[i]);

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
nsXULElement::IsSupported(const nsAReadableString& aFeature,
                          const nsAReadableString& aVersion,
                          PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}


//----------------------------------------------------------------------
// nsIDOMElement interface

NS_IMETHODIMP
nsXULElement::GetTagName(nsAWritableString& aTagName)
{
    return NodeInfo()->GetQualifiedName(aTagName);
}

NS_IMETHODIMP
nsXULElement::GetNodeInfo(nsINodeInfo*& aResult) const
{
    aResult = NodeInfo();
    NS_IF_ADDREF(aResult);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetAttribute(const nsAReadableString& aName,
                           nsAWritableString& aReturn)
{
    nsresult rv;
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> nameAtom;
    nsCOMPtr<nsINodeInfo> nodeInfo;

    if (NS_FAILED(rv = NormalizeAttrString(aName,
                                           *getter_AddRefs(nodeInfo)))) {
        NS_WARNING("unable to normalize attribute name");
        return rv;
    }

    nodeInfo->GetNameAtom(*getter_AddRefs(nameAtom));
    nodeInfo->GetNamespaceID(nameSpaceID);

    GetAttr(nameSpaceID, nameAtom, aReturn);
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::SetAttribute(const nsAReadableString& aName,
                           const nsAReadableString& aValue)
{
    nsresult rv;

    nsCOMPtr<nsINodeInfo> ni;

    rv = NormalizeAttrString(aName, *getter_AddRefs(ni));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to normalize attribute name");

    if (NS_SUCCEEDED(rv)) {
        rv = SetAttr(ni, aValue, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to set attribute");
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::RemoveAttribute(const nsAReadableString& aName)
{
    nsresult rv;

    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> tag;
    nsCOMPtr<nsINodeInfo> ni;

    rv = NormalizeAttrString(aName, *getter_AddRefs(ni));
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to parse attribute name");

    if (NS_SUCCEEDED(rv)) {
        ni->GetNameAtom(*getter_AddRefs(tag));
        ni->GetNamespaceID(nameSpaceID);

        rv = UnsetAttr(nameSpaceID, tag, PR_TRUE);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove attribute");
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetAttributeNode(const nsAReadableString& aName,
                               nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aReturn != nsnull, "null ptr");
    if (! aReturn)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    nsCOMPtr<nsIDOMNamedNodeMap> map;
    rv = GetAttributes(getter_AddRefs(map));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMNode> node;
    rv = map->GetNamedItem(aName, getter_AddRefs(node));
    if (NS_FAILED(rv)) return rv;

    if (node) {
        rv = node->QueryInterface(NS_GET_IID(nsIDOMAttr), (void**) aReturn);
    }
    else {
        *aReturn = nsnull;
        rv = NS_OK;
    }

    return rv;
}


NS_IMETHODIMP
nsXULElement::SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aNewAttr != nsnull, "null ptr");
    if (! aNewAttr)
        return NS_ERROR_NULL_POINTER;

    NS_NOTYETIMPLEMENTED("write me");

    NS_ADDREF(aNewAttr);
    *aReturn = aNewAttr;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn)
{
    NS_PRECONDITION(aOldAttr != nsnull, "null ptr");
    if (! aOldAttr)
        return NS_ERROR_NULL_POINTER;

    NS_NOTYETIMPLEMENTED("write me");

    NS_ADDREF(aOldAttr);
    *aReturn = aOldAttr;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetElementsByTagName(const nsAReadableString& aName,
                                   nsIDOMNodeList** aReturn)
{
    nsresult rv;

    nsRDFDOMNodeList* elements;
    rv = nsRDFDOMNodeList::Create(&elements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create node list");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMNode> domElement;
    rv = QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(domElement));
    if (NS_SUCCEEDED(rv)) {
        GetElementsByTagName(domElement, aName, elements);
    }

    // transfer ownership to caller
    *aReturn = elements;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetAttributeNS(const nsAReadableString& aNamespaceURI,
                             const nsAReadableString& aLocalName,
                             nsAWritableString& aReturn)
{
    nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aLocalName)));
    PRInt32 nsid;

    gNameSpaceManager->GetNameSpaceID(aNamespaceURI, nsid);

    if (nsid == kNameSpaceID_Unknown) {
        // Unkonwn namespace means no attr...

        aReturn.Truncate();
        return NS_OK;
    }

    GetAttr(nsid, name, aReturn);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetAttributeNS(const nsAReadableString& aNamespaceURI,
                             const nsAReadableString& aQualifiedName,
                             const nsAReadableString& aValue)
{
    nsCOMPtr<nsINodeInfoManager> nimgr;
    nsresult rv = NodeInfo()->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsINodeInfo> ni;
    rv = nimgr->GetNodeInfo(aQualifiedName, aNamespaceURI, *getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

    return SetAttr(ni, aValue, PR_TRUE);
}

NS_IMETHODIMP
nsXULElement::RemoveAttributeNS(const nsAReadableString& aNamespaceURI,
                                const nsAReadableString& aLocalName)
{
    PRInt32 nameSpaceId;
    nsCOMPtr<nsIAtom> tag = dont_AddRef(NS_NewAtom(aLocalName));

    gNameSpaceManager->GetNameSpaceID(aNamespaceURI, nameSpaceId);

    nsresult rv = UnsetAttr(nameSpaceId, tag, PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to remove attribute");

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,
                                 const nsAReadableString& aLocalName,
                                 nsIDOMAttr** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    nsresult rv;

    nsCOMPtr<nsIDOMNamedNodeMap> map;
    rv = GetAttributes(getter_AddRefs(map));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMNode> node;
    rv = map->GetNamedItemNS(aNamespaceURI, aLocalName, getter_AddRefs(node));
    if (NS_FAILED(rv)) return rv;

    if (node) {
        rv = node->QueryInterface(NS_GET_IID(nsIDOMAttr), (void**) aReturn);
    }
    else {
        *aReturn = nsnull;
        rv = NS_OK;
    }

    return rv;
}

NS_IMETHODIMP
nsXULElement::SetAttributeNodeNS(nsIDOMAttr* aNewAttr,
                                 nsIDOMAttr** aReturn)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULElement::GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                     const nsAReadableString& aLocalName,
                                     nsIDOMNodeList** aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    PRInt32 nameSpaceId = kNameSpaceID_Unknown;

    nsRDFDOMNodeList* elements;
    nsresult rv = nsRDFDOMNodeList::Create(&elements);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNodeList> kungFuGrip;
    kungFuGrip = dont_AddRef(NS_STATIC_CAST(nsIDOMNodeList *, elements));

    if (!aNamespaceURI.Equals(NS_LITERAL_STRING("*"))) {
        gNameSpaceManager->GetNameSpaceID(aNamespaceURI, nameSpaceId);

        if (nameSpaceId == kNameSpaceID_Unknown) {
            // Unkonwn namespace means no matches, we return an empty list...

            *aReturn = elements;
            NS_ADDREF(*aReturn);

            return NS_OK;
        }
    }

    rv = nsXULDocument::GetElementsByTagName(NS_STATIC_CAST(nsIStyledContent *,
                                                            this), aLocalName,
                                             nameSpaceId, elements);
    NS_ENSURE_SUCCESS(rv, rv);

    *aReturn = elements;
    NS_ADDREF(*aReturn);

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::HasAttribute(const nsAReadableString& aName, PRBool* aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    nsCOMPtr<nsIAtom> name;
    nsCOMPtr<nsINodeInfo> ni;
    PRInt32 nsid;

    nsresult rv = NormalizeAttrString(aName, *getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

    ni->GetNameAtom(*getter_AddRefs(name));
    ni->GetNamespaceID(nsid);

    nsAutoString tmp;
    rv = GetAttr(nsid, name, tmp);

    *aReturn = rv == NS_CONTENT_ATTR_NOT_THERE ? PR_FALSE : PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::HasAttributeNS(const nsAReadableString& aNamespaceURI,
                             const nsAReadableString& aLocalName,
                             PRBool* aReturn)
{
    NS_ENSURE_ARG_POINTER(aReturn);

    nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aLocalName)));
    PRInt32 nsid;

    gNameSpaceManager->GetNameSpaceID(aNamespaceURI, nsid);

    if (nsid == kNameSpaceID_Unknown) {
        // Unkonwn namespace means no attr...

        *aReturn = PR_FALSE;
        return NS_OK;
    }

    nsAutoString tmp;
    nsresult rv = GetAttr(nsid, name, tmp);

    *aReturn = rv == NS_CONTENT_ATTR_NOT_THERE ? PR_FALSE : PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetElementsByAttribute(const nsAReadableString& aAttribute,
                                     const nsAReadableString& aValue,
                                     nsIDOMNodeList** aReturn)
{
    nsresult rv;
    nsRDFDOMNodeList* elements;
    rv = nsRDFDOMNodeList::Create(&elements);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create node list");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMNode> domElement;
    rv = QueryInterface(NS_GET_IID(nsIDOMNode), getter_AddRefs(domElement));
    if (NS_SUCCEEDED(rv)) {
        GetElementsByAttribute(domElement, aAttribute, aValue, elements);
    }

    *aReturn = elements;
    return NS_OK;
}


//----------------------------------------------------------------------
// nsIXMLContent interface

NS_IMETHODIMP
nsXULElement::SetContainingNameSpace(nsINameSpace* aNameSpace)
{
    nsresult rv;

    rv = EnsureSlots();
    if (NS_FAILED(rv)) return rv;

    mSlots->mNameSpace = dont_QueryInterface(aNameSpace);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetContainingNameSpace(nsINameSpace*& aNameSpace) const
{
    nsresult rv;

    if (NameSpace()) {
        // If we have a namespace, return it.
        aNameSpace = NameSpace();
        NS_ADDREF(aNameSpace);
        return NS_OK;
    }

    // Next, try our parent.
    nsCOMPtr<nsIContent> parent( dont_QueryInterface(mParent) );
    while (parent) {
        nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(parent) );
        if (xml)
            return xml->GetContainingNameSpace(aNameSpace);

        nsCOMPtr<nsIContent> temp = parent;
        rv = temp->GetParent(*getter_AddRefs(parent));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get parent");
        if (NS_FAILED(rv)) return rv;
    }

    // Allright, we walked all the way to the top of our containment
    // hierarchy and couldn't find a parent that supported
    // nsIXMLContent. If we're in a document, try to doc's root
    // element.
    if (mDocument) {
        nsCOMPtr<nsIContent> docroot;
        mDocument->GetRootContent(getter_AddRefs(docroot));

        // Wow! Nasty cast to get an unambiguous, non-const
        // nsISupports pointer. We want to make sure that we're not
        // the docroot (this would otherwise spin off into infinity).
        nsISupports* me = NS_STATIC_CAST(nsIStyledContent*, NS_CONST_CAST(nsXULElement*, this));

        if (docroot && !::SameCOMIdentity(docroot, me)) {
            nsCOMPtr<nsIXMLContent> xml( do_QueryInterface(docroot) );
            if (xml)
                return xml->GetContainingNameSpace(aNameSpace);
        }
    }

    aNameSpace = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::MaybeTriggerAutoLink(nsIWebShell *aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetXMLBaseURI(nsIURI **aURI)
{
  // XXX TODO, should share the impl with nsXMLElement
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI=nsnull;
  if (mDocument) {
    mDocument->GetBaseURL(*aURI);
    if (!*aURI) {
      mDocument->GetDocumentURL(aURI);
    }
  }
  return NS_OK;
}

#if 0
NS_IMETHODIMP
nsXULElement::GetBaseURI(nsAWritableString &aURI)
{
  // XXX TODO, should share the impl with nsXMLElement
  aURI.Truncate();
  nsresult rv = NS_OK;
  if (mDocument) {
    nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mDocument));
    if (doc) {
      rv = doc->GetBaseURI(aURI);
    }
  }
  return rv;
}
#endif

//----------------------------------------------------------------------
// nsIXULContent interface

NS_IMETHODIMP
nsXULElement::PeekChildCount(PRInt32& aCount) const
{
    aCount = mChildren.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetLazyState(PRInt32 aFlags)
{
    mLazyState |= aFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::ClearLazyState(PRInt32 aFlags)
{
    mLazyState &= ~aFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetLazyState(PRInt32 aFlag, PRBool& aResult)
{
    aResult = (mLazyState & aFlag) ? PR_TRUE : PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::AddScriptEventListener(nsIAtom* aName,
                                     const nsAReadableString& aValue)
{
    if (! mDocument)
        return NS_OK; // XXX

    nsresult rv;
    nsCOMPtr<nsIScriptContext> context;
    nsCOMPtr<nsIScriptGlobalObject> global;
    {
        mDocument->GetScriptGlobalObject(getter_AddRefs(global));

        // This can happen normally as part of teardown code.
        if (! global)
            return NS_OK;

        rv = global->GetContext(getter_AddRefs(context));
        if (NS_FAILED(rv)) return rv;

        if (!context) return NS_OK;
    }

    nsCOMPtr<nsIContent> root;
    mDocument->GetRootContent(getter_AddRefs(root));
    nsCOMPtr<nsIContent> content(do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, this)));
    if ((!root || root == content) && !NodeInfo()->Equals(nsXULAtoms::overlay)) {
        nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(global);
        if (! receiver)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIEventListenerManager> manager;
        rv = receiver->GetListenerManager(getter_AddRefs(manager));
        if (NS_FAILED(rv)) return rv;

        rv = manager->AddScriptEventListener(context, global, aName,
                                             aValue, PR_FALSE);
    }
    else {
        nsCOMPtr<nsIEventListenerManager> manager;
        rv = GetListenerManager(getter_AddRefs(manager));
        if (NS_FAILED(rv)) return rv;

        rv = manager->AddScriptEventListener(context,
                                             NS_STATIC_CAST(nsIContent *,
                                                            this),
                                             aName, aValue, PR_TRUE);
    }

    return rv;
}


NS_IMETHODIMP
nsXULElement::ForceElementToOwnResource(PRBool aForce)
{
    nsresult rv;

    rv = EnsureSlots();
    if (NS_FAILED(rv)) return rv;

    if (aForce) {
        rv = GetResource(getter_AddRefs(mSlots->mOwnedResource));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // drop reference
        mSlots->mOwnedResource = nsnull;
    }

    return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMEventReceiver interface

NS_IMETHODIMP
nsXULElement::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    nsIEventListenerManager *manager;

    if (NS_OK == GetListenerManager(&manager)) {
        manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        NS_RELEASE(manager);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULElement::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
    if (mListenerManager) {
        mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULElement::AddEventListener(const nsAReadableString& aType,
                               nsIDOMEventListener* aListener,
                               PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULElement::RemoveEventListener(const nsAReadableString& aType,
                                  nsIDOMEventListener* aListener,
                                  PRBool aUseCapture)
{
  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULElement::DispatchEvent(nsIDOMEvent* aEvent, PRBool *_retval)
{
  // Obtain a presentation context
  PRInt32 count = mDocument->GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  mDocument->GetShellAt(0, getter_AddRefs(shell));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> aPresContext;
  shell->GetPresContext(getter_AddRefs(aPresContext));

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(esm)))) {
    return esm->DispatchNewEvent(NS_STATIC_CAST(nsIStyledContent*, this), aEvent, _retval);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXULElement::GetListenerManager(nsIEventListenerManager** aResult)
{
    if (! mListenerManager) {
        nsresult rv;

        rv = nsComponentManager::CreateInstance(kEventListenerManagerCID,
                                                nsnull,
                                                NS_GET_IID(nsIEventListenerManager),
                                                getter_AddRefs(mListenerManager));
        if (NS_FAILED(rv)) return rv;
        mListenerManager->SetListenerTarget(NS_STATIC_CAST(nsIStyledContent*, this));
    }

    *aResult = mListenerManager;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool noDefault;
  return DispatchEvent(aEvent, &noDefault);
}


//----------------------------------------------------------------------

#if 0
// XXX move this over to the scriptable helper...
static PRBool CanHaveBinding(nsIAtom* aTag) {
  // The layout atoms (the boxes, stacks, and spacers) are dodgy here.
  // Technically they could have bindings, but this will only apply
  // for display: none elts anyway, so we're getting into a real edge
  // case.
  return (aTag != nsXULAtoms::broadcaster) && (aTag != nsXULAtoms::commandset) &&
    (aTag != nsXULAtoms::commands) && (aTag != nsXULAtoms::command) && (aTag != nsXULAtoms::popupgroup) &&
    (aTag != nsXULAtoms::broadcasterset) && (aTag != nsXULAtoms::templateAtom) &&
    (aTag != nsXULAtoms::box) && (aTag != nsXULAtoms::hbox) && (aTag != nsXULAtoms::vbox) &&
    (aTag != nsXULAtoms::stack) && (aTag != nsXULAtoms::spring);
}

NS_IMETHODIMP
nsXULElement::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
    nsresult rv = NS_OK;

    if (! mScriptObject) {
        // Use the XBL service to get the `base' tag, which'll be how
        // we determine what kind of script object to cook up.
        nsIXBLService *xblService = GetXBLService();
        NS_ASSERTION(xblService != nsnull, "couldn't get XBL service");
        if (! xblService)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIAtom> tag;
        PRInt32 dummy;
        xblService->ResolveTag(NS_STATIC_CAST(nsIStyledContent*, this), &dummy, getter_AddRefs(tag));

        // Use the DOM's script object factory to cough up a script
        // object
        nsAutoString tagStr;
        tag->ToString(tagStr);

        nsCOMPtr<nsIDOMScriptObjectFactory> factory
            = do_GetService(kDOMScriptObjectFactoryCID, &rv);

        NS_ASSERTION(factory != nsnull, "couldn't get script object factory");
        if (! factory)
            return NS_ERROR_UNEXPECTED;

        // We'll either be parented by the element that encloses us
        // (if there is one), or the document.
        nsISupports* parent =  mParent
            ? NS_STATIC_CAST(nsISupports*, mParent)
            : NS_STATIC_CAST(nsISupports*, mDocument);

        void* scriptObject;
        rv = factory->NewScriptXULElement(tagStr, aContext,
                                          NS_STATIC_CAST(nsIStyledContent*, this),
                                          parent,
                                          &scriptObject);
        if (NS_FAILED(rv)) return rv;

        if (mScriptObject) {
            // We must have re-entered; discard the newly created
            // script object and use the one created during the
            // nesting instead.
            JSContext* cx = (JSContext*) aContext->GetNativeContext();
            ::JS_SetPrivate(cx, (JSObject*) scriptObject, nsnull);

            // Since we've eagerly cleared the transient script
            // object's native pointer, we now need to ``manually''
            // balance the reference that it had to us
            Release();

            *aScriptObject = mScriptObject;
            return NS_OK;
        }

        mScriptObject = scriptObject;

        // Only root if a script object was actually created.
        if (! mScriptObject) {
            *aScriptObject = nsnull;
            return NS_OK;
        }

        // tag's name
        const char* rootname;
        if (tag.get() == nsXULAtoms::tree)
            rootname = "nsXULTreeElement::mScriptObject";
        else
            rootname = "nsXULElement::mScriptObject";

        // Ensure that a reference exists to this element.
        //
        // XXX This is different from nsGenericElement, which doesn't
        // root until the element is in the document; however, we're
        // screwed, and GC will cause us to lose properties if we
        // don't eagerly root.
        aContext->AddNamedReference((void*) &mScriptObject, mScriptObject, rootname);

#ifdef DEBUG
        mIsScriptObjectRooted = PR_TRUE;
#endif

        // See if we have a frame.
        nsCOMPtr<nsIAtom> ourTag;
        GetTag(*getter_AddRefs(ourTag));
        if (mDocument && CanHaveBinding(ourTag)) {
          nsCOMPtr<nsIPresShell> shell;
          mDocument->GetShellAt(0, getter_AddRefs(shell));
          if (shell) {
            nsIFrame* frame;
            shell->GetPrimaryFrameFor(NS_STATIC_CAST(nsIStyledContent*, this), &frame);
            if (!frame) {
              // We must ensure that the XBL Binding is installed before we hand
              // back this object.
              nsCOMPtr<nsIBindingManager> bindingManager;
              mDocument->GetBindingManager(getter_AddRefs(bindingManager));
              nsCOMPtr<nsIXBLBinding> binding;
              bindingManager->GetBinding(NS_STATIC_CAST(nsIStyledContent*, this), getter_AddRefs(binding));
              if (!binding) {
                nsCOMPtr<nsIScriptGlobalObject> global;
                mDocument->GetScriptGlobalObject(getter_AddRefs(global));
                nsCOMPtr<nsIDOMViewCSS> viewCSS(do_QueryInterface(global));
                if (viewCSS) {
                  nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
                  nsAutoString empty;
                  viewCSS->GetComputedStyle(this, empty, getter_AddRefs(cssDecl));
                  /*nsString str;
                  ourTag->ToString(str);
                  nsCString cstr; cstr.AssignWithConversion(str);
                  printf("XUL ELEMENT UH-OH! %s\n", (const char*)cstr);
*/
                  if (cssDecl) {
                    nsAutoString behavior; behavior.Assign(NS_LITERAL_STRING("-moz-binding"));
                    nsAutoString value;
                    cssDecl->GetPropertyValue(behavior, value);
                    if (!value.IsEmpty()) {
                      // We have a binding that must be installed.
                      PRBool dummy2;
                      xblService->LoadBindings(NS_STATIC_CAST(nsIStyledContent*, this), value, PR_FALSE,
                                               getter_AddRefs(binding), &dummy2);
                      if (binding) {
                        binding->ExecuteAttachedHandler();
                      }
                    }
                  }
                }
              }
            }
          }
        }
    }

    *aScriptObject = mScriptObject;

    return rv;
}

NS_IMETHODIMP
nsXULElement::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}
#endif


//----------------------------------------------------------------------
// nsIScriptEventHandlerOwner interface

NS_IMETHODIMP
nsXULElement::GetCompiledEventHandler(nsIAtom *aName, void** aHandler)
{
    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheTests);
    *aHandler = nsnull;
    if (mPrototype) {
        for (PRInt32 i = 0; i < mPrototype->mNumAttributes; ++i) {
            nsXULPrototypeAttribute* attr = &(mPrototype->mAttributes[i]);

            if (attr->mNodeInfo->Equals(aName, kNameSpaceID_None)) {
                XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheHits);
                *aHandler = attr->mEventHandler;
                break;
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::CompileEventHandler(nsIScriptContext* aContext,
                                  void* aTarget,
                                  nsIAtom *aName,
                                  const nsAReadableString& aBody,
                                  void** aHandler)
{
    nsresult rv;

    XUL_PROTOTYPE_ATTRIBUTE_METER(gNumCacheSets);

    nsCOMPtr<nsIScriptContext> context;
    JSObject* scopeObject;
    PRBool shared;

    if (mPrototype) {
        // It'll be shared amonst the instances of the prototype
        shared = PR_TRUE;

        // Use the prototype document's special context
        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
        NS_ASSERTION(xuldoc != nsnull, "mDocument is not an nsIXULDocument");
        if (! xuldoc)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIXULPrototypeDocument> protodoc;
        rv = xuldoc->GetMasterPrototype(getter_AddRefs(protodoc));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(protodoc != nsnull, "xul document has no prototype");
        if (! protodoc)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner = do_QueryInterface(protodoc);
        nsCOMPtr<nsIScriptGlobalObject> global;
        globalOwner->GetScriptGlobalObject(getter_AddRefs(global));
        NS_ASSERTION(global != nsnull, "prototype doc does not have a script global");
        if (! global)
            return NS_ERROR_UNEXPECTED;

        rv = global->GetContext(getter_AddRefs(context));
        if (NS_FAILED(rv)) return rv;

        // Use the prototype script's special scope object

        scopeObject = global->GetGlobalJSObject();
        if (!scopeObject)
            return NS_ERROR_UNEXPECTED;
    }
    else {
        // We don't have a prototype; do a one-off compile.
        shared = PR_FALSE;
        context = aContext;
        scopeObject = NS_REINTERPRET_CAST(JSObject*, aTarget);
    }

    NS_ASSERTION(context != nsnull, "no script context");
    if (! context)
        return NS_ERROR_UNEXPECTED;

    // Compile the event handler
    rv = context->CompileEventHandler(scopeObject, aName, aBody, shared, aHandler);
    if (NS_FAILED(rv)) return rv;

    if (shared) {
        // If it's a shared handler, we need to bind the shared
        // function object to the real target.
        rv = aContext->BindCompiledEventHandler(aTarget, aName, *aHandler);
        if (NS_FAILED(rv)) return rv;
    }

    if (mPrototype) {
        // Remember the compiled event handler
        for (PRInt32 i = 0; i < mPrototype->mNumAttributes; ++i) {
            nsXULPrototypeAttribute* attr = &(mPrototype->mAttributes[i]);

            if (attr->mNodeInfo->Equals(aName, kNameSpaceID_None)) {
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

                break;
            }
        }
    }

    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsIContent interface
//

NS_IMETHODIMP
nsXULElement::GetDocument(nsIDocument*& aResult) const
{
    aResult = mDocument;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
{
    nsresult rv;

    if (aDocument != mDocument) {
        nsCOMPtr<nsIXULDocument> rdfDoc;
        if (mDocument) {
            /*
            if (mScriptObject) {
                nsCOMPtr<nsIScriptGlobalObject> global;
                mDocument->GetScriptGlobalObject(getter_AddRefs(global));
                if (global) {
                    nsCOMPtr<nsIScriptContext> context;
                    global->GetContext(getter_AddRefs(context));
                    if (context) {
                        context->RemoveReference((void*) &mScriptObject, mScriptObject);
#ifdef DEBUG
                        mIsScriptObjectRooted = PR_FALSE;
#endif
                    }
                }
            }
            */

            // XXX: Unroot!!!
        }


        if (mDocument) {
          // Notify XBL- & nsIAnonymousContentCreator-generated
          // anonymous content that the document is changing.
          nsCOMPtr<nsIBindingManager> bindingManager;
          mDocument->GetBindingManager(getter_AddRefs(bindingManager));
          NS_ASSERTION(bindingManager, "no binding manager");
          if (bindingManager) {
            bindingManager->ChangeDocumentFor(NS_STATIC_CAST(nsIStyledContent*, this), mDocument, aDocument);
          }

          nsIDOMElement* domElement = NS_STATIC_CAST(nsIDOMElement*, this);
          nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(mDocument));
          nsDoc->SetBoxObjectFor(domElement, nsnull);
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

        mDocument = aDocument; // not refcounted

        if (mDocument) {
            // Add a named reference to the script object.
            /*
            if (mScriptObject) {
                nsCOMPtr<nsIScriptGlobalObject> global;
                mDocument->GetScriptGlobalObject(getter_AddRefs(global));
                if (global) {
                    nsCOMPtr<nsIScriptContext> context;
                    global->GetContext(getter_AddRefs(context));
                    if (context) {
                        context->AddNamedReference((void*) &mScriptObject, mScriptObject, "nsXULElement::mScriptObject");

#ifdef DEBUG
                        mIsScriptObjectRooted = PR_TRUE;
#endif
                    }
                }
            }
            */

            // XXX: Root!!!

            // When we SetDocument(), we're either adding an element
            // into the document that wasn't there before, or we're
            // moving the element from one document to
            // another. Regardless, we need to (re-)initialize several
            // attributes that are dependant on the document. Do that
            // now.
            PRInt32 count;
            GetAttrCount(count);

            for (PRInt32 i = 0; i < count; ++i) {
                PRInt32 nameSpaceID;
                nsCOMPtr<nsIAtom> attr, prefix;
                GetAttrNameAt(i, nameSpaceID, *getter_AddRefs(attr), *getter_AddRefs(prefix));

                PRBool reset = PR_FALSE;

                if (nameSpaceID == kNameSpaceID_None) {
                    if (aCompileEventHandlers) {
                        nsIID iid;
                        rv = GetEventHandlerIID(attr, &iid, &reset);
                        if (NS_FAILED(rv)) return rv;
                    }

                    if (! reset) {
                        if ((attr.get() == nsXULAtoms::popup) || // XXXdwh deprecated
                            (attr.get() == nsXULAtoms::tooltip) ||
                            (attr.get() == nsXULAtoms::context) || // XXXdwh deprecated
                            (attr.get() == nsXULAtoms::menu) ||
                            (attr.get() == nsXULAtoms::contextmenu) ||
                            (attr.get() == nsXULAtoms::style)) {
                            reset = PR_TRUE;
                        }
                    }
                }

                if (reset) {
                    nsAutoString value;
                    rv = GetAttr(nameSpaceID, attr, value);
                    if (NS_FAILED(rv)) return rv;

                    rv = SetAttr(nameSpaceID, attr, value, PR_FALSE);
                    if (NS_FAILED(rv)) return rv;
                }
            }
        }
    }

    if (aDeep) {
        for (PRInt32 i = mChildren.Count() - 1; i >= 0; --i) {
            nsIContent* child = NS_STATIC_CAST(nsIContent*, mChildren[i]);
            child->SetDocument(aDocument, aDeep, aCompileEventHandlers);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetParent(nsIContent*& aResult) const
{
    aResult = mParent;
    NS_IF_ADDREF(aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetParent(nsIContent* aParent)
{
    mParent = aParent; // no refcount
    if (aParent) {
      nsCOMPtr<nsIContent> bindingPar;
      aParent->GetBindingParent(getter_AddRefs(bindingPar));
      if (bindingPar)
        SetBindingParent(bindingPar);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::CanContainChildren(PRBool& aResult) const
{
    // XXX Hmm -- not sure if this is unilaterally true...
    aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::ChildCount(PRInt32& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated())) {
        aResult = 0;
        return rv;
    }

    return PeekChildCount(aResult);
}

NS_IMETHODIMP
nsXULElement::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated())) {
        aResult = nsnull;
        return rv;
    }

    nsIContent* content = NS_STATIC_CAST(nsIContent*, mChildren[aIndex]);
    NS_IF_ADDREF(aResult = content);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated())) {
        aResult = -1;
        return rv;
    }

    aResult = mChildren.IndexOf(aPossibleChild);
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION(nsnull != aKid, "null ptr");

    // Make sure that we're not trying to insert the same child
    // twice. If we do, the DOM APIs (e.g., GetNextSibling()), will
    // freak out.
    NS_ASSERTION(mChildren.IndexOf(aKid) < 0, "element is already a child");

    PRBool insertOk = mChildren.InsertElementAt(aKid, aIndex);
    if (insertOk) {
        NS_ADDREF(aKid);
        aKid->SetParent(NS_STATIC_CAST(nsIStyledContent*, this));
        //nsRange::OwnerChildInserted(this, aIndex);

        aKid->SetDocument(mDocument, aDeepSetDocument, PR_TRUE);

        if (mDocument && HasMutationListeners(NS_STATIC_CAST(nsIStyledContent*,this),
                                              NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
          nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
          nsMutationEvent mutation;
          mutation.eventStructType = NS_MUTATION_EVENT;
          mutation.message = NS_MUTATION_NODEINSERTED;
          mutation.mTarget = node;

          nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*,this)));
          mutation.mRelatedNode = relNode;

          nsEventStatus status = nsEventStatus_eIgnore;
          aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
        }

        if (aNotify && mDocument) {
          mDocument->ContentInserted(NS_STATIC_CAST(nsIStyledContent*, this), aKid, aIndex);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                             PRBool aDeepSetDocument)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION(nsnull != aKid, "null ptr");
    if (! aKid)
        return NS_ERROR_NULL_POINTER;

    nsIContent* oldKid = NS_STATIC_CAST(nsIContent*, mChildren[aIndex]);
    NS_ASSERTION(oldKid != nsnull, "old kid not nsIContent");
    if (! oldKid)
        return NS_ERROR_FAILURE;

    if (oldKid == aKid)
        return NS_OK;

    PRBool replaceOk = mChildren.ReplaceElementAt(aKid, aIndex);
    if (replaceOk) {
        NS_ADDREF(aKid);
        aKid->SetParent(NS_STATIC_CAST(nsIStyledContent*, this));
        //nsRange::OwnerChildReplaced(this, aIndex, oldKid);

        aKid->SetDocument(mDocument, aDeepSetDocument, PR_TRUE);

        if (aNotify && mDocument) {
            mDocument->ContentReplaced(NS_STATIC_CAST(nsIStyledContent*, this), oldKid, aKid, aIndex);
        }

        // This will cause the script object to be unrooted for each
        // element in the subtree.
        oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);

        // We've got no mo' parent.
        oldKid->SetParent(nsnull);
        NS_RELEASE(oldKid);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::AppendChildTo(nsIContent* aKid, PRBool aNotify, PRBool aDeepSetDocument)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    NS_PRECONDITION((nsnull != aKid) && (aKid != NS_STATIC_CAST(nsIStyledContent*, this)), "null ptr");

    PRBool appendOk = mChildren.AppendElement(aKid);
    if (appendOk) {
        NS_ADDREF(aKid);
        aKid->SetParent(NS_STATIC_CAST(nsIStyledContent*, this));
        // ranges don't need adjustment since new child is at end of list

        aKid->SetDocument(mDocument, aDeepSetDocument, PR_TRUE);

        if (mDocument && HasMutationListeners(NS_STATIC_CAST(nsIStyledContent*,this),
                                              NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
          nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
          nsMutationEvent mutation;
          mutation.eventStructType = NS_MUTATION_EVENT;
          mutation.message = NS_MUTATION_NODEINSERTED;
          mutation.mTarget = node;

          nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*,this)));
          mutation.mRelatedNode = relNode;

          nsEventStatus status = nsEventStatus_eIgnore;
          aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
        }

        if (aNotify && mDocument) {
            mDocument->ContentAppended(NS_STATIC_CAST(nsIStyledContent*, this), mChildren.Count() - 1);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
    nsresult rv;
    if (NS_FAILED(rv = EnsureContentsGenerated()))
        return rv;

    nsIContent* oldKid = NS_STATIC_CAST(nsIContent*, mChildren[aIndex]);
    if (! oldKid)
        return NS_ERROR_FAILURE;

    if (HasMutationListeners(NS_STATIC_CAST(nsIStyledContent*,this), NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(oldKid));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_NODEREMOVED;
      mutation.mTarget = node;

      nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*,this)));
      mutation.mRelatedNode = relNode;

      nsEventStatus status = nsEventStatus_eIgnore;
      oldKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
    }

    // On the removal of a <treeitem>, <treechildren>, or <treecell> element,
    // the possibility exists that some of the items in the removed subtree
    // are selected (and therefore need to be deselected). We need to account for this.
    nsCOMPtr<nsIAtom> tag;
    nsCOMPtr<nsIDOMXULTreeElement> treeElement;
    nsCOMPtr<nsITreeBoxObject> treeBox;
    PRBool fireSelectionHandler = PR_FALSE;

    // -1 = do nothing, -2 = null out current item
    // anything else = index to re-set as current
    PRInt32 newCurrentIndex = -1;

    oldKid->GetTag(*getter_AddRefs(tag));
    if (tag && (tag.get() == nsXULAtoms::treechildren || tag.get() == nsXULAtoms::treeitem ||
                tag.get() == nsXULAtoms::treecell)) {
      // This is the nasty case. We have (potentially) a slew of selected items
      // and cells going away.
      // First, retrieve the tree.
      // Check first whether this element IS the tree
      treeElement = do_QueryInterface((nsIDOMXULElement*)this);

      // If it's not, look at our parent
      if (!treeElement)
        rv = GetParentTree(getter_AddRefs(treeElement));
      if (treeElement) {
        nsCOMPtr<nsIDOMNodeList> itemList;
        treeElement->GetSelectedItems(getter_AddRefs(itemList));

        nsCOMPtr<nsIDOMNode> parentKid = do_QueryInterface(oldKid);
        if (itemList) {
          // Iterate over all of the items and find out if they are contained inside
          // the removed subtree.
          PRUint32 length;
          itemList->GetLength(&length);
          for (PRUint32 i = 0; i < length; i++) {
            nsCOMPtr<nsIDOMNode> node;
            itemList->Item(i, getter_AddRefs(node));
            if (IsAncestor(parentKid, node)) {
              nsCOMPtr<nsIContent> content = do_QueryInterface(node);
              content->UnsetAttr(kNameSpaceID_None, nsXULAtoms::selected, PR_FALSE);
              nsCOMPtr<nsIXULTreeContent> tree = do_QueryInterface(treeElement);
              nsCOMPtr<nsIDOMXULElement> domxulnode = do_QueryInterface(node);
              if (tree && domxulnode)
                tree->CheckSelection(domxulnode);
              length--;
              i--;
              fireSelectionHandler = PR_TRUE;
            }
          }
        }

        nsCOMPtr<nsIDOMXULElement> curItem;
        treeElement->GetCurrentItem(getter_AddRefs(curItem));
        nsCOMPtr<nsIDOMNode> curNode = do_QueryInterface(curItem);
        if (IsAncestor(parentKid, curNode)) {
            // Current item going away
            nsCOMPtr<nsIBoxObject> box;
            treeElement->GetBoxObject(getter_AddRefs(box));
            treeBox = do_QueryInterface(box);
            if (treeBox) {
                nsCOMPtr<nsIDOMElement> domElem = do_QueryInterface(parentKid);
                if (domElem)
                    treeBox->GetIndexOfItem(domElem, &newCurrentIndex);
            }

            // If any of this fails, we'll just set the current item to null
            if (newCurrentIndex == -1)
              newCurrentIndex = -2;
        }
      }
    }

    if (oldKid) {
        nsIDocument* doc = mDocument;
        PRBool removeOk = mChildren.RemoveElementAt(aIndex);
        //nsRange::OwnerChildRemoved(this, aIndex, oldKid);
        if (aNotify && removeOk && mDocument) {
            doc->ContentRemoved(NS_STATIC_CAST(nsIStyledContent*, this), oldKid, aIndex);
        }

        if (newCurrentIndex == -2)
            treeElement->SetCurrentItem(nsnull);
        else if (newCurrentIndex > -1) {
            // Make sure the index is still valid
            PRInt32 treeRows;
            treeBox->GetRowCount(&treeRows);
            if (treeRows > 0) {
                newCurrentIndex = PR_MIN((treeRows - 1), newCurrentIndex);
                nsCOMPtr<nsIDOMElement> newCurrentItem;
                treeBox->GetItemAtIndex(newCurrentIndex, getter_AddRefs(newCurrentItem));
                if (newCurrentItem) {
                    nsCOMPtr<nsIDOMXULElement> xulCurItem = do_QueryInterface(newCurrentItem);
                    if (xulCurItem)
                        treeElement->SetCurrentItem(xulCurItem);
                }
            } else {
                treeElement->SetCurrentItem(nsnull);
            }
        }

        if (fireSelectionHandler) {
          nsCOMPtr<nsIXULTreeContent> tree = do_QueryInterface(treeElement);
          if (tree) {
            tree->FireOnSelectHandler();
          }
        }

        // This will cause the script object to be unrooted for each
        // element in the subtree.
        oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);

        // We've got no mo' parent.
        oldKid->SetParent(nsnull);
        NS_RELEASE(oldKid);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
    return NodeInfo()->GetNamespaceID(aNameSpaceID);
}

NS_IMETHODIMP
nsXULElement::GetTag(nsIAtom*& aResult) const
{
    return NodeInfo()->GetNameAtom(aResult);
}

NS_IMETHODIMP
nsXULElement::NormalizeAttrString(const nsAReadableString& aStr,
                                  nsINodeInfo*& aNodeInfo)
{
    PRInt32 i, count = Attributes() ? Attributes()->Count() : 0;
    for (i = 0; i < count; i++) {
        nsXULAttribute* attr = NS_REINTERPRET_CAST(nsXULAttribute*,
                                                   Attributes()->ElementAt(i));
        nsINodeInfo *ni = attr->GetNodeInfo();
        if (ni->QualifiedNameEquals(aStr)) {
            aNodeInfo = ni;
            NS_ADDREF(aNodeInfo);

            return NS_OK;
        }
    }

    count = mPrototype ? mPrototype->mNumAttributes : 0;
    for (i = 0; i < count; i++) {
        nsXULPrototypeAttribute* attr = &(mPrototype->mAttributes[i]);

        nsINodeInfo *ni = attr->mNodeInfo;
        if (ni->QualifiedNameEquals(aStr)) {
            aNodeInfo = ni;
            NS_ADDREF(aNodeInfo);

            return NS_OK;
        }
    }

    nsCOMPtr<nsINodeInfoManager> nimgr;
    NodeInfo()->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

    return nimgr->GetNodeInfo(aStr, nsnull, kNameSpaceID_None, aNodeInfo);
}


// XXX attribute code swiped from nsGenericContainerElement
// this class could probably just use nsGenericContainerElement
// needed to maintain attribute namespace ID as well as ordering
NS_IMETHODIMP
nsXULElement::SetAttr(nsINodeInfo* aNodeInfo,
                      const nsAReadableString& aValue,
                      PRBool aNotify)
{
    NS_ASSERTION(nsnull != aNodeInfo, "must have attribute nodeinfo");
    if (nsnull == aNodeInfo)
        return NS_ERROR_NULL_POINTER;

    nsresult rv = NS_OK;

    nsCOMPtr<nsIAtom> attrName;
    PRInt32 attrns;

    aNodeInfo->GetNameAtom(*getter_AddRefs(attrName));
    aNodeInfo->GetNamespaceID(attrns);

    if (mDocument) {
        mDocument->AttributeWillChange(this, attrns, attrName);
    }

    if (! Attributes()) {
        rv = EnsureSlots();
        if (NS_FAILED(rv)) return rv;

        // Since EnsureSlots() may have triggered mSlots->mAttributes construction,
        // we need to check _again_ before creating attributes.
        if (! Attributes()) {
            rv = nsXULAttributes::Create(NS_STATIC_CAST(nsIStyledContent*, this), &(mSlots->mAttributes));
            if (NS_FAILED(rv)) return rv;
        }
    }

    // XXX Class and Style attribute setting should be checking for the XUL namespace!

    // Check to see if the CLASS attribute is being set.  If so, we need to rebuild our
    // class list.
    if (aNodeInfo->Equals(nsXULAtoms::clazz, kNameSpaceID_None)) {
        Attributes()->UpdateClassList(aValue);
    }

    // Check to see if the STYLE attribute is being set.  If so, we need to create a new
    // style rule based off the value of this attribute, and we need to let the document
    // know about the StyleRule change.
    if (aNodeInfo->Equals(nsXULAtoms::style, kNameSpaceID_None) &&
        (mDocument != nsnull)) {
        nsCOMPtr <nsIURI> docURL;
        mDocument->GetBaseURL(*getter_AddRefs(docURL));
        Attributes()->UpdateStyleRule(docURL, aValue);
        // XXX Some kind of special document update might need to happen here.
    }

    // Need to check for the SELECTED attribute
    // being set.  If we're a <treeitem>, <treerow>, or <treecell>, the act of
    // setting these attributes forces us to update our selected arrays.
    nsCOMPtr<nsIAtom> tag;
    GetTag(*getter_AddRefs(tag));
    if (mDocument && aNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
      // See if we're a treeitem atom.
      nsCOMPtr<nsIRDFNodeList> nodeList;
      if (tag && (tag.get() == nsXULAtoms::treeitem) &&
          aNodeInfo->Equals(nsXULAtoms::selected)) {
        nsCOMPtr<nsIDOMXULTreeElement> treeElement;
        GetParentTree(getter_AddRefs(treeElement));
        if (treeElement) {
          nsCOMPtr<nsIDOMNodeList> nodes;
          treeElement->GetSelectedItems(getter_AddRefs(nodes));
          nodeList = do_QueryInterface(nodes);
        }
      }

      if (nodeList) {
        // Append this node to the list.
        nodeList->AppendNode(this);
      }
    }


    // Check to see if the POPUP attribute is being set.  If so, we need to attach
    // a new instance of our popup handler to the node.
    if (mDocument && (aNodeInfo->Equals(nsXULAtoms::popup, kNameSpaceID_None) || // XXXdwh deprecated
                      aNodeInfo->Equals(nsXULAtoms::menu, kNameSpaceID_None) ||
                      aNodeInfo->Equals(nsXULAtoms::tooltip, kNameSpaceID_None) || 
                      aNodeInfo->Equals(nsXULAtoms::contextmenu, kNameSpaceID_None) ||
                      aNodeInfo->Equals(nsXULAtoms::context, kNameSpaceID_None))) // XXXdwh deprecated
    {
        AddPopupListener(attrName);
    }

    // XXX need to check if they're changing an event handler: if so, then we need
    // to unhook the old one.

    nsXULAttribute* attr;
    PRInt32 i = 0;
    PRInt32 count = Attributes()->Count();
    while (i < count) {
        attr = Attributes()->ElementAt(i);
        if (aNodeInfo->Equals(attr->GetNodeInfo()))
            break;
        i++;
    }

    PRBool modification = PR_TRUE;
    nsAutoString oldValue;

    if (i < count) {
        attr->GetValue(oldValue);
        attr->SetValueInternal(aValue);
    }
    else {
        // didn't find it
        modification = PR_FALSE;
        rv = nsXULAttribute::Create(NS_STATIC_CAST(nsIStyledContent*, this),
                                    aNodeInfo, aValue, &attr);
        if (NS_FAILED(rv)) return rv;

        // transfer ownership here...
        Attributes()->AppendElement(attr);
    }

    // Check to see if this is an event handler, and add a script
    // listener if necessary.
    {
        nsIID iid;
        PRBool found;

        rv = GetEventHandlerIID(attrName, &iid, &found);
        if (NS_FAILED(rv)) return rv;

        if (found) {
            rv = AddScriptEventListener(attrName, aValue);
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Notify any broadcasters that are listening to this node.
    if (BroadcastListeners())
    {
        nsAutoString attribute;
        aNodeInfo->GetName(attribute);
        count = BroadcastListeners()->Count();
        for (i = 0; i < count; i++) {
            XULBroadcastListener* xulListener =
                NS_REINTERPRET_CAST(XULBroadcastListener*, BroadcastListeners()->ElementAt(i));

            if (xulListener->ObservingAttribute(attribute) &&
               (!aNodeInfo->Equals(nsXULAtoms::id)) &&
               (!aNodeInfo->Equals(nsXULAtoms::persist)) &&
               (!aNodeInfo->Equals(nsXULAtoms::ref)))
            {
                // XXX Should have a function that knows which attributes are special.
                // First we set the attribute in the observer.
                xulListener->mListener->SetAttribute(attribute, aValue);
                ExecuteOnBroadcastHandler(xulListener->mListener, attribute);
            }
        }
    }

    if (NS_SUCCEEDED(rv) && mDocument) {
      nsCOMPtr<nsIBindingManager> bindingManager;
      mDocument->GetBindingManager(getter_AddRefs(bindingManager));
      nsCOMPtr<nsIXBLBinding> binding;
      bindingManager->GetBinding(NS_STATIC_CAST(nsIStyledContent*, this), getter_AddRefs(binding));

      if (binding)
        binding->AttributeChanged(attrName, attrns, PR_FALSE);

      if (HasMutationListeners(NS_STATIC_CAST(nsIStyledContent*, this), NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, this)));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_ATTRMODIFIED;
        mutation.mTarget = node;

        nsAutoString attrName2;
        attrName->ToString(attrName2);
        nsCOMPtr<nsIDOMAttr> attrNode;
        GetAttributeNode(attrName2, getter_AddRefs(attrNode));
        mutation.mRelatedNode = attrNode;

        mutation.mAttrName = attrName;
        if (!oldValue.IsEmpty())
          mutation.mPrevAttrValue = getter_AddRefs(NS_NewAtom(oldValue));
        if (!aValue.IsEmpty())
          mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(aValue));
        if (modification)
          mutation.mAttrChange = nsIDOMMutationEvent::MODIFICATION;
        else
          mutation.mAttrChange = nsIDOMMutationEvent::ADDITION;
        nsEventStatus status = nsEventStatus_eIgnore;
        HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
      }

      if (aNotify) {
        nsCOMPtr<nsIAtom> tagName;
        NodeInfo()->GetNameAtom(*getter_AddRefs(tagName));
        if ((tagName.get() == nsXULAtoms::broadcaster) ||
            (tagName.get() == nsXULAtoms::command) ||
            (tagName.get() == nsXULAtoms::key))
            return rv;

        PRInt32 modHint = modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION)
                                       : PRInt32(nsIDOMMutationEvent::ADDITION);
        mDocument->AttributeChanged(this, attrns, attrName, modHint, 
                                    NS_STYLE_HINT_UNKNOWN);
      }
    }

    return rv;
}

NS_IMETHODIMP
nsXULElement::SetAttr(PRInt32 aNameSpaceID,
                      nsIAtom* aName,
                      const nsAReadableString& aValue,
                      PRBool aNotify)
{
    nsCOMPtr<nsINodeInfoManager> nimgr;

    NodeInfo()->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

    nsCOMPtr<nsINodeInfo> ni;
    nimgr->GetNodeInfo(aName, nsnull, aNameSpaceID, *getter_AddRefs(ni));

    return SetAttr(ni, aValue, aNotify);
}

NS_IMETHODIMP
nsXULElement::GetAttr(PRInt32 aNameSpaceID,
                      nsIAtom* aName,
                      nsAWritableString& aResult) const
{
    nsCOMPtr<nsIAtom> prefix;
    return GetAttr(aNameSpaceID, aName, *getter_AddRefs(prefix), aResult);
}

NS_IMETHODIMP
nsXULElement::GetAttr(PRInt32 aNameSpaceID,
                      nsIAtom* aName,
                      nsIAtom*& aPrefix,
                      nsAWritableString& aResult) const
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName) {
        return NS_ERROR_NULL_POINTER;
    }

    nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

    if (mSlots && mSlots->mAttributes) {
        PRInt32 count = Attributes()->Count();
        for (PRInt32 i = 0; i < count; i++) {
            nsXULAttribute* attr = NS_REINTERPRET_CAST(nsXULAttribute*, Attributes()->ElementAt(i));
            nsINodeInfo *ni = attr->GetNodeInfo();
            if (ni->Equals(aName, aNameSpaceID)) {
                ni->GetPrefixAtom(aPrefix);
                attr->GetValue(aResult);
                rv = aResult.Length() ? NS_CONTENT_ATTR_HAS_VALUE : NS_CONTENT_ATTR_NO_VALUE;
                break;
            }
        }
    }
    else if (mPrototype) {
        PRInt32 count = mPrototype->mNumAttributes;
        for (PRInt32 i = 0; i < count; i++) {
            nsXULPrototypeAttribute* attr = &(mPrototype->mAttributes[i]);

            nsINodeInfo *ni = attr->mNodeInfo;
            if (ni->Equals(aName, aNameSpaceID)) {
                ni->GetPrefixAtom(aPrefix);
                attr->mValue.GetValue( aResult );
                rv = aResult.Length() ? NS_CONTENT_ATTR_HAS_VALUE : NS_CONTENT_ATTR_NO_VALUE;
                break;
            }
        }
    }
    else {
        aResult.Truncate();
    }

    return rv;
}

NS_IMETHODIMP
nsXULElement::UnsetAttr(PRInt32 aNameSpaceID,
                        nsIAtom* aName, PRBool aNotify)
{
    NS_ASSERTION(nsnull != aName, "must have attribute name");
    if (nsnull == aName)
        return NS_ERROR_NULL_POINTER;

    // If we're unsetting an attribute, we actually need to do the
    // copy _first_ so that we can remove the value in the heavyweight
    // element.
    nsresult rv;
    rv = EnsureSlots();
    if (NS_FAILED(rv)) return rv;

    // It's possible that somebody has tried to 'unset' an attribute
    // on an element with _no_ attributes, in which case we'll have
    // paid the cost to make the thing heavyweight, but might still
    // not have created an 'mAttributes' in the slots. Test here, as
    // later code will dereference it...
    if (! Attributes())
        return NS_OK;

    // Check to see if the CLASS attribute is being unset.  If so, we need to
    // delete our class list.
    // XXXbe fuse common (mDocument && aNameSpaceId == kNameSpaceID_None)
    if (mDocument &&
        (aNameSpaceID == kNameSpaceID_None) &&
        (aName == nsXULAtoms::clazz)) {
        Attributes()->UpdateClassList(nsAutoString());
    }

    if (mDocument &&
        (aNameSpaceID == kNameSpaceID_None) &&
        aName == nsXULAtoms::style) {

        nsCOMPtr <nsIURI> docURL;
        mDocument->GetBaseURL(*getter_AddRefs(docURL));

        Attributes()->UpdateStyleRule(docURL, nsAutoString());
        // XXX Some kind of special document update might need to happen here.
    }

    // Need to check for the SELECTED attribute
    // being unset.  If we're a <treeitem>, <treerow>, or <treecell>, the act of
    // unsetting these attributes forces us to update our selected arrays.
    nsCOMPtr<nsIAtom> tag;
    GetTag(*getter_AddRefs(tag));
    if (aNameSpaceID == kNameSpaceID_None) {
        // See if we're a treeitem atom.
        // XXX Forgive me father, for I know exactly what I do, and I'm
        // doing it anyway.  Need to make an nsIRDFNodeList interface that
        // I can QI to for additions and removals of nodes.  For now
        // do an evil cast.
        nsCOMPtr<nsIRDFNodeList> nodeList;
        if (tag && (tag.get() == nsXULAtoms::treeitem) && (aName == nsXULAtoms::selected)) {
            nsCOMPtr<nsIDOMXULTreeElement> treeElement;
            GetParentTree(getter_AddRefs(treeElement));
            if (treeElement) {
                nsCOMPtr<nsIDOMNodeList> nodes;
                treeElement->GetSelectedItems(getter_AddRefs(nodes));
                nodeList = do_QueryInterface(nodes);
            }
        }

        if (nodeList) {
            // Remove this node from the list.
            nodeList->RemoveNode(this);
        }
    }

    // XXX Know how to remove POPUP event listeners when an attribute is unset?

    nsAutoString oldValue;

    rv = NS_OK;
    PRBool successful = PR_FALSE;
    if (Attributes()) {
        PRInt32 count = Attributes()->Count();
        PRInt32 i;
        for (i = 0; i < count; i++) {
            nsXULAttribute* attr = NS_REINTERPRET_CAST(nsXULAttribute*, Attributes()->ElementAt(i));
            if (attr->GetNodeInfo()->Equals(aName, aNameSpaceID)) {
                attr->GetValue(oldValue);

                if (HasMutationListeners(NS_STATIC_CAST(nsIStyledContent*, this), NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
                  nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, this)));
                  nsMutationEvent mutation;
                  mutation.eventStructType = NS_MUTATION_EVENT;
                  mutation.message = NS_MUTATION_ATTRMODIFIED;
                  mutation.mTarget = node;

                  nsAutoString attrName2;
                  aName->ToString(attrName2);
                  nsCOMPtr<nsIDOMAttr> attrNode;
                  GetAttributeNode(attrName2, getter_AddRefs(attrNode));
                  mutation.mRelatedNode = attrNode;

                  mutation.mAttrName = aName;
                  if (!oldValue.IsEmpty())
                    mutation.mPrevAttrValue = getter_AddRefs(NS_NewAtom(oldValue));
                  mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;
                  nsEventStatus status = nsEventStatus_eIgnore;
                  HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
                }

                Attributes()->RemoveElementAt(i);
                NS_RELEASE(attr);
                successful = PR_TRUE;
                break;
            }
        }
    }

    // XUL Only. Find out if we have a broadcast listener for this element.
    if (successful) {
        // Check to see if the OBSERVES attribute is being unset.  If so, we
        // need to remove ourselves completely.
        if (mDocument &&
            (aNameSpaceID == kNameSpaceID_None) &&
            (aName == nsXULAtoms::observes || aName == nsXULAtoms::command))
        {
            // Do a getElementById to retrieve the broadcaster.
            nsCOMPtr<nsIDOMElement> broadcaster;
            nsCOMPtr<nsIDOMXULDocument> domDoc = do_QueryInterface(mDocument);
            domDoc->GetElementById(oldValue, getter_AddRefs(broadcaster));
            if (broadcaster) {
                nsCOMPtr<nsIDOMXULElement> xulBroadcaster = do_QueryInterface(broadcaster);
                if (xulBroadcaster) {
                    xulBroadcaster->RemoveBroadcastListener(NS_LITERAL_STRING("*"), this);
                }
            }
        }

        if (BroadcastListeners()) {
            PRInt32 count = BroadcastListeners()->Count();
            for (PRInt32 i = 0; i < count; i++) {
                XULBroadcastListener* xulListener =
                    NS_REINTERPRET_CAST(XULBroadcastListener*, BroadcastListeners()->ElementAt(i));

                nsAutoString str;
                aName->ToString(str);
                if (xulListener->ObservingAttribute(str) &&
                   (aName != nsXULAtoms::id) &&
                   (aName != nsXULAtoms::persist) &&
                   (aName != nsXULAtoms::ref))
                {
                    // XXX Should have a function that knows which attributes are special.
                    // Unset the attribute in the broadcast listener.
                    nsCOMPtr<nsIDOMElement> element;
                    element = do_QueryInterface(xulListener->mListener);
                    if (element)
                        element->RemoveAttribute(str);
                }
            }
        }

        // Notify document
        if (NS_SUCCEEDED(rv) && mDocument) {
          nsCOMPtr<nsIBindingManager> bindingManager;
          mDocument->GetBindingManager(getter_AddRefs(bindingManager));
          nsCOMPtr<nsIXBLBinding> binding;
          bindingManager->GetBinding(NS_STATIC_CAST(nsIStyledContent*, this), getter_AddRefs(binding));
          if (binding)
            binding->AttributeChanged(aName, aNameSpaceID, PR_TRUE);

          if (aNotify) {
            nsCOMPtr<nsIAtom> tagName;
            NodeInfo()->GetNameAtom(*getter_AddRefs(tagName));
            if ((tagName.get() == nsXULAtoms::broadcaster) ||
                (tagName.get() == nsXULAtoms::command) ||
                (tagName.get() == nsXULAtoms::key))
                return rv;
            mDocument->AttributeChanged(NS_STATIC_CAST(nsIStyledContent*, this),
                                        aNameSpaceID, aName, nsIDOMMutationEvent::REMOVAL, 
                                        NS_STYLE_HINT_UNKNOWN);
          }
        }
    }

    // End XUL Only Code
    return rv;
}

NS_IMETHODIMP
nsXULElement::GetAttrNameAt(PRInt32 aIndex,
                            PRInt32& aNameSpaceID,
                            nsIAtom*& aName,
                            nsIAtom*& aPrefix) const
{
    if (Attributes()) {
        nsXULAttribute* attr = NS_REINTERPRET_CAST(nsXULAttribute*, Attributes()->ElementAt(aIndex));
        if (nsnull != attr) {
            attr->GetNodeInfo()->GetNamespaceID(aNameSpaceID);
            attr->GetNodeInfo()->GetNameAtom(aName);
            attr->GetNodeInfo()->GetPrefixAtom(aPrefix);
            return NS_OK;
        }
    }
    else if (mPrototype) {
        if (aIndex >= 0 && aIndex < mPrototype->mNumAttributes) {
            nsXULPrototypeAttribute* attr = &(mPrototype->mAttributes[aIndex]);

            attr->mNodeInfo->GetNamespaceID(aNameSpaceID);
            attr->mNodeInfo->GetNameAtom(aName);
            attr->mNodeInfo->GetPrefixAtom(aPrefix);
            return NS_OK;
        }
    }

    aNameSpaceID = kNameSpaceID_None;
    aName = nsnull;
    aPrefix = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
nsXULElement::GetAttrCount(PRInt32& aResult) const
{
    nsresult rv = NS_OK;
    if (Attributes()) {
        aResult = Attributes()->Count();
    }
    else if (mPrototype) {
        aResult = mPrototype->mNumAttributes;
    }
    else {
        aResult = 0;
    }

    return rv;
}


static void
rdf_Indent(FILE* out, PRInt32 aIndent)
{
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
}

NS_IMETHODIMP
nsXULElement::List(FILE* out, PRInt32 aIndent) const
{
    NS_PRECONDITION(mDocument != nsnull, "bad content");

    PRInt32 i;

    rdf_Indent(out, aIndent);
    fputs("<XUL", out);
    if (mSlots) fputs("*", out);
    fputs(" ", out);

    nsAutoString as;
    NodeInfo()->GetQualifiedName(as);
    fputs(NS_ConvertUCS2toUTF8(as).get(), out);

    fprintf(out, "@%p", this);

    PRInt32 nattrs;
    GetAttrCount(nattrs);

    for (i = 0; i < nattrs; ++i) {
        nsCOMPtr<nsIAtom> attr;
        nsCOMPtr<nsIAtom> prefix;
        PRInt32 nameSpaceID;
        GetAttrNameAt(i, nameSpaceID, *getter_AddRefs(attr), *getter_AddRefs(prefix));

        nsAutoString v;
        GetAttr(nameSpaceID, attr, v);

        fputs(" ", out);

        nsAutoString s;

        if (prefix) {
            prefix->ToString(s);

            fputs(NS_ConvertUCS2toUTF8(s).get(), out);
            fputs(":", out);
        }

        attr->ToString(s);

        fputs(NS_ConvertUCS2toUTF8(s).get(), out);
        fputs("=", out);
        fputs(NS_ConvertUCS2toUTF8(v).get(), out);
    }

    PRInt32 nchildren;
    ChildCount(nchildren);

    if (nchildren) {
        fputs("\n", out);

        for (i = 0; i < nchildren; ++i) {
            nsCOMPtr<nsIContent> child;
            ChildAt(i, *getter_AddRefs(child));

            child->List(out, aIndent + 1);
        }

        rdf_Indent(out, aIndent);
    }
    fputs(">\n", out);

    if (mDocument) {
        nsCOMPtr<nsIBindingManager> bindingManager;
        mDocument->GetBindingManager(getter_AddRefs(bindingManager));
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

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
    if (!aResult) {
        return NS_ERROR_NULL_POINTER;
    }
    PRUint32 sum = 0;
#ifdef DEBUG
    sum += (PRUint32) sizeof(*this);
#endif
    *aResult = sum;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                       nsEvent* aEvent,
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus* aEventStatus)
{
    nsresult ret = NS_OK;

    PRBool retarget = PR_FALSE;
    nsCOMPtr<nsIDOMEventTarget> oldTarget;

    nsIDOMEvent* domEvent = nsnull;
    if (NS_EVENT_FLAG_INIT & aFlags) {
        aDOMEvent = &domEvent;
        aEvent->flags = aFlags;
        aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
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
            aEvent->message == NS_DRAGDROP_GESTURE ||
            tagName == NS_LITERAL_STRING("menu") || tagName == NS_LITERAL_STRING("menuitem") || tagName == NS_LITERAL_STRING("menulist") ||
            tagName == NS_LITERAL_STRING("menubar") || tagName == NS_LITERAL_STRING("menupopup") || tagName == NS_LITERAL_STRING("key") || tagName == NS_LITERAL_STRING("keyset")) {
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
              privateEvent->SetTarget(this);
            }
            else return NS_ERROR_FAILURE;
        }
    }

    // Find out if we're anonymous.
    nsCOMPtr<nsIContent> bindingParent;
    if (*aDOMEvent) {
        (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));
        nsCOMPtr<nsIContent> content(do_QueryInterface(oldTarget));
        if (content)
            content->GetBindingParent(getter_AddRefs(bindingParent));
    }
    else
        GetBindingParent(getter_AddRefs(bindingParent));

    if (bindingParent) {
        // We're anonymous.  We may potentially need to retarget
        // our event if our parent is in a different scope.
        if (mParent) {
            nsCOMPtr<nsIContent> parentScope;
            mParent->GetBindingParent(getter_AddRefs(parentScope));
            if (parentScope != bindingParent)
                retarget = PR_TRUE;
        }
    }

    // determine the parent:
    nsCOMPtr<nsIContent> parent;
    if (mDocument) {
        nsCOMPtr<nsIBindingManager> bindingManager;
        mDocument->GetBindingManager(getter_AddRefs(bindingManager));
        if (bindingManager) {
            // we have a binding manager -- do we have an anonymous parent?
            bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
        }
    }
    if (parent) {
        retarget = PR_FALSE;
    }
    else {
        // if we didn't find an anonymous parent, use the explicit one,
        // whether it's null or not...
        parent = mParent;
    }

    if (retarget || (parent.get() != mParent)) {
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
          nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mParent);
          privateEvent->SetTarget(target);
      }
    }

    //Capturing stage evaluation
    if (NS_EVENT_FLAG_BUBBLE != aFlags) {
      //Initiate capturing phase.  Special case first call to document
      if (parent) {
        parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
      }
      else if (mDocument != nsnull) {
          ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                          NS_EVENT_FLAG_CAPTURE, aEventStatus);
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
        mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this, aFlags, aEventStatus);
        aEvent->flags &= ~aFlags;
    }

    if (retarget) {
      // The event originated beneath us, and we need to perform a retargeting.
      nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
      if (privateEvent) {
        nsCOMPtr<nsIDOMEventTarget> parentTarget(do_QueryInterface(mParent));
        privateEvent->SetTarget(parentTarget);
      }
    }

    //Bubbling stage
    if (NS_EVENT_FLAG_CAPTURE != aFlags) {
        if (parent != nsnull) {
            // We have a parent. Let them field the event.
            ret = parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                          NS_EVENT_FLAG_BUBBLE, aEventStatus);
      }
        else if (mDocument != nsnull) {
        // We must be the document root. The event should bubble to the
        // document.
        ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                        NS_EVENT_FLAG_BUBBLE, aEventStatus);
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
        // release here.
        if (nsnull != *aDOMEvent) {
            nsrefcnt rc;
            NS_RELEASE2(*aDOMEvent, rc);
            if (0 != rc) {
                // Okay, so someone in the DOM loop (a listener, JS object)
                // still has a ref to the DOM Event but the internal data
                // hasn't been malloc'd.  Force a copy of the data here so the
                // DOM Event is still valid.
                nsIPrivateDOMEvent *privateEvent;
                if (NS_OK == (*aDOMEvent)->QueryInterface(NS_GET_IID(nsIPrivateDOMEvent), (void**)&privateEvent)) {
                    privateEvent->DuplicatePrivateData();
                    NS_RELEASE(privateEvent);
                }
            }
        }
        aDOMEvent = nsnull;
    }
    return ret;
}


NS_IMETHODIMP
nsXULElement::GetContentID(PRUint32* aID)
{
    *aID = mContentId;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetContentID(PRUint32 aID)
{
    mContentId = aID;
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::RangeAdd(nsIDOMRange& aRange)
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::RangeRemove(nsIDOMRange& aRange)
{
    // rdf content does not yet support DOM ranges
    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::GetRangeList(nsVoidArray*& aResult) const
{
    // rdf content does not yet support DOM ranges
    aResult = nsnull;
    return NS_OK;
}


//----------------------------------------------------------------------
NS_IMETHODIMP
nsXULElement::AddBroadcastListener(const nsAReadableString& attr,
                                   nsIDOMElement* anElement)
{
    // Add ourselves to the array.
    nsresult rv;

    if (! BroadcastListeners()) {
        rv = EnsureSlots();
        if (NS_FAILED(rv)) return rv;

        mSlots->mBroadcastListeners = new nsVoidArray();
        if (! mSlots->mBroadcastListeners)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    BroadcastListeners()->AppendElement(new XULBroadcastListener(attr, anElement));

    // We need to sync up the initial attribute value.
    nsCOMPtr<nsIContent> listener( do_QueryInterface(anElement) );

    if (attr.Equals(NS_LITERAL_STRING("*"))) {
        // All of the attributes found on this node should be set on the
        // listener.
        if (Attributes()) {
            for (PRInt32 i = Attributes()->Count() - 1; i >= 0; --i) {
                nsXULAttribute* attr = NS_REINTERPRET_CAST(nsXULAttribute*, Attributes()->ElementAt(i));
                nsINodeInfo *ni = attr->GetNodeInfo();
                if (ni->Equals(nsXULAtoms::id, kNameSpaceID_None))
                    continue;

                // We aren't the id atom, so it's ok to set us in the listener.
                nsAutoString value;
                attr->GetValue(value);
                listener->SetAttr(ni, value, PR_TRUE);
            }
        }
    }
    else {
        // Find out if the attribute is even present at all.
        nsCOMPtr<nsIAtom> kAtom = dont_AddRef(NS_NewAtom(attr));

        nsAutoString attrValue;
        nsresult result = GetAttr(kNameSpaceID_None, kAtom, attrValue);
        PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                              result == NS_CONTENT_ATTR_HAS_VALUE);

        if (attrPresent) {
            // Set the attribute
            anElement->SetAttribute(attr, attrValue);
        }
        else {
            // Unset the attribute
            anElement->RemoveAttribute(attr);
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULElement::RemoveBroadcastListener(const nsAReadableString& attr,
                                      nsIDOMElement* anElement)
{
    if (BroadcastListeners()) {
        // Find the element.
        PRInt32 count = BroadcastListeners()->Count();
        for (PRInt32 i = 0; i < count; i++) {
            XULBroadcastListener* xulListener =
                NS_REINTERPRET_CAST(XULBroadcastListener*, BroadcastListeners()->ElementAt(i));

            if (xulListener->mListener == anElement) {
                if (xulListener->ObservingEverything() || attr.Equals(NS_LITERAL_STRING("*"))) {
                    // Do the removal.
                    BroadcastListeners()->RemoveElementAt(i);
                    delete xulListener;
                }
                else {
                    // We're observing specific attributes and removing a specific attribute
                    xulListener->RemoveAttribute(attr);
                    if (xulListener->IsEmpty()) {
                        // Do the removal.
                        BroadcastListeners()->RemoveElementAt(i);
                        delete xulListener;
                    }
                }
                break;
            }
        }
    }

    return NS_OK;
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
        rv = gRDFService->GetUnicodeResource(id.get(), aResource);
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

    if (mDocument) {
        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(mDocument);
        if (xuldoc)
            xuldoc->GetTemplateBuilderFor(NS_STATIC_CAST(nsIStyledContent*, this), aBuilder);
    }

    return NS_OK;
}


//----------------------------------------------------------------------
// Implementation methods

nsresult
nsXULElement::EnsureContentsGenerated(void) const
{
    if (mLazyState & nsIXULContent::eChildrenMustBeRebuilt) {
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
        unconstThis->mLazyState &= ~nsIXULContent::eChildrenMustBeRebuilt;

        // Walk up our ancestor chain, looking for an element with a
        // XUL content model builder attached to it.
        nsCOMPtr<nsIContent> element
            = do_QueryInterface(NS_STATIC_CAST(nsIStyledContent*, unconstThis));

        do {
            nsCOMPtr<nsIDOMXULElement> xulele = do_QueryInterface(element);
            if (xulele) {
                nsCOMPtr<nsIXULTemplateBuilder> builder;
                xulele->GetBuilder(getter_AddRefs(builder));
                if (builder) {
                    nsCOMPtr<nsIRDFContentModelBuilder> contentBuilder =
                        do_QueryInterface(builder);

                    if (contentBuilder)
                        return contentBuilder->CreateContents(NS_STATIC_CAST(nsIStyledContent*, unconstThis));
                }
            }

            nsCOMPtr<nsIContent> parent;
            element->GetParent(*getter_AddRefs(parent));

            element = parent;
        } while (element);

        NS_ERROR("lazy state set with no XUL content builder in ancestor chain");
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
}


nsresult
nsXULElement::ExecuteOnBroadcastHandler(nsIDOMElement* anElement, const nsAReadableString& attrName)
{
    // Now we execute the onchange handler in the context of the
    // observer. We need to find the observer in order to
    // execute the handler.
    nsCOMPtr<nsIContent> content(do_QueryInterface(anElement));
    PRInt32 count;
    content->ChildCount(count);
    for (PRInt32 i = 0; i < count; i++) {
        nsCOMPtr<nsIContent> child;
        content->ChildAt(i, *getter_AddRefs(child));
        nsCOMPtr<nsIAtom> tag;
        child->GetTag(*getter_AddRefs(tag));
        if (tag.get() == nsXULAtoms::observes) {
            nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(child));
            if (domElement) {
                // We have a domElement. Find out if it was listening to us.
                nsAutoString listeningToID;
                domElement->GetAttribute(NS_LITERAL_STRING("element"), listeningToID);
                nsAutoString broadcasterID;
                GetAttribute(NS_LITERAL_STRING("id"), broadcasterID);
                if (listeningToID == broadcasterID) {
                    // We are observing the broadcaster, but is this the right
                    // attribute?
                    nsAutoString listeningToAttribute;
                    domElement->GetAttribute(NS_LITERAL_STRING("attribute"), listeningToAttribute);
                    if (listeningToAttribute.Equals(attrName)) {
                        // This is the right observes node.
                        // Execute the onchange event handler
                        nsEvent event;
                        event.eventStructType = NS_EVENT;
                        event.message = NS_XUL_BROADCAST;
                        ExecuteJSCode(domElement, &event);
                    }
                }
            }
        }
    }

    return NS_OK;
}


nsresult
nsXULElement::ExecuteJSCode(nsIDOMElement* anElement, nsEvent* aEvent)
{
    // This code executes in every presentation context in which this
    // document is appearing.
    nsCOMPtr<nsIContent> content;
    content = do_QueryInterface(anElement);
    if (!content)
      return NS_OK;

    nsCOMPtr<nsIDocument> document;
    content->GetDocument(*getter_AddRefs(document));

    if (!document)
      return NS_OK;

    PRInt32 count = document->GetNumberOfShells();
    for (PRInt32 i = 0; i < count; i++) {
        nsCOMPtr<nsIPresShell> shell;
        document->GetShellAt(i, getter_AddRefs(shell));
        if (!shell)
            continue;

        // Retrieve the context in which our DOM event will fire.
        nsCOMPtr<nsIPresContext> aPresContext;
        shell->GetPresContext(getter_AddRefs(aPresContext));

        // Handle the DOM event
        nsEventStatus status = nsEventStatus_eIgnore;
        content->HandleDOMEvent(aPresContext, aEvent, nsnull, NS_EVENT_FLAG_INIT, &status);
    }

    return NS_OK;
}



nsresult
nsXULElement::GetElementsByTagName(nsIDOMNode* aNode,
                                   const nsAReadableString& aTagName,
                                   nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    nsCOMPtr<nsIDOMNodeList> children;
    if (NS_FAILED(rv = aNode->GetChildNodes( getter_AddRefs(children) ))) {
        NS_ERROR("unable to get node's children");
        return rv;
    }

    // no kids: terminate the recursion
    if (! children)
        return NS_OK;

    PRUint32 length;
    if (NS_FAILED(children->GetLength(&length))) {
        NS_ERROR("unable to get node list's length");
        return rv;
    }

    for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        if (NS_FAILED(rv = children->Item(i, getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from list");
            return rv;
        }

        nsCOMPtr<nsIDOMElement> element;
        element = do_QueryInterface(child);
        if (!element)
          continue;

        if (aTagName.Equals(NS_LITERAL_STRING("*"))) {
            if (NS_FAILED(rv = aElements->AppendNode(child))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }
        else {
            nsAutoString name;
            if (NS_FAILED(rv = child->GetNodeName(name))) {
                NS_ERROR("unable to get node name");
                return rv;
            }

            if (aTagName.Equals(name)) {
                if (NS_FAILED(rv = aElements->AppendNode(child))) {
                    NS_ERROR("unable to append element to node list");
                    return rv;
                }
            }
        }

        // Now recursively look for children
        if (NS_FAILED(rv = GetElementsByTagName(child, aTagName, aElements))) {
            NS_ERROR("unable to recursively get elements by tag name");
            return rv;
        }
    }

    return NS_OK;
}

nsresult
nsXULElement::GetElementsByAttribute(nsIDOMNode* aNode,
                                       const nsAReadableString& aAttribute,
                                       const nsAReadableString& aValue,
                                       nsRDFDOMNodeList* aElements)
{
    nsresult rv;

    nsCOMPtr<nsIDOMNodeList> children;
    if (NS_FAILED(rv = aNode->GetChildNodes( getter_AddRefs(children) ))) {
        NS_ERROR("unable to get node's children");
        return rv;
    }

    // no kids: terminate the recursion
    if (! children)
        return NS_OK;

    PRUint32 length;
    if (NS_FAILED(children->GetLength(&length))) {
        NS_ERROR("unable to get node list's length");
        return rv;
    }

    for (PRUint32 i = 0; i < length; ++i) {
        nsCOMPtr<nsIDOMNode> child;
        if (NS_FAILED(rv = children->Item(i, getter_AddRefs(child) ))) {
            NS_ERROR("unable to get child from list");
            return rv;
        }

        nsCOMPtr<nsIDOMElement> element;
        element = do_QueryInterface(child);
        if (!element)
          continue;

        nsAutoString attrValue;
        if (NS_FAILED(rv = element->GetAttribute(aAttribute, attrValue))) {
            NS_ERROR("unable to get attribute value");
            return rv;
        }

        if ((attrValue.Equals(aValue)) || (attrValue.Length() > 0 && aValue.Equals(NS_LITERAL_STRING("*")))) {
            if (NS_FAILED(rv = aElements->AppendNode(child))) {
                NS_ERROR("unable to append element to node list");
                return rv;
            }
        }

        // Now recursively look for children
        if (NS_FAILED(rv = GetElementsByAttribute(child, aAttribute, aValue, aElements))) {
            NS_ERROR("unable to recursively get elements by attribute");
            return rv;
        }
    }

    return NS_OK;
}

// nsIStyledContent Implementation
NS_IMETHODIMP
nsXULElement::GetID(nsIAtom*& aResult) const
{
    aResult = nsnull;

    if (mSlots && mSlots->mAttributes) {
        // Take advantage of the fact that the 'id' attribute will
        // already be atomized.
        PRInt32 count = mSlots->mAttributes->Count();
        for (PRInt32 i = 0; i < count; ++i) {
            nsXULAttribute* attr =
                NS_REINTERPRET_CAST(nsXULAttribute*, mSlots->mAttributes->ElementAt(i));

            if (attr->GetNodeInfo()->Equals(nsXULAtoms::id, kNameSpaceID_None)) {
                nsIAtom* result;
                attr->GetValueAsAtom(&result);
                aResult = result; // transfer refcnt
                break;
            }
        }
    }
    else if (mPrototype) {
        PRInt32 count = mPrototype->mNumAttributes;
        for (PRInt32 i = 0; i < count; i++) {
            nsXULPrototypeAttribute* attr = &(mPrototype->mAttributes[i]);
            if (attr->mNodeInfo->Equals(nsXULAtoms::id, kNameSpaceID_None)) {
                attr->mValue.GetValueAsAtom( &aResult );
                break;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetClasses(nsVoidArray& aArray) const
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (Attributes()) {
        rv = Attributes()->GetClasses(aArray);
    }
    else if (mPrototype) {
        rv = nsClassList::GetClasses(mPrototype->mClassList, aArray);
    }
    else {
        aArray.Clear();
    }
    return rv;
}

NS_IMETHODIMP
nsXULElement::HasClass(nsIAtom* aClass, PRBool /*aCaseSensitive*/) const
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    if (Attributes()) {
        rv = Attributes()->HasClass(aClass);
    }
    else if (mPrototype) {
        rv = nsClassList::HasClass(mPrototype->mClassList, aClass) ? NS_OK : NS_COMFALSE;
    }
    return rv;
}

NS_IMETHODIMP
nsXULElement::WalkContentStyleRules(nsIRuleWalker* aRuleWalker)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULElement::WalkInlineStyleRules(nsIRuleWalker* aRuleWalker)
{
    // Fetch the cached style rule from the attributes.
    nsresult result = NS_ERROR_NULL_POINTER;
    nsCOMPtr<nsIStyleRule> rule;
    if (aRuleWalker) {
        if (Attributes()) {
            result = Attributes()->GetInlineStyleRule(*getter_AddRefs(rule));
        }
        else if (mPrototype && mPrototype->mInlineStyleRule) {
            rule = mPrototype->mInlineStyleRule;
            result = NS_OK;
        }
    }

    if (rule)
        aRuleWalker->Forward(rule);
    return result;
}

NS_IMETHODIMP
nsXULElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                       PRInt32& aHint) const
{
    aHint = NS_STYLE_HINT_CONTENT;  // by default, never map attributes to style

    if (aAttribute == nsXULAtoms::value && 
        (aModType == nsIDOMMutationEvent::REMOVAL || aModType == nsIDOMMutationEvent::ADDITION)) {
      nsCOMPtr<nsIAtom> tag;
      GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsXULAtoms::label || tag.get() == nsXULAtoms::description)
        // Label and description dynamically morph between a normal block and a cropping single-line
        // XUL text frame.  If the value attribute is being added or removed, then we need to return
        // a hint of frame change.  (See bugzilla bug 95475 for details.)
        aHint = NS_STYLE_HINT_FRAMECHANGE;
      else
        aHint = NS_STYLE_HINT_ATTRCHANGE;
    }
    else if (aAttribute == nsXULAtoms::value || aAttribute == nsXULAtoms::flex ||
        aAttribute == nsXULAtoms::label) {
      // VERY IMPORTANT! This has a huge positive performance impact!
      aHint = NS_STYLE_HINT_ATTRCHANGE;
    }
    else if (aAttribute == nsXULAtoms::style) {
        // well, okay, "style=" maps to style. This is totally
        // non-optimal, because it's very likely that the frame
        // *won't* change. Oh well, you're a tool for setting the
        // "style" attribute anyway.
        aHint = NS_STYLE_HINT_FRAMECHANGE;
    }
    else if (NodeInfo()->Equals(nsXULAtoms::window) ||
             NodeInfo()->Equals(nsXULAtoms::page) ||
             NodeInfo()->Equals(nsXULAtoms::dialog) ||
             NodeInfo()->Equals(nsXULAtoms::wizard)) {
        // Ignore 'width' and 'height' on a <window>
        if (nsXULAtoms::width == aAttribute || nsXULAtoms::height == aAttribute)
            aHint = NS_STYLE_HINT_NONE;
    } else {
        // if left or top changes we reflow. This will happen in xul containers that
        // manage positioned children such as a bulletinboard.
        if (nsXULAtoms::left == aAttribute || nsXULAtoms::top == aAttribute)
            aHint = NS_STYLE_HINT_REFLOW;
    }

    return NS_OK;
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
  return nsDoc->GetBoxObjectFor(NS_STATIC_CAST(nsIDOMElement*, this), aResult);
}

// Methods for setting/getting attributes from nsIDOMXULElement
nsresult
nsXULElement::GetId(nsAWritableString& aId)
{
  GetAttribute(NS_LITERAL_STRING("id"), aId);
  return NS_OK;
}

nsresult
nsXULElement::SetId(const nsAReadableString& aId)
{
  SetAttribute(NS_LITERAL_STRING("id"), aId);
  return NS_OK;
}

nsresult
nsXULElement::GetClassName(nsAWritableString& aClassName)
{
  GetAttribute(NS_LITERAL_STRING("class"), aClassName);
  return NS_OK;
}

nsresult
nsXULElement::SetClassName(const nsAReadableString& aClassName)
{
  SetAttribute(NS_LITERAL_STRING("class"), aClassName);
  return NS_OK;
}

nsresult
nsXULElement::GetAlign(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("align"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetAlign(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("align"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetDir(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("dir"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetDir(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("dir"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetFlex(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("flex"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetFlex(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("flex"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetFlexGroup(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("flexgroup"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetFlexGroup(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("flexgroup"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetOrdinal(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("ordinal"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetOrdinal(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("ordinal"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetOrient(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("orient"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetOrient(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("orient"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetPack(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("pack"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetPack(const nsAReadableString& aAttr)
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
  if (val.EqualsIgnoreCase("true"))
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
  if (val.EqualsIgnoreCase("true"))
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
  if (val.EqualsIgnoreCase("true"))
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
nsXULElement::GetObserves(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("observes"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetObserves(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("observes"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMenu(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("menu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMenu(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("menu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetContextMenu(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("contextmenu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetContextMenu(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("contextmenu"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetTooltip(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("tooltip"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetTooltip(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("tooltip"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetWidth(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("width"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetWidth(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("width"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetHeight(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("height"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetHeight(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("height"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMinWidth(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("minwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMinWidth(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("minwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMinHeight(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("minheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMinHeight(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("minheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMaxWidth(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("maxwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMaxWidth(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("maxwidth"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetMaxHeight(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("maxheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetMaxHeight(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("maxheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetPersist(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("persist"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetPersist(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("maxheight"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetLeft(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("left"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetLeft(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("left"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetTop(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("top"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetTop(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("top"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetDatasources(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("datasources"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetDatasources(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("datasources"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetRef(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("ref"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetRef(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("ref"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetTooltipText(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("tooltiptext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetTooltipText(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("tooltiptext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetStatusText(nsAWritableString& aAttr)
{
  GetAttribute(NS_LITERAL_STRING("statustext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::SetStatusText(const nsAReadableString& aAttr)
{
  SetAttribute(NS_LITERAL_STRING("statustext"), aAttr);
  return NS_OK;
}

nsresult
nsXULElement::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXULElement::GetParentTree(nsIDOMXULTreeElement** aTreeElement)
{
  nsCOMPtr<nsIContent> current;
  GetParent(*getter_AddRefs(current));
  while (current) {
    nsCOMPtr<nsIAtom> tag;
    current->GetTag(*getter_AddRefs(tag));
    if (tag && (tag.get() == nsXULAtoms::tree)) {
      nsCOMPtr<nsIDOMXULTreeElement> element = do_QueryInterface(current);
      *aTreeElement = element;
      NS_IF_ADDREF(*aTreeElement);
      return NS_OK;
    }

    nsCOMPtr<nsIContent> parent;
    current->GetParent(*getter_AddRefs(parent));
    current = parent.get();
  }
  return NS_OK;
}

PRBool
nsXULElement::IsAncestor(nsIDOMNode* aParentNode, nsIDOMNode* aChildNode)
{
  nsCOMPtr<nsIDOMNode> parent = dont_QueryInterface(aChildNode);
  while (parent && (parent.get() != aParentNode)) {
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
  // Obtain a presentation context and then call SetFocus.
  PRInt32 count = mDocument->GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  mDocument->GetShellAt(0, getter_AddRefs(shell));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> aPresContext;
  shell->GetPresContext(getter_AddRefs(aPresContext));

  // Set focus
  return SetFocus(aPresContext);
}

NS_IMETHODIMP
nsXULElement::Blur()
{
  // Obtain a presentation context and then call SetFocus.
  PRInt32 count = mDocument->GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  mDocument->GetShellAt(0, getter_AddRefs(shell));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> aPresContext;
  shell->GetPresContext(getter_AddRefs(aPresContext));

  // Set focus
  return RemoveFocus(aPresContext);
}

NS_IMETHODIMP
nsXULElement::Click()
{
  nsCOMPtr<nsIDocument> doc; // Strong
  GetDocument(*getter_AddRefs(doc));
  if (doc) {
    PRInt32 numShells = doc->GetNumberOfShells();
    nsCOMPtr<nsIPresShell> shell; // Strong
    nsCOMPtr<nsIPresContext> context;

    for (PRInt32 i=0; i<numShells; i++) {
      doc->GetShellAt(i, getter_AddRefs(shell));
      shell->GetPresContext(getter_AddRefs(context));

	    nsEventStatus status = nsEventStatus_eIgnore;
	    nsMouseEvent event;
	    event.eventStructType = NS_GUI_EVENT;
	    event.message = NS_MOUSE_LEFT_CLICK;
      event.isShift = PR_FALSE;
      event.isControl = PR_FALSE;
      event.isAlt = PR_FALSE;
      event.isMeta = PR_FALSE;
      event.clickCount = 0;
      event.widget = nsnull;
      HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXULElement::DoCommand()
{
  nsCOMPtr<nsIDocument> doc;
  GetDocument(*getter_AddRefs(doc));
  if (doc) {
    PRInt32 numShells = doc->GetNumberOfShells();
    nsCOMPtr<nsIPresShell> shell;
    nsCOMPtr<nsIPresContext> context;

    for (PRInt32 i=0; i<numShells; i++) {
      doc->GetShellAt(i, getter_AddRefs(shell));
      shell->GetPresContext(getter_AddRefs(context));
        
      nsEventStatus status = nsEventStatus_eIgnore;
      nsMouseEvent event;
      event.eventStructType = NS_EVENT;
      event.message = NS_XUL_COMMAND;
      HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
  }

  return NS_OK;
}

// nsIFocusableContent interface and helpers

NS_IMETHODIMP
nsXULElement::SetFocus(nsIPresContext* aPresContext)
{
  nsAutoString disabled;
  GetAttribute(NS_LITERAL_STRING("disabled"), disabled);
  if (disabled == NS_LITERAL_STRING("true"))
    return NS_OK;

  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {

    esm->SetContentState((nsIStyledContent*)this, NS_EVENT_STATE_FOCUS);
    NS_RELEASE(esm);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULElement::RemoveFocus(nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXULElement::GetBindingParent(nsIContent** aContent)
{
  *aContent = mBindingParent;
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

NS_IMETHODIMP
nsXULElement::SetBindingParent(nsIContent* aParent)
{
  mBindingParent = aParent; // [Weak] no addref
  if (mBindingParent) {
    PRInt32 count;
    ChildCount(count);
    for (PRInt32 i = 0; i < count; i++) {
      nsCOMPtr<nsIContent> child;
      ChildAt(i, *getter_AddRefs(child));
      child->SetBindingParent(aParent);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsXULElement::IsContentOfType(PRUint32 aFlags)
{
  return !(aFlags & ~(eELEMENT | eXUL));
}

void nsXULElement::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  // XXX - implement this if you want the sizes of XUL style rules
  //       dumped during StyleSize dump
  return;
}


nsresult
nsXULElement::AddPopupListener(nsIAtom* aName)
{
    // Add a popup listener to the element
    nsresult rv;

    nsCOMPtr<nsIXULPopupListener> popupListener;
    rv = nsComponentManager::CreateInstance(kXULPopupListenerCID,
                                            nsnull,
                                            NS_GET_IID(nsIXULPopupListener),
                                            getter_AddRefs(popupListener));
    NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to create an instance of the popup listener object.");
    if (NS_FAILED(rv)) return rv;

    XULPopupType popupType;
    if (aName == nsXULAtoms::tooltip) {
        popupType = eXULPopupType_tooltip;
    }
    else if (aName == nsXULAtoms::context || aName == nsXULAtoms::contextmenu) {
        popupType = eXULPopupType_context;
    }
    else {
        popupType = eXULPopupType_popup;
    }

    // Add a weak reference to the node.
    popupListener->Init(this, popupType);

    // Add the popup as a listener on this element.
    nsCOMPtr<nsIDOMEventListener> eventListener = do_QueryInterface(popupListener);

    if (popupType == eXULPopupType_tooltip) {
        AddEventListener(NS_LITERAL_STRING("mouseout"), eventListener, PR_FALSE);
        AddEventListener(NS_LITERAL_STRING("mousemove"), eventListener, PR_FALSE);
        AddEventListener(NS_LITERAL_STRING("keydown"), eventListener, PR_FALSE);
    }
    else {
        AddEventListener(NS_LITERAL_STRING("mousedown"), eventListener, PR_FALSE);
        AddEventListener(NS_LITERAL_STRING("contextmenu"), eventListener, PR_FALSE);
    }

    return NS_OK;
}

//*****************************************************************************
// nsXULElement::nsIChromeEventHandler
//*****************************************************************************

NS_IMETHODIMP nsXULElement::HandleChromeEvent(nsIPresContext* aPresContext,
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
    // Ensure that the 'mSlots' field is valid. This makes the
    // nsXULElement 'heavyweight'.
    if (mSlots)
        return NS_OK;

    mSlots = new Slots(this);
    if (! mSlots)
        return NS_ERROR_OUT_OF_MEMORY;

    // Copy information from the prototype, if there is one.
    if (! mPrototype)
        return NS_OK;

    nsXULPrototypeElement* proto = mPrototype;
    mPrototype = nsnull;

    mSlots->mNameSpace       = proto->mNameSpace;
    mSlots->mNodeInfo        = proto->mNodeInfo;

    // Copy the attributes, if necessary. Arguably, we are over-eager
    // about copying attributes. But eagerly copying the attributes
    // vastly simplifies the "lookup" and "set" logic, which otherwise
    // would need to do some pretty tricky default logic.
    if (proto->mNumAttributes == 0)
        return NS_OK;

    nsresult rv;
    rv = nsXULAttributes::Create(NS_STATIC_CAST(nsIStyledContent*, this), &(mSlots->mAttributes));
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < proto->mNumAttributes; ++i) {
        nsXULPrototypeAttribute* protoattr = &(proto->mAttributes[i]);
        nsAutoString   valueStr;
        protoattr->mValue.GetValue( valueStr );

        // Create a CBufDescriptor to avoid copying the attribute's
        // value just to set it.
        nsXULAttribute* attr;
        rv = nsXULAttribute::Create(NS_STATIC_CAST(nsIStyledContent*, this),
                                    protoattr->mNodeInfo,
                                    valueStr,
                                    &attr);

        if (NS_FAILED(rv)) return rv;

        // transfer ownership of the nsXULAttribute object
        mSlots->mAttributes->AppendElement(attr);
    }

    mSlots->mAttributes->SetClassList(proto->mClassList);
    mSlots->mAttributes->SetInlineStyleRule(proto->mInlineStyleRule);

    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsXULElement::Slots
//

nsXULElement::Slots::Slots(nsXULElement* aElement)
    : mElement(aElement),
      mBroadcastListeners(nsnull),
      mBroadcaster(nsnull),
      mAttributes(nsnull),
      mInnerXULElement(nsnull)
{
    MOZ_COUNT_CTOR(nsXULElement::Slots);
}


nsXULElement::Slots::~Slots()
{
    MOZ_COUNT_DTOR(nsXULElement::Slots);

    NS_IF_RELEASE(mAttributes);

    // Release our broadcast listeners
    if (mBroadcastListeners) {
        PRInt32 count = mBroadcastListeners->Count();
        for (PRInt32 i = 0; i < count; i++) {
            XULBroadcastListener* xulListener =
                NS_REINTERPRET_CAST(XULBroadcastListener*, mBroadcastListeners->ElementAt(0));

            mElement->RemoveBroadcastListener(NS_LITERAL_STRING("*"), xulListener->mListener);
        }

        delete mBroadcastListeners;
    }

    // Delete the aggregated interface, if one exists.
    delete mInnerXULElement;
}


//----------------------------------------------------------------------
//
// nsXULPrototypeAttribute
//

nsXULPrototypeAttribute::~nsXULPrototypeAttribute()
{
    if (mEventHandler)
        RemoveJSGCRoot(&mEventHandler);
}


//----------------------------------------------------------------------
//
// nsXULPrototypeElement
//

nsresult
nsXULPrototypeElement::Serialize(nsIObjectOutputStream* aStream,
                                 nsIScriptContext* aContext)
{
    nsresult rv;

#if 0
    // XXXbe partial deserializer is not ready for this yet
    rv = aStream->Write32(PRUint32(mNumChildren));
    if (NS_FAILED(rv)) return rv;
#endif

    // XXXbe check for failure once all elements have been taught to serialize
    for (PRInt32 i = 0; i < mNumChildren; i++) {
        rv = mChildren[i]->Serialize(aStream, aContext);
        NS_ASSERTION(NS_SUCCEEDED(rv) || rv == NS_ERROR_NOT_IMPLEMENTED,
                     "can't serialize!");
    }
    return NS_OK;
}

nsresult
nsXULPrototypeElement::Deserialize(nsIObjectInputStream* aStream,
                                   nsIScriptContext* aContext)
{
    nsresult rv;

    PRUint32 numChildren;
    rv = aStream->Read32(&numChildren);
    if (NS_FAILED(rv)) return rv;

    // XXXbe temporary until we stop parsing tags when deserializing
    NS_ASSERTION(PRInt32(numChildren) == mNumChildren, "XUL/FastLoad mismatch");

    // XXXbe check for failure once all elements have been taught to serialize
    for (PRInt32 i = 0; i < mNumChildren; i++)
        (void) mChildren[i]->Deserialize(aStream, aContext);
    return NS_OK;
}


nsresult
nsXULPrototypeElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsAWritableString& aValue)
{
    for (PRInt32 i = 0; i < mNumAttributes; ++i) {
        if (mAttributes[i].mNodeInfo->Equals(aName, aNameSpaceID)) {
            mAttributes[i].mValue.GetValue( aValue );
            return aValue.Length() ? NS_CONTENT_ATTR_HAS_VALUE : NS_CONTENT_ATTR_NO_VALUE;
        }

    }
    return NS_CONTENT_ATTR_NOT_THERE;
}


//----------------------------------------------------------------------
//
// nsXULPrototypeScript
//

nsXULPrototypeScript::nsXULPrototypeScript(PRInt32 aLineNo, const char *aVersion)
    : nsXULPrototypeNode(eType_Script, aLineNo),
      mSrcLoading(PR_FALSE),
      mSrcLoadWaiters(nsnull),
      mJSObject(nsnull),
      mLangVersion(aVersion)
{
    MOZ_COUNT_CTOR(nsXULPrototypeScript);
    AddJSGCRoot(&mJSObject, "nsXULPrototypeScript::mJSObject");
}


nsXULPrototypeScript::~nsXULPrototypeScript()
{
    RemoveJSGCRoot(&mJSObject);
    MOZ_COUNT_DTOR(nsXULPrototypeScript);
}


nsresult
nsXULPrototypeScript::Serialize(nsIObjectOutputStream* aStream,
                                nsIScriptContext* aContext)
{
    nsresult rv;

    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nsnull || !mJSObject,
                 "script source still loading when serializing?!");
    
#if 0
    // XXXbe redundant while we're still parsing XUL instead of deserializing it
    // XXXbe also, we should serialize mType and mLineNo first.
    rv = NS_WriteOptionalCompoundObject(aStream, mSrcURI, NS_GET_IID(nsIURI),
                                        PR_TRUE);
    if (NS_FAILED(rv)) return rv;
#endif

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
nsXULPrototypeScript::Deserialize(nsIObjectInputStream* aStream,
                                  nsIScriptContext* aContext)
{
    nsresult rv;

    NS_ASSERTION(!mSrcLoading || mSrcLoadWaiters != nsnull || !mJSObject,
                 "prototype script not well-initialized when deserializing?!");
    
#if 0
    // XXXbe redundant while we're still parsing XUL instead of deserializing it
    // XXXbe also, we should deserialize mType and mLineNo first.
    rv = NS_ReadOptionalObject(aStream, PR_TRUE, getter_AddRefs(mSrcURI));
    if (NS_FAILED(rv)) return rv;
#endif

    PRUint32 size;
    rv = aStream->Read32(&size);
    if (NS_FAILED(rv)) return rv;

    char* data;
    rv = aStream->ReadBytes(&data, size);
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

// XXXbe temporary, goes away when we serialize entire XUL prototype document
#include "nsIFastLoadService.h"

nsresult
nsXULPrototypeScript::Compile(const PRUnichar* aText,
                              PRInt32 aTextLength,
                              nsIURI* aURI,
                              PRInt32 aLineNo,
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
    // document because it keeps the prototype cache
    // alive. Circularity!
    nsresult rv;

    // Use the prototype document's special context
    nsCOMPtr<nsIScriptContext> context;

    // Use the prototype script's special scope object
    JSObject* scopeObject;

    {
        nsCOMPtr<nsIScriptGlobalObject> global;
        nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner
          = do_QueryInterface(aPrototypeDocument);
        globalOwner->GetScriptGlobalObject(getter_AddRefs(global));
        NS_ASSERTION(global != nsnull, "prototype doc has no script global");
        if (! global)
            return NS_ERROR_UNEXPECTED;

        rv = global->GetContext(getter_AddRefs(context));
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(context != nsnull, "no context for script global");
        if (! context)
            return NS_ERROR_UNEXPECTED;

        scopeObject = global->GetGlobalJSObject();
        if (!scopeObject)
            return NS_ERROR_UNEXPECTED;
    }

    // Use the enclosing document's principal
    // XXX is this right? or should we use the protodoc's?
    nsCOMPtr<nsIPrincipal> principal;
    rv = aDocument->GetPrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv))
        return rv;

    nsXPIDLCString urlspec;
    aURI->GetSpec(getter_Copies(urlspec));

    // Ok, compile it to create a prototype script object!
    rv = context->CompileScript(aText,
                                aTextLength,
                                scopeObject,
                                principal,
                                urlspec,
                                PRUint32(aLineNo),
                                mLangVersion,
                                (void**)&mJSObject);

    if (NS_FAILED(rv)) return rv;

    if (mJSObject) {
        // Root the compiled prototype script object.
        JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                            context->GetNativeContext());
        if (!cx)
            return NS_ERROR_UNEXPECTED;

        // XXXbe temporary, until we serialize/deserialize everything from the
        //       nsXULPrototypeDocument on down...
        nsCOMPtr<nsIFastLoadService> fastLoadService(do_GetFastLoadService());
        nsCOMPtr<nsIObjectOutputStream> objectOutput;
        fastLoadService->GetOutputStream(getter_AddRefs(objectOutput));
        if (objectOutput) {
            rv = Serialize(objectOutput, context);
            if (NS_FAILED(rv))
                nsXULDocument::AbortFastLoads();
        }
    }

    return NS_OK;

}

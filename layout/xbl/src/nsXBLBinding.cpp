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
#include "nsXBLEventHandler.h"
#include "nsXBLBinding.h"

// Static IIDs/CIDs. Try to minimize these.
static char kNameSpaceSeparator = ':';

// Helper classes
// {A2892B81-CED9-11d3-97FB-00400553EEF0}
#define NS_IXBLATTR_IID \
{ 0xa2892b81, 0xced9, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIXBLAttributeEntry : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLATTR_IID; return iid; }

  NS_IMETHOD GetAttribute(nsIAtom** aResult) = 0;
  NS_IMETHOD GetElement(nsIContent** aResult) = 0;
  NS_IMETHOD GetNext(nsIXBLAttributeEntry** aResult) = 0;
  NS_IMETHOD SetNext(nsIXBLAttributeEntry* aEntry) = 0;
};
  

class nsXBLAttributeEntry : public nsIXBLAttributeEntry {
public:
  NS_IMETHOD GetAttribute(nsIAtom** aResult) { *aResult = mAttribute; NS_IF_ADDREF(*aResult); return NS_OK; };
  NS_IMETHOD GetElement(nsIContent** aResult) { *aResult = mElement; NS_IF_ADDREF(*aResult); return NS_OK; };

  NS_IMETHOD GetNext(nsIXBLAttributeEntry** aResult) { NS_IF_ADDREF(*aResult = mNext); return NS_OK; }
  NS_IMETHOD SetNext(nsIXBLAttributeEntry* aEntry) { mNext = aEntry; return NS_OK; }

  nsIContent* mElement;
  nsCOMPtr<nsIAtom> mAttribute;
  nsCOMPtr<nsIXBLAttributeEntry> mNext;

  static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
    return aAllocator.Alloc(aSize);
  }

  static void operator delete(void* aPtr, size_t aSize) {
    nsFixedSizeAllocator::Free(aPtr, aSize);
  }

  nsXBLAttributeEntry(nsIAtom* aAtom, nsIContent* aContent) {
    NS_INIT_REFCNT(); mAttribute = aAtom; mElement = aContent;
  };

  virtual ~nsXBLAttributeEntry() {};

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS1(nsXBLAttributeEntry, nsIXBLAttributeEntry)

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
 
nsIAtom* nsXBLBinding::kContentAtom = nsnull;
nsIAtom* nsXBLBinding::kImplementationAtom = nsnull;
nsIAtom* nsXBLBinding::kHandlersAtom = nsnull;
nsIAtom* nsXBLBinding::kExcludesAtom = nsnull;
nsIAtom* nsXBLBinding::kIncludesAtom = nsnull;
nsIAtom* nsXBLBinding::kInheritsAtom = nsnull;
nsIAtom* nsXBLBinding::kEventAtom = nsnull;
nsIAtom* nsXBLBinding::kPhaseAtom = nsnull;
nsIAtom* nsXBLBinding::kExtendsAtom = nsnull;
nsIAtom* nsXBLBinding::kChildrenAtom = nsnull;
nsIAtom* nsXBLBinding::kValueAtom = nsnull;
nsIAtom* nsXBLBinding::kActionAtom = nsnull;
nsIAtom* nsXBLBinding::kHTMLAtom = nsnull;
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
nsIAtom* nsXBLBinding::kBindingAttachedAtom = nsnull;
nsIAtom* nsXBLBinding::kInheritStyleAtom = nsnull;

nsIXBLService* nsXBLBinding::gXBLService = nsnull;

nsFixedSizeAllocator nsXBLBinding::kPool;

static const size_t kBucketSizes[] = {
  sizeof(nsXBLAttributeEntry)
};

static const PRInt32 kNumBuckets = sizeof(kBucketSizes)/sizeof(size_t);
static const PRInt32 kNumElements = 128;
static const PRInt32 kInitialSize = (NS_SIZE_IN_HEAP(sizeof(nsXBLAttributeEntry))) * kNumElements;

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
nsXBLBinding::nsXBLBinding(const nsCString& aDocURI, const nsCString& aID)
: mDocURI(aDocURI), mID(aID), mFirstHandler(nsnull),
  mIsStyleBinding(PR_TRUE),
  mAllowScripts(PR_TRUE),
  mInheritStyle(PR_TRUE),
  mMarkedForDeath(PR_FALSE),
  mAttributeTable(nsnull),
  mInsertionPointTable(nsnull)
{
  NS_INIT_REFCNT();
  gRefCnt++;
  //  printf("REF COUNT UP: %d %s\n", gRefCnt, (const char*)mID);

  if (gRefCnt == 1) {
    kPool.Init("XBL Attribute Entries", kBucketSizes, kNumBuckets, kInitialSize);

    kContentAtom = NS_NewAtom("content");
    kImplementationAtom = NS_NewAtom("implementation");
    kHandlersAtom = NS_NewAtom("handlers");
    kExcludesAtom = NS_NewAtom("excludes");
    kIncludesAtom = NS_NewAtom("includes");
    kInheritsAtom = NS_NewAtom("inherits");
    kEventAtom = NS_NewAtom("event");
    kPhaseAtom = NS_NewAtom("phase");
    kExtendsAtom = NS_NewAtom("extends");
    kChildrenAtom = NS_NewAtom("children");
    kHTMLAtom = NS_NewAtom("html");
    kValueAtom = NS_NewAtom("value");
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
    kBindingAttachedAtom = NS_NewAtom("bindingattached");
    kInheritStyleAtom = NS_NewAtom("inheritstyle");

    nsServiceManager::GetService("component://netscape/xbl",
                                   NS_GET_IID(nsIXBLService),
                                   (nsISupports**) &gXBLService);
    
    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
      entry->mAttributeAtom = NS_NewAtom(entry->mAttributeName);
      ++entry;
    }
  }
}


nsXBLBinding::~nsXBLBinding(void)
{
  delete mAttributeTable;
  delete mInsertionPointTable;

  gRefCnt--;
  //  printf("REF COUNT DOWN: %d %s\n", gRefCnt, (const char*)mID);

  if (gRefCnt == 0) {
    NS_RELEASE(kContentAtom);
    NS_RELEASE(kImplementationAtom);
    NS_RELEASE(kHandlersAtom);
    NS_RELEASE(kExcludesAtom);
    NS_RELEASE(kIncludesAtom);
    NS_RELEASE(kInheritsAtom);
    NS_RELEASE(kEventAtom);
    NS_RELEASE(kPhaseAtom);
    NS_RELEASE(kExtendsAtom);
    NS_RELEASE(kChildrenAtom);
    NS_RELEASE(kHTMLAtom);
    NS_RELEASE(kValueAtom);
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
    NS_RELEASE(kBindingAttachedAtom);
    NS_RELEASE(kInheritStyleAtom);

    nsServiceManager::ReleaseService("component://netscape/xbl", gXBLService);
    gXBLService = nsnull;

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

  // (3) We need to insert entries into our attribute table for any elements
  // that are inheriting attributes.  This table allows us to quickly determine 
  // which elements in our anonymous content need to be updated when attributes change.
  ConstructAttributeTable(aParent);
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBindingElement(nsIContent** aResult)
{
  *aResult = mBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBindingElement(nsIContent* aElement)
{
  mBinding = aElement;
  nsAutoString inheritStyle;
  mBinding->GetAttribute(kNameSpaceID_None, kInheritStyleAtom, inheritStyle);
  if (inheritStyle == NS_LITERAL_STRING("false"))
    mInheritStyle = PR_FALSE;

  return NS_OK;
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

  // Plan to build the content by default.
  PRBool buildContent = PR_TRUE;

  // See if there's an excludes attribute.
  nsAutoString excludes;
  content->GetAttribute(kNameSpaceID_None, kExcludesAtom, excludes);
  if ( excludes != NS_LITERAL_STRING("*")) {
    PRInt32 childCount;
    aBoundElement->ChildCount(childCount);
    if (childCount > 0) {
      // We'll only build content if all the explicit children are 
      // in the excludes list.
    
      if (!excludes.IsEmpty()) {
        // Walk the children and ensure that all of them
        // are in the excludes array.
        for (PRInt32 i = 0; i < childCount; i++) {
          nsCOMPtr<nsIContent> child;
          aBoundElement->ChildAt(i, *getter_AddRefs(child));
          nsCOMPtr<nsIAtom> tag;
          child->GetTag(*getter_AddRefs(tag));
          if (!IsInExcludesList(tag, excludes)) {
            buildContent = PR_FALSE;
            break;
          }
        }
      }
      else buildContent = PR_FALSE;
    }
  }
  
  nsCOMPtr<nsIContent> childrenElement;
  // see if we have a <children/> element
  GetNestedChild(kChildrenAtom, content, getter_AddRefs(childrenElement));
  if (childrenElement)
    buildContent = PR_TRUE;

  if (buildContent) {
     // Always check the content element for potential attributes.
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
    nsCOMPtr<nsIDOMNamedNodeMap> namedMap;

    node->GetAttributes(getter_AddRefs(namedMap));
    PRUint32 length;
    namedMap->GetLength(&length);

    nsCOMPtr<nsIDOMNode> attribute;
    for (PRUint32 i = 0; i < length; ++i)
    {
      namedMap->Item(i, getter_AddRefs(attribute));
      nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(attribute));
      nsAutoString name;
      attr->GetName(name);
      if (name  != NS_LITERAL_STRING("excludes")) {
        nsAutoString value;
        nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mBoundElement));
        element->GetAttribute(name, value);
        if (value.IsEmpty()) {
          nsAutoString value2;
          attr->GetValue(value2);
          nsCOMPtr<nsIAtom> atom = getter_AddRefs(NS_NewAtom(name));
          mBoundElement->SetAttribute(kNameSpaceID_None, atom, value2, PR_FALSE);
        }
      }
    }
  
    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(content);

    nsCOMPtr<nsIDOMNode> clonedNode;
    domElement->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));
    
    nsCOMPtr<nsIContent> clonedContent = do_QueryInterface(clonedNode);
    SetAnonymousContent(clonedContent);

    if (childrenElement)
      BuildInsertionTable();
  }
  
  /* XXX Handle selective decision to build anonymous content.
  if (mNextBinding) {
    return mNextBinding->GenerateAnonymousContent(aBoundElement);
  }
  */

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallEventHandlers(nsIContent* aBoundElement, nsIXBLBinding** aBinding)
{
  // Don't install handlers if scripts aren't allowed.
  if (!AllowScripts())
    return NS_OK;

  // Fetch the handlers prototypes for this binding.
  nsCOMPtr<nsIXBLDocumentInfo> info;
  gXBLService->GetXBLDocumentInfo(mDocURI, mBoundElement, getter_AddRefs(info));
  if (!info)
    return NS_OK;

  nsCOMPtr<nsIXBLPrototypeHandler> handlerChain;
  info->GetPrototypeHandler(mID, getter_AddRefs(handlerChain));
  if (!handlerChain)
    return NS_OK;

  nsCOMPtr<nsIXBLPrototypeHandler> curr = handlerChain;
  nsXBLEventHandler* currHandler = nsnull;

  while (curr) {
    nsCOMPtr<nsIContent> child;
    curr->GetHandlerElement(getter_AddRefs(child));

    // Fetch the type attribute.
    // XXX Deal with a comma-separated list of types
    nsAutoString type;
    child->GetAttribute(kNameSpaceID_None, kEventAtom, type);
  
    if (!type.IsEmpty()) {
      nsIID iid;
      PRBool found = PR_FALSE;
      PRBool special = PR_FALSE;
      nsCOMPtr<nsIAtom> eventAtom = getter_AddRefs(NS_NewAtom(type));
      if (eventAtom.get() == kBindingAttachedAtom) {
        *aBinding = this;
        NS_ADDREF(*aBinding);
        special = PR_TRUE;
      }
      else
        GetEventHandlerIID(eventAtom, &iid, &found);

      if (found || special) {
        // Add an event listener for mouse and key events only.
        PRBool mouse  = IsMouseHandler(type);
        PRBool key    = IsKeyHandler(type);
        PRBool focus  = IsFocusHandler(type);
        PRBool xul    = IsXULHandler(type);
        PRBool scroll = IsScrollHandler(type);
        
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

        if (mouse || key || focus || xul || scroll || special) {
          // Create a new nsXBLEventHandler.
          nsXBLEventHandler* handler;
          NS_NewXBLEventHandler(receiver, curr, type, &handler);

          // We chain all our event handlers together for easy
          // removal later (if/when the binding dies).
          if (!currHandler)
            mFirstHandler = handler;
          else 
            currHandler->SetNextHandler(handler);

          currHandler = handler;

          // Figure out if we're using capturing or not.
          PRBool useCapture = PR_FALSE;
          nsAutoString capturer;
          child->GetAttribute(kNameSpaceID_None, kPhaseAtom, capturer);
          if (capturer == NS_LITERAL_STRING("capturing"))
            useCapture = PR_TRUE;

          // Add the event listener.
          if (mouse)
            receiver->AddEventListener(type, (nsIDOMMouseListener*)handler, useCapture);
          else if(key)
            receiver->AddEventListener(type, (nsIDOMKeyListener*)handler, useCapture);
          else if(focus)
            receiver->AddEventListener(type, (nsIDOMFocusListener*)handler, useCapture);
          else if (xul)
            receiver->AddEventListener(type, (nsIDOMScrollListener*)handler, useCapture);
          else if (scroll)
            receiver->AddEventListener(type, (nsIDOMScrollListener*)handler, useCapture);

          if (!special) // Let the listener manager hold on to the handler.
            NS_RELEASE(handler);
        }
        else {
          // Call AddScriptEventListener for other IID types
          // XXX Want this to all go away!
          NS_WARNING("***** Non-compliant XBL event listener attached! *****");
          nsAutoString value;
          child->GetAttribute(kNameSpaceID_None, kActionAtom, value);
          if (value.IsEmpty())
              GetTextData(child, value);

          AddScriptEventListener(mBoundElement, eventAtom, value, iid);
        }
      }
    }

    nsCOMPtr<nsIXBLPrototypeHandler> next;
    curr->GetNextHandler(getter_AddRefs(next));
    curr = next;
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

  // XXX Cache the value as a "base" attribute so that we don't do this
  // check over and over each time the bound element occurs.

  // We are the base binding. Obtain the extends attribute.
  nsAutoString extends;
  mBinding->GetAttribute(kNameSpaceID_None, kExtendsAtom, extends);

  if (!extends.IsEmpty()) {
    // Obtain the namespace prefix.
    nsAutoString prefix;
    PRInt32 offset = extends.FindChar(kNameSpaceSeparator);
    if (-1 != offset) {
      extends.Left(prefix, offset);
      extends.Cut(0, offset+1);
    }
    if (prefix.Length() > 0) {
      // Look up the prefix.
      nsCOMPtr<nsIAtom> prefixAtom = getter_AddRefs(NS_NewAtom(prefix));
      nsCOMPtr<nsINameSpace> nameSpace;
      nsCOMPtr<nsIXMLContent> xmlContent(do_QueryInterface(mBinding));
      if (xmlContent) {
        xmlContent->GetContainingNameSpace(*getter_AddRefs(nameSpace));

        if (nameSpace) {
          nsCOMPtr<nsINameSpace> tagSpace;
          nameSpace->FindNameSpace(prefixAtom, *getter_AddRefs(tagSpace));
          if (tagSpace) {
            // Score! Return the tag.
            tagSpace->GetNameSpaceID(*aNameSpaceID);
            *aResult = NS_NewAtom(extends); // The addref happens here
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag)
{
// XXX check to see if we inherit anonymous content from a base binding 
// if (mNextBinding)
//    mNextBinding->AttributeChanged(aAttribute, aNameSpaceID, aRemoveFlag);

  if (!mAttributeTable)
    return NS_OK;

  nsISupportsKey key(aAttribute);
  nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, 
                                                                 mAttributeTable->Get(&key)));

  nsCOMPtr<nsIXBLAttributeEntry> xblAttr = do_QueryInterface(supports);
  if (!xblAttr)
    return NS_OK;

  // Iterate over the elements in the array.

  while (xblAttr) {
    nsCOMPtr<nsIContent> element;
    nsCOMPtr<nsIAtom> setAttr;
    xblAttr->GetElement(getter_AddRefs(element));
    xblAttr->GetAttribute(getter_AddRefs(setAttr));

    if (aRemoveFlag)
      element->UnsetAttribute(aNameSpaceID, setAttr, PR_TRUE);
    else {
      nsAutoString value;
      nsresult result = mBoundElement->GetAttribute(aNameSpaceID, aAttribute, value);
      PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                            result == NS_CONTENT_ATTR_HAS_VALUE);

      if (attrPresent)
        element->SetAttribute(aNameSpaceID, setAttr, value, PR_TRUE);
    }

    // See if we're the <html> tag in XUL, and see if value is being
    // set or unset on us.
    nsCOMPtr<nsIAtom> tag;
    element->GetTag(*getter_AddRefs(tag));
    if ((tag.get() == kHTMLAtom) && (setAttr.get() == kValueAtom)) {
      // Flush out all our kids.
      PRInt32 childCount;
      element->ChildCount(childCount);
      for (PRInt32 i = 0; i < childCount; i++)
        element->RemoveChildAt(0, PR_TRUE);
      
      if (!aRemoveFlag) {
        // Construct a new text node and insert it.
        nsAutoString value;
        mBoundElement->GetAttribute(aNameSpaceID, aAttribute, value);
        if (!value.IsEmpty()) {
          nsCOMPtr<nsIDOMText> textNode;
          nsCOMPtr<nsIDocument> doc;
          mBoundElement->GetDocument(*getter_AddRefs(doc));
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
          domDoc->CreateTextNode(value, getter_AddRefs(textNode));
          nsCOMPtr<nsIDOMNode> dummy;
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(element));
          domElement->AppendChild(textNode, getter_AddRefs(dummy));
        }
      }
    }
    nsCOMPtr<nsIXBLAttributeEntry> tmpAttr = xblAttr;
    tmpAttr->GetNext(getter_AddRefs(xblAttr));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::ExecuteAttachedHandler()
{
  if (mNextBinding)
    mNextBinding->ExecuteAttachedHandler();

  if (mFirstHandler)
    mFirstHandler->BindingAttached();

  return NS_OK;
}

NS_IMETHODIMP 
nsXBLBinding::ExecuteDetachedHandler()
{
  if (mFirstHandler)
    mFirstHandler->BindingDetached();

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
        anonymous->SetDocument(nsnull, PR_TRUE, AllowScripts()); // Kill it.
      else anonymous->SetDocument(aNewDocument, PR_TRUE, AllowScripts()); // Keep it around.
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXBLBinding::GetBindingURI(nsCString& aResult)
{
  aResult = mDocURI;
  aResult += "#";
  aResult += mID;
  return NS_OK;
}

NS_IMETHODIMP 
nsXBLBinding::GetDocURI(nsCString& aResult)
{
  aResult = mDocURI;
  return NS_OK;
}

NS_IMETHODIMP 
nsXBLBinding::GetID(nsCString& aResult)
{
  aResult = mID;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InheritsStyle(PRBool* aResult)
{
  if (mContent)
    *aResult = mInheritStyle;
  else if (mNextBinding)
    return mNextBinding->InheritsStyle(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::WalkRules(nsISupportsArrayEnumFunc aFunc, void* aData)
{
  if (mContent) {
    nsCOMPtr<nsIXBLDocumentInfo> info;
    gXBLService->GetXBLDocumentInfo(mDocURI, mBoundElement, getter_AddRefs(info));
    if (!info)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsISupportsArray> rules;
    info->GetRuleProcessors(getter_AddRefs(rules));
    if (rules)
      rules->EnumerateForwards(aFunc, aData);
  }
  else if (mNextBinding)
    return mNextBinding->WalkRules(aFunc, aData);

  return NS_OK;
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
  *aResult = nsnull;
  PRInt32 childCount;
  mBinding->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    mBinding->ChildAt(i, *getter_AddRefs(child));
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

void
nsXBLBinding::GetNestedChild(nsIAtom* aTag, nsIContent* aContent, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  aContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = aContent; // We return the parent of the correct child.
      NS_ADDREF(*aResult);
      return;
    }
    else {
      GetNestedChild(aTag, child, aResult);
      if (*aResult)
        return;
    }
  }
}

void
nsXBLBinding::GetNestedChildren(nsIAtom* aTag, nsIContent* aContent, nsISupportsArray* aList)
{
  PRInt32 childCount;
  aContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) 
      aList->AppendElement(child);
    else
      GetNestedChildren(aTag, child, aList);
  }
}

void 
nsXBLBinding::BuildInsertionTable()
{
  if (!mInsertionPointTable) 
    mInsertionPointTable = new nsSupportsHashtable;
  
  nsCOMPtr<nsISupportsArray> childrenElements;
  NS_NewISupportsArray(getter_AddRefs(childrenElements));
  GetNestedChildren(kChildrenAtom, mContent, childrenElements);

  PRUint32 count;
  childrenElements->Count(&count);
  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> supp;
    childrenElements->GetElementAt(i, getter_AddRefs(supp));
    nsCOMPtr<nsIContent> child(do_QueryInterface(supp));
    if (child) {
      nsCOMPtr<nsIContent> parent; 
      child->GetParent(*getter_AddRefs(parent));
      nsAutoString includes;
      child->GetAttribute(kNameSpaceID_None, kIncludesAtom, includes);
      if (includes.IsEmpty()) {
        nsISupportsKey key(kChildrenAtom);
        mInsertionPointTable->Put(&key, parent);
      }
      else {
        // The user specified at least one attribute.
        char* str = includes.ToNewCString();
        char* newStr;
        // XXX We should use a strtok function that tokenizes PRUnichar's
        // so that we don't have to convert from Unicode to ASCII and then back

        char* token = nsCRT::strtok( str, ", ", &newStr );
        while( token != NULL ) {
          // Build an atom out of this string.
          nsCOMPtr<nsIAtom> atom;
            
          nsAutoString tok; tok.AssignWithConversion(token);
          atom = getter_AddRefs(NS_NewAtom(tok.GetUnicode()));
           
          nsISupportsKey key(atom);
          mInsertionPointTable->Put(&key, parent);
          
          token = nsCRT::strtok( newStr, ", ", &newStr );
        }

        nsMemory::Free(str);
      }
    }
  }

  // Now remove the <children> elements.
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> supp;
    childrenElements->GetElementAt(i, getter_AddRefs(supp));
    nsCOMPtr<nsIContent> child(do_QueryInterface(supp));
    if (child) {
      nsCOMPtr<nsIContent> parent; 
      child->GetParent(*getter_AddRefs(parent));
      PRInt32 index;
      parent->IndexOf(child, index);
      parent->RemoveChildAt(index, PR_FALSE);
    }
  }
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
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  if (indx + element.Length() < aList.Length()) {
    PRUnichar ch = aList[indx + element.Length()];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLBinding::ConstructAttributeTable(nsIContent* aElement)
{
  nsAutoString inherits;
  aElement->GetAttribute(kNameSpaceID_None, kInheritsAtom, inherits);
  if (!inherits.IsEmpty()) {
    if (!mAttributeTable) {
      mAttributeTable = new nsSupportsHashtable(4);
    }

    // The user specified at least one attribute.
    char* str = inherits.ToNewCString();
    char* newStr;
    // XXX We should use a strtok function that tokenizes PRUnichar's
    // so that we don't have to convert from Unicode to ASCII and then back

    char* token = nsCRT::strtok( str, ", ", &newStr );
    while( token != NULL ) {
      // Build an atom out of this attribute.
      nsCOMPtr<nsIAtom> atom;
      nsCOMPtr<nsIAtom> attribute;

      // Figure out if this token contains a :. 
      nsAutoString attrTok; attrTok.AssignWithConversion(token);
      PRInt32 index = attrTok.Find(":", PR_TRUE);
      if (index != -1) {
        // This attribute maps to something different.
        nsAutoString left, right;
        attrTok.Left(left, index);
        attrTok.Right(right, attrTok.Length()-index-1);

        atom = getter_AddRefs(NS_NewAtom(left.GetUnicode()));
        attribute = getter_AddRefs(NS_NewAtom(right.GetUnicode()));
      }
      else {
        nsAutoString tok; tok.AssignWithConversion(token);
        atom = getter_AddRefs(NS_NewAtom(tok.GetUnicode()));
        attribute = atom;
      }
      
      // Create an XBL attribute entry.
      nsXBLAttributeEntry* xblAttr = new (kPool) nsXBLAttributeEntry(attribute, aElement);

      // Now we should see if some element within our anonymous
      // content is already observing this attribute.
      nsISupportsKey key(atom);
      nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, 
                                                                     mAttributeTable->Get(&key)));
  
      nsCOMPtr<nsIXBLAttributeEntry> entry = do_QueryInterface(supports);
      if (!entry) {
        // Put it in the table.
        mAttributeTable->Put(&key, xblAttr);
      } else {
        nsCOMPtr<nsIXBLAttributeEntry> attr = entry;
        nsCOMPtr<nsIXBLAttributeEntry> tmpAttr = entry;
        do {
          attr = tmpAttr;
          attr->GetNext(getter_AddRefs(tmpAttr));
        } while (tmpAttr);
        attr->SetNext(xblAttr);
      }

      // Now make sure that this attribute is initially set.
      // XXX How to deal with NAMESPACES!!!?
      nsAutoString value;
      nsresult result = mBoundElement->GetAttribute(kNameSpaceID_None, atom, value);
      PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                            result == NS_CONTENT_ATTR_HAS_VALUE);

      if (attrPresent) {
        aElement->SetAttribute(kNameSpaceID_None, attribute, value, PR_FALSE);
        nsCOMPtr<nsIAtom> tag;
        aElement->GetTag(*getter_AddRefs(tag));
        if ((tag.get() == kHTMLAtom) && (attribute.get() == kValueAtom) && !value.IsEmpty()) {
          nsCOMPtr<nsIDOMText> textNode;
          nsCOMPtr<nsIDocument> doc;
          mBoundElement->GetDocument(*getter_AddRefs(doc));
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
          domDoc->CreateTextNode(value, getter_AddRefs(textNode));
          nsCOMPtr<nsIDOMNode> dummy;
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(aElement));
          domElement->AppendChild(textNode, getter_AddRefs(dummy));
        }
      }

      token = nsCRT::strtok( newStr, ", ", &newStr );
    }

    nsMemory::Free(str);
  }

  // Recur into our children.
  PRInt32 childCount;
  aElement->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aElement->ChildAt(i, *getter_AddRefs(child));
    ConstructAttributeTable(child);
  }
  return NS_OK;
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

PRBool
nsXBLBinding::IsMouseHandler(const nsString& aName)
{
  return ((aName == NS_LITERAL_STRING("click")) || (aName == NS_LITERAL_STRING("dblclick")) || (aName == NS_LITERAL_STRING("mousedown")) ||
          (aName == NS_LITERAL_STRING("mouseover")) || (aName == NS_LITERAL_STRING("mouseout")) || (aName == NS_LITERAL_STRING("mouseup")));
}

PRBool
nsXBLBinding::IsKeyHandler(const nsString& aName)
{
  return ((aName == NS_LITERAL_STRING("keypress")) || (aName == NS_LITERAL_STRING("keydown")) || (aName == NS_LITERAL_STRING("keyup")));
}

PRBool
nsXBLBinding::IsFocusHandler(const nsString& aName)
{
  return ((aName == NS_LITERAL_STRING("focus")) || (aName == NS_LITERAL_STRING("blur")));
}

PRBool
nsXBLBinding::IsXULHandler(const nsString& aName)
{
  return ((aName == NS_LITERAL_STRING("create")) || (aName == NS_LITERAL_STRING("destroy")) || (aName == NS_LITERAL_STRING("broadcast")) ||
          (aName == NS_LITERAL_STRING("command")) || (aName == NS_LITERAL_STRING("commandupdate")) || (aName == NS_LITERAL_STRING("close")));
}

PRBool
nsXBLBinding::IsScrollHandler(const nsString& aName)
{
  return (aName == NS_LITERAL_STRING("overflow") ||
          aName == NS_LITERAL_STRING("underflow") ||
          aName == NS_LITERAL_STRING("overflowchanged"));
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
  return mAllowScripts;
}

NS_IMETHODIMP
nsXBLBinding::GetInsertionPoint(nsIContent* aChild, nsIContent** aResult)
{
  *aResult = nsnull;
  if (mInsertionPointTable) {
    nsCOMPtr<nsIAtom> tag;
    aChild->GetTag(*getter_AddRefs(tag));
    nsISupportsKey key(tag);
    nsCOMPtr<nsIContent> content = getter_AddRefs(NS_STATIC_CAST(nsIContent*, 
                                                                 mInsertionPointTable->Get(&key)));
    if (!content) {
      nsISupportsKey key2(kChildrenAtom);
      content = getter_AddRefs(NS_STATIC_CAST(nsIContent*, mInsertionPointTable->Get(&key2)));
    }

    *aResult = content;
    NS_IF_ADDREF(*aResult);
  }
  return NS_OK;  
}

NS_IMETHODIMP
nsXBLBinding::GetSingleInsertionPoint(nsIContent** aResult, PRBool* aMultipleInsertionPoints)
{
  *aResult = nsnull;
  *aMultipleInsertionPoints = PR_FALSE;
  if (mInsertionPointTable) {
    if(mInsertionPointTable->Count() == 1) {
      nsISupportsKey key(kChildrenAtom);
      nsCOMPtr<nsIContent> content = getter_AddRefs(NS_STATIC_CAST(nsIContent*, 
                                                                   mInsertionPointTable->Get(&key)));
      *aResult = content;
      NS_IF_ADDREF(*aResult);
    }
    else 
      *aMultipleInsertionPoints = PR_TRUE;
  }
  return NS_OK;  
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
NS_NewXBLBinding(const nsCString& aDocURI, const nsCString& aRef, nsIXBLBinding** aResult)
{
  *aResult = new nsXBLBinding(aDocURI, aRef);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}


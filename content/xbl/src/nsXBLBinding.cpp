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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 * Contributor(s): Brendan Eich (brendan@mozilla.org)
 *                 Scott MacGregor (mscott@netscape.com)
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
#include "nsReadableUtils.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXMLContent.h"
#include "nsIXULContent.h"
#include "nsIXULDocument.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "nsXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsSupportsArray.h"
#include "nsINameSpace.h"
#include "jsapi.h"
#include "nsXBLService.h"
#include "nsIXBLInsertionPoint.h"
#include "nsIXPConnect.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIScriptContext.h"

// Event listeners
#include "nsIEventListenerManager.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMXULListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMMutationListener.h"
#include "nsIDOMContextMenuListener.h"

#include "nsXBLAtoms.h"
#include "nsXULAtoms.h"

#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"

#include "nsIXBLPrototypeProperty.h"
#include "nsXBLPrototypeHandler.h"

#include "nsXBLKeyHandler.h"
#include "nsXBLFocusHandler.h"
#include "nsXBLMouseHandler.h"
#include "nsXBLMouseMotionHandler.h"
#include "nsXBLMutationHandler.h"
#include "nsXBLXULHandler.h"
#include "nsXBLScrollHandler.h"
#include "nsXBLFormHandler.h"
#include "nsXBLDragHandler.h"
#include "nsXBLLoadHandler.h"
#include "nsXBLContextMenuHandler.h"

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
  name = ToNewCString(aClassName);
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

    { "popupshowing",  nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "popupshown",    nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "popuphiding" ,  nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "popuphidden",   nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "close",         nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "command",       nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "broadcast",     nsnull, &NS_GET_IID(nsIDOMXULListener)        },
    { "commandupdate", nsnull, &NS_GET_IID(nsIDOMXULListener)        },

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

    { "DOMSubtreeModified",           nsnull, &NS_GET_IID(nsIDOMMutationListener)        },
    { "DOMAttrModified",              nsnull, &NS_GET_IID(nsIDOMMutationListener)        },
    { "DOMCharacterDataModified",     nsnull, &NS_GET_IID(nsIDOMMutationListener)        },
    { "DOMNodeInserted",              nsnull, &NS_GET_IID(nsIDOMMutationListener)        },
    { "DOMNodeRemoved",               nsnull, &NS_GET_IID(nsIDOMMutationListener)        },
    { "DOMNodeInsertedIntoDocument",  nsnull, &NS_GET_IID(nsIDOMMutationListener)        },
    { "DOMNodeRemovedFromDocument",   nsnull, &NS_GET_IID(nsIDOMMutationListener)        },

    { "contextmenu", nsnull, &NS_GET_IID(nsIDOMContextMenuListener) },

    { nsnull,            nsnull, nsnull                                 }
};

// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLBinding, nsIXBLBinding)

// Constructors/Destructors
nsXBLBinding::nsXBLBinding(nsIXBLPrototypeBinding* aBinding)
: mFirstHandler(nsnull),
  mInsertionPointTable(nsnull),
  mIsStyleBinding(PR_TRUE),
  mMarkedForDeath(PR_FALSE)
{
  NS_INIT_REFCNT();
  mPrototypeBinding = aBinding;
  gRefCnt++;
  //  printf("REF COUNT UP: %d %s\n", gRefCnt, (const char*)mID);

  if (gRefCnt == 1) {
    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
      entry->mAttributeAtom = NS_NewAtom(entry->mAttributeName);
      ++entry;
    }
  }
}


nsXBLBinding::~nsXBLBinding(void)
{
  delete mInsertionPointTable;

  gRefCnt--;
  //  printf("REF COUNT DOWN: %d %s\n", gRefCnt, (const char*)mID);

  if (gRefCnt == 0) {
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

void
nsXBLBinding::InstallAnonymousContent(nsIContent* aAnonParent, nsIContent* aElement)
{
  // We need to ensure two things.
  // (1) The anonymous content should be fooled into thinking it's in the bound
  // element's document.
  nsCOMPtr<nsIDocument> doc;
  aElement->GetDocument(*getter_AddRefs(doc));

  aAnonParent->SetDocument(doc, PR_TRUE, AllowScripts());

  nsCOMPtr<nsIXULDocument> xuldoc(do_QueryInterface(doc));

  // (2) The children's parent back pointer should not be to this synthetic root
  // but should instead point to the enclosing parent element.
  PRInt32 childCount;
  aAnonParent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aAnonParent->ChildAt(i, *getter_AddRefs(child));
    child->SetParent(aElement);
    child->SetBindingParent(mBoundElement);

    // To make XUL templates work (and other goodies that happen when
    // an element is added to a XUL document), we need to notify the
    // XUL document using its special API.
    if (xuldoc)
      xuldoc->AddSubtreeToDocument(child);
  }
}

NS_IMETHODIMP
nsXBLBinding::SetAnonymousContent(nsIContent* aParent)
{
  // First cache the element.
  mContent = aParent;

  InstallAnonymousContent(mContent, mBoundElement);
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
nsXBLBinding::GetFirstBindingWithConstructor(nsIXBLBinding** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIXBLPrototypeHandler> constructor;
  mPrototypeBinding->GetConstructor(getter_AddRefs(constructor));
  if (constructor) {
    *aResult = this;
    NS_ADDREF(*aResult);
  }
  else if (mNextBinding)
    return mNextBinding->GetFirstBindingWithConstructor(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::HasStyleSheets(PRBool* aResolveStyle)
{
  // Find out if we need to re-resolve style.  We'll need to do this
  // if we have additional stylesheets in our binding document.
  PRBool hasSheets;
  mPrototypeBinding->HasStyleSheets(&hasSheets);
  if (hasSheets) {
    *aResolveStyle = PR_TRUE;
    return NS_OK;
  }

  if (mNextBinding)
    return mNextBinding->HasStyleSheets(aResolveStyle);
  return NS_OK;
}

struct EnumData {
  nsXBLBinding* mBinding;
 
  EnumData(nsXBLBinding* aBinding)
    :mBinding(aBinding)
  {};
};

struct ContentListData : public EnumData {
  nsIBindingManager* mBindingManager;

  ContentListData(nsXBLBinding* aBinding, nsIBindingManager* aManager)
    :EnumData(aBinding), mBindingManager(aManager)
  {};
};



PRBool PR_CALLBACK BuildContentLists(nsHashKey* aKey, void* aData, void* aClosure)
{
  ContentListData* data = (ContentListData*)aClosure;
  nsIBindingManager* bm = data->mBindingManager;
  nsXBLBinding* binding = data->mBinding;

  nsCOMPtr<nsIContent> boundElement;
  binding->GetBoundElement(getter_AddRefs(boundElement));

  nsISupportsArray* arr = (nsISupportsArray*)aData;
  PRUint32 count;
  arr->Count(&count);
  
  if (count == 0)
    return NS_OK;

  // XXX Could this array just be altered in place and passed directly to
  // SetContentListFor?  We'd save space if we could pull this off.
  nsCOMPtr<nsISupportsArray> contentList;
  NS_NewISupportsArray(getter_AddRefs(contentList));

  // Figure out the relevant content node.
  PRUint32 j = 0;
  nsCOMPtr<nsIXBLInsertionPoint> currPoint = getter_AddRefs((nsIXBLInsertionPoint*)arr->ElementAt(j));
  nsCOMPtr<nsIContent> parent;
  PRInt32 currIndex;
  currPoint->GetInsertionParent(getter_AddRefs(parent));
  currPoint->GetInsertionIndex(&currIndex);

  nsCOMPtr<nsIDOMNodeList> nodeList;
  if (parent == boundElement) {
    // We are altering anonymous nodes to accommodate insertion points.
    binding->GetAnonymousNodes(getter_AddRefs(nodeList));
  }
  else {
    // We are altering the explicit content list of a node to accommodate insertion points.
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(parent));
    node->GetChildNodes(getter_AddRefs(nodeList));
  }

  nsCOMPtr<nsIXBLInsertionPoint> pseudoPoint;
  PRUint32 childCount;
  nodeList->GetLength(&childCount);
  for (PRUint32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIDOMNode> node;
    nodeList->Item(i, getter_AddRefs(node));
    nsCOMPtr<nsIContent> child(do_QueryInterface(node));
    if (((PRInt32)i) == currIndex) {
      // Add the currPoint to the supports array.
      contentList->AppendElement(currPoint);

      // Get the next real insertion point and update our currIndex.
      j++;
      if (j < count) {
        currPoint = getter_AddRefs((nsIXBLInsertionPoint*)arr->ElementAt(j));
        currPoint->GetInsertionIndex(&currIndex);
      }

      // Null out our current pseudo-point.
      pseudoPoint = nsnull;
    }
    
    if (!pseudoPoint) {
      NS_NewXBLInsertionPoint(parent.get(), -1, nsnull, getter_AddRefs(pseudoPoint));
      contentList->AppendElement(pseudoPoint);
    }

    pseudoPoint->AddChild(child);
  }

  // Add in all the remaining insertion points.
  for ( ; j < count; j++) {
      currPoint = getter_AddRefs((nsIXBLInsertionPoint*)arr->ElementAt(j));
      contentList->AppendElement(currPoint);
  }
  
  // Now set the content list using the binding manager,
  // If the bound element is the parent, then we alter the anonymous node list
  // instead.  This allows us to always maintain two distinct lists should
  // insertion points be nested into an inner binding.
  if (parent == boundElement)
    bm->SetAnonymousNodesFor(parent, contentList);
  else 
    bm->SetContentListFor(parent, contentList);
  return PR_TRUE;
}

PRBool PR_CALLBACK RealizeDefaultContent(nsHashKey* aKey, void* aData, void* aClosure)
{
  ContentListData* data = (ContentListData*)aClosure;
  nsIBindingManager* bm = data->mBindingManager;
  nsXBLBinding* binding = data->mBinding;

  nsCOMPtr<nsIContent> boundElement;
  binding->GetBoundElement(getter_AddRefs(boundElement));

  nsISupportsArray* arr = (nsISupportsArray*)aData;
  PRUint32 count;
  arr->Count(&count);
 
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIXBLInsertionPoint> currPoint = getter_AddRefs((nsIXBLInsertionPoint*)arr->ElementAt(i));
    PRUint32 insCount;
    currPoint->ChildCount(&insCount);
    
    if (insCount == 0) {
      nsCOMPtr<nsIContent> defContent;
      currPoint->GetDefaultContentTemplate(getter_AddRefs(defContent));
      if (defContent) {
        // We need to take this template and use it to realize the
        // actual default content (through cloning).
        // Clone this insertion point element.
        nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(defContent));
        nsCOMPtr<nsIDOMNode> clonedNode;
        elt->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));

        nsCOMPtr<nsIContent> insParent;
        currPoint->GetInsertionParent(getter_AddRefs(insParent));

        // Now that we have the cloned content, install the default content as
        // if it were additional anonymous content.
        nsCOMPtr<nsIContent> clonedContent(do_QueryInterface(clonedNode));
        binding->InstallAnonymousContent(clonedContent, insParent);

        // Cache the clone so that it can be properly destroyed if/when our
        // other anonymous content is destroyed.
        currPoint->SetDefaultContent(clonedContent);

        // Now make sure the kids of the clone are added to the insertion point as
        // children.
        PRInt32 cloneKidCount;
        clonedContent->ChildCount(cloneKidCount);
        for (PRInt32 k = 0; k < cloneKidCount; k++) {
          nsCOMPtr<nsIContent> cloneChild;
          clonedContent->ChildAt(k, *getter_AddRefs(cloneChild));
          bm->SetInsertionParent(cloneChild, insParent);
          currPoint->AddChild(cloneChild);
        }
      }
    }
  }

  return PR_TRUE;
}

PRBool PR_CALLBACK ChangeDocumentForDefaultContent(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsISupportsArray* arr = (nsISupportsArray*)aData;
  PRUint32 count;
  arr->Count(&count);
  
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIXBLInsertionPoint> currPoint = getter_AddRefs((nsIXBLInsertionPoint*)arr->ElementAt(i));
    nsCOMPtr<nsIContent> defContent;
    currPoint->GetDefaultContent(getter_AddRefs(defContent));
    
    if (defContent)
      defContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLBinding::GenerateAnonymousContent()
{
  // Fetch the content element for this binding.
  nsCOMPtr<nsIContent> content;
  GetImmediateChild(nsXBLAtoms::content, getter_AddRefs(content));

  if (!content) {
    // We have no anonymous content.
    if (mNextBinding)
      return mNextBinding->GenerateAnonymousContent();
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

#ifdef DEBUG
  // See if there's an includes attribute.
  nsAutoString includes;
  content->GetAttr(kNameSpaceID_None, nsXBLAtoms::includes, includes);
  if (!includes.IsEmpty()) {
    nsCAutoString id;
    mPrototypeBinding->GetID(id);
    nsCAutoString message("An XBL Binding with an id of ");
    message += id;
    message += " and found in the file ";
    nsCAutoString uri;
    mPrototypeBinding->GetDocURI(uri);
    message += uri;
    message += " is still using the deprecated\n<content includes=\"\"> syntax! Use <children> instead!\n"; 
    NS_WARNING(message.get());
  }
#endif

  if (hasContent || hasInsertionPoints) {
    nsCOMPtr<nsIDocument> doc;
    mBoundElement->GetDocument(*getter_AddRefs(doc));

    // XXX doc will be null if we're in the midst of paint suppression.
    if (! doc)
      return NS_OK;
    
    nsCOMPtr<nsIBindingManager> bindingManager;
    doc->GetBindingManager(getter_AddRefs(bindingManager));

    nsCOMPtr<nsIDOMNodeList> children;
    bindingManager->GetContentListFor(mBoundElement, getter_AddRefs(children));
 
    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIContent> childContent;
    PRUint32 length;
    children->GetLength(&length);
    if (length > 0 && !hasInsertionPoints) {
      // There are children being placed underneath us, but we have no specified
      // insertion points, and therefore no place to put the kids.  Don't generate
      // anonymous content.
      // Special case template and observes.
      for (PRUint32 i = 0; i < length; i++) {
        children->Item(i, getter_AddRefs(node));
        childContent = do_QueryInterface(node);
        nsCOMPtr<nsIAtom> tag;
        childContent->GetTag(*getter_AddRefs(tag));
        if (tag != nsXULAtoms::observes && tag != nsXULAtoms::templateAtom) {
          hasContent = PR_FALSE;
          break;
        }
      }
    }

    if (hasContent || hasInsertionPoints) {
      nsCOMPtr<nsIContent> clonedContent;
      nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(content));

      nsCOMPtr<nsIDOMNode> clonedNode;
      domElement->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));
  
      clonedContent = do_QueryInterface(clonedNode);
      SetAnonymousContent(clonedContent);

      if (hasInsertionPoints) {
        // Now check and see if we have a single insertion point 
        // or multiple insertion points.
      
        // Enumerate the prototype binding's insertion table to build
        // our table of instantiated insertion points.
        mPrototypeBinding->InstantiateInsertionPoints(this);

        // We now have our insertion point table constructed.  We
        // enumerate this table.  For each array of insertion points
        // bundled under the same content node, we generate a content
        // list.  In the case of the bound element, we generate a new
        // anonymous node list that will be used in place of the binding's
        // cached anonymous node list.
        ContentListData data(this, bindingManager);
        mInsertionPointTable->Enumerate(BuildContentLists, &data);
      
        // We need to place the children
        // at their respective insertion points.
        nsCOMPtr<nsIContent> singlePoint;
        PRUint32 index = 0;
        PRBool multiplePoints = PR_FALSE;
        nsCOMPtr<nsIContent> singleDefaultContent;
        GetSingleInsertionPoint(getter_AddRefs(singlePoint), &index, 
                                &multiplePoints, getter_AddRefs(singleDefaultContent));
      
        if (children) {
          if (multiplePoints) {
            // We must walk the entire content list in order to determine where
            // each child belongs.
            children->GetLength(&length);
            for (PRUint32 i = 0; i < length; i++) {
              children->Item(i, getter_AddRefs(node));
              childContent = do_QueryInterface(node);

              // Now determine the insertion point in the prototype table.
              nsCOMPtr<nsIContent> point;
              PRUint32 index;
              nsCOMPtr<nsIContent> defContent;
              GetInsertionPoint(childContent, getter_AddRefs(point), &index, getter_AddRefs(defContent));
              bindingManager->SetInsertionParent(childContent, point);

              // Find the correct nsIXBLInsertion point in our table.
              nsCOMPtr<nsIXBLInsertionPoint> insertionPoint;
              nsCOMPtr<nsISupportsArray> arr;
              GetInsertionPointsFor(point, getter_AddRefs(arr));
              PRUint32 arrCount;
              arr->Count(&arrCount);
              for (PRUint32 j = 0; j < arrCount; j++) {
                insertionPoint = getter_AddRefs((nsIXBLInsertionPoint*)arr->ElementAt(j));
                PRBool matches;
                insertionPoint->Matches(point, index, &matches);
                if (matches)
                  break;
                insertionPoint = nsnull;
              }

              if (insertionPoint) 
                insertionPoint->AddChild(childContent);
              else {
                // We were unable to place this child.  All anonymous content
                // should be thrown out.  Special-case template and observes.
                nsCOMPtr<nsIAtom> tag;
                childContent->GetTag(*getter_AddRefs(tag));
                if (tag != nsXULAtoms::observes && tag != nsXULAtoms::templateAtom) {
                  // Kill all anonymous content.
                  mContent = nsnull;
                  bindingManager->SetContentListFor(mBoundElement, nsnull);
                  bindingManager->SetAnonymousNodesFor(mBoundElement, nsnull);
                  return NS_OK;
                }
              }
            }
          }
          else {
            // All of our children are shunted to this single insertion point.
            nsCOMPtr<nsISupportsArray> arr;
            GetInsertionPointsFor(singlePoint, getter_AddRefs(arr));
            PRUint32 arrCount;
            arr->Count(&arrCount);
            nsCOMPtr<nsIXBLInsertionPoint> insertionPoint = getter_AddRefs((nsIXBLInsertionPoint*)arr->ElementAt(0));
        
            nsCOMPtr<nsIDOMNode> node;
            nsCOMPtr<nsIContent> content;
            PRUint32 length;
            children->GetLength(&length);
          
            for (PRUint32 i = 0; i < length; i++) {
              children->Item(i, getter_AddRefs(node));
              content = do_QueryInterface(node);
              bindingManager->SetInsertionParent(content, singlePoint);
              insertionPoint->AddChild(content);
            }
          }
        }

        // Now that all of our children have been added, we need to walk all of our
        // nsIXBLInsertion points to see if any of them have default content that
        // needs to be built.
        mInsertionPointTable->Enumerate(RealizeDefaultContent, &data);
      }
    }

    mPrototypeBinding->SetInitialAttributes(mBoundElement, mContent);
  }

  // Always check the content element for potential attributes.
  // This shorthand hack always happens, even when we didn't
  // build anonymous content.
  PRInt32 length;
  content->GetAttrCount(length);

  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> name;
  nsCOMPtr<nsIAtom> prefix;

  for (PRInt32 i = 0; i < length; ++i) {
    content->GetAttrNameAt(i, namespaceID, *getter_AddRefs(name), *getter_AddRefs(prefix));

    if (name != nsXBLAtoms::includes) {
      nsAutoString value;
      mBoundElement->GetAttr(namespaceID, name, value);
      if (value.IsEmpty()) {
        nsAutoString value2;
        content->GetAttr(namespaceID, name, value2);
        mBoundElement->SetAttr(namespaceID, name, value2, PR_FALSE);
      }
    }

    // Conserve space by wiping the attributes off the clone.
    if (mContent)
      mContent->UnsetAttr(namespaceID, name, PR_FALSE);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallEventHandlers()
{
  // Don't install handlers if scripts aren't allowed.
  if (AllowScripts()) {
    // Fetch the handlers prototypes for this binding.
    nsCOMPtr<nsIXBLDocumentInfo> info;
    mPrototypeBinding->GetXBLDocumentInfo(mBoundElement, getter_AddRefs(info));
    if (!info)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXBLPrototypeHandler> handlerChain;
    mPrototypeBinding->GetPrototypeHandlers(getter_AddRefs(handlerChain));
  
    nsCOMPtr<nsIXBLPrototypeHandler> curr = handlerChain;
    nsXBLEventHandler* currHandler = nsnull;

    while (curr) {
      // Fetch the event type.
      nsCOMPtr<nsIAtom> eventAtom;
      curr->GetEventName(getter_AddRefs(eventAtom));

      nsIID iid;
      PRBool found = PR_FALSE;
      GetEventHandlerIID(eventAtom, &iid, &found);

      if (found) { 
        nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(mBoundElement);
        /*
        // Disable ATTACHTO capability for Mozilla 1.0
        nsAutoString attachType;
        child->GetAttr(kNameSpaceID_None, kAttachToAtom, attachType);
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
        */

        // Figure out if we're using capturing or not.
        PRUint8 phase;
        curr->GetPhase(&phase);
        PRBool useCapture = (phase == NS_PHASE_CAPTURING);
        
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
        else if (iid.Equals(NS_GET_IID(nsIDOMXULListener))) {
          nsXBLXULHandler* xulHandler;
          NS_NewXBLXULHandler(receiver, curr, &xulHandler);
          receiver->AddEventListener(type, (nsIDOMXULListener*)xulHandler, useCapture);
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
        else if(iid.Equals(NS_GET_IID(nsIDOMMutationListener))) {
          nsXBLMutationHandler* mutationHandler;
          NS_NewXBLMutationHandler(receiver, curr, &mutationHandler);
          receiver->AddEventListener(type, (nsIDOMMutationListener*)mutationHandler, useCapture);
          handler = mutationHandler;
        }
        else if(iid.Equals(NS_GET_IID(nsIDOMContextMenuListener))) {
          nsXBLContextMenuHandler* menuHandler;
          NS_NewXBLContextMenuHandler(receiver, curr, &menuHandler);
          receiver->AddEventListener(type, (nsIDOMContextMenuListener*)menuHandler, useCapture);
          handler = menuHandler;
        }
        else {
          NS_WARNING("***** Non-compliant XBL event listener attached! *****");
          // XXX Need to get the event text from the prototype handler!
          //AddScriptEventListener(mBoundElement, eventAtom, value);
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

  if (mNextBinding)
    mNextBinding->InstallEventHandlers();

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallProperties()
{
  // Always install the base class properties first, so that
  // derived classes can reference the base class properties.

  if (mNextBinding)
    mNextBinding->InstallProperties();

  // iterate through each property in the prototype's list and install the property.
  if (AllowScripts()) {
    nsCOMPtr<nsIXBLPrototypeProperty> propertyChain;
    mPrototypeBinding->GetPrototypeProperties(getter_AddRefs(propertyChain));

    if (!propertyChain) return NS_OK; // kick out if our list is empty

    nsCOMPtr<nsIDocument> document;
    mBoundElement->GetDocument(*getter_AddRefs(document));
    if (!document) return NS_OK;

    nsCOMPtr<nsIScriptGlobalObject> global;
    document->GetScriptGlobalObject(getter_AddRefs(global));
    if (!global) return NS_OK;

    nsCOMPtr<nsIScriptContext> context;
    nsresult rv = global->GetContext(getter_AddRefs(context));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!context) return NS_OK;

    void * targetScriptObject = nsnull;
    void * targetClassObject = nsnull;
    rv = propertyChain->InitTargetObjects(context, mBoundElement, &targetScriptObject, &targetClassObject);
    NS_ENSURE_SUCCESS(rv, rv); // kick out if we were unable to properly intialize our target objects

    nsCOMPtr<nsIXBLPrototypeProperty> curr = propertyChain;
    do  {
      curr->InstallProperty(context, mBoundElement, targetScriptObject, targetClassObject);
      curr->GetNextProperty(getter_AddRefs(propertyChain));
      curr = propertyChain;
    } while (curr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBaseTag(PRInt32* aNameSpaceID, nsIAtom** aResult)
{
  mPrototypeBinding->GetBaseTag(aNameSpaceID, aResult);
  if (!*aResult && mNextBinding)
    return mNextBinding->GetBaseTag(aNameSpaceID, aResult);
  return NS_OK;
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
      GetImmediateChild(nsXBLAtoms::implementation, getter_AddRefs(interfaceElement));

      if (interfaceElement) { 
        nsCOMPtr<nsIScriptGlobalObject> global;
        aOldDocument->GetScriptGlobalObject(getter_AddRefs(global));
        if (global) {
          nsCOMPtr<nsIScriptContext> context;
          global->GetContext(getter_AddRefs(context));
          if (context) {
            JSContext *jscontext = (JSContext *)context->GetNativeContext();
 
            nsresult rv;
            nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(),
                                                     &rv));
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;

            rv = xpc->WrapNative(jscontext, ::JS_GetGlobalObject(jscontext),
                                 mBoundElement, NS_GET_IID(nsISupports),
                                 getter_AddRefs(wrapper));
            NS_ENSURE_SUCCESS(rv, rv);

            JSObject* scriptObject = nsnull;
            rv = wrapper->GetJSObject(&scriptObject);
            NS_ENSURE_SUCCESS(rv, rv);

            // XXX Stay in sync! What if a layered binding has an
            // <interface>?!

            // XXX Sanity check to make sure our class name matches
            // Pull ourselves out of the proto chain.
            JSObject* ourProto = ::JS_GetPrototype(jscontext, scriptObject);
            if (ourProto)
            {
              JSObject* grandProto = ::JS_GetPrototype(jscontext, ourProto);
              ::JS_SetPrototype(jscontext, scriptObject, grandProto);
            }

            // Don't remove the reference from the document to the
            // wrapper here since it'll be removed by the element
            // itself when that's taken out of the document.
          }
        }
      }
    }

    // Update the anonymous content.
    nsCOMPtr<nsIContent> anonymous;
    GetAnonymousContent(getter_AddRefs(anonymous));
    if (anonymous) {
      // Also kill the default content within all our insertion points.
      if (mInsertionPointTable)
        mInsertionPointTable->Enumerate(ChangeDocumentForDefaultContent,
                                        nsnull);

      // To make XUL templates work (and other XUL-specific stuff),
      // we'll need to notify it using its add & remove APIs. Grab the
      // interface now...
      nsCOMPtr<nsIXULDocument> xuldoc(do_QueryInterface(aOldDocument));

      anonymous->SetDocument(nsnull, PR_TRUE, PR_TRUE); // Kill it.
      if (xuldoc)
        xuldoc->RemoveSubtreeFromDocument(anonymous);
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

  nsCOMPtr<nsISupportsArray> rules;
  mPrototypeBinding->GetRuleProcessors(getter_AddRefs(rules));
  if (rules)
    rules->EnumerateForwards(aFunc, aData);
  
  return rv;
}

NS_IMETHODIMP
nsXBLBinding::AttributeAffectsStyle(nsISupportsArrayEnumFunc aFunc, void* aData, PRBool* aAffects)
{
  nsresult rv = NS_OK;
  if (mNextBinding) {
    rv = mNextBinding->AttributeAffectsStyle(aFunc, aData, aAffects);
    if (NS_FAILED(rv))
      return rv;

    if (*aAffects)
      return NS_OK;
  }

  nsCOMPtr<nsISupportsArray> sheets;
  mPrototypeBinding->GetStyleSheets(getter_AddRefs(sheets));
  if (sheets) {
    if (!sheets->EnumerateForwards(aFunc, aData))
      *aAffects = PR_TRUE;
    else
      *aAffects = PR_FALSE;
  }
  
  return rv;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::InitClass(const nsCString& aClassName, nsIScriptContext* aContext, 
                        nsIDocument* aDocument, void** aScriptObject, void** aClassObject)
{
  *aClassObject = nsnull;
  *aScriptObject = nsnull;

  nsresult rv;

  // Obtain the bound element's current script object.
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  JSContext* jscontext = (JSContext*)aContext->GetNativeContext();

  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;

  JSObject* global = ::JS_GetGlobalObject(jscontext);

  rv = xpc->WrapNative(jscontext, global, mBoundElement,
                       NS_GET_IID(nsISupports), getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* object = nsnull;

  rv = wrapper->GetJSObject(&object);
  NS_ENSURE_SUCCESS(rv, rv);

  *aScriptObject = object;

  // First ensure our JS class is initialized.
  jsval vp;
  JSObject* proto;

  if ((! ::JS_LookupProperty(jscontext, global, aClassName.get(), &vp)) ||
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
        c->name = ToNewCString(aClassName);
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

  // Root mBoundElement so that it doesn't loose it's binding
  nsCOMPtr<nsIDocument> doc;

  mBoundElement->GetDocument(*getter_AddRefs(doc));

  if (doc) {
    nsCOMPtr<nsIXPConnectWrappedNative> native_wrapper =
      do_QueryInterface(wrapper);

    if (native_wrapper) {
      doc->AddReference(mBoundElement, native_wrapper);
    }
  }

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
    if (aTag == tag) {
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
nsXBLBinding::AddScriptEventListener(nsIContent* aElement, nsIAtom* aName,
                                     const nsString& aValue)
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

  if (!context) return NS_OK;

  nsCOMPtr<nsIEventListenerManager> manager;
  rv = receiver->GetListenerManager(getter_AddRefs(manager));
  if (NS_FAILED(rv)) return rv;

  rv = manager->AddScriptEventListener(context, receiver, eventName,
                                       aValue, PR_FALSE);

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
nsXBLBinding::GetInsertionPointsFor(nsIContent* aParent, nsISupportsArray** aResult)
{
  if (!mInsertionPointTable)
    mInsertionPointTable = new nsSupportsHashtable(4);

  nsISupportsKey key(aParent);
  *aResult = NS_STATIC_CAST(nsISupportsArray*, mInsertionPointTable->Get(&key));

  if (!*aResult) {
    NS_NewISupportsArray(aResult);
    mInsertionPointTable->Put(&key, *aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetInsertionPoint(nsIContent* aChild, nsIContent** aResult, PRUint32* aIndex, 
                                nsIContent** aDefaultContent)
{
  *aResult = nsnull;
  *aDefaultContent = nsnull;
  if (mContent)
    return mPrototypeBinding->GetInsertionPoint(mBoundElement, mContent, aChild, aResult, aIndex, aDefaultContent);
  else if (mNextBinding)
    return mNextBinding->GetInsertionPoint(aChild, aResult, aIndex, aDefaultContent);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetSingleInsertionPoint(nsIContent** aResult, PRUint32* aIndex, PRBool* aMultipleInsertionPoints,
                                      nsIContent** aDefaultContent)
{
  *aResult = nsnull;
  *aDefaultContent = nsnull;
  *aMultipleInsertionPoints = PR_FALSE;
  if (mContent)
    return mPrototypeBinding->GetSingleInsertionPoint(mBoundElement, mContent, aResult, aIndex, 
                                                      aMultipleInsertionPoints, aDefaultContent);
  else if (mNextBinding)
    return mNextBinding->GetSingleInsertionPoint(aResult, aIndex, aMultipleInsertionPoints, aDefaultContent);
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
  ExecuteDetachedHandler();
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::MarkedForDeath(PRBool* aResult)
{
  *aResult = mMarkedForDeath;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::ImplementsInterface(REFNSIID aIID, PRBool* aResult)
{
  mPrototypeBinding->ImplementsInterface(aIID, aResult);
  if (!*aResult && mNextBinding)
    return mNextBinding->ImplementsInterface(aIID, aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetAnonymousNodes(nsIDOMNodeList** aResult)
{
  *aResult = nsnull;
  if (mContent) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mContent));
    return elt->GetChildNodes(aResult);
  }
  else if (mNextBinding)
    return mNextBinding->GetAnonymousNodes(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::ShouldBuildChildFrames(PRBool* aResult)
{
  *aResult = PR_TRUE;
  if (mContent)
    return mPrototypeBinding->ShouldBuildChildFrames(aResult);
  else if (mNextBinding) 
    return mNextBinding->ShouldBuildChildFrames(aResult);

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

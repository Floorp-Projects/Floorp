/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 *   Brendan Eich (brendan@mozilla.org)
 *   Scott MacGregor (mscott@netscape.com)
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
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIAtom.h"
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
#include "nsContentUtils.h"
#ifdef MOZ_XUL
#include "nsIXULDocument.h"
#endif
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "nsXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsSupportsArray.h"
#include "nsINameSpace.h"
#include "jsapi.h"
#include "nsXBLService.h"
#include "nsXBLInsertionPoint.h"
#include "nsIXPConnect.h"
#include "nsIScriptContext.h"
#include "nsCRT.h"

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
#include "nsIDOMEventGroup.h"

#include "nsXBLAtoms.h"
#include "nsXULAtoms.h"

#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"

#include "nsXBLPrototypeHandler.h"

#include "nsXBLPrototypeBinding.h"
#include "nsXBLBinding.h"

#include "prprf.h"

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

nsXBLJSClass::nsXBLJSClass(const nsAFlatCString& aClassName)
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

// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLBinding, nsIXBLBinding)

// Constructors/Destructors
nsXBLBinding::nsXBLBinding(nsXBLPrototypeBinding* aBinding)
: mPrototypeBinding(aBinding),
  mInsertionPointTable(nsnull),
  mIsStyleBinding(PR_TRUE),
  mMarkedForDeath(PR_FALSE)
{
  NS_ASSERTION(mPrototypeBinding, "Must have a prototype binding!");
  // Grab a ref to the document info so the prototype binding won't die
  NS_ADDREF(mPrototypeBinding->XBLDocumentInfo());
}


nsXBLBinding::~nsXBLBinding(void)
{
  delete mInsertionPointTable;
  nsIXBLDocumentInfo* info = mPrototypeBinding->XBLDocumentInfo();
  NS_RELEASE(info);
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
  nsCOMPtr<nsIDocument> doc = aElement->GetDocument();

  aAnonParent->SetDocument(doc, PR_TRUE, AllowScripts());

  // (2) The children's parent back pointer should not be to this synthetic root
  // but should instead point to the enclosing parent element.
  PRUint32 childCount = aAnonParent->GetChildCount();
  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent *child = aAnonParent->GetChildAt(i);
    child->SetParent(aElement);
    child->SetBindingParent(mBoundElement);

#ifdef MOZ_XUL
    // To make XUL templates work (and other goodies that happen when
    // an element is added to a XUL document), we need to notify the
    // XUL document using its special API.
    nsCOMPtr<nsIXULDocument> xuldoc(do_QueryInterface(doc));
    if (xuldoc)
      xuldoc->AddSubtreeToDocument(child);
#endif
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
nsXBLBinding::GetPrototypeBinding(nsXBLPrototypeBinding** aResult)
{
  *aResult = mPrototypeBinding;
  return NS_OK;
}
  
NS_IMETHODIMP
nsXBLBinding::SetPrototypeBinding(nsXBLPrototypeBinding* aProtoBinding)
{
  mPrototypeBinding = aProtoBinding;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBindingElement(nsIContent** aResult)
{
  *aResult = mPrototypeBinding->GetBindingElement().get();
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBindingElement(nsIContent* aElement)
{
  mPrototypeBinding->SetBindingElement(aElement);
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
nsXBLBinding::GetFirstBindingWithConstructor(nsIXBLBinding** aResult)
{
  *aResult = nsnull;

  nsXBLPrototypeHandler* constructor = mPrototypeBinding->GetConstructor();
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
  if (mPrototypeBinding->HasStyleSheets()) {
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



PR_STATIC_CALLBACK(PRBool)
BuildContentLists(nsHashKey* aKey, void* aData, void* aClosure)
{
  ContentListData* data = (ContentListData*)aClosure;
  nsIBindingManager* bm = data->mBindingManager;
  nsXBLBinding* binding = data->mBinding;

  nsCOMPtr<nsIContent> boundElement;
  binding->GetBoundElement(getter_AddRefs(boundElement));

  nsVoidArray* arr = NS_STATIC_CAST(nsVoidArray*, aData);
  PRInt32 count = arr->Count();
  
  if (count == 0)
    return NS_OK;

  // XXX Could this array just be altered in place and passed directly to
  // SetContentListFor?  We'd save space if we could pull this off.
  nsVoidArray* contentList = new nsVoidArray();

  // Figure out the relevant content node.
  nsXBLInsertionPoint* currPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, arr->ElementAt(0));
  nsCOMPtr<nsIContent> parent = currPoint->GetInsertionParent();
  PRInt32 currIndex = currPoint->GetInsertionIndex();

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

  nsXBLInsertionPoint* pseudoPoint = nsnull;
  PRUint32 childCount;
  nodeList->GetLength(&childCount);
  PRInt32 j = 0;

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
        currPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, arr->ElementAt(j));
        currIndex = currPoint->GetInsertionIndex();
      }

      // Null out our current pseudo-point.
      pseudoPoint = nsnull;
    }
    
    if (!pseudoPoint) {
      pseudoPoint = new nsXBLInsertionPoint(parent, (PRUint32) -1, nsnull);
      contentList->AppendElement(pseudoPoint);
    }

    pseudoPoint->AddChild(child);
  }

  // Add in all the remaining insertion points.
  for ( ; j < count; j++) {
    currPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, arr->ElementAt(j));
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

PR_STATIC_CALLBACK(PRBool)
RealizeDefaultContent(nsHashKey* aKey, void* aData, void* aClosure)
{
  ContentListData* data = (ContentListData*)aClosure;
  nsIBindingManager* bm = data->mBindingManager;
  nsXBLBinding* binding = data->mBinding;

  nsCOMPtr<nsIContent> boundElement;
  binding->GetBoundElement(getter_AddRefs(boundElement));

  nsVoidArray* arr = (nsVoidArray*)aData;
  PRInt32 count = arr->Count();
 
  for (PRInt32 i = 0; i < count; i++) {
    nsXBLInsertionPoint* currPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, arr->ElementAt(i));
    PRInt32 insCount = currPoint->ChildCount();
    
    if (insCount == 0) {
      nsCOMPtr<nsIContent> defContent = currPoint->GetDefaultContentTemplate();
      if (defContent) {
        // We need to take this template and use it to realize the
        // actual default content (through cloning).
        // Clone this insertion point element.
        nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(defContent));
        nsCOMPtr<nsIDOMNode> clonedNode;
        elt->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));

        nsCOMPtr<nsIContent> insParent = currPoint->GetInsertionParent();

        // Now that we have the cloned content, install the default content as
        // if it were additional anonymous content.
        nsCOMPtr<nsIContent> clonedContent(do_QueryInterface(clonedNode));
        binding->InstallAnonymousContent(clonedContent, insParent);

        // Cache the clone so that it can be properly destroyed if/when our
        // other anonymous content is destroyed.
        currPoint->SetDefaultContent(clonedContent);

        // Now make sure the kids of the clone are added to the insertion point as
        // children.
        PRUint32 cloneKidCount = clonedContent->GetChildCount();
        for (PRUint32 k = 0; k < cloneKidCount; k++) {
          nsIContent *cloneChild = clonedContent->GetChildAt(k);
          bm->SetInsertionParent(cloneChild, insParent);
          currPoint->AddChild(cloneChild);
        }
      }
    }
  }

  return PR_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
ChangeDocumentForDefaultContent(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsVoidArray* arr = NS_STATIC_CAST(nsVoidArray*, aData);
  PRInt32 count = arr->Count();
  
  for (PRInt32 i = 0; i < count; i++) {
    nsXBLInsertionPoint* currPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, arr->ElementAt(i));
    nsCOMPtr<nsIContent> defContent = currPoint->GetDefaultContent();
    
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
  PRUint32 contentCount = content->GetChildCount();

  // Plan to build the content by default.
  PRBool hasContent = (contentCount > 0);
  PRBool hasInsertionPoints = mPrototypeBinding->HasInsertionPoints();

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
    mPrototypeBinding->DocURI()->GetSpec(uri);
    message += uri;
    message += " is still using the deprecated\n<content includes=\"\"> syntax! Use <children> instead!\n"; 
    NS_WARNING(message.get());
  }
#endif

  if (hasContent || hasInsertionPoints) {
    nsIDocument* doc = mBoundElement->GetDocument();

    // XXX doc will be null if we're in the midst of paint suppression.
    if (! doc)
      return NS_OK;
    
    nsIBindingManager *bindingManager = doc->GetBindingManager();

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

        nsINodeInfo *ni = childContent->GetNodeInfo();

        if (!ni || (!ni->Equals(nsXULAtoms::observes, kNameSpaceID_XUL) &&
                    !ni->Equals(nsXULAtoms::templateAtom, kNameSpaceID_XUL))) {
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
              nsVoidArray* arr;
              GetInsertionPointsFor(point, &arr);
              nsXBLInsertionPoint* insertionPoint = nsnull;
              PRInt32 arrCount = arr->Count();
              for (PRInt32 j = 0; j < arrCount; j++) {
                insertionPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, arr->ElementAt(j));
                if (insertionPoint->Matches(point, index))
                  break;
                insertionPoint = nsnull;
              }

              if (insertionPoint) 
                insertionPoint->AddChild(childContent);
              else {
                // We were unable to place this child.  All anonymous content
                // should be thrown out.  Special-case template and observes.

                nsINodeInfo *ni = childContent->GetNodeInfo();

                if (!ni ||
                    (!ni->Equals(nsXULAtoms::observes, kNameSpaceID_XUL) &&
                     !ni->Equals(nsXULAtoms::templateAtom, kNameSpaceID_XUL))) {
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
            nsVoidArray* arr;
            GetInsertionPointsFor(singlePoint, &arr);
            nsXBLInsertionPoint* insertionPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, arr->ElementAt(0));
        
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
  PRUint32 length = content->GetAttrCount();

  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> name;
  nsCOMPtr<nsIAtom> prefix;

  for (PRUint32 i = 0; i < length; ++i) {
    content->GetAttrNameAt(i, &namespaceID, getter_AddRefs(name),
                           getter_AddRefs(prefix));

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
    nsXBLPrototypeHandler* handlerChain = mPrototypeBinding->GetPrototypeHandlers();

    if (handlerChain) {
      nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(mBoundElement);
      nsCOMPtr<nsIDOM3EventTarget> target = do_QueryInterface(receiver);
      nsCOMPtr<nsIDOMEventGroup> systemEventGroup;

      nsXBLPrototypeHandler* curr;
      for (curr = handlerChain; curr; curr = curr->GetNextHandler()) {
        // Fetch the event type.
        nsCOMPtr<nsIAtom> eventAtom = curr->GetEventName();
        if (!eventAtom ||
            eventAtom == nsXBLAtoms::keyup ||
            eventAtom == nsXBLAtoms::keydown ||
            eventAtom == nsXBLAtoms::keypress)
          continue;

        nsAutoString type;
        eventAtom->ToString(type);

        // Figure out if we're using capturing or not.
        PRBool useCapture = (curr->GetPhase() == NS_PHASE_CAPTURING);

        // If this is a command, add it in the system event group, otherwise 
        // add it to the standard event group.

        // This is a weak ref. systemEventGroup above is already a
        // strong ref, so we are guaranteed it will not go away.
        nsIDOMEventGroup* eventGroup = nsnull;
        if (curr->GetType() & NS_HANDLER_TYPE_XBL_COMMAND) {
          if (!systemEventGroup)
            receiver->GetSystemEventGroup(getter_AddRefs(systemEventGroup));
          eventGroup = systemEventGroup;
        }

        nsXBLEventHandler* handler = curr->GetEventHandler();
        if (handler) {
          target->AddGroupedEventListener(type, handler, useCapture,
                                          eventGroup);
        }
      }

      const nsCOMArray<nsXBLKeyEventHandler>* keyHandlers =
        mPrototypeBinding->GetKeyEventHandlers();
      PRInt32 i;
      for (i = 0; i < keyHandlers->Count(); ++i) {
        nsXBLKeyEventHandler* handler = keyHandlers->ObjectAt(i);

        nsAutoString type;
        handler->GetEventName(type);

        // Figure out if we're using capturing or not.
        PRBool useCapture = (handler->GetPhase() == NS_PHASE_CAPTURING);

        // If this is a command, add it in the system event group, otherwise 
        // add it to the standard event group.

        // This is a weak ref. systemEventGroup above is already a
        // strong ref, so we are guaranteed it will not go away.
        nsIDOMEventGroup* eventGroup = nsnull;
        if (handler->GetType() & NS_HANDLER_TYPE_XBL_COMMAND) {
          if (!systemEventGroup)
            receiver->GetSystemEventGroup(getter_AddRefs(systemEventGroup));
          eventGroup = systemEventGroup;
        }

        target->AddGroupedEventListener(type, handler, useCapture, eventGroup);
      }
    }
  }

  if (mNextBinding)
    mNextBinding->InstallEventHandlers();

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallImplementation()
{
  // Always install the base class properties first, so that
  // derived classes can reference the base class properties.

  if (mNextBinding)
    mNextBinding->InstallImplementation();

  // iterate through each property in the prototype's list and install the property.
  if (AllowScripts())
    mPrototypeBinding->InstallImplementation(mBoundElement);

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
nsXBLBinding::AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID,
                               PRBool aRemoveFlag, PRBool aNotify)
{
  // XXX Change if we ever allow multiple bindings in a chain to contribute anonymous content
  if (!mContent) {
    if (mNextBinding)
      return mNextBinding->AttributeChanged(aAttribute, aNameSpaceID,
                                            aRemoveFlag, aNotify);
    return NS_OK;
  }

  mPrototypeBinding->AttributeChanged(aAttribute, aNameSpaceID, aRemoveFlag,
                                      mBoundElement, mContent, aNotify);
  return NS_OK;
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
  nsXBLPrototypeHandler* handlerChain = mPrototypeBinding->GetPrototypeHandlers();

  if (handlerChain) {
    nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(mBoundElement);
    nsCOMPtr<nsIDOM3EventTarget> target = do_QueryInterface(receiver);
    nsCOMPtr<nsIDOMEventGroup> systemEventGroup;

    nsXBLPrototypeHandler* curr;
    for (curr = handlerChain; curr; curr = curr->GetNextHandler()) {
      nsXBLEventHandler* handler = curr->GetCachedEventHandler();
      if (handler) {
        nsCOMPtr<nsIAtom> eventAtom = curr->GetEventName();
        if (!eventAtom ||
            eventAtom == nsXBLAtoms::keyup ||
            eventAtom == nsXBLAtoms::keydown ||
            eventAtom == nsXBLAtoms::keypress)
          continue;

        nsAutoString type;
        eventAtom->ToString(type);

        // Figure out if we're using capturing or not.
        PRBool useCapture = (curr->GetPhase() == NS_PHASE_CAPTURING);

        // If this is a command, remove it from the system event group, otherwise 
        // remove it from the standard event group.

        // This is a weak ref. systemEventGroup above is already a
        // strong ref, so we are guaranteed it will not go away.
        nsIDOMEventGroup* eventGroup = nsnull;
        if (curr->GetType() & NS_HANDLER_TYPE_XBL_COMMAND) {
          if (!systemEventGroup)
            receiver->GetSystemEventGroup(getter_AddRefs(systemEventGroup));
          eventGroup = systemEventGroup;
        }

        target->RemoveGroupedEventListener(type, handler, useCapture,
                                           eventGroup);
      }
    }

    const nsCOMArray<nsXBLKeyEventHandler>* keyHandlers =
      mPrototypeBinding->GetKeyEventHandlers();
    PRInt32 i;
    for (i = 0; i < keyHandlers->Count(); ++i) {
      nsXBLKeyEventHandler* handler = keyHandlers->ObjectAt(i);

      nsAutoString type;
      handler->GetEventName(type);

      // Figure out if we're using capturing or not.
      PRBool useCapture = (handler->GetPhase() == NS_PHASE_CAPTURING);

      // If this is a command, remove it from the system event group, otherwise 
      // remove it from the standard event group.

      // This is a weak ref. systemEventGroup above is already a
      // strong ref, so we are guaranteed it will not go away.
      nsIDOMEventGroup* eventGroup = nsnull;
      if (handler->GetType() & NS_HANDLER_TYPE_XBL_COMMAND) {
        if (!systemEventGroup)
          receiver->GetSystemEventGroup(getter_AddRefs(systemEventGroup));
        eventGroup = systemEventGroup;
      }

      target->RemoveGroupedEventListener(type, handler, useCapture,
                                         eventGroup);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument)
{
  if (aOldDocument != aNewDocument) {
    if (mNextBinding)
      mNextBinding->ChangeDocument(aOldDocument, aNewDocument);

    // Only style bindings get their prototypes unhooked.
    if (mIsStyleBinding) {
      // Now the binding dies.  Unhook our prototypes.
      nsCOMPtr<nsIContent> interfaceElement;
      GetImmediateChild(nsXBLAtoms::implementation, getter_AddRefs(interfaceElement));

      if (interfaceElement) { 
        nsIScriptGlobalObject *global = aOldDocument->GetScriptGlobalObject();
        if (global) {
          nsIScriptContext *context = global->GetContext();
          if (context) {
            JSContext *jscontext = (JSContext *)context->GetNativeContext();
 
            nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
            nsresult rv = nsContentUtils::XPConnect()->
              WrapNative(jscontext, ::JS_GetGlobalObject(jscontext),
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

#ifdef MOZ_XUL
      nsCOMPtr<nsIXULDocument> xuldoc(do_QueryInterface(aOldDocument));
#endif

      anonymous->SetDocument(nsnull, PR_TRUE, PR_TRUE); // Kill it.

#ifdef MOZ_XUL
      // To make XUL templates work (and other XUL-specific stuff),
      // we'll need to notify it using its add & remove APIs. Grab the
      // interface now...
      if (xuldoc)
        xuldoc->RemoveSubtreeFromDocument(anonymous);
#endif
    }
  }

  return NS_OK;
}

NS_IMETHODIMP_(nsIURI*)
nsXBLBinding::BindingURI() const
{
  return mPrototypeBinding->BindingURI();
}

NS_IMETHODIMP_(nsIURI*) 
nsXBLBinding::DocURI() const
{
  return mPrototypeBinding->DocURI();
}

NS_IMETHODIMP 
nsXBLBinding::GetID(nsACString& aResult) const
{
  return mPrototypeBinding->GetID(aResult);
}

NS_IMETHODIMP
nsXBLBinding::InheritsStyle(PRBool* aResult)
{
  // XXX Will have to change if we ever allow multiple bindings to contribute anonymous content.
  // Most derived binding with anonymous content determines style inheritance for now.

  // XXX What about bindings with <content> but no kids, e.g., my treecell-text binding?
  if (mContent) {
    *aResult = mPrototypeBinding->InheritsStyle();
    return NS_OK;
  }
  
  if (mNextBinding)
    return mNextBinding->InheritsStyle(aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc, void* aData)
{
  nsresult rv = NS_OK;
  if (mNextBinding) {
    rv = mNextBinding->WalkRules(aFunc, aData);
    if (NS_FAILED(rv))
      return rv;
  }

  nsIStyleRuleProcessor *rules = mPrototypeBinding->GetRuleProcessor();
  if (rules)
    (*aFunc)(rules, aData);
  
  return rv;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

// static
nsresult
nsXBLBinding::DoInitJSClass(JSContext *cx, JSObject *global, JSObject *obj,
                            const nsAFlatCString& aClassName,
                            void **aClassObject)
{
  // First ensure our JS class is initialized.
  jsval val;
  JSObject* proto;

  nsCAutoString className(aClassName);
  // Retrieve the current prototype of obj.
  JSObject* parent_proto = ::JS_GetPrototype(cx, obj);
  if (parent_proto) {
    // We need to create a unique classname based on aClassName and
    // parent_proto.  Append a space (an invalid URI character) to ensure that
    // we don't have accidental collisions with the case when parent_proto is
    // null and aClassName ends in some bizarre numbers (yeah, it's unlikely).
    jsid parent_proto_id;
    if (!::JS_GetObjectId(cx, parent_proto, &parent_proto_id)) {
      // Probably OOM
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // One space, maybe "0x", at most 16 chars (on a 64-bit system) of long,
    // and a null-terminator (which PR_snprintf ensures is there even if the
    // string representation of what we're printing does not fit in the buffer
    // provided).
    char buf[20];
    PR_snprintf(buf, sizeof(buf), " %lx", parent_proto_id);
    className.Append(buf);
  }

  if ((!::JS_LookupProperty(cx, global, className.get(), &val)) ||
      JSVAL_IS_PRIMITIVE(val)) {  
    // We need to initialize the class.

    nsXBLJSClass* c;
    void* classObject;
    nsCStringKey key(className);
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
        c = new nsXBLJSClass(className);

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
        c->name = ToNewCString(className);
      }

      // Add c to our table.
      (nsXBLService::gClassTable)->Put(&key, (void*)c);
    }

    // The prototype holds a strong reference to its class struct.
    c->Hold();

    // Make a new object prototyped by parent_proto and parented by global.
    proto = ::JS_InitClass(cx,                  // context
                           global,              // global object
                           parent_proto,        // parent proto 
                           c,                   // JSClass
                           nsnull,              // JSNative ctor
                           0,                   // ctor args
                           nsnull,              // proto props
                           nsnull,              // proto funcs
                           nsnull,              // ctor props (static)
                           nsnull);             // ctor funcs (static)
    if (!proto) {
      // This will happen if we're OOM or if the security manager
      // denies defining the new class...

      (nsXBLService::gClassTable)->Remove(&key);

      c->Drop();

      return NS_ERROR_OUT_OF_MEMORY;
    }

    *aClassObject = (void*)proto;
  }
  else {
    proto = JSVAL_TO_OBJECT(val);
  }

  // Set the prototype of our object to be the new class.
  if (!::JS_SetPrototype(cx, obj, proto)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


nsresult
nsXBLBinding::InitClass(const nsCString& aClassName,
                        nsIScriptContext* aContext, 
                        nsIDocument* aDocument, void** aScriptObject,
                        void** aClassObject)
{
  *aClassObject = nsnull;
  *aScriptObject = nsnull;

  nsresult rv;

  // Obtain the bound element's current script object.
  JSContext* cx = (JSContext*)aContext->GetNativeContext();

  nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;

  JSObject* global = ::JS_GetGlobalObject(cx);

  rv = nsContentUtils::XPConnect()->WrapNative(cx, global, mBoundElement,
                                               NS_GET_IID(nsISupports),
                                               getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* object = nsnull;
  rv = wrapper->GetJSObject(&object);
  NS_ENSURE_SUCCESS(rv, rv);

  *aScriptObject = object;

  // First ensure our JS class is initialized.

  rv = DoInitJSClass(cx, global, object, aClassName, aClassObject);
  NS_ENSURE_SUCCESS(rv, rv);

  // Root mBoundElement so that it doesn't lose it's binding
  nsIDocument* doc = mBoundElement->GetDocument();

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
  nsCOMPtr<nsIContent> binding = mPrototypeBinding->GetBindingElement();

  *aResult = nsnull;
  PRUint32 childCount = binding->GetChildCount();

  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent *child = binding->GetChildAt(i);

    if (aTag == child->Tag()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }
}

PRBool
nsXBLBinding::IsInExcludesList(nsIAtom* aTag, const nsString& aList) 
{ 
  nsAutoString element;
  aTag->ToString(element);

  if (aList.EqualsLiteral("*"))
      return PR_TRUE; // match _everything_!

  PRInt32 indx = aList.Find(element, 0);
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

NS_IMETHODIMP
nsXBLBinding::AddScriptEventListener(nsIContent* aElement, nsIAtom* aName,
                                     const nsString& aValue)
{
  nsAutoString val;
  aName->ToString(val);
  
  nsAutoString eventStr(NS_LITERAL_STRING("on"));
  eventStr += val;

  nsCOMPtr<nsIAtom> eventName = do_GetAtom(eventStr);

  nsresult rv;

  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(aElement));
  if (!receiver)
    return NS_OK;

  nsCOMPtr<nsIEventListenerManager> manager;
  rv = receiver->GetListenerManager(getter_AddRefs(manager));
  if (NS_FAILED(rv)) return rv;

  rv = manager->AddScriptEventListener(receiver, eventName, aValue, PR_FALSE);

  return rv;
}

nsresult
nsXBLBinding::GetTextData(nsIContent *aParent, nsString& aResult)
{
  aResult.Truncate(0);

  PRUint32 textCount = aParent->GetChildCount();
  nsAutoString answer;
  for (PRUint32 j = 0; j < textCount; j++) {
    // Get the child.
    nsCOMPtr<nsIDOMText> text(do_QueryInterface(aParent->GetChildAt(j)));
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

PR_STATIC_CALLBACK(PRBool)
DeleteVoidArray(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete NS_STATIC_CAST(nsVoidArray*, aData);
  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLBinding::GetInsertionPointsFor(nsIContent* aParent, nsVoidArray** aResult)
{
  if (!mInsertionPointTable) {
    mInsertionPointTable = new nsObjectHashtable(nsnull, nsnull,
                                                 DeleteVoidArray, nsnull, 4);

    NS_ENSURE_TRUE(mInsertionPointTable, NS_ERROR_OUT_OF_MEMORY);
  }

  nsISupportsKey key(aParent);
  *aResult = NS_STATIC_CAST(nsVoidArray*, mInsertionPointTable->Get(&key));

  if (!*aResult) {
    *aResult = new nsVoidArray();
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
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
    mPrototypeBinding->GetInsertionPoint(mBoundElement, mContent, aChild, aResult, aIndex, aDefaultContent);
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
    mPrototypeBinding->GetSingleInsertionPoint(mBoundElement, mContent, aResult, aIndex, 
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
  *aResult = mPrototypeBinding->ImplementsInterface(aIID);
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
    *aResult = mPrototypeBinding->ShouldBuildChildFrames();
  else if (mNextBinding) 
    return mNextBinding->ShouldBuildChildFrames(aResult);

  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLBinding(nsXBLPrototypeBinding* aBinding, nsIXBLBinding** aResult)
{
  *aResult = new nsXBLBinding(aBinding);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

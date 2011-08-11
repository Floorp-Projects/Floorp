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
#include "nsXBLDocumentInfo.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIDOMEventTarget.h"
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
#include "jsapi.h"
#include "nsXBLService.h"
#include "nsXBLInsertionPoint.h"
#include "nsIXPConnect.h"
#include "nsIScriptContext.h"
#include "nsCRT.h"

// Event listeners
#include "nsEventListenerManager.h"
#include "nsIDOMEventListener.h"
#include "nsAttrName.h"

#include "nsGkAtoms.h"

#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"

#include "nsXBLPrototypeHandler.h"

#include "nsXBLPrototypeBinding.h"
#include "nsXBLBinding.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsEventListenerManager.h"
#include "nsGUIEvent.h"

#include "prprf.h"
#include "nsNodeUtils.h"

// Nasty hack.  Maybe we could move some of the classinfo utility methods
// (e.g. WrapNative and ThrowJSException) over to nsContentUtils?
#include "nsDOMClassInfo.h"
#include "nsJSUtils.h"

#include "mozilla/dom/Element.h"

// Helper classes

/***********************************************************************/
//
// The JS class for XBLBinding
//
static void
XBLFinalize(JSContext *cx, JSObject *obj)
{
  nsXBLDocumentInfo* docInfo =
    static_cast<nsXBLDocumentInfo*>(::JS_GetPrivate(cx, obj));
  NS_RELEASE(docInfo);
  
  nsXBLJSClass* c = static_cast<nsXBLJSClass*>(::JS_GET_CLASS(cx, obj));
  c->Drop();
}

static JSBool
XBLResolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
           JSObject **objp)
{
  // Note: if we get here, that means that the implementation for some binding
  // was installed, which means that AllowScripts() tested true.  Hence no need
  // to do checks like that here.
  
  // Default to not resolving things.
  NS_ASSERTION(*objp, "Must have starting object");

  JSObject* origObj = *objp;
  *objp = NULL;

  if (!JSID_IS_STRING(id)) {
    return JS_TRUE;
  }

  nsDependentJSString fieldName(id);

  jsval slotVal;
  ::JS_GetReservedSlot(cx, obj, 0, &slotVal);
  NS_ASSERTION(!JSVAL_IS_VOID(slotVal), "How did that happen?");
    
  nsXBLPrototypeBinding* protoBinding =
    static_cast<nsXBLPrototypeBinding*>(JSVAL_TO_PRIVATE(slotVal));
  NS_ASSERTION(protoBinding, "Must have prototype binding!");

  nsXBLProtoImplField* field = protoBinding->FindField(fieldName);
  if (!field) {
    return JS_TRUE;
  }

  // We have this field.  Time to install it.  Get our node.
  JSClass* nodeClass = ::JS_GET_CLASS(cx, origObj);
  if (!nodeClass) {
    return JS_FALSE;
  }
  
  if (~nodeClass->flags &
      (JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS)) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);
    return JS_FALSE;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> xpcWrapper =
    do_QueryInterface(static_cast<nsISupports*>(::JS_GetPrivate(cx, origObj)));
  if (!xpcWrapper) {
    // Looks like whatever |origObj| is it's not our nsIContent.  It might well
    // be the proto our binding installed, however, where the private is the
    // nsXBLDocumentInfo, so just baul out quietly.  Do NOT throw an exception
    // here.
    // We could make this stricter by checking the class maybe, but whatever
    return JS_TRUE;
  }

  nsCOMPtr<nsIContent> content = do_QueryWrappedNative(xpcWrapper);
  if (!content) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_UNEXPECTED);
    return JS_FALSE;
  }

  // This mirrors code in nsXBLProtoImpl::InstallImplementation
  nsIDocument* doc = content->GetOwnerDoc();
  if (!doc) {
    return JS_TRUE;
  }

  nsIScriptGlobalObject* global = doc->GetScriptGlobalObject();
  if (!global) {
    return JS_TRUE;
  }

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) {
    return JS_TRUE;
  }


  // Now we either resolve or fail
  PRBool didInstall;
  nsresult rv = field->InstallField(context, origObj,
                                    content->NodePrincipal(),
                                    protoBinding->DocURI(),
                                    &didInstall);
  if (NS_FAILED(rv)) {
    if (!::JS_IsExceptionPending(cx)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);
    }

    return JS_FALSE;
  }

  if (didInstall) {
    *objp = origObj;
  }
  // else we didn't resolve this field after all

  return JS_TRUE;
}

nsXBLJSClass::nsXBLJSClass(const nsAFlatCString& aClassName)
{
  memset(this, 0, sizeof(nsXBLJSClass));
  next = prev = static_cast<JSCList*>(this);
  name = ToNewCString(aClassName);
  flags =
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS |
    JSCLASS_NEW_RESOLVE | JSCLASS_NEW_RESOLVE_GETS_START |
    // Our one reserved slot holds the relevant nsXBLPrototypeBinding
    JSCLASS_HAS_RESERVED_SLOTS(1);
  addProperty = delProperty = getProperty = ::JS_PropertyStub;
  setProperty = ::JS_StrictPropertyStub;
  enumerate = ::JS_EnumerateStub;
  resolve = (JSResolveOp)XBLResolve;
  convert = ::JS_ConvertStub;
  finalize = XBLFinalize;
}

nsrefcnt
nsXBLJSClass::Destroy()
{
  NS_ASSERTION(next == prev && prev == static_cast<JSCList*>(this),
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
    JSCList* mru = static_cast<JSCList*>(this);
    JS_APPEND_LINK(mru, &nsXBLService::gClassLRUList);
    nsXBLService::gClassLRUListLength++;
  }

  return 0;
}

// Implementation /////////////////////////////////////////////////////////////////

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
  if (mContent) {
    nsXBLBinding::UninstallAnonymousContent(mContent->GetOwnerDoc(), mContent);
  }
  delete mInsertionPointTable;
  nsXBLDocumentInfo* info = mPrototypeBinding->XBLDocumentInfo();
  NS_RELEASE(info);
}

static PLDHashOperator
TraverseKey(nsISupports* aKey, nsInsertionPointList* aData, void* aClosure)
{
  nsCycleCollectionTraversalCallback &cb = 
    *static_cast<nsCycleCollectionTraversalCallback*>(aClosure);

  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mInsertionPointTable key");
  cb.NoteXPCOMChild(aKey);
  if (aData) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY(*aData, nsXBLInsertionPoint,
                                               "mInsertionPointTable value")
  }
  return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXBLBinding)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(nsXBLBinding)
  // XXX Probably can't unlink mPrototypeBinding->XBLDocumentInfo(), because
  //     mPrototypeBinding is weak.
  if (tmp->mContent) {
    nsXBLBinding::UninstallAnonymousContent(tmp->mContent->GetOwnerDoc(),
                                            tmp->mContent);
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mNextBinding)
  delete tmp->mInsertionPointTable;
  tmp->mInsertionPointTable = nsnull;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsXBLBinding)
  cb.NoteXPCOMChild(static_cast<nsIScriptGlobalObjectOwner*>(
                      tmp->mPrototypeBinding->XBLDocumentInfo()));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mNextBinding, nsXBLBinding)
  if (tmp->mInsertionPointTable)
    tmp->mInsertionPointTable->EnumerateRead(TraverseKey, &cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXBLBinding, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXBLBinding, Release)

void
nsXBLBinding::SetBaseBinding(nsXBLBinding* aBinding)
{
  if (mNextBinding) {
    NS_ERROR("Base XBL binding is already defined!");
    return;
  }

  mNextBinding = aBinding; // Comptr handles rel/add
}

void
nsXBLBinding::InstallAnonymousContent(nsIContent* aAnonParent, nsIContent* aElement)
{
  // We need to ensure two things.
  // (1) The anonymous content should be fooled into thinking it's in the bound
  // element's document, assuming that the bound element is in a document
  // Note that we don't change the current doc of aAnonParent here, since that
  // quite simply does not matter.  aAnonParent is just a way of keeping refs
  // to all its kids, which are anonymous content from the point of view of
  // aElement.
  // (2) The children's parent back pointer should not be to this synthetic root
  // but should instead point to the enclosing parent element.
  nsIDocument* doc = aElement->GetCurrentDoc();
  PRBool allowScripts = AllowScripts();

  nsAutoScriptBlocker scriptBlocker;

  PRUint32 childCount = aAnonParent->GetChildCount();
  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent *child = aAnonParent->GetChildAt(i);
    child->UnbindFromTree();
    nsresult rv =
      child->BindToTree(doc, aElement, mBoundElement, allowScripts);
    if (NS_FAILED(rv)) {
      // Oh, well... Just give up.
      // XXXbz This really shouldn't be a void method!
      child->UnbindFromTree();
      return;
    }        

    child->SetFlags(NODE_IS_ANONYMOUS);

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

void
nsXBLBinding::UninstallAnonymousContent(nsIDocument* aDocument,
                                        nsIContent* aAnonParent)
{
  nsAutoScriptBlocker scriptBlocker;
  // Hold a strong ref while doing this, just in case.
  nsCOMPtr<nsIContent> anonParent = aAnonParent;
#ifdef MOZ_XUL
  nsCOMPtr<nsIXULDocument> xuldoc =
    do_QueryInterface(aDocument);
#endif
  PRUint32 childCount = aAnonParent->GetChildCount();
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsIContent* child = aAnonParent->GetChildAt(i);
    child->UnbindFromTree();
#ifdef MOZ_XUL
    if (xuldoc) {
      xuldoc->RemoveSubtreeFromDocument(child);
    }
#endif
  }
}

void
nsXBLBinding::SetBoundElement(nsIContent* aElement)
{
  mBoundElement = aElement;
  if (mNextBinding)
    mNextBinding->SetBoundElement(aElement);
}

PRBool
nsXBLBinding::HasStyleSheets() const
{
  // Find out if we need to re-resolve style.  We'll need to do this
  // if we have additional stylesheets in our binding document.
  if (mPrototypeBinding->HasStyleSheets())
    return PR_TRUE;

  return mNextBinding ? mNextBinding->HasStyleSheets() : PR_FALSE;
}

struct EnumData {
  nsXBLBinding* mBinding;
 
  EnumData(nsXBLBinding* aBinding)
    :mBinding(aBinding)
  {}
};

struct ContentListData : public EnumData {
  nsBindingManager* mBindingManager;
  nsresult          mRv;

  ContentListData(nsXBLBinding* aBinding, nsBindingManager* aManager)
    :EnumData(aBinding), mBindingManager(aManager), mRv(NS_OK)
  {}
};

static PLDHashOperator
BuildContentLists(nsISupports* aKey,
                  nsAutoPtr<nsInsertionPointList>& aData,
                  void* aClosure)
{
  ContentListData* data = (ContentListData*)aClosure;
  nsBindingManager* bm = data->mBindingManager;
  nsXBLBinding* binding = data->mBinding;

  nsIContent *boundElement = binding->GetBoundElement();

  PRInt32 count = aData->Length();
  
  if (count == 0)
    return PL_DHASH_NEXT;

  // Figure out the relevant content node.
  nsXBLInsertionPoint* currPoint = aData->ElementAt(0);
  nsCOMPtr<nsIContent> parent = currPoint->GetInsertionParent();
  if (!parent) {
    data->mRv = NS_ERROR_FAILURE;
    return PL_DHASH_STOP;
  }
  PRInt32 currIndex = currPoint->GetInsertionIndex();

  // XXX Could this array just be altered in place and passed directly to
  // SetContentListFor?  We'd save space if we could pull this off.
  nsInsertionPointList* contentList = new nsInsertionPointList;
  if (!contentList) {
    data->mRv = NS_ERROR_OUT_OF_MEMORY;
    return PL_DHASH_STOP;
  }

  nsCOMPtr<nsIDOMNodeList> nodeList;
  if (parent == boundElement) {
    // We are altering anonymous nodes to accommodate insertion points.
    nodeList = binding->GetAnonymousNodes();
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
      // Add the currPoint to the insertion point list.
      contentList->AppendElement(currPoint);

      // Get the next real insertion point and update our currIndex.
      j++;
      if (j < count) {
        currPoint = aData->ElementAt(j);
        currIndex = currPoint->GetInsertionIndex();
      }

      // Null out our current pseudo-point.
      pseudoPoint = nsnull;
    }
    
    if (!pseudoPoint) {
      pseudoPoint = new nsXBLInsertionPoint(parent, (PRUint32) -1, nsnull);
      if (pseudoPoint) {
        contentList->AppendElement(pseudoPoint);
      }
    }
    if (pseudoPoint) {
      pseudoPoint->AddChild(child);
    }
  }

  // Add in all the remaining insertion points.
  contentList->AppendElements(aData->Elements() + j, count - j);
  
  // Now set the content list using the binding manager,
  // If the bound element is the parent, then we alter the anonymous node list
  // instead.  This allows us to always maintain two distinct lists should
  // insertion points be nested into an inner binding.
  if (parent == boundElement)
    bm->SetAnonymousNodesFor(parent, contentList);
  else 
    bm->SetContentListFor(parent, contentList);
  return PL_DHASH_NEXT;
}

static PLDHashOperator
RealizeDefaultContent(nsISupports* aKey,
                      nsAutoPtr<nsInsertionPointList>& aData,
                      void* aClosure)
{
  ContentListData* data = (ContentListData*)aClosure;
  nsBindingManager* bm = data->mBindingManager;
  nsXBLBinding* binding = data->mBinding;

  PRInt32 count = aData->Length();
 
  for (PRInt32 i = 0; i < count; i++) {
    nsXBLInsertionPoint* currPoint = aData->ElementAt(i);
    PRInt32 insCount = currPoint->ChildCount();
    
    if (insCount == 0) {
      nsCOMPtr<nsIContent> defContent = currPoint->GetDefaultContentTemplate();
      if (defContent) {
        // We need to take this template and use it to realize the
        // actual default content (through cloning).
        // Clone this insertion point element.
        nsCOMPtr<nsIContent> insParent = currPoint->GetInsertionParent();
        if (!insParent) {
          data->mRv = NS_ERROR_FAILURE;
          return PL_DHASH_STOP;
        }
        nsIDocument *document = insParent->GetOwnerDoc();
        if (!document) {
          data->mRv = NS_ERROR_FAILURE;
          return PL_DHASH_STOP;
        }

        nsCOMPtr<nsIDOMNode> clonedNode;
        nsCOMArray<nsINode> nodesWithProperties;
        nsNodeUtils::Clone(defContent, PR_TRUE, document->NodeInfoManager(),
                           nodesWithProperties, getter_AddRefs(clonedNode));

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

  return PL_DHASH_NEXT;
}

static PLDHashOperator
ChangeDocumentForDefaultContent(nsISupports* aKey,
                                nsAutoPtr<nsInsertionPointList>& aData,
                                void* aClosure)
{
  PRInt32 count = aData->Length();
  for (PRInt32 i = 0; i < count; i++) {
    aData->ElementAt(i)->UnbindDefaultContent();
  }

  return PL_DHASH_NEXT;
}

void
nsXBLBinding::GenerateAnonymousContent()
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "Someone forgot a script blocker");

  // Fetch the content element for this binding.
  nsIContent* content =
    mPrototypeBinding->GetImmediateChild(nsGkAtoms::content);

  if (!content) {
    // We have no anonymous content.
    if (mNextBinding)
      mNextBinding->GenerateAnonymousContent();

    return;
  }
     
  // Find out if we're really building kids or if we're just
  // using the attribute-setting shorthand hack.
  PRUint32 contentCount = content->GetChildCount();

  // Plan to build the content by default.
  PRBool hasContent = (contentCount > 0);
  PRBool hasInsertionPoints = mPrototypeBinding->HasInsertionPoints();

#ifdef DEBUG
  // See if there's an includes attribute.
  if (nsContentUtils::HasNonEmptyAttr(content, kNameSpaceID_None,
                                      nsGkAtoms::includes)) {
    nsCAutoString message("An XBL Binding with URI ");
    nsCAutoString uri;
    mPrototypeBinding->BindingURI()->GetSpec(uri);
    message += uri;
    message += " is still using the deprecated\n<content includes=\"\"> syntax! Use <children> instead!\n"; 
    NS_WARNING(message.get());
  }
#endif

  if (hasContent || hasInsertionPoints) {
    nsIDocument* doc = mBoundElement->GetOwnerDoc();

    // XXX doc will be null if we're in the midst of paint suppression.
    if (! doc)
      return;
    
    nsBindingManager *bindingManager = doc->BindingManager();

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

        nsINodeInfo *ni = childContent->NodeInfo();
        nsIAtom *localName = ni->NameAtom();
        if (ni->NamespaceID() != kNameSpaceID_XUL ||
            (localName != nsGkAtoms::observes &&
             localName != nsGkAtoms::_template)) {
          hasContent = PR_FALSE;
          break;
        }
      }
    }

    if (hasContent || hasInsertionPoints) {
      nsCOMPtr<nsIDOMNode> clonedNode;
      nsCOMArray<nsINode> nodesWithProperties;
      nsNodeUtils::Clone(content, PR_TRUE, doc->NodeInfoManager(),
                         nodesWithProperties, getter_AddRefs(clonedNode));

      mContent = do_QueryInterface(clonedNode);
      InstallAnonymousContent(mContent, mBoundElement);

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
        if (NS_FAILED(data.mRv)) {
          return;
        }

        // We need to place the children
        // at their respective insertion points.
        PRUint32 index = 0;
        PRBool multiplePoints = PR_FALSE;
        nsIContent *singlePoint = GetSingleInsertionPoint(&index,
                                                          &multiplePoints);
      
        if (children) {
          if (multiplePoints) {
            // We must walk the entire content list in order to determine where
            // each child belongs.
            children->GetLength(&length);
            for (PRUint32 i = 0; i < length; i++) {
              children->Item(i, getter_AddRefs(node));
              childContent = do_QueryInterface(node);

              // Now determine the insertion point in the prototype table.
              PRUint32 index;
              nsIContent *point = GetInsertionPoint(childContent, &index);
              bindingManager->SetInsertionParent(childContent, point);

              // Find the correct nsIXBLInsertion point in our table.
              nsInsertionPointList* arr = nsnull;
              GetInsertionPointsFor(point, &arr);
              nsXBLInsertionPoint* insertionPoint = nsnull;
              PRInt32 arrCount = arr->Length();
              for (PRInt32 j = 0; j < arrCount; j++) {
                insertionPoint = arr->ElementAt(j);
                if (insertionPoint->Matches(point, index))
                  break;
                insertionPoint = nsnull;
              }

              if (insertionPoint) 
                insertionPoint->AddChild(childContent);
              else {
                // We were unable to place this child.  All anonymous content
                // should be thrown out.  Special-case template and observes.

                nsINodeInfo *ni = childContent->NodeInfo();
                nsIAtom *localName = ni->NameAtom();
                if (ni->NamespaceID() != kNameSpaceID_XUL ||
                    (localName != nsGkAtoms::observes &&
                     localName != nsGkAtoms::_template)) {
                  // Undo InstallAnonymousContent
                  UninstallAnonymousContent(doc, mContent);

                  // Kill all anonymous content.
                  mContent = nsnull;
                  bindingManager->SetContentListFor(mBoundElement, nsnull);
                  bindingManager->SetAnonymousNodesFor(mBoundElement, nsnull);
                  return;
                }
              }
            }
          }
          else {
            // All of our children are shunted to this single insertion point.
            nsInsertionPointList* arr = nsnull;
            GetInsertionPointsFor(singlePoint, &arr);
            nsXBLInsertionPoint* insertionPoint = arr->ElementAt(0);
        
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
        if (NS_FAILED(data.mRv)) {
          return;
        }
      }
    }

    mPrototypeBinding->SetInitialAttributes(mBoundElement, mContent);
  }

  // Always check the content element for potential attributes.
  // This shorthand hack always happens, even when we didn't
  // build anonymous content.
  const nsAttrName* attrName;
  for (PRUint32 i = 0; (attrName = content->GetAttrNameAt(i)); ++i) {
    PRInt32 namespaceID = attrName->NamespaceID();
    // Hold a strong reference here so that the atom doesn't go away during
    // UnsetAttr.
    nsCOMPtr<nsIAtom> name = attrName->LocalName();

    if (name != nsGkAtoms::includes) {
      if (!nsContentUtils::HasNonEmptyAttr(mBoundElement, namespaceID, name)) {
        nsAutoString value2;
        content->GetAttr(namespaceID, name, value2);
        mBoundElement->SetAttr(namespaceID, name, attrName->GetPrefix(),
                               value2, PR_FALSE);
      }
    }

    // Conserve space by wiping the attributes off the clone.
    if (mContent)
      mContent->UnsetAttr(namespaceID, name, PR_FALSE);
  }
}

void
nsXBLBinding::InstallEventHandlers()
{
  // Don't install handlers if scripts aren't allowed.
  if (AllowScripts()) {
    // Fetch the handlers prototypes for this binding.
    nsXBLPrototypeHandler* handlerChain = mPrototypeBinding->GetPrototypeHandlers();

    if (handlerChain) {
      nsEventListenerManager* manager =
        mBoundElement->GetListenerManager(PR_TRUE);
      if (!manager)
        return;

      PRBool isChromeDoc =
        nsContentUtils::IsChromeDoc(mBoundElement->GetOwnerDoc());
      PRBool isChromeBinding = mPrototypeBinding->IsChrome();
      nsXBLPrototypeHandler* curr;
      for (curr = handlerChain; curr; curr = curr->GetNextHandler()) {
        // Fetch the event type.
        nsCOMPtr<nsIAtom> eventAtom = curr->GetEventName();
        if (!eventAtom ||
            eventAtom == nsGkAtoms::keyup ||
            eventAtom == nsGkAtoms::keydown ||
            eventAtom == nsGkAtoms::keypress)
          continue;

        nsXBLEventHandler* handler = curr->GetEventHandler();
        if (handler) {
          // Figure out if we're using capturing or not.
          PRInt32 flags = (curr->GetPhase() == NS_PHASE_CAPTURING) ?
            NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

          // If this is a command, add it in the system event group
          if ((curr->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND |
                                  NS_HANDLER_TYPE_SYSTEM)) &&
              (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
            flags |= NS_EVENT_FLAG_SYSTEM_EVENT;
          }

          PRBool hasAllowUntrustedAttr = curr->HasAllowUntrustedAttr();
          if ((hasAllowUntrustedAttr && curr->AllowUntrustedEvents()) ||
              (!hasAllowUntrustedAttr && !isChromeDoc)) {
            flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
          }

          manager->AddEventListenerByType(handler,
                                          nsDependentAtomString(eventAtom),
                                          flags);
        }
      }

      const nsCOMArray<nsXBLKeyEventHandler>* keyHandlers =
        mPrototypeBinding->GetKeyEventHandlers();
      PRInt32 i;
      for (i = 0; i < keyHandlers->Count(); ++i) {
        nsXBLKeyEventHandler* handler = keyHandlers->ObjectAt(i);
        handler->SetIsBoundToChrome(isChromeDoc);

        nsAutoString type;
        handler->GetEventName(type);

        // If this is a command, add it in the system event group, otherwise 
        // add it to the standard event group.

        // Figure out if we're using capturing or not.
        PRInt32 flags = (handler->GetPhase() == NS_PHASE_CAPTURING) ?
          NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

        if ((handler->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND |
                                   NS_HANDLER_TYPE_SYSTEM)) &&
            (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
          flags |= NS_EVENT_FLAG_SYSTEM_EVENT;
        }

        // For key handlers we have to set NS_PRIV_EVENT_UNTRUSTED_PERMITTED flag.
        // Whether the handling of the event is allowed or not is handled in
        // nsXBLKeyEventHandler::HandleEvent
        flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;

        manager->AddEventListenerByType(handler, type, flags);
      }
    }
  }

  if (mNextBinding)
    mNextBinding->InstallEventHandlers();
}

nsresult
nsXBLBinding::InstallImplementation()
{
  // Always install the base class properties first, so that
  // derived classes can reference the base class properties.

  if (mNextBinding) {
    nsresult rv = mNextBinding->InstallImplementation();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  // iterate through each property in the prototype's list and install the property.
  if (AllowScripts())
    return mPrototypeBinding->InstallImplementation(mBoundElement);

  return NS_OK;
}

nsIAtom*
nsXBLBinding::GetBaseTag(PRInt32* aNameSpaceID)
{
  nsIAtom *tag = mPrototypeBinding->GetBaseTag(aNameSpaceID);
  if (!tag && mNextBinding)
    return mNextBinding->GetBaseTag(aNameSpaceID);

  return tag;
}

void
nsXBLBinding::AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID,
                               PRBool aRemoveFlag, PRBool aNotify)
{
  // XXX Change if we ever allow multiple bindings in a chain to contribute anonymous content
  if (!mContent) {
    if (mNextBinding)
      mNextBinding->AttributeChanged(aAttribute, aNameSpaceID,
                                     aRemoveFlag, aNotify);
  } else {
    mPrototypeBinding->AttributeChanged(aAttribute, aNameSpaceID, aRemoveFlag,
                                        mBoundElement, mContent, aNotify);
  }
}

void
nsXBLBinding::ExecuteAttachedHandler()
{
  if (mNextBinding)
    mNextBinding->ExecuteAttachedHandler();

  if (AllowScripts())
    mPrototypeBinding->BindingAttached(mBoundElement);
}

void
nsXBLBinding::ExecuteDetachedHandler()
{
  if (AllowScripts())
    mPrototypeBinding->BindingDetached(mBoundElement);

  if (mNextBinding)
    mNextBinding->ExecuteDetachedHandler();
}

void
nsXBLBinding::UnhookEventHandlers()
{
  nsXBLPrototypeHandler* handlerChain = mPrototypeBinding->GetPrototypeHandlers();

  if (handlerChain) {
    nsEventListenerManager* manager =
      mBoundElement->GetListenerManager(PR_FALSE);
    if (!manager) {
      return;
    }
                                      
    PRBool isChromeBinding = mPrototypeBinding->IsChrome();
    nsXBLPrototypeHandler* curr;
    for (curr = handlerChain; curr; curr = curr->GetNextHandler()) {
      nsXBLEventHandler* handler = curr->GetCachedEventHandler();
      if (!handler) {
        continue;
      }
      
      nsCOMPtr<nsIAtom> eventAtom = curr->GetEventName();
      if (!eventAtom ||
          eventAtom == nsGkAtoms::keyup ||
          eventAtom == nsGkAtoms::keydown ||
          eventAtom == nsGkAtoms::keypress)
        continue;

      // Figure out if we're using capturing or not.
      PRInt32 flags = (curr->GetPhase() == NS_PHASE_CAPTURING) ?
        NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

      // If this is a command, remove it from the system event group,
      // otherwise remove it from the standard event group.

      if ((curr->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND |
                              NS_HANDLER_TYPE_SYSTEM)) &&
          (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
        flags |= NS_EVENT_FLAG_SYSTEM_EVENT;
      }

      manager->RemoveEventListenerByType(handler,
                                         nsDependentAtomString(eventAtom),
                                         flags);
    }

    const nsCOMArray<nsXBLKeyEventHandler>* keyHandlers =
      mPrototypeBinding->GetKeyEventHandlers();
    PRInt32 i;
    for (i = 0; i < keyHandlers->Count(); ++i) {
      nsXBLKeyEventHandler* handler = keyHandlers->ObjectAt(i);

      nsAutoString type;
      handler->GetEventName(type);

      // Figure out if we're using capturing or not.
      PRInt32 flags = (handler->GetPhase() == NS_PHASE_CAPTURING) ?
        NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

      // If this is a command, remove it from the system event group, otherwise 
      // remove it from the standard event group.

      if ((handler->GetType() & (NS_HANDLER_TYPE_XBL_COMMAND | NS_HANDLER_TYPE_SYSTEM)) &&
          (isChromeBinding || mBoundElement->IsInNativeAnonymousSubtree())) {
        flags |= NS_EVENT_FLAG_SYSTEM_EVENT;
      }

      manager->RemoveEventListenerByType(handler, type, flags);
    }
  }
}

void
nsXBLBinding::ChangeDocument(nsIDocument* aOldDocument, nsIDocument* aNewDocument)
{
  if (aOldDocument != aNewDocument) {
    // Only style bindings get their prototypes unhooked.  First do ourselves.
    if (mIsStyleBinding) {
      // Now the binding dies.  Unhook our prototypes.
      if (mPrototypeBinding->HasImplementation()) { 
        nsIScriptGlobalObject *global = aOldDocument->GetScopeObject();
        if (global) {
          JSObject *scope = global->GetGlobalJSObject();
          // scope might be null if we've cycle-collected the global
          // object, since the Unlink phase of cycle collection happens
          // after JS GC finalization.  But in that case, we don't care
          // about fixing the prototype chain, since everything's going
          // away immediately.

          nsCOMPtr<nsIScriptContext> context = global->GetContext();
          if (context && scope) {
            JSContext *cx = (JSContext *)context->GetNativeContext();
 
            nsCxPusher pusher;
            pusher.Push(cx);

            nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
            nsIXPConnect *xpc = nsContentUtils::XPConnect();
            nsresult rv =
              xpc->GetWrappedNativeOfNativeObject(cx, scope, mBoundElement,
                                                  NS_GET_IID(nsISupports),
                                                  getter_AddRefs(wrapper));
            if (NS_FAILED(rv))
              return;

            JSObject* scriptObject;
            if (wrapper)
                wrapper->GetJSObject(&scriptObject);
            else
                scriptObject = nsnull;

            if (scriptObject) {
              // XXX Stay in sync! What if a layered binding has an
              // <interface>?!
              // XXXbz what does that comment mean, really?  It seems to date
              // back to when there was such a thing as an <interface>, whever
              // that was...

              // Find the right prototype.
              JSObject* base = scriptObject;
              JSObject* proto;
              JSAutoRequest ar(cx);
              JSAutoEnterCompartment ac;
              if (!ac.enter(cx, scriptObject)) {
                return;
              }

              for ( ; true; base = proto) { // Will break out on null proto
                proto = ::JS_GetPrototype(cx, base);
                if (!proto) {
                  break;
                }

                JSClass* clazz = ::JS_GET_CLASS(cx, proto);
                if (!clazz ||
                    (~clazz->flags &
                     (JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS)) ||
                    JSCLASS_RESERVED_SLOTS(clazz) != 1 ||
                    clazz->resolve != (JSResolveOp)XBLResolve ||
                    clazz->finalize != XBLFinalize) {
                  // Clearly not the right class
                  continue;
                }

                nsRefPtr<nsXBLDocumentInfo> docInfo =
                  static_cast<nsXBLDocumentInfo*>(::JS_GetPrivate(cx, proto));
                if (!docInfo) {
                  // Not the proto we seek
                  continue;
                }

                jsval protoBinding;
                if (!::JS_GetReservedSlot(cx, proto, 0, &protoBinding)) {
                  NS_ERROR("Really shouldn't happen");
                  continue;
                }

                if (JSVAL_TO_PRIVATE(protoBinding) != mPrototypeBinding) {
                  // Not the right binding
                  continue;
                }

                // Alright!  This is the right prototype.  Pull it out of the
                // proto chain.
                JSObject* grandProto = ::JS_GetPrototype(cx, proto);
                ::JS_SetPrototype(cx, base, grandProto);
                break;
              }

              mPrototypeBinding->UndefineFields(cx, scriptObject);

              // Don't remove the reference from the document to the
              // wrapper here since it'll be removed by the element
              // itself when that's taken out of the document.
            }
          }
        }
      }

      // Remove our event handlers
      UnhookEventHandlers();
    }

    {
      nsAutoScriptBlocker scriptBlocker;

      // Then do our ancestors.  This reverses the construction order, so that at
      // all times things are consistent as far as everyone is concerned.
      if (mNextBinding) {
        mNextBinding->ChangeDocument(aOldDocument, aNewDocument);
      }

      // Update the anonymous content.
      // XXXbz why not only for style bindings?
      nsIContent *anonymous = mContent;
      if (anonymous) {
        // Also kill the default content within all our insertion points.
        if (mInsertionPointTable)
          mInsertionPointTable->Enumerate(ChangeDocumentForDefaultContent,
                                          nsnull);

        nsXBLBinding::UninstallAnonymousContent(aOldDocument, anonymous);
      }

      // Make sure that henceforth we don't claim that mBoundElement's children
      // have insertion parents in the old document.
      nsBindingManager* bindingManager = aOldDocument->BindingManager();
      for (PRUint32 i = mBoundElement->GetChildCount(); i > 0; --i) {
        NS_ASSERTION(mBoundElement->GetChildAt(i-1),
                     "Must have child at i for 0 <= i < GetChildCount()!");
        bindingManager->SetInsertionParent(mBoundElement->GetChildAt(i-1),
                                           nsnull);
      }
    }
  }
}

PRBool
nsXBLBinding::InheritsStyle() const
{
  // XXX Will have to change if we ever allow multiple bindings to contribute anonymous content.
  // Most derived binding with anonymous content determines style inheritance for now.

  // XXX What about bindings with <content> but no kids, e.g., my treecell-text binding?
  if (mContent)
    return mPrototypeBinding->InheritsStyle();
  
  if (mNextBinding)
    return mNextBinding->InheritsStyle();

  return PR_TRUE;
}

void
nsXBLBinding::WalkRules(nsIStyleRuleProcessor::EnumFunc aFunc, void* aData)
{
  if (mNextBinding)
    mNextBinding->WalkRules(aFunc, aData);

  nsIStyleRuleProcessor *rules = mPrototypeBinding->GetRuleProcessor();
  if (rules)
    (*aFunc)(rules, aData);
}

// Internal helper methods ////////////////////////////////////////////////////////////////

// static
nsresult
nsXBLBinding::DoInitJSClass(JSContext *cx, JSObject *global, JSObject *obj,
                            const nsAFlatCString& aClassName,
                            nsXBLPrototypeBinding* aProtoBinding,
                            void **aClassObject)
{
  // First ensure our JS class is initialized.
  jsval val;
  JSObject* proto = NULL;

  nsCAutoString className(aClassName);
  JSObject* parent_proto = nsnull;  // If we have an "obj" we can set this
  JSAutoRequest ar(cx);

  JSAutoEnterCompartment ac;
  if (!ac.enter(cx, global))
      return NS_ERROR_FAILURE;

  if (obj) {
    // Retrieve the current prototype of obj.
    parent_proto = ::JS_GetPrototype(cx, obj);
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
  }

  if ((!::JS_LookupPropertyWithFlags(cx, global, className.get(),
                                     JSRESOLVE_CLASSNAME,
                                     &val)) ||
      JSVAL_IS_PRIMITIVE(val)) {
    // We need to initialize the class.

    nsXBLJSClass* c;
    void* classObject;
    nsCStringKey key(className);
    classObject = (nsXBLService::gClassTable)->Get(&key);

    if (classObject) {
      c = static_cast<nsXBLJSClass*>(classObject);

      // If c is on the LRU list (i.e., not linked to itself), remove it now!
      JSCList* link = static_cast<JSCList*>(c);
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
        c = static_cast<nsXBLJSClass*>(lru);
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

    // Keep this proto binding alive while we're alive.  Do this first so that
    // we can guarantee that in XBLFinalize this will be non-null.
    // Note that we can't just store aProtoBinding in the private and
    // addref/release the nsXBLDocumentInfo through it, because cycle
    // collection doesn't seem to work right if the private is not an
    // nsISupports.
    nsXBLDocumentInfo* docInfo = aProtoBinding->XBLDocumentInfo();
    ::JS_SetPrivate(cx, proto, docInfo);
    NS_ADDREF(docInfo);

    if (!::JS_SetReservedSlot(cx, proto, 0, PRIVATE_TO_JSVAL(aProtoBinding))) {
      (nsXBLService::gClassTable)->Remove(&key);

      // |c| will get dropped when |proto| is finalized

      return NS_ERROR_OUT_OF_MEMORY;
    }

    *aClassObject = (void*)proto;
  }
  else {
    proto = JSVAL_TO_OBJECT(val);
  }

  if (obj) {
    // Set the prototype of our object to be the new class.
    if (!::JS_SetPrototype(cx, obj, proto)) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

PRBool
nsXBLBinding::AllowScripts()
{
  if (!mPrototypeBinding->GetAllowScripts())
    return PR_FALSE;

  // Nasty hack.  Use the JSContext of the bound node, since the
  // security manager API expects to get the docshell type from
  // that.  But use the nsIPrincipal of our document.
  nsIScriptSecurityManager* mgr = nsContentUtils::GetSecurityManager();
  if (!mgr) {
    return PR_FALSE;
  }

  nsIDocument* doc = mBoundElement ? mBoundElement->GetOwnerDoc() : nsnull;
  if (!doc) {
    return PR_FALSE;
  }

  nsIScriptGlobalObject* global = doc->GetScriptGlobalObject();
  if (!global) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIScriptContext> context = global->GetContext();
  if (!context) {
    return PR_FALSE;
  }
  
  JSContext* cx = (JSContext*) context->GetNativeContext();

  nsCOMPtr<nsIDocument> ourDocument =
    mPrototypeBinding->XBLDocumentInfo()->GetDocument();
  PRBool canExecute;
  nsresult rv =
    mgr->CanExecuteScripts(cx, ourDocument->NodePrincipal(), &canExecute);
  if (NS_FAILED(rv) || !canExecute) {
    return PR_FALSE;
  }

  // Now one last check: make sure that we're not allowing a privilege
  // escalation here.
  PRBool haveCert;
  doc->NodePrincipal()->GetHasCertificate(&haveCert);
  if (!haveCert) {
    return PR_TRUE;
  }

  PRBool subsumes;
  rv = ourDocument->NodePrincipal()->Subsumes(doc->NodePrincipal(), &subsumes);
  return NS_SUCCEEDED(rv) && subsumes;
}

void
nsXBLBinding::RemoveInsertionParent(nsIContent* aParent)
{
  if (mNextBinding) {
    mNextBinding->RemoveInsertionParent(aParent);
  }
  if (mInsertionPointTable) {
    nsInsertionPointList* list = nsnull;
    mInsertionPointTable->Get(aParent, &list);
    if (list) {
      PRInt32 count = list->Length();
      for (PRInt32 i = 0; i < count; ++i) {
        nsRefPtr<nsXBLInsertionPoint> currPoint = list->ElementAt(i);
        currPoint->UnbindDefaultContent();
#ifdef DEBUG
        nsCOMPtr<nsIContent> parent = currPoint->GetInsertionParent();
        NS_ASSERTION(!parent || parent == aParent, "Wrong insertion parent!");
#endif
        currPoint->ClearInsertionParent();
      }
      mInsertionPointTable->Remove(aParent);
    }
  }
}

PRBool
nsXBLBinding::HasInsertionParent(nsIContent* aParent)
{
  if (mInsertionPointTable) {
    nsInsertionPointList* list = nsnull;
    mInsertionPointTable->Get(aParent, &list);
    if (list) {
      return PR_TRUE;
    }
  }
  return mNextBinding ? mNextBinding->HasInsertionParent(aParent) : PR_FALSE;
}

nsresult
nsXBLBinding::GetInsertionPointsFor(nsIContent* aParent,
                                    nsInsertionPointList** aResult)
{
  if (!mInsertionPointTable) {
    mInsertionPointTable =
      new nsClassHashtable<nsISupportsHashKey, nsInsertionPointList>;
    if (!mInsertionPointTable || !mInsertionPointTable->Init(4)) {
      delete mInsertionPointTable;
      mInsertionPointTable = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  mInsertionPointTable->Get(aParent, aResult);

  if (!*aResult) {
    *aResult = new nsInsertionPointList;
    if (!*aResult || !mInsertionPointTable->Put(aParent, *aResult)) {
      delete *aResult;
      *aResult = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (aParent) {
      aParent->SetFlags(NODE_IS_INSERTION_PARENT);
    }
  }

  return NS_OK;
}

nsInsertionPointList*
nsXBLBinding::GetExistingInsertionPointsFor(nsIContent* aParent)
{
  if (!mInsertionPointTable) {
    return nsnull;
  }

  nsInsertionPointList* result = nsnull;
  mInsertionPointTable->Get(aParent, &result);
  return result;
}

nsIContent*
nsXBLBinding::GetInsertionPoint(const nsIContent* aChild, PRUint32* aIndex)
{
  if (mContent) {
    return mPrototypeBinding->GetInsertionPoint(mBoundElement, mContent,
                                                aChild, aIndex);
  }

  if (mNextBinding)
    return mNextBinding->GetInsertionPoint(aChild, aIndex);

  return nsnull;
}

nsIContent*
nsXBLBinding::GetSingleInsertionPoint(PRUint32* aIndex,
                                      PRBool* aMultipleInsertionPoints)
{
  *aMultipleInsertionPoints = PR_FALSE;
  if (mContent) {
    return mPrototypeBinding->GetSingleInsertionPoint(mBoundElement, mContent, 
                                                      aIndex, 
                                                      aMultipleInsertionPoints);
  }

  if (mNextBinding)
    return mNextBinding->GetSingleInsertionPoint(aIndex,
                                                 aMultipleInsertionPoints);

  return nsnull;
}

nsXBLBinding*
nsXBLBinding::RootBinding()
{
  if (mNextBinding)
    return mNextBinding->RootBinding();

  return this;
}

nsXBLBinding*
nsXBLBinding::GetFirstStyleBinding()
{
  if (mIsStyleBinding)
    return this;

  return mNextBinding ? mNextBinding->GetFirstStyleBinding() : nsnull;
}

PRBool
nsXBLBinding::ResolveAllFields(JSContext *cx, JSObject *obj) const
{
  if (!mPrototypeBinding->ResolveAllFields(cx, obj)) {
    return PR_FALSE;
  }

  if (mNextBinding) {
    return mNextBinding->ResolveAllFields(cx, obj);
  }

  return PR_TRUE;
}

void
nsXBLBinding::MarkForDeath()
{
  mMarkedForDeath = PR_TRUE;
  ExecuteDetachedHandler();
}

PRBool
nsXBLBinding::ImplementsInterface(REFNSIID aIID) const
{
  return mPrototypeBinding->ImplementsInterface(aIID) ||
    (mNextBinding && mNextBinding->ImplementsInterface(aIID));
}

nsINodeList*
nsXBLBinding::GetAnonymousNodes()
{
  if (mContent) {
    return mContent->GetChildNodesList();
  }

  if (mNextBinding)
    return mNextBinding->GetAnonymousNodes();

  return nsnull;
}

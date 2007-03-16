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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *  Allan Beaufour <abeaufour@novell.com>
 *  Darin Fisher <darin@meer.net>
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXFormsModelElement.h"
#include "nsIXTFElementWrapper.h"
#include "nsMemory.h"
#include "nsIDOMElement.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsXFormsAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIDOMEvent.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIXPathEvaluatorInternal.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIContent.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIXFormsControl.h"
#include "nsXFormsTypes.h"
#include "nsXFormsXPathParser.h"
#include "nsXFormsXPathAnalyzer.h"
#include "nsIInstanceElementPrivate.h"
#include "nsXFormsUtils.h"
#include "nsXFormsSchemaValidator.h"
#include "nsIXFormsUIWidget.h"
#include "nsIAttribute.h"
#include "nsISchemaLoader.h"
#include "nsISchema.h"
#include "nsAutoPtr.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIProgrammingLanguage.h"
#include "nsDOMError.h"
#include "nsXFormsControlStub.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIEventStateManager.h"
#include "nsStringEnumerator.h"

#define XFORMS_LAZY_INSTANCE_BINDING \
  "chrome://xforms/content/xforms.xml#xforms-lazy-instance"

#ifdef DEBUG
//#define DEBUG_MODEL
#endif

//------------------------------------------------------------------------------

// Helper function for using XPath to locate an <xsd:schema> element by
// matching its "id" attribute.  This is necessary since <xsd:schema> is
// treated as an ordinary XML data node without an "ID" attribute.
static void
GetSchemaElementById(nsIDOMElement *contextNode,
                     const nsString &id,
                     nsIDOMElement **resultNode)
{
  // search for an element with the given "id" attribute, and then verify
  // that the element is in the XML Schema namespace.

  nsAutoString expr;
  expr.AssignLiteral("//*[@id=\"");
  expr.Append(id);
  expr.AppendLiteral("\"]");

  nsCOMPtr<nsIDOMXPathResult> xpRes;
  nsresult rv =
    nsXFormsUtils::EvaluateXPath(expr,
                                 contextNode,
                                 contextNode,
                                 nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                 getter_AddRefs(xpRes));
  if (NS_SUCCEEDED(rv) && xpRes) {
    nsCOMPtr<nsIDOMNode> node;
    xpRes->GetSingleNodeValue(getter_AddRefs(node));
    if (node) {
      nsAutoString ns;
      node->GetNamespaceURI(ns);
      if (ns.EqualsLiteral(NS_NAMESPACE_XML_SCHEMA))
        CallQueryInterface(node, resultNode);
    }
  }
}

//------------------------------------------------------------------------------

static void
DeleteVoidArray(void    *aObject,
                nsIAtom *aPropertyName,
                void    *aPropertyValue,
                void    *aData)
{
  delete NS_STATIC_CAST(nsVoidArray *, aPropertyValue);
}

static nsresult
AddToModelList(nsIDOMDocument *domDoc, nsXFormsModelElement *model)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

  nsVoidArray *models =
      NS_STATIC_CAST(nsVoidArray *,
                     doc->GetProperty(nsXFormsAtoms::modelListProperty));
  if (!models) {
    models = new nsVoidArray(16);
    if (!models)
      return NS_ERROR_OUT_OF_MEMORY;
    doc->SetProperty(nsXFormsAtoms::modelListProperty, models, DeleteVoidArray);
  }
  models->AppendElement(model);
  return NS_OK;
}

static void
RemoveFromModelList(nsIDOMDocument *domDoc, nsXFormsModelElement *model)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

  nsVoidArray *models =
      NS_STATIC_CAST(nsVoidArray *,
                     doc->GetProperty(nsXFormsAtoms::modelListProperty));
  if (models)
    models->RemoveElement(model);
}

static const nsVoidArray *
GetModelList(nsIDOMDocument *domDoc)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);

  return NS_STATIC_CAST(nsVoidArray *,
                        doc->GetProperty(nsXFormsAtoms::modelListProperty));
}

static void
SupportsDtorFunc(void *aObject, nsIAtom *aPropertyName,
                 void *aPropertyValue, void *aData)
{
  nsISupports *propertyValue = NS_STATIC_CAST(nsISupports*, aPropertyValue);
  NS_IF_RELEASE(propertyValue);
}


//------------------------------------------------------------------------------
// --- nsXFormsControlListItem  ---


nsXFormsControlListItem::iterator::iterator()
  : mCur(0)
{
}

nsXFormsControlListItem::iterator::iterator(const nsXFormsControlListItem::iterator& aCopy)
  : mCur(aCopy.mCur)
{
  mStack = aCopy.mStack;
}

nsXFormsControlListItem::iterator
nsXFormsControlListItem::iterator::operator=(nsXFormsControlListItem* aCnt)
{
  mCur = aCnt;
  return *this;
}

bool
nsXFormsControlListItem::iterator::operator!=(const nsXFormsControlListItem* aCnt)
{
  return mCur != aCnt;
}

nsXFormsControlListItem::iterator
nsXFormsControlListItem::iterator::operator++()
{
  if (!mCur)
    return *this;

  if (mCur->mFirstChild) {
    if (!mCur->mNextSibling) {
      mCur = mCur->mFirstChild;
      return *this;
    }
    mStack.AppendElement(mCur->mFirstChild);
  }
    
  if (mCur->mNextSibling) {
    mCur = mCur->mNextSibling;
  } else if (mStack.Count()) {
    mCur = (nsXFormsControlListItem*) mStack[mStack.Count() - 1];
    mStack.RemoveElementAt(mStack.Count() - 1);
  } else {
    mCur = nsnull;
  }

  return *this;
}

nsXFormsControlListItem*
nsXFormsControlListItem::iterator::operator*()
{
  return mCur;
}

nsXFormsControlListItem::nsXFormsControlListItem(
  nsIXFormsControl* aControl,
  nsRefPtrHashtable<nsISupportsHashKey,nsXFormsControlListItem>* aHashtable)
  : mNode(aControl),
    mNextSibling(nsnull),
    mFirstChild(nsnull),
    mControlListHash(aHashtable)
{
  
}

nsXFormsControlListItem::~nsXFormsControlListItem()
{
  Clear();
}

nsXFormsControlListItem::nsXFormsControlListItem(const nsXFormsControlListItem& aCopy)
  : mNode(aCopy.mNode)
{
  if (aCopy.mNextSibling) {
    mNextSibling = new nsXFormsControlListItem(*aCopy.mNextSibling);
    NS_WARN_IF_FALSE(mNextSibling, "could not new?!");
  } else {
    mNextSibling = nsnull;
  }

  if (aCopy.mFirstChild) {
    mFirstChild = new nsXFormsControlListItem(*aCopy.mFirstChild);
    NS_WARN_IF_FALSE(mFirstChild, "could not new?!");
  } else {
    mFirstChild = nsnull;
  }
}

void
nsXFormsControlListItem::Clear()
{
  if (mFirstChild) {
    mFirstChild->Clear();
    NS_ASSERTION(!(mFirstChild->mFirstChild || mFirstChild->mNextSibling),
                 "child did not clear members!!");
    mFirstChild = nsnull;
  }
  if (mNextSibling) {
    mNextSibling->Clear();
    NS_ASSERTION(!(mNextSibling->mFirstChild || mNextSibling->mNextSibling),
                 "sibling did not clear members!!");
    mNextSibling = nsnull;
  }
  if (mNode) {
    /* we won't bother removing each item one by one from the hashtable.  This
     * approach assumes that we are clearing the whole model's list of controls
     * due to the model going away.  After the model clears this list, it will
     * clear the hashtable all at once.
     */
    mControlListHash = nsnull;
    mNode = nsnull;
  }
}

nsresult
nsXFormsControlListItem::AddControl(nsIXFormsControl *aControl,
                                    nsIXFormsControl *aParent)
{
  // Four insertion posibilities:

  // 1) Delegate to first child from root node
  if (!mNode && mFirstChild) {
    return mFirstChild->AddControl(aControl, aParent);
  }

  // 2) control with no parent
  if (!aParent) {
    nsRefPtr<nsXFormsControlListItem> newNode =
      new nsXFormsControlListItem(aControl, mControlListHash);
    NS_ENSURE_TRUE(newNode, NS_ERROR_OUT_OF_MEMORY);

    // Empty tree (we have already checked mFirstChild)
    if (!mNode) {
      mFirstChild = newNode;
      nsCOMPtr<nsIDOMElement> ele;
      aControl->GetElement(getter_AddRefs(ele));
      mControlListHash->Put(ele, newNode);
      return NS_OK;
    }

    if (mNextSibling) {
      newNode->mNextSibling = mNextSibling;
    }
    mNextSibling = newNode;
    nsCOMPtr<nsIDOMElement> ele;
    aControl->GetElement(getter_AddRefs(ele));
    mControlListHash->Put(ele, newNode);
#ifdef DEBUG
    nsXFormsControlListItem* next = newNode->mNextSibling;
    while (next) {
      NS_ASSERTION(aControl != next->mNode,
                   "Node already in tree!!");
      next = next->mNextSibling;
    }
#endif

    return NS_OK;
  }

  // Locate parent
  nsXFormsControlListItem* parentControl = FindControl(aParent);
  NS_ASSERTION(parentControl, "Parent not found?!");

  // 3) parentControl has a first child, insert as sibling to that
  if (parentControl->mFirstChild) {
    return parentControl->mFirstChild->AddControl(aControl, nsnull);
  }

  // 4) first child for parentControl
  nsRefPtr<nsXFormsControlListItem> newNode =
    new nsXFormsControlListItem(aControl, mControlListHash);
  NS_ENSURE_TRUE(newNode, NS_ERROR_OUT_OF_MEMORY);

  parentControl->mFirstChild = newNode;
  nsCOMPtr<nsIDOMElement> ele;
  aControl->GetElement(getter_AddRefs(ele));
  mControlListHash->Put(ele, newNode);

  return NS_OK;
}

nsresult
nsXFormsControlListItem::RemoveControl(nsIXFormsControl *aControl,
                                       PRBool           &aRemoved)
{
  nsXFormsControlListItem* deleteMe = nsnull;
  aRemoved = PR_FALSE;

  // Try children
  if (mFirstChild) {
    // The control to remove is our first child
    if (mFirstChild->mNode == aControl) {
      deleteMe = mFirstChild;

      // Fix siblings
      if (deleteMe->mNextSibling) {
        mFirstChild = deleteMe->mNextSibling;
        deleteMe->mNextSibling = nsnull;
      } else {
        mFirstChild = nsnull;
      }

      // Fix children
      if (deleteMe->mFirstChild) {
        if (!mFirstChild) {
          mFirstChild = deleteMe->mFirstChild;
        } else {
          nsXFormsControlListItem *insertPos = mFirstChild;
          while (insertPos->mNextSibling) {
            insertPos = insertPos->mNextSibling;
          }
          insertPos->mNextSibling = deleteMe->mFirstChild;
        }
        deleteMe->mFirstChild = nsnull;
      }
    } else {
      // Run through children
      nsresult rv = mFirstChild->RemoveControl(aControl, aRemoved);
      NS_ENSURE_SUCCESS(rv, rv);
      if (aRemoved)
        return rv;
    }
  }

  // Try siblings
  if (!deleteMe && mNextSibling) {
    if (mNextSibling->mNode == aControl) {
      deleteMe = mNextSibling;
      // Fix siblings
      if (deleteMe->mNextSibling) {
        mNextSibling = deleteMe->mNextSibling;
        deleteMe->mNextSibling = nsnull;
      } else {
        mNextSibling = nsnull;
      }
      // Fix children
      if (deleteMe->mFirstChild) {
        if (!mNextSibling) {
          mNextSibling = deleteMe->mFirstChild;
        } else {
          nsXFormsControlListItem *insertPos = mNextSibling;
          while (insertPos->mNextSibling) {
            insertPos = insertPos->mNextSibling;
          }
          insertPos->mNextSibling = deleteMe->mFirstChild;
        }
        deleteMe->mFirstChild = nsnull;
      }
    } else {
      // run through siblings
      return mNextSibling->RemoveControl(aControl, aRemoved);
    }
  }

  if (deleteMe) {
    NS_ASSERTION(!(deleteMe->mNextSibling),
                 "Deleted control should not have siblings!");
    NS_ASSERTION(!(deleteMe->mFirstChild),
                 "Deleted control should not have children!");
    nsCOMPtr<nsIDOMElement> element;
    deleteMe->mNode->GetElement(getter_AddRefs(element));
    mControlListHash->Remove(element);
    aRemoved = PR_TRUE;
  }

  return NS_OK;
}

nsXFormsControlListItem*
nsXFormsControlListItem::FindControl(nsIXFormsControl *aControl)
{
  if (!aControl)
    return nsnull;

  nsXFormsControlListItem *listItem;
  nsCOMPtr<nsIDOMElement> element;
  aControl->GetElement(getter_AddRefs(element));
  if (mControlListHash->Get(element, &listItem)) {
    return listItem;
  }

  return nsnull;
}

already_AddRefed<nsIXFormsControl>
nsXFormsControlListItem::Control()
{
  nsIXFormsControl* res = nsnull;
  if (mNode)
    NS_ADDREF(res = mNode);
  NS_WARN_IF_FALSE(res, "Returning nsnull for a control. Bad sign.");
  return res;
}

nsXFormsControlListItem*
nsXFormsControlListItem::begin()
{
  // handle root
  if (!mNode)
    return mFirstChild;

  return this;
}

nsXFormsControlListItem*
nsXFormsControlListItem::end()
{
  return nsnull;
}


//------------------------------------------------------------------------------

static const nsIID sScriptingIIDs[] = {
  NS_IXFORMSMODELELEMENT_IID,
  NS_IXFORMSNSMODELELEMENT_IID
};

static nsIAtom* sModelPropsList[eModel__count];

// This can be nsVoidArray because elements will remove
// themselves from the list if they are deleted during refresh.
static nsVoidArray* sPostRefreshList = nsnull;
static nsVoidArray* sContainerPostRefreshList = nsnull;

static PRInt32 sRefreshing = 0;

nsPostRefresh::nsPostRefresh()
{
#ifdef DEBUG_smaug
  printf("nsPostRefresh\n");
#endif
  ++sRefreshing;
}

nsPostRefresh::~nsPostRefresh()
{
#ifdef DEBUG_smaug
  printf("~nsPostRefresh\n");
#endif

  if (sRefreshing != 1) {
    --sRefreshing;
    return;
  }

  if (sPostRefreshList) {
    while (sPostRefreshList->Count()) {
      // Iterating this way because refresh can lead to
      // additions/deletions in sPostRefreshList.
      // Iterating from last to first saves possibly few memcopies,
      // see nsVoidArray::RemoveElementsAt().
      PRInt32 last = sPostRefreshList->Count() - 1;
      nsIXFormsControl* control =
        NS_STATIC_CAST(nsIXFormsControl*, sPostRefreshList->ElementAt(last));
      sPostRefreshList->RemoveElementAt(last);
      if (control)
        control->Refresh();
    }
    if (sRefreshing == 1) {
      delete sPostRefreshList;
      sPostRefreshList = nsnull;
    }
  }

  --sRefreshing;

  // process sContainerPostRefreshList after we've decremented sRefreshing.
  // container->refresh below could ask for ContainerNeedsPostRefresh which
  // will add an item to the sContainerPostRefreshList if sRefreshing > 0.
  // So keeping this under sRefreshing-- will avoid an infinite loop.
  while (sContainerPostRefreshList && sContainerPostRefreshList->Count()) {
    PRInt32 last = sContainerPostRefreshList->Count() - 1;
    nsIXFormsControl* container =
      NS_STATIC_CAST(nsIXFormsControl*, sContainerPostRefreshList->ElementAt(last));
    sContainerPostRefreshList->RemoveElementAt(last);
    if (container) {
      container->Refresh();
    }
  }
  delete sContainerPostRefreshList;
  sContainerPostRefreshList = nsnull;
}

const nsVoidArray* 
nsPostRefresh::PostRefreshList()
{
  return sPostRefreshList;
}

nsresult
nsXFormsModelElement::NeedsPostRefresh(nsIXFormsControl* aControl)
{
  if (sRefreshing) {
    if (!sPostRefreshList) {
      sPostRefreshList = new nsVoidArray();
      NS_ENSURE_TRUE(sPostRefreshList, NS_ERROR_OUT_OF_MEMORY);
    }

    if (sPostRefreshList->IndexOf(aControl) < 0) {
      sPostRefreshList->AppendElement(aControl);
    }
  } else {
    // We are not refreshing any models, so the control
    // can be refreshed immediately.
    aControl->Refresh();
  }
  return NS_OK;
}

PRBool
nsXFormsModelElement::ContainerNeedsPostRefresh(nsIXFormsControl* aControl)
{

  if (sRefreshing) {
    if (!sContainerPostRefreshList) {
      sContainerPostRefreshList = new nsVoidArray();
      if (!sContainerPostRefreshList) {
        return PR_FALSE;
      }
    }

    if (sContainerPostRefreshList->IndexOf(aControl) < 0) {
      sContainerPostRefreshList->AppendElement(aControl);
    }

    // return PR_TRUE to show that the control's refresh will be delayed,
    // whether as a result of this call or a previous call to this function.
    return PR_TRUE;
  }

  // Delaying the refresh doesn't make any sense.  But since this
  // function may be called from inside the control node's refresh already,
  // we shouldn't just assume that we can call the refresh here.  So
  // we'll just return PR_FALSE to signal that we couldn't delay the refresh.

  return PR_FALSE;
}

void
nsXFormsModelElement::CancelPostRefresh(nsIXFormsControl* aControl)
{
  if (sPostRefreshList)
    sPostRefreshList->RemoveElement(aControl);

  if (sContainerPostRefreshList)
    sContainerPostRefreshList->RemoveElement(aControl);
}

nsXFormsModelElement::nsXFormsModelElement()
  : mElement(nsnull),
    mFormControls(nsnull, &mControlListHash),
    mSchemaCount(0),
    mSchemaTotal(0),
    mPendingInstanceCount(0),
    mDocumentLoaded(PR_FALSE),
    mRebindAllControls(PR_FALSE),
    mInstancesInitialized(PR_FALSE),
    mReadyHandled(PR_FALSE),
    mLazyModel(PR_FALSE),
    mConstructDoneHandled(PR_FALSE),
    mProcessingUpdateEvent(PR_FALSE),
    mLoopMax(600),
    mInstanceDocuments(nsnull)
{
  mControlListHash.Init();
}

NS_INTERFACE_MAP_BEGIN(nsXFormsModelElement)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsModelElement)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsNSModelElement)
  NS_INTERFACE_MAP_ENTRY(nsIModelElementPrivate)
  NS_INTERFACE_MAP_ENTRY(nsISchemaLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebServiceErrorHandler)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsContextControl)
NS_INTERFACE_MAP_END_INHERITING(nsXFormsStubElement)

NS_IMPL_ADDREF_INHERITED(nsXFormsModelElement, nsXFormsStubElement)
NS_IMPL_RELEASE_INHERITED(nsXFormsModelElement, nsXFormsStubElement)

NS_IMETHODIMP
nsXFormsModelElement::OnDestroyed()
{
  mElement = nsnull;
  mSchemas = nsnull;

  if (mInstanceDocuments)
    mInstanceDocuments->DropReferences();

  mFormControls.Clear();
  mControlListHash.Clear();

  return NS_OK;
}

void
nsXFormsModelElement::RemoveModelFromDocument()
{
  mDocumentLoaded = PR_FALSE;

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  if (!domDoc)
    return;

  RemoveFromModelList(domDoc, this);

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(domDoc);
  if (targ) {
    targ->RemoveEventListener(NS_LITERAL_STRING("DOMContentLoaded"), this, PR_TRUE);

    nsCOMPtr<nsIDOMWindowInternal> window;
    nsXFormsUtils::GetWindowFromDocument(domDoc, getter_AddRefs(window));
    targ = do_QueryInterface(window);
    if (targ) {
      targ->RemoveEventListener(NS_LITERAL_STRING("unload"), this, PR_TRUE);
    }
  }
}

NS_IMETHODIMP
nsXFormsModelElement::GetScriptingInterfaces(PRUint32 *aCount, nsIID ***aArray)
{
  return nsXFormsUtils::CloneScriptingInterfaces(sScriptingIIDs,
                                                 NS_ARRAY_LENGTH(sScriptingIIDs),
                                                 aCount, aArray);
}

NS_IMETHODIMP
nsXFormsModelElement::WillChangeDocument(nsIDOMDocument* aNewDocument)
{
  RemoveModelFromDocument();
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::DocumentChanged(nsIDOMDocument* aNewDocument)
{
  if (!aNewDocument)
    return NS_OK;

  AddToModelList(aNewDocument, this);

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(aNewDocument);
  if (targ) {
    targ->AddEventListener(NS_LITERAL_STRING("DOMContentLoaded"), this, PR_TRUE);

    nsCOMPtr<nsIDOMWindowInternal> window;
    nsXFormsUtils::GetWindowFromDocument(aNewDocument, getter_AddRefs(window));
    targ = do_QueryInterface(window);
    if (targ) {
      targ->AddEventListener(NS_LITERAL_STRING("unload"), this, PR_TRUE);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::DoneAddingChildren()
{
  return InitializeInstances();
}

nsresult
nsXFormsModelElement::InitializeInstances()
{
  if (mInstancesInitialized || !mElement) {
    return NS_OK;
  }

  mInstancesInitialized = PR_TRUE;

  nsCOMPtr<nsIDOMNodeList> children;
  mElement->GetChildNodes(getter_AddRefs(children));

  PRUint32 childCount = 0;
  if (children) {
    children->GetLength(&childCount);
  }

  nsresult rv;
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDOMNode> child;
    children->Item(i, getter_AddRefs(child));
    if (nsXFormsUtils::IsXFormsElement(child, NS_LITERAL_STRING("instance"))) {
      nsCOMPtr<nsIInstanceElementPrivate> instance(do_QueryInterface(child));
      NS_ENSURE_STATE(instance);
      rv = instance->Initialize();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // (XForms 4.2.1)
  // 1. load xml schemas

  nsAutoString schemaList;
  mElement->GetAttribute(NS_LITERAL_STRING("schema"), schemaList);

  if (!schemaList.IsEmpty()) {
    NS_ENSURE_TRUE(mSchemas, NS_ERROR_FAILURE);
    // Parse the whitespace-separated list.
    nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
    nsRefPtr<nsIURI> baseURI = content->GetBaseURI();
    nsRefPtr<nsIURI> docURI = content->GetOwnerDoc() ?
      content->GetOwnerDoc()->GetDocumentURI() : nsnull;

    nsCStringArray schemas;
    schemas.ParseString(NS_ConvertUTF16toUTF8(schemaList).get(), " \t\r\n");

    // Increase by 1 to prevent OnLoad from calling FinishConstruction
    mSchemaTotal = schemas.Count();

    for (PRInt32 i=0; i<mSchemaTotal; ++i) {
      rv = NS_OK;
      nsCOMPtr<nsIURI> newURI;
      NS_NewURI(getter_AddRefs(newURI), *schemas[i], nsnull, baseURI);
      nsCOMPtr<nsIURL> newURL = do_QueryInterface(newURI);
      if (!newURL) {
        rv = NS_ERROR_UNEXPECTED;
      } else {
        // This code is copied from nsXMLEventsManager for extracting an
        // element ID from an xsd:anyURI link.
        nsCAutoString ref;
        newURL->GetRef(ref);
        newURL->SetRef(EmptyCString());
        PRBool equals = PR_FALSE;
        newURL->Equals(docURI, &equals);
        if (equals) {
          // We will not be able to locate the <xsd:schema> element using the
          // getElementById function defined on our document when <xsd:schema>
          // is treated as an ordinary XML data node.  So, we employ XPath to
          // locate it for us.

          NS_ConvertUTF8toUTF16 id(ref);

          nsCOMPtr<nsIDOMElement> el;
          GetSchemaElementById(mElement, id, getter_AddRefs(el));
          if (!el) {
            // Perhaps the <xsd:schema> element appears after the <xforms:model>
            // element in the document, so we'll defer loading it until the
            // document has finished loading.
            mPendingInlineSchemas.AppendString(id);
          } else {
            // We have an inline schema in the model element that was
            // referenced by the schema attribute. It will be processed
            // in FinishConstruction so we skip it now to avoid processing
            // it twice and giving invalid 'duplicate schema' errors.
            mSchemaTotal--;
            i--;
          }
        } else {
          nsCAutoString uriSpec;
          newURI->GetSpec(uriSpec);
          rv = mSchemas->LoadAsync(NS_ConvertUTF8toUTF16(uriSpec), this);
        }
      }
      if (NS_FAILED(rv)) {
        // this is a fatal error
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("schemaLoadError"), mElement);
        nsXFormsUtils::DispatchEvent(mElement, eEvent_LinkException);
        return NS_OK;
      }
    }
  }

  // If all of the children are added and there aren't any instance elements,
  // yet, then we need to make sure that one is ready in case the form author
  // is using lazy authoring.
  // Lazy <xforms:intance> element is created in anonymous content using XBL.
  NS_ENSURE_STATE(mInstanceDocuments);
  PRUint32 instCount;
  mInstanceDocuments->GetLength(&instCount);
  if (!instCount) {
#ifdef DEBUG
    printf("Creating lazy instance\n");
#endif
    nsCOMPtr<nsIDOMDocument> domDoc;
    mElement->GetOwnerDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(domDoc));
    if (xblDoc) {
      nsresult rv =
        xblDoc->AddBinding(mElement,
                           NS_LITERAL_STRING(XFORMS_LAZY_INSTANCE_BINDING));
      NS_ENSURE_SUCCESS(rv, rv);

      mInstanceDocuments->GetLength(&instCount);

      nsCOMPtr<nsIDOMNodeList> list;
      xblDoc->GetAnonymousNodes(mElement, getter_AddRefs(list));
      if (list) {
        PRUint32 childCount = 0;
        if (list) {
          list->GetLength(&childCount);
        }

        for (PRUint32 i = 0; i < childCount; ++i) {
          nsCOMPtr<nsIDOMNode> item;
          list->Item(i, getter_AddRefs(item));
          nsCOMPtr<nsIInstanceElementPrivate> instance =
            do_QueryInterface(item);
          if (instance) {
            rv = instance->Initialize();
            NS_ENSURE_SUCCESS(rv, rv);

            mLazyModel = PR_TRUE;
            break;
          }
        }
      }
    }
    NS_WARN_IF_FALSE(mLazyModel, "Installing lazy instance didn't succeed!");
  }

  // (XForms 4.2.1 - cont)
  // 2. construct an XPath data model from inline or external initial instance
  // data.  This is done by our child instance elements as they are inserted
  // into the document, and all of the instances will be processed by this
  // point.

  // schema and external instance data loads should delay document onload

  if (IsComplete()) {
    // No need to fire refresh event if we assume that all UI controls
    // appear later in the document.
    NS_ASSERTION(!mDocumentLoaded, "document should not be loaded yet");
    return FinishConstruction();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  if (!nsXFormsUtils::EventHandlingAllowed(aEvent, mElement))
    return NS_OK;

  *aHandled = PR_TRUE;

  nsAutoString type;
  aEvent->GetType(type);
  nsresult rv = NS_OK;

  if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Refresh].name)) {
    rv = Refresh();
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Revalidate].name)) {
    rv = Revalidate();
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Recalculate].name)) {
    rv = Recalculate();
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Rebuild].name)) {
    rv = Rebuild();
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_ModelConstructDone].name)) {
    rv = ConstructDone();
    mConstructDoneHandled = PR_TRUE;
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Reset].name)) {
    Reset();
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_BindingException].name)) {
    // we threw up a popup during the nsXFormsUtils::DispatchEvent that sent
    // this error to the model
    *aHandled = PR_TRUE;
  } else {
    *aHandled = PR_FALSE;
  }

  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "nsXFormsModelElement::HandleDefault() failed!\n");

  return rv;
}

nsresult
nsXFormsModelElement::ConstructDone()
{
  nsresult rv = InitializeControls();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::OnCreated(nsIXTFElementWrapper *aWrapper)
{
  aWrapper->SetNotificationMask(nsIXTFElement::NOTIFY_WILL_CHANGE_DOCUMENT |
                                nsIXTFElement::NOTIFY_DOCUMENT_CHANGED |
                                nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN |
                                nsIXTFElement::NOTIFY_HANDLE_DEFAULT);

  nsCOMPtr<nsIDOMElement> node;
  aWrapper->GetElementNode(getter_AddRefs(node));

  // It's ok to keep a weak pointer to mElement.  mElement will have an
  // owning reference to this object, so as long as we null out mElement in
  // OnDestroyed, it will always be valid.

  mElement = node;
  NS_ASSERTION(mElement, "Wrapper is not an nsIDOMElement, we'll crash soon");

  nsresult rv = mMDG.Init(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mSchemas = do_CreateInstance(NS_SCHEMALOADER_CONTRACTID);

  mInstanceDocuments = new nsXFormsModelInstanceDocuments();
  NS_ASSERTION(mInstanceDocuments, "could not create mInstanceDocuments?!");

  // Initialize hash tables
  NS_ENSURE_TRUE(mNodeToType.Init(), NS_ERROR_OUT_OF_MEMORY);
  NS_ENSURE_TRUE(mNodeToP3PType.Init(), NS_ERROR_OUT_OF_MEMORY);


  // Get eventual user-set loop maximum. Used by RequestUpdateEvent().
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && pref) {
    PRInt32 val;
    if (NS_SUCCEEDED(pref->GetIntPref("xforms.modelLoopMax", &val)))
      mLoopMax = val;
  }

  return NS_OK;
}

// nsIXFormsModelElement

NS_IMETHODIMP
nsXFormsModelElement::GetInstanceDocuments(nsIDOMNodeList **aDocuments)
{
  NS_ENSURE_STATE(mInstanceDocuments);
  NS_ENSURE_ARG_POINTER(aDocuments);
  NS_ADDREF(*aDocuments = mInstanceDocuments);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetInstanceDocument(const nsAString& aInstanceID,
                                          nsIDOMDocument **aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  *aDocument = FindInstanceDocument(aInstanceID).get();  // transfer reference

  if (*aDocument) {
    return NS_OK;
  }
  
  const nsPromiseFlatString& flat = PromiseFlatString(aInstanceID);
  const PRUnichar *strings[] = { flat.get() };
  nsXFormsUtils::ReportError(aInstanceID.IsEmpty() ?
                               NS_LITERAL_STRING("defInstanceNotFound") :
                               NS_LITERAL_STRING("instanceNotFound"),
                             strings, 1, mElement, nsnull);
  return NS_ERROR_DOM_NOT_FOUND_ERR;
}

NS_IMETHODIMP
nsXFormsModelElement::Rebuild()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Rebuild()\n");
#endif

  // 1 . Clear graph
  nsresult rv;
  rv = mMDG.Clear();
  NS_ENSURE_SUCCESS(rv, rv);

  // Clear any type information
  NS_ENSURE_TRUE(mNodeToType.IsInitialized() && mNodeToP3PType.IsInitialized(),
                 NS_ERROR_FAILURE);
  mNodeToType.Clear();
  mNodeToP3PType.Clear();

  // 2. Process bind elements
  rv = ProcessBindElements();
  NS_ENSURE_SUCCESS(rv, rv);

  // 3. If this is not form load, re-attach all elements and validate
  //    instance documents
  if (mReadyHandled) {
    mRebindAllControls = PR_TRUE;
    ValidateInstanceDocuments();
  }

  // 4. Rebuild graph
  return mMDG.Rebuild();
}

NS_IMETHODIMP
nsXFormsModelElement::Recalculate()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Recalculate()\n");
#endif
  
  return mMDG.Recalculate(&mChangedNodes);
}

void
nsXFormsModelElement::SetSingleState(nsIDOMElement *aElement,
                                     PRBool         aState,
                                     nsXFormsEvent  aOnEvent)
{
  nsXFormsEvent event = aState ? aOnEvent : (nsXFormsEvent) (aOnEvent + 1);

  // Dispatch event
  nsXFormsUtils::DispatchEvent(aElement, event);
}

NS_IMETHODIMP
nsXFormsModelElement::SetStates(nsIXFormsControl *aControl,
                                nsIDOMNode       *aNode)
{
  NS_ENSURE_ARG(aControl);
  
  nsCOMPtr<nsIDOMElement> element;
  aControl->GetElement(getter_AddRefs(element));
  NS_ENSURE_STATE(element);

  nsCOMPtr<nsIXTFElementWrapper> xtfWrap(do_QueryInterface(element));
  NS_ENSURE_STATE(xtfWrap);

  PRInt32 iState;
  const nsXFormsNodeState* ns = nsnull;
  if (aNode) {
    ns = mMDG.GetNodeState(aNode);
    NS_ENSURE_STATE(ns);
    iState = ns->GetIntrinsicState();
    nsCOMPtr<nsIContent> content(do_QueryInterface(element));
    NS_ENSURE_STATE(content);
    PRInt32 rangeState = content->IntrinsicState() &
                           (NS_EVENT_STATE_INRANGE | NS_EVENT_STATE_OUTOFRANGE);
    iState = ns->GetIntrinsicState() | rangeState;
  } else {
    aControl->GetDefaultIntrinsicState(&iState);
  }
  
  nsresult rv = xtfWrap->SetIntrinsicState(iState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Event dispatching is defined by the bound node, so if there's no bound
  // node, there are no events to send. xforms-ready also needs to be handled,
  // because these events are not sent before that.
  if (!ns || !mReadyHandled)
    return NS_OK;

  if (ns->ShouldDispatchValid()) {
    SetSingleState(element, ns->IsValid(), eEvent_Valid);
  }
  if (ns->ShouldDispatchReadonly()) {
    SetSingleState(element, ns->IsReadonly(), eEvent_Readonly);
  }
  if (ns->ShouldDispatchRequired()) {
    SetSingleState(element, ns->IsRequired(), eEvent_Required);
  }
  if (ns->ShouldDispatchRelevant()) {
    SetSingleState(element, ns->IsRelevant(), eEvent_Enabled);
  }

  if (ns->ShouldDispatchValueChanged()) {
    nsXFormsUtils::DispatchEvent(element, eEvent_ValueChanged);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::Revalidate()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Revalidate()\n");
#endif

#ifdef DEBUG_MODEL
  printf("[%s] Changed nodes:\n", __TIME__);
  for (PRInt32 j = 0; j < mChangedNodes.Count(); ++j) {
    nsCOMPtr<nsIDOMNode> node = mChangedNodes[j];
    nsAutoString name;
    node->GetNodeName(name);
    printf("\t%s [%p]\n",
           NS_ConvertUTF16toUTF8(name).get(),
           (void*) node);
  }
#endif

  // Revalidate nodes
  mMDG.Revalidate(&mChangedNodes);

  return NS_OK;
}

nsresult
nsXFormsModelElement::RefreshSubTree(nsXFormsControlListItem *aCurrent,
                                     PRBool                   aForceRebind)
{
  nsresult rv;
  nsRefPtr<nsXFormsControlListItem> current = aCurrent;
  while (current) {
    nsCOMPtr<nsIXFormsControl> control(current->Control());
    NS_ASSERTION(control, "A tree node without a control?!");
    
    // Get bound node
    nsCOMPtr<nsIDOMNode> boundNode;
    control->GetBoundNode(getter_AddRefs(boundNode));

    PRBool rebind = aForceRebind;
    PRBool refresh = PR_FALSE;
    PRBool rebindChildren = PR_FALSE;

#ifdef DEBUG_MODEL
      nsCOMPtr<nsIDOMElement> controlElement;
      control->GetElement(getter_AddRefs(controlElement));
      printf("rebind: %d, mRebindAllControls: %d, aForceRebind: %d\n",
             rebind, mRebindAllControls, aForceRebind);
      if (controlElement) {
        printf("Checking control: ");
        //DBG_TAGINFO(controlElement);
      }
#endif

    if (mRebindAllControls || rebind) {
      refresh = rebind = PR_TRUE;
    } else {
      PRBool usesModelBinding = PR_FALSE;
      control->GetUsesModelBinding(&usesModelBinding);

#ifdef DEBUG_MODEL
      printf("usesModelBinding: %d\n", usesModelBinding);
#endif

      nsCOMArray<nsIDOMNode> *deps = nsnull;
      if (usesModelBinding) {
        if (!boundNode) {
          PRBool usesSNB = PR_TRUE;
          control->GetUsesSingleNodeBinding(&usesSNB);
      
          // If the control doesn't use single node binding (and can thus be
          // bound to many nodes), the above test for boundNode means nothing.
          // We'll need to continue on with the work this function does so that
          // any controls that this control contains can be tested for whether
          // they may need to refresh.
          if (usesSNB) {
            // If a control uses a model binding, but has no bound node a
            // rebuild is the only thing that'll (eventually) change it.  We
            // don't need to worry about contained controls (like a label)
            // since the fact that there is no bound node means that this
            // control (and contained controls) need to behave as if
            // irrelevant per spec.
            current = current->NextSibling();
            continue;
          }
        }
      } else {
        // Get dependencies
        control->GetDependencies(&deps);
      }
      PRUint32 depCount = deps ? deps->Count() : 0;
        
#ifdef DEBUG_MODEL
      nsAutoString boundName;
      if (boundNode)
        boundNode->GetNodeName(boundName);
      printf("\tDependencies: %d, Bound to: '%s' [%p]\n",
             depCount,
             NS_ConvertUTF16toUTF8(boundName).get(),
             (void*) boundNode);

      nsAutoString depNodeName;
      for (PRUint32 t = 0; t < depCount; ++t) {
        nsCOMPtr<nsIDOMNode> tmpdep = deps->ObjectAt(t);
        if (tmpdep) {
          tmpdep->GetNodeName(depNodeName);
          printf("\t\t%s [%p]\n",
                 NS_ConvertUTF16toUTF8(depNodeName).get(),
                 (void*) tmpdep);
        }
      }
#endif

      nsCOMPtr<nsIDOM3Node> curChanged;

      // Iterator over changed nodes. Checking for rebind, too.  If it ever
      // becomes true due to some condition below, we can stop this testing
      // since any control that needs to rebind will also refresh.
      for (PRInt32 j = 0; j < mChangedNodes.Count() && !rebind; ++j) {
        curChanged = do_QueryInterface(mChangedNodes[j]);

        // Check whether the bound node is dirty. If so, we need to refresh the
        // control (get updated node value from the bound node)
        if (!refresh && boundNode) {
          curChanged->IsSameNode(boundNode, &refresh);

          // Two ways to go here. Keep in mind that controls using model
          // binding expressions never needs to have dependencies checked as
          // they only rebind on xforms-rebuild
          if (refresh && usesModelBinding) {
            // 1) If the control needs a refresh, and uses model bindings,
            // we can stop checking here
            break;
          }
          if (refresh || usesModelBinding) {
            // 2) If either the control needs a refresh or it uses a model
            // binding we can continue to next changed node
            continue;
          }
        }

        // Check whether any dependencies are dirty. If so, we need to rebind
        // the control (re-evaluate it's binding expression)
        for (PRUint32 k = 0; k < depCount; ++k) {
          /// @note beaufour: I'm not too happy about this ...
          /// O(mChangedNodes.Count() * deps->Count()), but using the pointers
          /// for sorting and comparing does not work...
          curChanged->IsSameNode(deps->ObjectAt(k), &rebind);
          if (rebind)
            // We need to rebind the control, no need to check any more
            break;
        }
      }
#ifdef DEBUG_MODEL
      printf("\trebind: %d, refresh: %d\n", rebind, refresh);
#endif    
    }

    // Handle rebinding
    if (rebind) {
      rv = control->Bind(&rebindChildren);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Handle refreshing
    if (rebind || refresh) {
      control->Refresh();
      // XXX: bug 336608: we should really check the return result, but
      // f.x. select1 returns error because of no widget...?  so we should
      // ensure that an error is only returned when there actually is an
      // error, and we should report that on the console... possibly we should
      // then continue, instead of bailing totally.
      // NS_ENSURE_SUCCESS(rv, rv);
    }

    // Refresh children
    rv = RefreshSubTree(current->FirstChild(), rebindChildren);
    NS_ENSURE_SUCCESS(rv, rv);

    current = current->NextSibling();
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXFormsModelElement::Refresh()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Refresh()\n");
#endif

  // XXXbeaufour: Can we somehow suspend redraw / "screen update" while doing
  // the refresh? That should save a lot of time, and avoid flickering of
  // controls.

  // Using brackets here to provide a scope for the
  // nsPostRefresh.  We want to make sure that nsPostRefresh's destructor
  // runs (and thus processes the postrefresh and containerpostrefresh lists)
  // before we clear the dispatch flags
  {
    nsPostRefresh postRefresh = nsPostRefresh();
  
    if (!mDocumentLoaded) {
      return NS_OK;
    }
  
    // Kick off refreshing on root node
    nsresult rv = RefreshSubTree(mFormControls.FirstChild(), PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Clear refresh structures
  mChangedNodes.Clear();
  mRebindAllControls = PR_FALSE;
  mMDG.ClearDispatchFlags();

  return NS_OK;
}

// nsISchemaLoadListener

NS_IMETHODIMP
nsXFormsModelElement::OnLoad(nsISchema* aSchema)
{
  mSchemaCount++;

  // If there is no model element, then schema loading finished after
  // main page failed to load.
  if (IsComplete() && mElement) {
    nsresult rv = FinishConstruction();
    NS_ENSURE_SUCCESS(rv, rv);

    MaybeNotifyCompletion();
  }

  return NS_OK;
}

// nsIWebServiceErrorHandler

NS_IMETHODIMP
nsXFormsModelElement::OnError(nsresult aStatus,
                              const nsAString &aStatusMessage)
{
  nsXFormsUtils::ReportError(NS_LITERAL_STRING("schemaLoadError"), mElement);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_LinkException);
  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsModelElement::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!nsXFormsUtils::EventHandlingAllowed(aEvent, mElement))
    return NS_OK;

  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("DOMContentLoaded")) {
    return HandleLoad(aEvent);
  }else if (type.EqualsLiteral("unload")) {
    return HandleUnload(aEvent);
  }

  return NS_OK;
}

// nsIModelElementPrivate

NS_IMETHODIMP
nsXFormsModelElement::AddFormControl(nsIXFormsControl *aControl,
                                     nsIXFormsControl *aParent)
{
#ifdef DEBUG_MODEL
  printf("nsXFormsModelElement::AddFormControl(con: %p, parent: %p)\n",
         (void*) aControl, (void*) aParent);
#endif

  NS_ENSURE_ARG(aControl);
  return mFormControls.AddControl(aControl, aParent);
}

NS_IMETHODIMP
nsXFormsModelElement::RemoveFormControl(nsIXFormsControl *aControl)
{
#ifdef DEBUG_MODEL
  printf("nsXFormsModelElement::RemoveFormControl(con: %p)\n",
         (void*) aControl);
#endif

  NS_ENSURE_ARG(aControl);
  PRBool removed;
  nsresult rv = mFormControls.RemoveControl(aControl, removed);
  NS_WARN_IF_FALSE(removed,
                   "Tried to remove control that was not in the model");
  return rv;
}

NS_IMETHODIMP
nsXFormsModelElement::GetTypeForControl(nsIXFormsControl  *aControl,
                                        nsISchemaType    **aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = nsnull;

  nsCOMPtr<nsIDOMNode> boundNode;
  aControl->GetBoundNode(getter_AddRefs(boundNode));
  if (!boundNode) {
    // if the control isn't bound to instance data, it doesn't make sense to 
    // return a type.  It is perfectly valid for there to be no bound node,
    // so no need to use an NS_ENSURE_xxx macro, either.
    return NS_ERROR_FAILURE;
  }

  nsAutoString schemaTypeName, schemaTypeNamespace;
  nsresult rv = GetTypeAndNSFromNode(boundNode, schemaTypeName,
                                     schemaTypeNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXFormsSchemaValidator validator;

  nsCOMPtr<nsISchemaCollection> schemaColl = do_QueryInterface(mSchemas);
  if (schemaColl) {
    nsCOMPtr<nsISchema> schema;
    schemaColl->GetSchema(schemaTypeNamespace, getter_AddRefs(schema));
    // if no schema found, then we will only handle built-in types.
    if (schema)
      validator.LoadSchema(schema);
  }

  PRBool foundType = validator.GetType(schemaTypeName, schemaTypeNamespace,
                                       aType);
  return foundType ? NS_OK : NS_ERROR_FAILURE;
}

/* static */ nsresult
nsXFormsModelElement::GetTypeAndNSFromNode(nsIDOMNode *aInstanceData,
                                           nsAString &aType, nsAString &aNSUri)
{
  nsresult rv = GetTypeFromNode(aInstanceData, aType, aNSUri);

  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // if there is no type assigned, then assume that the type is 'string'
    aNSUri.Assign(NS_LITERAL_STRING(NS_NAMESPACE_XML_SCHEMA));
    aType.Assign(NS_LITERAL_STRING("string"));
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsXFormsModelElement::InstanceLoadStarted()
{
  ++mPendingInstanceCount;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::InstanceLoadFinished(PRBool aSuccess)
{
  if (!aSuccess) {
    // This will leave mPendingInstanceCount in an invalid state, which is
    // exactly what we want, because this is a fatal error, and processing
    // should stop. If we decrease mPendingInstanceCount, the model would
    // finish construction, which is wrong.
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("instanceLoadError"), mElement);
    nsXFormsUtils::DispatchEvent(mElement, eEvent_LinkException);
    return NS_OK;
  }

  --mPendingInstanceCount;
  if (IsComplete()) {
    nsresult rv = FinishConstruction();
    if (NS_SUCCEEDED(rv)) {
      MaybeNotifyCompletion();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::FindInstanceElement(const nsAString &aID,
                                          nsIInstanceElementPrivate **aElement)
{
  NS_ENSURE_STATE(mInstanceDocuments);
  *aElement = nsnull;

  PRUint32 instCount;
  mInstanceDocuments->GetLength(&instCount);
  if (instCount) {
    nsCOMPtr<nsIDOMElement> element;
    nsAutoString id;
    for (PRUint32 i = 0; i < instCount; ++i) {
      nsIInstanceElementPrivate* instEle = mInstanceDocuments->GetInstanceAt(i);
      instEle->GetElement(getter_AddRefs(element));

      if (aID.IsEmpty()) {
        NS_ADDREF(instEle);
        *aElement = instEle;
        break;
      } else if (!element) {
        // this should only happen if the instance on the list is lazy authored
        // and as far as I can tell, a lazy authored instance should be the
        // first (and only) instance in the model and unable to have an ID.  
        // But that isn't clear to me reading the spec, so for now
        // we'll play it safe in case the WG more clearly defines lazy authoring
        // in the future.
        continue;
      }

      element->GetAttribute(NS_LITERAL_STRING("id"), id);
      if (aID.Equals(id)) {
        NS_ADDREF(instEle);
        *aElement = instEle;
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::SetNodeValue(nsIDOMNode      *aNode,
                                   const nsAString &aNodeValue,
                                   PRBool           aDoRefresh,
                                   PRBool          *aNodeChanged)
{
  NS_ENSURE_ARG_POINTER(aNodeChanged);
  nsresult rv = mMDG.SetNodeValue(aNode, aNodeValue, aNodeChanged);
  NS_ENSURE_SUCCESS(rv, rv);
  if (*aNodeChanged && aDoRefresh) {
    rv = RequestRecalculate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = RequestRevalidate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = RequestRefresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::SetNodeContent(nsIDOMNode *aNode,
                                     nsIDOMNode *aNodeContent,
                                     PRBool      aDoRebuild)
{ 
  nsresult rv = mMDG.SetNodeContent(aNode, aNodeContent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDoRebuild) {
    rv = RequestRebuild();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = RequestRecalculate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = RequestRevalidate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = RequestRefresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::ValidateNode(nsIDOMNode *aInstanceNode, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsAutoString schemaTypeName, schemaTypeNamespace;
  nsresult rv = GetTypeAndNSFromNode(aInstanceNode, schemaTypeName, 
                                     schemaTypeNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXFormsSchemaValidator validator;
  nsCOMPtr<nsISchemaCollection> schemaColl = do_QueryInterface(mSchemas);
  if (schemaColl) {
    nsCOMPtr<nsISchema> schema;
    schemaColl->GetSchema(schemaTypeNamespace, getter_AddRefs(schema));
    // if no schema found, then we will only handle built-in types.
    if (schema)
      validator.LoadSchema(schema);
  }

  nsAutoString value;
  nsXFormsUtils::GetNodeValue(aInstanceNode, value);
  PRBool isValid = validator.ValidateString(value, schemaTypeName, schemaTypeNamespace);

  *aResult = isValid;
  return NS_OK;
}

nsresult
nsXFormsModelElement::ValidateDocument(nsIDOMDocument *aInstanceDocument,
                                       PRBool         *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG(aInstanceDocument);

  /*
    This will process the instance document and check for schema validity.  It
    will mark nodes in the document with their schema types using nsIProperty
    until it hits a structural schema validation error.  So if the instance
    document's XML structure is invalid, don't expect type properties to be
    set.

    Note that if the structure is fine but some simple types nodes (nodes
    that contain text only) are invalid (say one has a empty nodeValue but
    should be a date), the schema validator will continue processing and add
    the type properties.  Schema validation will return false at the end.
  */

  nsCOMPtr<nsIDOMElement> element;
  nsresult rv = aInstanceDocument->GetDocumentElement(getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(element);

  // get namespace from node
  nsAutoString nsuri;
  element->GetNamespaceURI(nsuri);

  nsCOMPtr<nsISchemaCollection> schemaColl = do_QueryInterface(mSchemas);
  NS_ENSURE_STATE(schemaColl);

  nsCOMPtr<nsISchema> schema;
  schemaColl->GetSchema(nsuri, getter_AddRefs(schema));
  if (!schema) {
    // No schema found, so nothing to validate
    *aResult = PR_TRUE;
    return NS_OK;
  }
  
  nsXFormsSchemaValidator validator;
  validator.LoadSchema(schema);
  // Validate will validate the node and its subtree, as per the schema
  // specification.
  *aResult = validator.Validate(element);

  return NS_OK;
}

/*
 *  SUBMIT_SERIALIZE_NODE   - node is to be serialized
 *  SUBMIT_SKIP_NODE        - node is not to be serialized
 *  SUBMIT_ABORT_SUBMISSION - abort submission (invalid node or empty required node)
 */
NS_IMETHODIMP
nsXFormsModelElement::HandleInstanceDataNode(nsIDOMNode     *aInstanceDataNode,
                                             unsigned short *aResult)
{
  // abort by default
  *aResult = SUBMIT_ABORT_SUBMISSION;

  const nsXFormsNodeState* ns;
  ns = mMDG.GetNodeState(aInstanceDataNode);
  NS_ENSURE_STATE(ns);

  if (!ns->IsRelevant()) {
    // not relevant, thus skip
    *aResult = SUBMIT_SKIP_NODE;
  } else if (ns->IsRequired()) {
    // required and has a value, continue
    nsAutoString value;
    nsXFormsUtils::GetNodeValue(aInstanceDataNode, value);
    if (!value.IsEmpty() && ns->IsValid())
      *aResult = SUBMIT_SERIALIZE_NODE;
  } else if (ns->IsValid()) {
    // valid
    *aResult = SUBMIT_SERIALIZE_NODE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetLazyAuthored(PRBool *aLazyInstance)
{
  *aLazyInstance = mLazyModel;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetIsReady(PRBool *aIsReady)
{
  *aIsReady = mReadyHandled;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetTypeFromNode(nsIDOMNode *aInstanceData,
                                      nsAString  &aType,
                                      nsAString  &aNSUri)
{
  // aInstanceData could be an instance data node or it could be an attribute
  // on an instance data node (basically the node that a control is bound to).

  nsString *typeVal = nsnull;

  // Get type stored directly on instance node
  nsAutoString typeAttribute;
  nsCOMPtr<nsIDOMElement> nodeElem(do_QueryInterface(aInstanceData));
  if (nodeElem) {
    nodeElem->GetAttributeNS(NS_LITERAL_STRING(NS_NAMESPACE_XML_SCHEMA_INSTANCE),
                             NS_LITERAL_STRING("type"), typeAttribute);
    if (!typeAttribute.IsEmpty()) {
      typeVal = &typeAttribute;
    }
  }

  // If there was no type information on the node itself, check for a type
  // bound to the node via \<xforms:bind\>
  if (!typeVal && !mNodeToType.Get(aInstanceData, &typeVal)) {
    // check if schema validation left us a nsISchemaType*
    nsCOMPtr<nsIContent> content = do_QueryInterface(aInstanceData);

    if (content) {
      nsISchemaType *type;
      nsCOMPtr<nsIAtom> myAtom = do_GetAtom("xsdtype");

      type = NS_STATIC_CAST(nsISchemaType *, content->GetProperty(myAtom));
      if (type) {
        type->GetName(aType);
        type->GetTargetNamespace(aNSUri);
        return NS_OK;
      }
    }

    // No type information found
    return NS_ERROR_NOT_AVAILABLE;
  }

  // split type (ns:type) into namespace and type.
  nsAutoString prefix;
  PRInt32 separator = typeVal->FindChar(':');
  if ((PRUint32) separator == (typeVal->Length() - 1)) {
    const PRUnichar *strings[] = { typeVal->get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("missingTypeName"), strings, 1,
                               mElement, nsnull);
    return NS_ERROR_UNEXPECTED;
  }

  if (separator == kNotFound) {
    // no namespace prefix, which is valid.  In this case we should follow
    // http://www.w3.org/TR/2004/REC-xmlschema-1-20041028/#src-qname and pick
    // up the default namespace.  Which will happen by passing an empty string
    // as first parameter to LookupNamespaceURI.
    prefix = EmptyString();
    aType.Assign(*typeVal);
  } else {
    prefix.Assign(Substring(*typeVal, 0, separator));
    aType.Assign(Substring(*typeVal, ++separator, typeVal->Length()));

    if (prefix.IsEmpty()) {
      aNSUri = EmptyString();
      return NS_OK;
    }
  }

  // get the namespace url from the prefix using instance data node
  nsresult rv;
  nsCOMPtr<nsIDOM3Node> domNode3 = do_QueryInterface(aInstanceData, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = domNode3->LookupNamespaceURI(prefix, aNSUri);

  if (DOMStringIsNull(aNSUri)) {
    // if not found using instance data node, use <xf:instance> node
    nsCOMPtr<nsIDOMNode> instanceNode;
    rv = nsXFormsUtils::GetInstanceNodeForData(aInstanceData,
                                               getter_AddRefs(instanceNode));
    NS_ENSURE_SUCCESS(rv, rv);

    domNode3 = do_QueryInterface(instanceNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = domNode3->LookupNamespaceURI(prefix, aNSUri);
  }

  return rv;
}

/**
 * Poor man's try-catch to make sure that we set mProcessingUpdateEvent to
 * when leaving scope. If we actually bail with an error at some time,
 * something is pretty rotten, but at least we will not prevent any further
 * updates.
 */
class Updating {
private:
  nsXFormsModelElement* mModel;

public:
  Updating(nsXFormsModelElement* aModel)
    : mModel(aModel) { mModel->mProcessingUpdateEvent = PR_TRUE; };
  ~Updating() { mModel->mProcessingUpdateEvent = PR_FALSE; };
};

nsresult
nsXFormsModelElement::RequestUpdateEvent(nsXFormsEvent aEvent)
{
  if (mProcessingUpdateEvent) {
    mUpdateEventQueue.AppendElement(NS_INT32_TO_PTR(aEvent));
    return NS_OK;
  }

  Updating upd(this);

  // Send the requested event
  nsresult rv = nsXFormsUtils::DispatchEvent(mElement, aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Process queued events
  PRInt32 loopCount = 0;
  while (mUpdateEventQueue.Count()) {
    nsXFormsEvent event =
      NS_STATIC_CAST(nsXFormsEvent, NS_PTR_TO_UINT32(mUpdateEventQueue[0]));
    NS_ENSURE_TRUE(mUpdateEventQueue.RemoveElementAt(0), NS_ERROR_FAILURE);

    rv = nsXFormsUtils::DispatchEvent(mElement, event);
    NS_ENSURE_SUCCESS(rv, rv);
    ++loopCount;
    if (mLoopMax && loopCount > mLoopMax) {
      // Note: we could also popup a dialog asking the user whether or not to
      // continue.
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("modelLoopError"), mElement);
      nsXFormsUtils::HandleFatalError(mElement, NS_LITERAL_STRING("LoopError"));
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXFormsModelElement::RequestRebuild()
{
  return RequestUpdateEvent(eEvent_Rebuild);
}

NS_IMETHODIMP
nsXFormsModelElement::RequestRecalculate()
{
  return RequestUpdateEvent(eEvent_Recalculate);
}

NS_IMETHODIMP
nsXFormsModelElement::RequestRevalidate()
{
  return RequestUpdateEvent(eEvent_Revalidate);
}

NS_IMETHODIMP
nsXFormsModelElement::RequestRefresh()
{
  return RequestUpdateEvent(eEvent_Refresh);
}


// nsIXFormsContextControl

NS_IMETHODIMP
nsXFormsModelElement::SetContext(nsIDOMNode *aContextNode,
                                 PRInt32     aContextPosition,
                                 PRInt32     aContextSize)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXFormsModelElement::GetContext(nsAString      &aModelID,
                                 nsIDOMNode    **aContextNode,
                                 PRInt32        *aContextPosition,
                                 PRInt32        *aContextSize)
{
  // Adding the nsIXFormsContextControl interface to model to allow 
  // submission elements to call our binding evaluation methods, like
  // EvaluateNodeBinding.  If GetContext can get called outside of the binding
  // codepath, then this MIGHT lead to problems.

  NS_ENSURE_ARG(aContextSize);
  NS_ENSURE_ARG(aContextPosition);
  *aContextNode = nsnull;

  // better get the stuff most likely to fail out of the way first.  No sense
  // changing the other values that we are returning unless this is successful.
  nsresult rv = NS_ERROR_FAILURE;

  // Anybody (like a submission element) asking a model element for its context 
  // for XPath expressions will want the root node of the default instance
  // document
  nsCOMPtr<nsIDOMDocument> firstInstanceDoc =
    FindInstanceDocument(EmptyString());
  NS_ENSURE_TRUE(firstInstanceDoc, rv);

  nsCOMPtr<nsIDOMElement> firstInstanceRoot;
  rv = firstInstanceDoc->GetDocumentElement(getter_AddRefs(firstInstanceRoot));
  NS_ENSURE_TRUE(firstInstanceRoot, rv);

  nsCOMPtr<nsIDOMNode>rootNode = do_QueryInterface(firstInstanceRoot);
  rootNode.swap(*aContextNode);

  // found the context, so can finish up assinging the rest of the values that
  // we are returning
  *aContextPosition = 1;
  *aContextSize = 1;

  nsAutoString id;
  mElement->GetAttribute(NS_LITERAL_STRING("id"), id);
  aModelID.Assign(id);

  return NS_OK;
}

// internal methods

already_AddRefed<nsIDOMDocument>
nsXFormsModelElement::FindInstanceDocument(const nsAString &aID)
{
  nsCOMPtr<nsIInstanceElementPrivate> instance;
  nsXFormsModelElement::FindInstanceElement(aID, getter_AddRefs(instance));

  nsIDOMDocument *doc = nsnull;
  if (instance) {
    instance->GetInstanceDocument(&doc); // addrefs
  }

  return doc;
}

nsresult
nsXFormsModelElement::ProcessBindElements()
{
  // ProcessBindElements() will go through each xforms:bind element in
  // document order and apply all of the Model Item Properties to the
  // instance items in the nodeset. This information will also be entered
  // in the Master Dependency Graph.  Most of this work is done in the
  // ProcessBind() method.

  nsCOMPtr<nsIDOMDocument> firstInstanceDoc =
    FindInstanceDocument(EmptyString());
  if (!firstInstanceDoc)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> firstInstanceRoot;
  firstInstanceDoc->GetDocumentElement(getter_AddRefs(firstInstanceRoot));

  nsresult rv;
  nsCOMPtr<nsIDOMXPathEvaluator> xpath = do_QueryInterface(firstInstanceDoc,
                                                           &rv);
  NS_ENSURE_TRUE(xpath, rv);

  nsCOMPtr<nsIDOMNodeList> children;
  mElement->GetChildNodes(getter_AddRefs(children));

  PRUint32 childCount = 0;
  if (children)
    children->GetLength(&childCount);

  nsAutoString namespaceURI, localName;
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDOMNode> child;
    children->Item(i, getter_AddRefs(child));
    NS_ASSERTION(child, "there can't be null items in the NodeList!");

    child->GetLocalName(localName);
    if (localName.EqualsLiteral("bind")) {
      child->GetNamespaceURI(namespaceURI);
      if (namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
        rv = ProcessBind(xpath, firstInstanceRoot, 1, 1,
                         nsCOMPtr<nsIDOMElement>(do_QueryInterface(child)),
                         PR_TRUE);
        if (NS_FAILED(rv)) {
          return NS_OK;
        }
      }
    }
  }

  return NS_OK;
}

void
nsXFormsModelElement::Reset()
{
  BackupOrRestoreInstanceData(PR_TRUE);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_Rebuild);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_Recalculate);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_Revalidate);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_Refresh);
}

// This function will restore all of the model's instance data to it's original
// state if the supplied boolean is PR_TRUE.  If it is PR_FALSE, this function
// will cause this model's instance data to be backed up.
void
nsXFormsModelElement::BackupOrRestoreInstanceData(PRBool restore)
{
  if (!mInstanceDocuments)
    return;

  PRUint32 instCount;
  mInstanceDocuments->GetLength(&instCount);
  if (instCount) {
    for (PRUint32 i = 0; i < instCount; ++i) {
      nsIInstanceElementPrivate *instance =
        mInstanceDocuments->GetInstanceAt(i);

      // Don't know what to do with error if we get one.
      // Restore/BackupOriginalDocument will already output warnings.
      if(restore) {
        instance->RestoreOriginalDocument();
      }
      else {
        instance->BackupOriginalDocument();
      }
    }
  }

}


nsresult
nsXFormsModelElement::FinishConstruction()
{
  // Ensure that FinishConstruction isn't called due to some callback
  // or event handler after the model has started going through its
  // destruction phase
  NS_ENSURE_STATE(mElement);

  // process inline schemas that aren't referenced via the schema attribute
  nsCOMPtr<nsIDOMNodeList> children;
  mElement->GetChildNodes(getter_AddRefs(children));

  if (children) {
    PRUint32 childCount = 0;
    children->GetLength(&childCount);

    nsCOMPtr<nsIDOMNode> node;
    nsCOMPtr<nsIDOMElement> element;
    nsAutoString nsURI, localName, targetNamespace;

    for (PRUint32 i = 0; i < childCount; ++i) {
      children->Item(i, getter_AddRefs(node));

      element = do_QueryInterface(node);
      if (!element)
        continue;

      node->GetNamespaceURI(nsURI);
      node->GetLocalName(localName);
      if (nsURI.EqualsLiteral(NS_NAMESPACE_XML_SCHEMA) &&
          localName.EqualsLiteral("schema")) {
        if (!IsDuplicateSchema(element)) {
          nsCOMPtr<nsISchema> schema;
          nsresult rv = mSchemas->ProcessSchemaElement(element, nsnull,
                                                       getter_AddRefs(schema));
          if (!NS_SUCCEEDED(rv)) {
            nsXFormsUtils::ReportError(NS_LITERAL_STRING("schemaProcessError"),
                                       node);
          }
        }
      }
    }
  }

  // (XForms 4.2.1 - cont)
  // 3. if applicable, initialize P3P

  // 4. construct instance data from initial instance data.  apply all
  // <bind> elements in document order.

  // we get the instance data from our instance child nodes

  // We're done initializing this model.
  // 5. Perform an xforms-rebuild, xforms-recalculate, and xforms-revalidate in 
  // sequence, for this model element. (The xforms-refresh is not performed
  // since the user interface has not yet been initialized).
  nsXFormsUtils::DispatchEvent(mElement, eEvent_Rebuild);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_Recalculate);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_Revalidate);

  return NS_OK;
}

nsresult
nsXFormsModelElement::InitializeControls()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::InitializeControls()\n");
#endif
  nsPostRefresh postRefresh = nsPostRefresh();

  nsXFormsControlListItem::iterator it;
  nsresult rv;
  PRBool dummy;
  for (it = mFormControls.begin(); it != mFormControls.end(); ++it) {
    // Get control
    nsCOMPtr<nsIXFormsControl> control = (*it)->Control();
    NS_ASSERTION(control, "mFormControls has null control?!");

#ifdef DEBUG_MODEL
    printf("\tControl (%p): ", (void*) control);
    nsCOMPtr<nsIDOMElement> controlElement;
    control->GetElement(getter_AddRefs(controlElement));
    // DBG_TAGINFO(controlElement);
#endif
    // Rebind
    rv = control->Bind(&dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    // Refresh controls
    rv = control->Refresh();
    // XXX: Bug 336608, refresh still fails for some controls, for some
    // reason.
    // NS_ENSURE_SUCCESS(rv, rv);
  }

  mChangedNodes.Clear();

  return NS_OK;
}

void
nsXFormsModelElement::ValidateInstanceDocuments()
{
  if (mInstanceDocuments) {
    PRUint32 instCount;
    mInstanceDocuments->GetLength(&instCount);
    if (instCount) {
      nsCOMPtr<nsIDOMDocument> document;

      for (PRUint32 i = 0; i < instCount; ++i) {
        nsIInstanceElementPrivate* instEle =
          mInstanceDocuments->GetInstanceAt(i);
        nsCOMPtr<nsIXFormsNSInstanceElement> NSInstEle(instEle);
        NSInstEle->GetInstanceDocument(getter_AddRefs(document));
        NS_ASSERTION(document,
                     "nsIXFormsNSInstanceElement::GetInstanceDocument returned null?!");

        if (document) {
          PRBool isValid = PR_FALSE;
          ValidateDocument(document, &isValid);

          if (!isValid) {
            nsCOMPtr<nsIDOMElement> instanceElement;
            instEle->GetElement(getter_AddRefs(instanceElement));

            nsXFormsUtils::ReportError(NS_LITERAL_STRING("instDocumentInvalid"),
                                       instanceElement);
          }
        }
      }
    }
  }
}

// NOTE: This function only runs to completion for _one_ of the models in the
// document.
void
nsXFormsModelElement::MaybeNotifyCompletion()
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  const nsVoidArray *models = GetModelList(domDoc);
  if (!models) {
    NS_NOTREACHED("no model list property");
    return;
  }

  PRInt32 i;

  // Nothing to be done if any model is incomplete or hasn't seen
  // DOMContentLoaded.
  for (i = 0; i < models->Count(); ++i) {
    nsXFormsModelElement *model =
        NS_STATIC_CAST(nsXFormsModelElement *, models->ElementAt(i));
    if (!model->mDocumentLoaded || !model->IsComplete())
      return;

    // Check validity of |functions=| attribute, if it exists.  Since we
    // don't support ANY extension functions currently, the existance of
    // |functions=| with a non-empty value is an error.
    nsCOMPtr<nsIDOMElement> tElement = model->mElement;
    nsAutoString extFunctionAtt;
    tElement->GetAttribute(NS_LITERAL_STRING("functions"), extFunctionAtt);
    if (!extFunctionAtt.IsEmpty()) {
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("invalidExtFunction"),
                                 tElement);
      nsXFormsUtils::DispatchEvent(tElement, eEvent_ComputeException);
      return;
    }
  }

  // validate the instance documents because we want schemaValidation to add
  // schema type properties from the schema file unto our instance document
  // elements.
  // XXX: wrong location of this call, @see bug 339674
  ValidateInstanceDocuments();

  // Register deferred binds with the model. It does not bind the controls,
  // only bind them to the model they belong to.
  nsXFormsModelElement::ProcessDeferredBinds(domDoc);

  // Okay, dispatch xforms-model-construct-done
  for (i = 0; i < models->Count(); ++i) {
    nsXFormsModelElement *model =
        NS_STATIC_CAST(nsXFormsModelElement *, models->ElementAt(i));
    nsXFormsUtils::DispatchEvent(model->mElement, eEvent_ModelConstructDone);
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  if (doc) {
    PRUint32 loadingMessages = NS_PTR_TO_UINT32(
      doc->GetProperty(nsXFormsAtoms::externalMessagesProperty));
    if (loadingMessages) {
      // if we are still waiting for external messages to load, then put off
      // the xforms-ready until a model in the document is notified that they
      // are finished loading
   
      return;
    }
  }

  // Backup instances and fire xforms-ready
  for (i = 0; i < models->Count(); ++i) {
    nsXFormsModelElement *model =
        NS_STATIC_CAST(nsXFormsModelElement *, models->ElementAt(i));
    model->BackupOrRestoreInstanceData(PR_FALSE);
    model->mReadyHandled = PR_TRUE;
    nsXFormsUtils::DispatchEvent(model->mElement, eEvent_Ready);
  }
}

nsresult
nsXFormsModelElement::ProcessBind(nsIDOMXPathEvaluator *aEvaluator,
                                  nsIDOMNode           *aContextNode,
                                  PRInt32               aContextPosition,
                                  PRInt32               aContextSize,
                                  nsIDOMElement        *aBindElement,
                                  PRBool                aIsOuter)
{
  // Get the model item properties specified by this \<bind\>.
  nsCOMPtr<nsIDOMXPathExpression> props[eModel__count];
  nsAutoString propStrings[eModel__count];
  nsAutoString attrStr;

  nsCOMPtr<nsIDOMXPathNSResolver> resolver;
  nsresult rv = aEvaluator->CreateNSResolver(aBindElement,
                                             getter_AddRefs(resolver));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPathEvaluatorInternal> eval = do_QueryInterface(aEvaluator, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < eModel__count; ++i) {
    sModelPropsList[i]->ToString(attrStr);

    aBindElement->GetAttribute(attrStr, propStrings[i]);
  }

  // Find the nodeset that this bind applies to.
  nsCOMPtr<nsIDOMXPathResult> result;

  nsAutoString exprString;
  aBindElement->GetAttribute(NS_LITERAL_STRING("nodeset"), exprString);
  if (exprString.IsEmpty()) {
    exprString = NS_LITERAL_STRING(".");
  }

  nsCOMPtr<nsIXFormsXPathState> state = new nsXFormsXPathState(aBindElement,
                                                               aContextNode);
  NS_ENSURE_TRUE(state, NS_ERROR_OUT_OF_MEMORY);
  rv = nsXFormsUtils::EvaluateXPath(eval, exprString, aContextNode, resolver,
                                    state,
                                    nsIDOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                                    aContextPosition, aContextSize,
                                    nsnull, getter_AddRefs(result));
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_DOM_INVALID_EXPRESSION_ERR) {
      // the xpath expression isn't valid xpath
      const PRUnichar *strings[] = { exprString.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("exprParseError"),
                                 strings, 1, aBindElement, nsnull);
      nsXFormsUtils::DispatchEvent(mElement, eEvent_ComputeException);
    } else {
#ifdef DEBUG
      printf("xforms-binding-exception: XPath Evaluation failed\n");
#endif
      const PRUnichar *strings[] = { exprString.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("nodesetEvaluateError"),
                                 strings, 1, aBindElement, aBindElement);
      nsXFormsUtils::DispatchEvent(mElement, eEvent_BindingException);
    }
    return rv;
  }

  NS_ENSURE_STATE(result);
  
  // If this is an outer bind, store the nodeset, as controls binding to this
  // bind will need this.
  if (aIsOuter) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aBindElement));
    NS_ASSERTION(content, "nsIDOMElement not implementing nsIContent?!");
    rv = content->SetProperty(nsXFormsAtoms::bind, result,
                              SupportsDtorFunc);
    NS_ENSURE_SUCCESS(rv, rv);

    // addref, circumventing nsDerivedSave
    NS_ADDREF(NS_STATIC_CAST(nsIDOMXPathResult*, result));
  }

  PRUint32 snapLen;
  rv = result->GetSnapshotLength(&snapLen);
  NS_ENSURE_SUCCESS(rv, rv);
  

  // Iterate over resultset
  nsCOMArray<nsIDOMNode> deps;
  nsCOMPtr<nsIDOMNode> node;

  if (!snapLen) {
    return NS_OK;
  }

  // We rightly assume that all the nodes in the nodeset came from the same
  // document.  So now we'll get the xpath evaluator from that document.  We
  // need to ensure that the context node for the evaluation of each MIP
  // expression and the evaluator for those expressions came from the same
  // document.  It is a rule for xpath.
  PRUint32 snapItem = 0;

  for (; snapItem < snapLen; ++snapItem) {
    rv = result->SnapshotItem(snapItem, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    if (node){
      break;
    } else {
      NS_WARNING("nsXFormsModelElement::ProcessBind(): Empty node in result set.");
    }
  }

  if (!node) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> nodesetDoc;
  node->GetOwnerDocument(getter_AddRefs(nodesetDoc));

  nsCOMPtr<nsIDOMXPathEvaluator> nodesetEval = do_QueryInterface(nodesetDoc);
  nsCOMPtr<nsIXPathEvaluatorInternal> nodesetEvalInternal =
    do_QueryInterface(nodesetEval);
  NS_ENSURE_STATE(nodesetEval && nodesetEvalInternal);

  // Since we've already gotten the first node in the nodeset and verified it is
  // good to go, we'll contine on.  For this node and each subsequent node in
  // the nodeset, we'll evaluate the MIP expressions attached to the bind
  // element and add them to the MDG.  And also process any binds that this
  // bind contains (aka nested binds).
  while (node && snapItem < snapLen) {

    // set the context node for the expression that is being analyzed.
    nsCOMPtr<nsIXFormsXPathState> stateForMIP =
      new nsXFormsXPathState(aBindElement, node);
    NS_ENSURE_TRUE(stateForMIP, NS_ERROR_OUT_OF_MEMORY);

    // Apply MIPs
    nsXFormsXPathParser parser;
    nsXFormsXPathAnalyzer analyzer(eval, resolver, stateForMIP);
    PRBool multiMIP = PR_FALSE;
    for (PRUint32 j = 0; j < eModel__count; ++j) {
      if (propStrings[j].IsEmpty())
        continue;

      // type and p3ptype are stored as properties on the instance node
      if (j == eModel_type || j == eModel_p3ptype) {
        nsClassHashtable<nsISupportsHashKey, nsString> *table;
        table = j == eModel_type ? &mNodeToType : &mNodeToP3PType;
        NS_ENSURE_TRUE(table->IsInitialized(), NS_ERROR_FAILURE);

        // Check for existing value
        if (table->Get(node, nsnull)) {
          multiMIP = PR_TRUE;
          break;
        }

        // Insert value
        nsAutoPtr<nsString> newString(new nsString(propStrings[j]));
        NS_ENSURE_TRUE(newString, NS_ERROR_OUT_OF_MEMORY);
        NS_ENSURE_TRUE(table->Put(node, newString), NS_ERROR_OUT_OF_MEMORY);

        // string is successfully stored in the table, we should not dealloc it
        newString.forget();

        if (j == eModel_type) {
          // Inform MDG that it needs to check type. The only arguments
          // actually used are |eModel_constraint| and |node|.
          rv = mMDG.AddMIP(eModel_constraint, nsnull, nsnull, PR_FALSE, node,
                           1, 1);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } else {

        rv = nsXFormsUtils::CreateExpression(nodesetEvalInternal,
                                             propStrings[j], resolver,
                                             stateForMIP,
                                             getter_AddRefs(props[j]));
        if (NS_FAILED(rv)) {
          const PRUnichar *strings[] = { propStrings[j].get() };
          nsXFormsUtils::ReportError(NS_LITERAL_STRING("mipParseError"),
                                     strings, 1, aBindElement, aBindElement);
          nsXFormsUtils::DispatchEvent(mElement, eEvent_ComputeException);
          return rv;
        }

        // the rest of the MIPs are given to the MDG
        nsCOMPtr<nsIDOMNSXPathExpression> expr = do_QueryInterface(props[j]);

        // Get node dependencies
        nsAutoPtr<nsXFormsXPathNode> xNode(parser.Parse(propStrings[j]));
        deps.Clear();
        rv = analyzer.Analyze(node, xNode, expr, &propStrings[j], &deps,
                              snapItem + 1, snapLen, PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        // Insert into MDG
        rv = mMDG.AddMIP((ModelItemPropName) j,
                         expr,
                         &deps,
                         parser.UsesDynamicFunc(),
                         node,
                         snapItem + 1,
                         snapLen);

        // if the call results in NS_ERROR_ABORT the page has tried to set a
        // MIP twice, break and emit an exception.
        if (rv == NS_ERROR_ABORT) {
          multiMIP = PR_TRUE;
          break;
        }
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    // If the attribute is already there, the page sets a MIP twice
    // which is illegal, and should result in an xforms-binding-exception.
    // @see http://www.w3.org/TR/xforms/slice4.html#evt-modelConstruct
    // (item 4, c)
    if (multiMIP) {
#ifdef DEBUG
      printf("xforms-binding-exception: Multiple MIPs on same node!");
#endif
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("multiMIPError"),
                                 aBindElement);
      nsXFormsUtils::DispatchEvent(aBindElement,
                                   eEvent_BindingException);
      return NS_ERROR_FAILURE;
    }

    // Now evaluate any child \<bind\> elements.
    nsCOMPtr<nsIDOMNodeList> children;
    aBindElement->GetChildNodes(getter_AddRefs(children));
    if (children) {
      PRUint32 childCount = 0;
      children->GetLength(&childCount);

      nsCOMPtr<nsIDOMNode> child;
      nsAutoString value;

      for (PRUint32 k = 0; k < childCount; ++k) {
        children->Item(k, getter_AddRefs(child));
        if (child) {
          child->GetLocalName(value);
          if (!value.EqualsLiteral("bind"))
            continue;
          
          child->GetNamespaceURI(value);
          if (!value.EqualsLiteral(NS_NAMESPACE_XFORMS))
            continue;

          rv = ProcessBind(aEvaluator, node,
                           snapItem + 1, snapLen,
                           nsCOMPtr<nsIDOMElement>(do_QueryInterface(child)));
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
    }

    ++snapItem;
    while (snapItem < snapLen) {
      rv = result->SnapshotItem(snapItem, getter_AddRefs(node));
      NS_ENSURE_SUCCESS(rv, rv);
  
      if (node) {
        break;
      }

      NS_WARNING("nsXFormsModelElement::ProcessBind(): Empty node in result set.");
      snapItem++;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::AddInstanceElement(nsIInstanceElementPrivate *aInstEle) 
{
  NS_ENSURE_STATE(mInstanceDocuments);
  mInstanceDocuments->AddInstance(aInstEle);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::RemoveInstanceElement(nsIInstanceElementPrivate *aInstEle)
{
  NS_ENSURE_STATE(mInstanceDocuments);
  mInstanceDocuments->RemoveInstance(aInstEle);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::MessageLoadFinished() 
{
  // This is our signal that all external message links have been tested.  If
  // we were waiting for this to send out xforms-ready, then now is the time.

  // if this document hasn't processed xforms-model-construct-done, yet (which
  // must precede xforms-ready), then we'll send out the xforms-ready later
  // as part of our normal handling.  If we've already become ready, then this
  // event was probably generated by a change in the src attribute on the
  // message element.  Ignore it in that case.
  if (!mConstructDoneHandled || mReadyHandled) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));
  const nsVoidArray *models = GetModelList(domDoc);
  nsCOMPtr<nsIDocument>doc = do_QueryInterface(domDoc);
  nsCOMArray<nsIXFormsControl> *deferredBindList =
    NS_STATIC_CAST(nsCOMArray<nsIXFormsControl> *,
                   doc->GetProperty(nsXFormsAtoms::deferredBindListProperty));

  // if we've already gotten the xforms-model-construct-done event and not
  // yet the xforms-ready, we've hit a window where we may still be
  // processing the deferred control binding.  If so, we'll leave now and
  // leave it to MaybeNotifyCompletion to generate the xforms-ready event.
  if (deferredBindList) {
    return NS_OK;
  }


  // if we reached here, then we had to wait on sending out the xforms-ready
  // events until the external messages were tested.  Now we are finally
  // ready to send out xforms-ready to all of the models.
  for (int i = 0; i < models->Count(); ++i) {
    nsXFormsModelElement *model =
        NS_STATIC_CAST(nsXFormsModelElement *, models->ElementAt(i));
    model->mReadyHandled = PR_TRUE;
    nsXFormsUtils::DispatchEvent(model->mElement, eEvent_Ready);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetHasDOMContentFired(PRBool *aLoaded)
{
  NS_ENSURE_ARG_POINTER(aLoaded);

  *aLoaded = mDocumentLoaded;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::ForceRebind(nsIXFormsControl* aControl)
{
  if (!aControl) {
    return NS_OK;
  }

  nsXFormsControlListItem* controlItem = mFormControls.FindControl(aControl);
  NS_ENSURE_STATE(controlItem);

  PRBool rebindChildren;
  nsresult rv = aControl->Bind(&rebindChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aControl->Refresh();
  // XXX: no rv-check, see bug 336608

  // Refresh children
  return RefreshSubTree(controlItem->FirstChild(), rebindChildren);
}

nsresult
nsXFormsModelElement::GetBuiltinTypeName(PRUint16   aType,
                                         nsAString& aName)
{
  switch (aType) {
    case nsISchemaBuiltinType::BUILTIN_TYPE_STRING:
      aName.AssignLiteral("string");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN:
      aName.AssignLiteral("boolean");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL:
      aName.AssignLiteral("decimal");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT:
      aName.AssignLiteral("float");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_DOUBLE:
      aName.AssignLiteral("double");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_DURATION:
      aName.AssignLiteral("duration");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME:
      aName.AssignLiteral("dateTime");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_TIME:
      aName.AssignLiteral("time");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_DATE:
      aName.AssignLiteral("date");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH:
      aName.AssignLiteral("gYearMonth");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR:
      aName.AssignLiteral("gYear");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY:
      aName.AssignLiteral("gMonthDay");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_GDAY:
      aName.AssignLiteral("gDay");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH:
      aName.AssignLiteral("gMonth");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY:
      aName.AssignLiteral("hexBinary");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY:
      aName.AssignLiteral("base64Binary");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI:
      aName.AssignLiteral("anyURI");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_QNAME:
      aName.AssignLiteral("QName");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NOTATION:
      aName.AssignLiteral("NOTATION");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NORMALIZED_STRING:
      aName.AssignLiteral("normalizedString");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN:
      aName.AssignLiteral("token");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE:
      aName.AssignLiteral("byte");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDBYTE:
      aName.AssignLiteral("unsignedByte");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER:
      aName.AssignLiteral("integer");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER:
      aName.AssignLiteral("negativeInteger");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER:
      aName.AssignLiteral("nonPositiveInteger");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_LONG:
      aName.AssignLiteral("long");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER:
      aName.AssignLiteral("nonNegativeInteger");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_INT:
      aName.AssignLiteral("int");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDINT:
      aName.AssignLiteral("unsignedInt");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDLONG:
      aName.AssignLiteral("unsignedLong");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER:
      aName.AssignLiteral("positiveInteger");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_SHORT:
      aName.AssignLiteral("short");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDSHORT:
      aName.AssignLiteral("unsignedShort");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_LANGUAGE:
      aName.AssignLiteral("language");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKEN:
      aName.AssignLiteral("NMTOKEN");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NAME:
      aName.AssignLiteral("Name");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME:
      aName.AssignLiteral("NCName");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_ID:
      aName.AssignLiteral("ID");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_IDREF:
      aName.AssignLiteral("IDREF");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITY:
      aName.AssignLiteral("ENTITY");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_IDREFS:
      aName.AssignLiteral("IDREFS");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITIES:
      aName.AssignLiteral("ENTITIES");
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKENS:
      aName.AssignLiteral("NMTOKENS");
      break;
    default:
      // should never hit here
      NS_WARNING("nsXFormsModelElement::GetBuiltinTypeName: Unknown builtin type encountered.");
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
nsXFormsModelElement::GetBuiltinTypesNames(PRUint16       aType,
                                           nsStringArray *aNameArray)
{
  // This function recursively appends aType (and its base types) to
  // aNameArray.  So it assumes aType isn't in the array already.
  nsAutoString typeString, builtString;
  PRUint16 parentType = 0;

  // We won't append xsd:anyType as the base of every type since that is kinda
  // redundant.

  nsresult rv = GetBuiltinTypeName(aType, typeString);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (aType) {
    case nsISchemaBuiltinType::BUILTIN_TYPE_NORMALIZED_STRING:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_STRING;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NORMALIZED_STRING;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_SHORT;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDBYTE:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDSHORT;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_LONG:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_INT:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_LONG;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDINT:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDLONG;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDLONG:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_SHORT:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_INT;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDSHORT:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDINT;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_LANGUAGE:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKEN:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NAME:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NAME;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_ID:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_IDREF:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITY:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_IDREFS:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_IDREF;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITIES:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_ENTITY;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKENS:
      parentType = nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKEN;
      break;
  }

  builtString.AppendLiteral(NS_NAMESPACE_XML_SCHEMA);
  builtString.AppendLiteral("#");
  builtString.Append(typeString);
  aNameArray->AppendString(builtString);

  if (parentType)
    return GetBuiltinTypesNames(parentType, aNameArray);

  return NS_OK;
}

nsresult
nsXFormsModelElement::WalkTypeChainInternal(nsISchemaType *aType,
                                            PRBool         aFindRootBuiltin,
                                            PRUint16      *aBuiltinType,
                                            nsStringArray *aTypeArray)
{
  PRUint16 schemaTypeValue = 0;
  aType->GetSchemaType(&schemaTypeValue);
  NS_ENSURE_STATE(schemaTypeValue);
  nsresult rv = NS_OK;
  nsCOMPtr<nsISchemaSimpleType> simpleType;

  if (schemaTypeValue == nsISchemaType::SCHEMA_TYPE_SIMPLE) {
    simpleType = do_QueryInterface(aType);
    NS_ENSURE_STATE(simpleType);
    PRUint16 simpleTypeValue;
    simpleType->GetSimpleType(&simpleTypeValue);
    NS_ENSURE_STATE(simpleTypeValue);

    switch (simpleTypeValue) {
      case nsISchemaSimpleType::SIMPLE_TYPE_BUILTIN:
      {
        nsCOMPtr<nsISchemaBuiltinType> builtinType(do_QueryInterface(aType));
        NS_ENSURE_STATE(builtinType);

        if (aFindRootBuiltin)
          return BuiltinTypeToPrimative(builtinType, aBuiltinType);

        PRUint16 builtinTypeVal;
        rv = builtinType->GetBuiltinType(&builtinTypeVal);
        NS_ENSURE_SUCCESS(rv, rv);

        if (aBuiltinType)
          *aBuiltinType = builtinTypeVal;

        if (aTypeArray)
          return GetBuiltinTypesNames(builtinTypeVal, aTypeArray);

        return NS_OK;
      }
      case nsISchemaSimpleType::SIMPLE_TYPE_RESTRICTION:
      {
        nsCOMPtr<nsISchemaRestrictionType> restType(do_QueryInterface(aType));
        NS_ENSURE_STATE(restType);
        restType->GetBaseType(getter_AddRefs(simpleType));

        break;
      }
      case nsISchemaSimpleType::SIMPLE_TYPE_LIST:
      {
        nsCOMPtr<nsISchemaListType> listType(do_QueryInterface(aType));
        NS_ENSURE_STATE(listType);
        listType->GetListType(getter_AddRefs(simpleType));

        break;
      }
      case nsISchemaSimpleType::SIMPLE_TYPE_UNION:
      {
        // For now union types aren't supported.  A union means that the type
        // could be of any type listed in the union and still be valid.  But we
        // don't know which path it will take since we'd basically have to
        // validate the node value to know.  Someday we may have to figure out
        // how to properly handle this, though we may never need to if no other
        // processor supports it.  Strictly interpreting the spec, we don't
        // need to handle unions as far as determining whether a control can
        // bind to data of a given type.  Just the types defined in the spec
        // and restrictions of those types.
        return NS_ERROR_XFORMS_UNION_TYPE;
      }
      default:
        // We only anticipate the 4 types listed above.  Definitely an error
        // if we get something else.
        return NS_ERROR_FAILURE;
    }

  } else if (schemaTypeValue == nsISchemaType::SCHEMA_TYPE_COMPLEX) {
    nsCOMPtr<nsISchemaComplexType> complexType(do_QueryInterface(aType));
    NS_ENSURE_STATE(complexType);
    PRUint16 complexTypeValue = 0;
    complexType->GetDerivation(&complexTypeValue);
    NS_ENSURE_STATE(complexTypeValue);
    if ((complexTypeValue ==
         nsISchemaComplexType::DERIVATION_RESTRICTION_SIMPLE) ||
        (complexTypeValue ==
         nsISchemaComplexType::DERIVATION_EXTENSION_SIMPLE)) {
      complexType->GetSimpleBaseType(getter_AddRefs(simpleType));
    } else {
      return NS_ERROR_FAILURE;
    }
  } else {
    return NS_ERROR_FAILURE;
  }

  // For SIMPLE_TYPE_LIST and SIMPLE_TYPE_RESTRICTION we need to go around
  // the horn again with the next simpleType.  Same with
  // DERIVATION_RESTRICTION_SIMPLE and DERIVATION_EXTENSION_SIMPLE.  All other
  // types should not reach here.

  NS_ENSURE_STATE(simpleType);

  if (aTypeArray) {
    nsAutoString builtString;
    rv = aType->GetTargetNamespace(builtString);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoString typeName;
    rv = aType->GetName(typeName);
    NS_ENSURE_SUCCESS(rv, rv);
    builtString.AppendLiteral("#");
    builtString.Append(typeName);
    aTypeArray->AppendString(builtString);
  }

  return WalkTypeChainInternal(simpleType, aFindRootBuiltin, aBuiltinType,
                               aTypeArray);

}

nsresult
nsXFormsModelElement::BuiltinTypeToPrimative(nsISchemaBuiltinType *aSchemaType,
                                             PRUint16             *aPrimType)
{
  NS_ENSURE_ARG(aSchemaType);
  NS_ENSURE_ARG_POINTER(aPrimType);

  PRUint16 builtinType = 0;
  nsresult rv = aSchemaType->GetBuiltinType(&builtinType);
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: this won't return BUILTIN_TYPE_ANY since that is the root of all
  // types.

  switch (builtinType) {
    case nsISchemaBuiltinType::BUILTIN_TYPE_STRING:
    case nsISchemaBuiltinType::BUILTIN_TYPE_BOOLEAN:
    case nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL:
    case nsISchemaBuiltinType::BUILTIN_TYPE_FLOAT:
    case nsISchemaBuiltinType::BUILTIN_TYPE_DOUBLE:
    case nsISchemaBuiltinType::BUILTIN_TYPE_DURATION:
    case nsISchemaBuiltinType::BUILTIN_TYPE_DATETIME:
    case nsISchemaBuiltinType::BUILTIN_TYPE_TIME:
    case nsISchemaBuiltinType::BUILTIN_TYPE_DATE:
    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEARMONTH:
    case nsISchemaBuiltinType::BUILTIN_TYPE_GYEAR:
    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTHDAY:
    case nsISchemaBuiltinType::BUILTIN_TYPE_GDAY:
    case nsISchemaBuiltinType::BUILTIN_TYPE_GMONTH:
    case nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY:
    case nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY:
    case nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI:
    case nsISchemaBuiltinType::BUILTIN_TYPE_QNAME:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NOTATION:
      *aPrimType = builtinType;
      break;

    case nsISchemaBuiltinType::BUILTIN_TYPE_NORMALIZED_STRING:
    case nsISchemaBuiltinType::BUILTIN_TYPE_TOKEN:
    case nsISchemaBuiltinType::BUILTIN_TYPE_LANGUAGE:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKEN:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NAME:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NCNAME:
    case nsISchemaBuiltinType::BUILTIN_TYPE_ID:
    case nsISchemaBuiltinType::BUILTIN_TYPE_IDREF:
    case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITY:
    case nsISchemaBuiltinType::BUILTIN_TYPE_IDREFS:
    case nsISchemaBuiltinType::BUILTIN_TYPE_ENTITIES:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NMTOKENS:
      *aPrimType = nsISchemaBuiltinType::BUILTIN_TYPE_STRING;
      break;
    case nsISchemaBuiltinType::BUILTIN_TYPE_BYTE:
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDBYTE:
    case nsISchemaBuiltinType::BUILTIN_TYPE_INTEGER:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NEGATIVEINTEGER:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONPOSITIVEINTEGER:
    case nsISchemaBuiltinType::BUILTIN_TYPE_LONG:
    case nsISchemaBuiltinType::BUILTIN_TYPE_NONNEGATIVEINTEGER:
    case nsISchemaBuiltinType::BUILTIN_TYPE_INT:
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDINT:
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDLONG:
    case nsISchemaBuiltinType::BUILTIN_TYPE_POSITIVEINTEGER:
    case nsISchemaBuiltinType::BUILTIN_TYPE_SHORT:
    case nsISchemaBuiltinType::BUILTIN_TYPE_UNSIGNEDSHORT:
      *aPrimType = nsISchemaBuiltinType::BUILTIN_TYPE_DECIMAL;
      break;
    default:
      // should never hit here
      NS_WARNING("nsXFormsModelElement::BuiltinTypeToPrimative: Unknown builtin type encountered.");
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetDerivedTypeList(const nsAString &aType,
                                         const nsAString &aNamespace,
                                         nsAString       &aTypeList)
{
  nsCOMPtr<nsISchemaCollection> schemaColl = do_QueryInterface(mSchemas);
  NS_ENSURE_STATE(schemaColl);

  nsCOMPtr<nsISchemaType> schemaType;
  schemaColl->GetType(aType, aNamespace, getter_AddRefs(schemaType));
  NS_ENSURE_STATE(schemaType);

  nsStringArray typeArray;
  nsresult rv = WalkTypeChainInternal(schemaType, PR_FALSE, nsnull, &typeArray);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIStringEnumerator> stringEnum;
    rv = NS_NewStringEnumerator(getter_AddRefs(stringEnum), &typeArray);
    if (NS_SUCCEEDED(rv)) {
      nsAutoString constructorString;
      PRBool hasMore = PR_FALSE;
      rv = stringEnum->HasMore(&hasMore);
      while (NS_SUCCEEDED(rv) && hasMore) {
        nsAutoString tempString;
        rv = stringEnum->GetNext(tempString);
        if (NS_SUCCEEDED(rv)) {
          constructorString.Append(tempString);
          stringEnum->HasMore(&hasMore);
          if (hasMore) {
            constructorString.AppendLiteral(" ");
          }
        }
      }

      if (NS_SUCCEEDED(rv)) {
        aTypeList.Assign(constructorString);
      }
    }
  }

  if (NS_FAILED(rv)) {
    aTypeList.Assign(EmptyString());
  }

  typeArray.Clear();

  return rv;
}

NS_IMETHODIMP
nsXFormsModelElement::GetBuiltinTypeNameForControl(nsIXFormsControl  *aControl,
                                                   nsAString&         aTypeName)
{
  NS_ENSURE_ARG(aControl);

  nsCOMPtr<nsISchemaType> schemaType;
  nsresult rv = GetTypeForControl(aControl, getter_AddRefs(schemaType));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint16 builtinType;
  rv = WalkTypeChainInternal(schemaType, PR_FALSE, &builtinType);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetBuiltinTypeName(builtinType, aTypeName);
}

NS_IMETHODIMP
nsXFormsModelElement::GetRootBuiltinType(nsISchemaType *aType,
                                         PRUint16      *aBuiltinType)
{
  NS_ENSURE_ARG(aType);
  NS_ENSURE_ARG_POINTER(aBuiltinType);

  return WalkTypeChainInternal(aType, PR_TRUE, aBuiltinType);
}

/* static */ void
nsXFormsModelElement::Startup()
{
  sModelPropsList[eModel_type] = nsXFormsAtoms::type;
  sModelPropsList[eModel_readonly] = nsXFormsAtoms::readonly;
  sModelPropsList[eModel_required] = nsXFormsAtoms::required;
  sModelPropsList[eModel_relevant] = nsXFormsAtoms::relevant;
  sModelPropsList[eModel_calculate] = nsXFormsAtoms::calculate;
  sModelPropsList[eModel_constraint] = nsXFormsAtoms::constraint;
  sModelPropsList[eModel_p3ptype] = nsXFormsAtoms::p3ptype;
}

already_AddRefed<nsIDOMElement>
nsXFormsModelElement::GetDOMElement()
{
  nsIDOMElement* element = nsnull;
  NS_IF_ADDREF(element = mElement);
  return element;
}

static void
DeleteBindList(void    *aObject,
               nsIAtom *aPropertyName,
               void    *aPropertyValue,
               void    *aData)
{
  delete NS_STATIC_CAST(nsCOMArray<nsIXFormsControl> *, aPropertyValue);
}

/* static */ nsresult
nsXFormsModelElement::DeferElementBind(nsIXFormsControl *aControl)
{
  NS_ENSURE_ARG_POINTER(aControl);
  nsCOMPtr<nsIDOMElement> element;
  nsresult rv = aControl->GetElement(getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIContent> content(do_QueryInterface(element));
  NS_ASSERTION(content, "nsIDOMElement not implementing nsIContent?!");

  nsCOMPtr<nsIDocument> doc = content->GetCurrentDoc();
  if (!doc) {
    // We do not care about elements without a document. If they get added to
    // a document at some point in time, they'll try to bind again.
    return NS_OK;
  }

  // We are using a PRBool on each control to mark whether the control is on the
  // deferredBindList.  We are running into too many scenarios where a control
  // could be added more than once which will lead to inefficiencies because
  // calling bind and refresh on some controls is getting pretty expensive.
  // We need to keep the document order of the controls AND don't want
  // to walk the deferredBindList every time we want to check about adding a
  // control.
  PRBool onList = PR_FALSE;
  aControl->GetOnDeferredBindList(&onList);
  if (onList) {
    return NS_OK;
  }

  nsCOMArray<nsIXFormsControl> *deferredBindList =
    NS_STATIC_CAST(nsCOMArray<nsIXFormsControl> *,
                   doc->GetProperty(nsXFormsAtoms::deferredBindListProperty));

  if (!deferredBindList) {
    deferredBindList = new nsCOMArray<nsIXFormsControl>(16);
    NS_ENSURE_TRUE(deferredBindList, NS_ERROR_OUT_OF_MEMORY);

    doc->SetProperty(nsXFormsAtoms::deferredBindListProperty, deferredBindList,
                     DeleteBindList);
  }

  // always append to the end of the list.  We need to keep the elements in
  // document order when we process the binds later.  Otherwise we have trouble
  // when an element is trying to bind and should use its parent as a context
  // for the xpath evaluation but the parent isn't bound yet.
  deferredBindList->AppendObject(aControl);
  aControl->SetOnDeferredBindList(PR_TRUE);

  return NS_OK;
}

/* static */ void
nsXFormsModelElement::ProcessDeferredBinds(nsIDOMDocument *aDoc)
{
#ifdef DEBUG_MODEL
  printf("nsXFormsModelElement::ProcessDeferredBinds()\n");
#endif

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);

  if (!doc) {
    return;
  }

  nsPostRefresh postRefresh = nsPostRefresh();

  doc->SetProperty(nsXFormsAtoms::readyForBindProperty, doc);

  nsCOMArray<nsIXFormsControl> *deferredBindList =
      NS_STATIC_CAST(nsCOMArray<nsIXFormsControl> *,
                     doc->GetProperty(nsXFormsAtoms::deferredBindListProperty));

  if (deferredBindList) {
    for (PRInt32 i = 0; i < deferredBindList->Count(); ++i) {
      nsIXFormsControl *control = deferredBindList->ObjectAt(i);
      if (control) {
        control->BindToModel(PR_FALSE);
        control->SetOnDeferredBindList(PR_FALSE);
      }
    }

    doc->DeleteProperty(nsXFormsAtoms::deferredBindListProperty);
  }
}

nsresult
nsXFormsModelElement::HandleLoad(nsIDOMEvent* aEvent)
{
  if (!mInstancesInitialized) {
    // XXX This is for Bug 308106. In Gecko 1.8 DoneAddingChildren is not
    //     called in XUL if the element doesn't have any child nodes.
    InitializeInstances();
  }

  mDocumentLoaded = PR_TRUE;

  nsCOMPtr<nsIDOMDocument> document;
  mElement->GetOwnerDocument(getter_AddRefs(document));
  NS_ENSURE_STATE(document);
  nsXFormsUtils::DispatchDeferredEvents(document);

  // dispatch xforms-model-construct, xforms-rebuild, xforms-recalculate,
  // xforms-revalidate

  // We wait until DOMContentLoaded to dispatch xforms-model-construct,
  // since the model may have an action handler for this event and Mozilla
  // doesn't register XML Event listeners until the document is loaded.

  // xforms-model-construct is not cancellable, so always proceed.

  nsXFormsUtils::DispatchEvent(mElement, eEvent_ModelConstruct);

  if (mPendingInlineSchemas.Count() > 0) {
    nsCOMPtr<nsIDOMElement> el;
    nsresult rv;
    for (PRInt32 i=0; i<mPendingInlineSchemas.Count(); ++i) {
      GetSchemaElementById(mElement, *mPendingInlineSchemas[i],
                           getter_AddRefs(el));
      if (!el) {
        rv = NS_ERROR_UNEXPECTED;
      } else {
        if (!IsDuplicateSchema(el)) {
          nsCOMPtr<nsISchema> schema;
          // no need to observe errors via the callback.  instead, rely on
          // this method returning a failure code when it encounters errors.
          rv = mSchemas->ProcessSchemaElement(el, nsnull,
                                              getter_AddRefs(schema));
          if (NS_SUCCEEDED(rv))
            mSchemaCount++;
        }
      }
      if (NS_FAILED(rv)) {
        // this is a fatal error
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("schemaLoadError"), mElement);
        nsXFormsUtils::DispatchEvent(mElement, eEvent_LinkException);
        return NS_OK;
      }
    }
    if (IsComplete()) {
      rv = FinishConstruction();
      NS_ENSURE_SUCCESS(rv, rv);
    }
    mPendingInlineSchemas.Clear();
  }

  // We may still be waiting on external documents to load.
  MaybeNotifyCompletion();

  return NS_OK;
}

nsresult
nsXFormsModelElement::HandleUnload(nsIDOMEvent* aEvent)
{
  // due to fastback changes, had to move this notification out from under
  // model's WillChangeDocument override.
  return nsXFormsUtils::DispatchEvent(mElement, eEvent_ModelDestruct);
}

PRBool
nsXFormsModelElement::IsDuplicateSchema(nsIDOMElement *aSchemaElement)
{
  nsCOMPtr<nsISchemaCollection> schemaColl = do_QueryInterface(mSchemas);
  if (!schemaColl)
    return PR_FALSE;

  const nsAFlatString& empty = EmptyString();
  nsAutoString targetNamespace;
  aSchemaElement->GetAttributeNS(empty,
                                 NS_LITERAL_STRING("targetNamespace"),
                                 targetNamespace);
  targetNamespace.Trim(" \r\n\t");

  nsCOMPtr<nsISchema> schema;
  schemaColl->GetSchema(targetNamespace, getter_AddRefs(schema));
  if (!schema)
    return PR_FALSE;

  // A schema with the same target namespace already exists in the
  // schema collection and the first instance has already been processed.
  // Report an error to the JS console and dispatch the LinkError event,
  // but do not consider it a fatal error.
  const nsPromiseFlatString& flat = PromiseFlatString(targetNamespace);
  const PRUnichar *strings[] = { flat.get() };
  nsXFormsUtils::ReportError(NS_LITERAL_STRING("duplicateSchema"),
                             strings, 1, aSchemaElement, aSchemaElement,
                             nsnull);
  nsXFormsUtils::DispatchEvent(mElement, eEvent_LinkError);
  return PR_TRUE;
}

nsresult
NS_NewXFormsModelElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsModelElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}


// ---------------------------- //

// nsXFormsModelInstanceDocuments

NS_IMPL_ISUPPORTS2(nsXFormsModelInstanceDocuments, nsIDOMNodeList, nsIClassInfo)

nsXFormsModelInstanceDocuments::nsXFormsModelInstanceDocuments()
  : mInstanceList(16)
{
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetLength(PRUint32* aLength)
{
  *aLength = mInstanceList.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  nsIInstanceElementPrivate* instance = mInstanceList.SafeObjectAt(aIndex);
  if (instance) {
    nsCOMPtr<nsIDOMDocument> doc;
    if (NS_SUCCEEDED(instance->GetInstanceDocument(getter_AddRefs(doc))) && doc) {
      NS_ADDREF(*aReturn = doc);
    }
  }

  return NS_OK;
}

nsIInstanceElementPrivate*
nsXFormsModelInstanceDocuments::GetInstanceAt(PRUint32 aIndex)
{
  return mInstanceList.ObjectAt(aIndex);
}

void
nsXFormsModelInstanceDocuments::AddInstance(nsIInstanceElementPrivate *aInst)
{
  // always append to the end of the list.  We need to keep the elements in
  // document order since the first instance element is the default instance
  // document for the model.
  mInstanceList.AppendObject(aInst);
}

void
nsXFormsModelInstanceDocuments::RemoveInstance(nsIInstanceElementPrivate *aInst)
{
  mInstanceList.RemoveObject(aInst);
}

void
nsXFormsModelInstanceDocuments::DropReferences()
{
  mInstanceList.Clear();
}

// nsIClassInfo implementation

static const nsIID sInstScriptingIIDs[] = {
  NS_IDOMNODELIST_IID
};

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetInterfaces(PRUint32   *aCount,
                                              nsIID   * **aArray)
{
  return
    nsXFormsUtils::CloneScriptingInterfaces(sInstScriptingIIDs,
                                            NS_ARRAY_LENGTH(sInstScriptingIIDs),
                                            aCount, aArray);
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetHelperForLanguage(PRUint32 language,
                                                     nsISupports **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetClassID(nsCID * *aClassID)
{
  *aClassID = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetImplementationLanguage(PRUint32 *aLang)
{
  *aLang = nsIProgrammingLanguage::CPLUSPLUS;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelInstanceDocuments::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  return NS_ERROR_NOT_AVAILABLE;
}

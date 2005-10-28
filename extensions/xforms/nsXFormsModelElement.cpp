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
#include "nsIXTFGenericElementWrapper.h"
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
#include "nsIXFormsXPathEvaluator.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsIScriptGlobalObject.h"
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
#include "nsArray.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIEventStateManager.h"

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

  nsCOMPtr<nsIDOMXPathResult> xpRes =
      nsXFormsUtils::EvaluateXPath(expr,
                                   contextNode,
                                   contextNode,
                                   nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE);
  if (xpRes) {
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

//------------------------------------------------------------------------------

static const nsIID sScriptingIIDs[] = {
  NS_IDOMELEMENT_IID,
  NS_IDOMEVENTTARGET_IID,
  NS_IDOM3NODE_IID,
  NS_IXFORMSMODELELEMENT_IID,
  NS_IXFORMSNSMODELELEMENT_IID
};

static nsIAtom* sModelPropsList[eModel__count];

// This can be nsVoidArray because elements will remove
// themselves from the list if they are deleted during refresh.
static nsVoidArray* sPostRefreshList = nsnull;

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
  --sRefreshing;
  if (sPostRefreshList && !sRefreshing) {
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
    delete sPostRefreshList;
    sPostRefreshList = nsnull;
  }
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

void
nsXFormsModelElement::CancelPostRefresh(nsIXFormsControl* aControl)
{
  if (sPostRefreshList)
    sPostRefreshList->RemoveElement(aControl);
}

nsXFormsModelElement::nsXFormsModelElement()
  : mElement(nsnull),
    mSchemaCount(0),
    mSchemaTotal(0),
    mPendingInstanceCount(0),
    mDocumentLoaded(PR_FALSE),
    mNeedsRefresh(PR_FALSE),
    mInstancesInitialized(PR_FALSE),
    mInstanceDocuments(nsnull),
    mLazyModel(PR_FALSE)
{
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
  RemoveModelFromDocument();

  mElement = nsnull;
  mSchemas = nsnull;

  if (mInstanceDocuments)
    mInstanceDocuments->DropReferences();

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
  if (targ)
    targ->RemoveEventListener(NS_LITERAL_STRING("DOMContentLoaded"), this, PR_TRUE);
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
  if(!aNewDocument) {
    // can't send this much later or the model won't still be in the document!
    nsXFormsUtils::DispatchEvent(mElement, eEvent_ModelDestruct);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::DocumentChanged(nsIDOMDocument* aNewDocument)
{
  if (!aNewDocument)
    return NS_OK;

  AddToModelList(aNewDocument, this);

  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(aNewDocument);
  if (targ)
    targ->AddEventListener(NS_LITERAL_STRING("DOMContentLoaded"), this, PR_TRUE);
  
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

  for (PRUint32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDOMNode> child;
    children->Item(i, getter_AddRefs(child));
    if (nsXFormsUtils::IsXFormsElement(child, NS_LITERAL_STRING("instance"))) {
      nsCOMPtr<nsIInstanceElementPrivate> instance(do_QueryInterface(child));
      if (instance) {
        instance->Initialize();
      }
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

    nsCStringArray schemas;
    schemas.ParseString(NS_ConvertUTF16toUTF8(schemaList).get(), " \t\r\n");

    // Increase by 1 to prevent OnLoad from calling FinishConstruction
    mSchemaTotal = schemas.Count();

    for (PRInt32 i=0; i<mSchemaTotal; ++i) {
      nsresult rv = NS_OK;
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
        newURL->Equals(baseURI, &equals);
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
            nsCOMPtr<nsISchema> schema;
            // no need to observe errors via the callback.  instead, rely on
            // this method returning a failure code when it encounters errors.
            rv = mSchemas->ProcessSchemaElement(el, nsnull,
                                                getter_AddRefs(schema));
            if (NS_SUCCEEDED(rv))
              mSchemaCount++;
          }
        } else {
          nsCAutoString uriSpec;
          newURI->GetSpec(uriSpec);
          rv = mSchemas->LoadAsync(NS_ConvertUTF8toUTF16(uriSpec), this);
        }
      }
      if (NS_FAILED(rv)) {
        // this is a fatal error (XXX)
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
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Ready].name)) {
    Ready();
  } else if (type.EqualsASCII(sXFormsEventsEntries[eEvent_Reset].name)) {
    Reset();
  } else {
    *aHandled = PR_FALSE;
  }

#ifdef DEBUG
  if (NS_FAILED(rv))
    printf("nsXFormsModelElement::HandleDefault() failed!\n");
#endif

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
nsXFormsModelElement::OnCreated(nsIXTFGenericElementWrapper *aWrapper)
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

  mSchemas = do_GetService(NS_SCHEMALOADER_CONTRACTID);

  mInstanceDocuments = new nsXFormsModelInstanceDocuments();
  NS_ASSERTION(mInstanceDocuments, "could not create mInstanceDocuments?!");

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
  return *aDocument ? NS_OK : NS_ERROR_FAILURE;
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

  // 2. Re-attach all elements
  if (mDocumentLoaded) { // if it's not during initializing phase
    // Copy the form control list as it stands right now.
    nsVoidArray *oldFormList = new nsVoidArray();
    NS_ENSURE_TRUE(oldFormList, NS_ERROR_OUT_OF_MEMORY);
    *oldFormList = mFormControls;
  
    // Clear out mFormControls so that we can rebuild the list.  We'll go control 
    // by control over the old list and rebind the controls.
    mFormControls.Clear(); // if this happens on a documentchange

    PRInt32 controlCount = oldFormList->Count();
    for (PRInt32 i = 0; i < controlCount; ++i) {
      nsIXFormsControl* control = NS_STATIC_CAST(nsIXFormsControl*, 
                                                 (*oldFormList)[i]);
      /// @todo If a control is removed because of previous control has been
      /// refreshed, we do, obviously, not need to refresh it. So mFormControls
      /// should have weak bindings to the controls I guess? (XXX)
      ///
      /// This could happen for \<repeatitem\>s for example.
      if (!control) {
        continue;
      }

      // run bind to reset mBoundNode for all of these controls and also, in the
      // process, they will be added to the model that they should be bound to.
      control->Bind();
    }
    
    delete oldFormList;

    // Triggers a refresh of all controls
    mNeedsRefresh = PR_TRUE;
  }

  // 3. Rebuild graph
  rv = ProcessBindElements();
  NS_ENSURE_SUCCESS(rv, rv);

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

nsresult
nsXFormsModelElement::SetStatesInternal(nsIXFormsControl *aControl,
                                        nsIDOMNode       *aNode,
                                        PRBool            aAllStates)
{
  NS_ENSURE_ARG(aControl);
  if (!aNode)
    return NS_OK;
  
  nsCOMPtr<nsIDOMElement> element;
  aControl->GetElement(getter_AddRefs(element));
  NS_ENSURE_STATE(element);

  nsCOMPtr<nsIXTFElementWrapper> xtfWrap(do_QueryInterface(element));
  NS_ENSURE_STATE(xtfWrap);

  const nsXFormsNodeState *ns = mMDG.GetNodeState(aNode);
  NS_ENSURE_STATE(ns);

  // XXX nsXFormsNodeState could expose a bitmask using NS_EVENTs, to avoid
  // most of this...
  PRBool tmp = ns->IsValid();
  PRUint32 state =  tmp ? NS_EVENT_STATE_VALID : NS_EVENT_STATE_INVALID;
  if (aAllStates || ns->ShouldDispatchValid()) {
    SetSingleState(element, tmp, eEvent_Valid);
  }
  tmp = ns->IsReadonly();
  state |= tmp ? NS_EVENT_STATE_MOZ_READONLY : NS_EVENT_STATE_MOZ_READWRITE;
  if (aAllStates || ns->ShouldDispatchReadonly()) {
    SetSingleState(element, tmp, eEvent_Readonly);
  }
  tmp = ns->IsRequired();
  state |= tmp ? NS_EVENT_STATE_REQUIRED : NS_EVENT_STATE_OPTIONAL;
  if (aAllStates || ns->ShouldDispatchRequired()) {
    SetSingleState(element, tmp, eEvent_Required);
  }
  tmp = ns->IsRelevant();
  state |= tmp ? NS_EVENT_STATE_ENABLED : NS_EVENT_STATE_DISABLED;
  if (aAllStates || ns->ShouldDispatchRelevant()) {
    SetSingleState(element, tmp, eEvent_Enabled);
  }

  nsresult rv = xtfWrap->SetIntrinsicState(state);
  NS_ENSURE_SUCCESS(rv, rv);

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
           NS_ConvertUCS2toUTF8(name).get(),
           (void*) node);
  }
#endif

  // Revalidate nodes
  mMDG.Revalidate(&mChangedNodes);

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::Refresh()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Refresh()\n");
#endif
  nsPostRefresh postRefresh = nsPostRefresh();

  // Iterate over all form controls if not during initialization phase (then
  // this is handled in InitializeControls())
  if (mDocumentLoaded) {
    PRInt32 controlCount = mFormControls.Count();
    for (PRInt32 i = 0; i < controlCount; ++i) {
      nsIXFormsControl* control = NS_STATIC_CAST(nsIXFormsControl*, mFormControls[i]);
      /// @todo If a control is removed because of previous control has been
      /// refreshed, we do, obviously, not need to refresh it. So mFormControls
      /// should have weak bindings to the controls I guess? (XXX)
      ///
      /// This could happen for \<repeatitem\>s for example.
      if (!control) {
        continue;
      }

      // Get bound node
      nsCOMPtr<nsIDOMNode> boundNode;
      control->GetBoundNode(getter_AddRefs(boundNode));

      PRBool rebind = PR_FALSE;
      PRBool refresh = PR_FALSE;

      if (mNeedsRefresh) {
        refresh = PR_TRUE;
      } else {
        // Get dependencies
        nsCOMArray<nsIDOMNode> *deps = nsnull;
        control->GetDependencies(&deps);    

#ifdef DEBUG_MODEL
        PRUint32 depCount = deps ? deps->Count() : 0;
        nsCOMPtr<nsIDOMElement> controlElement;
        control->GetElement(getter_AddRefs(controlElement));
        if (controlElement) {
          printf("Checking control: ");      
          //DBG_TAGINFO(controlElement);
          nsAutoString boundName;
          if (boundNode)
            boundNode->GetNodeName(boundName);
          printf("\tDependencies: %d, Bound to: '%s' [%p]\n",
                 depCount,
                 NS_ConvertUCS2toUTF8(boundName).get(),
                 (void*) boundNode);

          nsAutoString depNodeName;
          for (PRUint32 t = 0; t < depCount; ++t) {
            nsCOMPtr<nsIDOMNode> tmpdep = deps->ObjectAt(t);
            if (tmpdep) {
              tmpdep->GetNodeName(depNodeName);
              printf("\t\t%s [%p]\n",
                     NS_ConvertUCS2toUTF8(depNodeName).get(),
                     (void*) tmpdep);
            }
          }
        }
#endif

        nsCOMPtr<nsIDOM3Node> curChanged;

        for (PRInt32 j = 0; j < mChangedNodes.Count(); ++j) {
          curChanged = do_QueryInterface(mChangedNodes[j]);

          // Check whether the bound node is dirty. If so, we need to refresh the
          // control (get updated node value from the bound node)
          if (!refresh && boundNode) {
            curChanged->IsSameNode(boundNode, &refresh);
        
            if (refresh)
              // We need to refresh the control. We cannot break out of the loop
              // as we need to check dependencies
              continue;
          }

          // Check whether any dependencies are dirty. If so, we need to rebind
          // the control (re-evaluate it's binding expression)
          for (PRInt32 k = 0; k < deps->Count(); ++k) {
            /// @note beaufour: I'm not to happy about this ...
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

      if (rebind) {
        control->Bind();
        control->GetBoundNode(getter_AddRefs(boundNode));
      }
      if (rebind || refresh) {
        nsresult rv = SetStatesInternal(control, boundNode);
        NS_ENSURE_SUCCESS(rv, rv);
        control->Refresh();
      }
    }

    mChangedNodes.Clear();
    mNeedsRefresh = PR_FALSE;
  }
  
  mMDG.ClearDispatchFlags();

  return NS_OK;
}

// nsISchemaLoadListener

NS_IMETHODIMP
nsXFormsModelElement::OnLoad(nsISchema* aSchema)
{
  mSchemaCount++;

  if (IsComplete()) {
    nsresult rv = FinishConstruction();
    NS_ENSURE_SUCCESS(rv, rv);

    nsXFormsUtils::DispatchEvent(mElement, eEvent_Refresh);

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
  if (!type.EqualsLiteral("DOMContentLoaded"))
    return NS_OK;

  if (!mInstancesInitialized) {
    // XXX This is for Bug 308106. In Gecko 1.8 DoneAddingChildren is not
    //     called in XUL if the element doesn't have any child nodes.
    InitializeInstances();
  }

  mDocumentLoaded = PR_TRUE;

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
        nsCOMPtr<nsISchema> schema;
        // no need to observe errors via the callback.  instead, rely on
        // this method returning a failure code when it encounters errors.
        rv = mSchemas->ProcessSchemaElement(el, nsnull,
                                            getter_AddRefs(schema));
        if (NS_SUCCEEDED(rv))
          mSchemaCount++;
      }
      if (NS_FAILED(rv)) {
        // this is a fatal error (XXX)
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("schemaLoadError"), mElement);
        nsXFormsUtils::DispatchEvent(mElement, eEvent_LinkException);
        return NS_OK;
      }
    }
    if (IsComplete()) {
      rv = FinishConstruction();
      NS_ENSURE_SUCCESS(rv, rv);
      nsXFormsUtils::DispatchEvent(mElement, eEvent_Refresh);
    }
    mPendingInlineSchemas.Clear();
  }

  // We may still be waiting on external documents to load.
  MaybeNotifyCompletion();

  return NS_OK;
}

// nsIModelElementPrivate

NS_IMETHODIMP
nsXFormsModelElement::AddFormControl(nsIXFormsControl *aControl)
{
  if (mFormControls.IndexOf(aControl) == -1)
    mFormControls.AppendElement(aControl);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::RemoveFormControl(nsIXFormsControl *aControl)
{
  mFormControls.RemoveElement(aControl);
  return NS_OK;
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

  return validator.GetType(schemaTypeName, schemaTypeNamespace, aType);
}

/* static */ nsresult
nsXFormsModelElement::GetTypeAndNSFromNode(nsIDOMNode *aInstanceData,
                                           nsAString &aType, nsAString &aNSUri)
{
  nsAutoString schemaTypePrefix;
  nsresult rv = nsXFormsUtils::ParseTypeFromNode(aInstanceData, aType,
                                                 schemaTypePrefix);

  if(rv == NS_ERROR_NOT_AVAILABLE) {
    // if there is no type assigned, then assume that the type is 'string'
    aNSUri.Assign(NS_LITERAL_STRING(NS_NAMESPACE_XML_SCHEMA));
    aType.Assign(NS_LITERAL_STRING("string"));
    rv = NS_OK;
  } else {
    if (schemaTypePrefix.IsEmpty()) {
      aNSUri.AssignLiteral("");
    } else {
      // get the namespace url from the prefix
      nsCOMPtr<nsIDOM3Node> domNode3 = do_QueryInterface(mElement, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = domNode3->LookupNamespaceURI(schemaTypePrefix, aNSUri);
    }
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
  --mPendingInstanceCount;
  if (!aSuccess) {
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("instanceLoadError"), mElement);
    nsXFormsUtils::DispatchEvent(mElement, eEvent_LinkException);
  } else if (IsComplete()) {
    nsresult rv = FinishConstruction();
    if (NS_SUCCEEDED(rv)) {
      nsXFormsUtils::DispatchEvent(mElement, eEvent_Refresh);
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
nsXFormsModelElement::SetNodeValue(nsIDOMNode      *aContextNode,
                                   const nsAString &aNodeValue,
                                   PRBool          *aNodeChanged)
{ 
  return mMDG.SetNodeValue(aContextNode,
                           aNodeValue,
                           aNodeChanged);
}

NS_IMETHODIMP
nsXFormsModelElement::GetNodeValue(nsIDOMNode *aContextNode,
                                   nsAString  &aNodeValue)
{
  return mMDG.GetNodeValue(aContextNode,
                           aNodeValue);
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

/*
 *  SUBMIT_SERIALIZE_NODE   - node is to be serialized
 *  SUBMIT_SKIP_NODE        - node is not to be serialized
 *  SUBMIT_ABORT_SUBMISSION - abort submission (invalid node or empty required node)
 */
NS_IMETHODIMP
nsXFormsModelElement::HandleInstanceDataNode(nsIDOMNode *aInstanceDataNode, unsigned short *aResult)
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

NS_IMETHODIMP
nsXFormsModelElement::GetLazyAuthored(PRBool *aLazyInstance)
{
  *aLazyInstance = mLazyModel;
  return NS_OK;
}

// internal methods

already_AddRefed<nsIDOMDocument>
nsXFormsModelElement::FindInstanceDocument(const nsAString &aID)
{
  nsCOMPtr<nsIInstanceElementPrivate> instance;
  nsXFormsModelElement::FindInstanceElement(aID, getter_AddRefs(instance));

  nsIDOMDocument *doc = nsnull;
  if (instance)
    instance->GetDocument(&doc); // addrefs

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
  nsCOMPtr<nsIXFormsXPathEvaluator> xpath = 
           do_CreateInstance("@mozilla.org/dom/xforms-xpath-evaluator;1", &rv);
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
                         nsCOMPtr<nsIDOMElement>(do_QueryInterface(child)));
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

void
nsXFormsModelElement::Ready()
{
  BackupOrRestoreInstanceData(PR_FALSE);
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
        // we don't have to check if the schema was already added because
        // nsSchemaLoader::ProcessSchemaElement takes care of that.
        nsCOMPtr<nsISchema> schema;
        nsresult rv = mSchemas->ProcessSchemaElement(element, nsnull,
                                                     getter_AddRefs(schema));
        if (!NS_SUCCEEDED(rv)) {
          nsXFormsUtils::ReportError(NS_LITERAL_STRING("schemaProcessError"), node);
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

  PRInt32 controlCount = mFormControls.Count();
  nsresult rv;
  for (PRInt32 i = 0; i < controlCount; ++i) {
    // Get control
    nsIXFormsControl *control = NS_STATIC_CAST(nsIXFormsControl*,
                                               mFormControls[i]);
    if (!control)
      continue;

    // Rebind
    rv = control->Bind();
    NS_ENSURE_SUCCESS(rv, rv);

    // Get bound node
    nsCOMPtr<nsIDOMNode> boundNode;
    rv = control->GetBoundNode(getter_AddRefs(boundNode));
    NS_ENSURE_SUCCESS(rv, rv);

    // Set MIP states on control
    rv = SetStatesInternal(control, boundNode, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Refresh controls
    rv = control->Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


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
  }

  // Okay, dispatch xforms-model-construct-done and xforms-ready events!
  for (i = 0; i < models->Count(); ++i) {
    nsXFormsModelElement *model =
        NS_STATIC_CAST(nsXFormsModelElement *, models->ElementAt(i));
    nsXFormsUtils::DispatchEvent(model->mElement, eEvent_ModelConstructDone);
  }

  nsXFormsModelElement::ProcessDeferredBinds(domDoc);

  for (i = 0; i < models->Count(); ++i) {
    nsXFormsModelElement *model =
        NS_STATIC_CAST(nsXFormsModelElement *, models->ElementAt(i));
    nsXFormsUtils::DispatchEvent(model->mElement, eEvent_Ready);  
  }
}

static void
DeleteAutoString(void    *aObject,
                 nsIAtom *aPropertyName,
                 void    *aPropertyValue,
                 void    *aData)
{
  delete NS_STATIC_CAST(nsAutoString*, aPropertyValue);
}

nsresult
nsXFormsModelElement::ProcessBind(nsIXFormsXPathEvaluator *aEvaluator,
                                  nsIDOMNode              *aContextNode,
                                  PRInt32                 aContextPosition,
                                  PRInt32                 aContextSize,
                                  nsIDOMElement           *aBindElement)
{
  // Get the model item properties specified by this \<bind\>.
  nsCOMPtr<nsIDOMNSXPathExpression> props[eModel__count];
  nsAutoString propStrings[eModel__count];
  nsresult rv;
  nsAutoString attrStr;

  for (PRUint32 i = 0; i < eModel__count; ++i) {
    sModelPropsList[i]->ToString(attrStr);

    aBindElement->GetAttribute(attrStr, propStrings[i]);
    if (!propStrings[i].IsEmpty() &&
        i != eModel_type &&
        i != eModel_p3ptype) {
      rv = aEvaluator->CreateExpression(propStrings[i], aBindElement,
                                        getter_AddRefs(props[i]));
      if (NS_FAILED(rv)) {
        const PRUnichar *strings[] = { propStrings[i].get() };
        nsXFormsUtils::ReportError(NS_LITERAL_STRING("mipParseError"),
                                   strings, 1, aBindElement, aBindElement);
        nsXFormsUtils::DispatchEvent(mElement, eEvent_ComputeException);
        return rv;
      }
    }
  }

  // Find the nodeset that this bind applies to.
  nsCOMPtr<nsIDOMXPathResult> result;

  nsAutoString expr;
  aBindElement->GetAttribute(NS_LITERAL_STRING("nodeset"), expr);
  if (expr.IsEmpty()) {
    expr = NS_LITERAL_STRING(".");
  }
  rv = aEvaluator->Evaluate(expr, aContextNode, aContextSize, aContextPosition,
                            aBindElement,
                            nsIDOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                            nsnull, getter_AddRefs(result));
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    printf("xforms-binding-exception: XPath Evaluation failed\n");
#endif
    const PRUnichar *strings[] = { expr.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("nodesetEvaluateError"),
                               strings, 1, aBindElement, aBindElement);
    nsXFormsUtils::DispatchEvent(mElement, eEvent_BindingException);
    return rv;
  }

  NS_ENSURE_STATE(result);

  PRUint32 snapLen;
  rv = result->GetSnapshotLength(&snapLen);
  NS_ENSURE_SUCCESS(rv, rv);
  

  // Iterate over resultset
  nsCOMArray<nsIDOMNode> deps;
  nsCOMPtr<nsIDOMNode> node;
  PRUint32 snapItem;
  for (snapItem = 0; snapItem < snapLen; ++snapItem) {
    rv = result->SnapshotItem(snapItem, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!node) {
      NS_WARNING("nsXFormsModelElement::ProcessBind(): Empty node in result set.");
      continue;
    }
    

    // Apply MIPs
    nsXFormsXPathParser parser;
    nsXFormsXPathAnalyzer analyzer(aEvaluator, aBindElement);
    PRBool multiMIP = PR_FALSE;
    for (PRUint32 j = 0; j < eModel__count; ++j) {
      if (propStrings[j].IsEmpty())
        continue;

      // type and p3ptype are stored as properties on the instance node
      if (j == eModel_type || j == eModel_p3ptype) {
        nsAutoPtr<nsAutoString> prop (new nsAutoString(propStrings[j]));
        nsCOMPtr<nsIContent> content = do_QueryInterface(node);
        if (content) {
          rv = content->SetProperty(sModelPropsList[j],
                                    prop,
                                    DeleteAutoString);
        } else {
          nsCOMPtr<nsIAttribute> attribute = do_QueryInterface(node);
          if (attribute) {
            rv = attribute->SetProperty(sModelPropsList[j],
                                        prop,
                                        DeleteAutoString);
          } else {
            NS_WARNING("node is neither nsIContent or nsIAttribute");
            continue;
          }
        }
        if (NS_SUCCEEDED(rv)) {
          prop.forget();
        } else {
          return rv;
        }
        if (rv == NS_PROPTABLE_PROP_OVERWRITTEN) {
          multiMIP = PR_TRUE;
          break;
        }

        if (j == eModel_type) {
          // Inform MDG that it needs to check type. The only arguments
          // actually used are |eModel_constraint| and |node|.
          rv = mMDG.AddMIP(eModel_constraint, nsnull, nsnull, PR_FALSE, node, 1,
                           1);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      } else {
        // the rest of the MIPs are given to the MDG
        nsCOMPtr<nsIDOMNSXPathExpression> expr = props[j];

        // Get node dependencies
        nsAutoPtr<nsXFormsXPathNode> xNode(parser.Parse(propStrings[j]));
        deps.Clear();
        rv = analyzer.Analyze(node, xNode, expr, &propStrings[j], &deps,
                              snapItem + 1, snapLen);
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
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::SetStates(nsIXFormsControl *aControl, nsIDOMNode *aBoundNode)
{
  return SetStatesInternal(aControl, aBoundNode, PR_TRUE);
}

nsresult
nsXFormsModelElement::AddInstanceElement(nsIInstanceElementPrivate *aInstEle) 
{
  NS_ENSURE_STATE(mInstanceDocuments);
  mInstanceDocuments->AddInstance(aInstEle);

  return NS_OK;
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

static void
DeleteBindList(void    *aObject,
               nsIAtom *aPropertyName,
               void    *aPropertyValue,
               void    *aData)
{
  delete NS_STATIC_CAST(nsCOMArray<nsIXFormsControl> *, aPropertyValue);
}

/* static */ nsresult
nsXFormsModelElement::DeferElementBind(nsIDOMDocument *aDoc, 
                                       nsIXFormsControlBase *aControl)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);

  if (!doc || !aControl) {
    return NS_ERROR_FAILURE;
  }

  nsCOMArray<nsIXFormsControlBase> *deferredBindList =
      NS_STATIC_CAST(nsCOMArray<nsIXFormsControlBase> *,
                    doc->GetProperty(nsXFormsAtoms::deferredBindListProperty));

  if (!deferredBindList) {
    deferredBindList = new nsCOMArray<nsIXFormsControlBase>(16);
    NS_ENSURE_TRUE(deferredBindList, NS_ERROR_OUT_OF_MEMORY);

    doc->SetProperty(nsXFormsAtoms::deferredBindListProperty, deferredBindList,
                     DeleteBindList);
  }

  // always append to the end of the list.  We need to keep the elements in
  // document order when we process the binds later.  Otherwise we have trouble
  // when an element is trying to bind and should use its parent as a context
  // for the xpath evaluation but the parent isn't bound yet.
  deferredBindList->AppendObject(aControl);

  return NS_OK;
}

/* static */ void
nsXFormsModelElement::ProcessDeferredBinds(nsIDOMDocument *aDoc)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);

  if (!doc) {
    return;
  }

  nsPostRefresh postRefresh = nsPostRefresh();

  doc->SetProperty(nsXFormsAtoms::readyForBindProperty, doc);

  nsCOMArray<nsIXFormsControlBase> *deferredBindList =
      NS_STATIC_CAST(nsCOMArray<nsIXFormsControlBase> *,
                     doc->GetProperty(nsXFormsAtoms::deferredBindListProperty));

  if (deferredBindList) {
    for (int i = 0; i < deferredBindList->Count(); ++i) {
      nsIXFormsControlBase *base = deferredBindList->ObjectAt(i);
      if (base) {
        base->Bind();
        base->Refresh();
      }
    }

    doc->DeleteProperty(nsXFormsAtoms::deferredBindListProperty);
  }
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
    if (NS_SUCCEEDED(instance->GetDocument(getter_AddRefs(doc))) && doc) {
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

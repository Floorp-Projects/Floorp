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
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNodeList.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsXFormsAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsINodeInfo.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMXPathExpression.h"
#include "nsIScriptGlobalObject.h"
#include "nsIContent.h"
#include "nsNetUtil.h"
#include "nsXFormsControl.h"
#include "nsXFormsTypes.h"
#include "nsXFormsXPathParser.h"
#include "nsXFormsXPathAnalyzer.h"
#include "nsXFormsInstanceElement.h"

#include "nsISchemaLoader.h"
#include "nsAutoPtr.h"

#ifdef DEBUG_beaufour
#include "nsIDOMSerializer.h"
#endif

static const nsIID sScriptingIIDs[] = {
  NS_IDOMELEMENT_IID,
  NS_IDOMEVENTTARGET_IID,
  NS_IDOM3NODE_IID,
  NS_IXFORMSMODELELEMENT_IID
};

struct EventData
{
  const char *name;
  PRBool      canCancel;
  PRBool      canBubble;
};

static const EventData sModelEvents[] = {
  { "xforms-model-construct",      PR_FALSE, PR_TRUE },
  { "xforms-model-construct-done", PR_FALSE, PR_TRUE },
  { "xforms-ready",                PR_FALSE, PR_TRUE },
  { "xforms-model-destruct",       PR_FALSE, PR_TRUE },
  { "xforms-rebuild",              PR_TRUE,  PR_TRUE },
  { "xforms-refresh",              PR_TRUE,  PR_TRUE },
  { "xforms-revalidate",           PR_TRUE,  PR_TRUE },
  { "xforms-recalculate",          PR_TRUE,  PR_TRUE },
  { "xforms-reset",                PR_TRUE,  PR_TRUE },
  { "xforms-binding-exception",    PR_FALSE, PR_TRUE },
  { "xforms-link-exception",       PR_FALSE, PR_TRUE },
  { "xforms-link-error",           PR_FALSE, PR_TRUE },
  { "xforms-compute-exception",    PR_FALSE, PR_TRUE }
};

static nsIAtom* sModelPropsList[eModel__count];

nsXFormsModelElement::nsXFormsModelElement()
  : mElement(nsnull),
    mSchemaCount(0),
    mPendingInstanceCount(0)
{
}

NS_IMPL_ADDREF(nsXFormsModelElement)
NS_IMPL_RELEASE(nsXFormsModelElement)

NS_INTERFACE_MAP_BEGIN(nsXFormsModelElement)
  NS_INTERFACE_MAP_ENTRY(nsIXTFElement)
  NS_INTERFACE_MAP_ENTRY(nsIXTFGenericElement)
  NS_INTERFACE_MAP_ENTRY(nsIXTFPrivate)
  NS_INTERFACE_MAP_ENTRY(nsIXFormsModelElement)
  NS_INTERFACE_MAP_ENTRY(nsISchemaLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebServiceErrorHandler)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXTFElement)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsXFormsModelElement::OnDestroyed()
{
  RemoveModelFromDocument();

  mElement = nsnull;
  return NS_OK;
}

void
nsXFormsModelElement::RemoveModelFromDocument()
{
  // Find out if we are handling the model-construct-done for this document.
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
  nsIScriptGlobalObject *window = nsnull;
  if (doc)
    window = doc->GetScriptGlobalObject();
  nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(window);
  if (targ) {
    nsVoidArray *models = NS_STATIC_CAST(nsVoidArray*,
                          doc->GetProperty(nsXFormsAtoms::modelListProperty));

    if (models) {
      if (models->SafeElementAt(0) == this) {
        nsXFormsModelElement *next =
          NS_STATIC_CAST(nsXFormsModelElement*, models->SafeElementAt(1));
        if (next) {
          targ->AddEventListener(NS_LITERAL_STRING("load"), next, PR_TRUE);
        }

        targ->RemoveEventListener(NS_LITERAL_STRING("load"), this, PR_TRUE);
      }

      models->RemoveElement(this);
    }
  }
}

NS_IMETHODIMP
nsXFormsModelElement::GetElementType(PRUint32 *aType)
{
  *aType = ELEMENT_TYPE_GENERIC_ELEMENT;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetIsAttributeHandler(PRBool *aIsHandler)
{
  *aIsHandler = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::GetScriptingInterfaces(PRUint32 *aCount, nsIID ***aArray)
{
  return CloneScriptingInterfaces(sScriptingIIDs,
                                  NS_ARRAY_LENGTH(sScriptingIIDs),
                                  aCount, aArray);
}

NS_IMETHODIMP
nsXFormsModelElement::WillChangeDocument(nsIDOMDocument* aNewDocument)
{
  RemoveModelFromDocument();
  return NS_OK;
}

static void
DeleteVoidArray(void    *aObject,
                nsIAtom *aPropertyName,
                void    *aPropertyValue,
                void    *aData)
{
  delete NS_STATIC_CAST(nsVoidArray*, aPropertyValue);
}

NS_IMETHODIMP
nsXFormsModelElement::DocumentChanged(nsIDOMDocument* aNewDocument)
{
  // Add this model to the document's model list.  If this is the first
  // model to be created, register an onload handler so that we can
  // do model-construct-done notifications.

  if (!aNewDocument)
    return NS_OK;

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aNewDocument);

  nsVoidArray *models = NS_STATIC_CAST(nsVoidArray*,
                  doc->GetProperty(nsXFormsAtoms::modelListProperty));

  if (!models) {
    models = new nsVoidArray(16);
    doc->SetProperty(nsXFormsAtoms::modelListProperty,
                     models, DeleteVoidArray);

    nsIScriptGlobalObject *window = doc->GetScriptGlobalObject();

    nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(window);
    targ->AddEventListener(NS_LITERAL_STRING("load"), this, PR_TRUE);
  }

  models->AppendElement(this);
  
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::WillChangeParent(nsIDOMElement* aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::ParentChanged(nsIDOMElement* aNewParent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::WillInsertChild(nsIDOMNode* aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::ChildInserted(nsIDOMNode* aChild, PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::WillAppendChild(nsIDOMNode* aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::ChildAppended(nsIDOMNode* aChild)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::WillRemoveChild(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::ChildRemoved(PRUint32 aIndex)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::WillSetAttribute(nsIAtom *aName,
                                       const nsAString &aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::AttributeSet(nsIAtom *aName, const nsAString &aNewValue)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::WillRemoveAttribute(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::AttributeRemoved(nsIAtom *aName)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::DoneAddingChildren()
{
  // We wait until all children are added to dispatch xforms-model-construct,
  // since the model may have an action handler for this event.

  nsresult rv = DispatchEvent(eEvent_ModelConstruct);
  NS_ENSURE_SUCCESS(rv, rv);

  // xforms-model-construct is not cancellable, so always proceed.
  // We continue here rather than doing this in HandleEvent since we know
  // it only makes sense to perform this default action once.

  // (XForms 4.2.1)
  // 1. load xml schemas

  nsAutoString schemaList;
  mElement->GetAttribute(NS_LITERAL_STRING("schema"), schemaList);
  if (!schemaList.IsEmpty()) {
    nsCOMPtr<nsISchemaLoader> loader = do_GetService(NS_SCHEMALOADER_CONTRACTID);
    NS_ENSURE_TRUE(loader, NS_ERROR_FAILURE);

    // Parse the space-separated list.
    PRUint32 offset = 0;
    nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
    nsRefPtr<nsIURI> baseURI = content->GetBaseURI();

    while (1) {
      ++mSchemaCount;
      PRInt32 index = schemaList.FindChar(PRUnichar(' '), offset);

      nsCOMPtr<nsIURI> newURI;
      NS_NewURI(getter_AddRefs(newURI),
                Substring(schemaList, offset, index - offset),
                nsnull, baseURI);

      if (!newURI) {
        DispatchEvent(eEvent_LinkException);  // this is a fatal error
        return NS_OK;
      }

      nsCAutoString uriSpec;
      newURI->GetSpec(uriSpec);

      rv = loader->LoadAsync(NS_ConvertUTF8toUTF16(uriSpec), this);
      if (NS_FAILED(rv)) {
        DispatchEvent(eEvent_LinkException);  // this is a fatal error
        return NS_OK;
      }
      if (index == -1)
        break;

      offset = index + 1;
    }
  }

  // 2. construct an XPath data model from inline or external initial instance
  // data.  This is done by our child instance elements as they are inserted
  // into the document, and all of the instances will be processed by this
  // point.

  // XXX schema and external instance data loads should delay document onload

  if (IsComplete()) {
    return FinishConstruction();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled)
{
  *aHandled = PR_TRUE;

  nsAutoString type;
  aEvent->GetType(type);

  if (type.EqualsLiteral("xforms-refresh")) {
    // refresh all of our form controls
    PRInt32 controlCount = mFormControls.Count();
    for (PRInt32 i = 0; i < controlCount; ++i) {
      NS_STATIC_CAST(nsXFormsControl*, mFormControls[i])->Refresh();
    }
  } else if (type.EqualsLiteral("xforms-revalidate")) {
    Revalidate();
  } else if (type.EqualsLiteral("xforms-recalculate")) {
    Recalculate();
  } else if (type.EqualsLiteral("xforms-rebuild")) {
    Rebuild();
  } else if (type.EqualsLiteral("xforms-reset")) {
#ifdef DEBUG
    printf("nsXFormsModelElement::Reset()\n");
#endif    
  } else {
    *aHandled = PR_FALSE;
  }

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

  nsresult rv = mMDG.Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// nsIXTFPrivate
NS_IMETHODIMP
nsXFormsModelElement::GetInner(nsISupports **aInner)
{
  NS_ENSURE_ARG_POINTER(aInner);
  NS_ADDREF(*aInner = NS_STATIC_CAST(nsIXFormsModelElement*, this));
  return NS_OK;
}

// nsIXFormsModelElement

NS_IMETHODIMP
nsXFormsModelElement::GetInstanceDocument(const nsAString& aInstanceID,
                                          nsIDOMDocument **aDocument)
{
  NS_ENSURE_ARG_POINTER(aDocument);

  NS_IF_ADDREF(*aDocument = FindInstanceDocument(aInstanceID));
  return *aDocument ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXFormsModelElement::Rebuild()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Rebuild()\n");
#endif

  // TODO: Clear graph and re-attach elements

  // 1 . Clear graph
  // mMDG.Clear();

  // 2. Re-attach all elements

  // 3. Rebuild graph
  return mMDG.Rebuild();
}

NS_IMETHODIMP
nsXFormsModelElement::Recalculate()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Recalculate()\n");
#endif
  
  nsXFormsMDGSet changedNodes;
  // TODO: Handle changed nodes. That is, dispatch events, etc.
  
  return mMDG.Recalculate(changedNodes);
}

NS_IMETHODIMP
nsXFormsModelElement::Revalidate()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Revalidate()\n");
#endif

#ifdef DEBUG_beaufour
  // Dump instance document to stdout
  nsresult rv;
  nsCOMPtr<nsIDOMSerializer> serializer(do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: Should use SerializeToStream and write directly to stdout...
  nsAutoString instanceString;
  rv = serializer->SerializeToString(mInstanceDocument, instanceString);
  NS_ENSURE_SUCCESS(rv, rv);
  
  printf("Instance data:\n%s\n", NS_ConvertUCS2toUTF8(instanceString).get());
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::Refresh()
{
#ifdef DEBUG
  printf("nsXFormsModelElement::Refresh()\n");
#endif

  return NS_OK;
}

// nsISchemaLoadListener

NS_IMETHODIMP
nsXFormsModelElement::OnLoad(nsISchema* aSchema)
{
  mSchemas.AppendObject(aSchema);
  if (IsComplete()) {
    nsresult rv = FinishConstruction();
    NS_ENSURE_SUCCESS(rv, rv);

    DispatchEvent(eEvent_Refresh);
  }

  return NS_OK;
}

// nsIWebServiceErrorHandler

NS_IMETHODIMP
nsXFormsModelElement::OnError(nsresult aStatus,
                              const nsAString &aStatusMessage)
{
  DispatchEvent(eEvent_LinkException);
  return NS_OK;
}

// nsIDOMEventListener

NS_IMETHODIMP
nsXFormsModelElement::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::Load(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  aEvent->GetTarget(getter_AddRefs(target));

  nsCOMPtr<nsIDocument> document = do_QueryInterface(target);
  if (document) {
    // The document has finished loading; that means that all of the models
    // in it are initialized.  Fire the model-construct-done event to each
    // model.

    nsVoidArray *models = NS_STATIC_CAST(nsVoidArray*,
                      document->GetProperty(nsXFormsAtoms::modelListProperty));

    NS_ASSERTION(models, "models list is empty!");
    for (PRInt32 i = 0; i < models->Count(); ++i) {
      NS_STATIC_CAST(nsXFormsModelElement*, models->ElementAt(i))
        ->DispatchEvent(eEvent_ModelConstructDone);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::BeforeUnload(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::Unload(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::Abort(nsIDOMEvent* aEvent)
{
  DispatchEvent(eEvent_LinkException);
  return NS_OK;
}

NS_IMETHODIMP
nsXFormsModelElement::Error(nsIDOMEvent* aEvent)
{
  DispatchEvent(eEvent_LinkException);
  return NS_OK;
}

// internal methods

nsIDOMDocument*
nsXFormsModelElement::FindInstanceDocument(const nsAString &aID)
{
  nsCOMPtr<nsIDOMNodeList> children;
  mElement->GetChildNodes(getter_AddRefs(children));

  if (!children)
    return nsnull;

  PRUint32 childCount = 0;
  children->GetLength(&childCount);

  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMElement> element;
  nsAutoString id;

  for (PRUint32 i = 0; i < childCount; ++i) {
    children->Item(i, getter_AddRefs(node));
    NS_ASSERTION(node, "incorrect NodeList length?");

    element = do_QueryInterface(node);
    if (!element)
      continue;

    element->GetAttribute(NS_LITERAL_STRING("id"), id);
    if (aID.IsEmpty() || aID.Equals(id)) {
      // make sure this is an xforms instance element
      nsAutoString namespaceURI, localName;
      element->GetNamespaceURI(namespaceURI);
      element->GetLocalName(localName);

      if (localName.EqualsLiteral("instance") &&
          namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS)) {

        // This is the requested instance, so get its document.
        nsCOMPtr<nsIXTFPrivate> xtfPriv = do_QueryInterface(element);
        NS_ENSURE_TRUE(xtfPriv, nsnull);

        nsCOMPtr<nsISupports> instanceInner;
        xtfPriv->GetInner(getter_AddRefs(instanceInner));
        NS_ENSURE_TRUE(instanceInner, nsnull);

        nsISupports *isupp = NS_STATIC_CAST(nsISupports*, instanceInner.get());
        nsXFormsInstanceElement *instance =
          NS_STATIC_CAST(nsXFormsInstanceElement*,
                         NS_STATIC_CAST(nsIXTFGenericElement*, isupp));

        return instance->GetDocument();
      }
    }
  }

  return nsnull;
}

nsresult
nsXFormsModelElement::FinishConstruction()
{
  // 3. if applicable, initialize P3P

  // 4. construct instance data from initial instance data.  apply all
  // <bind> elements in document order.

  // we get the instance data from our instance child nodes

  nsIDOMDocument *firstInstanceDoc = FindInstanceDocument(EmptyString());
  if (!firstInstanceDoc)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> firstInstanceRoot;
  firstInstanceDoc->GetDocumentElement(getter_AddRefs(firstInstanceRoot));

  nsCOMPtr<nsIDOMXPathEvaluator> xpath = do_QueryInterface(firstInstanceDoc);
  
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
        if (!ProcessBind(xpath, firstInstanceRoot,
                         nsCOMPtr<nsIDOMElement>(do_QueryInterface(child)))) {
          DispatchEvent(eEvent_BindingException);
          return NS_OK;
        }
      }
    }
  }

  // 5. dispatch xforms-rebuild, xforms-recalculate, xforms-revalidate

  DispatchEvent(eEvent_Rebuild);
  DispatchEvent(eEvent_Recalculate);
  DispatchEvent(eEvent_Revalidate);

  // We're done initializing this model.

  return NS_OK;
}

static void
ReleaseExpr(void    *aElement,
            nsIAtom *aPropertyName,
            void    *aPropertyValue,
            void    *aData)
{
  nsIDOMXPathExpression *expr = NS_STATIC_CAST(nsIDOMXPathExpression*,
                                               aPropertyValue);

  NS_RELEASE(expr);
}

PRBool
nsXFormsModelElement::ProcessBind(nsIDOMXPathEvaluator *aEvaluator,
                                  nsIDOMNode    *aContextNode,
                                  nsIDOMElement *aBindElement)
{
  // Get the expression for the nodes that this <bind> applies to.
  nsAutoString expr;
  aBindElement->GetAttribute(NS_LITERAL_STRING("nodeset"), expr);
  if (expr.IsEmpty())
    return PR_TRUE;

  nsCOMPtr<nsIDOMXPathNSResolver> resolver;
  aEvaluator->CreateNSResolver(aBindElement, getter_AddRefs(resolver));

  // Get the model item properties specified by this <bind>.
  nsCOMPtr<nsIDOMXPathExpression> props[eModel__count];
  nsAutoString exprStrings[eModel__count];
  PRInt32 propCount = 0;
  nsresult rv = NS_OK;
  nsAutoString attrStr;

  for (int i = 0; i < eModel__count; ++i) {
    sModelPropsList[i]->ToString(attrStr);

    aBindElement->GetAttribute(attrStr, exprStrings[i]);
    if (!exprStrings[i].IsEmpty()) {
      rv = aEvaluator->CreateExpression(exprStrings[i], resolver,
                                        getter_AddRefs(props[i]));
      if (NS_FAILED(rv))
        return PR_FALSE;

      ++propCount;
    }
  }

  if (propCount == 0)
    return PR_TRUE;  // successful, but nothing to do

  nsCOMPtr<nsIDOMXPathResult> result;
  rv = aEvaluator->Evaluate(expr, aContextNode, resolver,
                            nsIDOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                            nsnull, getter_AddRefs(result));
  if (NS_FAILED(rv))
    return PR_FALSE;

  PRUint32 snapLen;
  rv = result->GetSnapshotLength(&snapLen);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsXFormsMDGSet set;
  nsCOMPtr<nsIDOMNode> node;
  PRInt32 contextPosition = 1;
  for (PRUint32 snapItem = 0; snapItem < snapLen; ++snapItem) {
    rv = result->SnapshotItem(snapItem, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (!node) {
      NS_WARNING("nsXFormsModelElement::ProcessBind(): Empty node in result set.");
      continue;
    }
    
    nsXFormsXPathParser parser;
    nsXFormsXPathAnalyzer analyzer(aEvaluator, resolver);
    
    // We must check whether the properties already exist on the node.
    for (int j = 0; j < eModel__count; ++j) {
      if (props[j]) {
        nsCOMPtr<nsIContent> content = do_QueryInterface(node, &rv);

        if (NS_FAILED(rv)) {
          NS_WARNING("nsXFormsModelElement::ProcessBind(): Node is not IContent!\n");
          continue;
        }

        nsIDOMXPathExpression *expr = props[j];
        NS_ADDREF(expr);

        // Set property
        rv = content->SetProperty(sModelPropsList[j], expr, ReleaseExpr);
        if (rv == NS_PROPTABLE_PROP_OVERWRITTEN) {
          return PR_FALSE;
        }
        
        // Get node dependencies
        nsAutoPtr<nsXFormsXPathNode> xNode(parser.Parse(exprStrings[j]));
        set.Clear();
        rv = analyzer.Analyze(node, xNode, expr, &exprStrings[j], &set);
        NS_ENSURE_SUCCESS(rv, rv);
        
        // Insert into MDG
        rv = mMDG.AddMIP((ModelItemPropName) j, expr, &set, parser.UsesDynamicFunc(),
                         node, contextPosition++, snapLen);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  
  return PR_TRUE;
}

nsresult
nsXFormsModelElement::DispatchEvent(nsXFormsModelEvent aEvent)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  mElement->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMDocumentEvent> doc = do_QueryInterface(domDoc);

  nsCOMPtr<nsIDOMEvent> event;
  doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  const EventData *data = &sModelEvents[aEvent];
  event->InitEvent(NS_ConvertUTF8toUTF16(data->name),
                   data->canBubble, data->canCancel);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mElement);
  PRBool cancelled;
  return target->DispatchEvent(event, &cancelled);
}

already_AddRefed<nsISchemaType>
nsXFormsModelElement::GetTypeForControl(nsXFormsControl *aControl)
{
  return nsnull;
}

void
nsXFormsModelElement::AddFormControl(nsXFormsControl *aControl)
{
  if (mFormControls.IndexOf(aControl) == -1)
    mFormControls.AppendElement(aControl);
}

void
nsXFormsModelElement::RemoveFormControl(nsXFormsControl *aControl)
{
  mFormControls.RemoveElement(aControl);
}

void
nsXFormsModelElement::RemovePendingInstance()
{
  --mPendingInstanceCount;
  if (IsComplete()) {
    nsresult rv = FinishConstruction();
    if (NS_SUCCEEDED(rv))
      DispatchEvent(eEvent_Refresh);
  }
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

nsresult
NS_NewXFormsModelElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsModelElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}

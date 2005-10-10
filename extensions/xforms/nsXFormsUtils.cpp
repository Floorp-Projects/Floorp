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

#include "nsXFormsUtils.h"
#include "nsString.h"
#include "nsXFormsAtoms.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMNodeList.h"
#include "nsIXFormsXPathEvaluator.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsIModelElementPrivate.h"
#include "nsIXFormsModelElement.h"
#include "nsIXFormsControl.h"
#include "nsIInstanceElementPrivate.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMLocation.h"
#include "nsIDOMSerializer.h"
#include "nsIContent.h"
#include "nsIAttribute.h"
#include "nsXFormsAtoms.h"
#include "nsIXFormsRepeatElement.h"

#include "nsIXFormsContextControl.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsDataHashtable.h"

#include "nsAutoPtr.h"
#include "nsXFormsXPathAnalyzer.h"
#include "nsXFormsXPathParser.h"
#include "nsXFormsXPathNode.h"
#include "nsIDOMNSXPathExpression.h"
#include "nsArray.h"

#include "nsIScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIXFormsUtilityService.h"
#include "nsIDOMAttr.h"
#include "nsIDOM3Node.h"
#include "nsIConsoleService.h"
#include "nsIStringBundle.h"
#include "nsIDOMNSEvent.h"
#include "nsIURI.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIParserService.h"

#define CANCELABLE 0x01
#define BUBBLES    0x02

const EventData sXFormsEventsEntries[42] = {
  { "xforms-model-construct",      PR_FALSE, PR_TRUE  },
  { "xforms-model-construct-done", PR_FALSE, PR_TRUE  },
  { "xforms-ready",                PR_FALSE, PR_TRUE  },
  { "xforms-model-destruct",       PR_FALSE, PR_TRUE  },
  { "xforms-previous",             PR_TRUE,  PR_FALSE },
  { "xforms-next",                 PR_TRUE,  PR_FALSE },
  { "xforms-focus",                PR_TRUE,  PR_FALSE },
  { "xforms-help",                 PR_TRUE,  PR_TRUE  },
  { "xforms-hint",                 PR_TRUE,  PR_TRUE  },
  { "xforms-rebuild",              PR_TRUE,  PR_TRUE  },
  { "xforms-refresh",              PR_TRUE,  PR_TRUE  },
  { "xforms-revalidate",           PR_TRUE,  PR_TRUE  },
  { "xforms-recalculate",          PR_TRUE,  PR_TRUE  },
  { "xforms-reset",                PR_TRUE,  PR_TRUE  },
  { "xforms-submit",               PR_TRUE,  PR_TRUE  },
  { "DOMActivate",                 PR_TRUE,  PR_TRUE  },
  { "xforms-value-changed",        PR_FALSE, PR_TRUE  },
  { "xforms-select",               PR_FALSE, PR_TRUE  },
  { "xforms-deselect",             PR_FALSE, PR_TRUE  },
  { "xforms-scroll-first",         PR_FALSE, PR_TRUE  },
  { "xforms-scroll-last",          PR_FALSE, PR_TRUE  },
  { "xforms-insert",               PR_FALSE, PR_TRUE  },
  { "xforms-delete",               PR_FALSE, PR_TRUE  },
  { "xforms-valid",                PR_FALSE, PR_TRUE  },
  { "xforms-invalid",              PR_FALSE, PR_TRUE  },
  { "DOMFocusIn",                  PR_FALSE, PR_TRUE  },
  { "DOMFocusOut",                 PR_FALSE, PR_TRUE  },
  { "xforms-readonly",             PR_FALSE, PR_TRUE  },
  { "xforms-readwrite",            PR_FALSE, PR_TRUE  },
  { "xforms-required",             PR_FALSE, PR_TRUE  },
  { "xforms-optional",             PR_FALSE, PR_TRUE  },
  { "xforms-enabled",              PR_FALSE, PR_TRUE  },
  { "xforms-disabled",             PR_FALSE, PR_TRUE  },
  { "xforms-in-range",             PR_FALSE, PR_TRUE  },
  { "xforms-out-of-range",         PR_FALSE, PR_TRUE  },
  { "xforms-submit-done",          PR_FALSE, PR_TRUE  },
  { "xforms-submit-error",         PR_FALSE, PR_TRUE  },
  { "xforms-binding-exception",    PR_FALSE, PR_TRUE  },
  { "xforms-link-exception",       PR_FALSE, PR_TRUE  },
  { "xforms-link-error",           PR_FALSE, PR_TRUE  },
  { "xforms-compute-exception",    PR_FALSE, PR_TRUE  },
  { "xforms-moz-hint-off",         PR_FALSE, PR_TRUE  }
};

static const EventData sEventDefaultsEntries[] = {
  //UIEvents already in sXFormsEvents
  
  //MouseEvent
  { "click",                       PR_TRUE,  PR_TRUE  },
  { "mousedown",                   PR_TRUE,  PR_TRUE  },
  { "mouseup",                     PR_TRUE,  PR_TRUE  },
  { "mouseover",                   PR_TRUE,  PR_TRUE  },
  { "mousemove",                   PR_FALSE, PR_TRUE  },
  { "mouseout",                    PR_TRUE,  PR_TRUE  },
  //MutationEvent
  { "DOMSubtreeModified",          PR_FALSE, PR_TRUE  },
  { "DOMNodeInserted",             PR_FALSE, PR_TRUE  },
  { "DOMNodeRemoved",              PR_FALSE, PR_TRUE  },
  { "DOMNodeRemovedFromDocument",  PR_FALSE, PR_FALSE },
  { "DOMNodeInsertedIntoDocument", PR_FALSE, PR_FALSE },
  { "DOMAttrModified",             PR_FALSE, PR_TRUE  },
  { "DOMCharacterDataModified",    PR_FALSE, PR_TRUE  },
  //HTMLEvents
  { "load",                        PR_FALSE, PR_FALSE },
  { "unload",                      PR_FALSE, PR_FALSE },
  { "abort",                       PR_FALSE, PR_TRUE  },
  { "error",                       PR_FALSE, PR_TRUE  },
  { "select",                      PR_FALSE, PR_TRUE  },
  { "change",                      PR_FALSE, PR_TRUE  },
  { "submit",                      PR_TRUE,  PR_TRUE  },
  { "reset",                       PR_FALSE, PR_TRUE  },
  { "focus",                       PR_FALSE, PR_FALSE },
  { "blur",                        PR_FALSE, PR_FALSE },
  { "resize",                      PR_FALSE, PR_TRUE  },
  { "scroll",                      PR_FALSE, PR_TRUE  }
};

static nsDataHashtable<nsStringHashKey,PRUint32> sXFormsEvents;
static nsDataHashtable<nsStringHashKey,PRUint32> sEventDefaults;

/**
 * @todo The attribute names used on the elements to reflect the pseudo class
 * state until bug 271720 is landed. (XXX)
 */
const nsString kStateAttributes[8] = { NS_LITERAL_STRING("valid"),
                                       NS_LITERAL_STRING("invalid"),
                                       NS_LITERAL_STRING("read-only"),
                                       NS_LITERAL_STRING("read-write"),
                                       NS_LITERAL_STRING("required"),
                                       NS_LITERAL_STRING("optional"),
                                       NS_LITERAL_STRING("enabled"),
                                       NS_LITERAL_STRING("disabled") };

/* static */ nsresult
nsXFormsUtils::Init()
{
  if (!sXFormsEvents.Init())
    return NS_ERROR_FAILURE;

  unsigned int i;

  for (i = 0; i < NS_ARRAY_LENGTH(sXFormsEventsEntries); ++i) {
    PRUint32 flag = 0;
    if (sXFormsEventsEntries[i].canCancel)
      flag |= CANCELABLE;
    if (sXFormsEventsEntries[i].canBubble)
      flag |= BUBBLES;
    sXFormsEvents.Put(NS_ConvertUTF8toUTF16(sXFormsEventsEntries[i].name),
                                            flag);
  }

  if (!sEventDefaults.Init())
    return NS_ERROR_FAILURE;
  for (i = 0; i < NS_ARRAY_LENGTH(sEventDefaultsEntries); ++i) {
    PRUint32 flag = 0;
    if (sEventDefaultsEntries[i].canCancel)
      flag |= CANCELABLE;
    if (sEventDefaultsEntries[i].canBubble)
      flag |= BUBBLES;
    sEventDefaults.Put(NS_ConvertUTF8toUTF16(sEventDefaultsEntries[i].name),
                                             flag);
  }
  return NS_OK;
}

/* static */ PRBool
nsXFormsUtils::GetParentModel(nsIDOMElement           *aBindElement,
                              nsIModelElementPrivate **aModel)
{
  PRBool res = PR_TRUE;
  nsCOMPtr<nsIDOMNode> modelWrapper;

  // Walk up the tree looking for the containing model.
  aBindElement->GetParentNode(getter_AddRefs(modelWrapper));

  nsAutoString localName, namespaceURI;
  nsCOMPtr<nsIDOMNode> temp;

  while (modelWrapper) {
    modelWrapper->GetLocalName(localName);
    if (localName.EqualsLiteral("model")) {
      modelWrapper->GetNamespaceURI(namespaceURI);
      if (namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS))
        break;
    }

    temp.swap(modelWrapper);
    temp->GetParentNode(getter_AddRefs(modelWrapper));

    // Model is not the immediate parent, this is a reference to a nested
    // (invalid) bind
    res = PR_FALSE;
  }
  *aModel = nsnull;
  nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelWrapper);
  model.swap(*aModel);

  return res;
}

/**
 * beaufour: Section 7.4 in the specification does a really bad job of
 * explaining how to find the model, so the code below is my interpretation of
 * it...
 *
 * @see http://bugzilla.mozilla.org/show_bug.cgi?id=265216
 */

/* static */ nsresult
nsXFormsUtils::GetNodeContext(nsIDOMElement           *aElement,
                              PRUint32                 aElementFlags,
                              nsIModelElementPrivate **aModel,
                              nsIDOMElement          **aBindElement,
                              PRBool                  *aOuterBind,
                              nsIDOMNode             **aContextNode,
                              PRInt32                 *aContextPosition,
                              PRInt32                 *aContextSize)
{
  NS_ENSURE_ARG(aElement);
  NS_ENSURE_ARG(aOuterBind);
  NS_ENSURE_ARG_POINTER(aContextNode);
  NS_ENSURE_ARG_POINTER(aBindElement);
  *aBindElement = nsnull;

  // Set default context size and position
  if (aContextSize)
    *aContextSize = 1;
  if (aContextPosition)
    *aContextPosition = 1;

  // Find correct model element
  nsCOMPtr<nsIDOMDocument> domDoc;
  aElement->GetOwnerDocument(getter_AddRefs(domDoc));
  NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

  nsAutoString bindId;
  NS_NAMED_LITERAL_STRING(bindStr, "bind");
  aElement->GetAttribute(bindStr, bindId);
  if (!bindId.IsEmpty()) {
    // CASE 1: Use @bind
    domDoc->GetElementById(bindId, aBindElement);
    if (!IsXFormsElement(*aBindElement, bindStr)) {
      const PRUnichar *strings[] = { bindId.get(), bindStr.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("idRefError"),
                                 strings, 2, aElement, aElement);
      DispatchEvent(aElement, eEvent_BindingException);
      return NS_ERROR_ABORT;
    }

    *aOuterBind = GetParentModel(*aBindElement, aModel);
    NS_ENSURE_STATE(*aModel);
  } else {
    if (aElementFlags & ELEMENT_WITH_MODEL_ATTR) {
      // CASE 2: Use @model
      // If bind did not set model, and the element has a model attribute we use this
      nsAutoString modelId;
      NS_NAMED_LITERAL_STRING(modelStr, "model");
      aElement->GetAttribute(modelStr, modelId);
      
      if (!modelId.IsEmpty()) {
        nsCOMPtr<nsIDOMElement> modelElement;
        domDoc->GetElementById(modelId, getter_AddRefs(modelElement));
        nsCOMPtr<nsIModelElementPrivate> model = do_QueryInterface(modelElement);
        
        // No element found, or element not a \<model\> element
        if (!model) {
          const PRUnichar *strings[] = { modelId.get(), modelStr.get() };
          nsXFormsUtils::ReportError(NS_LITERAL_STRING("idRefError"),
                                     strings, 2, aElement, aElement);
          nsXFormsUtils::DispatchEvent(aElement, eEvent_BindingException);        
          return NS_ERROR_FAILURE;
        }
        
        NS_ADDREF(*aModel = model);
      }
    }

    // Search for a parent setting context for us
    nsresult rv = FindParentContext(aElement,
                                    aModel,
                                    aContextNode,
                                    aContextPosition,
                                    aContextSize);
    // CASE 3/4: Use parent's model / first model in document.
    // If FindParentContext() does not find a parent context but |aModel| is not
    // set, it sets the model to the first model in the document.
  
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // if context node is not set, it's the document element of the model's
  // default instance
  if (!*aContextNode) {
    nsCOMPtr<nsIXFormsModelElement> modelInt = do_QueryInterface(*aModel);
    NS_ENSURE_STATE(modelInt);

    nsCOMPtr<nsIDOMDocument> instanceDoc;
    modelInt->GetInstanceDocument(EmptyString(),
                                  getter_AddRefs(instanceDoc));
    NS_ENSURE_STATE(instanceDoc);

    nsIDOMElement* docElement;
    instanceDoc->GetDocumentElement(&docElement); // addrefs
    NS_ENSURE_STATE(docElement);
    *aContextNode = docElement; // addref'ed above
  }  

  return NS_OK;
}

/* static */ already_AddRefed<nsIModelElementPrivate>
nsXFormsUtils::GetModel(nsIDOMElement  *aElement,
                        PRUint32        aElementFlags)

{
  nsCOMPtr<nsIModelElementPrivate> model;
  nsCOMPtr<nsIDOMNode> contextNode;
  nsCOMPtr<nsIDOMElement> bind;
  PRBool outerbind;

  GetNodeContext(aElement,
                 aElementFlags,
                 getter_AddRefs(model),
                 getter_AddRefs(bind),
                 &outerbind,
                 getter_AddRefs(contextNode));

  NS_ENSURE_TRUE(model, nsnull);

  nsIModelElementPrivate *result = nsnull;
  if (model)
    CallQueryInterface(model, &result);  // addrefs
  return result;
}

/* static */ already_AddRefed<nsIDOMXPathResult>
nsXFormsUtils::EvaluateXPath(const nsAString        &aExpression,
                             nsIDOMNode             *aContextNode,
                             nsIDOMNode             *aResolverNode,
                             PRUint16                aResultType,
                             PRInt32                 aContextPosition,
                             PRInt32                 aContextSize,
                             nsCOMArray<nsIDOMNode> *aSet,
                             nsStringArray          *aIndexesUsed)
{
  nsCOMPtr<nsIDOMDocument> doc;
  aContextNode->GetOwnerDocument(getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, nsnull);

  nsCOMPtr<nsIXFormsXPathEvaluator> eval = 
           do_CreateInstance("@mozilla.org/dom/xforms-xpath-evaluator;1");
  NS_ENSURE_TRUE(eval, nsnull);

  nsCOMPtr<nsIDOMNSXPathExpression> expression;
  eval->CreateExpression(aExpression,
                         aResolverNode,
                         getter_AddRefs(expression));

  nsIDOMXPathResult *result = nsnull;
  PRBool throwException = PR_FALSE;
  if (!expression) {
    const nsPromiseFlatString& flat = PromiseFlatString(aExpression);
    const PRUnichar *strings[] = { flat.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("exprParseError"),
                               strings, 1, aContextNode, nsnull);
    throwException = PR_TRUE;
  } else {
    nsCOMPtr<nsISupports> supResult;
    nsresult rv = expression->EvaluateWithContext(aContextNode,
                                                  aContextPosition,
                                                  aContextSize,
                                                  aResultType,
                                                  nsnull,
                                                  getter_AddRefs(supResult));

    if (NS_SUCCEEDED(rv) && supResult) {
      /// @todo beaufour: This is somewhat "hackish". Hopefully, this will
      /// improve when we integrate properly with Transformiix (XXX)
      /// @see http://bugzilla.mozilla.org/show_bug.cgi?id=265212
      if (aSet) {
        nsXFormsXPathParser parser;
        nsXFormsXPathAnalyzer analyzer(eval, aResolverNode);
        nsAutoPtr<nsXFormsXPathNode> xNode(parser.Parse(aExpression));
        rv = analyzer.Analyze(aContextNode,
                              xNode,
                              expression,
                              &aExpression,
                              aSet,
                              aContextPosition,
                              aContextSize);
        NS_ENSURE_SUCCESS(rv, nsnull);

        if (aIndexesUsed)
          *aIndexesUsed = analyzer.IndexesUsed();
      }
      CallQueryInterface(supResult, &result);  // addrefs
    } else if (rv == NS_ERROR_XFORMS_CALCUATION_EXCEPTION) {
      const nsPromiseFlatString& flat = PromiseFlatString(aExpression);
      const PRUnichar *strings[] = { flat.get() };
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("exprEvaluateError"),
                                 strings, 1, aContextNode, nsnull);
      throwException = PR_TRUE;
    }
  }

  // Throw xforms-compute-exception
  if (throwException) {
    nsCOMPtr<nsIDOMElement> resolverElement = do_QueryInterface(aResolverNode);
    nsCOMPtr<nsIModelElementPrivate> modelPriv = nsXFormsUtils::GetModel(resolverElement);
    nsCOMPtr<nsIDOMNode> model = do_QueryInterface(modelPriv);
    DispatchEvent(model, eEvent_ComputeException);
  }

  return result;
}

/* static */ nsresult
nsXFormsUtils::EvaluateNodeBinding(nsIDOMElement           *aElement,
                                   PRUint32                 aElementFlags,
                                   const nsString          &aBindingAttr,
                                   const nsString          &aDefaultRef,
                                   PRUint16                 aResultType,
                                   nsIModelElementPrivate **aModel,
                                   nsIDOMXPathResult      **aResult,
                                   nsCOMArray<nsIDOMNode>  *aDeps,
                                   nsStringArray           *aIndexesUsed)
{
  if (!aElement || !aModel || !aResult) {
    return NS_OK;
  }

  *aModel = nsnull;
  *aResult = nsnull;

  nsCOMPtr<nsIDOMNode>    contextNode;
  nsCOMPtr<nsIDOMElement> bindElement;
  PRBool outerBind;
  PRInt32 contextPosition;
  PRInt32 contextSize;
  nsresult rv = GetNodeContext(aElement,
                               aElementFlags,
                               aModel,
                               getter_AddRefs(bindElement),
                               &outerBind,
                               getter_AddRefs(contextNode),
                               &contextPosition,
                               &contextSize);

  NS_ENSURE_SUCCESS(rv, rv);

  if (!contextNode) {
    return NS_OK;   // this will happen if the doc is still loading
  }

  nsAutoString expr;
  if (bindElement) {
    if (!outerBind) {
      // "When you refer to @id on a nested bind it returns an emtpy nodeset
      // because it has no meaning. The XForms WG will assign meaning in the
      // future."
      // @see http://www.w3.org/MarkUp/Group/2004/11/f2f/2004Nov11#resolution6

      return NS_OK;
    } else {
      // If there is a (outer) bind element, we retrive its nodeset.
      bindElement->GetAttribute(NS_LITERAL_STRING("nodeset"), expr);
    }
  } else {
    // If there's no bind element, we expect there to be a |aBindingAttr| attribute.
    aElement->GetAttribute(aBindingAttr, expr);

    if (expr.IsEmpty())
    {
      if (aDefaultRef.IsEmpty())
        return NS_OK;

      expr.Assign(aDefaultRef);
    }
  }

  // Evaluate |expr|
  nsCOMPtr<nsIDOMXPathResult> res = EvaluateXPath(expr,
                                                  contextNode,
                                                  aElement,
                                                  aResultType,
                                                  contextPosition,
                                                  contextSize,
                                                  aDeps,
                                                  aIndexesUsed);

  // If the evaluation failed because the node wasn't there and we should be
  // lazy authoring, then create it (if the situation qualifies, of course).
  // Novell only allows lazy authoring of single bound nodes.
  if (aResultType == nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE) {
    if (res) {
      nsCOMPtr<nsIDOMNode> node;
      rv = res->GetSingleNodeValue(getter_AddRefs(node));
      if (NS_SUCCEEDED(rv) && !node) {
        PRBool lazy = PR_FALSE;
        if (*aModel)
          (*aModel)->GetLazyAuthored(&lazy);
        if (lazy) {
          // according to sec 4.2.2 in the spec, "An instance data element node
          // will be created using the binding expression from the user
          // interface control as the name. If the name is not a valid QName, 
          // processing halts with an exception (xforms-binding-exception)"
          nsCOMPtr<nsIParserService> parserService =
                                    do_GetService(NS_PARSERSERVICE_CONTRACTID);
          if (NS_SUCCEEDED(rv) && parserService) {
            const PRUnichar* colon;
            rv = parserService->CheckQName(expr, PR_TRUE, &colon);
            if (NS_SUCCEEDED(rv)) {
              nsAutoString namespaceURI(EmptyString());

              // if we detect a namespace, we'll add it to the node, otherwise
              // we'll use the empty namespace.  If we should have gotten a
              // namespace and didn't, then we might as well give up.
              if (colon) {
                nsCOMPtr<nsIDOM3Node> dom3node = do_QueryInterface(aElement);
                rv = dom3node->LookupNamespaceURI(Substring(expr.get(), colon), 
                                                  namespaceURI);
                NS_ENSURE_SUCCESS(rv, rv);
              }

              if (NS_SUCCEEDED(rv)) {
                nsCOMArray<nsIInstanceElementPrivate> *instList = nsnull;
                (*aModel)->GetInstanceList(&instList);
                nsCOMPtr<nsIInstanceElementPrivate> instance = 
                  instList->ObjectAt(0);
                nsCOMPtr<nsIDOMDocument> domdoc;
                instance->GetDocument(getter_AddRefs(domdoc));
                nsCOMPtr<nsIDOMElement> instanceDataEle;
                nsCOMPtr<nsIDOMNode> childReturn;
                rv = domdoc->CreateElementNS(namespaceURI, expr,
                                             getter_AddRefs(instanceDataEle));
                NS_ENSURE_SUCCESS(rv, rv);
                nsCOMPtr<nsIDOMElement> instanceRoot;
                rv = domdoc->GetDocumentElement(getter_AddRefs(instanceRoot));
                rv = instanceRoot->AppendChild(instanceDataEle, 
                                               getter_AddRefs(childReturn));
                NS_ENSURE_SUCCESS(rv, rv);
              }

              // now that we inserted the lazy authored node, try to bind
              // again
              res = EvaluateXPath(expr, contextNode, aElement, aResultType,
                                  contextPosition, contextSize, aDeps,
                                  aIndexesUsed);
            } else {
              nsXFormsUtils::DispatchEvent(aElement, eEvent_BindingException);
            }
          }
        }
      }
    }
  }

  res.swap(*aResult); // exchanges ref

  return NS_OK;
}

/* static */ void
nsXFormsUtils::GetNodeValue(nsIDOMNode* aDataNode, nsAString& aNodeValue)
{
  PRUint16 nodeType;
  aDataNode->GetNodeType(&nodeType);

  switch(nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
  case nsIDOMNode::TEXT_NODE:
    // "Returns the string-value of the node."
    aDataNode->GetNodeValue(aNodeValue);
    return;

  case nsIDOMNode::ELEMENT_NODE:
    {
      // "If text child nodes are present, returns the string-value of the
      // first text child node.  Otherwise, returns "" (the empty string)".

      // Find the first child text node.
      nsCOMPtr<nsIDOMNodeList> childNodes;
      aDataNode->GetChildNodes(getter_AddRefs(childNodes));

      if (childNodes) {
        nsCOMPtr<nsIDOMNode> child;
        PRUint32 childCount;
        childNodes->GetLength(&childCount);

        for (PRUint32 i = 0; i < childCount; ++i) {
          childNodes->Item(i, getter_AddRefs(child));
          NS_ASSERTION(child, "DOMNodeList length is wrong!");

          child->GetNodeType(&nodeType);
          if (nodeType == nsIDOMNode::TEXT_NODE) {
            child->GetNodeValue(aNodeValue);
            return;
          }
        }
      }

      // No child text nodes.  Return an empty string.
    }
    break;
          
  default:
    // namespace, processing instruction, comment, XPath root node
    NS_WARNING("String value for this node type is not defined");
  }

  aNodeValue.Truncate(0);
}

/* static */ void
nsXFormsUtils::SetNodeValue(nsIDOMNode* aDataNode, const nsString& aNodeValue)
{
  PRUint16 nodeType;
  aDataNode->GetNodeType(&nodeType);

  switch(nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
    // "The string-value of the attribute is replaced with a string
    // corresponding to the new value."
    aDataNode->SetNodeValue(aNodeValue);
    break;

  case nsIDOMNode::TEXT_NODE:
    // "The text node is replaced with a new one corresponding to the new
    // value".
    {
      nsCOMPtr<nsIDOMDocument> document;
      aDataNode->GetOwnerDocument(getter_AddRefs(document));
      if (!document)
        break;

      nsCOMPtr<nsIDOMText> textNode;
      document->CreateTextNode(aNodeValue, getter_AddRefs(textNode));
      if (!textNode)
        break;

      nsCOMPtr<nsIDOMNode> parentNode;
      aDataNode->GetParentNode(getter_AddRefs(parentNode));
      if (parentNode) {
        nsCOMPtr<nsIDOMNode> childReturn;
        parentNode->ReplaceChild(textNode, aDataNode,
                                 getter_AddRefs(childReturn));
      }

      break;
    }

  case nsIDOMNode::ELEMENT_NODE:
    {
      // "If the element has any child text nodes, the first text node is
      // replaced with one corresponding to the new value."

      // Start by creating a text node for the new value.
      nsCOMPtr<nsIDOMDocument> document;
      aDataNode->GetOwnerDocument(getter_AddRefs(document));
      if (!document)
        break;

      nsCOMPtr<nsIDOMText> textNode;
      document->CreateTextNode(aNodeValue, getter_AddRefs(textNode));
      if (!textNode)
        break;

      // Now find the first child text node.
      nsCOMPtr<nsIDOMNodeList> childNodes;
      aDataNode->GetChildNodes(getter_AddRefs(childNodes));

      if (!childNodes)
        break;

      nsCOMPtr<nsIDOMNode> child, childReturn;
      PRUint32 childCount;
      childNodes->GetLength(&childCount);

      for (PRUint32 i = 0; i < childCount; ++i) {
        childNodes->Item(i, getter_AddRefs(child));
        NS_ASSERTION(child, "DOMNodeList length is wrong!");

        child->GetNodeType(&nodeType);
        if (nodeType == nsIDOMNode::TEXT_NODE) {
          // We found one, replace it with our new text node.
          aDataNode->ReplaceChild(textNode, child,
                                  getter_AddRefs(childReturn));
          return;
        }
      }

      // "If no child text nodes are present, a text node is created,
      // corresponding to the new value, and appended as the first child node."

      // XXX This is a bit vague since "appended as the first child node"
      // implies that there are no child nodes at all, but all we've
      // established is that there are no child _text_nodes.
      // Taking this to mean "inserted as the first child node" until this is
      // clarified.

      aDataNode->GetFirstChild(getter_AddRefs(child));
      if (child)
        aDataNode->InsertBefore(textNode, child, getter_AddRefs(childReturn));
      else
        aDataNode->AppendChild(textNode, getter_AddRefs(childReturn));

    }
    break;
          
  default:
    NS_WARNING("Trying to set node value for unsupported node type");
  }
}

///
/// @todo Use this consistently, or delete? (XXX)
/* static */ PRBool
nsXFormsUtils::GetSingleNodeBindingValue(nsIDOMElement* aElement,
                                         nsString& aValue)
{
  if (!aElement)
    return PR_FALSE;
  nsCOMPtr<nsIModelElementPrivate> model;
  nsCOMPtr<nsIDOMXPathResult> result;
  
  nsresult rv = EvaluateNodeBinding(aElement,
                                    nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                    NS_LITERAL_STRING("ref"),
                                    EmptyString(),
                                    nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                    getter_AddRefs(model),
                                    getter_AddRefs(result));

  if (NS_FAILED(rv) || !result)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNode> singleNode;
  result->GetSingleNodeValue(getter_AddRefs(singleNode));
  if (!singleNode)
    return PR_FALSE;

  nsXFormsUtils::GetNodeValue(singleNode, aValue);
  return PR_TRUE;
}

/* static */ nsresult
nsXFormsUtils::DispatchEvent(nsIDOMNode* aTarget, nsXFormsEvent aEvent,
                             PRBool *aDefaultActionEnabled)
{
  if (!aTarget)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXFormsControl> control = do_QueryInterface(aTarget);
  if (control) {
    switch (aEvent) {
      case eEvent_Previous:
      case eEvent_Next:
      case eEvent_Focus:
      case eEvent_Help:
      case eEvent_Hint:
      case eEvent_DOMActivate:
      case eEvent_ValueChanged:
      case eEvent_Valid:
      case eEvent_Invalid:
      case eEvent_DOMFocusIn:
      case eEvent_DOMFocusOut:
      case eEvent_Readonly:
      case eEvent_Readwrite:
      case eEvent_Required:
      case eEvent_Optional:
      case eEvent_Enabled:
      case eEvent_Disabled:
      case eEvent_InRange:
      case eEvent_OutOfRange:
        {
          PRBool acceptableEventTarget = PR_FALSE;
          control->IsEventTarget(&acceptableEventTarget);
          if (!acceptableEventTarget) {
            return NS_OK;
          }
          break;
        }
      default:
        break;
    }
  }

  nsCOMPtr<nsIDOMDocument> domDoc;
  aTarget->GetOwnerDocument(getter_AddRefs(domDoc));

  nsCOMPtr<nsIDOMDocumentEvent> doc = do_QueryInterface(domDoc);
  NS_ENSURE_STATE(doc);
  
  nsCOMPtr<nsIDOMEvent> event;
  doc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  const EventData *data = &sXFormsEventsEntries[aEvent];
  event->InitEvent(NS_ConvertUTF8toUTF16(data->name),
                   data->canBubble, data->canCancel);

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(aTarget);
  NS_ENSURE_STATE(target);

  SetEventTrusted(event, aTarget);

  PRBool defaultActionEnabled = PR_TRUE;
  nsresult rv = target->DispatchEvent(event, &defaultActionEnabled);

  if (NS_SUCCEEDED(rv) && aDefaultActionEnabled)
    *aDefaultActionEnabled = defaultActionEnabled;

  return rv;
}

/* static */ nsresult
nsXFormsUtils::SetEventTrusted(nsIDOMEvent* aEvent, nsIDOMNode* aRelatedNode)
{
  nsCOMPtr<nsIDOMNSEvent> event(do_QueryInterface(aEvent));
  if (event) {
    PRBool isTrusted = PR_FALSE;
    event->GetIsTrusted(&isTrusted);
    if (!isTrusted && aRelatedNode) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      aRelatedNode->GetOwnerDocument(getter_AddRefs(domDoc));
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
      if (doc) {
        nsIURI* uri = doc->GetDocumentURI();
        if (uri) {
          PRBool isChrome = PR_FALSE;
          uri->SchemeIs("chrome", &isChrome);
          if (isChrome) {
            nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aEvent));
            NS_ENSURE_STATE(privateEvent);
            privateEvent->SetTrusted(PR_TRUE);
          }
        }
      }
    }
  }
  return NS_OK;
}

/* static */ PRBool
nsXFormsUtils::EventHandlingAllowed(nsIDOMEvent* aEvent, nsIDOMNode* aTarget)
{
  PRBool allow = PR_FALSE;
  if (aEvent && aTarget) {
    nsCOMPtr<nsIDOMNSEvent> related(do_QueryInterface(aEvent));
    if (related) {
      PRBool isTrusted = PR_FALSE;
      if (NS_SUCCEEDED(related->GetIsTrusted(&isTrusted))) {
        if (isTrusted) {
          allow = PR_TRUE;
        } else {
          nsCOMPtr<nsIDOMDocument> domDoc;
          aTarget->GetOwnerDocument(getter_AddRefs(domDoc));
          nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
          if (doc) {
            nsIURI* uri = doc->GetDocumentURI();
            if (uri) {
              PRBool isChrome = PR_FALSE;
              uri->SchemeIs("chrome", &isChrome);
              allow = !isChrome;
            }
          }
        }
      }
    }
  }
  NS_WARN_IF_FALSE(allow, "Event handling not allowed!");
  return allow;
}

/* static */ PRBool
nsXFormsUtils::IsXFormsEvent(const nsAString& aEvent,
                             PRBool& aCancelable,
                             PRBool& aBubbles)
{
  PRUint32 flag = 0;
  if (!sXFormsEvents.Get(aEvent, &flag))
    return PR_FALSE;
  aCancelable = (flag & CANCELABLE) ? PR_TRUE : PR_FALSE;
  aBubbles = (flag & BUBBLES) ? PR_TRUE : PR_FALSE;
  return PR_TRUE;
}

/* static */ void
nsXFormsUtils::GetEventDefaults(const nsAString& aEvent,
                                PRBool& aCancelable,
                                PRBool& aBubbles)
{
  PRUint32 flag = 0;
  if (!sEventDefaults.Get(aEvent, &flag))
    return;
  aCancelable = (flag & CANCELABLE) ? PR_TRUE : PR_FALSE;
  aBubbles = (flag & BUBBLES) ? PR_TRUE : PR_FALSE;
}

/* static */ nsresult
nsXFormsUtils::CloneScriptingInterfaces(const nsIID *aIIDList,
                                        unsigned int aIIDCount,
                                        PRUint32 *aOutCount,
                                        nsIID ***aOutArray)
{
  nsIID **iids = NS_STATIC_CAST(nsIID**,
                                nsMemory::Alloc(aIIDCount * sizeof(nsIID*)));
  if (!iids) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
 
  for (PRUint32 i = 0; i < aIIDCount; ++i) {
    iids[i] = NS_STATIC_CAST(nsIID*,
                             nsMemory::Clone(&aIIDList[i], sizeof(nsIID)));
 
    if (!iids[i]) {
      for (PRUint32 j = 0; j < i; ++j)
        nsMemory::Free(iids[j]);
      nsMemory::Free(iids);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
 
  *aOutArray = iids;
  *aOutCount = aIIDCount;
  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::FindParentContext(nsIDOMElement           *aElement,
                                 nsIModelElementPrivate **aModel,
                                 nsIDOMNode             **aContextNode,
                                 PRInt32                 *aContextPosition,
                                 PRInt32                 *aContextSize)
{
  NS_ENSURE_ARG(aElement);
  NS_ENSURE_ARG_POINTER(aModel);
  NS_ENSURE_ARG_POINTER(aContextNode);

  nsCOMPtr<nsIDOMNode> curNode;
  nsresult rv = aElement->GetParentNode(getter_AddRefs(curNode));
  NS_ENSURE_SUCCESS(rv, NS_OK);

  // If a model is set, get its ID
  nsAutoString childModelID;
  if (*aModel) {
    nsCOMPtr<nsIDOMElement> modelElement = do_QueryInterface(*aModel);
    NS_ENSURE_TRUE(modelElement, NS_ERROR_FAILURE);
    modelElement->GetAttribute(NS_LITERAL_STRING("id"), childModelID);
  }

  // Find our context:
  // Iterate over all parents and find first one that implements nsIXFormsContextControl,
  // and has the same model as us.
  nsCOMPtr<nsIDOMNode> temp;  
  nsAutoString contextModelID;
  while (curNode) {
    nsCOMPtr<nsIXFormsContextControl> contextControl = do_QueryInterface(curNode);

    if (contextControl) {
      PRInt32 cSize;
      PRInt32 cPosition;
      nsCOMPtr<nsIDOMNode> tempNode;
      rv = contextControl->GetContext(contextModelID,
                                      getter_AddRefs(tempNode),
                                      &cPosition,
                                      &cSize);
      NS_ENSURE_SUCCESS(rv, rv);
      // If the call failed, it means that we _have_ a parent which sets the
      // context but it is invalid, ie. the XPath expression could have
      // generated an error.

      if (tempNode && (childModelID.IsEmpty()
                       || childModelID.Equals(contextModelID))) {
        NS_ADDREF(*aContextNode = tempNode);
        if (aContextSize) 
          *aContextSize = cSize;
        if (aContextPosition)
          *aContextPosition = cPosition;
        break;
      }
    }
    // Next ancestor
    temp.swap(curNode);
    rv = temp->GetParentNode(getter_AddRefs(curNode));
    NS_ENSURE_SUCCESS(rv, NS_OK);
  }

  // Child had no model set, set it
  if (!*aModel) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    nsresult rv = aElement->GetOwnerDocument(getter_AddRefs(domDoc));
    NS_ENSURE_SUCCESS(rv, rv);
    
    NS_ENSURE_TRUE(domDoc, NS_ERROR_FAILURE);

    nsCOMPtr<nsIModelElementPrivate> model;
    if (!*aContextNode || contextModelID.IsEmpty()) {
      // We have either not found a context node, or we have found one where
      // the model ID is empty. That means we use the first model in document
      nsCOMPtr<nsIDOMNodeList> nodes;
      domDoc->GetElementsByTagNameNS(NS_LITERAL_STRING(NS_NAMESPACE_XFORMS),
                                     NS_LITERAL_STRING("model"),
                                     getter_AddRefs(nodes));
      // No model element in document!
      NS_ENSURE_STATE(nodes);

      nsCOMPtr<nsIDOMNode> modelNode;
      nodes->Item(0, getter_AddRefs(modelNode));
      model = do_QueryInterface(modelNode);
    } else {
      // Get the model with the correct ID
      nsCOMPtr<nsIDOMElement> modelElement;
      domDoc->GetElementById(contextModelID, getter_AddRefs(modelElement));
      model = do_QueryInterface(modelElement);
    }    
    if (!model) {
      nsXFormsUtils::ReportError(NS_LITERAL_STRING("noModelError"), aElement);
      DispatchEvent(aElement, eEvent_BindingException);
      return NS_ERROR_ABORT;
    }
    NS_ADDREF(*aModel = model);
  }
  
  return NS_OK;
}

/* static */ PRBool
nsXFormsUtils::CheckSameOrigin(nsIURI *aBaseURI, nsIURI *aTestURI)
{
  nsresult rv;

  // check to see if we're allowed to load this URI
  nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  if (secMan) {
    rv = secMan->CheckSameOriginURI(aBaseURI, aTestURI);
    if (NS_SUCCEEDED(rv))
      return PR_TRUE;
  }

  // else, check with the permission manager to see if this host is
  // permitted to access sites from other domains.

  nsCOMPtr<nsIPermissionManager> permMgr =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  PRUint32 perm;
  rv = permMgr->TestPermission(aBaseURI, "xforms-load", &perm);
  if (NS_SUCCEEDED(rv) && perm == nsIPermissionManager::ALLOW_ACTION)
    return PR_TRUE; 

  return PR_FALSE;
}

/*static*/ PRBool
nsXFormsUtils::IsXFormsElement(nsIDOMNode* aNode, const nsAString& aName)
{
  if (aNode) {
    PRUint16 nodeType;
    aNode->GetNodeType(&nodeType);
    if (nodeType == nsIDOMNode::ELEMENT_NODE) {
      nsAutoString name;
      aNode->GetLocalName(name);
      if (name.Equals(aName)) {
        nsAutoString ns;
        aNode->GetNamespaceURI(ns);
        if (ns.EqualsLiteral(NS_NAMESPACE_XFORMS))
          return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}

/* static */ PRBool
nsXFormsUtils::IsLabelElement(nsIDOMNode *aElement)
{
  nsAutoString value;
  aElement->GetLocalName(value);
  if (value.EqualsLiteral("label")) {
    aElement->GetNamespaceURI(value);
    return value.EqualsLiteral(NS_NAMESPACE_XFORMS);
  }

  return PR_FALSE;
}

/* static */ PRBool
nsXFormsUtils::FocusControl(nsIDOMElement *aElement)
{
  PRBool ret = PR_FALSE;
  nsCOMPtr<nsIDOMNSHTMLElement> element(do_QueryInterface(aElement));
  if (element && NS_SUCCEEDED(element->Focus()))
    ret = PR_TRUE;
  return ret;
}

int
sortFunc(nsIDOMNode *aNode1, nsIDOMNode *aNode2, void *aArg) 
{
  return (void*) aNode1 > (void*) aNode2;
}

/* static */ void
nsXFormsUtils::MakeUniqueAndSort(nsCOMArray<nsIDOMNode> *aArray)
{
  if (!aArray)
    return;

  aArray->Sort(sortFunc, nsnull);  

  PRInt32 pos = 0;
  while (pos + 1 < aArray->Count()) {
    if (aArray->ObjectAt(pos) == aArray->ObjectAt(pos + 1)) {
      aArray->RemoveObjectAt(pos + 1);
    } else {
      ++pos;
    }
  }
}

/* static */ nsresult
nsXFormsUtils::GetInstanceNodeForData(nsIDOMNode             *aInstanceDataNode,
                                      nsIDOMNode             **aInstanceNode)
{
  NS_ENSURE_ARG(aInstanceDataNode);
  NS_ENSURE_ARG_POINTER(aInstanceNode);
  *aInstanceNode = nsnull;

  nsCOMPtr<nsIDOMDocument> instanceDOMDoc;
  aInstanceDataNode->GetOwnerDocument(getter_AddRefs(instanceDOMDoc));
  // owner doc is null when the data node is the document (e.g., ref="/")
  if (!instanceDOMDoc) {
    instanceDOMDoc = do_QueryInterface(aInstanceDataNode);
  }

  nsCOMPtr<nsIDocument> instanceDoc(do_QueryInterface(instanceDOMDoc));
  NS_ENSURE_TRUE(instanceDoc, NS_ERROR_UNEXPECTED);

  nsISupports* owner =
    NS_STATIC_CAST(
      nsISupports*,
      instanceDoc->GetProperty(nsXFormsAtoms::instanceDocumentOwner));

  nsCOMPtr<nsIDOMNode> instanceNode(do_QueryInterface(owner));
  if (instanceNode) {
    NS_ADDREF(*aInstanceNode = instanceNode);
    return NS_OK;
  }

  // Two possibilities.  No instance nodes in model (which should never happen)
  // or instance node not found.
  return NS_ERROR_ABORT;
}

/* static */ nsresult
nsXFormsUtils::ParseTypeFromNode(nsIDOMNode *aInstanceData,
                                 nsAString &aType, nsAString &aNSPrefix)
{
  nsresult rv = NS_OK;

  // aInstanceData could be an instance data node or it could be an attribute
  // on an instance data node (basically the node that a control is bound to).

  nsAutoString *typeVal = nsnull;

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

  if (!typeVal) {
    // Get MIP type bound to node
    nsCOMPtr<nsIContent> nodeContent(do_QueryInterface(aInstanceData));
    if (nodeContent) {
      typeVal =
        NS_STATIC_CAST(nsAutoString*,
                       nodeContent->GetProperty(nsXFormsAtoms::type, &rv));
    } else {
      nsCOMPtr<nsIAttribute> nodeAttribute(do_QueryInterface(aInstanceData));
      if (!nodeAttribute)
        // node is neither content or attribute!
        return NS_ERROR_FAILURE;

      typeVal =
        NS_STATIC_CAST(nsAutoString*,
                       nodeAttribute->GetProperty(nsXFormsAtoms::type, &rv));
    }
  }

  if (NS_FAILED(rv) || !typeVal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // split type (ns:type) into namespace and type.
  PRInt32 separator = typeVal->FindChar(':');
  if ((PRUint32) separator == (typeVal->Length() - 1)) {
    const PRUnichar *strings[] = { typeVal->get() };
    // XXX: get an element from the document this came from
    ReportError(NS_LITERAL_STRING("missingTypeName"), strings, 1, nsnull, nsnull);
    return NS_ERROR_UNEXPECTED;
  } else if (separator == kNotFound) {
    // no namespace prefix, which is valid;
    aNSPrefix.AssignLiteral("");
    aType.Assign(*typeVal);
  } else {
    aNSPrefix.Assign(Substring(*typeVal, 0, separator));
    aType.Assign(Substring(*typeVal, ++separator, typeVal->Length()));
  }

  return NS_OK;
}

/* static */ void
nsXFormsUtils::ReportError(const nsString& aMessageName, const PRUnichar **aParams,
                           PRUint32 aParamLength, nsIDOMNode *aElement,
                           nsIDOMNode *aContext, PRUint32 aErrorFlag)
{

  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance("@mozilla.org/scripterror;1");

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID);

  if (!(consoleService && errorObject && bundleService))
    return;
  
  nsAutoString msg, srcFile, srcLine;

  // get the string from the bundle (xforms.properties)
  nsCOMPtr<nsIStringBundle> bundle;
  bundleService->CreateBundle("chrome://xforms/locale/xforms.properties",
                              getter_AddRefs(bundle));
  nsXPIDLString message;
  if (aParams) {
    bundle->FormatStringFromName(aMessageName.get(), aParams, aParamLength,
                                 getter_Copies(message));
    msg.Append(message);
  } else {
    bundle->GetStringFromName(aMessageName.get(),
                              getter_Copies(message));
    msg.Append(message);
  }

  if (msg.IsEmpty()) {
#ifdef DEBUG
    printf("nsXFormsUtils::ReportError() Failed to get message string for message id '%s'!\n",
           NS_ConvertUCS2toUTF8(aMessageName).get());
#endif
    return;
  }

  // if a context was defined, serialize it and append to the message.
  if (aContext) {
    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aContext));
    if (element) {
      srcLine.AppendLiteral("<");
    }

    // For other than element nodes nodeName should be enough.
    nsAutoString tmp;
    aContext->GetNodeName(tmp);
    srcLine.Append(tmp);
    
    if (element) {
      nsCOMPtr<nsIDOMNamedNodeMap> attrs;
      element->GetAttributes(getter_AddRefs(attrs));
      if (attrs) {
        PRUint32 len = 0;
        attrs->GetLength(&len);
        for (PRUint32 i = 0; i < len; ++i) {
          nsCOMPtr<nsIDOMNode> attr;
          attrs->Item(i, getter_AddRefs(attr));
          if (attr) {
            srcLine.AppendLiteral(" ");
            attr->GetNodeName(tmp);
            srcLine.Append(tmp);
            srcLine.AppendLiteral("=\"");
            attr->GetNodeValue(tmp);
            srcLine.Append(tmp);
            srcLine.AppendLiteral("\"");
          }
        }
      }
      srcLine.AppendLiteral("/>");
    }
  }

  // if aElement is defined, we use it to figure out the location of the file
  // that caused the error.
  if (aElement) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aElement->GetOwnerDocument(getter_AddRefs(domDoc));

    if (domDoc) {
      nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(domDoc));

      if (nsDoc) {
        nsCOMPtr<nsIDOMLocation> domLoc;
        nsDoc->GetLocation(getter_AddRefs(domLoc));

        if (domLoc) {
          nsAutoString location;
          domLoc->GetHref(srcFile);
        }
      }
    }
  }


  // Log the message to JavaScript Console
#ifdef DEBUG
  printf("ERR: %s\n", NS_ConvertUCS2toUTF8(msg).get());
#endif
  nsresult rv = errorObject->Init(msg.get(), srcFile.get(), srcLine.get(),
                                  0, 0, aErrorFlag, "XForms");
  if (NS_SUCCEEDED(rv)) {
    consoleService->LogMessage(errorObject);
  }
}

/* static */ PRBool
nsXFormsUtils::IsDocumentReadyForBind(nsIDOMDocument *aDocument)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);

  nsIDocument *test = NS_STATIC_CAST(nsIDocument *,
                        doc->GetProperty(nsXFormsAtoms::readyForBindProperty));
  return test ? PR_TRUE : PR_FALSE;
}

/**
 * Find the repeat context for an element: Returns a parent repeat or
 * contextcontainer (depending on |aFindContainer|).
 *
 * @param aElement            The element to retrieve context for
 * @param aFindContainer      find true: "contextcontainer", false: "repeat"
 * @return                    The repeat or contextcontainer node
 */
already_AddRefed<nsIDOMNode>
FindRepeatContext(nsIDOMElement *aElement, PRBool aFindContainer)
{
  nsIDOMNode *result = nsnull;
  
  // XXX Possibly use mRepeatState functionality from nsXFormsDelegateStub to
  // save running up the tree?
  nsCOMPtr<nsIDOMNode> context, temp;
  aElement->GetParentNode(getter_AddRefs(context));
  nsresult rv = NS_OK;
  while (context) {
    if (nsXFormsUtils::IsXFormsElement(context,
                                       aFindContainer ?
                                         NS_LITERAL_STRING("contextcontainer") :
                                         NS_LITERAL_STRING("repeat"))) {
      break;
    }
    temp.swap(context);
    rv = temp->GetParentNode(getter_AddRefs(context));
  }
  if (context && NS_SUCCEEDED(rv)) {
      NS_ADDREF(result = context);
  }
  return result;
}

/* static */
nsresult
nsXFormsUtils::GetElementById(nsIDOMDocument   *aDoc,
                              const nsAString  &aId,
                              const PRBool      aOnlyXForms,
                              nsIDOMElement    *aCaller,
                              nsIDOMElement   **aElement)
{
  NS_ENSURE_TRUE(!aId.IsEmpty(), NS_ERROR_INVALID_ARG);
  NS_ENSURE_ARG_POINTER(aDoc);
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = nsnull;

  nsCOMPtr<nsIDOMElement> element;
  aDoc->GetElementById(aId, getter_AddRefs(element));
  if (!element)
    return NS_OK;

  // Check whether the element is inside a repeat. If it is, find the cloned
  // element for the currently selected row.
  nsCOMPtr<nsIDOMNode> repeat = FindRepeatContext(element, PR_FALSE);
  if (!repeat) {
    // Element not inside a repeat, just return the element (eventually
    // checking for XForms namespace)
    if (aOnlyXForms) {
      nsAutoString ns;
      element->GetNamespaceURI(ns);
      if (!ns.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
        return NS_OK;
      }
    }
    NS_ADDREF(*aElement = element);
    return NS_OK;
  }

  // Check whether the caller is inside a repeat
  nsCOMPtr<nsIDOMNode> repeatRow;
  if (aCaller) {
    repeatRow = FindRepeatContext(aCaller, PR_TRUE);
  }

  if (!repeatRow) {
    // aCaller was either not set or not inside a repeat, used currently
    // focused row of the element's repeat
    nsCOMPtr<nsIXFormsRepeatElement> repeatInt(do_QueryInterface(repeat));
    NS_ENSURE_STATE(repeatInt);
    repeatInt->GetCurrentRepeatRow(getter_AddRefs(repeatRow));
    NS_ASSERTION(repeatRow, "huh? No currently selected repeat row?");
  }
  
  nsCOMPtr<nsIDOMElement> rowElement(do_QueryInterface(repeatRow));
  NS_ENSURE_STATE(rowElement);

  nsCOMPtr<nsIDOMDocument> repeatDoc;
  rowElement->GetOwnerDocument(getter_AddRefs(repeatDoc));
    
  nsCOMPtr<nsIDOMNodeList> descendants;
  nsresult rv;
  rv = rowElement->GetElementsByTagNameNS(aOnlyXForms ?
                                            NS_LITERAL_STRING(NS_NAMESPACE_XFORMS) :
                                            NS_LITERAL_STRING("*"),
                                          NS_LITERAL_STRING("*"),
                                          getter_AddRefs(descendants));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(descendants);
  PRUint32 elements;
  descendants->GetLength(&elements);
  if (!elements) {
    return NS_OK;
  }

  PRUint32 i;
  PRUint16 type;
  nsAutoString idVal;
  nsCOMPtr<nsIDOMNode> childNode;
  for (i = 0; i < elements; ++i) {
    descendants->Item(i, getter_AddRefs(childNode));
    childNode->GetNodeType(&type);
    if (type == nsIDOMNode::ELEMENT_NODE) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(childNode));
      NS_ASSERTION(content, "An ELEMENT_NODE not implementing nsIContent?!");
      rv = content->GetAttr(kNameSpaceID_None, content->GetIDAttributeName(),
                            idVal);
      if (rv == NS_CONTENT_ATTR_HAS_VALUE && idVal.Equals(aId)) {
        element = do_QueryInterface(childNode);
        break;
      }
    }
  }

  if (i == elements) {
    return NS_OK;
  }
    
  NS_ENSURE_STATE(element);
  NS_ADDREF(*aElement = element);

  return NS_OK;
}

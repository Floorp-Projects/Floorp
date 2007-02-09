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
#include "nsIDOMDocumentXBL.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXPathEvaluator.h"
#include "nsIDOMXPathExpression.h"
#include "nsIDOMXPathResult.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIXPathEvaluatorInternal.h"
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
#include "nsIContentPolicy.h"
#include "nsContentUtils.h"
#include "nsContentPolicyUtils.h"
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

#include "nsIScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsServiceManagerUtils.h"
#include "nsIDOMAttr.h"
#include "nsIDOM3Node.h"
#include "nsIConsoleService.h"
#include "nsIStringBundle.h"
#include "nsIXFormsRepeatElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIURI.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIParserService.h"
#include "nsISchemaValidator.h"
#include "nsISchemaDuration.h"
#include "nsXFormsSchemaValidator.h"
#include "prdtoa.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsPIDOMWindow.h"

#include "nsIDOMDocumentType.h"
#include "nsIDOMEntity.h"
#include "nsIDOMNotation.h"
#include "nsIEventStateManager.h"
#include "nsXFormsModelElement.h"

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

const PRInt32 kDefaultIntrinsicState =
  NS_EVENT_STATE_ENABLED |
  NS_EVENT_STATE_VALID |
  NS_EVENT_STATE_OPTIONAL |
  NS_EVENT_STATE_MOZ_READWRITE;

const PRInt32 kDisabledIntrinsicState =
  NS_EVENT_STATE_DISABLED |
  NS_EVENT_STATE_VALID |
  NS_EVENT_STATE_OPTIONAL |
  NS_EVENT_STATE_MOZ_READWRITE;

struct EventItem
{
  nsXFormsEvent           event;
  nsCOMPtr<nsIDOMNode>    eventTarget;
  nsCOMPtr<nsIDOMElement> srcElement;
};

static PRBool gExperimentalFeaturesEnabled = PR_FALSE;

PR_STATIC_CALLBACK(int) PrefChangedCallback(const char* aPref, void* aData)
{
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && pref) {

    // if our experimental pref changed, make sure to update our static
    // variable.
    if (strcmp(aPref, PREF_EXPERIMENTAL_FEATURES) == 0) {
      PRBool val;
      if (NS_SUCCEEDED(pref->GetBoolPref(PREF_EXPERIMENTAL_FEATURES, &val))) {
        gExperimentalFeaturesEnabled = val;
      }
    }
  }

  return 0; // PREF_OK
}

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

  nsresult rv;
  nsCOMPtr<nsIPrefBranch> prefBranch =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && prefBranch) {
    PRBool val;
    rv = prefBranch->GetBoolPref(PREF_EXPERIMENTAL_FEATURES, &val);
    if (NS_SUCCEEDED(rv)) {
      gExperimentalFeaturesEnabled = val;
    }
  }

  nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_STATE(pref);
  rv = pref->RegisterCallback(PREF_EXPERIMENTAL_FEATURES,
                              PrefChangedCallback, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::Shutdown()
{
  // unregister our pref listeners

  nsresult rv;
  nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID, &rv);
  NS_ENSURE_STATE(pref);
  rv = pref->UnregisterCallback(PREF_EXPERIMENTAL_FEATURES,
                                PrefChangedCallback, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);
  gExperimentalFeaturesEnabled = PR_FALSE;

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
                              nsIXFormsControl       **aParentControl,
                              nsIDOMNode             **aContextNode,
                              PRInt32                 *aContextPosition,
                              PRInt32                 *aContextSize)
{
  NS_ENSURE_ARG(aElement);
  NS_ENSURE_ARG(aOuterBind);
  NS_ENSURE_ARG_POINTER(aContextNode);
  NS_ENSURE_ARG_POINTER(aBindElement);
  *aBindElement = nsnull;
  if (aParentControl)
    *aParentControl = nsnull;

  // Set default context size and position
  if (aContextSize)
    *aContextSize = 1;
  if (aContextPosition)
    *aContextPosition = 1;

  // Find correct model element
  nsAutoString bindId;
  NS_NAMED_LITERAL_STRING(bindStr, "bind");
  aElement->GetAttribute(bindStr, bindId);
  if (!bindId.IsEmpty()) {
    // CASE 1: Use @bind
    GetElementByContextId(aElement, bindId, aBindElement);

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
        GetElementByContextId(aElement, modelId, getter_AddRefs(modelElement));
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
                                    aParentControl,
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
nsXFormsUtils::GetModel(nsIDOMElement     *aElement,
                        nsIXFormsControl **aParentControl,
                        PRUint32           aElementFlags,
                        nsIDOMNode       **aContextNode)

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
                 aParentControl,
                 getter_AddRefs(contextNode));

  NS_ENSURE_TRUE(model, nsnull);

  if (aContextNode) {
    NS_IF_ADDREF(*aContextNode = contextNode);
  }

  nsIModelElementPrivate *result = nsnull;
  if (model)
    NS_ADDREF(result = model);
  return result;
}

/* static */ nsresult
nsXFormsUtils::CreateExpression(nsIXPathEvaluatorInternal  *aEvaluator,
                                const nsAString            &aExpression,
                                nsIDOMXPathNSResolver      *aResolver,
                                nsIXFormsXPathState        *aState,
                                nsIDOMXPathExpression     **aResult)
{
  nsStringArray ns;
  nsCStringArray contractid;
  nsCOMArray<nsISupports> state;

  ns.AppendString(EmptyString());
  contractid.AppendCString(NS_LITERAL_CSTRING("@mozilla.org/xforms-xpath-functions;1"));
  state.AppendObject(aState);

  // if somehow the contextNode and the evaluator weren't spawned from the same
  // document, this could fail with NS_ERROR_DOM_WRONG_DOCUMENT_ERR
  nsCOMPtr<nsIDOMXPathExpression> expression;
  return aEvaluator->CreateExpression(aExpression, aResolver, &ns, &contractid,
                                      &state, aResult);
}

/* static */ nsresult
nsXFormsUtils::EvaluateXPath(const nsAString        &aExpression,
                             nsIDOMNode             *aContextNode,
                             nsIDOMNode             *aResolverNode,
                             PRUint16                aResultType,
                             nsIDOMXPathResult     **aResult,
                             PRInt32                 aContextPosition,
                             PRInt32                 aContextSize,
                             nsCOMArray<nsIDOMNode> *aSet,
                             nsStringArray          *aIndexesUsed)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  nsCOMPtr<nsIDOMDocument> doc;
  aContextNode->GetOwnerDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIDOMXPathEvaluator> eval = do_QueryInterface(doc);
  nsCOMPtr<nsIXPathEvaluatorInternal> evalInternal = do_QueryInterface(eval);
  NS_ENSURE_STATE(eval && evalInternal);

  nsCOMPtr<nsIDOMXPathNSResolver> resolver;
  nsresult rv = eval->CreateNSResolver(aResolverNode, getter_AddRefs(resolver));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXFormsXPathState> state = new nsXFormsXPathState(aResolverNode,
                                                               aContextNode);
  NS_ENSURE_TRUE(state, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIDOMXPathExpression> expression;
  rv = CreateExpression(evalInternal, aExpression, resolver, state,
                        getter_AddRefs(expression));

  PRBool throwException = PR_FALSE;
  if (!expression) {
    const nsPromiseFlatString& flat = PromiseFlatString(aExpression);
    const PRUnichar *strings[] = { flat.get() };
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("exprParseError"),
                               strings, 1, aContextNode, nsnull);
    throwException = PR_TRUE;
  } else {
    nsCOMPtr<nsIDOMNSXPathExpression> nsExpr =
      do_QueryInterface(expression, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> supResult;
    rv = nsExpr->EvaluateWithContext(aContextNode,
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
        nsXFormsXPathAnalyzer analyzer(evalInternal, resolver, state);
        nsAutoPtr<nsXFormsXPathNode> xNode(parser.Parse(aExpression));
        rv = analyzer.Analyze(aContextNode,
                              xNode,
                              nsExpr,
                              &aExpression,
                              aSet,
                              aContextPosition,
                              aContextSize,
                              aResultType == nsIDOMXPathResult::STRING_TYPE);
        NS_ENSURE_SUCCESS(rv, rv);

        if (aIndexesUsed)
          *aIndexesUsed = analyzer.IndexesUsed();
      }

      CallQueryInterface(supResult, aResult);  // addrefs
      return NS_OK;
    }

    if (rv == NS_ERROR_XFORMS_CALCULATION_EXCEPTION) {
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
    nsCOMPtr<nsIModelElementPrivate> modelPriv =
      nsXFormsUtils::GetModel(resolverElement);
    nsCOMPtr<nsIDOMNode> model = do_QueryInterface(modelPriv);
    DispatchEvent(model, eEvent_ComputeException, nsnull, resolverElement);
  }

  return rv;
}

/* static */ nsresult
nsXFormsUtils::EvaluateXPath(nsIXPathEvaluatorInternal  *aEvaluator,
                             const nsAString            &aExpression,
                             nsIDOMNode                 *aContextNode,
                             nsIDOMXPathNSResolver      *aResolver,
                             nsIXFormsXPathState        *aState,
                             PRUint16                    aResultType,
                             PRInt32                     aContextPosition,
                             PRInt32                     aContextSize,
                             nsIDOMXPathResult          *aInResult,
                             nsIDOMXPathResult         **aResult)
{
  nsCOMPtr<nsIDOMXPathExpression> expression;
  nsresult rv = CreateExpression(aEvaluator, aExpression, aResolver, aState,
                                 getter_AddRefs(expression));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNSXPathExpression> nsExpression =
    do_QueryInterface(expression, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // if somehow the contextNode and the evaluator weren't spawned from the same
  // document, this could fail with NS_ERROR_DOM_WRONG_DOCUMENT_ERR.  We are
  // NOT setting the state's original context node here.  We'll assume that
  // the state was set up correctly prior to this function being called.
  nsCOMPtr<nsISupports> supResult;
  rv = nsExpression->EvaluateWithContext(aContextNode, aContextPosition,
                                         aContextSize, aResultType,
                                         aInResult, getter_AddRefs(supResult));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(supResult, aResult);
}

/* static */ nsresult
nsXFormsUtils::EvaluateNodeBinding(nsIDOMElement           *aElement,
                                   PRUint32                 aElementFlags,
                                   const nsString          &aBindingAttr,
                                   const nsString          &aDefaultRef,
                                   PRUint16                 aResultType,
                                   nsIModelElementPrivate **aModel,
                                   nsIDOMXPathResult      **aResult,
                                   PRBool                  *aUsesModelBind,
                                   nsIXFormsControl       **aParentControl,
                                   nsCOMArray<nsIDOMNode>  *aDeps,
                                   nsStringArray           *aIndexesUsed)
{
  if (!aElement || !aModel || !aResult || !aUsesModelBind) {
    return NS_ERROR_FAILURE;
  }

  *aModel = nsnull;
  *aResult = nsnull;
  *aUsesModelBind = PR_FALSE;

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
                               aParentControl,
                               getter_AddRefs(contextNode),
                               &contextPosition,
                               &contextSize);

  NS_ENSURE_SUCCESS(rv, rv);

  if (!contextNode) {
    return NS_OK;   // this will happen if the doc is still loading
  }

  ////////////////////
  // STEP 1: Handle @bind'ings
  if (bindElement) {
    if (outerBind) {
      // If there is an outer bind element, we retrieve its nodeset.
      nsCOMPtr<nsIContent> content(do_QueryInterface(bindElement));
      NS_ASSERTION(content, "nsIDOMElement not implementing nsIContent?!");

      NS_IF_ADDREF(*aResult =
                   NS_STATIC_CAST(nsIDOMXPathResult*,
                                  content->GetProperty(nsXFormsAtoms::bind)));
      *aUsesModelBind = PR_TRUE;
      return NS_OK;
    }

    // References to inner binds are not defined.
    // "When you refer to @id on a nested bind it returns an emtpy nodeset
    // because it has no meaning. The XForms WG will assign meaning in the
    // future."
    // @see http://www.w3.org/MarkUp/Group/2004/11/f2f/2004Nov11#resolution6
    nsXFormsUtils::ReportError(NS_LITERAL_STRING("innerBindRefError"),
                               aElement);
    return NS_ERROR_FAILURE;
  }


  ////////////////////
  // STEP 2: No bind attribute
  // If there's no bind attribute, we expect there to be a |aBindingAttr|
  // attribute.
  nsAutoString expr;
  aElement->GetAttribute(aBindingAttr, expr);
  if (expr.IsEmpty()) {
    // if there's no default binding, bail out
    if (aDefaultRef.IsEmpty())
      return NS_OK;

    expr.Assign(aDefaultRef);
  }

  // Evaluate |expr|
  nsCOMPtr<nsIDOMXPathResult> res;
  rv  = EvaluateXPath(expr, contextNode, aElement, aResultType,
                      getter_AddRefs(res), contextPosition, contextSize,
                      aDeps, aIndexesUsed);
  NS_ENSURE_SUCCESS(rv, rv);

  ////////////////////
  // STEP 3: Check for lazy binding

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
                nsCOMPtr<nsIInstanceElementPrivate> instance;
                rv = (*aModel)->FindInstanceElement(EmptyString(),
                                                    getter_AddRefs(instance));
                NS_ENSURE_SUCCESS(rv, rv);
                nsCOMPtr<nsIDOMDocument> domdoc;
                instance->GetInstanceDocument(getter_AddRefs(domdoc));
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
              rv = EvaluateXPath(expr, contextNode, aElement, aResultType,
                                 getter_AddRefs(res), contextPosition,
                                 contextSize, aDeps, aIndexesUsed);
              NS_ENSURE_SUCCESS(rv, rv);
            } else {
                const PRUnichar *strings[] = { expr.get() };
                nsXFormsUtils::ReportError(NS_LITERAL_STRING("invalidQName"),
                                           strings, 1, aElement, aElement);
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
  aNodeValue = EmptyString();

  switch(nodeType) {
  case nsIDOMNode::ATTRIBUTE_NODE:
  case nsIDOMNode::TEXT_NODE:
  case nsIDOMNode::CDATA_SECTION_NODE:
    // "Returns the string-value of the node."
    aDataNode->GetNodeValue(aNodeValue);
    break;

  case nsIDOMNode::ELEMENT_NODE:
    {
      // XForms specs 8.1.1 (http://www.w3.org/TR/xforms/slice8.html#ui-processing)
      // says: 'if text child nodes are present, returns the string-value of the
      // first text child node. Otherwise, returns "" (the empty string)'.
      // The 'text child node' that is mentioned above is from the xforms
      // instance document which is, according to spec, is formed by
      // 'creating an XPath data model'. So we need to treat 'text node' in
      // this case as an XPath text node and not a DOM text node.
      // DOM XPath specs (http://www.w3.org/TR/2002/WD-DOM-Level-3-XPath-20020328/xpath.html#TextNodes)
      // says:
      // 'Applications using XPath in an environment with fragmented text nodes
      // must manually gather the text of a single logical text node possibly
      // from multiple nodes beginning with the first Text node or CDATASection
      // node returned by the implementation'.

      // Therefore we concatenate contiguous CDATA/DOM text nodes and return
      // as node value.

      // Find the first child text node.
      nsCOMPtr<nsIDOMNodeList> childNodes;
      aDataNode->GetChildNodes(getter_AddRefs(childNodes));

      if (childNodes) {
        nsCOMPtr<nsIDOMNode> child;
        PRUint32 childCount;
        childNodes->GetLength(&childCount);

        nsAutoString value;
        for (PRUint32 i = 0; i < childCount; ++i) {
          childNodes->Item(i, getter_AddRefs(child));
          NS_ASSERTION(child, "DOMNodeList length is wrong!");

          child->GetNodeType(&nodeType);
          if (nodeType == nsIDOMNode::TEXT_NODE ||
              nodeType == nsIDOMNode::CDATA_SECTION_NODE) {
            child->GetNodeValue(value);
            aNodeValue.Append(value);
          } else {
            break;
          }
        }
      }

      // No child text nodes.  Return an empty string.
    }
    break;
          
  case nsIDOMNode::ENTITY_REFERENCE_NODE:
  case nsIDOMNode::ENTITY_NODE:
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
  case nsIDOMNode::COMMENT_NODE:
  case nsIDOMNode::DOCUMENT_NODE:
  case nsIDOMNode::DOCUMENT_TYPE_NODE:
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
  case nsIDOMNode::NOTATION_NODE:
    // String value for these node types is not defined, ie. return the empty
    // string.
    break;

  default:
    NS_ASSERTION(PR_FALSE, "Huh? New node type added to Gecko?!");
  }
}

/* static */ PRBool
nsXFormsUtils::GetSingleNodeBinding(nsIDOMElement* aElement,
                                    nsIDOMNode** aNode,
                                    nsIModelElementPrivate** aModel)
{
  if (!aElement)
    return PR_FALSE;
  nsCOMPtr<nsIModelElementPrivate> model;
  nsCOMPtr<nsIDOMXPathResult> result;
  PRBool usesModelBind;
  
  nsresult rv = EvaluateNodeBinding(aElement,
                                    nsXFormsUtils::ELEMENT_WITH_MODEL_ATTR,
                                    NS_LITERAL_STRING("ref"),
                                    EmptyString(),
                                    nsIDOMXPathResult::FIRST_ORDERED_NODE_TYPE,
                                    getter_AddRefs(model),
                                    getter_AddRefs(result),
                                    &usesModelBind);

  if (NS_FAILED(rv) || !result)
    return PR_FALSE;

  nsCOMPtr<nsIDOMNode> singleNode;
  if (usesModelBind) {
    result->SnapshotItem(0, getter_AddRefs(singleNode));
  } else {
    result->GetSingleNodeValue(getter_AddRefs(singleNode));
  }
  if (!singleNode)
    return PR_FALSE;

  singleNode.swap(*aNode);
  if (aModel)
    model.swap(*aModel);  // transfers ref
  return PR_TRUE;
}

///
/// @todo Use this consistently, or delete? (XXX)
/* static */ PRBool
nsXFormsUtils::GetSingleNodeBindingValue(nsIDOMElement* aElement,
                                         nsString& aValue)
{
  nsCOMPtr<nsIDOMNode> node;
  if (GetSingleNodeBinding(aElement, getter_AddRefs(node), nsnull)) {
    nsXFormsUtils::GetNodeValue(node, aValue);
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
DispatchXFormsEvent(nsIDOMNode* aTarget, nsXFormsEvent aEvent,
                    PRBool *aDefaultActionEnabled)
{
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

  nsXFormsUtils::SetEventTrusted(event, aTarget);

  PRBool defaultActionEnabled = PR_TRUE;
  nsresult rv = target->DispatchEvent(event, &defaultActionEnabled);

  if (NS_SUCCEEDED(rv) && aDefaultActionEnabled)
    *aDefaultActionEnabled = defaultActionEnabled;

  // if this is a fatal error, then display the fatal error dialog if desired
  switch (aEvent) {
  case eEvent_LinkException:
    {
      nsCOMPtr<nsIDOMElement> targetEle(do_QueryInterface(aTarget));
      nsXFormsUtils::HandleFatalError(targetEle,
                                      NS_LITERAL_STRING("XFormsLinkException"));
      break;
    }
  case eEvent_ComputeException:
    {
      nsCOMPtr<nsIDOMElement> targetEle(do_QueryInterface(aTarget));
      nsXFormsUtils::HandleFatalError(targetEle,
                                      NS_LITERAL_STRING("XFormsComputeException"));
      break;
    }
  case eEvent_BindingException:
    {
      nsCOMPtr<nsIDOMElement> targetEle(do_QueryInterface(aTarget));
      nsXFormsUtils::HandleFatalError(targetEle,
                                      NS_LITERAL_STRING("XFormsBindingException"));
      break;
    }
  default:
    break;
  }

  return rv;
}

static void
DeleteVoidArray(void    *aObject,
                nsIAtom *aPropertyName,
                void    *aPropertyValue,
                void    *aData)
{
  nsVoidArray *array = NS_STATIC_CAST(nsVoidArray *, aPropertyValue);
  PRInt32 count = array->Count();
  for (PRInt32 i = 0; i < count; i++) {
    EventItem *item = (EventItem *)array->ElementAt(i);
    delete item;
  }

  array->Clear();
  delete array;
}

nsresult
DeferDispatchEvent(nsIDOMNode* aTarget, nsXFormsEvent aEvent,
                   nsIDOMElement *aSrcElement)
{
  nsCOMPtr<nsIDOMDocument> domDoc;
  if (aTarget) {
    aTarget->GetOwnerDocument(getter_AddRefs(domDoc));
  } else {
    if (aSrcElement) {
      aSrcElement->GetOwnerDocument(getter_AddRefs(domDoc));
    }
  }

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  NS_ENSURE_STATE(doc);

  nsVoidArray *eventList =
    NS_STATIC_CAST(nsVoidArray *,
                   doc->GetProperty(nsXFormsAtoms::deferredEventListProperty));
  if (!eventList) {
    eventList = new nsVoidArray(16);
    if (!eventList)
      return NS_ERROR_OUT_OF_MEMORY;
    doc->SetProperty(nsXFormsAtoms::deferredEventListProperty, eventList,
                     DeleteVoidArray);
  }

  EventItem *deferredEvent = new EventItem;
  NS_ENSURE_TRUE(deferredEvent, NS_ERROR_OUT_OF_MEMORY);
  deferredEvent->event = aEvent;
  deferredEvent->eventTarget = aTarget;
  deferredEvent->srcElement = aSrcElement;
  eventList->AppendElement(deferredEvent);

  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::DispatchEvent(nsIDOMNode* aTarget, nsXFormsEvent aEvent,
                             PRBool *aDefaultActionEnabled,
                             nsIDOMElement *aSrcElement)
{
  // it is valid to have aTarget be null if this is an event that must be
  // targeted at a model per spec and aSrcElement is non-null.  Basically we
  // are trying to make sure that we can handle the case where an event needs to
  // be sent to the model that doesn't exist, yet (like if the model is
  // located at the end of the document and the parser hasn't reached that
  // far).  In those cases, we'll defer the event dispatch until the model
  // exists.

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
        if (!aTarget) {
          return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsIXFormsControl> control = do_QueryInterface(aTarget);
        if (control) {
          PRBool acceptableEventTarget = PR_FALSE;
          control->IsEventTarget(&acceptableEventTarget);
          if (!acceptableEventTarget) {
            return NS_OK;
          }
        }
        break;
      }
    case eEvent_LinkError:
    case eEvent_LinkException:
    case eEvent_ComputeException:
      {
        // these events target only models.  Verifying that the target
        // exists or at least that we have enough information to find the
        // model later when the DOM is finished loading
        if (!aTarget) {
          if (aSrcElement) {
            DeferDispatchEvent(aTarget, aEvent, aSrcElement);
            return NS_OK;
          }
          return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsIModelElementPrivate> modelPriv(do_QueryInterface(aTarget));
        NS_ENSURE_STATE(modelPriv);

        PRBool safeToSendEvent = PR_FALSE;
        modelPriv->GetHasDOMContentFired(&safeToSendEvent);
        if (!safeToSendEvent) {
          DeferDispatchEvent(aTarget, aEvent, nsnull);
          return NS_OK;
        }
      
        break;
      }
    case eEvent_BindingException:
      {
        if (!aTarget) {
          return NS_ERROR_FAILURE;
        }

        // we only need special handling for binding exceptions that are
        // targeted at bind elements and even then, only if the containing
        // model hasn't gotten a DOMContentLoaded, yet.  If this is the case,
        // we'll defer the notification until XMLEvent handlers are attached.
        if (!IsXFormsElement(aTarget, NS_LITERAL_STRING("bind"))) {
          break;
        }

        // look up bind's parent chain looking for the containing model.  If
        // not found, that is not goodness.  Not taking the immediate parent
        // in case the form uses nested binds (which is ok on some processors
        // for XForms 1.0 and should be ok for all processors in XForms 1.1)
        nsCOMPtr<nsIDOMNode> parent, temp = aTarget;
        nsCOMPtr<nsIModelElementPrivate> modelPriv;
        do {
          nsresult rv = temp->GetParentNode(getter_AddRefs(parent));
          NS_ENSURE_SUCCESS(rv, rv);

          modelPriv = do_QueryInterface(parent);
          if (modelPriv) {
            break;
          }
          temp = parent;
        } while (temp);

        NS_ENSURE_STATE(modelPriv);

        PRBool safeToSendEvent = PR_FALSE;
        modelPriv->GetHasDOMContentFired(&safeToSendEvent);
        if (!safeToSendEvent) {
          DeferDispatchEvent(aTarget, aEvent, nsnull);
          return NS_OK;
        }
      
        break;
      }
    default:
      break;
  }

  return DispatchXFormsEvent(aTarget, aEvent, aDefaultActionEnabled);

}

/* static */ nsresult
nsXFormsUtils::DispatchDeferredEvents(nsIDOMDocument* aDocument)
{
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(aDocument));
  NS_ENSURE_STATE(doc);

  nsVoidArray *eventList =
    NS_STATIC_CAST(nsVoidArray *,
                   doc->GetProperty(nsXFormsAtoms::deferredEventListProperty));
  if (!eventList) {
    return NS_OK;
  }

  PRInt32 count = eventList->Count();
  for (PRInt32 i = 0; i < count; i++) {
    EventItem *item = (EventItem *)eventList->ElementAt(i);

    nsCOMPtr<nsIDOMDocument> objCurrDoc;
    if (item->eventTarget) {
      item->eventTarget->GetOwnerDocument(getter_AddRefs(objCurrDoc));
    } else {
      if (item->srcElement) {
        item->srcElement->GetOwnerDocument(getter_AddRefs(objCurrDoc));
      }
    }

    if (!objCurrDoc || (objCurrDoc != aDocument))
    {
      // well the event target or our way to find the event target aren't in
      // the document anymore so we probably shouldn't bother to send out the
      // event.  They are probably on a path to destruction.
      delete item;

      continue;
    }

    if (!item->eventTarget) {
      // item doesn't have an event target AND it has a srcElement.  This
      // should only happen in the case where we wanted to dispatch an event to
      // a model element that hasn't been parsed, yet.  Since
      // DispatchDeferredEvents gets called after DOMContentLoaded is
      // received by the model, then this should no longer be a problem.
      // Go ahead and grab the model from srcElement and make that the
      // event target.
      if (item->srcElement) {
        nsCOMPtr<nsIModelElementPrivate> modelPriv =
          nsXFormsUtils::GetModel(item->srcElement);
        item->eventTarget = do_QueryInterface(modelPriv);
      }
      NS_ENSURE_STATE(item->eventTarget);
    }

    DispatchXFormsEvent(item->eventTarget, item->event, nsnull);
  }

  doc->DeleteProperty(nsXFormsAtoms::deferredEventListProperty);
  return NS_OK;
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
  NS_ASSERTION(allow, "Event handling not allowed!");
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
                                 nsIXFormsControl       **aParentControl,
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
        // We QI from nsIXFormsContextControl to nsIXFormsControl here. This
        // will always suceed when needed, as the only element not
        // implementing both is the model element, and no children of model
        // need to register with the model (which is what aParentControl is
        // needed for).
        if (aParentControl) {
          CallQueryInterface(contextControl, aParentControl); // addrefs
        }

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
nsXFormsUtils::CheckConnectionAllowed(nsIDOMElement *aElement,
                                      nsIURI        *aTestURI,
                                      ConnectionType aType)
{
  if (!aElement || !aTestURI)
    return PR_FALSE;

  nsresult rv;

  
  nsCOMPtr<nsIDOMDocument> domDoc;
  aElement->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return PR_FALSE;

  // Start by checking UniversalBrowserRead, which overrides everything.
  nsIPrincipal *basePrincipal = doc->NodePrincipal();
  PRBool res;
  rv = basePrincipal->IsCapabilityEnabled("UniversalBrowserRead", nsnull, &res);
  if (NS_SUCCEEDED(rv) && res)
    return PR_TRUE;

  // Check same origin
  res = CheckSameOrigin(doc, aTestURI, aType);
  if (!res || aType == kXFormsActionSend)
    return res;

  // Check content policy
  return CheckContentPolicy(aElement, doc, aTestURI);
}

/* static */ PRBool
nsXFormsUtils::CheckSameOrigin(nsIDocument   *aBaseDocument,
                               nsIURI        *aTestURI,
                               ConnectionType aType)
{
  nsresult rv;

  NS_ASSERTION(aBaseDocument && aTestURI, "Got null parameters?!");

  // check the security manager and do a same original check on the principal
  nsIPrincipal* basePrincipal = aBaseDocument->NodePrincipal();
  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  if (secMan) {
    // get a principal for the uri we are testing
    nsCOMPtr<nsIPrincipal> testPrincipal;
    rv = secMan->GetCodebasePrincipal(aTestURI, getter_AddRefs(testPrincipal));

    if (NS_SUCCEEDED(rv)) {
      rv = secMan->CheckSameOriginPrincipal(basePrincipal, testPrincipal);
      if (NS_SUCCEEDED(rv))
        return PR_TRUE;
    }
  }

  // else, check with the permission manager to see if this host is
  // permitted to access sites from other domains.

  nsCOMPtr<nsIPermissionManager> permMgr =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permMgr, PR_FALSE);

  nsCOMPtr<nsIURI> principalURI;
  rv = basePrincipal->GetURI(getter_AddRefs(principalURI));

  if (NS_SUCCEEDED(rv)) {
    PRUint32 perm;
    rv = permMgr->TestPermission(principalURI, "xforms-xd", &perm);

    if (NS_SUCCEEDED(rv) && perm != nsIPermissionManager::UNKNOWN_ACTION) {
      // Safe cast, as we only have few ConnectionTypes.
      PRInt32 permSigned = perm;
      if (permSigned == kXFormsActionLoadSend || permSigned == aType)
        return PR_TRUE;
    }
  }

  return PR_FALSE;
}

/* static */ PRBool
nsXFormsUtils::CheckContentPolicy(nsIDOMElement *aElement,
                                  nsIDocument   *aDoc,
                                  nsIURI        *aURI)
{
  NS_ASSERTION(aElement && aDoc && aURI, "Got null parameters?!");

  nsIURI *docURI = aDoc->GetDocumentURI();
  NS_ENSURE_TRUE(docURI, PR_FALSE);

  PRInt16 decision = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_OTHER,
                                          aURI,
                                          docURI,
                                          aElement,        // context
                                          EmptyCString(),  // mime guess
                                          nsnull,          // extra
                                          &decision);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  return NS_CP_ACCEPTED(decision);
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
nsXFormsUtils::ParseTypeFromNode(nsIDOMNode             *aInstanceData,
                                 nsAString              &aType,
                                 nsAString              &aNSUri)
{
  nsresult rv;

  // Find the model for the instance data node
  nsCOMPtr<nsIDOMNode> instanceNode;
  rv = nsXFormsUtils::GetInstanceNodeForData(aInstanceData,
                                             getter_AddRefs(instanceNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> modelNode;
  rv = instanceNode->GetParentNode(getter_AddRefs(modelNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIModelElementPrivate> model(do_QueryInterface(modelNode));
  NS_ENSURE_STATE(model);

  return model->GetTypeFromNode(aInstanceData, aType, aNSUri);
}

/* static */ void
nsXFormsUtils::ReportError(const nsAString& aMessage, const PRUnichar **aParams,
                           PRUint32 aParamLength, nsIDOMNode *aElement,
                           nsIDOMNode *aContext, PRUint32 aErrorFlag,
                           PRBool aLiteralMessage)
{

  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance("@mozilla.org/scripterror;1");

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);

  if (!(consoleService && errorObject))
    return;

  nsAutoString msg;

  if (!aLiteralMessage) {
    nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);
  
    if (!bundleService)
      return;
    
    // get the string from the bundle (xforms.properties)
    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle("chrome://xforms/locale/xforms.properties",
                                getter_AddRefs(bundle));
    nsXPIDLString message;
    if (aParams) {
      bundle->FormatStringFromName(PromiseFlatString(aMessage).get(), aParams,
                                   aParamLength, getter_Copies(message));
      msg.Append(message);
    } else {
      bundle->GetStringFromName(PromiseFlatString(aMessage).get(),
                                getter_Copies(message));
      msg.Append(message);
    }
  
    if (msg.IsEmpty()) {
  #ifdef DEBUG
      printf("nsXFormsUtils::ReportError() Failed to get message string for message id '%s'!\n",
             NS_ConvertUTF16toUTF8(aMessage).get());
  #endif
      return;
    }
  } else {
    msg.Append(aMessage);
  }


  nsAutoString srcFile, srcLine;

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


  // Log the message to Error Console
#ifdef DEBUG
  printf("ERR: %s\n", NS_ConvertUTF16toUTF8(msg).get());
#endif
  nsresult rv = errorObject->Init(msg.get(), srcFile.get(), srcLine.get(),
                                  0, 0, aErrorFlag, "XForms");
  if (NS_SUCCEEDED(rv)) {
    consoleService->LogMessage(errorObject);
  }
}

/* static */ PRBool
nsXFormsUtils::IsDocumentReadyForBind(nsIDOMElement *aElement)
{
  if (!aElement)
    return PR_FALSE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  NS_ASSERTION(content, "mElement not implementing nsIContent?!");

  nsIDocument* doc = content->GetCurrentDoc();
  if (!doc)
    return PR_FALSE;

  void* test = doc->GetProperty(nsXFormsAtoms::readyForBindProperty);
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
nsXFormsUtils::GetElementById(const nsAString  &aId,
                              const PRBool      aOnlyXForms,
                              nsIDOMElement    *aCaller,
                              nsIDOMElement   **aElement)
{
  NS_ENSURE_TRUE(!aId.IsEmpty(), NS_ERROR_INVALID_ARG);
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = nsnull;

  nsCOMPtr<nsIDOMElement> element;
  GetElementByContextId(aCaller, aId, getter_AddRefs(element));
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
  nsCOMPtr<nsIDOMNode> childNode;
  for (i = 0; i < elements; ++i) {
    descendants->Item(i, getter_AddRefs(childNode));
    childNode->GetNodeType(&type);
    if (type == nsIDOMNode::ELEMENT_NODE) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(childNode));
      NS_ASSERTION(content, "An ELEMENT_NODE not implementing nsIContent?!");
      if (content->AttrValueIs(kNameSpaceID_None, content->GetIDAttributeName(),
                               aId, eCaseMatters)) {
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

/* static */
nsresult
nsXFormsUtils::GetElementByContextId(nsIDOMElement   *aRefNode,
                                     const nsAString &aId,
                                     nsIDOMElement   **aElement)
{
  NS_ENSURE_ARG(aRefNode);
  NS_ENSURE_ARG_POINTER(aElement);

  *aElement = nsnull;

  nsCOMPtr<nsIDOMDocument> document;
  aRefNode->GetOwnerDocument(getter_AddRefs(document));

  // Even if given element is anonymous node then search element by ID attribute
  // of document because anonymous node can inherit attribute from bound node
  // that is refID of document.

  nsresult rv = document->GetElementById(aId, aElement);
  NS_ENSURE_SUCCESS(rv, rv);

  if (*aElement)
    return NS_OK;

  nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(document));
  if (!xblDoc)
    return NS_OK;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aRefNode));
  if (!content)
    return NS_OK;

  // Search for the element with the given value in its 'anonid' attribute
  // throughout the complete bindings chain. We must ensure that the binding
  // parent of currently traversed element is not element itself to avoid an
  // infinite loop.
  nsIContent *boundContent;
  for (boundContent = content->GetBindingParent(); boundContent != nsnull &&
         boundContent != boundContent->GetBindingParent() && !*aElement;
         boundContent = boundContent->GetBindingParent()) {
    nsCOMPtr<nsIDOMElement> boundElm(do_QueryInterface(boundContent));
    xblDoc->GetAnonymousElementByAttribute(boundElm, NS_LITERAL_STRING("anonid"),
                                           aId, aElement);
  }

  return NS_OK;
}

/* static */
PRBool
nsXFormsUtils::HandleFatalError(nsIDOMElement    *aElement,
                                const nsAString  &aName)
{
  if (!aElement) {
    return PR_FALSE;
  }
  nsCOMPtr<nsIDOMDocument> doc;
  aElement->GetOwnerDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIDocument> iDoc(do_QueryInterface(doc));
  if (!iDoc) {
    return PR_FALSE;
  }

  // check for fatalError property, enforcing that only one fatal error will
  // be shown to the user
  if (iDoc->GetProperty(nsXFormsAtoms::fatalError)) {
    return PR_FALSE;
  }
  iDoc->SetProperty(nsXFormsAtoms::fatalError, iDoc);

  // Check for preference, disabling this popup
  PRBool disablePopup = PR_FALSE;
  nsresult rv;
  nsCOMPtr<nsIPrefBranch> pref = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && pref) {
    PRBool val;
    if (NS_SUCCEEDED(pref->GetBoolPref("xforms.disablePopup", &val)))
      disablePopup = val;
  }
  if (disablePopup)
    return PR_FALSE;

  // Get nsIDOMWindowInternal
  nsCOMPtr<nsIDOMWindowInternal> internal;
  rv = nsXFormsUtils::GetWindowFromDocument(doc, getter_AddRefs(internal));
  if (NS_FAILED(rv) || !internal) {
    return PR_FALSE;
  }


  // Show popup
  nsCOMPtr<nsIDOMWindow> messageWindow;
  rv = internal->OpenDialog(NS_LITERAL_STRING("chrome://xforms/content/bindingex.xul"),
                            aName,
                            NS_LITERAL_STRING("modal,dialog,chrome,dependent"),
                            nsnull,
                            getter_AddRefs(messageWindow));
  return NS_SUCCEEDED(rv);
}

/* static */
PRBool
nsXFormsUtils::AreEntitiesEqual(nsIDOMNamedNodeMap *aEntities1,
                                nsIDOMNamedNodeMap *aEntities2)
{
  if (!aEntities1 && !aEntities2) {
    return PR_TRUE;
  }

  if (!aEntities1 || !aEntities2) {
    return PR_FALSE;
  }

  PRUint32 entLength1, entLength2;
  nsresult rv1 = aEntities1->GetLength(&entLength1);
  nsresult rv2 = aEntities2->GetLength(&entLength2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || entLength1 != entLength2) {
    return PR_FALSE;
  }

  nsAutoString buffer1, buffer2;
  for (PRUint32 i = 0; i < entLength1; ++i) {
    nsCOMPtr<nsIDOMNode> entNode1, entNode2;

    rv1 = aEntities1->Item(i, getter_AddRefs(entNode1));
    rv2 = aEntities2->Item(i, getter_AddRefs(entNode2));
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !entNode1 || !entNode2) {
      return PR_FALSE;
    }

    nsCOMPtr<nsIDOMEntity> ent1, ent2;
    ent1 = do_QueryInterface(entNode1);
    ent2 = do_QueryInterface(entNode2);
    if (!ent1 || !ent2) {
      return PR_FALSE;
    }

    rv1 = ent1->GetPublicId(buffer1);
    rv2 = ent2->GetPublicId(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    rv1 = ent1->GetSystemId(buffer1);
    rv2 = ent2->GetSystemId(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    rv1 = ent1->GetNotationName(buffer1);
    rv2 = ent2->GetNotationName(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    // XXX: These will need to be uncommented when Mozilla supports these from
    // DOM3
#if 0
    rv1 = ent1->GetInputEncoding(buffer1);
    rv2 = ent2->GetInputEncoding(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    rv1 = ent1->GetXmlEncoding(buffer1);
    rv2 = ent2->GetXmlEncoding(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }
    rv1 = ent1->GetXmlVersion(buffer1);
    rv2 = ent2->GetXmlVersion(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }
#endif

  }
  return PR_TRUE;
}

/* static */ PRBool
nsXFormsUtils::AreNotationsEqual(nsIDOMNamedNodeMap *aNotations1,
                                 nsIDOMNamedNodeMap *aNotations2)
{
  if (!aNotations1 && !aNotations2) {
    return PR_TRUE;
  }

  if (!aNotations1 || !aNotations2) {
    return PR_FALSE;
  }

  PRUint32 notLength1, notLength2;
  nsresult rv1 = aNotations1->GetLength(&notLength1);
  nsresult rv2 = aNotations2->GetLength(&notLength2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || notLength1 != notLength2) {
    return PR_FALSE;
  }

  nsAutoString buffer1, buffer2;
  for (PRUint32 j = 0; j < notLength1; ++j) {
    nsCOMPtr<nsIDOMNode> notNode1, notNode2;

    rv1 = aNotations1->Item(j, getter_AddRefs(notNode1));
    rv2 = aNotations2->Item(j, getter_AddRefs(notNode2));
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !notNode1 || !notNode2) {
      return PR_FALSE;
    }

    nsCOMPtr<nsIDOMNotation> notation1, notation2;
    notation1 = do_QueryInterface(notNode1);
    notation2 = do_QueryInterface(notNode2);
    if (!notation1 || !notation2) {
      return PR_FALSE;
    }

    rv1 = notation1->GetPublicId(buffer1);
    rv2 = notation2->GetPublicId(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    rv1 = notation1->GetSystemId(buffer1);
    rv2 = notation2->GetSystemId(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

/* static */ PRBool
nsXFormsUtils::AreNodesEqual(nsIDOMNode *aFirstNode, nsIDOMNode *aSecondNode,
                             PRBool aAlreadyNormalized)
{
  if (!aFirstNode || !aSecondNode) {
    return PR_FALSE;
  }

  nsresult rv1, rv2;
  PRUint16 firstType, secondType;
  rv1 = aFirstNode->GetNodeType(&firstType);
  rv2 = aSecondNode->GetNodeType(&secondType);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || firstType != secondType) {
    return PR_FALSE;
  }

  nsAutoString buffer1, buffer2;
  if (firstType == nsIDOMNode::DOCUMENT_TYPE_NODE) {
    nsCOMPtr<nsIDOMDocumentType> doc1 = do_QueryInterface(aFirstNode);
    nsCOMPtr<nsIDOMDocumentType> doc2 = do_QueryInterface(aSecondNode);
    if (!doc1 || !doc2) {
      return PR_FALSE;
    }

    rv1 = doc1->GetName(buffer1);
    rv2 = doc2->GetName(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    rv1 = doc1->GetPublicId(buffer1);
    rv2 = doc2->GetPublicId(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    rv1 = doc1->GetSystemId(buffer1);
    rv2 = doc2->GetSystemId(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    rv1 = doc1->GetInternalSubset(buffer1);
    rv2 = doc2->GetInternalSubset(buffer2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
      return PR_FALSE;
    }

    nsCOMPtr<nsIDOMNamedNodeMap> map1, map2;
    rv1 = doc1->GetEntities(getter_AddRefs(map1));
    rv2 = doc2->GetEntities(getter_AddRefs(map2));

    // XXX need to handle the case where neither has entities?
    if (NS_FAILED(rv1) || NS_FAILED(rv2)) {
      return PR_FALSE;
    }

    PRBool equal = nsXFormsUtils::AreEntitiesEqual(map1, map2);
    if (!equal) {
      return PR_FALSE;
    }

    rv1 = doc1->GetNotations(getter_AddRefs(map1));
    rv2 = doc2->GetNotations(getter_AddRefs(map2));
    if (NS_FAILED(rv1) || NS_FAILED(rv2)) {
      return PR_FALSE;
    }

    equal = nsXFormsUtils::AreNotationsEqual(map1, map2);
    if (!equal) {
      return PR_FALSE;
    }

  }

  rv1 = aFirstNode->GetNodeName(buffer1);
  rv2 = aSecondNode->GetNodeName(buffer2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
    return PR_FALSE;
  }

  rv1 = aFirstNode->GetLocalName(buffer1);
  rv2 = aSecondNode->GetLocalName(buffer2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
    return PR_FALSE;
  }

  rv1 = aFirstNode->GetNamespaceURI(buffer1);
  rv2 = aSecondNode->GetNamespaceURI(buffer2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
    return PR_FALSE;
  }

  rv1 = aFirstNode->GetPrefix(buffer1);
  rv2 = aSecondNode->GetPrefix(buffer2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
    return PR_FALSE;
  }

  rv1 = aFirstNode->GetNodeValue(buffer1);
  rv2 = aSecondNode->GetNodeValue(buffer2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
    return PR_FALSE;
  }

  PRBool hasAttr1, hasAttr2;
  rv1 = aFirstNode->HasAttributes(&hasAttr1);
  rv2 = aSecondNode->HasAttributes(&hasAttr2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || hasAttr1 != hasAttr2) {
    return PR_FALSE;
  }

  if (hasAttr1) {
    nsCOMPtr<nsIDOMNamedNodeMap> attrs1, attrs2;
    PRUint32 attrLength1, attrLength2;

    rv1 = aFirstNode->GetAttributes(getter_AddRefs(attrs1));
    rv2 = aSecondNode->GetAttributes(getter_AddRefs(attrs2));
    if (NS_FAILED(rv1) || NS_FAILED(rv2)) {
      return PR_FALSE;
    }

    rv1 = attrs1->GetLength(&attrLength1);
    rv2 = attrs2->GetLength(&attrLength2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || attrLength1 != attrLength2) {
      return PR_FALSE;
    }

    // the order of the attributes on the two nodes doesn't matter.  But
    // every attribute on node1 must exist on node2 (and no more)
    for (PRUint32 i = 0; i < attrLength1; ++i) {
      nsCOMPtr<nsIDOMNode> attr1, attr2;
      rv1 = attrs1->Item(i, getter_AddRefs(attr1));
      if (!attr1) {
        return PR_FALSE;
      }

      attr1->GetLocalName(buffer1);
      attr1->GetNamespaceURI(buffer2);
      attrs2->GetNamedItemNS(buffer2, buffer1, getter_AddRefs(attr2));
      if (!attr2) {
        return PR_FALSE;
      }

      rv1 = attr1->GetNodeValue(buffer1);
      rv2 = attr2->GetNodeValue(buffer2);
      if (NS_FAILED(rv1) || NS_FAILED(rv2) || !buffer1.Equals(buffer2)) {
        return PR_FALSE;
      }
    }
  }

  // now looking at the child nodes.  They have to be 'equal' and at the same
  // index inside each of the parent nodes.
  PRBool hasChildren1, hasChildren2;
  rv1 = aFirstNode->HasChildNodes(&hasChildren1);
  rv2 = aSecondNode->HasChildNodes(&hasChildren2);
  if (NS_FAILED(rv1) || NS_FAILED(rv2) || hasChildren1 != hasChildren2) {
    return PR_FALSE;
  }

  if (hasChildren1) {
    nsCOMPtr<nsIDOMNodeList> children1, children2;
    PRUint32 childrenLength1, childrenLength2;

    rv1 = aFirstNode->GetChildNodes(getter_AddRefs(children1));
    rv2 = aSecondNode->GetChildNodes(getter_AddRefs(children2));
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || !children1 || !children2) {
      return PR_FALSE;
    }

    rv1 = children1->GetLength(&childrenLength1);
    rv2 = children2->GetLength(&childrenLength2);
    if (NS_FAILED(rv1) || NS_FAILED(rv2) || childrenLength1 != childrenLength2) {
      return PR_FALSE;
    }

    nsCOMPtr<nsIDOMNode> clone1, clone2;
    if (!aAlreadyNormalized) {
      // well we avoided this as long as we can.  If we haven't already
      // normalized all children, now is the time to do it.  We'll have to clone
      // nodes since the normalization process actually changes the DOM.
     
      rv1 = aFirstNode->CloneNode(PR_TRUE, getter_AddRefs(clone1));
      if (NS_FAILED(rv1) || !clone1) {
        return PR_FALSE;
      }
      rv2 = aSecondNode->CloneNode(PR_TRUE, getter_AddRefs(clone2));
      if (NS_FAILED(rv2) || !clone2) {
        return PR_FALSE;
      }

      rv1 = clone1->Normalize();
      rv2 = clone2->Normalize();
      if (NS_FAILED(rv1) || NS_FAILED(rv2)) {
        return PR_FALSE;
      }

      // since this already worked once on the original nodes, won't bother
      // checking the results for the clones
      clone1->GetChildNodes(getter_AddRefs(children1));
      clone2->GetChildNodes(getter_AddRefs(children2));

      // get length again since normalizing may have eliminated some text nodes
      rv1 = children1->GetLength(&childrenLength1);
      rv2 = children2->GetLength(&childrenLength2);
      if (NS_FAILED(rv1) || NS_FAILED(rv2) || childrenLength1 != childrenLength2) {
        return PR_FALSE;
      }
    }

    for (PRUint32 i = 0; i < childrenLength1; ++i) {
      nsCOMPtr<nsIDOMNode> child1, child2;

      rv1 = children1->Item(i, getter_AddRefs(child1));
      rv2 = children2->Item(i, getter_AddRefs(child2));
      if (NS_FAILED(rv1) || NS_FAILED(rv2)) {
        return PR_FALSE;
      }

      PRBool areEqual = nsXFormsUtils::AreNodesEqual(child1, child2, PR_TRUE);
      if (!areEqual) {
        return PR_FALSE;
      }
    }
  }

  return PR_TRUE;

}


/* static */
nsresult
nsXFormsUtils::GetWindowFromDocument(nsIDOMDocument         *aDoc,
                                     nsIDOMWindowInternal  **aWindow)
{
  NS_ENSURE_ARG(aDoc);
  NS_ENSURE_ARG_POINTER(aWindow);
  *aWindow = nsnull;

  // Get nsIDOMWindowInternal
  nsCOMPtr<nsIDOMDocumentView> dview(do_QueryInterface(aDoc));
  NS_ENSURE_STATE(dview);

  nsCOMPtr<nsIDOMAbstractView> aview;
  dview->GetDefaultView(getter_AddRefs(aview));

  nsCOMPtr<nsIDOMWindowInternal> internal(do_QueryInterface(aview));
  NS_ENSURE_STATE(internal);

  NS_ADDREF(*aWindow = internal);
  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::GetModelFromNode(nsIDOMNode *aNode, nsIDOMNode **aModel)
{
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
  NS_ASSERTION(aModel, "no return buffer, we'll crash soon");
  *aModel = nsnull;

  nsAutoString namespaceURI;
  aNode->GetNamespaceURI(namespaceURI);

  // If the node is in the XForms namespace and XTF based, then it should
  //   be able to be handled by GetModel.  Otherwise it is probably an instance
  //   node in a instance document.
  if (!namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIModelElementPrivate> modelPriv = nsXFormsUtils::GetModel(element);
  nsCOMPtr<nsIDOMNode> modelElement = do_QueryInterface(modelPriv);
  if( modelElement ) {
    NS_IF_ADDREF(*aModel = modelElement);
  }

  // No model found
  NS_ENSURE_TRUE(*aModel, NS_ERROR_FAILURE);

  return NS_OK;
}


/* static */ PRBool
nsXFormsUtils::IsNodeAssocWithModel(nsIDOMNode *aNode, nsIDOMNode *aModel)
{

  nsCOMPtr<nsIDOMNode> modelNode;

  nsAutoString namespaceURI;
  aNode->GetNamespaceURI(namespaceURI);

  // If the node is in the XForms namespace and XTF based, then it should
  //   be able to be handled by GetModel.  Otherwise it is probably an instance
  //   node in a instance document.
  if (namespaceURI.EqualsLiteral(NS_NAMESPACE_XFORMS)) {
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
    nsCOMPtr<nsIModelElementPrivate> modelPriv = GetModel(element);
    modelNode = do_QueryInterface(modelPriv);
  } else {
    // We are assuming that if the node coming in isn't a proper XForms element,
    //   then it is an instance element in an instance doc.  Now we just have
    //   to determine if the given model contains this instance document.
    nsCOMPtr<nsIDOMNode> instNode;
    nsresult rv =
      nsXFormsUtils::GetInstanceNodeForData(aNode, getter_AddRefs(instNode));
    if (NS_SUCCEEDED(rv) && instNode) {
      instNode->GetParentNode(getter_AddRefs(modelNode));
    }
  }

  return modelNode && (modelNode == aModel);
}

/* static */ nsresult
nsXFormsUtils::GetInstanceDocumentRoot(const nsAString &aID,
                                       nsIDOMNode *aModelNode,
                                       nsIDOMNode **aInstanceRoot)
{
  nsresult rv = NS_ERROR_FAILURE;
  NS_ASSERTION(aInstanceRoot, "no return buffer, we'll crash soon");
  *aInstanceRoot = nsnull;

  if (aID.IsEmpty()) {
    return rv;
  }

  nsCOMPtr<nsIXFormsModelElement> modelElement = do_QueryInterface(aModelNode);
  nsCOMPtr<nsIDOMDocument> doc;
  rv = modelElement->GetInstanceDocument(aID, getter_AddRefs(doc));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> element;
  rv = doc->GetDocumentElement(getter_AddRefs(element));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_IF_ADDREF(*aInstanceRoot = element);

  return NS_OK;
}

/* static */ PRBool
nsXFormsUtils::ValidateString(const nsAString &aValue, const nsAString & aType,
                              const nsAString & aNamespace)
{
  nsXFormsSchemaValidator validator;

  return validator.ValidateString(aValue, aType, aNamespace);
}

/* static */ nsresult
nsXFormsUtils::GetRepeatIndex(nsIDOMNode *aRepeat, PRInt32 *aIndex)
{
  NS_ASSERTION(aIndex, "no return buffer for index, we'll crash soon");
  *aIndex = 0;

  nsCOMPtr<nsIXFormsRepeatElement> repeatEle = do_QueryInterface(aRepeat);
  if (!repeatEle) {
    // if aRepeat isn't a repeat element, then setting aIndex to -1 to tell
    // XPath to return NaN.  Per 7.8.5 in the spec (1.0, 2nd edition)
    *aIndex = -1;
  } else {
    PRUint32 retIndex = 0;
    nsresult rv = repeatEle->GetIndex(&retIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    *aIndex = retIndex;
  }

  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::GetMonths(const nsAString & aValue, PRInt32 * aMonths)
{
  NS_ASSERTION(aMonths, "no return buffer for months, we'll crash soon");

  *aMonths = 0;
  nsCOMPtr<nsISchemaDuration> duration;
  nsCOMPtr<nsISchemaValidator> schemaValidator = 
    do_CreateInstance("@mozilla.org/schemavalidator;1");
  NS_ENSURE_TRUE(schemaValidator, NS_ERROR_FAILURE);

  nsresult rv = schemaValidator->ValidateBuiltinTypeDuration(aValue, 
                                                    getter_AddRefs(duration));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 sumMonths;
  PRUint32 years;
  PRUint32 months;

  duration->GetYears(&years);
  duration->GetMonths(&months);

  sumMonths = months + years*12;
  PRBool negative;
  duration->GetNegative(&negative);
  if (negative) {
    // according to the spec, "the sign of the result will match the sign
    // of the duration"
    sumMonths *= -1;
  }

  *aMonths = sumMonths;

  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::GetSeconds(const nsAString & aValue, double * aSeconds)
{
  nsCOMPtr<nsISchemaDuration> duration;
  nsCOMPtr<nsISchemaValidator> schemaValidator =
    do_CreateInstance("@mozilla.org/schemavalidator;1");
  NS_ENSURE_TRUE(schemaValidator, NS_ERROR_FAILURE);

  nsresult rv = schemaValidator->ValidateBuiltinTypeDuration(aValue,
                                                    getter_AddRefs(duration));
  NS_ENSURE_SUCCESS(rv, rv);

  double sumSeconds;
  PRUint32 days;
  PRUint32 hours;
  PRUint32 minutes;
  PRUint32 seconds;
  double fractSecs;

  duration->GetDays(&days);
  duration->GetHours(&hours);
  duration->GetMinutes(&minutes);
  duration->GetSeconds(&seconds);
  duration->GetFractionSeconds(&fractSecs);

  sumSeconds = seconds + minutes*60 + hours*3600 + days*24*3600 + fractSecs;

  PRBool negative;
  duration->GetNegative(&negative);
  if (negative) {
    // according to the spec, "the sign of the result will match the sign
    // of the duration"
    sumSeconds *= -1;
  }

  *aSeconds = sumSeconds;
  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::GetSecondsFromDateTime(const nsAString & aValue,
                                      double * aSeconds)
{
  PRTime dateTime;
  nsCOMPtr<nsISchemaValidator> schemaValidator =
    do_CreateInstance("@mozilla.org/schemavalidator;1");
  NS_ENSURE_TRUE(schemaValidator, NS_ERROR_FAILURE);

  nsresult rv = schemaValidator->ValidateBuiltinTypeDateTime(aValue, &dateTime);
  NS_ENSURE_SUCCESS(rv, rv);

  PRTime secs64 = dateTime, remain64;
  PRInt64 usecPerSec;
  PRInt32 secs32, remain32;

  // convert from PRTime (microseconds from epoch) to seconds.
  LL_I2L(usecPerSec, PR_USEC_PER_SEC);
  LL_MOD(remain64, secs64, usecPerSec);   /* remainder after conversion */
  LL_DIV(secs64, secs64, usecPerSec);     /* Conversion in whole seconds */

  // convert whole seconds and remainder to PRInt32
  LL_L2I(secs32, secs64);
  LL_L2I(remain32, remain64);

  // ready the result to send back to transformiix land now in case there are
  // no fractional seconds or we end up having a problem parsing them out.  If
  // we do, we'll just ignore the fractional seconds.
  double totalSeconds = secs32;
  *aSeconds = totalSeconds;

  // We're not getting fractional seconds back in the PRTime we get from
  // the schemaValidator.  We'll have to figure out the fractions from
  // the original value.  Since ValidateBuiltinTypeDateTime returned
  // successful for us to get this far, we know that the value is in
  // the proper format.
  int findFractionalSeconds = aValue.FindChar('.');
  if (findFractionalSeconds < 0) {
    // no fractions of seconds, so we are good to go as we are
    return NS_OK;
  }

  const nsAString& fraction = Substring(aValue, findFractionalSeconds+1,
                                        aValue.Length());

  PRBool done = PR_FALSE;
  PRUnichar currentChar;
  nsCAutoString fractionResult;
  nsAString::const_iterator start, end, buffStart;
  fraction.BeginReading(start);
  fraction.BeginReading(buffStart);
  fraction.EndReading(end);

  while ((start != end) && !done) {
    currentChar = *start++;

    // Time is usually terminated with Z or followed by a time zone
    // (i.e. -05:00).  Time can also be terminated by the end of the string, so
    // test for that as well.  All of this specified at:
    // http://www.w3.org/TR/xmlschema-2/#dateTime
    if ((currentChar == 'Z') || (currentChar == '+') || (currentChar == '-') ||
        (start == end)) {
      fractionResult.AssignLiteral("0.");
      AppendUTF16toUTF8(Substring(buffStart.get(), start.get()-1),
                        fractionResult);
    } else if ((currentChar > '9') || (currentChar < '0')) {
      // has to be a numerical character or else abort.  This should have been
      // caught by the schemavalidator, but it is worth double checking.
      done = PR_TRUE;
    }
  }

  if (fractionResult.IsEmpty()) {
    // couldn't successfully parse the fractional seconds, so we'll just return
    // without them.
    return NS_OK;
  }

  // convert the result string that we have to a double and add it to the total
  totalSeconds += PR_strtod(fractionResult.get(), nsnull);
  *aSeconds = totalSeconds;

  return NS_OK;
}

/* static */ nsresult
nsXFormsUtils::GetDaysFromDateTime(const nsAString & aValue, PRInt32 * aDays)
{
  NS_ASSERTION(aDays, "no return buffer for days, we'll crash soon");
  *aDays = 0;

  PRTime date;
  nsCOMPtr<nsISchemaValidator> schemaValidator =
    do_CreateInstance("@mozilla.org/schemavalidator;1");
  NS_ENSURE_TRUE(schemaValidator, NS_ERROR_FAILURE);

  // aValue could be a xsd:date or a xsd:dateTime.  If it is a dateTime, we
  // should ignore the hours, minutes, and seconds according to 7.10.2 in
  // the spec.  So search for such things now.  If they are there, strip 'em.
  PRInt32 findTime = aValue.FindChar('T');

  nsAutoString dateString;
  dateString.Assign(aValue);
  if (findTime >= 0) {
    dateString.Assign(Substring(dateString, 0, findTime));
  }

  nsresult rv = schemaValidator->ValidateBuiltinTypeDate(dateString, &date);
  NS_ENSURE_SUCCESS(rv, rv);

  PRTime secs64 = date;
  PRInt64 usecPerSec;
  PRInt32 secs32;

  // convert from PRTime (microseconds from epoch) to seconds.  Shouldn't
  // have to worry about remainders since input is a date.  Smallest value
  // is in whole days.
  LL_I2L(usecPerSec, PR_USEC_PER_SEC);
  LL_DIV(secs64, secs64, usecPerSec);

  // convert whole seconds to PRInt32
  LL_L2I(secs32, secs64);

  // convert whole seconds to days.  86400 seconds in a day.
  *aDays = secs32/86400;

  return NS_OK;
}

/* static */ PRBool
nsXFormsUtils::SetSingleNodeBindingValue(nsIDOMElement *aElement,
                                         const nsAString &aValue,
                                         PRBool *aChanged)
{
  *aChanged = PR_FALSE;
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIModelElementPrivate> model;
  if (GetSingleNodeBinding(aElement, getter_AddRefs(node),
                           getter_AddRefs(model)))
  {
    nsresult rv = model->SetNodeValue(node, aValue, PR_FALSE, aChanged);
    if (NS_SUCCEEDED(rv))
      return PR_TRUE;
  }
  return PR_FALSE;
}

/* static */ PRBool
nsXFormsUtils::NodeHasItemset(nsIDOMNode *aNode)
{
  PRBool hasItemset = PR_FALSE;

  nsCOMPtr<nsIDOMNodeList> children;
  aNode->GetChildNodes(getter_AddRefs(children));

  PRUint32 childCount = 0;
  if (children) {
    children->GetLength(&childCount);
  }

  for (PRUint32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDOMNode> child;
    children->Item(i, getter_AddRefs(child));
    if (nsXFormsUtils::IsXFormsElement(child, NS_LITERAL_STRING("itemset"))) {
      hasItemset = PR_TRUE;
      break;
    } else if (nsXFormsUtils::IsXFormsElement(child,
                                              NS_LITERAL_STRING("choices"))) {
      // The choices element may have an itemset.
      hasItemset = nsXFormsUtils::NodeHasItemset(child);
      if (hasItemset) {
        // No need to look at any other children.
        break;
      }
    }
  }
  return hasItemset;
}

/* static */ PRBool
nsXFormsUtils::ExperimentalFeaturesEnabled()
{
  // Return the value of the preference that surrounds all of our
  // 'not yet standardized' XForms work.
  return gExperimentalFeaturesEnabled;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Laurent Jouanneau <laurent.jouanneau@disruptive-innovations.com>
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
#include "nsAutoPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMXPathNSResolver.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"
#include "nsIURI.h"
#include "nsIArray.h"
#include "nsContentUtils.h"
#include "nsArrayUtils.h"
#include "nsPIDOMWindow.h"
#include "nsXULContentUtils.h"

#include "nsXULTemplateBuilder.h"
#include "nsXULTemplateQueryProcessorXML.h"
#include "nsXULTemplateResultXML.h"
#include "nsXULSortService.h"

NS_IMPL_ISUPPORTS1(nsXMLQuery, nsXMLQuery)

//----------------------------------------------------------------------
//
// nsXULTemplateResultSetXML
//

NS_IMPL_ISUPPORTS1(nsXULTemplateResultSetXML, nsISimpleEnumerator)

NS_IMETHODIMP
nsXULTemplateResultSetXML::HasMoreElements(bool *aResult)
{
    // if GetSnapshotLength failed, then the return type was not a set of
    // nodes, so just return false in this case.
    PRUint32 length;
    if (NS_SUCCEEDED(mResults->GetSnapshotLength(&length)))
        *aResult = (mPosition < length);
    else
        *aResult = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultSetXML::GetNext(nsISupports **aResult)
{
    nsCOMPtr<nsIDOMNode> node;
    nsresult rv = mResults->SnapshotItem(mPosition, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);

    nsXULTemplateResultXML* result =
        new nsXULTemplateResultXML(mQuery, node, mBindingSet);
    NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

    ++mPosition;
    *aResult = result;
    NS_ADDREF(result);
    return NS_OK;
}


//----------------------------------------------------------------------
//
// nsXULTemplateQueryProcessorXML
//

static PLDHashOperator
TraverseRuleToBindingsMap(nsISupports* aKey, nsXMLBindingSet* aMatch, void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        static_cast<nsCycleCollectionTraversalCallback*>(aContext);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mRuleToBindingsMap key");
    cb->NoteXPCOMChild(aKey);
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mRuleToBindingsMap value");
    cb->NoteNativeChild(aMatch, &NS_CYCLE_COLLECTION_NAME(nsXMLBindingSet));
    return PL_DHASH_NEXT;
}
  
NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTemplateQueryProcessorXML)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULTemplateQueryProcessorXML)
    if (tmp->mRuleToBindingsMap.IsInitialized()) {
        tmp->mRuleToBindingsMap.Clear();
    }
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRoot)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mEvaluator)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRequest)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULTemplateQueryProcessorXML)
    if (tmp->mRuleToBindingsMap.IsInitialized()) {
        tmp->mRuleToBindingsMap.EnumerateRead(TraverseRuleToBindingsMap, &cb);
    }
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRoot)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mEvaluator)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULTemplateQueryProcessorXML)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULTemplateQueryProcessorXML)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULTemplateQueryProcessorXML)
    NS_INTERFACE_MAP_ENTRY(nsIXULTemplateQueryProcessor)
    NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULTemplateQueryProcessor)
NS_INTERFACE_MAP_END

/*
 * Only the first datasource in aDataSource is used, which should be either an
 * nsIURI of an XML document, or a DOM node. If the former, GetDatasource will
 * load the document asynchronously and return null in aResult. Once the
 * document has loaded, the builder's datasource will be set to the XML
 * document. If the datasource is a DOM node, the node will be returned in
 * aResult.
 */
NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::GetDatasource(nsIArray* aDataSources,
                                              nsIDOMNode* aRootNode,
                                              bool aIsTrusted,
                                              nsIXULTemplateBuilder* aBuilder,
                                              bool* aShouldDelayBuilding,
                                              nsISupports** aResult)
{
    *aResult = nsnull;
    *aShouldDelayBuilding = PR_FALSE;

    nsresult rv;
    PRUint32 length;

    aDataSources->GetLength(&length);
    if (length == 0)
        return NS_OK;

    // we get only the first item, because the query processor supports only
    // one document as a datasource

    nsCOMPtr<nsIDOMNode> node = do_QueryElementAt(aDataSources, 0);
    if (node) {
        return CallQueryInterface(node, aResult);
    }

    nsCOMPtr<nsIURI> uri = do_QueryElementAt(aDataSources, 0);
    if (!uri)
        return NS_ERROR_UNEXPECTED;

    nsCAutoString uriStr;
    rv = uri->GetSpec(uriStr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> root = do_QueryInterface(aRootNode);
    if (!root)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDocument> doc = root->GetCurrentDoc();
    if (!doc)
        return NS_ERROR_UNEXPECTED;

    nsIPrincipal *docPrincipal = doc->NodePrincipal();

    bool hasHadScriptObject = true;
    nsIScriptGlobalObject* scriptObject =
      doc->GetScriptHandlingObject(hasHadScriptObject);
    NS_ENSURE_STATE(scriptObject);

    nsIScriptContext *context = scriptObject->GetContext();
    NS_ENSURE_TRUE(context, NS_OK);

    nsCOMPtr<nsIXMLHttpRequest> req =
        do_CreateInstance(NS_XMLHTTPREQUEST_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindow> owner = do_QueryInterface(scriptObject);
    req->Init(docPrincipal, context, owner, nsnull);

    rv = req->Open(NS_LITERAL_CSTRING("GET"), uriStr, PR_TRUE,
                   EmptyString(), EmptyString());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(req));
    rv = target->AddEventListener(NS_LITERAL_STRING("load"), this, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = target->AddEventListener(NS_LITERAL_STRING("error"), this, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = req->Send(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    mTemplateBuilder = aBuilder;
    mRequest = req;

    *aShouldDelayBuilding = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::InitializeForBuilding(nsISupports* aDatasource,
                                                      nsIXULTemplateBuilder* aBuilder,
                                                      nsIDOMNode* aRootNode)
{
    if (mGenerationStarted)
        return NS_ERROR_UNEXPECTED;

    // the datasource is either a document or a DOM element
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDatasource);
    if (doc)
        doc->GetDocumentElement(getter_AddRefs(mRoot));
    else
      mRoot = do_QueryInterface(aDatasource);
    NS_ENSURE_STATE(mRoot);

    mEvaluator = do_CreateInstance("@mozilla.org/dom/xpath-evaluator;1");
    NS_ENSURE_TRUE(mEvaluator, NS_ERROR_OUT_OF_MEMORY);

    if (!mRuleToBindingsMap.IsInitialized() &&
        !mRuleToBindingsMap.Init())
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::Done()
{
    mGenerationStarted = PR_FALSE;

    if (mRuleToBindingsMap.IsInitialized())
        mRuleToBindingsMap.Clear();

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::CompileQuery(nsIXULTemplateBuilder* aBuilder,
                                             nsIDOMNode* aQueryNode,
                                             nsIAtom* aRefVariable,
                                             nsIAtom* aMemberVariable,
                                             nsISupports** _retval)
{
    nsresult rv = NS_OK;

    *_retval = nsnull;

    nsCOMPtr<nsIContent> content = do_QueryInterface(aQueryNode);

    nsAutoString expr;
    content->GetAttr(kNameSpaceID_None, nsGkAtoms::expr, expr);

    // if an expression is not specified, then the default is to
    // just take all of the children
    if (expr.IsEmpty())
        expr.AssignLiteral("*");

    nsCOMPtr<nsIDOMXPathExpression> compiledexpr;
    rv = CreateExpression(expr, aQueryNode, getter_AddRefs(compiledexpr));
    if (NS_FAILED(rv)) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BAD_XPATH);
        return rv;
    }

    nsRefPtr<nsXMLQuery> query =
        new nsXMLQuery(this, aMemberVariable, compiledexpr);
    NS_ENSURE_TRUE(query, NS_ERROR_OUT_OF_MEMORY);

    for (nsIContent* condition = content->GetFirstChild();
         condition;
         condition = condition->GetNextSibling()) {

        if (condition->NodeInfo()->Equals(nsGkAtoms::assign,
                                          kNameSpaceID_XUL)) {
            nsAutoString var;
            condition->GetAttr(kNameSpaceID_None, nsGkAtoms::var, var);

            nsAutoString expr;
            condition->GetAttr(kNameSpaceID_None, nsGkAtoms::expr, expr);

            // ignore assignments without a variable or an expression
            if (!var.IsEmpty() && !expr.IsEmpty()) {
                nsCOMPtr<nsIDOMNode> conditionNode =
                    do_QueryInterface(condition);
                rv = CreateExpression(expr, conditionNode,
                                      getter_AddRefs(compiledexpr));
                if (NS_FAILED(rv)) {
                    nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BAD_ASSIGN_XPATH);
                    return rv;
                }

                nsCOMPtr<nsIAtom> varatom = do_GetAtom(var);

                rv = query->AddBinding(varatom, compiledexpr);
                NS_ENSURE_SUCCESS(rv, rv);
            }
        }
    }

    *_retval = query;
    NS_ADDREF(*_retval);

    return rv;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::GenerateResults(nsISupports* aDatasource,
                                                nsIXULTemplateResult* aRef,
                                                nsISupports* aQuery,
                                                nsISimpleEnumerator** aResults)
{
    if (!aQuery)
        return NS_ERROR_INVALID_ARG;

    mGenerationStarted = PR_TRUE;

    nsCOMPtr<nsXMLQuery> xmlquery = do_QueryInterface(aQuery);
    if (!xmlquery)
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIDOMNode> context;
    if (aRef)
      aRef->GetBindingObjectFor(xmlquery->GetMemberVariable(),
                                getter_AddRefs(context));
    if (!context)
        context = mRoot;

    nsIDOMXPathExpression* expr = xmlquery->GetResultsExpression();
    if (!expr)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports> exprsupportsresults;
    nsresult rv = expr->Evaluate(context,
                                 nsIDOMXPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                                 nsnull, getter_AddRefs(exprsupportsresults));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMXPathResult> exprresults =
        do_QueryInterface(exprsupportsresults);

    nsXULTemplateResultSetXML* results =
        new nsXULTemplateResultSetXML(xmlquery, exprresults,
                                      xmlquery->GetBindingSet());
    NS_ENSURE_TRUE(results, NS_ERROR_OUT_OF_MEMORY);

    *aResults = results;
    NS_ADDREF(*aResults);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::AddBinding(nsIDOMNode* aRuleNode,
                                           nsIAtom* aVar,
                                           nsIAtom* aRef,
                                           const nsAString& aExpr)
{
    if (mGenerationStarted)
        return NS_ERROR_FAILURE;

    nsRefPtr<nsXMLBindingSet> bindings = mRuleToBindingsMap.GetWeak(aRuleNode);
    if (!bindings) {
        bindings = new nsXMLBindingSet();
        if (!bindings || !mRuleToBindingsMap.Put(aRuleNode, bindings))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIDOMXPathExpression> compiledexpr;
    nsresult rv =
        CreateExpression(aExpr, aRuleNode, getter_AddRefs(compiledexpr));
    if (NS_FAILED(rv)) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BAD_BINDING_XPATH);
        return NS_OK;
    }

    // aRef isn't currently used for XML query processors
    return bindings->AddBinding(aVar, compiledexpr);
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::TranslateRef(nsISupports* aDatasource,
                                             const nsAString& aRefString,
                                             nsIXULTemplateResult** aRef)
{
    *aRef = nsnull;

    // the datasource is either a document or a DOM element
    nsCOMPtr<nsIDOMElement> rootElement;
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDatasource);
    if (doc)
        doc->GetDocumentElement(getter_AddRefs(rootElement));
    else
        rootElement = do_QueryInterface(aDatasource);

    // if no root element, just return. The document may not have loaded yet
    if (!rootElement)
        return NS_OK;
    
    nsXULTemplateResultXML* result =
        new nsXULTemplateResultXML(nsnull, rootElement, nsnull);
    NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

    *aRef = result;
    NS_ADDREF(*aRef);

    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::CompareResults(nsIXULTemplateResult* aLeft,
                                               nsIXULTemplateResult* aRight,
                                               nsIAtom* aVar,
                                               PRUint32 aSortHints,
                                               PRInt32* aResult)
{
    *aResult = 0;
    if (!aVar)
        return NS_OK;

    nsAutoString leftVal;
    if (aLeft)
        aLeft->GetBindingFor(aVar, leftVal);

    nsAutoString rightVal;
    if (aRight)
        aRight->GetBindingFor(aVar, rightVal);

    *aResult = XULSortServiceImpl::CompareValues(leftVal, rightVal, aSortHints);
    return NS_OK;
}

nsXMLBindingSet*
nsXULTemplateQueryProcessorXML::GetOptionalBindingsForRule(nsIDOMNode* aRuleNode)
{
    return mRuleToBindingsMap.GetWeak(aRuleNode);
}

nsresult
nsXULTemplateQueryProcessorXML::CreateExpression(const nsAString& aExpr,
                                                 nsIDOMNode* aNode,
                                                 nsIDOMXPathExpression** aCompiledExpr)
{
    nsCOMPtr<nsIDOMXPathNSResolver> nsResolver;

    nsCOMPtr<nsIDOMDocument> doc;
    aNode->GetOwnerDocument(getter_AddRefs(doc));

    nsCOMPtr<nsIDOMXPathEvaluator> eval = do_QueryInterface(doc);
    if (eval) {
        nsresult rv =
             eval->CreateNSResolver(aNode, getter_AddRefs(nsResolver));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return mEvaluator->CreateExpression(aExpr, nsResolver, aCompiledExpr);
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::HandleEvent(nsIDOMEvent* aEvent)
{
    NS_PRECONDITION(aEvent, "aEvent null");
    nsAutoString eventType;
    aEvent->GetType(eventType);

    if (eventType.EqualsLiteral("load") && mTemplateBuilder) {
        NS_ASSERTION(mRequest, "request was not set");
        nsCOMPtr<nsIDOMDocument> doc;
        if (NS_SUCCEEDED(mRequest->GetResponseXML(getter_AddRefs(doc))))
            mTemplateBuilder->SetDatasource(doc);

        // to avoid leak. we don't need it after...
        mTemplateBuilder = nsnull;
        mRequest = nsnull;
    }
    else if (eventType.EqualsLiteral("error")) {
        mTemplateBuilder = nsnull;
        mRequest = nsnull;
    }

    return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsComponentManagerUtils.h"
#include "nsGkAtoms.h"
#include "nsIURI.h"
#include "nsIArray.h"
#include "nsIScriptContext.h"
#include "nsArrayUtils.h"
#include "nsPIDOMWindow.h"
#include "nsXULContentUtils.h"
#include "nsXMLHttpRequest.h"
#include "mozilla/dom/XPathEvaluator.h"
#include "nsXULTemplateQueryProcessorXML.h"
#include "nsXULTemplateResultXML.h"
#include "nsXULSortService.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsXMLQuery, nsXMLQuery)

//----------------------------------------------------------------------
//
// nsXULTemplateResultSetXML
//

NS_IMPL_ISUPPORTS(nsXULTemplateResultSetXML, nsISimpleEnumerator)

NS_IMETHODIMP
nsXULTemplateResultSetXML::HasMoreElements(bool *aResult)
{
    // if GetSnapshotLength failed, then the return type was not a set of
    // nodes, so just return false in this case.
    ErrorResult rv;
    uint32_t length = mResults->GetSnapshotLength(rv);
    *aResult = !rv.Failed() && mPosition < length;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultSetXML::GetNext(nsISupports **aResult)
{
    ErrorResult rv;
    nsINode* node = mResults->SnapshotItem(mPosition, rv);
    if (rv.Failed()) {
        return rv.StealNSResult();
    }

    nsXULTemplateResultXML* result =
        new nsXULTemplateResultXML(mQuery, node->AsContent(), mBindingSet);

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
    return PL_DHASH_NEXT;
}
  
NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTemplateQueryProcessorXML)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULTemplateQueryProcessorXML)
    tmp->mRuleToBindingsMap.Clear();
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoot)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mEvaluator)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRequest)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULTemplateQueryProcessorXML)
    tmp->mRuleToBindingsMap.EnumerateRead(TraverseRuleToBindingsMap, &cb);
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvaluator)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRequest)
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
    *aResult = nullptr;
    *aShouldDelayBuilding = false;

    nsresult rv;
    uint32_t length;

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

    nsAutoCString uriStr;
    rv = uri->GetSpec(uriStr);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> root = do_QueryInterface(aRootNode);
    if (!root)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIDocument> doc = root->GetUncomposedDoc();
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

    rv = req->Init(docPrincipal, context,
                   scriptObject ? scriptObject : doc->GetScopeObject(),
                   nullptr, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = req->Open(NS_LITERAL_CSTRING("GET"), uriStr, true,
                   EmptyString(), EmptyString());
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<EventTarget> target(do_QueryInterface(req));
    rv = target->AddEventListener(NS_LITERAL_STRING("load"), this, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = target->AddEventListener(NS_LITERAL_STRING("error"), this, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = req->Send(nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    mTemplateBuilder = aBuilder;
    mRequest = req;

    *aShouldDelayBuilding = true;
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
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDatasource);
    if (doc)
        mRoot = doc->GetDocumentElement();
    else
      mRoot = do_QueryInterface(aDatasource);
    NS_ENSURE_STATE(mRoot);

    mEvaluator = new XPathEvaluator();

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::Done()
{
    mGenerationStarted = false;

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
    *_retval = nullptr;

    nsCOMPtr<nsIContent> content = do_QueryInterface(aQueryNode);

    nsAutoString expr;
    content->GetAttr(kNameSpaceID_None, nsGkAtoms::expr, expr);

    // if an expression is not specified, then the default is to
    // just take all of the children
    if (expr.IsEmpty())
        expr.Assign('*');

    ErrorResult rv;
    nsAutoPtr<XPathExpression> compiledexpr;
    compiledexpr = CreateExpression(expr, content, rv);
    if (rv.Failed()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BAD_XPATH);
        return rv.StealNSResult();
    }

    nsRefPtr<nsXMLQuery> query =
        new nsXMLQuery(this, aMemberVariable, Move(compiledexpr));

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
                compiledexpr = CreateExpression(expr, condition, rv);
                if (rv.Failed()) {
                    nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BAD_ASSIGN_XPATH);
                    return rv.StealNSResult();
                }

                nsCOMPtr<nsIAtom> varatom = do_GetAtom(var);

                query->AddBinding(varatom, Move(compiledexpr));
            }
        }
    }

    query.forget(_retval);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::GenerateResults(nsISupports* aDatasource,
                                                nsIXULTemplateResult* aRef,
                                                nsISupports* aQuery,
                                                nsISimpleEnumerator** aResults)
{
    if (!aQuery)
        return NS_ERROR_INVALID_ARG;

    mGenerationStarted = true;

    nsCOMPtr<nsXMLQuery> xmlquery = do_QueryInterface(aQuery);
    if (!xmlquery)
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsISupports> supports;
    nsCOMPtr<nsINode> context;
    if (aRef)
      aRef->GetBindingObjectFor(xmlquery->GetMemberVariable(),
                                getter_AddRefs(supports));
    context = do_QueryInterface(supports);
    if (!context)
        context = mRoot;

    XPathExpression* expr = xmlquery->GetResultsExpression();
    if (!expr)
        return NS_ERROR_FAILURE;

    ErrorResult rv;
    nsRefPtr<XPathResult> exprresults =
        expr->Evaluate(*context, XPathResult::ORDERED_NODE_SNAPSHOT_TYPE,
                       nullptr, rv);
    if (rv.Failed()) {
        return rv.StealNSResult();
    }

    nsRefPtr<nsXULTemplateResultSetXML> results =
        new nsXULTemplateResultSetXML(xmlquery, exprresults.forget(),
                                      xmlquery->GetBindingSet());

    results.forget(aResults);

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
        mRuleToBindingsMap.Put(aRuleNode, bindings);
    }

    nsCOMPtr<nsINode> ruleNode = do_QueryInterface(aRuleNode);

    ErrorResult rv;
    nsAutoPtr<XPathExpression> compiledexpr;
    compiledexpr = CreateExpression(aExpr, ruleNode, rv);
    if (rv.Failed()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BAD_BINDING_XPATH);
        return NS_OK;
    }

    // aRef isn't currently used for XML query processors
    bindings->AddBinding(aVar, Move(compiledexpr));
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::TranslateRef(nsISupports* aDatasource,
                                             const nsAString& aRefString,
                                             nsIXULTemplateResult** aRef)
{
    *aRef = nullptr;

    // the datasource is either a document or a DOM element
    nsCOMPtr<Element> rootElement;
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDatasource);
    if (doc)
        rootElement = doc->GetRootElement();
    else
        rootElement = do_QueryInterface(aDatasource);

    // if no root element, just return. The document may not have loaded yet
    if (!rootElement)
        return NS_OK;
    
    nsRefPtr<nsXULTemplateResultXML> result = new nsXULTemplateResultXML(nullptr, rootElement, nullptr);
    result.forget(aRef);

    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorXML::CompareResults(nsIXULTemplateResult* aLeft,
                                               nsIXULTemplateResult* aRight,
                                               nsIAtom* aVar,
                                               uint32_t aSortHints,
                                               int32_t* aResult)
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

XPathExpression*
nsXULTemplateQueryProcessorXML::CreateExpression(const nsAString& aExpr,
                                                 nsINode* aNode,
                                                 ErrorResult& aRv)
{
    return mEvaluator->CreateExpression(aExpr, aNode, aRv);
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
        mTemplateBuilder = nullptr;
        mRequest = nullptr;
    }
    else if (eventType.EqualsLiteral("error")) {
        mTemplateBuilder = nullptr;
        mRequest = nullptr;
    }

    return NS_OK;
}

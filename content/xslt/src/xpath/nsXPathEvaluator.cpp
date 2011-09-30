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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
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

#include "nsXPathEvaluator.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIDOMClassInfo.h"
#include "nsXPathExpression.h"
#include "nsXPathNSResolver.h"
#include "nsXPathResult.h"
#include "nsContentCID.h"
#include "txExpr.h"
#include "txExprParser.h"
#include "nsDOMError.h"
#include "txURIUtils.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMString.h"
#include "nsINameSpaceManager.h"
#include "txError.h"
#include "nsContentUtils.h"

// txIParseContext implementation
class nsXPathEvaluatorParseContext : public txIParseContext
{
public:
    nsXPathEvaluatorParseContext(nsXPathEvaluator &aEvaluator,
                                 nsIDOMXPathNSResolver* aResolver,
                                 nsTArray<PRInt32> *aNamespaceIDs,
                                 nsTArray<nsCString> *aContractIDs,
                                 nsCOMArray<nsISupports> *aState,
                                 bool aIsCaseSensitive)
        : mEvaluator(aEvaluator),
          mResolver(aResolver),
          mNamespaceIDs(aNamespaceIDs),
          mContractIDs(aContractIDs),
          mState(aState),
          mLastError(NS_OK),
          mIsCaseSensitive(aIsCaseSensitive)
    {
        NS_ASSERTION(mContractIDs ||
                     (!mNamespaceIDs || mNamespaceIDs->Length() == 0),
                     "Need contract IDs if there are namespaces.");
    }

    nsresult getError()
    {
        return mLastError;
    }

    nsresult resolveNamespacePrefix(nsIAtom* aPrefix, PRInt32& aID);
    nsresult resolveFunctionCall(nsIAtom* aName, PRInt32 aID,
                                 FunctionCall** aFunction);
    bool caseInsensitiveNameTests();
    void SetErrorOffset(PRUint32 aOffset);

private:
    nsXPathEvaluator &mEvaluator;
    nsIDOMXPathNSResolver* mResolver;
    nsTArray<PRInt32> *mNamespaceIDs;
    nsTArray<nsCString> *mContractIDs;
    nsCOMArray<nsISupports> *mState;
    nsresult mLastError;
    bool mIsCaseSensitive;
};

DOMCI_DATA(XPathEvaluator, nsXPathEvaluator)

NS_IMPL_AGGREGATED(nsXPathEvaluator)
NS_INTERFACE_MAP_BEGIN_AGGREGATED(nsXPathEvaluator)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXPathEvaluator)
    NS_INTERFACE_MAP_ENTRY(nsIXPathEvaluatorInternal)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XPathEvaluator)
NS_INTERFACE_MAP_END

nsXPathEvaluator::nsXPathEvaluator(nsISupports *aOuter)
{
    NS_INIT_AGGREGATED(aOuter);
}

nsresult
nsXPathEvaluator::Init()
{
    nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(fOuter);

    return document ? SetDocument(document) : NS_OK;
}

NS_IMETHODIMP
nsXPathEvaluator::CreateExpression(const nsAString & aExpression,
                                   nsIDOMXPathNSResolver *aResolver,
                                   nsIDOMXPathExpression **aResult)
{
    return CreateExpression(aExpression, aResolver, (nsTArray<PRInt32>*)nsnull,
                            nsnull, nsnull, aResult);
}

NS_IMETHODIMP
nsXPathEvaluator::CreateNSResolver(nsIDOMNode *aNodeResolver,
                                   nsIDOMXPathNSResolver **aResult)
{
    NS_ENSURE_ARG(aNodeResolver);
    if (!nsContentUtils::CanCallerAccess(aNodeResolver))
        return NS_ERROR_DOM_SECURITY_ERR;

    *aResult = new nsXPathNSResolver(aNodeResolver);
    NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXPathEvaluator::Evaluate(const nsAString & aExpression,
                           nsIDOMNode *aContextNode,
                           nsIDOMXPathNSResolver *aResolver,
                           PRUint16 aType,
                           nsISupports *aInResult,
                           nsISupports **aResult)
{
    nsCOMPtr<nsIDOMXPathExpression> expression;
    nsresult rv = CreateExpression(aExpression, aResolver,
                                   getter_AddRefs(expression));
    NS_ENSURE_SUCCESS(rv, rv);

    return expression->Evaluate(aContextNode, aType, aInResult, aResult);
}


NS_IMETHODIMP
nsXPathEvaluator::SetDocument(nsIDOMDocument* aDocument)
{
    mDocument = do_GetWeakReference(aDocument);
    return NS_OK;
}

NS_IMETHODIMP
nsXPathEvaluator::CreateExpression(const nsAString & aExpression,
                                   nsIDOMXPathNSResolver *aResolver,
                                   nsTArray<nsString> *aNamespaceURIs,
                                   nsTArray<nsCString> *aContractIDs,
                                   nsCOMArray<nsISupports> *aState,
                                   nsIDOMXPathExpression **aResult)
{
    nsTArray<PRInt32> namespaceIDs;
    if (aNamespaceURIs) {
        PRUint32 count = aNamespaceURIs->Length();

        if (!aContractIDs || aContractIDs->Length() != count) {
            return NS_ERROR_FAILURE;
        }

        if (!namespaceIDs.SetLength(count)) {
            return NS_ERROR_OUT_OF_MEMORY;
        }

        PRUint32 i;
        for (i = 0; i < count; ++i) {
            if (aContractIDs->ElementAt(i).IsEmpty()) {
                return NS_ERROR_FAILURE;
            }

            nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURIs->ElementAt(i), namespaceIDs[i]);
        }
    }


    return CreateExpression(aExpression, aResolver, &namespaceIDs, aContractIDs,
                            aState, aResult);
}

nsresult
nsXPathEvaluator::CreateExpression(const nsAString & aExpression,
                                   nsIDOMXPathNSResolver *aResolver,
                                   nsTArray<PRInt32> *aNamespaceIDs,
                                   nsTArray<nsCString> *aContractIDs,
                                   nsCOMArray<nsISupports> *aState,
                                   nsIDOMXPathExpression **aResult)
{
    nsresult rv;
    if (!mRecycler) {
        nsRefPtr<txResultRecycler> recycler = new txResultRecycler;
        NS_ENSURE_TRUE(recycler, NS_ERROR_OUT_OF_MEMORY);
        
        rv = recycler->init();
        NS_ENSURE_SUCCESS(rv, rv);
        
        mRecycler = recycler;
    }

    nsCOMPtr<nsIDocument> doc = do_QueryReferent(mDocument);
    nsXPathEvaluatorParseContext pContext(*this, aResolver, aNamespaceIDs,
                                          aContractIDs, aState,
                                          !(doc && doc->IsHTML()));

    nsAutoPtr<Expr> expression;
    rv = txExprParser::createExpr(PromiseFlatString(aExpression), &pContext,
                                  getter_Transfers(expression));
    if (NS_FAILED(rv)) {
        if (rv == NS_ERROR_DOM_NAMESPACE_ERR) {
            return NS_ERROR_DOM_NAMESPACE_ERR;
        }

        return NS_ERROR_DOM_INVALID_EXPRESSION_ERR;
    }

    nsCOMPtr<nsIDOMDocument> document = do_QueryReferent(mDocument);

    *aResult = new nsXPathExpression(expression, mRecycler, document);
    if (!*aResult) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*aResult);
    return NS_OK;
}

/*
 * Implementation of txIParseContext private to nsXPathEvaluator, based on a
 * nsIDOMXPathNSResolver
 */

nsresult nsXPathEvaluatorParseContext::resolveNamespacePrefix
    (nsIAtom* aPrefix, PRInt32& aID)
{
    aID = kNameSpaceID_Unknown;

    if (!mResolver) {
        return NS_ERROR_DOM_NAMESPACE_ERR;
    }

    nsAutoString prefix;
    if (aPrefix) {
        aPrefix->ToString(prefix);
    }

    nsVoidableString ns;
    nsresult rv = mResolver->LookupNamespaceURI(prefix, ns);
    NS_ENSURE_SUCCESS(rv, rv);

    if (DOMStringIsNull(ns)) {
        return NS_ERROR_DOM_NAMESPACE_ERR;
    }

    if (ns.IsEmpty()) {
        aID = kNameSpaceID_None;

        return NS_OK;
    }

    // get the namespaceID for the URI
    return nsContentUtils::NameSpaceManager()->RegisterNameSpace(ns, aID);
}

extern nsresult
TX_ResolveFunctionCallXPCOM(const nsCString &aContractID, PRInt32 aNamespaceID,
                            nsIAtom *aName, nsISupports *aState,
                            FunctionCall **aFunction);

nsresult
nsXPathEvaluatorParseContext::resolveFunctionCall(nsIAtom* aName,
                                                  PRInt32 aID,
                                                  FunctionCall** aFn)
{
    nsresult rv = NS_ERROR_XPATH_UNKNOWN_FUNCTION;

    PRUint32 i, count = mNamespaceIDs ? mNamespaceIDs->Length() : 0;
    for (i = 0; i < count; ++i) {
        if (mNamespaceIDs->ElementAt(i) == aID) {
            nsISupports *state = mState ? mState->SafeObjectAt(i) : nsnull;
            rv = TX_ResolveFunctionCallXPCOM(mContractIDs->ElementAt(i), aID,
                                             aName, state, aFn);
            if (NS_SUCCEEDED(rv)) {
                break;
            }
        }
    }

    return rv;
}

bool nsXPathEvaluatorParseContext::caseInsensitiveNameTests()
{
    return !mIsCaseSensitive;
}

void
nsXPathEvaluatorParseContext::SetErrorOffset(PRUint32 aOffset)
{
}

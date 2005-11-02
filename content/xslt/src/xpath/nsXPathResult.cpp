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

#include "nsXPathResult.h"
#include "txExprResult.h"
#include "txNodeSet.h"
#include "nsDOMError.h"
#include "nsIContent.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMNode.h"
#include "nsXPathException.h"
#include "nsIDOMDocument.h"
#include "nsDOMString.h"

nsXPathResult::nsXPathResult() : mDocument(nsnull),
                                 mCurrentPos(0),
                                 mResultType(ANY_TYPE),
                                 mInvalidIteratorState(PR_TRUE)
{
}

nsXPathResult::~nsXPathResult()
{
    if (mDocument) {
        mDocument->RemoveObserver(this);
    }
}

NS_IMPL_ADDREF(nsXPathResult)
NS_IMPL_RELEASE(nsXPathResult)
NS_INTERFACE_MAP_BEGIN(nsXPathResult)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathResult)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIXPathResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXPathResult)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XPathResult)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsXPathResult::GetResultType(PRUint16 *aResultType)
{
    *aResultType = mResultType;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetNumberValue(double *aNumberValue)
{
    if (mResultType != NUMBER_TYPE) {
        return NS_ERROR_DOM_TYPE_ERR;
    }

    *aNumberValue = mResult.get()->numberValue();

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetStringValue(nsAString &aStringValue)
{
    if (mResultType != STRING_TYPE) {
        return NS_ERROR_DOM_TYPE_ERR;
    }

    mResult.get()->stringValue(aStringValue);

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetBooleanValue(PRBool *aBooleanValue)
{
    if (mResultType != BOOLEAN_TYPE) {
        return NS_ERROR_DOM_TYPE_ERR;
    }

    *aBooleanValue = mResult.get()->booleanValue();

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetSingleNodeValue(nsIDOMNode **aSingleNodeValue)
{
    if (!isNode()) {
        return NS_ERROR_DOM_TYPE_ERR;
    }

    txNodeSet *nodeSet = NS_STATIC_CAST(txNodeSet*, mResult.get());
    if (nodeSet->size() > 0) {
        return txXPathNativeNode::getNode(nodeSet->get(0), aSingleNodeValue);
    }

    *aSingleNodeValue = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetInvalidIteratorState(PRBool *aInvalidIteratorState)
{
    *aInvalidIteratorState = isIterator() && mInvalidIteratorState;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetSnapshotLength(PRUint32 *aSnapshotLength)
{
    if (!isSnapshot()) {
        return NS_ERROR_DOM_TYPE_ERR;
    }

    txNodeSet *nodeSet = NS_STATIC_CAST(txNodeSet*, mResult.get());
    *aSnapshotLength = (PRUint32)nodeSet->size();

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::IterateNext(nsIDOMNode **aResult)
{
    if (!isIterator()) {
        return NS_ERROR_DOM_TYPE_ERR;
    }

    if (mDocument) {
        mDocument->FlushPendingNotifications(Flush_Content);
    }

    if (mInvalidIteratorState) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    txNodeSet *nodeSet = NS_STATIC_CAST(txNodeSet*, mResult.get());
    if (mCurrentPos < (PRUint32)nodeSet->size()) {
        return txXPathNativeNode::getNode(nodeSet->get(mCurrentPos++),
                                          aResult);
    }

    *aResult = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::SnapshotItem(PRUint32 aIndex, nsIDOMNode **aResult)
{
    if (!isSnapshot()) {
        return NS_ERROR_DOM_TYPE_ERR;
    }

    txNodeSet *nodeSet = NS_STATIC_CAST(txNodeSet*, mResult.get());
    if (aIndex < (PRUint32)nodeSet->size()) {
        return txXPathNativeNode::getNode(nodeSet->get(aIndex), aResult);
    }

    *aResult = nsnull;

    return NS_OK;
}

NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(nsXPathResult)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsXPathResult)
NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(nsXPathResult)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(nsXPathResult)
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(nsXPathResult)

void
nsXPathResult::CharacterDataChanged(nsIDocument* aDocument,
                                    nsIContent *aContent,
                                    PRBool aAppend)
{
    Invalidate();
}

void
nsXPathResult::AttributeChanged(nsIDocument* aDocument,
                                nsIContent* aContent,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType)
{
    Invalidate();
}

void
nsXPathResult::ContentAppended(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
    Invalidate();
}

void
nsXPathResult::ContentInserted(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer)
{
    Invalidate();
}

void
nsXPathResult::ContentRemoved(nsIDocument* aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
    Invalidate();
}

nsresult
nsXPathResult::SetExprResult(txAExprResult* aExprResult, PRUint16 aResultType)
{
    if (mDocument) {
        mDocument->RemoveObserver(this);
        mDocument = nsnull;
    }
 
    mResultType = aResultType;
    mResult.set(aExprResult);

    if (!isIterator()) {
        return NS_OK;
    }

    mInvalidIteratorState = PR_FALSE;

    txNodeSet* nodeSet = NS_STATIC_CAST(txNodeSet*, aExprResult);
    nsCOMPtr<nsIDOMNode> node;
    if (nodeSet->size() > 0) {
        nsresult rv = txXPathNativeNode::getNode(nodeSet->get(0),
                                                 getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);

        // If we support the document() function in DOM-XPath we need to
        // observe all documents that we have resultnodes in.
        nsCOMPtr<nsIDOMDocument> document;
        node->GetOwnerDocument(getter_AddRefs(document));
        if (document) {
            mDocument = do_QueryInterface(document);
        }
        else {
            mDocument = do_QueryInterface(node);
        }

        NS_ASSERTION(mDocument, "We need a document!");
        if (mDocument) {
            mDocument->AddObserver(this);
        }
    }

    return NS_OK;
}

void
nsXPathResult::Invalidate()
{
    if (mDocument) {
        mDocument->RemoveObserver(this);
        mDocument = nsnull;
    }
    mInvalidIteratorState = PR_TRUE;
}

nsresult
nsXPathResult::GetExprResult(txAExprResult** aExprResult)
{
    if (isIterator() && mInvalidIteratorState) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    *aExprResult = mResult.get();
    if (!*aExprResult) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    NS_ADDREF(*aExprResult);

    return NS_OK;
}

nsresult
nsXPathResult::Clone(nsIXPathResult **aResult)
{
    *aResult = nsnull;

    if (isIterator() && mInvalidIteratorState) {
        return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    nsCOMPtr<nsIXPathResult> result = new nsXPathResult();
    if (!result) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = result->SetExprResult(mResult.get(), mResultType);
    NS_ENSURE_SUCCESS(rv, rv);

    result.swap(*aResult);

    return NS_OK;
}

void
txResultHolder::set(txAExprResult *aResult)
{
    releaseNodeSet();

    // XXX This will keep the recycler alive, should we clear it?
    mResult = aResult;

    if (mResult && mResult->getResultType() == txAExprResult::NODESET) {
        txNodeSet *nodeSet =
            NS_STATIC_CAST(txNodeSet*,
                           NS_STATIC_CAST(txAExprResult*, mResult));
        PRInt32 i, count = nodeSet->size();
        for (i = 0; i < count; ++i) {
            txXPathNativeNode::addRef(nodeSet->get(i));
        }
    }
}

void
txResultHolder::releaseNodeSet()
{
    if (mResult && mResult->getResultType() == txAExprResult::NODESET) {
        txNodeSet *nodeSet =
            NS_STATIC_CAST(txNodeSet*,
                           NS_STATIC_CAST(txAExprResult*, mResult));
        PRInt32 i, count = nodeSet->size();
        for (i = 0; i < count; ++i) {
            txXPathNativeNode::release(nodeSet->get(i));
        }
    }
}

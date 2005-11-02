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
#include "ExprResult.h"
#include "txNodeSet.h"
#include "nsDOMError.h"
#include "nsIContent.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMNode.h"
#include "nsXPathException.h"
#include "nsIDOMDocument.h"
#include "nsDOMString.h"

nsXPathResult::nsXPathResult() : mNumberValue(0),
                                 mDocument(0),
                                 mCurrentPos(0),
                                 mResultType(ANY_TYPE),
                                 mInvalidIteratorState(PR_TRUE)
{
}

nsXPathResult::~nsXPathResult()
{
    Reset();
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
    NS_ENSURE_ARG(aResultType);
    *aResultType = mResultType;
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetNumberValue(double *aNumberValue)
{
    if (mResultType != NUMBER_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    NS_ENSURE_ARG(aNumberValue);
    *aNumberValue = mNumberValue;
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetStringValue(nsAString &aStringValue)
{
    if (mResultType != STRING_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    if (mStringValue)
        aStringValue.Assign(*mStringValue);
    else
        SetDOMStringToNull(aStringValue);
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetBooleanValue(PRBool *aBooleanValue)
{
    if (mResultType != BOOLEAN_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    NS_ENSURE_ARG(aBooleanValue);
    *aBooleanValue = mBooleanValue;
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetSingleNodeValue(nsIDOMNode **aSingleNodeValue)
{
    if (mResultType != FIRST_ORDERED_NODE_TYPE &&
        mResultType != ANY_UNORDERED_NODE_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    NS_ENSURE_ARG(aSingleNodeValue);
    *aSingleNodeValue = mNode;
    NS_IF_ADDREF(*aSingleNodeValue);
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetInvalidIteratorState(PRBool *aInvalidIteratorState)
{
    NS_ENSURE_ARG(aInvalidIteratorState);

    if (mResultType != UNORDERED_NODE_ITERATOR_TYPE &&
        mResultType != ORDERED_NODE_ITERATOR_TYPE) {
        *aInvalidIteratorState = PR_FALSE;
        return NS_OK;
    }

    *aInvalidIteratorState = mInvalidIteratorState;
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::GetSnapshotLength(PRUint32 *aSnapshotLength)
{
    if (mResultType != UNORDERED_NODE_SNAPSHOT_TYPE &&
        mResultType != ORDERED_NODE_SNAPSHOT_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    NS_ENSURE_ARG(aSnapshotLength);
    *aSnapshotLength = 0;
    if (mElements)
        *aSnapshotLength = mElements->Count();
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::IterateNext(nsIDOMNode **aResult)
{
    if (mResultType != UNORDERED_NODE_ITERATOR_TYPE &&
        mResultType != ORDERED_NODE_ITERATOR_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    if (mDocument)
        mDocument->FlushPendingNotifications(Flush_Content);

    if (mInvalidIteratorState)
        return NS_ERROR_DOM_INVALID_STATE_ERR;

    NS_ENSURE_ARG(aResult);
    if (mElements && mCurrentPos < (PRUint32)mElements->Count()) {
        *aResult = mElements->ObjectAt(mCurrentPos++);
        NS_ADDREF(*aResult);
        return NS_OK;
    }
    *aResult = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::SnapshotItem(PRUint32 aIndex, nsIDOMNode **aResult)
{
    if (mResultType != UNORDERED_NODE_SNAPSHOT_TYPE &&
        mResultType != ORDERED_NODE_SNAPSHOT_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    NS_ENSURE_ARG(aResult);
    if (mElements && aIndex < (PRUint32)mElements->Count()) {
        *aResult = mElements->ObjectAt(aIndex);
        NS_ADDREF(*aResult);
        return NS_OK;
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

NS_IMETHODIMP
nsXPathResult::SetExprResult(txAExprResult* aExprResult, PRUint16 aResultType)
{
    Reset();
 
    mResultType = aResultType;

    if (mResultType == NUMBER_TYPE) {
        mNumberValue = aExprResult->numberValue();
        return NS_OK;
    }

    if (mResultType == STRING_TYPE) {
        mStringValue = new nsString;
        NS_ENSURE_TRUE(mStringValue, NS_ERROR_OUT_OF_MEMORY);
        aExprResult->stringValue(*mStringValue);
        return NS_OK;
    }

    if (mResultType == BOOLEAN_TYPE) {
        mBooleanValue = aExprResult->booleanValue();
        return NS_OK;
    }

    if (aExprResult->getResultType() == txAExprResult::NODESET) {
        nsresult rv = NS_OK;
        txNodeSet* nodeSet = NS_STATIC_CAST(txNodeSet*, aExprResult);

        if (mResultType == FIRST_ORDERED_NODE_TYPE ||
            mResultType == ANY_UNORDERED_NODE_TYPE) {
            if (nodeSet->size() > 0) {
                txXPathNativeNode::getNode(nodeSet->get(0), &mNode);
            }
        }
        else {
            if (mResultType == UNORDERED_NODE_ITERATOR_TYPE ||
                mResultType == ORDERED_NODE_ITERATOR_TYPE) {
                mInvalidIteratorState = PR_FALSE;
            }

            PRInt32 count = nodeSet->size();
            if (count == 0)
                return NS_OK;

            mElements = new nsCOMArray<nsIDOMNode>;
            NS_ENSURE_TRUE(mElements, NS_ERROR_OUT_OF_MEMORY);

            nsCOMPtr<nsIDOMNode> node;
            PRInt32 i;
            for (i = 0; i < count; ++i) {
                txXPathNativeNode::getNode(nodeSet->get(i), getter_AddRefs(node));
                NS_ASSERTION(node, "node isn't an nsIDOMNode");
                mElements->AppendObject(node);
            }

            // If we support the document() function in DOM-XPath we need to
            // observe all documents that we have resultnodes in.
            if (mResultType == UNORDERED_NODE_ITERATOR_TYPE ||
                mResultType == ORDERED_NODE_ITERATOR_TYPE) {
                nsCOMPtr<nsIDOMDocument> document;
                node->GetOwnerDocument(getter_AddRefs(document));
                if (document)
                    mDocument = do_QueryInterface(document);
                else
                    mDocument = do_QueryInterface(node);

                NS_ASSERTION(mDocument, "We need a document!");
                if (mDocument)
                    mDocument->AddObserver(this);
            }
        }
        return rv;
    }

    return NS_ERROR_DOM_TYPE_ERR;
}

void
nsXPathResult::Invalidate()
{
    if (mDocument) {
        mDocument->RemoveObserver(this);
        mDocument = 0;
    }
    mInvalidIteratorState = PR_TRUE;
}

void
nsXPathResult::Reset()
{
    Invalidate();

    if (mResultType == STRING_TYPE) {
        delete mStringValue;
        mStringValue = 0;
    }
    else if (mResultType == UNORDERED_NODE_ITERATOR_TYPE ||
             mResultType == ORDERED_NODE_ITERATOR_TYPE ||
             mResultType == UNORDERED_NODE_SNAPSHOT_TYPE ||
             mResultType == ORDERED_NODE_SNAPSHOT_TYPE) {
        delete mElements;
        mCurrentPos = 0;
    }
    else if (mResultType == FIRST_ORDERED_NODE_TYPE ||
             mResultType == ANY_UNORDERED_NODE_TYPE) {
        NS_IF_RELEASE(mNode);
    }

    mResultType = ANY_TYPE;
    return;
}

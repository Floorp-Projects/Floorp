/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXPathResult.h"
#include "ExprResult.h"
#include "NodeSet.h"
#include "nsDOMError.h"
#include "nsIContent.h"
#include "nsIDOMClassInfo.h"
#include "nsSupportsArray.h"
#include "nsXPathException.h"

NS_IMPL_ADDREF(nsXPathResult)
NS_IMPL_RELEASE(nsXPathResult)
NS_INTERFACE_MAP_BEGIN(nsXPathResult)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathResult)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIXPathResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXPathResult)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XPathResult)
NS_INTERFACE_MAP_END

nsXPathResult::nsXPathResult() : mNumberValue(0),
                                 mDocument(0),
                                 mCurrentPos(0),
                                 mResultType(ANY_TYPE),
                                 mInvalidIteratorState(PR_TRUE)
{
    NS_INIT_ISUPPORTS();
}

nsXPathResult::~nsXPathResult()
{
    Reset();
}

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
    if (mElements)
        return mElements->Count(aSnapshotLength);
    *aSnapshotLength = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::IterateNext(nsIDOMNode **aResult)
{
    if (mResultType != UNORDERED_NODE_ITERATOR_TYPE &&
        mResultType != ORDERED_NODE_ITERATOR_TYPE)
        return NS_ERROR_DOM_TYPE_ERR;

    if (mDocument)
        mDocument->FlushPendingNotifications(PR_FALSE);

    if (mInvalidIteratorState)
        return NS_ERROR_DOM_INVALID_STATE_ERR;

    NS_ENSURE_ARG(aResult);
    if (mElements) {
        PRUint32 count;
        mElements->Count(&count);
        if (mCurrentPos < count) {
            return mElements->QueryElementAt(mCurrentPos++,
                                             NS_GET_IID(nsIDOMNode),
                                             (void**)aResult);
        }
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
    if (mElements) {
        PRUint32 count;
        mElements->Count(&count);
        if (aIndex < count) {
            return mElements->QueryElementAt(aIndex,
                                             NS_GET_IID(nsIDOMNode),
                                             (void**)aResult);
        }
    }
    *aResult = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::BeginUpdate(nsIDocument* aDocument)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::EndUpdate(nsIDocument* aDocument)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::BeginLoad(nsIDocument* aDocument)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::EndLoad(nsIDocument* aDocument)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::BeginReflow(nsIDocument* aDocument,
                           nsIPresShell* aShell)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::EndReflow(nsIDocument* aDocument,
                         nsIPresShell* aShell)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::ContentChanged(nsIDocument* aDocument,
                              nsIContent *aContent,
                              nsISupports *aSubContent)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::ContentStatesChanged(nsIDocument* aDocument,
                                    nsIContent* aContent1,
                                    nsIContent* aContent2,
                                    PRInt32 aStateMask)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::AttributeChanged(nsIDocument* aDocument,
                                nsIContent* aContent,
                                PRInt32 aNameSpaceID,
                                nsIAtom* aAttribute,
                                PRInt32 aModType,
                                PRInt32 aHint)
{
    Invalidate();
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::ContentAppended(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
    Invalidate();
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::ContentInserted(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer)
{
    Invalidate();
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::ContentReplaced(nsIDocument* aDocument,
                               nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer)
{
    Invalidate();
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::ContentRemoved(nsIDocument* aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
    Invalidate();
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::StyleSheetAdded(nsIDocument* aDocument,
                               nsIStyleSheet* aStyleSheet)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::StyleSheetRemoved(nsIDocument* aDocument,
                                 nsIStyleSheet* aStyleSheet)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::StyleSheetDisabledStateChanged(nsIDocument* aDocument,
                                              nsIStyleSheet* aStyleSheet,
                                              PRBool aDisabled)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::StyleRuleChanged(nsIDocument* aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule,
                                PRInt32 aHint)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::StyleRuleAdded(nsIDocument* aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::StyleRuleRemoved(nsIDocument* aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::DocumentWillBeDestroyed(nsIDocument* aDocument)
{
    Invalidate();
    return NS_OK;
}

NS_IMETHODIMP
nsXPathResult::SetExprResult(ExprResult* aExprResult, PRUint16 aResultType)
{
    Reset();
 
    mResultType = aResultType;

    if (mResultType == NUMBER_TYPE) {
        mNumberValue = aExprResult->numberValue();
        return NS_OK;
    }

    if (mResultType == STRING_TYPE) {
        mStringValue = new String;
        NS_ENSURE_TRUE(mStringValue, NS_ERROR_OUT_OF_MEMORY);
        aExprResult->stringValue(*mStringValue);
        return NS_OK;
    }

    if (mResultType == BOOLEAN_TYPE) {
        mBooleanValue = aExprResult->booleanValue();
        return NS_OK;
    }

    if (aExprResult->getResultType() == ExprResult::NODESET) {
        nsresult rv = NS_OK;
        NodeSet* nodeSet = (NodeSet*)aExprResult;

        if (mResultType == FIRST_ORDERED_NODE_TYPE ||
            mResultType == ANY_UNORDERED_NODE_TYPE) {
            Node* node = nodeSet->get(0);
            if (node)
                rv = CallQueryInterface(node->getNSObj(), &mNode);
        }
        else {
            if (mResultType == UNORDERED_NODE_ITERATOR_TYPE ||
                mResultType == ORDERED_NODE_ITERATOR_TYPE) {
                mInvalidIteratorState = PR_FALSE;
            }

            int count = nodeSet->size();
            if (count == 0)
                return NS_OK;

            NS_NewISupportsArray(&mElements);
            NS_ENSURE_TRUE(mElements, NS_ERROR_OUT_OF_MEMORY);

            nsISupports* mozNode = nsnull;
            int i;
            for (i = 0; i < count; ++i) {
                mozNode = nodeSet->get(i)->getNSObj();
                mElements->AppendElement(mozNode);
                NS_ADDREF(mozNode);
            }
            if (mResultType == UNORDERED_NODE_ITERATOR_TYPE ||
                mResultType == ORDERED_NODE_ITERATOR_TYPE) {
                nsCOMPtr<nsIContent> content = do_QueryInterface(mozNode);
                if (content)
                    content->GetDocument(*getter_AddRefs(mDocument));
                else
                    mDocument = do_QueryInterface(mozNode);
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
        NS_IF_RELEASE(mElements);
        mCurrentPos = 0;
    }
    else if (mResultType == FIRST_ORDERED_NODE_TYPE ||
             mResultType == ANY_UNORDERED_NODE_TYPE) {
        NS_IF_RELEASE(mNode);
    }

    mResultType = ANY_TYPE;
    return;
}

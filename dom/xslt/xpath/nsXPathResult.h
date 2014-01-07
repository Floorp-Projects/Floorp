/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPathResult_h__
#define nsXPathResult_h__

#include "txExprResult.h"
#include "nsIDOMXPathResult.h"
#include "nsStubMutationObserver.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsWeakPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

class nsIDocument;

// {662f2c9a-c7cd-4cab-9349-e733df5a838c}
#define NS_IXPATHRESULT_IID \
{ 0x662f2c9a, 0xc7cd, 0x4cab, {0x93, 0x49, 0xe7, 0x33, 0xdf, 0x5a, 0x83, 0x8c }}

class nsIXPathResult : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPATHRESULT_IID)
    virtual nsresult SetExprResult(txAExprResult *aExprResult,
                                   uint16_t aResultType,
                                   nsINode* aContextNode) = 0;
    virtual nsresult GetExprResult(txAExprResult **aExprResult) = 0;
    virtual nsresult Clone(nsIXPathResult **aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPathResult, NS_IXPATHRESULT_IID)

/**
 * A class for evaluating an XPath expression string
 */
class nsXPathResult MOZ_FINAL : public nsIDOMXPathResult,
                                public nsStubMutationObserver,
                                public nsIXPathResult
{
public:
    nsXPathResult();
    nsXPathResult(const nsXPathResult &aResult);
    ~nsXPathResult();

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXPathResult, nsIDOMXPathResult)

    // nsIDOMXPathResult interface
    NS_DECL_NSIDOMXPATHRESULT

    // nsIMutationObserver interface
    NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
    NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
    NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
    NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

    // nsIXPathResult interface
    nsresult SetExprResult(txAExprResult *aExprResult, uint16_t aResultType,
                           nsINode* aContextNode) MOZ_OVERRIDE;
    nsresult GetExprResult(txAExprResult **aExprResult) MOZ_OVERRIDE;
    nsresult Clone(nsIXPathResult **aResult) MOZ_OVERRIDE;
    void RemoveObserver();
private:
    static bool isSnapshot(uint16_t aResultType)
    {
        return aResultType == UNORDERED_NODE_SNAPSHOT_TYPE ||
               aResultType == ORDERED_NODE_SNAPSHOT_TYPE;
    }
    static bool isIterator(uint16_t aResultType)
    {
        return aResultType == UNORDERED_NODE_ITERATOR_TYPE ||
               aResultType == ORDERED_NODE_ITERATOR_TYPE;
    }
    static bool isNode(uint16_t aResultType)
    {
        return aResultType == FIRST_ORDERED_NODE_TYPE ||
               aResultType == ANY_UNORDERED_NODE_TYPE;
    }
    bool isSnapshot() const
    {
        return isSnapshot(mResultType);
    }
    bool isIterator() const
    {
        return isIterator(mResultType);
    }
    bool isNode() const
    {
        return isNode(mResultType);
    }

    void Invalidate(const nsIContent* aChangeRoot);

    nsRefPtr<txAExprResult> mResult;
    nsCOMArray<nsIDOMNode> mResultNodes;
    nsCOMPtr<nsIDocument> mDocument;
    uint32_t mCurrentPos;
    uint16_t mResultType;
    nsWeakPtr mContextNode;
    bool mInvalidIteratorState;
    bool mBooleanResult;
    double mNumberResult;
    nsString mStringResult;
};

#endif

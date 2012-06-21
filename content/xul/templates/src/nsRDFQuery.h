/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFQuery_h__
#define nsRDFQuery_h__

#include "nsAutoPtr.h"
#include "nsISimpleEnumerator.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

#define NS_ITEMPLATERDFQUERY_IID \
  {0x8929ff60, 0x1c9c, 0x4d87, \
    { 0xac, 0x02, 0x09, 0x14, 0x15, 0x3b, 0x48, 0xc4 }}

/**
 * A compiled query in the RDF query processor. This interface should not be
 * used directly outside of the RDF query processor.
 */
class nsITemplateRDFQuery : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITEMPLATERDFQUERY_IID)

    // return the processor the query was created from
    virtual nsXULTemplateQueryProcessorRDF* Processor() = 0;  // not addrefed

    // return the member variable for the query
    virtual nsIAtom* GetMemberVariable() = 0; // not addrefed

    // return the <query> node the query was compiled from
    virtual void GetQueryNode(nsIDOMNode** aQueryNode) = 0;

    // remove any results that are cached by the query
    virtual void ClearCachedResults() = 0;
};

class nsRDFQuery MOZ_FINAL : public nsITemplateRDFQuery
{
public:

    nsRDFQuery(nsXULTemplateQueryProcessorRDF* aProcessor)
      : mProcessor(aProcessor),
        mSimple(false),
        mRoot(nsnull),
        mCachedResults(nsnull)
    { }

    ~nsRDFQuery() { Finish(); }

    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(nsRDFQuery)

    /**
     * Retrieve the root node in the rule network
     * @return the root node in the rule network
     */
    TestNode* GetRoot() { return mRoot; }

    void SetRoot(TestNode* aRoot) { mRoot = aRoot; }

    void GetQueryNode(nsIDOMNode** aQueryNode)
    {
       *aQueryNode = mQueryNode;
       NS_IF_ADDREF(*aQueryNode);
    }

    void SetQueryNode(nsIDOMNode* aQueryNode)
    {
       mQueryNode = aQueryNode;
    }

    // an optimization is used when several queries all use the simple query
    // syntax. Since simple queries can only generate one possible set of
    // results, they only need to be calculated once and reused for every
    // simple query. The results may be cached in the query for this purpose.
    // If successful, this method takes ownership of aInstantiations.
    nsresult SetCachedResults(nsXULTemplateQueryProcessorRDF* aProcessor,
                              const InstantiationSet& aInstantiations);

    // grab the cached results, if any, causing the caller to take ownership
    // of them. This also has the effect of setting the cached results in this
    // nsRDFQuery to null.
    void UseCachedResults(nsISimpleEnumerator** aResults);

    // clear the cached results
    void ClearCachedResults()
    {
        mCachedResults = nsnull;
    }

    nsXULTemplateQueryProcessorRDF* Processor() { return mProcessor; }

    nsIAtom* GetMemberVariable() { return mMemberVariable; }

    bool IsSimple() { return mSimple; }

    void SetSimple() { mSimple = true; }

    // the reference and member variables for the query
    nsCOMPtr<nsIAtom> mRefVariable;
    nsCOMPtr<nsIAtom> mMemberVariable;

protected:

    nsXULTemplateQueryProcessorRDF* mProcessor;

    // true if the query is a simple rule (one with a default query)
    bool mSimple;

    /**
     * The root node in the network for this query
     */
    TestNode *mRoot;

    // the <query> node
    nsCOMPtr<nsIDOMNode> mQueryNode;

    // used for simple rules since their results are all determined in one step
    nsCOMPtr<nsISimpleEnumerator> mCachedResults;

    void Finish();
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsRDFQuery, NS_ITEMPLATERDFQUERY_IID)

#endif // nsRDFQuery_h__

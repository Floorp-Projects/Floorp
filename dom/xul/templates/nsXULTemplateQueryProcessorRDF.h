/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsXULTemplateQueryProcessorRDF_h__
#define nsXULTemplateQueryProcessorRDF_h__

#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIXULTemplateQueryProcessor.h"
#include "nsCollationCID.h"

#include "nsResourceSet.h"
#include "nsRuleNetwork.h"
#include "nsRDFQuery.h"
#include "nsRDFBinding.h"
#include "nsXULTemplateResultSetRDF.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

#include "mozilla/Logging.h"
extern mozilla::LazyLogModule gXULTemplateLog;

class nsIContent;
class nsXULTemplateResultRDF;

/**
 * An object that generates results from a query on an RDF graph
 */
class nsXULTemplateQueryProcessorRDF final : public nsIXULTemplateQueryProcessor,
                                             public nsIRDFObserver
{
public:
    typedef nsTArray<RefPtr<nsXULTemplateResultRDF> > ResultArray;

    nsXULTemplateQueryProcessorRDF();

    nsresult InitGlobals();

    // nsISupports interface
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXULTemplateQueryProcessorRDF,
                                             nsIXULTemplateQueryProcessor)

    // nsIXULTemplateQueryProcessor interface
    NS_DECL_NSIXULTEMPLATEQUERYPROCESSOR

    // nsIRDFObserver interface
    NS_DECL_NSIRDFOBSERVER

    /*
     * Propagate all changes through the rule network when an assertion is
     * added to the graph, adding any new results.
     */
    nsresult
    Propagate(nsIRDFResource* aSource,
              nsIRDFResource* aProperty,
              nsIRDFNode* aTarget);

    /*
     * Retract all changes through the rule network when an assertion is
     * removed from the graph, removing any results that no longer match.
     */
    nsresult
    Retract(nsIRDFResource* aSource,
            nsIRDFResource* aProperty,
            nsIRDFNode* aTarget);

    /*
     * Synchronize results when the graph changes, updating their bindings.
     */
    nsresult
    SynchronizeAll(nsIRDFResource* aSource,
                   nsIRDFResource* aProperty,
                   nsIRDFNode* aOldTarget,
                   nsIRDFNode* aNewTarget);

    /*
     * Return true if a resource is a container
     */
    nsresult
    CheckContainer(nsIRDFResource* aTargetResource,
                   bool* aIsContainer);

    /*
     * Check if a resource does not have any children
     */
    nsresult
    CheckEmpty(nsIRDFResource* aTargetResource,
               bool* aIsEmpty);

    /**
     * Check if a resource is a separator
     */
    nsresult
    CheckIsSeparator(nsIRDFResource* aResource, bool* aIsSeparator);

    /*
     * Compute the containment properties which are additional arcs which
     * indicate that a node is a container, in additional to the RDF container
     * tests. The computed list is stored in mContainmentProperties
     */
    nsresult
    ComputeContainmentProperties(nsIDOMNode* aRootNode);

    /**
     * Compile a query that uses the extended template syntax. The last
     * compiled node of the query is returned as aLastNode. This node will
     * have been added to mAllTests which owns the node.
     */
    nsresult
    CompileExtendedQuery(nsRDFQuery* aQuery,
                         nsIContent* aConditions,
                         TestNode** aLastNode);

    /**
     * Compile a single query child and return the compiled node in aResult.
     * This node will have been added to mAllTests which owns the node and
     * set as a child of aParentNode.
     */
    virtual nsresult
    CompileQueryChild(nsAtom* aTag,
                      nsRDFQuery* aQuery,
                      nsIContent* aConditions,
                      TestNode* aParentNode,
                      TestNode** aResult);

    /**
     * Parse the value of a property test assertion for a condition or a simple
     * rule based on the parseType attribute into the appropriate literal type.
     */
    nsresult ParseLiteral(const nsString& aParseType,
                          const nsString& aValue,
                          nsIRDFNode** aResult);

    /**
     * Compile a <triple> condition and return the compiled node in aResult.
     * This node will have been added to mAllTests which owns the node and
     * set as a child of aParentNode.
     */
    nsresult
    CompileTripleCondition(nsRDFQuery* aQuery,
                           nsIContent* aCondition,
                           TestNode* aParentNode,
                           TestNode** aResult);

    /**
     * Compile a <member> condition and return the compiled node in aResult.
     * This node will have been added to mAllTests which owns the node and
     * set as a child of aParentNode.
     */
    nsresult
    CompileMemberCondition(nsRDFQuery* aQuery,
                           nsIContent* aCondition,
                           TestNode* aParentNode,
                           TestNode** aResult);

    /**
     * Add the default rules shared by all simple queries. This creates
     * the content start node followed by a member test. The member TestNode
     * is returned in aChildNode. Both nodes will have been added to mAllTests
     * which owns the nodes.
     */
    nsresult
    AddDefaultSimpleRules(nsRDFQuery* aQuery,
                          TestNode** aChildNode);

    /**
     * Compile a query that's specified using the simple template
     * syntax. Each  TestNode is created in a chain, the last compiled node
     * is returned as aLastNode. All nodes will have been added to mAllTests
     * which owns the nodes.
     */
    nsresult
    CompileSimpleQuery(nsRDFQuery* aQuery,
                      nsIContent* aQueryElement,
                      TestNode** aLastNode);

    RDFBindingSet*
    GetBindingsForRule(nsIDOMNode* aRule);

    /*
     * Indicate that a result is dependant on a particular resource. When an
     * assertion is added to or removed from the graph involving that
     * resource, that result must be recalculated.
     */
    void
    AddBindingDependency(nsXULTemplateResultRDF* aResult,
                         nsIRDFResource* aResource);

    /**
     * Remove a dependency a result has on a particular resource.
     */
    void
    RemoveBindingDependency(nsXULTemplateResultRDF* aResult,
                            nsIRDFResource* aResource);

    /**
     * A memory element is a hash of an RDF triple. One exists for each triple
     * that was involved in generating a result. This function adds this to a
     * map, keyed by memory element, when the value is a list of results that
     * depend on that memory element. When an RDF triple is removed from the
     * datasource, RetractElement is called, and this map is examined to
     * determine which results are no longer valid.
     */
    nsresult
    AddMemoryElements(const Instantiation& aInst,
                      nsXULTemplateResultRDF* aResult);

    /**
     * Remove the memory elements associated with a result when the result is
     * no longer being used.
     */
    nsresult
    RemoveMemoryElements(const Instantiation& aInst,
                         nsXULTemplateResultRDF* aResult);

    /**
     * Remove the results associated with a memory element since the
     * RDF triple the memory element is a hash of has been removed.
     */
    void RetractElement(const MemoryElement& aMemoryElement);

    /**
     * Return the index of a result's resource in its RDF container
     */
    int32_t
    GetContainerIndexOf(nsIXULTemplateResult* aResult);

    /**
     * Given a result and a predicate to sort on, get the target value of
     * the triple to use for sorting. The sort predicate is the predicate
     * with '?sort=true' appended.
     */
    nsresult
    GetSortValue(nsIXULTemplateResult* aResult,
                 nsIRDFResource* aPredicate,
                 nsIRDFResource* aSortPredicate,
                 nsISupports** aResultNode);

    nsIRDFDataSource* GetDataSource() { return mDB; }

    nsIXULTemplateBuilder* GetBuilder() { return mBuilder; }

    nsResourceSet& ContainmentProperties() { return mContainmentProperties; }

    nsresult
    Log(const char* aOperation,
        nsIRDFResource* aSource,
        nsIRDFResource* aProperty,
        nsIRDFNode* aTarget);

#define LOG(_op, _src, _prop, _targ) \
    Log(_op, _src, _prop, _targ)

protected:
    ~nsXULTemplateQueryProcessorRDF();

    // We are an observer of the composite datasource. The cycle is
    // broken when the document is destroyed.
    nsCOMPtr<nsIRDFDataSource> mDB;

    // weak reference to the builder, cleared when the document is destroyed
    nsIXULTemplateBuilder* mBuilder;

    // true if the query processor has been initialized
    bool mQueryProcessorRDFInited;

    // true if results have been generated. Once set, bindings can no longer
    // be added. If they were, the binding value arrays for results that have
    // already been generated would be the wrong size
    bool mGenerationStarted;

    // nesting level for RDF batch notifications
    int32_t mUpdateBatchNest;

    // containment properties that are checked to determine if a resource is
    // a container
    nsResourceSet mContainmentProperties;

    // the end node of the default simple node hierarchy
    TestNode* mSimpleRuleMemberTest;

    // the reference variable
    RefPtr<nsAtom> mRefVariable;

    // the last ref that was calculated, used for simple rules
    nsCOMPtr<nsIXULTemplateResult> mLastRef;

    /**
     * A map between nsIRDFNodes that form the left-hand side (the subject) of
     * a <binding> and an array of nsIXULTemplateResults. When a new assertion
     * is added to the graph involving a particular rdf node, it is looked up
     * in this binding map. If it exists, the corresponding results must then
     * be synchronized.
     */
    nsClassHashtable<nsISupportsHashKey, ResultArray> mBindingDependencies;

    /**
     * A map between memory elements and an array of nsIXULTemplateResults.
     * When a triple is unasserted from the graph, the corresponding results
     * no longer match so they must be removed.
     */
    nsClassHashtable<nsUint32HashKey,
                     nsCOMArray<nsXULTemplateResultRDF> > mMemoryElementToResultMap;

    // map of the rules to the bindings for those rules.
    // XXXndeakin this might be better just as an array since there is usually
    //            ten or fewer rules
    nsRefPtrHashtable<nsISupportsHashKey, RDFBindingSet> mRuleToBindingsMap;

    /**
     * The queries
     */
    nsTArray<nsCOMPtr<nsITemplateRDFQuery> > mQueries;

    /**
     * All of the RDF tests in the rule network, which are checked when a new
     * assertion is added to the graph. This is a subset of mAllTests, which
     * also includes non-RDF tests.
     */
    ReteNodeSet mRDFTests;

    /**
     * All of the tests in the rule network, owned by this list
     */
    ReteNodeSet mAllTests;

    // pseudo-constants
    static nsrefcnt gRefCnt;

public:
    static nsIRDFService*            gRDFService;
    static nsIRDFContainerUtils*     gRDFContainerUtils;
    static nsIRDFResource*           kNC_BookmarkSeparator;
    static nsIRDFResource*           kRDF_type;
};

#endif // nsXULTemplateQueryProcessorRDF_h__

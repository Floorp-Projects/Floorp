/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFInferDataSource.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsIDOMDocument.h"
#include "nsAttrName.h"
#include "rdf.h"
#include "nsArrayUtils.h"

#include "nsContentTestNode.h"
#include "nsRDFConInstanceTestNode.h"
#include "nsRDFConMemberTestNode.h"
#include "nsRDFPropertyTestNode.h"
#include "nsInstantiationNode.h"
#include "nsRDFTestNode.h"
#include "nsXULContentUtils.h"
#include "nsXULTemplateBuilder.h"
#include "nsXULTemplateResultRDF.h"
#include "nsXULTemplateResultSetRDF.h"
#include "nsXULTemplateQueryProcessorRDF.h"
#include "nsXULSortService.h"

//----------------------------------------------------------------------

static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

#define PARSE_TYPE_INTEGER  "Integer"

nsrefcnt                  nsXULTemplateQueryProcessorRDF::gRefCnt = 0;
nsIRDFService*            nsXULTemplateQueryProcessorRDF::gRDFService;
nsIRDFContainerUtils*     nsXULTemplateQueryProcessorRDF::gRDFContainerUtils;
nsIRDFResource*           nsXULTemplateQueryProcessorRDF::kNC_BookmarkSeparator;
nsIRDFResource*           nsXULTemplateQueryProcessorRDF::kRDF_type;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTemplateQueryProcessorRDF)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULTemplateQueryProcessorRDF)
    tmp->Done();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

static PLDHashOperator
BindingDependenciesTraverser(nsISupports* key,
                             nsCOMArray<nsXULTemplateResultRDF>* array,
                             void* userArg)
{
    nsCycleCollectionTraversalCallback *cb = 
        static_cast<nsCycleCollectionTraversalCallback*>(userArg);

    PRInt32 i, count = array->Count();
    for (i = 0; i < count; ++i) {
        cb->NoteXPCOMChild(array->ObjectAt(i));
    }

    return PL_DHASH_NEXT;
}

static PLDHashOperator
MemoryElementTraverser(const PRUint32& key,
                       nsCOMArray<nsXULTemplateResultRDF>* array,
                       void* userArg)
{
    nsCycleCollectionTraversalCallback *cb = 
        static_cast<nsCycleCollectionTraversalCallback*>(userArg);

    PRInt32 i, count = array->Count();
    for (i = 0; i < count; ++i) {
        cb->NoteXPCOMChild(array->ObjectAt(i));
    }

    return PL_DHASH_NEXT;
}

static PLDHashOperator
RuleToBindingTraverser(nsISupports* key, RDFBindingSet* binding, void* userArg)
{
    nsCycleCollectionTraversalCallback *cb = 
        static_cast<nsCycleCollectionTraversalCallback*>(userArg);

    cb->NoteXPCOMChild(key);

    return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULTemplateQueryProcessorRDF)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDB)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mLastRef)
    if (tmp->mBindingDependencies.IsInitialized()) {
        tmp->mBindingDependencies.EnumerateRead(BindingDependenciesTraverser,
                                                &cb);
    }
    if (tmp->mMemoryElementToResultMap.IsInitialized()) {
        tmp->mMemoryElementToResultMap.EnumerateRead(MemoryElementTraverser,
                                                     &cb);
    }
    if (tmp->mRuleToBindingsMap.IsInitialized()) {
        tmp->mRuleToBindingsMap.EnumerateRead(RuleToBindingTraverser, &cb);
    }
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mQueries)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULTemplateQueryProcessorRDF)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULTemplateQueryProcessorRDF)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULTemplateQueryProcessorRDF)
    NS_INTERFACE_MAP_ENTRY(nsIXULTemplateQueryProcessor)
    NS_INTERFACE_MAP_ENTRY(nsIRDFObserver)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULTemplateQueryProcessor)
NS_INTERFACE_MAP_END

nsXULTemplateQueryProcessorRDF::nsXULTemplateQueryProcessorRDF(void)
    : mDB(nsnull),
      mBuilder(nsnull),
      mQueryProcessorRDFInited(false),
      mGenerationStarted(false),
      mUpdateBatchNest(0),
      mSimpleRuleMemberTest(nsnull)
{
    gRefCnt++;
}

nsXULTemplateQueryProcessorRDF::~nsXULTemplateQueryProcessorRDF(void)
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(gRDFContainerUtils);
        NS_IF_RELEASE(kNC_BookmarkSeparator);
        NS_IF_RELEASE(kRDF_type);
    }
}

nsresult
nsXULTemplateQueryProcessorRDF::InitGlobals()
{
    nsresult rv;

    // Initialize the global shared reference to the service
    // manager and get some shared resource objects.
    if (!gRDFService) {
        rv = CallGetService(kRDFServiceCID, &gRDFService);
        if (NS_FAILED(rv))
            return rv;
    }

    if (!gRDFContainerUtils) {
        rv = CallGetService(kRDFContainerUtilsCID, &gRDFContainerUtils);
        if (NS_FAILED(rv))
            return rv;
    }
  
    if (!kNC_BookmarkSeparator) {
        gRDFService->GetResource(
          NS_LITERAL_CSTRING(NC_NAMESPACE_URI "BookmarkSeparator"),
                             &kNC_BookmarkSeparator);
    }

    if (!kRDF_type) {
        gRDFService->GetResource(
          NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
                             &kRDF_type);
    }

    return MemoryElement::Init() ? NS_OK : NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
//
// nsIXULTemplateQueryProcessor interface
//

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::GetDatasource(nsIArray* aDataSources,
                                              nsIDOMNode* aRootNode,
                                              bool aIsTrusted,
                                              nsIXULTemplateBuilder* aBuilder,
                                              bool* aShouldDelayBuilding,
                                              nsISupports** aResult)
{
    nsCOMPtr<nsIRDFCompositeDataSource> compDB;
    nsCOMPtr<nsIContent> root = do_QueryInterface(aRootNode);
    nsresult rv;

    *aResult = nsnull;
    *aShouldDelayBuilding = false;

    NS_ENSURE_TRUE(root, NS_ERROR_UNEXPECTED);

    // make sure the RDF service is set up
    rv = InitGlobals();
    NS_ENSURE_SUCCESS(rv, rv);

    // create a database for the builder
    compDB = do_CreateInstance(NS_RDF_DATASOURCE_CONTRACTID_PREFIX 
                               "composite-datasource");
    if (!compDB) {
        NS_ERROR("unable to construct new composite data source");
        return NS_ERROR_UNEXPECTED;
    }

    // check for magical attributes. XXX move to ``flags''?
    if (root->AttrValueIs(kNameSpaceID_None,
                          nsGkAtoms::coalesceduplicatearcs,
                          nsGkAtoms::_false, eCaseMatters))
        compDB->SetCoalesceDuplicateArcs(false);

    if (root->AttrValueIs(kNameSpaceID_None,
                          nsGkAtoms::allownegativeassertions,
                          nsGkAtoms::_false, eCaseMatters))
        compDB->SetAllowNegativeAssertions(false);

    if (aIsTrusted) {
        // If we're a privileged (e.g., chrome) document, then add the
        // local store as the first data source in the db. Note that
        // we _might_ not be able to get a local store if we haven't
        // got a profile to read from yet.
        nsCOMPtr<nsIRDFDataSource> localstore;
        rv = gRDFService->GetDataSource("rdf:local-store",
                                        getter_AddRefs(localstore));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = compDB->AddDataSource(localstore);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add local store to db");
        NS_ENSURE_SUCCESS(rv, rv);
    }

    PRUint32 length, index;
    rv = aDataSources->GetLength(&length);
    NS_ENSURE_SUCCESS(rv,rv);

    for (index = 0; index < length; index++) {

        nsCOMPtr<nsIURI> uri = do_QueryElementAt(aDataSources, index);
        if (!uri) // we ignore other datasources than uri
            continue;

        nsCOMPtr<nsIRDFDataSource> ds;
        nsCAutoString uristrC;
        uri->GetSpec(uristrC);

        rv = gRDFService->GetDataSource(uristrC.get(), getter_AddRefs(ds));

        if (NS_FAILED(rv)) {
            // This is only a warning because the data source may not
            // be accessible for any number of reasons, including
            // security, a bad URL, etc.
  #ifdef DEBUG
            nsCAutoString msg;
            msg.Append("unable to load datasource '");
            msg.Append(uristrC);
            msg.Append('\'');
            NS_WARNING(msg.get());
  #endif
            continue;
        }

        compDB->AddDataSource(ds);
    }


    // check if we were given an inference engine type
    nsAutoString infer;
    nsCOMPtr<nsIRDFDataSource> db;
    root->GetAttr(kNameSpaceID_None, nsGkAtoms::infer, infer);
    if (!infer.IsEmpty()) {
        nsCString inferCID(NS_RDF_INFER_DATASOURCE_CONTRACTID_PREFIX);
        AppendUTF16toUTF8(infer, inferCID);
        nsCOMPtr<nsIRDFInferDataSource> inferDB =
            do_CreateInstance(inferCID.get());

        if (inferDB) {
            inferDB->SetBaseDataSource(compDB);
            db = do_QueryInterface(inferDB);
        }
        else {
            NS_WARNING("failed to construct inference engine specified on template");
        }
    }

    if (!db)
        db = compDB;

    return CallQueryInterface(db, aResult);
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::InitializeForBuilding(nsISupports* aDatasource,
                                                      nsIXULTemplateBuilder* aBuilder,
                                                      nsIDOMNode* aRootNode)
{
    if (!mQueryProcessorRDFInited) {
        nsresult rv = InitGlobals();
        if (NS_FAILED(rv))
            return rv;

        if (!mMemoryElementToResultMap.IsInitialized())
            mMemoryElementToResultMap.Init();
        if (!mBindingDependencies.IsInitialized())
            mBindingDependencies.Init();
        if (!mRuleToBindingsMap.IsInitialized())
            mRuleToBindingsMap.Init();

        mQueryProcessorRDFInited = true;
    }

    // don't do anything if generation has already been done
    if (mGenerationStarted)
        return NS_ERROR_UNEXPECTED;

    mDB = do_QueryInterface(aDatasource);
    mBuilder = aBuilder;

    ComputeContainmentProperties(aRootNode);

    // Add ourselves as a datasource observer
    if (mDB)
        mDB->AddObserver(this);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::Done()
{
    if (!mQueryProcessorRDFInited)
        return NS_OK;

    if (mDB)
        mDB->RemoveObserver(this);

    mDB = nsnull;
    mBuilder = nsnull;
    mRefVariable = nsnull;
    mLastRef = nsnull;

    mGenerationStarted = false;
    mUpdateBatchNest = 0;

    mContainmentProperties.Clear();

    for (ReteNodeSet::Iterator node = mAllTests.First();
         node != mAllTests.Last(); ++node)
        delete *node;

    mAllTests.Clear();
    mRDFTests.Clear();
    mQueries.Clear();

    mSimpleRuleMemberTest = nsnull;

    mBindingDependencies.Clear();

    mMemoryElementToResultMap.Clear();

    mRuleToBindingsMap.Clear();

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::CompileQuery(nsIXULTemplateBuilder* aBuilder,
                                             nsIDOMNode* aQueryNode,
                                             nsIAtom* aRefVariable,
                                             nsIAtom* aMemberVariable,
                                             nsISupports** _retval)
{
    nsRefPtr<nsRDFQuery> query = new nsRDFQuery(this);
    if (!query)
        return NS_ERROR_OUT_OF_MEMORY;

    query->mRefVariable = aRefVariable;
    if (!mRefVariable)
      mRefVariable = aRefVariable;

    if (!aMemberVariable)
        query->mMemberVariable = do_GetAtom("?");
    else
        query->mMemberVariable = aMemberVariable;

    nsresult rv;
    TestNode *lastnode = nsnull;

    nsCOMPtr<nsIContent> content = do_QueryInterface(aQueryNode);

    if (content->NodeInfo()->Equals(nsGkAtoms::_template, kNameSpaceID_XUL)) {
        // simplified syntax with no rules

        query->SetSimple();
        NS_ASSERTION(!mSimpleRuleMemberTest,
                     "CompileQuery called twice with the same template");
        if (!mSimpleRuleMemberTest)
            rv = CompileSimpleQuery(query, content, &lastnode);
        else
            rv = NS_ERROR_FAILURE;
    }
    else if (content->NodeInfo()->Equals(nsGkAtoms::rule, kNameSpaceID_XUL)) {
        // simplified syntax with at least one rule
        query->SetSimple();
        rv = CompileSimpleQuery(query, content, &lastnode);
    }
    else {
        rv = CompileExtendedQuery(query, content, &lastnode);
    }

    if (NS_FAILED(rv))
        return rv;

    query->SetQueryNode(aQueryNode);

    nsInstantiationNode* instnode = new nsInstantiationNode(this, query);
    if (!instnode)
        return NS_ERROR_OUT_OF_MEMORY;

    // this and other functions always add nodes to mAllTests first. That
    // way if something fails, the node will just sit harmlessly in mAllTests
    // where it can be deleted later. 
    rv = mAllTests.Add(instnode);
    if (NS_FAILED(rv)) {
        delete instnode;
        return rv;
    }

    rv = lastnode->AddChild(instnode);
    if (NS_FAILED(rv))
        return rv;

    rv = mQueries.AppendObject(query);
    if (NS_FAILED(rv))
        return rv;

    *_retval = query;
    NS_ADDREF(*_retval);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::GenerateResults(nsISupports* aDatasource,
                                                nsIXULTemplateResult* aRef,
                                                nsISupports* aQuery,
                                                nsISimpleEnumerator** aResults)
{
    nsCOMPtr<nsITemplateRDFQuery> rdfquery = do_QueryInterface(aQuery);
    if (! rdfquery)
        return NS_ERROR_INVALID_ARG;

    mGenerationStarted = true;

    // should be safe to cast here since the query is a
    // non-scriptable nsITemplateRDFQuery
    nsRDFQuery* query = static_cast<nsRDFQuery *>(aQuery);

    *aResults = nsnull;

    nsCOMPtr<nsISimpleEnumerator> results;

    if (aRef) {
        // make sure that cached results were generated for this ref, and if not,
        // regenerate them. Otherwise, things will go wrong for templates bound to
        // an HTML element as they are not generated lazily.
        if (aRef == mLastRef) {
            query->UseCachedResults(getter_AddRefs(results));
        }
        else {
            // clear the cached results
            PRInt32 count = mQueries.Count();
            for (PRInt32 r = 0; r < count; r++) {
                mQueries[r]->ClearCachedResults();
            }
        }

        if (! results) {
            if (! query->mRefVariable)
                query->mRefVariable = do_GetAtom("?uri");

            nsCOMPtr<nsIRDFResource> refResource;
            aRef->GetResource(getter_AddRefs(refResource));
            if (! refResource)
                return NS_ERROR_FAILURE;

            // Propagate the assignments through the network
            TestNode* root = query->GetRoot();

            if (query->IsSimple() && mSimpleRuleMemberTest) {
                // get the top node in the simple rule tree
                root = mSimpleRuleMemberTest->GetParent();
                mLastRef = aRef;
            }

#ifdef PR_LOGGING
            if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
                nsAutoString id;
                aRef->GetId(id);

                nsAutoString rvar;
                query->mRefVariable->ToString(rvar);
                nsAutoString mvar;
                query->mMemberVariable->ToString(mvar);

                PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
                       ("QueryProcessor::GenerateResults using ref %s and vars [ ref: %s  member: %s]",
                       NS_ConvertUTF16toUTF8(id).get(),
                       NS_ConvertUTF16toUTF8(rvar).get(),
                       NS_ConvertUTF16toUTF8(mvar).get()));
            }
#endif

            if (root) {
                // the seed is the initial instantiation, which has a single
                // assignment holding the reference point
                Instantiation seed;
                seed.AddAssignment(query->mRefVariable, refResource);

                InstantiationSet* instantiations = new InstantiationSet();
                if (!instantiations)
                    return NS_ERROR_OUT_OF_MEMORY;
                instantiations->Append(seed);

                // if the propagation caused a match, then the results will be
                // cached in the query, retrieved below by calling
                // UseCachedResults. The matching result set owns the
                // instantiations and will delete them when results have been
                // iterated over. If the propagation did not match, the
                // instantiations need to be deleted.
                bool owned = false;
                nsresult rv = root->Propagate(*instantiations, false, owned);
                if (! owned)
                    delete instantiations;
                if (NS_FAILED(rv))
                    return rv;

                query->UseCachedResults(getter_AddRefs(results));
            }
        }
    }

    if (! results) {
        // no results were found so create an empty set
        results = new nsXULTemplateResultSetRDF(this, query, nsnull);
        if (! results)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    results.swap(*aResults);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::AddBinding(nsIDOMNode* aRuleNode,
                                           nsIAtom* aVar,
                                           nsIAtom* aRef,
                                           const nsAString& aExpr)
{
    // add a <binding> to a rule. When a result is matched, the bindings are
    // examined to add additional variable assignments

    // bindings can't be added once result generation has started, otherwise
    // the array sizes will get out of sync
    if (mGenerationStarted)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIRDFResource> property;
    nsresult rv = gRDFService->GetUnicodeResource(aExpr, getter_AddRefs(property));
    if (NS_FAILED(rv))
        return rv;

    nsRefPtr<RDFBindingSet> bindings = mRuleToBindingsMap.GetWeak(aRuleNode);
    if (!bindings) {
        bindings = new RDFBindingSet();
        mRuleToBindingsMap.Put(aRuleNode, bindings);
    }

    return bindings->AddBinding(aVar, aRef, property);
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::TranslateRef(nsISupports* aDatasource,
                                                           const nsAString& aRefString,
                                                           nsIXULTemplateResult** aRef)
{
    // make sure the RDF service is set up
    nsresult rv = InitGlobals();
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIRDFResource> uri;
    gRDFService->GetUnicodeResource(aRefString, getter_AddRefs(uri));

    nsXULTemplateResultRDF* refresult = new nsXULTemplateResultRDF(uri);
    if (! refresult)
        return NS_ERROR_OUT_OF_MEMORY;

    *aRef = refresult;
    NS_ADDREF(*aRef);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::CompareResults(nsIXULTemplateResult* aLeft,
                                               nsIXULTemplateResult* aRight,
                                               nsIAtom* aVar,
                                               PRUint32 aSortHints,
                                               PRInt32* aResult)
{
    NS_ENSURE_ARG_POINTER(aLeft);
    NS_ENSURE_ARG_POINTER(aRight);

    *aResult = 0;

    // for natural order sorting, use the index in the RDF container for the
    // order. If there is no container, just sort them arbitrarily.
    if (!aVar) {
        // if a result has a negative index, just sort it first
        PRInt32 leftindex = GetContainerIndexOf(aLeft);
        PRInt32 rightindex = GetContainerIndexOf(aRight);
        *aResult = leftindex == rightindex ? 0 :
                   leftindex > rightindex ? 1 :
                   -1;
        return NS_OK;
    }

    nsDependentAtomString sortkey(aVar);

    nsCOMPtr<nsISupports> leftNode, rightNode;

    if (!sortkey.IsEmpty() && sortkey[0] != '?' &&
        !StringBeginsWith(sortkey, NS_LITERAL_STRING("rdf:")) &&
        mDB) {
        // if the sort key is not a template variable, it should be an RDF
        // predicate. Get the targets and compare those instead.
        nsCOMPtr<nsIRDFResource> predicate;
        nsresult rv = gRDFService->GetUnicodeResource(sortkey, getter_AddRefs(predicate));
        NS_ENSURE_SUCCESS(rv, rv);

        // create a predicate with '?sort=true' appended. This special
        // predicate may be used to have a different sort value than the
        // displayed value
        sortkey.AppendLiteral("?sort=true");

        nsCOMPtr<nsIRDFResource> sortPredicate;
        rv = gRDFService->GetUnicodeResource(sortkey, getter_AddRefs(sortPredicate));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = GetSortValue(aLeft, predicate, sortPredicate, getter_AddRefs(leftNode));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = GetSortValue(aRight, predicate, sortPredicate, getter_AddRefs(rightNode));
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        // get the values for the sort key from the results
        aLeft->GetBindingObjectFor(aVar, getter_AddRefs(leftNode));
        aRight->GetBindingObjectFor(aVar, getter_AddRefs(rightNode));
    }

    {
        // Literals?
        nsCOMPtr<nsIRDFLiteral> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFLiteral> r = do_QueryInterface(rightNode);
            if (r) {
                const PRUnichar *lstr, *rstr;
                l->GetValueConst(&lstr);
                r->GetValueConst(&rstr);

                *aResult = XULSortServiceImpl::CompareValues(
                               nsDependentString(lstr),
                               nsDependentString(rstr), aSortHints);
            }

            return NS_OK;
        }
    }

    {
        // Dates?
        nsCOMPtr<nsIRDFDate> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFDate> r = do_QueryInterface(rightNode);
            if (r) {
                PRTime ldate, rdate;
                l->GetValue(&ldate);
                r->GetValue(&rdate);

                PRInt64 delta;
                LL_SUB(delta, ldate, rdate);

                if (LL_IS_ZERO(delta))
                    *aResult = 0;
                else if (LL_GE_ZERO(delta))
                    *aResult = 1;
                else
                    *aResult = -1;
            }

            return NS_OK;
        }
    }

    {
        // Integers?
        nsCOMPtr<nsIRDFInt> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFInt> r = do_QueryInterface(rightNode);
            if (r) {
                PRInt32 lval, rval;
                l->GetValue(&lval);
                r->GetValue(&rval);

                *aResult = lval - rval;
            }

            return NS_OK;
        }
    }

    nsICollation* collation = nsXULContentUtils::GetCollation();
    if (collation) {
        // Blobs? (We can only compare these reasonably if we have a
        // collation object.)
        nsCOMPtr<nsIRDFBlob> l = do_QueryInterface(leftNode);
        if (l) {
            nsCOMPtr<nsIRDFBlob> r = do_QueryInterface(rightNode);
            if (r) {
                const PRUint8 *lval, *rval;
                PRInt32 llen, rlen;
                l->GetValue(&lval);
                l->GetLength(&llen);
                r->GetValue(&rval);
                r->GetLength(&rlen);
                
                collation->CompareRawSortKey(lval, llen, rval, rlen, aResult);
            }
        }
    }

    // if the results are none of the above, just pretend that they are equal
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIRDFObserver interface
//


NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::OnAssert(nsIRDFDataSource* aDataSource,
                                         nsIRDFResource* aSource,
                                         nsIRDFResource* aProperty,
                                         nsIRDFNode* aTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return(NS_OK);

    if (! mBuilder)
        return NS_OK;

    LOG("onassert", aSource, aProperty, aTarget);

    Propagate(aSource, aProperty, aTarget);
    SynchronizeAll(aSource, aProperty, nsnull, aTarget);
    return NS_OK;
}



NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::OnUnassert(nsIRDFDataSource* aDataSource,
                                           nsIRDFResource* aSource,
                                           nsIRDFResource* aProperty,
                                           nsIRDFNode* aTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return NS_OK;

    if (! mBuilder)
        return NS_OK;

    LOG("onunassert", aSource, aProperty, aTarget);

    Retract(aSource, aProperty, aTarget);
    SynchronizeAll(aSource, aProperty, aTarget, nsnull);
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::OnChange(nsIRDFDataSource* aDataSource,
                                         nsIRDFResource* aSource,
                                         nsIRDFResource* aProperty,
                                         nsIRDFNode* aOldTarget,
                                         nsIRDFNode* aNewTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return NS_OK;

    if (! mBuilder)
        return NS_OK;

    LOG("onchange", aSource, aProperty, aNewTarget);

    if (aOldTarget) {
        // Pull any old results that were relying on aOldTarget
        Retract(aSource, aProperty, aOldTarget);
    }

    if (aNewTarget) {
        // Fire any new results that are activated by aNewTarget
        Propagate(aSource, aProperty, aNewTarget);
    }

    // Synchronize any of the content model that may have changed.
    SynchronizeAll(aSource, aProperty, aOldTarget, aNewTarget);
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::OnMove(nsIRDFDataSource* aDataSource,
                                       nsIRDFResource* aOldSource,
                                       nsIRDFResource* aNewSource,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return NS_OK;

    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::OnBeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
    mUpdateBatchNest++;
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorRDF::OnEndUpdateBatch(nsIRDFDataSource* aDataSource)
{
    NS_ASSERTION(mUpdateBatchNest > 0, "badly nested update batch");
    if (--mUpdateBatchNest <= 0) {
        mUpdateBatchNest = 0;

        if (mBuilder)
            mBuilder->Rebuild();
    }

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::Propagate(nsIRDFResource* aSource,
                                          nsIRDFResource* aProperty,
                                          nsIRDFNode* aTarget)
{
    // When a new assertion is added to the graph, determine any new matches
    // that must be added to the template builder. First, iterate through all
    // the RDF tests (<member> and <triple> tests), and find the topmost test
    // that would be affected by the new assertion.
    nsresult rv;

    ReteNodeSet livenodes;

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* sourceStr;
        aSource->GetValueConst(&sourceStr);
        const char* propertyStr;
        aProperty->GetValueConst(&propertyStr);
        nsAutoString targetStr;
        nsXULContentUtils::GetTextForNode(aTarget, targetStr);

        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("nsXULTemplateQueryProcessorRDF::Propagate: [%s] -> [%s] -> [%s]\n",
               sourceStr, propertyStr, NS_ConvertUTF16toUTF8(targetStr).get()));
    }
#endif

    {
        ReteNodeSet::Iterator last = mRDFTests.Last();
        for (ReteNodeSet::Iterator i = mRDFTests.First(); i != last; ++i) {
            nsRDFTestNode* rdftestnode = static_cast<nsRDFTestNode*>(*i);

            Instantiation seed;
            if (rdftestnode->CanPropagate(aSource, aProperty, aTarget, seed)) {
                rv = livenodes.Add(rdftestnode);
                if (NS_FAILED(rv))
                    return rv;
            }
        }
    }

    // Now, we'll go through each, and any that aren't dominated by
    // another live node will be used to propagate the assertion
    // through the rule network
    {
        ReteNodeSet::Iterator last = livenodes.Last();
        for (ReteNodeSet::Iterator i = livenodes.First(); i != last; ++i) {
            nsRDFTestNode* rdftestnode = static_cast<nsRDFTestNode*>(*i);

            // What happens here is we create an instantiation as if we were
            // at the found test in the rule network. For example, if the
            // found test was a member test (parent => child), the parent
            // and child variables are assigned the values provided by the new
            // RDF assertion in the graph. The Constrain call is used to go
            // up to earlier RDF tests, filling in variables as it goes.
            // Constrain will eventually get up to the top node, an
            // nsContentTestNode, which takes the value of the reference
            // variable and calls the template builder to see if a result has
            // been generated already for the reference value. If it hasn't,
            // the new assertion couldn't cause a new match. If the result
            // exists, call Propagate to continue to the later RDF tests to
            // fill in the rest of the variable assignments.

            // Bogus, to get the seed instantiation
            Instantiation seed;
            rdftestnode->CanPropagate(aSource, aProperty, aTarget, seed);

            InstantiationSet* instantiations = new InstantiationSet();
            if (!instantiations)
                return NS_ERROR_OUT_OF_MEMORY;
            instantiations->Append(seed);

            rv = rdftestnode->Constrain(*instantiations);
            if (NS_FAILED(rv)) {
                delete instantiations;
                return rv;
            }

            bool owned = false;
            if (!instantiations->Empty())
                rv = rdftestnode->Propagate(*instantiations, true, owned);

            // owned should always be false in update mode, but check just
            // to be sure
            if (!owned)
                delete instantiations;
            if (NS_FAILED(rv))
                return rv;
        }
    }

    return NS_OK;
}


nsresult
nsXULTemplateQueryProcessorRDF::Retract(nsIRDFResource* aSource,
                                        nsIRDFResource* aProperty,
                                        nsIRDFNode* aTarget)
{

#ifdef PR_LOGGING
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        const char* sourceStr;
        aSource->GetValueConst(&sourceStr);
        const char* propertyStr;
        aProperty->GetValueConst(&propertyStr);
        nsAutoString targetStr;
        nsXULContentUtils::GetTextForNode(aTarget, targetStr);

        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("nsXULTemplateQueryProcessorRDF::Retract: [%s] -> [%s] -> [%s]\n",
               sourceStr, propertyStr, NS_ConvertUTF16toUTF8(targetStr).get()));
    }
#endif

    // Retract any currently active rules that will no longer be matched.
    ReteNodeSet::ConstIterator lastnode = mRDFTests.Last();
    for (ReteNodeSet::ConstIterator node = mRDFTests.First(); node != lastnode; ++node) {
        const nsRDFTestNode* rdftestnode = static_cast<const nsRDFTestNode*>(*node);

        rdftestnode->Retract(aSource, aProperty, aTarget);

        // Now fire any newly revealed rules
        // XXXwaterson yo. write me.
        // The intent here is to handle any rules that might be
        // "revealed" by the removal of an assertion from the datasource.
        // Waterson doesn't think we support negated conditions in a rule.
        // Nor is he sure that this is currently useful.
    }

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::SynchronizeAll(nsIRDFResource* aSource,
                                               nsIRDFResource* aProperty,
                                               nsIRDFNode* aOldTarget,
                                               nsIRDFNode* aNewTarget)
{
    // Update each match that contains <aSource, aProperty, aOldTarget>.

    // Get all the matches whose assignments are currently supported
    // by aSource and aProperty: we'll need to recompute them.
    nsCOMArray<nsXULTemplateResultRDF>* results;
    if (!mBindingDependencies.Get(aSource, &results) || !mBuilder)
        return NS_OK;

    PRUint32 length = results->Count();

    for (PRUint32 r = 0; r < length; r++) {
        nsXULTemplateResultRDF* result = (*results)[r];
        if (result) {
            // synchronize the result's bindings and then update the builder
            // so that content can be updated
            if (result->SyncAssignments(aSource, aProperty, aNewTarget)) {
                nsITemplateRDFQuery* query = result->Query();
                if (query) {
                    nsCOMPtr<nsIDOMNode> querynode;
                    query->GetQueryNode(getter_AddRefs(querynode));

                    mBuilder->ResultBindingChanged(result);
                }
            }
        }
    }

    return NS_OK;
}

#ifdef PR_LOGGING
nsresult
nsXULTemplateQueryProcessorRDF::Log(const char* aOperation,
                                    nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget)
{
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsresult rv;

        const char* sourceStr;
        rv = aSource->GetValueConst(&sourceStr);
        if (NS_FAILED(rv))
            return rv;

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] %8s [%s]--", this, aOperation, sourceStr));

        const char* propertyStr;
        rv = aProperty->GetValueConst(&propertyStr);
        if (NS_FAILED(rv))
            return rv;

        nsAutoString targetStr;
        rv = nsXULContentUtils::GetTextForNode(aTarget, targetStr);
        if (NS_FAILED(rv))
            return rv;

        nsCAutoString targetstrC;
        targetstrC.AssignWithConversion(targetStr);
        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("                        --[%s]-->[%s]",
                propertyStr,
                targetstrC.get()));
    }
    return NS_OK;
}
#endif

nsresult
nsXULTemplateQueryProcessorRDF::CheckContainer(nsIRDFResource* aResource,
                                               bool* aIsContainer)
{
    NS_ENSURE_ARG_POINTER(aIsContainer);
    NS_ENSURE_STATE(mDB);

    // We have to look at all of the arcs extending out of the
    // resource: if any of them are that "containment" property, then
    // we know we'll have children.
    bool isContainer = false;

    for (nsResourceSet::ConstIterator property = mContainmentProperties.First();
         property != mContainmentProperties.Last();
         property++) {
        bool hasArc = false;
        mDB->HasArcOut(aResource, *property, &hasArc);

        if (hasArc) {
            // Well, it's a container...
            isContainer = true;
            break;
        }
    }

    // If we get here, and we're still not sure if it's a container,
    // then see if it's an RDF container
    if (! isContainer) {
        gRDFContainerUtils->IsContainer(mDB, aResource, &isContainer);
    }

    *aIsContainer = isContainer;

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::CheckEmpty(nsIRDFResource* aResource,
                                           bool* aIsEmpty)
{
    NS_ENSURE_STATE(mDB);
    *aIsEmpty = true;

    for (nsResourceSet::ConstIterator property = mContainmentProperties.First();
         property != mContainmentProperties.Last();
         property++) {

        nsCOMPtr<nsIRDFNode> dummy;
        mDB->GetTarget(aResource, *property, true, getter_AddRefs(dummy));

        if (dummy) {
            *aIsEmpty = false;
            break;
        }
    }

    if (*aIsEmpty){
        return nsXULTemplateQueryProcessorRDF::gRDFContainerUtils->
                   IsEmpty(mDB, aResource, aIsEmpty);
    }

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::CheckIsSeparator(nsIRDFResource* aResource,
                                                 bool* aIsSeparator)
{
    NS_ENSURE_STATE(mDB);
    return mDB->HasAssertion(aResource, kRDF_type, kNC_BookmarkSeparator,
                             true, aIsSeparator);
}

//----------------------------------------------------------------------

nsresult
nsXULTemplateQueryProcessorRDF::ComputeContainmentProperties(nsIDOMNode* aRootNode)
{
    // The 'containment' attribute on the root node is a
    // whitespace-separated list that tells us which properties we
    // should use to test for containment.
    nsresult rv;

    mContainmentProperties.Clear();

    nsCOMPtr<nsIContent> content = do_QueryInterface(aRootNode);

    nsAutoString containment;
    content->GetAttr(kNameSpaceID_None, nsGkAtoms::containment, containment);

    PRUint32 len = containment.Length();
    PRUint32 offset = 0;
    while (offset < len) {
        while (offset < len && nsCRT::IsAsciiSpace(containment[offset]))
            ++offset;

        if (offset >= len)
            break;

        PRUint32 end = offset;
        while (end < len && !nsCRT::IsAsciiSpace(containment[end]))
            ++end;

        nsAutoString propertyStr;
        containment.Mid(propertyStr, offset, end - offset);

        nsCOMPtr<nsIRDFResource> property;
        rv = gRDFService->GetUnicodeResource(propertyStr, getter_AddRefs(property));
        if (NS_FAILED(rv))
            return rv;

        rv = mContainmentProperties.Add(property);
        if (NS_FAILED(rv))
            return rv;

        offset = end;
    }

#define TREE_PROPERTY_HACK 1
#if defined(TREE_PROPERTY_HACK)
    if (! len) {
        // Some ever-present membership tests.
        mContainmentProperties.Add(nsXULContentUtils::NC_child);
        mContainmentProperties.Add(nsXULContentUtils::NC_Folder);
    }
#endif

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::CompileExtendedQuery(nsRDFQuery* aQuery,
                                                     nsIContent* aConditions,
                                                     TestNode** aLastNode)
{
    // Compile an extended query's children

    nsContentTestNode* idnode =
        new nsContentTestNode(this, aQuery->mRefVariable);
    if (! idnode)
        return NS_ERROR_OUT_OF_MEMORY;

    aQuery->SetRoot(idnode);
    nsresult rv = mAllTests.Add(idnode);
    if (NS_FAILED(rv)) {
        delete idnode;
        return rv;
    }

    TestNode* prevnode = idnode;

    for (nsIContent* condition = aConditions->GetFirstChild();
         condition;
         condition = condition->GetNextSibling()) {

        // the <content> condition should always be the first child
        if (condition->Tag() == nsGkAtoms::content) {
            if (condition != aConditions->GetFirstChild()) {
                nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_CONTENT_NOT_FIRST);
                continue;
            }

            // check for <content tag='tag'/> which indicates that matches
            // should only be generated for items inside content with that tag
            nsAutoString tagstr;
            condition->GetAttr(kNameSpaceID_None, nsGkAtoms::tag, tagstr);

            nsCOMPtr<nsIAtom> tag;
            if (! tagstr.IsEmpty()) {
                tag = do_GetAtom(tagstr);
            }

            nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(condition->GetDocument());
            if (! doc)
                return NS_ERROR_FAILURE;

            idnode->SetTag(tag, doc);
            continue;
        }

        TestNode* testnode = nsnull;
        nsresult rv = CompileQueryChild(condition->Tag(), aQuery, condition,
                                        prevnode, &testnode);
        if (NS_FAILED(rv))
            return rv;

        if (testnode) {
            rv = prevnode->AddChild(testnode);
            if (NS_FAILED(rv))
                return rv;

            prevnode = testnode;
        }
    }

    *aLastNode = prevnode;

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::CompileQueryChild(nsIAtom* aTag,
                                                  nsRDFQuery* aQuery,
                                                  nsIContent* aCondition,
                                                  TestNode* aParentNode,
                                                  TestNode** aResult)
{
    nsresult rv;

    if (aTag == nsGkAtoms::triple) {
        rv = CompileTripleCondition(aQuery, aCondition, aParentNode, aResult);
    }
    else if (aTag == nsGkAtoms::member) {
        rv = CompileMemberCondition(aQuery, aCondition, aParentNode, aResult);
    }
    else {
#ifdef PR_LOGGING
        nsAutoString tagstr;
        aTag->ToString(tagstr);

        nsCAutoString tagstrC;
        tagstrC.AssignWithConversion(tagstr);
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] unrecognized condition test <%s>",
                this, tagstrC.get()));
#endif

        rv = NS_OK;
    }

    return rv;
}

nsresult
nsXULTemplateQueryProcessorRDF::ParseLiteral(const nsString& aParseType, 
                                             const nsString& aValue,
                                             nsIRDFNode** aResult)
{
    nsresult rv = NS_OK;
    *aResult = nsnull;

    if (aParseType.EqualsLiteral(PARSE_TYPE_INTEGER)) {
        nsCOMPtr<nsIRDFInt> intLiteral;
        PRInt32 errorCode;
        PRInt32 intValue = aValue.ToInteger(&errorCode);
        if (NS_FAILED(errorCode))
            return NS_ERROR_FAILURE;
        rv = gRDFService->GetIntLiteral(intValue, getter_AddRefs(intLiteral));
        if (NS_FAILED(rv)) 
            return rv;
        rv = CallQueryInterface(intLiteral, aResult);
    }
    else {
        nsCOMPtr<nsIRDFLiteral> literal;
        rv = gRDFService->GetLiteral(aValue.get(), getter_AddRefs(literal));
        if (NS_FAILED(rv)) 
            return rv;
        rv = CallQueryInterface(literal, aResult);
    }
    return rv;
}

nsresult
nsXULTemplateQueryProcessorRDF::CompileTripleCondition(nsRDFQuery* aQuery,
                                                       nsIContent* aCondition,
                                                       TestNode* aParentNode,
                                                       TestNode** aResult)
{
    // Compile a <triple> condition, which must be of the form:
    //
    //   <triple subject="?var1|resource"
    //           predicate="resource"
    //           object="?var2|resource|literal" />
    //
    // XXXwaterson Some day it would be cool to allow the 'predicate'
    // to be bound to a variable.

    // subject
    nsAutoString subject;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::subject, subject);

    nsCOMPtr<nsIAtom> svar;
    nsCOMPtr<nsIRDFResource> sres;
    if (subject.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_TRIPLE_BAD_SUBJECT);
        return NS_OK;
    }
    if (subject[0] == PRUnichar('?'))
        svar = do_GetAtom(subject);
    else
        gRDFService->GetUnicodeResource(subject, getter_AddRefs(sres));

    // predicate
    nsAutoString predicate;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::predicate, predicate);

    nsCOMPtr<nsIRDFResource> pres;
    if (predicate.IsEmpty() || predicate[0] == PRUnichar('?')) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_TRIPLE_BAD_PREDICATE);
        return NS_OK;
    }
    gRDFService->GetUnicodeResource(predicate, getter_AddRefs(pres));

    // object
    nsAutoString object;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::object, object);

    nsCOMPtr<nsIAtom> ovar;
    nsCOMPtr<nsIRDFNode> onode;
    if (object.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_TRIPLE_BAD_OBJECT);
        return NS_OK;
    }

    if (object[0] == PRUnichar('?')) {
        ovar = do_GetAtom(object);
    }
    else if (object.FindChar(':') != -1) { // XXXwaterson evil.
        // treat as resource
        nsCOMPtr<nsIRDFResource> resource;
        gRDFService->GetUnicodeResource(object, getter_AddRefs(resource));
        onode = do_QueryInterface(resource);
    }
    else {
        nsAutoString parseType;
        aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::parsetype, parseType);
        nsresult rv = ParseLiteral(parseType, object, getter_AddRefs(onode));
        if (NS_FAILED(rv))
            return rv;
    }

    nsRDFPropertyTestNode* testnode = nsnull;

    if (svar && ovar) {
        testnode = new nsRDFPropertyTestNode(aParentNode, this, svar, pres, ovar);
    }
    else if (svar) {
        testnode = new nsRDFPropertyTestNode(aParentNode, this, svar, pres, onode);
    }
    else if (ovar) {
        testnode = new nsRDFPropertyTestNode(aParentNode, this, sres, pres, ovar);
    }
    else {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_TRIPLE_NO_VAR);
        return NS_OK;
    }

    if (! testnode)
        return NS_ERROR_OUT_OF_MEMORY;

    // add testnode to mAllTests first. If adding to mRDFTests fails, just
    // leave it in the list so that it can be deleted later.
    nsresult rv = mAllTests.Add(testnode);
    if (NS_FAILED(rv)) {
        delete testnode;
        return rv;
    }

    rv = mRDFTests.Add(testnode);
    if (NS_FAILED(rv))
        return rv;

    *aResult = testnode;
    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::CompileMemberCondition(nsRDFQuery* aQuery,
                                                       nsIContent* aCondition,
                                                       TestNode* aParentNode,
                                                       TestNode** aResult)
{
    // Compile a <member> condition, which must be of the form:
    //
    //   <member container="?var1" child="?var2" />
    //

    // container
    nsAutoString container;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::container, container);

    if (!container.IsEmpty() && container[0] != PRUnichar('?')) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_MEMBER_NOCONTAINERVAR);
        return NS_OK;
    }

    nsCOMPtr<nsIAtom> containervar = do_GetAtom(container);

    // child
    nsAutoString child;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::child, child);

    if (!child.IsEmpty() && child[0] != PRUnichar('?')) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_MEMBER_NOCHILDVAR);
        return NS_OK;
    }

    nsCOMPtr<nsIAtom> childvar = do_GetAtom(child);

    TestNode* testnode =
        new nsRDFConMemberTestNode(aParentNode,
                                   this,
                                   containervar,
                                   childvar);

    if (! testnode)
        return NS_ERROR_OUT_OF_MEMORY;

    // add testnode to mAllTests first. If adding to mRDFTests fails, just
    // leave it in the list so that it can be deleted later.
    nsresult rv = mAllTests.Add(testnode);
    if (NS_FAILED(rv)) {
        delete testnode;
        return rv;
    }

    rv = mRDFTests.Add(testnode);
    if (NS_FAILED(rv))
        return rv;

    *aResult = testnode;
    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::AddDefaultSimpleRules(nsRDFQuery* aQuery,
                                                      TestNode** aChildNode)
{
    // XXXndeakin should check for tag in query processor instead of builder?
    nsContentTestNode* idnode =
        new nsContentTestNode(this,
                              aQuery->mRefVariable);
    if (! idnode)
        return NS_ERROR_OUT_OF_MEMORY;

    // Create (?container ^member ?member)
    nsRDFConMemberTestNode* membernode =
        new nsRDFConMemberTestNode(idnode,
                                   this,
                                   aQuery->mRefVariable,
                                   aQuery->mMemberVariable);

    if (! membernode) {
        delete idnode;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // add nodes to mAllTests first. If later calls fail, just leave them in
    // the list so that they can be deleted later.
    nsresult rv = mAllTests.Add(idnode);
    if (NS_FAILED(rv)) {
        delete idnode;
        delete membernode;
        return rv;
    }

    rv = mAllTests.Add(membernode);
    if (NS_FAILED(rv)) {
        delete membernode;
        return rv;
    }

    rv = mRDFTests.Add(membernode);
    if (NS_FAILED(rv))
        return rv;

    rv = idnode->AddChild(membernode);
    if (NS_FAILED(rv))
        return rv;

    mSimpleRuleMemberTest = membernode;
    *aChildNode = membernode;

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::CompileSimpleQuery(nsRDFQuery* aQuery,
                                                   nsIContent* aQueryElement,
                                                   TestNode** aLastNode)
{
    // Compile a "simple" (or old-school style) <template> query.
    nsresult rv;

    TestNode* parentNode;

    if (! mSimpleRuleMemberTest) {
        rv = AddDefaultSimpleRules(aQuery, &parentNode);
        if (NS_FAILED(rv))
            return rv;
    }

    bool hasContainerTest = false;

    TestNode* prevnode = mSimpleRuleMemberTest;

    // Add constraints for the LHS
    const nsAttrName* name;
    for (PRUint32 i = 0; (name = aQueryElement->GetAttrNameAt(i)); ++i) {
        // Note: some attributes must be skipped on XUL template query subtree

        // never compare against rdf:property, rdf:instanceOf, {}:id or {}:parsetype attribute
        if (name->Equals(nsGkAtoms::property, kNameSpaceID_RDF) ||
            name->Equals(nsGkAtoms::instanceOf, kNameSpaceID_RDF) ||
            name->Equals(nsGkAtoms::id, kNameSpaceID_None) ||
            name->Equals(nsGkAtoms::parsetype, kNameSpaceID_None)) {
            continue;
        }

        PRInt32 attrNameSpaceID = name->NamespaceID();
        if (attrNameSpaceID == kNameSpaceID_XMLNS)
          continue;
        nsIAtom* attr = name->LocalName();

        nsAutoString value;
        aQueryElement->GetAttr(attrNameSpaceID, attr, value);

        TestNode* testnode = nsnull;

        if (name->Equals(nsGkAtoms::iscontainer, kNameSpaceID_None) ||
            name->Equals(nsGkAtoms::isempty, kNameSpaceID_None)) {
            // Tests about containerhood and emptiness. These can be
            // globbed together, mostly. Check to see if we've already
            // added a container test: we only need one.
            if (hasContainerTest)
                continue;

            nsRDFConInstanceTestNode::Test iscontainer =
                nsRDFConInstanceTestNode::eDontCare;

            static nsIContent::AttrValuesArray strings[] =
              {&nsGkAtoms::_true, &nsGkAtoms::_false, nsnull};
            switch (aQueryElement->FindAttrValueIn(kNameSpaceID_None,
                                                   nsGkAtoms::iscontainer,
                                                   strings, eCaseMatters)) {
                case 0: iscontainer = nsRDFConInstanceTestNode::eTrue; break;
                case 1: iscontainer = nsRDFConInstanceTestNode::eFalse; break;
            }

            nsRDFConInstanceTestNode::Test isempty =
                nsRDFConInstanceTestNode::eDontCare;

            switch (aQueryElement->FindAttrValueIn(kNameSpaceID_None,
                                                   nsGkAtoms::isempty,
                                                   strings, eCaseMatters)) {
                case 0: isempty = nsRDFConInstanceTestNode::eTrue; break;
                case 1: isempty = nsRDFConInstanceTestNode::eFalse; break;
            }

            testnode = new nsRDFConInstanceTestNode(prevnode,
                                                    this,
                                                    aQuery->mMemberVariable,
                                                    iscontainer,
                                                    isempty);

            if (! testnode)
                return NS_ERROR_OUT_OF_MEMORY;

            rv = mAllTests.Add(testnode);
            if (NS_FAILED(rv)) {
                delete testnode;
                return rv;
            }

            rv = mRDFTests.Add(testnode);
            if (NS_FAILED(rv))
                return rv;
        }
        else if (attrNameSpaceID != kNameSpaceID_None || attr != nsGkAtoms::parent) {
            // It's a simple RDF test
            nsCOMPtr<nsIRDFResource> property;
            rv = nsXULContentUtils::GetResource(attrNameSpaceID, attr, getter_AddRefs(property));
            if (NS_FAILED(rv))
                return rv;

            // XXXwaterson this is so manky
            nsCOMPtr<nsIRDFNode> target;
            if (value.FindChar(':') != -1) { // XXXwaterson WRONG WRONG WRONG!
                nsCOMPtr<nsIRDFResource> resource;
                rv = gRDFService->GetUnicodeResource(value, getter_AddRefs(resource));
                if (NS_FAILED(rv))
                    return rv;

                target = do_QueryInterface(resource);
            }
            else {                
              nsAutoString parseType;
              aQueryElement->GetAttr(kNameSpaceID_None, nsGkAtoms::parsetype, parseType);
              rv = ParseLiteral(parseType, value, getter_AddRefs(target));
              if (NS_FAILED(rv))
                  return rv;
            }

            testnode = new nsRDFPropertyTestNode(prevnode, this,
                                                 aQuery->mMemberVariable, property, target);
            if (! testnode)
                return NS_ERROR_OUT_OF_MEMORY;

            rv = mAllTests.Add(testnode);
            if (NS_FAILED(rv)) {
                delete testnode;
                return rv;
            }

            rv = mRDFTests.Add(testnode);
            if (NS_FAILED(rv))
                return rv;
        }

        if (testnode) {
            if (prevnode) {
                rv = prevnode->AddChild(testnode);
                if (NS_FAILED(rv))
                    return rv;
            }                
            else {
                aQuery->SetRoot(testnode);
            }

            prevnode = testnode;
        }
    }

    *aLastNode = prevnode;

    return NS_OK;
}

RDFBindingSet*
nsXULTemplateQueryProcessorRDF::GetBindingsForRule(nsIDOMNode* aRuleNode)
{
    return mRuleToBindingsMap.GetWeak(aRuleNode);
}

nsresult
nsXULTemplateQueryProcessorRDF::AddBindingDependency(nsXULTemplateResultRDF* aResult,
                                                     nsIRDFResource* aResource)
{
    nsCOMArray<nsXULTemplateResultRDF>* arr;
    if (!mBindingDependencies.Get(aResource, &arr)) {
        arr = new nsCOMArray<nsXULTemplateResultRDF>();
        if (!arr)
            return NS_ERROR_OUT_OF_MEMORY;

        mBindingDependencies.Put(aResource, arr);
    }

    PRInt32 index = arr->IndexOf(aResult);
    if (index == -1)
        return arr->AppendObject(aResult);

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::RemoveBindingDependency(nsXULTemplateResultRDF* aResult,
                                                        nsIRDFResource* aResource)
{
    nsCOMArray<nsXULTemplateResultRDF>* arr;
    if (mBindingDependencies.Get(aResource, &arr)) {
        PRInt32 index = arr->IndexOf(aResult);
        if (index >= 0)
            return arr->RemoveObjectAt(index);
    }

    return NS_OK;
}


nsresult
nsXULTemplateQueryProcessorRDF::AddMemoryElements(const Instantiation& aInst,
                                                  nsXULTemplateResultRDF* aResult)
{
    // Add the result to a table indexed by supporting MemoryElement
    MemoryElementSet::ConstIterator last = aInst.mSupport.Last();
    for (MemoryElementSet::ConstIterator element = aInst.mSupport.First();
                                         element != last; ++element) {

        PLHashNumber hash = (element.operator->())->Hash();

        nsCOMArray<nsXULTemplateResultRDF>* arr;
        if (!mMemoryElementToResultMap.Get(hash, &arr)) {
            arr = new nsCOMArray<nsXULTemplateResultRDF>();
            if (!arr)
                return NS_ERROR_OUT_OF_MEMORY;

            mMemoryElementToResultMap.Put(hash, arr);
        }

        // results may be added more than once so they will all get deleted properly
        arr->AppendObject(aResult);
    }

    return NS_OK;
}

nsresult
nsXULTemplateQueryProcessorRDF::RemoveMemoryElements(const Instantiation& aInst,
                                                     nsXULTemplateResultRDF* aResult)
{
    // Remove the results mapped by the supporting MemoryElement
    MemoryElementSet::ConstIterator last = aInst.mSupport.Last();
    for (MemoryElementSet::ConstIterator element = aInst.mSupport.First();
                                         element != last; ++element) {

        PLHashNumber hash = (element.operator->())->Hash();

        nsCOMArray<nsXULTemplateResultRDF>* arr;
        if (mMemoryElementToResultMap.Get(hash, &arr)) {
            PRInt32 index = arr->IndexOf(aResult);
            if (index >= 0)
                arr->RemoveObjectAt(index);

            PRUint32 length = arr->Count();
            if (! length)
                mMemoryElementToResultMap.Remove(hash);
        }
    }

    return NS_OK;
}

void
nsXULTemplateQueryProcessorRDF::RetractElement(const MemoryElement& aMemoryElement)
{
    if (! mBuilder)
        return;

    // when an assertion is removed, look through the memory elements and
    // find results that are associated with them. Those results will need
    // to be removed because they no longer match.
    PLHashNumber hash = aMemoryElement.Hash();

    nsCOMArray<nsXULTemplateResultRDF>* arr;
    if (mMemoryElementToResultMap.Get(hash, &arr)) {
        PRUint32 length = arr->Count();

        for (PRInt32 r = length - 1; r >= 0; r--) {
            nsXULTemplateResultRDF* result = (*arr)[r];
            if (result) {
                // because the memory elements are hashed by an integer,
                // sometimes two different memory elements will have the same
                // hash code. In this case we check the result to make sure
                // and only remove those that refer to that memory element.
                if (result->HasMemoryElement(aMemoryElement)) {
                    nsITemplateRDFQuery* query = result->Query();
                    if (query) {
                        nsCOMPtr<nsIDOMNode> querynode;
                        query->GetQueryNode(getter_AddRefs(querynode));

                        mBuilder->RemoveResult(result);
                    }

                    // a call to RemoveMemoryElements may have removed it
                    if (!mMemoryElementToResultMap.Get(hash, nsnull))
                        return;

                    // the array should have been reduced by one, but check
                    // just to make sure
                    PRUint32 newlength = arr->Count();
                    if (r > (PRInt32)newlength)
                        r = newlength;
                }
            }
        }

        // if there are no items left, remove the memory element from the hashtable
        if (!arr->Count())
            mMemoryElementToResultMap.Remove(hash);
    }
}

PRInt32
nsXULTemplateQueryProcessorRDF::GetContainerIndexOf(nsIXULTemplateResult* aResult)
{
    // get the reference variable and look up the container in the result
    nsCOMPtr<nsISupports> ref;
    nsresult rv = aResult->GetBindingObjectFor(mRefVariable,
                                               getter_AddRefs(ref));
    if (NS_FAILED(rv) || !mDB)
        return -1;

    nsCOMPtr<nsIRDFResource> container = do_QueryInterface(ref);
    if (container) {
        // if the container is an RDF Seq, return the index of the result
        // in the container.
        bool isSequence = false;
        gRDFContainerUtils->IsSeq(mDB, container, &isSequence);
        if (isSequence) {
            nsCOMPtr<nsIRDFResource> resource;
            aResult->GetResource(getter_AddRefs(resource));
            if (resource) {
                PRInt32 index;
                gRDFContainerUtils->IndexOf(mDB, container, resource, &index);
                return index;
            }
        }
    }

    // if the container isn't a Seq, or the result isn't in the container,
    // return -1 indicating no index.
    return -1;
}

nsresult
nsXULTemplateQueryProcessorRDF::GetSortValue(nsIXULTemplateResult* aResult,
                                             nsIRDFResource* aPredicate,
                                             nsIRDFResource* aSortPredicate,
                                             nsISupports** aResultNode)
{
    nsCOMPtr<nsIRDFResource> source;
    nsresult rv = aResult->GetResource(getter_AddRefs(source));
    if (NS_FAILED(rv))
        return rv;
    
    nsCOMPtr<nsIRDFNode> value;
    if (source && mDB) {
        // first check predicate?sort=true so that datasources may use a
        // custom value for sorting
        rv = mDB->GetTarget(source, aSortPredicate, true,
                            getter_AddRefs(value));
        if (NS_FAILED(rv))
            return rv;

        if (!value) {
            rv = mDB->GetTarget(source, aPredicate, true,
                                getter_AddRefs(value));
            if (NS_FAILED(rv))
                return rv;
        }
    }

    *aResultNode = value;
    NS_IF_ADDREF(*aResultNode);
    return NS_OK;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  Builds content from a datasource using the XUL <template> tag.

  TO DO

  . Fix ContentTagTest's location in the network construction

  To turn on logging for this module, set:

    NSPR_LOG_MODULES nsXULTemplateBuilder:5

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsBindingManager.h"
#include "nsIDOMNodeList.h"
#include "nsINameSpaceManager.h"
#include "nsIObserverService.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFInferDataSource.h"
#include "nsIRDFContainerUtils.h"
#include "nsIXULDocument.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIXULBuilderListener.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIMutableArray.h"
#include "nsIURL.h"
#include "nsIXPConnect.h"
#include "nsContentCID.h"
#include "nsRDFCID.h"
#include "nsXULContentUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsXPIDLString.h"
#include "nsWhitespaceTokenizer.h"
#include "nsGkAtoms.h"
#include "nsXULElement.h"
#include "jsapi.h"
#include "prlog.h"
#include "rdf.h"
#include "pldhash.h"
#include "plhash.h"
#include "nsDOMClassInfoID.h"
#include "nsPIDOMWindow.h"
#include "nsIConsoleService.h" 
#include "nsNetUtil.h"
#include "nsXULTemplateBuilder.h"
#include "nsXULTemplateQueryProcessorRDF.h"
#include "nsXULTemplateQueryProcessorXML.h"
#include "nsXULTemplateQueryProcessorStorage.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"

using namespace mozilla::dom;
using namespace mozilla;

//----------------------------------------------------------------------

static NS_DEFINE_CID(kRDFContainerUtilsCID,      NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

//----------------------------------------------------------------------
//
// nsXULTemplateBuilder
//

nsrefcnt                  nsXULTemplateBuilder::gRefCnt = 0;
nsIRDFService*            nsXULTemplateBuilder::gRDFService;
nsIRDFContainerUtils*     nsXULTemplateBuilder::gRDFContainerUtils;
nsIScriptSecurityManager* nsXULTemplateBuilder::gScriptSecurityManager;
nsIPrincipal*             nsXULTemplateBuilder::gSystemPrincipal;
nsIObserverService*       nsXULTemplateBuilder::gObserverService;

#ifdef PR_LOGGING
PRLogModuleInfo* gXULTemplateLog;
#endif

#define NS_QUERY_PROCESSOR_CONTRACTID_PREFIX "@mozilla.org/xul/xul-query-processor;1?name="

//----------------------------------------------------------------------
//
// nsXULTemplateBuilder methods
//

nsXULTemplateBuilder::nsXULTemplateBuilder(void)
    : mQueriesCompiled(false),
      mFlags(0),
      mTop(nullptr),
      mObservedDocument(nullptr)
{
}

static PLDHashOperator
DestroyMatchList(nsISupports* aKey, nsTemplateMatch*& aMatch, void* aContext)
{
    // delete all the matches in the list
    while (aMatch) {
        nsTemplateMatch* next = aMatch->mNext;
        nsTemplateMatch::Destroy(aMatch, true);
        aMatch = next;
    }

    return PL_DHASH_REMOVE;
}

nsXULTemplateBuilder::~nsXULTemplateBuilder(void)
{
    Uninit(true);

    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(gRDFContainerUtils);
        NS_IF_RELEASE(gSystemPrincipal);
        NS_IF_RELEASE(gScriptSecurityManager);
        NS_IF_RELEASE(gObserverService);
    }
}


nsresult
nsXULTemplateBuilder::InitGlobals()
{
    nsresult rv;

    if (gRefCnt++ == 0) {
        // Initialize the global shared reference to the service
        // manager and get some shared resource objects.
        rv = CallGetService(kRDFServiceCID, &gRDFService);
        if (NS_FAILED(rv))
            return rv;

        rv = CallGetService(kRDFContainerUtilsCID, &gRDFContainerUtils);
        if (NS_FAILED(rv))
            return rv;

        rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                            &gScriptSecurityManager);
        if (NS_FAILED(rv))
            return rv;

        rv = gScriptSecurityManager->GetSystemPrincipal(&gSystemPrincipal);
        if (NS_FAILED(rv))
            return rv;

        rv = CallGetService(NS_OBSERVERSERVICE_CONTRACTID, &gObserverService);
        if (NS_FAILED(rv))
            return rv;
    }

#ifdef PR_LOGGING
    if (! gXULTemplateLog)
        gXULTemplateLog = PR_NewLogModule("nsXULTemplateBuilder");
#endif

    if (!mMatchMap.IsInitialized())
        mMatchMap.Init();

    return NS_OK;
}

void
nsXULTemplateBuilder::CleanUp(bool aIsFinal)
{
    for (int32_t q = mQuerySets.Length() - 1; q >= 0; q--) {
        nsTemplateQuerySet* qs = mQuerySets[q];
        delete qs;
    }

    mQuerySets.Clear();

    mMatchMap.Enumerate(DestroyMatchList, nullptr);

    // Setting mQueryProcessor to null will close connections. This would be
    // handled by the cycle collector, but we want to close them earlier.
    if (aIsFinal)
        mQueryProcessor = nullptr;
}

void
nsXULTemplateBuilder::Uninit(bool aIsFinal)
{
    if (mObservedDocument && aIsFinal) {
        gObserverService->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
        gObserverService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        mObservedDocument->RemoveObserver(this);
        mObservedDocument = nullptr;
    }

    if (mQueryProcessor)
        mQueryProcessor->Done();

    CleanUp(aIsFinal);

    mRootResult = nullptr;
    mRefVariable = nullptr;
    mMemberVariable = nullptr;

    mQueriesCompiled = false;
}

static PLDHashOperator
TraverseMatchList(nsISupports* aKey, nsTemplateMatch* aMatch, void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        static_cast<nsCycleCollectionTraversalCallback*>(aContext);

    cb->NoteXPCOMChild(aKey);
    nsTemplateMatch* match = aMatch;
    while (match) {
        cb->NoteXPCOMChild(match->GetContainer());
        cb->NoteXPCOMChild(match->mResult);
        match = match->mNext;
    }

    return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDataSource)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mDB)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mCompDB)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoot)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mRootResult)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mListeners)
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mQueryProcessor)
    if (tmp->mMatchMap.IsInitialized()) {
      tmp->mMatchMap.Enumerate(DestroyMatchList, nullptr);
    }
    for (uint32_t i = 0; i < tmp->mQuerySets.Length(); ++i) {
        nsTemplateQuerySet* qs = tmp->mQuerySets[i];
        delete qs;
    }
    tmp->mQuerySets.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULTemplateBuilder)
    if (tmp->mObservedDocument && !cb.WantAllTraces()) {
        // The global observer service holds us alive.
        return NS_SUCCESS_INTERRUPTED_TRAVERSE;
    }

    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDataSource)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDB)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCompDB)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRootResult)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mQueryProcessor)
    if (tmp->mMatchMap.IsInitialized())
        tmp->mMatchMap.EnumerateRead(TraverseMatchList, &cb);
    {
      uint32_t i, count = tmp->mQuerySets.Length();
      for (i = 0; i < count; ++i) {
        nsTemplateQuerySet *set = tmp->mQuerySets[i];
        cb.NoteXPCOMChild(set->mQueryNode);
        cb.NoteXPCOMChild(set->mCompiledQuery);
        uint16_t j, rulesCount = set->RuleCount();
        for (j = 0; j < rulesCount; ++j) {
          set->GetRuleAt(j)->Traverse(cb);
        }
      }
    }
    tmp->Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULTemplateBuilder)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULTemplateBuilder)

DOMCI_DATA(XULTemplateBuilder, nsXULTemplateBuilder)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULTemplateBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIXULTemplateBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULTemplateBuilder)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XULTemplateBuilder)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
//
// nsIXULTemplateBuilder methods
//

NS_IMETHODIMP
nsXULTemplateBuilder::GetRoot(nsIDOMElement** aResult)
{
    if (mRoot) {
        return CallQueryInterface(mRoot, aResult);
    }
    *aResult = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::GetDatasource(nsISupports** aResult)
{
    if (mCompDB)
        NS_ADDREF(*aResult = mCompDB);
    else
        NS_IF_ADDREF(*aResult = mDataSource);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::SetDatasource(nsISupports* aResult)
{
    mDataSource = aResult;
    mCompDB = do_QueryInterface(mDataSource);

    return Rebuild();
}

NS_IMETHODIMP
nsXULTemplateBuilder::GetDatabase(nsIRDFCompositeDataSource** aResult)
{
    NS_IF_ADDREF(*aResult = mCompDB);
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::GetQueryProcessor(nsIXULTemplateQueryProcessor** aResult)
{
    NS_IF_ADDREF(*aResult = mQueryProcessor.get());
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::AddRuleFilter(nsIDOMNode* aRule, nsIXULTemplateRuleFilter* aFilter)
{
    if (!aRule || !aFilter)
        return NS_ERROR_NULL_POINTER;

    // a custom rule filter may be added, one for each rule. If a new one is
    // added, it replaces the old one. Look for the right rule and set its
    // filter

    int32_t count = mQuerySets.Length();
    for (int32_t q = 0; q < count; q++) {
        nsTemplateQuerySet* queryset = mQuerySets[q];

        int16_t rulecount = queryset->RuleCount();
        for (int16_t r = 0; r < rulecount; r++) {
            nsTemplateRule* rule = queryset->GetRuleAt(r);

            nsCOMPtr<nsIDOMNode> rulenode;
            rule->GetRuleNode(getter_AddRefs(rulenode));
            if (aRule == rulenode) {
                rule->SetRuleFilter(aFilter);
                return NS_OK;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::Rebuild()
{
    int32_t i;

    for (i = mListeners.Count() - 1; i >= 0; --i) {
        mListeners[i]->WillRebuild(this);
    }

    nsresult rv = RebuildAll();

    for (i = mListeners.Count() - 1; i >= 0; --i) {
        mListeners[i]->DidRebuild(this);
    }

    return rv;
}

NS_IMETHODIMP
nsXULTemplateBuilder::Refresh()
{
    nsresult rv;

    if (!mCompDB)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsISimpleEnumerator> dslist;
    rv = mCompDB->GetDataSources(getter_AddRefs(dslist));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    nsCOMPtr<nsISupports> next;
    nsCOMPtr<nsIRDFRemoteDataSource> rds;

    while(NS_SUCCEEDED(dslist->HasMoreElements(&hasMore)) && hasMore) {
        dslist->GetNext(getter_AddRefs(next));
        if (next && (rds = do_QueryInterface(next))) {
            rds->Refresh(false);
        }
    }

    // XXXbsmedberg: it would be kinda nice to install an async nsIRDFXMLSink
    // observer and call rebuild() once the load is complete. See bug 254600.

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::Init(nsIContent* aElement)
{
    NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);
    mRoot = aElement;

    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    NS_ASSERTION(doc, "element has no document");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    bool shouldDelay;
    nsresult rv = LoadDataSources(doc, &shouldDelay);

    if (NS_SUCCEEDED(rv)) {
        // Add ourselves as a document observer
        doc->AddObserver(this);

        mObservedDocument = doc;
        gObserverService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                      false);
        gObserverService->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC,
                                      false);
    }

    return rv;
}

NS_IMETHODIMP
nsXULTemplateBuilder::CreateContents(nsIContent* aElement, bool aForceCreation)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::HasGeneratedContent(nsIRDFResource* aResource,
                                          nsIAtom* aTag,
                                          bool* aGenerated)
{
    *aGenerated = false;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::AddResult(nsIXULTemplateResult* aResult,
                                nsIDOMNode* aQueryNode)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aQueryNode);

    return UpdateResult(nullptr, aResult, aQueryNode);
}

NS_IMETHODIMP
nsXULTemplateBuilder::RemoveResult(nsIXULTemplateResult* aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    return UpdateResult(aResult, nullptr, nullptr);
}

NS_IMETHODIMP
nsXULTemplateBuilder::ReplaceResult(nsIXULTemplateResult* aOldResult,
                                    nsIXULTemplateResult* aNewResult,
                                    nsIDOMNode* aQueryNode)
{
    NS_ENSURE_ARG_POINTER(aOldResult);
    NS_ENSURE_ARG_POINTER(aNewResult);
    NS_ENSURE_ARG_POINTER(aQueryNode);

    // just remove the old result and then add a new result separately

    nsresult rv = UpdateResult(aOldResult, nullptr, nullptr);
    if (NS_FAILED(rv))
        return rv;

    return UpdateResult(nullptr, aNewResult, aQueryNode);
}

nsresult
nsXULTemplateBuilder::UpdateResult(nsIXULTemplateResult* aOldResult,
                                   nsIXULTemplateResult* aNewResult,
                                   nsIDOMNode* aQueryNode)
{
    PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
           ("nsXULTemplateBuilder::UpdateResult %p %p %p",
           aOldResult, aNewResult, aQueryNode));

    if (!mRoot || !mQueriesCompiled)
      return NS_OK;

    // get the containers where content may be inserted. If
    // GetInsertionLocations returns false, no container has generated
    // any content yet so new content should not be generated either. This
    // will be false if the result applies to content that is in a closed menu
    // or treeitem for example.

    nsAutoPtr<nsCOMArray<nsIContent> > insertionPoints;
    bool mayReplace = GetInsertionLocations(aOldResult ? aOldResult : aNewResult,
                                              getter_Transfers(insertionPoints));
    if (! mayReplace)
        return NS_OK;

    nsresult rv = NS_OK;

    nsCOMPtr<nsIRDFResource> oldId, newId;
    nsTemplateQuerySet* queryset = nullptr;

    if (aOldResult) {
        rv = GetResultResource(aOldResult, getter_AddRefs(oldId));
        if (NS_FAILED(rv))
            return rv;

        // Ignore re-entrant builds for content that is currently in our
        // activation stack.
        if (IsActivated(oldId))
            return NS_OK;
    }

    if (aNewResult) {
        rv = GetResultResource(aNewResult, getter_AddRefs(newId));
        if (NS_FAILED(rv))
            return rv;

        // skip results that don't have ids
        if (! newId)
            return NS_OK;

        // Ignore re-entrant builds for content that is currently in our
        // activation stack.
        if (IsActivated(newId))
            return NS_OK;

        // look for the queryset associated with the supplied query node
        nsCOMPtr<nsIContent> querycontent = do_QueryInterface(aQueryNode);

        int32_t count = mQuerySets.Length();
        for (int32_t q = 0; q < count; q++) {
            nsTemplateQuerySet* qs = mQuerySets[q];
            if (qs->mQueryNode == querycontent) {
                queryset = qs;
                break;
            }
        }

        if (! queryset)
            return NS_OK;
    }

    if (insertionPoints) {
        // iterate over each insertion point and add or remove the result from
        // that container
        uint32_t count = insertionPoints->Count();
        for (uint32_t t = 0; t < count; t++) {
            nsCOMPtr<nsIContent> insertionPoint = insertionPoints->SafeObjectAt(t);
            if (insertionPoint) {
                rv = UpdateResultInContainer(aOldResult, aNewResult, queryset,
                                             oldId, newId, insertionPoint);
                if (NS_FAILED(rv))
                    return rv;
            }
        }
    }
    else {
        // The tree builder doesn't use insertion points, so no insertion
        // points will be set. In this case, just update the one result.
        rv = UpdateResultInContainer(aOldResult, aNewResult, queryset,
                                     oldId, newId, nullptr);
    }

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::UpdateResultInContainer(nsIXULTemplateResult* aOldResult,
                                              nsIXULTemplateResult* aNewResult,
                                              nsTemplateQuerySet* aQuerySet,
                                              nsIRDFResource* aOldId,
                                              nsIRDFResource* aNewId,
                                              nsIContent* aInsertionPoint)
{
    // This method takes a result that no longer applies (aOldResult) and
    // replaces it with a new result (aNewResult). Either may be null
    // indicating to just remove a result or add a new one without replacing.
    //
    // Matches are stored in the hashtable mMatchMap, keyed by result id. If
    // there is more than one query, or the same id is found in different
    // containers, the values in the hashtable will be a linked list of all
    // the matches for that id. The matches are sorted according to the
    // queries they are associated with. Matches for earlier queries in the
    // template take priority over matches from later queries. The priority
    // for a match is determined from the match's QuerySetPriority method.
    // The first query has a priority 0, and higher numbers are for later
    // queries with successively higher priorities. Thus, a match takes
    // precedence if it has a lower priority than another. If there is only
    // one query or container, then the match doesn't have any linked items.
    //
    // Matches are nsTemplateMatch objects. They are wrappers around
    // nsIXULTemplateResult result objects and are created with
    // nsTemplateMatch::Create below. The aQuerySet argument specifies which
    // query the match is associated with.
    //
    // When a result id exists in multiple containers, the match's mContainer
    // field is set to the container it corresponds to. The aInsertionPoint
    // argument specifies which container is being updated. Even though they
    // are stored in the same linked list as other matches of the same id, the
    // matches for different containers are treated separately. They are only
    // stored in the same hashtable to avoid a more complex data structure, as
    // the use of the same id in multiple containers isn't a common occurance.
    //
    // Only one match with a given id per container is active at a time. When
    // a match is active, content is generated for it. When a match is
    // inactive, content is not generated for it. A match becomes active if
    // another match with the same id and container with a lower priority
    // isn't already active, and the match has a rule or conditions clause
    // which evaluates to true. The former is checked by comparing the value
    // of the QuerySetPriority method of the match with earlier matches. The
    // latter is checked with the DetermineMatchedRule method.
    //
    // Naturally, if a match with a lower priority is active, it overrides
    // the new match, so the new match is hooked up into the match linked
    // list as inactive, and no content is generated for it. If a match with a
    // higher priority is active, and the new match's conditions evaluate
    // to true, then this existing match with the higher priority needs to have
    // its generated content removed and replaced with the new match's
    // generated content.
    //
    // Similar situations apply when removing an existing match. If the match
    // is active, the existing generated content will need to be removed, and
    // a match of higher priority that is revealed may become active and need
    // to have content generated.
    //
    // Content removal and generation is done by the ReplaceMatch method which
    // is overridden for the content builder and tree builder to update the
    // generated output for each type.
    //
    // The code below handles all of the various cases and ensures that the
    // match lists are maintained properly.

    nsresult rv = NS_OK;
    int16_t ruleindex;
    nsTemplateRule* matchedrule = nullptr;

    // Indicates that the old match was active and must have its content
    // removed
    bool oldMatchWasActive = false;

    // acceptedmatch will be set to a new match that has to have new content
    // generated for it. If a new match doesn't need to have content
    // generated, (because for example, a match with a lower priority
    // already applies), then acceptedmatch will be null, but the match will
    // be still hooked up into the chain, since it may become active later
    // as other results are updated.
    nsTemplateMatch* acceptedmatch = nullptr;

    // When aOldResult is specified, removematch will be set to the
    // corresponding match. This match needs to be deleted as it no longer
    // applies. However, removedmatch will be null when aOldResult is null, or
    // when no match was found corresponding to aOldResult.
    nsTemplateMatch* removedmatch = nullptr;

    // These will be set when aNewResult is specified indicating to add a
    // result, but will end up replacing an existing match. The former
    // indicates a match being replaced that was active and had content
    // generated for it, while the latter indicates a match that wasn't active
    // and just needs to be deleted. Both may point to different matches. For
    // example, if the new match becomes active, replacing an inactive match,
    // the inactive match will need to be deleted. However, if another match
    // with a higher priority is active, the new match will override it, so
    // content will need to be generated for the new match and removed for
    // this existing active match.
    nsTemplateMatch* replacedmatch = nullptr, * replacedmatchtodelete = nullptr;

    if (aOldResult) {
        nsTemplateMatch* firstmatch;
        if (mMatchMap.Get(aOldId, &firstmatch)) {
            nsTemplateMatch* oldmatch = firstmatch;
            nsTemplateMatch* prevmatch = nullptr;

            // look for the right match if there was more than one
            while (oldmatch && (oldmatch->mResult != aOldResult)) {
                prevmatch = oldmatch;
                oldmatch = oldmatch->mNext;
            }

            if (oldmatch) {
                nsTemplateMatch* findmatch = oldmatch->mNext;

                // Keep a reference so that linked list can be hooked up at
                // the end in case an error occurs.
                nsTemplateMatch* nextmatch = findmatch;

                if (oldmatch->IsActive()) {
                    // Indicate that the old match was active so its content
                    // will be removed later.
                    oldMatchWasActive = true;

                    // The match being removed is the active match, so scan
                    // through the later matches to determine if one should
                    // now become the active match.
                    while (findmatch) {
                        // only other matches with the same container should
                        // now match, leave other containers alone
                        if (findmatch->GetContainer() == aInsertionPoint) {
                            nsTemplateQuerySet* qs =
                                mQuerySets[findmatch->QuerySetPriority()];
                        
                            DetermineMatchedRule(aInsertionPoint, findmatch->mResult,
                                                 qs, &matchedrule, &ruleindex);

                            if (matchedrule) {
                                rv = findmatch->RuleMatched(qs,
                                                            matchedrule, ruleindex,
                                                            findmatch->mResult);
                                if (NS_FAILED(rv))
                                    return rv;

                                acceptedmatch = findmatch;
                                break;
                            }
                        }

                        findmatch = findmatch->mNext;
                    }
                }

                if (oldmatch == firstmatch) {
                    // the match to remove is at the beginning
                    if (oldmatch->mNext) {
                        mMatchMap.Put(aOldId, oldmatch->mNext);
                    }
                    else {
                        mMatchMap.Remove(aOldId);
                    }
                }

                if (prevmatch)
                    prevmatch->mNext = nextmatch;

                removedmatch = oldmatch;
                if (mFlags & eLoggingEnabled)
                    OutputMatchToLog(aOldId, removedmatch, false);
            }
        }
    }

    nsTemplateMatch *newmatch = nullptr;
    if (aNewResult) {
        // only allow a result to be inserted into containers with a matching tag
        nsIAtom* tag = aQuerySet->GetTag();
        if (aInsertionPoint && tag && tag != aInsertionPoint->Tag())
            return NS_OK;

        int32_t findpriority = aQuerySet->Priority();

        newmatch = nsTemplateMatch::Create(findpriority,
                                           aNewResult, aInsertionPoint);
        if (!newmatch)
            return NS_ERROR_OUT_OF_MEMORY;

        nsTemplateMatch* firstmatch;
        if (mMatchMap.Get(aNewId, &firstmatch)) {
            bool hasEarlierActiveMatch = false;

            // Scan through the existing matches to find where the new one
            // should be inserted. oldmatch will be set to the old match for
            // the same query and prevmatch will be set to the match before it.
            nsTemplateMatch* prevmatch = nullptr;
            nsTemplateMatch* oldmatch = firstmatch;
            while (oldmatch) {
                // Break out once we've reached a query in the list with a
                // lower priority. The new match will be inserted at this
                // location so that the match list is sorted by priority.
                int32_t priority = oldmatch->QuerySetPriority();
                if (priority > findpriority) {
                    oldmatch = nullptr;
                    break;
                }

                // look for matches that belong in the same container
                if (oldmatch->GetContainer() == aInsertionPoint) {
                    if (priority == findpriority)
                        break;

                    // If a match with a lower priority is active, the new
                    // match can't replace it.
                    if (oldmatch->IsActive())
                        hasEarlierActiveMatch = true;
                }

                prevmatch = oldmatch;
                oldmatch = oldmatch->mNext;
            }

            // At this point, oldmatch will either be null, or set to a match
            // with the same container and priority. If set, oldmatch will
            // need to be replaced by newmatch.

            if (oldmatch)
                newmatch->mNext = oldmatch->mNext;
            else if (prevmatch)
                newmatch->mNext = prevmatch->mNext;
            else
                newmatch->mNext = firstmatch;

            // hasEarlierActiveMatch will be set to true if a match with a
            // lower priority was found. The new match won't replace it in
            // this case. If hasEarlierActiveMatch is false, then the new match
            // may be become active if it matches one of the rules, and will
            // generate output. It's also possible however, that a match with
            // the same priority already exists, which means that the new match
            // will replace the old one. In this case, oldmatch will be set to
            // the old match. The content for the old match must be removed and
            // content for the new match generated in its place.
            if (! hasEarlierActiveMatch) {
                // If the old match was the active match, set replacedmatch to
                // indicate that it needs its content removed.
                if (oldmatch) {
                    if (oldmatch->IsActive())
                        replacedmatch = oldmatch;
                    replacedmatchtodelete = oldmatch;
                }

                // check if the new result matches the rules
                rv = DetermineMatchedRule(aInsertionPoint, newmatch->mResult,
                                          aQuerySet, &matchedrule, &ruleindex);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(newmatch, false);
                    return rv;
                }

                if (matchedrule) {
                    rv = newmatch->RuleMatched(aQuerySet,
                                               matchedrule, ruleindex,
                                               newmatch->mResult);
                    if (NS_FAILED(rv)) {
                        nsTemplateMatch::Destroy(newmatch, false);
                        return rv;
                    }

                    // acceptedmatch may have been set in the block handling
                    // aOldResult earlier. If so, we would only get here when
                    // that match has a higher priority than this new match.
                    // As only one match can have content generated for it, it
                    // is OK to set acceptedmatch here to the new match,
                    // ignoring the other one.
                    acceptedmatch = newmatch;

                    // Clear the matched state of the later results for the
                    // same container.
                    nsTemplateMatch* clearmatch = newmatch->mNext;
                    while (clearmatch) {
                        if (clearmatch->GetContainer() == aInsertionPoint &&
                            clearmatch->IsActive()) {
                            clearmatch->SetInactive();
                            // Replacedmatch should be null here. If not, it
                            // means that two matches were active which isn't
                            // a valid state
                            NS_ASSERTION(!replacedmatch,
                                         "replaced match already set");
                            replacedmatch = clearmatch;
                            break;
                        }
                        clearmatch = clearmatch->mNext;
                    }
                }
                else if (oldmatch && oldmatch->IsActive()) {
                    // The result didn't match the rules, so look for a later
                    // one. However, only do this if the old match was the
                    // active match.
                    newmatch = newmatch->mNext;
                    while (newmatch) {
                        if (newmatch->GetContainer() == aInsertionPoint) {
                            rv = DetermineMatchedRule(aInsertionPoint, newmatch->mResult,
                                                      aQuerySet, &matchedrule, &ruleindex);
                            if (NS_FAILED(rv)) {
                                nsTemplateMatch::Destroy(newmatch, false);
                                return rv;
                            }

                            if (matchedrule) {
                                rv = newmatch->RuleMatched(aQuerySet,
                                                           matchedrule, ruleindex,
                                                           newmatch->mResult);
                                if (NS_FAILED(rv)) {
                                    nsTemplateMatch::Destroy(newmatch, false);
                                    return rv;
                                }

                                acceptedmatch = newmatch;
                                break;
                            }
                        }

                        newmatch = newmatch->mNext;
                    }
                }

                // put the match in the map if there isn't a previous match
                if (! prevmatch) {
                    mMatchMap.Put(aNewId, newmatch);
                }
            }

            // hook up the match last in case an error occurs
            if (prevmatch)
                prevmatch->mNext = newmatch;
        }
        else {
            // The id is not used in the hashtable yet so create a new match
            // and add it to the hashtable.
            rv = DetermineMatchedRule(aInsertionPoint, aNewResult,
                                      aQuerySet, &matchedrule, &ruleindex);
            if (NS_FAILED(rv)) {
                nsTemplateMatch::Destroy(newmatch, false);
                return rv;
            }

            if (matchedrule) {
                rv = newmatch->RuleMatched(aQuerySet, matchedrule,
                                           ruleindex, aNewResult);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(newmatch, false);
                    return rv;
                }

                acceptedmatch = newmatch;
            }

            mMatchMap.Put(aNewId, newmatch);
        }
    }

    // The ReplaceMatch method is builder specific and removes the generated
    // content for a match.

    // Remove the content for a match that was active and needs to be replaced.
    if (replacedmatch) {
        rv = ReplaceMatch(replacedmatch->mResult, nullptr, nullptr,
                          aInsertionPoint);

        if (mFlags & eLoggingEnabled)
            OutputMatchToLog(aNewId, replacedmatch, false);
    }

    // remove a match that needs to be deleted.
    if (replacedmatchtodelete)
        nsTemplateMatch::Destroy(replacedmatchtodelete, true);

    // If the old match was active, the content for it needs to be removed.
    // If the old match was not active, it shouldn't have had any content,
    // so just pass null to ReplaceMatch. If acceptedmatch was set, then
    // content needs to be generated for a new match.
    if (oldMatchWasActive || acceptedmatch)
        rv = ReplaceMatch(oldMatchWasActive ? aOldResult : nullptr,
                          acceptedmatch, matchedrule, aInsertionPoint);

    // delete the old match that was replaced
    if (removedmatch)
        nsTemplateMatch::Destroy(removedmatch, true);

    if (mFlags & eLoggingEnabled && newmatch)
        OutputMatchToLog(aNewId, newmatch, true);

    return rv;
}

NS_IMETHODIMP
nsXULTemplateBuilder::ResultBindingChanged(nsIXULTemplateResult* aResult)
{
    // A binding update is used when only the values of the bindings have
    // changed, so the same rule still applies. Just synchronize the content.
    // The new result will have the new values.
    NS_ENSURE_ARG_POINTER(aResult);

    if (!mRoot || !mQueriesCompiled)
      return NS_OK;

    return SynchronizeResult(aResult);
}

NS_IMETHODIMP
nsXULTemplateBuilder::GetRootResult(nsIXULTemplateResult** aResult)
{
  *aResult = mRootResult;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::GetResultForId(const nsAString& aId,
                                     nsIXULTemplateResult** aResult)
{
    if (aId.IsEmpty())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIRDFResource> resource;
    gRDFService->GetUnicodeResource(aId, getter_AddRefs(resource));

    *aResult = nullptr;

    nsTemplateMatch* match;
    if (mMatchMap.Get(resource, &match)) {
        // find the active match
        while (match) {
            if (match->IsActive()) {
                *aResult = match->mResult;
                NS_IF_ADDREF(*aResult);
                break;
            }
            match = match->mNext;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::GetResultForContent(nsIDOMElement* aContent,
                                          nsIXULTemplateResult** aResult)
{
    *aResult = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::AddListener(nsIXULBuilderListener* aListener)
{
    NS_ENSURE_ARG(aListener);

    if (!mListeners.AppendObject(aListener))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::RemoveListener(nsIXULBuilderListener* aListener)
{
    NS_ENSURE_ARG(aListener);

    mListeners.RemoveObject(aListener);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
{
    // Uuuuber hack to clean up circular references that the cycle collector
    // doesn't know about. See bug 394514.
    if (!strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC)) {
        nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aSubject);
        if (window) {
            nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
            if (doc && doc == mObservedDocument)
                NodeWillBeDestroyed(doc);
        }
    } else if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
        UninitTrue();
    }
    return NS_OK;
}
//----------------------------------------------------------------------
//
// nsIDocumentOberver interface
//

void
nsXULTemplateBuilder::AttributeChanged(nsIDocument* aDocument,
                                       Element*     aElement,
                                       int32_t      aNameSpaceID,
                                       nsIAtom*     aAttribute,
                                       int32_t      aModType)
{
    if (aElement == mRoot && aNameSpaceID == kNameSpaceID_None) {
        // Check for a change to the 'ref' attribute on an atom, in which
        // case we may need to nuke and rebuild the entire content model
        // beneath the element.
        if (aAttribute == nsGkAtoms::ref)
            nsContentUtils::AddScriptRunner(
                NS_NewRunnableMethod(this, &nsXULTemplateBuilder::RunnableRebuild));

        // Check for a change to the 'datasources' attribute. If so, setup
        // mDB by parsing the new value and rebuild.
        else if (aAttribute == nsGkAtoms::datasources) {
            nsContentUtils::AddScriptRunner(
                NS_NewRunnableMethod(this, &nsXULTemplateBuilder::RunnableLoadAndRebuild));
        }
    }
}

void
nsXULTemplateBuilder::ContentRemoved(nsIDocument* aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     int32_t aIndexInContainer,
                                     nsIContent* aPreviousSibling)
{
    if (mRoot && nsContentUtils::ContentIsDescendantOf(mRoot, aChild)) {
        nsRefPtr<nsXULTemplateBuilder> kungFuDeathGrip(this);

        if (mQueryProcessor)
            mQueryProcessor->Done();

        // Pass false to Uninit since content is going away anyway
        nsContentUtils::AddScriptRunner(
            NS_NewRunnableMethod(this, &nsXULTemplateBuilder::UninitFalse));

        aDocument->RemoveObserver(this);

        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(aDocument);
        if (xuldoc)
            xuldoc->SetTemplateBuilderFor(mRoot, nullptr);

        // clear the template state when removing content so that template
        // content will be regenerated again if the content is reinserted
        nsXULElement *xulcontent = nsXULElement::FromContent(mRoot);
        if (xulcontent)
            xulcontent->ClearTemplateGenerated();

        CleanUp(true);

        mDB = nullptr;
        mCompDB = nullptr;
        mDataSource = nullptr;
    }
}

void
nsXULTemplateBuilder::NodeWillBeDestroyed(const nsINode* aNode)
{
    // The call to RemoveObserver could release the last reference to
    // |this|, so hold another reference.
    nsRefPtr<nsXULTemplateBuilder> kungFuDeathGrip(this);

    // Break circular references
    if (mQueryProcessor)
        mQueryProcessor->Done();

    mDataSource = nullptr;
    mDB = nullptr;
    mCompDB = nullptr;

    nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &nsXULTemplateBuilder::UninitTrue));
}




//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
nsXULTemplateBuilder::LoadDataSources(nsIDocument* aDocument,
                                      bool* aShouldDelayBuilding)
{
    NS_PRECONDITION(mRoot != nullptr, "not initialized");

    nsresult rv;
    bool isRDFQuery = false;
  
    // we'll set these again later, after we create a new composite ds
    mDB = nullptr;
    mCompDB = nullptr;
    mDataSource = nullptr;

    *aShouldDelayBuilding = false;

    nsAutoString datasources;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::datasources, datasources);

    nsAutoString querytype;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::querytype, querytype);

    // create the query processor. The querytype attribute on the root element
    // may be used to create one of a specific type.
  
    // XXX should non-chrome be restricted to specific names?
    if (querytype.IsEmpty())
        querytype.AssignLiteral("rdf");

    if (querytype.EqualsLiteral("rdf")) {
        isRDFQuery = true;
        mQueryProcessor = new nsXULTemplateQueryProcessorRDF();
        NS_ENSURE_TRUE(mQueryProcessor, NS_ERROR_OUT_OF_MEMORY);
    }
    else if (querytype.EqualsLiteral("xml")) {
        mQueryProcessor = new nsXULTemplateQueryProcessorXML();
        NS_ENSURE_TRUE(mQueryProcessor, NS_ERROR_OUT_OF_MEMORY);
    }
    else if (querytype.EqualsLiteral("storage")) {
        mQueryProcessor = new nsXULTemplateQueryProcessorStorage();
        NS_ENSURE_TRUE(mQueryProcessor, NS_ERROR_OUT_OF_MEMORY);
    }
    else {
        nsAutoCString cid(NS_QUERY_PROCESSOR_CONTRACTID_PREFIX);
        AppendUTF16toUTF8(querytype, cid);
        mQueryProcessor = do_CreateInstance(cid.get(), &rv);

        if (!mQueryProcessor) {
            nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_INVALID_QUERYPROCESSOR);
            return rv;
        }
    }

    rv = LoadDataSourceUrls(aDocument, datasources,
                            isRDFQuery, aShouldDelayBuilding);
    NS_ENSURE_SUCCESS(rv, rv);

    // Now set the database on the element, so that script writers can
    // access it.
    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(aDocument);
    if (xuldoc)
        xuldoc->SetTemplateBuilderFor(mRoot, this);

    if (!mRoot->IsXUL()) {
        // Hmm. This must be an HTML element. Try to set it as a
        // JS property "by hand".
        InitHTMLTemplateRoot();
    }
  
    return NS_OK;
}
  
nsresult
nsXULTemplateBuilder::LoadDataSourceUrls(nsIDocument* aDocument,
                                         const nsAString& aDataSources,
                                         bool aIsRDFQuery,
                                         bool* aShouldDelayBuilding)
{
    // Grab the doc's principal...
    nsIPrincipal *docPrincipal = aDocument->NodePrincipal();

    NS_ASSERTION(docPrincipal == mRoot->NodePrincipal(),
                 "Principal mismatch?  Which one to use?");

    bool isTrusted = false;
    nsresult rv = IsSystemPrincipal(docPrincipal, &isTrusted);
    NS_ENSURE_SUCCESS(rv, rv);

    // Parse datasources: they are assumed to be a whitespace
    // separated list of URIs; e.g.,
    //
    //     rdf:bookmarks rdf:history http://foo.bar.com/blah.cgi?baz=9
    //
    nsIURI *docurl = aDocument->GetDocumentURI();

    nsCOMPtr<nsIMutableArray> uriList = do_CreateInstance(NS_ARRAY_CONTRACTID);
    if (!uriList)
        return NS_ERROR_FAILURE;

    nsAutoString datasources(aDataSources);
    uint32_t first = 0;
    while (1) {
        while (first < datasources.Length() && nsCRT::IsAsciiSpace(datasources.CharAt(first)))
            ++first;

        if (first >= datasources.Length())
            break;

        uint32_t last = first;
        while (last < datasources.Length() && !nsCRT::IsAsciiSpace(datasources.CharAt(last)))
            ++last;

        nsAutoString uriStr;
        datasources.Mid(uriStr, first, last - first);
        first = last + 1;

        // A special 'dummy' datasource
        if (uriStr.EqualsLiteral("rdf:null"))
            continue;

        if (uriStr.CharAt(0) == '#') {
            // ok, the datasource is certainly a node of the current document
            nsCOMPtr<nsIDOMDocument> domdoc = do_QueryInterface(aDocument);
            nsCOMPtr<nsIDOMElement> dsnode;

            domdoc->GetElementById(Substring(uriStr, 1),
                                   getter_AddRefs(dsnode));

            if (dsnode)
                uriList->AppendElement(dsnode, false);
            continue;
        }

        // N.B. that `failure' (e.g., because it's an unknown
        // protocol) leaves uriStr unaltered.
        NS_MakeAbsoluteURI(uriStr, uriStr, docurl);

        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), uriStr);
        if (NS_FAILED(rv) || !uri)
            continue; // Necko will barf if our URI is weird

        // don't add the uri to the list if the document is not allowed to
        // load it
        if (!isTrusted && NS_FAILED(docPrincipal->CheckMayLoad(uri, true, false)))
          continue;

        uriList->AppendElement(uri, false);
    }

    nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(mRoot);
    rv = mQueryProcessor->GetDatasource(uriList,
                                        rootNode,
                                        isTrusted,
                                        this,
                                        aShouldDelayBuilding,
                                        getter_AddRefs(mDataSource));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aIsRDFQuery && mDataSource) {  
        // check if we were given an inference engine type
        nsCOMPtr<nsIRDFInferDataSource> inferDB = do_QueryInterface(mDataSource);
        if (inferDB) {
            nsCOMPtr<nsIRDFDataSource> ds;
            inferDB->GetBaseDataSource(getter_AddRefs(ds));
            if (ds)
                mCompDB = do_QueryInterface(ds);
        }

        if (!mCompDB)
            mCompDB = do_QueryInterface(mDataSource);

        mDB = do_QueryInterface(mDataSource);
    }

    if (!mDB && isTrusted) {
        gRDFService->GetDataSource("rdf:local-store", getter_AddRefs(mDB));
    }

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::InitHTMLTemplateRoot()
{
    // Use XPConnect and the JS APIs to whack mDB and this as the
    // 'database' and 'builder' properties onto aElement.
    nsresult rv;

    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    NS_ASSERTION(doc, "no document");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIScriptGlobalObject> global =
      do_QueryInterface(doc->GetWindow());
    if (! global)
        return NS_ERROR_UNEXPECTED;

    nsIScriptContext *context = global->GetContext();
    if (! context)
        return NS_ERROR_UNEXPECTED;

    AutoPushJSContext jscontext(context->GetNativeContext());
    NS_ASSERTION(context != nullptr, "no jscontext");
    if (! jscontext)
        return NS_ERROR_UNEXPECTED;

    JS::Rooted<JSObject*> scope(jscontext, global->GetGlobalJSObject());
    JS::Rooted<JS::Value> v(jscontext);
    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = nsContentUtils::WrapNative(jscontext, scope, mRoot, mRoot, v.address(),
                                    getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Rooted<JSObject*> jselement(jscontext, JSVAL_TO_OBJECT(v));

    if (mDB) {
        // database
        JS::Rooted<JS::Value> jsdatabase(jscontext);
        rv = nsContentUtils::WrapNative(jscontext, scope, mDB,
                                        &NS_GET_IID(nsIRDFCompositeDataSource),
                                        jsdatabase.address(), getter_AddRefs(wrapper));
        NS_ENSURE_SUCCESS(rv, rv);

        bool ok;
        ok = JS_SetProperty(jscontext, jselement, "database", jsdatabase.address());
        NS_ASSERTION(ok, "unable to set database property");
        if (! ok)
            return NS_ERROR_FAILURE;
    }

    {
        // builder
        JS::Rooted<JS::Value> jsbuilder(jscontext);
        nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
        rv = nsContentUtils::WrapNative(jscontext, jselement,
                                        static_cast<nsIXULTemplateBuilder*>(this),
                                        &NS_GET_IID(nsIXULTemplateBuilder),
                                        jsbuilder.address(), getter_AddRefs(wrapper));
        NS_ENSURE_SUCCESS(rv, rv);

        bool ok;
        ok = JS_SetProperty(jscontext, jselement, "builder", jsbuilder.address());
        if (! ok)
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::DetermineMatchedRule(nsIContent *aContainer,
                                           nsIXULTemplateResult* aResult,
                                           nsTemplateQuerySet* aQuerySet,
                                           nsTemplateRule** aMatchedRule,
                                           int16_t *aRuleIndex)
{
    // iterate through the rules and look for one that the result matches
    int16_t count = aQuerySet->RuleCount();
    for (int16_t r = 0; r < count; r++) {
        nsTemplateRule* rule = aQuerySet->GetRuleAt(r);
        // If a tag was specified, it must match the tag of the container
        // where content is being inserted.
        nsIAtom* tag = rule->GetTag();
        if ((!aContainer || !tag || tag == aContainer->Tag()) &&
            rule->CheckMatch(aResult)) {
            *aMatchedRule = rule;
            *aRuleIndex = r;
            return NS_OK;
        }
    }

    *aRuleIndex = -1;
    *aMatchedRule = nullptr;
    return NS_OK;
}

void
nsXULTemplateBuilder::ParseAttribute(const nsAString& aAttributeValue,
                                     void (*aVariableCallback)(nsXULTemplateBuilder*, const nsAString&, void*),
                                     void (*aTextCallback)(nsXULTemplateBuilder*, const nsAString&, void*),
                                     void* aClosure)
{
    nsAString::const_iterator done_parsing;
    aAttributeValue.EndReading(done_parsing);

    nsAString::const_iterator iter;
    aAttributeValue.BeginReading(iter);

    nsAString::const_iterator mark(iter), backup(iter);

    for (; iter != done_parsing; backup = ++iter) {
        // A variable is either prefixed with '?' (in the extended
        // syntax) or "rdf:" (in the simple syntax).
        bool isvar;
        if (*iter == PRUnichar('?') && (++iter != done_parsing)) {
            isvar = true;
        }
        else if ((*iter == PRUnichar('r') && (++iter != done_parsing)) &&
                 (*iter == PRUnichar('d') && (++iter != done_parsing)) &&
                 (*iter == PRUnichar('f') && (++iter != done_parsing)) &&
                 (*iter == PRUnichar(':') && (++iter != done_parsing))) {
            isvar = true;
        }
        else {
            isvar = false;
        }

        if (! isvar) {
            // It's not a variable, or we ran off the end of the
            // string after the initial variable prefix. Since we may
            // have slurped down some characters before realizing that
            // fact, back up to the point where we started.
            iter = backup;
            continue;
        }
        else if (backup != mark && aTextCallback) {
            // Okay, we've found a variable, and there's some vanilla
            // text that's been buffered up. Flush it.
            (*aTextCallback)(this, Substring(mark, backup), aClosure);
        }

        if (*iter == PRUnichar('?')) {
            // Well, it was not really a variable, but "??". We use one
            // question mark (the second one, actually) literally.
            mark = iter;
            continue;
        }

        // Construct a substring that is the symbol we need to look up
        // in the rule's symbol table. The symbol is terminated by a
        // space character, a caret, or the end of the string,
        // whichever comes first.
        nsAString::const_iterator first(backup);

        PRUnichar c = 0;
        while (iter != done_parsing) {
            c = *iter;
            if ((c == PRUnichar(' ')) || (c == PRUnichar('^')))
                break;

            ++iter;
        }

        nsAString::const_iterator last(iter);

        // Back up so we don't consume the terminating character
        // *unless* the terminating character was a caret: the caret
        // means "concatenate with no space in between".
        if (c != PRUnichar('^'))
            --iter;

        (*aVariableCallback)(this, Substring(first, last), aClosure);
        mark = iter;
        ++mark;
    }

    if (backup != mark && aTextCallback) {
        // If there's any text left over, then fire the text callback
        (*aTextCallback)(this, Substring(mark, backup), aClosure);
    }
}


struct MOZ_STACK_CLASS SubstituteTextClosure {
    SubstituteTextClosure(nsIXULTemplateResult* aResult, nsAString& aString)
        : result(aResult), str(aString) {}

    // some datasources are lazily initialized or modified while values are
    // being retrieved, causing results to be removed. Due to this, hold a
    // strong reference to the result.
    nsCOMPtr<nsIXULTemplateResult> result;
    nsAString& str;
};

nsresult
nsXULTemplateBuilder::SubstituteText(nsIXULTemplateResult* aResult,
                                     const nsAString& aAttributeValue,
                                     nsAString& aString)
{
    // See if it's the special value "..."
    if (aAttributeValue.EqualsLiteral("...")) {
        aResult->GetId(aString);
        return NS_OK;
    }

    // Reasonable guess at how big it should be
    aString.SetCapacity(aAttributeValue.Length());

    SubstituteTextClosure closure(aResult, aString);
    ParseAttribute(aAttributeValue,
                   SubstituteTextReplaceVariable,
                   SubstituteTextAppendText,
                   &closure);

    return NS_OK;
}


void
nsXULTemplateBuilder::SubstituteTextAppendText(nsXULTemplateBuilder* aThis,
                                               const nsAString& aText,
                                               void* aClosure)
{
    // Append aString to the closure's result
    SubstituteTextClosure* c = static_cast<SubstituteTextClosure*>(aClosure);
    c->str.Append(aText);
}

void
nsXULTemplateBuilder::SubstituteTextReplaceVariable(nsXULTemplateBuilder* aThis,
                                                    const nsAString& aVariable,
                                                    void* aClosure)
{
    // Substitute the value for the variable and append to the
    // closure's result.
    SubstituteTextClosure* c = static_cast<SubstituteTextClosure*>(aClosure);

    nsAutoString replacementText;

    // The symbol "rdf:*" is special, and means "this guy's URI"
    if (aVariable.EqualsLiteral("rdf:*")){
        c->result->GetId(replacementText);
    }
    else {
        // Got a variable; get the value it's assigned to
        nsCOMPtr<nsIAtom> var = do_GetAtom(aVariable);
        c->result->GetBindingFor(var, replacementText);
    }

    c->str += replacementText;
}

bool
nsXULTemplateBuilder::IsTemplateElement(nsIContent* aContent)
{
    return aContent->NodeInfo()->Equals(nsGkAtoms::_template,
                                        kNameSpaceID_XUL);
}

nsresult
nsXULTemplateBuilder::GetTemplateRoot(nsIContent** aResult)
{
    NS_PRECONDITION(mRoot != nullptr, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    // First, check and see if the root has a template attribute. This
    // allows a template to be specified "out of line"; e.g.,
    //
    //   <window>
    //     <foo template="MyTemplate">...</foo>
    //     <template id="MyTemplate">...</template>
    //   </window>
    //
    nsAutoString templateID;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::_template, templateID);

    if (! templateID.IsEmpty()) {
        nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(mRoot->GetDocument());
        if (! domDoc)
            return NS_OK;

        nsCOMPtr<nsIDOMElement> domElement;
        domDoc->GetElementById(templateID, getter_AddRefs(domElement));

        if (domElement) {
            nsCOMPtr<nsIContent> content = do_QueryInterface(domElement);
            NS_ENSURE_STATE(content &&
                            !nsContentUtils::ContentIsDescendantOf(mRoot,
                                                                   content));
            content.forget(aResult);
            return NS_OK;
        }
    }

#if 1 // XXX hack to workaround bug with XBL insertion/removal?
    {
        // If root node has no template attribute, then look for a child
        // node which is a template tag
        for (nsIContent* child = mRoot->GetFirstChild();
             child;
             child = child->GetNextSibling()) {

            if (IsTemplateElement(child)) {
                NS_ADDREF(*aResult = child);
                return NS_OK;
            }
        }
    }
#endif

    // If we couldn't find a real child, look through the anonymous
    // kids, too.
    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    if (! doc)
        return NS_OK;

    nsCOMPtr<nsIDOMNodeList> kids;
    doc->BindingManager()->GetXBLChildNodesFor(mRoot, getter_AddRefs(kids));

    if (kids) {
        uint32_t length;
        kids->GetLength(&length);

        for (uint32_t i = 0; i < length; ++i) {
            nsCOMPtr<nsIDOMNode> node;
            kids->Item(i, getter_AddRefs(node));
            if (! node)
                continue;

            nsCOMPtr<nsIContent> child = do_QueryInterface(node);

            if (IsTemplateElement(child)) {
                NS_ADDREF(*aResult = child.get());
                return NS_OK;
            }
        }
    }

    *aResult = nullptr;
    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileQueries()
{
    nsCOMPtr<nsIContent> tmpl;
    GetTemplateRoot(getter_AddRefs(tmpl));
    if (! tmpl)
        return NS_OK;

    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    // Determine if there are any special settings we need to observe
    mFlags = 0;

    nsAutoString flags;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::flags, flags);

    // if the dont-test-empty flag is set, containers should not be checked to
    // see if they are empty. If dont-recurse is set, then don't process the
    // template recursively and only show one level of results. The logging
    // flag logs errors and results to the console, which is useful when
    // debugging templates.
    nsWhitespaceTokenizer tokenizer(flags);
    while (tokenizer.hasMoreTokens()) {
      const nsDependentSubstring& token(tokenizer.nextToken());
      if (token.EqualsLiteral("dont-test-empty"))
        mFlags |= eDontTestEmpty;
      else if (token.EqualsLiteral("dont-recurse"))
        mFlags |= eDontRecurse;
      else if (token.EqualsLiteral("logging"))
        mFlags |= eLoggingEnabled;
    }

#ifdef PR_LOGGING
    // always enable logging if the debug setting is used
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG))
        mFlags |= eLoggingEnabled;
#endif

    nsCOMPtr<nsIDOMNode> rootnode = do_QueryInterface(mRoot);
    nsresult rv =
        mQueryProcessor->InitializeForBuilding(mDataSource, this, rootnode);
    if (NS_FAILED(rv))
        return rv;

    // Set the "container" and "member" variables, if the user has specified
    // them. The container variable may be specified with the container
    // attribute on the <template> and the member variable may be specified
    // using the member attribute or the value of the uri attribute inside the
    // first action body in the template. If not specified, the container
    // variable defaults to '?uri' and the member variable defaults to '?' or
    // 'rdf:*' for simple queries.

    // For RDF queries, the container variable may also be set via the
    // <content> tag.

    nsAutoString containervar;
    tmpl->GetAttr(kNameSpaceID_None, nsGkAtoms::container, containervar);

    if (containervar.IsEmpty())
        mRefVariable = do_GetAtom("?uri");
    else
        mRefVariable = do_GetAtom(containervar);

    nsAutoString membervar;
    tmpl->GetAttr(kNameSpaceID_None, nsGkAtoms::member, membervar);

    if (membervar.IsEmpty())
        mMemberVariable = nullptr;
    else
        mMemberVariable = do_GetAtom(membervar);

    nsTemplateQuerySet* queryset = new nsTemplateQuerySet(0);
    if (!queryset)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!mQuerySets.AppendElement(queryset)) {
        delete queryset;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    bool canUseTemplate = false;
    int32_t priority = 0;
    rv = CompileTemplate(tmpl, queryset, false, &priority, &canUseTemplate);

    if (NS_FAILED(rv) || !canUseTemplate) {
        for (int32_t q = mQuerySets.Length() - 1; q >= 0; q--) {
            nsTemplateQuerySet* qs = mQuerySets[q];
            delete qs;
        }
        mQuerySets.Clear();
    }

    mQueriesCompiled = true;

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileTemplate(nsIContent* aTemplate,
                                      nsTemplateQuerySet* aQuerySet,
                                      bool aIsQuerySet,
                                      int32_t* aPriority,
                                      bool* aCanUseTemplate)
{
    NS_ASSERTION(aQuerySet, "No queryset supplied");

    nsresult rv = NS_OK;

    bool isQuerySetMode = false;
    bool hasQuerySet = false, hasRule = false, hasQuery = false;

    for (nsIContent* rulenode = aTemplate->GetFirstChild();
         rulenode;
         rulenode = rulenode->GetNextSibling()) {

        nsINodeInfo *ni = rulenode->NodeInfo();

        // don't allow more queries than can be supported
        if (*aPriority == INT16_MAX)
            return NS_ERROR_FAILURE;

        // XXXndeakin queryset isn't a good name for this tag since it only
        //            ever contains one query
        if (!aIsQuerySet && ni->Equals(nsGkAtoms::queryset, kNameSpaceID_XUL)) {
            if (hasRule || hasQuery) {
              nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_INVALID_QUERYSET);
              continue;
            }

            isQuerySetMode = true;

            // only create a queryset for those after the first since the
            // first one is always created by CompileQueries
            if (hasQuerySet) {
                aQuerySet = new nsTemplateQuerySet(++*aPriority);
                if (!aQuerySet)
                    return NS_ERROR_OUT_OF_MEMORY;

                // once the queryset is appended to the mQuerySets list, it
                // will be removed by CompileQueries if an error occurs
                if (!mQuerySets.AppendElement(aQuerySet)) {
                    delete aQuerySet;
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }

            hasQuerySet = true;

            rv = CompileTemplate(rulenode, aQuerySet, true, aPriority, aCanUseTemplate);
            if (NS_FAILED(rv))
                return rv;
        }

        // once a queryset is used, everything must be a queryset
        if (isQuerySetMode)
            continue;

        if (ni->Equals(nsGkAtoms::rule, kNameSpaceID_XUL)) {
            nsCOMPtr<nsIContent> action;
            nsXULContentUtils::FindChildByTag(rulenode,
                                              kNameSpaceID_XUL,
                                              nsGkAtoms::action,
                                              getter_AddRefs(action));

            if (action){
                nsCOMPtr<nsIAtom> memberVariable = mMemberVariable;
                if (!memberVariable) {
                    memberVariable = DetermineMemberVariable(action);
                    if (!memberVariable) {
                        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_NO_MEMBERVAR);
                        continue;
                    }
                }

                if (hasQuery) {
                    nsCOMPtr<nsIAtom> tag;
                    DetermineRDFQueryRef(aQuerySet->mQueryNode,
                                         getter_AddRefs(tag));
                    if (tag)
                        aQuerySet->SetTag(tag);

                    if (! aQuerySet->mCompiledQuery) {
                        nsCOMPtr<nsIDOMNode> query(do_QueryInterface(aQuerySet->mQueryNode));

                        rv = mQueryProcessor->CompileQuery(this, query,
                                                           mRefVariable, memberVariable,
                                                           getter_AddRefs(aQuerySet->mCompiledQuery));
                        if (NS_FAILED(rv))
                            return rv;
                    }

                    if (aQuerySet->mCompiledQuery) {
                        rv = CompileExtendedQuery(rulenode, action, memberVariable,
                                                  aQuerySet);
                        if (NS_FAILED(rv))
                            return rv;

                        *aCanUseTemplate = true;
                    }
                }
                else {
                    // backwards-compatible RDF template syntax where there is
                    // an <action> node but no <query> node. In this case,
                    // use the conditions as if it was the query.

                    nsCOMPtr<nsIContent> conditions;
                    nsXULContentUtils::FindChildByTag(rulenode,
                                                      kNameSpaceID_XUL,
                                                      nsGkAtoms::conditions,
                                                      getter_AddRefs(conditions));

                    if (conditions) {
                        // create a new queryset if one hasn't been created already
                        if (hasQuerySet) {
                            aQuerySet = new nsTemplateQuerySet(++*aPriority);
                            if (! aQuerySet)
                                return NS_ERROR_OUT_OF_MEMORY;

                            if (!mQuerySets.AppendElement(aQuerySet)) {
                                delete aQuerySet;
                                return NS_ERROR_OUT_OF_MEMORY;
                            }
                        }

                        nsCOMPtr<nsIAtom> tag;
                        DetermineRDFQueryRef(conditions, getter_AddRefs(tag));
                        if (tag)
                            aQuerySet->SetTag(tag);

                        hasQuerySet = true;

                        nsCOMPtr<nsIDOMNode> conditionsnode(do_QueryInterface(conditions));

                        aQuerySet->mQueryNode = conditions;
                        rv = mQueryProcessor->CompileQuery(this, conditionsnode,
                                                           mRefVariable,
                                                           memberVariable,
                                                           getter_AddRefs(aQuerySet->mCompiledQuery));
                        if (NS_FAILED(rv))
                            return rv;

                        if (aQuerySet->mCompiledQuery) {
                            rv = CompileExtendedQuery(rulenode, action, memberVariable,
                                                      aQuerySet);
                            if (NS_FAILED(rv))
                                return rv;

                            *aCanUseTemplate = true;
                        }
                    }
                }
            }
            else {
                if (hasQuery)
                    continue;

                // a new queryset must always be created in this case
                if (hasQuerySet) {
                    aQuerySet = new nsTemplateQuerySet(++*aPriority);
                    if (! aQuerySet)
                        return NS_ERROR_OUT_OF_MEMORY;

                    if (!mQuerySets.AppendElement(aQuerySet)) {
                        delete aQuerySet;
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                }

                hasQuerySet = true;

                rv = CompileSimpleQuery(rulenode, aQuerySet, aCanUseTemplate);
                if (NS_FAILED(rv))
                    return rv;
            }

            hasRule = true;
        }
        else if (ni->Equals(nsGkAtoms::query, kNameSpaceID_XUL)) {
            if (hasQuery)
              continue;

            aQuerySet->mQueryNode = rulenode;
            hasQuery = true;
        }
        else if (ni->Equals(nsGkAtoms::action, kNameSpaceID_XUL)) {
            // the query must appear before the action
            if (! hasQuery)
                continue;

            nsCOMPtr<nsIAtom> tag;
            DetermineRDFQueryRef(aQuerySet->mQueryNode, getter_AddRefs(tag));
            if (tag)
                aQuerySet->SetTag(tag);

            nsCOMPtr<nsIAtom> memberVariable = mMemberVariable;
            if (!memberVariable) {
                memberVariable = DetermineMemberVariable(rulenode);
                if (!memberVariable) {
                    nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_NO_MEMBERVAR);
                    continue;
                }
            }

            nsCOMPtr<nsIDOMNode> query(do_QueryInterface(aQuerySet->mQueryNode));

            rv = mQueryProcessor->CompileQuery(this, query,
                                               mRefVariable, memberVariable,
                                               getter_AddRefs(aQuerySet->mCompiledQuery));

            if (aQuerySet->mCompiledQuery) {
                nsTemplateRule* rule = aQuerySet->NewRule(aTemplate, rulenode, aQuerySet);
                if (! rule)
                    return NS_ERROR_OUT_OF_MEMORY;

                rule->SetVars(mRefVariable, memberVariable);

                *aCanUseTemplate = true;

                return NS_OK;
            }
        }
    }

    if (! hasRule && ! hasQuery && ! hasQuerySet) {
        // if no rules are specified in the template, then the contents of the
        // <template> tag are the one-and-only template.
        rv = CompileSimpleQuery(aTemplate, aQuerySet, aCanUseTemplate);
     }

    return rv;
}

nsresult
nsXULTemplateBuilder::CompileExtendedQuery(nsIContent* aRuleElement,
                                           nsIContent* aActionElement,
                                           nsIAtom* aMemberVariable,
                                           nsTemplateQuerySet* aQuerySet)
{
    // Compile an "extended" <template> rule. An extended rule may have
    // a <conditions> child, an <action> child, and a <bindings> child.
    nsresult rv;

    nsTemplateRule* rule = aQuerySet->NewRule(aRuleElement, aActionElement, aQuerySet);
    if (! rule)
         return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIContent> conditions;
    nsXULContentUtils::FindChildByTag(aRuleElement,
                                      kNameSpaceID_XUL,
                                      nsGkAtoms::conditions,
                                      getter_AddRefs(conditions));

    // allow the conditions to be placed directly inside the rule
    if (!conditions)
        conditions = aRuleElement;
  
    rv = CompileConditions(rule, conditions);
    // If the rule compilation failed, then we have to bail.
    if (NS_FAILED(rv)) {
        aQuerySet->RemoveRule(rule);
        return rv;
    }

    rule->SetVars(mRefVariable, aMemberVariable);

    // If we've got bindings, add 'em.
    nsCOMPtr<nsIContent> bindings;
    nsXULContentUtils::FindChildByTag(aRuleElement,
                                      kNameSpaceID_XUL,
                                      nsGkAtoms::bindings,
                                      getter_AddRefs(bindings));

    // allow bindings to be placed directly inside rule
    if (!bindings)
        bindings = aRuleElement;

    rv = CompileBindings(rule, bindings);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

already_AddRefed<nsIAtom>
nsXULTemplateBuilder::DetermineMemberVariable(nsIContent* aElement)
{
    // recursively iterate over the children looking for an element
    // with uri="?..."
    for (nsIContent* child = aElement->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
        nsAutoString uri;
        child->GetAttr(kNameSpaceID_None, nsGkAtoms::uri, uri);
        if (!uri.IsEmpty() && uri[0] == PRUnichar('?')) {
            return NS_NewAtom(uri);
        }

        nsCOMPtr<nsIAtom> result = DetermineMemberVariable(child);
        if (result) {
            return result.forget();
        }
    }

    return nullptr;
}

void
nsXULTemplateBuilder::DetermineRDFQueryRef(nsIContent* aQueryElement, nsIAtom** aTag)
{
    // check for a tag
    nsCOMPtr<nsIContent> content;
    nsXULContentUtils::FindChildByTag(aQueryElement,
                                      kNameSpaceID_XUL,
                                      nsGkAtoms::content,
                                      getter_AddRefs(content));

    if (! content) {
        // look for older treeitem syntax as well
        nsXULContentUtils::FindChildByTag(aQueryElement,
                                          kNameSpaceID_XUL,
                                          nsGkAtoms::treeitem,
                                          getter_AddRefs(content));
    }

    if (content) {
        nsAutoString uri;
        content->GetAttr(kNameSpaceID_None, nsGkAtoms::uri, uri);

        if (!uri.IsEmpty())
            mRefVariable = do_GetAtom(uri);

        nsAutoString tag;
        content->GetAttr(kNameSpaceID_None, nsGkAtoms::tag, tag);

        if (!tag.IsEmpty())
            *aTag = NS_NewAtom(tag).get();
    }
}

nsresult
nsXULTemplateBuilder::CompileSimpleQuery(nsIContent* aRuleElement,
                                         nsTemplateQuerySet* aQuerySet,
                                         bool* aCanUseTemplate)
{
    // compile a simple query, which is a query with no <query> or
    // <conditions>. This means that a default query is used.
    nsCOMPtr<nsIDOMNode> query(do_QueryInterface(aRuleElement));

    nsCOMPtr<nsIAtom> memberVariable;
    if (mMemberVariable)
        memberVariable = mMemberVariable;
    else
        memberVariable = do_GetAtom("rdf:*");

    // since there is no <query> node for a simple query, the query node will
    // be either the <rule> node if multiple rules are used, or the <template> node.
    aQuerySet->mQueryNode = aRuleElement;
    nsresult rv = mQueryProcessor->CompileQuery(this, query,
                                                mRefVariable, memberVariable,
                                                getter_AddRefs(aQuerySet->mCompiledQuery));
    if (NS_FAILED(rv))
        return rv;

    if (! aQuerySet->mCompiledQuery) {
        *aCanUseTemplate = false;
        return NS_OK;
    }

    nsTemplateRule* rule = aQuerySet->NewRule(aRuleElement, aRuleElement, aQuerySet);
    if (! rule)
        return NS_ERROR_OUT_OF_MEMORY;

    rule->SetVars(mRefVariable, memberVariable);

    nsAutoString tag;
    aRuleElement->GetAttr(kNameSpaceID_None, nsGkAtoms::parent, tag);

    if (!tag.IsEmpty()) {
        nsCOMPtr<nsIAtom> tagatom = do_GetAtom(tag);
        aQuerySet->SetTag(tagatom);
    }

    *aCanUseTemplate = true;

    return AddSimpleRuleBindings(rule, aRuleElement);
}

nsresult
nsXULTemplateBuilder::CompileConditions(nsTemplateRule* aRule,
                                        nsIContent* aCondition)
{
    nsAutoString tag;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::parent, tag);

    if (!tag.IsEmpty()) {
        nsCOMPtr<nsIAtom> tagatom = do_GetAtom(tag);
        aRule->SetTag(tagatom);
    }

    nsTemplateCondition* currentCondition = nullptr;

    for (nsIContent* node = aCondition->GetFirstChild();
         node;
         node = node->GetNextSibling()) {

        if (node->NodeInfo()->Equals(nsGkAtoms::where, kNameSpaceID_XUL)) {
            nsresult rv = CompileWhereCondition(aRule, node, &currentCondition);
            if (NS_FAILED(rv))
                return rv;
        }
    }

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileWhereCondition(nsTemplateRule* aRule,
                                            nsIContent* aCondition,
                                            nsTemplateCondition** aCurrentCondition)
{
    // Compile a <where> condition, which must be of the form:
    //
    //   <where subject="?var1|string" rel="relation" value="?var2|string" />
    //
    //    The value of rel may be:
    //      equal - subject must be equal to object
    //      notequal - subject must not be equal to object
    //      less - subject must be less than object
    //      greater - subject must be greater than object
    //      startswith - subject must start with object
    //      endswith - subject must end with object
    //      contains - subject must contain object
    //    Comparisons are done as strings unless the subject is an integer.

    // subject
    nsAutoString subject;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::subject, subject);
    if (subject.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_WHERE_NO_SUBJECT);
        return NS_OK;
    }

    nsCOMPtr<nsIAtom> svar;
    if (subject[0] == PRUnichar('?'))
        svar = do_GetAtom(subject);

    nsAutoString relstring;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, relstring);
    if (relstring.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_WHERE_NO_RELATION);
        return NS_OK;
    }

    // object
    nsAutoString value;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::value, value);
    if (value.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_WHERE_NO_VALUE);
        return NS_OK;
    }

    // multiple
    bool shouldMultiple =
      aCondition->AttrValueIs(kNameSpaceID_None, nsGkAtoms::multiple,
                              nsGkAtoms::_true, eCaseMatters);

    nsCOMPtr<nsIAtom> vvar;
    if (!shouldMultiple && (value[0] == PRUnichar('?'))) {
        vvar = do_GetAtom(value);
    }

    // ignorecase
    bool shouldIgnoreCase =
      aCondition->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ignorecase,
                              nsGkAtoms::_true, eCaseMatters);

    // negate
    bool shouldNegate =
      aCondition->AttrValueIs(kNameSpaceID_None, nsGkAtoms::negate,
                              nsGkAtoms::_true, eCaseMatters);

    nsTemplateCondition* condition;

    if (svar && vvar) {
        condition = new nsTemplateCondition(svar, relstring, vvar,
                                            shouldIgnoreCase, shouldNegate);
    }
    else if (svar && !value.IsEmpty()) {
        condition = new nsTemplateCondition(svar, relstring, value,
                                            shouldIgnoreCase, shouldNegate, shouldMultiple);
    }
    else if (vvar) {
        condition = new nsTemplateCondition(subject, relstring, vvar,
                                            shouldIgnoreCase, shouldNegate);
    }
    else {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_WHERE_NO_VAR);
        return NS_OK;
    }

    if (! condition)
        return NS_ERROR_OUT_OF_MEMORY;

    if (*aCurrentCondition) {
        (*aCurrentCondition)->SetNext(condition);
    }
    else {
        aRule->SetCondition(condition);
    }

    *aCurrentCondition = condition;

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileBindings(nsTemplateRule* aRule, nsIContent* aBindings)
{
    // Add an extended rule's bindings.
    nsresult rv;

    for (nsIContent* binding = aBindings->GetFirstChild();
         binding;
         binding = binding->GetNextSibling()) {

        if (binding->NodeInfo()->Equals(nsGkAtoms::binding,
                                        kNameSpaceID_XUL)) {
            rv = CompileBinding(aRule, binding);
            if (NS_FAILED(rv))
                return rv;
        }
    }

    aRule->AddBindingsToQueryProcessor(mQueryProcessor);

    return NS_OK;
}


nsresult
nsXULTemplateBuilder::CompileBinding(nsTemplateRule* aRule,
                                     nsIContent* aBinding)
{
    // Compile a <binding> "condition", which must be of the form:
    //
    //   <binding subject="?var1"
    //            predicate="resource"
    //            object="?var2" />
    //
    // XXXwaterson Some day it would be cool to allow the 'predicate'
    // to be bound to a variable.

    // subject
    nsAutoString subject;
    aBinding->GetAttr(kNameSpaceID_None, nsGkAtoms::subject, subject);
    if (subject.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BINDING_BAD_SUBJECT);
        return NS_OK;
    }

    nsCOMPtr<nsIAtom> svar;
    if (subject[0] == PRUnichar('?')) {
        svar = do_GetAtom(subject);
    }
    else {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BINDING_BAD_SUBJECT);
        return NS_OK;
    }

    // predicate
    nsAutoString predicate;
    aBinding->GetAttr(kNameSpaceID_None, nsGkAtoms::predicate, predicate);
    if (predicate.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BINDING_BAD_PREDICATE);
        return NS_OK;
    }

    // object
    nsAutoString object;
    aBinding->GetAttr(kNameSpaceID_None, nsGkAtoms::object, object);

    if (object.IsEmpty()) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BINDING_BAD_OBJECT);
        return NS_OK;
    }

    nsCOMPtr<nsIAtom> ovar;
    if (object[0] == PRUnichar('?')) {
        ovar = do_GetAtom(object);
    }
    else {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_BINDING_BAD_OBJECT);
        return NS_OK;
    }

    return aRule->AddBinding(svar, predicate, ovar);
}

nsresult
nsXULTemplateBuilder::AddSimpleRuleBindings(nsTemplateRule* aRule,
                                            nsIContent* aElement)
{
    // Crawl the content tree of a "simple" rule, adding a variable
    // assignment for any attribute whose value is "rdf:".

    nsAutoTArray<nsIContent*, 8> elements;

    if (elements.AppendElement(aElement) == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;

    while (elements.Length()) {
        // Pop the next element off the stack
        uint32_t i = elements.Length() - 1;
        nsIContent* element = elements[i];
        elements.RemoveElementAt(i);

        // Iterate through its attributes, looking for substitutions
        // that we need to add as bindings.
        uint32_t count = element->GetAttrCount();

        for (i = 0; i < count; ++i) {
            const nsAttrName* name = element->GetAttrNameAt(i);

            if (!name->Equals(nsGkAtoms::id, kNameSpaceID_None) &&
                !name->Equals(nsGkAtoms::uri, kNameSpaceID_None)) {
                nsAutoString value;
                element->GetAttr(name->NamespaceID(), name->LocalName(), value);

                // Scan the attribute for variables, adding a binding for
                // each one.
                ParseAttribute(value, AddBindingsFor, nullptr, aRule);
            }
        }

        // Push kids onto the stack, and search them next.
        for (nsIContent* child = element->GetLastChild();
             child;
             child = child->GetPreviousSibling()) {

            if (!elements.AppendElement(child))
                return NS_ERROR_OUT_OF_MEMORY;
        }
    }

    aRule->AddBindingsToQueryProcessor(mQueryProcessor);

    return NS_OK;
}

void
nsXULTemplateBuilder::AddBindingsFor(nsXULTemplateBuilder* aThis,
                                     const nsAString& aVariable,
                                     void* aClosure)
{
    // We should *only* be recieving "rdf:"-style variables. Make
    // sure...
    if (!StringBeginsWith(aVariable, NS_LITERAL_STRING("rdf:")))
        return;

    nsTemplateRule* rule = static_cast<nsTemplateRule*>(aClosure);

    nsCOMPtr<nsIAtom> var = do_GetAtom(aVariable);

    // Strip it down to the raw RDF property by clobbering the "rdf:"
    // prefix
    nsAutoString property;
    property.Assign(Substring(aVariable, uint32_t(4), aVariable.Length() - 4));

    if (! rule->HasBinding(rule->GetMemberVariable(), property, var))
        // In the simple syntax, the binding is always from the
        // member variable, through the property, to the target.
        rule->AddBinding(rule->GetMemberVariable(), property, var);
}


nsresult
nsXULTemplateBuilder::IsSystemPrincipal(nsIPrincipal *principal, bool *result)
{
  if (!gSystemPrincipal)
    return NS_ERROR_UNEXPECTED;

  *result = (principal == gSystemPrincipal);
  return NS_OK;
}

bool
nsXULTemplateBuilder::IsActivated(nsIRDFResource *aResource)
{
    for (ActivationEntry *entry = mTop;
         entry != nullptr;
         entry = entry->mPrevious) {
        if (entry->mResource == aResource)
            return true;
    }
    return false;
}

nsresult
nsXULTemplateBuilder::GetResultResource(nsIXULTemplateResult* aResult,
                                        nsIRDFResource** aResource)
{
    // get the resource for a result by checking its resource property. If it
    // is not set, check the id. This allows non-chrome implementations to
    // avoid having to use RDF.
    nsresult rv = aResult->GetResource(aResource);
    if (NS_FAILED(rv))
        return rv;

    if (! *aResource) {
        nsAutoString id;
        rv = aResult->GetId(id);
        if (NS_FAILED(rv))
            return rv;

        return gRDFService->GetUnicodeResource(id, aResource);
    }

    return rv;
}


void
nsXULTemplateBuilder::OutputMatchToLog(nsIRDFResource* aId,
                                       nsTemplateMatch* aMatch,
                                       bool aIsNew)
{
    int32_t priority = aMatch->QuerySetPriority() + 1;
    int32_t activePriority = -1;

    nsAutoString msg;

    nsAutoString templateid;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::id, templateid);
    msg.AppendLiteral("In template");
    if (!templateid.IsEmpty()) {
        msg.AppendLiteral(" with id ");
        msg.Append(templateid);
    }

    nsAutoString refstring;
    aMatch->mResult->GetBindingFor(mRefVariable, refstring);
    if (!refstring.IsEmpty()) {
        msg.AppendLiteral(" using ref ");
        msg.Append(refstring);
    }

    msg.AppendLiteral("\n    ");

    nsTemplateMatch* match = nullptr;
    if (mMatchMap.Get(aId, &match)){
        while (match) {
            if (match == aMatch)
                break;
            if (match->IsActive() &&
                match->GetContainer() == aMatch->GetContainer()) {
                activePriority = match->QuerySetPriority() + 1;
                break;
            }
            match = match->mNext;
        }
    }

    if (aMatch->IsActive()) {
        if (aIsNew) {
            msg.AppendLiteral("New active result for query ");
            msg.AppendInt(priority);
            msg.AppendLiteral(" matching rule ");
            msg.AppendInt(aMatch->RuleIndex() + 1);
        }
        else {
            msg.AppendLiteral("Removed active result for query ");
            msg.AppendInt(priority);
            if (activePriority > 0) {
                msg.AppendLiteral(" (new active query is ");
                msg.AppendInt(activePriority);
                msg.Append(')');
            }
            else {
                msg.AppendLiteral(" (no new active query)");
            }
        }
    }
    else {
        if (aIsNew) {
            msg.AppendLiteral("New inactive result for query ");
            msg.AppendInt(priority);
            if (activePriority > 0) {
                msg.AppendLiteral(" (overridden by query ");
                msg.AppendInt(activePriority);
                msg.Append(')');
            }
            else {
                msg.AppendLiteral(" (didn't match a rule)");
            }
        }
        else {
            msg.AppendLiteral("Removed inactive result for query ");
            msg.AppendInt(priority);
            if (activePriority > 0) {
                msg.AppendLiteral(" (active query is ");
                msg.AppendInt(activePriority);
                msg.Append(')');
            }
            else {
                msg.AppendLiteral(" (no active query)");
            }
        }
    }

    nsAutoString idstring;
    nsXULContentUtils::GetTextForNode(aId, idstring);
    msg.AppendLiteral(": ");
    msg.Append(idstring);

    nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs)
      cs->LogStringMessage(msg.get());
}

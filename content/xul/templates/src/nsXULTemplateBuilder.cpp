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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Churchill <rjc@netscape.com>
 *   David Hyatt <hyatt@netscape.com>
 *   Chris Waterson <waterson@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Joe Hewitt <hewitt@netscape.com>
 *   Neil Deakin <enndeakin@sympatico.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*

  Builds content from a datasource using the XUL <template> tag.

  TO DO

  . Fix ContentTagTest's location in the network construction

  To turn on logging for this module, set:

    NSPR_LOG_MODULES nsXULTemplateBuilder:5

 */

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsFixedSizeAllocator.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsBindingManager.h"
#include "nsIDOMNodeList.h"
#include "nsINameSpaceManager.h"
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
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsIXPConnect.h"
#include "nsContentCID.h"
#include "nsRDFCID.h"
#include "nsXULContentUtils.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsGkAtoms.h"
#include "nsXULElement.h"
#include "jsapi.h"
#include "prlog.h"
#include "rdf.h"
#include "pldhash.h"
#include "plhash.h"
#include "nsIDOMClassInfo.h"

#include "nsNetUtil.h"
#include "nsXULTemplateBuilder.h"
#include "nsXULTemplateQueryProcessorRDF.h"

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

#ifdef PR_LOGGING
PRLogModuleInfo* gXULTemplateLog;
#endif

#define NS_QUERY_PROCESSOR_CONTRACTID_PREFIX "@mozilla.org/xul/xul-query-processor;1?name="

//----------------------------------------------------------------------
//
// nsXULTemplateBuilder methods
//

nsXULTemplateBuilder::nsXULTemplateBuilder(void)
    : mDB(nsnull),
      mCompDB(nsnull),
      mRoot(nsnull),
      mQueriesCompiled(PR_FALSE),
      mFlags(0),
      mTop(nsnull)
{
}

static PLDHashOperator
DestroyMatchList(nsISupports* aKey, nsTemplateMatch* aMatch, void* aContext)
{
    nsFixedSizeAllocator* pool = NS_STATIC_CAST(nsFixedSizeAllocator *, aContext);

    // delete all the matches in the list
    while (aMatch) {
        if (aMatch->mResult)
            aMatch->mResult->HasBeenRemoved();

        nsTemplateMatch* next = aMatch->mNext;
        nsTemplateMatch::Destroy(*pool, aMatch);
        aMatch = next;
    }

    return PL_DHASH_NEXT;
}

nsXULTemplateBuilder::~nsXULTemplateBuilder(void)
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(gRDFContainerUtils);
        NS_IF_RELEASE(gSystemPrincipal);
        NS_IF_RELEASE(gScriptSecurityManager);
    }

    Uninit(PR_TRUE);
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
    }

#ifdef PR_LOGGING
    if (! gXULTemplateLog)
        gXULTemplateLog = PR_NewLogModule("nsXULTemplateBuilder");
#endif

    if (!mMatchMap.IsInitialized() && !mMatchMap.Init())
        return NS_ERROR_OUT_OF_MEMORY;

    const size_t bucketsizes[] = { sizeof(nsTemplateMatch) };
    return mPool.Init("nsXULTemplateBuilder", bucketsizes, 1, 256);
}


void
nsXULTemplateBuilder::Uninit(PRBool aIsFinal)
{
    if (mQueryProcessor)
        mQueryProcessor->Done();

    for (PRInt32 q = mQuerySets.Length() - 1; q >= 0; q--) {
        nsTemplateQuerySet* qs = mQuerySets[q];
        delete qs;
    }

    mQuerySets.Clear();

    mMatchMap.EnumerateRead(DestroyMatchList, &mPool);
    mMatchMap.Clear();

    mRootResult = nsnull;
    mRefVariable = nsnull;
    mMemberVariable = nsnull;

    mQueriesCompiled = PR_FALSE;
}

static PLDHashOperator
TraverseMatchList(nsISupports* aKey, nsTemplateMatch* aMatch, void* aContext)
{
    nsCycleCollectionTraversalCallback *cb =
        NS_STATIC_CAST(nsCycleCollectionTraversalCallback*, aContext);

    cb->NoteXPCOMChild(aKey);
    nsTemplateMatch* match = aMatch;
    while (match) {
        cb->NoteXPCOMChild(match->GetContainer());
        cb->NoteXPCOMChild(match->mResult);
        match = match->mNext;
    }

    return PL_DHASH_NEXT;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULTemplateBuilder)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDB)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCompDB)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULTemplateBuilder)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDB)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCompDB)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRoot)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRootResult)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mListeners)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mQueryProcessor)
    if (tmp->mMatchMap.IsInitialized())
        tmp->mMatchMap.EnumerateRead(TraverseMatchList, &cb);
    {
      PRUint32 i, count = tmp->mQuerySets.Length();
      for (i = 0; i < count; ++i) {
        nsTemplateQuerySet *set = tmp->mQuerySets[i];
        cb.NoteXPCOMChild(set->mQueryNode);
        cb.NoteXPCOMChild(set->mCompiledQuery);
        PRUint16 j, rulesCount = set->RuleCount();
        for (j = 0; j < rulesCount; ++j) {
          set->GetRuleAt(j)->Traverse(cb);
        }
      }
    }
    tmp->Traverse(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsXULTemplateBuilder,
                                          nsIXULTemplateBuilder)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsXULTemplateBuilder,
                                           nsIXULTemplateBuilder)

NS_INTERFACE_MAP_BEGIN(nsXULTemplateBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIXULTemplateBuilder)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULTemplateBuilder)
  NS_INTERFACE_MAP_ENTRY_DOM_CLASSINFO(XULTemplateBuilder)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsXULTemplateBuilder)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
//
// nsIXULTemplateBuilder methods
//

NS_IMETHODIMP
nsXULTemplateBuilder::GetRoot(nsIDOMElement** aResult)
{
    return CallQueryInterface(mRoot, aResult);
}

NS_IMETHODIMP
nsXULTemplateBuilder::GetDatabase(nsIRDFCompositeDataSource** aResult)
{
    NS_IF_ADDREF(*aResult = mCompDB.get());
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

    PRInt32 count = mQuerySets.Length();
    for (PRInt32 q = 0; q < count; q++) {
        nsTemplateQuerySet* queryset = mQuerySets[q];

        PRInt16 rulecount = queryset->RuleCount();
        for (PRInt16 r = 0; r < rulecount; r++) {
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
    PRInt32 i;

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

    nsCOMPtr<nsISimpleEnumerator> dslist;
    rv = mCompDB->GetDataSources(getter_AddRefs(dslist));
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    nsCOMPtr<nsISupports> next;
    nsCOMPtr<nsIRDFRemoteDataSource> rds;

    while(NS_SUCCEEDED(dslist->HasMoreElements(&hasMore)) && hasMore) {
        dslist->GetNext(getter_AddRefs(next));
        if (next && (rds = do_QueryInterface(next))) {
            rds->Refresh(PR_FALSE);
        }
    }

    // XXXbsmedberg: it would be kinda nice to install an async nsIRDFXMLSink
    // observer and call rebuild() once the load is complete. See bug 254600.

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::Init(nsIContent* aElement)
{
    NS_PRECONDITION(aElement, "null ptr");
    mRoot = aElement;

    nsCOMPtr<nsIDocument> doc = mRoot->GetDocument();
    NS_ASSERTION(doc, "element has no document");
    if (! doc)
        return NS_ERROR_UNEXPECTED;

    nsresult rv = LoadDataSources(doc);

    if (NS_SUCCEEDED(rv)) {
        // Add ourselves as a document observer
        doc->AddObserver(this);
    }

    // create the query processor. The querytype attribute on the root element
    // may be used to create one of a specific type.

    // XXX should non-chrome be restricted to specific names?

    nsAutoString type;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::querytype, type);

    if (type.IsEmpty() || type.EqualsLiteral("rdf")) {
        mQueryProcessor = new nsXULTemplateQueryProcessorRDF();
        if (! mQueryProcessor)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        nsCAutoString cid(NS_QUERY_PROCESSOR_CONTRACTID_PREFIX);
        AppendUTF16toUTF8(type, cid);
        mQueryProcessor = do_CreateInstance(cid.get(), &rv);
        if (!mQueryProcessor) {
            // XXXndeakin should log an error here
            return rv;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsXULTemplateBuilder::CreateContents(nsIContent* aElement)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::HasGeneratedContent(nsIRDFResource* aResource,
                                          nsIAtom* aTag,
                                          PRBool* aGenerated)
{
    *aGenerated = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::AddResult(nsIXULTemplateResult* aResult,
                                nsIDOMNode* aQueryNode)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aQueryNode);

    return UpdateResult(nsnull, aResult, aQueryNode);
}

NS_IMETHODIMP
nsXULTemplateBuilder::RemoveResult(nsIXULTemplateResult* aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    return UpdateResult(aResult, nsnull, nsnull);
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

    nsresult rv = UpdateResult(aOldResult, nsnull, nsnull);
    if (NS_FAILED(rv))
        return rv;

    return UpdateResult(nsnull, aNewResult, aQueryNode);
}

nsresult
nsXULTemplateBuilder::UpdateResult(nsIXULTemplateResult* aOldResult,
                                   nsIXULTemplateResult* aNewResult,
                                   nsIDOMNode* aQueryNode)
{
    PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
           ("nsXULTemplateBuilder::UpdateResult %p %p %p",
           aOldResult, aNewResult, aQueryNode));

    // get the containers where content may be inserted. If
    // GetInsertionLocations returns false, no container has generated
    // any content yet so new content should not be generated either. This
    // will be false if the result applies to content that is in a closed menu
    // or treeitem for example.

    nsAutoPtr<nsCOMArray<nsIContent> > insertionPoints;
    PRBool mayReplace = GetInsertionLocations(aOldResult ? aOldResult : aNewResult,
                                              getter_Transfers(insertionPoints));
    if (! mayReplace)
        return NS_OK;

    nsresult rv = NS_OK;

    nsCOMPtr<nsIRDFResource> oldId, newId;
    nsTemplateQuerySet* queryset = nsnull;

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

        PRInt32 count = mQuerySets.Length();
        for (PRInt32 q = 0; q < count; q++) {
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
        PRUint32 count = insertionPoints->Count();
        for (PRUint32 t = 0; t < count; t++) {
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
                                     oldId, newId, nsnull);
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
    nsresult rv = NS_OK;
    PRInt16 ruleindex;
    nsTemplateRule* matchedrule = nsnull;
    nsTemplateMatch* acceptedmatch = nsnull, * removedmatch = nsnull;
    nsTemplateMatch* replacedmatch = nsnull;

    if (aOldResult) {
        nsTemplateMatch* firstmatch;
        if (mMatchMap.Get(aOldId, &firstmatch)) {
            nsTemplateMatch* oldmatch = firstmatch;
            nsTemplateMatch* prevmatch = nsnull;

            // look for the right match if there was more than one
            while (oldmatch && (oldmatch->mResult != aOldResult)) {
                prevmatch = oldmatch;
                oldmatch = oldmatch->mNext;
            }

            if (oldmatch) {
                nsTemplateMatch* findmatch = oldmatch->mNext;

                // keep reference so that linked list can be hooked up at
                // the end in case an error occurs
                nsTemplateMatch* nextmatch = findmatch;

                if (oldmatch->IsActive()) {
                    // the match being removed is the active match, so scan
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
                        if (!mMatchMap.Put(aOldId, oldmatch->mNext))
                            return NS_ERROR_OUT_OF_MEMORY;
                    }
                    else {
                        mMatchMap.Remove(aOldId);
                    }
                }

                if (prevmatch)
                    prevmatch->mNext = nextmatch;

                removedmatch = oldmatch;
            }
        }
    }

    if (aNewResult) {
        // only allow a result to be inserted into containers with a matching tag
        nsIAtom* tag = aQuerySet->GetTag();
        if (aInsertionPoint && tag && tag != aInsertionPoint->Tag())
            return NS_OK;

        PRInt32 findpriority = aQuerySet->Priority();

        nsTemplateMatch *newmatch =
            nsTemplateMatch::Create(mPool, findpriority,
                                    aNewResult, aInsertionPoint);
        if (!newmatch)
            return NS_ERROR_OUT_OF_MEMORY;

        nsTemplateMatch* firstmatch;
        if (mMatchMap.Get(aNewId, &firstmatch)) {
            PRBool hasEarlierActiveMatch = PR_FALSE;

            // scan through the existing matches to find where the new one
            // should be inserted. oldmatch will be set to the old match for
            // the same query and prevmatch will be set to the match before it
            nsTemplateMatch* prevmatch = nsnull;
            nsTemplateMatch* oldmatch = firstmatch;
            while (oldmatch) {
                // break out once we've reached a query in the list with a
                // higher priority. The new match will be inserted at this
                // location so that the match list is sorted by priority
                PRInt32 priority = oldmatch->QuerySetPriority();
                if (priority > findpriority) {
                    oldmatch = nsnull;
                    break;
                }

                if (oldmatch->GetContainer() == aInsertionPoint) {
                    if (priority == findpriority)
                        break;

                    if (oldmatch->IsActive())
                        hasEarlierActiveMatch = PR_TRUE;
                }

                prevmatch = oldmatch;
                oldmatch = oldmatch->mNext;
            }

            if (oldmatch)
                newmatch->mNext = oldmatch->mNext;
            else if (prevmatch)
                newmatch->mNext = prevmatch->mNext;
            else
                newmatch->mNext = firstmatch;

            // if the active match was earlier than the new match, the new
            // match won't become active but it should still be added to the
            // list in case it will match later
            if (! hasEarlierActiveMatch) {
                // check if the new result matches the rules
                rv = DetermineMatchedRule(aInsertionPoint, newmatch->mResult,
                                          aQuerySet, &matchedrule, &ruleindex);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(mPool, newmatch);
                    return rv;
                }

                if (matchedrule) {
                    rv = newmatch->RuleMatched(aQuerySet,
                                               matchedrule, ruleindex,
                                               newmatch->mResult);
                    if (NS_FAILED(rv)) {
                        nsTemplateMatch::Destroy(mPool, newmatch);
                        return rv;
                    }

                    acceptedmatch = newmatch;

                    // clear the matched state of the later results
                    // for the same container
                    nsTemplateMatch* clearmatch = newmatch->mNext;
                    while (clearmatch) {
                        if (clearmatch->GetContainer() == aInsertionPoint)
                            clearmatch->SetInactive();
                        clearmatch = clearmatch->mNext;
                    }
                }
                else if (oldmatch && oldmatch->IsActive()) {
                    // the result didn't match the rules, so look for a later
                    // one. However, only do this if the old match was the
                    // active match

                    newmatch = newmatch->mNext;
                    while (newmatch) {
                        if (newmatch->GetContainer() == aInsertionPoint) {
                            rv = DetermineMatchedRule(aInsertionPoint, newmatch->mResult,
                                                      aQuerySet, &matchedrule, &ruleindex);
                            if (NS_FAILED(rv)) {
                                nsTemplateMatch::Destroy(mPool, newmatch);
                                return rv;
                            }

                            if (matchedrule) {
                                rv = newmatch->RuleMatched(aQuerySet,
                                                           matchedrule, ruleindex,
                                                           newmatch->mResult);
                                if (NS_FAILED(rv)) {
                                    nsTemplateMatch::Destroy(mPool, newmatch);
                                    return rv;
                                }

                                acceptedmatch = newmatch;
                                break;
                            }
                        }

                        newmatch = newmatch->mNext;
                    }
                }

                // put the match in the map if there isn't an earlier match
                if (! prevmatch) {
                    if (!mMatchMap.Put(aNewId, newmatch)) {
                        nsTemplateMatch::Destroy(mPool, newmatch);
                        return rv;
                    }
                }

                if (oldmatch)
                    replacedmatch = oldmatch;
            }

            // hook up the match last in case an error occurs
            if (prevmatch)
                prevmatch->mNext = newmatch;
        }
        else {
            // the id is not used in a match yet so add a new match
            rv = DetermineMatchedRule(aInsertionPoint, aNewResult,
                                      aQuerySet, &matchedrule, &ruleindex);
            if (NS_FAILED(rv)) {
                nsTemplateMatch::Destroy(mPool, newmatch);
                return rv;
            }

            if (matchedrule) {
                rv = newmatch->RuleMatched(aQuerySet, matchedrule,
                                           ruleindex, aNewResult);
                if (NS_FAILED(rv)) {
                    nsTemplateMatch::Destroy(mPool, newmatch);
                    return rv;
                }

                acceptedmatch = newmatch;
            }

            if (!mMatchMap.Put(aNewId, newmatch)) {
                nsTemplateMatch::Destroy(mPool, newmatch);
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    if (replacedmatch) {
        // delete a replaced match
        rv = ReplaceMatch(replacedmatch->mResult, nsnull, nsnull, aInsertionPoint);

        replacedmatch->mResult->HasBeenRemoved();
        nsTemplateMatch::Destroy(mPool, replacedmatch);
    }

    // remove the content generated for the old result and add the content for
    // the new result if it matched a rule
    if (aOldResult || acceptedmatch)
        rv = ReplaceMatch(aOldResult, acceptedmatch, matchedrule, aInsertionPoint);

    if (removedmatch) {
        // delete the old match
        removedmatch->mResult->HasBeenRemoved();
        nsTemplateMatch::Destroy(mPool, removedmatch);
    }

    return rv;
}

NS_IMETHODIMP
nsXULTemplateBuilder::ResultBindingChanged(nsIXULTemplateResult* aResult)
{
    // A binding update is used when only the values of the bindings have
    // changed, so the same rule still applies. Just synchronize the content.
    // The new result will have the new values.
    NS_ENSURE_ARG_POINTER(aResult);

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

    *aResult = nsnull;

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
    *aResult = nsnull;
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

//----------------------------------------------------------------------
//
// nsIDocumentOberver interface
//

void
nsXULTemplateBuilder::AttributeChanged(nsIDocument* aDocument,
                                       nsIContent*  aContent,
                                       PRInt32      aNameSpaceID,
                                       nsIAtom*     aAttribute,
                                       PRInt32      aModType)
{
    if (aContent == mRoot && aNameSpaceID == kNameSpaceID_None) {
        // Check for a change to the 'ref' attribute on an atom, in which
        // case we may need to nuke and rebuild the entire content model
        // beneath the element.
        if (aAttribute == nsGkAtoms::ref)
            Rebuild();

        // Check for a change to the 'datasources' attribute. If so, setup
        // mDB by parsing the vew value and rebuild.
        else if (aAttribute == nsGkAtoms::datasources) {
            LoadDataSources(aDocument);
            Rebuild();
        }
    }
}

void
nsXULTemplateBuilder::ContentRemoved(nsIDocument* aDocument,
                                     nsIContent* aContainer,
                                     nsIContent* aChild,
                                     PRInt32 aIndexInContainer)
{
    if (mRoot && nsContentUtils::ContentIsDescendantOf(mRoot, aChild)) {
        nsRefPtr<nsXULTemplateBuilder> kungFuDeathGrip(this);

        if (mQueryProcessor)
            mQueryProcessor->Done();

        // use false since content is going away anyway
        Uninit(PR_FALSE);

        aDocument->RemoveObserver(this);

        nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(aDocument);
        if (xuldoc)
            xuldoc->SetTemplateBuilderFor(mRoot, nsnull);

        // clear the lazy state when removing content so that it will be
        // regenerated again if the content is reinserted
        nsXULElement *xulcontent = nsXULElement::FromContent(mRoot);
        if (xulcontent) {
            xulcontent->ClearLazyState(nsXULElement::eTemplateContentsBuilt);
            xulcontent->ClearLazyState(nsXULElement::eContainerContentsBuilt);
        }

        mDB = nsnull;
        mCompDB = nsnull;
        mRoot = nsnull;
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

    mDB = nsnull;
    mCompDB = nsnull;
    mRoot = nsnull;

    Uninit(PR_TRUE);
}




//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult
nsXULTemplateBuilder::LoadDataSources(nsIDocument* doc)
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");

    nsresult rv;

    if (mDB) {
        // we'll set it again later, after we create a new composite ds
        mDB = nsnull;
    }

    // create a database for the builder
    mCompDB = do_CreateInstance(NS_RDF_DATASOURCE_CONTRACTID_PREFIX "composite-datasource");

    if (! mCompDB) {
        NS_ERROR("unable to construct new composite data source");
        return NS_ERROR_UNEXPECTED;
    }

    // check for magical attributes. XXX move to ``flags''?
    if (mRoot->AttrValueIs(kNameSpaceID_None, nsGkAtoms::coalesceduplicatearcs,
                           nsGkAtoms::_false, eCaseMatters))
        mCompDB->SetCoalesceDuplicateArcs(PR_FALSE);
 
    if (mRoot->AttrValueIs(kNameSpaceID_None, nsGkAtoms::allownegativeassertions,
                           nsGkAtoms::_false, eCaseMatters))
        mCompDB->SetAllowNegativeAssertions(PR_FALSE);

    // Grab the doc's principal...
    nsIPrincipal *docPrincipal = doc->NodePrincipal();

    NS_ASSERTION(docPrincipal == mRoot->NodePrincipal(),
                 "Principal mismatch?  Which one to use?");

    PRBool isTrusted = PR_FALSE;
    rv = IsSystemPrincipal(docPrincipal, &isTrusted);
    if (NS_FAILED(rv))
        return rv;

    if (isTrusted) {
        // If we're a privileged (e.g., chrome) document, then add the
        // local store as the first data source in the db. Note that
        // we _might_ not be able to get a local store if we haven't
        // got a profile to read from yet.
        nsCOMPtr<nsIRDFDataSource> localstore;
        rv = gRDFService->GetDataSource("rdf:local-store", getter_AddRefs(localstore));
        if (NS_SUCCEEDED(rv)) {
            rv = mCompDB->AddDataSource(localstore);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add local store to db");
            if (NS_FAILED(rv))
                return rv;
        }
    }

    // Parse datasources: they are assumed to be a whitespace
    // separated list of URIs; e.g.,
    //
    //     rdf:bookmarks rdf:history http://foo.bar.com/blah.cgi?baz=9
    //
    nsIURI *docurl = doc->GetDocumentURI();

    nsAutoString datasources;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::datasources, datasources);

    PRUint32 first = 0;

    while(1) {
        while (first < datasources.Length() && nsCRT::IsAsciiSpace(datasources.CharAt(first)))
            ++first;

        if (first >= datasources.Length())
            break;

        PRUint32 last = first;
        while (last < datasources.Length() && !nsCRT::IsAsciiSpace(datasources.CharAt(last)))
            ++last;

        nsAutoString uriStr;
        datasources.Mid(uriStr, first, last - first);
        first = last + 1;

        // A special 'dummy' datasource
        if (uriStr.EqualsLiteral("rdf:null"))
            continue;

        // N.B. that `failure' (e.g., because it's an unknown
        // protocol) leaves uriStr unaltered.
        NS_MakeAbsoluteURI(uriStr, uriStr, docurl);

        if (!isTrusted) {
            // Our document is untrusted, so check to see if we can
            // load the datasource that they've asked for.
            nsCOMPtr<nsIURI> uri;
            rv = NS_NewURI(getter_AddRefs(uri), uriStr);
            if (NS_FAILED(rv) || !uri)
                continue; // Necko will barf if our URI is weird

            nsCOMPtr<nsIPrincipal> principal;
            rv = gScriptSecurityManager->GetCodebasePrincipal(uri, getter_AddRefs(principal));
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get codebase principal");
            if (NS_FAILED(rv))
                return rv;

            PRBool same;
            rv = docPrincipal->Equals(principal, &same);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to test same origin");
            if (NS_FAILED(rv))
                return rv;

            if (! same)
                continue;

            // If we get here, we've run the gauntlet, and the
            // datasource's URI has the same origin as our
            // document. Let it load!
        }

        nsCOMPtr<nsIRDFDataSource> ds;
        nsCAutoString uristrC;
        uristrC.AssignWithConversion(uriStr);

        rv = gRDFService->GetDataSource(uristrC.get(), getter_AddRefs(ds));

        if (NS_FAILED(rv)) {
            // This is only a warning because the data source may not
            // be accessible for any number of reasons, including
            // security, a bad URL, etc.
#ifdef DEBUG
            nsCAutoString msg;
            msg.Append("unable to load datasource '");
            msg.AppendWithConversion(uriStr);
            msg.Append('\'');
            NS_WARNING(msg.get());
#endif
            continue;
        }

        mCompDB->AddDataSource(ds);
    }

    // check if we were given an inference engine type
    nsAutoString infer;
    mRoot->GetAttr(kNameSpaceID_None, nsGkAtoms::infer, infer);
    if (!infer.IsEmpty()) {
        nsCString inferContractID(NS_RDF_INFER_DATASOURCE_CONTRACTID_PREFIX);
        AppendUTF16toUTF8(infer, inferContractID);
        nsCOMPtr<nsIRDFInferDataSource> inferDB = do_CreateInstance(inferContractID.get());

        if (inferDB) {
            inferDB->SetBaseDataSource(mCompDB);
            mDB = do_QueryInterface(inferDB);
        } else {
            NS_WARNING("failed to construct inference engine specified on template");
        }
    }

    if (!mDB)
        mDB = mCompDB;

    nsCOMPtr<nsIXULDocument> xuldoc = do_QueryInterface(doc);
    if (xuldoc)
        xuldoc->SetTemplateBuilderFor(mRoot, this);

    // Now set the database on the element, so that script writers can
    // access it.
    nsXULElement *xulcontent = nsXULElement::FromContent(mRoot);
    if (! xulcontent) {
        // Hmm. This must be an HTML element. Try to set it as a
        // JS property "by hand".
        InitHTMLTemplateRoot();
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

    nsIScriptGlobalObject *global = doc->GetScriptGlobalObject();
    if (! global)
        return NS_ERROR_UNEXPECTED;

    JSObject *scope = global->GetGlobalJSObject();

    nsIScriptContext *context = global->GetContext();
    if (! context)
        return NS_ERROR_UNEXPECTED;

    JSContext* jscontext = NS_REINTERPRET_CAST(JSContext*, context->GetNativeContext());
    NS_ASSERTION(context != nsnull, "no jscontext");
    if (! jscontext)
        return NS_ERROR_UNEXPECTED;

    nsIXPConnect *xpc = nsContentUtils::XPConnect();

    JSObject* jselement = nsnull;

    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(jscontext, scope, mRoot, NS_GET_IID(nsIDOMElement),
                         getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = wrapper->GetJSObject(&jselement);
    NS_ENSURE_SUCCESS(rv, rv);

    {
        // database
        rv = xpc->WrapNative(jscontext, scope, mDB,
                             NS_GET_IID(nsIRDFCompositeDataSource),
                             getter_AddRefs(wrapper));
        NS_ENSURE_SUCCESS(rv, rv);

        JSObject* jsobj;
        rv = wrapper->GetJSObject(&jsobj);
        NS_ENSURE_SUCCESS(rv, rv);

        jsval jsdatabase = OBJECT_TO_JSVAL(jsobj);

        PRBool ok;
        ok = JS_SetProperty(jscontext, jselement, "database", &jsdatabase);
        NS_ASSERTION(ok, "unable to set database property");
        if (! ok)
            return NS_ERROR_FAILURE;
    }

    {
        // builder
        nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
        rv = xpc->WrapNative(jscontext, jselement,
                             NS_STATIC_CAST(nsIXULTemplateBuilder*, this),
                             NS_GET_IID(nsIXULTemplateBuilder),
                             getter_AddRefs(wrapper));
        NS_ENSURE_SUCCESS(rv, rv);

        JSObject* jsobj;
        rv = wrapper->GetJSObject(&jsobj);
        NS_ENSURE_SUCCESS(rv, rv);

        jsval jsbuilder = OBJECT_TO_JSVAL(jsobj);

        PRBool ok;
        ok = JS_SetProperty(jscontext, jselement, "builder", &jsbuilder);
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
                                           PRInt16 *aRuleIndex)
{
    // iterate through the rules and look for one that the result matches
    PRInt16 count = aQuerySet->RuleCount();
    for (PRInt16 r = 0; r < count; r++) {
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
    *aMatchedRule = nsnull;
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
        PRBool isvar;
        if (*iter == PRUnichar('?') && (++iter != done_parsing)) {
            isvar = PR_TRUE;
        }
        else if ((*iter == PRUnichar('r') && (++iter != done_parsing)) &&
                 (*iter == PRUnichar('d') && (++iter != done_parsing)) &&
                 (*iter == PRUnichar('f') && (++iter != done_parsing)) &&
                 (*iter == PRUnichar(':') && (++iter != done_parsing))) {
            isvar = PR_TRUE;
        }
        else {
            isvar = PR_FALSE;
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


struct SubstituteTextClosure {
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
    SubstituteTextClosure* c = NS_STATIC_CAST(SubstituteTextClosure*, aClosure);
    c->str.Append(aText);
}

void
nsXULTemplateBuilder::SubstituteTextReplaceVariable(nsXULTemplateBuilder* aThis,
                                                    const nsAString& aVariable,
                                                    void* aClosure)
{
    // Substitute the value for the variable and append to the
    // closure's result.
    SubstituteTextClosure* c = NS_STATIC_CAST(SubstituteTextClosure*, aClosure);

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

PRBool
nsXULTemplateBuilder::IsTemplateElement(nsIContent* aContent)
{
    return aContent->NodeInfo()->Equals(nsGkAtoms::_template,
                                        kNameSpaceID_XUL);
}

nsresult
nsXULTemplateBuilder::GetTemplateRoot(nsIContent** aResult)
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
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

        if (domElement)
            return CallQueryInterface(domElement, aResult);
    }

#if 1 // XXX hack to workaround bug with XBL insertion/removal?
    {
        // If root node has no template attribute, then look for a child
        // node which is a template tag
        PRUint32 count = mRoot->GetChildCount();

        for (PRUint32 i = 0; i < count; ++i) {
            nsIContent *child = mRoot->GetChildAt(i);

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
        PRUint32 length;
        kids->GetLength(&length);

        for (PRUint32 i = 0; i < length; ++i) {
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

    *aResult = nsnull;
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
    // see if they are empty
    if (flags.Find(NS_LITERAL_STRING("dont-test-empty")) >= 0)
        mFlags |= eDontTestEmpty;

    if (flags.Find(NS_LITERAL_STRING("dont-recurse")) >= 0)
        mFlags |= eDontRecurse;

    nsCOMPtr<nsIDOMNode> rootnode = do_QueryInterface(mRoot);
    nsresult rv = mQueryProcessor->InitializeForBuilding(mDB, this, rootnode);
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
        mMemberVariable = nsnull;
    else
        mMemberVariable = do_GetAtom(membervar);

    nsTemplateQuerySet* queryset = new nsTemplateQuerySet(0);
    if (!queryset)
        return NS_ERROR_OUT_OF_MEMORY;

    if (!mQuerySets.AppendElement(queryset)) {
        delete queryset;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    PRBool canUseTemplate = PR_FALSE;
    PRInt32 priority = 0;
    rv = CompileTemplate(tmpl, queryset, PR_FALSE, &priority, &canUseTemplate);

    if (NS_FAILED(rv) || !canUseTemplate) {
        for (PRInt32 q = mQuerySets.Length() - 1; q >= 0; q--) {
            nsTemplateQuerySet* qs = mQuerySets[q];
            delete qs;
        }
        mQuerySets.Clear();
    }

    mQueriesCompiled = PR_TRUE;

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileTemplate(nsIContent* aTemplate,
                                      nsTemplateQuerySet* aQuerySet,
                                      PRBool aIsQuerySet,
                                      PRInt32* aPriority,
                                      PRBool* aCanUseTemplate)
{
    NS_ASSERTION(aQuerySet, "No queryset supplied");

    // XXXndeakin log syntax errors

    nsresult rv = NS_OK;

    PRBool isQuerySetMode = PR_FALSE;
    PRBool hasQuerySet = PR_FALSE, hasRule = PR_FALSE, hasQuery = PR_FALSE;

    PRUint32 count = aTemplate->GetChildCount();

    for (PRUint32 i = 0; i < count; i++) {
        nsIContent *rulenode = aTemplate->GetChildAt(i);
        nsINodeInfo *ni = rulenode->NodeInfo();

        // don't allow more queries than can be supported
        if (*aPriority == PR_INT16_MAX)
            return NS_ERROR_FAILURE;

        // XXXndeakin queryset isn't a good name for this tag since it only
        //            ever contains one query
        if (!aIsQuerySet && ni->Equals(nsGkAtoms::queryset, kNameSpaceID_XUL)) {
            if (hasRule || hasQuery)
              continue;

            isQuerySetMode = PR_TRUE;

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

            hasQuerySet = PR_TRUE;

            rv = CompileTemplate(rulenode, aQuerySet, PR_TRUE, aPriority, aCanUseTemplate);
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
                nsCOMPtr<nsIAtom> memberVariable;
                DetermineMemberVariable(action, getter_AddRefs(memberVariable));
                if (! memberVariable) continue;

                if (hasQuery) {
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

                        *aCanUseTemplate = PR_TRUE;
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

                        hasQuerySet = PR_TRUE;

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

                            *aCanUseTemplate = PR_TRUE;
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

                hasQuerySet = PR_TRUE;

                rv = CompileSimpleQuery(rulenode, aQuerySet, aCanUseTemplate);
                if (NS_FAILED(rv))
                    return rv;
            }

            hasRule = PR_TRUE;
        }
        else if (ni->Equals(nsGkAtoms::query, kNameSpaceID_XUL)) {
            if (hasQuery)
              continue;

            aQuerySet->mQueryNode = rulenode;
            hasQuery = PR_TRUE;
        }
        else if (ni->Equals(nsGkAtoms::action, kNameSpaceID_XUL)) {
            // the query must appear before the action
            if (! hasQuery)
                continue;

            nsCOMPtr<nsIAtom> tag;
            DetermineRDFQueryRef(aQuerySet->mQueryNode, getter_AddRefs(tag));
            if (tag)
                aQuerySet->SetTag(tag);

            nsCOMPtr<nsIAtom> memberVariable;
            DetermineMemberVariable(rulenode, getter_AddRefs(memberVariable));
            if (! memberVariable) continue;

            nsCOMPtr<nsIDOMNode> query(do_QueryInterface(aQuerySet->mQueryNode));

            rv = mQueryProcessor->CompileQuery(this, query,
                                               mRefVariable, memberVariable,
                                               getter_AddRefs(aQuerySet->mCompiledQuery));

            if (aQuerySet->mCompiledQuery) {
                nsTemplateRule* rule = new nsTemplateRule(aTemplate, rulenode, aQuerySet);
                if (! rule)
                    return NS_ERROR_OUT_OF_MEMORY;

                rv = aQuerySet->AddRule(rule);
                if (NS_FAILED(rv)) {
                    delete rule;
                    return rv;
                }

                rule->SetVars(mRefVariable, memberVariable);

                *aCanUseTemplate = PR_TRUE;

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

    nsTemplateRule* rule = new nsTemplateRule(aRuleElement, aActionElement, aQuerySet);
    if (! rule)
         return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIContent> conditions;
    nsXULContentUtils::FindChildByTag(aRuleElement,
                                      kNameSpaceID_XUL,
                                      nsGkAtoms::conditions,
                                      getter_AddRefs(conditions));

    if (conditions) {
        rv = CompileConditions(rule, conditions);

        // If the rule compilation failed, then we have to bail.
        if (NS_FAILED(rv)) {
            delete rule;
            return rv;
        }
    }

    rv = aQuerySet->AddRule(rule);
    if (NS_FAILED(rv)) {
        delete rule;
        return rv;
    }

    rule->SetVars(mRefVariable, aMemberVariable);

    // If we've got bindings, add 'em.
    nsCOMPtr<nsIContent> bindings;
    nsXULContentUtils::FindChildByTag(aRuleElement,
                                      kNameSpaceID_XUL,
                                      nsGkAtoms::bindings,
                                      getter_AddRefs(bindings));

    if (bindings) {
        rv = CompileBindings(rule, bindings);
        if (NS_FAILED(rv))
            return rv;
    }

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::DetermineMemberVariable(nsIContent* aActionElement,
                                              nsIAtom** aMemberVariable)
{
    // If the member variable hasn't already been specified, then
    // grovel over <action> to find it. We'll use the first one
    // that we find in a breadth-first search.

    if (mMemberVariable) {
        *aMemberVariable = mMemberVariable;
        NS_IF_ADDREF(*aMemberVariable);
    }
    else {
        *aMemberVariable = nsnull;

        nsCOMArray<nsIContent> unvisited;

        if (!unvisited.AppendObject(aActionElement))
            return NS_ERROR_OUT_OF_MEMORY;

        while (unvisited.Count()) {
            nsIContent* next = unvisited[0];
            unvisited.RemoveObjectAt(0);

            nsAutoString uri;
            next->GetAttr(kNameSpaceID_None, nsGkAtoms::uri, uri);

            if (!uri.IsEmpty() && uri[0] == PRUnichar('?')) {
                // Found it.
                *aMemberVariable = NS_NewAtom(uri);
                break;
            }

            // otherwise, append the children to the unvisited list: this
            // results in a breadth-first search.
            PRUint32 count = next->GetChildCount();

            for (PRUint32 i = 0; i < count; ++i) {
                nsIContent *child = next->GetChildAt(i);

                if (!unvisited.AppendObject(child))
                    return NS_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    return NS_OK;
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
            *aTag = NS_NewAtom(tag);
    }
}

nsresult
nsXULTemplateBuilder::CompileSimpleQuery(nsIContent* aRuleElement,
                                         nsTemplateQuerySet* aQuerySet,
                                         PRBool* aCanUseTemplate)
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
        *aCanUseTemplate = PR_FALSE;
        return NS_OK;
    }

    nsTemplateRule* rule = new nsTemplateRule(aRuleElement, aRuleElement, aQuerySet);
    if (! rule)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = aQuerySet->AddRule(rule);
    if (NS_FAILED(rv)) {
        delete rule;
        return rv;
    }

    rule->SetVars(mRefVariable, memberVariable);

    nsAutoString tag;
    aRuleElement->GetAttr(kNameSpaceID_None, nsGkAtoms::parent, tag);

    if (!tag.IsEmpty()) {
        nsCOMPtr<nsIAtom> tagatom = do_GetAtom(tag);
        aQuerySet->SetTag(tagatom);
    }

    *aCanUseTemplate = PR_TRUE;

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

    PRUint32 count = aCondition->GetChildCount();

    nsTemplateCondition* currentCondition = nsnull;

    for (PRUint32 i = 0; i < count; i++) {
        nsIContent *node = aCondition->GetChildAt(i);

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
    if (subject.IsEmpty())
        return NS_OK;

    nsCOMPtr<nsIAtom> svar;
    if (subject[0] == PRUnichar('?'))
        svar = do_GetAtom(subject);

    nsAutoString relstring;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, relstring);
    if (relstring.IsEmpty())
        return NS_OK;

    // object
    nsAutoString value;
    aCondition->GetAttr(kNameSpaceID_None, nsGkAtoms::value, value);
    if (value.IsEmpty())
        return NS_OK;

    // multiple
    PRBool shouldMultiple =
      aCondition->AttrValueIs(kNameSpaceID_None, nsGkAtoms::multiple,
                              nsGkAtoms::_true, eCaseMatters);

    nsCOMPtr<nsIAtom> vvar;
    if (!shouldMultiple && (value[0] == PRUnichar('?'))) {
        vvar = do_GetAtom(value);
    }

    // ignorecase
    PRBool shouldIgnoreCase =
      aCondition->AttrValueIs(kNameSpaceID_None, nsGkAtoms::ignorecase,
                              nsGkAtoms::_true, eCaseMatters);

    // negate
    PRBool shouldNegate =
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
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] on <where> test, expected at least one variable", this));
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

    PRUint32 count = aBindings->GetChildCount();

    for (PRUint32 i = 0; i < count; ++i) {
        nsIContent *binding = aBindings->GetChildAt(i);

        if (binding->NodeInfo()->Equals(nsGkAtoms::binding,
                                        kNameSpaceID_XUL)) {
            rv = CompileBinding(aRule, binding);
        }
        else {
#ifdef PR_LOGGING
            nsAutoString tagstr;
            binding->NodeInfo()->GetQualifiedName(tagstr);

            nsCAutoString tagstrC;
            tagstrC.AssignWithConversion(tagstr);
            PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
                   ("xultemplate[%p] unrecognized binding <%s>",
                    this, tagstrC.get()));
#endif

            continue;
        }

        if (NS_FAILED(rv))
            return rv;
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
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `subject'", this));

        return NS_OK;
    }

    nsCOMPtr<nsIAtom> svar;
    if (subject[0] == PRUnichar('?')) {
        svar = do_GetAtom(subject);
    }
    else {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `subject' to be a variable", this));

        return NS_OK;
    }

    // predicate
    nsAutoString predicate;
    aBinding->GetAttr(kNameSpaceID_None, nsGkAtoms::predicate, predicate);
    if (predicate.IsEmpty()) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `predicate'", this));

        return NS_OK;
    }

    // object
    nsAutoString object;
    aBinding->GetAttr(kNameSpaceID_None, nsGkAtoms::object, object);

    if (object.IsEmpty()) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `object'", this));

        return NS_OK;
    }

    nsCOMPtr<nsIAtom> ovar;
    if (object[0] == PRUnichar('?')) {
        ovar = do_GetAtom(object);
    }
    else {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `object' to be a variable", this));

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

    nsAutoVoidArray elements;

    if (!elements.AppendElement(aElement))
        return NS_ERROR_OUT_OF_MEMORY;

    while (elements.Count()) {
        // Pop the next element off the stack
        PRUint32 i = (PRUint32)(elements.Count() - 1);
        nsIContent* element = NS_STATIC_CAST(nsIContent*, elements[i]);
        elements.RemoveElementAt(i);

        // Iterate through its attributes, looking for substitutions
        // that we need to add as bindings.
        PRUint32 count = element->GetAttrCount();

        for (i = 0; i < count; ++i) {
            const nsAttrName* name = element->GetAttrNameAt(i);

            if (!name->Equals(nsGkAtoms::id, kNameSpaceID_None) &&
                !name->Equals(nsGkAtoms::uri, kNameSpaceID_None)) {
                nsAutoString value;
                element->GetAttr(name->NamespaceID(), name->LocalName(), value);

                // Scan the attribute for variables, adding a binding for
                // each one.
                ParseAttribute(value, AddBindingsFor, nsnull, aRule);
            }
        }

        // Push kids onto the stack, and search them next.
        count = element->GetChildCount();

        while (count-- > 0) {
            if (!elements.AppendElement(element->GetChildAt(count)))
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

    nsTemplateRule* rule = NS_STATIC_CAST(nsTemplateRule*, aClosure);

    nsCOMPtr<nsIAtom> var = do_GetAtom(aVariable);

    // Strip it down to the raw RDF property by clobbering the "rdf:"
    // prefix
    nsAutoString property;
    property.Assign(Substring(aVariable, PRUint32(4), aVariable.Length() - 4));

    if (! rule->HasBinding(rule->GetMemberVariable(), property, var))
        // In the simple syntax, the binding is always from the
        // member variable, through the property, to the target.
        rule->AddBinding(rule->GetMemberVariable(), property, var);
}


nsresult
nsXULTemplateBuilder::IsSystemPrincipal(nsIPrincipal *principal, PRBool *result)
{
  if (!gSystemPrincipal)
    return NS_ERROR_UNEXPECTED;

  *result = (principal == gSystemPrincipal);
  return NS_OK;
}

PRBool
nsXULTemplateBuilder::IsActivated(nsIRDFResource *aResource)
{
    for (ActivationEntry *entry = mTop;
         entry != nsnull;
         entry = entry->mPrevious) {
        if (entry->mResource == aResource)
            return PR_TRUE;
    }
    return PR_FALSE;
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

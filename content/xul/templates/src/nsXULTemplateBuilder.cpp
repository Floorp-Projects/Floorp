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

  Builds content from an RDF graph using the XUL <template> tag.

  TO DO

  . Fix ContentTagTest's location in the network construction

  . We're a bit schizophrenic about whether we want to use
    nsXULTemplateBuilder's members to determine the container & member
    variables, or if we want to use the mRule member of a nsTemplateMatch. Which
    is right? Is there redundancy here?

  . MatchSet has extra information about the "best match" and "last
    match" that really seems like it should be a part of
    ConflictSet::ClusterEntry.

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
#include "nsIBindingManager.h"
#include "nsIDOMNodeList.h"
#include "nsINameSpace.h"
#include "nsINameSpaceManager.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFInferDataSource.h"
#include "nsIRDFContainerUtils.h" 
#include "nsIXULDocument.h"
#include "nsIXULTemplateBuilder.h"
#include "nsIXULBuilderListener.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsArray.h"
#include "nsITimer.h"
#include "nsIURL.h"
#include "nsIXPConnect.h"
#include "nsIXULSortService.h"
#include "nsContentCID.h"
#include "nsRDFCID.h"
#include "nsIXULContent.h"
#include "nsXULContentUtils.h"
#include "nsRDFSort.h"
#include "nsRuleNetwork.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "nsXULAtoms.h"
#include "nsXULElement.h"
#include "jsapi.h"
#include "prlog.h"
#include "rdf.h"
#include "pldhash.h"
#include "plhash.h"

#include "nsClusterKeySet.h"
#include "nsConflictSet.h"
#include "nsInstantiationNode.h"
#include "nsNetUtil.h"
#include "nsRDFConInstanceTestNode.h"
#include "nsRDFConMemberTestNode.h"
#include "nsRDFPropertyTestNode.h"
#include "nsRDFTestNode.h"
#include "nsResourceSet.h"
#include "nsTemplateRule.h"
#include "nsXULTemplateBuilder.h"

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

//----------------------------------------------------------------------
//
// nsXULTempalteBuilder methods
//

nsXULTemplateBuilder::nsXULTemplateBuilder(void)
    : mDB(nsnull),
      mCompDB(nsnull),
      mRoot(nsnull),
      mUpdateBatchNest(0),
      mRulesCompiled(PR_FALSE),
      mFlags(0),
      mTop(nsnull)
{
}

nsXULTemplateBuilder::~nsXULTemplateBuilder(void)
{
    if (--gRefCnt == 0) {
        NS_IF_RELEASE(gRDFService);
        NS_IF_RELEASE(gRDFContainerUtils);
        NS_IF_RELEASE(gSystemPrincipal);
        NS_IF_RELEASE(gScriptSecurityManager);
    }
}


nsresult
nsXULTemplateBuilder::Init()
{
    if (gRefCnt++ == 0) {
        nsresult rv;

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

    return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsXULTemplateBuilder,
                   nsIXULTemplateBuilder,
                   nsIRDFObserver)

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

    return rv;
}

NS_IMETHODIMP
nsXULTemplateBuilder::CreateContents(nsIContent* aElement)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::AddListener(nsIXULBuilderListener* aListener)
{
    NS_ENSURE_ARG(aListener);

    mListeners.AppendObject(aListener);

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
nsXULTemplateBuilder::AttributeChanged(nsIDocument *aDocument,
                                       nsIContent*  aContent,
                                       PRInt32      aNameSpaceID,
                                       nsIAtom*     aAttribute,
                                       PRInt32      aModType)
{
    if (aContent == mRoot) {
        // Check for a change to the 'ref' attribute on an atom, in which
        // case we may need to nuke and rebuild the entire content model
        // beneath the element.
        if (aAttribute == nsXULAtoms::ref)
            Rebuild();

        // Check for a change to the 'datasources' attribute. If so, setup
        // mDB by parsing the vew value and rebuild.
        else if (aAttribute == nsXULAtoms::datasources) {
            LoadDataSources(aDocument);
            Rebuild();
        }
    }        
}

void
nsXULTemplateBuilder::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
    // Break circular references
    if (mDB) {
        mDB->RemoveObserver(this);
        mDB = nsnull;
        mCompDB = nsnull;
    }

    mRoot = nsnull;
}


//----------------------------------------------------------------------
//
// nsIRDFObserver interface
//

nsresult
nsXULTemplateBuilder::Propagate(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget,
                                nsClusterKeySet& aNewKeys)
{
    // Find the "dominating" tests that could be used to propagate the
    // assertion we've just received. (Test A "dominates" test B if A
    // is an ancestor of B in the rule network).
    nsresult rv;

    // First, we'll go through and find all of the test nodes that can
    // propagate the assertion.
    ReteNodeSet livenodes;

    {
        ReteNodeSet::Iterator last = mRDFTests.Last();
        for (ReteNodeSet::Iterator i = mRDFTests.First(); i != last; ++i) {
            nsRDFTestNode* rdftestnode = NS_STATIC_CAST(nsRDFTestNode*, *i);

            Instantiation seed;
            if (rdftestnode->CanPropagate(aSource, aProperty, aTarget, seed))
                livenodes.Add(rdftestnode);
        }
    }

    // Now, we'll go through each, and any that aren't dominated by
    // another live node will be used to propagate the assertion
    // through the rule network
    {
        ReteNodeSet::Iterator last = livenodes.Last();
        for (ReteNodeSet::Iterator i = livenodes.First(); i != last; ++i) {
            nsRDFTestNode* rdftestnode = NS_STATIC_CAST(nsRDFTestNode*, *i);

            PRBool isdominated = PR_FALSE;

            for (ReteNodeSet::ConstIterator j = livenodes.First(); j != last; ++j) {
                // we can't be dominated by ourself
                if (j == i)
                    continue;

                if (rdftestnode->HasAncestor(*j)) {
                    isdominated = PR_TRUE;
                    break;
                }
            }

            if (! isdominated) {
                // Bogus, to get the seed instantiation
                Instantiation seed;
                rdftestnode->CanPropagate(aSource, aProperty, aTarget, seed);

                InstantiationSet instantiations;
                instantiations.Append(seed);

                rv = rdftestnode->Constrain(instantiations, &mConflictSet);
                if (NS_FAILED(rv)) return rv;

                if (! instantiations.Empty()) {
                    rv = rdftestnode->Propagate(instantiations, &aNewKeys);
                    if (NS_FAILED(rv)) return rv;
                }
            }
        }
    }

    return NS_OK;
}


nsresult
nsXULTemplateBuilder::FireNewlyMatchedRules(const nsClusterKeySet& aNewKeys)
{
    // Iterate through newly added keys to determine which rules fired.
    //
    // XXXwaterson Unfortunately, this could also lead to retractions;
    // e.g., (contaner ?a ^empty false) could become "unmatched". How
    // to track those?
    nsClusterKeySet::ConstIterator last = aNewKeys.Last();
    for (nsClusterKeySet::ConstIterator key = aNewKeys.First(); key != last; ++key) {
        nsConflictSet::MatchCluster* matches =
            mConflictSet.GetMatchesForClusterKey(*key);

        NS_ASSERTION(matches != nsnull, "no matched rules for new key");
        if (! matches)
            continue;

        nsTemplateMatch* bestmatch =
            mConflictSet.GetMatchWithHighestPriority(matches);

        NS_ASSERTION(bestmatch != nsnull, "no matches in match set");
        if (! bestmatch)
            continue;

        // If the new "bestmatch" is different from the last match,
        // then we need to yank some content out and rebuild it.
        const nsTemplateMatch* lastmatch = matches->mLastMatch;
        if (bestmatch != lastmatch) {
            ReplaceMatch(VALUE_TO_IRDFRESOURCE(key->mMemberValue), lastmatch, bestmatch);

            // Remember the best match as the new "last" match
            matches->mLastMatch = bestmatch;
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateBuilder::OnAssert(nsIRDFDataSource* aDataSource,
                               nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return(NS_OK);

    // Ignore re-entrant builds for content that is currently in our
    // activation stack.
    if (IsActivated(aSource))
        return NS_OK;

	if (mCache)
        mCache->Assert(aSource, aProperty, aTarget, PR_TRUE /* XXX should be value passed in */);

    LOG("onassert", aSource, aProperty, aTarget);

    nsClusterKeySet newkeys;
    Propagate(aSource, aProperty, aTarget, newkeys);
    FireNewlyMatchedRules(newkeys);
    SynchronizeAll(aSource, aProperty, nsnull, aTarget);
    return NS_OK;
}


nsresult
nsXULTemplateBuilder::Retract(nsIRDFResource* aSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aTarget)
{
    // Retract any currently active rules that will no longer be
    // matched.
    ReteNodeSet::ConstIterator lastnode = mRDFTests.Last();
    for (ReteNodeSet::ConstIterator node = mRDFTests.First(); node != lastnode; ++node) {
        const nsRDFTestNode* rdftestnode = NS_STATIC_CAST(const nsRDFTestNode*, *node);

        nsTemplateMatchSet firings(mConflictSet.GetPool());
        nsTemplateMatchSet retractions(mConflictSet.GetPool());
        rdftestnode->Retract(aSource, aProperty, aTarget, firings, retractions);

        {
            nsTemplateMatchSet::ConstIterator last = retractions.Last();
            for (nsTemplateMatchSet::ConstIterator match = retractions.First(); match != last; ++match) {
                Value memberval;
                match->mAssignments.GetAssignmentFor(match->mRule->GetMemberVariable(), &memberval);

                ReplaceMatch(VALUE_TO_IRDFRESOURCE(memberval), match.operator->(), nsnull);
            }
        }
#if 0
        // Now fire any newly revealed rules
        {
            nsTemplateMatchSet::ConstIterator last = firings.Last();
            for (nsTemplateMatchSet::ConstIterator match = firings.First(); match != last; ++match) {
                // XXXwaterson yo. write me.
                // The intent here is to handle any rules that might be
                // "revealed" by the removal of an assertion from the datasource.
                // Waterson doesn't think we support negated conditions in a rule.
                // Nor is he sure that this is currently useful.
            }
        }
#endif
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateBuilder::OnUnassert(nsIRDFDataSource* aDataSource,
                                 nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return NS_OK;

    // Ignore re-entrant builds for content that is currently in our
    // activation stack.
    if (IsActivated(aSource))
        return NS_OK;

	if (mCache)
		mCache->Unassert(aSource, aProperty, aTarget);

    LOG("onunassert", aSource, aProperty, aTarget);

    Retract(aSource, aProperty, aTarget);
    SynchronizeAll(aSource, aProperty, aTarget, nsnull);
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateBuilder::OnChange(nsIRDFDataSource* aDataSource,
                               nsIRDFResource* aSource,
                               nsIRDFResource* aProperty,
                               nsIRDFNode* aOldTarget,
                               nsIRDFNode* aNewTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return NS_OK;

    // Ignore re-entrant builds for content that is currently in our
    // activation stack.
    if (IsActivated(aSource))
        return NS_OK;

	if (mCache) {
		if (aOldTarget)
			// XXX fix this: in-memory DS doesn't like a null oldTarget
			mCache->Change(aSource, aProperty, aOldTarget, aNewTarget);
		else
			// XXX should get tv via observer interface
			mCache->Assert(aSource, aProperty, aNewTarget, PR_TRUE);
	}

    LOG("onchange", aSource, aProperty, aNewTarget);

    if (aOldTarget) {
        // Pull any old rules that were relying on aOldTarget
        Retract(aSource, aProperty, aOldTarget);
    }

    if (aNewTarget) {
        // Fire any new rules that are activated by aNewTarget
        nsClusterKeySet newkeys;
        Propagate(aSource, aProperty, aNewTarget, newkeys);
        FireNewlyMatchedRules(newkeys);
    }

    // Synchronize any of the content model that may have changed.
    SynchronizeAll(aSource, aProperty, aOldTarget, aNewTarget);
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateBuilder::OnMove(nsIRDFDataSource* aDataSource,
                             nsIRDFResource* aOldSource,
                             nsIRDFResource* aNewSource,
                             nsIRDFResource* aProperty,
                             nsIRDFNode* aTarget)
{
    // Ignore updates if we're batching
    if (mUpdateBatchNest)
        return NS_OK;

	if (mCache)
		mCache->Move(aOldSource, aNewSource, aProperty, aTarget);

    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXULTemplateBuilder::OnBeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
    mUpdateBatchNest++;
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateBuilder::OnEndUpdateBatch(nsIRDFDataSource* aDataSource)
{
    NS_ASSERTION(mUpdateBatchNest > 0, "badly nested update batch");
    if (--mUpdateBatchNest == 0) {
        Rebuild();
    }

    return NS_OK;
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

    // flush (delete) the cache when re-rerooting the generated content
    if (mCache)
    	mCache = nsnull;

    if (mDB) {
        mDB->RemoveObserver(this);

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
	nsAutoString coalesce;
	mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::coalesceduplicatearcs, coalesce);
    if (coalesce.EqualsLiteral("false"))
		mCompDB->SetCoalesceDuplicateArcs(PR_FALSE);

    nsAutoString allowneg;
    mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::allownegativeassertions, allowneg);
    if (allowneg.EqualsLiteral("false"))
		mCompDB->SetAllowNegativeAssertions(PR_FALSE);

    // Grab the doc's principal...
    nsIPrincipal *docPrincipal = doc->GetPrincipal();
    if (!docPrincipal)
        return NS_ERROR_FAILURE;

    PRBool isTrusted = PR_FALSE;
    rv = IsSystemPrincipal(docPrincipal, &isTrusted);
    if (NS_FAILED(rv)) return rv;

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
            if (NS_FAILED(rv)) return rv;
        }
    }

    // Parse datasources: they are assumed to be a whitespace
    // separated list of URIs; e.g.,
    //
    //     rdf:bookmarks rdf:history http://foo.bar.com/blah.cgi?baz=9
    //
    nsIURI *docurl = doc->GetDocumentURI();

    nsAutoString datasources;
    mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::datasources, datasources);

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
            if (NS_FAILED(rv)) return rv;

            PRBool same;
            rv = docPrincipal->Equals(principal, &same);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unable to test same origin");
            if (NS_FAILED(rv)) return rv;

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
            // be accessable for any number of reasons, including
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
    mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::infer, infer);
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
    nsCOMPtr<nsIXULContent> xulcontent = do_QueryInterface(mRoot);
    if (! xulcontent) {
        // Hmm. This must be an HTML element. Try to set it as a
        // JS property "by hand".
        InitHTMLTemplateRoot();
    }

    // Add ourselves as a datasource observer
    mDB->AddObserver(this);

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

    nsIScriptContext *context = global->GetContext();
    if (! context)
        return NS_ERROR_UNEXPECTED;

    JSContext* jscontext = NS_REINTERPRET_CAST(JSContext*, context->GetNativeContext());
    NS_ASSERTION(context != nsnull, "no jscontext");
    if (! jscontext)
        return NS_ERROR_UNEXPECTED;

    static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(kXPConnectCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject* jselement = nsnull;

    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(jscontext, ::JS_GetGlobalObject(jscontext), mRoot,
                         NS_GET_IID(nsIDOMElement),
                         getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = wrapper->GetJSObject(&jselement);
    NS_ENSURE_SUCCESS(rv, rv);

    {
        // database
        rv = xpc->WrapNative(jscontext, ::JS_GetGlobalObject(jscontext), mDB,
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
    SubstituteTextClosure(nsTemplateMatch& aMatch, nsAString& aResult)
        : match(aMatch), result(aResult) {}

    nsTemplateMatch& match;
    nsAString& result;
};

nsresult
nsXULTemplateBuilder::SubstituteText(nsTemplateMatch& aMatch,
                                     const nsAString& aAttributeValue,
                                     nsAString& aResult)
{
    // See if it's the special value "..."
    if (aAttributeValue.EqualsLiteral("...")) {
        Value memberval;
        aMatch.GetAssignmentFor(mConflictSet, mMemberVar, &memberval);

        nsIRDFResource* member = VALUE_TO_IRDFRESOURCE(memberval);
        NS_ASSERTION(member != nsnull, "no member!");
        if (! member)
            return NS_ERROR_UNEXPECTED;

        const char *uri = nsnull;
        member->GetValueConst(&uri);

        CopyUTF8toUTF16(uri, aResult);

        return NS_OK;
    }

    // Reasonable guess at how big it should be
    aResult.SetCapacity(aAttributeValue.Length());

    SubstituteTextClosure closure(aMatch, aResult);
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
    c->result.Append(aText);
}

void
nsXULTemplateBuilder::SubstituteTextReplaceVariable(nsXULTemplateBuilder* aThis,
                                                    const nsAString& aVariable,
                                                    void* aClosure)
{
    // Substitute the value for the variable and append to the
    // closure's result.
    SubstituteTextClosure* c = NS_STATIC_CAST(SubstituteTextClosure*, aClosure);

    // The symbol "rdf:*" is special, and means "this guy's URI"
    PRInt32 var = 0;
    if (aVariable.EqualsLiteral("rdf:*"))
        var = c->match.mRule->GetMemberVariable();
    else
        var = aThis->mRules.LookupSymbol(PromiseFlatString(aVariable).get());

    // No variable; treat as a variable with no substitution. (This
    // shouldn't ever happen, really...)
    if (! var)
        return;

    // Got a variable; get the value it's assigned to
    Value value;
    PRBool hasAssignment =
        c->match.GetAssignmentFor(aThis->mConflictSet, var, &value);

    // If there was no assignment for the variable, bail. This'll
    // leave the result string with an empty substitution for the
    // variable.
    if (! hasAssignment)
        return;

    // Got a value; substitute it.
    switch (value.GetType()) {
    case Value::eISupports:
        {
            nsISupports* isupports = NS_STATIC_CAST(nsISupports*, value);

            nsCOMPtr<nsIRDFNode> node = do_QueryInterface(isupports);
            if (node) {
                // XXX ideally we'd just point this right at the
                // substring which is the end of the string,
                // turning this into an in-place append.
                nsAutoString temp;
                nsXULContentUtils::GetTextForNode(node, temp);
                c->result += temp;
            }
        }
    break;

    case Value::eString:
        c->result += NS_STATIC_CAST(const PRUnichar*, value);
        break;

    default:
        break;
    }
    
}

struct IsVarInSetClosure {
    IsVarInSetClosure(nsTemplateMatch& aMatch, const VariableSet& aModifiedVars)
        : match(aMatch), modifiedVars(aModifiedVars), result(PR_FALSE) {}

    nsTemplateMatch& match;
    const VariableSet& modifiedVars;
    PRBool result;
};


void
nsXULTemplateBuilder::IsVarInSet(nsXULTemplateBuilder* aThis,
                                 const nsAString& aVariable,
                                 void* aClosure)
{
    IsVarInSetClosure* c = NS_STATIC_CAST(IsVarInSetClosure*, aClosure);

    PRInt32 var =
        aThis->mRules.LookupSymbol(PromiseFlatString(aVariable).get());

    // No variable; treat as a variable with no substitution. (This
    // shouldn't ever happen, really...)
    if (! var)
        return;

    // See if this was one of the variables that was modified. If it
    // *was*, then this attribute *will* be impacted by the modified
    // variable set...
    c->result = c->result || c->modifiedVars.Contains(var);
}

PRBool
nsXULTemplateBuilder::IsAttrImpactedByVars(nsTemplateMatch& aMatch,
                                           const nsAString& aAttributeValue,
                                           const VariableSet& aModifiedVars)
{
    // XXX at some point, it might be good to remember what attributes
    // are impacted by variable changes using information that we
    // could get at rule compilation time, rather than grovelling over
    // the attribute string.
    IsVarInSetClosure closure(aMatch, aModifiedVars);
    ParseAttribute(aAttributeValue, IsVarInSet, nsnull, &closure);

    return closure.result;
}


nsresult
nsXULTemplateBuilder::SynchronizeAll(nsIRDFResource* aSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aOldTarget,
                                     nsIRDFNode* aNewTarget)
{
    // Update each match that contains <aSource, aProperty, aOldTarget>.

    // Get all the matches whose assignments are currently supported
    // by aSource and aProperty: we'll need to recompute them.
    const nsTemplateMatchRefSet* matches =
        mConflictSet.GetMatchesWithBindingDependency(aSource);

    if (! matches || matches->Empty())
        return NS_OK;

    // Since we'll actually be manipulating the match set as we
    // iterate through it, we need to copy it into our own private
    // area before performing the iteration.
    nsTemplateMatchRefSet copy = *matches;

    nsTemplateMatchRefSet::ConstIterator last = copy.Last();
    for (nsTemplateMatchRefSet::ConstIterator match = copy.First(); match != last; ++match) {
        const nsTemplateRule* rule = match->mRule;

        // Recompute the assignments. This will replace aOldTarget with
        // aNewTarget, which will disrupt the match set.
        VariableSet modified;
        rule->RecomputeBindings(mConflictSet, match.operator->(),
                                aSource, aProperty, aOldTarget, aNewTarget,
                                modified);

        // If nothing changed, then continue on to the next match.
        if (0 == modified.GetCount())
            continue;

#ifdef PR_LOGGING
        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] match %p, %d modified binding(s)",
                this, match.operator->(), modified.GetCount()));

        for (PRInt32 i = 0; i < modified.GetCount(); ++i) {
            PRInt32 var = modified.GetVariableAt(i);
            Value val;
            match->GetAssignmentFor(mConflictSet, var, &val);

            nsCAutoString str;
            val.ToCString(str);

            PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
                   ("xultemplate[%p]   %d <= %s", this, var, str.get()));
        }
#endif

        SynchronizeMatch(match.operator->(), modified);
    }

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CheckContainer(nsIRDFResource* aResource, PRBool* aIsContainer, PRBool* aIsEmpty)
{
    // We have to look at all of the arcs extending out of the
    // resource: if any of them are that "containment" property, then
    // we know we'll have children.
    PRBool isContainer = PR_FALSE;
    PRBool isEmpty = PR_TRUE; // assume empty

    for (nsResourceSet::ConstIterator property = mContainmentProperties.First();
         property != mContainmentProperties.Last();
         property++) {
        PRBool hasArc = PR_FALSE;
        mDB->HasArcOut(aResource, *property, &hasArc);

        if (hasArc) {
            // Well, it's a container...
            isContainer = PR_TRUE;

            // ...should we check if it's empty?
            if (!aIsEmpty || (mFlags & eDontTestEmpty)) {
                isEmpty = PR_FALSE;
                break;
            }

            // Yes: call GetTarget() and see if there's anything on
            // the other side...
            nsCOMPtr<nsIRDFNode> dummy;
            mDB->GetTarget(aResource, *property, PR_TRUE, getter_AddRefs(dummy));

            if (dummy) {
                isEmpty = PR_FALSE;
                break;
            }

            // Even if there isn't a target for *this* containment
            // property, we have continue to check the other
            // properties: one of them may have a target.
        }
    }

    // If we get here, and we're still not sure if it's a container,
    // then see if it's an RDF container
    if (! isContainer) {
        gRDFContainerUtils->IsContainer(mDB, aResource, &isContainer);

        if (isContainer && aIsEmpty && !(mFlags & eDontTestEmpty))
            gRDFContainerUtils->IsEmpty(mDB, aResource, &isEmpty);
    }

    if (aIsContainer)
        *aIsContainer = isContainer;

    if (aIsEmpty)
        *aIsEmpty = isEmpty;

    return NS_OK;
}

#ifdef PR_LOGGING
nsresult
nsXULTemplateBuilder::Log(const char* aOperation,
                          nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget)
{
    if (PR_LOG_TEST(gXULTemplateLog, PR_LOG_DEBUG)) {
        nsresult rv;

        const char* sourceStr;
        rv = aSource->GetValueConst(&sourceStr);
        if (NS_FAILED(rv)) return rv;

        PR_LOG(gXULTemplateLog, PR_LOG_DEBUG,
               ("xultemplate[%p] %8s [%s]--", this, aOperation, sourceStr));

        const char* propertyStr;
        rv = aProperty->GetValueConst(&propertyStr);
        if (NS_FAILED(rv)) return rv;

        nsAutoString targetStr;
        rv = nsXULContentUtils::GetTextForNode(aTarget, targetStr);
        if (NS_FAILED(rv)) return rv;

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

//----------------------------------------------------------------------

nsresult
nsXULTemplateBuilder::ComputeContainmentProperties()
{
    // The 'containment' attribute on the root node is a
    // whitespace-separated list that tells us which properties we
    // should use to test for containment.
    nsresult rv;

    mContainmentProperties.Clear();

    nsAutoString containment;
    rv = mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::containment, containment);
    if (NS_FAILED(rv)) return rv;

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
        if (NS_FAILED(rv)) return rv;

        rv = mContainmentProperties.Add(property);
        if (NS_FAILED(rv)) return rv;

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

PRBool
nsXULTemplateBuilder::IsTemplateElement(nsIContent* aContent)
{
    nsINodeInfo *ni = aContent->GetNodeInfo();

    return ni && ni->Equals(nsXULAtoms::Template, kNameSpaceID_XUL);
}

nsresult
nsXULTemplateBuilder::InitializeRuleNetwork()
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    // Determine if there are any special settings we need to observe
    mFlags = 0;

    nsAutoString flags;
    mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::flags, flags);

    if (flags.Find(NS_LITERAL_STRING("dont-test-empty")) >= 0)
        mFlags |= eDontTestEmpty;

    // Initialize the rule network
    mRules.Clear();
    mRules.Clear();
    mRDFTests.Clear();
    ComputeContainmentProperties();

    mContainerVar = mRules.CreateAnonymousVariable();
    mMemberVar = mRules.CreateAnonymousVariable();

    return NS_OK;
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
    mRoot->GetAttr(kNameSpaceID_None, nsXULAtoms::templateAtom, templateID);

    if (!templateID.IsEmpty()) {
        nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(mRoot->GetDocument());
        NS_ASSERTION(domDoc, "expected a XUL document");
        if (! domDoc)
            return NS_ERROR_FAILURE;

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
    NS_ASSERTION(doc, "root element has no document");
    if (! doc)
        return NS_ERROR_FAILURE;

    nsIBindingManager *bindingManager = doc->GetBindingManager();

    if (bindingManager) {
        nsCOMPtr<nsIDOMNodeList> kids;
        bindingManager->GetXBLChildNodesFor(mRoot, getter_AddRefs(kids));

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
    }

    *aResult = nsnull;
    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileRules()
{
    NS_PRECONDITION(mRoot != nsnull, "not initialized");
    if (! mRoot)
        return NS_ERROR_NOT_INITIALIZED;

    mRulesCompiled = PR_FALSE;

    // Initialize the rule network
    InitializeRuleNetwork();

    nsCOMPtr<nsIContent> tmpl;
    GetTemplateRoot(getter_AddRefs(tmpl));
    if (! tmpl)
        return NS_OK;

    // Used for simple rules, if there are any.
    InnerNode* childnode = nsnull;

    // Set the "container" and "member" variables, if the user has
    // specified them.
    mContainerSymbol.Truncate();
    tmpl->GetAttr(kNameSpaceID_None, nsXULAtoms::container, mContainerSymbol);
    if (!mContainerSymbol.IsEmpty())
        mRules.PutSymbol(mContainerSymbol.get(), mContainerVar);

    mMemberSymbol.Truncate();
    tmpl->GetAttr(kNameSpaceID_None, nsXULAtoms::member, mMemberSymbol);
    if (!mMemberSymbol.IsEmpty())
        mRules.PutSymbol(mMemberSymbol.get(), mMemberVar);

    // Compile the rules beneath the <template>
    PRUint32 count = tmpl->GetChildCount();

    PRUint32 nrules = 0;

    for (PRUint32 i = 0; i < count; i++) {
        nsIContent *rule = tmpl->GetChildAt(i);
        nsINodeInfo *ni = rule->GetNodeInfo();

        if (ni && ni->Equals(nsXULAtoms::rule, kNameSpaceID_XUL)) {
            ++nrules;

            // If the <rule> has a <conditions> element, then
            // compile it using the extended syntax.
            nsCOMPtr<nsIContent> conditions;
            nsXULContentUtils::FindChildByTag(rule,
                                              kNameSpaceID_XUL,
                                              nsXULAtoms::conditions,
                                              getter_AddRefs(conditions));

            if (conditions)
                CompileExtendedRule(rule, nrules, mRules.GetRoot());
            else {
                if (! childnode)
                    InitializeRuleNetworkForSimpleRules(&childnode);

                CompileSimpleRule(rule, nrules, childnode);
            }
        }
    }

    if (nrules == 0) {
        // if no rules are specified in the template, then the
        // contents of the <template> tag are the one-and-only
        // template.
        InitializeRuleNetworkForSimpleRules(&childnode);
        CompileSimpleRule(tmpl, 1, childnode);
    }

    // XXXwaterson post-process the rule network to optimize

    mRulesCompiled = PR_TRUE;
    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileExtendedRule(nsIContent* aRuleElement,
                                          PRInt32 aPriority,
                                          InnerNode* aParentNode)
{
    // Compile an "extended" <template> rule. An extended rule must
    // have a <conditions> child, and an <action> child, and may
    // optionally have a <bindings> child.
    nsresult rv;

    nsCOMPtr<nsIContent> conditions;
    nsXULContentUtils::FindChildByTag(aRuleElement,
                                      kNameSpaceID_XUL,
                                      nsXULAtoms::conditions,
                                      getter_AddRefs(conditions));

    if (! conditions) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] no <conditions> element in extended rule", this));

        return NS_OK;
    }

    nsCOMPtr<nsIContent> action;
    nsXULContentUtils::FindChildByTag(aRuleElement,
                                      kNameSpaceID_XUL,
                                      nsXULAtoms::action,
                                      getter_AddRefs(action));

    if (! action) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] no <action> element in extended rule", this));

        return NS_OK;
    }

    // If we've got <conditions> and <action>, we can make a rule.
    nsTemplateRule* rule = new nsTemplateRule(mDB, action, aPriority);
    if (! rule)
        return NS_ERROR_OUT_OF_MEMORY;

    rule->SetContainerVariable(mContainerVar);

    if (mMemberSymbol.IsEmpty()) {
        // If the member variable hasn't already been specified, then
        // grovel over <action> to find it. We'll use the first one
        // that we find in a breadth-first search.
        nsVoidArray unvisited;
        unvisited.AppendElement(action.get());

        while (unvisited.Count()) {
            nsIContent* next = NS_STATIC_CAST(nsIContent*, unvisited[0]);
            unvisited.RemoveElementAt(0);

            nsAutoString uri;
            next->GetAttr(kNameSpaceID_None, nsXULAtoms::uri, uri);

            if (!uri.IsEmpty() && uri[0] == PRUnichar('?')) {
                // Found it.
                mMemberSymbol = uri;

                if (! mRules.LookupSymbol(mMemberSymbol.get()))
                    mRules.PutSymbol(mMemberSymbol.get(), mMemberVar);

                break;
            }

            // otherwise, append the children to the unvisited list: this
            // results in a breadth-first search.
            PRUint32 count = next->GetChildCount();

            for (PRUint32 i = 0; i < count; ++i) {
                nsIContent *child = next->GetChildAt(i);

                unvisited.AppendElement(child);
            }
        }
    }

    // If we can't find a member symbol, then we're out of luck. Bail.
    if (mMemberSymbol.IsEmpty()) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] could not deduce member variable", this));

        delete rule;
        return NS_OK;
    }

    rule->SetMemberVariable(mMemberVar);

    InnerNode* last;
    rv = CompileConditions(rule, conditions, aParentNode, &last);

    // If the rule compilation failed, or we don't have a container
    // symbol, then we have to bail.
    if (NS_FAILED(rv)) {
        delete rule;
        return rv;
    }

    if (mContainerSymbol.IsEmpty()) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] could not deduce container variable", this));

        delete rule;
        return NS_OK;
    }

    // And now add the instantiation node: it owns the rule now.
    nsInstantiationNode* instnode =
        new nsInstantiationNode(mConflictSet, rule, mDB);

    if (! instnode) {
        delete rule;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    last->AddChild(instnode);
    mRules.AddNode(instnode);
    
    // If we've got bindings, add 'em.
    nsCOMPtr<nsIContent> bindings;
    nsXULContentUtils::FindChildByTag(aRuleElement,
                                      kNameSpaceID_XUL,
                                      nsXULAtoms::bindings,
                                      getter_AddRefs(bindings));

    if (bindings) {
        rv = CompileBindings(rule, bindings);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileConditions(nsTemplateRule* aRule,
                                        nsIContent* aConditions,
                                        InnerNode* aParentNode,
                                        InnerNode** aLastNode)
{
    // Compile an extended rule's conditions.
    nsresult rv;

    PRUint32 count = aConditions->GetChildCount();

    for (PRUint32 i = 0; i < count; ++i) {
        nsIContent *condition = aConditions->GetChildAt(i);

        TestNode* testnode = nsnull;
        rv = CompileCondition(condition->Tag(), aRule, condition,
                              aParentNode, &testnode);
        if (NS_FAILED(rv)) return rv;

        // XXXwaterson proably wrong to just drill it straight down
        // like this.
        if (testnode) {
            aParentNode->AddChild(testnode);
            mRules.AddNode(testnode);
            aParentNode = testnode;
        }
    }

    *aLastNode = aParentNode;
    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileCondition(nsIAtom* aTag,
                                       nsTemplateRule* aRule,
                                       nsIContent* aCondition,
                                       InnerNode* aParentNode,
                                       TestNode** aResult)
{
    nsresult rv;

    if (aTag == nsXULAtoms::triple) {
        rv = CompileTripleCondition(aRule, aCondition, aParentNode, aResult);
    }
    else if (aTag == nsXULAtoms::member) {
        rv = CompileMemberCondition(aRule, aCondition, aParentNode, aResult);
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
nsXULTemplateBuilder::CompileTripleCondition(nsTemplateRule* aRule,
                                             nsIContent* aCondition,
                                             InnerNode* aParentNode,
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
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::subject, subject);

    PRInt32 svar = 0;
    nsCOMPtr<nsIRDFResource> sres;
    if (subject[0] == PRUnichar('?'))
        svar = mRules.LookupSymbol(subject.get(), PR_TRUE);
    else
        gRDFService->GetUnicodeResource(subject, getter_AddRefs(sres));

    // predicate
    nsAutoString predicate;
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::predicate, predicate);

    nsCOMPtr<nsIRDFResource> pres;
    if (predicate[0] == PRUnichar('?')) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] cannot handle variables in <triple> 'predicate'", this));

        return NS_OK;
    }
    else {
        gRDFService->GetUnicodeResource(predicate, getter_AddRefs(pres));
    }

    // object
    nsAutoString object;
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::object, object);

    PRInt32 ovar = 0;
    nsCOMPtr<nsIRDFNode> onode;
    if (object[0] == PRUnichar('?')) {
        ovar = mRules.LookupSymbol(object.get(), PR_TRUE);
    }
    else if (object.FindChar(':') != -1) { // XXXwaterson evil.
        // treat as resource
        nsCOMPtr<nsIRDFResource> resource;
        gRDFService->GetUnicodeResource(object, getter_AddRefs(resource));
        onode = do_QueryInterface(resource);
    }
    else {
        nsCOMPtr<nsIRDFLiteral> literal;
        gRDFService->GetLiteral(object.get(), getter_AddRefs(literal));
        onode = do_QueryInterface(literal);
    }

    nsRDFPropertyTestNode* testnode = nsnull;

    if (svar && ovar) {
        testnode = new nsRDFPropertyTestNode(aParentNode, mConflictSet, mDB, svar, pres, ovar);
    }
    else if (svar) {
        testnode = new nsRDFPropertyTestNode(aParentNode, mConflictSet, mDB, svar, pres, onode);
    }
    else if (ovar) {
        testnode = new nsRDFPropertyTestNode(aParentNode, mConflictSet, mDB, sres, pres, ovar);
    }
    else {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] tautology in <triple> test", this));

        return NS_OK;
    }

    if (! testnode)
        return NS_ERROR_OUT_OF_MEMORY;

    mRDFTests.Add(testnode);

    *aResult = testnode;
    return NS_OK;
}

nsresult
nsXULTemplateBuilder::CompileMemberCondition(nsTemplateRule* aRule,
                                             nsIContent* aCondition,
                                             InnerNode* aParentNode,
                                             TestNode** aResult)
{
    // Compile a <member> condition, which must be of the form:
    //
    //   <member container="?var1" child="?var2" />
    //

    // container
    nsAutoString container;
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::container, container);

    if (container[0] != PRUnichar('?')) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] on <member> test, expected 'container' attribute to name a variable", this));

        return NS_OK;
    }

    PRInt32 containervar = mRules.LookupSymbol(container.get(), PR_TRUE);

    // child
    nsAutoString child;
    aCondition->GetAttr(kNameSpaceID_None, nsXULAtoms::child, child);

    if (child[0] != PRUnichar('?')) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] on <member> test, expected 'child' attribute to name a variable", this));

        return NS_OK;
    }

    PRInt32 childvar = mRules.LookupSymbol(child.get(), PR_TRUE);

    TestNode* testnode =
        new nsRDFConMemberTestNode(aParentNode,
                                   mConflictSet,
                                   mDB,
                                   mContainmentProperties,
                                   containervar,
                                   childvar);

    if (! testnode)
        return NS_ERROR_OUT_OF_MEMORY;

    mRDFTests.Add(testnode);
    
    *aResult = testnode;
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

        nsINodeInfo *ni = binding->GetNodeInfo();

        if (ni && ni->Equals(nsXULAtoms::binding, kNameSpaceID_XUL)) {
            rv = CompileBinding(aRule, binding);
        }
        else {
#ifdef PR_LOGGING
            nsAutoString tagstr;
            if (ni) {
                ni->GetQualifiedName(tagstr);
            }

            nsCAutoString tagstrC;
            tagstrC.AssignWithConversion(tagstr);
            PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
                   ("xultemplate[%p] unrecognized binding <%s>",
                    this, tagstrC.get()));
#endif

            continue;
        }

        if (NS_FAILED(rv)) return rv;
    }

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
    aBinding->GetAttr(kNameSpaceID_None, nsXULAtoms::subject, subject);

    if (subject.IsEmpty()) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `subject'", this));

        return NS_OK;
    }

    PRInt32 svar = 0;
    if (subject[0] == PRUnichar('?')) {
        svar = mRules.LookupSymbol(subject.get(), PR_TRUE);
    }
    else {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `subject' to be a variable", this));

        return NS_OK;
    }

    // predicate
    nsAutoString predicate;
    aBinding->GetAttr(kNameSpaceID_None, nsXULAtoms::predicate, predicate);
    if (predicate.IsEmpty()) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `predicate'", this));

        return NS_OK;
    }

    nsCOMPtr<nsIRDFResource> pred;
    if (predicate[0] == PRUnichar('?')) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] cannot handle variables in <binding> `predicate'", this));

        return NS_OK;
    }
    else {
        gRDFService->GetUnicodeResource(predicate, getter_AddRefs(pred));
    }

    // object
    nsAutoString object;
    aBinding->GetAttr(kNameSpaceID_None, nsXULAtoms::object, object);

    if (object.IsEmpty()) {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `object'", this));

        return NS_OK;
    }

    PRInt32 ovar = 0;
    if (object[0] == PRUnichar('?')) {
        ovar = mRules.LookupSymbol(object.get(), PR_TRUE);
    }
    else {
        PR_LOG(gXULTemplateLog, PR_LOG_ALWAYS,
               ("xultemplate[%p] <binding> requires `object' to be a variable", this));

        return NS_OK;
    }

    return aRule->AddBinding(svar, pred, ovar);
}

nsresult
nsXULTemplateBuilder::CompileSimpleRule(nsIContent* aRuleElement,
                                        PRInt32 aPriority,
                                        InnerNode* aParentNode)
{
    // Compile a "simple" (or old-school style) <template> rule.
    nsresult rv;

    PRBool hasContainerTest = PR_FALSE;

    PRUint32 count = aRuleElement->GetAttrCount();

    // Add constraints for the LHS
    for (PRUint32 i = 0; i < count; ++i) {
        PRInt32 attrNameSpaceID;
        nsCOMPtr<nsIAtom> attr, prefix;
        rv = aRuleElement->GetAttrNameAt(i, &attrNameSpaceID,
                                         getter_AddRefs(attr),
                                         getter_AddRefs(prefix));
        if (NS_FAILED(rv)) return rv;

        // Note: some attributes must be skipped on XUL template rule subtree

        // never compare against rdf:property attribute
        if ((attr.get() == nsXULAtoms::property) && (attrNameSpaceID == kNameSpaceID_RDF))
            continue;
        // never compare against rdf:instanceOf attribute
        else if ((attr.get() == nsXULAtoms::instanceOf) && (attrNameSpaceID == kNameSpaceID_RDF))
            continue;
        // never compare against {}:id attribute
        else if ((attr.get() == nsXULAtoms::id) && (attrNameSpaceID == kNameSpaceID_None))
            continue;
        else if ((attr.get() == nsXULAtoms::parsetype) && (attrNameSpaceID == kNameSpaceID_None))
            continue;

        nsAutoString value;
        rv = aRuleElement->GetAttr(attrNameSpaceID, attr, value);
        if (NS_FAILED(rv)) return rv;

        TestNode* testnode = nsnull;

        if (CompileSimpleAttributeCondition(attrNameSpaceID, attr, value, aParentNode, &testnode)) {
            // handled by subclass
        }
        else if (((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == nsXULAtoms::iscontainer)) ||
                 ((attrNameSpaceID == kNameSpaceID_None) && (attr.get() == nsXULAtoms::isempty))) {
            // Tests about containerhood and emptiness. These can be
            // globbed together, mostly. Check to see if we've already
            // added a container test: we only need one.
            if (hasContainerTest)
                continue;

            nsRDFConInstanceTestNode::Test iscontainer =
                nsRDFConInstanceTestNode::eDontCare;

            rv = aRuleElement->GetAttr(kNameSpaceID_None, nsXULAtoms::iscontainer, value);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                if (value.EqualsLiteral("true")) {
                    iscontainer = nsRDFConInstanceTestNode::eTrue;
                }
                else if (value.EqualsLiteral("false")) {
                    iscontainer = nsRDFConInstanceTestNode::eFalse;
                }
            }

            nsRDFConInstanceTestNode::Test isempty =
                nsRDFConInstanceTestNode::eDontCare;

            rv = aRuleElement->GetAttr(kNameSpaceID_None, nsXULAtoms::isempty, value);
            if (NS_FAILED(rv)) return rv;

            if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
                if (value.EqualsLiteral("true")) {
                    isempty = nsRDFConInstanceTestNode::eTrue;
                }
                else if (value.EqualsLiteral("false")) {
                    isempty = nsRDFConInstanceTestNode::eFalse;
                }
            }

            testnode = new nsRDFConInstanceTestNode(aParentNode,
                                                    mConflictSet,
                                                    mDB,
                                                    mContainmentProperties,
                                                    mMemberVar,
                                                    iscontainer,
                                                    isempty);

            if (! testnode)
                return NS_ERROR_OUT_OF_MEMORY;

            mRDFTests.Add(testnode);
        }
        else {
            // It's a simple RDF test
            nsCOMPtr<nsIRDFResource> property;
            rv = nsXULContentUtils::GetResource(attrNameSpaceID, attr, getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            // XXXwaterson this is so manky
            nsCOMPtr<nsIRDFNode> target;
            if (value.FindChar(':') != -1) { // XXXwaterson WRONG WRONG WRONG!
                nsCOMPtr<nsIRDFResource> resource;
                rv = gRDFService->GetUnicodeResource(value, getter_AddRefs(resource));
                if (NS_FAILED(rv)) return rv;

                target = do_QueryInterface(resource);
            }
            else {                
                if (aRuleElement->HasAttr(kNameSpaceID_None, nsXULAtoms::parsetype)) {
                   nsAutoString parseType;
                   aRuleElement->GetAttr(kNameSpaceID_None, nsXULAtoms::parsetype, parseType);
                   if (parseType.EqualsLiteral("Integer")) {                     
                     nsCOMPtr<nsIRDFInt> intLiteral;
                     PRInt32 errorCode = nsnull;                     
                     rv = gRDFService->GetIntLiteral(value.ToInteger(&errorCode), getter_AddRefs(intLiteral));
                     if (NS_FAILED(rv)) return rv;
                     
                     target = do_QueryInterface(intLiteral);
                   }
                }
                else {
                   nsCOMPtr<nsIRDFLiteral> literal;
                   rv = gRDFService->GetLiteral(value.get(), getter_AddRefs(literal));
                   if (NS_FAILED(rv)) return rv;

                   target = do_QueryInterface(literal);
                }
            }

            testnode = new nsRDFPropertyTestNode(aParentNode, mConflictSet, mDB, mMemberVar, property, target);
            if (! testnode)
                return NS_ERROR_OUT_OF_MEMORY;

            mRDFTests.Add(testnode);
        }

        aParentNode->AddChild(testnode);
        mRules.AddNode(testnode);
        aParentNode = testnode;
    }

    // Create the rule.
    nsTemplateRule* rule = new nsTemplateRule(mDB, aRuleElement, aPriority);
    if (! rule)
        return NS_ERROR_OUT_OF_MEMORY;

    rule->SetContainerVariable(mContainerVar);
    rule->SetMemberVariable(mMemberVar);

    AddSimpleRuleBindings(rule, aRuleElement);

    // The InstantiationNode owns the rule now.
    nsInstantiationNode* instnode =
        new nsInstantiationNode(mConflictSet, rule, mDB);

    if (! instnode)
        return NS_ERROR_OUT_OF_MEMORY;

    aParentNode->AddChild(instnode);
    mRules.AddNode(instnode);
    
    return NS_OK;
}

PRBool
nsXULTemplateBuilder::CompileSimpleAttributeCondition(PRInt32 aNameSpaceID,
                                                      nsIAtom* aAttribute,
                                                      const nsAString& aValue,
                                                      InnerNode* aParentNode,
                                                      TestNode** aResult)
{
    // By default, not handled
    return PR_FALSE;
}

nsresult
nsXULTemplateBuilder::AddSimpleRuleBindings(nsTemplateRule* aRule, nsIContent* aElement)
{
    // Crawl the content tree of a "simple" rule, adding a variable
    // assignment for any attribute whose value is "rdf:".

    nsAutoVoidArray elements;

    elements.AppendElement(aElement);
    while (elements.Count()) {
        // Pop the next element off the stack
        PRUint32 i = (PRUint32)(elements.Count() - 1);
        nsIContent* element = NS_STATIC_CAST(nsIContent*, elements[i]);
        elements.RemoveElementAt(i);

        // Iterate through its attributes, looking for substitutions
        // that we need to add as bindings.
        PRUint32 count = element->GetAttrCount();

        for (i = 0; i < count; ++i) {
            PRInt32 nameSpaceID;
            nsCOMPtr<nsIAtom> attr, prefix;

            element->GetAttrNameAt(i, &nameSpaceID, getter_AddRefs(attr),
                                   getter_AddRefs(prefix));

            nsAutoString value;
            element->GetAttr(nameSpaceID, attr, value);

            // Scan the attribute for variables, adding a binding for
            // each one.
            ParseAttribute(value, AddBindingsFor, nsnull, aRule);
        }

        // Push kids onto the stack, and search them next.
        count = element->GetChildCount();

        while (count-- > 0) {
            elements.AppendElement(element->GetChildAt(count));
        }
    }

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

    // Lookup the variable symbol
    PRInt32 var = aThis->mRules.LookupSymbol(PromiseFlatString(aVariable).get(), PR_TRUE);

    // Strip it down to the raw RDF property by clobbering the "rdf:"
    // prefix
    const nsAString& propertyStr = Substring(aVariable, PRUint32(4), aVariable.Length() - 4);

    nsCOMPtr<nsIRDFResource> property;
    gRDFService->GetUnicodeResource(propertyStr, getter_AddRefs(property));

    if (! rule->HasBinding(aThis->mMemberVar, property, var))
        // In the simple syntax, the binding is always from the
        // member variable, through the property, to the target.
        rule->AddBinding(aThis->mMemberVar, property, var);
}

//----------------------------------------------------------------------


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

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * See bug 235665,
 * also http://www.axel-hecht.de/mozwiki/ProxyDataSource
 */

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsStringAPI.h"
#include "rdf.h"
#include "nsRDFCID.h"

#include "nsEnumeratorUtils.h"

#include "nsForwardProxyDataSource.h"

nsresult
nsForwardProxyDataSource::Init(void)
{
    nsresult rv;

    // do we need to initialize our globals?
    nsCOMPtr<nsIRDFService> rdf = do_GetService("@mozilla.org/rdf/rdf-service;1");
    if (!rdf) {
        NS_WARNING ("unable to get RDF service");
        return NS_ERROR_FAILURE;
    }

    // per-instance members: this might eventually be different per-DS,
    // so it's not static
    rv = rdf->GetResource(NS_LITERAL_CSTRING(DEVMO_NAMESPACE_URI_PREFIX "forward-proxy#forward-proxy"),
                          getter_AddRefs(mProxyProperty));
    
    return rv;
}

nsForwardProxyDataSource::nsForwardProxyDataSource()
    : mUpdateBatchNest(0)
{
}

nsForwardProxyDataSource::~nsForwardProxyDataSource()
{
}

//
// our two methods for munging sources
// note that the only two possible return values are NS_OK and NS_RDF_NO_VALUE
//
// This implementation only supports a single forward proxy arc,
// though the extension for multiple proxy arcs would be
// straightforward and is planned for the future.  (In other words,
// right now, only GetTarget will ever be used on the proxy property,
// not GetTargets.)  See bug 254959.
//

nsresult
nsForwardProxyDataSource::GetProxyResource(nsIRDFResource *aSource, nsIRDFResource **aResult)
{
    nsresult rv;
    nsCOMPtr<nsIRDFNode> proxyTarget;
    nsCOMPtr<nsIRDFResource> proxyResource;

    *aResult = nsnull;

    rv = mDS->GetTarget(aSource, mProxyProperty, PR_TRUE, getter_AddRefs(proxyTarget));
    if (NS_FAILED(rv)) return NS_RDF_NO_VALUE;

    if (rv != NS_RDF_NO_VALUE &&
        (proxyResource = do_QueryInterface(proxyTarget)) != nsnull)
    {
        proxyResource.swap(*aResult);
        return NS_OK;
    }

    return NS_RDF_NO_VALUE;
}

nsresult
nsForwardProxyDataSource::GetRealSource(nsIRDFResource *aSource, nsIRDFResource **aResult)
{
    nsresult rv;
    nsCOMPtr<nsIRDFResource> realSourceResource;

    *aResult = nsnull;

    rv = mDS->GetSource(mProxyProperty, aSource, PR_TRUE,
                        getter_AddRefs(realSourceResource));
    if (NS_FAILED(rv) || rv == NS_RDF_NO_VALUE) return NS_RDF_NO_VALUE;

    realSourceResource.swap(*aResult);
    return NS_OK;
}

//----------------------------------------------------------------------
//
// nsISupports interface
//

NS_IMPL_CYCLE_COLLECTION_CLASS(nsForwardProxyDataSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsForwardProxyDataSource)
    NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mObservers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsForwardProxyDataSource)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDS)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mObservers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsForwardProxyDataSource,
                                          nsIRDFInferDataSource)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsForwardProxyDataSource,
                                           nsIRDFInferDataSource)

NS_INTERFACE_MAP_BEGIN(nsForwardProxyDataSource)
    NS_INTERFACE_MAP_ENTRY(nsIRDFInferDataSource)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRDFDataSource, nsIRDFInferDataSource)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRDFInferDataSource)
    NS_INTERFACE_MAP_ENTRY(nsIRDFObserver)
    NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsForwardProxyDataSource)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
//
// nsIRDFDataSource interface
//

// methods which need no proxying

NS_IMETHODIMP
nsForwardProxyDataSource::GetURI(char* *uri)
{
    NS_PRECONDITION(uri != nsnull, "null pointer");
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    nsresult rv;

    nsCAutoString theURI(NS_LITERAL_CSTRING("x-rdf-infer:forward-proxy"));

    nsCString dsURI;
    rv = mDS->GetURI(getter_Copies(dsURI));
    if (NS_FAILED(rv)) return rv;

    if (!dsURI.IsEmpty()) {
        theURI += NS_LITERAL_CSTRING("?ds=");
        theURI += dsURI;
    }

    *uri = ToNewCString(theURI);
    if (*uri == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsForwardProxyDataSource::GetSource(nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue,
                                    nsIRDFResource** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->GetSource(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP
nsForwardProxyDataSource::GetSources(nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget,
                                     PRBool aTruthValue,
                                     nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->GetSources(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP
nsForwardProxyDataSource::Assert(nsIRDFResource* aSource, 
                                 nsIRDFResource* aProperty, 
                                 nsIRDFNode* aTarget,
                                 PRBool aTruthValue)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->Assert(aSource, aProperty, aTarget, aTruthValue);
}

NS_IMETHODIMP
nsForwardProxyDataSource::Unassert(nsIRDFResource* aSource,
                                   nsIRDFResource* aProperty,
                                   nsIRDFNode* aTarget)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->Unassert(aSource, aProperty, aTarget);
}

NS_IMETHODIMP
nsForwardProxyDataSource::Change(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aOldTarget,
                                 nsIRDFNode* aNewTarget)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->Change(aSource, aProperty, aOldTarget, aNewTarget);
}

NS_IMETHODIMP
nsForwardProxyDataSource::Move(nsIRDFResource* aOldSource,
                        nsIRDFResource* aNewSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->Move(aOldSource, aNewSource, aProperty, aTarget);
}

NS_IMETHODIMP 
nsForwardProxyDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->HasArcIn(aNode, aArc, aResult);
}

NS_IMETHODIMP
nsForwardProxyDataSource::ArcLabelsIn(nsIRDFNode* aTarget, nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->ArcLabelsIn(aTarget, aResult);
}

NS_IMETHODIMP
nsForwardProxyDataSource::GetAllResources(nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->GetAllResources(aResult);
}

NS_IMETHODIMP
nsForwardProxyDataSource::BeginUpdateBatch()
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->BeginUpdateBatch();
}

NS_IMETHODIMP
nsForwardProxyDataSource::EndUpdateBatch()
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    return mDS->EndUpdateBatch();
}

//
// methods which need proxying
//

NS_IMETHODIMP
nsForwardProxyDataSource::GetTarget(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    PRBool aTruthValue,
                                    nsIRDFNode** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv = NS_RDF_NO_VALUE;
    nsCOMPtr<nsIRDFResource> proxyResource;

    rv = mDS->GetTarget(aSource, aProperty, aTruthValue, aResult);
    if (NS_FAILED(rv) || rv == NS_OK)
        return rv;

    if (GetProxyResource(aSource, getter_AddRefs(proxyResource)) == NS_OK) {
        rv = mDS->GetTarget(proxyResource, aProperty, aTruthValue, aResult);
    }

    return rv;
}

NS_IMETHODIMP
nsForwardProxyDataSource::GetTargets(nsIRDFResource* aSource,
                                     nsIRDFResource* aProperty,
                                     PRBool aTruthValue,
                                     nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv;
    nsCOMPtr<nsIRDFResource> proxyResource;
    nsCOMPtr<nsISimpleEnumerator> proxyEnumerator, dsEnumerator;

    rv = mDS->GetTargets(aSource, aProperty, aTruthValue, getter_AddRefs(dsEnumerator));
    if (NS_FAILED(rv)) return rv;

    if (GetProxyResource(aSource, getter_AddRefs(proxyResource)) == NS_OK) {
        rv = mDS->GetTargets(proxyResource, aProperty,
                             aTruthValue, getter_AddRefs(proxyEnumerator));
        if (NS_FAILED(rv)) return rv;
    }

    rv = NS_NewUnionEnumerator(aResult, dsEnumerator, proxyEnumerator);

    return rv;
}

NS_IMETHODIMP
nsForwardProxyDataSource::HasAssertion(nsIRDFResource* aSource,
                                       nsIRDFResource* aProperty,
                                       nsIRDFNode* aTarget,
                                       PRBool aTruthValue,
                                       PRBool* aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv;
    nsCOMPtr<nsIRDFResource> proxyResource;

    *aResult = PR_FALSE;

    rv = mDS->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
    if (NS_SUCCEEDED(rv) && *aResult) {
        return rv;
    }

    if (GetProxyResource(aSource, getter_AddRefs(proxyResource)) == NS_OK) {
        rv = mDS->HasAssertion(proxyResource, aProperty, aTarget, aTruthValue, aResult);
    }

    return rv;
}

NS_IMETHODIMP 
nsForwardProxyDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv;
    nsCOMPtr<nsIRDFResource> proxyResource;

    *aResult = PR_FALSE;

    rv = mDS->HasArcOut(aSource, aArc, aResult);
    if (NS_SUCCEEDED(rv) && *aResult) {
        return rv;
    }

    if (GetProxyResource(aSource, getter_AddRefs(proxyResource)) == NS_OK) {
        rv = mDS->HasArcOut(proxyResource, aArc, aResult);
    }

    return rv;
}


NS_IMETHODIMP
nsForwardProxyDataSource::ArcLabelsOut(nsIRDFResource* aSource,
                                       nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv;
    nsCOMPtr<nsIRDFResource> proxyResource;
    nsCOMPtr<nsISimpleEnumerator> proxyEnumerator, dsEnumerator;

    rv = mDS->ArcLabelsOut(aSource, getter_AddRefs(dsEnumerator));
    if (NS_FAILED(rv)) return rv;

    if (GetProxyResource(aSource, getter_AddRefs(proxyResource)) == NS_OK) {
        rv = mDS->ArcLabelsOut(proxyResource, getter_AddRefs(proxyEnumerator));
        if (NS_FAILED(rv)) return rv;
    }

    rv = NS_NewUnionEnumerator(aResult, dsEnumerator, proxyEnumerator);

    return rv;
}

NS_IMETHODIMP
nsForwardProxyDataSource::GetAllCmds(nsIRDFResource* aSource,
                                     nsISimpleEnumerator** aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv;
    nsCOMPtr<nsIRDFResource> proxyResource;
    nsCOMPtr<nsISimpleEnumerator> proxyEnumerator, dsEnumerator;

    if (GetProxyResource(aSource, getter_AddRefs(proxyResource)) == NS_OK) {
        rv = mDS->GetAllCmds(proxyResource, getter_AddRefs(proxyEnumerator));
        if (NS_FAILED(rv)) return rv;
    }

    rv = mDS->GetAllCmds(aSource, getter_AddRefs(dsEnumerator));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewUnionEnumerator(aResult, dsEnumerator, proxyEnumerator);

    return rv;
}

// helper method for munging an array of sources into an array of the
// sources plus their proxy resources.

nsresult
nsForwardProxyDataSource::GetProxyResourcesArray (nsISupportsArray* aSources,
                                                  nsISupportsArray** aSourcesAndProxies)
{
    NS_ENSURE_ARG_POINTER(aSources);
    NS_ENSURE_ARG_POINTER(aSourcesAndProxies);

    nsresult rv;
    nsCOMPtr<nsISupportsArray> fixedSources;
    nsCOMPtr<nsIRDFResource> source;
    nsCOMPtr<nsIRDFResource> proxyResource;

    PRUint32 sourcesCount;
    rv = aSources->Count(&sourcesCount);
    if (sourcesCount == 0) {
        *aSourcesAndProxies = aSources;
        NS_ADDREF(*aSourcesAndProxies);
        return NS_OK;
    }

    // go through the aSources array, looking for things which
    // have aggregate resources -- if they're found, append them
    // to a new array
    for (PRUint32 i = 0; i < sourcesCount; i++) {
        rv = aSources->QueryElementAt(i, NS_GET_IID(nsIRDFResource),
                                      getter_AddRefs(source));
        if (NS_FAILED(rv)) return rv;

        if (GetProxyResource(source, getter_AddRefs(proxyResource)) == NS_OK) {
            if (!fixedSources) {
                rv = aSources->Clone(getter_AddRefs(fixedSources));
                if (NS_FAILED(rv)) return rv;
            }

            rv = fixedSources->AppendElement(proxyResource);
            if (NS_FAILED(rv)) return rv;
        }
    }

    if (!fixedSources) {
        *aSourcesAndProxies = aSources;
    } else {
        *aSourcesAndProxies = fixedSources;
    }
    NS_ADDREF(*aSourcesAndProxies);

    return NS_OK;
}

NS_IMETHODIMP
nsForwardProxyDataSource::IsCommandEnabled(nsISupportsArray* aSources,
                                           nsIRDFResource* aCommand,
                                           nsISupportsArray* aArguments,
                                           PRBool* aResult)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv;
    nsCOMPtr<nsISupportsArray> fixedSources;

    rv = GetProxyResourcesArray(aSources, getter_AddRefs(fixedSources));
    if (NS_FAILED(rv)) return rv;

    return mDS->IsCommandEnabled(fixedSources, aCommand, aArguments, aResult);
}

NS_IMETHODIMP
nsForwardProxyDataSource::DoCommand(nsISupportsArray* aSources,
                                    nsIRDFResource* aCommand,
                                    nsISupportsArray* aArguments)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    nsresult rv;
    nsCOMPtr<nsISupportsArray> fixedSources;

    rv = GetProxyResourcesArray(aSources, getter_AddRefs(fixedSources));
    if (NS_FAILED(rv)) return rv;

    return mDS->DoCommand(fixedSources, aCommand, aArguments);
}


// Observer bits

NS_IMETHODIMP
nsForwardProxyDataSource::AddObserver(nsIRDFObserver* aObserver)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver) {
        return NS_ERROR_NULL_POINTER;
    }

    mObservers.AppendObject(aObserver);

    return NS_OK;
}

NS_IMETHODIMP
nsForwardProxyDataSource::RemoveObserver(nsIRDFObserver* aObserver)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver) {
        return NS_ERROR_NULL_POINTER;
    }

    mObservers.RemoveObject(aObserver);

    return NS_OK;
}

//
// nsIRDFObserver impl
//

// if a given notification source is a proxied resource, we generate
// notifications for the real source as well.

NS_IMETHODIMP
nsForwardProxyDataSource::OnAssert(nsIRDFDataSource* aDataSource,
                                   nsIRDFResource* aSource,
                                   nsIRDFResource* aProperty,
                                   nsIRDFNode* aTarget)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        mObservers[i]->OnAssert(this, aSource, aProperty, aTarget);
    }

    nsCOMPtr<nsIRDFResource> realSourceResource;
    if (GetRealSource(aSource, getter_AddRefs(realSourceResource)) == NS_OK) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            mObservers[i]->OnAssert(this, realSourceResource, aProperty, aTarget);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsForwardProxyDataSource::OnUnassert(nsIRDFDataSource* aDataSource,
                                     nsIRDFResource* aSource,
                                     nsIRDFResource* aProperty,
                                     nsIRDFNode* aTarget)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        mObservers[i]->OnUnassert(this, aSource, aProperty, aTarget);
    }

    nsCOMPtr<nsIRDFResource> realSourceResource;

    if (GetRealSource(aSource, getter_AddRefs(realSourceResource)) == NS_OK) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            mObservers[i]->OnUnassert(this, realSourceResource, aProperty, aTarget);
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsForwardProxyDataSource::OnChange(nsIRDFDataSource* aDataSource,
                                   nsIRDFResource* aSource,
                                   nsIRDFResource* aProperty,
                                   nsIRDFNode* aOldTarget,
                                   nsIRDFNode* aNewTarget)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
        mObservers[i]->OnChange(this, aSource, aProperty, aOldTarget, aNewTarget);
    }

    nsCOMPtr<nsIRDFResource> realSourceResource;

    if (GetRealSource(aSource, getter_AddRefs(realSourceResource)) == NS_OK) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            mObservers[i]->OnChange(this, realSourceResource, aProperty, aOldTarget, aNewTarget);
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsForwardProxyDataSource::OnMove(nsIRDFDataSource* aDataSource,
                                 nsIRDFResource* aOldSource,
                                 nsIRDFResource* aNewSource,
                                 nsIRDFResource* aProperty,
                                 nsIRDFNode* aTarget)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    PRInt32 i;

    for (i = mObservers.Count() - 1; i >= 0; --i) {
        mObservers[i]->OnChange(this, aOldSource, aNewSource, aProperty, aTarget);
    }

    nsCOMPtr<nsIRDFResource> realOldResource;
    nsCOMPtr<nsIRDFResource> realNewResource;

    GetRealSource(aOldSource, getter_AddRefs(realOldResource));
    GetRealSource(aNewSource, getter_AddRefs(realNewResource));

    if ((!realOldResource && !realNewResource) ||
        (realOldResource == realNewResource)) {
        return NS_OK;
    }

    for (i = mObservers.Count() - 1; i >= 0; --i) {
        mObservers[i]->OnChange(this,
                                realOldResource ? realOldResource.get() : aOldSource,
                                realNewResource ? realNewResource.get() : aNewSource,
                                aProperty, aTarget);
    }

    return NS_OK;
}


NS_IMETHODIMP
nsForwardProxyDataSource::OnBeginUpdateBatch(nsIRDFDataSource* aDataSource)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");

    if (mUpdateBatchNest++ == 0) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            mObservers[i]->OnBeginUpdateBatch(this);
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
nsForwardProxyDataSource::OnEndUpdateBatch(nsIRDFDataSource* aDataSource)
{
    NS_PRECONDITION(mDS != nsnull, "Null datasource");
    NS_ASSERTION(mUpdateBatchNest > 0, "badly nested update batch");
    if (--mUpdateBatchNest == 0) {
        for (PRInt32 i = mObservers.Count() - 1; i >= 0; --i) {
            mObservers[i]->OnEndUpdateBatch(this);
        }
    }
    return NS_OK;
}

//
// nsIRDFInferDataSource
//

NS_IMETHODIMP
nsForwardProxyDataSource::SetBaseDataSource(nsIRDFDataSource *aDataSource)
{
    NS_ENSURE_ARG_POINTER(aDataSource);

    if (mDS != nsnull) {
        return NS_ERROR_UNEXPECTED;
    }

    mDS = aDataSource;
    mDS->AddObserver(this);
    return NS_OK;
}

NS_IMETHODIMP
nsForwardProxyDataSource::GetBaseDataSource(nsIRDFDataSource **aDataSource)
{
    NS_ENSURE_ARG_POINTER(aDataSource);

    *aDataSource = mDS;
    NS_ADDREF(*aDataSource);
    return NS_OK;
}


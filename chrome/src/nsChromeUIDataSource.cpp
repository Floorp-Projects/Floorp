/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsChromeUIDataSource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsCRT.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFContainer.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsISupportsArray.h"
#include "nsIIOService.h"
#include "nsIResProtocolHandler.h"

////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsChromeUIDataSource::nsChromeUIDataSource(nsIRDFDataSource* aComposite)
{
	NS_INIT_REFCNT();
  mComposite = aComposite;
  mComposite->AddObserver(this);

  nsresult rv;
  rv = nsServiceManager::GetService(kRDFServiceCID,
                                    NS_GET_IID(nsIRDFService),
                                    (nsISupports**)&mRDFService);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");

  mRDFService->RegisterDataSource(this, PR_FALSE);
}

nsChromeUIDataSource::~nsChromeUIDataSource()
{
  mComposite->RemoveObserver(this);

  mRDFService->UnregisterDataSource(this);

  if (mRDFService) {
    nsServiceManager::ReleaseService(kRDFServiceCID, mRDFService);
    mRDFService = nsnull;
  }
}

NS_IMPL_ISUPPORTS2(nsChromeUIDataSource, nsIRDFDataSource, nsIRDFObserver);


//----------------------------------------------------------------------
//
// nsIRDFDataSource interface
//

NS_IMETHODIMP
nsChromeUIDataSource::GetURI(char** aURI)
{
  *aURI = nsXPIDLCString::Copy("rdf:chrome");
	if (! *aURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::GetSource(nsIRDFResource* property,
                                nsIRDFNode* target,
                                PRBool tv,
                                nsIRDFResource** source)
{
  return mComposite->GetSource(property, target, tv, source);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetSources(nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget,
                                    PRBool aTruthValue,
                                    nsISimpleEnumerator** aResult)
{
  return mComposite->GetSources(aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetTarget(nsIRDFResource* aSource,
                                   nsIRDFResource* aProperty,
                                   PRBool aTruthValue,
                                   nsIRDFNode** aResult)
{
  return mComposite->GetTarget(aSource, aProperty, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetTargets(nsIRDFResource* aSource,
                                 nsIRDFResource* aProperty,
                                 PRBool aTruthValue,
                                 nsISimpleEnumerator** aResult)
{
  return mComposite->GetTargets(aSource, aProperty, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::Assert(nsIRDFResource* aSource, 
                             nsIRDFResource* aProperty, 
                             nsIRDFNode* aTarget,
                             PRBool aTruthValue)
{
  nsresult rv = mComposite->Assert(aSource, aProperty, aTarget, aTruthValue);
  if (mObservers) {
    PRUint32 count;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnAssert(aSource, aProperty, aTarget);
      NS_RELEASE(obs);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsChromeUIDataSource::Unassert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
  nsresult rv = mComposite->Unassert(aSource, aProperty, aTarget);
  if (mObservers) {
    PRUint32 count;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnUnassert(aSource, aProperty, aTarget);
      NS_RELEASE(obs);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsChromeUIDataSource::Change(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aOldTarget,
                                nsIRDFNode* aNewTarget)
{
  nsresult rv = mComposite->Change(aSource, aProperty, aOldTarget, aNewTarget);
  if (mObservers) {
    PRUint32 count;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnChange(aSource, aProperty, aOldTarget, aNewTarget);
      NS_RELEASE(obs);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsChromeUIDataSource::Move(nsIRDFResource* aOldSource,
                              nsIRDFResource* aNewSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aTarget)
{
  nsresult rv = mComposite->Move(aOldSource, aNewSource, aProperty, aTarget);
  if (mObservers) {
    PRUint32 count;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnMove(aOldSource, aNewSource, aProperty, aTarget);
      NS_RELEASE(obs);
    }
  }
  return rv;
}


NS_IMETHODIMP
nsChromeUIDataSource::HasAssertion(nsIRDFResource* aSource,
                                      nsIRDFResource* aProperty,
                                      nsIRDFNode* aTarget,
                                      PRBool aTruthValue,
                                      PRBool* aResult)
{
  return mComposite->HasAssertion(aSource, aProperty, aTarget, aTruthValue, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::AddObserver(nsIRDFObserver* aObserver)
{
  NS_PRECONDITION(aObserver != nsnull, "null ptr");
  if (! aObserver)
      return NS_ERROR_NULL_POINTER;

  if (!mObservers) {
      nsresult rv;
      rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
      if (NS_FAILED(rv)) return rv;
  }

  // XXX ensure uniqueness?

  mObservers->AppendElement(aObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::RemoveObserver(nsIRDFObserver* aObserver)
{
  NS_PRECONDITION(aObserver != nsnull, "null ptr");
  if (! aObserver)
      return NS_ERROR_NULL_POINTER;

  if (!mObservers)
      return NS_OK;

  mObservers->RemoveElement(aObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::ArcLabelsIn(nsIRDFNode* aTarget, nsISimpleEnumerator** aResult)
{
  return mComposite->ArcLabelsIn(aTarget, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::ArcLabelsOut(nsIRDFResource* aSource,
                                      nsISimpleEnumerator** aResult)
{
  return mComposite->ArcLabelsOut(aSource, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetAllResources(nsISimpleEnumerator** aResult)
{
  return mComposite->GetAllResources(aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetAllCommands(nsIRDFResource* source,
                                     nsIEnumerator/*<nsIRDFResource>*/** result)
{
  return mComposite->GetAllCommands(source, result);
}

NS_IMETHODIMP
nsChromeUIDataSource::GetAllCmds(nsIRDFResource* source,
                                        nsISimpleEnumerator/*<nsIRDFResource>*/** result)
{
	return mComposite->GetAllCmds(source, result);
}

NS_IMETHODIMP
nsChromeUIDataSource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                          nsIRDFResource*   aCommand,
                                          nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                          PRBool* aResult)
{
  return mComposite->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
}

NS_IMETHODIMP
nsChromeUIDataSource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                   nsIRDFResource*   aCommand,
                                   nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  return mComposite->DoCommand(aSources, aCommand, aArguments);
}

//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsChromeUIDataSource::OnAssert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
  if (mObservers) {
    PRUint32 count;
    nsresult rv;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnAssert(aSource, aProperty, aTarget);
      NS_RELEASE(obs);
      // XXX ignore return value?
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeUIDataSource::OnUnassert(nsIRDFResource* aSource,
                                    nsIRDFResource* aProperty,
                                    nsIRDFNode* aTarget)
{
  if (mObservers) {
    PRUint32 count;
    nsresult rv;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnUnassert(aSource, aProperty, aTarget);
      NS_RELEASE(obs);
      // XXX ignore return value?
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsChromeUIDataSource::OnChange(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aOldTarget,
                                  nsIRDFNode* aNewTarget)
{
  if (mObservers) {
    PRUint32 count;
    nsresult rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnChange(aSource, aProperty, aOldTarget, aNewTarget);
      NS_RELEASE(obs);
      // XXX ignore return value?
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsChromeUIDataSource::OnMove(nsIRDFResource* aOldSource,
                                nsIRDFResource* aNewSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aTarget)
{
  if (mObservers) {
    PRUint32 count;
    nsresult rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = PRInt32(count) - 1; i >= 0; --i) {
      nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
      obs->OnMove(aOldSource, aNewSource, aProperty, aTarget);
      NS_RELEASE(obs);
      // XXX ignore return value?
    }
  }
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////
nsresult
NS_NewChromeUIDataSource(nsIRDFDataSource* aComposite, nsIRDFDataSource** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
      return NS_ERROR_NULL_POINTER;

  // No addrefs. The composite addrefs us already.
  nsChromeUIDataSource* ChromeUIDataSource = new nsChromeUIDataSource(aComposite);
  if (ChromeUIDataSource == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
  *aResult = ChromeUIDataSource;
  return NS_OK;
}

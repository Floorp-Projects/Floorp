/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsMsgRDFDataSource.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsMsgRDFUtils.h"
#include "nsEnumeratorUtils.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsMsgRDFDataSource::nsMsgRDFDataSource():
    mRDFService(nsnull)
{
    NS_INIT_REFCNT();
}

nsMsgRDFDataSource::~nsMsgRDFDataSource()
{
    if (mRDFService) nsServiceManager::ReleaseService(kRDFServiceCID,
                                                      mRDFService,
                                                      this);
}

/* void Init (); */
nsresult
nsMsgRDFDataSource::Init()
{
    nsresult rv=NS_OK;
    
    getRDFService();
    
    rv = mRDFService->RegisterDataSource(this, PR_FALSE);
    if (!rv) return rv;
    
    return rv;
}


NS_IMPL_ADDREF(nsMsgRDFDataSource)
NS_IMPL_RELEASE(nsMsgRDFDataSource)
    
nsresult
nsMsgRDFDataSource::QueryInterface(const nsIID& iid, void **result)
{
    nsresult rv = NS_NOINTERFACE;
  if (! result)
    return NS_ERROR_NULL_POINTER;

  // we have to use kISupportsIID and do the static cast for nsISupports
  // because otherwise gcc/egcs complains about an ambiguous nsISupports
  void *res=nsnull;
  
  if (iid.Equals(nsCOMTypeInfo<nsIRDFDataSource>::GetIID()) ||
      iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
      res = NS_STATIC_CAST(nsIRDFDataSource*, this);
  else if(iid.Equals(nsCOMTypeInfo<nsIShutdownListener>::GetIID()))
      res = NS_STATIC_CAST(nsIShutdownListener*, this);

  if (res) {
      NS_ADDREF(this);
      *result = res;
      rv = NS_OK;
  }

  return rv;
}


/* readonly attribute string URI; */
NS_IMETHODIMP
nsMsgRDFDataSource::GetURI(char * *aURI)
{
    NS_NOTREACHED("should be implemented by a subclass");
    return NS_ERROR_UNEXPECTED;
}


/* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgRDFDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgRDFDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgRDFDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgRDFDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgRDFDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_RDF_NO_VALUE;
}


/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP
nsMsgRDFDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsMsgRDFDataSource::Change(nsIRDFResource *aSource,
                           nsIRDFResource *aProperty,
                           nsIRDFNode *aOldTarget,
                           nsIRDFNode *aNewTarget)
{
    return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsMsgRDFDataSource::Move(nsIRDFResource *aOldSource,
                         nsIRDFResource *aNewSource,
                         nsIRDFResource *aProperty,
                         nsIRDFNode *aTarget)
{
    return NS_RDF_NO_VALUE;
}


/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgRDFDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}


/* void AddObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsMsgRDFDataSource::AddObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers) {
    nsresult rv;
    rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    if (NS_FAILED(rv)) return rv;
  }
  mObservers->AppendElement(aObserver);
  return NS_OK;
}


/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsMsgRDFDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
  if (! mObservers)
    return NS_OK;
  mObservers->RemoveElement(aObserver);
  return NS_OK;
}


/* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
NS_IMETHODIMP
nsMsgRDFDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
 //return empty enumerator
  nsCOMPtr<nsISupportsArray> arcs;

  nsresult rv = NS_NewISupportsArray(getter_AddRefs(arcs));
  if(NS_FAILED(rv))
	  return rv;

  nsArrayEnumerator* arrayEnumerator =
    new nsArrayEnumerator(arcs);
  
  if (arrayEnumerator == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(arrayEnumerator);
  *_retval = arrayEnumerator;

  return NS_OK;
}


/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgRDFDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP
nsMsgRDFDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgRDFDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgRDFDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_RDF_NO_VALUE;
}


/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsMsgRDFDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    return NS_RDF_NO_VALUE;
}


/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP
nsMsgRDFDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsMsgRDFDataSource::OnShutdown(const nsCID& aClass, nsISupports* service)
{
    
    // the question is do we release the service or what?
    // at the very least we set our member variable to nsnull so
    // that getRDFService knows to re-get the service
    mRDFService=nsnull;
    return NS_OK;
}

nsIRDFService *
nsMsgRDFDataSource::getRDFService()
{
    
    if (!mRDFService) {
        nsresult rv;
        
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          nsCOMTypeInfo<nsIRDFService>::GetIID(),
                                          (nsISupports**) &mRDFService,
                                          this);
        if (NS_FAILED(rv)) return nsnull;
    }
    
    return mRDFService;
}

nsresult nsMsgRDFDataSource::NotifyObservers(nsIRDFResource *subject,
                                                nsIRDFResource *property,
                                                nsIRDFNode *object,
                                                PRBool assert)
{
	if(mObservers)
	{
		nsMsgRDFNotification note = { subject, property, object };
		if (assert)
			mObservers->EnumerateForwards(assertEnumFunc, &note);
		else
			mObservers->EnumerateForwards(unassertEnumFunc, &note);
  }
	return NS_OK;
}

PRBool
nsMsgRDFDataSource::assertEnumFunc(nsISupports *aElement, void *aData)
{
  nsMsgRDFNotification *note = (nsMsgRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;
  
  observer->OnAssert(note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

PRBool
nsMsgRDFDataSource::unassertEnumFunc(nsISupports *aElement, void *aData)
{
  nsMsgRDFNotification* note = (nsMsgRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

nsresult 
nsMsgRDFDataSource::GetTransactionManager(nsISupportsArray *aSources, nsITransactionManager **aTransactionManager)
{
	if(!aTransactionManager)
		return NS_ERROR_NULL_POINTER;

	*aTransactionManager = nsnull;
	nsresult rv = NS_OK;

	nsCOMPtr<nsITransactionManager> transactionManager;

	PRUint32 cnt;

	rv = aSources->Count(&cnt);
	if (NS_FAILED(rv)) return rv;

	if (cnt > 0)
	{
		nsCOMPtr<nsISupports> supports;

		supports = getter_AddRefs(aSources->ElementAt(0));
		transactionManager = do_QueryInterface(supports, &rv);
		if (NS_SUCCEEDED(rv) && transactionManager)
		{
			aSources->RemoveElementAt(0);
			*aTransactionManager = transactionManager;
			NS_IF_ADDREF(*aTransactionManager);
		}
	}

	return NS_OK;	
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMsgRDFDataSource.h"
#include "nsRDFCID.h"
#include "rdf.h"
#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsMsgRDFUtils.h"
#include "nsEnumeratorUtils.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsMsgRDFDataSource::nsMsgRDFDataSource():
    m_shuttingDown(PR_FALSE),
    mInitialized(PR_FALSE),
    mRDFService(nsnull)
{
    NS_INIT_REFCNT();

    // do one-time initialization here
    
    NS_NewISupportsArray(getter_AddRefs(mObservers));
}

nsMsgRDFDataSource::~nsMsgRDFDataSource()
{
    // final shutdown happens here
    NS_ASSERTION(!mInitialized, "Object going away without cleanup, possibly dangerous!");
    if (mInitialized) Cleanup();
}

/* initialization happens here - object is constructed,
   but possibly partially shut down
*/
nsresult
nsMsgRDFDataSource::Init()
{
    nsresult rv=NS_OK;

    if (mInitialized)
        return NS_ERROR_ALREADY_INITIALIZED;
    
    /* Add an observer to XPCOM shutdown */
    nsCOMPtr<nsIObserverService> obs = do_GetService(NS_OBSERVERSERVICE_CONTRACTID,
                                                     &rv);
    if (NS_FAILED(rv)) return rv;
    rv = obs->AddObserver(NS_STATIC_CAST(nsIObserver*, this), NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    /* Get and keep the rdf service. Will be released by the observer */
    getRDFService();
    
    mInitialized=PR_TRUE;
    return rv;
}

// clean yourself up - undo anything you did in Init()
void nsMsgRDFDataSource::Cleanup()
{
    nsresult rv;
    mRDFService = nsnull;
    
    // release ourselves from the observer service
    nsCOMPtr<nsIObserverService> obs = do_GetService(NS_OBSERVERSERVICE_CONTRACTID,
                                                     &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = obs->RemoveObserver(NS_STATIC_CAST(nsIObserver*, this),
                                 NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }
    
    // release the window
	mWindow = nsnull;

    mInitialized = PR_FALSE;
}

NS_IMPL_ADDREF(nsMsgRDFDataSource)
NS_IMPL_RELEASE(nsMsgRDFDataSource)

NS_INTERFACE_MAP_BEGIN(nsMsgRDFDataSource)
	NS_INTERFACE_MAP_ENTRY(nsIRDFDataSource)
	NS_INTERFACE_MAP_ENTRY(nsIObserver)
	NS_INTERFACE_MAP_ENTRY(nsIMsgRDFDataSource)
	NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRDFDataSource)
NS_INTERFACE_MAP_END


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
    // make sure we're initialized
    if (!mInitialized)
        Init();
    
    NS_ASSERTION(mObservers->IndexOf(aObserver) == -1,
                 "better not already be observing this");

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

  // when we hit 0 observers, then it's probably time
  // to go away - clean ourselves up now
  PRUint32 count;
  mObservers->Count(&count);
  if (count == 0)
      Cleanup();

  return NS_OK;
}


NS_IMETHODIMP 
nsMsgRDFDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsMsgRDFDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  *result = PR_FALSE;
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

  return NS_NewArrayEnumerator(_retval, arcs);
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


/* XPCOM Shutdown observer */
NS_IMETHODIMP
nsMsgRDFDataSource::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData )
{
	m_shuttingDown = PR_TRUE;
	Cleanup();
	return NS_OK;
}


NS_IMETHODIMP nsMsgRDFDataSource::GetWindow(nsIMsgWindow * *aWindow)
{
	if(!aWindow)
		return NS_ERROR_NULL_POINTER;

	*aWindow = mWindow;
	NS_IF_ADDREF(*aWindow);
	return NS_OK;
}

NS_IMETHODIMP nsMsgRDFDataSource::SetWindow(nsIMsgWindow * aWindow)
{
	mWindow = aWindow;
	return NS_OK;
}


nsIRDFService *
nsMsgRDFDataSource::getRDFService()
{
    if (!mRDFService && !m_shuttingDown) {
        nsresult rv;
        mRDFService = do_GetService(kRDFServiceCID, &rv);
        if (NS_FAILED(rv)) return nsnull;
    }
    
    return mRDFService;
}

nsresult nsMsgRDFDataSource::NotifyPropertyChanged(nsIRDFResource *resource,
													  nsIRDFResource *propertyResource,
													  nsIRDFNode *newNode)
{

	NotifyObservers(resource, propertyResource, newNode, PR_FALSE, PR_TRUE);
	return NS_OK;

}

nsresult nsMsgRDFDataSource::NotifyObservers(nsIRDFResource *subject,
                                                nsIRDFResource *property,
                                                nsIRDFNode *object,
                                                PRBool assert, PRBool change)
{
    NS_ASSERTION(!(change && assert),
                 "Can't change and assert at the same time!\n");
    
	if(mObservers)
	{
		nsMsgRDFNotification note = { this, subject, property, object };
		if(change)
			mObservers->EnumerateForwards(changeEnumFunc, &note);
		else if (assert)
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
  
  observer->OnAssert(note->datasource,
                     note->subject,
                     note->property,
                     note->object);
  return PR_TRUE;
}

PRBool
nsMsgRDFDataSource::unassertEnumFunc(nsISupports *aElement, void *aData)
{
  nsMsgRDFNotification* note = (nsMsgRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnUnassert(note->datasource,
                       note->subject,
                       note->property,
                       note->object);
  return PR_TRUE;
}

PRBool
nsMsgRDFDataSource::changeEnumFunc(nsISupports *aElement, void *aData)
{
  nsMsgRDFNotification* note = (nsMsgRDFNotification *)aData;
  nsIRDFObserver* observer = (nsIRDFObserver *)aElement;

  observer->OnChange(note->datasource,
                     note->subject,
                     note->property,
                     nsnull, note->object);
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


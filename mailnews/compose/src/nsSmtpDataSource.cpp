/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsXPIDLString.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"

#include "nsISmtpService.h"
#include "nsISmtpServer.h"
#include "nsSmtpDataSource.h"
#include "nsMsgCompCID.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);

#define NC_RDF_SMTPSERVERS "NC:smtpservers"

nsrefcnt nsSmtpDataSource::gRefCount = 0;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_Child;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_Name;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_Key;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_SmtpServers;

nsCOMPtr<nsISupportsArray> nsSmtpDataSource::mServerArcsOut;
nsCOMPtr<nsISupportsArray> nsSmtpDataSource::mServerRootArcsOut;

NS_IMPL_ISUPPORTS1(nsSmtpDataSource, nsIRDFDataSource)

nsSmtpDataSource::nsSmtpDataSource()
{
    NS_INIT_ISUPPORTS();
    gRefCount++;
    if (gRefCount == 1)
        initGlobalObjects();
}

nsSmtpDataSource::~nsSmtpDataSource()
{
  
}

nsresult
nsSmtpDataSource::initGlobalObjects()
{
    nsresult rv;
    nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rdf->GetResource(NC_RDF_CHILD, getter_AddRefs(kNC_Child));
    rdf->GetResource(NC_RDF_NAME, getter_AddRefs(kNC_Name));
    rdf->GetResource(NC_RDF_KEY, getter_AddRefs(kNC_Key));
    rdf->GetResource(NC_RDF_SMTPSERVERS, getter_AddRefs(kNC_SmtpServers));

    rv = NS_NewISupportsArray(getter_AddRefs(mServerArcsOut));
    if (NS_FAILED(rv)) return rv;
    mServerArcsOut->AppendElement(kNC_Name);
    mServerArcsOut->AppendElement(kNC_Key);

    rv = NS_NewISupportsArray(getter_AddRefs(mServerRootArcsOut));
    mServerArcsOut->AppendElement(kNC_Child);
    mServerArcsOut->AppendElement(kNC_SmtpServers);

    return NS_OK;
}

/* readonly attribute string URI; */
NS_IMETHODIMP nsSmtpDataSource::GetURI(char * *aURI)
{
    *aURI = nsCRT::strdup("NC:smtpservers");
    return NS_OK;
}

/* nsIRDFResource GetSource (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsSmtpDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetSources (in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsSmtpDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP 
nsSmtpDataSource::GetTarget(nsIRDFResource *aSource,
                            nsIRDFResource *aProperty, 
                            PRBool aTruthValue, nsIRDFNode **aResult)
{
    nsresult rv;
    nsCOMPtr<nsISmtpServer> smtpServer;
    rv = aSource->GetDelegate("smtpserver", NS_GET_IID(nsISmtpServer),
                              (void **)getter_AddRefs(smtpServer));
    if (NS_SUCCEEDED(rv)) {
        nsXPIDLCString str;
        if (aProperty == kNC_Name.get()) {
            smtpServer->GetHostname(getter_Copies(str));
        } else if (aProperty == kNC_Key.get()) {
            smtpServer->GetKey(getter_Copies(str));
        }
    }

    return rv;
}

/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP nsSmtpDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **aResult)
{
    if (aSource == kNC_SmtpServers.get()) {
        // enumerate the smtp servers
    }

    return NS_OK;
}

nsresult
nsSmtpDataSource::GetSmtpServerTargets(nsISupportsArray **aResultArray)
{
    nsresult rv;
    nsCOMPtr<nsISmtpService> smtpService =
        do_GetService(kSmtpServiceCID, &rv);
    NS_ENSURE_TRUE(rv, rv);

    nsCOMPtr<nsIRDFService> rdf =
        do_GetService(kRDFServiceCID, &rv);
    
    nsCOMPtr<nsISupportsArray> smtpServers;
    rv = smtpService->GetSmtpServers(getter_AddRefs(smtpServers));
    NS_ENSURE_TRUE(rv, rv);
    
    nsCOMPtr<nsISupportsArray> smtpServerResources;
    rv = NS_NewISupportsArray(getter_AddRefs(smtpServerResources));
    
    PRUint32 count;
    rv = smtpServers->Count(&count);
    NS_ENSURE_TRUE(rv, rv);

    PRUint32 i;
    for (i=0; i<count; i++) {
        nsCOMPtr<nsISmtpServer> smtpServer;
        rv = smtpServers->QueryElementAt(i, NS_GET_IID(nsISmtpServer),
                                    (void **)getter_AddRefs(smtpServer));
        if (NS_FAILED(rv)) continue;
        
        nsXPIDLCString smtpServerUri;
        rv = smtpServer->GetServerURI(getter_Copies(smtpServerUri));
        if (NS_FAILED(rv)) continue;

        nsCOMPtr<nsIRDFResource> smtpServerResource;
        rv = rdf->GetResource(smtpServerUri, getter_AddRefs(smtpServerResource));
        if (NS_FAILED(rv)) continue;

        rv = smtpServerResources->AppendElement(smtpServerResource);
    }
    

    return NS_OK;
}

/* void Assert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsSmtpDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP nsSmtpDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Change (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aOldTarget, in nsIRDFNode aNewTarget); */
NS_IMETHODIMP nsSmtpDataSource::Change(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aOldTarget, nsIRDFNode *aNewTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Move (in nsIRDFResource aOldSource, in nsIRDFResource aNewSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget); */
NS_IMETHODIMP nsSmtpDataSource::Move(nsIRDFResource *aOldSource, nsIRDFResource *aNewSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource aProperty, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP nsSmtpDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP nsSmtpDataSource::AddObserver(nsIRDFObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP nsSmtpDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
NS_IMETHODIMP nsSmtpDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP nsSmtpDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **aResult)
{
    nsresult rv;
    nsCOMPtr<nsISmtpServer> smtpServer;
    rv = aSource->GetDelegate("smtpserver", NS_GET_IID(nsISmtpServer),
                              (void **)getter_AddRefs(smtpServer));
    if (NS_SUCCEEDED(rv)) {
        NS_NewArrayEnumerator(aResult, mServerArcsOut);
    } else {
        // empty array
        nsCOMPtr<nsISupportsArray> array;
        NS_NewISupportsArray(getter_AddRefs(array));
        rv = NS_NewArrayEnumerator(aResult, array);
    }
    return rv;
}

/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP nsSmtpDataSource::GetAllResources(nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP nsSmtpDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP nsSmtpDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource aCommand, in nsISupportsArray aArguments); */
NS_IMETHODIMP nsSmtpDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetAllCmds (in nsIRDFResource aSource); */
NS_IMETHODIMP nsSmtpDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

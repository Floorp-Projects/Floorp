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
 * Alec Flett <alecf@netscape.com>
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

#include "nsXPIDLString.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsIServiceManager.h"

#include "nsISmtpService.h"
#include "nsISmtpServer.h"
#include "nsSmtpDataSource.h"
#include "nsMsgCompCID.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kSmtpServiceCID, NS_SMTPSERVICE_CID);

#define NC_RDF_SMTPSERVERS "NC:smtpservers"
#define NC_RDF_ISDEFAULT        NC_NAMESPACE_URI "IsDefaultServer"
#define NC_RDF_ISSESSIONDEFAULT NC_NAMESPACE_URI "IsSessionDefaultServer"

nsrefcnt nsSmtpDataSource::gRefCount = 0;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_Child;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_Name;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_Key;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_SmtpServers;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_IsDefaultServer;
nsCOMPtr<nsIRDFResource> nsSmtpDataSource::kNC_IsSessionDefaultServer;

nsCOMPtr<nsIRDFLiteral> nsSmtpDataSource::kTrueLiteral;

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
    rdf->GetResource(NC_RDF_ISDEFAULT, getter_AddRefs(kNC_IsDefaultServer));
    rdf->GetResource(NC_RDF_ISSESSIONDEFAULT, getter_AddRefs(kNC_IsSessionDefaultServer));

    nsAutoString trueStr; trueStr.AssignWithConversion("true");
    rdf->GetLiteral(trueStr.get(), getter_AddRefs(kTrueLiteral));

    // now create cached arrays for each type we support
    rv = NS_NewISupportsArray(getter_AddRefs(mServerArcsOut));
    if (NS_FAILED(rv)) return rv;
    mServerArcsOut->AppendElement(kNC_Name);
    mServerArcsOut->AppendElement(kNC_Key);
    mServerArcsOut->AppendElement(kNC_IsDefaultServer);
    mServerArcsOut->AppendElement(kNC_IsSessionDefaultServer);

    rv = NS_NewISupportsArray(getter_AddRefs(mServerRootArcsOut));
    mServerRootArcsOut->AppendElement(kNC_Child);
    mServerRootArcsOut->AppendElement(kNC_SmtpServers);
    
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
    nsXPIDLCString str;
        
    *aResult = nsnull;
    
    nsCOMPtr<nsISmtpServer> smtpServer;
    rv = aSource->GetDelegate("smtpserver", NS_GET_IID(nsISmtpServer),
                              (void **)getter_AddRefs(smtpServer));
    
    if (NS_FAILED(rv))
        return NS_RDF_NO_VALUE;

    if (aProperty == kNC_Name.get() ||
        aProperty == kNC_Key.get()) {
    
        if (aProperty == kNC_Name.get()) {
            smtpServer->GetHostname(getter_Copies(str));
        } else if (aProperty == kNC_Key.get()) {
            smtpServer->GetKey(getter_Copies(str));
        }
        
        nsCOMPtr<nsIRDFService> rdf =
            do_GetService(kRDFServiceCID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        
        nsAutoString unicodeString;
        unicodeString.AssignWithConversion((const char*)str);
        
        nsCOMPtr<nsIRDFLiteral> literalResult;
        rv = rdf->GetLiteral(unicodeString.get(), getter_AddRefs(literalResult));
        NS_ENSURE_SUCCESS(rv, rv);
        
        *aResult = literalResult;
        NS_ADDREF(*aResult);
    }

    else if (aProperty == kNC_IsDefaultServer.get() ||
             aProperty == kNC_IsSessionDefaultServer.get()) {

        nsCOMPtr<nsISmtpService> smtpService =
            do_GetService(kSmtpServiceCID, &rv);

        NS_ENSURE_SUCCESS(rv, rv);
        
        PRBool truthValue = PR_FALSE;
        nsCOMPtr<nsISmtpServer> testServer;
        
        if (aProperty == kNC_IsDefaultServer.get()) {
            printf("Checking for default..");
            smtpService->GetDefaultServer(getter_AddRefs(testServer));
        }

        else if (aProperty == kNC_IsSessionDefaultServer.get()) {
            printf("checking for session default..");
            smtpService->GetSessionDefaultServer(getter_AddRefs(testServer));
        }
        
        if (testServer.get() == smtpServer.get())
            truthValue = PR_TRUE;

        printf("%s\n",  truthValue ? "TRUE" : "FALSE");
        if (truthValue) {
            *aResult = kTrueLiteral;
            NS_ADDREF(*aResult);
        }
    }

    else {
        printf("smtpDatasource: Unknown property\n");
    }

    return NS_OK;
}

/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP nsSmtpDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **aResult)
{
    nsresult rv = NS_OK;

    
    if (aSource == kNC_SmtpServers.get() &&
        aProperty == kNC_Child.get()) {

        nsCOMPtr<nsISupportsArray> arcs;
        GetSmtpServerTargets(getter_AddRefs(arcs));
        
        // enumerate the smtp servers
        rv = NS_NewArrayEnumerator(aResult, arcs);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        rv = NS_NewEmptyEnumerator(aResult);
    }

    return NS_OK;
}

nsresult
nsSmtpDataSource::GetSmtpServerTargets(nsISupportsArray **aResultArray)
{
    nsresult rv;
    nsCOMPtr<nsISmtpService> smtpService =
        do_GetService(kSmtpServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFService> rdf =
        do_GetService(kRDFServiceCID, &rv);
    
    nsCOMPtr<nsISupportsArray> smtpServers;
    rv = smtpService->GetSmtpServers(getter_AddRefs(smtpServers));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsCOMPtr<nsISupportsArray> smtpServerResources;
    rv = NS_NewISupportsArray(getter_AddRefs(smtpServerResources));
    
    PRUint32 count;
    rv = smtpServers->Count(&count);
    NS_ENSURE_SUCCESS(rv, rv);

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

    *aResultArray = smtpServerResources;
    NS_ADDREF(*aResultArray);

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
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;
 
    if (! mObservers)
    {
        nsresult rv;
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv)) return rv;
    }
    mObservers->AppendElement(aObserver);
    return NS_OK;
}

/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP nsSmtpDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (! mObservers)
        return NS_OK;

    mObservers->RemoveElement(aObserver);
    return NS_OK;
}

NS_IMETHODIMP 
nsSmtpDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsSmtpDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  nsresult rv;
  if (aSource == kNC_SmtpServers.get()) {
      *result = mServerRootArcsOut->IndexOf(aArc) != -1;
  }
  else {
      nsCOMPtr<nsISmtpServer> smtpServer;
      rv = aSource->GetDelegate("smtpserver", NS_GET_IID(nsISmtpServer),
                                (void **)getter_AddRefs(smtpServer));
      if (NS_SUCCEEDED(rv)) {
          *result = mServerArcsOut->IndexOf(aArc) != -1;
      }
      else {
          *result = PR_FALSE;
      }
  }
  return NS_OK;
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

    if (aSource == kNC_SmtpServers.get()) {
        rv = NS_NewArrayEnumerator(aResult, mServerRootArcsOut);
    } else {
    
        nsCOMPtr<nsISmtpServer> smtpServer;
        rv = aSource->GetDelegate("smtpserver", NS_GET_IID(nsISmtpServer),
                                  (void **)getter_AddRefs(smtpServer));
        if (NS_SUCCEEDED(rv)) {
            rv = NS_NewArrayEnumerator(aResult, mServerArcsOut);
        } 
    }

    if (!*aResult)
        rv = NS_NewEmptyEnumerator(aResult);
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

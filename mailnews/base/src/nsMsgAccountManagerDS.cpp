/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*
 * RDF datasource for the account manager
 */

#include "nsMsgAccountManagerDS.h"
#include "nsMsgRDFDataSource.h"

#include "nsIMsgAccountManager.h"

#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFDataSource.h"
#include "nsEnumeratorUtils.h"
#include "nsIServiceManager.h"
#include "nsIMsgMailSession.h"

#include "nsCOMPtr.h"

#include "plstr.h"
#include "prmem.h"

#include "nsMsgRDFUtils.h"

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

class nsMsgAccountManagerDataSource : public nsMsgRDFDataSource
{

public:
    
  nsMsgAccountManagerDataSource();
  virtual ~nsMsgAccountManagerDataSource();
  // service manager shutdown method

  // RDF datasource methods
  
  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri);
    

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *source,
                       nsIRDFResource *property,
                       PRBool aTruthValue,
                       nsIRDFNode **_retval);

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *source,
                        nsIRDFResource *property,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);
  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *source, nsISimpleEnumerator **_retval);

protected:

  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_Account;
  static nsIRDFResource* kNC_Server;
  static nsIRDFResource* kNC_Identity;

private:
  // enumeration function to convert each server (element)
  // to an nsIRDFResource and append it to the array (in data)
  static PRBool createServerResources(nsISupports *element, void *data);
  
  nsCOMPtr<nsIMsgAccountManager> mAccountManager;

};

typedef struct _serverCreationParams {
  nsISupportsArray *serverArray;
  nsIRDFService *rdfService;
} serverCreationParams;

// static members
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Child=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Name=nsnull;

// properties corresponding to interfaces
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Account=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Server=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Identity=nsnull;

// RDF to match
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Account);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Server);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Identity);

nsMsgAccountManagerDataSource::nsMsgAccountManagerDataSource():
  mAccountManager(null_nsCOMPtr())
{
#ifdef DEBUG_alecf
  fprintf(stderr, "nsMsgAccountManagerDataSource() being created\n");
#endif
}

nsMsgAccountManagerDataSource::~nsMsgAccountManagerDataSource()
{
  if (getRDFService()) nsServiceManager::ReleaseService(kRDFServiceCID,
                                                    getRDFService(),
                                                    this);

}
/* void Init (in string uri); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::Init(const char *uri)
{
    nsMsgRDFDataSource::Init(uri);
    nsresult rv=NS_OK;
#ifdef DEBUG_alecf
    fprintf(stderr, "nsMsgAccountManagerDataSource::Init(%s)\n", uri ? uri : "(null)");
#endif
    
    if (!mAccountManager) {
        NS_WITH_SERVICE(nsIMsgMailSession, mailSession,
                        kMsgMailSessionCID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        // maybe the account manager should be a service too? not sure.
        rv = mailSession->GetAccountManager(getter_AddRefs(mAccountManager));
        if (NS_FAILED(rv)) return rv;
    }
    
    if (! kNC_Child) {
      getRDFService()->GetResource(NC_RDF_CHILD, &kNC_Child);
      getRDFService()->GetResource(NC_RDF_NAME, &kNC_Name);
      getRDFService()->GetResource(kURINC_Account, &kNC_Account);
      getRDFService()->GetResource(kURINC_Server, &kNC_Server);
      getRDFService()->GetResource(kURINC_Identity, &kNC_Identity);
    }
    return NS_OK;
}

/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetTarget(nsIRDFResource *source,
                                         nsIRDFResource *property,
                                         PRBool aTruthValue,
                                         nsIRDFNode **target)
{
  return NS_RDF_NO_VALUE;
}


/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_RDF_NO_VALUE;

  // create array and enumerator
  // even if we're not handling this we need to return something empty?
  nsCOMPtr<nsISupportsArray> nodes;
  rv = NS_NewISupportsArray(getter_AddRefs(nodes));
  if (NS_FAILED(rv)) return rv;
  
  nsISimpleEnumerator* enumerator =
    new nsArrayEnumerator(nodes);
  if (!enumerator) return NS_ERROR_OUT_OF_MEMORY;

  *_retval = enumerator;
  NS_ADDREF(*_retval);

  // if the property is "account" or "child" then return the
  // list of accounts
  // if the property is "server" return a union of all servers
  // in the account (mainly for the folder pane)

  char *source_value=nsnull;
  rv = source->GetValue(&source_value);

  if (NS_SUCCEEDED(rv) &&
      PL_strcmp(source_value, "msgaccounts:/")==0) {
    
    nsCOMPtr<nsISupportsArray> servers;
    rv = mAccountManager->GetAllServers(getter_AddRefs(servers));

    // fill up the nodes array with the RDF Resources for the servers
    serverCreationParams params = { nodes, getRDFService() };
    servers->EnumerateForwards(createServerResources, (void*)&params);
  }
  
  return NS_OK;
}

// enumeration function to convert each server (element)
// to an nsIRDFResource and append it to the array (in data)
// always return PR_TRUE to try on every element instead of aborting early
PRBool
nsMsgAccountManagerDataSource::createServerResources(nsISupports *element,
                                                     void *data)
{
  nsresult rv;
  // get parameters out of the data argument
  serverCreationParams *params = (serverCreationParams*)data;
  nsCOMPtr<nsISupportsArray> servers = dont_QueryInterface(params->serverArray);
  nsCOMPtr<nsIRDFService> rdf = dont_QueryInterface(params->rdfService);

  // the server itself is in the element argument
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(element, &rv);
  if (NS_FAILED(rv)) return PR_TRUE;

  // get the URI from the incoming server
  char *serverUri;
  rv = server->GetServerURI(&serverUri);
  if (NS_FAILED(rv)) return PR_TRUE;

  // get the corresponding RDF resource
  // RDF will create the server resource if it doesn't already exist
  nsCOMPtr<nsIRDFResource> serverResource;
  rv = rdf->GetResource(serverUri, getter_AddRefs(serverResource));
  if (NS_FAILED(rv)) return PR_TRUE;

  // add the resource to the array
  rv = servers->AppendElement(serverResource);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  return PR_TRUE;
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::ArcLabelsOut(nsIRDFResource *source,
                                     nsISimpleEnumerator **_retval)
{
  nsresult rv;
  
  // we have to return something, so always create the array/enumerators
  nsCOMPtr<nsISupportsArray> arcs;
  rv = NS_NewISupportsArray(getter_AddRefs(arcs));

  if (NS_FAILED(rv)) return rv;
  
  nsArrayEnumerator* enumerator =
    new nsArrayEnumerator(arcs);

  if (!enumerator) return NS_ERROR_OUT_OF_MEMORY;
  
  *_retval = enumerator;
  NS_ADDREF(*_retval);
  
  char *value=nsnull;
  source->GetValue(&value);
  if (PL_strcmp(value, "msgaccounts:/")==0) {
    arcs->AppendElement(kNC_Child);
    arcs->AppendElement(kNC_Name);
    arcs->AppendElement(kNC_Server);
  }
  
  PR_FREEIF(value);
  return NS_OK;
}

nsresult
NS_NewMsgAccountManagerDataSource(const nsIID& iid, void ** result)
{
  nsMsgAccountManagerDataSource *amds = new nsMsgAccountManagerDataSource();
  return amds->QueryInterface(iid, result);
}

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

#include "plstr.h"

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
  
  nsIMsgAccountManager *mAccountManager;

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

nsMsgAccountManagerDataSource::nsMsgAccountManagerDataSource()
{
  fprintf(stderr, "nsMsgAccountManagerDataSource() being created\n");
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

    fprintf(stderr, "nsMsgAccountManagerDataSource::Init(%s)\n", uri ? uri : "(null)");
    
    if (!mAccountManager) {
        NS_WITH_SERVICE(nsIMsgMailSession, mailSession,
                        kMsgMailSessionCID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        // maybe the account manager should be a service too? not sure.
        rv = mailSession->GetAccountManager(&mAccountManager);
        if (NS_FAILED(rv)) return rv;
    }
    
    if (! kNC_Child) {
      getRDFService()->GetResource(kURINC_child, &kNC_Child);
      getRDFService()->GetResource(kURINC_Name, &kNC_Name);
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
  nsresult rv = NS_RDF_NO_VALUE;
  if (!aTruthValue) return NS_RDF_NO_VALUE;
  
  // I don't think we'll ever get this?
  
  return rv;
}


/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{
  printf("accountmanager::GetTargets()\n");
  nsresult rv = NS_RDF_NO_VALUE;

  if (!aTruthValue) return NS_RDF_NO_VALUE;

  // if the property is "account" or "child" then return the
  // list of accounts
  // if the property is "server" return a union of all servers
  // in the account (mainly for the folder pane)

  char *source_value=nsnull;
  rv = source->GetValue(&source_value);

#ifdef DEBUG_alecf
  char *property_value=nsnull;
  rv = property->GetValue(&property_value);
  printf("nsMsgAccountManagerDataSource::GetTargets(%s, %s, %s, ..)\n",
         source_value, property_value, aTruthValue?"PR_TRUE":"PR_FALSE");
#endif
  
  if (NS_FAILED(rv) ||
      PL_strcmp(source_value, "msgaccounts:/")!=0)
    return NS_RDF_NO_VALUE;

  printf("GetTargets(): This is an account manager..\n");

  nsISupportsArray *servers;
  mAccountManager->GetAllServers(&servers);

  nsISupportsArray *nodes;
  NS_NewISupportsArray(&nodes);

  // fill up the nodes array with the RDF Resources for the servers
  serverCreationParams params = { nodes, getRDFService() };
  servers->EnumerateForwards(createServerResources, (void*)&params);
  
  nsISimpleEnumerator* enumerator =
    new nsArrayEnumerator(nodes);

  if (!enumerator) return NS_ERROR_OUT_OF_MEMORY;
    
  NS_ADDREF(enumerator);
  *_retval = enumerator;
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
  nsISupportsArray *servers = params->serverArray;
  nsIRDFService *rdf = params->rdfService;

  // the server itself is in the element argument
  nsIMsgIncomingServer *server;
  rv = element->QueryInterface(nsIMsgIncomingServer::GetIID(),
                               (void **)&server);
  if (NS_FAILED(rv)) return PR_TRUE;

  // get the URI from the incoming server
  char *serverUri;
  rv = server->GetServerURI(&serverUri);
  if (NS_FAILED(rv)) return PR_TRUE;

  printf("createServerResources: Adding %s\n", serverUri);
  // get the corresponding RDF resource
  // RDF will create the server resource if it doesn't already exist
  nsIRDFResource *serverResource;
  rv = rdf->GetResource(serverUri, &serverResource);
  if (NS_FAILED(rv)) return PR_TRUE;

  // add the resource to the array
  rv = servers->AppendElement(serverResource);
  if (NS_FAILED(rv)) return PR_TRUE;
  
  NS_RELEASE(server);
  
  return PR_TRUE;
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::ArcLabelsOut(nsIRDFResource *source,
                                     nsISimpleEnumerator **_retval)
{
    char *value=nsnull;
    source->GetValue(&value);
    printf("accountmanager::ArcLabelsOut(%s)\n", value ? value : "(nsnull)");

    if (PL_strcmp(value, "msgaccounts:/")!=0)
      return NS_RDF_NO_VALUE;

    nsISupportsArray *arcs;
    NS_NewISupportsArray(&arcs);

    arcs->AppendElement(kNC_Child);
    arcs->AppendElement(kNC_Name);
    arcs->AppendElement(kNC_Server);

    nsArrayEnumerator* enumerator =
        new nsArrayEnumerator(arcs);

    if (!enumerator) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(enumerator);
    *_retval = enumerator;
    
    return NS_OK;
}

nsresult
NS_NewMsgAccountManagerDataSource(const nsIID& iid, void ** result)
{
  nsMsgAccountManagerDataSource *amds = new nsMsgAccountManagerDataSource();
  return amds->QueryInterface(iid, result);
}

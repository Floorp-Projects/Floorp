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
#include "nsXPIDLString.h"

#include "nsMsgRDFUtils.h"
#include "nsIMsgFolder.h"
#include "nsMsgBaseCID.h"

// turn this on to see useful output
#undef DEBUG_amds

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

#define NC_RDF_PAGETITLE_MAIN     NC_NAMESPACE_URI "PageTitleMain"
#define NC_RDF_PAGETITLE_SERVER   NC_NAMESPACE_URI "PageTitleServer"
#define NC_RDF_PAGETITLE_COPIES   NC_NAMESPACE_URI "PageTitleCopies"
#define NC_RDF_PAGETITLE_ADVANCED NC_NAMESPACE_URI "PageTitleAdvanced"
#define NC_RDF_PAGETAG NC_NAMESPACE_URI "PageTag"

#define NC_RDF_ACCOUNTROOT "msgaccounts:/"

class nsMsgAccountManagerDataSource : public nsMsgRDFDataSource
{

public:
    
  nsMsgAccountManagerDataSource();
  virtual ~nsMsgAccountManagerDataSource();
  virtual nsresult Init();

  // service manager shutdown method

  // RDF datasource methods
  
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
  static nsIRDFResource* kNC_PageTag;
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_AccountRoot;
  
  static nsIRDFResource* kNC_Account;
  static nsIRDFResource* kNC_Server;
  static nsIRDFResource* kNC_Identity;
  static nsIRDFResource* kNC_Settings;

  static nsIRDFResource* kNC_PageTitleMain;
  static nsIRDFResource* kNC_PageTitleServer;
  static nsIRDFResource* kNC_PageTitleCopies;
  static nsIRDFResource* kNC_PageTitleAdvanced;
  

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
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTag=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Settings=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_AccountRoot=nsnull;

// properties corresponding to interfaces
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Account=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Server=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Identity=nsnull;

// individual pages
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleMain=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleServer=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleCopies=nsnull;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_PageTitleAdvanced=nsnull;

// RDF to match
#define NC_RDF_ACCOUNT NC_NAMESPACE_URI "Account"
#define NC_RDF_SERVER  NC_NAMESPACE_URI "Server"
#define NC_RDF_IDENTITY NC_NAMESPACE_URI "Identity"
#define NC_RDF_SETTINGS NC_NAMESPACE_URI "Settings"

nsMsgAccountManagerDataSource::nsMsgAccountManagerDataSource():
  mAccountManager(null_nsCOMPtr())
{
#ifdef DEBUG_amds
  printf("nsMsgAccountManagerDataSource() being created\n");
#endif

  // XXX This call should be moved to a NS_NewMsgFooDataSource()
  // method that the factory calls, so that failure to construct
  // will return an error code instead of returning a partially
  // initialized object.
  nsresult rv = Init();
  NS_ASSERTION(NS_SUCCEEDED(rv), "uh oh, initialization failed");
  if (NS_FAILED(rv)) return /* rv */;

  return /* NS_OK */;
}

nsMsgAccountManagerDataSource::~nsMsgAccountManagerDataSource()
{
  if (getRDFService()) nsServiceManager::ReleaseService(kRDFServiceCID,
                                                    getRDFService(),
                                                    this);

}

nsresult
nsMsgAccountManagerDataSource::Init()
{
    nsresult rv=NS_OK;
    
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
      getRDFService()->GetResource(NC_RDF_PAGETAG, &kNC_PageTag);
      getRDFService()->GetResource(NC_RDF_ACCOUNT, &kNC_Account);
      getRDFService()->GetResource(NC_RDF_SERVER, &kNC_Server);
      getRDFService()->GetResource(NC_RDF_IDENTITY, &kNC_Identity);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_MAIN, &kNC_PageTitleMain);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_SERVER, &kNC_PageTitleServer);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_COPIES, &kNC_PageTitleCopies);
      getRDFService()->GetResource(NC_RDF_PAGETITLE_ADVANCED, &kNC_PageTitleAdvanced);
      
      getRDFService()->GetResource(NC_RDF_ACCOUNTROOT, &kNC_AccountRoot);
      
      // eventually these need to exist in some kind of array
      // that's easily extensible
      getRDFService()->GetResource(NC_RDF_SETTINGS, &kNC_Settings);
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
  nsresult rv;
  
#ifdef DEBUG_amds
  nsXPIDLCString srcval;
  rv = source->GetValue(getter_Copies(srcval));
  nsXPIDLCString propval;
  rv = property->GetValue(getter_Copies(propval));

  printf("GetTarget(%s on arc %s, ..)\n",
         (const char*)srcval, (const char*)propval);
#endif
  
  rv = NS_RDF_NO_VALUE;

  nsString str="";
  if (property == kNC_Name) {

    // XXX these should be localized
    if (source == kNC_PageTitleMain)
      str = "Main";
    
    else if (source == kNC_PageTitleServer)
      str = "Server";

    else if (source == kNC_PageTitleCopies)
      str = "Copies";

    else if (source == kNC_PageTitleAdvanced)
      str = "Advanced";

    else {
      nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(source, &rv);
      if (NS_SUCCEEDED(rv)) {
		PRUint32 depth;
		rv = folder->GetDepth(&depth);
		if(NS_SUCCEEDED(rv) && depth == 0)
		{
			nsXPIDLString prettyName;
			rv = folder->GetPrettyName(getter_Copies(prettyName));
			if (NS_SUCCEEDED(rv))
			  str = prettyName;
		}
      }
    }
  }

  else if (property == kNC_PageTag) {
    // do NOT localize these strings. these are the urls of the XUL files
    if (source == kNC_PageTitleServer)
      str = "am-server.xul";
    else if (source == kNC_PageTitleCopies)
      str = "am-copies.xul";
    else if (source == kNC_PageTitleAdvanced)
      str = "am-advanced.xul";
    else
      str = "am-main.xul";
  }

  if (str!="")
    rv = createNode(str, target);
  //if we have an empty string and we don't have an error value, then 
  //we don't have a value for RDF.
  else if(NS_SUCCEEDED(rv))
	rv = NS_RDF_NO_VALUE;

  return rv;
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

#ifdef DEBUG_amds
  nsXPIDLCString source_value;
  rv = source->GetValue(getter_Copies(source_value));

  nsXPIDLCString property_arc;
  rv = property->GetValue(getter_Copies(property_arc));
  if (NS_FAILED(rv)) return rv;
  
  printf("GetTargets(%s with arc %s...)\n",
         (const char*)source_value,
         (const char*)property_arc);
#endif
  
  if (source == kNC_AccountRoot) {

    if (property == kNC_Child ||
        property == kNC_Settings) {
      
      nsCOMPtr<nsISupportsArray> servers;
      rv = mAccountManager->GetAllServers(getter_AddRefs(servers));
      
      // fill up the nodes array with the RDF Resources for the servers
      serverCreationParams params = { nodes, getRDFService() };
      servers->EnumerateForwards(createServerResources, (void*)&params);
#ifdef DEBUG_amds
      printf("GetTargets(): added the servers on %s\n",
             (const char*)property_arc);
#endif
    }
#ifdef DEBUG_amds
    else {
      printf("unknown arc %s on msgaccounts:/\n", (const char*)property_arc);
    }
#endif
    
  } else {
    /* if this is a server, then support the settings */
    nsCOMPtr<nsIMsgFolder> folder = do_QueryInterface(source, &rv);

    if (NS_SUCCEEDED(rv)) {
      // all the Settings - main, server, copies, advanced, etc
      if (property == kNC_Settings) {
        
        nsIRDFResource* res;
        rv = getRDFService()->GetResource(NC_RDF_PAGETITLE_MAIN, &res);
        if (NS_SUCCEEDED(rv)) nodes->AppendElement(res);
        rv = getRDFService()->GetResource(NC_RDF_PAGETITLE_SERVER, &res);
        if (NS_SUCCEEDED(rv)) nodes->AppendElement(res);
        rv = getRDFService()->GetResource(NC_RDF_PAGETITLE_COPIES, &res);
        if (NS_SUCCEEDED(rv)) nodes->AppendElement(res);
        rv = getRDFService()->GetResource(NC_RDF_PAGETITLE_ADVANCED, &res);
        if (NS_SUCCEEDED(rv)) nodes->AppendElement(res);
        
        PRUint32 nodecount;
        nodes->Count(&nodecount);
#ifdef DEBUG_amds
        printf("GetTargets(): Added %d settings resources on %s\n",
               nodecount, (const char*)property_arc);
#endif
      }
    }
    
#ifdef DEBUG_amds
    else {
      printf("GetTargets(): Unknown source %s\n", (const char*)source_value);
    }
#endif
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
  nsXPIDLCString serverUri;
  rv = server->GetServerURI(getter_Copies(serverUri));
  if (NS_FAILED(rv)) return PR_TRUE;

  // get the corresponding RDF resource
  // RDF will create the server resource if it doesn't already exist
  nsCOMPtr<nsIRDFResource> serverResource;
  rv = rdf->GetResource(serverUri, getter_AddRefs(serverResource));
  if (NS_FAILED(rv)) return PR_TRUE;

  // make incoming server know about its root server folder so we 
  // can find sub-folders given an incoming server.
  nsCOMPtr <nsIFolder> serverFolder = do_QueryInterface(serverResource);
  if (serverFolder) {
	server->SetRootFolder(serverFolder);

    nsXPIDLString serverName;
    server->GetPrettyName(getter_Copies(serverName));
    serverFolder->SetPrettyName(NS_CONST_CAST(PRUnichar*,
                                              (const PRUnichar*)serverName));
  }

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
  
  if (source == kNC_AccountRoot) {
    arcs->AppendElement(kNC_Server);
  }

  arcs->AppendElement(kNC_Child);
  arcs->AppendElement(kNC_Settings);
  arcs->AppendElement(kNC_Name);
  arcs->AppendElement(kNC_PageTag);

#ifdef DEBUG_amds_
  printf("GetArcLabelsOut(%s): Adding child, settings, and name arclabels\n", value);
#endif
  
  return NS_OK;
}

nsresult
NS_NewMsgAccountManagerDataSource(const nsIID& iid, void ** result)
{
  nsMsgAccountManagerDataSource *amds = new nsMsgAccountManagerDataSource();
  if (!amds) return NS_ERROR_OUT_OF_MEMORY;
  return amds->QueryInterface(iid, result);
}

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

/*
 * RDF datasource for the account manager
 */

#include "nsIMsgAccountManager.h"
#include "nsMsgAccountManagerDS.h"

#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFDataSource.h"

#include "nsIServiceManager.h"
#include "nsIMsgMailSession.h"

#include "plstr.h"

#include "nsMsgRDFUtils.h"

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

class nsMsgAccountManagerDataSource : public nsIRDFDataSource,
                                      public nsIShutdownListener
{

public:
  NS_DECL_ISUPPORTS

  nsMsgAccountManagerDataSource();
  virtual ~nsMsgAccountManagerDataSource();
  // service manager shutdown method

  NS_IMETHOD OnShutdown(const nsCID& aClass, nsISupports* service);

  // RDF datasource methods
  
  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri);

  /* readonly attribute string URI; */
  NS_IMETHOD GetURI(char * *aURI);

  /* nsIRDFResource GetSource (in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSource(nsIRDFResource *property,
                       nsIRDFNode *aTarget,
                       PRBool aTruthValue,
                       nsIRDFResource **_retval);

  /* nsISimpleEnumerator GetSources (in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD GetSources(nsIRDFResource *property,
                        nsIRDFNode *aTarget,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);

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

  /* void Assert (in nsIRDFResource aSource, in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD Assert(nsIRDFResource *source,
                    nsIRDFResource *property,
                    nsIRDFNode *aTarget,
                    PRBool aTruthValue);

  /* void Unassert (in nsIRDFResource aSource, in nsIRDFResource property, in nsIRDFNode aTarget); */
  NS_IMETHOD Unassert(nsIRDFResource *source,
                      nsIRDFResource *property,
                      nsIRDFNode *aTarget);

  /* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
  NS_IMETHOD HasAssertion(nsIRDFResource *source,
                          nsIRDFResource *property,
                          nsIRDFNode *aTarget,
                          PRBool aTruthValue,
                          PRBool *_retval);

  /* void AddObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD AddObserver(nsIRDFObserver *aObserver);

  /* void RemoveObserver (in nsIRDFObserver aObserver); */
  NS_IMETHOD RemoveObserver(nsIRDFObserver *aObserver);

  /* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
  NS_IMETHOD ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *source, nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator GetAllResources (); */
  NS_IMETHOD GetAllResources(nsISimpleEnumerator **_retval);

  /* void Flush (); */
  NS_IMETHOD Flush();

  /* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
  NS_IMETHOD GetAllCommands(nsIRDFResource *source, nsIEnumerator **_retval);

  /* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource command, in nsISupportsArray arguments); */
  NS_IMETHOD IsCommandEnabled(nsISupportsArray *sources, nsIRDFResource *command, nsISupportsArray *arguments, PRBool *_retval);

  /* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource command, in nsISupportsArray arguments); */
  NS_IMETHOD DoCommand(nsISupportsArray *sources, nsIRDFResource *command, nsISupportsArray *arguments);


protected:

  static inline nsresult getAccountManager(nsIRDFResource *resource,
                                           nsIMsgAccountManager* *accountManager)
    { return resource->QueryInterface(nsIMsgAccountManager::GetIID(),
                                      (void **)accountManager); }

  
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_Account;
  static nsIRDFResource* kNC_Server;
  static nsIRDFResource* kNC_Identity;

private:

  char* mURI;

  nsIMsgAccountManager *mAccountManager;
  nsIRDFService *mRDFService;

};

#define NS_GET_ACCOUNT_MANAGER(_resource, _variable)	\
nsIMsgAccountManager *_variable;						\
rv = getAccountManager(_resource, &_variable);			\
if (NS_FAILED(rv)) return rv;					\



// static members
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Child;

// properties corresponding to interfaces
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Account;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Server;
nsIRDFResource* nsMsgAccountManagerDataSource::kNC_Identity;

// RDF to match
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Account);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Server);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Identity);


nsMsgAccountManagerDataSource::nsMsgAccountManagerDataSource():
  mURI(nsnull),
  mRDFService(nsnull)
{
  NS_INIT_REFCNT();
  
}

nsMsgAccountManagerDataSource::~nsMsgAccountManagerDataSource()
{
  if (mRDFService) nsServiceManager::ReleaseService(kRDFServiceCID,
                                                    mRDFService,
                                                    this);

  if (mURI) PL_strfree(mURI);
  
}

NS_IMPL_ADDREF(nsMsgAccountManagerDataSource)
NS_IMPL_RELEASE(nsMsgAccountManagerDataSource)

nsresult
nsMsgAccountManagerDataSource::QueryInterface(const nsIID& iid,
                                              void ** result)
{

  return NS_OK;
}


/* void Init (in string uri); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::Init(const char *uri)
{

  nsresult rv=NS_OK;

  if (!mRDFService) {
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      nsIRDFService::GetIID(),
                                      (nsISupports**) &mRDFService,
                                      this);
    if (NS_FAILED(rv)) return rv;
  }

  if (!mAccountManager) {
    NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    // maybe the account manager should be a service too? not sure.
    rv = mailSession->GetAccountManager(&mAccountManager);
    if (NS_FAILED(rv)) return rv;
  }
  
  if (!mURI || PL_strcmp(uri, mURI) != 0)
    mURI = PL_strdup(uri);
  
  rv = mRDFService->RegisterDataSource(this, PR_FALSE);
  if (!rv) return rv;
  

  if (! kNC_Child) {
    mRDFService->GetResource(kURINC_child, &kNC_Child);
    mRDFService->GetResource(kURINC_Account, &kNC_Account);
    mRDFService->GetResource(kURINC_Server, &kNC_Server);
    mRDFService->GetResource(kURINC_Identity, &kNC_Identity);
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute string URI; */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetURI(char * *aURI)
{
  if (!aURI) return NS_ERROR_NULL_POINTER;
  
  *aURI = PL_strdup(mURI);
  
  return NS_OK;
}

/* nsIRDFResource GetSource (in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetSource(nsIRDFResource *property,
                                  nsIRDFNode *aTarget,
                                  PRBool aTruthValue,
                                  nsIRDFResource **_retval)
{
  
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetSources (in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetSources(nsIRDFResource *property,
                                   nsIRDFNode *aTarget,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
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
  
  NS_GET_ACCOUNT_MANAGER(source, accountManager);


  NS_RELEASE(accountManager);
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

  if (!aTruthValue) return NS_RDF_NO_VALUE;

  nsIMsgAccountManager *accountManager;
  if (NS_FAILED(source->QueryInterface(nsIMsgAccountManager::GetIID(),
                                          (void **)&accountManager)))
    return rv;

  
  // if the property is "account" or "child" then return the
  // list of accounts
  // if the property is "server" return a union of all servers
  // in the account (mainly for the folder pane)


  
  return rv;
  
}


/* void Assert (in nsIRDFResource aSource, in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::Assert(nsIRDFResource *source,
                               nsIRDFResource *property,
                               nsIRDFNode *aTarget,
                               PRBool aTruthValue)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Unassert (in nsIRDFResource aSource, in nsIRDFResource property, in nsIRDFNode aTarget); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::Unassert(nsIRDFResource *source,
                                 nsIRDFResource *property,
                                 nsIRDFNode *aTarget)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean HasAssertion (in nsIRDFResource aSource, in nsIRDFResource property, in nsIRDFNode aTarget, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::HasAssertion(nsIRDFResource *source,
                                     nsIRDFResource *property,
                                     nsIRDFNode *aTarget,
                                     PRBool aTruthValue,
                                     PRBool *_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void AddObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::AddObserver(nsIRDFObserver *aObserver)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void RemoveObserver (in nsIRDFObserver aObserver); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::ArcLabelsIn(nsIRDFNode *aNode,
                                    nsISimpleEnumerator **_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::ArcLabelsOut(nsIRDFResource *source,
                                     nsISimpleEnumerator **_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator GetAllResources (); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void Flush (); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::Flush()
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIEnumerator GetAllCommands (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::GetAllCommands(nsIRDFResource *source,
                                       nsIEnumerator **_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean IsCommandEnabled (in nsISupportsArray aSources, in nsIRDFResource command, in nsISupportsArray arguments); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::IsCommandEnabled(nsISupportsArray *sources,
                                         nsIRDFResource *command,
                                         nsISupportsArray *arguments,
                                         PRBool *_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void DoCommand (in nsISupportsArray aSources, in nsIRDFResource command, in nsISupportsArray arguments); */
NS_IMETHODIMP
nsMsgAccountManagerDataSource::DoCommand(nsISupportsArray *sources,
                                  nsIRDFResource *command,
                                  nsISupportsArray *arguments)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsMsgAccountManagerDataSource::OnShutdown(const nsCID& aClass,
                                          nsISupports* service)
{
  return NS_OK;
}


nsresult
NS_NewMsgAccountManagerDataSource(const nsIID& iid, void ** result)
{
  nsMsgAccountManagerDataSource *amds = new nsMsgAccountManagerDataSource();
  return amds->QueryInterface(iid, result);
}

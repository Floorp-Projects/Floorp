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

#include "nsMsgAccountDataSource.h"
#include "nsMsgRDFDataSource.h"
#include "nsIMsgAccount.h"

#include "nsIServiceManager.h"
#include "nsIMsgMailSession.h"

#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFResource.h"

#include "nsXPIDLString.h"
#include "plstr.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);


class nsMsgAccountDataSource : public nsMsgRDFDataSource
{

public:
  nsMsgAccountDataSource();
  virtual ~nsMsgAccountDataSource();
  virtual nsresult Init();
  
  NS_DECL_ISUPPORTS_INHERITED
  
  // RDF datasource methods
  NS_IMETHOD GetURI(char* *aURI);

  /* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *source,
                       nsIRDFResource *aProperty,
                       PRBool aTruthValue,
                       nsIRDFNode **_retval);

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *source,
                        nsIRDFResource *aProperty,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *source, nsISimpleEnumerator **_retval);

protected:

  static inline nsresult getAccount(nsIRDFResource *resource,
                                    nsIMsgAccount* *account)
    {  return resource->QueryInterface(nsCOMTypeInfo<nsIMsgAccount>::GetIID(),
                                       (void **)account); }
  
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_Account;
  static nsIRDFResource* kNC_Server;
  static nsIRDFResource* kNC_Identity;
  
private:
  PRBool mInitialized;

};

// call this at the beginning of most RDF 
#define NS_GET_ACCOUNT(_resource, _variable)	\
nsIMsgAccount *_variable;						\
rv = getAccount(_resource, &_variable);			\
if (NS_FAILED(rv)) return rv;					\




// static members
nsIRDFResource* nsMsgAccountDataSource::kNC_Child;

// properties corresponding to interfaces
nsIRDFResource* nsMsgAccountDataSource::kNC_Account;
nsIRDFResource* nsMsgAccountDataSource::kNC_Server;
nsIRDFResource* nsMsgAccountDataSource::kNC_Identity;

// properties corresponding to interface members
// account properties

// server properties

// identity properties

// RDF to match
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Account);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Server);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Identity);



nsMsgAccountDataSource::nsMsgAccountDataSource():
  mInitialized(PR_FALSE)
{
    nsresult rv;
    rv = Init();

    // XXX This call should be moved to a NS_NewMsgFooDataSource()
    // method that the factory calls, so that failure to construct
    // will return an error code instead of returning a partially
    // initialized object.
    NS_ASSERTION(NS_SUCCEEDED(rv), "uh oh, initialization failed");
    if (NS_FAILED(rv)) return /* rv */;

    return /* NS_OK */;
}

nsMsgAccountDataSource::~nsMsgAccountDataSource()
{
}


nsresult
nsMsgAccountDataSource::Init()
{
    nsMsgRDFDataSource::Init();
    
    if (! kNC_Child) {
            getRDFService()->GetResource(kURINC_child, &kNC_Child);
            getRDFService()->GetResource(kURINC_Account, &kNC_Account);
            getRDFService()->GetResource(kURINC_Server, &kNC_Server);
            getRDFService()->GetResource(kURINC_Identity, &kNC_Identity);
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMPL_ADDREF_INHERITED(nsMsgAccountDataSource, nsMsgRDFDataSource)
NS_IMPL_RELEASE_INHERITED(nsMsgAccountDataSource, nsMsgRDFDataSource)

nsresult
nsMsgAccountDataSource::QueryInterface(const nsIID& iid, void **result)
{
  return NS_OK;
}


NS_IMETHODIMP
nsMsgAccountDataSource::GetURI(char* *aURI)
{
    *aURI = nsXPIDLCString::Copy("rdf:msgaccounts");
    if (! *aURI)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}


/* nsIRDFNode GetTarget (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountDataSource::GetTarget(nsIRDFResource *source,
                                  nsIRDFResource *property,
                                  PRBool aTruthValue,
                                  nsIRDFNode **target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  if (!aTruthValue) return NS_RDF_NO_VALUE;

  NS_GET_ACCOUNT(source, account);



  
  NS_RELEASE(account);
  
  return rv;
}

/* nsISimpleEnumerator GetTargets (in nsIRDFResource aSource, in nsIRDFResource aProperty, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgAccountDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_RDF_NO_VALUE;

  if (!aTruthValue) return NS_RDF_NO_VALUE;

  NS_GET_ACCOUNT(source, account);



  
  NS_RELEASE(account);
  
  return rv;
}


/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource aSource); */
NS_IMETHODIMP
nsMsgAccountDataSource::ArcLabelsOut(nsIRDFResource *source,
                                     nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_RDF_NO_VALUE;

  NS_GET_ACCOUNT(source, account);


  
  NS_RELEASE(account);
  return rv;

}


nsresult
NS_NewMsgAccountDataSource(const nsIID& iid, void **result)
{
  nsMsgAccountDataSource *ads = new nsMsgAccountDataSource();
  if (!ads) return NS_ERROR_OUT_OF_MEMORY;
  return ads->QueryInterface(iid, result);
}


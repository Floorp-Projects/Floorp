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
 * RDF datasource for an identity
 */

#include "nsMsgIdentityDataSource.h"
#include "nsMsgRDFDataSource.h"
#include "nsIMsgIdentity.h"
#include "nsIServiceManager.h"
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsIRDFDataSource.h"
#include "nsIServiceManager.h"
#include "nsIMsgMailSession.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

class nsMsgIdentityDataSource : public nsMsgRDFDataSource
{

public:
  nsMsgIdentityDataSource();
  virtual ~nsMsgIdentityDataSource();
  virtual nsresult Init();

  // RDF datasource methods
  
  NS_IMETHOD GetURI(char* *aURI);
    
  /* nsIRDFNode GetTarget (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
  NS_IMETHOD GetTarget(nsIRDFResource *source,
                       nsIRDFResource *property,
                       PRBool aTruthValue,
                       nsIRDFNode **_retval);

  /* nsISimpleEnumerator GetTargets (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *source,
                        nsIRDFResource *property,
                        PRBool aTruthValue,
                        nsISimpleEnumerator **_retval);

  /* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource source); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *source, nsISimpleEnumerator **_retval);


protected:

  static inline nsresult getIdentity(nsIRDFResource *resource,
                                     nsIMsgIdentity **identity)
        { return resource->QueryInterface(nsCOMTypeInfo<nsIMsgIdentity>::GetIID(),
                                          (void **)identity); }

  
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_Identity;

private:

};

#define NS_GET_IDENTITY(_resource, _variable)	\
nsIMsgIdentity *_variable;						\
rv = getIdentity(_resource, &_variable);		\
if (NS_FAILED(rv)) return rv;					\

// static members
nsIRDFResource* nsMsgIdentityDataSource::kNC_Child;

// properties corresponding to interfaces
nsIRDFResource* nsMsgIdentityDataSource::kNC_Identity;

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Identity);

nsMsgIdentityDataSource::nsMsgIdentityDataSource()
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


nsMsgIdentityDataSource::~nsMsgIdentityDataSource()
{
    // XXX Release all the resources we acquired in Init()
}

nsresult
nsMsgIdentityDataSource::Init()
{
    nsMsgRDFDataSource::Init();
    
    if (! kNC_Child) {
        getRDFService()->GetResource(kURINC_child, &kNC_Child);
        getRDFService()->GetResource(kURINC_Identity, &kNC_Identity);
    }
    return NS_OK;
}


NS_IMETHODIMP
nsMsgIdentityDataSource::GetURI(char* *aURI)
{
    *aURI = nsXPIDLCString::Copy("rdf:msgidentities");
    if (! *aURI)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

/* nsIRDFNode GetTarget (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgIdentityDataSource::GetTarget(nsIRDFResource *source,
                                  nsIRDFResource *property,
                                  PRBool aTruthValue,
                                  nsIRDFNode **target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  nsCOMPtr<nsIMsgIdentity> identity;
  rv = getIdentity(source, getter_AddRefs(identity));
  if NS_FAILED(rv) return rv;

  if (!aTruthValue) return NS_RDF_NO_VALUE;
  
  // identity code here

  return rv;
}


/* nsISimpleEnumerator GetTargets (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgIdentityDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_RDF_NO_VALUE;

  if (!aTruthValue) return NS_RDF_NO_VALUE;

  NS_GET_IDENTITY(source, identity);

  return rv;
  
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource source); */
NS_IMETHODIMP
nsMsgIdentityDataSource::ArcLabelsOut(nsIRDFResource *source,
                                     nsISimpleEnumerator **_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewMsgIdentityDataSource(const nsIID& iid, void **result)
{
  nsMsgIdentityDataSource *ids = new nsMsgIdentityDataSource();
  if (!ids) return NS_ERROR_OUT_OF_MEMORY;
  return ids->QueryInterface(iid, result);
}


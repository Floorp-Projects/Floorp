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

#include "plstr.h"

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

class nsMsgIdentityDataSource : public nsMsgRDFDataSource
{

public:

  // RDF datasource methods
  
  /* void Init (in string uri); */
  NS_IMETHOD Init(const char *uri);
    
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
        { return resource->QueryInterface(nsIMsgIdentity::GetIID(),
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
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Server);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Identity);

/* void Init (in string uri); */
NS_IMETHODIMP
nsMsgIdentityDataSource::Init(const char *uri)
{
    nsMsgRDFDataSource::Init(uri);
    
    if (! kNC_Child) {
        getRDFService()->GetResource(kURINC_child, &kNC_Child);
        getRDFService()->GetResource(kURINC_Identity, &kNC_Identity);
    }
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

  nsIMsgIdentity *identity;
  rv = getIdentity(source, &identity);
  if NS_FAILED(rv) return rv;

  if (!aTruthValue) return NS_RDF_NO_VALUE;
  
  // identity code here

  
  NS_RELEASE(identity);
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
  return ids->QueryInterface(iid, result);
}


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

#include "nsMsgServerDataSource.h"
#include "nsMsgRDFDataSource.h"

#include "nsIMsgIncomingServer.h"

#include "rdf.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

class nsMsgServerDataSource : public nsMsgRDFDataSource
{
public:
  nsMsgServerDataSource();
  virtual ~nsMsgServerDataSource();
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

  static inline nsresult getServer(nsIRDFResource *resource,
                                   nsIMsgIncomingServer **server)
        { return resource->QueryInterface(nsCOMTypeInfo<nsIMsgIncomingServer>::GetIID(),
                                          (void **)server); }

  
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_Server;

private:

};

#define NS_GET_SERVER(_resource, _variable)	\
nsCOMPtr<nsIMsgIncomingServer> _variable;						\
rv = getServer(_resource, getter_AddRefs(_variable));		\
if (NS_FAILED(rv)) return rv;					\

// static members
nsIRDFResource* nsMsgServerDataSource::kNC_Child;

// properties corresponding to interfaces
nsIRDFResource* nsMsgServerDataSource::kNC_Server;

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Server);

nsMsgServerDataSource::nsMsgServerDataSource()
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


nsMsgServerDataSource::~nsMsgServerDataSource()
{
}

nsresult
nsMsgServerDataSource::Init()
{
    nsMsgRDFDataSource::Init();
    
    if (! kNC_Child) {
        getRDFService()->GetResource(kURINC_child, &kNC_Child);
        getRDFService()->GetResource(kURINC_Server, &kNC_Server);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsMsgServerDataSource::GetURI(char* *aURI)
{
    *aURI = nsXPIDLCString::Copy("rdf:msgservers");
    if (! *aURI)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

/* nsIRDFNode GetTarget (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgServerDataSource::GetTarget(nsIRDFResource *source,
                                  nsIRDFResource *property,
                                  PRBool aTruthValue,
                                  nsIRDFNode **target)
{
  nsresult rv = NS_RDF_NO_VALUE;

  NS_GET_SERVER(source, server);

  if (!aTruthValue) return NS_RDF_NO_VALUE;
  
  // server code here

  return rv;
}


/* nsISimpleEnumerator GetTargets (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgServerDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{
  nsresult rv = NS_RDF_NO_VALUE;

  if (!aTruthValue) return NS_RDF_NO_VALUE;

  NS_GET_SERVER(source, server);

  return rv;
  
}

/* nsISimpleEnumerator ArcLabelsOut (in nsIRDFResource source); */
NS_IMETHODIMP
nsMsgServerDataSource::ArcLabelsOut(nsIRDFResource *source,
                                     nsISimpleEnumerator **_retval)
{
    nsresult rv;
    NS_GET_SERVER(source, server);
  
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewMsgServerDataSource(const nsIID& iid, void **result)
{
  nsMsgServerDataSource *ids = new nsMsgServerDataSource();
  if (!ids) return NS_ERROR_OUT_OF_MEMORY;
  return ids->QueryInterface(iid, result);
}

    

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


class nsMsgServerDataSource : public nsMsgRDFDataSource
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

  /* nsIRDFAssertionCursor GetTargets (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
  NS_IMETHOD GetTargets(nsIRDFResource *source,
                        nsIRDFResource *property,
                        PRBool aTruthValue,
                        nsIRDFAssertionCursor **_retval);

  /* nsIRDFArcsOutCursor ArcLabelsOut (in nsIRDFResource source); */
  NS_IMETHOD ArcLabelsOut(nsIRDFResource *source, nsIRDFArcsOutCursor **_retval);


protected:

  static inline nsresult getServer(nsIRDFResource *resource,
                                   nsIMsgIncomingServer **server)
        { return resource->QueryInterface(nsIMsgIncomingServer::GetIID(),
                                          (void **)server); }

  
  static nsIRDFResource* kNC_Child;
  static nsIRDFResource* kNC_Server;

private:

};

#define NS_GET_SERVER(_resource, _variable)	\
nsIMsgIncomingServer *_variable;						\
rv = getServer(_resource, &_variable);		\
if (NS_FAILED(rv)) return rv;					\

// static members
nsIRDFResource* nsMsgServerDataSource::kNC_Child;

// properties corresponding to interfaces
nsIRDFResource* nsMsgServerDataSource::kNC_Server;

DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, child);
DEFINE_RDF_VOCAB(NC_NAMESPACE_URI, NC, Server);

/* void Init (in string uri); */
NS_IMETHODIMP
nsMsgServerDataSource::Init(const char *uri)
{
    nsMsgRDFDataSource::Init(uri);
    
    if (! kNC_Child) {
        getRDFService()->GetResource(kURINC_child, &kNC_Child);
        getRDFService()->GetResource(kURINC_Server, &kNC_Server);
    }
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

  nsIMsgIncomingServer *server;
  rv = getServer(source, &server);
  if NS_FAILED(rv) return rv;

  if (!aTruthValue) return NS_RDF_NO_VALUE;
  
  // server code here

  
  NS_RELEASE(server);
  return rv;
}


/* nsIRDFAssertionCursor GetTargets (in nsIRDFResource source, in nsIRDFResource property, in boolean aTruthValue); */
NS_IMETHODIMP
nsMsgServerDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsIRDFAssertionCursor **_retval)
{
  nsresult rv = NS_RDF_NO_VALUE;

  if (!aTruthValue) return NS_RDF_NO_VALUE;

  NS_GET_SERVER(source, server);

  return rv;
  
}

/* nsIRDFArcsOutCursor ArcLabelsOut (in nsIRDFResource source); */
NS_IMETHODIMP
nsMsgServerDataSource::ArcLabelsOut(nsIRDFResource *source,
                                     nsIRDFArcsOutCursor **_retval)
{

  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewMsgServerDataSource(const nsIID& iid, void **result)
{
  nsMsgServerDataSource *ids = new nsMsgServerDataSource();
  return ids->QueryInterface(iid, result);
}

    

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsMsgFilterDataSource.h"
#include "nsMsgRDFUtils.h"
#include "nsEnumeratorUtils.h"
#include "nsIMsgFilter.h"

NS_IMPL_ISUPPORTS1(nsMsgFilterDataSource, nsIRDFDataSource)

nsrefcnt nsMsgFilterDataSource::mGlobalRefCount = 0;
nsCOMPtr<nsIRDFResource> nsMsgFilterDataSource::kNC_Child;
nsCOMPtr<nsIRDFResource> nsMsgFilterDataSource::kNC_Name;

nsCOMPtr<nsISupportsArray> nsMsgFilterDataSource::mFolderArcsOut;
nsCOMPtr<nsISupportsArray> nsMsgFilterDataSource::mFilterArcsOut;
  
nsMsgFilterDataSource::nsMsgFilterDataSource()
{
  NS_INIT_ISUPPORTS();
  if (mGlobalRefCount == 0)
    initGlobalObjects(getRDFService());
  
  mGlobalRefCount++;
  /* member initializers and constructor code */
}

nsMsgFilterDataSource::~nsMsgFilterDataSource()
{
  mGlobalRefCount--;
  if (mGlobalRefCount == 0)
    cleanupGlobalObjects();
}

nsresult
nsMsgFilterDataSource::cleanupGlobalObjects()
{
  mFolderArcsOut = nsnull;
  mFilterArcsOut = nsnull;
  kNC_Child = nsnull;
  kNC_Name = nsnull;
  return NS_OK;
}

nsresult
nsMsgFilterDataSource::initGlobalObjects(nsIRDFService *rdf)
{
  rdf->GetResource(NC_RDF_CHILD, getter_AddRefs(kNC_Child));
  rdf->GetResource(NC_RDF_NAME, getter_AddRefs(kNC_Name));

  NS_NewISupportsArray(getter_AddRefs(mFolderArcsOut));
  mFolderArcsOut->AppendElement(kNC_Child);
  
  NS_NewISupportsArray(getter_AddRefs(mFilterArcsOut));
  mFilterArcsOut->AppendElement(kNC_Name);
  
  return NS_OK;
}

NS_IMETHODIMP
nsMsgFilterDataSource::GetTargets(nsIRDFResource *source,
                                   nsIRDFResource *property,
                                   PRBool aTruthValue,
                                   nsISimpleEnumerator **_retval)
{
  
  return NS_OK;
}


NS_IMETHODIMP
nsMsgFilterDataSource::ArcLabelsOut(nsIRDFResource *source,
                                    nsISimpleEnumerator **_retval)
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> arcs;
  
  nsCOMPtr<nsIMsgFolder> folder =
    do_QueryInterface(source, &rv);
  if (NS_SUCCEEDED(rv)) {
    arcs = mFolderArcsOut;
    
  } else {

    nsCOMPtr<nsIMsgFilter> filter;
    rv = source->GetDelegate("mail-filter", NS_GET_IID(nsIMsgFilter),
                      (void **)getter_AddRefs(filter));
    
    if (NS_SUCCEEDED(rv))
      arcs = mFilterArcsOut;
  }

  if (!arcs) {
    *_retval = nsnull;
    return NS_RDF_NO_VALUE;
  }

  nsArrayEnumerator* enumerator =
    new nsArrayEnumerator(arcs);
  NS_ENSURE_TRUE(enumerator, NS_ERROR_OUT_OF_MEMORY);
  
  *_retval = enumerator;
  NS_ADDREF(*_retval);

  return NS_OK;
}

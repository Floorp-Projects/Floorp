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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 */

#include "nsMsgServiceProvider.h"
#include "nsIServiceManager.h"

#include "nsXPIDLString.h"

#include "nsRDFCID.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"

#include "nsIChromeRegistry.h" // for chrome registry

#include "nsIFileSpec.h"
#include "nsAppDirectoryServiceDefs.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);

nsMsgServiceProviderService::nsMsgServiceProviderService()
{
  NS_INIT_REFCNT();
}

nsMsgServiceProviderService::~nsMsgServiceProviderService()
{


}

NS_IMPL_ISUPPORTS1(nsMsgServiceProviderService, nsIRDFDataSource)

nsresult
nsMsgServiceProviderService::Init()
{
  nsresult rv;
  nsCOMPtr<nsIRDFService> rdf = do_GetService(kRDFServiceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mInnerDataSource = do_CreateInstance(kRDFCompositeDataSourceCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> dataFilesDir;
  rv = NS_GetSpecialDirectory(NS_APP_DEFAULTS_50_DIR, getter_AddRefs(dataFilesDir));
  NS_ENSURE_SUCCESS(rv,rv);

  rv = dataFilesDir->Append("isp");
  NS_ENSURE_SUCCESS(rv,rv);

  // test if there is a locale provider
  PRBool isexists = PR_FALSE;
  rv = dataFilesDir->Exists(&isexists);
  NS_ENSURE_SUCCESS(rv,rv);
  if (isexists) {    
    nsCOMPtr<nsIChromeRegistry> chromeRegistry = do_GetService(kChromeRegistryCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsXPIDLString lc_name;
      nsAutoString tmpstr; tmpstr.AssignWithConversion("navigator");
      rv = chromeRegistry->GetSelectedLocale(tmpstr.GetUnicode(), getter_Copies(lc_name));
      if (NS_SUCCEEDED(rv) && (const PRUnichar *)lc_name && nsCRT::strlen((const PRUnichar *)lc_name)) {

          nsCOMPtr<nsIFile> tmpdataFilesDir;
          rv = dataFilesDir->Clone(getter_AddRefs(tmpdataFilesDir));
          NS_ENSURE_SUCCESS(rv,rv);
          rv = tmpdataFilesDir->AppendUnicode(lc_name);
          NS_ENSURE_SUCCESS(rv,rv);
          rv = tmpdataFilesDir->Exists(&isexists);
          NS_ENSURE_SUCCESS(rv,rv);
          if (isexists) {
            // use locale provider instead
            rv = dataFilesDir->AppendUnicode(lc_name);            
            NS_ENSURE_SUCCESS(rv,rv);
          }
       }
    }
      
    // now enumerate every file in the directory, and suck it into the datasource
    PRBool hasMore = PR_FALSE;
    nsCOMPtr<nsISimpleEnumerator> dirIterator;
    rv = dataFilesDir->GetDirectoryEntries(getter_AddRefs(dirIterator));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIFile> dirEntry;
    nsCOMPtr<nsIFileURL> dirEntryURL(do_CreateInstance("@mozilla.org/network/standard-url;1"));

    while ((rv = dirIterator->HasMoreElements(&hasMore)) == NS_OK && hasMore) {
      rv = dirIterator->GetNext((nsISupports**)getter_AddRefs(dirEntry));
      if (NS_FAILED(rv))
        continue;
         
      rv = dirEntryURL->SetFile(dirEntry);
      if (NS_FAILED(rv))
         continue;

      nsXPIDLCString urlSpec;
      rv = dirEntryURL->GetSpec(getter_Copies(urlSpec));
      rv = LoadDataSource(urlSpec);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Failed reading in the datasource\n");
    }
  }
  
  return NS_OK;
}


nsresult
nsMsgServiceProviderService::LoadDataSource(const char *aURI)
{
    nsresult rv;

    nsCOMPtr<nsIRDFDataSource> ds =
        do_CreateInstance(kRDFXMLDataSourceCID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    nsCOMPtr<nsIRDFRemoteDataSource> remote =
        do_QueryInterface(ds, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = remote->Init(aURI);
    NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG_alecf
    PRBool loaded;
    rv = remote->GetLoaded(&loaded);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed getload\n");

    printf("Before refresh: datasource is %s\n", loaded ? "loaded" : "not loaded");
#endif

    // for now load synchronously (async seems to be busted)
    rv = remote->Refresh(PR_TRUE);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed refresh?\n");

#ifdef DEBUG_alecf
    rv = remote->GetLoaded(&loaded);
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed getload\n");
    printf("After refresh: datasource is %s\n", loaded ? "loaded" : "not loaded");
#endif

    rv = mInnerDataSource->AddDataSource(ds);

    return rv;
}

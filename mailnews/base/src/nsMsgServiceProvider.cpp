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

#include "nsSpecialSystemDirectory.h"
#include "nsNetUtil.h"
#include "nsIChromeRegistry.h"

#include "nsIFileSpec.h"
#include "nsFileLocations.h"
#include "nsIFileLocator.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFCompositeDataSourceCID, NS_RDFCOMPOSITEDATASOURCE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kChromeRegistryCID, NS_CHROMEREGISTRY_CID);
static NS_DEFINE_CID(kFileSpecCID, NS_FILESPEC_CID);

//----------------------------------------------------------------------------------------
static nsresult GetConvertedChromeURL(const char* uriStr, nsIFileSpec* *outSpec)
//----------------------------------------------------------------------------------------
{

  nsresult rv;
    
  nsCOMPtr<nsIURI> uri;
  uri = do_CreateInstance(kStandardUrlCID);
  uri->SetSpec(uriStr);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIChromeRegistry> chromeRegistry =
      do_GetService(kChromeRegistryCID, &rv);

  nsXPIDLCString newSpec;  
  if (NS_SUCCEEDED(rv)) {

      rv = chromeRegistry->ConvertChromeURL(uri, getter_Copies(newSpec));
      if (NS_FAILED(rv))
          return rv;
  }
  const char *urlSpec = newSpec;
  uri->SetSpec(urlSpec);

  /* won't deal remote URI yet */
  nsString fileStr; fileStr.AssignWithConversion("file");
  nsString resStr; resStr.AssignWithConversion("res");
  nsString resoStr; resoStr.AssignWithConversion("resource");
  
  char *uriScheme = nsnull;
  uri->GetScheme(&uriScheme);
  nsString tmpStr; tmpStr.AssignWithConversion(uriScheme);
   
  NS_ASSERTION(((tmpStr == fileStr) || (tmpStr == resStr) || (tmpStr == resoStr)), "won't deal remote URI yet! \n");
   
  if ((tmpStr != fileStr)) {
       /* resolve to fileURL */
       nsSpecialSystemDirectory dir(nsSpecialSystemDirectory::Moz_BinDirectory);
       nsFileURL fileURL(dir); // file:///moz_0511/mozilla/...
       
       char *uriPath = nsnull;
       uri->GetPath(&uriPath);
       fileURL += uriPath;
       urlSpec = fileURL.GetURLString();
  }

  nsCOMPtr<nsIFileSpec> dataFilesDir = do_GetService(kFileSpecCID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  *outSpec = dataFilesDir;
  NS_ADDREF(*outSpec);

  return dataFilesDir->SetURLString(urlSpec);
}

//========================================================================================
// Implementation of nsMsgServiceProviderService
//========================================================================================

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

  nsCOMPtr<nsIFileLocator> locator = do_GetService(kFileLocatorCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFileSpec> dataFilesDir;
  rv = GetConvertedChromeURL("chrome://messenger/locale/isp/", getter_AddRefs(dataFilesDir));

  // now enumerate every file in the directory, and suck it into the datasource

  nsCOMPtr<nsIDirectoryIterator> fileIterator =
    do_CreateInstance(NS_DIRECTORYITERATOR_PROGID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = fileIterator->Init(dataFilesDir, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists=PR_TRUE;
  fileIterator->Exists(&exists);
  
  while (exists) {
    nsCOMPtr<nsIFileSpec> ispFile;
    
    rv = fileIterator->GetCurrentSpec(getter_AddRefs(ispFile));
    if (NS_FAILED(rv)) continue;

    nsXPIDLCString url;
    ispFile->GetURLString(getter_Copies(url));
    
#if defined(DEBUG_alecf) || defined(DEBUG_tao)
    printf("nsMsgServiceProvider: reading %s\n", (const char*)url);
#endif

    rv = LoadDataSource(url);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Failed reading in the datasource\n");
    // fall through on failure though, just keep going
    
    fileIterator->Next();
    fileIterator->Exists(&exists);
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

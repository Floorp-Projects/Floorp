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

#include "nsCOMPtr.h"
#include "nsIChromeRegistry.h"
#include "nsIRDFObserver.h"
#include "nsCRT.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFCursor.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsXPIDLString.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFServiceIID, NS_IRDFSERVICE_IID);
static NS_DEFINE_IID(kIRDFObserverIID, NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kIRDFIntIID, NS_IRDFINT_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

#define CHROME_NAMESPACE_URI "http://chrome.mozilla.org/rdf#"
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, chrome);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, skin);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, content);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, base);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, main);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, archive);

////////////////////////////////////////////////////////////////////////////////

class nsChromeRegistry : public nsIChromeRegistry, public nsIRDFObserver {
public:
    NS_DECL_ISUPPORTS

    // nsIChromeRegistry methods:
    NS_IMETHOD Init();
    NS_IMETHOD ConvertChromeURL(nsIURL* aChromeURL);

    // nsIRDFObserver methods:
    NS_IMETHOD OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object);
    NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object);

    // nsChromeRegistry methods:
    nsChromeRegistry();
    virtual ~nsChromeRegistry();

    static PRUint32 gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFDataSource* gRegistryDB; 
    static nsIRDFResource* kCHROME_chrome;
    static nsIRDFResource* kCHROME_skin;
    static nsIRDFResource* kCHROME_content;
    static nsIRDFResource* kCHROME_base;
    static nsIRDFResource* kCHROME_main;
    static nsIRDFResource* kCHROME_archive;

protected:
    nsresult EnsureRegistryDataSource();
};

PRUint32 nsChromeRegistry::gRefCnt = 0;
nsIRDFService* nsChromeRegistry::gRDFService = nsnull;
nsIRDFDataSource* nsChromeRegistry::gRegistryDB = nsnull; 

nsIRDFResource* nsChromeRegistry::kCHROME_chrome = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_skin = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_content = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_base = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_main = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_archive = nsnull;

////////////////////////////////////////////////////////////////////////////////

nsChromeRegistry::nsChromeRegistry()
{
	NS_INIT_REFCNT();
  
}

NS_IMETHODIMP
nsChromeRegistry::Init()
{
  nsresult rv = NS_OK;

  gRefCnt++;
  if (gRefCnt == 1) {
      rv = nsServiceManager::GetService(kRDFServiceCID,
                                        kIRDFServiceIID,
                                        (nsISupports**)&gRDFService);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
      if (NS_FAILED(rv)) return rv;

      // get all the properties we'll need:
      rv = gRDFService->GetResource(kURICHROME_chrome, &kCHROME_chrome);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;

      rv = gRDFService->GetResource(kURICHROME_skin, &kCHROME_skin);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;

      rv = gRDFService->GetResource(kURICHROME_content, &kCHROME_content);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;

      rv = gRDFService->GetResource(kURICHROME_base, &kCHROME_base);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;

      rv = gRDFService->GetResource(kURICHROME_main, &kCHROME_main);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;

      rv = gRDFService->GetResource(kURICHROME_archive, &kCHROME_archive);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;
      
  }

  if (gRegistryDB)
  {
      rv = gRegistryDB->AddObserver(this);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add self as registry observer");
  }
  
  return rv;
}

nsChromeRegistry::~nsChromeRegistry()
{
    // Stop observing the history data source
    if (gRegistryDB)
        gRegistryDB->RemoveObserver(this);

    --gRefCnt;
    if (gRefCnt == 0) {
        // release all the properties:
        NS_IF_RELEASE(kCHROME_chrome);
        NS_IF_RELEASE(kCHROME_skin);
        NS_IF_RELEASE(kCHROME_content);
        NS_IF_RELEASE(kCHROME_base);
        NS_IF_RELEASE(kCHROME_main);
        NS_IF_RELEASE(kCHROME_archive);
       
        if (gRegistryDB) {
            NS_RELEASE(gRegistryDB);
            gRegistryDB = nsnull;
        }

        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }
    }
}

NS_IMPL_ADDREF(nsChromeRegistry)
NS_IMPL_RELEASE(nsChromeRegistry)

NS_IMETHODIMP
nsChromeRegistry::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsIChromeRegistry::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aResult = NS_STATIC_CAST(nsIChromeRegistry*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    if (aIID.Equals(kIRDFObserverIID)) {
        *aResult = NS_STATIC_CAST(nsIChromeRegistry*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChromeRegistry methods:

NS_IMETHODIMP
nsChromeRegistry::ConvertChromeURL(nsIURL* aChromeURL)
{
    nsresult rv = NS_OK;
    if (NS_FAILED(rv = EnsureRegistryDataSource())) {
        NS_ERROR("Unable to ensure the existence of a chrome registry.");
        return rv;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRDFObserver methods:

NS_IMETHODIMP
nsChromeRegistry::OnAssert(nsIRDFResource* subject,
                            nsIRDFResource* predicate,
                            nsIRDFNode* object)
{
    nsresult rv = NS_OK;
    /*if (predicate == kNC_Page) {
        nsIRDFResource* objRes;
        rv = object->QueryInterface(kIRDFResourceIID, (void**)&objRes);
        if (NS_FAILED(rv)) return rv;
        nsXPIDLCString url;
        rv = objRes->GetValue( getter_Copies(url) );
        if (NS_SUCCEEDED(rv)) {
            rv = CountPageVisit(url);
        }
        NS_RELEASE(objRes);
    }*/
    return rv;
}

NS_IMETHODIMP
nsChromeRegistry::OnUnassert(nsIRDFResource* subject,
                              nsIRDFResource* predicate,
                              nsIRDFNode* object)
{
    // XXX To do
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////

nsresult
nsChromeRegistry::EnsureRegistryDataSource()
{
    return gRDFService->GetDataSource("resource:/chrome/registry.rdf", &gRegistryDB);
}

nsresult
NS_NewChromeRegistry(nsIChromeRegistry** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsChromeRegistry* chromeRegistry = new nsChromeRegistry();
    if (chromeRegistry == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(chromeRegistry);
    *aResult = chromeRegistry;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

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
static NS_DEFINE_IID(kIRDFLiteralIID, NS_IRDFLITERAL_IID);
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

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Description);

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
    nsresult GetSkinOrContentResource(const nsString& aChromeType, nsIRDFResource** aResult);
    nsresult GetChromeResource(nsString& aResult, nsIRDFResource* aChromeResource,
                               nsIRDFResource* aProperty);
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

    // Retrieve the resource for this chrome element.
    const char* host;
    if (NS_FAILED(rv = aChromeURL->GetHost(&host))) {
        NS_ERROR("Unable to retrieve the host for a chrome URL.");
        return rv;
    }

    nsAutoString hostStr(host);
    hostStr.ToLowerCase();

    // Construct a chrome URL and use it to look up a resource.
    nsAutoString windowType = nsAutoString("chrome://") + hostStr + "/";

    // Find out if the chrome URL referenced a skin or content.
    const char* file;
    aChromeURL->GetFile(&file);
    nsAutoString restOfURL(file);
    restOfURL.ToLowerCase();
    if (restOfURL.Find("/skin") == 0)
        windowType += "skin/";
    else windowType += "content/";

    // We have the resource URI that we wish to retrieve. Fetch it.
    nsCOMPtr<nsIRDFResource> chromeResource;
    if (NS_FAILED(rv = GetSkinOrContentResource(windowType, getter_AddRefs(chromeResource)))) {
        NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
        return rv;
    }

    nsString chromeBase;
    if (NS_FAILED(rv = GetChromeResource(chromeBase, chromeResource, kCHROME_base))) {
        NS_ERROR("Unable to retrieve codebase for chrome entry.");
        return rv;
    }

    // Check to see if we should append the "main" entry in the registry.
    // Only do this when the user doesn't have anything following "skin"
    // or "content" in the specified URL.
    if (restOfURL == "/skin" || restOfURL == "/skin/" ||
        restOfURL == "/content" || restOfURL == "/content/")
    {
        // Append the "main" entry.
        nsString mainFile;
        if (NS_FAILED(rv = GetChromeResource(mainFile, chromeResource, kCHROME_main))) {
            NS_ERROR("Unable to retrieve the main file registry entry for a chrome URL.");
            return rv;
        }
        chromeBase += mainFile;
    }
    else
    {
        // XXX Just append the rest of the URL following the skin or content.
    }

    char* finalDecision = chromeBase.ToNewCString();
    aChromeURL->SetSpec(finalDecision);
    delete []finalDecision;
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
    // XXX This must be synchronously loaded.  Ask Chris about
    // how this is done.
    return gRDFService->GetDataSource("resource:/chrome/registry.rdf", &gRegistryDB);
}

nsresult
nsChromeRegistry::GetSkinOrContentResource(const nsString& aChromeType,
                                           nsIRDFResource** aResult)
{
    nsresult rv = NS_OK;
    char* url = aChromeType.ToNewCString();
    if (NS_FAILED(rv = gRDFService->GetResource(url, aResult))) {
        NS_ERROR("Unable to retrieve a resource for this chrome.");
        *aResult = nsnull;
        delete []url;
        return rv;
    }
    delete []url;
    return NS_OK;
}

nsresult 
nsChromeRegistry::GetChromeResource(nsString& aResult, 
                                    nsIRDFResource* aChromeResource,
                                    nsIRDFResource* aProperty)
{
    nsresult rv = NS_OK;

    if (gRegistryDB == nsnull)
        return NS_ERROR_FAILURE; // Must have a DB to attempt this operation.

    nsCOMPtr<nsIRDFNode> chromeBase;
    if (NS_FAILED(rv = gRegistryDB->GetTarget(aChromeResource, aProperty, PR_TRUE, getter_AddRefs(chromeBase)))) {
        NS_ERROR("Unable to obtain a base resource.");
        return rv;
    }

    if (chromeBase == nsnull)
      return NS_OK;

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    if (NS_SUCCEEDED(rv = chromeBase->QueryInterface(kIRDFResourceIID,
                                                     (void**) getter_AddRefs(resource)))) {
        nsXPIDLCString uri;
        resource->GetValue( getter_Copies(uri) );
        aResult = uri;
    }
    else if (NS_SUCCEEDED(rv = chromeBase->QueryInterface(kIRDFLiteralIID,
                                                      (void**) getter_AddRefs(literal)))) {
        nsXPIDLString s;
        literal->GetValue( getter_Copies(s) );
        aResult = s;
    }
    else {
        // This should _never_ happen.
        NS_ERROR("uh, this isn't a resource or a literal!");
        return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsSpecialSystemDirectory.h"
#include "nsIChromeRegistry.h"
#include "nsIChromeEntry.h"
#include "nsChromeRegistry.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFObserver.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsCRT.h"
#include "rdf.h"
#include "nsIServiceManager.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFContainer.h"
#include "nsHashtable.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsFileLocations.h"
#include "nsIFileLocator.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMLocation.h"
#include "nsIWindowMediator.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIXULPrototypeCache.h"
#include "nsIStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLContentContainer.h"
#include "nsIPresShell.h"
#include "nsIStyleSet.h"
#include "nsISupportsArray.h"
#include "nsICSSLoader.h"
#include "nsIDocumentObserver.h"
#include "nsIXULDocument.h"
#include "nsINameSpaceManager.h"

static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);

class nsChromeRegistry;

#define CHROME_NAMESPACE_URI "http://chrome.mozilla.org/rdf#"
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, chrome);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, skin);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, content);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, locale);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, base);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, main);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, name);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, archive);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, text);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, version);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, author);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, siteURL);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, previewImageURL);

// XXX This nasty function should disappear when we land Necko completely and 
// change chrome://global/skin/foo to chrome://skin@global/foo
//
void BreakProviderAndRemainingFromPath(const char* i_path, char** o_provider, char** o_remaining);

////////////////////////////////////////////////////////////////////////////////

// XXX LOCAL COMPONENT PROBLEM! overlayEnumerator must take two sets of
// arcs rather than one, and must be able to move to the second set after
// finishing the first (chromeEntryEnumerator can do this, code needs to be copied).
class nsOverlayEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    nsOverlayEnumerator(nsISimpleEnumerator *aArcs);
    virtual ~nsOverlayEnumerator();

private:
    nsCOMPtr<nsISimpleEnumerator> mArcs;
};

NS_IMPL_ISUPPORTS1(nsOverlayEnumerator, nsISimpleEnumerator)

nsOverlayEnumerator::nsOverlayEnumerator(nsISimpleEnumerator *aArcs)
{
  NS_INIT_REFCNT();
  mArcs = aArcs;
}

nsOverlayEnumerator::~nsOverlayEnumerator()
{
}

NS_IMETHODIMP nsOverlayEnumerator::HasMoreElements(PRBool *aIsTrue)
{
  return mArcs->HasMoreElements(aIsTrue);
}

NS_IMETHODIMP nsOverlayEnumerator::GetNext(nsISupports **aResult)
{
  nsresult rv;
  *aResult = nsnull;

  if (!mArcs)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupports> supports;
  mArcs->GetNext(getter_AddRefs(supports));

  nsCOMPtr<nsIRDFLiteral> value = do_QueryInterface(supports, &rv);
  if (NS_FAILED(rv))
    return NS_OK;

  const PRUnichar* valueStr;
  rv = value->GetValueConst(&valueStr);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url;
  
  rv = nsComponentManager::CreateInstance("component://netscape/network/standard-url",
                                          nsnull,
                                          NS_GET_IID(nsIURL),
                                          getter_AddRefs(url));

  if (NS_FAILED(rv))
    return NS_OK;

  nsCAutoString str(valueStr);
  url->SetSpec(str);
  
  nsCOMPtr<nsISupports> sup;
  sup = do_QueryInterface(url, &rv);
  if (NS_FAILED(rv))
    return NS_OK;

  *aResult = sup;
  NS_ADDREF(*aResult);

  return NS_OK;
}

class nsChromeEntryEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    nsChromeEntryEnumerator(nsIRDFDataSource* aProfileDataSource,
                            nsIRDFDataSource* aInstallDataSource,
                            nsISimpleEnumerator *aProfileArcs,
                            nsISimpleEnumerator *aInstallArcs);
    virtual ~nsChromeEntryEnumerator();

private:
    nsCOMPtr<nsIRDFDataSource> mProfileDataSource;
    nsCOMPtr<nsIRDFDataSource> mInstallDataSource;
    nsCOMPtr<nsIRDFDataSource> mCurrentDataSource;

    nsCOMPtr<nsISimpleEnumerator> mProfileArcs;
    nsCOMPtr<nsISimpleEnumerator> mInstallArcs;
    nsCOMPtr<nsISimpleEnumerator> mCurrentArcs;
};

NS_IMPL_ISUPPORTS1(nsChromeEntryEnumerator, nsISimpleEnumerator)

nsChromeEntryEnumerator::nsChromeEntryEnumerator(nsIRDFDataSource* aProfileDataSource,
                                                 nsIRDFDataSource* aInstallDataSource,
                                                 nsISimpleEnumerator *aProfileArcs,
                                                 nsISimpleEnumerator *aInstallArcs)
{
  NS_INIT_REFCNT();
  mProfileDataSource = aProfileDataSource;
  mInstallDataSource = aInstallDataSource;
  mCurrentDataSource = mProfileDataSource;

  mProfileArcs = aProfileArcs;
  mInstallArcs = aInstallArcs;
  mCurrentArcs = mProfileArcs;
}

nsChromeEntryEnumerator::~nsChromeEntryEnumerator()
{
}

NS_IMETHODIMP nsChromeEntryEnumerator::HasMoreElements(PRBool *aIsTrue)
{
  *aIsTrue = PR_FALSE;

  if (!mProfileArcs) {
    if (!mInstallArcs)
      return NS_OK; // Default value of FALSE for no arcs in either places
    return mInstallArcs->HasMoreElements(aIsTrue); // No profile arcs means check install
  }
  else {
    nsresult rv = mProfileArcs->HasMoreElements(aIsTrue);
    if (*aIsTrue || !mInstallArcs) // Have profile elts or no install arcs means bail.
      return rv;

    return mInstallArcs->HasMoreElements(aIsTrue); // Otherwise just use install arcs.
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeEntryEnumerator::GetNext(nsISupports **aResult)
{
  nsresult rv;
  *aResult = nsnull;

  if (!mCurrentArcs) {
    mCurrentArcs = mInstallArcs;
    mCurrentDataSource = mInstallDataSource;

    if (!mInstallArcs)
      return NS_ERROR_FAILURE;
  }
  else if (mCurrentArcs == mProfileArcs) {
    PRBool hasMore;
    mProfileArcs->HasMoreElements(&hasMore);
    if (!hasMore) {
      mCurrentArcs = mInstallArcs;
      mCurrentDataSource = mInstallDataSource;
    }
    if (!mInstallArcs)
      return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> supports;
  mCurrentArcs->GetNext(getter_AddRefs(supports));

  // The resource that we obtain is the name of the skin/locale/package with
  // "chrome:" prepended to it. It has arcs to simple literals
  // for each of the fields that should be in the chrome entry.
  // We need to construct a chrome entry and then fill in the
  // values by enumerating the arcs out of our resource (to the
  // literals)
  nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(supports, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIChromeEntry> chromeEntry;
  rv = nsComponentManager::CreateInstance("component://netscape/chrome/chrome-entry",
                                          nsnull,
                                          NS_GET_IID(nsIChromeEntry),
                                          getter_AddRefs(chromeEntry));

  // XXX Now that we have a resource, we must examine all of its outgoing
  // arcs and use the literals at the ends of the arcs to set the fields
  // of the chrome entry.
  nsAutoString name, text, author, version, siteURL, previewImageURL, archive;
  nsChromeRegistry::GetChromeResource(mCurrentDataSource, name, resource, 
                                      nsChromeRegistry::kCHROME_name);
  nsChromeRegistry::GetChromeResource(mCurrentDataSource, name, resource, 
                                      nsChromeRegistry::kCHROME_text);
  nsChromeRegistry::GetChromeResource(mCurrentDataSource, name, resource, 
                                      nsChromeRegistry::kCHROME_author);
  nsChromeRegistry::GetChromeResource(mCurrentDataSource, name, resource, 
                                      nsChromeRegistry::kCHROME_version);
  nsChromeRegistry::GetChromeResource(mCurrentDataSource, name, resource, 
                                      nsChromeRegistry::kCHROME_siteURL);
  nsChromeRegistry::GetChromeResource(mCurrentDataSource, name, resource, 
                                      nsChromeRegistry::kCHROME_previewImageURL);
  nsChromeRegistry::GetChromeResource(mCurrentDataSource, name, resource, 
                                      nsChromeRegistry::kCHROME_archive);
  chromeEntry->SetName(name.GetUnicode());
  chromeEntry->SetText(text.GetUnicode());
  chromeEntry->SetAuthor(author.GetUnicode());
  chromeEntry->SetVersion(version.GetUnicode());
  chromeEntry->SetSiteURL(siteURL.GetUnicode());
  chromeEntry->SetPreviewImageURL(previewImageURL.GetUnicode());
  chromeEntry->SetArchive(archive.GetUnicode());

  nsCOMPtr<nsISupports> sup;
  sup = do_QueryInterface(chromeEntry, &rv);
  if (NS_FAILED(rv))
    return NS_OK;

  *aResult = sup;
  NS_ADDREF(*aResult);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////

PRUint32 nsChromeRegistry::gRefCnt  ;
nsIRDFService* nsChromeRegistry::gRDFService = nsnull;
nsSupportsHashtable* nsChromeRegistry::mDataSourceTable = nsnull;

nsIRDFResource* nsChromeRegistry::kCHROME_chrome = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_skin = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_content = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_locale = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_base = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_main = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_name = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_archive = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_text = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_version = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_author = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_siteURL = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_previewImageURL = nsnull;

////////////////////////////////////////////////////////////////////////////////

nsChromeRegistry::nsChromeRegistry()
{
	NS_INIT_REFCNT();


  gRefCnt++;
  if (gRefCnt == 1) {
      nsresult rv;
      rv = nsServiceManager::GetService(kRDFServiceCID,
                                        NS_GET_IID(nsIRDFService),
                                        (nsISupports**)&gRDFService);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");

      // get all the properties we'll need:
      rv = gRDFService->GetResource(kURICHROME_chrome, &kCHROME_chrome);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_skin, &kCHROME_skin);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_content, &kCHROME_content);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_locale, &kCHROME_locale);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_base, &kCHROME_base);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_main, &kCHROME_main);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_name, &kCHROME_name);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_archive, &kCHROME_archive);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_text, &kCHROME_text);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_version, &kCHROME_version);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_author, &kCHROME_author);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_siteURL, &kCHROME_siteURL);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");

      rv = gRDFService->GetResource(kURICHROME_previewImageURL, &kCHROME_previewImageURL);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
  }
}

nsChromeRegistry::~nsChromeRegistry()
{
    --gRefCnt;
    if (gRefCnt == 0) {

        // release all the properties:
        NS_IF_RELEASE(kCHROME_chrome);
        NS_IF_RELEASE(kCHROME_skin);
        NS_IF_RELEASE(kCHROME_content);
        NS_IF_RELEASE(kCHROME_locale);
        NS_IF_RELEASE(kCHROME_base);
        NS_IF_RELEASE(kCHROME_main);
        NS_IF_RELEASE(kCHROME_name);
        NS_IF_RELEASE(kCHROME_archive);
        NS_IF_RELEASE(kCHROME_text);
        NS_IF_RELEASE(kCHROME_author);
        NS_IF_RELEASE(kCHROME_version);
        NS_IF_RELEASE(kCHROME_siteURL);
        NS_IF_RELEASE(kCHROME_previewImageURL);
        
        delete mDataSourceTable;
       
        if (gRDFService) {
            nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
            gRDFService = nsnull;
        }
    }
}

NS_IMPL_ISUPPORTS1(nsChromeRegistry, nsIChromeRegistry)

////////////////////////////////////////////////////////////////////////////////
// nsIChromeRegistry methods:
/*
    The ConvertChromeURL takes a chrome URL and converts it into a resource: or 
    an HTTP: url type with certain rules. Here are the current portions of a 
    chrome: url that make up the chrome-

            chrome://global/skin/foo?bar
            \------/ \----/\---/ \-----/
                |       |     |     |
                |       |     |     `-- RemainingPortion
                |       |     |
                |       |     `-- Provider 
                |       |
                |       `-- Package
                |
                '-- Always "chrome://"


    Sometime in future when Necko lands completely this will change to the 
    following syntax-

            chrome://skin@global/foo?bar

    This will make the parsing simpler and quicker (since the URL parsing already
    takes this into account)

*/
NS_IMETHODIMP
nsChromeRegistry::ConvertChromeURL(nsIURI* aChromeURL)
{
    nsresult rv = NS_OK;
    NS_ASSERTION(aChromeURL, "null url!");
    if (!aChromeURL)
        return NS_ERROR_NULL_POINTER;

#ifdef NS_DEBUG
    //Ensure that we got a chrome url!
    nsXPIDLCString scheme;
    rv = aChromeURL->GetScheme(getter_Copies(scheme));
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION((0 == PL_strncmp(scheme, "chrome", 6)), 
        "Bad scheme URL in chrome URL conversion!");
    if (0 != PL_strncmp(scheme, "chrome", 6))
        return NS_ERROR_FAILURE;
#endif

    // Obtain the package, provider and remaining from the URL
    nsXPIDLCString package, provider, remaining;

#if 0 // This change happens when we switch to using chrome://skin@global/foo..
    rv = aChromeURL->GetHost(getter_Copies(package));
    if (NS_FAILED(rv)) return rv;
    rv = aChromeURL->GetPreHost(getter_Copies(provider));
    if (NS_FAILED(rv)) return rv;
    rv = aChromeURL->GetPath(getter_Copies(remaining));
    if (NS_FAILED(rv)) return rv;
#else // For now however...

    rv = aChromeURL->GetHost(getter_Copies(package));
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString tempPath;
    rv = aChromeURL->GetPath(getter_Copies(tempPath));
    if (NS_FAILED(rv)) return rv;

    BreakProviderAndRemainingFromPath(
        (const char*)tempPath, 
        getter_Copies(provider), 
        getter_Copies(remaining));

#endif

    // Construct the lookup string-
    // which is basically chrome:// + package + provider
    
    nsAutoString lookup("chrome://");

    lookup += package; // no trailing slash here
    
    NS_ASSERTION(*provider == '/', "No leading slash here!");
    
    //definitely have a leading slash...
    if (*provider != '/')
        lookup += '/';
    lookup += provider; 
    
    // end it on a slash if none is present
    if (lookup.CharAt(lookup.Length()-1) != '/')
        lookup += '/';

    // Get the chromeResource from this lookup string
    nsCOMPtr<nsIRDFResource> chromeResource;
    if (NS_FAILED(rv = GetPackageTypeResource(lookup, getter_AddRefs(chromeResource)))) {
        NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
        return rv;
    }
    
    // Using this chrome resource get the three basic things of a chrome entry-
    // base, name, main. and don't bail if they don't exist.

    nsAutoString base, name, main;


    nsCOMPtr<nsIRDFDataSource> dataSource;
    nsString packageStr(package), providerStr(provider);
    InitializeDataSource(packageStr, providerStr, getter_AddRefs(dataSource));

    rv = GetChromeResource(dataSource, name, chromeResource, kCHROME_name);
    if (NS_FAILED (rv)) {
        if (PL_strcmp(provider,"/locale") == 0)
          name = "en-US";
        else
          name = "default";
    }

    rv = GetChromeResource(dataSource, base, chromeResource, kCHROME_base);
    if (NS_FAILED(rv))
    {
        // No base entry was found, default it to our cache.
        base = "resource:/chrome/";
        base += package;
        if ((base.CharAt(base.Length()-1) != '/') && *provider != '/')
            base += '/';
        base += provider;
        if (base.CharAt(base.Length()-1) != '/') 
            base += '/';
        if (name.Length())
            base += name;
        if (base.CharAt(base.Length()-1) != '/')
            base += '/';
    }

    NS_ASSERTION(base.CharAt(base.Length()-1) == '/', "Base doesn't end in a slash!");
    if ('/' != base.CharAt(base.Length()-1))
        base += '/';
  
    // Now we construct our finalString
    nsAutoString finalString(base);

    if (!remaining || (0 == PL_strlen(remaining)))
    {
        rv = GetChromeResource(dataSource, main, chromeResource, kCHROME_main);
        if (NS_FAILED(rv))
        {
            //we'd definitely need main for an empty remaining
            //NS_ERROR("Unable to retrieve the main file registry entry for a chrome URL.");
            //return rv;
            main = package;
            if (PL_strcmp(provider, "/skin") == 0)
              main += ".css";
            else if (PL_strcmp(provider, "/content") == 0)
              main += ".xul";
            else if (PL_strcmp(provider, "/locale") == 0)
              main += ".dtd";
        }
        finalString += main;
    }
    else
        finalString += remaining;

    char* finalURI = finalString.ToNewCString();
    aChromeURL->SetSpec(finalURI);

    nsCRT::free(finalURI);
    return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::GetOverlayDataSource(nsIURI *aChromeURL, nsIRDFDataSource **aResult)
{
  *aResult = nsnull;

  nsresult rv;

  if (!mDataSourceTable)
    return NS_OK;

  // Obtain the package, provider and remaining from the URL
  nsXPIDLCString package, provider, remaining;

#if 0 // This change happens when we switch to using chrome://skin@global/foo..
  rv = aChromeURL->GetHost(getter_Copies(package));
  if (NS_FAILED(rv)) return rv;
  rv = aChromeURL->GetPreHost(getter_Copies(provider));
  if (NS_FAILED(rv)) return rv;
  rv = aChromeURL->GetPath(getter_Copies(remaining));
  if (NS_FAILED(rv)) return rv;
#else // For now however...

  rv = aChromeURL->GetHost(getter_Copies(package));
  if (NS_FAILED(rv)) return rv;
  nsXPIDLCString tempPath;
  rv = aChromeURL->GetPath(getter_Copies(tempPath));
  if (NS_FAILED(rv)) return rv;

  BreakProviderAndRemainingFromPath(
      (const char*)tempPath, 
      getter_Copies(provider), 
      getter_Copies(remaining));

#endif

  nsCAutoString overlayFile;

  // Retrieve the mInner data source.
  overlayFile = "resource:/chrome/";
  overlayFile += package;
  overlayFile += provider; // provider already has a / in the front of it
  overlayFile += "/";
  overlayFile += "overlays.rdf";

  nsStringKey skey(overlayFile);
  nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, mDataSourceTable->Get(&skey)));
  if (supports)
  {
    nsCOMPtr<nsIRDFDataSource> dataSource = do_QueryInterface(supports, &rv);
    if (NS_SUCCEEDED(rv))
    {
      *aResult = dataSource;
      NS_ADDREF(*aResult);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsChromeRegistry::GetOverlays(nsIURI *aChromeURL, nsISimpleEnumerator **aResult)
{
  *aResult = nsnull;

  nsresult rv;

  if (!mDataSourceTable)
    return NS_OK;

  nsCOMPtr<nsIRDFDataSource> dataSource;
  GetOverlayDataSource(aChromeURL, getter_AddRefs(dataSource));

  if (dataSource)
  {
    nsCOMPtr<nsIRDFContainer> container;
    rv = nsComponentManager::CreateInstance("component://netscape/rdf/container",
                                            nsnull,
                                            NS_GET_IID(nsIRDFContainer),
                                            getter_AddRefs(container));
    if (NS_FAILED(rv))
      return NS_OK;
 
    char *lookup;
    aChromeURL->GetSpec(&lookup);

    // Get the chromeResource from this lookup string
    nsCOMPtr<nsIRDFResource> chromeResource;
    if (NS_FAILED(rv = GetPackageTypeResource(lookup, getter_AddRefs(chromeResource)))) {
        NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
        return rv;
    }
    nsAllocator::Free(lookup);

    if (NS_FAILED(container->Init(dataSource, chromeResource)))
      return NS_OK;

    nsCOMPtr<nsISimpleEnumerator> arcs;
    if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
      return NS_OK;

    *aResult = new nsOverlayEnumerator(arcs);

    NS_ADDREF(*aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::LoadDataSource(const nsCAutoString &aFileName, nsIRDFDataSource **aResult, 
                                               PRBool aUseProfileDir)
{
    nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                     nsnull,
                                                     NS_GET_IID(nsIRDFDataSource),
                                                     (void**) aResult);
    if (NS_FAILED(rv)) return rv;


    nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(*aResult);
    if (! remote)
        return NS_ERROR_UNEXPECTED;

    if (!mDataSourceTable)
      mDataSourceTable = new nsSupportsHashtable;

    nsCAutoString fileURL;
    CheckForProfileFile(aFileName, fileURL);

    rv = remote->Init(fileURL);
    if (NS_FAILED(rv)) {
      nsStringKey skey(fileURL);
      mDataSourceTable->Put(&skey, nsnull);
      return rv;
    }

    // We need to read this synchronously.
    rv = remote->Refresh(PR_TRUE);
    
    nsCOMPtr<nsISupports> supports = do_QueryInterface(remote);
    nsStringKey skey(fileURL);
    mDataSourceTable->Put(&skey, (void*)supports.get());

    return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::InitializeDataSource(nsString &aPackage,
                                       nsString &aProvider,
                                       nsIRDFDataSource **aResult,
                                       PRBool aUseProfileOnly)
{
    nsCAutoString chromeFile, overlayFile;

    // Retrieve the mInner data source.
    chromeFile = "chrome/";
    chromeFile += aPackage;
    chromeFile += aProvider; // provider already has a / in the front of it
    chromeFile += "/";
    overlayFile = chromeFile;
    chromeFile += "chrome.rdf";
    overlayFile += "overlays.rdf";

    // Try the profile root first.
    nsCAutoString profileKey;
    GetProfileRoot(profileKey);
    profileKey += chromeFile;

    nsCAutoString installKey("resource:/");
    installKey += chromeFile;

    if (mDataSourceTable)
    {
      // current.rdf and overlays.rdf are loaded in pairs and so if one is loaded, the other should be too.
      nsStringKey skey(profileKey);
      nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, mDataSourceTable->Get(&skey)));
  
      if (!supports && !aUseProfileOnly) {
        // Try the install key instead.
        nsStringKey instkey(installKey);
        supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, mDataSourceTable->Get(&instkey)));
      }

      if (supports)
      {
        nsCOMPtr<nsIRDFDataSource> dataSource = do_QueryInterface(supports);
        if (dataSource)
        {
          *aResult = dataSource;
          NS_ADDREF(*aResult);
          return NS_OK;
        }
        return NS_ERROR_FAILURE;
      }
    }

   nsCOMPtr<nsIRDFDataSource> overlayDataSource;

   LoadDataSource(chromeFile, aResult, aUseProfileOnly);
   LoadDataSource(overlayFile, getter_AddRefs(overlayDataSource), aUseProfileOnly);

   return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsChromeRegistry::GetPackageTypeResource(const nsString& aChromeType,
                                           nsIRDFResource** aResult)
{
    nsresult rv = NS_OK;
    char* url = aChromeType.ToNewCString();
    if (NS_FAILED(rv = gRDFService->GetResource(url, aResult))) {
        NS_ERROR("Unable to retrieve a resource for this package type.");
        *aResult = nsnull;
        nsCRT::free(url);
        return rv;
    }
    nsCRT::free(url);
    return NS_OK;
}

nsresult 
nsChromeRegistry::GetChromeResource(nsIRDFDataSource *aDataSource,
                                    nsString& aResult, 
                                    nsIRDFResource* aChromeResource,
                                    nsIRDFResource* aProperty)
{
    if (!aDataSource)
        return NS_ERROR_FAILURE;

    nsresult rv;

    nsCOMPtr<nsIRDFNode> chromeBase;
    if (NS_FAILED(rv = aDataSource->GetTarget(aChromeResource, aProperty, PR_TRUE, getter_AddRefs(chromeBase)))) {
        NS_ERROR("Unable to obtain a base resource.");
        return rv;
    }

    if (chromeBase == nsnull)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIRDFResource> resource;
    nsCOMPtr<nsIRDFLiteral> literal;

    if (NS_SUCCEEDED(rv = chromeBase->QueryInterface(NS_GET_IID(nsIRDFResource),
                                                     (void**) getter_AddRefs(resource)))) {
        nsXPIDLCString uri;
        resource->GetValue( getter_Copies(uri) );
        aResult = uri;
    }
    else if (NS_SUCCEEDED(rv = chromeBase->QueryInterface(NS_GET_IID(nsIRDFLiteral),
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

////////////////////////////////////////////////////////////////////////

//
// Path = provider/remaining
// 
void BreakProviderAndRemainingFromPath(const char* i_path, char** o_provider, char** o_remaining)
{
    if (!i_path || !o_provider || !o_remaining)
        return;
    int len = PL_strlen(i_path);
    NS_ASSERTION(len>1, "path is messed up!");
    char* slash = PL_strchr(i_path+1, '/'); // +1 to skip the leading slash if any
    if (slash)
    {
        *o_provider = PL_strndup(i_path, (slash - i_path)); // dont include the trailing slash
        if (slash != (i_path + len-1)) // if that was not the last trailing slash...
        {
            // don't include the leading slash here as well...
            *o_remaining = PL_strndup(slash+1, len - (slash-i_path + 1)); 
        }
        else
            *o_remaining = nsnull;
    }
    else // everything is just the provider
        *o_provider = PL_strndup(i_path, len);
}




// theme stuff

NS_IMETHODIMP nsChromeRegistry::RefreshSkins()
{
  nsresult rv;

  // Flush the style sheet cache completely.
  // XXX For now flush everything.  need a better call that only flushes style sheets.
  NS_WITH_SERVICE(nsIXULPrototypeCache, xulCache, "components://netscape/rdf/xul-prototype-cache", &rv);
  if (NS_SUCCEEDED(rv) && xulCache) {
    xulCache->Flush();
  }
  
  // Get the window mediator
  NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator)))) {
      // Get each dom window
      PRBool more;
      windowEnumerator->HasMoreElements(&more);
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (NS_SUCCEEDED(rv) && protoWindow) {
          nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(protoWindow);
          if (domWindow)
            RefreshWindow(domWindow);
        }
        windowEnumerator->HasMoreElements(&more);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::RefreshWindow(nsIDOMWindow* aWindow)
{
  // Get the DOM document.
  nsCOMPtr<nsIDOMDocument> domDocument;
  aWindow->GetDocument(getter_AddRefs(domDocument));
  if (!domDocument)
    return NS_OK;

	nsCOMPtr<nsIDocument> document = do_QueryInterface(domDocument);
	if (!document)
	  return NS_OK;

  nsCOMPtr<nsIXULDocument> xulDoc = do_QueryInterface(domDocument);
  if (xulDoc) {
		
		nsCOMPtr<nsIHTMLContentContainer> container = do_QueryInterface(document);
		nsCOMPtr<nsICSSLoader> cssLoader;
		container->GetCSSLoader(*getter_AddRefs(cssLoader));

		// Build an array of nsIURIs of style sheets we need to load.
		nsCOMPtr<nsISupportsArray> urls;
		NS_NewISupportsArray(getter_AddRefs(urls));

		PRInt32 count = document->GetNumberOfStyleSheets();
  
		// Iterate over the style sheets.
		for (PRInt32 i = 0; i < count; i++) {
			// Get the style sheet
			nsCOMPtr<nsIStyleSheet> styleSheet = getter_AddRefs(document->GetStyleSheetAt(i));
    
			// Make sure we aren't the special style sheets that never change.  We
			// want to skip those.
			nsCOMPtr<nsIHTMLStyleSheet> attrSheet;
			container->GetAttributeStyleSheet(getter_AddRefs(attrSheet));

			nsCOMPtr<nsIHTMLCSSStyleSheet> inlineSheet;
			container->GetInlineStyleSheet(getter_AddRefs(inlineSheet));

			nsCOMPtr<nsIStyleSheet> attr = do_QueryInterface(attrSheet);
			nsCOMPtr<nsIStyleSheet> inl = do_QueryInterface(inlineSheet);
			if ((attr.get() != styleSheet.get()) &&
				(inl.get() != styleSheet.get())) {
				// Get the URI and add it to our array.
				nsCOMPtr<nsIURI> uri;
				styleSheet->GetURL(*getter_AddRefs(uri));
				urls->AppendElement(uri);
      
				// Remove the sheet. 
				count--;
				i--;
				document->RemoveStyleSheet(styleSheet);
			}
		}
  
		// Iterate over the URL array and kick off an asynchronous load of the
		// sheets for our doc.
		PRUint32 urlCount;
		urls->Count(&urlCount);
		for (PRUint32 j = 0; j < urlCount; j++) {
			nsCOMPtr<nsISupports> supports = getter_AddRefs(urls->ElementAt(j));
			nsCOMPtr<nsIURL> url = do_QueryInterface(supports);
			ProcessStyleSheet(url, cssLoader, document);
		}
	}

  // Get our frames object
	nsCOMPtr<nsIDOMWindowCollection> frames;
	aWindow->GetFrames(getter_AddRefs(frames));
	if (!frames)
		return NS_OK;

  // Walk the frames
	PRUint32 length;
	frames->GetLength(&length);
  for (PRUint32 i = 0; i < length; i++) {
    // For each frame, recur.
		nsCOMPtr<nsIDOMWindow> childWindow;
		frames->Item(i, getter_AddRefs(childWindow));
		if (childWindow)
			RefreshWindow(childWindow);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsChromeRegistry::ProcessStyleSheet(nsIURL* aURL, nsICSSLoader* aLoader, nsIDocument* aDocument)
{
	PRBool doneLoading;
  nsresult rv = aLoader->LoadStyleLink(nsnull, // anElement
															aURL,
															"", // aTitle
															"", // aMedia
															kNameSpaceID_Unknown,
                              aDocument->GetNumberOfStyleSheets(),
                              nsnull,
                              doneLoading,  // Ignore doneLoading. Don't care.
                              nsnull);

  return rv;
}


NS_IMETHODIMP nsChromeRegistry::ApplyTheme(const PRUnichar *themeFileName)
{
  nsCAutoString chromeFile = "resource:/chrome/themes.rdf";
  nsCOMPtr<nsIRDFDataSource> dataSource;
  LoadDataSource(chromeFile, getter_AddRefs(dataSource));

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::ReallyRemoveOverlayFromDataSource(const PRUnichar *aDocURI,
                                                                  char *aOverlayURI)
{
  nsresult rv;
  nsCOMPtr<nsIURL> url;
  
  rv = nsComponentManager::CreateInstance("component://netscape/network/standard-url",
                                          nsnull,
                                          NS_GET_IID(nsIURL),
                                          getter_AddRefs(url));

  if (NS_FAILED(rv))
    return NS_OK;

  nsCAutoString str(aDocURI);
  url->SetSpec(str);
  nsCOMPtr<nsIRDFDataSource> dataSource;
  GetOverlayDataSource(url, getter_AddRefs(dataSource));

  if (!dataSource)
    return NS_OK;

  nsCOMPtr<nsIRDFResource> resource;
  rv = GetPackageTypeResource(aDocURI,
                              getter_AddRefs(resource));

  if (NS_FAILED(rv))
    return NS_OK;

  nsCOMPtr<nsIRDFContainer> container;

  rv = nsComponentManager::CreateInstance("component://netscape/rdf/container",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  if (NS_FAILED(container->Init(dataSource, resource)))
    return NS_ERROR_FAILURE;

  nsAutoString unistr(aOverlayURI);
  nsCOMPtr<nsIRDFLiteral> literal;
  gRDFService->GetLiteral(unistr.GetUnicode(), getter_AddRefs(literal));

  container->RemoveElement(literal, PR_TRUE);

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource, &rv);
  if (NS_FAILED(rv))
    return NS_OK;
  
  remote->Flush();

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::RemoveOverlay(nsIRDFDataSource *aDataSource, nsIRDFResource *aResource)
{
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv;

  rv = nsComponentManager::CreateInstance("component://netscape/rdf/container",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  if (NS_FAILED(container->Init(aDataSource, aResource)))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_ERROR_FAILURE;

  PRBool moreElements;
  arcs->HasMoreElements(&moreElements);
  
  char *value;
  aResource->GetValue(&value);

  while (moreElements)
  {
    nsCOMPtr<nsISupports> supports;
    arcs->GetNext(getter_AddRefs(supports));

    nsCOMPtr<nsIRDFLiteral> literal = do_QueryInterface(supports, &rv);

    if (NS_SUCCEEDED(rv))
    {
      const PRUnichar* valueStr;
      rv = literal->GetValueConst(&valueStr);
      if (NS_FAILED(rv))
        return rv;

      ReallyRemoveOverlayFromDataSource(valueStr, value);
    }
    arcs->HasMoreElements(&moreElements);
  }
  nsAllocator::Free(value);

  return NS_OK;
}


NS_IMETHODIMP nsChromeRegistry::RemoveOverlays(nsAutoString aPackage,
                                               nsAutoString aProvider,
                                               nsIRDFContainer *aContainer,
                                               nsIRDFDataSource *aDataSource)
{
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(aContainer->GetElements(getter_AddRefs(arcs))))
    return NS_OK;

  PRBool moreElements;
  arcs->HasMoreElements(&moreElements);
  
  while (moreElements)
  {
    nsCOMPtr<nsISupports> supports;
    arcs->GetNext(getter_AddRefs(supports));

    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(supports, &rv);

    if (NS_SUCCEEDED(rv))
    {
      RemoveOverlay(aDataSource, resource);
    }

    arcs->HasMoreElements(&moreElements);
  }

  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::SetSkin(const PRUnichar *aSkin,
                                           const PRUnichar *aPackageName)
{
  nsAutoString provider("skin");
  return SelectProviderForPackage(aSkin, aPackageName, provider.GetUnicode());
}

NS_IMETHODIMP nsChromeRegistry::SetLocale(const PRUnichar *aLocale,
                                             const PRUnichar *aPackageName)
{
  nsAutoString provider("locale");
  return SelectProviderForPackage(aLocale, aPackageName, provider.GetUnicode());
}

NS_IMETHODIMP nsChromeRegistry::SetDefaultSkin(const PRUnichar *aSkin,
                                               const PRUnichar *aPackageName)
{
  // XXX To do.
  NS_ERROR("WRITE ME!\n");
  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::SetDefaultLocale(const PRUnichar *aSkin,
                                                 const PRUnichar *aPackageName)
{
  // XXX To do.
  NS_ERROR("WRITE ME!\n");
  return NS_OK;
}

NS_IMETHODIMP nsChromeRegistry::SelectProviderForPackage(const PRUnichar *aThemeFileName,
                                                         const PRUnichar *aPackageName, 
                                                         const PRUnichar *aProviderName)
{
  nsresult rv;

  // Fetch the data source, guaranteeing that it comes from the
  // profile directory.
  // Get the correct data source. Force it to come from the profile dir.
  nsCOMPtr<nsIRDFDataSource> dataSource;
  nsAutoString package(aPackageName), provider(aProviderName);
  if (NS_FAILED(rv = InitializeDataSource(package, provider, getter_AddRefs(dataSource), PR_TRUE)))
    return rv;

  // Assert the new skin.  If a value already exists, blow it away using a Change operation.
  nsCOMPtr<nsIRDFResource> chromeResource;
  nsAutoString name;
  rv = GetChromeResource(dataSource, name, chromeResource, kCHROME_name);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsAutoString lookup("chrome://");
  lookup += aPackageName;
  lookup += "/";
  lookup += aProviderName;
  lookup += "/";

  nsCOMPtr<nsIRDFResource> source;
  if (NS_FAILED(rv = GetPackageTypeResource(lookup, getter_AddRefs(source)))) {
    NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
    return rv;
  }

  // Get the old targets
  nsCOMPtr<nsIRDFNode> retVal;
  dataSource->GetTarget(source, chromeResource, PR_TRUE, getter_AddRefs(retVal));

  nsCOMPtr<nsIRDFLiteral> literal;
  gRDFService->GetLiteral(name.GetUnicode(), getter_AddRefs(literal));

  if (retVal) {
    // Perform a CHANGE operation.
    dataSource->Change(source, chromeResource, retVal, literal);
  } else {
    // Do an ASSERT instead.
    dataSource->Assert(source, chromeResource, literal, PR_TRUE);
  }

  nsCOMPtr<nsIRDFRemoteDataSource> remote = do_QueryInterface(dataSource);
  if (! remote)
    return NS_ERROR_UNEXPECTED;

  remote->Flush();

  return NS_OK;
}

NS_IMETHODIMP 
nsChromeRegistry::CheckForProfileFile(const nsCAutoString& aFileName, nsCAutoString& aFileURL) 
{ 
  nsCOMPtr<nsIFileLocator> fl;
  
  nsresult rv = nsComponentManager::CreateInstance("component://netscape/filelocator",
                                          nsnull,
                                          NS_GET_IID(nsIFileLocator),
                                          getter_AddRefs(fl));

  if (NS_FAILED(rv))
    return NS_OK;

  // Build a fileSpec that points to the destination
  // (profile dir + chrome + package + provider + chrome.rdf)
  nsCOMPtr<nsIFileSpec> chromeFileInterface;
  fl->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, getter_AddRefs(chromeFileInterface));
  nsFileSpec chromeFile;

  PRBool exists = PR_FALSE;
  if (chromeFileInterface) {
    chromeFileInterface->GetFileSpec(&chromeFile);
    chromeFile += "/";
    chromeFile += (const char*)aFileName;
    exists = chromeFile.Exists();
  }

  if (exists) {
    nsFileURL fileURL(chromeFile);
    const char* fileStr = fileURL.GetURLString();
    aFileURL = fileStr;
    aFileURL += aFileName;
  }
  else {
    // Use the install dir.
    aFileURL = "resource:/";
    aFileURL += aFileName;
  }

  return NS_OK; 
}
    
NS_IMETHODIMP
nsChromeRegistry::GetProfileRoot(nsCAutoString& aFileURL) 
{ 
  nsCOMPtr<nsIFileLocator> fl;
  
  nsresult rv = nsComponentManager::CreateInstance("component://netscape/filelocator",
                                          nsnull,
                                          NS_GET_IID(nsIFileLocator),
                                          getter_AddRefs(fl));

  if (NS_FAILED(rv))
    return NS_OK;

  // Build a fileSpec that points to the destination
  // (profile dir + chrome + package + provider + chrome.rdf)
  nsCOMPtr<nsIFileSpec> chromeFileInterface;
  fl->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, getter_AddRefs(chromeFileInterface));

  if (chromeFileInterface) {
    nsFileSpec chromeFile;
    chromeFileInterface->GetFileSpec(&chromeFile);
    nsFileURL fileURL(chromeFile);
    const char* fileStr = fileURL.GetURLString();
    aFileURL = fileStr;
  }
  else {
    aFileURL = "resource:/";
  }
  return NS_OK; 
}


NS_IMETHODIMP
nsChromeRegistry::ReloadChrome()
{
	// Do a reload of all top level windows.
	nsresult rv;

  // Flush the cache completely.
  NS_WITH_SERVICE(nsIXULPrototypeCache, xulCache, "components://netscape/rdf/xul-prototype-cache", &rv);
  if (NS_SUCCEEDED(rv) && xulCache) {
    xulCache->Flush();
  }
  
  // Get the window mediator
  NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator)))) {
      // Get each dom window
      PRBool more;
      windowEnumerator->HasMoreElements(&more);
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        rv = windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (NS_SUCCEEDED(rv) && protoWindow) {
          nsCOMPtr<nsPIDOMWindow> domWindow = do_QueryInterface(protoWindow);
          if (domWindow) {
						nsCOMPtr<nsIDOMLocation> location;
						domWindow->GetLocation(getter_AddRefs(location));
						if (location)
              location->Reload(PR_FALSE);
					}
        }
        windowEnumerator->HasMoreElements(&more);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::GetEnumeratorForType(const nsCAutoString& type, nsISimpleEnumerator** aResult)
{
  // Load the data source.
  nsCAutoString chromeFile("chrome/");
  chromeFile += type;
  chromeFile += ".rdf";

  // Get the profile root for the first set of arcs.
  nsCAutoString profileKey;
  GetProfileRoot(profileKey);
  profileKey += chromeFile;

  // Use the install root for the second set of arcs.
  nsCAutoString installKey("resource:/");
  installKey += chromeFile;

  nsCOMPtr<nsIRDFDataSource> profileDataSource;
  nsCOMPtr<nsIRDFDataSource> installDataSource;

  nsresult rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                     nsnull,
                                                     NS_GET_IID(nsIRDFDataSource),
                                                     (void**) getter_AddRefs(profileDataSource));
  if (NS_FAILED(rv)) return rv;

  rv = nsComponentManager::CreateInstance(kRDFXMLDataSourceCID,
                                                     nsnull,
                                                     NS_GET_IID(nsIRDFDataSource),
                                                     (void**) getter_AddRefs(installDataSource));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFRemoteDataSource> remoteProfile = do_QueryInterface(profileDataSource);
  nsCOMPtr<nsIRDFRemoteDataSource> remoteInstall = do_QueryInterface(installDataSource);
   
  nsCOMPtr<nsIFileLocator> fl;
  
  rv = nsComponentManager::CreateInstance("component://netscape/filelocator",
                                          nsnull,
                                          NS_GET_IID(nsIFileLocator),
                                          getter_AddRefs(fl));

  if (NS_FAILED(rv))
    return NS_OK;

  // Build a fileSpec that points to the destination
  nsCOMPtr<nsIFileSpec> chromeFileInterface;
  fl->GetFileLocation(nsSpecialFileSpec::App_UserProfileDirectory50, getter_AddRefs(chromeFileInterface));
  nsFileSpec fileSpec;

  if (chromeFileInterface) {
    // Read in the profile data source (or try to anyway)
    chromeFileInterface->GetFileSpec(&fileSpec);
    fileSpec += "/";
    fileSpec += (const char*)chromeFile;

    nsFileURL fileURL(fileSpec);
    const char* fileStr = fileURL.GetURLString();
    nsCAutoString fileURLStr = fileStr;
    fileURLStr += chromeFile;

    remoteProfile->Init(fileURLStr);
    remoteProfile->Refresh(PR_TRUE);
  }

  // Read in the install data source.
  nsCAutoString fileURLStr = "resource:/";
  fileURLStr += chromeFile;
  remoteInstall->Init(fileURLStr);
  remoteInstall->Refresh(PR_TRUE);

  // Get some arcs from each data source.
  nsCOMPtr<nsISimpleEnumerator> profileArcs, installArcs;

  GetArcs(profileDataSource, type, getter_AddRefs(profileArcs));
  GetArcs(installDataSource, type, getter_AddRefs(installArcs));

  // Build an nsChromeEntryEnumerator and return it.
  *aResult = new nsChromeEntryEnumerator(profileDataSource, installDataSource, 
                                         profileArcs, installArcs);
  NS_ADDREF(*aResult);
  
  return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::GetSkins(nsISimpleEnumerator** aResult)
{
	nsCAutoString type("skins");
  return GetEnumeratorForType(type, aResult);
}

NS_IMETHODIMP
nsChromeRegistry::GetPackages(nsISimpleEnumerator** aResult)
{
  nsCAutoString type("packages");
  return GetEnumeratorForType(type, aResult);
}

NS_IMETHODIMP
nsChromeRegistry::GetLocales(nsISimpleEnumerator** aResult)
{
  nsCAutoString type("locales");
	return GetEnumeratorForType(type, aResult);
}

NS_IMETHODIMP
nsChromeRegistry::GetArcs(nsIRDFDataSource* aDataSource,
                          const nsCAutoString& aType,
                          nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIRDFContainer> container;
  nsresult rv = nsComponentManager::CreateInstance("component://netscape/rdf/container",
                                          nsnull,
                                          NS_GET_IID(nsIRDFContainer),
                                          getter_AddRefs(container));
  if (NS_FAILED(rv))
    return NS_OK;

  nsCAutoString lookup("chrome:");
  lookup += aType;

  // Get the chromeResource from this lookup string
  nsCOMPtr<nsIRDFResource> chromeResource;
  if (NS_FAILED(rv = GetPackageTypeResource(lookup, getter_AddRefs(chromeResource)))) {
      NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
      return rv;
  }
  
  if (NS_FAILED(container->Init(aDataSource, chromeResource)))
    return NS_OK;

  nsCOMPtr<nsISimpleEnumerator> arcs;
  if (NS_FAILED(container->GetElements(getter_AddRefs(arcs))))
    return NS_OK;
  
  *aResult = arcs;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////

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

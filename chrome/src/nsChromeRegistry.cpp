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
#include "nsFileSpec.h"
#include "nsSpecialSystemDirectory.h"
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
static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);
static NS_DEFINE_CID(kRDFXMLDataSourceCID, NS_RDFXMLDATASOURCE_CID);
static NS_DEFINE_IID(kIRDFIntIID, NS_IRDFINT_IID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

#define CHROME_NAMESPACE_URI "http://chrome.mozilla.org/rdf#"
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, chrome);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, skin);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, content);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, locale);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, platform);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, behavior);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, base);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, main);
DEFINE_RDF_VOCAB(CHROME_NAMESPACE_URI, CHROME, archive);

DEFINE_RDF_VOCAB(RDF_NAMESPACE_URI, RDF, Description);

////////////////////////////////////////////////////////////////////////////////

class nsChromeRegistry : public nsIChromeRegistry, public nsIRDFDataSource {
public:
    NS_DECL_ISUPPORTS

    // nsIChromeRegistry methods:
    NS_IMETHOD InitRegistry();  
    NS_IMETHOD ConvertChromeURL(nsIURL* aChromeURL);

    // nsIRDFDataSource methods
    NS_IMETHOD Init(const char* uri)  ; // Called to init the data source.
    NS_IMETHOD GetURI(char** uri);
    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source /* out */)  ;
    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources /* out */)  ;
    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target /* out */)  ;
    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets /* out */)  ;
    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv)  ;
    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target)  ;
    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion /* out */)  ;
    NS_IMETHOD AddObserver(nsIRDFObserver* n)  ;
    NS_IMETHOD RemoveObserver(nsIRDFObserver* n)  ;
    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels /* out */)  ;
    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels /* out */)  ;
    NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor)  ;
    NS_IMETHOD Flush(void)  ;
    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands)  ;
    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* retVal)  ;
    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments)  ;

    // nsChromeRegistry methods:
    nsChromeRegistry();
    virtual ~nsChromeRegistry();

    static PRUint32 gRefCnt;
    static nsIRDFService* gRDFService;
    static nsIRDFResource* kCHROME_chrome;
    static nsIRDFResource* kCHROME_skin;
    static nsIRDFResource* kCHROME_content;
    static nsIRDFResource* kCHROME_platform;
    static nsIRDFResource* kCHROME_locale;
    static nsIRDFResource* kCHROME_behavior;
    static nsIRDFResource* kCHROME_base;
    static nsIRDFResource* kCHROME_main;
    static nsIRDFResource* kCHROME_archive;
    static nsIRDFDataSource* mInner;

protected:
    nsresult GetProviderTypeResource(const nsString& aChromeType, nsIRDFResource** aResult);
    nsresult GetChromeResource(nsString& aResult, nsIRDFResource* aChromeResource,
                               nsIRDFResource* aProperty);    
};

PRUint32 nsChromeRegistry::gRefCnt  ;
nsIRDFService* nsChromeRegistry::gRDFService = nsnull;

nsIRDFResource* nsChromeRegistry::kCHROME_chrome = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_skin = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_content = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_locale = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_behavior = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_platform = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_base = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_main = nsnull;
nsIRDFResource* nsChromeRegistry::kCHROME_archive = nsnull;
nsIRDFDataSource* nsChromeRegistry::mInner = nsnull;

////////////////////////////////////////////////////////////////////////////////

nsChromeRegistry::nsChromeRegistry()
{
	NS_INIT_REFCNT();
  
}

nsChromeRegistry::~nsChromeRegistry()
{
    
    --gRefCnt;
    if (gRefCnt == 0) {

        // Release our inner data source
        NS_IF_RELEASE(mInner);

        // release all the properties:
        NS_IF_RELEASE(kCHROME_chrome);
        NS_IF_RELEASE(kCHROME_skin);
        NS_IF_RELEASE(kCHROME_content);
        NS_IF_RELEASE(kCHROME_platform);
        NS_IF_RELEASE(kCHROME_locale);
        NS_IF_RELEASE(kCHROME_behavior);
        NS_IF_RELEASE(kCHROME_base);
        NS_IF_RELEASE(kCHROME_main);
        NS_IF_RELEASE(kCHROME_archive);
       
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
    if (aIID.Equals(kIRDFDataSourceIID)) {
        *aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
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
    
    // Retrieve the resource for this chrome element.
    const char* host;
    if (NS_FAILED(rv = aChromeURL->GetHost(&host))) {
        NS_ERROR("Unable to retrieve the host for a chrome URL.");
        return rv;
    }

    nsAutoString hostStr(host);
    
    // Construct a chrome URL and use it to look up a resource.
    nsAutoString windowType = nsAutoString("chrome://") + hostStr + "/";

    // Find out the provider type of the URL
    const char* file;
    aChromeURL->GetFile(&file);
    nsAutoString restOfURL(file);
    
		// Find the second slash.
		nsAutoString providerType("content");
		nsAutoString path("");
	  PRInt32 slashIndex = -1;
	  if (restOfURL.Length() > 1)
		{
			// There is something to the right of that slash. A provider type must have
			// been specified.
			slashIndex = restOfURL.Find('/', 1);
		  if (slashIndex == -1)
		    slashIndex = restOfURL.Length();

			restOfURL.Mid(providerType, 1, slashIndex - 1);

			if (slashIndex < restOfURL.Length()-1)
			{
				// There are some extra subdirectories to remember.
				restOfURL.Right(path, restOfURL.Length()-slashIndex-1);
			}
		}

    windowType += providerType + "/";

    // We have the resource URI that we wish to retrieve. Fetch it.
    nsCOMPtr<nsIRDFResource> chromeResource;
    if (NS_FAILED(rv = GetProviderTypeResource(windowType, getter_AddRefs(chromeResource)))) {
        NS_ERROR("Unable to retrieve the resource corresponding to the chrome skin or content.");
        return rv;
    }

    nsString chromeBase;
    if (NS_FAILED(rv = GetChromeResource(chromeBase, chromeResource, kCHROME_base))) {
        NS_ERROR("Unable to retrieve codebase for chrome entry.");
        return rv;
    }

		// Make sure base ends in a slash
	  PRInt32 length = chromeBase.Length();
		if (length > 0)
		{
			PRUnichar c = chromeBase.CharAt(length-1);
			if (c != '/')
				chromeBase += "/"; // Ensure that a slash is present.
		}

    // Check to see if we should append the "main" entry in the registry.
    // Only do this when the user doesn't have anything following "skin"
    // or "content" in the specified URL.
    if (path == "")
    {
			  PRInt32 length = restOfURL.Length();
				if (length > 0)
				{
					PRUnichar c = restOfURL.CharAt(length-1);
					if (c != '/')
						restOfURL += "/"; // Ensure that a slash is present.
				}
				  
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
        // XXX Just append the rest of the URL to base to get the actual URL to look up.
			  chromeBase += path;
    }

    char* finalDecision = chromeBase.ToNewCString();
    aChromeURL->SetSpec(finalDecision);
    delete []finalDecision;
    return NS_OK;
}

NS_IMETHODIMP
nsChromeRegistry::InitRegistry()
{
  if (mInner == nsnull)
    return Init("rdf:chrome");
  else return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRDFObserver methods:


////////////////////////////////////////////////////////////////////////////////

nsresult
nsChromeRegistry::GetProviderTypeResource(const nsString& aChromeType,
                                           nsIRDFResource** aResult)
{
    nsresult rv = NS_OK;
    char* url = aChromeType.ToNewCString();
    if (NS_FAILED(rv = gRDFService->GetResource(url, aResult))) {
        NS_ERROR("Unable to retrieve a resource for this provider type.");
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

    if (mInner == nsnull)
        return NS_ERROR_FAILURE; // Must have a DB to attempt this operation.

    nsCOMPtr<nsIRDFNode> chromeBase;
    if (NS_FAILED(rv = GetTarget(aChromeResource, aProperty, PR_TRUE, getter_AddRefs(chromeBase)))) {
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

// nsIRDFDataSource methods
NS_IMETHODIMP
nsChromeRegistry::Init(const char* uri)
{
  // We're going to be init'ed with the "rdf:chrome" URI.
  // We want to use the location of the registry source instead.
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

      rv = gRDFService->GetResource(kURICHROME_content, &kCHROME_platform);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;

      rv = gRDFService->GetResource(kURICHROME_content, &kCHROME_locale);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv)) return rv;

      rv = gRDFService->GetResource(kURICHROME_content, &kCHROME_behavior);
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

      // Retrieve the mInner data source.
      nsSpecialSystemDirectory chromeFile(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
      chromeFile += "chrome";
      chromeFile += "registry.rdf";

      nsFileURL chromeURL(chromeFile);
      const char* innerURI = chromeURL.GetAsString();

      // Try to create the new data source.
      if (NS_FAILED(rv = gRDFService->GetDataSource(innerURI, &mInner))) 
        return rv;
  }
  
  return rv;
}
  
NS_IMETHODIMP 
nsChromeRegistry::GetURI(char** uri)
{
  return mInner->GetURI(uri);
}

NS_IMETHODIMP 
nsChromeRegistry::GetSource(nsIRDFResource* property,
                     nsIRDFNode* target,
                     PRBool tv,
                     nsIRDFResource** source /* out */)
{
  return mInner->GetSource(property, target, tv, source);
}

NS_IMETHODIMP 
nsChromeRegistry::GetSources(nsIRDFResource* property,
                      nsIRDFNode* target,
                      PRBool tv,
                      nsIRDFAssertionCursor** sources /* out */)
{
  return mInner->GetSources(property, target, tv, sources);
}

NS_IMETHODIMP 
nsChromeRegistry::GetTarget(nsIRDFResource* source,
                     nsIRDFResource* property,
                     PRBool tv,
                     nsIRDFNode** target /* out */)
{

  return mInner->GetTarget(source, property, tv, target);
}

NS_IMETHODIMP 
nsChromeRegistry::GetTargets(nsIRDFResource* source,
                      nsIRDFResource* property,
                      PRBool tv,
                      nsIRDFAssertionCursor** targets /* out */)
{
  return mInner->GetTargets(source, property, tv, targets);
}

NS_IMETHODIMP 
nsChromeRegistry::Assert(nsIRDFResource* source, 
                  nsIRDFResource* property, 
                  nsIRDFNode* target,
                  PRBool tv)
{
  return mInner->Assert(source, property, target, tv);
}

NS_IMETHODIMP 
nsChromeRegistry::Unassert(nsIRDFResource* source,
                    nsIRDFResource* property,
                    nsIRDFNode* target)
{
  return mInner->Unassert(source, property, target);
}

NS_IMETHODIMP 
nsChromeRegistry::HasAssertion(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target,
                        PRBool tv,
                        PRBool* hasAssertion /* out */)
{
  return mInner->HasAssertion(source, property, target, tv, hasAssertion);
}

NS_IMETHODIMP nsChromeRegistry::AddObserver(nsIRDFObserver* n)
{
  return mInner->AddObserver(n);
}

NS_IMETHODIMP nsChromeRegistry::RemoveObserver(nsIRDFObserver* n)
{
  return mInner->RemoveObserver(n);
}

NS_IMETHODIMP nsChromeRegistry::ArcLabelsIn(nsIRDFNode* node,
                       nsIRDFArcsInCursor** labels /* out */)
{
  return mInner->ArcLabelsIn(node, labels);
}

NS_IMETHODIMP nsChromeRegistry::ArcLabelsOut(nsIRDFResource* source,
                        nsIRDFArcsOutCursor** labels /* out */) 
{
  return mInner->ArcLabelsOut(source, labels);
}

NS_IMETHODIMP nsChromeRegistry::GetAllResources(nsIRDFResourceCursor** aCursor)
{
  return mInner->GetAllResources(aCursor);
}

NS_IMETHODIMP 
nsChromeRegistry::Flush(void)
{
  return mInner->Flush();
}

NS_IMETHODIMP 
nsChromeRegistry::GetAllCommands(nsIRDFResource* source,
                          nsIEnumerator/*<nsIRDFResource>*/** commands)
{
  return mInner->GetAllCommands(source, commands);
}

NS_IMETHODIMP 
nsChromeRegistry::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                            nsIRDFResource*   aCommand,
                            nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                            PRBool* retVal)
{
  return mInner->IsCommandEnabled(aSources, aCommand, aArguments, retVal);
}

NS_IMETHODIMP 
nsChromeRegistry::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                     nsIRDFResource*   aCommand,
                     nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  return mInner->DoCommand(aSources, aCommand, aArguments);
}

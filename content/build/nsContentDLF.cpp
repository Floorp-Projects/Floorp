/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsContentDLF.h"
#include "nsIComponentManager.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIURL.h"
#include "nsICSSStyleSheet.h"
#include "nsString.h"
#include "nsContentCID.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsICSSLoader.h"

#include "nsRDFCID.h"
#include "nsIRDFResource.h"
#include "nsIXULContentSink.h"
#include "nsIDocStreamLoaderFactory.h"

// URL for the "user agent" style sheet
#define UA_CSS_URL "resource:/res/ua.css"

// Factory code for creating variations on html documents

#undef NOISY_REGISTRY

static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_IID(kImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kXULDocumentCID, NS_XULDOCUMENT_CID);

extern nsresult NS_NewDocumentViewer(nsIDocumentViewer** aResult);

static char* gHTMLTypes[] = {
  "text/html",
  "text/plain",
  "text/css",
  "text/javascript",
  "application/x-javascript",
  "text/html; x-view-type=view-source",
  "text/plain; x-view-type=view-source",
  "text/css; x-view-type=view-source",
  "text/javascript; x-view-type=view-source",
  "application/x-javascript; x-view-type=view-source",
  0
};
  
static char* gXMLTypes[] = {
  "text/xml",
  "application/xml",
  "application/xhtml+xml",
  "text/xml; x-view-type=view-source",
  "application/xml; x-view-type=view-source",
  "application/xhtml+xml; x-view-type=view-source",
  0
};


static char* gRDFTypes[] = {
  "text/rdf",
  "application/vnd.mozilla.xul+xml",
  "mozilla.application/cached-xul",
  "application/vnd.mozilla.xul+xml; x-view-type=view-source",
  "mozilla.application/cached-xul; x-view-type=view-source",
  0
};

static char* gImageTypes[] = {
  "image/gif",
  "image/jpeg",
  "image/jpg",
  "image/pjpeg",
  "image/png",
  "image/x-png",
  "image/x-art",
  "image/x-jg",
  "image/bmp",
  "image/x-icon",
  0
};

nsICSSStyleSheet* nsContentDLF::gUAStyleSheet;

nsresult
NS_NewContentDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult)
{
  NS_PRECONDITION(aResult, "null OUT ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsContentDLF* it = new nsContentDLF();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDocumentLoaderFactory), (void**)aResult);
}

nsContentDLF::nsContentDLF()
{
  NS_INIT_REFCNT();
}

nsContentDLF::~nsContentDLF()
{
}

NS_IMPL_ISUPPORTS2(nsContentDLF,
                   nsIDocumentLoaderFactory,
                   nsIDocStreamLoaderFactory);

NS_IMETHODIMP
nsContentDLF::CreateInstance(const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             const char* aContentType, 
                             nsISupports* aContainer,
                             nsISupports* aExtraInfo,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aDocViewer)
{
  nsresult rv = NS_OK;
  if (!gUAStyleSheet) {
    // Load the UA style sheet
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), UA_CSS_URL);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICSSLoader> cssLoader;
      NS_NewCSSLoader(getter_AddRefs(cssLoader));
      if (cssLoader) {
        PRBool complete;
        rv = cssLoader->LoadAgentSheet(uri, gUAStyleSheet, complete, nsnull);
      }
    }
    if (NS_FAILED(rv)) {
  #ifdef DEBUG
      printf("*** open of %s failed: error=%x\n", UA_CSS_URL, rv);
  #endif
      return rv;
    }
  }

  // Check aContentType to see if it's a view-source type
  //
  // If it's a "view-source:", aContentType will be of the form
  //
  //    <orig_type>; x-view-type=view-source
  //
  //  where <orig_type> can be text/html, text/xml etc.
  //

  nsCAutoString strContentType; strContentType.Append(aContentType);
  PRInt32 idx = strContentType.Find("; x-view-type=view-source", PR_TRUE, 0, -1);
  if(idx != -1)
  { // Found "; x-view-type=view-source" param in content type. 

      // Set aCommand to view-source

      aCommand = "view-source";

     // Null terminate at the ";" in "text/html; x-view-type=view-source"
     // The idea is to end up with the original content type i.e. without 
     // the x-view-type param was added to it.

     strContentType.SetCharAt('\0', idx);

     aContentType = strContentType.get(); //This will point to the "original" mime type
  }

  if(0==PL_strcmp(aCommand,"view-source")) {
    NS_ENSURE_ARG(aChannel);
    // It's a view-source. Reset channel's content type to the original 
    // type so as not to choke the parser when it asks the channel 
    // for the content type during the parse phase
    aChannel->SetContentType(aContentType);
    aContentType=gHTMLTypes[0];    
  }

  // Try html
  int typeIndex=0;
  while(gHTMLTypes[typeIndex]) {
    if (0== PL_strcmp(gHTMLTypes[typeIndex++], aContentType)) {
      return CreateDocument(aCommand, 
                            aChannel, aLoadGroup,
                            aContainer, kHTMLDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try XML
  typeIndex = 0;
  while(gXMLTypes[typeIndex]) {
    if (0== PL_strcmp(gXMLTypes[typeIndex++], aContentType)) {
      return CreateDocument(aCommand, 
                            aChannel, aLoadGroup,
                            aContainer, kXMLDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try RDF
  typeIndex = 0;
  while (gRDFTypes[typeIndex]) {
    if (0 == PL_strcmp(gRDFTypes[typeIndex++], aContentType)) {
      return CreateRDFDocument(aCommand, 
                               aChannel, aLoadGroup,
                               aContentType, aContainer,
                               aExtraInfo, aDocListener, aDocViewer);
    }
  }

  // Try image types
  typeIndex = 0;
  while(gImageTypes[typeIndex]) {
    if (0== PL_strcmp(gImageTypes[typeIndex++], aContentType)) {
      return CreateDocument(aCommand, 
                            aChannel, aLoadGroup,
                            aContainer, kImageDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // If we get here, then we weren't able to create anything. Sorry!
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsContentDLF::CreateInstanceForDocument(nsISupports* aContainer,
                                        nsIDocument* aDocument,
                                        const char *aCommand,
                                        nsIContentViewer** aDocViewerResult)
{
  nsresult rv = NS_ERROR_FAILURE;  

  do {
    nsCOMPtr<nsIDocumentViewer> docv;
    rv = NS_NewDocumentViewer(getter_AddRefs(docv));
    if (NS_FAILED(rv))
      break;

    docv->SetUAStyleSheet(NS_STATIC_CAST(nsIStyleSheet*, gUAStyleSheet));

    // Bind the document to the Content Viewer
    nsIContentViewer* cv = NS_STATIC_CAST(nsIContentViewer*, docv.get());
    rv = cv->LoadStart(aDocument);
    NS_ADDREF(*aDocViewerResult = cv);
  } while (PR_FALSE);

  return rv;
}

nsresult
nsContentDLF::CreateDocument(const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsISupports* aContainer,
                             const nsCID& aDocumentCID,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aDocViewer)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

#ifdef NOISY_CREATE_DOC
  if (nsnull != aURL) {
    nsAutoString tmp;
    aURL->ToString(tmp);
    fputs(NS_LossyConvertUCS2toASCII(tmp).get(), stdout);
    printf(": creating document\n");
  }
#endif

  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIDocumentViewer> docv;
  do {
    // Create the document
    rv = nsComponentManager::CreateInstance(aDocumentCID, nsnull,
                                            NS_GET_IID(nsIDocument),
                                            getter_AddRefs(doc));
    if (NS_FAILED(rv))
      break;

    // Create the document viewer  XXX: could reuse document viewer here!
    rv = NS_NewDocumentViewer(getter_AddRefs(docv));
    if (NS_FAILED(rv))
      break;
    docv->SetUAStyleSheet(gUAStyleSheet);

    // Initialize the document to begin loading the data.  An
    // nsIStreamListener connected to the parser is returned in
    // aDocListener.
    rv = doc->StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer, aDocListener, PR_TRUE);
    if (NS_FAILED(rv))
      break;

    // Bind the document to the Content Viewer
    rv = docv->LoadStart(doc);
    *aDocViewer = docv;
    NS_IF_ADDREF(*aDocViewer);
  } while (PR_FALSE);

  return rv;
}

NS_IMETHODIMP
nsContentDLF::CreateInstance(nsIInputStream& aInputStream,
                             const char* aContentType,
                             const char* aCommand,
                             nsISupports* aContainer,
                             nsISupports* aExtraInfo,
                             nsIContentViewer** aDocViewer)

{
  nsresult status = NS_ERROR_FAILURE;

  // Try RDF
  int typeIndex = 0;
  while (gRDFTypes[typeIndex]) {
    if (0 == PL_strcmp(gRDFTypes[typeIndex++], aContentType)) {
      return CreateXULDocumentFromStream(aInputStream,
                                         aCommand,
                                         aContainer,
                                         aExtraInfo,
                                         aDocViewer);
    }
  }

  return status;
}

// ...common work for |CreateRDFDocument| and |CreateXULDocumentFromStream|
nsresult
nsContentDLF::CreateRDFDocument(nsISupports* aExtraInfo,
                                nsCOMPtr<nsIDocument>* doc,
                                nsCOMPtr<nsIDocumentViewer>* docv)
{
  nsresult rv = NS_ERROR_FAILURE;
    
  // Create the XUL document
  rv = nsComponentManager::CreateInstance(kXULDocumentCID, nsnull,
                                          NS_GET_IID(nsIDocument),
                                          getter_AddRefs(*doc));
  if (NS_FAILED(rv)) return rv;

  // Create the image content viewer...
  rv = NS_NewDocumentViewer(getter_AddRefs(*docv));
  if (NS_FAILED(rv)) return rv;

  // Load the UA style sheet if we haven't already done that
  (*docv)->SetUAStyleSheet(gUAStyleSheet);

  return NS_OK;
}

// ...note, this RDF document _may_ be XUL :-)
nsresult
nsContentDLF::CreateRDFDocument(const char* aCommand,
                                nsIChannel* aChannel,
                                nsILoadGroup* aLoadGroup,
                                const char* aContentType,
                                nsISupports* aContainer,
                                nsISupports* aExtraInfo,
                                nsIStreamListener** aDocListener,
                                nsIContentViewer** aDocViewer)
{
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIDocumentViewer> docv;
  nsresult rv = CreateRDFDocument(aExtraInfo, address_of(doc), address_of(docv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

  /* 
   * Initialize the document to begin loading the data...
   *
   * An nsIStreamListener connected to the parser is returned in
   * aDocListener.
   */
  rv = doc->StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer, aDocListener, PR_TRUE);
  if (NS_SUCCEEDED(rv)) {
    /*
     * Bind the document to the Content Viewer...
     */
    rv = docv->LoadStart(doc);
    *aDocViewer = docv;
    NS_IF_ADDREF(*aDocViewer);
  }
   
  return rv;
}

nsresult
nsContentDLF::CreateXULDocumentFromStream(nsIInputStream& aXULStream,
                                          const char* aCommand,
                                          nsISupports* aContainer,
                                          nsISupports* aExtraInfo,
                                          nsIContentViewer** aDocViewer)
{
  nsresult status = NS_OK;

#if 0 // XXX dead code; remove
  do
  {
    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsIDocumentViewer> docv;
    if ( NS_FAILED(status = CreateRDFDocument(aExtraInfo, address_of(doc), address_of(docv))) )
      break;

    if ( NS_FAILED(status = docv->LoadStart(doc)) )
      break;

    *aDocViewer = docv;
    NS_IF_ADDREF(*aDocViewer);

    nsCOMPtr<nsIStreamLoadableDocument> loader = do_QueryInterface(doc, &status);
    if ( NS_FAILED(status) )
      break;

    status = loader->LoadFromStream(aXULStream, aContainer, aCommand);
  }
  while (0);
#endif

  return status;
}

static NS_DEFINE_IID(kDocumentFactoryImplCID, NS_CONTENT_DOCUMENT_LOADER_FACTORY_CID);

static nsresult
RegisterTypes(nsIComponentManager* aCompMgr,
              nsICategoryManager* aCatMgr,
              const char* aCommand,
              nsIFile* aPath,
              const char *aLocation,
              const char *aType,
              char** aTypes)
{
  nsresult rv = NS_OK;
  while (*aTypes) {
    char contractid[500];
    char* contentType = *aTypes++;
    PR_snprintf(contractid, sizeof(contractid),
                NS_DOCUMENT_LOADER_FACTORY_CONTRACTID_PREFIX "%s;1?type=%s",
                aCommand, contentType);
#ifdef NOISY_REGISTRY
    printf("Register %s => %s\n", contractid, aPath);
#endif
    rv = aCompMgr->RegisterComponentWithType(kDocumentFactoryImplCID, "Layout",
                                         contractid, aPath, aLocation,
                                         PR_TRUE, PR_TRUE, aType);
    if (NS_FAILED(rv)) break;

    // add the MIME types layotu can handle to the handlers category.
    // this allows users of layout's viewers (the docshell for example)
    // to query the types of viewers layout can create.
    nsXPIDLCString previous;
    rv = aCatMgr->AddCategoryEntry("Gecko-Content-Viewers", contentType,
			                    contractid,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
    if (NS_FAILED(rv)) break;
  }
  return rv;
}

NS_IMETHODIMP
nsContentDLF::RegisterDocumentFactories(nsIComponentManager* aCompMgr,
                                        nsIFile* aPath,
                                        const char *aLocation,
                                        const char *aType,
                                        const nsModuleComponentInfo* aInfo)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catmgr(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  do {
    rv = RegisterTypes(aCompMgr, catmgr, "view", aPath, aLocation, aType, gHTMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(aCompMgr, catmgr, "view-source", aPath, aLocation, aType, gHTMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(aCompMgr, catmgr, "view", aPath, aLocation, aType, gXMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(aCompMgr, catmgr, "view-source", aPath, aLocation, aType, gXMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(aCompMgr, catmgr, "view", aPath, aLocation, aType, gImageTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(aCompMgr, catmgr, "view", aPath, aLocation, aType, gRDFTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(aCompMgr, catmgr, "view-source", aPath, aLocation, aType, gRDFTypes);
    if (NS_FAILED(rv))
      break;
  } while (PR_FALSE);
  return rv;
}

NS_IMETHODIMP
nsContentDLF::UnregisterDocumentFactories(nsIComponentManager* aCompMgr,
                                          nsIFile* aPath,
                                          const char* aRegistryLocation,
                                          const nsModuleComponentInfo* aInfo)
{
  // XXXwaterson seems like this leaves the registry pretty dirty.
  return aCompMgr->UnregisterComponentSpec(kDocumentFactoryImplCID, aPath);
}

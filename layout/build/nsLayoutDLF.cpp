/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIComponentManager.h"
#include "nsIDocumentLoader.h"
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIServiceManager.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsString.h"
#include "nsLayoutCID.h"
#include "nsCOMPtr.h"
#include "prprf.h"
#include "plstr.h"

#include "nsRDFCID.h"
#include "nsIXULParentDocument.h"
#include "nsIXULChildDocument.h"
#include "nsIRDFResource.h"
#include "nsIXULDocumentInfo.h"
#include "nsIXULContentSink.h"
#include "nsIStreamLoadableDocument.h"
#include "nsIDocStreamLoaderFactory.h"

// Factory code for creating variations on html documents

#undef NOISY_REGISTRY

// URL for the "user agent" style sheet
#define UA_CSS_URL "resource:/res/ua.css"

static NS_DEFINE_IID(kIDocumentLoaderFactoryIID, NS_IDOCUMENTLOADERFACTORY_IID);
static NS_DEFINE_IID(kIDocStreamLoaderFactoryIID, NS_IDOCSTREAMLOADERFACTORY_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);
static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_IID(kImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kXULDocumentCID, NS_XULDOCUMENT_CID);

extern nsresult NS_NewDocumentViewer(nsIDocumentViewer** aResult);

static char* gHTMLTypes[] = {
  "text/html",
  "application/rtf",
  "text/plain",
  0
};

static char* gXMLTypes[] = {
  "text/xml",
  "application/xml",
  0
};

static char* gRDFTypes[] = {
  "text/rdf",
  "text/xul",
  0
};

static char* gImageTypes[] = {
  "image/gif",
  "image/jpeg",
  "image/png",
  "image/x-art",
  "image/x-jg",
  0
};

// XXX temporary
static char* gPluginTypes[] = {
  "video/quicktime",
  "video/msvideo",
  "video/x-msvideo",
  "application/vnd.netfpx",
  "image/vnd.fpx",
  "model/vrml",
  "x-world/x-vrml",
  "audio/midi",
  "audio/x-midi",
  "audio/wav",
  "audio/x-wav", 
  "audio/aiff",
  "audio/x-aiff",
  "audio/basic",
  "application/x-shockwave-flash",
  "application/pdf",
  "application/npapi-test",
  0
};

extern nsresult
NS_NewPluginContentViewer(const char* aCommand,
                          nsIStreamListener** aDocListener,
                          nsIContentViewer** aDocViewer);

class nsLayoutDLF : public nsIDocumentLoaderFactory,
                    public nsIDocStreamLoaderFactory
{
public:
  nsLayoutDLF();

  NS_DECL_ISUPPORTS

  // for nsIDocumentLoaderFactory
  NS_IMETHOD CreateInstance(nsIURI* aURL,
                            const char* aContentType, 
                            const char* aCommand,
                            nsIContentViewerContainer* aContainer,
                            nsISupports* aExtraInfo,
                            nsIStreamListener** aDocListener,
                            nsIContentViewer** aDocViewer);

  NS_IMETHOD CreateInstanceForDocument(nsIContentViewerContainer* aContainer,
                                       nsIDocument* aDocument,
                                       const char *aCommand,
                                       nsIContentViewer** aDocViewerResult);

  // for nsIDocStreamLoaderFactory
  NS_METHOD CreateInstance(nsIInputStream& aInputStream,
                           const char* aContentType,
                           const char* aCommand,
                           nsIContentViewerContainer* aContainer,
                           nsISupports* aExtraInfo,
                           nsIContentViewer** aDocViewer);

  nsresult InitUAStyleSheet();

  nsresult CreateDocument(nsIURI* aURL, 
                          const char* aCommand,
                          nsIContentViewerContainer* aContainer,
                          const nsCID& aDocumentCID,
                          nsIStreamListener** aDocListener,
                          nsIContentViewer** aDocViewer);

  nsresult CreateRDFDocument(const char* aContentType, nsIURI* aURL, 
                             const char* aCommand,
                             nsIContentViewerContainer* aContainer,
                             nsISupports* aExtraInfo,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aDocViewer);

  nsresult CreateXULDocumentFromStream(nsIInputStream& aXULStream,
                                       const char* aCommand,
                                       nsIContentViewerContainer* aContainer,
                                       nsISupports* aExtraInfo,
                                       nsIContentViewer** aDocViewer);

  nsresult CreateRDFDocument(nsISupports*,
                             nsCOMPtr<nsIDocument>*,
                             nsCOMPtr<nsIDocumentViewer>*);

  static nsICSSStyleSheet* gUAStyleSheet;
};

nsresult
NS_NewLayoutDocumentLoaderFactory(nsIDocumentLoaderFactory** aResult)
{
  NS_PRECONDITION(aResult, "null OUT ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsLayoutDLF* it = new nsLayoutDLF();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDocumentLoaderFactoryIID, (void**)aResult);
}

nsICSSStyleSheet* nsLayoutDLF::gUAStyleSheet;

nsLayoutDLF::nsLayoutDLF()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ADDREF(nsLayoutDLF)
NS_IMPL_RELEASE(nsLayoutDLF)

NS_IMETHODIMP
nsLayoutDLF::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIDocumentLoaderFactoryIID)) {
    nsIDocumentLoaderFactory *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDocStreamLoaderFactoryIID)) {
    nsIDocStreamLoaderFactory *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDocumentLoaderFactory *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsLayoutDLF::CreateInstance(nsIURI* aURL, 
                            const char* aContentType, 
                            const char *aCommand,
                            nsIContentViewerContainer* aContainer,
                            nsISupports* aExtraInfo,
                            nsIStreamListener** aDocListener,
                            nsIContentViewer** aDocViewer)
{
  nsresult rv = NS_ERROR_FAILURE;


  // XXX vile hack
  if(0==PL_strcmp(aCommand,"view-source")) {
    if((0==PL_strcmp(gXMLTypes[0],aContentType)) ||
       (0==PL_strcmp(gRDFTypes[0],aContentType)) ||
       (0==PL_strcmp(gRDFTypes[1],aContentType))) {
      aContentType=gHTMLTypes[0];
    }
  }

  // Try html
  int typeIndex=0;
  while(gHTMLTypes[typeIndex]) {
    if (0== PL_strcmp(gHTMLTypes[typeIndex++], aContentType)) {
      return CreateDocument(aURL, aCommand, aContainer, kHTMLDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try XML
  typeIndex = 0;
  while(gXMLTypes[typeIndex]) {
    if (0== PL_strcmp(gXMLTypes[typeIndex++], aContentType)) {
      return CreateDocument(aURL, aCommand, aContainer, kXMLDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try RDF
  typeIndex = 0;
  while (gRDFTypes[typeIndex]) {
    if (0 == PL_strcmp(gRDFTypes[typeIndex++], aContentType)) {
      return CreateRDFDocument(aContentType, aURL, aCommand, aContainer,
                               aExtraInfo, aDocListener, aDocViewer);
    }
  }

  // Try image types
  typeIndex = 0;
  while(gImageTypes[typeIndex]) {
    if (0== PL_strcmp(gImageTypes[typeIndex++], aContentType)) {
      return CreateDocument(aURL, aCommand, aContainer, kImageDocumentCID,
                            aDocListener, aDocViewer);
    }
  }

  // Try plugin types
  typeIndex = 0;
  while(gPluginTypes[typeIndex]) {
    if (0== PL_strcmp(gPluginTypes[typeIndex++], aContentType)) {
      return NS_NewPluginContentViewer(aCommand, aDocListener, aDocViewer);
    }
  }

  return rv;
}


NS_IMETHODIMP
nsLayoutDLF::CreateInstanceForDocument(nsIContentViewerContainer* aContainer,
                                       nsIDocument* aDocument,
                                       const char *aCommand,
                                       nsIContentViewer** aDocViewerResult)
{
  nsresult rv = NS_ERROR_FAILURE;  

  // Load the UA style sheet if we haven't already done that
  if (nsnull == gUAStyleSheet) {
    InitUAStyleSheet();
  }

  do {
    nsCOMPtr<nsIDocumentViewer> docv;
    // Create the document viewer
    rv = NS_NewDocumentViewer(getter_AddRefs(docv));
    if (NS_FAILED(rv))
      break;
    docv->SetUAStyleSheet(gUAStyleSheet);

    // Bind the document to the Content Viewer
    rv = docv->BindToDocument(aDocument, aCommand);
    *aDocViewerResult = docv;
    NS_IF_ADDREF(*aDocViewerResult);
  } while (PR_FALSE);

  return rv;
}

nsresult
nsLayoutDLF::CreateDocument(nsIURI* aURL, 
                            const char* aCommand,
                            nsIContentViewerContainer* aContainer,
                            const nsCID& aDocumentCID,
                            nsIStreamListener** aDocListener,
                            nsIContentViewer** aDocViewer)
{
  nsresult rv = NS_ERROR_FAILURE;

#ifdef NOISY_CREATE_DOC
  if (nsnull != aURL) {
    nsAutoString tmp;
    aURL->ToString(tmp);
    fputs(tmp, stdout);
    printf(": creating document\n");
  }
#endif

  // Load the UA style sheet if we haven't already done that
  if (nsnull == gUAStyleSheet) {
    InitUAStyleSheet();
  }

  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIDocumentViewer> docv;
  do {
    // Create the document
    rv = nsComponentManager::CreateInstance(aDocumentCID, nsnull,
                                            kIDocumentIID,
                                            getter_AddRefs(doc));
    if (NS_FAILED(rv))
      break;

    // Create the document viewer
    rv = NS_NewDocumentViewer(getter_AddRefs(docv));
    if (NS_FAILED(rv))
      break;
    docv->SetUAStyleSheet(gUAStyleSheet);

    // Initialize the document to begin loading the data.  An
    // nsIStreamListener connected to the parser is returned in
    // aDocListener.
    rv = doc->StartDocumentLoad(aURL, aContainer, aDocListener, aCommand);
    if (NS_FAILED(rv))
      break;

    // Bind the document to the Content Viewer
    rv = docv->BindToDocument(doc, aCommand);
    *aDocViewer = docv;
    NS_IF_ADDREF(*aDocViewer);
  } while (PR_FALSE);

  return rv;
}

NS_IMETHODIMP
nsLayoutDLF::CreateInstance(nsIInputStream& aInputStream,
                            const char* aContentType,
                            const char* aCommand,
                            nsIContentViewerContainer* aContainer,
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
nsLayoutDLF::CreateRDFDocument(nsISupports* aExtraInfo,
                               nsCOMPtr<nsIDocument>* doc,
                               nsCOMPtr<nsIDocumentViewer>* docv)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIXULDocumentInfo> xulDocumentInfo;
  xulDocumentInfo = do_QueryInterface(aExtraInfo);
    
  // Load the UA style sheet if we haven't already done that
  if (nsnull == gUAStyleSheet) {
    InitUAStyleSheet();
  }

  do {
    // Create the XUL document
    rv = nsComponentManager::CreateInstance(kXULDocumentCID, nsnull,
                                            kIDocumentIID,
                                            getter_AddRefs(*doc));
    if (NS_FAILED(rv))
      break;

    /*
     * Create the image content viewer...
     */
    rv = NS_NewDocumentViewer(getter_AddRefs(*docv));
    if (NS_FAILED(rv))
      break;
    (*docv)->SetUAStyleSheet(gUAStyleSheet);

    // We are capable of being a XUL overlay. If we have extra
    // info that supports the XUL document info interface, then we'll
    // know for sure.
    if (xulDocumentInfo) {
      // We are a XUL overlay. Retrieve the parent content sink.
      nsCOMPtr<nsIXULContentSink> parentSink;
      rv = xulDocumentInfo->GetContentSink(getter_AddRefs(parentSink));
      if (NS_FAILED(rv))
        break;
      
	  // Retrieve the parent document.
	  nsCOMPtr<nsIDocument> parentDocument;
      rv = xulDocumentInfo->GetDocument(getter_AddRefs(parentDocument));
      if (NS_FAILED(rv))
        break;

      // We were able to obtain a resource and a parent document.
      parentDocument->AddSubDocument(*doc);
      (*doc)->SetParentDocument(parentDocument);

      // We need to set our content sink as well.  The
      // XUL child document interface is required to do this.
      nsCOMPtr<nsIXULChildDocument> xulChildDoc;
      xulChildDoc = do_QueryInterface(*doc);
      if (xulChildDoc) {
        xulChildDoc->SetContentSink(parentSink);
      }
    }
  } while (PR_FALSE);

  return rv;
}

// ...note, this RDF document _may_ be XUL :-)
nsresult
nsLayoutDLF::CreateRDFDocument(const char* aContentType,
                               nsIURI* aURL, 
                               const char* aCommand,
                               nsIContentViewerContainer* aContainer,
                               nsISupports* aExtraInfo,
                               nsIStreamListener** aDocListener,
                               nsIContentViewer** aDocViewer)
{
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIDocumentViewer> docv;
  nsresult rv = CreateRDFDocument(aExtraInfo, &doc, &docv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /* 
   * Initialize the document to begin loading the data...
   *
   * An nsIStreamListener connected to the parser is returned in
   * aDocListener.
   */
  rv = doc->StartDocumentLoad(aURL, aContainer, aDocListener, aCommand);
  if (NS_SUCCEEDED(rv)) {
    /*
     * Bind the document to the Content Viewer...
     */
    rv = docv->BindToDocument(doc, aCommand);
    *aDocViewer = docv;
    NS_IF_ADDREF(*aDocViewer);
  }
   
  return rv;
}

nsresult
nsLayoutDLF::CreateXULDocumentFromStream(nsIInputStream& aXULStream,
                                         const char* aCommand,
                                         nsIContentViewerContainer* aContainer,
                                         nsISupports* aExtraInfo,
                                         nsIContentViewer** aDocViewer)
{
  nsresult status = NS_OK;

  do
  {
    nsCOMPtr<nsIDocument> doc;
    nsCOMPtr<nsIDocumentViewer> docv;
    if ( NS_FAILED(status = CreateRDFDocument(aExtraInfo, &doc, &docv)) )
      break;

    if ( NS_FAILED(status = docv->BindToDocument(doc, aCommand)) )
      break;

    *aDocViewer = docv;
    NS_IF_ADDREF(*aDocViewer);

    nsCOMPtr<nsIStreamLoadableDocument> loader = do_QueryInterface(doc, &status);
    if ( NS_FAILED(status) )
      break;

    status = loader->LoadFromStream(aXULStream, aContainer, aCommand);
  }
  while (0);

  return status;
}

// XXX move this into nsDocumentViewer.cpp
nsresult
nsLayoutDLF::InitUAStyleSheet()
{
  nsresult rv = NS_OK;

  if (nsnull == gUAStyleSheet) {  // snarf one
    nsIURI* uaURL;
#ifndef NECKO
    rv = NS_NewURL(&uaURL, nsString(UA_CSS_URL)); // XXX this bites, fix it
#else
    NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIURI *uri = nsnull;
    rv = service->NewURI(UA_CSS_URL, nsnull, &uri);
    if (NS_FAILED(rv)) return rv;

    rv = uri->QueryInterface(nsIURI::GetIID(), (void**)&uaURL);
    NS_RELEASE(uri);
#endif // NECKO
    if (NS_SUCCEEDED(rv)) {
      nsICSSLoader* cssLoader;
      rv = NS_NewCSSLoader(&cssLoader);
      if (NS_SUCCEEDED(rv) && cssLoader) {
        PRBool complete;
        rv = cssLoader->LoadAgentSheet(uaURL, gUAStyleSheet, complete, nsnull, nsnull);
        NS_RELEASE(cssLoader);
        if (NS_FAILED(rv)) {
          printf("open of %s failed: error=%x\n", UA_CSS_URL, rv);
          rv = NS_ERROR_ILLEGAL_VALUE;  // XXX need a better error code here
        }
      }
      NS_RELEASE(uaURL);
    }
  }
  return rv;
}

static NS_DEFINE_IID(kDocumentFactoryImplCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);

static nsresult
RegisterTypes(nsIComponentManager* cm, const char* aCommand,
              const char* aPath, char** aTypes)
{
  nsresult rv = NS_OK;
  while (*aTypes) {
    char progid[500];
    char* contentType = *aTypes++;
    PR_snprintf(progid, sizeof(progid),
                NS_DOCUMENT_LOADER_FACTORY_PROGID_PREFIX "%s/%s",
                aCommand, contentType);
#ifdef NOISY_REGISTRY
    printf("Register %s => %s\n", progid, aPath);
#endif
    rv = cm->RegisterComponent(kDocumentFactoryImplCID,
                               "Layout", progid, aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) {
      break;
    }
  }
  return rv;
}

nsresult
NS_RegisterDocumentFactories(nsIComponentManager* cm, const char* aPath)
{
  nsresult rv;

  do {
    rv = RegisterTypes(cm, "view", aPath, gHTMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(cm, "view-source", aPath, gHTMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(cm, "view", aPath, gXMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(cm, "view-source", aPath, gXMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(cm, "view", aPath, gImageTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(cm, "view", aPath, gPluginTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(cm, "view", aPath, gRDFTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(cm, "view-source", aPath, gRDFTypes);
    if (NS_FAILED(rv))
      break;
  } while (PR_FALSE);
  return rv;
}

nsresult
NS_UnregisterDocumentFactories(nsIComponentManager* cm, const char* aPath)
{
  nsresult rv;
  rv = cm->UnregisterComponent(kDocumentFactoryImplCID, aPath);
  return rv;
}

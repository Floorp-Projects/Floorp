/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsContentDLF.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIHTMLContent.h"
#include "nsIURL.h"
#include "nsICSSStyleSheet.h"
#include "nsNodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsString.h"
#include "nsContentCID.h"
#include "prprf.h"
#include "nsNetUtil.h"
#include "nsICSSLoader.h"
#include "nsCRT.h"
#include "nsIViewSourceChannel.h"

#include "nsRDFCID.h"
#include "nsIRDFResource.h"

#include "imgILoader.h"

// plugins
#include "nsIPluginManager.h"
#include "nsIPluginHost.h"
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kPluginDocumentCID, NS_PLUGINDOCUMENT_CID);

// URL for the "user agent" style sheet
#define UA_CSS_URL "resource://gre/res/ua.css"

// Factory code for creating variations on html documents

#undef NOISY_REGISTRY

static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
#ifdef MOZ_SVG
static NS_DEFINE_IID(kSVGDocumentCID, NS_SVGDOCUMENT_CID);
#endif
static NS_DEFINE_IID(kImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kXULDocumentCID, NS_XULDOCUMENT_CID);

nsresult
NS_NewDocumentViewer(nsIDocumentViewer** aResult);

static const char* const gHTMLTypes[] = {
  "text/html",
  "text/plain",
  "text/css",
  "text/javascript",
  "application/x-javascript",
#ifdef MOZ_VIEW_SOURCE
  "application/x-view-source", //XXX I wish I could just use nsMimeTypes.h here
#endif
  "application/xhtml+xml",
  0
};
  
static const char* const gXMLTypes[] = {
  "text/xml",
  "application/xml",
  0
};

#ifdef MOZ_SVG
static char* gSVGTypes[] = {
  "image/svg+xml",
  0
};
#endif

static const char* const gRDFTypes[] = {
  "application/rdf+xml",
  "text/rdf",
  "application/vnd.mozilla.xul+xml",
  "mozilla.application/cached-xul",
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

  return CallQueryInterface(it, aResult);
}

nsContentDLF::nsContentDLF()
{
}

nsContentDLF::~nsContentDLF()
{
}

NS_IMPL_ISUPPORTS1(nsContentDLF,
                   nsIDocumentLoaderFactory)

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
  EnsureUAStyleSheet();

  // Are we viewing source?
#ifdef MOZ_VIEW_SOURCE
  nsCOMPtr<nsIViewSourceChannel> viewSourceChannel = do_QueryInterface(aChannel);
  if (viewSourceChannel)
  {
    aCommand = "view-source";

    // The parser freaks out when it sees the content-type that a
    // view-source channel normally returns.  Get the actual content
    // type of the data.  If it's known, use it; otherwise use
    // text/plain.
    nsCAutoString type;
    viewSourceChannel->GetOriginalContentType(type);
    PRBool knownType = PR_FALSE;
    PRInt32 typeIndex;
    for (typeIndex = 0; gHTMLTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gHTMLTypes[typeIndex]) &&
          !type.EqualsLiteral("application/x-view-source")) {
        knownType = PR_TRUE;
      }
    }

    for (typeIndex = 0; gXMLTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gXMLTypes[typeIndex])) {
        knownType = PR_TRUE;
      }
    }

#ifdef MOZ_SVG
    for (typeIndex = 0; gSVGTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gSVGTypes[typeIndex])) {
        knownType = PR_TRUE;
      }
    }
#endif // MOZ_SVG

    for (typeIndex = 0; gRDFTypes[typeIndex] && !knownType; ++typeIndex) {
      if (type.Equals(gRDFTypes[typeIndex])) {
        knownType = PR_TRUE;
      }
    }

    if (knownType) {
      viewSourceChannel->SetContentType(type);
    } else {
      viewSourceChannel->SetContentType(NS_LITERAL_CSTRING("text/plain"));
    }
  } else if (0 == PL_strcmp("application/x-view-source", aContentType)) {
    aChannel->SetContentType(NS_LITERAL_CSTRING("text/plain"));
    aContentType = "text/plain";
  }
#endif
  // Try html
  int typeIndex=0;
  while(gHTMLTypes[typeIndex]) {
    if (0 == PL_strcmp(gHTMLTypes[typeIndex++], aContentType)) {
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

#ifdef MOZ_SVG
  // Try SVG
  typeIndex = 0;
  while(gSVGTypes[typeIndex]) {
    if (!PL_strcmp(gSVGTypes[typeIndex++], aContentType)) {
      return CreateDocument(aCommand,
                            aChannel, aLoadGroup,
                            aContainer, kSVGDocumentCID,
                            aDocListener, aDocViewer);
    }
  }
#endif

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
  nsCOMPtr<imgILoader> loader(do_GetService("@mozilla.org/image/loader;1"));
  PRBool isReg = PR_FALSE;
  loader->SupportImageWithMimeType(aContentType, &isReg);
  if (isReg) {
    return CreateDocument(aCommand, 
                          aChannel, aLoadGroup,
                          aContainer, kImageDocumentCID,
                          aDocListener, aDocViewer);
  }

  nsCOMPtr<nsIPluginHost> ph (do_GetService(kPluginManagerCID));
  if(ph && NS_SUCCEEDED(ph->IsPluginEnabledForType(aContentType))) {
    return CreateDocument(aCommand,
                          aChannel, aLoadGroup,
                          aContainer, kPluginDocumentCID,
                          aDocListener, aDocViewer);
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

  EnsureUAStyleSheet();

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

NS_IMETHODIMP
nsContentDLF::CreateBlankDocument(nsILoadGroup *aLoadGroup, nsIDocument **aDocument)
{
  *aDocument = nsnull;

  nsresult rv = NS_ERROR_FAILURE;

  // create a new blank HTML document
  nsCOMPtr<nsIDocument> blankDoc(do_CreateInstance(kHTMLDocumentCID));

  if (blankDoc) {
    // initialize
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("about:blank"));
    if (uri) {
      blankDoc->ResetToURI(uri, aLoadGroup);
      rv = NS_OK;
    }
  }

  // add some simple content structure
  if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_FAILURE;

    nsNodeInfoManager *nim = blankDoc->NodeInfoManager();

    nsCOMPtr<nsINodeInfo> htmlNodeInfo;

    // generate an html html element
    nim->GetNodeInfo(nsHTMLAtoms::html, 0, kNameSpaceID_None,
                     getter_AddRefs(htmlNodeInfo));
    nsCOMPtr<nsIHTMLContent> htmlElement =
      NS_NewHTMLHtmlElement(htmlNodeInfo);

    // generate an html head element
    nim->GetNodeInfo(nsHTMLAtoms::head, 0, kNameSpaceID_None,
                     getter_AddRefs(htmlNodeInfo));
    nsCOMPtr<nsIHTMLContent> headElement =
      NS_NewHTMLHeadElement(htmlNodeInfo);

    // generate an html body element
    nim->GetNodeInfo(nsHTMLAtoms::body, 0, kNameSpaceID_None,
                     getter_AddRefs(htmlNodeInfo));
    nsCOMPtr<nsIHTMLContent> bodyElement =
      NS_NewHTMLBodyElement(htmlNodeInfo);

    // blat in the structure
    if (htmlElement && headElement && bodyElement) {
      htmlElement->SetDocument(blankDoc, PR_FALSE, PR_TRUE);
      blankDoc->SetRootContent(htmlElement);

      htmlElement->AppendChildTo(headElement, PR_FALSE, PR_FALSE);

      bodyElement->SetContentID(blankDoc->GetAndIncrementContentID());
      htmlElement->AppendChildTo(bodyElement, PR_FALSE, PR_FALSE);

      rv = NS_OK;
    }
  }

  // add a nice bow
  if (NS_SUCCEEDED(rv)) {
    *aDocument = blankDoc;
    NS_ADDREF(*aDocument);
  }
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
    doc = do_CreateInstance(aDocumentCID, &rv);
    if (NS_FAILED(rv))
      break;

    // Create the document viewer  XXX: could reuse document viewer here!
    rv = NS_NewDocumentViewer(getter_AddRefs(docv));
    if (NS_FAILED(rv))
      break;
    docv->SetUAStyleSheet(gUAStyleSheet);

    doc->SetContainer(aContainer);

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

// ...common work for |CreateRDFDocument| and |CreateXULDocumentFromStream|
nsresult
nsContentDLF::CreateRDFDocument(nsISupports* aExtraInfo,
                                nsCOMPtr<nsIDocument>* doc,
                                nsCOMPtr<nsIDocumentViewer>* docv)
{
  nsresult rv = NS_ERROR_FAILURE;
    
  // Create the XUL document
  *doc = do_CreateInstance(kXULDocumentCID, &rv);
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

  doc->SetContainer(aContainer);

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

static nsresult
RegisterTypes(nsICategoryManager* aCatMgr,
              const char* const* aTypes)
{
  nsresult rv = NS_OK;
  while (*aTypes) {
    const char* contentType = *aTypes++;
#ifdef NOISY_REGISTRY
    printf("Register %s => %s\n", contractid, aPath);
#endif
    // add the MIME types layout can handle to the handlers category.
    // this allows users of layout's viewers (the docshell for example)
    // to query the types of viewers layout can create.
    rv = aCatMgr->AddCategoryEntry("Gecko-Content-Viewers", contentType,
                                   "@mozilla.org/content/document-loader-factory;1",
                                   PR_TRUE, PR_TRUE, nsnull);
    if (NS_FAILED(rv)) break;
  }
  return rv;
}

static nsresult UnregisterTypes(nsICategoryManager* aCatMgr,
                                const char* const* aTypes)
{
  nsresult rv = NS_OK;
  while (*aTypes) {
    const char* contentType = *aTypes++;
    rv = aCatMgr->DeleteCategoryEntry("Gecko-Content-Viewers", contentType, PR_TRUE);
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
    rv = RegisterTypes(catmgr, gHTMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = RegisterTypes(catmgr, gXMLTypes);
    if (NS_FAILED(rv))
      break;
#ifdef MOZ_SVG
    rv = RegisterTypes(catmgr, gSVGTypes);
    if (NS_FAILED(rv))
      break;
#endif
    rv = RegisterTypes(catmgr, gRDFTypes);
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
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catmgr(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;

  do {
    rv = UnregisterTypes(catmgr, gHTMLTypes);
    if (NS_FAILED(rv))
      break;
    rv = UnregisterTypes(catmgr, gXMLTypes);
    if (NS_FAILED(rv))
      break;
#ifdef MOZ_SVG
    rv = UnregisterTypes(catmgr, gSVGTypes);
    if (NS_FAILED(rv))
      break;
#endif
    rv = UnregisterTypes(catmgr, gRDFTypes);
    if (NS_FAILED(rv))
      break;
  } while (PR_FALSE);

  return rv;
}

/* static */ nsresult
nsContentDLF::EnsureUAStyleSheet()
{
  if (gUAStyleSheet)
    return NS_OK;

  // Load the UA style sheet
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING(UA_CSS_URL));
  if (NS_FAILED(rv)) {
#ifdef DEBUG
    printf("*** open of %s failed: error=%x\n", UA_CSS_URL, rv);
#endif
    return rv;
  }
  nsCOMPtr<nsICSSLoader> cssLoader;
  NS_NewCSSLoader(getter_AddRefs(cssLoader));
  if (!cssLoader)
    return NS_ERROR_OUT_OF_MEMORY;
  rv = cssLoader->LoadAgentSheet(uri, &gUAStyleSheet);
#ifdef DEBUG
  if (NS_FAILED(rv))
    printf("*** open of %s failed: error=%x\n", UA_CSS_URL, rv);
#endif
  return rv;
}

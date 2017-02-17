/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsContentDLF.h"
#include "nsDocShell.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIComponentManager.h"
#include "nsIComponentRegistrar.h"
#include "nsIContentViewer.h"
#include "nsICategoryManager.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsNodeInfoManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsString.h"
#include "nsContentCID.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsIViewSourceChannel.h"
#include "nsContentUtils.h"
#include "imgLoader.h"
#include "nsCharsetSource.h"
#include "nsMimeTypes.h"
#include "DecoderTraits.h"


// plugins
#include "nsIPluginHost.h"
#include "nsPluginHost.h"
static NS_DEFINE_CID(kPluginDocumentCID, NS_PLUGINDOCUMENT_CID);

// Factory code for creating variations on html documents

#undef NOISY_REGISTRY

static NS_DEFINE_IID(kHTMLDocumentCID, NS_HTMLDOCUMENT_CID);
static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);
static NS_DEFINE_IID(kSVGDocumentCID, NS_SVGDOCUMENT_CID);
static NS_DEFINE_IID(kVideoDocumentCID, NS_VIDEODOCUMENT_CID);
static NS_DEFINE_IID(kImageDocumentCID, NS_IMAGEDOCUMENT_CID);
static NS_DEFINE_IID(kXULDocumentCID, NS_XULDOCUMENT_CID);

already_AddRefed<nsIContentViewer> NS_NewContentViewer();

static const char* const gHTMLTypes[] = {
  TEXT_HTML,
  VIEWSOURCE_CONTENT_TYPE,
  APPLICATION_XHTML_XML,
  APPLICATION_WAPXHTML_XML,
  0
};
  
static const char* const gXMLTypes[] = {
  TEXT_XML,
  APPLICATION_XML,
  APPLICATION_MATHML_XML,
  APPLICATION_RDF_XML,
  TEXT_RDF,
  0
};

static const char* const gSVGTypes[] = {
  IMAGE_SVG_XML,
  0
};

static const char* const gXULTypes[] = {
  TEXT_XUL,
  APPLICATION_CACHED_XUL,
  0
};

static bool
IsTypeInList(const nsACString& aType, const char* const aList[])
{
  int32_t typeIndex;
  for (typeIndex = 0; aList[typeIndex]; ++typeIndex) {
    if (aType.Equals(aList[typeIndex])) {
      return true;
    }
  }

  return false;
}

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

NS_IMPL_ISUPPORTS(nsContentDLF,
                  nsIDocumentLoaderFactory)

bool
MayUseXULXBL(nsIChannel* aChannel)
{
  nsIScriptSecurityManager *securityManager =
    nsContentUtils::GetSecurityManager();
  if (!securityManager) {
    return false;
  }

  nsCOMPtr<nsIPrincipal> principal;
  securityManager->GetChannelResultPrincipal(aChannel, getter_AddRefs(principal));
  NS_ENSURE_TRUE(principal, false);

  return nsContentUtils::AllowXULXBLForPrincipal(principal);
}

NS_IMETHODIMP
nsContentDLF::CreateInstance(const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             const nsACString& aContentType,
                             nsIDocShell* aContainer,
                             nsISupports* aExtraInfo,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aDocViewer)
{
  // Make a copy of aContentType, because we're possibly going to change it.
  nsAutoCString contentType(aContentType);

  // Are we viewing source?
  nsCOMPtr<nsIViewSourceChannel> viewSourceChannel = do_QueryInterface(aChannel);
  if (viewSourceChannel)
  {
    aCommand = "view-source";

    // The parser freaks out when it sees the content-type that a
    // view-source channel normally returns.  Get the actual content
    // type of the data.  If it's known, use it; otherwise use
    // text/plain.
    nsAutoCString type;
    mozilla::Unused << viewSourceChannel->GetOriginalContentType(type);
    bool knownType =
      (!type.EqualsLiteral(VIEWSOURCE_CONTENT_TYPE) &&
        IsTypeInList(type, gHTMLTypes)) ||
      nsContentUtils::IsPlainTextType(type) ||
      IsTypeInList(type, gXMLTypes) ||
      IsTypeInList(type, gSVGTypes) ||
      IsTypeInList(type, gXMLTypes);

    if (knownType) {
      viewSourceChannel->SetContentType(type);
    } else if (IsImageContentType(type.get())) {
      // If it's an image, we want to display it the same way we normally would.
      // Also note the lifetime of "type" allows us to safely use "get()" here.
      contentType = type;
    } else {
      viewSourceChannel->SetContentType(NS_LITERAL_CSTRING(TEXT_PLAIN));
    }
  } else if (aContentType.EqualsLiteral(VIEWSOURCE_CONTENT_TYPE)) {
    aChannel->SetContentType(NS_LITERAL_CSTRING(TEXT_PLAIN));
    contentType = TEXT_PLAIN;
  }

  // Try html or plaintext; both use the same document CID
  if (IsTypeInList(contentType, gHTMLTypes) ||
      nsContentUtils::IsPlainTextType(contentType)) {
    return CreateDocument(aCommand,
                          aChannel, aLoadGroup,
                          aContainer, kHTMLDocumentCID,
                          aDocListener, aDocViewer);
  }

  // Try XML
  if (IsTypeInList(contentType, gXMLTypes)) {
    return CreateDocument(aCommand,
                          aChannel, aLoadGroup,
                          aContainer, kXMLDocumentCID,
                          aDocListener, aDocViewer);
  }

  // Try SVG
  if (IsTypeInList(contentType, gSVGTypes)) {
    return CreateDocument(aCommand,
                          aChannel, aLoadGroup,
                          aContainer, kSVGDocumentCID,
                          aDocListener, aDocViewer);
  }

  // Try XUL
  if (IsTypeInList(contentType, gXULTypes)) {
    if (!MayUseXULXBL(aChannel)) {
      return NS_ERROR_REMOTE_XUL;
    }

    return CreateXULDocument(aCommand, aChannel, aLoadGroup, aContainer,
                             aExtraInfo, aDocListener, aDocViewer);
  }

  if (mozilla::DecoderTraits::ShouldHandleMediaType(contentType.get(),
                    /* DecoderDoctorDiagnostics* */ nullptr)) {
    return CreateDocument(aCommand,
                          aChannel, aLoadGroup,
                          aContainer, kVideoDocumentCID,
                          aDocListener, aDocViewer);
  }

  // Try image types
  if (IsImageContentType(contentType.get())) {
    return CreateDocument(aCommand,
                          aChannel, aLoadGroup,
                          aContainer, kImageDocumentCID,
                          aDocListener, aDocViewer);
  }

  RefPtr<nsPluginHost> pluginHost = nsPluginHost::GetInst();
  // Don't exclude disabled plugins, which will still trigger the "this plugin
  // is disabled" placeholder.
  if (pluginHost && pluginHost->HavePluginForType(contentType,
                                                  nsPluginHost::eExcludeNone)) {
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
                                        nsIContentViewer** aContentViewer)
{
  MOZ_ASSERT(aDocument);

  nsCOMPtr<nsIContentViewer> contentViewer = NS_NewContentViewer();

  // Bind the document to the Content Viewer
  contentViewer->LoadStart(aDocument);
  contentViewer.forget(aContentViewer);
  return NS_OK;
}

NS_IMETHODIMP
nsContentDLF::CreateBlankDocument(nsILoadGroup *aLoadGroup,
                                  nsIPrincipal* aPrincipal,
                                  nsIDocument **aDocument)
{
  *aDocument = nullptr;

  nsresult rv = NS_ERROR_FAILURE;

  // create a new blank HTML document
  nsCOMPtr<nsIDocument> blankDoc(do_CreateInstance(kHTMLDocumentCID));

  if (blankDoc) {
    // initialize
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), NS_LITERAL_CSTRING("about:blank"));
    if (uri) {
      blankDoc->ResetToURI(uri, aLoadGroup, aPrincipal);
      rv = NS_OK;
    }
  }

  // add some simple content structure
  if (NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_FAILURE;

    nsNodeInfoManager *nim = blankDoc->NodeInfoManager();

    RefPtr<mozilla::dom::NodeInfo> htmlNodeInfo;

    // generate an html html element
    htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::html, 0, kNameSpaceID_XHTML,
                                    nsIDOMNode::ELEMENT_NODE);
    nsCOMPtr<nsIContent> htmlElement =
      NS_NewHTMLHtmlElement(htmlNodeInfo.forget());

    // generate an html head element
    htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::head, 0, kNameSpaceID_XHTML,
                                    nsIDOMNode::ELEMENT_NODE);
    nsCOMPtr<nsIContent> headElement =
      NS_NewHTMLHeadElement(htmlNodeInfo.forget());

    // generate an html body elemment
    htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::body, 0, kNameSpaceID_XHTML,
                                    nsIDOMNode::ELEMENT_NODE);
    nsCOMPtr<nsIContent> bodyElement =
      NS_NewHTMLBodyElement(htmlNodeInfo.forget());

    // blat in the structure
    if (htmlElement && headElement && bodyElement) {
      NS_ASSERTION(blankDoc->GetChildCount() == 0,
                   "Shouldn't have children");
      rv = blankDoc->AppendChildTo(htmlElement, false);
      if (NS_SUCCEEDED(rv)) {
        rv = htmlElement->AppendChildTo(headElement, false);

        if (NS_SUCCEEDED(rv)) {
          // XXXbz Why not notifying here?
          htmlElement->AppendChildTo(bodyElement, false);
        }
      }
    }
  }

  // add a nice bow
  if (NS_SUCCEEDED(rv)) {
    blankDoc->SetDocumentCharacterSetSource(kCharsetFromDocTypeDefault);
    blankDoc->SetDocumentCharacterSet(NS_LITERAL_CSTRING("UTF-8"));
    
    blankDoc.forget(aDocument);
  }
  return rv;
}


nsresult
nsContentDLF::CreateDocument(const char* aCommand,
                             nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsIDocShell* aContainer,
                             const nsCID& aDocumentCID,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aContentViewer)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

#ifdef NOISY_CREATE_DOC
  if (nullptr != aURL) {
    nsAutoString tmp;
    aURL->ToString(tmp);
    fputs(NS_LossyConvertUTF16toASCII(tmp).get(), stdout);
    printf(": creating document\n");
  }
#endif

  // Create the document
  nsCOMPtr<nsIDocument> doc = do_CreateInstance(aDocumentCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the content viewer  XXX: could reuse content viewer here!
  nsCOMPtr<nsIContentViewer> contentViewer = NS_NewContentViewer();

  doc->SetContainer(static_cast<nsDocShell*>(aContainer));

  // Initialize the document to begin loading the data.  An
  // nsIStreamListener connected to the parser is returned in
  // aDocListener.
  rv = doc->StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer, aDocListener, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the document to the Content Viewer
  contentViewer->LoadStart(doc);
  contentViewer.forget(aContentViewer);
  return NS_OK;
}

nsresult
nsContentDLF::CreateXULDocument(const char* aCommand,
                                nsIChannel* aChannel,
                                nsILoadGroup* aLoadGroup,
                                nsIDocShell* aContainer,
                                nsISupports* aExtraInfo,
                                nsIStreamListener** aDocListener,
                                nsIContentViewer** aContentViewer)
{
  nsresult rv;
  nsCOMPtr<nsIDocument> doc = do_CreateInstance(kXULDocumentCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIContentViewer> contentViewer = NS_NewContentViewer();

  nsCOMPtr<nsIURI> aURL;
  rv = aChannel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

  /* 
   * Initialize the document to begin loading the data...
   *
   * An nsIStreamListener connected to the parser is returned in
   * aDocListener.
   */

  doc->SetContainer(static_cast<nsDocShell*>(aContainer));

  rv = doc->StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer, aDocListener, true);
  if (NS_FAILED(rv)) return rv;

  /*
   * Bind the document to the Content Viewer...
   */
  contentViewer->LoadStart(doc);
  contentViewer.forget(aContentViewer);
  return NS_OK;
}

bool nsContentDLF::IsImageContentType(const char* aContentType) {
  return imgLoader::SupportImageWithMimeType(aContentType);
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsContentDLF.h"

#include "mozilla/Encoding.h"

#include "nsCOMPtr.h"
#include "nsDocShell.h"
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIContentViewer.h"
#include "nsIDocumentLoaderFactory.h"
#include "mozilla/dom/Document.h"
#include "nsNodeInfoManager.h"
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

// Factory code for creating variations on html documents

#undef NOISY_REGISTRY

using mozilla::dom::Document;

already_AddRefed<nsIContentViewer> NS_NewContentViewer();

static const char* const gHTMLTypes[] = {TEXT_HTML, VIEWSOURCE_CONTENT_TYPE,
                                         APPLICATION_XHTML_XML,
                                         APPLICATION_WAPXHTML_XML, 0};

static const char* const gXMLTypes[] = {TEXT_XML,
                                        APPLICATION_XML,
                                        APPLICATION_MATHML_XML,
                                        APPLICATION_RDF_XML,
                                        TEXT_RDF,
                                        0};

static const char* const gSVGTypes[] = {IMAGE_SVG_XML, 0};

static bool IsTypeInList(const nsACString& aType, const char* const aList[]) {
  int32_t typeIndex;
  for (typeIndex = 0; aList[typeIndex]; ++typeIndex) {
    if (aType.Equals(aList[typeIndex])) {
      return true;
    }
  }

  return false;
}

nsresult NS_NewContentDocumentLoaderFactory(
    nsIDocumentLoaderFactory** aResult) {
  MOZ_ASSERT(aResult, "null OUT ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsContentDLF> it = new nsContentDLF();
  it.forget(aResult);
  return NS_OK;
}

nsContentDLF::nsContentDLF() = default;

nsContentDLF::~nsContentDLF() = default;

NS_IMPL_ISUPPORTS(nsContentDLF, nsIDocumentLoaderFactory)

NS_IMETHODIMP
nsContentDLF::CreateInstance(const char* aCommand, nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             const nsACString& aContentType,
                             nsIDocShell* aContainer, nsISupports* aExtraInfo,
                             nsIStreamListener** aDocListener,
                             nsIContentViewer** aDocViewer) {
  // Make a copy of aContentType, because we're possibly going to change it.
  nsAutoCString contentType(aContentType);

  // Are we viewing source?
  nsCOMPtr<nsIViewSourceChannel> viewSourceChannel =
      do_QueryInterface(aChannel);
  if (viewSourceChannel) {
    aCommand = "view-source";

    // The parser freaks out when it sees the content-type that a
    // view-source channel normally returns.  Get the actual content
    // type of the data.  If it's known, use it; otherwise use
    // text/plain.
    nsAutoCString type;
    mozilla::Unused << viewSourceChannel->GetOriginalContentType(type);
    bool knownType = (!type.EqualsLiteral(VIEWSOURCE_CONTENT_TYPE) &&
                      IsTypeInList(type, gHTMLTypes)) ||
                     nsContentUtils::IsPlainTextType(type) ||
                     IsTypeInList(type, gXMLTypes) ||
                     IsTypeInList(type, gSVGTypes) ||
                     IsTypeInList(type, gXMLTypes);

    if (knownType) {
      viewSourceChannel->SetContentType(type);
    } else if (IsImageContentType(type)) {
      // If it's an image, we want to display it the same way we normally would.
      contentType = type;
    } else {
      viewSourceChannel->SetContentType(nsLiteralCString(TEXT_PLAIN));
    }
  } else if (aContentType.EqualsLiteral(VIEWSOURCE_CONTENT_TYPE)) {
    aChannel->SetContentType(nsLiteralCString(TEXT_PLAIN));
    contentType = TEXT_PLAIN;
  }

  // Try html or plaintext; both use the same document CID
  if (IsTypeInList(contentType, gHTMLTypes) ||
      nsContentUtils::IsPlainTextType(contentType)) {
    return CreateDocument(
        aCommand, aChannel, aLoadGroup, aContainer,
        []() -> already_AddRefed<Document> {
          RefPtr<Document> doc;
          nsresult rv = NS_NewHTMLDocument(getter_AddRefs(doc));
          NS_ENSURE_SUCCESS(rv, nullptr);
          return doc.forget();
        },
        aDocListener, aDocViewer);
  }

  // Try XML
  if (IsTypeInList(contentType, gXMLTypes)) {
    return CreateDocument(
        aCommand, aChannel, aLoadGroup, aContainer,
        []() -> already_AddRefed<Document> {
          RefPtr<Document> doc;
          nsresult rv = NS_NewXMLDocument(getter_AddRefs(doc));
          NS_ENSURE_SUCCESS(rv, nullptr);
          return doc.forget();
        },
        aDocListener, aDocViewer);
  }

  // Try SVG
  if (IsTypeInList(contentType, gSVGTypes)) {
    return CreateDocument(
        aCommand, aChannel, aLoadGroup, aContainer,
        []() -> already_AddRefed<Document> {
          RefPtr<Document> doc;
          nsresult rv = NS_NewSVGDocument(getter_AddRefs(doc));
          NS_ENSURE_SUCCESS(rv, nullptr);
          return doc.forget();
        },
        aDocListener, aDocViewer);
  }

  if (mozilla::DecoderTraits::ShouldHandleMediaType(
          contentType.get(),
          /* DecoderDoctorDiagnostics* */ nullptr)) {
    return CreateDocument(
        aCommand, aChannel, aLoadGroup, aContainer,
        []() -> already_AddRefed<Document> {
          RefPtr<Document> doc;
          nsresult rv = NS_NewVideoDocument(getter_AddRefs(doc));
          NS_ENSURE_SUCCESS(rv, nullptr);
          return doc.forget();
        },
        aDocListener, aDocViewer);
  }

  // Try image types
  if (IsImageContentType(contentType)) {
    return CreateDocument(
        aCommand, aChannel, aLoadGroup, aContainer,
        []() -> already_AddRefed<Document> {
          RefPtr<Document> doc;
          nsresult rv = NS_NewImageDocument(getter_AddRefs(doc));
          NS_ENSURE_SUCCESS(rv, nullptr);
          return doc.forget();
        },
        aDocListener, aDocViewer);
  }

  // If we get here, then we weren't able to create anything. Sorry!
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsContentDLF::CreateInstanceForDocument(nsISupports* aContainer,
                                        Document* aDocument,
                                        const char* aCommand,
                                        nsIContentViewer** aContentViewer) {
  MOZ_ASSERT(aDocument);

  nsCOMPtr<nsIContentViewer> contentViewer = NS_NewContentViewer();

  // Bind the document to the Content Viewer
  contentViewer->LoadStart(aDocument);
  contentViewer.forget(aContentViewer);
  return NS_OK;
}

/* static */
already_AddRefed<Document> nsContentDLF::CreateBlankDocument(
    nsILoadGroup* aLoadGroup, nsIPrincipal* aPrincipal,
    nsIPrincipal* aPartitionedPrincipal, nsDocShell* aContainer) {
  // create a new blank HTML document
  RefPtr<Document> blankDoc;
  mozilla::Unused << NS_NewHTMLDocument(getter_AddRefs(blankDoc));

  if (!blankDoc) {
    return nullptr;
  }

  // initialize
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), "about:blank"_ns);
  if (!uri) {
    return nullptr;
  }
  blankDoc->ResetToURI(uri, aLoadGroup, aPrincipal, aPartitionedPrincipal);
  blankDoc->SetContainer(aContainer);

  // add some simple content structure
  nsNodeInfoManager* nim = blankDoc->NodeInfoManager();

  RefPtr<mozilla::dom::NodeInfo> htmlNodeInfo;

  // generate an html html element
  htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::html, 0, kNameSpaceID_XHTML,
                                  nsINode::ELEMENT_NODE);
  nsCOMPtr<nsIContent> htmlElement =
      NS_NewHTMLHtmlElement(htmlNodeInfo.forget());

  // generate an html head element
  htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::head, 0, kNameSpaceID_XHTML,
                                  nsINode::ELEMENT_NODE);
  nsCOMPtr<nsIContent> headElement =
      NS_NewHTMLHeadElement(htmlNodeInfo.forget());

  // generate an html body elemment
  htmlNodeInfo = nim->GetNodeInfo(nsGkAtoms::body, 0, kNameSpaceID_XHTML,
                                  nsINode::ELEMENT_NODE);
  nsCOMPtr<nsIContent> bodyElement =
      NS_NewHTMLBodyElement(htmlNodeInfo.forget());

  // blat in the structure
  NS_ASSERTION(blankDoc->GetChildCount() == 0, "Shouldn't have children");
  if (!htmlElement || !headElement || !bodyElement ||
      NS_FAILED(blankDoc->AppendChildTo(htmlElement, false)) ||
      NS_FAILED(htmlElement->AppendChildTo(headElement, false)) ||
      // XXXbz Why not notifying here?
      NS_FAILED(htmlElement->AppendChildTo(bodyElement, false))) {
    return nullptr;
  }

  // add a nice bow
  blankDoc->SetDocumentCharacterSetSource(kCharsetFromDocTypeDefault);
  blankDoc->SetDocumentCharacterSet(UTF_8_ENCODING);
  return blankDoc.forget();
}

nsresult nsContentDLF::CreateDocument(
    const char* aCommand, nsIChannel* aChannel, nsILoadGroup* aLoadGroup,
    nsIDocShell* aContainer, nsContentDLF::DocumentCreator aDocumentCreator,
    nsIStreamListener** aDocListener, nsIContentViewer** aContentViewer) {
  MOZ_ASSERT(aDocumentCreator);

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
  RefPtr<Document> doc = aDocumentCreator();
  NS_ENSURE_TRUE(doc, NS_ERROR_FAILURE);

  // Create the content viewer  XXX: could reuse content viewer here!
  nsCOMPtr<nsIContentViewer> contentViewer = NS_NewContentViewer();

  doc->SetContainer(static_cast<nsDocShell*>(aContainer));

  // Initialize the document to begin loading the data.  An
  // nsIStreamListener connected to the parser is returned in
  // aDocListener.
  rv = doc->StartDocumentLoad(aCommand, aChannel, aLoadGroup, aContainer,
                              aDocListener, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the document to the Content Viewer
  contentViewer->LoadStart(doc);
  contentViewer.forget(aContentViewer);
  return NS_OK;
}

bool nsContentDLF::IsImageContentType(const nsACString& aContentType) {
  return imgLoader::SupportImageWithMimeType(aContentType);
}

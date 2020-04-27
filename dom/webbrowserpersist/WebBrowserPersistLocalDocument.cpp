/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebBrowserPersistLocalDocument.h"
#include "WebBrowserPersistDocumentParent.h"

#include "mozilla/dom/Attr.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLAnchorElement.h"
#include "mozilla/dom/HTMLAreaElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLLinkElement.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/dom/HTMLSharedElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/NodeFilterBinding.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/TreeWalker.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsContentCID.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMAttributeMap.h"
#include "nsFrameLoader.h"
#include "nsGlobalWindowOuter.h"
#include "nsIContent.h"
#include "nsIDOMWindowUtils.h"
#include "mozilla/dom/Document.h"
#include "nsIDocumentEncoder.h"
#include "nsILoadContext.h"
#include "nsIProtocolHandler.h"
#include "nsISHEntry.h"
#include "nsIURIMutator.h"
#include "nsIWebBrowserPersist.h"
#include "nsIWebNavigation.h"
#include "nsIWebPageDescriptor.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebBrowserPersistLocalDocument)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebBrowserPersistLocalDocument)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebBrowserPersistLocalDocument)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersistDocument)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(WebBrowserPersistLocalDocument, mDocument)

WebBrowserPersistLocalDocument::WebBrowserPersistLocalDocument(
    dom::Document* aDocument)
    : mDocument(aDocument), mPersistFlags(0) {
  MOZ_ASSERT(mDocument);
}

WebBrowserPersistLocalDocument::~WebBrowserPersistLocalDocument() = default;

NS_IMETHODIMP
WebBrowserPersistLocalDocument::SetPersistFlags(uint32_t aFlags) {
  mPersistFlags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetPersistFlags(uint32_t* aFlags) {
  *aFlags = mPersistFlags;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetIsPrivate(bool* aIsPrivate) {
  nsCOMPtr<nsILoadContext> privacyContext = mDocument->GetLoadContext();
  *aIsPrivate = privacyContext && privacyContext->UsePrivateBrowsing();
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetDocumentURI(nsACString& aURISpec) {
  nsCOMPtr<nsIURI> uri = mDocument->GetDocumentURI();
  if (!uri) {
    return NS_ERROR_UNEXPECTED;
  }
  return uri->GetSpec(aURISpec);
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetBaseURI(nsACString& aURISpec) {
  nsCOMPtr<nsIURI> uri = GetBaseURI();
  if (!uri) {
    return NS_ERROR_UNEXPECTED;
  }
  return uri->GetSpec(aURISpec);
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetContentType(nsACString& aContentType) {
  nsAutoString utf16Type;
  mDocument->GetContentType(utf16Type);
  CopyUTF16toUTF8(utf16Type, aContentType);
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetCharacterSet(nsACString& aCharSet) {
  GetCharacterSet()->Name(aCharSet);
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetTitle(nsAString& aTitle) {
  nsAutoString titleBuffer;
  mDocument->GetTitle(titleBuffer);
  aTitle = titleBuffer;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetReferrerInfo(
    nsIReferrerInfo** aReferrerInfo) {
  *aReferrerInfo = mDocument->GetReferrerInfo();
  NS_IF_ADDREF(*aReferrerInfo);
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetContentDisposition(nsAString& aCD) {
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  if (NS_WARN_IF(!window)) {
    aCD.SetIsVoid(true);
    return NS_OK;
  }
  nsCOMPtr<nsIDOMWindowUtils> utils =
      nsGlobalWindowOuter::Cast(window)->WindowUtils();
  nsresult rv =
      utils->GetDocumentMetadata(NS_LITERAL_STRING("content-disposition"), aCD);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aCD.SetIsVoid(true);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetCacheKey(uint32_t* aKey) {
  *aKey = 0;
  nsCOMPtr<nsISHEntry> history = GetHistory();
  if (history) {
    history->GetCacheKey(aKey);
  }
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetPostData(nsIInputStream** aStream) {
  nsCOMPtr<nsISHEntry> history = GetHistory();
  if (!history) {
    *aStream = nullptr;
    return NS_OK;
  }
  return history->GetPostData(aStream);
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::GetPrincipal(nsIPrincipal** aPrincipal) {
  nsCOMPtr<nsIPrincipal> nodePrincipal = mDocument->NodePrincipal();
  nodePrincipal.forget(aPrincipal);
  return NS_OK;
}

already_AddRefed<nsISHEntry> WebBrowserPersistLocalDocument::GetHistory() {
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  if (NS_WARN_IF(!window)) {
    return nullptr;
  }
  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(window);
  if (NS_WARN_IF(!webNav)) {
    return nullptr;
  }
  nsCOMPtr<nsIWebPageDescriptor> desc = do_QueryInterface(webNav);
  if (NS_WARN_IF(!desc)) {
    return nullptr;
  }
  nsCOMPtr<nsISupports> curDesc;
  nsresult rv = desc->GetCurrentDescriptor(getter_AddRefs(curDesc));
  // This can fail if, e.g., the document is a Print Preview.
  if (NS_FAILED(rv) || NS_WARN_IF(!curDesc)) {
    return nullptr;
  }
  nsCOMPtr<nsISHEntry> history = do_QueryInterface(curDesc);
  return history.forget();
}

NotNull<const Encoding*> WebBrowserPersistLocalDocument::GetCharacterSet()
    const {
  return mDocument->GetDocumentCharacterSet();
}

uint32_t WebBrowserPersistLocalDocument::GetPersistFlags() const {
  return mPersistFlags;
}

nsIURI* WebBrowserPersistLocalDocument::GetBaseURI() const {
  return mDocument->GetBaseURI();
}

namespace {

// Helper class for ReadResources().
class ResourceReader final : public nsIWebBrowserPersistDocumentReceiver {
 public:
  ResourceReader(WebBrowserPersistLocalDocument* aParent,
                 nsIWebBrowserPersistResourceVisitor* aVisitor);
  nsresult OnWalkDOMNode(nsINode* aNode);

  // This is called both to indicate the end of the document walk
  // and when a subdocument is (maybe asynchronously) sent to the
  // visitor.  The call to EndVisit needs to happen after both of
  // those have finished.
  void DocumentDone(nsresult aStatus);

  NS_DECL_NSIWEBBROWSERPERSISTDOCUMENTRECEIVER
  NS_DECL_ISUPPORTS

 private:
  RefPtr<WebBrowserPersistLocalDocument> mParent;
  nsCOMPtr<nsIWebBrowserPersistResourceVisitor> mVisitor;
  nsCOMPtr<nsIURI> mCurrentBaseURI;
  uint32_t mPersistFlags;

  // The number of DocumentDone calls after which EndVisit will be
  // called on the visitor.  Counts the main document if it's still
  // being walked and any outstanding asynchronous subdocument
  // StartPersistence calls.
  size_t mOutstandingDocuments;
  // Collects the status parameters to DocumentDone calls.
  nsresult mEndStatus;

  nsresult OnWalkURI(const nsACString& aURISpec,
                     nsContentPolicyType aContentPolicyType);
  nsresult OnWalkURI(nsIURI* aURI, nsContentPolicyType aContentPolicyType);
  nsresult OnWalkAttribute(dom::Element* aElement,
                           nsContentPolicyType aContentPolicyType,
                           const char* aAttribute,
                           const char* aNamespaceURI = "");
  nsresult OnWalkSubframe(nsINode* aNode);

  ~ResourceReader();

  using IWBP = nsIWebBrowserPersist;
};

NS_IMPL_ISUPPORTS(ResourceReader, nsIWebBrowserPersistDocumentReceiver)

ResourceReader::ResourceReader(WebBrowserPersistLocalDocument* aParent,
                               nsIWebBrowserPersistResourceVisitor* aVisitor)
    : mParent(aParent),
      mVisitor(aVisitor),
      mCurrentBaseURI(aParent->GetBaseURI()),
      mPersistFlags(aParent->GetPersistFlags()),
      mOutstandingDocuments(1),
      mEndStatus(NS_OK) {
  MOZ_ASSERT(mCurrentBaseURI);
}

ResourceReader::~ResourceReader() { MOZ_ASSERT(mOutstandingDocuments == 0); }

void ResourceReader::DocumentDone(nsresult aStatus) {
  MOZ_ASSERT(mOutstandingDocuments > 0);
  if (NS_SUCCEEDED(mEndStatus)) {
    mEndStatus = aStatus;
  }
  if (--mOutstandingDocuments == 0) {
    mVisitor->EndVisit(mParent, mEndStatus);
  }
}

nsresult ResourceReader::OnWalkSubframe(nsINode* aNode) {
  RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(aNode);
  NS_ENSURE_STATE(loaderOwner);
  RefPtr<nsFrameLoader> loader = loaderOwner->GetFrameLoader();
  NS_ENSURE_STATE(loader);

  RefPtr<dom::BrowsingContext> context = loader->GetBrowsingContext();
  NS_ENSURE_STATE(context);

  if (loader->IsRemoteFrame()) {
    mVisitor->VisitBrowsingContext(mParent, context);
    return NS_OK;
  }

  ++mOutstandingDocuments;
  ErrorResult err;
  loader->StartPersistence(context, this, err);
  nsresult rv = err.StealNSResult();
  if (NS_FAILED(rv)) {
    if (rv == NS_ERROR_NO_CONTENT) {
      // Just ignore frames with no content document.
      rv = NS_OK;
    }
    // StartPersistence won't eventually call this if it failed,
    // so this does so (to keep mOutstandingDocuments correct).
    DocumentDone(rv);
  }
  return rv;
}

NS_IMETHODIMP
ResourceReader::OnDocumentReady(nsIWebBrowserPersistDocument* aDocument) {
  mVisitor->VisitDocument(mParent, aDocument);
  DocumentDone(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
ResourceReader::OnError(nsresult aFailure) {
  DocumentDone(aFailure);
  return NS_OK;
}

nsresult ResourceReader::OnWalkURI(nsIURI* aURI,
                                   nsContentPolicyType aContentPolicyType) {
  // Test if this URI should be persisted. By default
  // we should assume the URI  is persistable.
  bool doNotPersistURI;
  nsresult rv = NS_URIChainHasFlags(
      aURI, nsIProtocolHandler::URI_NON_PERSISTABLE, &doNotPersistURI);
  if (NS_SUCCEEDED(rv) && doNotPersistURI) {
    return NS_OK;
  }

  nsAutoCString stringURI;
  rv = aURI->GetSpec(stringURI);
  NS_ENSURE_SUCCESS(rv, rv);
  return mVisitor->VisitResource(mParent, stringURI, aContentPolicyType);
}

nsresult ResourceReader::OnWalkURI(const nsACString& aURISpec,
                                   nsContentPolicyType aContentPolicyType) {
  nsresult rv;
  nsCOMPtr<nsIURI> uri;

  rv = NS_NewURI(getter_AddRefs(uri), aURISpec, mParent->GetCharacterSet(),
                 mCurrentBaseURI);
  if (NS_FAILED(rv)) {
    // We don't want to break saving a page in case of a malformed URI.
    return NS_OK;
  }
  return OnWalkURI(uri, aContentPolicyType);
}

static void ExtractAttribute(dom::Element* aElement, const char* aAttribute,
                             const char* aNamespaceURI, nsCString& aValue) {
  // Find the named URI attribute on the (element) node and store
  // a reference to the URI that maps onto a local file name

  RefPtr<nsDOMAttributeMap> attrMap = aElement->Attributes();

  NS_ConvertASCIItoUTF16 namespaceURI(aNamespaceURI);
  NS_ConvertASCIItoUTF16 attribute(aAttribute);
  RefPtr<dom::Attr> attr = attrMap->GetNamedItemNS(namespaceURI, attribute);
  if (attr) {
    nsAutoString value;
    attr->GetValue(value);
    CopyUTF16toUTF8(value, aValue);
  } else {
    aValue.Truncate();
  }
}

nsresult ResourceReader::OnWalkAttribute(dom::Element* aElement,
                                         nsContentPolicyType aContentPolicyType,
                                         const char* aAttribute,
                                         const char* aNamespaceURI) {
  nsAutoCString uriSpec;
  ExtractAttribute(aElement, aAttribute, aNamespaceURI, uriSpec);
  if (uriSpec.IsEmpty()) {
    return NS_OK;
  }
  return OnWalkURI(uriSpec, aContentPolicyType);
}

static nsresult GetXMLStyleSheetLink(dom::ProcessingInstruction* aPI,
                                     nsAString& aHref) {
  nsAutoString data;
  aPI->GetData(data);

  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::href, aHref);
  return NS_OK;
}

nsresult ResourceReader::OnWalkDOMNode(nsINode* aNode) {
  // Fixup xml-stylesheet processing instructions
  if (auto nodeAsPI = dom::ProcessingInstruction::FromNode(aNode)) {
    nsAutoString target;
    nodeAsPI->GetTarget(target);
    if (target.EqualsLiteral("xml-stylesheet")) {
      nsAutoString href;
      GetXMLStyleSheetLink(nodeAsPI, href);
      if (!href.IsEmpty()) {
        return OnWalkURI(NS_ConvertUTF16toUTF8(href),
                         nsIContentPolicy::TYPE_STYLESHEET);
      }
    }
    return NS_OK;
  }

  // Test the node to see if it's an image, frame, iframe, css, js
  if (aNode->IsHTMLElement(nsGkAtoms::img)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_IMAGE,
                           "src");
  }

  if (aNode->IsSVGElement(nsGkAtoms::img)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_IMAGE,
                           "href", "http://www.w3.org/1999/xlink");
  }

  if (aNode->IsAnyOfHTMLElements(nsGkAtoms::audio, nsGkAtoms::video)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_MEDIA,
                           "src");
  }

  if (aNode->IsHTMLElement(nsGkAtoms::source)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_MEDIA,
                           "src");
  }

  if (aNode->IsHTMLElement(nsGkAtoms::body)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_IMAGE,
                           "background");
  }

  if (aNode->IsHTMLElement(nsGkAtoms::table)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_IMAGE,
                           "background");
  }

  if (aNode->IsHTMLElement(nsGkAtoms::tr)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_IMAGE,
                           "background");
  }

  if (aNode->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_IMAGE,
                           "background");
  }

  if (aNode->IsHTMLElement(nsGkAtoms::script)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_SCRIPT,
                           "src");
  }

  if (aNode->IsSVGElement(nsGkAtoms::script)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_SCRIPT,
                           "href", "http://www.w3.org/1999/xlink");
  }

  if (aNode->IsHTMLElement(nsGkAtoms::embed)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_OBJECT,
                           "src");
  }

  if (aNode->IsHTMLElement(nsGkAtoms::object)) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_OBJECT,
                           "data");
  }

  if (auto nodeAsLink = dom::HTMLLinkElement::FromNode(aNode)) {
    // Test if the link has a rel value indicating it to be a stylesheet
    nsAutoString linkRel;
    nodeAsLink->GetRel(linkRel);
    if (!linkRel.IsEmpty()) {
      nsReadingIterator<char16_t> start;
      nsReadingIterator<char16_t> end;
      nsReadingIterator<char16_t> current;

      linkRel.BeginReading(start);
      linkRel.EndReading(end);

      // Walk through space delimited string looking for "stylesheet"
      for (current = start; current != end; ++current) {
        // Ignore whitespace
        if (nsCRT::IsAsciiSpace(*current)) {
          continue;
        }

        // Grab the next space delimited word
        nsReadingIterator<char16_t> startWord = current;
        do {
          ++current;
        } while (current != end && !nsCRT::IsAsciiSpace(*current));

        // Store the link for fix up if it says "stylesheet"
        if (Substring(startWord, current)
                .LowerCaseEqualsLiteral("stylesheet")) {
          OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_STYLESHEET,
                          "href");
          return NS_OK;
        }
        if (current == end) {
          break;
        }
      }
    }
    return NS_OK;
  }

  if (aNode->IsHTMLElement(nsGkAtoms::frame)) {
    return OnWalkSubframe(aNode);
  }

  if (aNode->IsHTMLElement(nsGkAtoms::iframe) &&
      !(mPersistFlags & IWBP::PERSIST_FLAGS_IGNORE_IFRAMES)) {
    return OnWalkSubframe(aNode);
  }

  auto nodeAsInput = dom::HTMLInputElement::FromNode(aNode);
  if (nodeAsInput) {
    return OnWalkAttribute(aNode->AsElement(), nsIContentPolicy::TYPE_IMAGE,
                           "src");
  }

  return NS_OK;
}

// Helper class for node rewriting in writeContent().
class PersistNodeFixup final : public nsIDocumentEncoderNodeFixup {
 public:
  PersistNodeFixup(WebBrowserPersistLocalDocument* aParent,
                   nsIWebBrowserPersistURIMap* aMap, nsIURI* aTargetURI);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOCUMENTENCODERNODEFIXUP
 private:
  virtual ~PersistNodeFixup() = default;
  RefPtr<WebBrowserPersistLocalDocument> mParent;
  nsClassHashtable<nsCStringHashKey, nsCString> mMap;
  nsCOMPtr<nsIURI> mCurrentBaseURI;
  nsCOMPtr<nsIURI> mTargetBaseURI;

  bool IsFlagSet(uint32_t aFlag) const {
    return mParent->GetPersistFlags() & aFlag;
  }

  nsresult GetNodeToFixup(nsINode* aNodeIn, nsINode** aNodeOut);
  nsresult FixupURI(nsAString& aURI);
  nsresult FixupAttribute(nsINode* aNode, const char* aAttribute,
                          const char* aNamespaceURI = "");
  nsresult FixupAnchor(nsINode* aNode);
  nsresult FixupXMLStyleSheetLink(dom::ProcessingInstruction* aPI,
                                  const nsAString& aHref);

  using IWBP = nsIWebBrowserPersist;
};

NS_IMPL_ISUPPORTS(PersistNodeFixup, nsIDocumentEncoderNodeFixup)

PersistNodeFixup::PersistNodeFixup(WebBrowserPersistLocalDocument* aParent,
                                   nsIWebBrowserPersistURIMap* aMap,
                                   nsIURI* aTargetURI)
    : mParent(aParent),
      mCurrentBaseURI(aParent->GetBaseURI()),
      mTargetBaseURI(aTargetURI) {
  if (aMap) {
    uint32_t mapSize;
    nsresult rv = aMap->GetNumMappedURIs(&mapSize);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    NS_ENSURE_SUCCESS_VOID(rv);
    for (uint32_t i = 0; i < mapSize; ++i) {
      nsAutoCString urlFrom;
      auto* urlTo = new nsCString();

      rv = aMap->GetURIMapping(i, urlFrom, *urlTo);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        mMap.Put(urlFrom, urlTo);
      }
    }
  }
}

nsresult PersistNodeFixup::GetNodeToFixup(nsINode* aNodeIn,
                                          nsINode** aNodeOut) {
  // Avoid mixups in FixupNode that could leak objects; this goes
  // against the usual out parameter convention, but it's a private
  // method so shouldn't be a problem.
  MOZ_ASSERT(!*aNodeOut);

  if (!IsFlagSet(IWBP::PERSIST_FLAGS_FIXUP_ORIGINAL_DOM)) {
    ErrorResult rv;
    *aNodeOut = aNodeIn->CloneNode(false, rv).take();
    return rv.StealNSResult();
  }

  NS_ADDREF(*aNodeOut = aNodeIn);
  return NS_OK;
}

nsresult PersistNodeFixup::FixupURI(nsAString& aURI) {
  // get the current location of the file (absolutized)
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURI, mParent->GetCharacterSet(),
                          mCurrentBaseURI);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  const nsCString* replacement = mMap.Get(spec);
  if (!replacement) {
    // Note that most callers ignore this "failure".
    return NS_ERROR_FAILURE;
  }
  if (!replacement->IsEmpty()) {
    aURI = NS_ConvertUTF8toUTF16(*replacement);
  }
  return NS_OK;
}

nsresult PersistNodeFixup::FixupAttribute(nsINode* aNode,
                                          const char* aAttribute,
                                          const char* aNamespaceURI) {
  MOZ_ASSERT(aNode->IsElement());
  dom::Element* element = aNode->AsElement();

  RefPtr<nsDOMAttributeMap> attrMap = element->Attributes();

  NS_ConvertASCIItoUTF16 attribute(aAttribute);
  NS_ConvertASCIItoUTF16 namespaceURI(aNamespaceURI);
  RefPtr<dom::Attr> attr = attrMap->GetNamedItemNS(namespaceURI, attribute);
  nsresult rv = NS_OK;
  if (attr) {
    nsString uri;
    attr->GetValue(uri);
    rv = FixupURI(uri);
    if (NS_SUCCEEDED(rv)) {
      attr->SetValue(uri, IgnoreErrors());
    }
  }

  return rv;
}

nsresult PersistNodeFixup::FixupAnchor(nsINode* aNode) {
  if (IsFlagSet(IWBP::PERSIST_FLAGS_DONT_FIXUP_LINKS)) {
    return NS_OK;
  }

  MOZ_ASSERT(aNode->IsElement());
  dom::Element* element = aNode->AsElement();

  RefPtr<nsDOMAttributeMap> attrMap = element->Attributes();

  // Make all anchor links absolute so they point off onto the Internet
  nsString attribute(NS_LITERAL_STRING("href"));
  RefPtr<dom::Attr> attr = attrMap->GetNamedItem(attribute);
  if (attr) {
    nsString oldValue;
    attr->GetValue(oldValue);
    NS_ConvertUTF16toUTF8 oldCValue(oldValue);

    // Skip empty values and self-referencing bookmarks
    if (oldCValue.IsEmpty() || oldCValue.CharAt(0) == '#') {
      return NS_OK;
    }

    // if saving file to same location, we don't need to do any fixup
    bool isEqual;
    if (mTargetBaseURI &&
        NS_SUCCEEDED(mCurrentBaseURI->Equals(mTargetBaseURI, &isEqual)) &&
        isEqual) {
      return NS_OK;
    }

    nsCOMPtr<nsIURI> relativeURI;
    relativeURI = IsFlagSet(IWBP::PERSIST_FLAGS_FIXUP_LINKS_TO_DESTINATION)
                      ? mTargetBaseURI
                      : mCurrentBaseURI;
    // Make a new URI to replace the current one
    nsCOMPtr<nsIURI> newURI;
    nsresult rv = NS_NewURI(getter_AddRefs(newURI), oldCValue,
                            mParent->GetCharacterSet(), relativeURI);
    if (NS_SUCCEEDED(rv) && newURI) {
      Unused
          << NS_MutateURI(newURI).SetUserPass(EmptyCString()).Finalize(newURI);
      nsAutoCString uriSpec;
      rv = newURI->GetSpec(uriSpec);
      NS_ENSURE_SUCCESS(rv, rv);
      attr->SetValue(NS_ConvertUTF8toUTF16(uriSpec), IgnoreErrors());
    }
  }

  return NS_OK;
}

static void AppendXMLAttr(const nsAString& key, const nsAString& aValue,
                          nsAString& aBuffer) {
  if (!aBuffer.IsEmpty()) {
    aBuffer.Append(' ');
  }
  aBuffer.Append(key);
  aBuffer.AppendLiteral(R"(=")");
  for (size_t i = 0; i < aValue.Length(); ++i) {
    switch (aValue[i]) {
      case '&':
        aBuffer.AppendLiteral("&amp;");
        break;
      case '<':
        aBuffer.AppendLiteral("&lt;");
        break;
      case '>':
        aBuffer.AppendLiteral("&gt;");
        break;
      case '"':
        aBuffer.AppendLiteral("&quot;");
        break;
      default:
        aBuffer.Append(aValue[i]);
        break;
    }
  }
  aBuffer.Append('"');
}

nsresult PersistNodeFixup::FixupXMLStyleSheetLink(
    dom::ProcessingInstruction* aPI, const nsAString& aHref) {
  NS_ENSURE_ARG_POINTER(aPI);

  nsAutoString data;
  aPI->GetData(data);

  nsAutoString href;
  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::href, href);

  // Construct and set a new data value for the xml-stylesheet
  if (!aHref.IsEmpty() && !href.IsEmpty()) {
    nsAutoString alternate;
    nsAutoString charset;
    nsAutoString title;
    nsAutoString type;
    nsAutoString media;

    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::alternate,
                                            alternate);
    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::charset, charset);
    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::title, title);
    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::type, type);
    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::media, media);

    nsAutoString newData;
    AppendXMLAttr(NS_LITERAL_STRING("href"), aHref, newData);
    if (!title.IsEmpty()) {
      AppendXMLAttr(NS_LITERAL_STRING("title"), title, newData);
    }
    if (!media.IsEmpty()) {
      AppendXMLAttr(NS_LITERAL_STRING("media"), media, newData);
    }
    if (!type.IsEmpty()) {
      AppendXMLAttr(NS_LITERAL_STRING("type"), type, newData);
    }
    if (!charset.IsEmpty()) {
      AppendXMLAttr(NS_LITERAL_STRING("charset"), charset, newData);
    }
    if (!alternate.IsEmpty()) {
      AppendXMLAttr(NS_LITERAL_STRING("alternate"), alternate, newData);
    }
    aPI->SetData(newData, IgnoreErrors());
  }

  return NS_OK;
}

NS_IMETHODIMP
PersistNodeFixup::FixupNode(nsINode* aNodeIn, bool* aSerializeCloneKids,
                            nsINode** aNodeOut) {
  *aNodeOut = nullptr;
  *aSerializeCloneKids = false;

  uint16_t type = aNodeIn->NodeType();
  if (type != nsINode::ELEMENT_NODE &&
      type != nsINode::PROCESSING_INSTRUCTION_NODE) {
    return NS_OK;
  }

  MOZ_ASSERT(aNodeIn->IsContent());

  // Fixup xml-stylesheet processing instructions
  if (auto nodeAsPI = dom::ProcessingInstruction::FromNode(aNodeIn)) {
    nsAutoString target;
    nodeAsPI->GetTarget(target);
    if (target.EqualsLiteral("xml-stylesheet")) {
      nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
      if (NS_SUCCEEDED(rv) && *aNodeOut) {
        MOZ_ASSERT((*aNodeOut)->IsProcessingInstruction());
        auto nodeAsPI = static_cast<dom::ProcessingInstruction*>(*aNodeOut);
        nsAutoString href;
        GetXMLStyleSheetLink(nodeAsPI, href);
        if (!href.IsEmpty()) {
          FixupURI(href);
          FixupXMLStyleSheetLink(nodeAsPI, href);
        }
      }
    }
    return NS_OK;
  }

  nsCOMPtr<nsIContent> content = do_QueryInterface(aNodeIn);
  if (!content) {
    return NS_OK;
  }

  // BASE elements are replaced by a comment so relative links are not hosed.
  if (!IsFlagSet(IWBP::PERSIST_FLAGS_NO_BASE_TAG_MODIFICATIONS) &&
      content->IsHTMLElement(nsGkAtoms::base)) {
    // Base uses HTMLSharedElement, which would be awkward to implement
    // FromContent on, since it represents multiple elements. Since we've
    // already checked IsHTMLElement here, just cast as we were doing.
    auto* base = static_cast<dom::HTMLSharedElement*>(content.get());
    dom::Document* ownerDoc = base->OwnerDoc();

    nsAutoString href;
    base->GetHref(href);  // Doesn't matter if this fails
    nsAutoString commentText;
    commentText.AssignLiteral(" base ");
    if (!href.IsEmpty()) {
      commentText +=
          NS_LITERAL_STRING("href=\"") + href + NS_LITERAL_STRING("\" ");
    }
    *aNodeOut = ownerDoc->CreateComment(commentText).take();
    return NS_OK;
  }

  // Fix up href and file links in the elements
  RefPtr<dom::HTMLAnchorElement> nodeAsAnchor =
      dom::HTMLAnchorElement::FromNode(content);
  if (nodeAsAnchor) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAnchor(*aNodeOut);
    }
    return rv;
  }

  RefPtr<dom::HTMLAreaElement> nodeAsArea =
      dom::HTMLAreaElement::FromNode(content);
  if (nodeAsArea) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAnchor(*aNodeOut);
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::body)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "background");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::table)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "background");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::tr)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "background");
    }
    return rv;
  }

  if (content->IsAnyOfHTMLElements(nsGkAtoms::td, nsGkAtoms::th)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "background");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::img)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      // Disable image loads
      nsCOMPtr<nsIImageLoadingContent> imgCon = do_QueryInterface(*aNodeOut);
      if (imgCon) {
        imgCon->SetLoadingEnabled(false);
      }
      FixupAnchor(*aNodeOut);
      FixupAttribute(*aNodeOut, "src");
    }
    return rv;
  }

  if (content->IsAnyOfHTMLElements(nsGkAtoms::audio, nsGkAtoms::video)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "src");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::source)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "src");
    }
    return rv;
  }

  if (content->IsSVGElement(nsGkAtoms::img)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      // Disable image loads
      nsCOMPtr<nsIImageLoadingContent> imgCon = do_QueryInterface(*aNodeOut);
      if (imgCon) imgCon->SetLoadingEnabled(false);

      // FixupAnchor(*aNodeOut);  // XXXjwatt: is this line needed?
      FixupAttribute(*aNodeOut, "href", "http://www.w3.org/1999/xlink");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::script)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "src");
    }
    return rv;
  }

  if (content->IsSVGElement(nsGkAtoms::script)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "href", "http://www.w3.org/1999/xlink");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::embed)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "src");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::object)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "data");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::link)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      // First see if the link represents linked content
      rv = FixupAttribute(*aNodeOut, "href");
      if (NS_FAILED(rv)) {
        // Perhaps this link is actually an anchor to related content
        FixupAnchor(*aNodeOut);
      }
      // TODO if "type" attribute == "text/css"
      //        fixup stylesheet
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::frame)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "src");
    }
    return rv;
  }

  if (content->IsHTMLElement(nsGkAtoms::iframe)) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      FixupAttribute(*aNodeOut, "src");
    }
    return rv;
  }

  RefPtr<dom::HTMLInputElement> nodeAsInput =
      dom::HTMLInputElement::FromNodeOrNull(content);
  if (nodeAsInput) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      // Disable image loads
      nsCOMPtr<nsIImageLoadingContent> imgCon = do_QueryInterface(*aNodeOut);
      if (imgCon) {
        imgCon->SetLoadingEnabled(false);
      }

      FixupAttribute(*aNodeOut, "src");

      nsAutoString valueStr;
      NS_NAMED_LITERAL_STRING(valueAttr, "value");
      // Update element node attributes with user-entered form state
      RefPtr<dom::HTMLInputElement> outElt =
          dom::HTMLInputElement::FromNode((*aNodeOut)->AsContent());
      nsCOMPtr<nsIFormControl> formControl = do_QueryInterface(*aNodeOut);
      switch (formControl->ControlType()) {
        case NS_FORM_INPUT_EMAIL:
        case NS_FORM_INPUT_SEARCH:
        case NS_FORM_INPUT_TEXT:
        case NS_FORM_INPUT_TEL:
        case NS_FORM_INPUT_URL:
        case NS_FORM_INPUT_NUMBER:
        case NS_FORM_INPUT_RANGE:
        case NS_FORM_INPUT_DATE:
        case NS_FORM_INPUT_TIME:
        case NS_FORM_INPUT_COLOR:
          nodeAsInput->GetValue(valueStr, dom::CallerType::System);
          // Avoid superfluous value="" serialization
          if (valueStr.IsEmpty()) {
            outElt->RemoveAttribute(valueAttr, IgnoreErrors());
          } else {
            outElt->SetAttribute(valueAttr, valueStr, IgnoreErrors());
          }
          break;
        case NS_FORM_INPUT_CHECKBOX:
        case NS_FORM_INPUT_RADIO: {
          bool checked = nodeAsInput->Checked();
          outElt->SetDefaultChecked(checked, IgnoreErrors());
        } break;
        default:
          break;
      }
    }
    return rv;
  }

  dom::HTMLTextAreaElement* nodeAsTextArea =
      dom::HTMLTextAreaElement::FromNode(content);
  if (nodeAsTextArea) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      // Tell the document encoder to serialize the text child we create below
      *aSerializeCloneKids = true;

      nsAutoString valueStr;
      nodeAsTextArea->GetValue(valueStr);

      (*aNodeOut)->SetTextContent(valueStr, IgnoreErrors());
    }
    return rv;
  }

  dom::HTMLOptionElement* nodeAsOption =
      dom::HTMLOptionElement::FromNode(content);
  if (nodeAsOption) {
    nsresult rv = GetNodeToFixup(aNodeIn, aNodeOut);
    if (NS_SUCCEEDED(rv) && *aNodeOut) {
      dom::HTMLOptionElement* outElt =
          dom::HTMLOptionElement::FromNode((*aNodeOut)->AsContent());
      bool selected = nodeAsOption->Selected();
      outElt->SetDefaultSelected(selected, IgnoreErrors());
    }
    return rv;
  }

  return NS_OK;
}

}  // unnamed namespace

NS_IMETHODIMP
WebBrowserPersistLocalDocument::ReadResources(
    nsIWebBrowserPersistResourceVisitor* aVisitor) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIWebBrowserPersistResourceVisitor> visitor = aVisitor;

  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  ErrorResult err;
  RefPtr<dom::TreeWalker> walker = mDocument->CreateTreeWalker(
      *mDocument,
      dom::NodeFilter_Binding::SHOW_ELEMENT |
          dom::NodeFilter_Binding::SHOW_DOCUMENT |
          dom::NodeFilter_Binding::SHOW_PROCESSING_INSTRUCTION,
      nullptr, err);

  if (NS_WARN_IF(err.Failed())) {
    return err.StealNSResult();
  }
  MOZ_ASSERT(walker);

  RefPtr<ResourceReader> reader = new ResourceReader(this, aVisitor);
  nsCOMPtr<nsINode> currentNode = walker->CurrentNode();
  do {
    rv = reader->OnWalkDOMNode(currentNode);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      break;
    }

    ErrorResult err;
    currentNode = walker->NextNode(err);
    if (NS_WARN_IF(err.Failed())) {
      err.SuppressException();
      break;
    }
  } while (currentNode);
  reader->DocumentDone(rv);
  // If NS_FAILED(rv), it was / will be reported by an EndVisit call
  // via DocumentDone.  This method must return a failure if and
  // only if visitor won't be invoked.
  return NS_OK;
}

static uint32_t ConvertEncoderFlags(uint32_t aEncoderFlags) {
  uint32_t encoderFlags = 0;

  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_SELECTION_ONLY)
    encoderFlags |= nsIDocumentEncoder::OutputSelectionOnly;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_FORMATTED)
    encoderFlags |= nsIDocumentEncoder::OutputFormatted;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_RAW)
    encoderFlags |= nsIDocumentEncoder::OutputRaw;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_BODY_ONLY)
    encoderFlags |= nsIDocumentEncoder::OutputBodyOnly;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_PREFORMATTED)
    encoderFlags |= nsIDocumentEncoder::OutputPreformatted;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_WRAP)
    encoderFlags |= nsIDocumentEncoder::OutputWrap;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_FORMAT_FLOWED)
    encoderFlags |= nsIDocumentEncoder::OutputFormatFlowed;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_ABSOLUTE_LINKS)
    encoderFlags |= nsIDocumentEncoder::OutputAbsoluteLinks;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_ENCODE_BASIC_ENTITIES)
    encoderFlags |= nsIDocumentEncoder::OutputEncodeBasicEntities;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_CR_LINEBREAKS)
    encoderFlags |= nsIDocumentEncoder::OutputCRLineBreak;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_LF_LINEBREAKS)
    encoderFlags |= nsIDocumentEncoder::OutputLFLineBreak;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_NOSCRIPT_CONTENT)
    encoderFlags |= nsIDocumentEncoder::OutputNoScriptContent;
  if (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_NOFRAMES_CONTENT)
    encoderFlags |= nsIDocumentEncoder::OutputNoFramesContent;

  return encoderFlags;
}

static bool ContentTypeEncoderExists(const nsACString& aType) {
  return do_getDocumentTypeSupportedForEncoding(
      PromiseFlatCString(aType).get());
}

void WebBrowserPersistLocalDocument::DecideContentType(
    nsACString& aContentType) {
  if (aContentType.IsEmpty()) {
    if (NS_WARN_IF(NS_FAILED(GetContentType(aContentType)))) {
      aContentType.Truncate();
    }
  }
  if (!aContentType.IsEmpty() && !ContentTypeEncoderExists(aContentType)) {
    aContentType.Truncate();
  }
  if (aContentType.IsEmpty()) {
    aContentType.AssignLiteral("text/html");
  }
}

nsresult WebBrowserPersistLocalDocument::GetDocEncoder(
    const nsACString& aContentType, uint32_t aEncoderFlags,
    nsIDocumentEncoder** aEncoder) {
  nsCOMPtr<nsIDocumentEncoder> encoder =
      do_createDocumentEncoder(PromiseFlatCString(aContentType).get());
  NS_ENSURE_TRUE(encoder, NS_ERROR_FAILURE);

  nsresult rv =
      encoder->NativeInit(mDocument, NS_ConvertASCIItoUTF16(aContentType),
                          ConvertEncoderFlags(aEncoderFlags));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsAutoCString charSet;
  rv = GetCharacterSet(charSet);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  rv = encoder->SetCharset(charSet);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  encoder.forget(aEncoder);
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserPersistLocalDocument::WriteContent(
    nsIOutputStream* aStream, nsIWebBrowserPersistURIMap* aMap,
    const nsACString& aRequestedContentType, uint32_t aEncoderFlags,
    uint32_t aWrapColumn, nsIWebBrowserPersistWriteCompletion* aCompletion) {
  NS_ENSURE_ARG_POINTER(aStream);
  NS_ENSURE_ARG_POINTER(aCompletion);
  nsAutoCString contentType(aRequestedContentType);
  DecideContentType(contentType);

  nsCOMPtr<nsIDocumentEncoder> encoder;
  nsresult rv =
      GetDocEncoder(contentType, aEncoderFlags, getter_AddRefs(encoder));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aWrapColumn != 0 &&
      (aEncoderFlags & nsIWebBrowserPersist::ENCODE_FLAGS_WRAP)) {
    encoder->SetWrapColumn(aWrapColumn);
  }

  nsCOMPtr<nsIURI> targetURI;
  if (aMap) {
    nsAutoCString targetURISpec;
    rv = aMap->GetTargetBaseURI(targetURISpec);
    if (NS_SUCCEEDED(rv) && !targetURISpec.IsEmpty()) {
      rv = NS_NewURI(getter_AddRefs(targetURI), targetURISpec);
      NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);
    } else if (mPersistFlags &
               nsIWebBrowserPersist::PERSIST_FLAGS_FIXUP_LINKS_TO_DESTINATION) {
      return NS_ERROR_UNEXPECTED;
    }
  }
  rv = encoder->SetNodeFixup(new PersistNodeFixup(this, aMap, targetURI));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  rv = encoder->EncodeToStream(aStream);
  aCompletion->OnFinish(this, aStream, contentType, rv);
  return NS_OK;
}

}  // namespace mozilla

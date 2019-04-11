/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  An implementation for the XUL document. This implementation serves
  as the basis for generating an NGLayout content model.

  Notes
  -----

  1. We do some monkey business in the document observer methods to
     keep the element map in sync for HTML elements. Why don't we just
     do it for _all_ elements? Well, in the case of XUL elements,
     which may be lazily created during frame construction, the
     document observer methods will never be called because we'll be
     adding the XUL nodes into the content model "quietly".

*/

#include "mozilla/ArrayUtils.h"

#include <algorithm>

#include "XULDocument.h"

#include "nsError.h"
#include "nsIBoxObject.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIContentViewer.h"
#include "nsIStreamListener.h"
#include "nsITimer.h"
#include "nsDocShell.h"
#include "nsGkAtoms.h"
#include "nsXMLContentSink.h"
#include "nsXULContentSink.h"
#include "nsXULContentUtils.h"
#include "nsIStringEnumerator.h"
#include "nsDocElementCreatedNotificationRunner.h"
#include "nsNetUtil.h"
#include "nsParserCIID.h"
#include "nsPIBoxObject.h"
#include "mozilla/dom/BoxObject.h"
#include "nsString.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsXULElement.h"
#include "nsXULPrototypeCache.h"
#include "mozilla/Logging.h"
#include "nsIFrame.h"
#include "nsXBLService.h"
#include "nsCExternalHandlerService.h"
#include "nsMimeTypes.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsContentList.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsIParser.h"
#include "nsCharsetSource.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "nsIScriptError.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIObserverService.h"
#include "nsNodeUtils.h"
#include "nsIXULWindow.h"
#include "nsXULPopupManager.h"
#include "nsCCUncollectableMarker.h"
#include "nsURILoader.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/DocumentL10n.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/NodeInfoInlines.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/XULDocumentBinding.h"
#include "mozilla/dom/XULPersist.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "nsTextNode.h"
#include "nsJSUtils.h"
#include "js/CompilationAndEvaluation.h"
#include "js/SourceText.h"
#include "mozilla/dom/URL.h"
#include "nsIContentPolicy.h"
#include "mozAutoDocUpdate.h"
#include "xpcpublic.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "nsIConsoleService.h"

#include "mozilla/parser/PrototypeDocumentParser.h"
#include "mozilla/dom/PrototypeDocumentContentSink.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------
//
// CIDs
//

static NS_DEFINE_CID(kParserCID, NS_PARSER_CID);

//----------------------------------------------------------------------
//
// Statics
//

int32_t XULDocument::gRefCnt = 0;

LazyLogModule XULDocument::gXULLog("XULDocument");

//----------------------------------------------------------------------
//
// ctors & dtors
//

namespace mozilla {
namespace dom {

XULDocument::XULDocument(void)
    : XMLDocument("application/vnd.mozilla.xul+xml") {
  // Override the default in Document
  mCharacterSet = UTF_8_ENCODING;

  mDefaultElementType = kNameSpaceID_XUL;
  mType = eXUL;

  mAllowXULXBL = eTriTrue;
}

XULDocument::~XULDocument() {}

}  // namespace dom
}  // namespace mozilla

nsresult NS_NewXULDocument(Document** result) {
  MOZ_ASSERT(result != nullptr, "null ptr");
  if (!result) return NS_ERROR_NULL_POINTER;

  RefPtr<XULDocument> doc = new XULDocument();

  nsresult rv;
  if (NS_FAILED(rv = doc->Init())) {
    return rv;
  }

  doc.forget(result);
  return NS_OK;
}

namespace mozilla {
namespace dom {

//----------------------------------------------------------------------
//
// nsISupports interface
//

NS_IMPL_CYCLE_COLLECTION_CLASS(XULDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XULDocument, XMLDocument)
  NS_ASSERTION(
      !nsCCUncollectableMarker::InGeneration(cb, tmp->GetMarkedCCGeneration()),
      "Shouldn't traverse XULDocument!");
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XULDocument, XMLDocument)
  // XXX We should probably unlink all the objects we traverse.
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XULDocument, XMLDocument)

//----------------------------------------------------------------------
//
// Document interface
//

void XULDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) {
  MOZ_ASSERT_UNREACHABLE("Reset");
}

void XULDocument::ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                             nsIPrincipal* aPrincipal,
                             nsIPrincipal* aStoragePrincipal) {
  MOZ_ASSERT_UNREACHABLE("ResetToURI");
}

void XULDocument::SetContentType(const nsAString& aContentType) {
  NS_ASSERTION(
      aContentType.EqualsLiteral("application/vnd.mozilla.xul+xml"),
      "xul-documents always has content-type application/vnd.mozilla.xul+xml");
  // Don't do anything, xul always has the mimetype
  // application/vnd.mozilla.xul+xml
}

// This is called when the master document begins loading, whether it's
// being cached or not.
nsresult XULDocument::StartDocumentLoad(const char* aCommand,
                                        nsIChannel* aChannel,
                                        nsILoadGroup* aLoadGroup,
                                        nsISupports* aContainer,
                                        nsIStreamListener** aDocListener,
                                        bool aReset, nsIContentSink* aSink) {
  if (MOZ_LOG_TEST(gXULLog, LogLevel::Warning)) {
    nsCOMPtr<nsIURI> uri;
    nsresult rv = aChannel->GetOriginalURI(getter_AddRefs(uri));
    if (NS_SUCCEEDED(rv)) {
      nsAutoCString urlspec;
      rv = uri->GetSpec(urlspec);
      if (NS_SUCCEEDED(rv)) {
        MOZ_LOG(gXULLog, LogLevel::Warning,
                ("xul: load document '%s'", urlspec.get()));
      }
    }
  }

  MOZ_ASSERT(GetReadyStateEnum() == Document::READYSTATE_UNINITIALIZED,
             "Bad readyState");
  SetReadyStateInternal(READYSTATE_LOADING);

  mDocumentLoadGroup = do_GetWeakReference(aLoadGroup);

  mChannel = aChannel;

  // Get the URI.  Note that this should match nsDocShell::OnLoadingSite
  nsresult rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(mDocumentURI));
  NS_ENSURE_SUCCESS(rv, rv);

  mOriginalURI = mDocumentURI;

  // Get the document's principal
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIPrincipal> storagePrincipal;
  rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipals(
      mChannel, getter_AddRefs(principal), getter_AddRefs(storagePrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  bool equal = principal->Equals(storagePrincipal);

  principal = MaybeDowngradePrincipal(principal);
  if (equal) {
    storagePrincipal = principal;
  } else {
    storagePrincipal = MaybeDowngradePrincipal(storagePrincipal);
  }

  SetPrincipals(principal, storagePrincipal);

  ResetStylesheetsToURI(mDocumentURI);

  RetrieveRelevantHeaders(aChannel);

  mParser = new mozilla::parser::PrototypeDocumentParser(mDocumentURI, this);
  nsCOMPtr<nsIStreamListener> listener = mParser->GetStreamListener();
  listener.forget(aDocListener);

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));
  nsCOMPtr<nsIContentSink> sink;
  NS_NewPrototypeDocumentContentSink(getter_AddRefs(sink), this, mDocumentURI,
                                     docShell, aChannel);
  mParser->SetContentSink(sink);

  mParser->Parse(mDocumentURI, nullptr, (void*)this);

  return NS_OK;
}

void XULDocument::EndLoad() {
  mSynchronousDOMContentLoaded = true;
  Document::EndLoad();
}

//----------------------------------------------------------------------
//
// nsINode interface
//

nsresult XULDocument::Clone(mozilla::dom::NodeInfo* aNodeInfo,
                            nsINode** aResult) const {
  // We don't allow cloning of a XUL document
  *aResult = nullptr;
  return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
}

//----------------------------------------------------------------------
//
// Implementation methods
//

nsresult XULDocument::Init() {
  nsresult rv = XMLDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  if (gRefCnt++ == 0) {
    // ensure that the XUL prototype cache is instantiated successfully,
    // so that we can use nsXULPrototypeCache::GetInstance() without
    // null-checks in the rest of the class.
    nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
    if (!cache) {
      NS_ERROR("Could not instantiate nsXULPrototypeCache");
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

JSObject* XULDocument::WrapNode(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return XULDocument_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla

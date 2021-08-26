/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLDocument.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_intl.h"
#include "nsCommandManager.h"
#include "nsCOMPtr.h"
#include "nsGlobalWindow.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIHTMLContentSink.h"
#include "nsIProtocolHandler.h"
#include "nsIXMLContentSink.h"
#include "nsHTMLParts.h"
#include "nsHTMLStyleSheet.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsPIDOMWindow.h"
#include "nsDOMString.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIContentViewer.h"
#include "nsDocShell.h"
#include "nsDocShellLoadTypes.h"
#include "nsIScriptContext.h"
#include "nsContentList.h"
#include "nsError.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsAttrName.h"

#include "nsNetCID.h"
#include "nsParserCIID.h"
#include "mozilla/parser/PrototypeDocumentParser.h"
#include "mozilla/dom/PrototypeDocumentContentSink.h"
#include "nsNameSpaceManager.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/css/Loader.h"
#include "nsFrameSelection.h"

#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "DocumentInlines.h"
#include "nsICachingChannel.h"
#include "nsIContentViewer.h"
#include "nsIScriptElement.h"
#include "nsArrayUtils.h"

// AHMED 12-2
#include "nsBidiUtils.h"

#include "mozilla/Encoding.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/IdentifierMapEntry.h"
#include "mozilla/LoadInfo.h"
#include "nsNodeInfoManager.h"
#include "nsRange.h"
#include "mozAutoDocUpdate.h"
#include "nsCCUncollectableMarker.h"
#include "nsHtml5Module.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include "nsMimeTypes.h"
#include "nsIRequest.h"
#include "nsHtml5TreeOpExecutor.h"
#include "nsHtml5Parser.h"
#include "nsSandboxFlags.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLDocumentBinding.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ShadowIncludingTreeIterator.h"
#include "nsCharsetSource.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::dom;

#include "prtime.h"

//#define DEBUG_charset

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

// ==================================================================
// =
// ==================================================================

static bool IsAsciiCompatible(const Encoding* aEncoding) {
  return aEncoding->IsAsciiCompatible() || aEncoding == ISO_2022_JP_ENCODING;
}

nsresult NS_NewHTMLDocument(Document** aInstancePtrResult, bool aLoadedAsData) {
  RefPtr<nsHTMLDocument> doc = new nsHTMLDocument();

  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    *aInstancePtrResult = nullptr;
    return rv;
  }

  doc->SetLoadedAsData(aLoadedAsData, /* aConsiderForMemoryReporting */ true);
  doc.forget(aInstancePtrResult);

  return NS_OK;
}

nsHTMLDocument::nsHTMLDocument()
    : Document("text/html"),
      mContentListHolder(nullptr),
      mNumForms(0),
      mLoadFlags(0),
      mWarnedWidthHeight(false),
      mIsPlainText(false),
      mViewSource(false) {
  mType = eHTML;
  mDefaultElementType = kNameSpaceID_XHTML;
  mCompatMode = eCompatibility_NavQuirks;
}

nsHTMLDocument::~nsHTMLDocument() = default;

JSObject* nsHTMLDocument::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return HTMLDocument_Binding::Wrap(aCx, this, aGivenProto);
}

nsresult nsHTMLDocument::Init() {
  nsresult rv = Document::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  // Now reset the compatibility mode of the CSSLoader
  // to match our compat mode.
  CSSLoader()->SetCompatibilityMode(mCompatMode);

  return NS_OK;
}

void nsHTMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup) {
  Document::Reset(aChannel, aLoadGroup);

  if (aChannel) {
    aChannel->GetLoadFlags(&mLoadFlags);
  }
}

void nsHTMLDocument::ResetToURI(nsIURI* aURI, nsILoadGroup* aLoadGroup,
                                nsIPrincipal* aPrincipal,
                                nsIPrincipal* aPartitionedPrincipal) {
  mLoadFlags = nsIRequest::LOAD_NORMAL;

  Document::ResetToURI(aURI, aLoadGroup, aPrincipal, aPartitionedPrincipal);

  mImages = nullptr;
  mApplets = nullptr;
  mEmbeds = nullptr;
  mLinks = nullptr;
  mAnchors = nullptr;
  mScripts = nullptr;

  mForms = nullptr;

  // Make the content type default to "text/html", we are a HTML
  // document, after all. Once we start getting data, this may be
  // changed.
  SetContentTypeInternal(nsDependentCString("text/html"));
}

void nsHTMLDocument::TryReloadCharset(nsIContentViewer* aCv,
                                      int32_t& aCharsetSource,
                                      NotNull<const Encoding*>& aEncoding) {
  if (aCv) {
    int32_t reloadEncodingSource;
    const auto reloadEncoding =
        aCv->GetReloadEncodingAndSource(&reloadEncodingSource);
    if (kCharsetUninitialized != reloadEncodingSource) {
      aCv->ForgetReloadEncoding();

      if (reloadEncodingSource <= aCharsetSource) return;

      if (reloadEncoding && IsAsciiCompatible(reloadEncoding)) {
        aCharsetSource = reloadEncodingSource;
        aEncoding = WrapNotNull(reloadEncoding);
      }
    }
  }
}

void nsHTMLDocument::TryUserForcedCharset(nsIContentViewer* aCv,
                                          nsIDocShell* aDocShell,
                                          int32_t& aCharsetSource,
                                          NotNull<const Encoding*>& aEncoding) {
  if (aCharsetSource >= kCharsetFromXmlDeclarationUtf16) {
    return;
  }

  // mCharacterSet not updated yet for channel, so check aEncoding, too.
  if (WillIgnoreCharsetOverride() || !IsAsciiCompatible(aEncoding)) {
    return;
  }

  if (aDocShell && nsDocShell::Cast(aDocShell)->GetForcedAutodetection()) {
    // This is the Character Encoding menu code path in Firefox
    aEncoding = WINDOWS_1252_ENCODING;
    aCharsetSource = kCharsetFromPendingUserForcedAutoDetection;
    nsDocShell::Cast(aDocShell)->ResetForcedAutodetection();
  }
}

void nsHTMLDocument::TryParentCharset(nsIDocShell* aDocShell,
                                      int32_t& aCharsetSource,
                                      NotNull<const Encoding*>& aEncoding) {
  if (!aDocShell) {
    return;
  }
  if (aCharsetSource >= kCharsetFromXmlDeclarationUtf16) {
    return;
  }

  int32_t parentSource;
  const Encoding* parentCharset;
  nsCOMPtr<nsIPrincipal> parentPrincipal;
  aDocShell->GetParentCharset(parentCharset, &parentSource,
                              getter_AddRefs(parentPrincipal));
  if (!parentCharset) {
    return;
  }
  if (kCharsetFromPendingUserForcedAutoDetection == parentSource ||
      kCharsetFromInitialUserForcedAutoDetection == parentSource ||
      kCharsetFromFinalUserForcedAutoDetection == parentSource) {
    if (WillIgnoreCharsetOverride() ||
        !IsAsciiCompatible(aEncoding) ||  // if channel said UTF-16
        !IsAsciiCompatible(parentCharset)) {
      return;
    }
    aEncoding = WrapNotNull(parentCharset);
    aCharsetSource = kCharsetFromPendingUserForcedAutoDetection;
    return;
  }

  if (aCharsetSource >= kCharsetFromParentFrame) {
    return;
  }

  if (kCharsetFromInitialAutoDetectionASCII <= parentSource) {
    // Make sure that's OK
    if (!NodePrincipal()->Equals(parentPrincipal) ||
        !IsAsciiCompatible(parentCharset)) {
      return;
    }

    aEncoding = WrapNotNull(parentCharset);
    aCharsetSource = kCharsetFromParentFrame;
  }
}

// Using a prototype document is only allowed with chrome privilege.
bool ShouldUsePrototypeDocument(nsIChannel* aChannel, Document* aDoc) {
  if (!aChannel || !aDoc ||
      !StaticPrefs::dom_prototype_document_cache_enabled()) {
    return false;
  }
  return nsContentUtils::IsChromeDoc(aDoc);
}

nsresult nsHTMLDocument::StartDocumentLoad(const char* aCommand,
                                           nsIChannel* aChannel,
                                           nsILoadGroup* aLoadGroup,
                                           nsISupports* aContainer,
                                           nsIStreamListener** aDocListener,
                                           bool aReset, nsIContentSink* aSink) {
  if (!aCommand) {
    MOZ_ASSERT(false, "Command is mandatory");
    return NS_ERROR_INVALID_POINTER;
  }
  if (aSink) {
    MOZ_ASSERT(false, "Got a sink override. Should not happen for HTML doc.");
    return NS_ERROR_INVALID_ARG;
  }
  if (mType != eHTML) {
    MOZ_ASSERT(mType == eXHTML);
    MOZ_ASSERT(false, "Must not set HTML doc to XHTML mode before load start.");
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsAutoCString contentType;
  aChannel->GetContentType(contentType);

  bool view =
      !strcmp(aCommand, "view") || !strcmp(aCommand, "external-resource");
  mViewSource = !strcmp(aCommand, "view-source");
  bool asData = !strcmp(aCommand, kLoadAsData);
  if (!(view || mViewSource || asData)) {
    MOZ_ASSERT(false, "Bad parser command");
    return NS_ERROR_INVALID_ARG;
  }

  bool html = contentType.EqualsLiteral(TEXT_HTML);
  bool xhtml = !html && (contentType.EqualsLiteral(APPLICATION_XHTML_XML) ||
                         contentType.EqualsLiteral(APPLICATION_WAPXHTML_XML));
  mIsPlainText =
      !html && !xhtml && nsContentUtils::IsPlainTextType(contentType);
  if (!(html || xhtml || mIsPlainText || mViewSource)) {
    MOZ_ASSERT(false, "Channel with bad content type.");
    return NS_ERROR_INVALID_ARG;
  }

  bool forceUtf8 =
      mIsPlainText && nsContentUtils::IsUtf8OnlyPlainTextType(contentType);

  bool loadAsHtml5 = true;

  if (!mViewSource && xhtml) {
    // We're parsing XHTML as XML, remember that.
    mType = eXHTML;
    SetCompatibilityMode(eCompatibility_FullStandards);
    loadAsHtml5 = false;
  }

  // TODO: Proper about:blank treatment is bug 543435
  if (loadAsHtml5 && view) {
    // mDocumentURI hasn't been set, yet, so get the URI from the channel
    nsCOMPtr<nsIURI> uri;
    aChannel->GetOriginalURI(getter_AddRefs(uri));
    // Adapted from nsDocShell:
    // GetSpec can be expensive for some URIs, so check the scheme first.
    if (uri && uri->SchemeIs("about")) {
      if (uri->GetSpecOrDefault().EqualsLiteral("about:blank")) {
        loadAsHtml5 = false;
      }
    }
  }

  nsresult rv = Document::StartDocumentLoad(aCommand, aChannel, aLoadGroup,
                                            aContainer, aDocListener, aReset);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

  bool loadWithPrototype = false;
  RefPtr<nsHtml5Parser> html5Parser;
  if (loadAsHtml5) {
    html5Parser = nsHtml5Module::NewHtml5Parser();
    mParser = html5Parser;
    if (mIsPlainText) {
      if (mViewSource) {
        html5Parser->MarkAsNotScriptCreated("view-source-plain");
      } else {
        html5Parser->MarkAsNotScriptCreated("plain-text");
      }
    } else if (mViewSource && !html) {
      html5Parser->MarkAsNotScriptCreated("view-source-xml");
    } else {
      html5Parser->MarkAsNotScriptCreated(aCommand);
    }
  } else if (xhtml && ShouldUsePrototypeDocument(aChannel, this)) {
    loadWithPrototype = true;
    nsCOMPtr<nsIURI> originalURI;
    aChannel->GetOriginalURI(getter_AddRefs(originalURI));
    mParser = new mozilla::parser::PrototypeDocumentParser(originalURI, this);
  } else {
    mParser = do_CreateInstance(kCParserCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Look for the parent document.  Note that at this point we don't have our
  // content viewer set up yet, and therefore do not have a useful
  // mParentDocument.

  // in this block of code, if we get an error result, we return it
  // but if we get a null pointer, that's perfectly legal for parent
  // and parentContentViewer
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  if (docShell) {
    docShell->GetInProcessSameTypeParent(getter_AddRefs(parentAsItem));
  }

  nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));
  nsCOMPtr<nsIContentViewer> parentContentViewer;
  if (parent) {
    rv = parent->GetContentViewer(getter_AddRefs(parentContentViewer));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIContentViewer> cv;
  if (docShell) {
    docShell->GetContentViewer(getter_AddRefs(cv));
  }
  if (!cv) {
    cv = std::move(parentContentViewer);
  }

  nsAutoCString urlSpec;
  uri->GetSpec(urlSpec);
#ifdef DEBUG_charset
  printf("Determining charset for %s\n", urlSpec.get());
#endif

  // These are the charset source and charset for our document
  int32_t charsetSource;
  auto encoding = UTF_8_ENCODING;

  // For error reporting and referrer policy setting
  nsHtml5TreeOpExecutor* executor = nullptr;
  if (loadAsHtml5) {
    executor = static_cast<nsHtml5TreeOpExecutor*>(mParser->GetContentSink());
  }

  bool channelHadCharset = false;
  if (forceUtf8) {
    charsetSource = kCharsetFromUtf8OnlyMime;
  } else if (!IsHTMLDocument() || !docShell) {  // no docshell for text/html XHR
    charsetSource =
        IsHTMLDocument() ? kCharsetFromFallback : kCharsetFromDocTypeDefault;
    TryChannelCharset(aChannel, charsetSource, encoding, executor);
    channelHadCharset = (charsetSource == kCharsetFromChannel);
  } else {
    NS_ASSERTION(docShell, "Unexpected null value");

    charsetSource = kCharsetUninitialized;
    // Used for .in and .lk TLDs. .jp is handled in the parser.
    encoding = WINDOWS_1252_ENCODING;

    // The following will try to get the character encoding from various
    // sources. Each Try* function will return early if the source is already
    // at least as large as any of the sources it might look at.  Some of
    // these functions (like TryReloadCharset and TryParentCharset) can set
    // charsetSource to various values depending on where the charset they
    // end up finding originally comes from.

    // Try the channel's charset (e.g., charset from HTTP
    // "Content-Type" header) first. This way, we get to reject overrides in
    // TryParentCharset and TryUserForcedCharset if the channel said UTF-16.
    // This is to avoid socially engineered XSS by adding user-supplied
    // content to a UTF-16 site such that the byte have a dangerous
    // interpretation as ASCII and the user can be lured to using the
    // charset menu.
    TryChannelCharset(aChannel, charsetSource, encoding, executor);
    channelHadCharset = (charsetSource == kCharsetFromChannel);

    TryUserForcedCharset(cv, docShell, charsetSource, encoding);

    TryReloadCharset(cv, charsetSource, encoding);  // For encoding reload
    TryParentCharset(docShell, charsetSource, encoding);
  }

  SetDocumentCharacterSetSource(charsetSource);
  SetDocumentCharacterSet(encoding);

  // Set the parser as the stream listener for the document loader...
  rv = NS_OK;
  nsCOMPtr<nsIStreamListener> listener = mParser->GetStreamListener();
  listener.forget(aDocListener);

#ifdef DEBUG_charset
  printf(" charset = %s source %d\n", charset.get(), charsetSource);
#endif
  mParser->SetDocumentCharset(encoding, charsetSource, channelHadCharset);
  mParser->SetCommand(aCommand);

  if (!IsHTMLDocument()) {
    MOZ_ASSERT(!loadAsHtml5);
    if (loadWithPrototype) {
      nsCOMPtr<nsIContentSink> sink;
      NS_NewPrototypeDocumentContentSink(getter_AddRefs(sink), this, uri,
                                         docShell, aChannel);
      mParser->SetContentSink(sink);
    } else {
      nsCOMPtr<nsIXMLContentSink> xmlsink;
      NS_NewXMLContentSink(getter_AddRefs(xmlsink), this, uri, docShell,
                           aChannel);
      mParser->SetContentSink(xmlsink);
    }
  } else {
    if (loadAsHtml5) {
      html5Parser->Initialize(this, uri, docShell, aChannel);
    } else {
      // about:blank *only*
      nsCOMPtr<nsIHTMLContentSink> htmlsink;
      NS_NewHTMLContentSink(getter_AddRefs(htmlsink), this, uri, docShell,
                            aChannel);
      mParser->SetContentSink(htmlsink);
    }
  }

  // parser the content of the URI
  mParser->Parse(uri, this);

  return rv;
}

bool nsHTMLDocument::UseWidthDeviceWidthFallbackViewport() const {
  if (mIsPlainText) {
    // Plain text documents are simple enough that font inflation doesn't offer
    // any appreciable advantage over defaulting to "width=device-width" and
    // subsequently turning on word-wrapping.
    return true;
  }
  return Document::UseWidthDeviceWidthFallbackViewport();
}

Element* nsHTMLDocument::GetUnfocusedKeyEventTarget() {
  if (nsGenericHTMLElement* body = GetBody()) {
    return body;
  }
  return Document::GetUnfocusedKeyEventTarget();
}

bool nsHTMLDocument::IsRegistrableDomainSuffixOfOrEqualTo(
    const nsAString& aHostSuffixString, const nsACString& aOrigHost) {
  // https://html.spec.whatwg.org/multipage/browsers.html#is-a-registrable-domain-suffix-of-or-is-equal-to
  if (aHostSuffixString.IsEmpty()) {
    return false;
  }

  nsCOMPtr<nsIURI> origURI = CreateInheritingURIForHost(aOrigHost);
  if (!origURI) {
    // Error: failed to parse input domain
    return false;
  }

  nsCOMPtr<nsIURI> newURI =
      RegistrableDomainSuffixOfInternal(aHostSuffixString, origURI);
  if (!newURI) {
    // Error: illegal domain
    return false;
  }
  return true;
}

void nsHTMLDocument::AddedForm() { ++mNumForms; }

void nsHTMLDocument::RemovedForm() { --mNumForms; }

int32_t nsHTMLDocument::GetNumFormsSynchronous() { return mNumForms; }

bool nsHTMLDocument::ResolveName(JSContext* aCx, const nsAString& aName,
                                 JS::MutableHandle<JS::Value> aRetval,
                                 ErrorResult& aError) {
  IdentifierMapEntry* entry = mIdentifierMap.GetEntry(aName);
  if (!entry) {
    return false;
  }

  nsBaseContentList* list = entry->GetNameContentList();
  uint32_t length = list ? list->Length() : 0;

  nsIContent* node;
  if (length > 0) {
    if (length > 1) {
      // The list contains more than one element, return the whole list.
      if (!ToJSValue(aCx, list, aRetval)) {
        aError.NoteJSContextException(aCx);
        return false;
      }
      return true;
    }

    // Only one element in the list, return the element instead of returning
    // the list.
    node = list->Item(0);
  } else {
    // No named items were found, see if there's one registerd by id for aName.
    Element* e = entry->GetIdElement();

    if (!e || !nsGenericHTMLElement::ShouldExposeIdAsHTMLDocumentProperty(e)) {
      return false;
    }

    node = e;
  }

  if (!ToJSValue(aCx, node, aRetval)) {
    aError.NoteJSContextException(aCx);
    return false;
  }

  return true;
}

void nsHTMLDocument::GetSupportedNames(nsTArray<nsString>& aNames) {
  for (const auto& entry : mIdentifierMap) {
    if (entry.HasNameElement() ||
        entry.HasIdElementExposedAsHTMLDocumentProperty()) {
      aNames.AppendElement(entry.GetKeyAsString());
    }
  }
}

//----------------------------

// forms related stuff

bool nsHTMLDocument::MatchFormControls(Element* aElement, int32_t aNamespaceID,
                                       nsAtom* aAtom, void* aData) {
  return aElement->IsNodeOfType(nsIContent::eHTML_FORM_CONTROL);
}

nsresult nsHTMLDocument::Clone(dom::NodeInfo* aNodeInfo,
                               nsINode** aResult) const {
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  RefPtr<nsHTMLDocument> clone = new nsHTMLDocument();
  nsresult rv = CloneDocHelper(clone.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // State from nsHTMLDocument
  clone->mLoadFlags = mLoadFlags;

  clone.forget(aResult);
  return NS_OK;
}

/* virtual */
void nsHTMLDocument::DocAddSizeOfExcludingThis(
    nsWindowSizes& aWindowSizes) const {
  Document::DocAddSizeOfExcludingThis(aWindowSizes);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mLinks
  // - mAnchors
}

bool nsHTMLDocument::WillIgnoreCharsetOverride() {
  if (mEncodingMenuDisabled) {
    return true;
  }
  if (mType != eHTML) {
    MOZ_ASSERT(mType == eXHTML);
    return true;
  }
  if (mCharacterSetSource >= kCharsetFromByteOrderMark) {
    return true;
  }
  if (!mCharacterSet->IsAsciiCompatible() &&
      mCharacterSet != ISO_2022_JP_ENCODING) {
    return true;
  }
  nsIURI* uri = GetOriginalURI();
  if (uri) {
    if (uri->SchemeIs("about")) {
      return true;
    }
    bool isResource;
    nsresult rv = NS_URIChainHasFlags(
        uri, nsIProtocolHandler::URI_IS_UI_RESOURCE, &isResource);
    if (NS_FAILED(rv) || isResource) {
      return true;
    }
  }

  switch (mCharacterSetSource) {
    case kCharsetUninitialized:
    case kCharsetFromFallback:
    case kCharsetFromDocTypeDefault:
    case kCharsetFromInitialAutoDetectionWouldHaveBeenUTF8:
    case kCharsetFromInitialAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD:
    case kCharsetFromFinalAutoDetectionWouldHaveBeenUTF8:
    case kCharsetFromFinalAutoDetectionWouldNotHaveBeenUTF8DependedOnTLD:
    case kCharsetFromParentFrame:
    case kCharsetFromXmlDeclaration:
    case kCharsetFromMetaPrescan:
    case kCharsetFromMetaTag:
    case kCharsetFromChannel:
      return false;
  }

  bool potentialEffect = false;
  nsIPrincipal* parentPrincipal = NodePrincipal();

  auto subDoc = [&potentialEffect, parentPrincipal](Document& aSubDoc) {
    if (parentPrincipal->Equals(aSubDoc.NodePrincipal()) &&
        !aSubDoc.WillIgnoreCharsetOverride()) {
      potentialEffect = true;
      return CallState::Stop;
    }
    return CallState::Continue;
  };
  EnumerateSubDocuments(subDoc);

  return !potentialEffect;
}

void nsHTMLDocument::GetFormsAndFormControls(nsContentList** aFormList,
                                             nsContentList** aFormControlList) {
  RefPtr<ContentListHolder> holder = mContentListHolder;
  if (!holder) {
    // Flush our content model so it'll be up to date
    // If this becomes unnecessary and the following line is removed,
    // please also remove the corresponding flush operation from
    // nsHtml5TreeBuilderCppSupplement.h. (Look for "See bug 497861." there.)
    // XXXsmaug nsHtml5TreeBuilderCppSupplement doesn't seem to have such flush
    //         anymore.
    FlushPendingNotifications(FlushType::Content);

    RefPtr<nsContentList> htmlForms = GetExistingForms();
    if (!htmlForms) {
      // If the document doesn't have an existing forms content list, create a
      // new one which will be released soon by ContentListHolder.  The idea is
      // that we don't have that list hanging around for a long time and slowing
      // down future DOM mutations.
      //
      // Please keep this in sync with Document::Forms().
      htmlForms = new nsContentList(this, kNameSpaceID_XHTML, nsGkAtoms::form,
                                    nsGkAtoms::form,
                                    /* aDeep = */ true,
                                    /* aLiveList = */ true);
    }

    RefPtr<nsContentList> htmlFormControls = new nsContentList(
        this, nsHTMLDocument::MatchFormControls, nullptr, nullptr,
        /* aDeep = */ true,
        /* aMatchAtom = */ nullptr,
        /* aMatchNameSpaceId = */ kNameSpaceID_None,
        /* aFuncMayDependOnAttr = */ true,
        /* aLiveList = */ true);

    holder = new ContentListHolder(this, htmlForms, htmlFormControls);
    RefPtr<ContentListHolder> runnable = holder;
    if (NS_SUCCEEDED(
            Dispatch(TaskCategory::GarbageCollection, runnable.forget()))) {
      mContentListHolder = holder;
    }
  }

  NS_ADDREF(*aFormList = holder->mFormList);
  NS_ADDREF(*aFormControlList = holder->mFormControlList);
}

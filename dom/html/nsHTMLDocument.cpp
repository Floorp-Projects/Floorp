/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLDocument.h"

#include "nsIContentPolicy.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/HTMLAllCollection.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "nsCommandManager.h"
#include "nsCOMPtr.h"
#include "nsGlobalWindow.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIDocumentLoader.h"
#include "nsIHTMLContentSink.h"
#include "nsIXMLContentSink.h"
#include "nsHTMLParts.h"
#include "nsHTMLStyleSheet.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsPIDOMWindow.h"
#include "nsDOMString.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsIIOService.h"
#include "nsNetUtil.h"
#include "nsIContentViewer.h"
#include "nsDocShell.h"
#include "nsDocShellLoadTypes.h"
#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsContentList.h"
#include "nsError.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsIScriptSecurityManager.h"
#include "nsAttrName.h"
#include "nsNodeUtils.h"

#include "nsNetCID.h"
#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsIComponentManager.h"
#include "nsParserCIID.h"
#include "mozilla/parser/PrototypeDocumentParser.h"
#include "mozilla/dom/PrototypeDocumentContentSink.h"
#include "nsNameSpaceManager.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/css/Loader.h"
#include "nsIHttpChannel.h"
#include "nsIFile.h"
#include "nsFrameSelection.h"

#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "DocumentInlines.h"
#include "nsIDocumentEncoder.h"  //for outputting selection
#include "nsICachingChannel.h"
#include "nsIContentViewer.h"
#include "nsIScriptElement.h"
#include "nsIScriptError.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsIEffectiveTLDService.h"

// AHMED 12-2
#include "nsBidiUtils.h"

#include "mozilla/dom/FallbackEncoding.h"
#include "mozilla/Encoding.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/HTMLEditor.h"
#include "mozilla/IdentifierMapEntry.h"
#include "mozilla/LoadInfo.h"
#include "nsIEditingSession.h"
#include "nsNodeInfoManager.h"
#include "nsIPlaintextEditor.h"
#include "nsIEditorStyleSheets.h"
#include "nsIInlineSpellChecker.h"
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
#include "nsIImageDocument.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLDocumentBinding.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ShadowIncludingTreeIterator.h"
#include "nsCharsetSource.h"
#include "nsIStringBundle.h"
#include "nsFocusManager.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsIStructuredCloneContainer.h"
#include "nsLayoutStylesheetCache.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Unused.h"
#include "nsCommandParams.h"

using namespace mozilla;
using namespace mozilla::dom;

#define NS_MAX_DOCUMENT_WRITE_DEPTH 20

#include "prtime.h"

//#define DEBUG_charset

static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

// this function will return false if the command is not recognized
// inCommandID will be converted as necessary for internal operations
// inParam will be converted as necessary for internal operations
// outParam will be Empty if no parameter is needed or if returning a boolean
// outIsBoolean will determine whether to send param as a boolean or string
// outBooleanParam will not be set unless outIsBoolean
static bool ConvertToMidasInternalCommand(const nsAString& inCommandID,
                                          const nsAString& inParam,
                                          nsACString& outCommandID,
                                          nsACString& outParam, bool& isBoolean,
                                          bool& boolValue);

static bool ConvertToMidasInternalCommand(const nsAString& inCommandID,
                                          nsACString& outCommandID);

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

  doc->SetLoadedAsData(aLoadedAsData);
  doc.forget(aInstancePtrResult);

  return NS_OK;
}

nsHTMLDocument::nsHTMLDocument()
    : Document("text/html"),
      mContentListHolder(nullptr),
      mNumForms(0),
      mWriteLevel(0),
      mLoadFlags(0),
      mTooDeepWriteRecursion(false),
      mDisableDocWrite(false),
      mWarnedWidthHeight(false),
      mContentEditableCount(0),
      mEditingState(EditingState::eOff),
      mPendingMaybeEditingStateChanged(false),
      mHasBeenEditable(false),
      mIsPlainText(false) {
  mType = eHTML;
  mDefaultElementType = kNameSpaceID_XHTML;
  mCompatMode = eCompatibility_NavQuirks;
}

nsHTMLDocument::~nsHTMLDocument() {}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsHTMLDocument, Document, mAll,
                                   mMidasCommandManager)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(nsHTMLDocument, Document,
                                             nsIHTMLDocument)

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
                                nsIPrincipal* aStoragePrincipal) {
  mLoadFlags = nsIRequest::LOAD_NORMAL;

  Document::ResetToURI(aURI, aLoadGroup, aPrincipal, aStoragePrincipal);

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

void nsHTMLDocument::TryHintCharset(nsIContentViewer* aCv,
                                    int32_t& aCharsetSource,
                                    NotNull<const Encoding*>& aEncoding) {
  if (aCv) {
    int32_t requestCharsetSource;
    nsresult rv = aCv->GetHintCharacterSetSource(&requestCharsetSource);

    if (NS_SUCCEEDED(rv) && kCharsetUninitialized != requestCharsetSource) {
      auto requestCharset = aCv->GetHintCharset();
      aCv->SetHintCharacterSetSource((int32_t)(kCharsetUninitialized));

      if (requestCharsetSource <= aCharsetSource) return;

      if (requestCharset && IsAsciiCompatible(requestCharset)) {
        aCharsetSource = requestCharsetSource;
        aEncoding = WrapNotNull(requestCharset);
      }
    }
  }
}

void nsHTMLDocument::TryUserForcedCharset(nsIContentViewer* aCv,
                                          nsIDocShell* aDocShell,
                                          int32_t& aCharsetSource,
                                          NotNull<const Encoding*>& aEncoding) {
  if (kCharsetFromUserForced <= aCharsetSource) return;

  // mCharacterSet not updated yet for channel, so check aEncoding, too.
  if (WillIgnoreCharsetOverride() || !IsAsciiCompatible(aEncoding)) {
    return;
  }

  const Encoding* forceCharsetFromDocShell = nullptr;
  if (aCv) {
    // XXX mailnews-only
    forceCharsetFromDocShell = aCv->GetForceCharset();
  }

  if (forceCharsetFromDocShell && IsAsciiCompatible(forceCharsetFromDocShell)) {
    aEncoding = WrapNotNull(forceCharsetFromDocShell);
    aCharsetSource = kCharsetFromUserForced;
    return;
  }

  if (aDocShell) {
    // This is the Character Encoding menu code path in Firefox
    auto encoding = nsDocShell::Cast(aDocShell)->GetForcedCharset();

    if (encoding) {
      if (!IsAsciiCompatible(encoding)) {
        return;
      }
      aEncoding = WrapNotNull(encoding);
      aCharsetSource = kCharsetFromUserForced;
      aDocShell->SetForcedCharset(NS_LITERAL_CSTRING(""));
    }
  }
}

void nsHTMLDocument::TryCacheCharset(nsICachingChannel* aCachingChannel,
                                     int32_t& aCharsetSource,
                                     NotNull<const Encoding*>& aEncoding) {
  nsresult rv;

  if (kCharsetFromCache <= aCharsetSource) {
    return;
  }

  nsCString cachedCharset;
  rv = aCachingChannel->GetCacheTokenCachedCharset(cachedCharset);
  if (NS_FAILED(rv) || cachedCharset.IsEmpty()) {
    return;
  }
  // The canonical names changed, so the cache may have an old name.
  const Encoding* encoding = Encoding::ForLabelNoReplacement(cachedCharset);
  if (!encoding) {
    return;
  }
  // Check IsAsciiCompatible() even in the cache case, because the value
  // might be stale and in the case of a stale charset that is not a rough
  // ASCII superset, the parser has no way to recover.
  if (!encoding->IsAsciiCompatible() && encoding != ISO_2022_JP_ENCODING) {
    return;
  }
  aEncoding = WrapNotNull(encoding);
  aCharsetSource = kCharsetFromCache;
}

void nsHTMLDocument::TryParentCharset(nsIDocShell* aDocShell,
                                      int32_t& aCharsetSource,
                                      NotNull<const Encoding*>& aEncoding) {
  if (!aDocShell) {
    return;
  }
  if (aCharsetSource >= kCharsetFromParentForced) {
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
  if (kCharsetFromParentForced == parentSource ||
      kCharsetFromUserForced == parentSource) {
    if (WillIgnoreCharsetOverride() ||
        !IsAsciiCompatible(aEncoding) ||  // if channel said UTF-16
        !IsAsciiCompatible(parentCharset)) {
      return;
    }
    aEncoding = WrapNotNull(parentCharset);
    aCharsetSource = kCharsetFromParentForced;
    return;
  }

  if (aCharsetSource >= kCharsetFromParentFrame) {
    return;
  }

  if (kCharsetFromCache <= parentSource) {
    // Make sure that's OK
    if (!NodePrincipal()->Equals(parentPrincipal) ||
        !IsAsciiCompatible(parentCharset)) {
      return;
    }

    aEncoding = WrapNotNull(parentCharset);
    aCharsetSource = kCharsetFromParentFrame;
  }
}

void nsHTMLDocument::TryTLD(int32_t& aCharsetSource,
                            NotNull<const Encoding*>& aEncoding) {
  if (aCharsetSource >= kCharsetFromTopLevelDomain) {
    return;
  }
  if (!FallbackEncoding::sGuessFallbackFromTopLevelDomain) {
    return;
  }
  if (!mDocumentURI) {
    return;
  }
  nsAutoCString host;
  mDocumentURI->GetAsciiHost(host);
  if (host.IsEmpty()) {
    return;
  }
  // First let's see if the host is DNS-absolute and ends with a dot and
  // get rid of that one.
  if (host.Last() == '.') {
    host.SetLength(host.Length() - 1);
    if (host.IsEmpty()) {
      return;
    }
  }
  // If we still have a dot, the host is weird, so let's continue only
  // if we have something other than a dot now.
  if (host.Last() == '.') {
    return;
  }
  int32_t index = host.RFindChar('.');
  if (index == kNotFound) {
    // We have an intranet host, Gecko-internal URL or an IPv6 address.
    return;
  }
  // Since the string didn't end with a dot and we found a dot,
  // there is at least one character between the dot and the end of
  // the string, so taking the substring below is safe.
  nsAutoCString tld;
  ToLowerCase(Substring(host, index + 1, host.Length() - (index + 1)), tld);
  // Reject generic TLDs and country TLDs that need more research
  if (!FallbackEncoding::IsParticipatingTopLevelDomain(tld)) {
    return;
  }
  // Check if we have an IPv4 address
  bool seenNonDigit = false;
  for (size_t i = 0; i < tld.Length(); ++i) {
    char c = tld.CharAt(i);
    if (c < '0' || c > '9') {
      seenNonDigit = true;
      break;
    }
  }
  if (!seenNonDigit) {
    return;
  }
  aCharsetSource = kCharsetFromTopLevelDomain;
  aEncoding = FallbackEncoding::FromTopLevelDomain(tld);
}

void nsHTMLDocument::TryFallback(int32_t& aCharsetSource,
                                 NotNull<const Encoding*>& aEncoding) {
  if (kCharsetFromFallback <= aCharsetSource) return;

  aCharsetSource = kCharsetFromFallback;
  aEncoding = FallbackEncoding::FromLocale();
}

// Using a prototype document is only allowed with chrome privilege.
bool ShouldUsePrototypeDocument(nsIChannel* aChannel, Document* aDoc) {
  if (!aChannel || !aDoc ||
      !StaticPrefs::dom_prototype_document_cache_enabled()) {
    return false;
  }
  if (!nsContentUtils::IsChromeDoc(aDoc)) {
    return false;
  }
  nsCOMPtr<nsIURI> originalURI;
  aChannel->GetOriginalURI(getter_AddRefs(originalURI));
  return IsChromeURI(originalURI);
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
  bool viewSource = !strcmp(aCommand, "view-source");
  bool asData = !strcmp(aCommand, kLoadAsData);
  if (!(view || viewSource || asData)) {
    MOZ_ASSERT(false, "Bad parser command");
    return NS_ERROR_INVALID_ARG;
  }

  bool html = contentType.EqualsLiteral(TEXT_HTML);
  bool xhtml = !html && (contentType.EqualsLiteral(APPLICATION_XHTML_XML) ||
                         contentType.EqualsLiteral(APPLICATION_WAPXHTML_XML));
  mIsPlainText =
      !html && !xhtml && nsContentUtils::IsPlainTextType(contentType);
  if (!(html || xhtml || mIsPlainText || viewSource)) {
    MOZ_ASSERT(false, "Channel with bad content type.");
    return NS_ERROR_INVALID_ARG;
  }

  bool forceUtf8 =
      mIsPlainText && nsContentUtils::IsUtf8OnlyPlainTextType(contentType);

  bool loadAsHtml5 = true;

  if (!viewSource && xhtml) {
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
    bool isAbout = false;
    if (uri && NS_SUCCEEDED(uri->SchemeIs("about", &isAbout)) && isAbout) {
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

  // Store the security info for future use.
  aChannel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsICachingChannel> cachingChan = do_QueryInterface(aChannel);
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(aContainer));

  bool loadWithPrototype = false;
  if (loadAsHtml5) {
    mParser = nsHtml5Module::NewHtml5Parser();
    if (mIsPlainText) {
      if (viewSource) {
        mParser->MarkAsNotScriptCreated("view-source-plain");
      } else {
        mParser->MarkAsNotScriptCreated("plain-text");
      }
    } else if (viewSource && !html) {
      mParser->MarkAsNotScriptCreated("view-source-xml");
    } else {
      mParser->MarkAsNotScriptCreated(aCommand);
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
    docShell->GetSameTypeParent(getter_AddRefs(parentAsItem));
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
    cv = parentContentViewer.forget();
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
    if (mReferrerPolicySet) {
      // CSP may have set the referrer policy, so a speculative parser should
      // start with the new referrer policy.
      executor->SetSpeculationReferrerPolicy(
          static_cast<ReferrerPolicy>(mReferrerPolicy));
    }
  }

  if (forceUtf8) {
    charsetSource = kCharsetFromUtf8OnlyMime;
  } else if (!IsHTMLDocument() || !docShell) {  // no docshell for text/html XHR
    charsetSource =
        IsHTMLDocument() ? kCharsetFromFallback : kCharsetFromDocTypeDefault;
    TryChannelCharset(aChannel, charsetSource, encoding, executor);
  } else {
    NS_ASSERTION(docShell, "Unexpected null value");

    charsetSource = kCharsetUninitialized;

    // The following will try to get the character encoding from various
    // sources. Each Try* function will return early if the source is already
    // at least as large as any of the sources it might look at.  Some of
    // these functions (like TryHintCharset and TryParentCharset) can set
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

    TryUserForcedCharset(cv, docShell, charsetSource, encoding);

    TryHintCharset(cv, charsetSource, encoding);  // XXX mailnews-only
    TryParentCharset(docShell, charsetSource, encoding);

    if (cachingChan && !urlSpec.IsEmpty()) {
      TryCacheCharset(cachingChan, charsetSource, encoding);
    }

    TryTLD(charsetSource, encoding);
    TryFallback(charsetSource, encoding);
  }

  SetDocumentCharacterSetSource(charsetSource);
  SetDocumentCharacterSet(encoding);

  if (cachingChan) {
    nsAutoCString charset;
    encoding->Name(charset);
    rv = cachingChan->SetCacheTokenCachedCharset(charset);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "cannot SetMetaDataElement");
    rv = NS_OK;  // don't propagate error
  }

  // Set the parser as the stream listener for the document loader...
  rv = NS_OK;
  nsCOMPtr<nsIStreamListener> listener = mParser->GetStreamListener();
  listener.forget(aDocListener);

#ifdef DEBUG_charset
  printf(" charset = %s source %d\n", charset.get(), charsetSource);
#endif
  mParser->SetDocumentCharset(encoding, charsetSource);
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
      nsHtml5Module::Initialize(mParser, this, uri, docShell, aChannel);
    } else {
      // about:blank *only*
      nsCOMPtr<nsIHTMLContentSink> htmlsink;
      NS_NewHTMLContentSink(getter_AddRefs(htmlsink), this, uri, docShell,
                            aChannel);
      mParser->SetContentSink(htmlsink);
    }
  }

  // parser the content of the URI
  mParser->Parse(uri, nullptr, (void*)this);

  return rv;
}

void nsHTMLDocument::BeginLoad() {
  if (IsEditingOn()) {
    // Reset() blows away all event listeners in the document, and our
    // editor relies heavily on those. Midas is turned on, to make it
    // work, re-initialize it to give it a chance to add its event
    // listeners again.

    TurnEditingOff();
    EditingStateChanged();
  }
  Document::BeginLoad();
}

void nsHTMLDocument::EndLoad() {
  bool turnOnEditing =
      mParser && (HasFlag(NODE_IS_EDITABLE) || mContentEditableCount > 0);
  // Note: Document::EndLoad nulls out mParser.
  Document::EndLoad();
  if (turnOnEditing) {
    EditingStateChanged();
  }

  if (!GetWindow()) {
    // This is a document that's not in a window.  For example, this could be an
    // XMLHttpRequest responseXML document, or a document created via DOMParser
    // or DOMImplementation.  We don't reach this code normally for such
    // documents (which is not obviously correct), but can reach it via
    // document.open()/document.close().
    //
    // Such documents don't fire load events, but per spec should set their
    // readyState to "complete" when parsing and all loading of subresources is
    // done.  Parsing is done now, and documents not in a window don't load
    // subresources, so just go ahead and mark ourselves as complete.
    SetReadyStateInternal(Document::READYSTATE_COMPLETE,
                          /* updateTimingInformation = */ false);

    // Reset mSkipLoadEventAfterClose just in case.
    mSkipLoadEventAfterClose = false;
  }
}

void nsHTMLDocument::SetCompatibilityMode(nsCompatibility aMode) {
  NS_ASSERTION(IsHTMLDocument() || aMode == eCompatibility_FullStandards,
               "Bad compat mode for XHTML document!");

  if (mCompatMode == aMode) {
    return;
  }
  mCompatMode = aMode;
  CompatibilityModeChanged();
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

already_AddRefed<nsIURI> nsHTMLDocument::GetDomainURI() {
  nsIPrincipal* principal = NodePrincipal();

  nsCOMPtr<nsIURI> uri;
  principal->GetDomain(getter_AddRefs(uri));
  if (uri) {
    return uri.forget();
  }

  principal->GetURI(getter_AddRefs(uri));
  return uri.forget();
}

void nsHTMLDocument::GetDomain(nsAString& aDomain) {
  nsCOMPtr<nsIURI> uri = GetDomainURI();

  if (!uri) {
    aDomain.Truncate();
    return;
  }

  nsAutoCString hostName;
  nsresult rv = nsContentUtils::GetHostOrIPv6WithBrackets(uri, hostName);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(hostName, aDomain);
  } else {
    // If we can't get the host from the URI (e.g. about:, javascript:,
    // etc), just return an empty string.
    aDomain.Truncate();
  }
}

already_AddRefed<nsIURI> nsHTMLDocument::CreateInheritingURIForHost(
    const nsACString& aHostString) {
  if (aHostString.IsEmpty()) {
    return nullptr;
  }

  // Create new URI
  nsCOMPtr<nsIURI> uri = GetDomainURI();
  if (!uri) {
    return nullptr;
  }

  nsresult rv;
  rv = NS_MutateURI(uri)
           .SetUserPass(EmptyCString())
           .SetPort(-1)  // we want to reset the port number if needed.
           .SetHostPort(aHostString)
           .Finalize(uri);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return uri.forget();
}

already_AddRefed<nsIURI> nsHTMLDocument::RegistrableDomainSuffixOfInternal(
    const nsAString& aNewDomain, nsIURI* aOrigHost) {
  if (NS_WARN_IF(!aOrigHost)) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> newURI =
      CreateInheritingURIForHost(NS_ConvertUTF16toUTF8(aNewDomain));
  if (!newURI) {
    // Error: failed to parse input domain
    return nullptr;
  }

  // Check new domain - must be a superdomain of the current host
  // For example, a page from foo.bar.com may set domain to bar.com,
  // but not to ar.com, baz.com, or fi.foo.bar.com.
  nsAutoCString current;
  nsAutoCString domain;
  if (NS_FAILED(aOrigHost->GetAsciiHost(current))) {
    current.Truncate();
  }
  if (NS_FAILED(newURI->GetAsciiHost(domain))) {
    domain.Truncate();
  }

  bool ok = current.Equals(domain);
  if (current.Length() > domain.Length() && StringEndsWith(current, domain) &&
      current.CharAt(current.Length() - domain.Length() - 1) == '.') {
    // We're golden if the new domain is the current page's base domain or a
    // subdomain of it.
    nsCOMPtr<nsIEffectiveTLDService> tldService =
        do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
    if (!tldService) {
      return nullptr;
    }

    nsAutoCString currentBaseDomain;
    ok = NS_SUCCEEDED(
        tldService->GetBaseDomain(aOrigHost, 0, currentBaseDomain));
    NS_ASSERTION(StringEndsWith(domain, currentBaseDomain) ==
                     (domain.Length() >= currentBaseDomain.Length()),
                 "uh-oh!  slight optimization wasn't valid somehow!");
    ok = ok && domain.Length() >= currentBaseDomain.Length();
  }

  if (!ok) {
    // Error: illegal domain
    return nullptr;
  }

  return CreateInheritingURIForHost(domain);
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

void nsHTMLDocument::SetDomain(const nsAString& aDomain, ErrorResult& rv) {
  if (mSandboxFlags & SANDBOXED_DOMAIN) {
    // We're sandboxed; disallow setting domain
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!FeaturePolicyUtils::IsFeatureAllowed(
          this, NS_LITERAL_STRING("document-domain"))) {
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (aDomain.IsEmpty()) {
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsCOMPtr<nsIURI> uri = GetDomainURI();
  if (!uri) {
    rv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Check new domain - must be a superdomain of the current host
  // For example, a page from foo.bar.com may set domain to bar.com,
  // but not to ar.com, baz.com, or fi.foo.bar.com.

  nsCOMPtr<nsIURI> newURI = RegistrableDomainSuffixOfInternal(aDomain, uri);
  if (!newURI) {
    // Error: illegal domain
    rv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  rv = NodePrincipal()->SetDomain(newURI);
}

mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder> nsHTMLDocument::Open(
    const nsAString& aURL, const nsAString& aName, const nsAString& aFeatures,
    bool aReplace, ErrorResult& rv) {
  MOZ_ASSERT(nsContentUtils::CanCallerAccess(this),
             "XOW should have caught this!");

  nsCOMPtr<nsPIDOMWindowInner> window = GetInnerWindow();
  if (!window) {
    rv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindowOuter> outer =
      nsPIDOMWindowOuter::GetFromCurrentInner(window);
  if (!outer) {
    rv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }
  RefPtr<nsGlobalWindowOuter> win = nsGlobalWindowOuter::Cast(outer);
  nsCOMPtr<nsPIDOMWindowOuter> newWindow;
  // XXXbz We ignore aReplace for now.
  rv = win->OpenJS(aURL, aName, aFeatures, getter_AddRefs(newWindow));
  if (!newWindow) {
    return nullptr;
  }
  return WindowProxyHolder(newWindow->GetBrowsingContext());
}

Document* nsHTMLDocument::Open(const Optional<nsAString>& /* unused */,
                               const nsAString& /* unused */,
                               ErrorResult& aError) {
  // Implements
  // <https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#document-open-steps>

  MOZ_ASSERT(nsContentUtils::CanCallerAccess(this),
             "XOW should have caught this!");

  // Step 1 -- throw if we're an XML document.
  if (!IsHTMLDocument() || mDisableDocWrite) {
    aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Step 2 -- throw if dynamic markup insertion should throw.
  if (ShouldThrowOnDynamicMarkupInsertion()) {
    aError.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Step 3 -- get the entry document, so we can use it for security checks.
  nsCOMPtr<Document> callerDoc = GetEntryDocument();
  if (!callerDoc) {
    // If we're called from C++ or in some other way without an originating
    // document we can't do a document.open w/o changing the principal of the
    // document to something like about:blank (as that's the only sane thing to
    // do when we don't know the origin of this call), and since we can't
    // change the principals of a document for security reasons we'll have to
    // refuse to go ahead with this call.

    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Step 4 -- make sure we're same-origin (not just same origin-domain) with
  // the entry document.
  if (!callerDoc->NodePrincipal()->Equals(NodePrincipal())) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // Step 5 -- if we have an active parser, just no-op.
  // If we already have a parser we ignore the document.open call.
  if (mParser || mParserAborted) {
    // The WHATWG spec used to say: "If the document has an active parser that
    // isn't a script-created parser, and the insertion point associated with
    // that parser's input stream is not undefined (that is, it does point to
    // somewhere in the input stream), then the method does nothing. Abort these
    // steps and return the Document object on which the method was invoked."
    // Note that aborting a parser leaves the parser "active" with its insertion
    // point "not undefined". We track this using mParserAborted, because
    // aborting a parser nulls out mParser.
    //
    // Actually, the spec says something slightly different now, about having
    // an "active parser whose script nesting level is greater than 0".  It
    // does not mention insertion points at all.  Not sure whether it matters
    // in practice.  It seems like "script nesting level" replaced the
    // insertion point concept?  Anyway,
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1475000 is probably
    // relevant here.
    return this;
  }

  // Step 6 -- check for open() during unload.  Per spec, this is just a check
  // of the ignore-opens-during-unload counter, but our unload event code
  // doesn't affect that counter yet (unlike pagehide and beforeunload, which
  // do), so we check for unload directly.
  if (ShouldIgnoreOpens()) {
    return this;
  }

  nsCOMPtr<nsIDocShell> shell(mDocumentContainer);
  if (shell) {
    bool inUnload;
    shell->GetIsInUnload(&inUnload);
    if (inUnload) {
      return this;
    }
  }

  // At this point we know this is a valid-enough document.open() call
  // and not a no-op.  Increment our use counter.
  SetDocumentAndPageUseCounter(eUseCounter_custom_DocumentOpen);

  // Step 7 -- stop existing navigation of our browsing context (and all other
  // loads it's doing) if we're the active document of our browsing context.
  // Note that we do not want to stop anything if there is no existing
  // navigation.
  if (shell && IsCurrentActiveDocument() &&
      shell->GetIsAttemptingToNavigate()) {
    nsCOMPtr<nsIWebNavigation> webnav(do_QueryInterface(shell));
    webnav->Stop(nsIWebNavigation::STOP_NETWORK);

    // The Stop call may have cancelled the onload blocker request or
    // prevented it from getting added, so we need to make sure it gets added
    // to the document again otherwise the document could have a non-zero
    // onload block count without the onload blocker request being in the
    // loadgroup.
    EnsureOnloadBlocker();
  }

  // Step 8 -- clear event listeners out of our DOM tree
  for (nsINode* node : ShadowIncludingTreeIterator(*this)) {
    if (EventListenerManager* elm = node->GetExistingListenerManager()) {
      elm->RemoveAllListeners();
    }
  }

  // Step 9 -- clear event listeners from our window, if we have one.
  //
  // Note that we explicitly want the inner window, and only if we're its
  // document.  We want to do this (per spec) even when we're not the "active
  // document", so we can't go through GetWindow(), because it might forward to
  // the wrong inner.
  if (nsPIDOMWindowInner* win = GetInnerWindow()) {
    if (win->GetExtantDoc() == this) {
      if (EventListenerManager* elm =
              nsGlobalWindowInner::Cast(win)->GetExistingListenerManager()) {
        elm->RemoveAllListeners();
      }
    }
  }

  // Step 10 -- remove all our DOM kids without firing any mutation events.
  DisconnectNodeTree();

  // Step 11 -- if we're the current document in our docshell, do the
  // equivalent of pushState() with the new URL we should have.
  if (shell && IsCurrentActiveDocument()) {
    nsCOMPtr<nsIURI> newURI = callerDoc->GetDocumentURI();
    if (callerDoc != this) {
      nsCOMPtr<nsIURI> noFragmentURI;
      nsresult rv = NS_GetURIWithoutRef(newURI, getter_AddRefs(noFragmentURI));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aError.Throw(rv);
        return nullptr;
      }
      newURI = noFragmentURI.forget();
    }

    // UpdateURLAndHistory might do various member-setting, so make sure we're
    // holding strong refs to all the refcounted args on the stack.  We can
    // assume that our caller is holding on to "this" already.
    nsCOMPtr<nsIURI> currentURI = GetDocumentURI();
    bool equalURIs;
    nsresult rv = currentURI->Equals(newURI, &equalURIs);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aError.Throw(rv);
      return nullptr;
    }
    nsCOMPtr<nsIStructuredCloneContainer> stateContainer(mStateObjectContainer);
    rv = shell->UpdateURLAndHistory(this, newURI, stateContainer, EmptyString(),
                                    /* aReplace = */ true, currentURI,
                                    equalURIs);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aError.Throw(rv);
      return nullptr;
    }

    // And use the security info of the caller document as well, since
    // it's the thing providing our data.
    mSecurityInfo = callerDoc->GetSecurityInfo();

    // This is not mentioned in the spec, but I think that's a spec bug.  See
    // <https://github.com/whatwg/html/issues/4299>.  In any case, since our
    // URL may be changing away from about:blank here, we really want to unset
    // this flag no matter what, since only about:blank can be an initial
    // document.
    SetIsInitialDocument(false);

    // And let our docloader know that it will need to track our load event.
    nsDocShell::Cast(shell)->SetDocumentOpenedButNotLoaded();
  }

  // Per spec nothing happens with our URI in other cases, though note
  // <https://github.com/whatwg/html/issues/4286>.

  // Note that we don't need to do anything here with base URIs per spec.
  // That said, this might be assuming that we implement
  // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#fallback-base-url
  // correctly, which we don't right now for the about:blank case.

  // Step 12, but note <https://github.com/whatwg/html/issues/4292>.
  mSkipLoadEventAfterClose = mLoadEventFiring;

  // Preliminary to steps 13-16.  Set our ready state to uninitialized before
  // we do anything else, so we can then proceed to later ready state levels.
  SetReadyStateInternal(READYSTATE_UNINITIALIZED,
                        /* updateTimingInformation = */ false);

  // Step 13 -- set our compat mode to standards.
  SetCompatibilityMode(eCompatibility_FullStandards);

  // Step 14 -- create a new parser associated with document.  This also does
  // step 16 implicitly.
  mParserAborted = false;
  mParser = nsHtml5Module::NewHtml5Parser();
  nsHtml5Module::Initialize(mParser, this, GetDocumentURI(), shell, nullptr);
  if (mReferrerPolicySet) {
    // CSP may have set the referrer policy, so a speculative parser should
    // start with the new referrer policy.
    nsHtml5TreeOpExecutor* executor = nullptr;
    executor = static_cast<nsHtml5TreeOpExecutor*>(mParser->GetContentSink());
    if (executor && mReferrerPolicySet) {
      executor->SetSpeculationReferrerPolicy(
          static_cast<ReferrerPolicy>(mReferrerPolicy));
    }
  }

  // Some internal non-spec bookkeeping.
  mContentTypeForWriteCalls.AssignLiteral("text/html");

  if (shell) {
    // Prepare the docshell and the document viewer for the impending
    // out-of-band document.write()
    shell->PrepareForNewContentModel();

    nsCOMPtr<nsIContentViewer> cv;
    shell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      cv->LoadStart(this);
    }
  }

  // Step 15.
  SetReadyStateInternal(Document::READYSTATE_LOADING,
                        /* updateTimingInformation = */ false);

  // Step 16 happened with step 14 above.

  // Step 17.
  return this;
}

void nsHTMLDocument::Close(ErrorResult& rv) {
  if (!IsHTMLDocument()) {
    // No calling document.close() on XHTML!

    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (ShouldThrowOnDynamicMarkupInsertion()) {
    rv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (!mParser || !mParser->IsScriptCreated()) {
    return;
  }

  ++mWriteLevel;
  rv = (static_cast<nsHtml5Parser*>(mParser.get()))
           ->Parse(EmptyString(), nullptr, mContentTypeForWriteCalls, true);
  --mWriteLevel;

  // Even if that Parse() call failed, do the rest of this method

  // XXX Make sure that all the document.written content is
  // reflowed.  We should remove this call once we change
  // nsHTMLDocument::OpenCommon() so that it completely destroys the
  // earlier document's content and frame hierarchy.  Right now, it
  // re-uses the earlier document's root content object and
  // corresponding frame objects.  These re-used frame objects think
  // that they have already been reflowed, so they drop initial
  // reflows.  For certain cases of document.written content, like a
  // frameset document, the dropping of the initial reflow means
  // that we end up in document.close() without appended any reflow
  // commands to the reflow queue and, consequently, without adding
  // the dummy layout request to the load group.  Since the dummy
  // layout request is not added to the load group, the onload
  // handler of the frameset fires before the frames get reflowed
  // and loaded.  That is the long explanation for why we need this
  // one line of code here!
  // XXXbz as far as I can tell this may not be needed anymore; all
  // the testcases in bug 57636 pass without this line...  Leaving
  // it be for now, though.  In any case, there's no reason to do
  // this if we have no presshell, since in that case none of the
  // above about reusing frames applies.
  //
  // XXXhsivonen keeping this around for bug 577508 / 253951 still :-(
  if (GetPresShell()) {
    FlushPendingNotifications(FlushType::Layout);
  }
}

void nsHTMLDocument::WriteCommon(const Sequence<nsString>& aText,
                                 bool aNewlineTerminate,
                                 mozilla::ErrorResult& rv) {
  // Fast path the common case
  if (aText.Length() == 1) {
    WriteCommon(aText[0], aNewlineTerminate, rv);
  } else {
    // XXXbz it would be nice if we could pass all the strings to the parser
    // without having to do all this copying and then ask it to start
    // parsing....
    nsString text;
    for (uint32_t i = 0; i < aText.Length(); ++i) {
      text.Append(aText[i]);
    }
    WriteCommon(text, aNewlineTerminate, rv);
  }
}

void nsHTMLDocument::WriteCommon(const nsAString& aText, bool aNewlineTerminate,
                                 ErrorResult& aRv) {
  mTooDeepWriteRecursion =
      (mWriteLevel > NS_MAX_DOCUMENT_WRITE_DEPTH || mTooDeepWriteRecursion);
  if (NS_WARN_IF(mTooDeepWriteRecursion)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  if (!IsHTMLDocument() || mDisableDocWrite) {
    // No calling document.write*() on XHTML!

    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (ShouldThrowOnDynamicMarkupInsertion()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (mParserAborted) {
    // Hixie says aborting the parser doesn't undefine the insertion point.
    // However, since we null out mParser in that case, we track the
    // theoretically defined insertion point using mParserAborted.
    return;
  }

  // Implement Step 4.1 of:
  // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#document-write-steps
  if (ShouldIgnoreOpens()) {
    return;
  }

  void* key = GenerateParserKey();
  if (mParser && !mParser->IsInsertionPointDefined()) {
    if (mIgnoreDestructiveWritesCounter) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM Events"), this,
          nsContentUtils::eDOM_PROPERTIES, "DocumentWriteIgnored", nullptr, 0,
          mDocumentURI);
      return;
    }
    // The spec doesn't tell us to ignore opens from here, but we need to
    // ensure opens are ignored here.
    IgnoreOpensDuringUnload ignoreOpenGuard(this);
    mParser->Terminate();
    MOZ_RELEASE_ASSERT(!mParser, "mParser should have been null'd out");
  }

  if (!mParser) {
    if (mIgnoreDestructiveWritesCounter) {
      // Instead of implying a call to document.open(), ignore the call.
      nsContentUtils::ReportToConsole(
          nsIScriptError::warningFlag, NS_LITERAL_CSTRING("DOM Events"), this,
          nsContentUtils::eDOM_PROPERTIES, "DocumentWriteIgnored", nullptr, 0,
          mDocumentURI);
      return;
    }

    Open(Optional<nsAString>(), EmptyString(), aRv);

    // If Open() fails, or if it didn't create a parser (as it won't
    // if the user chose to not discard the current document through
    // onbeforeunload), don't write anything.
    if (aRv.Failed() || !mParser) {
      return;
    }
  }

  static NS_NAMED_LITERAL_STRING(new_line, "\n");

  ++mWriteLevel;

  // This could be done with less code, but for performance reasons it
  // makes sense to have the code for two separate Parse() calls here
  // since the concatenation of strings costs more than we like. And
  // why pay that price when we don't need to?
  if (aNewlineTerminate) {
    aRv = (static_cast<nsHtml5Parser*>(mParser.get()))
              ->Parse(aText + new_line, key, mContentTypeForWriteCalls, false);
  } else {
    aRv = (static_cast<nsHtml5Parser*>(mParser.get()))
              ->Parse(aText, key, mContentTypeForWriteCalls, false);
  }

  --mWriteLevel;

  mTooDeepWriteRecursion = (mWriteLevel != 0 && mTooDeepWriteRecursion);
}

void nsHTMLDocument::Write(const Sequence<nsString>& aText, ErrorResult& rv) {
  WriteCommon(aText, false, rv);
}

void nsHTMLDocument::Writeln(const Sequence<nsString>& aText, ErrorResult& rv) {
  WriteCommon(aText, true, rv);
}

void nsHTMLDocument::AddedForm() { ++mNumForms; }

void nsHTMLDocument::RemovedForm() { --mNumForms; }

int32_t nsHTMLDocument::GetNumFormsSynchronous() { return mNumForms; }

void nsHTMLDocument::GetAlinkColor(nsAString& aAlinkColor) {
  aAlinkColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetALink(aAlinkColor);
  }
}

void nsHTMLDocument::SetAlinkColor(const nsAString& aAlinkColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetALink(aAlinkColor);
  }
}

void nsHTMLDocument::GetLinkColor(nsAString& aLinkColor) {
  aLinkColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetLink(aLinkColor);
  }
}

void nsHTMLDocument::SetLinkColor(const nsAString& aLinkColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetLink(aLinkColor);
  }
}

void nsHTMLDocument::GetVlinkColor(nsAString& aVlinkColor) {
  aVlinkColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetVLink(aVlinkColor);
  }
}

void nsHTMLDocument::SetVlinkColor(const nsAString& aVlinkColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetVLink(aVlinkColor);
  }
}

void nsHTMLDocument::GetBgColor(nsAString& aBgColor) {
  aBgColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetBgColor(aBgColor);
  }
}

void nsHTMLDocument::SetBgColor(const nsAString& aBgColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetBgColor(aBgColor);
  }
}

void nsHTMLDocument::GetFgColor(nsAString& aFgColor) {
  aFgColor.Truncate();

  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->GetText(aFgColor);
  }
}

void nsHTMLDocument::SetFgColor(const nsAString& aFgColor) {
  HTMLBodyElement* body = GetBodyElement();
  if (body) {
    body->SetText(aFgColor);
  }
}

void nsHTMLDocument::CaptureEvents() {
  WarnOnceAbout(Document::eUseOfCaptureEvents);
}

void nsHTMLDocument::ReleaseEvents() {
  WarnOnceAbout(Document::eUseOfReleaseEvents);
}

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
  for (auto iter = mIdentifierMap.Iter(); !iter.Done(); iter.Next()) {
    IdentifierMapEntry* entry = iter.Get();
    if (entry->HasNameElement() ||
        entry->HasIdElementExposedAsHTMLDocumentProperty()) {
      aNames.AppendElement(entry->GetKeyAsString());
    }
  }
}

//----------------------------

// forms related stuff

bool nsHTMLDocument::MatchFormControls(Element* aElement, int32_t aNamespaceID,
                                       nsAtom* aAtom, void* aData) {
  return aElement->IsNodeOfType(nsIContent::eHTML_FORM_CONTROL);
}

void* nsHTMLDocument::GenerateParserKey(void) {
  if (!mScriptLoader) {
    // If we don't have a script loader, then the parser probably isn't parsing
    // anything anyway, so just return null.
    return nullptr;
  }

  // The script loader provides us with the currently executing script element,
  // which is guaranteed to be unique per script.
  nsIScriptElement* script = mScriptLoader->GetCurrentParserInsertedScript();
  if (script && mParser && mParser->IsScriptCreated()) {
    nsCOMPtr<nsIParser> creatorParser = script->GetCreatorParser();
    if (creatorParser != mParser) {
      // Make scripts that aren't inserted by the active parser of this document
      // participate in the context of the script that document.open()ed
      // this document.
      return nullptr;
    }
  }
  return script;
}

void nsHTMLDocument::GetDesignMode(nsAString& aDesignMode) {
  if (HasFlag(NODE_IS_EDITABLE)) {
    aDesignMode.AssignLiteral("on");
  } else {
    aDesignMode.AssignLiteral("off");
  }
}

void nsHTMLDocument::MaybeEditingStateChanged() {
  if (!mPendingMaybeEditingStateChanged && mMayStartLayout &&
      mUpdateNestLevel == 0 && (mContentEditableCount > 0) != IsEditingOn()) {
    if (nsContentUtils::IsSafeToRunScript()) {
      EditingStateChanged();
    } else if (!mInDestructor) {
      nsContentUtils::AddScriptRunner(
          NewRunnableMethod("nsHTMLDocument::MaybeEditingStateChanged", this,
                            &nsHTMLDocument::MaybeEditingStateChanged));
    }
  }
}

void nsHTMLDocument::EndUpdate() {
  const bool reset = !mPendingMaybeEditingStateChanged;
  mPendingMaybeEditingStateChanged = true;
  Document::EndUpdate();
  if (reset) {
    mPendingMaybeEditingStateChanged = false;
  }
  MaybeEditingStateChanged();
}

void nsHTMLDocument::SetMayStartLayout(bool aMayStartLayout) {
  Document::SetMayStartLayout(aMayStartLayout);

  MaybeEditingStateChanged();
}

// Helper class, used below in ChangeContentEditableCount().
class DeferredContentEditableCountChangeEvent : public Runnable {
 public:
  DeferredContentEditableCountChangeEvent(nsHTMLDocument* aDoc,
                                          nsIContent* aElement)
      : mozilla::Runnable("DeferredContentEditableCountChangeEvent"),
        mDoc(aDoc),
        mElement(aElement) {}

  NS_IMETHOD Run() override {
    if (mElement && mElement->OwnerDoc() == mDoc) {
      mDoc->DeferredContentEditableCountChange(mElement);
    }
    return NS_OK;
  }

 private:
  RefPtr<nsHTMLDocument> mDoc;
  nsCOMPtr<nsIContent> mElement;
};

nsresult nsHTMLDocument::ChangeContentEditableCount(nsIContent* aElement,
                                                    int32_t aChange) {
  NS_ASSERTION(int32_t(mContentEditableCount) + aChange >= 0,
               "Trying to decrement too much.");

  mContentEditableCount += aChange;

  nsContentUtils::AddScriptRunner(
      new DeferredContentEditableCountChangeEvent(this, aElement));

  return NS_OK;
}

void nsHTMLDocument::DeferredContentEditableCountChange(nsIContent* aElement) {
  if (mParser ||
      (mUpdateNestLevel > 0 && (mContentEditableCount > 0) != IsEditingOn())) {
    return;
  }

  EditingState oldState = mEditingState;

  nsresult rv = EditingStateChanged();
  NS_ENSURE_SUCCESS_VOID(rv);

  if (oldState == mEditingState && mEditingState == eContentEditable) {
    // We just changed the contentEditable state of a node, we need to reset
    // the spellchecking state of that node.
    if (aElement) {
      nsPIDOMWindowOuter* window = GetWindow();
      if (!window) return;

      nsIDocShell* docshell = window->GetDocShell();
      if (!docshell) return;

      RefPtr<HTMLEditor> htmlEditor = docshell->GetHTMLEditor();
      if (htmlEditor) {
        RefPtr<nsRange> range = new nsRange(aElement);
        IgnoredErrorResult res;
        range->SelectNode(*aElement, res);
        if (res.Failed()) {
          // The node might be detached from the document at this point,
          // which would cause this call to fail.  In this case, we can
          // safely ignore the contenteditable count change.
          return;
        }

        nsCOMPtr<nsIInlineSpellChecker> spellChecker;
        rv = htmlEditor->GetInlineSpellChecker(false,
                                               getter_AddRefs(spellChecker));
        NS_ENSURE_SUCCESS_VOID(rv);

        if (spellChecker) {
          rv = spellChecker->SpellCheckRange(range);
        }
      }
    }
  }
}

HTMLAllCollection* nsHTMLDocument::All() {
  if (!mAll) {
    mAll = new HTMLAllCollection(this);
  }
  return mAll;
}

static void NotifyEditableStateChange(nsINode* aNode, Document* aDocument) {
  for (nsIContent* child = aNode->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsElement()) {
      child->AsElement()->UpdateState(true);
    }
    NotifyEditableStateChange(child, aDocument);
  }
}

void nsHTMLDocument::TearingDownEditor() {
  if (IsEditingOn()) {
    mEditingState = eTearingDown;
    RemoveContentEditableStyleSheets();
  }
}

nsresult nsHTMLDocument::TurnEditingOff() {
  NS_ASSERTION(mEditingState != eOff, "Editing is already off.");

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) return NS_ERROR_FAILURE;

  nsIDocShell* docshell = window->GetDocShell();
  if (!docshell) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIEditingSession> editSession;
  nsresult rv = docshell->GetEditingSession(getter_AddRefs(editSession));
  NS_ENSURE_SUCCESS(rv, rv);

  // turn editing off
  rv = editSession->TearDownEditorOnWindow(window);
  NS_ENSURE_SUCCESS(rv, rv);

  mEditingState = eOff;

  // Editor resets selection since it is being destroyed.  But if focus is
  // still into editable control, we have to initialize selection again.
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    Element* element = fm->GetFocusedElement();
    nsCOMPtr<nsITextControlElement> txtCtrl = do_QueryInterface(element);
    if (txtCtrl) {
      RefPtr<TextEditor> textEditor = txtCtrl->GetTextEditor();
      if (textEditor) {
        textEditor->ReinitializeSelection(*element);
      }
    }
  }

  return NS_OK;
}

static bool HasPresShell(nsPIDOMWindowOuter* aWindow) {
  nsIDocShell* docShell = aWindow->GetDocShell();
  if (!docShell) return false;
  return docShell->GetPresShell() != nullptr;
}

nsresult nsHTMLDocument::SetEditingState(EditingState aState) {
  mEditingState = aState;
  return NS_OK;
}

nsresult nsHTMLDocument::EditingStateChanged() {
  if (mRemovedFromDocShell) {
    return NS_OK;
  }

  if (mEditingState == eSettingUp || mEditingState == eTearingDown) {
    // XXX We shouldn't recurse
    return NS_OK;
  }

  bool designMode = HasFlag(NODE_IS_EDITABLE);
  EditingState newState =
      designMode ? eDesignMode
                 : (mContentEditableCount > 0 ? eContentEditable : eOff);
  if (mEditingState == newState) {
    // No changes in editing mode.
    return NS_OK;
  }

  if (newState == eOff) {
    // Editing is being turned off.
    nsAutoScriptBlocker scriptBlocker;
    NotifyEditableStateChange(this, this);
    return TurnEditingOff();
  }

  // Flush out style changes on our _parent_ document, if any, so that
  // our check for a presshell won't get stale information.
  if (mParentDocument) {
    mParentDocument->FlushPendingNotifications(FlushType::Style);
  }

  // get editing session, make sure this is a strong reference so the
  // window can't get deleted during the rest of this call.
  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (!window) return NS_ERROR_FAILURE;

  nsIDocShell* docshell = window->GetDocShell();
  if (!docshell) return NS_ERROR_FAILURE;

  // FlushPendingNotifications might destroy our docshell.
  bool isBeingDestroyed = false;
  docshell->IsBeingDestroyed(&isBeingDestroyed);
  if (isBeingDestroyed) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIEditingSession> editSession;
  nsresult rv = docshell->GetEditingSession(getter_AddRefs(editSession));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<HTMLEditor> htmlEditor = editSession->GetHTMLEditorForWindow(window);
  if (htmlEditor) {
    // We might already have an editor if it was set up for mail, let's see
    // if this is actually the case.
    uint32_t flags = 0;
    htmlEditor->GetFlags(&flags);
    if (flags & nsIPlaintextEditor::eEditorMailMask) {
      // We already have a mail editor, then we should not attempt to create
      // another one.
      return NS_OK;
    }
  }

  if (!HasPresShell(window)) {
    // We should not make the window editable or setup its editor.
    // It's probably style=display:none.
    return NS_OK;
  }

  bool makeWindowEditable = mEditingState == eOff;
  bool updateState = false;
  bool spellRecheckAll = false;
  bool putOffToRemoveScriptBlockerUntilModifyingEditingState = false;
  htmlEditor = nullptr;

  {
    EditingState oldState = mEditingState;
    nsAutoEditingState push(this, eSettingUp);

    RefPtr<PresShell> presShell = GetPresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    MOZ_ASSERT(mStyleSetFilled);

    // Before making this window editable, we need to modify UA style sheet
    // because new style may change whether focused element will be focusable
    // or not.
    AddContentEditableStyleSheetsToStyleSet(designMode);

    // Should we update the editable state of all the nodes in the document? We
    // need to do this when the designMode value changes, as that overrides
    // specific states on the elements.
    updateState = designMode || oldState == eDesignMode;
    if (designMode) {
      // designMode is being turned on (overrides contentEditable).
      spellRecheckAll = oldState == eContentEditable;
    }

    // Adjust focused element with new style but blur event shouldn't be fired
    // until mEditingState is modified with newState.
    nsAutoScriptBlocker scriptBlocker;
    if (designMode) {
      nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
      nsIContent* focusedContent = nsFocusManager::GetFocusedDescendant(
          window, nsFocusManager::eOnlyCurrentWindow,
          getter_AddRefs(focusedWindow));
      if (focusedContent) {
        nsIFrame* focusedFrame = focusedContent->GetPrimaryFrame();
        bool clearFocus = focusedFrame ? !focusedFrame->IsFocusable()
                                       : !focusedContent->IsFocusable();
        if (clearFocus) {
          nsFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm) {
            fm->ClearFocus(window);
            // If we need to dispatch blur event, we should put off after
            // modifying mEditingState since blur event handler may change
            // designMode state again.
            putOffToRemoveScriptBlockerUntilModifyingEditingState = true;
          }
        }
      }
    }

    if (makeWindowEditable) {
      // Editing is being turned on (through designMode or contentEditable)
      // Turn on editor.
      // XXX This can cause flushing which can change the editing state, so make
      //     sure to avoid recursing.
      rv = editSession->MakeWindowEditable(window, "html", false, false, true);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // XXX Need to call TearDownEditorOnWindow for all failures.
    htmlEditor = docshell->GetHTMLEditor();
    if (!htmlEditor) {
      return NS_ERROR_FAILURE;
    }

    // If we're entering the design mode, put the selection at the beginning of
    // the document for compatibility reasons.
    if (designMode && oldState == eOff) {
      htmlEditor->BeginningOfDocument();
    }

    if (putOffToRemoveScriptBlockerUntilModifyingEditingState) {
      nsContentUtils::AddScriptBlocker();
    }
  }

  mEditingState = newState;
  if (putOffToRemoveScriptBlockerUntilModifyingEditingState) {
    nsContentUtils::RemoveScriptBlocker();
    // If mEditingState is overwritten by another call and already disabled
    // the editing, we shouldn't keep making window editable.
    if (mEditingState == eOff) {
      return NS_OK;
    }
  }

  if (makeWindowEditable) {
    // Set the editor to not insert br's on return when in p
    // elements by default.
    // XXX Do we only want to do this for designMode?
    // Note that it doesn't matter what CallerType we pass, because the callee
    // doesn't use it for this command.  Play it safe and pass the more
    // restricted one.
    ErrorResult errorResult;
    nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
    Unused << ExecCommand(NS_LITERAL_STRING("insertBrOnReturn"), false,
                          NS_LITERAL_STRING("false"),
                          // Principal doesn't matter here, because the
                          // insertBrOnReturn command doesn't use it.   Still
                          // it's too bad we can't easily grab a nullprincipal
                          // from somewhere without allocating one..
                          *principal, errorResult);

    if (errorResult.Failed()) {
      // Editor setup failed. Editing is not on after all.
      // XXX Should we reset the editable flag on nodes?
      editSession->TearDownEditorOnWindow(window);
      mEditingState = eOff;

      return errorResult.StealNSResult();
    }
  }

  if (updateState) {
    nsAutoScriptBlocker scriptBlocker;
    NotifyEditableStateChange(this, this);
  }

  // Resync the editor's spellcheck state.
  if (spellRecheckAll) {
    nsCOMPtr<nsISelectionController> selectionController =
        htmlEditor->GetSelectionController();
    if (NS_WARN_IF(!selectionController)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<Selection> spellCheckSelection = selectionController->GetSelection(
        nsISelectionController::SELECTION_SPELLCHECK);
    if (spellCheckSelection) {
      spellCheckSelection->RemoveAllRanges(IgnoreErrors());
    }
  }
  htmlEditor->SyncRealTimeSpell();

  MaybeDispatchCheckKeyPressEventModelEvent();

  return NS_OK;
}

void nsHTMLDocument::MaybeDispatchCheckKeyPressEventModelEvent() {
  // Currently, we need to check only when we're becoming editable for
  // contenteditable.
  if (mEditingState != eContentEditable) {
    return;
  }

  if (mHasBeenEditable) {
    return;
  }
  mHasBeenEditable = true;

  // Dispatch "CheckKeyPressEventModel" event.  That is handled only by
  // KeyPressEventModelCheckerChild.  Then, it calls SetKeyPressEventModel()
  // with proper keypress event for the active web app.
  WidgetEvent checkEvent(true, eUnidentifiedEvent);
  checkEvent.mSpecifiedEventType = nsGkAtoms::onCheckKeyPressEventModel;
  checkEvent.mFlags.mCancelable = false;
  checkEvent.mFlags.mBubbles = false;
  checkEvent.mFlags.mOnlySystemGroupDispatch = true;
  // Post the event rather than dispatching it synchronously because we need
  // a call of SetKeyPressEventModel() before first key input.  Therefore, we
  // can avoid paying unnecessary runtime cost for most web apps.
  (new AsyncEventDispatcher(this, checkEvent))->PostDOMEvent();
}

void nsHTMLDocument::SetKeyPressEventModel(uint16_t aKeyPressEventModel) {
  PresShell* presShell = GetPresShell();
  if (!presShell) {
    return;
  }
  presShell->SetKeyPressEventModel(aKeyPressEventModel);
}

void nsHTMLDocument::SetDesignMode(const nsAString& aDesignMode,
                                   nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& rv) {
  SetDesignMode(aDesignMode, Some(&aSubjectPrincipal), rv);
}

void nsHTMLDocument::SetDesignMode(
    const nsAString& aDesignMode, const Maybe<nsIPrincipal*>& aSubjectPrincipal,
    ErrorResult& rv) {
  if (aSubjectPrincipal.isSome() &&
      !aSubjectPrincipal.value()->Subsumes(NodePrincipal())) {
    rv.Throw(NS_ERROR_DOM_PROP_ACCESS_DENIED);
    return;
  }
  bool editableMode = HasFlag(NODE_IS_EDITABLE);
  if (aDesignMode.LowerCaseEqualsASCII(editableMode ? "off" : "on")) {
    SetEditableFlag(!editableMode);

    rv = EditingStateChanged();
  }
}

nsCommandManager* nsHTMLDocument::GetMidasCommandManager() {
  // check if we have it cached
  if (mMidasCommandManager) {
    return mMidasCommandManager;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    return nullptr;
  }

  nsIDocShell* docshell = window->GetDocShell();
  if (!docshell) {
    return nullptr;
  }

  mMidasCommandManager = docshell->GetCommandManager();
  return mMidasCommandManager;
}

struct MidasCommand {
  const char* incomingCommandString;
  const char* internalCommandString;
  const char* internalParamString;
  bool useNewParam;
  bool convertToBoolean;
};

static const struct MidasCommand gMidasCommandTable[] = {
    // clang-format off
  { "bold",          "cmd_bold",            "", true,  false },
  { "italic",        "cmd_italic",          "", true,  false },
  { "underline",     "cmd_underline",       "", true,  false },
  { "strikethrough", "cmd_strikethrough",   "", true,  false },
  { "subscript",     "cmd_subscript",       "", true,  false },
  { "superscript",   "cmd_superscript",     "", true,  false },
  { "cut",           "cmd_cut",             "", true,  false },
  { "copy",          "cmd_copy",            "", true,  false },
  { "paste",         "cmd_paste",           "", true,  false },
  { "delete",        "cmd_deleteCharBackward", "", true,  false },
  { "forwarddelete", "cmd_deleteCharForward", "", true,  false },
  { "selectall",     "cmd_selectAll",       "", true,  false },
  { "undo",          "cmd_undo",            "", true,  false },
  { "redo",          "cmd_redo",            "", true,  false },
  { "indent",        "cmd_indent",          "", true,  false },
  { "outdent",       "cmd_outdent",         "", true,  false },
  { "backcolor",     "cmd_highlight",       "", false, false },
  { "forecolor",     "cmd_fontColor",       "", false, false },
  { "hilitecolor",   "cmd_highlight",       "", false, false },
  { "fontname",      "cmd_fontFace",        "", false, false },
  { "fontsize",      "cmd_fontSize",        "", false, false },
  { "increasefontsize", "cmd_increaseFont", "", false, false },
  { "decreasefontsize", "cmd_decreaseFont", "", false, false },
  { "inserthorizontalrule", "cmd_insertHR", "", true,  false },
  { "createlink",    "cmd_insertLinkNoUI",  "", false, false },
  { "insertimage",   "cmd_insertImageNoUI", "", false, false },
  { "inserthtml",    "cmd_insertHTML",      "", false, false },
  { "inserttext",    "cmd_insertText",      "", false, false },
  { "gethtml",       "cmd_getContents",     "", false, false },
  { "justifyleft",   "cmd_align",       "left", true,  false },
  { "justifyright",  "cmd_align",      "right", true,  false },
  { "justifycenter", "cmd_align",     "center", true,  false },
  { "justifyfull",   "cmd_align",    "justify", true,  false },
  { "removeformat",  "cmd_removeStyles",    "", true,  false },
  { "unlink",        "cmd_removeLinks",     "", true,  false },
  { "insertorderedlist",   "cmd_ol",        "", true,  false },
  { "insertunorderedlist", "cmd_ul",        "", true,  false },
  { "insertparagraph", "cmd_insertParagraph", "", true,  false },
  { "insertlinebreak", "cmd_insertLineBreak", "", true,  false },
  { "formatblock",   "cmd_paragraphState",  "", false, false },
  { "heading",       "cmd_paragraphState",  "", false, false },
  { "styleWithCSS",  "cmd_setDocumentUseCSS", "", false, true },
  { "contentReadOnly", "cmd_setDocumentReadOnly", "", false, true },
  { "insertBrOnReturn", "cmd_insertBrOnReturn", "", false, true },
  { "defaultParagraphSeparator", "cmd_defaultParagraphSeparator", "", false, false },
  { "enableObjectResizing", "cmd_enableObjectResizing", "", false, true },
  { "enableInlineTableEditing", "cmd_enableInlineTableEditing", "", false, true },
  { "enableAbsolutePositionEditing", "cmd_enableAbsolutePositionEditing", "", false, true },
#if 0
// no editor support to remove alignments right now
  { "justifynone",   "cmd_align",           "", true,  false },

// the following will need special review before being turned on
  { "saveas",        "cmd_saveAs",          "", true,  false },
  { "print",         "cmd_print",           "", true,  false },
#endif
  { nullptr, nullptr, nullptr, false, false }
    // clang-format on
};

#define MidasCommandCount \
  ((sizeof(gMidasCommandTable) / sizeof(struct MidasCommand)) - 1)

static const char* const gBlocks[] = {
    // clang-format off
  "ADDRESS",
  "BLOCKQUOTE",
  "DD",
  "DIV",
  "DL",
  "DT",
  "H1",
  "H2",
  "H3",
  "H4",
  "H5",
  "H6",
  "P",
  "PRE"
    // clang-format on
};

static bool ConvertToMidasInternalCommandInner(
    const nsAString& inCommandID, const nsAString& inParam,
    nsACString& outCommandID, nsACString& outParam, bool& outIsBoolean,
    bool& outBooleanValue, bool aIgnoreParams) {
  NS_ConvertUTF16toUTF8 convertedCommandID(inCommandID);

  // Hack to support old boolean commands that were backwards (see bug 301490).
  bool invertBool = false;
  if (convertedCommandID.LowerCaseEqualsLiteral("usecss")) {
    convertedCommandID.AssignLiteral("styleWithCSS");
    invertBool = true;
  } else if (convertedCommandID.LowerCaseEqualsLiteral("readonly")) {
    convertedCommandID.AssignLiteral("contentReadOnly");
    invertBool = true;
  }

  uint32_t i;
  bool found = false;
  for (i = 0; i < MidasCommandCount; ++i) {
    if (convertedCommandID.Equals(gMidasCommandTable[i].incomingCommandString,
                                  nsCaseInsensitiveCStringComparator())) {
      found = true;
      break;
    }
  }

  if (!found) {
    // reset results if the command is not found in our table
    outCommandID.SetLength(0);
    outParam.SetLength(0);
    outIsBoolean = false;
    return false;
  }

  // set outCommandID (what we use internally)
  outCommandID.Assign(gMidasCommandTable[i].internalCommandString);

  // set outParam & outIsBoolean based on flags from the table
  outIsBoolean = gMidasCommandTable[i].convertToBoolean;

  if (aIgnoreParams) {
    // No further work to do
    return true;
  }

  if (gMidasCommandTable[i].useNewParam) {
    // Just have to copy it, no checking
    outParam.Assign(gMidasCommandTable[i].internalParamString);
    return true;
  }

  // handle checking of param passed in
  if (outIsBoolean) {
    // If this is a boolean value and it's not explicitly false (e.g. no value)
    // we default to "true". For old backwards commands we invert the check (see
    // bug 301490).
    if (invertBool) {
      outBooleanValue = inParam.LowerCaseEqualsLiteral("false");
    } else {
      outBooleanValue = !inParam.LowerCaseEqualsLiteral("false");
    }
    outParam.Truncate();

    return true;
  }

  // String parameter -- see if we need to convert it (necessary for
  // cmd_paragraphState and cmd_fontSize)
  if (outCommandID.EqualsLiteral("cmd_paragraphState")) {
    const char16_t* start = inParam.BeginReading();
    const char16_t* end = inParam.EndReading();
    if (start != end && *start == '<' && *(end - 1) == '>') {
      ++start;
      --end;
    }

    NS_ConvertUTF16toUTF8 convertedParam(Substring(start, end));
    uint32_t j;
    for (j = 0; j < ArrayLength(gBlocks); ++j) {
      if (convertedParam.Equals(gBlocks[j],
                                nsCaseInsensitiveCStringComparator())) {
        outParam.Assign(gBlocks[j]);
        break;
      }
    }

    if (j == ArrayLength(gBlocks)) {
      outParam.Truncate();
    }
  } else if (outCommandID.EqualsLiteral("cmd_fontSize")) {
    // Per editing spec as of April 23, 2012, we need to reject the value if
    // it's not a valid floating-point number surrounded by optional whitespace.
    // Otherwise, we parse it as a legacy font size.  For now, we just parse as
    // a legacy font size regardless (matching WebKit) -- bug 747879.
    outParam.Truncate();
    int32_t size = nsContentUtils::ParseLegacyFontSize(inParam);
    if (size) {
      outParam.AppendInt(size);
    }
  } else {
    CopyUTF16toUTF8(inParam, outParam);
  }

  return true;
}

static bool ConvertToMidasInternalCommand(const nsAString& inCommandID,
                                          const nsAString& inParam,
                                          nsACString& outCommandID,
                                          nsACString& outParam,
                                          bool& outIsBoolean,
                                          bool& outBooleanValue) {
  return ConvertToMidasInternalCommandInner(inCommandID, inParam, outCommandID,
                                            outParam, outIsBoolean,
                                            outBooleanValue, false);
}

static bool ConvertToMidasInternalCommand(const nsAString& inCommandID,
                                          nsACString& outCommandID) {
  nsAutoCString dummyCString;
  nsAutoString dummyString;
  bool dummyBool;
  return ConvertToMidasInternalCommandInner(inCommandID, dummyString,
                                            outCommandID, dummyCString,
                                            dummyBool, dummyBool, true);
}

bool nsHTMLDocument::ExecCommand(const nsAString& commandID, bool doShowUI,
                                 const nsAString& value,
                                 nsIPrincipal& aSubjectPrincipal,
                                 ErrorResult& rv) {
  //  for optional parameters see dom/src/base/nsHistory.cpp: HistoryImpl::Go()
  //  this might add some ugly JS dependencies?

  nsAutoCString cmdToDispatch, paramStr;
  bool isBool, boolVal;
  if (!ConvertToMidasInternalCommand(commandID, value, cmdToDispatch, paramStr,
                                     isBool, boolVal)) {
    return false;
  }

  bool isCutCopy = (commandID.LowerCaseEqualsLiteral("cut") ||
                    commandID.LowerCaseEqualsLiteral("copy"));
  bool isPaste = commandID.LowerCaseEqualsLiteral("paste");

  // if editing is not on, bail
  if (!isCutCopy && !isPaste && !IsEditingOnAfterFlush()) {
    return false;
  }

  // if they are requesting UI from us, let's fail since we have no UI
  if (doShowUI) {
    return false;
  }

  // special case for cut & copy
  // cut & copy are allowed in non editable documents
  if (isCutCopy) {
    if (!nsContentUtils::IsCutCopyAllowed(&aSubjectPrincipal)) {
      // We have rejected the event due to it not being performed in an
      // input-driven context therefore, we report the error to the console.
      nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                      NS_LITERAL_CSTRING("DOM"), this,
                                      nsContentUtils::eDOM_PROPERTIES,
                                      "ExecCommandCutCopyDeniedNotInputDriven");
      return false;
    }

    // For cut & copy commands, we need the behaviour from
    // nsWindowRoot::GetControllers which is to look at the focused element, and
    // defer to a focused textbox's controller The code past taken by other
    // commands in ExecCommand always uses the window directly, rather than
    // deferring to the textbox, which is desireable for most editor commands,
    // but not 'cut' and 'copy' (as those should allow copying out of embedded
    // editors). This behaviour is invoked if we call DoCommand directly on the
    // docShell.
    nsCOMPtr<nsIDocShell> docShell(mDocumentContainer);
    if (docShell) {
      nsresult res = docShell->DoCommand(cmdToDispatch.get());
      if (res == NS_SUCCESS_DOM_NO_OPERATION) {
        return false;
      }
      return NS_SUCCEEDED(res);
    }
    return false;
  }

  if (commandID.LowerCaseEqualsLiteral("gethtml")) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (isPaste && !nsContentUtils::PrincipalHasPermission(
                     &aSubjectPrincipal, nsGkAtoms::clipboardRead)) {
    return false;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (!window) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if ((cmdToDispatch.EqualsLiteral("cmd_fontSize") ||
       cmdToDispatch.EqualsLiteral("cmd_insertImageNoUI") ||
       cmdToDispatch.EqualsLiteral("cmd_insertLinkNoUI") ||
       cmdToDispatch.EqualsLiteral("cmd_paragraphState")) &&
      paramStr.IsEmpty()) {
    // Invalid value, return false
    return false;
  }

  if (cmdToDispatch.EqualsLiteral("cmd_defaultParagraphSeparator") &&
      !paramStr.LowerCaseEqualsLiteral("div") &&
      !paramStr.LowerCaseEqualsLiteral("p") &&
      !paramStr.LowerCaseEqualsLiteral("br")) {
    // Invalid value
    return false;
  }

  // Return false for disabled commands (bug 760052)
  if (!commandManager->IsCommandEnabled(cmdToDispatch, window)) {
    return false;
  }

  if (!isBool && paramStr.IsEmpty()) {
    rv = commandManager->DoCommand(cmdToDispatch.get(), nullptr, window);
  } else {
    // we have a command that requires a parameter, create params
    RefPtr<nsCommandParams> params = new nsCommandParams();
    if (isBool) {
      rv = params->SetBool("state_attribute", boolVal);
    } else if (cmdToDispatch.EqualsLiteral("cmd_fontFace") ||
               cmdToDispatch.EqualsLiteral("cmd_insertImageNoUI") ||
               cmdToDispatch.EqualsLiteral("cmd_insertLinkNoUI")) {
      rv = params->SetString("state_attribute", value);
    } else if (cmdToDispatch.EqualsLiteral("cmd_insertHTML") ||
               cmdToDispatch.EqualsLiteral("cmd_insertText")) {
      rv = params->SetString("state_data", value);
    } else {
      rv = params->SetCString("state_attribute", paramStr);
    }
    if (rv.Failed()) {
      return false;
    }
    rv = commandManager->DoCommand(cmdToDispatch.get(), params, window);
  }

  return !rv.Failed();
}

bool nsHTMLDocument::QueryCommandEnabled(const nsAString& commandID,
                                         nsIPrincipal& aSubjectPrincipal,
                                         ErrorResult& rv) {
  nsAutoCString cmdToDispatch;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch)) {
    return false;
  }

  // cut & copy are always allowed
  bool isCutCopy = commandID.LowerCaseEqualsLiteral("cut") ||
                   commandID.LowerCaseEqualsLiteral("copy");
  if (isCutCopy) {
    return nsContentUtils::IsCutCopyAllowed(&aSubjectPrincipal);
  }

  // Report false for restricted commands
  bool restricted = commandID.LowerCaseEqualsLiteral("paste");
  if (restricted && !nsContentUtils::IsSystemPrincipal(&aSubjectPrincipal)) {
    return false;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return false;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return commandManager->IsCommandEnabled(cmdToDispatch, window);
}

bool nsHTMLDocument::QueryCommandIndeterm(const nsAString& commandID,
                                          ErrorResult& rv) {
  nsAutoCString cmdToDispatch;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch)) {
    return false;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return false;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();
  rv = commandManager->GetCommandState(cmdToDispatch.get(), window, params);
  if (rv.Failed()) {
    return false;
  }

  // If command does not have a state_mixed value, this call fails and sets
  // retval to false.  This is fine -- we want to return false in that case
  // anyway (bug 738385), so we just don't throw regardless.
  return params->GetBool("state_mixed");
}

bool nsHTMLDocument::QueryCommandState(const nsAString& commandID,
                                       ErrorResult& rv) {
  nsAutoCString cmdToDispatch, paramToCheck;
  bool dummy, dummy2;
  if (!ConvertToMidasInternalCommand(commandID, commandID, cmdToDispatch,
                                     paramToCheck, dummy, dummy2)) {
    return false;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return false;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsPIDOMWindowOuter* window = GetWindow();
  if (!window) {
    rv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  if (commandID.LowerCaseEqualsLiteral("usecss")) {
    // Per spec, state is supported for styleWithCSS but not useCSS, so we just
    // return false always.
    return false;
  }

  RefPtr<nsCommandParams> params = new nsCommandParams();
  rv = commandManager->GetCommandState(cmdToDispatch.get(), window, params);
  if (rv.Failed()) {
    return false;
  }

  // handle alignment as a special case (possibly other commands too?)
  // Alignment is special because the external api is individual
  // commands but internally we use cmd_align with different
  // parameters.  When getting the state of this command, we need to
  // return the boolean for this particular alignment rather than the
  // string of 'which alignment is this?'
  if (cmdToDispatch.EqualsLiteral("cmd_align")) {
    nsAutoCString actualAlignmentType;
    rv = params->GetCString("state_attribute", actualAlignmentType);
    return !rv.Failed() && !actualAlignmentType.IsEmpty() &&
           paramToCheck == actualAlignmentType;
  }

  // If command does not have a state_all value, this call fails and sets
  // retval to false.  This is fine -- we want to return false in that case
  // anyway (bug 738385), so we just succeed and return false regardless.
  return params->GetBool("state_all");
}

bool nsHTMLDocument::QueryCommandSupported(const nsAString& commandID,
                                           CallerType aCallerType) {
  // Gecko technically supports all the clipboard commands including
  // cut/copy/paste, but non-privileged content will be unable to call
  // paste, and depending on the pref "dom.allow_cut_copy", cut and copy
  // may also be disallowed to be called from non-privileged content.
  // For that reason, we report the support status of corresponding
  // command accordingly.
  if (aCallerType != CallerType::System) {
    if (commandID.LowerCaseEqualsLiteral("paste")) {
      return false;
    }
    if (!StaticPrefs::dom_allow_cut_copy()) {
      // XXXbz should we worry about correctly reporting "true" in the
      // "restricted, but we're an addon with clipboardWrite permissions" case?
      // See also nsContentUtils::IsCutCopyAllowed.
      if (commandID.LowerCaseEqualsLiteral("cut") ||
          commandID.LowerCaseEqualsLiteral("copy")) {
        return false;
      }
    }
  }

  // commandID is supported if it can be converted to a Midas command
  nsAutoCString cmdToDispatch;
  return ConvertToMidasInternalCommand(commandID, cmdToDispatch);
}

void nsHTMLDocument::QueryCommandValue(const nsAString& commandID,
                                       nsAString& aValue, ErrorResult& rv) {
  aValue.Truncate();

  nsAutoCString cmdToDispatch, paramStr;
  if (!ConvertToMidasInternalCommand(commandID, cmdToDispatch)) {
    // Return empty string
    return;
  }

  // if editing is not on, bail
  if (!IsEditingOnAfterFlush()) {
    return;
  }

  // get command manager and dispatch command to our window if it's acceptable
  RefPtr<nsCommandManager> commandManager = GetMidasCommandManager();
  if (!commandManager) {
    rv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = GetWindow();
  if (!window) {
    rv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // this is a special command since we are calling DoCommand rather than
  // GetCommandState like the other commands
  RefPtr<nsCommandParams> params = new nsCommandParams();
  if (cmdToDispatch.EqualsLiteral("cmd_getContents")) {
    rv = params->SetBool("selection_only", true);
    if (rv.Failed()) {
      return;
    }
    rv = params->SetCString("format", NS_LITERAL_CSTRING("text/html"));
    if (rv.Failed()) {
      return;
    }
    rv = commandManager->DoCommand(cmdToDispatch.get(), params, window);
    if (rv.Failed()) {
      return;
    }
    params->GetString("result", aValue);
    return;
  }

  rv = params->SetCString("state_attribute", paramStr);
  if (rv.Failed()) {
    return;
  }

  rv = commandManager->GetCommandState(cmdToDispatch.get(), window, params);
  if (rv.Failed()) {
    return;
  }

  // If command does not have a state_attribute value, this call fails, and
  // aValue will wind up being the empty string.  This is fine -- we want to
  // return "" in that case anyway (bug 738385), so we just return NS_OK
  // regardless.
  nsAutoCString result;
  params->GetCString("state_attribute", result);
  CopyUTF8toUTF16(result, aValue);
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

bool nsHTMLDocument::IsEditingOnAfterFlush() {
  Document* doc = GetParentDocument();
  if (doc) {
    // Make sure frames are up to date, since that can affect whether
    // we're editable.
    doc->FlushPendingNotifications(FlushType::Frames);
  }

  return IsEditingOn();
}

void nsHTMLDocument::RemovedFromDocShell() {
  mEditingState = eOff;
  Document::RemovedFromDocShell();
}

/* virtual */
void nsHTMLDocument::DocAddSizeOfExcludingThis(
    nsWindowSizes& aWindowSizes) const {
  Document::DocAddSizeOfExcludingThis(aWindowSizes);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mLinks
  // - mAnchors
  // - mMidasCommandManager
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
    bool schemeIs = false;
    uri->SchemeIs("about", &schemeIs);
    if (schemeIs) {
      return true;
    }
    bool isResource;
    nsresult rv = NS_URIChainHasFlags(
        uri, nsIProtocolHandler::URI_IS_UI_RESOURCE, &isResource);
    if (NS_FAILED(rv) || isResource) {
      return true;
    }
  }
  return false;
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

void nsHTMLDocument::UserInteractionForTesting() { SetUserHasInteracted(); }

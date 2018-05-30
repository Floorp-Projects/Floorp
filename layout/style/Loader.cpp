/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* loading of CSS style sheets using the network APIs */

#include "mozilla/css/Loader.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/SRILogHelper.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Logging.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/URLPreloader.h"
#include "nsIRunnable.h"
#include "nsSyncLoadService.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentPolicyUtils.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIClassOfService.h"
#include "nsIScriptError.h"
#include "nsMimeTypes.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsICSSLoaderObserver.h"
#include "nsThreadUtils.h"
#include "nsGkAtoms.h"
#include "nsIThreadInternal.h"
#include "nsINetworkPredictor.h"
#include "nsStringStream.h"
#include "mozilla/dom/MediaList.h"
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/URL.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/css/StreamLoader.h"

#ifdef MOZ_XUL
#include "nsXULPrototypeCache.h"
#endif

#include "nsError.h"

#include "nsIContentSecurityPolicy.h"
#include "mozilla/dom/SRICheck.h"

#include "mozilla/Encoding.h"

using namespace mozilla::dom;

// 1024 bytes is specified in https://drafts.csswg.org/css-syntax/
#define SNIFFING_BUFFER_SIZE 1024

/**
 * OVERALL ARCHITECTURE
 *
 * The CSS Loader gets requests to load various sorts of style sheets:
 * inline style from <style> elements, linked style, @import-ed child
 * sheets, non-document sheets.  The loader handles the following tasks:
 * 1) Creation of the actual style sheet objects: CreateSheet()
 * 2) setting of the right media, title, enabled state, etc on the
 *    sheet: PrepareSheet()
 * 3) Insertion of the sheet in the proper cascade order:
 *    InsertSheetInDoc() and InsertChildSheet()
 * 4) Load of the sheet: LoadSheet() including security checks
 * 5) Parsing of the sheet: ParseSheet()
 * 6) Cleanup: SheetComplete()
 *
 * The detailed documentation for these functions is found with the
 * function implementations.
 *
 * The following helper object is used:
 *    SheetLoadData -- a small class that is used to store all the
 *                     information needed for the loading of a sheet;
 *                     this class handles listening for the stream
 *                     loader completion and also handles charset
 *                     determination.
 */

namespace mozilla {
namespace css {

#include "mozilla/Logging.h"

static mozilla::LazyLogModule sCssLoaderLog("nsCSSLoader");

static mozilla::LazyLogModule gSriPRLog("SRI");

#define LOG_ERROR(args) MOZ_LOG(sCssLoaderLog, mozilla::LogLevel::Error, args)
#define LOG_WARN(args) MOZ_LOG(sCssLoaderLog, mozilla::LogLevel::Warning, args)
#define LOG_DEBUG(args) MOZ_LOG(sCssLoaderLog, mozilla::LogLevel::Debug, args)
#define LOG(args) LOG_DEBUG(args)

#define LOG_ERROR_ENABLED() MOZ_LOG_TEST(sCssLoaderLog, mozilla::LogLevel::Error)
#define LOG_WARN_ENABLED() MOZ_LOG_TEST(sCssLoaderLog, mozilla::LogLevel::Warning)
#define LOG_DEBUG_ENABLED() MOZ_LOG_TEST(sCssLoaderLog, mozilla::LogLevel::Debug)
#define LOG_ENABLED() LOG_DEBUG_ENABLED()

#define LOG_URI(format, uri)                        \
  PR_BEGIN_MACRO                                    \
    NS_ASSERTION(uri, "Logging null uri");          \
    if (LOG_ENABLED()) {                            \
      LOG((format, uri->GetSpecOrDefault().get())); \
    }                                               \
  PR_END_MACRO

// And some convenience strings...
static const char* const gStateStrings[] = {
  "eSheetStateUnknown",
  "eSheetNeedsParser",
  "eSheetPending",
  "eSheetLoading",
  "eSheetComplete"
};

/********************************
 * SheetLoadData implementation *
 ********************************/
NS_IMPL_ISUPPORTS(SheetLoadData, nsIRunnable,
                  nsIThreadObserver)

SheetLoadData::SheetLoadData(Loader* aLoader,
                             const nsAString& aTitle,
                             nsIURI* aURI,
                             StyleSheet* aSheet,
                             nsIStyleSheetLinkingElement* aOwningElement,
                             IsAlternate aIsAlternate,
                             MediaMatched aMediaMatches,
                             nsICSSLoaderObserver* aObserver,
                             nsIPrincipal* aLoaderPrincipal,
                             nsINode* aRequestingNode)
  : mLoader(aLoader)
  , mTitle(aTitle)
  , mEncoding(nullptr)
  , mURI(aURI)
  , mLineNumber(1)
  , mSheet(aSheet)
  , mNext(nullptr)
  , mPendingChildren(0)
  , mSyncLoad(false)
  , mIsNonDocumentSheet(false)
  , mIsLoading(false)
  , mIsBeingParsed(false)
  , mIsCancelled(false)
  , mMustNotify(false)
  , mWasAlternate(aIsAlternate == IsAlternate::Yes)
  , mMediaMatched(aMediaMatches == MediaMatched::Yes)
  , mUseSystemPrincipal(false)
  , mSheetAlreadyComplete(false)
  , mIsCrossOriginNoCORS(false)
  , mBlockResourceTiming(false)
  , mLoadFailed(false)
  , mOwningElement(aOwningElement)
  , mObserver(aObserver)
  , mLoaderPrincipal(aLoaderPrincipal)
  , mRequestingNode(aRequestingNode)
  , mPreloadEncoding(nullptr)
{
  MOZ_ASSERT(mLoader, "Must have a loader!");
}

SheetLoadData::SheetLoadData(Loader* aLoader,
                             nsIURI* aURI,
                             StyleSheet* aSheet,
                             SheetLoadData* aParentData,
                             nsICSSLoaderObserver* aObserver,
                             nsIPrincipal* aLoaderPrincipal,
                             nsINode* aRequestingNode)
  : mLoader(aLoader)
  , mEncoding(nullptr)
  , mURI(aURI)
  , mLineNumber(1)
  , mSheet(aSheet)
  , mNext(nullptr)
  , mParentData(aParentData)
  , mPendingChildren(0)
  , mSyncLoad(false)
  , mIsNonDocumentSheet(false)
  , mIsLoading(false)
  , mIsBeingParsed(false)
  , mIsCancelled(false)
  , mMustNotify(false)
  , mWasAlternate(false)
  , mMediaMatched(true)
  , mUseSystemPrincipal(false)
  , mSheetAlreadyComplete(false)
  , mIsCrossOriginNoCORS(false)
  , mBlockResourceTiming(false)
  , mLoadFailed(false)
  , mOwningElement(nullptr)
  , mObserver(aObserver)
  , mLoaderPrincipal(aLoaderPrincipal)
  , mRequestingNode(aRequestingNode)
  , mPreloadEncoding(nullptr)
{
  MOZ_ASSERT(mLoader, "Must have a loader!");
  if (mParentData) {
    mSyncLoad = mParentData->mSyncLoad;
    mIsNonDocumentSheet = mParentData->mIsNonDocumentSheet;
    mUseSystemPrincipal = mParentData->mUseSystemPrincipal;
    ++(mParentData->mPendingChildren);
  }

  MOZ_ASSERT(!mUseSystemPrincipal || mSyncLoad,
             "Shouldn't use system principal for async loads");
}

SheetLoadData::SheetLoadData(Loader* aLoader,
                             nsIURI* aURI,
                             StyleSheet* aSheet,
                             bool aSyncLoad,
                             bool aUseSystemPrincipal,
                             const Encoding* aPreloadEncoding,
                             nsICSSLoaderObserver* aObserver,
                             nsIPrincipal* aLoaderPrincipal,
                             nsINode* aRequestingNode)
  : mLoader(aLoader)
  , mEncoding(nullptr)
  , mURI(aURI)
  , mLineNumber(1)
  , mSheet(aSheet)
  , mNext(nullptr)
  , mPendingChildren(0)
  , mSyncLoad(aSyncLoad)
  , mIsNonDocumentSheet(true)
  , mIsLoading(false)
  , mIsBeingParsed(false)
  , mIsCancelled(false)
  , mMustNotify(false)
  , mWasAlternate(false)
  , mMediaMatched(true)
  , mUseSystemPrincipal(aUseSystemPrincipal)
  , mSheetAlreadyComplete(false)
  , mIsCrossOriginNoCORS(false)
  , mBlockResourceTiming(false)
  , mLoadFailed(false)
  , mOwningElement(nullptr)
  , mObserver(aObserver)
  , mLoaderPrincipal(aLoaderPrincipal)
  , mRequestingNode(aRequestingNode)
  , mPreloadEncoding(aPreloadEncoding)
{
  MOZ_ASSERT(mLoader, "Must have a loader!");
  MOZ_ASSERT(!mUseSystemPrincipal || mSyncLoad,
             "Shouldn't use system principal for async loads");
}

SheetLoadData::~SheetLoadData()
{
  NS_CSS_NS_RELEASE_LIST_MEMBER(SheetLoadData, this, mNext);
}

NS_IMETHODIMP
SheetLoadData::Run()
{
  mLoader->HandleLoadEvent(this);
  return NS_OK;
}

NS_IMETHODIMP
SheetLoadData::OnDispatchedEvent()
{
  return NS_OK;
}

NS_IMETHODIMP
SheetLoadData::OnProcessNextEvent(nsIThreadInternal* aThread,
                                  bool aMayWait)
{
  // XXXkhuey this is insane!
  // We want to fire our load even before or after event processing,
  // whichever comes first.
  FireLoadEvent(aThread);
  return NS_OK;
}

NS_IMETHODIMP
SheetLoadData::AfterProcessNextEvent(nsIThreadInternal* aThread,
                                     bool aEventWasProcessed)
{
  // XXXkhuey this too!
  // We want to fire our load even before or after event processing,
  // whichever comes first.
  FireLoadEvent(aThread);
  return NS_OK;
}

void
SheetLoadData::FireLoadEvent(nsIThreadInternal* aThread)
{

  // First remove ourselves as a thread observer.  But we need to keep
  // ourselves alive while doing that!
  RefPtr<SheetLoadData> kungFuDeathGrip(this);
  aThread->RemoveObserver(this);

  // Now fire the event
  nsCOMPtr<nsINode> node = do_QueryInterface(mOwningElement);
  NS_ASSERTION(node, "How did that happen???");

  nsContentUtils::DispatchTrustedEvent(node->OwnerDoc(),
                                       node,
                                       mLoadFailed ?
                                         NS_LITERAL_STRING("error") :
                                         NS_LITERAL_STRING("load"),
                                       false, false);

  // And unblock onload
  mLoader->UnblockOnload(true);
}

void
SheetLoadData::ScheduleLoadEventIfNeeded()
{
  if (!mOwningElement) {
    return;
  }

  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  nsCOMPtr<nsIThreadInternal> internalThread = do_QueryInterface(thread);
  if (NS_SUCCEEDED(internalThread->AddObserver(this))) {
    // Make sure to block onload here
    mLoader->BlockOnload();
  }
}

/*********************
 * Style sheet reuse *
 *********************/

bool
LoaderReusableStyleSheets::FindReusableStyleSheet(nsIURI* aURL,
                                                  RefPtr<StyleSheet>& aResult)
{
  MOZ_ASSERT(aURL);
  for (size_t i = mReusableSheets.Length(); i > 0; --i) {
    size_t index = i - 1;
    bool sameURI;
    MOZ_ASSERT(mReusableSheets[index]->GetOriginalURI());
    nsresult rv = aURL->Equals(mReusableSheets[index]->GetOriginalURI(),
                               &sameURI);
    if (!NS_FAILED(rv) && sameURI) {
      aResult = mReusableSheets[index];
      mReusableSheets.RemoveElementAt(index);
      return true;
    }
  }
  return false;
}

/*************************
 * Loader Implementation *
 *************************/

Loader::Loader()
  : mDocument(nullptr)
  , mDatasToNotifyOn(0)
  , mCompatMode(eCompatibility_FullStandards)
  , mEnabled(true)
  , mReporter(new ConsoleReportCollector())
#ifdef DEBUG
  , mSyncCallback(false)
#endif
{
}

Loader::Loader(DocGroup* aDocGroup)
  : Loader()
{
  mDocGroup = aDocGroup;
}

Loader::Loader(nsIDocument* aDocument)
  : Loader()
{
  mDocument = aDocument;
  MOZ_ASSERT(mDocument, "We should get a valid document from the caller!");
}

Loader::~Loader()
{
  NS_ASSERTION(!mSheets || mSheets->mLoadingDatas.Count() == 0,
               "How did we get destroyed when there are loading data?");
  NS_ASSERTION(!mSheets || mSheets->mPendingDatas.Count() == 0,
               "How did we get destroyed when there are pending data?");
  // Note: no real need to revoke our stylesheet loaded events -- they
  // hold strong references to us, so if we're going away that means
  // they're all done.
}

void
Loader::DropDocumentReference(void)
{
  mDocument = nullptr;
  // Flush out pending datas just so we don't leak by accident.  These
  // loads should short-circuit through the mDocument check in
  // LoadSheet and just end up in SheetComplete immediately
  if (mSheets) {
    StartDeferredLoads();
  }
}

void
Loader::DocumentStyleSheetSetChanged()
{
  MOZ_ASSERT(mDocument);

  // start any pending alternates that aren't alternates anymore
  if (!mSheets) {
    return;
  }

  LoadDataArray arr(mSheets->mPendingDatas.Count());
  for (auto iter = mSheets->mPendingDatas.Iter(); !iter.Done(); iter.Next()) {
    SheetLoadData* data = iter.Data();
    MOZ_ASSERT(data, "Must have a data");

    // Note that we don't want to affect what the selected style set is, so
    // use true for aHasAlternateRel.
    auto isAlternate = data->mLoader->IsAlternateSheet(data->mTitle, true);
    if (isAlternate == IsAlternate::No) {
      arr.AppendElement(data);
      iter.Remove();
    }
  }

  mDatasToNotifyOn += arr.Length();
  for (uint32_t i = 0; i < arr.Length(); ++i) {
    --mDatasToNotifyOn;
    LoadSheet(arr[i], eSheetNeedsParser, false);
  }
}

static const char kCharsetSym[] = "@charset \"";

static bool GetCharsetFromData(const char* aStyleSheetData,
                               uint32_t aDataLength,
                               nsACString& aCharset)
{
  aCharset.Truncate();
  if (aDataLength <= sizeof(kCharsetSym) - 1)
    return false;

  if (strncmp(aStyleSheetData,
              kCharsetSym,
              sizeof(kCharsetSym) - 1)) {
    return false;
  }

  for (uint32_t i = sizeof(kCharsetSym) - 1; i < aDataLength; ++i) {
    char c = aStyleSheetData[i];
    if (c == '"') {
      ++i;
      if (i < aDataLength && aStyleSheetData[i] == ';') {
        return true;
      }
      // fail
      break;
    }
    aCharset.Append(c);
  }

  // Did not see end quote or semicolon
  aCharset.Truncate();
  return false;
}

NotNull<const Encoding*>
SheetLoadData::DetermineNonBOMEncoding(nsACString const& aSegment,
                                       nsIChannel* aChannel)
{
  const Encoding* encoding;
  nsAutoCString label;

  // Check HTTP
  if (aChannel && NS_SUCCEEDED(aChannel->GetContentCharset(label))) {
    encoding = Encoding::ForLabel(label);
    if (encoding) {
      return WrapNotNull(encoding);
    }
  }

  // Check @charset
  auto sniffingLength = aSegment.Length();
  if (sniffingLength > SNIFFING_BUFFER_SIZE) {
    sniffingLength = SNIFFING_BUFFER_SIZE;
  }
  if (GetCharsetFromData(aSegment.BeginReading(), sniffingLength, label)) {
    encoding = Encoding::ForLabel(label);
    if (encoding == UTF_16BE_ENCODING || encoding == UTF_16LE_ENCODING) {
      return UTF_8_ENCODING;
    }
    if (encoding) {
      return WrapNotNull(encoding);
    }
  }

  // Now try the charset on the <link> or processing instruction
  // that loaded us
  if (mOwningElement) {
    nsAutoString label16;
    mOwningElement->GetCharset(label16);
    encoding = Encoding::ForLabel(label16);
    if (encoding) {
      return WrapNotNull(encoding);
    }
  }

  // In the preload case, the value of the charset attribute on <link> comes
  // in via mPreloadEncoding instead.
  if (mPreloadEncoding) {
    return WrapNotNull(mPreloadEncoding);
  }

  // Try charset from the parent stylesheet.
  if (mParentData) {
    encoding = mParentData->mEncoding;
    if (encoding) {
      return WrapNotNull(encoding);
    }
  }

  if (mLoader->mDocument) {
    // Use the document charset.
    return mLoader->mDocument->GetDocumentCharacterSet();
  }

  return UTF_8_ENCODING;
}

already_AddRefed<nsIURI>
SheetLoadData::GetReferrerURI()
{
  nsCOMPtr<nsIURI> uri;
  if (mParentData)
    uri = mParentData->mSheet->GetSheetURI();
  if (!uri && mLoader->mDocument)
    uri = mLoader->mDocument->GetDocumentURI();
  return uri.forget();
}

static nsresult
VerifySheetIntegrity(const SRIMetadata& aMetadata,
                     nsIChannel* aChannel,
                     const nsACString& aFirst,
                     const nsACString& aSecond,
                     const nsACString& aSourceFileURI,
                     nsIConsoleReportCollector* aReporter)
{
  NS_ENSURE_ARG_POINTER(aReporter);

  if (MOZ_LOG_TEST(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug)) {
    nsAutoCString requestURL;
    nsCOMPtr<nsIURI> originalURI;
    if (aChannel &&
        NS_SUCCEEDED(aChannel->GetOriginalURI(getter_AddRefs(originalURI))) &&
        originalURI) {
      originalURI->GetAsciiSpec(requestURL);
    }
    MOZ_LOG(SRILogHelper::GetSriLog(), mozilla::LogLevel::Debug,
            ("VerifySheetIntegrity (unichar stream)"));
  }

  SRICheckDataVerifier verifier(aMetadata, aSourceFileURI, aReporter);
  nsresult rv =
    verifier.Update(aFirst.Length(), (const uint8_t*)aFirst.BeginReading());
  NS_ENSURE_SUCCESS(rv, rv);
  rv =
    verifier.Update(aSecond.Length(), (const uint8_t*)aSecond.BeginReading());
  NS_ENSURE_SUCCESS(rv, rv);

  return verifier.Verify(aMetadata, aChannel, aSourceFileURI, aReporter);
}

/*
 * Stream completion code shared by Stylo and the old style system.
 *
 * Here we need to check that the load did not give us an http error
 * page and check the mimetype on the channel to make sure we're not
 * loading non-text/css data in standards mode.
 */
nsresult
SheetLoadData::VerifySheetReadyToParse(nsresult aStatus,
                                       const nsACString& aBytes1,
                                       const nsACString& aBytes2,
                                       nsIChannel* aChannel)
{
  LOG(("SheetLoadData::OnStreamComplete"));
  NS_ASSERTION(!mLoader->mSyncCallback, "Synchronous callback from necko");

  if (mIsCancelled) {
    // Just return.  Don't call SheetComplete -- it's already been
    // called and calling it again will lead to an extra NS_RELEASE on
    // this data and a likely crash.
    return NS_OK;
  }

  if (!mLoader->mDocument && !mIsNonDocumentSheet) {
    // Sorry, we don't care about this load anymore
    LOG_WARN(("  No document and not non-document sheet; dropping load"));
    mLoader->SheetComplete(this, NS_BINDING_ABORTED);
    return NS_OK;
  }

  if (NS_FAILED(aStatus)) {
    LOG_WARN(("  Load failed: status 0x%" PRIx32, static_cast<uint32_t>(aStatus)));
    // Handle sheet not loading error because source was a tracking URL.
    // We make a note of this sheet node by including it in a dedicated
    // array of blocked tracking nodes under its parent document.
    //
    // Multiple sheet load instances might be tied to this request,
    // we annotate each one linked to a valid owning element (node).
    if (aStatus == NS_ERROR_TRACKING_URI) {
      nsIDocument* doc = mLoader->GetDocument();
      if (doc) {
        for (SheetLoadData* data = this; data; data = data->mNext) {
          // mOwningElement may be null but AddBlockTrackingNode can cope
          nsCOMPtr<nsIContent> content = do_QueryInterface(data->mOwningElement);
          doc->AddBlockedTrackingNode(content);
        }
      }
    }
    mLoader->SheetComplete(this, aStatus);
    return NS_OK;
  }

  if (!aChannel) {
    mLoader->SheetComplete(this, NS_OK);
    return NS_OK;
  }

  nsCOMPtr<nsIURI> originalURI;
  aChannel->GetOriginalURI(getter_AddRefs(originalURI));

  // If the channel's original URI is "chrome:", we want that, since
  // the observer code in nsXULPrototypeCache depends on chrome stylesheets
  // having a chrome URI.  (Whether or not chrome stylesheets come through
  // this codepath seems nondeterministic.)
  // Otherwise we want the potentially-HTTP-redirected URI.
  nsCOMPtr<nsIURI> channelURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));

  if (!channelURI || !originalURI) {
    NS_ERROR("Someone just violated the nsIRequest contract");
    LOG_WARN(("  Channel without a URI.  Bad!"));
    mLoader->SheetComplete(this, NS_ERROR_UNEXPECTED);
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> principal;
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  nsresult result = NS_ERROR_NOT_AVAILABLE;
  if (secMan) {  // Could be null if we already shut down
    if (mUseSystemPrincipal) {
      result = secMan->GetSystemPrincipal(getter_AddRefs(principal));
    } else {
      result =
        secMan->GetChannelResultPrincipal(aChannel, getter_AddRefs(principal));
    }
  }

  if (NS_FAILED(result)) {
    LOG_WARN(("  Couldn't get principal"));
    mLoader->SheetComplete(this, result);
    return NS_OK;
  }

  mSheet->SetPrincipal(principal);

  if (mLoaderPrincipal && mSheet->GetCORSMode() == CORS_NONE) {
    bool subsumed;
    result = mLoaderPrincipal->Subsumes(principal, &subsumed);
    if (NS_FAILED(result) || !subsumed) {
      mIsCrossOriginNoCORS = true;
    }
  }

  // If it's an HTTP channel, we want to make sure this is not an
  // error document we got.
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aChannel));
  if (httpChannel) {
    bool requestSucceeded;
    result = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(result) && !requestSucceeded) {
      LOG(("  Load returned an error page"));
      mLoader->SheetComplete(this, NS_ERROR_NOT_AVAILABLE);
      return NS_OK;
    }

    nsAutoCString sourceMapURL;
    if (nsContentUtils::GetSourceMapURL(httpChannel, sourceMapURL)) {
      mSheet->SetSourceMapURL(NS_ConvertUTF8toUTF16(sourceMapURL));
    }
  }

  nsAutoCString contentType;
  aChannel->GetContentType(contentType);

  // In standards mode, a style sheet must have one of these MIME
  // types to be processed at all.  In quirks mode, we accept any
  // MIME type, but only if the style sheet is same-origin with the
  // requesting document or parent sheet.  See bug 524223.

  bool validType = contentType.EqualsLiteral("text/css") ||
    contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE) ||
    contentType.IsEmpty();

  if (!validType) {
    const char *errorMessage;
    uint32_t errorFlag;
    bool sameOrigin = true;

    if (mLoaderPrincipal) {
      bool subsumed;
      result = mLoaderPrincipal->Subsumes(principal, &subsumed);
      if (NS_FAILED(result) || !subsumed) {
        sameOrigin = false;
      }
    }

    if (sameOrigin && mLoader->mCompatMode == eCompatibility_NavQuirks) {
      errorMessage = "MimeNotCssWarn";
      errorFlag = nsIScriptError::warningFlag;
    } else {
      errorMessage = "MimeNotCss";
      errorFlag = nsIScriptError::errorFlag;
    }

    const nsString& specUTF16 =
      NS_ConvertUTF8toUTF16(channelURI->GetSpecOrDefault());
    const nsString& ctypeUTF16 = NS_ConvertASCIItoUTF16(contentType);
    const char16_t *strings[] = { specUTF16.get(), ctypeUTF16.get() };

    nsCOMPtr<nsIURI> referrer = GetReferrerURI();
    nsContentUtils::ReportToConsole(errorFlag,
                                    NS_LITERAL_CSTRING("CSS Loader"),
                                    mLoader->mDocument,
                                    nsContentUtils::eCSS_PROPERTIES,
                                    errorMessage,
                                    strings, ArrayLength(strings),
                                    referrer);

    if (errorFlag == nsIScriptError::errorFlag) {
      LOG_WARN(("  Ignoring sheet with improper MIME type %s",
                contentType.get()));
      mLoader->SheetComplete(this, NS_ERROR_NOT_AVAILABLE);
      return NS_OK;
    }
  }

  SRIMetadata sriMetadata;
  mSheet->GetIntegrity(sriMetadata);
  if (sriMetadata.IsEmpty()) {
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (loadInfo && loadInfo->GetEnforceSRI()) {
      LOG(("  Load was blocked by SRI"));
      MOZ_LOG(gSriPRLog, mozilla::LogLevel::Debug,
              ("css::Loader::OnStreamComplete, required SRI not found"));
      mLoader->SheetComplete(this, NS_ERROR_SRI_CORRUPT);
      // log the failed load to web console
      nsCOMPtr<nsIContentSecurityPolicy> csp;
      loadInfo->LoadingPrincipal()->GetCsp(getter_AddRefs(csp));
      nsAutoCString spec;
      mLoader->mDocument->GetDocumentURI()->GetAsciiSpec(spec);
      // line number unknown. mRequestingNode doesn't bear this info.
      csp->LogViolationDetails(
        nsIContentSecurityPolicy::VIOLATION_TYPE_REQUIRE_SRI_FOR_STYLE,
        NS_ConvertUTF8toUTF16(spec), EmptyString(),
        0, EmptyString(), EmptyString());
      return NS_OK;
    }
  } else {
    nsAutoCString sourceUri;
    if (mLoader->mDocument && mLoader->mDocument->GetDocumentURI()) {
      mLoader->mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
    }
    nsresult rv = VerifySheetIntegrity(
      sriMetadata, aChannel, aBytes1, aBytes2, sourceUri, mLoader->mReporter);

    nsCOMPtr<nsILoadGroup> loadGroup;
    aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
      mLoader->mReporter->FlushConsoleReports(loadGroup);
    } else {
      mLoader->mReporter->FlushConsoleReports(mLoader->mDocument);
    }

    if (NS_FAILED(rv)) {
      LOG(("  Load was blocked by SRI"));
      MOZ_LOG(gSriPRLog, mozilla::LogLevel::Debug,
              ("css::Loader::OnStreamComplete, bad metadata"));
      mLoader->SheetComplete(this, NS_ERROR_SRI_CORRUPT);
      return NS_OK;
    }
  }

  // Enough to set the URIs on mSheet, since any sibling datas we have share
  // the same mInner as mSheet and will thus get the same URI.
  mSheet->SetURIs(channelURI, originalURI, channelURI);
  return NS_OK_PARSE_SHEET;
}

Loader::IsAlternate
Loader::IsAlternateSheet(const nsAString& aTitle, bool aHasAlternateRel)
{
  // A sheet is alternate if it has a nonempty title that doesn't match the
  // currently selected style set.  But if there _is_ no currently selected
  // style set, the sheet wasn't marked as an alternate explicitly, and aTitle
  // is nonempty, we should select the style set corresponding to aTitle, since
  // that's a preferred sheet.
  //
  // FIXME(emilio): This should return false for Shadow DOM regardless of the
  // document.
  if (aTitle.IsEmpty()) {
    return IsAlternate::No;
  }

  if (mDocument) {
    const nsString& currentSheetSet = mDocument->GetCurrentStyleSheetSet();
    if (!aHasAlternateRel && currentSheetSet.IsEmpty()) {
      // There's no preferred set yet, and we now have a sheet with a title.
      // Make that be the preferred set.
      // FIXME(emilio): This is kinda wild, can we do it somewhere else?
      mDocument->SetPreferredStyleSheetSet(aTitle);
      // We're definitely not an alternate. Also, beware that at this point
      // currentSheetSet may dangle.
      return IsAlternate::No;
    }

    if (aTitle.Equals(currentSheetSet)) {
      return IsAlternate::No;
    }
  }

  return IsAlternate::Yes;
}

nsresult
Loader::ObsoleteSheet(nsIURI* aURI)
{
  if (!mSheets) {
    return NS_OK;
  }
  if (!aURI) {
    return NS_ERROR_INVALID_ARG;
  }
  for (auto iter = mSheets->mCompleteSheets.Iter(); !iter.Done(); iter.Next()) {
    nsIURI* sheetURI = iter.Key()->GetURI();
    bool areEqual;
    nsresult rv = sheetURI->Equals(aURI, &areEqual);
    if (NS_SUCCEEDED(rv) && areEqual) {
      iter.Remove();
    }
  }
  return NS_OK;
}

nsresult
Loader::CheckContentPolicy(nsIPrincipal* aLoadingPrincipal,
                           nsIPrincipal* aTriggeringPrincipal,
                           nsIURI* aTargetURI,
                           nsINode* aRequestingNode,
                           bool aIsPreload)
{
  // When performing a system load (e.g. aUseSystemPrincipal = true)
  // then aLoadingPrincipal == null; don't consult content policies.
  if (!aLoadingPrincipal) {
    return NS_OK;
  }

  nsContentPolicyType contentPolicyType =
    aIsPreload ? nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD
               : nsIContentPolicy::TYPE_INTERNAL_STYLESHEET;

  nsCOMPtr<nsILoadInfo> secCheckLoadInfo =
    new net::LoadInfo(aLoadingPrincipal,
                      aTriggeringPrincipal,
                      aRequestingNode,
                      nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
                      contentPolicyType);

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(aTargetURI,
                                          secCheckLoadInfo,
                                          NS_LITERAL_CSTRING("text/css"),
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy());
   if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
     return NS_ERROR_CONTENT_BLOCKED;
   }
   return NS_OK;
}

/**
 * CreateSheet() creates a CSSStyleSheet object for the given URI,
 * if any.  If there is no URI given, we just create a new style sheet
 * object.  Otherwise, we check for an existing style sheet object for
 * that uri in various caches and clone it if we find it.  Cloned
 * sheets will have the title/media/enabled state of the sheet they
 * are clones off; make sure to call PrepareSheet() on the result of
 * CreateSheet().
 */
nsresult
Loader::CreateSheet(nsIURI* aURI,
                    nsIContent* aLinkingContent,
                    nsIPrincipal* aLoaderPrincipal,
                    css::SheetParsingMode aParsingMode,
                    CORSMode aCORSMode,
                    ReferrerPolicy aReferrerPolicy,
                    const nsAString& aIntegrity,
                    bool aSyncLoad,
                    StyleSheetState& aSheetState,
                    RefPtr<StyleSheet>* aSheet)
{
  LOG(("css::Loader::CreateSheet"));
  MOZ_ASSERT(aSheet, "Null out param!");

  if (!mSheets) {
    mSheets = new Sheets();
  }

  *aSheet = nullptr;
  aSheetState = eSheetStateUnknown;

  if (aURI) {
    aSheetState = eSheetComplete;
    RefPtr<StyleSheet> sheet;

    // First, the XUL cache
#ifdef MOZ_XUL
    if (IsChromeURI(aURI)) {
      nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
      if (cache && cache->IsEnabled()) {
        sheet = cache->GetStyleSheet(aURI);
        LOG(("  From XUL cache: %p", sheet.get()));
      }
    }
#endif

    bool fromCompleteSheets = false;
    if (!sheet) {
      // Then our per-document complete sheets.
      URIPrincipalReferrerPolicyAndCORSModeHashKey key(aURI, aLoaderPrincipal, aCORSMode, aReferrerPolicy);

      StyleSheet* completeSheet = nullptr;
      mSheets->mCompleteSheets.Get(&key, &completeSheet);
      sheet = completeSheet;
      LOG(("  From completed: %p", sheet.get()));

      fromCompleteSheets = !!sheet;
    }

    if (sheet) {
      // This sheet came from the XUL cache or our per-document hashtable; it
      // better be a complete sheet.
      NS_ASSERTION(sheet->IsComplete(),
                   "Sheet thinks it's not complete while we think it is");

      // Make sure it hasn't been forced to have a unique inner;
      // that is an indication that its rules have been exposed to
      // CSSOM and so we can't use it.
      if (sheet->HasForcedUniqueInner()) {
        LOG(("  Not cloning completed sheet %p because it has a "
             "forced unique inner",
             sheet.get()));
        sheet = nullptr;
        fromCompleteSheets = false;
      }
    }

    // Then loading sheets
    if (!sheet && !aSyncLoad) {
      aSheetState = eSheetLoading;
      SheetLoadData* loadData = nullptr;
      URIPrincipalReferrerPolicyAndCORSModeHashKey key(aURI, aLoaderPrincipal, aCORSMode, aReferrerPolicy);
      mSheets->mLoadingDatas.Get(&key, &loadData);
      if (loadData) {
        sheet = loadData->mSheet;
        LOG(("  From loading: %p", sheet.get()));

#ifdef DEBUG
        bool debugEqual;
        NS_ASSERTION((!aLoaderPrincipal && !loadData->mLoaderPrincipal) ||
                     (aLoaderPrincipal && loadData->mLoaderPrincipal &&
                      NS_SUCCEEDED(aLoaderPrincipal->
                                   Equals(loadData->mLoaderPrincipal,
                                          &debugEqual)) && debugEqual),
                     "Principals should be the same");
#endif
      }

      // Then alternate sheets
      if (!sheet) {
        aSheetState = eSheetPending;
        loadData = nullptr;
        mSheets->mPendingDatas.Get(&key, &loadData);
        if (loadData) {
          sheet = loadData->mSheet;
          LOG(("  From pending: %p", sheet.get()));

#ifdef DEBUG
          bool debugEqual;
          NS_ASSERTION((!aLoaderPrincipal && !loadData->mLoaderPrincipal) ||
                       (aLoaderPrincipal && loadData->mLoaderPrincipal &&
                        NS_SUCCEEDED(aLoaderPrincipal->
                                     Equals(loadData->mLoaderPrincipal,
                                            &debugEqual)) && debugEqual),
                       "Principals should be the same");
#endif
        }
      }
    }

    if (sheet) {
      // The sheet we have now should be either incomplete or without
      // a forced unique inner.
      NS_ASSERTION(!sheet->HasForcedUniqueInner() ||
                   !sheet->IsComplete(),
                   "Unexpected complete sheet with forced unique inner");
      NS_ASSERTION(sheet->IsComplete() ||
                   aSheetState != eSheetComplete,
                   "Sheet thinks it's not complete while we think it is");

      RefPtr<StyleSheet> clonedSheet =
        sheet->Clone(nullptr, nullptr, nullptr, nullptr);
      *aSheet = Move(clonedSheet);
      if (*aSheet && fromCompleteSheets &&
          !sheet->GetOwnerNode() &&
          !sheet->GetParentSheet()) {
        // The sheet we're cloning isn't actually referenced by
        // anyone.  Replace it in the cache, so that if our CSSOM is
        // later modified we don't end up with two copies of our inner
        // hanging around.
        URIPrincipalReferrerPolicyAndCORSModeHashKey key(aURI, aLoaderPrincipal, aCORSMode, aReferrerPolicy);
        NS_ASSERTION((*aSheet)->IsComplete(),
                     "Should only be caching complete sheets");
        mSheets->mCompleteSheets.Put(&key, *aSheet);
      }
    }
  }

  if (!*aSheet) {
    aSheetState = eSheetNeedsParser;
    nsIURI *sheetURI;
    nsCOMPtr<nsIURI> baseURI;
    nsIURI* originalURI;
    if (!aURI) {
      // Inline style.  Use the document's base URL so that @import in
      // the inline sheet picks up the right base.
      NS_ASSERTION(aLinkingContent, "Inline stylesheet without linking content?");
      baseURI = aLinkingContent->GetBaseURI();
      sheetURI = aLinkingContent->OwnerDoc()->GetDocumentURI();
      originalURI = nullptr;
    } else {
      baseURI = aURI;
      sheetURI = aURI;
      originalURI = aURI;
    }

    SRIMetadata sriMetadata;
    if (!aIntegrity.IsEmpty()) {
      MOZ_LOG(gSriPRLog, mozilla::LogLevel::Debug,
              ("css::Loader::CreateSheet, integrity=%s",
               NS_ConvertUTF16toUTF8(aIntegrity).get()));
      nsAutoCString sourceUri;
      if (mDocument && mDocument->GetDocumentURI()) {
        mDocument->GetDocumentURI()->GetAsciiSpec(sourceUri);
      }
      SRICheck::IntegrityMetadata(aIntegrity, sourceUri, mReporter,
                                  &sriMetadata);
    }

    *aSheet = new StyleSheet(aParsingMode, aCORSMode, aReferrerPolicy, sriMetadata);
    (*aSheet)->SetURIs(sheetURI, originalURI, baseURI);
  }

  NS_ASSERTION(*aSheet, "We should have a sheet by now!");
  NS_ASSERTION(aSheetState != eSheetStateUnknown, "Have to set a state!");
  LOG(("  State: %s", gStateStrings[aSheetState]));

  return NS_OK;
}

static Loader::MediaMatched
MediaListMatches(const MediaList* aMediaList, const nsIDocument* aDocument)
{
  if (!aMediaList || !aDocument) {
    return Loader::MediaMatched::Yes;
  }

  nsPresContext* pc = aDocument->GetPresContext();
  if (!pc) {
    // Conservatively assume a match.
    return Loader::MediaMatched::Yes;
  }

  if (aMediaList->Matches(pc)) {
    return Loader::MediaMatched::Yes;
  }

  return Loader::MediaMatched::No;
}

/**
 * PrepareSheet() handles setting the media and title on the sheet, as
 * well as setting the enabled state based on the title and whether
 * the sheet had "alternate" in its rel.
 */
Loader::MediaMatched
Loader::PrepareSheet(StyleSheet* aSheet,
                     const nsAString& aTitle,
                     const nsAString& aMediaString,
                     MediaList* aMediaList,
                     IsAlternate aIsAlternate)
{
  MOZ_ASSERT(aSheet, "Must have a sheet!");

  RefPtr<MediaList> mediaList(aMediaList);

  if (!aMediaString.IsEmpty()) {
    NS_ASSERTION(!aMediaList,
                 "must not provide both aMediaString and aMediaList");
    mediaList = MediaList::Create(aMediaString);
  }

  aSheet->SetMedia(mediaList);

  aSheet->SetTitle(aTitle);
  aSheet->SetEnabled(aIsAlternate == IsAlternate::No);
  return MediaListMatches(mediaList, mDocument);
}

/**
 * InsertSheetInDoc handles ordering of sheets in the document.  Here
 * we have two types of sheets -- those with linking elements and
 * those without.  The latter are loaded by Link: headers.
 * The following constraints are observed:
 * 1) Any sheet with a linking element comes after all sheets without
 *    linking elements
 * 2) Sheets without linking elements are inserted in the order in
 *    which the inserting requests come in, since all of these are
 *    inserted during header data processing in the content sink
 * 3) Sheets with linking elements are ordered based on document order
 *    as determined by CompareDocumentPosition.
 */
nsresult
Loader::InsertSheetInDoc(StyleSheet* aSheet,
                         nsIContent* aLinkingContent,
                         nsIDocument* aDocument)
{
  LOG(("css::Loader::InsertSheetInDoc"));
  MOZ_ASSERT(aSheet, "Nothing to insert");
  MOZ_ASSERT(aDocument, "Must have a document to insert into");

  // XXX Need to cancel pending sheet loads for this element, if any

  int32_t sheetCount = aDocument->SheetCount();

  /*
   * Start the walk at the _end_ of the list, since in the typical
   * case we'll just want to append anyway.  We want to break out of
   * the loop when insertionPoint points to just before the index we
   * want to insert at.  In other words, when we leave the loop
   * insertionPoint is the index of the stylesheet that immediately
   * precedes the one we're inserting.
   */
  int32_t insertionPoint;
  for (insertionPoint = sheetCount - 1; insertionPoint >= 0; --insertionPoint) {
    StyleSheet* curSheet = aDocument->SheetAt(insertionPoint);
    NS_ASSERTION(curSheet, "There must be a sheet here!");
    nsCOMPtr<nsINode> sheetOwner = curSheet->GetOwnerNode();
    if (sheetOwner && !aLinkingContent) {
      // Keep moving; all sheets with a sheetOwner come after all
      // sheets without a linkingNode
      continue;
    }

    if (!sheetOwner) {
      // Aha!  The current sheet has no sheet owner, so we want to
      // insert after it no matter whether we have a linkingNode
      break;
    }

    NS_ASSERTION(aLinkingContent != sheetOwner,
                 "Why do we still have our old sheet?");

    // Have to compare
    if (nsContentUtils::PositionIsBefore(sheetOwner, aLinkingContent)) {
      // The current sheet comes before us, and it better be the first
      // such, because now we break
      break;
    }
  }

  ++insertionPoint; // adjust the index to the spot we want to insert in

  // XXX <meta> elements do not implement nsIStyleSheetLinkingElement;
  // need to fix this for them to be ordered correctly.
  nsCOMPtr<nsIStyleSheetLinkingElement>
    linkingElement = do_QueryInterface(aLinkingContent);
  if (linkingElement) {
    linkingElement->SetStyleSheet(aSheet); // This sets the ownerNode on the sheet
  }

  aDocument->InsertStyleSheetAt(aSheet, insertionPoint);
  LOG(("  Inserting into document at position %d", insertionPoint));

  return NS_OK;
}

/**
 * InsertChildSheet handles ordering of @import-ed sheet in their
 * parent sheets.  Here we want to just insert based on order of the
 * @import rules that imported the sheets.  In theory we can't just
 * append to the end because the CSSOM can insert @import rules.  In
 * practice, we get the call to load the child sheet before the CSSOM
 * has finished inserting the @import rule, so we have no idea where
 * to put it anyway.  So just append for now.  (In the future if we
 * want to insert the sheet at the correct position, we'll need to
 * restore CSSStyleSheet::InsertStyleSheetAt, which was removed in
 * bug 1220506.)
 */
nsresult
Loader::InsertChildSheet(StyleSheet* aSheet, StyleSheet* aParentSheet)
{
  LOG(("css::Loader::InsertChildSheet"));
  MOZ_ASSERT(aSheet, "Nothing to insert");
  MOZ_ASSERT(aParentSheet, "Need a parent to insert into");

  // child sheets should always start out enabled, even if they got
  // cloned off of top-level sheets which were disabled
  aSheet->SetEnabled(true);
  aParentSheet->PrependStyleSheet(aSheet);

  LOG(("  Inserting into parent sheet"));
  return NS_OK;
}

/**
 * LoadSheet handles the actual load of a sheet.  If the load is
 * supposed to be synchronous it just opens a channel synchronously
 * using the given uri, wraps the resulting stream in a converter
 * stream and calls ParseSheet.  Otherwise it tries to look for an
 * existing load for this URI and piggyback on it.  Failing all that,
 * a new load is kicked off asynchronously.
 */
nsresult
Loader::LoadSheet(SheetLoadData* aLoadData,
                  StyleSheetState aSheetState,
                  bool aIsPreload)
{
  LOG(("css::Loader::LoadSheet"));
  MOZ_ASSERT(aLoadData, "Need a load data");
  MOZ_ASSERT(aLoadData->mURI, "Need a URI to load");
  MOZ_ASSERT(aLoadData->mSheet, "Need a sheet to load into");
  MOZ_ASSERT(aSheetState != eSheetComplete, "Why bother?");
  MOZ_ASSERT(!aLoadData->mUseSystemPrincipal || aLoadData->mSyncLoad,
             "Shouldn't use system principal for async loads");
  NS_ASSERTION(mSheets, "mLoadingDatas should be initialized by now.");

  LOG_URI("  Load from: '%s'", aLoadData->mURI);

  nsresult rv = NS_OK;

  if (!mDocument && !aLoadData->mIsNonDocumentSheet) {
    // No point starting the load; just release all the data and such.
    LOG_WARN(("  No document and not non-document sheet; pre-dropping load"));
    SheetComplete(aLoadData, NS_BINDING_ABORTED);
    return NS_BINDING_ABORTED;
  }

  SRIMetadata sriMetadata;
  aLoadData->mSheet->GetIntegrity(sriMetadata);

  if (aLoadData->mSyncLoad) {
    LOG(("  Synchronous load"));
    NS_ASSERTION(!aLoadData->mObserver, "Observer for a sync load?");
    NS_ASSERTION(aSheetState == eSheetNeedsParser,
                 "Sync loads can't reuse existing async loads");

    // Create a StreamLoader instance to which we will feed
    // the data from the sync load.  Do this before creating the
    // channel to make error recovery simpler.
    nsCOMPtr<nsIStreamListener> streamLoader = new StreamLoader(aLoadData);

    if (mDocument) {
      mozilla::net::PredictorLearn(aLoadData->mURI, mDocument->GetDocumentURI(),
                                   nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
                                   mDocument);
    }

    nsSecurityFlags securityFlags =
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS |
      nsILoadInfo::SEC_ALLOW_CHROME;

    nsContentPolicyType contentPolicyType =
      aIsPreload ? nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD
                 : nsIContentPolicy::TYPE_INTERNAL_STYLESHEET;

    // Just load it
    nsCOMPtr<nsIChannel> channel;
    // Note that we are calling NS_NewChannelWithTriggeringPrincipal() with both
    // a node and a principal.
    // This is because of a case where the node is the document being styled and
    // the principal is the stylesheet (perhaps from a different origin) that is
    // applying the styles.
    if (aLoadData->mRequestingNode && aLoadData->mLoaderPrincipal) {
      rv = NS_NewChannelWithTriggeringPrincipal(getter_AddRefs(channel),
                                                aLoadData->mURI,
                                                aLoadData->mRequestingNode,
                                                aLoadData->mLoaderPrincipal,
                                                securityFlags,
                                                contentPolicyType);
    }
    else {
      // either we are loading something inside a document, in which case
      // we should always have a requestingNode, or we are loading something
      // outside a document, in which case the loadingPrincipal and the
      // triggeringPrincipal should always be the systemPrincipal.
      auto result = URLPreloader::ReadURI(aLoadData->mURI);
      if (result.isOk()) {
        nsCOMPtr<nsIInputStream> stream;
        MOZ_TRY(NS_NewCStringInputStream(getter_AddRefs(stream), result.unwrap()));

        rv = NS_NewInputStreamChannel(getter_AddRefs(channel),
                                      aLoadData->mURI,
                                      stream.forget(),
                                      nsContentUtils::GetSystemPrincipal(),
                                      securityFlags,
                                      contentPolicyType);
      } else {
        rv = NS_NewChannel(getter_AddRefs(channel),
                           aLoadData->mURI,
                           nsContentUtils::GetSystemPrincipal(),
                           securityFlags,
                           contentPolicyType);
      }
    }
    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to create channel"));
      SheetComplete(aLoadData, rv);
      return rv;
    }

    nsCOMPtr<nsIInputStream> stream;
    rv = channel->Open2(getter_AddRefs(stream));

    if (NS_FAILED(rv)) {
      LOG_ERROR(("  Failed to open URI synchronously"));
      SheetComplete(aLoadData, rv);
      return rv;
    }

    // Force UA sheets to be UTF-8.
    // XXX this is only necessary because the default in
    // SheetLoadData::OnDetermineCharset is wrong (bug 521039).
    channel->SetContentCharset(NS_LITERAL_CSTRING("UTF-8"));

    // Manually feed the streamloader the contents of the stream.
    // This will call back into OnStreamComplete
    // and thence to ParseSheet.  Regardless of whether this fails,
    // SheetComplete has been called.
    return nsSyncLoadService::PushSyncStreamToListener(stream.forget(),
                                                       streamLoader,
                                                       channel);
  }

  SheetLoadData* existingData = nullptr;

  URIPrincipalReferrerPolicyAndCORSModeHashKey key(aLoadData->mURI,
                                     aLoadData->mLoaderPrincipal,
                                     aLoadData->mSheet->GetCORSMode(),
                                     aLoadData->mSheet->GetReferrerPolicy());
  if (aSheetState == eSheetLoading) {
    mSheets->mLoadingDatas.Get(&key, &existingData);
    NS_ASSERTION(existingData, "CreateSheet lied about the state");
  }
  else if (aSheetState == eSheetPending){
    mSheets->mPendingDatas.Get(&key, &existingData);
    NS_ASSERTION(existingData, "CreateSheet lied about the state");
  }

  if (existingData) {
    LOG(("  Glomming on to existing load"));
    SheetLoadData* data = existingData;
    while (data->mNext) {
      data = data->mNext;
    }
    data->mNext = aLoadData; // transfer ownership
    if (aSheetState == eSheetPending && !aLoadData->ShouldDefer()) {
      // Kick the load off; someone cares about it right away

#ifdef DEBUG
      SheetLoadData* removedData;
      NS_ASSERTION(mSheets->mPendingDatas.Get(&key, &removedData) &&
                   removedData == existingData,
                   "Bad pending table.");
#endif

      mSheets->mPendingDatas.Remove(&key);

      LOG(("  Forcing load of pending data"));
      return LoadSheet(existingData, eSheetNeedsParser, aIsPreload);
    }
    // All done here; once the load completes we'll be marked complete
    // automatically
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    loadGroup = mDocument->GetDocumentLoadGroup();
    // load for a document with no loadgrup indicates that something is
    // completely bogus, let's bail out early.
    if (!loadGroup) {
      LOG_ERROR(("  Failed to query loadGroup from document"));
      SheetComplete(aLoadData, NS_ERROR_UNEXPECTED);
      return NS_ERROR_UNEXPECTED;
    }
  }
#ifdef DEBUG
  mSyncCallback = true;
#endif

  CORSMode ourCORSMode = aLoadData->mSheet->GetCORSMode();
  nsSecurityFlags securityFlags =
    ourCORSMode == CORS_NONE
      ? nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS
      : nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;
  if (ourCORSMode == CORS_ANONYMOUS) {
    securityFlags |= nsILoadInfo::SEC_COOKIES_SAME_ORIGIN;
  } else if (ourCORSMode == CORS_USE_CREDENTIALS) {
    securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
  }
  securityFlags |= nsILoadInfo::SEC_ALLOW_CHROME;

  nsContentPolicyType contentPolicyType =
    aIsPreload ? nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD
               : nsIContentPolicy::TYPE_INTERNAL_STYLESHEET;

  nsCOMPtr<nsIChannel> channel;
  // Note we are calling NS_NewChannelWithTriggeringPrincipal here with a node
  // and a principal. This is because of a case where the node is the document
  // being styled and the principal is the stylesheet (perhaps from a different
  // origin)  that is applying the styles.
  if (aLoadData->mRequestingNode && aLoadData->mLoaderPrincipal) {
    rv = NS_NewChannelWithTriggeringPrincipal(getter_AddRefs(channel),
                                              aLoadData->mURI,
                                              aLoadData->mRequestingNode,
                                              aLoadData->mLoaderPrincipal,
                                              securityFlags,
                                              contentPolicyType,
                                              nullptr, // Performancestorage
                                              loadGroup,
                                              nullptr,   // aCallbacks
                                              nsIChannel::LOAD_NORMAL |
                                              nsIChannel::LOAD_CLASSIFY_URI);
  }
  else {
    // either we are loading something inside a document, in which case
    // we should always have a requestingNode, or we are loading something
    // outside a document, in which case the loadingPrincipal and the
    // triggeringPrincipal should always be the systemPrincipal.
    rv = NS_NewChannel(getter_AddRefs(channel),
                       aLoadData->mURI,
                       nsContentUtils::GetSystemPrincipal(),
                       securityFlags,
                       contentPolicyType,
                       nullptr, // aPerformanceStorage
                       loadGroup,
                       nullptr,   // aCallbacks
                       nsIChannel::LOAD_NORMAL |
                       nsIChannel::LOAD_CLASSIFY_URI);
  }

  if (NS_FAILED(rv)) {
#ifdef DEBUG
    mSyncCallback = false;
#endif
    LOG_ERROR(("  Failed to create channel"));
    SheetComplete(aLoadData, rv);
    return rv;
  }

  if (!aLoadData->ShouldDefer()) {
    nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));
    if (cos) {
      cos->AddClassFlags(nsIClassOfService::Leader);
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // Send a minimal Accept header for text/css
    rv = httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                       NS_LITERAL_CSTRING("text/css,*/*;q=0.1"),
                                       false);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> referrerURI = aLoadData->GetReferrerURI();
    if (referrerURI) {
      rv = httpChannel->SetReferrerWithPolicy(referrerURI,
                                              aLoadData->mSheet->GetReferrerPolicy());
      Unused << NS_WARN_IF(NS_FAILED(rv));
    }

    nsCOMPtr<nsIHttpChannelInternal> internalChannel = do_QueryInterface(httpChannel);
    if (internalChannel) {
      rv = internalChannel->SetIntegrityMetadata(sriMetadata.GetIntegrityString());
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Set the initiator type
    nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
    if (timedChannel) {
      if (aLoadData->mParentData) {
        timedChannel->SetInitiatorType(NS_LITERAL_STRING("css"));

        // This is a child sheet load.
        //
        // The resource timing of the sub-resources that a document loads
        // should normally be reported to the document.  One exception is any
        // sub-resources of any cross-origin resources that are loaded.  We
        // don't mind reporting timing data for a direct child cross-origin
        // resource since the resource that linked to it (and hence potentially
        // anything in that parent origin) is aware that the cross-origin
        // resources is to be loaded.  However, we do not want to report
        // timings for any sub-resources that a cross-origin resource may load
        // since that obviously leaks information about what the cross-origin
        // resource loads, which is bad.
        //
        // In addition to checking whether we're an immediate child resource of
        // a cross-origin resource (by checking if mIsCrossOriginNoCORS is set
        // to true on our parent), we also check our parent to see whether it
        // itself is a sub-resource of a cross-origin resource by checking
        // mBlockResourceTiming.  If that is set then we too are such a
        // sub-resource and so we set the flag on ourself too to propagate it
        // on down.
        //
        if (aLoadData->mParentData->mIsCrossOriginNoCORS ||
            aLoadData->mParentData->mBlockResourceTiming) {
          // Set a flag so any other stylesheet triggered by this one will
          // not be reported
          aLoadData->mBlockResourceTiming = true;

          // Mark the channel so PerformanceMainThread::AddEntry will not
          // report the resource.
          timedChannel->SetReportResourceTiming(false);
        }

      } else {
        timedChannel->SetInitiatorType(NS_LITERAL_STRING("link"));
      }
    }
  }

  // Now tell the channel we expect text/css data back....  We do
  // this before opening it, so it's only treated as a hint.
  channel->SetContentType(NS_LITERAL_CSTRING("text/css"));

  // We don't have to hold on to the stream loader.  The ownership
  // model is: Necko owns the stream loader, which owns the load data,
  // which owns us
  nsCOMPtr<nsIStreamListener> streamLoader = new StreamLoader(aLoadData);

  if (mDocument) {
    mozilla::net::PredictorLearn(aLoadData->mURI, mDocument->GetDocumentURI(),
                                 nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE,
                                 mDocument);
  }

  rv = channel->AsyncOpen2(streamLoader);

#ifdef DEBUG
  mSyncCallback = false;
#endif

  if (NS_FAILED(rv)) {
    LOG_ERROR(("  Failed to create stream loader"));
    SheetComplete(aLoadData, rv);
    return rv;
  }

  mSheets->mLoadingDatas.Put(&key, aLoadData);
  aLoadData->mIsLoading = true;

  return NS_OK;
}

/**
 * ParseSheet handles parsing the data stream.
 */
void
Loader::ParseSheet(const nsAString& aUTF16,
                   const nsACString& aUTF8,
                   SheetLoadData* aLoadData,
                   bool aAllowAsync,
                   bool& aCompleted)
{
  LOG(("css::Loader::ParseSheet"));
  MOZ_ASSERT(aLoadData, "Must have load data");
  MOZ_ASSERT(aLoadData->mSheet, "Must have sheet to parse into");
  aCompleted = false;
  MOZ_ASSERT(aUTF16.IsEmpty() || aUTF8.IsEmpty());
  if (!aUTF16.IsEmpty()) {
    DoParseSheetServo(NS_ConvertUTF16toUTF8(aUTF16),
                      aLoadData,
                      aAllowAsync,
                      aCompleted);
  } else {
    DoParseSheetServo(aUTF8, aLoadData, aAllowAsync, aCompleted);
  }
}

void
Loader::DoParseSheetServo(const nsACString& aBytes,
                          SheetLoadData* aLoadData,
                          bool aAllowAsync,
                          bool& aCompleted)
{
  aLoadData->mIsBeingParsed = true;

  StyleSheet* sheet = aLoadData->mSheet;
  MOZ_ASSERT(sheet);

  // Some cases, like inline style and UA stylesheets, need to be parsed
  // synchronously. The former may trigger child loads, the latter must not.
  if (aLoadData->mSyncLoad || !aAllowAsync) {
    sheet->ParseSheetSync(this, aBytes, aLoadData, aLoadData->mLineNumber);
    aLoadData->mIsBeingParsed = false;

    bool noPendingChildren = aLoadData->mPendingChildren == 0;
    MOZ_ASSERT_IF(aLoadData->mSyncLoad, noPendingChildren);
    if (noPendingChildren) {
      aCompleted = true;
      SheetComplete(aLoadData, NS_OK);
    }

    return;
  }

  // This parse does not need to be synchronous. \o/
  //
  // Note that we need to block onload because there may be no network requests
  // pending.
  BlockOnload();
  RefPtr<SheetLoadData> loadData = aLoadData;
  nsCOMPtr<nsISerialEventTarget> target = DispatchTarget();
  sheet->ParseSheet(this, aBytes, aLoadData)->Then(target, __func__,
    [loadData = Move(loadData)](bool aDummy) {
      MOZ_ASSERT(NS_IsMainThread());
      loadData->mIsBeingParsed = false;
      loadData->mLoader->UnblockOnload(/* aFireSync = */ false);
      // If there are no child sheets outstanding, mark us as complete.
      // Otherwise, the children are holding strong refs to the data and
      // will call SheetComplete() on it when they complete.
      if (loadData->mPendingChildren == 0) {
        loadData->mLoader->SheetComplete(loadData, NS_OK);
      }
    }, [] { MOZ_CRASH("rejected parse promise"); }
  );
}

/**
 * SheetComplete is the do-it-all cleanup function.  It removes the
 * load data from the "loading" hashtable, adds the sheet to the
 * "completed" hashtable, massages the XUL cache, handles siblings of
 * the load data (other loads for the same URI), handles unblocking
 * blocked parent loads as needed, and most importantly calls
 * NS_RELEASE on the load data to destroy the whole mess.
 */
void
Loader::SheetComplete(SheetLoadData* aLoadData, nsresult aStatus)
{
  LOG(("css::Loader::SheetComplete, status: 0x%" PRIx32, static_cast<uint32_t>(aStatus)));

  // If aStatus is a failure we need to mark this data failed.  We also need to
  // mark any ancestors of a failing data as failed and any sibling of a
  // failing data as failed.  Note that SheetComplete is never called on a
  // SheetLoadData that is the mNext of some other SheetLoadData.
  if (NS_FAILED(aStatus)) {
    MarkLoadTreeFailed(aLoadData);
  }

  // 8 is probably big enough for all our common cases.  It's not likely that
  // imports will nest more than 8 deep, and multiple sheets with the same URI
  // are rare.
  AutoTArray<RefPtr<SheetLoadData>, 8> datasToNotify;
  DoSheetComplete(aLoadData, datasToNotify);

  // Now it's safe to go ahead and notify observers
  uint32_t count = datasToNotify.Length();
  mDatasToNotifyOn += count;
  for (uint32_t i = 0; i < count; ++i) {
    --mDatasToNotifyOn;

    SheetLoadData* data = datasToNotify[i];
    NS_ASSERTION(data && data->mMustNotify, "How did this data get here?");
    if (data->mObserver) {
      LOG(("  Notifying observer %p for data %p.  wasAlternate: %d",
           data->mObserver.get(), data, data->mWasAlternate));
      data->mObserver->StyleSheetLoaded(data->mSheet, data->ShouldDefer(),
                                        aStatus);
    }

    nsTObserverArray<nsCOMPtr<nsICSSLoaderObserver> >::ForwardIterator iter(mObservers);
    nsCOMPtr<nsICSSLoaderObserver> obs;
    while (iter.HasMore()) {
      obs = iter.GetNext();
      LOG(("  Notifying global observer %p for data %p.  wasAlternate: %d",
           obs.get(), data, data->mWasAlternate));
      obs->StyleSheetLoaded(data->mSheet, data->mWasAlternate, aStatus);
    }
  }

  if (mSheets->mLoadingDatas.Count() == 0 && mSheets->mPendingDatas.Count() > 0) {
    LOG(("  No more loading sheets; starting deferred loads"));
    StartDeferredLoads();
  }
}

void
Loader::DoSheetComplete(SheetLoadData* aLoadData, LoadDataArray& aDatasToNotify)
{
  LOG(("css::Loader::DoSheetComplete"));
  MOZ_ASSERT(aLoadData, "Must have a load data!");
  MOZ_ASSERT(aLoadData->mSheet, "Must have a sheet");
  NS_ASSERTION(mSheets, "mLoadingDatas should be initialized by now.");

  // Twiddle the hashtables
  if (aLoadData->mURI) {
    LOG_URI("  Finished loading: '%s'", aLoadData->mURI);
    // Remove the data from the list of loading datas
    if (aLoadData->mIsLoading) {
      URIPrincipalReferrerPolicyAndCORSModeHashKey key(aLoadData->mURI,
                                         aLoadData->mLoaderPrincipal,
                                         aLoadData->mSheet->GetCORSMode(),
                                         aLoadData->mSheet->GetReferrerPolicy());
#ifdef DEBUG
      SheetLoadData *loadingData;
      NS_ASSERTION(
        mSheets->mLoadingDatas.Get(&key, &loadingData) &&
        loadingData == aLoadData,
        "Bad loading table");
#endif

      mSheets->mLoadingDatas.Remove(&key);
      aLoadData->mIsLoading = false;
    }
  }

  // Go through and deal with the whole linked list.
  SheetLoadData* data = aLoadData;
  while (data) {
    if (!data->mSheetAlreadyComplete) {
      // If mSheetAlreadyComplete, then the sheet could well be modified between
      // when we posted the async call to SheetComplete and now, since the sheet
      // was page-accessible during that whole time.
      MOZ_ASSERT(!data->mSheet->HasForcedUniqueInner(),
                 "should not get a forced unique inner during parsing");
      data->mSheet->SetComplete();
      data->ScheduleLoadEventIfNeeded();
    }
    if (data->mMustNotify && (data->mObserver || !mObservers.IsEmpty())) {
      // Don't notify here so we don't trigger script.  Remember the
      // info we need to notify, then do it later when it's safe.
      aDatasToNotify.AppendElement(data);

      // On append failure, just press on.  We'll fail to notify the observer,
      // but not much we can do about that....
    }

    NS_ASSERTION(!data->mParentData ||
                 data->mParentData->mPendingChildren != 0,
                 "Broken pending child count on our parent");

    // If we have a parent, our parent is no longer being parsed, and
    // we are the last pending child, then our load completion
    // completes the parent too.  Note that the parent _can_ still be
    // being parsed (eg if the child (us) failed to open the channel
    // or some such).
    if (data->mParentData &&
        --(data->mParentData->mPendingChildren) == 0 &&
        !data->mParentData->mIsBeingParsed) {
      DoSheetComplete(data->mParentData, aDatasToNotify);
    }

    data = data->mNext;
  }

  // Now that it's marked complete, put the sheet in our cache.
  // If we ever start doing this for failed loads, we'll need to
  // adjust the PostLoadEvent code that thinks anything already
  // complete must have loaded succesfully.
  if (!aLoadData->mLoadFailed && aLoadData->mURI) {
    // Pick our sheet to cache carefully.  Ideally, we want to cache
    // one of the sheets that will be kept alive by a document or
    // parent sheet anyway, so that if someone then accesses it via
    // CSSOM we won't have extra clones of the inner lying around.
    data = aLoadData;
    StyleSheet* sheet = aLoadData->mSheet;
    while (data) {
      if (data->mSheet->GetParentSheet() || data->mSheet->GetOwnerNode()) {
        sheet = data->mSheet;
        break;
      }
      data = data->mNext;
    }
#ifdef MOZ_XUL
    if (IsChromeURI(aLoadData->mURI)) {
      nsXULPrototypeCache* cache = nsXULPrototypeCache::GetInstance();
      if (cache && cache->IsEnabled()) {
        if (!cache->GetStyleSheet(aLoadData->mURI)) {
          LOG(("  Putting sheet in XUL prototype cache"));
          NS_ASSERTION(sheet->IsComplete(),
                       "Should only be caching complete sheets");
          cache->PutStyleSheet(sheet);
        }
      }
    }
    else {
#endif
      URIPrincipalReferrerPolicyAndCORSModeHashKey key(aLoadData->mURI,
                                         aLoadData->mLoaderPrincipal,
                                         aLoadData->mSheet->GetCORSMode(),
                                         aLoadData->mSheet->GetReferrerPolicy());
      NS_ASSERTION(sheet->IsComplete(),
                   "Should only be caching complete sheets");
      mSheets->mCompleteSheets.Put(&key, sheet);
#ifdef MOZ_XUL
    }
#endif
  }

  NS_RELEASE(aLoadData);  // this will release parents and siblings and all that
}

void
Loader::MarkLoadTreeFailed(SheetLoadData* aLoadData)
{
  if (aLoadData->mURI) {
    LOG_URI("  Load failed: '%s'", aLoadData->mURI);
  }

  do {
    aLoadData->mLoadFailed = true;

    if (aLoadData->mParentData) {
      MarkLoadTreeFailed(aLoadData->mParentData);
    }

    aLoadData = aLoadData->mNext;
  } while (aLoadData);
}

Result<Loader::LoadSheetResult, nsresult>
Loader::LoadInlineStyle(const SheetInfo& aInfo,
                        const nsAString& aBuffer,
                        uint32_t aLineNumber,
                        nsICSSLoaderObserver* aObserver)
{
  LOG(("css::Loader::LoadInlineStyle"));

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  if (!mDocument) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  nsCOMPtr<nsIStyleSheetLinkingElement> owningElement(
      do_QueryInterface(aInfo.mContent));
  NS_ASSERTION(owningElement, "Element is not a style linking element!");

  // Since we're not planning to load a URI, no need to hand a principal to the
  // load data or to CreateSheet().

  // Check IsAlternateSheet now, since it can mutate our document.
  auto isAlternate = IsAlternateSheet(aInfo.mTitle, aInfo.mHasAlternateRel);

  StyleSheetState state;
  RefPtr<StyleSheet> sheet;
  nsresult rv = CreateSheet(aInfo,
                            nullptr,
                            eAuthorSheetFeatures,
                            false,
                            state,
                            &sheet);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }
  NS_ASSERTION(state == eSheetNeedsParser,
               "Inline sheets should not be cached");

  LOG(("  Sheet is alternate: %d", static_cast<int>(isAlternate)));

  auto matched =
    PrepareSheet(sheet, aInfo.mTitle, aInfo.mMedia, nullptr, isAlternate);

  if (aInfo.mContent->HasFlag(NODE_IS_IN_SHADOW_TREE)) {
    ShadowRoot* containingShadow = aInfo.mContent->GetContainingShadow();
    MOZ_ASSERT(containingShadow);
    containingShadow->InsertSheet(sheet, aInfo.mContent);
  } else {
    rv = InsertSheetInDoc(sheet, aInfo.mContent, mDocument);
    if (NS_FAILED(rv)) {
      return Err(rv);
    }
  }

  nsIPrincipal* principal = aInfo.mContent->NodePrincipal();
  if (aInfo.mTriggeringPrincipal) {
    // The triggering principal may be an expanded principal, which is safe to
    // use for URL security checks, but not as the loader principal for a
    // stylesheet. So treat this as principal inheritance, and downgrade if
    // necessary.
    principal =
      BasePrincipal::Cast(aInfo.mTriggeringPrincipal)->PrincipalToInherit();
  }

  SheetLoadData* data = new SheetLoadData(this,
                                          aInfo.mTitle,
                                          nullptr,
                                          sheet,
                                          owningElement,
                                          isAlternate,
                                          matched,
                                          aObserver,
                                          nullptr,
                                          aInfo.mContent);

  // We never actually load this, so just set its principal directly
  sheet->SetPrincipal(principal);

  NS_ADDREF(data);
  data->mLineNumber = aLineNumber;
  bool completed = true;
  // Parse completion releases the load data.
  //
  // Note that we need to parse synchronously, since the web expects that the
  // effects of inline stylesheets are visible immediately (aside from @imports).
  ParseSheet(aBuffer, EmptyCString(), data, /* aAllowAsync = */ false, completed);

  // If completed is true, |data| may well be deleted by now.
  if (!completed) {
    data->mMustNotify = true;
  }
  return LoadSheetResult { completed ? Completed::Yes : Completed::No, isAlternate, matched };
}

Result<Loader::LoadSheetResult, nsresult>
Loader::LoadStyleLink(const SheetInfo& aInfo, nsICSSLoaderObserver* aObserver)
{
  MOZ_ASSERT(aInfo.mURI, "Must have URL to load");
  LOG(("css::Loader::LoadStyleLink"));
  LOG_URI("  Link uri: '%s'", aInfo.mURI);
  LOG(("  Link title: '%s'", NS_ConvertUTF16toUTF8(aInfo.mTitle).get()));
  LOG(("  Link media: '%s'", NS_ConvertUTF16toUTF8(aInfo.mMedia).get()));
  LOG(("  Link alternate rel: %d", aInfo.mHasAlternateRel));

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return Err(NS_ERROR_NOT_AVAILABLE);
  }

  if (!mDocument) {
    return Err(NS_ERROR_NOT_INITIALIZED);
  }

  nsIPrincipal* loadingPrincipal = aInfo.mContent
    ? aInfo.mContent->NodePrincipal()
    : mDocument->NodePrincipal();

  nsIPrincipal* principal = aInfo.mTriggeringPrincipal
    ? aInfo.mTriggeringPrincipal.get()
    : loadingPrincipal;

  nsINode* context = aInfo.mContent;
  if (!context) {
    context = mDocument;
  }

  nsresult rv = CheckContentPolicy(loadingPrincipal, principal, aInfo.mURI, context, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Don't fire the error event if our document is loaded as data.  We're
    // supposed to not even try to do loads in that case... Unfortunately, we
    // implement that via nsDataDocumentContentPolicy, which doesn't have a good
    // way to communicate back to us that _it_ is the thing that blocked the
    // load.
    if (aInfo.mContent && !mDocument->IsLoadedAsData()) {
      // Fire an async error event on it.
      RefPtr<AsyncEventDispatcher> loadBlockingAsyncDispatcher =
        new LoadBlockingAsyncEventDispatcher(aInfo.mContent,
                                             NS_LITERAL_STRING("error"),
                                             false, false);
      loadBlockingAsyncDispatcher->PostDOMEvent();
    }
    return Err(rv);
  }

  StyleSheetState state;
  RefPtr<StyleSheet> sheet;
  auto isAlternate = IsAlternateSheet(aInfo.mTitle, aInfo.mHasAlternateRel);
  rv = CreateSheet(aInfo,
                   principal,
                   eAuthorSheetFeatures,
                   false,
                   state,
                   &sheet);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  LOG(("  Sheet is alternate: %d", static_cast<int>(isAlternate)));

  auto matched =
    PrepareSheet(sheet, aInfo.mTitle, aInfo.mMedia, nullptr, isAlternate);

  // FIXME(emilio, bug 1410578): Shadow DOM should be handled here too.
  rv = InsertSheetInDoc(sheet, aInfo.mContent, mDocument);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  nsCOMPtr<nsIStyleSheetLinkingElement> owningElement(
    do_QueryInterface(aInfo.mContent));

  if (state == eSheetComplete) {
    LOG(("  Sheet already complete: 0x%p", sheet.get()));
    if (aObserver || !mObservers.IsEmpty() || owningElement) {
      rv = PostLoadEvent(aInfo.mURI,
                         sheet,
                         aObserver,
                         isAlternate,
                         matched,
                         owningElement);
      if (NS_FAILED(rv)) {
        return Err(rv);
      }
    }

    // The load hasn't been completed yet, will be done in PostLoadEvent.
    return LoadSheetResult { Completed::No, isAlternate, matched };
  }

  // Now we need to actually load it.
  SheetLoadData* data = new SheetLoadData(this,
                                          aInfo.mTitle,
                                          aInfo.mURI,
                                          sheet,
                                          owningElement,
                                          isAlternate,
                                          matched,
                                          aObserver,
                                          principal,
                                          context);
  NS_ADDREF(data);

  auto result = LoadSheetResult { Completed::No, isAlternate, matched };

  MOZ_ASSERT(result.ShouldBlock() == !data->ShouldDefer(),
             "These should better match!");

  // If we have to parse and it's a non-blocking non-inline sheet, defer it.
  if (state == eSheetNeedsParser &&
      mSheets->mLoadingDatas.Count() != 0 &&
      !result.ShouldBlock()) {
    LOG(("  Deferring sheet load"));
    URIPrincipalReferrerPolicyAndCORSModeHashKey key(data->mURI,
                                                     data->mLoaderPrincipal,
                                                     data->mSheet->GetCORSMode(),
                                                     data->mSheet->GetReferrerPolicy());
    mSheets->mPendingDatas.Put(&key, data);

    data->mMustNotify = true;
    return result;
  }

  // Load completion will free the data
  rv = LoadSheet(data, state, false);
  if (NS_FAILED(rv)) {
    return Err(rv);
  }

  data->mMustNotify = true;
  return result;
}

static bool
HaveAncestorDataWithURI(SheetLoadData *aData, nsIURI *aURI)
{
  if (!aData->mURI) {
    // Inline style; this won't have any ancestors
    MOZ_ASSERT(!aData->mParentData,
               "How does inline style have a parent?");
    return false;
  }

  bool equal;
  if (NS_FAILED(aData->mURI->Equals(aURI, &equal)) || equal) {
    return true;
  }

  // Datas down the mNext chain have the same URI as aData, so we
  // don't have to compare to them.  But they might have different
  // parents, and we have to check all of those.
  while (aData) {
    if (aData->mParentData &&
        HaveAncestorDataWithURI(aData->mParentData, aURI)) {
      return true;
    }

    aData = aData->mNext;
  }

  return false;
}

nsresult
Loader::LoadChildSheet(StyleSheet* aParentSheet,
                       SheetLoadData* aParentData,
                       nsIURI* aURL,
                       dom::MediaList* aMedia,
                       LoaderReusableStyleSheets* aReusableSheets)
{
  LOG(("css::Loader::LoadChildSheet"));
  MOZ_ASSERT(aURL, "Must have a URI to load");
  MOZ_ASSERT(aParentSheet, "Must have a parent sheet");

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  LOG_URI("  Child uri: '%s'", aURL);

  nsCOMPtr<nsINode> owningNode;

  // Check for an associated document or shadow root: if none, don't bother
  // walking up the parent sheets.
  if (aParentSheet->GetAssociatedDocumentOrShadowRoot()) {
    StyleSheet* topSheet = aParentSheet;
    while (StyleSheet* parent = topSheet->GetParentSheet()) {
      topSheet = parent;
    }
    owningNode = topSheet->GetOwnerNode();
  }

  nsINode* context = nullptr;
  nsIPrincipal* loadingPrincipal = nullptr;
  if (owningNode) {
    context = owningNode;
    loadingPrincipal = owningNode->NodePrincipal();
  } else if (mDocument) {
    context = mDocument;
    loadingPrincipal = mDocument->NodePrincipal();
  }

  nsIPrincipal* principal = aParentSheet->Principal();
  nsresult rv = CheckContentPolicy(loadingPrincipal, principal, aURL, context, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (aParentData) {
      MarkLoadTreeFailed(aParentData);
    }
    return rv;
  }

  nsCOMPtr<nsICSSLoaderObserver> observer;

  if (aParentData) {
    LOG(("  Have a parent load"));
    // Check for cycles
    if (HaveAncestorDataWithURI(aParentData, aURL)) {
      // Houston, we have a loop, blow off this child and pretend this never
      // happened
      LOG_ERROR(("  @import cycle detected, dropping load"));
      return NS_OK;
    }

    NS_ASSERTION(aParentData->mSheet == aParentSheet,
                 "Unexpected call to LoadChildSheet");
  } else {
    LOG(("  No parent load; must be CSSOM"));
    // No parent load data, so the sheet will need to be notified when
    // we finish, if it can be, if we do the load asynchronously.
    observer = aParentSheet;
  }

  // Now that we know it's safe to load this (passes security check and not a
  // loop) do so.
  RefPtr<StyleSheet> sheet;
  StyleSheetState state;
  if (aReusableSheets && aReusableSheets->FindReusableStyleSheet(aURL, sheet)) {
    state = eSheetComplete;
  } else {
    const nsAString& empty = EmptyString();
    // For now, use CORS_NONE for child sheets
    rv = CreateSheet(aURL,
                     nullptr,
                     principal,
                     aParentSheet->ParsingMode(),
                     CORS_NONE,
                     aParentSheet->GetReferrerPolicy(),
                     EmptyString(), // integrity is only checked on main sheet
                     aParentData ? aParentData->mSyncLoad : false,
                     state,
                     &sheet);
    NS_ENSURE_SUCCESS(rv, rv);

    PrepareSheet(sheet, empty, empty, aMedia, IsAlternate::No);
  }

  rv = InsertChildSheet(sheet, aParentSheet);
  NS_ENSURE_SUCCESS(rv, rv);

  if (state == eSheetComplete) {
    LOG(("  Sheet already complete"));
    // We're completely done.  No need to notify, even, since the
    // @import rule addition/modification will trigger the right style
    // changes automatically.
    return NS_OK;
  }

  SheetLoadData* data = new SheetLoadData(this, aURL, sheet, aParentData,
                                          observer, principal, context);

  NS_ADDREF(data);
  bool syncLoad = data->mSyncLoad;

  // Load completion will release the data
  rv = LoadSheet(data, state, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // If syncLoad is true, |data| will be deleted by now.
  if (!syncLoad) {
    data->mMustNotify = true;
  }
  return rv;
}

nsresult
Loader::LoadSheetSync(nsIURI* aURL,
                      SheetParsingMode aParsingMode,
                      bool aUseSystemPrincipal,
                      RefPtr<StyleSheet>* aSheet)
{
  LOG(("css::Loader::LoadSheetSync"));
  return InternalLoadNonDocumentSheet(aURL,
                                      false,
                                      aParsingMode,
                                      aUseSystemPrincipal,
                                      nullptr,
                                      nullptr,
                                      aSheet,
                                      nullptr);
}

nsresult
Loader::LoadSheet(nsIURI* aURL,
                  SheetParsingMode aParsingMode,
                  bool aUseSystemPrincipal,
                  nsICSSLoaderObserver* aObserver,
                  RefPtr<StyleSheet>* aSheet)
{
  LOG(("css::Loader::LoadSheet(aURL, aParsingMode, aUseSystemPrincipal, aObserver, aSheet)"));
  return InternalLoadNonDocumentSheet(aURL,
                                      false,
                                      aParsingMode,
                                      aUseSystemPrincipal,
                                      nullptr,
                                      nullptr,
                                      aSheet,
                                      aObserver);
}

nsresult
Loader::LoadSheet(nsIURI* aURL,
                  nsIPrincipal* aOriginPrincipal,
                  nsICSSLoaderObserver* aObserver,
                  RefPtr<StyleSheet>* aSheet)
{
  LOG(("css::Loader::LoadSheet(aURL, aObserver, aSheet) api call"));
  MOZ_ASSERT(aSheet, "aSheet is null");
  return InternalLoadNonDocumentSheet(aURL,
                                      false,
                                      eAuthorSheetFeatures,
                                      false,
                                      aOriginPrincipal,
                                      nullptr,
                                      aSheet,
                                      aObserver);
}

nsresult
Loader::LoadSheet(nsIURI* aURL,
                  bool aIsPreload,
                  nsIPrincipal* aOriginPrincipal,
                  const Encoding* aPreloadEncoding,
                  nsICSSLoaderObserver* aObserver,
                  CORSMode aCORSMode,
                  ReferrerPolicy aReferrerPolicy,
                  const nsAString& aIntegrity)
{
  LOG(("css::Loader::LoadSheet(aURL, aObserver) api call"));
  return InternalLoadNonDocumentSheet(aURL,
                                      aIsPreload,
                                      eAuthorSheetFeatures,
                                      false,
                                      aOriginPrincipal,
                                      aPreloadEncoding,
                                      nullptr,
                                      aObserver,
                                      aCORSMode,
                                      aReferrerPolicy,
                                      aIntegrity);
}

nsresult
Loader::InternalLoadNonDocumentSheet(nsIURI* aURL,
                                     bool aIsPreload,
                                     SheetParsingMode aParsingMode,
                                     bool aUseSystemPrincipal,
                                     nsIPrincipal* aOriginPrincipal,
                                     const Encoding* aPreloadEncoding,
                                     RefPtr<StyleSheet>* aSheet,
                                     nsICSSLoaderObserver* aObserver,
                                     CORSMode aCORSMode,
                                     ReferrerPolicy aReferrerPolicy,
                                     const nsAString& aIntegrity)
{
  MOZ_ASSERT(aURL, "Must have a URI to load");
  MOZ_ASSERT(aSheet || aObserver, "Sheet and observer can't both be null");
  MOZ_ASSERT(!aUseSystemPrincipal || !aObserver,
             "Shouldn't load system-principal sheets async");

  LOG_URI("  Non-document sheet uri: '%s'", aURL);

  if (aSheet) {
    *aSheet = nullptr;
  }

  if (!mEnabled) {
    LOG_WARN(("  Not enabled"));
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIPrincipal> loadingPrincipal = (aOriginPrincipal && mDocument
                                             ? mDocument->NodePrincipal()
                                             : nullptr);
  nsresult rv = CheckContentPolicy(loadingPrincipal, aOriginPrincipal,
                                   aURL, mDocument, aIsPreload);
  NS_ENSURE_SUCCESS(rv, rv);

  StyleSheetState state;
  RefPtr<StyleSheet> sheet;
  bool syncLoad = (aObserver == nullptr);
  const nsAString& empty = EmptyString();
  rv = CreateSheet(aURL, nullptr, aOriginPrincipal, aParsingMode,
                   aCORSMode, aReferrerPolicy, aIntegrity, syncLoad,
                   state, &sheet);
  NS_ENSURE_SUCCESS(rv, rv);

  PrepareSheet(sheet, empty, empty, nullptr, IsAlternate::No);

  if (state == eSheetComplete) {
    LOG(("  Sheet already complete"));
    if (aObserver || !mObservers.IsEmpty()) {
      rv = PostLoadEvent(aURL,
                         sheet,
                         aObserver,
                         IsAlternate::No,
                         MediaMatched::Yes,
                         nullptr);
    }
    if (aSheet) {
      sheet.swap(*aSheet);
    }
    return rv;
  }

  SheetLoadData* data = new SheetLoadData(this,
                                          aURL,
                                          sheet,
                                          syncLoad,
                                          aUseSystemPrincipal,
                                          aPreloadEncoding,
                                          aObserver,
                                          aOriginPrincipal,
                                          mDocument);

  NS_ADDREF(data);
  rv = LoadSheet(data, state, aIsPreload);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aSheet) {
    sheet.swap(*aSheet);
  }
  if (aObserver) {
    data->mMustNotify = true;
  }

  return rv;
}

nsresult
Loader::PostLoadEvent(nsIURI* aURI,
                      StyleSheet* aSheet,
                      nsICSSLoaderObserver* aObserver,
                      IsAlternate aWasAlternate,
                      MediaMatched aMediaMatched,
                      nsIStyleSheetLinkingElement* aElement)
{
  LOG(("css::Loader::PostLoadEvent"));
  MOZ_ASSERT(aSheet, "Must have sheet");
  MOZ_ASSERT(aObserver || !mObservers.IsEmpty() || aElement,
             "Must have observer or element");

  RefPtr<SheetLoadData> evt =
    new SheetLoadData(this,
                      EmptyString(), // title doesn't matter here
                      aURI,
                      aSheet,
                      aElement,
                      aWasAlternate,
                      aMediaMatched,
                      aObserver,
                      nullptr,
                      mDocument);

  if (!mPostedEvents.AppendElement(evt)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv;
  RefPtr<SheetLoadData> runnable(evt);
  if (mDocument) {
    rv = mDocument->Dispatch(TaskCategory::Other, runnable.forget());
  } else if (mDocGroup) {
    rv = mDocGroup->Dispatch(TaskCategory::Other, runnable.forget());
  } else {
    rv = SystemGroup::Dispatch(TaskCategory::Other, runnable.forget());
  }

  if (NS_FAILED(rv)) {
    NS_WARNING("failed to dispatch stylesheet load event");
    mPostedEvents.RemoveElement(evt);
  } else {
    // We'll unblock onload when we handle the event.
    BlockOnload();

    // We want to notify the observer for this data.
    evt->mMustNotify = true;
    evt->mSheetAlreadyComplete = true;

    // If we get to this code, aSheet loaded correctly at some point, so
    // we can just schedule a load event and don't need to touch the
    // data's mLoadFailed.  Note that we do this here and not from
    // inside our SheetComplete so that we don't end up running the load
    // event async.
    MOZ_ASSERT(!evt->mLoadFailed, "Why are we marked as failed?");
    evt->ScheduleLoadEventIfNeeded();
  }

  return rv;
}

void
Loader::HandleLoadEvent(SheetLoadData* aEvent)
{
  // XXXbz can't assert this yet.... May not have an observer because
  // we're unblocking the parser
  // NS_ASSERTION(aEvent->mObserver, "Must have observer");
  NS_ASSERTION(aEvent->mSheet, "Must have sheet");

  // Very important: this needs to come before the SheetComplete call
  // below, so that HasPendingLoads() will test true as needed under
  // notifications we send from that SheetComplete call.
  mPostedEvents.RemoveElement(aEvent);

  if (!aEvent->mIsCancelled) {
    // SheetComplete will call Release(), so give it a reference to do
    // that with.
    NS_ADDREF(aEvent);
    SheetComplete(aEvent, NS_OK);
  }

  UnblockOnload(true);
}

static void
StopLoadingSheets(
  nsDataHashtable<URIPrincipalReferrerPolicyAndCORSModeHashKey, SheetLoadData*>& aDatas,
  Loader::LoadDataArray& aArr)
{
  for (auto iter = aDatas.Iter(); !iter.Done(); iter.Next()) {
    SheetLoadData* data = iter.Data();
    MOZ_ASSERT(data, "Must have a data!");

    data->mIsLoading = false; // we will handle the removal right here
    data->mIsCancelled = true;

    aArr.AppendElement(data);

    iter.Remove();
  }
}

nsresult
Loader::Stop()
{
  uint32_t pendingCount = mSheets ? mSheets->mPendingDatas.Count() : 0;
  uint32_t loadingCount = mSheets ? mSheets->mLoadingDatas.Count() : 0;
  LoadDataArray arr(pendingCount + loadingCount + mPostedEvents.Length());

  if (pendingCount) {
    StopLoadingSheets(mSheets->mPendingDatas, arr);
  }
  if (loadingCount) {
    StopLoadingSheets(mSheets->mLoadingDatas, arr);
  }

  uint32_t i;
  for (i = 0; i < mPostedEvents.Length(); ++i) {
    SheetLoadData* data = mPostedEvents[i];
    data->mIsCancelled = true;
    if (arr.AppendElement(data)) {
      // SheetComplete() calls Release(), so give this an extra ref.
      NS_ADDREF(data);
    }
#ifdef DEBUG
    else {
      NS_NOTREACHED("We preallocated this memory... shouldn't really fail, "
                    "except we never check that preallocation succeeds.");
    }
#endif
  }
  mPostedEvents.Clear();

  mDatasToNotifyOn += arr.Length();
  for (i = 0; i < arr.Length(); ++i) {
    --mDatasToNotifyOn;
    SheetComplete(arr[i], NS_BINDING_ABORTED);
  }
  return NS_OK;
}

bool
Loader::HasPendingLoads()
{
  return
    (mSheets && mSheets->mLoadingDatas.Count() != 0) ||
    (mSheets && mSheets->mPendingDatas.Count() != 0) ||
    mPostedEvents.Length() != 0 ||
    mDatasToNotifyOn != 0;
}

nsresult
Loader::AddObserver(nsICSSLoaderObserver* aObserver)
{
  MOZ_ASSERT(aObserver, "Must have observer");
  if (mObservers.AppendElementUnlessExists(aObserver)) {
    return NS_OK;
  }

  return NS_ERROR_OUT_OF_MEMORY;
}

void
Loader::RemoveObserver(nsICSSLoaderObserver* aObserver)
{
  mObservers.RemoveElement(aObserver);
}

void
Loader::StartDeferredLoads()
{
  MOZ_ASSERT(mSheets, "Don't call me!");
  LoadDataArray arr(mSheets->mPendingDatas.Count());
  for (auto iter = mSheets->mPendingDatas.Iter(); !iter.Done(); iter.Next()) {
    arr.AppendElement(iter.Data());
    iter.Remove();
  }

  mDatasToNotifyOn += arr.Length();
  for (uint32_t i = 0; i < arr.Length(); ++i) {
    --mDatasToNotifyOn;
    LoadSheet(arr[i], eSheetNeedsParser, false);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Loader)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Loader)
  if (tmp->mSheets) {
    for (auto iter = tmp->mSheets->mCompleteSheets.Iter();
         !iter.Done();
         iter.Next()) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "Sheet cache nsCSSLoader");
      cb.NoteXPCOMChild(iter.UserData());
    }
  }
  nsTObserverArray<nsCOMPtr<nsICSSLoaderObserver>>::ForwardIterator
    it(tmp->mObservers);
  while (it.HasMore()) {
    ImplCycleCollectionTraverse(cb, it.GetNext(),
                                "mozilla::css::Loader.mObservers");
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Loader)
  if (tmp->mSheets) {
    tmp->mSheets->mCompleteSheets.Clear();
  }
  tmp->mObservers.Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Loader, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Loader, Release)

size_t
Loader::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  if (mSheets) {
    n += mSheets->mCompleteSheets.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto iter = mSheets->mCompleteSheets.ConstIter();
         !iter.Done();
         iter.Next()) {
      // If aSheet has a parent, then its parent will report it so we don't
      // have to worry about it here. Likewise, if aSheet has an owning node,
      // then the document that node is in will report it.
      const StyleSheet* sheet = iter.UserData();
      n += (sheet->GetOwnerNode() || sheet->GetParentSheet())
         ? 0
         : sheet->SizeOfIncludingThis(aMallocSizeOf);
    }
  }
  n += mObservers.ShallowSizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mLoadingDatas: transient, and should be small
  // - mPendingDatas: transient, and should be small
  // - mPostedEvents: transient, and should be small
  //
  // The following members aren't measured:
  // - mDocument, because it's a weak backpointer

  return n;
}

void
Loader::BlockOnload()
{
  if (mDocument) {
    mDocument->BlockOnload();
  }
}

void
Loader::UnblockOnload(bool aFireSync)
{
  if (mDocument) {
    mDocument->UnblockOnload(aFireSync);
  }
}

already_AddRefed<nsISerialEventTarget>
Loader::DispatchTarget()
{
  nsCOMPtr<nsISerialEventTarget> target;
  if (mDocument) {
    target = mDocument->EventTargetFor(TaskCategory::Other);
  } else if (mDocGroup) {
    target = mDocGroup->EventTargetFor(TaskCategory::Other);
  } else {
    target = SystemGroup::EventTargetFor(TaskCategory::Other);
  }

  return target.forget();
}

} // namespace css
} // namespace mozilla

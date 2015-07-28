/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#include "nsScriptLoader.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "xpcpublic.h"
#include "nsIUnicodeDecoder.h"
#include "nsIContent.h"
#include "nsJSUtils.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/Element.h"
#include "nsGkAtoms.h"
#include "nsNetUtil.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsJSPrincipals.h"
#include "nsContentPolicyUtils.h"
#include "nsIHttpChannel.h"
#include "nsIClassOfService.h"
#include "nsITimedChannel.h"
#include "nsIScriptElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDocShell.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "nsIXPConnect.h"
#include "nsError.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/Logging.h"
#include "nsCRT.h"
#include "nsContentCreatorFunctions.h"
#include "nsCORSListenerProxy.h"
#include "nsProxyRelease.h"
#include "nsSandboxFlags.h"
#include "nsContentTypeParser.h"
#include "nsINetworkPredictor.h"
#include "ImportManager.h"
#include "mozilla/dom/EncodingUtils.h"

#include "mozilla/Attributes.h"
#include "mozilla/unused.h"

static PRLogModuleInfo* gCspPRLog;

using namespace mozilla;
using namespace mozilla::dom;

// The nsScriptLoadRequest is passed as the context to necko, and thus
// it needs to be threadsafe. Necko won't do anything with this
// context, but it will AddRef and Release it on other threads.
NS_IMPL_ISUPPORTS0(nsScriptLoadRequest)

nsScriptLoadRequestList::~nsScriptLoadRequestList()
{
  Clear();
}

void
nsScriptLoadRequestList::Clear()
{
  while (!isEmpty()) {
    nsRefPtr<nsScriptLoadRequest> first = StealFirst();
    first->Cancel();
    // And just let it go out of scope and die.
  }
}

#ifdef DEBUG
bool
nsScriptLoadRequestList::Contains(nsScriptLoadRequest* aElem)
{
  for (nsScriptLoadRequest* req = getFirst();
       req; req = req->getNext()) {
    if (req == aElem) {
      return true;
    }
  }

  return false;
}
#endif // DEBUG

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

nsScriptLoader::nsScriptLoader(nsIDocument *aDocument)
  : mDocument(aDocument),
    mBlockerCount(0),
    mEnabled(true),
    mDeferEnabled(false),
    mDocumentParsingDone(false),
    mBlockingDOMContentLoaded(false)
{
  // enable logging for CSP
  if (!gCspPRLog)
    gCspPRLog = PR_NewLogModule("CSP");
}

nsScriptLoader::~nsScriptLoader()
{
  mObservers.Clear();

  if (mParserBlockingRequest) {
    mParserBlockingRequest->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mXSLTRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mDeferRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mLoadingAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (nsScriptLoadRequest* req = mLoadedAsyncRequests.getFirst(); req;
       req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for(nsScriptLoadRequest* req = mNonAsyncExternalScriptInsertedRequests.getFirst();
      req;
      req = req->getNext()) {
    req->FireScriptAvailable(NS_ERROR_ABORT);
  }

  // Unblock the kids, in case any of them moved to a different document
  // subtree in the meantime and therefore aren't actually going away.
  for (uint32_t j = 0; j < mPendingChildLoaders.Length(); ++j) {
    mPendingChildLoaders[j]->RemoveExecuteBlocker();
  }  
}

NS_IMPL_ISUPPORTS(nsScriptLoader, nsIStreamLoaderObserver)

// Helper method for checking if the script element is an event-handler
// This means that it has both a for-attribute and a event-attribute.
// Also, if the for-attribute has a value that matches "\s*window\s*",
// and the event-attribute matches "\s*onload([ \(].*)?" then it isn't an
// eventhandler. (both matches are case insensitive).
// This is how IE seems to filter out a window's onload handler from a
// <script for=... event=...> element.

static bool
IsScriptEventHandler(nsIContent* aScriptElement)
{
  if (!aScriptElement->IsHTMLElement()) {
    return false;
  }

  nsAutoString forAttr, eventAttr;
  if (!aScriptElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_for, forAttr) ||
      !aScriptElement->GetAttr(kNameSpaceID_None, nsGkAtoms::event, eventAttr)) {
    return false;
  }

  const nsAString& for_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(forAttr);
  if (!for_str.LowerCaseEqualsLiteral("window")) {
    return true;
  }

  // We found for="window", now check for event="onload".
  const nsAString& event_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(eventAttr, false);
  if (!StringBeginsWith(event_str, NS_LITERAL_STRING("onload"),
                        nsCaseInsensitiveStringComparator())) {
    // It ain't "onload.*".

    return true;
  }

  nsAutoString::const_iterator start, end;
  event_str.BeginReading(start);
  event_str.EndReading(end);

  start.advance(6); // advance past "onload"

  if (start != end && *start != '(' && *start != ' ') {
    // We got onload followed by something other than space or
    // '('. Not good enough.

    return true;
  }

  return false;
}

nsresult
nsScriptLoader::CheckContentPolicy(nsIDocument* aDocument,
                                   nsISupports *aContext,
                                   nsIURI *aURI,
                                   const nsAString &aType)
{
  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_INTERNAL_SCRIPT,
                                          aURI,
                                          aDocument->NodePrincipal(),
                                          aContext,
                                          NS_LossyConvertUTF16toASCII(aType),
                                          nullptr,    //extra
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
      return NS_ERROR_CONTENT_BLOCKED;
    }
    return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
  }

  return NS_OK;
}

nsresult
nsScriptLoader::ShouldLoadScript(nsIDocument* aDocument,
                                 nsISupports* aContext,
                                 nsIURI* aURI,
                                 const nsAString &aType)
{
  // Check that the containing page is allowed to load this URI.
  nsresult rv = nsContentUtils::GetSecurityManager()->
    CheckLoadURIWithPrincipal(aDocument->NodePrincipal(), aURI,
                              nsIScriptSecurityManager::ALLOW_CHROME);

  NS_ENSURE_SUCCESS(rv, rv);

  // After the security manager, the content-policy stuff gets a veto
  rv = CheckContentPolicy(aDocument, aContext, aURI, aType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult
nsScriptLoader::StartLoad(nsScriptLoadRequest *aRequest, const nsAString &aType,
                          bool aScriptFromHead)
{
  nsISupports *context = aRequest->mElement.get()
                         ? static_cast<nsISupports *>(aRequest->mElement.get())
                         : static_cast<nsISupports *>(mDocument);
  nsresult rv = ShouldLoadScript(mDocument, context, aRequest->mURI, aType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mDocument->MasterDocument()->GetWindow()));

  if (!window) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIDocShell *docshell = window->GetDocShell();

  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

  // If this document is sandboxed without 'allow-scripts', abort.
  if (mDocument->GetSandboxFlags() & SANDBOXED_SCRIPTS) {
    return NS_OK;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aRequest->mURI,
                     mDocument,
                     nsILoadInfo::SEC_NORMAL,
                     nsIContentPolicy::TYPE_INTERNAL_SCRIPT,
                     loadGroup,
                     prompter,
                     nsIRequest::LOAD_NORMAL |
                     nsIChannel::LOAD_CLASSIFY_URI);

  NS_ENSURE_SUCCESS(rv, rv);

  nsIScriptElement *script = aRequest->mElement;
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(channel));

  if (cos) {
    if (aScriptFromHead &&
        !(script && (script->GetScriptAsync() || script->GetScriptDeferred()))) {
      // synchronous head scripts block lading of most other non js/css
      // content such as images
      cos->AddClassFlags(nsIClassOfService::Leader);
    } else if (!(script && script->GetScriptDeferred())) {
      // other scripts are neither blocked nor prioritized unless marked deferred
      cos->AddClassFlags(nsIClassOfService::Unblocked);
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // HTTP content negotation has little value in this context.
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING("*/*"),
                                  false);
    httpChannel->SetReferrerWithPolicy(mDocument->GetDocumentURI(),
                                       aRequest->mReferrerPolicy);
  }

  nsCOMPtr<nsILoadContext> loadContext(do_QueryInterface(docshell));
  mozilla::net::PredictorLearn(aRequest->mURI, mDocument->GetDocumentURI(),
      nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE, loadContext);

  // Set the initiator type
  nsCOMPtr<nsITimedChannel> timedChannel(do_QueryInterface(httpChannel));
  if (timedChannel) {
    timedChannel->SetInitiatorType(NS_LITERAL_STRING("script"));
  }

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStreamListener> listener = loader.get();

  if (aRequest->mCORSMode != CORS_NONE) {
    bool withCredentials = (aRequest->mCORSMode == CORS_USE_CREDENTIALS);
    nsRefPtr<nsCORSListenerProxy> corsListener =
      new nsCORSListenerProxy(listener, mDocument->NodePrincipal(),
                              withCredentials);
    rv = corsListener->Init(channel, DataURIHandling::Allow);
    NS_ENSURE_SUCCESS(rv, rv);
    listener = corsListener;
  }

  rv = channel->AsyncOpen(listener, aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool
nsScriptLoader::PreloadURIComparator::Equals(const PreloadInfo &aPi,
                                             nsIURI * const &aURI) const
{
  bool same;
  return NS_SUCCEEDED(aPi.mRequest->mURI->Equals(aURI, &same)) &&
         same;
}

class nsScriptRequestProcessor : public nsRunnable
{
private:
  nsRefPtr<nsScriptLoader> mLoader;
  nsRefPtr<nsScriptLoadRequest> mRequest;
public:
  nsScriptRequestProcessor(nsScriptLoader* aLoader,
                           nsScriptLoadRequest* aRequest)
    : mLoader(aLoader)
    , mRequest(aRequest)
  {}
  NS_IMETHODIMP Run()
  {
    return mLoader->ProcessRequest(mRequest);
  }
};

static inline bool
ParseTypeAttribute(const nsAString& aType, JSVersion* aVersion)
{
  MOZ_ASSERT(!aType.IsEmpty());
  MOZ_ASSERT(aVersion);
  MOZ_ASSERT(*aVersion == JSVERSION_DEFAULT);

  nsContentTypeParser parser(aType);

  nsAutoString mimeType;
  nsresult rv = parser.GetType(mimeType);
  NS_ENSURE_SUCCESS(rv, false);

  if (!nsContentUtils::IsJavascriptMIMEType(mimeType)) {
    return false;
  }

  // Get the version string, and ensure the language supports it.
  nsAutoString versionName;
  rv = parser.GetParameter("version", versionName);

  if (NS_SUCCEEDED(rv)) {
    *aVersion = nsContentUtils::ParseJavascriptVersion(versionName);
  } else if (rv != NS_ERROR_INVALID_ARG) {
    return false;
  }

  return true;
}

static bool
CSPAllowsInlineScript(nsIScriptElement *aElement, nsIDocument *aDocument)
{
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  // Note: For imports NodePrincipal and the principal of the master are
  // the same.
  nsresult rv = aDocument->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, false);

  if (!csp) {
    // no CSP --> allow
    return true;
  }

  // An inline script can be allowed because all inline scripts are allowed,
  // or else because it is whitelisted by a nonce-source or hash-source. This
  // is a logical OR between whitelisting methods, so the allowInlineScript
  // outparam can be reused for each check as long as we stop checking as soon
  // as it is set to true. This also optimizes performance by avoiding the
  // overhead of unnecessary checks.
  bool allowInlineScript = true;
  nsAutoTArray<unsigned short, 3> violations;

  bool reportInlineViolation = false;
  rv = csp->GetAllowsInlineScript(&reportInlineViolation, &allowInlineScript);
  NS_ENSURE_SUCCESS(rv, false);
  if (reportInlineViolation) {
    violations.AppendElement(static_cast<unsigned short>(
          nsIContentSecurityPolicy::VIOLATION_TYPE_INLINE_SCRIPT));
  }

  nsAutoString nonce;
  if (!allowInlineScript) {
    nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);
    bool foundNonce = scriptContent->GetAttr(kNameSpaceID_None,
                                             nsGkAtoms::nonce, nonce);
    if (foundNonce) {
      bool reportNonceViolation;
      rv = csp->GetAllowsNonce(nonce, nsIContentPolicy::TYPE_SCRIPT,
                               &reportNonceViolation, &allowInlineScript);
      NS_ENSURE_SUCCESS(rv, false);
      if (reportNonceViolation) {
        violations.AppendElement(static_cast<unsigned short>(
              nsIContentSecurityPolicy::VIOLATION_TYPE_NONCE_SCRIPT));
      }
    }
  }

  if (!allowInlineScript) {
    bool reportHashViolation;
    nsAutoString scriptText;
    aElement->GetScriptText(scriptText);
    rv = csp->GetAllowsHash(scriptText, nsIContentPolicy::TYPE_SCRIPT,
                            &reportHashViolation, &allowInlineScript);
    NS_ENSURE_SUCCESS(rv, false);
    if (reportHashViolation) {
      violations.AppendElement(static_cast<unsigned short>(
            nsIContentSecurityPolicy::VIOLATION_TYPE_HASH_SCRIPT));
    }
  }

  // What violation(s) should be reported?
  //
  // 1. If the script tag has a nonce attribute, and the nonce does not match
  // the policy, report VIOLATION_TYPE_NONCE_SCRIPT.
  // 2. If the policy has at least one hash-source, and the hashed contents of
  // the script tag did not match any of them, report VIOLATION_TYPE_HASH_SCRIPT
  // 3. Otherwise, report VIOLATION_TYPE_INLINE_SCRIPT if appropriate.
  //
  // 1 and 2 may occur together, 3 should only occur by itself. Naturally,
  // every VIOLATION_TYPE_NONCE_SCRIPT and VIOLATION_TYPE_HASH_SCRIPT are also
  // VIOLATION_TYPE_INLINE_SCRIPT, but reporting the
  // VIOLATION_TYPE_INLINE_SCRIPT is redundant and does not help the developer.
  if (!violations.IsEmpty()) {
    MOZ_ASSERT(violations[0] == nsIContentSecurityPolicy::VIOLATION_TYPE_INLINE_SCRIPT,
               "How did we get any violations without an initial inline script violation?");
    // gather information to log with violation report
    nsIURI* uri = aDocument->GetDocumentURI();
    nsAutoCString asciiSpec;
    uri->GetAsciiSpec(asciiSpec);
    nsAutoString scriptText;
    aElement->GetScriptText(scriptText);
    nsAutoString scriptSample(scriptText);

    // cap the length of the script sample at 40 chars
    if (scriptSample.Length() > 40) {
      scriptSample.Truncate(40);
      scriptSample.AppendLiteral("...");
    }

    for (uint32_t i = 0; i < violations.Length(); i++) {
      // Skip reporting the redundant inline script violation if there are
      // other (nonce and/or hash violations) as well.
      if (i > 0 || violations.Length() == 1) {
        csp->LogViolationDetails(violations[i], NS_ConvertUTF8toUTF16(asciiSpec),
                                 scriptSample, aElement->GetScriptLineNumber(),
                                 nonce, scriptText);
      }
    }
  }

  if (!allowInlineScript) {
    NS_ASSERTION(!violations.IsEmpty(),
        "CSP blocked inline script but is not reporting a violation");
   return false;
  }
  return true;
}

bool
nsScriptLoader::ProcessScriptElement(nsIScriptElement *aElement)
{
  // We need a document to evaluate scripts.
  NS_ENSURE_TRUE(mDocument, false);

  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return false;
  }

  NS_ASSERTION(!aElement->IsMalformed(), "Executing malformed script");

  nsCOMPtr<nsIContent> scriptContent = do_QueryInterface(aElement);

  // Step 12. Check that the script is not an eventhandler
  if (IsScriptEventHandler(scriptContent)) {
    return false;
  }

  JSVersion version = JSVERSION_DEFAULT;

  // Check the type attribute to determine language and version.
  // If type exists, it trumps the deprecated 'language='
  nsAutoString type;
  aElement->GetScriptType(type);
  if (!type.IsEmpty()) {
    NS_ENSURE_TRUE(ParseTypeAttribute(type, &version), false);
  } else {
    // no 'type=' element
    // "language" is a deprecated attribute of HTML, so we check it only for
    // HTML script elements.
    if (scriptContent->IsHTMLElement()) {
      nsAutoString language;
      scriptContent->GetAttr(kNameSpaceID_None, nsGkAtoms::language, language);
      if (!language.IsEmpty()) {
        if (!nsContentUtils::IsJavaScriptLanguage(language)) {
          return false;
        }
      }
    }
  }

  // Step 14. in the HTML5 spec
  nsresult rv = NS_OK;
  nsRefPtr<nsScriptLoadRequest> request;
  if (aElement->GetScriptExternal()) {
    // external script
    nsCOMPtr<nsIURI> scriptURI = aElement->GetScriptURI();
    if (!scriptURI) {
      // Asynchronously report the failure to create a URI object
      NS_DispatchToCurrentThread(
        NS_NewRunnableMethod(aElement,
                             &nsIScriptElement::FireErrorEvent));
      return false;
    }

    // Double-check that the preload matches what we're asked to load now.
    mozilla::net::ReferrerPolicy ourRefPolicy = mDocument->GetReferrerPolicy();
    CORSMode ourCORSMode = aElement->GetCORSMode();
    nsTArray<PreloadInfo>::index_type i =
      mPreloads.IndexOf(scriptURI.get(), 0, PreloadURIComparator());
    if (i != nsTArray<PreloadInfo>::NoIndex) {
      // preloaded
      // note that a script-inserted script can steal a preload!
      request = mPreloads[i].mRequest;
      request->mElement = aElement;
      nsString preloadCharset(mPreloads[i].mCharset);
      mPreloads.RemoveElementAt(i);

      // Double-check that the charset the preload used is the same as
      // the charset we have now.
      nsAutoString elementCharset;
      aElement->GetScriptCharset(elementCharset);
      if (elementCharset.Equals(preloadCharset) &&
          ourCORSMode == request->mCORSMode &&
          ourRefPolicy == request->mReferrerPolicy) {
        rv = CheckContentPolicy(mDocument, aElement, request->mURI, type);
        NS_ENSURE_SUCCESS(rv, false);
      } else {
        // Drop the preload
        request = nullptr;
      }
    }

    if (!request) {
      // no usable preload
      request = new nsScriptLoadRequest(aElement, version, ourCORSMode);
      request->mURI = scriptURI;
      request->mIsInline = false;
      request->mProgress = nsScriptLoadRequest::Progress_Loading;
      request->mReferrerPolicy = ourRefPolicy;

      // set aScriptFromHead to false so we don't treat non preloaded scripts as
      // blockers for full page load. See bug 792438.
      rv = StartLoad(request, type, false);
      if (NS_FAILED(rv)) {
        // Asynchronously report the load failure
        NS_DispatchToCurrentThread(
          NS_NewRunnableMethod(aElement,
                               &nsIScriptElement::FireErrorEvent));
        return false;
      }
    }

    // Should still be in loading stage of script.
    NS_ASSERTION(!request->InCompilingStage(),
                 "Request should noet yet be in compiling stage.");

    request->mJSVersion = version;

    if (aElement->GetScriptAsync()) {
      request->mIsAsync = true;
      if (request->IsDoneLoading()) {
        mLoadedAsyncRequests.AppendElement(request);
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.

        // KVKV TODO: Instead of processing immediately, try off-thread-parsing
        // it and only schedule a ProcessRequest if that fails.
        ProcessPendingRequestsAsync();
      } else {
        mLoadingAsyncRequests.AppendElement(request);
      }
      return false;
    }
    if (!aElement->GetParserCreated()) {
      // Violate the HTML5 spec in order to make LABjs and the "order" plug-in
      // for RequireJS work with their Gecko-sniffed code path. See
      // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
      request->mIsNonAsyncScriptInserted = true;
      mNonAsyncExternalScriptInsertedRequests.AppendElement(request);
      if (request->IsDoneLoading()) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return false;
    }
    // we now have a parser-inserted request that may or may not be still
    // loading
    if (aElement->GetScriptDeferred()) {
      // We don't want to run this yet.
      // If we come here, the script is a parser-created script and it has
      // the defer attribute but not the async attribute. Since a
      // a parser-inserted script is being run, we came here by the parser
      // running the script, which means the parser is still alive and the
      // parse is ongoing.
      NS_ASSERTION(mDocument->GetCurrentContentSink() ||
                   aElement->GetParserCreated() == FROM_PARSER_XSLT,
          "Non-XSLT Defer script on a document without an active parser; bug 592366.");
      AddDeferRequest(request);
      return false;
    }

    if (aElement->GetParserCreated() == FROM_PARSER_XSLT) {
      // Need to maintain order for XSLT-inserted scripts
      NS_ASSERTION(!mParserBlockingRequest,
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      request->mIsXSLT = true;
      mXSLTRequests.AppendElement(request);
      if (request->IsDoneLoading()) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return true;
    }
    if (request->IsDoneLoading() && ReadyToExecuteScripts()) {
      // The request has already been loaded and there are no pending style
      // sheets. If the script comes from the network stream, cheat for
      // performance reasons and avoid a trip through the event loop.
      if (aElement->GetParserCreated() == FROM_PARSER_NETWORK) {
        return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
      }
      // Otherwise, we've got a document.written script, make a trip through
      // the event loop to hide the preload effects from the scripts on the
      // Web page.
      NS_ASSERTION(!mParserBlockingRequest,
          "There can be only one parser-blocking script at a time");
      NS_ASSERTION(mXSLTRequests.isEmpty(),
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      mParserBlockingRequest = request;
      ProcessPendingRequestsAsync();
      return true;
    }
    // The script hasn't loaded yet or there's a style sheet blocking it.
    // The script will be run when it loads or the style sheet loads.
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    NS_ASSERTION(mXSLTRequests.isEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mParserBlockingRequest = request;
    return true;
  }

  // inline script
  // Is this document sandboxed without 'allow-scripts'?
  if (mDocument->GetSandboxFlags() & SANDBOXED_SCRIPTS) {
    return false;
  }

  // Does CSP allow this inline script to run?
  if (!CSPAllowsInlineScript(aElement, mDocument)) {
    return false;
  }

  // Inline scripts ignore ther CORS mode and are always CORS_NONE
  request = new nsScriptLoadRequest(aElement, version, CORS_NONE);
  request->mJSVersion = version;
  request->mProgress = nsScriptLoadRequest::Progress_DoneLoading;
  request->mIsInline = true;
  request->mURI = mDocument->GetDocumentURI();
  request->mLineNo = aElement->GetScriptLineNumber();

  if (aElement->GetParserCreated() == FROM_PARSER_XSLT &&
      (!ReadyToExecuteScripts() || !mXSLTRequests.isEmpty())) {
    // Need to maintain order for XSLT-inserted scripts
    NS_ASSERTION(!mParserBlockingRequest,
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mXSLTRequests.AppendElement(request);
    return true;
  }
  if (aElement->GetParserCreated() == NOT_FROM_PARSER) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
        "A script-inserted script is inserted without an update batch?");
    nsContentUtils::AddScriptRunner(new nsScriptRequestProcessor(this,
                                                                 request));
    return false;
  }
  if (aElement->GetParserCreated() == FROM_PARSER_NETWORK &&
      !ReadyToExecuteScripts()) {
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    mParserBlockingRequest = request;
    NS_ASSERTION(mXSLTRequests.isEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    return true;
  }
  // We now have a document.written inline script or we have an inline script
  // from the network but there is no style sheet that is blocking scripts.
  // Don't check for style sheets blocking scripts in the document.write
  // case to avoid style sheet network activity affecting when
  // document.write returns. It's not really necessary to do this if
  // there's no document.write currently on the call stack. However,
  // this way matches IE more closely than checking if document.write
  // is on the call stack.
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
      "Not safe to run a parser-inserted script?");
  return ProcessRequest(request) == NS_ERROR_HTMLPARSER_BLOCK;
}

namespace {

class NotifyOffThreadScriptLoadCompletedRunnable : public nsRunnable
{
  nsRefPtr<nsScriptLoadRequest> mRequest;
  nsRefPtr<nsScriptLoader> mLoader;
  void *mToken;

public:
  NotifyOffThreadScriptLoadCompletedRunnable(nsScriptLoadRequest* aRequest,
                                             nsScriptLoader* aLoader)
    : mRequest(aRequest), mLoader(aLoader), mToken(nullptr)
  {}

  virtual ~NotifyOffThreadScriptLoadCompletedRunnable();

  void SetToken(void* aToken) {
    MOZ_ASSERT(aToken && !mToken);
    mToken = aToken;
  }

  NS_DECL_NSIRUNNABLE
};

} /* anonymous namespace */

nsresult
nsScriptLoader::ProcessOffThreadRequest(nsScriptLoadRequest* aRequest)
{
  MOZ_ASSERT(aRequest->mProgress == nsScriptLoadRequest::Progress_Compiling);
  aRequest->mProgress = nsScriptLoadRequest::Progress_DoneCompiling;
  nsresult rv = ProcessRequest(aRequest);
  mDocument->UnblockOnload(false);
  return rv;
}

NotifyOffThreadScriptLoadCompletedRunnable::~NotifyOffThreadScriptLoadCompletedRunnable()
{
  if (MOZ_UNLIKELY(mRequest || mLoader) && !NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    if (mainThread) {
      NS_ProxyRelease(mainThread, mRequest);
      NS_ProxyRelease(mainThread, mLoader);
    } else {
      MOZ_ASSERT(false, "We really shouldn't leak!");
      // Better to leak than crash.
      unused << mRequest.forget();
      unused << mLoader.forget();
    }
  }
}

NS_IMETHODIMP
NotifyOffThreadScriptLoadCompletedRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  // We want these to be dropped on the main thread, once we return from this
  // function.
  nsRefPtr<nsScriptLoadRequest> request = mRequest.forget();
  nsRefPtr<nsScriptLoader> loader = mLoader.forget();

  request->mOffThreadToken = mToken;
  nsresult rv = loader->ProcessOffThreadRequest(request);

  return rv;
}

static void
OffThreadScriptLoaderCallback(void *aToken, void *aCallbackData)
{
  nsRefPtr<NotifyOffThreadScriptLoadCompletedRunnable> aRunnable =
    dont_AddRef(static_cast<NotifyOffThreadScriptLoadCompletedRunnable*>(aCallbackData));
  aRunnable->SetToken(aToken);
  NS_DispatchToMainThread(aRunnable);
}

nsresult
nsScriptLoader::AttemptAsyncScriptParse(nsScriptLoadRequest* aRequest)
{
  if (!aRequest->mElement->GetScriptAsync() || aRequest->mIsInline) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  AutoJSAPI jsapi;
  if (!jsapi.InitWithLegacyErrorReporting(globalObject)) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> global(cx, globalObject->GetGlobalJSObject());
  JS::CompileOptions options(cx);
  FillCompileOptionsForRequest(jsapi, aRequest, global, &options);

  if (!JS::CanCompileOffThread(cx, options, aRequest->mScriptTextLength)) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<NotifyOffThreadScriptLoadCompletedRunnable> runnable =
    new NotifyOffThreadScriptLoadCompletedRunnable(aRequest, this);

  if (!JS::CompileOffThread(cx, options,
                            aRequest->mScriptTextBuf, aRequest->mScriptTextLength,
                            OffThreadScriptLoaderCallback,
                            static_cast<void*>(runnable))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mDocument->BlockOnload();
  aRequest->mProgress = nsScriptLoadRequest::Progress_Compiling;

  unused << runnable.forget();
  return NS_OK;
}

nsresult
nsScriptLoader::CompileOffThreadOrProcessRequest(nsScriptLoadRequest* aRequest)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(!aRequest->mOffThreadToken,
               "Candidate for off-thread compile is already parsed off-thread");
  NS_ASSERTION(!aRequest->InCompilingStage(),
               "Candidate for off-thread compile is already in compiling stage.");

  nsresult rv = AttemptAsyncScriptParse(aRequest);
  if (rv != NS_ERROR_FAILURE) {
    return rv;
  }

  return ProcessRequest(aRequest);
}

nsresult
nsScriptLoader::ProcessRequest(nsScriptLoadRequest* aRequest)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");
  NS_ASSERTION(aRequest->IsReadyToRun(),
               "Processing a request that is not ready to run.");

  NS_ENSURE_ARG(aRequest);
  nsAutoString textData;
  const char16_t* scriptBuf = nullptr;
  size_t scriptLength = 0;
  JS::SourceBufferHolder::Ownership giveScriptOwnership =
    JS::SourceBufferHolder::NoOwnership;

  nsCOMPtr<nsIDocument> doc;

  nsCOMPtr<nsINode> scriptElem = do_QueryInterface(aRequest->mElement);

  // If there's no script text, we try to get it from the element
  if (aRequest->mIsInline) {
    // XXX This is inefficient - GetText makes multiple
    // copies.
    aRequest->mElement->GetScriptText(textData);

    scriptBuf = textData.get();
    scriptLength = textData.Length();
    giveScriptOwnership = JS::SourceBufferHolder::NoOwnership;
  }
  else {
    scriptBuf = aRequest->mScriptTextBuf;
    scriptLength = aRequest->mScriptTextLength;

    giveScriptOwnership = JS::SourceBufferHolder::GiveOwnership;
    aRequest->mScriptTextBuf = nullptr;
    aRequest->mScriptTextLength = 0;

    doc = scriptElem->OwnerDoc();
  }

  JS::SourceBufferHolder srcBuf(scriptBuf, scriptLength, giveScriptOwnership);

  nsCOMPtr<nsIScriptElement> oldParserInsertedScript;
  uint32_t parserCreated = aRequest->mElement->GetParserCreated();
  if (parserCreated) {
    oldParserInsertedScript = mCurrentParserInsertedScript;
    mCurrentParserInsertedScript = aRequest->mElement;
  }

  FireScriptAvailable(NS_OK, aRequest);

  // The window may have gone away by this point, in which case there's no point
  // in trying to run the script.
  nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
  nsPIDOMWindow *pwin = master->GetInnerWindow();
  bool runScript = !!pwin;
  if (runScript) {
    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(),
                                         scriptElem,
                                         NS_LITERAL_STRING("beforescriptexecute"),
                                         true, true, &runScript);
  }

  // Inner window could have gone away after firing beforescriptexecute
  pwin = master->GetInnerWindow();
  if (!pwin) {
    runScript = false;
  }

  nsresult rv = NS_OK;
  if (runScript) {
    if (doc) {
      doc->BeginEvaluatingExternalScript();
    }
    aRequest->mElement->BeginEvaluating();
    rv = EvaluateScript(aRequest, srcBuf);
    aRequest->mElement->EndEvaluating();
    if (doc) {
      doc->EndEvaluatingExternalScript();
    }

    nsContentUtils::DispatchTrustedEvent(scriptElem->OwnerDoc(),
                                         scriptElem,
                                         NS_LITERAL_STRING("afterscriptexecute"),
                                         true, false);
  }

  FireScriptEvaluated(rv, aRequest);

  if (parserCreated) {
    mCurrentParserInsertedScript = oldParserInsertedScript;
  }

  if (aRequest->mOffThreadToken) {
    // The request was parsed off-main-thread, but the result of the off
    // thread parse was not actually needed to process the request
    // (disappearing window, some other error, ...). Finish the
    // request to avoid leaks in the JS engine.
    JS::FinishOffThreadScript(nullptr, xpc::GetJSRuntime(), aRequest->mOffThreadToken);
    aRequest->mOffThreadToken = nullptr;
  }

  return rv;
}

void
nsScriptLoader::FireScriptAvailable(nsresult aResult,
                                    nsScriptLoadRequest* aRequest)
{
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptAvailable(aResult, aRequest->mElement,
                         aRequest->mIsInline, aRequest->mURI,
                         aRequest->mLineNo);
  }

  aRequest->FireScriptAvailable(aResult);
}

void
nsScriptLoader::FireScriptEvaluated(nsresult aResult,
                                    nsScriptLoadRequest* aRequest)
{
  for (int32_t i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptEvaluated(aResult, aRequest->mElement,
                         aRequest->mIsInline);
  }

  aRequest->FireScriptEvaluated(aResult);
}

already_AddRefed<nsIScriptGlobalObject>
nsScriptLoader::GetScriptGlobalObject()
{
  nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
  nsPIDOMWindow *pwin = master->GetInnerWindow();
  if (!pwin) {
    return nullptr;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObject = do_QueryInterface(pwin);
  NS_ASSERTION(globalObject, "windows must be global objects");

  // and make sure we are setup for this type of script.
  nsresult rv = globalObject->EnsureScriptEnvironment();
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return globalObject.forget();
}

void
nsScriptLoader::FillCompileOptionsForRequest(const AutoJSAPI &jsapi,
                                             nsScriptLoadRequest *aRequest,
                                             JS::Handle<JSObject *> aScopeChain,
                                             JS::CompileOptions *aOptions)
{
  // It's very important to use aRequest->mURI, not the final URI of the channel
  // aRequest ended up getting script data from, as the script filename.
  nsContentUtils::GetWrapperSafeScriptFilename(mDocument, aRequest->mURI, aRequest->mURL);

  aOptions->setIntroductionType("scriptElement");
  aOptions->setFileAndLine(aRequest->mURL.get(), aRequest->mLineNo);
  aOptions->setVersion(JSVersion(aRequest->mJSVersion));
  aOptions->setIsRunOnce(true);
  // We only need the setNoScriptRval bit when compiling off-thread here, since
  // otherwise nsJSUtils::EvaluateString will set it up for us.
  aOptions->setNoScriptRval(true);
  if (aRequest->mHasSourceMapURL) {
    aOptions->setSourceMapURL(aRequest->mSourceMapURL.get());
  }
  if (aRequest->mOriginPrincipal) {
    nsIPrincipal* scriptPrin = nsContentUtils::ObjectPrincipal(aScopeChain);
    bool subsumes = scriptPrin->Subsumes(aRequest->mOriginPrincipal);
    aOptions->setMutedErrors(!subsumes);
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> elementVal(cx);
  MOZ_ASSERT(aRequest->mElement);
  if (NS_SUCCEEDED(nsContentUtils::WrapNative(cx, aRequest->mElement,
                                              &elementVal,
                                              /* aAllowWrapping = */ true))) {
    MOZ_ASSERT(elementVal.isObject());
    aOptions->setElement(&elementVal.toObject());
  }
}

nsresult
nsScriptLoader::EvaluateScript(nsScriptLoadRequest* aRequest,
                               JS::SourceBufferHolder& aSrcBuf)
{
  // We need a document to evaluate scripts.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> scriptContent(do_QueryInterface(aRequest->mElement));
  nsIDocument* ownerDoc = scriptContent->OwnerDoc();
  if (ownerDoc != mDocument) {
    // Willful violation of HTML5 as of 2010-12-01
    return NS_ERROR_FAILURE;
  }

  // Get the script-type to be used by this element.
  NS_ASSERTION(scriptContent, "no content - what is default script-type?");

  nsCOMPtr<nsIScriptGlobalObject> globalObject = GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_FAILURE;
  }

  // Make sure context is a strong reference since we access it after
  // we've executed a script, which may cause all other references to
  // the context to go away.
  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext();
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  JSVersion version = JSVersion(aRequest->mJSVersion);
  if (version == JSVERSION_UNKNOWN) {
    return NS_OK;
  }

  // New script entry point required, due to the "Create a script" sub-step of
  // http://www.whatwg.org/specs/web-apps/current-work/#execute-the-script-block
  nsAutoMicroTask mt;
  AutoEntryScript entryScript(globalObject, "<script> element", true,
                              context->GetNativeContext());
  entryScript.TakeOwnershipOfErrorReporting();
  JS::Rooted<JSObject*> global(entryScript.cx(),
                               globalObject->GetGlobalJSObject());

  bool oldProcessingScriptTag = context->GetProcessingScriptTag();
  context->SetProcessingScriptTag(true);
  nsresult rv;
  {
    // Update our current script.
    AutoCurrentScriptUpdater scriptUpdater(this, aRequest->mElement);
    Maybe<AutoCurrentScriptUpdater> masterScriptUpdater;
    nsCOMPtr<nsIDocument> master = mDocument->MasterDocument();
    if (master != mDocument) {
      // If this script belongs to an import document, it will be
      // executed in the context of the master document. During the
      // execution currentScript of the master should refer to this
      // script. So let's update the mCurrentScript of the ScriptLoader
      // of the master document too.
      masterScriptUpdater.emplace(master->ScriptLoader(),
                                  aRequest->mElement);
    }

    JS::CompileOptions options(entryScript.cx());
    FillCompileOptionsForRequest(entryScript, aRequest, global, &options);
    rv = nsJSUtils::EvaluateString(entryScript.cx(), aSrcBuf, global, options,
                                   aRequest->OffThreadTokenPtr());
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);
  return rv;
}

void
nsScriptLoader::ProcessPendingRequestsAsync()
{
  if (mParserBlockingRequest || !mPendingChildLoaders.IsEmpty()) {
    nsCOMPtr<nsIRunnable> ev = NS_NewRunnableMethod(this,
      &nsScriptLoader::ProcessPendingRequests);

    NS_DispatchToCurrentThread(ev);
  }
}

void
nsScriptLoader::ProcessPendingRequests()
{
  nsRefPtr<nsScriptLoadRequest> request;

  if (mParserBlockingRequest &&
      mParserBlockingRequest->IsReadyToRun() &&
      ReadyToExecuteScripts()) {
    request.swap(mParserBlockingRequest);
    UnblockParser(request);
    ProcessRequest(request);
    ContinueParserAsync(request);
  }

  while (ReadyToExecuteScripts() && 
         !mXSLTRequests.isEmpty() &&
         mXSLTRequests.getFirst()->IsReadyToRun()) {
    request = mXSLTRequests.StealFirst();
    ProcessRequest(request);
  }

  while (mEnabled && !mLoadedAsyncRequests.isEmpty()) {
    request = mLoadedAsyncRequests.StealFirst();
    CompileOffThreadOrProcessRequest(request);
  }

  while (mEnabled && !mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
         mNonAsyncExternalScriptInsertedRequests.getFirst()->IsReadyToRun()) {
    // Violate the HTML5 spec and execute these in the insertion order in
    // order to make LABjs and the "order" plug-in for RequireJS work with
    // their Gecko-sniffed code path. See
    // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
    request = mNonAsyncExternalScriptInsertedRequests.StealFirst();
    ProcessRequest(request);
  }

  if (mDocumentParsingDone && mXSLTRequests.isEmpty()) {
    while (!mDeferRequests.isEmpty() && mDeferRequests.getFirst()->IsReadyToRun()) {
      request = mDeferRequests.StealFirst();
      ProcessRequest(request);
    }
  }

  while (!mPendingChildLoaders.IsEmpty() && ReadyToExecuteScripts()) {
    nsRefPtr<nsScriptLoader> child = mPendingChildLoaders[0];
    mPendingChildLoaders.RemoveElementAt(0);
    child->RemoveExecuteBlocker();
  }

  if (mDocumentParsingDone && mDocument && !mParserBlockingRequest &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty() &&
      MaybeRemovedDeferRequests()) {
    return ProcessPendingRequests();
  }

  if (mDocumentParsingDone && mDocument &&
      !mParserBlockingRequest && mLoadingAsyncRequests.isEmpty() &&
      mLoadedAsyncRequests.isEmpty() &&
      mNonAsyncExternalScriptInsertedRequests.isEmpty() &&
      mXSLTRequests.isEmpty() && mDeferRequests.isEmpty()) {
    // No more pending scripts; time to unblock onload.
    // OK to unblock onload synchronously here, since callers must be
    // prepared for the world changing anyway.
    mDocumentParsingDone = false;
    mDocument->UnblockOnload(true);
  }
}

bool
nsScriptLoader::ReadyToExecuteScripts()
{
  // Make sure the SelfReadyToExecuteScripts check is first, so that
  // we don't block twice on an ancestor.
  if (!SelfReadyToExecuteScripts()) {
    return false;
  }

  for (nsIDocument* doc = mDocument; doc; doc = doc->GetParentDocument()) {
    nsScriptLoader* ancestor = doc->ScriptLoader();
    if (!ancestor->SelfReadyToExecuteScripts() &&
        ancestor->AddPendingChildLoader(this)) {
      AddExecuteBlocker();
      return false;
    }
  }

  if (mDocument && !mDocument->IsMasterDocument()) {
    nsRefPtr<ImportManager> im = mDocument->ImportManager();
    nsRefPtr<ImportLoader> loader = im->Find(mDocument);
    MOZ_ASSERT(loader, "How can we have an import document without a loader?");

    // The referring link that counts in the execution order calculation
    // (in spec: flagged as branch)
    nsCOMPtr<nsINode> referrer = loader->GetMainReferrer();
    MOZ_ASSERT(referrer, "There has to be a main referring link for each imports");

    // Import documents are blocked by their import predecessors. We need to
    // wait with script execution until all the predecessors are done.
    // Technically it means we have to wait for the last one to finish,
    // which is the neares one to us in the order.
    nsRefPtr<ImportLoader> lastPred = im->GetNearestPredecessor(referrer);
    if (!lastPred) {
      // If there is no predecessor we can run.
      return true;
    }

    nsCOMPtr<nsIDocument> doc = lastPred->GetDocument();
    if (lastPred->IsBlocking() || !doc || (doc && !doc->ScriptLoader()->SelfReadyToExecuteScripts())) {
      // Document has not been created yet or it was created but not ready.
      // Either case we are blocked by it. The ImportLoader will take care
      // of blocking us, and adding the pending child loader to the blocking
      // ScriptLoader when it's possible (at this point the blocking loader
      // might not have created the document/ScriptLoader)
      lastPred->AddBlockedScriptLoader(this);
      // As more imports are parsed, this can change, let's cache what we
      // blocked, so it can be later updated if needed (see: ImportLoader::Updater).
      loader->SetBlockingPredecessor(lastPred);
      return false;
    }
  }

  return true;
}

// This function was copied from nsParser.cpp. It was simplified a bit.
static bool
DetectByteOrderMark(const unsigned char* aBytes, int32_t aLen, nsCString& oCharset)
{
  if (aLen < 2)
    return false;

  switch(aBytes[0]) {
  case 0xEF:
    if (aLen >= 3 && 0xBB == aBytes[1] && 0xBF == aBytes[2]) {
      // EF BB BF
      // Win2K UTF-8 BOM
      oCharset.AssignLiteral("UTF-8");
    }
    break;
  case 0xFE:
    if (0xFF == aBytes[1]) {
      // FE FF
      // UTF-16, big-endian
      oCharset.AssignLiteral("UTF-16BE");
    }
    break;
  case 0xFF:
    if (0xFE == aBytes[1]) {
      // FF FE
      // UTF-16, little-endian
      oCharset.AssignLiteral("UTF-16LE");
    }
    break;
  }
  return !oCharset.IsEmpty();
}

/* static */ nsresult
nsScriptLoader::ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
                               uint32_t aLength, const nsAString& aHintCharset,
                               nsIDocument* aDocument,
                               char16_t*& aBufOut, size_t& aLengthOut)
{
  if (!aLength) {
    aBufOut = nullptr;
    aLengthOut = 0;
    return NS_OK;
  }

  // The encoding info precedence is as follows from high to low:
  // The BOM
  // HTTP Content-Type (if name recognized)
  // charset attribute (if name recognized)
  // The encoding of the document

  nsAutoCString charset;

  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;

  if (DetectByteOrderMark(aData, aLength, charset)) {
    // charset is now "UTF-8" or "UTF-16". The UTF-16 decoder will re-sniff
    // the BOM for endianness. Both the UTF-16 and the UTF-8 decoder will
    // take care of swallowing the BOM.
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder &&
      aChannel &&
      NS_SUCCEEDED(aChannel->GetContentCharset(charset)) &&
      EncodingUtils::FindEncodingForLabel(charset, charset)) {
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder &&
      EncodingUtils::FindEncodingForLabel(aHintCharset, charset)) {
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder && aDocument) {
    charset = aDocument->GetDocumentCharacterSet();
    unicodeDecoder = EncodingUtils::DecoderForEncoding(charset);
  }

  if (!unicodeDecoder) {
    // Curiously, there are various callers that don't pass aDocument. The
    // fallback in the old code was ISO-8859-1, which behaved like
    // windows-1252. Saying windows-1252 for clarity and for compliance
    // with the Encoding Standard.
    unicodeDecoder = EncodingUtils::DecoderForEncoding("windows-1252");
  }

  int32_t unicodeLength = 0;

  nsresult rv =
    unicodeDecoder->GetMaxLength(reinterpret_cast<const char*>(aData),
                                 aLength, &unicodeLength);
  NS_ENSURE_SUCCESS(rv, rv);

  aBufOut = static_cast<char16_t*>(js_malloc(unicodeLength * sizeof(char16_t)));
  if (!aBufOut) {
    aLengthOut = 0;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aLengthOut = unicodeLength;

  rv = unicodeDecoder->Convert(reinterpret_cast<const char*>(aData),
                               (int32_t *) &aLength, aBufOut,
                               &unicodeLength);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  aLengthOut = unicodeLength;
  if (NS_FAILED(rv)) {
    js_free(aBufOut);
    aBufOut = nullptr;
    aLengthOut = 0;
  }
  return rv;
}

NS_IMETHODIMP
nsScriptLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                 nsISupports* aContext,
                                 nsresult aStatus,
                                 uint32_t aStringLen,
                                 const uint8_t* aString)
{
  nsScriptLoadRequest* request = static_cast<nsScriptLoadRequest*>(aContext);
  NS_ASSERTION(request, "null request in stream complete handler");
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsresult rv = PrepareLoadedRequest(request, aLoader, aStatus, aStringLen,
                                     aString);
  if (NS_FAILED(rv)) {
    /*
     * Handle script not loading error because source was a tracking URL.
     * We make a note of this script node by including it in a dedicated
     * array of blocked tracking nodes under its parent document.
     */
    if (rv == NS_ERROR_TRACKING_URI) {
      nsCOMPtr<nsIContent> cont = do_QueryInterface(request->mElement);
      mDocument->AddBlockedTrackingNode(cont);
    }

    if (request->mIsDefer) {
      if (request->isInList()) {
        nsRefPtr<nsScriptLoadRequest> req = mDeferRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (request->mIsAsync) {
      if (request->isInList()) {
        nsRefPtr<nsScriptLoadRequest> req = mLoadingAsyncRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (request->mIsNonAsyncScriptInserted) {
      if (request->isInList()) {
        nsRefPtr<nsScriptLoadRequest> req =
          mNonAsyncExternalScriptInsertedRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (request->mIsXSLT) {
      if (request->isInList()) {
        nsRefPtr<nsScriptLoadRequest> req = mXSLTRequests.Steal(request);
        FireScriptAvailable(rv, req);
      }
    } else if (mParserBlockingRequest == request) {
      mParserBlockingRequest = nullptr;
      UnblockParser(request);
      FireScriptAvailable(rv, request);
      ContinueParserAsync(request);
    } else {
      mPreloads.RemoveElement(request, PreloadRequestComparator());
    }
    rv = NS_OK;
  } else {
    free(const_cast<uint8_t *>(aString));
    rv = NS_SUCCESS_ADOPTED_DATA;
  }

  // Process our request and/or any pending ones
  ProcessPendingRequests();

  return rv;
}

void
nsScriptLoader::UnblockParser(nsScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->mElement->UnblockParser();
}

void
nsScriptLoader::ContinueParserAsync(nsScriptLoadRequest* aParserBlockingRequest)
{
  aParserBlockingRequest->mElement->ContinueParserAsync();
}

nsresult
nsScriptLoader::PrepareLoadedRequest(nsScriptLoadRequest* aRequest,
                                     nsIStreamLoader* aLoader,
                                     nsresult aStatus,
                                     uint32_t aStringLen,
                                     const uint8_t* aString)
{
  if (NS_FAILED(aStatus)) {
    return aStatus;
  }

  if (aRequest->IsCanceled()) {
    return NS_BINDING_ABORTED;
  }

  // If we don't have a document, then we need to abort further
  // evaluation.
  if (!mDocument) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // If the load returned an error page, then we need to abort
  nsCOMPtr<nsIRequest> req;
  nsresult rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(req);
  if (httpChannel) {
    bool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(rv) && !requestSucceeded) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsAutoCString sourceMapURL;
    rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("X-SourceMap"), sourceMapURL);
    if (NS_SUCCEEDED(rv)) {
      aRequest->mHasSourceMapURL = true;
      aRequest->mSourceMapURL = NS_ConvertUTF8toUTF16(sourceMapURL);
    }
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);
  // If this load was subject to a CORS check; don't flag it with a
  // separate origin principal, so that it will treat our document's
  // principal as the origin principal
  if (aRequest->mCORSMode == CORS_NONE) {
    rv = nsContentUtils::GetSecurityManager()->
      GetChannelResultPrincipal(channel, getter_AddRefs(aRequest->mOriginPrincipal));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aStringLen) {
    // Check the charset attribute to determine script charset.
    nsAutoString hintCharset;
    if (!aRequest->IsPreload()) {
      aRequest->mElement->GetScriptCharset(hintCharset);
    } else {
      nsTArray<PreloadInfo>::index_type i =
        mPreloads.IndexOf(aRequest, 0, PreloadRequestComparator());
      NS_ASSERTION(i != mPreloads.NoIndex, "Incorrect preload bookkeeping");
      hintCharset = mPreloads[i].mCharset;
    }
    rv = ConvertToUTF16(channel, aString, aStringLen, hintCharset, mDocument,
                        aRequest->mScriptTextBuf, aRequest->mScriptTextLength);

    NS_ENSURE_SUCCESS(rv, rv);
  }

  // This assertion could fire errorously if we ran out of memory when
  // inserting the request in the array. However it's an unlikely case
  // so if you see this assertion it is likely something else that is
  // wrong, especially if you see it more than once.
  NS_ASSERTION(mDeferRequests.Contains(aRequest) ||
               mLoadingAsyncRequests.Contains(aRequest) ||
               mNonAsyncExternalScriptInsertedRequests.Contains(aRequest) ||
               mXSLTRequests.Contains(aRequest)  ||
               mPreloads.Contains(aRequest, PreloadRequestComparator()) ||
               mParserBlockingRequest,
               "aRequest should be pending!");

  // Mark this as loaded
  aRequest->mProgress = nsScriptLoadRequest::Progress_DoneLoading;

  // And if it's async, move it to the loaded list.  aRequest->mIsAsync really
  // _should_ be in a list, but the consequences if it's not are bad enough we
  // want to avoid trying to move it if it's not.
  if (aRequest->mIsAsync) {
    MOZ_ASSERT(aRequest->isInList());
    if (aRequest->isInList()) {
      nsRefPtr<nsScriptLoadRequest> req = mLoadingAsyncRequests.Steal(aRequest);
      mLoadedAsyncRequests.AppendElement(req);
    }
  }

  return NS_OK;
}

void
nsScriptLoader::ParsingComplete(bool aTerminated)
{
  if (mDeferEnabled) {
    // Have to check because we apparently get ParsingComplete
    // without BeginDeferringScripts in some cases
    mDocumentParsingDone = true;
  }
  mDeferEnabled = false;
  if (aTerminated) {
    mDeferRequests.Clear();
    mLoadingAsyncRequests.Clear();
    mLoadedAsyncRequests.Clear();
    mNonAsyncExternalScriptInsertedRequests.Clear();
    mXSLTRequests.Clear();
    if (mParserBlockingRequest) {
      mParserBlockingRequest->Cancel();
      mParserBlockingRequest = nullptr;
    }
  }

  // Have to call this even if aTerminated so we'll correctly unblock
  // onload and all.
  ProcessPendingRequests();
}

void
nsScriptLoader::PreloadURI(nsIURI *aURI, const nsAString &aCharset,
                           const nsAString &aType,
                           const nsAString &aCrossOrigin,
                           bool aScriptFromHead,
                           const mozilla::net::ReferrerPolicy aReferrerPolicy)
{
  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return;
  }

  nsRefPtr<nsScriptLoadRequest> request =
    new nsScriptLoadRequest(nullptr, 0,
                            Element::StringToCORSMode(aCrossOrigin));
  request->mURI = aURI;
  request->mIsInline = false;
  request->mProgress = nsScriptLoadRequest::Progress_Loading;
  request->mReferrerPolicy = aReferrerPolicy;

  nsresult rv = StartLoad(request, aType, aScriptFromHead);
  if (NS_FAILED(rv)) {
    return;
  }

  PreloadInfo *pi = mPreloads.AppendElement();
  pi->mRequest = request;
  pi->mCharset = aCharset;
}

void
nsScriptLoader::AddDeferRequest(nsScriptLoadRequest* aRequest)
{
  aRequest->mIsDefer = true;
  mDeferRequests.AppendElement(aRequest);
  if (mDeferEnabled && aRequest == mDeferRequests.getFirst() &&
      mDocument && !mBlockingDOMContentLoaded) {
    MOZ_ASSERT(mDocument->GetReadyStateEnum() == nsIDocument::READYSTATE_LOADING);
    mBlockingDOMContentLoaded = true;
    mDocument->BlockDOMContentLoaded();
  }
}

bool
nsScriptLoader::MaybeRemovedDeferRequests()
{
  if (mDeferRequests.isEmpty() && mDocument &&
      mBlockingDOMContentLoaded) {
    mBlockingDOMContentLoaded = false;
    mDocument->UnblockDOMContentLoaded();
    return true;
  }
  return false;
}

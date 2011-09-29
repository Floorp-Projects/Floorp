/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao <vidur@netscape.com> (original author)
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

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#include "jscntxt.h"
#include "nsScriptLoader.h"
#include "nsIDOMCharacterData.h"
#include "nsParserUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "nsGkAtoms.h"
#include "nsNetUtil.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIScriptRuntime.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsIHttpChannel.h"
#include "nsIScriptElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDocShell.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "nsIXPConnect.h"
#include "nsContentErrors.h"
#include "nsIParser.h"
#include "nsThreadUtils.h"
#include "nsDocShellCID.h"
#include "nsIContentSecurityPolicy.h"
#include "prlog.h"
#include "nsIChannelPolicy.h"
#include "nsChannelPolicy.h"
#include "nsCRT.h"
#include "nsContentCreatorFunctions.h"

#include "mozilla/FunctionTimer.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gCspPRLog;
#endif

using namespace mozilla::dom;

//////////////////////////////////////////////////////////////
// Per-request data structure
//////////////////////////////////////////////////////////////

class nsScriptLoadRequest : public nsISupports {
public:
  nsScriptLoadRequest(nsIScriptElement* aElement,
                      PRUint32 aVersion)
    : mElement(aElement),
      mLoading(PR_TRUE),
      mIsInline(PR_TRUE),
      mJSVersion(aVersion), mLineNo(1)
  {
  }

  NS_DECL_ISUPPORTS

  void FireScriptAvailable(nsresult aResult)
  {
    mElement->ScriptAvailable(aResult, mElement, mIsInline, mURI, mLineNo);
  }
  void FireScriptEvaluated(nsresult aResult)
  {
    mElement->ScriptEvaluated(aResult, mElement, mIsInline);
  }

  bool IsPreload()
  {
    return mElement == nsnull;
  }

  nsCOMPtr<nsIScriptElement> mElement;
  bool mLoading;             // Are we still waiting for a load to complete?
  bool mIsInline;            // Is the script inline or loaded?
  nsString mScriptText;              // Holds script for loaded scripts
  PRUint32 mJSVersion;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mFinalURI;
  PRInt32 mLineNo;
};

// The nsScriptLoadRequest is passed as the context to necko, and thus
// it needs to be threadsafe. Necko won't do anything with this
// context, but it will AddRef and Release it on other threads.
NS_IMPL_THREADSAFE_ISUPPORTS0(nsScriptLoadRequest)

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

nsScriptLoader::nsScriptLoader(nsIDocument *aDocument)
  : mDocument(aDocument),
    mBlockerCount(0),
    mEnabled(PR_TRUE),
    mDeferEnabled(PR_FALSE),
    mDocumentParsingDone(PR_FALSE)
{
  // enable logging for CSP
#ifdef PR_LOGGING
  if (!gCspPRLog)
    gCspPRLog = PR_NewLogModule("CSP");
#endif
}

nsScriptLoader::~nsScriptLoader()
{
  mObservers.Clear();

  if (mParserBlockingRequest) {
    mParserBlockingRequest->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (PRUint32 i = 0; i < mXSLTRequests.Length(); i++) {
    mXSLTRequests[i]->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (PRUint32 i = 0; i < mDeferRequests.Length(); i++) {
    mDeferRequests[i]->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (PRUint32 i = 0; i < mAsyncRequests.Length(); i++) {
    mAsyncRequests[i]->FireScriptAvailable(NS_ERROR_ABORT);
  }

  for (PRUint32 i = 0; i < mNonAsyncExternalScriptInsertedRequests.Length(); i++) {
    mNonAsyncExternalScriptInsertedRequests[i]->FireScriptAvailable(NS_ERROR_ABORT);
  }

  // Unblock the kids, in case any of them moved to a different document
  // subtree in the meantime and therefore aren't actually going away.
  for (PRUint32 j = 0; j < mPendingChildLoaders.Length(); ++j) {
    mPendingChildLoaders[j]->RemoveExecuteBlocker();
  }  
}

NS_IMPL_ISUPPORTS1(nsScriptLoader, nsIStreamLoaderObserver)

// Helper method for checking if the script element is an event-handler
// This means that it has both a for-attribute and a event-attribute.
// Also, if the for-attribute has a value that matches "\s*window\s*",
// and the event-attribute matches "\s*onload([ \(].*)?" then it isn't an
// eventhandler. (both matches are case insensitive).
// This is how IE seems to filter out a window's onload handler from a
// <script for=... event=...> element.

static bool
IsScriptEventHandler(nsIScriptElement *aScriptElement)
{
  nsCOMPtr<nsIContent> contElement = do_QueryInterface(aScriptElement);
  NS_ASSERTION(contElement, "nsIScriptElement isn't nsIContent");

  nsAutoString forAttr, eventAttr;
  if (!contElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_for, forAttr) ||
      !contElement->GetAttr(kNameSpaceID_None, nsGkAtoms::event, eventAttr)) {
    return PR_FALSE;
  }

  const nsAString& for_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(forAttr);
  if (!for_str.LowerCaseEqualsLiteral("window")) {
    return PR_TRUE;
  }

  // We found for="window", now check for event="onload".
  const nsAString& event_str =
    nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(eventAttr, PR_FALSE);
  if (!StringBeginsWith(event_str, NS_LITERAL_STRING("onload"),
                        nsCaseInsensitiveStringComparator())) {
    // It ain't "onload.*".

    return PR_TRUE;
  }

  nsAutoString::const_iterator start, end;
  event_str.BeginReading(start);
  event_str.EndReading(end);

  start.advance(6); // advance past "onload"

  if (start != end && *start != '(' && *start != ' ') {
    // We got onload followed by something other than space or
    // '('. Not good enough.

    return PR_TRUE;
  }

  return PR_FALSE;
}

nsresult
nsScriptLoader::CheckContentPolicy(nsIDocument* aDocument,
                                   nsISupports *aContext,
                                   nsIURI *aURI,
                                   const nsAString &aType)
{
  PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_SCRIPT,
                                          aURI,
                                          aDocument->NodePrincipal(),
                                          aContext,
                                          NS_LossyConvertUTF16toASCII(aType),
                                          nsnull,    //extra
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
nsScriptLoader::StartLoad(nsScriptLoadRequest *aRequest, const nsAString &aType)
{
  nsISupports *context = aRequest->mElement.get()
                         ? static_cast<nsISupports *>(aRequest->mElement.get())
                         : static_cast<nsISupports *>(mDocument);
  nsresult rv = ShouldLoadScript(mDocument, context, aRequest->mURI, aType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
  nsCOMPtr<nsIStreamLoader> loader;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mDocument->GetScriptGlobalObject()));
  if (!window) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIDocShell *docshell = window->GetDocShell();

  nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

  // check for a Content Security Policy to pass down to the channel
  // that will be created to load the script
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = mDocument->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);
  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_SCRIPT);
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     aRequest->mURI, nsnull, loadGroup, prompter,
                     nsIRequest::LOAD_NORMAL | nsIChannel::LOAD_CLASSIFY_URI,
                     channelPolicy);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    // HTTP content negotation has little value in this context.
    httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                  NS_LITERAL_CSTRING("*/*"),
                                  PR_FALSE);
    httpChannel->SetReferrer(mDocument->GetDocumentURI());
  }

  rv = NS_NewStreamLoader(getter_AddRefs(loader), this);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->AsyncOpen(loader, aRequest);
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

nsresult
nsScriptLoader::ProcessScriptElement(nsIScriptElement *aElement)
{
  // We need a document to evaluate scripts.
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(!aElement->IsMalformed(), "Executing malformed script");

  // Check that the script is not an eventhandler
  if (IsScriptEventHandler(aElement)) {
    return NS_CONTENT_SCRIPT_IS_EVENTHANDLER;
  }

  // Script evaluation can also be disabled in the current script
  // context even though it's enabled in the document.
  // XXX - still hard-coded for JS here, even though another language
  // may be specified.  Should this check be made *after* we examine
  // the attributes to locate the script-type?
  // For now though, if JS is disabled we assume every language is
  // disabled.
  // XXX is this different from the mDocument->IsScriptEnabled() call?
  nsIScriptGlobalObject *globalObject = mDocument->GetScriptGlobalObject();
  if (!globalObject) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  nsIScriptContext *context = globalObject->GetScriptContext(
                                        nsIProgrammingLanguage::JAVASCRIPT);

  // If scripts aren't enabled in the current context, there's no
  // point in going on.
  if (!context || !context->GetScriptsEnabled()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Default script language is whatever the root element specifies
  // (which may come from a header or http-meta tag), or if there
  // is no root element, from the script global object.
  Element* rootElement = mDocument->GetRootElement();
  PRUint32 typeID = rootElement ? rootElement->GetScriptTypeID() :
                                  context->GetScriptTypeID();
  PRUint32 version = 0;
  nsAutoString language, type, src;
  nsresult rv = NS_OK;

  // Check the type attribute to determine language and version.
  // If type exists, it trumps the deprecated 'language='
  aElement->GetScriptType(type);
  if (!type.IsEmpty()) {
    nsContentTypeParser parser(type);

    nsAutoString mimeType;
    rv = parser.GetType(mimeType);
    NS_ENSURE_SUCCESS(rv, rv);

    // Javascript keeps the fast path, optimized for most-likely type
    // Table ordered from most to least likely JS MIME types.
    // See bug 62485, feel free to add <script type="..."> survey data to it,
    // or to a new bug once 62485 is closed.
    static const char *jsTypes[] = {
      "text/javascript",
      "text/ecmascript",
      "application/javascript",
      "application/ecmascript",
      "application/x-javascript",
      nsnull
    };

    bool isJavaScript = false;
    for (PRInt32 i = 0; jsTypes[i]; i++) {
      if (mimeType.LowerCaseEqualsASCII(jsTypes[i])) {
        isJavaScript = PR_TRUE;
        break;
      }
    }
    if (isJavaScript)
      typeID = nsIProgrammingLanguage::JAVASCRIPT;
    else {
      // Use the object factory to locate a matching language.
      nsCOMPtr<nsIScriptRuntime> runtime;
      rv = NS_GetScriptRuntime(mimeType, getter_AddRefs(runtime));
      if (NS_FAILED(rv) || runtime == nsnull) {
        // Failed to get the explicitly specified language
        NS_WARNING("Failed to find a scripting language");
        typeID = nsIProgrammingLanguage::UNKNOWN;
      } else
        typeID = runtime->GetScriptTypeID();
    }
    if (typeID != nsIProgrammingLanguage::UNKNOWN) {
      // Get the version string, and ensure the language supports it.
      nsAutoString versionName;
      rv = parser.GetParameter("version", versionName);
      if (NS_FAILED(rv)) {
        // no version attribute - version remains 0.
        if (rv != NS_ERROR_INVALID_ARG)
          return rv;
      } else {
        nsCOMPtr<nsIScriptRuntime> runtime;
        rv = NS_GetScriptRuntimeByID(typeID, getter_AddRefs(runtime));
        if (NS_FAILED(rv)) {
          NS_ERROR("Failed to locate the language with this ID");
          return rv;
        }
        rv = runtime->ParseVersion(versionName, &version);
        if (NS_FAILED(rv)) {
          NS_WARNING("This script language version is not supported - ignored");
          typeID = nsIProgrammingLanguage::UNKNOWN;
        }
      }
    }

    // Some js specifics yet to be abstracted.
    if (typeID == nsIProgrammingLanguage::JAVASCRIPT) {
      nsAutoString value;
      rv = parser.GetParameter("e4x", value);
      if (NS_FAILED(rv)) {
        if (rv != NS_ERROR_INVALID_ARG)
          return rv;
      } else {
        if (value.Length() == 1 && value[0] == '1')
          // This means that we need to set JSOPTION_XML in the JS options.
          // We re-use our knowledge of the implementation to reuse
          // JSVERSION_HAS_XML as a safe version flag.
          // If version has JSVERSION_UNKNOWN (-1), then this is still OK.
          version |= js::VersionFlags::HAS_XML;
      }
    }
  } else {
    // no 'type=' element
    // "language" is a deprecated attribute of HTML, so we check it only for
    // HTML script elements.
    nsCOMPtr<nsIDOMHTMLScriptElement> htmlScriptElement =
      do_QueryInterface(aElement);
    if (htmlScriptElement) {
      htmlScriptElement->GetAttribute(NS_LITERAL_STRING("language"), language);
      if (!language.IsEmpty()) {
        if (nsParserUtils::IsJavaScriptLanguage(language, &version))
          typeID = nsIProgrammingLanguage::JAVASCRIPT;
        else
          typeID = nsIProgrammingLanguage::UNKNOWN;
        // IE, Opera, etc. do not respect language version, so neither should
        // we at this late date in the browser wars saga.  Note that this change
        // affects HTML but not XUL or SVG (but note also that XUL has its own
        // code to check nsParserUtils::IsJavaScriptLanguage -- that's probably
        // a separate bug, one we may not be able to fix short of XUL2).  See
        // bug 255895 (https://bugzilla.mozilla.org/show_bug.cgi?id=255895).
        NS_ASSERTION(JSVERSION_DEFAULT == 0,
                     "We rely on all languages having 0 as a version default");
        version = 0;
      }
    }
  }

  // If we don't know the language, we don't know how to evaluate
  if (typeID == nsIProgrammingLanguage::UNKNOWN) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  // If not from a chrome document (which is always trusted), we need some way 
  // of checking the language is "safe".  Currently the only other language 
  // impl is Python, and that is *not* safe in untrusted code - so fixing 
  // this isn't a priority.!
  // See also similar code in nsXULContentSink.cpp
  if (typeID != nsIProgrammingLanguage::JAVASCRIPT &&
      !nsContentUtils::IsChromeDoc(mDocument)) {
    NS_WARNING("Untrusted language called from non-chrome - ignored");
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIContent> eltContent(do_QueryInterface(aElement));
  eltContent->SetScriptTypeID(typeID);

  // Step 9. in the HTML5 spec

  nsRefPtr<nsScriptLoadRequest> request;
  if (aElement->GetScriptExternal()) {
    // external script
    nsCOMPtr<nsIURI> scriptURI = aElement->GetScriptURI();
    if (!scriptURI) {
      return NS_ERROR_NOT_AVAILABLE;
    }
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
      if (elementCharset.Equals(preloadCharset)) {
        rv = CheckContentPolicy(mDocument, aElement, request->mURI, type);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // Drop the preload
        request = nsnull;
      }
    }

    if (!request) {
      // no usable preload
      request = new nsScriptLoadRequest(aElement, version);
      NS_ENSURE_TRUE(request, NS_ERROR_OUT_OF_MEMORY);
      request->mURI = scriptURI;
      request->mIsInline = PR_FALSE;
      request->mLoading = PR_TRUE;
      rv = StartLoad(request, type);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    request->mJSVersion = version;

    if (aElement->GetScriptAsync()) {
      mAsyncRequests.AppendElement(request);
      if (!request->mLoading) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return NS_OK;
    }
    if (!aElement->GetParserCreated()) {
      // Violate the HTML5 spec in order to make LABjs and the "order" plug-in
      // for RequireJS work with their Gecko-sniffed code path. See
      // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
      mNonAsyncExternalScriptInsertedRequests.AppendElement(request);
      if (!request->mLoading) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return NS_OK;
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
      mDeferRequests.AppendElement(request);
      return NS_OK;
    }

    if (aElement->GetParserCreated() == FROM_PARSER_XSLT) {
      // Need to maintain order for XSLT-inserted scripts
      NS_ASSERTION(!mParserBlockingRequest,
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      mXSLTRequests.AppendElement(request);
      if (!request->mLoading) {
        // The script is available already. Run it ASAP when the event
        // loop gets a chance to spin.
        ProcessPendingRequestsAsync();
      }
      return NS_ERROR_HTMLPARSER_BLOCK;
    }
    if (!request->mLoading && ReadyToExecuteScripts()) {
      // The request has already been loaded and there are no pending style
      // sheets. If the script comes from the network stream, cheat for
      // performance reasons and avoid a trip through the event loop.
      if (aElement->GetParserCreated() == FROM_PARSER_NETWORK) {
        return ProcessRequest(request);
      }
      // Otherwise, we've got a document.written script, make a trip through
      // the event loop to hide the preload effects from the scripts on the
      // Web page.
      NS_ASSERTION(!mParserBlockingRequest,
          "There can be only one parser-blocking script at a time");
      NS_ASSERTION(mXSLTRequests.IsEmpty(),
          "Parser-blocking scripts and XSLT scripts in the same doc!");
      mParserBlockingRequest = request;
      ProcessPendingRequestsAsync();
      return NS_ERROR_HTMLPARSER_BLOCK;
    }
    // The script hasn't loaded yet or there's a style sheet blocking it.
    // The script will be run when it loads or the style sheet loads.
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    NS_ASSERTION(mXSLTRequests.IsEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mParserBlockingRequest = request;
    return NS_ERROR_HTMLPARSER_BLOCK;
  }

  // inline script
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = mDocument->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS(rv, rv);

  if (csp) {
    PR_LOG(gCspPRLog, PR_LOG_DEBUG, ("New ScriptLoader i ****with CSP****"));
    bool inlineOK;
    rv = csp->GetAllowsInlineScript(&inlineOK);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!inlineOK) {
      PR_LOG(gCspPRLog, PR_LOG_DEBUG, ("CSP blocked inline scripts (2)"));
      // gather information to log with violation report
      nsIURI* uri = mDocument->GetDocumentURI();
      nsCAutoString asciiSpec;
      uri->GetAsciiSpec(asciiSpec);
      nsAutoString scriptText;
      aElement->GetScriptText(scriptText);

      // cap the length of the script sample at 40 chars
      if (scriptText.Length() > 40) {
        scriptText.Truncate(40);
        scriptText.Append(NS_LITERAL_STRING("..."));
      }

      csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_INLINE_SCRIPT,
                               NS_ConvertUTF8toUTF16(asciiSpec),
                               scriptText,
                               aElement->GetScriptLineNumber());
      return NS_ERROR_FAILURE;
    }
  }

  request = new nsScriptLoadRequest(aElement, version);
  NS_ENSURE_TRUE(request, NS_ERROR_OUT_OF_MEMORY);
  request->mJSVersion = version;
  request->mLoading = PR_FALSE;
  request->mIsInline = PR_TRUE;
  request->mURI = mDocument->GetDocumentURI();
  request->mLineNo = aElement->GetScriptLineNumber();

  if (aElement->GetParserCreated() == FROM_PARSER_XSLT &&
      (!ReadyToExecuteScripts() || !mXSLTRequests.IsEmpty())) {
    // Need to maintain order for XSLT-inserted scripts
    NS_ASSERTION(!mParserBlockingRequest,
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    mXSLTRequests.AppendElement(request);
    return NS_ERROR_HTMLPARSER_BLOCK;
  }
  if (aElement->GetParserCreated() == NOT_FROM_PARSER) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
        "A script-inserted script is inserted without an update batch?");
    nsContentUtils::AddScriptRunner(new nsScriptRequestProcessor(this,
                                                                 request));
    return NS_OK;
  }
  if (aElement->GetParserCreated() == FROM_PARSER_NETWORK &&
      !ReadyToExecuteScripts()) {
    NS_ASSERTION(!mParserBlockingRequest,
        "There can be only one parser-blocking script at a time");
    mParserBlockingRequest = request;
    NS_ASSERTION(mXSLTRequests.IsEmpty(),
        "Parser-blocking scripts and XSLT scripts in the same doc!");
    return NS_ERROR_HTMLPARSER_BLOCK;
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
  return ProcessRequest(request);
}

nsresult
nsScriptLoader::ProcessRequest(nsScriptLoadRequest* aRequest)
{
  NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
               "Processing requests when running scripts is unsafe.");

  NS_ENSURE_ARG(aRequest);
  nsAFlatString* script;
  nsAutoString textData;

  NS_TIME_FUNCTION;

  nsCOMPtr<nsIDocument> doc;

  nsCOMPtr<nsINode> scriptElem = do_QueryInterface(aRequest->mElement);

  // If there's no script text, we try to get it from the element
  if (aRequest->mIsInline) {
    // XXX This is inefficient - GetText makes multiple
    // copies.
    aRequest->mElement->GetScriptText(textData);

    script = &textData;
  }
  else {
    script = &aRequest->mScriptText;

    doc = scriptElem->GetOwnerDoc();
  }

  nsCOMPtr<nsIScriptElement> oldParserInsertedScript;
  PRUint32 parserCreated = aRequest->mElement->GetParserCreated();
  if (parserCreated) {
    oldParserInsertedScript = mCurrentParserInsertedScript;
    mCurrentParserInsertedScript = aRequest->mElement;
  }

  FireScriptAvailable(NS_OK, aRequest);

  bool runScript = true;
  nsContentUtils::DispatchTrustedEvent(scriptElem->GetOwnerDoc(),
                                       scriptElem,
                                       NS_LITERAL_STRING("beforescriptexecute"),
                                       PR_TRUE, PR_TRUE, &runScript);

  nsresult rv = NS_OK;
  if (runScript) {
    if (doc) {
      doc->BeginEvaluatingExternalScript();
    }
    aRequest->mElement->BeginEvaluating();
    rv = EvaluateScript(aRequest, *script);
    aRequest->mElement->EndEvaluating();
    if (doc) {
      doc->EndEvaluatingExternalScript();
    }

    nsContentUtils::DispatchTrustedEvent(scriptElem->GetOwnerDoc(),
                                         scriptElem,
                                         NS_LITERAL_STRING("afterscriptexecute"),
                                         PR_TRUE, PR_FALSE);
  }

  FireScriptEvaluated(rv, aRequest);

  if (parserCreated) {
    mCurrentParserInsertedScript = oldParserInsertedScript;
  }

  return rv;
}

void
nsScriptLoader::FireScriptAvailable(nsresult aResult,
                                    nsScriptLoadRequest* aRequest)
{
  for (PRInt32 i = 0; i < mObservers.Count(); i++) {
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
  for (PRInt32 i = 0; i < mObservers.Count(); i++) {
    nsCOMPtr<nsIScriptLoaderObserver> obs = mObservers[i];
    obs->ScriptEvaluated(aResult, aRequest->mElement,
                         aRequest->mIsInline);
  }

  aRequest->FireScriptEvaluated(aResult);
}

nsresult
nsScriptLoader::EvaluateScript(nsScriptLoadRequest* aRequest,
                               const nsAFlatString& aScript)
{
  nsresult rv = NS_OK;

  // We need a document to evaluate scripts.
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIContent> scriptContent(do_QueryInterface(aRequest->mElement));
  nsIDocument* ownerDoc = scriptContent->GetOwnerDoc();
  if (ownerDoc != mDocument) {
    // Willful violation of HTML5 as of 2010-12-01
    return NS_ERROR_FAILURE;
  }

  nsPIDOMWindow *pwin = mDocument->GetInnerWindow();
  if (!pwin || !pwin->IsInnerWindow()) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIScriptGlobalObject> globalObject = do_QueryInterface(pwin);
  NS_ASSERTION(globalObject, "windows must be global objects");

  // Get the script-type to be used by this element.
  NS_ASSERTION(scriptContent, "no content - what is default script-type?");
  PRUint32 stid = scriptContent ? scriptContent->GetScriptTypeID() :
                                  nsIProgrammingLanguage::JAVASCRIPT;
  // and make sure we are setup for this type of script.
  rv = globalObject->EnsureScriptEnvironment(stid);
  if (NS_FAILED(rv))
    return rv;

  // Make sure context is a strong reference since we access it after
  // we've executed a script, which may cause all other references to
  // the context to go away.
  nsCOMPtr<nsIScriptContext> context = globalObject->GetScriptContext(stid);
  if (!context) {
    return NS_ERROR_FAILURE;
  }

  nsIURI* uri = aRequest->mFinalURI ? aRequest->mFinalURI : aRequest->mURI;

  bool oldProcessingScriptTag = context->GetProcessingScriptTag();
  context->SetProcessingScriptTag(PR_TRUE);

  // Update our current script.
  nsCOMPtr<nsIScriptElement> oldCurrent = mCurrentScript;
  mCurrentScript = aRequest->mElement;

  nsCAutoString url;
  nsContentUtils::GetWrapperSafeScriptFilename(mDocument, uri, url);

  bool isUndefined;
  rv = context->EvaluateString(aScript,
                          globalObject->GetScriptGlobal(stid),
                          mDocument->NodePrincipal(), url.get(),
                          aRequest->mLineNo, aRequest->mJSVersion, nsnull,
                          &isUndefined);

  // Put the old script back in case it wants to do anything else.
  mCurrentScript = oldCurrent;

  JSContext *cx = nsnull; // Initialize this to keep GCC happy.
  if (stid == nsIProgrammingLanguage::JAVASCRIPT) {
    cx = context->GetNativeContext();
    ::JS_BeginRequest(cx);
    NS_ASSERTION(!::JS_IsExceptionPending(cx),
                 "JS_ReportPendingException wasn't called in EvaluateString");
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);

  if (stid == nsIProgrammingLanguage::JAVASCRIPT) {
    NS_ASSERTION(!::JS_IsExceptionPending(cx),
                 "JS_ReportPendingException wasn't called");
    ::JS_EndRequest(cx);
  }
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
      !mParserBlockingRequest->mLoading &&
      ReadyToExecuteScripts()) {
    request.swap(mParserBlockingRequest);
    // nsContentSink::ScriptAvailable unblocks the parser
    ProcessRequest(request);
  }

  while (ReadyToExecuteScripts() && 
         !mXSLTRequests.IsEmpty() && 
         !mXSLTRequests[0]->mLoading) {
    request.swap(mXSLTRequests[0]);
    mXSLTRequests.RemoveElementAt(0);
    ProcessRequest(request);
  }

  PRUint32 i = 0;
  while (mEnabled && i < mAsyncRequests.Length()) {
    if (!mAsyncRequests[i]->mLoading) {
      request.swap(mAsyncRequests[i]);
      mAsyncRequests.RemoveElementAt(i);
      ProcessRequest(request);
      continue;
    }
    ++i;
  }

  while (mEnabled && !mNonAsyncExternalScriptInsertedRequests.IsEmpty() &&
         !mNonAsyncExternalScriptInsertedRequests[0]->mLoading) {
    // Violate the HTML5 spec and execute these in the insertion order in
    // order to make LABjs and the "order" plug-in for RequireJS work with
    // their Gecko-sniffed code path. See
    // http://lists.w3.org/Archives/Public/public-html/2010Oct/0088.html
    request.swap(mNonAsyncExternalScriptInsertedRequests[0]);
    mNonAsyncExternalScriptInsertedRequests.RemoveElementAt(0);
    ProcessRequest(request);
  }

  if (mDocumentParsingDone && mXSLTRequests.IsEmpty()) {
    while (!mDeferRequests.IsEmpty() && !mDeferRequests[0]->mLoading) {
      request.swap(mDeferRequests[0]);
      mDeferRequests.RemoveElementAt(0);
      ProcessRequest(request);
    }
  }

  while (!mPendingChildLoaders.IsEmpty() && ReadyToExecuteScripts()) {
    nsRefPtr<nsScriptLoader> child = mPendingChildLoaders[0];
    mPendingChildLoaders.RemoveElementAt(0);
    child->RemoveExecuteBlocker();
  }

  if (mDocumentParsingDone && mDocument &&
      !mParserBlockingRequest && mAsyncRequests.IsEmpty() &&
      mNonAsyncExternalScriptInsertedRequests.IsEmpty() &&
      mXSLTRequests.IsEmpty() && mDeferRequests.IsEmpty()) {
    // No more pending scripts; time to unblock onload.
    // OK to unblock onload synchronously here, since callers must be
    // prepared for the world changing anyway.
    mDocumentParsingDone = PR_FALSE;
    mDocument->UnblockOnload(PR_TRUE);
  }
}

bool
nsScriptLoader::ReadyToExecuteScripts()
{
  // Make sure the SelfReadyToExecuteScripts check is first, so that
  // we don't block twice on an ancestor.
  if (!SelfReadyToExecuteScripts()) {
    return PR_FALSE;
  }
  
  for (nsIDocument* doc = mDocument; doc; doc = doc->GetParentDocument()) {
    nsScriptLoader* ancestor = doc->ScriptLoader();
    if (!ancestor->SelfReadyToExecuteScripts() &&
        ancestor->AddPendingChildLoader(this)) {
      AddExecuteBlocker();
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}


// This function was copied from nsParser.cpp. It was simplified a bit.
static bool
DetectByteOrderMark(const unsigned char* aBytes, PRInt32 aLen, nsCString& oCharset)
{
  if (aLen < 2)
    return PR_FALSE;

  switch(aBytes[0]) {
  case 0xEF:
    if (aLen >= 3 && 0xBB == aBytes[1] && 0xBF == aBytes[2]) {
      // EF BB BF
      // Win2K UTF-8 BOM
      oCharset.Assign("UTF-8");
    }
    break;
  case 0xFE:
    if (0xFF == aBytes[1]) {
      // FE FF
      // UTF-16, big-endian
      oCharset.Assign("UTF-16");
    }
    break;
  case 0xFF:
    if (0xFE == aBytes[1]) {
      // FF FE
      // UTF-16, little-endian
      oCharset.Assign("UTF-16");
    }
    break;
  }
  return !oCharset.IsEmpty();
}

/* static */ nsresult
nsScriptLoader::ConvertToUTF16(nsIChannel* aChannel, const PRUint8* aData,
                               PRUint32 aLength, const nsString& aHintCharset,
                               nsIDocument* aDocument, nsString& aString)
{
  if (!aLength) {
    aString.Truncate();
    return NS_OK;
  }

  nsCAutoString characterSet;

  nsresult rv = NS_OK;
  if (aChannel) {
    rv = aChannel->GetContentCharset(characterSet);
  }

  if (!aHintCharset.IsEmpty() && (NS_FAILED(rv) || characterSet.IsEmpty())) {
    // charset name is always ASCII.
    LossyCopyUTF16toASCII(aHintCharset, characterSet);
  }

  if (NS_FAILED(rv) || characterSet.IsEmpty()) {
    DetectByteOrderMark(aData, aLength, characterSet);
  }

  if (characterSet.IsEmpty() && aDocument) {
    // charset from document default
    characterSet = aDocument->GetDocumentCharacterSet();
  }

  if (characterSet.IsEmpty()) {
    // fall back to ISO-8859-1, see bug 118404
    characterSet.AssignLiteral("ISO-8859-1");
  }

  nsCOMPtr<nsICharsetConverterManager> charsetConv =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);

  nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;

  if (NS_SUCCEEDED(rv) && charsetConv) {
    rv = charsetConv->GetUnicodeDecoder(characterSet.get(),
                                        getter_AddRefs(unicodeDecoder));
    if (NS_FAILED(rv)) {
      // fall back to ISO-8859-1 if charset is not supported. (bug 230104)
      rv = charsetConv->GetUnicodeDecoderRaw("ISO-8859-1",
                                             getter_AddRefs(unicodeDecoder));
    }
  }

  // converts from the charset to unicode
  if (NS_SUCCEEDED(rv)) {
    PRInt32 unicodeLength = 0;

    rv = unicodeDecoder->GetMaxLength(reinterpret_cast<const char*>(aData),
                                      aLength, &unicodeLength);
    if (NS_SUCCEEDED(rv)) {
      if (!EnsureStringLength(aString, unicodeLength))
        return NS_ERROR_OUT_OF_MEMORY;

      PRUnichar *ustr = aString.BeginWriting();

      PRInt32 consumedLength = 0;
      PRInt32 originalLength = aLength;
      PRInt32 convertedLength = 0;
      PRInt32 bufferLength = unicodeLength;
      do {
        rv = unicodeDecoder->Convert(reinterpret_cast<const char*>(aData),
                                     (PRInt32 *) &aLength, ustr,
                                     &unicodeLength);
        if (NS_FAILED(rv)) {
          // if we failed, we consume one byte, replace it with U+FFFD
          // and try the conversion again.
          ustr[unicodeLength++] = (PRUnichar)0xFFFD;
          ustr += unicodeLength;

          unicodeDecoder->Reset();
        }
        aData += ++aLength;
        consumedLength += aLength;
        aLength = originalLength - consumedLength;
        convertedLength += unicodeLength;
        unicodeLength = bufferLength - convertedLength;
      } while (NS_FAILED(rv) && (originalLength > consumedLength) && (bufferLength > convertedLength));
      aString.SetLength(convertedLength);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsScriptLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                 nsISupports* aContext,
                                 nsresult aStatus,
                                 PRUint32 aStringLen,
                                 const PRUint8* aString)
{
  nsScriptLoadRequest* request = static_cast<nsScriptLoadRequest*>(aContext);
  NS_ASSERTION(request, "null request in stream complete handler");
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsresult rv = PrepareLoadedRequest(request, aLoader, aStatus, aStringLen,
                                     aString);
  if (NS_FAILED(rv)) {
    if (mDeferRequests.RemoveElement(request) ||
        mAsyncRequests.RemoveElement(request) ||
        mNonAsyncExternalScriptInsertedRequests.RemoveElement(request) ||
        mXSLTRequests.RemoveElement(request)) {
      FireScriptAvailable(rv, request);
    } else if (mParserBlockingRequest == request) {
      mParserBlockingRequest = nsnull;
      // nsContentSink::ScriptAvailable unblocks the parser
      FireScriptAvailable(rv, request);
    } else {
      mPreloads.RemoveElement(request, PreloadRequestComparator());
    }
  }

  // Process our request and/or any pending ones
  ProcessPendingRequests();

  return NS_OK;
}

nsresult
nsScriptLoader::PrepareLoadedRequest(nsScriptLoadRequest* aRequest,
                                     nsIStreamLoader* aLoader,
                                     nsresult aStatus,
                                     PRUint32 aStringLen,
                                     const PRUint8* aString)
{
  if (NS_FAILED(aStatus)) {
    return aStatus;
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
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);
  NS_GetFinalChannelURI(channel, getter_AddRefs(aRequest->mFinalURI));
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
                        aRequest->mScriptText);

    NS_ENSURE_SUCCESS(rv, rv);

    if (!ShouldExecuteScript(mDocument, channel)) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  // This assertion could fire errorously if we ran out of memory when
  // inserting the request in the array. However it's an unlikely case
  // so if you see this assertion it is likely something else that is
  // wrong, especially if you see it more than once.
  NS_ASSERTION(mDeferRequests.IndexOf(aRequest) >= 0 ||
               mAsyncRequests.IndexOf(aRequest) >= 0 ||
               mNonAsyncExternalScriptInsertedRequests.IndexOf(aRequest) >= 0 ||
               mXSLTRequests.IndexOf(aRequest) >= 0 ||
               mPreloads.Contains(aRequest, PreloadRequestComparator()) ||
               mParserBlockingRequest,
               "aRequest should be pending!");

  // Mark this as loaded
  aRequest->mLoading = PR_FALSE;

  return NS_OK;
}

/* static */
bool
nsScriptLoader::ShouldExecuteScript(nsIDocument* aDocument,
                                    nsIChannel* aChannel)
{
  if (!aChannel) {
    return PR_FALSE;
  }

  bool hasCert;
  nsIPrincipal* docPrincipal = aDocument->NodePrincipal();
  docPrincipal->GetHasCertificate(&hasCert);
  if (!hasCert) {
    return PR_TRUE;
  }

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsresult rv = nsContentUtils::GetSecurityManager()->
    GetChannelPrincipal(aChannel, getter_AddRefs(channelPrincipal));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  NS_ASSERTION(channelPrincipal, "Gotta have a principal here!");

  // If the channel principal isn't at least as powerful as the
  // document principal, then we don't execute the script.
  bool subsumes;
  rv = channelPrincipal->Subsumes(docPrincipal, &subsumes);
  return NS_SUCCEEDED(rv) && subsumes;
}

void
nsScriptLoader::ParsingComplete(bool aTerminated)
{
  if (mDeferEnabled) {
    // Have to check because we apparently get ParsingComplete
    // without BeginDeferringScripts in some cases
    mDocumentParsingDone = PR_TRUE;
  }
  mDeferEnabled = PR_FALSE;
  if (aTerminated) {
    mDeferRequests.Clear();
    mAsyncRequests.Clear();
    mNonAsyncExternalScriptInsertedRequests.Clear();
    mXSLTRequests.Clear();
    mParserBlockingRequest = nsnull;
  }

  // Have to call this even if aTerminated so we'll correctly unblock
  // onload and all.
  ProcessPendingRequests();
}

void
nsScriptLoader::PreloadURI(nsIURI *aURI, const nsAString &aCharset,
                           const nsAString &aType)
{
  // Check to see if scripts has been turned off.
  if (!mEnabled || !mDocument->IsScriptEnabled()) {
    return;
  }

  nsRefPtr<nsScriptLoadRequest> request = new nsScriptLoadRequest(nsnull, 0);
  request->mURI = aURI;
  request->mIsInline = PR_FALSE;
  request->mLoading = PR_TRUE;
  nsresult rv = StartLoad(request, aType);
  if (NS_FAILED(rv)) {
    return;
  }

  PreloadInfo *pi = mPreloads.AppendElement();
  pi->mRequest = request;
  pi->mCharset = aCharset;
}

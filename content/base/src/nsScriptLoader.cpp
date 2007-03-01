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

#include "nsScriptLoader.h"
#include "nsIDOMCharacterData.h"
#include "nsParserUtils.h"
#include "nsIMIMEHeaderParam.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsIContent.h"
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
#include "jscntxt.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "nsIXPConnect.h"
#include "nsContentErrors.h"
#include "nsIParser.h"
#include "nsThreadUtils.h"

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

// If aMaybeCertPrincipal is a cert principal and aNewPrincipal is not the same
// as aMaybeCertPrincipal, downgrade aMaybeCertPrincipal to a codebase
// principal.  Return the downgraded principal, or aMaybeCertPrincipal if no
// downgrade was needed.
static already_AddRefed<nsIPrincipal>
MaybeDowngradeToCodebase(nsIPrincipal *aMaybeCertPrincipal,
                         nsIPrincipal *aNewPrincipal)
{
  NS_PRECONDITION(aMaybeCertPrincipal, "Null old principal!");
  NS_PRECONDITION(aNewPrincipal, "Null new principal!");

  nsIPrincipal *principal = aMaybeCertPrincipal;

  PRBool hasCert;
  aMaybeCertPrincipal->GetHasCertificate(&hasCert);
  if (hasCert) {
    PRBool equal;
    aMaybeCertPrincipal->Equals(aNewPrincipal, &equal);
    if (!equal) {
      nsCOMPtr<nsIURI> uri, domain;
      aMaybeCertPrincipal->GetURI(getter_AddRefs(uri));
      aMaybeCertPrincipal->GetDomain(getter_AddRefs(domain));

      nsContentUtils::GetSecurityManager()->GetCodebasePrincipal(uri,
                                                                 &principal);
      if (principal && domain) {
        principal->SetDomain(domain);
      }

      return principal;
    }
  }

  NS_ADDREF(principal);

  return principal;
}

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

  nsCOMPtr<nsIScriptElement> mElement;
  PRPackedBool mLoading;             // Are we still waiting for a load to complete?
  PRPackedBool mIsInline;            // Is the script inline or loaded?
  nsString mScriptText;              // Holds script for loaded scripts
  PRUint32 mJSVersion;
  nsCOMPtr<nsIURI> mURI;
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
    mHadPendingScripts(PR_FALSE)
{
}

nsScriptLoader::~nsScriptLoader()
{
  mObservers.Clear();

  for (PRInt32 i = 0; i < mPendingRequests.Count(); i++) {
    mPendingRequests[i]->FireScriptAvailable(NS_ERROR_ABORT);
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

static PRBool
IsScriptEventHandler(nsIScriptElement *aScriptElement)
{
  nsCOMPtr<nsIContent> contElement = do_QueryInterface(aScriptElement);
  NS_ASSERTION(contElement, "nsIScriptElement isn't nsIContent");

  nsAutoString forAttr, eventAttr;
  if (!contElement->GetAttr(kNameSpaceID_None, nsGkAtoms::_for, forAttr) ||
      !contElement->GetAttr(kNameSpaceID_None, nsGkAtoms::event, eventAttr)) {
    return PR_FALSE;
  }

  const nsAString& for_str = nsContentUtils::TrimWhitespace(forAttr);
  if (!for_str.LowerCaseEqualsLiteral("window")) {
    return PR_TRUE;
  }

  // We found for="window", now check for event="onload".
  const nsAString& event_str = nsContentUtils::TrimWhitespace(eventAttr, PR_FALSE);
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
  if (globalObject)
  {
    nsIScriptContext *context = globalObject->GetScriptContext(
                                          nsIProgrammingLanguage::JAVASCRIPT);

    // If scripts aren't enabled in the current context, there's no
    // point in going on.
    if (context && !context->GetScriptsEnabled()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  // Default script language is whatever the root content specifies
  // (which may come from a header or http-meta tag)
  PRUint32 typeID = mDocument->GetRootContent()->GetScriptTypeID();
  PRUint32 version = 0;
  nsAutoString language, type, src;
  nsresult rv = NS_OK;

  // Check the type attribute to determine language and version.
  // If type exists, it trumps the deprecated 'language='
  aElement->GetScriptType(type);
  if (!type.IsEmpty()) {
    nsCOMPtr<nsIMIMEHeaderParam> mimeHdrParser =
      do_GetService("@mozilla.org/network/mime-hdrparam;1");
    NS_ENSURE_TRUE(mimeHdrParser, NS_ERROR_FAILURE);

    NS_ConvertUTF16toUTF8 typeAndParams(type);

    nsAutoString mimeType;
    rv = mimeHdrParser->GetParameter(typeAndParams, nsnull,
                                     EmptyCString(), PR_FALSE, nsnull,
                                     mimeType);
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

    PRBool isJavaScript = PR_FALSE;
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
      rv = mimeHdrParser->GetParameter(typeAndParams, "version",
                                       EmptyCString(), PR_FALSE, nsnull,
                                       versionName);
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

      rv = mimeHdrParser->GetParameter(typeAndParams, "e4x",
                                       EmptyCString(), PR_FALSE, nsnull,
                                       value);
      if (NS_FAILED(rv)) {
        if (rv != NS_ERROR_INVALID_ARG)
          return rv;
      } else {
        if (value.Length() == 1 && value[0] == '1')
          // This means that we need to set JSOPTION_XML in the JS options.
          // We re-use our knowledge of the implementation to reuse
          // JSVERSION_HAS_XML as a safe version flag.
          // If version has JSVERSION_UNKNOWN (-1), then this is still OK.
          version |= JSVERSION_HAS_XML;
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

  // Create a request object for this script
  nsRefPtr<nsScriptLoadRequest> request = new nsScriptLoadRequest(aElement, version);
  NS_ENSURE_TRUE(request, NS_ERROR_OUT_OF_MEMORY);

  // First check to see if this is an external script
  nsCOMPtr<nsIURI> scriptURI = aElement->GetScriptURI();
  if (scriptURI) {
    // Check that the containing page is allowed to load this URI.
    rv = nsContentUtils::GetSecurityManager()->
      CheckLoadURIWithPrincipal(mDocument->NodePrincipal(), scriptURI,
                                nsIScriptSecurityManager::ALLOW_CHROME);

    NS_ENSURE_SUCCESS(rv, rv);

    // After the security manager, the content-policy stuff gets a veto
    if (globalObject) {
      PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
      nsIURI *docURI = mDocument->GetDocumentURI();
      rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_SCRIPT,
                                     scriptURI,
                                     docURI,
                                     aElement,
                                     NS_LossyConvertUTF16toASCII(type),
                                     nsnull,    //extra
                                     &shouldLoad,
                                     nsContentUtils::GetContentPolicy());
      if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
        if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
          return NS_ERROR_CONTENT_BLOCKED;
        }
        return NS_ERROR_CONTENT_BLOCKED_SHOW_ALT;
      }

      request->mURI = scriptURI;
      request->mIsInline = PR_FALSE;
      request->mLoading = PR_TRUE;

      nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
      nsCOMPtr<nsIStreamLoader> loader;

      nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(globalObject));
      nsIDocShell *docshell = window->GetDocShell();

      nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

      nsCOMPtr<nsIChannel> channel;
      rv = NS_NewChannel(getter_AddRefs(channel),
                         scriptURI, nsnull, loadGroup,
                         prompter, nsIRequest::LOAD_NORMAL);
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

      rv = channel->AsyncOpen(loader, request);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    request->mLoading = PR_FALSE;
    request->mIsInline = PR_TRUE;
    request->mURI = mDocument->GetDocumentURI();

    request->mLineNo = aElement->GetScriptLineNumber();

    // If we've got existing pending requests, add ourselves
    // to this list.
    if (ReadyToExecuteScripts() && mPendingRequests.Count() == 0) {
      return ProcessRequest(request);
    }
  }

  // Add the request to our pending requests list
  NS_ENSURE_TRUE(mPendingRequests.AppendObject(request),
                 NS_ERROR_OUT_OF_MEMORY);

  // Added as pending request, now we can send blocking back
  return NS_ERROR_HTMLPARSER_BLOCK;
}

nsresult
nsScriptLoader::ProcessRequest(nsScriptLoadRequest* aRequest)
{
  NS_ASSERTION(ReadyToExecuteScripts(),
               "Caller forgot to check ReadyToExecuteScripts()");

  NS_ENSURE_ARG(aRequest);
  nsAFlatString* script;
  nsAutoString textData;

  // If there's no script text, we try to get it from the element
  if (aRequest->mIsInline) {
    // XXX This is inefficient - GetText makes multiple
    // copies.
    aRequest->mElement->GetScriptText(textData);

    script = &textData;
  }
  else {
    script = &aRequest->mScriptText;
  }

  FireScriptAvailable(NS_OK, aRequest);
  nsresult rv = EvaluateScript(aRequest, *script);
  FireScriptEvaluated(rv, aRequest);

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

  nsIScriptGlobalObject *globalObject = mDocument->GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, NS_ERROR_FAILURE);

  // Get the script-type to be used by this element.
  nsCOMPtr<nsIContent> scriptContent(do_QueryInterface(aRequest->mElement));
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

  nsCAutoString url;

  if (aRequest->mURI) {
    rv = aRequest->mURI->GetSpec(url);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  PRBool oldProcessingScriptTag = context->GetProcessingScriptTag();
  context->SetProcessingScriptTag(PR_TRUE);

  // Update our current script.
  nsCOMPtr<nsIScriptElement> oldCurrent = mCurrentScript;
  mCurrentScript = aRequest->mElement;

  PRBool isUndefined;
  rv = context->EvaluateString(aScript,
                          globalObject->GetScriptGlobal(stid),
                          mDocument->NodePrincipal(), url.get(),
                          aRequest->mLineNo, aRequest->mJSVersion, nsnull,
                          &isUndefined);

  // Put the old script back in case it wants to do anything else.
  mCurrentScript = oldCurrent;

  JSContext *cx = nsnull; // Initialize this to keep GCC happy.
  if (stid == nsIProgrammingLanguage::JAVASCRIPT) {
    cx = (JSContext *)context->GetNativeContext();
    ::JS_BeginRequest(cx);
    ::JS_ReportPendingException(cx);
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);

  if (stid == nsIProgrammingLanguage::JAVASCRIPT) {
    nsCOMPtr<nsIXPCNativeCallContext> ncc;
    nsContentUtils::XPConnect()->
      GetCurrentNativeCallContext(getter_AddRefs(ncc));

    if (ncc) {
      NS_ASSERTION(!::JS_IsExceptionPending(cx),
                   "JS_ReportPendingException wasn't called");
      ncc->SetExceptionWasThrown(PR_FALSE);
    }
    ::JS_EndRequest(cx);
  }
  return rv;
}

void
nsScriptLoader::ProcessPendingRequestsAsync()
{
  if (mPendingRequests.Count()) {
    nsCOMPtr<nsIRunnable> ev = new nsRunnableMethod<nsScriptLoader>(this,
      &nsScriptLoader::ProcessPendingRequests);

    NS_DispatchToCurrentThread(ev);
  }
}

void
nsScriptLoader::ProcessPendingRequests()
{
  nsRefPtr<nsScriptLoadRequest> request;
  while (ReadyToExecuteScripts() && mPendingRequests.Count() &&
         !(request = mPendingRequests[0])->mLoading) {
    mPendingRequests.RemoveObjectAt(0);
    ProcessRequest(request);
  }
}


// This function was copied from nsParser.cpp. It was simplified a bit.
static PRBool
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
      oCharset.Assign("UTF-16BE");
    }
    break;
  case 0xFF:
    if (0xFE == aBytes[1]) {
      // FF FE
      // UTF-16, little-endian
      oCharset.Assign("UTF-16LE");
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

  if (characterSet.IsEmpty()) {
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

    rv = unicodeDecoder->GetMaxLength(NS_REINTERPRET_CAST(const char*, aData),
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
        rv = unicodeDecoder->Convert(NS_REINTERPRET_CAST(const char*, aData),
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
  nsScriptLoadRequest* request = NS_STATIC_CAST(nsScriptLoadRequest*, aContext);
  NS_ASSERTION(request, "null request in stream complete handler");
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsresult rv = PrepareLoadedRequest(request, aLoader, aStatus, aStringLen,
                                     aString);
  if (NS_FAILED(rv)) {
    mPendingRequests.RemoveObject(request);
    FireScriptAvailable(rv, request);
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
    PRBool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(rv) && !requestSucceeded) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);
  if (aStringLen) {
    // Check the charset attribute to determine script charset.
    nsAutoString hintCharset;
    aRequest->mElement->GetScriptCharset(hintCharset);
    rv = ConvertToUTF16(channel, aString, aStringLen, hintCharset, mDocument,
                        aRequest->mScriptText);

    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Could not convert external JavaScript to Unicode!");
    NS_ENSURE_SUCCESS(rv, rv);

    // -- Merge the principal of the script file with that of the document; if
    // the script has a non-cert principal, the document's principal should be
    // downgraded.
    if (channel) {
      nsCOMPtr<nsISupports> owner;
      channel->GetOwner(getter_AddRefs(owner));
      nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);

      if (principal) {
        nsCOMPtr<nsIPrincipal> newPrincipal =
          MaybeDowngradeToCodebase(mDocument->NodePrincipal(), principal);

        mDocument->SetPrincipal(newPrincipal);
      }
    }
  }

  // This assertion could fire errorously if we ran out of memory when
  // inserting the request in the array. However it's an unlikely case
  // so if you see this assertion it is likely something else that is
  // wrong, especially if you see it more than once.
  NS_ASSERTION(mPendingRequests.IndexOf(aRequest) >= 0,
               "aRequest should be pending!");

  // Mark this as loaded
  aRequest->mLoading = PR_FALSE;

  return NS_OK;
}

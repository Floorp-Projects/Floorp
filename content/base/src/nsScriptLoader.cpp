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
#include "nsHTMLAtoms.h"
#include "nsNetUtil.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsIHttpChannel.h"
#include "nsIScriptElement.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDocShell.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "nsIXPConnect.h"
#include "nsContentErrors.h"

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
                      nsIScriptLoaderObserver* aObserver,
                      const char* aVersionString,
                      PRBool aHasE4XOption);
  virtual ~nsScriptLoadRequest();

  NS_DECL_ISUPPORTS

  void FireScriptAvailable(nsresult aResult,
                           const nsAFlatString& aScript);
  void FireScriptEvaluated(nsresult aResult);

  nsCOMPtr<nsIScriptElement> mElement;
  nsCOMPtr<nsIScriptLoaderObserver> mObserver;
  PRPackedBool mLoading;             // Are we still waiting for a load to complete?
  PRPackedBool mWasPending;          // Processed immediately or pending
  PRPackedBool mIsInline;            // Is the script inline or loaded?
  PRPackedBool mHasE4XOption;        // Has E4X=1 script type parameter
  nsString mScriptText;              // Holds script for loaded scripts
  const char* mJSVersion;            // We don't own this string
  nsCOMPtr<nsIURI> mURI;
  PRInt32 mLineNo;
};

nsScriptLoadRequest::nsScriptLoadRequest(nsIScriptElement* aElement,
                                         nsIScriptLoaderObserver* aObserver,
                                         const char* aVersionString,
                                         PRBool aHasE4XOption) :
  mElement(aElement), mObserver(aObserver),
  mLoading(PR_TRUE), mWasPending(PR_FALSE),
  mIsInline(PR_TRUE), mHasE4XOption(aHasE4XOption),
  mJSVersion(aVersionString), mLineNo(1)
{
}

nsScriptLoadRequest::~nsScriptLoadRequest()
{
}


// The nsScriptLoadRequest is passed as the context to necko, and thus
// it needs to be threadsafe. Necko won't do anything with this
// context, but it will AddRef and Release it on other threads.

NS_IMPL_THREADSAFE_ISUPPORTS0(nsScriptLoadRequest)

void
nsScriptLoadRequest::FireScriptAvailable(nsresult aResult,
                                         const nsAFlatString& aScript)
{
  if (mObserver) {
    mObserver->ScriptAvailable(aResult, mElement, mIsInline, mWasPending,
                               mURI, mLineNo,
                               aScript);
  }
}

void
nsScriptLoadRequest::FireScriptEvaluated(nsresult aResult)
{
  if (mObserver) {
    mObserver->ScriptEvaluated(aResult, mElement, mIsInline, mWasPending);
  }
}

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

nsScriptLoader::nsScriptLoader()
  : mDocument(nsnull), mEnabled(PR_TRUE)
{
}

nsScriptLoader::~nsScriptLoader()
{
  mObservers.Clear();

  PRInt32 count = mPendingRequests.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsScriptLoadRequest* req = mPendingRequests[i];
    if (req) {
      req->FireScriptAvailable(NS_ERROR_ABORT, EmptyString());
    }
  }

  mPendingRequests.Clear();
}

NS_INTERFACE_MAP_BEGIN(nsScriptLoader)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptLoader)
  NS_INTERFACE_MAP_ENTRY(nsIScriptLoader)
  NS_INTERFACE_MAP_ENTRY(nsIStreamLoaderObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsScriptLoader)
NS_IMPL_RELEASE(nsScriptLoader)

/* void init (in nsIDocument aDocument); */
NS_IMETHODIMP
nsScriptLoader::Init(nsIDocument *aDocument)
{
  mDocument = aDocument;

  return NS_OK;
}

/* void dropDocumentReference (); */
NS_IMETHODIMP
nsScriptLoader::DropDocumentReference()
{
  mDocument = nsnull;

  return NS_OK;
}

/* void addObserver (in nsIScriptLoaderObserver aObserver); */
NS_IMETHODIMP
nsScriptLoader::AddObserver(nsIScriptLoaderObserver *aObserver)
{
  NS_ENSURE_ARG(aObserver);

  mObservers.AppendObject(aObserver);

  return NS_OK;
}

/* void removeObserver (in nsIScriptLoaderObserver aObserver); */
NS_IMETHODIMP
nsScriptLoader::RemoveObserver(nsIScriptLoaderObserver *aObserver)
{
  NS_ENSURE_ARG(aObserver);

  mObservers.RemoveObject(aObserver);

  return NS_OK;
}

PRBool
nsScriptLoader::InNonScriptingContainer(nsIScriptElement* aScriptElement)
{
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aScriptElement));
  nsCOMPtr<nsIDOMNode> parent;

  node->GetParentNode(getter_AddRefs(parent));
  while (parent) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(parent));
    if (!content) {
      break;
    }

    nsIAtom *localName = content->Tag();

    // XXX noframes and noembed are currently unconditionally not
    // displayed and processed. This might change if we support either
    // prefs or per-document container settings for not allowing
    // frames or plugins.
    if (content->IsNodeOfType(nsINode::eHTML) &&
        (localName == nsHTMLAtoms::iframe ||
         localName == nsHTMLAtoms::noframes ||
         localName == nsHTMLAtoms::noembed)) {
      return PR_TRUE;
    }

    node = parent;
    node->GetParentNode(getter_AddRefs(parent));
  }

  return PR_FALSE;
}

// Helper method for checking if the script element is an event-handler
// This means that it has both a for-attribute and a event-attribute.
// Also, if the for-attribute has a value that matches "\s*window\s*",
// and the event-attribute matches "\s*onload([ \(].*)?" then it isn't an
// eventhandler. (both matches are case insensitive).
// This is how IE seems to filter out a window's onload handler from a
// <script for=... event=...> element.

PRBool
nsScriptLoader::IsScriptEventHandler(nsIScriptElement *aScriptElement)
{
  nsCOMPtr<nsIContent> contElement = do_QueryInterface(aScriptElement);
  NS_ASSERTION(contElement, "nsIScriptElement isn't nsIContent");

  nsAutoString forAttr, eventAttr;
  if (!contElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::_for, forAttr) ||
      !contElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::event, eventAttr)) {
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

/* void processScriptElement (in nsIScriptElement aElement, in nsIScriptLoaderObserver aObserver); */
NS_IMETHODIMP
nsScriptLoader::ProcessScriptElement(nsIScriptElement *aElement,
                                     nsIScriptLoaderObserver *aObserver)
{
  PRBool fireErrorNotification;
  nsresult rv = DoProcessScriptElement(aElement, aObserver,
                                       &fireErrorNotification);
  if (fireErrorNotification) {
    // Note that rv _can_ be a success code here.  It just can't be NS_OK.
    NS_ASSERTION(rv != NS_OK, "Firing error notification for NS_OK?");
    FireErrorNotification(rv, aElement, aObserver);
  }

  return rv;  
}

nsresult
nsScriptLoader::DoProcessScriptElement(nsIScriptElement *aElement,
                                       nsIScriptLoaderObserver *aObserver,
                                       PRBool* aFireErrorNotification)
{
  // Default to firing the error notification until we've actually gotten to
  // loading or running the script.
  *aFireErrorNotification = PR_TRUE;
  
  NS_ENSURE_ARG(aElement);

  // We need a document to evaluate scripts.
  NS_ENSURE_TRUE(mDocument, NS_ERROR_FAILURE);

  // Check to see that the element is not in a container that
  // suppresses script evaluation within it and that we should be
  // evaluating scripts for this document in the first place.
  if (!mEnabled || !mDocument->IsScriptEnabled() ||
      aElement->IsMalformed() || InNonScriptingContainer(aElement)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Check that the script is not an eventhandler
  if (IsScriptEventHandler(aElement)) {
    return NS_CONTENT_SCRIPT_IS_EVENTHANDLER;
  }

  // Script evaluation can also be disabled in the current script
  // context even though it's enabled in the document.
  nsIScriptGlobalObject *globalObject = mDocument->GetScriptGlobalObject();
  if (globalObject)
  {
    nsIScriptContext *context = globalObject->GetContext();

    // If scripts aren't enabled in the current context, there's no
    // point in going on.
    if (context && !context->GetScriptsEnabled()) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  PRBool isJavaScript = PR_TRUE;
  PRBool hasE4XOption = PR_FALSE;
  const char* jsVersionString = nsnull;
  nsAutoString language, type, src;

  // "language" is a deprecated attribute of HTML, so we check it only for
  // HTML script elements.
  nsCOMPtr<nsIDOMHTMLScriptElement> htmlScriptElement =
    do_QueryInterface(aElement);
  if (htmlScriptElement) {
    // Check the language attribute first, so type can trump language.
    htmlScriptElement->GetAttribute(NS_LITERAL_STRING("language"), language);
    if (!language.IsEmpty()) {
      isJavaScript = nsParserUtils::IsJavaScriptLanguage(language,
                                                         &jsVersionString);

      // IE, Opera, etc. do not respect language version, so neither should
      // we at this late date in the browser wars saga.  Note that this change
      // affects HTML but not XUL or SVG (but note also that XUL has its own
      // code to check nsParserUtils::IsJavaScriptLanguage -- that's probably
      // a separate bug, one we may not be able to fix short of XUL2).  See
      // bug 255895 (https://bugzilla.mozilla.org/show_bug.cgi?id=255895).
      jsVersionString = ::JS_VersionToString(JSVERSION_DEFAULT);
    }
  }

  nsresult rv = NS_OK;

  // Check the type attribute to determine language and version.
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

    isJavaScript = PR_FALSE;
    for (PRInt32 i = 0; jsTypes[i]; i++) {
      if (mimeType.LowerCaseEqualsASCII(jsTypes[i])) {
        isJavaScript = PR_TRUE;
        break;
      }
    }

    if (isJavaScript) {
      JSVersion jsVersion = JSVERSION_DEFAULT;
      nsAutoString value;
      rv = mimeHdrParser->GetParameter(typeAndParams, "version",
                                       EmptyCString(), PR_FALSE, nsnull,
                                       value);
      if (NS_FAILED(rv)) {
        if (rv != NS_ERROR_INVALID_ARG)
          return rv;
      } else {
        if (value.Length() != 3 || value[0] != '1' || value[1] != '.')
          jsVersion = JSVERSION_UNKNOWN;
        else switch (value[2]) {
          case '0': jsVersion = JSVERSION_1_0; break;
          case '1': jsVersion = JSVERSION_1_1; break;
          case '2': jsVersion = JSVERSION_1_2; break;
          case '3': jsVersion = JSVERSION_1_3; break;
          case '4': jsVersion = JSVERSION_1_4; break;
          case '5': jsVersion = JSVERSION_1_5; break;
          case '6': jsVersion = JSVERSION_1_6; break;
          default:  jsVersion = JSVERSION_UNKNOWN;
        }
      }
      jsVersionString = ::JS_VersionToString(jsVersion);

      rv = mimeHdrParser->GetParameter(typeAndParams, "e4x",
                                       EmptyCString(), PR_FALSE, nsnull,
                                       value);
      if (NS_FAILED(rv)) {
        if (rv != NS_ERROR_INVALID_ARG)
          return rv;
      } else {
        if (value.Length() == 1 && value[0] == '1')
          hasE4XOption = PR_TRUE;
      }
    }
  }

  // If this isn't JavaScript, we don't know how to evaluate.
  // XXX How and where should we deal with other scripting languages?
  //     See bug 255942 (https://bugzilla.mozilla.org/show_bug.cgi?id=255942).
  if (!isJavaScript) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Create a request object for this script
  nsRefPtr<nsScriptLoadRequest> request = new nsScriptLoadRequest(aElement, aObserver, jsVersionString, hasE4XOption);
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
      request->mWasPending = PR_TRUE;
      request->mLoading = PR_TRUE;

      // Add the request to our pending requests list
      mPendingRequests.AppendObject(request);

      nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
      nsCOMPtr<nsIStreamLoader> loader;

      nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(globalObject));
      nsIDocShell *docshell = window->GetDocShell();

      nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

      nsCOMPtr<nsIChannel> channel;
      rv = NS_NewChannel(getter_AddRefs(channel),
                         scriptURI, nsnull, loadGroup,
                         prompter, nsIRequest::LOAD_NORMAL);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
        if (httpChannel) {
          // HTTP content negotation has little value in this context.
          httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                        NS_LITERAL_CSTRING("*/*"),
                                        PR_FALSE);
          httpChannel->SetReferrer(mDocument->GetDocumentURI());
        }
        rv = NS_NewStreamLoader(getter_AddRefs(loader), channel, this, request);
      }
      if (NS_FAILED(rv)) {
        mPendingRequests.RemoveObject(request);
        return rv;
      }

      // At this point we've successfully started the load, so we need not call
      // FireErrorNotification anymore.
      *aFireErrorNotification = PR_FALSE;
    }
  } else {
    request->mLoading = PR_FALSE;
    request->mIsInline = PR_TRUE;
    request->mURI = mDocument->GetDocumentURI();

    request->mLineNo = aElement->GetScriptLineNumber();

    // If we've got existing pending requests, add ourselves
    // to this list.
    if (mPendingRequests.Count() > 0) {
      request->mWasPending = PR_TRUE;
      NS_ENSURE_TRUE(mPendingRequests.AppendObject(request),
                     NS_ERROR_OUT_OF_MEMORY);
    }
    else {
      request->mWasPending = PR_FALSE;
      rv = ProcessRequest(request);
    }

    // We're either going to, or have run this inline script, so we shouldn't
    // call FireErrorNotification for it.
    *aFireErrorNotification = PR_FALSE;
  }

  return rv;
}

nsresult
nsScriptLoader::GetCurrentScript(nsIScriptElement **aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = mCurrentScript;

  NS_IF_ADDREF(*aElement);

  return NS_OK;
}

void
nsScriptLoader::FireErrorNotification(nsresult aResult,
                                      nsIScriptElement* aElement,
                                      nsIScriptLoaderObserver* aObserver)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIScriptLoaderObserver> observer = mObservers[i];

    if (observer) {
      observer->ScriptAvailable(aResult, aElement,
                                PR_TRUE, PR_FALSE,
                                nsnull, 0,
                                EmptyString());
    }
  }

  if (aObserver) {
    aObserver->ScriptAvailable(aResult, aElement,
                               PR_TRUE, PR_FALSE,
                               nsnull, 0,
                               EmptyString());
  }
}

nsresult
nsScriptLoader::ProcessRequest(nsScriptLoadRequest* aRequest)
{
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

  FireScriptAvailable(NS_OK, aRequest, *script);
  nsresult rv = EvaluateScript(aRequest, *script);
  FireScriptEvaluated(rv, aRequest);

  return rv;
}

void
nsScriptLoader::FireScriptAvailable(nsresult aResult,
                                    nsScriptLoadRequest* aRequest,
                                    const nsAFlatString& aScript)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIScriptLoaderObserver> observer = mObservers[i];

    if (observer) {
      observer->ScriptAvailable(aResult, aRequest->mElement,
                                aRequest->mIsInline, aRequest->mWasPending,
                                aRequest->mURI, aRequest->mLineNo,
                                aScript);
    }
  }

  aRequest->FireScriptAvailable(aResult, aScript);
}

void
nsScriptLoader::FireScriptEvaluated(nsresult aResult,
                                    nsScriptLoadRequest* aRequest)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIScriptLoaderObserver> observer = mObservers[i];

    if (observer) {
      observer->ScriptEvaluated(aResult, aRequest->mElement,
                                aRequest->mIsInline, aRequest->mWasPending);
    }
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

  // Make sure context is a strong reference since we access it after
  // we've executed a script, which may cause all other references to
  // the context to go away.
  nsCOMPtr<nsIScriptContext> context = globalObject->GetContext();
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

  JSContext *cx = (JSContext *)context->GetNativeContext();
  uint32 options = ::JS_GetOptions(cx);
  JSBool changed = (aRequest->mHasE4XOption ^ !!(options & JSOPTION_XML));
  if (changed) {
    ::JS_SetOptions(cx,
                    aRequest->mHasE4XOption
                    ? options | JSOPTION_XML
                    : options & ~JSOPTION_XML);
  }

  // Update our current script.
  nsCOMPtr<nsIScriptElement> oldCurrent = mCurrentScript;
  mCurrentScript = aRequest->mElement;

  PRBool isUndefined;
  rv = context->EvaluateString(aScript, globalObject->GetGlobalJSObject(),
                               mDocument->NodePrincipal(), url.get(),
                               aRequest->mLineNo, aRequest->mJSVersion, nsnull,
                               &isUndefined);

  // Put the old script back in case it wants to do anything else.
  mCurrentScript = oldCurrent;

  JSAutoRequest ar(cx);

  if (NS_FAILED(rv)) {
    ::JS_ReportPendingException(cx);
  }
  if (changed) {
    ::JS_SetOptions(cx, options);
  }

  context->SetProcessingScriptTag(oldProcessingScriptTag);

  nsCOMPtr<nsIXPCNativeCallContext> ncc;
  nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(ncc));

  if (ncc) {
    ncc->SetExceptionWasThrown(PR_FALSE);
  }

  return rv;
}

void
nsScriptLoader::ProcessPendingReqests()
{
  if (mPendingRequests.Count() == 0) {
    return;
  }

  nsRefPtr<nsScriptLoadRequest> request = mPendingRequests[0];
  while (request && !request->mLoading) {
    mPendingRequests.RemoveObjectAt(0);
    ProcessRequest(request);
    if (mPendingRequests.Count() == 0) {
      return;
    }
    request = mPendingRequests[0];
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
      aString.SetLength(unicodeLength);
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
                                 PRUint32 stringLen,
                                 const PRUint8* string)
{
  nsresult rv;
  nsScriptLoadRequest* request = NS_STATIC_CAST(nsScriptLoadRequest*, aContext);
  NS_ASSERTION(request, "null request in stream complete handler");
  if (!request) {
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(aStatus)) {
    mPendingRequests.RemoveObject(request);
    FireScriptAvailable(aStatus, request, EmptyString());
    ProcessPendingReqests();
    return NS_OK;
  }

  // If we don't have a document, then we need to abort further
  // evaluation.
  if (!mDocument) {
    mPendingRequests.RemoveObject(request);
    FireScriptAvailable(NS_ERROR_NOT_AVAILABLE, request,
                        EmptyString());
    ProcessPendingReqests();
    return NS_OK;
  }

  // If the load returned an error page, then we need to abort
  nsCOMPtr<nsIRequest> req;
  rv = aLoader->GetRequest(getter_AddRefs(req));
  NS_ASSERTION(req, "StreamLoader's request went away prematurely");
  if (NS_FAILED(rv)) return rv;  // XXX Should this remove the pending request?
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(req));
  if (httpChannel) {
    PRBool requestSucceeded;
    rv = httpChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(rv) && !requestSucceeded) {
      mPendingRequests.RemoveObject(request);
      FireScriptAvailable(NS_ERROR_NOT_AVAILABLE, request,
                          EmptyString());
      ProcessPendingReqests();
      return NS_OK;
    }
  }

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(req);
  if (stringLen) {
    // Check the charset attribute to determine script charset.
    nsAutoString hintCharset;
    request->mElement->GetScriptCharset(hintCharset);
    rv = ConvertToUTF16(channel, string, stringLen, hintCharset, mDocument,
                        request->mScriptText);

    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Could not convert external JavaScript to Unicode!");
    if (NS_FAILED(rv)) {
      mPendingRequests.RemoveObject(request);
      FireScriptAvailable(rv, request, EmptyString());
      ProcessPendingReqests();
      return NS_OK;
    }

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


  // If we're not the first in the pending list, we mark ourselves
  // as loaded and just stay on the list.
  NS_ASSERTION(mPendingRequests.Count() > 0, "aContext is a pending request!");
  if (mPendingRequests[0] != request) {
    request->mLoading = PR_FALSE;
    return NS_OK;
  }

  mPendingRequests.RemoveObject(request);
  ProcessRequest(request);

  // Process any pending requests
  ProcessPendingReqests();

  return NS_OK;
}

NS_IMETHODIMP
nsScriptLoader::GetEnabled(PRBool *aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);
  *aEnabled = mEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptLoader::SetEnabled(PRBool aEnabled)
{
  mEnabled = aEnabled;
  return NS_OK;
}

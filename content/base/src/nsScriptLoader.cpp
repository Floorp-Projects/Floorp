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

#include "nsScriptLoader.h"
#include "nsIDOMCharacterData.h"
#include "nsParserUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsNetUtil.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsINodeInfo.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsIHttpChannel.h"
#include "nsIScriptElement.h"
#include "nsIDocShell.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

static already_AddRefed<nsIPrincipal>
IntersectPrincipalCerts(nsIPrincipal *aOld, nsIPrincipal *aNew)
{
  NS_PRECONDITION(aOld, "Null old principal!");
  NS_PRECONDITION(aNew, "Null new principal!");

  nsIPrincipal *principal = aOld;

  PRBool hasCert;
  aOld->GetHasCertificate(&hasCert);
  if (hasCert) {
    PRBool equal;
    aOld->Equals(aNew, &equal);
    if (!equal) {
      nsCOMPtr<nsIURI> uri, domain;
      aOld->GetURI(getter_AddRefs(uri));
      aOld->GetDomain(getter_AddRefs(domain));

      nsContentUtils::GetSecurityManager()->GetCodebasePrincipal(uri, &principal);
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
  nsScriptLoadRequest(nsIDOMHTMLScriptElement* aElement,
                      nsIScriptLoaderObserver* aObserver,
                      const char* aVersionString);
  virtual ~nsScriptLoadRequest();

  NS_DECL_ISUPPORTS
  
  void FireScriptAvailable(nsresult aResult,
                           const nsAFlatString& aScript);
  void FireScriptEvaluated(nsresult aResult);
  
  nsCOMPtr<nsIDOMHTMLScriptElement> mElement;
  nsCOMPtr<nsIScriptLoaderObserver> mObserver;
  PRPackedBool mLoading;             // Are we still waiting for a load to complete?
  PRPackedBool mWasPending;          // Processed immediately or pending
  PRPackedBool mIsInline;            // Is the script inline or loaded?
  nsString mScriptText;              // Holds script for loaded scripts
  const char* mJSVersion;            // We don't own this string
  nsCOMPtr<nsIURI> mURI;
  PRInt32 mLineNo;
};

nsScriptLoadRequest::nsScriptLoadRequest(nsIDOMHTMLScriptElement* aElement,
                                         nsIScriptLoaderObserver* aObserver,
                                         const char* aVersionString) :
  mElement(aElement), mObserver(aObserver), 
  mLoading(PR_TRUE), mWasPending(PR_FALSE),
  mIsInline(PR_TRUE), mJSVersion(aVersionString), mLineNo(1)
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
nsScriptLoader::InNonScriptingContainer(nsIDOMHTMLScriptElement* aScriptElement)
{
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aScriptElement));
  nsCOMPtr<nsIDOMNode> parent;

  node->GetParentNode(getter_AddRefs(parent));
  while (parent) {   
    nsCOMPtr<nsIContent> content(do_QueryInterface(parent));
    if (!content) {
      break;
    }

    nsINodeInfo *nodeInfo = content->GetNodeInfo();
    NS_ASSERTION(nodeInfo, "element without node info");

    if (nodeInfo) {
      nsIAtom *localName = nodeInfo->NameAtom();
      
      // XXX noframes and noembed are currently unconditionally not
      // displayed and processed. This might change if we support either
      // prefs or per-document container settings for not allowing
      // frames or plugins.
      if (content->IsContentOfType(nsIContent::eHTML) &&
          ((localName == nsHTMLAtoms::iframe) ||
           (localName == nsHTMLAtoms::noframes) ||
           (localName == nsHTMLAtoms::noembed))) {
        return PR_TRUE;
      }
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
nsScriptLoader::IsScriptEventHandler(nsIDOMHTMLScriptElement *aScriptElement)
{
  nsCOMPtr<nsIContent> contElement = do_QueryInterface(aScriptElement);
  if (!contElement ||
      !contElement->HasAttr(kNameSpaceID_None, nsHTMLAtoms::_event) ||
      !contElement->HasAttr(kNameSpaceID_None, nsHTMLAtoms::_for)) {
      return PR_FALSE;
  }

  nsAutoString str;
  nsresult rv = contElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::_for,
                                     str);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  const nsAString& for_str = nsContentUtils::TrimWhitespace(str);

  if (!for_str.Equals(NS_LITERAL_STRING("window"),
                      nsCaseInsensitiveStringComparator())) {
    return PR_TRUE;
  }

  // We found for="window", now check for event="onload".

  rv = contElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::_event, str);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  const nsAString& event_str = nsContentUtils::TrimWhitespace(str, PR_FALSE);

  if (event_str.Length() < 6) {
    // String too short, can't be "onload".

    return PR_TRUE;
  }

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

/* void processScriptElement (in nsIDOMHTMLScriptElement aElement, in nsIScriptLoaderObserver aObserver); */
NS_IMETHODIMP 
nsScriptLoader::ProcessScriptElement(nsIDOMHTMLScriptElement *aElement, 
                                     nsIScriptLoaderObserver *aObserver)
{
  NS_ENSURE_ARG(aElement);

  nsresult rv = NS_OK;

  // We need a document to evaluate scripts.
  if (!mDocument) {
    return FireErrorNotification(NS_ERROR_FAILURE, aElement, aObserver);
  }

  // Check to see that the element is not in a container that
  // suppresses script evaluation within it.
  if (!mEnabled || InNonScriptingContainer(aElement)) {
    return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement, aObserver);
  }

  // Check that the script is not an eventhandler
  if (IsScriptEventHandler(aElement)) {
    return FireErrorNotification(NS_CONTENT_SCRIPT_IS_EVENTHANDLER, aElement,
                                 aObserver);
  }

  // Check whether we should be executing scripts at all for this document
  if (!mDocument->IsScriptEnabled()) {
    return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement, aObserver);
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
      return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement,
                                   aObserver);
    }
  }

  PRBool isJavaScript = PR_TRUE;
  const char* jsVersionString = nsnull;
  nsAutoString language, type, src;

  // Check the language attribute first, so type can trump language.
  aElement->GetAttribute(NS_LITERAL_STRING("language"), language);
  if (!language.IsEmpty()) {
    isJavaScript = nsParserUtils::IsJavaScriptLanguage(language, &jsVersionString);
  }

  // Check the type attribute to determine language and version.
  aElement->GetType(type);
  if (!type.IsEmpty()) {
    nsAutoString  mimeType;
    nsAutoString  params;
    nsParserUtils::SplitMimeType(type, mimeType, params);

    isJavaScript = mimeType.EqualsIgnoreCase("application/x-javascript") || 
                   mimeType.EqualsIgnoreCase("text/javascript");
    if (isJavaScript) {
      JSVersion jsVersion = JSVERSION_DEFAULT;
      if (params.Find("version=", PR_TRUE) == 0) {
        if (params.Length() != 11 || params[8] != '1' || params[9] != '.')
          jsVersion = JSVERSION_UNKNOWN;
        else switch (params[10]) {
        case '0': jsVersion = JSVERSION_1_0; break;
        case '1': jsVersion = JSVERSION_1_1; break;
        case '2': jsVersion = JSVERSION_1_2; break;
        case '3': jsVersion = JSVERSION_1_3; break;
        case '4': jsVersion = JSVERSION_1_4; break;
        case '5': jsVersion = JSVERSION_1_5; break;
        default:  jsVersion = JSVERSION_UNKNOWN;
        }
      }
      jsVersionString = JS_VersionToString(jsVersion);
    }
  }

  // If this isn't JavaScript, we don't know how to evaluate.
  // XXX How and where should we deal with other scripting languages??
  if (!isJavaScript) {
    return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement, aObserver);
  }

  // Create a request object for this script
  nsRefPtr<nsScriptLoadRequest> request = new nsScriptLoadRequest(aElement, aObserver, jsVersionString);
  if (!request) {
    return FireErrorNotification(NS_ERROR_OUT_OF_MEMORY, aElement, aObserver);
  }

  // Check to see if we have a src attribute.
  aElement->GetSrc(src);
  if (!src.IsEmpty()) {
    nsCOMPtr<nsIURI> scriptURI;

    // Use the SRC attribute value to load the URL
    nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
    NS_ASSERTION(content, "nsIDOMHTMLScriptElement not implementing nsIContent");
    nsCOMPtr<nsIURI> baseURI = content->GetBaseURI();
    rv = NS_NewURI(getter_AddRefs(scriptURI), src, nsnull, baseURI);

    if (NS_FAILED(rv)) {
      return FireErrorNotification(rv, aElement, aObserver);
    }

    // Check that the containing page is allowed to load this URI.
    nsIPrincipal *docPrincipal = mDocument->GetPrincipal();
    if (!docPrincipal) {
      return FireErrorNotification(NS_ERROR_UNEXPECTED, aElement, aObserver);
    }
    rv = nsContentUtils::GetSecurityManager()->
      CheckLoadURIWithPrincipal(docPrincipal, scriptURI,
                                nsIScriptSecurityManager::ALLOW_CHROME);

    if (NS_FAILED(rv)) {
      return FireErrorNotification(rv, aElement, aObserver);
    }
    
    // After the security manager, the content-policy stuff gets a veto
    if (globalObject) {
      PRInt16 shouldLoad = nsIContentPolicy::ACCEPT;
      nsIURI *docURI = mDocument->GetDocumentURI();
      rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_SCRIPT,
                                     scriptURI,
                                     docURI,
                                     aElement,
                                     NS_LossyConvertUCS2toASCII(type),
                                     nsnull,    //extra
                                     &shouldLoad);
      if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
        if (NS_FAILED(rv) || shouldLoad != nsIContentPolicy::REJECT_TYPE) {
          return FireErrorNotification(NS_ERROR_CONTENT_BLOCKED, aElement,
                                       aObserver);
        }
        return FireErrorNotification(NS_ERROR_CONTENT_BLOCKED_SHOW_ALT,
                                     aElement, aObserver);
      }

      request->mURI = scriptURI;
      request->mIsInline = PR_FALSE;
      request->mWasPending = PR_TRUE;
      request->mLoading = PR_TRUE;

      // Add the request to our pending requests list
      mPendingRequests.AppendObject(request);

      nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
      nsCOMPtr<nsIStreamLoader> loader;

      nsIDocShell *docshell = globalObject->GetDocShell();

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
        return FireErrorNotification(rv, aElement, aObserver);
      }
    }
  } else {
    request->mLoading = PR_FALSE;
    request->mIsInline = PR_TRUE;
    request->mURI = mDocument->GetDocumentURI();

    nsCOMPtr<nsIScriptElement> scriptElement(do_QueryInterface(aElement));
    if (scriptElement) {
      PRUint32 lineNumber;
      scriptElement->GetLineNumber(&lineNumber);
      request->mLineNo = lineNumber;
    }

    // If we've got existing pending requests, add ourselves
    // to this list.
    if (mPendingRequests.Count() > 0) {
      request->mWasPending = PR_TRUE;
      mPendingRequests.AppendObject(request);
    }
    else {
      request->mWasPending = PR_FALSE;
      rv = ProcessRequest(request);
    }
  }
  
  return rv;
}

nsresult
nsScriptLoader::FireErrorNotification(nsresult aResult,
                                      nsIDOMHTMLScriptElement* aElement,
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

  return aResult;
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
    aRequest->mElement->GetText(textData);

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

  nsIPrincipal *principal = mDocument->GetPrincipal();
  // We can survive without a principal, but we really should
  // have one.
  NS_ASSERTION(principal, "principal required for document");

  nsCAutoString url;

  if (aRequest->mURI) {
    rv = aRequest->mURI->GetSpec(url);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  context->SetProcessingScriptTag(PR_TRUE);

  PRBool isUndefined;
  context->EvaluateString(aScript, nsnull, principal, url.get(),
                          aRequest->mLineNo, aRequest->mJSVersion, nsnull,
                          &isUndefined);  

  context->SetProcessingScriptTag(PR_FALSE);

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


// This function is copied from nsParser.cpp. It was simplified though, unnecessary part is removed.
static PRBool 
DetectByteOrderMark(const unsigned char* aBytes, PRInt32 aLen, nsCString& oCharset) 
{
  if (aLen < 2)
    return PR_FALSE;

  switch(aBytes[0]) {
  case 0xEF:  
    if( aLen >= 3 && (0xBB==aBytes[1]) && (0xBF==aBytes[2])) {
        // EF BB BF
        // Win2K UTF-8 BOM
        oCharset.Assign("UTF-8"); 
    }
    break;
  case 0xFE:
    if(0xFF==aBytes[1]) {
      // FE FF
      // UTF-16, big-endian 
      oCharset.Assign("UTF-16BE"); 
    }
    break;
  case 0xFF:
    if(0xFE==aBytes[1]) {
      // FF FE
      // UTF-16, little-endian 
      oCharset.Assign("UTF-16LE"); 
    }
    break;
  }  // switch
  return !oCharset.IsEmpty();
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

  if (stringLen) {
    nsCAutoString characterSet;
    nsCOMPtr<nsIChannel> channel;

    channel = do_QueryInterface(req);
    if (channel) {
      rv = channel->GetContentCharset(characterSet);
    }

    if (NS_FAILED(rv) || characterSet.IsEmpty()) {
      // Check the charset attribute to determine script charset.
      nsAutoString charset;
      rv = request->mElement->GetCharset(charset);
      if (NS_SUCCEEDED(rv)) {  
        // charset name is always ASCII.
        LossyCopyUTF16toASCII(charset, characterSet);
      }
    }

    if (NS_FAILED(rv) || characterSet.IsEmpty()) {
      DetectByteOrderMark(string, stringLen, characterSet);
    }

    if (characterSet.IsEmpty()) {
      // charset from document default
      characterSet = mDocument->GetDocumentCharacterSet();
    }

    if (characterSet.IsEmpty()) {
      // fall back to ISO-8859-1, see bug 118404
      characterSet = NS_LITERAL_CSTRING("ISO-8859-1");
    }
    
    nsCOMPtr<nsICharsetConverterManager> charsetConv =
      do_GetService(kCharsetConverterManagerCID, &rv);

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

      rv = unicodeDecoder->GetMaxLength(NS_REINTERPRET_CAST(const char*, string), stringLen, &unicodeLength);
      if (NS_SUCCEEDED(rv)) {
        nsString tempStr;
        tempStr.SetLength(unicodeLength);
        PRUnichar *ustr;
        tempStr.BeginWriting(ustr);
        
        PRInt32 consumedLength = 0;
        PRInt32 originalLength = stringLen;
        PRInt32 convertedLength = 0;
        PRInt32 bufferLength = unicodeLength;
        do {
          rv = unicodeDecoder->Convert(NS_REINTERPRET_CAST(const char*, string), (PRInt32 *) &stringLen, ustr,
                                       &unicodeLength);
          if (NS_FAILED(rv)) {
            // if we failed, we consume one byte, replace it with U+FFFD
            // and try the conversion again.
            ustr[unicodeLength++] = (PRUnichar)0xFFFD;
            ustr += unicodeLength;

            unicodeDecoder->Reset();
          }
          string += ++stringLen;
          consumedLength += stringLen;
          stringLen = originalLength - consumedLength;
          convertedLength += unicodeLength;
          unicodeLength = bufferLength - convertedLength;
        } while (NS_FAILED(rv) && (originalLength > consumedLength) && (bufferLength > convertedLength));
        tempStr.SetLength(convertedLength);
        request->mScriptText = tempStr;
      }
    }

    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Could not convert external JavaScript to Unicode!");
    if (NS_FAILED(rv)) {
      mPendingRequests.RemoveObject(request);
      FireScriptAvailable(rv, request, EmptyString());
      ProcessPendingReqests();
      return NS_OK;
    }

    //-- Merge the principal of the script file with that of the document
    if (channel) {
      nsCOMPtr<nsISupports> owner;
      channel->GetOwner(getter_AddRefs(owner));
      nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(owner);

      if (principal) {
        nsIPrincipal *docPrincipal = mDocument->GetPrincipal();
        if (docPrincipal) {
          nsCOMPtr<nsIPrincipal> newPrincipal =
              IntersectPrincipalCerts(docPrincipal, principal);

          mDocument->SetPrincipal(newPrincipal);
        } else {
          mPendingRequests.RemoveObject(request);
          FireScriptAvailable(rv, request, EmptyString());
          ProcessPendingReqests();
          return NS_OK;
        }
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

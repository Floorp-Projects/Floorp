/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Vidur Apparao <vidur@netscape.com> (original author)
 */

#include "nsScriptLoader.h"
#include "nsIDOMCharacterData.h"
#include "nsParserUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsIUnicodeDecoder.h"
#include "nsICharsetAlias.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsNetUtil.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsINodeInfo.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsIHttpChannel.h"
#include "nsIScriptElement.h"
#include "nsIDocShell.h"
#include "jsapi.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////

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
  PRBool mLoading;             // Are we still waiting for a load to complete?
  PRBool mWasPending;          // Processed immediately or pending
  PRBool mIsInline;            // Is the script inline or loaded?
  //  nsSharableString mScriptText;  // Holds script for loaded scripts
  nsString mScriptText;
  const char* mJSVersion;      // We don't own this string
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
  NS_INIT_ISUPPORTS();
}

nsScriptLoadRequest::~nsScriptLoadRequest()
{
}

NS_IMPL_ISUPPORTS0(nsScriptLoadRequest)

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
  : mDocument(nsnull), mSuspendCount(0)
{
  NS_INIT_ISUPPORTS();
}

nsScriptLoader::~nsScriptLoader()
{
  mObservers.Clear();
  
  PRUint32 i, count;
  mPendingRequests.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> sup(mPendingRequests.ElementAt(i));
    if (sup) {
      nsScriptLoadRequest* req = NS_REINTERPRET_CAST(nsScriptLoadRequest*, sup.get());
      req->FireScriptAvailable(NS_ERROR_ABORT, NS_LITERAL_STRING(""));
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
  
  mObservers.AppendElement(aObserver);
  
  return NS_OK;
}

/* void removeObserver (in nsIScriptLoaderObserver aObserver); */
NS_IMETHODIMP 
nsScriptLoader::RemoveObserver(nsIScriptLoaderObserver *aObserver)
{
  NS_ENSURE_ARG(aObserver);
  
  mObservers.RemoveElement(aObserver, 0);

  return NS_OK;
}

static void SplitMimeType(const nsString& aValue, nsString& aType, nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  PRInt32 semiIndex = aValue.FindChar(PRUnichar(';'));
  if (-1 != semiIndex) {
    aValue.Left(aType, semiIndex);
    aValue.Right(aParams, (aValue.Length() - semiIndex) - 1);
    aParams.StripWhitespace();
  }
  else {
    aType = aValue;
  }
  aType.StripWhitespace();
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
    
    nsCOMPtr<nsINodeInfo> nodeInfo;
    content->GetNodeInfo(*getter_AddRefs(nodeInfo));
    NS_ASSERTION(nodeInfo, "element without node info");

    if (nodeInfo) {
      nsCOMPtr<nsIAtom> localName;
      nodeInfo->GetNameAtom(*getter_AddRefs(localName));
      
      // XXX noframes and noembed are currently unconditionally not
      // displayed and processed. This might change if we support either
      // prefs or per-document container settings for not allowing
      // frames or plugins.
      if (content->IsContentOfType(nsIContent::eHTML) &&
          ((localName.get() == nsHTMLAtoms::iframe) ||
           (localName.get() == nsHTMLAtoms::noframes) ||
           (localName.get() == nsHTMLAtoms::noembed))) {
        return PR_TRUE;
      }
    }

    node = parent;
    node->GetParentNode(getter_AddRefs(parent));
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
  if (mSuspendCount || InNonScriptingContainer(aElement)) {
    return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement, aObserver);
  }

  // See if script evaluation is enabled.
  PRBool scriptsEnabled = PR_TRUE;
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
  if (globalObject)
  {
    nsCOMPtr<nsIScriptContext> context;
    if (NS_SUCCEEDED(globalObject->GetContext(getter_AddRefs(context)))
        && context)
      context->GetScriptsEnabled(&scriptsEnabled);
  }

  // If scripts aren't enabled there's no point in going on.
  if (!scriptsEnabled) {
    return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement, aObserver);
  }

  PRBool isJavaScript = PR_TRUE;
  const char* jsVersionString = nsnull;
  nsAutoString src, type, language;

  // Check the type attribute to determine language and version.
  aElement->GetType(type);
  if (!type.IsEmpty()) {
    nsAutoString  mimeType;
    nsAutoString  params;
    SplitMimeType(type, mimeType, params);

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

  // Check the language attribute.
  aElement->GetAttribute(NS_LITERAL_STRING("language"), language);
  if (!language.IsEmpty()) {
    isJavaScript = nsParserUtils::IsJavaScriptLanguage(language, &jsVersionString);
  }

  // If this isn't JavaScript, we don't know how to evaluate.
  // XXX How and where should we deal with other scripting languages??
  if (!isJavaScript) {
    return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement, aObserver);
  }

  // Create a request object for this script
  nsScriptLoadRequest* request = new nsScriptLoadRequest(aElement, aObserver, jsVersionString);
  if (!request) {
    return FireErrorNotification(NS_ERROR_OUT_OF_MEMORY, aElement, aObserver);
  }
  // Temporarily hold on to the request
  nsCOMPtr<nsISupports> reqsup(request);

  // Check to see if we have a src attribute.
  aElement->GetSrc(src);
  if (!src.IsEmpty()) {
    nsCOMPtr<nsIURI> baseURI, scriptURI;
    
    // Use the SRC attribute value to load the URL
    mDocument->GetBaseURL(*getter_AddRefs(baseURI));
    rv = NS_NewURI(getter_AddRefs(scriptURI), src, baseURI);
    if (NS_FAILED(rv)) {
      return FireErrorNotification(rv, aElement, aObserver);
    }
    
    // Check that the containing page is allowed to load this URI.
    nsCOMPtr<nsIScriptSecurityManager> securityManager(do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) {
      return FireErrorNotification(rv, aElement, aObserver);
    }
    rv = securityManager->CheckLoadURI(baseURI, scriptURI, 
                                       nsIScriptSecurityManager::ALLOW_CHROME);
    if (NS_FAILED(rv)) {
      return FireErrorNotification(rv, aElement, aObserver);
    }
    
    // After the security manager, the content-policy stuff gets a veto
    if (globalObject) {
      nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(globalObject));

      PRBool shouldLoad = PR_TRUE;
      rv = NS_CheckContentLoadPolicy(nsIContentPolicy::SCRIPT,
                                     scriptURI, aElement, domWin, &shouldLoad);
      if (NS_SUCCEEDED(rv) && !shouldLoad) {
        return FireErrorNotification(NS_ERROR_NOT_AVAILABLE, aElement, aObserver);
      }

      request->mURI = scriptURI;
      request->mIsInline = PR_FALSE;
      request->mWasPending = PR_TRUE;
      request->mLoading = PR_TRUE;

      // Add the request to our pending requests list
      mPendingRequests.AppendElement(reqsup);

      nsCOMPtr<nsILoadGroup> loadGroup;
      nsCOMPtr<nsIStreamLoader> loader;

      (void) mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));

      nsCOMPtr<nsIDocShell> docshell;
      rv = globalObject->GetDocShell(getter_AddRefs(docshell));
      if (NS_FAILED(rv)) {
        mPendingRequests.RemoveElement(reqsup, 0);
        return FireErrorNotification(rv, aElement, aObserver);
      }

      nsCOMPtr<nsIInterfaceRequestor> prompter(do_QueryInterface(docshell));

      rv = NS_NewStreamLoader(getter_AddRefs(loader), scriptURI, this,
                              reqsup, loadGroup, prompter,
                              nsIChannel::LOAD_NORMAL);
      if (NS_FAILED(rv)) {
        mPendingRequests.RemoveElement(reqsup, 0);
        return FireErrorNotification(rv, aElement, aObserver);
      }
    }
    
  } else {
    request->mLoading = PR_FALSE;
    request->mIsInline = PR_TRUE;
    mDocument->GetDocumentURL(getter_AddRefs(request->mURI));

    nsCOMPtr<nsIScriptElement> scriptElement(do_QueryInterface(aElement));
    if (scriptElement) {
      PRUint32 lineNumber;
      scriptElement->GetLineNumber(&lineNumber);
      request->mLineNo = lineNumber;
    }

    PRUint32 pendingRequestCount;
    mPendingRequests.Count(&pendingRequestCount);

    // If we've got existing pending requests, add ourselves
    // to this list.
    if (pendingRequestCount) {
      request->mWasPending = PR_TRUE;
      mPendingRequests.AppendElement(reqsup);
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
  PRUint32 i, count;
  
  mObservers.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> sup(dont_AddRef(mObservers.ElementAt(i)));
    nsCOMPtr<nsIScriptLoaderObserver> observer(do_QueryInterface(sup));

    if (observer) {
      observer->ScriptAvailable(aResult, aElement, 
                                PR_TRUE, PR_FALSE,
                                nsnull, 0,
                                NS_LITERAL_STRING(""));
    }
  }

  if (aObserver) {
    aObserver->ScriptAvailable(aResult, aElement, 
                               PR_TRUE, PR_FALSE,
                               nsnull, 0,
                               NS_LITERAL_STRING(""));
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
  PRUint32 i, count;
  
  mObservers.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> sup(dont_AddRef(mObservers.ElementAt(i)));
    nsCOMPtr<nsIScriptLoaderObserver> observer(do_QueryInterface(sup));

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
  PRUint32 i, count;
  
  mObservers.Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> sup(dont_AddRef(mObservers.ElementAt(i)));
    nsCOMPtr<nsIScriptLoaderObserver> observer(do_QueryInterface(sup));

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

  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
  NS_ENSURE_TRUE(globalObject, NS_ERROR_FAILURE);

  nsCOMPtr<nsIScriptContext> context;
  rv = globalObject->GetContext(getter_AddRefs(context));
  if (NS_FAILED(rv) || !context) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrincipal> principal;
  mDocument->GetPrincipal(getter_AddRefs(principal));
  // We can survive without a principal, but we really should
  // have one.
  NS_ASSERTION(principal, "principal required for document");

  nsAutoString ret;
  nsXPIDLCString url;

  if (aRequest->mURI) {
    rv = aRequest->mURI->GetSpec(getter_Copies(url));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  
  context->SetProcessingScriptTag(PR_TRUE);

  PRBool isUndefined;
  context->EvaluateString(aScript, nsnull, principal, url,
                          aRequest->mLineNo, aRequest->mJSVersion, 
                          ret, &isUndefined);  

  context->SetProcessingScriptTag(PR_FALSE);

  return rv;
}

void
nsScriptLoader::ProcessPendingReqests()
{
  nsCOMPtr<nsISupports> reqsup(dont_AddRef(mPendingRequests.ElementAt(0)));
  nsScriptLoadRequest* request = NS_REINTERPRET_CAST(nsScriptLoadRequest*, 
                                                     reqsup.get());
  while (request && !request->mLoading) {
    mPendingRequests.RemoveElement(reqsup, 0);
    ProcessRequest(request);
    reqsup = dont_AddRef(mPendingRequests.ElementAt(0));
    request = NS_REINTERPRET_CAST(nsScriptLoadRequest*, reqsup.get());
  }
}

NS_IMETHODIMP
nsScriptLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                 nsISupports* aContext,
                                 nsresult aStatus,
                                 PRUint32 stringLen,
                                 const char* string)
{
  nsresult rv;
  nsScriptLoadRequest* request = NS_REINTERPRET_CAST(nsScriptLoadRequest*, aContext);
  NS_ASSERTION(request, "null request in stream complete handler");
  if (!request) {
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(aStatus)) {
    mPendingRequests.RemoveElement(aContext, 0);
    FireScriptAvailable(aStatus, request, NS_LITERAL_STRING(""));
    ProcessPendingReqests();
    return NS_OK;
  }

  // If we don't have a document, then we need to abort further
  // evaluation.
  if (!mDocument) {
    mPendingRequests.RemoveElement(aContext, 0);
    FireScriptAvailable(NS_ERROR_NOT_AVAILABLE, request, 
                        NS_LITERAL_STRING(""));
    ProcessPendingReqests();
    return NS_OK;
  }

  if (stringLen) {
    nsAutoString characterSet, preferred;
    nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;
    nsCOMPtr<nsIHttpChannel> httpChannel;

    nsCOMPtr<nsIChannel> channel;
    nsCOMPtr<nsIRequest> req;
    rv = aLoader->GetRequest(getter_AddRefs(req));    
    NS_ASSERTION(req, "StreamLoader's request went away prematurely");
    if (NS_FAILED(rv)) return rv;

    channel = do_QueryInterface(req);

    httpChannel = do_QueryInterface(channel);
    if (httpChannel) {
      nsXPIDLCString charsetheader;
      rv = httpChannel->GetCharset(getter_Copies(charsetheader));
    
      if (NS_SUCCEEDED(rv)) {
        characterSet = NS_ConvertASCIItoUCS2(charsetheader);      
        nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID,&rv));

        if(NS_SUCCEEDED(rv) && calias) {
          rv = calias->GetPreferred(characterSet, preferred);

          if(NS_SUCCEEDED(rv)) {
            characterSet = preferred;
          }
        }
      }
    }

    if (NS_FAILED(rv) || characterSet.IsEmpty()) {
      nsAutoString charset;
      // Check the charset attribute to determine script charset.
      request->mElement->GetCharset(charset);
      if (!charset.IsEmpty()) {
        // Get the preferred charset from charset alias service if there
        // is one.
        nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID,&rv));
        if (NS_SUCCEEDED(rv)) {
          rv = calias->GetPreferred(charset, preferred);
          
          if(NS_SUCCEEDED(rv)) {
            characterSet = preferred;
          }
        }
      }
    }

    if (NS_FAILED(rv) || characterSet.IsEmpty()) {
      // charset from document default
      rv = mDocument->GetDocumentCharacterSet(characterSet);
    }

    NS_ASSERTION(NS_SUCCEEDED(rv), "Could not get document charset!");

    nsCOMPtr<nsICharsetConverterManager> charsetConv =
      do_GetService(kCharsetConverterManagerCID, &rv);

    if (NS_SUCCEEDED(rv) && charsetConv) {
      rv = charsetConv->GetUnicodeDecoder(&characterSet,
                                          getter_AddRefs(unicodeDecoder));
    }

    // converts from the charset to unicode
    if (NS_SUCCEEDED(rv)) {
      PRInt32 unicodeLength = 0;

      rv = unicodeDecoder->GetMaxLength(string, stringLen, &unicodeLength);
      if (NS_SUCCEEDED(rv)) {
        typedef nsSharedBufferHandle<PRUnichar>* HandlePtr;
        typedef nsAString* StrPtr;
        HandlePtr handle = NS_AllocateContiguousHandleWithData(HandlePtr(0), NS_STATIC_CAST(PRUint32, unicodeLength+1), StrPtr(0));
        PRUnichar *ustr = (PRUnichar *)handle->DataStart();
        
        PRInt32 consumedLength = 0;
        PRInt32 originalLength = stringLen;
        PRInt32 convertedLength = 0;
        PRInt32 bufferLength = unicodeLength;
        do {
          rv = unicodeDecoder->Convert(string, (PRInt32 *) &stringLen, ustr,
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
        handle->DataEnd(handle->DataStart() + convertedLength);
        nsSharableString tempStr(handle);
        request->mScriptText = tempStr;
      }
    }

    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "Could not convert external JavaScript to Unicode!");
    if (NS_FAILED(rv)) {
      mPendingRequests.RemoveElement(aContext, 0);
      FireScriptAvailable(rv, request, NS_LITERAL_STRING(""));
      ProcessPendingReqests();
      return NS_OK;
    }

    //-- Merge the principal of the script file with that of the document
    if (channel) {
      nsCOMPtr<nsISupports> owner;
      channel->GetOwner(getter_AddRefs(owner));
      nsCOMPtr<nsIPrincipal> prin;
      
      if (owner) {
        prin = do_QueryInterface(owner, &rv);
      }
      
      rv = mDocument->AddPrincipal(prin);
      if (NS_FAILED(rv)) {
        mPendingRequests.RemoveElement(aContext, 0);
        FireScriptAvailable(rv, request, NS_LITERAL_STRING(""));
        ProcessPendingReqests();
        return NS_OK;
      }
    }
  }


  // If we're not the first in the pending list, we mark ourselves
  // as loaded and just stay on the list.
  nsCOMPtr<nsISupports> first(dont_AddRef(mPendingRequests.ElementAt(0)));
  if (first != aContext) {
    request->mLoading = PR_FALSE;
    return NS_OK;
  }

  mPendingRequests.RemoveElement(aContext, 0);
  ProcessRequest(request);

  // Process any pending requests
  ProcessPendingReqests();

  return NS_OK;
}

NS_IMETHODIMP
nsScriptLoader::Suspend()
{
  mSuspendCount++;

  return NS_OK;
}

NS_IMETHODIMP
nsScriptLoader::Resume()
{
  NS_ASSERTION((mSuspendCount > 0), "nsScriptLoader call to resume() unbalanced");
  mSuspendCount--;

  return NS_OK;
}

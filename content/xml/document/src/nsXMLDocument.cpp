/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsXMLDocument.h"
#include "nsParserCIID.h"
#include "nsIParser.h"
#include "nsIXMLContent.h"
#include "nsIXMLContentSink.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h" 
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIDocumentLoader.h"
#include "nsIHTMLContent.h"
#include "nsHTMLParts.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIStyleSet.h"
#include "nsIComponentManager.h"
#include "nsIDOMComment.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIBaseWindow.h"
#include "nsIDOMWindow.h"
#include "nsIDOMCDATASection.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMDocumentType.h"
#include "nsINameSpaceManager.h"
#include "nsICSSLoader.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIHttpChannel.h"
#include "nsIURI.h"
#include "nsIServiceManager.h"
#include "nsICharsetAlias.h"
#include "nsICharsetAlias.h"
#include "nsNetUtil.h"
#include "nsDOMError.h"
#include "nsScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsIAggregatePrincipal.h"
#include "nsLayoutCID.h"
#include "nsDOMAttribute.h"
#include "nsGUIEvent.h"
#include "nsIFIXptr.h"
#include "nsIXPointer.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIEventListenerManager.h"
#include "nsContentUtils.h"
#include "nsIElementFactory.h"
#include "nsCRT.h"
#include "nsIWindowWatcher.h"
#include "nsIAuthPrompt.h"
#include "nsIScriptGlobalObjectOwner.h"
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

// XXX The XML world depends on the html atoms
#include "nsHTMLAtoms.h"

static const char kLoadAsData[] = "loadAsData";

static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);


// ==================================================================
// =
// ==================================================================


nsresult
NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                  const nsAString& aNamespaceURI, 
                  const nsAString& aQualifiedName, 
                  nsIDOMDocumentType* aDoctype,
                  nsIURI* aBaseURI)
{
  nsresult rv;

  *aInstancePtrResult = nsnull;

  nsXMLDocument* doc = new nsXMLDocument();
  if (!doc)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = doc->Init();

  if (NS_FAILED(rv)) {
    delete doc;

    return rv;
  }

  nsCOMPtr<nsIDOMDocument> kungFuDeathGrip(doc);

  doc->SetDocumentURL(aBaseURI);
  doc->SetBaseURL(aBaseURI);

  if (aDoctype) {
    nsCOMPtr<nsIDOMNode> tmpNode;
    rv = doc->AppendChild(aDoctype, getter_AddRefs(tmpNode));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  if (!aQualifiedName.IsEmpty()) {
    nsCOMPtr<nsIDOMElement> root;
    rv = doc->CreateElementNS(aNamespaceURI, aQualifiedName,
                              getter_AddRefs(root));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> tmpNode;

    rv = doc->AppendChild(root, getter_AddRefs(tmpNode));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aInstancePtrResult = doc;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsresult
NS_NewXMLDocument(nsIDocument** aInstancePtrResult)
{
  nsXMLDocument* doc = new nsXMLDocument();
  NS_ENSURE_TRUE(doc, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    delete doc;

    return rv;
  }

  *aInstancePtrResult = doc;
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsXMLDocument::nsXMLDocument() 
  : mAsync(PR_TRUE)
{

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.
}

nsXMLDocument::~nsXMLDocument()
{
  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
  }
  if (mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
  }
  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();
  }

  // XXX We rather crash than hang
  mLoopingForSyncLoad = PR_FALSE;
}


// QueryInterface implementation for nsXMLDocument
NS_INTERFACE_MAP_BEGIN(nsXMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLContentContainer)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIHttpEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXMLDocument)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XMLDocument)
NS_INTERFACE_MAP_END_INHERITING(nsDocument)


NS_IMPL_ADDREF_INHERITED(nsXMLDocument, nsDocument)
NS_IMPL_RELEASE_INHERITED(nsXMLDocument, nsDocument)


nsresult
nsXMLDocument::Init()
{
  nsresult rv = nsDocument::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mEventQService = do_GetService(kEventQueueServiceCID, &rv);

  return rv;
}

NS_IMETHODIMP 
nsXMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsresult result = nsDocument::Reset(aChannel, aLoadGroup);
  if (NS_FAILED(result))
    return result;
  nsCOMPtr<nsIURI> url;
  if (aChannel) {
    result = aChannel->GetURI(getter_AddRefs(url));
    if (NS_FAILED(result))
      return result;
  }

  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
  }
  if (mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
  }

  result = SetDefaultStylesheets(url);

  mBaseTarget.Truncate();

  return result;
}

/////////////////////////////////////////////////////
// nsIInterfaceRequestor methods:
//
NS_IMETHODIMP
nsXMLDocument::GetInterface(const nsIID& aIID, void** aSink)
{
  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    NS_ENSURE_ARG_POINTER(aSink);
    *aSink = nsnull;

    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> ww(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIAuthPrompt> prompt;
    rv = ww->GetNewAuthPrompter(nsnull, getter_AddRefs(prompt));
    if (NS_FAILED(rv))
      return rv;

    nsIAuthPrompt *p = prompt.get();
    NS_ADDREF(p);
    *aSink = p;
    return NS_OK;
  }


  return QueryInterface(aIID, aSink);
}

// nsIHttpEventSink
NS_IMETHODIMP
nsXMLDocument::OnRedirect(nsIHttpChannel *aHttpChannel, nsIChannel *aNewChannel)
{
  NS_ENSURE_ARG_POINTER(aNewChannel);

  nsresult rv;

  nsCOMPtr<nsIScriptSecurityManager> secMan = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  nsCOMPtr<nsIURI> newLocation;
  rv = aNewChannel->GetURI(getter_AddRefs(newLocation)); // The redirected URI
  if (NS_FAILED(rv)) 
    return rv;

  if (mScriptContext && !mCrossSiteAccessEnabled) {
    nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", & rv));
    if (NS_FAILED(rv))
      return rv;

    JSContext *cx = (JSContext *)mScriptContext->GetNativeContext();
    if (!cx)
      return NS_ERROR_UNEXPECTED;

    stack->Push(cx);

    rv = secMan->CheckSameOrigin(nsnull, newLocation);

    stack->Pop(&cx);
  
    if (NS_FAILED(rv))
      return rv;
  }

  nsCOMPtr<nsIPrincipal> newCodebase;
  rv = secMan->GetCodebasePrincipal(newLocation,
                                    getter_AddRefs(newCodebase));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAggregatePrincipal> agg = do_QueryInterface(mPrincipal, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Principal not an aggregate.");

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  rv = agg->SetCodebase(newCodebase);

  return rv;
}

NS_IMETHODIMP
nsXMLDocument::EvaluateFIXptr(const nsAString& aExpression, nsIDOMRange **aRange)
{
  nsresult rv;
  nsCOMPtr<nsIFIXptrEvaluator> e =
    do_CreateInstance("@mozilla.org/xmlextras/fixptrevaluator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return e->Evaluate(this, aExpression, aRange);
}

NS_IMETHODIMP
nsXMLDocument::EvaluateXPointer(const nsAString& aExpression,
                                nsIXPointerResult **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIXPointerEvaluator> e =
    do_CreateInstance("@mozilla.org/xmlextras/xpointerevaluator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return e->Evaluate(this, aExpression, aResult);
}

NS_IMETHODIMP
nsXMLDocument::GetAsync(PRBool *aAsync)
{
  NS_ENSURE_ARG_POINTER(aAsync);
  *aAsync = mAsync;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocument::SetAsync(PRBool aAsync)
{
  mAsync = aAsync;
  return NS_OK;
}

nsresult 
nsXMLDocument::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = nsnull;

  if (mScriptContext) {
    nsCOMPtr<nsIScriptGlobalObject> global;
    mScriptContext->GetGlobalObject(getter_AddRefs(global));
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(global);
    if (window) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      window->GetDocument(getter_AddRefs(domdoc));
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
      if (doc) {
        doc->GetDocumentLoadGroup(aLoadGroup);
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXMLDocument::Load(const nsAString& aUrl, PRBool *aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = PR_FALSE;

  nsCOMPtr<nsIChannel> channel;
  nsCOMPtr<nsIURI> uri;
  nsresult rv;
  
  // Create a new URI
  rv = NS_NewURI(getter_AddRefs(uri), aUrl, nsnull, mDocumentURL);
  if (NS_FAILED(rv)) return rv;

  // Get security manager, check to see if we're allowed to load this URI
  nsCOMPtr<nsIScriptSecurityManager> secMan = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = secMan->CheckConnect(nsnull, uri, "XMLDocument", "load");
  if (NS_FAILED(rv)) {
    // We need to return success here so that JS will get a proper exception
    // thrown later. Native calls should always result in CheckConnect() succeeding,
    // but in case JS calls C++ which calls this code the exception might be lost.
    return NS_OK;
  }

  // Partial Reset, need to restore principal for security reasons and
  // event listener manager so that load listeners etc. will remain.

  nsCOMPtr<nsIPrincipal> principal(mPrincipal);
  nsCOMPtr<nsIEventListenerManager> elm(mListenerManager);

  Reset(nsnull, nsnull);
  
  mPrincipal = principal;
  mListenerManager = elm;
  
  SetDocumentURL(uri);
  SetBaseURL(uri);

  // Store script context, if any, in case we encounter redirect
  // (because we need it there)
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (stack) {
    JSContext *cx;
    if (NS_SUCCEEDED(stack->Peek(&cx)) && cx) {
      nsContentUtils::GetDynamicScriptContext(cx,
                                              getter_AddRefs(mScriptContext));
    }
  }

  // Find out if UniversalBrowserRead privileges are enabled - we will
  // need this in case of a redirect
  PRBool crossSiteAccessEnabled;
  rv = secMan->IsCapabilityEnabled("UniversalBrowserRead",
                                   &crossSiteAccessEnabled);
  if (NS_FAILED(rv)) return rv;

  mCrossSiteAccessEnabled = crossSiteAccessEnabled;

  // Create a channel
  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.
  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(getter_AddRefs(loadGroup));

  // nsIRequest::LOAD_BACKGROUND prevents throbber from becoming active,
  // which in turn keeps STOP button from becoming active  
  rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull, loadGroup, this, 
                     nsIRequest::LOAD_BACKGROUND);
  if (NS_FAILED(rv)) return rv;

  // Set a principal for this document
  nsCOMPtr<nsISupports> channelOwner;
  rv = channel->GetOwner(getter_AddRefs(channelOwner));

  // We don't care if GetOwner() succeeded here, if it failed,
  // channelOwner will be null, which is what we want in that case.

  mPrincipal = do_QueryInterface(channelOwner);

  if (NS_FAILED(rv) || !mPrincipal) {
    rv = secMan->GetCodebasePrincipal(uri, getter_AddRefs(mPrincipal));
    NS_ENSURE_TRUE(mPrincipal, rv);
  }

  nsCOMPtr<nsIEventQueue> modalEventQueue;

  if(!mAsync) {
    NS_ENSURE_TRUE(mEventQService, NS_ERROR_FAILURE);

    rv = mEventQService->PushThreadEventQueue(getter_AddRefs(modalEventQueue));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Prepare for loading the XML document "into oneself"
  nsCOMPtr<nsIStreamListener> listener;
  if (NS_FAILED(rv = StartDocumentLoad(kLoadAsData, channel, 
                                       loadGroup, nsnull, 
                                       getter_AddRefs(listener),
                                       PR_FALSE))) {
    NS_ERROR("nsXMLDocument::Load: Failed to start the document load.");
    if (modalEventQueue) {
      mEventQService->PopThreadEventQueue(modalEventQueue);
    }
    return rv;
  }

  // Start an asynchronous read of the XML document
  rv = channel->AsyncOpen(listener, nsnull);
  if (NS_FAILED(rv)) {
    if (modalEventQueue) {
      mEventQService->PopThreadEventQueue(modalEventQueue);
    }
    return rv;
  }

  if (!mAsync) {
    mLoopingForSyncLoad = PR_TRUE;

    while (mLoopingForSyncLoad) {
      modalEventQueue->ProcessPendingEvents();
    }

    mEventQService->PopThreadEventQueue(modalEventQueue);

    // We set return to true unless there was a parsing error
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mRootContent);
    if (node) {
      nsAutoString name, ns;      
      if (NS_SUCCEEDED(node->GetLocalName(name)) &&
          name.Equals(NS_LITERAL_STRING("parsererror")) &&
          NS_SUCCEEDED(node->GetNamespaceURI(ns)) &&
          ns.Equals(NS_LITERAL_STRING("http://www.mozilla.org/newlayout/xml/parsererror.xml"))) {
        //return is already false
      } else {
        *aReturn = PR_TRUE;
      }
    }
  } else {
    *aReturn = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::StartDocumentLoad(const char* aCommand,
                                 nsIChannel* aChannel,
                                 nsILoadGroup* aLoadGroup,
                                 nsISupports* aContainer,
                                 nsIStreamListener **aDocListener,
                                 PRBool aReset,
                                 nsIContentSink* aSink)
{
  if (nsCRT::strcmp(kLoadAsData, aCommand) == 0) {
    // We need to disable script & style loading in this case.
    // We leave them disabled even in EndLoad(), and let anyone
    // who puts the document on display to worry about enabling.

    // scripts
    nsCOMPtr<nsIScriptLoader> loader;
    nsresult rv = GetScriptLoader(getter_AddRefs(loader));
    if (NS_FAILED(rv))
      return rv;
    if (loader) {
      loader->SetEnabled(PR_FALSE); // Do not load/process scripts when loading as data
    }

    // styles
    nsCOMPtr<nsICSSLoader> cssLoader;
    rv = GetCSSLoader(*getter_AddRefs(cssLoader));
    if (NS_FAILED(rv))
      return rv;
    if (cssLoader) {
      cssLoader->SetEnabled(PR_FALSE); // Do not load/process styles when loading as data
    }
  } else if (nsCRT::strcmp("loadAsInteractiveData", aCommand) == 0) {
    aCommand = kLoadAsData; // XBL, for example, needs scripts and styles
  }
  
  if (nsCRT::strcmp(aCommand, kLoadAsData) == 0) {
    mLoadedAsData = PR_TRUE;
  }

  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
                                              aChannel, aLoadGroup,
                                              aContainer, 
                                              aDocListener, aReset, aSink);
  if (NS_FAILED(rv)) return rv;

  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8"));
  PRInt32 charsetSource = kCharsetFromDocTypeDefault;

  nsCOMPtr<nsIURI> aUrl;
  rv = aChannel->GetURI(getter_AddRefs(aUrl));
  if (NS_FAILED(rv)) return rv;

  { // check channel's charset...
    nsCAutoString charsetVal;
    rv = aChannel->GetContentCharset(charsetVal);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID,&rv));

      if(NS_SUCCEEDED(rv) && (nsnull != calias) ) {
        nsCAutoString preferred;
        rv = calias->GetPreferred(charsetVal, charset);
        if(NS_SUCCEEDED(rv)){            
          charsetSource = kCharsetFromChannel;
        }
      }
    }
  } //end of checking channel's charset

  static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

  nsCOMPtr<nsIParser> parser = do_CreateInstance(kCParserCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXMLContentSink> sink;
    
  if (aSink) {
    sink = do_QueryInterface(aSink);
  }
  else {
    nsCOMPtr<nsIDocShell> docShell;
    if (aContainer) {
      docShell = do_QueryInterface(aContainer);
      NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    }
    rv = NS_NewXMLContentSink(getter_AddRefs(sink), this, aUrl, docShell,
                              aChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set the parser as the stream listener for the document loader...
  rv = CallQueryInterface(parser, aDocListener);
  NS_ENSURE_SUCCESS(rv, rv);

  SetDocumentCharacterSet(charset);
  parser->SetDocumentCharset(charset, charsetSource);
  parser->SetCommand(aCommand);
  parser->SetContentSink(sink);
  parser->Parse(aUrl, nsnull, PR_FALSE, (void *)this);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::EndLoad()
{
  mLoopingForSyncLoad = PR_FALSE;

  if (mLoadedAsData) {
    // Generate a document load event for the case when an XML document was loaded
    // as pure data without any presentation attached to it.
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_PAGE_LOAD;

    nsCOMPtr<nsIScriptGlobalObject> sgo;
    nsCOMPtr<nsIScriptGlobalObjectOwner> container =
      do_QueryReferent(mDocumentContainer);
    if (container) {
      container->GetScriptGlobalObject(getter_AddRefs(sgo));
    }

    nsCxPusher pusher(sgo);

    HandleDOMEvent(nsnull, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }    
  return nsDocument::EndLoad();  
}

NS_IMETHODIMP 
nsXMLDocument::GetAttributeStyleSheet(nsIHTMLStyleSheet** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mAttrStyleSheet;
  NS_ENSURE_TRUE(mAttrStyleSheet, NS_ERROR_NOT_AVAILABLE); // probably not the right error...

  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mInlineStyleSheet;
  NS_ENSURE_TRUE(mInlineStyleSheet, NS_ERROR_NOT_AVAILABLE); // probably not the right error...

  NS_ADDREF(*aResult);

  return NS_OK;
}

// subclass hook for sheet ordering
void
nsXMLDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags)
{
  // XXXbz this catalog stuff should be in the UA level in the cascade!
  if (aFlags & NS_STYLESHEET_FROM_CATALOG) {
    // always after other catalog sheets
    mStyleSheets.InsertObjectAt(aSheet, mCatalogSheetCount);
    ++mCatalogSheetCount;
  }
  else if (aSheet == mAttrStyleSheet) {  // always after catalog sheets
    NS_ASSERTION(mStyleSheets.Count() == 0 ||
                 mAttrStyleSheet != mStyleSheets[0],
                 "Adding attr sheet twice!");
    mStyleSheets.InsertObjectAt(aSheet, mCatalogSheetCount);
  }
  else if (aSheet == mInlineStyleSheet) {  // always last
    NS_ASSERTION(mStyleSheets.Count() == 0 ||
                 mStyleSheets[mStyleSheets.Count() - 1] != mInlineStyleSheet,
                 "Adding style attr sheet twice!");
    mStyleSheets.AppendObject(aSheet);
  }
  else {
    PRInt32 count = mStyleSheets.Count();
    if (count != 0 && mInlineStyleSheet == mStyleSheets[count - 1]) {
      // keep attr sheet last
      mStyleSheets.InsertObjectAt(aSheet, count - 1);
    }
    else {
      mStyleSheets.AppendObject(aSheet);
    }
  }
}

void
nsXMLDocument::InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{
  NS_ASSERTION(0 <= aIndex &&
               aIndex <= (
                          mStyleSheets.Count()
                          /* Don't count Attribute stylesheet */
                          - 1
                          /* Don't count catalog sheets */
                          - mCatalogSheetCount
                          /* No insertion allowed after StyleAttr stylesheet */
                          - (mInlineStyleSheet ? 1: 0)
                          ),
               "index out of bounds");
  // offset w.r.t. catalog style sheets and the attr style sheet
  mStyleSheets.InsertObjectAt(aSheet, aIndex + mCatalogSheetCount + 1);
}

already_AddRefed<nsIStyleSheet>
nsXMLDocument::InternalGetStyleSheetAt(PRInt32 aIndex)
{
  PRInt32 count = InternalGetNumberOfStyleSheets();

  if (aIndex >= 0 && aIndex < count) {
    nsIStyleSheet* sheet = mStyleSheets[aIndex + mCatalogSheetCount + 1];
    NS_ADDREF(sheet);
    return sheet;
  } else {
    NS_ERROR("Index out of range");
    return nsnull;
  }
}

PRInt32
nsXMLDocument::InternalGetNumberOfStyleSheets()
{
  PRInt32 count = mStyleSheets.Count();

  if (count != 0 && mInlineStyleSheet == mStyleSheets[count - 1]) {
    // subtract the inline style sheet
    --count;
  }

  if (count != 0 && mAttrStyleSheet == mStyleSheets[mCatalogSheetCount]) {
    // subtract the attr sheet
    --count;
  }

  count -= mCatalogSheetCount;

  NS_ASSERTION(count >= 0, "How did we get a negative count?");

  return count;
}

// nsIDOMDocument interface
NS_IMETHODIMP    
nsXMLDocument::GetDoctype(nsIDOMDocumentType** aDocumentType)
{
  return nsDocument::GetDoctype(aDocumentType); 
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateCDATASection(const nsAString& aData,
                                  nsIDOMCDATASection** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsReadingIterator<PRUnichar> begin;
  nsReadingIterator<PRUnichar> end;
  aData.BeginReading(begin);
  aData.EndReading(end);
  if (FindInReadable(NS_LITERAL_STRING("]]>"),begin,end))
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;

  nsCOMPtr<nsIContent> content;
  nsresult rv = NS_NewXMLCDATASection(getter_AddRefs(content));

  if (NS_SUCCEEDED(rv)) {
    rv = CallQueryInterface(content, aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateEntityReference(const nsAString& aName,
                                     nsIDOMEntityReference** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = nsnull;
  return NS_OK;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateProcessingInstruction(const nsAString& aTarget, 
                                           const nsAString& aData, 
                                           nsIDOMProcessingInstruction** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsCOMPtr<nsIContent> content;
  nsresult rv = NS_NewXMLProcessingInstruction(getter_AddRefs(content),
                                               aTarget, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return CallQueryInterface(content, aReturn);
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateElement(const nsAString& aTagName, 
                              nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;
  NS_ENSURE_TRUE(!aTagName.IsEmpty(), NS_ERROR_DOM_INVALID_CHARACTER_ERR);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv;

  rv = mNodeInfoManager->GetNodeInfo(aTagName, nsnull, kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  return CreateElement(nodeInfo, aReturn);
}

NS_IMETHODIMP    
nsXMLDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv;
  nsCOMPtr<nsIDOMDocumentType> docType, newDocType;
  nsCOMPtr<nsIDOMDocument> newDoc;

  // Get the doctype prior to new document construction. There's no big 
  // advantage now to dealing with the doctype separately, but maybe one 
  // day we'll do something significant with the doctype on document creation.
  GetDoctype(getter_AddRefs(docType));
  if (docType) {
    nsCOMPtr<nsIDOMNode> newDocTypeNode;
    rv = docType->CloneNode(PR_TRUE, getter_AddRefs(newDocTypeNode));
    if (NS_FAILED(rv)) return rv;
    newDocType = do_QueryInterface(newDocTypeNode);
  }

  // Create an empty document
  nsAutoString emptyStr;
  emptyStr.Truncate();
  rv = NS_NewDOMDocument(getter_AddRefs(newDoc), emptyStr, emptyStr,
                         newDocType, mDocumentURL);
  if (NS_FAILED(rv)) return rv;

  if (aDeep) {
    // If there was a doctype, a new one has already been inserted into the
    // new document. We might have to add nodes before it.
    PRBool beforeDocType = (docType.get() != nsnull);
    nsCOMPtr<nsIDOMNodeList> childNodes;
    
    GetChildNodes(getter_AddRefs(childNodes));
    if (childNodes) {
      PRUint32 index, count;
      childNodes->GetLength(&count);
      for (index=0; index < count; index++) {
        nsCOMPtr<nsIDOMNode> child;
        childNodes->Item(index, getter_AddRefs(child));
        if (child && (child != docType)) {
          nsCOMPtr<nsIDOMNode> newChild;
          rv = child->CloneNode(aDeep, getter_AddRefs(newChild));
          if (NS_FAILED(rv)) return rv;
          
          nsCOMPtr<nsIDOMNode> dummyNode;
          if (beforeDocType) {
            rv = newDoc->InsertBefore(newChild, 
                                      docType, 
                                      getter_AddRefs(dummyNode));
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
          }
          else {
            rv = newDoc->AppendChild(newChild, 
                                     getter_AddRefs(dummyNode));
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
          }
        }
        else {
          beforeDocType = PR_FALSE;
        }
      }
    }
  }

  return CallQueryInterface(newDoc, aReturn);
}
 
NS_IMETHODIMP
nsXMLDocument::ImportNode(nsIDOMNode* aImportedNode,
                          PRBool aDeep,
                          nsIDOMNode** aReturn)
{
  return nsDocument::ImportNode(aImportedNode, aDeep, aReturn);
}

NS_IMETHODIMP
nsXMLDocument::CreateAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aQualifiedName,
                                 nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = mNodeInfoManager->GetNodeInfo(aQualifiedName, aNamespaceURI,
                                              getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  nsDOMAttribute* attribute = new nsDOMAttribute(nsnull, nodeInfo, value);
  NS_ENSURE_TRUE(attribute, NS_ERROR_OUT_OF_MEMORY); 

  return CallQueryInterface(attribute, aReturn);
}

NS_IMETHODIMP
nsXMLDocument::CreateElementNS(const nsAString& aNamespaceURI,
                               const nsAString& aQualifiedName,
                               nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsresult rv = NS_OK;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(aQualifiedName, aNamespaceURI,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  return CreateElement(nodeInfo, aReturn);
}

// Id attribute matching function used by nsXMLDocument and
// nsHTMLDocument.
nsIContent *
MatchElementId(nsIContent *aContent, const nsACString& aUTF8Id, const nsAString& aId)
{
  if (aContent->IsContentOfType(nsIContent::eHTML)) {
    if (aContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::id)) {
      nsAutoString value;
      aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value);

      if (aId.Equals(value)) {
        return aContent;
      }
    }
  }
  else if (aContent->IsContentOfType(nsIContent::eELEMENT)) {
    nsCOMPtr<nsIXMLContent> xmlContent = do_QueryInterface(aContent);    

    if (xmlContent) {
      nsCOMPtr<nsIAtom> value;
      if (NS_SUCCEEDED(xmlContent->GetID(getter_AddRefs(value))) &&
          value && value->EqualsUTF8(aUTF8Id)) {
        return aContent;
      }
    }
  }
  
  nsIContent *result = nsnull;
  PRInt32 i, count;

  aContent->ChildCount(count);
  nsCOMPtr<nsIContent> child;
  for (i = 0; i < count && result == nsnull; i++) {
    aContent->ChildAt(i, getter_AddRefs(child));
    result = MatchElementId(child, aUTF8Id, aId);
  }  

  return result;
}

NS_IMETHODIMP
nsXMLDocument::GetElementById(const nsAString& aElementId,
                              nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  NS_WARN_IF_FALSE(!aElementId.IsEmpty(), "getElementById(\"\"), fix caller?");
  if (aElementId.IsEmpty())
    return NS_OK;

  // If we tried to load a document and something went wrong, we might not have
  // root content. This can happen when you do document.load() and the document
  // to load is not XML, for example.
  if (!mRootContent)
    return NS_OK;

  // XXX For now, we do a brute force search of the content tree.
  // We should come up with a more efficient solution.
  // Note that content is *not* refcounted here, so do *not* release it!
  nsIContent *content = MatchElementId(mRootContent,
                                       NS_ConvertUCS2toUTF8(aElementId),
                                       aElementId);

  if (!content) {
    return NS_OK;
  }

  return CallQueryInterface(content, aReturn);
}

nsresult
nsXMLDocument::SetDefaultStylesheets(nsIURI* aUrl)
{
  if (!aUrl) {
    return NS_OK;
  }

  nsresult rv = NS_NewHTMLStyleSheet(getter_AddRefs(mAttrStyleSheet), aUrl,
                                     this);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewHTMLCSSStyleSheet(getter_AddRefs(mInlineStyleSheet), aUrl,
                               this);
  NS_ENSURE_SUCCESS(rv, rv);

  // tell the world about our new style sheets
  AddStyleSheet(mAttrStyleSheet, 0);
  AddStyleSheet(mInlineStyleSheet, 0);

  return rv;
}

NS_IMETHODIMP 
nsXMLDocument::SetBaseTarget(const nsAString &aBaseTarget)
{
  mBaseTarget.Assign(aBaseTarget);
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::GetBaseTarget(nsAString &aBaseTarget)
{
  aBaseTarget.Assign(mBaseTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLDocument::GetCSSLoader(nsICSSLoader*& aLoader)
{
  if (!mCSSLoader) {
    nsresult rv = NS_NewCSSLoader(this, getter_AddRefs(mCSSLoader));
    NS_ENSURE_SUCCESS(rv, rv);

    mCSSLoader->SetCaseSensitive(PR_TRUE);
    // no quirks in XML
    mCSSLoader->SetCompatibilityMode(eCompatibility_FullStandards);
  }

  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);

  return NS_OK;
}

nsresult
nsXMLDocument::CreateElement(nsINodeInfo *aNodeInfo, nsIDOMElement** aResult)
{
  *aResult = nsnull;
  
  nsresult rv;
  
  nsCOMPtr<nsIContent> content;

  PRInt32 namespaceID = aNodeInfo->GetNamespaceID();

  nsCOMPtr<nsIElementFactory> elementFactory;
  nsContentUtils::GetNSManagerWeakRef()->GetElementFactory(namespaceID,
                                                           getter_AddRefs(elementFactory));

  if (elementFactory) {
    rv = elementFactory->CreateInstanceByTag(aNodeInfo,
                                             getter_AddRefs(content));
  } else {
    rv = NS_NewXMLElement(getter_AddRefs(content), aNodeInfo);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  content->SetContentID(mNextContentID++);

  return CallQueryInterface(content, aResult);
}

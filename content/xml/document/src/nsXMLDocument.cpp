/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsHTMLParts.h"
#include "nsHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIComponentManager.h"
#include "nsIDOMComment.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsIBaseWindow.h"
#include "nsIDOMWindow.h"
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
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsLayoutCID.h"
#include "nsDOMAttribute.h"
#include "nsGUIEvent.h"
#include "nsIFIXptr.h"
#include "nsIXPointer.h"
#include "nsCExternalHandlerService.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIEventListenerManager.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsIElementFactory.h"
#include "nsCRT.h"
#include "nsIWindowWatcher.h"
#include "nsIAuthPrompt.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIJSContextStack.h"

// XXX The XML world depends on the html atoms
#include "nsHTMLAtoms.h"

static const char kLoadAsData[] = "loadAsData";

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
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

  doc->nsIDocument::SetDocumentURI(aBaseURI);
  doc->SetBaseURI(aBaseURI);

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
  // XXX We rather crash than hang
  mLoopingForSyncLoad = PR_FALSE;
}


// QueryInterface implementation for nsXMLDocument
NS_INTERFACE_MAP_BEGIN(nsXMLDocument)
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

void
nsXMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsDocument::Reset(aChannel, aLoadGroup);

  mScriptContext = nsnull;
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

  nsCOMPtr<nsIURI> newLocation;
  nsresult rv = aNewChannel->GetURI(getter_AddRefs(newLocation)); // The redirected URI
  if (NS_FAILED(rv)) 
    return rv;

  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

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

  return secMan->GetCodebasePrincipal(newLocation, getter_AddRefs(mPrincipal));
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
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(mScriptContext->GetGlobalObject());

    if (window) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      window->GetDocument(getter_AddRefs(domdoc));
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
      if (doc) {
        *aLoadGroup = doc->GetDocumentLoadGroup().get(); // already_AddRefed
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

  nsIScriptContext *callingContext = nsnull;

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (stack) {
    JSContext *cx;
    if (NS_SUCCEEDED(stack->Peek(&cx)) && cx) {
      callingContext = nsJSUtils::GetDynamicScriptContext(cx);
    }
  }

  nsIURI *baseURI = mDocumentURI;
  nsCAutoString charset;

  if (callingContext) {
    nsCOMPtr<nsIDOMWindow> window =
      do_QueryInterface(callingContext->GetGlobalObject());

    if (window) {
      nsCOMPtr<nsIDOMDocument> dom_doc;
      window->GetDocument(getter_AddRefs(dom_doc));
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(dom_doc));

      if (doc) {
        baseURI = doc->GetBaseURI();
        charset = doc->GetDocumentCharacterSet();
      }
    }
  }

  // Create a new URI
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aUrl, charset.get(), baseURI);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Partial Reset, need to restore principal for security reasons and
  // event listener manager so that load listeners etc. will
  // remain. This should be done before the security check is done to
  // ensure that the document is reset even if the new document can't
  // be loaded.
  nsCOMPtr<nsIPrincipal> principal(mPrincipal);
  nsCOMPtr<nsIEventListenerManager> elm(mListenerManager);

  ResetToURI(uri, nsnull);

  mPrincipal = principal;
  mListenerManager = elm;

  // Get security manager, check to see if we're allowed to load this URI
  nsCOMPtr<nsIScriptSecurityManager> secMan = 
           do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = secMan->CheckConnect(nsnull, uri, "XMLDocument", "load");
  if (NS_FAILED(rv)) {
    // We need to return success here so that JS will get a proper
    // exception thrown later. Native calls should always result in
    // CheckConnect() succeeding, but in case JS calls C++ which calls
    // this code the exception might be lost.
    return NS_OK;
  }

  // Store script context, if any, in case we encounter redirect
  // (because we need it there)

  mScriptContext = callingContext;

  // Find out if UniversalBrowserRead privileges are enabled - we will
  // need this in case of a redirect
  PRBool crossSiteAccessEnabled;
  rv = secMan->IsCapabilityEnabled("UniversalBrowserRead",
                                   &crossSiteAccessEnabled);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mCrossSiteAccessEnabled = crossSiteAccessEnabled;

  // Create a channel
  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.
  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(getter_AddRefs(loadGroup));

  nsCOMPtr<nsIChannel> channel;
  // nsIRequest::LOAD_BACKGROUND prevents throbber from becoming active,
  // which in turn keeps STOP button from becoming active  
  rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull, loadGroup, this, 
                     nsIRequest::LOAD_BACKGROUND);
  if (NS_FAILED(rv)) {
    return rv;
  }

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
          name.EqualsLiteral("parsererror") &&
          NS_SUCCEEDED(node->GetNamespaceURI(ns)) &&
          ns.EqualsLiteral("http://www.mozilla.org/newlayout/xml/parsererror.xml")) {
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

nsresult
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
    nsIScriptLoader *loader = GetScriptLoader();
    if (loader) {
      loader->SetEnabled(PR_FALSE); // Do not load/process scripts when loading as data
    }

    // styles
    nsICSSLoader* cssLoader = GetCSSLoader();
    if (!cssLoader)
      return NS_ERROR_OUT_OF_MEMORY;
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

  PRInt32 charsetSource = kCharsetFromDocTypeDefault;
  nsCAutoString charset(NS_LITERAL_CSTRING("UTF-8"));
  TryChannelCharset(aChannel, charsetSource, charset);

  nsCOMPtr<nsIURI> aUrl;
  rv = aChannel->GetURI(getter_AddRefs(aUrl));
  if (NS_FAILED(rv)) return rv;

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

void
nsXMLDocument::EndLoad()
{
  mLoopingForSyncLoad = PR_FALSE;

  if (mLoadedAsData) {
    // Generate a document load event for the case when an XML document was loaded
    // as pure data without any presentation attached to it.
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(NS_PAGE_LOAD);

    nsCOMPtr<nsIScriptGlobalObject> sgo;
    nsCOMPtr<nsIScriptGlobalObjectOwner> container =
      do_QueryReferent(mDocumentContainer);
    if (container) {
      container->GetScriptGlobalObject(getter_AddRefs(sgo));
    }

    nsCxPusher pusher(sgo);

    HandleDOMEvent(nsnull, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }    
  nsDocument::EndLoad();  
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
  else if (aSheet == mStyleAttrStyleSheet) {  // always last
    NS_ASSERTION(mStyleSheets.Count() == 0 ||
                 mStyleSheets[mStyleSheets.Count() - 1] != mStyleAttrStyleSheet,
                 "Adding style attr sheet twice!");
    mStyleSheets.AppendObject(aSheet);
  }
  else {
    PRInt32 count = mStyleSheets.Count();
    if (count != 0 && mStyleAttrStyleSheet == mStyleSheets[count - 1]) {
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
                          - (mStyleAttrStyleSheet ? 1: 0)
                          ),
               "index out of bounds");
  // offset w.r.t. catalog style sheets and the attr style sheet
  mStyleSheets.InsertObjectAt(aSheet, aIndex + mCatalogSheetCount + 1);
}

nsIStyleSheet*
nsXMLDocument::InternalGetStyleSheetAt(PRInt32 aIndex) const
{
  PRInt32 count = InternalGetNumberOfStyleSheets();

  if (aIndex >= 0 && aIndex < count) {
    return mStyleSheets[aIndex + mCatalogSheetCount + 1];
  } else {
    NS_ERROR("Index out of range");
    return nsnull;
  }
}

PRInt32
nsXMLDocument::InternalGetNumberOfStyleSheets() const
{
  PRInt32 count = mStyleSheets.Count();

  if (count != 0 && mStyleAttrStyleSheet == mStyleSheets[count - 1]) {
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

// nsIDOMNode interface

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
                         newDocType, mDocumentURI);
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
  PRUint32 i, count = aContent->GetChildCount();

  for (i = 0; i < count && result == nsnull; i++) {
    result = MatchElementId(aContent->GetChildAt(i), aUTF8Id, aId);
  }  

  return result;
}

// nsIDOMDocument interface

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

nsICSSLoader*
nsXMLDocument::GetCSSLoader()
{
  if (!mCSSLoader) {
    NS_NewCSSLoader(this, getter_AddRefs(mCSSLoader));
    if (mCSSLoader) {
      mCSSLoader->SetCaseSensitive(PR_TRUE);
      // no quirks in XML
      mCSSLoader->SetCompatibilityMode(eCompatibility_FullStandards);
    }
  }

  return mCSSLoader;
}

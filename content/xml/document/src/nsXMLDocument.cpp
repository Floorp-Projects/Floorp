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
#include "nsIXMLContentSink.h"
#include "nsIPresShell.h"
#include "nsPresContext.h" 
#include "nsIContent.h"
#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsIMarkupDocumentViewer.h"
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
#include "nsThreadUtils.h"
#include "nsJSUtils.h"
#include "nsCRT.h"
#include "nsIWindowWatcher.h"
#include "nsIAuthPrompt.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIJSContextStack.h"
#include "nsContentCreatorFunctions.h"
#include "nsIDOMUserDataHandler.h"
#include "nsEventDispatcher.h"
#include "nsNodeUtils.h"


// ==================================================================
// =
// ==================================================================


nsresult
NS_NewDOMDocument(nsIDOMDocument** aInstancePtrResult,
                  const nsAString& aNamespaceURI, 
                  const nsAString& aQualifiedName, 
                  nsIDOMDocumentType* aDoctype,
                  nsIURI* aDocumentURI,
                  nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal)
{
  // Note: can't require that aDocumentURI/aBaseURI/aPrincipal be non-null,
  // since at least one caller (XMLHttpRequest) doesn't have decent args to
  // pass in.
  
  nsresult rv;

  *aInstancePtrResult = nsnull;

  nsRefPtr<nsXMLDocument> doc = new nsXMLDocument();
  if (!doc)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = doc->Init();

  if (NS_FAILED(rv)) {
    return rv;
  }

  doc->nsDocument::SetDocumentURI(aDocumentURI);
  // Must set the principal first, since SetBaseURI checks it.
  doc->SetPrincipal(aPrincipal);
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

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
  }

  *aInstancePtrResult = doc;

  return rv;
}

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.

nsXMLDocument::nsXMLDocument(const char* aContentType)
  : nsDocument(aContentType),
    mAsync(PR_TRUE)
{

  // NOTE! nsDocument::operator new() zeroes out all members, so don't
  // bother initializing members to 0.
}

nsXMLDocument::~nsXMLDocument()
{
  // XXX We rather crash than hang
  mLoopingForSyncLoad = PR_FALSE;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLDocument)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXMLDocument, nsDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// QueryInterface implementation for nsXMLDocument
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
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

  return rv;
}

void
nsXMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsDocument::Reset(aChannel, aLoadGroup);

  mScriptContext = nsnull;
}

void
nsXMLDocument::ResetToURI(nsIURI *aURI, nsILoadGroup *aLoadGroup,
                          nsIPrincipal* aPrincipal)
{
  if (mChannelIsPending) {
    StopDocumentLoad();
    mChannel->Cancel(NS_BINDING_ABORTED);
    mChannelIsPending = nsnull;
  }
  
  nsDocument::ResetToURI(aURI, aLoadGroup, aPrincipal);
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

// nsIChannelEventSink
NS_IMETHODIMP
nsXMLDocument::OnChannelRedirect(nsIChannel *aOldChannel,
                                 nsIChannel *aNewChannel,
                                 PRUint32 aFlags)
{
  NS_PRECONDITION(aNewChannel, "Redirecting to null channel?");

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
  
    if (NS_FAILED(rv)) {
      // The security manager set a pending exception.  Since we're
      // running under the event loop, we need to report it.
      ::JS_ReportPendingException(cx);
      return rv;
    }
  }

  // XXXbz Shouldn't we look at the owner on the new channel at some point?
  // It's not gonna be right here, but eventually it will....
  nsCOMPtr<nsIPrincipal> principal;
  rv = secMan->GetCodebasePrincipal(newLocation, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  SetPrincipal(principal);
  return NS_OK;
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
  // be loaded.  Note that we need to hold a strong ref to |principal|
  // here, because ResetToURI will null out our node principal before
  // setting the new one.
  nsCOMPtr<nsIPrincipal> principal = NodePrincipal();
  nsCOMPtr<nsIEventListenerManager> elm(mListenerManager);
  mListenerManager = nsnull;

  ResetToURI(uri, nsnull, principal);

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
  // XXXbz StartDocumentLoad should handle that.... And we shouldn't be calling
  // StartDocumentLoad until we get an OnStartRequest from this channel!
  nsCOMPtr<nsISupports> channelOwner;
  rv = channel->GetOwner(getter_AddRefs(channelOwner));

  // We don't care if GetOwner() succeeded here, if it failed,
  // channelOwner will be null, which is what we want in that case.
  principal = do_QueryInterface(channelOwner);

  if (NS_FAILED(rv) || !principal) {
    rv = secMan->GetCodebasePrincipal(uri, getter_AddRefs(principal));
    NS_ENSURE_TRUE(principal, rv);
  }

  SetPrincipal(principal);

  // Prepare for loading the XML document "into oneself"
  nsCOMPtr<nsIStreamListener> listener;
  if (NS_FAILED(rv = StartDocumentLoad(kLoadAsData, channel, 
                                       loadGroup, nsnull, 
                                       getter_AddRefs(listener),
                                       PR_FALSE))) {
    NS_ERROR("nsXMLDocument::Load: Failed to start the document load.");
    return rv;
  }

  // After this point, if we error out of this method we should clear
  // mChannelIsPending.

  // Start an asynchronous read of the XML document
  rv = channel->AsyncOpen(listener, nsnull);
  if (NS_FAILED(rv)) {
    mChannelIsPending = PR_FALSE;
    return rv;
  }

  if (!mAsync) {
    nsCOMPtr<nsIThread> thread = do_GetCurrentThread();

    mLoopingForSyncLoad = PR_TRUE;
    while (mLoopingForSyncLoad) {
      if (!NS_ProcessNextEvent(thread))
        break;
    }

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
    mLoadedAsData = PR_TRUE;
    // We need to disable script & style loading in this case.
    // We leave them disabled even in EndLoad(), and let anyone
    // who puts the document on display to worry about enabling.

    // scripts
    nsScriptLoader *loader = GetScriptLoader();
    if (loader) {
      loader->SetEnabled(PR_FALSE); // Do not load/process scripts when loading as data
    }

    // styles
    CSSLoader()->SetEnabled(PR_FALSE); // Do not load/process styles when loading as data
  } else if (nsCRT::strcmp("loadAsInteractiveData", aCommand) == 0) {
    mLoadedAsInteractiveData = PR_TRUE;
    aCommand = kLoadAsData; // XBL, for example, needs scripts and styles
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

  mParser = do_CreateInstance(kCParserCID, &rv);
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
  rv = CallQueryInterface(mParser, aDocListener);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(mChannel, "How can we not have a channel here?");
  mChannelIsPending = PR_TRUE;
  
  SetDocumentCharacterSet(charset);
  mParser->SetDocumentCharset(charset, charsetSource);
  mParser->SetCommand(aCommand);
  mParser->SetContentSink(sink);
  mParser->Parse(aUrl, nsnull, (void *)this);

  return NS_OK;
}

void
nsXMLDocument::EndLoad()
{
  mChannelIsPending = PR_FALSE;
  mLoopingForSyncLoad = PR_FALSE;

  if (mLoadedAsData || mLoadedAsInteractiveData) {
    // Generate a document load event for the case when an XML
    // document was loaded as pure data without any presentation
    // attached to it.
    nsEvent event(PR_TRUE, NS_LOAD);
    nsEventStatus status = nsEventStatus_eIgnore;

    nsIScriptGlobalObject* sgo = nsnull;
    nsCOMPtr<nsIScriptGlobalObjectOwner> container =
      do_QueryReferent(mDocumentContainer);
    if (container) {
      sgo = container->GetScriptGlobalObject();
    }

    nsCxPusher pusher(sgo);

    nsEventDispatcher::Dispatch(NS_STATIC_CAST(nsIDocument*, this), nsnull,
                                &event, nsnull, &status);
  }    
  nsDocument::EndLoad();  
}

PRBool
nsXMLDocument::IsLoadedAsData()
{
  return mLoadedAsData;
}
  

// nsIDOMNode interface

NS_IMETHODIMP    
nsXMLDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  return nsNodeUtils::CloneNodeImpl(this, aDeep, aReturn);
}
 
// nsIDOMDocument interface

NS_IMETHODIMP
nsXMLDocument::GetElementById(const nsAString& aElementId,
                              nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  if (!CheckGetElementByIdArg(aElementId))
    return NS_OK;

  // If we tried to load a document and something went wrong, we might not have
  // root content. This can happen when you do document.load() and the document
  // to load is not XML, for example.
  if (!mRootContent)
    return NS_OK;

  // XXX For now, we do a brute force search of the content tree.
  // We should come up with a more efficient solution.
  // Note that content is *not* refcounted here, so do *not* release it!
  nsIContent *content =
    nsContentUtils::MatchElementId(mRootContent, aElementId);

  if (!content) {
    return NS_OK;
  }

  return CallQueryInterface(content, aReturn);
}

nsresult
nsXMLDocument::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  NS_ASSERTION(aNodeInfo->NodeInfoManager() == mNodeInfoManager,
               "Can't import this document into another document!");

  nsCOMPtr<nsIDOMDocument> newDoc;
  nsresult rv = NS_NewDOMDocument(getter_AddRefs(newDoc), EmptyString(),
                                  EmptyString(), nsnull,
                                  nsIDocument::GetDocumentURI(),
                                  nsIDocument::GetBaseURI(), NodePrincipal());
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(newDoc, aResult);
}

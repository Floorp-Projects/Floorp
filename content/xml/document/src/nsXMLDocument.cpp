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
#include "nsIWebShell.h"
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
#include "nsContentCID.h"
#include "nsDOMAttribute.h"
#include "nsGUIEvent.h"
#include "nsFIXptr.h"
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

static NS_DEFINE_CID(kHTMLStyleSheetCID,NS_HTMLSTYLESHEET_CID);

// XXX The XML world depends on the html atoms
#include "nsHTMLAtoms.h"

static const char* kLoadAsData = "loadAsData";

// ==================================================================
// =
// ==================================================================


NS_EXPORT nsresult
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


NS_EXPORT nsresult
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

nsXMLDocument::nsXMLDocument() 
  : mAttrStyleSheet(nsnull), mInlineStyleSheet(nsnull), 
    mCountCatalogSheets(0), mParser(nsnull),
    mCrossSiteAccessEnabled(PR_FALSE), mXMLDeclarationBits(0)
{
}

nsXMLDocument::~nsXMLDocument()
{
  NS_IF_RELEASE(mParser);  
  if (mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
  }
  if (mCSSLoader) {
    mCSSLoader->DropDocumentReference();
  }
}


// QueryInterface implementation for nsHTMLAnchorElement
NS_INTERFACE_MAP_BEGIN(nsXMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIXMLDocument)
  NS_INTERFACE_MAP_ENTRY(nsIHTMLContentContainer)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIHttpEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXMLDocument)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XMLDocument)
NS_INTERFACE_MAP_END_INHERITING(nsDocument)


NS_IMPL_ADDREF_INHERITED(nsXMLDocument, nsDocument)
NS_IMPL_RELEASE_INHERITED(nsXMLDocument, nsDocument)


NS_IMETHODIMP 
nsXMLDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsresult result = nsDocument::Reset(aChannel, aLoadGroup);
  if (NS_FAILED(result)) return result;
  nsCOMPtr<nsIURI> url;
  if (aChannel) {
    result = aChannel->GetURI(getter_AddRefs(url));
    if (NS_FAILED(result)) return result;
  }

  if (nsnull != mAttrStyleSheet) {
    mAttrStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mAttrStyleSheet);
  }
  if (nsnull != mInlineStyleSheet) {
    mInlineStyleSheet->SetOwningDocument(nsnull);
    NS_RELEASE(mInlineStyleSheet);
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
  return nsFIXptr::Evaluate(this, aExpression, aRange);
}

NS_IMETHODIMP
nsXMLDocument::Load(const nsAString& aUrl)
{
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
  if (NS_FAILED(secMan->CheckConnect(nsnull, uri, "XMLDocument", "load")))
    return NS_ERROR_FAILURE;

  // Partial Reset, need to restore principal for security reasons and
  // event listener manager so that load listeners etc. will remain.
  nsCOMPtr<nsIPrincipal> principal = mPrincipal;
  nsCOMPtr<nsIEventListenerManager> elm = mListenerManager;

  Reset(nsnull, nsnull);
  
  mPrincipal = principal;
  mListenerManager = elm;
  NS_IF_ADDREF(mListenerManager);
  
  SetDocumentURL(uri);
  SetBaseURL(uri);

  // Store script context, if any, in case we encounter redirect (because we need it there)
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (stack) {
    JSContext *cx;
    if (NS_SUCCEEDED(stack->Peek(&cx)) && cx) {
      nsISupports *priv = (nsISupports *)::JS_GetContextPrivate(cx);
      if (priv) {
        priv->QueryInterface(NS_GET_IID(nsIScriptContext), getter_AddRefs(mScriptContext));        
      }
    }
  }

  // Find out if UniversalBrowserRead privileges are enabled - we will need this
  // in case of a redirect
  PRBool crossSiteAccessEnabled;
  rv = secMan->IsCapabilityEnabled("UniversalBrowserRead", &crossSiteAccessEnabled);
  if (NS_FAILED(rv)) return rv;

  mCrossSiteAccessEnabled = crossSiteAccessEnabled;

  // Create a channel
  rv = NS_NewChannel(getter_AddRefs(channel), uri, nsnull, nsnull, this);
  if (NS_FAILED(rv)) return rv;

  // Set a principal for this document
  mPrincipal = nsnull;
  nsCOMPtr<nsISupports> channelOwner;
  rv = channel->GetOwner(getter_AddRefs(channelOwner));
  if (NS_SUCCEEDED(rv) && channelOwner)
      mPrincipal = do_QueryInterface(channelOwner, &rv);

  if (NS_FAILED(rv) || !channelOwner)
  {
    rv = secMan->GetCodebasePrincipal(uri, getter_AddRefs(mPrincipal));
    if (!mPrincipal) return rv;
  }

  // Prepare for loading the XML document "into oneself"
  nsCOMPtr<nsIStreamListener> listener;
  if (NS_FAILED(rv = StartDocumentLoad(kLoadAsData, channel, 
                                       nsnull, nsnull, 
                                       getter_AddRefs(listener),
                                       PR_FALSE))) {
    NS_ERROR("nsXMLDocument::Load: Failed to start the document load.");
    return rv;
  }

  // Start an asynchronous read of the XML document
  rv = channel->AsyncOpen(listener, nsnull);

  return rv;
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

  nsresult rv = nsDocument::StartDocumentLoad(aCommand,
                                              aChannel, aLoadGroup,
                                              aContainer, 
                                              aDocListener, aReset, aSink);
  if (NS_FAILED(rv)) return rv;

  nsAutoString charset(NS_LITERAL_STRING("UTF-8"));
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
        nsAutoString preferred;
        rv = calias->GetPreferred(NS_ConvertASCIItoUCS2(charsetVal), preferred);
        if(NS_SUCCEEDED(rv)){            
          charset = preferred;
          charsetSource = kCharsetFromChannel;
        }
      }
    }
  } //end of checking channel's charset

  static NS_DEFINE_CID(kCParserCID, NS_PARSER_CID);

  rv = nsComponentManager::CreateInstance(kCParserCID, nsnull, 
                                          NS_GET_IID(nsIParser), 
                                          (void **)&mParser);
  if (NS_FAILED(rv))  return rv;

  nsCOMPtr<nsIXMLContentSink> sink;
    
  nsCOMPtr<nsIDocShell> docShell;
  if(aContainer)
  {
    docShell = do_QueryInterface(aContainer, &rv);
    if(NS_FAILED(rv) || !(docShell))  return rv; 

    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
    if (aSink)
      sink = do_QueryInterface(aSink);
    else
      rv = NS_NewXMLContentSink(getter_AddRefs(sink), this, aUrl, webShell, aChannel);
  }
  else {
    if (aSink)
      sink = do_QueryInterface(aSink);
    else
      rv = NS_NewXMLContentSink(getter_AddRefs(sink), this, aUrl, nsnull, aChannel);
  }

  if (NS_OK == rv) {      
    // Set the parser as the stream listener for the document loader...
    rv = mParser->QueryInterface(NS_GET_IID(nsIStreamListener), (void**)aDocListener);

    if (NS_OK == rv) {
      SetDocumentCharacterSet(charset);
      mParser->SetDocumentCharset(charset, charsetSource);
      mParser->SetCommand(aCommand);
      mParser->SetContentSink(sink);
      mParser->Parse(aUrl, nsnull, PR_FALSE, (void *)this);
    }
  }

  return rv;
}

NS_IMETHODIMP 
nsXMLDocument::EndLoad()
{
  nsAutoString cmd;
  if (mParser) mParser->GetCommand(cmd);
  NS_IF_RELEASE(mParser);
  if (cmd.EqualsWithConversion(kLoadAsData)) {
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
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mAttrStyleSheet;
  if (nsnull == mAttrStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  else {
    NS_ADDREF(mAttrStyleSheet);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLDocument::GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mInlineStyleSheet;
  if (nsnull == mInlineStyleSheet) {
    return NS_ERROR_NOT_AVAILABLE;  // probably not the right error...
  }
  else {
    NS_ADDREF(mInlineStyleSheet);
  }
  return NS_OK;
}

// subclass hook for sheet ordering
void nsXMLDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet, PRUint32 aFlags)
{
  if (aFlags & NS_STYLESHEET_FROM_CATALOG) {
    // always after other catalog sheets
    mStyleSheets.InsertObjectAt(aSheet, mCountCatalogSheets);
    ++mCountCatalogSheets;
  }
  else if (aSheet == mAttrStyleSheet) {  // always after catalog sheets
    mStyleSheets.InsertObjectAt(aSheet, mCountCatalogSheets);
  }
  else if (aSheet == mInlineStyleSheet) {  // always last
    mStyleSheets.AppendObject(aSheet);
  }
  else {
    PRInt32 count = mStyleSheets.Count();
    if (count != 0 && mInlineStyleSheet == mStyleSheets.ObjectAt(count - 1)) {
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
  // offset w.r.t. catalog style sheets and the attr style sheet
  mStyleSheets.InsertObjectAt(aSheet, aIndex + mCountCatalogSheets + 1);
}

// nsIDOMDocument interface
NS_IMETHODIMP    
nsXMLDocument::GetDoctype(nsIDOMDocumentType** aDocumentType)
{
  return nsDocument::GetDoctype(aDocumentType); 
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateCDATASection(const nsAString& aData, nsIDOMCDATASection** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsReadingIterator<PRUnichar> begin;
  nsReadingIterator<PRUnichar> end;
  aData.BeginReading(begin);
  aData.EndReading(end);
  if (FindInReadable(NS_LITERAL_STRING("]]>"),begin,end))
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;

  nsIContent* content;
  nsresult rv = NS_NewXMLCDATASection(&content);

  if (NS_OK == rv) {
    rv = content->QueryInterface(NS_GET_IID(nsIDOMCDATASection), (void**)aReturn);
    (*aReturn)->AppendData(aData);
    NS_RELEASE(content);
  }

  return rv;
}
 
NS_IMETHODIMP    
nsXMLDocument::CreateEntityReference(const nsAString& aName, nsIDOMEntityReference** aReturn)
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

  nsIContent* content;
  nsresult rv = NS_NewXMLProcessingInstruction(&content, aTarget, aData);

  if (NS_OK != rv) {
    return rv;
  }

  rv = content->QueryInterface(NS_GET_IID(nsIDOMProcessingInstruction), (void**)aReturn);
  NS_RELEASE(content);
  
  return rv;
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
                                     *getter_AddRefs(nodeInfo));
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

  return newDoc->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);
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
                                              *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString value;
  nsDOMAttribute* attribute;

  attribute = new nsDOMAttribute(nsnull, nodeInfo, value);
  NS_ENSURE_TRUE(attribute, NS_ERROR_OUT_OF_MEMORY); 

  return attribute->QueryInterface(NS_GET_IID(nsIDOMAttr), (void**)aReturn);
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
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  return CreateElement(nodeInfo, aReturn);
}

static nsIContent *
MatchId(nsIContent *aContent, const nsAString& aName)
{
  nsAutoString value;
  nsIContent *result = nsnull;
  PRInt32 ns;

  aContent->GetNameSpaceID(ns);
  if (kNameSpaceID_XHTML == ns) {
    if ((NS_CONTENT_ATTR_HAS_VALUE == aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, value)) &&
        aName.Equals(value)) {
      return aContent;
    }
  }
  else {
    nsCOMPtr<nsIXMLContent> xmlContent = do_QueryInterface(aContent);    
    nsCOMPtr<nsIAtom> IDValue;
    if (xmlContent && NS_SUCCEEDED(xmlContent->GetID(*getter_AddRefs(IDValue))) && IDValue) {
      const PRUnichar* IDValStr = nsnull;
      IDValue->GetUnicode(&IDValStr);       
      if (aName.Equals(IDValStr)) {
        return aContent;
      }
    }
  }
  
  PRInt32 i, count;
  aContent->ChildCount(count);
  for (i = 0; i < count && result == nsnull; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    result = MatchId(child, aName);
    NS_RELEASE(child);
  }  

  return result;
}

NS_IMETHODIMP
nsXMLDocument::GetElementById(const nsAString& aElementId,
                             nsIDOMElement** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  NS_WARN_IF_FALSE(!aElementId.IsEmpty(),"getElementById(\"\"), fix caller?");
  if (aElementId.IsEmpty())
    return NS_OK;

  // If we tried to load a document and something went wrong, we might not have
  // root content. This can happen when you do document.load() and the document
  // to load is not XML, for example.
  if (!mRootContent)
    return NS_OK;

  // XXX For now, we do a brute force search of the content tree.
  // We should come up with a more efficient solution.
  nsCOMPtr<nsIContent> content(do_QueryInterface(MatchId(mRootContent,aElementId)));

  nsresult rv = NS_OK;
  if (content) {
    rv = content->QueryInterface(NS_GET_IID(nsIDOMElement),(void**)aReturn);
  }

  return rv;
}

// nsIXMLDocument
NS_IMETHODIMP 
nsXMLDocument::SetDefaultStylesheets(nsIURI* aUrl)
{
  nsresult result = NS_OK;
  if (aUrl) {
    //result = NS_NewHTMLStyleSheet(&mAttrStyleSheet, aUrl, this);
    result = nsComponentManager::CreateInstance(kHTMLStyleSheetCID,nsnull,NS_GET_IID(nsIHTMLStyleSheet),(void**)&mAttrStyleSheet);
    if (NS_SUCCEEDED(result)) {
      result = mAttrStyleSheet->Init(aUrl,this);
      if (NS_FAILED(result)) {
        NS_RELEASE(mAttrStyleSheet);
      }
    }
    if (NS_OK == result) {
      AddStyleSheet(mAttrStyleSheet, 0); // tell the world about our new style sheet
      
      result = NS_NewHTMLCSSStyleSheet(&mInlineStyleSheet, aUrl, this);
      if (NS_OK == result) {
        AddStyleSheet(mInlineStyleSheet, 0); // tell the world about our new style sheet
      }
    }
  }

  return result;
}

NS_IMETHODIMP 
nsXMLDocument::SetXMLDeclaration(const nsAString& aVersion,
                                 const nsAString& aEncoding,
                                 const nsAString& aStandalone)
{
  if (aVersion.IsEmpty()) {
    mXMLDeclarationBits = 0;
    return NS_OK;
  }

  mXMLDeclarationBits = XML_DECLARATION_BITS_DECLARATION_EXISTS;

  if (!aEncoding.IsEmpty()) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_ENCODING_EXISTS;
  }

  if (aStandalone.Equals(NS_LITERAL_STRING("yes"))) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS |
                           XML_DECLARATION_BITS_STANDALONE_YES;
  } else if (aStandalone.Equals(NS_LITERAL_STRING("no"))) {
    mXMLDeclarationBits |= XML_DECLARATION_BITS_STANDALONE_EXISTS;
  }

  return NS_OK;
}                               

NS_IMETHODIMP 
nsXMLDocument::GetXMLDeclaration(nsAString& aVersion,
                                 nsAString& aEncoding,
                                 nsAString& aStandalone)
{
  aVersion.Truncate();
  aEncoding.Truncate();
  aStandalone.Truncate();

  if (!(mXMLDeclarationBits & XML_DECLARATION_BITS_DECLARATION_EXISTS)) {
    return NS_OK;
  }

  aVersion.Assign(NS_LITERAL_STRING("1.0")); // always until we start supporting 1.1 etc.

  if (mXMLDeclarationBits & XML_DECLARATION_BITS_ENCODING_EXISTS) {
    GetDocumentCharacterSet(aEncoding); // This is what we have stored, not necessarily what was written in the original
  }

  if (mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_EXISTS) {
    if (mXMLDeclarationBits & XML_DECLARATION_BITS_STANDALONE_YES) {
      aStandalone.Assign(NS_LITERAL_STRING("yes"));
    } else {
      aStandalone.Assign(NS_LITERAL_STRING("no"));
    }
  }

  return NS_OK;
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
  nsresult result = NS_OK;
  if (! mCSSLoader) {
    result = NS_NewCSSLoader(this, getter_AddRefs(mCSSLoader));
    if (mCSSLoader) {
      mCSSLoader->SetCaseSensitive(PR_TRUE);
      // No quirks in XML
      mCSSLoader->SetCompatibilityMode(eCompatibility_FullStandards);
    }
  }
  aLoader = mCSSLoader;
  NS_IF_ADDREF(aLoader);
  return result;
}

nsresult
nsXMLDocument::CreateElement(nsINodeInfo *aNodeInfo, nsIDOMElement** aResult)
{
  *aResult = nsnull;
  
  nsresult rv;
  
  nsCOMPtr<nsIContent> content;

  PRInt32 namespaceID;
  aNodeInfo->GetNamespaceID(namespaceID);

  nsCOMPtr<nsIElementFactory> elementFactory;
  mNameSpaceManager->GetElementFactory(namespaceID,
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

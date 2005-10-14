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
#include "nsCOMPtr.h"
#include "nsXMLContentSink.h"
#include "nsIParser.h"
#include "nsIUnicharInputStream.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMNSDocument.h"
#include "nsIXMLContent.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIDOMComment.h"
#include "nsIDOMCDATASection.h"
#include "nsDOMDocumentType.h"
#include "nsHTMLParts.h"
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsHTMLAtoms.h"
#include "nsContentUtils.h"
#include "nsLayoutAtoms.h"
#include "nsIScriptContext.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContentViewer.h"
#include "prtime.h"
#include "prlog.h"
#include "prmem.h"
#include "nsParserUtils.h"
#include "nsRect.h"
#include "nsGenericElement.h"
#include "nsIWebNavigation.h"
#include "nsIScriptElement.h"
#include "nsIScriptLoader.h"
#include "nsStyleLinkElement.h"
#include "nsIImageLoadingContent.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsICookieService.h"
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIChannel.h"
#include "nsIPrincipal.h"
#include "nsXBLAtoms.h"
#include "nsXMLPrettyPrinter.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"

#ifdef MOZ_SVG
#include "nsSVGAtoms.h"
#include "nsGUIEvent.h"
#endif

#define kXSLType "text/xsl"

static const char kLoadAsData[] = "loadAsData";



// XXX Open Issues:
// 1) what's not allowed - We need to figure out which HTML tags
//    (prefixed with a HTML namespace qualifier) are explicitly not
//    allowed (if any).
// 2) factoring code with nsHTMLContentSink - There's some amount of
//    common code between this and the HTML content sink. This will
//    increase as we support more and more HTML elements. How can code
//    from the code be factored?

nsresult
NS_NewXMLContentSink(nsIXMLContentSink** aResult,
                     nsIDocument* aDoc,
                     nsIURI* aURI,
                     nsISupports* aContainer,
                     nsIChannel* aChannel)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXMLContentSink* it;
  NS_NEWXPCOM(it, nsXMLContentSink);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsCOMPtr<nsIXMLContentSink> kungFuDeathGrip = it;
  nsresult rv = it->Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);
  
  return CallQueryInterface(it, aResult);
}

nsXMLContentSink::nsXMLContentSink()
  : mDocElement(nsnull),
    mText(nsnull),
    mTextLength(0),
    mTextSize(0),
    mConstrainSize(PR_TRUE),
    mInTitle(PR_FALSE),
    mPrettyPrintXML(PR_TRUE),
    mPrettyPrintHasSpecialRoot(PR_FALSE),
    mPrettyPrintHasFactoredElements(PR_FALSE),
    mHasProcessedBase(PR_FALSE),
    mAllowAutoXLinks(PR_TRUE)
{
}

nsXMLContentSink::~nsXMLContentSink()
{
  NS_IF_RELEASE(mDocElement);
  if (mText) {
    PR_Free(mText);  //  Doesn't null out, unlike PR_FREEIF
  }
}

nsresult
nsXMLContentSink::Init(nsIDocument* aDoc,
                       nsIURI* aURI,
                       nsISupports* aContainer,
                       nsIChannel* aChannel)
{
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mDocShell) {
    mPrettyPrintXML = PR_FALSE;
  }
  
  mState = eXMLContentSinkState_InProlog;
  mDocElement = nsnull;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED4(nsXMLContentSink,
                             nsContentSink,
                             nsIContentSink,
                             nsIXMLContentSink,
                             nsIExpatSink,
                             nsITransformObserver)

// nsIContentSink
NS_IMETHODIMP
nsXMLContentSink::WillBuildModel(void)
{
  // Notify document that the load is beginning
  mDocument->BeginLoad();

  // Check for correct load-command for maybe prettyprinting
  if (mPrettyPrintXML) {
    nsCAutoString command;
    mParser->GetCommand(command);
    if (!command.EqualsLiteral("view")) {
      mPrettyPrintXML = PR_FALSE;
    }
  }
  
  return NS_OK;
}

nsresult
nsXMLContentSink::MaybePrettyPrint()
{
  if (!mPrettyPrintXML || (mPrettyPrintHasFactoredElements &&
                           !mPrettyPrintHasSpecialRoot)) {
    mPrettyPrintXML = PR_FALSE;

    return NS_OK;
  }

  // Reenable the CSSLoader so that the prettyprinting stylesheets can load
  if (mCSSLoader) {
    mCSSLoader->SetEnabled(PR_TRUE);
  }
  
  nsCOMPtr<nsXMLPrettyPrinter> printer;
  nsresult rv = NS_NewXMLPrettyPrinter(getter_AddRefs(printer));
  NS_ENSURE_SUCCESS(rv, rv);

  return printer->PrettyPrint(mDocument);
}

NS_IMETHODIMP
nsXMLContentSink::DidBuildModel()
{
  if (mDocument && mDocument->GetDocumentTitle().IsVoid()) {
    nsCOMPtr<nsIDOMNSDocument> dom_doc(do_QueryInterface(mDocument));
    dom_doc->SetTitle(EmptyString());
  }

  if (mXSLTProcessor) {
    nsCOMPtr<nsIDOMDocument> currentDOMDoc(do_QueryInterface(mDocument));
    mXSLTProcessor->SetSourceContentModel(currentDOMDoc);
    // Since the processor now holds a reference to us we drop our reference
    // to it to avoid owning cycles
    mXSLTProcessor = nsnull;
  }
  else {
    // Kick off layout for non-XSLT transformed documents.
    nsIScriptLoader *loader = mDocument->GetScriptLoader();
    if (loader) {
      loader->RemoveObserver(this);
    }

    if (mDocElement) {
      // Notify document observers that all the content has been stuck
      // into the document.
      // XXX do we need to notify for things like PIs?  Or just the
      // documentElement?
      NS_ASSERTION(mDocument->IndexOf(mDocElement) != -1,
                   "mDocElement not in doc?");

      mozAutoDocUpdate docUpdate(mDocument, UPDATE_CONTENT_MODEL, PR_TRUE);
      mDocument->ContentInserted(nsnull, mDocElement,
                                 // XXXbz is this last arg relevant if
                                 // the container is null?
                                 mDocument->IndexOf(mDocElement));
    }

    // Check if we want to prettyprint
    MaybePrettyPrint();

    StartLayout();

#if 0 /* Disable until this works for XML */
    //  Scroll to Anchor only if the document was *not* loaded through history means. 
    if (mDocShell) {
      PRUint32 documentLoadType = 0;
      mDocShell->GetLoadType(&documentLoadType);
      ScrollToRef(!(documentLoadType & nsIDocShell::LOAD_CMD_HISTORY));
    }
#else
    ScrollToRef(PR_TRUE);
#endif

    mDocument->EndLoad();
  }

  // Drop our reference to the parser to get rid of a circular
  // reference.
  mParser = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::OnDocumentCreated(nsIDOMDocument* aResultDocument)
{
  NS_ENSURE_ARG(aResultDocument);

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    contentViewer->SetDOMDocument(aResultDocument);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::OnTransformDone(nsresult aResult,
                                  nsIDOMDocument* aResultDocument)
{
  NS_ASSERTION(NS_FAILED(aResult) || aResultDocument,
               "Don't notify about transform success without a document.");

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));

  if (NS_FAILED(aResult) && contentViewer) {
    // Transform failed.
    if (aResultDocument) {
      // We have an error document.
      contentViewer->SetDOMDocument(aResultDocument);
    }
    else {
      // We don't have an error document, display the
      // untransformed source document.
      nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(mDocument);
      contentViewer->SetDOMDocument(document);
    }
  }

  nsCOMPtr<nsIDocument> originalDocument = mDocument;
  if (NS_SUCCEEDED(aResult) || aResultDocument) {
    // Transform succeeded or it failed and we have an error
    // document to display.
    mDocument = do_QueryInterface(aResultDocument);
  }

  nsIScriptLoader *loader = originalDocument->GetScriptLoader();
  if (loader) {
    loader->RemoveObserver(this);
  }

  // Notify document observers that all the content has been stuck
  // into the document.  
  // XXX do we need to notify for things like PIs?  Or just the
  // documentElement?
  nsIContent *rootContent = mDocument->GetRootContent();
  if (rootContent) {
    NS_ASSERTION(mDocument->IndexOf(rootContent) != -1,
                 "rootContent not in doc?");
    mDocument->BeginUpdate(UPDATE_CONTENT_MODEL);
    mDocument->ContentInserted(nsnull, rootContent,
                               // XXXbz is this last arg relevant if
                               // the container is null?
                               mDocument->IndexOf(rootContent));
    mDocument->EndUpdate(UPDATE_CONTENT_MODEL);
  }
  
  // Start the layout process
  StartLayout();

#if 0 /* Disable until this works for XML */
  //  Scroll to Anchor only if the document was *not* loaded through history means. 
  PRUint32 documentLoadType = 0;
  docShell->GetLoadType(&documentLoadType);
  ScrollToRef(!(documentLoadType & nsIDocShell::LOAD_CMD_HISTORY));
#else
  ScrollToRef(PR_TRUE);
#endif

  originalDocument->EndLoad();

  return NS_OK;
}


NS_IMETHODIMP
nsXMLContentSink::WillInterrupt(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::WillResume(void)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::SetParser(nsIParser* aParser)
{
  mParser = aParser;
  return NS_OK;
}

nsresult
nsXMLContentSink::CreateElement(const PRUnichar** aAtts, PRUint32 aAttsCount,
                                nsINodeInfo* aNodeInfo, PRUint32 aLineNumber,
                                nsIContent** aResult, PRBool* aAppendContent)
{
  NS_ASSERTION(aNodeInfo, "can't create element without nodeinfo");

  *aResult = nsnull;
  *aAppendContent = PR_TRUE;
  nsresult rv = NS_OK;

  nsCOMPtr<nsIContent> content;
  rv = NS_NewElement(getter_AddRefs(content), aNodeInfo->NamespaceID(),
                     aNodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNodeInfo->Equals(nsHTMLAtoms::script, kNameSpaceID_XHTML)
#ifdef MOZ_SVG
      || aNodeInfo->Equals(nsSVGAtoms::script, kNameSpaceID_SVG)
#endif
    ) {
    // Don't append the content to the tree until we're all
    // done collecting its contents
    mConstrainSize = PR_FALSE;
    mScriptLineNo = aLineNumber;
    *aAppendContent = PR_FALSE;
  }

  // XHTML needs some special attention
  if (aNodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    mPrettyPrintHasFactoredElements = PR_TRUE;
  }
  else {
    // If we care, find out if we just used a special factory.
    if (!mPrettyPrintHasFactoredElements && !mPrettyPrintHasSpecialRoot &&
        mPrettyPrintXML) {
      mPrettyPrintHasFactoredElements =
        nsContentUtils::NameSpaceManager()->
          HasElementCreator(aNodeInfo->NamespaceID());
    }

    if (!aNodeInfo->NamespaceEquals(kNameSpaceID_SVG)) {
      content.swap(*aResult);

      return NS_OK;
    }
  }

  if (aNodeInfo->Equals(nsHTMLAtoms::title, kNameSpaceID_XHTML)) {
    if (mDocument && mDocument->GetDocumentTitle().IsVoid()) {
      mInTitle = PR_TRUE; // The first title wins
    }
  }
#ifdef MOZ_SVG
  else if (aNodeInfo->Equals(nsSVGAtoms::title, kNameSpaceID_SVG)) {
    nsIContent* parent = GetCurrentContent();
    if (mDocument && mDocument->GetDocumentTitle().IsVoid() &&
        parent && parent == mDocElement &&
        parent->NodeInfo()->Equals(nsSVGAtoms::svg, kNameSpaceID_SVG)) {
      mInTitle = PR_TRUE; // The first title wins
    }
  }
#endif // MOZ_SVG
  else if (aNodeInfo->Equals(nsHTMLAtoms::link, kNameSpaceID_XHTML) ||
           aNodeInfo->Equals(nsHTMLAtoms::style, kNameSpaceID_XHTML) ||
           aNodeInfo->Equals(nsHTMLAtoms::style, kNameSpaceID_SVG)) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(content));
    if (ssle) {
      ssle->InitStyleLinkElement(mParser, PR_FALSE);
      ssle->SetEnableUpdates(PR_FALSE);
      if (!aNodeInfo->Equals(nsHTMLAtoms::link, kNameSpaceID_XHTML)) {
        ssle->SetLineNumber(aLineNumber);
      }
    }
  } 

  content.swap(*aResult);

  return NS_OK;
}


nsresult
nsXMLContentSink::CloseElement(nsIContent* aContent, nsIContent* aParent,
                               PRBool* aAppendContent)
{
  NS_ASSERTION(aContent, "missing element to close");

  *aAppendContent = PR_FALSE;

  nsINodeInfo *nodeInfo = aContent->NodeInfo();

  // Some HTML nodes need DoneAddingChildren() called to initialize
  // properly (eg form state restoration).
  if ((nodeInfo->NamespaceID() == kNameSpaceID_XHTML &&
       (nodeInfo->NameAtom() == nsHTMLAtoms::select ||
        nodeInfo->NameAtom() == nsHTMLAtoms::textarea ||
        nodeInfo->NameAtom() == nsHTMLAtoms::object ||
        nodeInfo->NameAtom() == nsHTMLAtoms::applet))
#ifdef MOZ_XTF
      || nodeInfo->NamespaceID() > kNameSpaceID_LastBuiltin
#endif
      ) {
    aContent->DoneAddingChildren(PR_FALSE);
  }
  
  if (!nodeInfo->NamespaceEquals(kNameSpaceID_XHTML) &&
      !nodeInfo->NamespaceEquals(kNameSpaceID_SVG)) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  if (nodeInfo->Equals(nsHTMLAtoms::script, kNameSpaceID_XHTML)
#ifdef MOZ_SVG
      || nodeInfo->Equals(nsSVGAtoms::script, kNameSpaceID_SVG)
#endif
    ) {
    rv = ProcessEndSCRIPTTag(aContent, aParent);
    *aAppendContent = PR_TRUE;
    return rv;
  }
  
  if ((nodeInfo->Equals(nsHTMLAtoms::title, kNameSpaceID_XHTML)
#ifdef MOZ_SVG
       || nodeInfo->Equals(nsSVGAtoms::title, kNameSpaceID_SVG)
#endif // MOZ_SVG
      ) && mInTitle) {
    NS_ASSERTION(mDocument, "How did mInTitle get to be true if mDocument is null?");
    // The first title wins
    nsCOMPtr<nsIDOMNSDocument> dom_doc(do_QueryInterface(mDocument));
    mTitleText.CompressWhitespace();
    dom_doc->SetTitle(mTitleText);
    mInTitle = PR_FALSE;
  }
  else if (nodeInfo->Equals(nsHTMLAtoms::base, kNameSpaceID_XHTML) &&
           !mHasProcessedBase) {
    // The first base wins
    rv = ProcessBASETag(aContent);
    mHasProcessedBase = PR_TRUE;
  }
  else if (nodeInfo->Equals(nsHTMLAtoms::meta, kNameSpaceID_XHTML) &&
           // Need to check here to make sure this meta tag does not set
           // mPrettyPrintXML to false when we have a special root!
           (!mPrettyPrintXML || !mPrettyPrintHasSpecialRoot)) {
    rv = ProcessMETATag(aContent);
  }
  else if (nodeInfo->Equals(nsHTMLAtoms::link, kNameSpaceID_XHTML) ||
           nodeInfo->Equals(nsHTMLAtoms::style, kNameSpaceID_XHTML) ||
           nodeInfo->Equals(nsHTMLAtoms::style, kNameSpaceID_SVG)) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(aContent));
    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      rv = ssle->UpdateStyleSheet(nsnull, nsnull);
      if (rv == NS_ERROR_HTMLPARSER_BLOCK && mParser) {
        mParser->BlockParser();
      }
    }
  }

  return rv;
}  

nsresult
nsXMLContentSink::AddContentAsLeaf(nsIContent *aContent)
{
  nsresult result = NS_OK;

  if ((eXMLContentSinkState_InProlog == mState) ||
      (eXMLContentSinkState_InEpilog == mState)) {
    nsCOMPtr<nsIDOMDocument> domDoc( do_QueryInterface(mDocument) );
    nsCOMPtr<nsIDOMNode> trash;
    nsCOMPtr<nsIDOMNode> child( do_QueryInterface(aContent) );
    NS_ASSERTION(child, "not a dom node");
    domDoc->AppendChild(child, getter_AddRefs(trash));
  }
  else {
    nsCOMPtr<nsIContent> parent = GetCurrentContent();

    if (parent) {
      result = parent->AppendChildTo(aContent, PR_FALSE);
    }
  }

  return result;
}

// Create an XML parser and an XSL content sink and start parsing
// the XSL stylesheet located at the given URI.
nsresult
nsXMLContentSink::LoadXSLStyleSheet(nsIURI* aUrl)
{
  mXSLTProcessor =
    do_CreateInstance("@mozilla.org/document-transformer;1?type=xslt");
  if (!mXSLTProcessor) {
    // No XSLT processor available, continue normal document loading
    return NS_OK;
  }

  mXSLTProcessor->SetTransformObserver(this);

  nsCOMPtr<nsILoadGroup> loadGroup = mDocument->GetDocumentLoadGroup();
  if (!loadGroup) {
    mXSLTProcessor = nsnull;
    return NS_ERROR_FAILURE;
  }

  return mXSLTProcessor->LoadStyleSheet(aUrl, loadGroup, mDocument->GetPrincipal());
}

nsresult
nsXMLContentSink::ProcessStyleLink(nsIContent* aElement,
                                   const nsSubstring& aHref,
                                   PRBool aAlternate,
                                   const nsSubstring& aTitle,
                                   const nsSubstring& aType,
                                   const nsSubstring& aMedia)
{
  nsresult rv = NS_OK;
  mPrettyPrintXML = PR_FALSE;

  nsCAutoString cmd;
  if (mParser)
    mParser->GetCommand(cmd);
  if (cmd.EqualsASCII(kLoadAsData))
    return NS_OK; // Do not load stylesheets when loading as data

  NS_ConvertUTF16toUTF8 type(aType);
  if (type.EqualsIgnoreCase(kXSLType) ||
      type.EqualsIgnoreCase(kXMLTextContentType) ||
      type.EqualsIgnoreCase(kXMLApplicationContentType)) {
    if (aAlternate) {
      // don't load alternate XSLT
      return NS_OK;
    }
    // LoadXSLStyleSheet needs a mDocShell.
    if (!mDocShell)
      return NS_OK;

    nsCOMPtr<nsIURI> url;
    rv = NS_NewURI(getter_AddRefs(url), aHref, nsnull, mDocumentBaseURI);
    NS_ENSURE_SUCCESS(rv, rv);

    // Do security check
    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
    rv = secMan->
      CheckLoadURIWithPrincipal(mDocument->GetPrincipal(), url,
                                nsIScriptSecurityManager::ALLOW_CHROME);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    rv = secMan->CheckSameOriginURI(mDocumentURI, url);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    // Do content policy check
    PRInt16 decision = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_STYLESHEET,
                                   url,
                                   mDocument->GetDocumentURI(),
                                   aElement,
                                   type,
                                   nsnull,
                                   &decision,
                                   nsContentUtils::GetContentPolicy());

    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_CP_REJECTED(decision)) {
      return NS_OK;
    }

    return LoadXSLStyleSheet(url);
  }

  // Let nsContentSink deal with css.
  rv = nsContentSink::ProcessStyleLink(aElement, aHref, aAlternate,
                                       aTitle, aType, aMedia);

  if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
    if (mParser) {
      mParser->BlockParser();
    }
    return NS_OK;
  }

  return rv;
}

nsresult
nsXMLContentSink::ProcessBASETag(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing base-element");

  nsresult rv = NS_OK;

  if (mDocument) {
    nsAutoString value;
  
    if (aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::target, value) ==
        NS_CONTENT_ATTR_HAS_VALUE) {
      mDocument->SetBaseTarget(value);
    }

    if (aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, value) ==
        NS_CONTENT_ATTR_HAS_VALUE) {
      nsCOMPtr<nsIURI> baseURI;
      rv = NS_NewURI(getter_AddRefs(baseURI), value);
      if (NS_SUCCEEDED(rv)) {
        rv = mDocument->SetBaseURI(baseURI); // The document checks if it is legal to set this base
        if (NS_SUCCEEDED(rv)) {
          mDocumentBaseURI = mDocument->GetBaseURI();
        }
      }
    }
  }

  return rv;
}


NS_IMETHODIMP 
nsXMLContentSink::SetDocumentCharset(nsACString& aCharset)
{
  if (mDocument) {
    mDocument->SetDocumentCharacterSet(aCharset);
  }
  
  return NS_OK;
}

nsISupports *
nsXMLContentSink::GetTarget()
{
  return mDocument;
}

nsresult
nsXMLContentSink::FlushText(PRBool aCreateTextNode, PRBool* aDidFlush)
{
  nsresult rv = NS_OK;
  PRBool didFlush = PR_FALSE;
  if (0 != mTextLength) {
    if (aCreateTextNode) {
      nsCOMPtr<nsITextContent> textContent;
      rv = NS_NewTextNode(getter_AddRefs(textContent), mNodeInfoManager);
      NS_ENSURE_SUCCESS(rv, rv);

      // Set the text in the text node
      textContent->SetText(mText, mTextLength, PR_FALSE);

      // Add text to its parent
      AddContentAsLeaf(textContent);
    }
    mTextLength = 0;
    didFlush = PR_TRUE;
  }

  if (nsnull != aDidFlush) {
    *aDidFlush = didFlush;
  }
  return rv;
}

nsIContent*
nsXMLContentSink::GetCurrentContent()
{
  PRInt32 count = mContentStack.Count();

  if (count == 0) {
    return nsnull;
  }

  NS_ASSERTION(count > 0, "Bogus Count()");

  return mContentStack[count-1];
}

PRInt32
nsXMLContentSink::PushContent(nsIContent *aContent)
{
  NS_PRECONDITION(aContent, "Null content being pushed!");
  mContentStack.AppendObject(aContent);
  return mContentStack.Count();
}

already_AddRefed<nsIContent>
nsXMLContentSink::PopContent()
{  
  PRInt32 count = mContentStack.Count();

  if (count == 0) {
    NS_WARNING("Popping empty stack");
    return nsnull;
  }

  NS_ASSERTION(count > 0, "Bogus Count()");

  nsIContent* content = mContentStack[count - 1];
  NS_IF_ADDREF(content);
  mContentStack.RemoveObjectAt(count - 1);
  
  return content;
}

void
nsXMLContentSink::StartLayout()
{
  PRBool topLevelFrameset = PR_FALSE;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  if (docShellAsItem) {
    nsCOMPtr<nsIDocShellTreeItem> root;
    docShellAsItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
    if(docShellAsItem == root) {
      topLevelFrameset = PR_TRUE;
    }
  }
  
  nsContentSink::StartLayout(topLevelFrameset);

}

#ifdef MOZ_MATHML
////////////////////////////////////////////////////////////////////////
// MathML Element Factory - temporary location for bug 132844
// Will be factored out post 1.0

nsresult
NS_NewMathMLElement(nsIContent** aResult, nsINodeInfo* aNodeInfo)
{
  static const char kMathMLStyleSheetURI[] = "resource://gre/res/mathml.css";

  aNodeInfo->SetIDAttributeAtom(nsHTMLAtoms::id);
  
  // this bit of code is to load mathml.css on demand
  nsIDocument *doc = aNodeInfo->GetDocument();
  if (doc)
    doc->EnsureCatalogStyleSheet(kMathMLStyleSheetURI);

  return NS_NewXMLElement(aResult, aNodeInfo);
}
#endif // MOZ_MATHML


////////////////////////////////////////////////////////////////////////

PRBool
nsXMLContentSink::SetDocElement(PRInt32 aNameSpaceID,
                                nsIAtom* aTagName,
                                nsIContent *aContent)
{
  if (mDocElement)
    return PR_FALSE;

  // check for root elements that needs special handling for
  // prettyprinting
  if ((aNameSpaceID == kNameSpaceID_XBL &&
       aTagName == nsXBLAtoms::bindings) ||
      (aNameSpaceID == kNameSpaceID_XSLT &&
       (aTagName == nsLayoutAtoms::stylesheet ||
        aTagName == nsLayoutAtoms::transform))) {
    mPrettyPrintHasSpecialRoot = PR_TRUE;
    if (mPrettyPrintXML) {
      // In this case, disable script execution, stylesheet
      // loading, and auto XLinks since we plan to prettyprint.
      mAllowAutoXLinks = PR_FALSE;
      nsIScriptLoader* scriptLoader = mDocument->GetScriptLoader();
      if (scriptLoader) {
        scriptLoader->SetEnabled(PR_FALSE);
      }
      if (mCSSLoader) {
        mCSSLoader->SetEnabled(PR_FALSE);
      }
    }        
  }

  mDocElement = aContent;
  NS_ADDREF(mDocElement);

  nsresult rv = mDocument->SetRootContent(mDocElement);
  if (NS_FAILED(rv)) {
    // If we return PR_FALSE here, the caller will bail out because it won't
    // find a parent content node to append to, which is fine.
    return PR_FALSE;
  }
  return PR_TRUE;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleStartElement(const PRUnichar *aName, 
                                     const PRUnichar **aAtts, 
                                     PRUint32 aAttsCount, 
                                     PRInt32 aIndex, 
                                     PRUint32 aLineNumber)
{
  NS_PRECONDITION(aIndex >= -1, "Bogus aIndex");
  NS_PRECONDITION(aAttsCount % 2 == 0, "incorrect aAttsCount");
  // Adjust aAttsCount so it's the actual number of attributes
  aAttsCount /= 2;

  nsresult result = NS_OK;
  PRBool appendContent = PR_TRUE;
  nsCOMPtr<nsIContent> content;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the epilog, there should be no
  // new elements
  PR_ASSERT(eXMLContentSinkState_InEpilog != mState);

  FlushText();

  mState = eXMLContentSinkState_InDocumentElement;

  PRInt32 nameSpaceID;
  nsCOMPtr<nsIAtom> prefix, localName;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                 getter_AddRefs(localName), &nameSpaceID);

  if (!OnOpenContainer(aAtts, aAttsCount, nameSpaceID, localName, aLineNumber)) {
    return NS_OK;
  }
  
  nsCOMPtr<nsINodeInfo> nodeInfo;
  result = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID,
                                         getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(result, result);

  result = CreateElement(aAtts, aAttsCount, nodeInfo, aLineNumber,
                         getter_AddRefs(content), &appendContent);
  NS_ENSURE_SUCCESS(result, result);

  if (mDocument) {
    content->SetContentID(mDocument->GetAndIncrementContentID());
  }

  // Set the ID attribute atom on the node info object for this node
  // This must occur before the attributes are added so the name
  // of the id attribute is known.
  if (aIndex != -1 && NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIAtom> IDAttr = do_GetAtom(aAtts[aIndex]);

    if (IDAttr) {
      nodeInfo->SetIDAttributeAtom(IDAttr);
    }
  }
  
#ifdef MOZ_XTF
  if (nameSpaceID > kNameSpaceID_LastBuiltin)
    content->BeginAddingChildren();
#endif

  // Set the attributes on the new content element
  result = AddAttributes(aAtts, content);

  if (NS_OK == result) {
    // Store the element 
    if (!SetDocElement(nameSpaceID, localName, content) && appendContent) {
      nsCOMPtr<nsIContent> parent = GetCurrentContent();
      NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

      parent->AppendChildTo(content, PR_FALSE);
    }

    PushContent(content);
  }

  // Some HTML nodes need DoneCreatingElement() called to initialize
  // properly (eg form state restoration).
  if (nodeInfo->NamespaceID() == kNameSpaceID_XHTML &&
      (nodeInfo->NameAtom() == nsHTMLAtoms::input ||
       nodeInfo->NameAtom() == nsHTMLAtoms::button)) {
    content->DoneCreatingElement();
  }

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleEndElement(const PRUnichar *aName)
{
  nsresult result = NS_OK;
  PRBool appendContent = PR_FALSE;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the prolog or epilog, there should be
  // no close tags for elements.
  PR_ASSERT(eXMLContentSinkState_InDocumentElement == mState);

  FlushText();

  nsCOMPtr<nsIContent> content = PopContent();
  NS_ASSERTION(content, "failed to pop content");
#ifdef DEBUG
  // Check that we're closing the right thing
  nsCOMPtr<nsIAtom> debugNameSpacePrefix, debugTagAtom;
  PRInt32 debugNameSpaceID;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(debugNameSpacePrefix),
                                 getter_AddRefs(debugTagAtom),
                                 &debugNameSpaceID);
  NS_ASSERTION(content->NodeInfo()->Equals(debugTagAtom, debugNameSpaceID),
               "Wrong element being closed");
#endif  

  nsCOMPtr<nsIContent> parent = GetCurrentContent();
  
  result = CloseElement(content, parent, &appendContent);
  NS_ENSURE_SUCCESS(result, result);

  if (mDocElement == content) {
    // XXXbz for roots that don't want to be appended on open, we
    // probably need to deal here.... (and stop appending them on open).
    mState = eXMLContentSinkState_InEpilog;
  }
  else if (appendContent) {
    NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

    parent->AppendChildTo(content, PR_FALSE);
  }

  if (mNeedToBlockParser || (mParser && !mParser->IsParserEnabled())) {
    if (mParser) mParser->BlockParser();
    result = NS_ERROR_HTMLPARSER_BLOCK;
  }

#ifdef MOZ_SVG
  if (content->GetNameSpaceID() == kNameSpaceID_SVG &&
      content->HasAttr(kNameSpaceID_None, nsSVGAtoms::onload)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(PR_TRUE, NS_SVG_LOAD);
    event.eventStructType = NS_SVG_EVENT;
    nsIPresShell *presShell = mDocument->GetShellAt(0);
    if (presShell)
      presShell->HandleDOMEventWithTarget(content, &event, &status);
  }
#endif

  return result;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleComment(const PRUnichar *aName)
{
  FlushText();

  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment), mNodeInfoManager);
  if (comment) {
    nsCOMPtr<nsIDOMComment> domComment = do_QueryInterface(comment, &rv);
    if (domComment) {
      domComment->AppendData(nsDependentString(aName));
      rv = AddContentAsLeaf(comment);
    }
  }

  return rv;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleCDataSection(const PRUnichar *aData, 
                                     PRUint32 aLength)
{
  FlushText();
  
  if (mInTitle) {
    mTitleText.Append(aData, aLength);
  }
  
  nsCOMPtr<nsIContent> cdata;
  nsresult rv = NS_NewXMLCDATASection(getter_AddRefs(cdata), mNodeInfoManager);
  if (cdata) {
    nsCOMPtr<nsIDOMCDATASection> domCDATA = do_QueryInterface(cdata);
    if (domCDATA) {
      domCDATA->SetData(nsDependentString(aData, aLength));
      rv = AddContentAsLeaf(cdata);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsXMLContentSink::HandleDoctypeDecl(const nsAString & aSubset, 
                                    const nsAString & aName, 
                                    const nsAString & aSystemId, 
                                    const nsAString & aPublicId,
                                    nsISupports* aCatalogData)
{
  FlushText();

  nsresult rv = NS_OK;

  nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mDocument));
  if (!doc)
    return NS_OK;

  nsCOMPtr<nsIAtom> name = do_GetAtom(aName);
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  // Create a new doctype node
  nsCOMPtr<nsIDOMDocumentType> docType;
  rv = NS_NewDOMDocumentType(getter_AddRefs(docType), mNodeInfoManager, nsnull,
                             name, nsnull, nsnull, aPublicId, aSystemId,
                             aSubset);
  if (NS_FAILED(rv) || !docType) {
    return rv;
  }

  if (aCatalogData && mCSSLoader && mDocument) {
    // bug 124570 - we only expect additional agent sheets for now -- ignore
    // exit codes, error are not fatal here, just that the stylesheet won't apply
    nsCOMPtr<nsIURI> uri(do_QueryInterface(aCatalogData));
    if (uri) {
      nsCOMPtr<nsICSSStyleSheet> sheet;
      mCSSLoader->LoadSheetSync(uri, getter_AddRefs(sheet));
      
#ifdef NS_DEBUG
      nsCAutoString uriStr;
      uri->GetSpec(uriStr);
      printf("Loading catalog stylesheet: %s ... %s\n", uriStr.get(), sheet.get() ? "Done" : "Failed");
#endif
      if (sheet) {
        mDocument->BeginUpdate(UPDATE_STYLE);
        mDocument->AddCatalogStyleSheet(sheet);
        mDocument->EndUpdate(UPDATE_STYLE);
      }
    }
  }

  nsCOMPtr<nsIDOMNode> tmpNode;
  
  return doc->AppendChild(docType, getter_AddRefs(tmpNode));
}

NS_IMETHODIMP 
nsXMLContentSink::HandleCharacterData(const PRUnichar *aData, 
                                      PRUint32 aLength)
{
  if (aData && mState != eXMLContentSinkState_InProlog &&
      mState != eXMLContentSinkState_InEpilog) {
    return AddText(aData, aLength);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                              const PRUnichar *aData)
{
  FlushText();

  nsresult result = NS_OK;
  const nsDependentString target(aTarget);
  const nsDependentString data(aData);

  nsCOMPtr<nsIContent> node;

  result = NS_NewXMLProcessingInstruction(getter_AddRefs(node),
                                          mNodeInfoManager, target, data);
  if (NS_OK == result) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(node));

    if (ssle) {
      ssle->InitStyleLinkElement(mParser, PR_FALSE);
      ssle->SetEnableUpdates(PR_FALSE);
      mPrettyPrintXML = PR_FALSE;
    }

    result = AddContentAsLeaf(node);

    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      result = ssle->UpdateStyleSheet(nsnull, nsnull);

      if (NS_FAILED(result)) {
        if (result == NS_ERROR_HTMLPARSER_BLOCK && mParser) {
          mParser->BlockParser();
        }
        return result;
      }
    }

    // If it's not a CSS stylesheet PI...
    nsAutoString type;
    nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("type"), type);
    if (mState == eXMLContentSinkState_InProlog && 
        target.EqualsLiteral("xml-stylesheet") && 
        !type.LowerCaseEqualsLiteral("text/css")) {
      nsAutoString href, title, media, alternate;

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("href"), href);
      // If there was no href, we can't do anything with this PI
      if (href.IsEmpty()) {
        return NS_OK;
      }

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("title"), title);
      title.CompressWhitespace();

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("media"), media);
      ToLowerCase(media);

      nsParserUtils::GetQuotedAttributeValue(data, NS_LITERAL_STRING("alternate"), alternate);

      result = ProcessStyleLink(node, href, alternate.EqualsLiteral("yes"),
                                title, type, media);
    }
  }
  return result;
}

NS_IMETHODIMP
nsXMLContentSink::HandleXMLDeclaration(const PRUnichar *aVersion,
                                       const PRUnichar *aEncoding,
                                       PRInt32 aStandalone)
{
  mDocument->SetXMLDeclaration(aVersion, aEncoding, aStandalone);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::ReportError(const PRUnichar* aErrorText, 
                              const PRUnichar* aSourceText)
{
  nsresult rv = NS_OK;
  
  mPrettyPrintXML = PR_FALSE;

  mState = eXMLContentSinkState_InProlog;

  // Clear the current content and
  // prepare to set <parsererror> as the document root
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mDocument));
  if (node) {
    for (;;) {
      nsCOMPtr<nsIDOMNode> child, dummy;
      node->GetLastChild(getter_AddRefs(child));
      if (!child)
        break;
      node->RemoveChild(child, getter_AddRefs(dummy));
    }
  }
  NS_IF_RELEASE(mDocElement); 

  // Clear any buffered-up text we have.  It's enough to set the length to 0.
  // The buffer itself is allocated when we're created and deleted in our
  // destructor, so don't mess with it.
  mTextLength = 0;

  if (mXSLTProcessor) {
    // Get rid of the XSLT processor.
    mXSLTProcessor->CancelLoads();
    mXSLTProcessor = nsnull;
  }

  const PRUnichar* noAtts[] = { 0, 0 };

  NS_NAMED_LITERAL_STRING(errorNs,
                          "http://www.mozilla.org/newlayout/xml/parsererror.xml");

  nsAutoString parsererror(errorNs);
  parsererror.Append((PRUnichar)0xFFFF);
  parsererror.AppendLiteral("parsererror");
  
  rv = HandleStartElement(parsererror.get(), noAtts, 0, -1, (PRUint32)-1);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = HandleCharacterData(aErrorText, nsCRT::strlen(aErrorText));
  NS_ENSURE_SUCCESS(rv,rv);  
  
  nsAutoString sourcetext(errorNs);
  sourcetext.Append((PRUnichar)0xFFFF);
  sourcetext.AppendLiteral("sourcetext");

  rv = HandleStartElement(sourcetext.get(), noAtts, 0, -1, (PRUint32)-1);
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleCharacterData(aSourceText, nsCRT::strlen(aSourceText));
  NS_ENSURE_SUCCESS(rv,rv);
  
  rv = HandleEndElement(sourcetext.get());
  NS_ENSURE_SUCCESS(rv,rv); 
  
  rv = HandleEndElement(parsererror.get());
  NS_ENSURE_SUCCESS(rv,rv);

  return NS_OK;
}

nsresult
nsXMLContentSink::AddAttributes(const PRUnichar** aAtts,
                                nsIContent* aContent)
{
  // Add tag attributes to the content attributes
  nsCOMPtr<nsIAtom> prefix, localName;
  while (*aAtts) {
    PRInt32 nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    // Add attribute to content
    aContent->SetAttr(nameSpaceID, localName, prefix,
                      nsDependentString(aAtts[1]), PR_FALSE);
    aAtts += 2;
  }

  // Give autoloading links a chance to fire
  if (mDocShell && mAllowAutoXLinks) {
    nsCOMPtr<nsIXMLContent> xmlcontent(do_QueryInterface(aContent));
    if (xmlcontent) {
      nsresult rv = xmlcontent->MaybeTriggerAutoLink(mDocShell);
      if (rv == NS_XML_AUTOLINK_REPLACE ||
          rv == NS_XML_AUTOLINK_UNDEFINED) {
        // If we do not terminate the parse, we just keep generating link trigger
        // events. We want to parse only up to the first replace link, and stop.
        mParser->Terminate();
      }
    }
  }

  return NS_OK;
}

#define NS_ACCUMULATION_BUFFER_SIZE 4096

nsresult
nsXMLContentSink::AddText(const PRUnichar* aText, 
                          PRInt32 aLength)
{

  if (mInTitle) {
    mTitleText.Append(aText,aLength);
  }

  // Create buffer when we first need it
  if (0 == mTextSize) {
    mText = (PRUnichar *) PR_MALLOC(sizeof(PRUnichar) * NS_ACCUMULATION_BUFFER_SIZE);
    if (nsnull == mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = NS_ACCUMULATION_BUFFER_SIZE;
  }

  const nsAString& str = Substring(aText, aText+aLength);

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
  PRBool  isLastCharCR = PR_FALSE;
  while (0 != aLength) {
    PRInt32 amount = mTextSize - mTextLength;
    if (amount > aLength) {
      amount = aLength;
    }
    if (0 == amount) {
      if (mConstrainSize) {
        nsresult rv = FlushText();
        if (NS_OK != rv) {
          return rv;
        }
      }
      else {
        mTextSize += aLength;
        mText = (PRUnichar *) PR_REALLOC(mText, sizeof(PRUnichar) * mTextSize);
        if (nsnull == mText) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    }
    mTextLength +=
      nsContentUtils::CopyNewlineNormalizedUnicodeTo(str, 
                                                     offset, 
                                                     &mText[mTextLength], 
                                                     amount,
                                                     isLastCharCR);
    offset  += amount;
    aLength -= amount;
  }

  return NS_OK;
}

nsresult
nsXMLContentSink::ProcessEndSCRIPTTag(nsIContent* aContent,
                                      nsIContent* aParent)
{
  nsresult result = NS_OK;

  mConstrainSize = PR_TRUE; 
  nsCOMPtr<nsIScriptElement> scriptElement(do_QueryInterface(aContent));
  NS_ASSERTION(scriptElement, "null script element in XML content sink");

  scriptElement->SetScriptLineNumber(mScriptLineNo);

  if (!aParent || aParent->GetCurrentDoc() == mDocument) {
    // Assume that we're going to block the parser with a script load.
    // If it's an inline script, we'll be told otherwise in the call
    // to our ScriptAvailable method.
    mScriptElements.AppendObject(scriptElement);
    mNeedToBlockParser = PR_TRUE;
  }

  return result;
}

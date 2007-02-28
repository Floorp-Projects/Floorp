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
 *   Henri Sivonen <hsivonen@iki.fi>
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
#include "nsIContent.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIContent.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDOMComment.h"
#include "nsIDOMCDATASection.h"
#include "nsDOMDocumentType.h"
#include "nsHTMLParts.h"
#include "nsVoidArray.h"
#include "nsCRT.h"
#include "nsICSSLoader.h"
#include "nsICSSStyleSheet.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
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
#include "nsScriptLoader.h"
#include "nsStyleLinkElement.h"
#include "nsIImageLoadingContent.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsICookieService.h"
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"
#include "nsIChannel.h"
#include "nsIPrincipal.h"
#include "nsXMLPrettyPrinter.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsContentErrors.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsNodeUtils.h"
#include "nsIScriptGlobalObject.h"

#ifdef MOZ_SVG
#include "nsGUIEvent.h"
#endif

#define kXSLType "text/xsl"

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
  : mConstrainSize(PR_TRUE),
    mPrettyPrintXML(PR_TRUE),
    mAllowAutoXLinks(PR_TRUE)
{
}

nsXMLContentSink::~nsXMLContentSink()
{
  if (mDocument) {
    // Remove ourselves just to be safe, though we really should have
    // been removed in DidBuildModel if everything worked right.
    mDocument->RemoveObserver(this);
  }

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
  MOZ_TIMER_DEBUGLOG(("Reset and start: nsXMLContentSink::Init(), this=%p\n",
                      this));
  MOZ_TIMER_RESET(mWatch);
  MOZ_TIMER_START(mWatch);
	
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  aDoc->AddObserver(this);

  if (!mDocShell) {
    mPrettyPrintXML = PR_FALSE;
  }
  
  mState = eXMLContentSinkState_InProlog;
  mDocElement = nsnull;

  MOZ_TIMER_DEBUGLOG(("Stop: nsXMLContentSink::Init()\n"));
  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED7(nsXMLContentSink,
                             nsContentSink,
                             nsIContentSink,
                             nsIXMLContentSink,
                             nsIExpatSink,
                             nsITimerCallback,
                             nsIDocumentObserver,
                             nsIMutationObserver,
                             nsITransformObserver)

// nsIContentSink
NS_IMETHODIMP
nsXMLContentSink::WillTokenize(void)
{
  return WillProcessTokensImpl();
}

NS_IMETHODIMP
nsXMLContentSink::WillBuildModel(void)
{
  WillBuildModelImpl();

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

PRBool
nsXMLContentSink::CanStillPrettyPrint()
{
  return mPrettyPrintXML &&
         (!mPrettyPrintHasFactoredElements || mPrettyPrintHasSpecialRoot);
}

nsresult
nsXMLContentSink::MaybePrettyPrint()
{
  if (!CanStillPrettyPrint()) {
    mPrettyPrintXML = PR_FALSE;

    return NS_OK;
  }

  // stop observing in order to avoid crashing when replacing content
  mDocument->RemoveObserver(this);

  // Reenable the CSSLoader so that the prettyprinting stylesheets can load
  if (mCSSLoader) {
    mCSSLoader->SetEnabled(PR_TRUE);
  }
  
  nsCOMPtr<nsXMLPrettyPrinter> printer;
  nsresult rv = NS_NewXMLPrettyPrinter(getter_AddRefs(printer));
  NS_ENSURE_SUCCESS(rv, rv);

  return printer->PrettyPrint(mDocument);
}

static void
CheckXSLTParamPI(nsIDOMProcessingInstruction* aPi,
                 nsIDocumentTransformer* aProcessor,
                 nsIDocument* aDocument)
{
  nsAutoString target, data;
  aPi->GetTarget(target);

  // Check for namespace declarations
  if (target.EqualsLiteral("xslt-param-namespace")) {
    aPi->GetData(data);
    nsAutoString prefix, namespaceAttr;
    nsParserUtils::GetQuotedAttributeValue(data, nsGkAtoms::prefix,
                                           prefix);
    if (!prefix.IsEmpty() &&
        nsParserUtils::GetQuotedAttributeValue(data, nsGkAtoms::_namespace,
                                               namespaceAttr)) {
      aProcessor->AddXSLTParamNamespace(prefix, namespaceAttr);
    }
  }

  // Check for actual parameters
  else if (target.EqualsLiteral("xslt-param")) {
    aPi->GetData(data);
    nsAutoString name, namespaceAttr, select, value;
    nsParserUtils::GetQuotedAttributeValue(data, nsGkAtoms::name,
                                           name);
    nsParserUtils::GetQuotedAttributeValue(data, nsGkAtoms::_namespace,
                                           namespaceAttr);
    if (!nsParserUtils::GetQuotedAttributeValue(data, nsGkAtoms::select, select)) {
      select.SetIsVoid(PR_TRUE);
    }
    if (!nsParserUtils::GetQuotedAttributeValue(data, nsGkAtoms::value, value)) {
      value.SetIsVoid(PR_TRUE);
    }
    if (!name.IsEmpty()) {
      nsCOMPtr<nsIDOMNode> doc = do_QueryInterface(aDocument);
      aProcessor->AddXSLTParam(name, namespaceAttr, select, value, doc);
    }
  }
}

NS_IMETHODIMP
nsXMLContentSink::DidBuildModel()
{
  DidBuildModelImpl();

  if (mXSLTProcessor) {
    // stop observing in order to avoid crashing when replacing content
    mDocument->RemoveObserver(this);

    // Check for xslt-param and xslt-param-namespace PIs
    PRUint32 i;
    nsIContent* child;
    for (i = 0; (child = mDocument->GetChildAt(i)); ++i) {
      if (child->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
        nsCOMPtr<nsIDOMProcessingInstruction> pi = do_QueryInterface(child);
        CheckXSLTParamPI(pi, mXSLTProcessor, mDocument);
      }
      else if (child->IsNodeOfType(nsINode::eELEMENT)) {
        // Only honor PIs in the prolog
        break;
      }
    }

    nsCOMPtr<nsIDOMDocument> currentDOMDoc(do_QueryInterface(mDocument));
    mXSLTProcessor->SetSourceContentModel(currentDOMDoc);
    // Since the processor now holds a reference to us we drop our reference
    // to it to avoid owning cycles
    mXSLTProcessor = nsnull;
  }
  else {
    // Kick off layout for non-XSLT transformed documents.
    nsScriptLoader *loader = mDocument->GetScriptLoader();
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
    }

    // Check if we want to prettyprint
    MaybePrettyPrint();

    StartLayout();

    ScrollToRef();

    mDocument->RemoveObserver(this);

    mDocument->EndLoad();
  }

  DropParserAndPerfHint();

  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::OnDocumentCreated(nsIDocument* aResultDocument)
{
  NS_ENSURE_ARG(aResultDocument);

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aResultDocument);
    return contentViewer->SetDOMDocument(doc);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::OnTransformDone(nsresult aResult,
                                  nsIDocument* aResultDocument)
{
  NS_ASSERTION(NS_FAILED(aResult) || aResultDocument,
               "Don't notify about transform success without a document.");

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aResultDocument);

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));

  if (NS_FAILED(aResult) && contentViewer) {
    // Transform failed.
    if (domDoc) {
      // We have an error document.
      contentViewer->SetDOMDocument(domDoc);
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
    mDocument = aResultDocument;
  }

  nsScriptLoader *loader = originalDocument->GetScriptLoader();
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
    nsNodeUtils::ContentInserted(mDocument, rootContent,
                                 mDocument->IndexOf(rootContent));
    mDocument->EndUpdate(UPDATE_CONTENT_MODEL);
  }
  
  // Start the layout process
  StartLayout();

  ScrollToRef();

  originalDocument->EndLoad();

  return NS_OK;
}


NS_IMETHODIMP
nsXMLContentSink::WillInterrupt(void)
{
  return WillInterruptImpl();
}

NS_IMETHODIMP
nsXMLContentSink::WillResume(void)
{
  return WillResumeImpl();
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

  if (aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XHTML)
#ifdef MOZ_SVG
      || aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_SVG)
#endif
    ) {
    nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(content);
    sele->SetScriptLineNumber(aLineNumber);
    sele->WillCallDoneAddingChildren();
    mConstrainSize = PR_FALSE;
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

  if (aNodeInfo->Equals(nsGkAtoms::title, kNameSpaceID_XHTML)) {
    if (mDocument && mDocument->GetDocumentTitle().IsVoid()) {
      mInTitle = PR_TRUE; // The first title wins
    }
  }
#ifdef MOZ_SVG
  else if (aNodeInfo->Equals(nsGkAtoms::title, kNameSpaceID_SVG)) {
    nsIContent* parent = GetCurrentContent();
    if (mDocument && mDocument->GetDocumentTitle().IsVoid() &&
        parent && parent == mDocElement &&
        parent->NodeInfo()->Equals(nsGkAtoms::svg, kNameSpaceID_SVG)) {
      mInTitle = PR_TRUE; // The first title wins
    }
  }
#endif // MOZ_SVG
  else if (aNodeInfo->Equals(nsGkAtoms::link, kNameSpaceID_XHTML) ||
           aNodeInfo->Equals(nsGkAtoms::style, kNameSpaceID_XHTML) ||
           aNodeInfo->Equals(nsGkAtoms::style, kNameSpaceID_SVG)) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(content));
    if (ssle) {
      ssle->InitStyleLinkElement(mParser, PR_FALSE);
      ssle->SetEnableUpdates(PR_FALSE);
      if (!aNodeInfo->Equals(nsGkAtoms::link, kNameSpaceID_XHTML)) {
        ssle->SetLineNumber(aLineNumber);
      }
    }
  } 

  content.swap(*aResult);

  return NS_OK;
}


nsresult
nsXMLContentSink::CloseElement(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing element to close");

  nsINodeInfo *nodeInfo = aContent->NodeInfo();

  // Some HTML nodes need DoneAddingChildren() called to initialize
  // properly (eg form state restoration).
  if ((nodeInfo->NamespaceID() == kNameSpaceID_XHTML &&
       (nodeInfo->NameAtom() == nsGkAtoms::select ||
        nodeInfo->NameAtom() == nsGkAtoms::textarea ||
        nodeInfo->NameAtom() == nsGkAtoms::object ||
        nodeInfo->NameAtom() == nsGkAtoms::applet))
#ifdef MOZ_XTF
      || nodeInfo->NamespaceID() > kNameSpaceID_LastBuiltin
#endif
      ) {
    aContent->DoneAddingChildren(PR_FALSE);
  }
  
  if (IsMonolithicContainer(nodeInfo)) {
    mInMonolithicContainer--;
  }

  if (!nodeInfo->NamespaceEquals(kNameSpaceID_XHTML) &&
      !nodeInfo->NamespaceEquals(kNameSpaceID_SVG)) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  if (nodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XHTML)
#ifdef MOZ_SVG
      || nodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_SVG)
#endif
    ) {
    mConstrainSize = PR_TRUE; 

    // Now tell the script that it's ready to go. This may execute the script
    // or return NS_ERROR_HTMLPARSER_BLOCK. Or neither if the script doesn't
    // need executing.
    rv = aContent->DoneAddingChildren(PR_TRUE);

    // If the act of insertion evaluated the script, we're fine.
    // Else, block the parser till the script has loaded.
    if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
      nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(aContent);
      mScriptElements.AppendObject(sele);
    }

    // If the parser got blocked, make sure to return the appropriate rv.
    // I'm not sure if this is actually needed or not.
    if (mParser && !mParser->IsParserEnabled()) {
      // XXX The HTML sink doesn't call BlockParser here, why do we?
      mParser->BlockParser();
      rv = NS_ERROR_HTMLPARSER_BLOCK;
    }

    return rv;
  }
  
  if ((nodeInfo->Equals(nsGkAtoms::title, kNameSpaceID_XHTML)
#ifdef MOZ_SVG
       || nodeInfo->Equals(nsGkAtoms::title, kNameSpaceID_SVG)
#endif // MOZ_SVG
      ) && mInTitle) {
    NS_ASSERTION(mDocument, "How did mInTitle get to be true if mDocument is null?");
    // The first title wins
    nsCOMPtr<nsIDOMNSDocument> dom_doc(do_QueryInterface(mDocument));
    mTitleText.CompressWhitespace();
    dom_doc->SetTitle(mTitleText);
    mInTitle = PR_FALSE;
  }
  else if (nodeInfo->Equals(nsGkAtoms::base, kNameSpaceID_XHTML) &&
           !mHasProcessedBase) {
    // The first base wins
    rv = ProcessBASETag(aContent);
    mHasProcessedBase = PR_TRUE;
  }
  else if (nodeInfo->Equals(nsGkAtoms::meta, kNameSpaceID_XHTML) &&
           // Need to check here to make sure this meta tag does not set
           // mPrettyPrintXML to false when we have a special root!
           (!mPrettyPrintXML || !mPrettyPrintHasSpecialRoot)) {
    rv = ProcessMETATag(aContent);
  }
  else if (nodeInfo->Equals(nsGkAtoms::link, kNameSpaceID_XHTML) ||
           nodeInfo->Equals(nsGkAtoms::style, kNameSpaceID_XHTML) ||
           nodeInfo->Equals(nsGkAtoms::style, kNameSpaceID_SVG)) {
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
    NS_ASSERTION(mDocument, "Fragments have no prolog or epilog");
    mDocument->AppendChildTo(aContent, PR_FALSE);
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

  return mXSLTProcessor->LoadStyleSheet(aUrl, loadGroup,
                                        mDocument->NodePrincipal());
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
      CheckLoadURIWithPrincipal(mDocument->NodePrincipal(), url,
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
  
    if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::target, value)) {
      mDocument->SetBaseTarget(value);
    }

    if (aContent->GetAttr(kNameSpaceID_None, nsGkAtoms::href, value)) {
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
      nsCOMPtr<nsIContent> textContent;
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
  if (mContentStack.Length() == 0) {
    return nsnull;
  }
  return GetCurrentStackNode().mContent;
}

StackNode &
nsXMLContentSink::GetCurrentStackNode()
{
  PRInt32 count = mContentStack.Length();
  NS_ASSERTION(count > 0, "Bogus Length()");
  return mContentStack[count-1];
}


nsresult
nsXMLContentSink::PushContent(nsIContent *aContent)
{
  NS_PRECONDITION(aContent, "Null content being pushed!");
  StackNode *sn = mContentStack.AppendElement();
  NS_ENSURE_TRUE(sn, NS_ERROR_OUT_OF_MEMORY);

  sn->mContent = aContent;
  sn->mNumFlushed = 0;
  return NS_OK;
}

void
nsXMLContentSink::PopContent()
{
  PRInt32 count = mContentStack.Length();

  if (count == 0) {
    NS_WARNING("Popping empty stack");
    return;
  }

  mContentStack.RemoveElementAt(count - 1);
}

void
nsXMLContentSink::MaybeStartLayout()
{
  if (mLayoutStarted || mXSLTProcessor || CanStillPrettyPrint()) {
    return;
  }
  StartLayout();
}

void
nsXMLContentSink::StartLayout()
{
  if (mLayoutStarted) {
    return;
  }
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

  aNodeInfo->SetIDAttributeAtom(nsGkAtoms::id);
  
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
       aTagName == nsGkAtoms::bindings) ||
      (aNameSpaceID == kNameSpaceID_XSLT &&
       (aTagName == nsGkAtoms::stylesheet ||
        aTagName == nsGkAtoms::transform))) {
    mPrettyPrintHasSpecialRoot = PR_TRUE;
    if (mPrettyPrintXML) {
      // In this case, disable script execution, stylesheet
      // loading, and auto XLinks since we plan to prettyprint.
      mAllowAutoXLinks = PR_FALSE;
      nsScriptLoader* scriptLoader = mDocument->GetScriptLoader();
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
  nsresult rv = mDocument->AppendChildTo(mDocElement, PR_TRUE);
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
  DidAddContent();

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

    result = PushContent(content);
    NS_ENSURE_SUCCESS(result, result);
  }

  // Some HTML nodes need DoneCreatingElement() called to initialize
  // properly (eg form state restoration).
  if (nodeInfo->NamespaceID() == kNameSpaceID_XHTML &&
      (nodeInfo->NameAtom() == nsGkAtoms::input ||
       nodeInfo->NameAtom() == nsGkAtoms::button)) {
    content->DoneCreatingElement();
  }

  if (IsMonolithicContainer(nodeInfo)) {
    mInMonolithicContainer++;
  }

  MaybeStartLayout();

  return NS_SUCCEEDED(result) ? DidProcessATokenImpl() : result;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleEndElement(const PRUnichar *aName)
{
  nsresult result = NS_OK;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the prolog or epilog, there should be
  // no close tags for elements.
  PR_ASSERT(eXMLContentSinkState_InDocumentElement == mState);

  FlushText();

  StackNode & sn = GetCurrentStackNode();

  nsCOMPtr<nsIContent> content;
  sn.mContent.swap(content);
  PRUint32 numFlushed = sn.mNumFlushed;

  PopContent();
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

  result = CloseElement(content);

  if (mDocElement == content) {
    // XXXbz for roots that don't want to be appended on open, we
    // probably need to deal here.... (and stop appending them on open).
    mState = eXMLContentSinkState_InEpilog;
  }

  PRInt32 stackLen = mContentStack.Length();
  if (mNotifyLevel >= stackLen) {
    if (numFlushed < content->GetChildCount()) {
    	  NotifyAppend(content, numFlushed);
    }
    mNotifyLevel = stackLen - 1;
  }
  DidAddContent();

#ifdef MOZ_SVG
  if (content->GetNameSpaceID() == kNameSpaceID_SVG &&
      content->HasAttr(kNameSpaceID_None, nsGkAtoms::onload)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event(PR_TRUE, NS_SVG_LOAD);
    event.eventStructType = NS_SVG_EVENT;
    nsIPresShell *presShell = mDocument->GetShellAt(0);
    if (presShell) {
      FlushTags();
      presShell->HandleDOMEventWithTarget(content, &event, &status);
    }
  }
#endif

  return NS_SUCCEEDED(result) ? DidProcessATokenImpl() : result;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleComment(const PRUnichar *aName)
{
  FlushText();

  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment), mNodeInfoManager);
  if (comment) {
    comment->SetText(nsDependentString(aName), PR_FALSE);
    rv = AddContentAsLeaf(comment);
    DidAddContent();
  }

  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleCDataSection(const PRUnichar *aData, 
                                     PRUint32 aLength)
{
  // XSLT doesn't differentiate between text and cdata and wants adjacent
  // textnodes merged, so add as text.
  if (mXSLTProcessor) {
    return AddText(aData, aLength);
  }

  FlushText();
  
  if (mInTitle) {
    mTitleText.Append(aData, aLength);
  }
  
  nsCOMPtr<nsIContent> cdata;
  nsresult rv = NS_NewXMLCDATASection(getter_AddRefs(cdata), mNodeInfoManager);
  if (cdata) {
    cdata->SetText(aData, aLength, PR_FALSE);
    rv = AddContentAsLeaf(cdata);
    DidAddContent();
  }

  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
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

  NS_ASSERTION(mDocument, "Shouldn't get here from a document fragment");

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
      mCSSLoader->LoadSheetSync(uri, PR_TRUE, getter_AddRefs(sheet));
      
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

  nsCOMPtr<nsIContent> content = do_QueryInterface(docType);
  NS_ASSERTION(content, "doctype isn't content?");

  rv = mDocument->AppendChildTo(content, PR_FALSE);
  DidAddContent();
  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

NS_IMETHODIMP 
nsXMLContentSink::HandleCharacterData(const PRUnichar *aData, 
                                      PRUint32 aLength)
{
  nsresult rv = NS_OK;
  if (aData && mState != eXMLContentSinkState_InProlog &&
      mState != eXMLContentSinkState_InEpilog) {
    rv = AddText(aData, aLength);
  }
  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

NS_IMETHODIMP
nsXMLContentSink::HandleProcessingInstruction(const PRUnichar *aTarget, 
                                              const PRUnichar *aData)
{
  FlushText();

  const nsDependentString target(aTarget);
  const nsDependentString data(aData);

  nsCOMPtr<nsIContent> node;

  nsresult rv = NS_NewXMLProcessingInstruction(getter_AddRefs(node),
                                               mNodeInfoManager, target, data);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(node));
  if (ssle) {
    ssle->InitStyleLinkElement(mParser, PR_FALSE);
    ssle->SetEnableUpdates(PR_FALSE);
    mPrettyPrintXML = PR_FALSE;
  }

  rv = AddContentAsLeaf(node);
  NS_ENSURE_SUCCESS(rv, rv);
  DidAddContent();

  if (ssle) {
    ssle->SetEnableUpdates(PR_TRUE);
    rv = ssle->UpdateStyleSheet(nsnull, nsnull);

    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_HTMLPARSER_BLOCK && mParser) {
        mParser->BlockParser();
      }
      return rv;
    }
  }

  // If it's not a CSS stylesheet PI...
  nsAutoString type;
  nsParserUtils::GetQuotedAttributeValue(data, nsGkAtoms::type, type);

  if (mState != eXMLContentSinkState_InProlog ||
      !target.EqualsLiteral("xml-stylesheet") ||
      type.LowerCaseEqualsLiteral("text/css")) {
    return DidProcessATokenImpl();
  }

  nsAutoString href, title, media;
  PRBool isAlternate = PR_FALSE;
  ParsePIData(data, href, title, media, isAlternate);

  // If there was no href, we can't do anything with this PI
  if (href.IsEmpty()) {
      return DidProcessATokenImpl();
  }

  rv = ProcessStyleLink(node, href, isAlternate, title, type, media);
  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

/* static */
void
nsXMLContentSink::ParsePIData(const nsString &aData, nsString &aHref,
                              nsString &aTitle, nsString &aMedia,
                              PRBool &aIsAlternate)
{
  nsParserUtils::GetQuotedAttributeValue(aData, nsGkAtoms::href, aHref);

  // If there was no href, we can't do anything with this PI
  if (aHref.IsEmpty()) {
    return;
  }

  nsParserUtils::GetQuotedAttributeValue(aData, nsGkAtoms::title, aTitle);

  nsParserUtils::GetQuotedAttributeValue(aData, nsGkAtoms::media, aMedia);

  nsAutoString alternate;
  nsParserUtils::GetQuotedAttributeValue(aData, nsGkAtoms::alternate, alternate);

  aIsAlternate = alternate.EqualsLiteral("yes");
}

NS_IMETHODIMP
nsXMLContentSink::HandleXMLDeclaration(const PRUnichar *aVersion,
                                       const PRUnichar *aEncoding,
                                       PRInt32 aStandalone)
{
  mDocument->SetXMLDeclaration(aVersion, aEncoding, aStandalone);

  return DidProcessATokenImpl();
}

NS_IMETHODIMP
nsXMLContentSink::ReportError(const PRUnichar* aErrorText, 
                              const PRUnichar* aSourceText,
                              nsIScriptError *aError,
                              PRBool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");
  nsresult rv = NS_OK;

  // The expat driver should report the error.  We're just cleaning up the mess.
  *_retval = PR_TRUE;
  
  mPrettyPrintXML = PR_FALSE;

  mState = eXMLContentSinkState_InProlog;

  // XXX need to stop scripts here -- hsivonen

  // stop observing in order to avoid crashing when removing content
  mDocument->RemoveObserver(this);

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

  // release the nodes on stack
  mContentStack.Clear();
  mNotifyLevel = 0;

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

  FlushTags();

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
    nsresult rv = aContent->MaybeTriggerAutoLink(mDocShell);
    if (rv == NS_XML_AUTOLINK_REPLACE ||
        rv == NS_XML_AUTOLINK_UNDEFINED) {
      // If we do not terminate the parse, we just keep generating link trigger
      // events. We want to parse only up to the first replace link, and stop.
      mParser->Terminate();
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
      // XSLT wants adjacent textnodes merged.
      if (mConstrainSize && !mXSLTProcessor) {
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
    // Line breaks should not be normalized twice. See bug 343870.
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

void
nsXMLContentSink::FlushPendingNotifications(mozFlushType aType)
{
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant)
  if (!mInNotification) {
    if (aType & Flush_SinkNotifications) {
      FlushTags();
    }
    else {
      FlushText();
    }
    if (aType & Flush_OnlyReflow) {
      // Make sure that layout has started so that the reflow flush
      // will actually happen.
      MaybeStartLayout();
    }
  }
}

/**
 * NOTE!! Forked from SinkContext. Please keep in sync.
 *
 * Flush all elements that have been seen so far such that
 * they are visible in the tree. Specifically, make sure
 * that they are all added to their respective parents.
 * Also, do notification at the top for all content that
 * has been newly added so that the frame tree is complete.
 */
nsresult
nsXMLContentSink::FlushTags()
{
  // Don't release last text node in case we need to add to it again
  FlushText();

  ++mInNotification;
  mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, PR_TRUE);

  // Start from the base of the stack (growing downward) and do
  // a notification from the node that is closest to the root of
  // tree for any content that has been added.

  PRInt32 stackPos;
  PRInt32 stackLen = mContentStack.Length();
  PRBool flushed = PR_FALSE;
  PRUint32 childCount;
  nsIContent* content;

  for (stackPos = 0; stackPos < stackLen; ++stackPos) {
    content = mContentStack[stackPos].mContent;
    childCount = content->GetChildCount();

    if (!flushed && (mContentStack[stackPos].mNumFlushed < childCount)) {
      NotifyAppend(content, mContentStack[stackPos].mNumFlushed);
      flushed = PR_TRUE;
    }

    mContentStack[stackPos].mNumFlushed = childCount;
  }
  mNotifyLevel = stackLen - 1;

  --mInNotification;

  return NS_OK;
}

/**
 * NOTE!! Forked from SinkContext. Please keep in sync.
 */
void
nsXMLContentSink::UpdateChildCounts()
{
  // Start from the top of the stack (growing upwards) and see if any
  // new content has been appended. If so, we recognize that reflows
  // have been generated for it and we should make sure that no
  // further reflows occur.  Note that we have to include stackPos == 0
  // to properly notify on kids of <html>.
  PRInt32 stackLen = mContentStack.Length();
  PRInt32 stackPos = stackLen - 1;
  while (stackPos >= 0) {
    StackNode & node = mContentStack[stackPos];
    node.mNumFlushed = node.mContent->GetChildCount();

    stackPos--;
  }
  mNotifyLevel = stackLen - 1;
}

PRBool
nsXMLContentSink::IsMonolithicContainer(nsINodeInfo* aNodeInfo)
{
  return ((aNodeInfo->NamespaceID() == kNameSpaceID_XHTML &&
          (aNodeInfo->NameAtom() == nsGkAtoms::tr ||
           aNodeInfo->NameAtom() == nsGkAtoms::select ||
           aNodeInfo->NameAtom() == nsGkAtoms::object ||
           aNodeInfo->NameAtom() == nsGkAtoms::applet))
#ifdef MOZ_MATHML
       || (aNodeInfo->NamespaceID() == kNameSpaceID_MathML &&
          (aNodeInfo->NameAtom() == nsGkAtoms::math))
#endif
          );
}

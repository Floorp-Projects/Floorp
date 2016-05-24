/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsXMLContentSink.h"
#include "nsIParser.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIContent.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIDocShell.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIDOMComment.h"
#include "nsIDOMCDATASection.h"
#include "DocumentType.h"
#include "nsHTMLParts.h"
#include "nsCRT.h"
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/css/Loader.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIScriptContext.h"
#include "nsNameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsIContentViewer.h"
#include "prtime.h"
#include "mozilla/Logging.h"
#include "prmem.h"
#include "nsRect.h"
#include "nsIWebNavigation.h"
#include "nsIScriptElement.h"
#include "nsScriptLoader.h"
#include "nsStyleLinkElement.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsICookieService.h"
#include "nsIPrompt.h"
#include "nsIChannel.h"
#include "nsIPrincipal.h"
#include "nsXMLPrettyPrinter.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsError.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsNodeUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIHTMLDocument.h"
#include "mozAutoDocUpdate.h"
#include "nsMimeTypes.h"
#include "nsHtml5SVGLoadDispatcher.h"
#include "nsTextNode.h"
#include "mozilla/dom/CDATASection.h"
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/ProcessingInstruction.h"

using namespace mozilla;
using namespace mozilla::dom;

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
  NS_PRECONDITION(nullptr != aResult, "null ptr");
  if (nullptr == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  RefPtr<nsXMLContentSink> it = new nsXMLContentSink();

  nsresult rv = it->Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  it.forget(aResult);
  return NS_OK;
}

nsXMLContentSink::nsXMLContentSink()
  : mPrettyPrintXML(true)
{
}

nsXMLContentSink::~nsXMLContentSink()
{
}

nsresult
nsXMLContentSink::Init(nsIDocument* aDoc,
                       nsIURI* aURI,
                       nsISupports* aContainer,
                       nsIChannel* aChannel)
{
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  aDoc->AddObserver(this);
  mIsDocumentObserver = true;

  if (!mDocShell) {
    mPrettyPrintXML = false;
  }

  mState = eXMLContentSinkState_InProlog;
  mDocElement = nullptr;

  return NS_OK;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsXMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIXMLContentSink)
  NS_INTERFACE_MAP_ENTRY(nsIExpatSink)
  NS_INTERFACE_MAP_ENTRY(nsITransformObserver)
NS_INTERFACE_MAP_END_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(nsXMLContentSink, nsContentSink)
NS_IMPL_RELEASE_INHERITED(nsXMLContentSink, nsContentSink)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLContentSink)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsXMLContentSink,
                                                  nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCurrentHead)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocElement)
  for (uint32_t i = 0, count = tmp->mContentStack.Length(); i < count; i++) {
    const StackNode& node = tmp->mContentStack.ElementAt(i);
    cb.NoteXPCOMChild(node.mContent);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// nsIContentSink
NS_IMETHODIMP
nsXMLContentSink::WillParse(void)
{
  return WillParseImpl();
}

NS_IMETHODIMP
nsXMLContentSink::WillBuildModel(nsDTDMode aDTDMode)
{
  WillBuildModelImpl();

  // Notify document that the load is beginning
  mDocument->BeginLoad();

  // Check for correct load-command for maybe prettyprinting
  if (mPrettyPrintXML) {
    nsAutoCString command;
    GetParser()->GetCommand(command);
    if (!command.EqualsLiteral("view")) {
      mPrettyPrintXML = false;
    }
  }

  return NS_OK;
}

bool
nsXMLContentSink::CanStillPrettyPrint()
{
  return mPrettyPrintXML &&
         (!mPrettyPrintHasFactoredElements || mPrettyPrintHasSpecialRoot);
}

nsresult
nsXMLContentSink::MaybePrettyPrint()
{
  if (!CanStillPrettyPrint()) {
    mPrettyPrintXML = false;

    return NS_OK;
  }

  // stop observing in order to avoid crashing when replacing content
  mDocument->RemoveObserver(this);
  mIsDocumentObserver = false;

  // Reenable the CSSLoader so that the prettyprinting stylesheets can load
  if (mCSSLoader) {
    mCSSLoader->SetEnabled(true);
  }

  RefPtr<nsXMLPrettyPrinter> printer;
  nsresult rv = NS_NewXMLPrettyPrinter(getter_AddRefs(printer));
  NS_ENSURE_SUCCESS(rv, rv);

  bool isPrettyPrinting;
  rv = printer->PrettyPrint(mDocument, &isPrettyPrinting);
  NS_ENSURE_SUCCESS(rv, rv);

  mPrettyPrinting = isPrettyPrinting;
  return NS_OK;
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
    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::prefix,
                                            prefix);
    if (!prefix.IsEmpty() &&
        nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::_namespace,
                                                namespaceAttr)) {
      aProcessor->AddXSLTParamNamespace(prefix, namespaceAttr);
    }
  }

  // Check for actual parameters
  else if (target.EqualsLiteral("xslt-param")) {
    aPi->GetData(data);
    nsAutoString name, namespaceAttr, select, value;
    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::name,
                                            name);
    nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::_namespace,
                                            namespaceAttr);
    if (!nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::select, select)) {
      select.SetIsVoid(true);
    }
    if (!nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::value, value)) {
      value.SetIsVoid(true);
    }
    if (!name.IsEmpty()) {
      nsCOMPtr<nsIDOMNode> doc = do_QueryInterface(aDocument);
      aProcessor->AddXSLTParam(name, namespaceAttr, select, value, doc);
    }
  }
}

NS_IMETHODIMP
nsXMLContentSink::DidBuildModel(bool aTerminated)
{
  if (!mParser) {
    // If mParser is null, this parse has already been terminated and must
    // not been terminated again. However, nsDocument may still think that
    // the parse has not been terminated and call back into here in the case
    // where the XML parser has finished but the XSLT transform associated
    // with the document has not.
    return NS_OK;
  }

  DidBuildModelImpl(aTerminated);

  if (mXSLTProcessor) {
    // stop observing in order to avoid crashing when replacing content
    mDocument->RemoveObserver(this);
    mIsDocumentObserver = false;

    // Check for xslt-param and xslt-param-namespace PIs
    for (nsIContent* child = mDocument->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      if (child->IsNodeOfType(nsINode::ePROCESSING_INSTRUCTION)) {
        nsCOMPtr<nsIDOMProcessingInstruction> pi = do_QueryInterface(child);
        CheckXSLTParamPI(pi, mXSLTProcessor, mDocument);
      }
      else if (child->IsElement()) {
        // Only honor PIs in the prolog
        break;
      }
    }

    nsCOMPtr<nsIDOMDocument> currentDOMDoc(do_QueryInterface(mDocument));
    mXSLTProcessor->SetSourceContentModel(currentDOMDoc);
    // Since the processor now holds a reference to us we drop our reference
    // to it to avoid owning cycles
    mXSLTProcessor = nullptr;
  }
  else {
    // Kick off layout for non-XSLT transformed documents.

    // Check if we want to prettyprint
    MaybePrettyPrint();

    bool startLayout = true;

    if (mPrettyPrinting) {
      NS_ASSERTION(!mPendingSheetCount, "Shouldn't have pending sheets here!");

      // We're pretty-printing now.  See whether we should wait up on
      // stylesheet loads
      if (mDocument->CSSLoader()->HasPendingLoads() &&
          NS_SUCCEEDED(mDocument->CSSLoader()->AddObserver(this))) {
        // wait for those sheets to load
        startLayout = false;
      }
    }

    if (startLayout) {
      StartLayout(false);

      ScrollToRef();
    }

    mDocument->RemoveObserver(this);
    mIsDocumentObserver = false;

    mDocument->EndLoad();
  }

  DropParserAndPerfHint();

  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::OnDocumentCreated(nsIDocument* aResultDocument)
{
  NS_ENSURE_ARG(aResultDocument);

  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(aResultDocument);
  if (htmlDoc) {
    htmlDoc->SetDocWriteDisabled(true);
  }

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer) {
    return contentViewer->SetDocumentInternal(aResultDocument, true);
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
      aResultDocument->SetMayStartLayout(false);
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
    nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
    if (htmlDoc) {
      htmlDoc->SetDocWriteDisabled(false);
    }
  }

  // Notify document observers that all the content has been stuck
  // into the document.
  // XXX do we need to notify for things like PIs?  Or just the
  // documentElement?
  nsIContent *rootElement = mDocument->GetRootElement();
  if (rootElement) {
    NS_ASSERTION(mDocument->IndexOf(rootElement) != -1,
                 "rootElement not in doc?");
    mDocument->BeginUpdate(UPDATE_CONTENT_MODEL);
    nsNodeUtils::ContentInserted(mDocument, rootElement,
                                 mDocument->IndexOf(rootElement));
    mDocument->EndUpdate(UPDATE_CONTENT_MODEL);
  }

  // Start the layout process
  StartLayout(false);

  ScrollToRef();

  originalDocument->EndLoad();

  return NS_OK;
}

NS_IMETHODIMP
nsXMLContentSink::StyleSheetLoaded(StyleSheetHandle aSheet,
                                   bool aWasAlternate,
                                   nsresult aStatus)
{
  if (!mPrettyPrinting) {
    return nsContentSink::StyleSheetLoaded(aSheet, aWasAlternate, aStatus);
  }

  if (!mDocument->CSSLoader()->HasPendingLoads()) {
    mDocument->CSSLoader()->RemoveObserver(this);
    StartLayout(false);
    ScrollToRef();
  }

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
nsXMLContentSink::SetParser(nsParserBase* aParser)
{
  NS_PRECONDITION(aParser, "Should have a parser here!");
  mParser = aParser;
  return NS_OK;
}

nsresult
nsXMLContentSink::CreateElement(const char16_t** aAtts, uint32_t aAttsCount,
                                mozilla::dom::NodeInfo* aNodeInfo, uint32_t aLineNumber,
                                nsIContent** aResult, bool* aAppendContent,
                                FromParser aFromParser)
{
  NS_ASSERTION(aNodeInfo, "can't create element without nodeinfo");

  *aResult = nullptr;
  *aAppendContent = true;
  nsresult rv = NS_OK;

  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  RefPtr<Element> content;
  rv = NS_NewElement(getter_AddRefs(content), ni.forget(), aFromParser);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XHTML)
      || aNodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_SVG)
    ) {
    nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(content);
    sele->SetScriptLineNumber(aLineNumber);
    sele->SetCreatorParser(GetParser());
  }

  // XHTML needs some special attention
  if (aNodeInfo->NamespaceEquals(kNameSpaceID_XHTML)) {
    mPrettyPrintHasFactoredElements = true;
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
      content.forget(aResult);

      return NS_OK;
    }
  }

  if (aNodeInfo->Equals(nsGkAtoms::link, kNameSpaceID_XHTML) ||
      aNodeInfo->Equals(nsGkAtoms::style, kNameSpaceID_XHTML) ||
      aNodeInfo->Equals(nsGkAtoms::style, kNameSpaceID_SVG)) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(content));
    if (ssle) {
      ssle->InitStyleLinkElement(false);
      if (aFromParser) {
        ssle->SetEnableUpdates(false);
      }
      if (!aNodeInfo->Equals(nsGkAtoms::link, kNameSpaceID_XHTML)) {
        ssle->SetLineNumber(aFromParser ? aLineNumber : 0);
      }
    }
  }

  content.forget(aResult);

  return NS_OK;
}


nsresult
nsXMLContentSink::CloseElement(nsIContent* aContent)
{
  NS_ASSERTION(aContent, "missing element to close");

  mozilla::dom::NodeInfo *nodeInfo = aContent->NodeInfo();

  // Some HTML nodes need DoneAddingChildren() called to initialize
  // properly (eg form state restoration).
  if ((nodeInfo->NamespaceID() == kNameSpaceID_XHTML &&
       (nodeInfo->NameAtom() == nsGkAtoms::select ||
        nodeInfo->NameAtom() == nsGkAtoms::textarea ||
        nodeInfo->NameAtom() == nsGkAtoms::video ||
        nodeInfo->NameAtom() == nsGkAtoms::audio ||
        nodeInfo->NameAtom() == nsGkAtoms::object ||
        nodeInfo->NameAtom() == nsGkAtoms::applet))
      || nodeInfo->NameAtom() == nsGkAtoms::title
      ) {
    aContent->DoneAddingChildren(HaveNotifiedForCurrentContent());
  }

  if (IsMonolithicContainer(nodeInfo)) {
    mInMonolithicContainer--;
  }

  if (!nodeInfo->NamespaceEquals(kNameSpaceID_XHTML) &&
      !nodeInfo->NamespaceEquals(kNameSpaceID_SVG)) {
    return NS_OK;
  }

  if (nodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_XHTML)
      || nodeInfo->Equals(nsGkAtoms::script, kNameSpaceID_SVG)
    ) {
    nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(aContent);

    if (mPreventScriptExecution) {
      sele->PreventExecution();
      return NS_OK;
    }

    // Always check the clock in nsContentSink right after a script
    StopDeflecting();

    // Now tell the script that it's ready to go. This may execute the script
    // or return true, or neither if the script doesn't need executing.
    bool block = sele->AttemptToExecute();

    // If the parser got blocked, make sure to return the appropriate rv.
    // I'm not sure if this is actually needed or not.
    if (mParser && !mParser->IsParserEnabled()) {
      // XXX The HTML sink doesn't call BlockParser here, why do we?
      GetParser()->BlockParser();
      block = true;
    }

    return block ? NS_ERROR_HTMLPARSER_BLOCK : NS_OK;
  }

  nsresult rv = NS_OK;
  if (nodeInfo->Equals(nsGkAtoms::meta, kNameSpaceID_XHTML) &&
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
      ssle->SetEnableUpdates(true);
      bool willNotify;
      bool isAlternate;
      rv = ssle->UpdateStyleSheet(mRunsToCompletion ? nullptr : this,
                                  &willNotify,
                                  &isAlternate);
      if (NS_SUCCEEDED(rv) && willNotify && !isAlternate && !mRunsToCompletion) {
        ++mPendingSheetCount;
        mScriptLoader->AddParserBlockingScriptExecutionBlocker();
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
    mDocument->AppendChildTo(aContent, false);
  }
  else {
    nsCOMPtr<nsIContent> parent = GetCurrentContent();

    if (parent) {
      result = parent->AppendChildTo(aContent, false);
    }
  }
  return result;
}

// Create an XML parser and an XSL content sink and start parsing
// the XSL stylesheet located at the given URI.
nsresult
nsXMLContentSink::LoadXSLStyleSheet(nsIURI* aUrl)
{
  nsCOMPtr<nsIDocumentTransformer> processor =
    do_CreateInstance("@mozilla.org/document-transformer;1?type=xslt");
  if (!processor) {
    // No XSLT processor available, continue normal document loading
    return NS_OK;
  }

  processor->SetTransformObserver(this);

  if (NS_SUCCEEDED(processor->LoadStyleSheet(aUrl, mDocument))) {
    mXSLTProcessor.swap(processor);
  }

  // Intentionally ignore errors here, we should continue loading the
  // XML document whether we're able to load the XSLT stylesheet or
  // not.

  return NS_OK;
}

nsresult
nsXMLContentSink::ProcessStyleLink(nsIContent* aElement,
                                   const nsSubstring& aHref,
                                   bool aAlternate,
                                   const nsSubstring& aTitle,
                                   const nsSubstring& aType,
                                   const nsSubstring& aMedia)
{
  nsresult rv = NS_OK;
  mPrettyPrintXML = false;

  nsAutoCString cmd;
  if (mParser)
    GetParser()->GetCommand(cmd);
  if (cmd.EqualsASCII(kLoadAsData))
    return NS_OK; // Do not load stylesheets when loading as data

  NS_ConvertUTF16toUTF8 type(aType);
  if (type.EqualsIgnoreCase(TEXT_XSL) ||
      type.EqualsIgnoreCase(APPLICATION_XSLT_XML) ||
      type.EqualsIgnoreCase(TEXT_XML) ||
      type.EqualsIgnoreCase(APPLICATION_XML)) {
    if (aAlternate) {
      // don't load alternate XSLT
      return NS_OK;
    }
    // LoadXSLStyleSheet needs a mDocShell.
    if (!mDocShell)
      return NS_OK;

    nsCOMPtr<nsIURI> url;
    rv = NS_NewURI(getter_AddRefs(url), aHref, nullptr,
                   mDocument->GetDocBaseURI());
    NS_ENSURE_SUCCESS(rv, rv);

    // Do security check
    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
    rv = secMan->
      CheckLoadURIWithPrincipal(mDocument->NodePrincipal(), url,
                                nsIScriptSecurityManager::ALLOW_CHROME);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    // Do content policy check
    int16_t decision = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_XSLT,
                                   url,
                                   mDocument->NodePrincipal(),
                                   aElement,
                                   type,
                                   nullptr,
                                   &decision,
                                   nsContentUtils::GetContentPolicy(),
                                   nsContentUtils::GetSecurityManager());

    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_CP_REJECTED(decision)) {
      return NS_OK;
    }

    return LoadXSLStyleSheet(url);
  }

  // Let nsContentSink deal with css.
  rv = nsContentSink::ProcessStyleLink(aElement, aHref, aAlternate,
                                       aTitle, aType, aMedia);

  // nsContentSink::ProcessStyleLink handles the bookkeeping here wrt
  // pending sheets.

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

bool
nsXMLContentSink::IsScriptExecuting()
{
  return IsScriptExecutingImpl();
}

nsresult
nsXMLContentSink::FlushText(bool aReleaseTextNode)
{
  nsresult rv = NS_OK;

  if (mTextLength != 0) {
    if (mLastTextNode) {
      bool notify = HaveNotifiedForCurrentContent();
      // We could probably always increase mInNotification here since
      // if AppendText doesn't notify it shouldn't trigger evil code.
      // But just in case it does, we don't want to mask any notifications.
      if (notify) {
        ++mInNotification;
      }
      rv = mLastTextNode->AppendText(mText, mTextLength, notify);
      if (notify) {
        --mInNotification;
      }

      mTextLength = 0;
    } else {
      RefPtr<nsTextNode> textContent = new nsTextNode(mNodeInfoManager);

      mLastTextNode = textContent;

      // Set the text in the text node
      textContent->SetText(mText, mTextLength, false);
      mTextLength = 0;

      // Add text to its parent
      rv = AddContentAsLeaf(textContent);
    }
  }

  if (aReleaseTextNode) {
    mLastTextNode = nullptr;
  }

  return rv;
}

nsIContent*
nsXMLContentSink::GetCurrentContent()
{
  if (mContentStack.Length() == 0) {
    return nullptr;
  }
  return GetCurrentStackNode()->mContent;
}

StackNode*
nsXMLContentSink::GetCurrentStackNode()
{
  int32_t count = mContentStack.Length();
  return count != 0 ? &mContentStack[count-1] : nullptr;
}


nsresult
nsXMLContentSink::PushContent(nsIContent *aContent)
{
  NS_PRECONDITION(aContent, "Null content being pushed!");
  StackNode *sn = mContentStack.AppendElement();
  NS_ENSURE_TRUE(sn, NS_ERROR_OUT_OF_MEMORY);

  nsIContent* contentToPush = aContent;

  // When an XML parser would append a node to a template element, it
  // must instead append it to the template element's template contents.
  if (contentToPush->IsHTMLElement(nsGkAtoms::_template)) {
    HTMLTemplateElement* templateElement =
      static_cast<HTMLTemplateElement*>(contentToPush);
    contentToPush = templateElement->Content();
  }

  sn->mContent = contentToPush;
  sn->mNumFlushed = 0;
  return NS_OK;
}

void
nsXMLContentSink::PopContent()
{
  int32_t count = mContentStack.Length();

  if (count == 0) {
    NS_WARNING("Popping empty stack");
    return;
  }

  mContentStack.RemoveElementAt(count - 1);
}

bool
nsXMLContentSink::HaveNotifiedForCurrentContent() const
{
  uint32_t stackLength = mContentStack.Length();
  if (stackLength) {
    const StackNode& stackNode = mContentStack[stackLength - 1];
    nsIContent* parent = stackNode.mContent;
    return stackNode.mNumFlushed == parent->GetChildCount();
  }
  return true;
}

void
nsXMLContentSink::MaybeStartLayout(bool aIgnorePendingSheets)
{
  // XXXbz if aIgnorePendingSheets is true, what should we do when
  // mXSLTProcessor or CanStillPrettyPrint()?
  if (mLayoutStarted || mXSLTProcessor || CanStillPrettyPrint()) {
    return;
  }
  StartLayout(aIgnorePendingSheets);
}

////////////////////////////////////////////////////////////////////////

bool
nsXMLContentSink::SetDocElement(int32_t aNameSpaceID,
                                nsIAtom* aTagName,
                                nsIContent *aContent)
{
  if (mDocElement)
    return false;

  // check for root elements that needs special handling for
  // prettyprinting
  if ((aNameSpaceID == kNameSpaceID_XBL &&
       aTagName == nsGkAtoms::bindings) ||
      (aNameSpaceID == kNameSpaceID_XSLT &&
       (aTagName == nsGkAtoms::stylesheet ||
        aTagName == nsGkAtoms::transform))) {
    mPrettyPrintHasSpecialRoot = true;
    if (mPrettyPrintXML) {
      // In this case, disable script execution, stylesheet
      // loading, and auto XLinks since we plan to prettyprint.
      mDocument->ScriptLoader()->SetEnabled(false);
      if (mCSSLoader) {
        mCSSLoader->SetEnabled(false);
      }
    }
  }

  mDocElement = aContent;
  nsresult rv = mDocument->AppendChildTo(mDocElement, NotifyForDocElement());
  if (NS_FAILED(rv)) {
    // If we return false here, the caller will bail out because it won't
    // find a parent content node to append to, which is fine.
    return false;
  }

  if (aTagName == nsGkAtoms::html &&
      aNameSpaceID == kNameSpaceID_XHTML) {
    ProcessOfflineManifest(aContent);
  }

  return true;
}

NS_IMETHODIMP
nsXMLContentSink::HandleStartElement(const char16_t *aName,
                                     const char16_t **aAtts,
                                     uint32_t aAttsCount,
                                     uint32_t aLineNumber)
{
  return HandleStartElement(aName, aAtts, aAttsCount, aLineNumber,
                            true);
}

nsresult
nsXMLContentSink::HandleStartElement(const char16_t *aName,
                                     const char16_t **aAtts,
                                     uint32_t aAttsCount,
                                     uint32_t aLineNumber,
                                     bool aInterruptable)
{
  NS_PRECONDITION(aAttsCount % 2 == 0, "incorrect aAttsCount");
  // Adjust aAttsCount so it's the actual number of attributes
  aAttsCount /= 2;

  nsresult result = NS_OK;
  bool appendContent = true;
  nsCOMPtr<nsIContent> content;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the epilog, there should be no
  // new elements
  PR_ASSERT(eXMLContentSinkState_InEpilog != mState);

  FlushText();
  DidAddContent();

  mState = eXMLContentSinkState_InDocumentElement;

  int32_t nameSpaceID;
  nsCOMPtr<nsIAtom> prefix, localName;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                 getter_AddRefs(localName), &nameSpaceID);

  if (!OnOpenContainer(aAtts, aAttsCount, nameSpaceID, localName, aLineNumber)) {
    return NS_OK;
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID,
                                           nsIDOMNode::ELEMENT_NODE);

  result = CreateElement(aAtts, aAttsCount, nodeInfo, aLineNumber,
                         getter_AddRefs(content), &appendContent,
                         FROM_PARSER_NETWORK);
  NS_ENSURE_SUCCESS(result, result);

  // Have to do this before we push the new content on the stack... and have to
  // do that before we set attributes, call BindToTree, etc.  Ideally we'd push
  // on the stack inside CreateElement (which is effectively what the HTML sink
  // does), but that's hard with all the subclass overrides going on.
  nsCOMPtr<nsIContent> parent = GetCurrentContent();

  result = PushContent(content);
  NS_ENSURE_SUCCESS(result, result);

  // Set the attributes on the new content element
  result = AddAttributes(aAtts, content);

  if (NS_OK == result) {
    // Store the element
    if (!SetDocElement(nameSpaceID, localName, content) && appendContent) {
      NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

      parent->AppendChildTo(content, false);
    }
  }

  // Some HTML nodes need DoneCreatingElement() called to initialize
  // properly (eg form state restoration).
  if (nodeInfo->NamespaceID() == kNameSpaceID_XHTML) {
    if (nodeInfo->NameAtom() == nsGkAtoms::input ||
        nodeInfo->NameAtom() == nsGkAtoms::button ||
        nodeInfo->NameAtom() == nsGkAtoms::menuitem ||
        nodeInfo->NameAtom() == nsGkAtoms::audio ||
        nodeInfo->NameAtom() == nsGkAtoms::video) {
      content->DoneCreatingElement();
    } else if (nodeInfo->NameAtom() == nsGkAtoms::head && !mCurrentHead) {
      mCurrentHead = content;
    }
  }

  if (IsMonolithicContainer(nodeInfo)) {
    mInMonolithicContainer++;
  }

  if (content != mDocElement && !mCurrentHead) {
    // This isn't the root and we're not inside an XHTML <head>.
    // Might need to start layout
    MaybeStartLayout(false);
  }

  if (content == mDocElement) {
    NotifyDocElementCreated(mDocument);
  }

  return aInterruptable && NS_SUCCEEDED(result) ? DidProcessATokenImpl() :
                                                  result;
}

NS_IMETHODIMP
nsXMLContentSink::HandleEndElement(const char16_t *aName)
{
  return HandleEndElement(aName, true);
}

nsresult
nsXMLContentSink::HandleEndElement(const char16_t *aName,
                                   bool aInterruptable)
{
  nsresult result = NS_OK;

  // XXX Hopefully the parser will flag this before we get
  // here. If we're in the prolog or epilog, there should be
  // no close tags for elements.
  PR_ASSERT(eXMLContentSinkState_InDocumentElement == mState);

  FlushText();

  StackNode* sn = GetCurrentStackNode();
  if (!sn) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIContent> content;
  sn->mContent.swap(content);
  uint32_t numFlushed = sn->mNumFlushed;

  PopContent();
  NS_ASSERTION(content, "failed to pop content");
#ifdef DEBUG
  // Check that we're closing the right thing
  nsCOMPtr<nsIAtom> debugNameSpacePrefix, debugTagAtom;
  int32_t debugNameSpaceID;
  nsContentUtils::SplitExpatName(aName, getter_AddRefs(debugNameSpacePrefix),
                                 getter_AddRefs(debugTagAtom),
                                 &debugNameSpaceID);
  // Check if we are closing a template element because template
  // elements do not get pushed on the stack, the template
  // element content is pushed instead.
  bool isTemplateElement = debugTagAtom == nsGkAtoms::_template &&
                           debugNameSpaceID == kNameSpaceID_XHTML;
  NS_ASSERTION(content->NodeInfo()->Equals(debugTagAtom, debugNameSpaceID) ||
               isTemplateElement, "Wrong element being closed");
#endif

  result = CloseElement(content);

  if (mCurrentHead == content) {
    mCurrentHead = nullptr;
  }

  if (mDocElement == content) {
    // XXXbz for roots that don't want to be appended on open, we
    // probably need to deal here.... (and stop appending them on open).
    mState = eXMLContentSinkState_InEpilog;

    // We might have had no occasion to start layout yet.  Do so now.
    MaybeStartLayout(false);
  }

  int32_t stackLen = mContentStack.Length();
  if (mNotifyLevel >= stackLen) {
    if (numFlushed < content->GetChildCount()) {
      NotifyAppend(content, numFlushed);
    }
    mNotifyLevel = stackLen - 1;
  }
  DidAddContent();

  if (content->IsSVGElement(nsGkAtoms::svg)) {
    FlushTags();
    nsCOMPtr<nsIRunnable> event = new nsHtml5SVGLoadDispatcher(content);
    if (NS_FAILED(NS_DispatchToMainThread(event))) {
      NS_WARNING("failed to dispatch svg load dispatcher");
    }
  }

  return aInterruptable && NS_SUCCEEDED(result) ? DidProcessATokenImpl() :
                                                  result;
}

NS_IMETHODIMP
nsXMLContentSink::HandleComment(const char16_t *aName)
{
  FlushText();

  RefPtr<Comment> comment = new Comment(mNodeInfoManager);
  comment->SetText(nsDependentString(aName), false);
  nsresult rv = AddContentAsLeaf(comment);
  DidAddContent();

  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

NS_IMETHODIMP
nsXMLContentSink::HandleCDataSection(const char16_t *aData,
                                     uint32_t aLength)
{
  // XSLT doesn't differentiate between text and cdata and wants adjacent
  // textnodes merged, so add as text.
  if (mXSLTProcessor) {
    return AddText(aData, aLength);
  }

  FlushText();

  RefPtr<CDATASection> cdata = new CDATASection(mNodeInfoManager);
  cdata->SetText(aData, aLength, false);
  nsresult rv = AddContentAsLeaf(cdata);
  DidAddContent();

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

  nsCOMPtr<nsIAtom> name = NS_Atomize(aName);
  NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);

  // Create a new doctype node
  nsCOMPtr<nsIDOMDocumentType> docType;
  rv = NS_NewDOMDocumentType(getter_AddRefs(docType), mNodeInfoManager,
                             name, aPublicId, aSystemId, aSubset);
  if (NS_FAILED(rv) || !docType) {
    return rv;
  }

  MOZ_ASSERT(!aCatalogData, "Need to add back support for catalog style "
                            "sheets");

  nsCOMPtr<nsIContent> content = do_QueryInterface(docType);
  NS_ASSERTION(content, "doctype isn't content?");

  rv = mDocument->AppendChildTo(content, false);
  DidAddContent();
  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

NS_IMETHODIMP
nsXMLContentSink::HandleCharacterData(const char16_t *aData,
                                      uint32_t aLength)
{
  return HandleCharacterData(aData, aLength, true);
}

nsresult
nsXMLContentSink::HandleCharacterData(const char16_t *aData, uint32_t aLength,
                                      bool aInterruptable)
{
  nsresult rv = NS_OK;
  if (aData && mState != eXMLContentSinkState_InProlog &&
      mState != eXMLContentSinkState_InEpilog) {
    rv = AddText(aData, aLength);
  }
  return aInterruptable && NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

NS_IMETHODIMP
nsXMLContentSink::HandleProcessingInstruction(const char16_t *aTarget,
                                              const char16_t *aData)
{
  FlushText();

  const nsDependentString target(aTarget);
  const nsDependentString data(aData);

  nsCOMPtr<nsIContent> node =
    NS_NewXMLProcessingInstruction(mNodeInfoManager, target, data);

  nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(node));
  if (ssle) {
    ssle->InitStyleLinkElement(false);
    ssle->SetEnableUpdates(false);
    mPrettyPrintXML = false;
  }

  nsresult rv = AddContentAsLeaf(node);
  NS_ENSURE_SUCCESS(rv, rv);
  DidAddContent();

  if (ssle) {
    // This is an xml-stylesheet processing instruction... but it might not be
    // a CSS one if the type is set to something else.
    ssle->SetEnableUpdates(true);
    bool willNotify;
    bool isAlternate;
    rv = ssle->UpdateStyleSheet(mRunsToCompletion ? nullptr : this,
                                &willNotify,
                                &isAlternate);
    NS_ENSURE_SUCCESS(rv, rv);

    if (willNotify) {
      // Successfully started a stylesheet load
      if (!isAlternate && !mRunsToCompletion) {
        ++mPendingSheetCount;
        mScriptLoader->AddParserBlockingScriptExecutionBlocker();
      }

      return NS_OK;
    }
  }

  // If it's not a CSS stylesheet PI...
  nsAutoString type;
  nsContentUtils::GetPseudoAttributeValue(data, nsGkAtoms::type, type);

  if (mState != eXMLContentSinkState_InProlog ||
      !target.EqualsLiteral("xml-stylesheet") ||
      type.IsEmpty()                          ||
      type.LowerCaseEqualsLiteral("text/css")) {
    return DidProcessATokenImpl();
  }

  nsAutoString href, title, media;
  bool isAlternate = false;

  // If there was no href, we can't do anything with this PI
  if (!ParsePIData(data, href, title, media, isAlternate)) {
      return DidProcessATokenImpl();
  }

  rv = ProcessStyleLink(node, href, isAlternate, title, type, media);
  return NS_SUCCEEDED(rv) ? DidProcessATokenImpl() : rv;
}

/* static */
bool
nsXMLContentSink::ParsePIData(const nsString &aData, nsString &aHref,
                              nsString &aTitle, nsString &aMedia,
                              bool &aIsAlternate)
{
  // If there was no href, we can't do anything with this PI
  if (!nsContentUtils::GetPseudoAttributeValue(aData, nsGkAtoms::href, aHref)) {
    return false;
  }

  nsContentUtils::GetPseudoAttributeValue(aData, nsGkAtoms::title, aTitle);

  nsContentUtils::GetPseudoAttributeValue(aData, nsGkAtoms::media, aMedia);

  nsAutoString alternate;
  nsContentUtils::GetPseudoAttributeValue(aData,
                                          nsGkAtoms::alternate,
                                          alternate);

  aIsAlternate = alternate.EqualsLiteral("yes");

  return true;
}

NS_IMETHODIMP
nsXMLContentSink::HandleXMLDeclaration(const char16_t *aVersion,
                                       const char16_t *aEncoding,
                                       int32_t aStandalone)
{
  mDocument->SetXMLDeclaration(aVersion, aEncoding, aStandalone);

  return DidProcessATokenImpl();
}

NS_IMETHODIMP
nsXMLContentSink::ReportError(const char16_t* aErrorText,
                              const char16_t* aSourceText,
                              nsIScriptError *aError,
                              bool *_retval)
{
  NS_PRECONDITION(aError && aSourceText && aErrorText, "Check arguments!!!");
  nsresult rv = NS_OK;

  // The expat driver should report the error.  We're just cleaning up the mess.
  *_retval = true;

  mPrettyPrintXML = false;

  mState = eXMLContentSinkState_InProlog;

  // XXX need to stop scripts here -- hsivonen

  // stop observing in order to avoid crashing when removing content
  mDocument->RemoveObserver(this);
  mIsDocumentObserver = false;

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
  mDocElement = nullptr;

  // Clear any buffered-up text we have.  It's enough to set the length to 0.
  // The buffer itself is allocated when we're created and deleted in our
  // destructor, so don't mess with it.
  mTextLength = 0;

  if (mXSLTProcessor) {
    // Get rid of the XSLT processor.
    mXSLTProcessor->CancelLoads();
    mXSLTProcessor = nullptr;
  }

  // release the nodes on stack
  mContentStack.Clear();
  mNotifyLevel = 0;

  rv = HandleProcessingInstruction(MOZ_UTF16("xml-stylesheet"),
                                   MOZ_UTF16("href=\"chrome://global/locale/intl.css\" type=\"text/css\""));
  NS_ENSURE_SUCCESS(rv, rv);

  const char16_t* noAtts[] = { 0, 0 };

  NS_NAMED_LITERAL_STRING(errorNs,
                          "http://www.mozilla.org/newlayout/xml/parsererror.xml");

  nsAutoString parsererror(errorNs);
  parsererror.Append((char16_t)0xFFFF);
  parsererror.AppendLiteral("parsererror");

  rv = HandleStartElement(parsererror.get(), noAtts, 0, (uint32_t)-1,
                          false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = HandleCharacterData(aErrorText, NS_strlen(aErrorText), false);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString sourcetext(errorNs);
  sourcetext.Append((char16_t)0xFFFF);
  sourcetext.AppendLiteral("sourcetext");

  rv = HandleStartElement(sourcetext.get(), noAtts, 0, (uint32_t)-1,
                          false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = HandleCharacterData(aSourceText, NS_strlen(aSourceText), false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = HandleEndElement(sourcetext.get(), false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = HandleEndElement(parsererror.get(), false);
  NS_ENSURE_SUCCESS(rv, rv);

  FlushTags();

  return NS_OK;
}

nsresult
nsXMLContentSink::AddAttributes(const char16_t** aAtts,
                                nsIContent* aContent)
{
  // Add tag attributes to the content attributes
  nsCOMPtr<nsIAtom> prefix, localName;
  while (*aAtts) {
    int32_t nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    // Add attribute to content
    aContent->SetAttr(nameSpaceID, localName, prefix,
                      nsDependentString(aAtts[1]), false);
    aAtts += 2;
  }

  return NS_OK;
}

#define NS_ACCUMULATION_BUFFER_SIZE 4096

nsresult
nsXMLContentSink::AddText(const char16_t* aText,
                          int32_t aLength)
{
  // Copy data from string into our buffer; flush buffer when it fills up.
  int32_t offset = 0;
  while (0 != aLength) {
    int32_t amount = NS_ACCUMULATION_BUFFER_SIZE - mTextLength;
    if (0 == amount) {
      nsresult rv = FlushText(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      MOZ_ASSERT(mTextLength == 0);
      amount = NS_ACCUMULATION_BUFFER_SIZE;
    }

    if (amount > aLength) {
      amount = aLength;
    }
    memcpy(&mText[mTextLength], &aText[offset], sizeof(char16_t) * amount);
    mTextLength += amount;
    offset += amount;
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
    if (mIsDocumentObserver) {
      // Only flush if we're still a document observer (so that our child
      // counts should be correct).
      if (aType >= Flush_ContentAndNotify) {
        FlushTags();
      }
      else {
        FlushText(false);
      }
    }
    if (aType >= Flush_InterruptibleLayout) {
      // Make sure that layout has started so that the reflow flush
      // will actually happen.
      MaybeStartLayout(true);
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
  mDeferredFlushTags = false;
  bool oldBeganUpdate = mBeganUpdate;
  uint32_t oldUpdates = mUpdatesInNotification;

  mUpdatesInNotification = 0;
  ++mInNotification;
  {
    // Scope so we call EndUpdate before we decrease mInNotification
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_CONTENT_MODEL, true);
    mBeganUpdate = true;

    // Don't release last text node in case we need to add to it again
    FlushText(false);

    // Start from the base of the stack (growing downward) and do
    // a notification from the node that is closest to the root of
    // tree for any content that has been added.

    int32_t stackPos;
    int32_t stackLen = mContentStack.Length();
    bool flushed = false;
    uint32_t childCount;
    nsIContent* content;

    for (stackPos = 0; stackPos < stackLen; ++stackPos) {
      content = mContentStack[stackPos].mContent;
      childCount = content->GetChildCount();

      if (!flushed && (mContentStack[stackPos].mNumFlushed < childCount)) {
        NotifyAppend(content, mContentStack[stackPos].mNumFlushed);
        flushed = true;
      }

      mContentStack[stackPos].mNumFlushed = childCount;
    }
    mNotifyLevel = stackLen - 1;
  }
  --mInNotification;

  if (mUpdatesInNotification > 1) {
    UpdateChildCounts();
  }

  mUpdatesInNotification = oldUpdates;
  mBeganUpdate = oldBeganUpdate;

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
  int32_t stackLen = mContentStack.Length();
  int32_t stackPos = stackLen - 1;
  while (stackPos >= 0) {
    StackNode & node = mContentStack[stackPos];
    node.mNumFlushed = node.mContent->GetChildCount();

    stackPos--;
  }
  mNotifyLevel = stackLen - 1;
}

bool
nsXMLContentSink::IsMonolithicContainer(mozilla::dom::NodeInfo* aNodeInfo)
{
  return ((aNodeInfo->NamespaceID() == kNameSpaceID_XHTML &&
          (aNodeInfo->NameAtom() == nsGkAtoms::tr ||
           aNodeInfo->NameAtom() == nsGkAtoms::select ||
           aNodeInfo->NameAtom() == nsGkAtoms::object ||
           aNodeInfo->NameAtom() == nsGkAtoms::applet)) ||
          (aNodeInfo->NamespaceID() == kNameSpaceID_MathML &&
          (aNodeInfo->NameAtom() == nsGkAtoms::math))
          );
}

void
nsXMLContentSink::ContinueInterruptedParsingIfEnabled()
{
  if (mParser && mParser->IsParserEnabled()) {
    GetParser()->ContinueInterruptedParsing();
  }
}

void
nsXMLContentSink::ContinueInterruptedParsingAsync()
{
  nsCOMPtr<nsIRunnable> ev = NewRunnableMethod(this,
    &nsXMLContentSink::ContinueInterruptedParsingIfEnabled);

  NS_DispatchToCurrentThread(ev);
}

nsIParser*
nsXMLContentSink::GetParser()
{
  return static_cast<nsIParser*>(mParser.get());
}

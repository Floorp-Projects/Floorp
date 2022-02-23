/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file is near-OBSOLETE. It is used for about:blank only and for the
 * HTML element factory.
 * Don't bother adding new stuff in this file.
 */

#include "mozilla/ArrayUtils.h"

#include "nsContentSink.h"
#include "nsCOMPtr.h"
#include "nsHTMLTags.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIHTMLContentSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include "mozilla/dom/NodeInfo.h"
#include "mozilla/dom/ScriptLoader.h"
#include "nsCRT.h"
#include "prtime.h"
#include "mozilla/Logging.h"
#include "nsIContent.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/MutationObservers.h"
#include "mozilla/Preferences.h"

#include "nsGenericHTMLElement.h"

#include "nsIScriptElement.h"

#include "nsDocElementCreatedNotificationRunner.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "mozilla/dom/Document.h"
#include "nsStubDocumentObserver.h"
#include "nsHTMLDocument.h"
#include "nsTArray.h"
#include "nsTextFragment.h"
#include "nsIScriptGlobalObject.h"
#include "nsNameSpaceManager.h"

#include "nsError.h"
#include "nsContentPolicyUtils.h"
#include "nsIDocShell.h"
#include "nsIScriptContext.h"

#include "nsLayoutCID.h"

#include "nsEscape.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "mozAutoDocUpdate.h"
#include "nsTextNode.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------

nsGenericHTMLElement* NS_NewHTMLNOTUSEDElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser) {
  MOZ_ASSERT_UNREACHABLE("The element ctor should never be called");
  return nullptr;
}

#define HTML_TAG(_tag, _classname, _interfacename) \
  NS_NewHTML##_classname##Element,
#define HTML_OTHER(_tag) NS_NewHTMLNOTUSEDElement,
static const HTMLContentCreatorFunction sHTMLContentCreatorFunctions[] = {
    NS_NewHTMLUnknownElement,
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_OTHER
    NS_NewHTMLUnknownElement};

class SinkContext;
class HTMLContentSink;

/**
 * This class is near-OBSOLETE. It is used for about:blank only.
 * Don't bother adding new stuff in this file.
 */
class HTMLContentSink : public nsContentSink, public nsIHTMLContentSink {
 public:
  friend class SinkContext;

  HTMLContentSink();

  nsresult Init(Document* aDoc, nsIURI* aURI, nsISupports* aContainer,
                nsIChannel* aChannel);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLContentSink, nsContentSink)

  // nsIContentSink
  NS_IMETHOD WillParse(void) override;
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode) override;
  NS_IMETHOD DidBuildModel(bool aTerminated) override;
  NS_IMETHOD WillInterrupt(void) override;
  NS_IMETHOD WillResume(void) override;
  NS_IMETHOD SetParser(nsParserBase* aParser) override;
  virtual void FlushPendingNotifications(FlushType aType) override;
  virtual void SetDocumentCharset(NotNull<const Encoding*> aEncoding) override;
  virtual nsISupports* GetTarget() override;
  virtual bool IsScriptExecuting() override;
  virtual void ContinueInterruptedParsingAsync() override;

  // nsIHTMLContentSink
  NS_IMETHOD OpenContainer(ElementType aNodeType) override;
  NS_IMETHOD CloseContainer(ElementType aTag) override;

 protected:
  virtual ~HTMLContentSink();

  RefPtr<nsHTMLDocument> mHTMLDocument;

  // The maximum length of a text run
  int32_t mMaxTextRun;

  RefPtr<nsGenericHTMLElement> mRoot;
  RefPtr<nsGenericHTMLElement> mBody;
  RefPtr<nsGenericHTMLElement> mHead;

  AutoTArray<SinkContext*, 8> mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;

  // Boolean indicating whether we've seen a <head> tag that might have had
  // attributes once already.
  bool mHaveSeenHead;

  // Boolean indicating whether we've notified insertion of our root content
  // yet.  We want to make sure to only do this once.
  bool mNotifiedRootInsertion;

  nsresult FlushTags() override;

  // Routines for tags that require special handling
  nsresult CloseHTML();
  nsresult OpenBody();
  nsresult CloseBody();

  void CloseHeadContext();

  // nsContentSink overrides
  void UpdateChildCounts() override;

  void NotifyInsert(nsIContent* aContent, nsIContent* aChildContent);
  void NotifyRootInsertion();

 private:
  void ContinueInterruptedParsingIfEnabled();
};

class SinkContext {
 public:
  explicit SinkContext(HTMLContentSink* aSink);
  ~SinkContext();

  nsresult Begin(nsHTMLTag aNodeType, nsGenericHTMLElement* aRoot,
                 uint32_t aNumFlushed, int32_t aInsertionPoint);
  nsresult OpenBody();
  nsresult CloseBody();
  nsresult End();

  nsresult GrowStack();
  nsresult FlushTags();

  bool IsCurrentContainer(nsHTMLTag mType);

  void DidAddContent(nsIContent* aContent);
  void UpdateChildCounts();

 private:
  // Function to check whether we've notified for the current content.
  // What this actually does is check whether we've notified for all
  // of the parent's kids.
  bool HaveNotifiedForCurrentContent() const;

 public:
  HTMLContentSink* mSink;
  int32_t mNotifyLevel;

  struct Node {
    nsHTMLTag mType;
    nsGenericHTMLElement* mContent;
    uint32_t mNumFlushed;
    int32_t mInsertionPoint;

    nsIContent* Add(nsIContent* child);
  };

  Node* mStack;
  int32_t mStackSize;
  int32_t mStackPos;
};

nsresult NS_NewHTMLElement(Element** aResult,
                           already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                           FromParser aFromParser, nsAtom* aIsAtom,
                           mozilla::dom::CustomElementDefinition* aDefinition) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo = aNodeInfo;

  NS_ASSERTION(
      nodeInfo->NamespaceEquals(kNameSpaceID_XHTML),
      "Trying to create HTML elements that don't have the XHTML namespace");

  return nsContentUtils::NewXULOrHTMLElement(aResult, nodeInfo, aFromParser,
                                             aIsAtom, aDefinition);
}

already_AddRefed<nsGenericHTMLElement> CreateHTMLElement(
    uint32_t aNodeType, already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser) {
  NS_ASSERTION(
      aNodeType <= NS_HTML_TAG_MAX || aNodeType == eHTMLTag_userdefined,
      "aNodeType is out of bounds");

  HTMLContentCreatorFunction cb = sHTMLContentCreatorFunctions[aNodeType];

  NS_ASSERTION(cb != NS_NewHTMLNOTUSEDElement,
               "Don't know how to construct tag element!");

  RefPtr<nsGenericHTMLElement> result = cb(std::move(aNodeInfo), aFromParser);

  return result.forget();
}

//----------------------------------------------------------------------

SinkContext::SinkContext(HTMLContentSink* aSink)
    : mSink(aSink),
      mNotifyLevel(0),
      mStack(nullptr),
      mStackSize(0),
      mStackPos(0) {
  MOZ_COUNT_CTOR(SinkContext);
}

SinkContext::~SinkContext() {
  MOZ_COUNT_DTOR(SinkContext);

  if (mStack) {
    for (int32_t i = 0; i < mStackPos; i++) {
      NS_RELEASE(mStack[i].mContent);
    }
    delete[] mStack;
  }
}

nsresult SinkContext::Begin(nsHTMLTag aNodeType, nsGenericHTMLElement* aRoot,
                            uint32_t aNumFlushed, int32_t aInsertionPoint) {
  if (mStackSize < 1) {
    nsresult rv = GrowStack();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mStack[0].mType = aNodeType;
  mStack[0].mContent = aRoot;
  mStack[0].mNumFlushed = aNumFlushed;
  mStack[0].mInsertionPoint = aInsertionPoint;
  NS_ADDREF(aRoot);
  mStackPos = 1;

  return NS_OK;
}

bool SinkContext::IsCurrentContainer(nsHTMLTag aTag) {
  if (aTag == mStack[mStackPos - 1].mType) {
    return true;
  }

  return false;
}

void SinkContext::DidAddContent(nsIContent* aContent) {
  if ((mStackPos == 2) && (mSink->mBody == mStack[1].mContent)) {
    // We just finished adding something to the body
    mNotifyLevel = 0;
  }

  // If we just added content to a node for which
  // an insertion happen, we need to do an immediate
  // notification for that insertion.
  if (0 < mStackPos && mStack[mStackPos - 1].mInsertionPoint != -1 &&
      mStack[mStackPos - 1].mNumFlushed <
          mStack[mStackPos - 1].mContent->GetChildCount()) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;
    mSink->NotifyInsert(parent, aContent);
    mStack[mStackPos - 1].mNumFlushed = parent->GetChildCount();
  } else if (mSink->IsTimeToNotify()) {
    FlushTags();
  }
}

nsresult SinkContext::OpenBody() {
  if (mStackPos <= 0) {
    NS_ERROR("container w/o parent");

    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (mStackPos + 1 > mStackSize) {
    rv = GrowStack();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo =
      mSink->mNodeInfoManager->GetNodeInfo(
          nsGkAtoms::body, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_UNEXPECTED);

  // Make the content object
  RefPtr<nsGenericHTMLElement> body =
      NS_NewHTMLBodyElement(nodeInfo.forget(), FROM_PARSER_NETWORK);
  if (!body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mStack[mStackPos].mType = eHTMLTag_body;
  body.forget(&mStack[mStackPos].mContent);
  mStack[mStackPos].mNumFlushed = 0;
  mStack[mStackPos].mInsertionPoint = -1;
  ++mStackPos;
  mStack[mStackPos - 2].Add(mStack[mStackPos - 1].mContent);

  return NS_OK;
}

bool SinkContext::HaveNotifiedForCurrentContent() const {
  if (0 < mStackPos) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;
    return mStack[mStackPos - 1].mNumFlushed == parent->GetChildCount();
  }

  return true;
}

nsIContent* SinkContext::Node::Add(nsIContent* child) {
  NS_ASSERTION(mContent, "No parent to insert/append into!");
  if (mInsertionPoint != -1) {
    NS_ASSERTION(mNumFlushed == mContent->GetChildCount(),
                 "Inserting multiple children without flushing.");
    nsCOMPtr<nsIContent> nodeToInsertBefore =
        mContent->GetChildAt_Deprecated(mInsertionPoint++);
    mContent->InsertChildBefore(child, nodeToInsertBefore, false,
                                IgnoreErrors());
  } else {
    mContent->AppendChildTo(child, false, IgnoreErrors());
  }
  return child;
}

nsresult SinkContext::CloseBody() {
  NS_ASSERTION(mStackPos > 0, "stack out of bounds. wrong context probably!");

  if (mStackPos <= 0) {
    return NS_OK;  // Fix crash - Ref. bug 45975 or 45007
  }

  --mStackPos;
  NS_ASSERTION(mStack[mStackPos].mType == eHTMLTag_body,
               "Tag mismatch.  Closing tag on wrong context or something?");

  nsGenericHTMLElement* content = mStack[mStackPos].mContent;

  content->Compact();

  // If we're in a state where we do append notifications as
  // we go up the tree, and we're at the level where the next
  // notification needs to be done, do the notification.
  if (mNotifyLevel >= mStackPos) {
    // Check to see if new content has been added after our last
    // notification

    if (mStack[mStackPos].mNumFlushed < content->GetChildCount()) {
      mSink->NotifyAppend(content, mStack[mStackPos].mNumFlushed);
      mStack[mStackPos].mNumFlushed = content->GetChildCount();
    }

    // Indicate that notification has now happened at this level
    mNotifyLevel = mStackPos - 1;
  }

  DidAddContent(content);
  NS_IF_RELEASE(content);

  return NS_OK;
}

nsresult SinkContext::End() {
  for (int32_t i = 0; i < mStackPos; i++) {
    NS_RELEASE(mStack[i].mContent);
  }

  mStackPos = 0;

  return NS_OK;
}

nsresult SinkContext::GrowStack() {
  int32_t newSize = mStackSize * 2;
  if (newSize == 0) {
    newSize = 32;
  }

  Node* stack = new Node[newSize];

  if (mStackPos != 0) {
    memcpy(stack, mStack, sizeof(Node) * mStackPos);
    delete[] mStack;
  }

  mStack = stack;
  mStackSize = newSize;

  return NS_OK;
}

/**
 * NOTE!! Forked into nsXMLContentSink. Please keep in sync.
 *
 * Flush all elements that have been seen so far such that
 * they are visible in the tree. Specifically, make sure
 * that they are all added to their respective parents.
 * Also, do notification at the top for all content that
 * has been newly added so that the frame tree is complete.
 */
nsresult SinkContext::FlushTags() {
  mSink->mDeferredFlushTags = false;
  uint32_t oldUpdates = mSink->mUpdatesInNotification;

  ++(mSink->mInNotification);
  mSink->mUpdatesInNotification = 0;
  {
    // Scope so we call EndUpdate before we decrease mInNotification
    mozAutoDocUpdate updateBatch(mSink->mDocument, true);

    // Start from the base of the stack (growing downward) and do
    // a notification from the node that is closest to the root of
    // tree for any content that has been added.

    // Note that we can start at stackPos == 0 here, because it's the caller's
    // responsibility to handle flushing interactions between contexts (see
    // HTMLContentSink::BeginContext).
    int32_t stackPos = 0;
    bool flushed = false;
    uint32_t childCount;
    nsGenericHTMLElement* content;

    while (stackPos < mStackPos) {
      content = mStack[stackPos].mContent;
      childCount = content->GetChildCount();

      if (!flushed && (mStack[stackPos].mNumFlushed < childCount)) {
        if (mStack[stackPos].mInsertionPoint != -1) {
          // We might have popped the child off our stack already
          // but not notified on it yet, which is why we have to get it
          // directly from its parent node.

          int32_t childIndex = mStack[stackPos].mInsertionPoint - 1;
          nsIContent* child = content->GetChildAt_Deprecated(childIndex);
          // Child not on stack anymore; can't assert it's correct
          NS_ASSERTION(!(mStackPos > (stackPos + 1)) ||
                           (child == mStack[stackPos + 1].mContent),
                       "Flushing the wrong child.");
          mSink->NotifyInsert(content, child);
        } else {
          mSink->NotifyAppend(content, mStack[stackPos].mNumFlushed);
        }

        flushed = true;
      }

      mStack[stackPos].mNumFlushed = childCount;
      stackPos++;
    }
    mNotifyLevel = mStackPos - 1;
  }
  --(mSink->mInNotification);

  if (mSink->mUpdatesInNotification > 1) {
    UpdateChildCounts();
  }

  mSink->mUpdatesInNotification = oldUpdates;

  return NS_OK;
}

/**
 * NOTE!! Forked into nsXMLContentSink. Please keep in sync.
 */
void SinkContext::UpdateChildCounts() {
  // Start from the top of the stack (growing upwards) and see if any
  // new content has been appended. If so, we recognize that reflows
  // have been generated for it and we should make sure that no
  // further reflows occur.  Note that we have to include stackPos == 0
  // to properly notify on kids of <html>.
  int32_t stackPos = mStackPos - 1;
  while (stackPos >= 0) {
    Node& node = mStack[stackPos];
    node.mNumFlushed = node.mContent->GetChildCount();

    stackPos--;
  }

  mNotifyLevel = mStackPos - 1;
}

nsresult NS_NewHTMLContentSink(nsIHTMLContentSink** aResult, Document* aDoc,
                               nsIURI* aURI, nsISupports* aContainer,
                               nsIChannel* aChannel) {
  NS_ENSURE_ARG_POINTER(aResult);

  RefPtr<HTMLContentSink> it = new HTMLContentSink();

  nsresult rv = it->Init(aDoc, aURI, aContainer, aChannel);

  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = it;
  NS_ADDREF(*aResult);

  return NS_OK;
}

HTMLContentSink::HTMLContentSink()
    : mMaxTextRun(0),
      mCurrentContext(nullptr),
      mHeadContext(nullptr),
      mHaveSeenHead(false),
      mNotifiedRootInsertion(false) {}

HTMLContentSink::~HTMLContentSink() {
  if (mNotificationTimer) {
    mNotificationTimer->Cancel();
  }

  if (mCurrentContext == mHeadContext && !mContextStack.IsEmpty()) {
    // Pop off the second html context if it's not done earlier
    mContextStack.RemoveLastElement();
  }

  for (int32_t i = 0, numContexts = mContextStack.Length(); i < numContexts;
       i++) {
    SinkContext* sc = mContextStack.ElementAt(i);
    if (sc) {
      sc->End();
      if (sc == mCurrentContext) {
        mCurrentContext = nullptr;
      }

      delete sc;
    }
  }

  if (mCurrentContext == mHeadContext) {
    mCurrentContext = nullptr;
  }

  delete mCurrentContext;

  delete mHeadContext;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(HTMLContentSink, nsContentSink,
                                   mHTMLDocument, mRoot, mBody, mHead)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLContentSink, nsContentSink,
                                             nsIContentSink, nsIHTMLContentSink)

nsresult HTMLContentSink::Init(Document* aDoc, nsIURI* aURI,
                               nsISupports* aContainer, nsIChannel* aChannel) {
  NS_ENSURE_TRUE(aContainer, NS_ERROR_NULL_POINTER);

  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aDoc->AddObserver(this);
  mIsDocumentObserver = true;
  mHTMLDocument = aDoc->AsHTMLDocument();

  NS_ASSERTION(mDocShell, "oops no docshell!");

  // Changed from 8192 to greatly improve page loading performance on
  // large pages.  See bugzilla bug 77540.
  mMaxTextRun = Preferences::GetInt("content.maxtextrun", 8191);

  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(
      nsGkAtoms::html, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);

  // Make root part
  mRoot = NS_NewHTMLHtmlElement(nodeInfo.forget());
  if (!mRoot) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ASSERTION(mDocument->GetChildCount() == 0,
               "Document should have no kids here!");
  ErrorResult error;
  mDocument->AppendChildTo(mRoot, false, error);
  if (error.Failed()) {
    return error.StealNSResult();
  }

  // Make head part
  nodeInfo = mNodeInfoManager->GetNodeInfo(
      nsGkAtoms::head, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);

  mHead = NS_NewHTMLHeadElement(nodeInfo.forget());
  if (NS_FAILED(rv)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRoot->AppendChildTo(mHead, false, IgnoreErrors());

  mCurrentContext = new SinkContext(this);
  mCurrentContext->Begin(eHTMLTag_html, mRoot, 0, -1);
  mContextStack.AppendElement(mCurrentContext);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillParse(void) { return WillParseImpl(); }

NS_IMETHODIMP
HTMLContentSink::WillBuildModel(nsDTDMode aDTDMode) {
  WillBuildModelImpl();

  nsCompatibility mode = eCompatibility_NavQuirks;
  switch (aDTDMode) {
    case eDTDMode_full_standards:
      mode = eCompatibility_FullStandards;
      break;
    case eDTDMode_almost_standards:
      mode = eCompatibility_AlmostStandards;
      break;
    default:
      break;
  }
  mDocument->SetCompatibilityMode(mode);

  // Notify document that the load is beginning
  mDocument->BeginLoad();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidBuildModel(bool aTerminated) {
  DidBuildModelImpl(aTerminated);

  // Reflow the last batch of content
  if (mBody) {
    mCurrentContext->FlushTags();
  } else if (!mLayoutStarted) {
    // We never saw the body, and layout never got started. Force
    // layout *now*, to get an initial reflow.
    // NOTE: only force the layout if we are NOT destroying the
    // docshell. If we are destroying it, then starting layout will
    // likely cause us to crash, or at best waste a lot of time as we
    // are just going to tear it down anyway.
    bool bDestroying = true;
    if (mDocShell) {
      mDocShell->IsBeingDestroyed(&bDestroying);
    }

    if (!bDestroying) {
      StartLayout(false);
    }
  }

  ScrollToRef();

  // Make sure we no longer respond to document mutations.  We've flushed all
  // our notifications out, so there's no need to do anything else here.

  // XXXbz I wonder whether we could End() our contexts here too, or something,
  // just to make sure we no longer notify...  Or is the mIsDocumentObserver
  // thing sufficient?
  mDocument->RemoveObserver(this);
  mIsDocumentObserver = false;

  mDocument->EndLoad();

  DropParserAndPerfHint();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::SetParser(nsParserBase* aParser) {
  MOZ_ASSERT(aParser, "Should have a parser here!");
  mParser = aParser;
  return NS_OK;
}

nsresult HTMLContentSink::CloseHTML() {
  if (mHeadContext) {
    if (mCurrentContext == mHeadContext) {
      // Pop off the second html context if it's not done earlier
      mCurrentContext = mContextStack.PopLastElement();
    }

    mHeadContext->End();

    delete mHeadContext;
    mHeadContext = nullptr;
  }

  return NS_OK;
}

nsresult HTMLContentSink::OpenBody() {
  CloseHeadContext();  // do this just in case if the HEAD was left open!

  // if we already have a body we're done
  if (mBody) {
    return NS_OK;
  }

  nsresult rv = mCurrentContext->OpenBody();

  if (NS_FAILED(rv)) {
    return rv;
  }

  mBody = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;

  if (mCurrentContext->mStackPos > 1) {
    int32_t parentIndex = mCurrentContext->mStackPos - 2;
    nsGenericHTMLElement* parent =
        mCurrentContext->mStack[parentIndex].mContent;
    int32_t numFlushed = mCurrentContext->mStack[parentIndex].mNumFlushed;
    int32_t childCount = parent->GetChildCount();
    NS_ASSERTION(numFlushed < childCount, "Already notified on the body?");

    int32_t insertionPoint =
        mCurrentContext->mStack[parentIndex].mInsertionPoint;

    // XXX: I have yet to see a case where numFlushed is non-zero and
    // insertionPoint is not -1, but this code will try to handle
    // those cases too.

    uint32_t oldUpdates = mUpdatesInNotification;
    mUpdatesInNotification = 0;
    if (insertionPoint != -1) {
      NotifyInsert(parent, mBody);
    } else {
      NotifyAppend(parent, numFlushed);
    }
    mCurrentContext->mStack[parentIndex].mNumFlushed = childCount;
    if (mUpdatesInNotification > 1) {
      UpdateChildCounts();
    }
    mUpdatesInNotification = oldUpdates;
  }

  StartLayout(false);

  return NS_OK;
}

nsresult HTMLContentSink::CloseBody() {
  // Flush out anything that's left
  mCurrentContext->FlushTags();
  mCurrentContext->CloseBody();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(ElementType aElementType) {
  nsresult rv = NS_OK;

  switch (aElementType) {
    case eBody:
      rv = OpenBody();
      break;
    case eHTML:
      if (mRoot) {
        // If we've already hit this code once, then we're done
        if (!mNotifiedRootInsertion) {
          NotifyRootInsertion();
        }
      }
      break;
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const ElementType aTag) {
  nsresult rv = NS_OK;

  switch (aTag) {
    case eBody:
      rv = CloseBody();
      break;
    case eHTML:
      rv = CloseHTML();
      break;
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::WillInterrupt() { return WillInterruptImpl(); }

NS_IMETHODIMP
HTMLContentSink::WillResume() { return WillResumeImpl(); }

void HTMLContentSink::CloseHeadContext() {
  if (mCurrentContext) {
    if (!mCurrentContext->IsCurrentContainer(eHTMLTag_head)) return;

    mCurrentContext->FlushTags();
  }

  if (!mContextStack.IsEmpty()) {
    mCurrentContext = mContextStack.PopLastElement();
  }
}

void HTMLContentSink::NotifyInsert(nsIContent* aContent,
                                   nsIContent* aChildContent) {
  mInNotification++;

  {
    // Scope so we call EndUpdate before we decrease mInNotification
    // Note that aContent->OwnerDoc() may be different to mDocument already.
    MOZ_AUTO_DOC_UPDATE(aContent ? aContent->OwnerDoc() : mDocument.get(),
                        true);
    MutationObservers::NotifyContentInserted(NODE_FROM(aContent, mDocument),
                                             aChildContent);
    mLastNotificationTime = PR_Now();
  }

  mInNotification--;
}

void HTMLContentSink::NotifyRootInsertion() {
  MOZ_ASSERT(!mNotifiedRootInsertion, "Double-notifying on root?");
  NS_ASSERTION(!mLayoutStarted,
               "How did we start layout without notifying on root?");
  // Now make sure to notify that we have now inserted our root.  If
  // there has been no initial reflow yet it'll be a no-op, but if
  // there has been one we need this to get its frames constructed.
  // Note that if mNotifiedRootInsertion is true we don't notify here,
  // since that just means there are multiple <html> tags in the
  // document; in those cases we just want to put all the attrs on one
  // tag.
  mNotifiedRootInsertion = true;
  NotifyInsert(nullptr, mRoot);

  // Now update the notification information in all our
  // contexts, since we just inserted the root and notified on
  // our whole tree
  UpdateChildCounts();

  nsContentUtils::AddScriptRunner(
      new nsDocElementCreatedNotificationRunner(mDocument));
}

void HTMLContentSink::UpdateChildCounts() {
  uint32_t numContexts = mContextStack.Length();
  for (uint32_t i = 0; i < numContexts; i++) {
    SinkContext* sc = mContextStack.ElementAt(i);

    sc->UpdateChildCounts();
  }

  mCurrentContext->UpdateChildCounts();
}

void HTMLContentSink::FlushPendingNotifications(FlushType aType) {
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant)
  if (!mInNotification) {
    // Only flush if we're still a document observer (so that our child counts
    // should be correct).
    if (mIsDocumentObserver) {
      if (aType >= FlushType::ContentAndNotify) {
        FlushTags();
      }
    }
    if (aType >= FlushType::EnsurePresShellInitAndFrames) {
      // Make sure that layout has started so that the reflow flush
      // will actually happen.
      StartLayout(true);
    }
  }
}

nsresult HTMLContentSink::FlushTags() {
  if (!mNotifiedRootInsertion) {
    NotifyRootInsertion();
    return NS_OK;
  }

  return mCurrentContext ? mCurrentContext->FlushTags() : NS_OK;
}

void HTMLContentSink::SetDocumentCharset(NotNull<const Encoding*> aEncoding) {
  MOZ_ASSERT_UNREACHABLE("<meta charset> case doesn't occur with about:blank");
}

nsISupports* HTMLContentSink::GetTarget() { return ToSupports(mDocument); }

bool HTMLContentSink::IsScriptExecuting() { return IsScriptExecutingImpl(); }

void HTMLContentSink::ContinueInterruptedParsingIfEnabled() {
  if (mParser && mParser->IsParserEnabled()) {
    static_cast<nsIParser*>(mParser.get())->ContinueInterruptedParsing();
  }
}

void HTMLContentSink::ContinueInterruptedParsingAsync() {
  nsCOMPtr<nsIRunnable> ev = NewRunnableMethod(
      "HTMLContentSink::ContinueInterruptedParsingIfEnabled", this,
      &HTMLContentSink::ContinueInterruptedParsingIfEnabled);

  RefPtr<Document> doc = mHTMLDocument;
  doc->Dispatch(mozilla::TaskCategory::Other, ev.forget());
}

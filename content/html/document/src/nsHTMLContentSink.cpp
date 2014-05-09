/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
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
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIHTMLContentSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsScriptLoader.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsINodeInfo.h"
#include "nsToken.h"
#include "nsIAppShell.h"
#include "nsCRT.h"
#include "prtime.h"
#include "prlog.h"
#include "nsNodeUtils.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"

#include "nsGenericHTMLElement.h"

#include "nsIDOMDocument.h"
#include "nsIDOMDocumentType.h"
#include "nsIScriptElement.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsStubDocumentObserver.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsICookieService.h"
#include "nsTArray.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsTextFragment.h"
#include "nsIScriptGlobalObject.h"
#include "nsNameSpaceManager.h"

#include "nsIParserService.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsITimer.h"
#include "nsError.h"
#include "nsContentPolicyUtils.h"
#include "nsIScriptContext.h"
#include "nsStyleLinkElement.h"

#include "nsWeakReference.h" // nsHTMLElementFactory supports weak references
#include "nsIPrompt.h"
#include "nsLayoutCID.h"
#include "nsIDocShellTreeItem.h"

#include "nsEscape.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "mozAutoDocUpdate.h"
#include "nsTextNode.h"

using namespace mozilla;
using namespace mozilla::dom;

//----------------------------------------------------------------------

typedef nsGenericHTMLElement*
  (*contentCreatorCallback)(already_AddRefed<nsINodeInfo>&&,
                            FromParser aFromParser);

nsGenericHTMLElement*
NS_NewHTMLNOTUSEDElement(already_AddRefed<nsINodeInfo>&& aNodeInfo,
                         FromParser aFromParser)
{
  NS_NOTREACHED("The element ctor should never be called");
  return nullptr;
}

#define HTML_TAG(_tag, _classname) NS_NewHTML##_classname##Element,
#define HTML_HTMLELEMENT_TAG(_tag) NS_NewHTMLElement,
#define HTML_OTHER(_tag) NS_NewHTMLNOTUSEDElement,
static const contentCreatorCallback sContentCreatorCallbacks[] = {
  NS_NewHTMLUnknownElement,
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_HTMLELEMENT_TAG
#undef HTML_OTHER
  NS_NewHTMLUnknownElement
};

class SinkContext;
class HTMLContentSink;

/**
 * This class is near-OBSOLETE. It is used for about:blank only.
 * Don't bother adding new stuff in this file.
 */
class HTMLContentSink : public nsContentSink,
                        public nsIHTMLContentSink
{
public:
  friend class SinkContext;

  HTMLContentSink();
  virtual ~HTMLContentSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsresult Init(nsIDocument* aDoc, nsIURI* aURI, nsISupports* aContainer,
                nsIChannel* aChannel);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLContentSink, nsContentSink)

  // nsIContentSink
  NS_IMETHOD WillParse(void);
  NS_IMETHOD WillBuildModel(nsDTDMode aDTDMode);
  NS_IMETHOD DidBuildModel(bool aTerminated);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsParserBase* aParser);
  virtual void FlushPendingNotifications(mozFlushType aType);
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget();
  virtual bool IsScriptExecuting();

  // nsIHTMLContentSink
  NS_IMETHOD OpenContainer(ElementType aNodeType);
  NS_IMETHOD CloseContainer(ElementType aTag);

protected:
  nsCOMPtr<nsIHTMLDocument> mHTMLDocument;

  // The maximum length of a text run
  int32_t mMaxTextRun;

  nsRefPtr<nsGenericHTMLElement> mRoot;
  nsRefPtr<nsGenericHTMLElement> mBody;
  nsRefPtr<nsGenericHTMLElement> mHead;

  nsAutoTArray<SinkContext*, 8> mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;

  // Boolean indicating whether we've seen a <head> tag that might have had
  // attributes once already.
  bool mHaveSeenHead;

  // Boolean indicating whether we've notified insertion of our root content
  // yet.  We want to make sure to only do this once.
  bool mNotifiedRootInsertion;

  uint8_t mScriptEnabled : 1;
  uint8_t mFramesEnabled : 1;
  uint8_t unused : 6;  // bits available if someone needs one

  nsINodeInfo* mNodeInfoCache[NS_HTML_TAG_MAX + 1];

  nsresult FlushTags();

  // Routines for tags that require special handling
  nsresult CloseHTML();
  nsresult OpenBody();
  nsresult CloseBody();

  void CloseHeadContext();

  // nsContentSink overrides
  void UpdateChildCounts();

  void NotifyInsert(nsIContent* aContent,
                    nsIContent* aChildContent,
                    int32_t aIndexInContainer);
  void NotifyRootInsertion();
};

class SinkContext
{
public:
  SinkContext(HTMLContentSink* aSink);
  ~SinkContext();

  nsresult Begin(nsHTMLTag aNodeType, nsGenericHTMLElement* aRoot,
                 uint32_t aNumFlushed, int32_t aInsertionPoint);
  nsresult OpenBody();
  nsresult CloseBody();
  nsresult End();

  nsresult GrowStack();
  nsresult FlushTags();

  bool     IsCurrentContainer(nsHTMLTag mType);

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

    nsIContent *Add(nsIContent *child);
  };

  Node* mStack;
  int32_t mStackSize;
  int32_t mStackPos;
};

nsresult
NS_NewHTMLElement(Element** aResult, already_AddRefed<nsINodeInfo>&& aNodeInfo,
                  FromParser aFromParser)
{
  *aResult = nullptr;

  nsCOMPtr<nsINodeInfo> nodeInfo = aNodeInfo;

  nsIParserService* parserService = nsContentUtils::GetParserService();
  if (!parserService)
    return NS_ERROR_OUT_OF_MEMORY;

  nsIAtom *name = nodeInfo->NameAtom();

  NS_ASSERTION(nodeInfo->NamespaceEquals(kNameSpaceID_XHTML), 
               "Trying to HTML elements that don't have the XHTML namespace");

  // Per the Custom Element specification, unknown tags that are valid custom
  // element names should be HTMLElement instead of HTMLUnknownElement.
  int32_t tag = parserService->HTMLCaseSensitiveAtomTagToId(name);
  if (tag == eHTMLTag_userdefined &&
      nsContentUtils::IsCustomElementName(name)) {
    nsIDocument* doc = nodeInfo->GetDocument();

    NS_IF_ADDREF(*aResult = NS_NewHTMLElement(nodeInfo.forget(), aFromParser));
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Element may be unresolved at this point.
    doc->RegisterUnresolvedElement(*aResult);

    // Try to enqueue a created callback. The custom element data will be set
    // and created callback will be enqueued if the custom element type
    // has already been registered.
    doc->EnqueueLifecycleCallback(nsIDocument::eCreated, *aResult);

    return NS_OK;
  }

  *aResult = CreateHTMLElement(tag,
                               nodeInfo.forget(), aFromParser).take();
  return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

already_AddRefed<nsGenericHTMLElement>
CreateHTMLElement(uint32_t aNodeType,
                  already_AddRefed<nsINodeInfo>&& aNodeInfo,
                  FromParser aFromParser)
{
  NS_ASSERTION(aNodeType <= NS_HTML_TAG_MAX ||
               aNodeType == eHTMLTag_userdefined,
               "aNodeType is out of bounds");

  contentCreatorCallback cb = sContentCreatorCallbacks[aNodeType];

  NS_ASSERTION(cb != NS_NewHTMLNOTUSEDElement,
               "Don't know how to construct tag element!");

  nsRefPtr<nsGenericHTMLElement> result = cb(Move(aNodeInfo), aFromParser);

  return result.forget();
}

//----------------------------------------------------------------------

SinkContext::SinkContext(HTMLContentSink* aSink)
  : mSink(aSink),
    mNotifyLevel(0),
    mStack(nullptr),
    mStackSize(0),
    mStackPos(0)
{
  MOZ_COUNT_CTOR(SinkContext);
}

SinkContext::~SinkContext()
{
  MOZ_COUNT_DTOR(SinkContext);

  if (mStack) {
    for (int32_t i = 0; i < mStackPos; i++) {
      NS_RELEASE(mStack[i].mContent);
    }
    delete [] mStack;
  }
}

nsresult
SinkContext::Begin(nsHTMLTag aNodeType,
                   nsGenericHTMLElement* aRoot,
                   uint32_t aNumFlushed,
                   int32_t aInsertionPoint)
{
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

bool
SinkContext::IsCurrentContainer(nsHTMLTag aTag)
{
  if (aTag == mStack[mStackPos - 1].mType) {
    return true;
  }

  return false;
}

void
SinkContext::DidAddContent(nsIContent* aContent)
{
  if ((mStackPos == 2) && (mSink->mBody == mStack[1].mContent)) {
    // We just finished adding something to the body
    mNotifyLevel = 0;
  }

  // If we just added content to a node for which
  // an insertion happen, we need to do an immediate
  // notification for that insertion.
  if (0 < mStackPos &&
      mStack[mStackPos - 1].mInsertionPoint != -1 &&
      mStack[mStackPos - 1].mNumFlushed <
      mStack[mStackPos - 1].mContent->GetChildCount()) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;
    int32_t childIndex = mStack[mStackPos - 1].mInsertionPoint - 1;
    NS_ASSERTION(parent->GetChildAt(childIndex) == aContent,
                 "Flushing the wrong child.");
    mSink->NotifyInsert(parent, aContent, childIndex);
    mStack[mStackPos - 1].mNumFlushed = parent->GetChildCount();
  } else if (mSink->IsTimeToNotify()) {
    FlushTags();
  }
}

nsresult
SinkContext::OpenBody()
{
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

    nsCOMPtr<nsINodeInfo> nodeInfo =
      mSink->mNodeInfoManager->GetNodeInfo(nsGkAtoms::body, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_UNEXPECTED);

  // Make the content object
  nsRefPtr<nsGenericHTMLElement> body =
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

bool
SinkContext::HaveNotifiedForCurrentContent() const
{
  if (0 < mStackPos) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;
    return mStack[mStackPos-1].mNumFlushed == parent->GetChildCount();
  }

  return true;
}

nsIContent *
SinkContext::Node::Add(nsIContent *child)
{
  NS_ASSERTION(mContent, "No parent to insert/append into!");
  if (mInsertionPoint != -1) {
    NS_ASSERTION(mNumFlushed == mContent->GetChildCount(),
                 "Inserting multiple children without flushing.");
    mContent->InsertChildAt(child, mInsertionPoint++, false);
  } else {
    mContent->AppendChildTo(child, false);
  }
  return child;
}

nsresult
SinkContext::CloseBody()
{
  NS_ASSERTION(mStackPos > 0,
               "stack out of bounds. wrong context probably!");

  if (mStackPos <= 0) {
    return NS_OK; // Fix crash - Ref. bug 45975 or 45007
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

nsresult
SinkContext::End()
{
  for (int32_t i = 0; i < mStackPos; i++) {
    NS_RELEASE(mStack[i].mContent);
  }

  mStackPos = 0;

  return NS_OK;
}

nsresult
SinkContext::GrowStack()
{
  int32_t newSize = mStackSize * 2;
  if (newSize == 0) {
    newSize = 32;
  }

  Node* stack = new Node[newSize];

  if (mStackPos != 0) {
    memcpy(stack, mStack, sizeof(Node) * mStackPos);
    delete [] mStack;
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
nsresult
SinkContext::FlushTags()
{
  mSink->mDeferredFlushTags = false;
  bool oldBeganUpdate = mSink->mBeganUpdate;
  uint32_t oldUpdates = mSink->mUpdatesInNotification;

  ++(mSink->mInNotification);
  mSink->mUpdatesInNotification = 0;
  {
    // Scope so we call EndUpdate before we decrease mInNotification
    mozAutoDocUpdate updateBatch(mSink->mDocument, UPDATE_CONTENT_MODEL,
                                 true);
    mSink->mBeganUpdate = true;

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
          nsIContent* child = content->GetChildAt(childIndex);
          // Child not on stack anymore; can't assert it's correct
          NS_ASSERTION(!(mStackPos > (stackPos + 1)) ||
                       (child == mStack[stackPos + 1].mContent),
                       "Flushing the wrong child.");
          mSink->NotifyInsert(content, child, childIndex);
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
  mSink->mBeganUpdate = oldBeganUpdate;

  return NS_OK;
}

/**
 * NOTE!! Forked into nsXMLContentSink. Please keep in sync.
 */
void
SinkContext::UpdateChildCounts()
{
  // Start from the top of the stack (growing upwards) and see if any
  // new content has been appended. If so, we recognize that reflows
  // have been generated for it and we should make sure that no
  // further reflows occur.  Note that we have to include stackPos == 0
  // to properly notify on kids of <html>.
  int32_t stackPos = mStackPos - 1;
  while (stackPos >= 0) {
    Node & node = mStack[stackPos];
    node.mNumFlushed = node.mContent->GetChildCount();

    stackPos--;
  }

  mNotifyLevel = mStackPos - 1;
}

nsresult
NS_NewHTMLContentSink(nsIHTMLContentSink** aResult,
                      nsIDocument* aDoc,
                      nsIURI* aURI,
                      nsISupports* aContainer,
                      nsIChannel* aChannel)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsRefPtr<HTMLContentSink> it = new HTMLContentSink();

  nsresult rv = it->Init(aDoc, aURI, aContainer, aChannel);

  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = it;
  NS_ADDREF(*aResult);

  return NS_OK;
}

HTMLContentSink::HTMLContentSink()
{
  // Note: operator new zeros our memory
}

HTMLContentSink::~HTMLContentSink()
{
  if (mNotificationTimer) {
    mNotificationTimer->Cancel();
  }

  int32_t numContexts = mContextStack.Length();

  if (mCurrentContext == mHeadContext && numContexts > 0) {
    // Pop off the second html context if it's not done earlier
    mContextStack.RemoveElementAt(--numContexts);
  }

  int32_t i;
  for (i = 0; i < numContexts; i++) {
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

  for (i = 0; uint32_t(i) < ArrayLength(mNodeInfoCache); ++i) {
    NS_IF_RELEASE(mNodeInfoCache[i]);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLContentSink)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLContentSink, nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHTMLDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBody)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHead)
  for (uint32_t i = 0; i < ArrayLength(tmp->mNodeInfoCache); ++i) {
    NS_IF_RELEASE(tmp->mNodeInfoCache[i]);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLContentSink,
                                                  nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHTMLDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBody)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHead)
  for (uint32_t i = 0; i < ArrayLength(tmp->mNodeInfoCache); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mNodeInfoCache[i]");
    cb.NoteXPCOMChild(tmp->mNodeInfoCache[i]);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLContentSink)
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY(HTMLContentSink, nsIContentSink)
    NS_INTERFACE_TABLE_ENTRY(HTMLContentSink, nsIHTMLContentSink)
  NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(HTMLContentSink, nsContentSink)
NS_IMPL_RELEASE_INHERITED(HTMLContentSink, nsContentSink)

static bool
IsScriptEnabled(nsIDocument *aDoc, nsIDocShell *aContainer)
{
  NS_ENSURE_TRUE(aDoc && aContainer, true);

  nsCOMPtr<nsIScriptGlobalObject> globalObject =
    do_QueryInterface(aDoc->GetInnerWindow());

  // Getting context is tricky if the document hasn't had its
  // GlobalObject set yet
  if (!globalObject) {
    globalObject = aContainer->GetScriptGlobalObject();
  }

  NS_ENSURE_TRUE(globalObject && globalObject->GetGlobalJSObject(), true);
  return nsContentUtils::GetSecurityManager()->
           ScriptAllowed(globalObject->GetGlobalJSObject());
}

nsresult
HTMLContentSink::Init(nsIDocument* aDoc,
                      nsIURI* aURI,
                      nsISupports* aContainer,
                      nsIChannel* aChannel)
{
  NS_ENSURE_TRUE(aContainer, NS_ERROR_NULL_POINTER);
  
  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aDoc->AddObserver(this);
  mIsDocumentObserver = true;
  mHTMLDocument = do_QueryInterface(aDoc);

  NS_ASSERTION(mDocShell, "oops no docshell!");

  // Find out if subframes are enabled
  if (mDocShell) {
    bool subFramesEnabled = true;
    mDocShell->GetAllowSubframes(&subFramesEnabled);
    if (subFramesEnabled) {
      mFramesEnabled = true;
    }
  }

  // Find out if scripts are enabled, if not, show <noscript> content
  if (IsScriptEnabled(aDoc, mDocShell)) {
    mScriptEnabled = true;
  }


  // Changed from 8192 to greatly improve page loading performance on
  // large pages.  See bugzilla bug 77540.
  mMaxTextRun = Preferences::GetInt("content.maxtextrun", 8191);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::html, nullptr,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  // Make root part
  mRoot = NS_NewHTMLHtmlElement(nodeInfo.forget());
  if (!mRoot) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ASSERTION(mDocument->GetChildCount() == 0,
               "Document should have no kids here!");
  rv = mDocument->AppendChildTo(mRoot, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // Make head part
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::head,
                                           nullptr, kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);

  mHead = NS_NewHTMLHeadElement(nodeInfo.forget());
  if (NS_FAILED(rv)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRoot->AppendChildTo(mHead, false);

  mCurrentContext = new SinkContext(this);
  mCurrentContext->Begin(eHTMLTag_html, mRoot, 0, -1);
  mContextStack.AppendElement(mCurrentContext);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillParse(void)
{
  return WillParseImpl();
}

NS_IMETHODIMP
HTMLContentSink::WillBuildModel(nsDTDMode aDTDMode)
{
  WillBuildModelImpl();

  if (mHTMLDocument) {
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
    mHTMLDocument->SetCompatibilityMode(mode);
  }

  // Notify document that the load is beginning
  mDocument->BeginLoad();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidBuildModel(bool aTerminated)
{
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
HTMLContentSink::SetParser(nsParserBase* aParser)
{
  NS_PRECONDITION(aParser, "Should have a parser here!");
  mParser = aParser;
  return NS_OK;
}

nsresult
HTMLContentSink::CloseHTML()
{
  if (mHeadContext) {
    if (mCurrentContext == mHeadContext) {
      uint32_t numContexts = mContextStack.Length();

      // Pop off the second html context if it's not done earlier
      mCurrentContext = mContextStack.ElementAt(--numContexts);
      mContextStack.RemoveElementAt(numContexts);
    }

    mHeadContext->End();

    delete mHeadContext;
    mHeadContext = nullptr;
  }

  return NS_OK;
}

nsresult
HTMLContentSink::OpenBody()
{
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
    int32_t parentIndex    = mCurrentContext->mStackPos - 2;
    nsGenericHTMLElement *parent = mCurrentContext->mStack[parentIndex].mContent;
    int32_t numFlushed     = mCurrentContext->mStack[parentIndex].mNumFlushed;
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
      NotifyInsert(parent, mBody, insertionPoint - 1);
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

nsresult
HTMLContentSink::CloseBody()
{
  // Flush out anything that's left
  mCurrentContext->FlushTags();
  mCurrentContext->CloseBody();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(ElementType aElementType)
{
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
        ProcessOfflineManifest(mRoot);
      }
      break;
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const ElementType aTag)
{
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
HTMLContentSink::WillInterrupt()
{
  return WillInterruptImpl();
}

NS_IMETHODIMP
HTMLContentSink::WillResume()
{
  return WillResumeImpl();
}

void
HTMLContentSink::CloseHeadContext()
{
  if (mCurrentContext) {
    if (!mCurrentContext->IsCurrentContainer(eHTMLTag_head))
      return;

    mCurrentContext->FlushTags();
  }

  if (!mContextStack.IsEmpty())
  {
    uint32_t n = mContextStack.Length() - 1;
    mCurrentContext = mContextStack.ElementAt(n);
    mContextStack.RemoveElementAt(n);
  }
}

void
HTMLContentSink::NotifyInsert(nsIContent* aContent,
                              nsIContent* aChildContent,
                              int32_t aIndexInContainer)
{
  if (aContent && aContent->GetCurrentDoc() != mDocument) {
    // aContent is not actually in our document anymore.... Just bail out of
    // here; notifying on our document for this insert would be wrong.
    return;
  }

  mInNotification++;

  {
    // Scope so we call EndUpdate before we decrease mInNotification
    MOZ_AUTO_DOC_UPDATE(mDocument, UPDATE_CONTENT_MODEL, !mBeganUpdate);
    nsNodeUtils::ContentInserted(NODE_FROM(aContent, mDocument),
                                 aChildContent, aIndexInContainer);
    mLastNotificationTime = PR_Now();
  }

  mInNotification--;
}

void
HTMLContentSink::NotifyRootInsertion()
{
  NS_PRECONDITION(!mNotifiedRootInsertion, "Double-notifying on root?");
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
  int32_t index = mDocument->IndexOf(mRoot);
  NS_ASSERTION(index != -1, "mRoot not child of document?");
  NotifyInsert(nullptr, mRoot, index);

  // Now update the notification information in all our
  // contexts, since we just inserted the root and notified on
  // our whole tree
  UpdateChildCounts();
}

void
HTMLContentSink::UpdateChildCounts()
{
  uint32_t numContexts = mContextStack.Length();
  for (uint32_t i = 0; i < numContexts; i++) {
    SinkContext* sc = mContextStack.ElementAt(i);

    sc->UpdateChildCounts();
  }

  mCurrentContext->UpdateChildCounts();
}

void
HTMLContentSink::FlushPendingNotifications(mozFlushType aType)
{
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant)
  if (!mInNotification) {
    // Only flush if we're still a document observer (so that our child counts
    // should be correct).
    if (mIsDocumentObserver) {
      if (aType >= Flush_ContentAndNotify) {
        FlushTags();
      }
    }
    if (aType >= Flush_InterruptibleLayout) {
      // Make sure that layout has started so that the reflow flush
      // will actually happen.
      StartLayout(true);
    }
  }
}

nsresult
HTMLContentSink::FlushTags()
{
  if (!mNotifiedRootInsertion) {
    NotifyRootInsertion();
    return NS_OK;
  }
  
  return mCurrentContext ? mCurrentContext->FlushTags() : NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::SetDocumentCharset(nsACString& aCharset)
{
  MOZ_ASSERT_UNREACHABLE("<meta charset> case doesn't occur with about:blank");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsISupports *
HTMLContentSink::GetTarget()
{
  return mDocument;
}

bool
HTMLContentSink::IsScriptExecuting()
{
  return IsScriptExecutingImpl();
}

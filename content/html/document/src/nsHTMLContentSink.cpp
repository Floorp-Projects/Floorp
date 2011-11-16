/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=78: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Daniel Glazman <glazman@netscape.com>
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

#include "mozilla/Util.h"

#include "nsContentSink.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsIHTMLContentSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIParser.h"
#include "nsParserUtils.h"
#include "nsScriptLoader.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsINodeInfo.h"
#include "nsHTMLTokens.h"
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

#include "nsIDOMHTMLFormElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"

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
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsICookieService.h"
#include "nsTArray.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsTextFragment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"

#include "nsIParserService.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsITimer.h"
#include "nsDOMError.h"
#include "nsContentPolicyUtils.h"
#include "nsIScriptContext.h"
#include "nsStyleLinkElement.h"

#include "nsReadableUtils.h"
#include "nsWeakReference.h" // nsHTMLElementFactory supports weak references
#include "nsIPrompt.h"
#include "nsLayoutCID.h"
#include "nsIDocShellTreeItem.h"

#include "nsEscape.h"
#include "nsIElementObserver.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "mozAutoDocUpdate.h"

using namespace mozilla;
using namespace mozilla::dom;

#ifdef NS_DEBUG
static PRLogModuleInfo* gSinkLogModuleInfo;

#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj) \
  _obj->SinkTraceNode(_bit, _msg, _tag, _sp, this)

#else
#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj)
#endif

//----------------------------------------------------------------------

typedef nsGenericHTMLElement*
  (*contentCreatorCallback)(already_AddRefed<nsINodeInfo>,
                            FromParser aFromParser);

nsGenericHTMLElement*
NS_NewHTMLNOTUSEDElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                         FromParser aFromParser)
{
  NS_NOTREACHED("The element ctor should never be called");
  return nsnull;
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

static void MaybeSetForm(nsGenericHTMLElement*, nsHTMLTag, HTMLContentSink*);

class HTMLContentSink : public nsContentSink,
#ifdef DEBUG
                        public nsIDebugDumpContent,
#endif
                        public nsIHTMLContentSink
{
public:
  friend class SinkContext;
  friend void MaybeSetForm(nsGenericHTMLElement*, nsHTMLTag, HTMLContentSink*);

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
  NS_IMETHOD SetParser(nsIParser* aParser);
  virtual void FlushPendingNotifications(mozFlushType aType);
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget();
  virtual bool IsScriptExecuting();

  // nsIHTMLContentSink
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD CloseMalformedContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  NS_IMETHOD DidProcessTokens(void);
  NS_IMETHOD WillProcessAToken(void);
  NS_IMETHOD DidProcessAToken(void);
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode);
  NS_IMETHOD BeginContext(PRInt32 aID);
  NS_IMETHOD EndContext(PRInt32 aID);
  NS_IMETHOD OpenHead();
  NS_IMETHOD IsEnabled(PRInt32 aTag, bool* aReturn);
  NS_IMETHOD_(bool) IsFormOnStack();

#ifdef DEBUG
  // nsIDebugDumpContent
  NS_IMETHOD DumpContentModel();
#endif

protected:
  // If aCheckIfPresent is true, will only set an attribute in cases
  // when it's not already set.
  nsresult AddAttributes(const nsIParserNode& aNode, nsIContent* aContent,
                         bool aNotify = false,
                         bool aCheckIfPresent = false);
  already_AddRefed<nsGenericHTMLElement>
  CreateContentObject(const nsIParserNode& aNode, nsHTMLTag aNodeType);

#ifdef NS_DEBUG
  void SinkTraceNode(PRUint32 aBit,
                     const char* aMsg,
                     const nsHTMLTag aTag,
                     PRInt32 aStackPos,
                     void* aThis);
#endif

  nsCOMPtr<nsIHTMLDocument> mHTMLDocument;

  // The maximum length of a text run
  PRInt32 mMaxTextRun;

  nsRefPtr<nsGenericHTMLElement> mRoot;
  nsRefPtr<nsGenericHTMLElement> mBody;
  nsRefPtr<nsGenericHTMLElement> mFrameset;
  nsRefPtr<nsGenericHTMLElement> mHead;

  nsRefPtr<nsGenericHTMLElement> mCurrentForm;

  nsAutoTArray<SinkContext*, 8> mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;
  PRInt32 mNumOpenIFRAMES;

  // depth of containment within <noembed>, <noframes> etc
  PRInt32 mInsideNoXXXTag;

  // Boolean indicating whether we've seen a <head> tag that might have had
  // attributes once already.
  bool mHaveSeenHead;

  // Boolean indicating whether we've notified insertion of our root content
  // yet.  We want to make sure to only do this once.
  bool mNotifiedRootInsertion;

  PRUint8 mScriptEnabled : 1;
  PRUint8 mFramesEnabled : 1;
  PRUint8 mFormOnStack : 1;
  PRUint8 unused : 5;  // bits available if someone needs one

  nsCOMPtr<nsIObserverEntry> mObservers;

  nsINodeInfo* mNodeInfoCache[NS_HTML_TAG_MAX + 1];

  nsresult FlushTags();

  void StartLayout(bool aIgnorePendingSheets);

  // Routines for tags that require special handling
  nsresult CloseHTML();
  nsresult OpenFrameset(const nsIParserNode& aNode);
  nsresult CloseFrameset();
  nsresult OpenBody(const nsIParserNode& aNode);
  nsresult CloseBody();
  nsresult OpenForm(const nsIParserNode& aNode);
  nsresult CloseForm();
  nsresult ProcessLINKTag(const nsIParserNode& aNode);

  // Routines for tags that require special handling when we reach their end
  // tag.
  nsresult ProcessSCRIPTEndTag(nsGenericHTMLElement* content,
                               bool aMalformed);
  nsresult ProcessSTYLEEndTag(nsGenericHTMLElement* content);

  nsresult OpenHeadContext();
  void CloseHeadContext();

  // nsContentSink overrides
  virtual void PreEvaluateScript();
  virtual void PostEvaluateScript(nsIScriptElement *aElement);

  void UpdateChildCounts();

  void NotifyInsert(nsIContent* aContent,
                    nsIContent* aChildContent,
                    PRInt32 aIndexInContainer);
  void NotifyRootInsertion();
  
  bool IsMonolithicContainer(nsHTMLTag aTag);

#ifdef NS_DEBUG
  void ForceReflow();
#endif
};

class SinkContext
{
public:
  SinkContext(HTMLContentSink* aSink);
  ~SinkContext();

  nsresult Begin(nsHTMLTag aNodeType, nsGenericHTMLElement* aRoot,
                 PRUint32 aNumFlushed, PRInt32 aInsertionPoint);
  nsresult OpenContainer(const nsIParserNode& aNode);
  nsresult CloseContainer(const nsHTMLTag aTag, bool aMalformed);
  nsresult AddLeaf(const nsIParserNode& aNode);
  nsresult AddLeaf(nsIContent* aContent);
  nsresult AddComment(const nsIParserNode& aNode);
  nsresult End();

  nsresult GrowStack();
  nsresult AddText(const nsAString& aText);
  nsresult FlushText(bool* aDidFlush = nsnull,
                     bool aReleaseLast = false);
  nsresult FlushTextAndRelease(bool* aDidFlush = nsnull)
  {
    return FlushText(aDidFlush, true);
  }

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
  PRInt32 mNotifyLevel;
  nsCOMPtr<nsIContent> mLastTextNode;
  PRInt32 mLastTextNodeSize;

  struct Node {
    nsHTMLTag mType;
    nsGenericHTMLElement* mContent;
    PRUint32 mNumFlushed;
    PRInt32 mInsertionPoint;

    nsIContent *Add(nsIContent *child);
  };

  Node* mStack;
  PRInt32 mStackSize;
  PRInt32 mStackPos;

  PRUnichar* mText;
  PRInt32 mTextLength;
  PRInt32 mTextSize;

private:
  bool mLastTextCharWasCR;
};

//----------------------------------------------------------------------

#ifdef NS_DEBUG
void
HTMLContentSink::SinkTraceNode(PRUint32 aBit,
                               const char* aMsg,
                               const nsHTMLTag aTag,
                               PRInt32 aStackPos,
                               void* aThis)
{
  if (SINK_LOG_TEST(gSinkLogModuleInfo, aBit)) {
    nsIParserService *parserService = nsContentUtils::GetParserService();
    if (!parserService)
      return;

    NS_ConvertUTF16toUTF8 tag(parserService->HTMLIdToStringTag(aTag));
    PR_LogPrint("%s: this=%p node='%s' stackPos=%d", 
                aMsg, aThis, tag.get(), aStackPos);
  }
}
#endif

nsresult
HTMLContentSink::AddAttributes(const nsIParserNode& aNode,
                               nsIContent* aContent, bool aNotify,
                               bool aCheckIfPresent)
{
  // Add tag attributes to the content attributes

  PRInt32 ac = aNode.GetAttributeCount();

  if (ac == 0) {
    // No attributes, nothing to do. Do an early return to avoid
    // constructing the nsAutoString object for nothing.

    return NS_OK;
  }

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

  // The attributes are on the parser node in the order they came in in the
  // source.  What we want to happen if a single attribute is set multiple
  // times on an element is that the first time should "win".  That is, <input
  // value="foo" value="bar"> should show "foo".  So we loop over the
  // attributes backwards; this ensures that the first attribute in the set
  // wins.  This does mean that we do some extra work in the case when the same
  // attribute is set multiple times, but we save a HasAttr call in the much
  // more common case of reasonable HTML.  Note that if aCheckIfPresent is set
  // then we actually want to loop _forwards_ to preserve the "first attribute
  // wins" behavior.  That does mean that when aCheckIfPresent is set the order
  // of attributes will get "reversed" from the point of view of the
  // serializer.  But aCheckIfPresent is only true for malformed documents with
  // multiple <html>, <head>, or <body> tags, so we're doing fixup anyway at
  // that point.

  PRInt32 i, limit, step;
  if (aCheckIfPresent) {
    i = 0;
    limit = ac;
    step = 1;
  } else {
    i = ac - 1;
    limit = -1;
    step = -1;
  }
  
  nsAutoString key;
  for (; i != limit; i += step) {
    // Get lower-cased key
    nsContentUtils::ASCIIToLower(aNode.GetKeyAt(i), key);

    nsCOMPtr<nsIAtom> keyAtom = do_GetAtom(key);

    if (aCheckIfPresent && aContent->HasAttr(kNameSpaceID_None, keyAtom)) {
      continue;
    }

    // Get value and remove mandatory quotes
    static const char* kWhitespace = "\n\r\t\b";

    // Bug 114997: Don't trim whitespace on <input value="...">:
    // Using ?: outside the function call would be more efficient, but
    // we don't trust ?: with references.
    const nsAString& v =
      nsContentUtils::TrimCharsInSet(
        (nodeType == eHTMLTag_input &&
          keyAtom == nsGkAtoms::value) ?
        "" : kWhitespace, aNode.GetValueAt(i));

    if (nodeType == eHTMLTag_a && keyAtom == nsGkAtoms::name) {
      NS_ConvertUTF16toUTF8 cname(v);
      NS_ConvertUTF8toUTF16 uv(nsUnescape(cname.BeginWriting()));

      // Add attribute to content
      aContent->SetAttr(kNameSpaceID_None, keyAtom, uv, aNotify);
    } else {
      // Add attribute to content
      aContent->SetAttr(kNameSpaceID_None, keyAtom, v, aNotify);
    }
  }

  return NS_OK;
}

static void
MaybeSetForm(nsGenericHTMLElement* aContent, nsHTMLTag aNodeType,
             HTMLContentSink* aSink)
{
  nsGenericHTMLElement* form = aSink->mCurrentForm;

  if (!form || aSink->mInsideNoXXXTag) {
    return;
  }

  switch (aNodeType) {
    case eHTMLTag_button:
    case eHTMLTag_fieldset:
    case eHTMLTag_label:
    case eHTMLTag_object:
    case eHTMLTag_input:
    case eHTMLTag_select:
    case eHTMLTag_textarea:
      break;
    default:
      return;
  }
  
  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(aContent));
  NS_ASSERTION(formControl,
               "nsGenericHTMLElement didn't implement nsIFormControl");
  nsCOMPtr<nsIDOMHTMLFormElement> formElement(do_QueryInterface(form));
  NS_ASSERTION(formElement,
               "nsGenericHTMLElement didn't implement nsIDOMHTMLFormElement");

  formControl->SetForm(formElement);
}

/**
 * Factory subroutine to create all of the html content objects.
 */
already_AddRefed<nsGenericHTMLElement>
HTMLContentSink::CreateContentObject(const nsIParserNode& aNode,
                                     nsHTMLTag aNodeType)
{
  // Find/create atom for the tag name

  nsCOMPtr<nsINodeInfo> nodeInfo;

  if (aNodeType == eHTMLTag_userdefined) {
    nsAutoString lower;
    nsContentUtils::ASCIIToLower(aNode.GetText(), lower);
    nsCOMPtr<nsIAtom> name = do_GetAtom(lower);
    nodeInfo = mNodeInfoManager->GetNodeInfo(name, nsnull, kNameSpaceID_XHTML,
                                             nsIDOMNode::ELEMENT_NODE);
  }
  else if (mNodeInfoCache[aNodeType]) {
    nodeInfo = mNodeInfoCache[aNodeType];
  }
  else {
    nsIParserService *parserService = nsContentUtils::GetParserService();
    if (!parserService)
      return nsnull;

    nsIAtom *name = parserService->HTMLIdToAtomTag(aNodeType);
    NS_ASSERTION(name, "What? Reverse mapping of id to string broken!!!");

    nodeInfo = mNodeInfoManager->GetNodeInfo(name, nsnull, kNameSpaceID_XHTML,
                                             nsIDOMNode::ELEMENT_NODE);
    NS_IF_ADDREF(mNodeInfoCache[aNodeType] = nodeInfo);
  }

  NS_ENSURE_TRUE(nodeInfo, nsnull);

  // Make the content object
  return CreateHTMLElement(aNodeType, nodeInfo.forget(), FROM_PARSER_NETWORK);
}

nsresult
NS_NewHTMLElement(nsIContent** aResult, already_AddRefed<nsINodeInfo> aNodeInfo,
                  FromParser aFromParser)
{
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> nodeInfo = aNodeInfo;

  nsIParserService* parserService = nsContentUtils::GetParserService();
  if (!parserService)
    return NS_ERROR_OUT_OF_MEMORY;

  nsIAtom *name = nodeInfo->NameAtom();

  NS_ASSERTION(nodeInfo->NamespaceEquals(kNameSpaceID_XHTML), 
               "Trying to HTML elements that don't have the XHTML namespace");
  
  *aResult = CreateHTMLElement(parserService->
                                 HTMLCaseSensitiveAtomTagToId(name),
                               nodeInfo.forget(), aFromParser).get();
  return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

already_AddRefed<nsGenericHTMLElement>
CreateHTMLElement(PRUint32 aNodeType, already_AddRefed<nsINodeInfo> aNodeInfo,
                  FromParser aFromParser)
{
  NS_ASSERTION(aNodeType <= NS_HTML_TAG_MAX ||
               aNodeType == eHTMLTag_userdefined,
               "aNodeType is out of bounds");

  contentCreatorCallback cb = sContentCreatorCallbacks[aNodeType];

  NS_ASSERTION(cb != NS_NewHTMLNOTUSEDElement,
               "Don't know how to construct tag element!");

  nsGenericHTMLElement* result = cb(aNodeInfo, aFromParser);
  NS_IF_ADDREF(result);

  return result;
}

//----------------------------------------------------------------------

SinkContext::SinkContext(HTMLContentSink* aSink)
  : mSink(aSink),
    mNotifyLevel(0),
    mLastTextNodeSize(0),
    mStack(nsnull),
    mStackSize(0),
    mStackPos(0),
    mText(nsnull),
    mTextLength(0),
    mTextSize(0),
    mLastTextCharWasCR(false)
{
  MOZ_COUNT_CTOR(SinkContext);
}

SinkContext::~SinkContext()
{
  MOZ_COUNT_DTOR(SinkContext);

  if (mStack) {
    for (PRInt32 i = 0; i < mStackPos; i++) {
      NS_RELEASE(mStack[i].mContent);
    }
    delete [] mStack;
  }

  delete [] mText;
}

nsresult
SinkContext::Begin(nsHTMLTag aNodeType,
                   nsGenericHTMLElement* aRoot,
                   PRUint32 aNumFlushed,
                   PRInt32 aInsertionPoint)
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
  mTextLength = 0;

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
  if ((mStackPos == 2) && (mSink->mBody == mStack[1].mContent ||
                           mSink->mFrameset == mStack[1].mContent)) {
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

#ifdef NS_DEBUG
    // Tracing code
    nsIParserService *parserService = nsContentUtils::GetParserService();
    if (parserService) {
      nsHTMLTag tag = nsHTMLTag(mStack[mStackPos - 1].mType);
      NS_ConvertUTF16toUTF8 str(parserService->HTMLIdToStringTag(tag));

      SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
                 ("SinkContext::DidAddContent: Insertion notification for "
                  "parent=%s at position=%d and stackPos=%d",
                  str.get(), mStack[mStackPos - 1].mInsertionPoint - 1,
                  mStackPos - 1));
    }
#endif

    PRInt32 childIndex = mStack[mStackPos - 1].mInsertionPoint - 1;
    NS_ASSERTION(parent->GetChildAt(childIndex) == aContent,
                 "Flushing the wrong child.");
    mSink->NotifyInsert(parent, aContent, childIndex);
    mStack[mStackPos - 1].mNumFlushed = parent->GetChildCount();
  } else if (mSink->IsTimeToNotify()) {
    SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("SinkContext::DidAddContent: Notification as a result of the "
                "interval expiring; backoff count: %d", mSink->mBackoffCount));
    FlushTags();
  }
}

nsresult
SinkContext::OpenContainer(const nsIParserNode& aNode)
{
  FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::OpenContainer", 
                  nsHTMLTag(aNode.GetNodeType()), 
                  mStackPos, 
                  mSink);

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

  // Create new container content object
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  nsGenericHTMLElement* content =
    mSink->CreateContentObject(aNode, nodeType).get();
  if (!content) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mStack[mStackPos].mType = nodeType;
  mStack[mStackPos].mContent = content;
  mStack[mStackPos].mNumFlushed = 0;
  mStack[mStackPos].mInsertionPoint = -1;
  ++mStackPos;

  // XXX Need to do this before we start adding attributes.
  if (nodeType == eHTMLTag_style) {
    nsCOMPtr<nsIStyleSheetLinkingElement> ssle = do_QueryInterface(content);
    NS_ASSERTION(ssle, "Style content isn't a style sheet?");
    ssle->SetLineNumber(aNode.GetSourceLineNumber());

    // Now disable updates so that every time we add an attribute or child
    // text token, we don't try to update the style sheet.
    if (!mSink->mInsideNoXXXTag) {
      ssle->InitStyleLinkElement(false);
    }
    else {
      // We're not going to be evaluating this style anyway.
      ssle->InitStyleLinkElement(true);
    }

    ssle->SetEnableUpdates(false);
  }
  
  rv = mSink->AddAttributes(aNode, content);
  MaybeSetForm(content, nodeType, mSink);

  mStack[mStackPos - 2].Add(content);

  NS_ENSURE_SUCCESS(rv, rv);

  if (mSink->IsMonolithicContainer(nodeType)) {
    mSink->mInMonolithicContainer++;
  }

  // Special handling for certain tags
  switch (nodeType) {
    case eHTMLTag_form:
      mSink->mCurrentForm = content;
      break;

    case eHTMLTag_frameset:
      if (!mSink->mFrameset && mSink->mFramesEnabled) {
        mSink->mFrameset = content;
      }
      break;

    case eHTMLTag_noembed:
    case eHTMLTag_noframes:
      mSink->mInsideNoXXXTag++;
      break;

    case eHTMLTag_iframe:
      mSink->mNumOpenIFRAMES++;
      break;

    case eHTMLTag_script:
      {
        nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(content);
        NS_ASSERTION(sele, "Script content isn't a script element?");
        sele->SetScriptLineNumber(aNode.GetSourceLineNumber());
      }
      break;

    case eHTMLTag_button:
      content->DoneCreatingElement();
      break;
      
    default:
      break;
  }

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
SinkContext::CloseContainer(const nsHTMLTag aTag, bool aMalformed)
{
  nsresult result = NS_OK;

  // Flush any collected text content. Release the last text
  // node to indicate that no more should be added to it.
  FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::CloseContainer", 
                  aTag, mStackPos - 1, mSink);

  NS_ASSERTION(mStackPos > 0,
               "stack out of bounds. wrong context probably!");

  if (mStackPos <= 0) {
    return NS_OK; // Fix crash - Ref. bug 45975 or 45007
  }

  --mStackPos;
  nsHTMLTag nodeType = mStack[mStackPos].mType;

  NS_ASSERTION(nodeType == eHTMLTag_form || nodeType == aTag,
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
#ifdef NS_DEBUG
      {
        // Tracing code
        SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
                   ("SinkContext::CloseContainer: reflow on notifyImmediate "
                    "tag=%s newIndex=%d stackPos=%d",
                    nsAtomCString(mStack[mStackPos].mContent->Tag()).get(),
                    mStack[mStackPos].mNumFlushed, mStackPos));
      }
#endif
      mSink->NotifyAppend(content, mStack[mStackPos].mNumFlushed);
      mStack[mStackPos].mNumFlushed = content->GetChildCount();
    }

    // Indicate that notification has now happened at this level
    mNotifyLevel = mStackPos - 1;
  }

  if (mSink->IsMonolithicContainer(nodeType)) {
    --mSink->mInMonolithicContainer;
  }

  DidAddContent(content);

  // Special handling for certain tags
  switch (nodeType) {
  case eHTMLTag_noembed:
  case eHTMLTag_noframes:
    // Fix bug 40216
    NS_ASSERTION((mSink->mInsideNoXXXTag > 0), "mInsideNoXXXTag underflow");
    if (mSink->mInsideNoXXXTag > 0) {
      mSink->mInsideNoXXXTag--;
    }

    break;
  case eHTMLTag_form:
    {
      mSink->mFormOnStack = false;
      // If there's a FORM on the stack, but this close tag doesn't
      // close the form, then close out the form *and* close out the
      // next container up. This is since the parser doesn't do fix up
      // of invalid form nesting. When the end FORM tag comes through,
      // we'll ignore it.
      if (aTag != nodeType) {
        result = CloseContainer(aTag, false);
      }
    }

    break;
  case eHTMLTag_iframe:
    mSink->mNumOpenIFRAMES--;

    break;

#ifdef MOZ_MEDIA
  case eHTMLTag_video:
  case eHTMLTag_audio:
#endif
  case eHTMLTag_select:
  case eHTMLTag_textarea:
  case eHTMLTag_object:
  case eHTMLTag_applet:
  case eHTMLTag_title:
    content->DoneAddingChildren(HaveNotifiedForCurrentContent());
    break;

  case eHTMLTag_script:
    result = mSink->ProcessSCRIPTEndTag(content,
                                        aMalformed);
    break;

  case eHTMLTag_style:
    result = mSink->ProcessSTYLEEndTag(content);
    break;

  default:
    break;
  }

  NS_IF_RELEASE(content);

#ifdef DEBUG
  if (SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return result;
}

nsresult
SinkContext::AddLeaf(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::AddLeaf", 
                  nsHTMLTag(aNode.GetNodeType()), 
                  mStackPos, mSink);

  nsresult rv = NS_OK;

  switch (aNode.GetTokenType()) {
  case eToken_start:
    {
      FlushTextAndRelease();

      // Create new leaf content object
      nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
      nsRefPtr<nsGenericHTMLElement> content =
        mSink->CreateContentObject(aNode, nodeType);
      NS_ENSURE_TRUE(content, NS_ERROR_OUT_OF_MEMORY);

      if (nodeType == eHTMLTag_form) {
        mSink->mCurrentForm = content;
      }

      rv = mSink->AddAttributes(aNode, content);

      NS_ENSURE_SUCCESS(rv, rv);

      MaybeSetForm(content, nodeType, mSink);

      // Add new leaf to its parent
      AddLeaf(content);

      // Additional processing needed once the element is in the tree
      switch (nodeType) {
      case eHTMLTag_meta:
        // XXX It's just not sufficient to check if the parent is head. Also
        // check for the preference.
        // Bug 40072: Don't evaluate METAs after FRAMESET.
        if (!mSink->mInsideNoXXXTag && !mSink->mFrameset) {
          rv = mSink->ProcessMETATag(content);
        }
        break;

      case eHTMLTag_input:
        content->DoneCreatingElement();
        break;

      case eHTMLTag_menuitem:
        content->DoneCreatingElement();
        break;

      default:
        break;
      }
    }
    break;

  case eToken_text:
  case eToken_whitespace:
  case eToken_newline:
    rv = AddText(aNode.GetText());

    break;
  case eToken_entity:
    {
      nsAutoString tmp;
      PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
      if (unicode < 0) {
        rv = AddText(aNode.GetText());
      } else {
        // Map carriage returns to newlines
        if (!tmp.IsEmpty()) {
          if (tmp.CharAt(0) == '\r') {
            tmp.Assign((PRUnichar)'\n');
          }
          rv = AddText(tmp);
        }
      }
    }

    break;
  default:
    break;
  }

  return rv;
}

nsresult
SinkContext::AddLeaf(nsIContent* aContent)
{
  NS_ASSERTION(mStackPos > 0, "leaf w/o container");
  if (mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }
  
  DidAddContent(mStack[mStackPos - 1].Add(aContent));

#ifdef DEBUG
  if (SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return NS_OK;
}

nsresult
SinkContext::AddComment(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::AddLeaf", 
                  nsHTMLTag(aNode.GetNodeType()), 
                  mStackPos, mSink);
  FlushTextAndRelease();

  if (!mSink) {
    return NS_ERROR_UNEXPECTED;
  }
  
  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment),
                                  mSink->mNodeInfoManager);
  NS_ENSURE_SUCCESS(rv, rv);

  comment->SetText(aNode.GetText(), false);

  NS_ASSERTION(mStackPos > 0, "stack out of bounds");
  if (mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }

  {
    Node &parentNode = mStack[mStackPos - 1];
    nsGenericHTMLElement *parent = parentNode.mContent;
    if (!mSink->mBody && !mSink->mFrameset && mSink->mHead)
      // XXXbz but this will make DidAddContent use the wrong parent for
      // the notification!  That seems so bogus it's not even funny.
      parentNode.mContent = mSink->mHead;
    DidAddContent(parentNode.Add(comment));
    parentNode.mContent = parent;
  }

#ifdef DEBUG
  if (SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return rv;
}

nsresult
SinkContext::End()
{
  for (PRInt32 i = 0; i < mStackPos; i++) {
    NS_RELEASE(mStack[i].mContent);
  }

  mStackPos = 0;
  mTextLength = 0;

  return NS_OK;
}

nsresult
SinkContext::GrowStack()
{
  PRInt32 newSize = mStackSize * 2;
  if (newSize == 0) {
    newSize = 32;
  }

  Node* stack = new Node[newSize];
  if (!stack) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mStackPos != 0) {
    memcpy(stack, mStack, sizeof(Node) * mStackPos);
    delete [] mStack;
  }

  mStack = stack;
  mStackSize = newSize;

  return NS_OK;
}

/**
 * Add textual content to the current running text buffer. If the text buffer
 * overflows, flush out the text by creating a text content object and adding
 * it to the content tree.
 */

// XXX If we get a giant string grow the buffer instead of chopping it
// up???
nsresult
SinkContext::AddText(const nsAString& aText)
{
  PRInt32 addLen = aText.Length();
  if (addLen == 0) {
    return NS_OK;
  }
  
  // Create buffer when we first need it
  if (mTextSize == 0) {
    mText = new PRUnichar[4096];
    if (!mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = 4096;
  }

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;

  while (addLen != 0) {
    PRInt32 amount = mTextSize - mTextLength;

    if (amount > addLen) {
      amount = addLen;
    }

    if (amount == 0) {
      // Don't release last text node so we can add to it again
      nsresult rv = FlushText();
      if (NS_FAILED(rv)) {
        return rv;
      }

      // Go back to the top of the loop so we re-calculate amount and
      // don't fall through to CopyNewlineNormalizedUnicodeTo with a
      // zero-length amount (which invalidates mLastTextCharWasCR).
      continue;
    }

    mTextLength +=
      nsContentUtils::CopyNewlineNormalizedUnicodeTo(aText, offset,
                                                     &mText[mTextLength],
                                                     amount,
                                                     mLastTextCharWasCR);
    offset += amount;
    addLen -= amount;
  }

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
  PRUint32 oldUpdates = mSink->mUpdatesInNotification;

  ++(mSink->mInNotification);
  mSink->mUpdatesInNotification = 0;
  {
    // Scope so we call EndUpdate before we decrease mInNotification
    mozAutoDocUpdate updateBatch(mSink->mDocument, UPDATE_CONTENT_MODEL,
                                 true);
    mSink->mBeganUpdate = true;

    // Don't release last text node in case we need to add to it again
    FlushText();

    // Start from the base of the stack (growing downward) and do
    // a notification from the node that is closest to the root of
    // tree for any content that has been added.

    // Note that we can start at stackPos == 0 here, because it's the caller's
    // responsibility to handle flushing interactions between contexts (see
    // HTMLContentSink::BeginContext).
    PRInt32 stackPos = 0;
    bool flushed = false;
    PRUint32 childCount;
    nsGenericHTMLElement* content;

    while (stackPos < mStackPos) {
      content = mStack[stackPos].mContent;
      childCount = content->GetChildCount();

      if (!flushed && (mStack[stackPos].mNumFlushed < childCount)) {
#ifdef NS_DEBUG
        {
          // Tracing code
          SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
                     ("SinkContext::FlushTags: tag=%s from newindex=%d at "
                      "stackPos=%d",
                      nsAtomCString(mStack[stackPos].mContent->Tag()).get(),
                      mStack[stackPos].mNumFlushed, stackPos));
        }
#endif
        if (mStack[stackPos].mInsertionPoint != -1) {
          // We might have popped the child off our stack already
          // but not notified on it yet, which is why we have to get it
          // directly from its parent node.

          PRInt32 childIndex = mStack[stackPos].mInsertionPoint - 1;
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
  PRInt32 stackPos = mStackPos - 1;
  while (stackPos >= 0) {
    Node & node = mStack[stackPos];
    node.mNumFlushed = node.mContent->GetChildCount();

    stackPos--;
  }

  mNotifyLevel = mStackPos - 1;
}

/**
 * Flush any buffered text out by creating a text content object and
 * adding it to the content.
 */
nsresult
SinkContext::FlushText(bool* aDidFlush, bool aReleaseLast)
{
  nsresult rv = NS_OK;
  bool didFlush = false;

  if (mTextLength != 0) {
    if (mLastTextNode) {
      if ((mLastTextNodeSize + mTextLength) > mSink->mMaxTextRun) {
        mLastTextNodeSize = 0;
        mLastTextNode = nsnull;
        FlushText(aDidFlush, aReleaseLast);
      } else {
        bool notify = HaveNotifiedForCurrentContent();
        // We could probably always increase mInNotification here since
        // if AppendText doesn't notify it shouldn't trigger evil code.
        // But just in case it does, we don't want to mask any notifications.
        if (notify) {
          ++mSink->mInNotification;
        }
        rv = mLastTextNode->AppendText(mText, mTextLength, notify);
        if (notify) {
          --mSink->mInNotification;
        }

        mLastTextNodeSize += mTextLength;
        mTextLength = 0;
        didFlush = true;
      }
    } else {
      nsCOMPtr<nsIContent> textContent;
      rv = NS_NewTextNode(getter_AddRefs(textContent),
                          mSink->mNodeInfoManager);
      NS_ENSURE_SUCCESS(rv, rv);

      mLastTextNode = textContent;

      // Set the text in the text node
      mLastTextNode->SetText(mText, mTextLength, false);

      // Eat up the rest of the text up in state.
      mLastTextNodeSize += mTextLength;
      mTextLength = 0;

      rv = AddLeaf(mLastTextNode);
      NS_ENSURE_SUCCESS(rv, rv);

      didFlush = true;
    }
  }

  if (aDidFlush) {
    *aDidFlush = didFlush;
  }

  if (aReleaseLast) {
    mLastTextNodeSize = 0;
    mLastTextNode = nsnull;
    mLastTextCharWasCR = false;
  }

#ifdef DEBUG
  if (didFlush &&
      SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return rv;
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

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aDoc, aURI, aContainer, aChannel);

  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = it;
  NS_ADDREF(*aResult);

  return NS_OK;
}

HTMLContentSink::HTMLContentSink()
{
  // Note: operator new zeros our memory


#ifdef NS_DEBUG
  if (!gSinkLogModuleInfo) {
    gSinkLogModuleInfo = PR_NewLogModule("htmlcontentsink");
  }
#endif
}

HTMLContentSink::~HTMLContentSink()
{
  if (mNotificationTimer) {
    mNotificationTimer->Cancel();
  }

  PRInt32 numContexts = mContextStack.Length();

  if (mCurrentContext == mHeadContext && numContexts > 0) {
    // Pop off the second html context if it's not done earlier
    mContextStack.RemoveElementAt(--numContexts);
  }

  PRInt32 i;
  for (i = 0; i < numContexts; i++) {
    SinkContext* sc = mContextStack.ElementAt(i);
    if (sc) {
      sc->End();
      if (sc == mCurrentContext) {
        mCurrentContext = nsnull;
      }

      delete sc;
    }
  }

  if (mCurrentContext == mHeadContext) {
    mCurrentContext = nsnull;
  }

  delete mCurrentContext;

  delete mHeadContext;

  for (i = 0; PRUint32(i) < ArrayLength(mNodeInfoCache); ++i) {
    NS_IF_RELEASE(mNodeInfoCache[i]);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLContentSink)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLContentSink, nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mHTMLDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mBody)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFrameset)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mHead)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCurrentForm)
  for (PRUint32 i = 0; i < ArrayLength(tmp->mNodeInfoCache); ++i) {
    NS_IF_RELEASE(tmp->mNodeInfoCache[i]);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLContentSink,
                                                  nsContentSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mHTMLDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mBody)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFrameset)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mHead)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCurrentForm)
  for (PRUint32 i = 0; i < ArrayLength(tmp->mNodeInfoCache); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mNodeInfoCache[i]");
    cb.NoteXPCOMChild(tmp->mNodeInfoCache[i]);
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLContentSink)
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY(HTMLContentSink, nsIContentSink)
    NS_INTERFACE_TABLE_ENTRY(HTMLContentSink, nsIHTMLContentSink)
#if DEBUG
    NS_INTERFACE_TABLE_ENTRY(HTMLContentSink, nsIDebugDumpContent)
#endif
  NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL_INHERITING(nsContentSink)

NS_IMPL_ADDREF_INHERITED(HTMLContentSink, nsContentSink)
NS_IMPL_RELEASE_INHERITED(HTMLContentSink, nsContentSink)

static bool
IsScriptEnabled(nsIDocument *aDoc, nsIDocShell *aContainer)
{
  NS_ENSURE_TRUE(aDoc && aContainer, true);

  nsCOMPtr<nsIScriptGlobalObject> globalObject = aDoc->GetScriptGlobalObject();

  // Getting context is tricky if the document hasn't had its
  // GlobalObject set yet
  if (!globalObject) {
    nsCOMPtr<nsIScriptGlobalObjectOwner> owner = do_GetInterface(aContainer);
    NS_ENSURE_TRUE(owner, true);

    globalObject = owner->GetScriptGlobalObject();
    NS_ENSURE_TRUE(globalObject, true);
  }

  nsIScriptContext *scriptContext = globalObject->GetContext();
  NS_ENSURE_TRUE(scriptContext, true);

  JSContext* cx = scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, true);

  bool enabled = true;
  nsContentUtils::GetSecurityManager()->
    CanExecuteScripts(cx, aDoc->NodePrincipal(), &enabled);
  return enabled;
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

  mObservers = nsnull;
  nsIParserService* service = nsContentUtils::GetParserService();
  if (!service) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  service->GetTopicObservers(NS_LITERAL_STRING("text/html"),
                             getter_AddRefs(mObservers));

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
  nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::html, nsnull,
                                           kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

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
                                           nsnull, kNameSpaceID_XHTML,
                                           nsIDOMNode::ELEMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  mHead = NS_NewHTMLHeadElement(nodeInfo.forget());
  if (NS_FAILED(rv)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRoot->AppendChildTo(mHead, false);

  mCurrentContext = new SinkContext(this);
  NS_ENSURE_TRUE(mCurrentContext, NS_ERROR_OUT_OF_MEMORY);
  mCurrentContext->Begin(eHTMLTag_html, mRoot, 0, -1);
  mContextStack.AppendElement(mCurrentContext);

#ifdef NS_DEBUG
  nsCAutoString spec;
  (void)aURI->GetSpec(spec);
  SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_CALLS,
             ("HTMLContentSink::Init: this=%p url='%s'",
              this, spec.get()));
#endif
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
  if (mBody || mFrameset) {
    SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: layout final content"));
    mCurrentContext->FlushTags();
  } else if (!mLayoutStarted) {
    // We never saw the body, and layout never got started. Force
    // layout *now*, to get an initial reflow.
    SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: forcing reflow on empty "
                "document"));

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

  mDocument->ScriptLoader()->RemoveObserver(this);

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
HTMLContentSink::SetParser(nsIParser* aParser)
{
  NS_PRECONDITION(aParser, "Should have a parser here!");
  mParser = aParser;
  return NS_OK;
}

NS_IMETHODIMP_(bool)
HTMLContentSink::IsFormOnStack()
{
  return mFormOnStack;
}

NS_IMETHODIMP
HTMLContentSink::BeginContext(PRInt32 aPosition)
{
  NS_PRECONDITION(aPosition > -1, "out of bounds");

  if (!mCurrentContext) {
    NS_ERROR("Nonexistent context");

    return NS_ERROR_FAILURE;
  }

  // Flush everything in the current context so that we don't have
  // to worry about insertions resulting in inconsistent frame creation.
  mCurrentContext->FlushTags();

  // Sanity check.
  if (mCurrentContext->mStackPos <= aPosition) {
    NS_ERROR("Out of bounds position");
    return NS_ERROR_FAILURE;
  }

  PRInt32 insertionPoint = -1;
  nsHTMLTag nodeType      = mCurrentContext->mStack[aPosition].mType;
  nsGenericHTMLElement* content = mCurrentContext->mStack[aPosition].mContent;

  // If the content under which the new context is created
  // has a child on the stack, the insertion point is
  // before the last child.
  if (aPosition < (mCurrentContext->mStackPos - 1)) {
    insertionPoint = content->GetChildCount() - 1;
  }

  SinkContext* sc = new SinkContext(this);
  sc->Begin(nodeType,
            content,
            mCurrentContext->mStack[aPosition].mNumFlushed,
            insertionPoint);
  NS_ADDREF(sc->mSink);

  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = sc;
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::EndContext(PRInt32 aPosition)
{
  NS_PRECONDITION(mCurrentContext && aPosition > -1, "nonexistent context");

  PRUint32 n = mContextStack.Length() - 1;
  SinkContext* sc = mContextStack.ElementAt(n);

  const SinkContext::Node &bottom = mCurrentContext->mStack[0];
  
  NS_ASSERTION(sc->mStack[aPosition].mType == bottom.mType,
               "ending a wrong context");

  mCurrentContext->FlushTextAndRelease();
  
  NS_ASSERTION(bottom.mContent->GetChildCount() == bottom.mNumFlushed,
               "Node at base of context stack not fully flushed.");

  // Flushing tags before the assertion on the previous line would
  // undoubtedly prevent the assertion from failing, but it shouldn't
  // be failing anyway, FlushTags or no.  Flushing here is nevertheless
  // a worthwhile precaution, since we lose some information (e.g.,
  // mInsertionPoints) when we end the current context.
  mCurrentContext->FlushTags();

  sc->mStack[aPosition].mNumFlushed = bottom.mNumFlushed;

  for (PRInt32 i = 0; i<mCurrentContext->mStackPos; i++) {
    NS_IF_RELEASE(mCurrentContext->mStack[i].mContent);
  }

  delete [] mCurrentContext->mStack;

  mCurrentContext->mStack      = nsnull;
  mCurrentContext->mStackPos   = 0;
  mCurrentContext->mStackSize  = 0;

  delete [] mCurrentContext->mText;

  mCurrentContext->mText       = nsnull;
  mCurrentContext->mTextLength = 0;
  mCurrentContext->mTextSize   = 0;

  NS_IF_RELEASE(mCurrentContext->mSink);

  delete mCurrentContext;

  mCurrentContext = sc;
  mContextStack.RemoveElementAt(n);
  return NS_OK;
}

nsresult
HTMLContentSink::CloseHTML()
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                 "HTMLContentSink::CloseHTML", 
                 eHTMLTag_html, 0, this);

  if (mHeadContext) {
    if (mCurrentContext == mHeadContext) {
      PRUint32 numContexts = mContextStack.Length();

      // Pop off the second html context if it's not done earlier
      mCurrentContext = mContextStack.ElementAt(--numContexts);
      mContextStack.RemoveElementAt(numContexts);
    }

    NS_ASSERTION(mHeadContext->mTextLength == 0, "Losing text");

    mHeadContext->End();

    delete mHeadContext;
    mHeadContext = nsnull;
  }

  return NS_OK;
}

nsresult
HTMLContentSink::OpenHead()
{
  nsresult rv = OpenHeadContext();
  return rv;
}

nsresult
HTMLContentSink::OpenBody(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenBody", 
                  eHTMLTag_body,
                  mCurrentContext->mStackPos, 
                  this);

  CloseHeadContext();  // do this just in case if the HEAD was left open!

  // Add attributes, if any, to the current BODY node
  if (mBody) {
    AddAttributes(aNode, mBody, true, true);
    return NS_OK;
  }

  nsresult rv = mCurrentContext->OpenContainer(aNode);

  if (NS_FAILED(rv)) {
    return rv;
  }

  mBody = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;

  if (mCurrentContext->mStackPos > 1) {
    PRInt32 parentIndex    = mCurrentContext->mStackPos - 2;
    nsGenericHTMLElement *parent = mCurrentContext->mStack[parentIndex].mContent;
    PRInt32 numFlushed     = mCurrentContext->mStack[parentIndex].mNumFlushed;
    PRInt32 childCount = parent->GetChildCount();
    NS_ASSERTION(numFlushed < childCount, "Already notified on the body?");
    
    PRInt32 insertionPoint =
      mCurrentContext->mStack[parentIndex].mInsertionPoint;

    // XXX: I have yet to see a case where numFlushed is non-zero and
    // insertionPoint is not -1, but this code will try to handle
    // those cases too.

    PRUint32 oldUpdates = mUpdatesInNotification;
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
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseBody", 
                  eHTMLTag_body,
                  mCurrentContext->mStackPos - 1, 
                  this);

  bool didFlush;
  nsresult rv = mCurrentContext->FlushTextAndRelease(&didFlush);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Flush out anything that's left
  SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
             ("HTMLContentSink::CloseBody: layout final body content"));

  mCurrentContext->FlushTags();
  mCurrentContext->CloseContainer(eHTMLTag_body, false);

  return NS_OK;
}

nsresult
HTMLContentSink::OpenForm(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  mCurrentContext->FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenForm", 
                  eHTMLTag_form,
                  mCurrentContext->mStackPos, 
                  this);

  // Close out previous form if it's there. If there is one
  // around, it's probably because the last one wasn't well-formed.
  mCurrentForm = nsnull;

  // Check if the parent is a table, tbody, thead, tfoot, tr, col or
  // colgroup. If so, we fix up by making the form leaf content.
  if (mCurrentContext->IsCurrentContainer(eHTMLTag_table) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tbody) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_thead) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tfoot) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tr) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_col) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_colgroup)) {
    result = mCurrentContext->AddLeaf(aNode);
  } else {
    mFormOnStack = true;
    // Otherwise the form can be a content parent.
    result = mCurrentContext->OpenContainer(aNode);
  }

  return result;
}

nsresult
HTMLContentSink::CloseForm()
{
  nsresult result = NS_OK;

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseForm",
                  eHTMLTag_form,
                  mCurrentContext->mStackPos - 1, 
                  this);

  if (mCurrentForm) {
    // if this is a well-formed form, close it too
    if (mCurrentContext->IsCurrentContainer(eHTMLTag_form)) {
      result = mCurrentContext->CloseContainer(eHTMLTag_form, false);
      mFormOnStack = false;
    }

    mCurrentForm = nsnull;
  }

  return result;
}

nsresult
HTMLContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenFrameset", 
                  eHTMLTag_frameset,
                  mCurrentContext->mStackPos, 
                  this);

  CloseHeadContext(); // do this just in case if the HEAD was left open!

  // Need to keep track of whether OpenContainer changes mFrameset
  nsGenericHTMLElement* oldFrameset = mFrameset;
  nsresult rv = mCurrentContext->OpenContainer(aNode);
  bool isFirstFrameset = NS_SUCCEEDED(rv) && mFrameset != oldFrameset;

  if (isFirstFrameset && mCurrentContext->mStackPos > 1) {
    NS_ASSERTION(mFrameset, "Must have frameset!");
    // Have to notify for the frameset now, since we never actually
    // close out <html>, so won't notify for it then.
    PRInt32 parentIndex    = mCurrentContext->mStackPos - 2;
    nsGenericHTMLElement *parent = mCurrentContext->mStack[parentIndex].mContent;
    PRInt32 numFlushed     = mCurrentContext->mStack[parentIndex].mNumFlushed;
    PRInt32 childCount = parent->GetChildCount();
    NS_ASSERTION(numFlushed < childCount, "Already notified on the frameset?");

    PRInt32 insertionPoint =
      mCurrentContext->mStack[parentIndex].mInsertionPoint;

    // XXX: I have yet to see a case where numFlushed is non-zero and
    // insertionPoint is not -1, but this code will try to handle
    // those cases too.

    PRUint32 oldUpdates = mUpdatesInNotification;
    mUpdatesInNotification = 0;
    if (insertionPoint != -1) {
      NotifyInsert(parent, mFrameset, insertionPoint - 1);
    } else {
      NotifyAppend(parent, numFlushed);
    }
    mCurrentContext->mStack[parentIndex].mNumFlushed = childCount;
    if (mUpdatesInNotification > 1) {
      UpdateChildCounts();
    }
    mUpdatesInNotification = oldUpdates;
  }
  
  return rv;
}

nsresult
HTMLContentSink::CloseFrameset()
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                   "HTMLContentSink::CloseFrameset", 
                   eHTMLTag_frameset,
                   mCurrentContext->mStackPos - 1,
                   this);

  SinkContext* sc = mCurrentContext;
  nsGenericHTMLElement* fs = sc->mStack[sc->mStackPos - 1].mContent;
  bool done = fs == mFrameset;

  nsresult rv;
  if (done) {
    bool didFlush;
    rv = sc->FlushTextAndRelease(&didFlush);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Flush out anything that's left
    SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("HTMLContentSink::CloseFrameset: layout final content"));

    sc->FlushTags();
  }

  rv = sc->CloseContainer(eHTMLTag_frameset, false);    

  if (done && mFramesEnabled) {
    StartLayout(false);
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::IsEnabled(PRInt32 aTag, bool* aReturn)
{
  nsHTMLTag theHTMLTag = nsHTMLTag(aTag);

  if (theHTMLTag == eHTMLTag_script) {
    *aReturn = mScriptEnabled;
  } else if (theHTMLTag == eHTMLTag_frameset) {
    *aReturn = mFramesEnabled;
  } else {
    *aReturn = false;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;

  switch (aNode.GetNodeType()) {
    case eHTMLTag_frameset:
      rv = OpenFrameset(aNode);
      break;
    case eHTMLTag_head:
      rv = OpenHeadContext();
      if (NS_SUCCEEDED(rv)) {
        rv = AddAttributes(aNode, mHead, true, mHaveSeenHead);
        mHaveSeenHead = true;
      }
      break;
    case eHTMLTag_body:
      rv = OpenBody(aNode);
      break;
    case eHTMLTag_html:
      if (mRoot) {
        // If we've already hit this code once, need to check for
        // already-present attributes on the root.
        AddAttributes(aNode, mRoot, true, mNotifiedRootInsertion);
        if (!mNotifiedRootInsertion) {
          NotifyRootInsertion();
        }
        ProcessOfflineManifest(mRoot);
      }
      break;
    case eHTMLTag_form:
      rv = OpenForm(aNode);
      break;
    default:
      rv = mCurrentContext->OpenContainer(aNode);
      break;
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const eHTMLTags aTag)
{
  nsresult rv = NS_OK;

  switch (aTag) {
    case eHTMLTag_frameset:
      rv = CloseFrameset();
      break;
    case eHTMLTag_head:
      CloseHeadContext();
      break;
    case eHTMLTag_body:
      rv = CloseBody();
      break;
    case eHTMLTag_html:
      rv = CloseHTML();
      break;
    case eHTMLTag_form:
      rv = CloseForm();
      break;
    default:
      rv = mCurrentContext->CloseContainer(aTag, false);
      break;
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseMalformedContainer(const eHTMLTags aTag)
{
  return mCurrentContext->CloseContainer(aTag, true);
}

NS_IMETHODIMP
HTMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  nsresult rv;

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  switch (nodeType) {
  case eHTMLTag_link:
    mCurrentContext->FlushTextAndRelease();
    rv = ProcessLINKTag(aNode);

    break;
  default:
    rv = mCurrentContext->AddLeaf(aNode);

    break;
  }

  return rv;
}

/**
 * This gets called by the parsing system when we find a comment
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
nsresult
HTMLContentSink::AddComment(const nsIParserNode& aNode)
{
  nsresult rv = mCurrentContext->AddComment(aNode);

  return rv;
}

/**
 * This gets called by the parsing system when we find a PI
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
nsresult
HTMLContentSink::AddProcessingInstruction(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  // Implementation of AddProcessingInstruction() should start here

  return result;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
HTMLContentSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
  nsAutoString docTypeStr(aNode.GetText());
  nsresult rv = NS_OK;

  PRInt32 publicStart = docTypeStr.Find("PUBLIC", true);
  PRInt32 systemStart = docTypeStr.Find("SYSTEM", true);
  nsAutoString name, publicId, systemId;

  if (publicStart >= 0 || systemStart >= 0) {
    /*
     * If we find the keyword 'PUBLIC' after the keyword 'SYSTEM' we assume
     * that we got a system id that happens to contain the string "PUBLIC"
     * and we ignore that as the start of the public id.
     */
    if (systemStart >= 0 && (publicStart > systemStart)) {
      publicStart = -1;
    }

    /*
     * We found 'PUBLIC' or 'SYSTEM' in the doctype, put everything before
     * the first one of those in name.
     */
    docTypeStr.Mid(name, 0, publicStart >= 0 ? publicStart : systemStart);

    if (publicStart >= 0) {
      // We did find 'PUBLIC'
      docTypeStr.Mid(publicId, publicStart + 6,
                     docTypeStr.Length() - publicStart);
      publicId.Trim(" \t\n\r");

      // Strip quotes
      PRUnichar ch = publicId.IsEmpty() ? '\0' : publicId.First();

      bool hasQuote = false;
      if (ch == '"' || ch == '\'') {
        publicId.Cut(0, 1);

        PRInt32 end = publicId.FindChar(ch);

        if (end < 0) {
          /*
           * We didn't find an end quote, so just make sure we cut off
           * the '>' on the end of the doctype declaration
           */

          end = publicId.FindChar('>');
        } else {
          hasQuote = true;
        }

        /*
         * If we didn't find a closing quote or a '>' we leave publicId as
         * it is.
         */
        if (end >= 0) {
          publicId.Truncate(end);
        }
      } else {
        // No quotes, ignore the public id
        publicId.Truncate();
      }

      /*
       * Make sure the 'SYSTEM' word we found is not inside the pubilc id
       */
      PRInt32 pos = docTypeStr.Find(publicId);

      if (systemStart > 0) {
        if (systemStart < pos + (PRInt32)publicId.Length()) {
          systemStart = docTypeStr.Find("SYSTEM", true,
                                        pos + publicId.Length());
        }
      }

      /*
       * If we didn't find 'SYSTEM' we treat everything after the public id
       * as the system id.
       */
      if (systemStart < 0) {
        // 1 is the end quote
        systemStart = pos + publicId.Length() + (hasQuote ? 1 : 0);
      }
    }

    if (systemStart >= 0) {
      // We either found 'SYSTEM' or we have data after the public id
      docTypeStr.Mid(systemId, systemStart,
                     docTypeStr.Length() - systemStart);

      // Strip off 'SYSTEM' if we have it.
      if (StringBeginsWith(systemId, NS_LITERAL_STRING("SYSTEM")))
        systemId.Cut(0, 6);

      systemId.Trim(" \t\n\r");

      // Strip quotes
      PRUnichar ch = systemId.IsEmpty() ? '\0' : systemId.First();

      if (ch == '"' || ch == '\'') {
        systemId.Cut(0, 1);

        PRInt32 end = systemId.FindChar(ch);

        if (end < 0) {
          // We didn't find an end quote, then we just make sure we
          // cut of the '>' on the end of the doctype declaration

          end = systemId.FindChar('>');
        }

        // If we found an closing quote nor a '>' we truncate systemId
        // at that length.
        if (end >= 0) {
          systemId.Truncate(end);
        }
      } else {
        systemId.Truncate();
      }
    }
  } else {
    name.Assign(docTypeStr);
  }

  // Cut out "<!DOCTYPE" or "DOCTYPE" from the name.
  if (StringBeginsWith(name, NS_LITERAL_STRING("<!DOCTYPE"), nsCaseInsensitiveStringComparator())) {
    name.Cut(0, 9);
  } else if (StringBeginsWith(name, NS_LITERAL_STRING("DOCTYPE"), nsCaseInsensitiveStringComparator())) {
    name.Cut(0, 7);
  }

  name.Trim(" \t\n\r");

  // Check if name contains whitespace chars. If it does and the first
  // char is not a quote, we set the name to contain the characters
  // before the whitespace
  PRInt32 nameEnd = 0;

  if (name.IsEmpty() || (name.First() != '"' && name.First() != '\'')) {
    nameEnd = name.FindCharInSet(" \n\r\t");
  }

  // If we didn't find a public id we grab everything after the name
  // and use that as public id.
  if (publicStart < 0) {
    name.Mid(publicId, nameEnd, name.Length() - nameEnd);
    publicId.Trim(" \t\n\r");

    PRUnichar ch = publicId.IsEmpty() ? '\0' : publicId.First();

    if (ch == '"' || ch == '\'') {
      publicId.Cut(0, 1);

      PRInt32 publicEnd = publicId.FindChar(ch);

      if (publicEnd < 0) {
        publicEnd = publicId.FindChar('>');
      }

      if (publicEnd < 0) {
        publicEnd = publicId.Length();
      }

      publicId.Truncate(publicEnd);
    } else {
      // No quotes, no public id
      publicId.Truncate();
    }
  }

  if (nameEnd >= 0) {
    name.Truncate(nameEnd);
  } else {
    nameEnd = name.FindChar('>');

    if (nameEnd >= 0) {
      name.Truncate(nameEnd);
    }
  }

  if (!publicId.IsEmpty() || !systemId.IsEmpty() || !name.IsEmpty()) {
    nsCOMPtr<nsIDOMDocumentType> oldDocType;
    nsCOMPtr<nsIDOMDocumentType> docType;

    nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mHTMLDocument));
    doc->GetDoctype(getter_AddRefs(oldDocType));

    // Assign "HTML" if we don't have anything, and normalize
    // the name if it is something like "hTmL", per HTML5.
    if (name.IsEmpty() || name.LowerCaseEqualsLiteral("html")) {
      name.AssignLiteral("HTML");
    }

    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(name);
    if (!nameAtom) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Indicate that there is no internal subset (not just an empty one)
    nsAutoString voidString;
    voidString.SetIsVoid(true);
    rv = NS_NewDOMDocumentType(getter_AddRefs(docType),
                               mDocument->NodeInfoManager(), nameAtom,
                               publicId, systemId, voidString);
    NS_ENSURE_SUCCESS(rv, rv);

    if (oldDocType) {
      // If we already have a doctype we replace the old one.
      nsCOMPtr<nsIDOMNode> tmpNode;
      rv = doc->ReplaceChild(oldDocType, docType, getter_AddRefs(tmpNode));
    } else {
      // If we don't already have one we insert it as the first child,
      // this might not be 100% correct but since this is called from
      // the content sink we assume that this is what we want.
      nsCOMPtr<nsIContent> content = do_QueryInterface(docType);
      NS_ASSERTION(content, "Doctype isn't content?");
      
      mDocument->InsertChildAt(content, 0, true);
    }
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::DidProcessTokens(void)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillProcessAToken(void)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidProcessAToken(void)
{
  return DidProcessATokenImpl();
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

NS_IMETHODIMP
HTMLContentSink::NotifyTagObservers(nsIParserNode* aNode)
{
  // Bug 125317
  // Inform observers that we're handling a document.write().
  // This information is necessary for the charset observer, atleast,
  // to make a decision whether a new charset loading is required or not.

  if (!mObservers) {
    return NS_OK;
  }

  PRUint32 flag = 0;

  if (mHTMLDocument && mHTMLDocument->IsWriting()) {
    flag = nsIElementObserver::IS_DOCUMENT_WRITE;
  }

  return mObservers->Notify(aNode, mParser, mDocShell, flag);
}

void
HTMLContentSink::StartLayout(bool aIgnorePendingSheets)
{
  if (mLayoutStarted) {
    return;
  }

  mHTMLDocument->SetIsFrameset(mFrameset != nsnull);

  nsContentSink::StartLayout(aIgnorePendingSheets);
}

nsresult
HTMLContentSink::OpenHeadContext()
{
  if (mCurrentContext && mCurrentContext->IsCurrentContainer(eHTMLTag_head))
    return NS_OK;

  // Flush everything in the current context so that we don't have
  // to worry about insertions resulting in inconsistent frame creation.
  //
  // Try to do this only if needed (costly), i.e., only if we are sure
  // we are changing contexts from some other context to the head.
  //
  // PERF: This call causes approximately a 2% slowdown in page load time
  // according to jrgm's page load tests, but seems to be a necessary evil
  if (mCurrentContext && (mCurrentContext != mHeadContext)) {
    mCurrentContext->FlushTags();
  }

  if (!mHeadContext) {
    mHeadContext = new SinkContext(this);
    NS_ENSURE_TRUE(mHeadContext, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = mHeadContext->Begin(eHTMLTag_head, mHead, 0, -1);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = mHeadContext;

  return NS_OK;
}

void
HTMLContentSink::CloseHeadContext()
{
  if (mCurrentContext) {
    if (!mCurrentContext->IsCurrentContainer(eHTMLTag_head))
      return;

    mCurrentContext->FlushTextAndRelease();
    mCurrentContext->FlushTags();
  }

  if (!mContextStack.IsEmpty())
  {
    PRUint32 n = mContextStack.Length() - 1;
    mCurrentContext = mContextStack.ElementAt(n);
    mContextStack.RemoveElementAt(n);
  }
}

nsresult
HTMLContentSink::ProcessLINKTag(const nsIParserNode& aNode)
{
  nsresult  result = NS_OK;

  if (mCurrentContext) {
    // Create content object
    nsCOMPtr<nsIContent> element;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    nodeInfo = mNodeInfoManager->GetNodeInfo(nsGkAtoms::link, nsnull,
                                             kNameSpaceID_XHTML,
                                             nsIDOMNode::ELEMENT_NODE);

    result = NS_NewHTMLElement(getter_AddRefs(element), nodeInfo.forget(),
                               NOT_FROM_PARSER);
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(element));

    if (ssle) {
      // XXX need prefs. check here.
      if (!mInsideNoXXXTag) {
        ssle->InitStyleLinkElement(false);
        ssle->SetEnableUpdates(false);
      } else {
        ssle->InitStyleLinkElement(true);
      }
    }

    // Add in the attributes and add the style content object to the
    // head container.
    result = AddAttributes(aNode, element);
    if (NS_FAILED(result)) {
      return result;
    }

    mCurrentContext->AddLeaf(element); // <link>s are leaves

    if (ssle) {
      ssle->SetEnableUpdates(true);
      bool willNotify;
      bool isAlternate;
      result = ssle->UpdateStyleSheet(mFragmentMode ? nsnull : this,
                                      &willNotify,
                                      &isAlternate);
      if (NS_SUCCEEDED(result) && willNotify && !isAlternate && !mFragmentMode) {
        ++mPendingSheetCount;
        mScriptLoader->AddExecuteBlocker();
      }

      // look for <link rel="next" href="url">
      nsAutoString relVal;
      element->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, relVal);
      if (!relVal.IsEmpty()) {
        // XXX seems overkill to generate this string array
        nsAutoTArray<nsString, 4> linkTypes;
        nsStyleLinkElement::ParseLinkTypes(relVal, linkTypes);
        bool hasPrefetch = linkTypes.Contains(NS_LITERAL_STRING("prefetch"));
        if (hasPrefetch || linkTypes.Contains(NS_LITERAL_STRING("next"))) {
          nsAutoString hrefVal;
          element->GetAttr(kNameSpaceID_None, nsGkAtoms::href, hrefVal);
          if (!hrefVal.IsEmpty()) {
            PrefetchHref(hrefVal, element, hasPrefetch);
          }
        }
        if (linkTypes.Contains(NS_LITERAL_STRING("dns-prefetch"))) {
          nsAutoString hrefVal;
          element->GetAttr(kNameSpaceID_None, nsGkAtoms::href, hrefVal);
          if (!hrefVal.IsEmpty()) {
            PrefetchDNS(hrefVal);
          }
        }
      }
    }
  }

  return result;
}

#ifdef DEBUG
void
HTMLContentSink::ForceReflow()
{
  mCurrentContext->FlushTags();
}
#endif

void
HTMLContentSink::NotifyInsert(nsIContent* aContent,
                              nsIContent* aChildContent,
                              PRInt32 aIndexInContainer)
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
  PRInt32 index = mDocument->IndexOf(mRoot);
  NS_ASSERTION(index != -1, "mRoot not child of document?");
  NotifyInsert(nsnull, mRoot, index);

  // Now update the notification information in all our
  // contexts, since we just inserted the root and notified on
  // our whole tree
  UpdateChildCounts();
}

bool
HTMLContentSink::IsMonolithicContainer(nsHTMLTag aTag)
{
  if (aTag == eHTMLTag_tr     ||
      aTag == eHTMLTag_select ||
      aTag == eHTMLTag_applet ||
      aTag == eHTMLTag_object) {
    return true;
  }

  return false;
}

void
HTMLContentSink::UpdateChildCounts()
{
  PRUint32 numContexts = mContextStack.Length();
  for (PRUint32 i = 0; i < numContexts; i++) {
    SinkContext* sc = mContextStack.ElementAt(i);

    sc->UpdateChildCounts();
  }

  mCurrentContext->UpdateChildCounts();
}

void
HTMLContentSink::PreEvaluateScript()
{
  // Eagerly append all pending elements (including the current body child)
  // to the body (so that they can be seen by scripts) and force reflow.
  SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_CALLS,
             ("HTMLContentSink::PreEvaluateScript: flushing tags before "
              "evaluating script"));

  // XXX Should this call FlushTags()?
  mCurrentContext->FlushText();
}

void
HTMLContentSink::PostEvaluateScript(nsIScriptElement *aElement)
{
  mHTMLDocument->ScriptExecuted(aElement);
}

nsresult
HTMLContentSink::ProcessSCRIPTEndTag(nsGenericHTMLElement *content,
                                     bool aMalformed)
{
  // Flush all tags up front so that we are in as stable state as possible
  // when calling DoneAddingChildren. This may not be strictly needed since
  // any ScriptAvailable calls will cause us to flush anyway. But it gives a
  // warm fuzzy feeling to be in a stable state before even attempting to
  // run scripts.
  // It would however be needed if we properly called BeginUpdate and
  // EndUpdate while we were inserting stuff into the DOM.

  // XXX Should this call FlushTags()?
  mCurrentContext->FlushText();

  nsCOMPtr<nsIScriptElement> sele = do_QueryInterface(content);
  NS_ASSERTION(sele, "Not really closing a script tag?");

  if (aMalformed) {
    // Make sure to serialize this script correctly, for nice round tripping.
    sele->SetIsMalformed();
  }
  if (mFrameset) {
    sele->PreventExecution();
  }

  // Notify our document that we're loading this script.
  mHTMLDocument->ScriptLoading(sele);

  // Now tell the script that it's ready to go. This may execute the script
  // or return true, or neither if the script doesn't need executing.
  bool block = sele->AttemptToExecute();

  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  if (block) {
    // If this append fails we'll never unblock the parser, but the UI will
    // still remain responsive. There are other ways to deal with this, but
    // the end result is always that the page gets botched, so there is no
    // real point in making it more complicated.
    mScriptElements.AppendObject(sele);
  } else {
    // This may have already happened if the script executed, but in case
    // it didn't then remove the element so that it doesn't get stuck forever.
    mHTMLDocument->ScriptExecuted(sele);
  }

  // If the parser got blocked, make sure to return the appropriate rv.
  // I'm not sure if this is actually needed or not.
  if (mParser && !mParser->IsParserEnabled()) {
    block = true;
  }

  return block ? NS_ERROR_HTMLPARSER_BLOCK : NS_OK;
}

// 3 ways to load a style sheet: inline, style src=, link tag
// XXX What does nav do if we have SRC= and some style data inline?

nsresult
HTMLContentSink::ProcessSTYLEEndTag(nsGenericHTMLElement* content)
{
  nsCOMPtr<nsIStyleSheetLinkingElement> ssle = do_QueryInterface(content);

  NS_ASSERTION(ssle,
               "html:style doesn't implement nsIStyleSheetLinkingElement");

  nsresult rv = NS_OK;

  if (ssle) {
    // Note: if we are inside a noXXX tag, then we init'ed this style element
    // with mDontLoadStyle = true, so these two calls will have no effect.
    ssle->SetEnableUpdates(true);
    bool willNotify;
    bool isAlternate;
    rv = ssle->UpdateStyleSheet(mFragmentMode ? nsnull : this,
                                &willNotify,
                                &isAlternate);
    if (NS_SUCCEEDED(rv) && willNotify && !isAlternate && !mFragmentMode) {
      ++mPendingSheetCount;
      mScriptLoader->AddExecuteBlocker();
    }
  }

  return rv;
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
      else if (mCurrentContext) {
        mCurrentContext->FlushText();
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
  if (mDocShell) {
    // the following logic to get muCV is copied from
    // nsHTMLDocument::StartDocumentLoad
    // We need to call muCV->SetPrevDocCharacterSet here in case
    // the charset is detected by parser DetectMetaTag
    nsCOMPtr<nsIMarkupDocumentViewer> muCV;
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
       muCV = do_QueryInterface(cv);
    } else {
      // in this block of code, if we get an error result, we return
      // it but if we get a null pointer, that's perfectly legal for
      // parent and parentContentViewer

      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
        do_QueryInterface(mDocShell);
      NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);

      nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
      docShellAsItem->GetSameTypeParent(getter_AddRefs(parentAsItem));

      nsCOMPtr<nsIDocShell> parent(do_QueryInterface(parentAsItem));
      if (parent) {
        nsCOMPtr<nsIContentViewer> parentContentViewer;
        nsresult rv =
          parent->GetContentViewer(getter_AddRefs(parentContentViewer));
        if (NS_SUCCEEDED(rv) && parentContentViewer) {
          muCV = do_QueryInterface(parentContentViewer);
        }
      }
    }

    if (muCV) {
      muCV->SetPrevDocCharacterSet(aCharset);
    }
  }

  if (mDocument) {
    mDocument->SetDocumentCharacterSet(aCharset);
  }

  return NS_OK;
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

#ifdef DEBUG
/**
 *  This will dump content model into the output file.
 *
 *  @update  harishd 05/25/00
 *  @param
 *  @return  NS_OK all went well, error on failure
 */

NS_IMETHODIMP
HTMLContentSink::DumpContentModel()
{
  FILE* out = ::fopen("rtest_html.txt", "a");
  if (out) {
    if (mDocument) {
      Element* root = mDocument->GetRootElement();
      if (root) {
        if (mDocumentURI) {
          nsCAutoString buf;
          mDocumentURI->GetSpec(buf);
          fputs(buf.get(), out);
        }

        fputs(";", out);
        root->DumpContent(out, 0, false);
        fputs(";\n", out);
      }
    }

    fclose(out);
  }

  return NS_OK;
}
#endif


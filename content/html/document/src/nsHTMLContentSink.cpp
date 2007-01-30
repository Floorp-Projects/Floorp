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
#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsINodeInfo.h"
#include "nsHTMLTokens.h"
#include "nsIAppShell.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "prtime.h"
#include "prlog.h"
#include "nsInt64.h"
#include "nsNodeUtils.h"
#include "nsIContent.h"

#include "nsGenericHTMLElement.h"

#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIScriptElement.h"

#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIDocShell.h"
#include "nsIDocument.h"
#include "nsStubDocumentObserver.h"
#include "nsIHTMLDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsICookieService.h"
#include "nsVoidArray.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsTextFragment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"

#include "nsIParserService.h"
#include "nsISelectElement.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsTimer.h"
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

#ifdef NS_DEBUG
static PRLogModuleInfo* gSinkLogModuleInfo;

#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj) \
  _obj->SinkTraceNode(_bit, _msg, _tag, _sp, this)

#else
#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj)
#endif

//----------------------------------------------------------------------

typedef nsGenericHTMLElement* (*contentCreatorCallback)(nsINodeInfo*, PRBool aFromParser);

nsGenericHTMLElement*
NS_NewHTMLNOTUSEDElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
{
  NS_NOTREACHED("The element ctor should never be called");
  return nsnull;
}

#define HTML_TAG(_tag, _classname) NS_NewHTML##_classname##Element,
#define HTML_OTHER(_tag) NS_NewHTMLNOTUSEDElement,
static const contentCreatorCallback sContentCreatorCallbacks[] = {
  NS_NewHTMLUnknownElement,
#include "nsHTMLTagList.h"
#undef HTML_TAG
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

  // nsIContentSink
  NS_IMETHOD WillTokenize(void);
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(void);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  virtual void FlushPendingNotifications(mozFlushType aType);
  NS_IMETHOD SetDocumentCharset(nsACString& aCharset);
  virtual nsISupports *GetTarget();

  // nsIHTMLContentSink
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
  NS_IMETHOD CloseMalformedContainer(const nsHTMLTag aTag);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode);
  NS_IMETHOD WillProcessTokens(void);
  NS_IMETHOD DidProcessTokens(void);
  NS_IMETHOD WillProcessAToken(void);
  NS_IMETHOD DidProcessAToken(void);
  NS_IMETHOD NotifyTagObservers(nsIParserNode* aNode);
  NS_IMETHOD BeginContext(PRInt32 aID);
  NS_IMETHOD EndContext(PRInt32 aID);
  NS_IMETHOD OpenHead();
  NS_IMETHOD IsEnabled(PRInt32 aTag, PRBool* aReturn);
  NS_IMETHOD_(PRBool) IsFormOnStack();

#ifdef DEBUG
  // nsIDebugDumpContent
  NS_IMETHOD DumpContentModel();
#endif

protected:
  nsresult UpdateDocumentTitle();
  // If aCheckIfPresent is true, will only set an attribute in cases
  // when it's not already set.
  nsresult AddAttributes(const nsIParserNode& aNode, nsIContent* aContent,
                         PRBool aNotify = PR_FALSE,
                         PRBool aCheckIfPresent = PR_FALSE);
  already_AddRefed<nsGenericHTMLElement>
  CreateContentObject(const nsIParserNode& aNode, nsHTMLTag aNodeType);

#ifdef NS_DEBUG
  void SinkTraceNode(PRUint32 aBit,
                     const char* aMsg,
                     const nsHTMLTag aTag,
                     PRInt32 aStackPos,
                     void* aThis);
#endif

  nsIHTMLDocument* mHTMLDocument;

  // The maximum length of a text run
  PRInt32 mMaxTextRun;

  nsGenericHTMLElement* mRoot;
  nsGenericHTMLElement* mBody;
  nsRefPtr<nsGenericHTMLElement> mFrameset;
  nsGenericHTMLElement* mHead;

  PRPackedBool mInTitle;

  nsString mTitleString;
  nsRefPtr<nsGenericHTMLElement> mCurrentForm;

  nsAutoVoidArray mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;
  PRInt32 mNumOpenIFRAMES;

  nsCOMPtr<nsIURI> mBaseHref;
  nsCOMPtr<nsIAtom> mBaseTarget;

  // depth of containment within <noembed>, <noframes> etc
  PRInt32 mInsideNoXXXTag;

  // Boolean indicating whether we've seen a <head> tag that might have had
  // attributes once already.
  PRPackedBool mHaveSeenHead;

  nsCOMPtr<nsIObserverEntry> mObservers;

  nsINodeInfo* mNodeInfoCache[NS_HTML_TAG_MAX + 1];

  nsresult FlushTags();

  void StartLayout();

  void TryToScrollToRef();

  /**
   * AddBaseTagInfo adds the "current" base URI and target to the content node
   * in the form of bogo-attributes.  This MUST be called before attributes are
   * added to the content node, since the way URI attributes are treated may
   * depend on the value of the base URI
   */
  void AddBaseTagInfo(nsIContent* aContent);

  // Routines for tags that require special handling
  nsresult CloseHTML();
  nsresult OpenFrameset(const nsIParserNode& aNode);
  nsresult CloseFrameset();
  nsresult OpenBody(const nsIParserNode& aNode);
  nsresult CloseBody();
  nsresult OpenForm(const nsIParserNode& aNode);
  nsresult CloseForm();
  void ProcessBASEElement(nsGenericHTMLElement* aElement);
  nsresult ProcessLINKTag(const nsIParserNode& aNode);

  // Routines for tags that require special handling when we reach their end
  // tag.
  nsresult ProcessSCRIPTEndTag(nsGenericHTMLElement* content,
                               PRBool aMalformed);
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
  PRBool IsMonolithicContainer(nsHTMLTag aTag);

#ifdef NS_DEBUG
  void ForceReflow();
#endif

  // Measures content model creation time for current document
  MOZ_TIMER_DECLARE(mWatch)
};

class SinkContext
{
public:
  SinkContext(HTMLContentSink* aSink);
  ~SinkContext();

  nsresult Begin(nsHTMLTag aNodeType, nsGenericHTMLElement* aRoot,
                 PRUint32 aNumFlushed, PRInt32 aInsertionPoint);
  nsresult OpenContainer(const nsIParserNode& aNode);
  nsresult CloseContainer(const nsHTMLTag aTag, PRBool aMalformed);
  nsresult AddLeaf(const nsIParserNode& aNode);
  nsresult AddLeaf(nsGenericHTMLElement* aContent);
  nsresult AddComment(const nsIParserNode& aNode);
  nsresult End();

  nsresult GrowStack();
  nsresult AddText(const nsAString& aText);
  nsresult FlushText(PRBool* aDidFlush = nsnull,
                     PRBool aReleaseLast = PR_FALSE);
  nsresult FlushTextAndRelease(PRBool* aDidFlush = nsnull)
  {
    return FlushText(aDidFlush, PR_TRUE);
  }

  nsresult FlushTags();

  PRBool   IsCurrentContainer(nsHTMLTag mType);
  PRBool   IsAncestorContainer(nsHTMLTag mType);

  void DidAddContent(nsIContent* aContent);
  void UpdateChildCounts();

private:
  // Function to check whether we've notified for the current content.
  // What this actually does is check whether we've notified for all
  // of the parent's kids.
  PRBool HaveNotifiedForCurrentContent() const;
  
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
  };

  Node* mStack;
  PRInt32 mStackSize;
  PRInt32 mStackPos;

  PRUnichar* mText;
  PRInt32 mTextLength;
  PRInt32 mTextSize;
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
                               nsIContent* aContent, PRBool aNotify,
                               PRBool aCheckIfPresent)
{
  // Add tag attributes to the content attributes

  PRInt32 ac = aNode.GetAttributeCount();

  if (ac == 0) {
    // No attributes, nothing to do. Do an early return to avoid
    // constructing the nsAutoString object for nothing.

    return NS_OK;
  }

  nsCAutoString k;
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
  
  for (; i != limit; i += step) {
    // Get lower-cased key
    const nsAString& key = aNode.GetKeyAt(i);
    // Copy up-front to avoid shared-buffer overhead (and convert to UTF-8
    // at the same time since that's what the atom table uses).
    CopyUTF16toUTF8(key, k);
    ToLowerCase(k);

    nsCOMPtr<nsIAtom> keyAtom = do_GetAtom(k);

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
    case eHTMLTag_legend:
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
  NS_ASSERTION(!form || formElement,
               "nsGenericHTMLElement didn't implement nsIDOMHTMLFormElement");

  formControl->SetForm(formElement, PR_TRUE, PR_FALSE);
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
    NS_ConvertUTF16toUTF8 tmp(aNode.GetText());
    ToLowerCase(tmp);

    nsCOMPtr<nsIAtom> name = do_GetAtom(tmp);
    mNodeInfoManager->GetNodeInfo(name, nsnull, kNameSpaceID_None,
                                  getter_AddRefs(nodeInfo));
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

    mNodeInfoManager->GetNodeInfo(name, nsnull, kNameSpaceID_None,
                                  getter_AddRefs(nodeInfo));
    NS_IF_ADDREF(mNodeInfoCache[aNodeType] = nodeInfo);
  }

  NS_ENSURE_TRUE(nodeInfo, nsnull);

  // Make the content object
  return CreateHTMLElement(aNodeType, nodeInfo, PR_TRUE);
}

nsresult
NS_NewHTMLElement(nsIContent** aResult, nsINodeInfo *aNodeInfo)
{
  *aResult = nsnull;

  nsIParserService* parserService = nsContentUtils::GetParserService();
  if (!parserService)
    return NS_ERROR_OUT_OF_MEMORY;

  nsIAtom *name = aNodeInfo->NameAtom();

#ifdef DEBUG
  if (aNodeInfo->NamespaceEquals(kNameSpaceID_None)) {
    nsAutoString nameStr, lname;
    name->ToString(nameStr);
    ToLowerCase(nameStr, lname);
    NS_ASSERTION(nameStr.Equals(lname), "name should be lowercase by now");
    NS_ASSERTION(!aNodeInfo->GetPrefixAtom(), "should not have a prefix");
  }
#endif
  
  *aResult = CreateHTMLElement(parserService->
                                 HTMLCaseSensitiveAtomTagToId(name),
                               aNodeInfo, PR_FALSE).get();
  return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

already_AddRefed<nsGenericHTMLElement>
CreateHTMLElement(PRUint32 aNodeType, nsINodeInfo *aNodeInfo,
                  PRBool aFromParser)
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
    mTextSize(0)
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

PRBool
SinkContext::IsCurrentContainer(nsHTMLTag aTag)
{
  if (aTag == mStack[mStackPos - 1].mType) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
SinkContext::IsAncestorContainer(nsHTMLTag aTag)
{
  PRInt32 stackPos = mStackPos - 1;

  while (stackPos >= 0) {
    if (aTag == mStack[stackPos].mType) {
      return PR_TRUE;
    }
    stackPos--;
  }

  return PR_FALSE;
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

    mSink->NotifyInsert(parent, aContent,
                        mStack[mStackPos - 1].mInsertionPoint - 1);
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
      ssle->InitStyleLinkElement(mSink->mParser, PR_FALSE);
    }
    else {
      // We're not going to be evaluating this style anyway.
      ssle->InitStyleLinkElement(nsnull, PR_TRUE);
    }

    ssle->SetEnableUpdates(PR_FALSE);
  }

  // Make sure to add base tag info, if needed, before setting any other
  // attributes -- what URI attrs do will depend on the base URI.  Only do this
  // for elements that have useful URI attributes.
  // See bug 18478 and bug 30617 for why we need to do this.
  switch (nodeType) {
    // Containers with "href="
    case eHTMLTag_a:
    case eHTMLTag_map:

    // Containers with "src="
    case eHTMLTag_script:
    
    // Containers with "action="
    case eHTMLTag_form:

    // Containers with "data="
    case eHTMLTag_object:

    // Containers with "background="
    case eHTMLTag_table:
    case eHTMLTag_thead:
    case eHTMLTag_tbody:
    case eHTMLTag_tfoot:
    case eHTMLTag_tr:
    case eHTMLTag_td:
    case eHTMLTag_th:
      mSink->AddBaseTagInfo(content);

      break;
    default:
      break;    
  }
  
  rv = mSink->AddAttributes(aNode, content);
  MaybeSetForm(content, nodeType, mSink);

  nsGenericHTMLElement* parent = mStack[mStackPos - 2].mContent;

  if (mStack[mStackPos - 2].mInsertionPoint != -1) {
    parent->InsertChildAt(content,
                          mStack[mStackPos - 2].mInsertionPoint++,
                          PR_FALSE);
  } else {
    parent->AppendChildTo(content, PR_FALSE);
  }

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

    case eHTMLTag_title:
      if (mSink->mDocument->GetDocumentTitle().IsVoid()) {
        // The first title wins.
        mSink->mInTitle = PR_TRUE;
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

PRBool
SinkContext::HaveNotifiedForCurrentContent() const
{
  if (0 < mStackPos) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;
    return mStack[mStackPos-1].mNumFlushed == parent->GetChildCount();
  }

  return PR_TRUE;
}

nsresult
SinkContext::CloseContainer(const nsHTMLTag aTag, PRBool aMalformed)
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
        const char *tagStr;
        mStack[mStackPos].mContent->Tag()->GetUTF8String(&tagStr);

        SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
                   ("SinkContext::CloseContainer: reflow on notifyImmediate "
                    "tag=%s newIndex=%d stackPos=%d",
                    tagStr,
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
      mSink->mFormOnStack = PR_FALSE;
      // If there's a FORM on the stack, but this close tag doesn't
      // close the form, then close out the form *and* close out the
      // next container up. This is since the parser doesn't do fix up
      // of invalid form nesting. When the end FORM tag comes through,
      // we'll ignore it.
      if (aTag != nodeType) {
        result = CloseContainer(aTag, PR_FALSE);
      }
    }

    break;
  case eHTMLTag_iframe:
    mSink->mNumOpenIFRAMES--;

    break;

  case eHTMLTag_select:
  case eHTMLTag_textarea:
  case eHTMLTag_object:
  case eHTMLTag_applet:
    content->DoneAddingChildren(HaveNotifiedForCurrentContent());
    break;

  case eHTMLTag_script:
    result = mSink->ProcessSCRIPTEndTag(content,
                                        aMalformed);
    break;

  case eHTMLTag_style:
    result = mSink->ProcessSTYLEEndTag(content);
    break;

  case eHTMLTag_title:
    if (mSink->mInTitle) {
      mSink->UpdateDocumentTitle();
      mSink->mInTitle = PR_FALSE;
    }
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

      // Make sure to add base tag info, if needed, before setting any other
      // attributes -- what URI attrs do will depend on the base URI.  Only do
      // this for elements that have useful URI attributes.
      // See bug 18478 and bug 30617 for why we need to do this.
      switch (nodeType) {
      case eHTMLTag_area:
      case eHTMLTag_meta:
      case eHTMLTag_img:
      case eHTMLTag_frame:
      case eHTMLTag_input:
      case eHTMLTag_embed:
        mSink->AddBaseTagInfo(content);
        break;

      // <form> can end up as a leaf if it's misnested with table elements
      case eHTMLTag_form:
        mSink->AddBaseTagInfo(content);
        mSink->mCurrentForm = content;

        break;
      default:
        break;
      }

      rv = mSink->AddAttributes(aNode, content);

      NS_ENSURE_SUCCESS(rv, rv);

      MaybeSetForm(content, nodeType, mSink);

      // Add new leaf to its parent
      AddLeaf(content);

      // Additional processing needed once the element is in the tree
      switch (nodeType) {
      case eHTMLTag_base:
        if (!mSink->mInsideNoXXXTag) {
          mSink->ProcessBASEElement(content);
        }
        break;

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
SinkContext::AddLeaf(nsGenericHTMLElement* aContent)
{
  NS_ASSERTION(mStackPos > 0, "leaf w/o container");
  if (mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }

  nsGenericHTMLElement* parent = mStack[mStackPos - 1].mContent;

  // If the parent has an insertion point, insert rather than append.
  if (mStack[mStackPos - 1].mInsertionPoint != -1) {
    parent->InsertChildAt(aContent,
                          mStack[mStackPos - 1].mInsertionPoint++,
                          PR_FALSE);
  } else {
    parent->AppendChildTo(aContent, PR_FALSE);
  }

  DidAddContent(aContent);

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

  comment->SetText(aNode.GetText(), PR_FALSE);

  NS_ASSERTION(mStackPos > 0, "stack out of bounds");
  if (mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }

  nsGenericHTMLElement* parent;
  if (!mSink->mBody && !mSink->mFrameset && mSink->mHead) {
    // XXXbz but this will make DidAddContent use the wrong parent for
    // the notification!  That seems so bogus it's not even funny.
    parent = mSink->mHead;
  } else {
    parent = mStack[mStackPos - 1].mContent;
  }

  // If the parent has an insertion point, insert rather than append.
  if (mStack[mStackPos - 1].mInsertionPoint != -1) {
    parent->InsertChildAt(comment,
                          mStack[mStackPos - 1].mInsertionPoint++,
                          PR_FALSE);
  } else {
    parent->AppendChildTo(comment, PR_FALSE);
  }

  DidAddContent(comment);

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
  
  if (mSink->mInTitle) {
    // Hang onto the title text specially.
    mSink->mTitleString.Append(aText);
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
  PRBool  isLastCharCR = PR_FALSE;

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
    }

    mTextLength +=
      nsContentUtils::CopyNewlineNormalizedUnicodeTo(aText, offset,
                                                     &mText[mTextLength],
                                                     amount, isLastCharCR);
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
  ++(mSink->mInNotification);
  mozAutoDocUpdate updateBatch(mSink->mDocument, UPDATE_CONTENT_MODEL,
                               PR_TRUE);

  // Don't release last text node in case we need to add to it again
  FlushText();

  // Start from the base of the stack (growing downward) and do
  // a notification from the node that is closest to the root of
  // tree for any content that has been added.

  // Note that we can start at stackPos == 0 here, because it's the caller's
  // responsibility to handle flushing interactions between contexts (see
  // HTMLContentSink::BeginContext).
  PRInt32 stackPos = 0;
  PRBool flushed = PR_FALSE;
  PRUint32 childCount;
  nsGenericHTMLElement* content;

  while (stackPos < mStackPos) {
    content = mStack[stackPos].mContent;
    childCount = content->GetChildCount();

    if (!flushed && (mStack[stackPos].mNumFlushed < childCount)) {
#ifdef NS_DEBUG
      {
        // Tracing code
        const char* tagStr;
        mStack[stackPos].mContent->Tag()->GetUTF8String(&tagStr);

        SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
                   ("SinkContext::FlushTags: tag=%s from newindex=%d at "
                    "stackPos=%d", tagStr,
                    mStack[stackPos].mNumFlushed, stackPos));
      }
#endif
      if ((mStack[stackPos].mInsertionPoint != -1) &&
          (mStackPos > (stackPos + 1))) {
        nsIContent* child = mStack[stackPos + 1].mContent;
        mSink->NotifyInsert(content,
                            child,
                            mStack[stackPos].mInsertionPoint);
      } else {
        mSink->NotifyAppend(content, mStack[stackPos].mNumFlushed);
      }

      flushed = PR_TRUE;
    }

    mStack[stackPos].mNumFlushed = childCount;
    stackPos++;
  }
  mNotifyLevel = mStackPos - 1;
  --(mSink->mInNotification);

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
SinkContext::FlushText(PRBool* aDidFlush, PRBool aReleaseLast)
{
  nsresult rv = NS_OK;
  PRBool didFlush = PR_FALSE;

  if (mTextLength != 0) {
    if (mLastTextNode) {
      if ((mLastTextNodeSize + mTextLength) > mSink->mMaxTextRun) {
        mLastTextNodeSize = 0;
        mLastTextNode = nsnull;
        FlushText(aDidFlush, aReleaseLast);
      } else {
        PRBool notify = HaveNotifiedForCurrentContent();
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
        didFlush = PR_TRUE;
      }
    } else {
      nsCOMPtr<nsIContent> textContent;
      rv = NS_NewTextNode(getter_AddRefs(textContent),
                          mSink->mNodeInfoManager);
      NS_ENSURE_SUCCESS(rv, rv);

      mLastTextNode = textContent;

      // Set the text in the text node
      mLastTextNode->SetText(mText, mTextLength, PR_FALSE);

      // Eat up the rest of the text up in state.
      mLastTextNodeSize += mTextLength;
      mTextLength = 0;

      // Add text to its parent
      NS_ASSERTION(mStackPos > 0, "leaf w/o container");
      if (mStackPos <= 0) {
        return NS_ERROR_FAILURE;
      }

      nsGenericHTMLElement* parent = mStack[mStackPos - 1].mContent;
      if (mStack[mStackPos - 1].mInsertionPoint != -1) {
        parent->InsertChildAt(mLastTextNode,
                              mStack[mStackPos - 1].mInsertionPoint++,
                              PR_FALSE);
      } else {
        parent->AppendChildTo(mLastTextNode, PR_FALSE);
      }

      didFlush = PR_TRUE;

      DidAddContent(mLastTextNode);
    }
  }

  if (aDidFlush) {
    *aDidFlush = didFlush;
  }

  if (aReleaseLast) {
    mLastTextNodeSize = 0;
    mLastTextNode = nsnull;
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
  NS_IF_RELEASE(mHead);
  NS_IF_RELEASE(mBody);
  NS_IF_RELEASE(mRoot);

  if (mDocument) {
    // Remove ourselves just to be safe, though we really should have
    // been removed in DidBuildModel if everything worked right.
    mDocument->RemoveObserver(this);
  }
  NS_IF_RELEASE(mHTMLDocument);

  if (mNotificationTimer) {
    mNotificationTimer->Cancel();
  }

  PRInt32 numContexts = mContextStack.Count();

  if (mCurrentContext == mHeadContext && numContexts > 0) {
    // Pop off the second html context if it's not done earlier
    mContextStack.RemoveElementAt(--numContexts);
  }

  PRInt32 i;
  for (i = 0; i < numContexts; i++) {
    SinkContext* sc = (SinkContext*)mContextStack.ElementAt(i);
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

  for (i = 0; i < NS_ARRAY_LENGTH(mNodeInfoCache); ++i) {
    NS_IF_RELEASE(mNodeInfoCache[i]);
  }
}

#if DEBUG
NS_IMPL_ISUPPORTS_INHERITED6(HTMLContentSink,
                             nsContentSink,
                             nsIContentSink,
                             nsIHTMLContentSink,
                             nsITimerCallback,
                             nsIDocumentObserver,
                             nsIMutationObserver,
                             nsIDebugDumpContent)
#else
NS_IMPL_ISUPPORTS_INHERITED5(HTMLContentSink,
                             nsContentSink,
                             nsIContentSink,
                             nsIHTMLContentSink,
                             nsITimerCallback,
                             nsIDocumentObserver,
                             nsIMutationObserver)
#endif

static PRBool
IsScriptEnabled(nsIDocument *aDoc, nsIDocShell *aContainer)
{
  NS_ENSURE_TRUE(aDoc && aContainer, PR_TRUE);

  nsCOMPtr<nsIScriptGlobalObject> globalObject = aDoc->GetScriptGlobalObject();

  // Getting context is tricky if the document hasn't had it's
  // GlobalObject set yet
  if (!globalObject) {
    nsCOMPtr<nsIScriptGlobalObjectOwner> owner = do_GetInterface(aContainer);
    NS_ENSURE_TRUE(owner, PR_TRUE);

    globalObject = owner->GetScriptGlobalObject();
    NS_ENSURE_TRUE(globalObject, PR_TRUE);
  }

  nsIScriptContext *scriptContext = globalObject->GetContext();
  NS_ENSURE_TRUE(scriptContext, PR_TRUE);

  JSContext* cx = (JSContext *) scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, PR_TRUE);

  PRBool enabled = PR_TRUE;
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


  MOZ_TIMER_DEBUGLOG(("Reset and start: nsHTMLContentSink::Init(), this=%p\n",
                      this));
  MOZ_TIMER_RESET(mWatch);
  MOZ_TIMER_START(mWatch);

  nsresult rv = nsContentSink::Init(aDoc, aURI, aContainer, aChannel);
  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }

  aDoc->AddObserver(this);
  CallQueryInterface(aDoc, &mHTMLDocument);

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
    PRBool subFramesEnabled = PR_TRUE;
    mDocShell->GetAllowSubframes(&subFramesEnabled);
    if (subFramesEnabled) {
      mFramesEnabled = PR_TRUE;
    }
  }

  // Find out if scripts are enabled, if not, show <noscript> content
  if (IsScriptEnabled(aDoc, mDocShell)) {
    mScriptEnabled = PR_TRUE;
  }


  // Changed from 8192 to greatly improve page loading performance on
  // large pages.  See bugzilla bug 77540.
  mMaxTextRun = nsContentUtils::GetIntPref("content.maxtextrun", 8191);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsGkAtoms::html, nsnull,
                                     kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // Make root part
  nsIContent *doc_root = mDocument->GetRootContent();

  if (doc_root) {
    // If the document already has a root we'll use it. This will
    // happen when we do document.open()/.write()/.close()...

    NS_ADDREF(mRoot = NS_STATIC_CAST(nsGenericHTMLElement*, doc_root));
  } else {
    mRoot = NS_NewHTMLHtmlElement(nodeInfo);
    if (!mRoot) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
      MOZ_TIMER_STOP(mWatch);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mRoot);

    NS_ASSERTION(mDocument->GetChildCount() == 0,
                 "Document should have no kids here!");
    rv = mDocument->AppendChildTo(mRoot, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Make head part
  rv = mNodeInfoManager->GetNodeInfo(nsGkAtoms::head,
                                     nsnull, kNameSpaceID_None,
                                     getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  mHead = NS_NewHTMLHeadElement(nodeInfo);
  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(mHead);

  mRoot->AppendChildTo(mHead, PR_FALSE);

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

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillBuildModel(void)
{
  WillBuildModelImpl();
  if (mHTMLDocument) {
    NS_ASSERTION(mParser, "no parser");
    nsCompatibility mode = eCompatibility_NavQuirks;
    if (mParser) {
      nsDTDMode dtdMode = mParser->GetParseMode();
      switch (dtdMode) {
        case eDTDMode_full_standards:
          mode = eCompatibility_FullStandards;

          break;
        case eDTDMode_almost_standards:
          mode = eCompatibility_AlmostStandards;

          break;
        default:
          mode = eCompatibility_NavQuirks;

          break;
      }
    }
    mHTMLDocument->SetCompatibilityMode(mode);
  }

  // Notify document that the load is beginning
  mDocument->BeginLoad();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidBuildModel(void)
{

  // NRA Dump stopwatch stop info here
#ifdef MOZ_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::DidBuildModel(), this=%p\n",
                      this));
  MOZ_TIMER_STOP(mWatch);
  MOZ_TIMER_LOG(("Content creation time (this=%p): ", this));
  MOZ_TIMER_PRINT(mWatch);
#endif

  DidBuildModelImpl();

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
    PRBool bDestroying = PR_TRUE;
    if (mDocShell) {
      mDocShell->IsBeingDestroyed(&bDestroying);
    }

    if (!bDestroying) {
      StartLayout();
    }
  }

  if (mDocShell) {
    PRUint32 LoadType = 0;
    mDocShell->GetLoadType(&LoadType);

    if (ScrollToRef(!(LoadType & nsIDocShell::LOAD_CMD_HISTORY))) {
      mScrolledToRefAlready = PR_TRUE;
    }
  }

  nsScriptLoader *loader = mDocument->GetScriptLoader();
  if (loader) {
    loader->RemoveObserver(this);
  }

  // Make sure we no longer respond to document mutations.  We've flushed all
  // our notifications out, so there's no need to do anything else here.

  // XXXbz I wonder whether we could End() our contexts here too, or something,
  // just to make sure we no longer notify...
  mDocument->RemoveObserver(this);
  
  mDocument->EndLoad();

  DropParserAndPerfHint();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::SetParser(nsIParser* aParser)
{
  mParser = aParser;
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
HTMLContentSink::IsFormOnStack()
{
  return mFormOnStack;
}

NS_IMETHODIMP
HTMLContentSink::BeginContext(PRInt32 aPosition)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::BeginContext()\n"));
  MOZ_TIMER_START(mWatch);
  NS_PRECONDITION(aPosition > -1, "out of bounds");

  // Create new context
  SinkContext* sc = new SinkContext(this);
  if (!sc) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::BeginContext()\n"));
    MOZ_TIMER_STOP(mWatch);

    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!mCurrentContext) {
    NS_ERROR("Non-existing context");

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

  sc->Begin(nodeType,
            content,
            mCurrentContext->mStack[aPosition].mNumFlushed,
            insertionPoint);
  NS_ADDREF(sc->mSink);

  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = sc;

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::BeginContext()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::EndContext(PRInt32 aPosition)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::EndContext()\n"));
  MOZ_TIMER_START(mWatch);
  NS_PRECONDITION(mCurrentContext && aPosition > -1, "non-existing context");

  PRInt32 n = mContextStack.Count() - 1;
  SinkContext* sc = (SinkContext*) mContextStack.ElementAt(n);

  NS_ASSERTION(sc->mStack[aPosition].mType == mCurrentContext->mStack[0].mType,
               "ending a wrong context");

  mCurrentContext->FlushTextAndRelease();

  sc->mStack[aPosition].mNumFlushed = mCurrentContext->mStack[0].mNumFlushed;

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

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::EndContext()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

nsresult
HTMLContentSink::CloseHTML()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseHTML()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                 "HTMLContentSink::CloseHTML", 
                 eHTMLTag_html, 0, this);

  if (mHeadContext) {
    if (mCurrentContext == mHeadContext) {
      PRInt32 numContexts = mContextStack.Count();

      // Pop off the second html context if it's not done earlier
      mCurrentContext = (SinkContext*)mContextStack.ElementAt(--numContexts);
      mContextStack.RemoveElementAt(numContexts);
    }

    NS_ASSERTION(mHeadContext->mTextLength == 0, "Losing text");

    mHeadContext->End();

    delete mHeadContext;
    mHeadContext = nsnull;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseHTML()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

nsresult
HTMLContentSink::OpenHead()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenHead()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = OpenHeadContext();

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

nsresult
HTMLContentSink::OpenBody(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenBody()\n"));
  MOZ_TIMER_START(mWatch);

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenBody", 
                  eHTMLTag_body,
                  mCurrentContext->mStackPos, 
                  this);

  CloseHeadContext();  // do this just in case if the HEAD was left open!

  // Add attributes, if any, to the current BODY node
  if (mBody) {
    AddAttributes(aNode, mBody, PR_TRUE, PR_TRUE);

    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return NS_OK;
  }

  nsresult rv = mCurrentContext->OpenContainer(aNode);

  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return rv;
  }

  mBody = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;

  NS_ADDREF(mBody);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
  MOZ_TIMER_STOP(mWatch);

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

    if (insertionPoint != -1) {
      NotifyInsert(parent, mBody, insertionPoint - 1);
    } else {
      NotifyAppend(parent, numFlushed);
    }
    mCurrentContext->mStack[parentIndex].mNumFlushed = childCount;
  }

  StartLayout();

  return NS_OK;
}

nsresult
HTMLContentSink::CloseBody()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseBody()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseBody", 
                  eHTMLTag_body,
                  mCurrentContext->mStackPos - 1, 
                  this);

  PRBool didFlush;
  nsresult rv = mCurrentContext->FlushTextAndRelease(&didFlush);
  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return rv;
  }

  // Flush out anything that's left
  SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
             ("HTMLContentSink::CloseBody: layout final body content"));

  mCurrentContext->FlushTags();
  mCurrentContext->CloseContainer(eHTMLTag_body, PR_FALSE);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseBody()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

nsresult
HTMLContentSink::OpenForm(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenForm()\n"));
  MOZ_TIMER_START(mWatch);

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
    mFormOnStack = PR_TRUE;
    // Otherwise the form can be a content parent.
    result = mCurrentContext->OpenContainer(aNode);
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenForm()\n"));
  MOZ_TIMER_STOP(mWatch);

  return result;
}

nsresult
HTMLContentSink::CloseForm()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseForm()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult result = NS_OK;

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseForm",
                  eHTMLTag_form,
                  mCurrentContext->mStackPos - 1, 
                  this);

  if (mCurrentForm) {
    // if this is a well-formed form, close it too
    if (mCurrentContext->IsCurrentContainer(eHTMLTag_form)) {
      result = mCurrentContext->CloseContainer(eHTMLTag_form, PR_FALSE);
      mFormOnStack = PR_FALSE;
    }

    mCurrentForm = nsnull;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseForm()\n"));
  MOZ_TIMER_STOP(mWatch);

  return result;
}

nsresult
HTMLContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenFrameset()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenFrameset", 
                  eHTMLTag_frameset,
                  mCurrentContext->mStackPos, 
                  this);

  CloseHeadContext(); // do this just in case if the HEAD was left open!

  // Need to keep track of whether OpenContainer changes mFrameset
  nsGenericHTMLElement* oldFrameset = mFrameset;
  nsresult rv = mCurrentContext->OpenContainer(aNode);
  PRBool isFirstFrameset = NS_SUCCEEDED(rv) && mFrameset != oldFrameset;

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);

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

    if (insertionPoint != -1) {
      NotifyInsert(parent, mFrameset, insertionPoint - 1);
    } else {
      NotifyAppend(parent, numFlushed);
    }
    mCurrentContext->mStack[parentIndex].mNumFlushed = childCount;
  }
  
  return rv;
}

nsresult
HTMLContentSink::CloseFrameset()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseFrameset()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                   "HTMLContentSink::CloseFrameset", 
                   eHTMLTag_frameset,
                   mCurrentContext->mStackPos - 1,
                   this);

  SinkContext* sc = mCurrentContext;
  nsGenericHTMLElement* fs = sc->mStack[sc->mStackPos - 1].mContent;
  PRBool done = fs == mFrameset;

  nsresult rv;
  if (done) {
    PRBool didFlush;
    rv = sc->FlushTextAndRelease(&didFlush);
    if (NS_FAILED(rv)) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseFrameset()\n"));
      MOZ_TIMER_STOP(mWatch);

      return rv;
    }

    // Flush out anything that's left
    SINK_TRACE(gSinkLogModuleInfo, SINK_TRACE_REFLOW,
               ("HTMLContentSink::CloseFrameset: layout final content"));

    sc->FlushTags();
  }

  rv = sc->CloseContainer(eHTMLTag_frameset, PR_FALSE);    

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);

  if (done && mFramesEnabled) {
    StartLayout();
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::IsEnabled(PRInt32 aTag, PRBool* aReturn)
{
  nsHTMLTag theHTMLTag = nsHTMLTag(aTag);

  if (theHTMLTag == eHTMLTag_script) {
    *aReturn = mScriptEnabled;
  } else if (theHTMLTag == eHTMLTag_frameset) {
    *aReturn = mFramesEnabled;
  } else {
    *aReturn = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenContainer()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = NS_OK;

  switch (aNode.GetNodeType()) {
    case eHTMLTag_frameset:
      rv = OpenFrameset(aNode);
      break;
    case eHTMLTag_head:
      rv = OpenHeadContext();
      if (NS_SUCCEEDED(rv)) {
        rv = AddAttributes(aNode, mHead, PR_FALSE, mHaveSeenHead);
        mHaveSeenHead = PR_TRUE;
      }
      break;
    case eHTMLTag_body:
      rv = OpenBody(aNode);
      break;
    case eHTMLTag_html:
      if (mRoot) {
        // If we've already hit this code once, need to check for
        // already-present attributes on the root.
        AddAttributes(aNode, mRoot, PR_TRUE, mNotifiedRootInsertion);
        if (!mNotifiedRootInsertion) {
          NS_ASSERTION(!mLayoutStarted,
                       "How did we start layout without notifying on root?");
          // Now make sure to notify that we have now inserted our root.  If
          // there has been no initial reflow yet it'll be a no-op, but if
          // there has been one we need this to get its frames constructed.
          // Note that if mNotifiedRootInsertion is true we don't notify here,
          // since that just means there are multiple <html> tags in the
          // document; in those cases we just want to put all the attrs on one
          // tag.
          mNotifiedRootInsertion = PR_TRUE;
          PRInt32 index = mDocument->IndexOf(mRoot);
          NS_ASSERTION(index != -1, "mRoot not child of document?");
          NotifyInsert(nsnull, mRoot, index);

          // Now update the notification information in all our
          // contexts, since we just inserted the root and notified on
          // our whole tree
          UpdateChildCounts();
        }
      }
      break;
    case eHTMLTag_form:
      rv = OpenForm(aNode);
      break;
    default:
      rv = mCurrentContext->OpenContainer(aNode);
      break;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenContainer()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const eHTMLTags aTag)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseContainer()\n"));
  MOZ_TIMER_START(mWatch);

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
      rv = mCurrentContext->CloseContainer(aTag, PR_FALSE);
      break;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseContainer()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseMalformedContainer(const eHTMLTags aTag)
{
  return mCurrentContext->CloseContainer(aTag, PR_TRUE);
}

NS_IMETHODIMP
HTMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddLeaf()\n"));
  MOZ_TIMER_START(mWatch);

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

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddLeaf()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

nsresult 
HTMLContentSink::UpdateDocumentTitle()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::UpdateDocumentTitle()\n"));
  MOZ_TIMER_START(mWatch);
  NS_ASSERTION(mCurrentContext == mHeadContext, "title not in head");

  if (!mDocument->GetDocumentTitle().IsVoid()) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::UpdateDocumentTitle()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_OK;
  }

  // Use mTitleString.
  mTitleString.CompressWhitespace(PR_TRUE, PR_TRUE);

  nsCOMPtr<nsIDOMNSDocument> domDoc(do_QueryInterface(mDocument));
  domDoc->SetTitle(mTitleString);

  mTitleString.Truncate();

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::UpdateDocumentTitle()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
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
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddComment()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = mCurrentContext->AddComment(aNode);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddComment()\n"));
  MOZ_TIMER_STOP(mWatch);

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

  MOZ_TIMER_START(mWatch);
  // Implementation of AddProcessingInstruction() should start here

  MOZ_TIMER_STOP(mWatch);

  return result;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
HTMLContentSink::AddDocTypeDecl(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddDocTypeDecl()\n"));
  MOZ_TIMER_START(mWatch);

  nsAutoString docTypeStr(aNode.GetText());
  nsresult rv = NS_OK;

  PRInt32 publicStart = docTypeStr.Find("PUBLIC", PR_TRUE);
  PRInt32 systemStart = docTypeStr.Find("SYSTEM", PR_TRUE);
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

      PRBool hasQuote = PR_FALSE;
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
          hasQuote = PR_TRUE;
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
          systemStart = docTypeStr.Find("SYSTEM", PR_TRUE,
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

    rv = NS_NewDOMDocumentType(getter_AddRefs(docType),
                               mDocument->NodeInfoManager(), nsnull,
                               nameAtom, nsnull, nsnull, publicId, systemId,
                               EmptyString());
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
      
      mDocument->InsertChildAt(content, 0, PR_TRUE);
    }
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddDocTypeDecl()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::WillTokenize(void)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillProcessTokens(void)
{
  return WillProcessTokensImpl();
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
HTMLContentSink::StartLayout()
{
  if (mLayoutStarted) {
    return;
  }

  mHTMLDocument->SetIsFrameset(mFrameset != nsnull);

  nsContentSink::StartLayout(mFrameset != nsnull);
}

void
HTMLContentSink::TryToScrollToRef()
{
  if (mRef.IsEmpty()) {
    return;
  }

  if (mScrolledToRefAlready) {
    return;
  }

  if (ScrollToRef(PR_TRUE)) {
    mScrolledToRefAlready = PR_TRUE;
  }
}

void
HTMLContentSink::AddBaseTagInfo(nsIContent* aContent)
{
  nsresult rv;
  if (mBaseHref) {
    rv = aContent->SetProperty(nsGkAtoms::htmlBaseHref, mBaseHref,
                               nsPropertyTable::SupportsDtorFunc, PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      // circumvent nsDerivedSafe
      NS_ADDREF(NS_STATIC_CAST(nsIURI*, mBaseHref));
    }
  }
  if (mBaseTarget) {
    rv = aContent->SetProperty(nsGkAtoms::htmlBaseTarget, mBaseTarget,
                               nsPropertyTable::SupportsDtorFunc, PR_TRUE);
    if (NS_SUCCEEDED(rv)) {
      // circumvent nsDerivedSafe
      NS_ADDREF(NS_STATIC_CAST(nsIAtom*, mBaseTarget));
    }
  }
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
  }

  NS_ASSERTION(mContextStack.Count() > 0, "Stack should not be empty");
  
  PRInt32 n = mContextStack.Count() - 1;
  mCurrentContext = (SinkContext*) mContextStack.ElementAt(n);
  mContextStack.RemoveElementAt(n);
}

void
HTMLContentSink::ProcessBASEElement(nsGenericHTMLElement* aElement)
{
  // href attribute
  nsAutoString attrValue;
  if (aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::href, attrValue)) {
    //-- Make sure this page is allowed to load this URI
    nsresult rv;
    nsCOMPtr<nsIURI> baseHrefURI;
    rv = nsContentUtils::NewURIWithDocumentCharset(getter_AddRefs(baseHrefURI),
                                                   attrValue, mDocument,
                                                   nsnull);
    if (NS_FAILED(rv))
      return;

    // Setting "BASE URI" from the last BASE tag appearing in HEAD.
    if (!mBody) {
      // The document checks if it is legal to set this base. Failing here is
      // ok, we just won't set a new base.
      rv = mDocument->SetBaseURI(baseHrefURI);
      if (NS_SUCCEEDED(rv)) {
        mDocumentBaseURI = mDocument->GetBaseURI();
      }
    } else {
      // NAV compatibility quirk

      nsIScriptSecurityManager *securityManager =
        nsContentUtils::GetSecurityManager();

      rv = securityManager->
        CheckLoadURIWithPrincipal(mDocument->NodePrincipal(), baseHrefURI,
                                  nsIScriptSecurityManager::STANDARD);
      if (NS_SUCCEEDED(rv)) {
        mBaseHref = baseHrefURI;
      }
    }
  }

  // target attribute
  if (aElement->GetAttr(kNameSpaceID_None, nsGkAtoms::target, attrValue)) {
    if (!mBody) {
      // still in real HEAD
      mDocument->SetBaseTarget(attrValue);
    } else {
      // NAV compatibility quirk
      mBaseTarget = do_GetAtom(attrValue);
    }
  }
}

nsresult
HTMLContentSink::ProcessLINKTag(const nsIParserNode& aNode)
{
  nsresult  result = NS_OK;
  nsGenericHTMLElement* parent = nsnull;

  if (mCurrentContext) {
    parent = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  }

  if (parent) {
    // Create content object
    nsCOMPtr<nsIContent> element;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    mNodeInfoManager->GetNodeInfo(nsGkAtoms::link, nsnull, kNameSpaceID_None,
                                  getter_AddRefs(nodeInfo));

    result = NS_NewHTMLElement(getter_AddRefs(element), nodeInfo);
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsIStyleSheetLinkingElement> ssle(do_QueryInterface(element));

    if (ssle) {
      // XXX need prefs. check here.
      if (!mInsideNoXXXTag) {
        ssle->InitStyleLinkElement(mParser, PR_FALSE);
        ssle->SetEnableUpdates(PR_FALSE);
      } else {
        ssle->InitStyleLinkElement(nsnull, PR_TRUE);
      }
    }

    // Add in the attributes and add the style content object to the
    // head container.
    AddBaseTagInfo(element);
    result = AddAttributes(aNode, element);
    if (NS_FAILED(result)) {
      return result;
    }
    parent->AppendChildTo(element, PR_FALSE);

    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      result = ssle->UpdateStyleSheet(nsnull, nsnull);

      // look for <link rel="next" href="url">
      nsAutoString relVal;
      element->GetAttr(kNameSpaceID_None, nsGkAtoms::rel, relVal);
      if (!relVal.IsEmpty()) {
        // XXX seems overkill to generate this string array
        nsStringArray linkTypes;
        nsStyleLinkElement::ParseLinkTypes(relVal, linkTypes);
        PRBool hasPrefetch = (linkTypes.IndexOf(NS_LITERAL_STRING("prefetch")) != -1);
        if (hasPrefetch || linkTypes.IndexOf(NS_LITERAL_STRING("next")) != -1) {
          nsAutoString hrefVal;
          element->GetAttr(kNameSpaceID_None, nsGkAtoms::href, hrefVal);
          if (!hrefVal.IsEmpty()) {
            PrefetchHref(hrefVal, hasPrefetch);
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

  MOZ_TIMER_DEBUGLOG(("Save and stop: nsHTMLContentSink::NotifyInsert()\n"));
  MOZ_TIMER_SAVE(mWatch)
  MOZ_TIMER_STOP(mWatch);

  nsNodeUtils::ContentInserted(NODE_FROM(aContent, mDocument),
                               aChildContent, aIndexInContainer);
  mLastNotificationTime = PR_Now();

  MOZ_TIMER_DEBUGLOG(("Restore: nsHTMLContentSink::NotifyInsert()\n"));
  MOZ_TIMER_RESTORE(mWatch);

  mInNotification--;
}

PRBool
HTMLContentSink::IsMonolithicContainer(nsHTMLTag aTag)
{
  if (aTag == eHTMLTag_tr     ||
      aTag == eHTMLTag_select ||
      aTag == eHTMLTag_applet ||
      aTag == eHTMLTag_object) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

void
HTMLContentSink::UpdateChildCounts()
{
  PRInt32 numContexts = mContextStack.Count();
  for (PRInt32 i = 0; i < numContexts; i++) {
    SinkContext* sc = (SinkContext*)mContextStack.ElementAt(i);

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
                                     PRBool aMalformed)
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
  // or return NS_ERROR_HTMLPARSER_BLOCK. Or neither if the script doesn't
  // need executing.
  nsresult rv = content->DoneAddingChildren(PR_TRUE);

  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  if (rv == NS_ERROR_HTMLPARSER_BLOCK) {
    // If this append fails we'll never unblock the parser, but the UI will
    // still remain responsive. There are other ways to deal with this, but
    // the end result is always that the page gets botched, so there is no
    // real point in making it more complicated.
    mScriptElements.AppendObject(sele);
  }
  else {
    // This may have already happened if the script executed, but in case
    // it didn't then remove the element so that it doesn't get stuck forever.
    mHTMLDocument->ScriptExecuted(sele);
  }

  // If the parser got blocked, make sure to return the appropriate rv.
  // I'm not sure if this is actually needed or not.
  if (mParser && !mParser->IsParserEnabled()) {
    rv = NS_ERROR_HTMLPARSER_BLOCK;
  }

  return rv;
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
    // with mDontLoadStyle = PR_TRUE, so these two calls will have no effect.
    ssle->SetEnableUpdates(PR_TRUE);
    rv = ssle->UpdateStyleSheet(nsnull, nsnull);
  }

  return rv;
}

void
HTMLContentSink::FlushPendingNotifications(mozFlushType aType)
{
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant)
  if (mCurrentContext && !mInNotification) {
    if (aType & Flush_SinkNotifications) {
      mCurrentContext->FlushTags();
    }
    else {
      mCurrentContext->FlushText();
    }
    if (aType & Flush_OnlyReflow) {
      // Make sure that layout has started so that the reflow flush
      // will actually happen.
      StartLayout();
    }
  }
}

nsresult
HTMLContentSink::FlushTags()
{
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
      nsIContent* root = mDocument->GetRootContent();
      if (root) {
        if (mDocumentURI) {
          nsCAutoString buf;
          mDocumentURI->GetSpec(buf);
          fputs(buf.get(), out);
        }

        fputs(";", out);
        root->DumpContent(out, 0, PR_FALSE);
        fputs(";\n", out);
      }
    }

    fclose(out);
  }

  return NS_OK;
}
#endif


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
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsXPIDLString.h"
#include "nsIHTMLContentSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIParser.h"
#include "nsParserUtils.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsIScriptLoader.h"
#include "nsIScriptLoaderObserver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsIUnicharInputStream.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsINodeInfo.h"
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "nsSupportsArray.h"
#include "jsapi.h" // for JSVERSION_* and JS_VersionToString
#include "prtime.h"
#include "prlog.h"

#include "nsGenericHTMLElement.h"
#include "nsIElementFactory.h"
#include "nsITextContent.h"

#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMHTMLScriptElement.h"
#include "nsIScriptElement.h"

#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIFormControl.h"
#include "nsIForm.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsIScrollableView.h"
#include "nsHTMLAtoms.h"
#include "nsContentUtils.h"
#include "nsIFrame.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"
#include "nsIUnicodeDecoder.h"
#include "nsICharsetAlias.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsCPrefetchService.h"

#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIWebNavigation.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIHTMLDocument.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIRefreshURI.h"
#include "nsICookieService.h"
#include "nsVoidArray.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsICodebasePrincipal.h"
#include "nsIAggregatePrincipal.h"
#include "nsTextFragment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"

#include "nsIParserService.h"
#include "nsISelectElement.h"
#include "nsITextAreaElement.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

// XXX Go through a factory for this one
#include "nsICSSParser.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsIDOMHTMLTitleElement.h"
#include "nsTimer.h"
#include "nsITimer.h"
#include "nsDOMError.h"
#include "nsIScrollable.h"
#include "nsContentPolicyUtils.h"
#include "nsIScriptContext.h"
#include "nsStyleLinkElement.h"

#include "nsReadableUtils.h"
#include "nsWeakReference.h" // nsHTMLElementFactory supports weak references
#include "nsIPrompt.h"
#include "nsIDOMWindowInternal.h"

#include "nsLayoutCID.h"
#include "nsIFrameManager.h"
#include "nsILayoutHistoryState.h"
#include "nsIDocShellTreeItem.h"
#include "plevent.h"


#include "nsEscape.h"

#include "nsIElementObserver.h"

static NS_DEFINE_CID(kLayoutHistoryStateCID, NS_LAYOUT_HISTORY_STATE_CID);

#ifdef ALLOW_ASYNCH_STYLE_SHEETS
const PRBool kBlockByDefault = PR_FALSE;
#else
const PRBool kBlockByDefault = PR_TRUE;
#endif


//----------------------------------------------------------------------

#ifdef NS_DEBUG
static PRLogModuleInfo* gSinkLogModuleInfo;

#define SINK_TRACE_CALLS              0x1
#define SINK_TRACE_REFLOW             0x2
#define SINK_ALWAYS_REFLOW            0x4

#define SINK_LOG_TEST(_lm, _bit) (PRIntn((_lm)->level) & (_bit))

#define SINK_TRACE(_bit, _args)                       \
  PR_BEGIN_MACRO                                      \
    if (SINK_LOG_TEST(gSinkLogModuleInfo, _bit)) {    \
      PR_LogPrint _args;                              \
    }                                                 \
  PR_END_MACRO

#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj) \
  _obj->SinkTraceNode(_bit, _msg, _tag, _sp, this)

#else
#define SINK_TRACE(_bit, _args)
#define SINK_TRACE_NODE(_bit, _msg, _tag, _sp, _obj)
#endif

#undef SINK_NO_INCREMENTAL

//----------------------------------------------------------------------

#define NS_SINK_FLAG_SCRIPT_ENABLED       0x00000008

#define NS_SINK_FLAG_FRAMES_ENABLED       0x00000010

// Interrupt parsing when mMaxTokenProcessingTime is exceeded
#define NS_SINK_FLAG_CAN_INTERRUPT_PARSER 0x00000020

// Lower the value for mNotificationInterval and
// mMaxTokenProcessingTime
#define NS_SINK_FLAG_DYNAMIC_LOWER_VALUE  0x00000040

#define NS_SINK_FLAG_FORM_ON_STACK        0x00000080


// 1/2 second fudge factor for window creation
#define NS_DELAY_FOR_WINDOW_CREATION  500000

// 200 determined empirically to provide good user response without
// sampling the clock too often.
#define NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE 200

class SinkContext;

class HTMLContentSink : public nsIHTMLContentSink,
                        public nsIScriptLoaderObserver,
                        public nsITimerCallback,
                        public nsICSSLoaderObserver,
#ifdef DEBUG
                        public nsIDebugDumpContent,
#endif
                        public nsIDocumentObserver
{
public:
  HTMLContentSink();
  virtual ~HTMLContentSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsresult Init(nsIDocument* aDoc, nsIURI* aURL, nsIWebShell* aContainer,
                nsIChannel* aChannel);

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADEROBSERVER

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD FlushPendingNotifications();
  NS_IMETHOD SetDocumentCharset(nsAString& aCharset);

  // nsIHTMLContentSink
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsHTMLTag aTag);
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
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML();
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead();
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody();
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm();
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset();
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap();
  NS_IMETHOD GetPref(PRInt32 aTag, PRBool& aPref);
  NS_IMETHOD_(PRBool) IsFormOnStack();

  NS_IMETHOD DoFragment(PRBool aFlag);

  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK
  
  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify)
  {
    return NS_OK;
  }

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER

#ifdef DEBUG
  // nsIDebugDumpContent
  NS_IMETHOD DumpContentModel();
#endif

  PRBool IsTimeToNotify();
  PRBool IsInScript();

  nsresult AddAttributes(const nsIParserNode& aNode, nsIHTMLContent* aContent,
                         PRBool aNotify = PR_FALSE);
  nsresult CreateContentObject(const nsIParserNode& aNode, nsHTMLTag aNodeType,
                               nsIDOMHTMLFormElement* aForm,
                               nsIWebShell* aWebShell,
                               nsIHTMLContent** aResult);

  inline PRInt32 GetNotificationInterval()
  {
    if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
      return 1000;
    }

    return mNotificationInterval;
  }

  inline PRInt32 GetMaxTokenProcessingTime()
  {
    if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
      return 3000;
    }

    return mMaxTokenProcessingTime;
  }

#ifdef NS_DEBUG
  void SinkTraceNode(PRUint32 aBit,
                     const char* aMsg,
                     const nsHTMLTag aTag,
                     PRInt32 aStackPos,
                     void* aThis);
#endif

  nsIDocument* mDocument;
  nsIHTMLDocument* mHTMLDocument;
  nsCOMPtr<nsINodeInfoManager> mNodeInfoManager;
  nsIURI* mDocumentURI;
  nsIURI* mDocumentBaseURL;
  nsIWebShell* mWebShell;
  nsIParser* mParser;

  // back off timer notification after count
  PRInt32 mBackoffCount;

  // Notification interval in microseconds
  PRInt32 mNotificationInterval;

  // Time of last notification
  PRTime mLastNotificationTime;

  // Timer used for notification
  nsCOMPtr<nsITimer> mNotificationTimer;

  // The maximum length of a text run
  PRInt32 mMaxTextRun;

  nsIHTMLContent* mRoot;
  nsIHTMLContent* mBody;
  nsIHTMLContent* mFrameset;
  nsIHTMLContent* mHead;
  nsString mTitle;
  nsString mUnicodeXferBuf;

  nsString mSkippedContent;

  // Do we notify based on time?
  PRPackedBool mNotifyOnTimer;

  PRPackedBool mLayoutStarted;
  PRPackedBool mScrolledToRefAlready;
  PRPackedBool mNeedToBlockParser;

  PRInt32 mInScript;
  PRInt32 mInNotification;
  nsCOMPtr<nsIDOMHTMLFormElement> mCurrentForm;
  nsCOMPtr<nsIHTMLContent> mCurrentMap;

  nsAutoVoidArray mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;
  PRInt32 mNumOpenIFRAMES;
  nsCOMPtr<nsISupportsArray> mScriptElements;
  nsCOMPtr<nsIRequest> mDummyParserRequest;

  nsCString mRef;

  nsString mBaseHREF;
  nsString mBaseTarget;

  nsICSSLoader *mCSSLoader;
  PRInt32 mInsideNoXXXTag;
  PRInt32 mInMonolithicContainer;
  PRUint32 mFlags;

  // -- Can interrupt parsing members --
  PRUint32 mDelayTimerStart;

  // Interrupt parsing during token procesing after # of microseconds
  PRInt32 mMaxTokenProcessingTime;

  // Switch between intervals when time is exceeded
  PRInt32 mDynamicIntervalSwitchThreshold;

  PRInt32 mBeginLoadTime;

  // Last mouse event or keyboard event time sampled by the content
  // sink
  PRUint32 mLastSampledUserEventTime;

  // The number of tokens that have been processed while in the low
  // frequency parser interrupt mode without falling through to the
  // logic which decides whether to switch to the high frequency
  // parser interrupt mode.
  PRUint8 mDeflectedCount;

  nsCOMPtr<nsIObserverEntry> mObservers;

  void StartLayout();

  void ScrollToRef();
  void TryToScrollToRef();

  void AddBaseTagInfo(nsIHTMLContent* aContent);

  nsresult ProcessLinkHeader(nsIHTMLContent* aElement,
                             const nsAString& aLinkData);
  nsresult ProcessLink(nsIHTMLContent* aElement, const nsString& aHref,
                       const nsString& aRel, const nsString& aTitle,
                       const nsString& aType, const nsString& aMedia);
  nsresult ProcessStyleLink(nsIHTMLContent* aElement,
                            const nsString& aHref,
                            const nsStringArray& aLinkTypes,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);
  void PrefetchHref(const nsAString &aHref);

  void ProcessBaseHref(const nsAString& aBaseHref);
  void ProcessBaseTarget(const nsAString& aBaseTarget);

  nsresult RefreshIfEnabled(nsIViewManager* vm);

  // Routines for tags that require special handling
  nsresult ProcessATag(const nsIParserNode& aNode, nsIHTMLContent* aContent);
  nsresult ProcessAREATag(const nsIParserNode& aNode);
  nsresult ProcessBASETag(const nsIParserNode& aNode);
  nsresult ProcessLINKTag(const nsIParserNode& aNode);
  nsresult ProcessMAPTag(const nsIParserNode& aNode, nsIHTMLContent* aContent);
  nsresult ProcessMETATag(const nsIParserNode& aNode);
  nsresult ProcessSCRIPTTag(const nsIParserNode& aNode);
  nsresult ProcessSTYLETag(const nsIParserNode& aNode);

  nsresult ProcessHeaderData(nsIAtom* aHeader, const nsAString& aValue,
                             nsIHTMLContent* aContent = nsnull);
  nsresult ProcessHTTPHeaders(nsIChannel* aChannel);

  // Script processing related routines
  nsresult ResumeParsing();
  PRBool PreEvaluateScript();
  void PostEvaluateScript();

  void UpdateAllContexts();
  void NotifyAppend(nsIContent* aContent,
                    PRInt32 aStartIndex);
  void NotifyInsert(nsIContent* aContent,
                    nsIContent* aChildContent,
                    PRInt32 aIndexInContainer);
  PRBool IsMonolithicContainer(nsHTMLTag aTag);

  // CanInterrupt parsing related routines
  nsresult AddDummyParserRequest(void);
  nsresult RemoveDummyParserRequest(void);

#ifdef NS_DEBUG
  void ForceReflow();
#endif

  // Measures content model creation time for current document
  MOZ_TIMER_DECLARE(mWatch)
};


//----------------------------------------------------------------------
//
// DummyParserRequest
//
//   This is a dummy request implementation that we add to the document's load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't fire
//   before we've finished all of parsing and tokenizing of the document.
//

class DummyParserRequest : public nsIChannel
{
protected:
  DummyParserRequest(nsIHTMLContentSink* aSink);
  virtual ~DummyParserRequest();

  static PRInt32 gRefCnt;
  static nsIURI* gURI;

  nsCOMPtr<nsILoadGroup> mLoadGroup;

  nsIHTMLContentSink* mSink; // Weak reference

public:
  static nsresult
  Create(nsIRequest** aResult, nsIHTMLContentSink* aSink);

  NS_DECL_ISUPPORTS

	// nsIRequest
  NS_IMETHOD GetName(nsACString &result)
  {
    result = NS_LITERAL_CSTRING("about:layout-dummy-request");
    return NS_OK;
  }

  NS_IMETHOD IsPending(PRBool *_retval)
  {
    *_retval = PR_TRUE;
    return NS_OK;
  }

  NS_IMETHOD GetStatus(nsresult *status)
  {
    *status = NS_OK;
    return NS_OK;
  }

  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend(void)
  {
    return NS_OK;
  }

  NS_IMETHOD Resume(void)
  {
    return NS_OK;
  }

  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup)
  {
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);

    return NS_OK;
  }

  NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup)
  {
    mLoadGroup = aLoadGroup;

    return NS_OK;
  }

  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags)
  {
    *aLoadFlags = nsIRequest::LOAD_NORMAL;

    return NS_OK;
  }

  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags)
  {
    return NS_OK;
  }

 	// nsIChannel
  NS_IMETHOD GetOriginalURI(nsIURI **aOriginalURI)
  {
    *aOriginalURI = gURI;
    NS_ADDREF(*aOriginalURI);

    return NS_OK;
  }

  NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI)
  {
    gURI = aOriginalURI;
    NS_ADDREF(gURI);

    return NS_OK;
  }

  NS_IMETHOD GetURI(nsIURI **aURI)
  {
    *aURI = gURI;
    NS_ADDREF(*aURI);

    return NS_OK;
  }

  NS_IMETHOD SetURI(nsIURI* aURI)
  {
    gURI = aURI;
    NS_ADDREF(gURI);

    return NS_OK;
  }

  NS_IMETHOD Open(nsIInputStream **_retval)
  {
    *_retval = nsnull;

    return NS_OK;
  }

  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
  {
    return NS_OK;
  }

  NS_IMETHOD GetOwner(nsISupports **aOwner)
  {
    *aOwner = nsnull;

    return NS_OK;
  }

  NS_IMETHOD SetOwner(nsISupports *aOwner)
  {
    return NS_OK;
  }

  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aNotifCallbacks)
  {
    *aNotifCallbacks = nsnull;

    return NS_OK;
  }

  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor *aNotifCallbacks)
  {
    return NS_OK;
  }

  NS_IMETHOD GetSecurityInfo(nsISupports **aSecurityInfo)
  {
    *aSecurityInfo = nsnull;

    return NS_OK;
  }

  NS_IMETHOD GetContentType(nsACString &aContentType)
  {
    aContentType.Truncate();

    return NS_OK;
  }

  NS_IMETHOD SetContentType(const nsACString &aContentType)
  {
    return NS_OK;
  }

  NS_IMETHOD GetContentCharset(nsACString &aContentCharset)
  {
    aContentCharset.Truncate();

    return NS_OK;
  }

  NS_IMETHOD SetContentCharset(const nsACString &aContentCharset)
  {
    return NS_OK;
  }

  NS_IMETHOD GetContentLength(PRInt32 *aContentLength)
  {
    return NS_OK;
  }

  NS_IMETHOD SetContentLength(PRInt32 aContentLength)
  {
    return NS_OK;
  }
};

PRInt32 DummyParserRequest::gRefCnt;
nsIURI* DummyParserRequest::gURI;

NS_IMPL_ADDREF(DummyParserRequest);
NS_IMPL_RELEASE(DummyParserRequest);
NS_IMPL_QUERY_INTERFACE2(DummyParserRequest, nsIRequest, nsIChannel);

nsresult
DummyParserRequest::Create(nsIRequest** aResult, nsIHTMLContentSink* aSink)
{
  *aResult = new DummyParserRequest(aSink);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}


DummyParserRequest::DummyParserRequest(nsIHTMLContentSink* aSink)
{
  NS_INIT_ISUPPORTS();

  if (gRefCnt++ == 0) {
#ifdef DEBUG
    nsresult rv =
#endif
    NS_NewURI(&gURI, NS_LITERAL_CSTRING("about:parser-dummy-request"));

    NS_ASSERTION(NS_SUCCEEDED(rv),
                 "unable to create about:parser-dummy-request");
  }

  mSink = aSink;
}


DummyParserRequest::~DummyParserRequest()
{
  if (--gRefCnt == 0) {
    NS_IF_RELEASE(gURI);
  }
}

NS_IMETHODIMP
DummyParserRequest::Cancel(nsresult status)
{
  // Cancel parser
  nsresult rv = NS_OK;
  HTMLContentSink* sink = NS_STATIC_CAST(HTMLContentSink*, mSink);
  if ((sink) && (sink->mParser)) {
    sink->mParser->CancelParsingEvents();
  }
  return rv;
}


class SinkContext
{
public:
  SinkContext(HTMLContentSink* aSink);
  ~SinkContext();

  // Normally when OpenContainer's are done the container is not
  // appended to it's parent until the container is closed. By setting
  // pre-append to true, the container will be appended when it is
  // created.
  void SetPreAppend(PRBool aPreAppend)
  {
    mPreAppend = aPreAppend;
  }

  nsresult Begin(nsHTMLTag aNodeType, nsIHTMLContent* aRoot,
                 PRInt32 aNumFlushed, PRInt32 aInsertionPoint);
  nsresult OpenContainer(const nsIParserNode& aNode);
  nsresult CloseContainer(const nsHTMLTag aTag);
  nsresult AddLeaf(const nsIParserNode& aNode);
  nsresult AddLeaf(nsIHTMLContent* aContent);
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

  nsresult FlushTags(PRBool aNotify = PR_TRUE);

  PRBool   IsCurrentContainer(nsHTMLTag mType);
  PRBool   IsAncestorContainer(nsHTMLTag mType);
  nsIHTMLContent* GetCurrentContainer();

  void DidAddContent(nsIContent* aContent, PRBool aDidNotify = PR_FALSE);
  void UpdateChildCounts();

  HTMLContentSink* mSink;
  PRBool mPreAppend;
  PRInt32 mNotifyLevel;
  nsIContent* mLastTextNode;
  PRInt32 mLastTextNodeSize;

  struct Node {
    nsHTMLTag mType;
    nsIHTMLContent* mContent;
    PRUint32 mFlags;
    PRInt32 mNumFlushed;
    PRInt32 mInsertionPoint;
  };

// Node.mFlags
#define APPENDED 0x1

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
    nsCOMPtr<nsIDTD> dtd;
    if (mParser) {
      mParser->GetDTD(getter_AddRefs(dtd));
      if (!dtd) 
        return;
    }
    const char* cp = 
      NS_ConvertUCS2toUTF8(dtd->IntTagToStringTag(aTag)).get();
    PR_LogPrint("%s: this=%p node='%s' stackPos=%d", 
                aMsg, aThis, cp, aStackPos);
  }
}
#endif

nsresult
HTMLContentSink::AddAttributes(const nsIParserNode& aNode,
                               nsIHTMLContent* aContent, PRBool aNotify)
{
  // Add tag attributes to the content attributes

  PRInt32 ac = aNode.GetAttributeCount();

  if (ac == 0) {
    // No attributes, nothing to do. Do an early return to avoid
    // constructing the nsAutoString object for nothing.

    return NS_OK;
  }

  nsAutoString k;
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAString& key = aNode.GetKeyAt(i);
    k.Assign(key);
    ToLowerCase(k);

    nsCOMPtr<nsIAtom>  keyAtom(dont_AddRef(NS_NewAtom(k)));

    if (!aContent->HasAttr(kNameSpaceID_None, keyAtom)) {
      // Get value and remove mandatory quotes
      static const char* kWhitespace = "\n\r\t\b";
      const nsAString& v =
        nsContentUtils::TrimCharsInSet(kWhitespace, aNode.GetValueAt(i));

      if (nodeType == eHTMLTag_a && keyAtom == nsHTMLAtoms::name) {
        NS_ConvertUCS2toUTF8 cname(v);
        NS_ConvertUTF8toUCS2 uv(nsUnescape(NS_CONST_CAST(char *,
                                                         cname.get())));

        // Add attribute to content
        aContent->SetAttr(kNameSpaceID_None, keyAtom, uv, aNotify);
      } else {
        // Add attribute to content
        aContent->SetAttr(kNameSpaceID_None, keyAtom, v, aNotify);
      }
    }
  }

  return NS_OK;
}

static void
SetForm(nsIHTMLContent* aContent, nsIDOMHTMLFormElement* aForm)
{
  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(aContent));

  if (formControl) {
    formControl->SetForm(aForm);
  }
}

static nsresult
MakeContentObject(nsHTMLTag aNodeType, nsINodeInfo *aNodeInfo,
                  nsIDOMHTMLFormElement* aForm, nsIWebShell* aWebShell,
                  nsIHTMLContent** aResult, PRBool aInsideNoXXXTag,
                  PRBool aFromParser);

/**
 * Factory subroutine to create all of the html content objects.
 */
nsresult
HTMLContentSink::CreateContentObject(const nsIParserNode& aNode,
                                     nsHTMLTag aNodeType,
                                     nsIDOMHTMLFormElement* aForm,
                                     nsIWebShell* aWebShell,
                                     nsIHTMLContent** aResult)
{
  nsresult rv = NS_OK;

  // Find/create atom for the tag name

  nsCOMPtr<nsINodeInfo> nodeInfo;

  if (aNodeType == eHTMLTag_userdefined) {
    nsAutoString tmp;
    tmp.Append(aNode.GetText());
    ToLowerCase(tmp);

    rv = mNodeInfoManager->GetNodeInfo(tmp, nsnull, kNameSpaceID_None,
                                       *getter_AddRefs(nodeInfo));
  } else {
    nsCOMPtr<nsIDTD> dtd;
    rv = mParser->GetDTD(getter_AddRefs(dtd));
    if (NS_SUCCEEDED(rv)) {
      nsDependentString tag(dtd->IntTagToStringTag(aNodeType));

      rv = mNodeInfoManager->GetNodeInfo(tag, nsnull, kNameSpaceID_None,
                                         *getter_AddRefs(nodeInfo));
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // XXX if the parser treated the text in a textarea like a normal
  // textnode we wouldn't need to do this.

  if (aNodeType == eHTMLTag_textarea) {
    nsCOMPtr<nsIDTD> dtd;
    mParser->GetDTD(getter_AddRefs(dtd));
    NS_ENSURE_TRUE(dtd, NS_ERROR_FAILURE);

    PRInt32 lineNo = 0;

    dtd->CollectSkippedContent(eHTMLTag_textarea, mSkippedContent, lineNo);
  }

  // Make the content object
  rv = MakeContentObject(aNodeType, nodeInfo, aForm, aWebShell, aResult,
                         !!mInsideNoXXXTag, PR_TRUE);

  if (aNodeType == eHTMLTag_textarea && !mSkippedContent.IsEmpty()) {
    // XXX: if the parser treated the text in a textarea like a normal
    // textnode we wouldn't need to do this.

    // If the text area has some content, set it

    // Strip only one leading newline if there is one (bug 40394)
    nsString::const_iterator start, end;
    mSkippedContent.BeginReading(start);
    mSkippedContent.EndReading(end);
    if (*start == nsCRT::CR) {
      ++start;

      if (start != end && *start == nsCRT::LF) {
        ++start;
      }
    } else if (*start == nsCRT::LF) {
      ++start;
    }

    nsCOMPtr<nsIDOMHTMLTextAreaElement> ta(do_QueryInterface(*aResult));
    NS_ASSERTION(ta, "Huh? text area doesn't implement "
                 "nsIDOMHTMLTextAreaElement?");

    ta->SetDefaultValue(Substring(start, end));

    // Release whatever's in the skipped content string, no point in
    // holding on to this any longer.
    mSkippedContent.Truncate();
  }

  PRInt32 id;
  mDocument->GetAndIncrementContentID(&id);
  (*aResult)->SetContentID(id);

  return rv;
}

nsresult
NS_CreateHTMLElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo,
                     PRBool aCaseSensitive)
{
  nsresult rv = NS_OK;

  nsIParserService* parserService = nsContentUtils::GetParserServiceWeakRef();
  if (!parserService)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIAtom> name;
  rv = aNodeInfo->GetNameAtom(*getter_AddRefs(name));
  NS_ENSURE_SUCCESS(rv, rv);

  // Find tag in tag table
  PRInt32 id;

  if (aCaseSensitive) {
    parserService->HTMLCaseSensitiveAtomTagToId(name, &id);
  } else {
    parserService->HTMLAtomTagToId(name, &id);
  }

  if (aCaseSensitive) {
    rv = MakeContentObject(nsHTMLTag(id), aNodeInfo, nsnull, nsnull,
                           aResult, PR_FALSE, PR_FALSE);
  } else {
    // Revese map id to name to get the correct character case in
    // the tag name.

    nsCOMPtr<nsINodeInfo> kungFuDeathGrip;
    nsINodeInfo *nodeInfo = aNodeInfo;

    if (nsHTMLTag(id) != eHTMLTag_userdefined) {
      const PRUnichar *tag = nsnull;
      parserService->HTMLIdToStringTag(id, &tag);
      NS_ASSERTION(tag, "What? Reverse mapping of id to string broken!!!");

      const PRUnichar *name_str = nsnull;
      name->GetUnicode(&name_str);
      NS_ASSERTION(name_str, "What? No string in atom?!?");

      if (nsCRT::strcmp(tag, name_str) != 0) {
        nsCOMPtr<nsIAtom> atom(dont_AddRef(NS_NewAtom(tag)));

        rv = aNodeInfo->NameChanged(atom, *getter_AddRefs(kungFuDeathGrip));
        NS_ENSURE_SUCCESS(rv, rv);

        nodeInfo = kungFuDeathGrip;
      }
    }

    rv = MakeContentObject(nsHTMLTag(id), nodeInfo, nsnull, nsnull, aResult,
                           PR_FALSE, PR_FALSE);
  }

  return rv;
}

//----------------------------------------------------------------------


class nsHTMLElementFactory : public nsIElementFactory,
                             public nsSupportsWeakReference
{
public:
  nsHTMLElementFactory();
  virtual ~nsHTMLElementFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                 nsIContent** aResult);
};

nsresult
NS_NewHTMLElementFactory(nsIElementFactory** aInstancePtrResult)
{
  *aInstancePtrResult = new nsHTMLElementFactory();
  if (!*aInstancePtrResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsHTMLElementFactory::nsHTMLElementFactory()
{
  NS_INIT_ISUPPORTS();
}

nsHTMLElementFactory::~nsHTMLElementFactory()
{
}

NS_IMPL_ISUPPORTS2(nsHTMLElementFactory, nsIElementFactory,
                   nsISupportsWeakReference);

NS_IMETHODIMP
nsHTMLElementFactory::CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                          nsIContent** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsresult rv;
  nsCOMPtr<nsIHTMLContent> htmlContent;
  rv = NS_CreateHTMLElement(getter_AddRefs(htmlContent), aNodeInfo,
                            aNodeInfo->NamespaceEquals(kNameSpaceID_XHTML));

  *aResult = htmlContent;
  NS_IF_ADDREF(*aResult);

  return rv;
}

// XXX compare switch statement against nsHTMLTags.h's list
nsresult
MakeContentObject(nsHTMLTag aNodeType, nsINodeInfo *aNodeInfo,
                  nsIDOMHTMLFormElement* aForm, nsIWebShell* aWebShell,
                  nsIHTMLContent** aResult, PRBool aInsideNoXXXTag,
                  PRBool aFromParser)
{
  nsresult rv = NS_OK;

  switch (aNodeType) {
  case eHTMLTag_a:
    rv = NS_NewHTMLAnchorElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_applet:
    rv = NS_NewHTMLAppletElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_area:
    rv = NS_NewHTMLAreaElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_base:
    rv = NS_NewHTMLBaseElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_basefont:
    rv = NS_NewHTMLBaseFontElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_blockquote:
    rv = NS_NewHTMLQuoteElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_body:
    rv = NS_NewHTMLBodyElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_br:
    rv = NS_NewHTMLBRElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_button:
    rv = NS_NewHTMLButtonElement(aResult, aNodeInfo);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_caption:
    rv = NS_NewHTMLTableCaptionElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_col:
    rv = NS_NewHTMLTableColElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_colgroup:
    rv = NS_NewHTMLTableColGroupElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_dir:
    rv = NS_NewHTMLDirectoryElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_del:
    rv = NS_NewHTMLDelElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_div:
  case eHTMLTag_marquee:
  case eHTMLTag_noembed:
  case eHTMLTag_noframes:
  case eHTMLTag_noscript:
  case eHTMLTag_parsererror:
  case eHTMLTag_sourcetext:
    rv = NS_NewHTMLDivElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_dl:
    rv = NS_NewHTMLDListElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_embed:
    rv = NS_NewHTMLEmbedElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_fieldset:
    rv = NS_NewHTMLFieldSetElement(aResult, aNodeInfo);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_font:
    rv = NS_NewHTMLFontElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_form:
    // the form was already created
    if (aForm) {
      rv = CallQueryInterface(aForm, aResult);
    } else {
      rv = NS_NewHTMLFormElement(aResult, aNodeInfo);
    }

    break;
  case eHTMLTag_frame:
    rv = NS_NewHTMLFrameElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_frameset:
    rv = NS_NewHTMLFrameSetElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_h1:
  case eHTMLTag_h2:
  case eHTMLTag_h3:
  case eHTMLTag_h4:
  case eHTMLTag_h5:
  case eHTMLTag_h6:
    rv = NS_NewHTMLHeadingElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_head:
    rv = NS_NewHTMLHeadElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_hr:
    rv = NS_NewHTMLHRElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_html:
    rv = NS_NewHTMLHtmlElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_iframe:
    rv = NS_NewHTMLIFrameElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_img:
    rv = NS_NewHTMLImageElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_input:
    rv = NS_NewHTMLInputElement(aResult, aNodeInfo, aFromParser);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_ins:
    rv = NS_NewHTMLInsElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_isindex:
    rv = NS_NewHTMLIsIndexElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_label:
    rv = NS_NewHTMLLabelElement(aResult, aNodeInfo);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_legend:
    rv = NS_NewHTMLLegendElement(aResult, aNodeInfo);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_li:
    rv = NS_NewHTMLLIElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_link:
    rv = NS_NewHTMLLinkElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_map:
    rv = NS_NewHTMLMapElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_menu:
    rv = NS_NewHTMLMenuElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_meta:
    rv = NS_NewHTMLMetaElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_object:
    rv = NS_NewHTMLObjectElement(aResult, aNodeInfo);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_ol:
    rv = NS_NewHTMLOListElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_optgroup:
    rv = NS_NewHTMLOptGroupElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_option:
    rv = NS_NewHTMLOptionElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_p:
    rv = NS_NewHTMLParagraphElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_pre:
    rv = NS_NewHTMLPreElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_param:
    rv = NS_NewHTMLParamElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_q:
    rv = NS_NewHTMLQuoteElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_script:
    rv = NS_NewHTMLScriptElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_select:
    rv = NS_NewHTMLSelectElement(aResult, aNodeInfo, aFromParser);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_spacer:
    rv = NS_NewHTMLSpacerElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_style:
    rv = NS_NewHTMLStyleElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_table:
    rv = NS_NewHTMLTableElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_tbody:
  case eHTMLTag_thead:
  case eHTMLTag_tfoot:
    rv = NS_NewHTMLTableSectionElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_td:
  case eHTMLTag_th:
    rv = NS_NewHTMLTableCellElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_textarea:
    rv = NS_NewHTMLTextAreaElement(aResult, aNodeInfo);
    if (!aInsideNoXXXTag) {
      SetForm(*aResult, aForm);
    }

    break;
  case eHTMLTag_title:
    rv = NS_NewHTMLTitleElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_tr:
    rv = NS_NewHTMLTableRowElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_ul:
    rv = NS_NewHTMLUListElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_wbr:
    rv = NS_NewHTMLWBRElement(aResult, aNodeInfo);

    break;
  case eHTMLTag_unknown:
  case eHTMLTag_userdefined:
    rv = NS_NewHTMLUnknownElement(aResult, aNodeInfo);

    break;
  default:
    rv = NS_NewHTMLSpanElement(aResult, aNodeInfo);

    break;
  }

  return rv;
}


//----------------------------------------------------------------------

MOZ_DECL_CTOR_COUNTER(SinkContext)

SinkContext::SinkContext(HTMLContentSink* aSink)
  : mSink(aSink), mPreAppend(PR_FALSE), mNotifyLevel(0), mLastTextNode(nsnull),
    mLastTextNodeSize(0), mStack(nsnull), mStackSize(0), mStackPos(0),
    mText(nsnull), mTextLength(0), mTextSize(0)
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

  NS_IF_RELEASE(mLastTextNode);
}

nsresult
SinkContext::Begin(nsHTMLTag aNodeType,
                   nsIHTMLContent* aRoot,
                   PRInt32 aNumFlushed,
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
  mStack[0].mFlags = APPENDED;
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

nsIHTMLContent*
SinkContext::GetCurrentContainer()
{
  nsIHTMLContent* content = mStack[mStackPos - 1].mContent;
  NS_ADDREF(content);

  return content;
}

void
SinkContext::DidAddContent(nsIContent* aContent, PRBool aDidNotify)
{
  PRInt32 childCount;

  // If there was a notification done for this content, update the
  // parent's notification count.
  if (aDidNotify && (0 < mStackPos)) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;
    parent->ChildCount(childCount);
    mStack[mStackPos - 1].mNumFlushed = childCount;
  }

  if ((mStackPos == 2) && (mSink->mBody == mStack[1].mContent)) {
    // We just finished adding something to the body
    mNotifyLevel = 0;
  }

  // If we just added content to a node for which
  // an insertion happen, we need to do an immediate
  // notification for that insertion.
  if (!aDidNotify && (0 < mStackPos) &&
      (mStack[mStackPos - 1].mInsertionPoint != -1)) {
    nsIContent* parent = mStack[mStackPos - 1].mContent;

#ifdef NS_DEBUG
    // Tracing code
    nsCOMPtr<nsIDTD> dtd;
    mSink->mParser->GetDTD(getter_AddRefs(dtd));
    nsHTMLTag tag = nsHTMLTag(mStack[mStackPos - 1].mType);
    nsDependentString str(dtd->IntTagToStringTag(tag));

    SINK_TRACE(SINK_TRACE_REFLOW,
               ("SinkContext::DidAddContent: Insertion notification for "
                "parent=%s at position=%d and stackPos=%d",
                NS_LossyConvertUCS2toASCII(str).get(),
                mStack[mStackPos - 1].mInsertionPoint - 1, mStackPos - 1));
#endif

    mSink->NotifyInsert(parent, aContent,
                        mStack[mStackPos - 1].mInsertionPoint - 1);
    parent->ChildCount(childCount);
    mStack[mStackPos - 1].mNumFlushed = childCount;
  } else if (!aDidNotify && mSink->IsTimeToNotify()) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("SinkContext::DidAddContent: Notification as a result of the "
                "interval expiring; backoff count: %d", mSink->mBackoffCount));
    FlushTags(PR_TRUE);
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

  nsresult rv;
  if (mStackPos + 1 > mStackSize) {
    rv = GrowStack();
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // Create new container content object
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  nsIHTMLContent* content;
  rv = mSink->CreateContentObject(aNode, nodeType, mSink->mCurrentForm,
                                  mSink->mFrameset ? mSink->mWebShell : nsnull,
                                  &content);
  NS_ENSURE_SUCCESS(rv, rv);

  mStack[mStackPos].mType = nodeType;
  mStack[mStackPos].mContent = content;
  mStack[mStackPos].mFlags = 0;
  mStack[mStackPos].mNumFlushed = 0;
  mStack[mStackPos].mInsertionPoint = -1;
  content->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);

  rv = mSink->AddAttributes(aNode, content);

  if (mPreAppend) {
    if (mStackPos <= 0) {
      NS_ERROR("container w/o parent");

      return NS_ERROR_FAILURE;
    }

    nsIHTMLContent* parent = mStack[mStackPos - 1].mContent;

    if (mStack[mStackPos - 1].mInsertionPoint != -1) {
      parent->InsertChildAt(content,
                            mStack[mStackPos - 1].mInsertionPoint++,
                            PR_FALSE, PR_FALSE);
    } else {
      parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
    }

    mStack[mStackPos].mFlags |= APPENDED;
  }

  mStackPos++;

  NS_ENSURE_SUCCESS(rv, rv);

  if (mSink->IsMonolithicContainer(nodeType)) {
    mSink->mInMonolithicContainer++;
  }

  // Special handling for certain tags
  switch (nodeType) {
    case eHTMLTag_noembed:
    case eHTMLTag_noframes:
      mSink->mInsideNoXXXTag++;

      break;
    case eHTMLTag_a:
      mSink->ProcessATag(aNode, content);

      break;
    case eHTMLTag_form:
    case eHTMLTag_table:
    case eHTMLTag_thead:
    case eHTMLTag_tbody:
    case eHTMLTag_tfoot:
    case eHTMLTag_tr:
    case eHTMLTag_td:
    case eHTMLTag_th:
      // XXX if navigator_quirks_mode (only body in html supports background)
      mSink->AddBaseTagInfo(content);

      break;
    case eHTMLTag_map:
      mSink->ProcessMAPTag(aNode, content);

      break;
    case eHTMLTag_iframe:
      mSink->mNumOpenIFRAMES++;

      break;
    default:
      break;
  }

  return NS_OK;
}

nsresult
SinkContext::CloseContainer(const nsHTMLTag aTag)
{
  nsresult result = NS_OK;

  // Flush any collected text content. Release the last text
  // node to indicate that no more should be added to it.
  FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::CloseContainer", 
                  aTag, mStackPos - 1, mSink);

  NS_WARN_IF_FALSE(mStackPos > 0,
                   "stack out of bounds. wrong context probably!");

  if (mStackPos <= 0) {
    return NS_OK; // Fix crash - Ref. bug 45975 or 45007
  }

  --mStackPos;
  nsHTMLTag nodeType = mStack[mStackPos].mType;

  nsIHTMLContent* content = mStack[mStackPos].mContent;

  content->Compact();

  // Add container to its parent if we haven't already done it
  if ((mStack[mStackPos].mFlags & APPENDED) == 0) {
    NS_ASSERTION(mStackPos > 0, "container w/o parent");
    if (mStackPos <= 0) {
      return NS_ERROR_FAILURE;
    }

    nsIHTMLContent* parent = mStack[mStackPos - 1].mContent;

    // If the parent has an insertion point, insert rather than
    // append.
    if (mStack[mStackPos - 1].mInsertionPoint != -1) {
      result = parent->InsertChildAt(content,
                                     mStack[mStackPos - 1].mInsertionPoint++,
                                     PR_FALSE, PR_FALSE);
    } else {
      result = parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
    }
  }

  // If we're in a state where we do append notifications as
  // we go up the tree, and we're at the level where the next
  // notification needs to be done, do the notification.
  if (mNotifyLevel >= mStackPos) {
    PRInt32 childCount;

    // Check to see if new content has been added after our last
    // notification
    content->ChildCount(childCount);

    if (mStack[mStackPos].mNumFlushed < childCount) {
#ifdef NS_DEBUG
      // Tracing code
      nsCOMPtr<nsIAtom> tag;
      mStack[mStackPos].mContent->GetTag(*getter_AddRefs(tag));
      const PRUnichar* tagChar;
      tag->GetUnicode(&tagChar);
      nsDependentString str(tagChar);

      SINK_TRACE(SINK_TRACE_REFLOW,
                 ("SinkContext::CloseContainer: reflow on notifyImmediate "
                  "tag=%s newIndex=%d stackPos=%d",
                  NS_LossyConvertUCS2toASCII(str).get(),
                  mStack[mStackPos].mNumFlushed, mStackPos));
#endif
      mSink->NotifyAppend(content, mStack[mStackPos].mNumFlushed);
    }

    // Indicate that notification has now happened at this level
    mNotifyLevel = mStackPos - 1;
  }

  if (mSink->IsMonolithicContainer(nodeType)) {
    --mSink->mInMonolithicContainer;
  }

  DidAddContent(content, PR_FALSE);

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
      mSink->mFlags &= ~NS_SINK_FLAG_FORM_ON_STACK;
      // If there's a FORM on the stack, but this close tag doesn't
      // close the form, then close out the form *and* close out the
      // next container up. This is since the parser doesn't do fix up
      // of invalid form nesting. When the end FORM tag comes through,
      // we'll ignore it.
      if (aTag != nodeType) {
        result = CloseContainer(aTag);
      }
    }

    break;
  case eHTMLTag_iframe:
    mSink->mNumOpenIFRAMES--;

    break;
  case eHTMLTag_select:
    {
      nsCOMPtr<nsISelectElement> select = do_QueryInterface(content);

      if (select) {
        result = select->DoneAddingChildren();
      }
    }

    break;
  default:
    break;
  }

  NS_IF_RELEASE(content);

#ifdef DEBUG
  if (mPreAppend &&
      SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
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
      nsCOMPtr<nsIHTMLContent> content;
      rv = mSink->CreateContentObject(aNode, nodeType,
                                      mSink->mCurrentForm, mSink->mWebShell,
                                      getter_AddRefs(content));
      NS_ENSURE_SUCCESS(rv, rv);

      // Set the content's document
      content->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);

      rv = mSink->AddAttributes(aNode, content);

      NS_ENSURE_SUCCESS(rv, rv);

      switch (nodeType) {
      case eHTMLTag_img:    // elements with 'SRC='
      case eHTMLTag_frame:
      case eHTMLTag_input:
        mSink->AddBaseTagInfo(content);

        break;
      default:
        break;
      }

      // Add new leaf to its parent
      AddLeaf(content);

      // Notify input and button that they are now fully created
      switch (nodeType) {
      case eHTMLTag_input:
      case eHTMLTag_button:
        content->DoneCreatingElement();

        break;
      case eHTMLTag_textarea:
      {
        // XXX textarea deserves to be treated like the container it is.
        nsCOMPtr<nsITextAreaElement> textarea(do_QueryInterface(content));
        if (textarea) {
          textarea->DoneAddingChildren();
        }

        break;
      }
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
SinkContext::AddLeaf(nsIHTMLContent* aContent)
{
  NS_ASSERTION(mStackPos > 0, "leaf w/o container");
  if (mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }

  nsIHTMLContent* parent = mStack[mStackPos - 1].mContent;

  // If the parent has an insertion point, insert rather than append.
  if (mStack[mStackPos - 1].mInsertionPoint != -1) {
    parent->InsertChildAt(aContent,
                          mStack[mStackPos - 1].mInsertionPoint++,
                          PR_FALSE, PR_FALSE);
  } else {
    parent->AppendChildTo(aContent, PR_FALSE, PR_FALSE);
  }

  DidAddContent(aContent, PR_FALSE);

#ifdef DEBUG
  if (mPreAppend &&
      SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
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

  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMComment> domComment(do_QueryInterface(comment));
  NS_ENSURE_TRUE(domComment, NS_ERROR_UNEXPECTED);

  domComment->AppendData(aNode.GetText());

  comment->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);

  nsIHTMLContent* parent;
  if (!mSink->mBody && mSink->mHead) {
    parent = mSink->mHead;
  } else {
    parent = mStack[mStackPos - 1].mContent;
  }

  // If the parent has an insertion point, insert rather than append.
  if (mStack[mStackPos - 1].mInsertionPoint != -1) {
    parent->InsertChildAt(comment,
                          mStack[mStackPos - 1].mInsertionPoint++,
                          PR_FALSE, PR_FALSE);
  } else {
    parent->AppendChildTo(comment, PR_FALSE, PR_FALSE);
  }

  DidAddContent(comment, PR_FALSE);

#ifdef DEBUG
  if (mPreAppend &&
      SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
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
 * Flush all elements that have been seen so far such that
 * they are visible in the tree. Specifically, make sure
 * that they are all added to their respective parents.
 * Also, do notification at the top for all content that
 * has been newly added so that the frame tree is complete.
 */
nsresult
SinkContext::FlushTags(PRBool aNotify)
{
  nsresult result = NS_OK;

  // Don't release last text node in case we need to add to it again
  FlushText();

  PRInt32 childCount;
  nsIHTMLContent* content;

  // Start from the top of the stack (growing upwards) and append
  // all content that hasn't been previously appended to the tree
  PRInt32 stackPos = mStackPos - 1;

  while ((stackPos > 0) && ((mStack[stackPos].mFlags & APPENDED) == 0)) {
    content = mStack[stackPos].mContent;
    nsIHTMLContent* parent = mStack[stackPos - 1].mContent;

    mStack[stackPos].mFlags |= APPENDED;

    // If the parent has an insertion point, insert rather than
    // append.
    if (mStack[mStackPos - 1].mInsertionPoint != -1) {
      parent->InsertChildAt(content, mStack[mStackPos - 1].mInsertionPoint++,
                            PR_FALSE, PR_FALSE);
    } else {
      parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
    }

    stackPos--;
  }

  if (aNotify) {
    // Start from the base of the stack (growing upward) and do
    // a notification from the node that is closest to the root of
    // tree for any content that has been added.
    stackPos = 1;
    PRBool flushed = PR_FALSE;
    while (stackPos < mStackPos) {
      content = mStack[stackPos].mContent;
      content->ChildCount(childCount);

      if (!flushed && (mStack[stackPos].mNumFlushed < childCount)) {
#ifdef NS_DEBUG
        // Tracing code
        nsCOMPtr<nsIAtom> tag;
        mStack[stackPos].mContent->GetTag(*getter_AddRefs(tag));
        const PRUnichar* tagChar;
        tag->GetUnicode(&tagChar);
        nsDependentString str(tagChar);

        SINK_TRACE(SINK_TRACE_REFLOW,
                   ("SinkContext::FlushTags: tag=%s from newindex=%d at "
                    "stackPos=%d", NS_LossyConvertUCS2toASCII(str).get(),
                    mStack[stackPos].mNumFlushed, stackPos));
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
  }

  return result;
}

void
SinkContext::UpdateChildCounts()
{
  PRInt32 childCount;
  nsIHTMLContent* content;

  // Start from the top of the stack (growing upwards) and see if any
  // new content has been appended. If so, we recognize that reflows
  // have been generated for it and we should make sure that no
  // further reflows occur.
  PRInt32 stackPos = mStackPos - 1;
  while (stackPos > 0) {
    if (mStack[stackPos].mFlags & APPENDED) {
      content = mStack[stackPos].mContent;
      content->ChildCount(childCount);
      mStack[stackPos].mNumFlushed = childCount;
    }

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
        NS_RELEASE(mLastTextNode);
        FlushText(aDidFlush, aReleaseLast);
      } else {
        nsCOMPtr<nsIDOMCharacterData> cdata(do_QueryInterface(mLastTextNode));

        if (cdata) {
          rv = cdata->AppendData(Substring(mText, mText + mTextLength));

          mLastTextNodeSize += mTextLength;
          mTextLength = 0;
          didFlush = PR_TRUE;
        }
      }
    } else {
      nsITextContent* content;
      rv = NS_NewTextNode(&content);
      NS_ENSURE_SUCCESS(rv, rv);

      // Set the content's document
      content->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);

      // Set the text in the text node
      content->SetText(mText, mTextLength, PR_FALSE);

      // Eat up the rest of the text up in state.
      mLastTextNode = content;
      mLastTextNodeSize += mTextLength;
      mTextLength = 0;

      // Add text to its parent
      NS_ASSERTION(mStackPos > 0, "leaf w/o container");
      if (mStackPos <= 0) {
        return NS_ERROR_FAILURE;
      }

      nsIHTMLContent* parent = mStack[mStackPos - 1].mContent;
      if (mStack[mStackPos - 1].mInsertionPoint != -1) {
        parent->InsertChildAt(content, mStack[mStackPos - 1].mInsertionPoint++,
                              PR_FALSE, PR_FALSE);
      } else {
        parent->AppendChildTo(content, PR_FALSE, PR_FALSE);
      }

      didFlush = PR_TRUE;

      DidAddContent(content, PR_FALSE);
    }
  }

  if (aDidFlush) {
    *aDidFlush = didFlush;
  }

  if (aReleaseLast && mLastTextNode) {
    mLastTextNodeSize = 0;
    NS_RELEASE(mLastTextNode);
  }

#ifdef DEBUG
  if (mPreAppend && didFlush &&
      SINK_LOG_TEST(gSinkLogModuleInfo, SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return rv;
}


nsresult
NS_NewHTMLContentSink(nsIHTMLContentSink** aResult,
                      nsIDocument* aDoc,
                      nsIURI* aURL,
                      nsIWebShell* aWebShell,
                      nsIChannel* aChannel)
{
  NS_ENSURE_ARG_POINTER(aResult);

  HTMLContentSink* it;
  NS_NEWXPCOM(it, HTMLContentSink);

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aDoc, aURL, aWebShell, aChannel);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aResult = it;
  NS_ADDREF(*aResult);

  return NS_OK;
}

HTMLContentSink::HTMLContentSink()
{
  // Note: operator new zeros our memory

  NS_INIT_ISUPPORTS();

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
  NS_IF_RELEASE(mFrameset);
  NS_IF_RELEASE(mRoot);

  if (mDocument) {
    mDocument->RemoveObserver(this);
    NS_RELEASE(mDocument);
  }
  NS_IF_RELEASE(mHTMLDocument);
  NS_IF_RELEASE(mDocumentURI);
  NS_IF_RELEASE(mDocumentBaseURL);
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mCSSLoader);

  if (mNotificationTimer) {
    mNotificationTimer->Cancel();
  }

  PRInt32 numContexts = mContextStack.Count();

  if (mCurrentContext == mHeadContext) {
    // Pop off the second html context if it's not done earlier
    mContextStack.RemoveElementAt(--numContexts);
  }

  for (PRInt32 i = 0; i < numContexts; i++) {
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
}

#ifdef DEBUG
NS_IMPL_ISUPPORTS7(HTMLContentSink,
                   nsIHTMLContentSink,
                   nsIContentSink,
                   nsIScriptLoaderObserver,
                   nsITimerCallback,
                   nsICSSLoaderObserver,
                   nsIDocumentObserver,
                   nsIDebugDumpContent)
#else
NS_IMPL_ISUPPORTS6(HTMLContentSink,
                   nsIHTMLContentSink,
                   nsIContentSink,
                   nsIScriptLoaderObserver,
                   nsITimerCallback,
                   nsICSSLoaderObserver,
                   nsIDocumentObserver)
#endif

static PRBool
IsScriptEnabled(nsIDocument *aDoc, nsIWebShell *aContainer)
{
  NS_ENSURE_TRUE(aDoc && aContainer, PR_TRUE);

  nsCOMPtr<nsIScriptSecurityManager> securityManager =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(securityManager, PR_TRUE);

  nsCOMPtr<nsIPrincipal> principal;
  aDoc->GetPrincipal(getter_AddRefs(principal));
  NS_ENSURE_TRUE(principal, PR_TRUE);

  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  aDoc->GetScriptGlobalObject(getter_AddRefs(globalObject));

  // Getting context is tricky if the document hasn't had it's
  // GlobalObject set yet
  if (!globalObject) {
    nsCOMPtr<nsIInterfaceRequestor> requestor(do_QueryInterface(aContainer));
    NS_ENSURE_TRUE(requestor, PR_TRUE);

    nsCOMPtr<nsIScriptGlobalObjectOwner> owner;
    requestor->GetInterface(NS_GET_IID(nsIScriptGlobalObjectOwner),
                            getter_AddRefs(owner));
    NS_ENSURE_TRUE(owner, PR_TRUE);

    owner->GetScriptGlobalObject(getter_AddRefs(globalObject));
    NS_ENSURE_TRUE(globalObject, PR_TRUE);
  }

  nsCOMPtr<nsIScriptContext> scriptContext;
  globalObject->GetContext(getter_AddRefs(scriptContext));
  NS_ENSURE_TRUE(scriptContext, PR_TRUE);

  JSContext* cx = (JSContext *) scriptContext->GetNativeContext();
  NS_ENSURE_TRUE(cx, PR_TRUE);

  PRBool enabled = PR_TRUE;
  securityManager->CanExecuteScripts(cx, principal, &enabled);

  return enabled;
}

nsresult
HTMLContentSink::Init(nsIDocument* aDoc,
                      nsIURI* aURL,
                      nsIWebShell* aContainer,
                      nsIChannel* aChannel)
{
  MOZ_TIMER_DEBUGLOG(("Reset and start: nsHTMLContentSink::Init(), this=%p\n",
                      this));
  MOZ_TIMER_RESET(mWatch);
  MOZ_TIMER_START(mWatch);

  if (!aDoc || !aURL || !aContainer) {
    NS_ERROR("Null ptr!");

    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);

    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;

  rv = NS_NewISupportsArray(getter_AddRefs(mScriptElements));
  if (NS_FAILED(rv)) return rv;

  mDocument = aDoc;
  NS_ADDREF(aDoc);

  aDoc->AddObserver(this);
  aDoc->QueryInterface(NS_GET_IID(nsIHTMLDocument), (void**)&mHTMLDocument);

  rv = mDocument->GetNodeInfoManager(*getter_AddRefs(mNodeInfoManager));
  NS_ENSURE_SUCCESS(rv, rv);

  mDocumentURI = aURL;
  NS_ADDREF(aURL);
  mDocumentBaseURL = aURL;
  NS_ADDREF(aURL);
  mWebShell = aContainer;
  NS_ADDREF(aContainer);

  mObservers = nsnull;

  nsIParserService* service = nsContentUtils::GetParserServiceWeakRef();
  if (!service) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  service->GetTopicObservers(NS_LITERAL_STRING("text/html"),
                             getter_AddRefs(mObservers));

  nsCOMPtr<nsIScriptLoader> loader;
  rv = mDocument->GetScriptLoader(getter_AddRefs(loader));
  NS_ENSURE_SUCCESS(rv, rv);
  loader->AddObserver(this);

  PRBool enabled = PR_TRUE;
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  NS_ASSERTION(docShell, "oops no docshell!");
  if (docShell) {
    docShell->GetAllowSubframes(&enabled);
    if (enabled) {
      mFlags |= NS_SINK_FLAG_FRAMES_ENABLED;
    }
  }

  // Find out if scripts are enabled, if not, show <noscript> content
  if (IsScriptEnabled(aDoc, aContainer)) {
    mFlags |= NS_SINK_FLAG_SCRIPT_ENABLED;
  }

  nsCOMPtr<nsIPrefBranch> prefBranch(do_GetService(NS_PREFSERVICE_CONTRACTID));

  PRBool notifyPref = PR_TRUE;
  if (prefBranch) {
    prefBranch->GetBoolPref("content.notify.ontimer", &notifyPref);
  }
  mNotifyOnTimer = notifyPref;

  mBackoffCount = -1; // never
  if (prefBranch) {
    prefBranch->GetIntPref("content.notify.backoffcount", &mBackoffCount);
  }

  // The mNotificationInterval has a dramatic effect on how long it
  // takes to initially display content for slow connections.
  // The current value provides good
  // incremental display of content without causing an increase
  // in page load time. If this value is set below 1/10 of second
  // it starts to impact page load performance.
  // see bugzilla bug 72138 for more info.
  mNotificationInterval = 120000;
  if (prefBranch) {
    prefBranch->GetIntPref("content.notify.interval", &mNotificationInterval);
  }

  // The mMaxTokenProcessingTime controls how long we stay away from
  // the event loop when processing token. A lower value makes the app
  // more responsive, but may increase page load time.  The content
  // sink mNotificationInterval gates how frequently the content is
  // processed so it will also affect how interactive the app is
  // during page load also. The mNotification prevents contents
  // flushes from happening too frequently. while
  // mMaxTokenProcessingTime prevents flushes from happening too
  // infrequently.

  // The current ratio of 3 to 1 was determined to be the lowest
  // mMaxTokenProcessingTime which does not impact page load
  // performance.  See bugzilla bug 76722 for details.

  mMaxTokenProcessingTime = mNotificationInterval * 3;

  PRBool enableInterruptParsing = PR_TRUE;

  // 3/4 second default for switching
  mDynamicIntervalSwitchThreshold = 750000;

  if (prefBranch) {
    prefBranch->GetBoolPref("content.interrupt.parsing",
                            &enableInterruptParsing);
    prefBranch->GetIntPref("content.max.tokenizing.time",
                           &mMaxTokenProcessingTime);
    prefBranch->GetIntPref("content.switch.threshold",
                           &mDynamicIntervalSwitchThreshold);
  }

  if (enableInterruptParsing) {
    mFlags |= NS_SINK_FLAG_CAN_INTERRUPT_PARSER;
  }

  // Changed from 8192 to greatly improve page loading performance on
  // large pages.  See bugzilla bug 77540.
  mMaxTextRun = 8191;
  if (prefBranch) {
    prefBranch->GetIntPref("content.maxtextrun", &mMaxTextRun);
  }

  nsCOMPtr<nsIHTMLContentContainer> htmlContainer(do_QueryInterface(aDoc));
  if (htmlContainer) {
    htmlContainer->GetCSSLoader(mCSSLoader);
  }

  // XXX this presumes HTTP header info is alread set in document
  // XXX if it isn't we need to set it here...

  ProcessHTTPHeaders(aChannel);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::html, nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // Make root part
  nsCOMPtr<nsIContent> doc_root;
  mDocument->GetRootContent(getter_AddRefs(doc_root));

  if (doc_root) {
    // If the document already has a root we'll use it. This will
    // happen when we do document.open()/.write()/.close()...

    CallQueryInterface(doc_root, &mRoot);
  } else {
    rv = NS_NewHTMLHtmlElement(&mRoot, nodeInfo);
    if (NS_FAILED(rv)) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
      MOZ_TIMER_STOP(mWatch);
      return rv;
    }

    mRoot->SetDocument(mDocument, PR_FALSE, PR_TRUE);
    mDocument->SetRootContent(mRoot);
  }

  // Make head part
  rv = mNodeInfoManager->GetNodeInfo(NS_LITERAL_STRING("head"),
                                     nsnull, kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewHTMLHeadElement(&mHead, nodeInfo);
  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }

  mRoot->AppendChildTo(mHead, PR_FALSE, PR_FALSE);

  mCurrentContext = new SinkContext(this);
  NS_ENSURE_TRUE(mCurrentContext, NS_ERROR_OUT_OF_MEMORY);
  mCurrentContext->Begin(eHTMLTag_html, mRoot, 0, -1);
  mContextStack.AppendElement(mCurrentContext);

#ifdef NS_DEBUG
  nsCAutoString spec;
  (void)aURL->GetSpec(spec);
  SINK_TRACE(SINK_TRACE_CALLS,
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
  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
    nsresult rv = AddDummyParserRequest();
    if (NS_FAILED(rv)) {
      NS_ERROR("Adding dummy parser request failed");

      // Don't return the error result, just reset flag which
      // indicates that it can interrupt parsing. If
      // AddDummyParserRequests fails it should not affect
      // WillBuildModel.
      mFlags &= ~NS_SINK_FLAG_CAN_INTERRUPT_PARSER;
    }

    mBeginLoadTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  mScrolledToRefAlready = PR_FALSE;

  if (mHTMLDocument) {
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
HTMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{

  // NRA Dump stopwatch stop info here
#ifdef MOZ_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::DidBuildModel(), this=%p\n",
                      this));
  MOZ_TIMER_STOP(mWatch);
  MOZ_TIMER_LOG(("Content creation time (this=%p): ", this));
  MOZ_TIMER_PRINT(mWatch);
#endif

  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: canceling notification "
                "timeout"));
    mNotificationTimer->Cancel();
    mNotificationTimer = 0;
  }

  if (mTitle.IsEmpty()) {
    nsCOMPtr<nsIDOMHTMLDocument> domDoc(do_QueryInterface(mHTMLDocument));
    if (domDoc)
      domDoc->SetTitle(mTitle);
  }

  // XXX this is silly; who cares? RickG cares. It's part of the
  // regression test. So don't bug me.
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell;
    mDocument->GetShellAt(i, getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIViewManager> vm;
      nsresult rv = shell->GetViewManager(getter_AddRefs(vm));
      if (NS_SUCCEEDED(rv) && vm) {
        vm->SetQuality(nsContentQuality(aQualityLevel));
      }
    }
  }

  // Reflow the last batch of content
  if (mBody) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: layout final content"));
    mCurrentContext->FlushTags(PR_TRUE);
  } else if (!mLayoutStarted) {
    // We never saw the body, and layout never got started. Force
    // layout *now*, to get an initial reflow.
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: forcing reflow on empty "
                "document"));

    // NOTE: only force the layout if we are NOT destroying the
    // webshell. If we are destroying it, then starting layout will
    // likely cause us to crash, or at best waste a lot of time as we
    // are just going to tear it down anyway.
    PRBool bDestroying = PR_TRUE;
    if (mWebShell) {
      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
      if (docShell) {
        docShell->IsBeingDestroyed(&bDestroying);
      }
    }

    if (!bDestroying) {
      StartLayout();
    }
  }

  if (mWebShell) {
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
    if (docShell) {
      PRUint32 LoadType;
      docShell->GetLoadType(&LoadType);

      if (!(LoadType & nsIDocShell::LOAD_CMD_HISTORY))
        ScrollToRef();
    }
  }

  nsCOMPtr<nsIScriptLoader> loader;
  mDocument->GetScriptLoader(getter_AddRefs(loader));
  if (loader) {
    loader->RemoveObserver(this);
  }

  mDocument->EndLoad();

  // Ref. Bug 49115
  // Do this hack to make sure that the parser
  // doesn't get destroyed, accidently, before
  // the circularity, between sink & parser, is
  // actually borken.
  nsCOMPtr<nsIParser> kungFuDeathGrip(mParser);

  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);

  if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
    // Reset the performance hint which was set to FALSE
    // when NS_SINK_FLAG_DYNAMIC_LOWER_VALUE was set. 
    PL_FavorPerformanceHint(PR_TRUE , 0);
  }

  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
    // Note: Don't return value from RemoveDummyParserRequest,
    // If RemoveDummyParserRequests fails it should not affect
    // DidBuildModel. The remove can fail if the parser request
    // was already removed by a DummyParserRequest::Cancel
    RemoveDummyParserRequest();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::Notify(nsITimer *timer)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_START(mWatch);

#ifdef MOZ_DEBUG
  {
    PRTime now = PR_Now();
    PRInt64 diff, interval;
    PRInt32 delay;

    LL_I2L(interval, GetNotificationInterval());
    LL_SUB(diff, now, mLastNotificationTime);

    LL_SUB(diff, diff, interval);
    LL_L2I(delay, diff);
    delay /= PR_USEC_PER_MSEC;

    mBackoffCount--;
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::Notify: reflow on a timer: %d milliseconds "
                "late, backoff count: %d", delay, mBackoffCount));
  }
#endif

  if (mCurrentContext) {
    mCurrentContext->FlushTags(PR_TRUE);
  }

  // Now try and scroll to the reference
  TryToScrollToRef();

  mNotificationTimer = 0;
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillInterrupt()
{
  nsresult result = NS_OK;

  SINK_TRACE(SINK_TRACE_CALLS, ("HTMLContentSink::WillInterrupt: this=%p",
                                this));
#ifndef SINK_NO_INCREMENTAL
  if (mNotifyOnTimer && mLayoutStarted) {
    if (mBackoffCount && !mInMonolithicContainer) {
      PRTime now = PR_Now();
      PRInt64 interval, diff;
      PRInt32 delay;

      LL_I2L(interval, GetNotificationInterval());
      LL_SUB(diff, now, mLastNotificationTime);

      // If it's already time for us to have a notification
      if (LL_CMP(diff, >, interval)) {
        mBackoffCount--;
        SINK_TRACE(SINK_TRACE_REFLOW,
                 ("HTMLContentSink::WillInterrupt: flushing tags since we've "
                  "run out time; backoff count: %d", mBackoffCount));
        result = mCurrentContext->FlushTags(PR_TRUE);
      } else {
        // If the time since the last notification is less than
        // the expected interval but positive, set a timer up for the remaining
        // interval.
        if (LL_CMP(diff, >, LL_ZERO)) {
          LL_SUB(diff, interval, diff);
          LL_L2I(delay, diff);
        } else {
          // Else set up a timer for the expected interval
          delay = GetNotificationInterval();
        }

        // Convert to milliseconds
        delay /= PR_USEC_PER_MSEC;

        // Cancel a timer if we had one out there
        if (mNotificationTimer) {
          SINK_TRACE(SINK_TRACE_REFLOW,
                     ("HTMLContentSink::WillInterrupt: canceling notification "
                      "timeout"));

          mNotificationTimer->Cancel();
        }

        mNotificationTimer = do_CreateInstance("@mozilla.org/timer;1",
                                               &result);
        if (NS_SUCCEEDED(result)) {
          SINK_TRACE(SINK_TRACE_REFLOW,
                     ("HTMLContentSink::WillInterrupt: setting up timer with "
                      "delay %d", delay));

          result =
            mNotificationTimer->InitWithCallback(this, delay,
                                                 nsITimer::TYPE_ONE_SHOT);
        }
      }
    }
  } else {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::WillInterrupt: flushing tags "
                "unconditionally"));

    result = mCurrentContext->FlushTags(PR_TRUE);
  }
#endif

  return result;
}

NS_IMETHODIMP
HTMLContentSink::WillResume()
{
  SINK_TRACE(SINK_TRACE_CALLS, ("HTMLContentSink::WillResume: this=%p", this));

  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::WillResume: canceling notification "
                "timeout"));

    mNotificationTimer->Cancel();
    mNotificationTimer = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::SetParser(nsIParser* aParser)
{
  NS_IF_RELEASE(mParser);
  mParser = aParser;
  NS_IF_ADDREF(mParser);

  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
HTMLContentSink::IsFormOnStack()
{
  return mFlags & NS_SINK_FLAG_FORM_ON_STACK;
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
  mCurrentContext->FlushTags(PR_TRUE);

  PRInt32 insertionPoint = -1;
  nsHTMLTag nodeType      = mCurrentContext->mStack[aPosition].mType;
  nsIHTMLContent* content = mCurrentContext->mStack[aPosition].mContent;

  // If the content under which the new context is created
  // has a child on the stack, the insertion point is
  // before the last child.
  if (aPosition < (mCurrentContext->mStackPos - 1)) {
    content->ChildCount(insertionPoint);
    insertionPoint--;
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


NS_IMETHODIMP
HTMLContentSink::SetTitle(const nsString& aValue)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::SetTitle()\n"));
  MOZ_TIMER_START(mWatch);
  NS_ASSERTION(mCurrentContext == mHeadContext, "SetTitle not in head");

  if (!mTitle.IsEmpty()) {
    // If the title was already set then don't try to overwrite it
    // when a new title is encountered - For backwards compatiblity
    //*mTitle = aValue;
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::SetTitle()\n"));
    MOZ_TIMER_STOP(mWatch);

    return NS_OK;
  }

  mTitle.Assign(aValue);
  mTitle.CompressWhitespace(PR_TRUE, PR_TRUE);

  nsCOMPtr<nsIDOMHTMLDocument> domDoc(do_QueryInterface(mHTMLDocument));
  if (domDoc) {
    domDoc->SetTitle(mTitle);
  }

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::title, nsnull,
                                              kNameSpaceID_None,
                                              *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> it;
  rv = NS_NewHTMLTitleElement(getter_AddRefs(it), nodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsITextContent> text;
  rv = NS_NewTextNode(getter_AddRefs(text));
  NS_ENSURE_SUCCESS(rv, rv);

  text->SetText(mTitle, PR_TRUE);

  it->AppendChildTo(text, PR_FALSE, PR_FALSE);
  text->SetDocument(mDocument, PR_FALSE, PR_TRUE);

  mHead->AppendChildTo(it, PR_FALSE, PR_FALSE);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::SetTitle()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenHTML(const nsIParserNode& aNode)
{
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenHTML", 
                  eHTMLTag_html, 0, this);

  if (mRoot) {
    // Add attributes to the node...if found.
    PRInt32 ac = aNode.GetAttributeCount();

    if (ac > 0) {
      AddAttributes(aNode, mRoot, PR_TRUE);
    }
  }

  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
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

    mHeadContext->End();

    delete mHeadContext;
    mHeadContext = nsnull;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseHTML()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenHead(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenHead()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenHead", 
                  eHTMLTag_head, 0, this);

  nsresult rv = NS_OK;

  // Flush everything in the current context so that we don't have
  // to worry about insertions resulting in inconsistent frame creation.
  //
  // Try to do this only if needed (costly), i.e., only if we are sure
  // we are changing contexts from some other context to the head.
  //
  // PERF: This call causes approximately a 2% slowdown in page load time
  // according to jrgm's page load tests, but seems to be a necessary evil
  if (mCurrentContext && (mCurrentContext != mHeadContext)) {
    mCurrentContext->FlushTags(PR_TRUE);
  }

  if (!mHeadContext) {
    mHeadContext = new SinkContext(this);
    if (!mHeadContext) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
      MOZ_TIMER_STOP(mWatch);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    mHeadContext->SetPreAppend(PR_TRUE);

    rv = mHeadContext->Begin(eHTMLTag_head, mHead, 0, -1);
    if (NS_FAILED(rv)) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
      MOZ_TIMER_STOP(mWatch);

      return rv;
    }
  }

  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = mHeadContext;

  if (mHead && aNode.GetNodeType() == eHTMLTag_head) {
    rv = AddAttributes(aNode, mHead);
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseHead()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseHead()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseHead", 
                  eHTMLTag_head, 0, this);

  PRInt32 n = mContextStack.Count() - 1;

  mCurrentContext = (SinkContext*) mContextStack.ElementAt(n);
  mContextStack.RemoveElementAt(n);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseHead()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenBody(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenBody()\n"));
  MOZ_TIMER_START(mWatch);

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenBody", 
                  eHTMLTag_body,
                  mCurrentContext->mStackPos, 
                  this);
  // Add attributes, if any, to the current BODY node

  if (mBody) {
    AddAttributes(aNode, mBody, PR_TRUE);

    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return NS_OK;
  }

  // Open body. Note that we pre-append the body to the root so that
  // incremental reflow during document loading will work properly.
  mCurrentContext->SetPreAppend(PR_TRUE);

  nsresult rv = mCurrentContext->OpenContainer(aNode);

  mCurrentContext->SetPreAppend(PR_FALSE);

  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);

    return rv;
  }

  mBody = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;

  NS_ADDREF(mBody);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
  MOZ_TIMER_STOP(mWatch);

  // Check to see if InitialReflow() has been called on any of our
  // presShells. If so, the InitialReflow() call inside StartLayout()
  // will be supressed, so we can't rely on it to construct the body
  // frame for us, so we'll have to manually call NotifyInsert() or
  // NotifyAppend() to make sure a body frame gets constructed. (Bug 153815)

  PRBool didInitialReflow = PR_FALSE;

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell;
    mDocument->GetShellAt(i, getter_AddRefs(shell));
    if (shell) {
      shell->GetDidInitialReflow(&didInitialReflow);
      if (didInitialReflow) {
        break;
      }
    }
  }

  if (didInitialReflow && mCurrentContext->mStackPos > 1) {
    PRInt32 parentIndex    = mCurrentContext->mStackPos - 2;
    nsIHTMLContent *parent = mCurrentContext->mStack[parentIndex].mContent;
    PRInt32 numFlushed     = mCurrentContext->mStack[parentIndex].mNumFlushed;
    PRInt32 insertionPoint =
      mCurrentContext->mStack[parentIndex].mInsertionPoint;

    // XXX: I have yet to see a case where numFlushed is non-zero and
    // insertionPoint is not -1, but this code will try to handle
    // those cases too.

    if (insertionPoint != -1) {
      NotifyInsert(parent, mBody, insertionPoint - 1);
    } else {
      // XXX: Would it be better to use |parent->ChildCount() - 1| so
      // that we don't cause notifications for the <head> element and
      // it's children?

      NotifyAppend(parent, numFlushed);
    }
  }

  StartLayout();

  return NS_OK;
}

NS_IMETHODIMP
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
  SINK_TRACE(SINK_TRACE_REFLOW,
             ("HTMLContentSink::CloseBody: layout final body content"));

  mCurrentContext->FlushTags(PR_TRUE);
  mCurrentContext->CloseContainer(eHTMLTag_body);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseBody()\n"));
  MOZ_TIMER_STOP(mWatch);

  return NS_OK;
}

NS_IMETHODIMP
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
    nsCOMPtr<nsINodeInfo> nodeInfo;
    result = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::form, nsnull,
                                           kNameSpaceID_None,
                                           *getter_AddRefs(nodeInfo));
    NS_ENSURE_SUCCESS(result, result);

    nsCOMPtr<nsIHTMLContent> content;
    result = NS_NewHTMLFormElement(getter_AddRefs(content), nodeInfo);
    NS_ENSURE_SUCCESS(result, result);

    mCurrentForm = do_QueryInterface(content);

    result = AddLeaf(aNode);
  } else {
    mFlags |= NS_SINK_FLAG_FORM_ON_STACK;
    // Otherwise the form can be a content parent.
    result = mCurrentContext->OpenContainer(aNode);
    if (NS_SUCCEEDED(result)) {
      nsCOMPtr<nsIHTMLContent> content =
        dont_AddRef(mCurrentContext->GetCurrentContainer());

      mCurrentForm = do_QueryInterface(content);
    }
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenForm()\n"));
  MOZ_TIMER_STOP(mWatch);

  return result;
}

// XXX MAYBE add code to place close form tag into the content model
// for navigator layout compatability.
NS_IMETHODIMP
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
      mCurrentContext->FlushTextAndRelease();
      result = mCurrentContext->CloseContainer(eHTMLTag_form);
      mFlags &= ~NS_SINK_FLAG_FORM_ON_STACK;
    }

    mCurrentForm = nsnull;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseForm()\n"));
  MOZ_TIMER_STOP(mWatch);

  return result;
}

NS_IMETHODIMP
HTMLContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenFrameset()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenFrameset", 
                  eHTMLTag_frameset,
                  mCurrentContext->mStackPos, 
                  this);

  nsresult rv = mCurrentContext->OpenContainer(aNode);

  if (NS_SUCCEEDED(rv) && !mFrameset &&
      (mFlags & NS_SINK_FLAG_FRAMES_ENABLED)) {
    mFrameset =
      mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
    NS_ADDREF(mFrameset);
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
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
  nsIHTMLContent* fs = sc->mStack[sc->mStackPos - 1].mContent;
  PRBool done = fs == mFrameset;
  nsresult rv = sc->CloseContainer(eHTMLTag_frameset);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);

  if (done && (mFlags & NS_SINK_FLAG_FRAMES_ENABLED)) {
    StartLayout();
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::OpenMap(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenMap()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = NS_OK;

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenMap", 
                  eHTMLTag_map,
                  mCurrentContext->mStackPos, 
                  this);

  // We used to treat MAP elements specially (i.e. they were
  // only parent elements for AREAs), but we don't anymore.
  // HTML 4.0 says that MAP elements can have block content
  // as children.
  rv = mCurrentContext->OpenContainer(aNode);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenMap()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseMap()
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseMap()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = NS_OK;

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseMap", 
                  eHTMLTag_map,
                  mCurrentContext->mStackPos - 1, 
                  this);

  mCurrentMap = nsnull;

  rv = mCurrentContext->CloseContainer(eHTMLTag_map);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseMap()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::GetPref(PRInt32 aTag, PRBool& aPref)
{
  nsHTMLTag theHTMLTag = nsHTMLTag(aTag);

  if (theHTMLTag == eHTMLTag_script) {
    aPref = mFlags & NS_SINK_FLAG_SCRIPT_ENABLED;
  } else if (theHTMLTag == eHTMLTag_frameset) {
    aPref = mFlags & NS_SINK_FLAG_FRAMES_ENABLED;
  } else {
    aPref = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenContainer()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv = NS_OK;

  // XXX work around parser bug
  if (eHTMLTag_frameset == aNode.GetNodeType()) {
    rv = OpenFrameset(aNode);
  } else {
    rv = mCurrentContext->OpenContainer(aNode);
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

  // XXX work around parser bug
  if (eHTMLTag_frameset == aTag) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseContainer()\n"));
    MOZ_TIMER_STOP(mWatch);
    return CloseFrameset();
  }

  rv = mCurrentContext->CloseContainer(aTag);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseContainer()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddLeaf()\n"));
  MOZ_TIMER_START(mWatch);

  nsresult rv;

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  switch (nodeType) {
  case eHTMLTag_area:
    rv = ProcessAREATag(aNode);

    break;
  case eHTMLTag_base:
    mCurrentContext->FlushTextAndRelease();
    rv = ProcessBASETag(aNode);

    break;
  case eHTMLTag_link:
    mCurrentContext->FlushTextAndRelease();
    rv = ProcessLINKTag(aNode);

    break;
  case eHTMLTag_meta:
    mCurrentContext->FlushTextAndRelease();
    rv = ProcessMETATag(aNode);

    break;
  case eHTMLTag_style:
    mCurrentContext->FlushTextAndRelease();
    rv = ProcessSTYLETag(aNode);

    break;
  case eHTMLTag_script:
    mCurrentContext->FlushTextAndRelease();
    rv = ProcessSCRIPTTag(aNode);

    break;
  default:
    rv = mCurrentContext->AddLeaf(aNode);

    break;
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddLeaf()\n"));
  MOZ_TIMER_STOP(mWatch);

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

  nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mHTMLDocument));

  if (!doc) {
    return NS_OK;
  }

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
       * If we didn't find 'SYSTEM' we tread everything after the public id
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
      if (Substring(systemId, 0, 6).Equals(NS_LITERAL_STRING("SYSTEM")))
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
  if (Substring(name, 0, 9).Equals(NS_LITERAL_STRING("<!DOCTYPE"))) {
    name.Cut(0, 9);
  } else if (Substring(name, 0, 7).Equals(NS_LITERAL_STRING("DOCTYPE"))) {
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

    doc->GetDoctype(getter_AddRefs(oldDocType));

    nsCOMPtr<nsIDOMDOMImplementation> domImpl;

    rv = doc->GetImplementation(getter_AddRefs(domImpl));

    if (NS_FAILED(rv) || !domImpl) {
      return rv;
    }

    if (name.IsEmpty()) {
      name.Assign(NS_LITERAL_STRING("HTML"));
    }

    rv = domImpl->CreateDocumentType(name, publicId, systemId,
                                     getter_AddRefs(docType));

    if (NS_FAILED(rv) || !docType) {
      return rv;
    }
    nsCOMPtr<nsIDOMNode> tmpNode;

    if (oldDocType) {
      // If we already have a doctype we replace the old one.

      rv = doc->ReplaceChild(oldDocType, docType, getter_AddRefs(tmpNode));
    } else {
      // If we don't already have one we insert it as the first child,
      // this might not be 100% correct but since this is called from
      // the content sink we assume that this is what we want.
      nsCOMPtr<nsIDOMNode> firstChild;

      doc->GetFirstChild(getter_AddRefs(firstChild));

      // If the above fails it must be because we don't have any child
      // nodes, then firstChild will be 0 and InsertBefore() will
      // append

      rv = doc->InsertBefore(docType, firstChild, getter_AddRefs(tmpNode));
    }
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddDocTypeDecl()\n"));
  MOZ_TIMER_STOP(mWatch);

  return rv;
}


NS_IMETHODIMP
HTMLContentSink::WillProcessTokens(void)
{
  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
    mDelayTimerStart = PR_IntervalToMicroseconds(PR_IntervalNow());
  }

  return NS_OK;
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
  if (mFlags & NS_SINK_FLAG_CAN_INTERRUPT_PARSER) {
#ifdef NS_DEBUG
    PRInt32 oldMaxTokenProcessingTime = GetMaxTokenProcessingTime();
#endif

    // There is both a high frequency interrupt mode and a low
    // frequency interupt mode controlled by the flag
    // NS_SINK_FLAG_DYNAMIC_LOWER_VALUE The high frequency mode
    // interupts the parser frequently to provide UI responsiveness at
    // the expense of page load time. The low frequency mode
    // interrupts the parser and samples the system clock infrequently
    // to provide fast page load time. When the user moves the mouse,
    // clicks or types the mode switches to the high frequency
    // interrupt mode. If the user stops moving the mouse or typing
    // for a duration of time (mDynamicIntervalSwitchThreshold) it
    // switches to low frequency interrupt mode.

    // Get the current user event time
    nsCOMPtr<nsIPresShell> shell;
    mDocument->GetShellAt(0, getter_AddRefs(shell));

    if (!shell) {
      // If there's no pres shell in the document, return early since
      // we're not laying anything out here.

      return NS_OK;
    }

    nsCOMPtr<nsIViewManager> vm;
    shell->GetViewManager(getter_AddRefs(vm));
    NS_ENSURE_TRUE(vm, NS_ERROR_FAILURE);
    PRUint32 eventTime;
    nsCOMPtr<nsIWidget> widget;
    nsresult rv = vm->GetWidget(getter_AddRefs(widget));
    if (!widget || NS_FAILED(widget->GetLastInputEventTime(eventTime))) {
        // If we can't get the last input time from the widget
        // then we will get it from the viewmanager.
        rv = vm->GetLastUserEventTime(eventTime);
        NS_ENSURE_TRUE(NS_SUCCEEDED(rv), NS_ERROR_FAILURE);
    }

    NS_ENSURE_TRUE(NS_SUCCEEDED(rv), NS_ERROR_FAILURE);

    if ((!(mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE)) &&
      (mLastSampledUserEventTime == eventTime)) {
      // The magic value of NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE
      // was selected by empirical testing. It provides reasonable
      // user response and prevents us from sampling the clock too
      // frequently.
      if (mDeflectedCount < NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE) {
        mDeflectedCount++;

        // return early to prevent sampling the clock. Note: This
        // prevents us from switching to higher frequency (better UI
        // responsive) mode, so limit ourselves to doing for no more
        // than NS_MAX_TOKENS_DEFLECTED_IN_LOW_FREQ_MODE tokens.

        return NS_OK;
      }

      // reset count and drop through to the code which samples the
      // clock and does the dynamic switch between the high
      // frequency and low frequency interruption of the parser.

      mDeflectedCount = 0;
    }

    mLastSampledUserEventTime = eventTime;

    PRUint32 currentTime = PR_IntervalToMicroseconds(PR_IntervalNow());

    // Get the last user event time and compare it with the current
    // time to determine if the lower value for content notification
    // and max token processing should be used. But only consider
    // using the lower value if the document has already been loading
    // for 2 seconds. 2 seconds was chosen because it is greater than
    // the default 3/4 of second that is used to determine when to
    // switch between the modes and it gives the document a little
    // time to create windows.  This is important because on some
    // systems (Windows, for example) when a window is created and the
    // mouse is over it, a mouse move event is sent, which will kick
    // us into interactive mode otherwise. It also supresses reaction
    // to pressing the ENTER key in the URL bar...

    PRUint32 delayBeforeLoweringThreshold =
      NS_STATIC_CAST(PRUint32, ((2 * mDynamicIntervalSwitchThreshold) +
                                NS_DELAY_FOR_WINDOW_CREATION));

    if ((currentTime - mBeginLoadTime) > delayBeforeLoweringThreshold) {
      if ((currentTime - eventTime) <
          NS_STATIC_CAST(PRUint32, mDynamicIntervalSwitchThreshold)) {
    
        if (! (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE)) {
          // lower the dynamic values to favor application
          // responsiveness over page load time.
          mFlags |= NS_SINK_FLAG_DYNAMIC_LOWER_VALUE;
          // Set the performance hint to prevent event starvation when
          // dispatching PLEvents. This improves application responsiveness 
          // during page loads.
          PL_FavorPerformanceHint(PR_FALSE, 0);
        }

      } else {
      
        if (mFlags & NS_SINK_FLAG_DYNAMIC_LOWER_VALUE) {
          // raise the content notification and MaxTokenProcessing time
          // to favor overall page load speed over responsiveness.
          mFlags &= ~NS_SINK_FLAG_DYNAMIC_LOWER_VALUE;
          // Reset the hint that to favoring performance for PLEvent dispatch.
          PL_FavorPerformanceHint(PR_TRUE, 0);
        }

      }
    }

#ifdef NS_DEBUG
#if 0
    PRInt32 newMaxTokenProcessingTime = GetMaxTokenProcessingTime();

    if (newMaxTokenProcessingTime != oldMaxTokenProcessingTime) {
      printf("Changed dynamic interval : MaxTokenProcessingTime %d\n",
             GetMaxTokenProcessingTime());
    }
#endif
#endif

    if ((currentTime - mDelayTimerStart) >
        NS_STATIC_CAST(PRUint32, GetMaxTokenProcessingTime())) {
      return NS_ERROR_HTMLPARSER_INTERRUPTED;
    }
  }

  return NS_OK;
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

  return mObservers->Notify(aNode, mParser, mWebShell, flag);
}

void
HTMLContentSink::StartLayout()
{
  if (mLayoutStarted) {
    return;
  }

  mLayoutStarted = PR_TRUE;

  mLastNotificationTime = PR_Now();

  // If it's a frameset document then disable scrolling.
  // Else, reset scrolling to default settings for this shell.
  // This must happen before the initial reflow, when we create the root frame
  nsresult rv;
  nsCOMPtr<nsIScrollable> scrollableContainer = do_QueryInterface(mWebShell);
  if (scrollableContainer) {
    if (mFrameset) {
      scrollableContainer->
        SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                       NS_STYLE_OVERFLOW_HIDDEN);
      scrollableContainer->
        SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                       NS_STYLE_OVERFLOW_HIDDEN);
    } else {
      scrollableContainer->ResetScrollbarPreferences();
    }
  }

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell;
    mDocument->GetShellAt(i, getter_AddRefs(shell));

    if (shell) {
      // Make sure we don't call InitialReflow() for a shell that has
      // already called it. This can happen when the layout frame for
      // an iframe is constructed *between* the Embed() call for the
      // docshell in the iframe, and the content sink's call to OpenBody().
      // (Bug 153815)

      PRBool didInitialReflow = PR_FALSE;
      shell->GetDidInitialReflow(&didInitialReflow);
      if (didInitialReflow) {
        // XXX: The assumption here is that if something already
        // called InitialReflow() on this shell, it also did some of
        // the setup below, so we do nothing and just move on to the
        // next shell in the list.

        continue;
      }

      // Make shell an observer for next time
      shell->BeginObservingDocument();

      // Resize-reflow this time
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsRect r;
      cx->GetVisibleArea(r);
      shell->InitialReflow(r.width, r.height);

      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      shell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        RefreshIfEnabled(vm);
      }
    }
  }

  // If the document we are loading has a reference or it is a
  // frameset document, disable the scroll bars on the views.

  if (mDocumentURI) {
    nsCAutoString ref;

    // Since all URI's that pass through here aren't URL's we can't
    // rely on the nsIURI implementation for providing a way for
    // finding the 'ref' part of the URI, we'll haveto revert to
    // string routines for finding the data past '#'

    rv = mDocumentURI->GetSpec(ref);

    nsReadingIterator<char> start, end;

    ref.BeginReading(start);
    ref.EndReading(end);

    if (FindCharInReadable('#', start, end)) {
      ++start; // Skip over the '#'

      mRef = Substring(start, end);
    }
  }

  if (!mRef.IsEmpty() || mFrameset) {
    // XXX support more than one presentation-shell here

    // Get initial scroll preference and save it away; disable the
    // scroll bars.
    ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsCOMPtr<nsIPresShell> shell;
      mDocument->GetShellAt(i, getter_AddRefs(shell));
      if (shell) {
        nsCOMPtr<nsIViewManager> vm;
        shell->GetViewManager(getter_AddRefs(vm));
        if (vm) {
          nsIView* rootView = nsnull;
          vm->GetRootView(rootView);
          if (rootView) {
            nsCOMPtr<nsIScrollableView> sview(do_QueryInterface(rootView));

            if (sview) {
              sview->SetScrollPreference(nsScrollPreference_kNeverScroll);
            }
          }
        }
      }
    }
  }
}

// Convert the ref from document charset to unicode.
nsresult
CharsetConvRef(const nsString& aDocCharset, const nsCString& aRefInDocCharset,
               nsString& aRefInUnicode)
{
  nsresult rv;

  nsCOMPtr <nsIAtom> docCharsetAtom;
  nsCOMPtr<nsICharsetConverterManager2> ccm2 =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = ccm2->GetCharsetAtom(aDocCharset.get(), getter_AddRefs(docCharsetAtom));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  rv = ccm2->GetUnicodeDecoder(docCharsetAtom, getter_AddRefs(decoder));
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 srcLen = aRefInDocCharset.Length();
  PRInt32 dstLen;
  rv = decoder->GetMaxLength(aRefInDocCharset.get(), srcLen, &dstLen);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRUnichar *ustr = (PRUnichar *)nsMemory::Alloc((dstLen + 1) *
                                                 sizeof(PRUnichar));
  if (!ustr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = decoder->Convert(aRefInDocCharset.get(), &srcLen, ustr, &dstLen);
  if (NS_SUCCEEDED(rv)) {
    ustr[dstLen] = 0;
    aRefInUnicode.Assign(ustr, dstLen);
  }

  nsMemory::Free(ustr);

  return rv;
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

  ScrollToRef();
}

void
HTMLContentSink::ScrollToRef()
{
  // XXX Duplicate code in nsXMLContentSink.
  // XXX Be sure to change both places if you make changes here.
  if (mRef.IsEmpty()) {
    return;
  }

  char* tmpstr = ToNewCString(mRef);
  if (!tmpstr) {
    return;
  }

  nsUnescape(tmpstr);
  nsCAutoString unescapedRef;
  unescapedRef.Assign(tmpstr);
  nsMemory::Free(tmpstr);

  nsresult rv = NS_ERROR_FAILURE;
  // We assume that the bytes are in UTF-8, as it says in the spec:
  // http://www.w3.org/TR/html4/appendix/notes.html#h-B.2.1
  nsAutoString ref = NS_ConvertUTF8toUCS2(unescapedRef);

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell;
    mDocument->GetShellAt(i, getter_AddRefs(shell));
    if (shell) {
      // Scroll to the anchor
      shell->FlushPendingNotifications(PR_FALSE);

      // Check an empty string which might be caused by the UTF-8 conversion
      if (!ref.IsEmpty()) {
        rv = shell->GoToAnchor(ref);
      } else {
        rv = NS_ERROR_FAILURE;
      }

      // If UTF-8 URL failed then try to assume the string as a
      // document's charset.

      if (NS_FAILED(rv)) {
        nsAutoString docCharset;
        rv = mDocument->GetDocumentCharacterSet(docCharset);

        if (NS_SUCCEEDED(rv)) {
          rv = CharsetConvRef(docCharset, unescapedRef, ref);

          if (NS_SUCCEEDED(rv) && !ref.IsEmpty())
            rv = shell->GoToAnchor(ref);
        }
      }

      if (NS_SUCCEEDED(rv)) {
        mScrolledToRefAlready = PR_TRUE;
      }
    }
  }
}

void
HTMLContentSink::AddBaseTagInfo(nsIHTMLContent* aContent)
{
  if (!mBaseHREF.IsEmpty()) {
    aContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::_baseHref, mBaseHREF,
                      PR_FALSE);
  }

  if (!mBaseTarget.IsEmpty()) {
    aContent->SetAttr(kNameSpaceID_None, nsHTMLAtoms::_baseTarget,
                      mBaseTarget, PR_FALSE);
  }
}

nsresult
HTMLContentSink::ProcessATag(const nsIParserNode& aNode,
                             nsIHTMLContent* aContent)
{
  AddBaseTagInfo(aContent);

  return NS_OK;
}

nsresult
HTMLContentSink::ProcessAREATag(const nsIParserNode& aNode)
{
  if (!mCurrentMap) {
    return NS_OK;
  }

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

  nsCOMPtr<nsIHTMLContent> area;
  nsresult rv = CreateContentObject(aNode, nodeType, nsnull, nsnull,
                                    getter_AddRefs(area));
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Set the content's document and attributes
  area->SetDocument(mDocument, PR_FALSE, PR_TRUE);
  rv = AddAttributes(aNode, area);
  NS_ENSURE_SUCCESS(rv, rv);

  AddBaseTagInfo(area); // basehref or basetarget. Fix. Bug: 30617

  // Add AREA object to the current map
  mCurrentMap->AppendChildTo(area, PR_FALSE, PR_FALSE);

  return NS_OK;
}

void
HTMLContentSink::ProcessBaseHref(const nsAString& aBaseHref)
{
  //-- Make sure this page is allowed to load this URL
  nsresult rv;
  nsCOMPtr<nsIURI> baseHrefURI;
  rv = NS_NewURI(getter_AddRefs(baseHrefURI), aBaseHref, nsnull);
  if (NS_FAILED(rv)) return;

  // Setting "BASE URL" from the last BASE tag appearing in HEAD.
  if (!mBody) {
    // The document checks if it is legal to set this base
    rv = mDocument->SetBaseURL(baseHrefURI);

    if (NS_SUCCEEDED(rv)) {
      NS_RELEASE(mDocumentBaseURL);
      mDocument->GetBaseURL(mDocumentBaseURL);
    }
  } else {
    // NAV compatibility quirk

    nsCOMPtr<nsIScriptSecurityManager> securityManager =
      do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return;
    }

    rv = securityManager->CheckLoadURI(mDocumentBaseURL, baseHrefURI,
                                       nsIScriptSecurityManager::STANDARD);
    if (NS_FAILED(rv)) {
      return;
    }

    mBaseHREF = aBaseHref;
  }
}

nsresult
HTMLContentSink::RefreshIfEnabled(nsIViewManager* vm)
{
  if (!vm) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));

  if (contentViewer) {
    PRBool enabled;
    contentViewer->GetEnableRendering(&enabled);
    if (enabled) {
      vm->EnableRefresh(NS_VMREFRESH_NO_SYNC);
    }
  }

  return NS_OK;
}

void
HTMLContentSink::ProcessBaseTarget(const nsAString& aBaseTarget)
{
  if (!mBody) {
    // still in real HEAD
    mDocument->SetBaseTarget(aBaseTarget);
  } else {
    // NAV compatibility quirk
    mBaseTarget = aBaseTarget;
  }
}

nsresult
HTMLContentSink::ProcessBASETag(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsIHTMLContent* parent = nsnull;

  if (mCurrentContext) {
    parent = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  }

  if (parent) {
    // Create content object
    nsCOMPtr<nsIHTMLContent> element;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    mNodeInfoManager->GetNodeInfo(NS_LITERAL_STRING("base"), nsnull,
                                  kNameSpaceID_None,
                                  *getter_AddRefs(nodeInfo));

    result = NS_CreateHTMLElement(getter_AddRefs(element), nodeInfo, PR_FALSE);
    NS_ENSURE_SUCCESS(result, result);

    PRInt32 id;
    mDocument->GetAndIncrementContentID(&id);
    element->SetContentID(id);

    // Add in the attributes and add the style content object to the
    // head container.
    element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
    result = AddAttributes(aNode, element);
    NS_ENSURE_SUCCESS(result, result);

    parent->AppendChildTo(element, PR_FALSE, PR_FALSE);
    if (!mInsideNoXXXTag) {
      nsAutoString value;
      if (element->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href,
                           value) == NS_CONTENT_ATTR_HAS_VALUE) {
        ProcessBaseHref(value);
      }

      if (element->GetAttr(kNameSpaceID_None, nsHTMLAtoms::target,
                           value) == NS_CONTENT_ATTR_HAS_VALUE) {
        ProcessBaseTarget(value);
      }
    }
  }

  return result;
}


const PRUnichar kSemiCh = PRUnichar(';');
const PRUnichar kCommaCh = PRUnichar(',');
const PRUnichar kEqualsCh = PRUnichar('=');
const PRUnichar kLessThanCh = PRUnichar('<');
const PRUnichar kGreaterThanCh = PRUnichar('>');

nsresult
HTMLContentSink::ProcessLinkHeader(nsIHTMLContent* aElement,
                                   const nsAString& aLinkData)
{
  nsresult result = NS_OK;

  // parse link content and call process style link
  nsAutoString href;
  nsAutoString rel;
  nsAutoString title;
  nsAutoString type;
  nsAutoString media;
  PRBool didBlock = PR_FALSE;

  // copy to work buffer
  nsAutoString stringList(aLinkData);

  // put an extra null at the end
  stringList.Append(kNullCh);

  PRUnichar* start = NS_CONST_CAST(PRUnichar *, stringList.get());
  PRUnichar* end   = start;
  PRUnichar* last  = start;
  PRUnichar  endCh;

  while (*start != kNullCh) {
    // skip leading space
    while ((*start != kNullCh) && nsCRT::IsAsciiSpace(*start)) {
      ++start;
    }

    end = start;
    last = end - 1;

    // look for semicolon or comma
    while (*end != kNullCh && *end != kSemiCh && *end != kCommaCh) {
      PRUnichar ch = *end;

      if (ch == kApostrophe || ch == kQuote || ch == kLessThanCh) {
        // quoted string

        PRUnichar quote = *end;
        if (quote == kLessThanCh) {
          quote = kGreaterThanCh;
        }

        PRUnichar* closeQuote = (end + 1);

        // seek closing quote
        while (*closeQuote != kNullCh && quote != *closeQuote) {
          ++closeQuote;
        }

        if (quote == *closeQuote) {
          // found closer

          // skip to close quote
          end = closeQuote;

          last = end - 1;

          ch = *(end + 1);

          if (ch != kNullCh && ch != kSemiCh && ch != kCommaCh) {
            // end string here
            *(++end) = kNullCh;

            ch = *(end + 1);

            // keep going until semi or comma
            while (ch != kNullCh && ch != kSemiCh && ch != kCommaCh) {
              ++end;

              ch = *end;
            }
          }
        }
      }

      ++end;
      ++last;
    }

    endCh = *end;

    // end string here
    *end = kNullCh;

    if (start < end) {
      if ((*start == kLessThanCh) && (*last == kGreaterThanCh)) {
        *last = kNullCh;

        if (href.IsEmpty()) { // first one wins
          href = (start + 1);
          href.StripWhitespace();
        }
      } else {
        PRUnichar* equals = start;

        while ((*equals != kNullCh) && (*equals != kEqualsCh)) {
          equals++;
        }

        if (*equals != kNullCh) {
          *equals = kNullCh;
          nsAutoString  attr(start);
          attr.StripWhitespace();

          PRUnichar* value = ++equals;
          while (nsCRT::IsAsciiSpace(*value)) {
            value++;
          }

          if (((*value == kApostrophe) || (*value == kQuote)) &&
              (*value == *last)) {
            *last = kNullCh;
            value++;
          }

          if (attr.EqualsIgnoreCase("rel")) {
            if (rel.IsEmpty()) {
              rel = value;
              rel.CompressWhitespace();
            }
          } else if (attr.EqualsIgnoreCase("title")) {
            if (title.IsEmpty()) {
              title = value;
              title.CompressWhitespace();
            }
          } else if (attr.EqualsIgnoreCase("type")) {
            if (type.IsEmpty()) {
              type = value;
              type.StripWhitespace();
            }
          } else if (attr.EqualsIgnoreCase("media")) {
            if (media.IsEmpty()) {
              media = value;

              // HTML4.0 spec is inconsistent, make it case INSENSITIVE
              ToLowerCase(media);
            }
          }
        }
      }
    }

    if (endCh == kCommaCh) {
      // hit a comma, process what we've got so far

      if (!href.IsEmpty() && !rel.IsEmpty()) {
        result = ProcessLink(aElement, href, rel, title, type, media);
        if (result == NS_ERROR_HTMLPARSER_BLOCK) {
          didBlock = PR_TRUE;
        }
      }

      href.Truncate();
      rel.Truncate();
      title.Truncate();
      type.Truncate();
      media.Truncate();
    }

    start = ++end;
  }

  if (!href.IsEmpty() && !rel.IsEmpty()) {
    result = ProcessLink(aElement, href, rel, title, type, media);

    if (NS_SUCCEEDED(result) && didBlock) {
      result = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }

  return result;
}

nsresult
HTMLContentSink::ProcessLink(nsIHTMLContent* aElement,
                             const nsString& aHref, const nsString& aRel,
                             const nsString& aTitle, const nsString& aType,
                             const nsString& aMedia)
{
  nsresult result = NS_OK;

  // XXX seems overkill to generate this string array
  nsStringArray linkTypes;
  nsStyleLinkElement::ParseLinkTypes(aRel, linkTypes);

  // prefetch href if relation is "next" or "prefetch"
  if (linkTypes.IndexOf(NS_LITERAL_STRING("next")) != -1 ||
      linkTypes.IndexOf(NS_LITERAL_STRING("prefetch")) != -1) {
    PrefetchHref(aHref);
  }

  // is it a stylesheet link?
  if (linkTypes.IndexOf(NS_LITERAL_STRING("stylesheet")) != -1) {
    result = ProcessStyleLink(aElement, aHref, linkTypes, aTitle, aType,
                              aMedia);
  }

  return result;
}

nsresult
HTMLContentSink::ProcessStyleLink(nsIHTMLContent* aElement,
                                  const nsString& aHref,
                                  const nsStringArray& aLinkTypes,
                                  const nsString& aTitle,
                                  const nsString& aType,
                                  const nsString& aMedia)
{
  nsresult result = NS_OK;
  PRBool isAlternate = PR_FALSE;

  // if alternate, does it have title?
  if (aLinkTypes.IndexOf(NS_LITERAL_STRING("alternate")) != -1) {
    if (aTitle.IsEmpty()) { // alternates must have title
      // return without error, for now

      return NS_OK;
    }

    isAlternate = PR_TRUE;
  }

  nsAutoString  mimeType;
  nsAutoString  params;
  nsParserUtils::SplitMimeType(aType, mimeType, params);

  // see bug 18817
  if (!mimeType.IsEmpty() && !mimeType.EqualsIgnoreCase("text/css")) {
    // Unknown stylesheet language
    return NS_OK;
  }

  nsCOMPtr<nsIURI> url;
  result = NS_NewURI(getter_AddRefs(url), aHref, nsnull, mDocumentBaseURL);

  if (NS_FAILED(result)) {
    // The URL is bad, move along, don't propagate the error (for now)

    return NS_OK;
  }

  if (!isAlternate) {
    // possibly preferred sheet

    if (!aTitle.IsEmpty()) {
      nsAutoString preferredStyle;
      mDocument->GetHeaderData(nsHTMLAtoms::headerDefaultStyle,
                               preferredStyle);
      if (preferredStyle.IsEmpty()) {
        mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, aTitle);
      }
    }
  }

  PRBool blockParser = kBlockByDefault;
  if (isAlternate) {
    blockParser = PR_FALSE;
  }

  // NOTE: no longer honoring the important keyword to indicate
  // blocking as it is proprietary and unnecessary since all
  // non-alternate will block the parser now -mja
#if 0
  if (linkTypes.IndexOf("important") != -1) {
    blockParser = PR_TRUE;
  }
#endif

  PRBool doneLoading;
  result = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia,
                                     kNameSpaceID_Unknown,
                                     ((blockParser) ? mParser : nsnull),
                                     doneLoading, 
                                     this);

  if (NS_SUCCEEDED(result) && blockParser && !doneLoading) {
    result = NS_ERROR_HTMLPARSER_BLOCK;
  }

  return result;
}

void
HTMLContentSink::PrefetchHref(const nsAString &aHref)
{
  //
  // SECURITY CHECK: disable prefetching from mailnews!
  //
  // walk up the docshell tree to see if any containing
  // docshell are of type MAIL.
  //
  nsCOMPtr<nsIDocShell> docshell(do_QueryInterface(mWebShell));
  if (!docshell)
    return;
  nsCOMPtr<nsIDocShellTreeItem> treeItem, parentItem;
  do {
    PRUint32 appType;
    nsresult rv = docshell->GetAppType(&appType);
    if (NS_FAILED(rv) || appType == nsIDocShell::APP_TYPE_MAIL)
      return; // do not prefetch from mailnews
    if (treeItem = do_QueryInterface(docshell)) {
      treeItem->GetParent(getter_AddRefs(parentItem));
      if (parentItem) {
        treeItem = parentItem;
        docshell = do_QueryInterface(treeItem);
        if (!docshell) {
          NS_ERROR("cannot get a docshell from a treeItem!");
          return;
        }
      }
    }
  } while (parentItem);
  
  // OK, we passed the security check...
  
  nsCOMPtr<nsIPrefetchService> prefetchService(
          do_GetService(NS_PREFETCHSERVICE_CONTRACTID));
  if (prefetchService) {
    // construct URI using document charset
    nsAutoString charset;
    mDocument->GetDocumentCharacterSet(charset);
    nsCOMPtr<nsIURI> uri;
    NS_NewURI(getter_AddRefs(uri), aHref,
            charset.IsEmpty() ? nsnull
                              : NS_LossyConvertUCS2toASCII(charset).get(),
            mDocumentBaseURL);
    if (uri)
      prefetchService->PrefetchURI(uri, mDocumentURI);
  }
}

nsresult
HTMLContentSink::ProcessLINKTag(const nsIParserNode& aNode)
{
  nsresult  result = NS_OK;
  nsIHTMLContent* parent = nsnull;

  if (mCurrentContext) {
    parent = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  }

  if (parent) {
    // Create content object
    nsCOMPtr<nsIHTMLContent> element;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::link, nsnull, kNameSpaceID_None,
                                  *getter_AddRefs(nodeInfo));

    result = NS_CreateHTMLElement(getter_AddRefs(element), nodeInfo, PR_FALSE);
    NS_ENSURE_SUCCESS(result, result);

    PRInt32 id;
    mDocument->GetAndIncrementContentID(&id);
    element->SetContentID(id);

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
    element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
    result = AddAttributes(aNode, element);
    if (NS_FAILED(result)) {
      return result;
    }
    parent->AppendChildTo(element, PR_FALSE, PR_FALSE);

    if (ssle) {
      ssle->SetEnableUpdates(PR_TRUE);
      result = ssle->UpdateStyleSheet(nsnull);

      // look for <link rel="next" href="url">
      nsAutoString relVal;
      element->GetAttr(kNameSpaceID_None, nsHTMLAtoms::rel, relVal);
      if (!relVal.IsEmpty()) {
        // XXX seems overkill to generate this string array
        nsStringArray linkTypes;
        nsStyleLinkElement::ParseLinkTypes(relVal, linkTypes);
        if (linkTypes.IndexOf(NS_LITERAL_STRING("next")) != -1 ||
            linkTypes.IndexOf(NS_LITERAL_STRING("prefetch")) != -1) {
          nsAutoString hrefVal;
          element->GetAttr(kNameSpaceID_None, nsHTMLAtoms::href, hrefVal);
          if (!hrefVal.IsEmpty()) {
            PrefetchHref(hrefVal);
          }
        }
      }
    }
  }

  return result;
}

nsresult
HTMLContentSink::ProcessMAPTag(const nsIParserNode& aNode,
                               nsIHTMLContent* aContent)
{
  mCurrentMap = nsnull;

  nsCOMPtr<nsIDOMHTMLMapElement> domMap(do_QueryInterface(aContent));
  if (!domMap) {
    return NS_ERROR_UNEXPECTED;
  }

  // We used to strip whitespace from the NAME attribute here, to
  // match a 4.x quirk, but it proved too quirky for us, and IE never
  // did that.  See bug 79738 for details.

  // This is for nav4 compatibility. (Bug 18478) The base tag should
  // only appear in the head, but nav4 allows the base tag in the body
  // as well.
  AddBaseTagInfo(aContent);

  // Don't need to add the map to the document here anymore.
  // The map adds itself

  mCurrentMap = aContent;

  return NS_OK;
}

nsresult
HTMLContentSink::ProcessMETATag(const nsIParserNode& aNode)
{
  nsIHTMLContent* parent = nsnull;

  if (mCurrentContext) {
    parent = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  }

  if (!parent) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  // Create content object
  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(NS_LITERAL_STRING("meta"), nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHTMLContent> it;
  rv = NS_NewHTMLMetaElement(getter_AddRefs(it), nodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  // Add in the attributes and add the meta content object to the head
  // container.
  it->SetDocument(mDocument, PR_FALSE, PR_TRUE);
  rv = AddAttributes(aNode, it);
  if (NS_FAILED(rv)) {
    return rv;
  }

  parent->AppendChildTo(it, PR_FALSE, PR_FALSE);

  // XXX It's just not sufficient to check if the parent is head. Also
  // check for the preference.
  if (!mInsideNoXXXTag) {
    // Bug 40072: Don't evaluate METAs after FRAMESET.

    if (!mFrameset) {
      // set any HTTP-EQUIV data into document's header data as well as url
      nsAutoString header;
      it->GetAttr(kNameSpaceID_None, nsHTMLAtoms::httpEquiv, header);

      if (!header.IsEmpty()) {
        nsAutoString result;
        it->GetAttr(kNameSpaceID_None, nsHTMLAtoms::content, result);

        if (!result.IsEmpty()) {
          ToLowerCase(header);
          nsCOMPtr<nsIAtom> fieldAtom(dont_AddRef(NS_NewAtom(header)));
          rv = ProcessHeaderData(fieldAtom, result, it);
        }
      }
    }
  }

  return rv;
}

nsresult
HTMLContentSink::ProcessHTTPHeaders(nsIChannel* aChannel)
{
  NS_ASSERTION(aChannel, "can't process http headers without a channel");

  nsCOMPtr<nsIHttpChannel> httpchannel(do_QueryInterface(aChannel));

  if (!httpchannel) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  const char *const headers[] = {
    "link",
    "default-style",
    "content-style-type",
    // add more http headers if you need
    0
  };

  const char *const *name = headers;
  nsCAutoString tmp;

  while (*name) {
    rv = httpchannel->GetResponseHeader(nsDependentCString(*name), tmp);
    if (NS_SUCCEEDED(rv) && !tmp.IsEmpty()) {
      nsCOMPtr<nsIAtom> key(dont_AddRef(NS_NewAtom(*name)));
      ProcessHeaderData(key, NS_ConvertASCIItoUCS2(tmp));
    }
    name++;
  }

  return rv;
}

nsresult
HTMLContentSink::ProcessHeaderData(nsIAtom* aHeader, const nsAString& aValue,
                                   nsIHTMLContent* aContent)
{
  nsresult rv = NS_OK;
  // XXX necko isn't going to process headers coming in from the
  // parser

  mDocument->SetHeaderData(aHeader, aValue);

  // see if we have a refresh "header".
  if (aHeader == nsHTMLAtoms::refresh) {
    // first get our baseURI
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(mWebShell, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIURI> baseURI;
    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(docShell);
    rv = webNav->GetCurrentURI(getter_AddRefs(baseURI));
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsIRefreshURI> reefer = do_QueryInterface(mWebShell);
    if (reefer) {
      rv = reefer->SetupRefreshURIFromHeader(baseURI,
                                             NS_ConvertUCS2toUTF8(aValue));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  } else if (aHeader == nsHTMLAtoms::setcookie) {
    nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(mWebShell, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    nsCOMPtr<nsICookieService> cookieServ =
      do_GetService(NS_COOKIESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Get a URI from the document principal

    // We use the original codebase in case the codebase was changed
    // by SetDomain

    nsCOMPtr<nsIPrincipal> docPrincipal;
    rv = mDocument->GetPrincipal(getter_AddRefs(docPrincipal));
    if (NS_FAILED(rv) || !docPrincipal) {
      return rv;
    }

    nsCOMPtr<nsIAggregatePrincipal> agg(do_QueryInterface(docPrincipal, &rv));
    // Document principal should always be an aggregate
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPrincipal> originalPrincipal;
    rv = agg->GetOriginalCodebase(getter_AddRefs(originalPrincipal));
    nsCOMPtr<nsICodebasePrincipal> originalCodebase =
      do_QueryInterface(originalPrincipal, &rv);
    if (NS_FAILED(rv)) {
      // Document's principal is not a codebase (may be system), so
      // can't set cookies

      return NS_OK;
    }

    nsCOMPtr<nsIURI> codebaseURI;
    rv = originalCodebase->GetURI(getter_AddRefs(codebaseURI));
    NS_ENSURE_SUCCESS(rv, rv);

    char *cookie = ToNewUTF8String(aValue);
    nsCOMPtr<nsIScriptGlobalObject> globalObj;
    nsCOMPtr<nsIPrompt> prompt;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
    if (globalObj) {
      nsCOMPtr<nsIDOMWindowInternal> window (do_QueryInterface(globalObj));
      if (window) {
        window->GetPrompter(getter_AddRefs(prompt));
      }
    }

    nsCOMPtr<nsIHttpChannel> httpChannel;
    if (mParser) {
      nsCOMPtr<nsIChannel> channel;
      if (NS_SUCCEEDED(mParser->GetChannel(getter_AddRefs(channel)))) {
        httpChannel = do_QueryInterface(channel);
      }
    }

    rv = cookieServ->SetCookieString(codebaseURI, prompt, cookie, httpChannel);
    nsCRT::free(cookie);

    if (NS_FAILED(rv)) {
      return rv;
    }
  } else if (aHeader == nsHTMLAtoms::link) {
    rv = ProcessLinkHeader(aContent, aValue);
  }
  else if (aHeader == nsHTMLAtoms::msthemecompatible) {
    // Disable theming for the presshell if the value is no.
    nsAutoString value(aValue);
    if (value.EqualsIgnoreCase("no")) {
      nsCOMPtr<nsIPresShell> shell;
      mDocument->GetShellAt(0, getter_AddRefs(shell));
      if (shell)
        shell->DisableThemeSupport();
    }
  }
  else if (mParser) {
    // we also need to report back HTTP-EQUIV headers to the channel
    // so that it can process things like pragma: no-cache or other
    // cache-control headers. Ideally this should also be the way for
    // cookies to be set! But we'll worry about that in the next
    // iteration
    nsCOMPtr<nsIChannel> channel;
    if (NS_SUCCEEDED(mParser->GetChannel(getter_AddRefs(channel)))) {
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
      if (httpChannel) {
        const PRUnichar *header = 0;
        (void)aHeader->GetUnicode(&header);
        (void)httpChannel->SetResponseHeader(
                       NS_ConvertUCS2toUTF8(header),
                       NS_ConvertUCS2toUTF8(aValue),
                       PR_TRUE);
      }
    }
  }

  return rv;
}

#ifdef DEBUG
void
HTMLContentSink::ForceReflow()
{
  mCurrentContext->FlushTags(PR_TRUE);
}
#endif

void
HTMLContentSink::NotifyAppend(nsIContent* aContainer, PRInt32 aStartIndex)
{
  mInNotification++;

  MOZ_TIMER_DEBUGLOG(("Save and stop: nsHTMLContentSink::NotifyAppend()\n"));
  MOZ_TIMER_SAVE(mWatch)
  MOZ_TIMER_STOP(mWatch);

  mDocument->ContentAppended(aContainer, aStartIndex);
  mLastNotificationTime = PR_Now();

  MOZ_TIMER_DEBUGLOG(("Restore: nsHTMLContentSink::NotifyAppend()\n"));
  MOZ_TIMER_RESTORE(mWatch);

  mInNotification--;
}

void
HTMLContentSink::NotifyInsert(nsIContent* aContent,
                              nsIContent* aChildContent,
                              PRInt32 aIndexInContainer)
{
  mInNotification++;

  MOZ_TIMER_DEBUGLOG(("Save and stop: nsHTMLContentSink::NotifyInsert()\n"));
  MOZ_TIMER_SAVE(mWatch)
  MOZ_TIMER_STOP(mWatch);

  mDocument->ContentInserted(aContent, aChildContent, aIndexInContainer);
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

PRBool
HTMLContentSink::IsTimeToNotify()
{
  if (!mNotifyOnTimer || !mLayoutStarted || !mBackoffCount ||
      mInMonolithicContainer) {
    return PR_FALSE;
  }

  PRTime now = PR_Now();
  PRInt64 interval, diff;

  LL_I2L(interval, GetNotificationInterval());
  LL_SUB(diff, now, mLastNotificationTime);

  if (LL_CMP(diff, >, interval)) {
    mBackoffCount--;
    return PR_TRUE;
  }

  return PR_FALSE;
}

void
HTMLContentSink::UpdateAllContexts()
{
  PRInt32 numContexts = mContextStack.Count();
  for (PRInt32 i = 0; i < numContexts; i++) {
    SinkContext* sc = (SinkContext*)mContextStack.ElementAt(i);

    sc->UpdateChildCounts();
  }

  mCurrentContext->UpdateChildCounts();
}

NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(HTMLContentSink)
NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(HTMLContentSink)
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(HTMLContentSink)
NS_IMPL_NSIDOCUMENTOBSERVER_CONTENT(HTMLContentSink)

NS_IMETHODIMP
HTMLContentSink::BeginUpdate(nsIDocument *aDocument)
{
  nsresult result = NS_OK;
  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Since this could result in frame
  // creation, make sure we've flushed everything before we
  // continue
  if (mInScript && !mInNotification && mCurrentContext) {
    result = mCurrentContext->FlushTags(PR_TRUE);
  }

  return result;
}

NS_IMETHODIMP
HTMLContentSink::EndUpdate(nsIDocument *aDocument)
{

  // If we're in a script and we didn't do the notification,
  // something else in the script processing caused the
  // notification to occur. Update our notion of how much
  // has been flushed to include any new content.
  if (mInScript && !mInNotification) {
    UpdateAllContexts();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::StyleSheetAdded(nsIDocument *aDocument,
                                 nsIStyleSheet* aStyleSheet)
{
  // We only care when applicable sheets are added
  NS_PRECONDITION(aStyleSheet, "Must have a style sheet!");
  PRBool applicable;
  aStyleSheet->GetApplicable(applicable);
  if (applicable) {
    // Processing of a new style sheet causes recreation of the frame
    // model. As a result, all contexts should update their notion of
    // how much frame creation has happened.
    UpdateAllContexts();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::StyleSheetRemoved(nsIDocument *aDocument,
                                   nsIStyleSheet* aStyleSheet)
{
  // We only care when applicable sheets are removed
  NS_PRECONDITION(aStyleSheet, "Must have a style sheet!");
  PRBool applicable;
  aStyleSheet->GetApplicable(applicable);
  if (applicable) {
    // Removing a style sheet causes recreation of the frame model.
    // As a result, all contexts should update their notion of how
    // much frame creation has happened.
    UpdateAllContexts();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::StyleSheetApplicableStateChanged(nsIDocument *aDocument,
                                                  nsIStyleSheet* aStyleSheet,
                                                  PRBool aApplicable)
{
  // Changing a style sheet's applicable state causes recreation of
  // the frame model. As a result, all contexts should update their
  // notion of how much frame creation has happened.
  UpdateAllContexts();

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::StyleRuleChanged(nsIDocument *aDocument,
                                  nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule, nsChangeHint aHint)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::StyleRuleAdded(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::StyleRuleRemoved(nsIDocument *aDocument,
                                  nsIStyleSheet* aStyleSheet,
                                  nsIStyleRule* aStyleRule)
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

nsresult
HTMLContentSink::ResumeParsing()
{
  nsresult result = NS_OK;
  if (mParser) {
    result = mParser->ContinueParsing();
  }

  return result;
}

PRBool
HTMLContentSink::PreEvaluateScript()
{
  // Eagerly append all pending elements (including the current body child)
  // to the body (so that they can be seen by scripts) and force reflow.
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::PreEvaluateScript: flushing tags before "
              "evaluating script"));

  mCurrentContext->FlushTags(PR_FALSE);
  mCurrentContext->SetPreAppend(PR_TRUE);

  mInScript++;

  return PR_TRUE;
}

void
HTMLContentSink::PostEvaluateScript()
{
  mInScript--;

  mCurrentContext->SetPreAppend(PR_FALSE);
}

PRBool
HTMLContentSink::IsInScript()
{
  return mInScript > 0;
}

NS_IMETHODIMP
HTMLContentSink::ScriptAvailable(nsresult aResult,
                                 nsIDOMHTMLScriptElement *aElement,
                                 PRBool aIsInline,
                                 PRBool aWasPending,
                                 nsIURI *aURI,
                                 PRInt32 aLineNo,
                                 const nsAString& aScript)
{
  // Check if this is the element we were waiting for
  PRUint32 count;
  mScriptElements->Count(&count);

  nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement =
    do_QueryElementAt(mScriptElements, count - 1);
  if (aElement != scriptElement) {
    return NS_OK;
  }

  if (mParser && !mParser->IsParserEnabled()) {
    // make sure to unblock the parser before evaluating the script,
    // we must unblock the parser even if loading the script failed or
    // if the script was empty, if we don't, the parser will never be
    // unblocked.
    mParser->UnblockParser();
  }

  // Mark the current script as loaded
  mNeedToBlockParser = PR_FALSE;

  if (NS_SUCCEEDED(aResult)) {
    PreEvaluateScript();
  } else {
    mScriptElements->RemoveElementAt(count - 1);

    if (mParser && aWasPending) {
      // Loading external script failed!. So, resume
      // parsing since the parser got blocked when loading
      // external script. - Ref. Bug: 94903
      mParser->ContinueParsing();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::ScriptEvaluated(nsresult aResult,
                                 nsIDOMHTMLScriptElement *aElement,
                                 PRBool aIsInline,
                                 PRBool aWasPending)
{
  // Check if this is the element we were waiting for
  PRUint32 count;
  mScriptElements->Count(&count);

  nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement =
    do_QueryElementAt(mScriptElements, count - 1);
  if (aElement != scriptElement) {
    return NS_OK;
  }

  // Pop the script element stack
  mScriptElements->RemoveElementAt(count - 1);

  if (NS_SUCCEEDED(aResult)) {
    PostEvaluateScript();
  }

  if (mParser && mParser->IsParserEnabled() && aWasPending) {
    mParser->ContinueParsing();
  }

  return NS_OK;
}

nsresult
HTMLContentSink::ProcessSCRIPTTag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;

  // Create content object
  NS_ASSERTION(mCurrentContext->mStackPos > 0, "leaf w/o container");
  if (mCurrentContext->mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }
  nsIHTMLContent* parent =
    mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  nsCOMPtr<nsIHTMLContent> element;
  nsCOMPtr<nsINodeInfo> nodeInfo;
  mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::script, nsnull, kNameSpaceID_None,
                                *getter_AddRefs(nodeInfo));

  rv = NS_CreateHTMLElement(getter_AddRefs(element), nodeInfo, PR_FALSE);
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 id;
  mDocument->GetAndIncrementContentID(&id);
  element->SetContentID(id);

  // Add in the attributes and add the style content object to the
  // head container.
  element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
  rv = AddAttributes(aNode, element);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Determine whether the script is really an event handler.
  // Event handler scripts are NOT immediately evaluated.
  nsCOMPtr<nsIDOMHTMLScriptElement> scriptElement = do_QueryInterface(element);
  PRBool bIsEventHandler = PR_FALSE;
  nsAutoString forString;

  scriptElement->GetHtmlFor(forString);
  if (!forString.IsEmpty()) {
    bIsEventHandler = PR_TRUE;
  }

  nsCOMPtr<nsIDTD> dtd;
  mParser->GetDTD(getter_AddRefs(dtd));
  NS_ENSURE_TRUE(dtd, NS_ERROR_FAILURE);

  nsCOMPtr<nsIScriptElement> sele(do_QueryInterface(element));
  nsAutoString script;
  PRInt32 lineNo = 0;

  dtd->CollectSkippedContent(eHTMLTag_script, script, lineNo);

  if (sele) {
    sele->SetLineNumber((PRUint32)lineNo);
  }

  // Create a text node holding the content. First, get the text
  // content of the script tag

  if (!script.IsEmpty()) {
    nsCOMPtr<nsITextContent> text;
    rv = NS_NewTextNode(getter_AddRefs(text));
    NS_ENSURE_SUCCESS(rv, rv);

    text->SetText(script, PR_TRUE);

    element->AppendChildTo(text, PR_FALSE, PR_FALSE);
    text->SetDocument(mDocument, PR_FALSE, PR_TRUE);
  }

  nsCOMPtr<nsIScriptLoader> loader;
  if (mFrameset) {
    // Fix bug 82498
    // We don't want to evaluate scripts in a frameset document.
    if (mDocument) {
      mDocument->GetScriptLoader(getter_AddRefs(loader));
      if (loader) {
        loader->SetEnabled(PR_FALSE);
      }
    }
  } else {
    // Don't include script loading and evaluation in the stopwatch
    // that is measuring content creation time
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::ProcessSCRIPTTag()\n"));
    MOZ_TIMER_STOP(mWatch);

    // Assume that we're going to block the parser with a script load.
    // If it's an inline script, we'll be told otherwise in the call
    // to our ScriptAvailable method.
    //
    // However, if this script is an event handler, then DO NOT block the
    // parser because it will NOT be executed now.
    mNeedToBlockParser = !bIsEventHandler;

    mScriptElements->AppendElement(scriptElement);
  }

  // Insert the child into the content tree. This will evaluate the
  // script as well.
  if (mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mInsertionPoint != -1) {
    parent->InsertChildAt(element,
                          mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mInsertionPoint++,
                          PR_FALSE, PR_FALSE);
  } else {
    parent->AppendChildTo(element, PR_FALSE, PR_FALSE);
  }

  // To prevent script evaluation, in a frameset document, we
  // suspended the script loader. Now that the script content
  // has been handled let's resume the script loader.
  if (loader) {
    loader->SetEnabled(PR_TRUE);
  }

  // If the act of insertion evaluated the script, we're fine.
  // Else, block the parser till the script has loaded.
  if (mNeedToBlockParser || (mParser && !mParser->IsParserEnabled())) {
    return NS_ERROR_HTMLPARSER_BLOCK;
  }

  return NS_OK;
}

// 3 ways to load a style sheet: inline, style src=, link tag
// XXX What does nav do if we have SRC= and some style data inline?

nsresult
HTMLContentSink::ProcessSTYLETag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  nsIHTMLContent* parent = nsnull;

  if (mCurrentContext) {
    parent = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  }

  if (!parent) {
    return NS_OK;
  }

  // Create content object
  nsCOMPtr<nsINodeInfo> nodeInfo;
  mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::style, nsnull, kNameSpaceID_None,
                                *getter_AddRefs(nodeInfo));

  nsCOMPtr<nsIHTMLContent> element;
  rv = NS_CreateHTMLElement(getter_AddRefs(element), nodeInfo, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 id;
  mDocument->GetAndIncrementContentID(&id);
  element->SetContentID(id);

  nsCOMPtr<nsIStyleSheetLinkingElement> ssle = do_QueryInterface(element);

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
  element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
  rv = AddAttributes(aNode, element);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // The skipped content contains the inline style data
  nsCOMPtr<nsIDTD> dtd;
  mParser->GetDTD(getter_AddRefs(dtd));
  NS_ENSURE_TRUE(dtd, NS_ERROR_FAILURE);

  nsAutoString content;
  PRInt32 lineNo = 0;

  dtd->CollectSkippedContent(eHTMLTag_style, content, lineNo);

  if (!content.IsEmpty()) {
    // Create a text node holding the content
    nsCOMPtr<nsITextContent> text;
    rv = NS_NewTextNode(getter_AddRefs(text));
    NS_ENSURE_SUCCESS(rv, rv);

    text->SetText(content, PR_TRUE);

    element->AppendChildTo(text, PR_FALSE, PR_FALSE);
  }

  parent->AppendChildTo(element, PR_FALSE, PR_FALSE);

  if (ssle) {
    ssle->SetEnableUpdates(PR_TRUE);
    rv = ssle->UpdateStyleSheet(nsnull);
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::FlushPendingNotifications()
{
  nsresult result = NS_OK;
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant) and if we're in a script (since we
  // only care to flush if this is done via script).
  //
  // Bug 4891: Also flush outside of <script> tags - if script is
  // executing after the page has finished loading, and a
  // document.write occurs, we might need this flush.
  if (mCurrentContext && !mInNotification) {
    result = mCurrentContext->FlushTags();
  }

  return result;
}

NS_IMETHODIMP
HTMLContentSink::SetDocumentCharset(nsAString& aCharset)
{
  if (mWebShell) {
    // the following logic to get muCV is copied from
    // nsHTMLDocument::StartDocumentLoad
    // We need to call muCV->SetPrevDocCharacterSet here in case
    // the charset is detected by parser DetectMetaTag
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
    nsCOMPtr<nsIMarkupDocumentViewer> muCV;
    nsCOMPtr<nsIContentViewer> cv;
    docShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
       muCV = do_QueryInterface(cv);
    } else {
      // in this block of code, if we get an error result, we return
      // it but if we get a null pointer, that's perfectly legal for
      // parent and parentContentViewer

      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
        do_QueryInterface(docShell);
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
      muCV->SetPrevDocCharacterSet(PromiseFlatString(aCharset).get());
    }
  }

  if (mDocument) {
    return mDocument->SetDocumentCharacterSet(aCharset);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DoFragment(PRBool aFlag)
{
  return NS_OK;
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
  nsresult result = NS_OK;
  FILE* out = ::fopen("rtest_html.txt", "a");
  if (out) {
    if (mDocument) {
      nsIContent* root = nsnull;
      mDocument->GetRootContent(&root);
      if (root) {
        if (mDocumentURI) {
          nsCAutoString buf;
          mDocumentURI->GetSpec(buf);
          fputs(buf.get(), out);
        }

        fputs(";", out);
        result = root->DumpContent(out, 0, PR_FALSE);
        fputs(";\n", out);
        NS_RELEASE(root);
      }
    }

    fclose(out);
  }

  return result;
}
#endif

// If the content sink can interrupt the parser (@see mCanInteruptParsing)
// then it needs to schedule a dummy parser request to delay the document
// from firing onload handlers and other document done actions until all of the
// parsing has completed.

nsresult
HTMLContentSink::AddDummyParserRequest(void)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(!mDummyParserRequest, "Already have a dummy parser request");

  rv = DummyParserRequest::Create(getter_AddRefs(mDummyParserRequest), this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    rv = mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (loadGroup) {
    rv = mDummyParserRequest->SetLoadGroup(loadGroup);
    if (NS_FAILED(rv)) {
      return rv;
    }

    rv = loadGroup->AddRequest(mDummyParserRequest, nsnull);
  }

  return rv;
}

nsresult
HTMLContentSink::RemoveDummyParserRequest(void)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  if (mDocument) {
    rv = mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
  }

  if (loadGroup && mDummyParserRequest) {
    rv = loadGroup->RemoveRequest(mDummyParserRequest, nsnull, NS_OK);
    if (NS_FAILED(rv)) {
      return rv;
    }

    mDummyParserRequest = nsnull;
  }

  return rv;
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIHTMLContentSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIParser.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsIURL.h"
#include "nsIStreamLoader.h"
#include "nsNetUtil.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIContentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsINodeInfo.h"
#include "nsHTMLTokens.h"  
#include "nsHTMLEntities.h" 
#include "nsCRT.h"
#include "jsapi.h" // for JSVERSION_* and JS_VersionToString
#include "prtime.h"
#include "prlog.h"

#include "nsHTMLParts.h"
#include "nsIElementFactory.h"
#include "nsITextContent.h"

#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentType.h"

#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIFormControl.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsIScrollableView.h"
#include "nsHTMLAtoms.h"
#include "nsIFrame.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsICharsetAlias.h"
#include "nsIChannel.h"
#include "nsIHTTPChannel.h"

#include "nsIWebShell.h"
#include "nsIDocShell.h"
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
#include "nsHTMLIIDs.h"
#include "nsTextFragment.h"
#include "nsIScriptGlobalObject.h"

#include "nsIParserService.h"
#include "nsParserCIID.h"
#include "nsISelectElement.h"

#include "nsIPref.h"

// XXX Go through a factory for this one
#include "nsICSSParser.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsIDOMHTMLTitleElement.h"
#include "nsTimer.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsDOMError.h"
#include "nsIScrollable.h"
#include "nsContentPolicyUtils.h"

#include "nsWeakReference.h"//nshtmlelementfactory supports weak references

#ifdef ALLOW_ASYNCH_STYLE_SHEETS
const PRBool kBlockByDefault=PR_FALSE;
#else
const PRBool kBlockByDefault=PR_TRUE;
#endif

static NS_DEFINE_IID(kIDOMHTMLTitleElementIID, NS_IDOMHTMLTITLEELEMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLMapElementIID, NS_IDOMHTMLMAPELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStyleSheetLinkingElementIID, NS_ISTYLESHEETLINKINGELEMENT_IID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

//----------------------------------------------------------------------

#ifdef NS_DEBUG
static PRLogModuleInfo* gSinkLogModuleInfo;

#define SINK_TRACE_CALLS        0x1
#define SINK_TRACE_REFLOW       0x2
#define SINK_ALWAYS_REFLOW      0x4

#define SINK_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#define SINK_TRACE(_bit,_args)                    \
  PR_BEGIN_MACRO                                  \
    if (SINK_LOG_TEST(gSinkLogModuleInfo,_bit)) { \
      PR_LogPrint _args;                          \
    }                                             \
  PR_END_MACRO

#define SINK_TRACE_NODE(_bit,_msg,_node,_sp,_obj) _obj->SinkTraceNode(_bit,_msg,_node,_sp,this)

#else
#define SINK_TRACE(_bit,_args)
#define SINK_TRACE_NODE(_bit,_msg,_node,_sp,_obj)
#endif

#undef SINK_NO_INCREMENTAL

//----------------------------------------------------------------------

class SinkContext;

class HTMLContentSink : public nsIHTMLContentSink,
                        public nsIStreamLoaderObserver,
                        public nsITimerCallback,
                        public nsICSSLoaderObserver,
                        public nsIDocumentObserver,
                        public nsIDebugDumpContent
{
public:
  HTMLContentSink();
  virtual ~HTMLContentSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsresult Init(nsIDocument* aDoc,
                nsIURI* aURL,
                nsIWebShell* aContainer);

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADEROBSERVER

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD FlushPendingNotifications();
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode=0);

  // nsIHTMLContentSink
  NS_IMETHOD BeginContext(PRInt32 aID);
  NS_IMETHOD EndContext(PRInt32 aID);
  NS_IMETHOD SetTitle(const nsString& aValue);
  NS_IMETHOD OpenHTML(const nsIParserNode& aNode);
  NS_IMETHOD CloseHTML(const nsIParserNode& aNode);
  NS_IMETHOD OpenHead(const nsIParserNode& aNode);
  NS_IMETHOD CloseHead(const nsIParserNode& aNode);
  NS_IMETHOD OpenBody(const nsIParserNode& aNode);
  NS_IMETHOD CloseBody(const nsIParserNode& aNode);
  NS_IMETHOD OpenForm(const nsIParserNode& aNode);
  NS_IMETHOD CloseForm(const nsIParserNode& aNode);
  NS_IMETHOD OpenFrameset(const nsIParserNode& aNode);
  NS_IMETHOD CloseFrameset(const nsIParserNode& aNode);
  NS_IMETHOD OpenMap(const nsIParserNode& aNode);
  NS_IMETHOD CloseMap(const nsIParserNode& aNode);

  NS_IMETHOD DoFragment(PRBool aFlag);

  // nsITimerCallback
  NS_IMETHOD_(void) Notify(nsITimer *timer);
  
  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet*aSheet, PRBool aNotify);

  // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument);
  NS_IMETHOD EndUpdate(nsIDocument *aDocument);
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument,
			                   nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument,
		                   nsIPresShell* aShell) { return NS_OK; } 
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
			                      nsIContent* aContent,
                            nsISupports* aSubContent) { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint) { return NS_OK; }
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer) 
                             { return NS_OK; }
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                        nsIStyleSheet* aStyleSheet,
                                        PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { return NS_OK; }

  // nsIDebugDumpContent
  NS_IMETHOD DumpContentModel();

  PRBool IsTimeToNotify();
  PRBool IsInScript();
  void ReduceEntities(nsString& aString);
  void GetAttributeValueAt(const nsIParserNode& aNode,
                           PRInt32 aIndex,
                           nsString& aResult);
  nsresult AddAttributes(const nsIParserNode& aNode,
                         nsIHTMLContent* aContent,
                         PRBool aNotify = PR_FALSE);
  nsresult CreateContentObject(const nsIParserNode& aNode,
                               nsHTMLTag aNodeType,
                               nsIDOMHTMLFormElement* aForm,
                               nsIWebShell* aWebShell,
                               nsIHTMLContent** aResult);
#ifdef NS_DEBUG
  void SinkTraceNode(PRUint32 aBit,
                     const char* aMsg,
                     const nsIParserNode& aNode,
                     PRInt32 aStackPos,
                     void* aThis);
#endif

  nsIDocument* mDocument;
  nsIHTMLDocument* mHTMLDocument;
  nsINodeInfoManager* mNodeInfoManager;
  nsIURI* mDocumentURI;
  nsIURI* mDocumentBaseURL;
  nsCOMPtr<nsIURI> mScriptURI;
  nsIWebShell* mWebShell;
  nsIParser* mParser;

  PRBool mNotifyOnTimer;           // Do we notify based on time?
  PRInt32 mBackoffCount;           // back off timer notification after count
  PRInt32 mNotificationInterval;   // Notification interval in microseconds
  PRTime mLastNotificationTime;    // Time of last notification
  nsCOMPtr<nsITimer> mNotificationTimer;    // Timer used for notification

  PRInt32 mMaxTextRun;             // The maximum length of a text run

  nsIHTMLContent* mRoot;
  nsIHTMLContent* mBody;
  nsIHTMLContent* mFrameset;
  nsIHTMLContent* mHead;
  nsString* mTitle;
  nsString  mUnicodeXferBuf;
  nsString  mScriptCharset;

  PRBool mLayoutStarted;
  PRInt32 mInScript;
  PRInt32 mInNotification;
  nsIDOMHTMLFormElement* mCurrentForm;
  nsIHTMLContent* mCurrentMap;

  nsVoidArray mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;
  PRInt32 mNumOpenIFRAMES;

  nsString*           mRef;
  nsScrollPreference  mOriginalScrollPreference;
  PRBool              mNotAtRef;
  nsIHTMLContent*     mRefContent;

  nsString            mBaseHREF;
  nsString            mBaseTarget;

  nsString            mPreferredStyle;
  PRInt32             mStyleSheetCount;
  nsICSSLoader*       mCSSLoader;
  PRInt32             mInsideNoXXXTag;
  PRInt32             mInMonolithicContainer;

  void StartLayout();

  void ScrollToRef();

  void AddBaseTagInfo(nsIHTMLContent* aContent);

  nsresult ProcessLink(nsIHTMLContent* aElement, const nsString& aLinkData);
  nsresult ProcessStyleLink(nsIHTMLContent* aElement,
                            const nsString& aHref, const nsString& aRel,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia);

  void ProcessBaseHref(const nsString& aBaseHref);
  void ProcessBaseTarget(const nsString& aBaseTarget);

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

  // Script processing related routines
  nsresult ResumeParsing();
  PRBool   PreEvaluateScript();
  void     PostEvaluateScript(PRBool aBodyPresent);
  nsresult EvaluateScript(nsString& aScript,
                          nsIURI *aScriptURI,
                          PRInt32 aLineNo,
                          const char* aVersion);
  const char* mScriptLanguageVersion;

  void UpdateAllContexts();
  void NotifyAppend(nsIContent* aContent, 
                    PRInt32 aStartIndex);
  void NotifyInsert(nsIContent* aContent,
                    nsIContent* aChildContent,
                    PRInt32 aIndexInContainer);
  PRBool IsMonolithicContainer(nsHTMLTag aTag);
#ifdef NS_DEBUG
  void ForceReflow();
#endif

  MOZ_TIMER_DECLARE(mWatch) //  Measures content model creation time for current document
};

class SinkContext {
public:
  SinkContext(HTMLContentSink* aSink);
  ~SinkContext();

  // Normally when OpenContainer's are done the container is not
  // appended to it's parent until the container is closed. By setting
  // pre-append to true, the container will be appended when it is
  // created.
  void SetPreAppend(PRBool aPreAppend) {
    mPreAppend = aPreAppend;
  }

  nsresult Begin(nsHTMLTag aNodeType, nsIHTMLContent* aRoot, 
                 PRInt32 aNumFlushed, PRInt32 aInsertionPoint);
  nsresult OpenContainer(const nsIParserNode& aNode);
  nsresult CloseContainer(const nsIParserNode& aNode);
  nsresult AddLeaf(const nsIParserNode& aNode);
  nsresult AddLeaf(nsIHTMLContent* aContent);
  nsresult AddComment(const nsIParserNode& aNode);
  nsresult DemoteContainer(const nsIParserNode& aNode);
  nsresult End();

  nsresult GrowStack();
  nsresult AddText(const nsString& aText);
  nsresult FlushText(PRBool* aDidFlush = nsnull, PRBool aReleaseLast = PR_FALSE);  
  nsresult FlushTextAndRelease(PRBool* aDidFlush = nsnull)
  {
    return FlushText(aDidFlush, PR_TRUE);
  }
  nsresult FlushTags(PRBool aNotify = PR_TRUE);

  PRBool   IsCurrentContainer(nsHTMLTag mType);
  PRBool   IsAncestorContainer(nsHTMLTag mType);
  nsIHTMLContent* GetCurrentContainer();

  void DidAddContent(nsIContent* aContent, PRBool aDidNotify=PR_FALSE);
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
  PRInt32 mIgnoredOpenCount;
};

//----------------------------------------------------------------------

#ifdef NS_DEBUG
void
HTMLContentSink::SinkTraceNode(PRUint32 aBit,
                               const char* aMsg,
                               const nsIParserNode& aNode,
                               PRInt32 aStackPos,
                               void* aThis)
{
  if (SINK_LOG_TEST(gSinkLogModuleInfo,aBit)) {
    char cbuf[40];
    const char* cp;
    nsAutoString str;
    PRInt32 nt = aNode.GetNodeType();
    if ((nt > PRInt32(eHTMLTag_unknown)) &&
        (nt < PRInt32(eHTMLTag_text)) && mParser) {
      nsCOMPtr<nsIDTD> dtd;
      mParser->GetDTD(getter_AddRefs(dtd));
      dtd->IntTagToStringTag(nsHTMLTag(aNode.GetNodeType()), str);
      cp = str.ToCString(cbuf, sizeof(cbuf));
    } else {
      aNode.GetText().ToCString(cbuf, sizeof(cbuf));
      cp = cbuf;
    }
    PR_LogPrint("%s: this=%p node='%s' stackPos=%d", aMsg, aThis, cp, aStackPos);
  }
}
#endif

/**
  * Helper to find identifiers that can terminate an entity
  *
  * harishd 06/23/00
  *
  * @param aSource      - Search for entity terminator in this string
  * @param aChar        - Holds the terminated character
  * @param aStartOffset - Beings search, in aSource, from this offset.
  */
 
PRInt32
GetEntityTerminator(nsString& aSource,PRUnichar& aChar,PRInt32 aStartOffset=0) {
 
  PRUnichar         theChar=aChar=0;
  PRInt32           theOffset=aStartOffset;
  PRInt32           theLength=aSource.Length();
  PRBool            found=PR_FALSE; 

  while(theOffset<theLength) {
 
    theChar=aSource[theOffset++];

    if(('a'<=theChar) && (theChar<='z'))
      found=PR_TRUE;
    else if(('A'<=theChar) && (theChar<='Z'))
      found=PR_TRUE;
    else if(('0'<=theChar) && (theChar<='9'))
      found=PR_TRUE;
    else if('#'==theChar) 
      found=PR_TRUE;
    else 
      found=PR_FALSE;

    if(!found) {
      aChar=theChar;
      return theOffset-1;
    }
  }
  return -1;
}


void HTMLContentSink::ReduceEntities(nsString& aString) {
  if (mParser) {
    nsCOMPtr<nsIDTD> dtd;

    nsresult rv = mParser->GetDTD(getter_AddRefs(dtd));

    if (NS_SUCCEEDED(rv)) {

      // XXX Note: as coded today, this will only convert well formed
      // entities.  This may not be compatible enough.
      // XXX there is a table in navigator that translates some numeric entities
      // should we be doing that? If so then it needs to live in two places (bad)
      // so we should add a translate numeric entity method from the parser...

      nsAutoString  theOutString;
      nsAutoString  theNCRStr;

      PRInt32 theLen=aString.Length();
      PRInt32 theAmpPos = aString.FindChar('&');
      PRInt32 theStartPos=0;
      PRInt32 theTermPos=-1;
      PRUnichar theTermChar='\0';
      const PRUnichar *theBuf=aString.GetUnicode();
  
      nsDTDMode mode;
      mHTMLDocument->GetDTDMode(mode);

      while(-1!=theAmpPos) { 

        if(theStartPos<theAmpPos) {
          //we have real data to copy to output string before doing our first conversion          
          theOutString.Append(&theBuf[theStartPos],theAmpPos-theStartPos);
        }

        theTermPos=GetEntityTerminator(aString,theTermChar,theAmpPos+1);
        
        if(-1!=theTermPos) {
          //having found a semi, copy chars between amppos and semipos;
          aString.Mid(theNCRStr,theAmpPos+1,theTermPos-theAmpPos-1);
        }
        else {
          aString.Mid(theNCRStr,theAmpPos+1,theLen-theAmpPos-1);
          PRInt32 theNewAmpPos=aString.FindChar('&',PR_FALSE,theAmpPos+1);
          theTermPos=(-1==theNewAmpPos) ? theLen+1 : theNewAmpPos-1;
        }

        theStartPos=theTermPos+1;

        PRUnichar theChar=(theLen>theAmpPos+1) ? aString.CharAt(theAmpPos+1) : '\0';
        PRUnichar theEntity=0;
        PRInt32   theErr=0;
        PRInt32   theNCRValue=0;

        switch(theChar) {
          case '#':
            theNCRValue=theNCRStr.ToInteger(&theErr,kAutoDetect);
            theEntity=PRUnichar(theNCRValue);
            break;
          case '{':
              //XXX Write ME!
            break;
          default:
            if(nsCRT::IsAsciiAlpha(theChar)) {
              dtd->ConvertEntityToUnicode(theNCRStr, &theNCRValue);
              if (eDTDMode_strict!=mode) {
                // XXX - Hack - Nav. does not support entity values > 255
                // on the other hand IE supports entity values > 255 with a
                // semicolon. I think it's reasonable to emulate IE than Nav.
                if(theNCRValue>255 && theTermChar!=';') break;
              }
              if(-1!=theNCRValue) {
                theEntity=PRUnichar(theNCRValue);
              }
            }
            break;
        } //switch
        
        if(theEntity) {
          theOutString.Append(theEntity);
          if(theTermChar!='\0' && theTermChar!='&' && theTermChar!=';') {
            theOutString.Append(theTermChar);
          }
        }
        else {
          //what looked like an entity is not really one.
          //so let's copy the ncrstring back to the output string
          if(theTermChar!='&') { theTermPos++; }
          aString.Mid(theNCRStr,theAmpPos,theTermPos-theAmpPos);
          theOutString.Append(theNCRStr);
        }
        theAmpPos = aString.FindChar('&',PR_FALSE,theTermPos);
      } //while

      if(0<theOutString.Length()) {

        if(theStartPos<theLen) {
          aString.Mid(theNCRStr,theStartPos,theLen-theStartPos);
          theOutString.Append(theNCRStr);
        }

        aString=theOutString;
      } 

    } //if
  } //if
}  


// Temporary factory code to create content objects

void
HTMLContentSink::GetAttributeValueAt(const nsIParserNode& aNode,
                                     PRInt32 aIndex,
                                     nsString& aResult)
{
  // Copy value
  const nsString& value = aNode.GetValueAt(aIndex); 
  aResult.Truncate();
  aResult.Append(value);
  aResult.Trim("\b\r\t\n",PR_TRUE,PR_TRUE,PR_TRUE);

  if ( !aResult.IsEmpty() ) {
    // Strip quotes if present
    PRUnichar first = aResult.First();
    if ((first == '\"') || (first == '\'')) {
      if (aResult.Last() == first) {
        aResult.Cut(0, 1);
        PRInt32 pos = aResult.Length() - 1;
        if (pos >= 0) {
          aResult.Cut(pos, 1);
        }
      } else {
        // Mismatched quotes - leave them in
      }
    }
    ReduceEntities(aResult);
  }
}

nsresult
HTMLContentSink::AddAttributes(const nsIParserNode& aNode,
                               nsIHTMLContent* aContent,
                               PRBool aNotify)
{
  // Add tag attributes to the content attributes
  nsAutoString k, v;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsString& key = aNode.GetKeyAt(i);
    k.Truncate();
    k.Append(key);
    k.ToLowerCase();

    nsIAtom*  keyAtom = NS_NewAtom(k);
    nsHTMLValue value;
    
    if (NS_CONTENT_ATTR_NOT_THERE == 
        aContent->GetHTMLAttribute(keyAtom, value)) {
      // Get value and remove mandatory quotes
      GetAttributeValueAt(aNode, i, v);

      // Add attribute to content
      aContent->SetAttribute(kNameSpaceID_HTML, keyAtom, v,aNotify);
    }
    NS_RELEASE(keyAtom);
  }
  return NS_OK;
}

static
void SetForm(nsIHTMLContent* aContent, nsIDOMHTMLFormElement* aForm)
{
  nsIFormControl* formControl = nsnull;
  nsresult result = aContent->QueryInterface(kIFormControlIID, (void**)&formControl);
  if ((NS_OK == result) && formControl) {
    formControl->SetForm(aForm);
    NS_RELEASE(formControl);
  }
}

// XXX compare switch statement against nsHTMLTags.h's list
static nsresult
MakeContentObject(nsHTMLTag aNodeType,
                  nsINodeInfo *aNodeInfo,
                  nsIDOMHTMLFormElement* aForm,
                  nsIWebShell* aWebShell,
                  nsIHTMLContent** aResult,
                  const nsString* aContent = nsnull)
{
  nsresult rv = NS_OK;
  switch (aNodeType) {
  default:
    rv = NS_NewHTMLSpanElement(aResult, aNodeInfo);
    break;

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
    SetForm(*aResult, aForm);
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
  case eHTMLTag_div:
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
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_font:
    rv = NS_NewHTMLFontElement(aResult, aNodeInfo);
    break;
  case eHTMLTag_form:
    // the form was already created 
    if (aForm) {
      rv = aForm->QueryInterface(kIHTMLContentIID, (void**)aResult);
    }
    else {
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
    rv = NS_NewHTMLInputElement(aResult, aNodeInfo);
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_isindex:
    rv = NS_NewHTMLIsIndexElement(aResult, aNodeInfo);
    break;
  case eHTMLTag_label:
    rv = NS_NewHTMLLabelElement(aResult, aNodeInfo);
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_legend:
    rv = NS_NewHTMLLegendElement(aResult, aNodeInfo);
    SetForm(*aResult, aForm);
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
    rv = NS_NewHTMLSelectElement(aResult, aNodeInfo);
    SetForm(*aResult, aForm);
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
    // XXX why is textarea not a container. If it were, this code would not be necessary
    // If the text area has some content, set it 
    if (aContent && (aContent->Length() > 0)) {
      nsIDOMHTMLTextAreaElement* taElem;
      rv = (*aResult)->QueryInterface(kIDOMHTMLTextAreaElementIID, (void **)&taElem);
      if ((NS_OK == rv) && taElem) {
        taElem->SetDefaultValue(*aContent);
        NS_RELEASE(taElem);
      }
    }
    SetForm(*aResult, aForm);
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
  case eHTMLTag_layer:
  case eHTMLTag_ilayer:
  case eHTMLTag_nolayer:
  case eHTMLTag_unknown:
  case eHTMLTag_userdefined:
    rv = NS_NewHTMLUnknownElement(aResult, aNodeInfo);
    break;
  }

  return rv;
}

#if 0
// XXX is this logic needed by nsDOMHTMLOptionElement?
void
GetOptionText(const nsIParserNode& aNode, nsString& aText)
{
  aText.SetLength(0);
  switch (aNode.GetTokenType()) {
  case eToken_text:
  case eToken_whitespace:
  case eToken_newline:
    aText.Append(aNode.GetText());
    break;

  case eToken_entity:
    {
      nsAutoString tmp2("");
      PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp2);
      if (unicode < 0) {
        aText.Append(aNode.GetText());
      } else {
        aText.Append(tmp2);
      }
    }
    break;
  }
    nsAutoString x;
    char* y = aText.ToNewCString();
    printf("foo");
}
#endif

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
  nsAutoString tmp;
  if (eHTMLTag_userdefined == aNodeType) {
    tmp.Append(aNode.GetText());
    tmp.ToLowerCase();
  }
  else {
    nsCOMPtr<nsIDTD> dtd;
    rv = mParser->GetDTD(getter_AddRefs(dtd));
    if (NS_SUCCEEDED(rv)) {
      nsAutoString str;
      dtd->IntTagToStringTag(aNodeType, str);
      tmp.Append(str);
    }
  }

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsINodeInfo> nodeInfo;
    rv = mNodeInfoManager->GetNodeInfo(tmp, nsnull, kNameSpaceID_None,
                                       *getter_AddRefs(nodeInfo));

    if (NS_FAILED(rv)) {
      return rv;
    }

    // Make the content object
    // XXX why is textarea not a container?
    nsAutoString content;
    if (eHTMLTag_textarea == aNodeType) {
      content.Assign(aNode.GetSkippedContent());
    }
    rv = MakeContentObject(aNodeType, nodeInfo, aForm, aWebShell,
                           aResult, &content);

    PRInt32 id;
    mDocument->GetAndIncrementContentID(&id);
    (*aResult)->SetContentID(id);
  }

  return rv;
}

static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);

nsresult
NS_CreateHTMLElement(nsIHTMLContent** aResult, nsINodeInfo *aNodeInfo)
{
  nsresult rv = NS_OK;

  NS_WITH_SERVICE(nsIParserService,
                  parserService, 
                  kParserServiceCID,
                  &rv);

  if (NS_SUCCEEDED(rv)) {
    nsAutoString tmpName;
    aNodeInfo->GetName(tmpName);
    // Find tag in tag table
    PRInt32 id; 
    rv = parserService->HTMLStringTagToId(tmpName, &id);
    if (eHTMLTag_userdefined == nsHTMLTag(id)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // Create atom for tag and then create content object
    nsAutoString tag;
    rv = parserService->HTMLIdToStringTag(id, tag);
    nsCOMPtr<nsIAtom> atom(dont_AddRef(NS_NewAtom(tag.GetUnicode())));
    nsCOMPtr<nsINodeInfo> newName;
    aNodeInfo->NameChanged(atom, *getter_AddRefs(newName));

    rv = MakeContentObject(nsHTMLTag(id), newName, nsnull, nsnull, aResult);
  }

  return rv;
}

//----------------------------------------------------------------------


class nsHTMLElementFactory : public nsIElementFactory, public nsSupportsWeakReference {
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
  NS_PRECONDITION(aInstancePtrResult, "null OUT ptr");
  if (!aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLElementFactory* it = new nsHTMLElementFactory();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIElementFactory),
                            (void**)aInstancePtrResult);
}

nsHTMLElementFactory::nsHTMLElementFactory()
{
  NS_INIT_REFCNT();
}

nsHTMLElementFactory::~nsHTMLElementFactory()
{
}

NS_IMPL_ISUPPORTS2(nsHTMLElementFactory, nsIElementFactory, nsISupportsWeakReference);

NS_IMETHODIMP
nsHTMLElementFactory::CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                          nsIContent** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  nsresult rv;
  nsCOMPtr<nsIHTMLContent> htmlContent;
  rv = NS_CreateHTMLElement(getter_AddRefs(htmlContent), aNodeInfo);
  nsCOMPtr<nsIContent> content = do_QueryInterface(htmlContent);
  *aResult = content;
  NS_IF_ADDREF(*aResult);
  return rv;
}

//----------------------------------------------------------------------

MOZ_DECL_CTOR_COUNTER(SinkContext);

SinkContext::SinkContext(HTMLContentSink* aSink)
{
  MOZ_COUNT_CTOR(SinkContext);
  mSink = aSink;
  mPreAppend = PR_FALSE;
  mNotifyLevel = 0;
  mStack = nsnull;
  mStackSize = 0;
  mStackPos = 0;
  mText = nsnull;
  mTextLength = 0;
  mTextSize = 0;
  mLastTextNode = nsnull;
  mLastTextNodeSize = 0;
  mIgnoredOpenCount = 0;
}

SinkContext::~SinkContext()
{
  MOZ_COUNT_DTOR(SinkContext);
  if (nsnull != mStack) {
    for (PRInt32 i = 0; i < mStackPos; i++) {
      NS_RELEASE(mStack[i].mContent);
    }
    delete [] mStack;
  }
  if (nsnull != mText) {
    delete [] mText;
  }
  NS_IF_RELEASE(mLastTextNode);
}

nsresult
SinkContext::Begin(nsHTMLTag aNodeType, 
                   nsIHTMLContent* aRoot, 
                   PRInt32 aNumFlushed,
                   PRInt32 aInsertionPoint)
{
  if (1 > mStackSize) {
    nsresult rv = GrowStack();
    if (NS_OK != rv) {
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
  if (aTag == mStack[mStackPos-1].mType) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

PRBool
SinkContext::IsAncestorContainer(nsHTMLTag aTag)
{
  PRInt32 stackPos = mStackPos-1;

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
  nsIHTMLContent* content = mStack[mStackPos-1].mContent;
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
    nsIContent* parent = mStack[mStackPos-1].mContent;
    parent->ChildCount(childCount);
    mStack[mStackPos-1].mNumFlushed = childCount;
  }

  if ((2 == mStackPos) &&
      (mSink->mBody == mStack[1].mContent)) {
    // We just finished adding something to the body
    mNotifyLevel = 0;
  }

  // If we just added content to a node for which
  // an insertion happen, we need to do an immediate 
  // notification for that insertion.
  if (!aDidNotify && (0 < mStackPos) && 
      (mStack[mStackPos-1].mInsertionPoint != -1)) {
    nsIContent* parent = mStack[mStackPos-1].mContent;

#ifdef NS_DEBUG
    // Tracing code
    char cbuf[40];
    const char* cp;
    nsAutoString str;
    nsCOMPtr<nsIDTD> dtd;
    mSink->mParser->GetDTD(getter_AddRefs(dtd));
    dtd->IntTagToStringTag(nsHTMLTag(mStack[mStackPos-1].mType), str);
    cp = str.ToCString(cbuf, sizeof(cbuf));
      
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("SinkContext::DidAddContent: Insertion notification for parent=%s at position=%d and stackPos=%d", 
                cp, mStack[mStackPos-1].mInsertionPoint-1, mStackPos-1));
#endif

    mSink->NotifyInsert(parent, 
                        aContent, 
                        mStack[mStackPos-1].mInsertionPoint-1);
    parent->ChildCount(childCount);
    mStack[mStackPos-1].mNumFlushed = childCount;
  }
  else if (!aDidNotify && mSink->IsTimeToNotify()) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("SinkContext::DidAddContent: Notification as a result of the interval expiring; backoff count: %d", mSink->mBackoffCount));
    FlushTags(PR_TRUE);
  }
}

nsresult
SinkContext::OpenContainer(const nsIParserNode& aNode)
{
  FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::OpenContainer", aNode, mStackPos, mSink);

  // XXX - Hack to handle stack over flow ( Bug 18480 )
  if (mStackPos > 1000) {
    mIgnoredOpenCount++;
    return AddLeaf(aNode);
  }
  // XXX - Hack Ends

  nsresult rv;
  if (mStackPos + 1 > mStackSize) {
    rv = GrowStack();
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Create new container content object
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  nsIHTMLContent* content;
  rv = mSink->CreateContentObject(aNode, nodeType,
                                  mSink->mCurrentForm,
                                  mSink->mFrameset ? mSink->mWebShell : nsnull,
                                  &content);
  if (NS_OK != rv) {
    return rv;
  }

  nsresult srv;
  nsCOMPtr<nsISelectElement> select = do_QueryInterface(content, &srv);
  if (NS_SUCCEEDED(srv)) {
    srv = select->DoneAddingContent(PR_FALSE);
  }

  mStack[mStackPos].mType = nodeType;
  mStack[mStackPos].mContent = content;
  mStack[mStackPos].mFlags = 0;
  mStack[mStackPos].mNumFlushed = 0;
  mStack[mStackPos].mInsertionPoint = -1;
  content->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);
  
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
  mSink->mDocument->GetScriptGlobalObject(getter_AddRefs(scriptGlobalObject));
  rv = mSink->AddAttributes(aNode, content);

  if (mPreAppend) {
    NS_ASSERTION(mStackPos > 0, "container w/o parent");
    if (mStackPos <= 0) {
      return NS_ERROR_FAILURE;
    }
    nsIHTMLContent* parent = mStack[mStackPos-1].mContent;
    if (mStack[mStackPos-1].mInsertionPoint != -1) {
      parent->InsertChildAt(content, 
                            mStack[mStackPos-1].mInsertionPoint++, 
                            PR_FALSE);
    }
    else {
      parent->AppendChildTo(content, PR_FALSE);
    }
    mStack[mStackPos].mFlags |= APPENDED;
  }
  mStackPos++;
  if (NS_OK != rv) {
    return rv;
  }

  if (mSink->IsMonolithicContainer(nodeType)) {
      mSink->mInMonolithicContainer++;
  }
    
  // Special handling for certain tags
  switch (nodeType) {

    case eHTMLTag_noembed:
    case eHTMLTag_noframes:
    case eHTMLTag_noscript:
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
SinkContext::CloseContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  // XXX - Hack to handle stack over flow ( Bug 18480 )
  if (mIgnoredOpenCount) {
    mIgnoredOpenCount--;
    return NS_OK;
  }
  // XXX -Hack Ends.

  // Flush any collected text content. Release the last text
  // node to indicate that no more should be added to it.
  FlushTextAndRelease();
  
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::CloseContainer", aNode, mStackPos-1, mSink);

  NS_WARN_IF_FALSE(mStackPos > 0,"stack out of bounds. wrong context probably!");

  if(mStackPos<=0) {
    return NS_OK; // Fix crash - Ref. bug 45975 or 45007
  }

  --mStackPos;
  nsHTMLTag nodeType = mStack[mStackPos].mType;
  nsIHTMLContent* content = mStack[mStackPos].mContent;
  content->Compact();

  // Add container to its parent if we haven't already done it
  if (0 == (mStack[mStackPos].mFlags & APPENDED)) {
    NS_ASSERTION(mStackPos > 0, "container w/o parent");
    if (mStackPos <= 0) {
      return NS_ERROR_FAILURE;
    }
    nsIHTMLContent* parent = mStack[mStackPos-1].mContent;
    // If the parent has an insertion point, insert rather than
    // append.
    if (mStack[mStackPos-1].mInsertionPoint != -1) {
      result = parent->InsertChildAt(content, 
                                     mStack[mStackPos-1].mInsertionPoint++, 
                                     PR_FALSE);
    }
    else {
      result = parent->AppendChildTo(content, PR_FALSE);
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
      char cbuf[40];
      const char* cp;
      nsAutoString str;
      nsCOMPtr<nsIDTD> dtd;
      mSink->mParser->GetDTD(getter_AddRefs(dtd));
      dtd->IntTagToStringTag(nsHTMLTag(nodeType), str);
      cp = str.ToCString(cbuf, sizeof(cbuf));

      SINK_TRACE(SINK_TRACE_REFLOW,
                 ("SinkContext::CloseContainer: reflow on notifyImmediate tag=%s newIndex=%d stackPos=%d", cp, mStack[mStackPos].mNumFlushed, mStackPos));
#endif 
      mSink->NotifyAppend(content, mStack[mStackPos].mNumFlushed);
    }

    // Indicate that notification has now happened at this level
    mNotifyLevel = mStackPos-1;
  }

  DidAddContent(content, PR_FALSE);

  if (mSink->IsMonolithicContainer(nodeType)) {
    --mSink->mInMonolithicContainer;
  }

  // Special handling for certain tags
  switch (nodeType) {

    case eHTMLTag_noembed:
    case eHTMLTag_noframes:
    case eHTMLTag_noscript:
      // Fix bug 40216
      NS_ASSERTION((mSink->mInsideNoXXXTag > 0), "mInsideNoXXXTag underflow");
      if (mSink->mInsideNoXXXTag > 0) {
        mSink->mInsideNoXXXTag--;
      }
      break;
 
    case eHTMLTag_form:
      {
        nsHTMLTag parserNodeType = nsHTMLTag(aNode.GetNodeType());
      
        // If there's a FORM on the stack, but this close tag doesn't
        // close the form, then close out the form *and* close out the
        // next container up. This is since the parser doesn't do fix up
        // of invalid form nesting. When the end FORM tag comes through, 
        // we'll ignore it.
        if (parserNodeType != nodeType) {
          result = CloseContainer(aNode);
        }
      }
      break;

    case eHTMLTag_iframe:
      mSink->mNumOpenIFRAMES--;
      break;

    case eHTMLTag_select:
      {
        nsCOMPtr<nsISelectElement> select = do_QueryInterface(content, &result);

        if (NS_SUCCEEDED(result)) {
          result = select->DoneAddingContent(PR_TRUE);
        }
      }
      break;

    default:
      break;

  }

  NS_IF_RELEASE(content);

#ifdef DEBUG
  if (mPreAppend && 
      SINK_LOG_TEST(gSinkLogModuleInfo,SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return result;
}

static void
SetDocumentInChildrenOf(nsIContent* aContent, 
                        nsIDocument* aDocument)
{
  PRInt32 i, n;
  aContent->ChildCount(n);
  for (i = 0; i < n; i++) {
    nsIContent* child;
    aContent->ChildAt(i, child);
    if (nsnull != child) {
      child->SetDocument(aDocument, PR_TRUE, PR_TRUE);
      NS_RELEASE(child);
    }
  }
}

// This method is called when a container is determined to be
// non well-formed in the source content. Currently this can only
// happen for forms, since the parser doesn't do fixup of forms.
// The method makes the container a leaf and moves all the container's
// children up a level to the container's parent.
nsresult
SinkContext::DemoteContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  
  // Search for the nearest container on the stack of the
  // specified type
  PRInt32 stackPos = mStackPos-1;
  while ((stackPos > 0) && (nodeType != mStack[stackPos].mType)) {
    stackPos--;
  }
  
  // If we find such a container...
  if (stackPos > 0) {
    nsIHTMLContent* container = mStack[stackPos].mContent;
    PRBool sync = PR_FALSE;
    
    // See if it has a parent on the stack. It should for all the
    // cases for which this is called, but put in a check anyway
    if (stackPos > 1) {
      nsIHTMLContent* parent = mStack[stackPos-1].mContent;
      PRInt32 parentCount;

      // If we've already flushed the demoted container, flush
      // what we have and do everything synchronously. If not,
      // the children that will be promoted will just be dealt
      // with later.
      parent->ChildCount(parentCount);
      if (mStack[stackPos-1].mNumFlushed == parentCount) {
        FlushTags(PR_TRUE);
        sync = PR_TRUE;
      }
      // Otherwise just append the container to the parent without
      // notification
      else {
        parent->AppendChildTo(container, PR_FALSE);
      }
      
      if (NS_SUCCEEDED(result)) {
        // Move all of the demoted containers children to its parent
        PRInt32 i, count;
        container->ChildCount(count);
        
        for (i = 0; i < count && NS_SUCCEEDED(result); i++) {
          nsIContent* child;
          
          // Since we're removing as we go along, always get the
          // first child
          result = container->ChildAt(0, child);
          if (NS_SUCCEEDED(result)) {
            // Remove it from its old parent (the demoted container)
            
            // If the child is a form control, cache the form that contains it.
            // After the form control is removed from it's container, restore
            // it's form.
            nsIFormControl* childFormControl = nsnull;
            result = child->QueryInterface(kIFormControlIID, (void**)&childFormControl);
            if (NS_SUCCEEDED(result)) {
              // It is a form control, so get it's form and cache it.
              nsIDOMHTMLFormElement* formElem = nsnull;
              childFormControl->GetForm(&formElem);
              // Removing the child will set it's form control to nsnull.
              result = container->RemoveChildAt(0, sync);
              // Restore the child's form control using the cache'd pointer.
              childFormControl->SetForm(formElem);
              
              NS_RELEASE(childFormControl);
              NS_IF_RELEASE(formElem);
            } else {
              result = container->RemoveChildAt(0, sync);
            }
            
            if (NS_SUCCEEDED(result)) {
              SetDocumentInChildrenOf(child, mSink->mDocument);
              // Note that we're doing synchronous notifications here
              // since we already did notifications for all content
              // that's come through with the FlushTags() call so far.
              result = parent->AppendChildTo(child, sync);
            }
            NS_RELEASE(child);
          }
        }
        
        // Remove the demoted element from the context stack.
        while (stackPos < mStackPos-1) {
          mStack[stackPos].mType = mStack[stackPos+1].mType;
          mStack[stackPos].mContent = mStack[stackPos+1].mContent;
          mStack[stackPos].mFlags = mStack[stackPos+1].mFlags;
          stackPos++;
        }
        mStackPos--;
      }
    }
    NS_RELEASE(container);

    if (sync) {
      // Update child counts for everything on the stack, since
      // we've moved around content in the hierarchy
      UpdateChildCounts();
    }
  }
  
  return result;
}

nsresult
SinkContext::AddLeaf(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::AddLeaf", aNode, mStackPos, mSink);

  nsresult rv = NS_OK;

  switch (aNode.GetTokenType()) {
  case eToken_start:
    {
      FlushTextAndRelease();

      // Create new leaf content object
      nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
      nsIHTMLContent* content;
      rv = mSink->CreateContentObject(aNode, nodeType,
                                      mSink->mCurrentForm, mSink->mWebShell,
                                      &content);
      if (NS_OK != rv) {
        return rv;
      }      

      // Set the content's document
      content->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);

      rv = mSink->AddAttributes(aNode, content);
      if (NS_OK != rv) {
        NS_RELEASE(content);
        return rv;
      }
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
      NS_RELEASE(content);
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
      }
      else {
        // Map carriage returns to newlines
        if (tmp.CharAt(0) == '\r') {
          tmp.AssignWithConversion("\n");
        }
        rv = AddText(tmp);
      }
    }
    break;

  case eToken_skippedcontent:
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
  nsIHTMLContent* parent = mStack[mStackPos-1].mContent;
  // If the parent has an insertion point, insert rather than
  // append.
  if (mStack[mStackPos-1].mInsertionPoint != -1) {
    parent->InsertChildAt(aContent, 
                          mStack[mStackPos-1].mInsertionPoint++, 
                          PR_FALSE);
  }
  else {
    parent->AppendChildTo(aContent, PR_FALSE);
  }

  DidAddContent(aContent, PR_FALSE);

#ifdef DEBUG
  if (mPreAppend && 
      SINK_LOG_TEST(gSinkLogModuleInfo,SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return NS_OK;
}

nsresult
SinkContext::AddComment(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::AddLeaf", aNode, mStackPos, mSink);

  nsIContent *comment;
  nsIDOMComment *domComment;
  nsresult result = NS_OK;

  FlushTextAndRelease();

  result = NS_NewCommentNode(&comment);
  if (NS_OK == result) {
    result = comment->QueryInterface(kIDOMCommentIID, 
                                     (void **)&domComment);
    if (NS_OK == result) {
      domComment->AppendData(aNode.GetText());
      NS_RELEASE(domComment);
      
      comment->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);
      
      nsIHTMLContent* parent;
      if ((nsnull == mSink->mBody) && (nsnull != mSink->mHead)) {
        parent = mSink->mHead;
      }
      else {
        parent = mStack[mStackPos - 1].mContent;
      }
      // If the parent has an insertion point, insert rather than
      // append.
      if (mStack[mStackPos-1].mInsertionPoint != -1) {
        parent->InsertChildAt(comment, 
                              mStack[mStackPos-1].mInsertionPoint++, 
                              PR_FALSE);
      }
      else {
        parent->AppendChildTo(comment, PR_FALSE);
      }
      
      DidAddContent(comment, PR_FALSE);

#ifdef DEBUG
      if (mPreAppend && 
          SINK_LOG_TEST(gSinkLogModuleInfo,SINK_ALWAYS_REFLOW)) {
        mSink->ForceReflow();
      }
#endif
    }
    NS_RELEASE(comment);
  }
  
  return result;
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
  if (0 == newSize) {
    newSize = 32;
  }
  Node* stack = new Node[newSize];
  if (nsnull == stack) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (0 != mStackPos) {
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
// XXX If we get a giant string grow the buffer instead of chopping it up???
nsresult
SinkContext::AddText(const nsString& aText)
{
  PRInt32 addLen = aText.Length();
  if (0 == addLen) {
    return NS_OK;
  }

  // Create buffer when we first need it
  if (0 == mTextSize) {
    mText = new PRUnichar[4096];
    if (nsnull == mText) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mTextSize = 4096;
  }
//  else if (mTextLength + addLen > mTextSize) {
//  }

  // Copy data from string into our buffer; flush buffer when it fills up
  PRInt32 offset = 0;
  while (0 != addLen) {
    PRInt32 amount = mTextSize - mTextLength;
    if (amount > addLen) {
      amount = addLen;
    }
    if (0 == amount) {
      // Don't release last text node so we can add to it again
      nsresult rv = FlushText();
      if (NS_OK != rv) {
        return rv;
      }
    }
    memcpy(&mText[mTextLength], aText.GetUnicode() + offset,
           sizeof(PRUnichar) * amount);
    mTextLength += amount;
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
  PRInt32 stackPos = mStackPos-1;
  while ((stackPos > 0) && (0 == (mStack[stackPos].mFlags & APPENDED))) {
    content = mStack[stackPos].mContent;
    nsIHTMLContent* parent = mStack[stackPos-1].mContent;
    
    // If the parent has an insertion point, insert rather than
    // append.
    if (mStack[mStackPos-1].mInsertionPoint != -1) {
      parent->InsertChildAt(content, 
                            mStack[mStackPos-1].mInsertionPoint++, 
                            PR_FALSE);
    }
    else {
      parent->AppendChildTo(content, PR_FALSE);
    }
    mStack[stackPos].mFlags |= APPENDED;

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
        char cbuf[40];
        const char* cp;
        nsAutoString str;
        nsCOMPtr<nsIDTD> dtd;
        mSink->mParser->GetDTD(getter_AddRefs(dtd));
        dtd->IntTagToStringTag(nsHTMLTag(mStack[stackPos].mType), str);
        cp = str.ToCString(cbuf, sizeof(cbuf));
        
        SINK_TRACE(SINK_TRACE_REFLOW,
                   ("SinkContext::FlushTags: tag=%s from newindex=%d at stackPos=%d", 
                    cp, mStack[stackPos].mNumFlushed, stackPos));
#endif
        if ((mStack[stackPos].mInsertionPoint != -1) &&
            (mStackPos > (stackPos+1))) {
          nsIContent* child = mStack[stackPos+1].mContent;
          mSink->NotifyInsert(content, 
                              child, 
                              mStack[stackPos].mInsertionPoint);    
        }
        else {
          mSink->NotifyAppend(content, mStack[stackPos].mNumFlushed);    
        }
        flushed = PR_TRUE;
      } 
      
      mStack[stackPos].mNumFlushed = childCount;
      stackPos++;
    }
    mNotifyLevel = mStackPos-1;
  }

  return result;
}

void
SinkContext::UpdateChildCounts()
{
  PRInt32 childCount;
  nsIHTMLContent* content;

  // Start from the top of the stack (growing upwards) and see if
  // any new content has been appended. If so, we recognize that
  // reflows have been generated for it and we should make sure that
  // no further reflows occur.
  PRInt32 stackPos = mStackPos-1;
  while (stackPos > 0) {
    if (mStack[stackPos].mFlags & APPENDED) {
      content = mStack[stackPos].mContent;
      content->ChildCount(childCount);
      mStack[stackPos].mNumFlushed = childCount;
    }
    stackPos--;
  }

  mNotifyLevel = mStackPos-1;
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
  if (0 != mTextLength) {
    if (mLastTextNode) {
      if ((mLastTextNodeSize + mTextLength) > mSink->mMaxTextRun) {
        mLastTextNodeSize = 0;
        NS_RELEASE(mLastTextNode);
        FlushText(aDidFlush, aReleaseLast);
      }
      else {
        nsCOMPtr<nsIDOMCharacterData> cdata = do_QueryInterface(mLastTextNode, &rv);
        if (NS_SUCCEEDED(rv)) {
          CBufDescriptor bd(mText, PR_TRUE, mTextSize+1, mTextLength);
          bd.mIsConst = PR_TRUE;
          nsAutoString str(bd);
          
          rv = cdata->AppendData(str);
          
          mLastTextNodeSize += mTextLength;
          mTextLength = 0;
          didFlush = PR_TRUE;
        }
      }
    }
    else {
      nsIContent* content;
      rv = NS_NewTextNode(&content);
      if (NS_OK == rv) {
        // Set the content's document
        content->SetDocument(mSink->mDocument, PR_FALSE, PR_TRUE);
        
        // Set the text in the text node
        nsITextContent* text = nsnull;
        content->QueryInterface(kITextContentIID, (void**) &text);
        text->SetText(mText, mTextLength, PR_FALSE);
        NS_RELEASE(text);
        
        // Add text to its parent
        NS_ASSERTION(mStackPos > 0, "leaf w/o container");
        if (mStackPos <= 0) {
          return NS_ERROR_FAILURE;
        }
        nsIHTMLContent* parent = mStack[mStackPos - 1].mContent;
        if (mStack[mStackPos-1].mInsertionPoint != -1) {
          parent->InsertChildAt(content, 
                                mStack[mStackPos-1].mInsertionPoint++, 
                                PR_FALSE);
        }
        else {
          parent->AppendChildTo(content, PR_FALSE);
        }

        mLastTextNode = content;

        mLastTextNodeSize += mTextLength;
		    mTextLength = 0;
    		didFlush = PR_TRUE;

        DidAddContent(content, PR_FALSE);
      }
    }
  }
  if (nsnull != aDidFlush) {
    *aDidFlush = didFlush;
  }

  if (aReleaseLast && mLastTextNode) {
    mLastTextNodeSize = 0;
    NS_RELEASE(mLastTextNode);
  }
#ifdef DEBUG
  if (mPreAppend && didFlush &&
      SINK_LOG_TEST(gSinkLogModuleInfo,SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif
  return rv;
}


nsresult
NS_NewHTMLContentSink(nsIHTMLContentSink** aResult,
                      nsIDocument* aDoc,
                      nsIURI* aURL,
                      nsIWebShell* aWebShell)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  HTMLContentSink* it;
  NS_NEWXPCOM(it, HTMLContentSink);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aDoc, aURL, aWebShell);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(kIHTMLContentSinkIID, (void **)aResult);
}

// Note: operator new zeros our memory
HTMLContentSink::HTMLContentSink() {
  NS_INIT_REFCNT();
#ifdef NS_DEBUG
  if (nsnull == gSinkLogModuleInfo) {
    gSinkLogModuleInfo = PR_NewLogModule("htmlcontentsink");
  }
#endif
  mNotAtRef        = PR_TRUE;  
  mInScript = 0;
  mInNotification = 0;
  mInMonolithicContainer = 0;
  mInsideNoXXXTag  = 0;
  mNodeInfoManager = nsnull;
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

  NS_IF_RELEASE(mCurrentForm);
  NS_IF_RELEASE(mCurrentMap);
  NS_IF_RELEASE(mRefContent);

  NS_IF_RELEASE(mNodeInfoManager);

  if (mNotificationTimer) {
    mNotificationTimer->Cancel();
  }

  PRInt32 numContexts = mContextStack.Count();
  
  if(mCurrentContext==mHeadContext) {
    // Pop off the second html context if it's not done earlier
    mContextStack.RemoveElementAt(--numContexts);
  }

  for (PRInt32 i = 0; i < numContexts; i++) {
    SinkContext* sc = (SinkContext*)mContextStack.ElementAt(i);
    if(sc) {
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
  if (nsnull != mCurrentContext) {
    delete mCurrentContext;
  }
  if (nsnull != mHeadContext) {
    delete mHeadContext;
  }
  if (nsnull != mTitle) {
    delete mTitle;
  }
  if (nsnull != mRef) {
    delete mRef;
  }
}

NS_IMPL_ISUPPORTS7(HTMLContentSink, 
                   nsIHTMLContentSink,
                   nsIContentSink,
                   nsIStreamLoaderObserver,
                   nsITimerCallback,
                   nsICSSLoaderObserver,
                   nsIDocumentObserver,
                   nsIDebugDumpContent)

nsresult
HTMLContentSink::Init(nsIDocument* aDoc,
                      nsIURI* aURL,
                      nsIWebShell* aContainer)
{  
  MOZ_TIMER_DEBUGLOG(("Reset and start: nsHTMLContentSink::Init(), this=%p\n", this));
  MOZ_TIMER_RESET(mWatch);
  MOZ_TIMER_START(mWatch);

  NS_PRECONDITION(nsnull != aDoc, "null ptr");
  NS_PRECONDITION(nsnull != aURL, "null ptr");
  NS_PRECONDITION(nsnull != aContainer, "null ptr");
  if ((nsnull == aDoc) || (nsnull == aURL) || (nsnull == aContainer)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;

  mDocument = aDoc;
  NS_ADDREF(aDoc);
  aDoc->AddObserver(this);
  aDoc->QueryInterface(kIHTMLDocumentIID, (void**)&mHTMLDocument);
  rv = mDocument->GetNodeInfoManager(mNodeInfoManager);
  NS_ENSURE_SUCCESS(rv, rv);
  mDocumentURI = aURL;
  NS_ADDREF(aURL);
  mDocumentBaseURL = aURL;
  NS_ADDREF(aURL);
  mWebShell = aContainer;
  NS_ADDREF(aContainer);

  NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);

  if (NS_FAILED(rv)) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }

  mNotifyOnTimer = PR_TRUE;
  prefs->GetBoolPref("content.notify.ontimer", &mNotifyOnTimer);

  mBackoffCount = -1; // never
  prefs->GetIntPref("content.notify.backoffcount", &mBackoffCount);

  mNotificationInterval = 1000000;
  prefs->GetIntPref("content.notify.interval", &mNotificationInterval);

  mMaxTextRun = 8192;
  prefs->GetIntPref("content.maxtextrun", &mMaxTextRun);

  nsIHTMLContentContainer* htmlContainer = nsnull;
  if (NS_SUCCEEDED(aDoc->QueryInterface(kIHTMLContentContainerIID, (void**)&htmlContainer))) {
    htmlContainer->GetCSSLoader(mCSSLoader);
    NS_RELEASE(htmlContainer);
  }

  // XXX this presumes HTTP header info is alread set in document
  // XXX if it isn't we need to set it here...
  mDocument->GetHeaderData(nsHTMLAtoms::headerDefaultStyle, mPreferredStyle);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::html, nsnull,
                                     kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  // Make root part
  rv = NS_NewHTMLHtmlElement(&mRoot, nodeInfo);
  if (NS_OK != rv) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }
  mRoot->SetDocument(mDocument, PR_FALSE, PR_TRUE);
  mDocument->SetRootContent(mRoot);

  // Make head part
  rv = mNodeInfoManager->GetNodeInfo(NS_ConvertASCIItoUCS2("head"),
                                     nsnull, kNameSpaceID_None,
                                     *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewHTMLHeadElement(&mHead, nodeInfo);
  if (NS_OK != rv) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }
  mRoot->AppendChildTo(mHead, PR_FALSE);

  mCurrentContext = new SinkContext(this);
  mCurrentContext->Begin(eHTMLTag_html, mRoot, 0, -1);
  mContextStack.AppendElement(mCurrentContext);

  char* spec;
  (void)aURL->GetSpec(&spec);
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::Init: this=%p url='%s'",
              this, spec));
  nsCRT::free(spec);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Init()\n"));
  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillBuildModel(void)
{
  // Notify document that the load is beginning
  mDocument->BeginLoad();
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  // NRA Dump stopwatch stop info here
#ifdef MOZ_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::DidBuildModel(), this=%p\n", this));
  MOZ_TIMER_STOP(mWatch);
  MOZ_TIMER_LOG(("Content creation time (this=%p): ", this));
  MOZ_TIMER_PRINT(mWatch);
#endif

  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: canceling notification timeout"));
    mNotificationTimer->Cancel();
    mNotificationTimer = 0;
  }

  if (nsnull == mTitle) {
    mHTMLDocument->SetTitle(nsString());
  }

  // XXX this is silly; who cares? RickG cares. It's part of the regression test. So don't bug me. 
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell(dont_AddRef(mDocument->GetShellAt(i)));
    if (shell) {
      nsCOMPtr<nsIViewManager> vm;
      nsresult rv = shell->GetViewManager(getter_AddRefs(vm));
      if(NS_SUCCEEDED(rv) && vm) {
        vm->SetQuality(nsContentQuality(aQualityLevel));
      }
    }
  }


  // Reflow the last batch of content
  if (nsnull != mBody) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::DidBuildModel: layout final content"));
    mCurrentContext->FlushTags(PR_TRUE);
  }
  ScrollToRef();

  mDocument->EndLoad();
  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);
  return NS_OK;
}

NS_IMETHODIMP_(void)
HTMLContentSink::Notify(nsITimer *timer)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_START(mWatch);
#ifdef MOZ_DEBUG
  PRTime now = PR_Now();
  PRInt64 diff, interval;
  PRInt32 delay;

  LL_I2L(interval, mNotificationInterval);
  LL_SUB(diff, now, mLastNotificationTime);

  LL_SUB(diff, diff, interval);
  LL_L2I(delay, diff);
  delay /= PR_USEC_PER_MSEC;

  mBackoffCount--;
  SINK_TRACE(SINK_TRACE_REFLOW,
             ("HTMLContentSink::Notify: reflow on a timer: %d milliseconds late, backoff count: %d", delay, mBackoffCount));
#endif
  if (mCurrentContext) {
    mCurrentContext->FlushTags(PR_TRUE);
  }
  mNotificationTimer = 0;
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::Notify()\n"));
  MOZ_TIMER_STOP(mWatch);
}

NS_IMETHODIMP
HTMLContentSink::WillInterrupt()
{
  nsresult result = NS_OK;

  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::WillInterrupt: this=%p", this));
#ifdef SINK_NO_INCREMENTAL
  return NS_OK;
#else
  if (mNotifyOnTimer && mLayoutStarted) {
    if (mBackoffCount && !mInMonolithicContainer) {
      PRTime now = PR_Now();
      PRInt64 interval, diff;
      PRInt32 delay;
      
      LL_I2L(interval, mNotificationInterval);
      LL_SUB(diff, now, mLastNotificationTime);
      
      // If it's already time for us to have a notification
      if (LL_CMP(diff, >, interval)) {
        mBackoffCount--;
        SINK_TRACE(SINK_TRACE_REFLOW,
                 ("HTMLContentSink::WillInterrupt: flushing tags since we've run out time; backoff count: %d", mBackoffCount));
        result = mCurrentContext->FlushTags(PR_TRUE);
      }
      else {
        // If the time since the last notification is less than
        // the expected interval but positive, set a timer up for the remaining
        // interval.
        if (LL_CMP(diff, >, LL_ZERO)) {
          LL_SUB(diff, interval, diff);
          LL_L2I(delay, diff);
        }
        // Else set up a timer for the expected interval
        else {
          delay = mNotificationInterval;
        }
        
        // Convert to milliseconds
        delay /= PR_USEC_PER_MSEC;
        
        // Cancel a timer if we had one out there
        if (mNotificationTimer) {
          SINK_TRACE(SINK_TRACE_REFLOW,
                     ("HTMLContentSink::WillInterrupt: canceling notification timeout"));
          mNotificationTimer->Cancel();
        }
        
        mNotificationTimer = do_CreateInstance("component://netscape/timer", &result);
        if (NS_SUCCEEDED(result)) {
          SINK_TRACE(SINK_TRACE_REFLOW,
                     ("HTMLContentSink::WillInterrupt: setting up timer with delay %d", delay));
          
          result = mNotificationTimer->Init(this, delay);
        }
      }
    }
  }
  else {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::WillInterrupt: flushing tags unconditionally"));
    result = mCurrentContext->FlushTags(PR_TRUE);
  }

  return result;
#endif
}

NS_IMETHODIMP
HTMLContentSink::WillResume()
{
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::WillResume: this=%p", this));
  // Cancel a timer if we had one out there
  if (mNotificationTimer) {
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::WillResume: canceling notification timeout"));
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

NS_IMETHODIMP
HTMLContentSink::BeginContext(PRInt32 aPosition)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::BeginContext()\n"));
  MOZ_TIMER_START(mWatch);
  NS_PRECONDITION(aPosition > -1, "out of bounds");

  // Create new context
  SinkContext* sc = new SinkContext(this);
  if (nsnull == sc) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::BeginContext()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_ERROR_OUT_OF_MEMORY;
  }
   
  NS_ASSERTION(mCurrentContext != nsnull," Non-existing context");
  if (mCurrentContext == nsnull) {
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
  if (aPosition < (mCurrentContext->mStackPos-1)) {
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
  NS_PRECONDITION(mCurrentContext != nsnull && aPosition > -1, "non-existing context");

  PRInt32 n = mContextStack.Count() - 1;
  SinkContext* sc = (SinkContext*) mContextStack.ElementAt(n);

  NS_ASSERTION(sc->mStack[aPosition].mType == mCurrentContext->mStack[0].mType,"ending a wrong context");
  
  mCurrentContext->FlushTextAndRelease();

  sc->mStack[aPosition].mNumFlushed = mCurrentContext->mStack[0].mNumFlushed;

  for(PRInt32 i=0; i<mCurrentContext->mStackPos; i++)
    NS_IF_RELEASE(mCurrentContext->mStack[i].mContent);
  delete [] mCurrentContext->mStack;
  mCurrentContext->mStack      = nsnull;
  mCurrentContext->mStackPos   = 0;
  mCurrentContext->mStackSize  = 0;
  if(mCurrentContext->mText != nsnull)
    delete [] mCurrentContext->mText;
   mCurrentContext->mText      = nsnull;
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

  if (nsnull == mTitle) {
    mTitle = new nsString(aValue);
  }
  else {
    // If the title was already set then don't try to overwrite it
    // when a new title is encountered - For backwards compatiblity
    //*mTitle = aValue;
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::SetTitle()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_OK;
  }
  ReduceEntities(*mTitle);
  mTitle->CompressWhitespace(PR_TRUE, PR_TRUE);
  mHTMLDocument->SetTitle(*mTitle);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::title, nsnull,
                                              kNameSpaceID_None,
                                              *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIHTMLContent* it = nsnull;
  rv = NS_NewHTMLTitleElement(&it, nodeInfo);
  if (NS_OK == rv) {
    nsIContent* text;
    rv = NS_NewTextNode(&text);
    if (NS_OK == rv) {
      nsIDOMText* tc;
      rv = text->QueryInterface(kIDOMTextIID, (void**)&tc);
      if (NS_OK == rv) {
        tc->SetData(*mTitle);
        NS_RELEASE(tc);
      }
      it->AppendChildTo(text, PR_FALSE);
      text->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      NS_RELEASE(text);
    }
    mHead->AppendChildTo(it, PR_FALSE);
    NS_RELEASE(it);
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::SetTitle()\n"));
  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenHTML(const nsIParserNode& aNode)
{

  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenHTML", aNode, 0, this);
  if(mRoot) {
    // Add attributes to the node...if found.
    PRInt32 ac = aNode.GetAttributeCount();
    if(ac>0) AddAttributes(aNode,mRoot,PR_TRUE);
  }

  MOZ_TIMER_STOP(mWatch);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseHTML(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseHTML()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseHTML", aNode, 0, this);
  if (nsnull != mHeadContext) {
    if(mCurrentContext==mHeadContext) {
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
                  "HTMLContentSink::OpenHead", aNode, 0, this);
  nsresult rv = NS_OK;
  if (nsnull == mHeadContext) {
    mHeadContext = new SinkContext(this);
    if (nsnull == mHeadContext) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
      MOZ_TIMER_STOP(mWatch);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mHeadContext->SetPreAppend(PR_TRUE);
    rv = mHeadContext->Begin(eHTMLTag_head, mHead, 0, -1);
    if (NS_OK != rv) {
      MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
      MOZ_TIMER_STOP(mWatch);
      return rv;
    }
  }
  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = mHeadContext;

  if (nsnull != mHead) {
    rv = AddAttributes(aNode, mHead);
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenHead()\n"));
  MOZ_TIMER_STOP(mWatch);
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseHead(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseHead()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseHead", aNode, 
                  0, this);
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
  //NS_PRECONDITION(nsnull == mBody, "parser called OpenBody twice");

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenBody", aNode, 
                  mCurrentContext->mStackPos, this);
  // Add attributes, if any, to the current BODY node
  if(mBody != nsnull){
    AddAttributes(aNode,mBody,PR_TRUE);
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);
    return NS_OK;
  }

  // Open body. Note that we pre-append the body to the root so that
  // incremental reflow during document loading will work properly.
  mCurrentContext->SetPreAppend(PR_TRUE);
   nsresult rv = mCurrentContext->OpenContainer(aNode);
  mCurrentContext->SetPreAppend(PR_FALSE);
  if (NS_OK != rv) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }
  mBody = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  NS_ADDREF(mBody);

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenBody()\n"));
  MOZ_TIMER_STOP(mWatch);
  StartLayout();
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseBody(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseBody()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseBody", aNode, 
                  mCurrentContext->mStackPos-1, this);

  PRBool didFlush;
  nsresult rv = mCurrentContext->FlushTextAndRelease(&didFlush);
  if (NS_OK != rv) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseBody()\n"));
    MOZ_TIMER_STOP(mWatch);
    return rv;
  }
  // Flush out anything that's left
  SINK_TRACE(SINK_TRACE_REFLOW,
             ("HTMLContentSink::CloseBody: layout final body content"));
  mCurrentContext->FlushTags(PR_TRUE);
  mCurrentContext->CloseContainer(aNode);

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
  nsIHTMLContent* content = nsnull;
  
  mCurrentContext->FlushTextAndRelease();
  
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenForm", aNode, 
                  mCurrentContext->mStackPos, this);
  
  // Close out previous form if it's there. If there is one
  // around, it's probably because the last one wasn't well-formed.
  NS_IF_RELEASE(mCurrentForm);
  
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

    result = NS_NewHTMLFormElement(&content, nodeInfo);
    if (NS_SUCCEEDED(result) && content) {
      content->QueryInterface(kIDOMHTMLFormElementIID, (void**)&mCurrentForm);
      NS_RELEASE(content);
    }
    
    result = AddLeaf(aNode);
  }
  else {
    // Otherwise the form can be a content parent.
    result = mCurrentContext->OpenContainer(aNode);
    if (NS_SUCCEEDED(result)) {
        
      content = mCurrentContext->GetCurrentContainer();
      if (nsnull != content) {
        result = content->QueryInterface(kIDOMHTMLFormElementIID, 
                                         (void**)&mCurrentForm);
        NS_RELEASE(content);
      }
    }
  }
   
  // add the form to the document
  if (mCurrentForm) {
    mHTMLDocument->AddForm(mCurrentForm);
  }

  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenForm()\n"));
  MOZ_TIMER_STOP(mWatch);
  return result;
}

// XXX MAYBE add code to place close form tag into the content model
// for navigator layout compatability.
NS_IMETHODIMP
HTMLContentSink::CloseForm(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseForm()\n"));
  MOZ_TIMER_START(mWatch);
  nsresult result = NS_OK;

  mCurrentContext->FlushTextAndRelease();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseForm", aNode, 
                  mCurrentContext->mStackPos-1, this);

  if (nsnull != mCurrentForm) {
    // Check if this is a well-formed form
    if (mCurrentContext->IsCurrentContainer(eHTMLTag_form)) {
      result = mCurrentContext->CloseContainer(aNode);
    }
    else if (mCurrentContext->IsAncestorContainer(eHTMLTag_form)) {
      result = mCurrentContext->DemoteContainer(aNode);
    }
    NS_RELEASE(mCurrentForm);
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
                  "HTMLContentSink::OpenFrameset", aNode, 
                  mCurrentContext->mStackPos, this);

  nsresult rv = mCurrentContext->OpenContainer(aNode);
  if ((NS_OK == rv) && (nsnull == mFrameset)) {
    mFrameset = mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
    NS_ADDREF(mFrameset);
  }
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseFrameset(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseFrameset()\n"));
  MOZ_TIMER_START(mWatch);
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseFrameset", aNode, 
                  mCurrentContext->mStackPos-1, this);

  SinkContext* sc = mCurrentContext;
  nsIHTMLContent* fs = sc->mStack[sc->mStackPos-1].mContent;
  PRBool done = fs == mFrameset;
  nsresult rv = sc->CloseContainer(aNode);
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseFrameset()\n"));
  MOZ_TIMER_STOP(mWatch);
  if (done) {
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
                  "HTMLContentSink::OpenMap", aNode, 
                  mCurrentContext->mStackPos, this);
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
HTMLContentSink::CloseMap(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseMap()\n"));
  MOZ_TIMER_START(mWatch);
  nsresult rv = NS_OK;
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseMap", aNode, 
                  mCurrentContext->mStackPos-1, this);
  NS_IF_RELEASE(mCurrentMap);

  rv = mCurrentContext->CloseContainer(aNode);
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseMap()\n"));
  MOZ_TIMER_STOP(mWatch);
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::OpenContainer()\n"));
  MOZ_TIMER_START(mWatch);
  nsresult rv = NS_OK;
  // XXX work around parser bug
  if (eHTMLTag_frameset == aNode.GetNodeType()) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenContainer()\n"));
    MOZ_TIMER_STOP(mWatch);
    return OpenFrameset(aNode);
  }
  rv = mCurrentContext->OpenContainer(aNode);
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::OpenContainer()\n"));
  MOZ_TIMER_STOP(mWatch);
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::CloseContainer()\n"));
  MOZ_TIMER_START(mWatch);
  nsresult rv = NS_OK;
  // XXX work around parser bug
  if (eHTMLTag_frameset == aNode.GetNodeType()) {
    MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::CloseContainer()\n"));
    MOZ_TIMER_STOP(mWatch);
    return CloseFrameset(aNode);
  }
  rv = mCurrentContext->CloseContainer(aNode);
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
nsresult HTMLContentSink::AddComment(const nsIParserNode& aNode) {
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddComment()\n"));
  MOZ_TIMER_START(mWatch);
  nsresult rv = NS_OK;
  rv = mCurrentContext->AddComment(aNode);
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
nsresult HTMLContentSink::AddProcessingInstruction(const nsIParserNode& aNode) {
  nsresult result= NS_OK;  
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
HTMLContentSink::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
  nsresult rv = NS_OK;
  MOZ_TIMER_DEBUGLOG(("Start: nsHTMLContentSink::AddDocTypeDecl()\n"));
  MOZ_TIMER_START(mWatch);
 
  mHTMLDocument->SetDTDMode((nsDTDMode)aMode);

  nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(mHTMLDocument));

  if (!doc)
    return NS_OK;

  const nsString& docTypeStr = aNode.GetText(); 

  PRInt32 publicStart = docTypeStr.Find("PUBLIC", PR_TRUE);
  PRInt32 systemStart = docTypeStr.Find("SYSTEM", PR_TRUE);
  nsAutoString name, publicId, systemId;

  if (publicStart >= 0 || systemStart >= 0) {
    /*
     * If we find the keyword 'PUBLIC' after the keyword 'SYSTEM' we assume
     * that we got a system id that happens to contain the string "PUBLIC"
     * and we ignore that as the start of the public id.
     */
    if (systemStart >= 0 && (publicStart > systemStart))
      publicStart = -1;

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
      PRUnichar ch = publicId.Length() ? publicId.First() : '\0';

      if (ch == '"' || ch == '\'') {
        publicId.Cut(0, 1);

        PRInt32 end = publicId.FindChar(ch);

        if (end < 0) {
          /*
           * We didn't find an end quote, then we just make sure we cut of
           * the '>' on the end of the doctype declaration
           */

          end = publicId.FindChar('>');
        }

        /*
         * If we didn't find an closing quote nor a '>' we leave publicId as
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
        systemStart = pos + publicId.Length() + 1; // 1 is the end quote
      }
    }

    if (systemStart >= 0) {
      // We either found 'SYSTEM' or we have data after the public id
      docTypeStr.Mid(systemId, systemStart,
                     docTypeStr.Length() - systemStart);

      // Strip off 'SYSTEM' if we have it.
      if (systemId.EqualsWithConversion("SYSTEM", PR_TRUE, 6))
        systemId.Cut(0, 6);

      systemId.Trim(" \t\n\r");

      // Strip quotes
      PRUnichar ch = systemId.Length() ? systemId.First() : '\0';

      if (ch == '"' || ch == '\'') {
        systemId.Cut(0, 1);

        PRInt32 end = systemId.FindChar(ch);

        if (end < 0) {
          /*
           * We didn't find an end quote, then we just make sure we cut of the
           * '>' on the end of the doctype declaration
           */

          end = systemId.FindChar('>');
        }

        /*
         * If we found an closing quote nor a '>' we truncate systemId at
         * that length.
         */
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

  /*
   * Cut out "DOCTYPE" or "<!DOCTYPE" from the name.
   */
  if (name.EqualsWithConversion("<!DOCTYPE", PR_TRUE, 9))
    name.Cut(0, 9);
  else if (name.EqualsWithConversion("DOCTYPE", PR_TRUE, 7))
    name.Cut(0, 7);

  name.Trim(" \t\n\r");

  /*
   * Check if name contains whitespace chars, if it does and the first char
   * is not a quote, we make set the name to contain the characters before
   * the whitespace
   */
  PRInt32 nameEnd = 0;

  if (name.IsEmpty() || (name.First() != '"' && name.First() != '\'')) {
    nameEnd = name.FindCharInSet(" \n\r\t");
  }

  /*
   * If we didn't find a public id we grab everything after the name and
   * use that as public id.
   */
  if (publicStart < 0) {
    name.Mid(publicId, nameEnd, name.Length() - nameEnd);
    publicId.Trim(" \t\n\r");

    PRUnichar ch = publicId.Length() ? publicId.First() : '\0';

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
      publicId.Truncate(); // No quotes, no public id
    }
  }

  if (nameEnd >= 0)
    name.Truncate(nameEnd);
  else {
    nameEnd = name.FindChar('>');

    if (nameEnd >= 0)
      name.Truncate(nameEnd);
  }

  if (publicId.Length() || systemId.Length() || name.Length()) {
    nsCOMPtr<nsIDOMDocumentType> oldDocType;
    nsCOMPtr<nsIDOMDocumentType> docType;

    doc->GetDoctype(getter_AddRefs(oldDocType));

    nsCOMPtr<nsIDOMDOMImplementation> domImpl;

    rv = doc->GetImplementation(getter_AddRefs(domImpl));

    if (NS_FAILED(rv) || !domImpl) {
      return rv;
    }

    if (!name.Length()) {
      name.AssignWithConversion("HTML");
    }

    rv = domImpl->CreateDocumentType(name, publicId, systemId,
                                     getter_AddRefs(docType));

    if (NS_FAILED(rv) || !docType) {
      return rv;
    }
    nsCOMPtr<nsIDOMNode> tmpNode;

    if (oldDocType) {
      /*
       * If we already have a doctype we replace the old one.
       */
      rv = doc->ReplaceChild(oldDocType, docType, getter_AddRefs(tmpNode));
    } else {
      /*
       * If we don't already have one we insert it as the first child,
       * this might not be 100% correct but since this is called from
       * the content sink we assume that this is what we want.
       */
      nsCOMPtr<nsIDOMNode> firstChild;

      doc->GetFirstChild(getter_AddRefs(firstChild));

      /*
       * If the above fails it must be because we don't have any child
       * nodes, then firstChild will be 0 and InsertBefore() will append
       */

      rv = doc->InsertBefore(docType, firstChild, getter_AddRefs(tmpNode));
    }
  }
  
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::AddDocTypeDecl()\n"));
  MOZ_TIMER_STOP(mWatch);
  return rv;
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
  if (mWebShell) {
    nsCOMPtr<nsIScrollable> scrollableContainer = do_QueryInterface(mWebShell, &rv);
    if (NS_SUCCEEDED(rv) && scrollableContainer) {
      if (mFrameset) {
        scrollableContainer->SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_Y, NS_STYLE_OVERFLOW_HIDDEN);
        scrollableContainer->SetCurrentScrollbarPreferences(nsIScrollable::ScrollOrientation_X, NS_STYLE_OVERFLOW_HIDDEN);
      } else {
        scrollableContainer->ResetScrollbarPreferences();
      }
    }
  }

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsCOMPtr<nsIPresShell> shell(dont_AddRef(mDocument->GetShellAt(i)));
    if (shell) {
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
  char* ref = nsnull;           // init in case mDocumentURI is not a url
  nsIURL* url;
  rv = mDocumentURI->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
  if (NS_SUCCEEDED(rv)) {
    rv = url->GetRef(&ref);
    NS_RELEASE(url);
  }
  if (rv == NS_OK) {
    mRef = new nsString;
    mRef->AssignWithConversion(ref);
    nsCRT::free(ref);
  }

  if ((nsnull != ref) || mFrameset) {
    // XXX support more than one presentation-shell here

    // Get initial scroll preference and save it away; disable the
    // scroll bars.
    ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsCOMPtr<nsIPresShell> shell(dont_AddRef(mDocument->GetShellAt(i)));
      if (shell) {
        nsCOMPtr<nsIViewManager> vm;
        shell->GetViewManager(getter_AddRefs(vm));
        if (vm) {
          nsIView* rootView = nsnull;
          vm->GetRootView(rootView);
          if (nsnull != rootView) {
            nsIScrollableView* sview = nsnull;
            rootView->QueryInterface(kIScrollableViewIID, (void**) &sview);
            if (nsnull != sview) {
              if (mFrameset)
                mOriginalScrollPreference = nsScrollPreference_kNeverScroll;
              else
                sview->GetScrollPreference(mOriginalScrollPreference);
              sview->SetScrollPreference(nsScrollPreference_kNeverScroll);
            }
          }
        }
      }
    }
  }
}

void
HTMLContentSink::ScrollToRef()
{
  if (mNotAtRef && (nsnull != mRef) && (nsnull != mRefContent)) {
    // See if the ref content has been reflowed by finding its frame
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsCOMPtr<nsIPresShell> shell( dont_AddRef(mDocument->GetShellAt(i)) );
      if (shell) {
        nsIFrame* frame;
        shell->GetPrimaryFrameFor(mRefContent, &frame);
        if (nsnull != frame) {
          nsCOMPtr<nsIViewManager> vm;
          shell->GetViewManager(getter_AddRefs(vm));
          if (vm) {
            nsIScrollableView* sview = nsnull;
            vm->GetRootScrollableView(&sview);

            if (sview) {
              // Determine the x,y scroll offsets for the given
              // frame. The offsets are relative to the
              // ScrollableView's upper left corner so we need frame
              // coordinates that are relative to that.
              nsPoint offset;
              nsIView* view;
              nsCOMPtr<nsIPresContext> presContext;
              shell->GetPresContext(getter_AddRefs(presContext));
              frame->GetOffsetFromView(presContext, offset, &view);
              nscoord x = 0;
              nscoord y = offset.y;
              sview->SetScrollPreference(mOriginalScrollPreference);
              // XXX If view != scrolledView, then there is a scrolled frame,
              // e.g., a DIV with 'overflow' of 'scroll', somewhere in the middle,
              // or maybe an absolutely positioned element that has a view. We
              // need to handle these cases...
              sview->ScrollTo(x, y, NS_VMREFRESH_IMMEDIATE);

              // Note that we did this so that we don't bother doing it again
              mNotAtRef = PR_FALSE;
            }
          }
        }
      }
    }
  }
}

void
HTMLContentSink::AddBaseTagInfo(nsIHTMLContent* aContent)
{
  if (mBaseHREF.Length() > 0) {
    aContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_baseHref, mBaseHREF, PR_FALSE);
  }
  if (mBaseTarget.Length() > 0) {
    aContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::_baseTarget, mBaseTarget, PR_FALSE);
  }
}

nsresult
HTMLContentSink::ProcessATag(const nsIParserNode& aNode,
                             nsIHTMLContent* aContent)
{
  AddBaseTagInfo(aContent);
  if ((nsnull != mRef) && (nsnull == mRefContent)) {
    nsHTMLValue value;
    aContent->GetHTMLAttribute(nsHTMLAtoms::name, value);
    if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString tmp;
      value.GetStringValue(tmp);
      if (mRef->EqualsIgnoreCase(tmp)) {
        // Winner. We just found the content that is the named anchor
        mRefContent = aContent;
        NS_ADDREF(aContent);
      }
    }
  }
  return NS_OK;
}

nsresult
HTMLContentSink::ProcessAREATag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  if (nsnull != mCurrentMap) {
    nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
    nsIHTMLContent* area;
    rv = CreateContentObject(aNode, nodeType, nsnull, nsnull, &area);
    if (NS_FAILED(rv)) {
      return rv;
    }    

    // Set the content's document and attributes
    area->SetDocument(mDocument, PR_FALSE, PR_TRUE);
    rv = AddAttributes(aNode, area);
    if (NS_FAILED(rv)) {
      NS_RELEASE(area);
      return rv;
    }        

    AddBaseTagInfo(area); // basehref or basetarget. Fix. Bug: 30617

    // Add AREA object to the current map
    mCurrentMap->AppendChildTo(area, PR_FALSE);
    NS_RELEASE(area);
  }
  return NS_OK;
}

void 
HTMLContentSink::ProcessBaseHref(const nsString& aBaseHref)
{
  //-- Make sure this page is allowed to load this URL
  nsresult rv;
  NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
  if (NS_FAILED(rv)) return;
  nsCOMPtr<nsIURI> baseHrefURI;
  rv = NS_NewURI(getter_AddRefs(baseHrefURI), aBaseHref, nsnull);
  if (NS_FAILED(rv)) return;
  rv = securityManager->CheckLoadURI(mDocumentBaseURL, baseHrefURI, PR_FALSE);
  if (NS_FAILED(rv)) return;

  if (nsnull == mBody) {  // still in real HEAD
    mHTMLDocument->SetBaseURL(aBaseHref);
    NS_RELEASE(mDocumentBaseURL);
    mDocument->GetBaseURL(mDocumentBaseURL);
  }
  else {  // NAV compatibility quirk
    mBaseHREF = aBaseHref;
  }
}

nsresult
HTMLContentSink::RefreshIfEnabled(nsIViewManager* vm)
{
  if (vm) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
    NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
    nsresult rv = docShell->GetContentViewer(getter_AddRefs(contentViewer));
    if (NS_SUCCEEDED(rv) && (contentViewer)) {
      PRBool enabled;
      contentViewer->GetEnableRendering(&enabled);
      if (enabled) {
        vm->EnableRefresh(NS_VMREFRESH_NO_SYNC);
      }
    }
  }
  return NS_OK;
}

void 
HTMLContentSink::ProcessBaseTarget(const nsString& aBaseTarget)
{
  if (nsnull == mBody) { // still in real HEAD
    mHTMLDocument->SetBaseTarget(aBaseTarget);
  }
  else {  // NAV compatibility quirk
    mBaseTarget = aBaseTarget;
  }
}

nsresult
HTMLContentSink::ProcessBASETag(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsIHTMLContent* parent = nsnull;
  
  if(mCurrentContext!=nsnull) {
    parent=mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
  }
 
  if(parent!=nsnull) {
    // Create content object
    nsCOMPtr<nsIHTMLContent> element;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    mNodeInfoManager->GetNodeInfo(NS_ConvertASCIItoUCS2("base"),
                                  nsnull, kNameSpaceID_None,
                                  *getter_AddRefs(nodeInfo));

    result = NS_CreateHTMLElement(getter_AddRefs(element), nodeInfo);
    if (NS_SUCCEEDED(result)) {
      PRInt32 id;
      mDocument->GetAndIncrementContentID(&id);
      element->SetContentID(id);

      // Add in the attributes and add the style content object to the
      // head container.
      element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      result = AddAttributes(aNode, element);
      if (NS_SUCCEEDED(result)) {
        parent->AppendChildTo(element, PR_FALSE);
        if(!mInsideNoXXXTag) {
          nsAutoString value;
          if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, value)) {
            ProcessBaseHref(value);
          }
          if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, value)) {
            ProcessBaseTarget(value);
          }
        }
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
HTMLContentSink::ProcessLink(nsIHTMLContent* aElement, const nsString& aLinkData)
{
  nsresult result = NS_OK;
  
  // parse link content and call process style link
  nsAutoString href;
  nsAutoString rel;
  nsAutoString title;
  nsAutoString type;
  nsAutoString media;
  PRBool didBlock = PR_FALSE;

  nsAutoString  stringList(aLinkData); // copy to work buffer
  
  stringList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)stringList.GetUnicode();
  PRUnichar* end   = start;
  PRUnichar* last  = start;
  PRUnichar  endCh;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }

    end = start;
    last = end - 1;

    while ((kNullCh != *end) && (kSemiCh != *end) && (kCommaCh != *end)) { // look for semicolon or comma
      if ((kApostrophe == *end) || (kQuote == *end) || 
          (kLessThanCh == *end)) { // quoted string
        PRUnichar quote = *end;
        if (kLessThanCh == quote) {
          quote = kGreaterThanCh;
        }
        PRUnichar* closeQuote = (end + 1);
        while ((kNullCh != *closeQuote) && (quote != *closeQuote)) {
          closeQuote++; // seek closing quote
        }
        if (quote == *closeQuote) { // found closer
          end = closeQuote; // skip to close quote
          last = end - 1;
          if ((kSemiCh != *(end + 1)) && (kNullCh != *(end + 1)) && (kCommaCh != *(end + 1))) {
            *(++end) = kNullCh;     // end string here
            while ((kNullCh != *(end + 1)) && (kSemiCh != *(end + 1)) &&
                   (kCommaCh != *(end + 1))) { // keep going until semi or comma
              end++;
            }
          }
        }
      }
      end++;
      last++;
    }

    endCh = *end;
    *end = kNullCh; // end string here

    if (start < end) {
      if ((kLessThanCh == *start) && (kGreaterThanCh == *last)) {
        *last = kNullCh;
        if (0 == href.Length()) { // first one wins
          href = (start + 1);
          href.StripWhitespace();
        }
      }
      else {
        PRUnichar* equals = start;
        while ((kNullCh != *equals) && (kEqualsCh != *equals)) {
          equals++;
        }
        if (kNullCh != *equals) {
          *equals = kNullCh;
          nsAutoString  attr(start);
          attr.StripWhitespace();

          PRUnichar* value = ++equals;
          while (nsCRT::IsAsciiSpace(*value)) {
            value++;
          }
          if (((kApostrophe == *value) || (kQuote == *value)) &&
              (*value == *last)) {
            *last = kNullCh;
            value++;
          }

          if (attr.EqualsIgnoreCase("rel")) {
            if (0 == rel.Length()) {
              rel = value;
              rel.CompressWhitespace();
            }
          }
          else if (attr.EqualsIgnoreCase("title")) {
            if (0 == title.Length()) {
              title = value;
              title.CompressWhitespace();
            }
          }
          else if (attr.EqualsIgnoreCase("type")) {
            if (0 == type.Length()) {
              type = value;
              type.StripWhitespace();
            }
          }
          else if (attr.EqualsIgnoreCase("media")) {
            if (0 == media.Length()) {
              media = value;
              media.ToLowerCase(); // HTML4.0 spec is inconsistent, make it case INSENSITIVE
            }
          }
        }
      }
    }
    if (kCommaCh == endCh) {  // hit a comma, process what we've got so far
      if (0 < href.Length()) {
        result = ProcessStyleLink(aElement, href, rel, title, type, media);
        if (NS_ERROR_HTMLPARSER_BLOCK == result) {
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

  if (0 < href.Length()) {
    result = ProcessStyleLink(aElement, href, rel, title, type, media);
    if (NS_SUCCEEDED(result) && didBlock) {
      result = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }
  return result;
}

static void ParseLinkTypes(const nsString& aTypes, nsStringArray& aResult)
{
  nsAutoString  stringList(aTypes); // copy to work buffer
  nsAutoString  subStr;

  stringList.ToLowerCase();
  stringList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)stringList.GetUnicode();
  PRUnichar* end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsCRT::IsAsciiSpace(*start)) {  // skip leading space
      start++;
    }

    end = start;

    while ((kNullCh != *end) && (! nsCRT::IsAsciiSpace(*end))) { // look for space
      end++;
    }
    *end = kNullCh; // end string here

    subStr = start;

    if (0 < subStr.Length()) {
      aResult.AppendString(subStr);
    }

    start = ++end;
  }
}

static void SplitMimeType(const nsString& aValue, nsString& aType, nsString& aParams)
{
  aType.Truncate();
  aParams.Truncate();
  PRInt32 semiIndex = aValue.FindChar(PRUnichar(';'));
  if (-1 != semiIndex) {
    aValue.Left(aType, semiIndex);
    aValue.Right(aParams, (aValue.Length() - semiIndex) - 1);
    aParams.StripWhitespace();
  }
  else {
    aType = aValue;
  }
  aType.StripWhitespace();
}

NS_IMETHODIMP
HTMLContentSink::StyleSheetLoaded(nsICSSStyleSheet* aSheet, 
                                  PRBool aDidNotify)
{
  // If there was a notification done for this style sheet, we know
  // that frames have been created for all content seen so far
  // (processing of a new style sheet causes recreation of the frame
  // model).  As a result, all contexts should update their notion of
  // how much frame creation has happened.
  if (aDidNotify) {
    UpdateAllContexts();
  }

  return NS_OK;
}

nsresult
HTMLContentSink::ProcessStyleLink(nsIHTMLContent* aElement,
                                  const nsString& aHref, const nsString& aRel,
                                  const nsString& aTitle, const nsString& aType,
                                  const nsString& aMedia)
{
  nsresult result = NS_OK;

  if (aHref.IsEmpty()) {
    // if href is empty then just bail
    return result;
  }

  nsStringArray linkTypes;
  ParseLinkTypes(aRel, linkTypes);
  PRBool isAlternate = PR_FALSE;

  if (-1 != linkTypes.IndexOf(NS_ConvertASCIItoUCS2("stylesheet"))) {  // is it a stylesheet link?

    if (-1 != linkTypes.IndexOf(NS_ConvertASCIItoUCS2("alternate"))) { // if alternate, does it have title?
      if (0 == aTitle.Length()) { // alternates must have title
        return NS_OK; //return without error, for now
      } else {
        isAlternate = PR_TRUE;
      }
    }

    nsAutoString  mimeType;
    nsAutoString  params;
    SplitMimeType(aType, mimeType, params);

		nsDTDMode mode;
		mHTMLDocument->GetDTDMode(mode);

		PRBool isStyleSheet = PR_FALSE;			// see bug 18817
		if (eDTDMode_strict== mode) {
			if (mimeType.EqualsIgnoreCase("text/css")) {
				isStyleSheet = PR_TRUE;					// strict mode + good mime type
			}
			else {
				if (0 == mimeType.Length()) {
					nsString extension;
					aHref.Right(extension, 4);
					if (extension.EqualsWithConversion(".css")) {
						isStyleSheet = PR_TRUE;			// strict mode + no mime type + '.css' extension
					}
				}
			}
		}
		else {
			if (0 == mimeType.Length() || mimeType.EqualsIgnoreCase("text/css")) {
				isStyleSheet = PR_TRUE;					// quirks mode + good mime type or no mime type at all
			}
		}
		
    if (isStyleSheet) {
      nsIURI* url = nsnull;
      {
        result = NS_NewURI(&url, aHref, mDocumentBaseURL);
      }
      if (NS_OK != result) {
        return NS_OK; // The URL is bad, move along, don't propogate the error (for now)
      }

      if (-1 == linkTypes.IndexOf(NS_ConvertASCIItoUCS2("alternate"))) {
        if (0 < aTitle.Length()) {  // possibly preferred sheet
          if (0 == mPreferredStyle.Length()) {
            mPreferredStyle = aTitle;
            mCSSLoader->SetPreferredSheet(aTitle);
            mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, aTitle);
          }
        }
      }

      PRBool blockParser = kBlockByDefault;
      if (isAlternate) {
        blockParser = PR_FALSE;
      }

      /* NOTE: no longer honoring the important keyword to indicate blocking
               as it is proprietary and unnecessary since all non-alternate 
               will block the parser now  -mja
      if (-1 != linkTypes.IndexOf("important")) {
        blockParser = PR_TRUE;
      }
      */

      PRBool doneLoading;
      result = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, kNameSpaceID_HTML,
                                         mStyleSheetCount++, 
                                         ((blockParser) ? mParser : nsnull),
                                         doneLoading, 
                                         this);
      NS_RELEASE(url);
      if (NS_SUCCEEDED(result) && blockParser && (! doneLoading)) {
        result = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
  }
  return result;
}

nsresult
HTMLContentSink::ProcessLINKTag(const nsIParserNode& aNode)
{
  nsresult  result = NS_OK;
  nsIHTMLContent* parent = nsnull;
  
  if(mCurrentContext!=nsnull) {
    parent=mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
  }
  
  if(parent!=nsnull) {
    // Create content object
    nsIHTMLContent* element = nsnull;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::link, nsnull, kNameSpaceID_HTML,
                                  *getter_AddRefs(nodeInfo));

    result = NS_CreateHTMLElement(&element, nodeInfo);
    if (NS_SUCCEEDED(result)) {
      PRInt32 id;
      mDocument->GetAndIncrementContentID(&id);      
      element->SetContentID(id);

      // Add in the attributes and add the style content object to the
      // head container.    
      element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      result = AddAttributes(aNode, element);
      if (NS_FAILED(result)) {
        NS_RELEASE(element);
        return result;
      }
      parent->AppendChildTo(element, PR_FALSE);
    }
    else {
      return result;
    }
    
    // XXX need prefs. check here.
    if(!mInsideNoXXXTag) {
      PRInt32   i;
      PRInt32   count = aNode.GetAttributeCount();

      nsAutoString href;
      nsAutoString rel; 
      nsAutoString title; 
      nsAutoString type; 
      nsAutoString media; 

      for (i = 0; i < count; i++) {
        const nsString& key = aNode.GetKeyAt(i);
        if (key.EqualsIgnoreCase("href")) {
          GetAttributeValueAt(aNode, i, href);
          href.StripWhitespace();
        }
        else if (key.EqualsIgnoreCase("rel")) {
          GetAttributeValueAt(aNode, i, rel);
          rel.CompressWhitespace();
        }
        else if (key.EqualsIgnoreCase("title")) {
          GetAttributeValueAt(aNode, i, title);
          title.CompressWhitespace();
        }
        else if (key.EqualsIgnoreCase("type")) {
          GetAttributeValueAt(aNode, i, type);
          type.StripWhitespace();
        }
        else if (key.EqualsIgnoreCase("media")) {
          GetAttributeValueAt(aNode, i, media);
          media.ToLowerCase(); // HTML4.0 spec is inconsistent, make it case INSENSITIVE
        }
      }
      if (0 < href.Length()) {
        result = ProcessStyleLink(element, href, rel, title, type, media);
      }
    }
    NS_RELEASE(element);
  }
  return result;
}

nsresult
HTMLContentSink::ProcessMAPTag(const nsIParserNode& aNode,
                               nsIHTMLContent* aContent)
{
  nsresult rv;

  NS_IF_RELEASE(mCurrentMap);

  nsIDOMHTMLMapElement* domMap;
  rv = aContent->QueryInterface(kIDOMHTMLMapElementIID, (void**)&domMap);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Strip out whitespace in the name for navigator compatability
  // XXX NAV QUIRK -- XXX this should be done in the content node, not the sink
  nsHTMLValue name;
  aContent->GetHTMLAttribute(nsHTMLAtoms::name, name);
  if (eHTMLUnit_String == name.GetUnit()) {
    nsAutoString tmp;
    name.GetStringValue(tmp);
    tmp.StripWhitespace();
    name.SetStringValue(tmp);
    aContent->SetHTMLAttribute(nsHTMLAtoms::name, name, PR_FALSE);
  }

  // This is for nav4 compatibility. (Bug 18478)
  // The base tag should only appear in the head,
  // but nav4 allows the base tag in the body as well.
  AddBaseTagInfo(aContent);

  // Don't need to add the map to the document here anymore.
  // The map adds itself
  mCurrentMap = aContent;  
  NS_ADDREF(aContent);
  NS_IF_RELEASE(domMap);

  return rv;
}

nsresult
HTMLContentSink::ProcessMETATag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  nsIHTMLContent* parent = nsnull;
  
  if(mCurrentContext!=nsnull) {
    parent=mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
  }

  if (nsnull != parent) {
    // Create content object
    nsCOMPtr<nsINodeInfo> nodeInfo;
    rv = mNodeInfoManager->GetNodeInfo(NS_ConvertASCIItoUCS2("meta"), nsnull,
                                       kNameSpaceID_None,
                                       *getter_AddRefs(nodeInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    nsIHTMLContent* it;
    rv = NS_NewHTMLMetaElement(&it, nodeInfo);
    if (NS_OK == rv) {
      // Add in the attributes and add the meta content object to the
      // head container.
      it->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      rv = AddAttributes(aNode, it);
      if (NS_OK != rv) {
        NS_RELEASE(it);
        return rv;
      }
      parent->AppendChildTo(it, PR_FALSE);
            
      // XXX It's just not sufficient to check if the parent is head. Also check for
      // the preference.
      if(!mInsideNoXXXTag) {

        // set any HTTP-EQUIV data into document's header data as well as url
        nsAutoString header;
        it->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::httpEquiv, header);
        if (header.Length() > 0) {
          nsAutoString result;
          it->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::content, result);
          if (result.Length() > 0) {
            // XXX necko isn't going to process headers coming in from the parser
            //NS_WARNING("need to fix how necko adds mime headers (in HTMLContentSink::ProcessMETATag)");
          
            nsCOMPtr<nsIDocShell> docShell = do_QueryInterface(mWebShell, &rv);
            if (NS_FAILED(rv)) return rv;

                // see if we have a refresh "header".
            if (!header.CompareWithConversion("refresh", PR_TRUE)) {
                // Refresh headers are parsed with the following format in mind
                // <META HTTP-EQUIV=REFRESH CONTENT="5; URL=http://uri">
                // By the time we are here, the following is true:
                // header = "REFRESH"
                // result = "5; URL=http://uri" // note the URL attribute is
                // optional, if it is absent, the currently loaded url is used.
                // Also note that the second and URL seperator can be either a ';'
                // or a ','. This breaks some websites. The ',' seperator should be
                // illegal but CNN is using it.

                // 
                // We need to handle the following strings.
                //  - X is a set of digits
                //  - URI is either a relative or absolute URI
                //  - FOO is any text
                //
                // "" 
                //  empty string. use the currently loaded URI
                //  and refresh immediately.
                // "X"
                //  Refresh the currently loaded URI in X milliseconds.
                // "X; URI"
                //  Refresh using URI as the destination in X milliseconds.
                // "X; URI; Blah"
                //  Refresh using URI as the destination in X milliseconds,
                //  ignoring "Blah"
                // "URI"
                //  Refresh immediately using URI as the destination.
                // "URI; Blah"
                //  Refresh immediately using URI as the destination,
                //  ignoring "Blah"
                // 
                // Note that we need to remove any tokens wrapping the URI.
                // These tokens currently include spaces, double and single
                // quotes.
 
                // first get our baseURI
                nsCOMPtr<nsIURI> baseURI;
                rv = docShell->GetCurrentURI(getter_AddRefs(baseURI));
                if (NS_FAILED(rv)) return rv;

                PRInt32 millis = -1;
                PRUnichar *uriAttrib = nsnull;
    
                PRInt32 semiColon = result.FindCharInSet(";,");
                nsAutoString token;
                if (semiColon > -1)
                    result.Left(token, semiColon);
                else
                    token = result;
    
                PRBool done = PR_FALSE;
                while (!done && !token.IsEmpty()) {
                    token.CompressWhitespace();
                    // Ref. bug 22886
                    // Apparently CONTENT can also start with a period (.).
                    // Ex: <meta http-equiv = "refresh" content=".1; url=./recommendations1.html">
                    // So let's relax a little bit otherwise http://www.mozillazine.org/resources/
                    // wouldn't get redirected to the correct URL.
                    if (millis == -1 && (nsCRT::IsAsciiDigit(token.First()) || token.First()==PRUnichar('.'))) {
                        PRBool tokenIsANumber = PR_TRUE;
                        nsReadingIterator<PRUnichar> doneIterating(token.EndReading());
                        nsReadingIterator<PRUnichar> iter(token.BeginReading());
                        while ( iter != doneIterating )
                          {
                            if (!(tokenIsANumber = nsCRT::IsAsciiDigit(*iter)) && *iter!=PRUnichar('.'))
                              break;
                            ++iter;
                           }
            
                        if (tokenIsANumber) {
                            PRInt32 err;
                            millis = token.ToInteger(&err) * 1000;
                        } else {
                            done = PR_TRUE;
                        }
                    } else {
                        done = PR_TRUE;
                    }
                    if (done) {
                        PRInt32 loc = token.FindChar('=');
                        if (loc > -1)
                            token.Cut(0, loc+1);
                        token.Trim(" \"'");
                        uriAttrib = token.ToNewUnicode();
                    } else {
                        // Increment to the next token.
                        if (semiColon > -1) {
                            semiColon++;
                            PRInt32 semiColon2 = result.FindCharInSet(";,", semiColon);
                            if (semiColon2 == -1) semiColon2 = result.Length();
                            result.Mid(token, semiColon, semiColon2 - semiColon);
                            semiColon = semiColon2;
                        } else {
                            done = PR_TRUE;
                        }
                    }
                } // end while

                nsCOMPtr<nsIURI> uri;
                if (!uriAttrib) {
                    uri = baseURI;
                } else {
                    rv = NS_NewURI(getter_AddRefs(uri), nsAutoString(uriAttrib), baseURI);
                    nsMemory::Free(uriAttrib);
                }

                if (NS_SUCCEEDED(rv)) {
                    NS_WITH_SERVICE(nsIScriptSecurityManager,
                                    securityManager, 
                                    NS_SCRIPTSECURITYMANAGER_PROGID,
                                    &rv);
                    if (NS_SUCCEEDED(rv)) {
                        rv = securityManager->CheckLoadURI(baseURI,
                                                           uri,
                                                           PR_TRUE);
                        if (NS_SUCCEEDED(rv)) {
                            nsCOMPtr<nsIRefreshURI> reefer = 
                                do_QueryInterface(mWebShell);
                            if (reefer) {
                                if (millis == -1) millis = 0;
                                rv = reefer->RefreshURI(uri, millis, 
                                                        PR_FALSE);
                                if (NS_FAILED(rv)) return rv;
                            }
                        }
                    }
                }
            } // END refresh
            else if (!header.CompareWithConversion("set-cookie", PR_TRUE)) {
                nsCOMPtr<nsICookieService> cookieServ = do_GetService(NS_COOKIESERVICE_PROGID, &rv);
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIURI> baseURI;
                rv = docShell->GetCurrentURI(getter_AddRefs(baseURI));
                if (NS_FAILED(rv)) return rv;

                rv = cookieServ->SetCookieString(baseURI, result);
                if (NS_FAILED(rv)) return rv;
            } // END set-cookie

            header.ToLowerCase();
            nsIAtom* fieldAtom = NS_NewAtom(header);
            mDocument->SetHeaderData(fieldAtom, result);

            if (fieldAtom == nsHTMLAtoms::headerDefaultStyle) {
              mPreferredStyle = result;
              mCSSLoader->SetPreferredSheet(mPreferredStyle);
            }
            else if (fieldAtom == nsHTMLAtoms::link) {
              rv = ProcessLink(it, result);
            }
            else if (fieldAtom == nsHTMLAtoms::headerContentBase) {
              ProcessBaseHref(result);
            }
            else if (fieldAtom == nsHTMLAtoms::headerWindowTarget) {
              ProcessBaseTarget(result);
            }
            NS_IF_RELEASE(fieldAtom);
          }//if (result.Length() > 0) 
        }//if (header.Length() > 0) 
      }//if(!mInsideNoXXXTag)
      NS_RELEASE(it);
    }//if (NS_OK == rv) 
  }//if (nsnull != parent)

  return rv;
}

// Returns PR_TRUE if the language name is a version of JavaScript and
// PR_FALSE otherwise
static PRBool
IsJavaScriptLanguage(const nsString& aName, const char* *aVersion)
{
  JSVersion version = JSVERSION_UNKNOWN;

  if (aName.EqualsIgnoreCase("JavaScript") ||
      aName.EqualsIgnoreCase("LiveScript") ||
      aName.EqualsIgnoreCase("Mocha")) {
    version = JSVERSION_DEFAULT;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.1")) {
    version = JSVERSION_1_1;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.2")) {
    version = JSVERSION_1_2;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.3")) {
    version = JSVERSION_1_3;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.4")) {
    version = JSVERSION_1_4;
  }
  else if (aName.EqualsIgnoreCase("JavaScript1.5")) {
    version = JSVERSION_1_5;
  }
  if (version == JSVERSION_UNKNOWN)
    return PR_FALSE;
  *aVersion = JS_VersionToString(version);
  return PR_TRUE;
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
  if (eHTMLTag_tr == aTag || eHTMLTag_select == aTag) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

PRBool
HTMLContentSink::IsTimeToNotify()
{
  if (!mNotifyOnTimer || !mLayoutStarted || !mBackoffCount || mInMonolithicContainer) {
    return PR_FALSE;
  }

  PRTime now = PR_Now();
  PRInt64 interval, diff;
  
  LL_I2L(interval, mNotificationInterval);
  LL_SUB(diff, now, mLastNotificationTime);

  if (LL_CMP(diff, >, interval)) {
    mBackoffCount--;
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
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

nsresult
HTMLContentSink::ResumeParsing()
{
  nsresult result=NS_OK;
  if (mParser) {
    result=mParser->EnableParser(PR_TRUE);
  }
  
  return result;
}

PRBool
HTMLContentSink::PreEvaluateScript()
{
  // Eagerly append all pending elements (including the current body child)
  // to the body (so that they can be seen by scripts) and force reflow.
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::PreEvaluateScript: flushing tags before evaluating script"));
  mCurrentContext->FlushTags(PR_FALSE);
  mCurrentContext->SetPreAppend(PR_TRUE);

  mInScript++;
  

  return PR_TRUE;
}

void
HTMLContentSink::PostEvaluateScript(PRBool aBodyPresent)
{
  mInScript--;
  mCurrentContext->SetPreAppend(PR_FALSE);
}

PRBool
HTMLContentSink::IsInScript()
{
  return (mInScript > 0);
}

nsresult
HTMLContentSink::EvaluateScript(nsString& aScript,
                                nsIURI *aScriptURI,
                                PRInt32 aLineNo,
                                const char* aVersion)
{
  nsresult rv = NS_OK;

  if (aScript.Length() > 0) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
    NS_ENSURE_TRUE(globalObject, NS_ERROR_FAILURE);

    nsCOMPtr<nsIScriptContext> context;
    NS_ENSURE_SUCCESS(globalObject->GetContext(getter_AddRefs(context)),
      NS_ERROR_FAILURE);

    nsCOMPtr<nsIPrincipal> principal;
    mDocument->GetPrincipal(getter_AddRefs(principal));
    NS_ASSERTION(principal, "principal required for document");

    nsAutoString ret;
    char* url = nsnull;

    if (aScriptURI) {
      (void)aScriptURI->GetSpec(&url);
    }
  
    PRBool isUndefined;
    context->EvaluateString(aScript, nsnull, principal, url,
                            aLineNo, aVersion, ret, &isUndefined);
    
    if (url) {
      nsCRT::free(url);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::OnStreamComplete(nsIStreamLoader* aLoader,
                                  nsISupports* aContext,
                                  nsresult aStatus,
                                  PRUint32 stringLen,
                                  const char* string)
{
  nsresult rv = NS_OK;
  
  if (stringLen) {

    PRUnichar *unicodeString = nsnull;
    PRInt32 unicodeLength = 0;
    nsAutoString characterSet;
    nsICharsetConverterManager  *charsetConv = nsnull;
    nsCOMPtr<nsIUnicodeDecoder> unicodeDecoder;
    nsIAtom* contentTypeKey = NS_NewAtom("content-type");
    nsXPIDLCString contenttypeheader;
    nsCOMPtr<nsIHTTPChannel> httpChannel;

    nsCOMPtr<nsIChannel> channel;
    rv = aLoader->GetChannel(getter_AddRefs(channel));
    NS_ASSERTION(channel, "StreamLoader's channel went away prematurely");

    if (channel) {
      httpChannel = do_QueryInterface(channel);
      if (httpChannel) {
        rv = httpChannel->GetResponseHeader(contentTypeKey, getter_Copies(contenttypeheader));
        NS_RELEASE(contentTypeKey);
      }
    }

    if (NS_SUCCEEDED(rv)) {

        nsAutoString contentType; contentType.AssignWithConversion( NS_STATIC_CAST(const char*, contenttypeheader) );
        PRInt32 start = contentType.RFind("charset=", PR_TRUE ) ;

        if(kNotFound != start)
        {
          start += 8; // 8 = "charset=".length
          PRInt32 end = contentType.FindCharInSet(";\n\r ", start  );
          if(kNotFound == end ) end = contentType.Length();

          contentType.Mid(characterSet, start, end - start);
          NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &rv);
          if(NS_SUCCEEDED(rv) && (nsnull != calias) )
          {
           nsAutoString preferred;
           rv = calias->GetPreferred(characterSet, preferred);
           if(NS_SUCCEEDED(rv))
           {
              characterSet = preferred;
           }
          }
        }
      }
      
    if (NS_FAILED(rv) || (characterSet.Length() == 0)) {
      //charset from script charset tag
      characterSet.Assign(mScriptCharset);
    }

    if (NS_FAILED(rv) || (characterSet.Length() == 0) ) {
      // charset from document default
      rv = mDocument->GetDocumentCharacterSet(characterSet);
    }

    NS_ASSERTION(NS_SUCCEEDED(rv), "Could not get document charset!");

    rv = nsServiceManager::GetService(kCharsetConverterManagerCID, 
                   NS_GET_IID(nsICharsetConverterManager), 
                   (nsISupports**)&charsetConv);

    if (NS_SUCCEEDED(rv) && (charsetConv))
    {
       rv = charsetConv->GetUnicodeDecoder(&characterSet,
           getter_AddRefs(unicodeDecoder));
       NS_RELEASE(charsetConv);
    }

    // converts from the charset to unicode
    if (NS_SUCCEEDED(rv)) {
      rv = unicodeDecoder->GetMaxLength(string, stringLen, &unicodeLength);
      if (NS_SUCCEEDED(rv)) {
          mUnicodeXferBuf.SetCapacity(unicodeLength);
          unicodeString = (PRUnichar *) mUnicodeXferBuf.GetUnicode();
          rv = unicodeDecoder->Convert(string, (PRInt32 *) &stringLen, unicodeString, &unicodeLength);
          if (NS_SUCCEEDED(rv)) {
            mUnicodeXferBuf.SetLength(unicodeLength);
          } else {
            mUnicodeXferBuf.SetLength(0);
          }
      }
    }

    NS_ASSERTION(NS_SUCCEEDED(rv), "Could not convert external JavaScript to Unicode!");

    if ((NS_OK == aStatus) && (NS_SUCCEEDED(rv))) {

      nsAutoString jsUnicodeBuffer(CBufDescriptor(unicodeString, PR_TRUE, unicodeLength+1, unicodeLength));
      PRBool bodyPresent = PreEvaluateScript();

      //-- Merge the principal of the script file with that of the document
      nsCOMPtr<nsISupports> owner;
      channel->GetOwner(getter_AddRefs(owner));
      nsCOMPtr<nsIPrincipal> prin;
      if (owner)
      {
        prin = do_QueryInterface(owner, &rv);
        if (NS_FAILED(rv)) return rv;
      }
      rv = mDocument->AddPrincipal(prin);
      if (NS_FAILED(rv)) return rv;

      rv = EvaluateScript(jsUnicodeBuffer, mScriptURI, 1, mScriptLanguageVersion);
      if (NS_FAILED(rv)) return rv;

      PostEvaluateScript(bodyPresent);

    }
  }

  rv = ResumeParsing();
  if (NS_FAILED(rv)) return rv;

  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
  //invalidate Xfer buffer content
  mUnicodeXferBuf.SetLength(0); 

  return rv;
}

nsresult
HTMLContentSink::ProcessSCRIPTTag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  PRBool isJavaScript = PR_TRUE;
  const char* jsVersionString = nsnull;
  PRInt32 i, ac = aNode.GetAttributeCount();

  // Look for SRC attribute and look for a LANGUAGE attribute
  nsAutoString src;
  for (i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("src")) {
      GetAttributeValueAt(aNode, i, src);
    }
    else if (key.EqualsIgnoreCase("type")) {
      nsAutoString  type;

      GetAttributeValueAt(aNode, i, type);

      nsAutoString  mimeType;
      nsAutoString  params;
      SplitMimeType(type, mimeType, params);

      isJavaScript = mimeType.EqualsIgnoreCase("application/x-javascript") || 
                     mimeType.EqualsIgnoreCase("text/javascript");
      if (isJavaScript) {
        JSVersion jsVersion = JSVERSION_DEFAULT;
        if (params.Find("version=", PR_TRUE) == 0) {
          if (params.Length() != 11 || params[8] != '1' || params[9] != '.')
            jsVersion = JSVERSION_UNKNOWN;
          else switch (params[10]) {
            case '0': jsVersion = JSVERSION_1_0; break;
            case '1': jsVersion = JSVERSION_1_1; break;
            case '2': jsVersion = JSVERSION_1_2; break;
            case '3': jsVersion = JSVERSION_1_3; break;
            case '4': jsVersion = JSVERSION_1_4; break;
            case '5': jsVersion = JSVERSION_1_5; break;
            default:  jsVersion = JSVERSION_UNKNOWN;
          }
        }
        jsVersionString = JS_VersionToString(jsVersion);
      }
    }
    else if (key.EqualsIgnoreCase("language")) {
      nsAutoString  lang;
       
      GetAttributeValueAt(aNode, i, lang);
      isJavaScript = IsJavaScriptLanguage(lang, &jsVersionString);
    }
    else if (key.EqualsIgnoreCase("charset")) {
      //charset from script charset tag
      nsAutoString  charset;

      GetAttributeValueAt(aNode, i, charset);
      NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &rv);
      if(NS_SUCCEEDED(rv) && (nsnull != calias) )
      {
       rv = calias->GetPreferred(charset, mScriptCharset);
      } else {
       mScriptCharset = charset;
      }
    }
  }

  // Create content object
  NS_ASSERTION(mCurrentContext->mStackPos > 0, "leaf w/o container");
  if (mCurrentContext->mStackPos <= 0) {
    return NS_ERROR_FAILURE;
  }
  nsIHTMLContent* parent = mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
  nsIHTMLContent* element = nsnull;
  nsCOMPtr<nsINodeInfo> nodeInfo;
  mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::script, nsnull, kNameSpaceID_None,
                                *getter_AddRefs(nodeInfo));

  rv = NS_CreateHTMLElement(&element, nodeInfo);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 id;
    mDocument->GetAndIncrementContentID(&id);    
    element->SetContentID(id);

    // Add in the attributes and add the style content object to the
    // head container.
    element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
    rv = AddAttributes(aNode, element);
    if (NS_FAILED(rv)) {
      NS_RELEASE(element);
      return rv;
    }
    if (mCurrentContext->mStack[mCurrentContext->mStackPos-1].mInsertionPoint != -1) {
      parent->InsertChildAt(element, 
                            mCurrentContext->mStack[mCurrentContext->mStackPos-1].mInsertionPoint++, 
                            PR_FALSE);
    }
    else {
      parent->AppendChildTo(element, PR_FALSE);
    }
  }
  else {
    return rv;
  }
  
  // Create a text node holding the content
  // First, get the text content of the script tag
  nsAutoString script;
  script.Assign(aNode.GetSkippedContent());

  if (script.Length() > 0) {
    nsIContent* text;
    rv = NS_NewTextNode(&text);
    if (NS_OK == rv) {
      nsIDOMText* tc;
      rv = text->QueryInterface(kIDOMTextIID, (void**)&tc);
      if (NS_OK == rv) {
        tc->SetData(script);
        NS_RELEASE(tc);
      }
      element->AppendChildTo(text, PR_FALSE);
      text->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      NS_RELEASE(text);
    }
  }
  NS_RELEASE(element);

  // Don't include script loading and evaluation in the stopwatch
  // that is measuring content creation time
  MOZ_TIMER_DEBUGLOG(("Stop: nsHTMLContentSink::ProcessSCRIPTTag()\n"));
  MOZ_TIMER_STOP(mWatch);

  // Don't process scripts that aren't JavaScript and don't process
  // scripts that are inside iframes, noframe, or noscript tags.
  if (isJavaScript && !mNumOpenIFRAMES && !mInsideNoXXXTag) {
    mScriptLanguageVersion = jsVersionString;

    // If there is a SRC attribute...
    if (src.Length() > 0) {
      // Use the SRC attribute value to load the URL
      {
        rv = NS_NewURI(getter_AddRefs(mScriptURI), src, mDocumentBaseURL);
      }
      if (NS_OK != rv) {
        return rv;
      }

      // Check that this page is allowed to load this URI.
      NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                      NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
      if (NS_FAILED(rv)) 
          return rv;
      rv = securityManager->CheckLoadURI(mDocumentBaseURL, mScriptURI, PR_FALSE);
      if (NS_FAILED(rv)) 
          return rv;

      // After the security manager, the content-policy stuff gets a veto
      // For pinkerton: a symphony for string conversion, in 3 parts.
      nsXPIDLCString urlCString;
      mScriptURI->GetSpec(getter_Copies(urlCString));
      nsAutoString url;
      url.AssignWithConversion((const char *)urlCString);
      nsCOMPtr<nsIDOMElement> DOMElement = do_QueryInterface(element, &rv);
      
      PRBool shouldLoad = PR_TRUE;
      if (NS_SUCCEEDED(rv) &&
          (rv = NS_CheckContentLoadPolicy(nsIContentPolicy::CONTENT_SCRIPT,
                                          url, DOMElement, &shouldLoad),
           NS_SUCCEEDED(rv)) &&
          !shouldLoad) {

        // content-policy veto causes silent failure
        return NS_OK;
      }

      nsCOMPtr<nsILoadGroup> loadGroup;
      nsIStreamLoader* loader;

      mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));

      // supply a prompter if you have one. this allows modal dialogs put up
      // from within this new stream loader to have proper parenting. but it's
      // not fatal if there isn't a prompter.
      nsCOMPtr<nsIInterfaceRequestor> promptcall(do_QueryInterface(mWebShell));
      rv = NS_NewStreamLoader(&loader, mScriptURI, this, nsnull, loadGroup,
                              promptcall, nsIChannel::FORCE_RELOAD);
      if (NS_OK == rv) {
        rv = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
    else {
      PRBool bodyPresent = PreEvaluateScript();

      PRUint32 lineNo = (PRUint32)aNode.GetSourceLineNumber();
      nsIURI *docURI = mDocument->GetDocumentURL();

      EvaluateScript(script, docURI, lineNo, jsVersionString);
      NS_IF_RELEASE(docURI);

      PostEvaluateScript(bodyPresent);

      // If the parse was disabled as a result of this evaluate script
      // (for example, if the script document.wrote a SCRIPT SRC= tag,
      // we remind the parser to block.
      if ((nsnull != mParser) && (PR_FALSE == mParser->IsParserEnabled())) {
        rv = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
  }

  return rv;
}


// 3 ways to load a style sheet: inline, style src=, link tag
// XXX What does nav do if we have SRC= and some style data inline?

nsresult
HTMLContentSink::ProcessSTYLETag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  nsIHTMLContent* parent = nsnull;
  
  if(mCurrentContext!=nsnull) {
    parent=mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
  }

  if(parent!=nsnull) {
    // Create content object
    nsIHTMLContent* element = nsnull;
    nsCOMPtr<nsINodeInfo> nodeInfo;
    mNodeInfoManager->GetNodeInfo(nsHTMLAtoms::style, nsnull,
                                  kNameSpaceID_None,
                                  *getter_AddRefs(nodeInfo));

    rv = NS_CreateHTMLElement(&element, nodeInfo);
    if (NS_SUCCEEDED(rv)) {
      PRInt32 id;
      mDocument->GetAndIncrementContentID(&id);    
      element->SetContentID(id);

      // Add in the attributes and add the style content object to the
      // head container.
      element->SetDocument(mDocument, PR_FALSE, PR_TRUE);
      rv = AddAttributes(aNode, element);
      if (NS_FAILED(rv)) {
        NS_RELEASE(element);
        return rv;
      }
      parent->AppendChildTo(element, PR_FALSE);
    }

    if(!mInsideNoXXXTag && NS_SUCCEEDED(rv)) {

      PRInt32 i, count = aNode.GetAttributeCount();

      nsAutoString src;
      nsAutoString title; 
      nsAutoString type; 
      nsAutoString media; 

      for (i = 0; i < count; i++) {
        const nsString& key = aNode.GetKeyAt(i);
        if (key.EqualsIgnoreCase("src")) {
          GetAttributeValueAt(aNode, i, src);
          src.StripWhitespace();
        }
        else if (key.EqualsIgnoreCase("title")) {
          GetAttributeValueAt(aNode, i, title);
          title.CompressWhitespace();
        }
        else if (key.EqualsIgnoreCase("type")) {
          GetAttributeValueAt(aNode, i, type);
          type.StripWhitespace();
        }
        else if (key.EqualsIgnoreCase("media")) {
          GetAttributeValueAt(aNode, i, media);
          media.ToLowerCase(); // HTML4.0 spec is inconsistent, make it case INSENSITIVE
        }
      }


      nsAutoString  mimeType;
      nsAutoString  params;
      SplitMimeType(type, mimeType, params);

      PRBool blockParser = kBlockByDefault;

      if ((0 == mimeType.Length()) || mimeType.EqualsIgnoreCase("text/css")) { 

        if (0 < title.Length()) {  // possibly preferred sheet
          if (0 == mPreferredStyle.Length()) {
            mPreferredStyle = title;
            mCSSLoader->SetPreferredSheet(title);
            mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, title);
          }
        }

        // The skipped content contains the inline style data
        const nsString& content = aNode.GetSkippedContent();
        PRBool doneLoading = PR_FALSE;

        nsIUnicharInputStream* uin = nsnull;
        if (0 == src.Length()) {

          // Create a text node holding the content
          nsIContent* text;
          rv = NS_NewTextNode(&text);
          if (NS_OK == rv) {
            nsIDOMText* tc;
            rv = text->QueryInterface(kIDOMTextIID, (void**)&tc);
            if (NS_OK == rv) {
              tc->SetData(content);
              NS_RELEASE(tc);
            }
            element->AppendChildTo(text, PR_FALSE);
            text->SetDocument(mDocument, PR_FALSE, PR_TRUE);
            NS_RELEASE(text);
          }

          // Create a string to hold the data and wrap it up in a unicode
          // input stream.
          rv = NS_NewStringUnicharInputStream(&uin, new nsString(content));
          if (NS_OK != rv) {
            return rv;
          }

          // Now that we have a url and a unicode input stream, parse the
          // style sheet.
          rv = mCSSLoader->LoadInlineStyle(element, uin, title, media, kNameSpaceID_HTML,
                                           mStyleSheetCount++, 
                                           ((blockParser) ? mParser : nsnull),
                                           doneLoading, this);
          NS_RELEASE(uin);
        } 
        else {
          // src with immediate style data doesn't add up
          // XXX what does nav do?
          // Use the SRC attribute value to load the URL
          nsIURI* url = nsnull;
          {
            rv = NS_NewURI(&url, src, mDocumentBaseURL);
          }
          if (NS_OK != rv) {
            return rv;
          }

          rv = mCSSLoader->LoadStyleLink(element, url, title, media, kNameSpaceID_HTML,
                                         mStyleSheetCount++, 
                                         ((blockParser) ? mParser : nsnull), 
                                         doneLoading, this);
          NS_RELEASE(url);
        }
        if (NS_SUCCEEDED(rv) && blockParser && (! doneLoading)) {
          rv = NS_ERROR_HTMLPARSER_BLOCK;
        }
      }//if ((0 == mimeType.Length()) || mimeType.EqualsIgnoreCase("text/css"))
    }//if(!mInsideNoXXXTag && NS_SUCCEEDED(rv))
    NS_RELEASE(element);
  }//if(parent!=nsnull)

  return rv;
}


NS_IMETHODIMP 
HTMLContentSink::NotifyError(const nsParserError* aError)
{
  // Errors in HTML? Who would have thought!
  // Why are you telling us, parser. Deal with it yourself.  
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::FlushPendingNotifications()
{
  nsresult result = NS_OK;
  // Only flush tags if we're not doing the notification ourselves
  // (since we aren't reentrant) and if we're in a script (since 
  // we only care to flush if this is done via script).
  // Bug 4891: Also flush outside of <script> tags - if script is executing
  //           after the page has finished loading, and a document.write
  //           occurs, we might need this flush.
  if (mCurrentContext && !mInNotification) {
    result = mCurrentContext->FlushTags();
  }

  return result;
}

NS_IMETHODIMP
HTMLContentSink::DoFragment(PRBool aFlag) 
{
  return NS_OK; 
}

/**
 *  This will dump content model into the output file.
 *  
 *  @update  harishd 05/25/00
 *  @param   
 *  @return  NS_OK all went well else ERROR
 */

NS_IMETHODIMP
HTMLContentSink::DumpContentModel() 
{
  nsresult result=NS_OK;
  FILE* out=::fopen("rtest_html.txt", "a");
  if(out!=nsnull) {
    if(mDocument) {
      nsIContent* root = mDocument->GetRootContent();
      if(root) {
        if(mDocumentURI) {
          char* buff[1]={0};
          mDocumentURI->GetSpec(buff);
          fputs(buff[0],out);
        }
        fputs(";",out);
        result=root->DumpContent(out,0,PR_FALSE);
        fputs(";\n",out);
        NS_RELEASE(root);
      }
    }
    fclose(out);
  }
  return result;
}

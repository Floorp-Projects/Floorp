/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "nsIHTMLContentSink.h"
#include "nsIParser.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSLoader.h"
#include "nsIUnicharInputStream.h"
#include "nsIHTMLContent.h"
#include "nsIHTMLContentContainer.h"
#include "nsIURL.h"
#include "nsIUnicharStreamLoader.h"
#ifdef NECKO
#include "nsNeckoUtil.h"
#else
#include "nsIURLGroup.h"
#include "nsIHttpURL.h"
#endif // NECKO
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsIContentViewer.h"
#include "nsHTMLTokens.h"  
#include "nsHTMLEntities.h" 
#include "nsCRT.h"
#include "prtime.h"
#include "prlog.h"

#include "nsHTMLParts.h"
#include "nsIHTMLElementFactory.h"
#include "nsITextContent.h"

#include "nsIDOMText.h"
#include "nsIDOMComment.h"

#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIFormControl.h"

#include "nsIComponentManager.h"

#include "nsIScrollableView.h"
#include "nsHTMLAtoms.h"
#include "nsIFrame.h"

#include "nsIWebShell.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLMapElement.h"
#ifdef NECKO
#include "nsIRefreshURI.h"
#endif //NECKO
#include "nsVoidArray.h"
#include "nsIScriptContextOwner.h"
#include "nsHTMLIIDs.h"

// XXX Go through a factory for this one
#include "nsICSSParser.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsIDOMHTMLTitleElement.h"

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
#ifndef NECKO
static NS_DEFINE_IID(kIHTTPURLIID, NS_IHTTPURL_IID);
#endif
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIHTMLContentContainerIID, NS_IHTMLCONTENTCONTAINER_IID);
static NS_DEFINE_IID(kIStreamListenerIID, NS_ISTREAMLISTENER_IID);
static NS_DEFINE_IID(kIStyleSheetLinkingElementIID, NS_ISTYLESHEETLINKINGELEMENT_IID);

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

#define SINK_TRACE_NODE(_bit,_msg,_node) SinkTraceNode(_bit,_msg,_node,this)

static void
SinkTraceNode(PRUint32 aBit,
              const char* aMsg,
              const nsIParserNode& aNode,
              void* aThis)
{
  if (SINK_LOG_TEST(gSinkLogModuleInfo,aBit)) {
    char cbuf[40];
    const char* cp;
    PRInt32 nt = aNode.GetNodeType();
    if ((nt > PRInt32(eHTMLTag_unknown)) &&
        (nt < PRInt32(eHTMLTag_text))) {
      cp = nsHTMLTags::GetStringValue(nsHTMLTag(aNode.GetNodeType()));
    } else {
      aNode.GetText().ToCString(cbuf, sizeof(cbuf));
      cp = cbuf;
    }
    PR_LogPrint("%s: this=%p node='%s'", aMsg, aThis, cp);
  }
}

#else
#define SINK_TRACE(_bit,_args)
#define SINK_TRACE_NODE(_bit,_msg,_node)
#endif

//----------------------------------------------------------------------

class SinkContext;

class HTMLContentSink : public nsIHTMLContentSink {
public:
  HTMLContentSink();
  virtual ~HTMLContentSink();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsresult Init(nsIDocument* aDoc,
                nsIURI* aURL,
                nsIWebShell* aContainer);

  // nsISupports
  NS_DECL_ISUPPORTS

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

  nsIDocument* mDocument;
  nsIHTMLDocument* mHTMLDocument;
  nsIURI* mDocumentURL;
  nsIURI* mDocumentBaseURL;
  nsIWebShell* mWebShell;
  nsIParser* mParser;

  nsIHTMLContent* mRoot;
  nsIHTMLContent* mBody;
  PRInt32         mBodyChildCount;
  nsIHTMLContent* mFrameset;
  nsIHTMLContent* mHead;
  nsString* mTitle;

  PRInt32 mInMonolithicContainer;
  PRBool mDirty;
  PRBool mLayoutStarted;
  nsIDOMHTMLFormElement* mCurrentForm;
  nsIHTMLContent* mCurrentMap;
  nsIDOMHTMLMapElement* mCurrentDOMMap;

  SinkContext** mContexts;
  PRInt32 mNumContexts;
  nsVoidArray mContextStack;
  SinkContext* mCurrentContext;
  SinkContext* mHeadContext;

  nsString* mRef;
  nsScrollPreference mOriginalScrollPreference;
  PRBool mNotAtRef;
  nsIHTMLContent* mRefContent;

  nsString mBaseHREF;
  nsString mBaseTarget;

  nsString mPreferredStyle;
  PRInt32 mStyleSheetCount;
  nsICSSLoader* mCSSLoader;

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
                          PRInt32 aLineNo);

  void NotifyBody() {
    PRInt32 currentCount;
    mBody->ChildCount(currentCount);
    if (mBodyChildCount < currentCount) {
      mDocument->ContentAppended(mBody, mBodyChildCount);
    }
    mBodyChildCount = currentCount;
  }
#ifdef DEBUG
  void ForceReflow() {
    NotifyBody();
    mDirty = PR_FALSE;
  }
#endif
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

  nsresult Begin(nsHTMLTag aNodeType, nsIHTMLContent* aRoot);
  nsresult OpenContainer(const nsIParserNode& aNode);
  nsresult CloseContainer(const nsIParserNode& aNode);
  nsresult AddLeaf(const nsIParserNode& aNode);
  nsresult AddLeaf(nsIHTMLContent* aContent);
  nsresult AddComment(const nsIParserNode& aNode);
  nsresult DemoteContainer(const nsIParserNode& aNode, 
                           nsIHTMLContent*& aParent);
  nsresult End();

  nsresult GrowStack();
  nsresult AddText(const nsString& aText);
  nsresult FlushText(PRBool* aDidFlush = nsnull);
  nsresult FlushTags();

  PRBool   IsCurrentContainer(nsHTMLTag mType);
  PRBool   IsAncestorContainer(nsHTMLTag mType);
  nsIHTMLContent* GetCurrentContainer();

  void MaybeMarkSinkDirty();
  void MaybeMarkSinkClean();

  HTMLContentSink* mSink;
  PRBool mPreAppend;

  struct Node {
    nsHTMLTag mType;
    nsIHTMLContent* mContent;
    PRUint32 mFlags;
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

static void
ReduceEntities(nsString& aString)
{
  // Reduce any entities
  // XXX Note: as coded today, this will only convert well formed
  // entities.  This may not be compatible enough.
  // XXX there is a table in navigator that translates some numeric entities
  // should we be doing that? If so then it needs to live in two places (bad)
  // so we should add a translate numeric entity method from the parser...
  char cbuf[100];
  PRInt32 i = 0;
  while (i < aString.Length()) {
    // If we have the start of an entity (and it's not at the end of
    // our string) then translate the entity into it's unicode value.
    if ((aString.CharAt(i++) == '&') && (i < aString.Length())) {
      PRInt32 start = i - 1;
      PRUnichar e = aString.CharAt(i);
      if (e == '#') {
        // Convert a numeric character reference
        i++;
        char* cp = cbuf;
        char* limit = cp + sizeof(cbuf) - 1;
        PRBool ok = PR_FALSE;
        PRInt32 slen = aString.Length();
        while ((i < slen) && (cp < limit)) {
          e = aString.CharAt(i);
          if (e == ';') {
            i++;
            ok = PR_TRUE;
            break;
          }
          if ((e >= '0') && (e <= '9')) {
            *cp++ = char(e);
            i++;
            continue;
          }
          break;
        }
        if (!ok || (cp == cbuf)) {
          continue;
        }
        *cp = '\0';
        if (cp - cbuf > 5) {
          continue;
        }
        PRInt32 ch = PRInt32( ::atoi(cbuf) );
        if (ch > 65535) {
          continue;
        }

        // Remove entity from string and replace it with the integer
        // value.
        aString.Cut(start, i - start);
        aString.Insert(PRUnichar(ch), start);
        i = start + 1;
      }
      else if (((e >= 'A') && (e <= 'Z')) ||
               ((e >= 'a') && (e <= 'z'))) {
        // Convert a named entity
        i++;
        char* cp = cbuf;
        char* limit = cp + sizeof(cbuf) - 1;
        *cp++ = char(e);
        PRBool ok = PR_FALSE;
        PRInt32 slen = aString.Length();
        while ((i < slen) && (cp < limit)) {
          e = aString.CharAt(i);
          if (e == ';') {
            i++;
            ok = PR_TRUE;
            break;
          }
          if (((e >= '0') && (e <= '9')) ||
              ((e >= 'A') && (e <= 'Z')) ||
              ((e >= 'a') && (e <= 'z'))) {
            *cp++ = char(e);
            i++;
            continue;
          }
          break;
        }
        if (!ok || (cp == cbuf)) {
          continue;
        }
        *cp = '\0';
        PRInt32 ch = nsHTMLEntities::EntityToUnicode(nsSubsumeCStr(cbuf, PR_FALSE));
        if (ch < 0) {
          continue;
        }

        // Remove entity from string and replace it with the integer
        // value.
        aString.Cut(start, i - start);
        aString.Insert(PRUnichar(ch), start);
        i = start + 1;
      }
      else if (e == '{') {
        // Convert a script entity
        // XXX write me!
      }
    }
  }
}

// Temporary factory code to create content objects

static void
GetAttributeValueAt(const nsIParserNode& aNode,
                    PRInt32 aIndex,
                    nsString& aResult,
                    nsIScriptContextOwner* aScriptContextOwner)
{
  // Copy value
  const nsString& value = aNode.GetValueAt(aIndex);
  aResult.Truncate();
  aResult.Append(value);

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

static nsresult
AddAttributes(const nsIParserNode& aNode,
              nsIHTMLContent* aContent,
              nsIScriptContextOwner* aScriptContextOwner,
              PRBool aNotify = PR_FALSE)
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
      GetAttributeValueAt(aNode, i, v, aScriptContextOwner);

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
                  nsIAtom* aAtom,
                  nsIDOMHTMLFormElement* aForm,
                  nsIWebShell* aWebShell,
                  nsIHTMLContent** aResult,
                  const nsString* aContent = nsnull)
{
  nsresult rv = NS_OK;
  switch (aNodeType) {
  default:
    rv = NS_NewHTMLSpanElement(aResult, aAtom);
    break;

  case eHTMLTag_a:
    rv = NS_NewHTMLAnchorElement(aResult, aAtom);
    break;
  case eHTMLTag_applet:
    rv = NS_NewHTMLAppletElement(aResult, aAtom);
    break;
  case eHTMLTag_area:
    rv = NS_NewHTMLAreaElement(aResult, aAtom);
    break;
  case eHTMLTag_base:
    rv = NS_NewHTMLBaseElement(aResult, aAtom);
    break;
  case eHTMLTag_basefont:
    rv = NS_NewHTMLBaseFontElement(aResult, aAtom);
    break;
  case eHTMLTag_blockquote:
    rv = NS_NewHTMLQuoteElement(aResult, aAtom);
    break;
  case eHTMLTag_body:
    rv = NS_NewHTMLBodyElement(aResult, aAtom);
    break;
  case eHTMLTag_br:
    rv = NS_NewHTMLBRElement(aResult, aAtom);
    break;
  case eHTMLTag_button:
    rv = NS_NewHTMLButtonElement(aResult, aAtom);
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_caption:
    rv = NS_NewHTMLTableCaptionElement(aResult, aAtom);
    break;
  case eHTMLTag_col:
    rv = NS_NewHTMLTableColElement(aResult, aAtom);
    break;
  case eHTMLTag_colgroup:
    rv = NS_NewHTMLTableColGroupElement(aResult, aAtom);
    break;
  case eHTMLTag_dir:
    rv = NS_NewHTMLDirectoryElement(aResult, aAtom);
    break;
  case eHTMLTag_div:
  case eHTMLTag_parsererror:
  case eHTMLTag_sourcetext:
    rv = NS_NewHTMLDivElement(aResult, aAtom);
    break;
  case eHTMLTag_dl:
    rv = NS_NewHTMLDListElement(aResult, aAtom);
    break;
  case eHTMLTag_embed:
    rv = NS_NewHTMLEmbedElement(aResult, aAtom);
    break;
  case eHTMLTag_fieldset:
    rv = NS_NewHTMLFieldSetElement(aResult, aAtom);
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_font:
    rv = NS_NewHTMLFontElement(aResult, aAtom);
    break;
  case eHTMLTag_form:
    // the form was already created 
    if (aForm) {
      rv = aForm->QueryInterface(kIHTMLContentIID, (void**)aResult);
    }
    else {
      rv = NS_NewHTMLFormElement(aResult, aAtom);
    }
    break;
  case eHTMLTag_frame:
    rv = NS_NewHTMLFrameElement(aResult, aAtom);
    break;
  case eHTMLTag_frameset:
    rv = NS_NewHTMLFrameSetElement(aResult, aAtom);
    break;
  case eHTMLTag_h1:
  case eHTMLTag_h2:
  case eHTMLTag_h3:
  case eHTMLTag_h4:
  case eHTMLTag_h5:
  case eHTMLTag_h6:
    rv = NS_NewHTMLHeadingElement(aResult, aAtom);
    break;
  case eHTMLTag_head:
    rv = NS_NewHTMLHeadElement(aResult, aAtom);
    break;
  case eHTMLTag_hr:
    rv = NS_NewHTMLHRElement(aResult, aAtom);
    break;
  case eHTMLTag_html:
    rv = NS_NewHTMLHtmlElement(aResult, aAtom);
    break;
  case eHTMLTag_iframe:
    rv = NS_NewHTMLIFrameElement(aResult, aAtom);
    break;
  case eHTMLTag_img:
    rv = NS_NewHTMLImageElement(aResult, aAtom);
    break;
  case eHTMLTag_input:
    rv = NS_NewHTMLInputElement(aResult, aAtom);
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_isindex:
    rv = NS_NewHTMLIsIndexElement(aResult, aAtom);
    break;
  case eHTMLTag_label:
    rv = NS_NewHTMLLabelElement(aResult, aAtom);
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_layer:
    rv = NS_NewHTMLLayerElement(aResult, aAtom);
    break;
  case eHTMLTag_ilayer:
    rv = NS_NewHTMLLayerElement(aResult, aAtom);
    break;
  case eHTMLTag_legend:
    rv = NS_NewHTMLLegendElement(aResult, aAtom);
    break;
  case eHTMLTag_li:
    rv = NS_NewHTMLLIElement(aResult, aAtom);
    break;
  case eHTMLTag_link:
    rv = NS_NewHTMLLinkElement(aResult, aAtom);
    break;
  case eHTMLTag_map:
    rv = NS_NewHTMLMapElement(aResult, aAtom);
    break;
  case eHTMLTag_menu:
    rv = NS_NewHTMLMenuElement(aResult, aAtom);
    break;
  case eHTMLTag_meta:
    rv = NS_NewHTMLMetaElement(aResult, aAtom);
    break;
  case eHTMLTag_object:
    rv = NS_NewHTMLObjectElement(aResult, aAtom);
    break;
  case eHTMLTag_ol:
    rv = NS_NewHTMLOListElement(aResult, aAtom);
    break;
  case eHTMLTag_optgroup:
    rv = NS_NewHTMLOptGroupElement(aResult, aAtom);
    break;
  case eHTMLTag_option:
    rv = NS_NewHTMLOptionElement(aResult, aAtom);
    break;
  case eHTMLTag_p:
    rv = NS_NewHTMLParagraphElement(aResult, aAtom);
    break;
  case eHTMLTag_pre:
    rv = NS_NewHTMLPreElement(aResult, aAtom);
    break;
  case eHTMLTag_param:
    rv = NS_NewHTMLParamElement(aResult, aAtom);
    break;
  case eHTMLTag_q:
    rv = NS_NewHTMLQuoteElement(aResult, aAtom);
    break;
  case eHTMLTag_script:
    rv = NS_NewHTMLScriptElement(aResult, aAtom);
    break;
  case eHTMLTag_select:
    rv = NS_NewHTMLSelectElement(aResult, aAtom);
    SetForm(*aResult, aForm);
    break;
  case eHTMLTag_spacer:
    rv = NS_NewHTMLSpacerElement(aResult, aAtom);
    break;
  case eHTMLTag_style:
    rv = NS_NewHTMLStyleElement(aResult, aAtom);
    break;
  case eHTMLTag_table:
    rv = NS_NewHTMLTableElement(aResult, aAtom);
    break;
  case eHTMLTag_tbody:
  case eHTMLTag_thead:
  case eHTMLTag_tfoot:
    rv = NS_NewHTMLTableSectionElement(aResult, aAtom);
    break;
  case eHTMLTag_td:
  case eHTMLTag_th:
    rv = NS_NewHTMLTableCellElement(aResult, aAtom);
    break;
  case eHTMLTag_textarea:
    rv = NS_NewHTMLTextAreaElement(aResult, aAtom);
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
    rv = NS_NewHTMLTitleElement(aResult, aAtom);
    break;
  case eHTMLTag_tr:
    rv = NS_NewHTMLTableRowElement(aResult, aAtom);
    break;
  case eHTMLTag_ul:
    rv = NS_NewHTMLUListElement(aResult, aAtom);
    break;
  case eHTMLTag_wbr:
    rv = NS_NewHTMLWBRElement(aResult, aAtom);
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
static nsresult
CreateContentObject(const nsIParserNode& aNode,
                    nsHTMLTag aNodeType,
                    nsIDOMHTMLFormElement* aForm,
                    nsIWebShell* aWebShell,
                    nsIHTMLContent** aResult)
{
  // Find/create atom for the tag name
  nsAutoString tmp;
  if (eHTMLTag_userdefined == aNodeType) {
    tmp.Append(aNode.GetText());
    tmp.ToLowerCase();
  }
  else {
    tmp.Append(nsHTMLTags::GetStringValue(aNodeType));
  }
  nsIAtom* atom = NS_NewAtom(tmp);
  if (nsnull == atom) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make the content object
  // XXX why is textarea not a container?
  nsAutoString content;
  if (eHTMLTag_textarea == aNodeType) {
    content = aNode.GetSkippedContent();
  }
  nsresult rv = MakeContentObject(aNodeType, atom, aForm, aWebShell,
                                  aResult, &content);

  NS_RELEASE(atom);

  return rv;
}

nsresult
NS_CreateHTMLElement(nsIHTMLContent** aResult, const nsString& aTag)
{
  // Find tag in tag table
  nsHTMLTag id = nsHTMLTags::LookupTag(aTag);
  if (eHTMLTag_userdefined == id) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Create atom for tag and then create content object
  const nsCString& tag = nsHTMLTags::GetStringValue(id);
  nsIAtom* atom = NS_NewAtom((const char*)tag);
  nsresult rv = MakeContentObject(id, atom, nsnull, nsnull, aResult);
  NS_RELEASE(atom);

  return rv;
}

//----------------------------------------------------------------------


static NS_DEFINE_IID(kIHTMLElementFactoryIID, NS_IHTML_ELEMENT_FACTORY_IID);

class nsHTMLElementFactory : public nsIHTMLElementFactory {
public:
  nsHTMLElementFactory();
  virtual ~nsHTMLElementFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstanceByTag(const nsString& aTag,
                                 nsIHTMLContent** aResult);
};

nsresult
NS_NewHTMLElementFactory(nsIHTMLElementFactory** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null OUT ptr");
  if (!aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsHTMLElementFactory* it = new nsHTMLElementFactory();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLElementFactoryIID,
                            (void**)aInstancePtrResult);
}

nsHTMLElementFactory::nsHTMLElementFactory()
{
  NS_INIT_REFCNT();
}

nsHTMLElementFactory::~nsHTMLElementFactory()
{
}

NS_IMPL_ISUPPORTS(nsHTMLElementFactory, kIHTMLElementFactoryIID);

NS_IMETHODIMP
nsHTMLElementFactory::CreateInstanceByTag(const nsString& aTag,
                                          nsIHTMLContent** aResult)
{
  nsresult rv;
  rv = NS_CreateHTMLElement(aResult, aTag);
  return rv;
}

//----------------------------------------------------------------------

SinkContext::SinkContext(HTMLContentSink* aSink)
{
  mSink = aSink;
  mPreAppend = PR_FALSE;
  mStack = nsnull;
  mStackSize = 0;
  mStackPos = 0;
  mText = nsnull;
  mTextLength = 0;
  mTextSize = 0;
}

SinkContext::~SinkContext()
{
  if (nsnull != mStack) {
    for (PRInt32 i = 0; i < mStackPos; i++) {
      NS_RELEASE(mStack[i].mContent);
    }
    delete [] mStack;
  }
  if (nsnull != mText) {
    delete [] mText;
  }
}

nsresult
SinkContext::Begin(nsHTMLTag aNodeType, nsIHTMLContent* aRoot)
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
SinkContext::MaybeMarkSinkDirty()
{
  if (!mSink->mDirty &&
      (2 == mStackPos) &&
      (mSink->mBody == mStack[1].mContent)) {
    // We just finished adding something to the body
    mSink->mDirty = PR_TRUE;
  }
}

void 
SinkContext::MaybeMarkSinkClean()
{
  // XXX For now just clear the dirty bit. In the future, we might
  // selectively clear it based on the current context.
  // Note that it will be marked dirty again when we're in a 
  // safe state.
  mSink->mDirty = PR_FALSE;
}

nsresult
SinkContext::OpenContainer(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::OpenContainer", aNode);

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
  rv = CreateContentObject(aNode, nodeType,
                           mSink->mCurrentForm,
                           mSink->mFrameset ? mSink->mWebShell : nsnull,
                           &content);
  if (NS_OK != rv) {
    return rv;
  }
  mStack[mStackPos].mType = nodeType;
  mStack[mStackPos].mContent = content;
  mStack[mStackPos].mFlags = 0;
  content->SetDocument(mSink->mDocument, PR_FALSE);

  nsIScriptContextOwner* sco = mSink->mDocument->GetScriptContextOwner();
  rv = AddAttributes(aNode, content, sco);
  NS_IF_RELEASE(sco);

  if (mPreAppend) {
    NS_ASSERTION(mStackPos > 0, "container w/o parent");
    nsIHTMLContent* parent = mStack[mStackPos-1].mContent;
    parent->AppendChildTo(content, PR_FALSE);
    mStack[mStackPos].mFlags |= APPENDED;
  }
  mStackPos++;
  if (NS_OK != rv) {
    return rv;
  }

  // Special handling for certain tags
  switch (nodeType) {
  case eHTMLTag_a:
    mSink->ProcessATag(aNode, content);
    break;
  case eHTMLTag_table:
    mSink->mInMonolithicContainer++;
  case eHTMLTag_layer:
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
  default:
    break;
  }

  return NS_OK;
}

nsresult
SinkContext::CloseContainer(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::CloseContainer", aNode);

  --mStackPos;
  nsHTMLTag nodeType = mStack[mStackPos].mType;
  nsIHTMLContent* content = mStack[mStackPos].mContent;
  content->Compact();

  // Add container to its parent if we haven't already done it
  if (0 == (mStack[mStackPos].mFlags & APPENDED)) {
    NS_ASSERTION(mStackPos > 0, "container w/o parent");
    nsIHTMLContent* parent = mStack[mStackPos-1].mContent;
    result = parent->AppendChildTo(content, PR_FALSE);
  }
  NS_IF_RELEASE(content);

  // Special handling for certain tags
  switch (nodeType) {
  case eHTMLTag_table:
    mSink->mInMonolithicContainer--;
    break;
  case eHTMLTag_form:
    {
      nsHTMLTag parserNodeType = nsHTMLTag(aNode.GetNodeType());
      
      // If there's a FORM on the stack, but this close tag doesn't
      // close the form, then close out the form *and* close out the
      // next container up. This is since the parser doesn't do fix up
      // of forms. When the end FORM tag comes through, we'll ignore
      // it.
      if (parserNodeType != nodeType) {
        result = CloseContainer(aNode);
      }
    }
  default:
    break;
  }

  // Mark sink dirty if it can safely reflow something
  MaybeMarkSinkDirty();

#ifdef DEBUG
  if (mPreAppend && mSink->mDirty &&
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
      child->SetDocument(aDocument, PR_TRUE);
      NS_RELEASE(child);
    }
  }
}

nsresult
SinkContext::DemoteContainer(const nsIParserNode& aNode, 
                             nsIHTMLContent*& aParent)
{
  nsresult result = NS_OK;
  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());

  aParent = nsnull;
  // Search for the nearest container on the stack of the
  // specified type
  PRInt32 stackPos = mStackPos-1;
  while ((stackPos > 0) && (nodeType != mStack[stackPos].mType)) {
    stackPos--;
  }
  
  // If we find such a container
  if (stackPos > 0) {
    nsIHTMLContent* container = mStack[stackPos].mContent;
    
    // See if it has a parent on the stack. It should for all the
    // cases for which this is called, but put in a check anyway
    if (stackPos > 1) {
      nsIHTMLContent* parent = mStack[stackPos-1].mContent;
      
      // If the demoted container hasn't yet been appended to its
      // parent, do so now.
      if (0 == (mStack[stackPos].mFlags & APPENDED)) {
        result = parent->AppendChildTo(container, PR_FALSE);
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
           result = container->RemoveChildAt(0, PR_FALSE);
           if (NS_SUCCEEDED(result)) {
             result = parent->AppendChildTo(child, PR_FALSE);
             SetDocumentInChildrenOf(child, mSink->mDocument);
           }
           NS_RELEASE(child);
         }
       }
       
       // Remove the current element from the context stack,
       // since it's been demoted.
       while (stackPos < mStackPos-1) {
         mStack[stackPos].mType = mStack[stackPos+1].mType;
         mStack[stackPos].mContent = mStack[stackPos+1].mContent;
         mStack[stackPos].mFlags = mStack[stackPos+1].mFlags;
         stackPos++;
       }
       mStackPos--;
     }
     aParent = parent;
     NS_ADDREF(parent);
    }
    NS_RELEASE(container);
  }
  
  return result;
}

nsresult
SinkContext::AddLeaf(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "SinkContext::AddLeaf", aNode);

  nsresult rv = NS_OK;

  switch (aNode.GetTokenType()) {
  case eToken_start:
    {
      FlushText();

      // Create new leaf content object
      nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
      nsIHTMLContent* content;
      rv = CreateContentObject(aNode, nodeType,
                               mSink->mCurrentForm, mSink->mWebShell,
                               &content);
      if (NS_OK != rv) {
        return rv;
      }

      // Set the content's document
      content->SetDocument(mSink->mDocument, PR_FALSE);

      nsIScriptContextOwner* sco = mSink->mDocument->GetScriptContextOwner();
      rv = AddAttributes(aNode, content, sco);
      NS_IF_RELEASE(sco);
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
          tmp = "\n";
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
  nsIHTMLContent* parent = mStack[mStackPos-1].mContent;
  parent->AppendChildTo(aContent, PR_FALSE);

  // Mark sink dirty if it can safely reflow something
  MaybeMarkSinkDirty();

#ifdef DEBUG
  if (mPreAppend && mSink->mDirty &&
      SINK_LOG_TEST(gSinkLogModuleInfo,SINK_ALWAYS_REFLOW)) {
    mSink->ForceReflow();
  }
#endif

  return NS_OK;
}

nsresult
SinkContext::AddComment(const nsIParserNode& aNode)
{
  nsIContent *comment;
  nsIDOMComment *domComment;
  nsresult result = NS_OK;

  FlushText();
  
  result = NS_NewCommentNode(&comment);
  if (NS_OK == result) {
    result = comment->QueryInterface(kIDOMCommentIID, 
                                     (void **)&domComment);
    if (NS_OK == result) {
      domComment->AppendData(aNode.GetText());
      NS_RELEASE(domComment);
      
      comment->SetDocument(mSink->mDocument, PR_FALSE);
      
      nsIHTMLContent* parent;
      if ((nsnull == mSink->mBody) && (nsnull != mSink->mHead)) {
        parent = mSink->mHead;
      }
      else {
        parent = mStack[mStackPos - 1].mContent;
      }
      parent->AppendChildTo(comment, PR_FALSE);
      
      // Mark sink dirty if it can safely reflow something
      MaybeMarkSinkDirty();

#ifdef DEBUG
      if (mPreAppend && mSink->mDirty &&
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
  NS_ASSERTION(mStackPos == 1, "insufficient close container calls");

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
 */
nsresult
SinkContext::FlushTags()
{
  FlushText();
  // Prevent reflows if we're in a situation where we have
  // incomplete content.
  MaybeMarkSinkClean();

  PRInt32 stackPos = mStackPos-1;
  while ((stackPos > 0) && (0 == (mStack[stackPos].mFlags & APPENDED))) {
    nsIHTMLContent* content = mStack[stackPos].mContent;
    nsIHTMLContent* parent = mStack[stackPos-1].mContent;
    
    parent->AppendChildTo(content, PR_FALSE);
    mStack[stackPos].mFlags |= APPENDED;
    stackPos--;
  }

  return NS_OK;
}

/**
 * Flush any buffered text out by creating a text content object and
 * adding it to the content.
 */
nsresult
SinkContext::FlushText(PRBool* aDidFlush)
{
  nsresult rv = NS_OK;
  PRBool didFlush = PR_FALSE;
  if (0 != mTextLength) {
    nsIContent* content;
    rv = NS_NewTextNode(&content);
    if (NS_OK == rv) {
      // Set the content's document
      content->SetDocument(mSink->mDocument, PR_FALSE);

      // Set the text in the text node
      nsITextContent* text = nsnull;
      content->QueryInterface(kITextContentIID, (void**) &text);
      text->SetText(mText, mTextLength, PR_FALSE);
      NS_RELEASE(text);

      // Add text to its parent
      NS_ASSERTION(mStackPos > 0, "leaf w/o container");
      nsIHTMLContent* parent = mStack[mStackPos - 1].mContent;
      parent->AppendChildTo(content, PR_FALSE);
      NS_RELEASE(content);

      // Mark sink dirty if it can safely reflow something
      MaybeMarkSinkDirty();
    }
    mTextLength = 0;
    didFlush = PR_TRUE;
  }
  if (nsnull != aDidFlush) {
    *aDidFlush = didFlush;
  }

#ifdef DEBUG
  if (mPreAppend && mSink->mDirty && didFlush &&
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

HTMLContentSink::HTMLContentSink()
{
#ifdef NS_DEBUG
  if (nsnull == gSinkLogModuleInfo) {
    gSinkLogModuleInfo = PR_NewLogModule("htmlcontentsink");
  }
#endif
  mNotAtRef        = PR_TRUE;
  mParser          = nsnull;
  mDocumentBaseURL = nsnull;
  mBody            = nsnull;
  mFrameset        = nsnull;
  mStyleSheetCount = 0;
  mCSSLoader       = nsnull;
}

HTMLContentSink::~HTMLContentSink()
{
  NS_IF_RELEASE(mHead);
  NS_IF_RELEASE(mBody);
  NS_IF_RELEASE(mFrameset);
  NS_IF_RELEASE(mRoot);

  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mHTMLDocument);
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mDocumentBaseURL);
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mParser);
  NS_IF_RELEASE(mCSSLoader);

  NS_IF_RELEASE(mCurrentForm);
  NS_IF_RELEASE(mCurrentMap);
  NS_IF_RELEASE(mCurrentDOMMap);
  NS_IF_RELEASE(mRefContent);

  for (PRInt32 i = 0; i < mNumContexts; i++) {
    SinkContext* sc = mContexts[i];
    sc->End();
    delete sc;
    if (sc == mCurrentContext) {
      mCurrentContext = nsnull;
    }
  }
  if (nsnull != mContexts) {
    delete [] mContexts;
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

NS_IMPL_ISUPPORTS(HTMLContentSink, kIHTMLContentSinkIID)

nsresult
HTMLContentSink::Init(nsIDocument* aDoc,
                      nsIURI* aURL,
                      nsIWebShell* aContainer)
{
  NS_PRECONDITION(nsnull != aDoc, "null ptr");
  NS_PRECONDITION(nsnull != aURL, "null ptr");
  NS_PRECONDITION(nsnull != aContainer, "null ptr");
  if ((nsnull == aDoc) || (nsnull == aURL) || (nsnull == aContainer)) {
    return NS_ERROR_NULL_POINTER;
  }

  mDocument = aDoc;
  NS_ADDREF(aDoc);
  aDoc->QueryInterface(kIHTMLDocumentIID, (void**)&mHTMLDocument);
  mDocumentURL = aURL;
  NS_ADDREF(aURL);
  mDocumentBaseURL = aURL;
  NS_ADDREF(aURL);
  mWebShell = aContainer;
  NS_ADDREF(aContainer);

  nsIHTMLContentContainer* htmlContainer = nsnull;
  if (NS_SUCCEEDED(aDoc->QueryInterface(kIHTMLContentContainerIID, (void**)&htmlContainer))) {
    htmlContainer->GetCSSLoader(mCSSLoader);
    NS_RELEASE(htmlContainer);
  }

  // XXX this presumes HTTP header info is alread set in document
  // XXX if it isn't we need to set it here...
  mDocument->GetHeaderData(nsHTMLAtoms::headerDefaultStyle, mPreferredStyle);

  // Make root part
  nsresult rv = NS_NewHTMLHtmlElement(&mRoot, nsHTMLAtoms::html);
  if (NS_OK != rv) {
    return rv;
  }
  mRoot->SetDocument(mDocument, PR_FALSE);
  mDocument->SetRootContent(mRoot);

  // Make head part
  nsIAtom* atom = NS_NewAtom("head");
  if (nsnull == atom) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = NS_NewHTMLHeadElement(&mHead, atom);
  NS_RELEASE(atom);
  if (NS_OK != rv) {
    return rv;
  }
  mRoot->AppendChildTo(mHead, PR_FALSE);

  mCurrentContext = new SinkContext(this);
  mCurrentContext->Begin(eHTMLTag_html, mRoot);
  mContextStack.AppendElement(mCurrentContext);

#ifdef NECKO
  char* spec;
#else
  const char* spec;
#endif
  (void)aURL->GetSpec(&spec);
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::Init: this=%p url='%s'",
              this, spec));
#ifdef NECKO
  nsCRT::free(spec);
#endif
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
  if (nsnull == mTitle) {
    mHTMLDocument->SetTitle("");
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
    NotifyBody();
  }
  ScrollToRef();

  mDocument->EndLoad();
  // Drop our reference to the parser to get rid of a circular
  // reference.
  NS_IF_RELEASE(mParser);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillInterrupt()
{
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::WillInterrupt: this=%p", this));
  if (mDirty) {
    if (nsnull != mBody) {
      SINK_TRACE(SINK_TRACE_REFLOW,
                 ("HTMLContentSink::WillInterrupt: reflow content"));
      NotifyBody();
    }
    mDirty = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::WillResume()
{
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::WillResume: this=%p", this));
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
  NS_PRECONDITION(aPosition > -1, "out of bounds");

  // Create new context
  SinkContext* sc = new SinkContext(this);
  if (nsnull == sc) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
   
  NS_ASSERTION(mCurrentContext != nsnull," Non-existing context");
  
  nsHTMLTag nodeType      = mCurrentContext->mStack[aPosition].mType;
  nsIHTMLContent* content = mCurrentContext->mStack[aPosition].mContent;
  sc->Begin(nodeType,content);
  NS_ADDREF(sc->mSink);

  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = sc;
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::EndContext(PRInt32 aPosition)
{
  NS_PRECONDITION(mCurrentContext != nsnull && aPosition > -1, "non-existing context");

  PRInt32 n = mContextStack.Count() - 1;
  SinkContext* sc = (SinkContext*) mContextStack.ElementAt(n);

  NS_ASSERTION(sc->mStack[aPosition].mType == mCurrentContext->mStack[0].mType,"ending a wrong context");
  
  mCurrentContext->FlushText();
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

  return NS_OK;
}


NS_IMETHODIMP
HTMLContentSink::SetTitle(const nsString& aValue)
{
  NS_ASSERTION(mCurrentContext == mHeadContext, "SetTitle not in head");

  if (nsnull == mTitle) {
    mTitle = new nsString(aValue);
  }
  else {
    // If the title was already set then don't try to overwrite it
    // when a new title is encountered - For backwards compatiblity
    //*mTitle = aValue;
    return NS_OK;
  }
  ReduceEntities(*mTitle);
  mTitle->CompressWhitespace(PR_TRUE, PR_TRUE);
  mHTMLDocument->SetTitle(*mTitle);

  nsIAtom* atom = NS_NewAtom("title");
  nsIHTMLContent* it = nsnull;
  nsresult rv = NS_NewHTMLTitleElement(&it, atom);
  if (NS_OK == rv) {
    nsIContent* text;
    rv = NS_NewTextNode(&text);
    if (NS_OK == rv) {
      nsIDOMText* tc;
      rv = text->QueryInterface(kIDOMTextIID, (void**)&tc);
      if (NS_OK == rv) {
        tc->SetData(aValue);
        NS_RELEASE(tc);
      }
      it->AppendChildTo(text, PR_FALSE);
      text->SetDocument(mDocument, PR_FALSE);
      NS_RELEASE(text);
    }
    mHead->AppendChildTo(it, PR_FALSE);
    NS_RELEASE(it);
  }
  NS_RELEASE(atom);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenHTML(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenHTML", aNode);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseHTML(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseHTML", aNode);
  if (nsnull != mHeadContext) {
    mHeadContext->End();
    delete mHeadContext;
    mHeadContext = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenHead(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenHead", aNode);
  nsresult rv = NS_OK;
  if (nsnull == mHeadContext) {
    mHeadContext = new SinkContext(this);
    if (nsnull == mHeadContext) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mHeadContext->SetPreAppend(PR_TRUE);
    rv = mHeadContext->Begin(eHTMLTag_head, mHead);
    if (NS_OK != rv) {
      return rv;
    }
  }
  mContextStack.AppendElement(mCurrentContext);
  mCurrentContext = mHeadContext;

  if (nsnull != mHead) {
    nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
    rv = AddAttributes(aNode, mHead, sco);
    NS_IF_RELEASE(sco);
  }

  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseHead(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseHead", aNode);
  PRInt32 n = mContextStack.Count() - 1;
  mCurrentContext = (SinkContext*) mContextStack.ElementAt(n);
  mContextStack.RemoveElementAt(n);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenBody(const nsIParserNode& aNode)
{
  //NS_PRECONDITION(nsnull == mBody, "parser called OpenBody twice");

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenBody", aNode);
  // Add attributes, if any, to the current BODY node
  if(mBody != nsnull){
    nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
    AddAttributes(aNode,mBody,sco,PR_TRUE);
    NS_IF_RELEASE(sco);      
    return NS_OK;
  }

  // Open body. Note that we pre-append the body to the root so that
  // incremental reflow during document loading will work properly.
  mCurrentContext->SetPreAppend(PR_TRUE);
   nsresult rv = mCurrentContext->OpenContainer(aNode);
  mCurrentContext->SetPreAppend(PR_FALSE);
  if (NS_OK != rv) {
    return rv;
  }
  mBody = mCurrentContext->mStack[mCurrentContext->mStackPos - 1].mContent;
  mBodyChildCount = 0;
  NS_ADDREF(mBody);

  StartLayout();
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseBody(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseBody", aNode);

  PRBool didFlush;
  nsresult rv = mCurrentContext->FlushText(&didFlush);
  if (NS_OK != rv) {
    return rv;
  }
  mCurrentContext->CloseContainer(aNode);

  if (didFlush) {
    // Trigger a reflow for the flushed text
    NotifyBody();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenForm(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;
  nsIHTMLContent* content = nsnull;
  
  mCurrentContext->FlushText();
  
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenForm", aNode);
  
  // Close out previous form if it's there. If there is one
  // around, it's probably because the last one wasn't well-formed.
  NS_IF_RELEASE(mCurrentForm);
  
  // Check if the parent is a table, tbody, thead, tfoot, tr, col or
  // colgroup. If so, this is just going to be a leaf element.
  // If so, we fix up by making the form leaf content.
  if (mCurrentContext->IsCurrentContainer(eHTMLTag_table) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tbody) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_thead) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tfoot) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_tr) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_col) ||
      mCurrentContext->IsCurrentContainer(eHTMLTag_colgroup)) {
    nsAutoString tmp("form");
    nsIAtom* atom = NS_NewAtom(tmp);
    result = NS_NewHTMLFormElement(&content, atom);
    if (NS_SUCCEEDED(result) && content) {
      content->QueryInterface(kIDOMHTMLFormElementIID, (void**)&mCurrentForm);
      NS_RELEASE(content);
    }
    NS_RELEASE(atom);
    
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
  
  return result;
}

// XXX MAYBE add code to place close form tag into the content model
// for navigator layout compatability.
NS_IMETHODIMP
HTMLContentSink::CloseForm(const nsIParserNode& aNode)
{
  nsresult result = NS_OK;

  mCurrentContext->FlushText();
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseForm", aNode);

  if (nsnull != mCurrentForm) {
    // Check if this is a well-formed form
    if (mCurrentContext->IsCurrentContainer(eHTMLTag_form)) {
      result = mCurrentContext->CloseContainer(aNode);
    }
    else if (mCurrentContext->IsAncestorContainer(eHTMLTag_form)) {
      nsIHTMLContent* parent;
      result = mCurrentContext->DemoteContainer(aNode, parent);
      if (NS_SUCCEEDED(result)) {
        if (parent == mBody) {
          NotifyBody();
        }
        NS_IF_RELEASE(parent);
      }
    }
    NS_RELEASE(mCurrentForm);
  }

  return result;
}

NS_IMETHODIMP
HTMLContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenFrameset", aNode);

  nsresult rv = mCurrentContext->OpenContainer(aNode);
  if ((NS_OK == rv) && (nsnull == mFrameset)) {
    mFrameset = mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
    NS_ADDREF(mFrameset);
  }
  mInMonolithicContainer++;
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseFrameset(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseFrameset", aNode);

  SinkContext* sc = mCurrentContext;
  nsIHTMLContent* fs = sc->mStack[sc->mStackPos-1].mContent;
  PRBool done = fs == mFrameset;
  nsresult rv = sc->CloseContainer(aNode);
  if (done) {
    StartLayout();
  }
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::OpenMap(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenMap", aNode);
  // We used to treat MAP elements specially (i.e. they were
  // only parent elements for AREAs), but we don't anymore.
  // HTML 4.0 says that MAP elements can have block content
  // as children.
  return mCurrentContext->OpenContainer(aNode);
}

NS_IMETHODIMP
HTMLContentSink::CloseMap(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseMap", aNode);
  NS_IF_RELEASE(mCurrentMap);
  NS_IF_RELEASE(mCurrentDOMMap);

  return mCurrentContext->CloseContainer(aNode);
}

NS_IMETHODIMP
HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  // XXX work around parser bug
  if (eHTMLTag_frameset == aNode.GetNodeType()) {
    return OpenFrameset(aNode);
  }
  return mCurrentContext->OpenContainer(aNode);
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  // XXX work around parser bug
  if (eHTMLTag_frameset == aNode.GetNodeType()) {
    return CloseFrameset(aNode);
  }
  return mCurrentContext->CloseContainer(aNode);
}

NS_IMETHODIMP
HTMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  nsresult rv;

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());
  switch (nodeType) {
  case eHTMLTag_area:
    rv = ProcessAREATag(aNode);
    break;

  case eHTMLTag_base:
    mCurrentContext->FlushText();
    rv = ProcessBASETag(aNode);
    break;

  case eHTMLTag_link:
    mCurrentContext->FlushText();
    rv = ProcessLINKTag(aNode);
    break;

  case eHTMLTag_meta:
    mCurrentContext->FlushText();
    rv = ProcessMETATag(aNode);
    break;

  case eHTMLTag_style:
    mCurrentContext->FlushText();
    rv = ProcessSTYLETag(aNode);
    break;

  case eHTMLTag_script:
    mCurrentContext->FlushText();
    rv = ProcessSCRIPTTag(aNode);
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
nsresult HTMLContentSink::AddComment(const nsIParserNode& aNode) {
  return mCurrentContext->AddComment(aNode);
}

/**
 * This gets called by the parsing system when we find a PI
 * @update	gess11/9/98
 * @param   aNode contains a comment token
 * @return  error code
 */
nsresult HTMLContentSink::AddProcessingInstruction(const nsIParserNode& aNode) {
  nsresult result= NS_OK;
  return result;
}

/**
 *  This gets called by the parser when it encounters
 *  a DOCTYPE declaration in the HTML document.
 */

NS_IMETHODIMP
HTMLContentSink::AddDocTypeDecl(const nsIParserNode& aNode, PRInt32 aMode)
{
  return NS_OK;
}


void
HTMLContentSink::StartLayout()
{
  if (mLayoutStarted) {
    return;
  }
  mLayoutStarted = PR_TRUE;

  // If it's a frameset document then disable scrolling. If it is not a <frame> 
  // document, then let the style dictate. We need to do this before the initial reflow...
  if (mWebShell) {
    if (mFrameset) {
      mWebShell->SetScrolling(NS_STYLE_OVERFLOW_HIDDEN, PR_FALSE);
    } 
    else if (mBody) {
      PRBool isFrameDoc = PR_FALSE;
      mWebShell->GetIsFrame(isFrameDoc);
      // a <frame> webshell will have its scrolling set by the parent nsFramesetFrame. 
      // a <body> webshell is reset here just for safety.
      if (!isFrameDoc) {
        mWebShell->SetScrolling(-1, PR_FALSE);
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
#ifdef NECKO
  char* ref;
  nsIURL* url;
  nsresult rv = mDocumentURL->QueryInterface(nsIURL::GetIID(), (void**)&url);
  if (NS_SUCCEEDED(rv)) {
    rv = url->GetRef(&ref);
    NS_RELEASE(url);
  }
#else
  const char* ref;
  nsresult rv = mDocumentURL->GetRef(&ref);
#endif
  if (rv == NS_OK) {
    mRef = new nsString(ref);
#ifdef NECKO
    nsCRT::free(ref);
#endif
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
              frame->GetOffsetFromView(offset, &view);
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
    area->SetDocument(mDocument, PR_FALSE);
    nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
    rv = AddAttributes(aNode, area, sco);
    NS_IF_RELEASE(sco);
    if (NS_FAILED(rv)) {
      NS_RELEASE(area);
      return rv;
    }

    // Add AREA object to the current map
    mCurrentMap->AppendChildTo(area, PR_FALSE);
    NS_RELEASE(area);
  }
  return NS_OK;
}

void 
HTMLContentSink::ProcessBaseHref(const nsString& aBaseHref)
{
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
    nsIContentViewer* contentViewer = nsnull;
    nsresult rv = mWebShell->GetContentViewer(&contentViewer);
    if (NS_SUCCEEDED(rv) && (contentViewer != nsnull)) {
      PRBool enabled;
      contentViewer->GetEnableRendering(&enabled);
      if (enabled) {
        vm->EnableRefresh();
      }
      NS_RELEASE(contentViewer); 
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
  nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();

  // Create content object
  nsAutoString tag("BASE");
  nsIHTMLContent* element = nsnull;
  result = NS_CreateHTMLElement(&element, tag);
  if (NS_SUCCEEDED(result)) {
    // Add in the attributes and add the style content object to the
    // head container.
    element->SetDocument(mDocument, PR_FALSE);
    result = AddAttributes(aNode, element, sco);
    if (NS_SUCCEEDED(result)) {
      mHead->AppendChildTo(element, PR_FALSE);

      nsAutoString value;
      if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::href, value)) {
        ProcessBaseHref(value);
      }
      if (NS_CONTENT_ATTR_HAS_VALUE == element->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::target, value)) {
        ProcessBaseTarget(value);
      }
    }
  }
  
  NS_IF_RELEASE(sco);
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
    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
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
          nsAutoString  attr = start;
          attr.StripWhitespace();

          PRUnichar* value = ++equals;
          while (nsString::IsSpace(*value)) {
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
    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }

    end = start;

    while ((kNullCh != *end) && (! nsString::IsSpace(*end))) { // look for space
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
  }
  else {
    aType = aValue;
  }
}

nsresult
HTMLContentSink::ProcessStyleLink(nsIHTMLContent* aElement,
                                  const nsString& aHref, const nsString& aRel,
                                  const nsString& aTitle, const nsString& aType,
                                  const nsString& aMedia)
{
  nsresult result = NS_OK;

  nsStringArray linkTypes;
  ParseLinkTypes(aRel, linkTypes);

  if (-1 != linkTypes.IndexOf("stylesheet")) {  // is it a stylesheet link?

    if (-1 != linkTypes.IndexOf("alternate")) { // if alternate, does it have title?
      if (0 == aTitle.Length()) { // alternates must have title
        return NS_OK; //return without error, for now
      }
    }

    nsAutoString  mimeType;
    nsAutoString  params;
    SplitMimeType(aType, mimeType, params);

    if ((0 == mimeType.Length()) || mimeType.EqualsIgnoreCase("text/css")) {
      nsIURI* url = nsnull;
#ifndef NECKO
      nsILoadGroup* LoadGroup = nsnull;
      mDocumentBaseURL->GetLoadGroup(&LoadGroup);
      if (LoadGroup) {
        result = LoadGroup->CreateURL(&url, mDocumentBaseURL, aHref, nsnull);
        NS_RELEASE(LoadGroup);
      }
      else
#endif
      {
#ifndef NECKO
        result = NS_NewURL(&url, aHref, mDocumentBaseURL);
#else
        result = NS_NewURI(&url, aHref, mDocumentBaseURL);
#endif // NECKO
      }
      if (NS_OK != result) {
        return NS_OK; // The URL is bad, move along, don't propogate the error (for now)
      }

      if (-1 == linkTypes.IndexOf("alternate")) {
        if (0 < aTitle.Length()) {  // possibly preferred sheet
          if (0 == mPreferredStyle.Length()) {
            mPreferredStyle = aTitle;
            mCSSLoader->SetPreferredSheet(aTitle);
            mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, aTitle);
          }
        }
      }

      PRBool blockParser = PR_FALSE;
      if (-1 != linkTypes.IndexOf("important")) {
        blockParser = PR_TRUE;
      }

      PRBool doneLoading;
      result = mCSSLoader->LoadStyleLink(aElement, url, aTitle, aMedia, kNameSpaceID_HTML,
                                         mStyleSheetCount++, 
                                         ((blockParser) ? mParser : nsnull),
                                         doneLoading);
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
  PRInt32   i;
  PRInt32   count = aNode.GetAttributeCount();

  nsAutoString href;
  nsAutoString rel; 
  nsAutoString title; 
  nsAutoString type; 
  nsAutoString media; 

  nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
  for (i = 0; i < count; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("href")) {
      GetAttributeValueAt(aNode, i, href, sco);
      href.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("rel")) {
      GetAttributeValueAt(aNode, i, rel, sco);
      rel.CompressWhitespace();
    }
    else if (key.EqualsIgnoreCase("title")) {
      GetAttributeValueAt(aNode, i, title, sco);
      title.CompressWhitespace();
    }
    else if (key.EqualsIgnoreCase("type")) {
      GetAttributeValueAt(aNode, i, type, sco);
      type.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("media")) {
      GetAttributeValueAt(aNode, i, media, sco);
      media.ToLowerCase(); // HTML4.0 spec is inconsistent, make it case INSENSITIVE
    }
  }

  // Create content object
  nsAutoString tag("LINK");
  nsIHTMLContent* element = nsnull;
  result = NS_CreateHTMLElement(&element, tag);
  if (NS_SUCCEEDED(result)) {
    // Add in the attributes and add the style content object to the
    // head container.
    element->SetDocument(mDocument, PR_FALSE);
    result = AddAttributes(aNode, element, sco);
    if (NS_FAILED(result)) {
      NS_RELEASE(element);
      return result;
    }
    mHead->AppendChildTo(element, PR_FALSE);
  }
  else {
    NS_IF_RELEASE(sco);
    return result;
  }
  NS_IF_RELEASE(sco);

  result = ProcessStyleLink(element, href, rel, title, type, media);

  NS_RELEASE(element);
  return result;
}

nsresult
HTMLContentSink::ProcessMAPTag(const nsIParserNode& aNode,
                               nsIHTMLContent* aContent)
{
  nsresult rv;

  NS_IF_RELEASE(mCurrentMap);
  NS_IF_RELEASE(mCurrentDOMMap);

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

  // Don't need to add the map to the document here anymore.
  // The map adds itself
  mCurrentMap = aContent;  
  NS_ADDREF(aContent);
  mCurrentDOMMap = domMap;  // holds a reference

  return rv;
}

nsresult
HTMLContentSink::ProcessMETATag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;

  if (nsnull != mHead) {
    // Create content object
    nsAutoString tmp("meta");
    nsIAtom* atom = NS_NewAtom(tmp);
    if (nsnull == atom) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    nsIHTMLContent* it;
    rv = NS_NewHTMLMetaElement(&it, atom);
    NS_RELEASE(atom);
    if (NS_OK == rv) {
      // Add in the attributes and add the meta content object to the
      // head container.
      nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
      it->SetDocument(mDocument, PR_FALSE);
      rv = AddAttributes(aNode, it, sco);
      NS_IF_RELEASE(sco);
      if (NS_OK != rv) {
        NS_RELEASE(it);
        return rv;
      }
      mHead->AppendChildTo(it, PR_FALSE);

#ifndef NECKO
      // If we are processing an HTTP url, handle meta http-equiv cases
      nsIHttpURL* httpUrl = nsnull;
      rv = mDocumentURL->QueryInterface(kIHTTPURLIID, (void **)&httpUrl);
#endif

      // set any HTTP-EQUIV data into document's header data as well as url
      nsAutoString header;
      it->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::httpEquiv, header);
      if (header.Length() > 0) {
        nsAutoString result;
        it->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::content, result);
        if (result.Length() > 0) {
#ifdef NECKO
          // XXX necko isn't going to process headers coming in from the parser
          //NS_WARNING("need to fix how necko adds mime headers (in HTMLContentSink::ProcessMETATag)");
          
          // see if we have a refresh "header".
          if (!header.Compare("refresh", PR_TRUE)) {
              // Refresh haders are parsed with the following format in mind
              // <META HTTP-EQUIV=REFRESH CONTENT="5; URL=http://uri">
              // By the time we are here, the following is true:
              // header = "REFRESH"
              // result = "5; URL=http://uri" // note the URL attribute is optional, if it
              //   is absent, the currently loaded url is used.
              const PRUnichar *uriCStr = nsnull;
              nsAutoString uriAttribStr;

              // first get our baseURI
              rv = mWebShell->GetURL(&uriCStr);
              if (NS_FAILED(rv)) return rv;

              nsIURI *baseURI = nsnull;
              rv = NS_NewURI(&baseURI, uriCStr, nsnull);
              if (NS_FAILED(rv)) return rv;

              // next get any uri provided as an attribute in the tag.
              PRInt32 loc = result.Find("url", PR_TRUE);
              PRInt32 urlLoc = loc;
              if (loc > -1) {
                  // there is a url attribute, let's use it.
                  loc += 3; 
                  // go past the '=' sign
                  loc = result.Find("=", PR_TRUE, loc);
                  if (loc > -1) {
                      loc++; // leading/trailign spaces get trimmed in url creating code.
                      result.Mid(uriAttribStr, loc, result.Length() - loc);
                      uriCStr = uriAttribStr.GetUnicode();
                  }
              }

              nsIURI *uri = nsnull;
              rv = NS_NewURI(&uri, uriCStr, baseURI);
              NS_RELEASE(baseURI);
              if (NS_FAILED(rv)) return rv;

              // the units of the numeric value contained in the result are seconds.
              // we need them in milliseconds before handing off to the refresh interface.

              PRInt32 millis;
              if (urlLoc > 1) {
                  nsString2 seconds;
                  result.Left(seconds, urlLoc-2);
                  millis = seconds.ToInteger(&loc) * 1000;
              } else {
                  millis = result.ToInteger(&loc) * 1000;
              }

              nsIRefreshURI *reefer = nsnull;
              rv = mWebShell->QueryInterface(nsCOMTypeInfo<nsIRefreshURI>::GetIID(), (void**)&reefer);
              if (NS_FAILED(rv)) {
                  NS_RELEASE(uri);
                  return rv;
              };

              rv = reefer->RefreshURI(uri, millis, PR_FALSE);
              NS_RELEASE(uri);
              NS_RELEASE(reefer);
              if (NS_FAILED(rv)) return rv;
          }
#else
          if (nsnull != httpUrl) {
            char* value = result.ToNewCString(), *csHeader;
            if (!value) {
              NS_RELEASE(it);
              return NS_ERROR_OUT_OF_MEMORY;
            }
            csHeader = header.ToNewCString();
            if (!csHeader) {
                delete[] value;
                NS_RELEASE(it);
                return NS_ERROR_OUT_OF_MEMORY;
            }
            httpUrl->AddMimeHeader(csHeader, value);
            delete[] csHeader;
            delete[] value;
          }
#endif

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
        }
      }
#ifndef NECKO
      NS_IF_RELEASE(httpUrl);
#endif

      NS_RELEASE(it);
    }
  }

  return rv;
}

// Returns PR_TRUE if the language name is a version of JavaScript and
// PR_FALSE otherwise
static PRBool
IsJavaScriptLanguage(const nsString& aName)
{
  if (aName.EqualsIgnoreCase("JavaScript") || 
      aName.EqualsIgnoreCase("LiveScript") || 
      aName.EqualsIgnoreCase("Mocha")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.1")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.2")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.3")) { 
    return PR_TRUE;
  } 
  else if (aName.EqualsIgnoreCase("JavaScript1.4")) { 
    return PR_TRUE;
  } 
  else { 
    return PR_FALSE;
  } 
}

nsresult
HTMLContentSink::ResumeParsing()
{
  nsresult result=NS_OK;
  if (nsnull != mParser) {
    result=mParser->EnableParser(PR_TRUE);
  }
  
  return result;
}

PRBool
HTMLContentSink::PreEvaluateScript()
{
  // Cause frame creation and reflow of all body children that 
  // have thus far been appended. Note that if mDirty is true
  // then we know that the current body child has not yet been
  // added to the content model (and, hence, it's safe to create
  // frames for all body children).
  // We don't want the current body child to be appended (and
  // have frames be constructed for it) since the current script
  // may add new content to the tree (and cause an immediate 
  // reflow). As long as frames don't exist for the subtree rooted
  // by the current body child, new content added to the subtree
  // will not generate new frames and, hence, we won't have to 
  // worry about reflowing of incomplete content or double frame
  // creation.
  if (mDirty) {
    if (nsnull != mBody) {
      SINK_TRACE(SINK_TRACE_REFLOW,
                 ("HTMLContentSink::PreEvaluateScript: reflow content"));
      NotifyBody();
    }
    mDirty = PR_FALSE;
  }

  // Now eagerly all pending elements (including the current body child)
  // to the body (so that they can be seen by scripts). Note that frames
  // have not yet been created for them (and shouldn't be).
  mCurrentContext->FlushTags();
  
  return (nsnull != mBody);
}

void
HTMLContentSink::PostEvaluateScript(PRBool aBodyPresent)
{
  // If the script added new content directly to the body, we update
  // our body child count so that frames aren't created twice.
  if (nsnull != mBody) {
    mBody->ChildCount(mBodyChildCount);
    // If the script is not a body child, we shouldn't include
    // the element that we eagerly appended (the ancestor of the
    // script), since it is not yet complete. Of course, this
    // should only happen if the body element was present *before*
    // script evaluation (a body could have been created by
    // the script).
    if (!mCurrentContext->IsCurrentContainer(eHTMLTag_body) &&
        aBodyPresent) {
      mBodyChildCount--;
    }
  }
}

nsresult
HTMLContentSink::EvaluateScript(nsString& aScript,
                                PRInt32 aLineNo)
{
  nsresult rv = NS_OK;

  if (aScript.Length() > 0) {
    nsIScriptContextOwner *owner;
    nsIScriptContext *context;
    owner = mDocument->GetScriptContextOwner();
    if (nsnull != owner) {
      
      rv = owner->GetScriptContext(&context);
      if (rv != NS_OK) {
        NS_RELEASE(owner);
        return rv;
      }
      
      nsAutoString ret;
      nsIURI* docURL = mDocument->GetDocumentURL();
#ifdef NECKO
      char* url;
#else
      const char* url;
#endif
      if (docURL) {
        (void)docURL->GetSpec(&url);
      }
  
      PRBool isUndefined;
      context->EvaluateString(aScript, url, aLineNo, 
                              ret, &isUndefined);
      
      NS_IF_RELEASE(docURL);
      
      NS_RELEASE(context);
      NS_RELEASE(owner);
    }
  }
  
  return rv;
}

static void
nsDoneLoadingScript(nsIUnicharStreamLoader* aLoader,
                    nsString& aData,
                    void* aRef,
                    nsresult aStatus)
{
  HTMLContentSink* sink = (HTMLContentSink*)aRef;

  if (NS_OK == aStatus) {

    PRBool bodyPresent = sink->PreEvaluateScript();

    // XXX We have no way of indicating failure. Silently fail?
    sink->EvaluateScript(aData, 0);

    sink->PostEvaluateScript(bodyPresent);
  }

  sink->ResumeParsing();

  // The url loader held a reference to the sink
  NS_RELEASE(sink);

  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
}

nsresult
HTMLContentSink::ProcessSCRIPTTag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  PRBool   isJavaScript = PR_TRUE;
  PRInt32 i, ac = aNode.GetAttributeCount();

  // Look for SRC attribute and look for a LANGUAGE attribute
  nsAutoString src;
  for (i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("src")) {
      GetAttributeValueAt(aNode, i, src, nsnull);
    }
    else if (key.EqualsIgnoreCase("type")) {
      nsAutoString  type;

      GetAttributeValueAt(aNode, i, type, nsnull);

      nsAutoString  mimeType;
      nsAutoString  params;
      SplitMimeType(type, mimeType, params);

      isJavaScript = mimeType.EqualsIgnoreCase("text/javascript") || 
        mimeType.EqualsIgnoreCase("application/x-javascript");
    }
    else if (key.EqualsIgnoreCase("language")) {
      nsAutoString  lang;
       
      GetAttributeValueAt(aNode, i, lang, nsnull);
      isJavaScript = IsJavaScriptLanguage(lang);
    }
  }

  // Create content object
  NS_ASSERTION(mCurrentContext->mStackPos > 0, "leaf w/o container");
  nsIHTMLContent* parent = mCurrentContext->mStack[mCurrentContext->mStackPos-1].mContent;
  nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
  nsAutoString tag("SCRIPT");
  nsIHTMLContent* element = nsnull;
  rv = NS_CreateHTMLElement(&element, tag);
  if (NS_SUCCEEDED(rv)) {
    // Add in the attributes and add the style content object to the
    // head container.
    element->SetDocument(mDocument, PR_FALSE);
    rv = AddAttributes(aNode, element, sco);
    if (NS_FAILED(rv)) {
      NS_RELEASE(element);
      return rv;
    }
    parent->AppendChildTo(element, PR_FALSE);
  }
  else {
    NS_IF_RELEASE(sco);
    return rv;
  }
  NS_IF_RELEASE(sco);
  
  // Create a text node holding the content
  // First, get the text content of the script tag
  nsAutoString script;
  script = aNode.GetSkippedContent();

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
      text->SetDocument(mDocument, PR_FALSE);
      NS_RELEASE(text);
    }
  }

  // Don't process scripts that aren't JavaScript
  if (isJavaScript) {
    // If there is a SRC attribute...
    if (src.Length() > 0) {
      // Use the SRC attribute value to load the URL
      nsIURI* url = nsnull;
#ifndef NECKO
      nsILoadGroup* LoadGroup = nsnull;
      mDocumentBaseURL->GetLoadGroup(&LoadGroup);
      if (LoadGroup) {
        rv = LoadGroup->CreateURL(&url, mDocumentBaseURL, src, nsnull);
        NS_RELEASE(LoadGroup);
      }
      else
#endif
      {
#ifndef NECKO
        rv = NS_NewURL(&url, src, mDocumentBaseURL);
#else
        rv = NS_NewURI(&url, src, mDocumentBaseURL);
#endif // NECKO
      }
      if (NS_OK != rv) {
        return rv;
      }

      // Add a reference to this since the url loader is holding
      // onto it as opaque data.
      NS_ADDREF(this);

      nsIUnicharStreamLoader* loader;
      rv = NS_NewUnicharStreamLoader(&loader,
                                     url, 
#ifdef NECKO
                                     nsCOMPtr<nsILoadGroup>(mDocument->GetDocumentLoadGroup()),
#endif
                                     (nsStreamCompleteFunc)nsDoneLoadingScript, 
                                     (void *)this);
      NS_RELEASE(url);
      if (NS_OK == rv) {
        rv = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
    else {
      PRBool bodyPresent = PreEvaluateScript();

      PRUint32 lineNo = (PRUint32)aNode.GetSourceLineNumber();

      EvaluateScript(script, lineNo);
      
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
  PRInt32 i, count = aNode.GetAttributeCount();

  nsAutoString src;
  nsAutoString title; 
  nsAutoString type; 
  nsAutoString media; 

  nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
  for (i = 0; i < count; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("src")) {
      GetAttributeValueAt(aNode, i, src, sco);
      src.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("title")) {
      GetAttributeValueAt(aNode, i, title, sco);
      title.CompressWhitespace();
    }
    else if (key.EqualsIgnoreCase("type")) {
      GetAttributeValueAt(aNode, i, type, sco);
      type.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("media")) {
      GetAttributeValueAt(aNode, i, media, sco);
      media.ToLowerCase(); // HTML4.0 spec is inconsistent, make it case INSENSITIVE
    }
  }

  // Create content object
  nsAutoString tag("STYLE");
  nsIHTMLContent* element = nsnull;
  rv = NS_CreateHTMLElement(&element, tag);
  if (NS_SUCCEEDED(rv)) {
    // Add in the attributes and add the style content object to the
    // head container.
    element->SetDocument(mDocument, PR_FALSE);
    rv = AddAttributes(aNode, element, sco);
    if (NS_FAILED(rv)) {
      NS_RELEASE(element);
      return rv;
    }
    mHead->AppendChildTo(element, PR_FALSE);
  }
  else {
    NS_IF_RELEASE(sco);
    return rv;
  }
  NS_IF_RELEASE(sco);

  nsAutoString  mimeType;
  nsAutoString  params;
  SplitMimeType(type, mimeType, params);

  PRBool blockParser = PR_FALSE;  // hardwired off for now

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
        text->SetDocument(mDocument, PR_FALSE);
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
                                       doneLoading);
      NS_RELEASE(uin);
    } 
    else {
      // src with immediate style data doesn't add up
      // XXX what does nav do?
      // Use the SRC attribute value to load the URL
      nsIURI* url = nsnull;
#ifndef NECKO
      nsILoadGroup* LoadGroup = nsnull;
      mDocumentBaseURL->GetLoadGroup(&LoadGroup);
      if (LoadGroup) {
        rv = LoadGroup->CreateURL(&url, mDocumentBaseURL, src, nsnull);
        NS_RELEASE(LoadGroup);
      }
      else
#endif
      {
#ifndef NECKO
        rv = NS_NewURL(&url, src, mDocumentBaseURL);
#else
        rv = NS_NewURI(&url, src, mDocumentBaseURL);
#endif // NECKO
      }
      if (NS_OK != rv) {
        return rv;
      }

      rv = mCSSLoader->LoadStyleLink(element, url, title, media, kNameSpaceID_HTML,
                                     mStyleSheetCount++, 
                                     ((blockParser) ? mParser : nsnull), 
                                     doneLoading);
      NS_RELEASE(url);
    }
    if (NS_SUCCEEDED(rv) && blockParser && (! doneLoading)) {
      rv = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }

  NS_RELEASE(element);

  return rv;
}


NS_IMETHODIMP 
HTMLContentSink::NotifyError(const nsParserError* aError)
{
  // Errors in HTML? Who would have thought!
  // Why are you telling us, parser. Deal with it yourself.
  PR_ASSERT(0);
  return NS_OK;
}


NS_IMETHODIMP
HTMLContentSink::DoFragment(PRBool aFlag) 
{
  return NS_OK; 
}


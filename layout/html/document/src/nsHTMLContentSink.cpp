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
#include "nsIUnicharInputStream.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIHTMLContent.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIHttpURL.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsHTMLTokens.h"  
#include "nsHTMLEntities.h" 
#include "nsCRT.h"
#include "prtime.h"
#include "prlog.h"

#include "nsHTMLParts.h"
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

#include "nsVoidArray.h"
#include "nsIScriptContextOwner.h"

// XXX Go through a factory for this one
#include "nsICSSParser.h"

#include "nsIStyleSheetLinkingElement.h"
#include "nsIDOMHTMLTitleElement.h"

static NS_DEFINE_IID(kIDOMHTMLTitleElementIID, NS_IDOMHTMLTITLEELEMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

#define XXX_ART_HACK 1

static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLMapElementIID, NS_IDOMHTMLMAPELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLTextAreaElementIID, NS_IDOMHTMLTEXTAREAELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kIHTTPURLIID, NS_IHTTPURL_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);
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
      cp = NS_EnumToTag(nsHTMLTag(aNode.GetNodeType()));
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
                nsIURL* aURL,
                nsIWebShell* aContainer);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);  
  
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
  NS_IMETHOD OpenContainer(const nsIParserNode& aNode);
  NS_IMETHOD CloseContainer(const nsIParserNode& aNode);
  NS_IMETHOD AddLeaf(const nsIParserNode& aNode);
  NS_IMETHOD NotifyError(const nsParserError* aError);
  NS_IMETHOD AddComment(const nsIParserNode& aNode);
  NS_IMETHOD AddProcessingInstruction(const nsIParserNode& aNode);
  NS_IMETHOD DoFragment(PRBool aFlag);

  nsIDocument* mDocument;
  nsIHTMLDocument* mHTMLDocument;
  nsIURL* mDocumentURL;
  nsIURL* mDocumentBaseURL;
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
  nsVoidArray mSheetMap;

  void StartLayout();

  void ScrollToRef();

  void AddBaseTagInfo(nsIHTMLContent* aContent);

  nsresult LoadStyleSheet(nsIURL* aURL,
                          nsIUnicharInputStream* aUIN,
                          PRBool aActive,
                          const nsString& aTitle,
                          const nsString& aMedia,
                          nsIHTMLContent* aOwner,
                          PRInt32 aIndex);

  nsresult ProcessLink(nsIHTMLContent* aElement, const nsString& aLinkData);
  nsresult ProcessStyleLink(nsIHTMLContent* aElement,
                            const nsString& aHref, const nsString& aRel,
                            const nsString& aTitle, const nsString& aType,
                            const nsString& aMedia, PRBool aBlockParser);

  void ProcessBaseHref(const nsString& aBaseHref);
  void ProcessBaseTarget(const nsString& aBaseTarget);

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
  nsresult PreEvaluateScript();
  nsresult PostEvaluateScript();
  nsresult EvaluateScript(nsString& aScript,
                          PRInt32 aLineNo);

#ifdef DEBUG
  void ForceReflow() {
    mDocument->ContentAppended(mBody, mBodyChildCount);
    mBody->ChildCount(mBodyChildCount);
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
  nsresult End();

  nsresult GrowStack();
  nsresult AddText(const nsString& aText);
  nsresult FlushText(PRBool* aDidFlush = nsnull);
  nsresult FlushTags();

  PRBool   IsCurrentContainer(nsHTMLTag mType);

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
  PRInt32 index = 0;
  while (index < aString.Length()) {
    // If we have the start of an entity (and it's not at the end of
    // our string) then translate the entity into it's unicode value.
    if ((aString.CharAt(index++) == '&') && (index < aString.Length())) {
      PRInt32 start = index - 1;
      PRUnichar e = aString.CharAt(index);
      if (e == '#') {
        // Convert a numeric character reference
        index++;
        char* cp = cbuf;
        char* limit = cp + sizeof(cbuf) - 1;
        PRBool ok = PR_FALSE;
        PRInt32 slen = aString.Length();
        while ((index < slen) && (cp < limit)) {
          PRUnichar e = aString.CharAt(index);
          if (e == ';') {
            index++;
            ok = PR_TRUE;
            break;
          }
          if ((e >= '0') && (e <= '9')) {
            *cp++ = char(e);
            index++;
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
        aString.Cut(start, index - start);
        aString.Insert(PRUnichar(ch), start);
        index = start + 1;
      }
      else if (((e >= 'A') && (e <= 'Z')) ||
               ((e >= 'a') && (e <= 'z'))) {
        // Convert a named entity
        index++;
        char* cp = cbuf;
        char* limit = cp + sizeof(cbuf) - 1;
        *cp++ = char(e);
        PRBool ok = PR_FALSE;
        PRInt32 slen = aString.Length();
        while ((index < slen) && (cp < limit)) {
          PRUnichar e = aString.CharAt(index);
          if (e == ';') {
            index++;
            ok = PR_TRUE;
            break;
          }
          if (((e >= '0') && (e <= '9')) ||
              ((e >= 'A') && (e <= 'Z')) ||
              ((e >= 'a') && (e <= 'z'))) {
            *cp++ = char(e);
            index++;
            continue;
          }
          break;
        }
        if (!ok || (cp == cbuf)) {
          continue;
        }
        *cp = '\0';
        PRInt32 ch = NS_EntityToUnicode(cbuf);
        if (ch < 0) {
          continue;
        }

        // Remove entity from string and replace it with the integer
        // value.
        aString.Cut(start, index - start);
        aString.Insert(PRUnichar(ch), start);
        index = start + 1;
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
              nsIScriptContextOwner* aScriptContextOwner)
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
      aContent->SetAttribute(kNameSpaceID_HTML, keyAtom, v, PR_FALSE);
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
    tmp.Append(NS_EnumToTag(aNodeType));
  }
  nsIAtom* atom = NS_NewAtom(tmp);
  if (nsnull == atom) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make the content object
  nsresult rv;
 
  // XXX right now this code is here because we need aNode to create
  // images, textareas and input form elements. As soon as all of the
  // generic content code is in use, it can be moved up into
  // MakeContentObject
  switch (aNodeType) {
  case eHTMLTag_img:
#ifdef XXX_ART_HACK
    {
      /* HACK - Jim Dunn 8/6
       * Check to see if this is an ART image type
       * If so then process it using the ART plugin
       * Otherwise treat it like a normal image
       */

      PRBool bArt = PR_FALSE;
      nsAutoString v;
      PRInt32 ac = aNode.GetAttributeCount();
      for (PRInt32 i = 0; i < ac; i++)    /* Go through all of this tag's attributes */
      {
        const nsString& key = aNode.GetKeyAt(i);

        if (!key.Compare("SRC", PR_TRUE))   /* Find the SRC (source) tag */
        {
          const nsString& key2 = aNode.GetValueAt(i);

          v.Truncate();
          v.Append(key2);
          v.ToLowerCase();
          if (-1 != v.Find(".art"))   /* See if it has an ART extension */
          {
            bArt = PR_TRUE;    /* Treat this like an embed */
            break;
          }
        }
        if (!key.Compare("TYPE", PR_TRUE))  /* Find the TYPE (mimetype) tag */
        {
          const nsString& key2 = aNode.GetValueAt(i);

          v.Truncate();
          v.Append(key2);
          v.ToLowerCase();
          if ((-1 != v.Find("image/x-art"))   /* See if it has an ART Mimetype */
              || (-1 != v.Find("image/art"))
              || (-1 != v.Find("image/x-jg")))
          {
            bArt = PR_TRUE;    /* Treat this like an embed */
            break;
          }
        }
      }
      if (bArt)
        rv = NS_NewHTMLEmbedElement(aResult, atom);
      else
        rv = NS_NewHTMLImageElement(aResult, atom);
    }
#else
    rv = NS_NewHTMLImageElement(aResult, atom);
#endif /* XXX */
    break;
  default:
    {
      // XXX why is textarea not a container?
      nsAutoString content;
      if (eHTMLTag_textarea == aNodeType) {
        content = aNode.GetSkippedContent();
      }
      rv = MakeContentObject(aNodeType, atom, aForm, aWebShell, aResult, &content);
      break;
    }
  }
  NS_RELEASE(atom);

  return rv;
}

nsresult
NS_CreateHTMLElement(nsIHTMLContent** aResult, const nsString& aTag)
{
  // Find tag in tag table
  nsAutoString tmp(aTag);
  tmp.ToLowerCase();
  char cbuf[20];
  tmp.ToCString(cbuf, sizeof(cbuf));
  nsHTMLTag id = NS_TagToEnum(cbuf);
  if (eHTMLTag_userdefined == id) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Create atom for tag and then create content object
  nsIAtom* atom = NS_NewAtom(tmp);
  nsresult rv = MakeContentObject(id, atom, nsnull, nsnull, aResult);
  NS_RELEASE(atom);

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
    parent->AppendChildTo(content, PR_FALSE);
  }
  NS_IF_RELEASE(content);

  // Special handling for certain tags
  switch (nodeType) {
  case eHTMLTag_table:
    mSink->mInMonolithicContainer--;
    break;
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

  return NS_OK;
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
      static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);
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
                      nsIURL* aURL,
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
                      nsIURL* aURL,
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

  const char* spec;
  (void)aURL->GetSpec(&spec);
  SINK_TRACE(SINK_TRACE_CALLS,
             ("HTMLContentSink::Init: this=%p url='%s'",
              this, spec));

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
    mDocument->ContentAppended(mBody, mBodyChildCount);
    mBody->ChildCount(mBodyChildCount);
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
      mDocument->ContentAppended(mBody, mBodyChildCount);
      mBody->ChildCount(mBodyChildCount);
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
    *mTitle = aValue;
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
  // Check for attributes on the second body and apply them to the
  // existing BODY node.
  if(mBody != nsnull){
    PRInt32 attrCount = aNode.GetAttributeCount();
    if(attrCount){
      nsAutoString k, newValue;
      nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
      for (PRInt32 index = 0; index < attrCount; index++) {
        // Get upper-cased key
        const nsString& key = aNode.GetKeyAt(index);
 
        k.Truncate();
        k.Append(key);
        k.ToLowerCase();

        nsIAtom*  keyAtom = NS_NewAtom(k);
        nsHTMLValue oldValue;
    
        // Get value and remove mandatory quotes
        GetAttributeValueAt(aNode, index, newValue, sco);
        // Add attribute to body
        mBody->SetAttribute(kNameSpaceID_HTML, keyAtom, newValue, PR_TRUE);
        NS_RELEASE(keyAtom);
      }
      NS_IF_RELEASE(sco);      
    }
    NS_ADDREF(mBody);
    mCurrentContext->mStackPos++;
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
    mDocument->ContentAppended(mBody, mBodyChildCount);
    mBody->ChildCount(mBodyChildCount);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenForm(const nsIParserNode& aNode)
{
  mCurrentContext->FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenForm", aNode);

  // Close out previous form if it's there
  NS_IF_RELEASE(mCurrentForm);

  // set the current form
  nsAutoString tmp("form");
  nsIAtom* atom = NS_NewAtom(tmp);
  nsIHTMLContent* iContent = nsnull;
  nsresult rv = NS_NewHTMLFormElement(&iContent, atom);
  if ((NS_OK == rv) && iContent) {
    iContent->QueryInterface(kIDOMHTMLFormElementIID, (void**)&mCurrentForm);
    NS_RELEASE(iContent);
  }
  NS_RELEASE(atom);

  AddLeaf(aNode);

  // add the form to the document
  if (mCurrentForm) {
    mHTMLDocument->AddForm(mCurrentForm);
  }

  return NS_OK;
}

// XXX MAYBE add code to place close form tag into the content model
// for navigator layout compatability.
NS_IMETHODIMP
HTMLContentSink::CloseForm(const nsIParserNode& aNode)
{
  mCurrentContext->FlushText();
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseForm", aNode);
  NS_IF_RELEASE(mCurrentForm);
  return NS_OK;
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
        vm->EnableRefresh();
      }
    }
  }

  // If the document we are loading has a reference or it is a 
  // frameset document, disable the scroll bars on the views.
  const char* ref;
  nsresult rv = mDocumentURL->GetRef(&ref);
  if (rv == NS_OK) {
    mRef = new nsString(ref);
  }

  if ((nsnull != ref) || mFrameset) {
    // XXX support more than one presentation-shell here

    // Get initial scroll preference and save it away; disable the
    // scroll bars.
    PRInt32 i, ns = mDocument->GetNumberOfShells();
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
  nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    nsAutoString value;
    if (key.EqualsIgnoreCase("href")) {
      GetAttributeValueAt(aNode, i, value, sco);
      ProcessBaseHref(value);
    } else if (key.EqualsIgnoreCase("target")) {
      GetAttributeValueAt(aNode, i, value, sco);
      ProcessBaseTarget(value);
    }
  }
  NS_RELEASE(sco);
  return NS_OK;
}

typedef struct {
  nsString mTitle;
  nsString mMedia;
  PRBool mIsActive;
  PRBool mBlocked;
  nsIURL* mURL;
  nsIHTMLContent* mElement;
  HTMLContentSink* mSink;
  PRInt32 mIndex;
} nsAsyncStyleProcessingDataHTML;

static void
nsDoneLoadingStyle(nsIUnicharStreamLoader* aLoader,
                   nsString& aData,
                   void* aRef,
                   nsresult aStatus)
{
  nsresult rv = NS_OK;
  nsAsyncStyleProcessingDataHTML* d = (nsAsyncStyleProcessingDataHTML*)aRef;
  nsIUnicharInputStream* uin = nsnull;

  if ((NS_OK == aStatus) && (0 < aData.Length())) {
    // wrap the string with the CSS data up in a unicode
    // input stream.
    rv = NS_NewStringUnicharInputStream(&uin, new nsString(aData));
    if (NS_OK == rv) {
      // XXX We have no way of indicating failure. Silently fail?
      rv = d->mSink->LoadStyleSheet(d->mURL, uin, d->mIsActive, 
                                    d->mTitle, d->mMedia, d->mElement, d->mIndex);
      NS_RELEASE(uin);
    }
  }

  if (d->mBlocked) {
    d->mSink->ResumeParsing();
  }
    
  NS_RELEASE(d->mURL);
  NS_IF_RELEASE(d->mElement);
  NS_RELEASE(d->mSink);
  delete d;

  // We added a reference when the loader was created. This
  // release should destroy it.
  NS_RELEASE(aLoader);
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
  PRBool blockParser = PR_FALSE;
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
            }
          }
          else if (attr.EqualsIgnoreCase("wait")) {
            blockParser = PR_TRUE;
          }
        }
      }
    }
    if (kCommaCh == endCh) {  // hit a comma, process what we've got so far
      if (0 < href.Length()) {
        result = ProcessStyleLink(aElement, href, rel, title, type, media, blockParser);
        if (blockParser) {
          didBlock = PR_TRUE;
        }
      }
      href.Truncate();
      rel.Truncate();
      title.Truncate();
      type.Truncate();
      media.Truncate();
      blockParser = PR_FALSE;
    }

    start = ++end;
  }

  if (0 < href.Length()) {
    result = ProcessStyleLink(aElement, href, rel, title, type, media, blockParser);
    if (NS_SUCCEEDED(result) && (blockParser || didBlock)) {
      result = NS_ERROR_HTMLPARSER_BLOCK;
    }
  }
  return result;
}

nsresult
HTMLContentSink::ProcessStyleLink(nsIHTMLContent* aElement,
                                  const nsString& aHref, const nsString& aRel,
                                  const nsString& aTitle, const nsString& aType,
                                  const nsString& aMedia, PRBool aBlockParser)
{
  nsresult result = NS_OK;

  if (aRel.EqualsIgnoreCase("stylesheet") || 
      ((aRel.EqualsIgnoreCase("alternate stylesheet") ||
        aRel.EqualsIgnoreCase("stylesheet alternate")) && 
       (0 < aTitle.Length()))) {
    if ((0 == aType.Length()) || aType.EqualsIgnoreCase("text/css")) {
      nsIURL* url = nsnull;
      nsIURLGroup* urlGroup = nsnull;
      mDocumentBaseURL->GetURLGroup(&urlGroup);
      if (urlGroup) {
        result = urlGroup->CreateURL(&url, mDocumentBaseURL, aHref, nsnull);
        NS_RELEASE(urlGroup);
      }
      else {
        result = NS_NewURL(&url, aHref, mDocumentBaseURL);
      }
      if (NS_OK != result) {
        return NS_OK; // The URL is bad, move along, don't propogate the error (for now)
      }

      PRBool isPersistent = PR_FALSE;
      PRBool isPreferred  = PR_FALSE;
      PRBool isAlternate  = PR_FALSE;
      if (aRel.EqualsIgnoreCase("stylesheet")) {
        if (0 == aTitle.Length()) {
          isPersistent = PR_TRUE;
        }
        else {
          if (0 == mPreferredStyle.Length()) {
            isPreferred = PR_TRUE;
            mPreferredStyle = aTitle;
            mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle, aTitle);
          }
          else {
            if (aTitle.EqualsIgnoreCase(mPreferredStyle)) {
              isPreferred = PR_TRUE;
            }
            else {
              isAlternate = PR_TRUE;
            }
          }
        }
      }
      else {
        isAlternate = PR_TRUE;
      }

      nsAsyncStyleProcessingDataHTML* d = new nsAsyncStyleProcessingDataHTML;
      if (nsnull == d) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      d->mTitle.SetString(aTitle);
      d->mMedia.SetString(aMedia);
      d->mIsActive = isPersistent;
      d->mBlocked = aBlockParser;
      d->mURL = url;
      NS_ADDREF(url);
      d->mElement = aElement;
      NS_IF_ADDREF(aElement);
      d->mSink = this;
      NS_ADDREF(this);
      d->mIndex = mStyleSheetCount++; // preserve ordering
      
      nsIUnicharStreamLoader* loader;
      result = NS_NewUnicharStreamLoader(&loader,
                                         url, 
                                         (nsStreamCompleteFunc)nsDoneLoadingStyle, 
                                         (void *)d);
      NS_RELEASE(url);
      if (NS_SUCCEEDED(result) && aBlockParser) {
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
  PRInt32   index;
  PRInt32   count = aNode.GetAttributeCount();

  nsAutoString href;
  nsAutoString rel; 
  nsAutoString title; 
  nsAutoString type; 
  nsAutoString media; 
  PRBool blockParser = PR_FALSE;

  nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
  for (index = 0; index < count; index++) {
    const nsString& key = aNode.GetKeyAt(index);
    if (key.EqualsIgnoreCase("href")) {
      GetAttributeValueAt(aNode, index, href, sco);
      href.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("rel")) {
      GetAttributeValueAt(aNode, index, rel, sco);
      rel.CompressWhitespace();
    }
    else if (key.EqualsIgnoreCase("title")) {
      GetAttributeValueAt(aNode, index, title, sco);
      title.CompressWhitespace();
    }
    else if (key.EqualsIgnoreCase("type")) {
      GetAttributeValueAt(aNode, index, type, sco);
      type.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("media")) {
      GetAttributeValueAt(aNode, index, media, sco);  // media is case sensitive
    }
    else if (key.EqualsIgnoreCase("wait")) {
      blockParser = PR_TRUE;
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

  result = ProcessStyleLink(element, href, rel, title, type, media, blockParser);

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
  // XXX NAV QUIRK
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

      // If we are processing an HTTP url, handle meta http-equiv cases
      nsIHttpURL* httpUrl = nsnull;
      rv = mDocumentURL->QueryInterface(kIHTTPURLIID, (void **)&httpUrl);
 
      // set any HTTP-EQUIV data into document's header data as well as url
      nsAutoString header;
      it->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::httpEquiv, header);
      if (header.Length() > 0) {
        nsAutoString result;
        it->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::content, result);
        if (result.Length() > 0) {
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

          header.ToLowerCase();
          nsIAtom* fieldAtom = NS_NewAtom(header);
          mDocument->SetHeaderData(fieldAtom, result);

          if (fieldAtom == nsHTMLAtoms::headerDefaultStyle) {
            mPreferredStyle = result;
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
      NS_IF_RELEASE(httpUrl);

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
  if (nsnull != mParser) {
    mParser->EnableParser(PR_TRUE);
  }
  
  return NS_OK;
}

nsresult
HTMLContentSink::PreEvaluateScript()
{
  // Cause frame creation and reflow of all body children that 
  // have thus far been appended. Note that if mDirty is true
  // then we know that the current body child has not yet been
  // added to the content model. 
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
      mDocument->ContentAppended(mBody, mBodyChildCount);
      mBody->ChildCount(mBodyChildCount);
    }
    mDirty = PR_FALSE;
  }

  return mCurrentContext->FlushTags();
}

nsresult
HTMLContentSink::PostEvaluateScript()
{
  // If the script added new content directly to the body, we update
  // our body child count so that frames aren't created twice.
  if (nsnull != mBody) {
    mBody->ChildCount(mBodyChildCount);
    // If the script is not a body child, we shouldn't include
    // the element that we eagerly appended (the ancestor of the
    // script), since it is not yet complete.
    if (!mCurrentContext->IsCurrentContainer(eHTMLTag_body)) {
      mBodyChildCount--;
    }
  }

  return NS_OK;
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
      nsIURL* docURL = mDocument->GetDocumentURL();
      const char* url;
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

    sink->PreEvaluateScript();

    // XXX We have no way of indicating failure. Silently fail?
    sink->EvaluateScript(aData, 0);

    sink->PostEvaluateScript();
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
      isJavaScript = type.EqualsIgnoreCase("text/javascript") || 
        type.EqualsIgnoreCase("application/x-javascript");
    }
    else if (key.EqualsIgnoreCase("language")) {
      nsAutoString  lang;
       
      GetAttributeValueAt(aNode, i, lang, nsnull);
      isJavaScript = IsJavaScriptLanguage(lang);
    }
  }

  // Don't process scripts that aren't JavaScript
  if (isJavaScript) {
    nsAutoString script;

    // If there is a SRC attribute...
    if (src.Length() > 0) {
      // Use the SRC attribute value to load the URL
      nsIURL* url = nsnull;
      nsIURLGroup* urlGroup = nsnull;
      mDocumentBaseURL->GetURLGroup(&urlGroup);
      if (urlGroup) {
        rv = urlGroup->CreateURL(&url, mDocumentBaseURL, src, nsnull);
        NS_RELEASE(urlGroup);
      }
      else {
        rv = NS_NewURL(&url, src, mDocumentBaseURL);
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
                                     (nsStreamCompleteFunc)nsDoneLoadingScript, 
                                     (void *)this);
      NS_RELEASE(url);
      if (NS_OK == rv) {
        rv = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
    else {
      PRBool enabled = PR_TRUE;

      PreEvaluateScript();

      // Otherwise, get the text content of the script tag
      script = aNode.GetSkippedContent();

      PRUint32 lineNo = (PRUint32)aNode.GetSourceLineNumber();

      EvaluateScript(script, lineNo);
      
      PostEvaluateScript();

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
// XXX This code and ProcessSCRIPTTag share alot in common; clean that up!

nsresult
HTMLContentSink::ProcessSTYLETag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  PRInt32 index, count = aNode.GetAttributeCount();

  nsAutoString src;
  nsAutoString title; 
  nsAutoString type; 
  nsAutoString media; 
  PRBool blockParser = PR_FALSE;

  nsIScriptContextOwner* sco = mDocument->GetScriptContextOwner();
  for (index = 0; index < count; index++) {
    const nsString& key = aNode.GetKeyAt(index);
    if (key.EqualsIgnoreCase("src")) {
      GetAttributeValueAt(aNode, index, src, sco);
      src.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("title")) {
      GetAttributeValueAt(aNode, index, title, sco);
      title.CompressWhitespace();
    }
    else if (key.EqualsIgnoreCase("type")) {
      GetAttributeValueAt(aNode, index, type, sco);
      type.StripWhitespace();
    }
    else if (key.EqualsIgnoreCase("media")) {
      GetAttributeValueAt(aNode, index, media, sco);  // case sensative
    }
    else if (key.EqualsIgnoreCase("wait")) {
      blockParser = PR_TRUE;
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

  if ((0 == type.Length()) || type.EqualsIgnoreCase("text/css")) { 
    // The skipped content contains the inline style data
    const nsString& content = aNode.GetSkippedContent();

    nsIUnicharInputStream* uin = nsnull;
    if (0 == src.Length()) {
      // Create a string to hold the data and wrap it up in a unicode
      // input stream.
      rv = NS_NewStringUnicharInputStream(&uin, new nsString(content));
      if (NS_OK != rv) {
        return rv;
      }

      // Now that we have a url and a unicode input stream, parse the
      // style sheet.
      rv = LoadStyleSheet(mDocumentBaseURL, uin, PR_TRUE, title, media, element,
                          mStyleSheetCount++);
      NS_RELEASE(uin);
    } 
    else {
      // src with immediate style data doesn't add up
      // XXX what does nav do?
      // Use the SRC attribute value to load the URL
      nsIURL* url = nsnull;
      nsIURLGroup* urlGroup = nsnull;
      mDocumentBaseURL->GetURLGroup(&urlGroup);
      if (urlGroup) {
        rv = urlGroup->CreateURL(&url, mDocumentBaseURL, src, nsnull);
        NS_RELEASE(urlGroup);
      }
      else {
        rv = NS_NewURL(&url, src, mDocumentBaseURL);
      }
      if (NS_OK != rv) {
        return rv;
      }

      nsAsyncStyleProcessingDataHTML* d = new nsAsyncStyleProcessingDataHTML;
      if (nsnull == d) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      d->mTitle.SetString(title);
      d->mMedia.SetString(media);
      d->mIsActive = PR_TRUE;
      d->mBlocked = blockParser;
      d->mURL = url;
      NS_ADDREF(url);
      d->mElement = element;
      NS_ADDREF(element);
      d->mSink = this;
      NS_ADDREF(this);
      d->mIndex = mStyleSheetCount++; // preserve ordering

      nsIUnicharStreamLoader* loader;
      rv = NS_NewUnicharStreamLoader(&loader,
                                     url, 
                                     (nsStreamCompleteFunc)nsDoneLoadingStyle, 
                                     (void *)d);
      NS_RELEASE(url);
      if (NS_SUCCEEDED(rv) && blockParser) {
        rv = NS_ERROR_HTMLPARSER_BLOCK;
      }
    }
  }

  NS_RELEASE(element);

  return rv;
}


typedef PRBool (*nsStringEnumFunc)(const nsString& aSubString, void *aData);
const PRUnichar kHyphenCh = PRUnichar('-');

static PRBool EnumerateMediaString(const nsString& aStringList, nsStringEnumFunc aFunc, void* aData)
{
  PRBool    running = PR_TRUE;

  nsAutoString  stringList(aStringList); // copy to work buffer
  nsAutoString  subStr;

  stringList.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)stringList.GetUnicode();
  PRUnichar* end   = start;

  while (running && (kNullCh != *start)) {
    PRBool  quoted = PR_FALSE;

    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }

    if ((kApostrophe == *start) || (kQuote == *start)) { // quoted string
      PRUnichar quote = *start++;
      quoted = PR_TRUE;
      end = start;
      while (kNullCh != *end) {
        if (quote == *end) {  // found closing quote
          *end++ = kNullCh;     // end string here
          while ((kNullCh != *end) && (kComma != *end)) { // keep going until comma
            end++;
          }
          break;
        }
        end++;
      }
    }
    else {  // non-quoted string or ended
      end = start;

      while ((kNullCh != *end) && (kComma != *end)) { // look for comma
        end++;
      }
      *end = kNullCh; // end string here
    }

    // truncate at first non letter, digit or hyphen
    PRUnichar* test = start;
    while (test <= end) {
      if ((PR_FALSE == nsString::IsAlpha(*test)) && 
          (PR_FALSE == nsString::IsDigit(*test)) && (kHyphenCh != *test)) {
        *test = kNullCh;
        break;
      }
      test++;
    }
    subStr = start;

    if (PR_FALSE == quoted) {
      subStr.CompressWhitespace(PR_FALSE, PR_TRUE);
    }

    if (0 < subStr.Length()) {
      running = (*aFunc)(subStr, aData);
    }

    start = ++end;
  }

  return running;
}

static PRBool MediumEnumFunc(const nsString& aSubString, void *aData)
{
  nsIAtom*  medium = NS_NewAtom(aSubString);
  ((nsICSSStyleSheet*)aData)->AppendMedium(medium);
  return PR_TRUE;
}

nsresult
HTMLContentSink::LoadStyleSheet(nsIURL* aURL,
                                nsIUnicharInputStream* aUIN,
                                PRBool aActive,
                                const nsString& aTitle,
                                const nsString& aMedia,
                                nsIHTMLContent* aOwner,
                                PRInt32 aIndex)
{
  /* XXX use repository */
  nsICSSParser* parser;
  nsresult rv = NS_NewCSSParser(&parser);
  if (NS_OK == rv) {
    nsICSSStyleSheet* sheet = nsnull;
    // XXX note: we are ignoring rv until the error code stuff in the
    // input routines is converted to use nsresult's
    parser->SetCaseSensitive(PR_FALSE);
    parser->Parse(aUIN, aURL, sheet);
    if (nsnull != sheet) {
      sheet->SetTitle(aTitle);
      if (aActive) {
        sheet->SetEnabled(PR_TRUE);
      }
      else {  // is alternate, test if preferred
        NS_ASSERTION(0 < aTitle.Length(), "alternate sheets need titles");
        sheet->SetEnabled(aTitle.EqualsIgnoreCase(mPreferredStyle));
      }
      if (0 < aMedia.Length()) {
        EnumerateMediaString(aMedia, MediumEnumFunc, sheet);
      }
      if (nsnull != aOwner) {
        nsIDOMNode* domNode = nsnull;
        if (NS_SUCCEEDED(aOwner->QueryInterface(kIDOMNodeIID, (void**)&domNode))) {
          sheet->SetOwningNode(domNode);
          NS_RELEASE(domNode);
        }

        nsIStyleSheetLinkingElement* element;
        if (NS_SUCCEEDED(aOwner->QueryInterface(kIStyleSheetLinkingElementIID,
                                                (void**)&element))) {
          element->SetStyleSheet(sheet);
          NS_RELEASE(element);
        }
      }

      PRInt32 insertIndex = mSheetMap.Count();
      PRBool notify = PRBool((mSheetMap.Count() + 1) == mStyleSheetCount);
        // always notify on last sheet
      while (0 <= --insertIndex) {
        PRInt32 targetIndex = (PRInt32)mSheetMap.ElementAt(insertIndex);
        if (targetIndex < aIndex) {
          mHTMLDocument->InsertStyleSheetAt(sheet, insertIndex + 1, notify);
          mSheetMap.InsertElementAt((void*)aIndex, insertIndex + 1);
          NS_RELEASE(sheet);
          break;
        }
      }
      if (nsnull != sheet) { // didn't insert yet
        mHTMLDocument->InsertStyleSheetAt(sheet, 0, notify);
        mSheetMap.InsertElementAt((void*)aIndex, 0);
        NS_RELEASE(sheet);
      }

      rv = NS_OK;
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;/* XXX */
    }
    NS_RELEASE(parser);
  }
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


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
#include "nsIHTMLContentSink.h"
#include "nsIStyleSheet.h"
#include "nsIUnicharInputStream.h"
#include "nsIHTMLContent.h"
#include "nsIURL.h"
#include "nsHTMLDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIViewManager.h"
#include "nsHTMLTokens.h" 
#include "nsCRT.h"
#include "prtime.h"
#include "prlog.h"

#include "nsHTMLParts.h"
#include "nsTablePart.h"
#include "nsTableRow.h"
#include "nsTableCell.h"
#include "nsIDOMText.h"

#include "nsHTMLForms.h"
#include "nsIFormManager.h"
#include "nsIFormControl.h"
#include "nsIImageMap.h"

#include "nsRepository.h"

#include "nsIWebShell.h"
extern nsresult NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                                 nsIAtom* aTag, nsIWebShell* aWebShell);  // XXX move
extern nsresult NS_NewHTMLFrame(nsIHTMLContent** aInstancePtrResult,
                                 nsIAtom* aTag, nsIWebShell* aWebShell);  // XXX move
extern nsresult NS_NewHTMLFrameset(nsIHTMLContent** aInstancePtrResult,
                                 nsIAtom* aTag, nsIWebShell* aWebShell);  // XXX move

// XXX attribute values have entities in them - use the parsers expander!

// XXX Go through a factory for this one
#include "nsICSSParser.h"

#ifdef NS_DEBUG
static PRLogModuleInfo* gSinkLogModuleInfo;

#define SINK_TRACE_CALLS        0x1
#define SINK_TRACE_REFLOW       0x2

#define SINK_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#define SINK_TRACE(_bit,_args)                    \
  PR_BEGIN_MACRO                                  \
    if (SINK_LOG_TEST(gSinkLogModuleInfo,_bit)) { \
      PR_LogPrint _args;                          \
    }                                             \
  PR_END_MACRO

#define SINK_TRACE_NODE(_bit,_msg,_node)                     \
  PR_BEGIN_MACRO                                             \
    if (SINK_LOG_TEST(gSinkLogModuleInfo,_bit)) {            \
      char cbuf[40];                                         \
      const char* cp;                                        \
      if ((_node).GetNodeType() != eHTMLTag_userdefined) {   \
        cp = NS_EnumToTag(nsHTMLTag((_node).GetNodeType())); \
      } else {                                               \
        (_node).GetText().ToCString(cbuf, sizeof(cbuf));     \
        cp = cbuf;                                           \
      }                                                      \
      PR_LogPrint("%s: node='%s'", _msg, cp);                \
    }                                                        \
  PR_END_MACRO

#else
#define SINK_TRACE(_bit,_args)
#define SINK_TRACE_NODE(_bit,_msg,_node)
#endif

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTML_CONTENT_SINK_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);

class HTMLContentSink : public nsIHTMLContentSink {
public:

  HTMLContentSink();
  ~HTMLContentSink();

  void* operator new(size_t size) {
    void* rv = ::operator new(size);
    nsCRT::zero(rv, size);
    return (void*) rv;
  }

  nsresult Init(nsIDocument* aDoc,
                nsIURL* aURL,
                nsIWebShell* aContainer);
  nsIHTMLContent* GetCurrentContainer(eHTMLTags* aType);
  nsIHTMLContent* GetTableParent();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(PRInt32 aQualityLevel);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);

  // nsIHTMLContentSink
  NS_IMETHOD PushMark();
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

protected:

  void StartLayout();

  void ReflowNewContent();

  //----------------------------------------------------------------------
  // Leaf tag handler routines that translate a leaf tag into a
  // content object, processing all of the tag attributes.

  nsresult ProcessAREATag(const nsIParserNode& aNode);
  nsresult ProcessBASETag(const nsIParserNode& aNode);
  nsresult ProcessMETATag(const nsIParserNode& aNode);
  nsresult ProcessSTYLETag(const nsIParserNode& aNode);
  nsresult ProcessSCRIPTTag(const nsIParserNode& aNode);

  nsresult ProcessBRTag(nsIHTMLContent** aInstancePtrResult,
                        const nsIParserNode& aNode);
  nsresult ProcessEMBEDTag(nsIHTMLContent** aInstancePtrResult,
                           const nsIParserNode& aNode);
  nsresult ProcessFrameTag(nsIHTMLContent** aInstancePtrResult,
                           const nsIParserNode& aNode);
  nsresult ProcessHRTag(nsIHTMLContent** aInstancePtrResult,
                        const nsIParserNode& aNode);
  nsresult ProcessINPUTTag(nsIHTMLContent** aInstancePtrResult,
                           const nsIParserNode& aNode);
  nsresult ProcessIMGTag(nsIHTMLContent** aInstancePtrResult,
                         const nsIParserNode& aNode);
  nsresult ProcessSPACERTag(nsIHTMLContent** aInstancePtrResult,
                            const nsIParserNode& aNode);
  nsresult ProcessTEXTAREATag(nsIHTMLContent** aInstancePtrResult,
                              const nsIParserNode& aNode);
  nsresult ProcessWBRTag(nsIHTMLContent** aInstancePtrResult,
                         const nsIParserNode& aNode);

  //----------------------------------------------------------------------

  nsresult ProcessOpenSELECTTag(nsIHTMLContent** aInstancePtrResult,
                                const nsIParserNode& aNode);
  nsresult ProcessCloseSELECTTag(const nsIParserNode& aNode);
  nsresult ProcessOpenOPTIONTag(nsIHTMLContent** aInstancePtrResult,
	                            const nsIParserNode& aNode);
  nsresult ProcessCloseOPTIONTag(const nsIParserNode& aNode);
  nsresult ProcessOPTIONTagContent(const nsIParserNode& aNode);

  nsresult ProcessIFRAMETag(nsIHTMLContent** aInstancePtrResult,
                            const nsIParserNode& aNode);
  nsresult ProcessFRAMESETTag(nsIHTMLContent** aInstancePtrResult,
                            const nsIParserNode& aNode);
  //----------------------------------------------------------------------

  void FlushText();

  nsresult AddText(const nsString& aText, nsIHTMLContent** aContent);

  void AppendToCorrectParent(nsHTMLTag aParentTag,
                             nsIHTMLContent* aParent,
                             nsHTMLTag aChildTag,
                             nsIHTMLContent* aChild,
                             PRBool aAllowReflow);

  void GetAttributeValueAt(const nsIParserNode& aNode,
                           PRInt32 aIndex,
                           nsString& aResult);

  PRBool FindAttribute(const nsIParserNode& aNode,
                       const nsString& aKeyName,
                       nsString& aResult);

  nsresult AddAttributes(const nsIParserNode& aNode,
                         nsIHTMLContent* aInstancePtrResult);

  nsIHTMLContent* GetBodyOrFrameset() { if (mBody) return mBody; else return mFrameset; }

  nsresult LoadStyleSheet(nsIURL* aURL,
                          nsIUnicharInputStream* aUIN);

  nsIDocument* mDocument;
  nsIURL* mDocumentURL;

  eHTMLTags mNodeStack[100];/* XXX */
  nsIHTMLContent* mContainerStack[100];/* XXX */
  PRInt32 mStackPos;
  nsString* mTitle;
  nsString mBaseHREF;
  nsString mBaseTarget;
  nsIStyleSheet* mStyleSheet;
  nsIFormManager* mCurrentForm;
  nsIImageMap* mCurrentMap;
  nsIHTMLContent* mCurrentSelect;
  nsIHTMLContent* mCurrentOption;
  nsIDOMText* mCurrentText;

  nsIHTMLContent* mRoot;
  nsIHTMLContent* mBody;
  nsIHTMLContent* mFrameset;
  nsIHTMLContent* mHead;

  PRTime mLastUpdateTime;
  PRTime mUpdateDelta;
  PRBool mLayoutStarted;
  PRInt32 mInMonolithicContainer;
  nsIWebShell* mWebShell;

  // XXX The parser needs to keep track of body tags and frameset tags 
  // and tell the content sink if they are to be ignored. For example, in nav4
  // <html><body><frameset> ignores the frameset and 
  // <html><frameset><body> ignores the body
};

// Note: operator new zeros our memory
HTMLContentSink::HTMLContentSink()
{
#ifdef NS_DEBUG
  if (nsnull == gSinkLogModuleInfo) {
    gSinkLogModuleInfo = PR_NewLogModule("htmlcontentsink");
  }
#endif

  // Set the first update delta to be 50ms
  LL_I2L(mUpdateDelta, PR_USEC_PER_MSEC * 50);
}
 
HTMLContentSink::~HTMLContentSink()
{
  NS_IF_RELEASE(mHead);
  NS_IF_RELEASE(mBody);
  NS_IF_RELEASE(mFrameset);
  NS_IF_RELEASE(mRoot);
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mStyleSheet);
  NS_IF_RELEASE(mCurrentForm);
  NS_IF_RELEASE(mCurrentMap);
  NS_IF_RELEASE(mCurrentSelect);
  NS_IF_RELEASE(mCurrentOption);
  NS_IF_RELEASE(mWebShell);

  if (nsnull != mTitle) {
    delete mTitle;
  }
}

nsresult
HTMLContentSink::Init(nsIDocument* aDoc,
                      nsIURL* aDocURL,
                      nsIWebShell* aWebShell)
{
  NS_IF_RELEASE(mDocument);
  mDocument = aDoc;
  NS_IF_ADDREF(mDocument);

  NS_IF_RELEASE(mDocumentURL);
  mDocumentURL = aDocURL;
  NS_IF_ADDREF(mDocumentURL);

  NS_IF_RELEASE(mWebShell);
  mWebShell = aWebShell;
  NS_IF_ADDREF(mWebShell);

  // Make root part
  NS_IF_RELEASE(mRoot);
  nsresult rv = NS_NewRootPart(&mRoot, aDoc);
  if (NS_OK != rv) {
    return rv;
  }

  // Make head container
  NS_IF_RELEASE(mHead);
  nsIAtom* atom = NS_NewAtom("HEAD");
  if (nsnull == atom) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = NS_NewHTMLHead(&mHead, atom);
  NS_RELEASE(atom);
  if (NS_OK != rv) {
    return rv;
  }
  mRoot->AppendChild(mHead, PR_FALSE);

  return rv;
}

NS_IMPL_ISUPPORTS(HTMLContentSink,kIHTMLContentSinkIID)

NS_IMETHODIMP
HTMLContentSink::OpenHTML(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenHTML", aNode);
  NS_PRECONDITION(0 == mStackPos, "bad stack pos");

  mNodeStack[0] = (eHTMLTags)aNode.GetNodeType();
  mContainerStack[0] = mRoot;
  mStackPos = 1;

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseHTML(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseHTML", aNode);

  // XXX this is the way it used to be
  //NS_ASSERTION(mStackPos > 0, "bad bad");
  //mNodeStack[--mStackPos] = eHTMLTag_unknown;
  if (mStackPos > 0) {
    mNodeStack[--mStackPos] = eHTMLTag_unknown;
  }

  NS_IF_RELEASE(mCurrentForm);

  return NS_OK; 
}

NS_IMETHODIMP
HTMLContentSink::OpenHead(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenHead", aNode);

  mNodeStack[mStackPos] = (eHTMLTags)aNode.GetNodeType();
  mContainerStack[mStackPos] = mHead;
  mStackPos++;
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseHead(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseHead", aNode);

  NS_ASSERTION(mStackPos > 0, "bad bad");
  mNodeStack[--mStackPos] = eHTMLTag_unknown;
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::PushMark()
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::SetTitle(const nsString& aValue)
{
  FlushText();

  if (nsnull == mTitle) {
    mTitle = new nsString(aValue);
  }
  else {
    *mTitle = aValue;
  }
  mTitle->CompressWhitespace(PR_TRUE, PR_TRUE);
  ((nsHTMLDocument*)mDocument)->SetTitle(*mTitle);

  nsIAtom* atom = NS_NewAtom("TITLE");
  nsIHTMLContent* it = nsnull;
  nsresult rv = NS_NewHTMLTitle(&it, atom, aValue);
  if (NS_OK == rv) {
    mHead->AppendChild(it, PR_FALSE);
  }
  NS_RELEASE(atom);
  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenBody(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenBody", aNode);

  mNodeStack[mStackPos] = (eHTMLTags)aNode.GetNodeType();

  // Make body container
  NS_IF_RELEASE(mBody);
  nsIAtom* atom = NS_NewAtom("BODY");
  if (nsnull == atom) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = NS_NewBodyPart(&mBody, atom);
  NS_RELEASE(atom);
  if (NS_OK != rv) {
    return rv;
  }

  mContainerStack[mStackPos] = mBody;
  mStackPos++;

  // Add attributes to the body content object, but only if it's really a body
  // tag that is triggering the OpenBody.
  if (eHTMLTag_body == aNode.GetNodeType()) {
    AddAttributes(aNode, mBody);
    // XXX If the body already existed and has been reflowed somewhat
    // then we need to trigger a style change
  }

  if (!mLayoutStarted) {
    // XXX This has to be done now that the body is in because we
    // don't know how to handle a content-appended reflow if the
    // root has no children
    SINK_TRACE(SINK_TRACE_REFLOW,
               ("HTMLContentSink::OpenBody: start layout"));

    // This is done here instead of earlier because we need the
    // attributes from the real body tag before we initiate reflow.
    mRoot->AppendChild(mBody, PR_FALSE);
    StartLayout();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseBody(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseBody", aNode);

  NS_ASSERTION(mStackPos > 0, "bad bad");
  mNodeStack[--mStackPos] = eHTMLTag_unknown;

  // Reflow any lingering content

  return NS_OK;
}

static NS_DEFINE_IID(kIFormManagerIID, NS_IFORMMANAGER_IID);

NS_IMETHODIMP
HTMLContentSink::OpenForm(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenForm", aNode);

  // Close out previous form if it's there
  if (nsnull != mCurrentForm) {
	  NS_RELEASE(mCurrentForm);
    mCurrentForm = nsnull;
  }

  nsresult rv;
  nsAutoString classID;
  if (FindAttribute(aNode, "classid", classID)) {
    // Translate classid string into an nsID
    char cbuf[50];
    classID.ToCString(cbuf, sizeof(cbuf));
    nsID clid;
    if (clid.Parse(cbuf)) {
      // Create a form manager using the repository
      nsIFactory* fac;
      rv = NSRepository::FindFactory(clid, &fac);
      if (NS_OK == rv) {
        rv = fac->CreateInstance(nsnull, kIFormManagerIID,
                                 (void**)&mCurrentForm);
#ifdef NS_DEBUG
        if (NS_OK != rv) {
          printf("OpenForm: can't create form manager instance: %d\n", rv);
        }
#endif
        NS_RELEASE(fac);
      }
      else {
#ifdef NS_DEBUG
        printf("OpenForm: can't find '%s' in the repository\n", cbuf);
#endif
      }
    }
    else {
#ifdef NS_DEBUG
      printf("OpenForm: classID is invalid: '%s'\n", cbuf);
#endif
    }
  }

  if (nsnull == mCurrentForm) {
    // Create new form
    nsAutoString tmp("FORM");
    nsIAtom* atom = NS_NewAtom(tmp);
    rv = NS_NewHTMLForm(&mCurrentForm, atom);
    NS_RELEASE(atom);
  }

  if (NS_OK == rv) {
    // Add tag attributes to the form
    nsAutoString k, v;
    PRInt32 ac = aNode.GetAttributeCount();
    for (PRInt32 i = 0; i < ac; i++) {
      // Get upper-cased key
      const nsString& key = aNode.GetKeyAt(i);
      k.SetLength(0);
      k.Append(key);
      k.ToUpperCase();

      // Get value and remove mandatory quotes
      GetAttributeValueAt(aNode, i, v);
      mCurrentForm->SetAttribute(k, v);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseForm(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseForm", aNode);

  if (nsnull != mCurrentForm) {
	  NS_RELEASE(mCurrentForm);
    mCurrentForm = nsnull;
  }
  return NS_OK;
}

// XXX this is a copy of OpenBody, consolidate!
NS_IMETHODIMP
HTMLContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenFrameset", aNode);

  mNodeStack[mStackPos] = (eHTMLTags)aNode.GetNodeType();

  // Make frameset container
  NS_IF_RELEASE(mFrameset);
  nsIAtom* atom = NS_NewAtom("FRAMESET");
  if (nsnull == atom) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = NS_NewHTMLFrameset(&mFrameset, atom, nsnull);
  NS_RELEASE(atom);
  if (NS_OK != rv) {
    return rv;
  }

  mContainerStack[mStackPos] = mFrameset;
  mStackPos++;

  // Add attributes to the frameset content object
  AddAttributes(aNode, mFrameset);
  // XXX If the frameset already existed and has been reflowed somewhat
  // then we need to trigger a style change
  mRoot->AppendChild(mFrameset, PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::CloseFrameset(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseFrameset", aNode);

  NS_ASSERTION(mStackPos > 0, "bad bad");
  mNodeStack[--mStackPos] = eHTMLTag_unknown;

  // Reflow any lingering content
  if (!mLayoutStarted) {
    StartLayout();
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLContentSink::OpenMap(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenMap", aNode);

  // Close out previous form if it's there
  if (nsnull != mCurrentMap) {
	  NS_RELEASE(mCurrentMap);
    mCurrentMap = nsnull;
  }

  nsAutoString tmp("MAP");
  nsIAtom* atom = NS_NewAtom(tmp);
  nsresult rv = NS_NewImageMap(&mCurrentMap, atom);
  NS_RELEASE(atom);
  if (NS_OK == rv) {
    // Look for name attribute and set the map name
    nsAutoString name;
    if (FindAttribute(aNode, "name", name)) {
      // XXX leading, trailing, interior non=-space ws is removed
      name.StripWhitespace();
      mCurrentMap->SetName(name);
    }
    // Add the map to the document
    ((nsHTMLDocument*)mDocument)->AddImageMap(mCurrentMap);
  }
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseMap(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseMap", aNode);

  if (nsnull != mCurrentMap) {
	  NS_RELEASE(mCurrentMap);
    mCurrentMap = nsnull;
  }
  return NS_OK;
}


NS_IMETHODIMP
HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::OpenContainer", aNode);

  nsHTMLTag nodeType = nsHTMLTag(aNode.GetNodeType());/* XXX bad parser api */
  nsAutoString tmp;
  if (eHTMLTag_userdefined == nodeType) {
    tmp.Append(aNode.GetText());
    tmp.ToUpperCase();
  }
  else {
    tmp.Append(NS_EnumToTag(nodeType));
  }
  nsIAtom* atom = NS_NewAtom(tmp);

  eHTMLTags parentType;
  nsIHTMLContent* parent = GetCurrentContainer(&parentType);
  switch (parentType) {
  case eHTMLTag_select:
    break;
  case eHTMLTag_option:
    break;
  }

  nsresult rv;
  nsIHTMLContent* container = nsnull;
  switch (nodeType) {
  case eHTMLTag_map:
    NS_NOTREACHED("bad parser: map != container");
    break;

  case eHTMLTag_applet:
    rv = NS_NewHTMLApplet(&container, atom);
    break;

  case eHTMLTag_object:
    rv = NS_NewHTMLObject(&container, atom);
    break;

  case eHTMLTag_table:
    rv = NS_NewTablePart(&container, atom);
    mInMonolithicContainer++;
    break;

  case eHTMLTag_caption:
    rv = NS_NewTableCaptionPart(&container, atom);
    break;

  case eHTMLTag_tr:
    rv = NS_NewTableRowPart(&container, atom);
    break;

  case eHTMLTag_tbody:
  case eHTMLTag_thead:
  case eHTMLTag_tfoot:
    rv = NS_NewTableRowGroupPart(&container, atom);
    break;

  case eHTMLTag_colgroup:
    rv = NS_NewTableColGroupPart(&container, atom);
    break;

  case eHTMLTag_col:
    rv = NS_NewTableColPart(&container, atom);
    break;

  case eHTMLTag_td:
  case eHTMLTag_th:
    rv = NS_NewTableCellPart(&container, atom);
    break;

  case eHTMLTag_select:
    rv = ProcessOpenSELECTTag(&container, aNode);
    break;

  case eHTMLTag_option:
    rv = ProcessOpenOPTIONTag(&container, aNode);
    break;

  case eHTMLTag_iframe:
    rv = ProcessIFRAMETag(&container, aNode);
    break;

  case eHTMLTag_frameset:
    if (!mFrameset) {
      rv = OpenFrameset(aNode);  // top level frameset
    } else {
      rv = ProcessFRAMESETTag(&container, aNode);
    }
    break;

  default:
    rv = NS_NewHTMLContainer(&container, atom);
    break;
  }

  // XXX for now assume that if it's a container, it's a simple container
  mNodeStack[mStackPos] = (eHTMLTags) aNode.GetNodeType();
  mContainerStack[mStackPos] = container;

  if (nsnull != container) {
    container->SetDocument(mDocument);
    rv = AddAttributes(aNode, container);
    mStackPos++;
  }

  NS_RELEASE(atom);
  return rv;
}

NS_IMETHODIMP
HTMLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  FlushText();

  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::CloseContainer", aNode);

  switch (aNode.GetNodeType()) {
  case eHTMLTag_map:
    NS_NOTREACHED("bad parser: map's in CloseContainer");
    NS_IF_RELEASE(mCurrentMap);
    return NS_OK;
  }

  // XXX we could assert things about the top tag name
  if (0 == mStackPos) {
    // Can't pop empty stack
    return NS_OK;
  }

  --mStackPos;
  nsIHTMLContent* container = mContainerStack[mStackPos];
  mNodeStack[mStackPos] = eHTMLTag_unknown;
  mContainerStack[mStackPos] = nsnull;


  nsIHTMLContent* parent = nsnull;

  switch (aNode.GetNodeType()) {
  case eHTMLTag_option:
    ProcessCloseOPTIONTag(aNode);
    break;

  case eHTMLTag_select:
    ProcessCloseSELECTTag(aNode); // add fall through
    break;

  case eHTMLTag_table:
    mInMonolithicContainer--;
    break;
  }

  if (nsnull != container) {
    // Now that this container is complete, append it to it's parent
    eHTMLTags parentType;
    parent = GetCurrentContainer(&parentType);
    container->Compact();
    // don't append the top level frameset to its parent, this was done in OpenFrameset
    // XXX this is necessary because the parser is calling OpenContainer, CloseContainer
    // on framesets. It should be calling OpenFrameset.
    if (container == mFrameset) {
      return CloseFrameset(aNode);
    }

    if (nsnull != parent) {
      PRBool allowReflow = parent == GetBodyOrFrameset();
#ifdef NS_DEBUG
      if (allowReflow) {
        SINK_TRACE(SINK_TRACE_REFLOW,
                   ("HTMLContentSink::CloseContainer: reflow after append"));
      }
#endif
      
      AppendToCorrectParent(parentType, parent,
                            (nsHTMLTag) aNode.GetNodeType(), container,
                            allowReflow);
#ifdef NS_DEBUG
      if (allowReflow && (((PRInt32)gSinkLogModuleInfo->level) > 127)) {
        PRInt32 i, ns = mDocument->GetNumberOfShells();
        for (i = 0; i < ns; i++) {
          nsIPresShell* shell = mDocument->GetShellAt(i);
          if (nsnull != shell) {
            nsIViewManager* vm = shell->GetViewManager();
            if(vm) {
              nsIView* rootView = vm->GetRootView();
              nsRect rect;
              rootView->GetBounds(rect);
              rect.x = 0;
              rect.y = 0;
              vm->Refresh(rootView, nsnull, &rect, NS_VMREFRESH_IMMEDIATE);
              NS_RELEASE(rootView);
            }
            NS_RELEASE(vm);
            NS_RELEASE(shell);
          }
        }
        printf("tick...\n");
        PR_Sleep(PR_SecondsToInterval(3));
      }
#endif
#if XXX
      if (parent == GetBodyOrFrameset()) {
        // We just closed a child of the body off. Trigger a
        // content-appended reflow if enough time has elapsed
        PRTime now = PR_Now();
        /* XXX this expression doesn't compile on the Mac
           kipp said it had to do with a type issue.
           if (now - mLastUpdateTime >= mUpdateDelta) {
           mLastUpdateTime = now;
           mUpdateDelta += mUpdateDelta;
           ReflowNewContent();
           }*/
      }
#endif
    }
    NS_RELEASE(container);
  }

  return NS_OK;
}

/**
 * This method gets called when the parser begins the process
 * of building the content model via the content sink.
 *
 * @update 5/7/98 gess
 */     
NS_IMETHODIMP
HTMLContentSink::WillBuildModel(void)
{
  mDocument->BeginLoad();
#if XXX
  // XXX temporary
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (nsnull != shell) {
      shell->BeginObservingDocument();
      NS_RELEASE(shell);
    }
  }

  SINK_TRACE(SINK_TRACE_REFLOW,
             ("HTMLContentSink::WillBuildModel: start layout"));
  StartLayout();
#endif
  return NS_OK;
}


/**
 * This method gets called when the parser concludes the process
 * of building the content model via the content sink.
 *
 * @param  aQualityLevel describes how well formed the doc was.
 *         0=GOOD; 1=FAIR; 2=POOR;
 * @update 6/21/98 gess
 */     
NS_IMETHODIMP
HTMLContentSink::DidBuildModel(PRInt32 aQualityLevel)
{
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (nsnull != shell) {
      nsIViewManager* vm = shell->GetViewManager();
      if(vm) {
        vm->SetQuality(nsContentQuality(aQualityLevel));
      }
      NS_RELEASE(vm);
      NS_RELEASE(shell);
    }
  }

  SINK_TRACE(SINK_TRACE_REFLOW,
             ("HTMLContentSink::DidBuildModel: layout new content"));
  ReflowNewContent();
  mDocument->EndLoad();

  return NS_OK;
}

/**
 * This method gets called when the parser gets i/o blocked,
 * and wants to notify the sink that it may be a while before
 * more data is available.
 *
 * @update 5/7/98 gess
 */     
NS_IMETHODIMP
HTMLContentSink::WillInterrupt(void)
{
  return NS_OK;
}

/**
 * This method gets called when the parser i/o gets unblocked,
 * and we're about to start dumping content again to the sink.
 *
 * @update 5/7/98 gess
 */     
NS_IMETHODIMP
HTMLContentSink::WillResume(void)
{
  return NS_OK;
}

//----------------------------------------------------------------------

void HTMLContentSink::StartLayout()
{
  if (!mLayoutStarted) {
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsIPresShell* shell = mDocument->GetShellAt(i);
      if (nsnull != shell) {
        // Make shell an observer for next time
        shell->BeginObservingDocument();

        // Resize-reflow this time
        nsIPresContext* cx = shell->GetPresContext();
        nsRect r;
        cx->GetVisibleArea(r);
        shell->ResizeReflow(r.width, r.height);
        NS_RELEASE(cx);
        NS_RELEASE(shell);
      }
    }
    mLayoutStarted = PR_TRUE;
  }
}

void HTMLContentSink::ReflowNewContent()
{
#if 0
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (nsnull != shell) {
      nsIPresContext* cx = shell->GetPresContext();
      nsRect r;
      cx->GetVisibleArea(r);
      shell->ResizeReflow(r.width, r.height);
      NS_RELEASE(cx);
      NS_RELEASE(shell);
    }
  }
#else
  // Trigger reflows in each of the presentation shells
  mDocument->ContentAppended(mBody);
#endif
}

nsIHTMLContent* HTMLContentSink::GetCurrentContainer(eHTMLTags* aType)
{
  nsIHTMLContent* parent;
  if (mStackPos <= 2) {         // assume HTML and BODY/FRAMESET are on the stack
    if (mBody) {
      parent = mBody;
      *aType = eHTMLTag_body;
    } else {
      parent = mFrameset;
      *aType = eHTMLTag_frameset;
    }
  } else {
    parent = mContainerStack[mStackPos - 1];
    *aType = mNodeStack[mStackPos - 1];
  }
  return parent;
}

//----------------------------------------------------------------------

// Leaf tag handling code

NS_IMETHODIMP HTMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  SINK_TRACE_NODE(SINK_TRACE_CALLS,
                  "HTMLContentSink::AddLeaf", aNode);

  // Check for nodes that require special handling
  switch (aNode.GetNodeType()) {
  case eHTMLTag_style:
    FlushText();
    ProcessSTYLETag(aNode);
    return NS_OK;

  case eHTMLTag_script:
    // XXX SCRIPT tag evaluation is currently turned off till we
    // get more scripts working.
    FlushText();
    ProcessSCRIPTTag(aNode);
    return NS_OK;

  case eHTMLTag_area:
    FlushText();
    ProcessAREATag(aNode);
    return NS_OK;

  case eHTMLTag_meta:
    // Add meta objects to the head object
    FlushText();
    ProcessMETATag(aNode);
    return NS_OK;
  }

  eHTMLTags parentType;
  nsIHTMLContent* parent = GetCurrentContainer(&parentType);

  switch (parentType) {
  /*case eHTMLTag_table:
  case eHTMLTag_tr:
  case eHTMLTag_tbody:
  case eHTMLTag_thead:
  case eHTMLTag_tfoot:
    // XXX Discard leaf content (those annoying \n's really) in
    // table's or table rows
    return PR_TRUE;
  */
  case eHTMLTag_option:
    FlushText();
    ProcessOPTIONTagContent(aNode);
    return NS_OK;

  case eHTMLTag_select:
    // Discard content in a select that's not an option
    if (eHTMLTag_option != aNode.GetNodeType()) {
      return NS_OK;
    }
    break;
  }

  nsresult rv = NS_OK;
  nsIHTMLContent* leaf = nsnull;
  switch (aNode.GetTokenType()) {
  case eToken_start:
    switch (aNode.GetNodeType()) {
    case eHTMLTag_br:
      FlushText();
      rv = ProcessBRTag(&leaf, aNode);
      break;
    case eHTMLTag_frame:
      FlushText();
      rv = ProcessFrameTag(&leaf, aNode);
      break;
    case eHTMLTag_hr:
      FlushText();
      rv = ProcessHRTag(&leaf, aNode);
      break;
    case eHTMLTag_input:
      FlushText();
      rv = ProcessINPUTTag(&leaf, aNode);
      break;
    case eHTMLTag_img:
      FlushText();
      rv = ProcessIMGTag(&leaf, aNode);
      break;
    case eHTMLTag_spacer:
      FlushText();
      rv = ProcessSPACERTag(&leaf, aNode);
      break;
    case eHTMLTag_textarea:
      FlushText();
      ProcessTEXTAREATag(&leaf, aNode);
      break;
    case eHTMLTag_embed:
      rv = ProcessEMBEDTag(&leaf, aNode);
      break;
    }
    break;

  case eToken_text:
  case eToken_whitespace:
  case eToken_newline:
    rv = AddText(aNode.GetText(), &leaf);
    break;

  case eToken_entity:
    {
      nsAutoString tmp;
      PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
      if (unicode < 0) {
        rv = AddText(aNode.GetText(), &leaf);
      }
      else {
        rv = AddText(tmp, &leaf);
      }
    }
    break;

  case eToken_skippedcontent:
    break;
  }

  if (NS_OK == rv) {
    if (nsnull != leaf) {
      if (nsnull != parent) {
        AppendToCorrectParent(parentType, parent,
                              (nsHTMLTag) aNode.GetNodeType(), leaf,
                              PR_FALSE);
      } else {
        // XXX drop stuff on the floor that doesn't have a container!
        // Bad parser!
      }
    }
  }
  NS_IF_RELEASE(leaf);

  return rv;
}

// Special handling code to push unexpected table content out of the
// table and into the table's parent, just before the table. Because
// the table is a container, it will not have been added to it's
// parent yet so we can just append the inappropriate content.
void
HTMLContentSink::AppendToCorrectParent(nsHTMLTag aParentTag,
                                       nsIHTMLContent* aParent,
                                       nsHTMLTag aChildTag,
                                       nsIHTMLContent* aChild,
                                       PRBool aAllowReflow)
{
  nsIHTMLContent* realParent = aParent;

  // These are the tags that are allowed in a table
  static char tableTagSet[] = {
    eHTMLTag_tbody, eHTMLTag_thead, eHTMLTag_tfoot,
    eHTMLTag_tr, eHTMLTag_col, eHTMLTag_colgroup,
    eHTMLTag_caption,/* XXX ok? */
    0,
  };

  // These are the tags that are allowed in a tbody/thead/tfoot
  static char tbodyTagSet[] = {
    eHTMLTag_tr,
    eHTMLTag_caption,/* XXX ok? */
    0,
  };

  // These are the tags that are allowed in a colgroup
  static char colgroupTagSet[] = {
    eHTMLTag_col,
    0,
  };

  // These are the tags that are allowed in a tr
  static char trTagSet[] = {
    eHTMLTag_td, eHTMLTag_th,
    0,
  };

  switch (aParentTag) {
  case eHTMLTag_table:
    if (0 == strchr(tableTagSet, aChildTag)) {
      realParent = GetTableParent();
    }
    break;

  case eHTMLTag_tbody:
  case eHTMLTag_thead:
  case eHTMLTag_tfoot:
    if (0 == strchr(tbodyTagSet, aChildTag)) {
      realParent = GetTableParent();
    }
    break;

  case eHTMLTag_col:
    realParent = GetTableParent();
    break;

  case eHTMLTag_colgroup:
    if (0 == strchr(colgroupTagSet, aChildTag)) {
      realParent = GetTableParent();
    }
    break;

  case eHTMLTag_tr:
    if (0 == strchr(trTagSet, aChildTag)) {
      realParent = GetTableParent();
    }
    break;

  default:
    break;
  }

  realParent->AppendChild(aChild, aAllowReflow);
}

// Find the parent of the currently open table
nsIHTMLContent*
HTMLContentSink::GetTableParent()
{
  PRInt32 sp = mStackPos - 1;
  while (sp >= 0) {
    switch (mNodeStack[sp]) {
    case eHTMLTag_table:
    case eHTMLTag_tr:
    case eHTMLTag_tbody:
    case eHTMLTag_thead:
    case eHTMLTag_tfoot:
    case eHTMLTag_col:
    case eHTMLTag_colgroup:
      break;
    default:
      return mContainerStack[sp];
    }
    sp--;
  }
  return GetBodyOrFrameset();
}

nsresult
HTMLContentSink::AddText(const nsString& aText, nsIHTMLContent** aContent)
{
  nsresult rv;
  if (nsnull != mCurrentText) {
    mCurrentText->Append((nsString&)aText);/* XXX fix dom text api!!! */
    *aContent = nsnull;
    rv = NS_OK;
  }
  else {
    static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
    rv = NS_NewHTMLText(aContent, aText.GetUnicode(), aText.Length());
    if (NS_OK == rv) {
      (*aContent)->QueryInterface(kIDOMTextIID, (void**) &mCurrentText);
    }
  }
  return rv;
}

void
HTMLContentSink::FlushText()
{
  if (nsnull != mCurrentText) {
    NS_RELEASE(mCurrentText);
  }
}

void HTMLContentSink::GetAttributeValueAt(const nsIParserNode& aNode,
                                          PRInt32 aIndex,
                                          nsString& aResult)
{
  // Copy value
  const nsString& value = aNode.GetValueAt(aIndex);
  aResult.Truncate();
  aResult.Append(value);

  // strip quotes if present
  PRUnichar first = aResult.First();
  if ((first == '"') || (first == '\'')) {
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
}

PRBool HTMLContentSink::FindAttribute(const nsIParserNode& aNode,
                                      const nsString& aKeyName,
                                      nsString& aResult)
{
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase(aKeyName)) {
      // Get value and remove mandatory quotes
      GetAttributeValueAt(aNode, i, aResult);
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsresult HTMLContentSink::AddAttributes(const nsIParserNode& aNode,
                                        nsIHTMLContent* aInstancePtrResult)
{
  nsIContent* content = (nsIContent*) aInstancePtrResult;

  // Add tag attributes to the content attributes
  nsAutoString k, v;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsString& key = aNode.GetKeyAt(i);
    k.Truncate();
    k.Append(key);
    k.ToUpperCase();
    
    // Get value and remove mandatory quotes
    GetAttributeValueAt(aNode, i, v);

    content->SetAttribute(k, v);
  }

  return NS_OK;
}

nsresult HTMLContentSink::ProcessAREATag(const nsIParserNode& aNode)
{
  if (nsnull != mCurrentMap) {
    nsAutoString shape, coords, href, target(mBaseTarget), alt;
    PRInt32 ac = aNode.GetAttributeCount();
    PRBool suppress = PR_FALSE;
    for (PRInt32 i = 0; i < ac; i++) {
      // Get upper-cased key
      const nsString& key = aNode.GetKeyAt(i);
      if (key.EqualsIgnoreCase("shape")) {
        GetAttributeValueAt(aNode, i, shape);
      }
      else if (key.EqualsIgnoreCase("coords")) {
        GetAttributeValueAt(aNode, i, coords);
      }
      else if (key.EqualsIgnoreCase("href")) {
        GetAttributeValueAt(aNode, i, href);
        href.StripWhitespace();
      }
      else if (key.EqualsIgnoreCase("target")) {
        GetAttributeValueAt(aNode, i, target);
      }
      else if (key.EqualsIgnoreCase("alt")) {
        GetAttributeValueAt(aNode, i, alt);
      }
      else if (key.EqualsIgnoreCase("suppress")) {
        suppress = PR_TRUE;
      }
    }
    mCurrentMap->AddArea(mBaseHREF, shape, coords, href, target, alt,
                         suppress);
  }
  return NS_OK;
}

nsresult HTMLContentSink::ProcessBASETag(const nsIParserNode& aNode)
{
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("href")) {
      const nsString& href = aNode.GetValueAt(i);
      if (href.Length() > 0) {
        mBaseHREF = href;
      }
    } else if (key.EqualsIgnoreCase("target")) {

      const nsString& target= aNode.GetValueAt(i);
      if (target.Length() > 0) {
        mBaseTarget = target;
      }
    }
  }
  return NS_OK;
}

nsresult
HTMLContentSink::ProcessMETATag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  if (nsnull != mHead) {
    nsAutoString tmp("META");
    nsIAtom* atom = NS_NewAtom(tmp);

    nsIHTMLContent* it = nsnull;
    rv = NS_NewHTMLMeta(&it, atom) ;
    if (NS_OK == rv) {
      rv = AddAttributes(aNode, it);
      mHead->AppendChild(it, PR_FALSE);
    }
    NS_RELEASE(atom);
  }
  return rv;
}

nsresult HTMLContentSink::ProcessBRTag(nsIHTMLContent** aInstancePtrResult,
                                       const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  nsAutoString tmp("BR");
  nsIAtom* atom = NS_NewAtom(tmp);
  rv = NS_NewHTMLBreak(aInstancePtrResult, atom);
  if (NS_OK == rv) {
    rv = AddAttributes(aNode, *aInstancePtrResult);
  }
  NS_RELEASE(atom);
  return rv;
}
nsresult HTMLContentSink::ProcessEMBEDTag(nsIHTMLContent** aInstancePtrResult,
                                          const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  nsAutoString tmp("EMBED");
  nsIAtom* atom = NS_NewAtom(tmp);
  rv = NS_NewHTMLEmbed(aInstancePtrResult, atom);
  if (NS_OK == rv) {
    rv = AddAttributes(aNode, *aInstancePtrResult);
  }
  NS_RELEASE(atom);
  return rv;
}

nsresult HTMLContentSink::ProcessHRTag(nsIHTMLContent** aInstancePtrResult,
                                       const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  nsAutoString tmp("HR");
  nsIAtom* atom = NS_NewAtom(tmp);
  rv = NS_NewHRulePart(aInstancePtrResult, atom);
  if (NS_OK == rv) {
    rv = AddAttributes(aNode, *aInstancePtrResult);
  }
  NS_RELEASE(atom);
  return rv;
}

nsresult HTMLContentSink::ProcessIMGTag(nsIHTMLContent** aInstancePtrResult,
                                        const nsIParserNode& aNode)
{
  nsAutoString tmp("IMG");
  nsIAtom* atom = NS_NewAtom(tmp);
  nsresult rv = NS_NewHTMLImage(aInstancePtrResult, atom);
  if (NS_OK == rv) {
    rv = AddAttributes(aNode, *aInstancePtrResult);
    // XXX get base url to image
  }
  NS_RELEASE(atom);
  return rv;
}

nsresult HTMLContentSink::ProcessSPACERTag(nsIHTMLContent** aInstancePtrResult,
                                           const nsIParserNode& aNode)
{
  nsAutoString tmp("SPACER");
  nsIAtom* atom = NS_NewAtom(tmp);
  nsresult rv = NS_NewHTMLSpacer(aInstancePtrResult, atom);
  if (NS_OK == rv) {
    rv = AddAttributes(aNode, *aInstancePtrResult);
  }
  NS_RELEASE(atom);
  return rv;
}

#define SCRIPT_BUF_SIZE 1024

nsresult HTMLContentSink::ProcessSCRIPTTag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  PRInt32 i, ac = aNode.GetAttributeCount();

  // Look for SRC attribute
  nsString* src = nsnull;
  for (i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("src")) {
      src = new nsString(aNode.GetValueAt(i));
    }
  }

  char *script = nsnull;
  PRInt32 len = 0;

  // If there is a SRC attribute, (for now) read from the
  // stream synchronously and hold the data in a string.
  if (nsnull != src) {
    // Use the SRC attribute value to open a blocking stream
    nsIURL* url = nsnull;
    char* spec = src->ToNewCString();
    rv = NS_NewURL(&url, nsnull, spec);
    delete spec;
    delete src;
    if (NS_OK != rv) {
      return rv;
    }
    PRInt32 ec;
    nsIInputStream* iin = url->Open(&ec);
    if (nsnull == iin) {
      NS_RELEASE(url);
      return (nsresult) ec;/* XXX fix url->Open */
    }

    // Drain the stream by reading from it a chunk at a time
    nsString data;
    PRInt32 err, nb;
    do {
      char buf[SCRIPT_BUF_SIZE];
      
      nb = iin->Read(&err, buf, 0, SCRIPT_BUF_SIZE);
      if (0 == err) {
        data.Append((const char *)buf, nb);
      }
    } while (err == 0);

    if (NS_INPUTSTREAM_EOF == err) {
      script = data.ToNewCString();
      len = data.Length();
    }
    else {
      rv = NS_ERROR_FAILURE;
    }

    NS_RELEASE(iin);
    NS_RELEASE(url);
  }
  else {
    // Otherwise, get the text content of the script tag
    const nsString& content = aNode.GetSkippedContent();
    script = content.ToNewCString();
    len = content.Length();
  }

  if (nsnull != script) {
    nsIScriptContextOwner *owner;
    nsIScriptContext *context;
    owner = mDocument->GetScriptContextOwner();
    if (nsnull != owner) {
    
      rv = owner->GetScriptContext(&context);
      if (rv != NS_OK) {
        NS_RELEASE(owner);
        return rv;
      }
      
      jsval val;
      PRBool result = context->EvaluateString(script, len, &val);
      
      if (PR_FALSE == result) {
        rv = NS_ERROR_FAILURE;
      }

      NS_RELEASE(context);
      NS_RELEASE(owner);
    }
    delete script;
  }

  return rv;
}

// 3 ways to load a style sheet: inline, style src=, link tag
  // XXX What does nav do if we have SRC= and some style data inline?
nsresult HTMLContentSink::ProcessSTYLETag(const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  PRInt32 i, ac = aNode.GetAttributeCount();

  nsString* src = nsnull;
  for (i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("src")) {
      src = new nsString(aNode.GetValueAt(i));
    }
  }

  // The skipped content contains the inline style data
  const nsString& content = aNode.GetSkippedContent();

  nsIURL* url = nsnull;
  nsIUnicharInputStream* uin = nsnull;
  if (nsnull == src) {
    // Create a string to hold the data and wrap it up in a unicode
    // input stream.
    rv = NS_NewStringUnicharInputStream(&uin, new nsString(content));
    if (NS_OK != rv) {
      return rv;
    }

    // Use the document's url since the style data came from there
    url = mDocumentURL;
    NS_IF_ADDREF(url);
  } else {
    // src with immediate style data doesn't add up
    // XXX what does nav do?
    char* spec = src->ToNewCString();
    rv = NS_NewURL(&url, nsnull, spec);
    delete spec;
    delete src;
    if (NS_OK != rv) {
      return rv;
    }
    PRInt32 ec;
    nsIInputStream* iin = url->Open(&ec);
    if (nsnull == iin) {
      NS_RELEASE(url);
      return (nsresult) ec;/* XXX fix url->Open */
    }
    rv = NS_NewConverterStream(&uin, nsnull, iin);
    NS_RELEASE(iin);
    if (NS_OK != rv) {
      NS_RELEASE(url);
      return rv;
    }
  }

  // Now that we have a url and a unicode input stream, parse the
  // style sheet.
  rv = LoadStyleSheet(url, uin);
  NS_RELEASE(uin);
  NS_RELEASE(url);

  return rv;
}

nsresult HTMLContentSink::LoadStyleSheet(nsIURL* aURL,
                                         nsIUnicharInputStream* aUIN)
{
  /* XXX use repository */
  nsICSSParser* parser;
  nsresult rv = NS_NewCSSParser(&parser);
  if (NS_OK == rv) {
    if (nsnull != mStyleSheet) {
      parser->SetStyleSheet(mStyleSheet);
      // XXX we do probably need to trigger a style change reflow
      // when we are finished if this is adding data to the same sheet
    }
    nsIStyleSheet* sheet = nsnull;
    // XXX note: we are ignoring rv until the error code stuff in the
    // input routines is converted to use nsresult's
    parser->Parse(aUIN, mDocumentURL, sheet);
    if (nsnull != sheet) {
      if (nsnull == mStyleSheet) {
        // Add in the sheet the first time; if we update the sheet
        // with new data (mutliple style tags in the same document)
        // then the sheet will be updated by the css parser and
        // therefore we don't need to add it to the document)
        mDocument->AddStyleSheet(sheet);
        mStyleSheet = sheet;
      }
      rv = NS_OK;
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;/* XXX */
    }
    NS_RELEASE(parser);
  }
  return rv;
}


nsresult HTMLContentSink::ProcessINPUTTag(nsIHTMLContent** aInstancePtrResult,
                                          const nsIParserNode& aNode)
{
  nsAutoString tmp("INPUT");
  nsIAtom* atom = NS_NewAtom(tmp);

  nsresult rv = NS_ERROR_NOT_INITIALIZED;

  // Find type attribute and then create the appropriate form element
  nsAutoString val;
  if (FindAttribute(aNode, "type", val)) {
    if (val.EqualsIgnoreCase("submit")) {
      rv = NS_NewHTMLInputSubmit(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("reset")) {
      rv = NS_NewHTMLInputReset(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("button")) {
      rv = NS_NewHTMLInputButton(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("checkbox")) {
      rv = NS_NewHTMLInputCheckbox(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("file")) {
      rv = NS_NewHTMLInputFile(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("hidden")) {
      rv = NS_NewHTMLInputHidden(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("image")) {
      rv = NS_NewHTMLInputImage(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("password")) {
      rv = NS_NewHTMLInputPassword(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("radio")) {
      rv = NS_NewHTMLInputRadio(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("text")) {
      rv = NS_NewHTMLInputText(aInstancePtrResult, atom, mCurrentForm);
    }
    else if (val.EqualsIgnoreCase("frameset1")) {  // TEMP hack XXX
      //      rv = NS_NewHTMLFrameset(aInstancePtrResult, atom, mWebWidget, 1);
    }
    else if (val.EqualsIgnoreCase("frameset2")) {  // TEMP hack XXX
      // rv = NS_NewHTMLFrameset(aInstancePtrResult, atom, mWebWidget, 2);
    }
    else if (val.EqualsIgnoreCase("frameset3")) {  // TEMP hack XXX
      // rv = NS_NewHTMLFrameset(aInstancePtrResult, atom, mWebWidget, 3);
    }
    else {
      rv = NS_NewHTMLInputSubmit(aInstancePtrResult, atom, mCurrentForm);
    }
  }

  if (NS_ERROR_NOT_INITIALIZED == rv) {
    // Create textfield when no type is specified
    rv = NS_NewHTMLInputText(aInstancePtrResult, atom, mCurrentForm);
  }

  if ((NS_OK == rv) && (nsnull != *aInstancePtrResult)) {
    // Add remaining attributes from the tag
    rv = AddAttributes(aNode, *aInstancePtrResult);
  }

  NS_RELEASE(atom);
  return rv;
}

nsresult HTMLContentSink::ProcessFrameTag(nsIHTMLContent** aInstancePtrResult,
                                          const nsIParserNode& aNode)
{
  nsAutoString tmp("FRAME");
  nsIAtom* atom = NS_NewAtom(tmp);

  nsresult rv = NS_NewHTMLFrame(aInstancePtrResult, atom, mWebShell);

  NS_RELEASE(atom);
  return rv;
}

nsresult
HTMLContentSink::ProcessTEXTAREATag(nsIHTMLContent** aInstancePtrResult,
                                    const nsIParserNode& aNode)
{
  nsAutoString tmp("TEXTAREA");
  nsIAtom* atom = NS_NewAtom(tmp);

  const nsString& content = aNode.GetSkippedContent();

  nsresult rv = NS_NewHTMLTextArea(aInstancePtrResult, atom, mCurrentForm);
  if ((NS_OK == rv) && (nsnull != *aInstancePtrResult)) {
    // Add remaining attributes from the tag
    rv = AddAttributes(aNode, *aInstancePtrResult);
    if (0 < content.Length()) {
      nsIFormControl* control;
      rv = (*aInstancePtrResult)->QueryInterface(kIFormControlIID, (void **)&control);
      if (NS_OK == rv) {
        control->SetContent(content);
      }
    }
  }

  NS_RELEASE(atom);
  return rv;
}

nsresult
HTMLContentSink::ProcessOpenSELECTTag(nsIHTMLContent** aInstancePtrResult,
                                      const nsIParserNode& aNode)
{
  nsAutoString tmp("SELECT");
  nsIAtom* atom = NS_NewAtom(tmp);

  if (nsnull != mCurrentSelect) {
    NS_RELEASE(mCurrentSelect);
  }
  nsresult rv = NS_NewHTMLSelect(&mCurrentSelect, atom, mCurrentForm);
  if ((NS_OK == rv) && (nsnull != mCurrentSelect)) {
    // Add remaining attributes from the tag
    //rv = AddAttributes(aNode, mCurrentSelect);
    *aInstancePtrResult = mCurrentSelect;
  }

  NS_RELEASE(atom);
  return rv;
}

nsresult
HTMLContentSink::ProcessCloseSELECTTag(const nsIParserNode& aNode)
{
  NS_IF_RELEASE(mCurrentSelect);
  mCurrentSelect = nsnull;
  return NS_OK;
}

nsresult
HTMLContentSink::ProcessOpenOPTIONTag(nsIHTMLContent** aInstancePtrResult,
                                      const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  if (nsnull != mCurrentSelect) {
    if (nsnull != mCurrentOption) {
      NS_RELEASE(mCurrentOption);
    }
    nsAutoString tmp("OPTION");
    nsIAtom* atom = NS_NewAtom(tmp);
    rv = NS_NewHTMLOption(&mCurrentOption, atom);
    if ((NS_OK == rv) && (nsnull != mCurrentSelect)) {
      // Add remaining attributes from the tag
      //rv = AddAttributes(aNode, mCurrentOption);
	    *aInstancePtrResult = mCurrentOption;
    }
    NS_RELEASE(atom);
  }
  return rv;
}

nsresult
HTMLContentSink::ProcessCloseOPTIONTag(const nsIParserNode& aNode)
{
  NS_IF_RELEASE(mCurrentOption);
  mCurrentOption = nsnull;
  return NS_OK;
}

nsresult
HTMLContentSink::ProcessOPTIONTagContent(const nsIParserNode& aNode)
{
  if ((nsnull != mCurrentSelect) && (nsnull != mCurrentOption)) {
    nsIFormControl* control = nsnull;
    mCurrentOption->QueryInterface(kIFormControlIID, (void **)&control);
    if (nsnull != control) {
      // Get current content and append on the new content
      nsAutoString currentText;
      control->GetContent(currentText);

      switch (aNode.GetTokenType()) {
      case eToken_text:
      case eToken_whitespace:
      case eToken_newline:
        currentText.Append(aNode.GetText());
        break;

      case eToken_entity:
        {
          nsAutoString tmp2("");
          PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp2);
          if (unicode < 0) {
            currentText.Append(aNode.GetText());
          } else {
            currentText.Append(tmp2);
          }
        }
        break;
      }
      control->SetContent(currentText);
    }
  }
  return NS_OK;
}

nsresult
HTMLContentSink::ProcessIFRAMETag(nsIHTMLContent** aInstancePtrResult,
                                      const nsIParserNode& aNode)
{
  nsAutoString tmp("IFRAME");
  nsIAtom* atom = NS_NewAtom(tmp);

  nsresult rv = NS_NewHTMLIFrame(aInstancePtrResult, atom, mWebShell);

  NS_RELEASE(atom);
  return rv;
}

nsresult
HTMLContentSink::ProcessFRAMESETTag(nsIHTMLContent** aInstancePtrResult,
                                      const nsIParserNode& aNode)
{
  nsAutoString tmp("FRAMESET");
  nsIAtom* atom = NS_NewAtom(tmp);

  nsresult rv = NS_NewHTMLIFrame(aInstancePtrResult, atom, mWebShell);

  NS_RELEASE(atom);
  return rv;
}

nsresult HTMLContentSink::ProcessWBRTag(nsIHTMLContent** aInstancePtrResult,
                                        const nsIParserNode& aNode)
{
  nsAutoString tmp("WBR");
  nsIAtom* atom = NS_NewAtom(tmp);
  nsresult rv = NS_NewHTMLWordBreak(aInstancePtrResult, atom);
  if (NS_OK == rv) {
    rv = AddAttributes(aNode, *aInstancePtrResult);
  }
  NS_RELEASE(atom);
  return rv;
}


//----------------------------------------------------------------------

nsresult NS_NewHTMLContentSink(nsIHTMLContentSink** aInstancePtrResult,
                               nsIDocument* aDoc,
                               nsIURL* aURL,
                               nsIWebShell* aWebShell)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  HTMLContentSink* it = new HTMLContentSink();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aDoc, aURL, aWebShell);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(kIHTMLContentSinkIID, (void **)aInstancePtrResult);
}

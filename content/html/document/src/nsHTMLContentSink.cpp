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
#include "nsHTMLTokens.h"
#include "nsCRT.h"
#include "prtime.h"
#include "prlog.h"

#include "nsHTMLParts.h"
#include "nsTablePart.h"
#include "nsTableRow.h"
#include "nsTableCell.h"

#include "nsHTMLForms.h"
#include "nsIFormManager.h"
#include "nsIImageMap.h"

// XXX attribute values have entities in them - use the parsers expander!

// XXX Go through a factory for this one
#include "nsICSSParser.h"

#ifdef NS_DEBUG

static PRInt32 gLogLevel = 0;

#define NOISY_SINK_TRACE(_msg,_node)             \
{                                                \
  char buf[200];                                 \
  (_node).GetText().ToCString(buf, sizeof(buf)); \
  if (gLogLevel >= PR_LOG_WARNING) {             \
    PR_LogPrint("%s; [%s]", _msg, buf);          \
  }                                              \
}

#define REALLY_NOISY_SINK_TRACE(_msg,_node)      \
{                                                \
  char buf[200];                                 \
  (_node).GetText().ToCString(buf, sizeof(buf)); \
  if (gLogLevel >= PR_LOG_DEBUG) {               \
    PR_LogPrint("%s; [%s]", _msg, buf);          \
  }                                              \
}

#else /* !NOISY_SINK */

#define NOISY_SINK_TRACE(_a,_node)
#define REALLY_NOISY_SINK_TRACE(_a,_node)

#endif /* NOISY_SINK */

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIHTMLContentSinkIID, NS_IHTMLCONTENTSINK_IID);

class HTMLContentSink : public nsIHTMLContentSink {
public:

  NS_DECL_ISUPPORTS

  HTMLContentSink();
  ~HTMLContentSink();

  void* operator new(size_t size) {
    void* rv = ::operator new(size);
    nsCRT::zero(rv, size);
    return (void*) rv;
  }

  nsresult Init(nsIDocument* aDoc, nsIURL* aURL);
  nsIHTMLContent* GetCurrentContainer(eHTMLTags* aType);

  virtual PRBool SetTitle(const nsString& aValue);

  // Called when Opening or closing the main HTML container
  virtual PRBool OpenHTML(const nsIParserNode& aNode);
  virtual PRBool CloseHTML(const nsIParserNode& aNode);

  // Called when Opening or closing the main HEAD container
  virtual PRBool OpenHead(const nsIParserNode& aNode);
  virtual PRBool CloseHead(const nsIParserNode& aNode);

  // Called when Opening or closing the main BODY container
  virtual PRBool OpenBody(const nsIParserNode& aNode);
  virtual PRBool CloseBody(const nsIParserNode& aNode);

  // Called when Opening or closing FORM containers
  virtual PRBool OpenForm(const nsIParserNode& aNode);
  virtual PRBool CloseForm(const nsIParserNode& aNode);

  // Called when Opening or closing the main FRAMESET container
  virtual PRBool OpenFrameset(const nsIParserNode& aNode);
  virtual PRBool CloseFrameset(const nsIParserNode& aNode);

  // Called when Opening or closing a general container
  // This includes: OL,UL,DIR,SPAN,TABLE,H[1..6],etc.
  // Until proven otherwise, I also plan to toss STYLE,
  // FORMS, FRAME, SCRIPT, etc. here too!
  virtual PRBool OpenContainer(const nsIParserNode& aNode);
  virtual PRBool CloseContainer(const nsIParserNode& aNode);
  virtual PRBool CloseTopmostContainer();

  // Called for text, comments and so on...
  virtual PRBool AddLeaf(const nsIParserNode& aNode);

protected:

  void StartLayout();

  void ReflowNewContent();

  //----------------------------------------------------------------------
  // Leaf tag handler routines that translate a leaf tag into a
  // content object, processing all of the tag attributes.

  nsresult ProcessAREATag(const nsIParserNode& aNode);
  nsresult ProcessBASETag(const nsIParserNode& aNode);
  nsresult ProcessSTYLETag(const nsIParserNode& aNode);

  nsresult ProcessBRTag(nsIHTMLContent** aInstancePtrResult,
                        const nsIParserNode& aNode);
  nsresult ProcessHRTag(nsIHTMLContent** aInstancePtrResult,
                        const nsIParserNode& aNode);
  nsresult ProcessINPUTTag(nsIHTMLContent** aInstancePtrResult,
                           const nsIParserNode& aNode);
  nsresult ProcessIMGTag(nsIHTMLContent** aInstancePtrResult,
                         const nsIParserNode& aNode);
  nsresult ProcessSPACERTag(nsIHTMLContent** aInstancePtrResult,
                            const nsIParserNode& aNode);
  nsresult ProcessWBRTag(nsIHTMLContent** aInstancePtrResult,
                         const nsIParserNode& aNode);

  //----------------------------------------------------------------------

  void GetAttributeValueAt(const nsIParserNode& aNode,
                           PRInt32 aIndex,
                           nsString& aResult);

  nsresult AddAttributes(const nsIParserNode& aNode,
                         nsIHTMLContent* aInstancePtrResult);

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

  nsIHTMLContent* mRoot;
  nsIHTMLContent* mBody;

  PRTime mLastUpdateTime;
  PRTime mUpdateDelta;
  PRBool mLayoutStarted;
};

// Note: operator new zeros our memory
HTMLContentSink::HTMLContentSink()
{
#if 0
  if (nsnull == gSinkLog) {
    gSinkLog = PR_NewLogModule("HTMLContentSink");
  }
#endif

  // Set the first update delta to be 50ms
  LL_I2L(mUpdateDelta, PR_USEC_PER_MSEC * 50);
}

HTMLContentSink::~HTMLContentSink()
{
  NS_IF_RELEASE(mBody);
  NS_IF_RELEASE(mRoot);
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mStyleSheet);
  NS_IF_RELEASE(mCurrentForm);
  NS_IF_RELEASE(mCurrentMap);
  if (nsnull != mTitle) {
    delete mTitle;
  }
}

nsresult HTMLContentSink::Init(nsIDocument* aDoc, nsIURL* aDocURL)
{
  mDocument = aDoc;
  NS_IF_ADDREF(aDoc);

  mDocumentURL = aDocURL;
  NS_IF_ADDREF(aDocURL);

  // Make root part
  nsresult rv = NS_NewRootPart(&mRoot, aDoc);
  if (NS_OK == rv) {
  }

  return rv;
}

NS_IMPL_ISUPPORTS(HTMLContentSink,kIHTMLContentSinkIID)

PRBool HTMLContentSink::OpenHTML(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("OpenHTML", aNode)
  NS_PRECONDITION(0 == mStackPos, "bad stack pos");

  mNodeStack[0] = (eHTMLTags)aNode.GetNodeType();
  mContainerStack[0] = mRoot;
  mStackPos = 1;

  return PR_TRUE;
}

PRBool HTMLContentSink::CloseHTML(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("CloseHTML", aNode)

  NS_ASSERTION(mStackPos > 0, "bad bad");
  mNodeStack[--mStackPos] = eHTMLTag_unknown;

  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (nsnull != shell) {
      shell->BeginObservingDocument();
      NS_RELEASE(shell);
    }
  }
  NS_IF_RELEASE(mCurrentForm);

  return PR_TRUE;
}

PRBool HTMLContentSink::OpenHead(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("OpenHead", aNode)

  mNodeStack[mStackPos++] = (eHTMLTags)aNode.GetNodeType();
  return PR_TRUE;
}

PRBool HTMLContentSink::CloseHead(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("CloseHead", aNode)

  NS_ASSERTION(mStackPos > 0, "bad bad");
  mNodeStack[--mStackPos] = eHTMLTag_unknown;
  return PR_TRUE;
}

PRBool HTMLContentSink::SetTitle(const nsString& aValue)
{
  if (nsnull == mTitle) {
    mTitle = new nsString(aValue);
  }
  else {
    *mTitle = aValue;
  }
  mTitle->CompressWhitespace(PR_TRUE, PR_TRUE);
  ((nsHTMLDocument*)mDocument)->SetTitle(*mTitle);
  return PR_TRUE;
}

PRBool HTMLContentSink::OpenBody(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("OpenBody", aNode)

  PRBool startLayout = PR_FALSE;
  if (nsnull == mBody) {
    nsIAtom* atom = NS_NewAtom("BODY");
    nsresult rv = NS_NewBodyPart(&mBody, atom);
    if (NS_OK == rv) {
      mRoot->AppendChild(mBody);
      startLayout = PR_TRUE;
    }
    NS_RELEASE(atom);
  }

  mNodeStack[mStackPos] = (eHTMLTags)aNode.GetNodeType();
  mContainerStack[mStackPos] = mBody;
  mStackPos++;

  // Add attributes to the body content object, but only if it's really a body
  // tag that is triggering the OpenBody.
  if (aNode.GetText().EqualsIgnoreCase("body")) {
    AddAttributes(aNode, mBody);
    // XXX If the body already existed and has been reflowed somewhat
    // then we need to trigger a style change
  }

  if (startLayout) {
    // XXX This has to be done now that the body is in because we
    // don't know how to handle a content-appended reflow if the
    // root has no children
    StartLayout();
  }

  return PR_TRUE;
}

PRBool HTMLContentSink::CloseBody(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("CloseBody", aNode)

  NS_ASSERTION(mStackPos > 0, "bad bad");
  mNodeStack[--mStackPos] = eHTMLTag_unknown;

  // Reflow any lingering content
  ReflowNewContent();

  return PR_TRUE;
}


PRBool HTMLContentSink::OpenForm(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("OpenForm", aNode)

  // Close out previous form if it's there
  if (nsnull != mCurrentForm) {
	  NS_RELEASE(mCurrentForm);
    mCurrentForm = nsnull;
  }

  // Create new form
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(tmp);
  nsresult rv = NS_NewHTMLForm(&mCurrentForm, atom);
  NS_RELEASE(atom);

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

  return PR_TRUE;
}

PRBool HTMLContentSink::CloseForm(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("CloseForm", aNode)

  if (nsnull != mCurrentForm) {
	  NS_RELEASE(mCurrentForm);
    mCurrentForm = nsnull;
  }
  return PR_TRUE;
}

PRBool HTMLContentSink::OpenFrameset(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("OpenFrameset", aNode)

  mNodeStack[mStackPos++] = (eHTMLTags)aNode.GetNodeType();
  return PR_TRUE;
}

PRBool HTMLContentSink::CloseFrameset(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("CloseFrameset", aNode)

  mNodeStack[--mStackPos] = eHTMLTag_unknown;
  return PR_TRUE;
}

PRBool HTMLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("OpenContainer", aNode)

  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(tmp);

  nsresult rv;
  nsIHTMLContent* container = nsnull;
  if (aNode.GetTokenType() == eToken_start) {
    switch (aNode.GetNodeType()) {
    case eHTMLTag_map:
      NS_IF_RELEASE(mCurrentMap);
      rv = NS_NewImageMap(&mCurrentMap, atom);
      if (NS_OK == rv) {
        // Look for name attribute and set the map name
        PRInt32 ac = aNode.GetAttributeCount();
        for (PRInt32 i = 0; i < ac; i++) {
          const nsString& key = aNode.GetKeyAt(i);
          if (key.EqualsIgnoreCase("name")) {
            nsAutoString name;
            GetAttributeValueAt(aNode, i, name);
            name.StripWhitespace();     // XXX leading, trailing, interior non=-space ws is removed
            mCurrentMap->SetName(name);
          }
        }
        // Add the map to the document
        ((nsHTMLDocument*)mDocument)->AddImageMap(mCurrentMap);
      }
      return PR_TRUE;

    case eHTMLTag_table:
      rv = NS_NewTablePart(&container, atom);
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

    default:
      rv = NS_NewHTMLContainer(&container, atom);
      break;
    }
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
  return PR_TRUE;
}

PRBool HTMLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  NOISY_SINK_TRACE("CloseContainer", aNode)

  switch (aNode.GetNodeType()) {
  case eHTMLTag_map:
    NS_IF_RELEASE(mCurrentMap);
    return PR_TRUE;
  }

  // XXX we could assert things about the top tag name == aNode.getText
  if (0 == mStackPos) {
    // Can't pop empty stack
    return PR_TRUE;
  }

  --mStackPos;
  nsIHTMLContent* container = mContainerStack[mStackPos];
  mNodeStack[mStackPos] = eHTMLTag_unknown;
  mContainerStack[mStackPos] = nsnull;

  if (nsnull != container) {
    // Now that this container is complete, append it to it's parent
    eHTMLTags parentType;
    nsIHTMLContent* parent = GetCurrentContainer(&parentType);
    container->Compact();

    if(parent) {
      parent->AppendChild(container);
      if (parent == mBody) {
        // We just closed a child of the body off. Trigger a
        // content-appended reflow if enough time has elapsed
        PRTime now = PR_Now();
        if (now - mLastUpdateTime >= mUpdateDelta) {
          mLastUpdateTime = now;
          mUpdateDelta += mUpdateDelta;
          ReflowNewContent();
        }
      }
    }
    NS_RELEASE(container);
  }
  return PR_TRUE;
}

void HTMLContentSink::StartLayout()
{
  if (!mLayoutStarted) {
    PRInt32 i, ns = mDocument->GetNumberOfShells();
    for (i = 0; i < ns; i++) {
      nsIPresShell* shell = mDocument->GetShellAt(i);
      if (nsnull != shell) {
        nsIPresContext* cx = shell->GetPresContext();
        nsRect r = cx->GetVisibleArea();
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
  printf("reflow body\n");

  // Trigger reflows in each of the presentation shells
  PRInt32 i, ns = mDocument->GetNumberOfShells();
  for (i = 0; i < ns; i++) {
    nsIPresShell* shell = mDocument->GetShellAt(i);
    if (nsnull != shell) {
      shell->ContentAppended(mBody);
      NS_RELEASE(shell);
    }
  }
}

PRBool HTMLContentSink::CloseTopmostContainer()
{
  return PR_TRUE;
}

nsIHTMLContent* HTMLContentSink::GetCurrentContainer(eHTMLTags* aType)
{
  nsIHTMLContent* parent;
  if (mStackPos <= 2) {         // assume HTML and BODY are on the stack
    parent = mBody;
    *aType = eHTMLTag_body;
  } else {
    parent = mContainerStack[mStackPos - 1];
    *aType = mNodeStack[mStackPos - 1];
  }
  return parent;
}

//----------------------------------------------------------------------

// Leaf tag handling code

PRBool HTMLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  REALLY_NOISY_SINK_TRACE("AddLeaf", aNode)

  // Check for nodes that require special handling
  switch (aNode.GetNodeType()) {
  case eHTMLTag_style:
    ProcessSTYLETag(aNode);
    return PR_TRUE;

  case eHTMLTag_script:
    return PR_TRUE;

  case eHTMLTag_area:
    ProcessAREATag(aNode);
    return PR_TRUE;
  }

  eHTMLTags parentType;
  nsIHTMLContent* parent = GetCurrentContainer(&parentType);

  switch (parentType) {
  case eHTMLTag_table:
  case eHTMLTag_tr:
    // XXX Discard leaf content (those annoying \n's really) in
    // table's or table rows
    return PR_TRUE;
  }

  nsresult rv = NS_OK;
  nsIHTMLContent* leaf = nsnull;
  switch (aNode.GetTokenType()) {
  case eToken_start:
    switch (aNode.GetNodeType()) {
    case eHTMLTag_br:
      rv = ProcessBRTag(&leaf, aNode);
      break;
    case eHTMLTag_hr:
      rv = ProcessHRTag(&leaf, aNode);
      break;
    case eHTMLTag_input:
      rv = ProcessINPUTTag(&leaf, aNode);
      break;
    case eHTMLTag_img:
      rv = ProcessIMGTag(&leaf, aNode);
      break;
    case eHTMLTag_spacer:
      rv = ProcessSPACERTag(&leaf, aNode);
      break;
    }
    break;

  case eToken_text:
  case eToken_whitespace:
    {
      nsAutoString tmp;
      tmp.Append(aNode.GetText());
      rv = NS_NewHTMLText(&leaf, tmp.GetUnicode(), tmp.Length());
    }
    break;

    // XXX ick
  case eToken_newline:
    {
      nsAutoString tmp;
      tmp.Append('\n');
      rv = NS_NewHTMLText(&leaf, tmp.GetUnicode(), tmp.Length());
    }
    break;

  case eToken_entity:
    {
      nsAutoString tmp;
      nsAutoString tmp2("");
      PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp2);
      if (unicode < 0) {
        tmp.Append(aNode.GetText());
      } else {
        tmp+=tmp2;
      }
      rv = NS_NewHTMLText(&leaf, tmp.GetUnicode(), tmp.Length());
    }
    break;

  case eToken_skippedcontent:
    break;
  }

  if (NS_OK == rv) {
    if (nsnull != leaf) {
      if (nsnull != parent) {
        parent->AppendChild(leaf);
      } else {
        // XXX drop stuff on the floor that doesn't have a container!
        // Bad parser!
      }
    }
  }
  NS_IF_RELEASE(leaf);

  return PR_TRUE;
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

nsresult HTMLContentSink::ProcessBRTag(nsIHTMLContent** aInstancePtrResult,
                                       const nsIParserNode& aNode)
{
  nsresult rv = NS_OK;
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(tmp);
  rv = NS_NewHTMLBreak(aInstancePtrResult, atom);
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
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
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
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
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
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(tmp);
  nsresult rv = NS_NewHTMLSpacer(aInstancePtrResult, atom);
  if (NS_OK == rv) {
    rv = AddAttributes(aNode, *aInstancePtrResult);
  }
  NS_RELEASE(atom);
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
    PRInt32 ec;
    if (nsnull != mStyleSheet) {
      parser->SetStyleSheet(mStyleSheet);
      // XXX we do probably need to trigger a style change reflow
      // when we are finished if this is adding data to the same sheet
    }
    nsIStyleSheet* sheet = parser->Parse(&ec, aUIN, mDocumentURL);
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
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
  nsIAtom* atom = NS_NewAtom(tmp);

  nsresult rv = NS_ERROR_NOT_INITIALIZED;

  // Find type attribute and then create the appropriate form element
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    const nsString& key = aNode.GetKeyAt(i);
    if (key.EqualsIgnoreCase("type")) {
      const nsString& val= aNode.GetValueAt(i);
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
      else {
        rv = NS_NewHTMLInputSubmit(aInstancePtrResult, atom, mCurrentForm);
      }
      break;
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

nsresult HTMLContentSink::ProcessWBRTag(nsIHTMLContent** aInstancePtrResult,
                                        const nsIParserNode& aNode)
{
  nsAutoString tmp(aNode.GetText());
  tmp.ToUpperCase();
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
                               nsIURL* aURL)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  HTMLContentSink* it = new HTMLContentSink();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult rv = it->Init(aDoc, aURL);
  if (NS_OK != rv) {
    delete it;
    return rv;
  }
  return it->QueryInterface(kIHTMLContentSinkIID, (void **)aInstancePtrResult);
}

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
#include "nsHTMLParts.h"
#include "nsCRT.h"
#include "nsSplittableFrame.h"
#include "nsIInlineReflow.h"
#include "nsCSSLineLayout.h"
#include "nsHTMLContent.h"
#include "nsString.h"
#include "nsIPresContext.h"
#include "nsStyleConsts.h"
#include "nsIStyleContext.h"
#include "nsCoord.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"
#include "nsHTMLIIDs.h"
#include "nsIPresShell.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsITimerCallback.h"
#include "nsITimer.h"
#include "prtime.h"
#include "nsVoidArray.h"
#include "prprf.h"
#include "nsIDOMText.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsXIFConverter.h"
#include "nsHTMLAtoms.h"
#include "nsITextContent.h"

// Selection includes
#include "nsISelection.h"
#include "nsSelectionRange.h"

#define CALC_DEBUG 0
#define DO_SELECTION 0

static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);

#ifdef NS_DEBUG
#undef NOISY
#undef NOISY_BLINK
#else
#undef NOISY
#undef NOISY_BLINK
#endif

// XXX TODO:
// 0. re-implement justified text
// 1. add in a rendering method that can render justified text
// 2. text renderer should negotiate with rendering context/font
//    metrics to see what it can handle; for example, if the renderer can
//    automatically deal with underlining, strikethrough, justification,
//    etc, then the text renderer should let the rc do the work;
//    otherwise there should be XP fallback code here.

// XXX TODO:
// implement nsIFrame::Reflow

// XXX Speedup ideas
// 1. justified text can use word width information during resize reflows
// 2. when we are doing an unconstrained reflow we know we are going to
// get reflowed again; collect up the word widths we are computing as we
// do this and then save them in the mWords; later on when we get reflowed
// again we can destroy them
// 3. when pulling up text get word width information from next-in-flow

// XXX temporary
#define XP_IS_SPACE(_ch) \
  (((_ch) == ' ') || ((_ch) == '\t') || ((_ch) == '\n'))

// XXX need more of this in nsIFontMetrics.GetWidth
#define CH_NBSP 160

// XXX use a PreTextFrame for pre-formatted text?

static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);

class TextFrame;

class TextTimer : public nsITimerCallback {
public:
  TextTimer();
  ~TextTimer();

  NS_DECL_ISUPPORTS

  void AddFrame(TextFrame* aFrame);

  PRBool RemoveFrame(TextFrame* aFrame);

  PRInt32 FrameCount();

  void Start();

  void Stop();

  virtual void Notify(nsITimer *timer);

  nsITimer* mTimer;
  nsVoidArray mFrames;
};

class TextFrame : public nsSplittableFrame, private nsIInlineReflow {
public:
  TextFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  NS_IMETHOD GetCursorAndContentAt(nsIPresContext& aPresContext,
                         const nsPoint& aPoint,
                         nsIFrame** aFrame,
                         nsIContent** aContent,
                         PRInt32& aCursor);
  NS_IMETHOD ContentChanged(nsIPresShell*   aShell,
                            nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD ListTag(FILE* out) const;

  // nsIInlineReflow
  NS_IMETHOD FindTextRuns(nsCSSLineLayout&  aLineLayout,
                          nsIReflowCommand* aReflowCommand);
  NS_IMETHOD InlineReflow(nsCSSLineLayout&     aLineLayout,
                          nsReflowMetrics&     aMetrics,
                          const nsReflowState& aReflowState);

  PRInt32 GetPosition(nsIPresContext& aCX,
                      nsGUIEvent*     aEvent,
                      nsIFrame *      aNewFrame,
                      PRUint32&       aAcutalContentOffset);

  void    CalcCursorPosition(nsIPresContext& aPresContext,
                             nsGUIEvent*     aEvent,
                             TextFrame *     aNewFrame,
                             PRInt32 &       aOffset, 
                             PRInt32 &       aWidth);

  void    CalcActualPosition(PRUint32         &aMsgType,
                             const PRUnichar* aCPStart, 
                             const PRUnichar* aCP, 
                             PRInt32 &        aOffset, 
                             PRInt32 &        aWidth,
                             nsIFontMetrics * aFM);

  PRInt32 GetContentOffset() { return mContentOffset;}
  PRInt32 GetContentLength() { return mContentLength;}

  char * CompressWhiteSpace(char            * aBuffer,
                            PRInt32           aBufSize,
                            PRUint16        * aIndexes,
                            PRUint32        & aStrLen,
                            PRBool          & aShouldDeleteStr);

protected:
  virtual ~TextFrame();

  void PaintRegularText(nsIPresContext& aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect& aDirtyRect,
                        nscoord dx, nscoord dy);

  nsInlineReflowStatus ReflowPre(nsCSSLineLayout& aLineLayout,
                                 nsReflowMetrics& aMetrics,
                                 const nsReflowState& aReflowState,
                                 const nsStyleFont& aFont,
                                 PRInt32 aStartingOffset);

  nsInlineReflowStatus ReflowNormal(nsCSSLineLayout& aLineLayout,
                                    nsReflowMetrics& aMetrics,
                                    const nsReflowState& aReflowState,
                                    const nsStyleFont& aFontStyle,
                                    const nsStyleText& aTextStyle,
                                    PRInt32 aStartingOffset);

public:
  PRInt32 mContentOffset;
  PRInt32 mContentLength;

  // XXX need a better packing
  PRUint32 mFlags;
  PRUint32 mColumn;
};

// Flag information used by rendering code. This information is
// computed by the ResizeReflow code. Remaining bits are used by the
// tab count.
#define TEXT_SKIP_LEADING_WS    0x01
#define TEXT_HAS_MULTIBYTE      0x02
#define TEXT_IS_PRE             0x04
#define TEXT_BLINK_ON           0x08
#define TEXT_ENDS_IN_WHITESPACE 0x10

#define TEXT_GET_TAB_COUNT(_mf) ((_mf) >> 5)
#define TEXT_SET_TAB_COUNT(_mf,_tabs) (_mf) = (_mf) | ((_tabs)<< 5)

#if XXX
// mWords bit definitions
#define WORD_IS_WORD                    0x80000000L
#define WORD_IS_SPACE                   0x00000000L
#define WORD_LENGTH_MASK                0x000000FFL
#define WORD_LENGTH_SHIFT               0
#define WORD_WIDTH_MASK                 0x7FFFFF00L
#define WORD_WIDTH_SHIFT                8
#define MAX_WORD_LENGTH                 256
#define MAX_WORD_WIDTH                  0x7FFFFFL

#define GET_WORD_LENGTH(_wi) \
  ((PRIntn) (((_wi) & WORD_LENGTH_MASK)/* >> WORD_LENGTH_SHIFT*/))
#define GET_WORD_WIDTH(_wi) \
  ((nscoord) ((PRUint32(_wi) & WORD_WIDTH_MASK) >> WORD_WIDTH_SHIFT))
#endif

#if 0
class Text : public nsHTMLContent, public nsIDOMText {
public:
  Text(const PRUnichar* aText, PRInt32 aLength);

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

#if 0
  virtual PRInt32 GetLength();
  virtual void GetText(nsStringBuf* aBuf, PRInt32 aOffset, PRInt32 aCount);
#endif

  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;

  NS_IMETHOD ToHTMLString(nsString& aBuf) const;

  NS_IMETHOD CreateFrame(nsIPresContext* aPresContext,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult);

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);

  // nsIDOMNode interface
  NS_IMETHOD    GetNodeName(nsString& aNodeName);
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue);
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);
  NS_IMETHOD    GetNodeType(PRInt32* aNodeType);
  NS_IMETHOD    CloneNode(nsIDOMNode** aReturn);
  NS_IMETHOD    Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn);
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);

  // nsIDOMData interface
  NS_IMETHOD    GetData(nsString& aData);
  NS_IMETHOD    SetData(const nsString& aData);
  NS_IMETHOD    GetSize(PRUint32* aSize);
  NS_IMETHOD    Substring(PRUint32 aStart, PRUint32 aEnd, nsString& aReturn);
  NS_IMETHOD    Append(const nsString& aData);
  NS_IMETHOD    Insert(PRUint32 aOffset, const nsString& aData);
  NS_IMETHOD    Remove(PRUint32 aOffset, PRUint32 aCount);
  NS_IMETHOD    Replace(PRUint32 aOffset, PRUint32 aCount, 
                        const nsString& aData);

  // nsIDOMText interface
  NS_IMETHOD    SplitText(PRUint32 aOffset, nsIDOMText** aReturn);
  NS_IMETHOD    JoinText(nsIDOMText* aNode1, nsIDOMText* aNode2, nsIDOMText** aReturn);

  void ToCString(nsString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  
  /**
   * Translate the content object into the (XIF) XML Interchange Format
   * XIF is an intermediate form of the content model, the buffer
   * will then be parsed into any number of formats including HTML, TXT, etc.

   * BeginConvertToXIF -- opens a container and writes out the attributes
   * ConvertContentToXIF -- typically does nothing unless there is text content
   * FinishConvertToXIF -- closes a container
   
  */
  NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const;
  NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const;
  NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const;

  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const {
    return NS_CONTENT_ATTR_NOT_THERE;
  }

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsString& aValue,
                               nsHTMLValue& aResult) {
    return NS_CONTENT_ATTR_NOT_THERE;
  }

  PRUnichar* mText;
  PRInt32 mLength;

protected:
  Text();
  virtual ~Text();
};
#endif

//----------------------------------------------------------------------

static PRBool gBlinkTextOff;
static TextTimer* gTextBlinker;
#ifdef NOISY_BLINK
static PRTime gLastTick;
#endif

TextTimer::TextTimer()
{
  NS_INIT_REFCNT();
  mTimer = nsnull;
}

TextTimer::~TextTimer()
{
  Stop();
}

void TextTimer::Start()
{
  nsresult rv = NS_NewTimer(&mTimer);
  if (NS_OK == rv) {
    mTimer->Init(this, 750);
  }
}

void TextTimer::Stop()
{
  if (nsnull != mTimer) {
    mTimer->Cancel();
    NS_RELEASE(mTimer);
  }
}

static NS_DEFINE_IID(kITimerCallbackIID, NS_ITIMERCALLBACK_IID);
NS_IMPL_ISUPPORTS(TextTimer, kITimerCallbackIID);

void TextTimer::AddFrame(TextFrame* aFrame) {
  mFrames.AppendElement(aFrame);
  if (1 == mFrames.Count()) {
    Start();
  }
printf("%d blinking frames [add %p]\n", mFrames.Count(), aFrame);
}

PRBool TextTimer::RemoveFrame(TextFrame* aFrame) {
  PRBool rv = mFrames.RemoveElement(aFrame);
  if (0 == mFrames.Count()) {
    Stop();
  }
printf("%d blinking frames [remove %p] [%s]\n",
       mFrames.Count(), aFrame, rv ? "true" : "FALSE");
  return rv;
}

PRInt32 TextTimer::FrameCount() {
  return mFrames.Count();
}

void TextTimer::Notify(nsITimer *timer)
{
  // Toggle blink state bit so that text code knows whether or not to
  // render. All text code shares the same flag so that they all blink
  // in unison.
  gBlinkTextOff = PRBool(!gBlinkTextOff);

  // XXX hack to get auto-repeating timers; restart before doing
  // expensive work so that time between ticks is more even
  Stop();
  Start();

#ifdef NOISY_BLINK
  PRTime now = PR_Now();
  char buf[50];
  PRTime delta;
  LL_SUB(delta, now, gLastTick);
  gLastTick = now;
  PR_snprintf(buf, sizeof(buf), "%lldusec", delta);
  printf("%s\n", buf);
#endif

  PRInt32 i, n = mFrames.Count();
  for (i = 0; i < n; i++) {
    TextFrame* text = (TextFrame*) mFrames.ElementAt(i);

    // Determine damaged area and tell view manager to redraw it
    nsPoint offset;
    nsRect bounds;
    text->GetRect(bounds);
    nsIView* view;
    text->GetOffsetFromView(offset, view);
    nsIViewManager* vm;
    view->GetViewManager(vm);
    bounds.x = offset.x;
    bounds.y = offset.y;
    vm->UpdateView(view, bounds, 0);
    NS_RELEASE(vm);
  }
}

//----------------------------------------------------------------------

nsresult
NS_NewTextFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                nsIFrame*& aResult)
{
  nsIFrame* frame = new TextFrame(aContent, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

TextFrame::TextFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsSplittableFrame(aContent, aParentFrame)
{
  if (nsnull == gTextBlinker) {
    // Create text timer the first time out
    gTextBlinker = new TextTimer();
  }
  NS_ADDREF(gTextBlinker);
}

TextFrame::~TextFrame()
{
  if (0 != (mFlags & TEXT_BLINK_ON)) {
    NS_ASSERTION(nsnull != gTextBlinker, "corrupted blinker");
    gTextBlinker->RemoveFrame(this);
  }
  if (0 == gTextBlinker->Release()) {
    // Release text timer when the last text frame is gone
    gTextBlinker = nsnull;
  }
}

NS_IMETHODIMP
TextFrame::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIInlineReflowIID)) {
    *aInstancePtrResult = (void*) ((nsIInlineReflow*)this);
    return NS_OK;
  }
  return nsFrame::QueryInterface(aIID, aInstancePtrResult);
}

NS_METHOD TextFrame::GetCursorAndContentAt(nsIPresContext& aPresContext,
                                 const nsPoint& aPoint,
                                 nsIFrame** aFrame,
                                 nsIContent** aContent,
                                 PRInt32& aCursor)
{
  *aContent = mContent;
  aCursor = NS_STYLE_CURSOR_IBEAM;
  return NS_OK;
}

NS_METHOD TextFrame::ContentChanged(nsIPresShell*   aShell,
                                    nsIPresContext* aPresContext,
                                    nsIContent*     aChild,
                                    nsISupports*    aSubContent)
{
  // Generate a reflow command with this frame as the target frame
  nsIReflowCommand* cmd;
  nsresult          result;
                                                
  result = NS_NewHTMLReflowCommand(&cmd, this, nsIReflowCommand::ContentChanged);
  if (NS_OK == result) {
    aShell->AppendReflowCommand(cmd);
    NS_RELEASE(cmd);
  }

  return result;
}

// XXX it would be nice to use a method pointer here, but I don't want
// to pay for the storage.

NS_METHOD TextFrame::Paint(nsIPresContext& aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect)
{
  if ((0 != (mFlags & TEXT_BLINK_ON)) && gBlinkTextOff) {
    return NS_OK;
  }

  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    // Get style data
    const nsStyleColor* color =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleFont* font =
      (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

    // Set font and color
    aRenderingContext.SetColor(color->mColor);
    aRenderingContext.SetFont(font->mFont);
    PaintRegularText(aPresContext, aRenderingContext, aDirtyRect, 0, 0);

#if XXX
    if (font->mThreeD) {
      nscoord onePixel = NSIntPixelsToTwips(1, aPresContext.GetPixelsToTwips());
      aRenderingContext.SetColor(color->mBackgroundColor);
      PaintRegularText(aPresContext, aRenderingContext, aDirtyRect, onePixel, onePixel);
    }
#endif
  }

  return NS_OK;
}

/**
 * To keep things efficient we depend on text content implementing
 * our nsITextContent API
 */
static const PRUnichar*
GetText(nsIContent* aContent, PRInt32& aLengthResult)
{
  const PRUnichar* cp = nsnull;
  nsITextContent* tc = nsnull;
  aContent->QueryInterface(kITextContentIID, (void**) &tc);
  if (nsnull != tc) {
    tc->GetText(cp, aLengthResult);
    NS_RELEASE(tc);
  }
  return cp;
}

//---------------------------------------------------------------------
// Compresses the White Space and builds a mapping between
// the compressed and uncompressed strings
//---------------------------------------------------------------------
char * TextFrame::CompressWhiteSpace(char            * aBuffer,
                                     PRInt32           aBufSize,
                                     PRUint16        * aIndexes,
                                     PRUint32        & aStrLen,
                                     PRBool          & aShouldDeleteStr) 
{

  char* s  = aBuffer;
  char* s0 = s;

  aBuffer[0] = 0;
  aShouldDeleteStr    = PR_FALSE;
  PRUint16 mappingInx = 0;
  PRUint16 strInx     = 0;
  aStrLen             = 0;

  memset(aIndexes, 255, 256); //debug only
  
  // XXX inefficient (especially for really large strings)
  PRInt32 textLength;
  const PRUnichar* cp = GetText(mContent, textLength);
  if (0 == textLength) {
    aStrLen = 0;
    aShouldDeleteStr = PR_FALSE;
    return aBuffer;
  }
  cp += mContentOffset;
  const PRUnichar* end = cp + mContentLength;

  // Skip leading space if necessary

  strInx = mContentOffset;

  if ((mFlags & TEXT_SKIP_LEADING_WS) != 0) {
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (!XP_IS_SPACE(ch)) {
        cp--;
        break;
      } else {
        aIndexes[mappingInx++] = strInx;
      }
    }
  }

  if ((mFlags & TEXT_IS_PRE) != 0) {
    if ((mFlags & TEXT_HAS_MULTIBYTE) != 0) {
      // XXX to be written
    } else {
      PRIntn tabs = TEXT_GET_TAB_COUNT(mFlags);

      // Make a copy of the text we are to render, translating tabs
      // into whitespace.
      PRInt32 maxLen = (end - cp) + 8*tabs;
      if (maxLen > aBufSize) {
        s0 = s = new char[maxLen];
        aShouldDeleteStr = PR_TRUE;
      }

      // Translate tabs into whitespace; translate other whitespace into
      // spaces.
      PRIntn col = (PRIntn) mColumn;
      while (cp < end) {
        PRUint8 ch = (PRUint8) *cp++;
        if (XP_IS_SPACE(ch)) {
          if (ch == '\t') {
            PRIntn spaces = 8 - (col & 7);
            col += spaces;
            while (--spaces >= 0) {
              *s++ = ' ';
              aStrLen++;
            }
            aIndexes[mappingInx++] = strInx;
            strInx++;
            continue;
          } else {
            *s++ = ' ';
            aStrLen++;
            aIndexes[mappingInx++] = strInx;
            strInx++;
          }
        } else if (ch == CH_NBSP) {
          *s++ = ' ';
          aStrLen++;
          aIndexes[mappingInx++] = strInx;
          strInx++;
        } else {
          *s++ = ch;
          aStrLen++;
          aIndexes[mappingInx++] = strInx;
          strInx++;
        }
        col++;
      }
    }
  } else { // is TEXT_IS_PRE

    if ((mFlags & TEXT_HAS_MULTIBYTE) != 0) {
      // XXX to be written
    } else {
      // Make a copy of the text we are to render, compressing out
      // whitespace; translating whitespace to literal spaces;
      // eliminating trailing whitespace.
      char* s = aBuffer;
      PRInt32 maxLen = end - cp;
      if (maxLen > aBufSize) {
        s0 = s = new char[maxLen];
        aShouldDeleteStr = PR_TRUE;
      }

      // Compress down the whitespace
      while (cp < end) {
        PRUint8 ch = (PRUint8) *cp++;
        if (XP_IS_SPACE(ch)) {
          while (cp < end) {
            ch = (PRUint8) *cp++;
            if (!XP_IS_SPACE(ch)) {
              cp--;
              break;
            } else {
              aIndexes[mappingInx++] = strInx;
            }
          }
          *s++ = ' ';
          aStrLen++;
          aIndexes[mappingInx++] = strInx;
          strInx++;
        } else {
          *s++ = ch;
          aStrLen++;
          aIndexes[mappingInx++] = strInx;
          strInx++;
        }
      }
    }
  }

  //aStrLen = s - s0;
  return s0;
}

void
TextFrame::PaintRegularText(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nscoord dx, nscoord dy)
{
  // Get Selection Object
  nsIPresShell * shell = aPresContext.GetShell();
  nsIDocument  * doc   = shell->GetDocument();
  

  if (doc->GetDisplaySelection() == PR_FALSE)
  {
    // Skip leading space if necessary
    PRInt32 textLength;
    const PRUnichar* cp  = GetText(mContent, textLength);
    if (0 == textLength) {
      return;
    }
    cp += mContentOffset;
    const PRUnichar* end = cp + mContentLength;

    // Skip leading space if necessary

    if ((mFlags & TEXT_SKIP_LEADING_WS) != 0) {
      while (cp < end) {
        PRUnichar ch = *cp++;
        if (!XP_IS_SPACE(ch)) {
          cp--;
          break;
        }
      }
    }


    if ((mFlags & TEXT_IS_PRE) != 0) {
      if ((mFlags & TEXT_HAS_MULTIBYTE) != 0) {
        // XXX to be written
      } else {
        PRIntn tabs = TEXT_GET_TAB_COUNT(mFlags);

        // Make a copy of the text we are to render, translating tabs
        // into whitespace.
        char buf[100];
        char* s = buf;
        char* s0 = s;
        PRInt32 maxLen = (end - cp) + 8*tabs;
        if (maxLen > sizeof(buf)) {
          s0 = s = new char[maxLen];
        }

        // Translate tabs into whitespace; translate other whitespace into
        // spaces.
        PRIntn col = (PRIntn) mColumn;
        while (cp < end) {
          PRUint8 ch = (PRUint8) *cp++;
          if (XP_IS_SPACE(ch)) {
            if (ch == '\t') {
              PRIntn spaces = 8 - (col & 7);
              col += spaces;
              while (--spaces >= 0) {
                *s++ = ' ';
              }
              continue;
            } else {
              *s++ = ' ';
            }
          } else if (ch == CH_NBSP) {
            *s++ = ' ';
          } else {
            *s++ = ch;
          }
          col++;
        }

        // Render the text
        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);
        aRenderingContext.DrawString(s0, s - s0, dx, dy, mRect.width);

/*
        // ROD -- WARNING!!!!!
        // 1.The fixed size buffers causes the application to CRASH.
        // 2 These statements do not appear to do anything.

        char cmpBuf[256];
        char sBuf[256];
        int len = s - s0;
        strncpy(cmpBuf, compressedStr, compressedStrLen);
        cmpBuf[compressedStrLen] = 0;
        strncpy(sBuf, s0, len);
        cmpBuf[compressedStrLen] = 0;

        if (compressedStrLen != (PRUint32)len ||
            strcmp(cmpBuf, sBuf)) {
          int x = 0;
        }
*/

        if (s0 != buf) {
          delete[] s0;
        }
      }

    } else {
      if ((mFlags & TEXT_HAS_MULTIBYTE) != 0) {
        // XXX to be written
      } else {
        // Make a copy of the text we are to render, compressing out
        // whitespace; translating whitespace to literal spaces;
        // eliminating trailing whitespace.
        char buf[100];
        char* s = buf;
        char* s0 = s;
        PRInt32 maxLen = end - cp;
        if (maxLen > sizeof(buf)) {
          s0 = s = new char[maxLen];
        }

        // Compress down the whitespace
        while (cp < end) {
          PRUint8 ch = (PRUint8) *cp++;
          if (XP_IS_SPACE(ch)) {
            while (cp < end) {
              ch = (PRUint8) *cp++;
              if (!XP_IS_SPACE(ch)) {
                cp--;
                break;
              }
            }
            *s++ = ' ';
          } else {
            *s++ = ch;
          }
        }

        // Render the text
        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);
        aRenderingContext.DrawString(s0, s - s0, dx, dy, mRect.width);

/*
        // ROD -- WARNING!!!!!
        // 1.The fixed size buffers causes the application to CRASH.
        // 2 These statements do not appear to do anything.
        // gpk


        char cmpBuf[256];
        char sBuf[256];
        int len = s - s0;
        strncpy(cmpBuf, compressedStr, compressedStrLen);
        cmpBuf[compressedStrLen] = 0;
        strncpy(sBuf, s0, len);
        sBuf[len] = 0;

        if (compressedStrLen != (PRUint32)len ||
            strcmp(cmpBuf, sBuf)) {
          int x = 0;
        }
*/

        if (s0 != buf) {
          delete[] s0;
        }
      }
    }
    NS_RELEASE(doc);
    NS_RELEASE(shell);
    return;
  }


  PRBool   delCompressedStr;
  PRUint16 indexes[1024];
  PRUint32 compressedStrLen = 0;
  char     buf[128];
  char *   compressedStr  = CompressWhiteSpace(buf, sizeof(buf), indexes, compressedStrLen, delCompressedStr);
  
  nsIDeviceContext * deviceContext = aRenderingContext.GetDeviceContext();
  
  nsISelection     * selection = doc->GetSelection();
  nsSelectionRange * range     = selection->GetRange();

  nsSelectionPoint * startPnt  = range->GetStartPoint();
  nsSelectionPoint * endPnt    = range->GetEndPoint();

  nsIContent       * startContent = startPnt->GetContent();
  nsIContent       * endContent   = endPnt->GetContent();

  PRBool startEndInSameContent;

  // This is a little more optimized, 
  // Check to see if the start and end points are in the same content 
  if (mContent == startContent && mContent == endContent) {
    startEndInSameContent = PR_TRUE;  // If this gets set to TRUE then we get to skip the IsInRange check

    // The Start & End contents are the same 
    // Now check to see if it is an "insert" cursor 
    if (startPnt->GetOffset() == endPnt->GetOffset()) {
      /*nsString target;
      startContent->GetAttribute(nsString("target"), target);
      if (target.Length() == 0) {
        int x = 0;
        startContent->GetAttribute(nsString(NS_HTML_BASE_TARGET), target);
        printf("Target length %d\n", target.Length());
      }
      printf("Target length %d\n", target.Length());*/

		  // Render the text Normal
		  const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
		  aRenderingContext.SetColor(color->mColor);
		  aRenderingContext.DrawString(compressedStr, compressedStrLen, dx, dy, mRect.width);

      // If cursor is in this frame the draw it
      if (startPnt->GetOffset() >= mContentOffset &&
          startPnt->GetOffset() < mContentOffset+mContentLength) { // Draw Cursor Only
        nsIFontMetrics * fm = aRenderingContext.GetFontMetrics();
		    nsString textStr;
		    textStr.Append(compressedStr, endPnt->GetOffset()- mContentOffset);

		    nscoord textLen;
        fm->GetWidth(deviceContext, textStr, textLen);

        // Draw little blue triangle
		    aRenderingContext.SetColor(NS_RGB(0,0,255));
        nsPoint pnts[4];
        nscoord ox = mRect.height / 4;
        nscoord oy = ox;
        nscoord yy = mRect.height;

        pnts[0].x = textLen-ox;
        pnts[0].y = yy;
        pnts[1].x = textLen;
        pnts[1].y = yy-oy;
        pnts[2].x = textLen+ox;
        pnts[2].y = yy;
        pnts[3].x = textLen-ox;
        pnts[3].y = yy;

        //aRenderingContext.DrawPolygon(pnts, 4);
        aRenderingContext.FillPolygon(pnts, 4);
		    //aRenderingContext.DrawLine(textLen, mRect.height-(mRect.height/3), textLen, mRect.height);
        NS_RELEASE(fm);
      }
      NS_IF_RELEASE(startContent);
      NS_IF_RELEASE(endContent);
      NS_IF_RELEASE(selection);
      NS_RELEASE(doc);
      NS_RELEASE(shell);
      return;
    } 
  } else {
    startEndInSameContent = PR_FALSE;
  }

  if (startEndInSameContent || doc->IsInRange(startContent, endContent, mContent)) {
    nsIFontMetrics * fm = aRenderingContext.GetFontMetrics();

    PRInt32 startOffset = startPnt->GetOffset() - mContentOffset;
    PRInt32 endOffset   = endPnt->GetOffset()   - mContentOffset;

    nsRect rect;
    GetRect(rect);
    rect.x = 0;     // HACK!!!: Not sure why x and y value are sometime garbage -- gpk
    rect.y = 0;
    rect.width--;
    rect.height--;

#ifdef DO_SELECTION
    {
      char buf[255];
      strncpy(buf, compressedStr, compressedStrLen);
      buf[compressedStrLen] = 0;
      printf("2 - Drawing [%s] at [%d,%d,%d,%d]\n", buf, rect.x, rect.y, rect.width, rect.height);
      /*if (strstr(buf, "Example")) {
        int xx = 0;
        xx++;
      }
      if (strstr(buf, "basic4")) {
        int xx = 0;
        xx++;
      }*/
      if (strstr(buf, "paragraph")) {
        int xx = 0;
        xx++;
      }
      if (strstr(buf, "two.")) {
        int xx = 0;
        xx++;
      }
    }
#endif

    if (startEndInSameContent) {

      // Assume here the offsets are different 
      // because we have already taken care of case where they are

      // Render the text Normal
      const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
      aRenderingContext.SetColor(color->mColor);

      //---------------------------------------------------
      // Calc the starting point of the Selection and
      // and draw from the beginning to where the selection starts
      //---------------------------------------------------
      nsString textStr;
      nscoord  startTwipLen = 0;
      nscoord  endCoord; 
      nscoord  startCoord;

      PRBool skip = PR_FALSE;
      startCoord = 0;
      if (startPnt->GetOffset() > mContentOffset+mContentLength ||
        (startPnt->GetOffset() == mContentOffset+mContentLength)) {// && mContentOffset == 0)) {
        skip = PR_TRUE;
      } else if (startPnt->GetOffset() <= mContentOffset) {
        //startCoord = 0;
      } else {
        startCoord = indexes[startPnt->GetOffset()-mContentOffset]-mContentOffset;

        /*PRUint32 i;
        for (i=0;i<mContentLength;i++) {
          if (indexes[i] == startPnt->GetOffset()) {
            startCoord = i;
            break;
          }
        }*/
      }

      endCoord = mContentLength;
      if (endPnt->GetOffset() >= mContentOffset+mContentLength) {
        //endCoord = mContentLength;
      } else if (endPnt->GetOffset() <= mContentOffset) {
        //endCoord = mContentLength;
        skip = PR_TRUE;
      } else {
        //if (endPnt->GetOffset() == (PRInt32)compressedStrLen) {
         // endCoord = compressedStrLen;
        //} else {
          endCoord = indexes[endPnt->GetOffset()-mContentOffset]-mContentOffset;
        //}
      }
 

      PRUint32 selTextCharLen = endCoord - startCoord;

      if (startCoord > 0) {
        textStr.Append(compressedStr, startCoord);
        fm->GetWidth(deviceContext, textStr, startTwipLen);
        aRenderingContext.DrawString(compressedStr, startCoord, dx, dy, startTwipLen);
      }
      //---------------------------------------------------
      // Calc the starting point of the Selection and
      // and draw the selected text 
      //---------------------------------------------------
      textStr.SetLength(0);
      textStr.Append(compressedStr+startCoord, selTextCharLen);

      nscoord selTextTwipLen;
      fm->GetWidth(deviceContext, textStr, selTextTwipLen);

      rect.x     = startTwipLen;
      rect.width = selTextTwipLen;

      if (!skip) {
        // Render Selected Text
        aRenderingContext.SetColor(NS_RGB(0,0,0));
        aRenderingContext.FillRect(rect);

        // Render the text in White
        aRenderingContext.SetColor(NS_RGB(255,255,255));
      }
      aRenderingContext.DrawString(compressedStr+startCoord, selTextCharLen, 
                                  dx+startTwipLen, dy, selTextTwipLen);

      //---------------------------------------------------
      // Calc the end point of the Selection and
      // and draw the remaining text 
      //---------------------------------------------------
      if (endCoord < (PRInt32)compressedStrLen && !skip) {
        textStr.SetLength(0);
        textStr.Append(compressedStr+endCoord, compressedStrLen-selTextCharLen);

        nscoord endTextTwipLen;
        fm->GetWidth(deviceContext, textStr, endTextTwipLen);

        aRenderingContext.SetColor(color->mColor);
        aRenderingContext.DrawString(compressedStr+endCoord, compressedStrLen-endCoord, 
                                     dx+startTwipLen+selTextTwipLen, dy, mRect.width-endTextTwipLen);
      }

    } else if (mContent == startContent) {

      if (startPnt->GetOffset() >= mContentOffset && 
          startPnt->GetOffset() < mContentOffset+mContentLength) {
        // Render the text Normal
        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);

        PRInt32 startOffset = 0;
        startOffset = indexes[startPnt->GetOffset()-mContentOffset]-mContentOffset;

        //---------------------------------------------------
        // Calc the starting point of the Selection and
        // and draw from the beginning to where the selection starts
        //---------------------------------------------------
        nsString textStr;
        nscoord  startTwipLen   = 0;
        PRUint32 selTextCharLen = compressedStrLen-startOffset;

        if (startOffset > 0) {
          textStr.Append(compressedStr, startOffset);
          fm->GetWidth(deviceContext, textStr, startTwipLen);
          aRenderingContext.DrawString(compressedStr, startOffset, dx, dy, startTwipLen);
        }

        //---------------------------------------------------
        // Calc the starting point of the Selection and
        // and draw the selected text 
        //---------------------------------------------------
        textStr.SetLength(0);
        textStr.Append(compressedStr+startOffset, selTextCharLen);

        nscoord selTextTwipLen;
        fm->GetWidth(deviceContext, textStr, selTextTwipLen);

        rect.x     = startTwipLen;
        rect.width = selTextTwipLen;

        // Render Selected Text
        aRenderingContext.SetColor(NS_RGB(0,0,0));
        aRenderingContext.FillRect(rect);

        aRenderingContext.SetColor(NS_RGB(255,255,255));
        // Render the text in White
        aRenderingContext.DrawString(compressedStr+startOffset, selTextCharLen, 
                                    dx+startTwipLen, dy, selTextTwipLen);
      } else {

        if (startPnt->GetOffset() < mContentOffset) {
          aRenderingContext.SetColor(NS_RGB(0,0,0));
          aRenderingContext.FillRect(rect);

          aRenderingContext.SetColor(NS_RGB(255,255,255));
        } else {
          const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
          aRenderingContext.SetColor(color->mColor);
        }
        // Render the text in White
        aRenderingContext.DrawString(compressedStr, compressedStrLen, dx, dy, mRect.width);

      }
    } else if (mContent == endContent) {

      if (endPnt->GetOffset() > mContentOffset && 
          endPnt->GetOffset() <= mContentOffset+mContentLength) {

        if (endPnt->GetOffset() == mContentOffset+mContentLength) {
          endOffset = compressedStrLen;
        } else {
          if (endPnt->GetOffset() == (PRInt32)compressedStrLen) {
            endOffset = compressedStrLen;
          } else {
            endOffset = indexes[endPnt->GetOffset()-mContentOffset]-mContentOffset;
          }
        }

        //---------------------------------------------------
        // Calc the starting point of the Selection and
        // and draw from the beginning to where the selection starts
        //---------------------------------------------------
        nsString textStr;
        PRUint32 selTextCharLen = endOffset;

        //---------------------------------------------------
        // Calc the starting point of the Selection and
        // and draw the selected text 
        //---------------------------------------------------
        textStr.SetLength(0);
        textStr.Append(compressedStr, selTextCharLen);

        nscoord selTextTwipLen;
        fm->GetWidth(deviceContext, textStr, selTextTwipLen);

        rect.width = selTextTwipLen;

        // Render Selected Text
        aRenderingContext.SetColor(NS_RGB(0,0,0));
        aRenderingContext.FillRect(rect);

        aRenderingContext.SetColor(NS_RGB(255,255,255));
        // Render the text in White
        aRenderingContext.DrawString(compressedStr, selTextCharLen,  dx, dy, selTextTwipLen);

        //---------------------------------------------------
        // Calc the end point of the Selection and
        // and draw the remaining text 
        //---------------------------------------------------
        textStr.SetLength(0);
        textStr.Append(compressedStr+endOffset, compressedStrLen-selTextCharLen);

        nscoord endTextTwipLen;
        fm->GetWidth(deviceContext, textStr, endTextTwipLen);

        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);
        aRenderingContext.DrawString(compressedStr+endOffset, compressedStrLen-endOffset, 
                                     dx+selTextTwipLen, dy, mRect.width-endTextTwipLen);
      } else {

        if (endPnt->GetOffset() > mContentOffset+mContentLength) {
          aRenderingContext.SetColor(NS_RGB(0,0,0));
          aRenderingContext.FillRect(rect);

          aRenderingContext.SetColor(NS_RGB(255,255,255));
        } else {
          const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
          aRenderingContext.SetColor(color->mColor);
        }
        aRenderingContext.DrawString(compressedStr, compressedStrLen, dx, dy, mRect.width);
      }
    
    } else {
      aRenderingContext.SetColor(NS_RGB(0,0,0));
      aRenderingContext.FillRect(rect);

      aRenderingContext.SetColor(NS_RGB(255,255,255));
      // Render the text
      aRenderingContext.DrawString(compressedStr, compressedStrLen, dx, dy, mRect.width);

      //const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
      //aRenderingContext.SetColor(color->mColor);
    }

    NS_RELEASE(fm);
  } else {

    // Render the text
    const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(color->mColor);
    aRenderingContext.DrawString(compressedStr, compressedStrLen, dx, dy, mRect.width);
  }

  if (delCompressedStr) {
    delete compressedStr;
  }
  //printf("startContent %d\n", startContent?startContent->Release():-1);
  //printf("endContent %d\n", endContent?endContent->Release():-1);
  NS_IF_RELEASE(deviceContext);
  NS_IF_RELEASE(startContent);
  NS_IF_RELEASE(endContent);
  NS_RELEASE(shell);
  NS_RELEASE(doc);
  NS_RELEASE(selection);
}

NS_IMETHODIMP
TextFrame::FindTextRuns(nsCSSLineLayout&  aLineLayout,
                        nsIReflowCommand* aReflowCommand)
{
  if (nsnull == mPrevInFlow) {
    aLineLayout.AddText(this);
  }
  return NS_OK;
}

NS_IMETHODIMP
TextFrame::InlineReflow(nsCSSLineLayout&     aLineLayout,
                        nsReflowMetrics&     aMetrics,
                        const nsReflowState& aReflowState)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter TextFrame::Reflow: aMaxSize=%d,%d",
      aReflowState.maxSize.width, aReflowState.maxSize.height));

  // Get starting offset into the content
  PRInt32 startingOffset = 0;
  if (nsnull != mPrevInFlow) {
    TextFrame* prev = (TextFrame*) mPrevInFlow;
    startingOffset = prev->mContentOffset + prev->mContentLength;
  }

  const nsStyleFont* font =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  // Initialize mFlags (without destroying the TEXT_BLINK_ON bit) bits
  // that are filled in by the reflow routines.
  mFlags &= TEXT_BLINK_ON;
  if (font->mFont.decorations & NS_STYLE_TEXT_DECORATION_BLINK) {
    if (0 == (mFlags & TEXT_BLINK_ON)) {
      mFlags |= TEXT_BLINK_ON;
      gTextBlinker->AddFrame(this);
    }
  }

  const nsStyleText* text =
    (const nsStyleText*)mStyleContext->GetStyleData(eStyleStruct_Text);

  nsInlineReflowStatus rs;
  if (NS_STYLE_WHITESPACE_PRE == text->mWhiteSpace) {
    // Use a specialized routine for pre-formatted text
    rs = ReflowPre(aLineLayout, aMetrics, aReflowState,
                   *font, startingOffset);
  } else {
    // Use normal wrapping routine for non-pre text (this includes
    // text that is not wrapping)
    rs = ReflowNormal(aLineLayout, aMetrics, aReflowState,
                      *font, *text, startingOffset);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit TextFrame::Reflow: rv=%x width=%d",
      rs, aMetrics.width));
  return rs;
}

// Reflow normal text (stuff that doesn't have to deal with horizontal
// tabs). Normal text reflow may or may not wrap depending on the
// "whiteSpace" style property.
nsInlineReflowStatus
TextFrame::ReflowNormal(nsCSSLineLayout& aLineLayout,
                        nsReflowMetrics& aMetrics,
                        const nsReflowState& aReflowState,
                        const nsStyleFont& aFont,
                        const nsStyleText& aTextStyle,
                        PRInt32 aStartingOffset)
{
  PRInt32 textLength;
  const PRUnichar* cp = GetText(mContent, textLength);
  cp += aStartingOffset;
  const PRUnichar* end = cp + textLength - aStartingOffset;
  const PRUnichar* cpStart = cp;
  mContentOffset = aStartingOffset;

  nsIFontMetrics* fm = aLineLayout.mPresContext->GetMetricsFor(aFont.mFont);
  PRInt32 spaceWidth;
  fm->GetWidth(' ', spaceWidth);
  PRBool wrapping = PR_TRUE;
  if (NS_STYLE_WHITESPACE_NORMAL != aTextStyle.mWhiteSpace) {
    wrapping = PR_FALSE;
  }

  // Set whitespace skip flag
  PRBool skipWhitespace = PR_FALSE;
  if (aLineLayout.GetSkipLeadingWhiteSpace()) {
    skipWhitespace = PR_TRUE;
    mFlags |= TEXT_SKIP_LEADING_WS;
  }

  // Try to fit as much of the text as possible. Note that if we are
  // at the left margin then the first word always fits. In addition,
  // we compute the size of the largest word that we contain. If we
  // end up containing nothing (because there isn't enough space for
  // the first word) then we still compute the size of that first
  // non-fitting word.

  // XXX XP_IS_SPACE must not return true for the unicode &nbsp character
  // XXX what about &zwj and it's cousins?
  nscoord x = 0;
  nscoord maxWidth = aReflowState.maxSize.width;
  nscoord maxWordWidth = 0;
  const PRUnichar* lastWordEnd = cpStart;
//  const PRUnichar* lastWordStart = cpStart;
  PRBool hasMultibyte = PR_FALSE;
  PRBool endsInWhitespace = PR_FALSE;

  while (cp < end) {
    PRUnichar ch = *cp++;
    PRBool isWhitespace;
    nscoord width;
    if (XP_IS_SPACE(ch)) {
      // Compress whitespace down to a single whitespace
      while (cp < end) {
        ch = *cp;
        if (XP_IS_SPACE(ch)) {
          cp++;
          continue;
        }
        break;
      }
      if (skipWhitespace) {
#if XXX_fix_me
        aLineLayout->AtSpace();
#endif
        skipWhitespace = PR_FALSE;
        continue;
      }
      width = spaceWidth;
      isWhitespace = PR_TRUE;
    } else {
      // The character is not a space character. Find the end of the
      // word and then measure it.
      if (ch >= 256) {
        hasMultibyte = PR_TRUE;
      }
      const PRUnichar* wordStart = cp - 1;
//      lastWordStart = wordStart;
      while (cp < end) {
        ch = *cp;
        if (ch >= 256) {
          hasMultibyte = PR_TRUE;
        }
        if (!XP_IS_SPACE(ch)) {
          cp++;
          continue;
        }
        break;
      }
      fm->GetWidth(wordStart, PRUint32(cp - wordStart), width);
      skipWhitespace = PR_FALSE;
      isWhitespace = PR_FALSE;
    }

    // Now that we have the end of the word or whitespace, see if it
    // will fit.
    if ((0 != x) && wrapping && (x + width > maxWidth)) {
      // The word/whitespace will not fit.
      cp = lastWordEnd;
      break;
    }

#if XXX_fix_me
    // Update break state in line reflow state
    // XXX move this out of the loop!
    if (isWhitespace) {
      aLineLayout.AtSpace();
    }
    else {
      aLineLayout.AtWordStart(this, x);
    }
#endif

    // The word fits. Add it to the run of text.
    x += width;
    if (width > maxWordWidth) {
      maxWordWidth = width;
    }
    lastWordEnd = cp;
    endsInWhitespace = isWhitespace;
  }

  if (hasMultibyte) {
    mFlags |= TEXT_HAS_MULTIBYTE;
  }
  if (endsInWhitespace) {
    mFlags |= TEXT_ENDS_IN_WHITESPACE;
  }

  if (0 == x) {
    // Since we collapsed into nothingness (all our whitespace
    // is ignored) leave the aState->mSkipLeadingWhiteSpace
    // flag alone since it doesn't want leading whitespace
  }
  else {
    aLineLayout.SetSkipLeadingWhiteSpace(endsInWhitespace);
  }

  // XXX too much code here: some of it isn't needed
  // Now we know our content length
  mContentLength = lastWordEnd - cpStart;
  if (0 == mContentLength) {
    if (cp == end) {
      // The entire chunk of text was whitespace that we skipped over.
      aMetrics.width = 0;
      aMetrics.height = 0;
      aMetrics.ascent = 0;
      aMetrics.descent = 0;
      mContentLength = end - cpStart;
      if (nsnull != aMetrics.maxElementSize) {
        aMetrics.maxElementSize->width = 0;
        aMetrics.maxElementSize->height = 0;
      }
      NS_RELEASE(fm);
      return NS_FRAME_COMPLETE;
    }
  }

  // Set desired size to the computed size
  aMetrics.width = x;
  fm->GetHeight(aMetrics.height);
  fm->GetMaxAscent(aMetrics.ascent);
  fm->GetMaxDescent(aMetrics.descent);
  if (!wrapping) {
    maxWordWidth = x;
  }
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = maxWordWidth;
    fm->GetHeight(aMetrics.maxElementSize->height);
  }
  NS_RELEASE(fm);
  return (cp == end) ? NS_FRAME_COMPLETE : NS_FRAME_NOT_COMPLETE;
}

nsInlineReflowStatus
TextFrame::ReflowPre(nsCSSLineLayout& aLineLayout,
                     nsReflowMetrics& aMetrics,
                     const nsReflowState& aReflowState,
                     const nsStyleFont& aFont,
                     PRInt32 aStartingOffset)
{
  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;

  PRInt32 textLength;
  const PRUnichar* cp = GetText(mContent, textLength);
  cp += aStartingOffset;
  const PRUnichar* cpStart = cp;
  const PRUnichar* end = cp + textLength - aStartingOffset;

  mFlags |= TEXT_IS_PRE;
  nsIFontMetrics* fm = aLineLayout.mPresContext->GetMetricsFor(aFont.mFont);
  const PRInt32* widths;
  fm->GetWidths(widths);
  PRInt32 width = 0;
  PRBool hasMultibyte = PR_FALSE;
  PRUint16 tabs = 0;
  PRUint16 col = aLineLayout.GetColumn();
  mColumn = col;
  nscoord spaceWidth = widths[' '];

  while (cp < end) {
    PRUnichar ch = *cp++;
    if (ch == '\n') {
      rs = (cp == end)
        ? NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_COMPLETE)
        : NS_INLINE_LINE_BREAK_AFTER(NS_FRAME_NOT_COMPLETE);
      break;
    }
    if (ch == '\t') {
      // Advance to next tab stop
      PRIntn spaces = 8 - (col & 7);
      width += spaces * spaceWidth;
      col += spaces;
      tabs++;
      continue;
    }
    if (ch == CH_NBSP) {
      width += spaceWidth;
      col++;
      continue;
    }
    if (ch < 256) {
      width += widths[ch];
    } else {
      nscoord charWidth;
      fm->GetWidth(ch, charWidth);
      widths += charWidth;
      hasMultibyte = PR_TRUE;
    }
    col++;
  }
  aLineLayout.SetColumn(col);
  if (hasMultibyte) {
    mFlags |= TEXT_HAS_MULTIBYTE;
  }
  TEXT_SET_TAB_COUNT(mFlags, tabs);

  mContentOffset = aStartingOffset;
  mContentLength = cp - cpStart;
  aMetrics.width = width;
  fm->GetHeight(aMetrics.height);
  fm->GetMaxAscent(aMetrics.ascent);
  fm->GetMaxDescent(aMetrics.descent);
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  NS_RELEASE(fm);

  return rs;
}

//--------------------------------------------------------------------------
// CalcActualPosition
// This finds the actual interger offset into the text and the width of the
// offset in Twips 
//--------------------------------------------------------------------------
void TextFrame::CalcActualPosition(PRUint32         &aMsgType,
                                   const PRUnichar* aCPStart, 
                                   const PRUnichar* aCP, 
                                   PRInt32 &        aOffset, 
                                   PRInt32 &        aWidth,
                                   nsIFontMetrics * aFM) {

  //printf("**STOPPED! Offset is [%d][%d]\n", width, PRUint32(cp - cpStart));

  if (aMsgType == NS_MOUSE_LEFT_BUTTON_UP || aMsgType == NS_MOUSE_MOVE) {
    aOffset = PRUint32(aCP - aCPStart);
    if (0 == aOffset) {
      aWidth = 0;
    }
    else {
      aFM->GetWidth(aCPStart, aOffset-1, aWidth);
    }
  } else if (aMsgType == NS_MOUSE_LEFT_BUTTON_DOWN) {
    aOffset = PRUint32(aCP - aCPStart)-1;
    if (aOffset == 0) {
      aWidth = 0;
    } else {
      aWidth = aFM->GetWidth(aCPStart, aOffset);
    }
  }
}

//--------------------------------------------------------------------------
//-- GetPosition
//--------------------------------------------------------------------------
PRInt32 TextFrame::GetPosition(nsIPresContext& aCX,
                               nsGUIEvent *    aEvent,
                               nsIFrame *      aNewFrame,
                               PRUint32&       aAcutalContentOffset) {

  PRInt32 offset; 
  PRInt32 width;
  CalcCursorPosition(aCX, aEvent, (TextFrame *)aNewFrame, offset, width);
  aAcutalContentOffset = ((TextFrame *)aNewFrame)->GetContentOffset();
  //offset += aAcutalContentOffset;
  return offset;
}


//--------------------------------------------------------------------------
//-- CalcCursorPosition
//--------------------------------------------------------------------------
void TextFrame::CalcCursorPosition(nsIPresContext& aCX,
                                   nsGUIEvent*     aEvent,
                                   TextFrame *     aNewFrame,
                                   PRInt32 &       aOffset, 
                                   PRInt32 &       aWidth) {

 
 
  nsIStyleContext * styleContext;

  aNewFrame->GetStyleContext(&aCX, styleContext);
  const nsStyleFont *font      = (const nsStyleFont*)styleContext->GetStyleData(eStyleStruct_Font);
  const nsStyleText *styleText = (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);
  NS_RELEASE(styleContext);

  nsIFontMetrics* fm   = aCX.GetMetricsFor(font->mFont);

  nscoord width        = 0;

  PRUint16        indexes[1024];
  PRUint32        compressedStrLen;
  PRBool          shouldDelete = PR_FALSE;
  char            buf[128];
  char *          compressedStr;

  compressedStr = CompressWhiteSpace(buf, sizeof(buf), indexes, compressedStrLen, shouldDelete);
  //compressedStr[compressedStrLen] = 0; // debug only

  PRInt32 i;
  char buffer[1024];
  for (i=1;i<PRInt32(compressedStrLen);i++) {
    strncpy(buffer, compressedStr, i);
	  buffer[i] = 0;
    fm->GetWidth(buffer, width);
    //printf("%s %d %d\n", buf, i, width);
    if (width >= aEvent->point.x) {
      if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
        i--;
        if (i < 0) {
          i = 0;
        }
      }
      aOffset = 0;
      PRInt32 j;
      for (j=0;j<PRInt32(mContentLength);j++) {
        if (indexes[j] == i+mContentOffset) {
          aOffset = j+mContentOffset;
          break;
          //printf("Char pos %d  Offset is %d\n", i, aOffset);
        }
      }
	    //aOffset = indexes[i];
	    aWidth  = width;
      aWidth = 0;
	    return;
    }
  }

  aOffset = mContentOffset+mContentLength;
  aWidth  = width;

  if (shouldDelete) {
    delete compressedStr;
  }

}

// XXX there is a copy of this in nsGenericDomDataNode.cpp
static void
ToCString(nsString& aBuf, const PRUnichar* cp, PRInt32 aLen)
{
  const PRUnichar* end = cp + aLen;
  while (cp < end) {
    PRUnichar ch = *cp++;
    if (ch == '\r') {
      aBuf.Append("\\r");
    } else if (ch == '\n') {
      aBuf.Append("\\n");
    } else if (ch == '\t') {
      aBuf.Append("\\t");
    } else if ((ch < ' ') || (ch >= 127)) {
      aBuf.Append("\\0");
      aBuf.Append((PRInt32)ch, 8);
    } else {
      aBuf.Append(ch);
    }
  }
}

NS_IMETHODIMP
TextFrame::ListTag(FILE* out) const
{
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, "Text(%d)@%p", contentIndex, this);
  return NS_OK;
}

NS_IMETHODIMP
TextFrame::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  ListTag(out);
  nsIView* view;
  GetView(view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
  }

  // Output the first/last content offset and prev/next in flow info
  // XXX inefficient (especially for really large strings)
  PRInt32 textLength;
  const PRUnichar* cp = GetText(mContent, textLength);

  PRBool isComplete = (mContentOffset + mContentLength) == textLength;
  fprintf(out, "[%d,%d,%c] ", 
          mContentOffset, mContentOffset+mContentLength-1,
          isComplete ? 'T':'F');
  if (nsnull != mPrevInFlow) {
    fprintf(out, "prev-in-flow=%p ", mPrevInFlow);
  }
  if (nsnull != mNextInFlow) {
    fprintf(out, "next-in-flow=%p ", mNextInFlow);
  }

  // Output the rect and state
  out << mRect;
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }

  // Output the text
  fputs("<\n", out);
  aIndent++;

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs("\"", out);
  nsAutoString tmp;
  ToCString(tmp, cp, mContentLength);
  fputs(tmp, out);
  fputs("\"\n", out);

  aIndent--;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);

#ifdef NOISY
  if (nsnull != mWords) {
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs("  @words ", out);
    PRUint32* wp = mWords;
    PRInt32 total = mContentLength;
    while (total != 0) {
      PRUint32 wordInfo = *wp++;
      nscoord wordWidth = GET_WORD_WIDTH(wordInfo);
      PRIntn wordLen = GET_WORD_LENGTH(wordInfo);
      fprintf(out, "%c-%d-%d ",
              ((wordInfo & WORD_IS_WORD) ? 'w' : 's'),
              wordWidth, wordLen);
      total -= wordLen;
      NS_ASSERTION(total >= 0, "bad word data");
    }
    fputs("\n", out);
  }
#endif
  return NS_OK;
}

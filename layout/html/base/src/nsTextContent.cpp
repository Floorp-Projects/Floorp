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

// Selection includes
#include "nsISelection.h"
#include "nsSelectionRange.h"

#define gCalcDebug 0

PRBool IsInRange(nsIContent * aStart, nsIContent * aEnd, nsIContent * aContent);


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

#if 0
static NS_DEFINE_IID(kITextContentIID, NS_ITEXTCONTENT_IID);
#endif

static NS_DEFINE_IID(kIScriptObjectOwner, NS_ISCRIPTOBJECTOWNER_IID);

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

class TextFrame : public nsSplittableFrame, nsIInlineReflow {
public:
  TextFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  // nsISupports
  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  // nsIFrame
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  NS_IMETHOD GetCursorAt(nsIPresContext& aPresContext,
                         const nsPoint& aPoint,
                         nsIFrame** aFrame,
                         PRInt32& aCursor);
  NS_IMETHOD ContentChanged(nsIPresShell*   aShell,
                            nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD ListTag(FILE* out) const;

  // nsIInlineReflow
  NS_IMETHOD FindTextRuns(nsCSSLineLayout& aLineLayout);
  NS_IMETHOD InlineReflow(nsCSSLineLayout&     aLineLayout,
                          nsReflowMetrics&     aMetrics,
                          const nsReflowState& aReflowState);

  // TextFrame methods
  PRInt32 GetPosition(nsIPresContext& aCX,
                      nsGUIEvent*     aEvent,
                      nsIFrame *      aNewFrame);

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

  virtual void List(FILE* out, PRInt32 aIndent) const;

  virtual void ToHTMLString(nsString& aBuf) const;

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);

  // nsIDOMText interface
  NS_IMETHOD GetNodeType(PRInt32 *aType);
  NS_IMETHOD GetParentNode(nsIDOMNode **aNode);
  NS_IMETHOD GetChildNodes(nsIDOMNodeIterator **aIterator);
  NS_IMETHOD HasChildNodes(PRBool *aReturn);
  NS_IMETHOD GetFirstChild(nsIDOMNode **aNode);
  NS_IMETHOD GetPreviousSibling(nsIDOMNode **aNode);
  NS_IMETHOD GetNextSibling(nsIDOMNode **aNode);
  NS_IMETHOD InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild);
  NS_IMETHOD ReplaceChild(nsIDOMNode *newChild, 
                          nsIDOMNode *oldChild);
  NS_IMETHOD RemoveChild(nsIDOMNode *oldChild);
  NS_IMETHOD GetData(nsString& aString);
  NS_IMETHOD SetData(nsString& aString);
  NS_IMETHOD Append(nsString& aData);
  NS_IMETHOD Insert(PRInt32 offset, nsString& aData);
  NS_IMETHOD Delete(PRInt32 offset, PRInt32 count);
  NS_IMETHOD Replace(PRInt32 offset, PRInt32 count, nsString& aData);
  NS_IMETHOD Splice(nsIDOMElement *element, PRInt32 offset, PRInt32 count);

  void ToCString(nsString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  PRUnichar* mText;
  PRInt32 mLength;

protected:
  Text();
  virtual ~Text();
};

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
  NS_IF_RELEASE(mTimer);
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
    nsIViewManager* vm = view->GetViewManager();
    bounds.x = offset.x;
    bounds.y = offset.y;
    vm->UpdateView(view, bounds, 0);
    NS_RELEASE(vm);
    NS_RELEASE(view);
  }
}

//----------------------------------------------------------------------

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

NS_METHOD TextFrame::GetCursorAt(nsIPresContext& aPresContext,
                                 const nsPoint& aPoint,
                                 nsIFrame** aFrame,
                                 PRInt32& aCursor)
{
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
      nscoord onePixel = nscoord(1.0f * aPresContext.GetPixelsToTwips());
      aRenderingContext.SetColor(color->mBackgroundColor);
      PaintRegularText(aPresContext, aRenderingContext, aDirtyRect, onePixel, onePixel);
    }
#endif
  }

  return NS_OK;
}

void
TextFrame::PaintRegularText(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nscoord dx, nscoord dy)
{
  // Skip leading space if necessary
  Text* txt = (Text*) mContent;
  const PRUnichar* cp = txt->mText + mContentOffset;
  const PRUnichar* end = cp + mContentLength;
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

      // Get Selection Object
      nsIPresShell     * shell     = aPresContext.GetShell();
      nsIDocument      * doc       = shell->GetDocument();
      nsISelection     * selection = doc->GetSelection();
      nsSelectionRange * range     = selection->GetRange();

      NS_RELEASE(shell);
      NS_RELEASE(doc);
      NS_RELEASE(selection);


      if (IsInRange(range->GetStartContent(), range->GetEndContent(), mContent)) {
        nsRect rect;
        GetRect(rect);
        rect.width--;
        rect.height--;

        nsIFontMetrics * fm = aRenderingContext.GetFontMetrics();

        aRenderingContext.SetColor(NS_RGB(0,0,0));
        aRenderingContext.FillRect(rect);

        aRenderingContext.SetColor(NS_RGB(255,255,255));

        // Render the text
        aRenderingContext.DrawString(s0, s - s0, dx, dy, mRect.width);
        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);
      } else {
        // Render the text
        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);
        aRenderingContext.DrawString(s0, s - s0, dx, dy, mRect.width);
      }

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

      // Get Selection Object
      nsIPresShell     * shell     = aPresContext.GetShell();
      nsIDocument      * doc       = shell->GetDocument();
      nsISelection     * selection = doc->GetSelection();
      nsSelectionRange * range     = selection->GetRange();

      NS_RELEASE(shell);
      NS_RELEASE(doc);
      NS_RELEASE(selection);

      if (IsInRange(range->GetStartContent(), range->GetEndContent(), mContent)) {
        nsRect rect;
        GetRect(rect);
        rect.width--;
        rect.height--;

        nsIFontMetrics * fm = aRenderingContext.GetFontMetrics();

        aRenderingContext.SetColor(NS_RGB(0,0,0));
        aRenderingContext.FillRect(rect);

        aRenderingContext.SetColor(NS_RGB(255,255,255));

        // Render the text
        aRenderingContext.DrawString(s0, s - s0, dx, dy, mRect.width);
        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);
      } else {
        // Render the text
        const nsStyleColor* color = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        aRenderingContext.SetColor(color->mColor);
        aRenderingContext.DrawString(s0, s - s0, dx, dy, mRect.width);
      }

      if (s0 != buf) {
        delete[] s0;
      }
    }
  }
}

NS_IMETHODIMP
TextFrame::FindTextRuns(nsCSSLineLayout& aLineLayout)
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
  Text* txt = (Text*) mContent;
  const PRUnichar* cp = txt->mText + aStartingOffset;
  const PRUnichar* end = cp + txt->mLength - aStartingOffset;
  const PRUnichar* cpStart = cp;
  mContentOffset = aStartingOffset;

  nsIFontMetrics* fm = aLineLayout.mPresContext->GetMetricsFor(aFont.mFont);
  PRInt32 spaceWidth = fm->GetWidth(' ');
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
  const PRUnichar* lastWordStart = cpStart;
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
      lastWordStart = wordStart;
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
      width = fm->GetWidth(wordStart, PRUint32(cp - wordStart));
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
      return NS_INLINE_REFLOW_COMPLETE;
    }
  }

  // Set desired size to the computed size
  aMetrics.width = x;
  aMetrics.height = fm->GetHeight();
  aMetrics.ascent = fm->GetMaxAscent();
  aMetrics.descent = fm->GetMaxDescent();
  if (!wrapping) {
    maxWordWidth = x;
  }
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = maxWordWidth;
    aMetrics.maxElementSize->height = fm->GetHeight();
  }
  NS_RELEASE(fm);
  return (cp == end) ? NS_INLINE_REFLOW_COMPLETE : NS_INLINE_REFLOW_NOT_COMPLETE;
}

nsInlineReflowStatus
TextFrame::ReflowPre(nsCSSLineLayout& aLineLayout,
                     nsReflowMetrics& aMetrics,
                     const nsReflowState& aReflowState,
                     const nsStyleFont& aFont,
                     PRInt32 aStartingOffset)
{
  nsInlineReflowStatus rs = NS_INLINE_REFLOW_COMPLETE;

  Text* txt = (Text*) mContent;
  const PRUnichar* cp = txt->mText + aStartingOffset;
  const PRUnichar* cpStart = cp;
  const PRUnichar* end = cp + txt->mLength - aStartingOffset;

  mFlags |= TEXT_IS_PRE;
  nsIFontMetrics* fm = aLineLayout.mPresContext->GetMetricsFor(aFont.mFont);
  const PRInt32* widths = fm->GetWidths();
  PRInt32 width = 0;
  PRBool hasMultibyte = PR_FALSE;
  PRUint16 tabs = 0;
  PRUint16 col = aLineLayout.GetColumn();
  mColumn = col;
  nscoord spaceWidth = widths[' '];

  while (cp < end) {
    PRUnichar ch = *cp++;
    if (ch == '\n') {
      rs = NS_INLINE_REFLOW_LINE_BREAK_AFTER;
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
      widths += fm->GetWidth(ch);
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
  aMetrics.height = fm->GetHeight();
  aMetrics.ascent = fm->GetMaxAscent();
  aMetrics.descent = fm->GetMaxDescent();
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
    aWidth  = aFM->GetWidth(aCPStart, aOffset-1);
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
                               nsGUIEvent * aEvent,
                               nsIFrame * aNewFrame) {

  PRInt32 offset; 
  PRInt32 width;
  CalcCursorPosition(aCX, aEvent, (TextFrame *)aNewFrame, offset, width);
  offset += ((TextFrame *)aNewFrame)->GetContentOffset();

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

 
  if (gCalcDebug) printf("Cursor Point [%d,%d]\n", aEvent->point.x, aEvent->point.y);

  // Get starting offset into the content
  PRInt32 startingOffset = 0;
  if (nsnull != aNewFrame->GetPrevInFlow()) {
    TextFrame* prev = (TextFrame*) aNewFrame->GetPrevInFlow();
    startingOffset = prev->GetContentOffset() + prev->GetContentLength();
  }

//XXX  // Get cached state for containing block frame
//  if (nsnull != aLineLayout) {
//    aLineLayout->mReflowResult = NS_LINE_LAYOUT_REFLOW_RESULT_AWARE;
//  }

  nsIContent * content;
  GetContent(content);

  Text* txt = (Text*) content;

  const PRUnichar* cp = txt->mText + startingOffset;
  const PRUnichar* end = cp + txt->mLength - startingOffset;
  const PRUnichar* cpStart = cp;
  //mContentOffset = startingOffset;

  nsIStyleContext * styleContext;

  aNewFrame->GetStyleContext(&aCX, styleContext);
  const nsStyleFont *font      = (const nsStyleFont*)styleContext->GetStyleData(eStyleStruct_Font);
  const nsStyleText *styleText = (const nsStyleText*)styleContext->GetStyleData(eStyleStruct_Text);
  NS_RELEASE(styleContext);

  nsIFontMetrics* fm   = aCX.GetMetricsFor(font->mFont);
  PRInt32 spaceWidth   = fm->GetWidth(' ');
  PRBool  atLeftMargin = PR_TRUE;
  PRBool  wrapping     = PR_TRUE;
  if (NS_STYLE_WHITESPACE_NORMAL != styleText->mWhiteSpace) {
    wrapping = PR_FALSE;
  }

  // Set whitespace skip flag
  PRBool skipWhitespace = PR_FALSE;
#if XXX_wont_work
  if (nsnull != aLineLayout) {
    if (aLineLayout->SkipLeadingWhiteSpace()) {
      skipWhitespace = PR_TRUE;
      mFlags |= TEXT_SKIP_LEADING_WS;
    }
  }
#endif

  // Try to fit as much of the text as possible. Note that if we are
  // at the left margin then the first word always fits. In addition,
  // we compute the size of the largest word that we contain. If we
  // end up containing nothing (because there isn't enough space for
  // the first word) then we still compute the size of that first
  // non-fitting word.

  // XXX XP_IS_SPACE must not return PR_TRUE for the unicode &nbsp character
  // XXX what about &zwj and it's cousins?
  nscoord x            = 0;
  nscoord maxWidth     = 100000;//aMaxSize.width;
  nscoord width        = 0;
  nscoord maxWordWidth = 0;

  const PRUnichar* lastWordEnd = cpStart;

  PRBool hasMultibyte     = PR_FALSE;
  PRBool endsInWhitespace = PR_FALSE;

  int w = 0;
  while (cp < end) {
    width = fm->GetWidth(cpStart, PRUint32(cp - cpStart));

    if (gCalcDebug) printf("Cursor %d ******>> Width %d Pos: %d   end: %d \n", aEvent->point.x, 
                                                               width, 
                                                               PRUint32(cp - cpStart), 
                                                               end-cpStart);
    if (width >= aEvent->point.x) {
      if (gCalcDebug) printf("*********************************\n*STOPPED**! Offset is [%d][%d]\n", width, PRUint32(cp - cpStart));
      if (gCalcDebug) printf("**********************************\n");
      CalcActualPosition(aEvent->message, cpStart, cp, aOffset, aWidth, fm);

      //--------- Major Debugging ----
     /*if (aEvent->message == NS_MOUSE_MOVE ||
          aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
          int len = txt->mLength - startingOffset;

          //printf("Len %d\n", len);
          PRUnichar buf0[256];
          PRUnichar buf1[256];
          PRUnichar buf2[256];
          const PRUnichar* start = txt->mText + startingOffset;

          printf("mStart %d  mEnd %d  len %d\n", mStartSelect, mEndSelect, len);

          for (PRUint32 i=0;i<mStartSelect;i++) {
            printf("%c/", *start);
            buf0[i] = *start;
            start++;
          }
          if (mEndSelect-mStartSelect > 4) {
            int x = 0;
            x++;
          }
          printf("\n%d>", mEndSelect-mStartSelect);
          buf0[i] = 0;
          for (i=0;i<mEndSelect-mStartSelect;i++) {
            printf("%c/", *start);
            buf1[i] = *start;
            start++;
          }
          printf("\n%d>", len-mEndSelect);
          buf1[i] = 0;
          for (i=0;i<len-mEndSelect;i++) {
            printf("%c/", *start);
            buf2[i] = *start;
            start++;
          }
          printf("\n");
          buf2[i] = 0;
          //printf("[%s][%s][%s]\n", buf0, buf1, buf2);

          int l0 = fm->GetWidth(cpStart,              PRUint32(mStartSelect));
          int l1 = fm->GetWidth(cpStart+mStartSelect, PRUint32(mEndSelect-mStartSelect));
          int l2 = fm->GetWidth(cpStart+mEndSelect,   PRUint32(len-mEndSelect));
          int l3 = fm->GetWidth(cpStart,              PRUint32(mEndSelect));
          printf("%d  %d  %d/\n", mStartSelect, mEndSelect-mStartSelect, len-mEndSelect);
          printf("%d  %d  %d  %d  %d\n", l0, l1, l2, l3, (l0+l1+l2));

          printf("mStart %d,%d  mEnd %d,%d  whole width %d  len %d\n", mStartSelect, mStartWidth, mEndSelect, mEndWidth, fm->GetWidth(cpStart, PRUint32(txt->mLength-startingOffset)), PRUint32(txt->mLength-startingOffset));
          if (mEndSelect-mStartSelect > 0) {
            mSelectWidth = fm->GetWidth(cpStart+mStartSelect, PRUint32(mEndSelect-mStartSelect));
          } else {
            mSelectWidth = 0;
          }
      }*/
      //------

      //----
      NS_RELEASE(content);
      return; 
        }

    PRUnichar ch = *cp++;

    PRBool isWhitespace;
    if (XP_IS_SPACE(ch)) {
      if (gCalcDebug) printf("Before 11111111111111111111111111111111\n");
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
        skipWhitespace = PR_FALSE;
        continue;
      }
      width = spaceWidth;
      isWhitespace = PR_TRUE;
      if (gCalcDebug) printf("After 11111111111111111111111111111111\n");
    } else {
      if (gCalcDebug) printf("Before 2222222222222222222222222222222222222\n");
      // The character is not a space character. Find the end of the
      // word and then measure it.
      if (ch >= 256) {
        hasMultibyte = PR_TRUE;
      }
      const PRUnichar* wordStart = cp - 1; 
      while (cp < end) {
        width = fm->GetWidth(cpStart, PRUint32(cp - cpStart));

        if (gCalcDebug) printf("-Cursor %d ******>> Width %d Pos: %d   end: %d \n", aEvent->point.x, 
                                                                   width, 
                                                                   PRUint32(cp - cpStart), 
                                                                   end-cpStart);
        if (width >= aEvent->point.x) {
          if (gCalcDebug) printf("**********************************\nSTOPPED! Offset is [%d][%d]\n", width, PRUint32(cp - cpStart));
          if (gCalcDebug) printf("**********************************\n");
          CalcActualPosition(aEvent->message, cpStart, cp, aOffset, aWidth, fm);
          NS_RELEASE(content);
          return; 
        }

        ch = *cp;
        if (ch >= 256) {
          hasMultibyte = PR_TRUE;
        }
        if (!XP_IS_SPACE(ch)) {
          cp++;
          continue;
        }
        if (gCalcDebug) printf("At bottom...breaking..........................\n");
        break;
      }
      if (gCalcDebug) printf("After 2222222222222222222222222222222222222\n");
    }

    // Now that we have the end of the word or whitespace, see if it
    // will fit.
    if (!atLeftMargin && wrapping && (x + width > maxWidth)) {
      // The word/whitespace will not fit.
      cp = lastWordEnd;
      if (x == 0) {
        // Nothing fit at all. In this case, we still may want to know
        // the maxWordWidth so compute it right now.
        maxWordWidth = width;
      }
      if (gCalcDebug) printf(">>>>> Breaking\n");
      break;
    } // if

    // The word fits. Add it to the run of text.
    x += width;
    if (width > maxWordWidth) {
      maxWordWidth = width;
    }
    atLeftMargin = PR_FALSE;
    lastWordEnd  = cp;
    width        = 0;
    endsInWhitespace = isWhitespace;
    if (gCalcDebug) printf("Bottom--------------------------------\n");
  } // while

  NS_RELEASE(content);

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
    NS_RELEASE(view);
  }

  // Output the first/last content offset and prev/next in flow info
  PRBool isComplete =
    mContentOffset + mContentLength == ((Text*)mContent)->mLength;
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
  Text* txt = (Text*) mContent;
  nsAutoString tmp;
  txt->ToCString(tmp, mContentOffset, mContentLength);
  fputs("\"", out);
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

//----------------------------------------------------------------------
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);


Text::Text(const PRUnichar* aText, PRInt32 aLength)
{
  if (0 == aLength) {
    aLength = nsCRT::strlen(aText);
  }
  mLength = aLength;
  mText = new PRUnichar[aLength];
  nsCRT::memcpy(mText, aText, aLength * sizeof(PRUnichar));
}

Text::Text()
{
  mText = nsnull;
  mLength = 0;
}

Text::~Text()
{
  if (nsnull != mText) {
    delete[] mText;
    mText = nsnull;
  }
  mLength = 0;
}

nsresult Text::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(nsnull != aInstancePtr, "null pointer");
  
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIDOMTextIID)) {
    *aInstancePtr = (void*)(nsIDOMText*)this;
    nsHTMLContent::AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNodeIID)) {
    *aInstancePtr = (void*)(nsIDOMNode*)(nsIDOMText*)this;
    nsHTMLContent::AddRef();
    return NS_OK;
  }
  
  return nsHTMLContent::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt Text::AddRef(void)
{
  return nsHTMLContent::AddRef();
}

nsrefcnt Text::Release(void)
{
  return nsHTMLContent::Release();
}

void Text::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs("Text", out);
  ListAttributes(out);
  fprintf(out, " RefCnt=%d<\"", mRefCnt);

  nsAutoString tmp;
  ToCString(tmp, 0, mLength);
  fputs(tmp, out);

  fputs("\">\n", out);
}

void Text::ToHTMLString(nsString& aBuf) const
{
  aBuf.SetLength(0);
  aBuf.Append(mText, mLength);
}

void Text::ToCString(nsString& aBuf, PRInt32 aOffset, PRInt32 aLen) const
{
  PRUnichar* cp = mText + aOffset;
  PRUnichar* end = cp + aLen;
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

#if 0
// From nsITextContent; might want this later
PRInt32 Text::GetLength()
{
  return mLength;
}

void Text::GetText(nsString& aBuf, PRInt32 aOffset, PRInt32 aCount)
{
  aBuf.SetLength(0);
  if ((PRUint32(aOffset) >= PRUint32(mLength)) ||
      (aCount <= 0)) {
    return;
  }
  if (aOffset + aCount > mLength) {
    aCount = mLength - aOffset;
  }
  aBuf.Append(mText + aOffset, aCount);
}
#endif

nsresult
Text::CreateFrame(nsIPresContext*  aPresContext,
                  nsIFrame*        aParentFrame,
                  nsIStyleContext* aStyleContext,
                  nsIFrame*&       aResult)
{
  nsIFrame* frame = new TextFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

nsresult Text::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptText(aContext, this, mParent, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

//
// nsIDOMText interface
//
nsresult Text::GetNodeType(PRInt32 *aType)
{
  *aType = nsHTMLContent::TEXT;
  return NS_OK;
}

nsresult Text::GetParentNode(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetParentNode(aNode);
}

nsresult Text::GetChildNodes(nsIDOMNodeIterator **aIterator)
{
  return nsHTMLContent::GetChildNodes(aIterator);
}

nsresult Text::HasChildNodes(PRBool *aReturn)
{
  return nsHTMLContent::HasChildNodes(aReturn);
}

nsresult Text::GetFirstChild(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetFirstChild(aNode);
}

nsresult Text::GetPreviousSibling(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetPreviousSibling(aNode);
}

nsresult Text::GetNextSibling(nsIDOMNode **aNode)
{
  return nsHTMLContent::GetNextSibling(aNode);
}

nsresult Text::InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild)
{
  return nsHTMLContent::InsertBefore(newChild, refChild);
}

nsresult Text::ReplaceChild(nsIDOMNode *newChild, nsIDOMNode *oldChild)
{
  return nsHTMLContent::ReplaceChild(newChild, oldChild);
}

nsresult Text::RemoveChild(nsIDOMNode *oldChild)
{
  return nsHTMLContent::RemoveChild(oldChild);
}

nsresult
Text::GetData(nsString& aString)
{
  if (nsnull != mText) {
    aString.SetString(mText, mLength);
  }
  return NS_OK;
}

nsresult
Text::SetData(nsString& aString)
{
  if (mText) {
    delete[] mText;
    mText = nsnull;
  }

  mLength = aString.Length();
  mText = aString.ToNewUnicode();

  // Notify the document that the text changed
  if (nsnull != mDocument) {
    mDocument->ContentChanged(this, nsnull);
  }
  return NS_OK;
}

nsresult
Text::Append(nsString& aData)
{
  return Replace(mLength, 0, aData);
}

nsresult
Text::Insert(PRInt32 offset, nsString& aData)
{
  return Replace(offset, 0, aData);
}

nsresult
Text::Delete(PRInt32 offset, PRInt32 count)
{
  nsAutoString empty;
  return Replace(offset, count, empty);
}

nsresult
Text::Replace(PRInt32 offset, PRInt32 count, nsString& aData)
{
  // sanitize arguments
  if (offset < 0) {
    offset = 0;
  }
  if (offset > mLength) {
    offset = mLength;
  }
  if (count < 0) {
    count = 0;
  }

  // Allocate new buffer
  PRInt32 endOffset = offset + count;
  if (endOffset > mLength) {
    count = mLength - offset;
    endOffset = mLength;
  }
  PRInt32 dataLength = aData.Length();
  PRInt32 newLength = mLength - count + dataLength;
  PRUnichar* to = new PRUnichar[newLength ? newLength : 1];
  if (nsnull == to) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Copy over appropriate data
  if (0 != offset) {
    nsCRT::memcpy(to, mText, sizeof(PRUnichar) * offset);
  }
  if (0 != dataLength) {
    nsCRT::memcpy(to + offset, aData.GetUnicode(),
                  sizeof(PRUnichar) * dataLength);
  }
  if (endOffset != mLength) {
    nsCRT::memcpy(to + offset + dataLength, mText + endOffset,
                  sizeof(PRUnichar) * (mLength - endOffset));
  }

  // Switch to new buffer
  if (nsnull != mText) {
    delete [] mText;
  }
  mText = to;
  mLength = newLength;

  // Notify the document that the text changed
  if (nsnull != mDocument) {
    mDocument->ContentChanged(this, nsnull);
  }

  return NS_OK;
}

nsresult
Text::Splice(nsIDOMElement* element, PRInt32 offset, PRInt32 count)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
NS_NewHTMLText(nsIHTMLContent** aInstancePtrResult,
               const PRUnichar* us, PRInt32 uslen)
{
  nsIHTMLContent* it = new Text(us, uslen);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

nsresult
NS_NewHTMLText(nsIHTMLContent** aInstancePtrResult,
               nsIArena* aArena, const PRUnichar* us, PRInt32 uslen)
{
  nsIHTMLContent* it = new(aArena) Text(us, uslen);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

//----------------------------------------------------------------------

class SharedText : public Text {
public:
  SharedText(PRUnichar* aText, PRInt32 aLength);

protected:
  virtual ~SharedText();
};

SharedText::SharedText(PRUnichar* aText, PRInt32 aLength)
  : Text()
{
  if (0 == aLength) {
    aLength = nsCRT::strlen(aText);
  }
  mText = aText;
  mLength = aLength;
}

SharedText::~SharedText()
{
  mText = nsnull;
}

nsresult
NS_NewSharedHTMLText(nsIHTMLContent** aInstancePtrResult,
                     PRUnichar* us, PRInt32 uslen)
{
  nsIHTMLContent* it = new SharedText(us, uslen);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

nsresult
NS_NewSharedHTMLText(nsIHTMLContent** aInstancePtrResult,
                     nsIArena* aArena, PRUnichar* us, PRInt32 uslen)
{
  nsIHTMLContent* it = new(aArena) SharedText(us, uslen);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

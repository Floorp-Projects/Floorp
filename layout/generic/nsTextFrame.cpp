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
#include "nsLineLayout.h"
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
#include "nsITextContent.h"
#include "nsISelection.h"
#include "nsSelectionRange.h"

static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);

#ifdef NS_DEBUG
#undef NOISY
#undef NOISY_BLINK
#undef DEBUG_WORD_WRAPPING
#else
#undef NOISY
#undef NOISY_BLINK
#undef DEBUG_WORD_WRAPPING
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

// XXX need more of this in nsIRenderingContext.GetWidth
#define CH_NBSP 160

// XXX use a PreTextFrame for pre-formatted text?

static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);

class TextFrame;

class BlinkTimer : public nsITimerCallback {
public:
  BlinkTimer();
  ~BlinkTimer();

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

class TextFrame : public nsSplittableFrame {
public:
  TextFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  // nsIFrame
  NS_IMETHOD CreateContinuingFrame(nsIPresContext&  aPresContext,
                                   nsIFrame*        aParent,
                                   nsIStyleContext* aStyleContext,
                                   nsIFrame*&       aContinuingFrame);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD GetCursorAndContentAt(nsIPresContext& aPresContext,
                                   const nsPoint& aPoint,
                                   nsIFrame** aFrame,
                                   nsIContent** aContent,
                                   PRInt32& aCursor);

  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);

  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;

  NS_IMETHOD ListTag(FILE* out) const;

  NS_IMETHOD GetPosition(nsIPresContext& aCX,
                         nsIRenderingContext * aRendContext,
                         nsGUIEvent*     aEvent,
                         nsIFrame *      aNewFrame,
                         PRUint32&       aAcutalContentOffset,
                         PRInt32&        aOffset);

  // nsIHTMLReflow
  NS_IMETHOD FindTextRuns(nsLineLayout& aLineLayout);
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  // TextFrame methods
  struct SelectionInfo {
    PRInt32 mStartOffset;
    PRInt32 mEndOffset;
    PRBool mEmptySelection;
  };

  void ComputeSelectionInfo(nsIRenderingContext& aRenderingContext,
                            nsIDocument* aDocument,
                            PRInt32* aIndicies, PRInt32 aTextLength,
                            SelectionInfo& aResult);

  PRUnichar* PrepareUnicodeText(nsIRenderingContext& aRenderingContext,
                                PRInt32* aIndicies,
                                PRUnichar* aBuffer, PRInt32 aBufSize,
                                PRInt32& aStrLen,
                                nscoord& aNewWidth);

  char* PrepareAsciiText(nsIRenderingContext& aRenderingContext,
                         PRInt32* aIndexes,
                         char* aBuffer, PRInt32 aBufSize,
                         PRInt32& aStrLen,
                         nscoord& aDeltaWidthResult);

  void PaintTextDecorations(nsIRenderingContext& aRenderingContext,
                            PRUint8 aDecorations, 
                            nscoord aX, nscoord aY, nscoord aWidth);

  void PaintUnicodeText(nsIPresContext& aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        nscolor aTextColor,
                        PRUint8 aDecorations,
                        nscolor aSelectionTextColor,
                        nscolor aSelectionBGColor,
                        nscoord dx, nscoord dy);

  void PaintAsciiText(nsIPresContext& aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      nscolor aTextColor,
                      PRUint8 aDecorations,
                      nscolor aSelectionTextColor,
                      nscolor aSelectionBGColor,
                      nscoord dx, nscoord dy);

  nsReflowStatus ReflowPre(nsLineLayout& aLineLayout,
                           nsHTMLReflowMetrics& aMetrics,
                           const nsHTMLReflowState& aReflowState,
                           const nsStyleFont& aFont,
                           PRInt32 aStartingOffset);

  nsReflowStatus ReflowNormal(nsLineLayout& aLineLayout,
                              nsHTMLReflowMetrics& aMetrics,
                              const nsHTMLReflowState& aReflowState,
                              const nsStyleFont& aFontStyle,
                              const nsStyleText& aTextStyle,
                              PRInt32 aStartingOffset);

  nscoord ComputeTotalWordWidth(nsLineLayout& aLineLayout,
                                const nsHTMLReflowState& aReflowState,
                                nsIFrame* aNextFrame,
                                nscoord aBaseWidth);

  nscoord ComputeWordFragmentWidth(nsLineLayout& aLineLayout,
                                   const nsHTMLReflowState& aReflowState,
                                   nsIFrame* aNextFrame,
                                   nsITextContent* aText,
                                   PRBool& aStop);

protected:
  virtual ~TextFrame();

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
//#define TEXT_ENDS_IN_WHITESPACE 0x10

#define TEXT_GET_TAB_COUNT(_mf) ((_mf) >> 5)
#define TEXT_SET_TAB_COUNT(_mf,_tabs) (_mf) = (_mf) | ((_tabs)<< 5)

//----------------------------------------------------------------------

static PRBool gBlinkTextOff;
static BlinkTimer* gTextBlinker;
#ifdef NOISY_BLINK
static PRTime gLastTick;
#endif

BlinkTimer::BlinkTimer()
{
  NS_INIT_REFCNT();
  mTimer = nsnull;
}

BlinkTimer::~BlinkTimer()
{
  Stop();
}

void BlinkTimer::Start()
{
  nsresult rv = NS_NewTimer(&mTimer);
  if (NS_OK == rv) {
    mTimer->Init(this, 750);
  }
}

void BlinkTimer::Stop()
{
  if (nsnull != mTimer) {
    mTimer->Cancel();
    NS_RELEASE(mTimer);
  }
}

static NS_DEFINE_IID(kITimerCallbackIID, NS_ITIMERCALLBACK_IID);
NS_IMPL_ISUPPORTS(BlinkTimer, kITimerCallbackIID);

void BlinkTimer::AddFrame(TextFrame* aFrame) {
  mFrames.AppendElement(aFrame);
  if (1 == mFrames.Count()) {
    Start();
  }
}

PRBool BlinkTimer::RemoveFrame(TextFrame* aFrame) {
  PRBool rv = mFrames.RemoveElement(aFrame);
  if (0 == mFrames.Count()) {
    Stop();
  }
  return rv;
}

PRInt32 BlinkTimer::FrameCount() {
  return mFrames.Count();
}

void BlinkTimer::Notify(nsITimer *timer)
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
    gTextBlinker = new BlinkTimer();
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
TextFrame::GetCursorAndContentAt(nsIPresContext& aPresContext,
                                 const nsPoint& aPoint,
                                 nsIFrame** aFrame,
                                 nsIContent** aContent,
                                 PRInt32& aCursor)
{
  *aContent = mContent;
  aCursor = NS_STYLE_CURSOR_IBEAM;
  return NS_OK;
}

NS_IMETHODIMP
TextFrame::CreateContinuingFrame(nsIPresContext&  aCX,
                                 nsIFrame*        aParent,
                                 nsIStyleContext* aStyleContext,
                                 nsIFrame*&       aContinuingFrame)
{
  TextFrame* cf = new TextFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  cf->SetStyleContext(&aCX, aStyleContext);
  cf->AppendToFlow(this);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_IMETHODIMP
TextFrame::ContentChanged(nsIPresContext* aPresContext,
                          nsIContent*     aChild,
                          nsISupports*    aSubContent)
{
  // Generate a reflow command with this frame as the target frame
  nsIReflowCommand* cmd;
  nsresult          result;
                                                
  result = NS_NewHTMLReflowCommand(&cmd, this, nsIReflowCommand::ContentChanged);
  if (NS_OK == result) {
    nsIPresShell* shell = aPresContext->GetShell();
    shell->AppendReflowCommand(cmd);
    NS_RELEASE(shell);
    NS_RELEASE(cmd);
  }

  return result;
}

NS_IMETHODIMP
TextFrame::Paint(nsIPresContext& aPresContext,
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
    nsFont  plainFont(font->mFont);
    plainFont.decorations = NS_FONT_DECORATION_NONE;
    aRenderingContext.SetFont(plainFont); // don't let the system paint decorations

    // XXX Get these from style
    nscolor selbg = NS_RGB(0, 0, 0);
    nscolor selfg = NS_RGB(255, 255, 255);

    // Select rendering method and render
    PRUint32 hints = 0;
    aRenderingContext.GetHints(hints);
    if ((TEXT_HAS_MULTIBYTE & mFlags) ||
        (0 == (hints & NS_RENDERING_HINT_FAST_8BIT_TEXT))) {
      // Use PRUnichar rendering routine
      PaintUnicodeText(aPresContext, aRenderingContext,
                       color->mColor, font->mFont.decorations, selfg, selbg, 0, 0);
    }
    else {
      // Use char rendering routine
      PaintAsciiText(aPresContext, aRenderingContext,
                       color->mColor, font->mFont.decorations, selfg, selbg, 0, 0);
    }
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
  nsresult rv = aContent->QueryInterface(kITextContentIID, (void**) &tc);
  if ((NS_OK == rv) && (nsnull != tc)) {
    tc->GetText(cp, aLengthResult);
    NS_RELEASE(tc);
  }
  return cp;
}

/**
 * This method computes the starting and ending offsets of the
 * selection for this frame. The results are placed into
 * aResult. There are 5 cases that we represent with a starting offset
 * (aResult.mStartOffset), ending offset (aResult.mEndOffset) and an
 * empty selection flag (aResult.mEmptySelection):
 *
 * case 1: The selection completely misses this content/frame. In this
 * case mStartOffset and mEndOffset will be set to aTextLength and
 * mEmptySelection will be false.
 *
 * case 2: The selection begins somewhere before or at this frame and
 * ends somewhere in this frame. In this case mStartOffset will be set
 * to 0 and mEndOffset will be set to the end of the selection and
 * mEmptySelection will be false.
 *
 * case 3: The selection begins somewhere in this frame and ends
 * somewhere in this frame. In this case mStartOffset and mEndOffset
 * are set accordingly and if they happen to be the same value then
 * mEmptySelection is set to true (otherwise it is set to false).
 *
 * case 4: The selection begins somewhere in this frame and ends
 * somewhere else. In this case mStartOffset is set to where the
 * selection begins and mEndOffset is set to aTextLength and
 * mEmptySelection is set to false.
 *
 * case 5: The selection covers the entire content/frame. In this case
 * mStartOffset is set to zero and mEndOffset is set to aTextLength and
 * mEmptySelection is set to false.
 */
void
TextFrame::ComputeSelectionInfo(nsIRenderingContext& aRenderingContext,
                                nsIDocument* aDocument,
                                PRInt32* aIndicies, PRInt32 aTextLength,
                                SelectionInfo& aResult)
{
  // Assume, for now, that the selection misses this section of
  // content completely.
  aResult.mStartOffset = aTextLength;
  aResult.mEndOffset = aTextLength;
  aResult.mEmptySelection = PR_FALSE;

  nsISelection     * selection;
  aDocument->GetSelection(selection);

  nsSelectionRange * range     = selection->GetRange();
  nsSelectionPoint * startPnt  = range->GetStartPoint();
  nsSelectionPoint * endPnt    = range->GetEndPoint();
  nsIContent       * startContent = startPnt->GetContent();
  nsIContent       * endContent   = endPnt->GetContent();
  PRInt32 startOffset = startPnt->GetOffset() - mContentOffset;
  PRInt32 endOffset   = endPnt->GetOffset()   - mContentOffset;

  // Check for the case that requires up to 3 sections first. This
  // case also handles the empty selection.
  if ((mContent == startContent) && (mContent == endContent)) {
    // Selection starts and ends in this content (but maybe not this
    // frame)
    if ((startOffset >= mContentLength) || (endOffset <= 0)) {
      // Selection doesn't intersect this frame
    }
    else if (endOffset < mContentLength) {
      // End of selection is in this frame
      aResult.mEndOffset = aIndicies[endOffset] - mContentOffset;
      if (startOffset > 0) {
        // Beginning of selection is also in this frame (this is the 3
        // section case)
        aResult.mStartOffset = aIndicies[startOffset] - mContentOffset;
      }
      else {
        // This is a 2 section case
        aResult.mStartOffset = 0;
      }
      if (startOffset == endOffset) {
        aResult.mEmptySelection = PR_TRUE;
      }
    } else if (startOffset > 0) {
      // This is a 2 section case
      aResult.mStartOffset = aIndicies[startOffset] - mContentOffset;
    } else {
      // This is a 1 section case (where the entire section is
      // selected)
      aResult.mStartOffset = 0;
    }
  }
  else if (aDocument->IsInRange(startContent, endContent, mContent)) {
    if (mContent == startContent) {
      // Selection starts (but does not end) in this content (but
      // maybe not in this frame)
      if (startOffset <= 0) {
        // Selection starts before or at this frame
        aResult.mStartOffset = 0;
      }
      else if (startOffset < mContentLength) {
        // Selection starts somewhere in this frame
        aResult.mStartOffset = aIndicies[startOffset] - mContentOffset;
      }
      else {
        // Selection starts after this frame
      }
    }
    else if (mContent == endContent) {
      // Selection ends (but does not start) in this content (but
      // maybe not in this frame)
      if (endOffset <= 0) {
        // Selection ends before this frame
      }
      else if (endOffset < mContentLength) {
        // Selection ends in this frame
        aResult.mStartOffset = 0;
        aResult.mEndOffset = aIndicies[endOffset] - mContentOffset;
      }
      else {
        // Selection ends after this frame (the entire frame is selected)
        aResult.mStartOffset = 0;
      }
    }
    else {
      // Selection starts before this content and ends after this
      // content therefore the entire frame is selected
      aResult.mStartOffset = 0;
    }
  }

  NS_IF_RELEASE(startContent);
  NS_IF_RELEASE(endContent);
  NS_RELEASE(selection);
}

#define XP_IS_SPACE_W XP_IS_SPACE

/**
 * Prepare the text in the content for rendering. If aIndexes is not nsnull
 * then fill in aIndexes's with the mapping from the original input to
 * the prepared output.
 */
PRUnichar*
TextFrame::PrepareUnicodeText(nsIRenderingContext& aRenderingContext,
                              PRInt32* aIndexes,
                              PRUnichar* aBuffer, PRInt32 aBufSize,
                              PRInt32& aStrLen,
                              nscoord& aNewWidth)
{
  PRUnichar* s = aBuffer;
  PRUnichar* s0 = s;

  // Get text content
  PRInt32 textLength;
  const PRUnichar* cp = GetText(mContent, textLength);
  if (0 == textLength) {
    aBuffer[0] = 0;
    aStrLen = 0;
    return aBuffer;
  }
  cp += mContentOffset;
  const PRUnichar* end = cp + mContentLength;

  // Skip leading space if necessary
  PRInt32 mappingInx = 0;
  PRInt32 strInx = mContentOffset;
  if (0 != (mFlags & TEXT_SKIP_LEADING_WS)) {
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (!XP_IS_SPACE_W(ch)) {
        cp--;
        break;
      } else {
        if (nsnull != aIndexes) {
          aIndexes[mappingInx++] = strInx;
        }
      }
    }
  }

  PRInt32 length = 0;
  if (0 != (TEXT_IS_PRE & mFlags)) {
    PRIntn tabs = TEXT_GET_TAB_COUNT(mFlags);

    // Make a copy of the text we are to render, translating tabs
    // into whitespace.
    PRInt32 maxLen = (end - cp) + 8*tabs;
    if (maxLen > aBufSize) {
      s0 = s = new PRUnichar[maxLen];
    }

    // Translate tabs into whitespace; translate other whitespace into
    // spaces.
    PRIntn col = (PRIntn) mColumn;
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (XP_IS_SPACE_W(ch)) {
        if (ch == '\t') {
          PRIntn spaces = 8 - (col & 7);
          col += spaces;
          while (--spaces >= 0) {
            *s++ = ' ';
            length++;
          }
          if (nsnull != aIndexes) {
            aIndexes[mappingInx++] = strInx;
            strInx++;/* XXX wrong? */
          }
          continue;
        } else {
          ch = ' ';
        }
      }
      else if (ch == CH_NBSP) {
        ch = ' ';
      }
      *s++ = ch;
      length++;
      if (nsnull != aIndexes) {
        aIndexes[mappingInx++] = strInx;
        strInx++;
      }
      col++;
    }
  } else {
    // Make a copy of the text we are to render, compressing out
    // whitespace; translating whitespace to literal spaces;
    // eliminating trailing whitespace.
    PRInt32 maxLen = end - cp;
    if (maxLen > aBufSize) {
      s0 = s = new PRUnichar[maxLen];
    }

    // Compress down the whitespace
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (XP_IS_SPACE_W(ch)) {
        while (cp < end) {
          ch = *cp++;
          if (!XP_IS_SPACE_W(ch)) {
            cp--;
            break;
          } else {
            if (nsnull != aIndexes) {
              aIndexes[mappingInx++] = strInx;
            }
          }
        }
        ch = ' ';
      }
      else if (ch == CH_NBSP) {
        ch = ' ';
      }
      *s++ = ch;
      length++;
      if (nsnull != aIndexes) {
        aIndexes[mappingInx++] = strInx;
        strInx++;
      }
    }
  }

  // Now remove trailing whitespace
  PRIntn zapped = 0;
  while (s > s0) {
    if (s[-1] == ' ') {
      s--;
      length--;
      zapped++;
    }
    else
      break;
  }
  if (0 != zapped) {
    nscoord spaceWidth;
    aRenderingContext.GetWidth(' ', spaceWidth);
    aNewWidth = aNewWidth - spaceWidth*zapped;
  }

  aStrLen = length;
  return s0;
}

/**
 * Prepare the text in the content for rendering. If aIndexes is not nsnull
 * then fill in aIndexes's with the mapping from the original input to
 * the prepared output.
 */
char*
TextFrame::PrepareAsciiText(nsIRenderingContext& aRenderingContext,
                            PRInt32* aIndexes,
                            char* aBuffer, PRInt32 aBufSize,
                            PRInt32& aStrLen,
                            nscoord& aNewWidth)
{
  char* s = aBuffer;
  char* s0 = s;

  // Get text content
  PRInt32 textLength;
  const PRUnichar* cp = GetText(mContent, textLength);
  if (0 == textLength) {
    aBuffer[0] = 0;
    aStrLen = 0;
    return aBuffer;
  }
  cp += mContentOffset;
  const PRUnichar* end = cp + mContentLength;

  // Skip leading space if necessary
  PRInt32 mappingInx = 0;
  PRInt32 strInx = mContentOffset;
  if (0 != (mFlags & TEXT_SKIP_LEADING_WS)) {
    while (cp < end) {
      char ch = char(*cp++);
      if (!XP_IS_SPACE(ch)) {
        cp--;
        break;
      } else {
        if (nsnull != aIndexes) {
          aIndexes[mappingInx++] = strInx;
        }
      }
    }
  }

  PRInt32 length = 0;
  if (0 != (TEXT_IS_PRE & mFlags)) {
    PRIntn tabs = TEXT_GET_TAB_COUNT(mFlags);

    // Make a copy of the text we are to render, translating tabs
    // into whitespace.
    PRInt32 maxLen = (end - cp) + 8*tabs;
    if (maxLen > aBufSize) {
      s0 = s = new char[maxLen];
    }

    // Translate tabs into whitespace; translate other whitespace into
    // spaces.
    PRIntn col = (PRIntn) mColumn;
    while (cp < end) {
      char ch = char(*cp++);
      if (XP_IS_SPACE(ch)) {
        if (ch == '\t') {
          PRIntn spaces = 8 - (col & 7);
          col += spaces;
          while (--spaces >= 0) {
            *s++ = ' ';
            length++;
          }
          if (nsnull != aIndexes) {
            aIndexes[mappingInx++] = strInx;
            strInx++;/* XXX wrong? */
          }
          continue;
        } else {
          ch = ' ';
        }
      }
      else if (ch == CH_NBSP) {
        ch = ' ';
      }
      *s++ = ch;
      length++;
      if (nsnull != aIndexes) {
        aIndexes[mappingInx++] = strInx;
        strInx++;
      }
      col++;
    }
  } else {
    // Make a copy of the text we are to render, compressing out
    // whitespace; translating whitespace to literal spaces;
    // eliminating trailing whitespace.
    PRInt32 maxLen = end - cp;
    if (maxLen > aBufSize) {
      s0 = s = new char[maxLen];
    }

    // Compress down the whitespace
    while (cp < end) {
      char ch = char(*cp++);
      if (XP_IS_SPACE(ch)) {
        while (cp < end) {
          ch = char(*cp++);
          if (!XP_IS_SPACE(ch)) {
            cp--;
            break;
          } else {
            if (nsnull != aIndexes) {
              aIndexes[mappingInx++] = strInx;
            }
          }
        }
        ch = ' ';
      }
      else if (ch == CH_NBSP) {
        ch = ' ';
      }
      *s++ = ch;
      length++;
      if (nsnull != aIndexes) {
        aIndexes[mappingInx++] = strInx;
        strInx++;
      }
    }
  }

  // Now remove trailing whitespace
  PRIntn zapped = 0;
  while (s > s0) {
    if (s[-1] == ' ') {
      s--;
      length--;
      zapped++;
    }
    else
      break;
  }
  if (0 != zapped) {
    nscoord spaceWidth;
    aRenderingContext.GetWidth(' ', spaceWidth);
    aNewWidth = aNewWidth - spaceWidth*zapped;
  }

  aStrLen = length;
  return s0;
}

// XXX This clearly needs to be done by the container, *somehow*
#define CURSOR_COLOR NS_RGB(0,0,255)
static void
RenderSelectionCursor(nsIRenderingContext& aRenderingContext,
                      nscoord dx, nscoord dy, nscoord aHeight,
                      nscolor aCursorColor)
{
  nsPoint pnts[4];
  nscoord ox = aHeight / 4;
  nscoord oy = ox;
  nscoord x0 = dx;
  nscoord y0 = dy + aHeight;
  pnts[0].x = x0 - ox;
  pnts[0].y = y0;
  pnts[1].x = x0;
  pnts[1].y = y0 - oy;
  pnts[2].x = x0 + ox;
  pnts[2].y = y0;
  pnts[3].x = x0 - ox;
  pnts[3].y = y0;

  // Draw little blue triangle
  aRenderingContext.SetColor(aCursorColor);
  aRenderingContext.FillPolygon(pnts, 4);
}

// XXX letter-spacing
// XXX word-spacing

void 
TextFrame::PaintTextDecorations(nsIRenderingContext& aRenderingContext,
                                PRUint8 aDecorations, 
                                nscoord aX, nscoord aY, nscoord aWidth)
{
  nsIFontMetrics* fontMetrics = aRenderingContext.GetFontMetrics();
  if (nsnull != fontMetrics) {
    nscolor overColor;
    nscolor underColor;
    nscolor strikeColor;
    nsIStyleContext*  context = mStyleContext;
    PRUint8 decorMask = aDecorations;

    NS_ADDREF(context);
    do {  // find decoration colors
      const nsStyleText* styleText = 
                (const nsStyleText*)context->GetStyleData(eStyleStruct_Text);
      if (decorMask & styleText->mTextDecoration) {  // a decoration defined here
        const nsStyleColor* styleColor =
                (const nsStyleColor*)context->GetStyleData(eStyleStruct_Color);
        if (NS_STYLE_TEXT_DECORATION_UNDERLINE & decorMask & styleText->mTextDecoration) {
          underColor = styleColor->mColor;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_UNDERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_OVERLINE & decorMask & styleText->mTextDecoration) {
          overColor = styleColor->mColor;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_OVERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_LINE_THROUGH & decorMask & styleText->mTextDecoration) {
          strikeColor = styleColor->mColor;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
        }
      }
      if (0 != decorMask) {
        nsIStyleContext*  lastContext = context;
        context = context->GetParent();
        NS_RELEASE(lastContext);
      }
    } while ((nsnull != context) && (0 != decorMask));
    NS_IF_RELEASE(context);

    nscoord offset;
    nscoord size;
    nscoord baseline;
    fontMetrics->GetMaxAscent(baseline);
    if (aDecorations & (NS_FONT_DECORATION_OVERLINE | NS_FONT_DECORATION_UNDERLINE)) {
      fontMetrics->GetUnderline(offset, size);
      if (aDecorations & NS_FONT_DECORATION_OVERLINE) {
        aRenderingContext.SetColor(overColor);
        aRenderingContext.FillRect(aX, aY, aWidth, size);
      }
      if (aDecorations & NS_FONT_DECORATION_UNDERLINE) {
        aRenderingContext.SetColor(underColor);
        aRenderingContext.FillRect(aX, aY + baseline - offset, aWidth, size);
      }
    }
    if (aDecorations & NS_FONT_DECORATION_LINE_THROUGH) {
      fontMetrics->GetStrikeout(offset, size);
      aRenderingContext.SetColor(strikeColor);
      aRenderingContext.FillRect(aX, aY + baseline - offset, aWidth, size);
    }
    NS_RELEASE(fontMetrics);
  }
}

void
TextFrame::PaintUnicodeText(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            nscolor aTextColor,
                            PRUint8 aDecorations,
                            nscolor aSelectionTextColor,
                            nscolor aSelectionBGColor,
                            nscoord dx, nscoord dy)
{
  nsIPresShell* shell = aPresContext.GetShell();
  nsIDocument* doc = shell->GetDocument();
  PRBool displaySelection;
  displaySelection = doc->GetDisplaySelection();

  PRInt32 textLength;
  PRInt32 indicies[500];
  PRInt32* ip = indicies;
  if (mContentLength > 500) {
    ip = new PRInt32[mContentLength];
  }
  PRUnichar buf[500];
  nscoord width = mRect.width;
  PRUnichar* text = PrepareUnicodeText(aRenderingContext,
                                       displaySelection ? ip : nsnull,
                                       buf, 500, textLength, width);
  if (0 != textLength) {
    if (!displaySelection) {
      // When there is no selection showing, use the fastest and
      // simplest rendering approach
      aRenderingContext.DrawString(text, textLength, dx, dy, width);
      PaintTextDecorations(aRenderingContext, aDecorations, dx, dy, width);
    }
    else {
      SelectionInfo si;
      ComputeSelectionInfo(aRenderingContext, doc, ip, textLength, si);

      nscoord textWidth;
      nsIFontMetrics * fm = aRenderingContext.GetFontMetrics();
      if (si.mEmptySelection) {
        aRenderingContext.DrawString(text, textLength, dx, dy, width);
        PaintTextDecorations(aRenderingContext, aDecorations, dx, dy, width);
        aRenderingContext.GetWidth(text, PRUint32(si.mStartOffset), textWidth);
        RenderSelectionCursor(aRenderingContext,
                              dx + textWidth, dy, mRect.height,
                              CURSOR_COLOR);
      }
      else {
        nscoord x = dx;

        if (0 != si.mStartOffset) {
          // Render first (unselected) section
          aRenderingContext.GetWidth(text, PRUint32(si.mStartOffset), textWidth);
          aRenderingContext.DrawString(text, si.mStartOffset,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy, textWidth);
          x += textWidth;
        }
        PRInt32 secondLen = si.mEndOffset - si.mStartOffset;
        if (0 != secondLen) {
          // Get the width of the second (selected) section
          aRenderingContext.GetWidth(text + si.mStartOffset, PRUint32(secondLen), textWidth);

          // Render second (selected) section
          aRenderingContext.SetColor(aSelectionBGColor);
          aRenderingContext.FillRect(x, dy, textWidth, mRect.height);
          aRenderingContext.SetColor(aSelectionTextColor);
          aRenderingContext.DrawString(text + si.mStartOffset, secondLen,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy, textWidth);
          aRenderingContext.SetColor(aTextColor);
          x += textWidth;
        }
        if (textLength != si.mEndOffset) {
          PRInt32 thirdLen = textLength - si.mEndOffset;

          // Render third (unselected) section
          aRenderingContext.GetWidth(text + si.mEndOffset, PRUint32(thirdLen), textWidth);
          aRenderingContext.DrawString(text + si.mEndOffset,
                                       thirdLen, x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy, textWidth);
        }
      }
      NS_RELEASE(fm);
    }
  }

  // Cleanup
  if (text != buf) {
    delete [] text;
  }
  if (ip != indicies) {
    delete [] ip;
  }
  NS_RELEASE(shell);
  NS_RELEASE(doc);
}

void
TextFrame::PaintAsciiText(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          nscolor aTextColor,
                          PRUint8 aDecorations,
                          nscolor aSelectionTextColor,
                          nscolor aSelectionBGColor,
                          nscoord dx, nscoord dy)
{
  nsIPresShell* shell = aPresContext.GetShell();
  nsIDocument* doc = shell->GetDocument();
  PRBool displaySelection;
  displaySelection = doc->GetDisplaySelection();

  PRInt32 textLength;
  PRInt32 indicies[500];
  PRInt32* ip = indicies;
  if (mContentLength > 500) {
    ip = new PRInt32[mContentLength];
  }
  char buf[500];
  nscoord width = mRect.width;
  char* text = PrepareAsciiText(aRenderingContext,
                                displaySelection ? ip : nsnull,
                                buf, 500, textLength, width);
  if (0 != textLength) {
    if (!displaySelection) {
      // When there is no selection showing, use the fastest and
      // simplest rendering approach
      aRenderingContext.DrawString(text, textLength, dx, dy, width);
      PaintTextDecorations(aRenderingContext, aDecorations, dx, dy, width);
    }
    else {
      SelectionInfo si;
      ComputeSelectionInfo(aRenderingContext, doc, ip, textLength, si);

      nscoord textWidth;
      nsIFontMetrics * fm = aRenderingContext.GetFontMetrics();
      if (si.mEmptySelection) {
        aRenderingContext.DrawString(text, textLength, dx, dy, width);
        PaintTextDecorations(aRenderingContext, aDecorations, dx, dy, width);
        aRenderingContext.GetWidth(text, PRUint32(si.mStartOffset), textWidth);
        RenderSelectionCursor(aRenderingContext,
                              dx + textWidth, dy, mRect.height,
                              CURSOR_COLOR);
      }
      else {
        nscoord x = dx;

        if (0 != si.mStartOffset) {
          // Render first (unselected) section
          aRenderingContext.GetWidth(text, PRUint32(si.mStartOffset), textWidth);
          aRenderingContext.DrawString(text, si.mStartOffset,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy, textWidth);
          x += textWidth;
        }
        PRInt32 secondLen = si.mEndOffset - si.mStartOffset;
        if (0 != secondLen) {
          // Get the width of the second (selected) section
          aRenderingContext.GetWidth(text + si.mStartOffset, PRUint32(secondLen), textWidth);

          // Render second (selected) section
          aRenderingContext.SetColor(aSelectionBGColor);
          aRenderingContext.FillRect(x, dy, textWidth, mRect.height);
          aRenderingContext.SetColor(aSelectionTextColor);
          aRenderingContext.DrawString(text + si.mStartOffset, secondLen,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy, textWidth);
          aRenderingContext.SetColor(aTextColor);
          x += textWidth;
        }
        if (textLength != si.mEndOffset) {
          PRInt32 thirdLen = textLength - si.mEndOffset;

          // Render third (unselected) section
          aRenderingContext.GetWidth(text + si.mEndOffset, PRUint32(thirdLen), textWidth);
          aRenderingContext.DrawString(text + si.mEndOffset,
                                       thirdLen, x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy, textWidth);
        }
      }
      NS_RELEASE(fm);
    }
  }

  // Cleanup
  if (text != buf) {
    delete [] text;
  }
  if (ip != indicies) {
    delete [] ip;
  }
  NS_RELEASE(shell);
  NS_RELEASE(doc);
}

NS_IMETHODIMP
TextFrame::FindTextRuns(nsLineLayout& aLineLayout)
{
  if (nsnull == mPrevInFlow) {
    aLineLayout.AddText(this);
  }
  return NS_OK;
}

//---------------------------------------------------
// Uses a binary search for find where the cursor falls in the line of text
// It also keeps track of the part of the string that has already been measured
// so it doesn't have to keep measuring the same text over and over
//
// Param "aBaseWidth" contains the width in twips of the portion 
// of the text that has already been measured, and aBaseInx contains
// the index of the text that has already been measured.
//
// aTextWidth returns the (in twips) the length of the text that falls before the cursor
// aIndex contains the index of the text where the cursor falls
static PRBool
BinarySearchForPosition(nsIRenderingContext* acx, 
                        PRUnichar* aText,
                        PRInt32    aBaseWidth,
                        PRInt32    aBaseInx,
                        PRInt32    aStartInx, 
                        PRInt32    aEndInx, 
                        PRInt32    aCursorPos, 
                        PRInt32&   aIndex,
                        PRInt32&   aTextWidth)
{
  PRInt32 range = aEndInx - aStartInx;
  if (range == 1) {
    aIndex   = aStartInx + aBaseInx;
    return PR_TRUE;
  }
  PRInt32 inx = aStartInx + (range / 2);

  PRInt32 textWidth;
  acx->GetWidth(aText, inx, textWidth);

  PRInt32 fullWidth = aBaseWidth + textWidth;
  if (fullWidth == aCursorPos) {
    aIndex = inx;
    return PR_TRUE;
  } else if (aCursorPos < fullWidth) {
    aTextWidth = aBaseWidth;
    if (BinarySearchForPosition(acx, aText, aBaseWidth, aBaseInx, aStartInx, inx, aCursorPos, aIndex, aTextWidth)) {
      return PR_TRUE;
    }
  } else {
    aTextWidth = fullWidth;
    PRInt32 end = aEndInx - inx;
    if (BinarySearchForPosition(acx, aText+inx, fullWidth, aBaseInx+inx, 0, end, aCursorPos, aIndex, aTextWidth)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

//---------------------------------------------------------------------------
// Uses a binary search to find the position of the cursor in the text.
// The "indices array is used to map from the compressed text back to the 
// un-compressed text, selection is based on the un-compressed text, the visual 
// display of selection is based on the compressed text.
//---------------------------------------------------------------------------
NS_IMETHODIMP
TextFrame::GetPosition(nsIPresContext& aCX,
                       nsIRenderingContext * aRendContext,
                       nsGUIEvent*     aEvent,
                       nsIFrame*       aNewFrame,
                       PRUint32&       aAcutalContentOffset,
                       PRInt32&        aOffset)
{
  const PRInt16 kNumIndices = 512;

  // Get the rendered form of the text
  PRInt32 textLength;
  PRInt32  indicies[kNumIndices];
  PRInt32* ip = indicies;
  if (mContentLength > kNumIndices) {
    ip = new PRInt32[mContentLength+1];
  }
  PRUnichar buf[kNumIndices];
  nscoord width = 0;
  PRUnichar* text = PrepareUnicodeText(*aRendContext,
                                       ip, buf, kNumIndices, textLength,
                                       width);
  ip[mContentLength] = ip[mContentLength-1]+1;

  // Find the font metrics for this text
  nsIStyleContext* styleContext;
  aNewFrame->GetStyleContext(&aCX, styleContext);
  const nsStyleFont *font = (const nsStyleFont*) styleContext->GetStyleData(eStyleStruct_Font);
  NS_RELEASE(styleContext);

  nsIFontMetrics* fm = aCX.GetMetricsFor(font->mFont);

  PRInt32 offset = mContentOffset + mContentLength;

  PRInt32 index;
  PRInt32 textWidth;

  aRendContext->SetFont(fm);

  PRBool found = BinarySearchForPosition(aRendContext, text, 0, 0, 0, PRInt32(textLength), PRInt32(aEvent->point.x), index, textWidth);
  if (found) {
    PRInt32 charWidth;
    aRendContext->GetWidth(text[index], charWidth);
    charWidth /= 2;

    if (PRInt32(aEvent->point.x) > textWidth+charWidth) {
      index++;
    }

    offset = 0;
    PRInt32 j;
    PRInt32* ptr = ip; // We will use pointer math to make it fast, instead of indexing into ip
    for (j=0;j<=PRInt32(mContentLength);j++) {
      if (*ptr == index+mContentOffset) {
        offset = j+mContentOffset;
        break;
      }
      ptr++;
    }      
  }

  NS_RELEASE(fm);
  if (ip != indicies) {
    delete [] ip;
  }
  if (text != buf) {
    delete [] text;
  }

  aAcutalContentOffset = ((TextFrame *)aNewFrame)->mContentOffset;
  aOffset = offset;

  return NS_OK;
}

NS_IMETHODIMP
TextFrame::Reflow(nsIPresContext& aPresContext,
                  nsHTMLReflowMetrics& aMetrics,
                  const nsHTMLReflowState& aReflowState,
                  nsReflowStatus& aStatus)
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

  NS_ASSERTION(nsnull != aReflowState.lineLayout, "no line layout");
  if (NS_STYLE_WHITESPACE_PRE == text->mWhiteSpace) {
    // Use a specialized routine for pre-formatted text
    aStatus = ReflowPre(*aReflowState.lineLayout, aMetrics, aReflowState,
                        *font, startingOffset);
  } else {
    // Use normal wrapping routine for non-pre text (this includes
    // text that is not wrapping)
    aStatus = ReflowNormal(*aReflowState.lineLayout, aMetrics, aReflowState,
                           *font, *text, startingOffset);
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit TextFrame::Reflow: status=%x width=%d",
      aStatus, aMetrics.width));
  return NS_OK;
}

// Reflow normal text (stuff that doesn't have to deal with horizontal
// tabs). Normal text reflow may or may not wrap depending on the
// "whiteSpace" style property.
nsReflowStatus
TextFrame::ReflowNormal(nsLineLayout& aLineLayout,
                        nsHTMLReflowMetrics& aMetrics,
                        const nsHTMLReflowState& aReflowState,
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

  nsIFontMetrics* fm = aLineLayout.mPresContext.GetMetricsFor(aFont.mFont);
  PRInt32 spaceWidth;
  aReflowState.rendContext->SetFont(fm);
  aReflowState.rendContext->GetWidth(' ', spaceWidth);
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
  nscoord prevMaxWordWidth = 0;
  nscoord lastWordWidth;
  const PRUnichar* lastWordEnd = cpStart;
  const PRUnichar* lastWordStart = nsnull;
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
        skipWhitespace = PR_FALSE;
        continue;
      }
      width = spaceWidth;
      isWhitespace = PR_TRUE;
      lastWordStart = cp;
    } else {
      // The character is not a space character. Find the end of the
      // word and then measure it.
      if (ch >= 128) {
        hasMultibyte = PR_TRUE;
      }
      lastWordStart = cp - 1;
      while (cp < end) {
        ch = *cp;
        if (ch >= 128) {
          hasMultibyte = PR_TRUE;
        }
        if (!XP_IS_SPACE(ch)) {
          cp++;
          continue;
        }
        break;
      }
      aReflowState.rendContext->GetWidth(lastWordStart,
                                         PRUint32(cp - lastWordStart),
                                         width);
      skipWhitespace = PR_FALSE;
      isWhitespace = PR_FALSE;
      lastWordWidth = width;
    }

    // Now that we have the end of the word or whitespace, see if it
    // will fit.
    if ((0 != x) && wrapping && (x + width > maxWidth)) {
      // The word/whitespace will not fit.
      cp = lastWordEnd;
      break;
    }

    // The word/whitespace fits. Add it to the run of text.
    x += width;
    prevMaxWordWidth = maxWordWidth;
    if (width > maxWordWidth) {
      maxWordWidth = width;
    }
    lastWordEnd = cp;
    endsInWhitespace = isWhitespace;
  }
  if (hasMultibyte) {
    mFlags |= TEXT_HAS_MULTIBYTE;
  }

  // Post processing logic to deal with non-breakable-units (in
  // english "words")
  if (aLineLayout.InNonBreakingUnit()) {
    // We are already in an nbu. This means a text frame prior to
    // this one had a fragment of an nbu that is joined with this
    // frame. It also means that the prior frame already found this
    // frame and recorded it as part of the nbu.
#ifdef DEBUG_WORD_WRAPPING
    ListTag(stdout);
    printf(": in nbu; skipping\n");
#endif
    aLineLayout.ForgetWordFrame(this);
  }
  else {
    // There is no currently active nbu. This frame may contain the
    // start of one.
    if (endsInWhitespace) {
      // Nope, this frame doesn't start an nbu.
      aLineLayout.ForgetWordFrames();
    }
    else if ((nsnull != lastWordStart) && wrapping && (cp == end)) {
      // This frame does start an nbu. However, there is no point
      // messing around with it if we are already out of room.
      if ((0 == aLineLayout.GetPlacedFrames()) || (x <= maxWidth)) {
        // There is room for this nbu fragment. It's possible that
        // this nbu fragment is the end of the text-run. If it's not
        // then we continue with the look-ahead processing.
        nsIFrame* next = aLineLayout.FindNextText(this);
        if (nsnull != next) {
#ifdef DEBUG_WORD_WRAPPING
          nsAutoString tmp(lastWordStart, cp - lastWordStart);
          ListTag(stdout);
          printf(": start='");
          fputs(tmp, stdout);
          printf("' baseWidth=%d\n", lastWordWidth);
#endif
          // Look ahead in the text-run and compute the final nbu
          // width, taking into account any style changes and stopping
          // at the first breakable point.
          nscoord nbuWidth = ComputeTotalWordWidth(aLineLayout, aReflowState,
                                                   next, lastWordWidth);
          if ((0 == aLineLayout.GetPlacedFrames()) ||
              (x - lastWordWidth + nbuWidth <= maxWidth)) {
            // The fully joined nbu has fit. Account for the joined
            // nbu's affect on the max-element-size here (since the
            // joined nbu is large than it's pieces, the right effect
            // will occur)
            if (nbuWidth > maxWordWidth) {
              maxWordWidth = nbuWidth;
            }
          }
          else {
            // The fully joined nbu won't fit. We need to reduce our
            // size by the lastWordWidth.
            x -= lastWordWidth;
            maxWordWidth = prevMaxWordWidth;
            cp = lastWordEnd = lastWordStart;
#ifdef DEBUG_WORD_WRAPPING
            printf("  x=%d maxWordWidth=%d len=%d\n", x, maxWordWidth,
                   cp - cpStart);
#endif
            aLineLayout.ForgetWordFrames();
          }
        }
      }
    }
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

nscoord
TextFrame::ComputeTotalWordWidth(nsLineLayout& aLineLayout,
                                 const nsHTMLReflowState& aReflowState,
                                 nsIFrame* aNextFrame,
                                 nscoord aBaseWidth)
{
  nscoord addedWidth = 0;
  while (nsnull != aNextFrame) {
    nsIFrame* frame = aNextFrame;
    while (nsnull != frame) {
      nsIContent* content = nsnull;
      if ((NS_OK == frame->GetContent(content)) && (nsnull != content)) {
#ifdef DEBUG_WORD_WRAPPING
        printf("  next textRun=");
        frame->ListTag(stdout);
        printf("\n");
#endif
        nsITextContent* tc;
        if (NS_OK == content->QueryInterface(kITextContentIID, (void**)&tc)) {
          PRBool stop;
          nscoord moreWidth = ComputeWordFragmentWidth(aLineLayout,
                                                       aReflowState,
                                                       frame, tc,
                                                       stop);
          NS_RELEASE(tc);
          NS_RELEASE(content);
          addedWidth += moreWidth;
#ifdef DEBUG_WORD_WRAPPING
          printf("  moreWidth=%d (addedWidth=%d) stop=%c\n", moreWidth,
                 addedWidth, stop?'T':'F');
#endif
          if (stop) {
            goto done;
          }
        }
        else {
          // It claimed it was text but it doesn't implement the
          // nsITextContent API. Therefore I don't know what to do with it
          // and can't look inside it. Oh well.
          NS_RELEASE(content);
          goto done;
        }
      }
      // XXX This assumes that a next-in-flow will follow it's
      // prev-in-flow in the text-run. Maybe not a good assumption?
      // What if the next-in-flow isn't actually part of the flow?
      frame->GetNextInFlow(frame);
    }

    // Move on to the next frame in the text-run
    aNextFrame = aLineLayout.FindNextText(aNextFrame);
  }

 done:;
#ifdef DEBUG_WORD_WRAPPING
  printf("  total word width=%d\n", aBaseWidth + addedWidth);
#endif
  return aBaseWidth + addedWidth;
}

nscoord
TextFrame::ComputeWordFragmentWidth(nsLineLayout& aLineLayout,
                                    const nsHTMLReflowState& aReflowState,
                                    nsIFrame* aTextFrame,
                                    nsITextContent* aText,
                                    PRBool& aStop)
{
  // Get a pointer and length count for the text
  const PRUnichar* cp;
  const PRUnichar* cp0;
  const PRUnichar* end;
  PRInt32 len;
  if (NS_OK != aText->GetText(cp0, len)) {
    aStop = PR_TRUE;
    return 0;
  }
  cp = cp0;
  end = cp0 + len;

  // Count forward in the text until we hit a whitespace character or
  // run out of text.
  aStop = PR_FALSE;
  while (cp < end) {
    PRUnichar ch = *cp;
    if (XP_IS_SPACE(ch)) {
      aStop = PR_TRUE;
      break;
    }
    cp++;
  }
  len = cp - cp0;
  if (0 == len) {
    // Don't bother measuring nothing
    return 0;
  }

  const nsStyleFont* font;
  aTextFrame->GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&) font);
  if (nsnull != font) {
    // Measure the piece of text. Note that we have to select the
    // appropriate font into the text first because the rendering
    // context has our font in it, not the font that aText is using.
    nscoord width;
    nsIRenderingContext& rc = *aReflowState.rendContext;
    nsIFontMetrics* oldfm = rc.GetFontMetrics();
    nsIFontMetrics* fm = aLineLayout.mPresContext.GetMetricsFor(font->mFont);
    rc.SetFont(fm);
    rc.GetWidth(cp0, len, width);
    rc.SetFont(oldfm);
    NS_RELEASE(oldfm);
    NS_RELEASE(fm);
#ifdef DEBUG_WORD_WRAPPING
    nsAutoString tmp(cp0, len);
    printf("  fragment='");
    fputs(tmp, stdout);
    printf("' width=%d\n", width);
#endif

    // Remember the text frame for later so that we don't bother doing
    // the word look ahead.
    aLineLayout.RecordWordFrame(aTextFrame);
    return width;
  }

  return 0;
}

nsReflowStatus
TextFrame::ReflowPre(nsLineLayout& aLineLayout,
                     nsHTMLReflowMetrics& aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     const nsStyleFont& aFont,
                     PRInt32 aStartingOffset)
{
  nsReflowStatus rs = NS_FRAME_COMPLETE;

  PRInt32 textLength;
  const PRUnichar* cp = GetText(mContent, textLength);
  cp += aStartingOffset;
  const PRUnichar* cpStart = cp;
  const PRUnichar* end = cp + textLength - aStartingOffset;

  mFlags |= TEXT_IS_PRE;
  nsIFontMetrics* fm = aLineLayout.mPresContext.GetMetricsFor(aFont.mFont);
  PRInt32 width = 0;
  PRBool hasMultibyte = PR_FALSE;
  PRUint16 tabs = 0;
  PRUint16 col = aLineLayout.GetColumn();
  mColumn = col;
  nscoord spaceWidth, charWidth;
  aReflowState.rendContext->SetFont(fm);
  aReflowState.rendContext->GetWidth(' ', spaceWidth);

// XXX change this to measure a line at a time
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
    aReflowState.rendContext->GetWidth(ch, charWidth);
    width += charWidth;
    if (ch > 128) {
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

// XXX there is a copy of this in nsGenericDomDataNode.cpp
static void
ToCString(const PRUnichar* cp, PRInt32 aLen, nsString& aBuf)
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
  fprintf(out, "Text(%d)@%p", ContentIndexInContainer(this), this);
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
  ToCString(cp, mContentLength, tmp);
  fputs(tmp, out);
  fputs("\"\n", out);

  aIndent--;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);

  return NS_OK;
}

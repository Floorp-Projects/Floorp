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
#include "nsISelection.h"
#include "nsSelectionRange.h"

#include "nsITextContent.h"
#include "nsTextReflow.h"/* XXX rename to nsTextRun */
#include "nsTextFragment.h"
#include "nsTextTransformer.h"

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

#define WORD_BUF_SIZE 100
#define TEXT_BUF_SIZE 1000

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

  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const;

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

  void PrepareUnicodeText(nsIRenderingContext& aRenderingContext,
                          nsTextTransformer& aTransformer,
                          PRInt32* aIndicies,
                          PRUnichar* aBuffer,
                          PRInt32& aTextLen,
                          nscoord& aNewWidth);

  void PrepareAsciiText(nsIRenderingContext& aRenderingContext,
                        nsTextTransformer& aTransformer,
                        PRInt32* aIndicies,
                        char* aBuffer,
                        PRInt32& aTextLen,
                        nscoord& aNewWidth);

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

  void ToCString(nsString& aBuf, PRInt32& aContentLength) const;

protected:
  virtual ~TextFrame();

  // XXX pack this tighter?
  PRInt32 mContentOffset;
  PRInt32 mContentLength;
  PRUint32 mFlags;
  PRInt32 mColumn;
};

// Flag information used by rendering code. This information is
// computed by the ResizeReflow code. Remaining bits are used by the
// tab count.

// Flag indicating that whitespace was skipped
#define TEXT_SKIP_LEADING_WS    0x01

#define TEXT_HAS_MULTIBYTE      0x02

#define TEXT_IS_PRE             0x04

#define TEXT_BLINK_ON           0x08

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
                       color->mColor, font->mFont.decorations,
                       selfg, selbg, 0, 0);
    }
    else {
      // Use char rendering routine
      PaintAsciiText(aPresContext, aRenderingContext,
                     color->mColor, font->mFont.decorations,
                     selfg, selbg, 0, 0);
    }
  }

  return NS_OK;
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
void
TextFrame::PrepareUnicodeText(nsIRenderingContext& aRenderingContext,
                              nsTextTransformer& aTX,
                              PRInt32* aIndexes,
                              PRUnichar* aBuffer,
                              PRInt32& aTextLen,
                              nscoord& aNewWidth)
{
  PRUnichar* dst = aBuffer;

  // Setup transform to operate starting in the content at our content
  // offset
  aTX.Init(this, mContentOffset);

  PRInt32 mappingInx = 0;
  PRInt32 strInx = mContentOffset;

  // Skip over the leading whitespace
  PRInt32 n = mContentLength;
  if (0 == (mFlags & TEXT_IS_PRE)) {
    if (0 != (mFlags & TEXT_SKIP_LEADING_WS)) {
      PRBool isWhitespace;
      PRInt32 wordLen, contentLen;
      aTX.GetNextWord(wordLen, contentLen, isWhitespace);
      NS_ASSERTION(isWhitespace, "mFlags and content are out of sync");
      if (isWhitespace) {
        if (nsnull != aIndexes) {
          // Point mapping indicies at the same content index since
          // all of the compressed whitespace maps down to the same
          // renderable character.
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx;
          }
        }
        n -= contentLen;
        NS_ASSERTION(n >= 0, "whoops");
      }
    }
  }

  // Rescan the content and transform it. Stop when we have consumed
  // mContentLength characters.
  PRInt32 column = mColumn;
  PRInt32 textLength = 0;
  while (0 != n) {
    PRUnichar* bp;
    PRInt32 wordLen, contentLen;
    if (0 != (TEXT_IS_PRE & mFlags)) {
      // Get the next section. Check for a tab and if we find one,
      // expand it.
      bp = aTX.GetNextSection(wordLen, contentLen);
      if (nsnull == bp) {
        break;
      }
      if ('\t' == bp[0]) {
        PRInt32 spaces = 8 - (7 & column);
        PRUnichar* tp = bp;
        wordLen = spaces;
        while (--spaces >= 0) {
          *tp++ = ' ';
        }
        // XXX This is a one to many mapping that I think isn't handled well
        if (nsnull != aIndexes) {
          *aIndexes++ = strInx;
          strInx++;
        }
      }
      else {
        // Point mapping indicies at each content index in the section
        if (nsnull != aIndexes) {
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx++;
          }
        }
      }
      column += wordLen;
    }
    else {
      // Get the next word
      PRBool isWhitespace;
      bp = aTX.GetNextWord(wordLen, contentLen, isWhitespace);
      if (nsnull == bp) {
        break;
      }
      if (isWhitespace) {
        if (nsnull != aIndexes) {
          // Point mapping indicies at the same content index since
          // all of the compressed whitespace maps down to the same
          // renderable character.
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx;
          }
          strInx++;
        }
      }
      else {
        if (nsnull != aIndexes) {
          // Point mapping indicies at each content index in the word
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx++;
          }
        }
      }
    }
    textLength += wordLen;
    n -= contentLen;
    nsCRT::memcpy(dst, bp, sizeof(PRUnichar) * wordLen);
    dst += wordLen;
    NS_ASSERTION(n >= 0, "whoops");
  }

  // Now remove trailing whitespace if appropriate. It's appropriate
  // if this frame is continued. And it's also appropriate if this is
  // not last (or only) frame in a text-run.
  // XXX fix this to fully obey the comment!
  if (nsnull != mNextInFlow) {
    PRIntn zapped = 0;
    while (dst > aBuffer) {
      if (dst[-1] == ' ') {
        dst--;
        textLength--;
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
  }
  aTextLen = textLength;
}

/**
 * Prepare the text in the content for rendering. If aIndexes is not nsnull
 * then fill in aIndexes's with the mapping from the original input to
 * the prepared output.
 */
void
TextFrame::PrepareAsciiText(nsIRenderingContext& aRenderingContext,
                            nsTextTransformer& aTX,
                            PRInt32* aIndexes,
                            char* aBuffer,
                            PRInt32& aTextLen,
                            nscoord& aNewWidth)
{
  char* dst = aBuffer;

  // Setup transform to operate starting in the content at our content
  // offset
  aTX.Init(this, mContentOffset);

  PRInt32 mappingInx = 0;
  PRInt32 strInx = mContentOffset;

  // Skip over the leading whitespace
  PRInt32 n = mContentLength;
  if (0 == (mFlags & TEXT_IS_PRE)) {
    if (0 != (mFlags & TEXT_SKIP_LEADING_WS)) {
      PRBool isWhitespace;
      PRInt32 wordLen, contentLen;
      aTX.GetNextWord(wordLen, contentLen, isWhitespace);
      NS_ASSERTION(isWhitespace, "mFlags and content are out of sync");
      if (isWhitespace) {
        if (nsnull != aIndexes) {
          // Point mapping indicies at the same content index since
          // all of the compressed whitespace maps down to the same
          // renderable character.
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx;
          }
        }
        n -= contentLen;
        NS_ASSERTION(n >= 0, "whoops");
      }
    }
  }

  // Rescan the content and transform it. Stop when we have consumed
  // mContentLength characters.
  PRInt32 column = mColumn;
  PRInt32 textLength = 0;
  while (0 != n) {
    PRInt32 wordLen, contentLen;
    PRUnichar* bp;
    if (0 != (TEXT_IS_PRE & mFlags)) {
      // Get the next section. Check for a tab and if we find one,
      // expand it.
      bp = aTX.GetNextSection(wordLen, contentLen);
      if (nsnull == bp) {
        break;
      }
      if ('\t' == bp[0]) {
        PRInt32 spaces = 8 - (7 & column);
        PRUnichar* tp = bp;
        wordLen = spaces;
        while (--spaces >= 0) {
          *tp++ = ' ';
        }
        // XXX This is a one to many mapping that I think isn't handled well
        if (nsnull != aIndexes) {
          *aIndexes++ = strInx;
          strInx++;
        }
      }
      else {
        // Point mapping indicies at each content index in the section
        if (nsnull != aIndexes) {
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx++;
          }
        }
      }
      column += wordLen;
    }
    else {
      // Get the next word
      PRBool isWhitespace;
      bp = aTX.GetNextWord(wordLen, contentLen, isWhitespace);
      if (nsnull == bp) {
        break;
      }
      if (isWhitespace) {
        if (nsnull != aIndexes) {
          // Point mapping indicies at the same content index since
          // all of the compressed whitespace maps down to the same
          // renderable character.
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx;
          }
          strInx++;
        }
      }
      else {
        if (nsnull != aIndexes) {
          // Point mapping indicies at each content index in the word
          PRInt32 i = contentLen;
          while (--i >= 0) {
            *aIndexes++ = strInx++;
          }
        }
      }
    }
    textLength += wordLen;
    n -= contentLen;

    // Convert PRUnichar's to char's
    PRInt32 i = wordLen;
    while (--i >= 0) {
      *dst++ = char((unsigned char) (*bp++));
    }
    NS_ASSERTION(n >= 0, "whoops");
  }

  // Now remove trailing whitespace if appropriate. It's appropriate
  // if this frame is continued. And it's also appropriate if this is
  // not last (or only) frame in a text-run.
  // XXX fix this to obey the comment!
  if (nsnull != mNextInFlow) {
    PRIntn zapped = 0;
    while (dst > aBuffer) {
      if (dst[-1] == ' ') {
        dst--;
        textLength--;
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
  }
  aTextLen = textLength;
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

  // Make enough space to transform
  PRUnichar wordBufMem[WORD_BUF_SIZE];
  PRUnichar paintBufMem[TEXT_BUF_SIZE];
  PRInt32 indicies[TEXT_BUF_SIZE];
  PRInt32* ip = indicies;
  PRUnichar* paintBuf = paintBufMem;
  if (mContentLength > TEXT_BUF_SIZE) {
    ip = new PRInt32[mContentLength];
    paintBuf = new PRUnichar[mContentLength];
  }
  nscoord width = mRect.width;
  PRInt32 textLength;

  // Transform text from content into renderable form
  nsTextTransformer tx(wordBufMem, WORD_BUF_SIZE);
  PrepareUnicodeText(aRenderingContext, tx,
                     displaySelection ? ip : nsnull,
                     paintBuf, textLength, width);

  PRUnichar* text = paintBuf;
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
          aRenderingContext.GetWidth(text, PRUint32(si.mStartOffset),
                                     textWidth);
          aRenderingContext.DrawString(text, si.mStartOffset,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy,
                               textWidth);
          x += textWidth;
        }
        PRInt32 secondLen = si.mEndOffset - si.mStartOffset;
        if (0 != secondLen) {
          // Get the width of the second (selected) section
          aRenderingContext.GetWidth(text + si.mStartOffset,
                                     PRUint32(secondLen), textWidth);

          // Render second (selected) section
          aRenderingContext.SetColor(aSelectionBGColor);
          aRenderingContext.FillRect(x, dy, textWidth, mRect.height);
          aRenderingContext.SetColor(aSelectionTextColor);
          aRenderingContext.DrawString(text + si.mStartOffset, secondLen,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy,
                               textWidth);
          aRenderingContext.SetColor(aTextColor);
          x += textWidth;
        }
        if (textLength != si.mEndOffset) {
          PRInt32 thirdLen = textLength - si.mEndOffset;

          // Render third (unselected) section
          aRenderingContext.GetWidth(text + si.mEndOffset, PRUint32(thirdLen),
                                     textWidth);
          aRenderingContext.DrawString(text + si.mEndOffset,
                                       thirdLen, x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy,
                               textWidth);
        }
      }
      NS_RELEASE(fm);
    }
  }

  // Cleanup
  if (paintBuf != paintBufMem) {
    delete [] paintBuf;
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

  // Make enough space to transform
  PRUnichar wordBufMem[WORD_BUF_SIZE];
  char paintBufMem[TEXT_BUF_SIZE];
  PRInt32 indicies[TEXT_BUF_SIZE];
  PRInt32* ip = indicies;
  char* paintBuf = paintBufMem;
  if (mContentLength > TEXT_BUF_SIZE) {
    ip = new PRInt32[mContentLength];
    paintBuf = new char[mContentLength];
  }
  nscoord width = mRect.width;
  PRInt32 textLength;

  // Transform text from content into renderable form
  nsTextTransformer tx(wordBufMem, WORD_BUF_SIZE);
  PrepareAsciiText(aRenderingContext, tx,
                   displaySelection ? ip : nsnull,
                   paintBuf, textLength, width);

  char* text = paintBuf;
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
          aRenderingContext.GetWidth(text, PRUint32(si.mStartOffset),
                                     textWidth);
          aRenderingContext.DrawString(text, si.mStartOffset,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy,
                               textWidth);
          x += textWidth;
        }
        PRInt32 secondLen = si.mEndOffset - si.mStartOffset;
        if (0 != secondLen) {
          // Get the width of the second (selected) section
          aRenderingContext.GetWidth(text + si.mStartOffset,
                                     PRUint32(secondLen), textWidth);

          // Render second (selected) section
          aRenderingContext.SetColor(aSelectionBGColor);
          aRenderingContext.FillRect(x, dy, textWidth, mRect.height);
          aRenderingContext.SetColor(aSelectionTextColor);
          aRenderingContext.DrawString(text + si.mStartOffset, secondLen,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy,
                               textWidth);
          aRenderingContext.SetColor(aTextColor);
          x += textWidth;
        }
        if (textLength != si.mEndOffset) {
          PRInt32 thirdLen = textLength - si.mEndOffset;

          // Render third (unselected) section
          aRenderingContext.GetWidth(text + si.mEndOffset, PRUint32(thirdLen),
                                     textWidth);
          aRenderingContext.DrawString(text + si.mEndOffset,
                                       thirdLen, x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aDecorations, x, dy,
                               textWidth);
        }
      }
      NS_RELEASE(fm);
    }
  }

  // Cleanup
  if (paintBuf != paintBufMem) {
    delete [] paintBuf;
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
  PRUnichar wordBufMem[WORD_BUF_SIZE];
  PRUnichar paintBufMem[TEXT_BUF_SIZE];
  PRInt32 indicies[TEXT_BUF_SIZE];
  PRInt32* ip = indicies;
  PRUnichar* paintBuf = paintBufMem;
  if (mContentLength >= TEXT_BUF_SIZE) {
    ip = new PRInt32[mContentLength+1];
    paintBuf = new PRUnichar[mContentLength];
  }
  nscoord width = 0;
  PRInt32 textLength;

  // Get the renderable form of the text
  nsTextTransformer tx(wordBufMem, WORD_BUF_SIZE);
  PrepareUnicodeText(*aRendContext, tx,
                     ip, paintBuf, textLength, width);
  ip[mContentLength] = ip[mContentLength-1]+1;

  // Find the font metrics for this text
  nsIStyleContext* styleContext;
  aNewFrame->GetStyleContext(&aCX, styleContext);
  const nsStyleFont *font = (const nsStyleFont*)
    styleContext->GetStyleData(eStyleStruct_Font);
  NS_RELEASE(styleContext);
  nsIFontMetrics* fm = aCX.GetMetricsFor(font->mFont);
  aRendContext->SetFont(fm);

  PRInt32 offset = mContentOffset + mContentLength;
  PRInt32 index;
  PRInt32 textWidth;
  PRUnichar* text = paintBuf;
  PRBool found = BinarySearchForPosition(aRendContext, text, 0, 0, 0,
                                         PRInt32(textLength),
                                         PRInt32(aEvent->point.x),
                                         index, textWidth);
  if (found) {
    PRInt32 charWidth;
    aRendContext->GetWidth(text[index], charWidth);
    charWidth /= 2;

    if (PRInt32(aEvent->point.x) > textWidth+charWidth) {
      index++;
    }

    offset = 0;
    PRInt32 j;
    PRInt32* ptr = ip;
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
  if (paintBuf != paintBufMem) {
    delete [] paintBuf;
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

static void
MeasureSmallCapsText(const nsHTMLReflowState& aReflowState,
                     const nsStyleFont& aFont,
                     PRUnichar* aWord,
                     PRInt32 aWordLength,
                     nscoord& aWidthResult)
{
  // XXX write me!
  aReflowState.rendContext->GetWidth(aWord, aWordLength, aWidthResult);
}

nsReflowStatus
TextFrame::ReflowNormal(nsLineLayout& aLineLayout,
                        nsHTMLReflowMetrics& aMetrics,
                        const nsHTMLReflowState& aReflowState,
                        const nsStyleFont& aFont,
                        const nsStyleText& aTextStyle,
                        PRInt32 aStartingOffset)
{
  nsIFontMetrics* fm = aLineLayout.mPresContext.GetMetricsFor(aFont.mFont);
  PRInt32 spaceWidth;
  aReflowState.rendContext->SetFont(fm);
  aReflowState.rendContext->GetWidth(' ', spaceWidth);
  PRBool wrapping = PR_TRUE;
  if (NS_STYLE_WHITESPACE_NORMAL != aTextStyle.mWhiteSpace) {
    wrapping = PR_FALSE;
  }
  PRBool smallCaps = NS_STYLE_FONT_VARIANT_SMALL_CAPS == aFont.mFont.variant;

  // Set whitespace skip flag
  PRBool skipWhitespace = PR_FALSE;
  if (aLineLayout.GetSkipLeadingWhiteSpace()) {
    skipWhitespace = PR_TRUE;
  }

  nscoord x = 0;
  nscoord maxWidth = aReflowState.maxSize.width;
  nscoord maxWordWidth = 0;
  nscoord prevMaxWordWidth = 0;
  PRBool endsInWhitespace = PR_FALSE;

  // Setup text transformer to transform this frames text content
  nsTextRun* textRun = aLineLayout.FindTextRunFor(this);
  PRUnichar wordBuf[WORD_BUF_SIZE];
  nsTextTransformer tx(wordBuf, WORD_BUF_SIZE);
  nsresult rv = tx.Init(/**textRun, XXX*/ this, aStartingOffset);
  if (NS_OK != rv) {
    return rv;
  }
  PRInt32 contentLength = tx.GetContentLength();

  // Offset tracks how far along we are in the content
  PRInt32 offset = aStartingOffset;
  PRInt32 prevOffset = -1;
  nscoord lastWordWidth = 0;

  // Loop over words and whitespace in content and measure
  for (;;) {
    // Get next word/whitespace from the text
    PRBool isWhitespace;
    PRInt32 wordLen, contentLen;
    PRUnichar* bp = tx.GetNextWord(wordLen, contentLen, isWhitespace);
    if (nsnull == bp) {
      break;
    }

    // Measure the word/whitespace
    nscoord width;
    if (isWhitespace) {
      if (skipWhitespace) {
        offset += contentLen;
        skipWhitespace = PR_FALSE;

        // Only set flag when we actually do skip whitespace
        mFlags |= TEXT_SKIP_LEADING_WS;
        continue;
      }
      width = spaceWidth;
    } else {
      if (smallCaps) {
        MeasureSmallCapsText(aReflowState, aFont, bp, wordLen, width);
      }
      else {
        aReflowState.rendContext->GetWidth(bp, wordLen, width);
      }
      skipWhitespace = PR_FALSE;
      lastWordWidth = width;
    }

    // See if there is room for the text
    if ((0 != x) && wrapping && (x + width > maxWidth)) {
      // The text will not fit.
      break;
    }
    x += width;
    prevMaxWordWidth = maxWordWidth;
    if (width > maxWordWidth) {
      maxWordWidth = width;
    }
    endsInWhitespace = isWhitespace;
    prevOffset = offset;
    offset += contentLen;
  }
  if (tx.HasMultibyte()) {
    mFlags |= TEXT_HAS_MULTIBYTE;
  }

  // Post processing logic to deal with word-breaking that spans
  // multiple frames.
  if (aLineLayout.InWord()) {
    // We are already in a word. This means a text frame prior to this
    // one had a fragment of a nbword that is joined with this
    // frame. It also means that the prior frame already found this
    // frame and recorded it as part of the word.
#ifdef DEBUG_WORD_WRAPPING
    ListTag(stdout);
    printf(": in nbu; skipping\n");
#endif
    aLineLayout.ForgetWordFrame(this);
  }
  else {
    // There is no currently active word. This frame may contain the
    // start of one.
    if (endsInWhitespace) {
      // Nope, this frame doesn't start a word.
      aLineLayout.ForgetWordFrames();
    }
    else if (wrapping && (offset == contentLength) && (prevOffset >= 0)) {
      // This frame does start a word. However, there is no point
      // messing around with it if we are already out of room.
      if ((0 == aLineLayout.GetPlacedFrames()) || (x <= maxWidth)) {
        // There is room for this word fragment. It's possible that
        // this word fragment is the end of the text-run. If it's not
        // then we continue with the look-ahead processing.
        nsIFrame* next = aLineLayout.FindNextText(this);
        if (nsnull != next) {
#ifdef DEBUG_WORD_WRAPPING
          nsAutoString tmp(tx.GetTextAt(prevOffset), offset-prevOffset);
          ListTag(stdout);
          printf(": start='");
          fputs(tmp, stdout);
          printf("' baseWidth=%d\n", lastWordWidth);
#endif
          // Look ahead in the text-run and compute the final word
          // width, taking into account any style changes and stopping
          // at the first breakable point.
          nscoord wordWidth = ComputeTotalWordWidth(aLineLayout, aReflowState,
                                                    next, lastWordWidth);
          if ((0 == aLineLayout.GetPlacedFrames()) ||
              (x - lastWordWidth + wordWidth <= maxWidth)) {
            // The fully joined word has fit. Account for the joined
            // word's affect on the max-element-size here (since the
            // joined word is large than it's pieces, the right effect
            // will occur from the perspective of the container
            // reflowing this frame)
            if (wordWidth > maxWordWidth) {
              maxWordWidth = wordWidth;
            }
          }
          else {
            // The fully joined word won't fit. We need to reduce our
            // size by the lastWordWidth.
            x -= lastWordWidth;
            maxWordWidth = prevMaxWordWidth;
            offset = prevOffset;
#ifdef DEBUG_WORD_WRAPPING
            printf("  x=%d maxWordWidth=%d len=%d\n", x, maxWordWidth,
                   offset - aStartingOffset);
#endif
            aLineLayout.ForgetWordFrames();
          }
        }
      }
    }
  }

  if (0 == x) {
    // Since we collapsed into nothingness (all our whitespace is
    // ignored) leave the skip-leading-whitespace flag alone so that
    // whitespace that immediately follows this frame can be properly
    // skipped.
  }
  else {
    aLineLayout.SetSkipLeadingWhiteSpace(endsInWhitespace);
  }

  // Setup metrics for caller; store final max-element-size information
  aMetrics.width = x;
  if (0 == x) {
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
  }
  else {
    fm->GetHeight(aMetrics.height);
    fm->GetMaxAscent(aMetrics.ascent);
    fm->GetMaxDescent(aMetrics.descent);
  }
  if (!wrapping) {
    maxWordWidth = x;
  }
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = maxWordWidth;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  NS_RELEASE(fm);

  // Set content offset and length and return completion status
  mContentOffset = aStartingOffset;
  mContentLength = offset - aStartingOffset;
  if (offset == contentLength) {
    return NS_FRAME_COMPLETE;
  }
  else if (offset == aStartingOffset) {
    return NS_INLINE_LINE_BREAK_BEFORE();
  }
  return NS_FRAME_NOT_COMPLETE;
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
  nsTextRun* textRun = aLineLayout.FindTextRunFor(aTextFrame);
  PRUnichar buf[TEXT_BUF_SIZE];
  nsTextTransformer tx(buf, TEXT_BUF_SIZE);
  // XXX we need the content-offset of the text frame!!! 0 won't
  // always be right when continuations are in action
  tx.Init(/**textRun, XXX*/ aTextFrame, 0);
                       
  PRBool isWhitespace;
  PRInt32 wordLen, contentLen;
  PRUnichar* bp = tx.GetNextWord(wordLen, contentLen, isWhitespace);
  if ((nsnull == bp) || isWhitespace) {
    // Don't bother measuring nothing
    aStop = PR_TRUE;
    return 0;
  }
  aStop = contentLen < tx.GetContentLength();

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
    if (NS_STYLE_FONT_VARIANT_SMALL_CAPS == font->mFont.variant) {
      MeasureSmallCapsText(aReflowState, *font, buf, wordLen, width);
    }
    else {
      rc.GetWidth(buf, wordLen, width);
    }
    rc.SetFont(oldfm);
    NS_RELEASE(oldfm);
    NS_RELEASE(fm);

#ifdef DEBUG_WORD_WRAPPING
    nsAutoString tmp(bp, wordLen);
    printf("  fragment='");
    fputs(tmp, stdout);
    printf("' width=%d [wordLen=%d contentLen=%d ContentLength=%d]\n",
           width, wordLen, contentLen, tx.GetContentLength());
#endif

    // Remember the text frame for later so that we don't bother doing
    // the word look ahead.
    aLineLayout.RecordWordFrame(aTextFrame);
    return width;
  }

  aStop = PR_TRUE;
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

  // Use text transformer to transform the next line of PRE content
  // into its expanded form (it exands tabs, translates NBSP's, etc.).
  PRInt32 column = aLineLayout.GetColumn();
  mColumn = column;
  PRInt32 lineLen, contentLen;
  PRUnichar buf[TEXT_BUF_SIZE];
  nsTextTransformer tx(buf, TEXT_BUF_SIZE);
  tx.Init(this, aStartingOffset);

  // Setup font for measuring with
  nscoord spaceWidth;
  nsIFontMetrics* fm = aLineLayout.mPresContext.GetMetricsFor(aFont.mFont);
  aReflowState.rendContext->SetFont(fm);
  aReflowState.rendContext->GetWidth(' ', spaceWidth);
  nscoord width = 0;

  // Transform each section of the content. The transformer stops on
  // tabs and newlines.
  nscoord totalLen = 0;
  for (;;) {
    PRUnichar* bp = tx.GetNextSection(lineLen, contentLen);
    if (nsnull == bp) {
      break;
    }
    totalLen += contentLen;
    if (0 == lineLen) {
      break;
    }
    if ('\t' == bp[0]) {
      PRInt32 spaces = 8 - (7 & column);
      width += spaceWidth * spaces;
    }
    else {
      nscoord sectionWidth;
      aReflowState.rendContext->GetWidth(bp, lineLen, sectionWidth);
      width += sectionWidth;
    }
  }
  if (tx.HasMultibyte()) {
    mFlags |= TEXT_HAS_MULTIBYTE;
  }
  aLineLayout.SetColumn(column);

  // Record content offset information
  mContentOffset = aStartingOffset;
  mContentLength = totalLen;
  mFlags |= TEXT_IS_PRE;

  aMetrics.width = width;
  fm->GetHeight(aMetrics.height);
  fm->GetMaxAscent(aMetrics.ascent);
  fm->GetMaxDescent(aMetrics.descent);
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  NS_RELEASE(fm);

  if (aStartingOffset + totalLen < tx.GetContentLength()) {
    rs = NS_FRAME_NOT_COMPLETE;
  }
  return rs;
}

// Translate the mapped content into a string that's printable
void
TextFrame::ToCString(nsString& aBuf, PRInt32& aContentLength) const
{
  const nsTextFragment* frag;
  PRInt32 numFrags;

  // Get the frames text content
  nsITextContent* tc;
  if (NS_OK != mContent->QueryInterface(kITextContentIID, (void**) &tc)) {
    return;
  }
  tc->GetText(frag, numFrags);
  NS_RELEASE(tc);

  // Compute the total length of the text content.
  PRInt32 sum = 0;
  PRInt32 i, n = numFrags;
  for (i = 0; i < n; i++) {
    sum += frag[i].GetLength();
  }
  aContentLength = sum;

  // Set current fragment and current fragment offset
  PRInt32 fragOffset = 0, offset = 0;
  n = numFrags;
  while (--n >= 0) {
    if (mContentOffset < offset + frag->GetLength()) {
      fragOffset = mContentOffset - offset;
      break;
    }
    offset += frag->GetLength();
    frag++;
  }

  if (0 == mContentLength) {
    return;
  }

  n = mContentLength;
  for (;;) {
    PRUnichar ch = frag->CharAt(fragOffset);
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
    if (--n == 0) {
      break;
    }
    if (++fragOffset == frag->GetLength()) {
      frag++;
      fragOffset = 0;
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
TextFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
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

  PRInt32 contentLength;
  nsAutoString tmp;
  ToCString(tmp, contentLength);

  // Output the first/last content offset and prev/next in flow info
  PRBool isComplete = (mContentOffset + mContentLength) == contentLength;
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
  fputs(tmp, out);
  fputs("\"\n", out);

  aIndent--;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);

  return NS_OK;
}

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
#include "nsHTMLAtoms.h"

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

// XXX these are a copy of nsTextTransformer's version and they aren't I18N'd!
#define XP_IS_LOWERCASE(_ch) \
  (((_ch) >= 'a') && ((_ch) <= 'z'))

#define XP_TO_UPPER(_ch) ((_ch) & ~32)

#define XP_IS_SPACE(_ch) \
  (((_ch) == ' ') || ((_ch) == '\t') || ((_ch) == '\n'))

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

  NS_IMETHOD GetCursor(nsIPresContext& aPresContext,
                       nsPoint& aPoint,
                       PRInt32& aCursor);

  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);

  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const;

  NS_IMETHOD GetFrameName(nsString& aResult) const;

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
  NS_IMETHOD AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace);
  NS_IMETHOD TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth);

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

  struct TextStyle {
    const nsStyleFont* mFont;
    const nsStyleText* mText;
    const nsStyleColor* mColor;
    nsIFontMetrics* mNormalFont;
    nsIFontMetrics* mSmallFont;
    nsIFontMetrics* mLastFont;
    PRBool mSmallCaps;
    nscoord mWordSpacing;
    nscoord mLetterSpacing;
    nscolor mSelectionTextColor;
    nscolor mSelectionBGColor;
    nscoord mSpaceWidth;
    PRBool mJustifying;
    PRIntn mNumSpaces;
    nscoord mExtraSpacePerSpace;
    nscoord mRemainingExtraSpace;

    TextStyle(nsIPresContext& aPresContext,
              nsIRenderingContext& aRenderingContext,
              nsIStyleContext* sc)
    {
      // Get style data
      mColor = (const nsStyleColor*) sc->GetStyleData(eStyleStruct_Color);
      mFont = (const nsStyleFont*) sc->GetStyleData(eStyleStruct_Font);
      mText = (const nsStyleText*) sc->GetStyleData(eStyleStruct_Text);
      aRenderingContext.SetColor(mColor->mColor);

      // Get the normal font
      nsFont plainFont(mFont->mFont);
      plainFont.decorations = NS_FONT_DECORATION_NONE;
      mNormalFont = aPresContext.GetMetricsFor(plainFont);
      aRenderingContext.SetFont(mNormalFont);
      aRenderingContext.GetWidth(' ', mSpaceWidth);
      mLastFont = mNormalFont;

      // Get the small-caps font if needed
      mSmallCaps = NS_STYLE_FONT_VARIANT_SMALL_CAPS == plainFont.variant;
      if (mSmallCaps) {
        plainFont.size = nscoord(0.7 * plainFont.size);
        mSmallFont = aPresContext.GetMetricsFor(plainFont);
      }
      else {
        mSmallFont = nsnull;
      }

      // XXX Get these from style
      mSelectionBGColor = NS_RGB(0, 0, 0);
      mSelectionTextColor = NS_RGB(255, 255, 255);

      // Get the word and letter spacing
      mWordSpacing = 0;
      mLetterSpacing = 0;
      if (NS_STYLE_WHITESPACE_PRE != mText->mWhiteSpace) {
        PRIntn unit = mText->mWordSpacing.GetUnit();
        if (eStyleUnit_Coord == unit) {
          mWordSpacing = mText->mWordSpacing.GetCoordValue();
        }
        unit = mText->mLetterSpacing.GetUnit();
        if (eStyleUnit_Coord == unit) {
          mLetterSpacing = mText->mLetterSpacing.GetCoordValue();
        }
      }
      mNumSpaces = 0;
      mRemainingExtraSpace = 0;
      mExtraSpacePerSpace = 0;
    }

    ~TextStyle() {
      NS_RELEASE(mNormalFont);
      NS_IF_RELEASE(mSmallFont);
    }
  };

  PRIntn PrepareUnicodeText(nsIRenderingContext& aRenderingContext,
                            nsTextTransformer& aTransformer,
                            PRInt32* aIndicies,
                            PRUnichar* aBuffer,
                            PRInt32& aTextLen,
                            nscoord& aNewWidth);

  void PaintTextDecorations(nsIRenderingContext& aRenderingContext,
                            nsIStyleContext* aStyleContext,
                            TextStyle& aStyle,
                            nscoord aX, nscoord aY, nscoord aWidth);

  void PaintTextSlowly(nsIPresContext& aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       nsIStyleContext* aStyleContext,
                       TextStyle& aStyle,
                       nscoord aX, nscoord aY);

  void RenderString(nsIRenderingContext& aRenderingContext,
                    nsIStyleContext* aStyleContext,
                    TextStyle& aStyle,
                    PRUnichar* aBuffer, PRInt32 aLength,
                    nscoord aX, nscoord aY,
                    nscoord aWidth);

  void MeasureSmallCapsText(const nsHTMLReflowState& aReflowState,
                            TextStyle& aStyle,
                            PRUnichar* aWord,
                            PRInt32 aWordLength,
                            nscoord& aWidthResult);

  void GetWidth(nsIRenderingContext& aRenderingContext,
                TextStyle& aStyle,
                PRUnichar* aBuffer, PRInt32 aLength,
                nscoord& aWidthResult);

  void PaintUnicodeText(nsIPresContext& aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        nsIStyleContext* aStyleContext,
                        TextStyle& aStyle,
                        nscoord dx, nscoord dy);

  void PaintAsciiText(nsIPresContext& aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      nsIStyleContext* aStyleContext,
                      TextStyle& aStyle,
                      nscoord dx, nscoord dy);

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
  nscoord mComputedWidth;
};

// Flag information used by rendering code. This information is
// computed by the ResizeReflow code. Remaining bits are used by the
// tab count.

// Flag indicating that whitespace was skipped
#define TEXT_SKIP_LEADING_WS 0x01

#define TEXT_HAS_MULTIBYTE   0x02

#define TEXT_BLINK_ON        0x04

#define TEXT_IN_WORD         0x08

#define TEXT_TRIMMED_WS      0x10

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
TextFrame::GetCursor(nsIPresContext& aPresContext,
                     nsPoint& aPoint,
                     PRInt32& aCursor)
{
  const nsStyleColor* styleColor;
  GetStyleData(eStyleStruct_Color, (const nsStyleStruct*&)styleColor);
  aCursor = styleColor->mCursor;

  if (NS_STYLE_CURSOR_AUTO == aCursor && nsnull != mGeometricParent) {
    mGeometricParent->GetCursor(aPresContext, aPoint, aCursor);
    if (NS_STYLE_CURSOR_AUTO == aCursor) {
      aCursor = NS_STYLE_CURSOR_TEXT;
    }
  }
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

  nsIStyleContext* sc = mStyleContext;
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    sc->GetStyleData(eStyleStruct_Display);
  if (disp->mVisible) {
    TextStyle ts(aPresContext, aRenderingContext, mStyleContext);
    if (ts.mSmallCaps || (0 != ts.mWordSpacing) || (0 != ts.mLetterSpacing) ||
        ((NS_STYLE_TEXT_ALIGN_JUSTIFY == ts.mText->mTextAlign) &&
         (mRect.width > mComputedWidth))) {
      PaintTextSlowly(aPresContext, aRenderingContext, sc, ts, 0, 0);
    }
    else {
      // Choose rendering pathway based on rendering context
      // performance hint.
      PRUint32 hints = 0;
      aRenderingContext.GetHints(hints);
      if ((TEXT_HAS_MULTIBYTE & mFlags) ||
          (0 == (hints & NS_RENDERING_HINT_FAST_8BIT_TEXT))) {
        // Use PRUnichar rendering routine
        PaintUnicodeText(aPresContext, aRenderingContext, sc, ts, 0, 0);
      }
      else {
        // Use char rendering routine
        PaintAsciiText(aPresContext, aRenderingContext, sc, ts, 0, 0);
      }
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
PRIntn
TextFrame::PrepareUnicodeText(nsIRenderingContext& aRenderingContext,
                              nsTextTransformer& aTX,
                              PRInt32* aIndexes,
                              PRUnichar* aBuffer,
                              PRInt32& aTextLen,
                              nscoord& aNewWidth)
{
  PRIntn numSpaces = 0;
  PRUnichar* dst = aBuffer;

  // Setup transform to operate starting in the content at our content
  // offset
  aTX.Init(this, mContentOffset);

  PRInt32 mappingInx = 0;
  PRInt32 strInx = mContentOffset;

  // Skip over the leading whitespace
  PRInt32 n = mContentLength;
  if (0 != (mFlags & TEXT_SKIP_LEADING_WS)) {
    PRBool isWhitespace;
    PRInt32 wordLen, contentLen;
    aTX.GetNextWord(PR_FALSE, wordLen, contentLen, isWhitespace);
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

  // Rescan the content and transform it. Stop when we have consumed
  // mContentLength characters.
  PRBool inWord = (TEXT_IN_WORD & mFlags) ? PR_TRUE : PR_FALSE;
  PRInt32 column = mColumn;
  PRInt32 textLength = 0;
  while (0 != n) {
    PRUnichar* bp;
    PRBool isWhitespace;
    PRInt32 wordLen, contentLen;

    // Get the next word
    bp = aTX.GetNextWord(inWord, wordLen, contentLen, isWhitespace);
    if (nsnull == bp) {
      break;
    }
    if (contentLen > n) {
      contentLen = n;
    }
    if (wordLen > n) {
      wordLen = n;
    }
    inWord = PR_FALSE;
    if (isWhitespace) {
      numSpaces++;
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
      else if (0 == wordLen) {
        break;
      }
      else if (nsnull != aIndexes) {
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
    column += wordLen;
    textLength += wordLen;
    n -= contentLen;
    nsCRT::memcpy(dst, bp, sizeof(PRUnichar) * wordLen);
    dst += wordLen;
  }

  // Remove trailing whitespace if it was trimmed after reflow
  if (TEXT_TRIMMED_WS & mFlags) {
    if (--dst >= aBuffer) {
      PRUnichar ch = *dst;
      if (XP_IS_SPACE(ch)) {
        textLength--;
      }
    }
    numSpaces--;
  }

  aTextLen = textLength;
  return numSpaces;
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
                                nsIStyleContext* aStyleContext,
                                TextStyle& aTextStyle,
                                nscoord aX, nscoord aY, nscoord aWidth)
{
  nscolor overColor;
  nscolor underColor;
  nscolor strikeColor;
  nsIStyleContext*  context = aStyleContext;
  PRUint8 decorations = aTextStyle.mFont->mFont.decorations;
  PRUint8 decorMask = decorations;

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
  aTextStyle.mNormalFont->GetMaxAscent(baseline);
  if (decorations & (NS_FONT_DECORATION_OVERLINE | NS_FONT_DECORATION_UNDERLINE)) {
    aTextStyle.mNormalFont->GetUnderline(offset, size);
    if (decorations & NS_FONT_DECORATION_OVERLINE) {
      aRenderingContext.SetColor(overColor);
      aRenderingContext.FillRect(aX, aY, aWidth, size);
    }
    if (decorations & NS_FONT_DECORATION_UNDERLINE) {
      aRenderingContext.SetColor(underColor);
      aRenderingContext.FillRect(aX, aY + baseline - offset, aWidth, size);
    }
  }
  if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
    aTextStyle.mNormalFont->GetStrikeout(offset, size);
    aRenderingContext.SetColor(strikeColor);
    aRenderingContext.FillRect(aX, aY + baseline - offset, aWidth, size);
  }
}

void
TextFrame::PaintUnicodeText(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            nsIStyleContext* aStyleContext,
                            TextStyle& aTextStyle,
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
      PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                           dx, dy, width);
    }
    else {
      SelectionInfo si;
      ComputeSelectionInfo(aRenderingContext, doc, ip, textLength, si);

      nscoord textWidth;
      if (si.mEmptySelection) {
        aRenderingContext.DrawString(text, textLength, dx, dy, width);
        PaintTextDecorations(aRenderingContext, aStyleContext,
                             aTextStyle, dx, dy, width);
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
          PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                               x, dy, textWidth);
          x += textWidth;
        }
        PRInt32 secondLen = si.mEndOffset - si.mStartOffset;
        if (0 != secondLen) {
          // Get the width of the second (selected) section
          aRenderingContext.GetWidth(text + si.mStartOffset,
                                     PRUint32(secondLen), textWidth);

          // Render second (selected) section
          aRenderingContext.SetColor(aTextStyle.mSelectionBGColor);
          aRenderingContext.FillRect(x, dy, textWidth, mRect.height);
          aRenderingContext.SetColor(aTextStyle.mSelectionTextColor);
          aRenderingContext.DrawString(text + si.mStartOffset, secondLen,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                               x, dy, textWidth);
          aRenderingContext.SetColor(aTextStyle.mColor->mColor);
          x += textWidth;
        }
        if (textLength != si.mEndOffset) {
          PRInt32 thirdLen = textLength - si.mEndOffset;

          // Render third (unselected) section
          aRenderingContext.GetWidth(text + si.mEndOffset, PRUint32(thirdLen),
                                     textWidth);
          aRenderingContext.DrawString(text + si.mEndOffset,
                                       thirdLen, x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                               x, dy, textWidth);
        }
      }
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
TextFrame::RenderString(nsIRenderingContext& aRenderingContext,
                        nsIStyleContext* aStyleContext,
                        TextStyle& aTextStyle,
                        PRUnichar* aBuffer, PRInt32 aLength,
                        nscoord aX, nscoord aY,
                        nscoord aWidth)
{
  PRUnichar buf[TEXT_BUF_SIZE];
  PRUnichar* bp0 = buf;
  if (aLength > TEXT_BUF_SIZE) {
    bp0 = new PRUnichar[aLength];
  }
  PRUnichar* bp = bp0;

  PRBool spacing = (0 != aTextStyle.mLetterSpacing) ||
    (0 != aTextStyle.mWordSpacing) || (mRect.width > mComputedWidth);
  nscoord spacingMem[TEXT_BUF_SIZE];
  PRIntn* sp0 = spacingMem; 
  if (spacing && (aLength > TEXT_BUF_SIZE)) {
    sp0 = new nscoord[aLength];
  }
  PRIntn* sp = sp0;

  nscoord smallY = aY;
  if (aTextStyle.mSmallCaps) {
    nscoord normalAscent, smallAscent;
    aTextStyle.mNormalFont->GetMaxAscent(normalAscent);
    aTextStyle.mSmallFont->GetMaxAscent(smallAscent);
    if (normalAscent > smallAscent) {
      smallY = aY + normalAscent - smallAscent;
    }
  }

  nsIFontMetrics* lastFont = aTextStyle.mLastFont;
  nscoord lastY = aY;
  if (lastFont == aTextStyle.mSmallFont) {
    lastY = smallY;
  }
  PRInt32 pendingCount;
  PRUnichar* runStart = bp;
  nscoord charWidth, width = 0;
  for (; --aLength >= 0; aBuffer++) {
    nsIFontMetrics* nextFont;
    nscoord nextY, glyphWidth;
    PRUnichar ch = *aBuffer;
    if (aTextStyle.mSmallCaps && XP_IS_LOWERCASE(ch)) {
      nextFont = aTextStyle.mSmallFont;
      nextY = smallY;
      ch = XP_TO_UPPER(ch);
      if (lastFont != aTextStyle.mSmallFont) {
        aRenderingContext.SetFont(aTextStyle.mSmallFont);
        aRenderingContext.GetWidth(ch, charWidth);
        aRenderingContext.SetFont(aTextStyle.mNormalFont);
      }
      else {
        aRenderingContext.GetWidth(ch, charWidth);
      }
      glyphWidth = charWidth + aTextStyle.mLetterSpacing;
    }
    else if (ch == ' ') {
      nextFont = aTextStyle.mNormalFont;
      nextY = aY;
      glyphWidth = aTextStyle.mSpaceWidth + aTextStyle.mWordSpacing;
      nscoord extra = aTextStyle.mExtraSpacePerSpace;
      if (--aTextStyle.mNumSpaces == 0) {
        extra += aTextStyle.mRemainingExtraSpace;
      }
      glyphWidth += extra;
    }
    else {
      if (lastFont != aTextStyle.mNormalFont) {
        aRenderingContext.SetFont(aTextStyle.mNormalFont);
        aRenderingContext.GetWidth(ch, charWidth);
        aRenderingContext.SetFont(aTextStyle.mSmallFont);
      }
      else {
        aRenderingContext.GetWidth(ch, charWidth);
      }
      nextFont = aTextStyle.mNormalFont;
      nextY = aY;
      glyphWidth = charWidth + aTextStyle.mLetterSpacing;
    }
    if (nextFont != lastFont) {
      pendingCount = bp - runStart;
      if (0 != pendingCount) {
        // Measure previous run of characters using the previous font
        aRenderingContext.DrawString(runStart, pendingCount,
                                     aX, lastY, width,
                                     spacing ? sp0 : nsnull);

        // Note: use aY not small-y so that decorations are drawn with
        // respect to the normal-font not the current font.
        PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                             aX, aY, width);

        aWidth -= width;
        aX += width;
        runStart = bp = bp0;
        sp = sp0;
        width = 0;
      }
      aRenderingContext.SetFont(nextFont);
      lastFont = nextFont;
      lastY = nextY;
    }
    *bp++ = ch;
    *sp++ = glyphWidth;
    width += glyphWidth;
  }
  pendingCount = bp - runStart;
  if (0 != pendingCount) {
    // Measure previous run of characters using the previous font
    aRenderingContext.DrawString(runStart, pendingCount, aX, lastY, width,
                                 spacing ? sp0 : nsnull);

    // Note: use aY not small-y so that decorations are drawn with
    // respect to the normal-font not the current font.
    PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                         aX, aY, aWidth);
  }
  aTextStyle.mLastFont = lastFont;

  if (bp0 != buf) {
    delete [] bp0;
  }
  if (sp0 != spacingMem) {
    delete [] sp0;
  }
}

inline void
TextFrame::MeasureSmallCapsText(const nsHTMLReflowState& aReflowState,
                                TextStyle& aTextStyle,
                                PRUnichar* aWord,
                                PRInt32 aWordLength,
                                nscoord& aWidthResult)
{
  nsIRenderingContext& rc = *aReflowState.rendContext;
  GetWidth(rc, aTextStyle, aWord, aWordLength, aWidthResult);
  if (aTextStyle.mLastFont != aTextStyle.mNormalFont) {
    rc.SetFont(aTextStyle.mNormalFont);
    aTextStyle.mLastFont = aTextStyle.mNormalFont;
  }
}

// XXX factor in logic from RenderString into here; gaps, justification, etc.
void
TextFrame::GetWidth(nsIRenderingContext& aRenderingContext,
                    TextStyle& aTextStyle,
                    PRUnichar* aBuffer, PRInt32 aLength,
                    nscoord& aWidthResult)
{
  PRUnichar buf[TEXT_BUF_SIZE];
  PRUnichar* bp0 = buf;
  if (aLength > TEXT_BUF_SIZE) {
    bp0 = new PRUnichar[aLength];
  }
  PRUnichar* bp = bp0;

  nsIFontMetrics* lastFont = aTextStyle.mLastFont;
  nscoord w, sum = 0;
  PRInt32 pendingCount;
  PRUnichar* runStart = bp;
  for (; --aLength >= 0; aBuffer++) {
    nsIFontMetrics* nextFont;
    PRUnichar ch = *aBuffer;
    if (aTextStyle.mSmallCaps && XP_IS_LOWERCASE(ch)) {
      nextFont = aTextStyle.mSmallFont;
      ch = XP_TO_UPPER(ch);
    }
    else {
      nextFont = aTextStyle.mNormalFont;
    }
    if (nextFont != lastFont) {
      pendingCount = bp - runStart;
      if (0 != pendingCount) {
        // Measure previous run of characters using the previous font
        aRenderingContext.GetWidth(runStart, pendingCount, w);
        sum += w;
        runStart = bp;
      }
      aRenderingContext.SetFont(nextFont);
      lastFont = nextFont;
    }
    *bp++ = ch;
  }
  pendingCount = bp - runStart;
  if (0 != pendingCount) {
    // Measure previous run of characters using the previous font
    aRenderingContext.GetWidth(runStart, pendingCount, w);
    sum += w;
  }
  if (bp0 != buf) {
    delete [] bp0;
  }
  aTextStyle.mLastFont = lastFont;
  aWidthResult = sum;
}

void
TextFrame::PaintTextSlowly(nsIPresContext& aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           nsIStyleContext* aStyleContext,
                           TextStyle& aTextStyle,
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
  aTextStyle.mNumSpaces = PrepareUnicodeText(aRenderingContext, tx,
                                             displaySelection ? ip : nsnull,
                                             paintBuf, textLength, width);
  if (mRect.width > mComputedWidth) {
    if (0 != aTextStyle.mNumSpaces) {
      nscoord extra = mRect.width - mComputedWidth;
      nscoord adjustPerSpace =
        aTextStyle.mExtraSpacePerSpace = extra / aTextStyle.mNumSpaces;
      aTextStyle.mRemainingExtraSpace = extra -
        (aTextStyle.mExtraSpacePerSpace * aTextStyle.mNumSpaces);
    }
    else {
      // We have no whitespace but were given some extra space. There
      // are two plausible places to put the extra space: to the left
      // and to the right. If this is anywhere but the last place on
      // the line then the correct answer is to the right.
    }
  }

  PRUnichar* text = paintBuf;
  if (0 != textLength) {
    if (!displaySelection) {
      // When there is no selection showing, use the fastest and
      // simplest rendering approach
      RenderString(aRenderingContext, aStyleContext, aTextStyle,
                   text, textLength, dx, dy, width);
    }
    else {
      SelectionInfo si;
      ComputeSelectionInfo(aRenderingContext, doc, ip, textLength, si);

      nscoord textWidth;
      if (si.mEmptySelection) {
        RenderString(aRenderingContext, aStyleContext, aTextStyle,
                     text, textLength, dx, dy, width);
        GetWidth(aRenderingContext, aTextStyle,
                 text, PRUint32(si.mStartOffset), textWidth);
        RenderSelectionCursor(aRenderingContext,
                              dx + textWidth, dy, mRect.height,
                              CURSOR_COLOR);
      }
      else {
        nscoord x = dx;

        if (0 != si.mStartOffset) {
          // Render first (unselected) section
          GetWidth(aRenderingContext, aTextStyle,
                   text, PRUint32(si.mStartOffset),
                   textWidth);
          RenderString(aRenderingContext, aStyleContext, aTextStyle,
                       text, si.mStartOffset,
                       x, dy, textWidth);
          x += textWidth;
        }
        PRInt32 secondLen = si.mEndOffset - si.mStartOffset;
        if (0 != secondLen) {
          // Get the width of the second (selected) section
          GetWidth(aRenderingContext, aTextStyle,
                   text + si.mStartOffset,
                   PRUint32(secondLen), textWidth);

          // Render second (selected) section
          aRenderingContext.SetColor(aTextStyle.mSelectionBGColor);
          aRenderingContext.FillRect(x, dy, textWidth, mRect.height);
          aRenderingContext.SetColor(aTextStyle.mSelectionTextColor);
          RenderString(aRenderingContext, aStyleContext, aTextStyle,
                       text + si.mStartOffset, secondLen,
                       x, dy, textWidth);
          aRenderingContext.SetColor(aTextStyle.mColor->mColor);
          x += textWidth;
        }
        if (textLength != si.mEndOffset) {
          PRInt32 thirdLen = textLength - si.mEndOffset;

          // Render third (unselected) section
          GetWidth(aRenderingContext, aTextStyle,
                   text + si.mEndOffset, PRUint32(thirdLen),
                   textWidth);
          RenderString(aRenderingContext, aStyleContext, aTextStyle,
                       text + si.mEndOffset,
                       thirdLen, x, dy, textWidth);
        }
      }
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
                          nsIStyleContext* aStyleContext,
                          TextStyle& aTextStyle,
                          nscoord dx, nscoord dy)
{
  nsIPresShell* shell = aPresContext.GetShell();
  nsIDocument* doc = shell->GetDocument();
  PRBool displaySelection;
  displaySelection = doc->GetDisplaySelection();

  // Make enough space to transform
  PRUnichar wordBufMem[WORD_BUF_SIZE];
  char paintBufMem[TEXT_BUF_SIZE];
  PRUnichar rawPaintBufMem[TEXT_BUF_SIZE];
  PRInt32 indicies[TEXT_BUF_SIZE];
  PRInt32* ip = indicies;
  char* paintBuf = paintBufMem;
  PRUnichar* rawPaintBuf = rawPaintBufMem;
  if (mContentLength > TEXT_BUF_SIZE) {
    ip = new PRInt32[mContentLength];
    paintBuf = new char[mContentLength];
    rawPaintBuf = new PRUnichar[mContentLength];
  }
  nscoord width = mRect.width;
  PRInt32 textLength;

  // Transform text from content into renderable form
  nsTextTransformer tx(wordBufMem, WORD_BUF_SIZE);
  PrepareUnicodeText(aRenderingContext, tx,
                     displaySelection ? ip : nsnull,
                     rawPaintBuf, textLength, width);

  // Translate unicode data into ascii for rendering
  char* dst = paintBuf;
  char* end = dst + textLength;
  PRUnichar* src = rawPaintBuf;
  while (dst < end) {
    *dst++ = (char) ((unsigned char) *src++);
  }

  char* text = paintBuf;
  if (0 != textLength) {
    if (!displaySelection) {
      // When there is no selection showing, use the fastest and
      // simplest rendering approach
      aRenderingContext.DrawString(text, textLength, dx, dy, width);
      PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                           dx, dy, width);
    }
    else {
      SelectionInfo si;
      ComputeSelectionInfo(aRenderingContext, doc, ip, textLength, si);

      nscoord textWidth;
      if (si.mEmptySelection) {
        aRenderingContext.DrawString(text, textLength, dx, dy, width);
        PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                             dx, dy, width);
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
          PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                               x, dy, textWidth);
          x += textWidth;
        }
        PRInt32 secondLen = si.mEndOffset - si.mStartOffset;
        if (0 != secondLen) {
          // Get the width of the second (selected) section
          aRenderingContext.GetWidth(text + si.mStartOffset,
                                     PRUint32(secondLen), textWidth);

          // Render second (selected) section
          aRenderingContext.SetColor(aTextStyle.mSelectionBGColor);
          aRenderingContext.FillRect(x, dy, textWidth, mRect.height);
          aRenderingContext.SetColor(aTextStyle.mSelectionTextColor);
          aRenderingContext.DrawString(text + si.mStartOffset, secondLen,
                                       x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                               x, dy, textWidth);
          aRenderingContext.SetColor(aTextStyle.mColor->mColor);
          x += textWidth;
        }
        if (textLength != si.mEndOffset) {
          PRInt32 thirdLen = textLength - si.mEndOffset;

          // Render third (unselected) section
          aRenderingContext.GetWidth(text + si.mEndOffset, PRUint32(thirdLen),
                                     textWidth);
          aRenderingContext.DrawString(text + si.mEndOffset,
                                       thirdLen, x, dy, textWidth);
          PaintTextDecorations(aRenderingContext, aStyleContext, aTextStyle,
                               x, dy, textWidth);
        }
      }
    }
  }

  // Cleanup
  if (paintBuf != paintBufMem) {
    delete [] paintBuf;
  }
  if (rawPaintBuf != rawPaintBufMem) {
    delete [] rawPaintBuf;
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

  // Find the font metrics for this text
  nsIStyleContext* styleContext;
  aNewFrame->GetStyleContext(styleContext);
  const nsStyleFont *font = (const nsStyleFont*)
    styleContext->GetStyleData(eStyleStruct_Font);
  NS_RELEASE(styleContext);
  nsIFontMetrics* fm = aCX.GetMetricsFor(font->mFont);
  aRendContext->SetFont(fm);
  PRBool smallCaps = NS_STYLE_FONT_VARIANT_SMALL_CAPS == font->mFont.variant;

  // Get the renderable form of the text
  nsTextTransformer tx(wordBufMem, WORD_BUF_SIZE);
  PrepareUnicodeText(*aRendContext, tx,
                     ip, paintBuf, textLength, width);
  ip[mContentLength] = ip[mContentLength-1]+1;

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
  //  NS_PRECONDITION(nsnull != aReflowState.lineLayout, "no line layout");
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter TextFrame::Reflow: aMaxSize=%d,%d",
      aReflowState.maxSize.width, aReflowState.maxSize.height));

  // XXX If there's no line layout, we shouldn't even have created this
  // frame. This may happen if, for example, this is text inside a table
  // but not inside a cell. For now, just don't reflow.
  if (nsnull == aReflowState.lineLayout) {
    return NS_OK;
  }

  // Get starting offset into the content
  PRInt32 startingOffset = 0;
  if (nsnull != mPrevInFlow) {
    TextFrame* prev = (TextFrame*) mPrevInFlow;
    startingOffset = prev->mContentOffset + prev->mContentLength;
  }

  nsLineLayout& lineLayout = *aReflowState.lineLayout;
  TextStyle ts(aPresContext, *aReflowState.rendContext, mStyleContext);

  // Initialize mFlags (without destroying the TEXT_BLINK_ON bit) bits
  // that are filled in by the reflow routines.
  mFlags &= TEXT_BLINK_ON;
  if (ts.mFont->mFont.decorations & NS_STYLE_TEXT_DECORATION_BLINK) {
    if (0 == (mFlags & TEXT_BLINK_ON)) {
      mFlags |= TEXT_BLINK_ON;
      gTextBlinker->AddFrame(this);
    }
  }
  else {
    if (0 != (mFlags & TEXT_BLINK_ON)) {
      mFlags &= ~TEXT_BLINK_ON;
      gTextBlinker->RemoveFrame(this);
    }
  }

  PRBool wrapping = NS_STYLE_WHITESPACE_NORMAL == ts.mText->mWhiteSpace;
  PRBool firstLetterOK = lineLayout.GetFirstLetterStyleOK();
  PRBool justDidFirstLetter = PR_FALSE;

  // Set whitespace skip flag
  PRBool skipWhitespace = PR_FALSE;
  if (NS_STYLE_WHITESPACE_PRE != ts.mText->mWhiteSpace) {
    if (lineLayout.GetEndsInWhiteSpace()) {
      skipWhitespace = PR_TRUE;
    }
  }

  nscoord x = 0;
  nscoord maxWidth = aReflowState.maxSize.width;
  nscoord maxWordWidth = 0;
  nscoord prevMaxWordWidth = 0;
  PRBool endsInWhitespace = PR_FALSE;
  PRBool endsInNewline = PR_FALSE;
  PRBool firstWord = PR_TRUE;

  // Setup text transformer to transform this frames text content
  nsTextRun* textRun = lineLayout.FindTextRunFor(this);
  PRUnichar wordBuf[WORD_BUF_SIZE];
  nsTextTransformer tx(wordBuf, WORD_BUF_SIZE);
  nsresult rv = tx.Init(/**textRun, XXX*/ this, startingOffset);
  if (NS_OK != rv) {
    return rv;
  }
  PRInt32 contentLength = tx.GetContentLength();

  // Offset tracks how far along we are in the content
  PRInt32 offset = startingOffset;
  PRInt32 prevOffset = -1;
  nscoord lastWordWidth = 0;

  // Loop over words and whitespace in content and measure. Set inWord
  // to true if we are part of a previous piece of text's word. This
  // is only valid for one pass through the measuring loop.
  PRBool inWord = lineLayout.InWord();
  if (inWord) {
    mFlags |= TEXT_IN_WORD;
  }
  PRInt32 column = lineLayout.GetColumn();
  PRInt32 prevColumn = column;
  mColumn = column;
  PRBool breakable = 0 != lineLayout.GetPlacedFrames();
  for (;;) {
    // Get next word/whitespace from the text
    PRBool isWhitespace;
    PRInt32 wordLen, contentLen;
    PRUnichar* bp = tx.GetNextWord(inWord, wordLen, contentLen, isWhitespace);
    if (nsnull == bp) {
      break;
    }
    inWord = PR_FALSE;

    // Measure the word/whitespace
    nscoord width;
    if (isWhitespace) {
      if (0 == wordLen) {
        // We hit a newline. Stop looping.
        prevOffset = offset;
        offset++;
        endsInWhitespace = PR_TRUE;
        endsInNewline = PR_TRUE;
        break;
      }
      if (skipWhitespace) {
        offset += contentLen;
        skipWhitespace = PR_FALSE;

        // Only set flag when we actually do skip whitespace
        mFlags |= TEXT_SKIP_LEADING_WS;
        continue;
      }
      if ('\t' == bp[0]) {
        // Expand tabs to the proper width
        wordLen = 8 - (7 & column);
        width = ts.mSpaceWidth * wordLen;
      }
      else {
        width = ts.mSpaceWidth + ts.mWordSpacing;/* XXX simplistic */
      }
      breakable = PR_TRUE;
      firstLetterOK = PR_FALSE;
    } else {
      if (firstLetterOK) {
        // XXX need a lookup function here; plus look ahead using the
        // text-runs
        if ((bp[0] == '\'') || (bp[0] == '"')) {
          wordLen = 2;
          contentLen = 2;
        }
        else {
          wordLen = 1;
          contentLen = 1;
        }
        justDidFirstLetter = PR_TRUE;
      }
      if (ts.mSmallCaps) {
        MeasureSmallCapsText(aReflowState, ts, bp, wordLen, width);
      }
      else {
        aReflowState.rendContext->GetWidth(bp, wordLen, width);
      }
      if (ts.mLetterSpacing) {
        width += ts.mLetterSpacing * wordLen;
      }
      skipWhitespace = PR_FALSE;
      lastWordWidth = width;
    }

    // See if there is room for the text
    if ((0 != x) && wrapping && (x + width > maxWidth)) {
      // The text will not fit.
      break;
    }
    prevColumn = column;
    column += wordLen;
    x += width;
    prevMaxWordWidth = maxWordWidth;
    if (width > maxWordWidth) {
      maxWordWidth = width;
    }
    endsInWhitespace = isWhitespace;
    prevOffset = offset;
    offset += contentLen;
    if (!isWhitespace && justDidFirstLetter) {
      // Time to stop
      break;
    }
  }
  if (tx.HasMultibyte()) {
    mFlags |= TEXT_HAS_MULTIBYTE;
  }

  // Post processing logic to deal with word-breaking that spans
  // multiple frames.
  if (lineLayout.InWord()) {
    // We are already in a word. This means a text frame prior to this
    // one had a fragment of a nbword that is joined with this
    // frame. It also means that the prior frame already found this
    // frame and recorded it as part of the word.
#ifdef DEBUG_WORD_WRAPPING
    ListTag(stdout);
    printf(": in word; skipping\n");
#endif
    lineLayout.ForgetWordFrame(this);
  }
  else {
    // There is no currently active word. This frame may contain the
    // start of one.
    if (endsInWhitespace) {
      // Nope, this frame doesn't start a word.
      lineLayout.ForgetWordFrames();
    }
    else if ((offset == contentLength) && (prevOffset >= 0)) {
      // Force breakable to false when we aren't wrapping (this
      // guarantees that the combined word will stay together)
      if (!wrapping) {
        breakable = PR_FALSE;
      }

      // This frame does start a word. However, there is no point
      // messing around with it if we are already out of room. We
      // always have room if we are not breakable.
      if (!breakable || (x <= maxWidth)) {
        // There is room for this word fragment. It's possible that
        // this word fragment is the end of the text-run. If it's not
        // then we continue with the look-ahead processing.
        nsIFrame* next = lineLayout.FindNextText(this);
        if (nsnull != next) {
#ifdef DEBUG_WORD_WRAPPING
          nsAutoString tmp(tx.GetTextAt(0), offset-prevOffset);
          ListTag(stdout);
          printf(": start='");
          fputs(tmp, stdout);
          printf("' baseWidth=%d [%d,%d]\n", lastWordWidth, prevOffset, offset);
#endif
          // Look ahead in the text-run and compute the final word
          // width, taking into account any style changes and stopping
          // at the first breakable point.
          nscoord wordWidth = ComputeTotalWordWidth(lineLayout, aReflowState,
                                                    next, lastWordWidth);
          if (!breakable || (x - lastWordWidth + wordWidth <= maxWidth)) {
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
            column = prevColumn;
#ifdef DEBUG_WORD_WRAPPING
            printf("  x=%d maxWordWidth=%d len=%d\n", x, maxWordWidth,
                   offset - startingOffset);
#endif
            lineLayout.ForgetWordFrames();
          }
        }
      }
    }
  }
  lineLayout.SetColumn(column);

  // Inform line layout of how this piece of text ends in whitespace
  // (only text objects do this). Note that if x is zero then this
  // text object collapsed into nothingness which means it shouldn't
  // effect the current setting of the ends-in-whitespace flag.
  lineLayout.SetUnderstandsWhiteSpace(PR_TRUE);
  if (0 != x) {
    lineLayout.SetEndsInWhiteSpace(endsInWhitespace);
  }
  if (justDidFirstLetter) {
    lineLayout.SetFirstLetterStyleOK(PR_FALSE);
  }

  // Setup metrics for caller; store final max-element-size information
  aMetrics.width = x;
  mComputedWidth = x;
  if (0 == x) {
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
  }
  else {
    ts.mNormalFont->GetHeight(aMetrics.height);
    ts.mNormalFont->GetMaxAscent(aMetrics.ascent);
    ts.mNormalFont->GetMaxDescent(aMetrics.descent);
  }
  if (!wrapping) {
    maxWordWidth = x;
  }
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = maxWordWidth;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  // Set content offset and length
  mContentOffset = startingOffset;
  mContentLength = offset - startingOffset;

  nsReflowStatus rs = (offset == contentLength)
    ? NS_FRAME_COMPLETE
    : NS_FRAME_NOT_COMPLETE;
  if (endsInNewline) {
    rs = NS_INLINE_LINE_BREAK_AFTER(rs);
  }
  else if (offset == startingOffset) {
    rs = NS_INLINE_LINE_BREAK_BEFORE();
  }
  aStatus = rs;

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit TextFrame::Reflow: status=%x width=%d",
                  aStatus, aMetrics.width));
  return NS_OK;
}

NS_IMETHODIMP
TextFrame::AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace)
{
  // Get the text fragments that make up our content
  const nsTextFragment* frag;
  PRInt32 numFrags;
  nsITextContent* tc;
  if (NS_OK == mContent->QueryInterface(kITextContentIID, (void**) &tc)) {
    tc->GetText(frag, numFrags);
    NS_RELEASE(tc);

    // Find fragment that contains the end of the mapped content
    PRInt32 endIndex = mContentOffset + mContentLength;
    PRInt32 offset = 0;
    const nsTextFragment* lastFrag = frag + numFrags;
    while (frag < lastFrag) {
      PRInt32 fragLen = frag->GetLength();
      if (endIndex <= offset + fragLen) {
        offset = mContentOffset - offset;
        if (frag->Is2b()) {
          const PRUnichar* cp = frag->Get2b() + offset;
          const PRUnichar* end = cp + mContentLength;
          while (cp < end) {
            PRUnichar ch = *cp++;
            if (XP_IS_SPACE(ch)) {
              aUsedSpace = aExtraSpace;
              mRect.width += aExtraSpace;
              return NS_OK;
            }
          }
        }
        else {
          const unsigned char* cp =
            ((const unsigned char*)frag->Get1b()) + offset;
          const unsigned char* end = cp + mContentLength;
          while (cp < end) {
            PRUnichar ch = PRUnichar(*cp++);
            if (XP_IS_SPACE(ch)) {
              aUsedSpace = aExtraSpace;
              mRect.width += aExtraSpace;
              return NS_OK;
            }
          }
        }
        break;
      }
      offset += fragLen;
      frag++;
    }
  }
  aUsedSpace = 0;
  return NS_OK;
}

NS_IMETHODIMP
TextFrame::TrimTrailingWhiteSpace(nsIPresContext& aPresContext,
                                  nsIRenderingContext& aRC,
                                  nscoord& aDeltaWidth)
{
  nscoord dw = 0;
  const nsStyleText* textStyle = (const nsStyleText*)
    mStyleContext->GetStyleData(eStyleStruct_Text);
  if (NS_STYLE_WHITESPACE_PRE != textStyle->mWhiteSpace) {
    // Get font metrics for a space so we can adjust the width by the
    // right amount.
    const nsStyleFont* fontStyle = (const nsStyleFont*)
      mStyleContext->GetStyleData(eStyleStruct_Font);
    nscoord spaceWidth;
    aRC.SetFont(fontStyle->mFont);
    aRC.GetWidth(' ', spaceWidth);

    // Get the text fragments that make up our content
    const nsTextFragment* frag;
    PRInt32 numFrags;
    nsITextContent* tc;
    if (NS_OK == mContent->QueryInterface(kITextContentIID, (void**) &tc)) {
      tc->GetText(frag, numFrags);
      NS_RELEASE(tc);

      // Find fragment that contains the end of the mapped content
      PRInt32 endIndex = mContentOffset + mContentLength;
      PRInt32 offset = 0;
      const nsTextFragment* lastFrag = frag + numFrags;
      while (frag < lastFrag) {
        PRInt32 fragLen = frag->GetLength();
        if (endIndex <= offset + fragLen) {
          // Look inside the fragments last few characters and see if they
          // are whitespace. If so, count how much width was supplied by
          // them.
          offset = mContentOffset - offset;
          if (frag->Is2b()) {
            // XXX If by chance the last content fragment is *all*
            // whitespace then this won't back up far enough.
            const PRUnichar* cp = frag->Get2b() + offset;
            const PRUnichar* end = cp + mContentLength;
            if (--end >= cp) {
              PRUnichar ch = *end;
              if (XP_IS_SPACE(ch)) {
                dw = spaceWidth;
              }
            }
          }
          else {
            const unsigned char* cp =
              ((const unsigned char*)frag->Get1b()) + offset;
            const unsigned char* end = cp + mContentLength;
            if (--end >= cp) {
              PRUnichar ch = PRUnichar(*end);
              if (XP_IS_SPACE(ch)) {
                dw = spaceWidth;
              }
            }
          }
          break;
        }
        offset += fragLen;
        frag++;
      }
    }
    if (mRect.width > dw) {
      mRect.width -= dw;
    }
    else {
      dw = mRect.width;
      mRect.width = 0;
    }
    mComputedWidth -= dw;
  }
  if (0 != dw) {
    mFlags |= TEXT_TRIMMED_WS;
  }
  else {
    mFlags &= ~TEXT_TRIMMED_WS;
  }
  aDeltaWidth = dw;
  return NS_OK;
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
        nsAutoString tmp;
        frame->GetFrameName(tmp);
        fputs(tmp, stdout);
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
  PRUnichar* bp = tx.GetNextWord(PR_TRUE, wordLen, contentLen, isWhitespace);
  if ((nsnull == bp) || isWhitespace) {
    // Don't bother measuring nothing
    aStop = PR_TRUE;
    return 0;
  }
  aStop = contentLen < tx.GetContentLength();

  nsIStyleContext* sc;
  if ((NS_OK == aTextFrame->GetStyleContext(sc)) &&
      (nsnull != sc)) {
    // Measure the piece of text. Note that we have to select the
    // appropriate font into the text first because the rendering
    // context has our font in it, not the font that aText is using.
    nscoord width;
    nsIRenderingContext& rc = *aReflowState.rendContext;
    nsIFontMetrics* oldfm;
    rc.GetFontMetrics(oldfm);

    TextStyle ts(aLineLayout.mPresContext, rc, sc);
    if (ts.mSmallCaps) {
      MeasureSmallCapsText(aReflowState, ts, buf, wordLen, width);
    }
    else {
      rc.GetWidth(buf, wordLen, width);
    }
    rc.SetFont(oldfm);
    NS_IF_RELEASE(oldfm);
    NS_RELEASE(sc);

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
TextFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Text", aResult);
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsBlockFrame.h"
#include "nsFrameReflowState.h"
#include "nsLineLayout.h"
#include "nsInlineReflow.h"
#include "nsAbsoluteFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIReflowCommand.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIFontMetrics.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLValue.h"
#include "nsDOMEvent.h"
#include "nsIHTMLContent.h"
#include "prprf.h"

// XXX temporary for :first-letter support
#include "nsITextContent.h"
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);/* XXX */

//XXX begin

// 11-03-98: low hanging memory fruit: get rid of text-runs that have
// only one piece of text in them!

// 09-17-98: I don't like keeping mInnerBottomMargin

// 09-18-98: floating block elements don't size quite right because we
// wrap them in a body frame and the body frame doesn't honor the css
// width/height properties (among others!). The body code needs
// updating.

//XXX end

#ifdef NS_DEBUG
#undef NOISY_FIRST_LINE
#undef REALLY_NOISY_FIRST_LINE
#undef NOISY_FIRST_LETTER
#undef NOISY_MAX_ELEMENT_SIZE
#undef NOISY_RUNIN
#undef NOISY_FLOATER_CLEARING
#else
#undef NOISY_FIRST_LINE
#undef REALLY_NOISY_FIRST_LINE
#undef NOISY_FIRST_LETTER
#undef NOISY_MAX_ELEMENT_SIZE
#undef NOISY_RUNIN
#undef NOISY_FLOATER_CLEARING
#endif

/* 52b33130-0b99-11d2-932e-00805f8add32 */
#define NS_BLOCK_FRAME_CID \
{ 0x52b33130, 0x0b99, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

static const nsIID kBlockFrameCID = NS_BLOCK_FRAME_CID;


class BulletFrame;
struct LineData;
class nsBlockFrame;

struct nsBlockReflowState : public nsFrameReflowState {
  nsBlockReflowState(nsIPresContext& aPresContext,
                     const nsHTMLReflowState& aReflowState,
                     const nsHTMLReflowMetrics& aMetrics);

  ~nsBlockReflowState();

  /**
   * Update the mCurrentBand data based on the current mY position.
   */
  void GetAvailableSpace();

  void InitFloater(nsPlaceholderFrame* aPlaceholderFrame);

  void AddFloater(nsPlaceholderFrame* aPlaceholderFrame);

  void PlaceFloater(nsPlaceholderFrame* aFloater, PRBool& aIsLeftFloater);

  void PlaceFloaters(nsVoidArray* aFloaters, PRBool aAllOfThem);

  void ClearFloaters(PRUint8 aBreakType);

  PRBool IsLeftMostChild(nsIFrame* aFrame);

  nsLineLayout mLineLayout;
  nsInlineReflow* mInlineReflow;

  nsISpaceManager* mSpaceManager;
  nscoord mSpaceManagerX, mSpaceManagerY;
  nsBlockFrame* mBlock;
  nsBlockFrame* mNextInFlow;

  PRBool mInlineReflowPrepared;

  nsBlockFrame* mRunInFrame;
  nsBlockFrame* mRunInToFrame;

  PRUint8 mTextAlign;

  PRUintn mPrevMarginFlags;
// previous margin flag bits (above and beyond the carried-out-margin flags)
#define HAVE_CARRIED_MARGIN 0x100
#define ALREADY_APPLIED_BOTTOM_MARGIN 0x200

  nscoord mBottomEdge;          // maximum Y

  PRBool mUnconstrainedWidth;
  PRBool mUnconstrainedHeight;
  nscoord mY;
  nscoord mKidXMost;

  nsIFrame* mPrevChild;

  LineData* mFreeList;

  nsVoidArray mPendingFloaters;

  LineData* mCurrentLine;
  LineData* mPrevLine;

  // The next list ordinal for counting list bullets
  PRInt32 mNextListOrdinal;

  // XXX what happens if we need more than 12 trapezoids?
  struct BlockBandData : public nsBandData {
    // Trapezoids used during band processing
    nsBandTrapezoid data[12];

    // Bounding rect of available space between any left and right floaters
    nsRect          availSpace;

    BlockBandData() {
      size = 12;
      trapezoids = data;
    }

    /**
     * Computes the bounding rect of the available space, i.e. space
     * between any left and right floaters Uses the current trapezoid
     * data, see nsISpaceManager::GetBandData(). Also updates member
     * data "availSpace".
     */
    void ComputeAvailSpaceRect();
  };

  BlockBandData mCurrentBand;
};

// XXX This is vile. Make it go away
void
nsLineLayout::InitFloater(nsPlaceholderFrame* aFrame)
{
  mBlockReflowState->InitFloater(aFrame);
}
void
nsLineLayout::AddFloater(nsPlaceholderFrame* aFrame)
{
  mBlockReflowState->AddFloater(aFrame);
}

//----------------------------------------------------------------------

#ifdef REALLY_NOISY_FIRST_LINE
static void
DumpStyleGeneaology(nsIFrame* aFrame, const char* gap)
{
  fputs(gap, stdout);
  aFrame->ListTag(stdout);
  fputs(name, out);
  printf(": ");
  nsIStyleContext* sc;
  aFrame->GetStyleContext(sc);
  while (nsnull != sc) {
    nsIStyleContext* psc;
    printf("%p ", sc);
    psc = sc->GetParent();
    NS_RELEASE(sc);
    sc = psc;
  }
  printf("\n");
}
#endif

//----------------------------------------------------------------------

#include "nsHTMLImage.h"
class BulletFrame : public nsFrame {
public:
  virtual ~BulletFrame();

  // nsIFrame
  NS_IMETHOD DeleteFrame(nsIPresContext& aPresContext);
  NS_IMETHOD Paint(nsIPresContext &aCX,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const;

  // nsIHTMLReflow
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  void SetListItemOrdinal(nsBlockReflowState& aBlockState);

  void GetDesiredSize(nsIPresContext* aPresContext,
                      const nsHTMLReflowState& aReflowState,
                      nsHTMLReflowMetrics& aMetrics);

  void GetListItemText(nsIPresContext& aCX,
                       const nsStyleList& aMol,
                       nsString& aResult);

  PRInt32 mOrdinal;
  nsMargin mPadding;
  nsHTMLImageLoader mImageLoader;
};

BulletFrame::~BulletFrame()
{
}

NS_IMETHODIMP
BulletFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.DestroyLoader();
  return nsFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
BulletFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Bullet", aResult);
}

NS_IMETHODIMP
BulletFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "Bullet(%d)@%p ", ContentIndexInContainer(this), this);
  nsIView* view;
  GetView(view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
  }

  out << mRect;
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fputs("<>\n", out);
  return NS_OK;
}

NS_METHOD
BulletFrame::Paint(nsIPresContext&      aCX,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect)
{
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  nscoord width;

  if (disp->mVisible) {
    const nsStyleList* myList =
      (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);

    if (myList->mListStyleImage.Length() > 0) {
      nsIImage* image = mImageLoader.GetImage();
      if (nsnull == image) {
        if (!mImageLoader.GetLoadImageFailed()) {
          // No image yet
          return NS_OK;
        }
      }
      else {
        nsRect innerArea(mPadding.left, mPadding.top,
                         mRect.width - (mPadding.left + mPadding.right),
                         mRect.height - (mPadding.top + mPadding.bottom));
        aRenderingContext.DrawImage(image, innerArea);
        return NS_OK;
      }
    }

    const nsStyleFont* myFont =
      (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
    const nsStyleColor* myColor =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    nsIFontMetrics* fm;
    aRenderingContext.SetColor(myColor->mColor);

    nsAutoString text;
    switch (myList->mListStyleType) {
    case NS_STYLE_LIST_STYLE_NONE:
      break;

    default:
    case NS_STYLE_LIST_STYLE_BASIC:
    case NS_STYLE_LIST_STYLE_DISC:
      aRenderingContext.FillEllipse(mPadding.left, mPadding.top,
                                    mRect.width - (mPadding.left + mPadding.right),
                                    mRect.height - (mPadding.top + mPadding.bottom));
      break;

    case NS_STYLE_LIST_STYLE_CIRCLE:
      aRenderingContext.DrawEllipse(mPadding.left, mPadding.top,
                                    mRect.width - (mPadding.left + mPadding.right),
                                    mRect.height - (mPadding.top + mPadding.bottom));
      break;

    case NS_STYLE_LIST_STYLE_SQUARE:
      aRenderingContext.FillRect(mPadding.left, mPadding.top,
                                 mRect.width - (mPadding.left + mPadding.right),
                                 mRect.height - (mPadding.top + mPadding.bottom));
      break;

    case NS_STYLE_LIST_STYLE_DECIMAL:
    case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
    case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
    case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
    case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
      fm = aCX.GetMetricsFor(myFont->mFont);
      GetListItemText(aCX, *myList, text);
      aRenderingContext.SetFont(fm);
      aRenderingContext.GetWidth(text, width);
      aRenderingContext.DrawString(text, mPadding.left, mPadding.top, width);
      NS_RELEASE(fm);
      break;
    }
  }
  return NS_OK;
}

void
BulletFrame::SetListItemOrdinal(nsBlockReflowState& aReflowState)
{
  // Assume that the ordinal comes from the block reflow state
  mOrdinal = aReflowState.mNextListOrdinal;

  // Try to get value directly from the list-item, if it specifies a
  // value attribute. Note: we do this with our parent's content
  // because our parent is the list-item.
  nsHTMLValue value;
  nsIContent* parentContent;
  mContentParent->GetContent(parentContent);
  nsIHTMLContent* hc;
  if (NS_OK == parentContent->QueryInterface(kIHTMLContentIID, (void**) &hc)) {
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetAttribute(nsHTMLAtoms::value, value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        // Use ordinal specified by the value attribute
        mOrdinal = value.GetIntValue();
        if (mOrdinal <= 0) {
          mOrdinal = 1;
        }
      }
    }
    NS_RELEASE(hc);
  }
  NS_RELEASE(parentContent);

  aReflowState.mNextListOrdinal = mOrdinal + 1;
}

static const char* gLowerRomanCharsA = "ixcm";
static const char* gUpperRomanCharsA = "IXCM";
static const char* gLowerRomanCharsB = "vld?";
static const char* gUpperRomanCharsB = "VLD?";
static const char* gLowerAlphaChars  = "abcdefghijklmnopqrstuvwxyz";
static const char* gUpperAlphaChars  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// XXX change roman/alpha to use unsigned math so that maxint and
// maxnegint will work
void
BulletFrame::GetListItemText(nsIPresContext& aCX,
                             const nsStyleList& aListStyle,
                             nsString& result)
{
  PRInt32 ordinal = mOrdinal;
  char cbuf[40];
  switch (aListStyle.mListStyleType) {
  case NS_STYLE_LIST_STYLE_DECIMAL:
    PR_snprintf(cbuf, sizeof(cbuf), "%ld", ordinal);
    result.Append(cbuf);
    break;

  case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
  case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
  {
    if (ordinal <= 0) {
      ordinal = 1;
    }
    nsAutoString addOn, decStr;
    decStr.Append(ordinal, 10);
    PRIntn len = decStr.Length();
    const PRUnichar* dp = decStr.GetUnicode();
    const PRUnichar* end = dp + len;
    PRIntn romanPos = len;
    PRIntn n;

    const char* achars;
    const char* bchars;
    if (aListStyle.mListStyleType == NS_STYLE_LIST_STYLE_LOWER_ROMAN) {
      achars = gLowerRomanCharsA;
      bchars = gLowerRomanCharsB;
    }
    else {
      achars = gUpperRomanCharsA;
      bchars = gUpperRomanCharsB;
    }
    for (; dp < end; dp++) {
      romanPos--;
      addOn.SetLength(0);
      switch(*dp) {
      case '3':  addOn.Append(achars[romanPos]);
      case '2':  addOn.Append(achars[romanPos]);
      case '1':  addOn.Append(achars[romanPos]);
        break;
      case '4':
        addOn.Append(achars[romanPos]);
        // FALLTHROUGH
      case '5': case '6':
      case '7': case  '8':
        addOn.Append(bchars[romanPos]);
        for(n=0;n<(*dp-'5');n++) {
          addOn.Append(achars[romanPos]);
        }
        break;
      case '9':
        addOn.Append(achars[romanPos]);
        addOn.Append(achars[romanPos+1]);
        break;
      default:
        break;
      }
      result.Append(addOn);
    }
  }
  break;

  case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
  case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
  {
    PRInt32 anOffset = -1;
    PRInt32 aBase = 26;
    PRInt32 ndex=0;
    PRInt32 root=1;
    PRInt32 next=aBase;
    PRInt32 expn=1;
    const char* chars =
      (aListStyle.mListStyleType == NS_STYLE_LIST_STYLE_LOWER_ALPHA)
      ? gLowerAlphaChars : gUpperAlphaChars;

    // must be positive here...
    if (ordinal <= 0) {
      ordinal = 1;
    }
    ordinal--;          // a == 0

    // scale up in baseN; exceed current value.
    while (next<=ordinal) {
      root=next;
      next*=aBase;
      expn++;
    }
    while (0!=(expn--)) {
      ndex = ((root<=ordinal) && (0!=root)) ? (ordinal/root): 0;
      ordinal %= root;
      if (root>1)
        result.Append(chars[ndex+anOffset]);
      else
        result.Append(chars[ndex]);
      root /= aBase;
    }
  }
  break;
  }
  result.Append(".");
}

#define MIN_BULLET_SIZE 5               // from laytext.c

static nsresult
UpdateBulletCB(nsIPresContext& aPresContext, nsIFrame* aFrame, PRIntn aStatus)
{
  nsresult rv = NS_OK;
  if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // Now that the size is available, trigger a reflow of the bullet
    // frame.
    nsIPresShell* shell;
    shell = aPresContext.GetShell();
    if (nsnull != shell) {
      nsIReflowCommand* cmd;
      rv = NS_NewHTMLReflowCommand(&cmd, aFrame,
                                   nsIReflowCommand::ContentChanged);
      if (NS_OK == rv) {
        shell->EnterReflowLock();
        shell->AppendReflowCommand(cmd);
        NS_RELEASE(cmd);
        shell->ExitReflowLock();
      }
      NS_RELEASE(shell);
    }
  }
  return rv;
}

void
BulletFrame::GetDesiredSize(nsIPresContext*  aCX,
                            const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics)
{
  const nsStyleList* myList =
    (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
  nscoord ascent;

  if (myList->mListStyleImage.Length() > 0) {
    mImageLoader.SetURL(myList->mListStyleImage);
    mImageLoader.GetDesiredSize(aCX, aReflowState, this, UpdateBulletCB,
                                aMetrics);
    if (!mImageLoader.GetLoadImageFailed()) {
      nsHTMLContainerFrame::CreateViewForFrame(*aCX, this, mStyleContext,
                                               PR_FALSE);
      aMetrics.ascent = aMetrics.height;
      aMetrics.descent = 0;
      return;
    }
  }

  const nsStyleFont* myFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  nsIFontMetrics* fm = aCX->GetMetricsFor(myFont->mFont);
  nscoord bulletSize;
  float p2t;
  float t2p;

  nsAutoString text;
  switch (myList->mListStyleType) {
  case NS_STYLE_LIST_STYLE_NONE:
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    break;

  default:
  case NS_STYLE_LIST_STYLE_DISC:
  case NS_STYLE_LIST_STYLE_CIRCLE:
  case NS_STYLE_LIST_STYLE_BASIC:
  case NS_STYLE_LIST_STYLE_SQUARE:
    t2p = aCX->GetTwipsToPixels();
    fm->GetMaxAscent(ascent);
    bulletSize = NSTwipsToIntPixels(
      (nscoord)NSToIntRound(0.8f * (float(ascent) / 2.0f)), t2p);
    if (bulletSize < 1) {
      bulletSize = MIN_BULLET_SIZE;
    }
    p2t = aCX->GetPixelsToTwips();
    bulletSize = NSIntPixelsToTwips(bulletSize, p2t);
    mPadding.bottom = ascent / 8;
    aMetrics.width = mPadding.right + bulletSize;
    aMetrics.height = mPadding.bottom + bulletSize;
    aMetrics.ascent = mPadding.bottom + bulletSize;
    aMetrics.descent = 0;
    break;

  case NS_STYLE_LIST_STYLE_DECIMAL:
  case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
  case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
  case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
  case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
    GetListItemText(*aCX, *myList, text);
    fm->GetHeight(aMetrics.height);
    aReflowState.rendContext->SetFont(fm);
    aReflowState.rendContext->GetWidth(text, aMetrics.width);
    aMetrics.width += mPadding.right;
    fm->GetMaxAscent(aMetrics.ascent);
    fm->GetMaxDescent(aMetrics.descent);
    break;
  }
  NS_RELEASE(fm);
}

NS_IMETHODIMP
BulletFrame::Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus)
{
  // Get the base size
  GetDesiredSize(&aPresContext, aReflowState, aMetrics);

  // Add in the border and padding; split the top/bottom between the
  // ascent and descent to make things look nice
  const nsStyleSpacing* space =(const nsStyleSpacing*)
    mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  nsHTMLReflowState::ComputeBorderPaddingFor(this,
                                             aReflowState.parentReflowState,
                                             borderPadding);
  aMetrics.width += borderPadding.left + borderPadding.right;
  aMetrics.height += borderPadding.top + borderPadding.bottom;
  aMetrics.ascent += borderPadding.top;
  aMetrics.descent += borderPadding.bottom;

  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

//----------------------------------------------------------------------

#define LINE_IS_DIRTY                 0x1
#define LINE_IS_BLOCK                 0x2
#define LINE_NEED_DID_REFLOW          0x8
#define LINE_TOP_MARGIN_IS_AUTO       0x10
#define LINE_BOTTOM_MARGIN_IS_AUTO    0x20
#define LINE_OUTSIDE_CHILDREN         0x40

struct LineData {
  LineData(nsIFrame* aFrame, PRInt32 aCount, PRUint16 flags) {
    mFirstChild = aFrame;
    mChildCount = aCount;
    mState = LINE_IS_DIRTY | LINE_NEED_DID_REFLOW | flags;
    mFloaters = nsnull;
    mNext = nsnull;
    mBounds.SetRect(0,0,0,0);
    mCombinedArea.SetRect(0,0,0,0);
    mCarriedOutTopMargin = 0;
    mCarriedOutBottomMargin = 0;
    mBreakType = NS_STYLE_CLEAR_NONE;
  }

  ~LineData();

  void List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter = nsnull,
            PRBool aOutputMe=PR_TRUE) const;

  PRInt32 ChildCount() const {
    return PRInt32(mChildCount);
  }

  nsIFrame* LastChild() const;

  PRBool IsLastChild(nsIFrame* aFrame) const;

  PRBool IsBlock() const {
    return 0 != (LINE_IS_BLOCK & mState);
  }

  void SetIsBlock() {
    mState |= LINE_IS_BLOCK;
  }

  void ClearIsBlock() {
    mState &= ~LINE_IS_BLOCK;
  }

  void SetIsBlock(PRBool aValue) {
    if (aValue) {
      SetIsBlock();
    }
    else {
      ClearIsBlock();
    }
  }

  void MarkDirty() {
    mState |= LINE_IS_DIRTY;
  }

  void ClearDirty() {
    mState &= ~LINE_IS_DIRTY;
  }

  void SetNeedDidReflow() {
    mState |= LINE_NEED_DID_REFLOW;
  }

  void ClearNeedDidReflow() {
    mState &= ~LINE_NEED_DID_REFLOW;
  }

  PRBool NeedsDidReflow() {
    return 0 != (LINE_NEED_DID_REFLOW & mState);
  }

  PRBool IsDirty() const {
    return 0 != (LINE_IS_DIRTY & mState);
  }

  void SetOutsideChildren() {
    mState |= LINE_OUTSIDE_CHILDREN;
  }

  void ClearOutsideChildren() {
    mState &= ~LINE_OUTSIDE_CHILDREN;
  }

  PRBool OutsideChildren() const {
    return 0 != (LINE_OUTSIDE_CHILDREN & mState);
  }

  PRUint16 GetState() const { return mState; }

  char* StateToString(char* aBuf, PRInt32 aBufSize) const;

  PRBool Contains(nsIFrame* aFrame) const;

  void SetMarginFlags(PRUintn aFlags) {
    mState &= ~(LINE_TOP_MARGIN_IS_AUTO|LINE_BOTTOM_MARGIN_IS_AUTO);
    if (NS_CARRIED_TOP_MARGIN_IS_AUTO & aFlags) {
      mState |= LINE_TOP_MARGIN_IS_AUTO;
    }
    if (NS_CARRIED_BOTTOM_MARGIN_IS_AUTO & aFlags) {
      mState |= LINE_BOTTOM_MARGIN_IS_AUTO;
    }
  }

  PRUintn GetMarginFlags() {
    return ((LINE_TOP_MARGIN_IS_AUTO & mState)
            ? NS_CARRIED_TOP_MARGIN_IS_AUTO : 0) |
      ((LINE_BOTTOM_MARGIN_IS_AUTO & mState) ?
       NS_CARRIED_BOTTOM_MARGIN_IS_AUTO : 0);
  }

  void UnplaceFloaters(nsISpaceManager* aSpaceManager) {
    if (nsnull != mFloaters) {
      PRInt32 i, n = mFloaters->Count();
      for (i = 0; i < n; i++) {
        nsPlaceholderFrame* pf = (nsPlaceholderFrame*) mFloaters->ElementAt(i);
        nsIFrame* floater = pf->GetAnchoredItem();
        aSpaceManager->RemoveRegion(floater);
      }
    }
  }

#ifdef NS_DEBUG
  void Verify();
#endif

  nsIFrame* mFirstChild;
  PRUint16 mChildCount;
  PRUint16 mState;
  PRUint8 mBreakType;
  nsRect mBounds;
  nsRect mCombinedArea;
  nscoord mCarriedOutTopMargin;
  nscoord mCarriedOutBottomMargin;
  nsVoidArray* mFloaters;
  LineData* mNext;
};

LineData::~LineData()
{
  if (nsnull != mFloaters) {
    delete mFloaters;
  }
}

static void
ListFloaters(FILE* out, PRInt32 aIndent, nsVoidArray* aFloaters)
{
  PRInt32 j, i, n = aFloaters->Count();
  for (i = 0; i < n; i++) {
    for (j = aIndent; --j >= 0; ) fputs("  ", out);
    nsPlaceholderFrame* ph = (nsPlaceholderFrame*) aFloaters->ElementAt(i);
    if (nsnull != ph) {
      fprintf(out, "placeholder@%p\n", ph);
      nsIFrame* frame = ph->GetAnchoredItem();
      if (nsnull != frame) {
        frame->List(out, aIndent + 1, nsnull);
      }
    }
  }
}

static void
ListTextRuns(FILE* out, PRInt32 aIndent, nsTextRun* aRuns)
{
  while (nsnull != aRuns) {
    aRuns->List(out, aIndent);
    aRuns = aRuns->GetNext();
  }
}

char*
LineData::StateToString(char* aBuf, PRInt32 aBufSize) const
{
  PR_snprintf(aBuf, aBufSize, "%s,%s",
              (mState & LINE_IS_DIRTY) ? "dirty" : "clean",
              (mState & LINE_IS_BLOCK) ? "block" : "inline");
  return aBuf;
}

void
LineData::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter,
               PRBool aOutputMe) const
{
  PRInt32 i;

  if (aOutputMe) {
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    char cbuf[100];
    fprintf(out, "line %p: count=%d state=%s ",
            this, ChildCount(), StateToString(cbuf, sizeof(cbuf)));
    if (0 != mCarriedOutTopMargin) {
      fprintf(out, "tm=%d ", mCarriedOutTopMargin);
    }
    if (0 != mCarriedOutBottomMargin) {
      fprintf(out, "bm=%d ", mCarriedOutBottomMargin);
    }
    out << mBounds;
    fprintf(out, " ca=");
    out << mCombinedArea;
    fprintf(out, " <\n");
  }

  nsIFrame* frame = mFirstChild;
  PRInt32 n = ChildCount();
  while (--n >= 0) {
    frame->List(out, aIndent + 1, aFilter);
    frame->GetNextSibling(frame);
  }

  if (aOutputMe) {
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    if (nsnull != mFloaters) {
      fputs("> floaters <\n", out);
      ListFloaters(out, aIndent + 1, mFloaters);
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
    }
    fputs(">\n", out);
  }
}

nsIFrame*
LineData::LastChild() const
{
  nsIFrame* frame = mFirstChild;
  PRInt32 n = ChildCount() - 1;
  while (--n >= 0) {
    frame->GetNextSibling(frame);
  }
  return frame;
}

PRBool
LineData::IsLastChild(nsIFrame* aFrame) const
{
  nsIFrame* lastFrame = LastChild();
  return aFrame == lastFrame;
}

PRBool
LineData::Contains(nsIFrame* aFrame) const
{
  PRInt32 n = ChildCount();
  nsIFrame* frame = mFirstChild;
  while (--n >= 0) {
    if (frame == aFrame) {
      return PR_TRUE;
    }
    frame->GetNextSibling(frame);
  }
  return PR_FALSE;
}

static PRInt32
LengthOf(nsIFrame* aFrame)
{
  PRInt32 result = 0;
  while (nsnull != aFrame) {
    result++;
    aFrame->GetNextSibling(aFrame);
  }
  return result;
}

#ifdef NS_DEBUG
void
LineData::Verify()
{
  nsIFrame* lastFrame = LastChild();
  if (nsnull != lastFrame) {
    nsIFrame* nextInFlow;
    lastFrame->GetNextInFlow(nextInFlow);
    if (nsnull != mNext) {
      nsIFrame* nextSibling;
      lastFrame->GetNextSibling(nextSibling);
      NS_ASSERTION(mNext->mFirstChild == nextSibling, "bad line list");
    }
  }
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len >= ChildCount(), "bad mChildCount");
}

static void
VerifyLines(LineData* aLine)
{
  while (nsnull != aLine) {
    aLine->Verify();
    aLine = aLine->mNext;
  }
}

static void
VerifyChildCount(LineData* aLines, PRBool aEmptyOK = PR_FALSE)
{
  if (nsnull != aLines) {
    PRInt32 childCount = LengthOf(aLines->mFirstChild);
    PRInt32 sum = 0;
    LineData* line = aLines;
    while (nsnull != line) {
      if (!aEmptyOK) {
        NS_ASSERTION(0 != line->ChildCount(), "empty line left in line list");
      }
      sum += line->ChildCount();
      line = line->mNext;
    }
    if (sum != childCount) {
      printf("Bad sibling list/line mChildCount's\n");
      LineData* line = aLines;
      while (nsnull != line) {
        line->List(stdout, 1);
        if (nsnull != line->mNext) {
          nsIFrame* lastFrame = line->LastChild();
          if (nsnull != lastFrame) {
            nsIFrame* nextSibling;
            lastFrame->GetNextSibling(nextSibling);
            if (line->mNext->mFirstChild != nextSibling) {
              printf("  [list broken: nextSibling=%p mNext->mFirstChild=%p]\n",
                     nextSibling, line->mNext->mFirstChild);
            }
          }
        }
        line = line->mNext;
      }
      NS_ASSERTION(sum == childCount, "bad sibling list/line mChildCount's");
    }
  }
}
#endif

static void
DeleteLineList(nsIPresContext& aPresContext, LineData* aLine)
{
  if (nsnull != aLine) {
    // Delete our child frames before doing anything else. In particular
    // we do all of this before our base class releases it's hold on the
    // view.
    for (nsIFrame* child = aLine->mFirstChild; child; ) {
      nsIFrame* nextChild;
      child->GetNextSibling(nextChild);
      child->DeleteFrame(aPresContext);
      child = nextChild;
    }

    while (nsnull != aLine) {
      LineData* next = aLine->mNext;
      delete aLine;
      aLine = next;
    }
  }
}

static LineData*
LastLine(LineData* aLine)
{
  if (nsnull != aLine) {
    while (nsnull != aLine->mNext) {
      aLine = aLine->mNext;
    }
  }
  return aLine;
}

static LineData*
FindLineContaining(LineData* aLine, nsIFrame* aFrame)
{
  while (nsnull != aLine) {
    if (aLine->Contains(aFrame)) {
      return aLine;
    }
    aLine = aLine->mNext;
  }
  return nsnull;
}

//----------------------------------------------------------------------

void
nsBlockReflowState::BlockBandData::ComputeAvailSpaceRect()
{
  nsBandTrapezoid*  trapezoid = data;

  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater, or when we've
    // looked at every trapezoid and none are right floaters
    for (i = 0; i < count; i++) {
      nsBandTrapezoid*  trapezoid = &data[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 j, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }
        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            break;
          }
        }
      }
    }
  foundRightFloater:

    if (i > 0) {
      trapezoid = &data[i - 1];
    }
  }

  if (nsBandTrapezoid::Available == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availSpace);
  } else {
    const nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availSpace);

    // XXX Better handle the case of multiple frames
    if (nsBandTrapezoid::Occupied == trapezoid->state) {
      trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&)display);
      if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
        availSpace.x = availSpace.XMost();
      }
    }
    availSpace.width = 0;
  }
}

//----------------------------------------------------------------------

nsBlockReflowState::nsBlockReflowState(nsIPresContext& aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       const nsHTMLReflowMetrics& aMetrics)
  : nsFrameReflowState(aPresContext, aReflowState, aMetrics),
    mLineLayout(aPresContext, aReflowState.spaceManager)
{
  mInlineReflow = nsnull;
  mLineLayout.Init(this);

  mSpaceManager = aReflowState.spaceManager;

  // Translate into our content area and then save the 
  // coordinate system origin for later.
  mSpaceManager->Translate(mBorderPadding.left, mBorderPadding.top);
  mSpaceManager->GetTranslation(mSpaceManagerX, mSpaceManagerY);

  mPresContext = aPresContext;
  mBlock = (nsBlockFrame*) frame;
  mBlock->GetNextInFlow((nsIFrame*&)mNextInFlow);
  mKidXMost = 0;

  mRunInFrame = nsnull;
  mRunInToFrame = nsnull;

  mY = 0;
  mUnconstrainedWidth = maxSize.width == NS_UNCONSTRAINEDSIZE;
  mUnconstrainedHeight = maxSize.height == NS_UNCONSTRAINEDSIZE;
#ifdef NS_DEBUG
  if (!mUnconstrainedWidth && (maxSize.width > 100000)) {
    mBlock->ListTag(stdout);
    printf(": bad parent: maxSize WAS %d,%d\n",
           maxSize.width, maxSize.height);
    maxSize.width = NS_UNCONSTRAINEDSIZE;
    mUnconstrainedWidth = PR_TRUE;
  }
  if (!mUnconstrainedHeight && (maxSize.height > 100000)) {
    mBlock->ListTag(stdout);
    printf(": bad parent: maxSize WAS %d,%d\n",
           maxSize.width, maxSize.height);
    maxSize.height = NS_UNCONSTRAINEDSIZE;
    mUnconstrainedHeight = PR_TRUE;
  }
#endif

  mTextAlign = mStyleText->mTextAlign;
  mPrevMarginFlags = 0;

  nscoord lr = mBorderPadding.left + mBorderPadding.right;
  mY = mBorderPadding.top;
  if (eHTMLFrameConstraint_FixedContent == widthConstraint) {
    // The CSS2 spec says that the width attribute defines the width
    // of the "content area" which does not include the border
    // padding. So we add those back in.
    mBorderArea.width = minWidth + lr;
    mContentArea.width = minWidth;
  }
  else {
    if (mUnconstrainedWidth) {
      mBorderArea.width = NS_UNCONSTRAINEDSIZE;
      mContentArea.width = NS_UNCONSTRAINEDSIZE;
    }
    else {
      mBorderArea.width = maxSize.width;
      mContentArea.width = maxSize.width - lr;
    }
  }
  mBorderArea.height = maxSize.height;
  mContentArea.height = maxSize.height;
  mBottomEdge = maxSize.height;
  if (!mUnconstrainedHeight) {
    mBottomEdge -= mBorderPadding.bottom;
  }

  mPrevChild = nsnull;
  mFreeList = nsnull;
  mPrevLine = nsnull;
}

nsBlockReflowState::~nsBlockReflowState()
{
  // Restore the coordinate system
  mSpaceManager->Translate(-mBorderPadding.left, -mBorderPadding.top);

  LineData* line = mFreeList;
  while (nsnull != line) {
    NS_ASSERTION((0 == line->ChildCount()) && (nsnull == line->mFirstChild),
                 "bad free line");
    LineData* next = line->mNext;
    delete line;
    line = next;
  }
}

// Get the available reflow space for the current y coordinate. The
// available space is relative to our coordinate system (0,0) is our
// upper left corner.
void
nsBlockReflowState::GetAvailableSpace()
{
  nsISpaceManager* sm = mSpaceManager;

#ifdef NS_DEBUG
  // Verify that the caller setup the coordinate system properly
  nscoord wx, wy;
  sm->GetTranslation(wx, wy);
  NS_ASSERTION((wx == mSpaceManagerX) && (wy == mSpaceManagerY),
               "bad coord system");
#endif

  // Fill in band data for the specific Y coordinate. Because the
  // space manager is pre-translated into our content-area (so that
  // nested blocks/inlines will line up properly), we have to remove
  // the Y translation to find the band coordinates relative to our
  // inner (content area) upper left corner (0,0).
  sm->GetBandData(mY - mBorderPadding.top, mContentArea, mCurrentBand);

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters.
  mCurrentBand.ComputeAvailSpaceRect();

  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("nsBlockReflowState::GetAvailableSpace: band={%d,%d,%d,%d} count=%d",
      mCurrentBand.availSpace.x, mCurrentBand.availSpace.y,
      mCurrentBand.availSpace.width, mCurrentBand.availSpace.height,
      mCurrentBand.count));
}

//----------------------------------------------------------------------

nsIAtom* nsBlockFrame::gFloaterAtom;
nsIAtom* nsBlockFrame::gBulletAtom;

nsresult
NS_NewBlockFrame(nsIFrame*& aNewFrame, PRUint32 aFlags)
{
  nsBlockFrame* it = new nsBlockFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  aNewFrame = it;
  return NS_OK;
}

nsBlockFrame::nsBlockFrame()
  : nsBlockFrameSuper()
{
  // XXX for now these are a memory leak
  if (nsnull == gFloaterAtom) {
    gFloaterAtom = NS_NewAtom("Floater-list");
  }
  if (nsnull == gBulletAtom) {
    gBulletAtom = NS_NewAtom("Bullet-list");
  }
}

nsBlockFrame::~nsBlockFrame()
{
  NS_IF_RELEASE(mFirstLineStyle);
  NS_IF_RELEASE(mFirstLetterStyle);
  nsTextRun::DeleteTextRuns(mTextRuns);
}

NS_IMETHODIMP
nsBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kBlockFrameCID)) {
    *aInstancePtr = (void*) this;
    return NS_OK;
  }
  return nsBlockFrameSuper::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsBlockFrame::ReResolveStyleContext(nsIPresContext* aPresContext,
                                    nsIStyleContext* aParentContext)
{
  nsIStyleContext* oldContext = mStyleContext;
  nsresult rv = nsFrame::ReResolveStyleContext(aPresContext, aParentContext);
  if (NS_OK != rv) {
    return rv;
  }
  if (oldContext != mStyleContext) {
    // Re-resolve the :first-line pseudo style context
    if (nsnull == mPrevInFlow) {
      nsIStyleContext* newFirstLineStyle =
        aPresContext->ProbePseudoStyleContextFor(mContent,
                                                 nsHTMLAtoms::firstLinePseudo,
                                                 mStyleContext);
      if (newFirstLineStyle != mFirstLineStyle) {
        NS_IF_RELEASE(mFirstLineStyle);
        mFirstLineStyle = newFirstLineStyle;
      }
      else {
        NS_IF_RELEASE(newFirstLineStyle);
      }

      // Re-resolve the :first-letter pseudo style context
      nsIStyleContext* newFirstLetterStyle =
        aPresContext->ProbePseudoStyleContextFor(mContent,
                                                 nsHTMLAtoms::firstLetterPseudo,
                                                 (nsnull != mFirstLineStyle
                                                  ? mFirstLineStyle
                                                  : mStyleContext));
      if (newFirstLetterStyle != mFirstLetterStyle) {
        NS_IF_RELEASE(mFirstLetterStyle);
        mFirstLetterStyle = newFirstLetterStyle;
      }
      else {
        NS_IF_RELEASE(newFirstLetterStyle);
      }
    }

    // Update the child frames on each line
    LineData* line = mLines;
    while (nsnull != line) {
      nsIFrame* child = line->mFirstChild;
      PRInt32 n = line->mChildCount;
      while ((--n >= 0) && NS_SUCCEEDED(rv)) {
        if (line == mLines) {
          rv = child->ReResolveStyleContext(aPresContext,
                                            (nsnull != mFirstLineStyle
                                             ? mFirstLineStyle
                                             : mStyleContext));
        }
        else {
          rv = child->ReResolveStyleContext(aPresContext, mStyleContext);
        }
        child->GetNextSibling(child);
      }
      line = line->mNext;
    }

    // Update the child frames on each overflow line too
    line = mOverflowLines;
    while (nsnull != line) {
      nsIFrame* child = line->mFirstChild;
      PRInt32 n = line->mChildCount;
      while ((--n >= 0) && NS_SUCCEEDED(rv)) {
        rv = child->ReResolveStyleContext(aPresContext, mStyleContext);
        child->GetNextSibling(child);
      }
      line = line->mNext;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsBlockFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  nsresult rv = AppendNewFrames(aPresContext, aChildList);
  if (NS_OK != rv) {
    return rv;
  }

  // Create list bullet if this is a list-item. Note that this is done
  // here so that RenumberLists will work (it needs the bullets to
  // store the bullet numbers).
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  if ((nsnull == mPrevInFlow) &&
      (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) &&
      (nsnull == mBullet)) {
    // Resolve style for the bullet frame
    nsIStyleContext* kidSC;
    kidSC = aPresContext.ResolvePseudoStyleContextFor(mContent, 
                                                      nsHTMLAtoms::bulletPseudo, 
                                                      mStyleContext);
    // Create bullet frame
    mBullet = new BulletFrame;
    if (nsnull == mBullet) {
      NS_RELEASE(kidSC);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mBullet->Init(aPresContext, mContent, this, kidSC);
    NS_RELEASE(kidSC);

    // If the list bullet frame should be positioned inside then add
    // it to the flow now.
    const nsStyleList* styleList;
    GetStyleData(eStyleStruct_List, (const nsStyleStruct*&) styleList);
    if (NS_STYLE_LIST_STYLE_POSITION_INSIDE == styleList->mListStylePosition) {
      InsertNewFrame(aPresContext, this, mBullet, nsnull);
    }
  }

  // Lookup up the two pseudo style contexts
  if (nsnull == mPrevInFlow) {
    mFirstLineStyle = aPresContext.
      ProbePseudoStyleContextFor(mContent, nsHTMLAtoms::firstLinePseudo,
                                 mStyleContext);
    mFirstLetterStyle = aPresContext.
      ProbePseudoStyleContextFor(mContent, nsHTMLAtoms::firstLetterPseudo,
                                 (nsnull != mFirstLineStyle
                                  ? mFirstLineStyle
                                  : mStyleContext));
#ifdef NOISY_FIRST_LETTER
    if (nsnull != mFirstLetterStyle) {
      printf("block(%d)@%p: first-letter style found\n",
             ContentIndexInContainer(this), this);
    }
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  // When we have a bullet frame and it's not in our child list then
  // we need to delete it ourselves (this is the common case for
  // list-item's that have outside bullets).
  if ((nsnull != mBullet) &&
      ((nsnull == mLines) || (mBullet != mLines->mFirstChild))) {
    mBullet->DeleteFrame(aPresContext);
    mBullet = nsnull;
  }

  DeleteLineList(aPresContext, mLines);
  DeleteLineList(aPresContext, mOverflowLines);
  DeleteFrameList(aPresContext, &mFloaters);

  return nsBlockFrameSuper::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsBlockFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::CreateContinuingFrame(nsIPresContext&  aCX,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aStyleContext,
                                    nsIFrame*&       aContinuingFrame)
{
  nsBlockFrame* cf = new nsBlockFrame;
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  cf->Init(aCX, mContent, aParent, aStyleContext);
  cf->SetFlags(mFlags);
  cf->AppendToFlow(this);
  aContinuingFrame = cf;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockFrame::CreateContinuingFrame: newFrame=%p", cf));
  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Block", aResult);
}

NS_METHOD
nsBlockFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  PRInt32 i;

  // if a filter is present, only output this frame if the filter says
  // we should
  nsAutoString tagString;
  if (nsnull != mContent) {
    nsIAtom* tag;
    mContent->GetTag(tag);
    if (tag != nsnull)  {
      tag->ToString(tagString);
      NS_RELEASE(tag);
    }
  }
  PRBool outputMe = (nsnull == aFilter) || aFilter->OutputTag(&tagString);
  if (outputMe) {
    // Indent
    for (i = aIndent; --i >= 0; ) fputs("  ", out);

    // Output the tag
    ListTag(out);
    nsIView* view;
    GetView(view);
    if (nsnull != view) {
      fprintf(out, " [view=%p]", view);
    }

    // Output the flow linkage
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
    fputs("<\n", out);
    aIndent++;
  }

  // Output bullet first
  if (nsnull != mBullet) {
    if (outputMe) {
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fprintf(out, "bullet <\n");
    }
    mBullet->List(out, aIndent+1, aFilter);
    if (outputMe) {
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }
  }

  // Output the lines
  if (nsnull != mLines) {
    LineData* line = mLines;
    while (nsnull != line) {
      line->List(out, aIndent, aFilter, outputMe);
      line = line->mNext;
    }
  }

  // Output floaters next
  if (nsnull != mFloaters) {
    if (outputMe) {
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fprintf(out, "all-floaters <\n");
    }
    nsIFrame* floater = mFloaters;
    while (nsnull != floater) {
      floater->List(out, aIndent+1, aFilter);
      floater->GetNextSibling(floater);
    }
    if (outputMe) {
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }
  }

  // Output the text-runs
  if (outputMe) {
    if (nsnull != mTextRuns) {
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs("text-runs <\n", out);

      ListTextRuns(out, aIndent + 1, mTextRuns);

      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }

    aIndent--;
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
  }

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsBlockFrame::FirstChild(nsIAtom* aListName, nsIFrame*& aFirstChild) const
{
  if (nsnull == aListName) {
    aFirstChild = (nsnull != mLines) ? mLines->mFirstChild : nsnull;
    return NS_OK;
  }
  else if (aListName == gFloaterAtom) {
    aFirstChild = mFloaters;
    return NS_OK;
  }
  else if (aListName == gBulletAtom) {
    aFirstChild = mBullet;
    return NS_OK;
  }
  aFirstChild = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsBlockFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom*& aListName) const
{
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  nsIAtom* atom = nsnull;
  switch (aIndex) {
  case NS_BLOCK_FRAME_FLOATER_LIST_INDEX:
    atom = gFloaterAtom;
    NS_ADDREF(atom);
    break;
  case NS_BLOCK_FRAME_BULLET_LIST_INDEX:
    atom = gBulletAtom;
    NS_ADDREF(atom);
    break;
  }
  aListName = atom;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

NS_IMETHODIMP
nsBlockFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_TRUE;
  return NS_OK;
}

void
nsBlockFrame::TakeRunInFrames(nsBlockFrame* aRunInFrame)
{
  // Simply steal the run-in-frame's line list and make it our
  // own. XXX Very similar to the logic in DrainOverflowLines...
  LineData* line = aRunInFrame->mLines;

  // Make all the frames on the mOverflowLines list mine
  nsIFrame* lastFrame = nsnull;
  nsIFrame* frame = line->mFirstChild;
  while (nsnull != frame) {
    nsIFrame* geometricParent;
    nsIFrame* contentParent;
    frame->GetGeometricParent(geometricParent);
    frame->GetContentParent(contentParent);
    if (contentParent == geometricParent) {
      frame->SetContentParent(this);
    }
    frame->SetGeometricParent(this);
    lastFrame = frame;
    frame->GetNextSibling(frame);
  }

  // Join the line lists
  if (nsnull == mLines) {
    mLines = line;
  }
  else {
    // Join the sibling lists together
    lastFrame->SetNextSibling(mLines->mFirstChild);

    // Place overflow lines at the front of our line list
    LineData* lastLine = LastLine(line);
    lastLine->mNext = mLines;
    mLines = line;
  }

  aRunInFrame->mLines = nsnull;
}

NS_IMETHODIMP
nsBlockFrame::Reflow(nsIPresContext&          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("enter nsBlockFrame::Reflow: maxSize=%d,%d reason=%d",
                  aReflowState.maxSize.width,
                  aReflowState.maxSize.height,
                  aReflowState.reason));

  // If this is the initial reflow, generate any synthetic content
  // that needs generating.
  if (eReflowReason_Initial == aReflowState.reason) {
    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }
  else {
    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  // Replace parent provided reflow state with our own significantly
  // more extensive version.
  nsBlockReflowState state(aPresContext, aReflowState, aMetrics);
  if (NS_BODY_NO_AUTO_MARGINS & mFlags) {
    // Pretend that there is a really big bottom margin preceeding the
    // first line so that it's margin is never applied.
  }
  if (NS_BODY_THE_BODY & mFlags) {
    state.mIsMarginRoot = PR_TRUE;
  }

  // Prepare inline-reflow engine
  nsInlineReflow inlineReflow(state.mLineLayout, state, this, PR_TRUE);
  state.mInlineReflow = &inlineReflow;
  state.mLineLayout.PushInline(&inlineReflow);

  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    if (nsnull != aReflowState.mRunInFrame) {
#ifdef NOISY_RUNIN
      ListTag(stdout);
      printf(": run-in from: ");
      aReflowState.mRunInFrame->ListTag(stdout);
      printf("\n");
#endif
      // Take frames away from the run-in frame
      TakeRunInFrames(aReflowState.mRunInFrame);
    }
    if (!DrainOverflowLines()) {
      RenumberLists(state);
      rv = InitialReflow(state);
    }
    else {
      RenumberLists(state);
      rv = ResizeReflow(state);
    }
    mState &= ~NS_FRAME_FIRST_REFLOW;
  }
  else if (eReflowReason_Incremental == state.reason) {
#if XXX
    // We can have an overflow here if our parent doesn't bother to
    // continue us
    DrainOverflowLines();
#endif
    nsIFrame* target;
    state.reflowCommand->GetTarget(target);
    if (this == target) {
      RenumberLists(state);
      nsIReflowCommand::ReflowType type;
      state.reflowCommand->GetType(type);
      switch (type) {
      case nsIReflowCommand::FrameAppended:
        rv = FrameAppendedReflow(state);
        break;

      case nsIReflowCommand::FrameInserted:
        rv = FrameInsertedReflow(state);
        break;

      case nsIReflowCommand::FrameRemoved:
        rv = FrameRemovedReflow(state);
        break;

      case nsIReflowCommand::StyleChanged:
        rv = StyleChangedReflow(state);
        break;

      default:
        // Map any other incremental operations into full reflows
        rv = ResizeReflow(state);
        break;
      }
    }
    else {
      // Get next frame in reflow command chain
      state.reflowCommand->GetNext(state.mNextRCFrame);

      // Now do the reflow
      rv = ChildIncrementalReflow(state);
    }
  }
  else if ((eReflowReason_Resize == state.reason) ||
           (eReflowReason_StyleChange == state.reason)) {
    DrainOverflowLines();
    rv = ResizeReflow(state);
  }

  // Compute our final size
  ComputeFinalSize(state, aMetrics);

  BuildFloaterList();

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines);
    VerifyLines(mLines);
  }
#endif
  aStatus = rv;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("exit nsBlockFrame::Reflow: size=%d,%d rv=%x",
                  aMetrics.width, aMetrics.height, rv));
  return NS_OK;
}

void
nsBlockFrame::BuildFloaterList()
{
  nsIFrame* head = nsnull;
  nsIFrame* current = nsnull;
  LineData* line = mLines;
  while (nsnull != line) {
    if (nsnull != line->mFloaters) {
      nsVoidArray& array = *line->mFloaters;
      PRInt32 i, n = array.Count();
      for (i = 0; i < n; i++) {
        nsPlaceholderFrame* ph = (nsPlaceholderFrame*) array[i];
        nsIFrame* floater = ph->GetAnchoredItem();
        if (nsnull == head) {
          current = head = floater;
        }
        else {
          current->SetNextSibling(floater);
          current = floater;
        }
      }
    }
    line = line->mNext;
  }

  // Terminate end of floater list just in case a floater was removed
  if (nsnull != current) {
    current->SetNextSibling(nsnull);
  }
  mFloaters = head;
}

void
nsBlockFrame::RenumberLists(nsBlockReflowState& aState)
{
  // Setup initial list ordinal value
  PRInt32 ordinal = 1;
  nsIHTMLContent* hc;
  if (mContent && (NS_OK == mContent->QueryInterface(kIHTMLContentIID, (void**) &hc))) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetAttribute(nsHTMLAtoms::start, value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        ordinal = value.GetIntValue();
        if (ordinal <= 0) {
          ordinal = 1;
        }
      }
    }
    NS_RELEASE(hc);
  }
  aState.mNextListOrdinal = ordinal;

  // Get to first-in-flow
  nsBlockFrame* block = this;
  while (nsnull != block->mPrevInFlow) {
    block = (nsBlockFrame*) block->mPrevInFlow;
  }

  // For each flow-block...
  while (nsnull != block) {
    // For each frame in the flow-block...
    nsIFrame* frame = block->mLines ? block->mLines->mFirstChild : nsnull;
    while (nsnull != frame) {
      // If the frame is a list-item and the frame implements our
      // block frame API then get it's bullet and set the list item
      // ordinal.
      const nsStyleDisplay* display;
      frame->GetStyleData(eStyleStruct_Display,
                          (const nsStyleStruct*&) display);
      if (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay) {
        // Make certain that the frame isa block-frame in case
        // something foriegn has crept in.
        nsBlockFrame* listItem;
        if (NS_OK == frame->QueryInterface(kBlockFrameCID,
                                           (void**) &listItem)) {
          if (nsnull != listItem->mBullet) {
            listItem->mBullet->SetListItemOrdinal(aState);
          }
        }
      }
      frame->GetNextSibling(frame);
    }
    block = (nsBlockFrame*) block->mNextInFlow;
  }
}

void
nsBlockFrame::ComputeFinalSize(nsBlockReflowState& aState,
                               nsHTMLReflowMetrics& aMetrics)
{
  // XXX handle floater problems this way...
  PRBool isFixedWidth =
    eHTMLFrameConstraint_FixedContent == aState.widthConstraint;
  PRBool isFixedHeight =
    eHTMLFrameConstraint_FixedContent == aState.heightConstraint;
#if 0
  if (NS_STYLE_FLOAT_NONE != aState.mStyleDisplay->mFloats) {
    isFixedWidth = PR_FALSE;
    isFixedHeight = PR_FALSE;
  }
#else
  if (NS_BODY_SHRINK_WRAP & mFlags) {
    isFixedWidth = PR_FALSE;
    isFixedHeight = PR_FALSE;
  }
#endif

  // Compute final width
  if (isFixedWidth) {
    // Use style defined width
    aMetrics.width = aState.mBorderPadding.left +
      aState.minWidth + aState.mBorderPadding.right;
  }
  else {
    nscoord computedWidth = aState.mKidXMost + aState.mBorderPadding.right;
    PRBool compact = PR_FALSE;
    if (NS_STYLE_DISPLAY_COMPACT == aState.mStyleDisplay->mDisplay) {
      // If we are display: compact AND we have no lines or we have
      // exactly one line and that line is not a block line AND that
      // line doesn't end in a BR of any sort THEN we remain a compact
      // frame.
      if ((nsnull == mLines) ||
          ((nsnull == mLines->mNext) && !mLines->IsBlock() &&
           (NS_STYLE_CLEAR_NONE == mLines->mBreakType) &&
           (computedWidth <= aState.mCompactMarginWidth))) {
        compact = PR_TRUE;
      }
    }

    // There are two options here. We either shrink wrap around our
    // contents or we fluff out to the maximum available width. Note:
    // We always shrink wrap when given an unconstrained width.
    if ((0 == (NS_BODY_SHRINK_WRAP & mFlags)) &&
        !aState.mUnconstrainedWidth &&
        !compact) {
      // Fluff out to the max width if we aren't already that wide
      if (computedWidth < aState.maxSize.width) {
        computedWidth = aState.maxSize.width;
      }
    }
    aMetrics.width = computedWidth;
  }

  // Compute final height
  if (isFixedHeight) {
    // Use style defined height
    aMetrics.height = aState.mBorderPadding.top +
      aState.minHeight + aState.mBorderPadding.bottom;
  }
  else {
    // Shrink wrap our height around our contents.
    if (aState.mIsMarginRoot) {
      // When we are a margin root make sure that our last childs
      // bottom margin is fully applied.
      if ((NS_BODY_NO_AUTO_MARGINS & mFlags) &&
          (NS_CARRIED_BOTTOM_MARGIN_IS_AUTO & aState.mPrevMarginFlags)) {
        // Do not apply last auto margin when the last childs margin
        // is auto and we are configured to ignore them.
      }
      else {
        aState.mY += aState.mPrevBottomMargin;
      }
    }
    aState.mY += aState.mBorderPadding.bottom;
    aMetrics.height = aState.mY;
  }

  // Return top and bottom margin information
  if (aState.mIsMarginRoot) {
    aMetrics.mCarriedOutTopMargin = 0;
    aMetrics.mCarriedOutBottomMargin = 0;
    aMetrics.mCarriedOutMarginFlags = 0;
  }
  else {
    aMetrics.mCarriedOutTopMargin = aState.mCollapsedTopMargin;
    aMetrics.mCarriedOutBottomMargin = aState.mPrevBottomMargin;
    aMetrics.mCarriedOutMarginFlags = aState.mCarriedOutMarginFlags;
    if (NS_CARRIED_BOTTOM_MARGIN_IS_AUTO & aState.mPrevMarginFlags) {
      aMetrics.mCarriedOutMarginFlags |= NS_CARRIED_BOTTOM_MARGIN_IS_AUTO;
    }
  }

  // Special check for zero sized content: If our content is zero
  // sized then we collapse into nothingness.
  if ((eHTMLFrameConstraint_Unconstrained == aState.widthConstraint) &&
      (eHTMLFrameConstraint_Unconstrained == aState.heightConstraint) &&
      ((0 == aState.mKidXMost - aState.mBorderPadding.left) &&
       (0 == aState.mY - aState.mBorderPadding.top))) {
    aMetrics.width = 0;
    aMetrics.height = 0;
  }

  aMetrics.ascent = aMetrics.height;
  aMetrics.descent = 0;

  // XXX this needs ALOT OF REWORKING
  if (aState.mComputeMaxElementSize) {
    nscoord maxWidth, maxHeight;

    if (aState.mNoWrap) {
      maxWidth = 0;
      maxHeight = 0;

      // Find the maximum line-box width and use that as the
      // max-element width
      LineData* line = mLines;
      while (nsnull != line) {
        nscoord xm = line->mBounds.XMost();
        if (xm > maxWidth) {
          maxWidth = xm;
        }
        line = line->mNext;
      }

      // XXX winging it!
      maxHeight = aState.mMaxElementSize.height +
        aState.mBorderPadding.top + aState.mBorderPadding.bottom;
    }
    else {
      maxWidth = aState.mMaxElementSize.width +
        aState.mBorderPadding.left + aState.mBorderPadding.right;
      maxHeight = aState.mMaxElementSize.height +
        aState.mBorderPadding.top + aState.mBorderPadding.bottom;

      // XXX This is still an approximation of the truth:

      // 1. it doesn't take into account that after a floater is
      // placed we always place at least *something* on the line.

      // 2. if a floater doesn't start at the left margin (because
      // it's placed relative to a floater on a preceeding line, say),
      // then we get the wrong answer.

      LineData* line = mLines;
      while (nsnull != line) {
        if (nsnull != line->mFloaters) {
          nsRect r;
          nsMargin floaterMargin;

          // Sum up the widths of the floaters on this line
          PRInt32 sum = 0;
          PRInt32 n = line->mFloaters->Count();
          for (PRInt32 i = 0; i < n; i++) {
            nsPlaceholderFrame* placeholder = (nsPlaceholderFrame*)
              line->mFloaters->ElementAt(i);
            nsIFrame* floater = placeholder->GetAnchoredItem();
            floater->GetRect(r);
            const nsStyleDisplay* floaterDisplay;
            const nsStyleSpacing* floaterSpacing;
            floater->GetStyleData(eStyleStruct_Display,
                                  (const nsStyleStruct*&)floaterDisplay);
            floater->GetStyleData(eStyleStruct_Spacing,
                                  (const nsStyleStruct*&)floaterSpacing);

            // Compute the margin for the floater (again!)
            nsHTMLReflowState::ComputeMarginFor(floater, &aState,
                                                floaterMargin);

            nscoord width = r.width + floaterMargin.left + floaterMargin.right;
            NS_ASSERTION((NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) ||
                         (NS_STYLE_FLOAT_RIGHT == floaterDisplay->mFloats),
                         "invalid float type");
            sum += width;
          }

          // See if we need a larger max-element width
          if (sum > maxWidth) {
            maxWidth = sum;
          }
        }
        line = line->mNext;
      }
    }

    // Store away the final value
    aMetrics.maxElementSize->width = maxWidth;
    aMetrics.maxElementSize->height = maxHeight;
#ifdef NOISY_MAX_ELEMENT_SIZE
    ListTag(stdout);
    printf(": max-element-size:%d,%d desired:%d,%d\n",
           maxWidth, maxHeight, aMetrics.width, aMetrics.height);
#endif
  }

  // Compute the combined area of our children
  // XXX take into account the overflow->clip property!
  nscoord x0 = 0, y0 = 0, x1 = aMetrics.width, y1 = aMetrics.height;
  LineData* line = mLines;
  while (nsnull != line) {
    // Compute min and max x/y values for the reflowed frame's
    // combined areas
    nscoord x = line->mCombinedArea.x;
    nscoord y = line->mCombinedArea.y;
    nscoord xmost = x + line->mCombinedArea.width;
    nscoord ymost = y + line->mCombinedArea.height;
    if (x < x0) x0 = x;
    if (xmost > x1) x1 = xmost;
    if (y < y0) y0 = y;
    if (ymost > y1) y1 = ymost;

    // If the line has floaters, factor those in as well
    nsVoidArray* floaters = line->mFloaters;
    if (nsnull != floaters) {
      PRInt32 i, n = floaters->Count();
      for (i = 0; i < n; i++) {
        nsPlaceholderFrame* ph = (nsPlaceholderFrame*) floaters->ElementAt(i);
        nsIFrame* frame = ph->GetAnchoredItem();
        // XXX This is wrong! The floater may have a combined area
        // that exceeds its bounding box!
        nsRect r;
        frame->GetRect(r);
        if (r.x < x0) x0 = r.x;
        if (r.XMost() > x1) x1 = r.XMost();
        if (r.y < y0) y0 = r.y;
        if (r.YMost() > y1) y1 = r.YMost();
      }
    }
    line = line->mNext;
  }
  if (nsnull != mBullet) {
    nsRect r;
    mBullet->GetRect(r);
    if (r.x < x0) x0 = r.x;
    if (r.XMost() > x1) x1 = r.XMost();
    if (r.y < y0) y0 = r.y;
    if (r.YMost() > y1) y1 = r.YMost();
  }
  aMetrics.mCombinedArea.x = x0;
  aMetrics.mCombinedArea.y = y0;
  aMetrics.mCombinedArea.width = x1 - x0;
  aMetrics.mCombinedArea.height = y1 - y0;

  // If the combined area of our children exceeds our bounding box
  // then set the NS_FRAME_OUTSIDE_CHILDREN flag, otherwise clear it.
  if ((aMetrics.mCombinedArea.x < 0) ||
      (aMetrics.mCombinedArea.y < 0) ||
      (aMetrics.mCombinedArea.XMost() > aMetrics.width) ||
      (aMetrics.mCombinedArea.YMost() > aMetrics.height)) {
    mState |= NS_FRAME_OUTSIDE_CHILDREN;
  }
  else {
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
  }
}

nsresult
nsBlockFrame::AppendNewFrames(nsIPresContext& aPresContext,
                              nsIFrame* aNewFrame)
{
  // Get our last line and then get its last child
  nsIFrame* lastFrame;
  LineData* lastLine = LastLine(mLines);
  if (nsnull != lastLine) {
    lastFrame = lastLine->LastChild();
  } else {
    lastFrame = nsnull;
  }

  // Add the new frames to the sibling list; wrap any frames that
  // require wrapping
  if (nsnull != lastFrame) {
    lastFrame->SetNextSibling(aNewFrame);
  }
  nsresult rv;

  // Make sure that new inlines go onto the end of the lastLine when
  // the lastLine is mapping inline frames.
  PRInt32 pendingInlines = 0;
  if (nsnull != lastLine) {
    if (!lastLine->IsBlock()) {
      pendingInlines = 1;
    }
  }

  // Now create some lines for the new frames
  nsIFrame* prevFrame = lastFrame;
  for (nsIFrame* frame = aNewFrame; nsnull != frame; frame->GetNextSibling(frame)) {
    // See if the child is a block or non-block
    const nsStyleDisplay* kidDisplay;
    rv = frame->GetStyleData(eStyleStruct_Display,
                             (const nsStyleStruct*&) kidDisplay);
    if (NS_OK != rv) {
      return rv;
    }
    const nsStylePosition* kidPosition;
    rv = frame->GetStyleData(eStyleStruct_Position,
                             (const nsStyleStruct*&) kidPosition);
    if (NS_OK != rv) {
      return rv;
    }
    PRBool isBlock =
      nsLineLayout::TreatFrameAsBlock(kidDisplay, kidPosition);

    // See if we need to move the frame outside of the flow, and insert a
    // placeholder frame in its place
    nsIFrame* placeholder;
    if (MoveFrameOutOfFlow(aPresContext, frame, kidDisplay, kidPosition,
                           placeholder)) {
      // Reset the previous frame's next sibling pointer
      if (nsnull != prevFrame) {
        prevFrame->SetNextSibling(placeholder);
      }

      // The placeholder frame is always inline
      frame = placeholder;
      isBlock = PR_FALSE;
    }
    else {
      // Wrap the frame in a view if necessary
      nsIStyleContext* kidSC;
      frame->GetStyleContext(kidSC);
      rv = CreateViewForFrame(aPresContext, frame, kidSC, PR_FALSE);
      NS_RELEASE(kidSC);
      if (NS_OK != rv) {
        return rv;
      }
    }

    // If the child is an inline then add it to the lastLine (if it's
    // an inline line, otherwise make a new line). If the child is a
    // block then make a new line and put the child in that line.
    if (isBlock) {
      // If the previous line has pending inline data to be reflowed,
      // do so now.
      if (0 != pendingInlines) {
        // Set this to true in case we don't end up reflowing all of the
        // frames on the line (because they end up being pushed).
        lastLine->MarkDirty();
        pendingInlines = 0;
      }

      // Create a line for the block
      LineData* line = new LineData(frame, 1, LINE_IS_BLOCK);
      if (nsnull == line) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (nsnull == lastLine) {
        mLines = line;
      }
      else {
        lastLine->mNext = line;
      }
      lastLine = line;
    }
    else {
      // Queue up the inlines for reflow later on
      if (0 == pendingInlines) {
        LineData* line = new LineData(frame, 0, 0);
        if (nsnull == line) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        if (nsnull == lastLine) {
          mLines = line;
        }
        else {
          lastLine->mNext = line;
        }
        lastLine = line;
      }
      lastLine->mChildCount++;
      pendingInlines++;
    }

    // Remember the previous frame
    prevFrame = frame;
  }

  if (0 != pendingInlines) {
    // Set this to true in case we don't end up reflowing all of the
    // frames on the line (because they end up being pushed).
    lastLine->MarkDirty();
  }

  return NS_OK;
}

nsresult
nsBlockFrame::InitialReflow(nsBlockReflowState& aState)
{
  // Generate text-run information
  nsresult rv = FindTextRuns(aState);
  if (NS_OK != rv) {
    return rv;
  }

  // Reflow everything
  aState.GetAvailableSpace();
  return ResizeReflow(aState);
}

void
nsBlockFrame::RecoverLineMargins(nsBlockReflowState& aState,
                                 LineData* aLine,
                                 nscoord& aTopMarginResult,
                                 nscoord& aBottomMarginResult)
{
  nscoord childsCarriedOutTopMargin = aLine->mCarriedOutTopMargin;
  nscoord childsCarriedOutBottomMargin = aLine->mCarriedOutBottomMargin;
  nscoord childsTopMargin = 0;
  nscoord childsBottomMargin = 0;
  if (aLine->IsBlock()) {
    // Recover the block frames computed bottom margin value
    nsIFrame* frame = aLine->mFirstChild;
    if (nsnull != frame) {
      const nsStyleSpacing* spacing;
      frame->GetStyleData(eStyleStruct_Spacing,
                          (const nsStyleStruct*&) spacing);
      if (nsnull != spacing) {
        nsMargin margin;
        nsInlineReflow::CalculateBlockMarginsFor(aState.mPresContext, frame,
                                                 &aState, spacing, margin);
        childsTopMargin = margin.top;
        childsBottomMargin = margin.bottom;
      }
    }
  }

  // Collapse the carried-out-margins with the childs margins
  aBottomMarginResult =
    nsInlineReflow::MaxMargin(childsCarriedOutBottomMargin, childsBottomMargin);
  aTopMarginResult =
    nsInlineReflow::MaxMargin(childsCarriedOutTopMargin, childsTopMargin);
}

nsresult
nsBlockFrame::FrameAppendedReflow(nsBlockReflowState& aState)
{
  nsresult  rv = NS_OK;

  // Get the first of the newly appended frames
  nsIFrame* firstAppendedFrame;
  aState.reflowCommand->GetChildFrame(firstAppendedFrame);

  // Add the new frames to the child list, and create new lines. Each
  // impacted line will be marked dirty
  AppendNewFrames(aState.mPresContext, firstAppendedFrame);
  RenumberLists(aState);

  // Generate text-run information
  rv = FindTextRuns(aState);
  if (NS_OK != rv) {
    return rv;
  }

  // Recover our reflow state
  LineData* firstDirtyLine = mLines;
  LineData* lastCleanLine = nsnull;
  PRBool haveRunIn = PR_FALSE;
  while (nsnull != firstDirtyLine) {
    if (firstDirtyLine->IsDirty()) {
      break;
    }

    // Recover run-in state
    if (firstDirtyLine->IsBlock()) {
      nsIFrame* frame = firstDirtyLine->mFirstChild;
      const nsStyleDisplay* display;
      frame->GetStyleData(eStyleStruct_Display,
                          (const nsStyleStruct*&) display);
      if ((NS_STYLE_DISPLAY_RUN_IN == display->mDisplay) ||
          (NS_STYLE_DISPLAY_COMPACT == display->mDisplay)) {
        haveRunIn = PR_TRUE;
        break;
      }
    }

    // Recover xmost
    nscoord xmost = firstDirtyLine->mBounds.XMost();
    if (xmost > aState.mKidXMost) {
      aState.mKidXMost = xmost;
    }

    // Recover the mPrevBottomMargin and the mCollapsedTopMargin values
    nscoord topMargin, bottomMargin;
    RecoverLineMargins(aState, firstDirtyLine, topMargin, bottomMargin);
    if (0 == firstDirtyLine->mBounds.height) {
      // For zero height lines, collapse the lines top and bottom
      // margins together to produce the effective bottomMargin value.
      bottomMargin = nsInlineReflow::MaxMargin(topMargin, bottomMargin);
      bottomMargin = nsInlineReflow::MaxMargin(aState.mPrevBottomMargin,
                                               bottomMargin);
    }
    else {
      aState.mPrevMarginFlags = firstDirtyLine->GetMarginFlags();
    }
    aState.mPrevBottomMargin = bottomMargin;

    // Advance Y to be below the line.
    aState.mY = firstDirtyLine->mBounds.YMost();

    // Advance to the next line
    lastCleanLine = firstDirtyLine;
    firstDirtyLine = firstDirtyLine->mNext;
  }

  if (haveRunIn) {
    // XXX For now, reflow the world when we contain a frame that is a
    // run-in/compact frame. (lazy code)
    lastCleanLine = nsnull;
    firstDirtyLine = mLines;
    aState.mY = aState.mBorderPadding.top;
    aState.mPrevMarginFlags = 0;
    aState.mPrevBottomMargin = 0;
  }

  if (nsnull != lastCleanLine) {
    // Unplace any floaters on the lastCleanLine and every line that
    // follows it (which shouldn't be much given that this is
    // frame-appended reflow)
    LineData* line = lastCleanLine;
    while (nsnull != line) {
      line->UnplaceFloaters(aState.mSpaceManager);
      line = line->mNext;
    }

    // Place any floaters the last clean line has
    if (nsnull != lastCleanLine->mFloaters) {
      aState.mCurrentLine = lastCleanLine;
      aState.PlaceFloaters(lastCleanLine->mFloaters, PR_TRUE);
    }

    // Apply any clear-after semantics the line might have
//XXX    nscoord y0, dy;
    switch (lastCleanLine->mBreakType) {
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                     ("line %p: ClearFloaters: y=%d, break-type=%d\n",
                      lastCleanLine, aState.mY, lastCleanLine->mBreakType));
//XXX      y0 = aState.mY;
      aState.ClearFloaters(lastCleanLine->mBreakType);
#if 0
      dy = aState.mY - y0;
      if (dy > aState.mPrevBottomMargin) {
        aState.mPrevBottomMargin = dy;
        aState.mPrevMarginFlags |= ALREADY_APPLIED_BOTTOM_MARGIN;
      }
#endif
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                     ("  cleared floaters, now y=%d\n", aState.mY));
      break;
    }
  }

  aState.GetAvailableSpace();
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockReflowState::FrameAppendedReflow: y=%d firstDirtyLine=%p",
      aState.mY, firstDirtyLine));

  // Reflow lines from there forward
  aState.mPrevLine = lastCleanLine;
  return ReflowLinesAt(aState, firstDirtyLine);
}

// XXX keep the text-run data in the first-in-flow of the block
nsresult
nsBlockFrame::FindTextRuns(nsBlockReflowState& aState)
{
  // Destroy old run information first
  nsTextRun::DeleteTextRuns(mTextRuns);
  mTextRuns = nsnull;
  aState.mLineLayout.ResetTextRuns();

  // Ask each child that implements nsIInlineReflow to find its text runs
  LineData* line = mLines;
  while (nsnull != line) {
    if (!line->IsBlock()) {
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->ChildCount();
      while (--n >= 0) {
        nsIHTMLReflow* hr;
        if (NS_OK == frame->QueryInterface(kIHTMLReflowIID, (void**)&hr)) {
          nsresult rv = hr->FindTextRuns(aState.mLineLayout);
          if (NS_OK != rv) {
            return rv;
          }
        }
        else {
          // A frame that doesn't implement nsIHTMLReflow isn't text
          // therefore it will end an open text run.
          aState.mLineLayout.EndTextRun();
        }
        frame->GetNextSibling(frame);
      }
    }
    else {
      // A frame that doesn't implement nsIInlineReflow isn't text
      // therefore it will end an open text run.
      aState.mLineLayout.EndTextRun();
    }
    line = line->mNext;
  }
  aState.mLineLayout.EndTextRun();

  // Now take the text-runs away from the line layout engine.
  mTextRuns = aState.mLineLayout.TakeTextRuns();

  return NS_OK;
}

// XXX rewrite this
nsresult
nsBlockFrame::FrameInsertedReflow(nsBlockReflowState& aState)
{
  // Get the inserted frame
  nsIFrame* newFrame;
  aState.reflowCommand->GetChildFrame(newFrame);

  // Get the previous sibling frame
  nsIFrame* prevSibling;
  aState.reflowCommand->GetPrevSiblingFrame(prevSibling);

  // Insert the frame. This marks the line dirty...
  InsertNewFrame(aState.mPresContext, this, newFrame, prevSibling);

#if XXX
  LineData* line = mLines;
  while (nsnull != line->mNext) {
    if (line->IsDirty()) {
      break;
    }
    line = line->mNext;
  }
  NS_ASSERTION(nsnull != line, "bad inserted reflow");
  //XXX return ReflowDirtyLines(aState, line);
#endif

  // XXX Correct implementation: reflow the dirty lines only; all
  // other lines can be moved; recover state before first dirty line.

  // XXX temporary
  aState.mSpaceManager->ClearRegions();
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

// XXX rewrite this
nsresult
nsBlockFrame::FrameRemovedReflow(nsBlockReflowState& aState)
{
  if (nsnull == mLines) {
    return NS_OK;
  } 

  // Get the deleted frame
  nsIFrame* deletedFrame;
  aState.reflowCommand->GetChildFrame(deletedFrame);

  // Find the previous sibling frame
  nsIFrame* prevSibling = nsnull;
  for (nsIFrame* f = mLines->mFirstChild; f != deletedFrame;
       f->GetNextSibling(f)) {
    if (nsnull == f) {
      // We didn't find the deleted frame in our child list
      NS_WARNING("Can't find deleted frame");
      return NS_OK;
    }

    prevSibling = f;
  }

  // Find the line that contains deletedFrame; we also find the pointer to
  // the line.
  nsBlockFrame* flow = this;
  LineData** linep = &flow->mLines;
  LineData* line = flow->mLines;
  while (nsnull != line) {
    if (line->Contains(deletedFrame)) {
      break;
    }
    linep = &line->mNext;
    line = line->mNext;
  }
  NS_ASSERTION(nsnull != line, "can't find deleted frame in lines");

  // Remove frame and its continuations
  while (nsnull != deletedFrame) {
    while ((nsnull != line) && (nsnull != deletedFrame)) {
#ifdef NS_DEBUG
      nsIFrame* parent;
      deletedFrame->GetGeometricParent(parent);
      NS_ASSERTION(flow == parent, "messed up delete code");
#endif
      NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsBlockFrame::ContentDeleted: deadFrame=%p", deletedFrame));

      // See if the frame is a floater (actually, the floaters
      // placeholder). If it is, then destroy the floated frame too.
      const nsStyleDisplay* display;
      nsresult rv = deletedFrame->GetStyleData(eStyleStruct_Display,
                                               (const nsStyleStruct*&)display);
      if (NS_SUCCEEDED(rv) && (nsnull != display)) {
        // XXX Sanitize "IsFloating" question *everywhere* (add a
        // static method on nsFrame?)
        if (NS_STYLE_FLOAT_NONE != display->mFloats) {
          nsPlaceholderFrame* ph = (nsPlaceholderFrame*) deletedFrame;
          nsIFrame* floater = ph->GetAnchoredItem();
          if (nsnull != floater) {
            floater->DeleteFrame(aState.mPresContext);
            if (nsnull != line->mFloaters) {
              // Wipe out the floater array for this line. It will get
              // recomputed during reflow anyway.
              delete line->mFloaters;
              line->mFloaters = nsnull;
            }
          }
        }
      }

      // Remove deletedFrame from the line
      if (line->mFirstChild == deletedFrame) {
        nsIFrame* nextFrame;
        deletedFrame->GetNextSibling(nextFrame);
        line->mFirstChild = nextFrame;
      }

      // Take deletedFrame out of the sibling list
      if (nsnull != prevSibling) {
        nsIFrame* nextFrame;
        deletedFrame->GetNextSibling(nextFrame);
        prevSibling->SetNextSibling(nextFrame);
      }

      // Destroy frame; capture its next-in-flow first in case we need
      // to destroy that too.
      nsIFrame* nextInFlow;
      deletedFrame->GetNextInFlow(nextInFlow);
      if (nsnull != nextInFlow) {
        deletedFrame->BreakFromNextFlow();
      }
      deletedFrame->DeleteFrame(aState.mPresContext);
      deletedFrame = nextInFlow;

      // If line is empty, remove it now
      LineData* next = line->mNext;
      if (0 == --line->mChildCount) {
        *linep = next;
        line->mNext = nsnull;
        delete line;
      }
      else {
        linep = &line->mNext;
      }
      line = next;
    }

    // Advance to next flow block if the frame has more continuations
    if (nsnull != deletedFrame) {
      flow = (nsBlockFrame*) flow->mNextInFlow;
      NS_ASSERTION(nsnull != flow, "whoops, continuation without a parent");
      line = flow->mLines;
      prevSibling = nsnull;
    }
  }

  // XXX Correct implementation: reflow the dirty lines only; all
  // other lines can be moved; recover state before first dirty line.

  // XXX temporary
  aState.mSpaceManager->ClearRegions();
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

// XXX Todo: some incremental reflows are passing through this block
// and into a child block; those cannot impact our text-runs. In that
// case skip the FindTextRuns work.

// XXX easy optimizations: find the line that contains the next child
// in the reflow-command path and mark it dirty and only reflow it;
// recover state before it, slide lines down after it.

nsresult
nsBlockFrame::ChildIncrementalReflow(nsBlockReflowState& aState)
{
#if 0
  // Generate text-run information; this will also "fluff out" any
  // inline children's frame tree.
  nsresult rv = FindTextRuns(aState);
  if (NS_OK != rv) {
    return rv;
  }
#endif

  // XXX temporary
  aState.mSpaceManager->ClearRegions();
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsBlockFrame::StyleChangedReflow(nsBlockReflowState& aState)
{
  // XXX temporary
  aState.mSpaceManager->ClearRegions();
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsBlockFrame::ResizeReflow(nsBlockReflowState& aState)
{
  // Mark everything dirty
  LineData* line = mLines;
  while (nsnull != line) {
    line->MarkDirty();
    line = line->mNext;
  }

  // Reflow all of our lines
  aState.GetAvailableSpace();
  aState.mPrevLine = nsnull;
  return ReflowLinesAt(aState, mLines);
}

nsresult
nsBlockFrame::ReflowLinesAt(nsBlockReflowState& aState, LineData* aLine)
{
  // Inform line layout of where the text runs are
  aState.mLineLayout.SetReflowTextRuns(mTextRuns);

  // Reflow the lines that are already ours
  while (nsnull != aLine) {
    nsReflowStatus rs;
    if (!ReflowLine(aState, aLine, rs)) {
      if (NS_IS_REFLOW_ERROR(rs)) {
        return nsresult(rs);
      }
      return NS_FRAME_NOT_COMPLETE;
    }
    aState.mLineLayout.NextLine();
    aState.mPrevLine = aLine;
    aLine = aLine->mNext;
  }

  // Pull data from a next-in-flow if we can
  while (nsnull != aState.mNextInFlow) {
    // Grab first line from our next-in-flow
    aLine = aState.mNextInFlow->mLines;
    if (nsnull == aLine) {
      aState.mNextInFlow = (nsBlockFrame*) aState.mNextInFlow->mNextInFlow;
      continue;
    }
    // XXX See if the line is not dirty; if it's not maybe we can
    // avoid the pullup if it can't fit?
    aState.mNextInFlow->mLines = aLine->mNext;
    aLine->mNext = nsnull;
    if (0 == aLine->ChildCount()) {
      // The line is empty. Try the next one.
      NS_ASSERTION(nsnull == aLine->mFirstChild, "bad empty line");
      aLine->mNext = aState.mFreeList;
      aState.mFreeList = aLine;
      continue;
    }

    // Make the children in the line ours.
    nsIFrame* frame = aLine->mFirstChild;
    nsIFrame* lastFrame = nsnull;
    PRInt32 n = aLine->ChildCount();
    while (--n >= 0) {
      nsIFrame* geometricParent;
      nsIFrame* contentParent;
      frame->GetGeometricParent(geometricParent);
      frame->GetContentParent(contentParent);
      if (contentParent == geometricParent) {
        frame->SetContentParent(this);
      }
      frame->SetGeometricParent(this);
      lastFrame = frame;
      frame->GetNextSibling(frame);
    }
    lastFrame->SetNextSibling(nsnull);

    // Add line to our line list
    if (nsnull == aState.mPrevLine) {
      NS_ASSERTION(nsnull == mLines, "bad aState.mPrevLine");
      mLines = aLine;
    }
    else {
      NS_ASSERTION(nsnull == aState.mPrevLine->mNext, "bad aState.mPrevLine");
      aState.mPrevLine->mNext = aLine;
      aState.mPrevChild->SetNextSibling(aLine->mFirstChild);
    }
#ifdef NS_DEBUG
    if (GetVerifyTreeEnable()) {
      VerifyChildCount(mLines);
    }
#endif

    // Now reflow it and any lines that it makes during it's reflow.
    while (nsnull != aLine) {
      nsReflowStatus rs;
      if (!ReflowLine(aState, aLine, rs)) {
        if (NS_IS_REFLOW_ERROR(rs)) {
          return nsresult(rs);
        }
        return NS_FRAME_NOT_COMPLETE;
      }
      aState.mLineLayout.NextLine();
      aState.mPrevLine = aLine;
      aLine = aLine->mNext;
    }
  }

  return NS_FRAME_COMPLETE;
}

/**
 * Reflow a line. The line will either contain a single block frame
 * or contain 1 or more inline frames.
 */
PRBool
nsBlockFrame::ReflowLine(nsBlockReflowState& aState,
                         LineData* aLine,
                         nsReflowStatus& aReflowResult)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockFrame::ReflowLine: line=%p", aLine));

  // Setup the line-layout for the new line
  aState.mLineLayout.Reset();
  aState.mCurrentLine = aLine;
  aState.mInlineReflowPrepared = PR_FALSE;

  // If the line already has floaters on it from last time, remove
  // them from the spacemanager now.
  if (nsnull != aLine->mFloaters) {
    if (eReflowReason_Resize != aState.reason) {
      aLine->UnplaceFloaters(aState.mSpaceManager);
    }
    delete aLine->mFloaters;
    aLine->mFloaters = nsnull;
  }

  aLine->ClearDirty();
  aLine->SetNeedDidReflow();

  // Reflow mapped frames in the line
  nsBlockFrame* nextInFlow;
  PRBool keepGoing = PR_FALSE;
  PRInt32 n = aLine->ChildCount();
  if (0 != n) {
    nsIFrame* frame = aLine->mFirstChild;
#ifdef NS_DEBUG
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) display);
    const nsStylePosition* position;
    frame->GetStyleData(eStyleStruct_Position,
                        (const nsStyleStruct*&) position);
    PRBool isBlock = nsLineLayout::TreatFrameAsBlock(display, position);
    NS_ASSERTION(isBlock == aLine->IsBlock(), "bad line isBlock");
#endif 
   if (aLine->IsBlock()) {
      keepGoing = ReflowBlockFrame(aState, aLine, frame, aReflowResult);
      return keepGoing;
    }
    else {
      while (--n >= 0) {
        PRBool addedToLine;
        keepGoing = ReflowInlineFrame(aState, aLine, frame, aReflowResult,
                                      addedToLine);
        if (!keepGoing) {
          // It is possible that one or more of next lines are empty
          // (because of DeleteNextInFlowsFor). If so, delete them now
          // in case we are finished.
          LineData* nextLine = aLine->mNext;
          while ((nsnull != nextLine) && (0 == nextLine->ChildCount())) {
            // Discard empty lines immediately. Empty lines can happen
            // here because of DeleteNextInFlowsFor not being able to
            // delete lines.
            aLine->mNext = nextLine->mNext;
            NS_ASSERTION(nsnull == nextLine->mFirstChild, "bad empty line");
            nextLine->mNext = aState.mFreeList;
            aState.mFreeList = nextLine;
            nextLine = aLine->mNext;
          }
          goto done;
        }
        frame->GetNextSibling(frame);
        if (addedToLine) {
          NS_ASSERTION(nsnull != frame, "added to line but no new frame");
          n++;
        }
      }
    }
  }

  // Pull frames from the next line until we can't
  while (nsnull != aLine->mNext) {
    PRBool addedToLine;
    keepGoing = PullFrame(aState, aLine, &aLine->mNext,
                          PR_FALSE, aReflowResult, addedToLine);
    if (!keepGoing) {
      goto done;
    }
  }

  // Pull frames from the next-in-flow(s) until we can't
  nextInFlow = aState.mNextInFlow;
  while (nsnull != nextInFlow) {
    LineData* line = nextInFlow->mLines;
    if (nsnull == line) {
      nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
      aState.mNextInFlow = nextInFlow;
      continue;
    }
    PRBool addedToLine;
    keepGoing = PullFrame(aState, aLine, &nextInFlow->mLines,
                          PR_TRUE, aReflowResult, addedToLine);
    if (!keepGoing) {
      goto done;
    }
  }
  keepGoing = PR_TRUE;

done:;
  if (!aLine->IsBlock()) {
    return PlaceLine(aState, aLine, aReflowResult);
  }
  return keepGoing;
}

// block's vs. inlines's:

// 1. break-before/break-after (is handled by nsInlineReflow)
// 2. pull-up: can't pull a block onto a non-empty line
// 3. blocks require vertical margin handling
// 4. left/right margins have to be figured differently for blocks

// Assume that container is aware of block/inline differences of the
// child frame and handles them through different pathways OR that the
// nsInlineReflow does and reports back the different results

// Here is how we format B0[ I0[ I1[ text0 B1 text1] ] ]
// B0 reflows I0
//  I0 formats I1
//   I1 reflows text0
//    I1 hits B1 and notices it can't be placed
//    I1 pushes B1 to overflow list
//    I1 returns not-complete
//  I0 creates I1'
//  I0 returns not-complete
// B0 creates I0'
// B0 flushes out line
// B0 creates new line
// B0 puts I0' on new line
// B0 reflows I0'
//  I0' reflows I1'
//   I1' reflows B1
//   I1' returns not-complete
//  I0' creates I1''
//  I0' returns not-complete
// B0 creates I0''
// B0 flushes out line
// B0 creates new line
// B0 puts I0'' on new line
// B0 reflows I0''
//  I0'' reflows I1''
//   I1'' reflows text1
//   I1'' returns complete
//  I0'' returns complete
// B0 flushes out line
// B0 returns complete

// Here is how we format B0[ I0[ text0 I1[ B1 text1] ] ]

// This is harder; I1 doesn't have text0 before B1 to know that we
// have to stop reflowing I1 before reflowing B1; and worse yet, we
// only have to stop if I1 begins on the same line that contains text0
// (it might not depending on the size of text0 and the size given to
// I0 to reflow into).

// To solve this we need to know when we hit B1 whether or not a break
// is required. To answer this question we need to be able to walk
// backwards up the nsReflowState's and discover where B1 will
// actually end up. Can't use the X coordinate because of
// border/padding.

// Since B0 will define this what we do is put a counter into the
// nsLineLayout structure. As the reflow recurses down the tree we
// will bump the counter whenever a non-inline frame is seen and
// placed (one that has a non-zero area too). When I1 goes to place B1
// it will see that the count is 1 (because of text0) and know to
// return a "break-before" status (instead of a not-complete; we
// return not-complete if we have placed ANY children and break-before
// when we have placed NO children).

// XXX factor this some more so that nsInlineFrame can use it too.
// The post-reflow-success code that computes the final margin
// values and carries them forward, plus the code that factors in
// the max-element size.

// XXX inline frames need this too when they wrap up blocks, right??
// Otherwise blocks in inlines won't interact with floaters properly.
void
nsBlockFrame::PrepareInlineReflow(nsBlockReflowState& aState,
                                  nsIFrame* aFrame,
                                  PRBool aIsBlock)
{
  // Setup initial coordinate system for reflowing the frame into
  nscoord x, availWidth, availHeight;

  if (aIsBlock) {
    // XXX Child needs to apply OUR border-padding and IT's left
    // margin and right margin! How should this be done?
    x = aState.mBorderPadding.left;
    if (aState.mUnconstrainedWidth) {
      availWidth = NS_UNCONSTRAINEDSIZE;
    }
    else {
      availWidth = aState.mContentArea.width;
    }
    if (aState.mUnconstrainedHeight) {
      availHeight = NS_UNCONSTRAINEDSIZE;
    }
    else {
      /* XXX get the height right! */
      availHeight = aState.mContentArea.height - aState.mY;
    }
  }
  else {
    // If the child doesn't handle run-around then we give it the band
    // adjusted data. The band aligned data doesn't have our
    // border/padding applied to it so add that in.
    x = aState.mCurrentBand.availSpace.x + aState.mBorderPadding.left;
    availWidth = aState.mCurrentBand.availSpace.width;
    if (aState.mUnconstrainedHeight) {
      availHeight = NS_UNCONSTRAINEDSIZE;
    }
    else {
      /* XXX get the height right! */
      availHeight = aState.mCurrentBand.availSpace.height;
    }
  }
  aState.mInlineReflow->Init(x, aState.mY, availWidth, availHeight);
  aState.mInlineReflowPrepared = PR_TRUE;

  // Setup the first-letter-style-ok flag
  nsLineLayout& lineLayout = aState.mLineLayout;
  if (mFirstLetterStyle && (0 == lineLayout.GetLineNumber())) {
    lineLayout.SetFirstLetterStyleOK(PR_TRUE);
  }
  else {
    lineLayout.SetFirstLetterStyleOK(PR_FALSE);
  }
}

// XXX switch to two versions: inline vs. block?
PRUintn
nsBlockFrame::CalculateMargins(nsBlockReflowState& aState,
                               LineData* aLine,
                               PRBool aInlineContext,
                               nscoord& aTopMarginResult,
                               nscoord& aBottomMarginResult)
{
  PRUintn result = 0;
  nsInlineReflow& ir = *aState.mInlineReflow;

  // First get the childs top and bottom margins. For inline
  // situations the child frame(s) margins were applied during line
  // layout, therefore we consider them zero here.
  nscoord childsTopMargin, childsBottomMargin;
  PRUintn marginFlags;
  if (aInlineContext) {
    childsTopMargin = 0;
    childsBottomMargin = 0;
    marginFlags = ir.GetCarriedOutMarginFlags();
  }
  else {
    childsTopMargin = ir.GetTopMargin();
    childsBottomMargin = ir.GetBottomMargin();
    marginFlags = ir.GetMarginFlags();
  }

  // Get the carried margin values. Note that even in an inline
  // situation there may be a carried margin (e.g. an inline frame
  // contains a block frame and the block frame has top/bottom
  // margins). While CSS doesn't specify what this means, we do to
  // improve compatability with poorly formed HTML (and commonly used
  // HTML).
  nscoord childsCarriedOutTopMargin = ir.GetCarriedOutTopMargin();
  nscoord childsCarriedOutBottomMargin = ir.GetCarriedOutBottomMargin();
  if (childsCarriedOutTopMargin || childsCarriedOutBottomMargin) {
    result |= HAVE_CARRIED_MARGIN;
  }

  // Compute the collapsed top margin value (this is a generational
  // margin collapse not a sibling margin collapse).
  nscoord collapsedTopMargin = 
    nsInlineReflow::MaxMargin(childsCarriedOutTopMargin, childsTopMargin);

  // Now perform sibling to sibling margin collapsing.
  collapsedTopMargin = nsInlineReflow::MaxMargin(aState.mPrevBottomMargin,
                                                 collapsedTopMargin);
  PRBool topMarginIsAuto = PR_FALSE;
  if (NS_CARRIED_TOP_MARGIN_IS_AUTO & marginFlags) {
    if (aInlineContext) {
      topMarginIsAuto = collapsedTopMargin == childsCarriedOutTopMargin;
    }
    else {
      topMarginIsAuto = collapsedTopMargin == childsTopMargin;
    }
  }

  // Compute the collapsed bottom margin value (this is a generational
  // margin collapse not a sibling margin collapse).
  nscoord collapsedBottomMargin =
    nsInlineReflow::MaxMargin(childsCarriedOutBottomMargin,
                              childsBottomMargin);
  if (NS_CARRIED_BOTTOM_MARGIN_IS_AUTO & marginFlags) {
    if (aInlineContext) {
      if (collapsedBottomMargin == childsCarriedOutBottomMargin) {
        result |= NS_CARRIED_BOTTOM_MARGIN_IS_AUTO;
      }
    }
    else if (collapsedBottomMargin == childsBottomMargin) {
      result |= NS_CARRIED_BOTTOM_MARGIN_IS_AUTO;
    }
  }

  // Now that we have the collapsed margin values, address any special
  // situations that limit or change how the margins are applied.
  if (0 == aLine->mBounds.height) {
    // When a line is empty, we collapse its top and bottom margins to
    // a single bottom margin value.
    // XXX This is not obviously a part of the css2 box model.
    collapsedBottomMargin =
      nsInlineReflow::MaxMargin(collapsedTopMargin, collapsedBottomMargin);
    collapsedTopMargin = 0;
  }
  else {
    PRBool isTopLine = (aState.mY == aState.mBorderPadding.top);
    if (isTopLine) {
      if (!aState.mIsMarginRoot) {
        // We are not a root for margin collapsing and this is our first
        // non-empty-line (with a block child).
        if (topMarginIsAuto) {
          // Propogate auto-margin flag outward
          aState.mCarriedOutMarginFlags |= NS_CARRIED_TOP_MARGIN_IS_AUTO;
          result |= NS_CARRIED_TOP_MARGIN_IS_AUTO;
        }

        // Keep the collapsed margin value around to pass out to our
        // parent. We don't apply its top margin (our parent will) so
        // zero it out.
        aState.mCollapsedTopMargin = collapsedTopMargin;
        collapsedTopMargin = 0;
      }
      else if (topMarginIsAuto && (NS_BODY_NO_AUTO_MARGINS & mFlags)) {
        // Our top margin is an auto margin and we are supposed to
        // ignore them. Change it to zero.
        collapsedTopMargin = 0;
      }
    }
  }
  aTopMarginResult = collapsedTopMargin;
  aBottomMarginResult = collapsedBottomMargin;

  return result;
}

void
nsBlockFrame::SlideFrames(nsIPresContext& aPresContext,
                          nsISpaceManager* aSpaceManager,
                          LineData* aLine, nscoord aDY)
{
  // Adjust the Y coordinate of the frames in the line
  nsIFrame* kid = aLine->mFirstChild;
  PRInt32 n = aLine->ChildCount();
  while (--n >= 0) {
    nsRect r;
    kid->GetRect(r);
    r.y += aDY;
    kid->SetRect(r);

    // If the child has an floaters that impact the space manager,
    // slide them now
    nsIHTMLReflow* ihr;
    if (NS_OK == kid->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
      ihr->MoveInSpaceManager(aPresContext, aSpaceManager, 0, aDY);
    }

    kid->GetNextSibling(kid);
  }

  // Slide down our floaters too
  nsVoidArray* floaters = aLine->mFloaters;
  if (nsnull != floaters) {
    PRInt32 i;
    n = floaters->Count();
    for (i = 0; i < n; i++) {
      nsPlaceholderFrame* ph = (nsPlaceholderFrame*) floaters->ElementAt(i);
      kid = ph->GetAnchoredItem();
      nsRect r;
      kid->GetRect(r);
      r.y += aDY;
      kid->SetRect(r);
    }
  }

  // Slide line box too
  aLine->mBounds.y += aDY;
}

NS_IMETHODIMP
nsBlockFrame::MoveInSpaceManager(nsIPresContext& aPresContext,
                                 nsISpaceManager* aSpaceManager,
                                 nscoord aDeltaX, nscoord aDeltaY)
{
  LineData* line = mLines;
  while (nsnull != line) {
    PRInt32 i, n;
    nsIFrame* kid;

    // Move the floaters in the spacemanager
    nsVoidArray* floaters = line->mFloaters;
    if (nsnull != floaters) {
      n = floaters->Count();
      for (i = 0; i < n; i++) {
        nsPlaceholderFrame* ph = (nsPlaceholderFrame*) floaters->ElementAt(i);
        kid = ph->GetAnchoredItem();
        aSpaceManager->OffsetRegion(kid, aDeltaX, aDeltaY);
      }
    }

    // Tell kids about the move too
    n = line->ChildCount();
    kid = line->mFirstChild;
    while (--n >= 0) {
      nsIHTMLReflow* ihr;
      if (NS_OK == kid->QueryInterface(kIHTMLReflowIID, (void**)&ihr)) {
        ihr->MoveInSpaceManager(aPresContext, aSpaceManager, aDeltaX, aDeltaY);
      }
      kid->GetNextSibling(kid);
    }
    
    line = line->mNext;
  }

  return NS_OK;
}

nsBlockFrame*
nsBlockFrame::FindFollowingBlockFrame(nsIFrame* aFrame)
{
  nsBlockFrame* followingBlockFrame = nsnull;
  nsIFrame* frame = aFrame;
  for (;;) {
    nsIFrame* nextFrame;
    frame->GetNextSibling(nextFrame);
    if (nsnull != nextFrame) {
      const nsStyleDisplay* display;
      nsresult rv = nextFrame->GetStyleData(eStyleStruct_Display,
                                            (const nsStyleStruct*&) display);
      if (NS_SUCCEEDED(rv) && (nsnull != display)) {
        if (NS_STYLE_DISPLAY_BLOCK == display->mDisplay) {
#ifdef NOISY_RUNIN
          ListTag(stdout);
          printf(": frame: ");
          aFrame->ListTag(stdout);
          printf(" followed by: ");
          nextFrame->ListTag(stdout);
          printf("\n");
#endif
          followingBlockFrame = (nsBlockFrame*) nextFrame;
          break;
        }
        else if (NS_STYLE_DISPLAY_INLINE == display->mDisplay) {
          // If it's a text-frame and it's just whitespace and we are
          // in a normal whitespace situation THEN skip it and keep
          // going...
          // XXX probably should really check this ...
        }
      }
      else
        break;
      frame = nextFrame;
    }
    else
      break;
  }
  return followingBlockFrame;
}

PRBool
nsBlockFrame::ReflowBlockFrame(nsBlockReflowState& aState,
                               LineData* aLine,
                               nsIFrame* aFrame,
                               nsReflowStatus& aReflowResult)
{
  NS_PRECONDITION(0 == aState.mLineLayout.GetPlacedFrames(),
                  "non-empty line with a block");

  nsInlineReflow& ir = *aState.mInlineReflow;

  // Prepare the inline reflow engine
  nsBlockFrame* runInToFrame;
  nsBlockFrame* compactWithFrame;
  nscoord compactMarginWidth = 0;
  PRBool isCompactFrame = PR_FALSE;
  PRBool asBlock = PR_TRUE;
  const nsStyleDisplay* display;
  nsresult rv = aFrame->GetStyleData(eStyleStruct_Display,
                                     (const nsStyleStruct*&) display);
  if ((NS_OK == rv) && (nsnull != display)) {
    switch (display->mDisplay) {
    case NS_STYLE_DISPLAY_TABLE:
      asBlock = PR_FALSE;
      break;

    case NS_STYLE_DISPLAY_RUN_IN:
#ifdef NOISY_RUNIN
      ListTag(stdout);
      printf(": trying to see if ");
      aFrame->ListTag(stdout);
      printf(" is a run-in candidate\n");
#endif
      runInToFrame = FindFollowingBlockFrame(aFrame);
      if (nsnull != runInToFrame) {
// XXX run-in frame should be pushed to the next-in-flow too if the
// run-in-to frame is pushed.
        aReflowResult = NS_FRAME_COMPLETE;
        nsRect r(0, aState.mY, 0, 0);
        aLine->mBounds = r;
        aLine->mCombinedArea = r;
        aLine->mCarriedOutTopMargin = 0;
        aLine->mCarriedOutBottomMargin = 0;
        aLine->SetMarginFlags(0);
        aLine->ClearOutsideChildren();
        aLine->mBreakType = NS_STYLE_CLEAR_NONE;
//XXX        aFrame->WillReflow(aState.mPresContext);
        aFrame->SetRect(r);
        aState.mPrevChild = aFrame;
        aState.mRunInToFrame = runInToFrame;
        aState.mRunInFrame = (nsBlockFrame*) aFrame;
        return PR_TRUE;
      }
      break;

    case NS_STYLE_DISPLAY_COMPACT:
      compactWithFrame = FindFollowingBlockFrame(aFrame);
      if (nsnull != compactWithFrame) {
        const nsStyleSpacing* spacing;
        nsMargin margin;
        rv = compactWithFrame->GetStyleData(eStyleStruct_Spacing,
                                            (const nsStyleStruct*&) spacing);
        if (NS_SUCCEEDED(rv) && (nsnull != spacing)) {
          nsHTMLReflowState::ComputeMarginFor(compactWithFrame, &aState,
                                              margin);
          compactMarginWidth = margin.left;
        }
        isCompactFrame = PR_TRUE;
      }
      break;
    }

    // clear floaters if the clear style is not none
    if (NS_STYLE_CLEAR_NONE != display->mBreakType) {
      aState.ClearFloaters(display->mBreakType);
    }
  }
  PrepareInlineReflow(aState, aFrame, asBlock);
  ir.SetIsFirstChild((aLine == mLines) &&
                     (aFrame == aLine->mFirstChild));
  ir.SetCompactMarginWidth(compactMarginWidth);
  if (aFrame == aState.mRunInToFrame) {
    ir.SetRunInFrame(aState.mRunInFrame);
  }

  // Reflow the block frame
  nsReflowStatus reflowStatus = ir.ReflowFrame(aFrame);
  if (NS_IS_REFLOW_ERROR(reflowStatus)) {
    aReflowResult = reflowStatus;
    return PR_FALSE;
  }
  aReflowResult = reflowStatus;

  // XXX We need to check the *type* of break and if it's a column/page
  // break apply and cause the block to be split (assuming we are
  // laying out in a column).
#if XXX
  if (NS_INLINE_IS_BREAK(reflowStatus)) {
    // XXX For now we ignore it
  }
#endif

  // Align the frame
  nscoord maxAscent, maxDescent;
  ir.VerticalAlignFrames(aLine->mBounds, maxAscent, maxDescent);
  ir.HorizontalAlignFrames(aLine->mBounds);
  ir.RelativePositionFrames(aLine->mCombinedArea);

  // Calculate margins
  nscoord topMargin, bottomMargin;
  PRUintn marginFlags = CalculateMargins(aState, aLine, PR_FALSE,
                                         topMargin, bottomMargin);

  // Try to place the frame
  nscoord newY = aLine->mBounds.YMost() + topMargin;
  if ((mLines != aLine) && (newY > aState.mBottomEdge)) {
    // The frame doesn't fit inside our available space. Push the
    // line to the next-in-flow and return our incomplete status to
    // our parent.
    PushLines(aState);
    aReflowResult = NS_FRAME_NOT_COMPLETE;
    return PR_FALSE;
  }
  if (isCompactFrame) {
    // For compact frames, we don't adjust the Y coordinate at all IF
    // the compact frame ended up fitting in the margin space
    // allocated for it.
    nsRect r;
    aFrame->GetRect(r);
    if (r.width <= compactMarginWidth) {
      // XXX margins will be wrong
      // XXX ltr/rtl for horizontal placement within the margin area
      // XXX vertical alignment with the compactWith frame's *first line*
      newY = aState.mY;
    }
  }
  else if ((nsnull != mBullet) && ShouldPlaceBullet(aLine)) {
    const nsStyleFont* font;
    rv = aFrame->GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&) font);
    if (NS_SUCCEEDED(rv) && (nsnull != font)) {
      nsIRenderingContext& rc = *aState.rendContext;
      rc.SetFont(font->mFont);
      nscoord firstLineAscent = 0;
      nsIFontMetrics* fm;
      rv = rc.GetFontMetrics(fm);
      if (NS_SUCCEEDED(rv) && (nsnull != fm)) {
        fm->GetMaxAscent(firstLineAscent);
        NS_RELEASE(fm);
      }
      PlaceBullet(aState, firstLineAscent, topMargin);
    }
  }
  aState.mY = newY;

  // Apply collapsed top-margin value
  if (0 != topMargin) {
    SlideFrames(aState.mPresContext, aState.mSpaceManager, aLine, topMargin);
  }

  // Record bottom margin value for sibling to sibling compression or
  // for returning as our carried out bottom margin. Adjust running
  // margin value when either we have carried margins from the line or
  // we have a non-zero height line.
  if ((HAVE_CARRIED_MARGIN & marginFlags) || (0 != aLine->mBounds.height)) {
    aState.mPrevBottomMargin = bottomMargin;
    aState.mPrevMarginFlags = marginFlags;
  }
  aLine->mCarriedOutTopMargin = ir.GetCarriedOutTopMargin();
  aLine->mCarriedOutBottomMargin = ir.GetCarriedOutBottomMargin();
  aLine->SetMarginFlags(marginFlags);
  aLine->ClearOutsideChildren();
  nsFrameState state;
  aFrame->GetFrameState(state);
  if (NS_FRAME_OUTSIDE_CHILDREN & state) {
    aLine->SetOutsideChildren();
  }

  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }

  // Update max-element-size
  if (aState.mComputeMaxElementSize) {
    const nsSize& kidMaxElementSize = ir.GetMaxElementSize();
    if (kidMaxElementSize.width > aState.mMaxElementSize.width) {
      aState.mMaxElementSize.width = kidMaxElementSize.width;
    }
    if (kidMaxElementSize.height > aState.mMaxElementSize.height) {
      aState.mMaxElementSize.height = kidMaxElementSize.height;
    }
  }

  aState.mPrevChild = aFrame;

  // Refresh our available space in case a floater was placed by our
  // child.
  // XXX expensive!
  // XXX unnecessary because the child block will contain the floater!
  aState.GetAvailableSpace();

  aLine->mBreakType = NS_STYLE_CLEAR_NONE;

  if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus)) {
    // Some of the block fit. We need to have the block frame
    // continued, so we make sure that it has a next-in-flow now.
    nsIFrame* nextInFlow;
    nsresult rv;
    rv = nsHTMLContainerFrame::CreateNextInFlow(aState.mPresContext,
                                                this,
                                                aFrame,
                                                nextInFlow);
    if (NS_OK != rv) {
      aReflowResult = rv;
      return PR_FALSE;
    }
    if (nsnull != nextInFlow) {
      // We made a next-in-flow for the block child frame. Create a
      // line to map the block childs next-in-flow.
      LineData* line = new LineData(nextInFlow, 1, LINE_IS_BLOCK);
      if (nsnull == line) {
        aReflowResult = NS_ERROR_OUT_OF_MEMORY;
        return PR_FALSE;
      }
      line->mNext = aLine->mNext;
      aLine->mNext = line;
    }

    // Advance mPrevLine because we are keeping aLine (since some of
    // the child block frame fit). Then push any remaining lines to
    // our next-in-flow
    aState.mPrevLine = aLine;
    if (nsnull != aLine->mNext) {
      PushLines(aState);
    }
    aReflowResult = NS_INLINE_LINE_BREAK_AFTER(reflowStatus);
    return PR_FALSE;
  }
  return PR_TRUE;
}

/**
 * Reflow an inline frame. Returns PR_FALSE if the frame won't fit
 * on the line and the line should be flushed.
 */
PRBool
nsBlockFrame::ReflowInlineFrame(nsBlockReflowState& aState,
                                LineData* aLine,
                                nsIFrame* aFrame,
                                nsReflowStatus& aReflowResult,
                                PRBool& aAddedToLine)
{
  nsresult rv;
  nsIFrame* nextInFlow;

  aAddedToLine = PR_FALSE;
  if (!aState.mInlineReflowPrepared) {
    PrepareInlineReflow(aState, aFrame, PR_FALSE);
  }

  // When reflowing a frame that is on the first-line, check and see
  // if a special style context should be placed in the context chain.
  PRBool reflowingFirstLetter = PR_FALSE;
  if ((nsnull == mPrevInFlow) &&
      (0 == aState.mLineLayout.GetLineNumber())) {
    if (nsnull != mFirstLineStyle) {
      // Update the child frames style to inherit from the first-line
      // style.
      // XXX add code to check first and only do it if it needs doing!
#ifdef REALLY_NOISY_FIRST_LINE
      DumpStyleGeneaology(aFrame, "");
#endif
#ifdef NOISY_FIRST_LINE
      aFrame->ListTag(stdout);
      printf(": adding in first-line style\n");
#endif
      aFrame->ReResolveStyleContext(&aState.mPresContext, mFirstLineStyle);
#ifdef REALLY_NOISY_FIRST_LINE
      DumpStyleGeneaology(aFrame, "  ");
#endif
    }
    if ((nsnull != mFirstLetterStyle) &&
        aState.mLineLayout.GetFirstLetterStyleOK()) {
      // XXX For now, we only allow text objects to have :first-letter
      // style.
      nsIContent* c;
      nsresult rv = aFrame->GetContent(c);
      if (NS_SUCCEEDED(rv) && (nsnull != c)) {
        nsITextContent* tc;
        nsresult rv = c->QueryInterface(kITextContentIID, (void**)&tc);
        if (NS_SUCCEEDED(rv)) {
          // XXX add code to check first and only do it if it needs doing!
          // Update the child frames style to inherit from the
          // first-letter style.
          aFrame->ReResolveStyleContext(&aState.mPresContext,
                                        mFirstLetterStyle);
          reflowingFirstLetter = PR_TRUE;
          NS_RELEASE(tc);
        }
        NS_RELEASE(c);
      }
    }
  }

  PRBool isFirstChild = (aLine == mLines) &&
    (aFrame == aLine->mFirstChild);
  aState.mInlineReflow->SetIsFirstChild(isFirstChild);
  aReflowResult = aState.mInlineReflow->ReflowFrame(aFrame);
  if (NS_IS_REFLOW_ERROR(aReflowResult)) {
    return PR_FALSE;
  }

  if (!NS_INLINE_IS_BREAK(aReflowResult)) {
    aState.mPrevChild = aFrame;
    if (NS_FRAME_IS_COMPLETE(aReflowResult)) {
      return PR_TRUE;
    }

    // Create continuation frame (if necessary); add it to the end of
    // the current line so that it can be pushed to the next line
    // properly.
    rv = nsHTMLContainerFrame::CreateNextInFlow(aState.mPresContext,
                                                this, aFrame, nextInFlow);
    if (NS_OK != rv) {
      aReflowResult = rv;
      return PR_FALSE;
    }
    if (nsnull != nextInFlow) {
      // Add new child to the line
      aLine->mChildCount++;
    }

    // XXX Special hackery to keep going if we just split because of a
    // :first-letter situation.
    if (reflowingFirstLetter) {
      if (nsnull != nextInFlow) {
        aAddedToLine = PR_TRUE;
        nextInFlow->ReResolveStyleContext(&aState.mPresContext,
                                          mFirstLineStyle
                                          ? mFirstLineStyle
                                          : mStyleContext);
      }
      if (aState.mInlineReflow->GetAvailWidth() > 0) {
        return PR_TRUE;
      }
    }

    // Split line after the frame we just reflowed
    aFrame->GetNextSibling(aFrame);
  }
  else {
    if (NS_INLINE_IS_BREAK_AFTER(aReflowResult)) {
      aState.mPrevChild = aFrame;
      if (NS_FRAME_IS_NOT_COMPLETE(aReflowResult)) {
        // Create continuation frame (if necessary); add it to the end of
        // the current line so that it can be pushed to the next line
        // properly.
        rv = nsHTMLContainerFrame::CreateNextInFlow(aState.mPresContext,
                                                    this, aFrame,
                                                    nextInFlow);
        if (NS_OK != rv) {
          aReflowResult = rv;
          return PR_FALSE;
        }
        if (nsnull != nextInFlow) {
          // Add new child to the line
          aLine->mChildCount++;
        }
      }
      aFrame->GetNextSibling(aFrame);
    }
  }

  // Split line since we aren't going to keep going
  rv = SplitLine(aState, aLine, aFrame);
  if (NS_IS_REFLOW_ERROR(rv)) {
    aReflowResult = rv;
  }
  return PR_FALSE;
}

void
nsBlockFrame::RestoreStyleFor(nsIPresContext& aPresContext, nsIFrame* aFrame)
{
#ifdef REALLY_NOISY_FIRST_LINE
  DumpStyleGeneaology(aFrame, "");
#endif

  nsIStyleContext* kidSC;
  aFrame->GetStyleContext(kidSC);
  if (nsnull != kidSC) {
    nsIStyleContext* kidParentSC;
    kidParentSC = kidSC->GetParent();
    if ((kidParentSC == mFirstLineStyle) ||
        (kidParentSC == mFirstLetterStyle)) {
#ifdef NOISY_FIRST_LINE
      printf("  ");
      aFrame->ListTag(stdout);
      printf(": removing first-line style\n");
#endif

      aFrame->ReResolveStyleContext(&aPresContext, mStyleContext);

#ifdef REALLY_NOISY_FIRST_LINE
      DumpStyleGeneaology(aFrame, "  ");
#endif
    }
    NS_IF_RELEASE(kidParentSC);
    NS_RELEASE(kidSC);
  }
}

// XXX alloc lines using free-list in aState

// XXX refactor this since the split NEVER has to deal with blocks

nsresult
nsBlockFrame::SplitLine(nsBlockReflowState& aState,
                        LineData* aLine,
                        nsIFrame* aFrame)
{
  PRInt32 pushCount = aLine->ChildCount() -
    aState.mInlineReflow->GetCurrentFrameNum();
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                 ("nsBlockFrame::SplitLine: pushing %d frames",
                  pushCount));
  if (0 != pushCount) {
    NS_ASSERTION(nsnull != aFrame, "whoops");
    LineData* to = aLine->mNext;
    if (nsnull != to) {
      // Only push into the next line if it's empty; otherwise we can
      // end up pushing a frame which is continued into the same frame
      // as it's continuation. This causes all sorts of bad side
      // effects so we don't allow it.
      if (0 != to->ChildCount()) {
        LineData* insertedLine = new LineData(aFrame, pushCount, 0);
        aLine->mNext = insertedLine;
        insertedLine->mNext = to;
        to = insertedLine;
      } else {
        to->mFirstChild = aFrame;
        to->mChildCount += pushCount;
      }
    } else {
      to = new LineData(aFrame, pushCount, 0);
      aLine->mNext = to;
    }
    if (nsnull == to) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aLine->mChildCount -= pushCount;
#ifdef NS_DEBUG
    if (GetVerifyTreeEnable()) {
      aLine->Verify();
    }
#endif
    NS_ASSERTION(0 != aLine->ChildCount(), "bad push");

    // Let inline reflow know that some frames are no longer part of
    // its state.
    aState.mInlineReflow->ChangeFrameCount(aLine->ChildCount());

    // Repair style contexts when frames move to the next line and
    // this is the first line.
    if (((nsnull != mFirstLineStyle) || (nsnull != mFirstLetterStyle)) &&
        (0 == aState.mLineLayout.GetLineNumber())) {
      // Take the pseudo-style out of the style context chain
      nsIFrame* kid = to->mFirstChild;
      PRInt32 n = pushCount;
      while (--n >= 0) {
        RestoreStyleFor(aState.mPresContext, kid);
        kid->GetNextSibling(kid);
      }
    }
  }
  return NS_OK;
}

PRBool
nsBlockFrame::PullFrame(nsBlockReflowState& aState,
                        LineData* aLine,
                        LineData** aFromList,
                        PRBool aUpdateGeometricParent,
                        nsReflowStatus& aReflowResult,
                        PRBool& aAddedToLine)
{
  LineData* fromLine = *aFromList;
  NS_ASSERTION(nsnull != fromLine, "bad line to pull from");
  if (0 == fromLine->ChildCount()) {
    // Discard empty lines immediately. Empty lines can happen here
    // because of DeleteChildsNextInFlow not being able to delete
    // lines.
    *aFromList = fromLine->mNext;
    NS_ASSERTION(nsnull == fromLine->mFirstChild, "bad empty line");
    fromLine->mNext = aState.mFreeList;
    aState.mFreeList = fromLine;
    return PR_TRUE;
  }

  // If our line is not empty and the child in aFromLine is a block
  // then we cannot pull up the frame into this line.
  if ((0 != aLine->ChildCount()) && fromLine->IsBlock()) {
    aReflowResult = NS_INLINE_LINE_BREAK_BEFORE();
    return PR_FALSE;
  }

  // Take frame from fromLine
  nsIFrame* frame = fromLine->mFirstChild;
  if (0 == aLine->mChildCount++) {
    aLine->mFirstChild = frame;
    aLine->SetIsBlock(fromLine->IsBlock());
#ifdef NS_DEBUG
    const nsStyleDisplay* display;
    frame->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&) display);
    const nsStylePosition* position;
    frame->GetStyleData(eStyleStruct_Position,
                        (const nsStyleStruct*&) position);
    PRBool isBlock = nsLineLayout::TreatFrameAsBlock(display, position);
    NS_ASSERTION(isBlock == aLine->IsBlock(), "bad line isBlock");
#endif
  }
  if (0 != --fromLine->mChildCount) {
    frame->GetNextSibling(fromLine->mFirstChild);
  }
  else {
    // Free up the fromLine now that it's empty
    *aFromList = fromLine->mNext;
    fromLine->mFirstChild = nsnull;
    fromLine->mNext = aState.mFreeList;
    aState.mFreeList = fromLine;
  }

  // Change geometric parents
  if (aUpdateGeometricParent) {
    nsIFrame* geometricParent;
    nsIFrame* contentParent;
    frame->GetGeometricParent(geometricParent);
    frame->GetContentParent(contentParent);
    if (contentParent == geometricParent) {
      frame->SetContentParent(this);
    }
    frame->SetGeometricParent(this);

    // The frame is being pulled from a next-in-flow; therefore we
    // need to add it to our sibling list.
    if (nsnull != aState.mPrevChild) {
      aState.mPrevChild->SetNextSibling(frame);
    }
    frame->SetNextSibling(nsnull);
  }

  // Reflow the frame
  if (aLine->IsBlock()) {
    aAddedToLine = PR_FALSE;
    return ReflowBlockFrame(aState, aLine, frame, aReflowResult);
  }
  else {
    return ReflowInlineFrame(aState, aLine, frame, aReflowResult,
                             aAddedToLine);
  }
}

PRBool
nsBlockFrame::IsLastLine(nsBlockReflowState& aState,
                         LineData* aLine,
                         nsReflowStatus aReflowStatus)
{
  // If the frame is not-complete then there must be another line
  // following...
  if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
    // If any subsequent line has a frame on it then this is not the
    // last line.
    LineData* next = aLine->mNext;
    while (nsnull != next) {
      if (0 != next->ChildCount()) {
        return PR_FALSE;
      }
      next = next->mNext;
    }

    // If there are next-in-flows and they have any non-empty lines
    // then this is not the last line (because the pullup code will
    // pullup frames from the next-in-flows after this line is
    // placed). Even if the pullup code doesn't pullup, we don't want
    // to signal the last line except in our last-in-flow.
    nsBlockFrame* nextInFlow = (nsBlockFrame*) mNextInFlow;
    while (nsnull != mNextInFlow) {
      LineData* line = nextInFlow->mLines;
      while (nsnull != line) {
        if (0 != next->ChildCount()) {
          return PR_FALSE;
        }
        line = line->mNext;
      }
      nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
    }

    // This is the last line
    return PR_TRUE;
  }
  return PR_FALSE;
}

// XXX This is identical to the back end of the block reflow code, not
// counting the continuation of block frames part. Factor this!

PRBool
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        LineData* aLine,
                        nsReflowStatus aReflowStatus)
{
  PRBool isLastLine = PR_FALSE;
  if (NS_STYLE_TEXT_ALIGN_JUSTIFY == aState.mStyleText->mTextAlign) {
    // For justification we need to know if this line is the last
    // line. If it is, then justification is disabled.
    isLastLine = IsLastLine(aState, aLine, aReflowStatus);
  }

  // Align the children. This also determines the actual height and
  // width of the line.
  nsInlineReflow& ir = *aState.mInlineReflow;
  nscoord maxAscent, maxDescent;
  ir.VerticalAlignFrames(aLine->mBounds, maxAscent, maxDescent);
  ir.HorizontalAlignFrames(aLine->mBounds, isLastLine);
  ir.RelativePositionFrames(aLine->mCombinedArea);

  // Calculate the bottom margin for the line.
  nscoord lineBottomMargin = 0;
  if (0 == aLine->mBounds.height) {
    nsIFrame* brFrame = aState.mLineLayout.GetBRFrame();
    if (nsnull != brFrame) {
      // If a line ends in a BR, and the line is empty of height, then
      // we make sure that the line ends up with some height
      // anyway. Note that the height looks like vertical margin so
      // that it can compress with other block margins.
      nsIStyleContext* brSC;
      nsIPresContext& px = aState.mPresContext;
      nsresult rv = brFrame->GetStyleContext(brSC);
      if ((NS_OK == rv) && (nsnull != brSC)) {
        const nsStyleFont* font = (const nsStyleFont*)
          brSC->GetStyleData(eStyleStruct_Font);
        nsIFontMetrics* fm = px.GetMetricsFor(font->mFont);
        if (nsnull != fm) {
          fm->GetHeight(lineBottomMargin);
          NS_RELEASE(fm);
        }
        NS_RELEASE(brSC);
      }
    }
  }
  else {
    aState.mRunInToFrame = nsnull;
    aState.mRunInFrame = nsnull;
  }

  // Calculate the lines top and bottom margin values. The margin will
  // come from an embedded block frame, not from inline frames.
  nscoord topMargin, bottomMargin;
  PRUintn marginFlags = CalculateMargins(aState, aLine, PR_TRUE,
                                         topMargin, bottomMargin);

  // See if the line fit. If it doesn't we need to push it. Our first
  // line will always fit.

  // XXX This is a good place to check and see if we have
  // below-current-line floaters, and if we do make sure that they fit
  // too.

  // XXX don't forget to factor in the top/bottom margin when sharing
  // this with the block code
  nscoord newY = aLine->mBounds.YMost() + topMargin + lineBottomMargin;
  NS_FRAME_TRACE(NS_FRAME_TRACE_CHILD_REFLOW,
     ("nsBlockFrame::PlaceLine: newY=%d limit=%d lineHeight=%d",
      newY, aState.mBottomEdge, aLine->mBounds.height));
  if ((mLines != aLine) && (newY > aState.mBottomEdge)) {
    // Push this line and all of it's children and anything else that
    // follows to our next-in-flow
    PushLines(aState);
    return PR_FALSE;
  }

  // Compute LINE_OUTSIDE_CHILDREN state for this line. The bit is set
  // if any child frame has outside children.
  aLine->ClearOutsideChildren();
  nsIFrame* kid = aLine->mFirstChild;
  PRInt32 n = aLine->ChildCount();
  while (--n >= 0) {
    nsFrameState state;
    kid->GetFrameState(state);
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
      aLine->SetOutsideChildren();
      break;
    }
    kid->GetNextSibling(kid);
  }

  // Apply collapsed top-margin value
  // XXX I bet the bullet placement just got broken by this code
  if (0 != topMargin) {
    SlideFrames(aState.mPresContext, aState.mSpaceManager, aLine, topMargin);
  }

  // Adjust running margin value when either we have carried margins
  // from the line or we have a non-zero height line.
  if ((HAVE_CARRIED_MARGIN & marginFlags) || (0 != aLine->mBounds.height)) {
    aState.mPrevBottomMargin = bottomMargin;
    aState.mPrevMarginFlags = marginFlags;
  }
  aLine->mCarriedOutTopMargin = ir.GetCarriedOutTopMargin();
  aLine->mCarriedOutBottomMargin = ir.GetCarriedOutBottomMargin();
  aLine->SetMarginFlags(marginFlags);

  // Now that we know the line is staying put, put in the outside
  // bullet if we have one.
  if ((nsnull == mPrevInFlow) && (nsnull != mBullet) &&
      ShouldPlaceBullet(aLine)) {
    PlaceBullet(aState, maxAscent, topMargin);
  }

  // Update max-element-size
  if (aState.mComputeMaxElementSize) {
    const nsSize& kidMaxElementSize = ir.GetMaxElementSize();
    if (kidMaxElementSize.width > aState.mMaxElementSize.width) {
      aState.mMaxElementSize.width = kidMaxElementSize.width;
    }
    if (kidMaxElementSize.height > aState.mMaxElementSize.height) {
      aState.mMaxElementSize.height = kidMaxElementSize.height;
    }
  }

  nscoord xmost = aLine->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }
  aState.mY = newY;

  // Any below current line floaters to place?
  if (0 != aState.mPendingFloaters.Count()) {
    aState.PlaceFloaters(&aState.mPendingFloaters, PR_FALSE);
    aState.mPendingFloaters.Clear();

    // XXX Factor in the height of the floaters as well when considering
    // whether the line fits.
    // The default policy is that if there isn't room for the floaters then
    // both the line and the floaters are pushed to the next-in-flow...
  }

  // Based on the last child we reflowed reflow status, we may need to
  // clear past any floaters.
  aLine->mBreakType = NS_STYLE_CLEAR_NONE;
  if (NS_INLINE_IS_BREAK_AFTER(aReflowStatus)) {
    PRUint8 breakType = NS_INLINE_GET_BREAK_TYPE(aReflowStatus);
    aLine->mBreakType = breakType;
    // Apply break to the line
    switch (breakType) {
    default:
      break;
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                     ("nsBlockFrame::PlaceLine: clearing floaters=%d",
                      breakType));
#if XXX
      {
        nscoord y0 = aState.mY;
        aState.ClearFloaters(breakType);
        nscoord dy = aState.mY - y0;
        if (dy > aLine->mCarriedOutBottomMargin) {
          aLine->mCarriedOutBottomMargin = dy;
        }
      }
#else
      aState.ClearFloaters(breakType);
#if 0
      y0 = aState.mY;
      aState.ClearFloaters(lastCleanLine->mBreakType);
      dy = aState.mY - y0;
      if (dy > aState.mPrevBottomMargin) {
        aState.mPrevBottomMargin = dy;
        aState.mPrevMarginFlags |= ALREADY_APPLIED_BOTTOM_MARGIN;
      }
#endif
#endif
      break;
    }
    // XXX page breaks, etc, need to be passed upwards too!
  }

  // Update available space after placing line in case below current
  // line floaters were placed or in case we just used up the space in
  // the current band and are ready to move into a new band.
  aState.GetAvailableSpace();

  return PR_TRUE;
}

PRBool
nsBlockFrame::ShouldPlaceBullet(LineData* aLine)
{
  PRBool ok = PR_FALSE;
  const nsStyleList* list;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&)list);
  if (NS_STYLE_LIST_STYLE_POSITION_OUTSIDE == list->mListStylePosition) {
    LineData* line = mLines;
    while (nsnull != line) {
      if (line->mBounds.height > 0) {
        if (aLine == line) {
          ok = PR_TRUE;
          break;
        }
      }
      if (aLine == line) {
        break;
      }
      line = line->mNext;
    }
  }
  return ok;
}

void
nsBlockFrame::PlaceBullet(nsBlockReflowState& aState,
                          nscoord aMaxAscent,
                          nscoord aTopMargin)
{
  // Reflow the bullet now
  nsSize availSize;
  availSize.width = NS_UNCONSTRAINEDSIZE;
  availSize.height = NS_UNCONSTRAINEDSIZE;
  nsHTMLReflowState reflowState(aState.mPresContext, mBullet, aState,
                                availSize, &aState.mLineLayout);
  nsHTMLReflowMetrics metrics(nsnull);
  nsIHTMLReflow* htmlReflow;
  nsresult rv = mBullet->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_SUCCEEDED(rv)) {
    nsReflowStatus  status;
    htmlReflow->WillReflow(aState.mPresContext);
    htmlReflow->Reflow(aState.mPresContext, metrics, reflowState, status);
    htmlReflow->DidReflow(aState.mPresContext, NS_FRAME_REFLOW_FINISHED);
  }

  // Place the bullet now; use its right margin to distance it
  // from the rest of the frames in the line
  nsMargin margin;
  nsHTMLReflowState::ComputeMarginFor(mBullet, &aState, margin);
  nscoord x = aState.mBorderPadding.left - margin.right - metrics.width;
  // XXX This calculation may be wrong, especially if
  // vertical-alignment occurs on the line!
  nscoord y = aState.mBorderPadding.top + aMaxAscent -
    metrics.ascent + aTopMargin;
  mBullet->SetRect(nsRect(x, y, metrics.width, metrics.height));
}

static nsresult
FindFloatersIn(nsIFrame* aFrame, nsVoidArray*& aArray)
{
  const nsStyleDisplay* display;
  aFrame->GetStyleData(eStyleStruct_Display,
                       (const nsStyleStruct*&) display);
  if (NS_STYLE_FLOAT_NONE != display->mFloats) {
    if (nsnull == aArray) {
      aArray = new nsVoidArray();
      if (nsnull == aArray) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    aArray->AppendElement(aFrame);
  }

  if (NS_STYLE_DISPLAY_INLINE == display->mDisplay) {
    nsIFrame* kid;
    aFrame->FirstChild(nsnull, kid);
    while (nsnull != kid) {
      nsresult rv = FindFloatersIn(kid, aArray);
      if (NS_OK != rv) {
        return rv;
      }
      kid->GetNextSibling(kid);
    }
  }
  return NS_OK;
}

void
nsBlockFrame::FindFloaters(LineData* aLine)
{
  nsVoidArray* floaters = aLine->mFloaters;
  if (nsnull != floaters) {
    // Empty floater array before proceeding
    floaters->Clear();
  }

  nsIFrame* frame = aLine->mFirstChild;
  PRInt32 n = aLine->ChildCount();
  while (--n >= 0) {
    FindFloatersIn(frame, floaters);
    frame->GetNextSibling(frame);
  }

  aLine->mFloaters = floaters;

  // Get rid of floater array if we don't need it
  if (nsnull != floaters) {
    if (0 == floaters->Count()) {
      delete floaters;
      aLine->mFloaters = nsnull;
    }
  }
}

void
nsBlockFrame::PushLines(nsBlockReflowState& aState)
{
  NS_ASSERTION(nsnull != aState.mPrevLine, "bad push");

  LineData* lastLine = aState.mPrevLine;
  LineData* nextLine = lastLine->mNext;

  lastLine->mNext = nsnull;
  mOverflowLines = nextLine;

  // Break frame sibling list
  nsIFrame* lastFrame = lastLine->LastChild();
  lastFrame->SetNextSibling(nsnull);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("nsBlockFrame::PushLines: line=%p prevInFlow=%p nextInFlow=%p",
      mOverflowLines, mPrevInFlow, mNextInFlow));
#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines);
    VerifyChildCount(mOverflowLines, PR_TRUE);
  }
#endif
}

PRBool
nsBlockFrame::DrainOverflowLines()
{
  PRBool drained = PR_FALSE;

  // First grab the prev-in-flows overflow lines
  nsBlockFrame* prevBlock = (nsBlockFrame*) mPrevInFlow;
  if (nsnull != prevBlock) {
    LineData* line = prevBlock->mOverflowLines;
    if (nsnull != line) {
      drained = PR_TRUE;
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsBlockFrame::DrainOverflowLines: line=%p prevInFlow=%p",
          line, prevBlock));
      prevBlock->mOverflowLines = nsnull;

      // Make all the frames on the mOverflowLines list mine
      nsIFrame* lastFrame = nsnull;
      nsIFrame* frame = line->mFirstChild;
      while (nsnull != frame) {
        nsIFrame* geometricParent;
        nsIFrame* contentParent;
        frame->GetGeometricParent(geometricParent);
        frame->GetContentParent(contentParent);
        if (contentParent == geometricParent) {
          frame->SetContentParent(this);
        }
        frame->SetGeometricParent(this);
        lastFrame = frame;
        frame->GetNextSibling(frame);
      }

      // Join the line lists
      if (nsnull == mLines) {
        mLines = line;
      }
      else {
        // Join the sibling lists together
        lastFrame->SetNextSibling(mLines->mFirstChild);

        // Place overflow lines at the front of our line list
        LineData* lastLine = LastLine(line);
        lastLine->mNext = mLines;
        mLines = line;
      }
    }
  }

  // Now grab our own overflow lines
  if (nsnull != mOverflowLines) {
    // This can happen when we reflow and not everything fits and then
    // we are told to reflow again before a next-in-flow is created
    // and reflows.
    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
       ("nsBlockFrame::DrainOverflowLines: from me, line=%p",
        mOverflowLines));
    LineData* lastLine = LastLine(mLines);
    if (nsnull == lastLine) {
      mLines = mOverflowLines;
    }
    else {
      lastLine->mNext = mOverflowLines;
      nsIFrame* lastFrame = lastLine->LastChild();
      lastFrame->SetNextSibling(mOverflowLines->mFirstChild);

      // Update our last-content-index now that we have a new last child
      lastLine = LastLine(mOverflowLines);
    }
    mOverflowLines = nsnull;
    drained = PR_TRUE;
  }

#ifdef NS_DEBUG
  if (GetVerifyTreeEnable()) {
    VerifyChildCount(mLines, PR_TRUE);
  }
#endif
  return drained;
}

nsresult
nsBlockFrame::InsertNewFrame(nsIPresContext& aPresContext,
                             nsBlockFrame*   aParentFrame,
                             nsIFrame*       aNewFrame,
                             nsIFrame*       aPrevSibling)
{
  if (nsnull == mLines) {
    NS_ASSERTION(nsnull == aPrevSibling, "prev-sibling and empty line list!");
    return AppendNewFrames(aPresContext, aNewFrame);
  }

  const nsStyleDisplay* display;
  aNewFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  const nsStylePosition* position;
  aNewFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&) position);
  PRUint16 newFrameIsBlock = nsLineLayout::TreatFrameAsBlock(display, position)
                               ? LINE_IS_BLOCK : 0;

  // See if we need to move the frame outside of the flow, and insert a
  // placeholder frame in its place
  nsIFrame* placeholder;
  if (MoveFrameOutOfFlow(aPresContext, aNewFrame, display, position, placeholder)) {
    // Add the placeholder frame to the flow
    aNewFrame = placeholder;
    newFrameIsBlock = PR_FALSE;  // placeholder frame is always inline
  }
  else {
    // Wrap the frame in a view if necessary
    nsIStyleContext* kidSC;
    aNewFrame->GetStyleContext(kidSC);
    nsresult rv = CreateViewForFrame(aPresContext, aNewFrame, kidSC, PR_FALSE);    
    NS_RELEASE(kidSC);
    if (NS_OK != rv) {
      return rv;
    }
  }

  // Insert/append the frame into flows line list at the right spot
  LineData* newLine;
  LineData* line = aParentFrame->mLines;
  if (nsnull == aPrevSibling) {
    // Insert new frame into the sibling list
    aNewFrame->SetNextSibling(line->mFirstChild);

    if (line->IsBlock() || newFrameIsBlock) {
      // Create a new line
      newLine = new LineData(aNewFrame, 1, newFrameIsBlock);
      if (nsnull == newLine) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      newLine->mNext = aParentFrame->mLines;
      aParentFrame->mLines = newLine;
    } else {
      // Insert frame at the front of the line
      line->mFirstChild = aNewFrame;
      line->mChildCount++;
      line->MarkDirty();
    }
  }
  else {
    // Find line containing the previous sibling to the new frame
    line = FindLineContaining(line, aPrevSibling);
    NS_ASSERTION(nsnull != line, "no line contains the previous sibling");
    if (nsnull != line) {
      if (line->IsBlock()) {
        // Create a new line just after line
        newLine = new LineData(aNewFrame, 1, newFrameIsBlock);
        if (nsnull == newLine) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        newLine->mNext = line->mNext;
        line->mNext = newLine;
      }
      else if (newFrameIsBlock) {
        // Split line in two, if necessary. We can't allow a block to
        // end up in an inline line.
        if (line->IsLastChild(aPrevSibling)) {
          // The new frame goes after prevSibling and prevSibling is
          // the last frame on the line. Therefore we don't need to
          // split the line, just create a new line.
          newLine = new LineData(aNewFrame, 1, newFrameIsBlock);
          if (nsnull == newLine) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          newLine->mNext = line->mNext;
          line->mNext = newLine;
        }
        else {
          // The new frame goes after prevSibling and prevSibling is
          // somewhere in the line, but not at the end. Split the line
          // just after prevSibling.
          PRInt32 i, n = line->ChildCount();
          nsIFrame* frame = line->mFirstChild;
          for (i = 0; i < n; i++) {
            if (frame == aPrevSibling) {
              nsIFrame* nextSibling;
              aPrevSibling->GetNextSibling(nextSibling);

              // Create new line to hold the remaining frames
              NS_ASSERTION(n - i - 1 > 0, "bad line count");
              newLine = new LineData(nextSibling, n - i - 1, 0);
              if (nsnull == newLine) {
                return NS_ERROR_OUT_OF_MEMORY;
              }
              newLine->mNext = line->mNext;
              line->mNext = newLine;
              line->MarkDirty();
              line->mChildCount = i + 1;
              break;
            }
            frame->GetNextSibling(frame);
          }

          // Now create a new line to hold the block
          newLine = new LineData(aNewFrame, 1, newFrameIsBlock);
          if (nsnull == newLine) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          newLine->mNext = line->mNext;
          line->mNext = newLine;
        }
      }
      else {
        // Insert frame into the line.
        //XXX NS_ASSERTION(line->GetLastContentIsComplete(), "bad line LCIC");
        line->mChildCount++;
        line->MarkDirty();
      }
    }

    // Insert new frame into the sibling list; note: this must be done
    // after the above logic because the above logic depends on the
    // sibling list being in the "before insertion" state.
    nsIFrame* nextSibling;
    aPrevSibling->GetNextSibling(nextSibling);
    aNewFrame->SetNextSibling(nextSibling);
    aPrevSibling->SetNextSibling(aNewFrame);
  }

  return NS_OK;
}

PRBool
nsBlockFrame::DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                     nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");

  nsIFrame* nextInFlow;
  nsBlockFrame* parent;
   
  aChild->GetNextInFlow(nextInFlow);
  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nextInFlow->GetGeometricParent((nsIFrame*&)parent);

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  nsIFrame* nextNextInFlow;
  nextInFlow->GetNextInFlow(nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    parent->DeleteChildsNextInFlow(aPresContext, nextInFlow);
  }

#ifdef NS_DEBUG
  PRInt32   childCount;
  nsIFrame* firstChild;
  nextInFlow->FirstChild(nsnull, firstChild);
  childCount = LengthOf(firstChild);
  NS_ASSERTION((0 == childCount) && (nsnull == firstChild),
               "deleting !empty next-in-flow");
#endif

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Remove nextInFlow from the parents line list. Also remove it from
  // the sibling list.
  if (RemoveChild(parent->mLines, nextInFlow)) {
    NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
       ("nsBlockFrame::DeleteNextInFlowsFor: frame=%p (from mLines)",
        nextInFlow));
    goto done;
  }

  // If we get here then we didn't find the child on the line list. If
  // it's not there then it has to be on the overflow lines list.
  if (nsnull != mOverflowLines) {
    if (RemoveChild(parent->mOverflowLines, nextInFlow)) {
      NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
         ("nsBlockFrame::DeleteNextInFlowsFor: frame=%p (from overflow)",
          nextInFlow));
      goto done;
    }
  }
  NS_NOTREACHED("can't find next-in-flow in overflow list");

 done:;
  // If the parent is us then we will finish reflowing and update the
  // content offsets of our parents when we are a pseudo-frame; if the
  // parent is not us then it's a next-in-flow which means it will get
  // reflowed by our parent and fix its content offsets. So there.

  // Delete the next-in-flow frame and adjust its parents child count
  nextInFlow->DeleteFrame(aPresContext);

#ifdef NS_DEBUG
  aChild->GetNextInFlow(nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif
  return PR_TRUE;
}

PRBool
nsBlockFrame::RemoveChild(LineData* aLines, nsIFrame* aChild)
{
  LineData* line = aLines;
  nsIFrame* prevChild = nsnull;
  while (nsnull != line) {
    nsIFrame* child = line->mFirstChild;
    PRInt32 n = line->ChildCount();
    while (--n >= 0) {
      nsIFrame* nextChild;
      child->GetNextSibling(nextChild);
      if (child == aChild) {
        NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
           ("nsBlockFrame::RemoveChild: line=%p frame=%p",
            line, aChild));
        // Continuations HAVE to be at the start of a line
        NS_ASSERTION(child == line->mFirstChild, "bad continuation");
        line->mFirstChild = nextChild;
        if (0 == --line->mChildCount) {
          line->mFirstChild = nsnull;
        }
        if (nsnull != prevChild) {
          // When nextInFlow and it's continuation are in the same
          // container then we remove the nextInFlow from the sibling
          // list.
          prevChild->SetNextSibling(nextChild);
        }
        return PR_TRUE;
      }
      prevChild = child;
      child = nextChild;
    }
    line = line->mNext;
  }
  return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////
// Floater support

void
nsBlockFrame::ReflowFloater(nsIPresContext& aPresContext,
                            nsBlockReflowState& aState,
                            nsIFrame* aFloaterFrame,
                            nsHTMLReflowState& aFloaterReflowState)
{
  // If either dimension is constrained then get the border and
  // padding values in advance.
  nsMargin bp(0, 0, 0, 0);
  if (aFloaterReflowState.HaveFixedContentWidth() ||
      aFloaterReflowState.HaveFixedContentHeight()) {
    nsHTMLReflowState::ComputeBorderPaddingFor(aFloaterFrame, &aState, bp);
  }

  // Compute the available width for the floater
  nsSize& kidAvailSize = aFloaterReflowState.maxSize;
  if (aFloaterReflowState.HaveFixedContentWidth()) {
    // When the floater has a contrained width, give it just enough
    // space for its styled width plus its borders and paddings.
    kidAvailSize.width = aFloaterReflowState.minWidth + bp.left + bp.right;
  }
  else {
    // If we are floating something and we don't know the width then
    // find a maximum width for it to reflow into. Walk upwards until
    // we find something with an unconstrained width.
    const nsHTMLReflowState* rsp = &aState;
    kidAvailSize.width = 0;
    while (nsnull != rsp) {
      if (eHTMLFrameConstraint_FixedContent == rsp->widthConstraint) {
        kidAvailSize.width = rsp->minWidth;
        break;
      }
      else if (NS_UNCONSTRAINEDSIZE != rsp->widthConstraint) {
        kidAvailSize.width = rsp->maxSize.width;
        if (kidAvailSize.width > 0) {
          break;
        }
      }
      // XXX This cast is unfortunate!
      rsp = (const nsHTMLReflowState*) rsp->parentReflowState;
    }
  }

  // Compute the available height for the floater
  if (aFloaterReflowState.HaveFixedContentHeight()) {
    kidAvailSize.height = aFloaterReflowState.minHeight + bp.top + bp.bottom;
  }
  else {
    kidAvailSize.height = NS_UNCONSTRAINEDSIZE;
  }

  // Resize reflow the anchored item into the available space
  nsIHTMLReflow*  floaterReflow;
  if (NS_OK == aFloaterFrame->QueryInterface(kIHTMLReflowIID,
                                             (void**)&floaterReflow)) {
    nsHTMLReflowMetrics desiredSize(nsnull);
    nsReflowStatus  status;
    floaterReflow->WillReflow(aPresContext);
    floaterReflow->Reflow(aPresContext, desiredSize, aFloaterReflowState,
                          status);
    aFloaterFrame->SizeTo(desiredSize.width, desiredSize.height);
  }
}

void
nsBlockReflowState::InitFloater(nsPlaceholderFrame* aPlaceholder)
{
  nsIFrame* floater = aPlaceholder->GetAnchoredItem();

  floater->SetGeometricParent(mBlock);

  // XXX the choice of constructors is confusing and non-obvious
  nsSize kidAvailSize(0, 0);
  nsHTMLReflowState reflowState(mPresContext, floater, *this,
                                kidAvailSize, eReflowReason_Initial);
  mBlock->ReflowFloater(mPresContext, *this, floater, reflowState);

  AddFloater(aPlaceholder);
}

// This is called by the line layout's AddFloater method when a
// place-holder frame is reflowed in a line. If the floater is a
// left-most child (it's x coordinate is at the line's left margin)
// then the floater is place immediately, otherwise the floater
// placement is deferred until the line has been reflowed.
void
nsBlockReflowState::AddFloater(nsPlaceholderFrame* aPlaceholder)
{
  // Update the current line's floater array
  NS_ASSERTION(nsnull != mCurrentLine, "null ptr");
  if (nsnull == mCurrentLine->mFloaters) {
    mCurrentLine->mFloaters = new nsVoidArray();
  }
  mCurrentLine->mFloaters->AppendElement(aPlaceholder);

  // Now place the floater immediately if possible. Otherwise stash it
  // away in mPendingFloaters and place it later.
  if (0 == mLineLayout.GetPlacedFrames()) {
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::AddFloater: IsLeftMostChild, placeHolder=%p",
        aPlaceholder));

    // Flush out pending bottom margin before placing floater
    if (0 != mPrevBottomMargin) {
      mY += mPrevBottomMargin;
      mPrevMarginFlags |= ALREADY_APPLIED_BOTTOM_MARGIN;
      mPrevBottomMargin = 0;
    }

    // Because we are in the middle of reflowing a placeholder frame
    // within a line (and possibly nested in an inline frame or two
    // that's a child of our block) we need to restore the space
    // manager's translation to the space that the block resides in
    // before placing the floater.
    PRBool isLeftFloater;
    nscoord ox, oy;
    mSpaceManager->GetTranslation(ox, oy);
    nscoord dx = ox - mSpaceManagerX;
    nscoord dy = oy - mSpaceManagerY;
    mSpaceManager->Translate(-dx, -dy);
    PlaceFloater(aPlaceholder, isLeftFloater);

    // Pass on updated available space to the current inline reflow engine
    GetAvailableSpace();
    mLineLayout.UpdateInlines(mCurrentBand.availSpace.x + mBorderPadding.left,
                              mY,
                              mCurrentBand.availSpace.width,
                              mCurrentBand.availSpace.height,
                              isLeftFloater);

    // Restore coordinate system
    mSpaceManager->Translate(dx, dy);
  }
  else {
    // This floater will be placed after the line is done (it is a
    // below current line floater).
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::AddFloater: pending, placeHolder=%p",
        aPlaceholder));
    mPendingFloaters.AppendElement(aPlaceholder);
  }
}

PRBool
nsBlockReflowState::IsLeftMostChild(nsIFrame* aFrame)
{
  for (;;) {
    nsIFrame* parent;
    aFrame->GetGeometricParent(parent);
    if (parent == mBlock) {
      nsIFrame* child = mCurrentLine->mFirstChild;
      PRInt32 n = mCurrentLine->ChildCount();
      while ((nsnull != child) && (aFrame != child) && (--n >= 0)) {
        nsSize  size;

        // Is the child zero-sized?
        child->GetSize(size);
        if (size.width > 0) {
          // We found a non-zero sized child frame that precedes aFrame
          return PR_FALSE;
        }
        child->GetNextSibling(child);
      }
      break;
    }
    else {
      // See if there are any non-zero sized child frames that precede
      // aFrame in the child list
      nsIFrame* child;
      parent->FirstChild(nsnull, child);
      while ((nsnull != child) && (aFrame != child)) {
        nsSize  size;

        // Is the child zero-sized?
        child->GetSize(size);
        if (size.width > 0) {
          // We found a non-zero sized child frame that precedes aFrame
          return PR_FALSE;
        }
        child->GetNextSibling(child);
      }
    }
  
    // aFrame is the left-most non-zero sized frame in its geometric parent.
    // Walk up one level and check that its parent is left-most as well
    aFrame = parent;
  }
  return PR_TRUE;
}

void
nsBlockReflowState::PlaceFloater(nsPlaceholderFrame* aPlaceholder,
                                 PRBool& aIsLeftFloater)
{
  nsIFrame* floater = aPlaceholder->GetAnchoredItem();

  // XXX the choice of constructors is confusing and non-obvious
  nsSize kidAvailSize(0, 0);
  nsHTMLReflowState reflowState(mPresContext, floater, *this, kidAvailSize);

  // Reflow the floater if it's targetted for a reflow
  if (nsnull != reflowCommand) {
    if (floater == mNextRCFrame) {
      reflowState.lineLayout = nsnull;
      mBlock->ReflowFloater(mPresContext, *this, floater, reflowState);
    }
  }

  // Get the type of floater
  const nsStyleDisplay* floaterDisplay;
  const nsStyleSpacing* floaterSpacing;
  floater->GetStyleData(eStyleStruct_Display,
                        (const nsStyleStruct*&)floaterDisplay);
  floater->GetStyleData(eStyleStruct_Spacing,
                        (const nsStyleStruct*&)floaterSpacing);

  // See if the floater should clear any preceeding floaters...
  if (NS_STYLE_CLEAR_NONE != floaterDisplay->mBreakType) {
    ClearFloaters(floaterDisplay->mBreakType);
  }
  else {
    // Get the band of available space
    GetAvailableSpace();
  }

  // Get the floaters bounding box and margin information
  nsRect region;
  floater->GetRect(region);
  nsMargin floaterMargin;
  ComputeMarginFor(floater, this, floaterMargin);

  // Adjust the floater size by its margin. That's the area that will
  // impact the space manager.
  region.width += floaterMargin.left + floaterMargin.right;
  region.height += floaterMargin.top + floaterMargin.bottom;

  // Find a place to place the floater. The CSS2 spec doesn't want
  // floaters overlapping each other or sticking out of the containing
  // block (CSS2 spec section 9.5.1, see the rule list).
  NS_ASSERTION((NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) ||
               (NS_STYLE_FLOAT_RIGHT == floaterDisplay->mFloats),
               "invalid float type");

  // While there is not enough room for the floater, clear past
  // other floaters until there is room (or the band is not impacted
  // by a floater).
  while ((mCurrentBand.availSpace.width < region.width) &&
         (mCurrentBand.availSpace.width < mContentArea.width)) {
    // The CSS2 spec says that floaters should be placed as high as
    // possible. We accomodate this easily by noting that if the band
    // is not the full width of the content area then it must have
    // been impacted by a floater. And we know that the height of the
    // band will be the height of the shortest floater, therefore we
    // adjust mY by that distance and keep trying until we have enough
    // space for this floater.
#ifdef NOISY_FLOATER_CLEARING
    mBlock->ListTag(stdout);
    printf(": clearing floater during floater placement: ");
    printf("availWidth=%d regionWidth=%d,%d(w/o margins) contentWidth=%d\n",
           mCurrentBand.availSpace.width, region.width,
           region.width - floaterMargin.left - floaterMargin.right,
           mContentArea.width);
#endif
    mY += mCurrentBand.availSpace.height;
    GetAvailableSpace();
  }

  // Assign an x and y coordinate to the floater. Note that the x,y
  // coordinates are computed <b>relative to the translation in the
  // spacemanager</b> which means that the impacted region will be
  // <b>inside</b> the border/padding area.
  if (NS_STYLE_FLOAT_LEFT == floaterDisplay->mFloats) {
    aIsLeftFloater = PR_TRUE;
    region.x = mCurrentBand.availSpace.x;
  }
  else {
    aIsLeftFloater = PR_FALSE;
    region.x = mCurrentBand.availSpace.XMost() - region.width;
  }
  region.y = mY - mBorderPadding.top;
  if (region.y < 0) {
    // CSS2 spec, 9.5.1 rule [4]: A floating box's outer top may not
    // be higher than the top of its containing block.

    // XXX It's not clear if it means the higher than the outer edge
    // or the border edge or the inner edge?
    region.y = 0;
  }

  // Place the floater in the space manager
  mSpaceManager->AddRectRegion(floater, region);

  // Set the origin of the floater frame, in frame coordinates. These
  // coordinates are <b>not</b> relative to the spacemanager
  // translation, therefore we have to factor in our border/padding.
  floater->MoveTo(mBorderPadding.left + floaterMargin.left + region.x,
                  mBorderPadding.top + floaterMargin.top + region.y);
}

/**
 * Place below-current-line floaters.
 */
void
nsBlockReflowState::PlaceFloaters(nsVoidArray* aFloaters, PRBool aAllOfThem)
{
  NS_PRECONDITION(aFloaters->Count() > 0, "no floaters");

  PRInt32 numFloaters = aFloaters->Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsPlaceholderFrame* placeholderFrame = (nsPlaceholderFrame*)
      aFloaters->ElementAt(i);
    if (!aAllOfThem && IsLeftMostChild(placeholderFrame)) {
      // Left-most children are placed during the line's reflow
      continue;
    }
    PRBool isLeftFloater;
    PlaceFloater(placeholderFrame, isLeftFloater);
  }

  // Update available spcae now that the floaters have been placed
  GetAvailableSpace();
}

void
nsBlockReflowState::ClearFloaters(PRUint8 aBreakType)
{
  // Update band information based on current mY before clearing
  GetAvailableSpace();

  for (;;) {
    PRBool haveFloater = PR_FALSE;

    // Find the Y coordinate to clear to. Note that the band trapezoid
    // coordinates are relative to the our spacemanager translation
    // (which means the band coordinates are inside the border+padding
    // area of this block frame).
    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::ClearFloaters: mY=%d trapCount=%d",
        mY, mCurrentBand.count));
    nscoord clearYMost = mY - mBorderPadding.top;
    nsRect tmp;
    PRInt32 i;
    for (i = 0; i < mCurrentBand.count; i++) {
      const nsStyleDisplay* display;
      nsBandTrapezoid* trapezoid = &mCurrentBand.data[i];
      NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
         ("nsBlockReflowState::ClearFloaters: trap=%d state=%d",
          i, trapezoid->state));
      if (nsBandTrapezoid::Available != trapezoid->state) {
        haveFloater = PR_TRUE;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 fn, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (fn = 0; fn < numFrames; fn++) {
            nsIFrame* frame = (nsIFrame*) trapezoid->frames->ElementAt(fn);
            frame->GetStyleData(eStyleStruct_Display,
                                (const nsStyleStruct*&)display);
            NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsBlockReflowState::ClearFloaters: frame[%d]=%p floats=%d",
                fn, frame, display->mFloats));
            switch (display->mFloats) {
            case NS_STYLE_FLOAT_LEFT:
              if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                  (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
                trapezoid->GetRect(tmp);
                nscoord ym = tmp.YMost();
                if (ym > clearYMost) {
                  clearYMost = ym;
                }
              }
              break;
            case NS_STYLE_FLOAT_RIGHT:
              if ((NS_STYLE_CLEAR_RIGHT == aBreakType) ||
                  (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
                trapezoid->GetRect(tmp);
                nscoord ym = tmp.YMost();
                if (ym > clearYMost) {
                  clearYMost = ym;
                  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                  ("nsBlockReflowState::ClearFloaters: right clearYMost=%d",
                   clearYMost));
                }
              }
              break;
            }
          }
        }
        else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
             ("nsBlockReflowState::ClearFloaters: frame=%p floats=%d",
              trapezoid->frame, display->mFloats));
          switch (display->mFloats) {
          case NS_STYLE_FLOAT_LEFT:
            if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
              trapezoid->GetRect(tmp);
              nscoord ym = tmp.YMost();
              if (ym > clearYMost) {
                clearYMost = ym;
                NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                  ("nsBlockReflowState::ClearFloaters: left clearYMost=%d",
                   clearYMost));
              }
            }
            break;
          case NS_STYLE_FLOAT_RIGHT:
            if ((NS_STYLE_CLEAR_RIGHT == aBreakType) ||
                (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
              trapezoid->GetRect(tmp);
              nscoord ym = tmp.YMost();
              if (ym > clearYMost) {
                clearYMost = ym;
                NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                  ("nsBlockReflowState::ClearFloaters: right clearYMost=%d",
                   clearYMost));
              }
            }
            break;
          }
        }
      }
    }

    // Nothing to clear
    if (!haveFloater || (clearYMost == mY - mBorderPadding.top)) {
      break;
    }

    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
       ("nsBlockReflowState::ClearFloaters: mY=%d clearYMost=%d",
        mY, clearYMost));
    mY = mBorderPadding.top + clearYMost + 1;

    // Get a new band
    GetAvailableSpace();
  }
}

//////////////////////////////////////////////////////////////////////
// Painting, event handling

PRIntn
nsBlockFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

NS_IMETHODIMP
nsBlockFrame::Paint(nsIPresContext&      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // Only paint the border and background if we're visible
  if (disp->mVisible) {
    PRIntn skipSides = GetSkipSides();
    const nsStyleColor* color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* spacing = (const nsStyleSpacing*)
      mStyleContext->GetStyleData(eStyleStruct_Spacing);

    // Paint background and border
    nsRect rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *color, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *spacing, skipSides);
  }

  // If overflow is hidden then set the clip rect so that children
  // don't leak out of us
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    PRBool clipState;
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(nsRect(0, 0, mRect.width, mRect.height),
                                  nsClipCombine_kIntersect, clipState);
  }

  // Child elements have the opportunity to override the visibility
  // property and display even if the parent is hidden
  PaintFloaters(aPresContext, aRenderingContext, aDirtyRect);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);

  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    PRBool clipState;
    aRenderingContext.PopState(clipState);
  }
  return NS_OK;
}

void
nsBlockFrame::PaintFloaters(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  for (LineData* line = mLines; nsnull != line; line = line->mNext) {
    nsVoidArray* floaters = line->mFloaters;
    if (nsnull == floaters) {
      continue;
    }
    PRInt32 i, n = floaters->Count();
    for (i = 0; i < n; i++) {
      nsPlaceholderFrame* ph = (nsPlaceholderFrame*) floaters->ElementAt(i);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect,
                 ph->GetAnchoredItem());
    }
  }
}

void
nsBlockFrame::PaintChildren(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  if (nsnull != mBullet) {
    // Paint outside bullets manually
    const nsStyleList* list = (const nsStyleList*)
    mStyleContext->GetStyleData(eStyleStruct_List);
    if (NS_STYLE_LIST_STYLE_POSITION_OUTSIDE == list->mListStylePosition) {
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, mBullet);
    }
  }

  for (LineData* line = mLines; nsnull != line; line = line->mNext) {
    // If the line has outside children or if the line intersects the
    // dirty rect then paint the children in the line.
    if (line->OutsideChildren() ||
        !((line->mBounds.YMost() <= aDirtyRect.y) ||
          (line->mBounds.y >= aDirtyRect.YMost()))) {
      nsIFrame* kid = line->mFirstChild;
      PRInt32 n = line->ChildCount();
      while (--n >= 0) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid);
        kid->GetNextSibling(kid);
      }
    }
  }
}

NS_IMETHODIMP
nsBlockFrame::GetFrameForPoint(const nsPoint& aPoint, nsIFrame** aFrame)
{
  nsresult rv = GetFrameForPointUsing(aPoint, nsnull, aFrame);
  if (NS_OK == rv) {
    return NS_OK;
  }
  if (nsnull != mBullet) {
    rv = GetFrameForPointUsing(aPoint, gBulletAtom, aFrame);
    if (NS_OK == rv) {
      return NS_OK;
    }
  }
  if (nsnull != mFloaters) {
    rv = GetFrameForPointUsing(aPoint, gFloaterAtom, aFrame);
    if (NS_OK == rv) {
      return NS_OK;
    }
  }
  *aFrame = this;
  return NS_ERROR_FAILURE;
}

//////////////////////////////////////////////////////////////////////
// Debugging

#ifdef NS_DEBUG
static PRBool
InLineList(LineData* aLines, nsIFrame* aFrame)
{
  while (nsnull != aLines) {
    nsIFrame* frame = aLines->mFirstChild;
    PRInt32 n = aLines->ChildCount();
    while (--n >= 0) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame->GetNextSibling(frame);
    }
    aLines = aLines->mNext;
  }
  return PR_FALSE;
}

static PRBool
InSiblingList(LineData* aLine, nsIFrame* aFrame)
{
  if (nsnull != aLine) {
    nsIFrame* frame = aLine->mFirstChild;
    while (nsnull != frame) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame->GetNextSibling(frame);
    }
  }
  return PR_FALSE;
}

PRBool
nsBlockFrame::IsChild(nsIFrame* aFrame)
{
  nsIFrame* parent;
  aFrame->GetGeometricParent(parent);
  if (parent != (nsIFrame*)this) {
    return PR_FALSE;
  }
  if (InLineList(mLines, aFrame) && InSiblingList(mLines, aFrame)) {
    return PR_TRUE;
  }
  if (InLineList(mOverflowLines, aFrame) &&
      InSiblingList(mOverflowLines, aFrame)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

#define VERIFY_ASSERT(_expr, _msg)      \
  if (!(_expr)) {                       \
    DumpTree();                         \
  }                                     \
  NS_ASSERTION(_expr, _msg)

NS_IMETHODIMP
nsBlockFrame::VerifyTree() const
{
  // XXX rewrite this
  return NS_OK;
}

#ifdef DO_SELECTION
nsIFrame * nsBlockFrame::FindHitFrame(nsBlockFrame * aBlockFrame, 
                                         const nscoord aX,
                                         const nscoord aY,
                                         const nsPoint & aPoint)
{
  nsPoint mousePoint(aPoint.x-aX, aPoint.y-aY);

  nsIFrame * contentFrame = nsnull;
  LineData * line         = aBlockFrame->mLines;
  if (nsnull != line) {
    // First find the line that contains the aIndex
    while (nsnull != line && contentFrame == nsnull) {
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->ChildCount();
      while (--n >= 0) {
        nsRect bounds;
        frame->GetRect(bounds);
        if (bounds.Contains(mousePoint)) {
           nsBlockFrame * blockFrame;
          if (NS_OK == frame->QueryInterface(kBlockFrameCID, (void**)&blockFrame)) {
            frame = FindHitFrame(blockFrame, bounds.x, bounds.y, aPoint);
            //NS_RELEASE(blockFrame);
            return frame;
          } else {
            return frame;
          }
        }
        frame->GetNextSibling(frame);
      }
      line = line->mNext;
    }
  }
  return aBlockFrame;
}

NS_IMETHODIMP
nsBlockFrame::HandleEvent(nsIPresContext& aPresContext,
                                    nsGUIEvent* aEvent,
                                    nsEventStatus& aEventStatus)
{
  if (DisplaySelection(aPresContext) == PR_FALSE) {
    if (aEvent->message != NS_MOUSE_LEFT_BUTTON_DOWN) {
      return NS_OK;
    }
  }

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    int x = 0;
  }

  //nsRect bounds;
  //GetRect(bounds);
  //nsIFrame * contentFrame = FindHitFrame(this, bounds.x, bounds.y, aEvent->point);
  nsIFrame * contentFrame = FindHitFrame(this, 0,0, aEvent->point);

  if (contentFrame == nsnull) {
    return NS_OK;
  }

  if(nsEventStatus_eConsumeNoDefault != aEventStatus) {
    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
    } else if (aEvent->message == NS_MOUSE_MOVE && mDoingSelection ||
               aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      // no-op
    } else {
      return NS_OK;
    }

    if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP) {
      if (mDoingSelection) {
        contentFrame->HandleRelease(aPresContext, aEvent, aEventStatus);
      }
    } else if (aEvent->message == NS_MOUSE_MOVE) {
      mDidDrag = PR_TRUE;
      contentFrame->HandleDrag(aPresContext, aEvent, aEventStatus);
    } else if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN) {
      contentFrame->HandlePress(aPresContext, aEvent, aEventStatus);
    }
  }

  return NS_OK;
}

nsIFrame * gNearByFrame = nsnull;

NS_METHOD nsBlockFrame::HandleDrag(nsIPresContext& aPresContext, 
                                      nsGUIEvent*     aEvent,
                                      nsEventStatus&  aEventStatus)
{
  if (DisplaySelection(aPresContext) == PR_FALSE)
  {
    aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

  // Keep old start and end
  //nsIContent * startContent = mSelectionRange->GetStartContent(); // ref counted
  //nsIContent * endContent   = mSelectionRange->GetEndContent();   // ref counted

  mDidDrag = PR_TRUE;

  nsIFrame * contentFrame = nsnull;
  LineData* line = mLines;
  if (nsnull != line) {
    // First find the line that contains the aIndex
    while (nsnull != line && contentFrame == nsnull) {
      nsIFrame* frame = line->mFirstChild;
      PRInt32 n = line->ChildCount();
      while (--n >= 0) {
        nsRect bounds;
        frame->GetRect(bounds);
        if (aEvent->point.y >= bounds.y && aEvent->point.y < bounds.y+bounds.height) {
          contentFrame = frame;
          if (frame != gNearByFrame) {
            if (gNearByFrame != nsnull) {
              int x = 0;
            }
            aEvent->point.x = bounds.x+bounds.width-50;
            gNearByFrame = frame;
            return contentFrame->HandleDrag(aPresContext, aEvent, aEventStatus);
          } else {
            return NS_OK;
          }
          //break;
        }
        frame->GetNextSibling(frame);
      }
      line = line->mNext;
    }
  }
  return NS_OK;
}
#endif

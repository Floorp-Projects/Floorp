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
#include "nsBulletFrame.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
#include "nsHTMLValue.h"
#include "nsHTMLContainerFrame.h"
#include "nsIFontMetrics.h"
#include "nsIHTMLContent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIReflowCommand.h"
#include "nsIRenderingContext.h"
#include "nsIURL.h"
#include "prprf.h"

nsBulletFrame::nsBulletFrame()
{
}

nsBulletFrame::~nsBulletFrame()
{
}

NS_IMETHODIMP
nsBulletFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  // Release image loader first so that its refcnt can go to zero
  mImageLoader.DestroyLoader();
  return nsFrame::DeleteFrame(aPresContext);
}

NS_IMETHODIMP
nsBulletFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Bullet", aResult);
}

NS_IMETHODIMP
nsBulletFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
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
nsBulletFrame::Paint(nsIPresContext&      aCX,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer)
{
  if (eFramePaintLayer_Content != aWhichLayer) {
    return NS_OK;
  }

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

PRInt32
nsBulletFrame::SetListItemOrdinal(PRInt32 aNextOrdinal)
{
  // Assume that the ordinal comes from the block reflow state
  mOrdinal = aNextOrdinal;

  // Try to get value directly from the list-item, if it specifies a
  // value attribute. Note: we do this with our parent's content
  // because our parent is the list-item.
  nsHTMLValue value;
  nsIContent* parentContent;
  mParent->GetContent(parentContent);
  nsIHTMLContent* hc;
  if (NS_OK == parentContent->QueryInterface(kIHTMLContentIID, (void**) &hc)) {
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetHTMLAttribute(nsHTMLAtoms::value, value)) {
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

  return mOrdinal + 1;
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
nsBulletFrame::GetListItemText(nsIPresContext& aCX,
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
nsBulletFrame::GetDesiredSize(nsIPresContext*  aCX,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics)
{
  const nsStyleList* myList =
    (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
  nscoord ascent;

  if (myList->mListStyleImage.Length() > 0) {
    mImageLoader.SetURLSpec(myList->mListStyleImage);
    nsIURL* baseURL = nsnull;
    nsIHTMLContent* htmlContent;
    if (NS_SUCCEEDED(mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) {
      htmlContent->GetBaseURL(baseURL);
      NS_RELEASE(htmlContent);
    }
    else {
      nsIDocument* doc;
      if (NS_SUCCEEDED(mContent->GetDocument(doc))) {
        doc->GetBaseURL(baseURL);
        NS_RELEASE(doc);
      }
    }
    mImageLoader.SetBaseURL(baseURL);
    NS_IF_RELEASE(baseURL);
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
nsBulletFrame::Reflow(nsIPresContext& aPresContext,
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

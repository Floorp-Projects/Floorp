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
#include "nsHTMLTagContent.h"
#include "nsLeafFrame.h"
#include "nsBlockFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLImage.h"
#include "prprf.h"
#include "nsHTMLFrame.h"
#include "nsIView.h"

class Bullet : public nsHTMLTagContent {
public:
  Bullet();

  NS_IMETHOD IsSynthetic(PRBool& aResult);

  void MapAttributesInto(nsIStyleContext* aContext, 
                         nsIPresContext* aPresContext);

  void List(FILE* out, PRInt32 aIndent) const;

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);
};

class BulletFrame : public nsLeafFrame {
public:
  BulletFrame(nsIContent* aContent, nsIFrame* aParentFrame);
  virtual ~BulletFrame();

  NS_IMETHOD DeleteFrame();

  NS_IMETHOD Paint(nsIPresContext &aCX,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext* aCX,
                    nsReflowMetrics& aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;

protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);

  /**
   * Return the reflow state for the list container that contains this
   * list item frame. There may be no list container (a dangling LI)
   * therefore this may return nsnull.
   */
  nsBlockReflowState* GetListContainerReflowState(nsIPresContext* aCX);

  PRInt32 GetListItemOrdinal(nsIPresContext* aCX, const nsStyleList& aMol);

  void GetListItemText(nsIPresContext* aCX, nsString& aResult,
                       const nsStyleList& aMol);

  PRPackedBool mOrdinalValid;
  PRInt32 mOrdinal;
  nsMargin mPadding;
  nsHTMLImageLoader mImageLoader;
};

//----------------------------------------------------------------------

Bullet::Bullet()
{
}

void
Bullet::MapAttributesInto(nsIStyleContext* aContext, 
                          nsIPresContext*  aPresContext)
{
  // Force display to be inline (bullet's are not block level)
  nsStyleDisplay* display = (nsStyleDisplay*)
    aContext->GetMutableStyleData(eStyleStruct_Display);
  display->mDisplay = NS_STYLE_DISPLAY_INLINE;
}

NS_METHOD
Bullet::IsSynthetic(PRBool& aResult)
{
  aResult = PR_TRUE;
  return NS_OK;
}

void
Bullet::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "Bullet RefCnt=%d<>\n", mRefCnt);
}

nsresult
Bullet::CreateFrame(nsIPresContext*  aPresContext,
                    nsIFrame*        aParentFrame,
                    nsIStyleContext* aStyleContext,
                    nsIFrame*&       aResult)
{
  BulletFrame* frame = new BulletFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  frame->SetStyleContext(aPresContext, aStyleContext);
  return NS_OK;
}

nsresult
NS_NewHTMLBullet(nsIHTMLContent** aInstancePtrResult)
{
  Bullet* it = new Bullet();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

//----------------------------------------------------------------------

BulletFrame::BulletFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aParentFrame)
{
}

BulletFrame::~BulletFrame()
{
}

NS_METHOD
BulletFrame::DeleteFrame()
{
  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.DestroyLoader();
  return nsLeafFrame::DeleteFrame();
}

NS_METHOD
BulletFrame::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  PRInt32 contentIndex;
  GetContentIndex(contentIndex);
  fprintf(out, "Bullet(%d)@%p ", 
          contentIndex, this);
  nsIView* view;
  GetView(view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
    NS_RELEASE(view);
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
      GetListItemText(&aCX, text, *myList);
      aRenderingContext.SetFont(myFont->mFont);
      aRenderingContext.DrawString(text, mPadding.left, mPadding.top,
                                   fm->GetWidth(text));
      NS_RELEASE(fm);
      break;
    }
  }
  return NS_OK;
}

NS_METHOD
BulletFrame::Reflow(nsIPresContext*      aCX,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus)
{
  GetDesiredSize(aCX, aReflowState, aDesiredSize);
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  // Get cached state for containing block frame
  nsBlockReflowState* state =
    nsBlockFrame::FindBlockReflowState(aCX, this);
  if (nsnull != state) {
    nsLineLayout* lineLayoutState = state->mCurrentLine;
    if (nsnull != lineLayoutState) {
      lineLayoutState->mReflowResult =
        NS_LINE_LAYOUT_REFLOW_RESULT_AWARE;
    }
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// Return the reflow state for the list container that contains this
// list item frame. There may be no list container (a dangling LI)
// therefore this may return nsnull.
nsBlockReflowState*
BulletFrame::GetListContainerReflowState(nsIPresContext* aCX)
{
  nsBlockReflowState* state = nsnull;
  nsIFrame* parent = mGeometricParent;
  while (nsnull != parent) {
    void* ft;
    nsresult status = parent->QueryInterface(kBlockFrameCID, &ft);
    if (NS_OK == status) {
      // The parent is a block. See if its content object is a list
      // container. Only UL, OL, MENU or DIR can be list containers.
      // XXX need something more flexible, say style?
      nsIContent* parentContent;
         
      parent->GetContent(parentContent);
      nsIAtom* tag = parentContent->GetTag();
      NS_RELEASE(parentContent);
      if ((tag == nsHTMLAtoms::ul) || (tag == nsHTMLAtoms::ol) ||
          (tag == nsHTMLAtoms::menu) || (tag == nsHTMLAtoms::dir)) {
        NS_RELEASE(tag);
        break;
      }
      NS_RELEASE(tag);
    }
    parent->GetGeometricParent(parent);
  }
  if (nsnull != parent) {
    nsIPresShell* shell = aCX->GetShell();
    state = (nsBlockReflowState*) shell->GetCachedData(parent);
    NS_RELEASE(shell);
  }
  return state;
}

PRInt32
BulletFrame::GetListItemOrdinal(nsIPresContext* aCX,
                                const nsStyleList& aListStyle)
{
  if (mOrdinalValid) {
    return mOrdinal;
  }

  PRInt32 ordinal = 0;

  // Get block reflow state for the list container
  nsBlockReflowState* state = GetListContainerReflowState(aCX);

  // Try to get value directly from the list-item, if it specifies a
  // value attribute. We do this with our parent's content.
  nsHTMLValue value;
  nsIContent* parentContent;
  mContentParent->GetContent(parentContent);
  nsIHTMLContent* html = (nsIHTMLContent*) parentContent;
  if (eContentAttr_HasValue == html->GetAttribute(nsHTMLAtoms::value, value)) {
    if (eHTMLUnit_Integer == value.GetUnit()) {
      ordinal = value.GetIntValue();
      if (nsnull != state) {
        state->mNextListOrdinal = ordinal + 1;
      }
      NS_RELEASE(html);
      goto done;
    }
  }
  NS_RELEASE(html);

  // Get ordinal from block reflow state
  if (nsnull != state) {
    ordinal = state->mNextListOrdinal;
    if (ordinal < 0) {
      // This is the first list item and the list container doesn't
      // have a "start" attribute. Get the starting ordinal value
      // correctly set.
      switch (aListStyle.mListStyleType) {
      case NS_STYLE_LIST_STYLE_DECIMAL:
      case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
      case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
        ordinal = 1;
        break;
      default:
        ordinal = 0;
        break;
      }
    }
    state->mNextListOrdinal = ordinal + 1;
  }

 done:
  mOrdinal = ordinal;
  mOrdinalValid = PR_TRUE;
  return ordinal;
}

static const char* gLowerRomanCharsA = "ixcm";
static const char* gUpperRomanCharsA = "IXCM";
static const char* gLowerRomanCharsB = "vld?";
static const char* gUpperRomanCharsB = "VLD?";
static const char* gLowerAlphaChars  = "abcdefghijklmnopqrstuvwxyz";
static const char* gUpperAlphaChars  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void
BulletFrame::GetListItemText(nsIPresContext* aCX,
                             nsString& result,
                             const nsStyleList& aListStyle)
{
  PRInt32 ordinal = GetListItemOrdinal(aCX, aListStyle);
  char cbuf[40];
  switch (aListStyle.mListStyleType) {
  case NS_STYLE_LIST_STYLE_DECIMAL:
    PR_snprintf(cbuf, sizeof(cbuf), "%ld", ordinal);
    result.Append(cbuf);
    break;

  case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
  case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
  {
    // XXX deal with an ordinal of ZERO
    nsAutoString addOn;
    nsAutoString decStr;
    decStr.Append(ordinal, 10);
    const PRUnichar* dp = decStr.GetUnicode();
    const PRUnichar* end = dp + decStr.Length();

    PRIntn           len=decStr.Length();
    PRIntn           romanPos=len;
    PRIntn           n;
    PRBool           negative=PRBool(ordinal<0);

    const char* achars;
    const char* bchars;
    if (aListStyle.mListStyleType == NS_STYLE_LIST_STYLE_LOWER_ROMAN) {
      achars = gLowerRomanCharsA;
      bchars = gLowerRomanCharsB;
    } else {
      achars = gUpperRomanCharsA;
      bchars = gUpperRomanCharsB;
    }
    ordinal=(ordinal < 0) ? -ordinal : ordinal;
    if (ordinal < 0) {
      // XXX max negative int
      break;
    }
    for (; dp < end; dp++)
    {
      romanPos--;
      addOn.SetLength(0);
      switch(*dp)
      {
      case '3':  addOn.Append(achars[romanPos]);
      case '2':  addOn.Append(achars[romanPos]);
      case '1':  addOn.Append(achars[romanPos]);
        break;

      case '4':
        addOn.Append(achars[romanPos]);

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
    ordinal = (ordinal < 0) ? -ordinal : ordinal;
    if (ordinal < 0) {
      // XXX max negative int
      break;
    }
    while (next<=ordinal)      // scale up in baseN; exceed current value.
    {
      root=next;
      next*=aBase;
      expn++;
    }

    while(0!=(expn--))
    {
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

// from laytext.c
#define MIN_BULLET_SIZE 5

void
BulletFrame::GetDesiredSize(nsIPresContext*  aCX,
                            const nsReflowState& aReflowState,
                            nsReflowMetrics& aDesiredSize)
{
  const nsStyleList* myList =
    (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
  if (myList->mListStyleImage.Length() > 0) {
    mImageLoader.SetURL(myList->mListStyleImage);
    mImageLoader.GetDesiredSize(aCX, aReflowState, aDesiredSize);
    if (!mImageLoader.GetLoadImageFailed()) {
      nsHTMLFrame::CreateViewForFrame(aCX, this, mStyleContext, PR_FALSE);
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
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
    aDesiredSize.width = 0;
    aDesiredSize.height = 0;
    aDesiredSize.ascent = 0;
    aDesiredSize.descent = 0;
    break;

  default:
  case NS_STYLE_LIST_STYLE_DISC:
  case NS_STYLE_LIST_STYLE_CIRCLE:
  case NS_STYLE_LIST_STYLE_BASIC:
  case NS_STYLE_LIST_STYLE_SQUARE:
    t2p = aCX->GetTwipsToPixels();
    bulletSize = nscoord(t2p * fm->GetMaxAscent() / 2);
    if (bulletSize < 1) {
      bulletSize = MIN_BULLET_SIZE;
    }
    p2t = aCX->GetPixelsToTwips();
    bulletSize = nscoord(p2t * bulletSize);
    mPadding.bottom = (fm->GetMaxAscent() / 8);
    if (NS_STYLE_LIST_STYLE_POSITION_INSIDE == myList->mListStylePosition) {
      mPadding.right = bulletSize / 2;
    }
    aDesiredSize.width = mPadding.right + bulletSize;
    aDesiredSize.height = mPadding.bottom + bulletSize;
    aDesiredSize.ascent = mPadding.bottom + bulletSize;
    aDesiredSize.descent = 0;
    break;

  case NS_STYLE_LIST_STYLE_DECIMAL:
  case NS_STYLE_LIST_STYLE_LOWER_ROMAN:
  case NS_STYLE_LIST_STYLE_UPPER_ROMAN:
  case NS_STYLE_LIST_STYLE_LOWER_ALPHA:
  case NS_STYLE_LIST_STYLE_UPPER_ALPHA:
    GetListItemText(aCX, text, *myList);
    if (NS_STYLE_LIST_STYLE_POSITION_INSIDE == myList->mListStylePosition) {
      // Inside bullets need some extra width to get the padding
      // between the list item and the content that follows.
      mPadding.right = fm->GetHeight() / 2;          // From old layout engine
    }
    aDesiredSize.width = mPadding.right + fm->GetWidth(text);
    aDesiredSize.height = fm->GetHeight();
    aDesiredSize.ascent = fm->GetMaxAscent();
    aDesiredSize.descent = fm->GetMaxDescent();
    break;
  }
  NS_RELEASE(fm);
}

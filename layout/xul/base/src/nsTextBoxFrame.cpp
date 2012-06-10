/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTextBoxFrame.h"

#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsBoxLayoutState.h"
#include "nsMenuBarListener.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULLabelElement.h"
#include "nsEventStateManager.h"
#include "nsITheme.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsCSSRendering.h"
#include "nsIReflowCallback.h"
#include "nsBoxFrame.h"
#include "mozilla/Preferences.h"
#include "nsLayoutUtils.h"

#ifdef IBMBIDI
#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

using namespace mozilla;

class nsAccessKeyInfo
{
public:
    PRInt32 mAccesskeyIndex;
    nscoord mBeforeWidth, mAccessWidth, mAccessUnderlineSize, mAccessOffset;
};


bool nsTextBoxFrame::gAlwaysAppendAccessKey          = false;
bool nsTextBoxFrame::gAccessKeyPrefInitialized       = false;
bool nsTextBoxFrame::gInsertSeparatorBeforeAccessKey = false;
bool nsTextBoxFrame::gInsertSeparatorPrefInitialized = false;

nsIFrame*
NS_NewTextBoxFrame (nsIPresShell* aPresShell, nsStyleContext* aContext)
{
    return new (aPresShell) nsTextBoxFrame (aPresShell, aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTextBoxFrame)


NS_IMETHODIMP
nsTextBoxFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
    bool aResize;
    bool aRedraw;

    UpdateAttributes(aAttribute, aResize, aRedraw);

    if (aResize) {
        PresContext()->PresShell()->
            FrameNeedsReflow(this, nsIPresShell::eStyleChange,
                             NS_FRAME_IS_DIRTY);
    } else if (aRedraw) {
        nsBoxLayoutState state(PresContext());
        Redraw(state);
    }

    // If the accesskey changed, register for the new value
    // The old value has been unregistered in nsXULElement::SetAttr
    if (aAttribute == nsGkAtoms::accesskey || aAttribute == nsGkAtoms::control)
        RegUnregAccessKey(true);

    return NS_OK;
}

nsTextBoxFrame::nsTextBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsLeafBoxFrame(aShell, aContext), mAccessKeyInfo(nsnull), mCropType(CropRight),
  mNeedsReflowCallback(false)
{
    MarkIntrinsicWidthsDirty();
}

nsTextBoxFrame::~nsTextBoxFrame()
{
    delete mAccessKeyInfo;
}


NS_IMETHODIMP
nsTextBoxFrame::Init(nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIFrame*        aPrevInFlow)
{
    nsTextBoxFrameSuper::Init(aContent, aParent, aPrevInFlow);

    bool aResize;
    bool aRedraw;
    UpdateAttributes(nsnull, aResize, aRedraw); /* update all */

    // register access key
    RegUnregAccessKey(true);

    return NS_OK;
}

void
nsTextBoxFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
    // unregister access key
    RegUnregAccessKey(false);
    nsTextBoxFrameSuper::DestroyFrom(aDestructRoot);
}

bool
nsTextBoxFrame::AlwaysAppendAccessKey()
{
  if (!gAccessKeyPrefInitialized) 
  {
    gAccessKeyPrefInitialized = true;

    const char* prefName = "intl.menuitems.alwaysappendaccesskeys";
    nsAdoptingString val = Preferences::GetLocalizedString(prefName);
    gAlwaysAppendAccessKey = val.Equals(NS_LITERAL_STRING("true"));
  }
  return gAlwaysAppendAccessKey;
}

bool
nsTextBoxFrame::InsertSeparatorBeforeAccessKey()
{
  if (!gInsertSeparatorPrefInitialized)
  {
    gInsertSeparatorPrefInitialized = true;

    const char* prefName = "intl.menuitems.insertseparatorbeforeaccesskeys";
    nsAdoptingString val = Preferences::GetLocalizedString(prefName);
    gInsertSeparatorBeforeAccessKey = val.EqualsLiteral("true");
  }
  return gInsertSeparatorBeforeAccessKey;
}

class nsAsyncAccesskeyUpdate : public nsIReflowCallback
{
public:
    nsAsyncAccesskeyUpdate(nsIFrame* aFrame) : mWeakFrame(aFrame)
    {
    }

    virtual bool ReflowFinished()
    {
        bool shouldFlush = false;
        nsTextBoxFrame* frame =
            static_cast<nsTextBoxFrame*>(mWeakFrame.GetFrame());
        if (frame) {
            shouldFlush = frame->UpdateAccesskey(mWeakFrame);
        }
        delete this;
        return shouldFlush;
    }

    virtual void ReflowCallbackCanceled()
    {
        delete this;
    }

    nsWeakFrame mWeakFrame;
};

bool
nsTextBoxFrame::UpdateAccesskey(nsWeakFrame& aWeakThis)
{
    nsAutoString accesskey;
    nsCOMPtr<nsIDOMXULLabelElement> labelElement = do_QueryInterface(mContent);
    NS_ENSURE_TRUE(aWeakThis.IsAlive(), false);
    if (labelElement) {
        // Accesskey may be stored on control.
        // Because this method is called by the reflow callback, current context
        // may not be the right one. Pushing the context of mContent so that
        // if nsIDOMXULLabelElement is implemented in XBL, we don't get a
        // security exception.
        nsCxPusher cx;
        if (cx.Push(mContent)) {
          labelElement->GetAccessKey(accesskey);
          NS_ENSURE_TRUE(aWeakThis.IsAlive(), false);
        }
    }
    else {
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accesskey);
    }

    if (!accesskey.Equals(mAccessKey)) {
        // Need to get clean mTitle.
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, mTitle);
        mAccessKey = accesskey;
        UpdateAccessTitle();
        PresContext()->PresShell()->
            FrameNeedsReflow(this, nsIPresShell::eStyleChange,
                             NS_FRAME_IS_DIRTY);
        return true;
    }
    return false;
}

void
nsTextBoxFrame::UpdateAttributes(nsIAtom*         aAttribute,
                                 bool&          aResize,
                                 bool&          aRedraw)
{
    bool doUpdateTitle = false;
    aResize = false;
    aRedraw = false;

    if (aAttribute == nsnull || aAttribute == nsGkAtoms::crop) {
        static nsIContent::AttrValuesArray strings[] =
          {&nsGkAtoms::left, &nsGkAtoms::start, &nsGkAtoms::center,
           &nsGkAtoms::right, &nsGkAtoms::end, nsnull};
        CroppingStyle cropType;
        switch (mContent->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::crop,
                                          strings, eCaseMatters)) {
          case 0:
          case 1:
            cropType = CropLeft;
            break;
          case 2:
            cropType = CropCenter;
            break;
          case 3:
          case 4:
            cropType = CropRight;
            break;
          default:
            cropType = CropNone;
            break;
        }

        if (cropType != mCropType) {
            aResize = true;
            mCropType = cropType;
        }
    }

    if (aAttribute == nsnull || aAttribute == nsGkAtoms::value) {
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, mTitle);
        doUpdateTitle = true;
    }

    if (aAttribute == nsnull || aAttribute == nsGkAtoms::accesskey) {
        mNeedsReflowCallback = true;
        // Ensure that layout is refreshed and reflow callback called.
        aResize = true;
    }

    if (doUpdateTitle) {
        UpdateAccessTitle();
        aResize = true;
    }

}

class nsDisplayXULTextBox : public nsDisplayItem {
public:
  nsDisplayXULTextBox(nsDisplayListBuilder* aBuilder,
                      nsTextBoxFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame),
    mDisableSubpixelAA(false)
  {
    MOZ_COUNT_CTOR(nsDisplayXULTextBox);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULTextBox() {
    MOZ_COUNT_DTOR(nsDisplayXULTextBox);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap);
  NS_DISPLAY_DECL_NAME("XULTextBox", TYPE_XUL_TEXT_BOX)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder);

  virtual void DisableComponentAlpha() { mDisableSubpixelAA = true; }

  void PaintTextToContext(nsRenderingContext* aCtx,
                          nsPoint aOffset,
                          const nscolor* aColor);

  bool mDisableSubpixelAA;
};

static void
PaintTextShadowCallback(nsRenderingContext* aCtx,
                        nsPoint aShadowOffset,
                        const nscolor& aShadowColor,
                        void* aData)
{
  reinterpret_cast<nsDisplayXULTextBox*>(aData)->
           PaintTextToContext(aCtx, aShadowOffset, &aShadowColor);
}

void
nsDisplayXULTextBox::Paint(nsDisplayListBuilder* aBuilder,
                           nsRenderingContext* aCtx)
{
  gfxContextAutoDisableSubpixelAntialiasing disable(aCtx->ThebesContext(),
                                                    mDisableSubpixelAA);

  // Paint the text shadow before doing any foreground stuff
  nsRect drawRect = static_cast<nsTextBoxFrame*>(mFrame)->mTextDrawRect +
                    ToReferenceFrame();
  nsLayoutUtils::PaintTextShadow(mFrame, aCtx,
                                 drawRect, mVisibleRect,
                                 mFrame->GetStyleColor()->mColor,
                                 PaintTextShadowCallback,
                                 (void*)this);

  PaintTextToContext(aCtx, nsPoint(0, 0), nsnull);
}

void
nsDisplayXULTextBox::PaintTextToContext(nsRenderingContext* aCtx,
                                        nsPoint aOffset,
                                        const nscolor* aColor)
{
  static_cast<nsTextBoxFrame*>(mFrame)->
    PaintTitle(*aCtx, mVisibleRect, ToReferenceFrame() + aOffset, aColor);
}

nsRect
nsDisplayXULTextBox::GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
  *aSnap = false;
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

nsRect
nsDisplayXULTextBox::GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder)
{
  return static_cast<nsTextBoxFrame*>(mFrame)->GetComponentAlphaBounds() +
      ToReferenceFrame();
}

NS_IMETHODIMP
nsTextBoxFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                 const nsRect&           aDirtyRect,
                                 const nsDisplayListSet& aLists)
{
    if (!IsVisibleForPainting(aBuilder))
      return NS_OK;

    nsresult rv = nsLeafBoxFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
    
    return aLists.Content()->AppendNewToTop(new (aBuilder)
        nsDisplayXULTextBox(aBuilder, this));
}

void
nsTextBoxFrame::PaintTitle(nsRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsPoint              aPt,
                           const nscolor*       aOverrideColor)
{
    if (mTitle.IsEmpty())
        return;

    DrawText(aRenderingContext, aDirtyRect, mTextDrawRect + aPt, aOverrideColor);
}

void
nsTextBoxFrame::DrawText(nsRenderingContext& aRenderingContext,
                         const nsRect&       aDirtyRect,
                         const nsRect&       aTextRect,
                         const nscolor*      aOverrideColor)
{
    nsPresContext* presContext = PresContext();

    // paint the title
    nscolor overColor;
    nscolor underColor;
    nscolor strikeColor;
    PRUint8 overStyle;
    PRUint8 underStyle;
    PRUint8 strikeStyle;

    // Begin with no decorations
    PRUint8 decorations = NS_STYLE_TEXT_DECORATION_LINE_NONE;
    // A mask of all possible decorations.
    PRUint8 decorMask = NS_STYLE_TEXT_DECORATION_LINE_LINES_MASK;

    nsIFrame* f = this;
    do {  // find decoration colors
      nsStyleContext* context = f->GetStyleContext();
      if (!context->HasTextDecorationLines()) {
        break;
      }
      const nsStyleTextReset* styleText = context->GetStyleTextReset();
      
      if (decorMask & styleText->mTextDecorationLine) {  // a decoration defined here
        nscolor color;
        if (aOverrideColor) {
          color = *aOverrideColor;
        } else {
          bool isForeground;
          styleText->GetDecorationColor(color, isForeground);
          if (isForeground) {
            color = nsLayoutUtils::GetColor(f, eCSSProperty_color);
          }
        }
        PRUint8 style = styleText->GetDecorationStyle();

        if (NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE & decorMask &
              styleText->mTextDecorationLine) {
          underColor = color;
          underStyle = style;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
          decorations |= NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_LINE_OVERLINE & decorMask &
              styleText->mTextDecorationLine) {
          overColor = color;
          overStyle = style;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_OVERLINE;
          decorations |= NS_STYLE_TEXT_DECORATION_LINE_OVERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH & decorMask &
              styleText->mTextDecorationLine) {
          strikeColor = color;
          strikeStyle = style;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH;
          decorations |= NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH;
        }
      }
    } while (0 != decorMask &&
             (f = nsLayoutUtils::GetParentOrPlaceholderFor(f)));

    nsRefPtr<nsFontMetrics> fontMet;
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));

    nscoord offset;
    nscoord size;
    nscoord ascent = fontMet->MaxAscent();

    nscoord baseline =
      presContext->RoundAppUnitsToNearestDevPixels(aTextRect.y + ascent);
    nsRefPtr<gfxContext> ctx = aRenderingContext.ThebesContext();
    gfxPoint pt(presContext->AppUnitsToGfxUnits(aTextRect.x),
                presContext->AppUnitsToGfxUnits(aTextRect.y));
    gfxFloat width = presContext->AppUnitsToGfxUnits(aTextRect.width);
    gfxFloat ascentPixel = presContext->AppUnitsToGfxUnits(ascent);
    gfxRect dirtyRect(presContext->AppUnitsToGfxUnits(aDirtyRect));

    // Underlines are drawn before overlines, and both before the text
    // itself, per http://www.w3.org/TR/CSS21/zindex.html point 7.2.1.4.1.1.
    // (We don't apply this rule to the access-key underline because we only
    // find out where that is as a side effect of drawing the text, in the
    // general case -- see below.)
    if (decorations & (NS_FONT_DECORATION_OVERLINE |
                       NS_FONT_DECORATION_UNDERLINE)) {
      fontMet->GetUnderline(offset, size);
      gfxFloat offsetPixel = presContext->AppUnitsToGfxUnits(offset);
      gfxFloat sizePixel = presContext->AppUnitsToGfxUnits(size);
      if ((decorations & NS_FONT_DECORATION_UNDERLINE) &&
          underStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
        nsCSSRendering::PaintDecorationLine(ctx, dirtyRect, underColor,
                          pt, gfxSize(width, sizePixel),
                          ascentPixel, offsetPixel,
                          NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE, underStyle);
      }
      if ((decorations & NS_FONT_DECORATION_OVERLINE) &&
          overStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
        nsCSSRendering::PaintDecorationLine(ctx, dirtyRect, overColor,
                          pt, gfxSize(width, sizePixel),
                          ascentPixel, ascentPixel,
                          NS_STYLE_TEXT_DECORATION_LINE_OVERLINE, overStyle);
      }
    }

    nsRefPtr<nsRenderingContext> refContext =
        PresContext()->PresShell()->GetReferenceRenderingContext();

    aRenderingContext.SetFont(fontMet);
    refContext->SetFont(fontMet);

    CalculateUnderline(*refContext);

    aRenderingContext.SetColor(aOverrideColor ? *aOverrideColor : GetStyleColor()->mColor);

#ifdef IBMBIDI
    nsresult rv = NS_ERROR_FAILURE;

    if (mState & NS_FRAME_IS_BIDI) {
      presContext->SetBidiEnabled();
      const nsStyleVisibility* vis = GetStyleVisibility();
      nsBidiDirection direction = (NS_STYLE_DIRECTION_RTL == vis->mDirection) ? NSBIDI_RTL : NSBIDI_LTR;
      if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
          // We let the RenderText function calculate the mnemonic's
          // underline position for us.
          nsBidiPositionResolve posResolve;
          posResolve.logicalIndex = mAccessKeyInfo->mAccesskeyIndex;
          rv = nsBidiPresUtils::RenderText(mCroppedTitle.get(), mCroppedTitle.Length(), direction,
                                           presContext, aRenderingContext,
                                           *refContext,
                                           aTextRect.x, baseline,
                                           &posResolve,
                                           1);
          mAccessKeyInfo->mBeforeWidth = posResolve.visualLeftTwips;
          mAccessKeyInfo->mAccessWidth = posResolve.visualWidth;
      }
      else
      {
          rv = nsBidiPresUtils::RenderText(mCroppedTitle.get(), mCroppedTitle.Length(), direction,
                                           presContext, aRenderingContext,
                                           *refContext,
                                           aTextRect.x, baseline);
      }
    }
    if (NS_FAILED(rv) )
#endif // IBMBIDI
    {
       aRenderingContext.SetTextRunRTL(false);

       if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
           // In the simple (non-BiDi) case, we calculate the mnemonic's
           // underline position by getting the text metric.
           // XXX are attribute values always two byte?
           if (mAccessKeyInfo->mAccesskeyIndex > 0)
               mAccessKeyInfo->mBeforeWidth =
                   refContext->GetWidth(mCroppedTitle.get(),
                                        mAccessKeyInfo->mAccesskeyIndex);
           else
               mAccessKeyInfo->mBeforeWidth = 0;
       }

       fontMet->DrawString(mCroppedTitle.get(), mCroppedTitle.Length(),
                           aTextRect.x, baseline, &aRenderingContext,
                           refContext.get());
    }

    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
        aRenderingContext.FillRect(aTextRect.x + mAccessKeyInfo->mBeforeWidth,
                                   aTextRect.y + mAccessKeyInfo->mAccessOffset,
                                   mAccessKeyInfo->mAccessWidth,
                                   mAccessKeyInfo->mAccessUnderlineSize);
    }

    // Strikeout is drawn on top of the text, per
    // http://www.w3.org/TR/CSS21/zindex.html point 7.2.1.4.1.1.
    if ((decorations & NS_FONT_DECORATION_LINE_THROUGH) &&
        strikeStyle != NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
      fontMet->GetStrikeout(offset, size);
      gfxFloat offsetPixel = presContext->AppUnitsToGfxUnits(offset);
      gfxFloat sizePixel = presContext->AppUnitsToGfxUnits(size);
      nsCSSRendering::PaintDecorationLine(ctx, dirtyRect, strikeColor,
                        pt, gfxSize(width, sizePixel), ascentPixel, offsetPixel,
                        NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH,
                        strikeStyle);
    }
}

void
nsTextBoxFrame::CalculateUnderline(nsRenderingContext& aRenderingContext)
{
    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
         // Calculate all fields of mAccessKeyInfo which
         // are the same for both BiDi and non-BiDi frames.
         const PRUnichar *titleString = mCroppedTitle.get();
         aRenderingContext.SetTextRunRTL(false);
         mAccessKeyInfo->mAccessWidth =
             aRenderingContext.GetWidth(titleString[mAccessKeyInfo->
                                                    mAccesskeyIndex]);

         nscoord offset, baseline;
         nsFontMetrics* metrics = aRenderingContext.FontMetrics();
         metrics->GetUnderline(offset, mAccessKeyInfo->mAccessUnderlineSize);
         baseline = metrics->MaxAscent();
         mAccessKeyInfo->mAccessOffset = baseline - offset;
    }
}

nscoord
nsTextBoxFrame::CalculateTitleForWidth(nsPresContext*      aPresContext,
                                       nsRenderingContext& aRenderingContext,
                                       nscoord              aWidth)
{
    if (mTitle.IsEmpty())
        return 0;

    nsRefPtr<nsFontMetrics> fm;
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
    aRenderingContext.SetFont(fm);

    // see if the text will completely fit in the width given
    nscoord titleWidth = nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                                       mTitle.get(), mTitle.Length());

    if (titleWidth <= aWidth) {
        mCroppedTitle = mTitle;
#ifdef IBMBIDI
        if (HasRTLChars(mTitle)) {
            mState |= NS_FRAME_IS_BIDI;
        }
#endif // IBMBIDI
        return titleWidth;  // fits, done.
    }

    const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();
    // start with an ellipsis
    mCroppedTitle.Assign(kEllipsis);

    // see if the width is even smaller than the ellipsis
    // if so, clear the text (XXX set as many '.' as we can?).
    aRenderingContext.SetTextRunRTL(false);
    titleWidth = aRenderingContext.GetWidth(kEllipsis);

    if (titleWidth > aWidth) {
        mCroppedTitle.SetLength(0);
        return 0;
    }

    // if the ellipsis fits perfectly, no use in trying to insert
    if (titleWidth == aWidth)
        return titleWidth;

    aWidth -= titleWidth;

    // XXX: This whole block should probably take surrogates into account
    // XXX and clusters!
    // ok crop things
    switch (mCropType)
    {
        case CropNone:
        case CropRight:
        {
            nscoord cwidth;
            nscoord twidth = 0;
            int length = mTitle.Length();
            int i;
            for (i = 0; i < length; ++i) {
                PRUnichar ch = mTitle.CharAt(i);
                // still in LTR mode
                cwidth = aRenderingContext.GetWidth(ch);
                if (twidth + cwidth > aWidth)
                    break;

                twidth += cwidth;
#ifdef IBMBIDI
                if (UCS2_CHAR_IS_BIDI(ch) ) {
                  mState |= NS_FRAME_IS_BIDI;
                }
#endif // IBMBIDI
            }

            if (i == 0)
                return titleWidth;

            // insert what character we can in.
            nsAutoString title( mTitle );
            title.Truncate(i);
            mCroppedTitle.Insert(title, 0);
        }
        break;

        case CropLeft:
        {
            nscoord cwidth;
            nscoord twidth = 0;
            int length = mTitle.Length();
            int i;
            for (i=length-1; i >= 0; --i) {
                PRUnichar ch = mTitle.CharAt(i);
                cwidth = aRenderingContext.GetWidth(ch);
                if (twidth + cwidth > aWidth)
                    break;

                twidth += cwidth;
#ifdef IBMBIDI
                if (UCS2_CHAR_IS_BIDI(ch) ) {
                  mState |= NS_FRAME_IS_BIDI;
                }
#endif // IBMBIDI
            }

            if (i == length-1)
                return titleWidth;

            nsAutoString copy;
            mTitle.Right(copy, length-1-i);
            mCroppedTitle += copy;
        }
        break;

        case CropCenter:
        {
            nscoord stringWidth =
                nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                              mTitle.get(), mTitle.Length());
            if (stringWidth <= aWidth) {
                // the entire string will fit in the maximum width
                mCroppedTitle.Insert(mTitle, 0);
                break;
            }

            // determine how much of the string will fit in the max width
            nscoord charWidth = 0;
            nscoord totalWidth = 0;
            PRUnichar ch;
            int leftPos, rightPos;
            nsAutoString leftString, rightString;

            rightPos = mTitle.Length() - 1;
            aRenderingContext.SetTextRunRTL(false);
            for (leftPos = 0; leftPos <= rightPos;) {
                // look at the next character on the left end
                ch = mTitle.CharAt(leftPos);
                charWidth = aRenderingContext.GetWidth(ch);
                totalWidth += charWidth;
                if (totalWidth > aWidth)
                    // greater than the allowable width
                    break;
                leftString.Insert(ch, leftString.Length());

#ifdef IBMBIDI
                if (UCS2_CHAR_IS_BIDI(ch))
                    mState |= NS_FRAME_IS_BIDI;
#endif

                // look at the next character on the right end
                if (rightPos > leftPos) {
                    // haven't looked at this character yet
                    ch = mTitle.CharAt(rightPos);
                    charWidth = aRenderingContext.GetWidth(ch);
                    totalWidth += charWidth;
                    if (totalWidth > aWidth)
                        // greater than the allowable width
                        break;
                    rightString.Insert(ch, 0);

#ifdef IBMBIDI
                    if (UCS2_CHAR_IS_BIDI(ch))
                        mState |= NS_FRAME_IS_BIDI;
#endif
                }

                // look at the next two characters
                leftPos++;
                rightPos--;
            }

            mCroppedTitle = leftString + kEllipsis + rightString;
        }
        break;
    }

    return nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                         mCroppedTitle.get(), mCroppedTitle.Length());
}

#define OLD_ELLIPSIS NS_LITERAL_STRING("...")

// the following block is to append the accesskey to mTitle if there is an accesskey
// but the mTitle doesn't have the character
void
nsTextBoxFrame::UpdateAccessTitle()
{
    /*
     * Note that if you change appending access key label spec,
     * you need to maintain same logic in following methods. See bug 324159.
     * toolkit/content/commonDialog.js (setLabelForNode)
     * toolkit/content/widgets/text.xml (formatAccessKey)
     */
    PRInt32 menuAccessKey;
    nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
    if (!menuAccessKey || mAccessKey.IsEmpty())
        return;

    if (!AlwaysAppendAccessKey() &&
        FindInReadable(mAccessKey, mTitle, nsCaseInsensitiveStringComparator()))
        return;

    nsAutoString accessKeyLabel;
    accessKeyLabel += '(';
    accessKeyLabel += mAccessKey;
    ToUpperCase(accessKeyLabel);
    accessKeyLabel += ')';

    if (mTitle.IsEmpty()) {
        mTitle = accessKeyLabel;
        return;
    }

    const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();
    PRUint32 offset = mTitle.Length();
    if (StringEndsWith(mTitle, kEllipsis)) {
        offset -= kEllipsis.Length();
    } else if (StringEndsWith(mTitle, OLD_ELLIPSIS)) {
        // Try to check with our old ellipsis (for old addons)
        offset -= OLD_ELLIPSIS.Length();
    } else {
        // Try to check with
        // our default ellipsis (for non-localized addons) or ':'
        const PRUnichar kLastChar = mTitle.Last();
        if (kLastChar == PRUnichar(0x2026) || kLastChar == PRUnichar(':'))
            offset--;
    }

    if (InsertSeparatorBeforeAccessKey() &&
        offset > 0 && !NS_IS_SPACE(mTitle[offset - 1])) {
        mTitle.Insert(' ', offset);
        offset++;
    }

    mTitle.Insert(accessKeyLabel, offset);
}

void
nsTextBoxFrame::UpdateAccessIndex()
{
    PRInt32 menuAccessKey;
    nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
    if (menuAccessKey) {
        if (mAccessKey.IsEmpty()) {
            if (mAccessKeyInfo) {
                delete mAccessKeyInfo;
                mAccessKeyInfo = nsnull;
            }
        } else {
            if (!mAccessKeyInfo) {
                mAccessKeyInfo = new nsAccessKeyInfo();
                if (!mAccessKeyInfo)
                    return;
            }

            nsAString::const_iterator start, end;
                
            mCroppedTitle.BeginReading(start);
            mCroppedTitle.EndReading(end);
            
            // remember the beginning of the string
            nsAString::const_iterator originalStart = start;

            bool found;
            if (!AlwaysAppendAccessKey()) {
                // not appending access key - do case-sensitive search
                // first
                found = FindInReadable(mAccessKey, start, end);
                if (!found) {
                    // didn't find it - perform a case-insensitive search
                    start = originalStart;
                    found = FindInReadable(mAccessKey, start, end,
                                           nsCaseInsensitiveStringComparator());
                }
            } else {
                found = RFindInReadable(mAccessKey, start, end,
                                        nsCaseInsensitiveStringComparator());
            }
            
            if (found)
                mAccessKeyInfo->mAccesskeyIndex = Distance(originalStart, start);
            else
                mAccessKeyInfo->mAccesskeyIndex = kNotFound;
        }
    }
}

NS_IMETHODIMP
nsTextBoxFrame::DoLayout(nsBoxLayoutState& aBoxLayoutState)
{
    if (mNeedsReflowCallback) {
        nsIReflowCallback* cb = new nsAsyncAccesskeyUpdate(this);
        if (cb) {
            PresContext()->PresShell()->PostReflowCallback(cb);
        }
        mNeedsReflowCallback = false;
    }

    nsresult rv = nsLeafBoxFrame::DoLayout(aBoxLayoutState);

    CalcDrawRect(*aBoxLayoutState.GetRenderingContext());

    const nsStyleText* textStyle = GetStyleText();
    
    nsRect scrollBounds(nsPoint(0, 0), GetSize());
    nsRect textRect = mTextDrawRect;
    
    nsRefPtr<nsFontMetrics> fontMet;
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));
    nsBoundingMetrics metrics = 
      fontMet->GetInkBoundsForVisualOverflow(mCroppedTitle.get(),
                                             mCroppedTitle.Length(),
                                             aBoxLayoutState.GetRenderingContext());

    textRect.x -= metrics.leftBearing;
    textRect.width = metrics.width;
    // In DrawText() we always draw with the baseline at MaxAscent() (relative to mTextDrawRect), 
    textRect.y += fontMet->MaxAscent() - metrics.ascent;
    textRect.height = metrics.ascent + metrics.descent;

    // Our scrollable overflow is our bounds; our visual overflow may
    // extend beyond that.
    nsRect visualBounds;
    visualBounds.UnionRect(scrollBounds, textRect);
    nsOverflowAreas overflow(visualBounds, scrollBounds);

    if (textStyle->mTextShadow) {
      // text-shadow extends our visual but not scrollable bounds
      nsRect &vis = overflow.VisualOverflow();
      vis.UnionRect(vis, nsLayoutUtils::GetTextShadowRectsUnion(mTextDrawRect, this));
    }
    FinishAndStoreOverflow(overflow, GetSize());

    return rv;
}

nsRect
nsTextBoxFrame::GetComponentAlphaBounds()
{
  return GetVisualOverflowRectRelativeToSelf();
}

bool
nsTextBoxFrame::ComputesOwnOverflowArea()
{
    return true;
}

/* virtual */ void
nsTextBoxFrame::MarkIntrinsicWidthsDirty()
{
    mNeedsRecalc = true;
    nsTextBoxFrameSuper::MarkIntrinsicWidthsDirty();
}

void
nsTextBoxFrame::GetTextSize(nsPresContext* aPresContext,
                            nsRenderingContext& aRenderingContext,
                            const nsString& aString,
                            nsSize& aSize, nscoord& aAscent)
{
    nsRefPtr<nsFontMetrics> fontMet;
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));
    aSize.height = fontMet->MaxHeight();
    aRenderingContext.SetFont(fontMet);
    aSize.width =
      nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                    aString.get(), aString.Length());
    aAscent = fontMet->MaxAscent();
}

void
nsTextBoxFrame::CalcTextSize(nsBoxLayoutState& aBoxLayoutState)
{
    if (mNeedsRecalc)
    {
        nsSize size;
        nsPresContext* presContext = aBoxLayoutState.PresContext();
        nsRenderingContext* rendContext = aBoxLayoutState.GetRenderingContext();
        if (rendContext) {
            GetTextSize(presContext, *rendContext,
                        mTitle, size, mAscent);
            mTextSize = size;
            mNeedsRecalc = false;
        }
    }
}

void
nsTextBoxFrame::CalcDrawRect(nsRenderingContext &aRenderingContext)
{
    nsRect textRect(nsPoint(0, 0), GetSize());
    nsMargin borderPadding;
    GetBorderAndPadding(borderPadding);
    textRect.Deflate(borderPadding);

    // determine (cropped) title and underline position
    nsPresContext* presContext = PresContext();
    // determine (cropped) title which fits in aRect.width and its width
    nscoord titleWidth =
        CalculateTitleForWidth(presContext, aRenderingContext, textRect.width);
    // determine if and at which position to put the underline
    UpdateAccessIndex();

    // make the rect as small as our (cropped) text.
    nscoord outerWidth = textRect.width;
    textRect.width = titleWidth;

    // Align our text within the overall rect by checking our text-align property.
    const nsStyleVisibility* vis = GetStyleVisibility();
    const nsStyleText* textStyle = GetStyleText();

    if (textStyle->mTextAlign == NS_STYLE_TEXT_ALIGN_CENTER)
      textRect.x += (outerWidth - textRect.width)/2;
    else if (textStyle->mTextAlign == NS_STYLE_TEXT_ALIGN_RIGHT ||
             (textStyle->mTextAlign == NS_STYLE_TEXT_ALIGN_DEFAULT &&
              vis->mDirection == NS_STYLE_DIRECTION_RTL) ||
             (textStyle->mTextAlign == NS_STYLE_TEXT_ALIGN_END &&
              vis->mDirection == NS_STYLE_DIRECTION_LTR)) {
      textRect.x += (outerWidth - textRect.width);
    }

    mTextDrawRect = textRect;
}

/**
 * Ok return our dimensions
 */
nsSize
nsTextBoxFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState)
{
    CalcTextSize(aBoxLayoutState);

    nsSize size = mTextSize;
    DISPLAY_PREF_SIZE(this, size);

    AddBorderAndPadding(size);
    bool widthSet, heightSet;
    nsIBox::AddCSSPrefSize(this, size, widthSet, heightSet);

    return size;
}

/**
 * Ok return our dimensions
 */
nsSize
nsTextBoxFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState)
{
    CalcTextSize(aBoxLayoutState);

    nsSize size = mTextSize;
    DISPLAY_MIN_SIZE(this, size);

    // if there is cropping our min width becomes our border and padding
    if (mCropType != CropNone)
        size.width = 0;

    AddBorderAndPadding(size);
    bool widthSet, heightSet;
    nsIBox::AddCSSMinSize(aBoxLayoutState, this, size, widthSet, heightSet);

    return size;
}

nscoord
nsTextBoxFrame::GetBoxAscent(nsBoxLayoutState& aBoxLayoutState)
{
    CalcTextSize(aBoxLayoutState);

    nscoord ascent = mAscent;

    nsMargin m(0,0,0,0);
    GetBorderAndPadding(m);
    ascent += m.top;

    return ascent;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTextBoxFrame::GetFrameName(nsAString& aResult) const
{
    MakeFrameName(NS_LITERAL_STRING("TextBox"), aResult);
    aResult += NS_LITERAL_STRING("[value=") + mTitle + NS_LITERAL_STRING("]");
    return NS_OK;
}
#endif

// If you make changes to this function, check its counterparts 
// in nsBoxFrame and nsXULLabelFrame
nsresult
nsTextBoxFrame::RegUnregAccessKey(bool aDoReg)
{
    // if we have no content, we can't do anything
    if (!mContent)
        return NS_ERROR_FAILURE;

    // check if we have a |control| attribute
    // do this check first because few elements have control attributes, and we
    // can weed out most of the elements quickly.

    // XXXjag a side-effect is that we filter out anonymous <label>s
    // in e.g. <menu>, <menuitem>, <button>. These <label>s inherit
    // |accesskey| and would otherwise register themselves, overwriting
    // the content we really meant to be registered.
    if (!mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::control))
        return NS_OK;

    // see if we even have an access key
    nsAutoString accessKey;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accessKey);

    if (accessKey.IsEmpty())
        return NS_OK;

    // With a valid PresContext we can get the ESM 
    // and (un)register the access key
    nsEventStateManager *esm = PresContext()->EventStateManager();

    PRUint32 key = accessKey.First();
    if (aDoReg)
        esm->RegisterAccessKey(mContent, key);
    else
        esm->UnregisterAccessKey(mContent, key);

    return NS_OK;
}

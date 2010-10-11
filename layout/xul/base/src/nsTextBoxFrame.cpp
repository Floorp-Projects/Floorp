/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Annema <disttsc@bart.nl>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsReadableUtils.h"
#include "nsTextBoxFrame.h"
#include "nsCOMPtr.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
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
#include "nsIEventStateManager.h"
#include "nsITheme.h"
#include "nsUnicharUtils.h"
#include "nsContentUtils.h"
#include "nsDisplayList.h"
#include "nsCSSRendering.h"
#include "nsIReflowCallback.h"
#include "nsBoxFrame.h"

#ifdef IBMBIDI
#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#define CROP_LEFT   "left"
#define CROP_RIGHT  "right"
#define CROP_CENTER "center"
#define CROP_START  "start"
#define CROP_END    "end"

// It's not clear to me whether nsLeafBoxFrame also uses some of the
// nsBoxFrame bits, so use NS_STATE_BOX_CHILD_RESERVED to be safe.
#define NS_STATE_NEED_LAYOUT NS_STATE_BOX_CHILD_RESERVED

class nsAccessKeyInfo
{
public:
    PRInt32 mAccesskeyIndex;
    nscoord mBeforeWidth, mAccessWidth, mAccessUnderlineSize, mAccessOffset;
};


PRBool nsTextBoxFrame::gAlwaysAppendAccessKey          = PR_FALSE;
PRBool nsTextBoxFrame::gAccessKeyPrefInitialized       = PR_FALSE;
PRBool nsTextBoxFrame::gInsertSeparatorBeforeAccessKey = PR_FALSE;
PRBool nsTextBoxFrame::gInsertSeparatorPrefInitialized = PR_FALSE;

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it
//
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
    mState |= NS_STATE_NEED_LAYOUT;
    PRBool aResize;
    PRBool aRedraw;

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
        RegUnregAccessKey(PR_TRUE);

    return NS_OK;
}

nsTextBoxFrame::nsTextBoxFrame(nsIPresShell* aShell, nsStyleContext* aContext):
  nsLeafBoxFrame(aShell, aContext), mAccessKeyInfo(nsnull), mCropType(CropRight),
  mNeedsReflowCallback(PR_FALSE)
{
    mState |= NS_STATE_NEED_LAYOUT;
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

    mState |= NS_STATE_NEED_LAYOUT;
    PRBool aResize;
    PRBool aRedraw;
    UpdateAttributes(nsnull, aResize, aRedraw); /* update all */

    // register access key
    RegUnregAccessKey(PR_TRUE);

    return NS_OK;
}

void
nsTextBoxFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
    // unregister access key
    RegUnregAccessKey(PR_FALSE);
    nsTextBoxFrameSuper::DestroyFrom(aDestructRoot);
}

PRBool
nsTextBoxFrame::AlwaysAppendAccessKey()
{
  if (!gAccessKeyPrefInitialized) 
  {
    gAccessKeyPrefInitialized = PR_TRUE;

    const char* prefName = "intl.menuitems.alwaysappendaccesskeys";
    nsAdoptingString val = nsContentUtils::GetLocalizedStringPref(prefName);
    gAlwaysAppendAccessKey = val.Equals(NS_LITERAL_STRING("true"));
  }
  return gAlwaysAppendAccessKey;
}

PRBool
nsTextBoxFrame::InsertSeparatorBeforeAccessKey()
{
  if (!gInsertSeparatorPrefInitialized)
  {
    gInsertSeparatorPrefInitialized = PR_TRUE;

    const char* prefName = "intl.menuitems.insertseparatorbeforeaccesskeys";
    nsAdoptingString val = nsContentUtils::GetLocalizedStringPref(prefName);
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

    virtual PRBool ReflowFinished()
    {
        PRBool shouldFlush = PR_FALSE;
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

PRBool
nsTextBoxFrame::UpdateAccesskey(nsWeakFrame& aWeakThis)
{
    nsAutoString accesskey;
    nsCOMPtr<nsIDOMXULLabelElement> labelElement = do_QueryInterface(mContent);
    NS_ENSURE_TRUE(aWeakThis.IsAlive(), PR_FALSE);
    if (labelElement) {
        // Accesskey may be stored on control.
        // Because this method is called by the reflow callback, current context
        // may not be the right one. Pushing the context of mContent so that
        // if nsIDOMXULLabelElement is implemented in XBL, we don't get a
        // security exception.
        nsCxPusher cx;
        if (cx.Push(mContent)) {
          labelElement->GetAccessKey(accesskey);
          NS_ENSURE_TRUE(aWeakThis.IsAlive(), PR_FALSE);
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
        return PR_TRUE;
    }
    return PR_FALSE;
}

void
nsTextBoxFrame::UpdateAttributes(nsIAtom*         aAttribute,
                                 PRBool&          aResize,
                                 PRBool&          aRedraw)
{
    PRBool doUpdateTitle = PR_FALSE;
    aResize = PR_FALSE;
    aRedraw = PR_FALSE;

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
            aResize = PR_TRUE;
            mCropType = cropType;
        }
    }

    if (aAttribute == nsnull || aAttribute == nsGkAtoms::value) {
        mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, mTitle);
        doUpdateTitle = PR_TRUE;
    }

    if (aAttribute == nsnull || aAttribute == nsGkAtoms::accesskey) {
        mNeedsReflowCallback = PR_TRUE;
        // Ensure that layout is refreshed and reflow callback called.
        aResize = PR_TRUE;
    }

    if (doUpdateTitle) {
        UpdateAccessTitle();
        aResize = PR_TRUE;
    }

}

class nsDisplayXULTextBox : public nsDisplayItem {
public:
  nsDisplayXULTextBox(nsDisplayListBuilder* aBuilder,
                      nsTextBoxFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayXULTextBox);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULTextBox() {
    MOZ_COUNT_DTOR(nsDisplayXULTextBox);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsIRenderingContext* aCtx);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  NS_DISPLAY_DECL_NAME("XULTextBox", TYPE_XUL_TEXT_BOX)

  virtual PRBool HasText() { return PR_TRUE; }
};

void
nsDisplayXULTextBox::Paint(nsDisplayListBuilder* aBuilder,
                           nsIRenderingContext* aCtx)
{
  static_cast<nsTextBoxFrame*>(mFrame)->
    PaintTitle(*aCtx, mVisibleRect, ToReferenceFrame());
}

nsRect
nsDisplayXULTextBox::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mFrame->GetVisualOverflowRect() + ToReferenceFrame();
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
nsTextBoxFrame::PaintTitle(nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsPoint              aPt)
{
    if (mTitle.IsEmpty())
        return;

    nsRect textRect(CalcTextRect(aRenderingContext, aPt));

    // Paint the text shadow before doing any foreground stuff
    const nsStyleText* textStyle = GetStyleText();
    if (textStyle->mTextShadow) {
      // Text shadow happens with the last value being painted at the back,
      // ie. it is painted first.
      for (PRUint32 i = textStyle->mTextShadow->Length(); i > 0; --i) {
        PaintOneShadow(aRenderingContext.ThebesContext(),
                       textRect,
                       textStyle->mTextShadow->ShadowAt(i - 1),
                       GetStyleColor()->mColor,
                       aDirtyRect);
      }
    }

    DrawText(aRenderingContext, textRect, nsnull);
}

void
nsTextBoxFrame::DrawText(nsIRenderingContext& aRenderingContext,
                         const nsRect&        aTextRect,
                         const nscolor*       aOverrideColor)
{
    nsPresContext* presContext = PresContext();

    // paint the title
    nscolor overColor;
    nscolor underColor;
    nscolor strikeColor;
    nsStyleContext* context = mStyleContext;
  
    PRUint8 decorations = NS_STYLE_TEXT_DECORATION_NONE; // Begin with no decorations
    PRUint8 decorMask = NS_STYLE_TEXT_DECORATION_UNDERLINE | NS_STYLE_TEXT_DECORATION_OVERLINE |
                        NS_STYLE_TEXT_DECORATION_LINE_THROUGH; // A mask of all possible decorations.
    PRBool hasDecorations = context->HasTextDecorations();

    do {  // find decoration colors
      const nsStyleTextReset* styleText = context->GetStyleTextReset();
      
      if (decorMask & styleText->mTextDecoration) {  // a decoration defined here
        nscolor color = aOverrideColor ? *aOverrideColor : context->GetStyleColor()->mColor;
    
        if (NS_STYLE_TEXT_DECORATION_UNDERLINE & decorMask & styleText->mTextDecoration) {
          underColor = color;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_UNDERLINE;
          decorations |= NS_STYLE_TEXT_DECORATION_UNDERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_OVERLINE & decorMask & styleText->mTextDecoration) {
          overColor = color;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_OVERLINE;
          decorations |= NS_STYLE_TEXT_DECORATION_OVERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_LINE_THROUGH & decorMask & styleText->mTextDecoration) {
          strikeColor = color;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
          decorations |= NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
        }
      }
      if (0 != decorMask) {
        context = context->GetParent();
        if (context) {
          hasDecorations = context->HasTextDecorations();
        }
      }
    } while (context && hasDecorations && (0 != decorMask));

    nsCOMPtr<nsIFontMetrics> fontMet;
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));

    nscoord offset;
    nscoord size;
    nscoord ascent;
    fontMet->GetMaxAscent(ascent);

    nscoord baseline =
      presContext->RoundAppUnitsToNearestDevPixels(aTextRect.y + ascent);
    nsRefPtr<gfxContext> ctx = aRenderingContext.ThebesContext();
    gfxPoint pt(presContext->AppUnitsToGfxUnits(aTextRect.x),
                presContext->AppUnitsToGfxUnits(aTextRect.y));
    gfxFloat width = presContext->AppUnitsToGfxUnits(aTextRect.width);
    gfxFloat ascentPixel = presContext->AppUnitsToGfxUnits(ascent);

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
      if (decorations & NS_FONT_DECORATION_UNDERLINE) {
        nsCSSRendering::PaintDecorationLine(ctx, underColor,
                          pt, gfxSize(width, sizePixel),
                          ascentPixel, offsetPixel,
                          NS_STYLE_TEXT_DECORATION_UNDERLINE,
                          nsCSSRendering::DECORATION_STYLE_SOLID);
      }
      if (decorations & NS_FONT_DECORATION_OVERLINE) {
        nsCSSRendering::PaintDecorationLine(ctx, overColor,
                          pt, gfxSize(width, sizePixel),
                          ascentPixel, ascentPixel,
                          NS_STYLE_TEXT_DECORATION_OVERLINE,
                          nsCSSRendering::DECORATION_STYLE_SOLID);
      }
    }

    aRenderingContext.SetFont(fontMet);

    CalculateUnderline(aRenderingContext);

    aRenderingContext.SetColor(aOverrideColor ? *aOverrideColor : GetStyleColor()->mColor);

#ifdef IBMBIDI
    nsresult rv = NS_ERROR_FAILURE;

    if (mState & NS_FRAME_IS_BIDI) {
      presContext->SetBidiEnabled();
      nsBidiPresUtils* bidiUtils = presContext->GetBidiUtils();

      if (bidiUtils) {
        const nsStyleVisibility* vis = GetStyleVisibility();
        nsBidiDirection direction = (NS_STYLE_DIRECTION_RTL == vis->mDirection) ? NSBIDI_RTL : NSBIDI_LTR;
        if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
           // We let the RenderText function calculate the mnemonic's
           // underline position for us.
           nsBidiPositionResolve posResolve;
           posResolve.logicalIndex = mAccessKeyInfo->mAccesskeyIndex;
           rv = bidiUtils->RenderText(mCroppedTitle.get(), mCroppedTitle.Length(), direction,
                                      presContext, aRenderingContext,
                                      aTextRect.x, baseline,
                                      &posResolve,
                                      1);
           mAccessKeyInfo->mBeforeWidth = posResolve.visualLeftTwips;
           mAccessKeyInfo->mAccessWidth = posResolve.visualWidth;
        }
        else
        {
           rv = bidiUtils->RenderText(mCroppedTitle.get(), mCroppedTitle.Length(), direction,
                                      presContext, aRenderingContext,
                                      aTextRect.x, baseline);
        }
      }
    }
    if (NS_FAILED(rv) )
#endif // IBMBIDI
    {
       aRenderingContext.SetTextRunRTL(PR_FALSE);

       if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
           // In the simple (non-BiDi) case, we calculate the mnemonic's
           // underline position by getting the text metric.
           // XXX are attribute values always two byte?
           if (mAccessKeyInfo->mAccesskeyIndex > 0)
               aRenderingContext.GetWidth(mCroppedTitle.get(), mAccessKeyInfo->mAccesskeyIndex,
                                          mAccessKeyInfo->mBeforeWidth);
           else
               mAccessKeyInfo->mBeforeWidth = 0;
       }

       aRenderingContext.DrawString(mCroppedTitle, aTextRect.x, baseline);
    }

    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
        aRenderingContext.FillRect(aTextRect.x + mAccessKeyInfo->mBeforeWidth,
                                   aTextRect.y + mAccessKeyInfo->mAccessOffset,
                                   mAccessKeyInfo->mAccessWidth,
                                   mAccessKeyInfo->mAccessUnderlineSize);
    }

    // Strikeout is drawn on top of the text, per
    // http://www.w3.org/TR/CSS21/zindex.html point 7.2.1.4.1.1.
    if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
      fontMet->GetStrikeout(offset, size);
      gfxFloat offsetPixel = presContext->AppUnitsToGfxUnits(offset);
      gfxFloat sizePixel = presContext->AppUnitsToGfxUnits(size);
      nsCSSRendering::PaintDecorationLine(ctx, strikeColor,
                        pt, gfxSize(width, sizePixel), ascentPixel, offsetPixel,
                        NS_STYLE_TEXT_DECORATION_LINE_THROUGH,
                        nsCSSRendering::DECORATION_STYLE_SOLID);
    }
}

void nsTextBoxFrame::PaintOneShadow(gfxContext*      aCtx,
                                    const nsRect&    aTextRect,
                                    nsCSSShadowItem* aShadowDetails,
                                    const nscolor&   aForegroundColor,
                                    const nsRect&    aDirtyRect) {
  nsPoint shadowOffset(aShadowDetails->mXOffset,
                       aShadowDetails->mYOffset);
  nscoord blurRadius = NS_MAX(aShadowDetails->mRadius, 0);

  nsRect shadowRect(aTextRect);
  shadowRect.MoveBy(shadowOffset);

  nsContextBoxBlur contextBoxBlur;
  gfxContext* shadowContext = contextBoxBlur.Init(shadowRect, 0, blurRadius,
                                                  PresContext()->AppUnitsPerDevPixel(),
                                                  aCtx, aDirtyRect, nsnull);

  if (!shadowContext)
    return;

  nscolor shadowColor;
  if (aShadowDetails->mHasColor)
    shadowColor = aShadowDetails->mColor;
  else
    shadowColor = aForegroundColor;

  // Conjure an nsIRenderingContext from a gfxContext for DrawText
  nsCOMPtr<nsIRenderingContext> renderingContext = nsnull;
  nsIDeviceContext* devCtx = PresContext()->DeviceContext();
  devCtx->CreateRenderingContextInstance(*getter_AddRefs(renderingContext));
  if (!renderingContext)
    return;
  renderingContext->Init(devCtx, shadowContext);

  aCtx->Save();
  aCtx->NewPath();
  aCtx->SetColor(gfxRGBA(shadowColor));

  // Draw the text onto our alpha-only surface to capture the alpha values.
  // Remember that the box blur context has a device offset on it, so we don't need to
  // translate any coordinates to fit on the surface.
  DrawText(*renderingContext, shadowRect, &shadowColor);
  contextBoxBlur.DoPaint();
  aCtx->Restore();
}

void
nsTextBoxFrame::LayoutTitle(nsPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aRect)
{
    // and do caculations if our size changed
    if ((mState & NS_STATE_NEED_LAYOUT)) {

        // determine (cropped) title which fits in aRect.width and its width
        CalculateTitleForWidth(aPresContext, aRenderingContext, aRect.width);

        // determine if and at which position to put the underline
        UpdateAccessIndex();

        // ok layout complete
        mState &= ~NS_STATE_NEED_LAYOUT;
    }
}

void
nsTextBoxFrame::CalculateUnderline(nsIRenderingContext& aRenderingContext)
{
    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
         // Calculate all fields of mAccessKeyInfo which
         // are the same for both BiDi and non-BiDi frames.
         const PRUnichar *titleString = mCroppedTitle.get();
         aRenderingContext.SetTextRunRTL(PR_FALSE);
         aRenderingContext.GetWidth(titleString[mAccessKeyInfo->mAccesskeyIndex],
                                    mAccessKeyInfo->mAccessWidth);

         nscoord offset, baseline;
         nsIFontMetrics *metrics;
         aRenderingContext.GetFontMetrics(metrics);
         metrics->GetUnderline(offset, mAccessKeyInfo->mAccessUnderlineSize);
         metrics->GetMaxAscent(baseline);
         NS_RELEASE(metrics);
         mAccessKeyInfo->mAccessOffset = baseline - offset;
    }
}

void
nsTextBoxFrame::CalculateTitleForWidth(nsPresContext*      aPresContext,
                                       nsIRenderingContext& aRenderingContext,
                                       nscoord              aWidth)
{
    if (mTitle.IsEmpty())
        return;

    nsLayoutUtils::SetFontFromStyle(&aRenderingContext, GetStyleContext());

    // see if the text will completely fit in the width given
    mTitleWidth = nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                                mTitle.get(), mTitle.Length());

    if (mTitleWidth <= aWidth) {
        mCroppedTitle = mTitle;
#ifdef IBMBIDI
        if (HasRTLChars(mTitle)) {
            mState |= NS_FRAME_IS_BIDI;
        }
#endif // IBMBIDI
        return;  // fits, done.
    }

    const nsDependentString& kEllipsis = nsContentUtils::GetLocalizedEllipsis();
    // start with an ellipsis
    mCroppedTitle.Assign(kEllipsis);

    // see if the width is even smaller than the ellipsis
    // if so, clear the text (XXX set as many '.' as we can?).
    aRenderingContext.SetTextRunRTL(PR_FALSE);
    aRenderingContext.GetWidth(kEllipsis, mTitleWidth);

    if (mTitleWidth > aWidth) {
        mCroppedTitle.SetLength(0);
        mTitleWidth = 0;
        return;
    }

    // if the ellipsis fits perfectly, no use in trying to insert
    if (mTitleWidth == aWidth)
        return;

    aWidth -= mTitleWidth;

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
                aRenderingContext.GetWidth(ch,cwidth);
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
                return;

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
                aRenderingContext.GetWidth(ch,cwidth);
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
                return;

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
            aRenderingContext.SetTextRunRTL(PR_FALSE);
            for (leftPos = 0; leftPos <= rightPos;) {
                // look at the next character on the left end
                ch = mTitle.CharAt(leftPos);
                aRenderingContext.GetWidth(ch, charWidth);
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
                    aRenderingContext.GetWidth(ch, charWidth);
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

    mTitleWidth = nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
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

            PRBool found;
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
        mNeedsReflowCallback = PR_FALSE;
    }

    mState |= NS_STATE_NEED_LAYOUT;

    nsresult rv = nsLeafBoxFrame::DoLayout(aBoxLayoutState);

    const nsStyleText* textStyle = GetStyleText();
    if (textStyle->mTextShadow) {
      nsRect bounds(nsPoint(0, 0), GetSize());
      nsOverflowAreas overflow(bounds, bounds);
      // Our scrollable overflow is our bounds; our visual overflow may
      // extend beyond that.
      nsPoint origin(0,0);
      nsRect textRect = CalcTextRect(*aBoxLayoutState.GetRenderingContext(), origin);
      nsRect &vis = overflow.VisualOverflow();
      vis.UnionRect(vis, nsLayoutUtils::GetTextShadowRectsUnion(textRect, this));
      FinishAndStoreOverflow(overflow, GetSize());
    }
    return rv;
}

PRBool
nsTextBoxFrame::ComputesOwnOverflowArea()
{
    return PR_TRUE;
}

/* virtual */ void
nsTextBoxFrame::MarkIntrinsicWidthsDirty()
{
    mNeedsRecalc = PR_TRUE;
    nsTextBoxFrameSuper::MarkIntrinsicWidthsDirty();
}

void
nsTextBoxFrame::GetTextSize(nsPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                                const nsString& aString, nsSize& aSize, nscoord& aAscent)
{
    nsCOMPtr<nsIFontMetrics> fontMet;
    nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fontMet));
    fontMet->GetHeight(aSize.height);
    aRenderingContext.SetFont(fontMet);
    aSize.width =
      nsLayoutUtils::GetStringWidth(this, &aRenderingContext, aString.get(), aString.Length());
    fontMet->GetMaxAscent(aAscent);
}

void
nsTextBoxFrame::CalcTextSize(nsBoxLayoutState& aBoxLayoutState)
{
    if (mNeedsRecalc)
    {
        nsSize size;
        nsPresContext* presContext = aBoxLayoutState.PresContext();
        nsIRenderingContext* rendContext = aBoxLayoutState.GetRenderingContext();
        if (rendContext) {
            GetTextSize(presContext, *rendContext,
                        mTitle, size, mAscent);
            mTextSize = size;
            mNeedsRecalc = PR_FALSE;
        }
    }
}

nsRect
nsTextBoxFrame::CalcTextRect(nsIRenderingContext &aRenderingContext, const nsPoint &aTextOrigin)
{
    nsRect textRect(aTextOrigin, GetSize());
    nsMargin borderPadding;
    GetBorderAndPadding(borderPadding);
    textRect.Deflate(borderPadding);
    // determine (cropped) title and underline position
    nsPresContext* presContext = PresContext();
    LayoutTitle(presContext, aRenderingContext, textRect);

    // make the rect as small as our (cropped) text.
    nscoord outerWidth = textRect.width;
    textRect.width = mTitleWidth;

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
    return textRect;
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
    PRBool widthSet, heightSet;
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
    PRBool widthSet, heightSet;
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
nsTextBoxFrame::RegUnregAccessKey(PRBool aDoReg)
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

    nsresult rv;

    // With a valid PresContext we can get the ESM 
    // and (un)register the access key
    nsIEventStateManager *esm = PresContext()->EventStateManager();

    PRUint32 key = accessKey.First();
    if (aDoReg)
        rv = esm->RegisterAccessKey(mContent, key);
    else
        rv = esm->UnregisterAccessKey(mContent, key);

    return rv;
}

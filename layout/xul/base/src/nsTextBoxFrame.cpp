/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Peter Annema <disttsc@bart.nl>
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsTextBoxFrame.h"
#include "nsCOMPtr.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsBoxLayoutState.h"
#include "nsMenuBarListener.h"

#define ELLIPSIS "..."

#define CROP_LEFT   "left"
#define CROP_RIGHT  "right"
#define CROP_CENTER "center"

#define NS_STATE_NEED_LAYOUT 0x01000000

class nsAccessKeyInfo
{
public:
    PRInt32 mAccesskeyIndex;
    nscoord mBeforeWidth, mAccessWidth, mAccessUnderlineSize, mAccessOffset;
};


//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewTextBoxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
    NS_PRECONDITION(aNewFrame, "null OUT ptr");
    if (nsnull == aNewFrame) {
        return NS_ERROR_NULL_POINTER;
    }
    nsTextBoxFrame* it = new (aPresShell) nsTextBoxFrame (aPresShell);
    if (nsnull == it)
        return NS_ERROR_OUT_OF_MEMORY;

    // it->SetFlags(aFlags);
    *aNewFrame = it;
    return NS_OK;

} // NS_NewTextFrame


NS_IMETHODIMP
nsTextBoxFrame::AttributeChanged(nsIPresContext* aPresContext,
                                 nsIContent*     aChild,
                                 PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aHint)
{
    mState |= NS_STATE_NEED_LAYOUT;
    PRBool aResize;
    PRBool aRedraw;

    UpdateAttributes(aPresContext, aAttribute, aResize, aRedraw);

    if (aResize) {
        nsBoxLayoutState state(aPresContext);
        MarkDirty(state);
    } else if (aRedraw) {
        nsBoxLayoutState state(aPresContext);
        Redraw(state);
    }

    return NS_OK;
}

nsTextBoxFrame::nsTextBoxFrame(nsIPresShell* aShell):nsLeafBoxFrame(aShell), mCropType(CropRight),mAccessKeyInfo(nsnull)
{
    mState |= NS_STATE_NEED_LAYOUT;
    NeedsRecalc();
}

nsTextBoxFrame::~nsTextBoxFrame()
{
    if (mAccessKeyInfo)
        delete mAccessKeyInfo;
}


NS_IMETHODIMP
nsTextBoxFrame::Init(nsIPresContext*  aPresContext,
                     nsIContent*      aContent,
                     nsIFrame*        aParent,
                     nsIStyleContext* aContext,
                     nsIFrame*        aPrevInFlow)
{
    nsresult  rv = nsLeafBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

    mState |= NS_STATE_NEED_LAYOUT;
    PRBool aResize;
    PRBool aRedraw;

    UpdateAttributes(aPresContext, nsnull, aResize, aRedraw); /* update all */

    return rv;
}

void
nsTextBoxFrame::UpdateAttributes(nsIPresContext*  aPresContext,
                                 nsIAtom*         aAttribute,
                                 PRBool&          aResize,
                                 PRBool&          aRedraw)
{
    PRBool doUpdateTitle = PR_FALSE;
    aResize = PR_FALSE;
    aRedraw = PR_FALSE;

    if (aAttribute == nsnull || aAttribute == nsXULAtoms::crop) {
        nsAutoString value;
        mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::crop, value);
        CroppingStyle cropType;

        if (value.EqualsIgnoreCase(CROP_LEFT))
            cropType = CropLeft;
        else if (value.EqualsIgnoreCase(CROP_CENTER))
            cropType = CropCenter;
        else if (value.EqualsIgnoreCase(CROP_RIGHT))
            cropType = CropRight;
        else
            cropType = CropNone;

        if (cropType != mCropType) {
            aResize = PR_TRUE;
            mCropType = cropType;
        }
    }

    if (aAttribute == nsnull || aAttribute == nsHTMLAtoms::value) {
        nsAutoString value;
        mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value);
        if (!value.Equals(mTitle)) {
            mTitle = value;
            doUpdateTitle = PR_TRUE;
        }
    }

    if (aAttribute == nsnull || aAttribute == nsXULAtoms::accesskey) {
        nsAutoString accesskey;
        mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey, accesskey);
        if (!accesskey.Equals(mAccessKey)) {
            if (!doUpdateTitle) {
                // Need to get clean mTitle and didn't already
                nsAutoString value;
                mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value);
                mTitle = value;
                doUpdateTitle = PR_TRUE;
            }
            mAccessKey = accesskey;
        }
    }

    if (doUpdateTitle) {
        UpdateAccessTitle();
        aResize = PR_TRUE;
    }
}

NS_IMETHODIMP
nsTextBoxFrame::Paint(nsIPresContext*      aPresContext,
                      nsIRenderingContext& aRenderingContext,
                      const nsRect&        aDirtyRect,
                      nsFramePaintLayer    aWhichLayer)
{
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);
    if (!disp->IsVisible())
        return NS_OK;

    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {

        // remove the border and padding
        const nsStyleSpacing* spacing = (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
        nsMargin border(0,0,0,0);
        spacing->GetBorderPadding(border);

        nsRect textRect(0,0,mRect.width, mRect.height);
        textRect.Deflate(border);

        PaintTitle(aPresContext, aRenderingContext, aDirtyRect, textRect);
    }

    return nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

NS_IMETHODIMP
nsTextBoxFrame::PaintTitle(nsIPresContext*      aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           const nsRect&        aRect)
{
    if (mTitle.Length() == 0)
        return NS_OK;

    // determine (cropped) title and underline position
    LayoutTitle(aPresContext, aRenderingContext, aRect);

    // make the rect as small as our (cropped) text.
    nsRect textRect(aRect);
    textRect.width = mTitleWidth;

    // don't draw if the title is not dirty
    if (PR_FALSE == aDirtyRect.Intersects(textRect))
        return NS_OK;

    // paint the title
    const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
    aRenderingContext.SetFont(fontStyle->mFont);

    CalculateUnderline(aRenderingContext);

    const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(colorStyle->mColor);

    aRenderingContext.DrawString(mCroppedTitle, textRect.x, textRect.y);

    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
        aRenderingContext.FillRect(textRect.x + mAccessKeyInfo->mBeforeWidth,
                                   textRect.y + mAccessKeyInfo->mAccessOffset,
                                   mAccessKeyInfo->mAccessWidth,
                                   mAccessKeyInfo->mAccessUnderlineSize);
    }

    return NS_OK;
}

void
nsTextBoxFrame::LayoutTitle(nsIPresContext*      aPresContext,
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
         // get all the underline-positioning stuff

         // XXX are attribute values always two byte?
         const PRUnichar *titleString;
         titleString = mCroppedTitle.GetUnicode();
         if (mAccessKeyInfo->mAccesskeyIndex > 0)
             aRenderingContext.GetWidth(titleString, mAccessKeyInfo->mAccesskeyIndex,
                                        mAccessKeyInfo->mBeforeWidth);
         else
             mAccessKeyInfo->mBeforeWidth = 0;

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
nsTextBoxFrame::CalculateTitleForWidth(nsIPresContext*      aPresContext,
                                       nsIRenderingContext& aRenderingContext,
                                       nscoord              aWidth)
{
    if (mTitle.Length() == 0)
        return;

    const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

    nsCOMPtr<nsIDeviceContext> deviceContext;
    aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

    nsCOMPtr<nsIFontMetrics> fontMet;
    deviceContext->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fontMet));
    aRenderingContext.SetFont(fontMet);

    // see if the text will completely fit in the width given
    aRenderingContext.GetWidth(mTitle, mTitleWidth);

    if (mTitleWidth <= aWidth) {
        mCroppedTitle = mTitle;
        return;  // fits, done.
    }

    // start with an ellipsis
    mCroppedTitle.AssignWithConversion(ELLIPSIS);

    // see if the width is even smaller than the ellipsis
    // if so, clear the text (XXX set as many '.' as we can?).
    nscoord ellipsisWidth;
    aRenderingContext.GetWidth(ELLIPSIS, ellipsisWidth);

    if (ellipsisWidth > aWidth) {
        mCroppedTitle.SetLength(0);
        mTitleWidth = aWidth;
        return;
    }

    // if the ellipsis fits perfectly, no use in trying to insert
    if (ellipsisWidth == aWidth) {
        mTitleWidth = aWidth;
        return;
    }

    aWidth -= ellipsisWidth;

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
                aRenderingContext.GetWidth(ch,cwidth);
                if (twidth + cwidth > aWidth)
                    break;

                twidth += cwidth;
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
            }

            if (i == length-1)
                break;

            nsAutoString copy;
            mTitle.Right(copy, length-1-i);
            mCroppedTitle += copy;
        }
        break;

        case CropCenter:
        {
            nsAutoString ellipsisLeft; ellipsisLeft.AssignWithConversion(ELLIPSIS);

            if (ellipsisWidth >= aWidth)
                ellipsisLeft.SetLength(0);
            else
                aWidth -= ellipsisWidth;

            nscoord cwidth;
            nscoord twidth;
            aRenderingContext.GetWidth(mTitle, twidth);

            int length = mTitle.Length();
            int i;
            int i2 = length-1;
            for (i = 0; i < length;) {
                PRUnichar ch;

                ch = mTitle.CharAt(i);
                aRenderingContext.GetWidth(ch,cwidth);
                twidth -= cwidth;
                ++i;

                if (twidth <= aWidth)
                    break;

                ch = mTitle.CharAt(i2);
                aRenderingContext.GetWidth(ch,cwidth);
                twidth -= cwidth;
                --i2;

                if (twidth <= aWidth)
                    break;

            }

            nsAutoString copy;

            if (i2 >= i)
                mTitle.Mid(copy, i, i2+1-i);

            /*
            char cht[100];
            mTitle.ToCString(cht,100);

            char chc[100];
            copy.ToCString(chc,100);

            printf("i=%d, i2=%d, diff=%d, old='%s', new='%s', aWidth=%d\n", i, i2, i2-i, cht,chc, aWidth);
            */

            mCroppedTitle.Insert(ellipsisLeft + copy, 0);
        }
        break;
    }

    aRenderingContext.GetWidth(mCroppedTitle, mTitleWidth);
}

// the following block is to append the accesskey to mTitle if there is an accesskey
// but the mTitle doesn't have the character
void
nsTextBoxFrame::UpdateAccessTitle()
{
    PRInt32 menuAccessKey;
    nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
    if (menuAccessKey) {
        if (!mAccessKey.IsEmpty()) {
            if (mTitle.Find(mAccessKey, PR_TRUE) == kNotFound) {
                nsAutoString tmpstring; tmpstring.AssignWithConversion("(");
                tmpstring += mAccessKey;
                tmpstring.ToUpperCase();
                tmpstring.AppendWithConversion(")");
                PRUint32 offset = mTitle.RFind("...");
                if (offset != kNotFound)
                    mTitle.Insert(tmpstring,offset);
                else
                    mTitle += tmpstring;
            }
        }
    }
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
            if (!mAccessKeyInfo)
                mAccessKeyInfo = new nsAccessKeyInfo();

            mAccessKeyInfo->mAccesskeyIndex = mCroppedTitle.Find(mAccessKey, PR_TRUE);
        }
    }
}

NS_IMETHODIMP
nsTextBoxFrame::DoLayout(nsBoxLayoutState& aBoxLayoutState)
{
    mState |= NS_STATE_NEED_LAYOUT;

    return nsLeafBoxFrame::DoLayout(aBoxLayoutState);
}

NS_IMETHODIMP
nsTextBoxFrame::NeedsRecalc()
{
    mNeedsRecalc = PR_TRUE;
    return NS_OK;
}

void
nsTextBoxFrame::GetTextSize(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
                                const nsString& aString, nsSize& aSize, nscoord& aAscent)
{
    const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

    nsCOMPtr<nsIDeviceContext> deviceContext;
    aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

    nsCOMPtr<nsIFontMetrics> fontMet;
    deviceContext->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fontMet));
    fontMet->GetHeight(aSize.height);
    aRenderingContext.SetFont(fontMet);
    aRenderingContext.GetWidth(aString, aSize.width);
    fontMet->GetMaxAscent(aAscent);
}

void
nsTextBoxFrame::CalcTextSize(nsBoxLayoutState& aBoxLayoutState)
{
    if (mNeedsRecalc)
    {
        nsSize size;
        nsIPresContext* presContext = aBoxLayoutState.GetPresContext();
        const nsHTMLReflowState* rstate = aBoxLayoutState.GetReflowState();
        if (!rstate)
            return;

        nsIRenderingContext* rendContext = rstate->rendContext;

        if (rendContext) {
            GetTextSize(presContext, *rendContext,
                        mTitle, size, mAscent);
            mTextSize = size;
            mNeedsRecalc = PR_FALSE;
        }
    }
}

/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsTextBoxFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
    CalcTextSize(aBoxLayoutState);

    aSize = mTextSize;

    AddBorderAndPadding(aSize);
    AddInset(aSize);
    nsIBox::AddCSSPrefSize(aBoxLayoutState, this, aSize);

    return NS_OK;
}

/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsTextBoxFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
    CalcTextSize(aBoxLayoutState);

    aSize = mTextSize;

     // if there is cropping our min width becomes our border and  padding
    if (mCropType != CropNone) {
       aSize.width = 0;
    }

    AddBorderAndPadding(aSize);
    AddInset(aSize);
    nsIBox::AddCSSMinSize(aBoxLayoutState, this, aSize);

    return NS_OK;
}

NS_IMETHODIMP
nsTextBoxFrame::GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aAscent)
{
    CalcTextSize(aBoxLayoutState);

    aAscent = mAscent;

    nsMargin m(0,0,0,0);
    GetBorderAndPadding(m);
    aAscent += m.top;
    GetInset(m);
    aAscent += m.top;

    return NS_OK;
}

NS_IMETHODIMP
nsTextBoxFrame::GetFrameName(nsString& aResult) const
{
    aResult.AssignWithConversion("Text[value=");
    aResult += mTitle;
    aResult.AppendWithConversion("]");
    return NS_OK;
}

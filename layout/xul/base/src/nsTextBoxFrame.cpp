/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIPref.h"
#include "nsXPIDLString.h"
#include "nsIServiceManager.h"

#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#define ELLIPSIS "..."

#define CROP_LEFT   "left"
#define CROP_RIGHT  "right"
#define CROP_CENTER "center"
#define CROP_START  "start"
#define CROP_END    "end"

#define NS_STATE_NEED_LAYOUT 0x01000000

class nsAccessKeyInfo
{
public:
    PRInt32 mAccesskeyIndex;
    nscoord mBeforeWidth, mAccessWidth, mAccessUnderlineSize, mAccessOffset;
};


PRBool nsTextBoxFrame::gAlwaysAppendAccessKey       = PR_FALSE;
PRBool nsTextBoxFrame::gAccessKeyPrefInitialized    = PR_FALSE;

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
                                 PRInt32         aModType, 
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

PRBool
nsTextBoxFrame::AlwaysAppendAccessKey()
{
  if (!gAccessKeyPrefInitialized) 
  {
    nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
    gAccessKeyPrefInitialized = PR_TRUE;
    if (prefs) 
    {
      nsXPIDLString prefValue;
      nsresult res = prefs->GetLocalizedUnicharPref(
                               "intl.menuitems.alwaysappendacceskeys", 
                                getter_Copies(prefValue));
      if (NS_SUCCEEDED(res)) 
      {
        gAlwaysAppendAccessKey = nsDependentString(prefValue).Equals(
                                             NS_LITERAL_STRING("true"));
      }
    }
  }
  return gAlwaysAppendAccessKey;
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
        mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::crop, value);
        CroppingStyle cropType;

        if (value.EqualsIgnoreCase(CROP_LEFT) || value.EqualsIgnoreCase(CROP_START))
            cropType = CropLeft;
        else if (value.EqualsIgnoreCase(CROP_CENTER))
            cropType = CropCenter;
        else if (value.EqualsIgnoreCase(CROP_RIGHT) || value.EqualsIgnoreCase(CROP_END))
            cropType = CropRight;
        else
            cropType = CropNone;

        if (cropType != mCropType) {
            aResize = PR_TRUE;
            mCropType = cropType;
        }

        if (mCropType == CropLeft || mCropType == CropRight) {
          const nsStyleVisibility* vis = 
            (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
          if (vis->mDirection == NS_STYLE_DIRECTION_RTL) {
            if (mCropType == CropLeft)
              mCropType = CropRight;
            else
              mCropType = CropLeft;
          }
        }
    }

    if (aAttribute == nsnull || aAttribute == nsHTMLAtoms::value) {
        nsAutoString value;
        mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, value);
        if (!value.Equals(mTitle)) {
            mTitle = value;
            doUpdateTitle = PR_TRUE;
        }
    }

    if (aAttribute == nsnull || aAttribute == nsXULAtoms::accesskey) {
        nsAutoString accesskey;
        mContent->GetAttr(kNameSpaceID_None, nsXULAtoms::accesskey, accesskey);
        if (!accesskey.Equals(mAccessKey)) {
            if (!doUpdateTitle) {
                // Need to get clean mTitle and didn't already
                nsAutoString value;
                mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::value, value);
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
                      nsFramePaintLayer    aWhichLayer,
                      PRUint32             aFlags)
{
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    if (!vis->IsVisible())
        return NS_OK;

    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {

        // remove the border and padding
        nsStyleBorderPadding  bPad;
        mStyleContext->GetBorderPaddingFor(bPad);
        nsMargin border(0,0,0,0);
        bPad.GetBorderPadding(border);

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

    // Align our text within the overall rect by checking our text-align property.
    const nsStyleVisibility* vis = (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    const nsStyleText* textStyle = (const nsStyleText*)mStyleContext->GetStyleData(eStyleStruct_Text);

    if (textStyle->mTextAlign == NS_STYLE_TEXT_ALIGN_CENTER)
      textRect.x += (aRect.width - textRect.width)/2;
    else if (textStyle->mTextAlign == NS_STYLE_TEXT_ALIGN_RIGHT) {
      if (vis->mDirection == NS_STYLE_DIRECTION_LTR)
        textRect.x += (aRect.width - textRect.width);
    }
    else {
      if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
        textRect.x += (aRect.width - textRect.width);
    }

    // don't draw if the title is not dirty
    if (PR_FALSE == aDirtyRect.Intersects(textRect))
        return NS_OK;

    // paint the title
    nscolor overColor;
    nscolor underColor;
    nscolor strikeColor;
    nsIStyleContext*  context = mStyleContext;
  
    PRUint8 decorations = NS_STYLE_TEXT_DECORATION_NONE; // Begin with no decorations
    PRUint8 decorMask = NS_STYLE_TEXT_DECORATION_UNDERLINE | NS_STYLE_TEXT_DECORATION_OVERLINE |
                        NS_STYLE_TEXT_DECORATION_LINE_THROUGH; // A mask of all possible decorations.
    PRBool hasDecorations = context->HasTextDecorations();

    NS_ADDREF(context);
    do {  // find decoration colors
      const nsStyleTextReset* styleText = 
        (const nsStyleTextReset*)context->GetStyleData(eStyleStruct_TextReset);
      
      if (decorMask & styleText->mTextDecoration) {  // a decoration defined here
        const nsStyleColor* styleColor =
          (const nsStyleColor*)context->GetStyleData(eStyleStruct_Color);
    
        if (NS_STYLE_TEXT_DECORATION_UNDERLINE & decorMask & styleText->mTextDecoration) {
          underColor = styleColor->mColor;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_UNDERLINE;
          decorations |= NS_STYLE_TEXT_DECORATION_UNDERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_OVERLINE & decorMask & styleText->mTextDecoration) {
          overColor = styleColor->mColor;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_OVERLINE;
          decorations |= NS_STYLE_TEXT_DECORATION_OVERLINE;
        }
        if (NS_STYLE_TEXT_DECORATION_LINE_THROUGH & decorMask & styleText->mTextDecoration) {
          strikeColor = styleColor->mColor;
          decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
          decorations |= NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
        }
      }
      if (0 != decorMask) {
        nsIStyleContext*  lastContext = context;
        context = context->GetParent();
        hasDecorations = context->HasTextDecorations();
        NS_RELEASE(lastContext);
      }
    } while ((nsnull != context) && hasDecorations && (0 != decorMask));
    NS_IF_RELEASE(context);

    nsStyleFont* fontStyle = (nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
    
    nscoord offset;
    nscoord size;
    nscoord baseline;
    if (decorations & (NS_FONT_DECORATION_OVERLINE | NS_FONT_DECORATION_UNDERLINE)) {
      nsCOMPtr<nsIDeviceContext> deviceContext;
      aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

      nsCOMPtr<nsIFontMetrics> fontMet;
      deviceContext->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fontMet));
      fontMet->GetMaxAscent(baseline);
      fontMet->GetUnderline(offset, size);
      if (decorations & NS_FONT_DECORATION_OVERLINE) {
        aRenderingContext.SetColor(overColor);
        aRenderingContext.FillRect(textRect.x, textRect.y, mRect.width, size);
      }
      if (decorations & NS_FONT_DECORATION_UNDERLINE) {
        aRenderingContext.SetColor(underColor);
        aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, mRect.width, size);
      }
    }
    if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
      nsCOMPtr<nsIDeviceContext> deviceContext;
      aPresContext->GetDeviceContext(getter_AddRefs(deviceContext));

      nsCOMPtr<nsIFontMetrics> fontMet;
      deviceContext->GetMetricsFor(fontStyle->mFont, *getter_AddRefs(fontMet));
      fontMet->GetMaxAscent(baseline);
      fontMet->GetStrikeout(offset, size);
      aRenderingContext.SetColor(strikeColor);
      aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, mRect.width, size);
    }
 
    aRenderingContext.SetFont(fontStyle->mFont);

    CalculateUnderline(aRenderingContext);

    const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(colorStyle->mColor);

#ifdef IBMBIDI
    nsresult rv = NS_ERROR_FAILURE;

    if (mState & NS_FRAME_IS_BIDI) {
      nsBidiPresUtils* bidiUtils;
      aPresContext->GetBidiUtils(&bidiUtils);

      if (bidiUtils) {
        nsCOMPtr<nsIBidi> bidiEngine;
        bidiUtils->GetBidiEngine(getter_AddRefs(bidiEngine));
        
        if (bidiEngine) {
          const nsStyleVisibility* vis;
          GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&) vis);
          PRUnichar* buffer = (PRUnichar*) mCroppedTitle.get();
          PRInt32 runCount;
          nsBidiDirection direction =
                                    (NS_STYLE_DIRECTION_RTL == vis->mDirection)
                                    ? NSBIDI_RTL : NSBIDI_LTR;

          rv = bidiEngine->SetPara(buffer, mCroppedTitle.Length(), direction, nsnull);

          if (NS_SUCCEEDED(rv) ) {
            rv = bidiEngine->CountRuns(&runCount);

            if (NS_SUCCEEDED(rv) ) {
              nscoord width;
              PRBool isRTL = PR_FALSE;
              PRInt32 i, start, limit, length;
              nsCharType charType;
              nsBidiLevel level;

              PRBool isBidiSystem;
              PRUint32 hints = 0;
              aRenderingContext.GetHints(hints);
              isBidiSystem = (hints & NS_RENDERING_HINT_BIDI_REORDERING);

              for (i = 0; i < runCount; i++) {
                rv = bidiEngine->GetVisualRun(i, &start, &length, &direction);
                if (NS_FAILED(rv) ) {
                  break;
                }
                bidiEngine->GetCharTypeAt(start, &charType);

                rv = bidiEngine->GetLogicalRun(start, &limit, &level);
                if (NS_FAILED(rv) ) {
                  break;
                }
                if (eCharType_RightToLeftArabic == charType) {
                  isBidiSystem = (hints & NS_RENDERING_HINT_ARABIC_SHAPING);
                }
                if (isBidiSystem && (CHARTYPE_IS_RTL(charType) ^ isRTL) ) {
                  // set reading order into DC
                  isRTL = !isRTL;
                  aRenderingContext.SetRightToLeftText(isRTL);
                }
                bidiUtils->FormatUnicodeText(aPresContext, buffer + start, length,
                                             charType, level & 1,
                                             isBidiSystem);

                aRenderingContext.GetWidth(buffer + start, length, width, nsnull);
                aRenderingContext.DrawString(buffer + start, length, textRect.x,
                                             textRect.y, width);
                textRect.x += width;
              } // for
              // Restore original x (for aRenderingContext.FillRect below),
              // as well as reading order
              textRect.x = aRect.x;
              if (isRTL) {
                aRenderingContext.SetRightToLeftText(PR_FALSE);
              }
            }
          }
        } // bidiEngine
      } // bidiUtils
    } // frame is bidi
    if (NS_FAILED(rv) )
#endif // IBMBIDI
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
         titleString = mCroppedTitle.get();
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
#ifdef IBMBIDI
        PRInt32 length = mTitle.Length();
        for (PRInt32 i = 0; i < length; i++) {
          if (CHAR_IS_BIDI(mTitle.CharAt(i) ) ) {
            mState |= NS_FRAME_IS_BIDI;
            break;
          }
        }
#endif // IBMBIDI
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
#ifdef IBMBIDI
                if (CHAR_IS_BIDI(ch) ) {
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
                if (CHAR_IS_BIDI(ch) ) {
                  mState |= NS_FRAME_IS_BIDI;
                }
#endif // IBMBIDI
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
            nscoord stringWidth = 0;
            aRenderingContext.GetWidth(mTitle, stringWidth);
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
                if (CHAR_IS_BIDI(ch))
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
                    if (CHAR_IS_BIDI(ch))
                        mState |= NS_FRAME_IS_BIDI;
#endif
                }

                // look at the next two characters
                leftPos++;
                rightPos--;
            }

            // form the new cropped string
            nsAutoString ellipsisString;
            ellipsisString.AssignWithConversion(ELLIPSIS);

            mCroppedTitle = leftString + ellipsisString + rightString;
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
            if ((mTitle.Find(mAccessKey, PR_TRUE) == kNotFound) 
                || AlwaysAppendAccessKey()) 
            {
                nsAutoString tmpstring; tmpstring.AssignWithConversion("(");
                tmpstring += mAccessKey;
                tmpstring.ToUpperCase();
                tmpstring.AppendWithConversion(")");
                PRInt32 offset = mTitle.RFind("...");
                if (offset != kNotFound)
                    mTitle.Insert(tmpstring,NS_STATIC_CAST(PRUint32, offset));
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

            if (!AlwaysAppendAccessKey()) {
                // not appending access key - do case-sensitive search first
                mAccessKeyInfo->mAccesskeyIndex = mCroppedTitle.Find(mAccessKey, PR_FALSE);
                if (mAccessKeyInfo->mAccesskeyIndex == kNotFound) {
                    // didn't find it - perform a case-insensitive search
                    mAccessKeyInfo->mAccesskeyIndex = mCroppedTitle.Find(mAccessKey, PR_TRUE);
                }
            } else {
                // use case-insensitive, reverse find for appended access keys
                mAccessKeyInfo->mAccesskeyIndex = mCroppedTitle.RFind(mAccessKey, PR_TRUE);
            }
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

#ifdef DEBUG
NS_IMETHODIMP
nsTextBoxFrame::GetFrameName(nsString& aResult) const
{
    MakeFrameName("Text", aResult);
    aResult += NS_LITERAL_STRING("[value=");
    aResult += mTitle;
    aResult += NS_LITERAL_STRING("]");
    return NS_OK;
}
#endif

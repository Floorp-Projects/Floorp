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

#ifdef IBMBIDI
#include "nsBidiUtils.h"
#include "nsBidiPresUtils.h"
#endif // IBMBIDI
#include "nsReadableUtils.h"

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
} // NS_NewTextFrame


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
        AddStateBits(NS_FRAME_IS_DIRTY);
        PresContext()->PresShell()->
          FrameNeedsReflow(this, nsIPresShell::eStyleChange);
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
  nsLeafBoxFrame(aShell, aContext), mCropType(CropRight),mAccessKeyInfo(nsnull)
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
nsTextBoxFrame::Destroy()
{
    // unregister access key
    RegUnregAccessKey(PR_FALSE);
    nsTextBoxFrameSuper::Destroy();
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
        nsAutoString accesskey;
        nsCOMPtr<nsIDOMXULLabelElement> labelElement = do_QueryInterface(mContent);
        if (labelElement) {
          labelElement->GetAccessKey(accesskey);  // Accesskey may be stored on control
        }
        else {
          mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accesskey, accesskey);
        }
        if (!accesskey.Equals(mAccessKey)) {
            if (!doUpdateTitle) {
                // Need to get clean mTitle and didn't already
                mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value, mTitle);
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

class nsDisplayXULTextBox : public nsDisplayItem {
public:
  nsDisplayXULTextBox(nsTextBoxFrame* aFrame) : nsDisplayItem(aFrame) {
      MOZ_COUNT_CTOR(nsDisplayXULTextBox);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayXULTextBox() {
      MOZ_COUNT_DTOR(nsDisplayXULTextBox);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("XULTextBox")
};

void nsDisplayXULTextBox::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsTextBoxFrame*, mFrame)->
    PaintTitle(*aCtx, aDirtyRect, aBuilder->ToReferenceFrame(mFrame));
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
        nsDisplayXULTextBox(this));
}

void
nsTextBoxFrame::PaintTitle(nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsPoint              aPt)
{
    if (mTitle.IsEmpty())
        return;

    nsRect textRect(aPt, GetSize());
    textRect.Deflate(GetUsedBorderAndPadding());

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
    else if (textStyle->mTextAlign == NS_STYLE_TEXT_ALIGN_RIGHT) {
      if (vis->mDirection == NS_STYLE_DIRECTION_LTR)
        textRect.x += (outerWidth - textRect.width);
    }
    else {
      if (vis->mDirection == NS_STYLE_DIRECTION_RTL)
        textRect.x += (outerWidth - textRect.width);
    }

    // don't draw if the title is not dirty
    if (PR_FALSE == aDirtyRect.Intersects(textRect))
        return;

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
        nscolor color = context->GetStyleColor()->mColor;
    
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

    const nsStyleFont* fontStyle = GetStyleFont();
    
    nscoord offset;
    nscoord size;
    nscoord baseline;
    nsCOMPtr<nsIFontMetrics> fontMet;
    presContext->DeviceContext()->GetMetricsFor(fontStyle->mFont,
                                                *getter_AddRefs(fontMet));
    fontMet->GetMaxAscent(baseline);

    if (decorations & (NS_FONT_DECORATION_OVERLINE | NS_FONT_DECORATION_UNDERLINE)) {
      fontMet->GetUnderline(offset, size);
      if (decorations & NS_FONT_DECORATION_OVERLINE) {
        aRenderingContext.SetColor(overColor);
        aRenderingContext.FillRect(textRect.x, textRect.y, textRect.width, size);
      }
      if (decorations & NS_FONT_DECORATION_UNDERLINE) {
        aRenderingContext.SetColor(underColor);
        aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, textRect.width, size);
      }
    }
    if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
      fontMet->GetStrikeout(offset, size);
      aRenderingContext.SetColor(strikeColor);
      aRenderingContext.FillRect(textRect.x, textRect.y + baseline - offset, textRect.width, size);
    }
 
    aRenderingContext.SetFont(fontStyle->mFont, nsnull);

    CalculateUnderline(aRenderingContext);

    aRenderingContext.SetColor(GetStyleColor()->mColor);

#ifdef IBMBIDI
    nsresult rv = NS_ERROR_FAILURE;

    if (mState & NS_FRAME_IS_BIDI) {
      presContext->SetBidiEnabled(PR_TRUE);
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
                                      textRect.x, textRect.y + baseline,
                                      &posResolve,
                                      1);
           mAccessKeyInfo->mBeforeWidth = posResolve.visualLeftTwips;
        }
        else
        {
           rv = bidiUtils->RenderText(mCroppedTitle.get(), mCroppedTitle.Length(), direction,
                                      presContext, aRenderingContext,
                                      textRect.x, textRect.y + baseline);
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

       aRenderingContext.DrawString(mCroppedTitle, textRect.x, textRect.y + baseline);
    }

    if (mAccessKeyInfo && mAccessKeyInfo->mAccesskeyIndex != kNotFound) {
        aRenderingContext.FillRect(textRect.x + mAccessKeyInfo->mBeforeWidth,
                                   textRect.y + mAccessKeyInfo->mAccessOffset,
                                   mAccessKeyInfo->mAccessWidth,
                                   mAccessKeyInfo->mAccessUnderlineSize);
    }
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

    nsCOMPtr<nsIFontMetrics> fontMet;
    aPresContext->DeviceContext()->GetMetricsFor(GetStyleFont()->mFont,
                                                 *getter_AddRefs(fontMet));
    aRenderingContext.SetFont(fontMet);

    // see if the text will completely fit in the width given
    mTitleWidth = nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                                mTitle.get(), mTitle.Length());

    if (mTitleWidth <= aWidth) {
        mCroppedTitle = mTitle;
#ifdef IBMBIDI
        PRInt32 length = mTitle.Length();
        for (PRInt32 i = 0; i < length; i++) {
          if ((UCS2_CHAR_IS_BIDI(mTitle.CharAt(i)) ) ||
              ((NS_IS_HIGH_SURROGATE(mTitle.CharAt(i))) &&
               (++i < length) &&
               (NS_IS_LOW_SURROGATE(mTitle.CharAt(i))) &&
               (UTF32_CHAR_IS_BIDI(SURROGATE_TO_UCS4(mTitle.CharAt(i-1),
                                                     mTitle.CharAt(i)))))) {
            mState |= NS_FRAME_IS_BIDI;
            break;
          }
        }
#endif // IBMBIDI
        return;  // fits, done.
    }

    // start with an ellipsis
    mCroppedTitle.AssignASCII(ELLIPSIS);

    // see if the width is even smaller than the ellipsis
    // if so, clear the text (XXX set as many '.' as we can?).
    nscoord ellipsisWidth;
    aRenderingContext.SetTextRunRTL(PR_FALSE);
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
                break;

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

            // form the new cropped string
            nsAutoString ellipsisString;
            ellipsisString.AssignASCII(ELLIPSIS);

            mCroppedTitle = leftString + ellipsisString + rightString;
        }
        break;
    }

    mTitleWidth = nsLayoutUtils::GetStringWidth(this, &aRenderingContext,
                                                mCroppedTitle.get(), mCroppedTitle.Length());
}

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
     * xpfe/global/resources/content/commonDialog.js (setLabelForNode)
     * xpfe/global/resources/content/bindings/text.xml (formatAccessKey)
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

    PRInt32 offset = mTitle.RFind("...");
    if (offset == kNotFound) {
        offset = (PRInt32)mTitle.Length();
        if (mTitle.Last() == PRUnichar(':'))
            offset--;
    }

    if (InsertSeparatorBeforeAccessKey() && offset > 0 &&
        !NS_IS_SPACE(mTitle[offset - 1])) {
        mTitle.Insert(' ', (PRUint32)offset);
        offset++;
    }

    mTitle.Insert(accessKeyLabel, (PRUint32)offset);
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
    mState |= NS_STATE_NEED_LAYOUT;

    return nsLeafBoxFrame::DoLayout(aBoxLayoutState);
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
    aPresContext->DeviceContext()->GetMetricsFor(GetStyleFont()->mFont,
                                                 *getter_AddRefs(fontMet));
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
    nsIBox::AddCSSPrefSize(aBoxLayoutState, this, size);

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
    nsIBox::AddCSSMinSize(aBoxLayoutState, this, size);

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
// in nsBoxFrame and nsAreaFrame
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

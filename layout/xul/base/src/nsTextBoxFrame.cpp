/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#define ELIPSIS "..."

#define CROP_LEFT   "left"
#define CROP_RIGHT  "right"
#define CROP_CENTER "center"

#define NS_STATE_NEED_LAYOUT 0x00400000

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
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  mState |= NS_STATE_NEED_LAYOUT;
  PRBool aResize;
  PRBool aRedraw;
  PRBool aUpdateAccessUnderline;

  UpdateAttributes(aPresContext, aAttribute, aResize, aRedraw, aUpdateAccessUnderline);

  if (aUpdateAccessUnderline)
      UpdateAccessUnderline();

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
  
  PRBool a,b,c;
  UpdateAttributes(aPresContext, nsnull, a, b, c  /* all */);

// the following block is to append the accesskey to to mTitle if there is an accesskey 
// but the mTitle doesn't have the character

  PRInt32 menuAccessKey;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (menuAccessKey) {
      nsAutoString accesskey;
      mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey,
                             accesskey);
      if (!accesskey.IsEmpty()) {   
          if (!mAccessKeyInfo)
              mAccessKeyInfo = new nsAccessKeyInfo();

          mAccessKeyInfo->mAccesskeyIndex = -1;
          mAccessKeyInfo->mAccesskeyIndex = mTitle.Find(accesskey, PR_TRUE);
	  if (mAccessKeyInfo->mAccesskeyIndex == -1) {
              nsAutoString tmpstring; tmpstring.AssignWithConversion("(");
              accesskey.ToUpperCase();
              tmpstring += accesskey;
              tmpstring.AppendWithConversion(")");
              PRUint32 offset = mTitle.RFind("...");
              if ( offset != (PRUint32)kNotFound)
                  mTitle.Insert(tmpstring,offset);
              else
                  mTitle += tmpstring;
	  }
      } else {
          if (mAccessKeyInfo) {
              delete mAccessKeyInfo;
              mAccessKeyInfo = nsnull;
          }
      }
  }

  return rv;
}

void
nsTextBoxFrame::UpdateAttributes(nsIPresContext*  aPresContext, nsIAtom* aAttribute, PRBool& aResize, PRBool& aRedraw, PRBool& aUpdateAccessUnderline)
{
    aResize = PR_FALSE;
    aRedraw = PR_FALSE;
    aUpdateAccessUnderline = PR_FALSE;

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
            aUpdateAccessUnderline = PR_TRUE;
        }
    }

    if (aAttribute == nsnull || aAttribute == nsHTMLAtoms::value) {
        nsAutoString value;
        mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value);
        if (!value.Equals(mTitle)) {
            /*
           ListTag(stdout);
           char a[100];
           char b[100];
           mTitle.ToCString(a,100);
           value.ToCString(b,100);

           printf(" value changed '%s'->'%s'\n",a,b);
           */
           aResize = PR_TRUE;
           mTitle = value;
           aUpdateAccessUnderline = PR_TRUE;
        }
    }

    if (aAttribute == nsXULAtoms::accesskey) {
       aRedraw = PR_TRUE;
       aUpdateAccessUnderline = PR_TRUE;
    }

}

NS_IMETHODIMP
nsTextBoxFrame::Paint(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
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

   	    LayoutTitle(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, textRect);  
        PaintTitle(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, textRect);   
    }
    
    return nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}


void
nsTextBoxFrame::LayoutTitle(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer,
                                const nsRect& aRect)
{
    // and do caculations if our size changed
    if (!(mState & NS_STATE_NEED_LAYOUT))
        return;
 
    // get title and center it

    CalculateTitleForWidth(aPresContext, aRenderingContext, aRect.width);
	 
    if (mAccessKeyInfo) 
    {
        if (mAccessKeyInfo->mAccesskeyIndex != -1) {

             // get all the underline-positioning stuff

             // XXX are attribute values always two byte?
             const PRUnichar *titleString;
             titleString = mCroppedTitle.GetUnicode();
             if (mAccessKeyInfo->mAccesskeyIndex)
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

    // ok layout complete
    mState &= ~NS_STATE_NEED_LAYOUT;
}

NS_IMETHODIMP
nsTextBoxFrame::PaintTitle(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer,
                                const nsRect& aRect)
{
 
     if (mTitle.Length() == 0)
         return NS_OK;

     // make the rect as small as our text.
     nsRect textRect(aRect);
     textRect.width = mTitleWidth;

     // don't draw if the title is not dirty
     if (PR_FALSE == aDirtyRect.Intersects(aRect))
           return NS_OK;

	   // paint the title 
	   const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
	   const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

	   aRenderingContext.SetFont(fontStyle->mFont);

       if (mAccessKeyInfo) {
         if (mAccessKeyInfo->mAccesskeyIndex != -1) {
         // get all the underline-positioning stuff

         // XXX are attribute values always two byte?
         const PRUnichar *titleString;
         titleString = mCroppedTitle.GetUnicode();
         if (mAccessKeyInfo->mAccesskeyIndex)
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

     aRenderingContext.SetColor(colorStyle->mColor);
     aRenderingContext.DrawString(mCroppedTitle, textRect.x, textRect.y);

     if (mAccessKeyInfo) {
         if (mAccessKeyInfo->mAccesskeyIndex != -1) {
           aRenderingContext.FillRect(textRect.x + mAccessKeyInfo->mBeforeWidth,
                                      textRect.y + mAccessKeyInfo->mAccessOffset,
                                      mAccessKeyInfo->mAccessWidth, mAccessKeyInfo->mAccessUnderlineSize);
         }
     }
  

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
nsTextBoxFrame::CalculateTitleForWidth(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, nscoord aWidth)
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
  mCroppedTitle = mTitle;

  if ( aWidth >= mTitleWidth)
          return;  // fits done.

   // see if the width is even smaller or equal to the elipsis the
   // text become the elipsis. 
   nscoord elipsisWidth;
   aRenderingContext.GetWidth(ELIPSIS, elipsisWidth);

   mTitleWidth = aWidth;
 
   if (aWidth <= elipsisWidth) {
       mCroppedTitle.SetLength(0);
       return;
   }

   mCroppedTitle.AssignWithConversion(ELIPSIS);

   aWidth -= elipsisWidth;

   // ok crop things
    switch (mCropType)
    {
       case CropNone:
       case CropRight: 
       {
		   nscoord cwidth;
		   nscoord twidth = 0;
           int length = mTitle.Length();
		   int i = 0;
           for (i = 0; i < length; i++)
           {
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
		   int i = length-1;
           for (i=length-1; i >= 0; i--)
           {
              PRUnichar ch = mTitle.CharAt(i);
              aRenderingContext.GetWidth(ch,cwidth);
              if (twidth + cwidth > aWidth) 
                  break;

			  twidth += cwidth;
           }

           if (i == 0) 
               break;
        
           nsAutoString copy;
           mTitle.Right(copy, length-i-1);
           mCroppedTitle += copy;
       } 
       break;

       case CropCenter:

       nsAutoString elipsisLeft; elipsisLeft.AssignWithConversion(ELIPSIS);

       if (aWidth <= elipsisWidth) 
         elipsisLeft.SetLength(0);
       else
          aWidth -= elipsisWidth;
    

	     nscoord cwidth;
		   nscoord twidth = 0;
       aRenderingContext.GetWidth(mTitle, twidth);

       int length = mTitle.Length();
		   int i = 0;
       int i2 = length-1;
           for (i = 0; i < length;)
           {
              PRUnichar ch = mTitle.CharAt(i);
              aRenderingContext.GetWidth(ch,cwidth);
              twidth -= cwidth;
              i++;

              if (twidth <= aWidth) 
                  break;

              ch = mTitle.CharAt(i2);
              aRenderingContext.GetWidth(ch,cwidth);
              i2--;
			        twidth -= cwidth;

              if (twidth <= aWidth) {
                  break;
              }

           }


           nsAutoString copy;

           if (i2 > i)
              mTitle.Mid(copy, i,i2-i);

           /*
           char cht[100];
           mTitle.ToCString(cht,100);

           char chc[100];
           copy.ToCString(chc,100);

           printf("i=%d, i2=%d, diff=%d, old='%s', new='%s', aWidth=%d\n", i, i2, i2-i, cht,chc, aWidth);
           */
           mCroppedTitle.Insert(elipsisLeft + copy, 0);
           break;
    }

    aRenderingContext.GetWidth(mCroppedTitle, mTitleWidth);
}

void
nsTextBoxFrame::UpdateAccessUnderline()
{
  PRInt32 menuAccessKey = -1;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (menuAccessKey) {
    nsAutoString accesskey;
    mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey,
                           accesskey);
    if (accesskey.IsEmpty()) {
        if (mAccessKeyInfo) {
            delete mAccessKeyInfo;
            mAccessKeyInfo = nsnull;
        }
        return;                     // our work here is done
    }

    if (!mAccessKeyInfo)
        mAccessKeyInfo = new nsAccessKeyInfo();

    mAccessKeyInfo->mAccesskeyIndex = -1;
    mAccessKeyInfo->mAccesskeyIndex = mCroppedTitle.Find(accesskey, PR_TRUE);
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
nsTextBoxFrame::CalcTextSize(nsBoxLayoutState& aBoxLayoutState)
{
  if (mNeedsRecalc) 
  {
    nsSize size;
    nsIPresContext* presContext = aBoxLayoutState.GetPresContext();
    const nsHTMLReflowState* rstate =  aBoxLayoutState.GetReflowState();
    if (!rstate)
      return;

    nsIRenderingContext* rendContext =rstate->rendContext;

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

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

#include "nsXULTextFrame.h"
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
NS_NewXULTextFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTextFrame* it = new (aPresShell) nsXULTextFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTextFrame


NS_IMETHODIMP
nsXULTextFrame::AttributeChanged(nsIPresContext* aPresContext,
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
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    mState |= NS_FRAME_IS_DIRTY;
    mParent->ReflowDirtyChild(shell, this);
  } else if (aRedraw) {
      Invalidate(aPresContext, nsRect(0,0,mRect.width, mRect.height), PR_FALSE);
  }

  return NS_OK;
}

nsXULTextFrame::nsXULTextFrame():mTitle(""), mCropType(CropRight),mAccessKeyInfo(nsnull)
{
	mState |= NS_STATE_NEED_LAYOUT;
}

nsXULTextFrame::~nsXULTextFrame()
{
    if (mAccessKeyInfo)
        delete mAccessKeyInfo;
}


NS_IMETHODIMP
nsXULTextFrame::Init(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsXULLeafFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  
  PRBool a,b,c;
  UpdateAttributes(aPresContext, nsnull, a, b, c  /* all */);

// the following block is to append the accesskey to to mTitle if there is an accesskey 
// but the mTitle doesn't have the character 

 
#ifndef XP_UNIX
  nsAutoString accesskey;
  mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey,
                         accesskey);
  if (!accesskey.IsEmpty()) {   
      if (!mAccessKeyInfo)
          mAccessKeyInfo = new nsAccessKeyInfo();

      mAccessKeyInfo->mAccesskeyIndex = -1;
      mAccessKeyInfo->mAccesskeyIndex = mTitle.Find(accesskey, PR_TRUE);
	  if (mAccessKeyInfo->mAccesskeyIndex == -1) {
		  nsString tmpstring = "(" ;
		  accesskey.ToUpperCase();
		  tmpstring += accesskey;
		  tmpstring += ")";
		  PRUint32 offset = mTitle.RFind("...");
		  if ( offset != kNotFound)
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
#endif

  return rv;
}

void
nsXULTextFrame::UpdateAttributes(nsIPresContext*  aPresContext, nsIAtom* aAttribute, PRBool& aResize, PRBool& aRedraw, PRBool& aUpdateAccessUnderline)
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
nsXULTextFrame::Paint(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{	
	const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->IsVisibleOrCollapsed())
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
nsXULTextFrame::LayoutTitle(nsIPresContext* aPresContext,
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
nsXULTextFrame::PaintTitle(nsIPresContext* aPresContext,
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
nsXULTextFrame::GetTextSize(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
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
nsXULTextFrame::CalculateTitleForWidth(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, nscoord aWidth)
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
       mCroppedTitle = "";
       return;
   }

   mCroppedTitle = ELIPSIS;

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
           nsString title = mTitle;
           title.Truncate(i);
		   mCroppedTitle = title + mCroppedTitle;
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
        
           nsString copy = "";
           mTitle.Right(copy, length-i-1);
           mCroppedTitle = mCroppedTitle + copy;
       } 
       break;

       case CropCenter:

       nsString elipsisLeft = ELIPSIS;

       if (aWidth <= elipsisWidth) 
         elipsisLeft = "";
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


           nsString copy = "";

           if (i2 > i)
              mTitle.Mid(copy, i,i2-i);

           /*
           char cht[100];
           mTitle.ToCString(cht,100);

           char chc[100];
           copy.ToCString(chc,100);

           printf("i=%d, i2=%d, diff=%d, old='%s', new='%s', aWidth=%d\n", i, i2, i2-i, cht,chc, aWidth);
           */
           mCroppedTitle = elipsisLeft + copy + mCroppedTitle;
           break;
    }

    aRenderingContext.GetWidth(mCroppedTitle, mTitleWidth);
}

void
nsXULTextFrame::UpdateAccessUnderline()
{
#ifndef XP_UNIX
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
#endif
}



NS_IMETHODIMP
nsXULTextFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  mState |= NS_STATE_NEED_LAYOUT;

  return nsXULLeafFrame::Reflow(aPresContext, aMetrics, aReflowState, aStatus);
}

/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsXULTextFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
  // depending on the type of alignment add in the space for the text
  nsSize size;
  nscoord ascent = 0;
  GetTextSize(aPresContext, *aReflowState.rendContext,
              mTitle, size, ascent);

   aSize.ascent         = ascent;
   aSize.prefSize       = size;
   aSize.minSize        = size;

   // if there is cropping our min width becomes 0
   if (mCropType != CropNone) 
       aSize.minSize.width = 0;
   
   return NS_OK;
}

NS_IMETHODIMP
nsXULTextFrame::GetFrameName(nsString& aResult) const
{
  aResult = "Text[value=";
  aResult += mTitle;
  aResult += "]";
  return NS_OK;
}

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

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsTitledButtonFrame.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsCOMPtr.h"
#include "nsIPresContext.h"
#include "nsButtonFrameRenderer.h"

#include "nsHTMLParts.h"
#include "nsString.h"
#include "nsLeafFrame.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsIImage.h"
#include "nsIWidget.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsIDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsImageMap.h"
#include "nsILinkHandler.h"
#include "nsIURL.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsHTMLContainerFrame.h"
#include "prprf.h"
#include "nsISizeOfHandler.h"
#include "nsIFontMetrics.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDeviceContext.h"
#include "nsINameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsIDOMHTMLMapElement.h"
#include "nsIStyleSet.h"
#include "nsIStyleContext.h"
#include "nsIPopUpMenu.h"

#ifndef _WIN32
#define BROKEN_IMAGE_URL "resource:/res/html/broken-image.gif"
#endif

static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

// Value's for mSuppress
#define SUPPRESS_UNSET   0
#define DONT_SUPPRESS    1
#define SUPPRESS         2
#define DEFAULT_SUPPRESS 3

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)


#define ELIPSIS "..."

#define ALIGN_LEFT   "left"
#define ALIGN_RIGHT  "right"
#define ALIGN_TOP    "top"
#define ALIGN_BOTTOM "bottom"

nsresult
nsTitledButtonFrame::UpdateImageFrame(nsIPresContext* aPresContext,
                                      nsHTMLImageLoader* aLoader,
                                      nsIFrame* aFrame,
                                      void* aClosure,
                                      PRUint32 aStatus)
{
  if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // Now that the size is available, trigger a content-changed reflow
    nsIContent* content = nsnull;
    aFrame->GetContent(&content);
    if (nsnull != content) {
      nsIDocument* document = nsnull;
      content->GetDocument(document);
      if (nsnull != document) {
        document->ContentChanged(content, nsnull);
        NS_RELEASE(document);
      }
      NS_RELEASE(content);
    }
  }
  return NS_OK;
}

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewTitledButtonFrame ( nsIFrame*& aNewFrame )
{
  nsTitledButtonFrame* it = new nsTitledButtonFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  aNewFrame = it;
  return NS_OK;
  
} // NS_NewTitledButtonFrame

NS_IMETHODIMP
nsTitledButtonFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  mNeedsLayout = PR_TRUE;
  UpdateAttributes(*aPresContext);

  mRenderer.Redraw();
  return NS_OK;
}

nsTitledButtonFrame::nsTitledButtonFrame()
{
	mTitle = "";
	mAlign = NS_SIDE_BOTTOM;
	mTruncationType = Right;
	mNeedsLayout = PR_TRUE;
	mHasImage = PR_FALSE;
}

NS_METHOD
nsTitledButtonFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.StopAllLoadImages(&aPresContext);

  return nsLeafFrame::DeleteFrame(aPresContext);
}


NS_IMETHODIMP
nsTitledButtonFrame::Init(nsIPresContext&  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mRenderer.SetNameSpace(kNameSpaceID_None);
  mRenderer.SetFrame(this,aPresContext);
  
  // place 4 pixels of spacing
  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  mSpacing = NSIntPixelsToTwips(4, p2t);

  mHasImage = PR_FALSE;

  // Always set the image loader's base URL, because someone may
  // decide to change a button _without_ an image to have an image
  // later.
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

  // Initialize the image loader. Make sure the source is correct so
  // that UpdateAttributes doesn't double start an image load.
  nsAutoString src;
  GetImageSource(src);
  if (!src.Equals("")) {
    mHasImage = PR_TRUE;
  }
  mImageLoader.Init(this, UpdateImageFrame, nsnull, baseURL, src);
  NS_IF_RELEASE(baseURL);

  UpdateAttributes(aPresContext);

  return rv;
}

void
nsTitledButtonFrame::SetDisabled(nsAutoString aDisabled)
{
  if (aDisabled.EqualsIgnoreCase("true"))
     mRenderer.SetDisabled(PR_TRUE, PR_TRUE);
  else
	 mRenderer.SetDisabled(PR_FALSE, PR_TRUE);
}

void
nsTitledButtonFrame::GetImageSource(nsString& aResult)
{
  // get the new image src
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::src, aResult);

  // if the new image is empty
  if (aResult.Equals("")) {
    // get the list-style-image
    const nsStyleList* myList =
      (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
  
    if (myList->mListStyleImage.Length() > 0) {
      aResult = myList->mListStyleImage;
    }
  }
}

void
nsTitledButtonFrame::UpdateAttributes(nsIPresContext&  aPresContext)
{
	nsAutoString value;
	mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);

  if (value.EqualsIgnoreCase(ALIGN_LEFT))
	  mAlign = NS_SIDE_LEFT;
  else if (value.EqualsIgnoreCase(ALIGN_RIGHT))
	  mAlign = NS_SIDE_RIGHT;
  else if (value.EqualsIgnoreCase(ALIGN_TOP))
	  mAlign = NS_SIDE_TOP;
  else 
	  mAlign = NS_SIDE_BOTTOM;

  value = "";
	mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value);
  mTitle = value;

  // see if the source changed
  // get the old image src
  nsString oldSrc ="";
  mImageLoader.GetURLSpec(oldSrc);

  // get the new image src
  nsAutoString src;
  GetImageSource(src);

   // see if the images are different
  if (PR_FALSE == oldSrc.Equals(src)) {
    if (!src.Equals("")) {
      if (mImageLoader.IsImageSizeKnown()) {
        mImageLoader.UpdateURLSpec(&aPresContext, src);
        PRUint32 loadStatus = mImageLoader.GetLoadStatus();
        if (loadStatus & NS_IMAGE_LOAD_STATUS_IMAGE_READY) {
          // Trigger a paint now because image-loader won't if the
          // image is already loaded and ready to go.
          Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_FALSE);
        }
      }
      else {
        // Force a reflow when the image size isn't already known
        if (nsnull != mContent) {
          nsIDocument* document = nsnull;
          mContent->GetDocument(document);
          if (nsnull != document) {
            document->ContentChanged(mContent, nsnull);
            NS_RELEASE(document);
          }
        }
      }
      mSizeFrozen = PR_FALSE;
      mHasImage = PR_TRUE;
    }
    else {
      mSizeFrozen = PR_TRUE;
      mHasImage = PR_FALSE;
    }
  }
}



NS_IMETHODIMP
nsTitledButtonFrame::Paint(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{	
	
	const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->mVisible)
		return NS_OK;


	nsRect rect (0,0, mRect.width, mRect.height);
	mRenderer.PaintButton(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, rect);
	
   LayoutTitleAndImage(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);  
   
   PaintTitle(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);   
   PaintImage(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);


   /*
   aRenderingContext.SetColor(NS_RGB(0,128,0));
   aRenderingContext.DrawRect(mImageRect);

   aRenderingContext.SetColor(NS_RGB(128,0,0));
   aRenderingContext.DrawRect(mTitleRect);
   */

   return NS_OK;
}


void
nsTitledButtonFrame::LayoutTitleAndImage(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
	// and do caculations if our size changed
	if (mNeedsLayout == PR_FALSE)
		 return;

	 // given our rect try to place the image and text
	 // if they don't fit then truncate the text, the image can't be squeezed.

	 nsRect rect; 
	 mRenderer.GetButtonContentRect(nsRect(0,0,mRect.width,mRect.height), rect);

	 // set up some variables we will use a lot. 
	 nscoord bottom_y = rect.y + rect.height;
	 nscoord top_y    = rect.y;
	 nscoord right_x  = rect.x + rect.width;
	 nscoord left_x   = rect.x;
	 nscoord center_x   = rect.x + rect.width/2;
	 nscoord center_y   = rect.y + rect.height/2;

	 // if we don't have a title
	 if (mTitle.Length() == 0)
	 {
		// have an image
		 if (PR_TRUE == mHasImage) {
			 // just center the image
 			 mImageRect.x = center_x  - mImageRect.width/2;
			 mImageRect.y = center_y  - mImageRect.height/2;
		 } else { // no image
			 //do nothing
		 }
	 } else if (mTitle.Length() > 0) {  // have a title
		 // but no image
		 if (PR_FALSE == mHasImage) {
			 // just center the title
             CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width);
      		 // title top
			 mTitleRect.x = center_x  - mTitleRect.width/2;
			 mTitleRect.y = center_y  - mTitleRect.height/2;
		 } else { // image too?
					 
		// for each type of alignment layout of the image and the text.
			 switch (mAlign) {
			  case NS_SIDE_TOP: {
				  // get title and center it
				  CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width);

				  // title top
				  mTitleRect.x = center_x  - mTitleRect.width/2;
				  mTitleRect.y = top_y;

				  // image bottom center
				  mImageRect.x = center_x - mImageRect.width/2;
				  mImageRect.y = rect.y + (rect.height - mTitleRect.height - mSpacing)/2 - mImageRect.height/2 + mTitleRect.height + mSpacing;
			  }       
			  break;
			  case NS_SIDE_BOTTOM:{
				  // get title and center it
				  CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width);

				  // image top center
				  mImageRect.x = center_x  - mImageRect.width/2;
				  mImageRect.y = rect.y + (rect.height - mTitleRect.height - mSpacing)/2 - mImageRect.height/2 + mSpacing;

				  // title bottom
				  mTitleRect.x = center_x - mTitleRect.width/2;
				  mTitleRect.y = bottom_y - mTitleRect.height;
			  }       
			  break;
			 case NS_SIDE_LEFT: {
				  // get title
				  CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width - (mImageRect.width + mSpacing));

 				  // title left
				  mTitleRect.x = left_x;
				  mTitleRect.y = center_y  - mTitleRect.height/2;

				  // image right center
				  mImageRect.x = (rect.width - mTitleRect.width - mSpacing)/2 - mImageRect.width/2 + mTitleRect.width + rect.x + mSpacing;
				  mImageRect.y = center_y - mImageRect.height/2;
			  }       
			  break;
			  case NS_SIDE_RIGHT: {
				  CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width - (mImageRect.width + mSpacing));

  				  // image left center
				  mImageRect.x = (rect.width - mTitleRect.width - mSpacing)/2 - mImageRect.width/2 +  rect.x;
				  mImageRect.y = center_y  - mImageRect.height/2;

				  // title right
				  mTitleRect.x = right_x - mTitleRect.width;
				  mTitleRect.y = center_y - mTitleRect.height/2;
			   }
			  break;
		   }

		 }
	 }
	 
   // ok layout complete
   mNeedsLayout = PR_FALSE;
}

void 
nsTitledButtonFrame::GetTextSize(nsIPresContext& aPresContext, nsIRenderingContext& aRenderingContext,
                                const nsString& aString, nsSize& aSize)
{
  const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  nsFont font(fontStyle->mFont);
  nsCOMPtr<nsIDeviceContext> deviceContext;
  aPresContext.GetDeviceContext(getter_AddRefs(deviceContext));

  nsCOMPtr<nsIFontMetrics> fontMet;
  deviceContext->GetMetricsFor(font, *getter_AddRefs(fontMet));
  fontMet->GetHeight(aSize.height);
  aRenderingContext.SetFont(fontMet);
  aRenderingContext.GetWidth(aString, aSize.width);
}

void 
nsTitledButtonFrame::CalculateTitleForWidth(nsIPresContext& aPresContext, nsIRenderingContext& aRenderingContext, nscoord aWidth)
{
  const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  nsFont font(fontStyle->mFont);
 
  nsCOMPtr<nsIDeviceContext> deviceContext;
  aPresContext.GetDeviceContext(getter_AddRefs(deviceContext));

  nsCOMPtr<nsIFontMetrics> fontMet;
  deviceContext->GetMetricsFor(font, *getter_AddRefs(fontMet));
  aRenderingContext.SetFont(fontMet);
 
  // see if the text will completely fit in the width given
  aRenderingContext.GetWidth(mTitle, mTitleRect.width);
  fontMet->GetHeight(mTitleRect.height);
  mTruncatedTitle = mTitle;

  if ( aWidth >= mTitleRect.width)
          return;  // fits done.

   // see if the width is even smaller or equal to the elipsis the
   // text become the elipsis. 
   nscoord elipsisWidth;
   aRenderingContext.GetWidth(ELIPSIS, elipsisWidth);

   mTitleRect.width = aWidth;
 
   if (aWidth <= elipsisWidth) {
       mTruncatedTitle = "";
       return;
   }

   mTruncatedTitle = ELIPSIS;

   aWidth -= elipsisWidth;

   // ok truncate things
    switch (mTruncationType)
    {
       case Right: 
       {
		   nscoord cwidth;
		   nscoord twidth = 0;
           int length = mTitle.Length();
		   int i = 0;
           for (i = 0; i < length; i++)
           {
              PRUnichar ch = mTitle[i];
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
		   mTruncatedTitle = title + mTruncatedTitle;
           return;
       } 
       break;
       
       case Left:
       {
		   nscoord cwidth;
		   nscoord twidth = 0;
           int length = mTitle.Length();
		   int i = length-1;
           for (i=length-1; i >= 0; i--)
           {
              PRUnichar ch = mTitle[i];
              aRenderingContext.GetWidth(ch,cwidth);
              if (twidth + cwidth > aWidth) 
                  break;

			  twidth += cwidth;
           }

           if (i == 0) 
               return;
        
           return;       
       } 
       break;

       case Center:
          // TBD
       break;
    }
}

NS_IMETHODIMP
nsTitledButtonFrame::PaintTitle(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
   if (eFramePaintLayer_Content == aWhichLayer) {
 
   	 // place 4 pixels of spacing
		 float p2t;
		 aPresContext.GetScaledPixelsToTwips(&p2t);
		 nscoord pixel = NSIntPixelsToTwips(1, p2t);

     nsRect disabledRect(mTitleRect.x+pixel, mTitleRect.y+pixel, mTitleRect.width, mTitleRect.height);

     // don't draw if the title is not dirty
     if (PR_FALSE == aDirtyRect.Intersects(mTitleRect) && PR_FALSE == aDirtyRect.Intersects(disabledRect))
           return NS_OK;

	   // paint the title 
	   const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
	   const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

	   aRenderingContext.SetFont(fontStyle->mFont);
	   
	   // if disabled paint 
	   if (PR_TRUE == mRenderer.isDisabled())
	   {
		   aRenderingContext.SetColor(NS_RGB(255,255,255));
		   aRenderingContext.DrawString(mTruncatedTitle, disabledRect.x, disabledRect.y);
	   }

	   aRenderingContext.SetColor(colorStyle->mColor);
	   aRenderingContext.DrawString(mTruncatedTitle, mTitleRect.x, mTitleRect.y);

   }

   return NS_OK;
}


NS_IMETHODIMP
nsTitledButtonFrame::PaintImage(nsIPresContext& aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
   if ((0 == mRect.width) || (0 == mRect.height)) {
    // Do not render when given a zero area. This avoids some useless
    // scaling work while we wait for our image dimensions to arrive
    // asynchronously.
    return NS_OK;
  }

  // don't draw if the image is not dirty
  if (PR_FALSE == aDirtyRect.Intersects(mImageRect))
      return NS_OK;


    nsIImage* image = mImageLoader.GetImage();
    if (nsnull == image) {
      // No image yet, or image load failed. Draw the alt-text and an icon
      // indicating the status
      if (eFramePaintLayer_Underlay == aWhichLayer) {
        DisplayAltFeedback(aPresContext, aRenderingContext,
                           mImageLoader.GetLoadImageFailed()
                           ? NS_ICON_BROKEN_IMAGE
                           : NS_ICON_LOADING_IMAGE);
      }
      return NS_OK;
    }

    if (eFramePaintLayer_Content == aWhichLayer) {
      // Now render the image into our content area (the area inside the
      // borders and padding)
      aRenderingContext.DrawImage(image, mImageRect);
    }

   return NS_OK;
}


NS_IMETHODIMP
nsTitledButtonFrame::Reflow(nsIPresContext&   aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  mNeedsLayout = PR_TRUE;
  nsresult result = nsLeafFrame::Reflow(aPresContext, aMetrics, aReflowState, aStatus);
  mRenderer.AddFocusBordersAndPadding(aPresContext, aReflowState, aMetrics, mBorderPadding);
  return result;
}

void
nsTitledButtonFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                    const nsHTMLReflowState& aReflowState,
                                    nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;

	if (mHasImage) {
	  // get the size of the image and set the desired size
	  if (mSizeFrozen) {
			float p2t;
			aPresContext->GetScaledPixelsToTwips(&p2t);
			int v = NSIntPixelsToTwips(30, p2t);

			aDesiredSize.width = v;
			aDesiredSize.height = v;
	  } else {
      // Ask the image loader for the *intrinsic* image size
      mImageLoader.GetDesiredSize(aPresContext, nsnull, aDesiredSize);

      if (aDesiredSize.width == 1 || aDesiredSize.height == 1)
      {
        // Use temporary size of 30x30 pixels until the size arrives
        float p2t;
        aPresContext->GetScaledPixelsToTwips(&p2t);
        int v = NSIntPixelsToTwips(30, p2t);

        aDesiredSize.width = v;
        aDesiredSize.height = v;
      }

	  }
	}

  mMinSize.width = aDesiredSize.width;
  mMinSize.height = aDesiredSize.height;

  // cache the width and height of the image
  mImageRect.width = aDesiredSize.width;
  mImageRect.height = aDesiredSize.height;
  
  // depending on the type of alignment add in the space for the text
  nsSize size;
  GetTextSize(*aPresContext, *aReflowState.rendContext, mTitle, size);
 
   switch (mAlign) {
      case NS_SIDE_TOP:
      case NS_SIDE_BOTTOM:
		  if (size.width > aDesiredSize.width)
			  aDesiredSize.width = size.width;

  		  if (mTitle.Length() > 0) 
             aDesiredSize.height += size.height;
			 
		  if (mTitle.Length() > 0 && mHasImage) 
              aDesiredSize.height += mSpacing;

		  mMinSize.height = aDesiredSize.height;
          break;
     case NS_SIDE_LEFT:
     case NS_SIDE_RIGHT:
		  if (size.height > aDesiredSize.height)
			  aDesiredSize.height = size.height;

   		  if (mTitle.Length() > 0) 
             aDesiredSize.width += size.width;

		  if (mTitle.Length() > 0 && mHasImage)
             aDesiredSize.width += mSpacing;

  		  mMinSize.height = aDesiredSize.height; 

         break;
  
   }

  PRBool fixedWidthContent = aReflowState.HaveFixedContentWidth();
  if (NS_INTRINSICSIZE == aReflowState.computedWidth) {
		fixedWidthContent = PR_FALSE;
  }

  PRBool fixedHeightContent = aReflowState.HaveFixedContentHeight();
  if (NS_INTRINSICSIZE == aReflowState.computedHeight) {
		fixedHeightContent = PR_FALSE;
  }

 
  nsRect minSize(0,0,mMinSize.width, mMinSize.height);
 
  // we need to factory or focus stuff into our computed size.
  nscoord computedWidth = aReflowState.computedWidth;
  nscoord computedHeight = aReflowState.computedHeight;

  
  nsMargin added = mRenderer.GetAddedButtonBorderAndPadding();

  computedWidth -= (added.left + added.right);
  computedHeight -= (added.top + added.bottom);

  // if the width is set
  if (fixedWidthContent)
	  if (computedWidth >= minSize.width)
	      aDesiredSize.width = computedWidth;
	  else
        aDesiredSize.width = minSize.width;
  

  // if the height is set
  if (fixedHeightContent) 
	  if (computedHeight >= minSize.height)
	      aDesiredSize.height = computedHeight;
	  else
        aDesiredSize.height = minSize.height;
}


struct nsTitleRecessedBorder : public nsStyleSpacing {
  nsTitleRecessedBorder(nscoord aBorderWidth)
    : nsStyleSpacing()
  {
    nsStyleCoord  styleCoord(aBorderWidth);

    mBorder.SetLeft(styleCoord);
    mBorder.SetTop(styleCoord);
    mBorder.SetRight(styleCoord);
    mBorder.SetBottom(styleCoord);

    mBorderStyle[0] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[1] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[2] = NS_STYLE_BORDER_STYLE_INSET;  
    mBorderStyle[3] = NS_STYLE_BORDER_STYLE_INSET;  

    mBorderColor[0] = 0;  
    mBorderColor[1] = 0;  
    mBorderColor[2] = 0;  
    mBorderColor[3] = 0;  

    mHasCachedMargin = mHasCachedPadding = mHasCachedBorder = PR_FALSE;
  }
};

void
nsTitledButtonFrame::DisplayAltFeedback(nsIPresContext&      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 PRInt32              aIconId)
{
  // Display a recessed one pixel border in the inner area
  PRBool clipState;
  nsRect  inner = mImageRect;
 
  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nsTitleRecessedBorder recessedBorder(NSIntPixelsToTwips(1, p2t));
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this, inner,
                              inner, recessedBorder, mStyleContext, 0);

  // Adjust the inner rect to account for the one pixel recessed border,
  // and a six pixel padding on each edge
  inner.Deflate(NSIntPixelsToTwips(7, p2t), NSIntPixelsToTwips(7, p2t));
  if (inner.IsEmpty()) {
    return;
  }

  // Clip so we don't render outside the inner rect
  aRenderingContext.PushState();
  aRenderingContext.SetClipRect(inner, nsClipCombine_kIntersect, clipState);

#ifdef _WIN32
  // Display the icon
  nsIDeviceContext* dc;
  aRenderingContext.GetDeviceContext(dc);
  nsIImage*         icon;

  if (NS_SUCCEEDED(dc->LoadIconImage(aIconId, icon))) {
    aRenderingContext.DrawImage(icon, inner.x, inner.y);

    // Reduce the inner rect by the width of the icon, and leave an
    // additional six pixels padding
    PRInt32 iconWidth = NSIntPixelsToTwips(icon->GetWidth() + 6, p2t);
    inner.x += iconWidth;
    inner.width -= iconWidth;

    NS_RELEASE(icon);
  }

  NS_RELEASE(dc);
#endif

  // If there's still room, display the alt-text
  if (!inner.IsEmpty()) {
    nsAutoString altText;
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::alt, altText)) {
      DisplayAltText(aPresContext, aRenderingContext, altText, inner);
    }
  }

  aRenderingContext.PopState(clipState);
}

// Formats the alt-text to fit within the specified rectangle. Breaks lines
// between words if a word would extend past the edge of the rectangle
void
nsTitledButtonFrame::DisplayAltText(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsString&      aAltText,
                             const nsRect&        aRect)
{
  const nsStyleColor* color =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
  const nsStyleFont* font =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  // Set font and color
  aRenderingContext.SetColor(color->mColor);
  aRenderingContext.SetFont(font->mFont);

  // Format the text to display within the formatting rect
  nsIFontMetrics* fm;
  aRenderingContext.GetFontMetrics(fm);

  nscoord maxDescent, height;
  fm->GetMaxDescent(maxDescent);
  fm->GetHeight(height);

  // XXX It would be nice if there was a way to have the font metrics tell
  // use where to break the text given a maximum width. At a minimum we need
  // to be able to get the break character...
  const PRUnichar* str = aAltText.GetUnicode();
  PRInt32          strLen = aAltText.Length();
  nscoord          y = aRect.y;
  while ((strLen > 0) && ((y + maxDescent) < aRect.YMost())) {
    // Determine how much of the text to display on this line
    PRUint32  maxFit;  // number of characters that fit
    MeasureString(str, strLen, aRect.width, maxFit, aRenderingContext);
    
    // Display the text
    aRenderingContext.DrawString(str, maxFit, aRect.x, y);

    // Move to the next line
    str += maxFit;
    strLen -= maxFit;
    y += height;
  }

  NS_RELEASE(fm);
}

// Computes the width of the specified string. aMaxWidth specifies the maximum
// width available. Once this limit is reached no more characters are measured.
// The number of characters that fit within the maximum width are returned in
// aMaxFit. NOTE: it is assumed that the fontmetrics have already been selected
// into the rendering context before this is called (for performance). MMP
void
nsTitledButtonFrame::MeasureString(const PRUnichar*     aString,
                            PRInt32              aLength,
                            nscoord              aMaxWidth,
                            PRUint32&            aMaxFit,
                            nsIRenderingContext& aContext)
{
  nscoord totalWidth = 0;
  nscoord spaceWidth;
  aContext.GetWidth(' ', spaceWidth);

  aMaxFit = 0;
  while (aLength > 0) {
    // Find the next place we can line break
    PRUint32  len = aLength;
    PRBool    trailingSpace = PR_FALSE;
    for (PRInt32 i = 0; i < aLength; i++) {
      if (XP_IS_SPACE(aString[i]) && (i > 0)) {
        len = i;  // don't include the space when measuring
        trailingSpace = PR_TRUE;
        break;
      }
    }
  
    // Measure this chunk of text, and see if it fits
    nscoord width;
    aContext.GetWidth(aString, len, width);
    PRBool  fits = (totalWidth + width) <= aMaxWidth;

    // If it fits on the line, or it's the first word we've processed then
    // include it
    if (fits || (0 == totalWidth)) {
      // New piece fits
      totalWidth += width;

      // If there's a trailing space then see if it fits as well
      if (trailingSpace) {
        if ((totalWidth + spaceWidth) <= aMaxWidth) {
          totalWidth += spaceWidth;
        } else {
          // Space won't fit. Leave it at the end but don't include it in
          // the width
          fits = PR_FALSE;
        }

        len++;
      }

      aMaxFit += len;
      aString += len;
      aLength -= len;
    }

    if (!fits) {
      break;
    }
  }
}

NS_IMETHODIMP
nsTitledButtonFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
  nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  // if disabled do nothing
  if (PR_TRUE == mRenderer.isDisabled()) {
    return NS_OK;
  }

  // let the renderer handle the UI stuff
  mRenderer.HandleEvent(aPresContext, aEvent, aEventStatus);

  aEventStatus = nsEventStatus_eIgnore;
 
  switch (aEvent->message) {

  case NS_MOUSE_ENTER:
    break;
    
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    mRenderer.SetFocus(PR_TRUE, PR_TRUE);
    break;
    
  case NS_MOUSE_LEFT_BUTTON_UP:
    break;

  case NS_MOUSE_EXIT:
    break;

  case NS_MOUSE_MOVE:
    break;
    
  default:
    break;
  }

  //aEventStatus = nsEventStatus_eConsumeNoDefault;

  return NS_OK;
}


//
// ReResolveStyleContext
//
// When the style context changes, make sure that all of our styles are still up to date.
//
NS_IMETHODIMP
nsTitledButtonFrame :: ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext,
                                               PRInt32 aParentChange, nsStyleChangeList* aChangeList,
                                               PRInt32* aLocalChange)
{
  // this re-resolves |mStyleContext|, so it may change
  nsresult rv = nsFrame::ReResolveStyleContext(aPresContext, aParentContext, aParentChange,
                                               aChangeList, aLocalChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_COMFALSE != rv) {
    mRenderer.ReResolveStyles(*aPresContext);
  }
  
  return rv;
  
} // ReResolveStyleContext


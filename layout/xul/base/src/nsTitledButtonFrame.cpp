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

#include "nsButtonFrameRenderer.h"
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
#include "nsBoxLayoutState.h"

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
#include "nsBoxLayoutState.h"
#include "nsMenuBarListener.h"

#include "nsFormControlHelper.h"

#define ONLOAD_CALLED_TOO_EARLY 1

#ifndef _WIN32
#define BROKEN_IMAGE_URL "resource:/res/html/broken-image.gif"
#endif


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

#define CROP_LEFT   "left"
#define CROP_RIGHT  "right"
#define CROP_CENTER "center"

class nsTitledButtonRenderer : public nsButtonFrameRenderer
{
public:
    // change this to do the XML thing. An boolean attribute is only true if it is set to true
    // it is false otherwise
    virtual PRBool isDisabled() 
    {
      nsCOMPtr<nsIContent> content;
      GetFrame()->GetContent(getter_AddRefs(content));
      nsAutoString value;
      if (NS_CONTENT_ATTR_HAS_VALUE == content->GetAttribute(GetNameSpace(), nsHTMLAtoms::disabled, value))
      {
          if (value.EqualsIgnoreCase("true"))
              return PR_TRUE;
      }

      return PR_FALSE;
    }
};

nsresult
nsTitledButtonFrame::UpdateImageFrame(nsIPresContext* aPresContext,
                                      nsHTMLImageLoader* aLoader,
                                      nsIFrame* aFrame,
                                      void* aClosure,
                                      PRUint32 aStatus)
{
  if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    nsTitledButtonFrame* us = (nsTitledButtonFrame*)aFrame;
    nsBoxLayoutState state(aPresContext);
    us->MarkDirty(state);

    /*
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
    */
  }
  return NS_OK;
}

//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewTitledButtonFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTitledButtonFrame* it = new (aPresShell) nsTitledButtonFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTitledButtonFrame

NS_IMETHODIMP
nsTitledButtonFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  mNeedsLayout = PR_TRUE;
  PRBool aResize;
  PRBool aRedraw;
  PRBool aUpdateAccessUnderline;
  UpdateAttributes(aPresContext, aAttribute, aResize, aRedraw, aUpdateAccessUnderline);

  // added back in because boxes now handle only redraw what is reflowed.
  // reflow
  if (aUpdateAccessUnderline)
      UpdateAccessUnderline();

  if (aResize) {
    nsBoxLayoutState state(aPresContext);
    MarkDirty(state);
  } else if (aRedraw) {
    mRenderer->Redraw(aPresContext);
  }

#if !ONLOAD_CALLED_TOO_EARLY
  // onload handlers are called to early, so we have to do this code
  // elsewhere. It really belongs HERE.
    if ( aAttribute == nsHTMLAtoms::value ) {
      CheckState newState = GetCurrentCheckState();
      if ( newState == eMixed ) {
        mHasOnceBeenInMixedState = PR_TRUE;
      }
  }
#endif


  return NS_OK;
}

nsTitledButtonFrame::nsTitledButtonFrame(nsIPresShell* aShell):nsLeafBoxFrame(aShell), 
                                                               mPrefSize(0,0),
                                                               mMinSize(0,0),
                                                               mAscent(0)
{
	mAlign = GetDefaultAlignment();
	mCropType = CropRight;
	mNeedsLayout = PR_TRUE;
  mNeedsAccessUpdate = PR_TRUE;
	mHasImage = PR_FALSE;
  mHasOnceBeenInMixedState = PR_FALSE;
  mRenderer = new nsTitledButtonRenderer();
  NeedsRecalc();
}

nsTitledButtonFrame::~nsTitledButtonFrame()
{
    delete mRenderer;
}


NS_IMETHODIMP
nsTitledButtonFrame::NeedsRecalc()
{
  mNeedsRecalc = PR_TRUE;
  return NS_OK;
}

NS_METHOD
nsTitledButtonFrame::Destroy(nsIPresContext* aPresContext)
{
  // Release image loader first so that it's refcnt can go to zero
  mImageLoader.StopAllLoadImages(aPresContext);

  return nsLeafBoxFrame::Destroy(aPresContext);
}


NS_IMETHODIMP
nsTitledButtonFrame::Init(nsIPresContext*  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsLeafBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mRenderer->SetNameSpace(kNameSpaceID_None);
  mRenderer->SetFrame(this,aPresContext);
  
  // place 4 pixels of spacing
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  mSpacing = NSIntPixelsToTwips(4, p2t);

  mHasImage = PR_FALSE;

  // Always set the image loader's base URL, because someone may
  // decide to change a button _without_ an image to have an image
  // later.
  nsIURI* baseURL = nsnull;
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
  if (!src.IsEmpty()) {
    mHasImage = PR_TRUE;
  }
  mImageLoader.Init(this, UpdateImageFrame, nsnull, baseURL, src);
  NS_IF_RELEASE(baseURL);

  PRBool a,b,c;
  UpdateAttributes(aPresContext, nsnull, a, b, c  /* all */);

  PRInt32 menuAccessKey = -1;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (menuAccessKey) {

// the following block is to append the accesskey to to mTitle if there is an accesskey 
// but the mTitle doesn't have the character 

      mAccesskeyIndex = -1;
      nsAutoString accesskey;
      mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey,
                             accesskey);
      if (!accesskey.IsEmpty()) {    
	  mAccesskeyIndex = mTitle.Find(accesskey, PR_TRUE);
	  if (mAccesskeyIndex == -1) {
              nsString tmpstring; tmpstring.AssignWithConversion("(");
              accesskey.ToUpperCase();
              tmpstring += accesskey;
              tmpstring.AppendWithConversion(")");
              PRUint32 offset = mTitle.RFind("...");
              if ( offset != (PRUint32)kNotFound)
                  mTitle.Insert(tmpstring,offset);
              else
                  mTitle += tmpstring;
	  }
      }
  }

  return rv;
}

void
nsTitledButtonFrame::SetDisabled(nsAutoString aDisabled)
{
  if (aDisabled.EqualsIgnoreCase("true"))
     mRenderer->SetDisabled(PR_TRUE, PR_TRUE);
  else
	 mRenderer->SetDisabled(PR_FALSE, PR_TRUE);
}

void
nsTitledButtonFrame::GetImageSource(nsString& aResult)
{
  // get the new image src
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::src, aResult);

  // if the new image is empty
  if (aResult.IsEmpty()) {
    // get the list-style-image
    const nsStyleList* myList =
      (const nsStyleList*)mStyleContext->GetStyleData(eStyleStruct_List);
  
    if (myList->mListStyleImage.Length() > 0) {
      aResult = myList->mListStyleImage;
    }
  }
}

PRIntn nsTitledButtonFrame::GetDefaultAlignment()
{
  return NS_SIDE_TOP;
}

void
nsTitledButtonFrame::UpdateAttributes(nsIPresContext*  aPresContext, nsIAtom* aAttribute, PRBool& aResize, PRBool& aRedraw, PRBool& aUpdateAccessUnderline)
{
    aResize = PR_FALSE;
    aRedraw = PR_FALSE;
    aUpdateAccessUnderline = PR_FALSE;

    if (aAttribute == nsnull || aAttribute == nsHTMLAtoms::align) {
    PRIntn align;
    nsAutoString value;
	mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, value);

        if (value.EqualsIgnoreCase(ALIGN_LEFT))
            align = NS_SIDE_LEFT;
        else if (value.EqualsIgnoreCase(ALIGN_RIGHT))
            align = NS_SIDE_RIGHT;
        else if (value.EqualsIgnoreCase(ALIGN_BOTTOM))
            align = NS_SIDE_BOTTOM;
        else 
            align = GetDefaultAlignment();

        if (align != mAlign) {
            aRedraw = PR_TRUE;
            mAlign = align;
        }
    }

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

    if (aAttribute == nsnull || aAttribute == nsHTMLAtoms::src) {
        UpdateImage(aPresContext, aResize);
    }

    if (aAttribute == nsXULAtoms::accesskey) {
       aUpdateAccessUnderline = PR_TRUE;
    }

}

void
nsTitledButtonFrame::UpdateImage(nsIPresContext*  aPresContext, PRBool& aResize)
{
  aResize = PR_FALSE;

  // see if the source changed
  // get the old image src
  nsAutoString oldSrc;
  mImageLoader.GetURLSpec(oldSrc);

  // get the new image src
  nsAutoString src;
  GetImageSource(src);

   // see if the images are different
  if (!oldSrc.Equals(src)) {      

        if (!src.IsEmpty()) {
          mSizeFrozen = PR_FALSE;
          mHasImage = PR_TRUE;
        } else {
          mSizeFrozen = PR_TRUE;
          mHasImage = PR_FALSE;
        }

        mImageLoader.UpdateURLSpec(aPresContext, src);  

        aResize = PR_TRUE;
  }
}

#ifdef DEBUG_shaver
#include <execinfo.h>
static PRBool dump_stack = PR_FALSE;
#endif

NS_IMETHODIMP
nsTitledButtonFrame::Paint(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{	
#ifdef DEBUG_shaver
    if (dump_stack) {
        fprintf(stderr, "Paint(%p)\n", this);
        void *bt[15];
        
        int n = backtrace(bt, sizeof bt / sizeof (bt[0]));
        if (n) {
            char **names = backtrace_symbols(bt, n);
            for (int i = 5; i < n; ++i)
                printf("\t\t%s\n", names[i]);
            free(names);
        }
    }
#endif
	const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->IsVisibleOrCollapsed())
		return NS_OK;

   	nsRect rect (0,0, mRect.width, mRect.height);

	mRenderer->PaintButton(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, rect);

        
#if 0
    // Removing this code because it seriously impacts performance of
    // titled buttons without providing any visible benefit. Please
    // talk to hyatt or waterson if you think this code should be put
    // back in. Ideally, we could pessimistically create a clip rect
    // only if LayoutTitleAndImage() determines that the drawing would
    // spill outside of the available rect.
    aRenderingContext.PushState();
    PRBool clipState;
    aRenderingContext.SetClipRect(rect, nsClipCombine_kIntersect, clipState);    
#endif

    LayoutTitleAndImage(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);  
   
    PaintTitle(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);   
    PaintImage(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

#if 0
    aRenderingContext.PopState(clipState);
#endif

   /*
   aRenderingContext.SetColor(NS_RGB(0,128,0));
   aRenderingContext.DrawRect(mImageRect);

   aRenderingContext.SetColor(NS_RGB(128,0,0));
   aRenderingContext.DrawRect(mTitleRect);
   */

   return NS_OK;
}


void
nsTitledButtonFrame::LayoutTitleAndImage(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
    // and do caculations if our size changed
    if (mNeedsLayout == PR_FALSE)
        return;
    // given our rect try to place the image and text
    // if they don't fit then crop the text, the image can't be squeezed.

    nsRect rect; 
    mRenderer->GetButtonContentRect(nsRect(0,0,mRect.width,mRect.height), rect);

	 // set up some variables we will use a lot. 
    nscoord bottom_y = rect.y + rect.height;
    nscoord top_y    = rect.y;
    nscoord right_x  = rect.x + rect.width;
    nscoord left_x   = rect.x;
    nscoord center_x   = rect.x + rect.width/2;
    nscoord center_y   = rect.y + rect.height/2;

    mCroppedTitle.SetLength(0);

    nscoord spacing = 0;

   // if we have a title or an image we need to do spacing
    if (mTitle.Length() > 0 && mHasImage) 
        {
            spacing = mSpacing;
        }

  
    // for each type of alignment layout of the image and the text.
    switch (mAlign) {
    case NS_SIDE_BOTTOM: {
        // get title and center it
        CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width);

        if (mHasImage) {

            // title top
            mTitleRect.x = center_x  - mTitleRect.width/2;
            mTitleRect.y = top_y;

            // image bottom center
            mImageRect.x = center_x - mImageRect.width/2;
            mImageRect.y = rect.y + (rect.height - mTitleRect.height - spacing)/2 - mImageRect.height/2 + mTitleRect.height + spacing;
        } else {
            // title bottom
            mTitleRect.x = center_x - mTitleRect.width/2;
            mTitleRect.y = bottom_y - mTitleRect.height;	  
        }
    }       
    break;
    case NS_SIDE_TOP:{
        // get title and center it
        CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width);

        if (mHasImage) {
            // image top center
            mImageRect.x = center_x  - mImageRect.width/2;
            mImageRect.y = rect.y + (rect.height - mTitleRect.height - spacing)/2 - mImageRect.height/2 + spacing;

            // title bottom
            mTitleRect.x = center_x - mTitleRect.width/2;
            mTitleRect.y = bottom_y - mTitleRect.height;
        } else {
            mTitleRect.x = center_x  - mTitleRect.width/2;
            mTitleRect.y = top_y;
        }
    }       
    break;
    case NS_SIDE_LEFT: {
        // get title
        CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width - (mImageRect.width + spacing));

        // image left
        mImageRect.x = left_x;
        mImageRect.y = center_y  - mImageRect.height/2;

        // text after image
        mTitleRect.x = mImageRect.x + mImageRect.width + spacing;
        mTitleRect.y = center_y - mTitleRect.height/2;
    }       
    break;
    case NS_SIDE_RIGHT: {
        CalculateTitleForWidth(aPresContext, aRenderingContext, rect.width - (mImageRect.width + spacing));

        // image right
        mImageRect.x = right_x - mImageRect.width;
        mImageRect.y = center_y - mImageRect.height/2;

        // text left of image
        mTitleRect.x = mImageRect.x - spacing - mTitleRect.width;
        mTitleRect.y = center_y  - mTitleRect.height/2;

    }
    break;
    }
	 
    // now that we've calculated the title, update the accesskey highlighting
    UpdateAccessUnderline();
    // ok layout complete
    mNeedsLayout = PR_FALSE;
}

void 
nsTitledButtonFrame::GetTextSize(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext,
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
nsTitledButtonFrame::CalculateTitleForWidth(nsIPresContext* aPresContext, nsIRenderingContext& aRenderingContext, nscoord aWidth)
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
  aRenderingContext.GetWidth(mTitle, mTitleRect.width);
  fontMet->GetHeight(mTitleRect.height);
  mCroppedTitle = mTitle;

  if ( aWidth >= mTitleRect.width)
          return;  // fits done.

   // see if the width is even smaller or equal to the elipsis the
   // text become the elipsis. 
   nscoord elipsisWidth;
   aRenderingContext.GetWidth(ELIPSIS, elipsisWidth);

   mTitleRect.width = aWidth;
 
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
        
           nsString copy;
           mTitle.Right(copy, length-i-1);
           mCroppedTitle = mCroppedTitle + copy;
       } 
       break;

       case CropCenter:

       nsString elipsisLeft; elipsisLeft.AssignWithConversion(ELIPSIS);

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


           nsString copy;

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

    aRenderingContext.GetWidth(mCroppedTitle, mTitleRect.width);
}

void
nsTitledButtonFrame::UpdateAccessUnderline()
{
  PRInt32 menuAccessKey = -1;
  nsMenuBarListener::GetMenuAccessKey(&menuAccessKey);
  if (menuAccessKey) {
    nsAutoString accesskey;
    mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::accesskey,
                           accesskey);
    if (accesskey.IsEmpty())
        return;                     // our work here is done
    
    mAccesskeyIndex = mCroppedTitle.Find(accesskey, PR_TRUE);
    mNeedsAccessUpdate = PR_TRUE;
  }
}

NS_IMETHODIMP
nsTitledButtonFrame::PaintTitle(nsIPresContext* aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect& aDirtyRect,
                                nsFramePaintLayer aWhichLayer)
{
   if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
 
     if (mTitle.Length() == 0)
         return NS_OK;

   	 // place 4 pixels of spacing
		 float p2t;
		 aPresContext->GetScaledPixelsToTwips(&p2t);
		 nscoord pixel = NSIntPixelsToTwips(1, p2t);

     nsRect disabledRect(mTitleRect.x+pixel, mTitleRect.y+pixel, mTitleRect.width, mTitleRect.height);

     // don't draw if the title is not dirty
     if (PR_FALSE == aDirtyRect.Intersects(mTitleRect) && PR_FALSE == aDirtyRect.Intersects(disabledRect))
           return NS_OK;

	   // paint the title 
	   const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
	   const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

	   aRenderingContext.SetFont(fontStyle->mFont);

     if (mAccesskeyIndex != -1 && mNeedsAccessUpdate) {
         // get all the underline-positioning stuff

         // XXX are attribute values always two byte?
         const PRUnichar *titleString;
         titleString = mCroppedTitle.GetUnicode();
         if (mAccesskeyIndex)
             aRenderingContext.GetWidth(titleString, mAccesskeyIndex,
                                        mBeforeWidth);
         else
             mBeforeWidth = 0;
         
         aRenderingContext.GetWidth(titleString[mAccesskeyIndex],
                                    mAccessWidth);

         nscoord offset, baseline;
         nsIFontMetrics *metrics;
         aRenderingContext.GetFontMetrics(metrics);
         metrics->GetUnderline(offset, mAccessUnderlineSize);
         metrics->GetMaxAscent(baseline);
         NS_RELEASE(metrics);
         mAccessOffset = baseline - offset;
         mNeedsAccessUpdate = PR_FALSE;
     }

     // this should not be hardcoded. CSS should decide what the text looks like.
     /*
	   // if disabled paint 
	   if (PR_TRUE == mRenderer->isDisabled())
	   {
		   aRenderingContext.SetColor(NS_RGB(255,255,255));
		   aRenderingContext.DrawString(mCroppedTitle, disabledRect.x, 
                                    disabledRect.y);
       if (mAccesskeyIndex != -1)
         aRenderingContext.FillRect(disabledRect.x + mBeforeWidth,
                                    disabledRect.y + mAccessOffset,
                                    mAccessWidth, mAccessUnderlineSize);
	   }
       */

	   aRenderingContext.SetColor(colorStyle->mColor);
	   aRenderingContext.DrawString(mCroppedTitle, mTitleRect.x, mTitleRect.y);

     if (mAccesskeyIndex != -1)
       aRenderingContext.FillRect(mTitleRect.x + mBeforeWidth,
                                  mTitleRect.y + mAccessOffset,
                                  mAccessWidth, mAccessUnderlineSize);

   }

   return NS_OK;
}


NS_IMETHODIMP
nsTitledButtonFrame::PaintImage(nsIPresContext* aPresContext,
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
  if (!mHasImage || !aDirtyRect.Intersects(mImageRect))
    return NS_OK;

  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer)
    return NS_OK;

  nsCOMPtr<nsIImage> image ( dont_AddRef(mImageLoader.GetImage()) );
  if ( !image ) {
    // No image yet, or image load failed. Draw the alt-text and an icon
    // indicating the status
    if (eFramePaintLayer_Underlay == aWhichLayer) {
      DisplayAltFeedback(aPresContext, aRenderingContext,
                       mImageLoader.GetLoadImageFailed()
                       ? NS_ICON_BROKEN_IMAGE
                       : NS_ICON_LOADING_IMAGE);
    }
  }
  else {
    // Now render the image into our content area (the area inside the
    // borders and padding)
    aRenderingContext.DrawImage(image, mImageRect);
  }
  
  return NS_OK;
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
nsTitledButtonFrame::DisplayAltFeedback(nsIPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 PRInt32              aIconId)
{
  // Display a recessed one pixel border in the inner area
  PRBool clipState;
  nsRect  inner = mImageRect;
 
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
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
     //XXX: #bug 6934 check for null icon as a patch
     //A question remains, why does LoadIconImage above, sometimes
     //return a null icon.
    if (nsnull != icon) {
      aRenderingContext.DrawImage(icon, inner.x, inner.y);

      // Reduce the inner rect by the width of the icon, and leave an
      // additional six pixels padding
      PRInt32 iconWidth = NSIntPixelsToTwips(icon->GetWidth() + 6, p2t);
      inner.x += iconWidth;
      inner.width -= iconWidth;

      NS_RELEASE(icon);
    }
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
nsTitledButtonFrame::DisplayAltText(nsIPresContext*      aPresContext,
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
nsTitledButtonFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus* aEventStatus)
{
  // if disabled do nothing
  if (PR_TRUE == mRenderer->isDisabled()) {
    return NS_OK;
  }

    switch (aEvent->message) {
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
          MouseClicked(aPresContext);
        }
      }
      break;

    case NS_MOUSE_LEFT_CLICK:
      MouseClicked(aPresContext);
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
      // if we are not toggling then do nothing
      CheckState oldState = GetCurrentCheckState();
      if (oldState == eUnset)
        break;

      CheckState newState = eOn;
      switch ( oldState ) {
        case eOn:
          newState = eOff;
          break;
      
        case eMixed:
          newState = eOn;
          break;
    
        case eOff:
          newState = mHasOnceBeenInMixedState ? eMixed: eOn;
          break;

        case eUnset:
          newState = eOn;
          break;
      }
      SetCurrentCheckState(newState);
      break;
  }

  return nsLeafBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

//
// GetCurrentCheckState
//
// Looks in the DOM to find out what the value is. 0 is off, 1 is on, 2 is mixed.
// This will default to "off" if no value is set in the DOM.
//
nsTitledButtonFrame::CheckState
nsTitledButtonFrame::GetCurrentCheckState()
{
  nsString value;
  CheckState outState = eOff;
  nsresult res = mContent->GetAttribute ( kNameSpaceID_None, nsXULAtoms::toggled, value );

  // if not se do not do toggling
  if ( res != NS_CONTENT_ATTR_HAS_VALUE )
    return eUnset;

  outState = StringToCheckState(value);  
   
#if ONLOAD_CALLED_TOO_EARLY
// this code really belongs in AttributeChanged, but is needed here because
// setting the value in onload doesn't trip the AttributeChanged method on the frame
  if ( outState == eMixed )
    mHasOnceBeenInMixedState = PR_TRUE;
#endif

  return outState;
} // GetCurrentCheckState

//
// SetCurrentCheckState
//
// Sets the value in the DOM. 0 is off, 1 is on, 2 is mixed.
//
void 
nsTitledButtonFrame::SetCurrentCheckState(CheckState aState)
{
  nsString valueAsString;
  CheckStateToString ( aState, valueAsString );
  mContent->SetAttribute(kNameSpaceID_None, nsXULAtoms::toggled, valueAsString, PR_TRUE);
} // SetCurrentCheckState

//
// MouseClicked
//
// handle when the mouse is clicked in the box. If the check is on or off, toggle it.
// If the state is mixed, then set it to off. You can't ever get back to mixed.
//
void 
nsTitledButtonFrame::MouseClicked (nsIPresContext* aPresContext) 
{
  // Execute the oncommand event handler.
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_MENU_ACTION;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    shell->HandleDOMEventWithTarget(mContent, &event, &status);
  }
}

//
// StringToCheckState
//
// Converts from a string to a CheckState enum
//
nsTitledButtonFrame::CheckState
nsTitledButtonFrame :: StringToCheckState ( const nsString & aStateAsString )
{
  if ( aStateAsString.EqualsWithConversion(NS_STRING_TRUE) )
    return eOn;
  else if ( aStateAsString.EqualsWithConversion(NS_STRING_FALSE) )
    return eOff;
  
  // not true and not false means mixed
  return eMixed;
  
} // StringToCheckState

//
// CheckStateToString
//
// Converts from a CheckState to a string
//
void
nsTitledButtonFrame :: CheckStateToString ( CheckState inState, nsString& outStateAsString )
{
  switch ( inState ) {
    case eOn:
      outStateAsString.Assign(NS_STRING_TRUE);
	  break;

    case eOff:
      outStateAsString.Assign(NS_STRING_FALSE);
      break;
 
    case eMixed:
      outStateAsString.AssignWithConversion("2");
      break;

    case eUnset:
      outStateAsString.SetLength(0);
  }
} // CheckStateToString


NS_IMETHODIMP
nsTitledButtonFrame::GetAdditionalStyleContext(PRInt32 aIndex, 
                                               nsIStyleContext** aStyleContext) const
{
  return mRenderer->GetStyleContext(aIndex, aStyleContext);
}

NS_IMETHODIMP
nsTitledButtonFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                               nsIStyleContext* aStyleContext)
{
  return mRenderer->SetStyleContext(aIndex, aStyleContext);
}

//
// DidSetStyleContext
//
// When the style context changes, make sure that all of our image is up to date.
//
NS_IMETHODIMP
nsTitledButtonFrame :: DidSetStyleContext( nsIPresContext* aPresContext )
{
  // if list-style-image change we want to change the image
  PRBool aResize;
  UpdateImage(aPresContext, aResize);
  
  return NS_OK;
  
} // DidSetStyleContext

void
nsTitledButtonFrame::GetImageSize(nsIPresContext* aPresContext)
{
  nsSize s(0,0);
  nsHTMLReflowMetrics desiredSize(&s);
  const PRInt32 kDefaultSize = 10;
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  const PRInt32 kDefaultSizeInTwips = NSIntPixelsToTwips(kDefaultSize, p2t);

// not calculated? Get the intrinsic size
	if (mHasImage) {
	  // get the size of the image and set the desired size
	  if (mSizeFrozen) {
			mImageRect.width = kDefaultSizeInTwips;
			mImageRect.height = kDefaultSizeInTwips;
      return;
	  } else {
      // Ask the image loader for the *intrinsic* image size
      mImageLoader.GetDesiredSize(aPresContext, nsnull, desiredSize);

      if (desiredSize.width == 1 || desiredSize.height == 1)
      {
        mImageRect.width = kDefaultSizeInTwips;
        mImageRect.height = kDefaultSizeInTwips;
        return;
      }
	  }
	}

  mImageRect.width = desiredSize.width;
  mImageRect.height = desiredSize.height;


}


/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsTitledButtonFrame::DoLayout(nsBoxLayoutState& aState)
{
  mNeedsLayout = PR_TRUE;
  return nsLeafBoxFrame::DoLayout(aState);
}

/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsTitledButtonFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (mNeedsRecalc) {
     CacheSizes(aBoxLayoutState);
  }

  aSize = mPrefSize;
  return NS_OK;
}

/**
 * Ok return our dimensions
 */
NS_IMETHODIMP
nsTitledButtonFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  if (mNeedsRecalc) {
     CacheSizes(aBoxLayoutState);
  }

  aSize = mMinSize;
  return NS_OK;
}

NS_IMETHODIMP
nsTitledButtonFrame::GetAscent(nsBoxLayoutState& aBoxLayoutState, nscoord& aCoord)
{
  if (mNeedsRecalc) {
     CacheSizes(aBoxLayoutState);
  }

  aCoord = mAscent;
  return NS_OK;
}

/**
 * Ok return our dimensions
 */
void
nsTitledButtonFrame::CacheSizes(nsBoxLayoutState& aBoxLayoutState)
{
  nsIPresContext* presContext = aBoxLayoutState.GetPresContext();
  const nsHTMLReflowState* state = aBoxLayoutState.GetReflowState();
  if (!state)
    return;

  nsIRenderingContext* rendContext = state->rendContext;

  GetImageSize(presContext);

  mMinSize.width = mImageRect.width;
  mMinSize.height = mImageRect.height;
  mPrefSize.width = mImageRect.width;
  mPrefSize.height = mImageRect.height;

  // depending on the type of alignment add in the space for the text
  nsSize size;
  nscoord ascent = 0;

  GetTextSize(presContext, *rendContext,
              mTitle, size, ascent);

  mAscent = ascent;
 
   switch (mAlign) {
      case NS_SIDE_TOP:
      case NS_SIDE_BOTTOM:

        // if we have an image add the image to our min size
        if (mHasImage)
           mMinSize.width = mPrefSize.width;

        // if we are not cropped then our min size also includes the text.
        if (mCropType == CropNone) 
           mMinSize.width = PR_MAX(size.width, mMinSize.width);

		  if (size.width > mPrefSize.width)
			  mPrefSize.width = size.width;

  		  if (mTitle.Length() > 0) {
             mPrefSize.height += size.height;
  		       if (mHasImage) 
                mPrefSize.height += mSpacing;
        }

        mMinSize.height = mPrefSize.height;

        break;
     case NS_SIDE_LEFT:
     case NS_SIDE_RIGHT:
		  if (size.height > mPrefSize.height)
			  mPrefSize.height = size.height;

      if (mHasImage)
         mMinSize.width = mPrefSize.width;
      
      // if we are not cropped then our min size also includes the text.
      if (mCropType == CropNone) 
         mMinSize.width += size.width;

      if (mTitle.Length() > 0) {
         mPrefSize.width += size.width;
      
         if (mHasImage) {
            mPrefSize.width += mSpacing;
            mMinSize.width += mSpacing;
         }
      }

      break;
   }

   //nscoord oldHeight = mPrefSize.height;

   nsMargin focusBorder = mRenderer->GetAddedButtonBorderAndPadding();

   mPrefSize.width += focusBorder.left + focusBorder.right;
   mPrefSize.height += focusBorder.top + focusBorder.bottom;

   mMinSize.width += focusBorder.left + focusBorder.right;
   mMinSize.height += focusBorder.top + focusBorder.bottom;


  AddBorderAndPadding(mPrefSize);
  AddInset(mPrefSize);
  nsIBox::AddCSSPrefSize(aBoxLayoutState, this, mPrefSize);

  AddBorderAndPadding(mMinSize);
  AddInset(mMinSize);
  nsIBox::AddCSSMinSize(aBoxLayoutState, this, mMinSize);

  if (mPrefSize.width < mMinSize.width)
     mPrefSize.width = mMinSize.width;

  if (mPrefSize.height < mMinSize.height)
     mPrefSize.height = mMinSize.height;

  nsMargin m(0,0,0,0);
  GetBorderAndPadding(m);
  mAscent += m.top;
  GetInset(m);
  mAscent += m.top;
  mAscent += focusBorder.top;

  mNeedsRecalc = PR_FALSE;
}

NS_IMETHODIMP
nsTitledButtonFrame::GetFrameName(nsString& aResult) const
{
  aResult.AssignWithConversion("TitledButton[value=");
  aResult += mTitle;
  aResult.AppendWithConversion("]");
  return NS_OK;
}

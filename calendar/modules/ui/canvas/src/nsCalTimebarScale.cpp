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

#include "nsCalTimebarScale.h"
#include "nsCalUICIID.h"

#include "nspr.h"
#include "nsCRT.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsBoxLayout.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimebarScaleCID, NS_CAL_TIMEBARSCALE_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);

#define DEFAULT_WIDTH  39
#define DEFAULT_HEIGHT 50

#define INSET 2

nsCalTimebarScale :: nsCalTimebarScale(nsISupports* outer) : nsCalTimebarCanvas(outer)
{
  NS_INIT_REFCNT();
  SetNameID(nsString("TimebarScale"));
  
  /*
   *  This is a bit of a hack until we figure out where
   *  this preference should go. We want to set the background
   *  and foreground colors of certain classes of widgets...
   */
  SetBackgroundColor(NS_RGB(68,141,192));
  SetForegroundColor(NS_RGB(255,255,255));
}

nsCalTimebarScale :: ~nsCalTimebarScale()
{
}

nsresult nsCalTimebarScale::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTimebarScaleCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIXPFCCanvasIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsCalTimebarCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsCalTimebarScale)
NS_IMPL_RELEASE(nsCalTimebarScale)

/*
 * Create a TimeContext with default values for now.
 */

nsresult nsCalTimebarScale :: Init()
{
  return (nsCalTimebarCanvas::Init());
}

/*
 * The TimeContext contains the data we use for figuring out how to draw
 * ourselves.  Get the data from it
 */

nsresult nsCalTimebarScale::PaintInterval(nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          PRUint32 aIndex,
                                          PRUint32 aStart,
                                          PRUint32 aSpace,
                                          PRUint32 aMinorInterval)
{
  int i;

  aRenderingContext.PushState();
  nsFont font(/* m_sFontName*/ "Arial", NS_FONT_STYLE_NORMAL,
		    NS_FONT_VARIANT_NORMAL,
		    NS_FONT_WEIGHT_BOLD,
		    0,
		    12);
  aRenderingContext.SetFont(font) ;

  /*
   * Paint this interval in it's entirety
   */
  nsCalTimebarCanvas::PaintInterval(aRenderingContext, aDirtyRect, aIndex, aStart, aSpace, aMinorInterval);
  nsRect rect;
  GetBounds(rect);

  aRenderingContext.SetColor(GetForegroundColor());

  aMinorInterval = 4;   // XXX: this is a hack, we should specify this in the XML -sman

  if (((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment() == eLayoutAlignment_horizontal)
  {
    rect.x = aStart;
    rect.width = aSpace;

    /*
     * draw the minor ticks...
     */
    PRUint32 iYStart = rect.y + (aSpace * 3 / 4);
    PRUint32 iYStop = rect.y + rect.height - INSET;
    PRUint32 iXSpace = rect.height / aMinorInterval;
    PRUint32 iX = rect.x + iXSpace;
    for (i = 0; i < (PRUint32) aMinorInterval; i++)
    {
      aRenderingContext.DrawLine(iX,iYStart, iX,iYStop);
      iX += iXSpace;
    }
  }
  else
  {
    /*
     * Vertical version
     */
    rect.y = aStart;
    rect.height = aSpace;

    /*
     * draw the minor ticks...
     */
    PRUint32 iXStart = rect.x + rect.width * 3 / 4;
    PRUint32 iXStop  = rect.x + rect.width - INSET;
    PRUint32 iYSpace = rect.height/ aMinorInterval;
    PRUint32 iY      = rect.y + INSET + iYSpace;
    for (i = 1; i < (PRUint32) aMinorInterval; i++)
    {
      aRenderingContext.DrawLine(iXStart,iY, iXStop,iY);
      iY += iYSpace;
    }
  }

  DrawTime(aRenderingContext, rect, aIndex);

  aRenderingContext.PopState();
  return NS_OK ;
}

nsresult nsCalTimebarScale :: DrawTime(nsIRenderingContext& aContext,
                                       nsRect& aRect,
                                       PRUint32 aIndex)
{

  /*
   * XXX: Create the String we will draw. We need to make this Unicode
   *      and support the various formats and time periods....
   */

  char text[20];
  PRUint32 hour;
  nscoord width, height, x, y;
  PRBool bAmPmFlag = PR_TRUE;
  PRBool bPM;

  hour = aIndex + GetTimeContext()->GetFirstVisibleTime();
  

  if ( bAmPmFlag )
  {
    bPM = (hour >= 12);
    if (hour == 0)
      hour = 12;
    else if (hour > 12)
      hour -= 12;
    PR_snprintf(text, 6, "%2d %s\0", hour, bPM ? "PM" : "AM" );
  }
  else
  {
    PR_snprintf(text, 6, "%2d:00\0", hour);
  }

  /*
   * compute the Metrics for the string
   */
  
  aContext.GetFontMetrics()->GetHeight(height);
  aContext.GetWidth(text,width);

  /*
   * center the text in our rect and draw it
   */
/*
  x = ((aRect.width - width)>>1)+aRect.x;
  y = ((aRect.height - height)>>1)+aRect.y;
*/
  x = aRect.x + (INSET << 1);
  y = aRect.y + (INSET << 1);

  aContext.DrawString(text,nsCRT::strlen(text),x,y,0);

  return (NS_OK);
}

nsEventStatus nsCalTimebarScale :: PaintBorder(nsIRenderingContext& aRenderingContext,
                                               const nsRect& aDirtyRect)
{
  nsRect rect;

  GetBounds(rect);

  rect.x++; rect.y++; rect.width-=2; rect.height-=2;
  aRenderingContext.SetColor(GetForegroundColor());
  aRenderingContext.DrawRect(rect);

  return nsEventStatus_eConsumeNoDefault;  
}

nsresult nsCalTimebarScale :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsCalTimebarCanvas::SetParameter(aKey, aValue));
}

nsresult nsCalTimebarScale :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

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

#include "nsCalTimebarTimeHeading.h"
#include "nsCalUICIID.h"

#include "nspr.h"
#include "nsCRT.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimebarTimeHeadingCID, NS_CAL_TIMEBARUSERHEADING_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);

nsCalTimebarTimeHeading :: nsCalTimebarTimeHeading(nsISupports* outer) : nsCalTimebarHeading(outer)
{
  NS_INIT_REFCNT();
}

nsCalTimebarTimeHeading :: ~nsCalTimebarTimeHeading()
{
}

nsresult nsCalTimebarTimeHeading::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTimebarTimeHeadingCID);                         
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
  return (nsCalTimebarHeading::QueryInterface(aIID, aInstancePtr));                                                 
}

NS_IMPL_ADDREF(nsCalTimebarTimeHeading)
NS_IMPL_RELEASE(nsCalTimebarTimeHeading)

nsresult nsCalTimebarTimeHeading :: Init()
{
  return NS_OK ;
}

nsEventStatus nsCalTimebarTimeHeading :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                                         const nsRect& aDirtyRect)
{

  if (GetTimeContext() == nsnull)
    return nsEventStatus_eConsumeNoDefault;  

  nscoord width, height, x, y;

  nsRect rect;

  GetBounds(rect);

  nsString * string;
  nsString pattern("EEE MMM-dd\n");

  GetTimeContext()->GetDTFirstVisible()->strftime(pattern, &string);

  /* 
   * set the font... 
   */
  aRenderingContext.PushState();
  nsFont font(/* m_sFontName*/ "Arial", NS_FONT_STYLE_NORMAL,
		    NS_FONT_VARIANT_NORMAL,
		    NS_FONT_WEIGHT_BOLD,
		    0,
		    12);
  aRenderingContext.SetFont(font) ;

  /*
   * compute the Metrics for the string
   */
  
  aRenderingContext.GetFontMetrics()->GetHeight(height);
  aRenderingContext.GetWidth(*string,width);

  /*
   * XXX: If we are too big, remove the Day of the week.  Need a better algorithm
   *      to use string who fits to begin with
   */

   if (width > rect.width)
   {
     pattern = "MMM dd\n";
     GetTimeContext()->GetDTFirstVisible()->strftime(pattern, &string);
     aRenderingContext.GetWidth(*string,width);
   }

  /*
   * center the text in our rect and draw it
   */

  x = ((rect.width - width)>>1)+rect.x;
  y = ((rect.height - height)>>1)+rect.y;

  aRenderingContext.SetColor(GetForegroundColor());
  aRenderingContext.DrawString(*string,nsCRT::strlen(*string),x,y,0);

  aRenderingContext.PopState();

  return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsCalTimebarTimeHeading :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                                         const nsRect& aDirtyRect)
{
  /*
   * Let the Base Canvas paint it's default background
   */

  nsXPFCCanvas::PaintBackground(aRenderingContext,aDirtyRect);

  return nsEventStatus_eConsumeNoDefault;  
}

nsresult nsCalTimebarTimeHeading :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsCalTimebarCanvas::SetParameter(aKey, aValue));
}

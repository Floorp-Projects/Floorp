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

#include "nsCalTimebarUserHeading.h"
#include "nsCalUICIID.h"

#include "nspr.h"
#include "nsCRT.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nscalstrings.h"
#include "nsIDeviceContext.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimebarUserHeadingCID, NS_CAL_TIMEBARUSERHEADING_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);

#define DEFAULT_WIDTH  25
#define DEFAULT_HEIGHT 25

nsCalTimebarUserHeading :: nsCalTimebarUserHeading(nsISupports* outer) : nsCalTimebarHeading(outer)
{
  NS_INIT_REFCNT();
  mUserName = CAL_STRING_DEFAULTUSERNAME;
}

nsCalTimebarUserHeading :: ~nsCalTimebarUserHeading()
{
}

nsresult nsCalTimebarUserHeading::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTimebarUserHeadingCID);                         
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
  return (nsCalTimebarHeading::QueryInterface(aIID,aInstancePtr));                                                 
}

NS_IMPL_ADDREF(nsCalTimebarUserHeading)
NS_IMPL_RELEASE(nsCalTimebarUserHeading)

nsresult nsCalTimebarUserHeading :: Init()
{
  return NS_OK ;
}

nsEventStatus nsCalTimebarUserHeading :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                                         const nsRect& aDirtyRect)
{
  nscoord width, height, x, y;

  nsRect rect;
  GetBounds(rect);

  /*
   * compute the Metrics for the string
   */
  
  aRenderingContext.GetFontMetrics()->GetHeight(height);
  aRenderingContext.GetWidth(mUserName,width);

  /*
   * center the text in our rect and draw it
   */

  x = ((rect.width - width)>>1)+rect.x;
  y = ((rect.height - height)>>1)+rect.y;

  aRenderingContext.SetColor(GetForegroundColor());
  aRenderingContext.DrawString(mUserName,x,y,0);

  return nsEventStatus_eConsumeNoDefault;  
}

nsresult nsCalTimebarUserHeading :: SetParameter(nsString& aKey, nsString& aValue)
{
  if (aKey.EqualsIgnoreCase(CAL_STRING_TITLE))
    SetUserName(aValue);

  return (nsCalTimebarCanvas::SetParameter(aKey, aValue));
}

nsresult nsCalTimebarUserHeading :: SetUserName(nsString& aString)
{
  mUserName = aString;
  return NS_OK;
}

nsString& nsCalTimebarUserHeading :: GetUserName()
{
  return mUserName;
}

nsresult nsCalTimebarUserHeading :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

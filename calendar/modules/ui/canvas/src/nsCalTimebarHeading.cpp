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

#include "nsCalTimebarHeading.h"
#include "nsCalUICIID.h"

#include "nspr.h"
#include "nsCRT.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#define USER_HEADER "Rickety Cricket's Agenda"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimebarHeadingCID, NS_CAL_TIMEBARHEADING_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);

#define DEFAULT_WIDTH  25
#define DEFAULT_HEIGHT 25

nsCalTimebarHeading :: nsCalTimebarHeading(nsISupports* outer) : nsCalTimebarCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsCalTimebarHeading :: ~nsCalTimebarHeading()
{
}

nsresult nsCalTimebarHeading::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTimebarHeadingCID);                         
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

NS_IMPL_ADDREF(nsCalTimebarHeading)
NS_IMPL_RELEASE(nsCalTimebarHeading)

nsresult nsCalTimebarHeading :: Init()
{
  return NS_OK ;
}

nsEventStatus nsCalTimebarHeading :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                                     const nsRect& aDirtyRect)
{
  /*
   * Let the Base Canvas paint it's default background
   */

  nsXPFCCanvas::PaintBackground(aRenderingContext,aDirtyRect);

  return nsEventStatus_eConsumeNoDefault;  
}

nsresult nsCalTimebarHeading :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsCalTimebarCanvas::SetParameter(aKey, aValue));
}

nsresult nsCalTimebarHeading :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

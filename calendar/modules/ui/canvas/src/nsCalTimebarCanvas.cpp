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

#include "nsCalTimebarCanvas.h"
#include "nsCalUICIID.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimebarCanvasCID, NS_CAL_TIMEBARCANVAS_CID);

#define LOCAL_INSET 1

nsCalTimebarCanvas :: nsCalTimebarCanvas(nsISupports* outer) : nsCalCanvas(outer)
{
  NS_INIT_REFCNT();
  mTimeContext = nsnull;
}

nsCalTimebarCanvas :: ~nsCalTimebarCanvas()
{
  NS_IF_RELEASE(mTimeContext);
}

nsresult nsCalTimebarCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTimebarCanvasCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsCalTimebarCanvas *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsCalCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsCalTimebarCanvas)
NS_IMPL_RELEASE(nsCalTimebarCanvas)

nsresult nsCalTimebarCanvas :: Init()
{
  static NS_DEFINE_IID(kCCalTimeContextCID, NS_CAL_TIME_CONTEXT_CID);
  static NS_DEFINE_IID(kCCalTimeContextIID, NS_ICAL_TIME_CONTEXT_IID);

  nsresult res = nsRepository::CreateInstance(kCCalTimeContextCID, 
                                              nsnull, 
                                              kCCalTimeContextIID, 
                                              (void **)&mTimeContext);

  if (NS_OK != res)
    return res ;

  mTimeContext->Init();

  return (nsCalCanvas::Init());    
}

PRUint32 nsCalTimebarCanvas::GetVisibleMajorIntervals()
{
  PRUint32 inc = mTimeContext->GetMajorIncrementInterval() ;
  PRUint32 delta = mTimeContext->GetVisibleTimeDifference();

  if ((delta/inc) > 1000) {
    PRUint32 inc2 = mTimeContext->GetMajorIncrementInterval() ;
    PRUint32 delta2 = mTimeContext->GetVisibleTimeDifference();
  }

  return (delta / inc) ;
}

PRUint32 nsCalTimebarCanvas::GetVisibleMinorIntervals()
{
  return (mTimeContext->GetMinorIncrementInterval()) ;
}

nsresult nsCalTimebarCanvas :: SetTimeContext(nsICalTimeContext * aContext)
{
  NS_IF_RELEASE(mTimeContext);
  mTimeContext = aContext;
  NS_ADDREF(mTimeContext);
  return NS_OK;
}

nsICalTimeContext * nsCalTimebarCanvas :: GetTimeContext()
{
  /*
   * If we have one, just return it
   */

  if (mTimeContext != nsnull)
    return (mTimeContext);

  /*
   * If we do not have one, look up the hierarchy. We need to be
   * careful here.  We must look for the first parent that implements
   * our interface
   */

  nsIXPFCCanvas * parent = GetParent();
  nsCalTimebarCanvas * time_canvas = nsnull;
  nsresult res ;

  while (parent != nsnull)
  {

    res = parent->QueryInterface(kCalTimebarCanvasCID,(void**)time_canvas);

    if (res == NS_OK)
    {
      /*
       * Check this parent
       */

      nsICalTimeContext * context = time_canvas->GetTimeContext();

      NS_RELEASE(time_canvas);

      return context;
    }

    parent = parent->GetParent();

  }

  return nsnull;

}


nsEventStatus nsCalTimebarCanvas :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                                    const nsRect& aDirtyRect)
{
  /*
   * Let the Base Canvas paint it's default background
   */

  nsRect rect;
  nsCalCanvas::PaintBackground(aRenderingContext,aDirtyRect);

  /*
   * Now paint the TimeContext over the base canvas background
   *
   * We need to find the amount of time between first and last
   * visible divied by the major increment. This is the number
   * of time intervals.  Then divide majorincrement by minor
   * increment to get number intervals between those
   */

  if (!mTimeContext)
    return nsEventStatus_eConsumeNoDefault;  

  PRUint32 major_intervals = GetVisibleMajorIntervals();
  PRUint32 minor_intervals = GetVisibleMinorIntervals();

  GetBounds(rect);

  PRUint32 space_per_interval, start;

  if (GetTimeContext()->GetHorizontal() == PR_TRUE) {
    space_per_interval = (rect.width) / major_intervals; 
    start = rect.x;
  } else {
    space_per_interval = ((rect.height-(2*LOCAL_INSET)) - ((rect.height-(2*LOCAL_INSET)) % major_intervals)) / major_intervals;
    start = rect.y+LOCAL_INSET;
  }

  PRUint32 i = 0;

  for (i=0; i<=major_intervals; i++) {

    PaintInterval(aRenderingContext, aDirtyRect, i,start,space_per_interval, minor_intervals);

    start += space_per_interval;
    
  }

  return nsEventStatus_eConsumeNoDefault;  
}

nsresult nsCalTimebarCanvas::PaintInterval(nsIRenderingContext& aRenderingContext,
                                           const nsRect& aDirtyRect,
                                           PRUint32 aIndex,
                                           PRUint32 aStart,
                                           PRUint32 aSpace,
                                           PRUint32 aMinorInterval)
{
  /*
   * Paint this interval in it's entirety
   */

  nsRect rect;

  GetBounds(rect);

  aRenderingContext.SetColor(GetForegroundColor());

  if (GetTimeContext()->GetHorizontal() == PR_TRUE) {
    rect.x = aStart;
    rect.width = aSpace;
  } else {
    rect.y = aStart;
    rect.height = aSpace;
  }

  aRenderingContext.DrawLine(rect.x+LOCAL_INSET, rect.y, rect.x+rect.width-LOCAL_INSET, rect.y);

  return NS_OK ;
}

nsresult nsCalTimebarCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsCalCanvas::SetParameter(aKey, aValue));
}


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
#include "nsXPFCToolkit.h"
#include "nsBoxLayout.h"
#include "nsxpfcCIID.h"
#include "nsIXPFCObserverManager.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimebarCanvasCID, NS_CAL_TIMEBARCANVAS_CID);
static NS_DEFINE_IID(kCXPFCObserverManagerCID, NS_XPFC_OBSERVERMANAGER_CID);
static NS_DEFINE_IID(kIXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);

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

  if (((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment() == eLayoutAlignment_horizontal) 
  {  
    space_per_interval = ((rect.width-(2*LOCAL_INSET)) - ((rect.width-(2*LOCAL_INSET)) % major_intervals)) / major_intervals;
    start = rect.x+LOCAL_INSET;
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

  if (((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment() == eLayoutAlignment_horizontal) {  
    rect.x = aStart;
    rect.width = aSpace;
    aRenderingContext.DrawLine(rect.x, rect.y+LOCAL_INSET, rect.x, rect.y+rect.height-LOCAL_INSET);
  } else {
    rect.y = aStart;
    rect.height = aSpace;
    aRenderingContext.DrawLine(rect.x+LOCAL_INSET, rect.y, rect.x+rect.width-LOCAL_INSET, rect.y);
  }

  return NS_OK ;
}

nsresult nsCalTimebarCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsCalCanvas::SetParameter(aKey, aValue));
}

/*
 * Call SetContext on all child canvas that support the
 * nsCalTimebarCanvas interface.  This routine is recursive. 
 *
 * We need to store this TimeContext for ourselves also in case
 * we get asked to add new DayView Canvas's, we'll ask the DayView
 * to copy our context to start with.
 */

nsresult nsCalTimebarCanvas :: SetTimeContext(nsICalTimeContext * aContext)
{

  NS_IF_RELEASE(mTimeContext);
  mTimeContext = aContext;
  NS_ADDREF(mTimeContext);

  return (NS_OK);
}

nsresult nsCalTimebarCanvas :: ChangeChildDateTime(nsCalTimebarCanvas * aCanvas,
                                                   nsDateTime * aDateTime)
{

  nsIDateTime * datetime;
  nsICalTimeContext * context = aCanvas->GetTimeContext();

  if (context == nsnull)
    return NS_OK;

  context->SetDate(aDateTime);

  datetime = context->GetDTStart() ;
  datetime->SetYear(aDateTime->GetYear());
  datetime->SetMonth(aDateTime->GetMonth());
  datetime->SetDay(aDateTime->GetDay());

  datetime = context->GetDTEnd() ;
  datetime->SetYear(aDateTime->GetYear());
  datetime->SetMonth(aDateTime->GetMonth());
  datetime->SetDay(aDateTime->GetDay()+1);

  datetime = context->GetDTFirstVisible() ;
  datetime->SetYear(aDateTime->GetYear());
  datetime->SetMonth(aDateTime->GetMonth());
  datetime->SetDay(aDateTime->GetDay());

  datetime = context->GetDTLastVisible() ;
  datetime->SetYear(aDateTime->GetYear());
  datetime->SetMonth(aDateTime->GetMonth());
  datetime->SetDay(aDateTime->GetDay());

  return NS_OK;
}

nsresult nsCalTimebarCanvas :: SetChildTimeContext(nsCalTimebarCanvas * aCanvas,
                                                   nsICalTimeContext * aContext,
                                                   PRUint32 increment)
{
  nsICalTimeContext * context;

  static NS_DEFINE_IID(kCCalTimeContextCID,  NS_CAL_TIME_CONTEXT_CID);
  static NS_DEFINE_IID(kCCalTimeContextIID,  NS_ICAL_TIME_CONTEXT_IID);

  nsresult res = nsRepository::CreateInstance(kCCalTimeContextCID, 
                                              nsnull, 
                                              kCCalTimeContextIID, 
                                              (void **)&context);

  if (NS_OK != res)
    return res ;

  context->Init();

  context->Copy(aContext);

  /* 
   * Register this context to observe the copied context.  We'll
   * need to deal with context mgmt if these things get frivolously
   * destroyed/created.
   */
  static NS_DEFINE_IID(kXPFCObserverIID, NS_IXPFC_OBSERVER_IID);
  static NS_DEFINE_IID(kXPFCSubjectIID,  NS_IXPFC_SUBJECT_IID);

  nsIXPFCSubject  * context_subject;
  nsIXPFCObserver * context_observer;

  aContext->QueryInterface(kXPFCSubjectIID, (void **)&context_subject);
  context->QueryInterface(kXPFCObserverIID, (void **)&context_observer);

  nsIXPFCObserverManager* om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);

  om->Register(context_subject, context_observer);

  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  NS_RELEASE(context_subject);
  NS_RELEASE(context_observer);

  /*
   * TODO:  Add the increment here for the appropriate period
   */

  aCanvas->SetTimeContext(context);


  context->AddPeriod(nsCalPeriodFormat_kDay,increment);

  NS_RELEASE(context);

  return NS_OK;
}



nsresult nsCalTimebarCanvas :: ChangeChildDateTime(PRUint32 aIndex, nsDateTime * aDateTime)
{
  nsresult res = NS_OK;
  nsIIterator * iterator ;
  nsCalTimebarCanvas * canvas ;
  PRUint32 index = 0;

  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return res;

  iterator->Init();

  while((!(iterator->IsDone())))
  {

    if ((index == aIndex)) {

      /*
       * Iterate through these children til we find the right one
       */

      canvas = (nsCalTimebarCanvas *) iterator->CurrentItem();

      nsresult res2 = NS_OK;
      nsIIterator * iterator2 ;
      nsCalTimebarCanvas * canvas2 ;

      res2 = canvas->CreateIterator(&iterator2);

      if (NS_OK != res2)
        return res2;

      iterator2->Init();

      while(!(iterator2->IsDone()))
      {

        canvas2 = (nsCalTimebarCanvas *) iterator2->CurrentItem();

        nsCalTimebarCanvas * canvas_iface = nsnull;;

        canvas2->QueryInterface(kCalTimebarCanvasCID, (void**) &canvas_iface);

        if ((canvas_iface != nsnull)) {
        
          ChangeChildDateTime(canvas_iface, aDateTime);

          NS_RELEASE(canvas_iface);
        }

        iterator2->Next();

      }

    }
       
    index++;
    iterator->Next();
  }

  return res;
}


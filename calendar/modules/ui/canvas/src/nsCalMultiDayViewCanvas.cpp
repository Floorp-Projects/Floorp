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

#include "nsCalMultiDayViewCanvas.h"
#include "nsCalDayViewCanvas.h"
#include "nsCalTimebarTimeHeading.h"
#include "nsBoxLayout.h"
#include "nsCalUICIID.h"

#include "nsIArray.h"
#include "nsIIterator.h"

#include "nsCalToolkit.h"
#include "nsCalDayListCommand.h"
#include "nsCalNewModelCommand.h"

#include "nscalstrings.h"
#include "nsxpfcstrings.h"


#define DEFAULT_NUMBER_VIEWABLE_DAYS 5

static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalMultiDayViewCanvasCID, NS_CAL_MULTIDAYVIEWCANVAS_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID,           NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kCalTimebarCanvasCID,      NS_CAL_TIMEBARCANVAS_CID);
static NS_DEFINE_IID(kCCalDayViewCID,           NS_CAL_DAYVIEWCANVAS_CID);

nsCalMultiDayViewCanvas :: nsCalMultiDayViewCanvas(nsISupports* outer) : nsCalMultiViewCanvas(outer)
{
  NS_INIT_REFCNT();
  mNumberViewableDays = 0; 
  mMinRepeat = 1;
  mMaxRepeat = 1000;
}

nsCalMultiDayViewCanvas :: ~nsCalMultiDayViewCanvas()
{
}

nsresult nsCalMultiDayViewCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalMultiDayViewCanvasCID);                         
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
  return (nsCalMultiViewCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsCalMultiDayViewCanvas)
NS_IMPL_RELEASE(nsCalMultiDayViewCanvas)

/*
 *
 */

nsresult nsCalMultiDayViewCanvas :: Init()
{

  /*
   * We also need a context.  Maybe this should be specifically
   * done rather than a part of Init.
   */

  nsCalMultiViewCanvas::Init();

  /*
   * Create the default number of viewable day canvas's as children
   */

  SetNumberViewableDays(DEFAULT_NUMBER_VIEWABLE_DAYS);

  return NS_OK;
}

/*
 * Since our Day View's will do the painting, just ignore
 */

nsEventStatus nsCalMultiDayViewCanvas :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                                         const nsRect& aDirtyRect)
{
  return (nsCalMultiViewCanvas :: PaintBackground(aRenderingContext,aDirtyRect));  
}

nsresult nsCalMultiDayViewCanvas :: SetNumberViewableDays(PRUint32 aNumberViewableDays)
{

  if (aNumberViewableDays < mMinRepeat)
    aNumberViewableDays = mMinRepeat;
  if (aNumberViewableDays > mMaxRepeat)
    aNumberViewableDays = mMaxRepeat;
     
  if (mNumberViewableDays != aNumberViewableDays) {

    PRUint32 i;

    mNumberViewableDays = aNumberViewableDays;

    /*
     * Do the creation/deleteion process here
     */

    DeleteChildren();

    for (i=0; i<mNumberViewableDays; i++) 
    {
      AddDayViewCanvas();
    }

  }

  return NS_OK;  
}


PRUint32 nsCalMultiDayViewCanvas :: GetNumberViewableDays()
{
  return (mNumberViewableDays);
}

nsIXPFCCanvas * nsCalMultiDayViewCanvas :: AddDayViewCanvas()
{
  static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);

  nsCalDayViewCanvas * canvas;
  nsIXPFCCanvas * parent;

  nsresult res;

  /*
   * Add a canvas to hold the rest
   */

  static NS_DEFINE_IID(kCXPFCCanvasCID,            NS_XPFC_CANVAS_CID);

  res = nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                     nsnull, 
                                     kIXPFCCanvasIID, 
                                     (void **)&canvas);

  if (NS_OK != res)
    return nsnull ;

  canvas->Init();      

  AddChildCanvas(canvas);

  canvas->SetBackgroundColor(GetBackgroundColor());
  
  nsLayoutAlignment la = ((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment();

  if (eLayoutAlignment_horizontal == la)
    ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);
  else
    ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_horizontal);
  
  parent = canvas;

  NS_RELEASE(canvas);

  /*
   * Add the Header Time Canvas
   */

  if (GetShowHeaders()) {

    static NS_DEFINE_IID(kCalTimebarTimeHeadingCID,     NS_CAL_TIMEBARTIMEHEADING_CID);

    res = nsRepository::CreateInstance(kCalTimebarTimeHeadingCID, 
                                       nsnull, 
                                       kIXPFCCanvasIID, 
                                       (void **)&canvas);

    if (NS_OK != res)
      return nsnull ;

    canvas->Init();      

    parent->AddChildCanvas(canvas);

    //canvas->SetBackgroundColor(GetBackgroundColor());
    canvas->SetPreferredSize(nsSize(25,25));
    canvas->SetMinimumSize(nsSize(25,25));
    canvas->SetMaximumSize(nsSize(25,25));

    NS_RELEASE(canvas);

  }

  /*
   * Add the Day View Canvas
   */

  res = nsRepository::CreateInstance(kCCalDayViewCID, 
                                     nsnull, 
                                     kIXPFCCanvasIID, 
                                     (void **)&canvas);

  if (NS_OK != res)
    return nsnull ;

  canvas->Init();      

  parent->AddChildCanvas(canvas);

  canvas->SetBackgroundColor(GetBackgroundColor());

  canvas->SetMinimumSize(nsSize(100,100));

  if (eLayoutAlignment_horizontal == la)
    ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_vertical);
  else
    ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(eLayoutAlignment_horizontal);

  NS_RELEASE(canvas);

  /*
   * Add the status View Canvas
   */

  if (GetShowStatus()) {

    res = nsRepository::CreateInstance(kCXPFCCanvasCID, 
                                       nsnull, 
                                       kIXPFCCanvasIID, 
                                       (void **)&canvas);

    if (NS_OK != res)
      return nsnull ;

    canvas->Init();      

    parent->AddChildCanvas(canvas);

    canvas->SetBackgroundColor(GetBackgroundColor());
    canvas->SetPreferredSize(nsSize(25,25));
    canvas->SetMinimumSize(nsSize(25,25));
    canvas->SetMaximumSize(nsSize(25,25));

    NS_RELEASE(canvas);

  }

  return (parent);
}


nsEventStatus nsCalMultiDayViewCanvas::Action(nsIXPFCCommand * aCommand)
{
  /*
   * If this is a DayList Command, modify our children as necessary
   */

  nsresult res;
  PRUint32 count = 0;

  nsCalDayListCommand *  daylist_command = nsnull;
  nsCalNewModelCommand * newmodel_command = nsnull;
  static NS_DEFINE_IID(kCalDayListCommandCID, NS_CAL_DAYLIST_COMMAND_CID);                 
  static NS_DEFINE_IID(kCalNewModelCommandCID, NS_CAL_NEWMODEL_COMMAND_CID);                 

  res = aCommand->QueryInterface(kCalDayListCommandCID,(void**)&daylist_command);

  if (NS_OK == res)
  {
    
    /*
     * It's a day list command ... set the date
     */

    nsIIterator * iterator;

    res = daylist_command->CreateIterator(&iterator);

    if (NS_OK != res)
      return nsEventStatus_eIgnore;

    iterator->Init();

    count = iterator->Count();

    if (iterator->Count() < mMinRepeat)
      count = mMinRepeat;
    if (iterator->Count() > mMaxRepeat)
      count = mMaxRepeat;

    if (count != GetChildCount()) {

      /*
       * The number of new days is different from current. If the number is
       * less, delete the difference.  If it is more, add new canvas.
       * 
       */

      if (count < GetChildCount()) {

        /*
         * Delete the difference
         */

        PRUint32 delta = GetChildCount() - count;

        DeleteChildren(delta);

      } else if (count > GetChildCount()) {

        /*
         * Add new children
         */

        PRUint32 delta = count - GetChildCount();

        PRUint32 i;

        for (i=0; i<delta; i++) 
        {
          AddDayViewCanvas();
        }


        mNumberViewableDays += delta;

        /*
         * Set the time context to be based off multi-day
         */

        SetTimeContext(GetTimeContext());

      }

    }

    /*
     * Now we have the right number of days, lets update the context
     */


    PRUint32 index = 0;

    while(!(iterator->IsDone()))
    {

      if (index > count)
        break;

      nsDateTime * datetime = (nsDateTime *) iterator->CurrentItem();

      if (datetime != nsnull) {

        ChangeChildDateTime(index,datetime);

      }
  
      index++;

      iterator->Next();
    }


    Layout();
  }

  res = aCommand->QueryInterface(kCalNewModelCommandCID,(void**)&newmodel_command);

  if (NS_OK == res)
  {
    nsIXPFCCanvas * canvas = AddDayViewCanvas();

    /*
     * Get the last sibling and set it's model....
     */

    if (canvas != nsnull)
    {

      nsDateTime * nsdatetime;
      static NS_DEFINE_IID(kCalDateTimeCID, NS_DATETIME_CID);
      static NS_DEFINE_IID(kCalDateTimeIID, NS_IDATETIME_IID);
      nsresult res = nsRepository::CreateInstance(kCalDateTimeCID,nsnull, kCalDateTimeCID, (void **)&nsdatetime);

      if (NS_OK == res)
        nsdatetime->Init();

      /*
       * Apply to all children ....
       */
      nsIIterator * iterator ;
      nsCalTimebarCanvas * tbc ;

      res = canvas->CreateIterator(&iterator);

      if (NS_OK == res)
      {
        iterator->Init();

        while(!(iterator->IsDone()))
        {

          tbc = (nsCalTimebarCanvas *) iterator->CurrentItem();

          nsCalTimebarCanvas * canvas_iface = nsnull;;

          tbc->QueryInterface(kCalTimebarCanvasCID, (void**) &canvas_iface);

          if (canvas_iface) 
          {
        
            SetChildTimeContext(canvas_iface, GetTimeContext(), 0);
            ChangeChildDateTime(canvas_iface, nsdatetime);      
            canvas_iface->SetModel(newmodel_command->mModel);
            
            NS_RELEASE(canvas_iface);
          }

          iterator->Next();

        }
       
        iterator->Next();
      }

      NS_RELEASE(iterator);

      Layout();

      nsRect bounds;
      GetView()->GetBounds(bounds);
      gXPFCToolkit->GetViewManager()->UpdateView(GetView(), bounds, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC);

      NS_RELEASE(tbc);

    }
  }
  
  return (nsCalMultiViewCanvas::Action(aCommand));

}

nsresult nsCalMultiDayViewCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  PRInt32 i ;

  if (aKey.EqualsIgnoreCase(CAL_STRING_MINREPEAT)) {

    mMinRepeat = (PRUint32) aValue.ToInteger(&i);

  } else if (aKey.EqualsIgnoreCase(CAL_STRING_MAXREPEAT)) {

    mMaxRepeat = (PRUint32) aValue.ToInteger(&i);

  } else if (aKey.EqualsIgnoreCase(CAL_STRING_COUNT)) {

    SetNumberViewableDays((PRUint32)aValue.ToInteger(&i));

  } else if (aKey.EqualsIgnoreCase(XPFC_STRING_LAYOUT)) {

    // XXX: Layout should implement this interface.
    //      Then, put functionality in the core layout class
    //      to identify the type of layout object needed.

    if (aValue.EqualsIgnoreCase(XPFC_STRING_XBOX)) {
      ((nsBoxLayout *)GetLayout())->SetLayoutAlignment(eLayoutAlignment_horizontal);
    } else if (aValue.EqualsIgnoreCase(XPFC_STRING_YBOX)) {
      ((nsBoxLayout *)GetLayout())->SetLayoutAlignment(eLayoutAlignment_vertical);
    }

    SetMultiDayLayout(((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment());

  }


  return (nsCalMultiViewCanvas::SetParameter(aKey, aValue));
}

nsresult nsCalMultiDayViewCanvas :: SetMultiDayLayout(nsLayoutAlignment aLayoutAlignment)
{
  nsresult res = NS_OK;
  nsIXPFCCanvas * canvas;
  nsIIterator * iterator ;

  /*
   * Enumarate the child canvas
   */
  
  nsLayoutAlignment la = ((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment();

  if (eLayoutAlignment_horizontal == la)
    la = eLayoutAlignment_vertical;
  else
    la = eLayoutAlignment_horizontal;

  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return res;

  iterator->Init();

  while(!(iterator->IsDone()))
  {
    canvas = (nsIXPFCCanvas *) iterator->CurrentItem();

    ((nsBoxLayout *)(canvas->GetLayout()))->SetLayoutAlignment(la);

      /*
       * Enumarate children looking for DayView and set its alignment
       */

    nsresult res2 = NS_OK;
    nsIIterator * iterator2 ;
    nsIXPFCCanvas * canvas2 ;

    res2 = canvas->CreateIterator(&iterator2);

    if (NS_OK == res2)
    {

      iterator2->Init();

      while(!(iterator2->IsDone()))
      {

        canvas2 = (nsIXPFCCanvas *) iterator2->CurrentItem();

        nsCalDayViewCanvas * canvas_iface = nsnull;;

        canvas2->QueryInterface(kCCalDayViewCID, (void**) &canvas_iface);

        if (canvas_iface) 
        {
        
          ((nsBoxLayout *)(canvas_iface->GetLayout()))->SetLayoutAlignment(la);          

          NS_RELEASE(canvas_iface);
        }

        iterator2->Next();

      }

      NS_RELEASE(iterator2);

    }
    
    iterator->Next();
  }

  NS_IF_RELEASE(iterator);

  return res;
}

/*
 * Call SetContext on all child canvas that support the
 * nsCalTimebarCanvas interface.  This routine is recursive. 
 *
 * We need to store this TimeContext for ourselves also in case
 * we get asked to add new DayView Canvas's, we'll ask the DayView
 * to copy our context to start with.
 */

nsresult nsCalMultiDayViewCanvas :: SetTimeContext(nsICalTimeContext * aContext)
{

  nsresult res = NS_OK;
  nsIIterator * iterator ;
  nsCalTimebarCanvas * canvas ;
  PRUint32 index = 0;

  res = CreateIterator(&iterator);

  if (NS_OK != res)
    return res;

  iterator->Init();

  while(!(iterator->IsDone()))
  {

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

      if (canvas_iface) {
        
        SetChildTimeContext(canvas_iface, aContext, index);

        NS_RELEASE(canvas_iface);
      }

      iterator2->Next();

    }

    NS_RELEASE(iterator2);
       
    index++;
    iterator->Next();
  }

  NS_RELEASE(iterator);

  return (nsCalMultiViewCanvas :: SetTimeContext(aContext));
}

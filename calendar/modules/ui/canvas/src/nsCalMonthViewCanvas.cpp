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

#include "nscore.h"
#include "nsCalMonthViewCanvas.h"
#include "nsCalUICIID.h"
#include "nsDateTime.h"
#include "nsCalToolkit.h"
#include "nsCRT.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsICalendarUser.h"
#include "nsICalendarModel.h"
#include "nsDateTime.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "prprf.h"
#include "nsString.h"

#include "datetime.h"
#include "ptrarray.h"
#include "vevent.h"

#include "capi.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalMonthViewCanvasCID, NS_CAL_MONTHVIEWCANVAS_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kICalendarUserIID,     NS_ICALENDAR_USER_IID); 
static NS_DEFINE_IID(kICalendarModelIID,    NS_ICALENDAR_MODEL_IID); 

nsCalMonthViewCanvas :: nsCalMonthViewCanvas(nsISupports* outer) : nsCalTimebarComponentCanvas(outer)
{
  NS_INIT_REFCNT();
  mNumRows = 0;
  mNumColumns = 0;
}

nsCalMonthViewCanvas :: ~nsCalMonthViewCanvas()
{
}

nsresult nsCalMonthViewCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalMonthViewCanvasCID);                         
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
  return (nsCalTimebarComponentCanvas::QueryInterface(aIID, aInstancePtr));                                                 
}

NS_IMPL_ADDREF(nsCalMonthViewCanvas)
NS_IMPL_RELEASE(nsCalMonthViewCanvas)

nsresult nsCalMonthViewCanvas :: Init()
{
  return (nsCalTimebarComponentCanvas::Init());
}


nsEventStatus nsCalMonthViewCanvas :: PaintBorder(nsIRenderingContext& aRenderingContext,
                                                  const nsRect& aDirtyRect)
{
  return (nsCalTimebarComponentCanvas::PaintBorder(aRenderingContext,aDirtyRect));
}

nsEventStatus nsCalMonthViewCanvas :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                                      const nsRect& aDirtyRect)
{
#if 0
  JulianPtrArray * evtVctr = 0;
  VEvent *pEvent = 0;
  nsRect rect;
  char *psBuf;
  char sBuf[256];
  nscoord fm_height ;
  UnicodeString usFmt("%(hh:mma)B-%(hh:mma)e");         // XXX this needs to be put in a resource, then a variable that can be switched
  UnicodeString usTemp;

  evtVctr = new JulianPtrArray();

  DateTime * dStart = ((nsDateTime *)GetTimeContext()->GetDTStart())->GetDateTime();
  DateTime * dEnd   = ((nsDateTime *)GetTimeContext()->GetDTEnd())->GetDateTime();

  nsIModel * model = GetModel();
  nsICalendarModel * calmodel = nsnull;

  if (nsnull == model)
    return nsEventStatus_eConsumeNoDefault;  

  nsICalendarUser * user = nsnull;

  nsresult res = model->QueryInterface(kICalendarModelIID, (void**)&calmodel);
  if (NS_OK != res)
    return nsEventStatus_eConsumeNoDefault;

  calmodel->GetCalendarUser(user);  

  if (NS_OK != res)
    return nsEventStatus_eConsumeNoDefault;

  nsILayer * layer;

  user->GetLayer(layer);

  NSCalendar * nscal;

  layer->GetCal(nscal); 

  nscal->getEventsByRange(evtVctr, *dStart, *dEnd); 

  for (int j = 0; j < evtVctr->GetSize(); j++)
  {
    pEvent = (VEvent *) evtVctr->GetAt(j);

    DateTime dstart = pEvent->getDTStart();
    DateTime dsend  = pEvent->getDTEnd();

    if (    (PRUint32) dstart.getHour() < (PRUint32) GetTimeContext()->GetLastVisibleTime()
         && (PRUint32) dsend.getHour()  > (PRUint32) GetTimeContext()->GetFirstVisibleTime())
    {
      /*
       * compute rect for this event
       *
       * x,width are ok.  y and height must be computed
       */
      nsRect bounds;
      GetBounds(rect);
      GetBounds(bounds);

      /*
       * XXX: Subtract off the modulus of the area. This should not be so hardcoded!
       */
      rect.height = rect.height - ((rect.height-2)%GetVisibleMajorIntervals());

      PRUint32 vis_event_start_min = dstart.getHour() * 60 + dstart.getMinute();
      PRUint32 vis_event_end_min   = dsend.getHour() * 60 + dsend.getMinute();

      PRFloat64 sratio = ((PRFloat64)(vis_event_start_min - vis_start_min)) * (div_ratio);
      PRFloat64 eratio = ((PRFloat64)(vis_event_end_min   - vis_start_min)) * (div_ratio);

      rect.y += (int)(rect.height * sratio);
      rect.height = (int)((rect.height * eratio) - (rect.y - bounds.y));

      aRenderingContext.SetColor(mComponentColor);

      rect.x = rect.x + 2 * INSET ; 
      rect.width = rect.width - 4*INSET ;
      rect.y = rect.y + INSET ;
      rect.height = rect.height - 2 * INSET;


      if (rect.y < bounds.y)
        rect.y = bounds.y+1;

      if ((rect.y+rect.height) > (bounds.y+bounds.height))
        rect.height = (bounds.y+bounds.height)-1;

      aRenderingContext.FillRect(rect);

      /*
      * Render the highlights
      */
      aRenderingContext.SetColor(nsLighter(mComponentColor));
      aRenderingContext.DrawLine(rect.x,rect.y,rect.x+rect.width,rect.y);
      aRenderingContext.DrawLine(rect.x,rect.y,rect.x,rect.y+rect.height);
      aRenderingContext.SetColor(nsDarker(mComponentColor));
      aRenderingContext.DrawLine(rect.x+rect.width,rect.y,rect.x+rect.width,rect.y+rect.height);
      aRenderingContext.DrawLine(rect.x,rect.y+rect.height,rect.x+rect.width,rect.y+rect.height);

      aRenderingContext.GetFontMetrics()->GetHeight(fm_height);

      if (rect.height > fm_height)
      {
        // rndctx->SetColor(GetForegroundColor());
        aRenderingContext.SetColor(NS_RGB(255,255,255));          /* XXX: This color should come from someplace else... */

        /*
         * XXX. we need to handle '\n' in a format string...
         * This should not require two separate coding calls
         * we need to generalize this.
         */
        psBuf = pEvent->toStringFmt(usFmt).toCString("");
        aRenderingContext.DrawString(psBuf,nsCRT::strlen(psBuf),rect.x+1,rect.y,0);
        delete psBuf;

        if (rect.height > (2 * fm_height))
        {
          psBuf = pEvent->getSummary().toCString("");
          aRenderingContext.DrawString(psBuf,nsCRT::strlen(psBuf),rect.x+1,rect.y+fm_height,0);
          delete psBuf;
        }
      }
    }
  }
            
  delete evtVctr;

  NS_RELEASE(calmodel);

  return nsEventStatus_eConsumeNoDefault;  

#endif

 return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsCalMonthViewCanvas :: PaintBackground(nsIRenderingContext& aRenderingContext,
                                                      const nsRect& aDirtyRect)
{
  /*
   * First, we need to find out how many rows/columns we want/need.
   */

  nsICalTimeContext * tc = GetTimeContext();  
  nsIDateTime * dts = nsnull;
  nsIDateTime * dtmonth = nsnull;
  nsresult res = NS_OK;
  DateTime * datetime = nsnull ;
  PRUint32 i,j;
  PRInt32 iDate = 1;
  PRInt32 iLastDate ;

  if (nsnull == tc)
    return nsEventStatus_eConsumeNoDefault;   

  dts = tc->GetDTStart(); // XXX: Deal with TimeContext correctly here
  dtmonth = dts->Copy();
  datetime = ((nsDateTime *)dtmonth)->GetDateTime();

  PRInt32 ii = datetime->get(Calendar::MONTH);

  /*
   * Compute number of weeks
   */

  datetime->setDayOfMonth(1);
  i = datetime->get(Calendar::WEEK_OF_YEAR);
  datetime->findLastDayOfMonth();
  j = datetime->get(Calendar::WEEK_OF_YEAR);
  datetime->setDayOfMonth(1);
  mNumRows = j-i+1;
  mNumColumns = 7; // XXX: Need a query for max number days in a week

  /*
   * compute last day of month ... 
   */

  iLastDate = datetime->getMonthLength(datetime->getMonth(),datetime->getYear());

  /*
   * compute what the day of week the first cell is on.  This may be
   * negative, up to -7
   */

  iDate -= (datetime->get(Calendar::DAY_OF_WEEK) -1);

  /*
   * Now, let's render the background 
   *
   * XXX:  As an optimization we may want to combine cell back/fore painting
   */

  nsCalCanvas::PaintBackground(aRenderingContext, aDirtyRect);  

  for (i=0; i<=mNumRows; i++)
  {
    for (j=0; j<mNumColumns; j++)
    { 
      PRUint32 thisDate;     

      if (iDate > 0 && iDate <= iLastDate)
        thisDate = iDate;
      else
        thisDate = 0;
      
      PaintCellBackground(i,j,thisDate,aRenderingContext,aDirtyRect);
      iDate++;
    }
  }


  NS_RELEASE(dtmonth);

  return nsEventStatus_eConsumeNoDefault;  
}

nsEventStatus nsCalMonthViewCanvas :: PaintCellBackground( PRUint32& aCellRow, PRUint32& aCellColumn,
                                                           PRUint32& aCellDate,
                                                           nsIRenderingContext& aRenderingContext,
                                                           const nsRect& aDirtyRect)
{
  /*
   * Draw a box using the foreground color
   */

  nscoord x,y,w,h;
  nscoord cell_width;
  nscoord cell_height;
  nsRect bounds;
  PRUint32 cell_width_remainder,cell_height_remainder;

  GetBounds(bounds);

  cell_width  = bounds.width  / mNumColumns;
  cell_height = bounds.height / mNumRows;

  x = cell_width  * aCellColumn + bounds.x;
  y = cell_height * aCellRow + bounds.y;
  w = cell_width;
  h = cell_height;

  /*
   * If there is a modulus, disperse that across the cells
   * beginning from front.
   */

  cell_width_remainder = bounds.width % mNumColumns;
  cell_height_remainder = bounds.height % mNumRows;

  if (aCellColumn >= cell_width_remainder)
    x += cell_width_remainder;
  else
    x += aCellColumn;

  if (cell_width_remainder > aCellColumn)
    w++;

  if (aCellRow >= cell_height_remainder)
    y += cell_height_remainder;
  else
    y += aCellRow;

  if (cell_height_remainder > aCellRow)
    h++;

  /*
   * Now paint the cell outline (with shadows)
   */

  nsRect rect;

  rect.x = x;
  rect.y = y;
  rect.width = w;
  rect.height = h;

  rect.width--; rect.height--;

  aRenderingContext.SetColor(GetForegroundColor());
  aRenderingContext.DrawLine(rect.x,rect.y,rect.x+rect.width,rect.y);
  aRenderingContext.DrawLine(rect.x,rect.y,rect.x,rect.y+rect.height);

  aRenderingContext.DrawLine(rect.x+rect.width,rect.y,rect.x+rect.width,rect.y+rect.height);
  aRenderingContext.DrawLine(rect.x,rect.y+rect.height,rect.x+rect.width,rect.y+rect.height);

  /*
   * Now render intervals
   *
   * XXX: Hardcoded til fix TimeContext
   */

  aRenderingContext.SetColor(Dim(GetBorderColor()));

  rect.x++;
  rect.width--;

  nscoord interval = rect.height / 5;

  PRInt32 i ;

  for (i=0; i<4; i++)
  {
    rect.y += interval;
    aRenderingContext.DrawLine(rect.x,rect.y,rect.x+rect.width,rect.y);
  }

  /*
   * Now put the number of the day in lower right hand corner
   */

  nsString strDate;

  if (aCellDate > 0)
  {
    char buf[5];

    PR_snprintf(buf, 5, "%d", aCellDate);

    strDate = buf;


    nscoord text_height ;
    nscoord text_width ;

    aRenderingContext.GetFontMetrics()->GetHeight(text_height);
    aRenderingContext.GetFontMetrics()->GetWidth(strDate, text_width);

    x = x + w - text_width;
    y = y + h - text_height;

    aRenderingContext.SetColor(GetForegroundColor());
    aRenderingContext.DrawString(strDate,nsCRT::strlen(strDate),x,y,0);
  }

  return nsEventStatus_eConsumeNoDefault;  
}


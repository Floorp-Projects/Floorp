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
#include "icalcomp.h"

#include "capi.h"

#define NUM_INTERVALS 5

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

  mArrayRows = nsnull;
  mArrayColumns = nsnull;

}

nsCalMonthViewCanvas :: ~nsCalMonthViewCanvas()
{
  if (mArrayRows != nsnull)
    delete mArrayRows;
  if (mArrayColumns != nsnull)
    delete mArrayColumns;
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

  // XXX: May need to init to some value here

  mArrayRows    = new nsVoidArray(31);
  mArrayColumns = new nsVoidArray(31);

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
  nsICalTimeContext * tc = GetTimeContext();  
  nsIDateTime * dts = nsnull;
  nsIDateTime * dtmonth1 = nsnull;
  nsIDateTime * dtmonth2 = nsnull;
  nsresult res = NS_OK;
  DateTime * dStart = nsnull ;
  DateTime * dEnd = nsnull ;
  PRUint32 i,j;
  PRUint32 row, column;
  PRUint32 currDate = 0;
  PRUint32 indexOnDate = 0;


  if (nsnull == tc)
    return nsEventStatus_eConsumeNoDefault;   

  dts = tc->GetDTStart(); // XXX: Deal with TimeContext correctly here
  dtmonth1 = dts->Copy();
  dtmonth2 = dts->Copy();
  dStart = ((nsDateTime *)dtmonth1)->GetDateTime();
  dEnd   = ((nsDateTime *)dtmonth2)->GetDateTime();

  /*
   * Compute number of weeks
   */

  dStart->setDayOfMonth(1);
  dEnd->findLastDayOfMonth();


  JulianPtrArray * evtVctr = 0;
  VEvent *pEvent = 0;
  nsRect rect;
  char *psBuf;
  char sBuf[256];
  nscoord fm_height ;
  UnicodeString usFmt("%(hh:mma)B-%(hh:mma)e");         // XXX this needs to be put in a resource, then a variable that can be switched
  UnicodeString usTemp;

  evtVctr = new JulianPtrArray();

  nsIModel * model = GetModel();
  nsICalendarModel * calmodel = nsnull;

  if (nsnull == model)
    return nsEventStatus_eConsumeNoDefault;  

  nsICalendarUser * user = nsnull;

  res = model->QueryInterface(kICalendarModelIID, (void**)&calmodel);
  if (NS_OK != res)
    return nsEventStatus_eConsumeNoDefault;

  calmodel->GetCalendarUser(user);  

  if (NS_OK != res)
    return nsEventStatus_eConsumeNoDefault;

  nsILayer * layer;

  user->GetLayer(layer);

#if 0
  NSCalendar * nscal;

  layer->GetCal(nscal); 

  nscal->sortComponentsByDTStart(ICalComponent::ICAL_COMPONENT::ICAL_COMPONENT_VEVENT);

  nscal->getEventsByRange(evtVctr, *dStart, *dEnd); 
#endif

  layer->FetchEventsByRange(dStart, dEnd, evtVctr); 

  for (j = 0; j < evtVctr->GetSize(); j++)
  {
    pEvent = (VEvent *) evtVctr->GetAt(j);

    DateTime dstart = pEvent->getDTStart();
    DateTime dsend  = pEvent->getDTEnd();

    /*
     * Let's find out which grid we belong to, then plop ourselves appropriately
     * within that grid
     */

    PRUint32 ii = dstart.getDate();

    CellFromDate(ii, row, column);

    // Multiple events on one day?
    if (currDate != dstart.getDate())
    {
      currDate = dstart.getDate();
      indexOnDate = 0;
    } else {
      indexOnDate++;
    }


    /*
     * Now, let's compute the bounds for our event
     */

    nsRect bounds;

    GetCellBounds(row, column, bounds);

    /*
     * Scale down to the size of our interval
     */

    bounds.height /= NUM_INTERVALS;
    bounds.width--;

    bounds.y += (bounds.height * indexOnDate);

    bounds.x+=2; bounds.y+=2;
    bounds.width-=4;bounds.height-=4;

    // Set the color
    aRenderingContext.SetColor(GetForegroundColor());

    aRenderingContext.GetFontMetrics()->GetHeight(fm_height);

    if (bounds.height > fm_height)
    {
      
      aRenderingContext.SetColor(mComponentColor);
      aRenderingContext.FillRect(bounds);
      aRenderingContext.SetColor(NS_BrightenColor(mComponentColor));
      aRenderingContext.DrawLine(bounds.x,bounds.y,bounds.x+bounds.width,bounds.y);
      aRenderingContext.DrawLine(bounds.x,bounds.y,bounds.x,bounds.y+bounds.height);
      aRenderingContext.SetColor(NS_DarkenColor(mComponentColor));
      aRenderingContext.DrawLine(bounds.x+bounds.width,bounds.y,bounds.x+bounds.width,bounds.y+bounds.height);
      aRenderingContext.DrawLine(bounds.x,bounds.y+bounds.height,bounds.x+bounds.width,bounds.y+bounds.height);

      psBuf = pEvent->toStringFmt(usFmt).toCString("");

      aRenderingContext.SetColor(NS_RGB(255,255,255)); 
      nsString string = psBuf;
      aRenderingContext.DrawString(string,bounds.x+1,bounds.y,0);

      delete psBuf;

      if (bounds.height > (2 * fm_height))
      {
        psBuf = pEvent->getSummary().toCString("");
        string = psBuf;
        aRenderingContext.DrawString(string,bounds.x+1,bounds.y+fm_height,0);
        delete psBuf;
      }
    }
  }
            
 delete evtVctr;

 NS_RELEASE(calmodel);

 NS_RELEASE(dtmonth1);
 NS_RELEASE(dtmonth2);

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

      CacheCellData(i,j,thisDate);

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
  nsRect rect;

  GetCellBounds(aCellRow, aCellColumn, rect);

  x = rect.x;
  y = rect.y;
  w = rect.width;
  h = rect.height;

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
  aRenderingContext.SetLineStyle(nsLineStyle_kDotted);

  rect.x++;
  rect.width--;

  nscoord interval = rect.height / NUM_INTERVALS;

  PRInt32 i ;

  for (i=0; i<4; i++)
  {
    rect.y += interval;
    aRenderingContext.DrawLine(rect.x,rect.y,rect.x+rect.width,rect.y);
  }

  /*
   * Now put the number of the day in lower right hand corner
   */

  aRenderingContext.SetLineStyle(nsLineStyle_kSolid);

  nsString strDate;

  if (aCellDate > 0)
  {
    char buf[5];

    PR_snprintf(buf, 5, "%d", aCellDate);

    strDate = buf;


    nscoord text_height ;
    nscoord text_width ;

    aRenderingContext.GetFontMetrics()->GetHeight(text_height);
    aRenderingContext.GetWidth(strDate, text_width);

    x = x + w - text_width;
    y = y + h - text_height;

    aRenderingContext.SetColor(GetForegroundColor());

    nsString string = strDate;
    aRenderingContext.DrawString(string,x,y,0);

  }

  return nsEventStatus_eConsumeNoDefault;  
}

nsresult nsCalMonthViewCanvas :: CacheCellData(PRUint32& aCellRow, PRUint32& aCellColumn, PRUint32& aCellDate)
{
  if (aCellDate > 0 && aCellDate < 32)
  {
    mArrayRows->ReplaceElementAt((void *)aCellRow, aCellDate);
    mArrayColumns->ReplaceElementAt((void *)aCellRow, aCellDate);
  }
  
  return NS_OK;
}

nsresult nsCalMonthViewCanvas :: CellFromDate(PRUint32& aCellDate, PRUint32& aCellRow, PRUint32& aCellColumn)
{
  aCellRow = -1;
  aCellColumn = -1;

  if (aCellDate > 0 && aCellDate < 32)
  {
    aCellRow    = (PRUint32) mArrayRows->ElementAt(aCellDate);
    aCellColumn = (PRUint32) mArrayColumns->ElementAt(aCellDate);
  }
  
  return NS_OK;
}

nsresult nsCalMonthViewCanvas :: GetCellBounds(PRUint32& aCellRow, PRUint32& aCellColumn, nsRect& aBounds)
{
  nscoord cell_width;
  nscoord cell_height;
  nsRect bounds;
  PRUint32 cell_width_remainder,cell_height_remainder;

  GetBounds(bounds);

  cell_width  = bounds.width  / mNumColumns;
  cell_height = bounds.height / mNumRows;

  aBounds.x = cell_width  * aCellColumn + bounds.x;
  aBounds.y = cell_height * aCellRow + bounds.y;
  aBounds.width = cell_width;
  aBounds.height = cell_height;

  /*
   * If there is a modulus, disperse that across the cells
   * beginning from front.
   */

  cell_width_remainder = bounds.width % mNumColumns;
  cell_height_remainder = bounds.height % mNumRows;

  if (aCellColumn >= cell_width_remainder)
    aBounds.x += cell_width_remainder;
  else
    aBounds.x += aCellColumn;

  if (cell_width_remainder > aCellColumn)
    aBounds.width++;

  if (aCellRow >= cell_height_remainder)
    aBounds.y += cell_height_remainder;
  else
    aBounds.y += aCellRow;

  if (cell_height_remainder > aCellRow)
    aBounds.height++;

  return NS_OK;
}

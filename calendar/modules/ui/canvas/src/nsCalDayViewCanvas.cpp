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
#include "nsCalDayViewCanvas.h"
#include "nsCalUICIID.h"
#include "nsDateTime.h"
#include "nsCalToolkit.h"
#include "nsCRT.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsICalendarUser.h"
#include "nsICalendarModel.h"
#include "nsBoxLayout.h"

#include "datetime.h"
#include "ptrarray.h"
#include "vevent.h"

typedef struct
{
    char *p;
    size_t iSize;
    nsIXPFCCanvas * canvas;
} CAPICTX;

#include "capi.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalDayViewCanvasCID, NS_CAL_DAYVIEWCANVAS_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kICalendarUserIID,     NS_ICALENDAR_USER_IID); 
static NS_DEFINE_IID(kICalendarModelIID,    NS_ICALENDAR_MODEL_IID); 

nsCalDayViewCanvas :: nsCalDayViewCanvas(nsISupports* outer) : nsCalTimebarComponentCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsCalDayViewCanvas :: ~nsCalDayViewCanvas()
{
}

nsresult nsCalDayViewCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalDayViewCanvasCID);                         
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

NS_IMPL_ADDREF(nsCalDayViewCanvas)
NS_IMPL_RELEASE(nsCalDayViewCanvas)

nsresult nsCalDayViewCanvas :: Init()
{
  return (nsCalTimebarComponentCanvas::Init());
}

static nscolor nsLighter(nscolor c)
{
  PRUint8 r = NS_GET_R(c);
  PRUint8 g = NS_GET_G(c);
  PRUint8 b = NS_GET_B(c);
  return NS_RGB( r + ((255-r)>>1), g + ((255-g)>>1), b + ((255-b)>>1) );
}
static nscolor nsDarker(nscolor c)
{
  PRUint8 r = NS_GET_R(c);
  PRUint8 g = NS_GET_G(c);
  PRUint8 b = NS_GET_B(c);
  return NS_RGB( r - (r>>1), g - (g>>1), b - (b>>1) );
}

/**
 * Paint some light gray lines behind the events...
 * @param aIndex
 * @param aStart
 * @param aSpace
 * @param aMinorInterval   how many minor ticks 
 * @return NX_OK on success
 */
nsresult nsCalDayViewCanvas::PaintInterval(nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect,
                                          PRUint32 aIndex,
                                          PRUint32 aStart,
                                          PRUint32 aSpace,
                                          PRUint32 aMinorInterval)
{
  PRUint32 i;

  /*
   * Paint this interval in it's entirety
   */
  nsCalTimebarCanvas::PaintInterval(aRenderingContext, aDirtyRect, aIndex, aStart, aSpace, aMinorInterval);
  nsRect rect;
  GetBounds(rect);

  aRenderingContext.SetColor(nsLighter(nsLighter(nsDarker(GetBackgroundColor()))));

  aMinorInterval = 4;   // XXX: this is a hack, we should specify this in the XML -sman

  if (((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment() == eLayoutAlignment_horizontal)
  {
    rect.x = aStart;
    rect.width = aSpace;

    /*
     * draw the minor ticks...
     */
    PRUint32 iYStart = rect.y + INSET;
    PRUint32 iYStop = rect.y + rect.height - INSET;
    PRUint32 iXSpace = rect.width / aMinorInterval;
    PRUint32 iX = rect.x + INSET + iXSpace;
    for (i = 1; i < (PRUint32) aMinorInterval; i++)
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
    PRUint32 iXStart = rect.x + INSET;
    PRUint32 iXStop  = rect.x + rect.width - INSET;
    PRUint32 iYSpace = rect.height/ aMinorInterval;
    PRUint32 iY      = rect.y + INSET + iYSpace;
    for (i = 1; i < (PRUint32) aMinorInterval; i++)
    {
      aRenderingContext.DrawLine(iXStart,iY, iXStop,iY);
      iY += iYSpace;
    }
  }

  //DrawTime(rndctx, rect, aIndex);

  return NS_OK ;
}

nsEventStatus nsCalDayViewCanvas :: PaintBorder(nsIRenderingContext& aRenderingContext,
                                                const nsRect& aDirtyRect)
{
  nsRect rect;

  GetBounds(rect);

  rect.x++; rect.y++; rect.width-=2; rect.height-=2;
  aRenderingContext.SetColor(GetForegroundColor());
  aRenderingContext.DrawRect(rect);

  return nsEventStatus_eConsumeNoDefault;  
}

/**
 * Paint the foreground of the canvas and the events that overlap it.
 *
 * XXX the logic here is not really correct. It should probably be
 *     a more generic canvas capable of displaying any time-based
 *     event. There should probably be a separate mechanism that
 *     connects it to VEVENTS.
 */
nsEventStatus nsCalDayViewCanvas :: PaintForeground(nsIRenderingContext& aRenderingContext,
                                                    const nsRect& aDirtyRect)
{
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

  layer->FetchEventsByRange(dStart, dEnd, evtVctr); 

  PRUint32 vis_start_min = GetTimeContext()->GetFirstVisibleTime(nsCalPeriodFormat_kHour) * 60 + GetTimeContext()->GetFirstVisibleTime(nsCalPeriodFormat_kMinute);
  PRUint32 vis_end_min   = GetTimeContext()->GetLastVisibleTime(nsCalPeriodFormat_kHour) * 60 + GetTimeContext()->GetLastVisibleTime(nsCalPeriodFormat_kMinute);
  PRFloat64 div_ratio =  ((PRFloat64)(1)) / (PRFloat64)(vis_end_min - vis_start_min) ;

  /* 
   * set the font... 
   */
  aRenderingContext.PushState();
  nsFont font(/* m_sFontName*/ "Arial", NS_FONT_STYLE_NORMAL,
		    NS_FONT_VARIANT_NORMAL,
		    NS_FONT_WEIGHT_BOLD,
		    0,
		    10);
  aRenderingContext.SetFont(font) ;

  /*
   * XXX. This whole algorithm must be changed. We need to grab all
   *      the events first, determine how many overlap. Then determine
   *      how big we can make each box, then render...
   */
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

      PRUint32 vis_event_start_min = dstart.getHour() * 60 + dstart.getMinute();
      PRUint32 vis_event_end_min   = dsend.getHour() * 60 + dsend.getMinute();

      PRFloat64 sratio = ((PRFloat64)(vis_event_start_min - vis_start_min)) * (div_ratio);
      PRFloat64 eratio = ((PRFloat64)(vis_event_end_min   - vis_start_min)) * (div_ratio);

      aRenderingContext.SetColor(mComponentColor);

      if (((nsBoxLayout *)(GetLayout()))->GetLayoutAlignment() == eLayoutAlignment_vertical)
      {
        /*
         * XXX: Subtract off the modulus of the area. This should not be so hardcoded!
         */
        rect.height = rect.height - ((rect.height-2)%GetVisibleMajorIntervals());

        rect.y += (int)(rect.height * sratio);
        rect.height = (int)((rect.height * eratio) - (rect.y - bounds.y));

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
      } else {

        /*
         * XXX: Subtract off the modulus of the area. This should not be so hardcoded!
         */
        rect.width = rect.width - ((rect.width-2)%GetVisibleMajorIntervals());

        rect.x += (int)(rect.width * sratio);
        rect.width = (int)((rect.width * eratio) - (rect.x - bounds.x));

        rect.y = rect.y + 2 * INSET ; 
        rect.height = rect.height - 4*INSET ;
        rect.x = rect.x + INSET ;
        rect.width = rect.width - 2 * INSET;


        if (rect.x < bounds.x)
          rect.x = bounds.x+1;

        if ((rect.x+rect.width) > (bounds.x+bounds.width))
          rect.width = (bounds.x+bounds.width)-1;

        aRenderingContext.FillRect(rect);

        /*
        * Render the highlights
        */
        aRenderingContext.SetColor(nsLighter(mComponentColor));
        aRenderingContext.DrawLine(rect.x,rect.y,rect.x,rect.y+rect.height);
        aRenderingContext.DrawLine(rect.x,rect.y,rect.x+rect.width,rect.y);
        aRenderingContext.SetColor(nsDarker(mComponentColor));
        aRenderingContext.DrawLine(rect.x,rect.y+rect.height,rect.x+rect.width,rect.y+rect.height);
        aRenderingContext.DrawLine(rect.x+rect.width,rect.y,rect.x+rect.width,rect.y+rect.height);

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
  }
  aRenderingContext.PopState();
            
  delete evtVctr;

  NS_RELEASE(calmodel);

  return nsEventStatus_eConsumeNoDefault;  
}

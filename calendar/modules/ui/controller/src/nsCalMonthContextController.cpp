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

#include "nsCalMonthContextController.h"
#include "nsCalUICIID.h"

#include "jdefines.h"
#include "events.h"
#include "ptrarray.h"
#include "datetime.h"

#include "nsIWidget.h"
#include "nsIDeviceContext.h"
#include "nsColor.h"
#include "nsFont.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "nsDebug.h"
#include "nsSize.h"
#include "nsPoint.h"
#include "nsDuration.h"
#include "nsxpfcCIID.h"


static NS_DEFINE_IID(kXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);
static NS_DEFINE_IID(kCalDurationCommandCID, NS_CAL_DURATION_COMMAND_CID);
static NS_DEFINE_IID(kCalDayListCommandCID, NS_CAL_DAYLIST_COMMAND_CID);

#include "nsCalDurationCommand.h"
#include "nsCalDayListCommand.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalMonthContextControllerCID, NS_CAL_MONTH_CONTEXT_CONTROLLER_CID);
static NS_DEFINE_IID(kIXPFCCanvasIID, NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kCalDateTimeCID, NS_DATETIME_CID);
static NS_DEFINE_IID(kCalDateTimeIID, NS_IDATETIME_IID);
static NS_DEFINE_IID(kCalDurationCID, NS_DURATION_CID);
static NS_DEFINE_IID(kCalDurationIID, NS_IDURATION_IID);

#define DEFAULT_WIDTH  210
#define DEFAULT_HEIGHT 210

nsCalMonthContextController :: nsCalMonthContextController(nsISupports* outer) : nsCalContextController(outer)
{
  NS_INIT_REFCNT();
    int i;
	  m_iFontSize = 10;
    m_iMCRows = 0;
    m_iMCCols = 0;
    m_sFontName = "Arial";  // Used to be "Times Roman"
    m_iMCVertSpacing = 10;
    m_iMCHorzSpacing = 10;
    m_iCellHorzSpacing = 4;
    m_iCellVertSpacing = 4;
    m_iDOWChars = 2;
    m_iYOffset = 2;
    m_iXOffset = 2;   // this far from the right edge of the cell.
    m_eTitleAlign = nsCalMonthContextController::CENTER;
    m_Locale = Locale::getDefault();
    for (i = 0; i < 7; i++)
        m_asDOW[i] = 0;
    for (i = 0; i < 12; i++)
        m_asMonths[i] = 0;
    m_iFDOW = 0;
    m_iMatrixSize = 0;
    m_pStartTimeMap = 0;
    m_pValidPosMap = 0;
    m_pDates = 0;
    m_iCmdArmed = (int)CMiniCalEvent::MNOTHING;
    m_iFDOW = Calendar::SUNDAY;
    SetMonthsArray();
    SetDOWNameArray(1);
    m_bGrid = 0;
    m_bSep = 1;
    m_bWeeks = 1;
    m_bArrows = 1;
    m_MCBGColor = GetBackgroundColor();
	  m_DIMFGColor = NS_RGB(128,64,0);
    m_CurDayFGColor = NS_RGB(255,255,255);
    m_CurDayBGColor = NS_RGB(0,0,255);
    m_HighlightFGColor = NS_RGB(255,255,255);
    m_HighlightBGColor = NS_RGB(255,0,0);
    m_ActionDateList = nsnull;
    SetDefaults();
}

nsCalMonthContextController :: ~nsCalMonthContextController()
{
  if (m_pDates != 0)
      delete m_pDates;

  if (nsnull != m_pValidPosMap)
    PR_Free(m_pValidPosMap);

  if (nsnull != m_pStartTimeMap)
    PR_Free(m_pStartTimeMap);

  NS_IF_RELEASE(m_ActionDateList);

}

nsresult nsCalMonthContextController::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalMonthContextControllerCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsCalMonthContextController *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIXPFCCanvasIID)) {                                      
    *aInstancePtr = (void*) (nsIXPFCCanvas *)(this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsCalContextController::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsCalMonthContextController)
NS_IMPL_RELEASE(nsCalMonthContextController)

nsresult nsCalMonthContextController :: Init()
{
  if (nsnull == m_pDates)
    m_pDates = new JulianPtrArray();

  /*
   * Create the Interactive date list array
   */

  if (nsnull == m_ActionDateList)
  {
    static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                                nsnull, 
                                                kCVectorCID, 
                                                (void **)&m_ActionDateList);

    if (NS_OK != res)
      return res ;

    m_ActionDateList->Init();
  }

  nsFont font(m_sFontName /*"Arial"*/, NS_FONT_STYLE_NORMAL,
		    NS_FONT_VARIANT_NORMAL,
		    NS_FONT_WEIGHT_BOLD,
		    0,
		    12);
  
  SetFont(font) ;

  return ResizeMapBuffers();
}


/**
***  Draw all the minicals we can in the space we have
***
**/
void nsCalMonthContextController::DrawMiniCal(void* p)
{
#define TIME_IT 1
#if TIME_IT
    /*
     *  A simple timing mechanism to help determine whether
     *  a change has help or hurt the redraw speed...
     */
    clock_t start, finish;
    start = clock();
#endif
    nsIRenderingContext * ctx = mRenderingContext;

    ctx->PushState();

    DateTime d = GetDate();
    InitRedraw(p);

    nsRect rect;
    GetBounds(rect);

    int iX = rect.x;
    int iY = rect.y;
    int iRows = GetMCRows();
    int iCols = GetMCCols();
    int i;
    
    for (i = 0; i < iRows; i++)
    {
        for (int j = 0; j < iCols; j++)
        {
            SetValidPos(i,j,0);
            SetStartTimeMap(i,j,0);
            PaintMiniCalElement( p, 
                i, j, iRows, iCols, iX,iY, d,
                (i == 0 && j == 0) || (i+1 == iRows && j+1 == iCols) );
            iX += GetMCWidth() + GetMCHorzSpacing();
    		d.nextMonth();
        }
        iX = rect.x;
        iY += GetMCHeight() + GetMCVertSpacing();
    }

#if TIME_IT
    finish = clock();
    double duration = (double)(finish - start) / CLOCKS_PER_SEC;
    start = clock();
#endif
    
    HighlightCell(GetDate(),GetCurrentDayFGColor(),GetCurrentDayBGColor());

    nscolor hFG = GetCurrentDayFGColor();
    nscolor hBG = GetCurrentDayBGColor();
    for (i = 0; i < m_pDates->GetSize(); i++)
    {
        HighlightCell(*(DateTime*)m_pDates->GetAt(i),hFG,hBG);
    }

#if TIME_IT
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
#endif

    PostRedraw(p);

    ctx->PopState();
}


/**
 *  Paint a particular minical.
 *  @param p        void* pointer that can be cast to a CDC*
 *  @param iRow     which row of minicals
 *  @param iCol     which column of minicals
 *  @param iTotRows total number of rows in the minical matrix
 *  @param iTotCols total number of columns in the minical matrix
 *  @param iXLeft   x coordinate of upper left corner of cell
 *  @param iYTop    y coordinate of upper left corner of cell
 *  @param baseDate a <code>DateTime</code> value with the correct month for this minical
 *  @param m_tm       used to help with proper spacing
 *  @param bLeading show dates in cells that don't belong to this month?
 */
void nsCalMonthContextController::PaintMiniCalElement(
    void* p, 
    int iRow, int iCol, 
    int iTotRows, int iTotCols, 
    int iXLeft, int iYTop, 
    DateTime& baseDate, 
    PRBool bLeading)
{
    nsIDeviceContext * pDeviceContext = (nsIDeviceContext *) p;
    int iDaysInYear = 365;
    int iX, iY;
    int iBottom, iRight;
    nsSize te;
    int iStrWidth, iStrHeight;
    int i, j;
  	char s[50];
    UnicodeString sFmt("MMMM yyyy");  // date-format pattern to print 
	  PRBool bFirstRowFirstCol = (PRBool)(iRow == 0 && iCol == 0);
	  PRBool bLastRowLastCol = (PRBool)(iRow + 1 == iTotRows && iCol + 1 == iTotCols);
    DateTime d = baseDate;

	  /*
	   *  Paint the background for the mini-cal
	   */
    mRenderingContext->SetColor(GetMCBGColor());
    mRenderingContext->FillRect(iXLeft,iYTop, GetMCWidth(),GetMCHeight());
    mRenderingContext->SetColor(GetForegroundColor());
    
    /*
     *  If the grid is enabled, draw it...
     */
    if (GetGrid())
    {
        iX = iXLeft;
        iY = iYTop;
        iBottom = iY + GetCellRows() * GetCellHeight();

        for ( i = 0; i <= GetCellCols(); i++)
        {
            mRenderingContext->DrawLine(iX,iY,iX,iBottom);        
            iX += GetCellWidth();
        }
        iRight = iXLeft + GetCellCols()*GetCellWidth();
        iY = iYTop;
        for (j = 0; j <= GetCellRows(); j++)
        {
            mRenderingContext->DrawLine(iXLeft,iY,iRight,iY);        
            iY += GetCellHeight();
        }
    }


    /*
     *  Print the title of the minical...
     */
    // sprintf(s,"%s",d.strftime(sFmt/*,GetLocale()*/).toCString("") );
    sprintf(s,"%s %d", m_asMonths[d.getMonth()], d.getYear() );
    GetTextExtent(s,strlen(s),iStrWidth,iStrHeight);
	  int iSpacing = 0;
    switch (m_eTitleAlign)
    {
        case nsCalMonthContextController::CENTER: iX = iXLeft + ((GetMCWidth() - iStrWidth)>>1);	break;
        case nsCalMonthContextController::LEFT:   iX = iXLeft + iSpacing; break;
        default:                                  iX = iXLeft + (GetMCWidth() - iStrWidth - iSpacing); break;
    }

    nscoord leading;

    mRenderingContext->GetFontMetrics()->GetLeading(leading);

    iY = iYTop + leading;

    mRenderingContext->DrawString(s, nsCRT::strlen(s), iX, iY, 100);

    /*
    ***
    ***  Draw the arrows if needed...
    ***
    **/
    if (GetArrows())
    {
        if (iRow == 0 && iCol == 0)
        {
            nsPoint pts[3];
            iY = iYTop;

            /*
             *  Left arrow...
             */
            switch (m_eTitleAlign)
            {
                case nsCalMonthContextController::CENTER: iX = iXLeft; break;
                case nsCalMonthContextController::LEFT:   iX = iXLeft + GetMCWidth() - 2 * GetCellWidth(); break;
                default:                                  iX = iXLeft; break;
            }

            pts[0].x = iX + ((3 * GetCellWidth()) >> 2);
            pts[0].y = iY + (GetCellHeight() >> 2);
            pts[1].x = iX + (GetCellWidth()  >> 2);
            pts[1].y = iY + (GetCellHeight() >> 1);
            pts[2].x = pts[0].x;
            pts[2].y = iY +  ((3 * GetCellHeight()) >> 2);
            mRenderingContext->FillPolygon(pts,3);
            //m_pDC->Polygon(pts,3);

            /*
             *  Right arrow...
             */
            switch (m_eTitleAlign)
            {
                case nsCalMonthContextController::CENTER: iX = iXLeft + GetMCWidth() - GetCellWidth(); break;
                case nsCalMonthContextController::LEFT:   iX = iXLeft + GetMCWidth() - GetCellWidth(); break;
                default:                   iX = iXLeft + GetCellWidth(); break;
            }

            pts[0].x = iX + (GetCellWidth() >> 2);
            pts[0].y = iY + (GetCellHeight() >> 2);
            pts[1].x = pts[0].x;
            pts[1].y = iY + ((3 * GetCellHeight()) >> 2);
            pts[2].x = iX + ((3 * GetCellWidth()) >> 2);
            pts[2].y = iY + (GetCellHeight() >> 1);
            mRenderingContext->FillPolygon(pts,3);
        }
    }


    /*
     *  Draw the days of the week
     */
    int iMaxChars = GetDOWColHdrChars();
    iX = iXLeft + (GetWeeks() ? GetCellWidth() : 0);
    iY = iYTop + GetCellHeight() + leading;
    for ( i = 0; i < 7; i++)
    {
        GetTextExtent(m_asDOW[i],iMaxChars,iStrWidth,iStrHeight);
        mRenderingContext->DrawString(m_asDOW[i], nsCRT::strlen(m_asDOW[i]), iX + ((GetCellWidth() - iStrWidth)>>1), iY, 100);
        iX += GetCellWidth();
    }

    /*
     *  Draw the separator(s)
     */
    if (GetSeparator())
    {
        iY = iYTop + 2 * GetCellHeight() - GetCellVertSpacing() + 1;
        mRenderingContext->DrawLine(iXLeft,iY,iXLeft + GetMCWidth(),iY);        
        if (GetWeeks())
        {
            iBottom = iY + 6*GetCellHeight();
            mRenderingContext->DrawLine(iXLeft + GetCellWidth(),iY,iXLeft + GetCellWidth(),iBottom);
        }
    }

    /*
     *  Draw the days of the monthly calendar
     */
    int iXL = iXLeft + GetCellWidth() - m_iXOffset;
    int iXLeftInit = iXL + (GetWeeks() ? GetCellWidth() : 0);
    int iDay = 1;
    iX = iXLeftInit;
    iY = iYTop + 2 * GetCellHeight() + m_iYOffset;
    d = baseDate;
    int iThisMonth = d.get(Calendar::MONTH);
    int iLastDOM;		// what is the last day of the current month
    PRBool bShow;

    /*
     *  Track the cell positions that mark where the days of the current month
     *  start and stop.  This during event handling to quickly determine whether
     *  or not the mouse is on a day of the month.  Note that these positions
     *  are relative to the 6row 7col subset of cells where the days of the month
     *  actually appear.
     */
    int iStartPos = -1; // first day this month starts at this position
    int iStopPos = -1;  // last day this month is at this position
    int iPos = 0;

    /*
     *  Set d to the first day that is to appear on the minical.  That is go to the first
     *  of the month, then back off days until we get to the desired first day of the week.
     *
     *  We jump through a few hoops here to set up counters so that we don't have to
     *  put a DateTime object inside the loops below.  Things slowed down considerably
     *  when a DateTime was incremented through the course of the loop.
     * 
     *  The state diagram is as follows:
     *    < 0  -  date is in previous month
     *   == 0  -  date is in current month
     *    > 0  -  date is in next month
     */
    d.set(Calendar::DAY_OF_MONTH,1);
    d.nextMonth();
    d.prevDay();
    iLastDOM = d.get(Calendar::DAY_OF_MONTH);           // this is the last day of the month
    d.nextDay();
    d.prevMonth();
    int iState = 0 - d.prevDOW(GetFDOW());   // this is the date for cell 0,0 of the month matrix
    SetStartTimeMap(iRow,iCol,d.getTime());  // update the map for this minical

    if ( d.get(Calendar::MONTH) != iThisMonth )
	    iDay = d.get(Calendar::DAY_OF_MONTH);
    int iDOY = d.get(Calendar::DAY_OF_YEAR);
    int iWeek;
    PRBool bDaysShownThisWeek;

    if (iDay == 1)
        iStartPos = 0;

    /*
     *  Now spin through the all the cells of the monthly calendar
     */
    for (i = 0; i < 6; i++)
    {
      bDaysShownThisWeek = 0;
      for (j = 0; j < 7; j++)
      {
        /*
         *  If the date is in the previous or next month, don't show it unless
         *  bLeading is true.
         */
        bShow = 1;
        if ( iState != 0)		// if d.month != iThisMonth
        {
          /*
           *  Actually, leading means show the previous days on the upper left
           *  minical and the trailing days on the lower right minical.
           */
          if ( i == 0 && bFirstRowFirstCol || i >  0 && bLastRowLastCol )
          {
            bShow = bLeading;
            mRenderingContext->SetColor(GetDIMFGColor());
          }
          else
            bShow = 0;
        }

        /*
         *  Print the date if we must...
         */
        if (bShow)
        {
          sprintf(s,"%2d", iDay);
          GetTextExtent(s,2,iStrWidth,iStrHeight);
          mRenderingContext->DrawString(s, nsCRT::strlen(s), iX - iStrWidth, iY, 100);
          bDaysShownThisWeek = 1;
        }

        /*
         *  Set the week number based on the 4th day of the week...
         */
        if (j == 3)
          iWeek = iDOY/7;

        ++iDay;
        if ((++iDOY) > iDaysInYear)
          iDOY = 1;

        if (iDay > iLastDOM && iState == 0)
        {
          iStopPos = iPos;    // last day of current month is at this position
          iState = 1;		    // next month now
          iDay = 1;
        }

        ++iPos;
        if (iState < 0)
        {
          ++iState;
          if (iState == 0)
          {
            iDay = 1;
            iStartPos = iPos;   // first day of current month is at this position
            mRenderingContext->SetColor(GetForegroundColor());
          }
        }

        iX += GetCellWidth();
      }

      /*
      *  If any days on the week were drawn, show the week number if weeks are turned on.
      */
      if (GetWeeks() && bDaysShownThisWeek)
      {
        sprintf(s,"%2d", iWeek );
        GetTextExtent(s,2,iStrWidth,iStrHeight);
        mRenderingContext->DrawString(s, nsCRT::strlen(s), iXL - iStrWidth, iY, 100);
      }

      /*
      *  Move to the next row...
      */
      iY += GetCellHeight();
      iX = iXLeftInit;
    }

    SetValidPos(iRow,iCol,iStartPos,iStopPos);
}

/**
 * Distinguish the cell for date d from the other cells
 *
 * @param d  this determines which cell will be highlighted
 * @param fg the forground color for highighting
 * @param bg the background color for highlighting
 */
void nsCalMonthContextController::HighlightCell(DateTime d, nscolor fg, nscolor bg)
{
    int iCellX, iCellY;
    SetDateCoords(d,iCellX,iCellY);
    HighlightCell( d, fg, bg, iCellX, iCellY );
}

/**
 * Distinguish the cell for date d from the other cells
 *
 * @param d  the date in the cell
 * @param iX the left coordinate of the cell
 * @param iY the top coordinate of the cell
 */
void nsCalMonthContextController::HighlightCell(DateTime d, nscolor fg, nscolor bg, int iX, int iY)
{
    if (iX == -1 || iY == -1)
        return;

    char sBuf[20];

    mRenderingContext->SetColor(bg);
    mRenderingContext->FillRect(iX,iY, GetCellWidth(),GetCellHeight());

	mRenderingContext->SetColor(fg);

    iX += GetCellWidth();
    sprintf(sBuf,"%2d", (int) d.getDate());
    
    int iW, iH;
    GetTextExtent(sBuf,2,iW,iH);
    mRenderingContext->DrawString(sBuf, nsCRT::strlen(sBuf), iX - iW - m_iXOffset, iY+m_iYOffset, 100);
}

/**
 *  Add the supplied dates to the current date list
 *  @param pV  pointer to an array of dates to add
 */
void nsCalMonthContextController::SetSelectedDates(JulianPtrArray* pV)
{
    DateTime *pD;

    for (int i = 0; i < pV->GetSize(); i++)
    {
        pD = (DateTime *)pV->GetAt(i);
        m_pDates->InsertBinary(new DateTime(*pD), DateTime_SortAscending);
        //m_pDates->Add( new DateTime(*pD) );
    }
}

/**
 *  Remove all dates from the current list
 *  
 */
void nsCalMonthContextController::ClearDateList()
{
    DateTime *pD;
    for (int i = 0; i < m_pDates->GetSize(); i++)
    {
        pD = (DateTime *)m_pDates->GetAt(i);
        delete pD;
    }
    m_pDates->RemoveAll();
}

/**
 *  UnhighlightDate sets the cell for the supplied date back
 *  to the normal foreground and background colors.
 *  @param d  the date to unhighlight.
 */
void nsCalMonthContextController::UnhighlightDate(DateTime d)
{

    HighlightCell(d, GetForegroundColor(), GetMCBGColor() );
}

/**
 *  RemoveSelectedDate removes the supplied date from the current
 *  date list if it is present.
 *  @param d  the date to remove.
 */
void nsCalMonthContextController::RemoveSelectedDate(DateTime d)
{
    DateTime *pD;
    for (int i = 0; i < m_pDates->GetSize(); i++)
    {
        pD = (DateTime *)m_pDates->GetAt(i);
        if (*pD == d)
        {
            delete pD;
            m_pDates->RemoveAt(i);
            return;
        }
    }
}

/**
 *  Sets internal variables that control the size of a minical.
 *
 *  @return 0 = success, 1 = failure
 */
int nsCalMonthContextController::SetPaintValues()
{
    /*
     * Based on the name, size, and style, this platform routine calculates
     * the width and height of a digit.
     */
    GetFontInfo(
        m_sFontName, m_iFontSize, 0,
        m_iDigitWidth, m_iDigitHeight, m_iMaxDOWColHdrWidth );

    /*
     * Now we have enough info to compute the size of a minical cell...
     */
    m_iNumRows    = 8;
    m_iNumCols    = 7 + (GetWeeks() ? 1 : 0);
    m_iCellWidth  = (m_iDigitWidth<<1);

    if (m_iMaxDOWColHdrWidth > m_iCellWidth)
        m_iCellWidth = m_iMaxDOWColHdrWidth;

    m_iCellWidth += m_iCellHorzSpacing;
    m_iCellHeight = m_iDigitHeight + m_iCellVertSpacing;
    m_iMCWidth    = m_iNumCols * m_iCellWidth;
    m_iMCHeight   = m_iNumRows * m_iCellHeight;

    /*
     *  Compute the number of minical rows and columns that will fit in the
     *  window we have to work with.
     */
    int iWidth, iHeight;
    GetWindowSize(iWidth,iHeight);
    m_iMCRows = (iHeight + m_iMCVertSpacing) / (m_iMCHeight + m_iMCVertSpacing);
    if (m_iMCRows < 1) m_iMCRows = 1;
    m_iMCCols = (iWidth + m_iMCHorzSpacing) / (m_iMCWidth + m_iMCHorzSpacing);
    if (m_iMCCols < 1) m_iMCCols = 1;

    /*
     *  The actual size is the amount of space taken up by all the
     *  minicals.  This will be the same or less than the window size.
     */
    m_iWidth = m_iMCCols * (m_iMCWidth + m_iMCHorzSpacing) - m_iMCHorzSpacing;
    m_iHeight = m_iMCRows * (m_iMCHeight + m_iMCVertSpacing) - m_iMCVertSpacing;

    /*
     *  Finally, make sure we have enough room in the map buffers.
     */
    return ResizeMapBuffers();
}

/**
 *  Handle events in this controller.
 *
 *  Here's the way this stuff works.  We catch low level events: mouse down, mouse up, 
 *  mouse move.  Depending on what widgets or controls are used to create this controller
 *  we may get full blown events.  This routine breaks up the handling into 3 phases.
 *
 *  Phase 1 - ARM
 *  The first phase handles mouse down. We translate this into a command and ARM the
 *  command. The command depends on where the mouse down happened.  For example, if the 
 *  mouse down happens over a date, we ARM the MDATE command.
 *  
 *  Phase 2 - COMMIT and DISARM
 *  When the mouse up event occurs, we translate the mouse-up point to a command and check
 *  to see if it is the same command as we ARMED. If it is the same, we COMMIT the command.
 *  We disarm any command after a mouse-up has occurred and after any commit has been 
 *  handled.
 *
 *  @param  pe    pointer to the mouse event pinpointing where the date must be determined.
 */
void nsCalMonthContextController::HandleEvent(CMiniCalEvent* pe)
{
    DateTime d;
    SetMousePickInfo(pe);

    /*
     *   PHASE 1  Handle MOUSE DOWN.  Set the armed command...
     */
    if (CMouseEvent::MOUSEDOWN == pe->GetType() || CMouseEvent::MOUSEMOVE == pe->GetType())
    {
        if (CMouseEvent::MOUSEDOWN == pe->GetType())
        {
          m_iCmdArmed = (int)pe->GetAction();
          /*
           * TODO: m_ActionDateList is cleared out by the routine that catches the 
           *       mouse down. That code should be moved to here.
           */
        }

        /*
         * Depending on the command, take any further action necessary
         */
        switch ( pe->GetAction() )
        {
        case CMiniCalEvent::MNOTHING:
            break;
    
        case CMiniCalEvent::MDATE:        
            if ((int)CMiniCalEvent::MNOTHING != m_iCmdArmed)
            {
              HandleDaySelect(pe);
            }
            break;

       
        case CMiniCalEvent::MDOW:
            if ((int)CMiniCalEvent::MNOTHING != m_iCmdArmed)
            {
              HandleColSelect(pe);
            }
            break;

        case CMiniCalEvent::MWEEK:
            if ((int)CMiniCalEvent::MNOTHING != m_iCmdArmed)
            {
              HandleWeekSelect(pe);
            }
            break;

        case CMiniCalEvent::MLEFTARROW:
        case CMiniCalEvent::MRIGHTARROW:
            break;
    
        default:
                /* do nothing */
                break;
        }
    }
    
    /*
     *  PHASES 2 and 3 require MOUSE UP
     */
    if (CMouseEvent::MOUSEUP == pe->GetType())
    {
        switch ( pe->GetAction() )
        {
        case CMiniCalEvent::MNOTHING:
            break;
    
        case CMiniCalEvent::MDATE:        
            if (((int)CMiniCalEvent::MDATE) == m_iCmdArmed)
            {
                HandleDaySelect(pe);
                CommitDateList();
                m_iCmdArmed = (int)CMiniCalEvent::MNOTHING;
                GetWidget()->Invalidate(0);
            }
            break;

        case CMiniCalEvent::MDOW:
            if (((int)CMiniCalEvent::MDOW) == m_iCmdArmed)
            {
                HandleColSelect(pe);
                CommitDateList();
                m_iCmdArmed = (int)CMiniCalEvent::MNOTHING;
                GetWidget()->Invalidate(0);
            }
            break;

        case CMiniCalEvent::MWEEK:
            if (((int)CMiniCalEvent::MWEEK) == m_iCmdArmed)
            {
                HandleWeekSelect(pe);
                CommitDateList();
                m_iCmdArmed = (int)CMiniCalEvent::MNOTHING;
                GetWidget()->Invalidate(0);
            }
            break;

        case CMiniCalEvent::MLEFTARROW:
            if (((int)CMiniCalEvent::MLEFTARROW) == m_iCmdArmed)
            {
                int iDay = 0;
                int iMonth = -1;
                if (0 != pe->GetCtrl())
                {
                  iMonth = 0;
                  iDay = -1;
                }
                else if (0 != pe->GetShift())
                {
                  iMonth = 0;
                  iDay = -7;
                }
                CommitArrow(iDay,iMonth);
                m_iCmdArmed = (int)CMiniCalEvent::MNOTHING;
                GetWidget()->Invalidate(0);
            }
            break;

        case CMiniCalEvent::MRIGHTARROW:
            if (((int)CMiniCalEvent::MRIGHTARROW) == m_iCmdArmed)
            {
                /*
                 *  Standard   = 1 month
                 *  Shift      = 1 week
                 *  Ctrl       = 1 Day
                 *  Shift/Ctrl = 1 Day (we can do something else with this)
                 */
                int iDay = 0;
                int iMonth = 1;
                if (0 != pe->GetCtrl())
                {
                  iMonth = 0;
                  iDay = 1;
                }
                else if (0 != pe->GetShift())
                {
                  iMonth = 0;
                  iDay = 7;
                }
                CommitArrow(iDay,iMonth);
                m_iCmdArmed = (int)CMiniCalEvent::MNOTHING;
                GetWidget()->Invalidate(0);
            }
            break;
    
        default:
            m_iCmdArmed = (int)CMiniCalEvent::MNOTHING;
            break;
        }
    }
}

/**
 *  Take all the dates in m_ActionDateList and notify observers.
 */
void nsCalMonthContextController::CommitDateList()
{
    nsCalDayListCommand * pCmd;
    nsresult res = nsRepository::CreateInstance(kCalDayListCommandCID, nsnull, kXPFCCommandIID, (void **)&pCmd);
    if (NS_OK != res)
        return  ;
    pCmd->Init();
    pCmd->AddDateVector(m_ActionDateList);
    Notify(pCmd);
    NS_IF_RELEASE(pCmd);
}

/**
 *  Move a month forward or backward
 *  @param iMonthVal  set to a positive number to move that many months forward,
 *                    or a negative number to move that many months backwards.
 */
void nsCalMonthContextController::CommitArrow(int iDayVal, int iMonthVal)
{
    nsCalDurationCommand * command;
    nsresult res = nsRepository::CreateInstance(kCalDurationCommandCID, nsnull, kXPFCCommandIID, (void **)&command);
    if (NS_OK != res)
        return;

    nsDuration * duration;
    res = nsRepository::CreateInstance(kCalDurationCID, nsnull, kCalDurationCID, (void **)&duration);
    command->Init(duration);
    command->SetPeriodFormat(nsCalPeriodFormat_kMonth);
    duration->SetMonth(iMonthVal);
    duration->SetDay(iDayVal);
    Notify(command);
    NS_IF_RELEASE(duration);
    NS_IF_RELEASE(command);
}

/**
 *  Set some useful variables for determining where the mouse cursor is.  It also
 *  sets the Action variable in the event.  It does not take action, it just sets the
 *  action to take.
 *
 *  @param  pe    pointer to the mouse event pinpointing where the date must be determined.
 */
void nsCalMonthContextController::SetMousePickInfo(CMiniCalEvent *pe)
{
    m_iTMCW     = m_iMCWidth + m_iMCHorzSpacing;        // total minical width
    m_iTMCH     = m_iMCHeight + m_iMCVertSpacing;       // total minical height;
    m_iPikMCCol = pe->GetX() / m_iTMCW;                 // which column of minicals
    m_iPikMCRow = pe->GetY() / m_iTMCH;                 // which row of minicals
    m_iMCX      = pe->GetX() - m_iPikMCCol * m_iTMCW;   // x coord relative to upper left of m_iPikMC
    m_iMCY      = pe->GetY() - m_iPikMCRow * m_iTMCH;   // y coord relative to upper left of m_iPikMC
    m_iCellCol  = m_iMCX / m_iCellWidth;                // which cell column of m_iPikMC
    m_iCellRow  = m_iMCY / m_iCellHeight;               // which cell row of m_iPikMC

    if (-1 == m_iAnchorMCCol)
    {
        m_iAnchorMCCol = m_iPikMCCol;
        m_iAnchorMCRow = m_iPikMCRow;
    }

    /*---------------------------------------------------------
     *  Now determine what type of event it was...
     *--------------------------------------------------------*/
    pe->SetAction(CMiniCalEvent::MNOTHING);

    /*
     *  CHECK FOR WEEKS
     */
    if ( GetWeeks() && m_iCellCol == 0 && 2 <= m_iCellRow && m_iCellRow <= 7)
    {
      int iStartPos, iStopPos;
      GetValidPos(m_iPikMCRow, m_iPikMCCol, &iStartPos, &iStopPos);
      if ( m_iCellRow - 2 > iStopPos/7 )
      {
        SetCursor(eCursor_standard);
      }
      else
      {
        pe->SetAction(CMiniCalEvent::MWEEK);
        pe->SetWeekOffset(m_iCellRow - 2);
        SetCursor(eCursor_hyperlink);
      }
      return;
    }

    int iCellCol = m_iCellCol;
    if (GetWeeks())
        --iCellCol;

    /*
     *  CHECK FOR ARROWS
     */
    if (GetArrows() && 0 == m_iCellRow && m_iPikMCRow == 0 && m_iPikMCCol == 0)
    {
        // Left Arrow...
        if (nsCalMonthContextController::RIGHT == m_eTitleAlign ||
            nsCalMonthContextController::CENTER == m_eTitleAlign )
        {
            if (m_iCellCol == 0)
            {
                SetCursor(eCursor_hyperlink);
                pe->SetAction(CMiniCalEvent::MLEFTARROW);
                return;
            }
        }
        if (nsCalMonthContextController::LEFT == m_eTitleAlign &&
            m_iCellCol == (5 + (GetWeeks() ? 1 : 0)) )
        {
            SetCursor(eCursor_hyperlink);
            pe->SetAction(CMiniCalEvent::MLEFTARROW);
            return;
        }

        // Right Arrow...
        if (nsCalMonthContextController::LEFT == m_eTitleAlign ||
            nsCalMonthContextController::CENTER == m_eTitleAlign )
        {
            if (m_iCellCol == (6 + (GetWeeks() ? 1 : 0)) )
            {
                SetCursor(eCursor_hyperlink);
                pe->SetAction(CMiniCalEvent::MRIGHTARROW);
                return;
            }
        }
        if (nsCalMonthContextController::RIGHT == m_eTitleAlign && m_iCellCol == 1)
        {
            SetCursor(eCursor_hyperlink);
            pe->SetAction(CMiniCalEvent::MRIGHTARROW);
            return;
        }
    }

    /*
     *  CHECK FOR DOW COLUMNS
     */
    if (m_iCellRow == 1 && iCellCol >= 0 && iCellCol <= 6)
    {
      FindDOW(pe);
      SetCursor(eCursor_hyperlink);
      pe->SetAction(CMiniCalEvent::MDOW);
      return;
    }

    /*
     *  CHECK FOR DATES
     */
    if (2 <= m_iCellRow && m_iCellRow <= 7 && iCellCol >= 0 && iCellCol <= 6)
    {
      if (FindDate(pe))
      {
        FindDOW(pe);
        SetCursor(eCursor_hyperlink);
        pe->SetAction(CMiniCalEvent::MDATE);
      }
      else
      {
        SetCursor(eCursor_standard);
      }
      return;
    }

    SetCursor(eCursor_standard);
}

/**
 *  Based on the date, set the upper left coordinate of the cell
 *  @param d       the date for which coordinates will be determined.
 *  @param iCellX  left coordinate of cell where date is located, -1 if date is not found
 *  @param iCellY  top coordinate of cell where date is located, -1 if date is not found
 */
void nsCalMonthContextController::SetDateCoords(DateTime d, int& iCellX, int& iCellY)
{
    PRBool bFound = 0;
    int iDay     = d.getDate();
    int iMonth   = d.getMonth();
    int iYear    = d.getYear();

    DateTime dd  = GetDate();
    iCellX     = iCellY = -1;       // assume it's not found

    //------------------------------------------------
    // Which Minical is it in?
    //------------------------------------------------
    int i=0,j=-1;
    for (i = 0; i < m_iMCRows; i++)
    {
        for (j = 0; j < m_iMCCols; j++)
        {
            if (dd.getMonth() == iMonth &&
                dd.getYear() == iYear )
            {
                bFound = TRUE;
                break;
            }
            else
                dd.nextMonth();
        }
        if (bFound) break;
    }

    if (!bFound) return;        // if we didn't find it, bail out now

    //------------------------------------------------
    // Track down the location...
    //------------------------------------------------
    iCellX = j * (m_iMCWidth + m_iMCHorzSpacing);
    iCellY = i * (m_iMCHeight + m_iMCVertSpacing);
    dd.setDayOfMonth(1);

    SetFDOW(dd);
    bFound = FALSE;
    for (i = 0; i < 6; i++)
    {
        for (j = 0; j < 7; j++)
        {
            if ( dd.get(Calendar::DAY_OF_MONTH) == iDay  &&
                 dd.get(Calendar::YEAR) == iYear  &&
                 dd.get(Calendar::MONTH) == iMonth )
            {
                nsRect rect;

                iCellX += (j + (GetWeeks()?1:0)) * m_iCellWidth;
                iCellY += (2 + i) * m_iCellHeight;

                GetBounds(rect);
                iCellX += rect.x;
                iCellY += rect.y;

                return;
            }
            dd.nextDay();
        }
    }
    iCellX = iCellY = -1;   // didn't find it
}

/**
 *  Add the days for this column to m_ActionDateList. Highlight the dates
 *  @param pe  the event
 */
nsresult nsCalMonthContextController::HandleColSelect(CMiniCalEvent *pe)
{
  nsresult res = GetDOWList(pe->GetDOW());

  // XXX What happens if we fail?
  if (NS_OK != res)
    return res;

  HighlightActionList();
  return NS_OK;
}

/**
 *  Add the days for this column to m_ActionDateList. Highlight the dates
 *  @param pe  the event
 *  @return NS_OK
 */
nsresult nsCalMonthContextController::HandleWeekSelect(CMiniCalEvent *pe)
{
  GetWeekDateList(pe->GetWeekOffset());
  HighlightActionList();
  return NS_OK;
}

/**
 *  Add the days for this column to m_ActionDateList. Highlight the dates
 *  @param pe  the event
 *  @return NS_OK
 */
nsresult nsCalMonthContextController::HandleDaySelect(CMiniCalEvent *pe)
{
  Date d(pe->GetDate().getTime() );
  AddToActionList( d );
  HighlightActionList();
  return NS_OK;
}

/**
 *  A comparison routine for nsDateTime values...
 *  @param v1 pointer to a pointer to the first nsDateTime
 *  @param v2 pointer to a pointer to the second nsDateTime
 *  @return  -1 if D1 < D2, 0 if D1 == D2,  1 if D1 > D2
 */
PRInt32 nsDateTime_SortAscending( const void* v1, const void* v2 )
{
    nsDateTime *p1 = *(nsDateTime**)v1;
    nsDateTime *p2 = *(nsDateTime**)v2;

    DateTime *pD1 = p1->GetDateTime();
    DateTime *pD2 = p2->GetDateTime();

    if (*pD1 == *pD2)
        return 0;
    else if (*pD1 > *pD2)
        return 1;
    else
        return -1;
}

/**
 *  The ActionList has been updated. Refresh the screen...
 */
nsresult nsCalMonthContextController::HighlightActionList()
{
    nsDateTime* pDate;

    /*
     * Create a Rendering Context for highlighting
     *
     * XXX:  Get rid of mRenderingContext
     */
    nsIRenderingContext * prevContext = nsnull;
    if (mRenderingContext != nsnull) 
    {
        prevContext = mRenderingContext;
    }
    else
    {            
        mRenderingContext = GetWidget()->GetRenderingContext();
        InitRedraw((void *)mRenderingContext);
    }
    
    /*
     *  Highlight the dates we found.
     */
    nsIIterator * iterator;
    m_ActionDateList->CreateIterator(&iterator);
    iterator->Init();
    DateTime another_datetime;

    while(!(iterator->IsDone())) 
    {
        pDate = (nsDateTime *) iterator->CurrentItem();
        another_datetime.setTime(pDate->GetDateTime()->getTime());
        HighlightCell(another_datetime,GetHighlightFGColor(),GetHighlightBGColor());
        iterator->Next();
    }
    NS_IF_RELEASE(iterator);
    
    if (prevContext == nsnull) 
    {
        NS_IF_RELEASE(mRenderingContext);    
        mRenderingContext = nsnull;
    }
    return NS_OK;
}

nsresult nsCalMonthContextController::AddToActionList(DateTime d)
{
    nsDateTime * nsdatetime;
    nsresult res = nsRepository::CreateInstance(kCalDateTimeCID,nsnull, kCalDateTimeCID, (void **)&nsdatetime);
    if (NS_OK == res) 
    {
        nsdatetime->Init();
        DateTime * dstar = new DateTime(d.getTime());
        nsdatetime->SetDateTime(dstar);
        if ( -1 == m_ActionDateList->InsertBinary(nsdatetime,(nsArrayCompareProc)nsDateTime_SortAscending, PR_FALSE /* do not allow dups */))
        {
            NS_IF_RELEASE(nsdatetime);
        }
    }
    return res;
}

/**
 *  Build a list of dates for the supplied month and the supplied day of the week.
 *  Note that the caller must delete everything.
 *
 *  @param  iDOW  the day of the week constant.  For example, Calendar.MONDAY
 *  @return       the vector of dates
 */
nsresult nsCalMonthContextController::GetDOWList(int iDOW)
{
    DateTime d;
    d.setTime(GetStartTimeMap(m_iPikMCRow,m_iPikMCCol));

    /*
     * If we're on a day of the previous month, move forward until we hit the
     * actual month.  If the day number is > 7 then we *must* be on the previous month
     */
    while (7 < d.getDate())
        d.nextDay();

    /*
     * Now move forward until we get to the day of the week that the user selected...
     */
    while ( iDOW != d.get(Calendar::DAY_OF_WEEK))
        d.nextDay();

    /*
     * Now collect all the days of this month that fall on the selected day of the week
     */
    for (int iMonth = d.getMonth(); iMonth == d.getMonth(); d.add(Calendar::DATE,7) ) 
        AddToActionList(d);

    return NS_OK;
}

/**
 *  Build a vector of dates for the week at the supplied offset in the selected minical.
 *  @param  iWeekOffset  the day of the week constant.  For example, Calendar::MONDAY
 *  @return NS_OK
 */
nsresult nsCalMonthContextController::GetWeekDateList(int iWeekOffset)
{
    DateTime d;
    d.setTime(GetStartTimeMap(m_iPikMCRow,m_iPikMCCol));

    int i;
    for (i = 1; i <= iWeekOffset; i++)
        d.add(Calendar::DATE, 7);
    for (i = 0; i < 7; d.nextDay(), i++ )
        AddToActionList(d);
    return NS_OK;
}


#if 0
/**
 *  Update the minical based on the date found under the latest mouse event.
 *  @param T   The date under the mouse
 */
void nsCalMonthContextController::trackMouseEvent(DateTime T)
{
    DateTime A = m_AnchorDate;
    DateTime L = m_LastTrackDate;

    if (null == m_AnchorDate)
    {
        m_AnchorDate = T;
        m_LastTrackDate = T;
        addToDateList( T );
        return;
    }

    /*
     *  Key:
     *    A = anchor date
     *    L = last track date
     *    T = current tracking date
     */
    if (L.equals(T))
        return;
    else if ( T.before(L) && A.equals(L) )                      /*    T  -  AL  */
        addToDateList( expandDateList( T,L.prevDay() ) );
    else if ( A.equals(L) && L.before(T) )                      /*    AL -  T   */
        addToDateList( expandDateList( L.nextDay(),T ) );
    else if ( L.before(A) && A.equals(T) )                      /*    L  -  AT  */
        removeFromDateList( expandDateList( L, A.prevDay() ) );
    else if ( A.equals(T) && A.before(L) )                      /*    AT -  L  */
        removeFromDateList( expandDateList( A.nextDay(),L ) );
    else if ( A.before(L)  &&  L.before(T) )                    /*    A  -  L  -  T  */
        addToDateList( expandDateList( L.nextDay(),T ) );
    else if ( A.before(T)  &&  T.before(L) )                    /*    A  -  T  -  L  */
        removeFromDateList( expandDateList( T.nextDay(),L ) );
    else if ( L.before(T)  &&  T.before(A) )                    /*    L  -  T  -  A  */
        removeFromDateList( expandDateList( L, T.prevDay() ) );
    else if ( T.before(L)  &&  L.before(A) )                    /*    T  -  L  -  A  */
        addToDateList( expandDateList( T, L.prevDay() ) );
    else if ( T.before(A)  &&  A.before(L) )                    /*    T  -  A  -  L  */
    {
        removeFromDateList( expandDateList( A.nextDay(), L ) );
        addToDateList( expandDateList( T, A.prevDay() ) );
    }
    else if ( L.before(A)  &&  A.before(T) )                    /*    L  -  A  -  T  */
    {
        removeFromDateList( expandDateList( L, A.prevDay() ) );
        addToDateList( expandDateList( A.nextDay(), T ) );
    }

    m_LastTrackDate = T;
}
#endif

/**
 *  Based on the supplied date, move the date backwards until
 *  it is on the desired first day of the week.
 *  @param d The date from which to back off
 */
void nsCalMonthContextController::SetFDOW(DateTime &d)
{
    int i = d.get(Calendar::DAY_OF_WEEK);
    while ( i != GetFDOW())
    {
        if ( i == Calendar::SUNDAY )
            i = Calendar::SATURDAY;
        else
            i--;
        d.prevDay();
        i = d.get(Calendar::DAY_OF_WEEK);
    }
}

/**
 *  Make sure there's enough room in the map buffers to hold entries for the 
 *  current number of rows and columns.  If not, reallocate some memory.
 *
 *  @return 0 = success, 1 = failure
 */
int nsCalMonthContextController::ResizeMapBuffers()
{
    int iMatrixSize = m_iMCCols * m_iMCRows;  // number of entries
    if (iMatrixSize < 25)
        iMatrixSize = 25;                     // minimum is for 5 rows 5 cols
    if (m_iMatrixSize >= iMatrixSize)
        return 0;                             // return if current buffers are big enough

    if (m_pStartTimeMap)
        PR_Free(m_pStartTimeMap);
    if (m_pValidPosMap)
        PR_Free(m_pValidPosMap);

    m_iMatrixSize = 0;                        // be pessimistic
    int iSize = iMatrixSize * sizeof(long);
    if (0 == (m_pStartTimeMap = (Date *)PR_Malloc(iSize)))
        return 1;

    iSize = iMatrixSize * sizeof(int);
    if (0 == (m_pValidPosMap = (int *)PR_Malloc(iSize)))
        return 1;

    m_iMatrixSize = iMatrixSize;              // success!
    return 0;
}

/**
 *  The entries in this matrix contain the valid start and stop cell positions
 *  in the minical at the specified row and column.  This routine returns those
 *  positions.
 *
 */
void nsCalMonthContextController::GetValidPos(int iRow, int iCol, int* piPosStart, int* piPosStop)
{
    int iVal = m_pValidPosMap[iRow * m_iMCCols + iCol];
    *piPosStart = iVal & 0xff;
    *piPosStop  = (iVal >> 8) & 0xff;
}

/**
 *  The entries in this matrix contain the valid start and stop cell positions
 *  in the minical at the specified row and column.  This routine sets the value
 *  at the supplied position
 */
void nsCalMonthContextController::SetValidPos(int iRow, int iCol, int iPosStart, int iPosStop)
{
    m_pValidPosMap[iRow * m_iMCCols + iCol] = (0xff & iPosStart) | ((0xff & iPosStop) << 8);
}


/**
 *  Determine the date that the mouse is over.  SetMousePickInfo must have been
 *  called prior to calling this routine.  Its calculations are based on its
 *  calculations.  If the pick is found to be outside the valid date range, this
 *  routine sets the Action to MNOTHING.  If a date is found, it is set in
 *  the Date slot for the CMiniCalEvent.
 *
 *  @return  1 if a date is found, 0 if no date is found
 */
int nsCalMonthContextController::FindDate(CMiniCalEvent* e)
{
    e->SetAction(CMiniCalEvent::MNOTHING);  // assume we don't find anything

    if (m_iPikMCCol >= m_iMCCols || m_iPikMCRow >= m_iMCRows)
        return 0;

    int iPos, iStartPos,iStopPos;
    int iCellCol = m_iCellCol;
    int iCellRow = m_iCellRow;

    /*
     *  Info for quick reject check.  If iRow,iCol does not point to a cell containing valid
     *  dates for this month, reject it.
     */
    GetValidPos(m_iPikMCRow, m_iPikMCCol, &iStartPos, &iStopPos);

    if (m_iPikMCRow < m_iMCRows && m_iPikMCCol < m_iMCCols)
    {
        DateTime d( GetStartTimeMap(m_iPikMCRow,m_iPikMCCol) );

        iCellRow -= 2;      // first 2 rows are month/year and weekday names
        if (GetWeeks())
            iCellCol -= 1;  // if weeks are on, first col is week number

        if (iCellRow < 6 && iCellRow >= 0 &&
            iCellCol < 7 && iCellCol >= 0 )
        {
            //---------------------------------------------------------------
            // Last check, make sure that this cell is showing a date...
            //---------------------------------------------------------------
            iPos = iCellRow*7 + iCellCol;
            if (iPos < iStartPos || iPos > iStopPos)
                return 0;

            //---------------------------------------------------------------
            // We got it!  It's a valid date.  Add it to the selected list.
            //---------------------------------------------------------------
            int iOffset = 7 * iCellRow + iCellCol;
            d.add(Calendar::DAY_OF_YEAR, iOffset);
            e->SetAction(CMiniCalEvent::MDATE);
            e->SetDate(d);
            return 1;
        }
    }
    return 0;
}

/**
 * Determine the day of the week that the mouse is over
 * sets the FDOW field in the event to the dow, -1 means no match
 * @param  e    the mouse event pinpointing where the dow must be determined.
 */
void nsCalMonthContextController::FindDOW(CMiniCalEvent* e)
{
    int iCellCol = m_iCellCol;

    if (GetWeeks())
        iCellCol -= 1;  // if weeks are on, first col is week number
    int i, iDOW;
    for (iDOW = GetFDOW(), i = 0; i <= iCellCol; i++)
    {
        if (i == iCellCol)
        {
            e->SetDOW(iDOW);
            return;
        }
        if (iDOW == Calendar::SATURDAY)
            iDOW = Calendar::SUNDAY;
        else
            iDOW++;
    }
    return;
}

/**
***  Reset the internal array of names for fast painting.
***
**/
void nsCalMonthContextController::SetDOWNameArray(PRBool bLocaleChange)
{
    UnicodeString sFmt("EEE");
    char sBuf[256];
    
    /*
     *  Did first day of week change?
     */
    int iFDOW = GetFDOW();
    if (m_iFDOW != iFDOW || bLocaleChange)
    {
        /*
         *  Something has changed. Update the strings...
         */
        sFmt = "EEE";
        int iMaxChars = GetDOWColHdrChars();
        int iLen = iMaxChars + 1;
        m_iFDOW = iFDOW;
        DateTime d;
        d.prevDOW(GetFDOW());
        for ( int i = 0; i < 7; i++)
        {
            if (m_asDOW[i])
                PR_Free(m_asDOW[i]);
            m_asDOW[i] = (char *) PR_Malloc(iLen);
            sprintf(sBuf,"%s", d.strftime(sFmt,GetLocale()).toCString("")); 
            strncpy(m_asDOW[i],sBuf,iMaxChars);
            (m_asDOW[i])[iMaxChars] = 0;
            d.nextDay();
        }
    }
}

/**
 *  The locale changed. Reload the months array...
 */
void nsCalMonthContextController::SetMonthsArray()
{
    UnicodeString sFmt("MMMM");
    char sBuf[256];
    DateTime d(1998,0,1);
    int iLen;

    for (int i = 0; i < 12; i++)
    {
        if (m_asMonths[i])
            PR_Free(m_asMonths[i]);
        sprintf(sBuf,"%s",d.strftime(sFmt,GetLocale()).toCString("") );
        iLen = strlen(sBuf);
        m_asMonths[i] = (char *) PR_Malloc(iLen+1);
        strcpy(m_asMonths[i],sBuf);
        (m_asMonths[i])[iLen] = 0;
        d.nextMonth();
    }
}


nsString nsCalMonthContextController::ToString()
{
    nsString sVal = "MiniCal \r\n";
    sVal += " Grid=";
    sVal += m_bGrid ? "on" : "off";
    sVal += "\r\n";

    sVal += " Sep=";
    sVal += m_bSep ? "on" : "off";
    sVal += "\r\n";

    sVal += " Weeks=";
    sVal += m_bWeeks ? "on" : "off";
    sVal += "\r\n";

    return sVal;
}

void nsCalMonthContextController::SetFDOW(int i)
{
    m_iFDOW = i;
    SetDOWNameArray(0); 
}


void nsCalMonthContextController::SetDefaults()
{
    m_sFontName = "Arial";  // "Times New Roman";
}


/////////////////////////////////////////////////////////////////////////////
// CMiniCalPlatform message handlers


nsEventStatus nsCalMonthContextController :: OnPaint(nsIRenderingContext& aRenderingContext,
                                                     const nsRect& aDirtyRect)
{
    mRenderingContext = &aRenderingContext;

    PushState(aRenderingContext);

    PaintBackground(aRenderingContext,aDirtyRect);

    DrawMiniCal((void *)mRenderingContext);

    PopState(aRenderingContext);

    mRenderingContext = nsnull;

    return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsCalMonthContextController :: OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    GetWidget()->Resize(0,0,aWidth,aHeight, PR_TRUE);
    return nsEventStatus_eConsumeNoDefault;
}

/**
 *  Use this routine to set up any variables needed to 
 *  redraw.  It is called by the bridge at the beginning of
 *  the redraw. It should be called once prior to drawing
 *  or highlighting.
 *
 *  @param p    the value m_pBridge->DrawMiniCal was called with.
 *              currently it is a pointer to the DC to use.
 *              
 */
void nsCalMonthContextController::InitRedraw(void* /*p*/)
{
    SetPaintValues();
    mRenderingContext->SetFont(GetFont());
}

/**
 *  Use this routine to clean up any variables after
 *  the redraw.
 *
 *  @param p    the value m_pBridge->DrawMiniCal was called with.
 *              currently it is a pointer to the DC to use
 */
void nsCalMonthContextController::PostRedraw(void* /*p*/)
{
}


/**
 *  Set the color used to draw text
 *
 *  @param p        character string to print
 *  @param iWidth   the width of the string
 *  @param iHeight  the height of the string
 */
void nsCalMonthContextController::GetTextExtent(char *p, int iChars, int& iWidth,int& iHeight)
{
  nsIFontMetrics * fm ;
   
  fm = mRenderingContext->GetFontMetrics();

  mRenderingContext->GetWidth(p,(nscoord)iWidth);
  fm->GetHeight((nscoord)iHeight);

}


/**
***  The bridge part calls this method when it needs to get
***  font metrics. We keep the local cached copy of the 
***  font name, size, and style up-to-date.  We load a new
***  font only if necessary.
**/
void nsCalMonthContextController::GetFontInfo(
        nsString s, int iFontSize, int /*iStyle*/,   // input
        int& riDigitWidth, int& riDigitHeight,      // output
        int& riMaxDOWColHdrWidth )                  // output
{

    /*
	 *  If anything about the font has changed, update the font...
	 */
    if (s != m_sFontName || iFontSize != m_iFontSize)
    {
        m_sFontName = s;
		m_iFontSize = iFontSize;
    }

    /*
     *  Set the font and associated values if necessary...
     */
	nsSize te;
    //if (NULL == mRenderingContext->GetFont())
    if (1)
    {
        mRenderingContext->SetFont(GetFont());

    	/*
         *  The minimal cell width is equal to the greater of:
         *     a) the amount of space required by the widest day-of-the-week (DOW) col hdr
         *     b) the width of two digits
         *  We must ask the bridge for the number of characters we are to show
         *  in the DOW col header and compute its width.
         */

        nsIRenderingContext * ctx = mRenderingContext;

	    nsFont font(m_sFontName, NS_FONT_STYLE_NORMAL,
		        NS_FONT_VARIANT_NORMAL,
		        NS_FONT_WEIGHT_NORMAL,
		        0,
		        12);

        ctx->SetFont(font);

        nsIFontMetrics * fm = ctx->GetFontMetrics();

        ctx->GetWidth("0",m_iDigitWidth);
        fm->GetHeight(m_iDigitHeight);

        /*
         *  The next few lines need to be replaced with some code that gets the 
         *  strings for every day of the week and computes the max width...
         */
        char sBuf[100];
        strcpy(sBuf,"Wednesday");
        sBuf[GetDOWColHdrChars() ] = 0;

        ctx->GetWidth(sBuf,m_iMaxDOWColHdrWidth);

    }

    /*
     *  Set measurement constants that are based on the font
     *  based on our cached values.
     */
    riDigitWidth        = m_iDigitWidth;
    riDigitHeight       = m_iDigitHeight;
    riMaxDOWColHdrWidth = m_iMaxDOWColHdrWidth;
}

/**
***  Return the current size of this window
**/
void nsCalMonthContextController::GetWindowSize(int& riWidth, int& riHeight)
{
    nsRect rect;
    GetBounds(rect);
    riWidth = rect.width;
    riHeight =rect.height;
}

/**
 *  clear out the action date list
 */
void nsCalMonthContextController::ClearActionDateList()
{
    if (m_ActionDateList != nsnull) 
    {
        nsIIterator * iterator = nsnull;
        nsresult res = m_ActionDateList->CreateIterator(&iterator);
        if (NS_OK == res) 
        {
            iterator->Init();
            while(!(iterator->IsDone()))
            {          
                nsDateTime * d = (nsDateTime *)iterator->CurrentItem();
                NS_IF_RELEASE(d);
                iterator->Next();
            }
            m_ActionDateList->RemoveAll();
        }
        NS_IF_RELEASE(iterator);
    }
}


nsEventStatus nsCalMonthContextController::OnLeftButtonDown(nsGUIEvent *aEvent) 
{    
    nsRect rect;
    GetBounds(rect);
    
    nsInputEvent * input_event = (nsInputEvent *) aEvent;

    /*
     * Clean out the current DateList if shift/control not down
     */

    if ((input_event->isShift == PR_FALSE) && (input_event->isControl == PR_FALSE))
    {
        ClearActionDateList();
    }


    CMiniCalEvent e( (int)aEvent->point.x - rect.x, (int)aEvent->point.y - rect.y, CMouseEvent::MOUSEDOWN, 1, 0 );
    if (input_event->isShift)
      e.SetShift();
    if (input_event->isControl)
      e.SetCtrl();
    HandleEvent( &e );

    return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsCalMonthContextController::OnLeftButtonUp(nsGUIEvent *aEvent) 
{    
    nsInputEvent * input_event = (nsInputEvent *) aEvent;
    nsRect rect;
    GetBounds(rect);
    CMiniCalEvent e( (int)aEvent->point.x - rect.x, (int)aEvent->point.y - rect.y, CMouseEvent::MOUSEUP, 1, 0 );
    if (input_event->isShift)
      e.SetShift();
    if (input_event->isControl)
      e.SetCtrl();
    HandleEvent( &e );
    return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsCalMonthContextController::OnMouseMove(nsGUIEvent *aEvent)
{ 
    nsInputEvent * input_event = (nsInputEvent *) aEvent;
    nsRect rect;
    GetBounds(rect);
    CMiniCalEvent e( (int)aEvent->point.x - rect.x, (int)aEvent->point.y - rect.y, CMouseEvent::MOUSEMOVE, 1, 0 );
    if (input_event->isShift)
      e.SetShift();
    if (input_event->isControl)
      e.SetCtrl();
    HandleEvent( &e );
    return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsCalMonthContextController::Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand)
{
  return (nsXPFCCanvas::Update(aSubject,aCommand));
}

/*
 * After some event occurs and modification is needed to this ontext controller,
 * adjust our DateTime object to reflect the event type.
 *
 */

nsEventStatus nsCalMonthContextController::Action(nsIXPFCCommand * aCommand)
{
    /*
     *  Adjust the duration if this is a duration command
     */

    nsCalDurationCommand * duration_command;
    nsDuration * dtDuration;
    nsresult res;

    static NS_DEFINE_IID(kCalDurationCommandCID, NS_CAL_DURATION_COMMAND_CID);                 

    /*
     *  DURATION 
     */
    res = aCommand->QueryInterface(kCalDurationCommandCID,(void**)&duration_command);
    if (NS_OK == res)
    {
      dtDuration = duration_command->GetDuration();
      if (nsnull == dtDuration)
        return nsEventStatus_eIgnore;

      DateTime d = GetDate();
      d.add(dtDuration->GetDuration());
      SetDate(d);

      for (int i = 0; i < m_pDates->GetSize(); i++)
          ((DateTime*)m_pDates->GetAt(i))->add(dtDuration->GetDuration());

      return nsEventStatus_eIgnore;
    }
  

    /*
     * DAYLIST
     */
    nsCalDayListCommand * daylist_command ;
    static NS_DEFINE_IID(kCalDayListCommandCID, NS_CAL_DAYLIST_COMMAND_CID);                 
    res = aCommand->QueryInterface(kCalDayListCommandCID,(void**)&daylist_command);
    if (NS_OK == res)
    {
      nsIIterator * iterator;
      res = daylist_command->CreateIterator(&iterator);
      int iDateSet = 0;

      if (NS_OK != res)
        return nsEventStatus_eIgnore;
      iterator->Init();
      ClearDateList();    /* empty the current date list first */
      while(!(iterator->IsDone()))
      {
        nsDateTime * datetime = (nsDateTime *) iterator->CurrentItem();
        if (datetime != nsnull) 
        {
          /*
           * XXX: TBD Use one array class for everything
           *
           * Copy into m_pDates from the Command Object
           */
          DateTime * dt = new DateTime(datetime->GetDateTime()->getTime());        
          if (0 == iDateSet)
          {
            SetDate( *dt );
            iDateSet = 1;
          }
          m_pDates->Add((void *) dt);        
        }
        iterator->Next();
      }
      return nsEventStatus_eIgnore;
    }

    return(nsXPFCCanvas::Action(aCommand));
}

nsresult nsCalMonthContextController :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

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

#ifndef nsCalMonthContextController_h___
#define nsCalMonthContextController_h___

#include "nsCalContextController.h"

#include "jdefines.h"
#include "datetime.h"
#include <locid.h>

#include "nsIWidget.h"
#include "nsIFontMetrics.h"
#include "nsString.h"

class DateTime;
class CMouseEvent;
class CMiniCalEvent;
class JulianPtrArray;

class nsCalMonthContextController : public nsCalContextController
{

public:
    nsCalMonthContextController(nsISupports* outer);
    NS_DECL_ISUPPORTS
    NS_IMETHOD Init();
    NS_IMETHOD GetClassPreferredSize(nsSize& aSize);

protected:
    ~nsCalMonthContextController();

public:
	enum EALIGNMENT {LEFT, RIGHT, CENTER};

protected:

	/*
	 *  Data members defining state and "non-ui" aspects of the the MiniCal
	 */
    PRBool         m_bGrid;               // show the grid?
    PRBool         m_bSep;                // show separator below day of week column headings
    PRBool         m_bWeeks;              // show weeks?
    PRBool         m_bColumnSelectMode;   // select by rows or columns?
    PRBool         m_bArrows;             // are arrows enabled
    DateTime       m_ULDate;			        // represents the month that is shown in the upper left corner
    DateTime       m_Date;                // the currently selected date
    
    nscolor        m_MCBGColor;           // background color for minicals
    nscolor        m_DIMFGColor;          // background color for the dates before and after current month
    nscolor        m_CurDayFGColor;       // foreground for current date
    nscolor        m_CurDayBGColor;       // background for current date
    nscolor        m_HighlightFGColor;    // foreground for current date
    nscolor        m_HighlightBGColor;    // background for current date
    int            m_iFDOW;               // what day do we show as first-day-of-the-week

public:

    inline PRBool  GetArrows()                                { return m_bArrows; }
    inline void    SetArrows(PRBool b)                        { m_bArrows = b;    }
    inline PRBool  GetGrid()                                  { return m_bGrid;   }
    inline void    SetGrid(PRBool b)                          { m_bGrid = b;      }
    inline PRBool  GetSeparator()                             { return m_bSep;    }
    inline void    SetSeparator(PRBool b)                     { m_bSep = b;       }
    inline PRBool  GetWeeks()                                 { return m_bWeeks;  }
    inline void    SetWeeks(PRBool b)                         { m_bWeeks = b;     }
    inline int     GetFDOW()                                  { return m_iFDOW;   }
    void           SetFDOW(int i);
	  DateTime&      GetDate()                                  { return m_ULDate;  }
	  inline void    SetDate(DateTime d)                        { m_ULDate = d;     }
  
    inline void    SetMCBGColor(int r, int g, int b)          { m_MCBGColor = NS_RGB(r,g,b); }
    inline nscolor GetMCBGColor()                             { return m_MCBGColor; }

    inline void   SetDIMFGColor(int r, int g, int b)          { m_DIMFGColor = NS_RGB(r,g,b); }
    inline nscolor GetDIMFGColor()                            { return m_DIMFGColor; }
    inline void    SetCurrentDayFGColor(int r, int g, int b)  { m_CurDayFGColor = NS_RGB(r,g,b); }
    inline nscolor GetCurrentDayFGColor()                     { return m_CurDayFGColor; }
    inline void    SetCurrentDayBGColor(int r, int g, int b)  { m_CurDayBGColor = NS_RGB(r,g,b); }
    inline nscolor GetCurrentDayBGColor()                     { return m_CurDayBGColor; }
    inline void    SetHighlightFGColor(int r, int g, int b)   { m_HighlightFGColor = NS_RGB(r,g,b); }
    inline nscolor GetHighlightFGColor()                      { return m_HighlightFGColor; }
    inline void    SetHighlightBGColor(int r, int g, int b)   { m_HighlightBGColor = NS_RGB(r,g,b); }
    inline nscolor GetHighlightBGColor()                      { return m_HighlightBGColor; }

    nsString       ToString();

protected:
    EALIGNMENT m_eTitleAlign;	// how the is title aligned

    Locale m_Locale;            // in what local are we working
    int m_iFontSize;            // point size of font
    int m_iDOWChars;            // number of chars to show in the day of the week col hdr
    int m_iDigitWidth;          // width of a digit 0 - 9
    int m_iMaxDOWColHdrWidth;   // max width of dow col hdr string
    int m_iDigitHeight;         // height of a digit
    
    int m_iNumRows;             // number of cell rows per minical
    int m_iNumCols;             // number of cell cols per minical
    int m_iCellWidth;           // total width of a minical cell
    int m_iCellHeight;          // total height of a minical cell
    int m_iCellHorzSpacing;     // horizontal space in addition to the character width in a cell
    int m_iCellVertSpacing;     // vertical space in addition to the character height
    
    int m_iMCWidth;             // total width of a single minical
    int m_iMCHeight;            // total height of a single minical
    int m_iMCVertSpacing;       // vertical spacing between minicals
    int m_iMCHorzSpacing;       // horizontal spacing between minicals
    int m_iMCRows;              // number of rows of minicals for current window height
    int m_iMCCols;              // number of columns of minicals for current window width

    int m_iWidth;               // combined width of all minicals
    int m_iHeight;              // combined height of all minicals

    int m_iTMCW;                // total minical width
    int m_iTMCH;                // total minical height;
    int m_iPikMCCol;            // which column of minicals
    int m_iPikMCRow;            // which row of minicals
    int m_iMCX;                 // x coord relative to upper left of m_iPikMC
    int m_iMCY;                 // y coord relative to upper left of m_iPikMC
    int m_iCellCol;             // which cell column of m_iPikMC
    int m_iCellRow;             // which cell row of m_iPikMC
    int m_iAnchorMCCol;         // column in which the drag was anchored
    int m_iAnchorMCRow;         // row in which the drag was anchored
    int m_iYOffset;
    int m_iXOffset;             // this far from the right edge of the cell.
    int m_iCmdArmed;            // mouse down started on this command

    char* m_asDOW[7];           // array holding the days of the week names
    char* m_asMonths[12];       // names for the months of the year

    int   m_iMatrixSize;        // the buffers below are valid for m_iMCRows*m_iMCCols is less that the value of this number
    Date* m_pStartTimeMap;      // buffer for a matrix of start times corresponding to the first day in each minical
    int*  m_pValidPosMap;       // buffer for a matrix of start/stop valid cell positions
	JulianPtrArray *m_pDates;       // Selected dates

    /*
     *  Internal methods
     */
    int   ResizeMapBuffers();
    void  SetFDOW(DateTime &d);

public:
    /*
     *  General requests for the Bridge...
     */
    void            DrawMiniCal(void* p);
    void            RemoveSelectedDate(DateTime d);
    void            UnhighlightDate(DateTime d);
	  void            SetDateCoords(DateTime d, int& iCellX, int& m_iCellY);
    void            HandleEvent(CMiniCalEvent* pe);
    void            SetMousePickInfo(CMiniCalEvent* e);
    nsresult        HandleColSelect(CMiniCalEvent* pe);
    nsresult        HandleWeekSelect(CMiniCalEvent* pe);
    nsresult        HandleDaySelect(CMiniCalEvent* pe);
    NS_IMETHOD      GetDOWList(int iDOW);
    int             FindDate(CMiniCalEvent* e);
    void            FindDOW(CMiniCalEvent* e);
    int             SetPaintValues();
    inline int      GetFontSize()	              { return m_iFontSize;       }
    inline void     SetFontSize(int i)          { m_iFontSize = i;          }
    inline nsString GetFontName()               { return m_sFontName;       }
    inline void     SetFontName(nsString s)     { m_sFontName = s;          }
    inline int      GetMCVertSpacing()          { return m_iMCVertSpacing;  }
    inline void     SetMCVertSpacing(int i)     { m_iMCVertSpacing = i;     }
    inline int      GetMCHorzSpacing()          { return m_iMCHorzSpacing;  }
    inline void     SetMCHorzSpacing(int i)     { m_iMCHorzSpacing = i;     }
    inline int      GetCellVertSpacing()        { return m_iCellVertSpacing;}
    inline void     SetCellVertSpacing(int i)   { m_iCellVertSpacing = i;   }
    inline int      GetCellHorzSpacing()        { return m_iCellHorzSpacing;}
    inline void     SetCellHorzSpacing(int i)   { m_iCellHorzSpacing = i;   }
    inline int      GetDOWColHdrChars()         { return m_iDOWChars;       }
    inline void     SetDOWColHdrChars(int i)    { m_iDOWChars = i;          }
    inline int      GetWidth()                  { return m_iWidth;          }
    inline int      GetHeight()                 { return m_iHeight;         }
    inline int      GetCellWidth()              { return m_iCellWidth;      }
    inline int      GetCellHeight()             { return m_iCellHeight;     }
    inline int      GetMCWidth()                { return m_iMCWidth;        }
    inline int      GetMCHeight()               { return m_iMCHeight;       }
    inline int      GetMCRows()                 { return m_iMCRows;         }
    inline int      GetMCCols()                 { return m_iMCCols;         }
    inline int      GetCellRows()               { return m_iNumRows;        }
    inline int      GetCellCols()               { return m_iNumCols;        }
    inline int      GetTMCW()                   { return m_iTMCW;           }
    inline int      GetTMCH()                   { return m_iTMCH;           }
    inline int      GetPikMCCol()               { return m_iPikMCCol;       }
    inline int      GetPikMCRow()               { return m_iPikMCRow;       }
    inline int      GetMCX()                    { return m_iMCX;            }
    inline int      GetMCY()                    { return m_iMCY;            }
    inline int      GetCellCol()                { return m_iCellCol;        }
    inline int      GetCellRow()                { return m_iCellRow;        }
    inline int      GetAnchorMCCol()            { return m_iAnchorMCCol;    }
    inline int      GetAnchorMCRow()            { return m_iAnchorMCRow;    }
    inline void     SetLocale(const Locale l)   { m_Locale = l; SetDOWNameArray(1); SetMonthsArray(); }
    Locale&         GetLocale()                 { return m_Locale;          }
    inline char**   GetDOWArray()               { return m_asDOW;           }
    inline char**   GetMonthArray()             { return m_asMonths;        }
    inline JulianPtrArray* GetDateList()        { return m_pDates;          }
    
    void            SetSelectedDates(JulianPtrArray* pV);
    void            ClearDateList();

    inline Date     GetStartTimeMap(int iRow, int iCol)             { return m_pStartTimeMap[iRow * m_iMCCols + iCol ]; }
    inline void     SetStartTimeMap(int iRow, int iCol, Date dVal)  { m_pStartTimeMap[iRow * m_iMCCols + iCol] = dVal; }
    void            GetValidPos(int iRow, int iCol, int* piPosStart, int* piPosStop);
    void            SetValidPos(int iRow, int iCol, int iPosStart, int iPosStop);
    inline void     SetValidPos(int iRow, int iCol, int iVal) {m_pValidPosMap[iRow * m_iMCCols + iCol] = iVal;}
    
    inline void     SetTitleAlignment(EALIGNMENT e)   { m_eTitleAlign = e; }
    inline          EALIGNMENT GetTitleAlignment()    { return m_eTitleAlign; }
    void            SetDOWNameArray(PRBool bLocaleChange);
    void            SetMonthsArray();

    void            PaintMiniCalElement(void* p, int iRow,int iCol, int iTotRows, int iTotCols, int iXLeft, int iYTop, DateTime& baseDate, PRBool bLeading );
    void            HighlightCell(DateTime d, nscolor fg, nscolor bg, int iX, int iY);
    void            HighlightCell(DateTime d, nscolor fg, nscolor bg);


protected:
    nsString        m_sFontName;        // the name of the font face

public:
    NS_IMETHOD_(nsEventStatus) OnPaint(nsIRenderingContext& aRenderingContext,
                                     const nsRect& aDirtyRect);
    NS_IMETHOD_(nsEventStatus) OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);
    NS_IMETHOD_(nsEventStatus) OnLeftButtonDown(nsGUIEvent *aEvent);
    NS_IMETHOD_(nsEventStatus) OnLeftButtonUp(nsGUIEvent *aEvent);
    NS_IMETHOD_(nsEventStatus) OnMouseMove(nsGUIEvent *aEvent);


// Operations
public:

// Implementation
public:
    void InitRedraw(void* p);
    void PostRedraw(void* p);

    void GetTextExtent(char* p, int iChars, int& iWidth,int& iHeight);
    void GetFontInfo(nsString s, int iFontSize, int iStyle,int& riDigitWidth, int& riDigitOutput, int& iMaxWidth );
    void GetWindowSize(int& riWidth, int& riHeight);


    // nsIXPFCObserver methods
    NS_IMETHOD_(nsEventStatus) Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand);

    // nsIXPFCCommandReceiver methods
    NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand);

    void SetDefaults();

  
protected:
    nsresult    GetWeekDateList(int iWeekOffset);
    nsresult    HighlightActionList();
    void        ClearActionDateList();
    nsresult    AddToActionList(DateTime d);
    void        CommitArrow(int iDayVal, int iMonthVal);
    void        CommitDateList();
    nsIArray * m_ActionDateList;  // Interactive DateList as we move the mouse

};

#endif /* nsCalMonthContextController_h___ */

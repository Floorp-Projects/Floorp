/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/////////////////////////////////////////////////////////////////////////////
// printpag.cpp : implementation file
// contains the printer page setup code.  This files contains
// two objects: the page setup dialog box and the page
// setup default setting object which also implements the
// IPrinter interface. - jre
// 

#include "stdafx.h"
#include <ctype.h>
#include <string.h>
#include "printpag.h"		// print page setup
#include "apipage.h"		// page setup api
#include "prefapi.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#define	MEASURE_US			1
#define MEASURE_METRIC		2

// metric or US measurement flag
static int measureUnit = -1;

// conversions
#define	TWIPSPERINCH		1440
#define TWIPSPERCENTIMETER	567

/////////////////////////////////////////////////////////////////////////////
// CPrintPageSetup dialog


CPrintPageSetup::CPrintPageSetup(CWnd* pParent /*=NULL*/)
	: CDialog(CPrintPageSetup::IDD, pParent)
{
	//{{AFX_DATA_INIT(CPrintPageSetup)
	m_bLinesBlack = FALSE;
	m_bPrintBkImage = FALSE;
	m_bTextBlack = FALSE;
	m_bDate = FALSE;
	m_bPageNo = FALSE;
	m_bSolidLines = FALSE;
	m_bTitle = FALSE;
	m_bTotal = FALSE;
	m_bURL = FALSE;
	//}}AFX_DATA_INIT

	// Initializes the measurement setting based on the locale
	SetLocaleUnit ( );

    ApiPageSetup(api,0);
    api->GetPageSize ( &lWidth, &lHeight );
	api->GetMargins (&m_InitialLeft, &m_InitialRight, &m_InitialTop, &m_InitialBottom );
}


void CPrintPageSetup::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrintPageSetup)
    DDX_Control(pDX, IDC_PAGESETUP_SAMPLE, m_PagePreview);
	DDX_Control(pDX, IDC_PAGESETUP_MARGINBOX, m_MarginBox);
	DDX_Control(pDX, IDC_PAGESETUP_MBOTTOM, m_BottomMargin);
	DDX_Control(pDX, IDC_PAGESETUP_MLEFT, m_LeftMargin);
	DDX_Control(pDX, IDC_PAGESETUP_MRIGHT, m_RightMargin);
	DDX_Control(pDX, IDC_PAGESETUP_MTOP, m_TopMargin);
	DDX_Check(pDX, IDC_PAGESETUP_ALLLINESBLACK, m_bLinesBlack);
	DDX_Check(pDX, IDC_PAGESETUP_ALLTEXTBLACK, m_bTextBlack);
	DDX_Check(pDX, IDC_PAGESETUP_DATE, m_bDate);
	DDX_Check(pDX, IDC_PAGESETUP_PAGENUMBERS, m_bPageNo);
	DDX_Check(pDX, IDC_PAGESETUP_SOLIDLINES, m_bSolidLines);
	DDX_Check(pDX, IDC_PAGESETUP_TITLE, m_bTitle);
	DDX_Check(pDX, IDC_PAGESETUP_TOTAL, m_bTotal);
	DDX_Check(pDX, IDC_PAGESETUP_URL, m_bURL);
    DDX_Check(pDX, IDC_PAGESETUP_REVERSEORDER, m_bReverseOrder);
#ifdef XP_WIN32
	DDX_Check(pDX, IDC_PRINTBKIMAGE, m_bPrintBkImage);
#endif
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CMarginEdit, CGenericEdit)
    ON_WM_CHAR()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// Filter out any unwanted characters.  Only allow valid margin characters.

void CMarginEdit::OnChar( UINT nChar, UINT nRepCnt, UINT nFlags )
{
    if ( ( nChar == VK_BACK ) ||
        strchr ( " 0123456789.,cm\"", tolower(nChar) ) 
        )
        CWnd::OnChar ( nChar, nRepCnt, nFlags );
}

BEGIN_MESSAGE_MAP(CPrintPageSetup, CDialog)
	//{{AFX_MSG_MAP(CPrintPageSetup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
          
BEGIN_MESSAGE_MAP(CPagePreview,CStatic)
	ON_WM_PAINT()
END_MESSAGE_MAP()

void CPagePreview::OnPaint() 
{
    CStatic::OnPaint();
	CClientDC dc(this); // device context for painting
    CPrintPageSetup * pDialog = (CPrintPageSetup *)GetOwner ( );
    if ( pDialog != NULL )
	    pDialog->ShowPagePreview ( dc );
}

/////////////////////////////////////////////////////////////////////////////
// CPrintPageSetup message handlers

// ShowPagePreview displays the small page layout in the the page setup
// control.  The printer coordinates are adjusted to screen coordinates for
// proper proportion.

void CPrintPageSetup::ShowPagePreview ( CClientDC & pdc )
{
	RECT rect, shade, margins;				// working rectangles
	long mtop, mbottom, mleft, mright;		// margins in twips
	ApiPageSetup(api,0);					// Get the page setup API
	int iX, 								// width of page in pixels
		iY, 								// height of page in pixels
		AspectXY;							// aspect ratio mul 1000

	// get the bounding rectangle of the sample area
    m_PagePreview.GetClientRect ( &rect );

	// compute the width and height
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	// get the margin to display the sample text
	api->GetMargins ( &mleft, &mright, &mtop, &mbottom );

	// check page orientation and base on height if normal orientation
	// or on width for landscape... make best use of screen space

	if ( lHeight > lWidth )
	{
		iY = height - ( height / 4 );	// use .75 of the height

		// compute the aspect ration ( percentage width to height )
		AspectXY = CASTINT(( lWidth * 1000 ) / lHeight);
		
		// get the preview item height in inches
		long lPreviewHeight = 
			( long(iY) * 1000L ) / pdc.GetDeviceCaps ( LOGPIXELSY );

		// compute the inches the width should be from the aspect
		long lX = lPreviewHeight * AspectXY;
		lX /= 1000;

		// how many pixels does that translate to?
		lX *= pdc.GetDeviceCaps ( LOGPIXELSX );
		iX = LOWORD(lX/1000);
	}
	else
	{
		iX = width - ( width / 4 );		// use .75 of the width

		// compute the aspect ration ( percentage width to height )
		AspectXY = CASTINT(( lHeight * 1000 ) / lWidth);

		// get the preview item width in inches
		long lPreviewWidth = 
			( long(iX) * 1000L ) / pdc.GetDeviceCaps ( LOGPIXELSX );

		// compute the inches the height should be from the aspect
		long lY = lPreviewWidth * AspectXY;
		lY /= 1000;

		// how many pixels does that translate to?
		lY *= pdc.GetDeviceCaps ( LOGPIXELSY );
		iY = LOWORD(lY/1000);
	}

	// compute the number of twips per pixel for margin computation
	long iXTwipsPerPixel = lWidth / iX;
	long iYTwipsPerPixel = lHeight / iY;

	HDC hdc = pdc.GetSafeHdc();
	// create a black pen for borders
//    CPen blackPen (PS_SOLID,1,RGB(0,0,0));
//    pdc.SelectObject(&blackPen);
	::SelectObject(hdc, (HPEN)::GetStockObject(BLACK_PEN));
	// center the page in the sample area
	rect.left += ( width  - iX ) / 2;
	rect.top  += ( height - iY ) / 2;
	rect.right = rect.left + iX;
	rect.bottom = rect.top + iY;

//    CBrush brWindow ( GetSysColor(COLOR_WINDOW) );
	HBRUSH brWindow = (HBRUSH)::CreateSolidBrush(GetSysColor(COLOR_WINDOW) );
//    pdc.SelectObject ( &brWindow );
	::SelectObject(hdc, (HBRUSH)brWindow);

    // draw the small folded over flap on the top left handl corner
	int iFoldOver = height / 15;
	::MoveToEx( hdc, rect.left, rect.top + iFoldOver, NULL);
	::LineTo (hdc, rect.left, rect.bottom );
	::LineTo (hdc, rect.right, rect.bottom );
	::LineTo (hdc, rect.right, rect.top );
	::LineTo (hdc, rect.left + iFoldOver, rect.top );
	::LineTo (hdc, rect.left, rect.top + iFoldOver );

	::FloodFill (hdc, rect.right / 2, rect.bottom / 2, RGB(0,0,0) );
	::LineTo (hdc, rect.left + iFoldOver, rect.top + iFoldOver );
	::LineTo (hdc, rect.left + iFoldOver, rect.top );
#if 0
	pdc.MoveTo ( rect.left, rect.top + iFoldOver );
	pdc.LineTo ( rect.left, rect.bottom );
	pdc.LineTo ( rect.right, rect.bottom );
	pdc.LineTo ( rect.right, rect.top );
	pdc.LineTo ( rect.left + iFoldOver, rect.top );
	pdc.LineTo ( rect.left, rect.top + iFoldOver );

	pdc.FloodFill ( rect.right / 2, rect.bottom / 2, RGB(0,0,0) );

	pdc.LineTo ( rect.left + iFoldOver, rect.top + iFoldOver );
	pdc.LineTo ( rect.left + iFoldOver, rect.top );
#endif
	// compute the text area from the margin layout
	memcpy ( &margins, &rect, sizeof(rect) );
	margins.left   += CASTINT( mleft   / iXTwipsPerPixel );
	margins.right  -= CASTINT( mright  / iXTwipsPerPixel );
	margins.top    += CASTINT( mtop    / iYTwipsPerPixel );
	margins.bottom -= CASTINT( mbottom / iYTwipsPerPixel );

    if ( ( margins.left < margins.right ) && ( margins.top < margins.bottom ) )
    {

    	// create a gray pen to display the sample text
//    	CPen greyPen ( PS_SOLID, 1, GetSysColor(COLOR_ACTIVECAPTION) );
//    	pdc.SelectObject ( &greyPen );

		HPEN greyPen = ::CreatePen(PS_SOLID, 1,GetSysColor(COLOR_ACTIVECAPTION)); 
		HPEN hOldPen = (HPEN)::SelectObject(hdc, 	(HPEN)greyPen);
    	int iy, iline;
    	BOOL bIndent = TRUE;

    	// draw lines for text layout the entire print area minus margins
    	for ( iy = 0, iline = 0; iy <= ( margins.bottom - margins.top ); iy++ )
    	{
		    if ( !( iy % 4 ) )		// every 4 pixel rows draw a line
		    {
    			if ( bIndent )		// is this the beginning of a paragraph?
			    {

				    // don't draw the line in the little fold over flap

				    if ( ( ( iy + margins.top ) < ( rect.top + iFoldOver ) ) &&
				    ( ( margins.left + ( iX / 10 ) ) < ( rect.left + iFoldOver ) ) )
//    					pdc.MoveTo ( rect.left + iFoldOver, iy + margins.top );
						::MoveToEx (hdc, rect.left + iFoldOver, iy + margins.top, NULL );
	    			else					
//		    			pdc.MoveTo ( margins.left + ( iX / 10 ), iy + margins.top );
		    			::MoveToEx ( hdc, margins.left + ( iX / 10 ), iy + margins.top, NULL );

//			    	pdc.LineTo ( margins.right, iy + margins.top ); 
					::LineTo ( hdc, margins.right, iy + margins.top ); 
				    bIndent = FALSE;
    			} 
			    else if ( !( iline % 5 ) )	// is this the end of a paragraph?
			    {
//    				pdc.MoveTo ( margins.left, iy + margins.top );
//				    pdc.LineTo ( margins.left + ( iX / 3 ), iy + margins.top ); 
    				::MoveToEx ( hdc, margins.left, iy + margins.top, NULL );
				    ::LineTo  ( hdc, margins.left + ( iX / 3 ), iy + margins.top ); 
				    bIndent = TRUE;
			    }
			    else 		// just draw a normal straight across line
			    {
    				// don't draw the line in the little fold over flap
    
				    if ( ( ( iy + margins.top ) < ( rect.top + iFoldOver ) ) &&
				    ( margins.left < ( rect.left + iFoldOver ) ) )
    					::MoveToEx  ( hdc, rect.left + iFoldOver, iy + margins.top, NULL);
				    else
    					::MoveToEx  ( hdc, margins.left, iy + margins.top, NULL);
					::LineTo (hdc, margins.right, iy + margins.top );
			    }

    			iline++;
	    	}
	    }
		::SelectObject(hdc, 	(HPEN)hOldPen);
		::DeleteObject(greyPen);

    }

	// draw the shading around the sample
//	shade.left   = rect.right;
//	shade.right  = rect.right + 3;
//	shade.top    = rect.top + 6;
//	shade.bottom = rect.bottom + 3;

	::SelectObject(hdc, (HPEN)::GetStockObject(BLACK_PEN));
	::SelectObject(hdc, (HBRUSH)::GetStockObject(BLACK_BRUSH));
//	pdc.Rectangle ( &shade );
	::Rectangle(hdc,rect.right, rect.top + 6, rect.right + 3, rect.bottom + 3);

//	shade.left   = rect.left + 6;
//	shade.top    = rect.bottom;

//	pdc.Rectangle ( &shade );
	::Rectangle(hdc,rect.left + 6, rect.bottom, rect.right + 3, rect.bottom + 3);
	::SelectObject(hdc, (HPEN)::GetStockObject(NULL_PEN));
	::SelectObject(hdc, (HBRUSH)::GetStockObject(NULL_BRUSH ));
	::DeleteObject(brWindow);
}

void CPrintPageSetup::MarginError ( char * szTitle )
{
    MessageBox ( 
        "The margin value entered was not valid.  Try a smaller number.",
        szTitle, MB_OK | MB_ICONEXCLAMATION );

}

// function reads all the margin text from the margin controls, converts
// it from the locale measurement units and stores the values back in the
// page setup object.

int CPrintPageSetup::UpdateMargins ( BOOL check )
{
	char szBuffer[ 10 ];					// buffer holds edit control data
	long mtop, mbottom, mleft, mright;		// margins in twips
	long mtopold, mbottomold, mleftold, mrightold;
	ApiPageSetup(api,0); 					// Get the page setup API

	api->GetMargins ( &mleftold, &mrightold, &mtopold, &mbottomold );

	// get the margin text from the edit controls and convert it 
	// to twips

	memset ( szBuffer, '\0', sizeof(szBuffer) );
	m_TopMargin.GetLine ( 0, szBuffer, sizeof(szBuffer) );
	strlwr ( szBuffer );
	mtop = LocaleUnitToTwips ( szBuffer );

    // if we are range checking, check to see if this is a valid margin

    if ( check && ( mtop < 0 ) )
    {
        MarginError ( szLoadString(IDS_TOPMARGINERROR) );
        m_TopMargin.SetFocus ( );
        m_TopMargin.SetSel ( 0, -1 );
        return 1;
    }
   
	memset ( szBuffer, '\0', sizeof(szBuffer) );
	m_BottomMargin.GetLine ( 0, szBuffer, sizeof(szBuffer) );
	strlwr ( szBuffer );
	mbottom = LocaleUnitToTwips ( szBuffer );

    if ( check && ( mbottom < 0 ) )
    {
        MarginError ( szLoadString(IDS_BOTTOMMARGINERROR) );
        m_BottomMargin.SetFocus ( );
        m_BottomMargin.SetSel ( 0, -1 );
        return 1;
    }

	memset ( szBuffer, '\0', sizeof(szBuffer) );
	m_LeftMargin.GetLine ( 0, szBuffer, sizeof(szBuffer) );
	strlwr ( szBuffer );
	mleft = LocaleUnitToTwips ( szBuffer );

    if ( check && ( mleft < 0 ) )
    {
        MarginError ( szLoadString(IDS_LEFTMARGINERROR) );
        m_LeftMargin.SetFocus ( );
        m_LeftMargin.SetSel ( 0, -1 );
        return 1;
    }

	memset ( szBuffer, '\0', sizeof(szBuffer) );
	m_RightMargin.GetLine ( 0, szBuffer, sizeof(szBuffer) );
	strlwr ( szBuffer );
	mright = LocaleUnitToTwips ( szBuffer );

    if ( check && ( mright < 0 ) )
    {
        MarginError ( szLoadString(IDS_RIGHTMARGINERROR) );
        m_RightMargin.SetFocus ( );
        m_RightMargin.SetSel ( 0, -1 );
        return 1;
    }

    //  1 inch x 1 inch minimum space.
    if(check) {
        if(mtop + mbottom >= lHeight - TWIPSPERINCH) {
            //  Send them to the top/bottom margin for editing the problem.
            if(mtop >= mbottom) {
                MarginError ( szLoadString(IDS_TOPMARGINERROR) );
                m_TopMargin.SetFocus ( );
                m_TopMargin.SetSel ( 0, -1 );
            }
            else {
                MarginError ( szLoadString(IDS_BOTTOMMARGINERROR) );
                m_BottomMargin.SetFocus ( );
                m_BottomMargin.SetSel ( 0, -1 );
            }
            return 1;
        }
        else if(mleft + mright >= lWidth - TWIPSPERINCH) {
            //  Send them to left/right margin for editing the problem.
            if(mleft >= mright)  {
                MarginError ( szLoadString(IDS_LEFTMARGINERROR) );
                m_LeftMargin.SetFocus ( );
                m_LeftMargin.SetSel ( 0, -1 );
            }
            else {
                MarginError ( szLoadString(IDS_RIGHTMARGINERROR) );
                m_RightMargin.SetFocus ( );
                m_RightMargin.SetSel ( 0, -1 );
            }
            return 1;
        }
    }

	// update the preview area only if margins have changed

	if ( ( mright != mrightold ) || 
		( mleft != mleftold ) ||
		( mbottom != mbottomold ) || 
		( mtop != mtopold ) )
	{
        m_PagePreview.Invalidate ( );
		// set the margins in the page setup object
		api->SetMargins ( mleft, mright, mtop, mbottom );
	}

    return 0;
}

void CPrintPageSetup::OnCancel()
{
	ApiPageSetup(api,0); 						// Get the page setup API
	api->SetMargins (m_InitialLeft, m_InitialRight, m_InitialTop, m_InitialBottom );
    CDialog::OnCancel();
}

// processed when the user presses OK button.  Does all margin validations
// etc.  Sets up the printer page setup object with the selected options.

void CPrintPageSetup::OnOK() 
{
	ApiPageSetup(api,0); 						// Get the page setup API

	UpdateData ( );	

	if ( UpdateMargins ( TRUE ) )
        return;

	// set up the header information
	api->Header ( 				
		( m_bTitle ? PRINT_TITLE : 0 ) | 
		( m_bURL ? PRINT_URL : 0 ) );

	// set the footer information
	api->Footer (
		( m_bPageNo ? PRINT_PAGENO : 0 ) |
		( m_bDate   ? PRINT_DATE   : 0 ) |
		( m_bTotal 	? PRINT_PAGECOUNT : 0 ) );

	// set up the miscellaneous page options
	api->BlackLines ( m_bLinesBlack );
	api->BlackText( m_bTextBlack );
	api->SolidLines ( !m_bSolidLines );
    api->ReverseOrder ( m_bReverseOrder );
    api->SetPrintingBkImage ( m_bPrintBkImage );
	
	CDialog::OnOK();
}

// OnCommand handler looks at the KILLFOCUS messages from the edit controls.
// The new margin values are retrieved and the layout area is updated. Also,
// the values are normalized and restored back in the edit control

BOOL CPrintPageSetup::OnCommand(WPARAM wParam, LPARAM lParam) 
{
#ifdef WIN32
	if ( HIWORD(wParam) == EN_KILLFOCUS )
#else
    if ( HIWORD(lParam) == EN_KILLFOCUS )
#endif
	{
		if ( ( LOWORD(wParam) == IDC_PAGESETUP_MTOP ) ||
			( LOWORD(wParam) == IDC_PAGESETUP_MBOTTOM ) ||
			( LOWORD(wParam) == IDC_PAGESETUP_MLEFT ) ||
			( LOWORD(wParam) == IDC_PAGESETUP_MRIGHT ) )
		{
			UpdateMargins ( );		// update the margin values - relayout

			// get the edit control and normalize the value
			CMarginEdit * pEdit = (CMarginEdit *) GetDlgItem ( LOWORD(wParam) );
			if ( pEdit )
			{
				char szBuffer[10];
				long lTwips;
				memset ( szBuffer, '\0', sizeof(szBuffer) );
				pEdit->GetLine ( 0, szBuffer, sizeof(szBuffer) );
				strlwr ( szBuffer );
				lTwips = LocaleUnitToTwips ( szBuffer );
				pEdit->SetWindowText( TwipsToLocaleUnit ( lTwips ) );
			}
		}
	}
	return CDialog::OnCommand(wParam, lParam);
}

// function converts twips to centimeters or inches based on the
// conversion and the locale.

char * CPrintPageSetup::TwipsToString ( long twips, long conversion )
{
	static char szBuffer[ 10 ];

	// load the centimeter abbreviation
	CString cs;
	cs.LoadString( IDS_PAGESETUP_CM );

	// Save to static buffer rounding to 2 dec places
	sprintf( szBuffer, "%.2f", (float)twips/(float)conversion );

#ifdef XP_WIN32
	char buf[10];
	GetNumberFormat(LOCALE_SYSTEM_DEFAULT, NULL, szBuffer, NULL, buf, 10);
	strcpy(szBuffer, buf);
#endif
	strcat(szBuffer, ( measureUnit == MEASURE_US ) ? "\"" : (const char *)cs);

	return szBuffer;	// return conversion
}

// function takes a text buffer and converts it to twips from
// inches or centimeters.  If the text contains a ", it assumed
// inches.  If the text is preceded by 'cm' it is assumed 
// metric.  Locale can be overriden by specifying one of the formentioned
// identifiers.
// 1440 twips/inch
//  567 twips/cm

long CPrintPageSetup::StringToTwips ( char * szBuffer, long conversion )
{
	int len = strlen ( szBuffer );	// string length to parse
	long lDecPart = 0;				// decimal part
	long lFracPart = 0;				// fractional part
    int precision = 0;              // make sure at least 2 decimal points
	int i;	
	BOOL decimal = TRUE;			// what part are we currently parsing

	// process the entire string
	for ( i = 0; i < len; i++ )
	{
		// ignore anything but digits for numeric processing
		if ( isdigit ( szBuffer[ i ] ) )	
		{
			// are we processing the decimal or fractional part
			if ( decimal )
				lDecPart = ( lDecPart * 10 ) + ( szBuffer[ i ] - '0' );
			else
            {
				lFracPart = ( lFracPart * 10 ) + ( szBuffer[ i ] - '0' );
                precision++;
            }
		}
		// toggle the dec/frac flag when we hit a decimal point
		else if ( szBuffer[ i ] == '.' || szBuffer[ i ] == ',' )
			decimal = FALSE;
	}
    
    lFracPart *= conversion;
    while ( precision-- )
        lFracPart /= 10;

	// final conversion for total twips	
	return (( lDecPart * conversion ) + lFracPart);
}

// function checks the locale and sets the units of measurement to either
// US or metric. 

void CPrintPageSetup::SetLocaleUnit ( void )
{
	// check only if the flag hasn't been initialized
	if ( measureUnit == -1 )
	{
#ifdef WIN32
		char szLCStr[2];
		GetLocaleInfo (
			GetSystemDefaultLCID(),	// current locale
			LOCALE_IMEASURE,		// measurement system
			szLCStr,				// will return '0'-metric,'1'-US
			sizeof(szLCStr) );

		if ( szLCStr[ 0 ] == '0' )
			measureUnit = MEASURE_METRIC;
		else
			measureUnit = MEASURE_US;
#else
        if (GetProfileInt("intl", "iMeasure", 0) == 0)
            measureUnit = MEASURE_METRIC;
        else
            measureUnit = MEASURE_US;
#endif
	}
}

// function converts twip count to a text string representing the
// value in centimeters or inches based on the locale

char * CPrintPageSetup::TwipsToLocaleUnit ( long lTwips )
{
	switch ( measureUnit )
	{
		case MEASURE_US:
			return TwipsToString ( lTwips, TWIPSPERINCH );
			break;

		case MEASURE_METRIC:
			return TwipsToString ( lTwips, TWIPSPERCENTIMETER );
			break;
	}
	
	return 0;

}

// function accepts a text string representing either inches or
// centimeters and converts it to twips.  The locale can be overriden
// by adding either a '"' for inches or 'cm' for centimeters.

long CPrintPageSetup::LocaleUnitToTwips ( char * szBuffer )
{
	CString cs;
	cs.LoadString ( IDS_PAGESETUP_CM );	// get the centimeter abrv.

	// check for locale override
	if ( strstr ( szBuffer, cs ) )
		return StringToTwips ( szBuffer, TWIPSPERCENTIMETER );
	else if ( strchr ( szBuffer, '\"' ) )
		return StringToTwips ( szBuffer, TWIPSPERINCH );

	// no locale overrides, convert based on system locale
	if ( measureUnit == MEASURE_US )
		return StringToTwips ( szBuffer, TWIPSPERINCH );
	else 
		return StringToTwips ( szBuffer, TWIPSPERCENTIMETER );
	
	return 0;
}

// called to preinitialize the page setup dialog.  This function takes
// the values from the page setup object and initializes the page setup
// dialog with them.

BOOL CPrintPageSetup::OnInitDialog() 
{
	CString cs, SampleText;	
 	long mright, mleft, mtop, mbottom;
	ApiPageSetup(api,0);		// page setup api
	// get the margins from the object
	api->GetMargins ( &mleft, &mright, &mtop, &mbottom );

	CDialog::OnInitDialog();

    // print the paer size on the sample window
    char szSize[50], szWidth[10], szHeight[10], szSample[50];
    strcpy ( szWidth, TwipsToLocaleUnit ( lWidth ) );
    if ( strchr ( szWidth, '\"' ) ) * strchr ( szWidth, '\"' ) = '\0';
    strcpy ( szHeight, TwipsToLocaleUnit ( lHeight ) );
    if ( strchr ( szHeight, '\"' ) ) * strchr ( szHeight, '\"' ) = '\0';
    wsprintf ( szSize, " (%s x %s)", szWidth, szHeight );
    CStatic * pBox = (CStatic *)GetDlgItem(IDC_PAGESETUP_SAMPLE);
	GetDlgItemText(IDC_PAGESETUP_SAMPLE, szSample, 50);
	SampleText = szSample;
	SampleText+=szSize;
	pBox->SetWindowText ( SampleText );

	
	// load the inches or centimeters message based on the locale
	if ( measureUnit == MEASURE_METRIC )
		cs.LoadString ( IDS_PAGESETUP_CENTIMETERS );
	else
		cs.LoadString ( IDS_PAGESETUP_INCHES );

	// set the 'Margins (Inches)' or 'Margins (Centimeters)' heading
	m_MarginBox.SetWindowText ( cs );

	// convert from twips and initialize the dialog with the default
	// types
	m_RightMargin.SetWindowText( TwipsToLocaleUnit ( mright ) );
	m_TopMargin.SetWindowText( TwipsToLocaleUnit ( mtop ) );
	m_BottomMargin.SetWindowText( TwipsToLocaleUnit ( mbottom ) );
	m_LeftMargin.SetWindowText( TwipsToLocaleUnit ( mleft ) );

	// initialize the focus to the first margin setting selected
	m_TopMargin.SetFocus ( );
	m_TopMargin.SetSel ( 0, -1 );

	// initialize the check boxes
	m_bLinesBlack = api->BlackLines();
	m_bTextBlack  = api->BlackText();
	m_bSolidLines = api->SolidLines();
	m_bTitle  = ( api->Header() & PRINT_TITLE ) ? TRUE : FALSE;
	m_bURL    = ( api->Header() & PRINT_URL ) ? TRUE : FALSE;
	m_bPageNo = ( api->Footer() & PRINT_PAGENO ) ? TRUE : FALSE;
	m_bTotal  = ( api->Footer() & PRINT_PAGECOUNT ) ? TRUE : FALSE;
	m_bDate   = ( api->Footer() & PRINT_DATE ) ? TRUE : FALSE;    
	m_bPrintBkImage = api->IsPrintingBkImage();
    m_bReverseOrder = api->ReverseOrder();

	( (CButton *) GetDlgItem(IDC_PAGESETUP_SOLIDLINES) )->SetCheck ( !m_bSolidLines );
	( (CButton *) GetDlgItem(IDC_PAGESETUP_ALLTEXTBLACK) )->SetCheck ( m_bTextBlack );
	( (CButton *) GetDlgItem(IDC_PAGESETUP_ALLLINESBLACK) )->SetCheck ( m_bLinesBlack );
	( (CButton *) GetDlgItem(IDC_PAGESETUP_TITLE) )->SetCheck ( m_bTitle );
	( (CButton *) GetDlgItem(IDC_PAGESETUP_PAGENUMBERS) )->SetCheck ( m_bPageNo);
	( (CButton *) GetDlgItem(IDC_PAGESETUP_URL) )->SetCheck ( m_bURL );
	( (CButton *) GetDlgItem(IDC_PAGESETUP_DATE) )->SetCheck( m_bDate );
	( (CButton *) GetDlgItem(IDC_PAGESETUP_TOTAL) )->SetCheck ( m_bTotal );
    ( (CButton *) GetDlgItem(IDC_PAGESETUP_REVERSEORDER) )->SetCheck ( m_bReverseOrder );
#ifdef XP_WIN32
	( (CButton *) GetDlgItem(IDC_PRINTBKIMAGE) )->SetCheck ( m_bPrintBkImage );
#endif
	// return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
	return FALSE;
}

// Default page setup options.  This is a class that is statically instantiated
// which implements the IPageSetup API.

class CPageSetupInfo : 	public IUnknown,
						public IPageSetup
{
protected:

	ULONG m_ulRefCount;

	// page setup values
	long m_lLeftMargin;
	long m_lRightMargin;
	long m_lTopMargin;
	long m_lBottomMargin;
	int  m_iHeader;
	int  m_iFooter;
	int  m_iSolidLines;
	int  m_iBlackText;
	int  m_iBlackLines;
    int  m_iReverseOrder;
	BOOL  m_bPrintBkImage;

    long lWidth, lHeight;
	
public:

	CPageSetupInfo ( );
	~CPageSetupInfo ( );

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);
   
	// IClassFactory Interface
	STDMETHODIMP			CreateInstance(LPUNKNOWN,REFIID,LPVOID*);
	STDMETHODIMP			LockServer(BOOL);

	// IPageSetup Interface
	void GetMargins ( long *, long *, long *, long * );
	void SetMargins ( long, long, long, long );
    void GetPageSize ( long *, long * );
    void SetPageSize ( long, long );
	int Header ( int flag );
	int Footer ( int flag );
	int SolidLines ( int flag );
	int BlackText ( int flag );
	int BlackLines ( int flag );
    int ReverseOrder ( int flag );
    void SetPrintingBkImage (BOOL flag);
    BOOL IsPrintingBkImage (void);
};

class CPageSetupInfoFactory :	public IClassFactory
{
protected:
	ULONG m_ulRefCount;

public:
	CPageSetupInfoFactory();

	// IUnknown Interface
	STDMETHODIMP			QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG)	AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);
   
	// IClassFactory Interface
	STDMETHODIMP			CreateInstance(LPUNKNOWN,REFIID,LPVOID*);
	STDMETHODIMP			LockServer(BOOL);
};

CPageSetupInfoFactory::CPageSetupInfoFactory()
{
	ApiApiPtr(api);		
	api->RegisterClassFactory(APICLASS_PAGESETUP,(LPCLASSFACTORY)this);
	api->CreateClassInstance(APICLASS_PAGESETUP);
}

STDMETHODIMP CPageSetupInfoFactory::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IClassFactory))
		*ppv = (LPCLASSFACTORY) this;

	if (*ppv != NULL) {
		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CPageSetupInfoFactory::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CPageSetupInfoFactory::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

STDMETHODIMP CPageSetupInfoFactory::CreateInstance(
	LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj)
{
	*ppvObj = NULL;
	CPageSetupInfo * pRepository = new CPageSetupInfo;
	return pRepository->QueryInterface(refiid,ppvObj);   
}

STDMETHODIMP CPageSetupInfoFactory::LockServer(BOOL fLock)
{
	return NOERROR;
}

// Constructor initializes the default values and registers the object
// as the implementor of the IPrinter interface.

CPageSetupInfo::CPageSetupInfo ( ) 
{
	m_ulRefCount = 0;

	m_lLeftMargin = 720;		// 0.5 inches
	m_lRightMargin = 720;
	m_lTopMargin = 720;
	m_lBottomMargin = 720;
	m_iHeader = PRINT_TITLE | PRINT_URL;
	m_iFooter = PRINT_PAGENO | PRINT_PAGECOUNT | PRINT_DATE;
	m_iSolidLines = FALSE;
	m_iBlackText = FALSE;
	m_iBlackLines = FALSE;
    m_iReverseOrder = FALSE;

    lWidth = lHeight = 0;
}

// constructor releases the default API which causes it to automatically
// unregister based on a zero reference count.  The API will not dissapear
// until everyone is done with it.

CPageSetupInfo::~CPageSetupInfo ( )
{
}

STDMETHODIMP CPageSetupInfo::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IPageSetup))
		*ppv = (LPPAGESETUP) this;

	if (*ppv != NULL) {
		AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CPageSetupInfo::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CPageSetupInfo::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CPageSetupInfo::SetPrintingBkImage (BOOL flag)
{
	PREF_SetBoolPref("browser.print_background",flag);
	m_bPrintBkImage = flag;
}
BOOL CPageSetupInfo::IsPrintingBkImage (void) 
{
	if (PREF_GetBoolPref("browser.print_background",&m_bPrintBkImage) != PREF_OK)
		m_bPrintBkImage = FALSE;
	return m_bPrintBkImage;
}
// function sets the margins in twips

void CPageSetupInfo::SetMargins ( long lLeft, long lRight, long lTop, long lBottom )
{
	m_lLeftMargin = lLeft;
	m_lRightMargin = lRight;
	m_lTopMargin = lTop;
	m_lBottomMargin = lBottom;
}

// gets the margins

void CPageSetupInfo::GetMargins ( long * plLeft, long * plRight, long * plTop, long * plBottom )
{
	*plLeft = m_lLeftMargin;
	*plRight = m_lRightMargin;
	*plTop = m_lTopMargin;
	*plBottom = m_lBottomMargin;
}

// get-set the header flags, -1 gets the value (default)
// flags are: PRINT_TITLE | PRINT_URL

int CPageSetupInfo::Header ( int flag )
{
	if ( flag == -1 )
		return m_iHeader;
	int temp = m_iHeader;
	m_iHeader = flag;
	return temp;
}

// get-set the footer flag, -1 gets the value (default)
// flags are: PRINT_PAGENO | PRINT_PAGECOUNT | PRINT_DATE

int CPageSetupInfo::Footer ( int flag )
{
	if ( flag == -1 )
		return m_iFooter;
	int temp = m_iFooter;
	m_iFooter = flag;
	return temp;
}

// get-set solid line flag.  Control printing of textured bevels

int CPageSetupInfo::SolidLines ( int flag )
{
	if ( flag == -1 )
		return m_iSolidLines;
	int temp = m_iSolidLines;
	m_iSolidLines = flag;
	return temp;
	
}

// get-set all text black flag.  If set to TRUE, all text is printed
// in black

int CPageSetupInfo::BlackText ( int flag )
{
	if ( flag == -1 )
		return m_iBlackText;
	int temp = m_iBlackText;
	m_iBlackText = flag;
	return temp;
}

// get-set all line black flag

int CPageSetupInfo::BlackLines ( int flag )
{
	if ( flag == -1 )
		return m_iBlackLines;
	int temp = m_iBlackLines;
	m_iBlackLines = flag;
	return temp;
}

void CPageSetupInfo::SetPageSize ( long lw, long lh )
{
    lWidth = lw;
    lHeight = lh;
}

void CPageSetupInfo::GetPageSize ( long * plWidth, long * plHeight )
{
    CPrintDialog printDialog ( FALSE );
    if ( printDialog.GetDefaults ( ) )
    {
        HDC hPrintDC = printDialog.GetPrinterDC ( );
        if ( hPrintDC != NULL )
        {
            POINT point;
            ::Escape ( hPrintDC, GETPHYSPAGESIZE, NULL, NULL, &point );
            lWidth = point.x;
            lHeight = point.y;
            lWidth = ( lWidth * TWIPSPERINCH ) / ::GetDeviceCaps ( hPrintDC, LOGPIXELSX );
            lHeight = ( lHeight * TWIPSPERINCH ) / ::GetDeviceCaps ( hPrintDC, LOGPIXELSY );
        }
		::DeleteDC(hPrintDC);
		::GlobalFree(printDialog.m_pd.hDevNames);
		::GlobalFree(printDialog.m_pd.hDevMode);
    }
    if ( !lWidth || !lHeight )
    {
        lWidth = (TWIPSPERINCH * 85L ) / 10;
        lHeight = TWIPSPERINCH * 11;
    }

    *plWidth = lWidth;
    *plHeight = lHeight;

}

int CPageSetupInfo::ReverseOrder ( int flag )
{
    if ( flag == -1 )
        return m_iReverseOrder;
    int temp = m_iReverseOrder;
    m_iReverseOrder = flag;
    return temp;
}

static CPageSetupInfoFactory _defSettings;  // static instantiation of page setup object

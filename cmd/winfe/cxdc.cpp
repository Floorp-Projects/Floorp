/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "stdafx.h"
#include "cxdc.h"
#include "cntritem.h"
#include "intlwin.h"
#include "mainfrm.h"
#include "npapi.h"
#include "np.h"
#include "feembed.h"
#include "fmabstra.h"
#include "custom.h"
#include "prefapi.h"
#include "feimage.h"
#include "il_icons.h"
#include "intl_csi.h"
#include "prefinfo.h"
#include "cxprint.h"

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "java.h"
#endif

// These four are all specified in pixels
#define IMAGE_SMICON_MARGIN	4

#define IMAGE_SMICON_WIDTH	14
#define IMAGE_SMICON_HEIGHT	16

#define IMAGE_SMICON_TEXT_OFFSET	2


#define IS_SECOND_TO_LAST_LAYER(layer) (CL_GetLayerBelow(layer) &&           \
				CL_GetLayerName(CL_GetLayerBelow(layer)) &&  \
		 (strcmp(CL_GetLayerName(CL_GetLayerBelow(layer)),           \
				 LO_BACKGROUND_LAYER_NAME) == 0))

#define LOADBITMAP(id) \
	::LoadBitmap(AfxGetResourceHandle(), MAKEINTRESOURCE(id))                         

CDCCX::CDCCX()	{
//	Purpose:	Constructor for DC Context
//	Arguments:	void 
//	Returns:	none
//	Comments:	Sets the context type basically
//	Revision History:
//		05-27-95	created GAB
//
	m_cxType = DeviceContext;
	m_bOwnDC = FALSE;
	m_bClassDC = FALSE;

	//	Our mapping mode will be anisotropic unless those which derive
	//		set otherwise.
	m_MM = MM_ANISOTROPIC;

	m_pPal = NULL;
    m_lWidth = 0;
    m_lHeight = 0;
	m_iOffset = 0;
	CString csNo(SZ_NO);

	m_pImageDC = NULL;
	curColorSpace = 0;

	//	As a default starting background color, use that of the button face.
	m_rgbBackgroundColor = sysInfo.m_clrBtnFace;
	m_rgbDarkColor = sysInfo.m_clrBtnShadow;
	m_rgbLightColor = sysInfo.m_clrBtnHilite;
    Set3DColors(m_rgbBackgroundColor);

	//	Set that we have no document, and we currently
	//		won't autodelete what may get assigned in.
	m_pDocument = NULL;
	m_bAutoDeleteDocument = FALSE;

    //	No previously selected font.
    m_pSelectedCachedFont = NULL;
	m_lastDCWithCachedFont = NULL;
	
	//	Document has no substance.
	m_lDocWidth = 0;
	m_lDocHeight = 0;
	m_lOrgX = m_lOrgY = 0;


	//	Start out with some default values in the conversions from pix 2 twips.
	m_lConvertX = 1;
	m_lConvertY = 1;
	GetContext()->convertPixX = 1;
	GetContext()->convertPixY = 1;

	// Assume the driver doesn't support 565 mode for 16 bpp DIBs
	m_bRGB565 = FALSE;

	//	Allow resolving of layout elements.
	m_bResolveElements = TRUE;

	//	We're not in an OLE server.
	m_bOleServer = FALSE;

	// Form text entry box font
	m_pFormFixFont = NULL;
	m_pFormPropFont = NULL;
    m_iFormFixCSID = CS_LATIN1;
    m_iFormPropCSID = CS_LATIN1;

	m_bLoadImagesNow = FALSE;
	m_bNexttimeLoadImagesNow = FALSE;   
	
	GetContext()->XpixelsPerPoint = 0;        
	GetContext()->YpixelsPerPoint = 0;        
}

CDCCX::~CDCCX()	{
//	Purpose:	Destructor for DC Context
//	Arguments:	void
//	Returns:	none
//	Comments:
//	Revision History:
//		05-27-95	created GAB
//

	//	If we've got a document, let it know we're gone now.
	if(GetDocument() != NULL)	{
		TRACE("Clearing document context\n");
		GetDocument()->ClearContext();
	}

	if(m_pImageDC != NULL)	{
		if (!IsGridCell()) { // only delete the temporary DC if we are the parent.
			if (m_bUseDibPalColors)	
			::SelectPalette(m_pImageDC, (HPALETTE)::GetStockObject(DEFAULT_PALETTE), TRUE);
			VERIFY(::DeleteDC( m_pImageDC));
		}
	}

	// Do not delete the default palette.
	if(m_pPal && m_pPal != WFE_GetUIPalette(NULL))	{
		VERIFY(::DeleteObject(m_pPal));
	}
	if(m_pDocument != NULL && m_bAutoDeleteDocument == TRUE)	{
		//	We call OnCloseDocument here, to delete the document.
		//	If we do this here, it basically indicates that we created the
		//		document ourselves (probably a print context), and it
		//		wasn't made in the MFC framework.
		m_pDocument->OnCloseDocument();
	}

	if (curColorSpace)
		IL_ReleaseColorSpace(curColorSpace);
}

void CDCCX::DestroyContext()	{
    if(IsDestroyed() == FALSE)  {
	    //	Give the document a chance to copy any needed information before
	    //		we go down.  This helps the OLE server stuff survive while
	    //		the context of the view has gone to the twilight zone and back.
	    if(GetDocument())	{
		    GetDocument()->CacheEphemeralData();
	    }

	    //	Get rid of all our fonts.
	    ClearFontCache();
    }


	//	Call the base.
	CStubsCX::DestroyContext();
}

BOOL CDCCX::ResolveTextExtent(HDC pDC, LPCTSTR pString, int iLength, LPSIZE pSize, CyaFont *pFont)	
{
	return ResolveTextExtent(
		INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(GetContext()))
		, pDC, pString, iLength, pSize, pFont);
}

#ifdef XP_WIN32
static BOOL
SupportsRGB565(HDC pDC)
{
    BITMAPINFO *lpBMInfo = NULL;
	int			iFlags;
	int			nSize = sizeof(BITMAPINFOHEADER) + 3 * sizeof(RGBQUAD);
    LPDWORD 	lpMasks;

	// Use the QUERYDIBSUPPORT escape to determine if the driver
	// supports RGB565 format DIBs
	lpBMInfo = (BITMAPINFO *)XP_ALLOC(nSize);
	if (!lpBMInfo)
		return FALSE;
    
    memset(lpBMInfo, 0, nSize);    
    lpBMInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    lpBMInfo->bmiHeader.biWidth = 10;
    lpBMInfo->bmiHeader.biHeight = 10;
    lpBMInfo->bmiHeader.biPlanes = 1;
    lpBMInfo->bmiHeader.biBitCount = 16;
    lpBMInfo->bmiHeader.biCompression = BI_BITFIELDS;

	// Define the color masks for a RGB565 DIB
    lpMasks = (LPDWORD)lpBMInfo->bmiColors;
	lpMasks[0] = 0xF800;  // red color mask
	lpMasks[1] = 0x07E0;  // green color mask
	lpMasks[2] = 0x001F;  // blue color mask

	// Ask the driver
	::Escape(pDC, QUERYDIBSUPPORT, nSize, (LPCSTR)lpBMInfo, &iFlags);
	XP_FREE(lpBMInfo);

	return (iFlags & (QDI_SETDIBITS | QDI_DIBTOSCREEN | QDI_STRETCHDIB)) != 0;
}
#endif

void CDCCX::PrepareDraw()
{
	if (m_pImageDC) {
		if (m_bUseDibPalColors && m_pPal) 
			::SelectPalette(m_pImageDC, (HPALETTE)GetStockObject(DEFAULT_PALETTE), FALSE);
		::DeleteDC(m_pImageDC);
	}
	m_pImageDC = ::CreateCompatibleDC(GetContextDC());
	if (m_bUseDibPalColors && m_pPal)	{
		::SelectPalette(m_pImageDC, m_pPal, FALSE);
	}

}

void CDCCX::EndDraw()
{
	if (m_pImageDC) {
		if (m_bUseDibPalColors && m_pPal) 
			::SelectPalette(m_pImageDC, (HPALETTE)GetStockObject(DEFAULT_PALETTE), FALSE);
		::DeleteDC(m_pImageDC);
	}
	m_pImageDC = 0;
}


void CDCCX::Initialize(BOOL bOwnDC, RECT *pRect, BOOL bInitialPalette, BOOL bNewMemDC)	{
	//	First thing, we need to create a document
	//		if no one has bothered assigning us one.
	if(m_pDocument == NULL)	{
		m_pDocument = new CGenericDoc();
		m_bAutoDeleteDocument = TRUE;	//	We clean up.
	}

	//	There exists a document.
	//	Let it know which context it can manipulate.
	GetDocument()->SetContext(this);

	HDC hdc = GetContextDC();

	//	Initialize some values which are used in CDC contexts
	//		for drawing purposes.
	SetMappingMode(hdc);
    ::SetBkMode(hdc, TRANSPARENT);

	//	Figure out the depth of the CDC.
	m_iBitsPerPixel = sysInfo.m_iBitsPerPixel;

    //	Now, if the bits per pixel are less than 16, and this device doesn't support
	//		a palette, then we need to set the bits per pixel to a very high value
	//		to emulate true color (so that we don't attempt to use the palette
	//		function later on).
	//	This is the Epson Stylus Color Printer bug fix on Win16.
	if(m_iBitsPerPixel < 16 && (::GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) == 0)	{
        //  24 is the largest common denominator which we should be able to set
        //      in order to avoid palette operations.
        //  Uped to 24 from 16 as the Hewlett Packard Laserjet 5L bug fix on Win32.
			m_iBitsPerPixel = 24;
	}

	//	Detect if we are going to be using DibPalColors. We only do this if the device
	// is a palette device and we never do this when printing
	m_bUseDibPalColors = !IsPrintContext() && (::GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE);


	//	Do some stretch blit setup.
#ifndef WIN32
	::SetStretchBltMode(hdc, STRETCH_DELETESCANS);
#else
    ::SetStretchBltMode(hdc, COLORONCOLOR);
#endif

	JMCException* exc = NULL;
	IMGCB* imageLibObj = IMGCBFactory_Create(&exc);

	m_pImageGroupContext = IL_NewGroupContext(GetContext(), (IMGCBIF *)imageLibObj);
    GetContext()->img_cx = m_pImageGroupContext; // Need this to be cross platform.
	IL_AddGroupObserver(m_pImageGroupContext, ImageGroupObserver, (void*)GetContext());

    COLORREF rgbColor = prefInfo.m_rgbBackgroundColor;
	SetTransparentColor(GetRValue(rgbColor), GetGValue(rgbColor), GetBValue(rgbColor));
	if (!IsGridCell()) {
		if(m_iBitsPerPixel < 16) {

			//	Set the transparency to a default color.
			//	This may change below.
			SetTransparentColor(192, 192, 192);

			//	Set up the per-context palette.
			//	This is per-window since not everything goes to the screen, and therefore
			//		won't always have the screen's attributes. (printing, metafiles DCs, etc).
			IL_ColorMap * defaultColorMap =  IL_NewCubeColorMap(NULL, 0,
						   MAX_IMAGE_PALETTE_ENTRIES+1);
			if (bInitialPalette)
				m_pPal = CDCCX::InitPalette(hdc);
			else // If we don't want to initialize the Palette, use the default palette.
				m_pPal = WFE_GetUIPalette(NULL);
			CDCCX::SetColormap(hdc, defaultColorMap, rgbTransparentColor, m_pPal);
			curColorSpace = IL_CreatePseudoColorSpace(defaultColorMap, MAX_IMAGE_PALETTE_ENTRIES+1,
							  m_iBitsPerPixel);
			HPALETTE hOldPal = NULL;
			if(m_pPal)    {
				if ( bInitialPalette) 
					hOldPal = (HPALETTE) ::SelectPalette(hdc, m_pPal, FALSE);
				else
					hOldPal = (HPALETTE) ::SelectPalette(hdc, m_pPal, TRUE);
				int error = RealizePalette(hdc);      // In with the new
			}
#ifdef DEBUG_mhwang
			HWND hwnd = ::GetFocus();
#endif

		}
		else { // use RGB value
			IL_RGBBits colorRGBBits;
		//	Enable true color.
			if(m_iBitsPerPixel >= 16)	{
				if(m_iBitsPerPixel == 16)	{
		#ifdef XP_WIN32
					// If this is Win95 we need to do rounding during quantization of
					// 24 bpp RGB values to 16 bpp RGB values
					if (sysInfo.IsWin4_32() == TRUE &&
						SupportsRGB565(hdc) != FALSE &&
						IsPrintContext() == FALSE) {

						//	Metafiles can't handle this hack.
						//	Also, the image lib will force metafiles to share the
						//		same image even though the settings would be slightly different.
						//	Pump us up to 24 bit images in this case.
						if(GetContextType() == MetaFile)	{
							colorRGBBits.red_shift = 16;  
							colorRGBBits.red_bits = 8;            /* Offset for red channel bits. */
							colorRGBBits.green_shift = 8;           /* Number of bits assigned to green channel. */
							colorRGBBits.green_bits = 8;          /* Offset for green channel bits. */
							colorRGBBits.blue_shift = 0;            /* Number of bits assigned to blue channel. */
							colorRGBBits.blue_bits = 8;           /* Offset for blue channel bits. */
							curColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);

						}
						else	{
							m_bRGB565 = TRUE;  // remember this when building BITMAPINFO struct
							colorRGBBits.red_shift = 11;  
							colorRGBBits.red_bits = 5;            /* Offset for red channel bits. */
							colorRGBBits.green_shift = 5;           /* Number of bits assigned to green channel. */
							colorRGBBits.green_bits = 6;          /* Offset for green channel bits. */
							colorRGBBits.blue_shift = 0;            /* Number of bits assigned to blue channel. */
							colorRGBBits.blue_bits = 5;           /* Offset for blue channel bits. */
							curColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 16);
						}

					} else {
						colorRGBBits.red_shift = 10;  
						colorRGBBits.red_bits = 5;            /* Offset for red channel bits. */
						colorRGBBits.green_shift = 5;           /* Number of bits assigned to green channel. */
						colorRGBBits.green_bits = 5;          /* Offset for green channel bits. */
						colorRGBBits.blue_shift = 0;            /* Number of bits assigned to blue channel. */
						colorRGBBits.blue_bits = 5;           /* Offset for blue channel bits. */
						curColorSpace =IL_CreateTrueColorSpace(&colorRGBBits, 16);
					}
		#else
					// 16 bpp DIB format isn't supported by all drivers in Win 3.1
					colorRGBBits.red_shift = 16;  
					colorRGBBits.red_bits = 8;            /* Offset for red channel bits. */
					colorRGBBits.green_shift = 8;           /* Number of bits assigned to green channel. */
					colorRGBBits.green_bits = 8;          /* Offset for green channel bits. */
					colorRGBBits.blue_shift = 0;            /* Number of bits assigned to blue channel. */
					colorRGBBits.blue_bits = 8;           /* Offset for blue channel bits. */
					curColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);
		#endif
				}
				else  {
					colorRGBBits.red_shift = 16;  
					colorRGBBits.red_bits = 8;            /* Offset for red channel bits. */
					colorRGBBits.green_shift = 8;           /* Number of bits assigned to green channel. */
					colorRGBBits.green_bits = 8;          /* Offset for green channel bits. */
					colorRGBBits.blue_shift = 0;            /* Number of bits assigned to blue channel. */
					colorRGBBits.blue_bits = 8;           /* Offset for blue channel bits. */
					curColorSpace = IL_CreateTrueColorSpace(&colorRGBBits, 24);
				}
			}
		}
		GetContext()->color_space = curColorSpace; // Need this to be cross platform.
	}
	else {
		MWContext* cxtx = GetParentContext();
		GetContext()->color_space = cxtx->color_space;
		curColorSpace =cxtx->color_space; 
        IL_AddRefToColorSpace(curColorSpace);
        m_bRGB565 = CXDC(cxtx)->m_bRGB565;
	}
    
	IL_AddRefToColorSpace(GetContext()->color_space);
	//	Get image prefs on a per context basis.
    IL_DitherMode ditherMode = IL_Auto; 

	char * prefStr=NULL;
	PREF_CopyCharPref("images.dither",&prefStr);
	// Images
	if (prefStr) {
		if(!strcmpi(prefStr,"yes")) { 
			ditherMode = IL_Dither;
		} else if(!strcmpi(prefStr,"no")) {   // closest
			ditherMode = IL_ClosestColor;
		} else {
			ditherMode = IL_Auto;
		}
		XP_FREE(prefStr);
	}

	PRBool bIncremental = ResolveIncrementalImages();
	IL_DisplayData displayData;
	displayData.dither_mode = IL_Auto;

	if (IsPrintContext())
		displayData.display_type = IL_Printer;
	else
		displayData.display_type = IL_Console;
	displayData.color_space = curColorSpace;
	displayData.progressive_display= bIncremental;
	IL_SetDisplayMode(m_pImageGroupContext, IL_PROGRESSIVE_DISPLAY | IL_DITHER_MODE | IL_COLOR_SPACE | IL_DISPLAY_TYPE,
                  &displayData);

	if (bNewMemDC) {
		if (!IsGridCell()) {  // only create the tempary CD only for gird parent.
		//	Ensure our memory DC exists for speedy image draws.
			m_pImageDC = ::CreateCompatibleDC(hdc);
			if (m_bUseDibPalColors && m_pPal)	{
				::SelectPalette(m_pImageDC, m_pPal, FALSE);
			}
		}
		else {
			MWContext *parentContext = GetParentContext();
			m_pImageDC = CXDC(parentContext)->m_pImageDC; 
		}
	}
	GetContext()->XpixelsPerPoint = ((double)GetDeviceCaps (GetAttribDC(), LOGPIXELSX)) / 72.0 ;
	GetContext()->YpixelsPerPoint = ((double)GetDeviceCaps (GetAttribDC(), LOGPIXELSY)) / 72.0 ;

#ifdef DEBUG_aliu
	double dd = ((double)GetDeviceCaps (hdc, LOGPIXELSX)) / 72.0 ;
	dd = ((double)GetDeviceCaps (hdc, LOGPIXELSY)) / 72.0 ;
#endif

	ReleaseContextDC(hdc);

	//	We optimize those DCs which we know are persistant (don't change or reset
	//		their values) for font caching and other stuff.
	//	As a default, consider all such DC classes as persistent, if not, then
	//		set this value otherwise.
	//	This needs to go at the end of this function, since the mapping mode should
	//		now be correctly set, et al....
	m_bOwnDC = bOwnDC;

#ifdef LAYERS
	GetContext()->compositor = NULL;
#endif // LAYERS

}

//	Default implementation of incremental image display switch.
PRBool CDCCX::ResolveIncrementalImages()
{
    XP_Bool bRetval = prefInfo.m_bAutoLoadImages;
	
	return (bRetval) ? PR_TRUE : PR_FALSE;
}

HPALETTE CDCCX::GetPalette() const	{
	//	Return our palette.
	return(m_pPal);
}


void CDCCX::ClearFontCache()	{
	//	If we're a persistant DC, we need to select a stock object before we release
	//		all the fonts.
	if(IsClassDC() || IsOwnDC())	{
		HDC hdc = GetContextDC();
		::SelectObject(hdc, ::GetStockObject(ANSI_FIXED_FONT));
		ReleaseContextDC(hdc);

		//	We have no previously selected font.
        m_pSelectedCachedFont = NULL;
		m_lastDCWithCachedFont = NULL;

	}

#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    //  Tell layout to get rid of all of it's cached FE font data
    //      in the LO_TextAttr.
    LO_InvalidateFontData(GetDocumentContext());
#endif


    //  Go through our context list of cached fonts and get rid of them all.
    POSITION rIndexnew = m_cplCachedFontList.GetHeadPosition();
    CyaFont *pCachedFont = NULL;
    while(rIndexnew)   {
        pCachedFont = (CyaFont *)m_cplCachedFontList.GetNext(rIndexnew);
        if(pCachedFont) {
            delete pCachedFont;
        }
    }
    //  Remove them all from the list.
    if(!m_cplCachedFontList.IsEmpty())   {
        m_cplCachedFontList.RemoveAll();
    }

    //  Get rid of widget fonts.
	if(m_pFormFixFont) {
		VERIFY(::DeleteObject(m_pFormFixFont));
        m_pFormFixFont = NULL;
    }
	if(m_pFormPropFont)    {
		VERIFY(::DeleteObject(m_pFormPropFont));
        m_pFormPropFont = NULL;
    }
}

void CDCCX::ClearAllFontCaches()	{
	MWContext *pTraverseContext = NULL;
	CAbstractCX *pTraverseCX = NULL;
	XP_List *pTraverse = XP_GetGlobalContextList();
	while (pTraverseContext = (MWContext *)XP_ListNextObject(pTraverse)) {
		if(pTraverseContext != NULL && ABSTRACTCX(pTraverseContext) != NULL)	{
			pTraverseCX = ABSTRACTCX(pTraverseContext);

			if(pTraverseCX->IsDCContext() == TRUE)	{
				CDCCX *pDCCX = VOID2CX(pTraverseCX, CDCCX);
				pDCCX->ClearFontCache();
			}
		}
	}
}


// size is the size of the font from layout
// iBaseSize is the users font size from the encoding
// iOffset is the offset based on increment and decrement font
double CDCCX::CalcFontPointSize(int size, int iBaseSize, int iOffset)
{
	double dFontSize;

	if( iBaseSize <=0 )
           iBaseSize = 10;

/*	size += iOffset;*/
	if(size < 0)
		size = 0;

	if(size  > 8)
		size = 8;

	switch(size)	{
	case 0:
		dFontSize = iBaseSize / 2;
		break;
	case 1:
		dFontSize = 7 * iBaseSize / 10;
		break;
	case 2:
		dFontSize = 85 * iBaseSize / 100;
		break;
	case 4:
		dFontSize = 12 * iBaseSize / 10;
		break;
	case 5:
		dFontSize = 3 * iBaseSize / 2;
		break;
	case 6:
		dFontSize = 2 * iBaseSize;
		break;
	case 7:
		dFontSize = 3 * iBaseSize;
		break;
	case 8:
		dFontSize = 4 * iBaseSize;
		break;
	case 3:
	default:
		dFontSize = iBaseSize;
	}

	//if(MMIsText() != TRUE)	  // for printer scaling to pointsize interface.
//		dFontSize *= 2.8;			//todo  this is a hack!!

	if(GetContext())
		dFontSize *= GetContext()->fontScalingPercentage;

    return dFontSize;

}	// CalcFontPointSize


// Manage DC level font cache: If font is selected into dc, don't do anything.
// Otherwise, call SelectNetscapeFont() and cache it into dc.
// return 1 if we miss any font in pAttr->font_face list.
int CDCCX::SelectNetscapeFontWithCache( HDC hdc, LO_TextAttr *pAttr, CyaFont *& pMyFont )	
{
//	Purpose:	Font selector
//	Arguments:	pDC	The DC in which to select the font
//				pAttr	Layout attributes by which we select the font.
//				pSelected	This is more of a return value.  It sets the reference of
//					the pointer value ot the font actually selected.
//	Returns:	no meaning, old font is saved in CyaFont.
//	Comments:

	//	Determine the size of the font and other stuff.
	//	Get the font out of the cache.
	int		returnCode = 0;		// assuming we don't miss any font.
// #ifdef nofontcache
    CyaFont *pCachedFont = (CyaFont *)pAttr->FE_Data;
	pMyFont = pCachedFont;
	//	See if we've already got this font selected if we're a persistant DC.
	//	We'll assume it's in the font cache if so.
	if(IsOwnDC() == TRUE && m_lastDCWithCachedFont == hdc && m_pSelectedCachedFont) {
		if(  m_pSelectedCachedFont == pCachedFont)   {
			//	Simply return, already selected.
			return( 0 );
		} else { 
			// relase the selected font.
			ReleaseNetscapeFont( hdc, m_pSelectedCachedFont );
			m_pSelectedCachedFont = NULL;
			m_lastDCWithCachedFont = NULL;
			// fall through to select a font.
		}
	} 

	// create font (or use FE_Data cached font) for the text 
	returnCode = SelectNetscapeFont( hdc, pAttr, pMyFont );

	if( ! pMyFont->IsEmptyFont() ) {

		//	If this is a persistant DC, then we mark the font currently selected so that
		//		we don't select it again if the same request comes in.
		//	We don't want to do this if this was the memory DC we're dealing with.
		if(IsOwnDC() == TRUE)	{
			m_lastDCWithCachedFont = hdc;
			m_pSelectedCachedFont = pMyFont;		// pSelectThis;
		}
	}
//#else 
//	returnCode = SelectNetscapeFont( hdc, pAttr, pMyFont );
//#endif // nofontcache

	return( returnCode );

}

// SelectNetscapeFont() does:
// always select the font into dc.
// manage FE_Data level cache.
// Create the font if not cached, 
// follow font list and use default font at last.
// return 1 if the any font in list not found.
// After creation, 
//    cache it into FE_Data, and calculate meanwidth.
//    link-list the font, so it can be deleted when destroy document.
int CDCCX::SelectNetscapeFont( HDC hdc, LO_TextAttr *pAttr, CyaFont *& pMyFont )
{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return 0;
#else
	// create font for the text and cache it in pAttr->FE_Data.
	EncodingInfo *pEncoding = theApp.m_pIntlFont->GetEncodingInfo(GetContext());
	BOOL bItalic = FALSE;
	BOOL bFixed = FALSE;
	int iFontWeight = FW_NORMAL;
	double	dFontSize;
	int iBaseSize;
	int iCharset = DEFAULT_CHARSET;  // used for Win95 font selection
	char aFontName[MAXFONTFACENAME];
	char		* pCharsetStr;
	int			reCode;
	int		returnCode = 0;		// assuming we don't miss any font.

    CyaFont *pCachedFont = (CyaFont *)pAttr->FE_Data;
	if(pCachedFont != NULL && ! pCachedFont->IsEmptyFont() )	{
		//	It's cached.

		//	use the old font, and select the font.
		pMyFont = pCachedFont;
		pMyFont->PrepareDrawText( hdc );               // actually, select the font into hdc
		return( 0 );                                   // we don't miss this font.
	}

	// retrieve face-name-independent attributes first.

	if(pAttr->fontmask & LO_FONT_BOLD)	{ 
		iFontWeight = FW_BOLD;
	}

	// font_weight has the highest priority.
	if( pAttr->font_weight > 0 )
		iFontWeight = pAttr->font_weight;

	if(pAttr->fontmask & LO_FONT_ITALIC)	{
		bItalic = TRUE;
	}

	if(pAttr->fontmask & LO_FONT_FIXED)	{
		bFixed = TRUE;
	}

	if( bFixed)	{
		iBaseSize = pEncoding->iFixSize;
		iCharset = pEncoding->iFixCharset;
	} else {  // ! bFixed 
        iBaseSize = pEncoding->iPropSize;
		iCharset = pEncoding->iPropCharset;
    }

	//	Map the base size to the real font size.
	dFontSize = CalcFontPointSize(pAttr->size, iBaseSize, m_iOffset);

	// pAttr->point_size has the highest priority.
	if( pAttr->point_size > 0 ) {   
		dFontSize = pAttr->point_size;
 	}
	
	// save the encording flag in font, so it will use textOutW() for Unicode.
	// refer to BOOL CIntlWin::TextOut() in file intlwin.cpp
	// INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(pContext))
	char *pEncordingStr = NULL;
	int		isUnicode = 0;
#ifdef XP_WIN32
	if ( CIntlWin::UseUnicodeFontAPI( pEncoding->iCSID ) &&
		 ! ( pEncoding->iCSID == CS_UTF8  && CIntlWin::UseVirtualFont()) ) {
		// set the encording to use ::TextOutW()
		pEncordingStr	= "Unicode";
		isUnicode		= 1;
	}
#endif

	int nUnderline = nfUnderlineDontCare ;
	if(pAttr->attrmask & LO_ATTR_ANCHOR) { 
        XP_Bool prefBool = prefInfo.m_bUnderlineAnchors; //  ? prefInfo.m_bUnderlineAnchors->m_Value.xp_bool : TRUE;
		if(prefBool)
			nUnderline = nfUnderlineYes;
	}
	if(pAttr->attrmask & LO_ATTR_UNDERLINE)	{
		nUnderline = nfUnderlineYes;
	}
	
	pMyFont = new CyaFont();

    //  If a font name is listed, use that instead.
    BOOL bUseDefault = TRUE;

    if(pAttr->font_face)    {
        //  Go through the comma delimeted list, and find one that we have.
        char wantedFontName[MAXFONTFACENAME];
		char *pFontName	= NULL;
		int	 offSet = 0;
        do {
			offSet = FEU_ExtractCommaDilimetedFontName(pAttr->font_face, offSet, wantedFontName );

			// for(int iTraverse = 1; pExtracted = FEU_ExtractCommaDilimetedString(pAttr->font_face, iTraverse); iTraverse++)    
			if( offSet > 0 && *wantedFontName != 0)  {
				pFontName = theGlobalNSFont.converGenericFont(wantedFontName);

				strcpy(aFontName, pFontName);
				if(CIntlWin::UseUnicodeFontAPI(pEncoding->iCSID))    {
					iCharset = DEFAULT_CHARSET;
				}
				// On Win95, charset info is important for Wingdings font
				if (sysInfo.m_bWin4 && (strcmpi(aFontName, "wingdings") == 0))
					iCharset = DEFAULT_CHARSET;
				// web font use string for charSet
				pCharsetStr  = theGlobalNSFont.convertToFmiCharset( iCharset );

				if (isUnicode == 0 )
				{
					pEncordingStr = pCharsetStr;
				}

				//	Create the font.
				reCode = pMyFont->CreateNetscapeFont(
					GetContext(),
					hdc,									// HDC		hDC, 
					aFontName,								// char    *FmiName,
					pCharsetStr,							// char    *FmiCharset,
					pEncordingStr,							// char    *FmiEncoding,
					iFontWeight,							// int     FmiWeight,
					bFixed?nfSpacingMonospaced:nfSpacingProportional,			// int     FmiPitch,
					bItalic?nfStyleItalic:nfStyleNormal,	// int     FmiStyle,	
					nUnderline,								// int		FmiUnderline,
					(pAttr->attrmask & LO_ATTR_STRIKEOUT) ? nfStrikeOutYes : nfStrikeOutDontCare, // int     FmiStrikeOut,
					0,										// int     FmiResolutionX,
					::GetDeviceCaps(GetAttribDC(), LOGPIXELSY),		// int     FmiResolutionY,
					dFontSize								// int		fontHeight		// not a fmi field
				); 
				
				//if( theGlobalNSFont.HasFaceName( hdc, pExtracted)) 
					//  Use this.
				if( reCode == FONTERR_OK ) {
					bUseDefault = FALSE;		// we got the font!
				} else {
					// remenber there is font face name not found, 
					returnCode = 1;
				}
			}

            //  Our responsibility to clean this up.
            // XP_FREE(pExtracted);

            //  If we've set that we're using a font, get out.
            if(!bUseDefault) {
                break;
            }
		}	while ( offSet > 0 );  // do  offSet = FEU_ExtractCommaDilimetedFontName() ...
    }	// if(pAttr->font_face)

	if(bUseDefault)  {
		if( bFixed)	
			strcpy(aFontName, pEncoding->szFixName);
		else   // ! bFixed 
			strcpy(aFontName, pEncoding->szPropName);

		if(CIntlWin::UseUnicodeFontAPI(pEncoding->iCSID))    {
			iCharset = DEFAULT_CHARSET;
		}
		// On Win95, charset info is important for Wingdings font
		if (sysInfo.m_bWin4 && (strcmpi(aFontName, "wingdings") == 0))
			iCharset = DEFAULT_CHARSET;
		// web font use string for charSet
		pCharsetStr  = theGlobalNSFont.convertToFmiCharset( iCharset );
		if (isUnicode == 0 )
		{
			pEncordingStr = pCharsetStr;
		}

		//	Create the font.
		reCode = pMyFont->CreateNetscapeFont(
			GetContext(),
			hdc,									// HDC		hDC, 
			aFontName,								// char    *FmiName,
			pCharsetStr,							// char    *FmiCharset,
			pEncordingStr,							// char    *FmiEncoding,
			iFontWeight,							// int     FmiWeight,
			bFixed?nfSpacingMonospaced:nfSpacingProportional,			// int     FmiPitch,
			bItalic?nfStyleItalic:nfStyleNormal,	// int     FmiStyle,	
			nUnderline,								// int		FmiUnderline,
			(pAttr->attrmask & LO_ATTR_STRIKEOUT) ? nfStrikeOutYes : nfStrikeOutDontCare, // int     FmiStrikeOut,
			0,										// int     FmiResolutionX,
			::GetDeviceCaps(GetAttribDC(), LOGPIXELSY),		// int     FmiResolutionY,
			dFontSize								// int		fontHeight		// not a fmi field
		); 
	}

	if( FONTERR_OK != reCode ) {
		// user selected default font failed, use hardcoded default font
		iCharset = DEFAULT_CHARSET;
		
		// web font use string for charSet
		pCharsetStr  = theGlobalNSFont.convertToFmiCharset( iCharset );
		if (isUnicode == 0 )
		{
			pEncordingStr = pCharsetStr;
		}

		// get the face name
#ifdef _WIN32
		LOGFONT   theLogFont;
		CFont	*pCFont= CFont::FromHandle((HFONT)GetStockObject(bFixed?ANSI_FIXED_FONT:ANSI_VAR_FONT));
		pCFont->GetLogFont( &theLogFont );
		strncpy(aFontName, theLogFont.lfFaceName,LF_FACESIZE);
#else
		strncpy(aFontName, ( bFixed? "Courier New" : "Times New Roman" ) ,LF_FACESIZE);
#endif
		//	Create the font.
		reCode = pMyFont->CreateNetscapeFont(
			GetContext(),
			hdc,									// HDC		hDC, 
			aFontName,							// char    *FmiName,
			pCharsetStr,							// char    *FmiCharset,
			pEncordingStr,							// char    *FmiEncoding,
			iFontWeight,							// int     FmiWeight,
			bFixed?nfSpacingMonospaced:nfSpacingProportional,			// int     FmiPitch,
			bItalic?nfStyleItalic:nfStyleNormal,	// int     FmiStyle,	
			nUnderline,								// int		FmiUnderline,
			(pAttr->attrmask & LO_ATTR_STRIKEOUT) ? nfStrikeOutYes : nfStrikeOutDontCare, // int     FmiStrikeOut,
			0,										// int     FmiResolutionX,
			::GetDeviceCaps(GetAttribDC(), LOGPIXELSY),		// int     FmiResolutionY,
			dFontSize								// int		fontHeight		// not a fmi field
		); 
	}

	// 	if( FONTERR_OK != reCode )    real problem, todo show dialog to user!!!

	//	Select the font.
	pMyFont->PrepareDrawText( hdc );               // actually, select the font into hdc

	pMyFont->CalculateMeanWidth( hdc, isUnicode);

	// and cache it in pAttr->FE_Data.
	//	Cache this font for future usage.
	//  First, make sure that the text attribute points to it.
	pAttr->FE_Data = (void *)pMyFont;

	//  Second, add it to our context list of fonts (so we know to delete it later).
	m_cplCachedFontList.AddTail((void *)pMyFont);  // pSelectThis

	return( returnCode );
#endif /* MOZ_NGLAYOUT */
}	// HFONT CDCCX::SelectNetscapeFont()

void CDCCX::ReleaseNetscapeFontWithCache(HDC hdc, CyaFont * pNetscapeFont)	
{
//#ifdef nofontcache
	if( IsOwnDC() == TRUE )
		return;
	m_pSelectedCachedFont = NULL;
	m_lastDCWithCachedFont = NULL;

//#endif
	ReleaseNetscapeFont(hdc, pNetscapeFont);
}

void CDCCX::ReleaseNetscapeFont(HDC hdc, CyaFont * pNetscapeFont)
{	
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
	pNetscapeFont->EndDrawText( hdc );					// restore the old font.
#endif /* MOZ_NGLAYOUT */
}

// m_iOffset can be at most 8 or at least -8 based on the values in
// CalcFontPointSize;
void CDCCX::ChangeFontOffset(int iIncrementor)
{
/*	BOOL bDontReload = (m_iOffset == 8 && iIncrementor >= 0) || (m_iOffset == -8 && iIncrementor <= 0);

	m_iOffset += iIncrementor;
	if(m_iOffset < -8)
		m_iOffset = -8;
	else if(m_iOffset > 8)
		m_iOffset = 8;
	
	if(!bDontReload)*/

	m_iOffset += iIncrementor;
	if(GetContext()){
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
		GetContext()->fontScalingPercentage = LO_GetScalingFactor(m_iOffset);
#endif
	}

	NiceReload();
}



void CDCCX::ExtendCoord(LTRB& Rect)
{
	int32 lOrgX, lOrgY;

	GetDrawingOrigin(&lOrgX, &lOrgY);

	Rect.left += lOrgX;
	Rect.top += lOrgY;
	Rect.right += lOrgX;
	Rect.bottom += lOrgY;
}

BOOL CDCCX::ResolveElement(LTRB& Rect, int32 x, int32 y, int32 x_offset, int32 y_offset,
								int32 width, int32 height)
{
	BOOL bRetval = TRUE;
	//	First, figure the coordinates.
	Rect.left = x + x_offset - m_lOrgX;
	Rect.top = y + y_offset - m_lOrgY;
	Rect.right = Rect.left + width;
	Rect.bottom = Rect.top + height;

	ExtendCoord(Rect);

	//	Return FALSE if this doesn't fall into view of the context.
	if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
		if(CanBlockDisplay())	{
			bRetval = FALSE;
		}
	}
	return bRetval;
}
BOOL CDCCX::ResolveElement(LTRB& Rect, LO_TextStruct *pText, int iLocation, int32 lStartPos, int32 lEndPos, int iClear)	{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return FALSE;
#else
	BOOL bRetval = TRUE;

	//	Subtext, in order to be considered at all, must first pass the text resolution.
	if(ResolveElement(Rect, pText->x, pText->y, pText->x_offset, 
								pText->y_offset,pText->width, pText->height))	{
		//	Adjust the coordinates further for the specific subtext we're going to be
		//		drawing.
		//	This means we actually have to size the text....
		HDC hdc = GetContextDC();
		
		CyaFont	*pMyFont;
		SelectNetscapeFontWithCache( hdc, pText->text_attr, pMyFont );
		CSize sz;
		ResolveTextExtent(hdc, (const char *)pText->text, CASTINT(lStartPos), &sz, pMyFont);
		Rect.left += sz.cx;
		ResolveTextExtent(hdc, (const char *)pText->text + lStartPos, CASTINT(lEndPos - lStartPos + 1), &sz, pMyFont);
		Rect.right = Rect.left + sz.cx;	
		
        Rect.bottom = Rect.top + sz.cy;   

		ReleaseNetscapeFontWithCache( hdc, pMyFont );

		ReleaseContextDC(hdc);

		//	Return FALSE if this doesn't fall into view of the context.
		if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
			if(CanBlockDisplay())	{
				bRetval = FALSE;
			}
		}
	}
	else if(CanBlockDisplay())	{
		//	Didn't pass even the normal text filter.
		bRetval = FALSE;
	}

	return(bRetval);
#endif /* MOZ_NGLAYOUT */
}

/* 
 * Note that this was brought back, since form element positions are 
 * expressed in document coordinates, not layer coordinates, so the
 * standard ResolveElement techniques will not work.
 */
BOOL CDCCX::ResolveElement(LTRB& Rect, LO_FormElementStruct *pFormElement)	{
	BOOL bRetval = TRUE;

	//	First, figure the coordinates.
	Rect.left = pFormElement->x + pFormElement->x_offset - m_lOrgX;
	Rect.top = pFormElement->y + pFormElement->y_offset - m_lOrgY;
	Rect.right = Rect.left + pFormElement->width;
	Rect.bottom = Rect.top + pFormElement->height;

	/* Don't call ExtendCoord() for forms.  Form element positions are in
	   document coordinates, not layer coordinates. */

	//	Return FALSE if this doesn't fall into view of the context.
	if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
		if(CanBlockDisplay())	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

BOOL CDCCX::ResolveElement(LTRB& Rect, NI_Pixmap *pImage, int32 lX, int32 lY, 
						   int32 orgx, int32 orgy,
						   uint32 ulWidth, uint32 ulHeight,
                                                   int32 lScaleWidth, int32 lScaleHeight)
{
	BOOL bRetval = TRUE;

        /* Use the size of the image if size not provided. */
	if(ulWidth == 0)	{
		ulWidth = pImage->header.width;
	}

	if(ulHeight == 0)	{
		ulHeight = pImage->header.height;
	}

        /*  Scaled size overrides further if this is a print context.*/
	if(IsPrintContext()) {
		if(lScaleWidth) {
			ulWidth = (uint32)lScaleWidth;
		}
		if(lScaleHeight) {
			ulHeight = (uint32)lScaleHeight;
		}
	}

	Rect.left = orgx + lX - m_lOrgX;
	Rect.top = orgy + lY - m_lOrgY;
	
	ExtendCoord(Rect);

	if (Rect.top < 0 && lX) Rect.top = 0;
	if (Rect.left < 0 && lY) Rect.left = 0;

	Rect.right = Rect.left + ulWidth;
	Rect.bottom = Rect.top + ulHeight;

	return(bRetval);
}

BOOL CDCCX::ResolveElement(LTRB& Rect, LO_TableStruct *pTable, int iLocation)	{
	BOOL bRetval = TRUE;

	//	First, figure the coordinates.
	Rect.left = pTable->x + pTable->x_offset - m_lOrgX;
	Rect.top = pTable->y + pTable->y_offset - m_lOrgY;
	Rect.right = Rect.left + pTable->width;
	Rect.bottom = Rect.top + pTable->height;

	//	Return FALSE if this doesn't fall into view of the context.
	if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
		if(CanBlockDisplay())	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

BOOL CDCCX::ResolveElement(LTRB& Rect, LO_SubDocStruct *pSubDoc, int iLocation)	{
	BOOL bRetval = TRUE;

	//	First, figure the coordinates.
	Rect.left = pSubDoc->x + pSubDoc->x_offset - m_lOrgX;
	Rect.top = pSubDoc->y + pSubDoc->y_offset - m_lOrgY;
	Rect.right = Rect.left + pSubDoc->width;
	Rect.bottom = Rect.top + pSubDoc->height;

	//	Return FALSE if this doesn't fall into view of the context.
	if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
		if(CanBlockDisplay())	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

BOOL CDCCX::ResolveElement(LTRB& Rect, LO_CellStruct *pCell, int iLocation)	{
	BOOL bRetval = TRUE;

	//	First, figure the coordinates.
	Rect.left = pCell->x + pCell->x_offset - m_lOrgX;
	Rect.top = pCell->y + pCell->y_offset - m_lOrgY;
	Rect.right = Rect.left + pCell->width;
	Rect.bottom = Rect.top + pCell->height;

	//	Return FALSE if this doesn't fall into view of the context.
	if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
		if(CanBlockDisplay())	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

BOOL CDCCX::ResolveElement(LTRB& Rect, LO_EmbedStruct *pEmbed, int iLocation, Bool bWindowed)	{
	BOOL bRetval = TRUE;

	//	First, figure the coordinates.
    Rect.left = -m_lOrgX;
    Rect.top = -m_lOrgY;

    // For windowed embeds, we only need the position of the embed,
    // and we ignore the layer origin. For windowless embeds, we only
    // need the layer origin, and not the position of the embed.
    if (bWindowed) {
        Rect.left += pEmbed->objTag.x + pEmbed->objTag.x_offset + pEmbed->objTag.border_width;
        Rect.top += pEmbed->objTag.y + pEmbed->objTag.y_offset + pEmbed->objTag.border_width;
    }
    
	Rect.right = Rect.left + pEmbed->objTag.width;
	Rect.bottom = Rect.top + pEmbed->objTag.height;

    if (!bWindowed)
        ExtendCoord(Rect);

	//	Return FALSE if this doesn't fall into view of the context.
	if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
		if(CanBlockDisplay())	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

BOOL CDCCX::ResolveElement(LTRB& Rect, LO_LinefeedStruct *pLineFeed, int iLocation)	{
	BOOL bRetval = TRUE;

	//	First, figure the coordinates.
	Rect.left = pLineFeed->x + pLineFeed->x_offset - m_lOrgX;
	Rect.top = pLineFeed->y + pLineFeed->y_offset - m_lOrgY;
	Rect.right = Rect.left + pLineFeed->width;
	Rect.bottom = Rect.top + pLineFeed->height;

	//	Return FALSE if this doesn't fall into view of the context.
	if(Rect.top > m_lHeight || Rect.bottom < 0 || Rect.right < 0 || Rect.left > m_lWidth) {
		if(CanBlockDisplay())	{
			bRetval = FALSE;
		}
	}

	return(bRetval);
}

inline BOOL CDCCX::ResolveLineSolid()	{
	//	Don't default to all solid lines.
	return(FALSE);
}

COLORREF CDCCX::ResolveTextColor(LO_TextAttr *pAttr)	{
//	Purpose:	Determine the Forground color of the attribute.
//	Arguments:	pAttr	The attributes from which we will obtain the color.
//	Returns:	COLORREF	The color.
//	Comments:	Override this if you need finer control over the selections
//					of color such as printing.
//	Revision History:
//		06-09-95	created GAB

	//	Default implementation simply returns the color requested. Make sure that
	// we get the closest match in the palette and not one of the static system
	// colors. or'ing 0x02000000L in here is easier than finding every occurence
	// of SetTextColor downstream
	if(pAttr != NULL)	{
		return(0x02000000L | RGB(pAttr->fg.red, pAttr->fg.green, pAttr->fg.blue));
	}
	else	{
		return(RGB(0,0,0));
	}
}

//
// if we are not in true-color mode snap the color to the palette
//  otherwise they will get dithered to the windows 16-color palette
//  in the view's EraseBackground method
//
COLORREF CDCCX::ResolveBGColor(unsigned uRed, unsigned uGreen, unsigned uBlue)	
{

    int index = 0;
    if ((m_iBitsPerPixel <= 8) && m_pPal) {

        index = ::GetNearestPaletteIndex(m_pPal, RGB(uRed, uGreen, uBlue));

        PALETTEENTRY palEntry;
        ::GetPaletteEntries(m_pPal, index, 1, &palEntry);
        uRed   = palEntry.peRed;
        uGreen = palEntry.peGreen;
        uBlue  = palEntry.peBlue;

    }

	return(RGB(uRed, uGreen, uBlue));

}

BOOL CDCCX::ResolveHRSolid(LO_HorizRuleStruct *pHorizRule)	
{
	//	Determine if an HR is to be solid or 3D.
	//	Override this if you need to specifically control the display of
	//		HRs.
	if((pHorizRule->ele_attrmask & LO_ELE_SHADED) == LO_ELE_SHADED && pHorizRule->thickness > 1) {
		return(FALSE);
	}
	return(TRUE);
}

inline COLORREF CDCCX::ResolveDarkLineColor()	{
	//	By default, return the dark color.
	//	Override if you need to control HR colors.
	return(m_rgbDarkColor);
}

inline COLORREF CDCCX::ResolveLightLineColor()	{
	//	By default, return the light color.
	//	Override if you need to control HR colors.
	return(m_rgbLightColor);
}

COLORREF CDCCX::ResolveBorderColor(LO_TextAttr *pAttr)	{
	//	Default is to simply return the color requested.
	//	Override if you need to control border display.
	if(pAttr != NULL)	{
		return(RGB(pAttr->fg.red, pAttr->fg.green, pAttr->fg.blue));
	}
	else	{
		return(RGB(0,0,0));
	}
}

COLORREF CDCCX::ResolveLOColor(LO_Color *color)	{
	//	Default is to simply return the color requested.
	//	Override if you need to control border display.
	if(color != NULL)	{
		return(RGB(color->red, color->green, color->blue));
	}
	else	{
		return(RGB(0,0,0));
	}
}

void CDCCX::ResolveTransparentColor(unsigned uRed, unsigned uGreen, unsigned uBlue)	{
	//	Sets the transparency color of images.
	//	This in most cases is the same as the background color.
	//	Override this if you take special interest in controlling the background color.

	int iIndex = 0;
	IL_IRGB *rgbTransparent = GetContext()->transparent_pixel;

    if ((m_iBitsPerPixel <= 8) && m_pPal) {
		iIndex = ::GetNearestPaletteIndex(m_pPal, m_rgbBackgroundColor);
		curColorSpace->cmap.index[MAX_IMAGE_PALETTE_ENTRIES] = iIndex;
	}

	//	Simply let the image lib know what's up.
	if (!rgbTransparent) {
        rgbTransparent = XP_NEW_ZAP(IL_IRGB);
		GetContext()->transparent_pixel = rgbTransparent;
	}

    rgbTransparent->index = MAX_IMAGE_PALETTE_ENTRIES;
    rgbTransparent->red = uRed;
    rgbTransparent->green = uGreen;
    rgbTransparent->blue = uBlue;    
}

// rgbLight is used to render the top/left edges, and rgbDark is used to
// render the bottom/right edges
void CDCCX::Display3DBox(LTRB& Rect, COLORREF rgbLight, COLORREF rgbDark, int32 lWidth, BOOL bAdjust)	{
	HDC hDC = GetContextDC();

	//	Adjust the y value by half the height.  We'll need to move it back
	//		to the original position, since we have a reference to the rect.
	int32 lAdjust = 0;
	if(bAdjust == TRUE)	{
		//	This should only happen with HRs!
		lAdjust = Rect.Height() / 2;
		if(MMIsText() == FALSE)	{
			Rect.top -= lAdjust;
			Rect.bottom -= lAdjust;
		}
	}
	else	{
		//	When bAdjust is false, this means we are dealing with a table/cell/subdoc.
		//	To properly draw these, we need to shrink the rect on the right and bottom
		//		by the width.
		lAdjust = lWidth;
		Rect.right -= lAdjust;
		Rect.bottom -= lAdjust;
	}

    // If there's a thick border then miter the join where the different
    // colors meet
	if(lWidth > Pix2TwipsX(1) && ResolveLineSolid() == FALSE)	{
		HPEN    hOldPen = (HPEN)::SelectObject(hDC, GetStockObject(NULL_PEN));
		POINT   aPoints[6];
        HBRUSH  hbDark, hbLight, hOldBrush;
        
        // Make sure that in 256 color mode we get a PALETTE RGB color
        hbDark = ::CreateSolidBrush(0x02000000L | rgbDark);
        hbLight = ::CreateSolidBrush(0x02000000L | rgbLight);

		//	Map all the points.
		//	Left and top.
		aPoints[0].x = CASTINT(Rect.left + lWidth);
		aPoints[0].y = CASTINT(Rect.top + lWidth);
		aPoints[1].x = CASTINT(Rect.left + lWidth);
		aPoints[1].y = CASTINT(Rect.bottom);
		aPoints[2].x = CASTINT(Rect.left);
		aPoints[2].y = CASTINT(Rect.bottom + lWidth);
		aPoints[3].x = CASTINT(Rect.left);
		aPoints[3].y = CASTINT(Rect.top);
		aPoints[4].x = CASTINT(Rect.right + lWidth);
		aPoints[4].y = CASTINT(Rect.top);
		aPoints[5].x = CASTINT(Rect.right);
		aPoints[5].y = CASTINT(Rect.top + lWidth);
		
		hOldBrush = (HBRUSH)::SelectObject(hDC, hbLight);
		::Polygon(hDC, aPoints, 6);

		//	Right and bottom.
		aPoints[0].x = CASTINT(Rect.right);
		aPoints[0].y = CASTINT(Rect.bottom);
		aPoints[3].x = CASTINT(Rect.right + lWidth);
		aPoints[3].y = CASTINT(Rect.bottom + lWidth);
		
		::SelectObject(hDC, hbDark);
		::Polygon(hDC, aPoints, 6);

		::SelectObject(hDC, hOldPen);
		::SelectObject(hDC, hOldBrush);
        VERIFY(::DeleteObject(hbDark));
        VERIFY(::DeleteObject(hbLight));
	}
	else	{
        HBRUSH  hbLight, hbDark, hOldBrush;

        // Left/top edges
        hbLight = ::CreateSolidBrush(0x02000000L | rgbLight);
		hOldBrush = (HBRUSH)::SelectObject(hDC, hbLight);
        
        ::PatBlt(hDC, CASTINT(Rect.left), CASTINT(Rect.top),
            CASTINT(Rect.right - Rect.left), lWidth, PATCOPY);
        ::PatBlt(hDC, CASTINT(Rect.left), CASTINT(Rect.top),
            lWidth, CASTINT(Rect.bottom - Rect.top), PATCOPY);
        
        ::SelectObject(hDC, hOldBrush);
        VERIFY(::DeleteObject(hbLight));

        // Bottom/right edges
        hbDark = ::CreateSolidBrush(0x02000000L | rgbDark);
		hOldBrush = (HBRUSH)::SelectObject(hDC, hbDark);

        ::PatBlt(hDC, CASTINT(Rect.left), CASTINT(Rect.bottom),
            CASTINT(Rect.right - Rect.left), lWidth, PATCOPY);
        ::PatBlt(hDC, CASTINT(Rect.right), CASTINT(Rect.top),
            lWidth, CASTINT(Rect.bottom - Rect.top) + 1, PATCOPY);
		
        ::SelectObject(hDC, hOldBrush);
        VERIFY(::DeleteObject(hbDark));
	}
    //	Reposition.
	if(bAdjust == TRUE)	{
		//	HRs
		if(MMIsText() == FALSE)	{
			Rect.top += lAdjust;
			Rect.bottom += lAdjust;
		}
	}
	else	{
		//	Table/cell/subdoc
		Rect.right += lAdjust;
		Rect.bottom += lAdjust;
	}

	ReleaseContextDC(hDC);
}

static HPEN
CreateStyledPen(COLORREF color, int nWidth, int nPenStyle)
{
#ifdef _WIN32
    LOGBRUSH    lb;
    DWORD       dwPenStyle;
#endif

#ifdef _WIN32
    lb.lbStyle = 0;  // ignored
    lb.lbColor = 0x02000000L | color;
    lb.lbStyle = 0;  // ignored

    dwPenStyle = PS_GEOMETRIC | PS_ENDCAP_SQUARE | PS_JOIN_MITER;

    return ::ExtCreatePen(dwPenStyle | nPenStyle, nWidth, &lb, 0, NULL);
#else
    return ::CreatePen(nPenStyle, nWidth, 0x02000000L | color);
#endif
}

static void
DisplaySpecialBorder(HDC hDC, RECT &r, COLORREF color, int iWidth, BOOL bSolid)
{
	HPEN	hPen = ::CreatePen(PS_DOT, 1, 0x02000000L | color);
	HPEN	hOldPen = (HPEN)::SelectObject(hDC, hPen);
	HBRUSH	hOldBrush = (HBRUSH) ::SelectObject(hDC, ::GetStockObject(NULL_BRUSH));

    ::SetBkMode(hDC, TRANSPARENT);
    for( int i = 0; i < iWidth; i++ )
    {
    	::Rectangle(hDC, r.left, r.top, r.right, r.bottom);
        ::InflateRect(&r, -1, -1);
    }
	
    ::SelectObject(hDC, hOldPen);
	::SelectObject(hDC, hOldBrush);
	VERIFY(::DeleteObject(hPen));
}

static void
DisplaySolidBorder(HDC hDC, RECT &r, COLORREF color, LTRB &widths)
{
	// See if all border edges are the same width
	if (widths.left == widths.top && widths.left == widths.right && widths.left == widths.bottom) {
		HPEN	hPen = ::CreatePen(PS_INSIDEFRAME, widths.left, 0x02000000L | color);
		HPEN	hOldPen = (HPEN)::SelectObject(hDC, hPen);
		HBRUSH	hOldBrush = (HBRUSH) ::SelectObject(hDC, ::GetStockObject(NULL_BRUSH));

		// Draw it using a single rectangle call
		::Rectangle(hDC, r.left, r.top, r.right, r.bottom);
		::SelectObject(hDC, hOldPen);
		::SelectObject(hDC, hOldBrush);
		VERIFY(::DeleteObject(hPen));

	} else {
        HBRUSH  hBrush = ::CreateSolidBrush(0x02000000L | color);
		HBRUSH  hOldBrush = (HBRUSH)::SelectObject(hDC, hBrush);

		// Draw each of the individual edges using PatBlt
		if (widths.left > 0)
			::PatBlt(hDC, r.left, r.top, widths.left, r.bottom - r.top, PATCOPY);

		if (widths.top > 0)
			::PatBlt(hDC, r.left, r.top, r.right - r.left, widths.top, PATCOPY);

		if (widths.right > 0)
			::PatBlt(hDC, r.right - widths.right, r.top, widths.right, r.bottom - r.top, PATCOPY);

		if (widths.bottom > 0)
			::PatBlt(hDC, r.left, r.bottom - widths.bottom, r.right - r.left, widths.bottom, PATCOPY);
		
        ::SelectObject(hDC, hOldBrush);
        VERIFY(::DeleteObject(hBrush));
	}
}

static void
DisplayDoubleBorder(HDC hDC, RECT &r, COLORREF color, LTRB &widths)
{
	LTRB	strokeWidths;

	// Compute the stroke width for each of the edges
	strokeWidths.left = (widths.left + 1) / 3;
	strokeWidths.top = (widths.top + 1) / 3;
	strokeWidths.right = (widths.right + 1) / 3;
	strokeWidths.bottom = (widths.bottom + 1) / 3;

	// Draw the outer lines
	DisplaySolidBorder(hDC, r, color, strokeWidths);

	// Adjust the rectangle to be the inner rectangle
	r.left += CASTINT(widths.left - strokeWidths.left);
	r.top += CASTINT(widths.top - strokeWidths.top);
	r.right -= CASTINT(widths.right - strokeWidths.right);
	r.bottom -= CASTINT(widths.bottom - strokeWidths.bottom);

	// Draw the inner lines
	DisplaySolidBorder(hDC, r, color, strokeWidths);
}

// rgbLight is used to render the top/left edges, and rgbDark is used to
// render the bottom/right edges
void
CDCCX::Display3DBorder(LTRB& Rect, COLORREF rgbLight, COLORREF rgbDark, LTRB &widths)
{
	if (widths.left == widths.top && widths.left == widths.right && widths.left == widths.bottom) {
		// This routine is faster especially for one-pixel thick lines	
		Display3DBox(Rect, rgbLight, rgbDark, widths.left, FALSE);

	} else {
		HDC	hDC = GetContextDC();

		HPEN    hOldPen = (HPEN)::SelectObject(hDC, GetStockObject(NULL_PEN));
		POINT   aPoints[6];
        HBRUSH  hbDark, hbLight, hOldBrush;
        
        // Make sure that in 256 color mode we get a PALETTE RGB color
        hbDark = ::CreateSolidBrush(0x02000000L | rgbDark);
        hbLight = ::CreateSolidBrush(0x02000000L | rgbLight);

		// Map all the points. Draw it inside of the rect. Draw the left
		// and top edges first
		aPoints[0].x = CASTINT(Rect.left + widths.left);
		aPoints[0].y = CASTINT(Rect.top + widths.top);
		aPoints[1].x = CASTINT(Rect.left + widths.left);
		aPoints[1].y = CASTINT(Rect.bottom - widths.bottom);
		aPoints[2].x = CASTINT(Rect.left);
		aPoints[2].y = CASTINT(Rect.bottom);
		aPoints[3].x = CASTINT(Rect.left);
		aPoints[3].y = CASTINT(Rect.top);
		aPoints[4].x = CASTINT(Rect.right);
		aPoints[4].y = CASTINT(Rect.top);
		aPoints[5].x = CASTINT(Rect.right - widths.right);
		aPoints[5].y = CASTINT(Rect.top + widths.top);
		
		hOldBrush = (HBRUSH)::SelectObject(hDC, hbLight);
		::Polygon(hDC, aPoints, 6);

		//	Right and bottom.
		aPoints[0].x = CASTINT(Rect.right - widths.right);
		aPoints[0].y = CASTINT(Rect.bottom - widths.bottom);
		aPoints[3].x = CASTINT(Rect.right);
		aPoints[3].y = CASTINT(Rect.bottom);
		
		::SelectObject(hDC, hbDark);
		::Polygon(hDC, aPoints, 6);

		::SelectObject(hDC, hOldPen);
		::SelectObject(hDC, hOldBrush);
        VERIFY(::DeleteObject(hbDark));
        VERIFY(::DeleteObject(hbLight));
		
		ReleaseContextDC(hDC);
	}
}

// Adjust border for selection feedback of table or cell
// Returns width used in excess of table border (into space between cells)
//  because that will have to be restored to background when removing selection
int32 SetSelectionBorder(int32 *pWidth, int32 iCellSpacing)
{
    int32 iOldWidth = *pWidth;
    int32 iExcess = 0;
    // Allow wider selection if border is very large
    int32 iMaxWidth = 2*ED_SELECTION_BORDER;
    // 
    *pWidth = min(*pWidth,iMaxWidth);
    // Also use spacing between cells if less than maximum
    if( *pWidth < 6 )
    {
        iExcess = min(iCellSpacing, 6-*pWidth);
        *pWidth += iExcess;
    }
    return iExcess;
}

int32
CDCCX::DisplayTableBorder(LTRB& Rect, LO_TableStruct *pTable)
{
    COLORREF    	rgbBorder = ResolveLOColor(&pTable->border_color);
	COLORREF		rgbLight, rgbDark;
    HDC         	hDC;
    RECT        	r = {CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom)};
    LTRB        	insetRect;
	LTRB	borderWidths(pTable->border_left_width, pTable->border_top_width,
								    pTable->border_right_width, pTable->border_bottom_width);
    //TRACE0("DisplayTableBorder\n");

    // Only the Editor sets table elements as selected, so we don't need EDT_IS_EDITOR
    if( pTable->ele_attrmask & LO_ELE_SELECTED )
    {
        // Use the selection background color to draw a solid border
        COLORREF rgbBorder;
        wfe_GetSelectionColors(m_rgbBackgroundColor, NULL, &rgbBorder);
		hDC = GetContextDC();
        // Use same border thickness for all sides - use the minimum Table border thickness
        //  and the space between cells if available
        int32 iBorderWidth = min(borderWidths.bottom,min(borderWidths.top,min(borderWidths.left, borderWidths.right)));
        SetSelectionBorder(&iBorderWidth, pTable->inter_cell_space);
        borderWidths.left = borderWidths.right = borderWidths.top = borderWidths.bottom = iBorderWidth;

	    // Show a solid border up to twice default selection border as the selection feedback
        DisplaySolidBorder(hDC, r, rgbBorder, borderWidths);
		ReleaseContextDC(hDC);
        // Return actual border width used
        return iBorderWidth;
    }
    else
    {
        if (pTable->border_style == BORDER_GROOVE || pTable->border_style == BORDER_RIDGE ||
	        pTable->border_style == BORDER_INSET || pTable->border_style == BORDER_OUTSET) {

	        // Compute the 3D colors to use. If the border color is the same as the background
	        // color then just use the light/dark colors we've already computed
	        if (rgbBorder == m_rgbBackgroundColor) {
		        rgbLight = m_rgbLightColor;
		        rgbDark = m_rgbDarkColor;
	        
	        } else {
		        Compute3DColors(rgbBorder, rgbLight, rgbDark);
	        }
        }

        hDC = GetContextDC();
        switch (pTable->border_style) {
            case BORDER_NONE:
                break;

            case BORDER_DOTTED:
            case BORDER_DASHED:
	        case BORDER_SOLID:
        //			hDC = GetContextDC();
		        DisplaySolidBorder(hDC, r, rgbBorder, borderWidths);
        //			ReleaseContextDC(hDC);
                break;

	        case BORDER_DOUBLE:
        //			hDC = GetContextDC();
		        DisplayDoubleBorder(hDC, r, rgbBorder, borderWidths);
        //			ReleaseContextDC(hDC);
                break;

            case BORDER_GROOVE:
                // This is done as a sunken outer border with a raised inner
                // border. Windows group boxes have this appearance
		        borderWidths.left /= 2;
		        borderWidths.top /= 2;
		        borderWidths.right /= 2;
		        borderWidths.bottom /= 2;
                Display3DBorder(Rect, rgbDark, rgbLight, borderWidths);
                insetRect.left = Rect.left + borderWidths.left;
                insetRect.top = Rect.top + borderWidths.top;
                insetRect.right = Rect.right - borderWidths.right;
                insetRect.bottom = Rect.bottom - borderWidths.bottom;
                Display3DBorder(insetRect, rgbLight, rgbDark, borderWidths);
                break;

            case BORDER_RIDGE:
                // This is done as a raised outer border with a sunken inner border
		        borderWidths.left /= 2;
		        borderWidths.top /= 2;
		        borderWidths.right /= 2;
		        borderWidths.bottom /= 2;
                Display3DBorder(Rect, rgbLight, rgbDark, borderWidths);
                insetRect.left = Rect.left + borderWidths.left;
                insetRect.top = Rect.top + borderWidths.top;
                insetRect.right = Rect.right - borderWidths.right;
                insetRect.bottom = Rect.bottom - borderWidths.bottom;
                Display3DBorder(insetRect, rgbDark, rgbLight, borderWidths);
                break;

            case BORDER_INSET:
                // This is what Windows refers to as a sunken border
		        Display3DBorder(Rect, rgbDark, rgbLight, borderWidths);
                break;

            case BORDER_OUTSET:
                // This is what Windows refers to as a raised border
		        Display3DBorder(Rect, rgbLight, rgbDark, borderWidths);
                break;

            default:
                ASSERT(FALSE);
                break;
        }
    }
    return 0;
}

void CDCCX::EditorDisplayZeroWidthBorder(LTRB& Rect, BOOL bSelected){
    // Show a 1-pixel dotted border. Used in the editor
    // Use the same color we would use for a selection background,
    //   but use the opposite color if we will be selecting,
    //   so NOT opperation yields a contrasting color relative to background
    COLORREF rgbColor;
    wfe_GetSelectionColors(m_rgbBackgroundColor, NULL, &rgbColor);

    HDC hdc = GetContextDC();
    // Use dotted line if not selected, or solid if selecting
	HPEN pPen = ::CreatePen(bSelected ? PS_SOLID : PS_DOT, 1, rgbColor);
	HPEN pOldPen = (HPEN)::SelectObject(hdc, pPen);
    
    ::Rectangle(hdc, Rect.left, Rect.top, Rect.right, Rect.bottom);

	::SelectObject(hdc, pOldPen);
	ReleaseContextDC(hdc);
	VERIFY(::DeleteObject(pPen));
}

void CDCCX::DrawRectBorder(LTRB& Rect, COLORREF rgbColor, int32 lWidth)	{
	//	This should only be called for display of the border for images, and embeds.
	HDC hdc = GetContextDC();

	// The border goes outside of the rectangle, so we need to adjust the
	// rectangle accordingly
	int32 lAdjust = lWidth / 2;
	int32 rAdjust = lAdjust;
	if(lWidth % 2)	{
		lAdjust += Pix2TwipsX(1);
	}
	rAdjust += Pix2TwipsX(1);

	LTRB rect(Rect.left - lAdjust, Rect.top - lAdjust,
		Rect.right + rAdjust, Rect.bottom + rAdjust);
	SafeSixteen(rect);

	HPEN hBorder = ::CreatePen(PS_SOLID, CASTINT(lWidth), rgbColor);
	HPEN hOldPen = (HPEN)::SelectObject(hdc, hBorder);
	HBRUSH hOldBrush = (HBRUSH) ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));

	//	Draw it.
    ::Rectangle(hdc, CASTINT(rect.left), CASTINT(rect.top),
        CASTINT(rect.right), CASTINT(rect.bottom));

	::SelectObject(hdc, hOldBrush);
	::SelectObject(hdc, hOldPen);
	VERIFY(::DeleteObject(hBorder));
	ReleaseContextDC(hdc);

}

void CDCCX::DisplaySelectionFeedback(uint16 ele_attrmask, const LTRB& rect)
{
    BOOL bSelect =  ele_attrmask & LO_ELE_SELECTED;
    // Don't do anything if unselecting any other objects
    //  since these will be completely redrawn.
    if ( !bSelect || !EDT_IS_EDITOR(GetContext()) ) {
        return;
    }
	LTRB Rect(rect.left, rect.top,
		rect.right, rect.bottom);

	SafeSixteen(Rect);
    // Draw selection feedback. It has to fit entirely within rect, because
    // when it's turned off, all that will happen is that it won't be drawn.
	HDC hdc = GetContextDC();

    int32 rWidth = Rect.right - Rect.left;
    int32 rHeight = Rect.bottom - Rect.top;
    if ( rWidth <= (ED_SELECTION_BORDER*2) || rHeight <= (ED_SELECTION_BORDER*2) ) {
        // Too narrow to draw frame. Draw solid.
        RECT r;
		::SetRect(&r, CASTINT(Rect.left), CASTINT(Rect.top),
            CASTINT(Rect.right), CASTINT(Rect.bottom));
        ::InvertRect(hdc, &r);
    }
    else {
    	//	Frame.
    	LTRB inner(Rect.left + ED_SELECTION_BORDER, Rect.top + ED_SELECTION_BORDER,
    		Rect.right - ED_SELECTION_BORDER, Rect.bottom - ED_SELECTION_BORDER);
    	SafeSixteen(inner);

        RECT SelRect;
		::SetRect(&SelRect, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(inner.top));
  		::InvertRect(hdc, &SelRect);
        ::SetRect(&SelRect, CASTINT(Rect.left), CASTINT(inner.top), CASTINT(inner.left), CASTINT(inner.bottom));
  		::InvertRect(hdc, &SelRect);
        ::SetRect(&SelRect, CASTINT(inner.right), CASTINT(inner.top), CASTINT(Rect.right), CASTINT(inner.bottom));
  		::InvertRect(hdc, &SelRect);
        ::SetRect(&SelRect, CASTINT(Rect.left), CASTINT(inner.bottom), CASTINT(Rect.right), CASTINT(Rect.bottom));
  		::InvertRect(hdc, &SelRect);
    }

	ReleaseContextDC(hdc);
}

void CDCCX::EraseToBackground(const LTRB& rect, int32 borderWidth)
{
	if(!EDT_IS_EDITOR(GetContext())){
        return;
    }
    LTRB Rect(rect.left - borderWidth, rect.top - borderWidth,
		        rect.right + borderWidth, rect.bottom + borderWidth);

    // The editor draws selection feedback. Touch all the pixels so that
    // any pre-existing selection feedback is erased.
	HDC hdc = GetContextDC();
    
    RECT	 destRect;
	::SetRect(&destRect, CASTINT(Twips2PixX(Rect.left)), CASTINT(Twips2PixY(Rect.top)),
        CASTINT(Twips2PixX(Rect.right)), CASTINT(Twips2PixX(Rect.bottom)));

    // Is this really true? (Troy thinks conversion is only needed for 
    //  memory DC)
    // Coordinates to _EraseBkgnd() must be in device units
    // NOTE: _EraseBkgnd() call will use m_pImageDC, that's why we
    // aren't using it
    _EraseBkgnd(hdc,destRect,
			Twips2PixX(GetOriginX() ),
			Twips2PixY(GetOriginY() ),
			NULL);
	ReleaseContextDC(hdc);
}


#ifdef LAYERS
// Erase the background at the given rect. x, y, width and height are
// in document coordinates.
void CDCCX::EraseBackground(MWContext *pContext, int iLocation, 
			    int32 x, int32 y, uint32 width, uint32 height,
			    LO_Color *pColor)
{
	HDC hdc = GetContextDC();
	int32 orgX = GetOriginX();
	int32 orgY = GetOriginY();
	int32 lDrawingOrgX, lDrawingOrgY;
	RECT   eraseRect;
	int32 x0, y0;

	x0 = (x > orgX) ? x - orgX : orgX - x;
	y0 = (y > orgY) ? y - orgY : orgY - y;
	::SetRect(&eraseRect, CASTINT(x0), CASTINT(y0), 
						CASTINT(x0+width), CASTINT(y0+height));
	GetDrawingOrigin(&lDrawingOrgX, &lDrawingOrgY);
	_EraseBkgnd(hdc, eraseRect, orgX-lDrawingOrgX, orgY-lDrawingOrgY, 
				pColor);

	ReleaseContextDC(hdc);
}
#endif /* LAYERS */

static BOOL
CanCreateBrush(HBITMAP hBitmap)
{
	BITMAP	bitmap;

	// We can create a brush if the bitmap is exacly 8x8
	::GetObject(hBitmap, sizeof(bitmap), &bitmap);
	return bitmap.bmWidth == 8 && bitmap.bmHeight == 8;
}


// orgX, orgY, and crRect are interpreted in the logical coordinates
// of pDC
//
// Note: The caller is responsible for having selected the logical palette
// before calling this routine. If not, the call to CreateSolidBrush() will
// get mapped to the nearest system color (probably not the desired behavior)
BOOL CDCCX::_EraseBkgnd(HDC pDC, RECT& crRect, int32 orgX, int32 orgY, 
						LO_Color* bg)
{
	// The order of priority is as follows:
	//   1. passed in image
	//   2. passed in color
	//   3. document backdrop image
	//   4. document solid color
	COLORREF	bgColor = bg ? RGB(bg->red, bg->green, bg->blue) : m_rgbBackgroundColor;
	
	HBRUSH hBrush = NULL;
	if (m_iBitsPerPixel == 16)
		// We don't want a dithered brush
		hBrush = ::CreateSolidBrush(::GetNearestColor(pDC,bgColor));
	else
		hBrush = ::CreateSolidBrush(0x02000000L | bgColor);

	::FillRect(pDC, &crRect, hBrush);

#ifdef DDRAW
	if (GetPrimarySurface()) {
		::FillRect(GetDispDC(), &crRect, hBrush);
	}
#if 0 //DEBUG_mhwang
	TRACE("CDCCX::_EraseBkgnd, %d,%d,%d,%d(ltrb)\n", crRect.left, crRect.top,
								crRect.right, crRect.bottom);
#endif
#endif
	VERIFY(::DeleteObject(hBrush));
	return TRUE;
}





struct iconMap{
	int iIconNum;
	int x;
	int y;
	unsigned int bitmapID;
	unsigned int maskID;
}

#define SMIME_ICON_CX	50
#define SMIME_ICON_CY	30
#define SECADV_ICON_CX	51
#define SECADV_ICON_CY	30

#define ICONMAPSIZE 31

static iconMap[ICONMAPSIZE] = {
	IL_IMAGE_DELAYED, IMAGE_SMICON_WIDTH, IMAGE_SMICON_HEIGHT,IDB_SMIMAGE_DELAYED, IDB_SMIMAGE_MASK,
	IL_IMAGE_NOT_FOUND, IMAGE_ICON_SIZE, IMAGE_ICON_SIZE, IDB_IMAGE_NOTFOUND,IDB_IMAGE_MASK,
	IL_IMAGE_BAD_DATA, IMAGE_ICON_SIZE, IMAGE_ICON_SIZE, IDB_IMAGE_BAD, IDB_IMAGE_BAD_MASK,
	IL_IMAGE_INSECURE, IMAGE_ICON_SIZE, IMAGE_ICON_SIZE, IDB_IMAGE_SEC_REPLACE,IDB_IMAGE_MASK,
	IL_GOPHER_FOLDER,GOPHER_ICON_SIZE, GOPHER_ICON_SIZE + 1, IDB_GOPHER_FOLDER,IDB_GOPHER_FOLDER_MASK,
	IL_GOPHER_IMAGE, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE + 1, IDB_GOPHER_IMAGE,IDB_GOPHER_AUDIO_MASK,
	IL_GOPHER_TEXT, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_GOPHER_TEXT, IDB_GOPHER_AUDIO_MASK,
	IL_GOPHER_BINARY, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_GOPHER_BINARY, IDB_GOPHER_AUDIO_MASK,
	IL_GOPHER_SOUND,  GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_GOPHER_AUDIO, IDB_GOPHER_AUDIO_MASK,
	IL_GOPHER_MOVIE, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_GOPHER_FIND, IDB_GOPHER_FIND_MASK,
	IL_GOPHER_TELNET, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_GOPHER_TELNET,IDB_GOPHER_TELNET_MASK,
	IL_GOPHER_UNKNOWN, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_GOPHER_GENERIC, IDB_GOPHER_AUDIO_MASK,
	IL_EDIT_NAMED_ANCHOR, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_EDIT_NAMED_ANCHOR, 	IDB_EDIT_NAMED_ANCHOR_MASK,
	IL_EDIT_FORM_ELEMENT, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_EDIT_FORM_ELEMENT,IDB_EDIT_FORM_ELEMENT_MASK,
	IL_EDIT_UNSUPPORTED_TAG, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_EDIT_UNSUPPORTED_TAG, IDB_EDIT_UNSUPPORTED_TAG_MASK,
	IL_EDIT_UNSUPPORTED_END_TAG, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_EDIT_UNSUPPORTED_END_TAG, IDB_EDIT_UNSUPPORTED_END_TAG_MASK,
	IL_EDIT_JAVA, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_EDIT_JAVA, IDB_EDIT_JAVA_MASK,
	IL_EDIT_PLUGIN, GOPHER_ICON_SIZE, GOPHER_ICON_SIZE+1, IDB_EDIT_PLUGIN,IDB_EDIT_PLUGIN_MASK,

	IL_SA_SIGNED, SECADV_ICON_CX, SECADV_ICON_CY, IDB_SASIGNED, IDB_SASIGNED_MASK,
	IL_SA_ENCRYPTED, SECADV_ICON_CX, SECADV_ICON_CY, IDB_SAENCRYPTED, IDB_SAENCRYPTED_MASK,
	IL_SA_NONENCRYPTED, SECADV_ICON_CX, SECADV_ICON_CY, IDB_SAUNENCRYPTED, IDB_SAUNENCRYPTED_MASK,
	IL_SA_SIGNED_BAD, SECADV_ICON_CX, SECADV_ICON_CY, IDB_SASIGNED, IDB_SASIGNED_MASK,
	IL_SA_ENCRYPTED_BAD, SECADV_ICON_CX, SECADV_ICON_CY, IDB_SASIGNEDBAD, IDB_SASIGNEDBAD_MASK,
	IL_SMIME_ATTACHED, SMIME_ICON_CX, SMIME_ICON_CY, IDB_MSGATTACHED, IDB_MSGSTAMPMASK,
	IL_SMIME_SIGNED, SMIME_ICON_CX, SMIME_ICON_CY, IDB_MSGSIGNED, IDB_MSGSTAMPMASK,
	IL_SMIME_ENCRYPTED, SMIME_ICON_CX, SMIME_ICON_CY, IDB_MSGENCRYPTED, IDB_MSGSTAMPMASK,
	IL_SMIME_ENC_SIGNED, SMIME_ICON_CX, SMIME_ICON_CY, IDB_MSGSIGNEDENCRYPTED, IDB_MSGSTAMPMASK,
	IL_SMIME_SIGNED_BAD, SMIME_ICON_CX, SMIME_ICON_CY, IDB_MSGSIGNEDBAD, IDB_MSGSTAMPMASK,
	IL_SMIME_ENCRYPTED_BAD, SMIME_ICON_CX, SMIME_ICON_CY, IDB_MSGENCRYPTEDBAD, IDB_MSGSTAMPMASK,
	IL_SMIME_ENC_SIGNED_BAD, SMIME_ICON_CX, SMIME_ICON_CY, IDB_MSGSIGNEDENCRYPTEDBAD, IDB_MSGSTAMPMASK,

	IL_MSG_ATTACH, 24, 29, IDB_MSGATTACHBUTTON, IDB_MSGATTACHBUTTONMASK
};



void CDCCX::GetIconDimensions(int32* width, int32* height, int iconNumber)
{
    unsigned int bitmapID;    // resource ID of bitmap to load
    unsigned int maskID = 0;  // resource ID of mask (if any)

    if (iconNumber < 0)
        return;

    int x, y;

	for (int i = 0; i < ICONMAPSIZE ; i++) {
		if (iconMap[i].iIconNum == iconNumber) {
			x = iconMap[i].x;
			y = iconMap[i].y;
			bitmapID = iconMap[i].bitmapID;
			maskID = iconMap[i].maskID;
			break;
		}
	}

	if (i == ICONMAPSIZE) { // used default icon here.
		x = GOPHER_ICON_SIZE;
        y = GOPHER_ICON_SIZE + 1;
        bitmapID = IDB_FILE;
        maskID = IDB_GOPHER_AUDIO_MASK;
	}


	*width = CASTINT(x);
	*height = CASTINT(y);
}

#ifndef XP_WIN32
/* Quaternary raster codes */
#define MAKEROP4(fore,back) (DWORD)((((back) << 8) & 0xFF000000) | (fore))
#endif

static BOOL
AlterBackgroundColor(HDC pSrcDC, int width, int height, HBITMAP hMask, HBRUSH hBrush)
{
	HDC	memDC;

	// Create a memory DC for holding the mask
	if (!(memDC = ::CreateCompatibleDC(NULL))) {
		TRACE("AlterBackgroundColor() can't create compatible memory DC!\n");
		return FALSE;
	}

	// Select the mask into the memory DC
	HBITMAP	hOldMaskBitmap = (HBITMAP)::SelectObject(memDC, hMask);

	// Change the transparent bits of the source bitmap to the desired background
	// color by doing an AND raster op
	//
	// The mask is monochrome and will be converted to color to match the depth of
	// the destination. The foreground pixels in the mask are set to 1, and the
	// background pixels to 0. When converting to a color bitmap, white pixels (1)
	// are set to the background color of the DC, and black pixels (0) are set to
	// the text color of the DC
	//
	// Select the brush
	HBRUSH	hOldBrush = (HBRUSH)::SelectObject(pSrcDC, hBrush);

	// Draw the brush where the mask is 0
	::BitBlt(pSrcDC,
				   0,
				   0,
				   width,
				   height,
				   memDC,
				   0,
				   0,
//				   ROP_PSDPxax);
				   MAKEROP4(SRCPAINT, R2_COPYPEN));

	// Restore the DCs
	::SelectObject(memDC, hOldMaskBitmap);
	::SelectObject(pSrcDC, hOldBrush);

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// equivalent to LoadBitmap(), except uses pDC for
// correct color mapping of BMP into pDC's palette.
BOOL CDCCX::cxLoadBitmap(LPCSTR pszBmName, char** bits,  BITMAPINFOHEADER** myBitmapInfo) 
{

    // get the resource from the file
    HBITMAP hBmp = (HBITMAP)FindResource(AfxGetInstanceHandle( ), pszBmName, RT_BITMAP);
    ASSERT(hBmp != NULL);
    if(!hBmp)
        return FALSE;
    HGLOBAL hRes = LoadResource(AfxGetInstanceHandle( ), (HRSRC)hBmp);
    ASSERT(hRes != NULL);
    if(!hRes)
        return FALSE;
    LPBITMAPINFO pPackedDib = (LPBITMAPINFO)(LockResource(hRes));
    ASSERT(pPackedDib != NULL);
    if(!pPackedDib)
        return FALSE;
        
    // build a DDB header for  pDC
    
    // create the DDB
    int iColorTableSize =
        (1 << pPackedDib->bmiHeader.biBitCount) * sizeof(RGBQUAD);
	LPBITMAPINFOHEADER bitmapInfo;
	bitmapInfo = (LPBITMAPINFOHEADER)calloc(sizeof(BITMAPINFOHEADER) + iColorTableSize, 1);;
    memcpy(bitmapInfo, pPackedDib, sizeof(BITMAPINFOHEADER) + iColorTableSize);
    bitmapInfo->biBitCount = m_iBitsPerPixel;
    bitmapInfo->biClrUsed = 0;
    bitmapInfo->biClrImportant = 0;
	*myBitmapInfo = bitmapInfo;
    void* pBits =
        ((char*)pPackedDib) + pPackedDib->bmiHeader.biSize + iColorTableSize;
	int widthBytes = (((bitmapInfo->biWidth * bitmapInfo->biBitCount) + 31) && ~31) >> 3;
	*bits =(char*) CDCCX::HugeAlloc(widthBytes * bitmapInfo->biHeight, 1);
	memcpy(bits, pBits, widthBytes * bitmapInfo->biHeight);
    // done, clean up
	UnlockResource(hRes);
    BOOL bResult = FreeResource(hRes);
    
    return TRUE;
}

void CDCCX::DisplayIcon(int32 x0, int32 y0, int icon_number)
{
    unsigned int bitmapID;    // resource ID of bitmap to load
    unsigned int maskID = 0;  // resource ID of mask (if any)

    if (icon_number < 0)
        return;

    int x, y;
	LTRB Rect;

    for (int i = 0; i < ICONMAPSIZE ; i++) {
        if (iconMap[i].iIconNum == icon_number) {
            x = iconMap[i].x;
            y = iconMap[i].y;
            bitmapID = iconMap[i].bitmapID;
            maskID = iconMap[i].maskID;
            break;
        }
    }

    if (i == ICONMAPSIZE) { // used default icon here.
        x = GOPHER_ICON_SIZE;
        y = GOPHER_ICON_SIZE + 1;
        bitmapID = IDB_FILE;
        maskID = IDB_GOPHER_AUDIO_MASK;
    }

    int width = CASTINT(Pix2TwipsX(x));
    int height = CASTINT(Pix2TwipsY(y));

	if (ResolveElement(Rect, x0, y0, 0, 0, width, height)) {
		SafeSixteen(Rect);
        
		HBITMAP hBitmap = NULL;
		HDC hdc = GetContextDC();    

#ifdef XP_WIN32
		if (icon_number == IL_IMAGE_DELAYED) {
			static HICON	hIcon=NULL;

			// The icon is represented as a small icon resource which is 16x16 pixels
			if(!hIcon)
				hIcon = (HICON)::LoadImage(AfxGetResourceHandle(),
					MAKEINTRESOURCE(IDI_IMAGE_DELAYED), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

			VERIFY(::DrawIconEx(hdc, CASTINT(Rect.left), CASTINT(Rect.top),
				hIcon, Pix2TwipsX(16), Pix2TwipsY(16), 0, NULL, DI_NORMAL));

			/* don't destroy hIcon because it's static */

//			return;
		}
		else
#endif
		{
			// load the bitmap out of the resource file
			HBITMAP mask;
			if (!IsPrintContext()) {
				hBitmap = LOADBITMAP(bitmapID);
				if(maskID) 
					mask = LOADBITMAP(maskID);
		
			HBITMAP hOldBitmap = NULL;                                       

			if(maskID) {
				if (IsPrintContext()) {
						HBITMAP hOldBitmap = (HBITMAP)::SelectObject(m_pImageDC, hBitmap);
						// filled in the background color for transparent bitmap.
						AlterBackgroundColor(m_pImageDC, x, y, mask, 
							(HBRUSH)::GetStockObject(WHITE_BRUSH));
						::StretchBlt(hdc,
									CASTINT(Rect.left), 
    								CASTINT(Rect.top),
    								CASTINT(width), 
    								CASTINT(height), 
    								m_pImageDC,
    								CASTINT(0), 
    								CASTINT(0), 
    								CASTINT(x),
    								CASTINT(y), 
    								SRCCOPY);
						::SelectObject(m_pImageDC, hOldBitmap);
				       }
				       else {
					        // load the mask out of the resource file
					        StretchMaskBlt(hdc, hBitmap, mask, 
							Rect.left, Rect.top, width, height,
							0, 0, x, y);
                                        }
 					VERIFY(::DeleteObject(mask));
                                } 
				else {
    				// load the bitmap into the cached image CDC                                                   
					hOldBitmap = (HBITMAP) ::SelectObject(m_pImageDC, hBitmap);

					::StretchBlt(hdc,
							CASTINT(Rect.left), 
    							CASTINT(Rect.top),
    							CASTINT(width), 
    							CASTINT(height), 
    							m_pImageDC,
    							CASTINT(0), 
    							CASTINT(0), 
    							CASTINT(x),
    							CASTINT(y), 
    							SRCCOPY);
					::SelectObject(m_pImageDC, hOldBitmap);
				}

				// restore the old bitmap
				VERIFY(::DeleteObject(hBitmap));
			}
			else {	// printing icon
				char** image_bits;
				BITMAPINFOHEADER* imageInfo;
				char** mask_bits;
				BITMAPINFOHEADER* maskInfo;
				if (cxLoadBitmap(MAKEINTRESOURCE(bitmapID), image_bits, &imageInfo)) {
					if(maskID) {
						BOOL fillBack = TRUE;
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
#ifdef XP_WIN32
						CPrintCX* pPrintCx = (CPrintCX*)this;
						fillBack = pPrintCx->IsPrintingBackground() ? FALSE : TRUE;
#endif
#endif
						cxLoadBitmap(MAKEINTRESOURCE(maskID), mask_bits, &maskInfo);
						WFE_StretchDIBitsWithMask(hdc,TRUE, m_pImageDC,
											CASTINT(Rect.left), 
    										CASTINT(Rect.top),
    										CASTINT(width), 
    										CASTINT(height), 
    										CASTINT(0), 
    										CASTINT(0), 
    										CASTINT(x),
    										CASTINT(y), 
											image_bits,
											(BITMAPINFO*)imageInfo,
											mask_bits,
											m_bUseDibPalColors,
											fillBack);
					}
					else {
						::StretchDIBits(hdc,
											CASTINT(Rect.left), 
    										CASTINT(Rect.top),
    										CASTINT(width), 
    										CASTINT(height), 
    										CASTINT(0), 
    										CASTINT(0), 
    										CASTINT(x),
    										CASTINT(y), 
											image_bits,
											(BITMAPINFO*)imageInfo,
											DIB_RGB_COLORS,
											SRCCOPY);
					}
				}

				if (image_bits)
					delete *image_bits;
				if (imageInfo)
					delete imageInfo;
				if(maskID) {
					if (mask_bits)
						delete *mask_bits;
					if (maskInfo)
						delete maskInfo;
				}
			}
		}
		ReleaseContextDC(hdc);
	}
}
	
int CDCCX::GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoading, BOOL bForceNew)	{
	//	Save the location of the current document, if not at the very top.
    //  Reset to top, then see if we need to change more....
    SHIST_SetPositionOfCurrentDoc(&(GetContext()->hist), 0);

#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    if(GetOriginX() || GetOriginY())    {
#ifdef LAYERS
	    LO_Any *pAny = (LO_Any *)LO_XYToNearestElement(GetDocumentContext(), GetOriginX(), GetOriginY(), NULL);
#else
	    LO_Any *pAny = (LO_Any *)LO_XYToNearestElement(GetDocumentContext(), GetOriginX(), GetOriginY());
#endif  /* LAYERS */
	    if(pAny != NULL)	{
		    TRACE("Remembering document position at element id %ld\n", pAny->ele_id);
		    SHIST_SetPositionOfCurrentDoc(&(GetContext()->hist), pAny->ele_id);
	    }
    }
#endif /* MOZ_NGLAYOUT */

	//	Handle forced image loading.
	m_csForceLoadImage = m_csNexttimeForceLoadImage;
	m_csNexttimeForceLoadImage.Empty();
	m_bLoadImagesNow = m_bNexttimeLoadImagesNow;
	m_bNexttimeLoadImagesNow = FALSE;

	//	Call the base.
	return(CStubsCX::GetUrl(pUrl, iFormatOut, bReallyLoading, bForceNew));
}

//	rewrite the BULLET_BASIC and BULLET_DISC part
//	the origional "o" approach cause a lot of problem in non-LATIN page
void CDCCX::DisplayBullet(MWContext *pContext, int iLocation, LO_BullettStruct *pBullet)	{
	//	Figure the coordinates.
	LTRB Rect;
	int		reduce;
//	if(ResolveElement(Rect, pBullet, iLocation) == TRUE)	{
	if (ResolveElement(Rect, pBullet->x, pBullet->y, 
				pBullet->x_offset, pBullet->y_offset,
				pBullet->width, pBullet->height) == TRUE) {
		SafeSixteen(Rect);

		COLORREF rgbBullet = ResolveTextColor(pBullet->text_attr);
		HDC hdc = GetContextDC();

		int w;
		if( pBullet->bullet_type == BULLET_MQUOTE 
			|| pBullet->bullet_type == BULLET_SQUARE ) {
			w = 1;
		} else {
			w = CASTINT( (Rect.bottom - Rect.top) / 10);
			if(w < 1)
				w = 1;
		}

		HPEN hpen = ::CreatePen(PS_SOLID,w,rgbBullet);
		HPEN oldPen = (HPEN)::SelectObject(hdc,hpen);      

		// draw_bullet_with_Ellipse
		HBRUSH hBrush = ::CreateSolidBrush(rgbBullet);
		HBRUSH hOldBrush = (HBRUSH)::SelectObject(hdc, hBrush);

		switch(pBullet->bullet_type)	{
		
		case BULLET_MQUOTE:
			// BUG#61210 draw quote as one pixel bar on the left
#if defined (WIN32)
			::MoveToEx(hdc, CASTINT(Rect.left), CASTINT(Rect.top), NULL);
#else
			::MoveTo(hdc, CASTINT(Rect.left), CASTINT(Rect.top));
#endif
			::LineTo(hdc,CASTINT(Rect.left), CASTINT(Rect.bottom));
			break;
		case BULLET_SQUARE:	
			// Size of square is too large relative to solid and open circle,
			//  so reduce a little
			Rect.top++;
			Rect.bottom--;
			Rect.left++;
			Rect.right--;
			::Rectangle(hdc, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
			break;

		case BULLET_NONE:
			// Do nothing
			break;

		case BULLET_BASIC:		// solid bullet
		case BULLET_ROUND :		// todo ::RoundRect()?
		default :				// Circle
			if( BULLET_BASIC != pBullet->bullet_type )  {   // Circle
				// don't touch hOldBrush, put hOldBrush back after draw.
				::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
			}
			reduce = (Rect.bottom - Rect.top ) / 4;
			::Ellipse(hdc, CASTINT(Rect.left+reduce), CASTINT(Rect.top+reduce),
				           CASTINT(Rect.right-reduce), CASTINT(Rect.bottom-reduce));
			break;
		}
		::SelectObject(hdc, hOldBrush);
		VERIFY(::DeleteObject( hBrush ));

		::SelectObject(hdc,oldPen);      
       ::DeleteObject(hpen);
		// end of draw_bullet_with_Ellipse

		ReleaseContextDC(hdc);
	}
}

#ifdef Frank_code
void CDCCX::DisplayBullet(MWContext *pContext, int iLocation, LO_BullettStruct *pBullet)	{
	//	Figure the coordinates.
	LTRB Rect;
//	if(ResolveElement(Rect, pBullet, iLocation) == TRUE)	{
	if (ResolveElement(Rect, pBullet->x, pBullet->y, 
				pBullet->x_offset, pBullet->y_offset,
				pBullet->width, pBullet->height) == TRUE) {
		SafeSixteen(Rect);

		COLORREF rgbBullet = ResolveTextColor(pBullet->text_attr);
		HDC hdc = GetContextDC();


        Rect.top++;
        Rect.bottom--;
        Rect.left++;
        Rect.right--;

		switch(pBullet->bullet_type)	{
		
        case BULLET_MQUOTE:
		case BULLET_SQUARE:	{
			HBRUSH hBrush = ::CreateSolidBrush(rgbBullet);
			HBRUSH hOldBrush = (HBRUSH)::SelectObject(hdc, hBrush);

			::Rectangle(hdc, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
			::SelectObject(hdc, hOldBrush);
			VERIFY(::DeleteObject( hBrush ));
			break;
		}

		case BULLET_BASIC:	{
			HBRUSH hBrush = ::CreateSolidBrush(rgbBullet);
			HBRUSH hOldBrush = (HBRUSH)::SelectObject(hdc, hBrush);

			::Ellipse(hdc, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
			::SelectObject(hdc, hOldBrush);
			VERIFY(::DeleteObject( hBrush ));
			break;

        }

		default:	{
			int w = CASTINT( (Rect.bottom - Rect.top) / 10);
			if(w < 1)
				w = 1;
			HBRUSH hBrush = (HBRUSH)::GetStockObject(NULL_BRUSH);
			HBRUSH hOldBrush = (HBRUSH)::SelectObject(hdc, hBrush);
			HPEN hStrike = ::CreatePen(PS_SOLID, CASTINT( w ), rgbBullet);
			HPEN hOldStrike = (HPEN)::SelectObject(hdc, hStrike);

			::Ellipse(hdc, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
			::SelectObject(hdc, hOldBrush);
			// Don't delete hBrush since it is a stock object

			::SelectObject(hdc, hOldStrike);
			VERIFY(::DeleteObject( hStrike ));
			break;

			}
		}

		ReleaseContextDC(hdc);
	}
}
#endif

// Display a border of the specified size and color.  x, y, width and height
// refer to the outside perimeter of the border.
void CDCCX::DisplayBorder(MWContext *pContext, int iLocation, int x, int y, int width, int height, int bw, LO_Color *color, LO_LineStyle style)
{
    LTRB Rect;
    
    if (bw) {
        // Adjust the input parameters so that they describe the inside
        // perimeter of the border.  Don't do this for 3D borders since
        // Draw3DBox does this for us.
        if (style != LO_BEVEL) {
            x += bw;
            y += bw;
            width -= 2 * bw;
            height -= 2 * bw;
        }

        if (ResolveElement(Rect, x, y, 0, 0, width, height)) {
            SafeSixteen(Rect);

            // Call the method which really draws the border.
            switch(style) {
            case LO_SOLID:
                DrawRectBorder(Rect, ResolveLOColor(color), bw);
                break;

            case LO_BEVEL:
                Display3DBox(Rect, ResolveDarkLineColor(),
                             ResolveLightLineColor(), Pix2TwipsX(bw), FALSE);
                break;

            default:
                break;
            }
        }
    }
}

void CDCCX::DisplayImageFeedback(MWContext *pContext, int iLocation, LO_Element *pElement, lo_MapAreaRec * theArea, uint32 drawFlag )
{
	if (pElement->lo_any.type == LO_IMAGE) {
		LO_ImageStruct *pImage = (LO_ImageStruct *)pElement;
		LTRB Rect;
		int borderWidth = CASTINT(pImage->border_width);
		int x = CASTINT(pImage->x + pImage->x_offset + borderWidth);
		int y = CASTINT(pImage->y + pImage->y_offset + borderWidth);

		if (ResolveElement(Rect, x, y, 0, 0, pImage->width, pImage->height)) {
			SafeSixteen(Rect);

            DisplaySelectionFeedback(pImage->ele_attrmask, Rect);

//#ifndef NO_TAB_NAVIGATION
            if((FE_DRAW_TAB_FOCUS & drawFlag) &&
               pImage->suppress_mode != LO_SUPPRESS) {
			
                // use the Rect from code above.
                if( theArea != NULL) {
                    //  It is a focused area in map, draw a focus border.
					DrawMapAreaBorder( CASTINT(Rect.left), CASTINT(Rect.top), theArea );
                } else {
                    // draw a focus border for the whole image
                    lo_MapAreaRec	area;
                    int32			coord[4];
                    area.coords		= coord;
                    area.type = AREA_SHAPE_RECT ;
                    area.coord_cnt = 4;
                    area.coords[0] = Rect.left;
                    area.coords[1] = Rect.top;
                    area.coords[2] = Rect.right;
                    area.coords[3] = Rect.bottom;
                    DrawMapAreaBorder( 0, 0, &area );
                }		
            }
//#endif	/* NO_TAB_NAVIGATION */
        }
    }
}


BOOL CDCCX::IsPluginFullPage(LO_EmbedStruct *pLayoutData)
{
    // 1x1 size is signal that plugin is full page, not embedded
    if((pLayoutData->objTag.width == Pix2TwipsX(1))
        && (pLayoutData->objTag.height == Pix2TwipsX(1)))
        return TRUE;
    return FALSE;
}

// returns TRUE if there is a plugin on the page and that plugin
// is of pluginType == NPWFE_FULL_PAGE.  There can be only one
// plugin on a page that contains a NPWFE_FULL_PAGE plugin.
BOOL CDCCX::ContainsFullPagePlugin()
{
    NPEmbeddedApp* pEmbeddedApp = GetContext()->pluginList;
    if(pEmbeddedApp) {
        if(pEmbeddedApp->pagePluginType == NP_FullPage)
            return TRUE;
    }
    return FALSE;
}

//  Determine location of plugin rect, convert from TWIPS to PIXELS
void CDCCX::GetPluginRect(MWContext *pContext,
                          LO_EmbedStruct *pLayoutData,
                          RECT &rect, int iLocation,
                          BOOL windowed)
{
    if(IsPluginFullPage(pLayoutData))
    {
		 ::GetClientRect(PANECX(pContext)->GetPane(), &rect);
    }
    else // plugin embedded in page
    {
        // get the display area and clamp it
        LTRB Rect;
        ResolveElement(Rect, pLayoutData, iLocation, windowed);

		SafeSixteen(Rect);
        rect.left   = (int)Rect.left;
        rect.top    = (int)Rect.top;
        rect.right  = (int)Rect.right;
        rect.bottom = (int)Rect.bottom;
    }
}

void CDCCX::DisplayWindowlessPlugin(MWContext *pContext, 
                                    LO_EmbedStruct *pEmbed,
                                    NPEmbeddedApp *pEmbeddedApp,
                                    int iLocation)
{
    RECT 		rect;
    NPWindow* 	pAppWin = pEmbeddedApp->wdata;
    HDC         hDC = GetContextDC();
    NPEvent event;
    FE_Region clip;
    
    // Get the coordinates of where it should be positioned
    GetPluginRect(pContext, pEmbed, rect, iLocation, FALSE);
    
    if ((pAppWin->window	!= (void *)hDC) ||
        (pAppWin->x			!= (uint32) rect.left) ||
        (pAppWin->y			!= (uint32) rect.top) ||
        (pAppWin->width		!= (uint32) (rect.right - rect.left)) ||
        (pAppWin->height	!= (uint32) (rect.bottom - rect.top))) 
    {
        pAppWin->window = (void *)hDC;
        pAppWin->x = rect.left;
        pAppWin->y = rect.top;
        pAppWin->width = rect.right - rect.left;
        pAppWin->height = rect.bottom - rect.top;
        pAppWin->type = NPWindowTypeDrawable;
        
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
        NPL_EmbedSize(pEmbeddedApp);
#endif
    }
    
#ifdef LAYERS
    clip = GetDrawingClip();
    if (clip) 
    {
        XP_Rect xprect;
        
        FE_GetRegionBoundingBox(clip, &xprect);
        rect.left = CASTINT(xprect.left);
        rect.top = CASTINT(xprect.top);
        rect.right = CASTINT(xprect.right);
        rect.bottom = CASTINT(xprect.bottom);
    }
#endif /* LAYERS */        
    
    event.event = WM_PAINT;
    event.wParam = (uint32)hDC;
    event.lParam = (uint32)&rect;
    
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
#else
    NPL_HandleEvent(pEmbeddedApp, (void *)&event, pAppWin->window);
#endif

    ReleaseContextDC(hDC);
}


// create a window and pass its offset/size to the plugin
void CDCCX::DisplayPlugin(MWContext *pContext, LO_EmbedStruct *pEmbed,
					      NPEmbeddedApp* pEmbeddedApp, int iLocation)
{
    RECT 		rect;
    NPWindow* 	pAppWin = pEmbeddedApp->wdata;

   	if(pAppWin->window == NULL)
        return;

    if (::IsWindowVisible((HWND)pAppWin->window) && 
        (pEmbed->objTag.ele_attrmask & LO_ELE_INVISIBLE))
        ::ShowWindow((HWND)pAppWin->window, SW_HIDE);

    // If the plugin is embedded we need to make sure the window is
    // positioned properly
    if (!IsPluginFullPage(pEmbed)) {
        RECT	curRect;
            
        // Get the coordinates of where it should be positioned
        GetPluginRect(pContext, pEmbed, rect, iLocation, TRUE);

        // Get the current coordinates relative to the parent window's
        // client area
        ::GetWindowRect((HWND)pAppWin->window, &curRect);
        MapWindowPoints(NULL, ::GetParent((HWND)pAppWin->window),
                        (POINT FAR *)&curRect, 2);
                
        // Only move/size the window if necessary
        if ((rect.left != curRect.left) ||
            (rect.right != curRect.right) ||
            (rect.top != curRect.top) ||
            (rect.bottom != curRect.bottom)){
            ::SetWindowPos((HWND)pAppWin->window, NULL, rect.left, rect.top,
                           rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
                    
        }
         
        // Update the NPWindow rect
        pAppWin->x = rect.left;
        pAppWin->y = rect.top;
        pAppWin->width = rect.right - rect.left;
        pAppWin->height = rect.bottom - rect.top;
    }
        
    if (!::IsWindowVisible((HWND)pAppWin->window) &&
        !(pEmbed->objTag.ele_attrmask & LO_ELE_INVISIBLE))
        ::ShowWindow((HWND)pAppWin->window, SW_SHOW);
       
}

#ifndef MOZ_NGLAYOUT
void CDCCX::DisplayEmbed(MWContext *pContext, int iLocation, LO_EmbedStruct *pEmbed)
{
    NPEmbeddedApp* pEmbeddedApp = (NPEmbeddedApp*)pEmbed->objTag.FE_Data;

    if(pEmbeddedApp == NULL)
        return;

	if(wfe_IsTypePlugin(pEmbeddedApp)) {
        if (NPL_IsEmbedWindowed(pEmbeddedApp)) {
            DisplayPlugin(pContext, pEmbed, pEmbeddedApp, iLocation);
            (void)NPL_EmbedSize(pEmbeddedApp);
        }
        else
            DisplayWindowlessPlugin(pContext, pEmbed, pEmbeddedApp, iLocation);

        if(IsPluginFullPage(pEmbed))
            pEmbeddedApp->pagePluginType = NP_FullPage;
        else
            pEmbeddedApp->pagePluginType = NP_Embedded;
        return;
    }

	// else must be an OLE display
	LTRB Rect;
	if(ResolveElement(Rect, pEmbed, iLocation, FALSE) == TRUE)	{
        int32 lOrgX, lOrgY;
        
        GetDrawingOrigin(&lOrgX, &lOrgY);
        SafeSixteen(Rect);

		//	Draw a border around the embed if needed.
		if(pEmbed->objTag.border_width != 0)	{
			DrawRectBorder(Rect, ResolveBorderColor(NULL), pEmbed->objTag.border_width);
		}

		//	Get the embedded item
        CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;
		if(pItem == NULL)	{
			DisplayIcon(Rect.left, Rect.top, IL_IMAGE_BAD_DATA);
			return;
		}

		//	Handle delayed, broken, and currently loading items.
		else if(pItem->m_bDelayed == TRUE)	{
			DisplayIcon(Rect.left - lOrgX, Rect.top - lOrgY, IL_IMAGE_DELAYED);
			return;
		}
		else if(pItem->m_bBroken == TRUE)	{
			DisplayIcon(Rect.left - lOrgX, Rect.top - lOrgY, IL_IMAGE_BAD_DATA);
		}
		else if(pItem->m_bLoading == TRUE)	{
			//	Can't draw if still loading.
			return;			
		}

		//	Okay to draw.
		HDC hdc = GetContextDC();
		RECT crRect;
		::SetRect(&crRect, (int)Rect.left,(int)Rect.top,(int)Rect.right,(int)Rect.bottom);
		CDC dc;
		dc.Attach(hdc);
		COLORREF backgroundColor = (COLORREF)GetSysColor(COLOR_WINDOW);
		HBRUSH tempBrush = ::CreateSolidBrush(backgroundColor);
		::FillRect(dc.GetSafeHdc(), &crRect,tempBrush);
		::DeleteObject(tempBrush);
		if (pItem->IsInPlaceActive())  {
			if (!pItem->m_bSetExtents) {
				pItem->m_bSetExtents = TRUE;
				SIZEL temp1;
				temp1.cx = Twips2MetricX(crRect.right - crRect.left);
				temp1.cy = Twips2MetricY(crRect.bottom - crRect.top);
				HRESULT sc = pItem->m_lpObject->SetExtent(DVASPECT_CONTENT, &temp1);
				sc = pItem->m_lpObject->Update();
			}
			crRect.left += sysInfo.m_iScrollWidth + sysInfo.m_iBorderWidth;
			crRect.top += sysInfo.m_iScrollHeight + sysInfo.m_iBorderHeight;
			crRect.right -=  sysInfo.m_iScrollWidth + sysInfo.m_iBorderWidth;
			crRect.bottom -= sysInfo.m_iScrollHeight + sysInfo.m_iBorderHeight;
			pItem->SetItemRects(&crRect);
		}
		pItem->Draw(&dc, &crRect);
		dc.Detach();
		ReleaseContextDC(hdc);
	}
}
#endif /* MOZ_NGLAYOUT */

void CDCCX::DisplayHR(MWContext *pContext, int iLocation, LO_HorizRuleStruct *pHorizRule)	
{
	//	Figure out the coordinates of where to draw the HR.
	LTRB Rect;
//	if(ResolveElement(Rect, pHorizRule, iLocation) == TRUE)	{
	if(ResolveElement(Rect, pHorizRule->x,pHorizRule->y,
					pHorizRule->x_offset, pHorizRule->y_offset,
					pHorizRule->width, pHorizRule->height) == TRUE)	{
		SafeSixteen(Rect);

#ifdef EDITOR
        // Don't draw the editor's end-of-document hrule unless we're
        // displaying paragraph marks.
        if ( pHorizRule->edit_offset < 0 && ! EDT_DISPLAY_PARAGRAPH_MARKS(pContext) ) {
            return;
        }

        // Increase height of selection rect if it is very small
        LTRB SelRect = Rect;
        if( SelRect.Height() < 6 )
        {
            SelRect.top -= (6 - SelRect.Height())/2;
            SelRect.bottom = SelRect.top + 6;
        }
//        EraseToBackground(SelRect);
#endif /* EDITOR */

		//	Find out if we are doing a solid or 3D HR.
		if(ResolveHRSolid(pHorizRule) == TRUE)	{
			HDC hdc = GetContextDC();

			//	Draw a solid HR.
			//	We'll be using the darker color to do this.
			COLORREF rgbColor = ResolveDarkLineColor();

			HPEN cpHR = ::CreatePen(PS_SOLID, CASTINT(Pix2TwipsY(pHorizRule->thickness)), rgbColor);
			HPEN pOldPen = (HPEN)::SelectObject(hdc, cpHR);
			if(MMIsText() == TRUE)	{
				::MoveToEx(hdc, CASTINT(Rect.left), CASTINT(Rect.top + Rect.Height() / 2), NULL);
				::LineTo(hdc, CASTINT(Rect.right), CASTINT(Rect.top + Rect.Height() / 2));	//	use top, or diagonal.
			}
			else	{
				::MoveToEx(hdc, CASTINT(Rect.left), CASTINT(Rect.top), NULL);
				::LineTo(hdc, CASTINT(Rect.right), CASTINT(Rect.top));	//	use top, or diagonal.
			}
			::SelectObject(hdc, pOldPen);
			VERIFY(::DeleteObject(cpHR));
	
			ReleaseContextDC(hdc);
		}
		else	{
			Display3DBox(Rect, ResolveDarkLineColor(), ResolveLightLineColor(),
				Pix2TwipsX(1), TRUE);
		}
#ifdef EDITOR
        DisplaySelectionFeedback(pHorizRule->ele_attrmask, SelRect);
#endif //EDITOR
	}
}

// Common algorithm for setting contrasting background and text colors
void wfe_GetSelectionColors( COLORREF rgbBackgroundColor, 
                             COLORREF* pTextColor, COLORREF* pBackColor)
{
    //CLM: Always use Blue text on white or white text on blue if selecting
    // Test intensity of real background so entire selection uses same colors
    // This algorithm favors using dark
    // Possible problem: If we have relatively bright text on a dark background image
    BOOL bDarkBackground = (GetRValue(rgbBackgroundColor) + 
                            GetGValue(rgbBackgroundColor) + 
                            GetBValue(rgbBackgroundColor)) < 192;

    if( pTextColor ){
        *pTextColor = bDarkBackground ? RGB(0,0,128) : RGB(255,255,255);
    }
    if( pBackColor ){
        *pBackColor = bDarkBackground ?  RGB(255,255,255) : RGB(0,0,128);
    }
}

               
void CDCCX::DisplayLineFeed(MWContext *pContext, int iLocation, LO_LinefeedStruct *pLineFeed, XP_Bool clear)
{
    LTRB Rect;
    Bool bElementResolved;
    Bool bSpecialDrawing = EDT_IS_EDITOR(pContext) && pLineFeed->break_type;

#ifdef EDITOR
    // Allow selection feedback and display of end-of-paragraph marks in the editor.
    // We do nothing if we're not in the editor.
    // We do nothing if this isn't an end-of-paragraph or hard break.
    // If we're displaying the marks, then we display them as little squares.
    // Otherwise we just do the selection feedback.
    // The selection feedback is a rectangle the height of the linefeed 1/2 of the rectangle's height.
    // located at the left edge of the linefeed's range.

    // We need to expand the linefeed's width here so that ResolveElement thinks it's at least as wide
    // as we're going to draw it.
    Bool bExpanded = FALSE;
    int32 expandedWidth;
    int32 expandedHeight;
    if ( bSpecialDrawing ) {
        const int32 kMinimumWidth = 7;
        const int32 kMinimumHeight = 12;

        int32 originalWidth = pLineFeed->width;
        int32 originalHeight= pLineFeed->height;
        expandedWidth = originalWidth;
        expandedHeight = originalHeight;

        if ( expandedWidth < kMinimumWidth ) {
            expandedWidth = kMinimumWidth;
            bExpanded = TRUE;
        }
        if ( expandedHeight < kMinimumHeight ) {
            expandedHeight = kMinimumHeight;
            bExpanded = TRUE;
        }
        pLineFeed->width = expandedWidth;
        pLineFeed->height = expandedHeight;
//        bElementResolved = ResolveElement(Rect, pLineFeed, iLocation);
        bElementResolved = ResolveElement(Rect, pLineFeed->x, pLineFeed->y,
									pLineFeed->x_offset,pLineFeed->y_offset,
									pLineFeed->width, pLineFeed->height);
        pLineFeed->width = originalWidth;
        pLineFeed->height = originalHeight;
    }
    else
    {
//        bElementResolved = ResolveElement(Rect, pLineFeed, iLocation);
        bElementResolved = ResolveElement(Rect, pLineFeed->x, pLineFeed->y,
								pLineFeed->x_offset, pLineFeed->y_offset,
								pLineFeed->width, pLineFeed->height);
    }
#else
//    bElementResolved = ResolveElement(Rect, pLineFeed, iLocation);
        bElementResolved = ResolveElement(Rect, pLineFeed->x, pLineFeed->y,
								pLineFeed->x_offset, pLineFeed->y_offset,
								pLineFeed->width, pLineFeed->height);
#endif

    if (bElementResolved) {
		SafeSixteen(Rect);

        if ( ! EDT_IS_EDITOR(pContext) || ! pLineFeed->break_type ) {
            // If the linefeed has a solid background color, then use it.
            // To Be Done: If we're selected, use the fg color instead of the bg color.
#ifndef LAYERS
	  // For layers, we don't want to draw the background 
		    if (pLineFeed->text_attr->no_background == FALSE && !IsPrintContext()) {
			    HBRUSH	 hBrush;
			    CRect	 cRect(CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
			    HDC hdc = GetContextDC(iLocation);

			    if (m_iBitsPerPixel == 16)
				    // We don't want a dithered brush
					hBrush = ::CreateSolidBrush(::GetNearestColor(hdc, RGB(pLineFeed->text_attr->bg.red,
					    pLineFeed->text_attr->bg.green, pLineFeed->text_attr->bg.blue)));
			    else
        		    hBrush = ::CreateSolidBrush(0x02000000L | ResolveBGColor(pLineFeed->text_attr->bg.red,
        			    pLineFeed->text_attr->bg.green, pLineFeed->text_attr->bg.blue));
				HBRUSH hOldBrush;
				hOldBrush = ::SelectObject(hdc, hBrush);
        	    ::FillRect(hdc, &cRect, hBrush);
				::SelectObject(hdc, hOldBrush);
        	    VERIFY(::DeleteObject(hBrush));

			    ReleaseContextDC(hdc, iLocation);
		    }
#endif // LAYERS
            return;
        }

		HDC hdc = GetContextDC();
        RECT r;
		::SetRect(&r, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
        // Need to look up the text color.
        LO_TextAttr * attr = pLineFeed->text_attr;

		//	Determine the color.
		COLORREF rgbColor = ResolveTextColor(attr);
        COLORREF textBackColor;
        Bool selected = pLineFeed->ele_attrmask & LO_ELE_SELECTED;

        if ( selected ) {
            wfe_GetSelectionColors(m_rgbBackgroundColor, &rgbColor, &textBackColor);
        }

        if (clear & ! selected) {
#ifdef EDITOR
            // If the linefeed has a solid background color, then use it
		    if (pLineFeed->text_attr->no_background == FALSE && !IsPrintContext()) {
			    HBRUSH	 hBrush;
			    RECT	 cRect;
				::SetRect(&cRect, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));

			    if (m_iBitsPerPixel == 16)
				    // We don't want a dithered brush
					hBrush = ::CreateSolidBrush(::GetNearestColor(hdc, RGB(pLineFeed->text_attr->bg.red,
					    pLineFeed->text_attr->bg.green, pLineFeed->text_attr->bg.blue)));
			    else
        		    hBrush = ::CreateSolidBrush(0x02000000L | ResolveBGColor(pLineFeed->text_attr->bg.red,
        			    pLineFeed->text_attr->bg.green, pLineFeed->text_attr->bg.blue));
        	    ::FillRect(hdc, &cRect, hBrush);
        	    VERIFY(::DeleteObject(hBrush));
		    }
            else {
                EraseToBackground(Rect);
            }
            // If this linefeed draws outside it bounds, then
            // EraseToBackground could have
            // smashed a nearby element.
            // So invalidate this portion. (The main examples of this are
            // linefeeds in tables.)
            if ( bExpanded && IsWindowContext()) {
                ::InvalidateRect(PANECX(pContext)->GetPane(), &r, FALSE);
            }
#endif
        }

#ifdef EDITOR
        // Limit the size of the drawn paragraph marker.
        const int32 kMaxHeight = 30;
        if ( expandedHeight > kMaxHeight ){
            expandedHeight = kMaxHeight;
        }
        int32 desiredWidth = expandedHeight / 2;
        if ( expandedWidth > desiredWidth ) {
            expandedWidth = desiredWidth;
        }

        r.right = CASTINT(r.left + expandedWidth);
        r.bottom = CASTINT(r.top + expandedHeight);

#endif        
        if (selected) {
			HBRUSH cbBrush = ::CreateSolidBrush(textBackColor);
			::FillRect(hdc, &r, cbBrush);
			VERIFY(::DeleteObject(cbBrush));
        }
        if ( EDT_DISPLAY_PARAGRAPH_MARKS(pContext)
            && (! pLineFeed->prev // Don't draw mark after end-of-doc mark
                || pLineFeed->prev->lo_any.edit_offset >= 0 ) ) {
			HBRUSH cbBrush = ::CreateSolidBrush(rgbColor);
			if ( r.right - r.left > 5 ) {
			    r.left += 2;
                r.right -= 2;
            }
            if ( r.bottom - r.top > 5 ) {
                r.top += 2;
                r.bottom -= 2;
            }
            // draw line breaks at 1/3 height of paragraph marks
            if ( pLineFeed->break_type == LO_LINEFEED_BREAK_HARD ){
                r.top += (r.bottom - r.top) * 2 / 3;
            }
			::FillRect(hdc, &r, cbBrush);
			VERIFY(::DeleteObject(cbBrush));
        }
		ReleaseContextDC(hdc);
    }
}

void CDCCX::DisplaySubDoc(MWContext *pContext, int iLocation, LO_SubDocStruct *pSubDoc)	{
	LTRB Rect;
//	if(ResolveElement(Rect, pSubDoc, iLocation) == TRUE)	{
	if(ResolveElement(Rect, pSubDoc->x, pSubDoc->y, pSubDoc->x_offset,
						pSubDoc->y_offset, pSubDoc->width, pSubDoc->height) == TRUE)	{
		SafeSixteen(Rect);

		if(pSubDoc->border_width > 0)	{
			Display3DBox(Rect, ResolveLightLineColor(), ResolveDarkLineColor(), pSubDoc->border_width);
		}
	}
}

void CDCCX::DisplayCell(MWContext *pContext, int iLocation, LO_CellStruct *pCell)	{
	LTRB Rect;
	if(ResolveElement(Rect, pCell->x, pCell->y, pCell->x_offset,
						pCell->y_offset, pCell->width, pCell->height) == TRUE)	{
		SafeSixteen(Rect);

#ifndef LAYERS
		// With layers, the background color is done with a layer

		// If the cell has a background color, then use it
		if (pCell->bg_color && !IsPrintContext()) {
			HBRUSH	 hBrush;
			RECT	 cRect;
			::SetRect(&cRect, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
			HDC hdc = GetContextDC(iLocation);

			if (m_iBitsPerPixel == 16)
				// We don't want a dithered brush
				hBrush = ::CreateSolidBrush(::GetNearestColor(hdc, RGB(pCell->bg_color->red,
					pCell->bg_color->green, pCell->bg_color->blue)));
			else
        		hBrush = ::CreateSolidBrush(0x02000000L | ResolveBGColor(pCell->bg_color->red,
        			pCell->bg_color->green, pCell->bg_color->blue));

        	::FillRect(hdc, &cRect, hBrush);
        	VERIFY(::DeleteObject(hBrush));

			ReleaseContextDC(pDC, iLocation);
		}

		// If we decide to allow nested table cells to have the
		// background show through then we need this code
#endif /* LAYERS */

#ifdef EDITOR
        if( EDT_IS_EDITOR(pContext) )
        {
            if( pCell->border_width == 0 || LO_IsEmptyCell(pCell) )
            {
                EraseToBackground(Rect);
            }
            int32 iExtraSpace = 0;
            int32 iBorder = pCell->border_width;
            // Cell highlightin is thicker
            int32 iMaxWidth = 2 * ED_SELECTION_BORDER;
            BOOL bSelected = pCell->ele_attrmask & LO_ELE_SELECTED;
            BOOL bSelectedSpecial = pCell->ele_attrmask & LO_ELE_SELECTED_SPECIAL;
            COLORREF rgbBorder;
            HDC hDC = 0;

            if( bSelected || bSelectedSpecial )
            {
                // Use the selection background color to draw a solid border
                wfe_GetSelectionColors(m_rgbBackgroundColor, NULL, &rgbBorder);
		        hDC = GetContextDC();
			    RECT r;
			    ::SetRect(&r, CASTINT(Rect.left), CASTINT(Rect.top), CASTINT(Rect.right), CASTINT(Rect.bottom));
                if( iBorder > 0 )
                {                
                    // If there is inter-cell spacing, 
                    // draw selection in that region as much as possible
                    if( pCell->inter_cell_space > 0 && iBorder < iMaxWidth )
                    {
                        iExtraSpace = min(iMaxWidth - iBorder, pCell->inter_cell_space / 2);
                        iBorder += iExtraSpace;
                        ::InflateRect(&r, iExtraSpace, iExtraSpace);
                    }
	                LTRB borderWidths(iBorder, iBorder, iBorder, iBorder);

                    if( bSelectedSpecial )
                    {
	                    // Show a solid DASHED border as the special selection feedback
                        DisplaySpecialBorder(hDC, r, rgbBorder, iBorder, TRUE);
                    } else {
	                    // Show a solid border as the selection feedback
                        DisplaySolidBorder(hDC, r, rgbBorder, borderWidths);
                    }
                }
            }

            if( pCell->border_width == 0 || LO_IsEmptyCell(pCell) )
            {
    		    // Draw zero-border and empty cells with the dotted line
                //   (Navigator will not display borders of empty cells)
                // Don't bother testing (EDT_DISPLAY_TABLE_BORDERS)
                //  since we don't support turning borders off now
                EditorDisplayZeroWidthBorder(Rect, bSelected | bSelectedSpecial );

		    }
            else if( !bSelected )
            {
    			Display3DBox(Rect, ResolveDarkLineColor(), ResolveLightLineColor(), pCell->border_width);
            }

            // If directly-drawn selection border is narrower than minimum selection width (ED_SELECTION_BORDER)
            //  add inverse highlighting whose width is ED_SELECTION_BORDER
            // Thus total selection effect will always be between 
            //      ED_SELECTION_BORDER and (2*ED_SELECTION_BORDER) in thickness
            if( hDC && (bSelected || bSelectedSpecial) &&
                iBorder < ED_SELECTION_BORDER )
            {
                //  TODO: IMAGES THAT FILL CELL WILL OVERWRITE THIS SELECTION - Especially bad when iBorder = 0
                LTRB CellRect = Rect;
                // Draw inside the border rect
                CellRect.Inflate(-1);

                if( bSelectedSpecial )
                {
			        RECT r;
			        ::SetRect(&r, CASTINT(CellRect.left), CASTINT(CellRect.top), 
                              CASTINT(CellRect.right), CASTINT(CellRect.bottom));
	                
                    // Show an inverted DASHED border as the special selection feedback
                    DisplaySpecialBorder(hDC, r, rgbBorder, ED_SELECTION_BORDER, FALSE);
                } else {
                    DisplaySelectionFeedback(LO_ELE_SELECTED, CellRect);
                }
            }
            
            if( hDC )
                ReleaseContextDC(hDC);
        } 
        else
#endif // EDITOR
        // Normal border drawing for Navigator
        if( pCell->border_width > 0 ) {
			Display3DBox(Rect, ResolveDarkLineColor(), ResolveLightLineColor(), pCell->border_width);
		}
	}
}

VOID CALLBACK EXPORT LineDDAProcTabFocus(
    int x,	// x-coordinate of point being evaluated  
    int y,	// y-coordinate of point being evaluated  
    LPARAM lpData 	// address of application-defined data 
   )
{
	static BOOL flip = FALSE;
	// HDC hdc = (HDC) lpData;
	flip = ! flip;
#ifdef _WIN32
	SetPixelV((HDC)lpData, x, y, flip? COLORREF(RGB(0,0,0)) : COLORREF(RGB(255,255,255)) ); 
#else
	SetPixel((HDC)lpData, x, y, flip? COLORREF(RGB(0,0,0)) : COLORREF(RGB(255,255,255)) ); 
#endif
	return;
}	

void CDCCX::DrawTabFocusLine( HDC hdc, BOOL supportPixel, int x, int y, int x2, int y2)
{
	if( supportPixel ) {
		// support SetPixel()
		LineDDA( x, y, x2, y2, (LINEDDAPROC)LineDDAProcTabFocus, (LPARAM)hdc );
	} else {
		// SetPixel is not supported
#ifdef _WIN32
		::MoveToEx(hdc, x,  y, NULL);
#else
		::MoveTo(hdc, x,  y );
#endif
		::LineTo(hdc, x2, y2);
	}

}

// Monochrome brush with every other pixel set
struct CAlternateBrush {
	HBRUSH	hBrush;

	CAlternateBrush();
   ~CAlternateBrush();
} alternateBrush;

CAlternateBrush::CAlternateBrush()
{
	// Monochrome pattern brush we use for drawing horizontal and vertical alternate
	// lines (lines with every other pixel set)
    static const WORD gray50 [] = {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55};
	HBITMAP	hBitmap = ::CreateBitmap(8, 8, 1, 1, gray50);

	hBrush = ::CreatePatternBrush(hBitmap);
	::DeleteObject(hBitmap);
}

CAlternateBrush::~CAlternateBrush()
{
	::DeleteObject(hBrush);
}

void CDCCX::DrawTabFocusRect( HDC hdc, BOOL supportPixel, int left, int top, int right, int bottom)
{
	RECT	rect = {left, top, right, bottom};
	DWORD	dwOldTextColor, dwOldBkColor;

	ASSERT(alternateBrush.hBrush);

	dwOldTextColor = ::SetTextColor(hdc, RGB(0,0,0));
	dwOldBkColor = ::SetBkColor(hdc, RGB(255,255,255));

	::FrameRect(hdc, &rect, alternateBrush.hBrush);

	::SetTextColor(hdc, dwOldTextColor);
	::SetBkColor(hdc, dwOldBkColor);
}

//#ifndef NO_TAB_NAVIGATION 
// see function lo_is_location_in_area() in file ns\lib\layout\laymap.c for area types.
void CDCCX::DrawMapAreaBorder( int baseX, int baseY, lo_MapAreaRec * theArea )
{
	int 	centerX, centerY, radius;
	int		left, top, right, bottom;
	int		oldDrawingMode;
	int		ii;
	HDC		hdc = GetContextDC();
	HPEN	pTabFocusPen;
	HPEN	pOldPen;
	HBRUSH	hOldBrush;
	BOOL	supportPixel = ::GetDeviceCaps(  hdc, RASTERCAPS ) & RC_BITBLT ;

	if( ! supportPixel || theArea->type == AREA_SHAPE_CIRCLE ) {
		// todo get color for hi-light
		oldDrawingMode	= SetROP2( hdc, R2_NOT);		// R2_NOT reverse the screen color
		pTabFocusPen	= ::CreatePen(PS_DOT, 1, COLORREF( RGB(0,0,0)) );
		pOldPen			= (HPEN)::SelectObject(hdc, pTabFocusPen);
		hOldBrush		= (HBRUSH) ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
	}
	
	switch( theArea->type )
	{
		case AREA_SHAPE_RECT:
			if (theArea->coord_cnt >= 4)
			{
				left	= (int)(baseX + theArea->coords[0]);		// Pix2TwipsY()?
				top		= (int)(baseY + theArea->coords[1]);
				right	= (int)(baseX + theArea->coords[2]);
				bottom	= (int)(baseY + theArea->coords[3]);
			    DrawTabFocusRect(hdc, supportPixel, left, top, right, bottom);
				if( supportPixel )
					DrawTabFocusRect(hdc, supportPixel, left+1, top+1, right-1, bottom-1);  // double line
			}
			break;

		case AREA_SHAPE_CIRCLE:
			if ( theArea->coord_cnt >= 3)
			{
				centerX = (int)theArea->coords[0] + baseX;
				centerY = (int)theArea->coords[1] + baseY;
				radius	= (int)theArea->coords[2];
				
				left	= centerX - radius ;
				top		= centerY - radius ;
				right	= centerX + radius ;
				bottom	= centerY + radius ;
				//todo the NULL_BRUSH may cause problems on some platforms(win3.1, win95).
				::Ellipse(hdc, left, top, right, bottom);
				::Ellipse(hdc, left+1, top+1, right-1, bottom-1);  // double line
			}
			break;

		case AREA_SHAPE_POLY:
			if (theArea->coord_cnt >= 6)
			{
				for( ii=0; ii<= theArea->coord_cnt-4; ii+=2) {
					DrawTabFocusLine( hdc, supportPixel, 
						(int)theArea->coords[ii]   + baseX, (int)theArea->coords[ii+1] + baseY,
						(int)theArea->coords[ii+2] + baseX, (int)theArea->coords[ii+3] + baseY);
				}
				// make it close
				// work around: for 6 edges, theArea->coord_cnt is 13 !! 
				// Cannot use theArea->coord_cnt-2 for last x.
				// use ii, which stoped at the last point.
				DrawTabFocusLine( hdc, supportPixel, 
					(int)theArea->coords[ii]   + baseX, (int)theArea->coords[ii+1] + baseY,
						(int)theArea->coords[0] + baseX, (int)theArea->coords[1] + baseY);

			}
			break;

		case AREA_SHAPE_DEFAULT:
			// TODO ??????
			break;

		case AREA_SHAPE_UNKNOWN:
		default:
			break;
	}

	if( ! supportPixel || theArea->type == AREA_SHAPE_CIRCLE ) {
		::SelectObject(hdc, hOldBrush);
		::SelectObject(hdc, pOldPen);
		VERIFY(::DeleteObject(pTabFocusPen));
		::SetROP2( hdc, oldDrawingMode );
	}

	ReleaseContextDC(hdc);

}
//#endif	/* NO_TAB_NAVIGATION */



void CDCCX::DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool iClear)    
{
	//	Figure the coordinates of where to draw the text.
	LTRB Rect;
	if(ResolveElement(Rect, pText, iLocation, lStartPos, lEndPos, iClear) == TRUE)	{
		SafeSixteen(Rect);

		HDC hdc = GetContextDC();

        // cache the attribute pointer
        LO_TextAttr * attr = pText->text_attr;

        BOOL display_background_color = !attr->no_background;

		//	Determine the color.
		COLORREF rgbColor = ResolveTextColor(attr);
		
		//	If the color is the same as the background color, then we are selecting.
		//	Go into opaque mode.
		COLORREF rgbOldBackgroundColor;

        // what color does layout think our background color is?
        COLORREF textBackColor = RGB(attr->bg.red, attr->bg.green, attr->bg.blue);

        //IMPORTANT! Note that we will get called to draw a sub-part of a text line
        //   with LO_ELE_SELECTED flag set and the selected start-end is 
        //   outside of range to be drawn, thus we are really drawing the UNSELECTED
        //   portion of the text. So be sure to test that drawing and selectd start positions match

        // [ = lStartPos
        // ] = lEndPos
        // ( = sel_start
        // ) = sel_end
        // selection calls one of these two:
        //     subtext           [ (    ) ]
        //     subtext [        ]  (    )
        // drawtext calls subtext three times: [       ] [(     )] [      ]

        BOOL bSelected = (pText->ele_attrmask & LO_ELE_SELECTED) && 
                         (lStartPos >= (int32)pText->sel_start) &&
                         ((int32)pText->sel_end >= lEndPos) ;

#if 0
        XP_TRACE(("DisplaySubtext args: lStartPos(%d) lEndPos(%d)"
            " iClear(%d) element: selected(%d) sel_start(%d) sel_end(%d) bSelected(%d)\n",
            lStartPos, lEndPos,
            iClear,
            (pText->ele_attrmask & LO_ELE_SELECTED),
            (int32)pText->sel_start,  (int32)pText->sel_end, bSelected
            ));
#endif
		if (bSelected) {
            // Note that we do NOT have the "real" text color if selected,
            //  making it impossible to preview new colors in Character Properties dialog
            wfe_GetSelectionColors(m_rgbBackgroundColor, &rgbColor, &textBackColor);
            display_background_color = TRUE;
        }

        if(display_background_color)
        {
            rgbOldBackgroundColor = ::SetBkColor(hdc, textBackColor);
            ::SetBkMode(hdc, OPAQUE);
        }

		//	Select the font.
		CyaFont	*pMyFont;
		SelectNetscapeFontWithCache( hdc, attr, pMyFont);

		//	Set the text color.
		COLORREF rgbOldColor = ::SetTextColor(hdc, rgbColor);

        // Originally, we drew just the substring of characters
        // that we were asked to. But this did not handle kerning
        // correctly, since kerned characters could intrude into the
        // area of the substring. Worse, when selecting, the reverse-
        // video characters were drawn outside their boundaries, which
        // caused thin vertical strips of selection to remain when the
        // selection was erased.
        //
        // Now we draw the whole string, clipped to the bounds of the
        // substring. -- jhp

        LTRB fullRect;
        ResolveElement(fullRect, pText->x, pText->y, pText->x_offset,
						pText->y_offset, pText->width, pText->height);
		SafeSixteen(fullRect);
		//	Output

        int iOldDC = ::SaveDC(hdc);
        ::IntersectClipRect(hdc, (int)Rect.left, (int)Rect.top, (int)Rect.right, (int)Rect.bottom);
		CIntlWin::TextOutWithCyaFont(
			pMyFont,
			INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(pContext)),
			hdc, 
			CASTINT(fullRect.left),
			CASTINT(fullRect.top),
			(const char *)pText->text,
		    CASTINT(pText->text_len));

        ::RestoreDC(hdc, iOldDC);

		::SetTextColor(hdc, rgbOldColor);


		//	Handle strike, under_line, Spell and INLINEINPUT manually
        DrawTextPostDecoration(hdc, attr, &Rect, rgbColor );

		//	Normalize our selection if present.
		if (display_background_color) {
			::SetBkColor(hdc, rgbOldBackgroundColor);
			::SetBkMode(hdc, TRANSPARENT);
		}

		ReleaseNetscapeFontWithCache( hdc, pMyFont );
	
		ReleaseContextDC(hdc);
	}
}	// void CDCCX::DisplaySubtext()

#define TABLE_HAS_BORDER(pTable) \
	((pTable)->border_top_width > 0 || (pTable)->border_right_width > 0 || \
	 (pTable)->border_bottom_width > 0 || (pTable)->border_left_width > 0)

void CDCCX::DisplayTable(MWContext *pContext, int iLocation, LO_TableStruct *pTable)	{
	LTRB Rect;
    int32 iSelectionBorder = 0;
    BOOL bHaveBorder = TABLE_HAS_BORDER(pTable);

	if(ResolveElement(Rect, pTable->x, pTable->y, pTable->x_offset,
						pTable->y_offset, pTable->width, pTable->height) == TRUE)	{
		SafeSixteen(Rect);
        LTRB TableRect = Rect;
 
        //TRACE0("DisplayTable\n");

		if( bHaveBorder ) {
            iSelectionBorder = DisplayTableBorder(Rect, pTable);
		} 
#ifdef EDITOR
        else if ( EDT_DISPLAY_TABLE_BORDERS(pContext) )
        {
            if( 0 == pTable->inter_cell_space )
            {
                // When no cell spacing, Table border is on top of cell borders,
                //   so increase by 1 pixel so we always have a distinquishable table border
                TableRect.Inflate(1);
                iSelectionBorder = 1;
            }
            EditorDisplayZeroWidthBorder(TableRect, pTable->ele_attrmask & LO_ELE_SELECTED);
        }

        // Show extra selection if border was not wide enough for clear selection feedback
        if( EDT_IS_EDITOR(pContext) && (pTable->ele_attrmask & LO_ELE_SELECTED) &&
            iSelectionBorder < ED_SELECTION_BORDER )
        {
            // If directly-drawn selection border is too narrow (or none),
            //  add Inverse-Video highlighting to the maximum thickness allowed
            // Decrease size by amount of solid border used,
            if( iSelectionBorder )
                TableRect.Inflate(-(iSelectionBorder));

            DisplaySelectionFeedback(LO_ELE_SELECTED, TableRect);
        }
#endif //EDITOR
	}
}

// handle strike , under_line, Spell and INLINEINPUT after text is drawn.
void CDCCX::DrawTextPostDecoration( HDC hdc, LO_TextAttr * attr, LTRB *pRect, COLORREF rgbColor )
{
#ifdef font_do_it  // font handles underline, strikeOut 
		//	Handle strike manually
		if(attr->attrmask & LO_ATTR_STRIKEOUT)	{
			HPEN cpStrike = ::CreatePen(PS_SOLID, CASTINT(pRect->Height() / 10), rgbColor);
			HPEN pOldPen = (HPEN)::SelectObject(hdc, cpStrike);
			::MoveToEx(hdc, CASTINT(pRect->left), CASTINT(pRect->top + pRect->Height() / 2), NULL);
			::LineTo(hdc, CASTINT(pRect->right), CASTINT(pRect->top + pRect->Height() / 2));
			::SelectObject(hdc, pOldPen);
			VERIFY(::DeleteObject(cpStrike));
		}

		// because netscape font module doesn't support under_list, it is handled manually
		BOOL bUnderline = FALSE;
		if(attr->attrmask & LO_ATTR_ANCHOR) { 
			/*
			 * if (prefInfo.m_bUnderlineAnchors)
			 *   bUnderline = TRUE;
			 */
		}
		if(attr->attrmask & LO_ATTR_UNDERLINE)	{
			bUnderline = TRUE;
		}
		if( bUnderline ) {
			HPEN cpStrike = ::CreatePen(PS_SOLID, CASTINT(pRect->Height() / 10), rgbColor);
			HPEN pOldPen = (HPEN)::SelectObject(hdc, cpStrike);
			::MoveToEx(hdc, CASTINT(pRect->left), CASTINT(pRect->top + pRect->Height()-1 ), NULL);
			::LineTo(hdc, CASTINT(pRect->right), CASTINT(pRect->top + pRect->Height()-1 ));
			::SelectObject(hdc, pOldPen);
			VERIFY(::DeleteObject(cpStrike));
		}
#endif  // font handles underline, strikeOut 
		//	Handle Spell and INLINEINPUT  manually
		if(attr->attrmask & (LO_ATTR_SPELL | LO_ATTR_INLINEINPUT)  )	{
			HPEN cpStrike = ::CreatePen(PS_DOT, CASTINT(pRect->Height() / 10), 
			            RGB(attr->attrmask & LO_ATTR_SPELL ? 255 : 0, 
			                0,
			                attr->attrmask & LO_ATTR_SPELL ? 255 : 0 ) );
			HPEN pOldPen = (HPEN)::SelectObject(hdc, cpStrike);
			::MoveToEx(hdc, CASTINT(pRect->left), CASTINT(pRect->top + pRect->Height()-1 ), NULL);
			::LineTo(hdc, CASTINT(pRect->right), CASTINT(pRect->top + pRect->Height()-1 ));
			::SelectObject(hdc, pOldPen);
			VERIFY(::DeleteObject(cpStrike));
		}

} // DrawTextPostDecoration()

//TODO add uint32 drawFalg to the interface, check all caller to pass in the drawFlag, 
// C++ default mechanism doesn't work for calling from C.
void CDCCX::DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool iClear)	
{
		DisplayText(pContext, iLocation, pText, iClear, 0);			// use default if calling grom C.
}

/* cannot use default drawFlag = 0, if this function is called from C */
// an old interface  gateway for caller in C.
void CDCCX::DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool iClear, uint32 drawFlag )	
{
	//	Figure out the coordinates of where to draw the text.
	LTRB Rect;
	if(ResolveElement(Rect, pText->x, pText->y, pText->x_offset,
						pText->y_offset, pText->width, pText->height) == TRUE)	{
		SafeSixteen(Rect);

        // cache the attribute pointer
        LO_TextAttr * attr = pText->text_attr;
	
		if (attr)	// dmb 11/21/96 - w/o this line, we crash on blinking things like the phonebook.
		{
            BOOL display_background_color = !attr->no_background;
			HDC hdc;
			hdc = GetContextDC();

			//	Determine the color.
			COLORREF rgbColor = ResolveTextColor(attr);

			// what does layout think the background color is?
			COLORREF textBackColor = RGB(attr->bg.red, attr->bg.green, attr->bg.blue);

			// If the text is the same color as the background we are selecting text
			//   and need to go into OPAQUE mode so the text will show up on the screen
			COLORREF rgbOldBackgroundColor;
			if (pText->ele_attrmask & LO_ELE_SELECTED) {
                display_background_color = TRUE;
				wfe_GetSelectionColors(m_rgbBackgroundColor, &rgbColor, &textBackColor);
			}

            if(display_background_color)
            {
                rgbOldBackgroundColor = ::SetBkColor(hdc, textBackColor);
			    ::SetBkMode(hdc, OPAQUE);
            }
			else {
			    ::SetBkMode(hdc, TRANSPARENT);
			}

			//	Select the font.
			// CNetscapeFont *pFont;
			// HFONT pOldFont = SelectFont(hdc, attr, pFont);

			//	Set the text color.
			COLORREF rgbOldColor = ::SetTextColor(hdc, rgbColor);

//	#ifndef NO_TAB_NAVIGATION
			// draw a focus box if pText is current  TabFocus
			if( drawFlag & FE_DRAW_TAB_FOCUS ) {
				HPEN pTabFocusPen;
				HPEN pOldPen;
				BOOL	supportPixel = ::GetDeviceCaps(  hdc, RASTERCAPS ) & RC_BITBLT ;
				if( ! supportPixel ) {
					pTabFocusPen = ::CreatePen(PS_DOT, 1, rgbColor);
					pOldPen = (HPEN)::SelectObject(hdc, pTabFocusPen);
				}
				
				DrawTabFocusRect(hdc, supportPixel, CASTINT(Rect.left), CASTINT(Rect.top),
					                            CASTINT(Rect.right), CASTINT(Rect.bottom));
				
				if( ! supportPixel ) {
					::SelectObject(hdc, pOldPen);
					VERIFY(::DeleteObject(pTabFocusPen));
				}
			}
//	#endif	/* NO_TAB_NAVIGATION */

			CyaFont			*pMyFont;
			SelectNetscapeFontWithCache( hdc, attr, pMyFont);
		
#ifdef DEBUG_aliu
			ASSERT( pMyFont );
#endif

			if( pMyFont ) {
				CIntlWin::TextOutWithCyaFont(
					pMyFont,
					INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(pContext)),
					hdc, 
					CASTINT(Rect.left),	
					CASTINT(Rect.top),
					(char *)pText->text,
					CASTINT(pText->text_len) 
				);

			}
			::SetTextColor(hdc, rgbOldColor);
			
		//	Handle strike, under_line, Spell and INLINEINPUT manually
            DrawTextPostDecoration(hdc, attr, &Rect, rgbColor );

#if 0
moved into DrawTextPostDecoration()
			//	Handle Spell and INLINEINPUT  manually
			if(attr->attrmask & (LO_ATTR_SPELL | LO_ATTR_INLINEINPUT)  )	{
				HPEN cpStrike = ::CreatePen(PS_DOT, CASTINT(Rect.Height() / 10), 
							RGB(attr->attrmask & LO_ATTR_SPELL ? 255 : 0, 
								0,
								attr->attrmask & LO_ATTR_INLINEINPUT ? 255 : 0 ) );
				HPEN pOldPen = (HPEN)::SelectObject(hdc, cpStrike);
				::MoveToEx(hdc, CASTINT(Rect.left), CASTINT(Rect.top + Rect.Height()-1 ), NULL);
				::LineTo(hdc, CASTINT(Rect.right), CASTINT(Rect.top + Rect.Height()-1 ));
				::SelectObject(hdc, pOldPen);
				VERIFY(::DeleteObject(cpStrike));
			}
#endif

			//	Normalize our selection if present.
			if (display_background_color) {
				::SetBkColor(hdc, rgbOldBackgroundColor);
				::SetBkMode(hdc, TRANSPARENT);
			}

			// ReleaseFont(hdc, pOldFont);
			ReleaseNetscapeFontWithCache(hdc, pMyFont);

			ReleaseContextDC(hdc);
		}
	}
}	// void CDCCX::DisplayText()

#ifndef MOZ_NGLAYOUT
void CDCCX::FreeEmbedElement(MWContext *pContext, LO_EmbedStruct *pEmbed)	{
	//	We have our OLE document handle this.
	GetDocument()->FreeEmbedElement(pContext, pEmbed);
}

void CDCCX::GetEmbedSize(MWContext *pContext, LO_EmbedStruct *pEmbed, NET_ReloadMethod bReload)	
{
	//	We have our OLE document handle this.
	GetDocument()->GetEmbedSize(pContext, pEmbed, bReload);
}
#endif /* MOZ_NGLAYOUT */

#ifdef LAYERS
void CDCCX::GetTextFrame(MWContext *pContext, LO_TextStruct *pText,
			 int32 lStartPos, int32 lEndPos, XP_Rect *frame)
{
    frame->left = pText->x + pText->x_offset;
    frame->top = pText->y + pText->y_offset;

    HDC pDC = GetContextDC();
		
    CyaFont	*pMyFont;
    SelectNetscapeFontWithCache( pDC, pText->text_attr, pMyFont );
    CSize sz;
    ResolveTextExtent(pDC, (const char *)pText->text, CASTINT(lStartPos), &sz, pMyFont);
    frame->left += sz.cx;
    ResolveTextExtent(pDC, (const char *)pText->text + lStartPos, CASTINT(lEndPos - lStartPos + 1), &sz, pMyFont);
    frame->right = frame->left + sz.cx;	
		
    frame->bottom = frame->top + sz.cy;   

    ReleaseNetscapeFontWithCache( pDC, pMyFont );

    ReleaseContextDC(pDC);
}
#endif  /* LAYERS */

int CDCCX::GetTextInfo(MWContext *pContext, LO_TextStruct *pText, LO_TextInfo *pTextInfo)	{
#ifdef MOZ_NGLAYOUT
  XP_ASSERT(0);
  return FALSE;
#else
	HDC hdc = GetAttribDC();

	//	Determine and select the font.
	CyaFont	*pMyFont;
	SelectNetscapeFontWithCache( hdc, pText->text_attr, pMyFont );

	//	Get the information regarding the size of the font for layout.
	SIZE sExtent;
	ResolveTextExtent(
		INTL_GetCSIWinCSID(LO_GetDocumentCharacterSetInfo(pContext))
		, hdc, (const char *)pText->text, pText->text_len, &sExtent, pMyFont);

	pTextInfo->max_width = sExtent.cx;

	pTextInfo->ascent = pMyFont->GetAscent();
	pTextInfo->descent = pMyFont->GetDescent();
	pTextInfo->lbearing = pTextInfo->rbearing = 0;

	ReleaseNetscapeFontWithCache( hdc, pMyFont );
	ReleaseContextDC(hdc);
	return(TRUE);
#endif /* MOZ_NGLAYOUT */
}

BOOL CDCCX::ResolveTextExtent(int16 wincsid, HDC pDC, LPCTSTR pString, int iLength, LPSIZE pSize, CyaFont *pMyFont)	
{
	// Always measure it because the bytes * mean width algorithm 
	// won't work for many code point. Expecially UTF8 which 3 byte 
	// could be one character which takes 1 or 2 columns.
	return CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, wincsid, pDC, pString, iLength, pSize);
#if 0
	BOOL bRetval;

	// if(pMyFont->GetMeanWidth() == 0 )	{
	if( ! pMyFont->IsFixedFont() ) {
		bRetval = CIntlWin::GetTextExtentPointWithCyaFont(pMyFont, wincsid, pDC, pString, iLength, pSize);
		return(bRetval);
	}
	else	{
		//	fixed size.
		pSize->cx = (int) ((long)pMyFont->GetMeanWidth() * (long)iLength);
		pSize->cy = pMyFont->GetHeight();
		bRetval = TRUE;
	}
	return(bRetval);
#endif
}

#ifdef TRANSPARENT_APPLET
// 
// This function is called only when applet are windowless and force a
// paint on the java side
//
void CDCCX::DrawJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *pJava)
{
#if defined(OJI) || defined(JAVA)

    RECT        rect;
    HDC         hDC = GetContextDC();
    NPEvent     event;
    FE_Region   clip;
    
#ifdef LAYERS
    clip = GetDrawingClip();
    if (clip) 
    {
        XP_Rect xprect;
        
        FE_GetRegionBoundingBox(clip, &xprect);
        rect.left = CASTINT(xprect.left);
        rect.top = CASTINT(xprect.top);
        rect.right = CASTINT(xprect.right);
        rect.bottom = CASTINT(xprect.bottom);
    }
#endif /* LAYERS */        
    
    event.event = WM_PAINT;
    event.wParam = (uint32)hDC;
    event.lParam = (uint32)&rect;

#if defined(OJI)
    // XXX help
#elif defined(JAVA) 
    LJ_HandleEvent(pContext, pJava, (void *)&event);
#endif

    ReleaseContextDC(hDC);
#endif
}

#endif

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

#ifndef __Context_Device_Context_H
//	Avoid include redundancy
//
#define __Context_Device_Context_H

//	Purpose:	Implement a Context which holds the common code for
//					contexts which draw information.
//	Comments:	Use for those contexts with actual windows to display,
//					or for printing.  Basically those with a CDC on
//					which to display and which handle output formats
//					that go through layout or other engines.
//	Revision History:
//		05-26-95	created GAB
//

//	Required Includes
//
#include "cxstubs.h"
#include "ni_pixmp.h"
#include "prtypes.h"
#include "il_types.h"
#include "il_util.h"
#include "libimg.h"
#ifdef XP_WIN32
//#define DDRAW
#endif
#ifdef DDRAW
#include "ddraw.h"
#endif

//	Constants
//
#define HIMETRIC_INCH 2540    // HIMETRIC units per inch
#define TWIPS_INCH 1440
#define MAX_IMAGE_PALETTE_ENTRIES 216
// DIBSECTIONS are pretty slow on most platforms except NT3.51
// We can use them if they're sped up in a later version of Windows
#define USE_DIB_SECTIONS 0

// Use for selection highlighting of objects in Editor
#define ED_SELECTION_BORDER 3

//#ifndef NO_TAB_NAVIGATION 
typedef enum {
	FE_DRAW_NORMAL		= 0x00000000,
	FE_DRAW_TAB_FOCUS	= 0x00000001,
} feDrawFlag_t ;
//#endif /* NO_TAB_NAVIGATION */


void WFE_StretchDIBitsWithMask(HDC hTargetDC,
							BOOL isDeviceDC,
							HDC hOffscreenDC,
							int dx,
							int dy,
							int dw,
							int dh,
							int sx,
							int sy,
							int sw,
							int sh,
							void XP_HUGE *imageBit,
							BITMAPINFO* lpImageInfo,
							void XP_HUGE *maskBit,
							BOOL bUseDibPalColors = FALSE,
							COLORREF fillWithBackground = NULL
							);


//	Structures
//
class LTRB	{
public:
	union	{
		int32 m_lLeft;
		int32 left;
	};
	union	{
		int32 m_lTop;
		int32 top;
	};
	union	{
		int32 m_lRight;
		int32 right;
	};
	union	{
		int32 m_lBottom;
		int32 bottom;
	};

	LTRB( LTRB& theRect)	{
		m_lLeft = theRect.m_lLeft;
		m_lTop = theRect.m_lTop;
		m_lRight = theRect.m_lRight;
		m_lBottom = theRect.m_lBottom;
	}

	LTRB& operator = (LTRB& theRect) {
		m_lLeft = theRect.m_lLeft;
		m_lTop = theRect.m_lTop;
		m_lRight = theRect.m_lRight;
		m_lBottom = theRect.m_lBottom;
		return *this;
	}

	void MergeRect(LTRB& theRect) {
		if ( m_lLeft >theRect.m_lLeft) 
			m_lLeft = theRect.m_lLeft;
		if (m_lTop > theRect.m_lTop)
			m_lTop = theRect.m_lTop;
		if (m_lRight < theRect.m_lRight)
			m_lRight = theRect.m_lRight;
		if (m_lBottom < theRect.m_lBottom)
			m_lBottom = theRect.m_lBottom;
	}

	void Empty() {
		m_lLeft = 0;
		m_lTop = 0;
		m_lRight = 0;
		m_lBottom = 0;
	}

	BOOL IsEmpty() {
		return(	m_lLeft == 0 &&
				m_lTop == 0 &&
				m_lRight == 0 &&
				m_lBottom == 0);
	}
	LTRB(int32 lLeft, int32 lTop, int32 lRight, int32 lBottom)	{
		m_lLeft = lLeft;
		m_lTop = lTop;
		m_lRight = lRight;
		m_lBottom = lBottom;
	}

	LTRB()	{
		m_lLeft = m_lTop = m_lRight = m_lBottom = 0L;
	}

	int32 Width()	{
		return(m_lRight - m_lLeft);
	}

	int32 Height()	{
		return(m_lBottom - m_lTop);
	}

	
    void Inflate(int32 lDelta)  {
        m_lLeft -= lDelta;
        m_lTop -= lDelta;
        m_lRight += lDelta;
        m_lBottom += lDelta;
    }
    void Inflate(int32 lDeltaX, int32 lDeltaY)  {
        m_lLeft -= lDeltaX;
        m_lTop -= lDeltaY;
        m_lRight += lDeltaX;
        m_lBottom += lDeltaY;
    }
};

class XY	{
public:
	union	{
		int32 m_lX;
		int32 x;
	};
	union	{
		int32 m_lY;
		int32 y;
	};

	XY(int32 lX, int32 lY)	{
		m_lX = lX;
		m_lY = lY;
	}

	XY()	{
		m_lX = m_lY = 0L;
	}
};

class CDCCX : public CStubsCX	{
//	Construction/Destruction
public:
	CDCCX();
	~CDCCX();
	virtual void DestroyContext();

//	CDC Access
private:
    BOOL m_bOwnDC;
    BOOL m_bClassDC;
protected:
    void SetOwnDC(BOOL bSet) { m_bOwnDC = bSet; }
    void SetClassDC(BOOL bSet) { m_bClassDC = bSet; }
protected:
	HDC m_pImageDC;
	IL_IRGB rgbTransparentColor;
	IL_ColorSpace* curColorSpace;
	IL_ColorMap *curColorMap;

public:
    virtual HDC GetContextDC() = 0;
    virtual void ReleaseContextDC(HDC hDC) = 0;
    virtual HDC GetAttribDC() { return GetContextDC(); }
    virtual void ReleaseAttribDC(HDC hDC) { ReleaseContextDC(hDC); }
	virtual HDC GetDispDC() { return GetContextDC(); }
    virtual void ReleaseDispDC(HDC hDC) { ReleaseContextDC(hDC); }
	
	virtual BOOL IsDeviceDC() { return TRUE; }
	virtual int32  GetXConvertUnit() { return m_lConvertX;}
	virtual int32  GetYConvertUnit() { return m_lConvertY;}
	IL_ColorSpace* GetCurrentColorSpace() { return curColorSpace;}
	virtual BOOL IsOwnDC() const	{
		return(m_bOwnDC);
	}
    virtual BOOL IsClassDC() const  {
        return(m_bClassDC);
    }
	HDC GetImageDC()	{
		//	Use this, a compatible DC, to size things to the display or for bitmaps
		//		to blt on the screen.
		//	Don't use to actually attempt display of anything.
		return(m_pImageDC);
	}

//	Post construction initialization.
//	Variables should be set by those deriving from this class at
//		the appropriate times.
public:
	// MWH - The usage for  bInitialPalette and 	bNewMemDC flags -
	// if  bInitialPalette is set to FALSE, we will use the default palette that is created when the App started.
	// otherwise a palette will be created.
	// if bNewMemDC = TRUE, the the cache memory DC(m_pImageDC) will be created.  Otherwise, it will be 
	// NULL.  If you set bNewMemDC = FALSE, you should call PrepareDraw() before any drawing happen, and
	// call	EndDraw() when you are done with this CDCCX.  When I'm working on Aurora, I find out that
	// I need this flag.   In Aurora, every button has an associate view, it gets created when the button
	// is created.  If the view has HTML pane, it will create a CDCCX.  I need to control when to cache
	// memory DC and when to destroy memory DC in order to use GDI resource efficiently under WIN16.
	virtual void Initialize(BOOL bOwnDC, RECT *pRect = NULL, BOOL bInitialPalette = TRUE, BOOL bNewMemDC = TRUE);
	virtual void PrepareDraw();
	virtual void EndDraw();
	int32 m_lOrgX;
	int32 m_lOrgY;
	int32 m_lWidth;
	int32 m_lHeight;
	int32 m_lDocHeight;
	int32 m_lDocWidth;
#ifdef DDRAW
	virtual LPDDSURFACEDESC	GetSurfDesc() {return 0;}
	virtual LPDIRECTDRAWSURFACE GetPrimarySurface() {return 0;}
	virtual LPDIRECTDRAW GetDrawObj() {return 0;}
	virtual LPDIRECTDRAWSURFACE GetBackSurface(){return 0;}
	virtual void ReleaseSurfDC(LPDIRECTDRAWSURFACE surf, HDC hdc){;}
	virtual void RestoreAllDrawSurface() {;}
	virtual void SetClipOnDrawSurface(LPDIRECTDRAWSURFACE surface, HRGN hClipRgn) {;}
	virtual LPDIRECTDRAWSURFACE CreateOffscreenSurface(RECT& rect) {return NULL;}
	virtual void BltToScreen(LTRB& rect) {;}
	virtual void LockOffscreenSurfDC(){;}
	virtual void ReleaseOffscreenSurfDC() {;}
#endif
//	Drawing conversions, informational.
protected:
	//	Hacks for speedup.
	int32 m_lConvertX;
	int32 m_lConvertY;
	//	What mapping mode we're in.
	int m_MM;
	HRGN curClipRgn;
public:
	virtual int GetLeftMargin() {return 0;}
	virtual int GetTopMargin() {return 0;}
	//	How to set the mapping mode.
	void SetMappingMode(HDC hdc)	{
		if(MMIsText() == TRUE)	{
			::SetMapMode(hdc, MM_TEXT);
		}
		else	{
			::SetMapMode(hdc, MM_ANISOTROPIC);
			::SetWindowExtEx(hdc, 1440,1440, NULL);
			::SetViewportExtEx(hdc, ::GetDeviceCaps(hdc, LOGPIXELSX), ::GetDeviceCaps(hdc, LOGPIXELSY), NULL);
		}
	}
	//	Can opt for 1 to 1 conversions.
	BOOL MMIsText() const	{
		return(m_MM == MM_TEXT);
	}
	int32 Pix2TwipsX(int32 lPixX)	{
		return(GetXConvertUnit() * lPixX);
	}
	int32 Twips2PixX(int32 lTwipsX)	{
		return(lTwipsX / m_lConvertX);
	}
	int32 Pix2TwipsY(int32 lPixY)	{
		return(GetYConvertUnit() * lPixY);
	}
	int32 Twips2PixY(int32 lTwipsY)	{
		return(lTwipsY / m_lConvertY);
	}
	int32 Metric2TwipsX(int32 lMetricX)	{
		//	We use a screen DC to get WYSIWYG
		CDC *pTempDC = theApp.m_pMainWnd->GetDC();
		float convertPixX = ((float)HIMETRIC_INCH / (float)pTempDC->GetDeviceCaps(LOGPIXELSX));
		theApp.m_pMainWnd->ReleaseDC(pTempDC);

		//	Convert the Metrics to pixels first.
		lMetricX = (int32)((float)lMetricX / convertPixX);

		//	Complete the conversion to twips.
		return(Pix2TwipsX(lMetricX));
	}
	int32 Metric2TwipsY(int32 lMetricY)	{
		//	We use a screen DC to get WYSIWYG
		CDC *pTempDC = theApp.m_pMainWnd->GetDC();
	    float convertPixY = ((float)HIMETRIC_INCH / (float)pTempDC->GetDeviceCaps(LOGPIXELSY));
		theApp.m_pMainWnd->ReleaseDC(pTempDC);

		//	Convert the Metrics to pixels first.
		lMetricY = (int32)((float)lMetricY / convertPixY);

		//	Complete the conversion to twips.
		return(Pix2TwipsY(lMetricY));
	}
	int32 Twips2MetricX(int32 lTwipsX)	{
		//	Convert the twips to pixels first.
		lTwipsX = Twips2PixX(lTwipsX);

		//	We use a screen DC to get WYSIWYG
		CDC *pTempDC = theApp.m_pMainWnd->GetDC();
		float convertPixX = ((float)HIMETRIC_INCH / (float)pTempDC->GetDeviceCaps(LOGPIXELSX));
		theApp.m_pMainWnd->ReleaseDC(pTempDC);

		//	Go metric.
		lTwipsX = (int32)((float)lTwipsX * convertPixX);
		return(lTwipsX);
	}
	int32 Twips2MetricY(int32 lTwipsY)	{
		//	Convert the twips to pixels first.
		lTwipsY = Twips2PixY(lTwipsY);

		//	We use a screen DC to get WYSIWYG
		CDC *pTempDC = theApp.m_pMainWnd->GetDC();
		float convertPixY = ((float)HIMETRIC_INCH / (float)pTempDC->GetDeviceCaps(LOGPIXELSY));
		theApp.m_pMainWnd->ReleaseDC(pTempDC);

		//	Go metric.
		lTwipsY = (int32)((float)lTwipsY * convertPixY);
		return(lTwipsY);
	}
	int32 GetOriginX()	{
		return(m_lOrgX);
	}
	int32 GetOriginY()	{
		return(m_lOrgY);
	}
	int32 GetWidth()	{
		return(m_lWidth);
	}
	int32 GetHeight()	{
		return(m_lHeight);
	}
	int32 GetDocumentWidth()	{
		return(m_lDocWidth);
	}
	int32 GetDocumentHeight()	{
		return(m_lDocHeight);
	}

	void SetTransparentColor(BYTE red, BYTE green, BYTE blue);
	IL_IRGB &GetTransparentColor() {return rgbTransparentColor;}

//	Turning on or off display blocking.
private:
	BOOL m_bResolveElements;
protected:
	BOOL CanBlockDisplay() const	{
		return(m_bResolveElements);
	}
public:
	void DisableDisplayBlocking()	{
		m_bResolveElements = FALSE;
	}
	void EnableDisplayBlocking()	{
		m_bResolveElements = TRUE;
	}

//	Coordinate resolution and display blocking.
public:
	virtual BOOL ResolveElement(LTRB& Rect, NI_Pixmap *pImage, int32 lX, int32 lY, 
						   int32 orgx, int32 orgy,
						   uint32 ulWidth, uint32 ulHeight,
                                                   int32 lScaleWidth, int32 lScaleHeight);

	virtual BOOL ResolveElement(LTRB& Rect, int32 x, int32 y, int32 x_offset, int32 y_offset,
								int32 width, int32 height);
	virtual BOOL ResolveElement(LTRB& Rect, LO_SubDocStruct *pSubDoc, int iLocation);
	virtual BOOL ResolveElement(LTRB& Rect, LO_LinefeedStruct *pLineFeed, int iLocation);
	virtual BOOL ResolveElement(LTRB& Rect, LO_CellStruct *pCell, int iLocation);
	virtual BOOL ResolveElement(LTRB& Rect, LO_TableStruct *pTable, int iLocation);
	virtual BOOL ResolveElement(LTRB& Rect, LO_EmbedStruct *pEmbed, int iLocation, Bool bWindowed);
        virtual BOOL ResolveElement(LTRB& Rect, LO_FormElementStruct *pFormElement);
	virtual BOOL ResolveElement(LTRB& Rect, LO_TextStruct *pText, int iLocation, int32 lStartPos, int32 lEndPos, int iClear);

//	How to make sure above functions work on windows 16.
public:
	void SafeSixteen(LTRB& Rect)	{
		//	This routine is called to make sure that drawing coords don't wrap
		//		with GDI functions which take short ints.
		//	Use some hard coded values to end this pain quickly.
		//	Leave some marginal values to allow for additional leeway of
		//		the result (2767 twips).
		//	Maximum display height known is 1200 pixels, which is about 24000
		//		twips, which is kinda close for the future of this product.
		//		This doesn't count those users which somehow get their app much
		//		larger than the screen size by using other means, and they
		//		are dead.
		const int iMax = 30000;
        const int iMin = -30000;

		if(Rect.left > iMax)	{
			Rect.left = iMax;
		}
		else if(Rect.left < iMin)	{
			Rect.left = iMin;
		}
		if(Rect.top > iMax)	{
			Rect.top = iMax;
		}
		else if(Rect.top < iMin)	{
			Rect.top = iMin;
		}
		if(Rect.right > iMax)	{
			Rect.right = iMax;
		}
		else if(Rect.right < iMin)	{
			Rect.right = iMin;
		}
		if(Rect.bottom > iMax)	{
			Rect.bottom = iMax;
		}
		else if(Rect.bottom < iMin)	{
			Rect.bottom = iMin;
		}
	}

//	Font selection, deselection, caching
private:
	enum	{
		m_MaxFontSizes = 9,
		m_MaxUnderline = 2,
		m_MaxItalic = 2,
		m_MaxBold = 2,
		m_MaxFixed = 2
	};
private :
	CPtrList m_cplCachedFontList;

protected:
	HDC				m_lastDCWithCachedFont;
	CyaFont			*m_pSelectedCachedFont;
	int				m_iOffset;
public:
	static void ClearAllFontCaches();
	void ClearFontCache();
	virtual int SelectNetscapeFont( HDC hdc, LO_TextAttr *pAttr, CyaFont *& pMyFont);
	virtual int SelectNetscapeFontWithCache( HDC hdc, LO_TextAttr *pAttr, CyaFont *& pMyFont );
	virtual void ReleaseNetscapeFont(HDC hdc, CyaFont * pNetscapeFont);
	virtual void ReleaseNetscapeFontWithCache(HDC hdc, CyaFont * pNetscapeFont);
	virtual void ChangeFontOffset(int iIncrementor);
//	Context sensitive attribute mapping.
public:
	virtual COLORREF ResolveTextColor(LO_TextAttr *pAttr);
	virtual COLORREF ResolveBGColor(unsigned uRed, unsigned uGreen, unsigned uBlue);
	virtual BOOL ResolveHRSolid(LO_HorizRuleStruct *pHorizRule);
	virtual BOOL ResolveLineSolid();
	virtual void ResolveTransparentColor(unsigned uRed, unsigned uGreen, unsigned uBlue);
	virtual COLORREF ResolveDarkLineColor();
	virtual COLORREF ResolveLightLineColor();
	virtual COLORREF ResolveBorderColor(LO_TextAttr *pAttr);
	COLORREF ResolveLOColor(LO_Color *color);
	virtual PRBool ResolveIncrementalImages();

#ifdef XP_WIN16
    static void HugeFree(void XP_HUGE *pFreeMe)	{
#ifdef __WATCOMC__
		hfree(pFreeMe);
#else
		_hfree(pFreeMe);
#endif
	}
	void XP_HUGE *HugeAlloc(int32 iSize, int32 iNum)	{
#ifdef __WATCOMC__
		return(halloc( (long) iSize, (size_t) iNum));
#else
		return(_halloc( (long) iSize, (size_t) iNum));
#endif
	}
#else
	static void HugeFree(void XP_HUGE *pFreeMe)	{
		free(pFreeMe);
	}
	void XP_HUGE *HugeAlloc(int32 iSize, int32 iNum)	{
		return(calloc(iNum, iSize));
	}
#endif

	BOOL ResolveTextExtent(int16 wincsid, HDC pDC, LPCTSTR pString, int iLength, LPSIZE pSize, CyaFont *pMyFont);
	BOOL ResolveTextExtent(HDC pDC, LPCTSTR pString, int iLength, LPSIZE pSize, CyaFont *pMyFont );
	BOOL ResolveTextExtent(HDC pDC, LPCTSTR pString, int iLength, LPSIZE pSize)
	{
		BOOL bRetval;
		bRetval = CIntlWin::GetTextExtentPoint(0, pDC, pString, iLength, pSize);
		return(bRetval);
	}

//	Color and palette.
protected:
	COLORREF m_rgbLightColor;
	COLORREF m_rgbDarkColor;
	HPALETTE m_pPal;
	BOOL	  m_bRGB565;  // driver supports 565 mode for DIBs
public:
	virtual HPALETTE  GetPalette() const;
	COLORREF m_rgbBackgroundColor;  // let our view access this

//	Misc draw helpers
public:
	virtual double CalcFontPointSize(int size, int iBaseSize, int iOffset = 0);
	void Display3DBox(LTRB& Rect, COLORREF rgbLight, COLORREF rgbDark, int32 lWidth, BOOL bAdjust = FALSE);
    void EditorDisplayZeroWidthBorder(LTRB& Rect, BOOL bSelected);
    void Compute3DColors(COLORREF rgbColor, COLORREF &rgbLight, COLORREF &rgbDark);
    // Returns width of selection border (if any) - used only by Composer
    int32 DisplayTableBorder(LTRB& Rect, LO_TableStruct *pTable);
    void Display3DBorder(LTRB& Rect, COLORREF rgbLight, COLORREF rgbDark, LTRB &);
	BOOL cxLoadBitmap(LPCSTR pszBmName, char** bits,  BITMAPINFOHEADER** myBitmapInfo); 
	virtual void DisplayIcon(int32 x, int32 y, int icon_number);
	virtual void GetIconDimensions(int32* width, int32* height, int iconNumber);
    void FloodRect(LTRB& Rect, HBRUSH hColor);
	BOOL WriteBitmapFile(LPCSTR lpszFileName, LO_ImageStruct*);
	HANDLE WriteBitmapToMemory(	IL_ImageReq *image_req, LO_Color* bgColor);
	BOOL CanWriteBitmapFile(LO_ImageStruct*);

    // Uses the document color/backdrop by default, but you can specify a color or backdrop image to be used instead
	virtual BOOL _EraseBkgnd(HDC pDC, RECT&, int32 tileX, int32 tileY, 
							 LO_Color* bg = NULL);

    void DisplaySelectionFeedback(uint16 ele_attrmask, const LTRB& rect);
    void EraseToBackground(const LTRB& rect, int32 borderWidth = 0);

protected:
	int m_iBitsPerPixel;
	BOOL m_bUseDibPalColors;

public:
	BOOL GetUseDibPalColors() {
	     return m_bUseDibPalColors;
	}

	int GetBitsPerPixel() {
	    return m_iBitsPerPixel;
	}
	void SetUseDibPalColors(BOOL flag) {
		m_bUseDibPalColors = flag;
	}

//	Preference representations needed per context.
protected:
public:

//	OLE Embed implementation.
private:
	BOOL m_bAutoDeleteDocument;
protected:
	CGenericDoc *m_pDocument;
public:
	CGenericDoc *GetDocument() const	{
		return(m_pDocument);
	}
	void ClearDoc()	{
		m_pDocument = NULL;
	}
void GetPluginRect(MWContext *pContext,
                   LO_EmbedStruct *pLayoutData,
                   RECT &rect, int iLocation, 
                   BOOL windowed);
BOOL IsPluginFullPage(LO_EmbedStruct *pLayoutData);
BOOL ContainsFullPagePlugin();

//	Document helpers.
public:
	virtual BOOL OnOpenDocumentCX(const char *pPathName);

//	URL retrieval
public:
	virtual int GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoad = TRUE, BOOL bForceNew = FALSE);

//	Function for easy creation of a URL from the current history entry.
public:
	virtual URL_Struct *CreateUrlFromHist(BOOL bClearStateData = FALSE, SHIST_SavedData *pSavedData = NULL, BOOL bWysiwyg = FALSE);

//	Image conditional loading.
private:
	CString m_csForceLoadImage;
	CString m_csNexttimeForceLoadImage;
	BOOL m_bLoadImagesNow;
	BOOL m_bNexttimeLoadImagesNow;
public:
	void ExplicitlyLoadThisImage(const char *pImageUrl)	{
		m_csNexttimeForceLoadImage = pImageUrl;
		NiceReload();
	}
	void ExplicitlyLoadAllImages()	{
		m_bNexttimeLoadImagesNow = TRUE;
		NiceReload();
	}

//	Wether or not we exist in an OLE server....
private:
	BOOL m_bOleServer;
public:
	BOOL IsOleServer() const	{
		return(m_bOleServer);
	}
	void EnableOleServer();
	void DisableOleServer()	{
		m_bOleServer = FALSE;
	}

//  Font for Form Text Entry
private:
	friend class CFormElement;
	enum	{
		m_FixedFont,
		m_ProportionalFont
	};

#if 0 /* dp */
	int		m_DownloadedFontList;	// for testing
#endif /* 0 */
	HFONT m_pFormFixFont;
	HFONT m_pFormPropFont;
	int   m_iFormFixCSID;	// csid for current form.
	int   m_iFormPropCSID;	// csid for current form.
	void ExtendCoord(LTRB &Rect);
	virtual void GetDrawingOrigin(int32 *plOrgX, int32 *plOrgY)
	  { *plOrgX = 0; *plOrgY = 0; }
        virtual FE_Region GetDrawingClip()
          { return NULL; }
        

private :
	void CDCCX::DrawTabFocusLine( HDC hdc, BOOL supportPixel, int x, int y, int x2, int y2);
	void CDCCX::DrawTabFocusRect( HDC hdc, BOOL supportPixel, int left, int top, int right, int bottom);
	void CDCCX::DrawRectBorder(LTRB& Rect, COLORREF rgbColor, int32 lWidth);
	void CDCCX::DrawMapAreaBorder( int baseX, int baseY, lo_MapAreaRec * theArea );

//	MFC Helpers;  abstraction away
public:
	virtual void ViewImages();
	virtual BOOL CanViewImages();

//	Context Overrides
	virtual void DisplayBullet(MWContext *pContext, int iLocation, LO_BullettStruct *pBullet);
	virtual void DisplayEmbed(MWContext *pContext, int iLocation, LO_EmbedStruct *pEmbed);
    virtual void DisplayBorder(MWContext *pContext, int iLocation, int x, int y, int width, int height, int bw, LO_Color *color, LO_LineStyle style);
	virtual void DisplayHR(MWContext *pContext, int iLocation, LO_HorizRuleStruct *pHorizRule);
	virtual BITMAPINFO*	 NewPixmap(NI_Pixmap* pImage, BOOL mask = FALSE);
	virtual int	 DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, int32 lScaleWidth, int32 lScaleHeight, LTRB& Rect);
	virtual void DisplayLineFeed(MWContext *pContext, int iLocation, LO_LinefeedStruct *pLineFeed, XP_Bool clear);
	virtual void DisplaySubDoc(MWContext *pContext, int iLocation, LO_SubDocStruct *pSubDoc);
	virtual void DisplayCell(MWContext *pContext, int iLocation, LO_CellStruct *pCell);
	virtual void DisplaySubtext(MWContext *pContext, int iLocation, LO_TextStruct *pText, int32 lStartPos, int32 lEndPos, XP_Bool clear);
	virtual void DisplayTable(MWContext *pContext, int iLocation, LO_TableStruct *pTable);

	// need to keep the old interface  for calling from C
	virtual void DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool clear);
	void CDCCX::DisplayImageFeedback(MWContext *pContext, int iLocation, LO_Element *pElement, lo_MapAreaRec * theArea, uint32 drawFlag );

	// /* cannot use default drawFlag = 0, if this function is called from C */
	virtual void DisplayText(MWContext *pContext, int iLocation, LO_TextStruct *pText, XP_Bool clear, uint32 drawFlag );
	// handle strike and under-line, after text is drawn.
	virtual void CDCCX::DrawTextPostDecoration(HDC hdc, LO_TextAttr * attr, LTRB * Rect, COLORREF rgbColor );

        virtual void DisplayWindowlessPlugin(MWContext *pContext, LO_EmbedStruct *pEmbed, NPEmbeddedApp *pEmbeddedApp, int iLocation);
	virtual void DisplayPlugin(MWContext *pContext, LO_EmbedStruct *pEmbed, NPEmbeddedApp* pEmbeddedApp, int iLocation);
#ifdef LAYERS
        virtual void EraseBackground(MWContext *pContext, int iLocation, int32 x, int32 y, uint32 width, uint32 height, LO_Color *pColor);
#endif
	virtual void FreeEmbedElement(MWContext *pContext, LO_EmbedStruct *pEmbed);
	virtual void GetEmbedSize(MWContext *pContext, LO_EmbedStruct *pEmbed, NET_ReloadMethod bReload);
#ifdef LAYERS
	virtual void GetTextFrame(MWContext *pContext, LO_TextStruct *pText,
				  int32 start, int32 end, XP_Rect *frame);
#endif
       	virtual int GetTextInfo(MWContext *pContext, LO_TextStruct *pText, LO_TextInfo *pTextInfo);
	virtual void ImageComplete(NI_Pixmap* image);
	virtual void LayoutNewDocument(MWContext *pContext, URL_Struct *pURL, int32 *pWidth, int32 *pHeight, int32 *pmWidth, int32 *pmHeight);
    virtual void SetBackgroundColor(MWContext *pContext, uint8 uRed, uint8 uGreen, uint8 uBlue);
    virtual COLORREF GetBackgroundColor() {return m_rgbBackgroundColor;}
    virtual void Set3DColors( COLORREF crBackground );
    static HPALETTE InitPalette(HDC hdc);
	static HPALETTE CreateColorPalette(HDC hdc, IL_IRGB& transparentColor, int bitsPerPixel);
	static void SetColormap(HDC hdc, NI_ColorMap *pMap, IL_IRGB& transparentColor, HPALETTE hPal);
	virtual void SetDocDimension(MWContext *pContext, int iLocation, int32 lWidth, int32 lLength);
	virtual void GetDocPosition(MWContext *pContext, int iLocation, int32 *lX_p, int32 *lY_p);
	virtual void DisplayFormElement(MWContext *pContext, int iLocation, LO_FormElementStruct *pFormElement);
	virtual void FormTextIsSubmit(MWContext *pContext, LO_FormElementStruct *pFormElement);
	virtual void GetFormElementInfo(MWContext *pContext, LO_FormElementStruct *pFormElement);
	virtual void GetFormElementValue(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool bTurnOff);
	virtual void ResetFormElement(MWContext *pContext, LO_FormElementStruct *pFormElement);
	virtual void SetFormElementToggle(MWContext *pContext, LO_FormElementStruct *pFormElement, XP_Bool iState);

#ifdef TRANSPARENT_APPLET
        virtual void DrawJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *pJava);
#endif

protected:
	void StretchMaskBlt(HDC hTargetDC, HBITMAP theBitmap, HBITMAP theMask, 
								int32 dx, int32 dy, int32 dw, int32 dh,
								int32 sx, int32 sy, int32 sw, int32 sh);
	void StretchPixmap(HDC hTargetDC, NI_Pixmap* pImage, 
								int32 dx, int32 dy, int32 dw, int32 dh,
								int32 sx, int32 sy, int32 sw, int32 sh);
	virtual void _StretchDIBitsWithMask(HDC pDC,
							int iDestX,
							int iDestY,
							int iDestWidth,
							int iDestHeight,
							int iSrcX,
							int iSrcY,
							int iSrcWidth,
							int iSrcHeight,
							NI_Pixmap *image,
							NI_Pixmap *mask);

#ifdef DDRAW
	BOOL TransparentBlt(int iDestX,
							int iDestY,
							int iDestWidth,
							int iDestHeight,
							int iSrcX,
							int iSrcY,
							int iSrcWidth,
							int iSrcHeight,
							NI_Pixmap *image,
							COLORREF transColor,
							LPDIRECTDRAWSURFACE surf);
#endif
	BITMAPINFO* FillBitmapInfoHeader(NI_Pixmap* pImage);
	HBITMAP CreateBitmap(HDC hTargetDC,  NI_Pixmap *image);
	HBITMAP CreateMask(HDC hTargetDC, NI_Pixmap* mask); 
	void TileImage(HDC hdc, LTRB& Rect, NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y);

};

// Global functions

// Calculate a contrastng text and background colors to use for selection
//   given a background color
void wfe_GetSelectionColors( COLORREF rgbBackgroundColor, 
                             COLORREF* pTextColor, COLORREF* pBackColor);

//	Global variables
//

//	Macros
//


#endif // __Context_Device_Context_H

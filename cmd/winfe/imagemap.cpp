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

#include "stdafx.h"
#include "imagemap.h"


class CNSImageMapFactory :  public  CGenericFactory
{
public:
    CNSImageMapFactory();
    ~CNSImageMapFactory();
    STDMETHODIMP CreateInstance(LPUNKNOWN pUnkOuter,REFIID refiid, LPVOID * ppvObj);
};

CNSImageMapFactory::CNSImageMapFactory()
{
    ApiApiPtr(api);
	api->RegisterClassFactory(APICLASS_IMAGEMAP,this);
}

CNSImageMapFactory::~CNSImageMapFactory()
{
}

STDMETHODIMP CNSImageMapFactory::CreateInstance(
    LPUNKNOWN pUnkOuter,
    REFIID refiid,
    LPVOID * ppvObj)
{
    CNSImageMap * pImageMap = new CNSImageMap;
    *ppvObj = (LPVOID)((LPUNKNOWN)pImageMap);
    return NOERROR;
}

DECLARE_FACTORY(CNSImageMapFactory);

////////////////////////////////////////////////////////////////////////
// CNSImageMap implementation

CNSImageMap::CNSImageMap()
{
    m_pImageMap = NULL;
}

CNSImageMap::~CNSImageMap()
{
    ASSERT(m_pImageMap);
    if (m_pImageMap)
        delete m_pImageMap;
}


STDMETHODIMP CNSImageMap::QueryInterface(
   REFIID refiid,
   LPVOID * ppv)
{
    *ppv = NULL;

    if (IsEqualIID(refiid,IID_IImageMap))
        *ppv = (LPIMAGEMAP) this;

    if (*ppv != NULL) {
   	    AddRef();
        return NOERROR;
    }

    return CGenericObject::QueryInterface(refiid,ppv);
}


int CNSImageMap::Initialize (unsigned int rcid, int width, int height)
{
    m_pImageMap = new CTreeImageMap(rcid,width,height);
    ASSERT(m_pImageMap);
    m_pImageMap->Initialize();
    return 0;
}

void CNSImageMap::DrawImage (int index,int x,int y, HDC hDestDC,BOOL bButton)
{
    ASSERT(m_pImageMap);
    m_pImageMap->DrawImage(index,x,y,hDestDC,bButton);
}

void CNSImageMap::DrawTransImage (int index,int x,int y, HDC hDestDC )
{
    ASSERT(m_pImageMap);
    m_pImageMap->DrawTransImage(index,x,y,hDestDC);
}

void CNSImageMap::DrawImage (int index,int x,int y,CDC * pDestDC,BOOL bButton)
{
    ASSERT(m_pImageMap);
    m_pImageMap->DrawImage(index,x,y,pDestDC->GetSafeHdc(),bButton);
}

void CNSImageMap::DrawTransImage (int index,int x,int y,CDC * pDestDC )
{
    ASSERT(m_pImageMap);
    m_pImageMap->DrawTransImage(index,x,y,pDestDC->GetSafeHdc());
}

void CNSImageMap::ReInitialize (void)
{
    ASSERT(m_pImageMap);
    m_pImageMap->ReInitialize();
}

void CNSImageMap::UseNormal (void)
{
    ASSERT(m_pImageMap);
    m_pImageMap->UseNormal();
}

void CNSImageMap::UseHighlight (void)
{
    ASSERT(m_pImageMap);
    m_pImageMap->UseHighlight();
}


int CNSImageMap::GetImageHeight (void)
{
    ASSERT(m_pImageMap);
    return m_pImageMap->GetImageHeight();
}

int CNSImageMap::GetImageWidth (void)
{
    ASSERT(m_pImageMap);
    return m_pImageMap->GetImageWidth();
}

unsigned int CNSImageMap::GetResourceID (void)
{
    if (!m_pImageMap)
        return 0;
    return m_pImageMap->GetResourceID();
}

int CNSImageMap::GetImageCount (void)
{
    ASSERT(m_pImageMap);
    return m_pImageMap->GetImageCount();
}

//////////////////////////////////////////////////////////////////////////////
// CTreeImageMap

CTreeImageMap::CTreeImageMap (unsigned int rcid, int width, int height )
{
    m_iResourceID = rcid;
    m_iImageWidth = width;
    m_iImageHeight = height;
    CreateDefaults ( width, height );
    
}

void CTreeImageMap::CreateDefaults ( int width, int height )
{
    m_hdcMem = ::CreateCompatibleDC ( NULL );
    ::SetBkColor( m_hdcMem, RGB(0xff,0xff,0xff) );
    ::SetTextColor( m_hdcMem, RGB(0x00, 0x00, 0x00) );

	VERIFY( m_hBitmap = ::LoadBitmap(AfxGetResourceHandle(), MAKEINTRESOURCE(m_iResourceID)) );
	BITMAP bm;
    ::GetObject( m_hBitmap, sizeof(bm), &bm );
    m_iBitmapWidth = bm.bmWidth;
    m_iImageCount  = m_iBitmapWidth / width;

    m_hbmNormal = NULL;
    m_hbmHighlight = NULL;
    m_hbmButton = NULL;
    m_hOriginalBitmap = (HBITMAP)::SelectObject ( m_hdcMem, m_hBitmap );
    m_iImageCount = 0;

    m_iImageWidth =  width;  
    m_iImageHeight = height;
}

CTreeImageMap::~CTreeImageMap ( )
{
    ::SelectObject ( m_hdcMem, m_hOriginalBitmap );
    VERIFY(::DeleteObject(m_hbmNormal));
    VERIFY(::DeleteObject(m_hbmHighlight));
    VERIFY(::DeleteObject(m_hbmButton));
    VERIFY(::DeleteObject(m_hBitmap));

    VERIFY(::DeleteDC( m_hdcMem ));
}

void CTreeImageMap::UseHighlight ( void ) 
{
    if ( m_hdcMem )
        ::SelectObject( m_hdcMem, m_hbmHighlight );
}

void CTreeImageMap::UseNormal ( void ) 
{
    if ( m_hdcMem )
        ::SelectObject( m_hdcMem, m_hbmNormal );
}

void CTreeImageMap::DrawImage ( int idxImage, int x, int y, HDC hDestDC, BOOL bButton )
{
    HBITMAP hOldBitmap = NULL;
    if (bButton) 
        hOldBitmap = (HBITMAP) ::SelectObject( m_hdcMem, m_hbmButton);

    ::BitBlt( hDestDC, x, y, m_iImageWidth, m_iImageHeight, 
			  m_hdcMem, idxImage * m_iImageWidth, 0, SRCCOPY );
    
	if ( hOldBitmap ) 
        ::SelectObject( m_hdcMem, hOldBitmap );
}

void CTreeImageMap::DrawTransImage ( int idxImage, int x, int y, HDC hDstDC )
{
    HBITMAP hOldBitmap = (HBITMAP) ::SelectObject( m_hdcMem, m_hBitmap );

	::FEU_TransBlt( hDstDC, x, y, m_iImageWidth, m_iImageHeight,
					m_hdcMem, idxImage * m_iImageWidth, 0 ,WFE_GetUIPalette(NULL));

    if (hOldBitmap) 
        SelectObject( m_hdcMem, hOldBitmap );
}

int CTreeImageMap::InitializeImage ( HBITMAP hImage )
{
    if (m_hdcMem && m_hBitmap)
    {
        // main color bitmap
        HDC hMemDC = ::CreateCompatibleDC ( m_hdcMem );
        HBITMAP hOldBitmap = (HBITMAP) ::SelectObject ( hMemDC, hImage );
        ::BitBlt ( m_hdcMem, 0, 0, m_iImageWidth, m_iImageHeight, hMemDC, 0, 0, SRCCOPY );
        ::SelectObject ( hMemDC, hOldBitmap );
    } 
    return 0;
}

void CTreeImageMap::Initialize ( void )
{
    HDC hdcMem, hdcDest;
    RECT rect;
	::SetRect( &rect, 0, 0, m_iBitmapWidth, m_iImageHeight );

    HBRUSH hBrush = ::CreateSolidBrush( GetSysColor ( COLOR_WINDOW ) ), hOldBrush;
    HPEN hPen = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_WINDOW ) ), hOldPen;
    HBRUSH hHighBrush = ::CreateSolidBrush( GetSysColor ( COLOR_HIGHLIGHT ) );
    HPEN hHighPen = ::CreatePen( PS_SOLID, 1, GetSysColor ( COLOR_HIGHLIGHT ) );
    HBRUSH hButtonBrush = ::CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    HPEN hButtonPen = ::CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNFACE));

    m_hbmNormal = ::CreateCompatibleBitmap( m_hdcMem, m_iBitmapWidth, m_iImageHeight );
    m_hbmHighlight = ::CreateCompatibleBitmap( m_hdcMem, m_iBitmapWidth, m_iImageHeight );
    m_hbmButton = ::CreateCompatibleBitmap( m_hdcMem, m_iBitmapWidth, m_iImageHeight );

	HBITMAP hbmSrc = ::CreateCompatibleBitmap( m_hdcMem, m_iBitmapWidth, m_iImageHeight );

    hdcMem = ::CreateCompatibleDC( m_hdcMem );
    hdcDest = ::CreateCompatibleDC( m_hdcMem );
    ::SetBkColor( hdcMem, RGB(0xff,0xff,0xff) );
    ::SetBkColor( hdcDest, RGB(0xff,0xff,0xff) );
    ::SetTextColor( hdcMem, RGB(0x00,0x00,0x00) );
    ::SetTextColor( hdcDest, RGB(0x00,0x00,0x00) );

	HBITMAP hbmMask = ::CreateBitmap ( m_iBitmapWidth, m_iImageHeight, 1, 1, NULL );

    // create the mask bitmap...
    HBITMAP hOld = (HBITMAP) SelectObject( m_hdcMem, hbmMask );
    HBITMAP hbmOldMem = (HBITMAP) ::SelectObject( hdcMem,  m_hBitmap );
    COLORREF cOld = ::SetBkColor ( hdcMem, RGB(255,0,255) );
    ::BitBlt( m_hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, NOTSRCCOPY );
	::SelectObject( m_hdcMem, hbmSrc );
    ::BitBlt( m_hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, SRCCOPY );
    ::SetBkColor ( hdcMem, cOld );
    ::SelectObject( hdcMem,  hbmOldMem );
    // end mask

    // create the normal (unactive highlight bar)
    HBITMAP hbmOldDest = (HBITMAP) ::SelectObject( hdcDest,  m_hbmNormal );
    hOldBrush = (HBRUSH) ::SelectObject( hdcDest,  hBrush );
    hOldPen = (HPEN) ::SelectObject( hdcDest,  hPen );
    ::Rectangle( hdcDest, rect.left, rect.top, rect.right, rect.bottom );

    hbmOldMem = (HBITMAP) ::SelectObject( hdcMem,  hbmMask );

    // replace pink with black
    ::BitBlt( m_hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, SRCAND );

    // invert mask
    ::BitBlt( hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, NOTSRCCOPY );

    // background
    ::BitBlt( hdcDest, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, SRCAND );

    // invert again
    ::BitBlt( hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, NOTSRCCOPY );

    ::BitBlt( hdcDest, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  m_hdcMem, 0, 0, SRCPAINT );

    // create the highlight bar
    ::SelectObject( hdcDest,  m_hbmHighlight );
    ::SelectObject( hdcDest,  hHighBrush );
    ::SelectObject( hdcDest,  hHighPen );
    ::Rectangle( hdcDest, rect.left, rect.top, rect.right, rect.bottom );

    // invert mask
    ::BitBlt( hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, NOTSRCCOPY );

    // background
    ::BitBlt( hdcDest, 0, 0, m_iBitmapWidth, m_iImageHeight,
			  hdcMem, 0, 0, SRCAND );

    // invert again
    ::BitBlt( hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, NOTSRCCOPY );

    ::BitBlt( hdcDest, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  m_hdcMem, 0, 0, SRCPAINT );

    // create the button bar
    ::SelectObject( hdcDest,  m_hbmButton );
    ::SelectObject( hdcDest,  hButtonBrush );
    ::SelectObject( hdcDest,  hButtonPen );
    ::Rectangle( hdcDest, rect.left, rect.top, rect.right, rect.bottom );

    // invert mask
    ::BitBlt( hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight,
			  hdcMem, 0, 0, NOTSRCCOPY );

    // background
    ::BitBlt( hdcDest, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, SRCAND );

    // invert again
    ::BitBlt( hdcMem, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  hdcMem, 0, 0, NOTSRCCOPY );

    ::BitBlt( hdcDest, 0, 0, m_iBitmapWidth, m_iImageHeight, 
			  m_hdcMem, 0, 0, SRCPAINT );

    ::SelectObject( hdcDest,  hOldBrush );
    ::SelectObject( hdcDest,  hOldPen );
    ::SelectObject( hdcDest,  hbmOldDest );
    ::SelectObject( hdcMem,  hbmOldMem );

	::SelectObject( m_hdcMem, hOld );
    ::SelectObject( m_hdcMem, m_hbmNormal );

	VERIFY(::DeleteObject( hBrush ));
	VERIFY(::DeleteObject( hPen ));
	VERIFY(::DeleteObject( hHighBrush ));
	VERIFY(::DeleteObject( hHighPen ));
	VERIFY(::DeleteObject( hButtonBrush ));
	VERIFY(::DeleteObject( hButtonPen ));

	VERIFY(::DeleteObject( hbmSrc ));
	VERIFY(::DeleteObject( hbmMask ));

	VERIFY(::DeleteDC( hdcMem ));
	VERIFY(::DeleteDC( hdcDest ));
}

void CTreeImageMap::ReInitialize ( void )
{
    ::SelectObject ( m_hdcMem, m_hOriginalBitmap );

	if ( m_hbmNormal ) {
	    VERIFY(::DeleteObject(m_hbmNormal));
		m_hbmNormal = NULL;
	}
	if ( m_hbmHighlight ) {
		VERIFY(::DeleteObject(m_hbmHighlight));
		m_hbmHighlight = NULL;
	}
	if ( m_hbmButton ) {
		VERIFY(::DeleteObject(m_hbmButton));
		m_hbmButton = NULL;
	}
	if ( m_hBitmap ) {
	    VERIFY(::DeleteObject(m_hBitmap));
		m_hBitmap = NULL;
	}

    VERIFY(::DeleteDC( m_hdcMem ));

    CreateDefaults ( m_iImageWidth, m_iImageHeight );
	Initialize();
}

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

#ifndef __IMAGEMAP_H
#define __IMAGEMAP_H

#ifndef __APIIMG_H
    #include "apiimg.h"
#endif

class CTreeImageMap 
{
protected:
    int m_iImageWidth;
    int m_iImageHeight;
    int m_iImageCount;
    int m_iBitmapWidth;
    unsigned int m_iResourceID;
    HBITMAP m_hBitmap;
    HBITMAP m_hbmNormal;
    HBITMAP m_hbmHighlight;
    HBITMAP m_hbmButton;
    HBITMAP m_hOriginalBitmap;
    HDC m_hdcMem;

    int InitializeImage (HBITMAP);

public:
    CTreeImageMap (unsigned int rcid, int width = 16, int height = 16 );
    ~CTreeImageMap ( );
    void Initialize ( void );
    int GetImageHeight ( void ) { return m_iImageHeight; }
    int GetImageWidth ( void )  { return m_iImageWidth; }
    int GetImageCount ( void )  { return m_iImageCount; }
    unsigned int GetResourceID ( void ) { return m_iResourceID; }
    void DrawImage ( int idxImage, int x, int y, HDC hDestDC, BOOL bButton = FALSE );
    void DrawTransImage ( int idxImage, int x, int y, HDC hDestDC );
    void UseHighlight ( void );
    void UseNormal ( void );
    void CreateDefaults (int width = 16, int height = 16);
    void ReInitialize(void);
};

class CNSImageMap :         public  CGenericObject,
                            public  IImageMap
{
protected:
    CTreeImageMap * m_pImageMap;
public:
    CNSImageMap();
    ~CNSImageMap();

	STDMETHODIMP QueryInterface(REFIID,LPVOID *);

    // IImageMap
    virtual int  Initialize (unsigned int rcid, int width, int height);
    virtual void DrawImage (int index,int x,int y,HDC hDestDC,BOOL bButton);
    virtual void DrawTransImage (int index,int x,int y,HDC hDestDC);
    virtual void DrawImage (int index,int x,int y,CDC * pDestDC,BOOL bButton);
    virtual void DrawTransImage (int index,int x,int y,CDC * pDestDC);
    virtual void ReInitialize (void);
    virtual void UseNormal (void);
    virtual void UseHighlight (void);
    virtual int GetImageHeight (void);
    virtual int GetImageWidth (void);
    virtual int GetImageCount (void);
    virtual unsigned int GetResourceID (void);
};

#endif

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

//   drawable.h - Classes used for maintaining the notion of 
//                offscreen and onscreen drawables, so that 
//                document contents can be drawn to either
//                location. Currently used for LAYERS.
#include "stdafx.h"
#include "layers.h"
#include "fe_rgn.h"
#include "cl_types.h"

#ifndef _DRAWABLE_H_
#define _DRAWABLE_H_

#ifdef LAYERS 

extern CL_DrawableVTable wfe_drawable_vtable;

class CDrawable {
  protected:
    int32 m_lOrgX, m_lOrgY;
    FE_Region m_hClipRgn;
	CAbstractCX*	m_parentContext;

  public:
    // Constructor and destructor
    CDrawable();
    CDrawable(	CAbstractCX*	m_parentContext);
    ~CDrawable() {}
 
    // Lock the drawable for use, based on the state passed in.
    // Does nothing in the super class.
    virtual PRBool LockDrawable(CL_Drawable *pCLDrawable, 
				CL_DrawableState newState) 
      { return PR_TRUE; }

    // Calls to indicate a new client or the relinquishing of the drawable
    // by a client.
    virtual void InitDrawable(CL_Drawable *pCLDrawable) {};
    virtual void RelinquishDrawable(CL_Drawable *pCLDrawable) {};

    // Get and set the origin of the drawable
    void SetOrigin(int32 lOrgX, int32 lOrgY);
    void GetOrigin(int32 *lOrgX, int32 *lOrgY);

    // Set the drawable's clip. A value of NULL indicates that the
    // clip should be restored to its initial value.
    virtual void SetClip(FE_Region hClipRgn);

    FE_Region GetClip() { return m_hClipRgn; }
        
    // Call to copy pixels from the given drawable
    void CopyPixels(CDrawable *pSrcDrawable, FE_Region hCopyRgn);

    // Call to set the width and height of a drawable;
    virtual void SetDimensions(int32 width, int32 height) {}
    
    // Call to set the palette of drawable
    virtual void SetPalette(HPALETTE hPal) {}

    // Return the device context associated with the drawable
    virtual HDC GetDrawableDC() {return NULL;}
	virtual void ReleaseDrawableDC(HDC hdc) {;}
};

class COnscreenDrawable : public CDrawable {
  protected:
    HDC m_hDC;
  public:
    COnscreenDrawable(HDC hDC, CAbstractCX* parentContext);
    ~COnscreenDrawable() {}

    // Return the device context associated with the drawable
    virtual HDC GetDrawableDC() {return m_hDC;}
	virtual void ReleaseDrawableDC(HDC hdc) {;}
};

class CPrinterDrawable : public CDrawable {
  protected:
    HDC m_hDC;
    int32 m_lLeftMargin, m_lRightMargin, m_lTopMargin, m_lBottomMargin;
    
  public:
    CPrinterDrawable(HDC hDC, int32 lLeftMargin, int32 lRightMargin,
                     int32 lTopMargin, int32 lBottomMargin, CAbstractCX* parentContext);
    ~CPrinterDrawable() {}

    virtual void SetClip(FE_Region hClipRgn);

    // Return the device context associated with the drawable
    virtual HDC GetDrawableDC() {return m_hDC;}
	virtual void ReleaseDrawableDC(HDC hdc) {;}
};

class COffscreenDrawable : public CDrawable {
  protected:
    HBITMAP m_hSaveBitmap;
    HPALETTE m_hSavePalette;
    HDC m_hParentDC;
	HPALETTE m_hParentPal;

    // In this implementation, we only have a single offscreen drawable
    static COffscreenDrawable *m_pOffscreenDrawable;
    static CL_Drawable *m_pOwner;
    static int32 m_lWidth, m_lHeight;
    static HBITMAP m_hBitmap;
    static HDC m_hDC;
    static UINT m_uRefCnt;
    static HPALETTE m_hSelectedPalette;

  public:
    COffscreenDrawable(HDC hParentDC, HPALETTE hPal, CAbstractCX* parentContext);
    ~COffscreenDrawable();
    
    // Factory for creating offscreen drawables
    static COffscreenDrawable *AllocateOffscreen(HDC hParentDC, HPALETTE hPal, CAbstractCX* parentContext);

    virtual void InitDrawable(CL_Drawable *pCLDrawable);
    virtual void RelinquishDrawable(CL_Drawable *pCLDrawable);

    virtual PRBool LockDrawable(CL_Drawable *pCLDrawable, 
				CL_DrawableState newState);
    virtual void SetDimensions(int32 width, int32 height);

    virtual void SetPalette(HPALETTE hPal);
    virtual void SetClip(FE_Region hClipRgn);
    
    // Return the device context associated with the drawable
    virtual HDC GetDrawableDC();
	virtual void ReleaseDrawableDC(HDC hdc);

  private:
    void delete_offscreen();
};

#endif // LAYERS 
#endif // _DRAWABLE_H_ 

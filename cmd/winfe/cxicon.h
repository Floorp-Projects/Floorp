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

#ifndef CXIcon_H
#define CXIcon_H
#include "cxdc.h"
void Icon_GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext);  
class CXIcon;

class CCustomImageObject
{
protected:
	CPtrList loadingImagesList; // A list of images that this window is still waiting for.

public:
	virtual void LoadComplete(HT_Resource r) = 0;

	virtual CRDFImage* LookupImage(const char* url, HT_Resource r);

	virtual ~CCustomImageObject();

	virtual void AddLoadingImage(CRDFImage* pImage);
	virtual void RemoveLoadingImage(CRDFImage* pImage);
};

struct CIconCallbackInfo
{
	HT_Resource pResource;
	CCustomImageObject* pObject;

	CIconCallbackInfo(CCustomImageObject* pObj, HT_Resource pRes)
		:pObject(pObj), pResource(pRes) {}
};


class CRDFImage 
{
public:
	char *pUrl;	// The URL at which the image can be found.
	
	BOOL m_bCompletelyLoaded;	// Whether or not the image has completely finished loading.
	BOOL m_bFrameLoaded;		// Whether or not a single frame of the image is available yet.
								// (Lets you know whether or not you can start drawing something.)

	BITMAPINFO *bmpInfo;		// The current bits of the image.
	void XP_HUGE *bits;
	void XP_HUGE *maskbits;
	HDC hSubDC;
	
	BOOL m_BadImage;					// Whether or not to use the bad image bitmap.
	static HBITMAP m_hBadImageBitmap;	// A bitmap to use if the image is not obtainable.

	static int refCount;				// A reference counter for the total # of RDF images that currently exist.
	
	CXIcon* iconContext;				// The context that is performing the load.
	
	CPtrList resourceList;				// The observers of this image.
	int m_nRefCount;					// The # of observers.
	
	int pairCount;						// Specifies whether or not the mask and the image bits are ready.

public:
	CRDFImage(const char * pUrl);
	virtual ~CRDFImage();
	
	void ProcessIcon();
	void CompleteCallback();
	void CompleteFrameCallback();

	BOOL FrameLoaded();  // Whether or not a single frame is ready (either good or bad).
	BOOL FrameSuccessfullyLoaded();	// Whether or not a good frame is ready.
	
	BOOL CompletelyLoaded();	// Whether or not the entire image is ready (either good or bad).
	
	void DestroyContext();

	void RemoveListener(CCustomImageObject* pObject);
	void RemoveListenerForSpecificResource(CCustomImageObject *pObject, HT_Resource r);
	void AddListener(CCustomImageObject* pObject, HT_Resource r);
};

class CXIcon : public CDCCX {

public:
	CXIcon(){}
	CXIcon(CRDFImage* image);
	virtual ~CXIcon();

private:
	CPtrList imageList;
	HDC m_hDC;
	NI_Pixmap* m_image;
	NI_Pixmap* m_mask;

	CRDFImage* m_icon;

public:
	virtual HDC GetContextDC() { return m_hDC; }
	void SubstituteDC(HDC hdc) { m_hDC = hdc; }
	virtual BOOL IsDeviceDC() { return TRUE; }
	virtual HDC GetAttribDC() { return m_hDC; }
	virtual BITMAPINFO *NewPixmap(NI_Pixmap *pImage, BOOL mask);
	void DeleteContextDC() { DeleteDC(m_hDC); }
	void ReleaseContextDC(HDC pDC) {}
	virtual void ImageComplete(NI_Pixmap* image);
	//	Don't display partial images.
	virtual void AllConnectionsComplete(MWContext *pContext);
	void NiceDestruction();
	virtual int DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, LTRB& Rect);
};

#endif

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
#include "cxicon.h"
#include "feimage.h"
#include "winproto.h"

HBITMAP NSNavCenterImage::m_hBadImageBitmap = NULL;
int NSNavCenterImage::refCount = 0;

NSNavCenterImage::NSNavCenterImage(char * url, CIconCallbackInfo* iconCallbackInfo)
{
	pUrl = _strdup( url);
	bmpInfo = NULL;
	m_bCompletelyLoaded = FALSE;
	bits = 0;
	maskbits = 0;
	m_BadImage = FALSE;
	resourceList.AddHead(iconCallbackInfo);  // list to store all the resources waiting on this image
	NSNavCenterImage::refCount++;
	iconContext = NULL;
	ProcessIcon();
}

NSNavCenterImage::~NSNavCenterImage()
{
	XP_FREE( bmpInfo);
	CDCCX::HugeFree(bits);
	CDCCX::HugeFree(maskbits);
	free(pUrl);
	
	NSNavCenterImage::refCount--;
	if (refCount == 0)
	{
		if (NSNavCenterImage::m_hBadImageBitmap)
			VERIFY(::DeleteObject(NSNavCenterImage::m_hBadImageBitmap));
	}
}

void NSNavCenterImage::DestroyContext()
{
	iconContext->DeleteContextDC();
	iconContext->DestroyContext(); // now destroy self.
	iconContext = NULL;
}

BOOL NSNavCenterImage::CompletelyLoaded()
{
	if (m_bCompletelyLoaded)
	{
		if (iconContext)
			DestroyContext();
		return TRUE;
	}

	return FALSE;
}

void Icon_GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)  
{
	//	Report any errors.
	if(iStatus < 0 && pUrl->error_msg != NULL)	
	{
		void* pData;
		NSNavCenterImage* theImage = NULL;
		if (CHTFEData::m_CustomURLCache.Lookup(pUrl->address, pData))
			theImage = (NSNavCenterImage*)pData;

		if (theImage) 
		{  // Since we cannot load this url, replace it with a bad image.
			theImage->maskbits = 0;
			theImage->bits = 0;
			theImage->m_BadImage = TRUE;
			if (!NSNavCenterImage::m_hBadImageBitmap)
				NSNavCenterImage::m_hBadImageBitmap = ::LoadBitmap(AfxGetResourceHandle(), MAKEINTRESOURCE(IDB_IMAGE_BAD));
			theImage->CompleteCallback();
		}
	}
}

static BOOL ValidNSBitmapFormat(char* extension)
{
	BOOL val = FALSE;
	CString theFormat = "image/";
	if(!extension)
		return val;
	theFormat += &extension[1];
	if (theFormat.CompareNoCase(IMAGE_GIF) == 0)
		val = TRUE;
	else if (theFormat.CompareNoCase(IMAGE_JPG) == 0)
		val = TRUE;
	else if (theFormat.CompareNoCase(IMAGE_PJPG) == 0)
		val = TRUE;
	else if (theFormat.CompareNoCase(IMAGE_PPM) == 0)
		val = TRUE;
	else if (theFormat.CompareNoCase(IMAGE_PNG) == 0)
		val = TRUE;
	else if (theFormat.CompareNoCase(IMAGE_XBM) == 0)
		val = TRUE;
	else if (theFormat.CompareNoCase(IMAGE_XBM2) == 0)
		val = TRUE;
	else if (theFormat.CompareNoCase(IMAGE_XBM3) == 0)
		val = TRUE;
	return val;
}

void NSNavCenterImage::ProcessIcon() 
{
	char *ext = FE_FindFileExt(pUrl);
	if (ValidNSBitmapFormat(ext)) 
	{
		// If there is no context, create one for processing the image.
		hSubDC = ::CreateCompatibleDC(NULL);

		if (iconContext == NULL)
		{
			iconContext = new CXIcon(this);
			iconContext->SubstituteDC(hSubDC);
			iconContext->Initialize(FALSE, NULL, FALSE);
			iconContext->SetUseDibPalColors(FALSE);
		}

		//  Ask for this via client pull.
		//  We may be in the call stack of the image lib, and doing
		//  lots of fast small get urls causes it to barf due
		//  to list management not being reentrant.
		FEU_ClientPull(iconContext->GetContext(), 0, NET_CreateURLStruct(pUrl, NET_DONT_RELOAD), FO_CACHE_AND_PRESENT, FALSE);
	}
	else 
	{ // handle window internal format BMP
		CString extension = ext;
		if (extension.CompareNoCase(".bmp")) 
		{
		}
		else  
		{
			// TODO: Error handling here, unknow bitmap format.
		}
	
	}
}

void NSNavCenterImage::CompleteCallback()
{
	m_bCompletelyLoaded = TRUE;
	while (!resourceList.IsEmpty())
	{
		CIconCallbackInfo* callback = (CIconCallbackInfo*)(resourceList.RemoveHead());
		callback->pObject->LoadComplete(callback->pResource);
		delete callback;
	}
}

CXIcon::CXIcon(NSNavCenterImage* theImage)
{
    MWContext *pContext = GetContext();
    m_cxType = IconCX;
    pContext->type = MWContextIcon;
    m_MM = MM_TEXT;
	m_hDC = 0;
	m_icon = theImage;
	m_image = NULL;
	m_mask = NULL;
}

CXIcon::~CXIcon()
{
}

BITMAPINFO * CXIcon::NewPixmap(NI_Pixmap *pImage, BOOL isMask)
{
	// remember which bitmap we have so we can get the bits later in imageComplete.
	if (isMask)
		m_mask = pImage;
	else m_image = pImage;
	
	return CDCCX::NewPixmap(pImage, isMask);
}


void CXIcon::ImageComplete(NI_Pixmap* image)
{
	FEBitmapInfo *imageInfo;
	imageInfo = (FEBitmapInfo*) image->client_data;
	BITMAPINFOHEADER* header = (BITMAPINFOHEADER*)imageInfo->bmpInfo;
	if (image == m_image)
	{
		int nColorTable;
		if (GetBitsPerPixel() == 16 || GetBitsPerPixel() == 32)
			nColorTable = 3;
		else if (GetBitsPerPixel() < 16)
			nColorTable = 1 << GetBitsPerPixel();
		else {
			ASSERT(GetBitsPerPixel() == 24);
			nColorTable = 0;
		}

		m_icon->bmpInfo = FillBitmapInfoHeader(image);
		m_icon->bits = HugeAlloc(header->biSizeImage, 1);
		memcpy( m_icon->bits, image->bits, header->biSizeImage );
	}
	else 
	{
		m_icon->maskbits = HugeAlloc(header->biSizeImage, 1);
		memcpy( m_icon->maskbits, image->bits, header->biSizeImage );
	}

	delete imageInfo;
	image->client_data = 0;
	if (m_image && m_mask) 
	{
		if (m_icon->maskbits && m_icon->bits) 
		{
			CDCCX::HugeFree(m_image->bits);
			m_image->bits = 0;
			CDCCX::HugeFree(m_mask->bits);
			m_mask->bits = 0;
			m_icon->CompleteCallback();
		}
	}
	else 
	{
		// We do not have mask, so we don't need to wait for mask.
		CDCCX::HugeFree(m_image->bits);
		m_image->bits = 0;
		m_icon->CompleteCallback();
	}
}
	//	Don't display partial images.
void CXIcon::AllConnectionsComplete(MWContext *pContext)
{
	CDCCX::AllConnectionsComplete(pContext);
}

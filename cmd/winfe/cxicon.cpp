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
#include "helper.h"

HBITMAP NSNavCenterImage::m_hBadImageBitmap = NULL;
int NSNavCenterImage::refCount = 0;

NSNavCenterImage::NSNavCenterImage(const char * url)
{
	m_nRefCount = 0;
	pUrl = _strdup( url);
	bmpInfo = NULL;
	m_bCompletelyLoaded = FALSE;
	bits = 0;
	maskbits = 0;
	m_BadImage = FALSE;
	NSNavCenterImage::refCount++;
	iconContext = NULL;
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

void NSNavCenterImage::AddListener(CCustomImageObject* pObject, HT_Resource r)
{
	// We only want to have one copy of the same image in this list.
	for (POSITION pos = resourceList.GetHeadPosition();
		 pos != NULL; )
	{
		// Enumerate over the list and call remove listener on each image.
		CIconCallbackInfo* pInfo = (CIconCallbackInfo*)resourceList.GetNext(pos);
		if (pInfo->pObject == pObject &&
			pInfo->pResource == r)
			return; // We're already listening for this resource.
	}
	
	// Add a listener.
	CIconCallbackInfo* iconCallbackInfo = new CIconCallbackInfo(pObject, r);
	resourceList.AddHead(iconCallbackInfo);  
	pObject->AddLoadingImage(this);
	m_nRefCount++;
	if (iconContext == NULL)
	{
		// Kick off the load.
		ProcessIcon();
	}
}

void NSNavCenterImage::RemoveListener(CCustomImageObject *pObject)
{
	// Listener has been destroyed.  Need to get all references to it out of the list.
	// Just do this by NULLing out the pObject field of the iconcallbackinfo structs.
	for (POSITION pos = resourceList.GetHeadPosition();
		 pos != NULL; )
	{
		CIconCallbackInfo* pInfo = (CIconCallbackInfo*)resourceList.GetNext(pos);
		if (pInfo->pObject == pObject)
		{	
			pInfo->pObject = NULL;
			m_nRefCount--;
			if (m_nRefCount == 0 && iconContext)
			{
				iconContext->Interrupt();
				DestroyContext();
				return;
			}
		}
	}
}

void NSNavCenterImage::DestroyContext()
{
	iconContext->DeleteContextDC();
	iconContext->NiceDestruction();
	iconContext = NULL;

	resourceList.RemoveAll();
}

BOOL NSNavCenterImage::CompletelyLoaded()
{
	return (m_bCompletelyLoaded);
}

BOOL NSNavCenterImage::SuccessfullyLoaded()
{
	return (m_bCompletelyLoaded && bits);
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

static BOOL IsImageMimeType(const CString& theFormat)
{
	BOOL val = FALSE;
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

static BOOL ValidNSBitmapFormat(char* extension)
{
	CPtrList* allHelpers = &(CHelperApp::m_cplHelpers);

	for (POSITION pos = allHelpers->GetHeadPosition(); pos != NULL;)
	{
		CHelperApp* app = (CHelperApp*)allHelpers->GetNext(pos);
		CString helperMime(app->cd_item->ci.type);

		if (IsImageMimeType(helperMime))
		{
			if (app->cd_item->num_exts > 0)
			{
				for (int i = 0; i < app->cd_item->num_exts; i++)	
				{
					CString extString(app->cd_item->exts[i]);
					if (extString == &extension[1])
						return TRUE;
				}
			}
		}
	}
	return FALSE;
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
		if (callback->pObject)
		{
			callback->pObject->LoadComplete(callback->pResource);
			callback->pObject->RemoveLoadingImage(this);
		}
		delete callback;
	}
	
	DestroyContext();
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

void CXIcon::NiceDestruction()
{
	m_bIdleDestroy = TRUE;
    FEU_RequestIdleProcessing(GetContext());
}

// ========================= CCustomImageObject Helpers =============================

NSNavCenterImage* CCustomImageObject::LookupImage(const char* url, HT_Resource r)
{
	// Find the image.
	void* pData;
	NSNavCenterImage* pImage = NULL;
	if (CHTFEData::m_CustomURLCache.Lookup(url, pData))
	{
		pImage = (NSNavCenterImage*)pData;

		// Add ourselves to the callback list if the image hasn't completely loaded.
		if (!pImage->CompletelyLoaded())
		{
			// The image is currently loading.  Register ourselves with the image so that we will get called
			// when the image finishes loading.
			pImage->AddListener(this, r);
		}
	}
	else
	{
		// Create a new NavCenter image.
		pImage = new NSNavCenterImage(url);
		pImage->AddListener(this, r);
		CHTFEData::m_CustomURLCache.SetAt(url, pImage);
	}

	return pImage;
}

CCustomImageObject::~CCustomImageObject()
{
	// The displayer of the image is being destroyed.  It should be removed as a listener from all 
	// images.
	for (POSITION pos = loadingImagesList.GetHeadPosition();
		 pos != NULL; )
	{
		// Enumerate over the list and call remove listener on each image.
		NSNavCenterImage* pImage = (NSNavCenterImage*)loadingImagesList.GetNext(pos);
		pImage->RemoveListener(this);
	}
}

void CCustomImageObject::AddLoadingImage(NSNavCenterImage* pImage)
{
	// We only want to have one copy of the same image in this list.
	for (POSITION pos = loadingImagesList.GetHeadPosition();
		 pos != NULL; )
	{
		// Enumerate over the list and call remove listener on each image.
		NSNavCenterImage* pNavImage = (NSNavCenterImage*)loadingImagesList.GetNext(pos);
		if (pNavImage == pImage)
			return;
	}
	
	// Add the image to the list.
	loadingImagesList.AddHead(pImage);
}

void CCustomImageObject::RemoveLoadingImage(NSNavCenterImage* pImage)
{
	// Should only occur once in our list.  Find and remove.
	POSITION pos = loadingImagesList.Find(pImage);
	if (pos)
		loadingImagesList.RemoveAt(pos);
}

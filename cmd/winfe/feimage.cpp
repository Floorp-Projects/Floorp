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

#define JMC_INIT_IMGCB_ID	1
#ifndef NSPR20
#include "coremem.h"
#include "stdafx.h"
#else
#include "stdafx.h"
#include "coremem.h"
#endif
#include "feimage.h"
#include "il_types.h"
#include "cxdc.h"

JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(struct IMGCB* self, jint op, void* displayContext, jint width, jint height, IL_Pixmap* image, IL_Pixmap* mask)
{
    MWContext *pContext = (MWContext *)displayContext;
	CAbstractCX  *dispCxt = (CAbstractCX *) pContext->fe.cx;

	FEBitmapInfo *imageInfo;

	imageInfo = new FEBitmapInfo;

	if (!imageInfo) return;
	NI_PixmapHeader* imageHeader = &image->header;  
	imageInfo->targetWidth = CASTINT(width);
	imageInfo->targetHeight = CASTINT(height);
	imageInfo->width = CASTINT(width);
	imageHeader->width = width;

	imageInfo->height = CASTINT(height);
	imageHeader->height = height;
	imageInfo->hBitmap = NULL;
	image->client_data = imageInfo;

	
	if ((imageInfo->bmpInfo  = dispCxt->NewPixmap(image, FALSE)) == NULL) {// error
		delete imageInfo;
		image->client_data = NULL;
		return;
	}
	if (mask) {
		FEBitmapInfo * maskInfo;

		NI_PixmapHeader* maskHeader = &mask->header;  
		maskInfo = new FEBitmapInfo;
		if (!maskInfo) {
			delete imageInfo;
			return;
		}
		maskHeader->width = imageHeader->width;
		maskHeader->height = imageHeader->height;

		mask->client_data = maskInfo;
		maskInfo->hBitmap = NULL;
		if ((maskInfo->bmpInfo = dispCxt->NewPixmap(mask, TRUE)) == NULL) {// error
#ifndef USE_DIB_SECTION
			if (image->bits) {
				CDCCX::HugeFree(image->bits);
				image->bits = NULL;
			}
#endif
			delete imageInfo;
			delete maskInfo;
			mask->client_data = NULL;
			return;
		}

		maskInfo->width = CASTINT(maskHeader->width);
		maskInfo->height = CASTINT(maskHeader->height);
		maskInfo->pContext = dispCxt;	// Not used.
		maskInfo->IsMask = TRUE;
	}

	imageInfo->pContext = dispCxt;		// Not used.
	imageInfo->IsMask = FALSE;
}

JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(struct IMGCB* self, jint op, void* a, IL_Pixmap* b, jint c, jint d, jint e, jint f)
{
}

JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(struct IMGCB* self, jint op, void* displayContext,
						IL_Pixmap* image, IL_PixmapControl c)
{
	if (c == IL_RELEASE_BITS) {
		MWContext *pContext = (MWContext *)displayContext;

		XP_ASSERT(pContext);
		if (!pContext)
			return;

		ABSTRACTCX(pContext)->ImageComplete(image);
	}
}

JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(struct IMGCB* self, jint op, void* displayContext, IL_Pixmap* pixmap)
{
	FEBitmapInfo *imageInfo;

	imageInfo = (FEBitmapInfo*)pixmap->client_data;

	if (imageInfo)
		delete imageInfo;
#ifndef USE_DIB_SECTION
	if (pixmap->bits) {
		CDCCX::HugeFree(pixmap->bits);
		pixmap->bits = NULL;
	}
#endif
	pixmap->client_data = NULL;
}

JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(struct IMGCB* self, jint op, void* displayContext, IL_Pixmap* image, IL_Pixmap* mask, 
					 jint x, jint y, jint x_offset, jint y_offset, jint width, jint height)
{
    MWContext *pContext = (MWContext *)displayContext;
	CDCCX  *dispCxt = (CDCCX *) pContext->fe.cx;
	LTRB Rect;
	dispCxt->DisplayPixmap( image, mask, x, y, x_offset, y_offset, width, height, Rect);
}

JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(struct IMGCB* self, jint op, void* displayContext, jint x, jint y, jint iconNumber)
{
    MWContext *pContext = (MWContext *)displayContext;
	CDCCX  *dispCxt = (CDCCX *) pContext->fe.cx;

	dispCxt->DisplayIcon( x, y, CASTINT(iconNumber));
}

// The width and height are *write-only* we don't care what the original
//   values are.  But, CDCCX::GetIconDimensions() is going to give us
//   x and y as int32's not int's so jump through a few hoops to get the
//   sizes right.
//
extern JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(struct IMGCB* self, jint op, void* displayContext, int* width, int* height, jint iconNumber)
{
    int32 lWidth = 0;
    int32 lHeight = 0;

    MWContext *pContext = (MWContext *)displayContext;
	CDCCX  *dispCxt = (CDCCX *)pContext->fe.cx;
	dispCxt->GetIconDimensions( &lWidth,  &lHeight, CASTINT(iconNumber));

    if(width)   {
        *width = CASTINT(lWidth);
    }
    if(height)  {
        *height = CASTINT(lHeight);
    }
}

JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* self, struct JMCException* *exceptionThrown)
{
}

JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* self, const JMCInterfaceID* iid,
	struct JMCException* *exceptionThrown)
{
	return NULL;
}

void
ImageGroupObserver(XP_Observable observable,
	      XP_ObservableMsg message,
	      void *message_data,
	      void *closure)
{
	MWContext *pXPCX = (MWContext *)closure;

	switch(message) {
	case IL_STARTED_LOADING:
		ABSTRACTCX(pXPCX)->SetImagesLoading(TRUE);
		break;

	case IL_ABORTED_LOADING:
		ABSTRACTCX(pXPCX)->SetImagesDelayed(TRUE);
		break;

	case IL_FINISHED_LOADING:
		ABSTRACTCX(pXPCX)->SetImagesLoading(FALSE);
		break;

	case IL_STARTED_LOOPING:
		ABSTRACTCX(pXPCX)->SetImagesLooping(TRUE);
		break;

	case IL_FINISHED_LOOPING:
		ABSTRACTCX(pXPCX)->SetImagesLooping(FALSE);
		break;

	default:
		break;
	}
	FE_UpdateStopState(pXPCX);
}

void
FE_MochaImageGroupObserver(XP_Observable observable,
	      XP_ObservableMsg message,
	      void *message_data,
	      void *closure)
{
	IL_GroupMessageData *data = (IL_GroupMessageData *)message_data;
	MWContext *pXPCX = (MWContext *)data->display_context;

	// If we are passed a NULL display context, the MWContext has been
	// destroyed.
	if (!pXPCX)
		return;
	
	switch(message) {
	case IL_STARTED_LOADING:
		ABSTRACTCX(pXPCX)->SetMochaImagesLoading(TRUE);
		break;

	case IL_ABORTED_LOADING:
		ABSTRACTCX(pXPCX)->SetMochaImagesDelayed(TRUE);
		break;

	case IL_FINISHED_LOADING:
		ABSTRACTCX(pXPCX)->SetMochaImagesLoading(FALSE);
		break;

	case IL_STARTED_LOOPING:
		ABSTRACTCX(pXPCX)->SetMochaImagesLooping(TRUE);
		break;

	case IL_FINISHED_LOOPING:
		ABSTRACTCX(pXPCX)->SetMochaImagesLooping(FALSE);
		break;

	default:
		break;
	}
	FE_UpdateStopState(pXPCX);
}

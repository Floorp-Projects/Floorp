/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
/* 
   RDFImage.cpp - Aurora Image loading class
   Created: Radha Kulkarni <radha@netscape.com>, 06-29-98
 */

#include "RDFImage.h"
#include "ViewGlue.h"
#include "xfe.h"
#include "il_util.h"

#ifdef DEBUG_radha
#define D(x) x
#else
#define D(x)
#endif

extern "C"
{
void DisplayPixmap(MWContext *, IL_Pixmap *, IL_Pixmap * , jint , jint , jint , jint , jint , jint) ;
void NewPixmap(MWContext *, IL_Pixmap * image, Boolean mask);
void ImageComplete(MWContext *, IL_Pixmap * image);
void fe_load_default_font(MWContext *context);
};


int XFE_RDFImage::refCount = 0;
Pixmap XFE_RDFImage::m_badImage = 0;

XFE_RDFImage::XFE_RDFImage(XFE_Component * frame,  char * imageURL,fe_colormap * cmap, Widget  baseWidget) {

  // Initializations 
  m_urlString = strdup(imageURL);
  completelyLoaded = False;
  //FrameLoaded = False;
  badImage = False;
  pairCount = 0;
  refCount = 0;
  m_imageContext = NULL;
  fec = NULL;
  cxtInitSucceeded = False;
  m_image = NULL;
  m_mask = NULL;
  completeCallback = (completeCallbackPtr) NULL;
  callbackData = (void *) NULL;
  m_frame = frame;
  imageDrawn = False;
  refCount++;

  // Create new context
  m_imageContext = XP_NewContext();
  fec = XP_NEW_ZAP(fe_ContextData);


  if (m_imageContext && fec) 
  {
    m_imageContext->type = MWContextIcon;
    CONTEXT_DATA(m_imageContext) = fec;

    // Set up the image library callbacks
    CONTEXT_DATA(m_imageContext)->DisplayPixmap = (DisplayPixmapPtr)DisplayPixmap;
    CONTEXT_DATA(m_imageContext)->NewPixmap = (NewPixmapPtr)NewPixmap;
    CONTEXT_DATA(m_imageContext)->ImageComplete = (ImageCompletePtr)ImageComplete;

   
    /* Stolen from Frame.cpp. Maybe we s'd make a generic class for
     * Creation and management of MWContexts 
     */
    CONTEXT_DATA (m_imageContext)->colormap = cmap;
    CONTEXT_WIDGET (m_imageContext) = baseWidget; 
    CONTEXT_DATA(m_imageContext)->drawing_area = baseWidget;

    m_imageContext->funcs = fe_BuildDisplayFunctionTable();
	m_imageContext->convertPixX = m_imageContext->convertPixY = 1;
	m_imageContext->is_grid_cell = FALSE;
	m_imageContext->grid_parent = NULL;

    XP_AddContextToList(m_imageContext);
    fe_InitIconColors(m_imageContext);

	// Use colors from prefs

	LO_Color *color;

	color = &fe_globalPrefs.links_color;
	CONTEXT_DATA(m_imageContext)->link_pixel = 
		fe_GetPixel(m_imageContext, color->red, color->green, color->blue);

	color = &fe_globalPrefs.vlinks_color;
	CONTEXT_DATA(m_imageContext)->vlink_pixel = 
		fe_GetPixel(m_imageContext, color->red, color->green, color->blue);

	color = &fe_globalPrefs.text_color;
	CONTEXT_DATA(m_imageContext)->default_fg_pixel = 
		fe_GetPixel(m_imageContext, color->red, color->green, color->blue);

	color = &fe_globalPrefs.background_color;
	CONTEXT_DATA(m_imageContext)->default_bg_pixel = 
		fe_GetPixel(m_imageContext, color->red, color->green, color->blue);


        SHIST_InitSession (m_imageContext);
	
        fe_load_default_font(m_imageContext);
	{
		Pixel unused_select_pixel;
		XmGetColors (XtScreen (baseWidget),
					 fe_cmap(m_imageContext),
					 CONTEXT_DATA (m_imageContext)->default_bg_pixel,
					 &(CONTEXT_DATA (m_imageContext)->fg_pixel),
					 &(CONTEXT_DATA (m_imageContext)->top_shadow_pixel),
					 &(CONTEXT_DATA (m_imageContext)->bottom_shadow_pixel),
					 &unused_select_pixel);
	}


    /* Add mapping between MWContext and frame */
    ViewGlue_addMapping((XFE_Frame *)m_frame, m_imageContext);


    /* Initialize the Imagelib callbacks, Imagecontexts, 
     * imageobserver and group contexts
     */
    if (!fe_init_image_callbacks(m_imageContext))
    {
       cxtInitSucceeded = False;
       return;
    }

	
    fe_InitColormap (m_imageContext);
    cxtInitSucceeded = True;
  }
  else
  {
    cxtInitSucceeded = False;
    return;
  }

}


XFE_RDFImage::~XFE_RDFImage()
{

  free(m_urlString);
  free (CONTEXT_DATA (m_imageContext));
  free (m_imageContext);
  refCount--;
 
  // Delete the bad image if refcount = 0
  if (refCount == 0)
  {
       D(printf("Need to delete the bad image\n");)

  }

  // Destroy the Image groupcontexts and observers
  if (m_imageContext)
  {
      PRBool observer_removed_p;

      if (m_imageContext->color_space) 
      {
         D(printf("Deleting image color spaces\n");)
	     IL_ReleaseColorSpace(m_imageContext->color_space);
          m_imageContext->color_space = NULL;
      }

      /* Destroy the image group context after removing the image group 
       * observer
       */
           D(printf("Deleting the image observer\n");)
            observer_removed_p =
                IL_RemoveGroupObserver(m_imageContext->img_cx,
                                       fe_ImageGroupObserver,
                                       (void *)m_imageContext);

			IL_DestroyGroupContext(m_imageContext->img_cx);
			m_imageContext->img_cx = NULL;
		}
}

void
XFE_RDFImage::setCompleteCallback(completeCallbackPtr callback,  void * client_data)
{
     completeCallback  = callback;
     callbackData = client_data;

}


void 
XFE_RDFImage::RDFDisplayPixmap(IL_Pixmap * image, IL_Pixmap * mask, jint width, jint height)
{
	m_image = image;
	m_mask = mask;
    pairCount++;

    if (!imageDrawn) 
    {   
       callbackClientData * client_data = XP_NEW_ZAP(callbackClientData);
       client_data->widget = (Widget)callbackData;
       client_data->image = getPixmap();
       client_data->mask = getMask();
       client_data->width = width;
       client_data->height = height;
       (*completeCallback)(client_data);
       imageDrawn = True;
    }
	return;
}


void
XFE_RDFImage::RDFNewPixmap(IL_Pixmap * image, Boolean isMask)
{
	if (isMask)
		m_mask = image;
	else m_image = image;

}

void
XFE_RDFImage::RDFImageComplete(IL_Pixmap * image)
{

#ifdef UNDEF
	// Will get a call for both the mask and for the image.  
	if (m_image && m_mask)
	{
		if (pairCount == 2)
			(*completeCallback)(getPixmap(), getMask(), callbackData);
		else 
          pairCount++;
	}
	else if (m_image)
	{
		// We have no mask.
		(*completeCallback)(getPixmap(), getMask(), callbackData);
	}
#endif  /* UNDEF  */

}


Boolean 
XFE_RDFImage::isImageLoaded(void)
{
     return (completelyLoaded);

}

extern "C"
{
void
Icon_GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext)  
{
	//	Report any errors.
	if(iStatus < 0 && pUrl->error_msg != NULL)	
	{
         printf("Couldn't load image. Need to put the bad image up\n");
#ifdef UNDEF
	     // Since we cannot load this url, replace it with a bad image.
     	     theImage->m_BadImage = TRUE;
			theImage->bits = 0;
			theImage->maskbits = 0;
			theImage->bmpInfo = 0;
			if (!CRDFImage::m_hBadImageBitmap)
				CRDFImage::m_hBadImageBitmap = ::LoadBitmap(AfxGetResourceHandle(), MAKEINTRESOURCE(IDB_IMAGE_BAD));
			theImage->CompleteCallback();
#endif    /* UNDEF   */
	}
}
};  /* extern C */


void 
XFE_RDFImage::loadImage(void)
{

   if (cxtInitSucceeded)
     NET_GetURL(NET_CreateURLStruct(m_urlString, NET_DONT_RELOAD), FO_CACHE_AND_PRESENT,   m_imageContext, Icon_GetUrlExitRoutine);

}

Pixmap XFE_RDFImage::getPixmap(void)
{
   if (m_image)
     return (Pixmap)(((fe_PixmapClientData *)(m_image->client_data))->pixmap);
   return (Pixmap)NULL;
}

Pixmap XFE_RDFImage::getMask(void)
{
    if (m_mask)
       return (Pixmap)(((fe_PixmapClientData *)(m_mask->client_data))->pixmap);
    return (Pixmap)NULL;

}






#ifdef UNDEF

CRDFImage::~CRDFImage()
{
//	XP_FREE( bmpInfo);
//	CDCCX::HugeFree(bits);
//	CDCCX::HugeFree(maskbits);
	free(pUrl);
	
	CRDFImage::refCount--;
	if (refCount == 0)
	{
		if (CRDFImage::m_hBadImageBitmap)
			VERIFY(::DeleteObject(CRDFImage::m_hBadImageBitmap));
	}
}

void CRDFImage::AddListener(CCustomImageObject* pObject, HT_Resource r)
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

void CRDFImage::RemoveListener(CCustomImageObject *pObject)
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

void CRDFImage::RemoveListenerForSpecificResource(CCustomImageObject *pObject, HT_Resource r)
{
	// Listener has been destroyed.  Need to get all references to it out of the list.
	// Just do this by NULLing out the pObject field of the iconcallbackinfo structs.
	for (POSITION pos = resourceList.GetHeadPosition();
		 pos != NULL; )
	{
		CIconCallbackInfo* pInfo = (CIconCallbackInfo*)resourceList.GetNext(pos);
		if (pInfo->pObject == pObject && pInfo->pResource == r)
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


void CRDFImage::DestroyContext()
{
	iconContext->DeleteContextDC();
	iconContext->NiceDestruction();
	iconContext = NULL;

	resourceList.RemoveAll();
}

BOOL CRDFImage::FrameLoaded()
{
	return (m_bFrameLoaded);
}

BOOL CRDFImage::FrameSuccessfullyLoaded()
{
	return (m_bFrameLoaded && bits);
}

BOOL CRDFImage::CompletelyLoaded()
{
	return m_bCompletelyLoaded;
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

void CRDFImage::ProcessIcon() 
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

// Temporary hack to disable loading RDF images for the layout integration
// build.
#ifndef MOZ_NGLAYOUT
		//  Ask for this via client pull.
		//  We may be in the call stack of the image lib, and doing
		//  lots of fast small get urls causes it to barf due
		//  to list management not being reentrant.
		FEU_ClientPull(iconContext->GetContext(), 0, NET_CreateURLStruct(pUrl, NET_DONT_RELOAD), FO_CACHE_AND_PRESENT, FALSE);
#endif
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

void CRDFImage::CompleteCallback()
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

void CRDFImage::CompleteFrameCallback()
{
	m_bFrameLoaded = TRUE;
	for (POSITION pos = resourceList.GetHeadPosition(); pos != NULL; )
	{
		CIconCallbackInfo* callback = (CIconCallbackInfo*)(resourceList.GetNext(pos));
		if (callback->pObject)
		{
			callback->pObject->LoadComplete(callback->pResource);
		}
	}
}

CXIcon::CXIcon(CRDFImage* theImage)
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

int CXIcon::DisplayPixmap(NI_Pixmap* image, NI_Pixmap* mask, int32 x, int32 y, int32 x_offset, int32 y_offset, int32 width, int32 height, int32 lScaleWidth, int32 lScaleHeight, LTRB& Rect)
{
	m_image = image;
	m_mask = mask;

	m_icon->bmpInfo = FillBitmapInfoHeader(image);
	m_icon->bits = image->bits;
	if (mask)
		m_icon->maskbits = mask->bits;

	m_icon->CompleteFrameCallback();
	return 1;
}

void CXIcon::ImageComplete(NI_Pixmap* image)
{
	// Will get a call for both the mask and for the image.  
	if (m_image && m_mask)
	{
		if (m_icon->pairCount == 2)
			m_icon->CompleteCallback();
		else m_icon->pairCount++;
	}
	else
	{
		// We have no mask.
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

CRDFImage* CCustomImageObject::LookupImage(const char* url, HT_Resource r)
{
	// Find the image.
	void* pData;
	CRDFImage* pImage = NULL;
	if (CHTFEData::m_CustomURLCache.Lookup(url, pData))
	{
		pImage = (CRDFImage*)pData;

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
		pImage = new CRDFImage(url);
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
		CRDFImage* pImage = (CRDFImage*)loadingImagesList.GetNext(pos);
		pImage->RemoveListener(this);
	}
}

void CCustomImageObject::AddLoadingImage(CRDFImage* pImage)
{
	// We only want to have one copy of the same image in this list.
	for (POSITION pos = loadingImagesList.GetHeadPosition();
		 pos != NULL; )
	{
		// Enumerate over the list and call remove listener on each image.
		CRDFImage* pNavImage = (CRDFImage*)loadingImagesList.GetNext(pos);
		if (pNavImage == pImage)
			return;
	}
	
	// Add the image to the list.
	loadingImagesList.AddHead(pImage);
}

void CCustomImageObject::RemoveLoadingImage(CRDFImage* pImage)
{
	// Should only occur once in our list.  Find and remove.
	POSITION pos = loadingImagesList.Find(pImage);
	if (pos)
		loadingImagesList.RemoveAt(pos);
}



#endif

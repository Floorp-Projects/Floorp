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

#define	JMC_INIT_IMGCB_ID

	// Netscape
// #include "mdmacmem.h"
#include "client.h"
#include "xp_mcom.h"
#include "merrors.h"
#include "xpassert.h"
#include "mimages.h"
#include "miconutils.h"
#include "il_util.h"
#include "il_icons.h"
#include "CBrowserContext.h"
#include "CHTMLView.h"
#include "RandomFrontEndCrap.h"
#include <UDrawingState.h>
#include "UTextBox.h"
#include "uerrmgr.h"
#include "resgui.h"
#ifndef NSPR20
#include "prglobal.h"
#endif
#include "MacMemAllocator.h"
#include "xlate.h"

#ifdef PROFILE
#pragma profile on
#endif


#define	TRACK_IMAGE_CACHE_SIZE		0

#pragma mark --- TYPES ---

/*
 * We store color spaces for the following bit depths:
 * 1, 2, 4, 8, 2gray, 4gray, 8gray, 16, 32.
 * Grayscale spaces are lower than colour spaces so that we can do a numerical compare
 * on indices.
 */

enum ColorSpaceIndex {
	kFirstColorSpace = 0,
	kOneBit = 0,
	kEightBitGray,
	kEightBitColor,
	kSixteenBit,
	kThirtytwoBit,
	kNumColorSpaces
};

enum DefaultColors {
	kDefaultBGColorRed = 192,
	kDefaultBGColorGreen = 192,
	kDefaultBGColorBlue = 192
};

/*
 * Our internal Pixmap structure. What's the policy for internal priv. structs as to
 * header files, etc?
 */

typedef struct NS_PixMap
{
	PixMap		pixmap;
	Handle		buffer_handle;
	void *		image_buffer;
	int32		lock_count;
	Boolean		tiled;
} NS_PixMap;


typedef struct DrawingState
{
	short		copyMode;
	NS_PixMap *	pixmap;
	NS_PixMap *	mask;
} DrawingState;


typedef struct PictureGWorldState {
	GWorldPtr	gworld;
	RGBColor	transparentColor;
	Int32		transparentIndex;
	CTabHandle	ctab;
	Int32		imageHeight;
	Int32		bandHeight;
	NS_PixMap *	image;
	NS_PixMap *	mask;
} PictureGWorldState;

/* BRAIN DAMAGE: This scary thing came from the old code */
#define IL_ICON_OFFSET 300

/*
 * Internal function prototypes
 */

static OSErr AllocatePixMap ( IL_Pixmap * il_pixmap, jint width, jint height,
	NS_PixMap * fe_pixmap, CTabHandle ctab );

static ColorSpaceIndex GetNextBestColorSpaceIndex ( ColorSpaceIndex index );
static ColorSpaceIndex ConvertColorSpaceToIndex ( IL_ColorSpace * color_space );
static ColorSpaceIndex ConvertDepthToColorSpaceIndex ( Int32 depth, Boolean grayScale );
static IL_ColorSpace * GetColorSpace ( MWContext * context, ColorSpaceIndex space_index, CTabHandle * color_table );
static CTabHandle ConvertColorSpaceToColorTable ( IL_ColorSpace * color_space );
static void SetColorSpaceTransparentColor ( IL_ColorSpace * color_space, Uint8 red, Uint8 green, Uint8 blue );

static OSErr CreatePictureGWorld ( IL_Pixmap * image, IL_Pixmap * mask, PictureGWorldState * state );
static void TeardownPictureGWorld ( PictureGWorldState * state );
static void CopyPicture ( PictureGWorldState * state );
static void CreateUniqueTransparentColor ( IL_Pixmap * image, PictureGWorldState * state );
static Boolean FindColorInCTable ( CTabHandle ctab, Uint32 skipIndex, RGBColor * rgb );

static void LockPixmapBuffer ( IL_Pixmap * pixmap );
static void UnlockPixmapBuffer ( IL_Pixmap * pixmap );

static void DrawScaledImage ( DrawingState * state, Point topLeft, jint x_offset,
	jint y_offset, jint width, jint height );
static void DrawTiledImage ( DrawingState * state, Point topLeft, jint x_offset,
	jint y_offset, jint width, jint height );

static CIconHandle GetIconHandle ( jint iconID );
static IL_GroupContext * CreateImageGroupContext ( MWContext * context );

static OSErr PreparePixmapForDrawing ( IL_Pixmap * image, IL_Pixmap * mask, Boolean canCopyMask,
	DrawingState * state );
static void DoneDrawingPixmap ( IL_Pixmap * image, IL_Pixmap * mask, DrawingState * state );

/*
 * Globals
 */

IL_ColorSpace *	gColorSpaces[ kNumColorSpaces ] = { NULL, NULL, NULL, NULL, NULL };
CTabHandle		gColorTables[ kNumColorSpaces ] = { NULL, NULL, NULL, NULL, NULL };
CTabHandle		gOneBitTable = NULL;

#if TRACK_IMAGE_CACHE_SIZE
static Uint32	gCacheSize = 0;
static Uint32	gMaxCacheSize = 0;
#endif

#pragma mark --- IMAGE LIB CALLBACKS ---

/*
 * Callback procs for the ImageLib
 */

JMC_PUBLIC_API(void*)
_IMGCB_getBackwardCompatibleInterface(struct IMGCB* /*self*/, const JMCInterfaceID* /*iid*/,
	JMCException* */*exception*/)
{
	return NULL;
}

JMC_PUBLIC_API(void)
_IMGCB_init(struct IMGCB* /*self*/, JMCException* */*exception*/)
{

}

JMC_PUBLIC_API(void)
_IMGCB_NewPixmap(struct IMGCB* /*self*/, jint /*op*/, void* a, jint width, jint height, IL_Pixmap* pixmap,
	IL_Pixmap* mask)
{
	MWContext *		context = (MWContext *) a;
	NS_PixMap *		fe_pixmap;
	NS_PixMap *		fe_mask;
	OSErr			err;
	ColorSpaceIndex	space_index;
	ColorSpaceIndex	image_index;
	CTabHandle		ctab;
	IL_ColorSpace *	color_space;
	FreeMemoryStats	freeMemory;
	Uint32			freeTempMemory;
	
	XP_ASSERT(pixmap != NULL);

	ctab = NULL;
	
	/* BRAIN DAMAGE - for now let the image lib do any scaling */
	pixmap->header.width = width;
	pixmap->header.height = height;
	
	fe_pixmap = XP_NEW(NS_PixMap);
	XP_ASSERT(fe_pixmap != NULL);
	if ( fe_pixmap != NULL )
		{
		fe_pixmap->lock_count = 0;
		fe_pixmap->image_buffer = 0L;
		fe_pixmap->buffer_handle = 0L;
		fe_pixmap->tiled = false;
		
		color_space = pixmap->header.color_space;
		
		/*
		 * if this pixmap's color space is of lower resolution than our current
		 * screen depth, then use the images, otherwise use the screen depth.
		 */
		
		space_index = ConvertDepthToColorSpaceIndex( 0, false );
		
		if ( color_space != NULL )
			{
			image_index = ConvertColorSpaceToIndex ( color_space );
			
			if ( image_index < space_index )
				{
				/*
				 * This image appears to be at a lower colour depth, but it may have a 
				 * unique color table, so we may want to allocate it at a deeper depth.
				 * So, if we're getting short on memory, decrease the depth, otherwise
				 * use the default depth.
				 *
				 * We don't really base this on the size of the image as we ideally want to
				 * start decreasing the size of images, before we run out of memory (which
				 * other parts of the browser may not handle as well).
				 */
				memtotal ( 1024, &freeMemory );
				freeTempMemory = TempFreeMem();
				
				if (( freeMemory.totalFreeBytes < 0x10000 ) || ( freeTempMemory < 0x40000 ))
					{
					space_index = image_index;
					}
				}
			
			/* we don't want this color space anymore */
			IL_ReleaseColorSpace ( color_space );
			pixmap->header.color_space = NULL;
			color_space = NULL;
			}
		
		/*
		 * Try to allocate the pixmap at the ideal colour depth. If we can't, then keep
		 * downgrading the colour resolution until we can.
		 */
		do
			{
			color_space = GetColorSpace ( context, space_index, &ctab );
			
			if ( color_space != NULL && ctab != NULL )
				{
				pixmap->header.color_space = color_space;
				err = AllocatePixMap ( pixmap, width, height, fe_pixmap, ctab );
				if ( err == noErr )
					{
					break;
					}
				}
			
			space_index = GetNextBestColorSpaceIndex ( space_index );
			}
		while ( space_index < kNumColorSpaces );
		
		if ( err == noErr )
			{
			/* add a ref to this color space */
			IL_AddRefToColorSpace ( color_space );
			XP_ASSERT(pixmap->header.color_space != NULL);
			}
		else
			{
			pixmap->header.color_space = NULL;
			XP_DELETE(fe_pixmap);
			fe_pixmap = NULL;
			}
		}
	
	pixmap->client_data = fe_pixmap;
	
	if ( mask != NULL )
		{
		/* BRAIN DAMAGE - for now let the image lib do any scaling */
		mask->header.width = width;
		mask->header.height = height;
		
		fe_mask = XP_NEW(NS_PixMap);
		XP_ASSERT(fe_mask != NULL);
		if ( fe_mask != NULL )
			{
			fe_mask->lock_count = 0;
			fe_mask->image_buffer = NULL;
			fe_mask->buffer_handle = NULL;
			fe_mask->tiled = false;
			
			err = AllocatePixMap ( mask, width, height, fe_mask, gOneBitTable );
			if ( err == noErr )
				{
				mask->client_data = fe_mask;
				}
			else
				{
				XP_DELETE(fe_mask);
				}
			}
		}
	
	/*
	 * If the image has a transparent color, make sure to update it from the current context
	 */
	if ( pixmap->header.transparent_pixel != NULL )
		{
		*pixmap->header.transparent_pixel = *context->transparent_pixel;
		}
		
	XP_ASSERT(pixmap->header.color_space->pixmap_depth != 0 );
}

JMC_PUBLIC_API(void)
_IMGCB_UpdatePixmap(struct IMGCB* /*self*/, jint /*op*/, void* /*a*/, IL_Pixmap* /*pixmap*/, jint /*x_offset*/,
	jint /*y_offset*/, jint /*width*/, jint /*height*/)
{
}

JMC_PUBLIC_API(void)
_IMGCB_ControlPixmapBits(struct IMGCB* /*self*/, jint /*op*/, void* a, IL_Pixmap* pixmap, IL_PixmapControl message)
{
	MWContext *	context = (MWContext *) a;
	NS_PixMap *	fe_pixmap = NULL;
	
	XP_ASSERT(pixmap != NULL);
	
	fe_pixmap = (NS_PixMap *) pixmap->client_data;
	XP_ASSERT(fe_pixmap != NULL);

	if ( fe_pixmap != NULL )
	{
		switch ( message )
		{
			case IL_LOCK_BITS:
				LockPixmapBuffer ( pixmap );
				break;
			
			case IL_UNLOCK_BITS:
				UnlockPixmapBuffer ( pixmap );
				break;
		}
	}
}

JMC_PUBLIC_API(void)
_IMGCB_DestroyPixmap(struct IMGCB* /*self*/, jint /*op*/, void* a, IL_Pixmap* pixmap)
{
	MWContext *	context = (MWContext *) a;
	NS_PixMap *	fe_pixmap = NULL;

	XP_ASSERT(pixmap != NULL);
	fe_pixmap = (NS_PixMap *) pixmap->client_data;

	if ( fe_pixmap != NULL )
		{
#if TRACK_IMAGE_CACHE_SIZE
		gCacheSize -= pixmap->header.widthBytes * pixmap->header.height;
#endif
		
		if ( fe_pixmap->buffer_handle != NULL )
			{
			DisposeHandle ( fe_pixmap->buffer_handle );
			}
		
		if ( fe_pixmap->image_buffer != NULL )
			{
			free ( fe_pixmap->image_buffer );
			}
		
		XP_DELETE ( fe_pixmap );
		}
}

JMC_PUBLIC_API(void)
_IMGCB_DisplayPixmap(struct IMGCB* /*self*/, jint /*op*/, void* a, IL_Pixmap* image, IL_Pixmap* mask,
	jint x, jint y, jint x_offset, jint y_offset, jint width, jint height)
{
	MWContext *		context = (MWContext *) a;
	NS_PixMap *		fe_pixmap;
	NS_PixMap *		fe_mask;
	SPoint32		topLeftImage;
	StColorState	saveColorState();
	Point			topLeft;
	int32			x_origin;
	int32			y_origin;
	Boolean			canCopyMask;
	OSErr			err;
	DrawingState	state;
	
	CHTMLView* theHTMLView = ExtractHyperView(context);
	if ( image != NULL && theHTMLView && theHTMLView->FocusDraw())
		{
		StColorPenState::Normalize();
		
		theHTMLView->GetLayerOrigin ( &x_origin, &y_origin );

		/*
		 * Can we use copymask for transparency? QuickDraw can't save Masks to Pictures nor
		 * can we print them.
		 */
		canCopyMask = ( context->type != MWContextPrint ) && ( qd.thePort->picSave == NULL );
		
		fe_pixmap = (NS_PixMap *) image->client_data;
		fe_mask = mask != NULL ? (NS_PixMap *) mask->client_data : NULL;
		
		if ( fe_pixmap != NULL )
			{			
			topLeftImage.h = x + x_offset + x_origin;
			topLeftImage.v = y + y_offset + y_origin;

			theHTMLView->ImageToLocalPoint( topLeftImage, topLeft );
			
			err = PreparePixmapForDrawing ( image, mask, canCopyMask, &state );
			if ( err == noErr )
				{
				if ( fe_pixmap->tiled != false )
					{
					DrawTiledImage ( &state, topLeft, x_offset, y_offset, width, height );
					}
				else
					{
					DrawScaledImage ( &state, topLeft, x_offset, y_offset, width, height );
					}
				
				DoneDrawingPixmap ( image, mask, &state );
				}
			}
		}
}

JMC_PUBLIC_API(void)
_IMGCB_DisplayIcon(struct IMGCB* /*self*/, jint /*op*/, void* a, jint x, jint y, jint icon_number)
{
	MWContext *	context = (MWContext *) a;
	Rect		icon_dest;
	CIconHandle	ic;
	PixMap *	pixmap;
	int32		x_origin;
	int32		y_origin;
	SPoint32	topLeftImage;
	Point		topLeft;
	
	CHTMLView* theHTMLView = ExtractHyperView(context);
	if (theHTMLView == NULL)
		return;

	ic = GetIconHandle ( icon_number );
	if ( ic != NULL )
	{
		pixmap = &(*ic)->iconPMap;
		
		// Convert from layer-relative coordinates to local coordinates
		theHTMLView->GetLayerOrigin ( &x_origin, &y_origin );
		topLeftImage.h = x + x_origin;
		topLeftImage.v = y + y_origin;
		theHTMLView->ImageToLocalPoint( topLeftImage, topLeft );
	
		icon_dest.left = topLeft.h;
		icon_dest.top = topLeft.v;
		icon_dest.right = icon_dest.left + ( pixmap->bounds.right - pixmap->bounds.left );
		icon_dest.bottom = icon_dest.top + ( pixmap->bounds.bottom - pixmap->bounds.top );
		
		::PlotCIconHandle( &icon_dest, atAbsoluteCenter, ttNone, ic );
		CIconList::ReturnIcon ( ic );
	}
}

JMC_PUBLIC_API(void)
_IMGCB_GetIconDimensions(struct IMGCB* /*self*/, jint /*op*/, void* a, int* width, int* height, jint icon_number)
{
	MWContext *	context = (MWContext *) a;
	CIconHandle	ic;
	PixMap *	pixmap;
	
	ic = GetIconHandle ( icon_number );
	if ( ic == NULL )
	{
		*width = 16;
		*height = 16;
	}
	else
	{
		pixmap = &(*ic)->iconPMap;
		*width = pixmap->bounds.right - pixmap->bounds.left;
		*height = pixmap->bounds.bottom - pixmap->bounds.top;

		CIconList::ReturnIcon ( ic );
	}
}

#pragma mark --- PUBLIC FUNCTIONS ---

EClickKind FindImagePart (
					MWContext *			context,
					LO_ImageStruct *	image,
					SPoint32 *			where,
					cstring *			url,
					cstring *			target,
					LO_AnchorData *	&	anchor )
{
	SPoint32			local_point;
	EClickKind			click;
	int					iconWidth;
	int					iconHeight;
	
	click = eNone;
	
	/*
	 * Convert the document coordinate to an image local coordinate
	 */
	local_point.h = where->h - ( image->x + image->x_offset + image->border_width );
	local_point.v = where->v - ( image->y + image->y_offset + image->border_width );

	if (image->anchor_href)
		{
		PA_LOCK(*url, char*, (char*)image->anchor_href->anchor);
		PA_UNLOCK(loImage->anchor_href->anchor );
		PA_LOCK(*target, char*, (char*)image->anchor_href->target);
		PA_UNLOCK(loImage->anchor_href->target );
		}
	
	if ( image->image_attr->attrmask & LO_ATTR_ISFORM )
	{
		char s[100];

		// ¥ form image
		click = eImageForm;
		char * printString = GetCString(IMAGE_SUBMIT_FORM);
		sprintf (s, (char*)printString, local_point.h, local_point.v); // "Submit form:%d,%d"
		*url = s;
	}
	else if ( image->image_attr->usemap_name != NULL )
	{
		// ¥ client-side image maps
		anchor = LO_MapXYToAreaAnchor( context, image, local_point.h, local_point.v );
		if ( anchor )
		{
			PA_LOCK( *url, char*, (char*)anchor->anchor );
			PA_UNLOCK( anchor->anchor );
			PA_LOCK( *target, char*, (char*)anchor->target );
			PA_UNLOCK( anchor->target );
			click = eImageAnchor;
		}
		else
			click = eNone;
	}
	else if ( (image->image_attr->attrmask & LO_ATTR_ISMAP) && image->anchor_href )
	{
		// ¥ÊISMAP
		click = eImageIsmap;
		char s[50];
		url->truncAt( '?' );
		url->truncAt( '#' );
		sprintf( s, "?%d,%d", local_point.h, local_point.v );
		*url += s;
	}
	else if ( IsImageComplete ( image ) == FALSE )
	{
		// Did we click in an icon or an alt text?
		// BRAIN DAMAGE
#ifdef OLD_IMAGE_LIB
		// ¥ delayed image
		if (((IconProxy*)imageProxy)->ClickInIcon(mImageWhere.h, mImageWhere.v))
			theKind = eImageIcon;
		else if ( ((IconProxy*)imageProxy)->ClickInAltText(mImageWhere.h, mImageWhere.v) && loImage->anchor_href)
			theKind = eImageAltText;
		else
			theKind = eNone;

#endif
		/* is the image an icon? */
		if ( image->is_icon )
			{
			IL_GetIconDimensions ( context->img_cx, image->icon_number, &iconWidth, &iconHeight );
			
			/*
			 * If the image has no anchor or the click is inside the icon, mark the click as being
			 * the icon
			 */
			if ( ( ( local_point.h < iconWidth ) && ( local_point.v < iconHeight ) )
				 || ( image->anchor_href == NULL ) )
				{
				click = eImageIcon;
				}
			else
				{
				click = eImageAltText;
				}
			}
	}
	else
	{
		// ¥ plain image with an anchor
		if ( image->anchor_href )
		{
			click = eImageAnchor;
		}
	}
	
	return click;
}

BOOL IsImageComplete (
			LO_ImageStruct *	image )
{
	return ((image->image_status == IL_IMAGE_COMPLETE) || 
        (image->image_status == IL_FRAME_COMPLETE));
}

// a hack to get the URL of the image
// if the image is using that reconnect hack, we get the URL of the document
// instead of the URL of the image
cstring GetURLFromImageElement(
				CBrowserContext *	inOwningContext,
				LO_ImageStruct *	inElement)
{
	cstring retString;
	cstring urlString = (char*)inElement->image_url;

	// ¥Êhandle anything starting with "internal-"
	if (IsInternalTypeLink(urlString))	
		{
		// ¥ handle special mail & news reconnects for internal decoded images
		//		that start "internal-external-reconnect:" followed by the original
		//		URL
		if (IsMailNewsReconnect(urlString))
			retString = &urlString[XP_STRLEN(reconnectHack) + 1];
		// ¥ handle "internal-external-reconnect" case when we don't have an
		//		associated anchor_href
		else if (!inElement->anchor_href && IsInternalImage(urlString))
			{
			retString = inOwningContext->GetCurrentURL();
			}
		else
		// ¥ jwz's hack upon a hack!
		//	That is, if there is both an image and an anchor, and the address of
		//	the image is internal-external-reconnect, use the anchor for the image,
		//	and pretend there was no anchor.

		//	A side effect of the way I did this is that internal-external-reconnect
		//	images which have HREF (mail and news articles) are clickable links to
		//	themselves; we could special case this, but it doesn't hurt anything,
		//	and could even be mistaken for intentional, so who cares ( JWZ wrote
		//	that origially and I don't think it applies to the actual logic here - tgm ).
			{
			retString = (char*)inElement->anchor_href->anchor;
			}
		}
	else
		retString = urlString;
		
	return retString;
}

PicHandle ConvertImageElementToPICT(
				LO_ImageStruct *	inElement)
{
	PicHandle			pic;
	OpenCPicParams		picParams;
	IL_Pixmap *			image;
	IL_Pixmap *			mask;
	DrawingState		state;
	OSErr				err;
	NS_PixMap *			fe_pixmap;
	Rect				dstRect;
	PictureGWorldState	picState;
	GrafPtr				oldPort;
	CGrafPort			cport;
	
	err = noErr;
	
	/*
	 * We render into our own port so that we don't need to play with other origin/port setting
	 * fun
	 */
	GetPort ( &oldPort );
	OpenCPort ( &cport );
	
	if ( QDError() != noErr )
		return NULL;
		
	SetPort ( (GrafPtr) &cport );
	
	SetRect ( &picParams.srcRect, 0, 0, inElement->width, inElement->height );
	picParams.hRes = 72L << 16;
	picParams.vRes = 72L << 16;
	picParams.version = 0;
	picParams.reserved1 = 0;
	picParams.reserved2 = 0;
	
	image = IL_GetImagePixmap ( inElement->image_req );
	mask = IL_GetMaskPixmap ( inElement->image_req );
	
	if ( image == NULL )
		{
		return NULL;
		}
		
	pic = OpenCPicture ( &picParams );
	if ( pic != NULL )
		{
		/* if we have no mask, then we're just dandy */
		if ( mask == NULL )
			{
			err = PreparePixmapForDrawing ( image, mask, false, &state );
			if ( err == noErr )
				{
				fe_pixmap = state.pixmap;
				SetRect ( &dstRect, 0, 0, inElement->width, inElement->height );
				
				CopyBits ( (BitMap *) &fe_pixmap->pixmap, &qd.thePort->portBits,
					&fe_pixmap->pixmap.bounds, &dstRect, srcCopy, NULL );
				
				DoneDrawingPixmap ( image, mask, &state );
				}
			}
		else
			{
			/*
			 * We have transparency, so we suck. What we basically do is allocate an offscreen,
			 * render a unique color into the background, copymask the image over it and then
			 * copy that image into a picture.
			 *
			 * This particularly sucks for grayscale images as we need to make sure that our
			 * transparent color is unique.
			 *
			 * Thus, for indexed images, we create a color table that contains our reserved
			 * entry as a reserved index. We then CopyBits the original image onto it. Then,
			 * we set our reserved entry to a magic value and do a final CopyBits with transparent
			 * mode into the picture.
			 */
			
			err = CreatePictureGWorld ( image, mask, &picState );
			if ( err == noErr )
				{
				LockPixmapBuffer ( image );
				LockPixmapBuffer ( mask );
				
				CopyPicture ( &picState );

				UnlockPixmapBuffer ( image );
				UnlockPixmapBuffer ( mask );

				TeardownPictureGWorld ( &picState );
				}
			}
				
		ClosePicture();
		}
	
	SetPort ( oldPort );
	CloseCPort ( &cport );
	
	/*
	 * If we got an error, don't return a bad picture
	 */
	if ( err != noErr )
		{
		KillPicture ( pic );
		pic = NULL;
		}
		
	return pic;
}

void CreateImageContext (
			MWContext *			context )
{
	IL_DisplayData	data;
	ColorSpaceIndex	space_index;
	
	/*
	 * Create a new image context.
	 */
	context->img_cx = CreateImageGroupContext ( context );
	Assert_(context->img_cx != NULL);
	
	if ( context->img_cx != NULL )
	{
		/*
		 * Set the context's color space to be our best one
		 */
		space_index = ConvertDepthToColorSpaceIndex( 0, false );
		context->color_space = GetColorSpace ( context, space_index, NULL );
		PR_ASSERT(context->color_space != NULL);
		
		if ( context->type == MWContextPrint )
			{
			data.display_type = IL_Printer;
			}
		else
			{
			data.display_type = IL_Console;
			}
			
		data.color_space = context->color_space;
		data.progressive_display = CPrefs::GetBoolean(CPrefs::DisplayWhileLoading) ? 
			PR_TRUE : PR_FALSE;
		data.dither_mode = IL_Auto;

		IL_SetDisplayMode ( context->img_cx,
			IL_PROGRESSIVE_DISPLAY | IL_DITHER_MODE | IL_COLOR_SPACE | IL_DISPLAY_TYPE, &data );
	}
	
	/*
	 * Get a standard one bit table for masks
	 */
	if ( gOneBitTable == NULL )
	{
		gOneBitTable = GetCTable ( 1 );
	}
	
	/*
	 * Set the default background color
	 */
	SetImageContextBackgroundColor ( context, kDefaultBGColorRed, kDefaultBGColorGreen, kDefaultBGColorBlue );
}

void DestroyImageContext (
			MWContext *			context )
{
	if ( context->color_space != NULL )
	{
		context->color_space = NULL;
	}
	
	if ( context->img_cx != NULL )
	{
		IL_DestroyGroupContext ( context->img_cx );
		context->img_cx = NULL;
	}
}

void SetImageContextBackgroundColor (
			MWContext *			context,
			Uint8 				red,
			Uint8				green,
			Uint8 				blue)
{
    IL_ColorSpace *	color_space;
    IL_IRGB *		trans_pixel;
    Uint8			count;

	color_space = context->color_space;
	trans_pixel = context->transparent_pixel;
	
	if ( trans_pixel == NULL )
		{
		trans_pixel = context->transparent_pixel = XP_NEW_ZAP(IL_IRGB);
		if ( trans_pixel == NULL )
			return;
		}

	/* Set the color of the transparent pixel. */
	trans_pixel->red = red;
	trans_pixel->green = green;
	trans_pixel->blue = blue;
	if ( color_space->type == NI_PseudoColor )
		{
		trans_pixel->index = color_space->cmap.num_colors - 1;
		}

	/*
	 * For PseudoColor color spaces, we must also update our color map with this
	 * new color. Our transparent/background color is always at the end of the color map.
	 */
	XP_ASSERT(color_space);
	SetColorSpaceTransparentColor ( color_space, red, green, blue );
	
	/*
	 * Now set the transparent color for any other cached spaces
	 */
	for ( count = 0; count < kNumColorSpaces; ++count )
		{
		if ( gColorSpaces[ count ] != NULL )
			{
			SetColorSpaceTransparentColor ( gColorSpaces[ count ], red, green, blue );
			}
		}
}

GDHandle GetDeepestDevice (
				void )
{
	GDHandle deepest = GetMainDevice();
	short depth = GetDepth (deepest);
	
	for (GDHandle current = GetDeviceList(); current; current = GetNextDevice (current)) {
		if (UDrawingUtils::IsActiveScreenDevice (current)) {
			short curDepth = GetDepth (current);
			if (depth < curDepth) {
				depth = curDepth;
				deepest = current;
			}
		}
	}

	return deepest;
}

Boolean VerifyDisplayContextColorSpace (
				MWContext * context )
{
	ColorSpaceIndex	screen_index;
	ColorSpaceIndex	current_index;
	IL_ColorSpace *	color_space;
	IL_DisplayData	data;
	Boolean			mustReload;
	uint32			cacheSize;
	
	mustReload = false;
	
	/*
	 * Make sure that the current display color space matches the context's color space
	 */
	screen_index = ConvertDepthToColorSpaceIndex ( 0, false );
	current_index = ConvertColorSpaceToIndex ( context->color_space );
	if ( screen_index != current_index )
		{
		/* the color space of the screen has changed, update our display context */
		color_space = GetColorSpace ( context, screen_index, NULL );
		if ( color_space != NULL )
			{
			context->color_space = color_space;
		
			data.color_space = color_space;
			IL_SetDisplayMode ( context->img_cx, IL_COLOR_SPACE, &data );
			
			SetColorSpaceTransparentColor ( color_space, context->transparent_pixel->red,
				context->transparent_pixel->green, context->transparent_pixel->blue );
			
			/* if this new space is deeper than the old, we should reload */
			mustReload = screen_index > current_index;
			
			/* and if we do need to reload, we should flush the image cache as well */
			/* it's up to our caller to decide if they want to reload the current page */
			if ( mustReload )
				{
				/* the only way to do this is to set the cache to 0 size and then restore it */
				cacheSize = IL_GetCacheSize();
				IL_SetCacheSize ( 0 );
				IL_SetCacheSize ( cacheSize );
				}
			}
		}
	
	return mustReload;
}

#pragma mark --- PRIVATE FUNCTIONS ---

/*
 * Private Utilities
 */

static IL_GroupContext * CreateImageGroupContext ( MWContext * context )
{
	IL_GroupContext *	img_cx;
	IMGCB *				img_cb;
	JMCException *		exc;

	exc = NULL;
	
    img_cb = IMGCBFactory_Create( &exc );
    if (exc)
    {
    	/* XXXM12N Should really return exception */
        JMC_DELETE_EXCEPTION( &exc );
        return 0L;
    }

    /*
     * Create an Image Group Context.  IL_NewGroupContext augments the
     * reference count for the JMC callback interface.  The opaque
     * argument to IL_NewGroupContext is the Front End's display
     * context, which will be passed back to all the Image Library's
     * FE callbacks.
     */
    img_cx = IL_NewGroupContext( (void*) context, (IMGCBIF *)img_cb);
    
    return img_cx;
}

static OSErr AllocatePixMap ( IL_Pixmap * il_pixmap, jint width, jint height,
	NS_PixMap * fe_pixmap, CTabHandle ctab )
{
	UInt32			rowbytes;
	Uint32			pixmap_depth;
	OSErr			err;
	IL_ColorSpace *	color_space;
	PixMap *		pixmap;
	Uint32			buffer_size;
	
	err = noErr;
	pixmap = &fe_pixmap->pixmap;
	
	/*
	 * Sanity checks
	 */
	if ( il_pixmap == NULL || fe_pixmap == NULL || ctab == NULL )
	{
		return -1;
	}
	
	/*
	 * Are we tiling this image?
	 */
	if ( il_pixmap->header.width == width && il_pixmap->header.height == height )
	{
		/* yup */
		fe_pixmap->tiled = true;
	}
	else
	{
		/* nope, so use the dimensions of the src image as CopyBits can scale */
		fe_pixmap->tiled = false;
		width = il_pixmap->header.width;
		height = il_pixmap->header.height;
	}
	
	/*
	 * Allocate the actual mac pixmap buffer
	 */
	
	color_space = il_pixmap->header.color_space;
	pixmap_depth = color_space->pixmap_depth;
	
	pixmap->bounds.top = 0;
	pixmap->bounds.left = 0;
	pixmap->bounds.right = width;
	pixmap->bounds.bottom = height;
	pixmap->pmVersion = 0;
	pixmap->packType = 0;
	pixmap->packSize = 0;
	pixmap->hRes = 72L << 16;
	pixmap->vRes = 72L << 16;
	pixmap->pixelSize = pixmap_depth;
	pixmap->planeBytes = 0;
	pixmap->pmReserved = 0;
	
	if ( pixmap_depth <= 8 )
	{
		/* set our pixel type to indexed */
		pixmap->pixelType = 0;
		pixmap->cmpCount = 1;
		pixmap->cmpSize = pixmap_depth;
	}
	else
	{
		/* set our pixel type to direct color. we need to be sure to do this */
		/* to prevent nasty crashes when saving to pictures/printing */
		pixmap->pixelType = 0x10;
		pixmap->cmpCount = 3;
		pixmap->cmpSize = pixmap_depth == 16 ? 5 : 8;
	}
	
	pixmap->pmTable = ctab;
	
	rowbytes = width * pixmap_depth;
	rowbytes = (( rowbytes + 31 ) >> 5) << 2;
	
	/*
	 * Sanity check rowBytes to make sure it's not over the QuickDraw limit
	 * This will cause the calling code to downgrade the bit depth until we can allocate
	 * the image, or just fail if it's too big overall
	 */
	if ( rowbytes > 0x3FFF )
		{
		return -1;
		}
		
	il_pixmap->header.widthBytes = rowbytes;

	pixmap->rowBytes = rowbytes | 0x8000;

	/*
	 * Allocate the buffer. If the image is really large, we just allocate out of temp
	 * memory directly as it won't force the allocator to allocate a big tem mem chunk
	 * which might be filled with other blocks that will force it to hang around for a
	 * long time.
	 *
	 * I will probably change the large block allocator to always use temp memory for
	 * huge blocks as it will simplify this code and fragment the large block heap less.
	 */
	buffer_size = rowbytes * height;
	if ( buffer_size > 63 * 1024L )
		{
		fe_pixmap->buffer_handle = TempNewHandle ( buffer_size, &err );
		
		/*
		 * If this fails, then go ahead and try malloc...
		 */
		if ( fe_pixmap->buffer_handle == NULL )
			{
			fe_pixmap->image_buffer = malloc ( buffer_size );
			if ( fe_pixmap->image_buffer == NULL )
				{
				err = memFullErr;
				}
			else
				{
				err = noErr;
				}
			}
		}
	else
		{
		fe_pixmap->image_buffer = malloc ( buffer_size );
		if ( fe_pixmap->image_buffer == NULL )
			{
			err = memFullErr;
			}
		}

	pixmap->baseAddr = (Ptr) 0xFF5E0001;
	il_pixmap->bits = (Ptr) 0xFF5E0001;

#if TRACK_IMAGE_CACHE_SIZE
	if ( err == noErr )
		{
		gCacheSize += buffer_size;
		if ( gCacheSize > gMaxCacheSize )
			gMaxCacheSize = gCacheSize;
		}
#endif
		
	return err;
}

static ColorSpaceIndex GetNextBestColorSpaceIndex ( ColorSpaceIndex index )
{
	switch ( index )
		{
		default:				index = kNumColorSpaces;	break;
		case kOneBit:			index = kNumColorSpaces;	break;
		case kEightBitColor:	index = kOneBit;			break;
		case kSixteenBit:		index = kEightBitColor;		break;
		case kThirtytwoBit:		index = kSixteenBit;		break;
		}
	
	return index;
}

static ColorSpaceIndex ConvertColorSpaceToIndex ( IL_ColorSpace * color_space )
{
	ColorSpaceIndex	index;

	index = kEightBitColor;
	
	switch ( color_space->type )
		{
		case NI_TrueColor:
			if ( color_space->pixmap_depth > 16 )
				{
				index = kThirtytwoBit;
				}
			else
				{
				index = kSixteenBit;
				}
				
			break;
		
		case NI_PseudoColor:
			switch ( color_space->pixmap_depth )
				{
				case 1:		index = kOneBit;		break;
				case 2:		index = kEightBitColor;	break;
				case 4:		index = kEightBitColor;	break;
				case 8:		index = kEightBitColor;	break;
				}
				
			break;
		
		case NI_GreyScale:
			switch ( color_space->pixmap_depth )
				{
				case 1:		index = kOneBit;		break;
				case 2:		index = kEightBitGray;	break;
				case 4:		index = kEightBitGray;	break;
				case 8:		index = kEightBitGray;	break;
				}

			break;
		}
	
	return index;
}

static ColorSpaceIndex ConvertDepthToColorSpaceIndex ( Int32 depth, Boolean grayscale )
{
	ColorSpaceIndex	index;
	GDHandle		gd;
	
	/*
	 * We default image depth to the depth of the deepest screen.
	 */
	if ( depth == 0 )
		{
		gd = GetDeepestDevice();
		depth = GetDepth ( gd );
		
		/* check that magic bit to see if it's a grayscale device */
		grayscale = ( (*gd)->gdFlags & ( 1 << gdDevType ) ) == 0;
		}
	
	/*
	 * If we're looking at allocating a direct pixmap, and the user's machine
	 * is low on temp memory, then downgrade to 8 bit
	 */
	if ( ( depth >= 16 ) && ( TempFreeMem() < ( 2048L * 1024L )) )
		{
		depth = 8;
		}
	
	if ( grayscale )
		{
		switch ( depth )
			{
			case 1:		index = kOneBit;		break;
			case 2:		index = kEightBitGray;	break;
			case 4:		index = kEightBitGray;	break;
			case 8:		index = kEightBitGray;	break;
			case 16:	index = kSixteenBit;	break;
			case 32:	index = kThirtytwoBit;	break;
			default:	index = kEightBitColor;	break;
			}
		}
	else
		{
		switch ( depth )
			{
			case 1:		index = kOneBit;		break;
			case 2:		index = kEightBitColor;	break;
			case 4:		index = kEightBitColor;	break;
			case 8:		index = kEightBitColor;	break;
			case 16:	index = kSixteenBit;	break;
			case 32:	index = kThirtytwoBit;	break;
			default:	index = kEightBitColor;	break;
			}
		}
	
	return index;
}

static IL_ColorSpace * AllocateColorSpace ( MWContext * context, ColorSpaceIndex space_index )
{
	IL_RGBBits		rgb;
	Int32			depth;
	Boolean			grayscale;
	IL_ColorMap *	color_map;
	IL_ColorSpace *	color_space;
	Int32			index;
	Uint8 *			index_map;
	Int32			num_colors;
	IL_IRGB			transparent_color;
	
	color_space = NULL;
	
	/* convert the space index back to depth/grayscale */
	switch ( space_index )
		{
		default:				return NULL;
		case kOneBit:			depth = 1; grayscale = true;	break;
		case kEightBitGray:		depth = 8; grayscale = true;	break;
		case kEightBitColor:	depth = 8; grayscale = false;	break;
		case kSixteenBit:		depth = 16; grayscale = false;	break;
		case kThirtytwoBit:		depth = 32; grayscale = false;	break;
		}
	
	if ( depth == 16 )
		{
		/*
		 * Create a 16 bit color space
		 */
		rgb.red_bits = 5;
		rgb.red_shift = 10;
		rgb.green_bits = 5;
		rgb.green_shift = 5;
		rgb.blue_bits = 5;
		rgb.blue_shift = 0;
		
		color_space = IL_CreateTrueColorSpace ( &rgb, 16 );
		}
	else
	if ( depth == 32 )
		{
		/*
		 * Create a 32 bit color space
		 */
		rgb.red_bits = 8;
		rgb.red_shift = 16;
		rgb.green_bits = 8;
		rgb.green_shift = 8;
		rgb.blue_bits = 8;
		rgb.blue_shift = 0;
		
		color_space = IL_CreateTrueColorSpace ( &rgb, 32 );
		}
	else
	if ( grayscale )
		{
		/*
		 * Create an indexed grayscale space.
		 */
		color_space = IL_CreateGreyScaleColorSpace ( depth, depth );
		}
	else
		{
		/*
		 * Create an indexed color space.
		 */
		
		/*
		 * First create a color map with a reserved color for the transparent color.
		 * When we first create a context, we won't yet have a transparent color, so
		 * then we just set it to the default background color
		 */
		if ( context->transparent_pixel != NULL )
			{
			transparent_color.red = context->transparent_pixel->red;
			transparent_color.green = context->transparent_pixel->green;
			transparent_color.blue = context->transparent_pixel->blue;
			}
		else
			{
			transparent_color.red = kDefaultBGColorRed;
			transparent_color.green = kDefaultBGColorGreen;
			transparent_color.blue = kDefaultBGColorBlue;
			}
		
		transparent_color.index = 0;
		
		color_map = IL_NewCubeColorMap ( NULL, 0, 1 << depth );
		
		/* Allocate the index map for this color map */
		if ( color_map != NULL )
			{
			IL_AddColorToColorMap ( color_map, &transparent_color );
						
			num_colors = ( 1 << depth );
			
			index_map = (Uint8 *) XP_ALLOC ( sizeof(uint8) * num_colors );
			if ( index_map != NULL )
				{
				for ( index = num_colors - 1; index >= 0; --index )
					{
					index_map[ index ] = index;
					}
				
				color_map->index = index_map;
				}
			else
				{
				IL_DestroyColorMap ( color_map );
				color_map = NULL;
				}
			}
		
		/* Now build a color space around it */
		if ( color_map != NULL )
			{
			color_space = IL_CreatePseudoColorSpace ( color_map, depth, depth );
			if ( color_space == NULL )
				{
				IL_DestroyColorMap ( color_map );
				color_map = NULL;
				}
			}
		}
	
	return color_space;
}

static IL_ColorSpace * GetColorSpace ( MWContext * context, ColorSpaceIndex space_index, CTabHandle * color_table )
{
	IL_ColorSpace *	color_space;
	CTabHandle		ctable;
	
	/* bounds check */
	if ( space_index >= kNumColorSpaces )
		{
		return NULL;
		}
	
	ctable = NULL;
	if ( color_table != NULL )
		{
		*color_table = NULL;
		}

	color_space = gColorSpaces[ space_index ];

	/*
	 * Do we need to allocate a space?
	 */
	if ( color_space == NULL )
		{
		color_space = AllocateColorSpace ( context, space_index );
				
		/*
		 * Allocate a mac color table for this color space
		 */
		if ( color_space != NULL )
			{
			ctable = ConvertColorSpaceToColorTable ( color_space );
			if ( ctable == NULL )
				{
				IL_ReleaseColorSpace ( color_space );
				color_space = NULL;
				}
			}
		
		/*
		 * Add a reference to this color space so we're sure no one ever deletes it
		 * out from under us (we keep them cached for the life of the browser).
		 */
		IL_AddRefToColorSpace ( color_space );
		
		gColorSpaces[ space_index ] = color_space;
		gColorTables [ space_index ] = ctable;
		}

	if ( color_space != NULL )
		{
		ctable = gColorTables [ space_index ];
		}
	
	if ( color_table != NULL )
		{
		*color_table = ctable;
		}

	return color_space;
}

static CTabHandle ConvertColorSpaceToColorTable ( IL_ColorSpace * color_space )
{
	CTabHandle	ctab;
	ColorSpec *	cspecs;
	uint32		ctab_entries;
	uint32		count;
	NI_RGB *	map_colors;
	
	ctab = NULL;
	if ( color_space->pixmap_depth <= 8 )
		{
		ctab_entries = color_space->cmap.num_colors;
		}
	else
		{
		ctab_entries = 1;
		}
	
	ctab = (CTabHandle) NewHandleClear ( sizeof(ColorTable) +
		sizeof(ColorSpec) * (ctab_entries - 1) );
	if ( ctab != NULL )
		{
		(*ctab)->ctSeed = GetCTSeed();
		(*ctab)->ctSize = ctab_entries - 1;
		
		/*
		 * Grab the color from the color map. If we're direct, then don't bother.
		 */
		if ( color_space->pixmap_depth <= 8 )
			{
			cspecs = &(*ctab)->ctTable[ 0 ];
			
			if ( color_space->type == NI_GreyScale )
				{
				Uint16	color;
				Uint16	color_inc;
				
				color_inc = 256 / ( 1 << color_space->pixmap_depth );
				color_inc |= color_inc << 8;
				
				color = 0;
				for ( count = 0; count < ctab_entries; ++count )
					{
					cspecs->value = count;
					cspecs->rgb.red = color;
					cspecs->rgb.green = color;
					cspecs->rgb.blue = color;
					
					cspecs++;
					color += color_inc;
					}
				}
			else
				{
				map_colors = &color_space->cmap.map[ 0 ];
				
				for ( count = 0; count < ctab_entries; ++count )
					{
					cspecs->value = count;
					cspecs->rgb.red = ((uint16) map_colors->red << 8 ) | (uint16) map_colors->red;
					cspecs->rgb.green = ((uint16) map_colors->green << 8 ) | (uint16) map_colors->green;
					cspecs->rgb.blue = ((uint16) map_colors->blue << 8 ) | (uint16) map_colors->blue;
					
					cspecs++;
					map_colors++;
					}
				}
			}
		}
	
	return ctab;
}

static void SetColorSpaceTransparentColor ( IL_ColorSpace * color_space, Uint8 red, Uint8 green, Uint8 blue )
{
	if ( color_space->type == NI_PseudoColor )
		{
		IL_RGB *map;
		
		/*
		 * The last color in the color map is always our transparent color
		 */
		map = &color_space->cmap.map[ color_space->cmap.num_colors - 1 ];
		map->red = red;
		map->green = green;
		map->blue = blue;
		}
}

static OSErr CreatePictureGWorld ( IL_Pixmap * image, IL_Pixmap * mask, PictureGWorldState * state )
{
	OSErr			err;
	CTabHandle		ctab;
	NI_IRGB *		transparent_pixel;
	IL_ColorSpace *	color_space;
	CGrafPtr		savePort;
	GDHandle		saveGD;
	Boolean			remapTransparentIndex;
	
	remapTransparentIndex = false;
	
	state->image = (NS_PixMap *) image->client_data;
	
	if ( mask != NULL )
		{
		state->mask = (NS_PixMap *) mask->client_data;
		
		color_space = image->header.color_space;
		XP_ASSERT(color_space);
		
		transparent_pixel = image->header.transparent_pixel;
		XP_ASSERT(transparent_pixel);
		
		/* get our own expended copy of the background/transparent color */
		state->transparentColor.red = (uint16) transparent_pixel->red | ( (uint16) transparent_pixel->red << 8 );
		state->transparentColor.green = (uint16) transparent_pixel->green | ( (uint16) transparent_pixel->green << 8 );
		state->transparentColor.blue = (uint16) transparent_pixel->blue | ( (uint16) transparent_pixel->blue << 8 );
		
		/*
		 * If the image has an indexed color color_space, then we know it has a unique
		 * transparent color. So, we can use it's color table for the copybits
		 */
		if ( image->header.color_space->type == NI_PseudoColor )
			{
			ctab = state->image->pixmap.pmTable;
			err = HandToHand ( (Handle *) &ctab );
			if ( err != noErr )
				return err;
			
			/* make sure the background entry contains the correct color */			
			state->transparentIndex = color_space->cmap.num_colors - 1;
			(*ctab)->ctTable[ state->transparentIndex ].rgb = state->transparentColor;
			remapTransparentIndex = true;
			}
		else
		/*
		 * If the image has a grayscale space, we're in a world of hurt. We need to steal
		 * an index to use for the transparent color
		 */
		if ( image->header.color_space->type == NI_GreyScale )
			{
			ctab = state->image->pixmap.pmTable;
			err = HandToHand ( (Handle *) &ctab );
			if ( err != noErr )
				return err;
			
			/* use the default transparent color, but find it's true index */
			state->transparentIndex = -1;
			remapTransparentIndex = true;
			}
		else
		/*
		 * The image has a true color space. Hopefully, the transparent color is unique to the
		 * image. If not, (ie the site's bg color is the same as a valid color in the image), we'll
		 * have more be transparent than should be. There's not a whole lot we can do about this
		 */
			{
			ctab = NULL;			
			}
		
		state->ctab = ctab;

		/*
		 * Make sure that our transparent color is unique. We can choose any color we want as it
		 * won't actually be displayed.
		 */
		CreateUniqueTransparentColor ( image, state );
						
		err = NewGWorld ( &state->gworld, image->header.color_space->pixmap_depth,
			&state->image->pixmap.bounds, ctab, NULL, useTempMem );
		
		/*
		 * If we have an indexed image, then find out where the transparent color maps to
		 * so that we can remove it
		 */
		if ( err == noErr && remapTransparentIndex )
			{
			GetGWorld ( &savePort, &saveGD );
			SetGWorld ( state->gworld, NULL );
			
			/* where does our transparent color really map to? */
			state->transparentIndex = Color2Index ( &state->transparentColor );
			
			/* Remove this from the inverse table */
			ReserveEntry ( state->transparentIndex, true );
			
			/*
			 * get the real RGB color this maps to (should always be the same for PseudoColor but
			 * could be different for grayscape where we may not have a unique color).
			 */
			Index2Color ( state->transparentIndex, &state->transparentColor );
			
			SetGWorld ( savePort, saveGD );
			}
		else
			{
			state->transparentIndex = -1;
			}
		}
	else
		{
		state->mask = NULL;
		
		}
	
	return err;
}

static void TeardownPictureGWorld ( PictureGWorldState * state )
{
	if ( state->gworld != NULL )
		{
		DisposeGWorld ( state->gworld );
		}
	
	if ( state->ctab != NULL )
		{
		DisposeCTable ( state->ctab );
		}
}

static void CreateUniqueTransparentColor ( IL_Pixmap * image, PictureGWorldState * state )
{
	RGBColor	rgb;
	Boolean		foundColor;
	
	if ( state->ctab == NULL )
		return;
	
	/*
	 * We have no idea which colors are actually used in the image (unless we want to actually
	 * go over each pixel).
	 *
	 * For direct pixels and grayscale images, we just use the transparent color that came along
	 * with the page. On color images, we actually try to find a unique color
	 */

#define	INV_TAB_RES		4
#define	INV_TAB_MAX		((Uint16)~((0x8000 >> (INV_TAB_RES-1)) - 1))
#define	INV_TAB_MASK(c)	((c) & INV_TAB_MAX)
#define	INV_TAB_INC		(0x8000 >> (INV_TAB_RES-1))

	if ( image->header.color_space->type == NI_PseudoColor )
		{
		/*
		 * We assume QD will use it's default 4bit inverse table, so try and find a unique color
		 * within that space.
		 * First look for our default transparent color. Most of the time it should be unique.
		 */
				
		if ( FindColorInCTable ( state->ctab, state->transparentIndex, &state->transparentColor ))
			{
			foundColor = false;
			
			/* we found that color, so run through all the colors in the hope of finding something unique */
			for ( rgb.red = 0; (Uint16)rgb.red <= INV_TAB_MAX; rgb.red += INV_TAB_INC )
				{
				for ( rgb.green = 0; (Uint16)rgb.green <= INV_TAB_MAX; rgb.green += INV_TAB_INC )
					{
					for ( rgb.blue = 0; (Uint16)rgb.blue <= INV_TAB_MAX; rgb.blue += INV_TAB_INC )
						{
						if ( FindColorInCTable ( state->ctab, state->transparentIndex, &rgb ) == false )
							{
							foundColor = true;
							goto foundit;
							}
						}
					}
				}

foundit:				
			/* if we found a color, then use it */
			if ( foundColor )
				{
				state->transparentColor = rgb;
				(*state->ctab)->ctTable[ state->transparentIndex ].rgb = rgb;
				}
			}
		}
}

static Boolean FindColorInCTable ( CTabHandle ctab, Uint32 skipIndex, RGBColor * rgb )
{
	Boolean		colorIsUsed;
	Uint32		count;
	UInt32		num_entries;
	ColorSpec *	cspecs;

	cspecs = (*ctab)->ctTable;
	num_entries = (*ctab)->ctSize;
	colorIsUsed = false;

	/* run through the color table and try to find this color */
	for ( count = 0; count < num_entries; ++count )
		{
		/* don't bother checking the current transparent color */
		if ( count != skipIndex )
			{
			/* make sure the color is unique within the resolution of the inverse table */
			if ( INV_TAB_MASK(cspecs[count].rgb.red) == INV_TAB_MASK(rgb->red) &&
					INV_TAB_MASK(cspecs[count].rgb.green) == INV_TAB_MASK(rgb->green) &&
					INV_TAB_MASK(cspecs[count].rgb.blue) == INV_TAB_MASK(rgb->blue) )
				{
				colorIsUsed = true;
				break;
				}
			}
		}
	
	return colorIsUsed;
}

static void CopyPicture ( PictureGWorldState * state )
{
	CGrafPtr		savePort;
	GDHandle		saveGD;
	PixMapHandle	pm;
	SInt8			hState;
	
	GetGWorld ( &savePort, &saveGD );
	pm = GetGWorldPixMap ( state->gworld );
	hState = HGetState ( (Handle) pm );
	
	LockPixels ( pm );

	SetGWorld ( state->gworld, NULL );
	
	/* copy our image into the gworld */
	CopyBits ( (BitMap *) &state->image->pixmap, (BitMap *) *pm, &state->image->pixmap.bounds,
		&state->image->pixmap.bounds, srcCopy, NULL );
	
	/* make our transparent index writeable again */
	if ( state->transparentIndex != -1 )
		{
		ReserveEntry ( state->transparentIndex, false );
		
		/* make sure the value field in the color table for the transparent entry is correct */
		(*(*pm)->pmTable)->ctTable[ state->transparentIndex ].value = state->transparentIndex;
		}
	
	/* fill the masked area with this color */
	RGBForeColor ( &state->transparentColor );
	
	CopyBits ( (BitMap *) &state->mask->pixmap, (BitMap *) *pm, &state->mask->pixmap.bounds,
		&state->image->pixmap.bounds, notSrcOr, NULL );

	SetGWorld ( savePort, saveGD );
	
	/* now actually copy the picture data, making the transparent color transparent */
	ForeColor ( blackColor );	
	RGBBackColor ( &state->transparentColor );
	
	CopyBits ( (BitMap *) *pm, &qd.thePort->portBits, &(*pm)->bounds, &(*pm)->bounds, transparent, NULL );
	
	UnlockPixels ( pm );
	HSetState ( (Handle) pm, hState );
}

static void LockPixmapBuffer ( IL_Pixmap * pixmap )
{
	NS_PixMap * fe_pixmap;
	
	fe_pixmap = (NS_PixMap *) pixmap->client_data;
	if ( fe_pixmap != NULL )
		{
		if ( fe_pixmap->lock_count == 0 )
			{
			if ( fe_pixmap->buffer_handle != NULL )
				{
				HLock ( fe_pixmap->buffer_handle );
				fe_pixmap->pixmap.baseAddr = *fe_pixmap->buffer_handle;
				}
			else
				{
				fe_pixmap->pixmap.baseAddr = (char *) fe_pixmap->image_buffer;
				}
				
			pixmap->bits = fe_pixmap->pixmap.baseAddr;
			}
		
		fe_pixmap->lock_count++;
		}
}

static void UnlockPixmapBuffer ( IL_Pixmap * pixmap )
{
	NS_PixMap * fe_pixmap;

	fe_pixmap = (NS_PixMap *) pixmap->client_data;

	if ( fe_pixmap != NULL )
		{
		if ( --fe_pixmap->lock_count == 0 )
			{
			if ( fe_pixmap->buffer_handle != NULL )
				{
				HUnlock ( fe_pixmap->buffer_handle );
				}

			fe_pixmap->pixmap.baseAddr = (Ptr) 0xFF5E0001;
			pixmap->bits = fe_pixmap->pixmap.baseAddr;
			}
		}
}


static void DrawScaledImage ( DrawingState * state, Point topLeft, jint x_offset,
	jint y_offset, jint width, jint height )
{
	Rect			srcRect;
	Rect *			maskRectPtr;
	Rect			dstRect;
	PixMap *		maskPtr;
	NS_PixMap *		fe_pixmap;
	NS_PixMap *		fe_mask;
	
	fe_pixmap = state->pixmap;
	fe_mask = state->mask;
	
	if ( fe_mask != NULL )
	{
		maskPtr = &fe_mask->pixmap;
		maskRectPtr = &maskPtr->bounds;
	}
	else
	{
		maskPtr = NULL;
		maskRectPtr = NULL;
	}

	srcRect.left = x_offset;
	srcRect.top = y_offset;
	srcRect.right = x_offset + width;
	srcRect.bottom = y_offset + height;
	
	/*
	 * Clip src Rect to the image.
	 */
	if ( fe_pixmap->pixmap.bounds.right < srcRect.right )
	{
		srcRect.right = fe_pixmap->pixmap.bounds.right;
	}
	if ( fe_pixmap->pixmap.bounds.bottom < srcRect.bottom )
	{
		srcRect.bottom = fe_pixmap->pixmap.bounds.bottom;
	}
	
	SetRect ( &dstRect, topLeft.h, topLeft.v, topLeft.h + width, topLeft.v + height );
				
	if ( maskPtr == NULL )
	{
		CopyBits ( (BitMap *) &fe_pixmap->pixmap, &qd.thePort->portBits, &srcRect,
			&dstRect, state->copyMode, NULL );
	}
	else
	{
		CopyMask ( (BitMap *) &fe_pixmap->pixmap, (BitMap *) maskPtr, &qd.thePort->portBits,
			&srcRect, maskRectPtr, &dstRect );
	}
}

static void DrawTiledImage ( DrawingState * state, Point topLeft, jint x_offset,
	jint y_offset, jint width, jint height )
{

	Rect			srcRect;
	Rect *			maskRectPtr;
	Rect			dstRect;
	PixMap *		maskPtr;
	int32			right_clip;
	int32			bottom_clip;
	int32			left;
	int32			top;
	int32			img_width;
	int32			img_height;
	int32			tile_width;
	int32			tile_height;
	int32			src_x_offset;
	int32			src_y_offset;
	NS_PixMap *		fe_pixmap;
	NS_PixMap *		fe_mask;
			
	fe_pixmap = state->pixmap;
	fe_mask = state->mask;

	if ( fe_mask != NULL )
	{
		maskPtr = &fe_mask->pixmap;
		maskRectPtr = &srcRect;
	}
	else
	{
		maskPtr = NULL;
		maskRectPtr = NULL;
	}
	
	img_width = fe_pixmap->pixmap.bounds.right - fe_pixmap->pixmap.bounds.left;
	img_height = fe_pixmap->pixmap.bounds.bottom - fe_pixmap->pixmap.bounds.top;

	right_clip = topLeft.h + width;
	bottom_clip = topLeft.v + height;
	
	/* the first row may be shorter */
	tile_height = img_height - ( y_offset % img_height );
	
	/*
	 * Walk through all the tiles in dst space
	 */
	
	top = topLeft.v;
	src_y_offset = y_offset % img_height;
	
	while ( top < bottom_clip )
	{
		left = topLeft.h;
		tile_width = img_width - ( x_offset % img_width );
		src_x_offset = x_offset % img_width;
		
		while ( left < right_clip )
		{
			dstRect.left = left;
			dstRect.top = top;
			dstRect.right = left + tile_width;
			dstRect.bottom = top + tile_height;
			
			srcRect.left = src_x_offset;
			srcRect.top = src_y_offset;
			srcRect.right = src_x_offset + tile_width;
			srcRect.bottom = src_y_offset + tile_height;
				
			if ( maskPtr == NULL )
			{
				CopyBits ( (BitMap *) &fe_pixmap->pixmap, &qd.thePort->portBits, &srcRect,
					&dstRect, state->copyMode, NULL );
			}
			else
			{
				CopyMask ( (BitMap *) &fe_pixmap->pixmap, (BitMap *) maskPtr, &qd.thePort->portBits,
					&srcRect, maskRectPtr, &dstRect );
			}
			
			/*
			 * Bump to the next column and be sure to clip if it's the last one
			 */
			left += tile_width;
			tile_width = img_width;
			if ( left + tile_width > right_clip )
			{
				tile_width = right_clip - left;
			}
			src_x_offset = 0;
		}
		
		/*
		 * Bump to the next row and be sure to clip if it's the last one
		 */
		top += tile_height;
		tile_height = img_height;
		if ( top + tile_height > bottom_clip )
		{
			tile_height = bottom_clip - top;
		}
		src_y_offset = 0;
	}
}

static OSErr PreparePixmapForDrawing ( IL_Pixmap * image, IL_Pixmap * mask, Boolean canCopyMask,
	DrawingState * state )
{
	RGBColor		rgb;
	long			index;
	NI_IRGB *		transparent_pixel;
	IL_ColorSpace *	color_space;
	CTabHandle		ctab;
	
	state->copyMode = srcCopy;
	state->pixmap = (NS_PixMap *) image->client_data;
	state->mask = NULL;
	
	if ( state->pixmap == NULL )
		{
		return -1;
		}
		
	color_space = image->header.color_space;
	XP_ASSERT(color_space);
	
	if ( mask != NULL )
		{
		state->mask = (NS_PixMap *) mask->client_data;
		}
	
	/*
	 * If we can't copymask and we have a transparent colour, then we need to use QuickDraw's
	 * transparent mode and set the OpColor
	 */
	transparent_pixel = image->header.transparent_pixel;
	
	if ( transparent_pixel != NULL )
		{
		/* Extract the transparent color for this pixmap */
		rgb.red = (uint16) transparent_pixel->red | ( (uint16) transparent_pixel->red << 8 );
		rgb.green = (uint16) transparent_pixel->green | ( (uint16) transparent_pixel->green << 8 );
		rgb.blue = (uint16) transparent_pixel->blue | ( (uint16) transparent_pixel->blue << 8 );
		
		/* if we have an indexed color space, then update the bg color. Grayscale color */
		/* spaces assume the color's been mapped to the closest default gray */
		if (color_space->type == NI_PseudoColor)
			{
			index = color_space->cmap.num_colors - 1;
			
			ctab = state->pixmap->pixmap.pmTable;
			(*ctab)->ctTable[ index ].rgb = rgb;
			}
		
		/*
		 * now, if we have a mask, but we can't copymask, then set our transfer mode
		 * to be transparent. We may have problems if the background color matches a
		 * color in the image, but that's life for now...
		 */
		if ( mask != NULL && !canCopyMask )
			{
			/* don't use the mask anymore */
			state->mask = NULL;
			
			state->copyMode = transparent;
			RGBBackColor ( &rgb );
			}
		}
		
	LockPixmapBuffer ( image );
	if ( state->mask != NULL )
		{
		LockPixmapBuffer ( mask );
		}

	return noErr;
}

static void DoneDrawingPixmap ( IL_Pixmap * image, IL_Pixmap * mask, DrawingState * state )
{
	UnlockPixmapBuffer ( image );
	if ( state->mask != NULL )
		{
		UnlockPixmapBuffer ( mask );
		}
}

static CIconHandle GetIconHandle ( jint iconID )
{
	CIconHandle	ic;
	
	if ( iconID == IL_IMAGE_EMBED )
	{
		iconID = IL_IMAGE_BAD_DATA;
	}
	
	iconID += IL_ICON_OFFSET;

	ic = CIconList::GetIcon( iconID );
	if ( !ic )
	{
		if (iconID >= IL_GOPHER_FIRST)
			iconID = IL_GOPHER_FIRST-1;
		else if (iconID >= IL_NEWS_FIRST)
			iconID = IL_NEWS_FIRST-1;
		else
			iconID = IL_IMAGE_FIRST-1;
		iconID = iconID + IL_ICON_OFFSET;
		ic = CIconList::GetIcon(iconID);
	}

	return ic;
}

void
ImageGroupObserver(XP_Observable /*observable*/,
	      XP_ObservableMsg message,
	      void* /*message_data*/,
	      void* closure)
{
	MWContext* theContext = static_cast<MWContext*>(closure);
	Assert_(theContext);
	if (!theContext)
		return;
		
	CBrowserContext* theBrowserContext = dynamic_cast<CBrowserContext*>(theContext->fe.newContext);
	Assert_(theContext);
	if (!theBrowserContext)
		return;
	
	switch(message)
	{
		case IL_STARTED_LOADING:
			theBrowserContext->SetImagesLoading(true);
			break;

		case IL_ABORTED_LOADING:
			theBrowserContext->SetImagesDelayed(true);
			break;

		case IL_FINISHED_LOADING:
			theBrowserContext->SetImagesLoading(false);
			break;

		case IL_STARTED_LOOPING:
			theBrowserContext->SetImagesLooping(true);
			break;

		case IL_FINISHED_LOOPING:
			theBrowserContext->SetImagesLooping(false);
			break;

		default:
			break;
	}
}

void
FE_MochaImageGroupObserver(XP_Observable /*observable*/,
	      XP_ObservableMsg message,
	      void *message_data,
	      void */*closure*/)
{
	IL_GroupMessageData *data = (IL_GroupMessageData *)message_data;
	MWContext *theContext = (MWContext *)data->display_context;

	// If we are passed a NULL display context, the MWContext has been
	// destroyed.
	if (!theContext)
		return;
		
	CBrowserContext* theBrowserContext = dynamic_cast<CBrowserContext*>(theContext->fe.newContext);
	if (!theBrowserContext)
		return;
	
	switch(message)
	{
		case IL_STARTED_LOADING:
			theBrowserContext->SetMochaImagesLoading(true);
			break;

		case IL_ABORTED_LOADING:
			theBrowserContext->SetMochaImagesDelayed(true);
			break;

		case IL_FINISHED_LOADING:
			theBrowserContext->SetMochaImagesLoading(false);
			break;

		case IL_STARTED_LOOPING:
			theBrowserContext->SetMochaImagesLooping(true);
			break;

		case IL_FINISHED_LOOPING:
			theBrowserContext->SetMochaImagesLooping(false);
			break;

		default:
			break;
	}
}

#ifdef PROFILE
#pragma profile off
#endif


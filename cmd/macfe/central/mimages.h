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

#pragma once

#include "ntypes.h"
#include "structs.h"
#include "cstring.h"
#include "Netscape_Constants.h"
#include "libimg.h"

class CBrowserContext;


/*
 * Our internal Pixmap structure.
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


XP_BEGIN_PROTOS

void	ImageGroupObserver(XP_Observable observable,
			XP_ObservableMsg message,
			void* message_data,
			void* closure);

/*
 * Macros that are here for historic reasons
 */
#define TRUNC_TO_MAX(LVALUE,MAX)	((LVALUE > MAX)? (LVALUE = MAX): LVALUE)
#define TRUNC_TO_MIN(LVALUE,MAX)	((LVALUE > MAX)? (LVALUE = MAX): LVALUE)


/*
 * High Level FE Palette Management
 */
void		CreateImageContext (
					MWContext *			context );

void		DestroyImageContext (
					MWContext *			context );

void		SetImageContextBackgroundColor (
					MWContext *			context,
					Uint8 				inRed,
					Uint8				inGreen,
					Uint8 				inBlue);

GDHandle	GetDeepestDevice (
					void );

Boolean		VerifyDisplayContextColorSpace (
					MWContext * context );
					

/*
 * High level FE functions for image management.
 */
EClickKind	FindImagePart (
					MWContext *			context,
					LO_ImageStruct *	image,
					SPoint32 *			where,
					cstring *			url,
					cstring *			target,
					LO_AnchorData * &		anchor );
							
BOOL		IsImageComplete (
					LO_ImageStruct *	image );

PicHandle	ConvertImageElementToPICT(
					LO_ImageStruct *	inElement);

cstring		GetURLFromImageElement(
					CBrowserContext *		inOwningContext,
					LO_ImageStruct *	inElement);


/*
 * These used to be private, but need to be public so we can start using more images and
 * fewer icons (for things like toolbars, Aurora, etc.)
 */

	/* width and height are the width/height of the destination rectangle. The image
	 * will be scaled to fit withing that rect. */
void DrawScaledImage ( DrawingState * state, Point topLeft, jint x_offset,
							jint y_offset, jint width, jint height );
	/* width and height are the width/height of the area into which you want to tile
	 * the image and have nothing to do with the size of the image itself */
void DrawTiledImage ( DrawingState * state, Point topLeft, jint x_offset,
							jint y_offset, jint width, jint height );

OSErr PreparePixmapForDrawing ( IL_Pixmap * image, IL_Pixmap * mask, Boolean canCopyMask,
									DrawingState * state );
void DoneDrawingPixmap ( IL_Pixmap * image, IL_Pixmap * mask, DrawingState * state );

void LockFEPixmapBuffer ( NS_PixMap* inPixmap ) ;
void UnlockFEPixmapBuffer ( NS_PixMap* inPixmap ) ;

void DestroyFEPixmap ( NS_PixMap* fe_pixmap ) ;

/*
 * Inline Utilities
 */
inline short GetDepth (GDHandle h) {
	return (* (*h)->gdPMap)->pixelSize;
}

XP_END_PROTOS

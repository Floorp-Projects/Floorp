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
 * Inline Utilities
 */
inline short GetDepth (GDHandle h) {
	return (* (*h)->gdPMap)->pixelSize;
}

XP_END_PROTOS

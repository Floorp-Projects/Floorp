/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*----------------------------------------------------------------------*/
/*																		*/
/* Name:		<Xfe/XfeBm.h>											*/
/*																		*/
/* Description:	Header for BmButton and BmCascade.  Widgets subclassed	*/
/*              from XmPushButton and XmCascadeButton.  Used in the		*/
/*              Mozilla bookmark filing mechanism.						*/
/*																		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/


#ifndef _XfeXfeBm_h_							/* start XfeBm.h		*/
#define _XfeXfeBm_h_

#include <Xfe/Xfe.h>

#ifdef __cplusplus								/* start C++			*/
extern "C" {
#endif

/*----------------------------------------------------------------------*/
/*																		*/
/* Bm Resource Names													*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmNaccentType					"accentType"
#define XmNarmPixmapMask				"armPixmapMask"
#define XmNlabelPixmapMask				"labelPixmapMask"

/*----------------------------------------------------------------------*/
/*																		*/
/* Bm Class Names														*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmCAccentType					"AccentType"
#define XmCArmPixmapMask				"ArmPixmapMask"
#define XmCLabelPixmapMask				"LabelPixmapMask"

/*----------------------------------------------------------------------*/
/*																		*/
/* Bm Representation Types												*/
/*																		*/
/*----------------------------------------------------------------------*/
#define XmRAccentType					"AccentType"

/*----------------------------------------------------------------------*/
/*																		*/
/* XmRAccentType														*/
/*																		*/
/*----------------------------------------------------------------------*/
enum
{
    XmACCENT_NONE,
    XmACCENT_BOTTOM,
	XmACCENT_TOP,
    XmACCENT_ALL
};
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Accent filing mode													*/
/*																		*/
/*----------------------------------------------------------------------*/
typedef enum
{
    XmACCENT_FILE_ANYWHERE,
    XmACCENT_FILE_SELF
} XfeAccentFileMode;
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Bm Representation types												*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void			
XfeBmRegisterRepresentationTypes	(void);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Accent enabled/disabled functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentDisable			(void);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentEnable			(void);
/*----------------------------------------------------------------------*/
extern Boolean
XfeBmAccentIsEnabled		(void);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetOffsetLeft		(Dimension		offset_left);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetOffsetRight		(Dimension		offset_right);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetShadowThickness	(Dimension		shadow_thickness);
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetThickness			(Dimension		thickness);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetOffsetLeft		(void);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetOffsetRight		(void);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetShadowThickness	(void);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmAccentGetThickness			(void);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent filing mode.						*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmAccentSetFileMode			(XfeAccentFileMode	file_mode);
/*----------------------------------------------------------------------*/
extern XfeAccentFileMode
XfeBmAccentGetFileMode			(void);
/*----------------------------------------------------------------------*/


/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade pixmap offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeBmPixmapSetOffset			(Dimension		offset_lefft);
/*----------------------------------------------------------------------*/
extern Dimension
XfeBmPixmapGetOffset			(void);
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Public accent drawing functions.										*/
/*																		*/
/*----------------------------------------------------------------------*/
extern void
XfeMenuItemDrawAccent			(Widget			item,
								 unsigned char	accent_type,
								 Dimension		offset_left,
								 Dimension		offset_right,
								 Dimension		shadow_thickness,
								 Dimension		accent_thickness);
/*----------------------------------------------------------------------*/
extern void
XfeMenuItemEraseAccent			(Widget			item,
								 unsigned char	accent_type,
								 Dimension		offset_left,
								 Dimension		offset_right,
								 Dimension		shadow_thickness,
								 Dimension		accent_thickness);
/*----------------------------------------------------------------------*/

#ifdef __cplusplus								/* end C++				*/
}
#endif

#endif											/* end XfeBm.h			*/

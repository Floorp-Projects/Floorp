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
/* Name:		<XfeBm/BmGlobal.c>										*/
/*																		*/
/* Description:	Global functions to set/get accent drawing and filing	*/
/*				properties.												*/
/*																		*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/

#include <Xfe/XfeBm.h>

/*----------------------------------------------------------------------*/
/*																		*/
/* Accent enabled/disabled functions									*/
/*																		*/
/*----------------------------------------------------------------------*/
static Boolean _accent_is_enabled = False;

/* extern */ Boolean
XfeBmAccentIsEnabled(void)
{
	return _accent_is_enabled;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentDisable(void)
{
	_accent_is_enabled = False;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentEnable(void)
{
	_accent_is_enabled = True;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
static Dimension	_accent_offset_left			= 4;
static Dimension	_accent_offset_right		= 8;
static Dimension	_accent_shadow_thickness	= 1;
static Dimension	_accent_thickness			= 1;

/* extern */ void
XfeBmAccentSetOffsetLeft(Dimension offset_left)
{
	_accent_offset_left = offset_left;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentSetOffsetRight(Dimension offset_right)
{
	_accent_offset_right = offset_right;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentSetShadowThickness(Dimension shadow_thickness)
{
	_accent_shadow_thickness = shadow_thickness;
}
/*----------------------------------------------------------------------*/
/* extern */ void
XfeBmAccentSetThickness(Dimension thickness)
{
	_accent_thickness = thickness;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetOffsetLeft(void)
{
	return _accent_offset_left;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetOffsetRight(void)
{
	return _accent_offset_right;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetShadowThickness(void)
{
	return _accent_shadow_thickness;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmAccentGetThickness(void)
{
	return _accent_thickness;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade accent filing mode.						*/
/*																		*/
/*----------------------------------------------------------------------*/
static XfeAccentFileMode	_accent_file_mode = XmACCENT_FILE_ANYWHERE;

/* extern */ void
XfeBmAccentSetFileMode(XfeAccentFileMode file_mode)
{
	_accent_file_mode = file_mode;
}
/*----------------------------------------------------------------------*/
/* extern */ XfeAccentFileMode
XfeBmAccentGetFileMode(void)
{
	return _accent_file_mode;
}
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*/
/*																		*/
/* Global BmButton/BmCascade pixmap offset values set/get functions		*/
/*																		*/
/*----------------------------------------------------------------------*/
static Dimension	_pixmap_offset = 4;

/* extern */ void
XfeBmPixmapSetOffset(Dimension offset)
{
	_pixmap_offset = offset;
}
/*----------------------------------------------------------------------*/
/* extern */ Dimension
XfeBmPixmapGetOffset(void)
{
	return _pixmap_offset;
}
/*----------------------------------------------------------------------*/

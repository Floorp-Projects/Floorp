/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*---------------------------------------*/
/*                                                                      */
/* Name:        AnimationType.h                                         */
/* Description: EAnimationType component header file.                   */
/* Created:     Daniel Malmer <malmer@netscape.com>                     */
/*                                                                      */
/* Since Logo.h and e_kit.c both need this enum type.                   */
/*----------------------------------------------------------------------*/


#ifndef _xfe_animation_type_h
#define _xfe_animation_type_h

/*
 * Animation Type
 * Make sure these match the order in xfe/icons/Makefile.
*/
typedef enum
{
    XFE_ANIMATION_MAIN = 0,
#ifdef NETSCAPE_PRIV
    XFE_ANIMATION_COMPASS,
    XFE_ANIMATION_MOZILLA,
#endif /* NETSCAPE_PRIV */
    XFE_ANIMATION_CUSTOM,
    XFE_ANIMATION_MAX
} EAnimationType;

#endif

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
/*
   icondata.h --- declarations for generated file icondata.c
   Created: Jamie Zawinski <jwz@netscape.com>, 28-Feb-95.
 */
#ifndef _ICONDATA_H_
#define _ICONDATA_H_

extern unsigned int fe_n_icon_colors;
extern unsigned short fe_icon_colors [][3];

#define MAX_ANIM_FRAMES 50
#define MAX_ANIMS 10

extern unsigned int fe_anim_frames[MAX_ANIMS];

struct fe_icon_data
{
  unsigned int width;
  unsigned int height;
  unsigned char *mono_bits;
  unsigned char *mask_bits;
  unsigned char *color_bits;
};

extern struct fe_icon_data anim_main_large[];
extern struct fe_icon_data anim_main_small[];
#ifdef NETSCAPE_PRIV
extern struct fe_icon_data anim_mozilla_large[];
extern struct fe_icon_data anim_mozilla_small[];
extern struct fe_icon_data anim_compass_large[];
extern struct fe_icon_data anim_compass_small[];
#ifdef __sgi
extern struct fe_icon_data anim_sgi_large[];
extern struct fe_icon_data anim_sgi_small[];
#endif /* __sgi */
#endif /* NETSCAPE_PRIV */
extern struct fe_icon_data* anim_custom_large;
extern struct fe_icon_data* anim_custom_small;

/* Include the generated externs of the icons */
#include "icon_extern.h"

#endif /*_ICONDATA_H_*/


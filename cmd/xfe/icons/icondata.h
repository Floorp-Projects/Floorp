/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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


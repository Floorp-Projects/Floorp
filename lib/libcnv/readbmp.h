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

/*readbmp.h*/
#ifndef _READBP_H
#define _READBP_H

/*BITMAP DECLARATIONS*/

CONVERT_IMAGERESULT start_input_bmp (CONVERT_IMG_INFO *p_imageinfo, CONVERT_IMGCONTEXT *p_inputparam);
CONVERT_IMAGERESULT finish_input_bmp(CONVERT_IMG_INFO *p_imageinfo, CONVERT_IMGCONTEXT *input,CONVERT_IMG_ARRAY *p_samparray);
CONVERT_IMAGERESULT read_colormap_bmp (CONVERT_IMGCONTEXT *p_inputparam, BYTE *p_colormap,int16 cmaplen, int16 mapentrysize);
/*end BITMAP DECLARATIONS*/
#endif


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
/*write png.c*/
#include <stdio.h>
#include <memory.h>
#include <malloc.h>

#include "xp.h"
#include "xp_core.h"/*defines of int32 ect*/
#include "xpassert.h"
#include "ntypes.h" /* for MWContext to include libcnv.h*/

#include "libcnv.h"
#include "writepng.h"
#include "pbmplus.h"
#include "ppm.h"
#include "ppmcmap.h"
#include "colorqnt.h"

#include "png.h"

#define MAXCOLORS 256

/*prototypes*/
void fill_png_row(CONVERT_IMG_ROW inputrgb, png_bytep outputindex, int16 width,colorhash_table cht);




CONVERT_IMAGERESULT
write_PNG_file (CONVERT_IMG_ARRAY p_rowarray,CONVERT_IMGCONTEXT *output,CONVERT_IMG_INFO *p_imageinfo,CONVERT_CALLBACKS p_callbacks)
{
    int16 colors,i;
    int16 maxcolor=MAXCOLORS-1;
    colorhist_vector chv;
    pixval maxval=MAXCOLORS-1;
    colorhash_table cht=NULL;
    png_structp write_ptr;
    png_infop info_ptr;/* important!*/
    png_bytep rowbuf=NULL;

    int32 y;
    int num_pass, pass;
    CONVERT_IMAGERESULT result=CONV_OK;

    if (!output||!p_imageinfo)
    {
        return CONVERR_INVALIDPARAMS;
    }

    /* Figure out the colormap. */
    chv = ppm_computecolorhist( (pixel **)p_rowarray, p_imageinfo->m_image_width, p_imageinfo->m_image_height, MAXCOLORS, &colors );
    if ( chv == (colorhist_vector) 0 )/*more than 256 colors*/
    {
        result=quantize_colors(p_rowarray,p_imageinfo->m_image_width,p_imageinfo->m_image_height,&maxcolor,&colors,&chv);
        if (result!=CONV_OK)
            return result;
        if ( chv == (colorhist_vector) 0 )
            return CONVERR_INVALIDCOLORMAP;
    }

    /* And make a hash table for fast lookup. */
    cht = ppm_colorhisttocolorhash( chv, colors );
    if (!cht)
    {
        XP_ASSERT(FALSE);
        return CONVERR_OUTOFMEMORY;
    }

    write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (void *)NULL,
      (png_error_ptr)NULL, (png_error_ptr)NULL);

    info_ptr=png_create_info_struct(write_ptr);
    info_ptr->width=p_imageinfo->m_image_width;
    info_ptr->height=p_imageinfo->m_image_height;
    info_ptr->bit_depth=8;
    info_ptr->color_type=3; /*3= indexed color*/
    info_ptr->compression_type=0; /* only option available*/
    info_ptr->filter_type=0; /* only valid value for adaptive filtering*/
    info_ptr->interlace_type=0;/*0= no interlacing*/
    info_ptr->num_palette=colors; /*from quantize_colors*/
    info_ptr->palette=XP_ALLOC(3*colors);/*remember to free this*/
    info_ptr->valid|=PNG_INFO_PLTE;
    for (i=0;i<colors;i++)
    {
	    info_ptr->palette[i].red = PPM_GETR( chv[i].color );
	    info_ptr->palette[i].green = PPM_GETG( chv[i].color );
	    info_ptr->palette[i].blue = PPM_GETB( chv[i].color );
    }
    ppm_freecolorhist( chv );
    if (!info_ptr->palette)
    {
      png_destroy_write_struct(&write_ptr, &info_ptr);
    }

    /*transparancy???*/

    if (setjmp(write_ptr->jmpbuf))
    {
      png_destroy_write_struct(&write_ptr, &info_ptr);
      return CONVERR_BADWRITE;
    }

    png_init_io(write_ptr, output->m_stream.m_file);

    png_write_info(write_ptr, info_ptr);

    num_pass = 1;
    /*we need to look up each RGB  and change it to the index of the color table*/
    rowbuf=(png_bytep) XP_ALLOC(p_imageinfo->m_image_width);
    if (!rowbuf)
    {
      png_destroy_write_struct(&write_ptr, &info_ptr);
      return CONVERR_BADWRITE;
    }

    for (pass = 0; pass < num_pass; pass++)
    {
      for (y = 0; y < p_imageinfo->m_image_height; y++)
      {
         fill_png_row(p_rowarray[y],rowbuf,p_imageinfo->m_image_width,cht);
         png_write_rows(write_ptr, &rowbuf, 1);
      }
    }
    XP_FREE(rowbuf);
    png_write_end(write_ptr, NULL);
    XP_FREE(info_ptr->palette);
    png_destroy_write_struct(&write_ptr, &info_ptr);
    fclose(output->m_stream.m_file);
    return CONV_OK;
}

void
fill_png_row(CONVERT_IMG_ROW inputrgb, png_bytep outputindex, int16 width,colorhash_table cht)
{
    int i;
    XP_ASSERT(inputrgb);
    XP_ASSERT(outputindex);
    XP_ASSERT(cht);
    for (i=0; i<width; i++)
    {
        outputindex[i]=(BYTE)ppm_lookupcolor( cht, &((pixel*)inputrgb)[i] );
    }
}

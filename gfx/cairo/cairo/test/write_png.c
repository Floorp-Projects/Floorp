/*
 * Copyright © 2003 USC, Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * University of Southern California not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. The University of Southern
 * California makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL THE UNIVERSITY OF
 * SOUTHERN CALIFORNIA BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <png.h>

#include "write_png.h"

static void
unpremultiply_data (png_structp png, png_row_infop row_info, png_bytep data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
        unsigned char *b = &data[i];
        unsigned int pixel;
        unsigned char alpha;

	memcpy (&pixel, b, sizeof (unsigned int));
	alpha = (pixel & 0xff000000) >> 24;
        if (alpha == 0) {
	    b[0] = b[1] = b[2] = b[3] = 0;
	} else {
            b[0] = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
            b[1] = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
            b[2] = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
	    b[3] = alpha;
	}
    }
}

void
write_png_argb32 (char *buffer, FILE *file,
		  int width, int height, int stride)
{
    int i;
    png_struct *png;
    png_info *info;
    png_byte **rows;
    png_color_16 white;
    
    rows = malloc (height * sizeof(png_byte*));

    for (i = 0; i < height; i++) {
	rows[i] = buffer + i * stride;
    }

    png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct (png);

    png_init_io (png, file);
    png_set_IHDR (png, info,
		  width, height, 8,
		  PNG_COLOR_TYPE_RGB_ALPHA, 
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);

    white.red = 0xff;
    white.blue = 0xff;
    white.green = 0xff;
    png_set_bKGD (png, info, &white);

    png_set_write_user_transform_fn (png, unpremultiply_data);
    png_set_bgr (png);

    png_write_info (png, info);
    png_write_image (png, rows);
    png_write_end (png, info);

    png_destroy_write_struct (&png, &info);

    free (rows);
}

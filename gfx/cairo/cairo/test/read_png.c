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
 * Author: Carl D. Worth <cworth@isi.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <png.h>

#include "read_png.h"
#include "xmalloc.h"

static void
premultiply_data (png_structp   png,
                  png_row_infop row_info,
                  png_bytep     data)
{
    int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
	unsigned char  *base = &data[i];
	unsigned char  blue = base[0];
	unsigned char  green = base[1];
	unsigned char  red = base[2];
	unsigned char  alpha = base[3];
	unsigned long	p;

	red = ((unsigned) red * (unsigned) alpha + 127) / 255;
	green = ((unsigned) green * (unsigned) alpha + 127) / 255;
	blue = ((unsigned) blue * (unsigned) alpha + 127) / 255;
	p = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	memcpy (base, &p, sizeof (unsigned long));
    }
}

read_png_status_t
read_png_argb32 (const char         *filename,
		 unsigned char      **data,
		 unsigned int       *width,
		 unsigned int       *height,
		 unsigned int	    *stride)
{
    int i;
    FILE *file;
    static const int PNG_SIG_SIZE = 8;
    unsigned char png_sig[PNG_SIG_SIZE];
    int sig_bytes;
    png_struct *png;
    png_info *info;
    png_uint_32 png_width, png_height;
    int depth, color_type, interlace;
    unsigned int pixel_size;
    png_byte **row_pointers;

    file = fopen (filename, "rb");
    if (file == NULL) {
	return READ_PNG_FILE_NOT_FOUND;
    }

    sig_bytes = fread (png_sig, 1, PNG_SIG_SIZE, file);
    if (png_check_sig (png_sig, sig_bytes) == 0) {
        fclose (file);
	return READ_PNG_FILE_NOT_PNG;
    }

    /* XXX: Perhaps we'll want some other error handlers? */
    png = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                  NULL,
                                  NULL,
                                  NULL);
    if (png == NULL) {
        fclose (file);
	return READ_PNG_NO_MEMORY;
    }

    info = png_create_info_struct (png);
    if (info == NULL) {
        fclose (file);
        png_destroy_read_struct (&png, NULL, NULL);
	return READ_PNG_NO_MEMORY;
    }

    png_init_io (png, file);
    png_set_sig_bytes (png, sig_bytes);

    png_read_info (png, info);

    png_get_IHDR (png, info,
                  &png_width, &png_height, &depth,
                  &color_type, &interlace, NULL, NULL);
    *width = png_width;
    *height = png_height;
    *stride = 4 * png_width;


    /* convert palette/gray image to rgb */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb (png);

    /* expand gray bit depth if needed */
    if (color_type == PNG_COLOR_TYPE_GRAY && depth < 8)
        png_set_gray_1_2_4_to_8 (png);
    /* transform transparency to alpha */
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha (png);

    if (depth == 16)
        png_set_strip_16 (png);

    if (depth < 8)
        png_set_packing (png);

    /* convert grayscale to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY
        || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb (png);

    if (interlace != PNG_INTERLACE_NONE)
        png_set_interlace_handling (png);

    png_set_bgr (png);
    png_set_filler (png, 0xff, PNG_FILLER_AFTER);

    png_set_read_user_transform_fn (png, premultiply_data);

    png_read_update_info (png, info);

    pixel_size = 4;
    *data = xmalloc (png_width * png_height * pixel_size);

    row_pointers = malloc (png_height * sizeof(char *));
    for (i=0; i < png_height; i++)
        row_pointers[i] = (png_byte *) (*data + i * png_width * pixel_size);

    png_read_image (png, row_pointers);
    png_read_end (png, info);

    free (row_pointers);
    fclose (file);

    png_destroy_read_struct (&png, &info, NULL);

    return READ_PNG_SUCCESS;
}

/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 *	Kristian Høgsberg <krh@redhat.com>
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

#include <errno.h>
#include <png.h>

/* Unpremultiplies data and converts native endian ARGB => RGBA bytes */
static void
unpremultiply_data (png_structp png, png_row_infop row_info, png_bytep data)
{
    unsigned int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
        uint8_t *b = &data[i];
        uint32_t pixel;
        uint8_t  alpha;

	memcpy (&pixel, b, sizeof (uint32_t));
	alpha = (pixel & 0xff000000) >> 24;
        if (alpha == 0) {
	    b[0] = b[1] = b[2] = b[3] = 0;
	} else {
            b[0] = (((pixel & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
            b[1] = (((pixel & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
            b[2] = (((pixel & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
	    b[3] = alpha;
	}
    }
}

/* Converts native endian xRGB => RGBx bytes */
static void
convert_data_to_bytes (png_structp png, png_row_infop row_info, png_bytep data)
{
    unsigned int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
        uint8_t *b = &data[i];
        uint32_t pixel;

	memcpy (&pixel, b, sizeof (uint32_t));

	b[0] = (pixel & 0xff0000) >> 16;
	b[1] = (pixel & 0x00ff00) >>  8;
	b[2] = (pixel & 0x0000ff) >>  0;
	b[3] = 0;
    }
}

/* Use a couple of simple error callbacks that do not print anything to
 * stderr and rely on the user to check for errors via the #cairo_status_t
 * return.
 */
static void
png_simple_error_callback (png_structp png,
	                   png_const_charp error_msg)
{
    cairo_status_t *error = png_get_error_ptr (png);

    /* default to the most likely error */
    if (*error == CAIRO_STATUS_SUCCESS)
	*error = _cairo_error (CAIRO_STATUS_NO_MEMORY);

#ifdef PNG_SETJMP_SUPPORTED
    longjmp (png_jmpbuf (png), 1);
#endif

    /* if we get here, then we have to choice but to abort ... */
}

static void
png_simple_warning_callback (png_structp png,
	                     png_const_charp error_msg)
{
    cairo_status_t *error = png_get_error_ptr (png);

    /* default to the most likely error */
    if (*error == CAIRO_STATUS_SUCCESS)
	*error = _cairo_error (CAIRO_STATUS_NO_MEMORY);

    /* png does not expect to abort and will try to tidy up after a warning */
}


static cairo_status_t
write_png (cairo_surface_t	*surface,
	   png_rw_ptr		write_func,
	   void			*closure)
{
    int i;
    cairo_status_t status;
    cairo_image_surface_t *image;
    void *image_extra;
    png_struct *png;
    png_info *info;
    png_time pt;
    png_byte **volatile rows = NULL;
    png_color_16 white;
    int png_color_type;
    int depth;

    status = _cairo_surface_acquire_source_image (surface,
						  &image,
						  &image_extra);

    if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	return _cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH);
    else if (status)
        return status;

    /* PNG complains about "Image width or height is zero in IHDR" */
    if (image->width == 0 || image->height == 0) {
	status = _cairo_error (CAIRO_STATUS_WRITE_ERROR);
	goto BAIL1;
    }

    rows = _cairo_malloc_ab (image->height, sizeof (png_byte*));
    if (rows == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto BAIL1;
    }

    for (i = 0; i < image->height; i++)
	rows[i] = (png_byte *) image->data + i * image->stride;

    png = png_create_write_struct (PNG_LIBPNG_VER_STRING, &status,
	                           png_simple_error_callback,
	                           png_simple_warning_callback);
    if (png == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto BAIL2;
    }

    info = png_create_info_struct (png);
    if (info == NULL) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto BAIL3;
    }

#ifdef PNG_SETJMP_SUPPORTED
    if (setjmp (png_jmpbuf (png)))
	goto BAIL3;
#endif

    png_set_write_fn (png, closure, write_func, NULL);

    switch (image->format) {
    case CAIRO_FORMAT_ARGB32:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	break;
    case CAIRO_FORMAT_RGB24:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_RGB;
	break;
    case CAIRO_FORMAT_A8:
	depth = 8;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    case CAIRO_FORMAT_A1:
	depth = 1;
	png_color_type = PNG_COLOR_TYPE_GRAY;
	break;
    default:
	status = _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
	goto BAIL3;
    }

    png_set_IHDR (png, info,
		  image->width,
		  image->height, depth,
		  png_color_type,
		  PNG_INTERLACE_NONE,
		  PNG_COMPRESSION_TYPE_DEFAULT,
		  PNG_FILTER_TYPE_DEFAULT);

    white.gray = (1 << depth) - 1;
    white.red = white.blue = white.green = white.gray;
    png_set_bKGD (png, info, &white);

    png_convert_from_time_t (&pt, time (NULL));
    png_set_tIME (png, info, &pt);

    /* We have to call png_write_info() before setting up the write
     * transformation, since it stores data internally in 'png'
     * that is needed for the write transformation functions to work.
     */
    png_write_info (png, info);

    if (image->format == CAIRO_FORMAT_ARGB32)
	png_set_write_user_transform_fn (png, unpremultiply_data);
    else if (image->format == CAIRO_FORMAT_RGB24)
	png_set_write_user_transform_fn (png, convert_data_to_bytes);
    if (image->format == CAIRO_FORMAT_RGB24)
	png_set_filler (png, 0, PNG_FILLER_AFTER);

    png_write_image (png, rows);
    png_write_end (png, info);

BAIL3:
    png_destroy_write_struct (&png, &info);
BAIL2:
    free (rows);
BAIL1:
    _cairo_surface_release_source_image (surface, image, image_extra);

    return status;
}

static void
stdio_write_func (png_structp png, png_bytep data, png_size_t size)
{
    FILE *fp;

    fp = png_get_io_ptr (png);
    while (size) {
	size_t ret = fwrite (data, 1, size, fp);
	size -= ret;
	data += ret;
	if (size && ferror (fp)) {
	    cairo_status_t *error = png_get_error_ptr (png);
	    if (*error == CAIRO_STATUS_SUCCESS)
		*error = _cairo_error (CAIRO_STATUS_WRITE_ERROR);
	    png_error (png, NULL);
	}
    }
}

/**
 * cairo_surface_write_to_png:
 * @surface: a #cairo_surface_t with pixel contents
 * @filename: the name of a file to write to
 *
 * Writes the contents of @surface to a new file @filename as a PNG
 * image.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if the PNG file was written
 * successfully. Otherwise, %CAIRO_STATUS_NO_MEMORY if memory could not
 * be allocated for the operation or
 * CAIRO_STATUS_SURFACE_TYPE_MISMATCH if the surface does not have
 * pixel contents, or %CAIRO_STATUS_WRITE_ERROR if an I/O error occurs
 * while attempting to write the file.
 **/
cairo_status_t
cairo_surface_write_to_png (cairo_surface_t	*surface,
			    const char		*filename)
{
    FILE *fp;
    cairo_status_t status;

    if (surface->status)
	return surface->status;

    if (surface->finished)
	return _cairo_error (CAIRO_STATUS_SURFACE_FINISHED);

    fp = fopen (filename, "wb");
    if (fp == NULL) {
	switch (errno) {
	case ENOMEM:
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	default:
	    return _cairo_error (CAIRO_STATUS_WRITE_ERROR);
	}
    }

    status = write_png (surface, stdio_write_func, fp);

    if (fclose (fp) && status == CAIRO_STATUS_SUCCESS)
	status = _cairo_error (CAIRO_STATUS_WRITE_ERROR);

    return status;
}

struct png_write_closure_t {
    cairo_write_func_t		 write_func;
    void			*closure;
};

static void
stream_write_func (png_structp png, png_bytep data, png_size_t size)
{
    cairo_status_t status;
    struct png_write_closure_t *png_closure;

    png_closure = png_get_io_ptr (png);
    status = png_closure->write_func (png_closure->closure, data, size);
    if (status) {
	cairo_status_t *error = png_get_error_ptr (png);
	if (*error == CAIRO_STATUS_SUCCESS)
	    *error = status;
	png_error (png, NULL);
    }
}

/**
 * cairo_surface_write_to_png_stream:
 * @surface: a #cairo_surface_t with pixel contents
 * @write_func: a #cairo_write_func_t
 * @closure: closure data for the write function
 *
 * Writes the image surface to the write function.
 *
 * Return value: %CAIRO_STATUS_SUCCESS if the PNG file was written
 * successfully.  Otherwise, %CAIRO_STATUS_NO_MEMORY is returned if
 * memory could not be allocated for the operation,
 * CAIRO_STATUS_SURFACE_TYPE_MISMATCH if the surface does not have
 * pixel contents.
 **/
cairo_status_t
cairo_surface_write_to_png_stream (cairo_surface_t	*surface,
				   cairo_write_func_t	write_func,
				   void			*closure)
{
    struct png_write_closure_t png_closure;

    png_closure.write_func = write_func;
    png_closure.closure = closure;

    return write_png (surface, stream_write_func, &png_closure);
}
slim_hidden_def (cairo_surface_write_to_png_stream);

static inline int
multiply_alpha (int alpha, int color)
{
    int temp = (alpha * color) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

/* Premultiplies data and converts RGBA bytes => native endian */
static void
premultiply_data (png_structp   png,
                  png_row_infop row_info,
                  png_bytep     data)
{
    unsigned int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
	uint8_t *base  = &data[i];
	uint8_t  alpha = base[3];
	uint32_t p;

	if (alpha == 0) {
	    p = 0;
	} else {
	    uint8_t  red   = base[0];
	    uint8_t  green = base[1];
	    uint8_t  blue  = base[2];

	    if (alpha != 0xff) {
		red   = multiply_alpha (alpha, red);
		green = multiply_alpha (alpha, green);
		blue  = multiply_alpha (alpha, blue);
	    }
	    p = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	}
	memcpy (base, &p, sizeof (uint32_t));
    }
}

/* Converts RGBx bytes to native endian xRGB */
static void
convert_bytes_to_data (png_structp png, png_row_infop row_info, png_bytep data)
{
    unsigned int i;

    for (i = 0; i < row_info->rowbytes; i += 4) {
	uint8_t *base  = &data[i];
	uint8_t  red   = base[0];
	uint8_t  green = base[1];
	uint8_t  blue  = base[2];
	uint32_t pixel;

	pixel = (0xff << 24) | (red << 16) | (green << 8) | (blue << 0);
	memcpy (base, &pixel, sizeof (uint32_t));
    }
}

static cairo_surface_t *
read_png (png_rw_ptr	read_func,
	  void		*closure)
{
    cairo_surface_t *surface;
    png_struct *png = NULL;
    png_info *info;
    png_byte *data = NULL;
    png_byte **row_pointers = NULL;
    png_uint_32 png_width, png_height;
    int depth, color_type, interlace, stride;
    unsigned int i;
    cairo_format_t format;
    cairo_status_t status;

    /* XXX: Perhaps we'll want some other error handlers? */
    png = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                  &status,
	                          png_simple_error_callback,
	                          png_simple_warning_callback);
    if (png == NULL) {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	goto BAIL;
    }

    info = png_create_info_struct (png);
    if (info == NULL) {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	goto BAIL;
    }

    png_set_read_fn (png, closure, read_func);

    status = CAIRO_STATUS_SUCCESS;
#ifdef PNG_SETJMP_SUPPORTED
    if (setjmp (png_jmpbuf (png))) {
	surface = _cairo_surface_create_in_error (status);
	goto BAIL;
    }
#endif

    png_read_info (png, info);

    png_get_IHDR (png, info,
                  &png_width, &png_height, &depth,
                  &color_type, &interlace, NULL, NULL);
    if (status) { /* catch any early warnings */
	surface = _cairo_surface_create_in_error (status);
	goto BAIL;
    }

    /* convert palette/gray image to rgb */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb (png);

    /* expand gray bit depth if needed */
    if (color_type == PNG_COLOR_TYPE_GRAY) {
#if PNG_LIBPNG_VER >= 10209
        png_set_expand_gray_1_2_4_to_8 (png);
#else
        png_set_gray_1_2_4_to_8 (png);
#endif
    }

    /* transform transparency to alpha */
    if (png_get_valid (png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha (png);

    if (depth == 16)
        png_set_strip_16 (png);

    if (depth < 8)
        png_set_packing (png);

    /* convert grayscale to RGB */
    if (color_type == PNG_COLOR_TYPE_GRAY ||
	color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
	png_set_gray_to_rgb (png);
    }

    if (interlace != PNG_INTERLACE_NONE)
        png_set_interlace_handling (png);

    switch (color_type) {
	default:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
	case PNG_COLOR_TYPE_RGB_ALPHA:
	    format = CAIRO_FORMAT_ARGB32;
	    png_set_read_user_transform_fn (png, premultiply_data);
	    break;

	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_PALETTE:
	case PNG_COLOR_TYPE_RGB:
	    format = CAIRO_FORMAT_RGB24;
	    png_set_read_user_transform_fn (png, convert_bytes_to_data);
	    png_set_filler (png, 0xff, PNG_FILLER_AFTER);
	    break;
    }

    png_read_update_info (png, info);

    stride = cairo_format_stride_for_width (format, png_width);
    if (stride < 0) {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));
	goto BAIL;
    }

    data = _cairo_malloc_ab (png_height, stride);
    if (data == NULL) {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	goto BAIL;
    }

    row_pointers = _cairo_malloc_ab (png_height, sizeof (char *));
    if (row_pointers == NULL) {
	surface = _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	goto BAIL;
    }

    for (i = 0; i < png_height; i++)
        row_pointers[i] = &data[i * stride];

    png_read_image (png, row_pointers);
    png_read_end (png, info);

    if (status) { /* catch any late warnings - probably hit an error already */
	surface = _cairo_surface_create_in_error (status);
	goto BAIL;
    }

    surface = cairo_image_surface_create_for_data (data, format,
						   png_width, png_height,
						   stride);
    if (surface->status)
	goto BAIL;

    _cairo_image_surface_assume_ownership_of_data ((cairo_image_surface_t*)surface);
    data = NULL;

 BAIL:
    if (row_pointers)
	free (row_pointers);
    if (data)
	free (data);
    if (png)
	png_destroy_read_struct (&png, &info, NULL);

    return surface;
}

static void
stdio_read_func (png_structp png, png_bytep data, png_size_t size)
{
    FILE *fp;

    fp = png_get_io_ptr (png);
    while (size) {
	size_t ret = fread (data, 1, size, fp);
	size -= ret;
	data += ret;
	if (size && (feof (fp) || ferror (fp))) {
	    cairo_status_t *error = png_get_error_ptr (png);
	    if (*error == CAIRO_STATUS_SUCCESS)
		*error = _cairo_error (CAIRO_STATUS_READ_ERROR);
	    png_error (png, NULL);
	}
    }
}

/**
 * cairo_image_surface_create_from_png:
 * @filename: name of PNG file to load
 *
 * Creates a new image surface and initializes the contents to the
 * given PNG file.
 *
 * Return value: a new #cairo_surface_t initialized with the contents
 * of the PNG file, or a "nil" surface if any error occurred. A nil
 * surface can be checked for with cairo_surface_status(surface) which
 * may return one of the following values:
 *
 *	CAIRO_STATUS_NO_MEMORY
 *	CAIRO_STATUS_FILE_NOT_FOUND
 *	CAIRO_STATUS_READ_ERROR
 **/
cairo_surface_t *
cairo_image_surface_create_from_png (const char *filename)
{
    FILE *fp;
    cairo_surface_t *surface;

    fp = fopen (filename, "rb");
    if (fp == NULL) {
	cairo_status_t status;
	switch (errno) {
	case ENOMEM:
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    break;
	case ENOENT:
	    status = _cairo_error (CAIRO_STATUS_FILE_NOT_FOUND);
	    break;
	default:
	    status = _cairo_error (CAIRO_STATUS_READ_ERROR);
	    break;
	}
	return _cairo_surface_create_in_error (status);
    }

    surface = read_png (stdio_read_func, fp);

    fclose (fp);

    return surface;
}

struct png_read_closure_t {
    cairo_read_func_t		 read_func;
    void			*closure;
};

static void
stream_read_func (png_structp png, png_bytep data, png_size_t size)
{
    cairo_status_t status;
    struct png_read_closure_t *png_closure;

    png_closure = png_get_io_ptr (png);
    status = png_closure->read_func (png_closure->closure, data, size);
    if (status) {
	cairo_status_t *error = png_get_error_ptr (png);
	if (*error == CAIRO_STATUS_SUCCESS)
	    *error = status;
	png_error (png, NULL);
    }
}

/**
 * cairo_image_surface_create_from_png_stream:
 * @read_func: function called to read the data of the file
 * @closure: data to pass to @read_func.
 *
 * Creates a new image surface from PNG data read incrementally
 * via the @read_func function.
 *
 * Return value: a new #cairo_surface_t initialized with the contents
 * of the PNG file or %NULL if the data read is not a valid PNG image or
 * memory could not be allocated for the operation.
 **/
cairo_surface_t *
cairo_image_surface_create_from_png_stream (cairo_read_func_t	read_func,
					    void		*closure)
{
    struct png_read_closure_t png_closure;

    png_closure.read_func = read_func;
    png_closure.closure = closure;

    return read_png (stream_read_func, &png_closure);
}

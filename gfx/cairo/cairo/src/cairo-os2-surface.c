/* vim: set sw=4 sts=4 et cin: */
/* cairo - a vector graphics library with display and print output
 *
 * Copyright (c) 2005-2006 netlabs.org
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
 * The Initial Developer of the Original Code is
 *     Doodle <doodle@scenergy.dfmk.hu>
 *
 * Contributor(s):
 *     Peter Weilbacher <mozilla@Weilbacher.org>
 */

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#ifdef BUILD_CAIRO_DLL
# define INCL_WIN
# define INCL_GPI
# define INCL_DOS
# define INCL_DOSERRORS
# include <os2emx.h>
# include "cairo-os2.h"
# ifndef __WATCOMC__
#  include <emx/startup.h>
# endif
#endif
#include "cairoint.h"
#include "cairo-os2-private.h"
#include "fontconfig/fontconfig.h"

/*
 * Here comes the extra API for the OS/2 platform. Currently it consists
 * of two extra functions, the cairo_os2_init () and the
 * cairo_os2_fini (). Both of them are called automatically if
 * Cairo is compiled to be a DLL file, but you have to call them before
 * using the Cairo API if you link to Cairo statically!
 *
 * You'll also find the code in here which deals with DLL initialization
 * and termination, if the code is built to be a DLL.
 * (if BUILD_CAIRO_DLL is defined)
 */

/* Initialization counter: */
static int cairo_os2_initialization_count = 0;

/* The mutex semaphores Cairo uses all around: */
HMTX cairo_toy_font_face_hash_table_mutex = 0;
HMTX cairo_scaled_font_map_mutex = 0;
HMTX _global_image_glyph_cache_mutex = 0;
#ifdef CAIRO_HAS_FT_FONT
HMTX cairo_ft_unscaled_font_map_mutex = 0;
#endif

static void inline
DisableFPUException (void)
{
    unsigned short usCW;

    /* Some OS/2 PM API calls modify the FPU Control Word,
     * but forget to restore it.
     *
     * This can result in XCPT_FLOAT_INVALID_OPCODE exceptions,
     * so to be sure, we disable Invalid Opcode FPU exception
     * before using FPU stuffs.
     */
    usCW = _control87 (0, 0);
    usCW = usCW | EM_INVALID | 0x80;
    _control87 (usCW, MCW_EM | 0x80);
}

cairo_public void
cairo_os2_init (void)
{
    /* This may initialize some stuffs, like create mutex semaphores etc.. */

    cairo_os2_initialization_count++;
    if (cairo_os2_initialization_count > 1) return;

    DisableFPUException ();

    /* Create the mutex semaphores we'll use! */

    /* cairo-font.c: */
    DosCreateMutexSem (NULL, &cairo_toy_font_face_hash_table_mutex, 0, FALSE);
    DosCreateMutexSem (NULL, &cairo_scaled_font_map_mutex, 0, FALSE);
    DosCreateMutexSem (NULL, &_global_image_glyph_cache_mutex, 0, FALSE);

#ifdef CAIRO_HAS_FT_FONT
    /* cairo-ft-font.c: */
    DosCreateMutexSem (NULL, &cairo_ft_unscaled_font_map_mutex, 0, FALSE);
#endif

    /* Initialize FontConfig */
    FcInit ();
}

cairo_public void
cairo_os2_fini (void)
{
    /* This has to uninitialize some stuffs, like destroy mutex semaphores etc.. */

    if (cairo_os2_initialization_count <= 0) return;
    cairo_os2_initialization_count--;
    if (cairo_os2_initialization_count > 0) return;

    DisableFPUException ();

    /* Free allocated memories! */
    /* (Check cairo_debug_reset_static_date () for an example of this!) */
    _cairo_font_reset_static_data ();
#ifdef CAIRO_HAS_FT_FONT
    _cairo_ft_font_reset_static_data ();
#endif

    /* Destroy the mutex semaphores we've created! */
    /* cairo-font.c: */
    if (cairo_toy_font_face_hash_table_mutex) {
        DosCloseMutexSem (cairo_toy_font_face_hash_table_mutex);
        cairo_toy_font_face_hash_table_mutex = 0;
    }
    if (cairo_scaled_font_map_mutex) {
        DosCloseMutexSem (cairo_scaled_font_map_mutex);
        cairo_scaled_font_map_mutex = 0;
    }
    if (_global_image_glyph_cache_mutex) {
        DosCloseMutexSem (_global_image_glyph_cache_mutex);
        _global_image_glyph_cache_mutex = 0;
    }

#ifdef CAIRO_HAS_FT_FONT
    /* cairo-ft-font.c: */
    if (cairo_ft_unscaled_font_map_mutex) {
        DosCloseMutexSem (cairo_ft_unscaled_font_map_mutex);
        cairo_ft_unscaled_font_map_mutex = 0;
    }
#endif

    /* Uninitialize FontConfig */
    FcFini ();

#ifdef __WATCOMC__
    /* It can happen that the libraries we use have memory leaks,
     * so there are still memory chunks allocated at this point.
     * In these cases, Watcom might still have a bigger memory chunk,
     * called "the heap" allocated from the OS.
     * As we want to minimize the memory we lose from the point of
     * view of the OS, we call this function to shrink that heap
     * as much as possible.
     */
    _heapshrink ();
#endif
}

#ifdef BUILD_CAIRO_DLL
/* The main DLL entry for DLL initialization and uninitialization */
/* Only include this code if we're about to build a DLL.          */

#ifdef __WATCOMC__
unsigned _System
LibMain (unsigned hmod,
         unsigned termination)
#else
unsigned long _System
_DLL_InitTerm (unsigned long hModule,
               unsigned long termination)
#endif
{
    if (termination) {
        /* Unloading the DLL */
        cairo_os2_fini ();

#ifndef __WATCOMC__
        /* Uninitialize RTL of GCC */
        __ctordtorTerm ();
        _CRT_term ();
#endif
        return 1;
    } else {
        /* Loading the DLL */
#ifndef __WATCOMC__
        /* Initialize RTL of GCC */
        if (_CRT_init () != 0)
            return 0;
        __ctordtorInit ();
#endif

        cairo_os2_init ();
        return 1;
    }
}

#endif /* BUILD_CAIRO_DLL */

/*
 * The following part of the source file contains the code which might
 * be called the "core" of the OS/2 backend support. This contains the
 * OS/2 surface support functions and structures.
 */

/* Forward declaration */
static const cairo_surface_backend_t cairo_os2_surface_backend;

/* Unpublished API:
 *   GpiEnableYInversion = PMGPI.723
 *   GpiQueryYInversion = PMGPI.726
 *   BOOL APIENTRY GpiEnableYInversion (HPS hps, LONG lHeight);
 *   LONG APIENTRY GpiQueryYInversion (HPS hps);
 */
BOOL APIENTRY GpiEnableYInversion (HPS hps, LONG lHeight);
LONG APIENTRY GpiQueryYInversion (HPS hps);

#ifdef __WATCOMC__
/* Function declaration for GpiDrawBits () (missing from OpenWatcom headers) */
LONG APIENTRY GpiDrawBits (HPS hps,
                           PVOID pBits,
                           PBITMAPINFO2 pbmiInfoTable,
                           LONG lCount,
                           PPOINTL aptlPoints,
                           LONG lRop,
                           ULONG flOptions);
#endif

static void
_cairo_os2_surface_blit_pixels (cairo_os2_surface_t *surface,
                                HPS                  hps_begin_paint,
                                PRECTL               prcl_begin_paint_rect)
{
    POINTL aptlPoints[4];
    LONG lOldYInversion, rc = GPI_OK;

    /* Enable Y Inversion for the HPS, so the
     * GpiDrawBits will work with upside-top image, not with upside-down image!
     */
    lOldYInversion = GpiQueryYInversion (hps_begin_paint);
    GpiEnableYInversion (hps_begin_paint, surface->bitmap_info.cy-1);

    /* Target coordinates (Noninclusive) */
    aptlPoints[0].x = prcl_begin_paint_rect->xLeft;
    aptlPoints[0].y = prcl_begin_paint_rect->yBottom;

    aptlPoints[1].x = prcl_begin_paint_rect->xRight-1;
    aptlPoints[1].y = prcl_begin_paint_rect->yTop-1;

    /* Source coordinates (Inclusive) */
    aptlPoints[2].x = prcl_begin_paint_rect->xLeft;
    aptlPoints[2].y = prcl_begin_paint_rect->yBottom;

    aptlPoints[3].x = prcl_begin_paint_rect->xRight;
    aptlPoints[3].y = (prcl_begin_paint_rect->yTop);

    /* Some extra checking for limits
     * (Dunno if really needed, but had some crashes sometimes without it,
     *  while developing the code...)
     */
    {
        int i;
        for (i = 0; i < 4; i++) {
            if (aptlPoints[i].x < 0)
                aptlPoints[i].x = 0;
            if (aptlPoints[i].y < 0)
                aptlPoints[i].y = 0;
            if (aptlPoints[i].x > (LONG) surface->bitmap_info.cx)
                aptlPoints[i].x = (LONG) surface->bitmap_info.cx;
            if (aptlPoints[i].y > (LONG) surface->bitmap_info.cy)
                aptlPoints[i].y = (LONG) surface->bitmap_info.cy;
        }
    }

    /* Debug code to draw rectangle limits */
#if 0
    {
        int x, y;
        unsigned char *pixels;

        pixels = surface->pixels;
        for (x = 0; x < surface->bitmap_info.cx; x++) {
            for (y = 0; y < surface->bitmap_info.cy; y++) {
                if ((x == 0) ||
                    (y == 0) ||
                    (x == y) ||
                    (x >= surface->bitmap_info.cx-1) ||
                    (y >= surface->bitmap_info.cy-1))
                {
                    pixels[y*surface->bitmap_info.cx*4+x*4] = 255;
                }
            }
        }
    }
#endif
    rc = GpiDrawBits (hps_begin_paint,
                      surface->pixels,
                      &(surface->bitmap_info),
                      4,
                      aptlPoints,
                      ROP_SRCCOPY,
                      BBO_IGNORE);

    if (rc != GPI_OK) {
        /* if GpiDrawBits () failed then this is most likely because the
         * display driver could not handle a 32bit bitmap. So we need to
         * - create a buffer that only contains 3 bytes per pixel
         * - change the bitmap info header to contain 24bit
         * - pass the new buffer to GpiDrawBits () again
         * - clean up the new buffer
         */
        BITMAPINFOHEADER2 bmpheader;
        unsigned char *pchPixBuf, *pchPixSource;
        void *pBufStart;
        ULONG ulPixels;

        /* allocate temporary pixel buffer */
        pchPixBuf = (unsigned char *) malloc (3 * surface->bitmap_info.cx *
                                              surface->bitmap_info.cy);
        pchPixSource = surface->pixels; /* start at beginning of pixel buffer */
        pBufStart = pchPixBuf; /* remember beginning of the new pixel buffer */

        /* copy the first three bytes for each pixel but skip over the fourth */
        for (ulPixels = 0; ulPixels < surface->bitmap_info.cx * surface->bitmap_info.cy; ulPixels++)
        {
            /* copy BGR from source buffer */
            *pchPixBuf++ = *pchPixSource++;
            *pchPixBuf++ = *pchPixSource++;
            *pchPixBuf++ = *pchPixSource++;
            pchPixSource++; /* jump over alpha channel in source buffer */
        }

        /* jump back to start of the buffer for display and cleanup */
        pchPixBuf = pBufStart;

        /* set up the bitmap header, but this time with 24bit depth only */
        memset (&bmpheader, 0, sizeof (bmpheader));
        bmpheader.cbFix = sizeof (BITMAPINFOHEADER2);
        bmpheader.cx = surface->bitmap_info.cx;
        bmpheader.cy = surface->bitmap_info.cy;
        bmpheader.cPlanes = surface->bitmap_info.cPlanes;
        bmpheader.cBitCount = 24;
        rc = GpiDrawBits (hps_begin_paint,
                          pchPixBuf,
                          (PBITMAPINFO2)&bmpheader,
                          4,
                          aptlPoints,
                          ROP_SRCCOPY,
                          BBO_IGNORE);

        free (pchPixBuf);
    }

    /* Restore Y inversion */
    GpiEnableYInversion (hps_begin_paint, lOldYInversion);
}

static void
_cairo_os2_surface_get_pixels_from_screen (cairo_os2_surface_t *surface,
                                           HPS                  hps_begin_paint,
                                           PRECTL               prcl_begin_paint_rect)
{
    HPS hps;
    HDC hdc;
    HAB hab;
    SIZEL sizlTemp;
    HBITMAP hbmpTemp;
    BITMAPINFO2 bmi2Temp;
    POINTL aptlPoints[4];
    int y;
    unsigned char *pchTemp;

    /* To copy pixels from screen to our buffer, we do the following steps:
     *
     * - Blit pixels from screen to a HBITMAP:
     *   -- Create Memory Device Context
     *   -- Create a PS into it
     *   -- Create a HBITMAP
     *   -- Select HBITMAP into memory PS
     *   -- Blit dirty pixels from screen to HBITMAP
     * - Copy HBITMAP lines (pixels) into our buffer
     * - Free resources
     *
     * These steps will require an Anchor Block (HAB). However,
     * WinQUeryAnchorBlock () documentation says that HAB is not
     * used in current OS/2 implementations, OS/2 deduces all information
     * it needs from the TID. Anyway, we'd be in trouble if we'd have to
     * get a HAB where we only know a HPS...
     * So, we'll simply use a fake HAB.
     */

    hab = (HAB) 1; /* OS/2 doesn't really use HAB... */

    /* Create a memory device context */
    hdc = DevOpenDC (hab, OD_MEMORY,"*",0L, NULL, NULLHANDLE);
    if (!hdc) {
        return;
    }

    /* Create a memory PS */
    sizlTemp.cx = prcl_begin_paint_rect->xRight - prcl_begin_paint_rect->xLeft;
    sizlTemp.cy = prcl_begin_paint_rect->yTop - prcl_begin_paint_rect->yBottom;
    hps = GpiCreatePS (hab,
                       hdc,
                       &sizlTemp,
                       PU_PELS | GPIT_NORMAL | GPIA_ASSOC);
    if (!hps) {
        DevCloseDC (hdc);
        return;
    }

    /* Create an uninitialized bitmap. */
    /* Prepare BITMAPINFO2 structure for our buffer */
    memset (&bmi2Temp, 0, sizeof (bmi2Temp));
    bmi2Temp.cbFix = sizeof (BITMAPINFOHEADER2);
    bmi2Temp.cx = sizlTemp.cx;
    bmi2Temp.cy = sizlTemp.cy;
    bmi2Temp.cPlanes = 1;
    bmi2Temp.cBitCount = 32;

    hbmpTemp = GpiCreateBitmap (hps,
                                (PBITMAPINFOHEADER2) &bmi2Temp,
                                0,
                                NULL,
                                NULL);

    if (!hbmpTemp) {
        GpiDestroyPS (hps);
        DevCloseDC (hdc);
        return;
    }

    /* Select the bitmap into the memory device context. */
    GpiSetBitmap (hps, hbmpTemp);

    /* Target coordinates (Noninclusive) */
    aptlPoints[0].x = 0;
    aptlPoints[0].y = 0;

    aptlPoints[1].x = sizlTemp.cx;
    aptlPoints[1].y = sizlTemp.cy;

    /* Source coordinates (Inclusive) */
    aptlPoints[2].x = prcl_begin_paint_rect->xLeft;
    aptlPoints[2].y = surface->bitmap_info.cy - prcl_begin_paint_rect->yBottom;

    aptlPoints[3].x = prcl_begin_paint_rect->xRight;
    aptlPoints[3].y = surface->bitmap_info.cy - prcl_begin_paint_rect->yTop;

    /* Blit pixels from screen to bitmap */
    GpiBitBlt (hps,
               hps_begin_paint,
               4,
               aptlPoints,
               ROP_SRCCOPY,
               BBO_IGNORE);

    /* Now we have to extract the pixels from the bitmap. */
    pchTemp =
        surface->pixels +
        (prcl_begin_paint_rect->yBottom)*surface->bitmap_info.cx*4 +
        prcl_begin_paint_rect->xLeft*4;
    for (y = 0; y < sizlTemp.cy; y++) {
        /* Get one line of pixels */
        GpiQueryBitmapBits (hps,
                            sizlTemp.cy - y - 1, /* lScanStart */
                            1,                   /* lScans */
                            pchTemp,
                            &bmi2Temp);

        /* Go for next line */
        pchTemp += surface->bitmap_info.cx*4;
    }

    /* Clean up resources */
    GpiSetBitmap (hps, (HBITMAP) NULL);
    GpiDeleteBitmap (hbmpTemp);
    GpiDestroyPS (hps);
    DevCloseDC (hdc);
}

static cairo_status_t
_cairo_os2_surface_acquire_source_image (void                   *abstract_surface,
                                         cairo_image_surface_t **image_out,
                                         void                  **image_extra)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) abstract_surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;
    }

    DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT);

    /* Increase lend counter */
    local_os2_surface->pixel_array_lend_count++;

    *image_out = local_os2_surface->image_surface;
    *image_extra = NULL;

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_os2_surface_release_source_image (void                  *abstract_surface,
                                         cairo_image_surface_t *image,
                                         void                  *image_extra)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) abstract_surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return;
    }

    /* Decrease Lend counter! */
    DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT);

    if (local_os2_surface->pixel_array_lend_count > 0)
        local_os2_surface->pixel_array_lend_count--;
    DosPostEventSem (local_os2_surface->hev_pixel_array_came_back);

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);
    return;
}

static cairo_status_t
_cairo_os2_surface_acquire_dest_image (void                     *abstract_surface,
                                       cairo_rectangle_int16_t  *interest_rect,
                                       cairo_image_surface_t   **image_out,
                                       cairo_rectangle_int16_t  *image_rect,
                                       void                    **image_extra)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) abstract_surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;
    }

    DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT);

    /* Increase lend counter */
    local_os2_surface->pixel_array_lend_count++;

    *image_out = local_os2_surface->image_surface;
    *image_extra = NULL;

    image_rect->x = 0;
    image_rect->y = 0;
    image_rect->width = local_os2_surface->bitmap_info.cx;
    image_rect->height = local_os2_surface->bitmap_info.cy;

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_os2_surface_release_dest_image (void                    *abstract_surface,
                                       cairo_rectangle_int16_t *interest_rect,
                                       cairo_image_surface_t   *image,
                                       cairo_rectangle_int16_t *image_rect,
                                       void                    *image_extra)
{
    cairo_os2_surface_t *local_os2_surface;
    RECTL rclToBlit;

    local_os2_surface = (cairo_os2_surface_t *) abstract_surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return;
    }

    /* So, we got back the image, and if all goes well, then
     * something has been changed inside the interest_rect.
     * So, we blit it to the screen!
     */

    if (local_os2_surface->blit_as_changes) {
        /* Get mutex, we'll work with the pixel array! */
        if (DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT)!=NO_ERROR) {
            /* Could not get mutex! */
            return;
        }

        if (local_os2_surface->hwnd_client_window) {
            /* We know the HWND, so let's invalidate the window region,
             * so the application will redraw itself, using the
             * cairo_os2_surface_refresh_window () API from its own PM thread.
             *
             * This is the safe method, which should be preferred every time.
             */
            rclToBlit.xLeft = interest_rect->x;
            rclToBlit.xRight = interest_rect->x+interest_rect->width; /* Noninclusive */
            rclToBlit.yTop = local_os2_surface->bitmap_info.cy - (interest_rect->y);
            rclToBlit.yBottom = local_os2_surface->bitmap_info.cy - (interest_rect->y+interest_rect->height); /* Noninclusive */

            WinInvalidateRect (local_os2_surface->hwnd_client_window,
                               &rclToBlit,
                               FALSE);
        } else {
            /* We don't know the HWND, so try to blit the pixels from here!
             * Please note that it can be problematic if this is not the PM thread!
             *
             * It can cause internal PM stuffs to be scewed up, for some reason.
             * Please always tell the HWND to the surface using the
             * cairo_os2_surface_set_hwnd () API, and call cairo_os2_surface_refresh_window ()
             * from your WM_PAINT, if it's possible!
             */
            rclToBlit.xLeft = interest_rect->x;
            rclToBlit.xRight = interest_rect->x+interest_rect->width; /* Noninclusive */
            rclToBlit.yBottom = interest_rect->y;
            rclToBlit.yTop = interest_rect->y+interest_rect->height; /* Noninclusive */
            /* Now blit there the stuffs! */
            _cairo_os2_surface_blit_pixels (local_os2_surface,
                                            local_os2_surface->hps_client_window,
                                            &rclToBlit);
        }

        DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);
    }
    /* Also decrease Lend counter! */
    DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT);

    if (local_os2_surface->pixel_array_lend_count > 0)
        local_os2_surface->pixel_array_lend_count--;
    DosPostEventSem (local_os2_surface->hev_pixel_array_came_back);

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);
}

static cairo_int_status_t
_cairo_os2_surface_get_extents (void                    *abstract_surface,
                                cairo_rectangle_int16_t *rectangle)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) abstract_surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;
    }

    rectangle->x = 0;
    rectangle->y = 0;
    rectangle->width  = local_os2_surface->bitmap_info.cx;
    rectangle->height = local_os2_surface->bitmap_info.cy;

    return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *
cairo_os2_surface_create (HPS hps_client_window,
                          int width,
                          int height)
{
    cairo_os2_surface_t *local_os2_surface;
    int rc;

    /* Check the size of the window */
    if ((width <= 0) ||
        (height <= 0))
    {
        /* Invalid window size! */
        _cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t *) &_cairo_surface_nil;
    }

    local_os2_surface = (cairo_os2_surface_t *) malloc (sizeof (cairo_os2_surface_t));
    if (!local_os2_surface) {
        /* Not enough memory! */
        _cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t *) &_cairo_surface_nil;
    }

    /* Initialize the OS/2 specific parts of the surface! */

    /* Create mutex semaphore */
    rc = DosCreateMutexSem (NULL,
                            &(local_os2_surface->hmtx_use_private_fields),
                            0,
                            FALSE);
    if (rc != NO_ERROR) {
        /* Could not create mutex semaphore! */
        _cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t *) &_cairo_surface_nil;
    }

    /* Save PS handle */
    local_os2_surface->hps_client_window = hps_client_window;

    /* Defaults */
    local_os2_surface->hwnd_client_window = NULLHANDLE;
    local_os2_surface->blit_as_changes = TRUE;
    local_os2_surface->pixel_array_lend_count = 0;
    rc = DosCreateEventSem (NULL,
                            &(local_os2_surface->hev_pixel_array_came_back),
                            0,
                            FALSE);

    if (rc != NO_ERROR) {
        /* Could not create event semaphore! */
        DosCloseMutexSem (local_os2_surface->hmtx_use_private_fields);
        free (local_os2_surface);
        _cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t *) &_cairo_surface_nil;
    }

    /* Prepare BITMAPINFO2 structure for our buffer */
    memset (&(local_os2_surface->bitmap_info), 0, sizeof (local_os2_surface->bitmap_info));
    local_os2_surface->bitmap_info.cbFix = sizeof (BITMAPINFOHEADER2);
    local_os2_surface->bitmap_info.cx = width;
    local_os2_surface->bitmap_info.cy = height;
    local_os2_surface->bitmap_info.cPlanes = 1;
    local_os2_surface->bitmap_info.cBitCount = 32;

    /* Allocate memory for pixels */
    local_os2_surface->pixels = (unsigned char *) malloc (width * height * 4);
    if (!(local_os2_surface->pixels)) {
        /* Not enough memory for the pixels! */
        DosCloseEventSem (local_os2_surface->hev_pixel_array_came_back);
        DosCloseMutexSem (local_os2_surface->hmtx_use_private_fields);
        free (local_os2_surface);
        _cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t *) &_cairo_surface_nil;
    }

    /* This is possibly not needed, malloc'd space is
     * usually zero'd out!
     */
    /*
     memset (local_os2_surface->pixels, 0x00, swpTemp.cx * swpTemp.cy * 4);
     */

    /* Create image surface from pixel array */
    local_os2_surface->image_surface = (cairo_image_surface_t *)
        cairo_image_surface_create_for_data (local_os2_surface->pixels,
                                             CAIRO_FORMAT_ARGB32,
                                             width,      /* Width */
                                             height,     /* Height */
                                             width * 4); /* Rowstride */

    if (local_os2_surface->image_surface->base.status) {
        /* Could not create image surface! */
        free (local_os2_surface->pixels);
        DosCloseEventSem (local_os2_surface->hev_pixel_array_came_back);
        DosCloseMutexSem (local_os2_surface->hmtx_use_private_fields);
        free (local_os2_surface);
        _cairo_error (CAIRO_STATUS_NO_MEMORY);
        return (cairo_surface_t *) &_cairo_surface_nil;
    }

    /* Initialize base surface */
    _cairo_surface_init (&local_os2_surface->base,
                         &cairo_os2_surface_backend,
                         _cairo_content_from_format (CAIRO_FORMAT_ARGB32));

    return (cairo_surface_t *)local_os2_surface;
}

int
cairo_os2_surface_set_size (cairo_surface_t *surface,
                            int              new_width,
                            int              new_height,
                            int              timeout)
{
    cairo_os2_surface_t *local_os2_surface;
    unsigned char *pchNewPixels;
    int rc;
    cairo_image_surface_t *pNewImageSurface;

    local_os2_surface = (cairo_os2_surface_t *) surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;
    }

    if ((new_width <= 0) ||
        (new_height <= 0))
    {
        /* Invalid size! */
        return CAIRO_STATUS_NO_MEMORY;
    }

    /* Allocate memory for new stuffs */
    pchNewPixels = (unsigned char *) malloc (new_width * new_height * 4);
    if (!pchNewPixels) {
        /* Not enough memory for the pixels!
         * Everything remains the same!
         */
        return CAIRO_STATUS_NO_MEMORY;
    }

    /* This is possibly not needed, malloc'd space is usually
     * already zero'd out!
     */
    /*
     memset (pchNewPixels, 0x00, new_width * new_height * 4);
     */

    /* Create image surface from new pixel array */
    pNewImageSurface = (cairo_image_surface_t *)
        cairo_image_surface_create_for_data (pchNewPixels,
                                             CAIRO_FORMAT_ARGB32,
                                             new_width,      /* Width */
                                             new_height,     /* Height */
                                             new_width * 4); /* Rowstride */

    if (pNewImageSurface->base.status) {
        /* Could not create image surface!
         * Everything remains the same!
         */
        free (pchNewPixels);
        return CAIRO_STATUS_NO_MEMORY;
    }

    /* Okay, new memory allocated, so it's time to swap old buffers
     * to new ones!
     */
    if (DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT)!=NO_ERROR) {
        /* Could not get mutex!
         * Everything remains the same!
         */
        cairo_surface_destroy ((cairo_surface_t *) pNewImageSurface);
        free (pchNewPixels);
        return CAIRO_STATUS_NO_MEMORY;
    }

    /* We have to make sure that we won't destroy a surface which
     * is lent to some other code (Cairo is drawing into it)!
     */
    while (local_os2_surface->pixel_array_lend_count > 0) {
        ULONG ulPostCount;
        DosResetEventSem (local_os2_surface->hev_pixel_array_came_back, &ulPostCount);
        DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);
        /* Wait for somebody to return the pixels! */
        rc = DosWaitEventSem (local_os2_surface->hev_pixel_array_came_back, timeout);
        if (rc != NO_ERROR) {
            /* Either timeout or something wrong... Exit. */
            cairo_surface_destroy ((cairo_surface_t *) pNewImageSurface);
            free (pchNewPixels);
            return CAIRO_STATUS_NO_MEMORY;
        }
        /* Okay, grab mutex and check counter again! */
        if (DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT)
            != NO_ERROR)
        {
            /* Could not get mutex!
             * Everything remains the same!
             */
            cairo_surface_destroy ((cairo_surface_t *) pNewImageSurface);
            free (pchNewPixels);
            return CAIRO_STATUS_NO_MEMORY;
        }
    }

    /* Destroy old image surface */
    cairo_surface_destroy ((cairo_surface_t *) (local_os2_surface->image_surface));
    /* Destroy old pixel buffer */
    free (local_os2_surface->pixels);
    /* Set new image surface */
    local_os2_surface->image_surface = pNewImageSurface;
    /* Set new pixel buffer */
    local_os2_surface->pixels = pchNewPixels;
    /* Change bitmap2 structure */
    local_os2_surface->bitmap_info.cx = new_width;
    local_os2_surface->bitmap_info.cy = new_height;

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);
    return CAIRO_STATUS_SUCCESS;
}

void
cairo_os2_surface_refresh_window (cairo_surface_t *surface,
                                  HPS              hps_begin_paint,
                                  PRECTL           prcl_begin_paint_rect)
{
    cairo_os2_surface_t *local_os2_surface;
    RECTL rclTemp;

    local_os2_surface = (cairo_os2_surface_t *) surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return;
    }

    /* Manage defaults (NULLs) */
    if (!hps_begin_paint)
        hps_begin_paint = local_os2_surface->hps_client_window;

    if (prcl_begin_paint_rect == NULL) {
        /* Update the whole window! */
        rclTemp.xLeft = 0;
        rclTemp.xRight = local_os2_surface->bitmap_info.cx;
        rclTemp.yTop = local_os2_surface->bitmap_info.cy;
        rclTemp.yBottom = 0;
    } else {
        /* Use the rectangle we got passed as parameter! */
        rclTemp.xLeft = prcl_begin_paint_rect->xLeft;
        rclTemp.xRight = prcl_begin_paint_rect->xRight;
        rclTemp.yTop = local_os2_surface->bitmap_info.cy - prcl_begin_paint_rect->yBottom;
        rclTemp.yBottom = local_os2_surface->bitmap_info.cy - prcl_begin_paint_rect->yTop ;
    }

    /* Get mutex, we'll work with the pixel array! */
    if (DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT)
        != NO_ERROR)
    {
        /* Could not get mutex! */
        return;
    }

    if ((local_os2_surface->dirty_area_present) &&
        (local_os2_surface->rcl_dirty_area.xLeft == rclTemp.xLeft) &&
        (local_os2_surface->rcl_dirty_area.xRight == rclTemp.xRight) &&
        (local_os2_surface->rcl_dirty_area.yTop == rclTemp.yTop) &&
        (local_os2_surface->rcl_dirty_area.yBottom == rclTemp.yBottom))
    {
        /* Aha, this call was because of a dirty area, so in this case we
         * have to blit the pixels from the screen to the surface!
         */
        local_os2_surface->dirty_area_present = FALSE;
        _cairo_os2_surface_get_pixels_from_screen (local_os2_surface,
                                                   hps_begin_paint,
                                                   &rclTemp);
    } else {
        /* Okay, we have the surface, have the HPS
         * (might be from WinBeginPaint () or from WinGetPS () )
         * Now blit there the stuffs!
         */
        _cairo_os2_surface_blit_pixels (local_os2_surface,
                                        hps_begin_paint,
                                        &rclTemp);
    }

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);
}

static cairo_status_t
_cairo_os2_surface_finish (void *abstract_surface)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) abstract_surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;
    }

    DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT);

    /* Destroy old image surface */
    cairo_surface_destroy ((cairo_surface_t *) (local_os2_surface->image_surface));
    /* Destroy old pixel buffer */
    free (local_os2_surface->pixels);
    DosCloseMutexSem (local_os2_surface->hmtx_use_private_fields);
    DosCloseEventSem (local_os2_surface->hev_pixel_array_came_back);

    /* The memory itself will be free'd by the cairo_surface_destroy ()
     * who called us.
     */

    return CAIRO_STATUS_SUCCESS;
}

void
cairo_os2_surface_set_hwnd (cairo_surface_t *surface,
                            HWND             hwnd_client_window)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return;
    }

    if (DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT)
        != NO_ERROR)
    {
        /* Could not get mutex! */
        return;
    }

    local_os2_surface->hwnd_client_window = hwnd_client_window;

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);
}

void
cairo_os2_surface_set_manual_window_refresh (cairo_surface_t *surface,
                                             cairo_bool_t     manual_refresh)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return;
    }

    local_os2_surface->blit_as_changes = !manual_refresh;
}

cairo_bool_t
cairo_os2_surface_get_manual_window_refresh (cairo_surface_t *surface)
{
    cairo_os2_surface_t *local_os2_surface;

    local_os2_surface = (cairo_os2_surface_t *) surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return FALSE;
    }

    return !(local_os2_surface->blit_as_changes);
}

static cairo_status_t
_cairo_os2_surface_mark_dirty_rectangle (void *surface,
                                         int   x,
                                         int   y,
                                         int   width,
                                         int   height)
{
    cairo_os2_surface_t *local_os2_surface;
    RECTL rclToBlit;

    local_os2_surface = (cairo_os2_surface_t *) surface;
    if ((!local_os2_surface) ||
        (local_os2_surface->base.backend != &cairo_os2_surface_backend))
    {
        /* Invalid parameter (wrong surface)! */
        return CAIRO_STATUS_SURFACE_TYPE_MISMATCH;
    }

    /* Get mutex, we'll work with the pixel array! */
    if (DosRequestMutexSem (local_os2_surface->hmtx_use_private_fields, SEM_INDEFINITE_WAIT)
        != NO_ERROR)
    {
        /* Could not get mutex! */
        return CAIRO_STATUS_NO_MEMORY;
    }

    /* Check for defaults */
    if (width < 0)
        width = local_os2_surface->bitmap_info.cx;
    if (height < 0)
        height = local_os2_surface->bitmap_info.cy;

    if (local_os2_surface->hwnd_client_window) {
        /* We know the HWND, so let's invalidate the window region,
         * so the application will redraw itself, using the
         * cairo_os2_surface_refresh_window () API from its own PM thread.
         * From that function we'll note that it's not a redraw but a
         * dirty-rectangle deal stuff, so we'll handle the things from
         * there.
         *
         * This is the safe method, which should be preferred every time.
         */
        rclToBlit.xLeft = x;
        rclToBlit.xRight = x + width; /* Noninclusive */
        rclToBlit.yTop = local_os2_surface->bitmap_info.cy - (y);
        rclToBlit.yBottom = local_os2_surface->bitmap_info.cy - (y + height); /* Noninclusive */

#if 0
        if (local_os2_surface->dirty_area_present) {
            /* Yikes, there is already a dirty area which should be
             * cleaned up, but we'll overwrite it. Sorry.
             * TODO: Something clever should be done here.
             */
        }
#endif

        /* Set up dirty area reminder stuff */
        memcpy (&(local_os2_surface->rcl_dirty_area), &rclToBlit, sizeof (RECTL));
        local_os2_surface->dirty_area_present = TRUE;

        /* Invalidate window area */
        WinInvalidateRect (local_os2_surface->hwnd_client_window,
                           &rclToBlit,
                           FALSE);
    } else {
        /* We don't know the HWND, so try to blit the pixels from here!
         * Please note that it can be problematic if this is not the PM thread!
         *
         * It can cause internal PM stuffs to be scewed up, for some reason.
         * Please always tell the HWND to the surface using the
         * cairo_os2_surface_set_hwnd () API, and call cairo_os2_surface_refresh_window ()
         * from your WM_PAINT, if it's possible!
         */

        rclToBlit.xLeft = x;
        rclToBlit.xRight = x + width; /* Noninclusive */
        rclToBlit.yBottom = y;
        rclToBlit.yTop = y + height; /* Noninclusive */
        /* Now get the pixels from the screen! */
        _cairo_os2_surface_get_pixels_from_screen (local_os2_surface,
                                                   local_os2_surface->hps_client_window,
                                                   &rclToBlit);
    }

    DosReleaseMutexSem (local_os2_surface->hmtx_use_private_fields);

    return CAIRO_STATUS_SUCCESS;
}

static const cairo_surface_backend_t cairo_os2_surface_backend = {
    CAIRO_SURFACE_TYPE_OS2,
    NULL, /* create_similar */
    _cairo_os2_surface_finish,
    _cairo_os2_surface_acquire_source_image,
    _cairo_os2_surface_release_source_image,
    _cairo_os2_surface_acquire_dest_image,
    _cairo_os2_surface_release_dest_image,
    NULL, /* clone_similar */
    NULL, /* composite */
    NULL, /* fill_rectangles */
    NULL, /* composite_trapezoids */
    NULL, /* copy_page */
    NULL, /* show_page */
    NULL, /* set_clip_region */
    NULL, /* intersect_clip_path */
    _cairo_os2_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    NULL, /* flush */
    _cairo_os2_surface_mark_dirty_rectangle,
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
    NULL, /* show_glyphs */
    NULL  /* snapshot */
};

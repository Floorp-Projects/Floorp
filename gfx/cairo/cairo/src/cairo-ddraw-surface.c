/* -*- Mode: c; tab-width: 8; c-basic-offset: 4; indent-tabs-mode: t; -*- */
/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 NVIDIA Corporation
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
 * The Initial Developer of the Original Code is NVIDIA Corporation.
 *
 * Contributor(s):
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "cairoint.h"

#if CAIRO_HAS_DDRAW_SURFACE

#include "cairo-clip-private.h"
#include "cairo-ddraw-private.h"

#include <windows.h>
#include <ddraw.h>
#include <stddef.h>
#include <stdarg.h>
#include <cmnintrin.h>

#ifndef DDLOCK_WAITNOTBUSY
#error Your DirectDraw header is too old.  Copy in ddraw.h from the WM6 SDK over your SDK's ddraw.h.
#error Otherwise, you can fix this code to use IDirectDraw4 and IDirectDrawSurface5.
#endif

/* DirectDraw helper macros */

#define DDCALL0(fn,p)           ((p)->lpVtbl->fn((p)))
#define DDCALL1(fn,p,a)         ((p)->lpVtbl->fn((p),(a)))
#define DDCALL2(fn,p,a,b)       ((p)->lpVtbl->fn((p),(a),(b)))
#define DDCALL3(fn,p,a,b,c)     ((p)->lpVtbl->fn((p),(a),(b),(c)))
#define DDCALL4(fn,p,a,b,c,d)   ((p)->lpVtbl->fn((p),(a),(b),(c),(d)))
#define DDCALL5(fn,p,a,b,c,d,e) ((p)->lpVtbl->fn((p),(a),(b),(c),(d),(e)))

#define IUAddRef(p)                   DDCALL0(AddRef,p)
#define IURelease(p)                  DDCALL0(Release,p)

#define IDDCreateClipper(p,a,b,c)     DDCALL3(CreateClipper,p,a,b,c)
#define IDDCreateSurface(p,a,b,c)     DDCALL3(CreateSurface,p,a,b,c)
#define IDDSetCooperativeLevel(p,a,b) DDCALL2(SetCooperativeLevel,p,a,b)

#define IDDCSetClipList(p,a,b)        DDCALL2(SetClipList,p,a,b)

#define IDDSBlt(p,a,b,c,d,e)          DDCALL5(Blt,p,a,b,c,d,e)
#define IDDSGetDC(p,a)                DDCALL1(GetDC,p,a)
#define IDDSLock(p,a,b,c,d)           DDCALL4(Lock,p,a,b,c,d)
#define IDDSReleaseDC(p,a)            DDCALL1(ReleaseDC,p,a)
#define IDDSSetClipper(p,a)           DDCALL1(SetClipper,p,a)
#define IDDSUnlock(p,a)               DDCALL1(Unlock,p,a)

static const cairo_surface_backend_t _cairo_ddraw_surface_backend;

/* debugging flags */
#undef CAIRO_DDRAW_DEBUG
#undef CAIRO_DDRAW_DEBUG_VERBOSE
#undef CAIRO_DDRAW_LOG
#undef CAIRO_DDRAW_LOG_TO_STORAGE
#undef CAIRO_DDRAW_OGL_FONT_STATS
#undef CAIRO_DDRAW_LOCK_TIMES

static void _cairo_ddraw_log (const char *fmt, ...)
{
    va_list ap;
#ifdef CAIRO_DDRAW_LOG_TO_STORAGE
    FILE *fp = fopen ("\\Storage Card\\debug.txt", "a");
    if (fp) {
	va_start (ap, fmt);
	vfprintf (fp, fmt, ap);
	va_end (ap);
	fclose (fp);
    }
#endif

    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
}

/**
 * _cairo_ddraw_print_ddraw_error:
 * @context: context string to display along with the error
 * @hr: HRESULT code
 *
 * Helper function to dump out a human readable form of the
 * current error code.
 *
 * Return value: A cairo status code for the error code
 **/
static cairo_status_t
_cairo_ddraw_print_ddraw_error (const char *context, HRESULT hr)
{
    /*XXX make me pretty */
    _cairo_ddraw_log ("%s: DirectDraw error 0x%08x\n", context, hr);

    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

#ifdef CAIRO_DDRAW_LOCK_TIMES

#define LIST_TIMERS(_) \
_(lock) \
_(unlock)\
_(glfinish) \
_(waitnative) \
_(flush) \
_(clone) \
_(destroy) \
_(growglyphcache) \
_(addglyph) \
_(acquiresrc) \
_(acquiredst) \
_(getddraw) \
_(getimage) \
_(swfill)

#define DECLARE_TIMERS(tmr) \
    static uint32_t _cairo_ddraw_timer_##tmr, _cairo_ddraw_timer_start_##tmr;
LIST_TIMERS(DECLARE_TIMERS)
#undef DECLARE_TIMERS

static void
_cairo_ddraw_dump_timers (void)
{
#define PRINT_TIMERS(tmr) \
    _cairo_ddraw_log ("%20s: %10u\n", #tmr, _cairo_ddraw_timer_##tmr);
    LIST_TIMERS(PRINT_TIMERS)
#undef PRINT_TIMERS
}

#undef LIST_TIMERS

#define START_TIMER(tmr) (_cairo_ddraw_timer_start_##tmr = GetTickCount ())
#define END_TIMER(tmr) \
    (_cairo_ddraw_timer_##tmr += GetTickCount () - \
      _cairo_ddraw_timer_start_##tmr)

#else /* CAIRO_DDRAW_LOCK_TIMES */

#define START_TIMER(tmr)
#define END_TIMER(tmr)

#endif /* CAIRO_DDRAW_LOCK_TIMES */

#ifdef CAIRO_DDRAW_USE_GL

/* at least one of these must be enabled, both is ok */
/* #define CAIRO_DDRAW_OGL_BINARY_SHADER    GL_NVIDIA_PLATFORM_BINARY_NV */
#define CAIRO_DDRAW_OGL_SOURCE_SHADER

#undef CAIRO_DDRAW_OGL_FC_UNIFIED

#define CAIRO_DDRAW_OGL_FC_WIDTH            128
#define CAIRO_DDRAW_OGL_FC_INITIAL_HEIGHT    64
#define CAIRO_DDRAW_OGL_FC_HEIGHT_INCREMENT  64
#define CAIRO_DDRAW_OGL_FC_MAX_HEIGHT       512
#define CAIRO_DDRAW_OGL_FC_NUM_PARTITIONS     8
#define CAIRO_DDRAW_OGL_FC_GLYPH_INCREMENT   128

#define CAIRO_DDRAW_OGL_FC_PARTITION_HEIGHT \
    (CAIRO_DDRAW_OGL_FC_MAX_HEIGHT / CAIRO_DDRAW_OGL_FC_NUM_PARTITIONS)

/* note that the vbo needs to be at least the size of a quad */
#define CAIRO_DDRAW_OGL_NUM_SCRATCH_BUFFERS          4
#define CAIRO_DDRAW_OGL_SCRATCH_VBO_SIZE          4096
#define CAIRO_DDRAW_OGL_SCRATCH_IBO_SIZE          1024

static uint32_t   _cairo_ddraw_ogl_surface_count = 0;
static EGLDisplay _cairo_ddraw_egl_dpy = EGL_NO_DISPLAY;
static EGLContext _cairo_ddraw_egl_dummy_ctx = EGL_NO_CONTEXT;
static EGLSurface _cairo_ddraw_egl_dummy_surface = EGL_NO_SURFACE;

static GLint _cairo_ddraw_ogl_max_texture_size;


static cairo_status_t _cairo_ddraw_ogl_init(void);

static cairo_status_t
_cairo_ddraw_egl_error (const char *context)
{
    /*XXX make pretty */
    _cairo_ddraw_log ("%s returned EGL error code 0x%x\n",
		      context, eglGetError ());
    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

static cairo_status_t
_cairo_ddraw_check_ogl_error (const char *context)
{
    GLenum err = glGetError ();

    if (err == GL_NO_ERROR)
	return CAIRO_STATUS_SUCCESS;

    /*XXX make pretty */
    _cairo_ddraw_log ("%s returned OpenGL ES error code 0x%x\n", context, err);
    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

#ifdef CAIRO_DDRAW_LOG
# define CHECK_OGL_ERROR(context) \
(_cairo_ddraw_check_ogl_error (context) ? 0 : _cairo_ddraw_log ("%s succeeded\n", context), 0)
#else
# ifdef CAIRO_DDRAW_DEBUG
# define CHECK_OGL_ERROR(context) _cairo_ddraw_check_ogl_error (context)
# else
# define CHECK_OGL_ERROR(context)
# endif
#endif

#endif /* CAIRO_DDRAW_USE_GL */

static cairo_status_t
_cairo_ddraw_surface_set_image_clip (cairo_ddraw_surface_t *surface)
{
    if (surface->image_clip_invalid) {
	surface->image_clip_invalid = FALSE;
	if (surface->has_clip_region) {
	    unsigned int serial =
		_cairo_surface_allocate_clip_serial (surface->image);
	    surface->has_image_clip = TRUE;
	    return _cairo_surface_set_clip_region (surface->image,
						   &surface->clip_region,
						   serial);
	} else {
	    surface->has_image_clip = FALSE;
	    return _cairo_surface_set_clip_region (surface->image,
						   NULL, 0);
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_surface_set_clip_list (cairo_ddraw_surface_t * surface)
{
    DWORD stack_data[CAIRO_STACK_BUFFER_SIZE / sizeof (DWORD)];
    cairo_rectangle_int_t extents;
    int num_rects;
    RGNDATA * rgn;
    DWORD size;
    cairo_status_t status;
    cairo_point_int_t offset;
    HRESULT hr;
    RECT * prect;
    int i;

    if (!surface->has_clip_region || !surface->clip_list_invalid)
	return CAIRO_STATUS_SUCCESS;

    surface->clip_list_invalid = FALSE;

    cairo_region_get_extents (&surface->clip_region, &extents);
    
    rgn = (RGNDATA *) stack_data;
    num_rects = cairo_region_num_rectangles (&surface->clip_region);
    
    size = sizeof (RGNDATAHEADER) + sizeof (RECT) * num_rects;
    if (size > sizeof (stack_data)) {
	if ((rgn = malloc (size)) == 0)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    offset.x = MAX (0, surface->origin.x);
    offset.y = MAX (0, surface->origin.y);
    
    rgn->rdh.dwSize = sizeof(RGNDATAHEADER);
    rgn->rdh.iType = RDH_RECTANGLES;
    rgn->rdh.nCount = num_rects;
    rgn->rdh.nRgnSize = num_rects * sizeof (RECT);
    rgn->rdh.rcBound.left = extents.x + offset.x;
    rgn->rdh.rcBound.top = extents.y + offset.y;
    rgn->rdh.rcBound.right = extents.x + extents.width + offset.x;
    rgn->rdh.rcBound.bottom = extents.y + extents.height + offset.y;
    
    prect = (RECT *) &rgn->Buffer;
    for (i = 0; i < num_rects; ++i) {
	cairo_rectangle_int_t rect;
	
	cairo_region_get_rectangle (&surface->clip_region, i, &rect);
	
	prect->left = rect.x + offset.x;
	prect->top = rect.y + offset.y;
	prect->right = rect.x + rect.width + offset.x;
	prect->bottom = rect.y + rect.height + offset.y;
	++prect;
    }
    
    status = CAIRO_STATUS_SUCCESS;
    
    if (FAILED(hr = IDDCSetClipList (surface->lpddc, rgn, 0)))
	status = _cairo_ddraw_print_ddraw_error ("_set_clip_list", hr);
    
    if (rgn != (RGNDATA *) stack_data)
	free (rgn);
    
    return status;
}

static cairo_status_t
_cairo_ddraw_surface_lock (cairo_ddraw_surface_t *surface)
{
    HRESULT hr;
    DDSURFACEDESC ddsd;
    cairo_image_surface_t * img = (cairo_image_surface_t *) surface->image;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (surface->root != surface) {
	cairo_ddraw_surface_t * root = surface->root;
	cairo_image_surface_t * root_img;

	if (!root->locked) {
	    cairo_status_t status =
		_cairo_ddraw_surface_lock (root);
	    if (status)
		return status;
	}

	root_img = (cairo_image_surface_t *) root->image;

	if (img) {
	    if (img->data == root_img->data + surface->data_offset &&
		img->stride == root_img->stride)
		return CAIRO_STATUS_SUCCESS;

	    cairo_surface_destroy (surface->image);
	}

	surface->data_offset =
	    MAX (0, surface->origin.y) * root_img->stride +
	    MAX (0, surface->origin.x) * 4;
	surface->image =
	    cairo_image_surface_create_for_data (root_img->data +
						 surface->data_offset,
						 surface->format,
						 surface->extents.width,
						 surface->extents.height,
						 root_img->stride);
	if (surface->image->status)
	    return surface->image->status;

	surface->has_image_clip = FALSE;
	surface->image_clip_invalid = surface->has_clip_region;

	return _cairo_ddraw_surface_set_image_clip (surface);
    }

    if (surface->locked)
	return CAIRO_STATUS_SUCCESS;

    ddsd.dwSize = sizeof (ddsd);
    START_TIMER(lock);
    if (FAILED(hr = IDDSLock (surface->lpdds, NULL, &ddsd,
			      DDLOCK_WAITNOTBUSY, NULL)))
	return _cairo_ddraw_print_ddraw_error ("_lock", hr);
    END_TIMER(lock);

    assert (ddsd.lXPitch == (surface->format == CAIRO_FORMAT_A8 ? 1 : 4));

    surface->locked = TRUE;

    if (img) {
	if (img->data == ddsd.lpSurface && img->stride == ddsd.lPitch)
	    return CAIRO_STATUS_SUCCESS;

	cairo_surface_destroy (surface->image);
    }

    surface->image =
	cairo_image_surface_create_for_data (ddsd.lpSurface,
					     surface->format,
					     ddsd.dwWidth,
					     ddsd.dwHeight,
					     ddsd.lPitch);

    if (surface->image->status)
	return surface->image->status;

    surface->has_image_clip = FALSE;
    surface->image_clip_invalid = surface->has_clip_region;

    return _cairo_ddraw_surface_set_image_clip (surface);
}

static inline cairo_status_t
_cairo_ddraw_surface_unlock (cairo_ddraw_surface_t *surface)
{
    cairo_ddraw_surface_t * root = surface->root;
    HRESULT hr;

    if (!root->locked)
	return CAIRO_STATUS_SUCCESS;

    START_TIMER(unlock);
    if (FAILED(hr = IDDSUnlock (root->lpdds, NULL)))
	return _cairo_ddraw_print_ddraw_error ("_unlock", hr);
    END_TIMER(unlock);

    root->locked = FALSE;

    return CAIRO_STATUS_SUCCESS;
}

static inline cairo_int_status_t
_cairo_ddraw_surface_set_clipper (cairo_ddraw_surface_t *surface)
{
    cairo_ddraw_surface_t * root = surface->root;
    LPDIRECTDRAWCLIPPER myclip =
	surface->has_clip_region ? surface->lpddc : NULL;

    if (root->installed_clipper != myclip) {
	HRESULT hr;
	if (FAILED(hr = IDDSSetClipper(root->lpdds, myclip)))
	    return _cairo_ddraw_print_ddraw_error ("_set_clipper", hr);
	root->installed_clipper = myclip;
    }

    return CAIRO_STATUS_SUCCESS;
}

static inline cairo_status_t
_cairo_ddraw_surface_reset_clipper (cairo_ddraw_surface_t *surface)
{
    cairo_ddraw_surface_t * root = surface->root;

    if (root->installed_clipper != NULL) {
	HRESULT hr;
	if (FAILED(hr = IDDSSetClipper(root->lpdds, NULL)))
	    return _cairo_ddraw_print_ddraw_error ("_reset_clipper", hr);
	root->installed_clipper = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}
#ifdef CAIRO_DDRAW_USE_GL
#define CAIRO_DDRAW_API_ENTRY_STATUS                                  \
    do {                                                              \
	int status;                                                   \
	if (status = _cairo_ddraw_ogl_make_current())		      \
	    return (status);					      \
    } while (0)
#define CAIRO_DDRAW_API_ENTRY_VOID do {                               \
	int status;                                                   \
	if (status = _cairo_ddraw_ogl_make_current())		      \
	    return;						      \
    } while (0)
#define CAIRO_DDRAW_API_ENTRY_SURFACE				      \
    do {				                              \
	int status;                                                   \
	if (status = _cairo_ddraw_ogl_make_current())		      \
	    return (_cairo_surface_create_in_error (status));         \
    } while (0)
#else
#define CAIRO_DDRAW_API_ENTRY_STATUS do {} while (0)
#define CAIRO_DDRAW_API_ENTRY_VOID  do {} while (0)
#define CAIRO_DDRAW_API_ENTRY_SURFACE do {} while (0)
#endif

#ifdef CAIRO_DDRAW_USE_GL
static inline cairo_status_t
_cairo_ddraw_ogl_make_current()
{
    /* we haven't started GL yet so don't worry about make current */
    if ( 0 == _cairo_ddraw_ogl_surface_count ) return CAIRO_STATUS_SUCCESS;

    if(!eglMakeCurrent (_cairo_ddraw_egl_dpy,
			_cairo_ddraw_egl_dummy_surface,
			_cairo_ddraw_egl_dummy_surface,
			_cairo_ddraw_egl_dummy_ctx)){
	EGLint status = eglGetError();

	/*XXX make pretty */
	_cairo_ddraw_log ("eglMakeCurrent returned EGL error code 0x%x\n", status);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }
    return CAIRO_STATUS_SUCCESS;
}

static inline void
_cairo_ddraw_ogl_flush (cairo_ddraw_surface_t *surface)
{
    cairo_ddraw_surface_t * root = surface->root;

    if (root->dirty) {
 	START_TIMER(glfinish);
  	glFinish ();
 	END_TIMER(glfinish);
	CHECK_OGL_ERROR ("glFinish");

	root->dirty = FALSE;
    }
}

static inline cairo_status_t
_cairo_ddraw_ogl_reference (cairo_ddraw_surface_t *surface)
{
    cairo_ddraw_surface_t * root = surface->root;

    if (!root->dirty) {
 	cairo_status_t status;
 	START_TIMER(waitnative);
  #if 0
  	eglWaitNative (EGL_EGL_CORE_NATIVE_ENGINE);
 	(void) status;
  #else
  	/* XXX our eglWaitNative is broken, do Lock() instead */
 	status = _cairo_ddraw_surface_lock (surface);
  
  	if (status)
  	    return status;
  #endif
 	END_TIMER(waitnative);
	root->dirty = TRUE;
    }

    return CAIRO_STATUS_SUCCESS;
}

#endif /* CAIRO_DDRAW_USE_GL */

static cairo_status_t
_cairo_ddraw_surface_flush (void *abstract_surface)
{
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_status_t status;

    CAIRO_DDRAW_API_ENTRY_STATUS;

    START_TIMER(flush);
  #ifdef CAIRO_DDRAW_USE_GL
    _cairo_ddraw_ogl_flush (surface);
  #endif

    status = _cairo_ddraw_surface_unlock (surface);
    END_TIMER(flush);
 
    return status;
}

cairo_status_t
_cairo_ddraw_surface_reset (cairo_surface_t *surface)
{
    return _cairo_surface_reset_clip (surface);
}

static cairo_surface_t *
_cairo_ddraw_surface_create_similar (void * abstract_surface,
				     cairo_content_t content,
				     int width,
				     int height)
{
    cairo_ddraw_surface_t * orig_surface =
	(cairo_ddraw_surface_t *) abstract_surface;
    cairo_surface_t * surface;

    CAIRO_DDRAW_API_ENTRY_SURFACE;

    surface =
	cairo_ddraw_surface_create (orig_surface->lpdd,
				    _cairo_format_from_content (content),
				    width,
				    height);

    if (surface->status) {
	/* if we can't allocate a surface, return NULL so we use an image */
	surface = NULL;
    }

    return surface;
}

static cairo_status_t
_cairo_ddraw_surface_clone_similar (void * abstract_surface,
				    cairo_surface_t * src,
				    int src_x,
				    int src_y,
				    int width,
				    int height,
				    int *clone_offset_x,
				    int *clone_offset_y,
				    cairo_surface_t **clone_out)
{
    cairo_ddraw_surface_t * ddraw_surface =
	(cairo_ddraw_surface_t *) abstract_surface;
    cairo_surface_t * surface;
    cairo_image_surface_t * image = NULL;
    void * image_extra;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    unsigned char * srcptr;
    unsigned char * dstptr;
    int srcstride;
    int dststride;
    int lines;
    int bytes;

    CAIRO_DDRAW_API_ENTRY_STATUS;

    if (src->backend == ddraw_surface->base.backend) {
	/* return a reference */
	*clone_offset_x = *clone_offset_y = 0;
	*clone_out = cairo_surface_reference (src);
	return CAIRO_STATUS_SUCCESS;
    }

    /* otherwise, make a new surface with the subrect */
    surface =
	_cairo_ddraw_surface_create_similar (abstract_surface,
					     src->content,
					     width,
					     height);

    if (surface == NULL)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    ddraw_surface = (cairo_ddraw_surface_t *) surface;

    if ((status =
	 _cairo_surface_acquire_source_image (src, &image, &image_extra)))
	goto FAIL;

    START_TIMER(clone);
#ifdef CAIRO_DDRAW_USE_GL
    _cairo_ddraw_ogl_flush (ddraw_surface);
#endif

    if ((status = _cairo_ddraw_surface_lock (ddraw_surface)))
	goto FAIL;
    END_TIMER(clone);

    lines = height;
    dststride = cairo_image_surface_get_stride (ddraw_surface->image);
    dstptr = cairo_image_surface_get_data (ddraw_surface->image);
    srcstride = image->stride;

    switch (image->format) {
    case CAIRO_FORMAT_A1:
	/* convert to a8 */
	srcptr = image->data + src_y * srcstride + (src_x >> 5);
	while (lines) {
	    uint32_t * srcword = (uint32_t *) srcptr;
	    uint32_t word = *srcword++;
	    uint32_t mask = 1UL << (src_x & 31);
	    unsigned char * dp = dstptr;

	    bytes = width;
	    while (bytes) {
		*dp++ = word & mask ? 0xff : 0x00;
		mask <<= 1;
		if (mask == 0) {
		    word = *srcword++;
		    mask = 1;
		}
		--bytes;
	    }
	    dstptr += dststride;
	    srcptr += srcstride;
	    --lines;
	}
	status = CAIRO_STATUS_SUCCESS;
	goto FAIL;
    default:
	assert (0);
	status = CAIRO_INT_STATUS_UNSUPPORTED;
	goto FAIL;
    case CAIRO_FORMAT_A8:
	srcptr = image->data + src_y * srcstride + src_x;
	bytes = width;
	break;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	srcptr = image->data + src_y * srcstride + (src_x << 2);
	bytes = width << 2;
	break;
    }

    while (lines) {
	memcpy (dstptr, srcptr, bytes);
	dstptr += dststride;
	srcptr += srcstride;
	--lines;
    }

 FAIL:
    if (image)
	_cairo_surface_release_source_image (src, image, image_extra);

    if (status) {
	cairo_surface_destroy (surface);
	return status;
    }

    *clone_out = surface;
    *clone_offset_x = src_x;
    *clone_offset_y = src_y;

    return CAIRO_STATUS_SUCCESS;
}


#ifdef CAIRO_DDRAW_USE_GL


/* Yes, this uses OpenGL ES 2.0 to render to DirectDraw suraces.
 * Since OpenGL ES 2.0 uses binary shaders, it is by definition
 * vendor-specific.  You'll need to change this to support multiple
 * devices, although the changes are mostly straightforward.
 *
 * You also need support for some EGL and GL extensions...
 */

#define LIST_EXTENSIONS(_) \
    _(PFNGLMAPBUFFEROESPROC, glMapBufferOES) \
    _(PFNGLUNMAPBUFFEROESPROC, glUnmapBufferOES) \
    _(PFNEGLCREATEIMAGEKHRPROC, eglCreateImageKHR) \
    _(PFNEGLDESTROYIMAGEKHRPROC, eglDestroyImageKHR) \
    _(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC, glEGLImageTargetTexture2DOES)

#define DECLARE_PROC_POINTERS(type, name) static type _##name = NULL;
LIST_EXTENSIONS (DECLARE_PROC_POINTERS)
#undef DECLARE_PROC_POINTERS

static cairo_bool_t
_cairo_ddraw_ogl_get_extension_pointers (void)
{

#define GET_PROC_POINTERS(type, name) \
    if ((_##name = (type) eglGetProcAddress (#name)) == NULL) \
	return FALSE;
    LIST_EXTENSIONS (GET_PROC_POINTERS)
#undef GET_PROC_POINTERS

    return TRUE;
}

#undef MAP_EXTENSIONS
#undef IMAGE_EXTENSIONS
#undef LIST_EXTENSIONS

#define MAX_SCRATCH_COUNT(vtx, idx) \
    (CAIRO_DDRAW_OGL_SCRATCH_VBO_SIZE / (vtx) < \
     CAIRO_DDRAW_OGL_SCRATCH_IBO_SIZE / (idx) ? \
     CAIRO_DDRAW_OGL_SCRATCH_VBO_SIZE / (vtx) : \
     CAIRO_DDRAW_OGL_SCRATCH_IBO_SIZE / (idx))

static GLuint _cairo_ddraw_ogl_next_scratch_buffer;

static inline GLuint
_cairo_ddraw_ogl_get_scratch_buffer (void)
{
    GLuint vbo = _cairo_ddraw_ogl_next_scratch_buffer * 2 + 1;

    ++_cairo_ddraw_ogl_next_scratch_buffer;
    if (_cairo_ddraw_ogl_next_scratch_buffer >=
	CAIRO_DDRAW_OGL_NUM_SCRATCH_BUFFERS)
	_cairo_ddraw_ogl_next_scratch_buffer = 0;

    return vbo;
}

/*
 * Native pixmaps are Y-swapped in some implementations relative to GL images.
 * We check at runtime to determine if we need to invert the texture and clip
 * coordinates (which we do via the vertex program transform matrix) and/or
 * the scissor rectangles.  It's unlikely that we need to flip textures
 * differently than renderbuffers, but it costs us almost nothing to support
 * the ability to handle it if it ever happens...
 *
 * The effect of src_flipped on a source coordinate of 0 .. H is:
 *   FALSE: 0 .. H (src)  =>  0.0 .. 1.0 (t)  =>  0 .. H (v)
 *   TRUE:  0 .. H (src)  =>  1.0 .. 0.0 (t)  =>  H .. 0 (v)
 * and dst_flipped affects a dest coordinate of 0 .. H like:
 *   FALSE: 0 .. H (dst)  =>  -1.0 .. +1.0 (clip)  =>  0 .. H (viewport)
 *   TRUE:  0 .. H (dst)  =>  +1.0 .. -1.0 (clip)  =>  H .. 0 (viewport)
 * in addition, the scissor rect is Y-swapped
 *   FALSE: y .. y+height  =>  y .. y + height
 *   TRUE:  y .. y+height  =>  surf_height-y .. surf_height-y-height
 */

static cairo_bool_t _cairo_ddraw_ogl_src_flipped;
static cairo_bool_t _cairo_ddraw_ogl_dst_flipped;

static void
_cairo_ddraw_ogl_check_for_flips (cairo_ddraw_surface_t *surface)
{
    /* XXX TODO - make this a runtime check! */

    _cairo_ddraw_ogl_src_flipped = FALSE;
    _cairo_ddraw_ogl_dst_flipped = FALSE;
}


static inline cairo_status_t
_cairo_ddraw_surface_bind_to_ogl (cairo_ddraw_surface_t *surface)
{
    cairo_ddraw_surface_t * root = surface->root;
    cairo_status_t status = _cairo_ddraw_ogl_reference (surface);

    if (status)
	return status;

    glBindFramebuffer (GL_FRAMEBUFFER, root->gl_id);
    CHECK_OGL_ERROR ("glBindFramebuffer");

    return CAIRO_STATUS_SUCCESS;
}

/*
 * we have lots of fragment shaders and lots
 * of programs.  they are indexed by a program id:
 *
 * bit 1:0     0: solid color src
 *             1: RGBA surface src
 *             2: RGB surface src
 * bit 3:2     0: no mask
 *             1: solid mask
 *             2: pattern mask
 * bit 7:4     cairo operator
 * bit 9:8     0: normal src textures
 *             1: wrap src texture emulation
 *             2: clamp src texture emulation
 *
 *
 * dst alpha matters, but we can convert the operator.
 *
 * we have four distinct vertex shaders:
 * 0: pos only
 * 1: pos + src texture
 * 2: pos + mask texture
 * 3: pos + src + mask texture
 *
 * we could have gotten by with 3, but it makes things more orthogonal
 * to have all four.
 */

typedef enum _cairo_ddraw_ogl_source_tpye {
    CAIRO_DDRAW_OGL_SRC_SOLID,
    CAIRO_DDRAW_OGL_SRC_RGBA,
    CAIRO_DDRAW_OGL_SRC_RGB
} cairo_ddraw_ogl_source_type_t;

typedef enum _cairo_ddraw_ogl_mask_type {
    CAIRO_DDRAW_OGL_MASK_NONE,
    CAIRO_DDRAW_OGL_MASK_SOLID,
    CAIRO_DDRAW_OGL_MASK_A
} cairo_ddraw_ogl_mask_type_t;

typedef enum _cairo_ddraw_ogl_texemu_type {
    CAIRO_DDRAW_OGL_TEXEMU_NONE,
    CAIRO_DDRAW_OGL_TEXEMU_WRAP,
    CAIRO_DDRAW_OGL_TEXEMU_BORDER
} cairo_ddraw_ogl_texemu_type_t;

#define MAKE_SHADER_ID(op, src, mask) \
    (((op) << 4) | (src) | ((mask) << 2))

#define MAKE_EMU_SHADER_ID(op, src, mask, wrap) \
    (MAKE_SHADER_ID(op, src, mask) | ((wrap) << 8))

#define SHADER_SRC_EMU(id) ((id) >> 8)
#define SHADER_OP(id) (((id) & 0xf0 ) >> 4)
#define SHADER_SRC(id) ((id) & 0x3)
#define SHADER_MASK(id) (((id) & 0xc) >> 2)

#define CAIRO_DDRAW_OGL_NUM_VERTEX_SHADERS   4
#define CAIRO_DDRAW_OGL_NUM_PROGRAMS        (3 << 8)

typedef struct _cairo_ddraw_ogl_program_info {
    GLuint prog_id;
    GLuint vtx_id;
    GLuint frag_id;
    GLint pos_attr;
    GLint pos_xform_uniform;
    GLint src_attr;
    GLint src_sampler_uniform;
    GLint src_x_xform_uniform;
    GLint src_y_xform_uniform;
    GLint src_color_uniform;
    GLint mask_attr;
    GLint mask_sampler_uniform;
    GLint mask_x_xform_uniform;
    GLint mask_y_xform_uniform;
    GLint mask_color_uniform;
} cairo_ddraw_ogl_program_info_t;

static GLuint _cairo_ddraw_ogl_vertex_shaders[CAIRO_DDRAW_OGL_NUM_VERTEX_SHADERS];

static cairo_ddraw_ogl_program_info_t
_cairo_ddraw_ogl_program[CAIRO_DDRAW_OGL_NUM_PROGRAMS];

#ifdef CAIRO_DDRAW_OGL_BINARY_SHADER

static const char _cairo_ddraw_ogl_vertex_binary_pm[] = {
#include "vertex_pm.cghex"
};

static const char _cairo_ddraw_ogl_fragment_binary_mask_solid_over[] = {
#include "fragment_mask_solid_over.cghex"
};

#endif /* CAIRO_DDRAW_OGL_BINARY_SHADER */

cairo_ddraw_ogl_program_info_t *
_cairo_ddraw_ogl_load_program (int shader_id)
{
    cairo_ddraw_ogl_program_info_t * info =
	&_cairo_ddraw_ogl_program[shader_id];

    if (info->prog_id == 0) {
	cairo_bool_t has_src =
	    SHADER_OP (shader_id) != CAIRO_OPERATOR_CLEAR &&
	    SHADER_OP (shader_id) != CAIRO_OPERATOR_DEST;
	cairo_bool_t has_src_tex = has_src &&
	    SHADER_SRC(shader_id) != CAIRO_DDRAW_OGL_SRC_SOLID;
	cairo_bool_t has_mask_tex =
	    SHADER_MASK(shader_id) == CAIRO_DDRAW_OGL_MASK_A;
	cairo_bool_t has_mask =
	    SHADER_MASK(shader_id) != CAIRO_DDRAW_OGL_MASK_NONE;
	cairo_bool_t has_dst = has_mask ||
	    SHADER_OP (shader_id) != CAIRO_OPERATOR_SOURCE &&
	    SHADER_OP (shader_id) != CAIRO_OPERATOR_CLEAR;
	cairo_operator_t op = (cairo_operator_t) SHADER_OP (shader_id);
	int vtx_shader_id = has_src_tex + (has_mask_tex << 1);
	GLuint vtx_shader = _cairo_ddraw_ogl_vertex_shaders[vtx_shader_id];
	cairo_bool_t frag_shader_loaded = FALSE;

	if (vtx_shader == 0) {
	    cairo_bool_t vtx_shader_loaded = FALSE;

	    vtx_shader = glCreateShader (GL_VERTEX_SHADER);
	    CHECK_OGL_ERROR ("glCreateShader vertex");

	    _cairo_ddraw_ogl_vertex_shaders[vtx_shader_id] = vtx_shader;

#ifdef CAIRO_DDRAW_OGL_BINARY_SHADER
	    if (!vtx_shader_loaded) {
		const GLvoid * binary_data = NULL;
		GLsizei binary_len = 0;

		/* see if we have a precompiled one */
		switch (vtx_shader_id) {
		case 2:
		    binary_data = _cairo_ddraw_ogl_vertex_binary_pm;
		    binary_len = sizeof (_cairo_ddraw_ogl_vertex_binary_pm);
		    break;
		default:
		    binary_data = NULL;
		    binary_len = 0;
		    break;
		}

		if (binary_data) {
		    glShaderBinary (1, &vtx_shader, CAIRO_DDRAW_OGL_BINARY_SHADER,
				    binary_data, binary_len);
		    CHECK_OGL_ERROR ("glShaderBinary vertex");
		    vtx_shader_loaded = TRUE;
		}
	    }
#endif
#ifdef CAIRO_DDRAW_OGL_SOURCE_SHADER
	    if (!vtx_shader_loaded) {
		const char * shader_src[10];
		int i = 0;

		if (has_mask_tex)
		    shader_src[i++] =
			"attribute vec2 attMaskCoord;\n"
			"varying vec2 varMaskCoord;\n"
			"uniform vec3 uniMaskXformX;\n"
			"uniform vec3 uniMaskXformY;\n";
		if (has_src_tex)
		    shader_src[i++] =
			"attribute vec2 attSrcCoord;\n"
			"varying vec2 varSrcCoord;\n"
			"uniform vec3 uniSrcXformX;\n"
			"uniform vec3 uniSrcXformY;\n";
		shader_src[i++] = 
		    "attribute vec2 attPos;\n"
		    "uniform vec4 uniPosXform;\n"
		    "void main() {\n";
		if (has_mask_tex)
		    shader_src[i++] =
			"varMaskCoord.x = attMaskCoord.x * uniMaskXformX.x +\n"
			"attMaskCoord.y * uniMaskXformX.y + uniMaskXformX.z;\n"
			"varMaskCoord.y = attMaskCoord.x * uniMaskXformY.x +\n"
			"attMaskCoord.y * uniMaskXformY.y + uniMaskXformY.z;\n";
		if (has_src_tex)
		    shader_src[i++] =
			"varSrcCoord.x = attSrcCoord.x * uniSrcXformX.x +\n"
			"attSrcCoord.y * uniSrcXformX.y + uniSrcXformX.z;\n"
			"varSrcCoord.y = attSrcCoord.x * uniSrcXformY.x +\n"
			"attSrcCoord.y * uniSrcXformY.y + uniSrcXformY.z;\n";
		shader_src[i++] =
		    "gl_Position = vec4(attPos * uniPosXform.xz + uniPosXform.yw, 0, 1);}\n";

		glShaderSource (vtx_shader, i, shader_src, NULL);
		CHECK_OGL_ERROR ("glShaderSource vertex");

		glCompileShader (vtx_shader);
		CHECK_OGL_ERROR ("glCompileShader vertex");

#ifdef CAIRO_DDRAW_DEBUG_VERBOSE
		{
		    int j;
		    for (j = 0; j < i; ++j)
			_cairo_ddraw_log ("%s", shader_src[j]);
		}
#endif
#ifdef CAIRO_DDRAW_DEBUG
		_cairo_ddraw_log ("compiled vertex shader 0x%x\n", vtx_shader_id);
		{
		    char buffer[10000];
		    glGetShaderInfoLog (vtx_shader, sizeof (buffer), NULL, buffer);
		    CHECK_OGL_ERROR ("glGetShaderInfoLog");
		    if (buffer[0])
			_cairo_ddraw_log ("Compiler output:\n%s", buffer);
		}
#endif		
		/*XXX - check status? */
		vtx_shader_loaded = TRUE;
	    }
#endif /* CAIRO_DDRAW_OGL_SOURCE_SHADER */
	    if (!vtx_shader_loaded) {
		glDeleteShader (vtx_shader);
		return NULL;
	    }
	}

	info->vtx_id = vtx_shader;

	info->prog_id = glCreateProgram ();
	CHECK_OGL_ERROR ("glCreateProgram");

	info->frag_id = glCreateShader (GL_FRAGMENT_SHADER);
	CHECK_OGL_ERROR ("glCreateShader fragment");

#ifdef CAIRO_DDRAW_OGL_BINARY_SHADER
	if (!frag_shader_loaded) {
	    const GLvoid * binary_data = NULL;
	    GLsizei binary_len = 0;

	    /* see if we have a precompiled one */
	    switch (shader_id) {
	    case MAKE_SHADER_ID (CAIRO_OPERATOR_OVER,
				 CAIRO_DDRAW_OGL_SRC_SOLID,
				 CAIRO_DDRAW_OGL_MASK_A):
		binary_data = _cairo_ddraw_ogl_fragment_binary_mask_solid_over;
		binary_len = sizeof (_cairo_ddraw_ogl_fragment_binary_mask_solid_over);
		break;
	    default:
		binary_data = NULL;
		binary_len = 0;
		break;
	    }
	    
	    if (binary_data) {
		glShaderBinary (1, &info->frag_id, CAIRO_DDRAW_OGL_BINARY_SHADER,
				binary_data, binary_len);
		CHECK_OGL_ERROR ("glShaderBinary fragment");
		frag_shader_loaded = TRUE;
	    }
	}
#endif
#ifdef CAIRO_DDRAW_OGL_SOURCE_SHADER
	if (!frag_shader_loaded) {
	    const char * shader_src[100];
	    int i = 0;

	    shader_src[i++] =
		"#extension GL_NV_shader_framebuffer_fetch : enable\n";
	    if (has_mask_tex)
		shader_src[i++] =
		    "uniform sampler2D sampMask;\n"
		    "varying vec2 varMaskCoord;\n";
	    else if (has_mask)
		shader_src[i++] =
		    "uniform lowp float uniMaskColor;\n";
	    if (has_src_tex)
		shader_src[i++] =
		    "uniform sampler2D sampSrc;\n"
		    "varying vec2 varSrcCoord;\n";
	    else if (has_src)
		shader_src[i++] =
		    "uniform lowp vec4 uniSrcColor;\n";
	    shader_src[i++] =
		"void main(void) {\n";
	    if (has_mask_tex)
		shader_src[i++] = 
		    "lowp vec4 mask = texture2D(sampMask, varMaskCoord);\n"
		    "if (mask.a == 0.0) discard;\n";
	    else if (has_mask)
		shader_src[i++] =
		    "lowp vec4 mask = vec4(0.0, 0.0, 0.0, uniMaskColor);\n";
 	    if (has_src_tex) {
 		cairo_bool_t fix_alpha =
 		    SHADER_SRC (shader_id) == CAIRO_DDRAW_OGL_SRC_RGB;
 		
 		switch (SHADER_SRC_EMU (shader_id)) {
 		case CAIRO_DDRAW_OGL_TEXEMU_NONE:
 		    shader_src[i++] =
 			"lowp vec4 src = texture2D(sampSrc, varSrcCoord);\n";
 		    if (fix_alpha)
 			shader_src[i++] =
 			    "src.a = 1.0;\n";
 		    break;
 		case CAIRO_DDRAW_OGL_TEXEMU_WRAP:
 		    shader_src[i++] =
 			"lowp vec4 src = texture2D(sampSrc, fract(varSrcCoord));\n";
 		    if (fix_alpha)
 			shader_src[i++] =
 			    "src.a = 1.0;\n";
 		    break;
 		case CAIRO_DDRAW_OGL_TEXEMU_BORDER:
 		    shader_src[i++] =
 			"lowp vec4 src = texture2D(sampSrc, varSrcCoord);\n";
 		    if (fix_alpha)
 			shader_src[i++] =
 			    "src.a = 1.0;\n";
 		    shader_src[i++] =
 			"lowp vec2 win = step(vec2(0.0), varSrcCoord) * step(varSrcCoord, vec2(1.0));\n"
 			"src *= win.s * win.t;\n";
 		    break;
 		default:
 		    assert (0);
 		    goto FAIL;
 		}
 	    } else if (has_src)
  		shader_src[i++] =
  		    "lowp vec4 src = uniSrcColor;\n";
	    if (has_dst)
		shader_src[i++] =
		    "lowp vec4 dst = gl_LastFragColor;\n";
	    switch (op) {
	    case CAIRO_OPERATOR_CLEAR:
		shader_src[i++] =
		    "lowp vec4 blend = 0.0;\n";
		break;
	    case CAIRO_OPERATOR_SOURCE:
		shader_src[i++] =
		    "lowp vec4 blend = src;\n";
		break;
	    case CAIRO_OPERATOR_OVER:
		shader_src[i++] =
		    "lowp vec4 blend = src + (1.0 - src.a) * dst;\n";
		break;
	    case CAIRO_OPERATOR_IN:
		shader_src[i++] =
		    "lowp vec4 blend = dst.a * src;\n";
		break;
	    case CAIRO_OPERATOR_OUT:
		shader_src[i++] =
		    "lowp vec4 blend = (1.0 - dst.a) * src;\n";
		break;
	    case CAIRO_OPERATOR_ATOP:
		shader_src[i++] =
		    "lowp vec4 blend = dst.a * src * (1.0 - src.a) * dst;\n";
		break;
	    case CAIRO_OPERATOR_DEST:
		shader_src[i++] =
		    "lowp vec4 blend = dst;\n";
		break;
	    case CAIRO_OPERATOR_DEST_OVER:
		shader_src[i++] =
		    "lowp vec4 blend = (1.0 - dst.a) * src + dst;\n";
		break;
	    case CAIRO_OPERATOR_DEST_IN:
		shader_src[i++] =
		    "lowp vec4 blend = src.a * dst;\n";
		break;
	    case CAIRO_OPERATOR_DEST_OUT:
		shader_src[i++] =
		    "lowp vec4 blend = (1.0 - src.a) * dst;\n";
		break;
	    case CAIRO_OPERATOR_DEST_ATOP:
		shader_src[i++] =
		    "lowp vec4 blend = (1.0 - dst.a) * src + src.a * dst;\n";
		break;
	    case CAIRO_OPERATOR_XOR:
		shader_src[i++] =
		    "lowp vec4 blend = (1.0 - dst.a) * src + (1.0 - src.a) * dst;\n";
		break;
	    case CAIRO_OPERATOR_ADD:
		shader_src[i++] =
		    "lowp vec4 blend = src + dst;\n";
		break;
	    case CAIRO_OPERATOR_SATURATE:
		shader_src[i++] =
		    "if (src.a > dst.a) src *= dst.a / src.a;\n"
		    "lowp vec4 blend = src + dst\n";
		break;
	    default:
		goto FAIL;
	    }
	    if (has_mask)
		shader_src[i++] =
		    "gl_FragColor = mask.a * blend + (1.0 - mask.a) * dst;}\n";
	    else
		shader_src[i++] =
		    "gl_FragColor = blend;}\n";

	    glShaderSource (info->frag_id, i, shader_src, NULL);
	    CHECK_OGL_ERROR ("glShaderSource fragment");

	    glCompileShader (info->frag_id);
	    CHECK_OGL_ERROR ("glCompileShader fragment");

#ifdef CAIRO_DDRAW_DEBUG_VERBOSE
	    {
		int j;
		for (j = 0; j < i; ++j)
		    _cairo_ddraw_log ("%s", shader_src[j]);
	    }
#endif
#ifdef CAIRO_DDRAW_DEBUG
	    _cairo_ddraw_log ("compiled fragment shader 0x%x\n", shader_id);
	    {
		char buffer[10000];
		glGetShaderInfoLog (info->frag_id, sizeof (buffer), NULL, buffer);
		CHECK_OGL_ERROR ("glGetShaderInfoLog");
		if (buffer[0])
		    _cairo_ddraw_log ("Compiler output:\n%s", buffer);
	    }
#endif		

	    glReleaseShaderCompiler ();
	    CHECK_OGL_ERROR ("glReleaseShaderCompiler");

	    /*XXX check status? */
	    frag_shader_loaded = TRUE;
	}
#endif /* CAIRO_DDRAW_OGL_SOURCE_SHADER */
	if (!frag_shader_loaded)
	    goto FAIL;

	glAttachShader (info->prog_id, info->vtx_id);
	CHECK_OGL_ERROR ("glAttachShader vertex");

	glAttachShader (info->prog_id, info->frag_id);
	CHECK_OGL_ERROR ("glAttachShader fragment");

	glLinkProgram (info->prog_id);
	CHECK_OGL_ERROR ("glLinkProgram");

#ifdef CAIRO_DDRAW_DEBUG
	{
	    char buffer[10000];
	    glGetProgramInfoLog (info->prog_id, sizeof (buffer), NULL, buffer);
	    CHECK_OGL_ERROR ("glGetProgramInfoLog");
	    if (buffer[0])
		_cairo_ddraw_log ("Linker output:\n%s", buffer);
	}
#endif		
#ifdef CAIRO_DDRAW_DEBUG_VERBOSE
	{
	    int j, k, size, loc;
	    GLenum type;
	    char buffer[100];

	    glValidateProgram (info->prog_id);
	    glGetProgramiv (info->prog_id, GL_DELETE_STATUS, &k);
	    _cairo_ddraw_log ("DELETE_STATUS     %d\n", k);
	    glGetProgramiv (info->prog_id, GL_LINK_STATUS, &k);
	    _cairo_ddraw_log ("LINK_STATUS       %d\n", k);
	    glGetProgramiv (info->prog_id, GL_VALIDATE_STATUS, &k);
	    _cairo_ddraw_log ("VALIDATE_STATUS   %d\n", k);
	    glGetProgramiv (info->prog_id, GL_ACTIVE_ATTRIBUTES, &k);
	    _cairo_ddraw_log ("ACTIVE_ATTRIBUTES %d\n", k);
	    for (j = 0; j < k; ++j) {
		glGetActiveAttrib (info->prog_id, j, sizeof (buffer),
				   NULL, &size, &type, buffer);
		loc = glGetAttribLocation (info->prog_id, buffer);
		_cairo_ddraw_log ("  %20s  %2u  0x%04x  %3d\n",
				  buffer, size, type, loc);
	    }
	    glGetProgramiv (info->prog_id, GL_ACTIVE_UNIFORMS, &k);
	    _cairo_ddraw_log ("ACTIVE_UNIFORMS   %d\n", k);
	    for (j = 0; j < k; ++j) {
		glGetActiveUniform (info->prog_id, j, sizeof (buffer),
				    NULL, &size, &type, buffer);
		loc = glGetUniformLocation (info->prog_id, buffer);
		_cairo_ddraw_log ("  %20s  %2u  0x%04x  %3d\n",
				  buffer, size, type, loc);
	    }
	    
	}
#endif


	info->pos_attr = glGetAttribLocation (info->prog_id, "attPos");
	CHECK_OGL_ERROR ("glGetAttribLocation pos");
	if (info->pos_attr < 0)
	    goto FAIL;

	info->pos_xform_uniform =
	    glGetUniformLocation (info->prog_id, "uniPosXform");
	CHECK_OGL_ERROR ("glGetUniformLocation pos xform");
	if (info->pos_xform_uniform < 0)
	    goto FAIL;

	if (has_mask_tex) {
	    info->mask_attr = glGetAttribLocation (info->prog_id, "attMaskCoord");
	    CHECK_OGL_ERROR ("glGetAttribLocation mask");
	    if (info->mask_attr < 0)
		goto FAIL;
	    info->mask_x_xform_uniform =
		glGetUniformLocation (info->prog_id, "uniMaskXformX");
	    CHECK_OGL_ERROR ("glGetUniformLocation mask xform x");
	    if (info->mask_x_xform_uniform < 0)
		goto FAIL;
	    info->mask_y_xform_uniform =
		glGetUniformLocation (info->prog_id, "uniMaskXformY");
	    CHECK_OGL_ERROR ("glGetUniformLocation mask xform y");
	    if (info->mask_y_xform_uniform < 0)
		goto FAIL;
	    info->mask_sampler_uniform =
		glGetUniformLocation (info->prog_id, "sampMask");
	    CHECK_OGL_ERROR ("glGetUniformLocation mask sampler");
	    if (info->mask_sampler_uniform < 0)
		goto FAIL;

	    info->mask_color_uniform = -1;
	} else {
	    info->mask_attr =
		info->mask_x_xform_uniform =
		info->mask_y_xform_uniform =
		info->mask_sampler_uniform = -1;

	    if (has_mask) {
		info->mask_color_uniform =
		    glGetUniformLocation (info->prog_id, "uniMaskColor");
		CHECK_OGL_ERROR ("glGetUniformLocation mask color");
		if (info->mask_color_uniform < 0)
		    goto FAIL;
	    } else
		info->mask_color_uniform = -1;
	}

	if (has_src_tex) {
	    info->src_attr = glGetAttribLocation (info->prog_id, "attSrcCoord");
	    CHECK_OGL_ERROR ("glGetAttribLocation src");
	    if (info->src_attr < 0)
		goto FAIL;
	    info->src_x_xform_uniform =
		glGetUniformLocation (info->prog_id, "uniSrcXformX");
	    CHECK_OGL_ERROR ("glGetUniformLocation src xform x");
	    if (info->src_x_xform_uniform < 0)
		goto FAIL;
	    info->src_y_xform_uniform =
		glGetUniformLocation (info->prog_id, "uniSrcXformY");
	    CHECK_OGL_ERROR ("glGetUniformLocation src xform y");
	    if (info->src_y_xform_uniform < 0)
		goto FAIL;
	    info->src_sampler_uniform =
		glGetUniformLocation (info->prog_id, "sampSrc");
	    CHECK_OGL_ERROR ("glGetUniformLocation src sampler");
	    if (info->src_sampler_uniform < 0)
		goto FAIL;

	    info->src_color_uniform = -1;
	} else {
	    info->src_attr =
		info->src_x_xform_uniform =
		info->src_y_xform_uniform =
		info->src_sampler_uniform = -1;

	    if (has_src) {
		info->src_color_uniform =
		    glGetUniformLocation (info->prog_id, "uniSrcColor");
		CHECK_OGL_ERROR ("glGetUniformLocation src color");
		if (info->src_color_uniform < 0)
		    goto FAIL;
	    } else
		info->src_color_uniform = -1;
	}
    }

    return info;

 FAIL:
    glDeleteShader (info->frag_id);
    CHECK_OGL_ERROR ("glDeleteShader");
    glDeleteProgram (info->prog_id);
    CHECK_OGL_ERROR ("glDeleteProgram");

    info->prog_id = 0;
    return NULL;
}

/* for downgrading shaders when dst does not have alpha */
static const cairo_operator_t _cairo_ddraw_ogl_op_downgrade[] = {
    /* CLEAR     */ CAIRO_OPERATOR_CLEAR,
    /* SOURCE    */ CAIRO_OPERATOR_SOURCE,
    /* OVER      */ CAIRO_OPERATOR_OVER,
    /* IN        */ CAIRO_OPERATOR_SOURCE,
    /* OUT       */ CAIRO_OPERATOR_CLEAR,
    /* ATOP      */ CAIRO_OPERATOR_OVER,
    /* DEST      */ CAIRO_OPERATOR_DEST,
    /* DEST_OVER */ CAIRO_OPERATOR_DEST,
    /* DEST_IN   */ CAIRO_OPERATOR_DEST_IN,
    /* DEST_OUT  */ CAIRO_OPERATOR_DEST_OUT,
    /* DEST_ATOP */ CAIRO_OPERATOR_DEST_IN,
    /* XOR       */ CAIRO_OPERATOR_DEST_OUT,
    /* ADD       */ CAIRO_OPERATOR_ADD,
    /* SATURATE  */ CAIRO_OPERATOR_DEST,
};

typedef GLshort DstCoordinate_t;
typedef GLshort SrcCoordinate_t;
typedef GLushort Index_t;

enum {
    DST_COORDINATE_TYPE = GL_SHORT,
    SRC_COORDINATE_TYPE = GL_SHORT,
    INDEX_TYPE = GL_UNSIGNED_SHORT,
};

typedef struct _cairo_ddraw_ogl_dst_coord {
    DstCoordinate_t x, y;
} cairo_ddraw_ogl_dst_coord_t;

typedef struct _cairo_ddraw_ogl_src_coord {
    SrcCoordinate_t s, t;
} cairo_ddraw_ogl_src_coord_t;

typedef struct _cairo_ddraw_ogl_quad {
    cairo_ddraw_ogl_dst_coord_t dst[4];
    cairo_ddraw_ogl_src_coord_t src[4];
    cairo_ddraw_ogl_src_coord_t mask[4];
} cairo_ddraw_ogl_quad_t;

typedef struct _cairo_ddraw_ogl_glyph_vtx {
    DstCoordinate_t x, y;
    SrcCoordinate_t s, t;
} cairo_ddraw_ogl_glyph_vtx_t;

typedef struct _cairo_ddraw_ogl_glyph_quad {
    cairo_ddraw_ogl_glyph_vtx_t vtx[4];
} cairo_ddraw_ogl_glyph_quad_t;

typedef struct _cairo_ddraw_ogl_glyph_idx {
    Index_t idx[6];
} cairo_ddraw_ogl_glyph_idx_t;

typedef struct _cairo_ddraw_ogl_line {
    cairo_ddraw_ogl_dst_coord_t vtx[2];
} cairo_ddraw_ogl_line_t;

/* qualify textures by what we can do with the hardware */

typedef enum {
    CAIRO_DDRAW_OGL_TEXTURE_SUPPORTED,         /* can use as-is */
    CAIRO_DDRAW_OGL_TEXTURE_UNUSED,            /* is solid */
    CAIRO_DDRAW_OGL_TEXTURE_IGNORE_WRAP,       /* we don't cross an edge */
    CAIRO_DDRAW_OGL_TEXTURE_BORDER_IN_SHADER,  /* use a clamping shader */
    CAIRO_DDRAW_OGL_TEXTURE_WRAP_IN_SHADER,    /* use a wrapping shader */
    CAIRO_DDRAW_OGL_TEXTURE_MIRROR_IN_SHADER,  /* use a mirroring shader */
    CAIRO_DDRAW_OGL_TEXTURE_BORDER_AND_BLEND,  /* need to lerp edges */
    CAIRO_DDRAW_OGL_TEXTURE_PAD_AND_WRAP,      /* need to pad texture */
    CAIRO_DDRAW_OGL_TEXTURE_UNSUPPORTED,       /* no can do */
} cairo_ddraw_ogl_texture_type_t;

static inline cairo_ddraw_ogl_texture_type_t
_cairo_ddraw_ogl_analyze_pattern (const cairo_pattern_t * pattern,
				  int x,
				  int y,
				  int width,
				  int height,
				  cairo_rectangle_int_t * extents_out,
				  cairo_rectangle_int_t * bbox_out)
{
    if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_t * surface = 
	    ((cairo_surface_pattern_t *) pattern)->surface;
	cairo_rectangle_int_t extents;
	double x1, y1, x2, y2;
	double pad = 0.5;

	_cairo_surface_get_extents (surface, &extents);

	if (extents.width  > _cairo_ddraw_ogl_max_texture_size ||
	    extents.height > _cairo_ddraw_ogl_max_texture_size)
	    return CAIRO_DDRAW_OGL_TEXTURE_UNSUPPORTED;

	if (extents_out)
	    *extents_out = extents;

	/* maybe we don't leave the texture? */
	x1 = x;
	y1 = y;
	x2 = x + width;
	y2 = y + height;

	_cairo_matrix_transform_bounding_box (&pattern->matrix,
					      &x1, &y1, &x2, &y2,
					      NULL);

	if (pattern->filter == CAIRO_FILTER_NEAREST ||
	    _cairo_matrix_is_pixel_exact (&pattern->matrix))
	    pad = 0.0;

	x1 -= pad;
	x2 += pad;
	y1 -= pad;
	y2 += pad;

	if (bbox_out) {
	    bbox_out->x = (int) floor (x1);
	    bbox_out->y = (int) floor (y1);
	    bbox_out->width = (int) ceil (x2) - bbox_out->x;
	    bbox_out->height = (int) ceil (y2) - bbox_out->y;
	}

	if (x1 >= 0 && x2 <= extents.width &&
	    y1 >= 0 && y2 <= extents.height)
	    return CAIRO_DDRAW_OGL_TEXTURE_IGNORE_WRAP;

	/* check for hardware-supported modes */
	if (pattern->extend == CAIRO_EXTEND_PAD ||
	    pattern->extend != CAIRO_EXTEND_NONE &&
	    (extents.width & (extents.width - 1)) == 0 &&
	    (extents.height & (extents.height - 1)) == 0)
	    return CAIRO_DDRAW_OGL_TEXTURE_SUPPORTED;

	/* otherwise we need to do magic in the shader */
	switch (pattern->extend) {
	case CAIRO_EXTEND_NONE:
	    if (pad)
		return CAIRO_DDRAW_OGL_TEXTURE_BORDER_AND_BLEND;
	    return CAIRO_DDRAW_OGL_TEXTURE_BORDER_IN_SHADER;
	case CAIRO_EXTEND_REPEAT:
	    if (pad)
		return CAIRO_DDRAW_OGL_TEXTURE_PAD_AND_WRAP;
	    return CAIRO_DDRAW_OGL_TEXTURE_WRAP_IN_SHADER;
	case CAIRO_EXTEND_REFLECT:
	    return CAIRO_DDRAW_OGL_TEXTURE_MIRROR_IN_SHADER;
	default:
	    return CAIRO_DDRAW_OGL_TEXTURE_UNSUPPORTED;
	}

    } else if (pattern->type == CAIRO_PATTERN_TYPE_SOLID)
	return CAIRO_DDRAW_OGL_TEXTURE_UNUSED;

    return CAIRO_DDRAW_OGL_TEXTURE_UNSUPPORTED;
}

static cairo_int_status_t
_cairo_ddraw_ogl_bind_program (cairo_ddraw_ogl_program_info_t ** info_ret,
			       cairo_ddraw_surface_t * dst,
			       const cairo_pattern_t * src,
			       const cairo_pattern_t * mask,
			       cairo_operator_t op,
			       cairo_ddraw_ogl_texemu_type_t src_texemu,
			       cairo_rectangle_int_t * src_extents,
			       cairo_rectangle_int_t * mask_extents)
{
    cairo_ddraw_ogl_program_info_t * info;
    cairo_ddraw_ogl_source_type_t src_type;
    cairo_ddraw_ogl_mask_type_t mask_type = CAIRO_DDRAW_OGL_MASK_NONE;
    GLfloat scr, scg, scb, sca, mca;
    int dstw = dst->root->extents.width;
    int dsth = dst->root->extents.height;
    GLfloat ws, hs;
    int dx, dy;

    if (dst->base.content == CAIRO_CONTENT_COLOR)
	op = _cairo_ddraw_ogl_op_downgrade[op];

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_INT_STATUS_NOTHING_TO_DO;

    if (mask) {
	if (mask->type == CAIRO_PATTERN_TYPE_SURFACE)
	    mask_type = CAIRO_DDRAW_OGL_MASK_A;
	else if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	    cairo_solid_pattern_t * sp = (cairo_solid_pattern_t *) mask;
	    unsigned short ma = sp->color.alpha_short;
	    if (ma < 0x0100)
		mask = NULL;
	    else if (ma >= 0xff00)
		return CAIRO_INT_STATUS_NOTHING_TO_DO;
	    else {
		mask_type = CAIRO_DDRAW_OGL_MASK_SOLID;
		mca = ma * (1.0f / 65535.0f);
	    }
	} else
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (op == CAIRO_OPERATOR_CLEAR) {
	op = CAIRO_OPERATOR_SOURCE;
	src_type = CAIRO_DDRAW_OGL_SRC_SOLID;
	scr = scg = scb = sca = 0.0f;
    } else if (src->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_pattern_t * sp = (cairo_surface_pattern_t *) src;
	src_type = sp->surface->content == CAIRO_CONTENT_COLOR ?
	    CAIRO_DDRAW_OGL_SRC_RGB : CAIRO_DDRAW_OGL_SRC_RGBA;
    } else if (src->type == CAIRO_PATTERN_TYPE_SOLID) {
	cairo_solid_pattern_t * sp = (cairo_solid_pattern_t *) src;
	cairo_color_t * color = &sp->color;
	src_type = CAIRO_DDRAW_OGL_SRC_SOLID;
	scr = color->red_short   * (1.0f / 65535.0f);
	scg = color->green_short * (1.0f / 65535.0f);
	scb = color->blue_short  * (1.0f / 65535.0f);
	sca = color->alpha_short * (1.0f / 65535.0f);
    } else
	return CAIRO_INT_STATUS_UNSUPPORTED;

    info = _cairo_ddraw_ogl_load_program (MAKE_EMU_SHADER_ID (op,
							      src_type,
							      mask_type,
							      src_texemu));

    if (info == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    glUseProgram (info->prog_id);
    CHECK_OGL_ERROR ("glUseProgram");

    glVertexAttribPointer (info->pos_attr, 2, DST_COORDINATE_TYPE, GL_FALSE,
			   sizeof (cairo_ddraw_ogl_dst_coord_t),
			   (GLvoid *) offsetof (cairo_ddraw_ogl_quad_t, dst));
    CHECK_OGL_ERROR ("glVertexAttribPointer pos");

    glEnableVertexAttribArray (info->pos_attr);
    CHECK_OGL_ERROR ("glEnableVertexArray pos");

    if (_cairo_ddraw_ogl_dst_flipped)
	glUniform4f (info->pos_xform_uniform,
		     2.0f / dst->root->extents.width, -1.0f,
		     -2.0f / dst->root->extents.height, 1.0f);
    else
	glUniform4f (info->pos_xform_uniform,
		     2.0f / dst->root->extents.width, -1.0f,
		     2.0f / dst->root->extents.height, -1.0f);
    CHECK_OGL_ERROR ("glUniform4f pos xform");
    
    if (mask_type == CAIRO_DDRAW_OGL_MASK_A) {
	glVertexAttribPointer (info->mask_attr, 2, SRC_COORDINATE_TYPE, GL_FALSE,
			       sizeof (cairo_ddraw_ogl_src_coord_t),
			       (GLvoid *) offsetof (cairo_ddraw_ogl_quad_t, mask));
	CHECK_OGL_ERROR ("glVertexAttribPointer mask");
	
	glEnableVertexAttribArray (info->mask_attr);
	CHECK_OGL_ERROR ("glEnableVertexArray mask");

	glUniform1i (info->mask_sampler_uniform, 0);
	CHECK_OGL_ERROR ("glUniform1i mask sampler");

	if (mask_extents) {
	    dx = mask_extents->x;
	    dy = mask_extents->y;
	    ws = 1.0f / mask_extents->width;
	    hs = 1.0f / mask_extents->height;
	} else {
	    cairo_rectangle_int_t extents;
	    cairo_surface_t * surface =
		((cairo_surface_pattern_t *) mask)->surface;

	    _cairo_surface_get_extents (surface, &extents);

	    dx = extents.x;
	    dy = extents.y;
	    ws = 1.0f / extents.width;
	    hs = 1.0f / extents.height;
	}

	glUniform3f (info->mask_x_xform_uniform,
		     (GLfloat) mask->matrix.xx * ws,
		     (GLfloat) mask->matrix.xy * ws,
		     (GLfloat) (mask->matrix.x0 - dx) * ws);
	CHECK_OGL_ERROR ("glUniform3f mask xform x");

	if (_cairo_ddraw_ogl_src_flipped)
	    glUniform3f (info->mask_y_xform_uniform,
			 (GLfloat) mask->matrix.yx * -hs,
			 (GLfloat) mask->matrix.yy * -hs,
			 (GLfloat) (mask->matrix.y0 - dy) * -hs + 1.0f);
	else
	    glUniform3f (info->mask_y_xform_uniform,
			 (GLfloat) mask->matrix.yx * hs,
			 (GLfloat) mask->matrix.yy * hs,
			 (GLfloat) (mask->matrix.y0 - dy) * hs);
	CHECK_OGL_ERROR ("glUniform3f mask xform y");

    } else if (mask_type == CAIRO_DDRAW_OGL_MASK_SOLID) {
	glUniform1f (info->mask_color_uniform, mca);
	CHECK_OGL_ERROR ("glUniform1f mask color");
    }

    if (src_type != CAIRO_DDRAW_OGL_SRC_SOLID) {
	glVertexAttribPointer (info->src_attr, 2, SRC_COORDINATE_TYPE, GL_FALSE,
			       sizeof (cairo_ddraw_ogl_src_coord_t),
			       (GLvoid *) offsetof (cairo_ddraw_ogl_quad_t, src));
	CHECK_OGL_ERROR ("glVertexAttribPointer src");
	
	glEnableVertexAttribArray (info->src_attr);
	CHECK_OGL_ERROR ("glEnableVertexArray src");

	glUniform1i (info->src_sampler_uniform, 1);
	CHECK_OGL_ERROR ("glUniform1i src sampler");

	if (src_extents) {
	    dx = src_extents->x;
	    dy = src_extents->y;
	    ws = 1.0f / src_extents->width;
	    hs = 1.0f / src_extents->height;
	} else {
	    cairo_rectangle_int_t extents;
	    cairo_surface_t * surface =
		((cairo_surface_pattern_t *) src)->surface;

	    _cairo_surface_get_extents (surface, &extents);

	    dx = extents.x;
	    dy = extents.y;
	    ws = 1.0f / extents.width;
	    hs = 1.0f / extents.height;
	}

	glUniform3f (info->src_x_xform_uniform,
		     (GLfloat) src->matrix.xx * ws,
		     (GLfloat) src->matrix.xy * ws,
		     (GLfloat) (src->matrix.x0 - dx) * ws);
	CHECK_OGL_ERROR ("glUniform3f src xform x");

	if (_cairo_ddraw_ogl_src_flipped)
	    glUniform3f (info->src_y_xform_uniform,
			 (GLfloat) src->matrix.yx * -hs,
			 (GLfloat) src->matrix.yy * -hs,
			 (GLfloat) (src->matrix.y0 - dy) * -hs + 1.0f);
	else
	    glUniform3f (info->src_y_xform_uniform,
			 (GLfloat) src->matrix.yx * hs,
			 (GLfloat) src->matrix.yy * hs,
			 (GLfloat) (src->matrix.y0 - dy) * hs);
	CHECK_OGL_ERROR ("glUniform3f src xform y");

    } else {
	glUniform4f (info->src_color_uniform,
		     scr, scg, scb, sca);
	CHECK_OGL_ERROR ("glUniform4f src color");
    }

    glViewport (0, 0, dstw, dsth);
    CHECK_OGL_ERROR ("glViewport");

    return _cairo_ddraw_surface_bind_to_ogl (dst);
}


static void
_cairo_ddraw_ogl_free_programs (void)
{
    int i;

    for (i = 0; i < CAIRO_DDRAW_OGL_NUM_PROGRAMS; ++i) {
	cairo_ddraw_ogl_program_info_t * info =
	    &_cairo_ddraw_ogl_program[i];

	if (info->prog_id) {
	    glDeleteShader (info->frag_id);
	    CHECK_OGL_ERROR ("glDeleteShader fragment");
	    glDeleteProgram (info->prog_id);
	    CHECK_OGL_ERROR ("glDeleteProgram");
	    info->prog_id = 0;
	}
    }
    for (i = 0; i < CAIRO_DDRAW_OGL_NUM_VERTEX_SHADERS; ++i) {
	glDeleteShader (_cairo_ddraw_ogl_vertex_shaders[i]);
	CHECK_OGL_ERROR ("glDeleteShader vertex");
	_cairo_ddraw_ogl_vertex_shaders[i] = 0;
    }
}

static void
_cairo_ddraw_ogl_fini (void)
{
    GLuint i = 1;

    for (i = 0; i < CAIRO_DDRAW_OGL_NUM_SCRATCH_BUFFERS; ++i) {
	GLuint vbo = i * 2 + 1;
	GLuint ibo = i * 2 + 2;

	glDeleteBuffers (i, &vbo);
	CHECK_OGL_ERROR ("glDeleteBuffers");
	glDeleteBuffers (i, &ibo);
	CHECK_OGL_ERROR ("glDeleteBuffers");
    }

    _cairo_ddraw_ogl_free_programs ();

    if (_cairo_ddraw_egl_dpy) {

	eglMakeCurrent (_cairo_ddraw_egl_dpy,
			EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

	if (_cairo_ddraw_egl_dummy_surface != EGL_NO_SURFACE) {
	    eglDestroySurface (_cairo_ddraw_egl_dpy,
			       _cairo_ddraw_egl_dummy_surface);
	    _cairo_ddraw_egl_dummy_surface = EGL_NO_SURFACE;
	}

	if (_cairo_ddraw_egl_dummy_ctx != EGL_NO_CONTEXT) {
	    eglDestroyContext (_cairo_ddraw_egl_dpy,
			       _cairo_ddraw_egl_dummy_ctx);
	    _cairo_ddraw_egl_dummy_ctx = EGL_NO_CONTEXT;
	}

	eglTerminate (_cairo_ddraw_egl_dpy);
	_cairo_ddraw_egl_dpy = EGL_NO_DISPLAY;
    }
}

static const EGLint _cairo_ddraw_ogl_context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};


static cairo_status_t
_cairo_ddraw_ogl_init()
{
    EGLint num_configs;
    static const EGLint config_attribs[] = {
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_NONE
    };
    EGLConfig config;
    int i;

    if (!_cairo_ddraw_ogl_get_extension_pointers ())
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_ddraw_egl_dpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);

    if (_cairo_ddraw_egl_dpy == NULL ||
	!eglInitialize (_cairo_ddraw_egl_dpy, NULL, NULL) ||
	!eglBindAPI (EGL_OPENGL_ES_API) ||
	!eglChooseConfig (_cairo_ddraw_egl_dpy, config_attribs,
			  &config, 1, &num_configs))
	return _cairo_ddraw_egl_error ("_ogl_init");

    if (num_configs == 0)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    _cairo_ddraw_egl_dummy_surface =
	eglCreatePbufferSurface (_cairo_ddraw_egl_dpy,
				 config, NULL);

    if (_cairo_ddraw_egl_dummy_surface == NULL)
	return _cairo_ddraw_egl_error ("_ogl_init");

    _cairo_ddraw_egl_dummy_ctx =
	eglCreateContext (_cairo_ddraw_egl_dpy,
			  config, 0,
			  _cairo_ddraw_ogl_context_attribs);

    if (_cairo_ddraw_egl_dummy_ctx == EGL_NO_CONTEXT)
	return _cairo_ddraw_egl_error ("_ogl_init");

    if (!eglMakeCurrent (_cairo_ddraw_egl_dpy,
			 _cairo_ddraw_egl_dummy_surface,
			 _cairo_ddraw_egl_dummy_surface,
			 _cairo_ddraw_egl_dummy_ctx))
	return _cairo_ddraw_egl_error ("_ogl_init");

    /* create temp vbo bindings */
    for (i = 0; i < CAIRO_DDRAW_OGL_NUM_SCRATCH_BUFFERS; ++i) {
	glBindBuffer (GL_ARRAY_BUFFER, i * 2 + 1);
	CHECK_OGL_ERROR ("glBindBuffer");
	glBufferData (GL_ARRAY_BUFFER, CAIRO_DDRAW_OGL_SCRATCH_VBO_SIZE,
		      NULL, GL_DYNAMIC_DRAW);
	CHECK_OGL_ERROR ("glBufferData");
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, i * 2 + 2);
	CHECK_OGL_ERROR ("glBindBuffer");
	glBufferData (GL_ELEMENT_ARRAY_BUFFER,
		      CAIRO_DDRAW_OGL_SCRATCH_IBO_SIZE,
		      NULL, GL_DYNAMIC_DRAW);
	CHECK_OGL_ERROR ("glBufferData");
    }

    glPixelStorei (GL_UNPACK_ALIGNMENT, CAIRO_STRIDE_ALIGNMENT);
    CHECK_OGL_ERROR ("glPixelStorei");
    glPixelStorei (GL_PACK_ALIGNMENT, CAIRO_STRIDE_ALIGNMENT);
    CHECK_OGL_ERROR ("glPixelStorei");

    _cairo_ddraw_ogl_next_scratch_buffer = 0;

    glGetIntegerv (GL_MAX_TEXTURE_SIZE, &_cairo_ddraw_ogl_max_texture_size);

    atexit (_cairo_ddraw_ogl_fini);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_ogl_surface_create (cairo_ddraw_surface_t *surface)
{
    GLeglImageOES egl_image = (GLeglImageOES) EGL_NO_IMAGE_KHR;
    HRESULT hr;
    HDC hdc;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (_cairo_ddraw_ogl_surface_count == 0) {
	if ((status = _cairo_ddraw_ogl_init ()))
	    return status;
    }

    surface->gl_id = 0;

    if (FAILED(hr = IDDSGetDC (surface->lpdds, &hdc)))
	return _cairo_ddraw_print_ddraw_error ("_ogl_surface_create", hr);

    egl_image = (GLeglImageOES) _eglCreateImageKHR (_cairo_ddraw_egl_dpy,
						    EGL_NO_CONTEXT,
						    EGL_NATIVE_PIXMAP_KHR,
						    (EGLNativePixmapType) hdc,
						    NULL);

    if (egl_image == (GLeglImageOES) EGL_NO_IMAGE_KHR) {
	status =_cairo_ddraw_egl_error ("_ogl_surface_create");
	goto FAIL;
    }

    glGenTextures (1, &surface->gl_id);
    CHECK_OGL_ERROR ("glGenTextures");

    glBindTexture (GL_TEXTURE_2D, surface->gl_id);
    CHECK_OGL_ERROR ("glBindTexture");

    /* make sure we don't get a mip stack lest it orphan us */

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    CHECK_OGL_ERROR ("glTexParameteri");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    CHECK_OGL_ERROR ("glTexParameteri");

    _glEGLImageTargetTexture2DOES (GL_TEXTURE_2D, egl_image);
    CHECK_OGL_ERROR ("glEGLImageTargetTexture2DOES");
    
    glBindFramebuffer (GL_FRAMEBUFFER, surface->gl_id);
    CHECK_OGL_ERROR ("glBindFramebuffer");

    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			    GL_TEXTURE_2D, surface->gl_id, 0);

    if (!_eglDestroyImageKHR (_cairo_ddraw_egl_dpy, egl_image)) {
	status = _cairo_ddraw_egl_error ("_ogl_surface_create");
	goto FAIL;
    }
    if (FAILED(hr = IDDSReleaseDC (surface->lpdds, hdc))) {
    	status = _cairo_ddraw_print_ddraw_error ("_ogl_surface_create", hr);
	goto FAIL;
    }

    egl_image = (GLeglImageOES) EGL_NO_IMAGE_KHR;

    status = _cairo_ddraw_check_ogl_error ("_ogl_surface_create");

    if (!status) {
	if (_cairo_ddraw_ogl_surface_count == 0)
	    _cairo_ddraw_ogl_check_for_flips (surface);
	++_cairo_ddraw_ogl_surface_count;
    }

 FAIL:
    if (status && egl_image != (GLeglImageOES) EGL_NO_IMAGE_KHR)
	_eglDestroyImageKHR (_cairo_ddraw_egl_dpy, egl_image);

    return status;
}

static void
_cairo_ddraw_ogl_surface_destroy (cairo_ddraw_surface_t *surface)
{
    /* may not be necessary... */
    START_TIMER(destroy);
    _cairo_ddraw_ogl_flush (surface);
    END_TIMER(destroy);

    if (surface->gl_id) {
	glDeleteFramebuffers (1, &surface->gl_id);
	CHECK_OGL_ERROR ("glDeleteFramebuffers");

	glDeleteTextures (1, &surface->gl_id);
	CHECK_OGL_ERROR ("glDeleteTextures");
    }


    assert (_cairo_ddraw_ogl_surface_count);
    _cairo_ddraw_ogl_surface_count--;

    if (_cairo_ddraw_ogl_surface_count == 0)
	_cairo_ddraw_ogl_fini ();
}

#ifdef CAIRO_DDRAW_FONT_ACCELERATION

/*
 * The plan for glyphs:
 *
 * Each font has an A8 cairo_ddraw_surface_t which caches the font glyphs.
 * The surface is a fixed-width wide, and grows vertically in chunks as needed.
 * When the cache grows, we reallocate and blit.  The surface is chunked into
 * equal-sized vertical partitions, and once we hit a max size for the cache,
 * we throw out glyphs from the oldest partition and reuse the space.  We load
 * the glyphs using the DDraw path.
 */

#ifdef CAIRO_DDRAW_OGL_FONT_STATS

#define LIST_CAIRO_DDRAW_OGL_FONT_STATS(_) \
     _(runs_drawn) \
     _(glyphs_drawn) \
     _(batches_drawn) \
     _(glyphs_added) \
     _(glyphs_deleted) \
     _(fonts_added) \
     _(fonts_deleted) \
     _(resizes) \
     _(bytes_allocated) \
     _(bytes_freed) \
     _(partition_flushes) \
     _(flushed_glyphs)


static struct font_stats {
#define DECL_FS(name) uint32_t name;
    LIST_CAIRO_DDRAW_OGL_FONT_STATS (DECL_FS)
#undef DECL_FS
} fontstats;

static void dump_font_stats (void) {
#define PRINT_FS(name) _cairo_ddraw_log ("%20s: %8lu\n", #name, fontstats.name);
    LIST_CAIRO_DDRAW_OGL_FONT_STATS (PRINT_FS)
#undef PRINT_FS
}

#define FS_ADD(stat, num) fontstats.stat += (num)
#define FS_PRINT() dump_font_stats ()

#else

#define FS_ADD(stat, num) 0
#define FS_PRINT()

#endif

typedef struct _cairo_ddraw_ogl_glyph cairo_ddraw_ogl_glyph_t;

struct _cairo_ddraw_ogl_glyph {
    union {
	cairo_scaled_glyph_t * glyph;
	cairo_ddraw_ogl_glyph_t * nextfree;
    } u;
    SrcCoordinate_t s;
    SrcCoordinate_t t;
};

typedef struct _cairo_ddraw_ogl_font {
    cairo_ddraw_surface_t * glyph_cache;
    int next_x;
    int next_y;
    int row_h;
    int height;
    int partition;
    uint32_t num_glyphs;
    cairo_ddraw_ogl_glyph_t * glyph_data;
    cairo_ddraw_ogl_glyph_t * glyph_data_end;
    cairo_ddraw_ogl_glyph_t * freelist;
} cairo_ddraw_ogl_font_t;

static cairo_ddraw_ogl_glyph_t *
_cairo_ddraw_ogl_font_get_glyph (cairo_ddraw_ogl_font_t * font)
{
    cairo_ddraw_ogl_glyph_t * glyph = font->freelist;

    if (glyph == NULL) {
	/* grow the glyph table */
	cairo_ddraw_ogl_glyph_t * oldstart = font->glyph_data;
	size_t oldsize = font->glyph_data_end - oldstart;
	size_t newsize = oldsize + CAIRO_DDRAW_OGL_FC_GLYPH_INCREMENT;
	cairo_ddraw_ogl_glyph_t * start =
	    realloc (oldstart, newsize * sizeof (cairo_ddraw_ogl_glyph_t));
	cairo_ddraw_ogl_glyph_t * last = start + oldsize;
	cairo_ddraw_ogl_glyph_t * end = start + newsize;
	cairo_ddraw_ogl_glyph_t * p;
				    
	if (start == NULL)
	    return NULL;

	font->glyph_data = start;
	font->glyph_data_end = end;

	if (start != oldstart) {
	    /* rebind the private pointers to the new addresses */
	    for (p = start; p < last; ++p) {
		cairo_scaled_glyph_t *sf = p->u.glyph;

		assert (sf->surface_private == &oldstart[p - start]);
		sf->surface_private = p;
	    }
	}

	/* add new entries to freelist */
	for (p = last; p < end; ++p) {
	    p->u.nextfree = font->freelist;
	    font->freelist = p;
	}

	glyph = font->freelist;
    }

    assert (glyph >= font->glyph_data && glyph < font->glyph_data_end &&
	    (glyph->u.nextfree == 0 ||
	     glyph->u.nextfree >= font->glyph_data &&
	     glyph->u.nextfree < font->glyph_data_end));

    font->freelist = glyph->u.nextfree;

    return glyph;
}

static void
_cairo_ddraw_ogl_font_flush_cache (cairo_ddraw_ogl_font_t * font,
				   int min_t,
				   int max_t)
{
    cairo_ddraw_ogl_glyph_t *start = font->glyph_data;
    cairo_ddraw_ogl_glyph_t *end = font->glyph_data_end;
    cairo_ddraw_ogl_glyph_t *p;

    for (p = start; p < end; ++p) {
	cairo_scaled_glyph_t *glyph = p->u.glyph;

	/* skip freelist entries */
	if (p->u.nextfree >= start && p->u.nextfree < end ||
	    p->u.nextfree == 0)
	    continue;

	/* skip glyphs that start outside of range */
	if (p->t < min_t || p->t >= max_t)
	    continue;

	assert (glyph->surface_private == p);
	glyph->surface_private = NULL;

	p->u.nextfree = font->freelist;
	font->freelist = p;

	FS_ADD (flushed_glyphs, 1);
    }
}

static cairo_int_status_t
_cairo_ddraw_ogl_add_glyph (cairo_ddraw_ogl_font_t * font,
			    cairo_image_surface_t * image,
			    cairo_ddraw_ogl_glyph_t *glyphinfo,
			    LPDIRECTDRAW lpdd)
{
    int width = image->width;
    int height = image->height;
    cairo_int_status_t status;
    unsigned char * srcptr;
    unsigned char * dstptr;
    int srcstride;
    int dststride;
    int lines;

    if (width > CAIRO_DDRAW_OGL_FC_WIDTH ||
	height > CAIRO_DDRAW_OGL_FC_PARTITION_HEIGHT)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (font->next_x + width > CAIRO_DDRAW_OGL_FC_WIDTH) {
	font->next_y += font->row_h;
	font->next_x = font->row_h = 0;
    }

    if (font->next_y + height >
	font->partition + CAIRO_DDRAW_OGL_FC_PARTITION_HEIGHT) {
	font->partition += CAIRO_DDRAW_OGL_FC_PARTITION_HEIGHT;
	if (font->partition >= CAIRO_DDRAW_OGL_FC_MAX_HEIGHT)
	    font->partition = 0;
	if (font->partition < font->height) {
	    int end = font->partition +
		CAIRO_DDRAW_OGL_FC_PARTITION_HEIGHT;

	    _cairo_ddraw_ogl_font_flush_cache (font,
					       font->partition,
					       end);

	    FS_ADD(partition_flushes, 1);
	}
	font->next_y = font->partition;
	font->next_x = font->row_h = 0;
    }

    if (font->next_y + height > font->height) {
	int new_height =
	    (font->next_y + height +
	     CAIRO_DDRAW_OGL_FC_HEIGHT_INCREMENT - 1) /
	    CAIRO_DDRAW_OGL_FC_HEIGHT_INCREMENT *
	    CAIRO_DDRAW_OGL_FC_HEIGHT_INCREMENT;
	cairo_ddraw_surface_t *new_glyph_cache;

	if (new_height < CAIRO_DDRAW_OGL_FC_INITIAL_HEIGHT)
	    new_height = CAIRO_DDRAW_OGL_FC_INITIAL_HEIGHT;

	new_glyph_cache = (cairo_ddraw_surface_t *)
	    cairo_ddraw_surface_create (lpdd,
					CAIRO_FORMAT_A8,
					CAIRO_DDRAW_OGL_FC_WIDTH,
					new_height);

	if (new_glyph_cache->base.status)
	    return new_glyph_cache->base.status;

	FS_ADD(resizes, 1);
	FS_ADD(bytes_allocated,
	       new_height * CAIRO_DDRAW_OGL_FC_WIDTH);

	if (font->glyph_cache) {
	    HRESULT hr;
	    RECT rect;

	    /* copy old surface */

	    START_TIMER(growglyphcache);
	    _cairo_ddraw_ogl_flush (font->glyph_cache);

	    if ((status = _cairo_ddraw_surface_unlock (font->glyph_cache)))
		return status;
	    END_TIMER(growglyphcache);

	    assert (!new_glyph_cache->locked &&
		    !new_glyph_cache->has_clip_region &&
		    !new_glyph_cache->clip_list_invalid);

	    rect.left = 0;
	    rect.right = CAIRO_DDRAW_OGL_FC_WIDTH;
	    rect.top = 0;
	    rect.bottom = font->height;
	    if (FAILED(hr = IDDSBlt (new_glyph_cache->lpdds, &rect,
				     font->glyph_cache->lpdds, &rect,
				     DDBLT_WAITNOTBUSY, NULL)))
		return _cairo_ddraw_print_ddraw_error ("_ogl_add_glyph", hr);

	    cairo_surface_destroy ((cairo_surface_t *)font->glyph_cache);

	    FS_ADD(bytes_freed,
		   font->height * CAIRO_DDRAW_OGL_FC_WIDTH);
	}

	font->glyph_cache = new_glyph_cache;
	font->height = new_height;
    }

    START_TIMER(addglyph);
    _cairo_ddraw_ogl_flush (font->glyph_cache);

    if ((status = _cairo_ddraw_surface_lock (font->glyph_cache)))
	return status;
    END_TIMER(addglyph);

    srcstride = image->stride;
    srcptr = image->data;
    dststride = cairo_image_surface_get_stride (font->glyph_cache->image);
    dstptr = cairo_image_surface_get_data (font->glyph_cache->image) +
	font->next_y * dststride + font->next_x;

    lines = height;
    while (lines) {
	--lines;
	memcpy (dstptr, srcptr, width);
	dstptr += dststride;
	srcptr += srcstride;
    }

    glyphinfo->s = font->next_x;
    glyphinfo->t = font->next_y;

    font->next_x += width;
    if (height > font->row_h)
	font->row_h = height;

    ++font->num_glyphs;

    FS_ADD(glyphs_added, 1);

    return CAIRO_STATUS_SUCCESS;
}

static void
_cairo_ddraw_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_ddraw_ogl_font_t * font =
	(cairo_ddraw_ogl_font_t *) scaled_font->surface_private;

    CAIRO_DDRAW_API_ENTRY_VOID;

    if (font) {
	if (font->glyph_cache) {
	    cairo_surface_destroy ((cairo_surface_t *) font->glyph_cache);
	    FS_ADD(bytes_freed,
		   font->height * CAIRO_DDRAW_OGL_FC_WIDTH);
	}
	free (font->glyph_data);
	free (font);

	FS_ADD(fonts_deleted, 1);
    }

    scaled_font->surface_private = NULL;
}

static void
_cairo_ddraw_surface_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
					cairo_scaled_font_t *scaled_font)
{
    cairo_ddraw_ogl_font_t * font = scaled_font->surface_private;
    cairo_ddraw_ogl_glyph_t * glyph = scaled_glyph->surface_private;

    CAIRO_DDRAW_API_ENTRY_VOID;

    assert (font);

    if (glyph) {
	assert (glyph->u.glyph == scaled_glyph &&
		glyph >= font->glyph_data &&
		glyph < font->glyph_data_end);

	--font->num_glyphs;

	glyph->u.nextfree = font->freelist;
	font->freelist = glyph;

	scaled_glyph->surface_private = NULL;

	FS_ADD(glyphs_deleted, 1);
    }
}

static cairo_int_status_t
_cairo_ddraw_ogl_bind_font_program (cairo_ddraw_surface_t *dst,
				    cairo_ddraw_ogl_font_t *font,
				    cairo_color_t *color,
				    cairo_operator_t op)
{
    cairo_ddraw_surface_t * root = dst->root;
    int shader_id = MAKE_SHADER_ID (op,
				    CAIRO_DDRAW_OGL_SRC_SOLID,
				    CAIRO_DDRAW_OGL_MASK_A);
    cairo_ddraw_ogl_program_info_t * info;
    cairo_status_t status;

    if (dst->base.content == CAIRO_CONTENT_COLOR)
	op = _cairo_ddraw_ogl_op_downgrade[op];

    info = _cairo_ddraw_ogl_load_program (shader_id);

    if (info == NULL)
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    glUseProgram (info->prog_id);
    CHECK_OGL_ERROR ("glUseProgram");

    glUniform1i (info->mask_sampler_uniform, 0);
    CHECK_OGL_ERROR ("glUniform1i mask sampler");

    glVertexAttribPointer (info->pos_attr, 2,
			   DST_COORDINATE_TYPE, GL_FALSE,
			   sizeof (cairo_ddraw_ogl_glyph_vtx_t),
			   (GLvoid *) offsetof (cairo_ddraw_ogl_glyph_vtx_t, x));
    CHECK_OGL_ERROR ("glVertexAttribPointer pos");

    glEnableVertexAttribArray (info->pos_attr);
    CHECK_OGL_ERROR ("glEnableVertexAttribArray pos");

    if (_cairo_ddraw_ogl_dst_flipped)
	glUniform4f (info->pos_xform_uniform,
		     2.0f / root->extents.width, -1.0f,
		     -2.0f / root->extents.height, 1.0f);
    else
	glUniform4f (info->pos_xform_uniform,
		     2.0f / root->extents.width, -1.0f,
		     2.0f / root->extents.height, -1.0f);
    CHECK_OGL_ERROR ("glUniform4f pos xform");

    glVertexAttribPointer (info->mask_attr, 2,
			   SRC_COORDINATE_TYPE, GL_FALSE,
			   sizeof (cairo_ddraw_ogl_glyph_vtx_t),
			   (GLvoid *) offsetof (cairo_ddraw_ogl_glyph_vtx_t, s));
    CHECK_OGL_ERROR ("glVertexAttribPointer mask");

    glEnableVertexAttribArray (info->mask_attr);
    CHECK_OGL_ERROR ("glEnableVertexAttribArray mask");

    glUniform3f (info->mask_x_xform_uniform,
		 1.0f / CAIRO_DDRAW_OGL_FC_WIDTH, 0.0f, 0.0f);
    CHECK_OGL_ERROR ("glUniform3f mask xform x");

    if (_cairo_ddraw_ogl_src_flipped)
	glUniform3f (info->mask_y_xform_uniform,
		     0.0f, -1.0f / font->height, 1.0f);
    else
	glUniform3f (info->mask_y_xform_uniform,
		     0.0f, 1.0f / font->height, 0.0f);
    CHECK_OGL_ERROR ("glUniform3f mask xform y");

    if (op != CAIRO_OPERATOR_CLEAR && op != CAIRO_OPERATOR_DEST) {
	glUniform4f (info->src_color_uniform,
		     color->red_short * (1.0f / 65535.0f),
		     color->green_short * (1.0f / 65535.0f),
		     color->blue_short * (1.0f / 65535.0f),
		     color->alpha_short * (1.0f / 65535.0f));
	CHECK_OGL_ERROR ("glUniform4f src color");
    }

    glActiveTexture (GL_TEXTURE0);
    CHECK_OGL_ERROR ("glActiveTexture");

    glBindTexture (GL_TEXTURE_2D, font->glyph_cache->gl_id);
    CHECK_OGL_ERROR ("glBindTexture");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    CHECK_OGL_ERROR ("glTexParameteri");

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CHECK_OGL_ERROR ("glTexParameteri");

    glViewport (0, 0, root->extents.width, root->extents.height);
    CHECK_OGL_ERROR ("glViewport");

    if ((status = _cairo_ddraw_ogl_reference (font->glyph_cache)))
	return status;

    return _cairo_ddraw_surface_bind_to_ogl (dst);
}

static cairo_int_status_t
_cairo_ddraw_surface_show_glyphs (void		         *abstract_dst,
				  cairo_operator_t	 op,
				  const cairo_pattern_t  *pattern,
				  cairo_glyph_t	         *glyphs,
				  int		         num_glyphs,
				  cairo_scaled_font_t    *scaled_font,
				  int		         *remaining_glyphs,
				  cairo_rectangle_int_t  *extents)
{ 
    cairo_ddraw_surface_t * dst = (cairo_ddraw_surface_t *) abstract_dst;
    cairo_color_t * color;
    cairo_ddraw_ogl_font_t * font = scaled_font->surface_private;
    GLuint vbo;
    cairo_ddraw_ogl_glyph_quad_t * verts;
    cairo_ddraw_ogl_glyph_idx_t * indices;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    int glyph;
    int batch_size;
    enum {
	GLYPH_MAX_BATCH_SIZE =
	MAX_SCRATCH_COUNT (sizeof (cairo_ddraw_ogl_glyph_quad_t),
			   sizeof (cairo_ddraw_ogl_glyph_idx_t))
    };

    CAIRO_DDRAW_API_ENTRY_STATUS;

    assert (num_glyphs);

    if (pattern->type != CAIRO_PATTERN_TYPE_SOLID)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (dst->base.clip &&
	(dst->base.clip->mode != CAIRO_CLIP_MODE_REGION ||
	 dst->base.clip->surface != NULL))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    color = &((cairo_solid_pattern_t *)pattern)->color;

    if (font == NULL) {
	font = calloc (1, sizeof (cairo_ddraw_ogl_font_t));
	if (font == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	scaled_font->surface_private = font;
	scaled_font->surface_backend = &_cairo_ddraw_surface_backend;

	FS_ADD(fonts_added, 1);
    }

    glyph = 0;
    while (glyph < num_glyphs) {
	int num_quads;
	Index_t quad;

	batch_size = num_glyphs - glyph;
	if (batch_size > GLYPH_MAX_BATCH_SIZE)
	    batch_size = GLYPH_MAX_BATCH_SIZE;

	vbo = _cairo_ddraw_ogl_get_scratch_buffer ();

	glBindBuffer (GL_ARRAY_BUFFER, vbo);
	CHECK_OGL_ERROR ("glBindBuffer vbo");

	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, vbo + 1);
	CHECK_OGL_ERROR ("glBindBuffer ibo");

	verts = _glMapBufferOES (GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
	if (verts == NULL)
	    return _cairo_ddraw_check_ogl_error ("_show_glyphs");

	indices = _glMapBufferOES (GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
	if (indices == NULL) {
	    _glUnmapBufferOES (GL_ARRAY_BUFFER);
	    return _cairo_ddraw_check_ogl_error("_show_glyphs");
	}

	_cairo_scaled_font_freeze_cache (scaled_font);

	num_quads = batch_size * 4;
	for (quad = 0; quad < num_quads; quad += 4) {
	    cairo_scaled_glyph_t *scaled_glyph;
	    cairo_image_surface_t *image;
	    cairo_ddraw_ogl_glyph_quad_t vtx;
	    cairo_ddraw_ogl_glyph_idx_t idx;
	    cairo_ddraw_ogl_glyph_t * glyphinfo;

	    if ((status =
		 _cairo_scaled_glyph_lookup (scaled_font,
					     glyphs[glyph].index,
					     CAIRO_SCALED_GLYPH_INFO_SURFACE,
					     &scaled_glyph)))
		goto THAW;

	    image = scaled_glyph->surface;

	    /* XXX more formats? */
	    if (image->format != CAIRO_FORMAT_A8) {
		status = CAIRO_INT_STATUS_UNSUPPORTED;
		goto THAW;
	    }

	    glyphinfo = scaled_glyph->surface_private;
	
	    if (glyphinfo == NULL) {
		glyphinfo = _cairo_ddraw_ogl_font_get_glyph (font);
		if (glyphinfo == NULL) {
		    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
		    goto THAW;
		}

		scaled_glyph->surface_private = glyphinfo;
		glyphinfo->u.glyph = scaled_glyph;

		status = _cairo_ddraw_ogl_add_glyph (font,
						     image,
						     glyphinfo,
						     dst->lpdd);

		if (status) {
		    _cairo_ddraw_surface_scaled_glyph_fini (scaled_glyph,
							    scaled_font);
		    goto THAW;
		}
	    }

	    vtx.vtx[0].x = vtx.vtx[1].x = (DstCoordinate_t)
		_cairo_lround (glyphs[glyph].x -
			       image->base.device_transform.x0 +
			       dst->origin.x);
	    vtx.vtx[0].y = vtx.vtx[2].y = (DstCoordinate_t)
		_cairo_lround (glyphs[glyph].y -
			       image->base.device_transform.y0 +
			       dst->origin.y);

	    vtx.vtx[2].x = vtx.vtx[3].x =
		(DstCoordinate_t) (vtx.vtx[0].x + image->width);
	    vtx.vtx[1].y = vtx.vtx[3].y =
		(DstCoordinate_t) (vtx.vtx[0].y + image->height);

	    vtx.vtx[0].s = vtx.vtx[1].s = glyphinfo->s;
	    vtx.vtx[0].t = vtx.vtx[2].t = glyphinfo->t;

	    vtx.vtx[2].s = vtx.vtx[3].s =
		(SrcCoordinate_t) (vtx.vtx[0].s + image->width);
	    vtx.vtx[1].t = vtx.vtx[3].t =
		(SrcCoordinate_t) (vtx.vtx[0].t + image->height);

	    *verts++ = vtx;

	    idx.idx[0] = quad;
	    idx.idx[1] = idx.idx[4] = quad + 1;
	    idx.idx[2] = idx.idx[3] = quad + 2;
	    idx.idx[5] = quad + 3;

	    *indices++ = idx;

	    ++glyph;

	    FS_ADD(glyphs_drawn, 1);
	}

    THAW:
	_cairo_scaled_font_thaw_cache (scaled_font);

	_glUnmapBufferOES (GL_ARRAY_BUFFER);
	CHECK_OGL_ERROR ("glUnmapBufferOES vbo");

	_glUnmapBufferOES (GL_ELEMENT_ARRAY_BUFFER);
	CHECK_OGL_ERROR ("glUnmapBufferOES ibo");

	if (status)
	    return status;

	if ((status = _cairo_ddraw_ogl_bind_font_program (dst,
							  font,
							  color,
							  op)))
	    return status;

	if (dst->has_clip_region) {
	    int num_rects = cairo_region_num_rectangles (&dst->clip_region);
	    int offx = dst->extents.x + dst->origin.x;
	    int offy = dst->extents.y + dst->origin.y;
	    int surf_h = dst->root->extents.height;
	    int i;

	    glEnable (GL_SCISSOR_TEST);
	    CHECK_OGL_ERROR ("glEnable scissor");

	    for (i = 0; i < num_rects; ++i) {
		cairo_rectangle_int_t rect;

		cairo_region_get_rectangle (&dst->clip_region, i, &rect);

		if (_cairo_ddraw_ogl_dst_flipped)
		    glScissor (rect.x + offx,
			       surf_h - (rect.y + rect.height + offy),
			       rect.width, rect.height);
		else
		    glScissor (rect.x + offx, rect.y + offy,
			       rect.width, rect.height);
		CHECK_OGL_ERROR ("glScissor");

		glDrawElements (GL_TRIANGLES, batch_size * 6,
				INDEX_TYPE, 0);
		CHECK_OGL_ERROR ("glDrawElements");
	    }
	} else {
	    glDisable (GL_SCISSOR_TEST);
	    CHECK_OGL_ERROR ("glDisable scissor");

	    glDrawElements (GL_TRIANGLES, batch_size * 6,
			    INDEX_TYPE, 0);
	    CHECK_OGL_ERROR ("glDrawElements");
	}

	FS_ADD(batches_drawn, 1);
    }

    FS_ADD(runs_drawn, 1);

    return CAIRO_STATUS_SUCCESS;
}

#endif /* CAIRO_DDRAW_FONT_ACCELERATION */

#ifdef CAIRO_DDRAW_COMPOSITE_ACCELERATION

static cairo_int_status_t
_cairo_ddraw_surface_composite (cairo_operator_t	op,
				const cairo_pattern_t	*src,
				const cairo_pattern_t	*mask,
				void                    *abstract_dst,
				int			src_x,
				int			src_y,
				int			mask_x,
				int			mask_y,
				int			dst_x,
				int			dst_y,
				unsigned int		width,
				unsigned int		height)
{
    cairo_ddraw_surface_t * dst = (cairo_ddraw_surface_t *) abstract_dst;
    cairo_ddraw_ogl_program_info_t * info = NULL;
    cairo_int_status_t status = CAIRO_INT_STATUS_UNSUPPORTED;
    cairo_surface_t * mask_surface = NULL;
    cairo_surface_t * src_surface = NULL;
    cairo_ddraw_surface_t * mask_ddraw_surface = NULL;
    cairo_ddraw_surface_t * src_ddraw_surface = NULL;
    cairo_rectangle_int_t mask_extents;
    cairo_rectangle_int_t src_extents;
    cairo_rectangle_int_t mask_bbox;
    cairo_rectangle_int_t src_bbox;
    cairo_ddraw_ogl_quad_t quad;
    cairo_ddraw_ogl_quad_t * verts = NULL;
    GLuint vbo = _cairo_ddraw_ogl_get_scratch_buffer ();
    GLsizei vbosize = offsetof (cairo_ddraw_ogl_quad_t, src);
    GLenum param;
    cairo_ddraw_ogl_texemu_type_t src_texemu = CAIRO_DDRAW_OGL_TEXEMU_NONE;
    cairo_ddraw_ogl_texture_type_t mask_tex_type =
	CAIRO_DDRAW_OGL_TEXTURE_UNUSED;
    cairo_ddraw_ogl_texture_type_t src_tex_type;

    CAIRO_DDRAW_API_ENTRY_STATUS;

    /*XXX TODO - emulate hard textures with shader magic */

    if (op == CAIRO_OPERATOR_DEST)
	return CAIRO_STATUS_SUCCESS;

    /* bail out for source copies that aren't ddraw surfaces) */
    if (src->type == CAIRO_PATTERN_TYPE_SURFACE &&
	(((cairo_surface_pattern_t *) src)->surface->type 
	 != CAIRO_SURFACE_TYPE_DDRAW)) {
#ifdef CAIRO_DDRAW_DEBUG_VERBOSE
	_cairo_ddraw_log ("composite of non ddraw surface %d,%d\n", width, height);
#endif
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }
    
    if (mask) {
	mask_tex_type =
	    _cairo_ddraw_ogl_analyze_pattern (mask,
					      mask_x,
					      mask_y,
					      width,
					      height,
					      &mask_extents,
					      &mask_bbox);
	
	switch (mask_tex_type) {
	case CAIRO_DDRAW_OGL_TEXTURE_UNUSED:
	case CAIRO_DDRAW_OGL_TEXTURE_IGNORE_WRAP:
	    break;
	case CAIRO_DDRAW_OGL_TEXTURE_SUPPORTED:
	    /* need the whole texture */
	    mask_bbox = mask_extents;
	    break;
	default:
#ifdef CAIRO_DDRAW_DEBUG_VERBOSE
	    _cairo_ddraw_log ("composite: unhandled mask_tex_type %d\n",
			      mask_tex_type);
#endif
	    return CAIRO_INT_STATUS_UNSUPPORTED;
	}
    }
    
    src_tex_type =
	_cairo_ddraw_ogl_analyze_pattern (src,
					  src_x,
					  src_y,
					  width,
					  height,
					  &src_extents,
					  &src_bbox);
    
    switch (src_tex_type) {
    case CAIRO_DDRAW_OGL_TEXTURE_UNUSED:
    case CAIRO_DDRAW_OGL_TEXTURE_IGNORE_WRAP:
	break;
    case CAIRO_DDRAW_OGL_TEXTURE_SUPPORTED:
	/* need the whole texture */
	src_bbox = src_extents;
	break;
    case CAIRO_DDRAW_OGL_TEXTURE_BORDER_IN_SHADER:
	src_texemu = CAIRO_DDRAW_OGL_TEXEMU_BORDER;
	break;
    case CAIRO_DDRAW_OGL_TEXTURE_WRAP_IN_SHADER:
	src_texemu = CAIRO_DDRAW_OGL_TEXEMU_WRAP;
	break;
    default:
#ifdef CAIRO_DDRAW_DEBUG_VERBOSE
	_cairo_ddraw_log ("composite: unhandled src_tex_type %d\n",
			  src_tex_type);
#endif
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    CHECK_OGL_ERROR ("glBindBuffer vbo");

    verts = _glMapBufferOES (GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES);
    if (verts == NULL)
	return _cairo_ddraw_check_ogl_error ("_surface_composite");

    dst_x += dst->origin.x;
    dst_y += dst->origin.y;

    quad.dst[0].x = dst_x;
    quad.dst[0].y = dst_y;
    quad.dst[1].x = dst_x;
    quad.dst[1].y = dst_y + height;
    quad.dst[2].x = dst_x + width;
    quad.dst[2].y = dst_y;
    quad.dst[3].x = dst_x + width;
    quad.dst[3].y = dst_y + height;

    if (src->type == CAIRO_PATTERN_TYPE_SURFACE && 
	op != CAIRO_OPERATOR_CLEAR) {
	cairo_surface_t * surface =
	    ((cairo_surface_pattern_t *) src)->surface;
	int clone_x, clone_y;

	vbosize = offsetof (cairo_ddraw_ogl_quad_t, mask);

	glActiveTexture (GL_TEXTURE1);
	CHECK_OGL_ERROR ("glActiveTexture src");

	param =
	    (src->filter == CAIRO_FILTER_NEAREST) ? GL_NEAREST :
	    GL_LINEAR;
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
	CHECK_OGL_ERROR ("glTexParameteri src filter");
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
	CHECK_OGL_ERROR ("glTexParameteri src filter");

	param =
	    (src_tex_type !=
	     CAIRO_DDRAW_OGL_TEXTURE_SUPPORTED) ? GL_CLAMP_TO_EDGE :
	    (src->extend == CAIRO_EXTEND_PAD) ? GL_CLAMP_TO_EDGE :
	    (src->extend == CAIRO_EXTEND_REPEAT) ? GL_REPEAT :
	    (src->extend == CAIRO_EXTEND_REFLECT) ? GL_MIRRORED_REPEAT :
	    GL_CLAMP_TO_EDGE;
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, param);
	CHECK_OGL_ERROR ("glTexParameteri src wrap");
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, param);
	CHECK_OGL_ERROR ("glTexParameteri src wrap");

	status =
	    _cairo_ddraw_surface_clone_similar (dst,
						surface,
						src_bbox.x,
						src_bbox.y,
						src_bbox.width,
						src_bbox.height,
						&clone_x,
						&clone_y,
						&src_surface);

	if (status)
	    goto FAIL;

	quad.src[0].s = src_x;
	quad.src[0].t = src_y;
	quad.src[1].s = src_x;
	quad.src[1].t = src_y + height;
	quad.src[2].s = src_x + width;
	quad.src[2].t = src_y;
	quad.src[3].s = src_x + width;
	quad.src[3].t = src_y + height;
		
	src_ddraw_surface = (cairo_ddraw_surface_t *) src_surface;

	src_extents.x = clone_x;
	src_extents.y = clone_y;
	src_extents.width = src_ddraw_surface->extents.width;
	src_extents.height = src_ddraw_surface->extents.height;

	if ((status = _cairo_ddraw_ogl_reference (src_ddraw_surface)))
	    goto FAIL;

	glBindTexture (GL_TEXTURE_2D, src_ddraw_surface->gl_id);
	CHECK_OGL_ERROR ("glBindTexture src");
    }

    if (mask && mask->type == CAIRO_PATTERN_TYPE_SURFACE) {
	cairo_surface_t * surface =
	    ((cairo_surface_pattern_t *) mask)->surface;
	int clone_x, clone_y;

	vbosize = sizeof (cairo_ddraw_ogl_quad_t);

	glActiveTexture (GL_TEXTURE0);
	CHECK_OGL_ERROR ("glActiveTexture mask");

	param =
	    (mask->filter == CAIRO_FILTER_NEAREST) ? GL_NEAREST :
	    GL_LINEAR;
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, param);
	CHECK_OGL_ERROR ("glTexParameteri mask filter");
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, param);
	CHECK_OGL_ERROR ("glTexParameteri mask filter");

	param =
	    (mask_tex_type !=
	     CAIRO_DDRAW_OGL_TEXTURE_SUPPORTED) ? GL_CLAMP_TO_EDGE :
	    (mask->extend == CAIRO_EXTEND_PAD) ? GL_CLAMP_TO_EDGE :
	    (mask->extend == CAIRO_EXTEND_REPEAT) ? GL_REPEAT :
	    (mask->extend == CAIRO_EXTEND_REFLECT) ? GL_MIRRORED_REPEAT :
	    GL_CLAMP_TO_EDGE;
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, param);
	CHECK_OGL_ERROR ("glTexParameteri mask wrap");
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, param);
	CHECK_OGL_ERROR ("glTexParameteri mask wrap");

	status =
	    _cairo_ddraw_surface_clone_similar (dst,
						surface,
						mask_bbox.x,
						mask_bbox.y,
						mask_bbox.width,
						mask_bbox.height,
						&clone_x,
						&clone_y,
						&mask_surface);

	if (status)
	    goto FAIL;

	quad.mask[0].s = mask_x;
	quad.mask[0].t = mask_y;
	quad.mask[1].s = mask_x;
	quad.mask[1].t = mask_y + height;
	quad.mask[2].s = mask_x + width;
	quad.mask[2].t = mask_y;
	quad.mask[3].s = mask_x + width;
	quad.mask[3].t = mask_y + height;
		
	mask_ddraw_surface = (cairo_ddraw_surface_t *) mask_surface;

	mask_extents.x = clone_x;
	mask_extents.y = clone_y;
	mask_extents.width = mask_ddraw_surface->extents.width;
	mask_extents.height = mask_ddraw_surface->extents.height;

	if ((status = _cairo_ddraw_ogl_reference (mask_ddraw_surface)))
	    goto FAIL;

	glBindTexture (GL_TEXTURE_2D, mask_ddraw_surface->gl_id);
	CHECK_OGL_ERROR ("glBindTexture mask");
    }

    memcpy (verts, &quad, vbosize);

    _glUnmapBufferOES (GL_ARRAY_BUFFER);
    CHECK_OGL_ERROR ("glUnmapBufferOES vbo");

    verts = NULL;

    status =
	_cairo_ddraw_ogl_bind_program (&info,
				       dst,
				       src,
				       mask,
				       op,
				       src_texemu,
				       &src_extents,
				       &mask_extents);

    if (status)
	goto FAIL;

    if (dst->has_clip_region) {
	int offx = dst->extents.x + dst->origin.x;
	int offy = dst->extents.y + dst->origin.y;
	int num_rects = cairo_region_num_rectangles (&dst->clip_region);
	int dst_surf_h = dst->root->extents.height;
	int i;

	glEnable (GL_SCISSOR_TEST);
	CHECK_OGL_ERROR ("glEnable scissor");

	for (i = 0; i < num_rects; ++i) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (&dst->clip_region, i, &rect);

	    if (_cairo_ddraw_ogl_dst_flipped)
		glScissor (rect.x + offx,
			   dst_surf_h - (rect.y + rect.height + offy),
			   rect.width, rect.height);
	    else
		glScissor (rect.x + offx, rect.y + offy,
			   rect.width, rect.height);
	    CHECK_OGL_ERROR ("glScissor");

	    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
	    CHECK_OGL_ERROR ("glDrawArrays");
	}
    } else {
	glDisable (GL_SCISSOR_TEST);
	CHECK_OGL_ERROR ("glDisable scissor");

	glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
	CHECK_OGL_ERROR ("glDrawArrays");
    }

    status = CAIRO_STATUS_SUCCESS;

 FAIL:

    if (verts) {
	_glUnmapBufferOES (GL_ARRAY_BUFFER);
	CHECK_OGL_ERROR ("glUnmapBufferOES vbo");
    }

    if (mask_surface)
	cairo_surface_destroy (mask_surface);

    if (src_surface)
	cairo_surface_destroy (src_surface);

    return status;
}

#endif /* CAIRO_DDRAW_COMPOSITE_ACCELERATION */


#endif /* CAIRO_DDRAW_USE_GL */

static cairo_status_t
_cairo_ddraw_surface_finish (void *abstract_surface)
{
    cairo_ddraw_surface_t *surface = abstract_surface;
    ULONG count;

    CAIRO_DDRAW_API_ENTRY_STATUS;

    if (surface->image)
	cairo_surface_destroy (surface->image);

    if (surface->lpddc) {
	count = IURelease(surface->lpddc);
	assert (count == 0);
    }

    _cairo_region_fini (&surface->clip_region);

    if (surface->root != surface)
	cairo_surface_destroy ((cairo_surface_t *) surface->root);
    else {
#ifdef CAIRO_DDRAW_OGL_FONT_STATS
	extern void dump_font_stats ();
	
	dump_font_stats ();
#endif

#ifdef CAIRO_DDRAW_USE_GL
	_cairo_ddraw_ogl_surface_destroy (surface);
#endif
	if (surface->lpdds) {
	    count = IURelease (surface->lpdds);
	    assert (count == 0);
	}

	if (surface->lpdd)
	    count = IURelease (surface->lpdd);
	
#ifdef CAIRO_DDRAW_LOCK_TIMES
	_cairo_ddraw_dump_timers ();
#endif
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_surface_acquire_source_image (void                    *abstract_surface,
					   cairo_image_surface_t  **image_out,
					   void                   **image_extra)
{
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_status_t status;

    CAIRO_DDRAW_API_ENTRY_STATUS;

    if (surface->root != surface)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    START_TIMER(acquiresrc);
#ifdef CAIRO_DDRAW_USE_GL
    _cairo_ddraw_ogl_flush (surface);
#endif

    if ((status = _cairo_ddraw_surface_lock (surface)))
	return status;
    END_TIMER(acquiresrc);

    *image_out = (cairo_image_surface_t *) surface->image;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_ddraw_surface_acquire_dest_image (void                    *abstract_surface,
					 cairo_rectangle_int_t   *interest_rect,
					 cairo_image_surface_t  **image_out,
					 cairo_rectangle_int_t   *image_rect,
					 void                   **image_extra)
{
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_status_t status;

    CAIRO_DDRAW_API_ENTRY_STATUS;

    START_TIMER(acquiredst);
#ifdef CAIRO_DDRAW_USE_GL
    _cairo_ddraw_ogl_flush (surface);
#endif

    if ((status = _cairo_ddraw_surface_lock (surface)))
	return status;
    END_TIMER(acquiredst);

    if ((status = _cairo_ddraw_surface_set_image_clip (surface)))
	return status;

    *image_out = (cairo_image_surface_t *) surface->image;
    *image_rect = surface->extents;
    *image_extra = NULL;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_cairo_ddraw_surface_get_extents (void		          *abstract_surface,
				  cairo_rectangle_int_t   *rectangle)
{
    cairo_ddraw_surface_t *surface = abstract_surface;

    *rectangle = surface->extents;

    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
_cairo_ddraw_surface_set_clip_region (void *abstract_surface,
				      cairo_region_t *region)
{
    cairo_ddraw_surface_t * surface =
	(cairo_ddraw_surface_t *) abstract_surface;

    if (region) {
	cairo_region_t *tmp_region;
	surface->has_clip_region = TRUE;
	surface->image_clip_invalid = TRUE;
	surface->clip_list_invalid = TRUE;

	tmp_region = &surface->clip_region;
	if (! pixman_region32_copy (&tmp_region->rgn, &region->rgn))
	    return CAIRO_STATUS_NO_MEMORY;

	if (surface->extents.x || surface->extents.y)
	    cairo_region_translate (&surface->clip_region,
				    -surface->extents.x,
				    -surface->extents.y);
    } else {
	surface->has_clip_region = FALSE;
	surface->image_clip_invalid = surface->has_image_clip;
    }

    return CAIRO_STATUS_SUCCESS;
}

/**
 * cairo_ddraw_surface_create:
 * @lpdd: pointer to a DirectDraw object
 * @format: format of pixels in the surface to create
 * @width: width of the surface, in pixels
 * @height: height of the surface, in pixels
 *
 * Creates a DirectDraw surface not associated with any particular existing
 * surface.  The created surface will be uninitialized.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_ddraw_surface_create (LPDIRECTDRAW lpdd,
			    cairo_format_t format,
			    int	    width,
			    int	    height)
{
    cairo_ddraw_surface_t *surface;
    HRESULT hr;
    DDSURFACEDESC ddsd;
    cairo_status_t status;

    CAIRO_DDRAW_API_ENTRY_SURFACE;

    if (format != CAIRO_FORMAT_ARGB32 &&
	format != CAIRO_FORMAT_RGB24 &&
	format != CAIRO_FORMAT_A8)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

    if (lpdd == NULL) {
	if (FAILED(hr = DirectDrawCreate (NULL, &lpdd, NULL)) ||
	    FAILED(hr = IDDSetCooperativeLevel (lpdd, NULL, DDSCL_NORMAL))) {
	    status = _cairo_ddraw_print_ddraw_error ("_surface_create", hr);
	    return _cairo_surface_create_in_error (status);
	}
    } else
	IUAddRef (lpdd);

    surface = malloc (sizeof (cairo_ddraw_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    memset (&ddsd, 0, sizeof (ddsd));
    ddsd.dwSize = sizeof (ddsd);
    ddsd.dwFlags = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    ddsd.dwHeight = height ? height : 1;
    ddsd.dwWidth = width ? width : 1;
    ddsd.ddpfPixelFormat.dwSize = sizeof (ddsd.ddpfPixelFormat);
    if (format == CAIRO_FORMAT_A8) {
	ddsd.ddpfPixelFormat.dwFlags = DDPF_ALPHA;
	ddsd.ddpfPixelFormat.dwAlphaBitDepth = 8;
    } else {
	/* use ARGB32 for RGB24 */
	ddsd.ddpfPixelFormat.dwFlags =
	    DDPF_ALPHAPIXELS | DDPF_ALPHAPREMULT | DDPF_RGB;
	ddsd.ddpfPixelFormat.dwRGBBitCount = 32;
	ddsd.ddpfPixelFormat.dwRBitMask = 0x00ff0000;
	ddsd.ddpfPixelFormat.dwGBitMask = 0x0000ff00;
	ddsd.ddpfPixelFormat.dwBBitMask = 0x000000ff;
	ddsd.ddpfPixelFormat.dwRGBAlphaBitMask = 0xff000000;
    }

    if (FAILED(hr = IDDCreateSurface (lpdd, &ddsd, &surface->lpdds, NULL))) {
	status = _cairo_ddraw_print_ddraw_error ("_surface_create", hr);
	IURelease(lpdd);
	free (surface);
	return _cairo_surface_create_in_error (status);
    }

    surface->format = format;

    if (FAILED(hr = IDDCreateClipper (lpdd, 0, &surface->lpddc, NULL))) {
	status = _cairo_ddraw_print_ddraw_error ("_surface_create", hr);
	IURelease(surface->lpdds);
	free (surface);
	return _cairo_surface_create_in_error (status);
    }

#ifdef CAIRO_DDRAW_USE_GL
    status = _cairo_ddraw_ogl_surface_create (surface);
    if (status) {
	IURelease (surface->lpddc);
	IURelease (surface->lpdds);
	free (surface);
	return _cairo_surface_create_in_error (status);
    }
#endif

    _cairo_region_init (&surface->clip_region);

    surface->lpdd = lpdd;

    surface->locked = FALSE;
#ifdef CAIRO_DDRAW_USE_GL
    surface->dirty = FALSE;
#endif
    surface->has_clip_region = FALSE;
    surface->has_image_clip = FALSE;
    surface->image_clip_invalid = FALSE;
    surface->clip_list_invalid = FALSE;

    surface->installed_clipper = NULL;
    surface->image = NULL;
    surface->root = surface;
    surface->data_offset = 0;

    surface->extents.x = 0;
    surface->extents.y = 0;
    surface->extents.width = ddsd.dwWidth;
    surface->extents.height = ddsd.dwHeight;

    surface->origin.x = 0;
    surface->origin.y = 0;
    
    _cairo_surface_init (&surface->base, &_cairo_ddraw_surface_backend,
			 _cairo_content_from_format (format));

    return &surface->base;
}

/**
 * cairo_ddraw_surface_create_alias:
 * @root_surface: pointer to a DirectDraw surface
 * @x: x-origin of sub-surface
 * @y: y-origin of sub-surface
 * @width: width of sub-surface, in pixels
 * @height: height of the sub-surface, in pixels
 *
 * Creates an alias to an existing DirectDraw surface.  The alias is a
 * sub-surface which occupies a portion of the main surface.
 *
 * Return value: the newly created surface
 **/
cairo_surface_t *
cairo_ddraw_surface_create_alias (cairo_surface_t *root_surface,
				  int x,
				  int y,
				  int width,
				  int height)
{
    cairo_ddraw_surface_t * root = (cairo_ddraw_surface_t *) root_surface;
    cairo_ddraw_surface_t * surface;
    HRESULT hr;

    if (root_surface->type != CAIRO_SURFACE_TYPE_DDRAW)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_SURFACE_TYPE_MISMATCH));

    if (root->root != root) {
	x += root->origin.x;
	y += root->origin.y;
	root = root->root;
    }

    surface = malloc (sizeof (cairo_ddraw_surface_t));
    if (surface == NULL)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    if (FAILED(hr = IDDCreateClipper (root->lpdd, 0,
				      &surface->lpddc, NULL))) {
	cairo_status_t status =
	    _cairo_ddraw_print_ddraw_error ("_surface_create", hr);
	free (surface);
	return _cairo_surface_create_in_error (status);
    }

    _cairo_region_init (&surface->clip_region);

    cairo_surface_reference (root_surface);

    surface->locked = FALSE;
#ifdef CAIRO_DDRAW_USE_GL
    surface->dirty = FALSE;
#endif
    surface->has_clip_region = FALSE;
    surface->has_image_clip = FALSE;
    surface->image_clip_invalid = FALSE;
    surface->clip_list_invalid = FALSE;

    surface->format = root->format;
    surface->lpdds = root->lpdds;
    surface->lpdd = root->lpdd;
    surface->lpdds = root->lpdds;
    surface->installed_clipper = NULL;
    surface->image = NULL;
    surface->root = root;
    surface->data_offset = 0;

#ifdef CAIRO_DDRAW_USE_GL
    surface->gl_id = root->gl_id;
#endif

    surface->origin.x = x;
    surface->origin.y = y;

    surface->extents.x = MAX (0, -x);
    surface->extents.y = MAX (0, -y);
    surface->extents.width =
	MIN (x + width, (int)root->extents.width) - MAX (x, 0);
    surface->extents.height =
	MIN (y + height, (int)root->extents.height) - MAX (y, 0);

    _cairo_surface_init (&surface->base, &_cairo_ddraw_surface_backend,
			 root_surface->content);

    return &surface->base;
}

/**
 * cairo_ddraw_surface_get_ddraw_surface:
 * @surface: pointer to a DirectDraw surface
 *
 * Gets a pointer to the DirectDraw Surface.
 *
 * Return value: the DirectDraw surface pointer
 **/
LPDIRECTDRAWSURFACE
cairo_ddraw_surface_get_ddraw_surface (cairo_surface_t *surface)
{
    cairo_ddraw_surface_t * ddraw_surf = (cairo_ddraw_surface_t *) surface;

    if (surface->type != CAIRO_SURFACE_TYPE_DDRAW)
	return NULL;

    _cairo_ddraw_surface_reset_clipper (ddraw_surf);

    START_TIMER(getddraw);
#ifdef CAIRO_DDRAW_USE_GL
    _cairo_ddraw_ogl_flush (ddraw_surf);
#endif

    _cairo_ddraw_surface_unlock (ddraw_surf);
    END_TIMER(getddraw);

    return ddraw_surf->lpdds;
}

/**
 * cairo_ddraw_surface_get_image_surface:
 * @surface: pointer to a DirectDraw surface
 *
 * Gets an image surface alias of @surface.
 *
 * Return value: the image surface.
 **/
cairo_surface_t *
cairo_ddraw_surface_get_image (cairo_surface_t *surface)
{
    cairo_ddraw_surface_t * ddraw_surf = (cairo_ddraw_surface_t *) surface;

    if (surface->type != CAIRO_SURFACE_TYPE_DDRAW)
	return NULL;

    _cairo_ddraw_surface_reset_clipper (ddraw_surf);

    START_TIMER(getimage);
#ifdef CAIRO_DDRAW_USE_GL
    _cairo_ddraw_ogl_flush (ddraw_surf);
#endif

    _cairo_ddraw_surface_lock (ddraw_surf);
    END_TIMER(getimage);

    return ddraw_surf->image;
}

#ifdef CAIRO_DDRAW_FILL_ACCELERATION

#ifndef CAIRO_DDRAW_USE_GL

/* This big function tells us how to optimize operators for the
 * case of solid destination and constant-alpha source
 *
 * Note: This function needs revisiting if we add support for
 *       super-luminescent colors (a == 0, r,g,b > 0)
 */
static enum { DO_CLEAR, DO_SOURCE, DO_NOTHING, DO_UNSUPPORTED }
categorize_solid_dest_operator (cairo_operator_t op,
				unsigned short   alpha)
{
    enum { SOURCE_TRANSPARENT, SOURCE_LIGHT, SOURCE_SOLID, SOURCE_OTHER } source;

    if (alpha >= 0xff00)
	source = SOURCE_SOLID;
    else if (alpha < 0x100)
	source = SOURCE_TRANSPARENT;
    else
	source = SOURCE_OTHER;

    switch (op) {
    case CAIRO_OPERATOR_CLEAR:    /* 0                 0 */
    case CAIRO_OPERATOR_OUT:      /* 1 - Ab            0 */
	return DO_CLEAR;
	break;

    case CAIRO_OPERATOR_SOURCE:   /* 1                 0 */
    case CAIRO_OPERATOR_IN:       /* Ab                0 */
	return DO_SOURCE;
	break;

    case CAIRO_OPERATOR_OVER:     /* 1            1 - Aa */
    case CAIRO_OPERATOR_ATOP:     /* Ab           1 - Aa */
	if (source == SOURCE_SOLID)
	    return DO_SOURCE;
	else if (source == SOURCE_TRANSPARENT)
	    return DO_NOTHING;
	else
	    return DO_UNSUPPORTED;
	break;

    case CAIRO_OPERATOR_DEST_OUT: /* 0            1 - Aa */
    case CAIRO_OPERATOR_XOR:      /* 1 - Ab       1 - Aa */
	if (source == SOURCE_SOLID)
	    return DO_CLEAR;
	else if (source == SOURCE_TRANSPARENT)
	    return DO_NOTHING;
	else
	    return DO_UNSUPPORTED;
    	break;

    case CAIRO_OPERATOR_DEST:     /* 0                 1 */
    case CAIRO_OPERATOR_DEST_OVER:/* 1 - Ab            1 */
    case CAIRO_OPERATOR_SATURATE: /* min(1,(1-Ab)/Aa)  1 */
	return DO_NOTHING;
	break;

    case CAIRO_OPERATOR_DEST_IN:  /* 0                Aa */
    case CAIRO_OPERATOR_DEST_ATOP:/* 1 - Ab           Aa */
	if (source == SOURCE_SOLID)
	    return DO_NOTHING;
	else if (source == SOURCE_TRANSPARENT)
	    return DO_CLEAR;
	else
	    return DO_UNSUPPORTED;
	break;

    case CAIRO_OPERATOR_ADD:	  /* 1                1 */
	if (source == SOURCE_TRANSPARENT)
	    return DO_NOTHING;
	else
	    return DO_UNSUPPORTED;
	break;
    }

    ASSERT_NOT_REACHED;
    return DO_UNSUPPORTED;
}

#endif /* ndef CAIRO_DDRAW_USE_GL */

static cairo_int_status_t
_cairo_ddraw_surface_fill_rectangles (void			*abstract_surface,
				      cairo_operator_t		op,
				      const cairo_color_t	*color,
				      cairo_rectangle_int_t	*rects,
				      int			num_rects)
{
#ifdef CAIRO_DDRAW_USE_GL
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_ddraw_surface_t *root = surface->root;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    GLfloat sr, sg, sb, sa;
    uint32_t color_data = 0;
    cairo_bool_t can_fold = FALSE;
    int byte_shift = surface->format == CAIRO_FORMAT_A8 ? 0 : 2;
    enum {
	FILL_BATCH_SIZE =
	CAIRO_DDRAW_OGL_SCRATCH_VBO_SIZE / sizeof (cairo_ddraw_ogl_line_t)
    };

    CAIRO_DDRAW_API_ENTRY_STATUS;

    if (surface->base.content == CAIRO_CONTENT_COLOR)
	op = _cairo_ddraw_ogl_op_downgrade[op];

    sr = color->red_short   * (1.0f / 65535.0f);
    sg = color->green_short * (1.0f / 65535.0f);
    sb = color->blue_short  * (1.0f / 65535.0f);
    sa = color->alpha_short * (1.0f / 65535.0f);

    /* convert to solid clears if we can (so we can use glClear) */
    if (op == CAIRO_OPERATOR_SOURCE ||
	op == CAIRO_OPERATOR_OVER && color->alpha_short >= 0xff00) {
	op = CAIRO_OPERATOR_SOURCE;
	if (surface->format == CAIRO_FORMAT_A8) {
	    can_fold = TRUE;
	    color_data = color->alpha_short >> 8;
	} else if (surface->format = CAIRO_FORMAT_RGB24) {
	    uint32_t red = color->red_short >> 8;
	    uint32_t green = color->green_short >> 8;
	    uint32_t blue = color->blue_short >> 8;
	    color_data = (red << 16) | (green << 8) | blue;
	    if (red == blue && green == blue)
		can_fold = TRUE;
	} else {
	    uint32_t alpha = color->alpha_short >> 8;
	    uint32_t red = color->red_short >> 8;
	    uint32_t green = color->green_short >> 8;
	    uint32_t blue = color->blue_short >> 8;
	    color_data = (alpha << 24) | (red << 16) | (green << 8) | blue;
	    if (alpha == blue && red == blue && green == blue)
		can_fold = TRUE;
	}
    } else if (op == CAIRO_OPERATOR_CLEAR ||
	       op == CAIRO_OPERATOR_DEST_IN && color->alpha_short < 0x0100 ||
	       op == CAIRO_OPERATOR_DEST_OUT && color->alpha_short >= 0xff00) {
	op = CAIRO_OPERATOR_SOURCE;
	sr = sg = sb = sa = 0.0f;
	can_fold = TRUE;
	color_data = 0;
    } else if (op == CAIRO_OPERATOR_DEST ||
	       op == CAIRO_OPERATOR_OVER && color->alpha_short < 0x0100 ||
	       op == CAIRO_OPERATOR_DEST_IN && color->alpha_short >= 0xff00 ||
	       op == CAIRO_OPERATOR_DEST_OUT && color->alpha_short < 0x0100)
	return CAIRO_STATUS_SUCCESS;
    else {
	/* slow path */
	cairo_ddraw_ogl_line_t * verts = NULL;
	cairo_ddraw_ogl_line_t * vp = NULL;
	int last_line_width = 0;
	int shader_id = MAKE_SHADER_ID (op,
					CAIRO_DDRAW_OGL_SRC_SOLID,
					CAIRO_DDRAW_OGL_MASK_NONE);
	cairo_ddraw_ogl_program_info_t * info =
	    _cairo_ddraw_ogl_load_program (shader_id);
	
	if (info == NULL)
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	
	if ((status = _cairo_ddraw_surface_bind_to_ogl (surface)))
	    return status;
	
	glUseProgram (info->prog_id);
	CHECK_OGL_ERROR ("glUseProgram");
	
	/* these are in half-pixels */
	if (_cairo_ddraw_ogl_dst_flipped)
	    glUniform4f (info->pos_xform_uniform,
			 1.0f / root->extents.width, -1.0f,
			 -1.0f / root->extents.height, 1.0f);
	else
	    glUniform4f (info->pos_xform_uniform,
			 1.0f / root->extents.width, -1.0f,
			 1.0f / root->extents.height, -1.0f);
	CHECK_OGL_ERROR ("glUniform4f pos xform");
	
	if (info->src_color_uniform >= 0) {
	    glUniform4f (info->src_color_uniform, sr, sg, sb, sa);
	    CHECK_OGL_ERROR ("glUniform4f src color");
	}
	
	glViewport (0, 0, root->extents.width, root->extents.height);
	CHECK_OGL_ERROR ("glViewport");
		    
	glDisable (GL_SCISSOR_TEST);
	CHECK_OGL_ERROR ("glDisable scissor");
			
	if (surface->has_clip_region) {
	    int num_clip_rects =
		cairo_region_num_rectangles (&surface->clip_region);
	    int dx = surface->origin.x;
	    int dy = surface->origin.y;
	    int offx = surface->extents.x;
	    int offy = surface->extents.y;
	    int surf_h = root->extents.height;
	    int i;
	    
	    for (i = 0; i < num_clip_rects; ++i) {
		int j;
		
		for (j = 0; j < num_rects; ++j) {
		    cairo_rectangle_int_t clip_rect;
		    cairo_ddraw_ogl_line_t line;
		    int line_width;
		    
		    cairo_region_get_rectangle (&surface->clip_region, i,
						&clip_rect);
		    
		    clip_rect.x += offx;
		    clip_rect.y += offy;
		    
		    if (!_cairo_rectangle_intersect (&clip_rect, &rects[j]))
			continue;
		    
		    clip_rect.x += dx;
		    clip_rect.y += dy;
		    
		    /* these are in half-pixels */
		    if (clip_rect.height <= clip_rect.width) {
			line_width = clip_rect.height;
			line.vtx[0].y = line.vtx[1].y =
			    (clip_rect.y << 1) + line_width;
			line.vtx[0].x = (clip_rect.x << 1) + 1;
			line.vtx[1].x = line.vtx[0].x +
			    (clip_rect.width << 1);
		    } else {
			line_width = clip_rect.width;
			line.vtx[0].x = line.vtx[1].x =
			    (clip_rect.x << 1) + line_width;
			line.vtx[0].y = (clip_rect.y << 1) + 1;
			line.vtx[1].y = line.vtx[0].y +
			    (clip_rect.height << 1);
		    }
		    
		    if (vp && (last_line_width != line_width ||
			       vp - verts >= FILL_BATCH_SIZE)) {
			_glUnmapBufferOES (GL_ARRAY_BUFFER);
			CHECK_OGL_ERROR ("glUnmapBufferOES vbo");
			
			glDrawArrays (GL_LINES, 0, (vp - verts) * 2);
			CHECK_OGL_ERROR ("glDrawArrays");
			
			vp = verts = NULL;
		    }
		    
		    if (vp == NULL) {
			GLuint vbo = _cairo_ddraw_ogl_get_scratch_buffer ();
			
			glBindBuffer (GL_ARRAY_BUFFER, vbo);
			CHECK_OGL_ERROR ("glBindBuffer vbo");
			
			glVertexAttribPointer (info->pos_attr, 2,
					       DST_COORDINATE_TYPE, GL_FALSE,
					       sizeof (cairo_ddraw_ogl_dst_coord_t),
					       (GLvoid *) 0);
			CHECK_OGL_ERROR ("glVertexAttribPointer pos");
			
			glEnableVertexAttribArray (info->pos_attr);
			CHECK_OGL_ERROR ("glEnableVertexArray pos");
			
			verts = _glMapBufferOES (GL_ARRAY_BUFFER,
						 GL_WRITE_ONLY_OES);
			if (verts == NULL)
			    return _cairo_ddraw_check_ogl_error ("_fill_rectangles");
			
			vp = verts;
			last_line_width = line_width;
			
			glLineWidth ((GLfloat) line_width);
			CHECK_OGL_ERROR ("glLineWidth");
		    }
		    
		    *vp++ = line;
		}
	    }
	} else {
	    int dx = surface->origin.x;
	    int dy = surface->origin.y;
	    int surf_h = root->extents.height;
	    int i;
	    
	    for (i = 0; i < num_rects; i++) {
		cairo_ddraw_ogl_line_t line;
		int x = rects[i].x + dx;
		int y = rects[i].y + dy;
		int width = rects[i].width;
		int height = rects[i].height;
		int line_width;
		
		/* these are in half-pixels */
		if (height <= width) {
		    line_width = height;
		    line.vtx[0].y = line.vtx[1].y = (y << 1) + line_width;
		    line.vtx[0].x = (x << 1) + 1;
		    line.vtx[1].x = line.vtx[0].x + (width << 1);
		} else {
		    line_width = width;
		    line.vtx[0].x = line.vtx[1].x = (x << 1) + line_width;
		    line.vtx[0].y = (y << 1) + 1;
		    line.vtx[1].y = line.vtx[0].y + (height << 1);
		}
		
		if (vp && (last_line_width != line_width ||
			   vp - verts >= FILL_BATCH_SIZE)) {
		    _glUnmapBufferOES (GL_ARRAY_BUFFER);
		    CHECK_OGL_ERROR ("glUnmapBufferOES vbo");
		    
		    glDrawArrays (GL_LINES, 0, (vp - verts) * 2);
		    CHECK_OGL_ERROR ("glDrawArrays");
		    
		    vp = verts = NULL;
		}
		
		if (vp == NULL) {
		    GLuint vbo = _cairo_ddraw_ogl_get_scratch_buffer ();
		    
		    glBindBuffer (GL_ARRAY_BUFFER, vbo);
		    CHECK_OGL_ERROR ("glBindBuffer vbo");
		    
		    glVertexAttribPointer (info->pos_attr, 2,
					   DST_COORDINATE_TYPE, GL_FALSE,
					   sizeof (cairo_ddraw_ogl_dst_coord_t),
					   (GLvoid *) 0);
		    CHECK_OGL_ERROR ("glVertexAttribPointer pos");
		    
		    glEnableVertexAttribArray (info->pos_attr);
		    CHECK_OGL_ERROR ("glEnableVertexArray pos");
		    
		    verts = _glMapBufferOES (GL_ARRAY_BUFFER,
					     GL_WRITE_ONLY_OES);
		    if (verts == NULL)
			return _cairo_ddraw_check_ogl_error ("_fill_rectangles");
		    
		    vp = verts;
		    last_line_width = line_width;
		    
		    glLineWidth ((GLfloat) line_width);
		    CHECK_OGL_ERROR ("glLineWidth");
		}
	    
		*vp++ = line;
	    }
	}

	if (vp) {
	    _glUnmapBufferOES (GL_ARRAY_BUFFER);
	    CHECK_OGL_ERROR ("glUnmapBufferOES vbo");
	    
	    glDrawArrays (GL_LINES, 0, (vp - verts) * 2);
	    CHECK_OGL_ERROR ("glDrawArrays");
	}
	
	return CAIRO_STATUS_SUCCESS;
    }

    /* fast path */

    if ((status = _cairo_ddraw_surface_bind_to_ogl (surface)))
	return status;
			
    glEnable (GL_SCISSOR_TEST);
    CHECK_OGL_ERROR ("glEnable scissor");
    
    glClearColor (sr, sg, sb, sa);
    CHECK_OGL_ERROR ("glClearColor");
    
    if (surface->has_clip_region) {
	int num_clip_rects =
	    cairo_region_num_rectangles (&surface->clip_region);
	int dx = surface->origin.x;
	int dy = surface->origin.y;
	int offx = surface->extents.x;
	int offy = surface->extents.y;
	int surf_h = root->extents.height;
	int i;
	
	for (i = 0; i < num_clip_rects; ++i) {
	    int j;
	    
	    for (j = 0; j < num_rects; ++j) {
		cairo_rectangle_int_t clip_rect;
		
		cairo_region_get_rectangle (&surface->clip_region, i,
					    &clip_rect);
		
		clip_rect.x += offx;
		clip_rect.y += offy;
		
		if (!_cairo_rectangle_intersect (&clip_rect, &rects[j]))
		    continue;
		
		clip_rect.x += dx;
		clip_rect.y += dy;
		
		if (clip_rect.width * clip_rect.height >= 
		    CAIRO_DDRAW_FILL_THRESHOLD) {
		    
		    if (_cairo_ddraw_ogl_dst_flipped)
			glScissor (clip_rect.x,
				   surf_h - (clip_rect.y + clip_rect.height),
				   clip_rect.width, clip_rect.height);
		    else
			glScissor (clip_rect.x, clip_rect.y,
				   clip_rect.width, clip_rect.height);
		    CHECK_OGL_ERROR ("glScissor");
		    
		    glClear (GL_COLOR_BUFFER_BIT);
		    CHECK_OGL_ERROR ("glClear");
		} else {
		    cairo_image_surface_t * image =
			(cairo_image_surface_t *) root->image;
		    unsigned char * dst;
		    int dststride;
		    int lines = clip_rect.height;
		    
		    START_TIMER(swfill);
		    _cairo_ddraw_ogl_flush (surface);
		    
		    if ((status = _cairo_ddraw_surface_lock (surface)))
			return status;
		    END_TIMER(swfill);
		    
		    dststride = image->stride;
		    dst = image->data + dststride * clip_rect.y +
			(clip_rect.x << byte_shift);
		    
		    if (can_fold) {
			int bytes = clip_rect.width << byte_shift;
			while (lines) {
			    memset (dst, color_data, bytes);
			    dst += dststride;
			    --lines;
			}
		    } else {
			while (lines) {
			    int words = clip_rect.width;
			    uint32_t * dp = (uint32_t *) dst;
			    
			    while (words >> 3) {
				dp[0] = color_data;
				dp[1] = color_data;
				dp[2] = color_data;
				dp[3] = color_data;
				dp[4] = color_data;
				dp[5] = color_data;
				dp[6] = color_data;
				dp[7] = color_data;
				dp += 8;
				words -= 8;
			    }
			    while (words) {
				*dp++ = color_data;
				--words;
			    }
			    dst += dststride;
			    --lines;
			}
		    }
		}
	    }
	}
    } else {
	int dx = surface->origin.x;
	int dy = surface->origin.y;
	int surf_h = root->extents.height;
	int i;
	
	for (i = 0; i < num_rects; i++) {
	    int x = rects[i].x + dx;
	    int y = rects[i].y + dy;
	    int width = rects[i].width;
	    int height = rects[i].height;
	    
	    if (width * height > CAIRO_DDRAW_FILL_THRESHOLD) {
		if (_cairo_ddraw_ogl_dst_flipped)
		    glScissor (x, surf_h - (y + height),
			       width, height);
		else
		    glScissor (x, y, width, height);
		CHECK_OGL_ERROR ("glScissor");
		
		glClear (GL_COLOR_BUFFER_BIT);
		CHECK_OGL_ERROR ("glClear");
	    } else {
		cairo_image_surface_t * image =
		    (cairo_image_surface_t *) root->image;
		unsigned char * dst;
		int dststride;
		int lines = height;
		
		START_TIMER(swfill);
		_cairo_ddraw_ogl_flush (surface);
		
		if ((status = _cairo_ddraw_surface_lock (surface)))
		    return status;
		END_TIMER(swfill);
		
		dststride = image->stride;
		dst = image->data + dststride * y + (x << byte_shift);
		
		if (can_fold) {
		    int bytes = width << byte_shift;
		    while (lines) {
			memset (dst, color_data, bytes);
			dst += dststride;
			--lines;
		    }
		} else {
		    while (lines) {
			int words = width;
			uint32_t * dp = (uint32_t *) dst;
			
			while (words >> 3) {
			    dp[0] = color_data;
			    dp[1] = color_data;
			    dp[2] = color_data;
			    dp[3] = color_data;
			    dp[4] = color_data;
			    dp[5] = color_data;
			    dp[6] = color_data;
			    dp[7] = color_data;
			    dp += 8;
			    words -= 8;
			}
			while (words) {
			    *dp++ = color_data;
			    --words;
			}
			dst += dststride;
			--lines;
		    }
		}
	    }
	}
    }

    return CAIRO_STATUS_SUCCESS;

#else /* CAIRO_DDRAW_USE_GL */

    DDBLTFX ddbltfx;
    COLORREF new_color;
    cairo_ddraw_surface_t *surface = abstract_surface;
    cairo_status_t status;
    int i;

    /* XXXperf If it's not RGB24, we need to do a little more checking
     * to figure out when we can use GDI.  We don't have that checking
     * anywhere at the moment, so just bail and use the fallback
     * paths. */
    if (surface->format != CAIRO_FORMAT_RGB24)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    switch (categorize_solid_dest_operator (op, color->alpha_short)) {
    case DO_CLEAR:
        new_color = 0x00000000u;
	break;
    case DO_SOURCE:
        new_color = 0xff000000u |
            ((color->red_short & 0xff00u) << 8) |
            (color->green_short & 0xff00u) |
            (color->blue_short >> 8);
	break;
    case DO_NOTHING:
	return CAIRO_STATUS_SUCCESS;
    case DO_UNSUPPORTED:
    default:
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (surface->root->locked) {
#ifdef CAIRO_DDRAW_FILL_THRESHOLD
	uint32_t sum = 0;

	/* check to see if we have enough work to do */
	for (i = 0; i < num_rects; ++i) {
	    sum += rects[i].width * rects[i].height;
	    if (sum >= CAIRO_DDRAW_FILL_THRESHOLD)
		break;
	}

	if (sum < CAIRO_DDRAW_FILL_THRESHOLD)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
#endif

	if ((status = _cairo_ddraw_surface_unlock (surface)))
	    return status;
    }

    if ((status = _cairo_ddraw_surface_set_clip_list (surface)))
	return status;

    if ((status = _cairo_ddraw_surface_set_clipper (surface)))
	return status;

    memset (&ddbltfx, 0, sizeof (ddbltfx));
    ddbltfx.dwSize = sizeof (ddbltfx);
    ddbltfx.dwFillColor = new_color;

    for (i = 0; i < num_rects; i++) {
	RECT rect;
	HRESULT hr;

	rect.left = rects[i].x + surface->origin.x;
	rect.top = rects[i].y + surface->origin.y;
	rect.right = rects[i].x + rects[i].width + surface->origin.x;
	rect.bottom = rects[i].y + rects[i].height + surface->origin.y;

	if (FAILED (hr = IDDSBlt (surface->lpdds, &rect,
				 NULL, NULL,
				 DDBLT_COLORFILL | DDBLT_WAITNOTBUSY,
				 &ddbltfx)))
	    return _cairo_ddraw_print_ddraw_error ("_fill_rectangles", hr);
    }

    return CAIRO_STATUS_SUCCESS;

#endif /* CAIRO_DDRAW_USE_GL */
}

#endif /* CAIRO_DDRAW_FILL_ACCELERATION */

static const cairo_surface_backend_t _cairo_ddraw_surface_backend = {
    CAIRO_SURFACE_TYPE_DDRAW,
#ifdef CAIRO_DDRAW_CREATE_SIMILAR
    _cairo_ddraw_surface_create_similar,
#else
    NULL, /* create_similar */
#endif
    _cairo_ddraw_surface_finish,
    _cairo_ddraw_surface_acquire_source_image,
    NULL, /* release_source_image */
    _cairo_ddraw_surface_acquire_dest_image,
    NULL, /* release_dest_image */
#ifdef CAIRO_DDRAW_CLONE_SIMILAR
    _cairo_ddraw_surface_clone_similar,
#else
    NULL, /* clone_similar */
#endif
#ifdef CAIRO_DDRAW_COMPOSITE_ACCELERATION
    _cairo_ddraw_surface_composite,
#else
    NULL, /* composite */
#endif
#ifdef CAIRO_DDRAW_FILL_ACCELERATION
    _cairo_ddraw_surface_fill_rectangles,
#else
    NULL, /* fill_rectangles */
#endif
    NULL, /* composite_trapezoids */
    NULL, /* create_span_renderer */
    NULL, /* check_span_renderer */
    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_ddraw_surface_set_clip_region,
    NULL, /* intersect_clip_path */
    _cairo_ddraw_surface_get_extents,
    NULL, /* old_show_glyphs */
    NULL, /* get_font_options */
    _cairo_ddraw_surface_flush,
    NULL, /* mark_dirty_rectangle */
#ifdef CAIRO_DDRAW_FONT_ACCELERATION
    _cairo_ddraw_surface_scaled_font_fini,
    _cairo_ddraw_surface_scaled_glyph_fini,
#else
    NULL, /* scaled_font_fini */
    NULL, /* scaled_glyph_fini */
#endif

    NULL, /* paint */
    NULL, /* mask */
    NULL, /* stroke */
    NULL, /* fill */
#ifdef CAIRO_DDRAW_FONT_ACCELERATION
    _cairo_ddraw_surface_show_glyphs,
#else
    NULL, /* show_glyphs */
#endif

    NULL,  /* snapshot */
    NULL, /* is_similar */

    _cairo_ddraw_surface_reset,
};

#endif /* CAIRO_HAS_DDRAW_SURFACE */

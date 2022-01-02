/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Chris Wilson
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
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
 */

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-intel-private.h"
#include "cairo-drm-intel-ioctl-private.h"

#include "cairo-error-private.h"
#include "cairo-freelist-private.h"
#include "cairo-pattern-private.h"
#include "cairo-image-surface-private.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <drm/i915_drm.h>

#define GLYPH_CACHE_WIDTH 1024
#define GLYPH_CACHE_HEIGHT 1024
#define GLYPH_CACHE_MIN_SIZE 1
#define GLYPH_CACHE_MAX_SIZE 128

#define IMAGE_CACHE_WIDTH 1024
#define IMAGE_CACHE_HEIGHT 1024

int
intel_get (int fd, int param)
{
    struct drm_i915_getparam gp;
    int value;

    gp.param = param;
    gp.value = &value;
    if (ioctl (fd, DRM_IOCTL_I915_GETPARAM, &gp) < 0)
	return 0;

    VG (VALGRIND_MAKE_MEM_DEFINED (&value, sizeof (value)));

    return value;
}

cairo_bool_t
intel_info (int fd, uint64_t *gtt_size)
{
    struct drm_i915_gem_get_aperture info;

    if (! intel_get (fd, I915_PARAM_HAS_GEM))
	return FALSE;

    if (! intel_get (fd, I915_PARAM_HAS_EXECBUF2))
	return FALSE;

    if (ioctl (fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &info) < 0)
	return FALSE;

    VG (VALGRIND_MAKE_MEM_DEFINED (&info, sizeof (info)));

    if (gtt_size != NULL)
	*gtt_size = info.aper_size;

    return TRUE;
}

void
intel_bo_write (const intel_device_t *device,
		intel_bo_t *bo,
		unsigned long offset,
		unsigned long size,
		const void *data)
{
    struct drm_i915_gem_pwrite pwrite;
    int ret;

    assert (bo->tiling == I915_TILING_NONE);
    assert (size);
    assert (offset < bo->base.size);
    assert (size+offset <= bo->base.size);

    intel_bo_set_tiling (device, bo);

    assert (bo->_tiling == I915_TILING_NONE);

    memset (&pwrite, 0, sizeof (pwrite));
    pwrite.handle = bo->base.handle;
    pwrite.offset = offset;
    pwrite.size = size;
    pwrite.data_ptr = (uint64_t) (uintptr_t) data;
    do {
	ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
    } while (ret == -1 && errno == EINTR);
    assert (ret == 0);

    bo->busy = FALSE;
}

void
intel_bo_read (const intel_device_t *device,
	       intel_bo_t *bo,
	       unsigned long offset,
	       unsigned long size,
	       void *data)
{
    struct drm_i915_gem_pread pread;
    int ret;

    assert (bo->tiling == I915_TILING_NONE);
    assert (size);
    assert (offset < bo->base.size);
    assert (size+offset <= bo->base.size);

    intel_bo_set_tiling (device, bo);

    assert (bo->_tiling == I915_TILING_NONE);

    memset (&pread, 0, sizeof (pread));
    pread.handle = bo->base.handle;
    pread.offset = offset;
    pread.size = size;
    pread.data_ptr = (uint64_t) (uintptr_t) data;
    do {
	ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_PREAD, &pread);
    } while (ret == -1 && errno == EINTR);
    assert (ret == 0);

    bo->cpu = TRUE;
    bo->busy = FALSE;
}

void *
intel_bo_map (const intel_device_t *device, intel_bo_t *bo)
{
    struct drm_i915_gem_set_domain set_domain;
    uint32_t domain;
    int ret;

    intel_bo_set_tiling (device, bo);

    if (bo->virtual != NULL)
	return bo->virtual;

    if (bo->cpu && bo->tiling == I915_TILING_NONE) {
	struct drm_i915_gem_mmap mmap_arg;

	mmap_arg.handle = bo->base.handle;
	mmap_arg.offset = 0;
	mmap_arg.size = bo->base.size;
	mmap_arg.addr_ptr = 0;

	do {
	    ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_MMAP, &mmap_arg);
	} while (ret == -1 && errno == EINTR);
	if (unlikely (ret != 0)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
	}

	bo->virtual = (void *) (uintptr_t) mmap_arg.addr_ptr;
	domain = I915_GEM_DOMAIN_CPU;
    } else {
	struct drm_i915_gem_mmap_gtt mmap_arg;
	void *ptr;

	/* Get the fake offset back... */
	mmap_arg.handle = bo->base.handle;
	do {
	    ret = ioctl (device->base.fd,
			 DRM_IOCTL_I915_GEM_MMAP_GTT, &mmap_arg);
	} while (ret == -1 && errno == EINTR);
	if (unlikely (ret != 0)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
	}

	/* and mmap it */
	ptr = mmap (0, bo->base.size, PROT_READ | PROT_WRITE,
		    MAP_SHARED, device->base.fd,
		    mmap_arg.offset);
	if (unlikely (ptr == MAP_FAILED)) {
	    _cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	    return NULL;
	}

	bo->virtual = ptr;
	domain = I915_GEM_DOMAIN_GTT;
    }

    VG (VALGRIND_MAKE_MEM_DEFINED (bo->virtual, bo->base.size));

    set_domain.handle = bo->base.handle;
    set_domain.read_domains = domain;
    set_domain.write_domain = domain;

    do {
	ret = ioctl (device->base.fd,
		     DRM_IOCTL_I915_GEM_SET_DOMAIN,
		     &set_domain);
    } while (ret == -1 && errno == EINTR);

    if (ret != 0) {
	intel_bo_unmap (bo);
	_cairo_error_throw (CAIRO_STATUS_DEVICE_ERROR);
	return NULL;
    }

    bo->busy = FALSE;
    return bo->virtual;
}

void
intel_bo_unmap (intel_bo_t *bo)
{
    munmap (bo->virtual, bo->base.size);
    bo->virtual = NULL;
}

cairo_bool_t
intel_bo_is_inactive (const intel_device_t *device, intel_bo_t *bo)
{
    struct drm_i915_gem_busy busy;

    if (! bo->busy)
	return TRUE;

    /* Is this buffer busy for our intended usage pattern? */
    busy.handle = bo->base.handle;
    busy.busy = 1;
    ioctl (device->base.fd, DRM_IOCTL_I915_GEM_BUSY, &busy);

    bo->busy = busy.busy;
    return ! busy.busy;
}

cairo_bool_t
intel_bo_wait (const intel_device_t *device, const intel_bo_t *bo)
{
    struct drm_i915_gem_set_domain set_domain;
    int ret;

    set_domain.handle = bo->base.handle;
    set_domain.read_domains = I915_GEM_DOMAIN_GTT;
    set_domain.write_domain = 0;

    do {
	ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
    } while (ret == -1 && errno == EINTR);

    return ret == 0;
}

static inline int
pot (int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

cairo_bool_t
intel_bo_madvise (intel_device_t *device,
		  intel_bo_t *bo,
		  int advice)
{
    struct drm_i915_gem_madvise madv;

    madv.handle = bo->base.handle;
    madv.madv = advice;
    madv.retained = TRUE;
    ioctl (device->base.fd, DRM_IOCTL_I915_GEM_MADVISE, &madv);
    return madv.retained;
}

static void
intel_bo_set_real_size (intel_device_t *device,
			intel_bo_t *bo,
			size_t size)
{
    struct drm_i915_gem_real_size arg;
    int ret;

    return;

    if (size == bo->base.size)
	return;

    arg.handle = bo->base.handle;
    arg.size = size;
    do {
	ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_REAL_SIZE, &arg);
    } while (ret == -1 && errno == EINTR);

    if (ret == 0) {
	if (size > bo->base.size) {
	    assert (bo->exec == NULL);
	    bo->cpu = TRUE;
	    bo->busy = FALSE;
	}

	bo->base.size = size;
    }
}

intel_bo_t *
intel_bo_create (intel_device_t *device,
	         uint32_t max_size,
	         uint32_t real_size,
	         cairo_bool_t gpu_target,
		 uint32_t tiling,
		 uint32_t stride)
{
    intel_bo_t *bo;
    uint32_t cache_size;
    struct drm_i915_gem_create create;
    int bucket;
    int ret;

    max_size = (max_size + 4095) & -4096;
    real_size = (real_size + 4095) & -4096;
    cache_size = pot (max_size);
    bucket = ffs (cache_size / 4096) - 1;
    if (bucket >= INTEL_BO_CACHE_BUCKETS)
	cache_size = max_size;

    if (gpu_target) {
	intel_bo_t *first = NULL;

	cairo_list_foreach_entry (bo, intel_bo_t,
				  &device->bo_in_flight,
				  cache_list)
	{
	    assert (bo->exec != NULL);
	    if (tiling && bo->_tiling &&
		(bo->_tiling != tiling || bo->_stride != stride))
	    {
		continue;
	    }

	    if (real_size <= bo->base.size) {
		if (real_size >= bo->base.size/2) {
		    cairo_list_del (&bo->cache_list);
		    bo = intel_bo_reference (bo);
		    goto DONE;
		}

		if (first == NULL)
		    first = bo;
	    }
	}

	if (first != NULL) {
	    cairo_list_del (&first->cache_list);
	    bo = intel_bo_reference (first);
	    goto DONE;
	}
    }

    /* no cached buffer available, allocate fresh */
    bo = _cairo_freepool_alloc (&device->bo_pool);
    if (unlikely (bo == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return bo;
    }

    cairo_list_init (&bo->cache_list);

    bo->base.name = 0;

    bo->offset = 0;
    bo->virtual = NULL;
    bo->cpu = TRUE;

    bo->_tiling = I915_TILING_NONE;
    bo->_stride = 0;
    bo->purgeable = 0;
    bo->busy = FALSE;

    bo->opaque0 = 0;
    bo->opaque1 = 0;

    bo->exec = NULL;
    bo->batch_read_domains = 0;
    bo->batch_write_domain = 0;
    cairo_list_init (&bo->link);

    create.size = cache_size;
    create.handle = 0;
    ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_CREATE, &create);
    if (unlikely (ret != 0)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	_cairo_freepool_free (&device->bo_pool, bo);
	return NULL;
    }

    bo->base.handle = create.handle;
    bo->full_size = bo->base.size = create.size;

    intel_bo_set_real_size (device, bo, real_size);
    CAIRO_REFERENCE_COUNT_INIT (&bo->base.ref_count, 1);
DONE:
    bo->tiling = tiling;
    bo->stride = stride;
    return bo;
}

intel_bo_t *
intel_bo_create_for_name (intel_device_t *device, uint32_t name)
{
    struct drm_i915_gem_get_tiling get_tiling;
    cairo_status_t status;
    intel_bo_t *bo;
    int ret;

    bo = _cairo_freepool_alloc (&device->bo_pool);
    if (unlikely (bo == NULL)) {
	_cairo_error_throw (CAIRO_STATUS_NO_MEMORY);
	return NULL;
    }

    status = _cairo_drm_bo_open_for_name (&device->base, &bo->base, name);
    if (unlikely (status))
	goto FAIL;

    CAIRO_REFERENCE_COUNT_INIT (&bo->base.ref_count, 1);
    cairo_list_init (&bo->cache_list);

    bo->full_size = bo->base.size;
    bo->offset = 0;
    bo->virtual = NULL;
    bo->purgeable = 0;
    bo->busy = TRUE;
    bo->cpu = FALSE;

    bo->opaque0 = 0;
    bo->opaque1 = 0;

    bo->exec = NULL;
    bo->batch_read_domains = 0;
    bo->batch_write_domain = 0;
    cairo_list_init (&bo->link);

    memset (&get_tiling, 0, sizeof (get_tiling));
    get_tiling.handle = bo->base.handle;

    ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_GET_TILING, &get_tiling);
    if (unlikely (ret != 0)) {
	_cairo_error_throw (CAIRO_STATUS_DEVICE_ERROR);
	_cairo_drm_bo_close (&device->base, &bo->base);
	goto FAIL;
    }

    bo->_tiling = bo->tiling = get_tiling.tiling_mode;
    // bo->stride = get_tiling.stride; /* XXX not available from get_tiling */

    return bo;

FAIL:
    _cairo_freepool_free (&device->bo_pool, bo);
    return NULL;
}

static void
intel_bo_release (void *_dev, void *_bo)
{
    intel_device_t *device = _dev;
    intel_bo_t *bo = _bo;

    if (bo->virtual != NULL)
	intel_bo_unmap (bo);

    assert (bo->exec == NULL);
    assert (cairo_list_is_empty (&bo->cache_list));

    _cairo_drm_bo_close (&device->base, &bo->base);
    _cairo_freepool_free (&device->bo_pool, bo);
}

void
intel_bo_set_tiling (const intel_device_t *device,
	             intel_bo_t *bo)
{
    struct drm_i915_gem_set_tiling set_tiling;
    int ret;

    if (bo->tiling == bo->_tiling &&
	(bo->tiling == I915_TILING_NONE || bo->stride == bo->_stride))
	return;

    do {
	set_tiling.handle = bo->base.handle;
	set_tiling.tiling_mode = bo->tiling;
	set_tiling.stride = bo->stride;

	ret = ioctl (device->base.fd, DRM_IOCTL_I915_GEM_SET_TILING, &set_tiling);
    } while (ret == -1 && errno == EINTR);

    assert (ret == 0);
    bo->_tiling = bo->tiling;
    bo->_stride = bo->stride;
}

static cairo_status_t
_intel_bo_put_a1_image (intel_device_t *device,
			intel_bo_t *bo,
			cairo_image_surface_t *src,
			int src_x, int src_y,
			int width, int height,
			int dst_x, int dst_y)
{
    uint8_t buf[CAIRO_STACK_BUFFER_SIZE];
    uint8_t *a8 = buf;
    uint8_t *data;
    int x;

    data = src->data + src_y * src->stride;

    if (bo->tiling == I915_TILING_NONE && width == bo->stride) {
	uint8_t *p;
	int size;

	size = bo->stride * height;
	if (size > (int) sizeof (buf)) {
	    a8 = _cairo_malloc_ab (bo->stride, height);
	    if (a8 == NULL)
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	p = a8;
	while (height--) {
	    for (x = 0; x < width; x++) {
		int i = src_x + x;
		int byte = i / 8;
		int bit = i % 8;
		p[x] = data[byte] & (1 << bit) ? 0xff : 0x00;
	    }

	    data += src->stride;
	    p += bo->stride;
	}

	intel_bo_write (device, bo,
			dst_y * bo->stride + dst_x, /* XXX  bo_offset */
			size, a8);
    } else {
	uint8_t *dst;

	if (width > (int) sizeof (buf)) {
	    a8 = _cairo_malloc (width);
	    if (a8 == NULL)
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	dst = intel_bo_map (device, bo);
	if (dst == NULL) {
	    if (a8 != buf)
		free (a8);
	    return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);
	}

	dst += dst_y * bo->stride + dst_x; /* XXX  bo_offset */
	while (height--) {
	    for (x = 0; x < width; x++) {
		int i = src_x + x;
		int byte = i / 8;
		int bit = i % 8;
		a8[x] = data[byte] & (1 << bit) ? 0xff : 0x00;
	    }

	    memcpy (dst, a8, width);
	    dst  += bo->stride;
	    data += src->stride;
	}
    }

    if (a8 != buf)
	free (a8);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
intel_bo_put_image (intel_device_t *device,
		    intel_bo_t *bo,
		    cairo_image_surface_t *src,
		    int src_x, int src_y,
		    int width, int height,
		    int dst_x, int dst_y)
{
    uint8_t *data;
    int size;
    int offset;

    intel_bo_set_tiling (device, bo);

    offset = dst_y * bo->stride;
    data = src->data + src_y * src->stride;
    switch (src->format) {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
	offset += 4 * dst_x;
	data   += 4 * src_x;
	size    = 4 * width;
	break;
    case CAIRO_FORMAT_RGB16_565:
	offset += 2 * dst_x;
	data   += 2 * src_x;
	size    = 2 * width;
	break;
    case CAIRO_FORMAT_A8:
	offset += dst_x;
	data   += src_x;
	size    = width;
	break;
    case CAIRO_FORMAT_A1:
	return _intel_bo_put_a1_image (device, bo, src,
				       src_x, src_y,
				       width, height,
				       dst_x, dst_y);
    default:
    case CAIRO_FORMAT_INVALID:
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
    }

    if (bo->tiling == I915_TILING_NONE && src->stride == bo->stride) {
	intel_bo_write (device, bo, offset, bo->stride * height, data);
    } else {
	uint8_t *dst;

	dst = intel_bo_map (device, bo);
	if (unlikely (dst == NULL))
	    return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);

	dst += offset;
	while (height--) {
	    memcpy (dst, data, size);
	    dst  += bo->stride;
	    data += src->stride;
	}
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_bool_t
_intel_snapshot_cache_entry_can_remove (const void *closure)
{
    return TRUE;
}

static void
_intel_snapshot_cache_entry_destroy (void *closure)
{
    intel_surface_t *surface = cairo_container_of (closure,
						   intel_surface_t,
						   snapshot_cache_entry);

    surface->snapshot_cache_entry.hash = 0;
}

cairo_status_t
intel_device_init (intel_device_t *device, int fd)
{
    struct drm_i915_gem_get_aperture aperture;
    cairo_status_t status;
    size_t size;
    int ret;
    int n;

    ret = ioctl (fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);
    if (ret != 0)
	return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);

    CAIRO_MUTEX_INIT (device->mutex);

    device->gtt_max_size = aperture.aper_size;
    device->gtt_avail_size = aperture.aper_available_size;
    device->gtt_avail_size -= device->gtt_avail_size >> 5;

    size = aperture.aper_size / 8;
    device->snapshot_cache_max_size = size / 4;
    status = _cairo_cache_init (&device->snapshot_cache,
			        NULL,
				_intel_snapshot_cache_entry_can_remove,
				_intel_snapshot_cache_entry_destroy,
				size);
    if (unlikely (status))
	return status;

    for (n = 0; n < ARRAY_LENGTH (device->glyph_cache); n++) {
	device->glyph_cache[n].buffer.bo = NULL;
	cairo_list_init (&device->glyph_cache[n].rtree.pinned);
    }
    cairo_list_init (&device->fonts);

    device->gradient_cache.size = 0;

    device->base.bo.release = intel_bo_release;

    return CAIRO_STATUS_SUCCESS;
}

static void
_intel_gradient_cache_fini (intel_device_t *device)
{
    unsigned int n;

    for (n = 0; n < device->gradient_cache.size; n++) {
	_cairo_pattern_fini (&device->gradient_cache.cache[n].pattern.base);
	if (device->gradient_cache.cache[n].buffer.bo != NULL)
	    cairo_drm_bo_destroy (&device->base.base,
				  &device->gradient_cache.cache[n].buffer.bo->base);
    }
}

static void
_intel_glyph_cache_fini (intel_device_t *device, intel_buffer_cache_t *cache)
{
    if (cache->buffer.bo == NULL)
	return;

    intel_bo_destroy (device, cache->buffer.bo);
    _cairo_rtree_fini (&cache->rtree);
}

void
intel_device_fini (intel_device_t *device)
{
    cairo_scaled_font_t *scaled_font, *next_scaled_font;
    int n;

    cairo_list_foreach_entry_safe (scaled_font,
				   next_scaled_font,
				   cairo_scaled_font_t,
				   &device->fonts,
				   link)
    {
	_cairo_scaled_font_revoke_ownership (scaled_font);
    }

    for (n = 0; n < ARRAY_LENGTH (device->glyph_cache); n++)
	_intel_glyph_cache_fini (device, &device->glyph_cache[n]);

    _cairo_cache_fini (&device->snapshot_cache);

    _intel_gradient_cache_fini (device);
    _cairo_freepool_fini (&device->bo_pool);

    _cairo_drm_device_fini (&device->base);
}

void
intel_throttle (intel_device_t *device)
{
    ioctl (device->base.fd, DRM_IOCTL_I915_GEM_THROTTLE);
}

void
intel_glyph_cache_unpin (intel_device_t *device)
{
    int n;

    for (n = 0; n < ARRAY_LENGTH (device->glyph_cache); n++)
	_cairo_rtree_unpin (&device->glyph_cache[n].rtree);
}

static cairo_status_t
intel_glyph_cache_add_glyph (intel_device_t *device,
	                     intel_buffer_cache_t *cache,
			     cairo_scaled_glyph_t  *scaled_glyph)
{
    cairo_image_surface_t *glyph_surface = scaled_glyph->surface;
    intel_glyph_t *glyph;
    cairo_rtree_node_t *node = NULL;
    double sf_x, sf_y;
    cairo_status_t status;
    uint8_t *dst, *src;
    int width, height;

    width = glyph_surface->width;
    if (width < GLYPH_CACHE_MIN_SIZE)
	width = GLYPH_CACHE_MIN_SIZE;
    height = glyph_surface->height;
    if (height < GLYPH_CACHE_MIN_SIZE)
	height = GLYPH_CACHE_MIN_SIZE;

    /* search for an available slot */
    status = _cairo_rtree_insert (&cache->rtree, width, height, &node);
    /* search for an unpinned slot */
    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
	status = _cairo_rtree_evict_random (&cache->rtree, width, height, &node);
	if (status == CAIRO_STATUS_SUCCESS)
	    status = _cairo_rtree_node_insert (&cache->rtree, node, width, height, &node);
    }
    if (unlikely (status))
	return status;

    /* XXX streaming upload? */

    height = glyph_surface->height;
    src = glyph_surface->data;
    dst = cache->buffer.bo->virtual;
    if (dst == NULL) {
	dst = intel_bo_map (device, cache->buffer.bo);
	if (unlikely (dst == NULL))
	    return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);
    }

    dst += node->y * cache->buffer.stride;
    switch (glyph_surface->format) {
    case CAIRO_FORMAT_A1: {
	uint8_t buf[CAIRO_STACK_BUFFER_SIZE];
	uint8_t *a8 = buf;
	int x;

	if (width > (int) sizeof (buf)) {
	    a8 = _cairo_malloc (width);
	    if (unlikely (a8 == NULL))
		return _cairo_error (CAIRO_STATUS_NO_MEMORY);
	}

	dst += node->x;
	width = glyph_surface->width;
	while (height--) {
	    for (x = 0; x < width; x++)
		a8[x] = src[x>>3] & (1 << (x&7)) ? 0xff : 0x00;

	    memcpy (dst, a8, width);
	    dst += cache->buffer.stride;
	    src += glyph_surface->stride;
	}

	if (a8 != buf)
	    free (a8);
	break;
    }

    case CAIRO_FORMAT_A8:
	dst  += node->x;
	width = glyph_surface->width;
	while (height--) {
	    memcpy (dst, src, width);
	    dst += cache->buffer.stride;
	    src += glyph_surface->stride;
	}
	break;

    case CAIRO_FORMAT_ARGB32:
	dst  += 4*node->x;
	width = 4*glyph_surface->width;
	while (height--) {
	    memcpy (dst, src, width);
	    dst += cache->buffer.stride;
	    src += glyph_surface->stride;
	}
	break;
    default:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_INVALID:
	ASSERT_NOT_REACHED;
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
    }

    scaled_glyph->surface_private = node;

    glyph= (intel_glyph_t *) node;
    glyph->node.owner = &scaled_glyph->surface_private;
    glyph->cache = cache;

    /* compute tex coords: bottom-right, bottom-left, top-left */
    sf_x = 1. / cache->buffer.width;
    sf_y = 1. / cache->buffer.height;
    glyph->texcoord[0] =
	texcoord_2d_16 (sf_x * (node->x + glyph_surface->width),
		        sf_y * (node->y + glyph_surface->height));
    glyph->texcoord[1] =
	texcoord_2d_16 (sf_x * node->x,
		        sf_y * (node->y + glyph_surface->height));
    glyph->texcoord[2] =
	texcoord_2d_16 (sf_x * node->x,
	                sf_y * node->y);

    glyph->width  = glyph_surface->width;
    glyph->height = glyph_surface->height;

    return CAIRO_STATUS_SUCCESS;
}

void
intel_scaled_glyph_fini (cairo_scaled_glyph_t *scaled_glyph,
			 cairo_scaled_font_t  *scaled_font)
{
    intel_glyph_t *glyph;

    glyph = scaled_glyph->surface_private;
    if (glyph != NULL) {
	/* XXX thread-safety? Probably ok due to the frozen scaled-font. */
	glyph->node.owner = NULL;
	if (! glyph->node.pinned)
	    _cairo_rtree_node_remove (&glyph->cache->rtree, &glyph->node);
    }
}

void
intel_scaled_font_fini (cairo_scaled_font_t *scaled_font)
{
    cairo_list_del (&scaled_font->link);
}

static cairo_status_t
intel_get_glyph_cache (intel_device_t *device,
		       cairo_format_t format,
		       intel_buffer_cache_t **out)
{
    intel_buffer_cache_t *cache;
    cairo_status_t status;

    switch (format) {
    case CAIRO_FORMAT_ARGB32:
	cache = &device->glyph_cache[0];
	format = CAIRO_FORMAT_ARGB32;
	break;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	cache = &device->glyph_cache[1];
	format = CAIRO_FORMAT_A8;
	break;
    default:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_INVALID:
	ASSERT_NOT_REACHED;
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
    }

    if (unlikely (cache->buffer.bo == NULL)) {
	status = intel_buffer_cache_init (cache, device, format,
					 INTEL_GLYPH_CACHE_WIDTH,
					 INTEL_GLYPH_CACHE_HEIGHT);
	if (unlikely (status))
	    return status;

	_cairo_rtree_init (&cache->rtree,
			   INTEL_GLYPH_CACHE_WIDTH,
			   INTEL_GLYPH_CACHE_HEIGHT,
			   0, sizeof (intel_glyph_t));
    }

    *out = cache;
    return CAIRO_STATUS_SUCCESS;
}

cairo_int_status_t
intel_get_glyph (intel_device_t *device,
		 cairo_scaled_font_t *scaled_font,
		 cairo_scaled_glyph_t *scaled_glyph)
{
    cairo_bool_t own_surface = FALSE;
    intel_buffer_cache_t *cache;
    cairo_status_t status;

    if (scaled_glyph->surface == NULL) {
	status =
	    scaled_font->backend->scaled_glyph_init (scaled_font,
						     scaled_glyph,
						     CAIRO_SCALED_GLYPH_INFO_SURFACE);
	if (unlikely (status))
	    return status;

	if (unlikely (scaled_glyph->surface == NULL))
	    return CAIRO_INT_STATUS_UNSUPPORTED;

	own_surface = TRUE;
    }

    if (unlikely (scaled_glyph->surface->width == 0 ||
		  scaled_glyph->surface->height == 0))
    {
	return CAIRO_INT_STATUS_NOTHING_TO_DO;
    }

    if (unlikely (scaled_glyph->surface->width  > GLYPH_CACHE_MAX_SIZE ||
		  scaled_glyph->surface->height > GLYPH_CACHE_MAX_SIZE))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = intel_get_glyph_cache (device,
				    scaled_glyph->surface->format,
				    &cache);
    if (unlikely (status))
	return status;

    status = intel_glyph_cache_add_glyph (device, cache, scaled_glyph);
    if (unlikely (_cairo_status_is_error (status)))
	return status;

    if (unlikely (status == CAIRO_INT_STATUS_UNSUPPORTED)) {
	/* no room, replace entire cache */

	assert (cache->buffer.bo->exec != NULL);

	_cairo_rtree_reset (&cache->rtree);
	intel_bo_destroy (device, cache->buffer.bo);
	cache->buffer.bo = NULL;

	status = intel_buffer_cache_init (cache, device,
					  scaled_glyph->surface->format,
					  GLYPH_CACHE_WIDTH,
					  GLYPH_CACHE_HEIGHT);
	if (unlikely (status))
	    return status;

	status = intel_glyph_cache_add_glyph (device, cache, scaled_glyph);
	if (unlikely (status))
	    return status;
    }

    if (own_surface) {
	/* and release the copy of the image from system memory */
	cairo_surface_destroy (&scaled_glyph->surface->base);
	scaled_glyph->surface = NULL;
    }

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
intel_buffer_cache_init (intel_buffer_cache_t *cache,
		        intel_device_t *device,
			cairo_format_t format,
			int width, int height)
{
    const uint32_t tiling = I915_TILING_Y;
    uint32_t stride, size;

    assert ((width & 3) == 0);
    assert ((height & 1) == 0);
    cache->buffer.format = format;
    cache->buffer.width = width;
    cache->buffer.height = height;

    switch (format) {
    default:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_INVALID:
	ASSERT_NOT_REACHED;
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
    case CAIRO_FORMAT_ARGB32:
	cache->buffer.map0 = MAPSURF_32BIT | MT_32BIT_ARGB8888;
	stride = width * 4;
	break;
    case CAIRO_FORMAT_A8:
	cache->buffer.map0 = MAPSURF_8BIT | MT_8BIT_I8;
	stride = width;
	break;
    }

    size = height * stride;
    cache->buffer.bo = intel_bo_create (device,
					size, size,
					FALSE, tiling, stride);
    if (unlikely (cache->buffer.bo == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    cache->buffer.stride = stride;

    cache->buffer.offset = 0;
    cache->buffer.map0 |= MS3_tiling (tiling);
    cache->buffer.map0 |= ((height - 1) << MS3_HEIGHT_SHIFT) |
	                  ((width - 1)  << MS3_WIDTH_SHIFT);
    cache->buffer.map1 = ((stride / 4) - 1) << MS4_PITCH_SHIFT;

    cache->ref_count = 0;
    cairo_list_init (&cache->link);

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
intel_snapshot_cache_insert (intel_device_t *device,
			     intel_surface_t *surface)
{
    cairo_status_t status;

    surface->snapshot_cache_entry.size = surface->drm.bo->size;
    if (surface->snapshot_cache_entry.size >
	device->snapshot_cache_max_size)
    {
	return CAIRO_STATUS_SUCCESS;
    }

    if (device->snapshot_cache.freeze_count == 0)
	_cairo_cache_freeze (&device->snapshot_cache);

    surface->snapshot_cache_entry.hash = (unsigned long) surface;
    status = _cairo_cache_insert (&device->snapshot_cache,
	                          &surface->snapshot_cache_entry);
    if (unlikely (status)) {
	surface->snapshot_cache_entry.hash = 0;
	return status;
    }

    return CAIRO_STATUS_SUCCESS;
}

void
intel_surface_detach_snapshot (cairo_surface_t *abstract_surface)
{
    intel_surface_t *surface = (intel_surface_t *) abstract_surface;

    if (surface->snapshot_cache_entry.hash) {
	intel_device_t *device;

	device = (intel_device_t *) surface->drm.base.device;
	_cairo_cache_remove (&device->snapshot_cache,
		             &surface->snapshot_cache_entry);
	assert (surface->snapshot_cache_entry.hash == 0);
    }
}

void
intel_snapshot_cache_thaw (intel_device_t *device)
{
    if (device->snapshot_cache.freeze_count)
	_cairo_cache_thaw (&device->snapshot_cache);
}

static cairo_bool_t
_gradient_color_stops_equal (const cairo_gradient_pattern_t *a,
			     const cairo_gradient_pattern_t *b)
{
    unsigned int n;

    if (a->n_stops != b->n_stops)
	return FALSE;

    for (n = 0; n < a->n_stops; n++) {
	if (_cairo_fixed_from_double (a->stops[n].offset) !=
	    _cairo_fixed_from_double (b->stops[n].offset))
	{
	    return FALSE;
	}

	if (! _cairo_color_stop_equal (&a->stops[n].color, &b->stops[n].color))
	    return FALSE;
    }

    return TRUE;
}

static uint32_t
hars_petruska_f54_1_random (void)
{
#define rol(x,k) ((x << k) | (x >> (32-k)))
    static uint32_t x;
    return x = (x ^ rol (x, 5) ^ rol (x, 24)) + 0x37798849;
#undef rol
}

static int
intel_gradient_sample_width (const cairo_gradient_pattern_t *gradient)
{
    unsigned int n;
    int width;

    width = 8;
    for (n = 1; n < gradient->n_stops; n++) {
	double dx = gradient->stops[n].offset - gradient->stops[n-1].offset;
	double delta, max;
	int ramp;

	if (dx == 0)
	    continue;

	max = gradient->stops[n].color.red -
	      gradient->stops[n-1].color.red;

	delta = gradient->stops[n].color.green -
	        gradient->stops[n-1].color.green;
	if (delta > max)
	    max = delta;

	delta = gradient->stops[n].color.blue -
	        gradient->stops[n-1].color.blue;
	if (delta > max)
	    max = delta;

	delta = gradient->stops[n].color.alpha -
	        gradient->stops[n-1].color.alpha;
	if (delta > max)
	    max = delta;

	ramp = 128 * max / dx;
	if (ramp > width)
	    width = ramp;
    }

    width = (width + 7) & -8;
    return MIN (width, 1024);
}

cairo_status_t
intel_gradient_render (intel_device_t *device,
		       const cairo_gradient_pattern_t *pattern,
		       intel_buffer_t *buffer)
{
    pixman_image_t *gradient, *image;
    pixman_gradient_stop_t pixman_stops_stack[32];
    pixman_gradient_stop_t *pixman_stops;
    pixman_point_fixed_t p1, p2;
    int width;
    unsigned int i;
    cairo_status_t status;

    for (i = 0; i < device->gradient_cache.size; i++) {
	if (_gradient_color_stops_equal (pattern,
					 &device->gradient_cache.cache[i].pattern.gradient.base)) {
	    *buffer = device->gradient_cache.cache[i].buffer;
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    pixman_stops = pixman_stops_stack;
    if (unlikely (pattern->n_stops > ARRAY_LENGTH (pixman_stops_stack))) {
	pixman_stops = _cairo_malloc_ab (pattern->n_stops,
					 sizeof (pixman_gradient_stop_t));
	if (unlikely (pixman_stops == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    for (i = 0; i < pattern->n_stops; i++) {
	pixman_stops[i].x = _cairo_fixed_16_16_from_double (pattern->stops[i].offset);
	pixman_stops[i].color.red   = pattern->stops[i].color.red_short;
	pixman_stops[i].color.green = pattern->stops[i].color.green_short;
	pixman_stops[i].color.blue  = pattern->stops[i].color.blue_short;
	pixman_stops[i].color.alpha = pattern->stops[i].color.alpha_short;
    }

    width = intel_gradient_sample_width (pattern);

    p1.x = 0;
    p1.y = 0;
    p2.x = width << 16;
    p2.y = 0;

    gradient = pixman_image_create_linear_gradient (&p1, &p2,
						    pixman_stops,
						    pattern->n_stops);
    if (pixman_stops != pixman_stops_stack)
	free (pixman_stops);

    if (unlikely (gradient == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    pixman_image_set_filter (gradient, PIXMAN_FILTER_BILINEAR, NULL, 0);
    pixman_image_set_repeat (gradient, PIXMAN_REPEAT_PAD);

    image = pixman_image_create_bits (PIXMAN_a8r8g8b8, width, 1, NULL, 0);
    if (unlikely (image == NULL)) {
	pixman_image_unref (gradient);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    pixman_image_composite32 (PIXMAN_OP_SRC,
                              gradient, NULL, image,
                              0, 0,
                              0, 0,
                              0, 0,
                              width, 1);

    pixman_image_unref (gradient);

    buffer->bo = intel_bo_create (device,
				  4*width, 4*width,
				  FALSE, I915_TILING_NONE, 4*width);
    if (unlikely (buffer->bo == NULL)) {
	pixman_image_unref (image);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    intel_bo_write (device, buffer->bo, 0, 4*width, pixman_image_get_data (image));
    pixman_image_unref (image);

    buffer->offset = 0;
    buffer->width  = width;
    buffer->height = 1;
    buffer->stride = 4*width;
    buffer->format = CAIRO_FORMAT_ARGB32;
    buffer->map0 = MAPSURF_32BIT | MT_32BIT_ARGB8888;
    buffer->map0 |= ((width - 1) << MS3_WIDTH_SHIFT);
    buffer->map1 = (width - 1) << MS4_PITCH_SHIFT;

    if (device->gradient_cache.size < GRADIENT_CACHE_SIZE) {
	i = device->gradient_cache.size++;
    } else {
	i = hars_petruska_f54_1_random () % GRADIENT_CACHE_SIZE;
	_cairo_pattern_fini (&device->gradient_cache.cache[i].pattern.base);
	intel_bo_destroy (device, device->gradient_cache.cache[i].buffer.bo);
    }

    status = _cairo_pattern_init_copy (&device->gradient_cache.cache[i].pattern.base,
				       &pattern->base);
    if (unlikely (status)) {
	intel_bo_destroy (device, buffer->bo);
	/* Ensure the cache is correctly initialised for i965_device_destroy */
	_cairo_pattern_init_solid (&device->gradient_cache.cache[i].pattern.solid,
		                   CAIRO_COLOR_TRANSPARENT);
	return status;
    }

    device->gradient_cache.cache[i].buffer = *buffer;
    return CAIRO_STATUS_SUCCESS;
}

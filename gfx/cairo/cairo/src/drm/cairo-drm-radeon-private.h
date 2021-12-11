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

#ifndef CAIRO_DRM_RADEON_PRIVATE_H
#define CAIRO_DRM_RADEON_PRIVATE_H

#include "cairo-compiler-private.h"
#include "cairo-types-private.h"
#include "cairo-drm-private.h"
#include "cairo-freelist-private.h"

typedef struct _radeon_bo {
    cairo_drm_bo_t base;

    void *virtual;

    cairo_bool_t in_batch;
    uint32_t read_domains;
    uint32_t write_domain;
} radeon_bo_t;

typedef struct _radeon_device {
    cairo_drm_device_t base;
    cairo_freepool_t bo_pool;

    uint64_t vram_limit;
    uint64_t gart_limit;
} radeon_device_t;

cairo_private cairo_status_t
radeon_device_init (radeon_device_t *device, int fd);

cairo_private void
radeon_device_fini (radeon_device_t *device);

cairo_private cairo_bool_t
radeon_info (int fd,
             uint64_t *gart_size,
	     uint64_t *vram_size);

cairo_private void
radeon_bo_write (const radeon_device_t *dev,
	         radeon_bo_t *bo,
		 unsigned long offset,
		 unsigned long size,
		 const void *data);

cairo_private void
radeon_bo_read (const radeon_device_t *dev,
	        radeon_bo_t *bo,
	        unsigned long offset,
		unsigned long size,
		void *data);

cairo_private void
radeon_bo_wait (const radeon_device_t *dev, radeon_bo_t *bo);

cairo_private void *
radeon_bo_map (const radeon_device_t *dev, radeon_bo_t *bo);

cairo_private void
radeon_bo_unmap (radeon_bo_t *bo);

cairo_private cairo_drm_bo_t *
radeon_bo_create (radeon_device_t *dev,
		  uint32_t size,
		  uint32_t initial_domain);

cairo_private cairo_drm_bo_t *
radeon_bo_create_for_name (radeon_device_t *dev, uint32_t name);

cairo_private cairo_surface_t *
radeon_bo_get_image (const radeon_device_t *device,
	             radeon_bo_t *bo,
		     const cairo_drm_surface_t *surface);

#endif /* CAIRO_DRM_RADEON_PRIVATE_H */

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
#include "cairo-error-private.h"

#include <sys/ioctl.h>
#include <errno.h>
#include <libdrm/drm.h>

#define ERR_DEBUG(x) x

cairo_status_t
_cairo_drm_bo_open_for_name (const cairo_drm_device_t *dev,
			     cairo_drm_bo_t *bo,
			     uint32_t name)
{
    struct drm_gem_open open;
    int ret;

    open.name = name;
    open.handle = 0;
    open.size = 0;
    do {
	ret = ioctl (dev->fd, DRM_IOCTL_GEM_OPEN, &open);
    } while (ret == -1 && errno == EINTR);
    if (ret == -1) {
	ERR_DEBUG((fprintf (stderr, "Failed to open bo for name %d: %s\n",
			    name, strerror (errno))));
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    bo->name = name;
    bo->size = open.size;
    bo->handle = open.handle;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_drm_bo_flink (const cairo_drm_device_t *dev,
		     cairo_drm_bo_t *bo)
{
    struct drm_gem_flink flink;
    int ret;

    memset (&flink, 0, sizeof (flink));
    flink.handle = bo->handle;
    ret = ioctl (dev->fd, DRM_IOCTL_GEM_FLINK, &flink);
    if (ret == -1) {
	ERR_DEBUG((fprintf (stderr, "Failed to flink bo: %s\n",
			    strerror (errno))));
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    bo->name = flink.name;

    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_drm_bo_close (const cairo_drm_device_t *dev,
		     cairo_drm_bo_t *bo)
{
    struct drm_gem_close close;
    int ret;

    close.handle = bo->handle;
    do {
	ret = ioctl (dev->fd, DRM_IOCTL_GEM_CLOSE, &close);
    } while (ret == -1 && errno == EINTR);
}

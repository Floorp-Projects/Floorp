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

#ifndef CAIRO_DRM_INTEL_IOCTL_PRIVATE_H
#define CAIRO_DRM_INTEL_IOCTL_PRIVATE_H

#include "cairo-drm-intel-command-private.h"

#include <drm/i915_drm.h>

struct drm_i915_gem_real_size {
	uint32_t handle;
	uint64_t size;
};

#define DRM_I915_GEM_REAL_SIZE	0x2a
#define DRM_IOCTL_I915_GEM_REAL_SIZE	DRM_IOWR (DRM_COMMAND_BASE + DRM_I915_GEM_REAL_SIZE, struct drm_i915_gem_real_size)

#endif /* CAIRO_DRM_INTEL_IOCTL_PRIVATE_H */

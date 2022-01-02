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
 * **************************************************************************
 * This work was initially based upon xf86-video-intel/src/i915_render.c:
 * Copyright © 2006 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Wang Zhenyu <zhenyu.z.wang@intel.com>
 *    Eric Anholt <eric@anholt.net>
 *
 * **************************************************************************
 * and also upon libdrm/intel/intel_bufmgr_gem.c:
 * Copyright © 2007 Red Hat Inc.
 * Copyright © 2007 Intel Corporation
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 *          Keith Whitwell <keithw-at-tungstengraphics-dot-com>
 *          Eric Anholt <eric@anholt.net>
 *          Dave Airlie <airlied@linux.ie>
 */

/* XXX
 *
 * - Per thread context? Would it actually avoid many locks?
 *
 */

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-intel-private.h"
#include "cairo-drm-intel-command-private.h"
#include "cairo-drm-intel-ioctl-private.h"
#include "cairo-drm-i915-private.h"

#include "cairo-boxes-private.h"
#include "cairo-cache-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-freelist-private.h"
#include "cairo-list-private.h"
#include "cairo-path-fixed-private.h"
#include "cairo-region-private.h"
#include "cairo-surface-offset-private.h"
#include "cairo-image-surface-private.h"

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

static const uint32_t i915_batch_setup[] = {
    /* Disable line anti-aliasing */
    _3DSTATE_AA_CMD,

    /* Disable independent alpha blend */
    _3DSTATE_INDEPENDENT_ALPHA_BLEND_CMD |
	IAB_MODIFY_ENABLE |
	IAB_MODIFY_FUNC | (BLENDFUNC_ADD << IAB_FUNC_SHIFT) |
	IAB_MODIFY_SRC_FACTOR | (BLENDFACT_ONE << IAB_SRC_FACTOR_SHIFT) |
	IAB_MODIFY_DST_FACTOR | (BLENDFACT_ZERO << IAB_DST_FACTOR_SHIFT),

    /* Disable texture crossbar */
    _3DSTATE_COORD_SET_BINDINGS |
	CSB_TCB (0, 0) |
	CSB_TCB (1, 1) |
	CSB_TCB (2, 2) |
	CSB_TCB (3, 3) |
	CSB_TCB (4, 4) |
	CSB_TCB (5, 5) |
	CSB_TCB (6, 6) |
	CSB_TCB (7, 7),

    _3DSTATE_MODES_4_CMD | ENABLE_LOGIC_OP_FUNC | LOGIC_OP_FUNC (LOGICOP_COPY),

    _3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	I1_LOAD_S (2) |
	I1_LOAD_S (3) |
	I1_LOAD_S (4) |
	I1_LOAD_S (5) |
	I1_LOAD_S (6) |
	4,
    S2_TEXCOORD_NONE,
    0, /* Disable texture coordinate wrap-shortest */
    (1 << S4_POINT_WIDTH_SHIFT) |
	S4_LINE_WIDTH_ONE |
	S4_FLATSHADE_ALPHA |
	S4_FLATSHADE_FOG |
	S4_FLATSHADE_SPECULAR |
	S4_FLATSHADE_COLOR |
	S4_CULLMODE_NONE |
	S4_VFMT_XY,
    0, /* Disable stencil buffer */
    S6_COLOR_WRITE_ENABLE,

    _3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT,

    /* disable indirect state */
    _3DSTATE_LOAD_INDIRECT,
    0,
};

static const cairo_surface_backend_t i915_surface_backend;

static cairo_surface_t *
i915_surface_create_from_cacheable_image (cairo_drm_device_t *base_dev,
	                                   cairo_surface_t *source);

static cairo_status_t
i915_bo_exec (i915_device_t *device, intel_bo_t *bo, uint32_t offset)
{
    struct drm_i915_gem_execbuffer2 execbuf;
    int ret, cnt, i;

    /* Add the batch buffer to the validation list.  */
    cnt = device->batch.exec_count;
    if (cnt > 0 && bo->base.handle == device->batch.exec[cnt-1].handle)
	i = cnt - 1;
    else
	i = device->batch.exec_count++;
    device->batch.exec[i].handle = bo->base.handle;
    device->batch.exec[i].relocation_count = device->batch.reloc_count;
    device->batch.exec[i].relocs_ptr = (uintptr_t) device->batch.reloc;
    device->batch.exec[i].alignment = 0;
    device->batch.exec[i].offset = 0;
    device->batch.exec[i].flags = 0;
    device->batch.exec[i].rsvd1 = 0;
    device->batch.exec[i].rsvd2 = 0;

    execbuf.buffers_ptr = (uintptr_t) device->batch.exec;
    execbuf.buffer_count = device->batch.exec_count;
    execbuf.batch_start_offset = offset;
    execbuf.batch_len = (device->batch.used << 2) + sizeof (device->batch_header);
    execbuf.DR1 = 0;
    execbuf.DR4 = 0;
    execbuf.num_cliprects = 0;
    execbuf.cliprects_ptr = 0;
    execbuf.flags = 0;
    execbuf.rsvd1 = 0;
    execbuf.rsvd2 = 0;

    do {
	ret = ioctl (device->intel.base.fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);
    } while (ret != 0 && errno == EINTR);

    if (device->debug & I915_DEBUG_SYNC && ret == 0)
	ret = ! intel_bo_wait (&device->intel, bo);

    if (0 && ret) {
	int n, m;

	fprintf (stderr, "Batch submission failed: %d\n", errno);
	fprintf (stderr, "   relocation entries: %d/%d\n",
		 device->batch.reloc_count, I915_MAX_RELOCS);
	fprintf (stderr, "   gtt size: (%zd/%zd), (%zd/%zd)\n",
		 device->batch.est_gtt_size, device->batch.gtt_avail_size,
		 device->batch.total_gtt_size, device->intel.gtt_avail_size);

	fprintf (stderr, "   buffers:\n");
	for (n = 0; n < device->batch.exec_count; n++) {
	    fprintf (stderr, "  exec[%d] = %d, %d/%d bytes, gtt = %qx\n",
		    n,
		    device->batch.exec[n].handle,
		    n == device->batch.exec_count - 1 ? bo->base.size : device->batch.target_bo[n]->base.size,
		    n == device->batch.exec_count - 1 ? bo->full_size : device->batch.target_bo[n]->full_size,
		    device->batch.exec[n].offset);
	}
	for (n = 0; n < device->batch.reloc_count; n++) {
	    for (m = 0; m < device->batch.exec_count; m++)
		if (device->batch.exec[m].handle == device->batch.reloc[n].target_handle)
		    break;

	    fprintf (stderr, "  reloc[%d] = %d @ %qx -> %qx + %qx\n", n,
		     device->batch.reloc[n].target_handle,
		     device->batch.reloc[n].offset,
		     (unsigned long long) device->batch.exec[m].offset,
		     (unsigned long long) device->batch.reloc[n].delta);

	    device->batch_base[(device->batch.reloc[n].offset - sizeof (device->batch_header)) / 4] =
		device->batch.exec[m].offset + device->batch.reloc[n].delta;
	}

	intel_dump_batchbuffer (device->batch_header,
				execbuf.batch_len,
				device->intel.base.chip_id);
    }
    assert (ret == 0);

    VG (VALGRIND_MAKE_MEM_DEFINED (device->batch.exec, sizeof (device->batch.exec[0]) * i));

    bo->offset = device->batch.exec[i].offset;
    bo->busy = TRUE;
    if (bo->virtual)
	intel_bo_unmap (bo);
    bo->cpu = FALSE;

    while (cnt--) {
	intel_bo_t *bo = device->batch.target_bo[cnt];

	bo->offset = device->batch.exec[cnt].offset;
	bo->exec = NULL;
	bo->busy = TRUE;
	bo->batch_read_domains = 0;
	bo->batch_write_domain = 0;
	cairo_list_del (&bo->cache_list);

	if (bo->virtual)
	    intel_bo_unmap (bo);
	bo->cpu = FALSE;

	intel_bo_destroy (&device->intel, bo);
    }
    assert (cairo_list_is_empty (&device->intel.bo_in_flight));

    device->batch.exec_count = 0;
    device->batch.reloc_count = 0;
    device->batch.fences = 0;

    device->batch.est_gtt_size = I915_BATCH_SIZE;
    device->batch.total_gtt_size = I915_BATCH_SIZE;

    return ret == 0 ? CAIRO_STATUS_SUCCESS : _cairo_error (CAIRO_STATUS_NO_MEMORY);
}

void
i915_batch_add_reloc (i915_device_t *device,
		      uint32_t pos,
		      intel_bo_t *bo,
		      uint32_t offset,
		      uint32_t read_domains,
		      uint32_t write_domain,
		      cairo_bool_t needs_fence)
{
    int index;

    assert (offset < bo->base.size);

    if (bo->exec == NULL) {
	device->batch.total_gtt_size += bo->base.size;

	if (! bo->busy)
	    device->batch.est_gtt_size += bo->base.size;

	assert (device->batch.exec_count < ARRAY_LENGTH (device->batch.exec));

	index = device->batch.exec_count++;
	device->batch.exec[index].handle = bo->base.handle;
	device->batch.exec[index].relocation_count = 0;
	device->batch.exec[index].relocs_ptr = 0;
	device->batch.exec[index].alignment = 0;
	device->batch.exec[index].offset = 0;
	device->batch.exec[index].flags = 0;
	device->batch.exec[index].rsvd1 = 0;
	device->batch.exec[index].rsvd2 = 0;

	device->batch.target_bo[index] = intel_bo_reference (bo);

	bo->exec = &device->batch.exec[index];
    }

    if (bo->tiling != I915_TILING_NONE) {
	uint32_t alignment;

#if 0
	/* We presume that we will want to use a fence with X tiled objects... */
	if (needs_fence || bo->tiling == I915_TILING_X)
	    alignment = bo->full_size;
	else
	    alignment = 2*((bo->stride + 4095) & -4096);
#else
	alignment = bo->full_size;
#endif
	if (bo->exec->alignment < alignment)
	    bo->exec->alignment = alignment;

	if (needs_fence && (bo->exec->flags & EXEC_OBJECT_NEEDS_FENCE) == 0) {
	    bo->exec->flags |= EXEC_OBJECT_NEEDS_FENCE;
	    device->batch.fences++;

	    intel_bo_set_tiling (&device->intel, bo);
	}
    }

    assert (device->batch.reloc_count < ARRAY_LENGTH (device->batch.reloc));

    index = device->batch.reloc_count++;
    device->batch.reloc[index].offset = (pos << 2) + sizeof (device->batch_header);
    device->batch.reloc[index].delta = offset;
    device->batch.reloc[index].target_handle = bo->base.handle;
    device->batch.reloc[index].read_domains = read_domains;
    device->batch.reloc[index].write_domain = write_domain;
    device->batch.reloc[index].presumed_offset = bo->offset;

    assert (write_domain == 0 || bo->batch_write_domain == 0 || bo->batch_write_domain == write_domain);
    bo->batch_read_domains |= read_domains;
    bo->batch_write_domain |= write_domain;
}

void
i915_vbo_finish (i915_device_t *device)
{
    intel_bo_t *vbo;

    assert (CAIRO_MUTEX_IS_LOCKED (device->intel.base.base.mutex));
    assert (device->vbo_used);

    if (device->vertex_count) {
	if (device->vbo == 0) {
	    OUT_DWORD (_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		       I1_LOAD_S (0) |
		       I1_LOAD_S (1) |
		       1);
	    device->vbo = device->batch.used++;
	    device->vbo_max_index = device->batch.used;
	    OUT_DWORD ((device->floats_per_vertex << S1_VERTEX_WIDTH_SHIFT) |
		       (device->floats_per_vertex << S1_VERTEX_PITCH_SHIFT));
	}

	OUT_DWORD (PRIM3D_RECTLIST |
		   PRIM3D_INDIRECT_SEQUENTIAL |
		   device->vertex_count);
	OUT_DWORD (device->vertex_index);
    }

    if (device->last_vbo != NULL) {
	intel_bo_in_flight_add (&device->intel, device->last_vbo);
	intel_bo_destroy (&device->intel, device->last_vbo);
    }

    device->batch_base[device->vbo_max_index] |= device->vertex_index + device->vertex_count;

    /* will include a few bytes of inter-array padding */
    vbo = intel_bo_create (&device->intel,
			   device->vbo_used, device->vbo_used,
			   FALSE, I915_TILING_NONE, 0);
    i915_batch_fill_reloc (device, device->vbo, vbo, 0,
			   I915_GEM_DOMAIN_VERTEX, 0);
    intel_bo_write (&device->intel, vbo, 0, device->vbo_used, device->vbo_base);
    device->last_vbo = vbo;
    device->last_vbo_offset = (device->vbo_used+7)&-8;
    device->last_vbo_space = vbo->base.size - device->last_vbo_offset;

    device->vbo = 0;

    device->vbo_used = device->vbo_offset = 0;
    device->vertex_index = device->vertex_count = 0;

    if (! i915_check_aperture_size (device, 1, I915_VBO_SIZE, I915_VBO_SIZE)) {
	cairo_status_t status;

	status = i915_batch_flush (device);
	if (unlikely (status))
	    longjmp (device->shader->unwind, status);

	status = i915_shader_commit (device->shader, device);
	if (unlikely (status))
	    longjmp (device->shader->unwind, status);
    }
}

/* XXX improve state tracker/difference and flush state on vertex emission */
static void
i915_device_reset (i915_device_t *device)
{
    if (device->current_source != NULL)
	*device->current_source = 0;
    if (device->current_mask != NULL)
	*device->current_mask = 0;
    if (device->current_clip != NULL)
	*device->current_clip = 0;

    device->current_target = NULL;
    device->current_size = 0;
    device->current_source = NULL;
    device->current_mask = NULL;
    device->current_clip = NULL;
    device->current_texcoords = ~0;
    device->current_blend = 0;
    device->current_n_constants = 0;
    device->current_n_samplers = 0;
    device->current_n_maps = 0;
    device->current_colorbuf = 0;
    device->current_diffuse = 0;
    device->current_program = ~0;
    device->clear_alpha = ~0;

    device->last_source_fragment = ~0;
}

static void
i915_batch_cleanup (i915_device_t *device)
{
    int i;

    for (i = 0; i < device->batch.exec_count; i++) {
	intel_bo_t *bo = device->batch.target_bo[i];

	bo->exec = NULL;
	bo->batch_read_domains = 0;
	bo->batch_write_domain = 0;
	cairo_list_del (&bo->cache_list);

	intel_bo_destroy (&device->intel, bo);
    }

    device->batch.exec_count = 0;
    device->batch.reloc_count = 0;
}

static void
i915_batch_vbo_finish (i915_device_t *device)
{
    assert (CAIRO_MUTEX_IS_LOCKED (device->intel.base.base.mutex));

    if (device->vbo || i915_batch_space (device) < (int32_t) device->vbo_used) {
	intel_bo_t *vbo;

	if (device->vertex_count) {
	    if (device->vbo == 0) {
		OUT_DWORD (_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
			   I1_LOAD_S (0) |
			   I1_LOAD_S (1) |
			   1);
		device->vbo = device->batch.used++;
		device->vbo_max_index = device->batch.used;
		OUT_DWORD ((device->floats_per_vertex << S1_VERTEX_WIDTH_SHIFT) |
			(device->floats_per_vertex << S1_VERTEX_PITCH_SHIFT));
	    }

	    OUT_DWORD (PRIM3D_RECTLIST |
		       PRIM3D_INDIRECT_SEQUENTIAL |
		       device->vertex_count);
	    OUT_DWORD (device->vertex_index);
	}

	if (device->last_vbo != NULL)
	    intel_bo_destroy (&device->intel, device->last_vbo);

	device->batch_base[device->vbo_max_index] |= device->vertex_index + device->vertex_count;

	/* will include a few bytes of inter-array padding */
	vbo = intel_bo_create (&device->intel,
			       device->vbo_used, device->vbo_used,
			       FALSE, I915_TILING_NONE, 0);
	i915_batch_fill_reloc (device, device->vbo,
			       vbo, 0,
			       I915_GEM_DOMAIN_VERTEX, 0);
	intel_bo_write (&device->intel, vbo, 0, device->vbo_used, device->vbo_base);
	device->last_vbo = vbo;
	device->last_vbo_offset = (device->vbo_used+7)&-8;
	device->last_vbo_space = vbo->base.size - device->last_vbo_offset;

	device->vbo = 0;
    }
    else
    {
	/* Only a single rectlist in this batch, and no active vertex buffer. */
	OUT_DWORD (PRIM3D_RECTLIST | (device->vbo_used / 4 - 1));

	memcpy (BATCH_PTR (device), device->vbo_base, device->vbo_used);
	device->batch.used += device->vbo_used >> 2;
    }

    device->vbo_used = device->vbo_offset = 0;
    device->vertex_index = device->vertex_count = 0;
}

cairo_status_t
i915_batch_flush (i915_device_t *device)
{
    intel_bo_t *batch;
    cairo_status_t status;
    uint32_t length, offset;
    int n;

    assert (CAIRO_MUTEX_IS_LOCKED (device->intel.base.base.mutex));

    if (device->vbo_used)
	i915_batch_vbo_finish (device);

    if (device->batch.used == 0)
	return CAIRO_STATUS_SUCCESS;

    i915_batch_emit_dword (device, MI_BATCH_BUFFER_END);
    if ((device->batch.used & 1) != ((sizeof (device->batch_header)>>2) & 1))
	i915_batch_emit_dword (device, MI_NOOP);

    length = (device->batch.used << 2) + sizeof (device->batch_header);

    /* NB: it is faster to copy the data then map/unmap the batch,
     * presumably because we frequently only use a small part of the buffer.
     */
    batch = NULL;
    if (device->last_vbo) {
	if (length <= device->last_vbo_space) {
	    batch = device->last_vbo;
	    offset = device->last_vbo_offset;

	    /* fixup the relocations */
	    for (n = 0; n < device->batch.reloc_count; n++)
		device->batch.reloc[n].offset += offset;
	} else
	    intel_bo_destroy (&device->intel, device->last_vbo);
	device->last_vbo = NULL;
    }
    if (batch == NULL) {
	batch = intel_bo_create (&device->intel,
				 length, length,
				 FALSE, I915_TILING_NONE, 0);
	if (unlikely (batch == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    i915_batch_cleanup (device);
	    goto BAIL;
	}

	offset = 0;
    }
    intel_bo_write (&device->intel, batch, offset, length, device->batch_header);
    status = i915_bo_exec (device, batch, offset);
    intel_bo_destroy (&device->intel, batch);

BAIL:
    device->batch.used = 0;

    intel_glyph_cache_unpin (&device->intel);
    intel_snapshot_cache_thaw (&device->intel);

    i915_device_reset (device);

    return status;
}

#if 0
static float *
i915_add_rectangles (i915_device_t *device, int num_rects, int *count)
{
    float *vertices;
    uint32_t size;
    int cnt;

    assert (device->floats_per_vertex);

    size = device->rectangle_size;
    if (unlikely (device->vbo_offset + size > I915_VBO_SIZE))
	i915_vbo_finish (device);

    vertices = (float *) (device->vbo_base + device->vbo_offset);
    cnt = (I915_VBO_SIZE - device->vbo_offset) / size;
    if (cnt > num_rects)
	cnt = num_rects;
    device->vbo_used = device->vbo_offset += size * cnt;
    device->vertex_count += 3 * cnt;
    *count = cnt;
    return vertices;
}
#endif

static cairo_surface_t *
i915_surface_create_similar (void *abstract_other,
			     cairo_content_t content,
			     int width, int height)
{
    i915_surface_t *other;
    cairo_format_t format;
    uint32_t tiling = I915_TILING_DEFAULT;

    other = abstract_other;
    if (content == other->intel.drm.base.content)
	format = other->intel.drm.format;
    else
	format = _cairo_format_from_content (content);

    if (width * _cairo_format_bits_per_pixel (format) > 8 * 32*1024 || height > 64*1024)
	return NULL;

    /* we presume that a similar surface will be used for blitting */
    if (i915_surface_needs_tiling (other))
	tiling = I915_TILING_X;

    return i915_surface_create_internal ((cairo_drm_device_t *) other->intel.drm.base.device,
					 format,
					 width, height,
					 tiling, TRUE);
}

static cairo_status_t
i915_surface_finish (void *abstract_surface)
{
    i915_surface_t *surface = abstract_surface;
    i915_device_t *device = i915_device (surface);

    if (surface->stencil != NULL) {
	intel_bo_in_flight_add (&device->intel, surface->stencil);
	intel_bo_destroy (&device->intel, surface->stencil);
    }

    if (surface->is_current_texture) {
	if (surface->is_current_texture & CURRENT_SOURCE)
	    device->current_source = NULL;
	if (surface->is_current_texture & CURRENT_MASK)
	    device->current_mask = NULL;
	if (surface->is_current_texture & CURRENT_CLIP)
	    device->current_clip = NULL;
	device->current_n_samplers = 0;
    }

    if (surface == device->current_target)
	device->current_target = NULL;

    if (surface->cache != NULL) {
	i915_image_private_t *node = surface->cache;
	intel_buffer_cache_t *cache = node->container;

	if (--cache->ref_count == 0) {
	    intel_bo_in_flight_add (&device->intel, cache->buffer.bo);
	    intel_bo_destroy (&device->intel, cache->buffer.bo);
	    _cairo_rtree_fini (&cache->rtree);
	    cairo_list_del (&cache->link);
	    free (cache);
	} else {
	    node->node.state = CAIRO_RTREE_NODE_AVAILABLE;
	    cairo_list_move (&node->node.link, &cache->rtree.available);
	    _cairo_rtree_node_collapse (&cache->rtree, node->node.parent);
	}
    }

    return intel_surface_finish (&surface->intel);
}

static cairo_status_t
i915_surface_batch_flush (i915_surface_t *surface)
{
    cairo_status_t status;
    intel_bo_t *bo;

    assert (surface->intel.drm.fallback == NULL);

    bo = to_intel_bo (surface->intel.drm.bo);
    if (bo == NULL || bo->batch_write_domain == 0)
	return CAIRO_STATUS_SUCCESS;

    status = cairo_device_acquire (surface->intel.drm.base.device);
    if (unlikely (status))
	return status;

    status = i915_batch_flush (i915_device (surface));
    cairo_device_release (surface->intel.drm.base.device);

    return status;
}

static cairo_status_t
i915_surface_flush (void *abstract_surface,
		    unsigned flags)
{
    i915_surface_t *surface = abstract_surface;
    cairo_status_t status;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->intel.drm.fallback == NULL) {
	if (surface->intel.drm.base.finished) {
	    /* Forgo flushing on finish as the user cannot access the surface directly. */
	    return CAIRO_STATUS_SUCCESS;
	}

	if (surface->deferred_clear) {
	    status = i915_surface_clear (surface);
	    if (unlikely (status))
		return status;
	}

	return i915_surface_batch_flush (surface);
    }

    return intel_surface_flush (abstract_surface, flags);
}

/* rasterisation */

static cairo_status_t
_composite_boxes_spans (void			*closure,
			cairo_span_renderer_t	*renderer,
			const cairo_rectangle_int_t	*extents)
{
    cairo_boxes_t *boxes = closure;
    cairo_rectangular_scan_converter_t converter;
    struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    int i;

    _cairo_rectangular_scan_converter_init (&converter, extents);
    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	cairo_box_t *box = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    status = _cairo_rectangular_scan_converter_add_box (&converter, &box[i], 1);
	    if (unlikely (status))
		goto CLEANUP;
	}
    }

    status = converter.base.generate (&converter.base, renderer);

CLEANUP:
    converter.base.destroy (&converter.base);
    return status;
}

cairo_status_t
i915_fixup_unbounded (i915_surface_t *dst,
		      const cairo_composite_rectangles_t *extents,
		      cairo_clip_t *clip)
{
    i915_shader_t shader;
    i915_device_t *device;
    cairo_status_t status;

    if (clip != NULL) {
	cairo_region_t *clip_region = NULL;

	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED);
	assert (clip_region == NULL);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    clip = NULL;
    } else {
	if (extents->bounded.width == extents->unbounded.width &&
	    extents->bounded.height == extents->unbounded.height)
	{
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    if (clip != NULL) {
	i915_shader_init (&shader, dst, CAIRO_OPERATOR_DEST_OVER, 1.);
	i915_shader_set_clip (&shader, clip);
	status = i915_shader_acquire_pattern (&shader,
					      &shader.source,
					      &_cairo_pattern_white.base,
					      &extents->unbounded);
	assert (status == CAIRO_STATUS_SUCCESS);
    } else {
	i915_shader_init (&shader, dst, CAIRO_OPERATOR_CLEAR, 1.);
	status = i915_shader_acquire_pattern (&shader,
					      &shader.source,
					      &_cairo_pattern_clear.base,
					      &extents->unbounded);
	assert (status == CAIRO_STATUS_SUCCESS);
    }

    device = i915_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	return status;

    status = i915_shader_commit (&shader, device);
    if (unlikely (status))
	goto BAIL;

    if (extents->bounded.width == 0 || extents->bounded.height == 0) {
	shader.add_rectangle (&shader,
			      extents->unbounded.x,
			      extents->unbounded.y,
			      extents->unbounded.width,
			      extents->unbounded.height);
    } else {
	/* top */
	if (extents->bounded.y != extents->unbounded.y) {
	    shader.add_rectangle (&shader,
				  extents->unbounded.x,
				  extents->unbounded.y,
				  extents->unbounded.width,
				  extents->bounded.y - extents->unbounded.y);
	}

	/* left */
	if (extents->bounded.x != extents->unbounded.x) {
	    shader.add_rectangle (&shader,
				  extents->unbounded.x,
				  extents->bounded.y,
				  extents->bounded.x - extents->unbounded.x,
				  extents->bounded.height);
	}

	/* right */
	if (extents->bounded.x + extents->bounded.width != extents->unbounded.x + extents->unbounded.width) {
	    shader.add_rectangle (&shader,
				  extents->bounded.x + extents->bounded.width,
				  extents->bounded.y,
				  extents->unbounded.x + extents->unbounded.width - (extents->bounded.x + extents->bounded.width),
				  extents->bounded.height);
	}

	/* bottom */
	if (extents->bounded.y + extents->bounded.height != extents->unbounded.y + extents->unbounded.height) {
	    shader.add_rectangle (&shader,
				  extents->unbounded.x,
				  extents->bounded.y + extents->bounded.height,
				  extents->unbounded.width,
				  extents->unbounded.y + extents->unbounded.height - (extents->bounded.y + extents->bounded.height));
	}
    }

    i915_shader_fini (&shader);
  BAIL:
    cairo_device_release (&device->intel.base.base);
    return status;
}

static cairo_status_t
i915_fixup_unbounded_boxes (i915_surface_t *dst,
			    const cairo_composite_rectangles_t *extents,
			    cairo_clip_t *clip,
			    cairo_boxes_t *boxes)
{
    cairo_boxes_t clear;
    cairo_box_t box;
    cairo_region_t *clip_region = NULL;
    cairo_status_t status;
    struct _cairo_boxes_chunk *chunk;
    int i;

    if (boxes->num_boxes <= 1)
	return i915_fixup_unbounded (dst, extents, clip);

    _cairo_boxes_init (&clear);

    box.p1.x = _cairo_fixed_from_int (extents->unbounded.x + extents->unbounded.width);
    box.p1.y = _cairo_fixed_from_int (extents->unbounded.y);
    box.p2.x = _cairo_fixed_from_int (extents->unbounded.x);
    box.p2.y = _cairo_fixed_from_int (extents->unbounded.y + extents->unbounded.height);

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    clip = NULL;
    }

    if (clip_region == NULL) {
	cairo_boxes_t tmp;

	_cairo_boxes_init (&tmp);

	status = _cairo_boxes_add (&tmp, &box);
	assert (status == CAIRO_STATUS_SUCCESS);

	tmp.chunks.next = &boxes->chunks;
	tmp.num_boxes += boxes->num_boxes;

	status = _cairo_bentley_ottmann_tessellate_boxes (&tmp,
							  CAIRO_FILL_RULE_WINDING,
							  &clear);

	tmp.chunks.next = NULL;
    } else {
	pixman_box32_t *pbox;

	pbox = pixman_region32_rectangles (&clip_region->rgn, &i);
	_cairo_boxes_limit (&clear, (cairo_box_t *) pbox, i);

	status = _cairo_boxes_add (&clear, &box);
	assert (status == CAIRO_STATUS_SUCCESS);

	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		status = _cairo_boxes_add (&clear, &chunk->base[i]);
		if (unlikely (status)) {
		    _cairo_boxes_fini (&clear);
		    return status;
		}
	    }
	}

	status = _cairo_bentley_ottmann_tessellate_boxes (&clear,
							  CAIRO_FILL_RULE_WINDING,
							  &clear);
    }

    if (likely (status == CAIRO_STATUS_SUCCESS && clear.num_boxes)) {
	i915_shader_t shader;
	i915_device_t *device;

	if (clip != NULL) {
	    i915_shader_init (&shader, dst, CAIRO_OPERATOR_DEST_OVER, 1.);
	    i915_shader_set_clip (&shader, clip);
	    status = i915_shader_acquire_pattern (&shader,
						  &shader.source,
						  &_cairo_pattern_white.base,
						  &extents->unbounded);
	    assert (status == CAIRO_STATUS_SUCCESS);
	} else {
	    i915_shader_init (&shader, dst, CAIRO_OPERATOR_CLEAR, 1.);
	    status = i915_shader_acquire_pattern (&shader,
						  &shader.source,
						  &_cairo_pattern_clear.base,
						  &extents->unbounded);
	    assert (status == CAIRO_STATUS_SUCCESS);
	}

	device = i915_device (dst);
	status = cairo_device_acquire (&device->intel.base.base);
	if (unlikely (status))
	    goto err_shader;

	status = i915_shader_commit (&shader, device);
	if (unlikely (status))
	    goto err_device;

	for (chunk = &clear.chunks; chunk != NULL; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
		int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
		int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
		int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);

		shader.add_rectangle (&shader, x1, y1, x2 - x1, y2 - y1);
	    }
	}
err_device:
	cairo_device_release (&device->intel.base.base);
err_shader:
	i915_shader_fini (&shader);
    }

    _cairo_boxes_fini (&clear);

    return status;
}

static cairo_bool_t
i915_can_blt (i915_surface_t *dst,
	      const cairo_pattern_t *pattern)
{
    const cairo_surface_pattern_t *spattern;
    i915_surface_t *src;

    spattern = (const cairo_surface_pattern_t *) pattern;
    src = (i915_surface_t *) spattern->surface;

    if (src->intel.drm.base.device != dst->intel.drm.base.device)
	return FALSE;

    if (! i915_surface_needs_tiling (dst))
	return FALSE;

    if (! _cairo_matrix_is_translation (&pattern->matrix))
	return FALSE;

    if (! (pattern->filter == CAIRO_FILTER_NEAREST ||
	   pattern->filter == CAIRO_FILTER_FAST))
    {
	if (! _cairo_fixed_is_integer (_cairo_fixed_from_double (pattern->matrix.x0)) ||
	    ! _cairo_fixed_is_integer (_cairo_fixed_from_double (pattern->matrix.y0)))
	{
	    return FALSE;
	}
    }

    return _cairo_format_bits_per_pixel (src->intel.drm.format) ==
	_cairo_format_bits_per_pixel (dst->intel.drm.format);
}

static cairo_status_t
i915_blt (i915_surface_t *src,
	  i915_surface_t *dst,
	  int src_x, int src_y,
	  int width, int height,
	  int dst_x, int dst_y,
	  cairo_bool_t flush)
{
    i915_device_t *device;
    intel_bo_t *bo_array[2];
    cairo_status_t status;
    int br13, cmd;

    bo_array[0] = to_intel_bo (dst->intel.drm.bo);
    bo_array[1] = to_intel_bo (src->intel.drm.bo);

    status = i915_surface_fallback_flush (src);
    if (unlikely (status))
	return status;

    device = i915_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	return status;

    if (! i915_check_aperture_and_fences (device, bo_array, 2) ||
	i915_batch_space (device) < 9)
    {
	status = i915_batch_flush (device);
	if (unlikely (status))
	    goto CLEANUP;
    }

    cmd = XY_SRC_COPY_BLT_CMD;
    br13 = (0xCC << 16) | dst->intel.drm.stride;
    switch (dst->intel.drm.format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	ASSERT_NOT_REACHED;
    case CAIRO_FORMAT_A8:
	break;
    case CAIRO_FORMAT_RGB16_565:
	br13 |= BR13_565;
	break;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	br13 |= BR13_8888;
	cmd |= XY_BLT_WRITE_ALPHA | XY_BLT_WRITE_RGB;
	break;
    }

    OUT_DWORD (cmd);
    OUT_DWORD (br13);
    OUT_DWORD ((dst_y << 16) | dst_x);
    OUT_DWORD (((dst_y + height - 1) << 16) | (dst_x + width - 1));
    OUT_RELOC_FENCED (dst, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);
    OUT_DWORD ((src_y << 16) | src_x);
    OUT_DWORD (src->intel.drm.stride);
    OUT_RELOC_FENCED (src, I915_GEM_DOMAIN_RENDER, 0);
    /* require explicit RenderCache flush for 2D -> 3D sampler? */
    if (flush)
	OUT_DWORD (MI_FLUSH);

CLEANUP:
    cairo_device_release (&device->intel.base.base);
    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
i915_surface_copy_subimage (i915_device_t *device,
			    i915_surface_t *src,
			    const cairo_rectangle_int_t *extents,
			    cairo_bool_t flush,
			    i915_surface_t **clone_out)
{
    i915_surface_t *clone;
    cairo_status_t status;

    clone = (i915_surface_t *)
	i915_surface_create_internal (&device->intel.base,
				      src->intel.drm.format,
				      extents->width,
				      extents->height,
				      I915_TILING_X, TRUE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    status = i915_blt (src, clone,
		       extents->x, extents->y,
		       extents->width, extents->height,
		       0, 0,
		       flush);

    if (unlikely (status)) {
	cairo_surface_destroy (&clone->intel.drm.base);
	return status;
    }

    *clone_out = clone;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
i915_clear_boxes (i915_surface_t *dst,
		  const cairo_boxes_t *boxes)
{
    i915_device_t *device = i915_device (dst);
    const struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    intel_bo_t *bo_array[1] = { to_intel_bo (dst->intel.drm.bo) };
    int cmd, br13, clear = 0, i;

    cmd = XY_COLOR_BLT_CMD;
    br13 = (0xCC << 16) | dst->intel.drm.stride;
    switch (dst->intel.drm.format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	ASSERT_NOT_REACHED;
    case CAIRO_FORMAT_A8:
	break;
    case CAIRO_FORMAT_RGB16_565:
	br13 |= BR13_565;
	break;
    case CAIRO_FORMAT_RGB24:
	clear = 0xff000000;
    case CAIRO_FORMAT_ARGB32:
	br13 |= BR13_8888;
	cmd |= XY_BLT_WRITE_ALPHA | XY_BLT_WRITE_RGB;
	break;
    }

    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	return status;

    if (! i915_check_aperture_and_fences (device, bo_array, 1) ||
	i915_batch_space (device) < 6 * boxes->num_boxes)
    {
	status = i915_batch_flush (device);
	if (unlikely (status))
	    goto RELEASE;
    }

    if (device->vertex_count)
	i915_vbo_flush (device);

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_round (box[i].p1.x);
	    int x2 = _cairo_fixed_integer_round (box[i].p2.x);
	    int y1 = _cairo_fixed_integer_round (box[i].p1.y);
	    int y2 = _cairo_fixed_integer_round (box[i].p2.y);

	    if (x2 <= x1 || y2 <= y1)
		continue;

	    OUT_DWORD (cmd);
	    OUT_DWORD (br13);
	    OUT_DWORD ((y1 << 16) | x1);
	    OUT_DWORD (((y2 - 1) << 16) | (x2 - 1));
	    OUT_RELOC_FENCED (dst, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);
	    OUT_DWORD (clear);
	}
    }

RELEASE:
    cairo_device_release (&device->intel.base.base);
    return status;
}

static cairo_status_t
i915_surface_extract_X_from_Y (i915_device_t *device,
			       i915_surface_t *src,
			       const cairo_rectangle_int_t *extents,
			       i915_surface_t **clone_out)
{
    i915_surface_t *clone;
    i915_shader_t shader;
    cairo_surface_pattern_t pattern;
    cairo_rectangle_int_t rect;
    cairo_status_t status;

    status = i915_surface_fallback_flush (src);
    if (unlikely (status))
	return status;

    clone = (i915_surface_t *)
	i915_surface_create_internal (&device->intel.base,
				      src->intel.drm.format,
				      extents->width,
				      extents->height,
				      I915_TILING_X, TRUE);
    if (unlikely (clone->intel.drm.base.status))
	return clone->intel.drm.base.status;

    i915_shader_init (&shader, clone, CAIRO_OPERATOR_SOURCE, 1.);

    _cairo_pattern_init_for_surface (&pattern, &src->intel.drm.base);
    pattern.base.filter = CAIRO_FILTER_NEAREST;
    cairo_matrix_init_translate (&pattern.base.matrix, extents->x, extents->y);

    rect.x = rect.y = 0;
    rect.width = extents->width;
    rect.height = extents->height;
    status = i915_shader_acquire_pattern (&shader, &shader.source, &pattern.base, &rect);
    _cairo_pattern_fini (&pattern.base);

    if (unlikely (status))
	goto err_shader;

    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	goto err_shader;

    status = i915_shader_commit (&shader, device);
    if (unlikely (status))
	goto err_device;

    shader.add_rectangle (&shader, 0, 0, extents->width, extents->height);

    cairo_device_release (&device->intel.base.base);
    i915_shader_fini (&shader);

    *clone_out = clone;
    return CAIRO_STATUS_SUCCESS;

err_device:
    cairo_device_release (&device->intel.base.base);
err_shader:
    i915_shader_fini (&shader);
    cairo_surface_destroy (&clone->intel.drm.base);
    return status;
}

static cairo_status_t
i915_blt_boxes (i915_surface_t *dst,
		const cairo_pattern_t *pattern,
		const cairo_rectangle_int_t *extents,
		const cairo_boxes_t *boxes)
{
    const cairo_surface_pattern_t *spattern;
    i915_device_t *device;
    i915_surface_t *src;
    cairo_surface_t *free_me = NULL;
    const struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    int br13, cmd, tx, ty;
    intel_bo_t *bo_array[2];
    int i;

    if (! i915_can_blt (dst, pattern))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    spattern = (const cairo_surface_pattern_t *) pattern;
    src = (i915_surface_t *) spattern->surface;

    if (src->intel.drm.base.is_clear)
	return i915_clear_boxes (dst, boxes);

    if (pattern->extend != CAIRO_EXTEND_NONE &&
	(extents->x + tx < 0 ||
	 extents->y + ty < 0 ||
	 extents->x + tx + extents->width  > src->intel.drm.width ||
	 extents->y + ty + extents->height > src->intel.drm.height))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    status = i915_surface_fallback_flush (src);
    if (unlikely (status))
	return status;

    tx = _cairo_lround (pattern->matrix.x0);
    ty = _cairo_lround (pattern->matrix.y0);

    device = i915_device (dst);
    if (to_intel_bo (src->intel.drm.bo)->tiling == I915_TILING_Y) {
	cairo_rectangle_int_t extents;

	_cairo_boxes_extents (boxes, &extents);
	extents.x += tx;
	extents.y += ty;

	status = i915_surface_extract_X_from_Y (device, src, &extents, &src);
	if (unlikely (status))
	    return status;

	free_me = &src->intel.drm.base;
	tx = -extents.x;
	ty = -extents.y;
    }

    bo_array[0] = to_intel_bo (dst->intel.drm.bo);
    bo_array[1] = to_intel_bo (src->intel.drm.bo);

    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	goto CLEANUP_SURFACE;

    if (! i915_check_aperture_and_fences (device, bo_array, 2) ||
	i915_batch_space (device) < 8 * boxes->num_boxes)
    {
	status = i915_batch_flush (device);
	if (unlikely (status))
	    goto CLEANUP_DEVICE;
    }

    cmd = XY_SRC_COPY_BLT_CMD;
    br13 = (0xCC << 16) | dst->intel.drm.stride;
    switch (dst->intel.drm.format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	ASSERT_NOT_REACHED;
    case CAIRO_FORMAT_A8:
	break;
    case CAIRO_FORMAT_RGB16_565:
	br13 |= BR13_565;
	break;
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_ARGB32:
	br13 |= BR13_8888;
	cmd |= XY_BLT_WRITE_ALPHA | XY_BLT_WRITE_RGB;
	break;
    }

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	const cairo_box_t *box = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_round (box[i].p1.x);
	    int x2 = _cairo_fixed_integer_round (box[i].p2.x);
	    int y1 = _cairo_fixed_integer_round (box[i].p1.y);
	    int y2 = _cairo_fixed_integer_round (box[i].p2.y);

	    if (x1 + tx < 0)
		x1 = -tx;
	    if (x2 + tx > src->intel.drm.width)
		x2 = src->intel.drm.width - tx;

	    if (y1 + ty < 0)
		y1 = -ty;
	    if (y2 + ty > src->intel.drm.height)
		y2 = src->intel.drm.height - ty;

	    if (x2 <= x1 || y2 <= y1)
		continue;
	    if (x2 < 0 || y2 < 0)
		continue;
	    if (x1 >= dst->intel.drm.width || y2 >= dst->intel.drm.height)
		continue;

	    OUT_DWORD (cmd);
	    OUT_DWORD (br13);
	    OUT_DWORD ((y1 << 16) | x1);
	    OUT_DWORD (((y2 - 1) << 16) | (x2 - 1));
	    OUT_RELOC_FENCED (dst, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);
	    OUT_DWORD (((y1 + ty) << 16) | (x1 + tx));
	    OUT_DWORD (src->intel.drm.stride);
	    OUT_RELOC_FENCED (src, I915_GEM_DOMAIN_RENDER, 0);
	}
    }

    /* XXX fixup blank portions */

CLEANUP_DEVICE:
    cairo_device_release (&device->intel.base.base);
CLEANUP_SURFACE:
    cairo_surface_destroy (free_me);
    return status;
}

static cairo_status_t
_upload_image_inplace (i915_surface_t *surface,
		       const cairo_pattern_t *source,
		       const cairo_rectangle_int_t *extents,
		       const cairo_boxes_t *boxes)
{
    i915_device_t *device;
    const cairo_surface_pattern_t *pattern;
    cairo_image_surface_t *image;
    const struct _cairo_boxes_chunk *chunk;
    intel_bo_t *bo;
    int tx, ty, i;

    if (source->type != CAIRO_PATTERN_TYPE_SURFACE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    pattern = (const cairo_surface_pattern_t *) source;
    if (pattern->surface->type != CAIRO_SURFACE_TYPE_IMAGE)
	return CAIRO_INT_STATUS_UNSUPPORTED;

    if (! _cairo_matrix_is_integer_translation (&source->matrix, &tx, &ty))
	return CAIRO_INT_STATUS_UNSUPPORTED;

    image = (cairo_image_surface_t *) pattern->surface;
    if (source->extend != CAIRO_EXTEND_NONE &&
	(extents->x + tx < 0 ||
	 extents->y + ty < 0 ||
	 extents->x + tx + extents->width  > image->width ||
	 extents->y + ty + extents->height > image->height))
    {
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    device = i915_device (surface);
    bo = to_intel_bo (surface->intel.drm.bo);
    if (bo->exec != NULL || ! intel_bo_is_inactive (&device->intel, bo)) {
	intel_bo_t *new_bo;
	cairo_bool_t need_clear = FALSE;

	if (boxes->num_boxes != 1 ||
	    extents->width < surface->intel.drm.width ||
	    extents->height < surface->intel.drm.height)
	{
	    if (! surface->intel.drm.base.is_clear)
		return CAIRO_INT_STATUS_UNSUPPORTED;

	    need_clear = TRUE;
	}

	new_bo = intel_bo_create (&device->intel,
				  bo->full_size, bo->base.size,
				  FALSE, bo->tiling, bo->stride);
	if (unlikely (new_bo == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	intel_bo_in_flight_add (&device->intel, bo);
	intel_bo_destroy (&device->intel, bo);

	bo = new_bo;
	surface->intel.drm.bo = &bo->base;

	if (need_clear) {
	    memset (intel_bo_map (&device->intel, bo), 0,
		    bo->stride * surface->intel.drm.height);
	}
    }

    if (image->format == surface->intel.drm.format) {
	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    cairo_box_t *box = chunk->base;
	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_round (box[i].p1.x);
		int x2 = _cairo_fixed_integer_round (box[i].p2.x);
		int y1 = _cairo_fixed_integer_round (box[i].p1.y);
		int y2 = _cairo_fixed_integer_round (box[i].p2.y);
		cairo_status_t status;

		if (x1 + tx < 0)
		    x1 = -tx;
		if (x2 + tx > image->width)
		    x2 = image->width - tx;

		if (y1 + ty < 0)
		    y1 = -ty;
		if (y2 + ty > image->height)
		    y2 = image->height - ty;

		if (x2 <= x1 || y2 <= y1)
		    continue;
		if (x2 < 0 || y2 < 0)
		    continue;
		if (x1 >= surface->intel.drm.width || y2 >= surface->intel.drm.height)
		    continue;

		status = intel_bo_put_image (&device->intel,
					     bo,
					     image,
					     x1 + tx, y1 + ty,
					     x2 - x1, y2 - y1,
					     x1, y1);
		if (unlikely (status))
		    return status;
	    }
	}
    } else {
	pixman_image_t *dst;
	void *ptr;

	ptr = intel_bo_map (&device->intel, bo);
	if (unlikely (ptr == NULL))
	    return _cairo_error (CAIRO_STATUS_DEVICE_ERROR);

	dst = pixman_image_create_bits (_cairo_format_to_pixman_format_code (surface->intel.drm.format),
					surface->intel.drm.width,
					surface->intel.drm.height,
					ptr,
					surface->intel.drm.stride);
	if (unlikely (dst == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	    cairo_box_t *box = chunk->base;
	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_round (box[i].p1.x);
		int x2 = _cairo_fixed_integer_round (box[i].p2.x);
		int y1 = _cairo_fixed_integer_round (box[i].p1.y);
		int y2 = _cairo_fixed_integer_round (box[i].p2.y);

		if (x1 + tx < 0)
		    x1 = -tx;
		if (x2 + tx > image->width)
		    x2 = image->width - tx;

		if (y1 + ty < 0)
		    y1 = -ty;
		if (y2 + ty > image->height)
		    y2 = image->height - ty;

		if (x2 <= x1 || y2 <= y1)
		    continue;
		if (x2 < 0 || y2 < 0)
		    continue;
		if (x1 >= surface->intel.drm.width || y2 >= surface->intel.drm.height)
		    continue;

		pixman_image_composite32 (PIXMAN_OP_SRC,
					  image->pixman_image, NULL, dst,
					  x1 + tx, y1 + ty,
					  0, 0,
					  x1, y1,
					  x2 - x1, y2 - y1);
	    }
	}

	pixman_image_unref (dst);
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_composite_boxes (i915_surface_t *dst,
		  cairo_operator_t op,
		  const cairo_pattern_t *pattern,
		  cairo_boxes_t *boxes,
		  cairo_antialias_t antialias,
		  cairo_clip_t *clip,
		  double opacity,
		  const cairo_composite_rectangles_t *extents)
{
    cairo_bool_t need_clip_surface = FALSE;
    cairo_region_t *clip_region = NULL;
    const struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    i915_shader_t shader;
    i915_device_t *device;
    int i;

    /* If the boxes are not pixel-aligned, we will need to compute a real mask */
    if (antialias != CAIRO_ANTIALIAS_NONE) {
	if (! boxes->is_pixel_aligned)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    if (clip == NULL && op == CAIRO_OPERATOR_SOURCE && opacity == 1.) {
	if (pattern->type == CAIRO_PATTERN_TYPE_SURFACE) {
	    status = i915_blt_boxes (dst, pattern, &extents->bounded, boxes);
	    if (status == CAIRO_INT_STATUS_UNSUPPORTED) {
		status = _upload_image_inplace (dst, pattern,
						&extents->bounded, boxes);
	    }
	    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
		return status;
	}
    }

    if (i915_surface_needs_tiling (dst)) {
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    i915_shader_init (&shader, dst, op, opacity);

    status = i915_shader_acquire_pattern (&shader,
					  &shader.source,
					  pattern,
					  &extents->bounded);
    if (unlikely (status))
	return status;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || status == CAIRO_INT_STATUS_UNSUPPORTED);
	need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
	if (need_clip_surface)
	    i915_shader_set_clip (&shader, clip);
    }

    device = i915_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	goto err_shader;

    status = i915_shader_commit (&shader, device);
    if (unlikely (status))
	goto err_device;

    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	cairo_box_t *box = chunk->base;
	for (i = 0; i < chunk->count; i++) {
	    int x1 = _cairo_fixed_integer_round (box[i].p1.x);
	    int y1 = _cairo_fixed_integer_round (box[i].p1.y);
	    int x2 = _cairo_fixed_integer_round (box[i].p2.x);
	    int y2 = _cairo_fixed_integer_round (box[i].p2.y);

	    if (x2 > x1 && y2 > y1)
		shader.add_rectangle (&shader, x1, y1, x2 - x1, y2 - y1);
	}
    }

    if (! extents->is_bounded)
	status = i915_fixup_unbounded_boxes (dst, extents, clip, boxes);

  err_device:
    cairo_device_release (&device->intel.base.base);
  err_shader:
    i915_shader_fini (&shader);

    return status;
}

cairo_status_t
i915_surface_clear (i915_surface_t *dst)
{
    i915_device_t *device;
    cairo_status_t status;
    intel_bo_t *bo_array[1] = { to_intel_bo (dst->intel.drm.bo) };

    device = i915_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	return status;

    if (i915_surface_needs_tiling (dst)) {
	int cmd, br13, clear = 0;

	if (! i915_check_aperture_and_fences (device, bo_array, 1) ||
	    i915_batch_space (device) < 6)
	{
	    status = i915_batch_flush (device);
	    if (unlikely (status)) {
		cairo_device_release (&device->intel.base.base);
		return status;
	    }
	}

	if (device->vertex_count)
	    i915_vbo_flush (device);

	cmd = XY_COLOR_BLT_CMD;
	br13 = (0xCC << 16) | dst->intel.drm.stride;
	switch (dst->intel.drm.format) {
	default:
	case CAIRO_FORMAT_INVALID:
	case CAIRO_FORMAT_A1:
	    ASSERT_NOT_REACHED;
	case CAIRO_FORMAT_A8:
	    break;
	case CAIRO_FORMAT_RGB16_565:
	    br13 |= BR13_565;
	    break;
	case CAIRO_FORMAT_RGB24:
	    clear = 0xff000000;
	case CAIRO_FORMAT_ARGB32:
	    br13 |= BR13_8888;
	    cmd |= XY_BLT_WRITE_ALPHA | XY_BLT_WRITE_RGB;
	    break;
	}

	OUT_DWORD (cmd);
	OUT_DWORD (br13);
	OUT_DWORD (0);
	OUT_DWORD (((dst->intel.drm.height - 1) << 16) |
		   (dst->intel.drm.width - 1));
	OUT_RELOC_FENCED (dst,
			  I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);
	OUT_DWORD (clear);
    } else {
	if (! i915_check_aperture (device, bo_array, 1) ||
	    i915_batch_space (device) < 24)
	{
	    status = i915_batch_flush (device);
	    if (unlikely (status)) {
		cairo_device_release (&device->intel.base.base);
		return status;
	    }
	}

	if (device->vertex_count)
	    i915_vbo_flush (device);

	i915_set_dst (device, dst);

	/* set clear parameters */
	if (device->clear_alpha != (dst->intel.drm.base.content & CAIRO_CONTENT_ALPHA)) {
	    device->clear_alpha = dst->intel.drm.base.content & CAIRO_CONTENT_ALPHA;
	    OUT_DWORD (_3DSTATE_CLEAR_PARAMETERS);
	    OUT_DWORD (CLEARPARAM_CLEAR_RECT | CLEARPARAM_WRITE_COLOR);
	    /* ZONE_INIT color */
	    if (device->clear_alpha) /* XXX depends on pixel format, 16bit needs replication, 8bit? */
		OUT_DWORD (0x00000000);
	    else
		OUT_DWORD (0xff000000);
	    OUT_DWORD (0); /* ZONE_INIT depth */
	    /* CLEAR_RECT color */
	    if (device->clear_alpha)
		OUT_DWORD (0x00000000);
	    else
		OUT_DWORD (0xff000000);
	    OUT_DWORD (0); /* CLEAR_RECT depth */
	    OUT_DWORD (0); /* CLEAR_RECT stencil */
	}

	OUT_DWORD (PRIM3D_CLEAR_RECT | 5);
	OUT_DWORD (pack_float (dst->intel.drm.width));
	OUT_DWORD (pack_float (dst->intel.drm.height));
	OUT_DWORD (0);
	OUT_DWORD (pack_float (dst->intel.drm.height));
	OUT_DWORD (0);
	OUT_DWORD (0);
    }

    cairo_device_release (&device->intel.base.base);

    dst->deferred_clear = FALSE;
    return status;
}

static cairo_status_t
_clip_and_composite_boxes (i915_surface_t *dst,
			   cairo_operator_t op,
			   const cairo_pattern_t *src,
			   cairo_boxes_t *boxes,
			   cairo_antialias_t antialias,
			   const cairo_composite_rectangles_t *extents,
			   cairo_clip_t *clip,
			   double opacity)
{
    cairo_status_t status;

    if (boxes->num_boxes == 0) {
	if (extents->is_bounded)
	    return CAIRO_STATUS_SUCCESS;

	return i915_fixup_unbounded (dst, extents, clip);
    }

    if (clip == NULL &&
	(op == CAIRO_OPERATOR_SOURCE || (op == CAIRO_OPERATOR_OVER && dst->intel.drm.base.is_clear)) &&
	opacity == 1. &&
	boxes->num_boxes == 1 &&
	extents->bounded.width  == dst->intel.drm.width &&
	extents->bounded.height == dst->intel.drm.height)
    {
	op = CAIRO_OPERATOR_SOURCE;
	dst->deferred_clear = FALSE;

	status = _upload_image_inplace (dst, src,
					&extents->bounded, boxes);
	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    return status;
    }

    if (dst->deferred_clear) {
	status = i915_surface_clear (dst);
	if (unlikely (status))
	    return status;
    }

    /* Use a fast path if the boxes are pixel aligned */
    status = _composite_boxes (dst, op, src, boxes, antialias, clip, opacity, extents);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    /* Otherwise render the boxes via an implicit mask and composite in the usual
     * fashion.
     */
    return i915_clip_and_composite_spans (dst, op, src, antialias,
					  _composite_boxes_spans, boxes,
					  extents, clip, opacity);
}

static cairo_clip_path_t *
_clip_get_solitary_path (cairo_clip_t *clip)
{
    cairo_clip_path_t *iter = clip->path;
    cairo_clip_path_t *path = NULL;

    do {
	if ((iter->flags & CAIRO_CLIP_PATH_IS_BOX) == 0) {
	    if (path != NULL)
		return FALSE;

	    path = iter;
	}
	iter = iter->prev;
    } while (iter != NULL);

    return path;
}

typedef struct {
    cairo_polygon_t		polygon;
    cairo_fill_rule_t		 fill_rule;
    cairo_antialias_t		 antialias;
} composite_polygon_info_t;

static cairo_status_t
_composite_polygon_spans (void                          *closure,
			  cairo_span_renderer_t		*renderer,
			  const cairo_rectangle_int_t   *extents)
{
    composite_polygon_info_t *info = closure;
    cairo_botor_scan_converter_t converter;
    cairo_status_t status;
    cairo_box_t box;

    box.p1.x = _cairo_fixed_from_int (extents->x);
    box.p1.y = _cairo_fixed_from_int (extents->y);
    box.p2.x = _cairo_fixed_from_int (extents->x + extents->width);
    box.p2.y = _cairo_fixed_from_int (extents->y + extents->height);

    _cairo_botor_scan_converter_init (&converter, &box, info->fill_rule);

    status = converter.base.add_polygon (&converter.base, &info->polygon);
    if (likely (status == CAIRO_STATUS_SUCCESS))
	status = converter.base.generate (&converter.base, renderer);

    converter.base.destroy (&converter.base);

    return status;
}

static cairo_int_status_t
i915_surface_fill_with_alpha (void			*abstract_dst,
			      cairo_operator_t		 op,
			      const cairo_pattern_t	*source,
			      cairo_path_fixed_t	*path,
			      cairo_fill_rule_t		 fill_rule,
			      double			 tolerance,
			      cairo_antialias_t		 antialias,
			      cairo_clip_t		*clip,
			      double			 opacity)
{
    i915_surface_t *dst = abstract_dst;
    cairo_composite_rectangles_t extents;
    composite_polygon_info_t info;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_status_t status;

    status = _cairo_composite_rectangles_init_for_fill (&extents,
							dst->intel.drm.width,
							dst->intel.drm.height,
							op, source, path,
							clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (extents.is_bounded && clip != NULL) {
	cairo_clip_path_t *clip_path;

	if (((clip_path = _clip_get_solitary_path (clip)) != NULL) &&
	    _cairo_path_fixed_equal (&clip_path->path, path))
	{
	    clip = NULL;
	}
    }

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    assert (! _cairo_path_fixed_fill_is_empty (path));

    if (_cairo_path_fixed_fill_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	_cairo_boxes_init (&boxes);
	_cairo_boxes_limit (&boxes, clip_boxes, num_boxes);
	status = _cairo_path_fixed_fill_rectilinear_to_boxes (path,
							      fill_rule,
							      &boxes);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = _clip_and_composite_boxes (dst, op, source,
						&boxes, antialias,
						&extents, clip,
						opacity);
	}

	_cairo_boxes_fini (&boxes);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    goto CLEANUP_BOXES;
    }

    _cairo_polygon_init (&info.polygon, clip_boxes, num_boxes);

    status = _cairo_path_fixed_fill_to_polygon (path, tolerance, &info.polygon);
    if (unlikely (status))
	goto CLEANUP_POLYGON;

    if (extents.is_bounded) {
	cairo_rectangle_int_t rect;

	_cairo_box_round_to_rectangle (&info.polygon.extents, &rect);
	if (! _cairo_rectangle_intersect (&extents.bounded, &rect))
	    goto CLEANUP_POLYGON;
    }

    if (info.polygon.num_edges == 0) {
	if (! extents.is_bounded)
	    status = i915_fixup_unbounded (dst, &extents, clip);

	goto CLEANUP_POLYGON;
    }

    info.fill_rule = fill_rule;
    info.antialias = antialias;
    status = i915_clip_and_composite_spans (dst, op, source, antialias,
					    _composite_polygon_spans, &info,
					    &extents, clip, opacity);

CLEANUP_POLYGON:
    _cairo_polygon_fini (&info.polygon);

CLEANUP_BOXES:
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_int_status_t
i915_surface_paint_with_alpha (void			*abstract_dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*source,
			       cairo_clip_t		*clip,
			       double			 opacity)
{
    i915_surface_t *dst = abstract_dst;
    cairo_composite_rectangles_t extents;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_clip_path_t *clip_path;
    cairo_boxes_t boxes;
    int num_boxes = ARRAY_LENGTH (boxes.boxes_embedded);
    cairo_box_t *clip_boxes = boxes.boxes_embedded;
    cairo_status_t status;

    status = _cairo_composite_rectangles_init_for_paint (&extents,
							 dst->intel.drm.width,
							 dst->intel.drm.height,
							 op, source,
							 clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    /* If the clip cannot be reduced to a set of boxes, we will need to
     * use a clipmask. Paint is special as it is the only operation that
     * does not implicitly use a mask, so we may be able to reduce this
     * operation to a fill...
     */
    if (clip != NULL &&
	extents.is_bounded &&
	(clip_path = _clip_get_solitary_path (clip)) != NULL)
    {
	status = i915_surface_fill_with_alpha (dst, op, source,
					       &clip_path->path,
					       clip_path->fill_rule,
					       clip_path->tolerance,
					       clip_path->antialias,
					       NULL, opacity);
    }
    else
    {
	_cairo_boxes_init_for_array (&boxes, clip_boxes, num_boxes);
	status = _clip_and_composite_boxes (dst, op, source,
					    &boxes, CAIRO_ANTIALIAS_DEFAULT,
					    &extents, clip, opacity);
    }
    if (clip_boxes != boxes.boxes_embedded)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_int_status_t
i915_surface_paint (void			*abstract_dst,
		    cairo_operator_t		 op,
		    const cairo_pattern_t	*source,
		    cairo_clip_t		*clip)
{
    i915_surface_t *dst = abstract_dst;

    /* XXX unsupported operators? use pixel shader blending, eventually */

    if (op == CAIRO_OPERATOR_CLEAR && clip == NULL) {
	dst->deferred_clear = TRUE;
	return CAIRO_STATUS_SUCCESS;
    }

    return i915_surface_paint_with_alpha (dst, op, source, clip, 1.);
}

static cairo_int_status_t
i915_surface_mask (void				*abstract_dst,
		   cairo_operator_t		 op,
		   const cairo_pattern_t	*source,
		   const cairo_pattern_t	*mask,
		   cairo_clip_t			*clip)
{
    i915_surface_t *dst = abstract_dst;
    i915_device_t *device;
    cairo_composite_rectangles_t extents;
    i915_shader_t shader;
    cairo_clip_t local_clip;
    cairo_region_t *clip_region = NULL;
    cairo_bool_t need_clip_surface = FALSE;
    cairo_bool_t have_clip = FALSE;
    cairo_status_t status;

    if (mask->type == CAIRO_PATTERN_TYPE_SOLID) {
	const cairo_solid_pattern_t *solid = (cairo_solid_pattern_t *) mask;
	return i915_surface_paint_with_alpha (dst, op, source, clip, solid->color.alpha);
    }

    status = _cairo_composite_rectangles_init_for_mask (&extents,
							dst->intel.drm.width,
							dst->intel.drm.height,
							op, source, mask, clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL && extents.is_bounded) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	status = _cairo_clip_rectangle (clip, &extents.bounded);
	if (unlikely (status)) {
	    _cairo_clip_fini (&local_clip);
	    return status;
	}

	have_clip = TRUE;
    }

    i915_shader_init (&shader, dst, op, 1.);

    status = i915_shader_acquire_pattern (&shader,
					  &shader.source,
					  source,
					  &extents.bounded);
    if (unlikely (status))
	goto err_shader;

    status = i915_shader_acquire_pattern (&shader,
					  &shader.mask,
					  mask,
					  &extents.bounded);
    if (unlikely (status))
	goto err_shader;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	if (unlikely (_cairo_status_is_error (status) ||
		      status == CAIRO_INT_STATUS_NOTHING_TO_DO))
	{
	    goto err_shader;
	}

	need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
	if (need_clip_surface)
	    i915_shader_set_clip (&shader, clip);

	if (clip_region != NULL) {
	    cairo_rectangle_int_t rect;
	    cairo_bool_t is_empty;

	    status = CAIRO_STATUS_SUCCESS;
	    cairo_region_get_extents (clip_region, &rect);
	    is_empty = ! _cairo_rectangle_intersect (&extents.unbounded, &rect);
	    if (unlikely (is_empty))
		goto err_shader;

	    is_empty = ! _cairo_rectangle_intersect (&extents.bounded, &rect);
	    if (unlikely (is_empty && extents.is_bounded))
		goto err_shader;

	    if (cairo_region_num_rectangles (clip_region) == 1)
		clip_region = NULL;
	}
    }

    if (i915_surface_needs_tiling (dst)) {
	ASSERT_NOT_REACHED;
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    device = i915_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	goto err_shader;

    if (dst->deferred_clear) {
	status = i915_surface_clear (dst);
	if (unlikely (status))
	    goto err_shader;
    }

    status = i915_shader_commit (&shader, device);
    if (unlikely (status))
	goto err_device;

    if (clip_region != NULL) {
	unsigned int n, num_rectangles;

	num_rectangles = cairo_region_num_rectangles (clip_region);
	for (n = 0; n < num_rectangles; n++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, n, &rect);

	    shader.add_rectangle (&shader,
				  rect.x, rect.y,
				  rect.x + rect.width, rect.y + rect.height);
	}
    } else {
	shader.add_rectangle (&shader,
			      extents.bounded.x, extents.bounded.y,
			      extents.bounded.x + extents.bounded.width,
			      extents.bounded.y + extents.bounded.height);
    }

    if (! extents.is_bounded)
	status = i915_fixup_unbounded (dst, &extents, clip);

  err_device:
    cairo_device_release (&device->intel.base.base);
  err_shader:
    i915_shader_fini (&shader);
    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_int_status_t
i915_surface_stroke (void			*abstract_dst,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*source,
		     cairo_path_fixed_t		*path,
		     const cairo_stroke_style_t	*stroke_style,
		     const cairo_matrix_t	*ctm,
		     const cairo_matrix_t	*ctm_inverse,
		     double			 tolerance,
		     cairo_antialias_t		 antialias,
		     cairo_clip_t		*clip)
{
    i915_surface_t *dst = abstract_dst;
    cairo_composite_rectangles_t extents;
    composite_polygon_info_t info;
    cairo_box_t boxes_stack[32], *clip_boxes = boxes_stack;
    int num_boxes = ARRAY_LENGTH (boxes_stack);
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    cairo_status_t status;

    status = _cairo_composite_rectangles_init_for_stroke (&extents,
							  dst->intel.drm.width,
							  dst->intel.drm.height,
							  op, source,
							  path, stroke_style, ctm,
							  clip);
    if (unlikely (status))
	return status;

    if (_cairo_clip_contains_extents (clip, &extents))
	clip = NULL;

    if (clip != NULL) {
	clip = _cairo_clip_init_copy (&local_clip, clip);
	have_clip = TRUE;
    }

    status = _cairo_clip_to_boxes (&clip, &extents, &clip_boxes, &num_boxes);
    if (unlikely (status)) {
	if (have_clip)
	    _cairo_clip_fini (&local_clip);

	return status;
    }

    if (_cairo_path_fixed_stroke_is_rectilinear (path)) {
	cairo_boxes_t boxes;

	_cairo_boxes_init (&boxes);
	_cairo_boxes_limit (&boxes, clip_boxes, num_boxes);
	status = _cairo_path_fixed_stroke_rectilinear_to_boxes (path,
								stroke_style,
								ctm,
								&boxes);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    status = _clip_and_composite_boxes (dst, op, source,
						&boxes, antialias,
						&extents, clip, 1.);
	}

	_cairo_boxes_fini (&boxes);

	if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	    goto CLEANUP_BOXES;
    }

    _cairo_polygon_init (&info.polygon, clip_boxes, num_boxes);

    status = _cairo_path_fixed_stroke_to_polygon (path,
						  stroke_style,
						  ctm, ctm_inverse,
						  tolerance,
						  &info.polygon);
    if (unlikely (status))
	goto CLEANUP_POLYGON;

    if (extents.is_bounded) {
	cairo_rectangle_int_t rect;

	_cairo_box_round_to_rectangle (&info.polygon.extents, &rect);
	if (! _cairo_rectangle_intersect (&extents.bounded, &rect))
	    goto CLEANUP_POLYGON;
    }

    if (info.polygon.num_edges == 0) {
	if (! extents.is_bounded)
	    status = i915_fixup_unbounded (dst, &extents, clip);

	goto CLEANUP_POLYGON;
    }

    info.fill_rule = CAIRO_FILL_RULE_WINDING;
    info.antialias = antialias;
    status = i915_clip_and_composite_spans (dst, op, source, antialias,
					    _composite_polygon_spans, &info,
					    &extents, clip, 1.);

CLEANUP_POLYGON:
    _cairo_polygon_fini (&info.polygon);

CLEANUP_BOXES:
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_int_status_t
i915_surface_fill (void			*abstract_dst,
		   cairo_operator_t	 op,
		   const cairo_pattern_t*source,
		   cairo_path_fixed_t	*path,
		   cairo_fill_rule_t	 fill_rule,
		   double		 tolerance,
		   cairo_antialias_t	 antialias,
		   cairo_clip_t		*clip)
{
    return i915_surface_fill_with_alpha (abstract_dst, op, source, path, fill_rule, tolerance, antialias, clip, 1.);
}

static const cairo_surface_backend_t i915_surface_backend = {
    CAIRO_SURFACE_TYPE_DRM,
    _cairo_default_context_create,

    i915_surface_create_similar,
    i915_surface_finish,

    NULL,
    intel_surface_acquire_source_image,
    intel_surface_release_source_image,

    NULL, NULL, NULL,
    NULL, /* composite */
    NULL, /* fill */
    NULL, /* trapezoids */
    NULL, /* span */
    NULL, /* check-span */

    NULL, /* copy_page */
    NULL, /* show_page */
    _cairo_drm_surface_get_extents,
    NULL, /* old-glyphs */
    _cairo_drm_surface_get_font_options,

    i915_surface_flush,
    NULL, /* mark_dirty */
    intel_scaled_font_fini,
    intel_scaled_glyph_fini,

    i915_surface_paint,
    i915_surface_mask,
    i915_surface_stroke,
    i915_surface_fill,
    i915_surface_glyphs,
};

static void
i915_surface_init (i915_surface_t *surface,
		   cairo_drm_device_t *device,
		   cairo_format_t format,
		   int width, int height)
{
    intel_surface_init (&surface->intel, &i915_surface_backend, device,
			format, width, height);

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	ASSERT_NOT_REACHED;
    case CAIRO_FORMAT_ARGB32:
	surface->map0 = MAPSURF_32BIT | MT_32BIT_ARGB8888;
	surface->colorbuf = COLR_BUF_ARGB8888 | DEPTH_FRMT_24_FIXED_8_OTHER;
	break;
    case CAIRO_FORMAT_RGB24:
	surface->map0 = MAPSURF_32BIT | MT_32BIT_XRGB8888;
	surface->colorbuf = COLR_BUF_ARGB8888 | DEPTH_FRMT_24_FIXED_8_OTHER;
	break;
    case CAIRO_FORMAT_RGB16_565:
	surface->map0 = MAPSURF_16BIT | MT_16BIT_RGB565;
	surface->colorbuf = COLR_BUF_RGB565;
	break;
    case CAIRO_FORMAT_A8:
	surface->map0 = MAPSURF_8BIT | MT_8BIT_A8;
	surface->colorbuf = COLR_BUF_8BIT | DEPTH_FRMT_24_FIXED_8_OTHER;
	break;
    }
    surface->colorbuf |= DSTORG_HORT_BIAS (0x8) | DSTORG_VERT_BIAS (0x8);
    surface->map0 |= ((height - 1) << MS3_HEIGHT_SHIFT) |
		     ((width  - 1) << MS3_WIDTH_SHIFT);
    surface->map1 = 0;

    surface->is_current_texture = 0;
    surface->deferred_clear = FALSE;

    surface->offset = 0;

    surface->stencil  = NULL;
    surface->cache = NULL;
}

cairo_surface_t *
i915_surface_create_internal (cairo_drm_device_t *base_dev,
		              cairo_format_t format,
			      int width, int height,
			      uint32_t tiling,
			      cairo_bool_t gpu_target)
{
    i915_surface_t *surface;
    cairo_status_t status_ignored;

    surface = _cairo_malloc (sizeof (i915_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    i915_surface_init (surface, base_dev, format, width, height);

    if (width && height) {
	uint32_t size, stride;
	intel_bo_t *bo;

	width = (width + 3) & -4;
	stride = cairo_format_stride_for_width (surface->intel.drm.format, width);
	/* check for tiny surfaces for which tiling is irrelevant */
	if (height * stride <= 4096)
	    tiling = I915_TILING_NONE;
	if (tiling != I915_TILING_NONE && stride <= 512)
	    tiling = I915_TILING_NONE;
	if (tiling != I915_TILING_NONE) {
	    if (height <= 8)
		tiling = I915_TILING_NONE;
	    else if (height <= 16)
		tiling = I915_TILING_X;
	}
	/* large surfaces we need to blt, so force TILING_X */
	if (height > 2048)
	    tiling = I915_TILING_X;
	/* but there is a maximum limit to the tiling pitch */
	if (tiling != I915_TILING_NONE && stride > 8192)
	    tiling = I915_TILING_NONE;

	stride = i915_tiling_stride (tiling, stride);
	assert (stride >= (uint32_t) cairo_format_stride_for_width (surface->intel.drm.format, width));
	assert (tiling == I915_TILING_NONE || stride <= 8192);
	height = i915_tiling_height (tiling, height);
	if (height > 64*1024) {
	    free (surface);
	    cairo_device_destroy (&base_dev->base);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_SIZE));
	}

	size = stride * height;
	bo = intel_bo_create (to_intel_device (&base_dev->base),
			      i915_tiling_size (tiling, size), size,
			      gpu_target, tiling, stride);
	if (bo == NULL) {
	    status_ignored = _cairo_drm_surface_finish (&surface->intel.drm);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
	assert (bo->base.size >= size);

	surface->intel.drm.bo = &bo->base;
	surface->intel.drm.stride = stride;

	surface->map0 |= MS3_tiling (tiling);
	surface->map1 = (stride/4 - 1) << MS4_PITCH_SHIFT;
    }

    return &surface->intel.drm.base;
}

static cairo_surface_t *
i915_surface_create (cairo_drm_device_t *base_dev,
		     cairo_format_t format,
		     int width, int height)
{
    switch (format) {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_A8:
	break;
    case CAIRO_FORMAT_INVALID:
    default:
    case CAIRO_FORMAT_A1:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    }

    return i915_surface_create_internal (base_dev, format, width, height,
	                                 I915_TILING_DEFAULT, TRUE);
}

static cairo_surface_t *
i915_surface_create_for_name (cairo_drm_device_t *base_dev,
			      unsigned int name,
			      cairo_format_t format,
			      int width, int height, int stride)
{
    i915_surface_t *surface;

    /* Vol I, p134: size restrictions for textures */
    /* Vol I, p129: destination surface stride must be a multiple of 32 bytes */
    if (stride < cairo_format_stride_for_width (format, (width + 3) & -4) ||
	stride & 31)
    {
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));
    }

    switch (format) {
    default:
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB16_565:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_A8:
	break;
    }

    surface = _cairo_malloc (sizeof (i915_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    i915_surface_init (surface, base_dev, format, width, height);

    if (width && height) {
	surface->intel.drm.stride = stride;
	surface->map1 = (surface->intel.drm.stride/4 - 1) << MS4_PITCH_SHIFT;

	surface->intel.drm.bo =
	    &intel_bo_create_for_name (to_intel_device (&base_dev->base),
				       name)->base;
	if (unlikely (surface->intel.drm.bo == NULL)) {
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}
	to_intel_bo (surface->intel.drm.bo)->stride = stride;

	surface->map0 |= MS3_tiling (to_intel_bo (surface->intel.drm.bo)->tiling);
    }

    return &surface->intel.drm.base;
}

static cairo_status_t
i915_buffer_cache_init (intel_buffer_cache_t *cache,
		        i915_device_t *device,
			cairo_format_t format,
			int width, int height)
{
    const uint32_t tiling = I915_TILING_DEFAULT;
    uint32_t stride, size;

    assert ((width & 3) == 0);
    assert ((height & 1) == 0);
    cache->buffer.width = width;
    cache->buffer.height = height;

    switch (format) {
    case CAIRO_FORMAT_INVALID:
    case CAIRO_FORMAT_A1:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_RGB16_565:
	ASSERT_NOT_REACHED;
    case CAIRO_FORMAT_ARGB32:
	cache->buffer.map0 = MAPSURF_32BIT | MT_32BIT_ARGB8888;
	stride = width * 4;
	break;
    case CAIRO_FORMAT_A8:
	cache->buffer.map0 = MAPSURF_8BIT | MT_8BIT_I8;
	stride = width;
	break;
    }
    assert ((stride & 7) == 0);
    assert (i915_tiling_stride (tiling, stride) == stride);
    assert (i915_tiling_height (tiling, height) == height);

    size = height * stride;
    assert (i915_tiling_size (tiling, size) == size);
    cache->buffer.bo = intel_bo_create (&device->intel, size, size, FALSE, tiling, stride);
    if (unlikely (cache->buffer.bo == NULL))
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);

    cache->buffer.stride = cache->buffer.bo->stride;

    cache->buffer.map0 |= ((height - 1) << MS3_HEIGHT_SHIFT) |
			  ((width  - 1) << MS3_WIDTH_SHIFT);
    cache->buffer.map0 |= MS3_tiling (tiling);
    cache->buffer.map1 = ((stride / 4) - 1) << MS4_PITCH_SHIFT;

    cache->ref_count = 0;
    cairo_list_init (&cache->link);

    return CAIRO_STATUS_SUCCESS;
}

i915_surface_t *
i915_surface_create_from_cacheable_image_internal (i915_device_t *device,
						   cairo_image_surface_t *image)
{
    i915_surface_t *surface;
    cairo_status_t status;
    cairo_list_t *caches;
    intel_buffer_cache_t *cache;
    cairo_rtree_node_t *node;
    cairo_format_t format;
    int width, height, bpp;

    format = image->format;
    if (format == CAIRO_FORMAT_A1)
	format = CAIRO_FORMAT_A8;

    width = image->width;
    height = image->height;
    if (width > IMAGE_CACHE_WIDTH/2 || height > IMAGE_CACHE_HEIGHT/2) {
	surface = (i915_surface_t *)
	    i915_surface_create_internal (&device->intel.base,
					  format,
					  width, height,
					  I915_TILING_NONE, FALSE);
	if (unlikely (surface->intel.drm.base.status))
	    return surface;

	status = intel_bo_put_image (&device->intel,
				     to_intel_bo (surface->intel.drm.bo),
				     image,
				     0, 0,
				     width, height,
				     0, 0);

	if (unlikely (status)) {
	    cairo_surface_destroy (&surface->intel.drm.base);
	    return (i915_surface_t *) _cairo_surface_create_in_error (status);
	}

	return surface;
    }

    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	return (i915_surface_t *) _cairo_surface_create_in_error (status);

    switch (image->format) {
    case CAIRO_FORMAT_ARGB32:
    case CAIRO_FORMAT_RGB24:
    case CAIRO_FORMAT_RGB16_565:
	caches = &device->image_caches[0];
	format = CAIRO_FORMAT_ARGB32;
	bpp = 4;
	break;
    case CAIRO_FORMAT_A8:
    case CAIRO_FORMAT_A1:
	caches = &device->image_caches[1];
	format = CAIRO_FORMAT_A8;
	bpp = 1;
	break;
    case CAIRO_FORMAT_INVALID:
    default:
	ASSERT_NOT_REACHED;
	status = _cairo_error (CAIRO_STATUS_INVALID_FORMAT);
	goto CLEANUP_DEVICE;
    }

    node = NULL;
    cairo_list_foreach_entry (cache, intel_buffer_cache_t, caches, link) {
	if (! intel_bo_is_inactive (&device->intel, cache->buffer.bo))
	    continue;

	status = _cairo_rtree_insert (&cache->rtree, width, height, &node);
	if (unlikely (_cairo_status_is_error (status)))
	    goto CLEANUP_DEVICE;
	if (status == CAIRO_STATUS_SUCCESS)
	    break;
    }
    if (node == NULL) {
	cache = _cairo_malloc (sizeof (intel_buffer_cache_t));
	if (unlikely (cache == NULL)) {
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	    goto CLEANUP_DEVICE;
	}

	status = i915_buffer_cache_init (cache, device, format,
					 IMAGE_CACHE_WIDTH,
					 IMAGE_CACHE_HEIGHT);
	if (unlikely (status)) {
	    free (cache);
	    goto CLEANUP_DEVICE;
	}

	_cairo_rtree_init (&cache->rtree,
			   IMAGE_CACHE_WIDTH,
			   IMAGE_CACHE_HEIGHT,
			   4,
			   sizeof (i915_image_private_t));

	status = _cairo_rtree_insert (&cache->rtree, width, height, &node);
	assert (status == CAIRO_STATUS_SUCCESS);

	cairo_list_init (&cache->link);
    }
    cairo_list_move (&cache->link, caches);
    ((i915_image_private_t *) node)->container = cache;

    status = intel_bo_put_image (&device->intel,
				 cache->buffer.bo,
				 image,
				 0, 0,
				 width, height,
				 node->x, node->y);
    if (unlikely (status))
	goto CLEANUP_CACHE;

    surface = _cairo_malloc (sizeof (i915_surface_t));
    if (unlikely (surface == NULL)) {
	status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	goto CLEANUP_CACHE;
    }

    i915_surface_init (surface, &device->intel.base,
		       format, width, height);

    surface->intel.drm.stride = cache->buffer.stride;

    surface->map0 |= MS3_tiling (cache->buffer.bo->tiling);
    surface->map1 = (surface->intel.drm.stride/4 - 1) << MS4_PITCH_SHIFT;

    surface->intel.drm.bo = &intel_bo_reference (cache->buffer.bo)->base;
    surface->offset = node->y * cache->buffer.stride + bpp * node->x;

    surface->cache = (i915_image_private_t *) node;
    cache->ref_count++;

    cairo_device_release (&device->intel.base.base);

    return surface;

CLEANUP_CACHE:
    _cairo_rtree_node_destroy (&cache->rtree, node);
    if (cache->ref_count == 0) {
	intel_bo_destroy (&device->intel, cache->buffer.bo);
	_cairo_rtree_fini (&cache->rtree);
	cairo_list_del (&cache->link);
	free (cache);
    }
CLEANUP_DEVICE:
    cairo_device_release (&device->intel.base.base);
    return (i915_surface_t *) _cairo_surface_create_in_error (status);
}

static cairo_surface_t *
i915_surface_create_from_cacheable_image (cairo_drm_device_t *device,
					  cairo_surface_t *source)
{
    i915_surface_t *surface;
    cairo_image_surface_t *image;
    void *image_extra;
    cairo_status_t status;

    status = _cairo_surface_acquire_source_image (source, &image, &image_extra);
    if (unlikely (status))
	return _cairo_surface_create_in_error (status);

    surface = i915_surface_create_from_cacheable_image_internal ((i915_device_t *) device, image);

    _cairo_surface_release_source_image (source, image, image_extra);

    return &surface->intel.drm.base;
}

static cairo_status_t
i915_surface_enable_scan_out (void *abstract_surface)
{
    i915_surface_t *surface = abstract_surface;
    intel_bo_t *bo;
    cairo_status_t status;

    if (unlikely (surface->intel.drm.bo == NULL))
	return _cairo_error (CAIRO_STATUS_INVALID_SIZE);

    bo = to_intel_bo (surface->intel.drm.bo);
    if (bo->tiling == I915_TILING_Y) {
	status = i915_surface_batch_flush (surface);
	if (unlikely (status))
	    return status;

	bo->tiling = I915_TILING_X;
	surface->map0 &= ~MS3_tiling (I915_TILING_Y);
	surface->map0 |= MS3_tiling (I915_TILING_X);
    }


    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
i915_device_flush (cairo_drm_device_t *device)
{
    cairo_status_t status;

    if (unlikely (device->base.finished))
	return CAIRO_STATUS_SUCCESS;

    status = cairo_device_acquire (&device->base);
    if (likely (status == CAIRO_STATUS_SUCCESS)) {
	status = i915_batch_flush ((i915_device_t *) device);
	cairo_device_release (&device->base);
    }

    return status;
}

static cairo_int_status_t
i915_device_throttle (cairo_drm_device_t *device)
{
    cairo_status_t status;

    status = cairo_device_acquire (&device->base);
    if (unlikely (status))
	return status;

    status = i915_batch_flush ((i915_device_t *) device);
    intel_throttle ((intel_device_t *) device);

    cairo_device_release (&device->base);

    return status;
}

static void
i915_device_destroy (void *data)
{
    i915_device_t *device = data;

    if (device->last_vbo)
	intel_bo_destroy (&device->intel, device->last_vbo);

    i915_batch_cleanup (device);

    intel_device_fini (&device->intel);
    free (device);
}

COMPILE_TIME_ASSERT (sizeof (i915_batch_setup) == sizeof (((i915_device_t *)0)->batch_header));
COMPILE_TIME_ASSERT (offsetof (i915_device_t, batch_base) == offsetof (i915_device_t, batch_header) + sizeof (i915_batch_setup));

cairo_drm_device_t *
_cairo_drm_i915_device_create (int fd, dev_t dev_id, int vendor_id, int chip_id)
{
    i915_device_t *device;
    cairo_status_t status;
    uint64_t gtt_size;
    int n;

    if (! intel_info (fd, &gtt_size))
	return NULL;

    device = _cairo_malloc (sizeof (i915_device_t));
    if (device == NULL)
	return (cairo_drm_device_t *) _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = intel_device_init (&device->intel, fd);
    if (unlikely (status)) {
	free (device);
	return (cairo_drm_device_t *) _cairo_device_create_in_error (status);
    }

    device->debug = 0;
    if (getenv ("CAIRO_DEBUG_DRM") != NULL)
	device->debug = I915_DEBUG_SYNC;

    n = intel_get (fd, I915_PARAM_NUM_FENCES_AVAIL);
    if (n == 0)
	n = 8;
    device->batch.fences_avail = n - 2; /* conservative */

    device->batch.gtt_avail_size = device->intel.gtt_avail_size / 4;
    device->batch.est_gtt_size = I915_BATCH_SIZE;
    device->batch.total_gtt_size = I915_BATCH_SIZE;
    device->batch.exec_count = 0;
    device->batch.reloc_count = 0;
    device->batch.used = 0;
    device->batch.fences = 0;

    memcpy (device->batch_header, i915_batch_setup, sizeof (i915_batch_setup));
    device->vbo = 0;
    device->vbo_offset = 0;
    device->vbo_used = 0;
    device->vertex_index = 0;
    device->vertex_count = 0;
    device->last_vbo = NULL;

    for (n = 0; n < ARRAY_LENGTH (device->image_caches); n++)
	cairo_list_init (&device->image_caches[n]);

    device->intel.base.surface.create = i915_surface_create;
    device->intel.base.surface.create_for_name = i915_surface_create_for_name;
    device->intel.base.surface.create_from_cacheable_image = i915_surface_create_from_cacheable_image;

    device->intel.base.surface.flink = _cairo_drm_surface_flink;
    device->intel.base.surface.enable_scan_out = i915_surface_enable_scan_out;
    device->intel.base.surface.map_to_image = intel_surface_map_to_image;

    device->intel.base.device.flush = i915_device_flush;
    device->intel.base.device.throttle = i915_device_throttle;
    device->intel.base.device.destroy = i915_device_destroy;

    device->floats_per_vertex = 0;
    device->current_source = NULL;
    device->current_mask = NULL;
    device->current_clip = NULL;

    i915_device_reset (device);

    return _cairo_drm_device_init (&device->intel.base,
				   fd, dev_id, vendor_id, chip_id,
				   16*1024);
}

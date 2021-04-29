/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2009 Kristian Høgsberg
 * Copyright © 2009 Chris Wilson
 * Copyright © 2009 Intel Corporation
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
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Kristian Høgsberg.
 *
 * Based on the xf86-intel-driver i965 render acceleration code,
 * authored by:
 *    Wang Zhenyu <zhenyu.z.wang@intel.com>
 *    Eric Anholt <eric@anholt.net>
 *    Carl Worth <cworth@redhat.com>
 *    Keith Packard <keithp@keithp.com>
 */

/* XXX
 *
 * FIXME: Use brw_PLN for [DevCTG-B+]
 *
 */

#include "cairoint.h"

#include "cairo-drm-private.h"
#include "cairo-drm-intel-private.h"
#include "cairo-drm-intel-command-private.h"
#include "cairo-drm-intel-ioctl-private.h"
#include "cairo-drm-i965-private.h"

#include "cairo-boxes-private.h"
#include "cairo-composite-rectangles-private.h"
#include "cairo-default-context-private.h"
#include "cairo-error-private.h"
#include "cairo-region-private.h"
#include "cairo-surface-offset-private.h"

#include <sys/ioctl.h>
#include <errno.h>

#define I965_MAX_SIZE 8192

static const cairo_surface_backend_t i965_surface_backend;

static void
i965_stream_init (i965_stream_t *stream,
		  uint8_t *data, uint32_t size,
		  struct i965_pending_relocation *pending, int max_pending,
		  struct drm_i915_gem_relocation_entry *relocations, int max_relocations)

{
    stream->used = stream->committed = 0;
    stream->data = data;
    stream->size = size;
    stream->serial = 1;

    stream->num_pending_relocations = 0;
    stream->max_pending_relocations = max_pending;
    stream->pending_relocations = pending;

    stream->num_relocations = 0;
    stream->max_relocations = max_relocations;
    stream->relocations = relocations;
}

static void
i965_add_relocation (i965_device_t *device,
		     intel_bo_t *bo,
		     uint32_t read_domains,
		     uint32_t write_domain)
{
    if (bo->exec == NULL) {
	int i;

	device->exec.gtt_size += bo->base.size;

	i = device->exec.count++;
	assert (i < ARRAY_LENGTH (device->exec.exec));

	device->exec.exec[i].handle = bo->base.handle;
	device->exec.exec[i].relocation_count = 0;
	device->exec.exec[i].relocs_ptr = 0;
	device->exec.exec[i].alignment  = 0;
	device->exec.exec[i].offset = 0;
	device->exec.exec[i].flags  = 0;
	device->exec.exec[i].rsvd1  = 0;
	device->exec.exec[i].rsvd2  = 0;

	device->exec.bo[i] = intel_bo_reference (bo);
	bo->exec = &device->exec.exec[i];
    }

    if (cairo_list_is_empty (&bo->link))
	cairo_list_add_tail (&device->flush, &bo->link);

    assert (write_domain == 0 || bo->batch_write_domain == 0 || bo->batch_write_domain == write_domain);
    bo->batch_read_domains |= read_domains;
    bo->batch_write_domain |= write_domain;
}

void
i965_emit_relocation (i965_device_t *device,
		      i965_stream_t *stream,
		      intel_bo_t *target,
		      uint32_t target_offset,
		      uint32_t read_domains,
		      uint32_t write_domain,
		      uint32_t offset)
{
    int n;

    assert (target_offset < target->base.size);

    i965_add_relocation (device, target, read_domains, write_domain);

    n = stream->num_relocations++;
    assert (n < stream->max_relocations);

    stream->relocations[n].offset = offset;
    stream->relocations[n].delta  = target_offset;
    stream->relocations[n].target_handle   = target->base.handle;
    stream->relocations[n].read_domains    = read_domains;
    stream->relocations[n].write_domain    = write_domain;
    stream->relocations[n].presumed_offset = target->offset;
}

static void
i965_stream_reset (i965_stream_t *stream)
{
    stream->used = stream->committed = 0;
    stream->num_relocations = 0;
    stream->num_pending_relocations = 0;
    if (++stream->serial == 0)
	stream->serial = 1;
}

void
i965_stream_commit (i965_device_t *device,
		    i965_stream_t *stream)
{
    intel_bo_t *bo;
    int n;

    assert (stream->used);

    bo = intel_bo_create (&device->intel,
			  stream->used, stream->used,
			  FALSE, I915_TILING_NONE, 0);

    /* apply pending relocations */
    for (n = 0; n < stream->num_pending_relocations; n++) {
	struct i965_pending_relocation *p = &stream->pending_relocations[n];

	i965_emit_relocation (device, &device->batch, bo,
			      p->delta,
			      p->read_domains,
			      p->write_domain,
			      p->offset);
	if (bo->offset)
	    *(uint32_t *) (device->batch.data + p->offset) = bo->offset + p->delta;
    }

    intel_bo_write (&device->intel, bo, 0, stream->used, stream->data);

    if (stream->num_relocations) {
	assert (bo->exec != NULL);
	bo->exec->relocs_ptr = (uintptr_t) stream->relocations;
	bo->exec->relocation_count = stream->num_relocations;
    }

    intel_bo_destroy (&device->intel, bo);

    i965_stream_reset (stream);
}

static void
sf_states_pluck (void *entry, void *closure)
{
    i965_device_t *device = closure;

    _cairo_hash_table_remove (device->sf_states, entry);
    _cairo_freelist_free (&device->sf_freelist, entry);
}

static void
cc_offsets_pluck (void *entry, void *closure)
{
    i965_device_t *device = closure;

    _cairo_hash_table_remove (device->cc_states, entry);
    _cairo_freelist_free (&device->cc_freelist, entry);
}

static void
wm_kernels_pluck (void *entry, void *closure)
{
    i965_device_t *device = closure;

    _cairo_hash_table_remove (device->wm_kernels, entry);
    _cairo_freelist_free (&device->wm_kernel_freelist, entry);
}

static void
wm_states_pluck (void *entry, void *closure)
{
    i965_device_t *device = closure;

    _cairo_hash_table_remove (device->wm_states, entry);
    _cairo_freelist_free (&device->wm_state_freelist, entry);
}

static void
wm_bindings_pluck (void *entry, void *closure)
{
    i965_device_t *device = closure;

    _cairo_hash_table_remove (device->wm_bindings, entry);
    _cairo_freelist_free (&device->wm_binding_freelist, entry);
}

static void
samplers_pluck (void *entry, void *closure)
{
    i965_device_t *device = closure;

    _cairo_hash_table_remove (device->samplers, entry);
    _cairo_freelist_free (&device->sampler_freelist, entry);
}

void
i965_general_state_reset (i965_device_t *device)
{
    _cairo_hash_table_foreach (device->sf_states,
			       sf_states_pluck,
			       device);

    _cairo_hash_table_foreach (device->cc_states,
			       cc_offsets_pluck,
			       device);

    _cairo_hash_table_foreach (device->wm_kernels,
			       wm_kernels_pluck,
			       device);

    _cairo_hash_table_foreach (device->wm_states,
			       wm_states_pluck,
			       device);

    _cairo_hash_table_foreach (device->wm_bindings,
			       wm_bindings_pluck,
			       device);

    _cairo_hash_table_foreach (device->samplers,
			       samplers_pluck,
			       device);

    device->vs_offset = (uint32_t) -1;
    device->border_color_offset = (uint32_t) -1;

    if (device->general_state != NULL) {
	intel_bo_destroy (&device->intel, device->general_state);
	device->general_state = NULL;
    }
}

static void
i965_device_reset (i965_device_t *device)
{
    device->exec.count = 0;
    device->exec.gtt_size = I965_VERTEX_SIZE +
	                    I965_SURFACE_SIZE +
			    I965_GENERAL_SIZE +
			    I965_BATCH_SIZE;

    device->sf_state.entry.hash = (uint32_t) -1;
    device->wm_state.entry.hash = (uint32_t) -1;
    device->wm_binding.entry.hash = (uint32_t) -1;
    device->cc_state.entry.hash = (uint32_t) -1;

    device->target = NULL;
    device->source = NULL;
    device->mask = NULL;
    device->clip = NULL;

    device->draw_rectangle = (uint32_t) -1;

    device->vertex_type = (uint32_t) -1;
    device->vertex_size = 0;
    device->rectangle_size   = 0;
    device->last_vertex_size = 0;

    device->constants = NULL;
    device->constants_size = 0;

    device->have_urb_fences = FALSE;
}

static cairo_status_t
i965_exec (i965_device_t *device, uint32_t offset)
{
    struct drm_i915_gem_execbuffer2 execbuf;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;
    int ret, i;

    execbuf.buffers_ptr = (uintptr_t) device->exec.exec;
    execbuf.buffer_count = device->exec.count;
    execbuf.batch_start_offset = offset;
    execbuf.batch_len = device->batch.used;
    execbuf.DR1 = 0;
    execbuf.DR4 = 0;
    execbuf.num_cliprects = 0;
    execbuf.cliprects_ptr = 0;
    execbuf.flags = I915_GEM_3D_PIPELINE;
    execbuf.rsvd1 = 0;
    execbuf.rsvd2 = 0;

#if 0
    printf ("exec: offset=%d, length=%d, buffers=%d\n",
	    offset, device->batch.used, device->exec.count);
    intel_dump_batchbuffer ((uint32_t *) device->batch.data,
			    device->batch.used,
			    device->intel.base.chip_id);
#endif

    ret = 0;
    do {
	ret = ioctl (device->intel.base.fd, DRM_IOCTL_I915_GEM_EXECBUFFER2, &execbuf);
    } while (ret != 0 && errno == EINTR);
    if (unlikely (ret)) {
	if (errno == ENOMEM)
	    status = _cairo_error (CAIRO_STATUS_NO_MEMORY);
	else
	    status = _cairo_error (CAIRO_STATUS_DEVICE_ERROR);

	fprintf (stderr, "Batch submission failed: %d\n", errno);
	fprintf (stderr, "   gtt size: %zd/%zd\n",
		 device->exec.gtt_size, device->intel.gtt_avail_size);

	fprintf (stderr, "   %d buffers:\n",
		 device->exec.count);
	for (i = 0; i < device->exec.count; i++) {
	    fprintf (stderr, "     exec[%d] = %d\n",
		     i, device->exec.bo[i]->base.size);
	}

	intel_dump_batchbuffer ((uint32_t *) device->batch.data,
				device->batch.used,
				device->intel.base.chip_id);
    }

    /* XXX any write target within the batch should now be in error */
    for (i = 0; i < device->exec.count; i++) {
	intel_bo_t *bo = device->exec.bo[i];
	cairo_bool_t ret;

	bo->offset = device->exec.exec[i].offset;
	bo->exec = NULL;
	bo->batch_read_domains = 0;
	bo->batch_write_domain = 0;

	if (bo->virtual)
	    intel_bo_unmap (bo);
	bo->cpu = FALSE;

	if (bo->purgeable)
	    ret = intel_bo_madvise (&device->intel, bo, I915_MADV_DONTNEED);
	    /* ignore immediate notification of purging */

	cairo_list_del (&bo->cache_list);
	cairo_list_init (&bo->link);
	intel_bo_destroy (&device->intel, bo);
    }
    cairo_list_init (&device->flush);

    device->exec.count = 0;

    return status;
}

static inline uint32_t
next_bo_size (uint32_t v)
{
    v = (v + 8191) / 8192;

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v * 8192;
}

static void
_copy_to_bo_and_apply_relocations (i965_device_t *device,
				   intel_bo_t *bo,
				   i965_stream_t *stream,
				   uint32_t offset)
{
    int n;

    intel_bo_write (&device->intel, bo,
		    offset, stream->used,
		    stream->data);

    for (n = 0; n < stream->num_pending_relocations; n++) {
	struct i965_pending_relocation *p = &stream->pending_relocations[n];

	i965_emit_relocation (device, &device->batch, bo,
			      p->delta + offset,
			      p->read_domains,
			      p->write_domain,
			      p->offset);

	if (bo->offset) {
	    *(uint32_t *) (device->batch.data + p->offset) =
		bo->offset + p->delta + offset;
	}
    }
}

cairo_status_t
i965_device_flush (i965_device_t *device)
{
    cairo_status_t status;
    uint32_t aligned, max;
    intel_bo_t *bo;
    int n;

    if (device->batch.used == 0)
	return CAIRO_STATUS_SUCCESS;

    i965_flush_vertices (device);

    OUT_BATCH (MI_BATCH_BUFFER_END);
    /* Emit a padding dword if we aren't going to be quad-word aligned. */
    if (device->batch.used & 4)
	OUT_BATCH (MI_NOOP);

#if 0
    printf ("device flush: vertex=%d, constant=%d, surface=%d, general=%d, batch=%d\n",
	    device->vertex.used,
	    device->constant.used,
	    device->surface.used,
	    device->general.used,
	    device->batch.used);
#endif

    /* can we pack the surface state into the tail of the general state? */
    if (device->general.used == device->general.committed) {
	if (device->general.used) {
	    assert (device->general.num_pending_relocations == 1);
	    assert (device->general_state != NULL);
	    i965_emit_relocation (device, &device->batch,
				  device->general_state,
				  device->general.pending_relocations[0].delta,
				  device->general.pending_relocations[0].read_domains,
				  device->general.pending_relocations[0].write_domain,
				  device->general.pending_relocations[0].offset);

	    if (device->general_state->offset) {
		*(uint32_t *) (device->batch.data +
			       device->general.pending_relocations[0].offset) =
		    device->general_state->offset +
		    device->general.pending_relocations[0].delta;
	    }
	}
    } else {
	assert (device->general.num_pending_relocations == 1);
	if (device->general_state != NULL) {
	    intel_bo_destroy (&device->intel, device->general_state);
	    device->general_state = NULL;
	}

	bo = intel_bo_create (&device->intel,
			      device->general.used,
			      device->general.used,
			      FALSE, I915_TILING_NONE, 0);
	if (unlikely (bo == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	aligned = (device->general.used + 31) & -32;
	if (device->surface.used &&
	    aligned + device->surface.used <= bo->base.size)
	{
	    _copy_to_bo_and_apply_relocations (device, bo, &device->general, 0);
	    _copy_to_bo_and_apply_relocations (device, bo, &device->surface, aligned);

	    if (device->surface.num_relocations) {
		for (n = 0; n < device->surface.num_relocations; n++)
		    device->surface.relocations[n].offset += aligned;

		assert (bo->exec != NULL);
		bo->exec->relocs_ptr = (uintptr_t) device->surface.relocations;
		bo->exec->relocation_count = device->surface.num_relocations;
	    }

	    i965_stream_reset (&device->surface);
	}
	else
	{
	    _copy_to_bo_and_apply_relocations (device, bo, &device->general, 0);
	}

	/* Note we don't reset the general state, just mark what data we've committed. */
	device->general.committed = device->general.used;
	device->general_state = bo;
    }
    device->general.num_pending_relocations = 0;

    /* Combine vertex+constant+surface+batch streams? */
    max = aligned = device->vertex.used;
    if (device->surface.used) {
	aligned = (aligned + 63) & -64;
	aligned += device->surface.used;
	if (device->surface.used > max)
	    max = device->surface.used;
    }
    aligned = (aligned + 63) & -64;
    aligned += device->batch.used;
    if (device->batch.used > max)
	max = device->batch.used;
    if (aligned <= next_bo_size (max)) {
	int batch_num_relocations;

	if (aligned <= 8192)
	    max = aligned;

	bo = intel_bo_create (&device->intel,
			      max, max,
			      FALSE, I915_TILING_NONE, 0);
	if (unlikely (bo == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	assert (aligned <= bo->base.size);

	if (device->vertex.used)
	    _copy_to_bo_and_apply_relocations (device, bo, &device->vertex, 0);

	aligned = device->vertex.used;

	batch_num_relocations = device->batch.num_relocations;
	if (device->surface.used) {
	    aligned = (aligned + 63) & -64;
	    _copy_to_bo_and_apply_relocations (device, bo, &device->surface, aligned);

	    batch_num_relocations = device->batch.num_relocations;
	    if (device->surface.num_relocations) {
		assert (device->batch.num_relocations + device->surface.num_relocations < device->batch.max_relocations);

		memcpy (device->batch.relocations + device->batch.num_relocations,
			device->surface.relocations,
			sizeof (device->surface.relocations[0]) * device->surface.num_relocations);

		for (n = 0; n < device->surface.num_relocations; n++)
		    device->batch.relocations[device->batch.num_relocations + n].offset += aligned;

		device->batch.num_relocations += device->surface.num_relocations;
	    }

	    aligned += device->surface.used;
	}

	aligned = (aligned + 63) & -64;
	intel_bo_write (&device->intel, bo,
			aligned, device->batch.used,
			device->batch.data);

	for (n = 0; n < batch_num_relocations; n++)
	    device->batch.relocations[n].offset += aligned;

	if (device->exec.bo[device->exec.count-1] == bo) {
	    assert (bo->exec == &device->exec.exec[device->exec.count-1]);

	    bo->exec->relocation_count = device->batch.num_relocations;
	    bo->exec->relocs_ptr = (uintptr_t) device->batch.relocations;
	    intel_bo_destroy (&device->intel, bo);
	} else {
	    assert (bo->exec ==  NULL);

	    n = device->exec.count++;
	    device->exec.exec[n].handle = bo->base.handle;
	    device->exec.exec[n].relocation_count = device->batch.num_relocations;
	    device->exec.exec[n].relocs_ptr = (uintptr_t) device->batch.relocations;
	    device->exec.exec[n].alignment = 0;
	    device->exec.exec[n].offset = 0;
	    device->exec.exec[n].flags = 0;
	    device->exec.exec[n].rsvd1 = 0;
	    device->exec.exec[n].rsvd2 = 0;

	    /* transfer ownership to the exec */
	    device->exec.bo[n] = bo;
	}
    } else {
	i965_stream_commit (device, &device->vertex);
	if (device->surface.used)
	    i965_stream_commit (device, &device->surface);

	bo = intel_bo_create (&device->intel,
			      device->batch.used, device->batch.used,
			      FALSE, I915_TILING_NONE, 0);
	if (unlikely (bo == NULL))
	    return _cairo_error (CAIRO_STATUS_NO_MEMORY);

	intel_bo_write (&device->intel, bo,
			0, device->batch.used,
			device->batch.data);

	n = device->exec.count++;
	device->exec.exec[n].handle = bo->base.handle;
	device->exec.exec[n].relocation_count = device->batch.num_relocations;
	device->exec.exec[n].relocs_ptr = (uintptr_t) device->batch.relocations;
	device->exec.exec[n].alignment = 0;
	device->exec.exec[n].offset = 0;
	device->exec.exec[n].flags = 0;
	device->exec.exec[n].rsvd1 = 0;
	device->exec.exec[n].rsvd2 = 0;

	/* transfer ownership to the exec */
	device->exec.bo[n] = bo;
	aligned = 0;
    }

    status = i965_exec (device, aligned);

    i965_stream_reset (&device->vertex);
    i965_stream_reset (&device->surface);
    i965_stream_reset (&device->batch);

    intel_glyph_cache_unpin (&device->intel);
    intel_snapshot_cache_thaw (&device->intel);

    i965_device_reset (device);

    return status;
}

static cairo_surface_t *
i965_surface_create_similar (void *abstract_other,
			     cairo_content_t content,
			     int width, int height)
{
    i965_surface_t *other;
    cairo_format_t format;

    if (width > 8192 || height > 8192)
	return NULL;

    other = abstract_other;
    if (content == other->intel.drm.base.content)
	format = other->intel.drm.format;
    else
	format = _cairo_format_from_content (content);

    return i965_surface_create_internal ((cairo_drm_device_t *) other->intel.drm.base.device,
					 format,
					 width, height,
					 I965_TILING_DEFAULT, TRUE);
}

static cairo_status_t
i965_surface_finish (void *abstract_surface)
{
    i965_surface_t *surface = abstract_surface;

    return intel_surface_finish (&surface->intel);
}

static cairo_status_t
i965_surface_flush (void *abstract_surface, unsigned flags)
{
    i965_surface_t *surface = abstract_surface;
    cairo_status_t status = CAIRO_STATUS_SUCCESS;

    if (flags)
	return CAIRO_STATUS_SUCCESS;

    if (surface->intel.drm.fallback != NULL)
	return intel_surface_flush (abstract_surface);

    /* Forgo flushing on finish as the user cannot access the surface directly. */
    if (! surface->intel.drm.base.finished &&
	to_intel_bo (surface->intel.drm.bo)->exec != NULL)
    {
	status = cairo_device_acquire (surface->intel.drm.base.device);
	if (likely (status == CAIRO_STATUS_SUCCESS)) {
	    i965_device_t *device;

	    device = i965_device (surface);
	    status = i965_device_flush (device);
	    cairo_device_release (&device->intel.base.base);
	}
    }

    return status;
}

/* rasterisation */

static cairo_status_t
_composite_boxes_spans (void				*closure,
			cairo_span_renderer_t		*renderer,
			const cairo_rectangle_int_t	*extents)
{
    cairo_boxes_t *boxes = closure;
    cairo_rectangular_scan_converter_t converter;
    struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;

    _cairo_rectangular_scan_converter_init (&converter, extents);
    for (chunk = &boxes->chunks; chunk != NULL; chunk = chunk->next) {
	cairo_box_t *box = chunk->base;
	int i;

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
i965_fixup_unbounded (i965_surface_t *dst,
		      const cairo_composite_rectangles_t *extents,
		      cairo_clip_t *clip)
{
    i965_shader_t shader;
    i965_device_t *device;
    cairo_status_t status;

    i965_shader_init (&shader, dst, CAIRO_OPERATOR_CLEAR);

    if (clip != NULL) {
	cairo_region_t *clip_region = NULL;

	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || CAIRO_INT_STATUS_UNSUPPORTED);
	assert (clip_region == NULL);

	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    i965_shader_set_clip (&shader, clip);
    } else {
	if (extents->bounded.width  == extents->unbounded.width &&
	    extents->bounded.height == extents->unbounded.height)
	{
	    return CAIRO_STATUS_SUCCESS;
	}
    }

    status = i965_shader_acquire_pattern (&shader,
					  &shader.source,
					  &_cairo_pattern_clear.base,
					  &extents->unbounded);
    if (unlikely (status)) {
	i965_shader_fini (&shader);
	return status;
    }

    device = i965_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	return status;

    status = i965_shader_commit (&shader, device);
    if (unlikely (status)) {
	goto BAIL;
    }

    if (extents->bounded.width == 0 || extents->bounded.height == 0) {
	i965_shader_add_rectangle (&shader,
				   extents->unbounded.x,
				   extents->unbounded.y,
				   extents->unbounded.width,
				   extents->unbounded.height);
    } else { /* top */
	if (extents->bounded.y != extents->unbounded.y) {
	    cairo_rectangle_int_t rect;

	    rect.x = extents->unbounded.x;
	    rect.y = extents->unbounded.y;
	    rect.width  = extents->unbounded.width;
	    rect.height = extents->bounded.y - rect.y;

	    i965_shader_add_rectangle (&shader,
				       rect.x, rect.y,
				       rect.width, rect.height);
	}

	/* left */
	if (extents->bounded.x != extents->unbounded.x) {
	    cairo_rectangle_int_t rect;

	    rect.x = extents->unbounded.x;
	    rect.y = extents->bounded.y;
	    rect.width  = extents->bounded.x - extents->unbounded.x;
	    rect.height = extents->bounded.height;

	    i965_shader_add_rectangle (&shader,
				       rect.x, rect.y,
				       rect.width, rect.height);
	}

	/* right */
	if (extents->bounded.x + extents->bounded.width != extents->unbounded.x + extents->unbounded.width) {
	    cairo_rectangle_int_t rect;

	    rect.x = extents->bounded.x + extents->bounded.width;
	    rect.y = extents->bounded.y;
	    rect.width  = extents->unbounded.x + extents->unbounded.width - rect.x;
	    rect.height = extents->bounded.height;

	    i965_shader_add_rectangle (&shader,
				       rect.x, rect.y,
				       rect.width, rect.height);
	}

	/* bottom */
	if (extents->bounded.y + extents->bounded.height != extents->unbounded.y + extents->unbounded.height) {
	    cairo_rectangle_int_t rect;

	    rect.x = extents->unbounded.x;
	    rect.y = extents->bounded.y + extents->bounded.height;
	    rect.width  = extents->unbounded.width;
	    rect.height = extents->unbounded.y + extents->unbounded.height - rect.y;

	    i965_shader_add_rectangle (&shader,
				       rect.x, rect.y,
				       rect.width, rect.height);
	}
    }

    i965_shader_fini (&shader);
  BAIL:
    cairo_device_release (&device->intel.base.base);
    return status;
}

static cairo_status_t
i965_fixup_unbounded_boxes (i965_surface_t *dst,
			    const cairo_composite_rectangles_t *extents,
			    cairo_clip_t *clip,
			    cairo_boxes_t *boxes)
{
    cairo_boxes_t clear;
    cairo_box_t box;
    cairo_region_t *clip_region = NULL;
    cairo_status_t status;
    struct _cairo_boxes_chunk *chunk;
    i965_shader_t shader;
    int i;

    if (boxes->num_boxes <= 1)
	return i965_fixup_unbounded (dst, extents, clip);

    i965_shader_init (&shader, dst, CAIRO_OPERATOR_CLEAR);
    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || CAIRO_INT_STATUS_UNSUPPORTED);
	if (status == CAIRO_INT_STATUS_UNSUPPORTED)
	    i965_shader_set_clip (&shader, clip);
    }

    status = i965_shader_acquire_pattern (&shader,
					  &shader.source,
					  &_cairo_pattern_clear.base,
					  &extents->unbounded);
    if (unlikely (status)) {
	i965_shader_fini (&shader);
	return status;
    }

    _cairo_boxes_init (&clear);

    box.p1.x = _cairo_fixed_from_int (extents->unbounded.x + extents->unbounded.width);
    box.p1.y = _cairo_fixed_from_int (extents->unbounded.y);
    box.p2.x = _cairo_fixed_from_int (extents->unbounded.x);
    box.p2.y = _cairo_fixed_from_int (extents->unbounded.y + extents->unbounded.height);

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
	i965_device_t *device;

	device = i965_device (dst);
	status = cairo_device_acquire (&device->intel.base.base);
	if (unlikely (status))
	    goto err_shader;

	status = i965_shader_commit (&shader, device);
	if (unlikely (status))
	    goto err_device;

	for (chunk = &clear.chunks; chunk != NULL; chunk = chunk->next) {
	    for (i = 0; i < chunk->count; i++) {
		int x1 = _cairo_fixed_integer_part (chunk->base[i].p1.x);
		int y1 = _cairo_fixed_integer_part (chunk->base[i].p1.y);
		int x2 = _cairo_fixed_integer_part (chunk->base[i].p2.x);
		int y2 = _cairo_fixed_integer_part (chunk->base[i].p2.y);

		i965_shader_add_rectangle (&shader, x1, y1, x2 - x1, y2 - y1);
	    }
	}

err_device:
	cairo_device_release (&device->intel.base.base);
err_shader:
	i965_shader_fini (&shader);
    }

    _cairo_boxes_fini (&clear);

    return status;
}

static cairo_status_t
_composite_boxes (i965_surface_t *dst,
		  cairo_operator_t op,
		  const cairo_pattern_t *pattern,
		  cairo_boxes_t *boxes,
		  cairo_antialias_t antialias,
		  cairo_clip_t *clip,
		  const cairo_composite_rectangles_t *extents)
{
    cairo_bool_t need_clip_surface = FALSE;
    cairo_region_t *clip_region = NULL;
    const struct _cairo_boxes_chunk *chunk;
    cairo_status_t status;
    i965_shader_t shader;
    i965_device_t *device;
    int i;

    /* If the boxes are not pixel-aligned, we will need to compute a real mask */
    if (antialias != CAIRO_ANTIALIAS_NONE) {
	if (! boxes->is_pixel_aligned)
	    return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    i965_shader_init (&shader, dst, op);

    status = i965_shader_acquire_pattern (&shader,
					  &shader.source,
					  pattern,
					  &extents->bounded);
    if (unlikely (status))
	return status;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || CAIRO_INT_STATUS_UNSUPPORTED);
	need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
	if (need_clip_surface)
	    i965_shader_set_clip (&shader, clip);
    }

    device = i965_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	goto err_shader;

    status = i965_shader_commit (&shader, i965_device (dst));
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
		i965_shader_add_rectangle (&shader, x1, y1, x2 - x1, y2 - y1);
	}
    }

    if (! extents->is_bounded)
	status = i965_fixup_unbounded_boxes (dst, extents, clip, boxes);

  err_device:
    cairo_device_release (&device->intel.base.base);
  err_shader:
    i965_shader_fini (&shader);

    return status;
}

static cairo_status_t
_clip_and_composite_boxes (i965_surface_t *dst,
			   cairo_operator_t op,
			   const cairo_pattern_t *src,
			   cairo_boxes_t *boxes,
			   cairo_antialias_t antialias,
			   const cairo_composite_rectangles_t *extents,
			   cairo_clip_t *clip)
{
    cairo_status_t status;

    if (boxes->num_boxes == 0) {
	if (extents->is_bounded)
	    return CAIRO_STATUS_SUCCESS;

	return i965_fixup_unbounded (dst, extents, clip);
    }

    /* Use a fast path if the boxes are pixel aligned */
    status = _composite_boxes (dst, op, src, boxes, antialias, clip, extents);
    if (status != CAIRO_INT_STATUS_UNSUPPORTED)
	return status;

    /* Otherwise render the boxes via an implicit mask and composite in the usual
     * fashion.
     */
    return i965_clip_and_composite_spans (dst, op, src, antialias,
					  _composite_boxes_spans, boxes,
					  extents, clip);
}

static cairo_int_status_t
i965_surface_paint (void			*abstract_dst,
		    cairo_operator_t		 op,
		    const cairo_pattern_t	*source,
		    cairo_clip_t		*clip)
{
    i965_surface_t *dst = abstract_dst;
    cairo_composite_rectangles_t extents;
    cairo_boxes_t boxes;
    cairo_box_t *clip_boxes = boxes.boxes_embedded;
    cairo_clip_t local_clip;
    cairo_bool_t have_clip = FALSE;
    int num_boxes = ARRAY_LENGTH (boxes.boxes_embedded);
    cairo_status_t status;

    /* XXX unsupported operators? use pixel shader blending, eventually */

    status = _cairo_composite_rectangles_init_for_paint (&extents,
							 dst->intel.drm.width,
							 dst->intel.drm.height,
							 op, source,
							 clip);
    if (unlikely (status))
	return status;

    if (clip != NULL && _cairo_clip_contains_extents (clip, &extents))
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

    _cairo_boxes_init_for_array (&boxes, clip_boxes, num_boxes);
    status = _clip_and_composite_boxes (dst, op, source,
					&boxes, CAIRO_ANTIALIAS_DEFAULT,
					&extents, clip);
    if (clip_boxes != boxes.boxes_embedded)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static cairo_int_status_t
i965_surface_mask (void				*abstract_dst,
		   cairo_operator_t		 op,
		   const cairo_pattern_t	*source,
		   const cairo_pattern_t	*mask,
		   cairo_clip_t			*clip)
{
    i965_surface_t *dst = abstract_dst;
    cairo_composite_rectangles_t extents;
    i965_shader_t shader;
    i965_device_t *device;
    cairo_clip_t local_clip;
    cairo_region_t *clip_region = NULL;
    cairo_bool_t need_clip_surface = FALSE;
    cairo_bool_t have_clip = FALSE;
    cairo_status_t status;

    status = _cairo_composite_rectangles_init_for_mask (&extents,
							dst->intel.drm.width,
							dst->intel.drm.height,
							op, source, mask, clip);
    if (unlikely (status))
	return status;

    if (clip != NULL && _cairo_clip_contains_extents (clip, &extents))
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

    i965_shader_init (&shader, dst, op);

    status = i965_shader_acquire_pattern (&shader,
					  &shader.source,
					  source,
					  &extents.bounded);
    if (unlikely (status))
	goto err_shader;

    status = i965_shader_acquire_pattern (&shader,
					  &shader.mask,
					  mask,
					  &extents.bounded);
    if (unlikely (status))
	goto err_shader;

    if (clip != NULL) {
	status = _cairo_clip_get_region (clip, &clip_region);
	assert (status == CAIRO_STATUS_SUCCESS || CAIRO_INT_STATUS_UNSUPPORTED);
	need_clip_surface = status == CAIRO_INT_STATUS_UNSUPPORTED;
	if (need_clip_surface)
	    i965_shader_set_clip (&shader, clip);
    }

    device = i965_device (dst);
    status = cairo_device_acquire (&device->intel.base.base);
    if (unlikely (status))
	goto err_shader;

    status = i965_shader_commit (&shader, device);
    if (unlikely (status))
	goto err_device;

    if (clip_region != NULL) {
	unsigned int n, num_rectangles;

	num_rectangles = cairo_region_num_rectangles (clip_region);
	for (n = 0; n < num_rectangles; n++) {
	    cairo_rectangle_int_t rect;

	    cairo_region_get_rectangle (clip_region, n, &rect);

	    i965_shader_add_rectangle (&shader,
				       rect.x, rect.y,
				       rect.width, rect.height);
	}
    } else {
	i965_shader_add_rectangle (&shader,
				   extents.bounded.x,
				   extents.bounded.y,
				   extents.bounded.width,
				   extents.bounded.height);
    }

    if (! extents.is_bounded)
	status = i965_fixup_unbounded (dst, &extents, clip);

  err_device:
    cairo_device_release (&device->intel.base.base);
  err_shader:
    i965_shader_fini (&shader);
    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
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
i965_surface_stroke (void			*abstract_dst,
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
    i965_surface_t *dst = abstract_dst;
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

    if (clip != NULL && _cairo_clip_contains_extents (clip, &extents))
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
						&extents, clip);
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
	    status = i965_fixup_unbounded (dst, &extents, clip);
    } else {
	info.fill_rule = CAIRO_FILL_RULE_WINDING;
	info.antialias = antialias;
	status = i965_clip_and_composite_spans (dst, op, source, antialias,
						_composite_polygon_spans, &info,
						&extents, clip);
    }

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
i965_surface_fill (void			*abstract_dst,
		   cairo_operator_t	 op,
		   const cairo_pattern_t*source,
		   cairo_path_fixed_t	*path,
		   cairo_fill_rule_t	 fill_rule,
		   double		 tolerance,
		   cairo_antialias_t	 antialias,
		   cairo_clip_t		*clip)
{
    i965_surface_t *dst = abstract_dst;
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

    if (clip != NULL && _cairo_clip_contains_extents (clip, &extents))
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
						&extents, clip);
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
	    status = i965_fixup_unbounded (dst, &extents, clip);
    } else {
	info.fill_rule = fill_rule;
	info.antialias = antialias;
	status = i965_clip_and_composite_spans (dst, op, source, antialias,
						_composite_polygon_spans, &info,
						&extents, clip);
    }

CLEANUP_POLYGON:
    _cairo_polygon_fini (&info.polygon);

CLEANUP_BOXES:
    if (clip_boxes != boxes_stack)
	free (clip_boxes);

    if (have_clip)
	_cairo_clip_fini (&local_clip);

    return status;
}

static const cairo_surface_backend_t i965_surface_backend = {
    CAIRO_SURFACE_TYPE_DRM,
    _cairo_default_context_create,

    i965_surface_create_similar,
    i965_surface_finish,

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

    i965_surface_flush,
    NULL, /* mark_dirty */
    intel_scaled_font_fini,
    intel_scaled_glyph_fini,

    i965_surface_paint,
    i965_surface_mask,
    i965_surface_stroke,
    i965_surface_fill,
    i965_surface_glyphs,
};

static void
i965_surface_init (i965_surface_t *surface,
		   cairo_drm_device_t *device,
	           cairo_format_t format,
		   int width, int height)
{
    intel_surface_init (&surface->intel, &i965_surface_backend, device,
			format, width, height);
    surface->stream = 0;
}

static inline int cairo_const
i965_tiling_stride (uint32_t tiling, int stride)
{
    if (tiling == I915_TILING_NONE)
	return stride;

    return (stride + 127) & -128;
}

static inline int cairo_const
i965_tiling_height (uint32_t tiling, int height)
{
    switch (tiling) {
    default:
    case I915_TILING_NONE: return (height + 1) & -2;
    case I915_TILING_X: return (height + 7) & -8;
    case I915_TILING_Y: return (height + 31) & -32;
    }
}

cairo_surface_t *
i965_surface_create_internal (cairo_drm_device_t *base_dev,
		              cairo_format_t format,
			      int width, int height,
			      uint32_t tiling,
			      cairo_bool_t gpu_target)
{
    i965_surface_t *surface;
    cairo_status_t status_ignored;

    surface = _cairo_malloc (sizeof (i965_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    i965_surface_init (surface, base_dev, format, width, height);

    if (width && height) {
	uint32_t size, stride;
	intel_bo_t *bo;

	width = (width + 3) & -4;
	stride = cairo_format_stride_for_width (surface->intel.drm.format, width);
	stride = (stride + 63) & ~63;
	stride = i965_tiling_stride (tiling, stride);
	surface->intel.drm.stride = stride;

	height = i965_tiling_height (tiling, height);
	assert (height <= I965_MAX_SIZE);

	size = stride * height;
	bo = intel_bo_create (to_intel_device (&base_dev->base),
			      size, size,
			      gpu_target, tiling, stride);
	if (bo == NULL) {
	    status_ignored = _cairo_drm_surface_finish (&surface->intel.drm);
	    free (surface);
	    return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
	}

	bo->tiling = tiling;
	bo->stride = stride;
	surface->intel.drm.bo = &bo->base;

	assert (bo->base.size >= (size_t) stride*height);
    }

    return &surface->intel.drm.base;
}

static cairo_surface_t *
i965_surface_create (cairo_drm_device_t *device,
		     cairo_format_t format, int width, int height)
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

    return i965_surface_create_internal (device, format, width, height,
	                                 I965_TILING_DEFAULT, TRUE);
}

static cairo_surface_t *
i965_surface_create_for_name (cairo_drm_device_t *base_dev,
			      unsigned int name,
			      cairo_format_t format,
			      int width, int height, int stride)
{
    i965_device_t *device;
    i965_surface_t *surface;
    cairo_status_t status_ignored;
    int min_stride;

    min_stride = cairo_format_stride_for_width (format, (width + 3) & -4);
    if (stride < min_stride || stride & 63)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_STRIDE));

    if (format == CAIRO_FORMAT_A1)
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_INVALID_FORMAT));

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

    surface = _cairo_malloc (sizeof (i965_surface_t));
    if (unlikely (surface == NULL))
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));

    i965_surface_init (surface, base_dev, format, width, height);

    device = (i965_device_t *) base_dev;
    surface->intel.drm.bo = &intel_bo_create_for_name (&device->intel, name)->base;
    if (unlikely (surface->intel.drm.bo == NULL)) {
	status_ignored = _cairo_drm_surface_finish (&surface->intel.drm);
	free (surface);
	return _cairo_surface_create_in_error (_cairo_error (CAIRO_STATUS_NO_MEMORY));
    }

    surface->intel.drm.stride = stride;

    return &surface->intel.drm.base;
}

static cairo_status_t
i965_surface_enable_scan_out (void *abstract_surface)
{
    i965_surface_t *surface = abstract_surface;
    intel_bo_t *bo;

    if (unlikely (surface->intel.drm.bo == NULL))
	return _cairo_error (CAIRO_STATUS_INVALID_SIZE);

    bo = to_intel_bo (surface->intel.drm.bo);
    if (bo->tiling != I915_TILING_X) {
	i965_device_t *device = i965_device (surface);
	cairo_surface_pattern_t pattern;
	cairo_surface_t *clone;
	cairo_status_t status;

	clone = i965_surface_create_internal (&device->intel.base,
					      surface->intel.drm.base.content,
					      surface->intel.drm.width,
					      surface->intel.drm.height,
					      I915_TILING_X,
					      TRUE);
	if (unlikely (clone->status))
	    return clone->status;

	/* 2D blit? */
	_cairo_pattern_init_for_surface (&pattern, &surface->intel.drm.base);
	pattern.base.filter = CAIRO_FILTER_NEAREST;

	status = _cairo_surface_paint (clone,
				       CAIRO_OPERATOR_SOURCE,
				       &pattern.base,
				       NULL);

	_cairo_pattern_fini (&pattern.base);

	if (unlikely (status)) {
	    cairo_surface_destroy (clone);
	    return status;
	}

	/* swap buffer objects */
	surface->intel.drm.bo = ((cairo_drm_surface_t *) clone)->bo;
	((cairo_drm_surface_t *) clone)->bo = &bo->base;
	bo = to_intel_bo (surface->intel.drm.bo);

	cairo_surface_destroy (clone);
    }

    if (unlikely (bo->tiling == I915_TILING_Y))
	return _cairo_error (CAIRO_STATUS_INVALID_FORMAT); /* XXX */

    return CAIRO_STATUS_SUCCESS;
}

static cairo_int_status_t
_i965_device_flush (cairo_drm_device_t *device)
{
    cairo_status_t status;

    if (unlikely (device->base.finished))
	return CAIRO_STATUS_SUCCESS;

    status = cairo_device_acquire (&device->base);
    if (likely (status == CAIRO_STATUS_SUCCESS))
	status = i965_device_flush ((i965_device_t *) device);

    cairo_device_release (&device->base);

    return status;
}

static cairo_int_status_t
_i965_device_throttle (cairo_drm_device_t *device)
{
    cairo_status_t status;

    status = cairo_device_acquire (&device->base);
    if (unlikely (status))
	return status;

    status = i965_device_flush ((i965_device_t *) device);
    intel_throttle ((intel_device_t *) device);

    cairo_device_release (&device->base);

    return status;
}

static void
_i965_device_destroy (void *base)
{
    i965_device_t *device = base;

    i965_device_reset (device);
    i965_general_state_reset (device);

    _cairo_hash_table_destroy (device->sf_states);
    _cairo_hash_table_destroy (device->samplers);
    _cairo_hash_table_destroy (device->cc_states);
    _cairo_hash_table_destroy (device->wm_kernels);
    _cairo_hash_table_destroy (device->wm_states);
    _cairo_hash_table_destroy (device->wm_bindings);

    _cairo_freelist_fini (&device->sf_freelist);
    _cairo_freelist_fini (&device->cc_freelist);
    _cairo_freelist_fini (&device->wm_kernel_freelist);
    _cairo_freelist_fini (&device->wm_state_freelist);
    _cairo_freelist_fini (&device->wm_binding_freelist);
    _cairo_freelist_fini (&device->sampler_freelist);

    intel_device_fini (&device->intel);
    free (device);
}

static cairo_bool_t
hash_equal (const void *A, const void *B)
{
    const cairo_hash_entry_t *a = A, *b = B;
    return a->hash == b->hash;
}

cairo_drm_device_t *
_cairo_drm_i965_device_create (int fd, dev_t dev, int vendor_id, int chip_id)
{
    i965_device_t *device;
    uint64_t gtt_size;
    cairo_status_t status;

    if (! intel_info (fd, &gtt_size))
	return  NULL;

    device = _cairo_malloc (sizeof (i965_device_t));
    if (unlikely (device == NULL))
	return (cairo_drm_device_t *) _cairo_device_create_in_error (CAIRO_STATUS_NO_MEMORY);

    status = intel_device_init (&device->intel, fd);
    if (unlikely (status))
	goto CLEANUP;

    device->is_g4x = IS_G4X (chip_id);
    //device->is_g5x = IS_G5X (chip_id);

    device->intel.base.surface.create = i965_surface_create;
    device->intel.base.surface.create_for_name = i965_surface_create_for_name;
    device->intel.base.surface.create_from_cacheable_image = NULL;
    device->intel.base.surface.enable_scan_out = i965_surface_enable_scan_out;

    device->intel.base.device.flush = _i965_device_flush;
    device->intel.base.device.throttle = _i965_device_throttle;
    device->intel.base.device.destroy = _i965_device_destroy;

    device->sf_states = _cairo_hash_table_create (i965_sf_state_equal);
    if (unlikely (device->sf_states == NULL))
	goto CLEANUP_INTEL;

    _cairo_freelist_init (&device->sf_freelist,
			  sizeof (struct i965_sf_state));


    device->cc_states = _cairo_hash_table_create (i965_cc_state_equal);
    if (unlikely (device->cc_states == NULL))
	goto CLEANUP_SF;

    _cairo_freelist_init (&device->cc_freelist,
			  sizeof (struct i965_cc_state));


    device->wm_kernels = _cairo_hash_table_create (hash_equal);
    if (unlikely (device->wm_kernels == NULL))
	goto CLEANUP_CC;

    _cairo_freelist_init (&device->wm_kernel_freelist,
			  sizeof (struct i965_wm_kernel));

    device->wm_states = _cairo_hash_table_create (i965_wm_state_equal);
    if (unlikely (device->wm_states == NULL))
	goto CLEANUP_WM_KERNEL;

    _cairo_freelist_init (&device->wm_state_freelist,
			  sizeof (struct i965_wm_state));


    device->wm_bindings = _cairo_hash_table_create (i965_wm_binding_equal);
    if (unlikely (device->wm_bindings == NULL))
	goto CLEANUP_WM_STATE;

    _cairo_freelist_init (&device->wm_binding_freelist,
			  sizeof (struct i965_wm_binding));

    device->samplers = _cairo_hash_table_create (hash_equal);
    if (unlikely (device->samplers == NULL))
	goto CLEANUP_WM_BINDING;

    _cairo_freelist_init (&device->sampler_freelist,
			  sizeof (struct i965_sampler));

    i965_stream_init (&device->batch,
		      device->batch_base, sizeof (device->batch_base),
		      NULL, 0,
		      device->batch_relocations,
		      ARRAY_LENGTH (device->batch_relocations));

    i965_stream_init (&device->surface,
		      device->surface_base, sizeof (device->surface_base),
		      device->surface_pending_relocations,
		      ARRAY_LENGTH (device->surface_pending_relocations),
		      device->surface_relocations,
		      ARRAY_LENGTH (device->surface_relocations));

    i965_stream_init (&device->general,
		      device->general_base, sizeof (device->general_base),
		      device->general_pending_relocations,
		      ARRAY_LENGTH (device->general_pending_relocations),
		      NULL, 0);

    i965_stream_init (&device->vertex,
		      device->vertex_base, sizeof (device->vertex_base),
		      device->vertex_pending_relocations,
		      ARRAY_LENGTH (device->vertex_pending_relocations),
		      NULL, 0);

    cairo_list_init (&device->flush);
    i965_device_reset (device);
    device->vs_offset = (uint32_t) -1;
    device->border_color_offset = (uint32_t) -1;
    device->general_state = NULL;

    return _cairo_drm_device_init (&device->intel.base,
				   fd, dev, vendor_id, chip_id,
				   I965_MAX_SIZE);

  CLEANUP_WM_BINDING:
    _cairo_hash_table_destroy (device->wm_bindings);
  CLEANUP_WM_STATE:
    _cairo_hash_table_destroy (device->wm_states);
  CLEANUP_WM_KERNEL:
    _cairo_hash_table_destroy (device->wm_kernels);
  CLEANUP_CC:
    _cairo_hash_table_destroy (device->cc_states);
  CLEANUP_SF:
    _cairo_hash_table_destroy (device->sf_states);
  CLEANUP_INTEL:
    intel_device_fini (&device->intel);
  CLEANUP:
    free (device);
    return (cairo_drm_device_t *) _cairo_device_create_in_error (status);
}

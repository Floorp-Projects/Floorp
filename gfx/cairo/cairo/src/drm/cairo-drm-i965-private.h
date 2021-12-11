#ifndef CAIRO_DRM_I965_PRIVATE_H
#define CAIRO_DRM_I965_PRIVATE_H

#include "cairo-drm-intel-private.h"

#include "cairo-hash-private.h"
#include "cairo-freelist-private.h"

#include "cairo-drm-intel-brw-defines.h"

#include <setjmp.h>

#define BRW_MI_GLOBAL_SNAPSHOT_RESET   (1 << 3)

/*
 * New regs for broadwater -- we need to split this file up sensibly somehow.
 */
#define BRW_3D(Pipeline,Opcode,Subopcode) ((3 << 29) | \
					   ((Pipeline) << 27) | \
					   ((Opcode) << 24) | \
					   ((Subopcode) << 16))

#define BRW_URB_FENCE				BRW_3D(0, 0, 0)
#define BRW_CS_URB_STATE			BRW_3D(0, 0, 1)
#define BRW_CONSTANT_BUFFER			BRW_3D(0, 0, 2)
#define BRW_STATE_PREFETCH			BRW_3D(0, 0, 3)

#define BRW_STATE_BASE_ADDRESS			BRW_3D(0, 1, 1)
#define BRW_STATE_SIP				BRW_3D(0, 1, 2)
#define BRW_PIPELINE_SELECT			BRW_3D(0, 1, 4)

#define NEW_PIPELINE_SELECT			BRW_3D(1, 1, 4)

#define BRW_MEDIA_STATE_POINTERS		BRW_3D(2, 0, 0)
#define BRW_MEDIA_OBJECT			BRW_3D(2, 1, 0)

#define BRW_3DSTATE_PIPELINED_POINTERS		BRW_3D(3, 0, 0)
#define BRW_3DSTATE_BINDING_TABLE_POINTERS	BRW_3D(3, 0, 1)
#define BRW_3DSTATE_VERTEX_BUFFERS		BRW_3D(3, 0, 8)
#define BRW_3DSTATE_VERTEX_ELEMENTS		BRW_3D(3, 0, 9)
#define BRW_3DSTATE_INDEX_BUFFER		BRW_3D(3, 0, 0xa)
#define BRW_3DSTATE_VF_STATISTICS		BRW_3D(3, 0, 0xb)

#define BRW_3DSTATE_DRAWING_RECTANGLE		BRW_3D(3, 1, 0)
#define BRW_3DSTATE_CONSTANT_COLOR		BRW_3D(3, 1, 1)
#define BRW_3DSTATE_SAMPLER_PALETTE_LOAD	BRW_3D(3, 1, 2)
#define BRW_3DSTATE_CHROMA_KEY			BRW_3D(3, 1, 4)
#define BRW_3DSTATE_DEPTH_BUFFER		BRW_3D(3, 1, 5)
#define BRW_3DSTATE_POLY_STIPPLE_OFFSET		BRW_3D(3, 1, 6)
#define BRW_3DSTATE_POLY_STIPPLE_PATTERN	BRW_3D(3, 1, 7)
#define BRW_3DSTATE_LINE_STIPPLE		BRW_3D(3, 1, 8)
#define BRW_3DSTATE_GLOBAL_DEPTH_OFFSET_CLAMP	BRW_3D(3, 1, 9)
/* These two are BLC and CTG only, not BW or CL */
#define BRW_3DSTATE_AA_LINE_PARAMS		BRW_3D(3, 1, 0xa)
#define BRW_3DSTATE_GS_SVB_INDEX		BRW_3D(3, 1, 0xb)

#define BRW_PIPE_CONTROL			BRW_3D(3, 2, 0)

#define BRW_3DPRIMITIVE				BRW_3D(3, 3, 0)

#define PIPELINE_SELECT_3D		0
#define PIPELINE_SELECT_MEDIA		1

#define UF0_CS_REALLOC			(1 << 13)
#define UF0_VFE_REALLOC			(1 << 12)
#define UF0_SF_REALLOC			(1 << 11)
#define UF0_CLIP_REALLOC		(1 << 10)
#define UF0_GS_REALLOC			(1 << 9)
#define UF0_VS_REALLOC			(1 << 8)
#define UF1_CLIP_FENCE_SHIFT		20
#define UF1_GS_FENCE_SHIFT		10
#define UF1_VS_FENCE_SHIFT		0
#define UF2_CS_FENCE_SHIFT		20
#define UF2_VFE_FENCE_SHIFT		10
#define UF2_SF_FENCE_SHIFT		0

/* for BRW_STATE_BASE_ADDRESS */
#define BASE_ADDRESS_MODIFY		(1 << 0)

/* for BRW_3DSTATE_PIPELINED_POINTERS */
#define BRW_GS_DISABLE		       0
#define BRW_GS_ENABLE		       1
#define BRW_CLIP_DISABLE	       0
#define BRW_CLIP_ENABLE		       1

/* for BRW_PIPE_CONTROL */
#define BRW_PIPE_CONTROL_NOWRITE       (0 << 14)
#define BRW_PIPE_CONTROL_WRITE_QWORD   (1 << 14)
#define BRW_PIPE_CONTROL_WRITE_DEPTH   (2 << 14)
#define BRW_PIPE_CONTROL_WRITE_TIME    (3 << 14)
#define BRW_PIPE_CONTROL_DEPTH_STALL   (1 << 13)
#define BRW_PIPE_CONTROL_WC_FLUSH      (1 << 12)
#define BRW_PIPE_CONTROL_IS_FLUSH      (1 << 11)
#define BRW_PIPE_CONTROL_NOTIFY_ENABLE (1 << 8)
#define BRW_PIPE_CONTROL_GLOBAL_GTT    (1 << 2)
#define BRW_PIPE_CONTROL_LOCAL_PGTT    (0 << 2)

/* VERTEX_BUFFER_STATE Structure */
#define VB0_BUFFER_INDEX_SHIFT		27
#define VB0_VERTEXDATA			(0 << 26)
#define VB0_INSTANCEDATA		(1 << 26)
#define VB0_BUFFER_PITCH_SHIFT		0

/* VERTEX_ELEMENT_STATE Structure */
#define VE0_VERTEX_BUFFER_INDEX_SHIFT	27
#define VE0_VALID			(1 << 26)
#define VE0_FORMAT_SHIFT		16
#define VE0_OFFSET_SHIFT		0
#define VE1_VFCOMPONENT_0_SHIFT		28
#define VE1_VFCOMPONENT_1_SHIFT		24
#define VE1_VFCOMPONENT_2_SHIFT		20
#define VE1_VFCOMPONENT_3_SHIFT		16
#define VE1_DESTINATION_ELEMENT_OFFSET_SHIFT	0

/* 3DPRIMITIVE bits */
#define BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL (0 << 15)
#define BRW_3DPRIMITIVE_VERTEX_RANDOM	  (1 << 15)
/* Primitive types are in brw_defines.h */
#define BRW_3DPRIMITIVE_TOPOLOGY_SHIFT	  10

#define BRW_SVG_CTL		       0x7400

#define BRW_SVG_CTL_GS_BA	       (0 << 8)
#define BRW_SVG_CTL_SS_BA	       (1 << 8)
#define BRW_SVG_CTL_IO_BA	       (2 << 8)
#define BRW_SVG_CTL_GS_AUB	       (3 << 8)
#define BRW_SVG_CTL_IO_AUB	       (4 << 8)
#define BRW_SVG_CTL_SIP		       (5 << 8)

#define BRW_SVG_RDATA		       0x7404
#define BRW_SVG_WORK_CTL	       0x7408

#define BRW_VF_CTL		       0x7500

#define BRW_VF_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_VF_CTL_SNAPSHOT_MUX_SELECT_THREADID	   (0 << 8)
#define BRW_VF_CTL_SNAPSHOT_MUX_SELECT_VF_DEBUG	   (1 << 8)
#define BRW_VF_CTL_SNAPSHOT_TYPE_VERTEX_SEQUENCE   (0 << 4)
#define BRW_VF_CTL_SNAPSHOT_TYPE_VERTEX_INDEX	   (1 << 4)
#define BRW_VF_CTL_SKIP_INITIAL_PRIMITIVES	   (1 << 3)
#define BRW_VF_CTL_MAX_PRIMITIVES_LIMIT_ENABLE	   (1 << 2)
#define BRW_VF_CTL_VERTEX_RANGE_LIMIT_ENABLE	   (1 << 1)
#define BRW_VF_CTL_SNAPSHOT_ENABLE	     	   (1 << 0)

#define BRW_VF_STRG_VAL		       0x7504
#define BRW_VF_STR_VL_OVR	       0x7508
#define BRW_VF_VC_OVR		       0x750c
#define BRW_VF_STR_PSKIP	       0x7510
#define BRW_VF_MAX_PRIM		       0x7514
#define BRW_VF_RDATA		       0x7518

#define BRW_VS_CTL		       0x7600
#define BRW_VS_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_VS_CTL_SNAPSHOT_MUX_VERTEX_0	   (0 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VERTEX_1	   (1 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VALID_COUNT	   (2 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VS_KERNEL_POINTER  (3 << 8)
#define BRW_VS_CTL_SNAPSHOT_ALL_THREADS		   (1 << 2)
#define BRW_VS_CTL_THREAD_SNAPSHOT_ENABLE	   (1 << 1)
#define BRW_VS_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_VS_STRG_VAL		       0x7604
#define BRW_VS_RDATA		       0x7608

#define BRW_SF_CTL		       0x7b00
#define BRW_SF_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_0_FF_ID	   (0 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_0_REL_COUNT (1 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_1_FF_ID	   (2 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_1_REL_COUNT (3 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_2_FF_ID	   (4 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_2_REL_COUNT (5 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_COUNT	   (6 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_SF_KERNEL_POINTER  (7 << 8)
#define BRW_SF_CTL_MIN_MAX_PRIMITIVE_RANGE_ENABLE  (1 << 4)
#define BRW_SF_CTL_DEBUG_CLIP_RECTANGLE_ENABLE	   (1 << 3)
#define BRW_SF_CTL_SNAPSHOT_ALL_THREADS		   (1 << 2)
#define BRW_SF_CTL_THREAD_SNAPSHOT_ENABLE	   (1 << 1)
#define BRW_SF_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_SF_STRG_VAL		       0x7b04
#define BRW_SF_RDATA		       0x7b18

#define BRW_WIZ_CTL		       0x7c00
#define BRW_WIZ_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_WIZ_CTL_SUBSPAN_INSTANCE_SHIFT	   16
#define BRW_WIZ_CTL_SNAPSHOT_MUX_WIZ_KERNEL_POINTER   (0 << 8)
#define BRW_WIZ_CTL_SNAPSHOT_MUX_SUBSPAN_INSTANCE     (1 << 8)
#define BRW_WIZ_CTL_SNAPSHOT_MUX_PRIMITIVE_SEQUENCE   (2 << 8)
#define BRW_WIZ_CTL_SINGLE_SUBSPAN_DISPATCH	      (1 << 6)
#define BRW_WIZ_CTL_IGNORE_COLOR_SCOREBOARD_STALLS    (1 << 5)
#define BRW_WIZ_CTL_ENABLE_SUBSPAN_INSTANCE_COMPARE   (1 << 4)
#define BRW_WIZ_CTL_USE_UPSTREAM_SNAPSHOT_FLAG	      (1 << 3)
#define BRW_WIZ_CTL_SNAPSHOT_ALL_THREADS	      (1 << 2)
#define BRW_WIZ_CTL_THREAD_SNAPSHOT_ENABLE	      (1 << 1)
#define BRW_WIZ_CTL_SNAPSHOT_ENABLE		      (1 << 0)

#define BRW_WIZ_STRG_VAL			      0x7c04
#define BRW_WIZ_RDATA				      0x7c18

#define BRW_TS_CTL		       0x7e00
#define BRW_TS_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_TS_CTL_SNAPSHOT_MESSAGE_ERROR	   (0 << 8)
#define BRW_TS_CTL_SNAPSHOT_INTERFACE_DESCRIPTOR   (3 << 8)
#define BRW_TS_CTL_SNAPSHOT_ALL_CHILD_THREADS	   (1 << 2)
#define BRW_TS_CTL_SNAPSHOT_ALL_ROOT_THREADS  	   (1 << 1)
#define BRW_TS_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_TS_STRG_VAL		       0x7e04
#define BRW_TS_RDATA		       0x7e08

#define BRW_TD_CTL		       0x8000
#define BRW_TD_CTL_MUX_SHIFT	       8
#define BRW_TD_CTL_EXTERNAL_HALT_R0_DEBUG_MATCH	   (1 << 7)
#define BRW_TD_CTL_FORCE_EXTERNAL_HALT		   (1 << 6)
#define BRW_TD_CTL_EXCEPTION_MASK_OVERRIDE	   (1 << 5)
#define BRW_TD_CTL_FORCE_THREAD_BREAKPOINT_ENABLE  (1 << 4)
#define BRW_TD_CTL_BREAKPOINT_ENABLE		   (1 << 2)
#define BRW_TD_CTL2		       0x8004
#define BRW_TD_CTL2_ILLEGAL_OPCODE_EXCEPTION_OVERRIDE (1 << 28)
#define BRW_TD_CTL2_MASKSTACK_EXCEPTION_OVERRIDE      (1 << 26)
#define BRW_TD_CTL2_SOFTWARE_EXCEPTION_OVERRIDE	      (1 << 25)
#define BRW_TD_CTL2_ACTIVE_THREAD_LIMIT_SHIFT	      16
#define BRW_TD_CTL2_ACTIVE_THREAD_LIMIT_ENABLE	      (1 << 8)
#define BRW_TD_CTL2_THREAD_SPAWNER_EXECUTION_MASK_ENABLE (1 << 7)
#define BRW_TD_CTL2_WIZ_EXECUTION_MASK_ENABLE	      (1 << 6)
#define BRW_TD_CTL2_SF_EXECUTION_MASK_ENABLE	      (1 << 5)
#define BRW_TD_CTL2_CLIPPER_EXECUTION_MASK_ENABLE     (1 << 4)
#define BRW_TD_CTL2_GS_EXECUTION_MASK_ENABLE	      (1 << 3)
#define BRW_TD_CTL2_VS_EXECUTION_MASK_ENABLE	      (1 << 0)
#define BRW_TD_VF_VS_EMSK	       0x8008
#define BRW_TD_GS_EMSK		       0x800c
#define BRW_TD_CLIP_EMSK	       0x8010
#define BRW_TD_SF_EMSK		       0x8014
#define BRW_TD_WIZ_EMSK		       0x8018
#define BRW_TD_0_6_EHTRG_VAL	       0x801c
#define BRW_TD_0_7_EHTRG_VAL	       0x8020
#define BRW_TD_0_6_EHTRG_MSK           0x8024
#define BRW_TD_0_7_EHTRG_MSK	       0x8028
#define BRW_TD_RDATA		       0x802c
#define BRW_TD_TS_EMSK		       0x8030

#define BRW_EU_CTL		       0x8800
#define BRW_EU_CTL_SELECT_SHIFT	       16
#define BRW_EU_CTL_DATA_MUX_SHIFT      8
#define BRW_EU_ATT_0		       0x8810
#define BRW_EU_ATT_1		       0x8814
#define BRW_EU_ATT_DATA_0	       0x8820
#define BRW_EU_ATT_DATA_1	       0x8824
#define BRW_EU_ATT_CLR_0	       0x8830
#define BRW_EU_ATT_CLR_1	       0x8834
#define BRW_EU_RDATA		       0x8840

typedef struct i965_device i965_device_t;
typedef struct i965_surface i965_surface_t;
typedef struct i965_shader i965_shader_t;
typedef struct i965_stream i965_stream_t;

struct i965_sf_state {
    cairo_hash_entry_t entry;
    uint32_t offset;
};

cairo_private cairo_bool_t
i965_sf_state_equal (const void *, const void *);

struct i965_cc_state {
    cairo_hash_entry_t entry;
    uint32_t offset;
};

cairo_private cairo_bool_t
i965_cc_state_equal (const void *, const void *);

struct i965_wm_kernel {
    cairo_hash_entry_t entry;
    uint32_t offset;
};

struct i965_wm_state {
    cairo_hash_entry_t entry;
    uint32_t kernel;
    uint32_t sampler;
    uint32_t offset;
};

cairo_private cairo_bool_t
i965_wm_state_equal (const void *, const void *);

struct i965_wm_binding {
    cairo_hash_entry_t entry;
    uint32_t table[4];
    int size;
    uint32_t offset;
};

cairo_private cairo_bool_t
i965_wm_binding_equal (const void *, const void *);

struct i965_sampler {
    cairo_hash_entry_t entry;
    uint32_t offset;
};

struct i965_vbo {
    struct i965_vbo *next;
    intel_bo_t *bo;
    unsigned int count;
};

struct i965_surface {
    intel_surface_t intel;

    uint32_t stream;
    uint32_t offset;
};

struct i965_pending_relocation {
    uint32_t offset;
    uint32_t read_domains;
    uint32_t write_domain;
    uint32_t delta;
};

struct i965_stream {
    uint32_t used;
    uint32_t committed;
    uint32_t size;
    uint8_t *data;
    uint32_t serial;

    int num_pending_relocations;
    int max_pending_relocations;
    struct i965_pending_relocation *pending_relocations;

    int num_relocations;
    int max_relocations;
    struct drm_i915_gem_relocation_entry *relocations;
};

#define I965_BATCH_SIZE (16 * 4096)
#define I965_GENERAL_SIZE (16 * 4096)
#define I965_SURFACE_SIZE (32 * 4096)
#define I965_VERTEX_SIZE (128 * 4096)

#define I965_TILING_DEFAULT I915_TILING_Y


struct i965_device {
    intel_device_t intel;

    cairo_bool_t is_g4x;

    i965_shader_t *shader; /* note: only valid during geometry emission */

    /* track state changes */
    struct i965_sf_state sf_state;
    struct i965_cc_state cc_state;
    struct i965_wm_state wm_state;
    struct i965_wm_binding wm_binding;

    i965_surface_t *target;
    uint32_t target_offset;

    intel_bo_t *source;
    uint32_t source_offset;

    intel_bo_t *mask;
    uint32_t mask_offset;

    intel_bo_t *clip;
    uint32_t clip_offset;

    uint32_t draw_rectangle;

    uint32_t vs_offset;
    uint32_t border_color_offset;
    cairo_hash_table_t *sf_states;
    cairo_hash_table_t *cc_states;
    cairo_hash_table_t *wm_kernels;
    cairo_hash_table_t *wm_states;
    cairo_hash_table_t *wm_bindings;
    cairo_hash_table_t *samplers;
    intel_bo_t *general_state;

    cairo_freelist_t sf_freelist;
    cairo_freelist_t cc_freelist;
    cairo_freelist_t wm_kernel_freelist;
    cairo_freelist_t wm_state_freelist;
    cairo_freelist_t wm_binding_freelist;
    cairo_freelist_t sampler_freelist;

    uint32_t vertex_type;
    uint32_t vertex_size;
    uint32_t rectangle_size;
    uint32_t last_vertex_size;

    float *constants; /* 4 x matrix + 2 x source */
    unsigned constants_size;
    cairo_bool_t have_urb_fences;

    i965_stream_t batch;
    uint8_t batch_base[I965_BATCH_SIZE];
    struct drm_i915_gem_relocation_entry batch_relocations[2048];

    i965_stream_t surface;
    uint8_t surface_base[I965_SURFACE_SIZE];
    struct i965_pending_relocation surface_pending_relocations[1];
    struct drm_i915_gem_relocation_entry surface_relocations[1024];

    i965_stream_t general;
    uint8_t general_base[I965_GENERAL_SIZE];
    struct i965_pending_relocation general_pending_relocations[1];

    i965_stream_t vertex;
    uint8_t vertex_base[I965_VERTEX_SIZE];
    struct i965_pending_relocation vertex_pending_relocations[512];

    struct {
	size_t gtt_size;

	intel_bo_t *bo[1024];
	int count;

	struct drm_i915_gem_exec_object2 exec[1024];
    } exec;
    cairo_list_t flush;
};

typedef enum {
    VS_NONE = 0,
    VS_GLYPHS,
    VS_SPANS,
} i965_vertex_shader_t;

typedef enum {
    FS_NONE = 0,
    FS_CONSTANT,
    FS_LINEAR,
    FS_RADIAL,
    FS_SURFACE,
    FS_GLYPHS,
    FS_SPANS,
} i965_fragment_shader_t;

typedef enum {
    PATTERN_BASE,
    PATTERN_SOLID,
    PATTERN_LINEAR,
    PATTERN_RADIAL,
    PATTERN_SURFACE,
} i965_shader_channel_t;
#define PATTERN_NONE (i965_shader_channel_t)-1

struct i965_shader {
    i965_device_t *device;
    i965_surface_t *target;

    cairo_operator_t op;

    cairo_bool_t committed;
    cairo_bool_t need_combine;

    float constants[4*8 + 2*8]; /* 4 x matrix + 2 x source */
    unsigned constants_size;

    union i965_shader_channel {
	struct {
	    i965_vertex_shader_t vertex;
	    i965_fragment_shader_t fragment;
	    i965_shader_channel_t pattern;
	} type;
	struct i965_shader_base {
	    i965_vertex_shader_t vertex;
	    i965_fragment_shader_t fragment;
	    i965_shader_channel_t pattern;

	    uint32_t mode;

	    float constants[8];
	    unsigned constants_size;

	    intel_bo_t *bo;
	    cairo_format_t format;
	    cairo_content_t content;
	    int width, height, stride;
	    int filter, extend;
	    cairo_matrix_t matrix;
	    cairo_bool_t has_component_alpha;
	} base;
	struct i965_shader_solid {
	    struct i965_shader_base base;
	} solid;
	struct i965_shader_linear {
	    struct i965_shader_base base;
	} linear;
	struct i965_shader_radial {
	    struct i965_shader_base base;
	} radial;
	struct i965_shader_surface {
	    struct i965_shader_base base;
	    cairo_surface_t *surface;
	} surface;
    } source, mask, clip, dst;

    jmp_buf unwind;
};

enum i965_shader_linear_mode {
    /* XXX REFLECT */
    LINEAR_TEXTURE,
    LINEAR_NONE,
    LINEAR_REPEAT,
    LINEAR_PAD,
};

enum i965_shader_radial_mode {
    RADIAL_ONE,
    RADIAL_TWO
};

typedef cairo_status_t
(*i965_spans_func_t) (void			*closure,
		      cairo_span_renderer_t	*renderer,
		      const cairo_rectangle_int_t	*extents);

static inline i965_device_t *
i965_device (i965_surface_t *surface)
{
    return (i965_device_t *) surface->intel.drm.base.device;
}

cairo_private void
i965_emit_relocation (i965_device_t *device,
		      i965_stream_t *stream,
		      intel_bo_t *target,
		      uint32_t target_offset,
		      uint32_t read_domains,
		      uint32_t write_domain,
		      uint32_t offset);

static cairo_always_inline uint32_t
i965_stream_emit (i965_stream_t *stream, const void *data, size_t size)
{
    uint32_t offset;

    offset = stream->used;
    assert (offset + size <= stream->size);
    memcpy (stream->data + offset, data, size);
    stream->used += size;

    return offset;
}

static cairo_always_inline void
i965_stream_align (i965_stream_t *stream, uint32_t size)
{
    stream->used = (stream->used + size - 1) & -size;
}

static cairo_always_inline void *
i965_stream_alloc (i965_stream_t *stream, uint32_t align, uint32_t size)
{
    void *ptr;

    if (align)
	i965_stream_align (stream, align);

    assert (stream->used + size <= stream->size);
    ptr = stream->data + stream->used;
    stream->used += size;

    return ptr;
}

static cairo_always_inline uint32_t
i965_stream_offsetof (i965_stream_t *stream, const void *ptr)
{
    return (char *) ptr - (char *) stream->data;
}

cairo_private void
i965_stream_commit (i965_device_t *device,
		    i965_stream_t *stream);

cairo_private void
i965_general_state_reset (i965_device_t *device);

static inline void
i965_batch_emit_dword (i965_device_t *device, uint32_t dword)
{
    *(uint32_t *) (device->batch.data + device->batch.used) = dword;
    device->batch.used += 4;
}

#define OUT_BATCH(dword) i965_batch_emit_dword(device, dword)

cairo_private void
i965_clipped_vertices (i965_device_t *device,
		       struct i965_vbo *vbo,
		       cairo_region_t *clip_region);

cairo_private void
i965_flush_vertices (i965_device_t *device);

cairo_private void
i965_finish_vertices (i965_device_t *device);

static inline float *
i965_add_rectangle (i965_device_t *device)
{
    float *vertices;
    uint32_t size;

    size = device->rectangle_size;
    if (unlikely (device->vertex.used + size > device->vertex.size))
	i965_finish_vertices (device);

    vertices = (float *) (device->vertex.data + device->vertex.used);
    device->vertex.used += size;

    return vertices;
}

static inline void
i965_shader_add_rectangle (const i965_shader_t *shader,
			   int x, int y,
			   int w, int h)
{
    float *v;

    v= i965_add_rectangle (shader->device);

    /* bottom-right */
    *v++ = x + w;
    *v++ = y + h;

    /* bottom-left */
    *v++ = x;
    *v++ = y + h;

    /* top-left */
    *v++ = x;
    *v++ = y;
}

cairo_private cairo_surface_t *
i965_surface_create_internal (cairo_drm_device_t *base_dev,
			      cairo_format_t format,
			      int width, int height,
			      uint32_t tiling,
			      cairo_bool_t gpu_target);

cairo_private cairo_status_t
i965_clip_and_composite_spans (i965_surface_t		*dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*pattern,
			       cairo_antialias_t	 antialias,
			       i965_spans_func_t	 draw_func,
			       void			*draw_closure,
			       const cairo_composite_rectangles_t*extents,
			       cairo_clip_t		*clip);

cairo_private cairo_int_status_t
i965_surface_glyphs (void			*abstract_surface,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*source,
		     cairo_glyph_t		*glyphs,
		     int			 num_glyphs,
		     cairo_scaled_font_t	*scaled_font,
		     cairo_clip_t		*clip,
		     int *num_remaining);

cairo_private void
i965_shader_init (i965_shader_t *shader,
		  i965_surface_t *dst,
		  cairo_operator_t op);

cairo_private cairo_status_t
i965_shader_acquire_pattern (i965_shader_t *shader,
			     union i965_shader_channel *src,
			     const cairo_pattern_t *pattern,
			     const cairo_rectangle_int_t *extents);

cairo_private void
i965_shader_set_clip (i965_shader_t *shader,
		      cairo_clip_t *clip);

cairo_private cairo_status_t
i965_shader_commit (i965_shader_t *shader,
		    i965_device_t *device);

cairo_private void
i965_shader_fini (i965_shader_t *shader);

cairo_private cairo_status_t
i965_device_flush (i965_device_t *device);

cairo_private cairo_status_t
i965_fixup_unbounded (i965_surface_t *dst,
		      const cairo_composite_rectangles_t *extents,
		      cairo_clip_t *clip);

static inline int
i965_filter (cairo_filter_t filter)
{
    switch (filter) {
    default:
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	return BRW_MAPFILTER_NEAREST;

    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_BILINEAR:
    case CAIRO_FILTER_GAUSSIAN:
	return BRW_MAPFILTER_LINEAR;
    }
}

static inline int
i965_extend (cairo_extend_t extend)
{
    switch (extend) {
    default:
    case CAIRO_EXTEND_NONE:
	return BRW_TEXCOORDMODE_CLAMP_BORDER;
    case CAIRO_EXTEND_REPEAT:
	return BRW_TEXCOORDMODE_WRAP;
    case CAIRO_EXTEND_PAD:
	return BRW_TEXCOORDMODE_CLAMP;
    case CAIRO_EXTEND_REFLECT:
	return BRW_TEXCOORDMODE_MIRROR;
    }
}

#endif /* CAIRO_DRM_I965_PRIVATE_H */

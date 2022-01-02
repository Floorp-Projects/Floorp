/**************************************************************************
 *
 * Copyright 2005 Tungsten Graphics, Inc., Cedar Park, Texas.
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
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifndef CAIRO_DRM_INTEL_BRW_STRUCTS_H
#define CAIRO_DRM_INTEL_BRW_STRUCTS_H

#include "cairo.h"
#include "cairo-types-private.h"

/* Command packets:
*/
struct header {
    unsigned int length:16;
    unsigned int opcode:16;
};

union header_union {
    struct header bits;
    unsigned int dword;
};

struct brw_3d_control {
    struct {
	unsigned int length:8;
	unsigned int notify_enable:1;
	unsigned int pad:3;
	unsigned int wc_flush_enable:1;
	unsigned int depth_stall_enable:1;
	unsigned int operation:2;
	unsigned int opcode:16;
    } header;

    struct {
	unsigned int pad:2;
	unsigned int dest_addr_type:1;
	unsigned int dest_addr:29;
    } dest;

    unsigned int dword2;
    unsigned int dword3;
};


struct brw_3d_primitive {
    struct {
	unsigned int length:8;
	unsigned int pad:2;
	unsigned int topology:5;
	unsigned int indexed:1;
	unsigned int opcode:16;
    } header;

    unsigned int verts_per_instance;
    unsigned int start_vert_location;
    unsigned int instance_count;
    unsigned int start_instance_location;
    unsigned int base_vert_location;
};

/* These seem to be passed around as function args, so it works out
 * better to keep them as #defines:
 */
#define BRW_FLUSH_READ_CACHE           0x1
#define BRW_FLUSH_STATE_CACHE          0x2
#define BRW_INHIBIT_FLUSH_RENDER_CACHE 0x4
#define BRW_FLUSH_SNAPSHOT_COUNTERS    0x8

struct brw_mi_flush {
    unsigned int flags:4;
    unsigned int pad:12;
    unsigned int opcode:16;
};

struct brw_vf_statistics {
    unsigned int statistics_enable:1;
    unsigned int pad:15;
    unsigned int opcode:16;
};


struct brw_binding_table_pointers {
    struct header header;
    unsigned int vs;
    unsigned int gs;
    unsigned int clp;
    unsigned int sf;
    unsigned int wm;
};

struct brw_blend_constant_color {
    struct header header;
    float blend_constant_color[4];
};

struct brw_depthbuffer {
    union header_union header;

    union {
	struct {
	    unsigned int pitch:18;
	    unsigned int format:3;
	    unsigned int pad:4;
	    unsigned int depth_offset_disable:1;
	    unsigned int tile_walk:1;
	    unsigned int tiled_surface:1;
	    unsigned int pad2:1;
	    unsigned int surface_type:3;
	} bits;
	unsigned int dword;
    } dword1;

    unsigned int dword2_base_addr;

    union {
	struct {
	    unsigned int pad:1;
	    unsigned int mipmap_layout:1;
	    unsigned int lod:4;
	    unsigned int width:13;
	    unsigned int height:13;
	} bits;
	unsigned int dword;
    } dword3;

    union {
	struct {
	    unsigned int pad:12;
	    unsigned int min_array_element:9;
	    unsigned int depth:11;
	} bits;
	unsigned int dword;
    } dword4;
};

struct brw_drawrect {
    struct header header;
    unsigned int xmin:16;
    unsigned int ymin:16;
    unsigned int xmax:16;
    unsigned int ymax:16;
    unsigned int xorg:16;
    unsigned int yorg:16;
};

struct brw_global_depth_offset_clamp {
    struct header header;
    float depth_offset_clamp;
};

struct brw_indexbuffer {
    union {
	struct {
	    unsigned int length:8;
	    unsigned int index_format:2;
	    unsigned int cut_index_enable:1;
	    unsigned int pad:5;
	    unsigned int opcode:16;
	} bits;
	unsigned int dword;
    } header;
    unsigned int buffer_start;
    unsigned int buffer_end;
};


struct brw_line_stipple {
    struct header header;

    struct {
	unsigned int pattern:16;
	unsigned int pad:16;
    } bits0;

    struct {
	unsigned int repeat_count:9;
	unsigned int pad:7;
	unsigned int inverse_repeat_count:16;
    } bits1;
};

struct brw_pipelined_state_pointers {
    struct header header;

    struct {
	unsigned int pad:5;
	unsigned int offset:27;
    } vs;

    struct {
	unsigned int enable:1;
	unsigned int pad:4;
	unsigned int offset:27;
    } gs;

    struct {
	unsigned int enable:1;
	unsigned int pad:4;
	unsigned int offset:27;
    } clp;

    struct {
	unsigned int pad:5;
	unsigned int offset:27;
    } sf;

    struct {
	unsigned int pad:5;
	unsigned int offset:27;
    } wm;

    struct {
	unsigned int pad:6;
	unsigned int offset:26;
    } cc;
};

struct brw_polygon_stipple_offset {
    struct header header;

    struct {
	unsigned int y_offset:5;
	unsigned int pad:3;
	unsigned int x_offset:5;
	unsigned int pad0:19;
    } bits0;
};

struct brw_polygon_stipple {
    struct header header;
    unsigned int stipple[32];
};

struct brw_pipeline_select {
    struct {
	unsigned int pipeline_select:1;
	unsigned int pad:15;
	unsigned int opcode:16;
    } header;
};

struct brw_pipe_control {
    struct {
	unsigned int length:8;
	unsigned int notify_enable:1;
	unsigned int pad:2;
	unsigned int instruction_state_cache_flush_enable:1;
	unsigned int write_cache_flush_enable:1;
	unsigned int depth_stall_enable:1;
	unsigned int post_sync_operation:2;

	unsigned int opcode:16;
    } header;

    struct {
	unsigned int pad:2;
	unsigned int dest_addr_type:1;
	unsigned int dest_addr:29;
    } bits1;

    unsigned int data0;
    unsigned int data1;
};


struct brw_urb_fence {
    struct {
	unsigned int length:8;
	unsigned int vs_realloc:1;
	unsigned int gs_realloc:1;
	unsigned int clp_realloc:1;
	unsigned int sf_realloc:1;
	unsigned int vfe_realloc:1;
	unsigned int cs_realloc:1;
	unsigned int pad:2;
	unsigned int opcode:16;
    } header;

    struct {
	unsigned int vs_fence:10;
	unsigned int gs_fence:10;
	unsigned int clp_fence:10;
	unsigned int pad:2;
    } bits0;

    struct {
	unsigned int sf_fence:10;
	unsigned int vf_fence:10;
	unsigned int cs_fence:10;
	unsigned int pad:2;
    } bits1;
};

struct brw_constant_buffer_state {
    struct header header;

    struct {
	unsigned int nr_urb_entries:3;
	unsigned int pad:1;
	unsigned int urb_entry_size:5;
	unsigned int pad0:23;
    } bits0;
};

struct brw_constant_buffer {
    struct {
	unsigned int length:8;
	unsigned int valid:1;
	unsigned int pad:7;
	unsigned int opcode:16;
    } header;

    struct {
	unsigned int buffer_length:6;
	unsigned int buffer_address:26;
    } bits0;
};

struct brw_state_base_address {
    struct header header;

    struct {
	unsigned int modify_enable:1;
	unsigned int pad:4;
	unsigned int general_state_address:27;
    } bits0;

    struct {
	unsigned int modify_enable:1;
	unsigned int pad:4;
	unsigned int surface_state_address:27;
    } bits1;

    struct {
	unsigned int modify_enable:1;
	unsigned int pad:4;
	unsigned int indirect_object_state_address:27;
    } bits2;

    struct {
	unsigned int modify_enable:1;
	unsigned int pad:11;
	unsigned int general_state_upper_bound:20;
    } bits3;

    struct {
	unsigned int modify_enable:1;
	unsigned int pad:11;
	unsigned int indirect_object_state_upper_bound:20;
    } bits4;
};

struct brw_state_prefetch {
    struct header header;

    struct {
	unsigned int prefetch_count:3;
	unsigned int pad:3;
	unsigned int prefetch_pointer:26;
    } bits0;
};

struct brw_system_instruction_pointer {
    struct header header;

    struct {
	unsigned int pad:4;
	unsigned int system_instruction_pointer:28;
    } bits0;
};


/* State structs for the various fixed function units:
*/

struct thread0 {
    unsigned int pad0:1;
    unsigned int grf_reg_count:3;
    unsigned int pad1:2;
    unsigned int kernel_start_pointer:26;
};

struct thread1 {
    unsigned int ext_halt_exception_enable:1;
    unsigned int sw_exception_enable:1;
    unsigned int mask_stack_exception_enable:1;
    unsigned int timeout_exception_enable:1;
    unsigned int illegal_op_exception_enable:1;
    unsigned int pad0:3;
    unsigned int depth_coef_urb_read_offset:6;	/* WM only */
    unsigned int pad1:2;
    unsigned int floating_point_mode:1;
    unsigned int thread_priority:1;
    unsigned int binding_table_entry_count:8;
    unsigned int pad3:5;
    unsigned int single_program_flow:1;
};

struct thread2 {
    unsigned int per_thread_scratch_space:4;
    unsigned int pad0:6;
    unsigned int scratch_space_base_pointer:22;
};

struct thread3 {
    unsigned int dispatch_grf_start_reg:4;
    unsigned int urb_entry_read_offset:6;
    unsigned int pad0:1;
    unsigned int urb_entry_read_length:6;
    unsigned int pad1:1;
    unsigned int const_urb_entry_read_offset:6;
    unsigned int pad2:1;
    unsigned int const_urb_entry_read_length:6;
    unsigned int pad3:1;
};

struct brw_clip_unit_state {
    struct thread0 thread0;
    struct thread1 thread1;
    struct thread2 thread2;
    struct thread3 thread3;

    struct {
	unsigned int pad0:9;
	unsigned int gs_output_stats:1; /* not always */
	unsigned int stats_enable:1;
	unsigned int nr_urb_entries:7;
	unsigned int pad1:1;
	unsigned int urb_entry_allocation_size:5;
	unsigned int pad2:1;
	unsigned int max_threads:6;	/* may be less */
	unsigned int pad3:1;
    } thread4;

    struct {
	unsigned int pad0:13;
	unsigned int clip_mode:3;
	unsigned int userclip_enable_flags:8;
	unsigned int userclip_must_clip:1;
	unsigned int pad1:1;
	unsigned int guard_band_enable:1;
	unsigned int viewport_z_clip_enable:1;
	unsigned int viewport_xy_clip_enable:1;
	unsigned int vertex_position_space:1;
	unsigned int api_mode:1;
	unsigned int pad2:1;
    } clip5;

    struct {
	unsigned int pad0:5;
	unsigned int clipper_viewport_state_ptr:27;
    } clip6;

    float viewport_xmin;
    float viewport_xmax;
    float viewport_ymin;
    float viewport_ymax;
};

struct brw_cc_unit_state {
    struct {
	unsigned int pad0:3;
	unsigned int bf_stencil_pass_depth_pass_op:3;
	unsigned int bf_stencil_pass_depth_fail_op:3;
	unsigned int bf_stencil_fail_op:3;
	unsigned int bf_stencil_func:3;
	unsigned int bf_stencil_enable:1;
	unsigned int pad1:2;
	unsigned int stencil_write_enable:1;
	unsigned int stencil_pass_depth_pass_op:3;
	unsigned int stencil_pass_depth_fail_op:3;
	unsigned int stencil_fail_op:3;
	unsigned int stencil_func:3;
	unsigned int stencil_enable:1;
    } cc0;

    struct {
	unsigned int bf_stencil_ref:8;
	unsigned int stencil_write_mask:8;
	unsigned int stencil_test_mask:8;
	unsigned int stencil_ref:8;
    } cc1;

    struct {
	unsigned int logicop_enable:1;
	unsigned int pad0:10;
	unsigned int depth_write_enable:1;
	unsigned int depth_test_function:3;
	unsigned int depth_test:1;
	unsigned int bf_stencil_write_mask:8;
	unsigned int bf_stencil_test_mask:8;
    } cc2;

    struct {
	unsigned int pad0:8;
	unsigned int alpha_test_func:3;
	unsigned int alpha_test:1;
	unsigned int blend_enable:1;
	unsigned int ia_blend_enable:1;
	unsigned int pad1:1;
	unsigned int alpha_test_format:1;
	unsigned int pad2:16;
    } cc3;

    struct {
	unsigned int pad0:5;
	unsigned int cc_viewport_state_offset:27;
    } cc4;

    struct {
	unsigned int pad0:2;
	unsigned int ia_dest_blend_factor:5;
	unsigned int ia_src_blend_factor:5;
	unsigned int ia_blend_function:3;
	unsigned int statistics_enable:1;
	unsigned int logicop_func:4;
	unsigned int pad1:11;
	unsigned int dither_enable:1;
    } cc5;

    struct {
	unsigned int clamp_post_alpha_blend:1;
	unsigned int clamp_pre_alpha_blend:1;
	unsigned int clamp_range:2;
	unsigned int pad0:11;
	unsigned int y_dither_offset:2;
	unsigned int x_dither_offset:2;
	unsigned int dest_blend_factor:5;
	unsigned int src_blend_factor:5;
	unsigned int blend_function:3;
    } cc6;

    struct {
	union {
	    float f;
	    unsigned char ub[4];
	} alpha_ref;
    } cc7;
};

struct brw_sf_unit_state {
    struct thread0 thread0;
    struct {
	unsigned int pad0:7;
	unsigned int sw_exception_enable:1;
	unsigned int pad1:3;
	unsigned int mask_stack_exception_enable:1;
	unsigned int pad2:1;
	unsigned int illegal_op_exception_enable:1;
	unsigned int pad3:2;
	unsigned int floating_point_mode:1;
	unsigned int thread_priority:1;
	unsigned int binding_table_entry_count:8;
	unsigned int pad4:5;
	unsigned int single_program_flow:1;
    } sf1;

    struct thread2 thread2;
    struct thread3 thread3;

    struct {
	unsigned int pad0:10;
	unsigned int stats_enable:1;
	unsigned int nr_urb_entries:7;
	unsigned int pad1:1;
	unsigned int urb_entry_allocation_size:5;
	unsigned int pad2:1;
	unsigned int max_threads:6;
	unsigned int pad3:1;
    } thread4;

    struct {
	unsigned int front_winding:1;
	unsigned int viewport_transform:1;
	unsigned int pad0:3;
	unsigned int sf_viewport_state_offset:27;
    } sf5;

    struct {
	unsigned int pad0:9;
	unsigned int dest_org_vbias:4;
	unsigned int dest_org_hbias:4;
	unsigned int scissor:1;
	unsigned int disable_2x2_trifilter:1;
	unsigned int disable_zero_pix_trifilter:1;
	unsigned int point_rast_rule:2;
	unsigned int line_endcap_aa_region_width:2;
	unsigned int line_width:4;
	unsigned int fast_scissor_disable:1;
	unsigned int cull_mode:2;
	unsigned int aa_enable:1;
    } sf6;

    struct {
	unsigned int point_size:11;
	unsigned int use_point_size_state:1;
	unsigned int subpixel_precision:1;
	unsigned int sprite_point:1;
	unsigned int pad0:11;
	unsigned int trifan_pv:2;
	unsigned int linestrip_pv:2;
	unsigned int tristrip_pv:2;
	unsigned int line_last_pixel_enable:1;
    } sf7;
};

struct brw_gs_unit_state {
    struct thread0 thread0;
    struct thread1 thread1;
    struct thread2 thread2;
    struct thread3 thread3;

    struct {
	unsigned int pad0:10;
	unsigned int stats_enable:1;
	unsigned int nr_urb_entries:7;
	unsigned int pad1:1;
	unsigned int urb_entry_allocation_size:5;
	unsigned int pad2:1;
	unsigned int max_threads:1;
	unsigned int pad3:6;
    } thread4;

    struct {
	unsigned int sampler_count:3;
	unsigned int pad0:2;
	unsigned int sampler_state_pointer:27;
    } gs5;

    struct {
	unsigned int max_vp_index:4;
	unsigned int pad0:26;
	unsigned int reorder_enable:1;
	unsigned int pad1:1;
    } gs6;
};

struct brw_vs_unit_state {
    struct thread0 thread0;
    struct thread1 thread1;
    struct thread2 thread2;
    struct thread3 thread3;

    struct {
	unsigned int pad0:10;
	unsigned int stats_enable:1;
	unsigned int nr_urb_entries:7;
	unsigned int pad1:1;
	unsigned int urb_entry_allocation_size:5;
	unsigned int pad2:1;
	unsigned int max_threads:4;
	unsigned int pad3:3;
    } thread4;

    struct {
	unsigned int sampler_count:3;
	unsigned int pad0:2;
	unsigned int sampler_state_pointer:27;
    } vs5;

    struct {
	unsigned int vs_enable:1;
	unsigned int vert_cache_disable:1;
	unsigned int pad0:30;
    } vs6;
};

struct brw_wm_unit_state {
    struct thread0 thread0;
    struct thread1 thread1;
    struct thread2 thread2;
    struct thread3 thread3;

    struct {
	unsigned int stats_enable:1;
	unsigned int pad0:1;
	unsigned int sampler_count:3;
	unsigned int sampler_state_pointer:27;
    } wm4;

    struct {
	unsigned int enable_8_pix:1;
	unsigned int enable_16_pix:1;
	unsigned int enable_32_pix:1;
	unsigned int pad0:7;
	unsigned int legacy_global_depth_bias:1;
	unsigned int line_stipple:1;
	unsigned int depth_offset:1;
	unsigned int polygon_stipple:1;
	unsigned int line_aa_region_width:2;
	unsigned int line_endcap_aa_region_width:2;
	unsigned int early_depth_test:1;
	unsigned int thread_dispatch_enable:1;
	unsigned int program_uses_depth:1;
	unsigned int program_computes_depth:1;
	unsigned int program_uses_killpixel:1;
	unsigned int legacy_line_rast: 1;
	unsigned int transposed_urb_read:1;
	unsigned int max_threads:7;
    } wm5;

    float global_depth_offset_constant;
    float global_depth_offset_scale;
};

/* The hardware supports two different modes for border color. The
 * default (OpenGL) mode uses floating-point color channels, while the
 * legacy mode uses 4 bytes.
 *
 * More significantly, the legacy mode respects the components of the
 * border color for channels not present in the source, (whereas the
 * default mode will ignore the border color's alpha channel and use
 * alpha==1 for an RGB source, for example).
 *
 * The legacy mode matches the semantics specified by the Render
 * extension.
 */
struct brw_sampler_default_border_color {
    float color[4];
};

struct brw_sampler_legacy_border_color {
    uint8_t color[4];
};

struct brw_sampler_state {
    struct {
	unsigned int shadow_function:3;
	unsigned int lod_bias:11;
	unsigned int min_filter:3;
	unsigned int mag_filter:3;
	unsigned int mip_filter:2;
	unsigned int base_level:5;
	unsigned int pad:1;
	unsigned int lod_preclamp:1;
	unsigned int border_color_mode:1;
	unsigned int pad0:1;
	unsigned int disable:1;
    } ss0;

    struct {
	unsigned int r_wrap_mode:3;
	unsigned int t_wrap_mode:3;
	unsigned int s_wrap_mode:3;
	unsigned int pad:3;
	unsigned int max_lod:10;
	unsigned int min_lod:10;
    } ss1;

    struct {
	unsigned int pad:5;
	unsigned int border_color_pointer:27;
    } ss2;

    struct {
	unsigned int pad:19;
	unsigned int max_aniso:3;
	unsigned int chroma_key_mode:1;
	unsigned int chroma_key_index:2;
	unsigned int chroma_key_enable:1;
	unsigned int monochrome_filter_width:3;
	unsigned int monochrome_filter_height:3;
    } ss3;
};

struct brw_clipper_viewport {
    float xmin;
    float xmax;
    float ymin;
    float ymax;
};

struct brw_cc_viewport {
    float min_depth;
    float max_depth;
};

struct brw_sf_viewport {
    struct {
	float m00;
	float m11;
	float m22;
	float m30;
	float m31;
	float m32;
    } viewport;

    struct {
	short xmin;
	short ymin;
	short xmax;
	short ymax;
    } scissor;
};

/* Documented in the subsystem/shared-functions/sampler chapter...
*/
struct brw_surface_state {
    struct {
	unsigned int cube_pos_z:1;
	unsigned int cube_neg_z:1;
	unsigned int cube_pos_y:1;
	unsigned int cube_neg_y:1;
	unsigned int cube_pos_x:1;
	unsigned int cube_neg_x:1;
	unsigned int pad:3;
	unsigned int render_cache_read_mode:1;
	unsigned int mipmap_layout_mode:1;
	unsigned int vert_line_stride_ofs:1;
	unsigned int vert_line_stride:1;
	unsigned int color_blend:1;
	unsigned int writedisable_blue:1;
	unsigned int writedisable_green:1;
	unsigned int writedisable_red:1;
	unsigned int writedisable_alpha:1;
	unsigned int surface_format:9;
	unsigned int data_return_format:1;
	unsigned int pad0:1;
	unsigned int surface_type:3;
    } ss0;

    struct {
	unsigned int base_addr;
    } ss1;

    struct {
	unsigned int render_target_rotation:2;
	unsigned int mip_count:4;
	unsigned int width:13;
	unsigned int height:13;
    } ss2;

    struct {
	unsigned int tile_walk:1;
	unsigned int tiled_surface:1;
	unsigned int pad:1;
	unsigned int pitch:18;
	unsigned int depth:11;
    } ss3;

    struct {
	unsigned int pad:19;
	unsigned int min_array_elt:9;
	unsigned int min_lod:4;
    } ss4;

    struct {
	unsigned int pad:20;
	unsigned int y_offset:4;
	unsigned int pad2:1;
	unsigned int x_offset:7;
    } ss5;
};

struct brw_vertex_buffer_state {
    struct {
	unsigned int pitch:11;
	unsigned int pad:15;
	unsigned int access_type:1;
	unsigned int vb_index:5;
    } vb0;

    unsigned int start_addr;
    unsigned int max_index;
#if 1
    unsigned int instance_data_step_rate; /* not included for sequential/random vertices? */
#endif
};

#define BRW_VBP_MAX 17

struct brw_vb_array_state {
    struct header header;
    struct brw_vertex_buffer_state vb[BRW_VBP_MAX];
};

struct brw_vertex_element_state {
    struct {
	unsigned int src_offset:11;
	unsigned int pad:5;
	unsigned int src_format:9;
	unsigned int pad0:1;
	unsigned int valid:1;
	unsigned int vertex_buffer_index:5;
    } ve0;

    struct {
	unsigned int dst_offset:8;
	unsigned int pad:8;
	unsigned int vfcomponent3:4;
	unsigned int vfcomponent2:4;
	unsigned int vfcomponent1:4;
	unsigned int vfcomponent0:4;
    } ve1;
};

#define BRW_VEP_MAX 18

struct brw_vertex_element_packet {
    struct header header;
    struct brw_vertex_element_state ve[BRW_VEP_MAX];
};

struct brw_urb_immediate {
    unsigned int opcode:4;
    unsigned int offset:6;
    unsigned int swizzle_control:2;
    unsigned int pad:1;
    unsigned int allocate:1;
    unsigned int used:1;
    unsigned int complete:1;
    unsigned int response_length:4;
    unsigned int msg_length:4;
    unsigned int msg_target:4;
    unsigned int pad1:3;
    unsigned int end_of_thread:1;
};

/* Instruction format for the execution units: */

struct brw_instruction {
    struct {
	unsigned int opcode:7;
	unsigned int pad:1;
	unsigned int access_mode:1;
	unsigned int mask_control:1;
	unsigned int dependency_control:2;
	unsigned int compression_control:2;
	unsigned int thread_control:2;
	unsigned int predicate_control:4;
	unsigned int predicate_inverse:1;
	unsigned int execution_size:3;
	unsigned int destreg__conditonalmod:4; /* destreg - send, conditionalmod - others */
	unsigned int pad0:2;
	unsigned int debug_control:1;
	unsigned int saturate:1;
    } header;

    union {
	struct {
	    unsigned int dest_reg_file:2;
	    unsigned int dest_reg_type:3;
	    unsigned int src0_reg_file:2;
	    unsigned int src0_reg_type:3;
	    unsigned int src1_reg_file:2;
	    unsigned int src1_reg_type:3;
	    unsigned int pad:1;
	    unsigned int dest_subreg_nr:5;
	    unsigned int dest_reg_nr:8;
	    unsigned int dest_horiz_stride:2;
	    unsigned int dest_address_mode:1;
	} da1;

	struct {
	    unsigned int dest_reg_file:2;
	    unsigned int dest_reg_type:3;
	    unsigned int src0_reg_file:2;
	    unsigned int src0_reg_type:3;
	    unsigned int pad:6;
	    int dest_indirect_offset:10;	/* offset against the deref'd address reg */
	    unsigned int dest_subreg_nr:3; /* subnr for the address reg a0.x */
	    unsigned int dest_horiz_stride:2;
	    unsigned int dest_address_mode:1;
	} ia1;

	struct {
	    unsigned int dest_reg_file:2;
	    unsigned int dest_reg_type:3;
	    unsigned int src0_reg_file:2;
	    unsigned int src0_reg_type:3;
	    unsigned int src1_reg_file:2;
	    unsigned int src1_reg_type:3;
	    unsigned int pad0:1;
	    unsigned int dest_writemask:4;
	    unsigned int dest_subreg_nr:1;
	    unsigned int dest_reg_nr:8;
	    unsigned int pad1:2;
	    unsigned int dest_address_mode:1;
	} da16;

	struct {
	    unsigned int dest_reg_file:2;
	    unsigned int dest_reg_type:3;
	    unsigned int src0_reg_file:2;
	    unsigned int src0_reg_type:3;
	    unsigned int pad0:6;
	    unsigned int dest_writemask:4;
	    int dest_indirect_offset:6;
	    unsigned int dest_subreg_nr:3;
	    unsigned int pad1:2;
	    unsigned int dest_address_mode:1;
	} ia16;
    } bits1;


    union {
	struct {
	    unsigned int src0_subreg_nr:5;
	    unsigned int src0_reg_nr:8;
	    unsigned int src0_abs:1;
	    unsigned int src0_negate:1;
	    unsigned int src0_address_mode:1;
	    unsigned int src0_horiz_stride:2;
	    unsigned int src0_width:3;
	    unsigned int src0_vert_stride:4;
	    unsigned int flag_reg_nr:1;
	    unsigned int pad:6;
	} da1;

	struct {
	    int src0_indirect_offset:10;
	    unsigned int src0_subreg_nr:3;
	    unsigned int src0_abs:1;
	    unsigned int src0_negate:1;
	    unsigned int src0_address_mode:1;
	    unsigned int src0_horiz_stride:2;
	    unsigned int src0_width:3;
	    unsigned int src0_vert_stride:4;
	    unsigned int flag_reg_nr:1;
	    unsigned int pad:6;
	} ia1;

	struct {
	    unsigned int src0_swz_x:2;
	    unsigned int src0_swz_y:2;
	    unsigned int src0_subreg_nr:1;
	    unsigned int src0_reg_nr:8;
	    unsigned int src0_abs:1;
	    unsigned int src0_negate:1;
	    unsigned int src0_address_mode:1;
	    unsigned int src0_swz_z:2;
	    unsigned int src0_swz_w:2;
	    unsigned int pad0:1;
	    unsigned int src0_vert_stride:4;
	    unsigned int flag_reg_nr:1;
	    unsigned int pad1:6;
	} da16;

	struct {
	    unsigned int src0_swz_x:2;
	    unsigned int src0_swz_y:2;
	    int src0_indirect_offset:6;
	    unsigned int src0_subreg_nr:3;
	    unsigned int src0_abs:1;
	    unsigned int src0_negate:1;
	    unsigned int src0_address_mode:1;
	    unsigned int src0_swz_z:2;
	    unsigned int src0_swz_w:2;
	    unsigned int pad0:1;
	    unsigned int src0_vert_stride:4;
	    unsigned int flag_reg_nr:1;
	    unsigned int pad1:6;
	} ia16;

    } bits2;

    union {
	struct {
	    unsigned int src1_subreg_nr:5;
	    unsigned int src1_reg_nr:8;
	    unsigned int src1_abs:1;
	    unsigned int src1_negate:1;
	    unsigned int pad:1;
	    unsigned int src1_horiz_stride:2;
	    unsigned int src1_width:3;
	    unsigned int src1_vert_stride:4;
	    unsigned int pad0:7;
	} da1;

	struct {
	    unsigned int src1_swz_x:2;
	    unsigned int src1_swz_y:2;
	    unsigned int src1_subreg_nr:1;
	    unsigned int src1_reg_nr:8;
	    unsigned int src1_abs:1;
	    unsigned int src1_negate:1;
	    unsigned int pad0:1;
	    unsigned int src1_swz_z:2;
	    unsigned int src1_swz_w:2;
	    unsigned int pad1:1;
	    unsigned int src1_vert_stride:4;
	    unsigned int pad2:7;
	} da16;

	struct {
	    int src1_indirect_offset:10;
	    unsigned int src1_subreg_nr:3;
	    unsigned int src1_abs:1;
	    unsigned int src1_negate:1;
	    unsigned int pad0:1;
	    unsigned int src1_horiz_stride:2;
	    unsigned int src1_width:3;
	    unsigned int src1_vert_stride:4;
	    unsigned int flag_reg_nr:1;
	    unsigned int pad1:6;
	} ia1;

	struct {
	    unsigned int src1_swz_x:2;
	    unsigned int src1_swz_y:2;
	    int src1_indirect_offset:6;
	    unsigned int src1_subreg_nr:3;
	    unsigned int src1_abs:1;
	    unsigned int src1_negate:1;
	    unsigned int pad0:1;
	    unsigned int src1_swz_z:2;
	    unsigned int src1_swz_w:2;
	    unsigned int pad1:1;
	    unsigned int src1_vert_stride:4;
	    unsigned int flag_reg_nr:1;
	    unsigned int pad2:6;
	} ia16;

	struct {
	    int jump_count:16;	/* note: signed */
	    unsigned int pop_count:4;
	    unsigned int pad0:12;
	} if_else;

	struct {
	    unsigned int function:4;
	    unsigned int int_type:1;
	    unsigned int precision:1;
	    unsigned int saturate:1;
	    unsigned int data_type:1;
	    unsigned int pad0:8;
	    unsigned int response_length:4;
	    unsigned int msg_length:4;
	    unsigned int msg_target:4;
	    unsigned int pad1:3;
	    unsigned int end_of_thread:1;
	} math;

	struct {
	    unsigned int binding_table_index:8;
	    unsigned int sampler:4;
	    unsigned int return_format:2;
	    unsigned int msg_type:2;
	    unsigned int response_length:4;
	    unsigned int msg_length:4;
	    unsigned int msg_target:4;
	    unsigned int pad1:3;
	    unsigned int end_of_thread:1;
	} sampler;

	struct {
	    uint32_t binding_table_index:8;
	    uint32_t sampler:4;
	    uint32_t msg_type:4;
	    uint32_t response_length:4;
	    uint32_t msg_length:4;
	    uint32_t msg_target:4;
	    uint32_t pad1:3;
	    uint32_t end_of_thread:1;
	} sampler_g4x;

	struct brw_urb_immediate urb;

	struct {
	    unsigned int binding_table_index:8;
	    unsigned int msg_control:4;
	    unsigned int msg_type:2;
	    unsigned int target_cache:2;
	    unsigned int response_length:4;
	    unsigned int msg_length:4;
	    unsigned int msg_target:4;
	    unsigned int pad1:3;
	    unsigned int end_of_thread:1;
	} dp_read;

	struct {
	    unsigned int binding_table_index:8;
	    unsigned int msg_control:3;
	    unsigned int pixel_scoreboard_clear:1;
	    unsigned int msg_type:3;
	    unsigned int send_commit_msg:1;
	    unsigned int response_length:4;
	    unsigned int msg_length:4;
	    unsigned int msg_target:4;
	    unsigned int pad1:3;
	    unsigned int end_of_thread:1;
	} dp_write;

	struct {
	    unsigned int pad:16;
	    unsigned int response_length:4;
	    unsigned int msg_length:4;
	    unsigned int msg_target:4;
	    unsigned int pad1:3;
	    unsigned int end_of_thread:1;
	} generic;

	uint32_t ud;
	int32_t d;
    } bits3;
};

/* media pipeline */

struct brw_vfe_state {
    struct {
	unsigned int per_thread_scratch_space:4;
	unsigned int pad3:3;
	unsigned int extend_vfe_state_present:1;
	unsigned int pad2:2;
	unsigned int scratch_base:22;
    } vfe0;

    struct {
	unsigned int debug_counter_control:2;
	unsigned int children_present:1;
	unsigned int vfe_mode:4;
	unsigned int pad2:2;
	unsigned int num_urb_entries:7;
	unsigned int urb_entry_alloc_size:9;
	unsigned int max_threads:7;
    } vfe1;

    struct {
	unsigned int pad4:4;
	unsigned int interface_descriptor_base:28;
    } vfe2;
};

struct brw_vld_state {
    struct {
	unsigned int pad6:6;
	unsigned int scan_order:1;
	unsigned int intra_vlc_format:1;
	unsigned int quantizer_scale_type:1;
	unsigned int concealment_motion_vector:1;
	unsigned int frame_predict_frame_dct:1;
	unsigned int top_field_first:1;
	unsigned int picture_structure:2;
	unsigned int intra_dc_precision:2;
	unsigned int f_code_0_0:4;
	unsigned int f_code_0_1:4;
	unsigned int f_code_1_0:4;
	unsigned int f_code_1_1:4;
    } vld0;

    struct {
	unsigned int pad2:9;
	unsigned int picture_coding_type:2;
	unsigned int pad:21;
    } vld1;

    struct {
	unsigned int index_0:4;
	unsigned int index_1:4;
	unsigned int index_2:4;
	unsigned int index_3:4;
	unsigned int index_4:4;
	unsigned int index_5:4;
	unsigned int index_6:4;
	unsigned int index_7:4;
    } desc_remap_table0;

    struct {
	unsigned int index_8:4;
	unsigned int index_9:4;
	unsigned int index_10:4;
	unsigned int index_11:4;
	unsigned int index_12:4;
	unsigned int index_13:4;
	unsigned int index_14:4;
	unsigned int index_15:4;
    } desc_remap_table1;
};

struct brw_interface_descriptor {
    struct {
	unsigned int grf_reg_blocks:4;
	unsigned int pad:2;
	unsigned int kernel_start_pointer:26;
    } desc0;

    struct {
	unsigned int pad:7;
	unsigned int software_exception:1;
	unsigned int pad2:3;
	unsigned int maskstack_exception:1;
	unsigned int pad3:1;
	unsigned int illegal_opcode_exception:1;
	unsigned int pad4:2;
	unsigned int floating_point_mode:1;
	unsigned int thread_priority:1;
	unsigned int single_program_flow:1;
	unsigned int pad5:1;
	unsigned int const_urb_entry_read_offset:6;
	unsigned int const_urb_entry_read_len:6;
    } desc1;

    struct {
	unsigned int pad:2;
	unsigned int sampler_count:3;
	unsigned int sampler_state_pointer:27;
    } desc2;

    struct {
	unsigned int binding_table_entry_count:5;
	unsigned int binding_table_pointer:27;
    } desc3;
};

#endif

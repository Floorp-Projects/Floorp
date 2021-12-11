/*
   Copyright (C) Intel Corp.  2006.  All Rights Reserved.
   Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
   develop this 3D driver.

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice (including the
   next paragraph) shall be included in all copies or substantial
   portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
   LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
   WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **********************************************************************/
/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#ifndef CAIRO_DRM_INTEL_BRW_EU_H
#define CAIRO_DRM_INTEL_BRW_EU_H

#include "cairo.h"
#include "cairo-drm-intel-brw-structs.h"
#include "cairo-drm-intel-brw-defines.h"

#include <assert.h>


/*
 * Writemask values, 1 bit per component.
 */
#define WRITEMASK_X     0x1
#define WRITEMASK_Y     0x2
#define WRITEMASK_Z     0x4
#define WRITEMASK_W     0x8
#define WRITEMASK_XY    (WRITEMASK_X | WRITEMASK_Y)
#define WRITEMASK_XZ    (WRITEMASK_X | WRITEMASK_Z)
#define WRITEMASK_YZ    (WRITEMASK_Y | WRITEMASK_Z)
#define WRITEMASK_XYZ   (WRITEMASK_X | WRITEMASK_Y | WRITEMASK_Z)
#define WRITEMASK_XW    (WRITEMASK_X | WRITEMASK_W)
#define WRITEMASK_YW    (WRITEMASK_Y | WRITEMASK_W)
#define WRITEMASK_XYW   (WRITEMASK_X | WRITEMASK_Y | WRITEMASK_W)
#define WRITEMASK_ZW    (WRITEMASK_Z | WRITEMASK_W)
#define WRITEMASK_XZW   (WRITEMASK_X | WRITEMASK_Z | WRITEMASK_W)
#define WRITEMASK_YZW   (WRITEMASK_Y | WRITEMASK_Z | WRITEMASK_W)
#define WRITEMASK_XYZW  (WRITEMASK_X | WRITEMASK_Y | WRITEMASK_Z | WRITEMASK_W)

#define BRW_SWIZZLE4(a,b,c,d) (((a)<<0) | ((b)<<2) | ((c)<<4) | ((d)<<6))
#define BRW_GET_SWZ(swz, idx) (((swz) >> ((idx)*2)) & 0x3)

#define BRW_SWIZZLE_NOOP      BRW_SWIZZLE4 (0,1,2,3)
#define BRW_SWIZZLE_XYZW      BRW_SWIZZLE4 (0,1,2,3)
#define BRW_SWIZZLE_XXXX      BRW_SWIZZLE4 (0,0,0,0)
#define BRW_SWIZZLE_XYXY      BRW_SWIZZLE4 (0,1,0,1)

#define REG_SIZE (8*4)

/* These aren't hardware structs, just something useful for us to pass around:
 *
 * Align1 operation has a lot of control over input ranges.  Used in
 * WM programs to implement shaders decomposed into "channel serial"
 * or "structure of array" form:
 */
struct brw_reg {
   uint32_t type:4;
   uint32_t file:2;
   uint32_t nr:8;
   uint32_t subnr:5;		/* :1 in align16 */
   uint32_t negate:1;		/* source only */
   uint32_t abs:1;		/* source only */
   uint32_t vstride:4;		/* source only */
   uint32_t width:3;		/* src only, align1 only */
   uint32_t hstride:2;		/* align1 only */
   uint32_t address_mode:1;	/* relative addressing, hopefully! */
   uint32_t pad0:1;

   union {
      struct {
	 uint32_t swizzle:8;		/* src only, align16 only */
	 uint32_t writemask:4;		/* dest only, align16 only */
	 int32_t  indirect_offset:10;	/* relative addressing offset */
	 uint32_t pad1:10;		/* two dwords total */
      } bits;

      float f;
      int32_t   d;
      uint32_t ud;
   } dw1;
};

struct brw_indirect {
   uint32_t addr_subnr:4;
   int32_t addr_offset:10;
   uint32_t pad:18;
};

struct brw_glsl_label;
struct brw_glsl_call;

#define BRW_EU_MAX_INSN_STACK 5
#define BRW_EU_MAX_INSN 200

struct brw_compile {
   struct brw_instruction store[BRW_EU_MAX_INSN];
   uint32_t nr_insn;

   cairo_bool_t is_g4x;

   /* Allow clients to push/pop instruction state:
    */
   struct brw_instruction stack[BRW_EU_MAX_INSN_STACK];
   struct brw_instruction *current;

   uint32_t flag_value;
   int single_program_flow;
   struct brw_context *brw;

   struct brw_glsl_label *first_label;  /*< linked list of labels */
   struct brw_glsl_call *first_call;    /*< linked list of CALs */
};

cairo_private void
brw_save_label (struct brw_compile *c,
		const char *name,
		uint32_t position);

cairo_private void
brw_save_call (struct brw_compile *c,
	       const char *name,
	       uint32_t call_pos);

cairo_private void
brw_resolve_cals (struct brw_compile *c);

static cairo_always_inline int
type_sz (uint32_t type)
{
   switch (type) {
   case BRW_REGISTER_TYPE_UD:
   case BRW_REGISTER_TYPE_D:
   case BRW_REGISTER_TYPE_F:
      return 4;
   case BRW_REGISTER_TYPE_HF:
   case BRW_REGISTER_TYPE_UW:
   case BRW_REGISTER_TYPE_W:
      return 2;
   case BRW_REGISTER_TYPE_UB:
   case BRW_REGISTER_TYPE_B:
      return 1;
   default:
      return 0;
   }
}

/*
 * Construct a brw_reg.
 * \param file  one of the BRW_x_REGISTER_FILE values
 * \param nr  register number/index
 * \param subnr  register sub number
 * \param type  one of BRW_REGISTER_TYPE_x
 * \param vstride  one of BRW_VERTICAL_STRIDE_x
 * \param width  one of BRW_WIDTH_x
 * \param hstride  one of BRW_HORIZONTAL_STRIDE_x
 * \param swizzle  one of BRW_SWIZZLE_x
 * \param writemask  WRITEMASK_X/Y/Z/W bitfield
 */
static cairo_always_inline struct brw_reg
brw_reg (uint32_t file,
	 uint32_t nr,
	 uint32_t subnr,
	 uint32_t type,
	 uint32_t vstride,
	 uint32_t width,
	 uint32_t hstride,
	 uint32_t swizzle,
	 uint32_t writemask)
{
   struct brw_reg reg;

   if (type == BRW_GENERAL_REGISTER_FILE)
      assert(nr < 128);
   else if (type == BRW_MESSAGE_REGISTER_FILE)
      assert(nr < 9);
   else if (type == BRW_ARCHITECTURE_REGISTER_FILE)
      assert(nr <= BRW_ARF_IP);

   reg.type = type;
   reg.file = file;
   reg.nr = nr;
   reg.subnr = subnr * type_sz(type);
   reg.negate = 0;
   reg.abs = 0;
   reg.vstride = vstride;
   reg.width = width;
   reg.hstride = hstride;
   reg.address_mode = BRW_ADDRESS_DIRECT;
   reg.pad0 = 0;

   /* Could do better: If the reg is r5.3<0;1,0>, we probably want to
    * set swizzle and writemask to W, as the lower bits of subnr will
    * be lost when converted to align16.  This is probably too much to
    * keep track of as you'd want it adjusted by suboffset(), etc.
    * Perhaps fix up when converting to align16?
    */
   reg.dw1.bits.swizzle = swizzle;
   reg.dw1.bits.writemask = writemask;
   reg.dw1.bits.indirect_offset = 0;
   reg.dw1.bits.pad1 = 0;

   return reg;
}

/* Construct float[16] register */
static cairo_always_inline struct brw_reg
brw_vec16_reg (uint32_t file,
	       uint32_t nr,
	       uint32_t subnr)
{
    return brw_reg (file, nr, subnr,
		    BRW_REGISTER_TYPE_F,
		    BRW_VERTICAL_STRIDE_16,
		    BRW_WIDTH_16,
		    BRW_HORIZONTAL_STRIDE_1,
		    BRW_SWIZZLE_XYZW,
		    WRITEMASK_XYZW);
}

/* Construct float[8] register */
static cairo_always_inline struct brw_reg
brw_vec8_reg (uint32_t file,
	      uint32_t nr,
	      uint32_t subnr)
{
    return brw_reg (file, nr, subnr,
		    BRW_REGISTER_TYPE_F,
		    BRW_VERTICAL_STRIDE_8,
		    BRW_WIDTH_8,
		    BRW_HORIZONTAL_STRIDE_1,
		    BRW_SWIZZLE_XYZW,
		    WRITEMASK_XYZW);
}

/* Construct float[4] register */
static cairo_always_inline struct brw_reg
brw_vec4_reg (uint32_t file,
	      uint32_t nr,
	      uint32_t subnr)
{
   return brw_reg (file, nr, subnr,
		   BRW_REGISTER_TYPE_F,
		   BRW_VERTICAL_STRIDE_4,
		   BRW_WIDTH_4,
		   BRW_HORIZONTAL_STRIDE_1,
		   BRW_SWIZZLE_XYZW,
		   WRITEMASK_XYZW);
}

/* Construct float[2] register */
static cairo_always_inline struct brw_reg
brw_vec2_reg (uint32_t file,
	      uint32_t nr,
	      uint32_t subnr)
{
   return brw_reg (file, nr, subnr,
		   BRW_REGISTER_TYPE_F,
		   BRW_VERTICAL_STRIDE_2,
		   BRW_WIDTH_2,
		   BRW_HORIZONTAL_STRIDE_1,
		   BRW_SWIZZLE_XYXY,
		   WRITEMASK_XY);
}

/* Construct float[1] register */
static cairo_always_inline struct brw_reg
brw_vec1_reg (uint32_t file,
	      uint32_t nr,
	      uint32_t subnr)
{
   return brw_reg (file, nr, subnr,
		   BRW_REGISTER_TYPE_F,
		   BRW_VERTICAL_STRIDE_0,
		   BRW_WIDTH_1,
		   BRW_HORIZONTAL_STRIDE_0,
		   BRW_SWIZZLE_XXXX,
		   WRITEMASK_X);
}

static cairo_always_inline struct brw_reg
retype (struct brw_reg reg,
	uint32_t type)
{
   reg.type = type;
   return reg;
}

static cairo_always_inline struct brw_reg
suboffset (struct brw_reg reg,
	   uint32_t delta)
{
   reg.subnr += delta * type_sz (reg.type);
   return reg;
}

static cairo_always_inline struct brw_reg
offset (struct brw_reg reg,
	uint32_t delta)
{
   reg.nr += delta;
   return reg;
}

static cairo_always_inline struct brw_reg
byte_offset (struct brw_reg reg,
	     uint32_t bytes)
{
   uint32_t newoffset = reg.nr * REG_SIZE + reg.subnr + bytes;
   reg.nr = newoffset / REG_SIZE;
   reg.subnr = newoffset % REG_SIZE;
   return reg;
}

/* Construct unsigned word[16] register */
static cairo_always_inline struct brw_reg
brw_uw16_reg (uint32_t file,
	      uint32_t nr,
	      uint32_t subnr)
{
   return suboffset (retype (brw_vec16_reg (file, nr, 0), BRW_REGISTER_TYPE_UW), subnr);
}

/* Construct unsigned word[8] register */
static cairo_always_inline struct brw_reg
brw_uw8_reg (uint32_t file,
	     uint32_t nr,
	     uint32_t subnr)
{
   return suboffset (retype (brw_vec8_reg (file, nr, 0), BRW_REGISTER_TYPE_UW), subnr);
}

/* Construct unsigned word[2] register */
static cairo_always_inline struct brw_reg
brw_uw2_reg (uint32_t file,
	     uint32_t nr,
	     uint32_t subnr)
{
   return suboffset (retype (brw_vec2_reg (file, nr, 0), BRW_REGISTER_TYPE_UW), subnr);
}

/* Construct unsigned word[1] register */
static cairo_always_inline struct brw_reg
brw_uw1_reg (uint32_t file,
	     uint32_t nr,
	     uint32_t subnr)
{
   return suboffset (retype (brw_vec1_reg (file, nr, 0), BRW_REGISTER_TYPE_UW), subnr);
}

static cairo_always_inline struct brw_reg
brw_imm_reg (uint32_t type)
{
   return brw_reg (BRW_IMMEDIATE_VALUE,
		   0,
		   0,
		   type,
		   BRW_VERTICAL_STRIDE_0,
		   BRW_WIDTH_1,
		   BRW_HORIZONTAL_STRIDE_0,
		   0,
		   0);
}

/* Construct float immediate register */
static cairo_always_inline struct brw_reg brw_imm_f( float f )
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_F);
   imm.dw1.f = f;
   return imm;
}

/* Construct integer immediate register */
static cairo_always_inline struct brw_reg brw_imm_d( int32_t d )
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_D);
   imm.dw1.d = d;
   return imm;
}

/* Construct uint immediate register */
static cairo_always_inline struct brw_reg brw_imm_ud( uint32_t ud )
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_UD);
   imm.dw1.ud = ud;
   return imm;
}

/* Construct ushort immediate register */
static cairo_always_inline struct brw_reg brw_imm_uw( uint16_t uw )
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_UW);
   imm.dw1.ud = uw | (uw << 16);
   return imm;
}

/* Construct short immediate register */
static cairo_always_inline struct brw_reg brw_imm_w( int16_t w )
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_W);
   imm.dw1.d = w | (w << 16);
   return imm;
}

/* brw_imm_b and brw_imm_ub aren't supported by hardware - the type
 * numbers alias with _V and _VF below:
 */

/* Construct vector of eight signed half-byte values */
static cairo_always_inline
struct brw_reg brw_imm_v (uint32_t v)
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_V);
   imm.vstride = BRW_VERTICAL_STRIDE_0;
   imm.width = BRW_WIDTH_8;
   imm.hstride = BRW_HORIZONTAL_STRIDE_1;
   imm.dw1.ud = v;
   return imm;
}

/* Construct vector of four 8-bit float values */
static cairo_always_inline struct brw_reg
brw_imm_vf (uint32_t v)
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_VF);
   imm.vstride = BRW_VERTICAL_STRIDE_0;
   imm.width = BRW_WIDTH_4;
   imm.hstride = BRW_HORIZONTAL_STRIDE_1;
   imm.dw1.ud = v;
   return imm;
}

#define VF_ZERO 0x0
#define VF_ONE  0x30
#define VF_NEG  (1<<7)

static cairo_always_inline struct brw_reg
brw_imm_vf4 (uint32_t v0,
	     uint32_t v1,
	     uint32_t v2,
	     uint32_t v3)
{
   struct brw_reg imm = brw_imm_reg(BRW_REGISTER_TYPE_VF);
   imm.vstride = BRW_VERTICAL_STRIDE_0;
   imm.width = BRW_WIDTH_4;
   imm.hstride = BRW_HORIZONTAL_STRIDE_1;
   imm.dw1.ud = ((v0 << 0) |
		 (v1 << 8) |
		 (v2 << 16) |
		 (v3 << 24));
   return imm;
}

static cairo_always_inline struct brw_reg
brw_address (struct brw_reg reg)
{
   return brw_imm_uw (reg.nr * REG_SIZE + reg.subnr);
}

/* Construct float[1] general-purpose register */
static cairo_always_inline struct brw_reg
brw_vec1_grf (uint32_t nr, uint32_t subnr)
{
   return brw_vec1_reg (BRW_GENERAL_REGISTER_FILE, nr, subnr);
}

/* Construct float[2] general-purpose register */
static cairo_always_inline struct brw_reg
brw_vec2_grf (uint32_t nr, uint32_t subnr)
{
   return brw_vec2_reg (BRW_GENERAL_REGISTER_FILE, nr, subnr);
}

/* Construct float[4] general-purpose register */
static cairo_always_inline struct brw_reg
brw_vec4_grf (uint32_t nr, uint32_t subnr)
{
   return brw_vec4_reg (BRW_GENERAL_REGISTER_FILE, nr, subnr);
}

/* Construct float[8] general-purpose register */
static cairo_always_inline struct brw_reg
brw_vec8_grf (uint32_t nr)
{
   return brw_vec8_reg (BRW_GENERAL_REGISTER_FILE, nr, 0);
}

static cairo_always_inline struct brw_reg
brw_uw8_grf (uint32_t nr, uint32_t subnr)
{
   return brw_uw8_reg (BRW_GENERAL_REGISTER_FILE, nr, subnr);
}

static cairo_always_inline struct brw_reg
brw_uw16_grf (uint32_t nr, uint32_t subnr)
{
   return brw_uw16_reg (BRW_GENERAL_REGISTER_FILE, nr, subnr);
}

/* Construct null register (usually used for setting condition codes) */
static cairo_always_inline struct brw_reg
brw_null_reg (void)
{
   return brw_vec8_reg (BRW_ARCHITECTURE_REGISTER_FILE,
			BRW_ARF_NULL,
			0);
}

static cairo_always_inline struct brw_reg
brw_address_reg (uint32_t subnr)
{
   return brw_uw1_reg (BRW_ARCHITECTURE_REGISTER_FILE,
		       BRW_ARF_ADDRESS,
		       subnr);
}

/* If/else instructions break in align16 mode if writemask & swizzle
 * aren't xyzw.  This goes against the convention for other scalar
 * regs:
 */
static cairo_always_inline struct brw_reg
brw_ip_reg (void)
{
   return brw_reg (BRW_ARCHITECTURE_REGISTER_FILE,
		   BRW_ARF_IP,
		   0,
		   BRW_REGISTER_TYPE_UD,
		   BRW_VERTICAL_STRIDE_4, /* ? */
		   BRW_WIDTH_1,
		   BRW_HORIZONTAL_STRIDE_0,
		   BRW_SWIZZLE_XYZW,
		   WRITEMASK_XYZW);
}

static cairo_always_inline struct brw_reg
brw_acc_reg (void)
{
   return brw_vec8_reg (BRW_ARCHITECTURE_REGISTER_FILE,
			BRW_ARF_ACCUMULATOR,
			0);
}

static cairo_always_inline struct brw_reg
brw_flag_reg (void)
{
   return brw_uw1_reg (BRW_ARCHITECTURE_REGISTER_FILE,
		       BRW_ARF_FLAG,
		       0);
}

static cairo_always_inline struct brw_reg
brw_mask_reg (uint32_t subnr)
{
   return brw_uw1_reg (BRW_ARCHITECTURE_REGISTER_FILE,
		       BRW_ARF_MASK,
		       subnr);
}

static cairo_always_inline struct brw_reg
brw_message4_reg (uint32_t nr)
{
    return brw_vec4_reg (BRW_MESSAGE_REGISTER_FILE,
			 nr,
			 0);
}

static cairo_always_inline struct brw_reg
brw_message_reg (uint32_t nr)
{
   return brw_vec8_reg (BRW_MESSAGE_REGISTER_FILE,
			nr,
			0);
}

/* This is almost always called with a numeric constant argument, so
 * make things easy to evaluate at compile time:
 */
static cairo_always_inline uint32_t
cvt (uint32_t val)
{
   switch (val) {
   case 0: return 0;
   case 1: return 1;
   case 2: return 2;
   case 4: return 3;
   case 8: return 4;
   case 16: return 5;
   case 32: return 6;
   }
   return 0;
}

static cairo_always_inline struct brw_reg
stride (struct brw_reg reg,
	uint32_t vstride,
	uint32_t width,
	uint32_t hstride)
{
   reg.vstride = cvt (vstride);
   reg.width   = cvt (width) - 1;
   reg.hstride = cvt (hstride);
   return reg;
}

static cairo_always_inline struct brw_reg
vec16 (struct brw_reg reg)
{
   return stride (reg, 16,16,1);
}

static cairo_always_inline struct brw_reg
vec8 (struct brw_reg reg)
{
   return stride (reg, 8,8,1);
}

static cairo_always_inline struct brw_reg
vec4 (struct brw_reg reg)
{
   return stride (reg, 4,4,1);
}

static cairo_always_inline struct brw_reg
vec2 (struct brw_reg reg)
{
   return stride (reg, 2,2,1);
}

static cairo_always_inline struct brw_reg
vec1 (struct brw_reg reg)
{
   return stride (reg, 0,1,0);
}

static cairo_always_inline struct brw_reg
get_element (struct brw_reg reg, uint32_t elt)
{
   return vec1 (suboffset (reg, elt));
}

static cairo_always_inline struct brw_reg
get_element_ud (struct brw_reg reg, uint32_t elt)
{
   return vec1 (suboffset (retype (reg, BRW_REGISTER_TYPE_UD), elt));
}

static cairo_always_inline struct brw_reg
brw_swizzle (struct brw_reg reg,
	     uint32_t x,
	     uint32_t y,
	     uint32_t z,
	     uint32_t w)
{
    reg.dw1.bits.swizzle = BRW_SWIZZLE4 (BRW_GET_SWZ (reg.dw1.bits.swizzle, x),
					 BRW_GET_SWZ (reg.dw1.bits.swizzle, y),
					 BRW_GET_SWZ (reg.dw1.bits.swizzle, z),
					 BRW_GET_SWZ (reg.dw1.bits.swizzle, w));
   return reg;
}

static cairo_always_inline struct brw_reg
brw_swizzle1 (struct brw_reg reg,
	      uint32_t x)
{
   return brw_swizzle (reg, x, x, x, x);
}

static cairo_always_inline struct brw_reg
brw_writemask (struct brw_reg reg,
	       uint32_t mask)
{
   reg.dw1.bits.writemask &= mask;
   return reg;
}

static cairo_always_inline struct brw_reg
brw_set_writemask (struct brw_reg reg,
		   uint32_t mask)
{
   reg.dw1.bits.writemask = mask;
   return reg;
}

static cairo_always_inline struct brw_reg
negate (struct brw_reg reg)
{
   reg.negate ^= 1;
   return reg;
}

static cairo_always_inline struct brw_reg
brw_abs (struct brw_reg reg)
{
   reg.abs = 1;
   return reg;
}

static cairo_always_inline struct brw_reg
brw_vec4_indirect (uint32_t subnr,
		   int32_t offset)
{
   struct brw_reg reg = brw_vec4_grf (0, 0);
   reg.subnr = subnr;
   reg.address_mode = BRW_ADDRESS_REGISTER_INDIRECT_REGISTER;
   reg.dw1.bits.indirect_offset = offset;
   return reg;
}

static cairo_always_inline struct brw_reg
brw_vec1_indirect (uint32_t subnr,
		   int32_t offset)
{
   struct brw_reg reg = brw_vec1_grf (0, 0);
   reg.subnr = subnr;
   reg.address_mode = BRW_ADDRESS_REGISTER_INDIRECT_REGISTER;
   reg.dw1.bits.indirect_offset = offset;
   return reg;
}

static cairo_always_inline struct brw_reg
deref_4f (struct brw_indirect ptr, int32_t offset)
{
   return brw_vec4_indirect (ptr.addr_subnr, ptr.addr_offset + offset);
}

static cairo_always_inline struct brw_reg
deref_1f(struct brw_indirect ptr, int32_t offset)
{
   return brw_vec1_indirect (ptr.addr_subnr, ptr.addr_offset + offset);
}

static cairo_always_inline struct brw_reg
deref_4b(struct brw_indirect ptr, int32_t offset)
{
   return retype (deref_4f (ptr, offset), BRW_REGISTER_TYPE_B);
}

static cairo_always_inline struct brw_reg
deref_1uw(struct brw_indirect ptr, int32_t offset)
{
   return retype (deref_1f (ptr, offset), BRW_REGISTER_TYPE_UW);
}

static cairo_always_inline struct brw_reg
deref_1d (struct brw_indirect ptr, int32_t offset)
{
   return retype (deref_1f (ptr, offset), BRW_REGISTER_TYPE_D);
}

static cairo_always_inline struct brw_reg
deref_1ud (struct brw_indirect ptr, int32_t offset)
{
   return retype (deref_1f (ptr, offset), BRW_REGISTER_TYPE_UD);
}

static cairo_always_inline struct brw_reg
get_addr_reg (struct brw_indirect ptr)
{
   return brw_address_reg (ptr.addr_subnr);
}

static cairo_always_inline struct brw_indirect
brw_indirect_offset (struct brw_indirect ptr, int32_t offset)
{
   ptr.addr_offset += offset;
   return ptr;
}

static cairo_always_inline struct brw_indirect
brw_indirect (uint32_t addr_subnr, int32_t offset)
{
   struct brw_indirect ptr;
   ptr.addr_subnr = addr_subnr;
   ptr.addr_offset = offset;
   ptr.pad = 0;
   return ptr;
}

static cairo_always_inline struct brw_instruction *
current_insn (struct brw_compile *p)
{
   return &p->store[p->nr_insn];
}

cairo_private void brw_pop_insn_state (struct brw_compile *p);
cairo_private void brw_push_insn_state (struct brw_compile *p);
cairo_private void brw_set_mask_control (struct brw_compile *p, uint32_t value);
cairo_private void brw_set_saturate (struct brw_compile *p, uint32_t value);
cairo_private void brw_set_access_mode (struct brw_compile *p, uint32_t access_mode);
cairo_private void brw_set_compression_control (struct brw_compile *p, int control);
cairo_private void brw_set_predicate_control_flag_value (struct brw_compile *p, uint32_t value);
cairo_private void brw_set_predicate_control (struct brw_compile *p, uint32_t pc);
cairo_private void brw_set_conditionalmod (struct brw_compile *p, uint32_t conditional);

cairo_private void
brw_compile_init (struct brw_compile *p,
		  cairo_bool_t is_g4x);
cairo_private const uint32_t *brw_get_program (struct brw_compile *p, uint32_t *sz);

/* Helpers for regular instructions:
 */
#define ALU1(OP)					\
cairo_private_no_warn struct brw_instruction * \
brw_##OP(struct brw_compile *p,	\
	 struct brw_reg dest,			\
	 struct brw_reg src0);

#define ALU2(OP)					\
cairo_private_no_warn struct brw_instruction * \
brw_##OP(struct brw_compile *p,	\
	 struct brw_reg dest,			\
	 struct brw_reg src0,			\
	 struct brw_reg src1);

ALU1(MOV)
ALU2(SEL)
ALU1(NOT)
ALU2(AND)
ALU2(OR)
ALU2(XOR)
ALU2(SHR)
ALU2(SHL)
ALU2(RSR)
ALU2(RSL)
ALU2(ASR)
ALU2(JMPI)
ALU2(ADD)
ALU2(MUL)
ALU1(FRC)
ALU1(RNDD)
ALU1(RNDZ)
ALU2(MAC)
ALU2(MACH)
ALU1(LZD)
ALU2(DP4)
ALU2(DPH)
ALU2(DP3)
ALU2(DP2)
ALU2(LINE)

#undef ALU1
#undef ALU2

/* Helpers for SEND instruction: */
cairo_private void
brw_urb_WRITE (struct brw_compile *p,
	       struct brw_reg dest,
	       uint32_t msg_reg_nr,
	       struct brw_reg src0,
	       int allocate,
	       int used,
	       uint32_t msg_length,
	       uint32_t response_length,
	       int eot,
	       int writes_complete,
	       uint32_t offset,
	       uint32_t swizzle);

cairo_private void
brw_fb_WRITE (struct brw_compile *p,
	      struct brw_reg dest,
	      uint32_t msg_reg_nr,
	      struct brw_reg src0,
	      uint32_t binding_table_index,
	      uint32_t msg_length,
	      uint32_t response_length,
	      int eot);

cairo_private void
brw_SAMPLE (struct brw_compile *p,
	    struct brw_reg dest,
	    uint32_t msg_reg_nr,
	    struct brw_reg src0,
	    uint32_t binding_table_index,
	    uint32_t sampler,
	    uint32_t writemask,
	    uint32_t msg_type,
	    uint32_t response_length,
	    uint32_t msg_length,
	    cairo_bool_t eot);

cairo_private void
brw_math_16 (struct brw_compile *p,
	     struct brw_reg dest,
	     uint32_t function,
	     uint32_t saturate,
	     uint32_t msg_reg_nr,
	     struct brw_reg src,
	     uint32_t precision);

cairo_private void
brw_math (struct brw_compile *p,
	  struct brw_reg dest,
	  uint32_t function,
	  uint32_t saturate,
	  uint32_t msg_reg_nr,
	  struct brw_reg src,
	  uint32_t data_type,
	  uint32_t precision);

cairo_private void
brw_dp_READ_16 (struct brw_compile *p,
		struct brw_reg dest,
		uint32_t msg_reg_nr,
		uint32_t scratch_offset);

cairo_private void
brw_dp_WRITE_16 (struct brw_compile *p,
		 struct brw_reg src,
		 uint32_t msg_reg_nr,
		 uint32_t scratch_offset);

/* If/else/endif.  Works by manipulating the execution flags on each
 * channel.
 */
cairo_private struct brw_instruction *
brw_IF (struct brw_compile *p,
	uint32_t execute_size);

cairo_private struct brw_instruction *
brw_ELSE (struct brw_compile *p,
	  struct brw_instruction *if_insn);

cairo_private void
brw_ENDIF (struct brw_compile *p,
	   struct brw_instruction *if_or_else_insn);


/* DO/WHILE loops: */
cairo_private struct brw_instruction *
brw_DO (struct brw_compile *p,
	uint32_t execute_size);

cairo_private struct brw_instruction *
brw_WHILE (struct brw_compile *p,
	   struct brw_instruction *patch_insn);

cairo_private struct brw_instruction *
brw_BREAK (struct brw_compile *p);

cairo_private struct brw_instruction *
brw_CONT (struct brw_compile *p);

/* Forward jumps: */
cairo_private void
brw_land_fwd_jump (struct brw_compile *p,
		  struct brw_instruction *jmp_insn);

cairo_private void
brw_NOP (struct brw_compile *p);

/* Special case: there is never a destination, execution size will be
 * taken from src0:
 */
cairo_private void
brw_CMP (struct brw_compile *p,
	 struct brw_reg dest,
	 uint32_t conditional,
	 struct brw_reg src0,
	 struct brw_reg src1);

cairo_private void
brw_print_reg (struct brw_reg reg);

cairo_private struct brw_instruction *
brw_next_instruction (struct brw_compile *p,
		      uint32_t opcode);

cairo_private void
brw_instruction_set_destination (struct brw_instruction *insn,
				 struct brw_reg dest);

cairo_private void
brw_instruction_set_source0 (struct brw_instruction *insn,
			     struct brw_reg reg);

cairo_private void
brw_instruction_set_dp_write_message (struct brw_instruction *insn,
				      uint32_t binding_table_index,
				      uint32_t msg_control,
				      uint32_t msg_type,
				      uint32_t msg_length,
				      uint32_t pixel_scoreboard_clear,
				      uint32_t response_length,
				      uint32_t end_of_thread);

/***********************************************************************
 * brw_eu_util.c:
 */

cairo_private void
brw_copy_indirect_to_indirect (struct brw_compile *p,
			       struct brw_indirect dst_ptr,
			       struct brw_indirect src_ptr,
			       uint32_t count);

cairo_private void
brw_copy_from_indirect (struct brw_compile *p,
		       struct brw_reg dst,
		       struct brw_indirect ptr,
		       uint32_t count);

cairo_private void
brw_copy4 (struct brw_compile *p,
	   struct brw_reg dst,
	   struct brw_reg src,
	   uint32_t count);

cairo_private void
brw_copy8 (struct brw_compile *p,
	   struct brw_reg dst,
	   struct brw_reg src,
	   uint32_t count);

cairo_private void
brw_math_invert (struct brw_compile *p,
		 struct brw_reg dst,
		 struct brw_reg src);

cairo_private void
brw_set_src1 (struct brw_instruction *insn,
	      struct brw_reg reg);

#endif

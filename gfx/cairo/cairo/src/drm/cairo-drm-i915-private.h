/*
 * Copyright © 2006, 2009 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *    Chris Wilson <chris@chris-wilson.co.uk>
 */

#ifndef CAIRO_DRM_I915_PRIVATE_H
#define CAIRO_DRM_I915_PRIVATE_H

#include "cairo-types-private.h"

#include "cairo-drm-private.h"
#include "cairo-drm-intel-private.h"
#include "cairo-drm-intel-command-private.h"
#include "cairo-drm-intel-ioctl-private.h"
#include "cairo-freelist-private.h"

#include <setjmp.h>

#define I915_VERBOSE 1

#define I915_MAX_TEX_INDIRECT 4
#define I915_MAX_TEX_INSN     32
#define I915_MAX_ALU_INSN     64
#define I915_MAX_DECL_INSN    27
#define I915_MAX_TEMPORARY    16

/* Each instruction is 3 dwords long, though most don't require all
 * this space.  Maximum of 123 instructions.  Smaller maxes per insn
 * type.
 */
#define _3DSTATE_PIXEL_SHADER_PROGRAM    (CMD_3D|(0x1d<<24)|(0x5<<16))

#define REG_TYPE_R                 0 /* temporary regs, no need to
				      * dcl, must be written before
				      * read -- Preserved between
				      * phases.
				      */
#define REG_TYPE_T                 1 /* Interpolated values, must be
				      * dcl'ed before use.
				      *
				      * 0..7: texture coord,
				      * 8: diffuse spec,
				      * 9: specular color,
				      * 10: fog parameter in w.
				      */
#define REG_TYPE_CONST             2 /* Restriction: only one const
				      * can be referenced per
				      * instruction, though it may be
				      * selected for multiple inputs.
				      * Constants not initialized
				      * default to zero.
				      */
#define REG_TYPE_S                 3 /* sampler */
#define REG_TYPE_OC                4 /* output color (rgba) */
#define REG_TYPE_OD                5 /* output depth (w), xyz are
				      * temporaries.  If not written,
				      * interpolated depth is used?
				      */
#define REG_TYPE_U                 6 /* unpreserved temporaries */
#define REG_TYPE_MASK              0x7
#define REG_TYPE_SHIFT		   4
#define REG_NR_MASK                0xf

/* REG_TYPE_T:
 */
#define T_TEX0     0
#define T_TEX1     1
#define T_TEX2     2
#define T_TEX3     3
#define T_TEX4     4
#define T_TEX5     5
#define T_TEX6     6
#define T_TEX7     7
#define T_DIFFUSE  8
#define T_SPECULAR 9
#define T_FOG_W    10		/* interpolated fog is in W coord */

/* Arithmetic instructions */

/* .replicate_swizzle == selection and replication of a particular
 * scalar channel, ie., .xxxx, .yyyy, .zzzz or .wwww
 */
#define A0_NOP    (0x0<<24)		/* no operation */
#define A0_ADD    (0x1<<24)		/* dst = src0 + src1 */
#define A0_MOV    (0x2<<24)		/* dst = src0 */
#define A0_MUL    (0x3<<24)		/* dst = src0 * src1 */
#define A0_MAD    (0x4<<24)		/* dst = src0 * src1 + src2 */
#define A0_DP2ADD (0x5<<24)		/* dst.xyzw = src0.xy dot src1.xy + src2.replicate_swizzle */
#define A0_DP3    (0x6<<24)		/* dst.xyzw = src0.xyz dot src1.xyz */
#define A0_DP4    (0x7<<24)		/* dst.xyzw = src0.xyzw dot src1.xyzw */
#define A0_FRC    (0x8<<24)		/* dst = src0 - floor(src0) */
#define A0_RCP    (0x9<<24)		/* dst.xyzw = 1/(src0.replicate_swizzle) */
#define A0_RSQ    (0xa<<24)		/* dst.xyzw = 1/(sqrt(abs(src0.replicate_swizzle))) */
#define A0_EXP    (0xb<<24)		/* dst.xyzw = exp2(src0.replicate_swizzle) */
#define A0_LOG    (0xc<<24)		/* dst.xyzw = log2(abs(src0.replicate_swizzle)) */
#define A0_CMP    (0xd<<24)		/* dst = (src0 >= 0.0) ? src1 : src2 */
#define A0_MIN    (0xe<<24)		/* dst = (src0 < src1) ? src0 : src1 */
#define A0_MAX    (0xf<<24)		/* dst = (src0 >= src1) ? src0 : src1 */
#define A0_FLR    (0x10<<24)		/* dst = floor(src0) */
#define A0_MOD    (0x11<<24)		/* dst = src0 fmod 1.0 */
#define A0_TRC    (0x12<<24)		/* dst = int(src0) */
#define A0_SGE    (0x13<<24)		/* dst = src0 >= src1 ? 1.0 : 0.0 */
#define A0_SLT    (0x14<<24)		/* dst = src0 < src1 ? 1.0 : 0.0 */
#define A0_DEST_SATURATE                 (1<<22)
#define A0_DEST_TYPE_SHIFT                19
/* Allow: R, OC, OD, U */
#define A0_DEST_NR_SHIFT                 14
/* Allow R: 0..15, OC,OD: 0..0, U: 0..2 */
#define A0_DEST_CHANNEL_X                (1<<10)
#define A0_DEST_CHANNEL_Y                (2<<10)
#define A0_DEST_CHANNEL_Z                (4<<10)
#define A0_DEST_CHANNEL_W                (8<<10)
#define A0_DEST_CHANNEL_ALL              (0xf<<10)
#define A0_DEST_CHANNEL_SHIFT            10
#define A0_SRC0_TYPE_SHIFT               7
#define A0_SRC0_NR_SHIFT                 2

#define A0_DEST_CHANNEL_XY              (A0_DEST_CHANNEL_X|A0_DEST_CHANNEL_Y)
#define A0_DEST_CHANNEL_XYZ             (A0_DEST_CHANNEL_XY|A0_DEST_CHANNEL_Z)

#define SRC_X        0
#define SRC_Y        1
#define SRC_Z        2
#define SRC_W        3
#define SRC_ZERO     4
#define SRC_ONE      5

#define A1_SRC0_CHANNEL_X_NEGATE         ((int)(1u<<31))
#define A1_SRC0_CHANNEL_X_SHIFT          28
#define A1_SRC0_CHANNEL_Y_NEGATE         (1<<27)
#define A1_SRC0_CHANNEL_Y_SHIFT          24
#define A1_SRC0_CHANNEL_Z_NEGATE         (1<<23)
#define A1_SRC0_CHANNEL_Z_SHIFT          20
#define A1_SRC0_CHANNEL_W_NEGATE         (1<<19)
#define A1_SRC0_CHANNEL_W_SHIFT          16
#define A1_SRC1_TYPE_SHIFT               13
#define A1_SRC1_NR_SHIFT                 8
#define A1_SRC1_CHANNEL_X_NEGATE         (1<<7)
#define A1_SRC1_CHANNEL_X_SHIFT          4
#define A1_SRC1_CHANNEL_Y_NEGATE         (1<<3)
#define A1_SRC1_CHANNEL_Y_SHIFT          0

#define A2_SRC1_CHANNEL_Z_NEGATE         ((int)(1u<<31))
#define A2_SRC1_CHANNEL_Z_SHIFT          28
#define A2_SRC1_CHANNEL_W_NEGATE         (1<<27)
#define A2_SRC1_CHANNEL_W_SHIFT          24
#define A2_SRC2_TYPE_SHIFT               21
#define A2_SRC2_NR_SHIFT                 16
#define A2_SRC2_CHANNEL_X_NEGATE         (1<<15)
#define A2_SRC2_CHANNEL_X_SHIFT          12
#define A2_SRC2_CHANNEL_Y_NEGATE         (1<<11)
#define A2_SRC2_CHANNEL_Y_SHIFT          8
#define A2_SRC2_CHANNEL_Z_NEGATE         (1<<7)
#define A2_SRC2_CHANNEL_Z_SHIFT          4
#define A2_SRC2_CHANNEL_W_NEGATE         (1<<3)
#define A2_SRC2_CHANNEL_W_SHIFT          0

/* Texture instructions */
#define T0_TEXLD     (0x15<<24)	/* Sample texture using predeclared
				 * sampler and address, and output
				 * filtered texel data to destination
				 * register */
#define T0_TEXLDP    (0x16<<24)	/* Same as texld but performs a
				 * perspective divide of the texture
				 * coordinate .xyz values by .w before
				 * sampling. */
#define T0_TEXLDB    (0x17<<24)	/* Same as texld but biases the
				 * computed LOD by w.  Only S4.6 two's
				 * comp is used.  This implies that a
				 * float to fixed conversion is
				 * done. */
#define T0_TEXKILL   (0x18<<24)	/* Does not perform a sampling
				 * operation.  Simply kills the pixel
				 * if any channel of the address
				 * register is < 0.0. */
#define T0_DEST_TYPE_SHIFT                19
/* Allow: R, OC, OD, U */
/* Note: U (unpreserved) regs do not retain their values between
 * phases (cannot be used for feedback)
 *
 * Note: oC and OD registers can only be used as the destination of a
 * texture instruction once per phase (this is an implementation
 * restriction).
 */
#define T0_DEST_NR_SHIFT                 14
/* Allow R: 0..15, OC,OD: 0..0, U: 0..2 */
#define T0_SAMPLER_NR_SHIFT              0 /* This field ignored for TEXKILL */
#define T0_SAMPLER_NR_MASK               (0xf<<0)

#define T1_ADDRESS_REG_TYPE_SHIFT        24 /* Reg to use as texture coord */
/* Allow R, T, OC, OD -- R, OC, OD are 'dependent' reads, new program phase */
#define T1_ADDRESS_REG_NR_SHIFT          17
#define T2_MBZ                           0

/* Declaration instructions */
#define D0_DCL       (0x19<<24)	/* Declare a t (interpolated attrib)
				 * register or an s (sampler)
				 * register. */
#define D0_SAMPLE_TYPE_SHIFT              22
#define D0_SAMPLE_TYPE_2D                 (0x0<<22)
#define D0_SAMPLE_TYPE_CUBE               (0x1<<22)
#define D0_SAMPLE_TYPE_VOLUME             (0x2<<22)
#define D0_SAMPLE_TYPE_MASK               (0x3<<22)

#define D0_TYPE_SHIFT                19
/* Allow: T, S */
#define D0_NR_SHIFT                  14
/* Allow T: 0..10, S: 0..15 */
#define D0_CHANNEL_X                (1<<10)
#define D0_CHANNEL_Y                (2<<10)
#define D0_CHANNEL_Z                (4<<10)
#define D0_CHANNEL_W                (8<<10)
#define D0_CHANNEL_ALL              (0xf<<10)
#define D0_CHANNEL_NONE             (0<<10)

#define D0_CHANNEL_XY               (D0_CHANNEL_X|D0_CHANNEL_Y)
#define D0_CHANNEL_XYZ              (D0_CHANNEL_XY|D0_CHANNEL_Z)

/* I915 Errata: Do not allow (xz), (xw), (xzw) combinations for diffuse
 * or specular declarations.
 *
 * For T dcls, only allow: (x), (xy), (xyz), (w), (xyzw)
 *
 * Must be zero for S (sampler) dcls
 */
#define D1_MBZ                          0
#define D2_MBZ                          0


/* MASK_* are the unshifted bitmasks of the destination mask in arithmetic
 * operations
 */
#define MASK_X			0x1
#define MASK_Y			0x2
#define MASK_Z			0x4
#define MASK_W			0x8
#define MASK_XYZ		(MASK_X | MASK_Y | MASK_Z)
#define MASK_XYZW		(MASK_XYZ | MASK_W)
#define MASK_SATURATE		0x10

/* Temporary, undeclared regs. Preserved between phases */
#define FS_R0			((REG_TYPE_R << REG_TYPE_SHIFT) | 0)
#define FS_R1			((REG_TYPE_R << REG_TYPE_SHIFT) | 1)
#define FS_R2			((REG_TYPE_R << REG_TYPE_SHIFT) | 2)
#define FS_R3			((REG_TYPE_R << REG_TYPE_SHIFT) | 3)

/* Texture coordinate regs.  Must be declared. */
#define FS_T0			((REG_TYPE_T << REG_TYPE_SHIFT) | 0)
#define FS_T1			((REG_TYPE_T << REG_TYPE_SHIFT) | 1)
#define FS_T2			((REG_TYPE_T << REG_TYPE_SHIFT) | 2)
#define FS_T3			((REG_TYPE_T << REG_TYPE_SHIFT) | 3)
#define FS_T4			((REG_TYPE_T << REG_TYPE_SHIFT) | 4)
#define FS_T5			((REG_TYPE_T << REG_TYPE_SHIFT) | 5)
#define FS_T6			((REG_TYPE_T << REG_TYPE_SHIFT) | 6)
#define FS_T7			((REG_TYPE_T << REG_TYPE_SHIFT) | 7)
#define FS_T8			((REG_TYPE_T << REG_TYPE_SHIFT) | 8)
#define FS_T9			((REG_TYPE_T << REG_TYPE_SHIFT) | 9)
#define FS_T10			((REG_TYPE_T << REG_TYPE_SHIFT) | 10)

/* Constant values */
#define FS_C0			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 0)
#define FS_C1			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 1)
#define FS_C2			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 2)
#define FS_C3			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 3)
#define FS_C4			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 4)
#define FS_C5			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 5)
#define FS_C6			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 6)
#define FS_C7			((REG_TYPE_CONST << REG_TYPE_SHIFT) | 7)

/* Sampler regs */
#define FS_S0			((REG_TYPE_S << REG_TYPE_SHIFT) | 0)
#define FS_S1			((REG_TYPE_S << REG_TYPE_SHIFT) | 1)
#define FS_S2			((REG_TYPE_S << REG_TYPE_SHIFT) | 2)
#define FS_S3			((REG_TYPE_S << REG_TYPE_SHIFT) | 3)

/* Output color */
#define FS_OC			((REG_TYPE_OC << REG_TYPE_SHIFT) | 0)

/* Output depth */
#define FS_OD			((REG_TYPE_OD << REG_TYPE_SHIFT) | 0)

/* Unpreserved temporary regs */
#define FS_U0			((REG_TYPE_U << REG_TYPE_SHIFT) | 0)
#define FS_U1			((REG_TYPE_U << REG_TYPE_SHIFT) | 1)
#define FS_U2			((REG_TYPE_U << REG_TYPE_SHIFT) | 2)
#define FS_U3			((REG_TYPE_U << REG_TYPE_SHIFT) | 3)

#define X_CHANNEL_SHIFT (REG_TYPE_SHIFT + 3)
#define Y_CHANNEL_SHIFT (X_CHANNEL_SHIFT + 4)
#define Z_CHANNEL_SHIFT (Y_CHANNEL_SHIFT + 4)
#define W_CHANNEL_SHIFT (Z_CHANNEL_SHIFT + 4)

#define REG_CHANNEL_MASK 0xf

#define REG_NR(reg)		((reg) & REG_NR_MASK)
#define REG_TYPE(reg)		(((reg) >> REG_TYPE_SHIFT) & REG_TYPE_MASK)
#define REG_X(reg)		(((reg) >> X_CHANNEL_SHIFT) & REG_CHANNEL_MASK)
#define REG_Y(reg)		(((reg) >> Y_CHANNEL_SHIFT) & REG_CHANNEL_MASK)
#define REG_Z(reg)		(((reg) >> Z_CHANNEL_SHIFT) & REG_CHANNEL_MASK)
#define REG_W(reg)		(((reg) >> W_CHANNEL_SHIFT) & REG_CHANNEL_MASK)

enum i915_fs_channel {
    X_CHANNEL_VAL = 0,
    Y_CHANNEL_VAL,
    Z_CHANNEL_VAL,
    W_CHANNEL_VAL,
    ZERO_CHANNEL_VAL,
    ONE_CHANNEL_VAL,

    NEG_X_CHANNEL_VAL = X_CHANNEL_VAL | 0x8,
    NEG_Y_CHANNEL_VAL = Y_CHANNEL_VAL | 0x8,
    NEG_Z_CHANNEL_VAL = Z_CHANNEL_VAL | 0x8,
    NEG_W_CHANNEL_VAL = W_CHANNEL_VAL | 0x8,
    NEG_ONE_CHANNEL_VAL = ONE_CHANNEL_VAL | 0x8
};

#define i915_fs_operand(reg, x, y, z, w) \
    (reg) | \
    (x##_CHANNEL_VAL << X_CHANNEL_SHIFT) | \
    (y##_CHANNEL_VAL << Y_CHANNEL_SHIFT) | \
    (z##_CHANNEL_VAL << Z_CHANNEL_SHIFT) | \
    (w##_CHANNEL_VAL << W_CHANNEL_SHIFT)

/*
 * Construct an operand description for using a register with no swizzling
 */
#define i915_fs_operand_reg(reg)					\
    i915_fs_operand(reg, X, Y, Z, W)

#define i915_fs_operand_reg_negate(reg)					\
    i915_fs_operand(reg, NEG_X, NEG_Y, NEG_Z, NEG_W)

/*
 * Returns an operand containing (0.0, 0.0, 0.0, 0.0).
 */
#define i915_fs_operand_zero() i915_fs_operand(FS_R0, ZERO, ZERO, ZERO, ZERO)

/*
 * Returns an unused operand
 */
#define i915_fs_operand_none() i915_fs_operand_zero()

/*
 * Returns an operand containing (1.0, 1.0, 1.0, 1.0).
 */
#define i915_fs_operand_one() i915_fs_operand(FS_R0, ONE, ONE, ONE, ONE)

#define i915_get_hardware_channel_val(val, shift, negate) \
    (((val & 0x7) << shift) | ((val & 0x8) ? negate : 0))

/*
 * Outputs a fragment shader command to declare a sampler or texture register.
 */
#define i915_fs_dcl(reg)						\
do {									\
    OUT_DWORD (D0_DCL | \
	       (REG_TYPE(reg) << D0_TYPE_SHIFT) | \
	       (REG_NR(reg) << D0_NR_SHIFT) | \
               ((REG_TYPE(reg) != REG_TYPE_S) ? D0_CHANNEL_ALL : 0)); \
    OUT_DWORD (0); \
    OUT_DWORD (0); \
} while (0)

#define i915_fs_texld(dest_reg, sampler_reg, address_reg)		\
do {									\
    OUT_DWORD (T0_TEXLD | \
               (REG_TYPE(dest_reg) << T0_DEST_TYPE_SHIFT) | \
               (REG_NR(dest_reg) << T0_DEST_NR_SHIFT) | \
               (REG_NR(sampler_reg) << T0_SAMPLER_NR_SHIFT)); \
    OUT_DWORD((REG_TYPE(address_reg) << T1_ADDRESS_REG_TYPE_SHIFT) | \
              (REG_NR(address_reg) << T1_ADDRESS_REG_NR_SHIFT)); \
    OUT_DWORD (0); \
} while (0)

#define i915_fs_arith_masked(op, dest_reg, dest_mask, operand0, operand1, operand2)	\
    _i915_fs_arith_masked(A0_##op, dest_reg, dest_mask, operand0, operand1, operand2)

#define i915_fs_arith(op, dest_reg, operand0, operand1, operand2)	\
    _i915_fs_arith(A0_##op, dest_reg, operand0, operand1, operand2)

#define _i915_fs_arith_masked(cmd, dest_reg, dest_mask, operand0, operand1, operand2) \
do { \
    /* Set up destination register and write mask */ \
    OUT_DWORD (cmd | \
               (REG_TYPE(dest_reg) << A0_DEST_TYPE_SHIFT) | \
	       (REG_NR(dest_reg) << A0_DEST_NR_SHIFT) | \
               (((dest_mask) & ~MASK_SATURATE) << A0_DEST_CHANNEL_SHIFT) | \
               (((dest_mask) & MASK_SATURATE) ? A0_DEST_SATURATE : 0) | \
               /* Set up operand 0 */ \
	       (REG_TYPE(operand0) << A0_SRC0_TYPE_SHIFT) | \
	       (REG_NR(operand0) << A0_SRC0_NR_SHIFT)); \
    OUT_DWORD (i915_get_hardware_channel_val(REG_X(operand0), \
					      A1_SRC0_CHANNEL_X_SHIFT, \
					      A1_SRC0_CHANNEL_X_NEGATE) | \
               i915_get_hardware_channel_val(REG_Y(operand0), \
					      A1_SRC0_CHANNEL_Y_SHIFT, \
					      A1_SRC0_CHANNEL_Y_NEGATE) | \
               i915_get_hardware_channel_val(REG_Z(operand0), \
					      A1_SRC0_CHANNEL_Z_SHIFT, \
					      A1_SRC0_CHANNEL_Z_NEGATE) | \
               i915_get_hardware_channel_val(REG_W(operand0), \
					      A1_SRC0_CHANNEL_W_SHIFT, \
					      A1_SRC0_CHANNEL_W_NEGATE) | \
               /* Set up operand 1 */ \
               (REG_TYPE(operand1) << A1_SRC1_TYPE_SHIFT) | \
               (REG_NR(operand1) << A1_SRC1_NR_SHIFT) | \
               i915_get_hardware_channel_val(REG_X(operand1), \
					      A1_SRC1_CHANNEL_X_SHIFT, \
					      A1_SRC1_CHANNEL_X_NEGATE) | \
               i915_get_hardware_channel_val(REG_Y(operand1), \
					      A1_SRC1_CHANNEL_Y_SHIFT, \
					      A1_SRC1_CHANNEL_Y_NEGATE)); \
    OUT_DWORD (i915_get_hardware_channel_val(REG_Z(operand1), \
					      A2_SRC1_CHANNEL_Z_SHIFT, \
					      A2_SRC1_CHANNEL_Z_NEGATE) | \
	       i915_get_hardware_channel_val(REG_W(operand1), \
		                             A2_SRC1_CHANNEL_W_SHIFT, \
					     A2_SRC1_CHANNEL_W_NEGATE) | \
               /* Set up operand 2 */ \
               (REG_TYPE(operand2) << A2_SRC2_TYPE_SHIFT) | \
               (REG_NR(operand2) << A2_SRC2_NR_SHIFT) | \
               i915_get_hardware_channel_val(REG_X(operand2), \
					      A2_SRC2_CHANNEL_X_SHIFT, \
					      A2_SRC2_CHANNEL_X_NEGATE) | \
               i915_get_hardware_channel_val(REG_Y(operand2), \
					      A2_SRC2_CHANNEL_Y_SHIFT, \
					      A2_SRC2_CHANNEL_Y_NEGATE) | \
               i915_get_hardware_channel_val(REG_Z(operand2), \
					      A2_SRC2_CHANNEL_Z_SHIFT, \
					      A2_SRC2_CHANNEL_Z_NEGATE) | \
               i915_get_hardware_channel_val(REG_W(operand2), \
					       A2_SRC2_CHANNEL_W_SHIFT, \
					       A2_SRC2_CHANNEL_W_NEGATE)); \
} while (0)

#define _i915_fs_arith(cmd, dest_reg, operand0, operand1, operand2) do {\
    /* Set up destination register and write mask */ \
    OUT_DWORD (cmd | \
               (REG_TYPE(dest_reg) << A0_DEST_TYPE_SHIFT) | \
	       (REG_NR(dest_reg) << A0_DEST_NR_SHIFT) | \
	       (A0_DEST_CHANNEL_ALL) | \
               /* Set up operand 0 */ \
	       (REG_TYPE(operand0) << A0_SRC0_TYPE_SHIFT) | \
	       (REG_NR(operand0) << A0_SRC0_NR_SHIFT)); \
    OUT_DWORD (i915_get_hardware_channel_val(REG_X(operand0), \
					      A1_SRC0_CHANNEL_X_SHIFT, \
					      A1_SRC0_CHANNEL_X_NEGATE) | \
               i915_get_hardware_channel_val(REG_Y(operand0), \
					      A1_SRC0_CHANNEL_Y_SHIFT, \
					      A1_SRC0_CHANNEL_Y_NEGATE) | \
               i915_get_hardware_channel_val(REG_Z(operand0), \
					      A1_SRC0_CHANNEL_Z_SHIFT, \
					      A1_SRC0_CHANNEL_Z_NEGATE) | \
               i915_get_hardware_channel_val(REG_W(operand0), \
					      A1_SRC0_CHANNEL_W_SHIFT, \
					      A1_SRC0_CHANNEL_W_NEGATE) | \
               /* Set up operand 1 */ \
               (REG_TYPE(operand1) << A1_SRC1_TYPE_SHIFT) | \
               (REG_NR(operand1) << A1_SRC1_NR_SHIFT) | \
               i915_get_hardware_channel_val(REG_X(operand1), \
					      A1_SRC1_CHANNEL_X_SHIFT, \
					      A1_SRC1_CHANNEL_X_NEGATE) | \
               i915_get_hardware_channel_val(REG_Y(operand1), \
					      A1_SRC1_CHANNEL_Y_SHIFT, \
					      A1_SRC1_CHANNEL_Y_NEGATE)); \
    OUT_DWORD (i915_get_hardware_channel_val(REG_Z(operand1), \
					      A2_SRC1_CHANNEL_Z_SHIFT, \
					      A2_SRC1_CHANNEL_Z_NEGATE) | \
	       i915_get_hardware_channel_val(REG_W(operand1), \
		                             A2_SRC1_CHANNEL_W_SHIFT, \
					     A2_SRC1_CHANNEL_W_NEGATE) | \
               /* Set up operand 2 */ \
               (REG_TYPE(operand2) << A2_SRC2_TYPE_SHIFT) | \
               (REG_NR(operand2) << A2_SRC2_NR_SHIFT) | \
               i915_get_hardware_channel_val(REG_X(operand2), \
					      A2_SRC2_CHANNEL_X_SHIFT, \
					      A2_SRC2_CHANNEL_X_NEGATE) | \
               i915_get_hardware_channel_val(REG_Y(operand2), \
					      A2_SRC2_CHANNEL_Y_SHIFT, \
					      A2_SRC2_CHANNEL_Y_NEGATE) | \
               i915_get_hardware_channel_val(REG_Z(operand2), \
					      A2_SRC2_CHANNEL_Z_SHIFT, \
					      A2_SRC2_CHANNEL_Z_NEGATE) | \
               i915_get_hardware_channel_val(REG_W(operand2), \
					       A2_SRC2_CHANNEL_W_SHIFT, \
					       A2_SRC2_CHANNEL_W_NEGATE)); \
} while (0)

#define i915_fs_mov(dest_reg, operand0)					\
    i915_fs_arith(MOV, dest_reg, \
	          operand0,			\
	          i915_fs_operand_none(),			\
	          i915_fs_operand_none())

#define i915_fs_mov_masked(dest_reg, dest_mask, operand0)		\
    i915_fs_arith_masked (MOV, dest_reg, dest_mask, \
	                  operand0, \
	                  i915_fs_operand_none(), \
	                  i915_fs_operand_none())


#define i915_fs_frc(dest_reg, operand0)					\
    i915_fs_arith (FRC, dest_reg, \
	           operand0,			\
	           i915_fs_operand_none(),			\
	           i915_fs_operand_none())

/* Add operand0 and operand1 and put the result in dest_reg */
#define i915_fs_add(dest_reg, operand0, operand1)			\
    i915_fs_arith (ADD, dest_reg, \
	           operand0, operand1,	\
		   i915_fs_operand_none())

/* Multiply operand0 and operand1 and put the result in dest_reg */
#define i915_fs_mul(dest_reg, operand0, operand1)			\
    i915_fs_arith (MUL, dest_reg, \
	           operand0, operand1,	\
		   i915_fs_operand_none())

/* Computes 1/sqrt(operand0.replicate_swizzle) puts the result in dest_reg */
#define i915_fs_rsq(dest_reg, dest_mask, operand0)		\
do {									\
    if (dest_mask) {							\
	i915_fs_arith_masked (RSQ, dest_reg, dest_mask, \
		              operand0,			\
			      i915_fs_operand_none (),			\
			      i915_fs_operand_none ());			\
    } else { \
	i915_fs_arith (RSQ, dest_reg, \
		       operand0, \
		       i915_fs_operand_none (), \
		       i915_fs_operand_none ()); \
    } \
} while (0)

/* Puts the minimum of operand0 and operand1 in dest_reg */
#define i915_fs_min(dest_reg, operand0, operand1)			\
    i915_fs_arith (MIN, dest_reg, \
	           operand0, operand1, \
		   i915_fs_operand_none())

/* Puts the maximum of operand0 and operand1 in dest_reg */
#define i915_fs_max(dest_reg, operand0, operand1)			\
    i915_fs_arith (MAX, dest_reg, \
	           operand0, operand1, \
	           i915_fs_operand_none())

#define i915_fs_cmp(dest_reg, operand0, operand1, operand2)		\
    i915_fs_arith (CMP, dest_reg, operand0, operand1, operand2)

/* Perform operand0 * operand1 + operand2 and put the result in dest_reg */
#define i915_fs_mad(dest_reg, dest_mask, op0, op1, op2)	\
do {									\
    if (dest_mask) {							\
	i915_fs_arith_masked (MAD, dest_reg, dest_mask, op0, op1, op2); \
    } else { \
	i915_fs_arith (MAD, dest_reg, op0, op1, op2); \
    } \
} while (0)

#define i915_fs_dp2add(dest_reg, dest_mask, op0, op1, op2)	\
do {									\
    if (dest_mask) {							\
	i915_fs_arith_masked (DP2ADD, dest_reg, dest_mask, op0, op1, op2); \
    } else { \
	i915_fs_arith (DP2ADD, dest_reg, op0, op1, op2); \
    } \
} while (0)

/*
 * Perform a 3-component dot-product of operand0 and operand1 and put the
 * resulting scalar in the channels of dest_reg specified by the dest_mask.
 */
#define i915_fs_dp3(dest_reg, dest_mask, op0, op1)	\
do {									\
    if (dest_mask) {							\
	i915_fs_arith_masked (DP3, dest_reg, dest_mask, \
		              op0, op1,\
		              i915_fs_operand_none());			\
    } else { \
	i915_fs_arith (DP3, dest_reg, op0, op1,\
		       i915_fs_operand_none());			\
    } \
} while (0)

static inline uint32_t cairo_const
i915_fs_operand_pure_alpha (int pure)
{
    if (pure & (1 << 3))
	return i915_fs_operand_one ();
    else
	return i915_fs_operand_zero ();
}

#define I915_TILING_DEFAULT I915_TILING_Y
#define I915_BO_CACHE_BUCKETS 13 /* cache surfaces up to 16 MiB */

typedef struct i915_surface i915_surface_t;
typedef struct i915_device i915_device_t;
typedef struct i915_shader i915_shader_t;

typedef void (*i915_add_rectangle_func_t) (const i915_shader_t *shader,
					   int x, int y,
					   int w, int h);

#define IMAGE_CACHE_WIDTH 1024
#define IMAGE_CACHE_HEIGHT 1024

typedef struct i915_image_private {
    cairo_rtree_node_t node;
    intel_buffer_cache_t *container;
} i915_image_private_t;

#define I915_BATCH_SIZE (128*1024)
#define I915_VBO_SIZE (512*1024)
#define I915_MAX_RELOCS 2048

enum {
    I915_DEBUG_EXEC = 0x1,
    I915_DEBUG_SYNC = 0x2,
    I915_DEBUG_BATCH = 0x4,
    I915_DEBUG_BUFFER = 0x8,
    I915_DEBUG_BUFFER_CACHE = 0x10,
    I915_DEBUG_BUFFER_ALLOC = 0x20,
    I915_DEBUG_GLYPHS = 0x40,
    I915_DEBUG_MAP = 0x80,
    I915_DEBUG_THROTTLE = 0x100,
};

struct i915_device {
    intel_device_t intel;

    cairo_bool_t debug;

    i915_shader_t *shader; /* note: only valid during geometry emission */

    struct i915_batch {
	intel_bo_t *target_bo[I915_MAX_RELOCS];
	size_t gtt_avail_size;
	size_t est_gtt_size;
	size_t total_gtt_size;

	uint16_t fences;
	uint16_t fences_avail;
	uint16_t reloc_count;
	uint16_t exec_count;
	uint16_t used;

	struct drm_i915_gem_exec_object2 exec[I915_MAX_RELOCS];
	struct drm_i915_gem_relocation_entry reloc[I915_MAX_RELOCS];
    } batch;

    uint32_t vbo;
    uint32_t vbo_offset;
    uint32_t vbo_used;
    uint32_t vbo_max_index;
    uint32_t vertex_index;
    uint32_t vertex_count;
    uint32_t floats_per_vertex;
    uint32_t rectangle_size;
    intel_bo_t *last_vbo;
    uint32_t last_vbo_offset;
    uint32_t last_vbo_space;

    i915_surface_t *current_target;
    uint32_t current_size;
    uint32_t current_diffuse;
    uint32_t current_colorbuf;
    uint32_t *current_source;
    uint32_t *current_mask;
    uint32_t *current_clip;
    uint32_t current_program;
    uint32_t current_texcoords;
    uint32_t current_blend;
    uint32_t current_constants[8*4];
    uint32_t current_n_constants;
    uint32_t current_samplers[2*4];
    uint32_t current_maps[4*4];
    uint32_t current_n_samplers;
    uint32_t current_n_maps;
    uint32_t last_source_fragment;
    uint32_t clear_alpha;

    cairo_list_t image_caches[2];

    uint32_t batch_header[13];
    uint32_t batch_base[I915_BATCH_SIZE / sizeof (uint32_t)];
    uint8_t vbo_base[I915_VBO_SIZE];
};

enum {
    CURRENT_SOURCE = 0x1,
    CURRENT_MASK = 0x2,
    CURRENT_CLIP = 0x4
};

typedef enum {
    VS_ZERO,
    VS_CONSTANT,
    VS_LINEAR,
    VS_TEXTURE,
    VS_TEXTURE_16,
} i915_vertex_shader_t;

typedef enum {
    FS_ZERO,
    FS_ONE,
    FS_PURE,
    FS_CONSTANT,
    FS_DIFFUSE,
    FS_LINEAR,
    FS_RADIAL,
    FS_TEXTURE,
    FS_YUV,
    FS_SPANS,
} i915_fragment_shader_t;

#define FS_DETAILS_SHIFT 4

typedef enum {
    PATTERN_BASE,
    PATTERN_CONSTANT,
    PATTERN_LINEAR,
    PATTERN_RADIAL,
    PATTERN_TEXTURE,
} i915_shader_channel_t;

struct i915_surface {
    intel_surface_t intel;

    uint32_t map0, map1;
    uint32_t colorbuf;

    cairo_bool_t deferred_clear;
    uint32_t offset;
    uint32_t is_current_texture;

    i915_image_private_t *cache;

    intel_bo_t *stencil;
    uint32_t stencil_stride;
    uint32_t stencil_offset;
};

typedef enum {
    NONE = 0,
    YUV_I420,
    /* XXX */
    YUV_YV12,
    YUV_YUY2,
    YUV_UYVY,
} i915_packed_pixel_t;

/* read-only container */
#define I915_PACKED_PIXEL_SURFACE_TYPE 0x1000
typedef struct i915_packed_pixel_surface {
    cairo_surface_t base;

    i915_packed_pixel_t pixel;

    i915_device_t *device;
    intel_bo_t *bo;
    uint32_t is_current_texture;

    uint32_t offset[4];
    uint32_t stride[4];
    uint32_t width[4];
    uint32_t height[4];
    uint32_t map0[4], map1[4];
} i915_packed_pixel_surface_t;

struct i915_shader {
    i915_device_t *device;
    i915_surface_t *target;

    cairo_operator_t op;
    uint32_t blend;
    float opacity;
    cairo_content_t content;

    cairo_bool_t committed;
    cairo_bool_t need_combine;

    i915_add_rectangle_func_t add_rectangle;

    union i915_shader_channel {
	struct {
	    i915_vertex_shader_t vertex;
	    i915_fragment_shader_t fragment;
	    i915_shader_channel_t pattern;
	} type;
	struct i915_shader_base {
	    i915_vertex_shader_t vertex;
	    i915_fragment_shader_t fragment;
	    i915_shader_channel_t pattern;
	    uint32_t texfmt;
	    cairo_content_t content;
	    uint32_t mode;
	    intel_bo_t *bo;
	    uint32_t n_samplers;
	    uint32_t offset[4];
	    uint32_t map[2*4];
	    uint32_t sampler[2];
	    cairo_matrix_t matrix;
	} base;
	struct i915_shader_solid {
	    struct i915_shader_base base;
	    cairo_color_t color;
	    int pure;
	} solid;
	struct i915_shader_linear {
	    struct i915_shader_base base;
	    struct {
		float red, green, blue, alpha;
	    } color0, color1;
	    float dx, dy, offset;
	} linear;
	struct i915_shader_radial {
	    struct i915_shader_base base;
	    float constants[8];
	} radial;
	struct i915_shader_surface {
	    struct i915_shader_base base;
	    i915_packed_pixel_t pixel;
	} surface;
    } source, mask, clip, dst;

    jmp_buf unwind;
};

enum i915_shader_linear_mode {
    /* XXX REFLECT */
    LINEAR_TEXTURE,
    LINEAR_NONE,
    LINEAR_REPEAT,
    LINEAR_PAD,
};

enum i915_shader_radial_mode {
    RADIAL_ONE,
    RADIAL_TWO
};

typedef cairo_status_t
(*i915_spans_func_t) (void			*closure,
		      cairo_span_renderer_t	*renderer,
		      const cairo_rectangle_int_t	*extents);

cairo_private cairo_status_t
i915_clip_and_composite_spans (i915_surface_t		*dst,
			       cairo_operator_t		 op,
			       const cairo_pattern_t	*pattern,
			       cairo_antialias_t	 antialias,
			       i915_spans_func_t	 draw_func,
			       void			*draw_closure,
			       const cairo_composite_rectangles_t*extents,
			       cairo_clip_t		*clip,
			       double			 opacity);

cairo_private cairo_surface_t *
i915_surface_create_internal (cairo_drm_device_t *base_dev,
		              cairo_format_t format,
			      int width, int height,
			      uint32_t tiling,
			      cairo_bool_t gpu_target);

cairo_private i915_surface_t *
i915_surface_create_from_cacheable_image_internal (i915_device_t *device,
						   cairo_image_surface_t *image);

cairo_private void
i915_surface_scaled_font_fini (cairo_scaled_font_t *scaled_font);

cairo_private cairo_int_status_t
i915_surface_glyphs (void			*abstract_surface,
		     cairo_operator_t		 op,
		     const cairo_pattern_t	*source,
		     cairo_glyph_t		*glyphs,
		     int			 num_glyphs,
		     cairo_scaled_font_t	*scaled_font,
		     cairo_clip_t		*clip,
		     int *num_remaining);

static inline int cairo_const
i915_tiling_height (uint32_t tiling, int height)
{
    switch (tiling) {
    default:
    case I915_TILING_NONE: return (height + 1) & -2;
    case I915_TILING_X: return (height + 7) & -8;
    case I915_TILING_Y: return (height + 31) & -32;
    }
}

static inline uint32_t cairo_const
i915_tiling_stride (int format, uint32_t stride)
{
    uint32_t tile_width;

    /* use 64B alignment so that the buffer may be used as a scanout */
    if (format == I915_TILING_NONE)
	return (stride + 63) & -64;

    tile_width = 512;
    /* XXX Currently the kernel enforces a tile_width of 512 for TILING_Y.

       <jbarnes> the docs are a bit confused on that front
       <jbarnes> once we enable it on 915 we'll find out what the tile width size should be in the fence setup
       <jbarnes> it could be that 915 has y tiling but that the minimum width is 512 or something
       <jbarnes> yeah it's probably 128 on 915 also
       <jbarnes> it's just that we haven't tested
       <jbarnes> but I wasn't thinking that the tile widths were the same
       <jbarnes> only that in order to fence y tiles on 915 you needed pitch to be a multiple of 4 y tiles (or something like that)

       tile_width = format == I915_TILING_Y ? 128 : 512;
    */

    /* needs a pot tile width */
    while (tile_width < stride)
	tile_width <<= 1;

    return tile_width;
}

static inline uint32_t cairo_const
i915_tiling_size (uint32_t tiling, uint32_t size)
{
    uint32_t fence;

    if (tiling == I915_TILING_NONE)
	return (size + 4095) & -4096;

    fence = 1024 * 1024; /* 1 MiB */
    while (fence < size)
	fence <<= 1;

    return fence;
}

static inline cairo_bool_t cairo_const
i915_texture_filter_is_nearest (cairo_filter_t filter)
{
    switch (filter) {
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BILINEAR:
    case CAIRO_FILTER_GAUSSIAN:
	return FALSE;
    default:
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	return TRUE;
    }
}

static inline uint32_t cairo_const
i915_texture_filter (cairo_filter_t filter)
{
    switch (filter) {
    case CAIRO_FILTER_BEST:
    case CAIRO_FILTER_GOOD:
    case CAIRO_FILTER_BILINEAR:
    case CAIRO_FILTER_GAUSSIAN:
        return
	    (FILTER_LINEAR << SS2_MAG_FILTER_SHIFT) |
	    (FILTER_LINEAR << SS2_MIN_FILTER_SHIFT);
    default:
    case CAIRO_FILTER_FAST:
    case CAIRO_FILTER_NEAREST:
	return
	    (FILTER_NEAREST << SS2_MAG_FILTER_SHIFT) |
	    (FILTER_NEAREST << SS2_MIN_FILTER_SHIFT);
    }
}

static inline uint32_t cairo_const
i915_texture_extend (cairo_extend_t extend)
{
    switch (extend) {
    default:
    case CAIRO_EXTEND_NONE:
	return
	    (TEXCOORDMODE_CLAMP_BORDER << SS3_TCX_ADDR_MODE_SHIFT) |
	    (TEXCOORDMODE_CLAMP_BORDER << SS3_TCY_ADDR_MODE_SHIFT);
    case CAIRO_EXTEND_REPEAT:
	return
	    (TEXCOORDMODE_WRAP << SS3_TCX_ADDR_MODE_SHIFT) |
	    (TEXCOORDMODE_WRAP << SS3_TCY_ADDR_MODE_SHIFT);
    case CAIRO_EXTEND_PAD:
	return
	    (TEXCOORDMODE_CLAMP_EDGE << SS3_TCX_ADDR_MODE_SHIFT) |
	    (TEXCOORDMODE_CLAMP_EDGE << SS3_TCY_ADDR_MODE_SHIFT);
    case CAIRO_EXTEND_REFLECT:
	return
	    (TEXCOORDMODE_MIRROR << SS3_TCX_ADDR_MODE_SHIFT) |
	    (TEXCOORDMODE_MIRROR << SS3_TCY_ADDR_MODE_SHIFT);
    }
}

static inline uint32_t cairo_const
BUF_tiling (uint32_t tiling)
{
    switch (tiling) {
    default:
    case I915_TILING_NONE: return 0;
    case I915_TILING_X: return BUF_3D_TILED_SURFACE | BUF_3D_TILE_WALK_X;
    case I915_TILING_Y: return BUF_3D_TILED_SURFACE | BUF_3D_TILE_WALK_Y;
    }
}

#define OUT_DWORD(dword) i915_batch_emit_dword (device, dword)
#define OUT_RELOC(surface, read, write) i915_batch_emit_reloc (device, to_intel_bo (surface->intel.drm.bo), surface->offset, read, write, FALSE)
#define OUT_RELOC_FENCED(surface, read, write) i915_batch_emit_reloc (device, to_intel_bo (surface->intel.drm.bo), surface->offset, read, write, TRUE)

#define FS_LOCALS							\
    uint32_t *_shader_start

#define FS_BEGIN()							\
do {									\
    _shader_start = BATCH_PTR (device);					\
    OUT_DWORD (_3DSTATE_PIXEL_SHADER_PROGRAM);				\
} while (0)

#define FS_END()							\
do {									\
    *_shader_start |= BATCH_PTR (device) - _shader_start - 2;		\
} while (0);

static inline int32_t
i915_batch_space (i915_device_t *device)
{
    /* leave room for RECTLIST(4) + MI_BUFFER_END + MI_NOOP */
    return sizeof (device->batch_base) - (device->batch.used << 2) - 32;
}

static inline cairo_bool_t
i915_check_aperture_size (const i915_device_t *device, int relocs, size_t est_size, size_t size)
{
    return device->batch.reloc_count + relocs < I915_MAX_RELOCS - 2 &&
	   device->batch.est_gtt_size + est_size <= device->batch.gtt_avail_size &&
	   device->batch.total_gtt_size + size <= device->intel.gtt_avail_size;
}

static inline cairo_bool_t
i915_check_aperture (const i915_device_t *device, intel_bo_t **bo_array, int count)
{
    uint32_t relocs = 0, est_size = 0, size = 0;

    while (count--) {
	const intel_bo_t *bo = *bo_array++;
	if (bo->exec == NULL) {
	    relocs++;
	    size += bo->base.size;
	    if (!bo->busy)
		est_size += bo->base.size;
	}
    }

    return i915_check_aperture_size (device, relocs, est_size, size);
}

static inline cairo_bool_t
i915_check_aperture_and_fences (const i915_device_t *device, intel_bo_t **bo_array, int count)
{
    uint32_t relocs = 0, est_size = 0, size = 0;
    uint32_t fences = 0;

    while (count--) {
	const intel_bo_t *bo = *bo_array++;
	if (bo->exec == NULL) {
	    relocs++;
	    size += bo->base.size;
	    if (!bo->busy)
		est_size += bo->base.size;
	    if (bo->tiling != I915_TILING_NONE)
		fences++;
	} else if (bo->tiling != I915_TILING_NONE) {
	    if ((bo->exec->flags & EXEC_OBJECT_NEEDS_FENCE) == 0)
		fences++;
	}
    }

    return i915_check_aperture_size (device, relocs, est_size, size) &&
	   device->batch.fences + fences <= device->batch.fences_avail;
}

#define BATCH_PTR(device) &(device)->batch_base[(device)->batch.used]
static inline void
i915_batch_emit_dword (i915_device_t *device, uint32_t dword)
{
    device->batch_base[device->batch.used++] = dword;
}

cairo_private void
i915_batch_add_reloc (i915_device_t *device, uint32_t pos,
		      intel_bo_t *bo,
		      uint32_t offset,
		      uint32_t read_domains,
		      uint32_t write_domain,
		      cairo_bool_t needs_fence);

static inline void
i915_batch_fill_reloc (i915_device_t *device, uint32_t pos,
		       intel_bo_t *bo,
		       uint32_t offset,
		       uint32_t read_domains,
		       uint32_t write_domain)
{
    i915_batch_add_reloc (device, pos,
	                  bo, offset,
			  read_domains, write_domain,
			  FALSE);
    device->batch_base[pos] = bo->offset + offset;
}

static inline void
i915_batch_emit_reloc (i915_device_t *device,
		       intel_bo_t *bo,
		       uint32_t offset,
		       uint32_t read_domains,
		       uint32_t write_domain,
		       cairo_bool_t needs_fence)
{
    i915_batch_add_reloc (device, device->batch.used,
	                  bo, offset,
			  read_domains, write_domain,
			  needs_fence);
    i915_batch_emit_dword (device, bo->offset + offset);
}

cairo_private void
i915_vbo_flush (i915_device_t *device);

cairo_private void
i915_vbo_finish (i915_device_t *device);

cairo_private  cairo_status_t
i915_batch_flush (i915_device_t *device);

static inline float *
i915_add_rectangle (i915_device_t *device)
{
    float *vertices;
    uint32_t size;

    assert (device->floats_per_vertex);
    assert (device->rectangle_size == 3*device->floats_per_vertex*sizeof(float));

    size = device->rectangle_size;
    if (unlikely (device->vbo_offset + size > I915_VBO_SIZE))
	i915_vbo_finish (device);

    vertices = (float *) (device->vbo_base + device->vbo_offset);
    device->vbo_used = device->vbo_offset += size;
    device->vertex_count += 3;
    return vertices;
}

static inline i915_device_t *
i915_device (i915_surface_t *surface)
{
    return (i915_device_t *) surface->intel.drm.base.device;
}

cairo_private cairo_status_t
i915_surface_clear (i915_surface_t *dst);

cairo_private void
i915_set_dst (i915_device_t *device, i915_surface_t *dst);

cairo_private void
i915_shader_init (i915_shader_t *shader,
		  i915_surface_t *dst,
		  cairo_operator_t op,
		  double opacity);

cairo_private cairo_status_t
i915_shader_acquire_pattern (i915_shader_t *shader,
			     union i915_shader_channel *src,
			     const cairo_pattern_t *pattern,
			     const cairo_rectangle_int_t *extents);

cairo_private void
i915_shader_set_clip (i915_shader_t *shader,
		      cairo_clip_t *clip);

cairo_private int
i915_shader_num_texcoords (const i915_shader_t *shader);

static inline double cairo_const
i915_shader_linear_texcoord (const struct i915_shader_linear *l,
			     double src_x, double src_y)
{
    return l->dx * src_x + l->dy * src_y + l->offset;
}

cairo_private cairo_status_t
i915_shader_commit (i915_shader_t *shader,
		    i915_device_t *device);

cairo_private void
i915_shader_fini (i915_shader_t *shader);

cairo_private cairo_status_t
i915_fixup_unbounded (i915_surface_t *dst,
		      const cairo_composite_rectangles_t *extents,
		      cairo_clip_t *clip);

static inline cairo_bool_t
i915_surface_needs_tiling (i915_surface_t *dst)
{
    return dst->intel.drm.width > 2048 || dst->intel.drm.height > 2048;
}

cairo_private cairo_status_t
i915_surface_copy_subimage (i915_device_t *device,
			    i915_surface_t *src,
			    const cairo_rectangle_int_t *extents,
			    cairo_bool_t flush,
			    i915_surface_t **clone_out);

static inline uint32_t
pack_float (float f)
{
    union {
	float f;
	uint32_t ui;
    } t;
    t.f = f;
    return t.ui;
}

static inline cairo_status_t
i915_surface_fallback_flush (i915_surface_t *surface)
{
    cairo_status_t status;

    if (unlikely (surface->intel.drm.fallback != NULL))
	return intel_surface_flush (&surface->intel, 0);

    status = CAIRO_STATUS_SUCCESS;
    if (unlikely (surface->deferred_clear))
	status = i915_surface_clear (surface);

    return status;
}

#endif /* CAIRO_DRM_I915_PRIVATE_H */

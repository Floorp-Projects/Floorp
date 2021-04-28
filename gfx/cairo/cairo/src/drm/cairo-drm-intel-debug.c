/**************************************************************************
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
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

#include "cairoint.h"
#include "cairo-drm-intel-private.h"

struct debug_stream {
    unsigned offset;	/* current gtt offset */
    const char *ptr;		/* pointer to gtt offset zero */
    const char *end;		/* pointer to gtt offset zero */
};

static cairo_bool_t
debug (struct debug_stream *stream, const char *name, uint32_t len)
{
    uint32_t i;
    const uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);

    if (len == 0) {
	fprintf (stderr, "Error - zero length packet (0x%08x)\n", stream->ptr[0]);
	ASSERT_NOT_REACHED;
	return FALSE;
    }

    fprintf (stderr, "%04x:  ", stream->offset);

    fprintf (stderr, "%s (%d dwords):\n", name, len);
    for (i = 0; i < len; i++)
	fprintf (stderr, "\t0x%08x\n",  ptr[i]);
    fprintf (stderr, "\n");

    stream->offset += len * sizeof(uint32_t);
    return TRUE;
}


static const char *
get_prim_name (uint32_t val)
{
    switch (val & PRIM3D_MASK) {
    case PRIM3D_TRILIST: return "TRILIST";
    case PRIM3D_TRISTRIP: return "TRISTRIP";
    case PRIM3D_TRISTRIP_RVRSE: return "TRISTRIP_RVRSE";
    case PRIM3D_TRIFAN: return "TRIFAN";
    case PRIM3D_POLY: return "POLY";
    case PRIM3D_LINELIST: return "LINELIST";
    case PRIM3D_LINESTRIP: return "LINESTRIP";
    case PRIM3D_RECTLIST: return "RECTLIST";
    case PRIM3D_POINTLIST: return "POINTLIST";
    case PRIM3D_DIB: return "DIB";
    case PRIM3D_CLEAR_RECT: return "CLEAR_RECT";
    case PRIM3D_ZONE_INIT: return "ZONE_INIT";
    default: return "????";
    }
}

static cairo_bool_t
debug_prim (struct debug_stream *stream,
	    const char *name,
	    cairo_bool_t dump_floats,
	    uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    const char *prim = get_prim_name( ptr[0] );
    uint32_t i;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s %s (%d dwords):\n", name, prim, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[0]);
    for (i = 1; i < len; i++) {
	if (dump_floats)
	    fprintf (stderr, "\t0x%08x // %f\n",  ptr[i], *(float *)&ptr[i]);
	else
	    fprintf (stderr, "\t0x%08x\n",  ptr[i]);
    }

    fprintf (stderr, "\n");

    stream->offset += len * sizeof(uint32_t);
    return TRUE;
}

static const char *opcodes[] = {
    "NOP",
    "ADD",
    "MOV",
    "MUL",
    "MAD",
    "DP2ADD",
    "DP3",
    "DP4",
    "FRC",
    "RCP",
    "RSQ",
    "EXP",
    "LOG",
    "CMP",
    "MIN",
    "MAX",
    "FLR",
    "MOD",
    "TRC",
    "SGE",
    "SLT",
    "TEXLD",
    "TEXLDP",
    "TEXLDB",
    "TEXKILL",
    "DCL",
    "0x1a",
    "0x1b",
    "0x1c",
    "0x1d",
    "0x1e",
    "0x1f",
};

static const int args[] = {
    0,                           /* 0 nop */
    2,                           /* 1 add */
    1,                           /* 2 mov */
    2,                           /* 3 m ul */
    3,                           /* 4 mad */
    3,                           /* 5 dp2add */
    2,                           /* 6 dp3 */
    2,                           /* 7 dp4 */
    1,                           /* 8 frc */
    1,                           /* 9 rcp */
    1,                           /* a rsq */
    1,                           /* b exp */
    1,                           /* c log */
    3,                           /* d cmp */
    2,                           /* e min */
    2,                           /* f max */
    1,                           /* 10 flr */
    1,                           /* 11 mod */
    1,                           /* 12 trc */
    2,                           /* 13 sge */
    2,                           /* 14 slt */
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

static const char *regname[] = {
    "R",
    "T",
    "CONST",
    "S",
    "OC",
    "OD",
    "U",
    "UNKNOWN",
};

static void
print_reg_type_nr(uint32_t type, uint32_t nr)
{
    switch (type) {
    case REG_TYPE_T:
	switch (nr) {
	case T_DIFFUSE:
	    fprintf (stderr, "T_DIFFUSE");
	    return;
	case T_SPECULAR:
	    fprintf (stderr, "T_SPECULAR");
	    return;
	case T_FOG_W:
	    fprintf (stderr, "T_FOG_W");
	    return;
	default:
	    fprintf (stderr, "T_TEX%d", nr);
	    return;
	}
    case REG_TYPE_OC:
	if (nr == 0) {
	    fprintf (stderr, "oC");
	    return;
	}
	break;
    case REG_TYPE_OD:
	if (nr == 0) {
	    fprintf (stderr, "oD");
	    return;
	}
	break;
    default:
	break;
    }

    fprintf (stderr, "%s[%d]", regname[type], nr);
}

#define REG_SWIZZLE_MASK 0x7777
#define REG_NEGATE_MASK 0x8888

#define REG_SWIZZLE_XYZW ((SRC_X << A2_SRC2_CHANNEL_X_SHIFT) |	\
			  (SRC_Y << A2_SRC2_CHANNEL_Y_SHIFT) |	\
			  (SRC_Z << A2_SRC2_CHANNEL_Z_SHIFT) |	\
			  (SRC_W << A2_SRC2_CHANNEL_W_SHIFT))

static void
print_reg_neg_swizzle(uint32_t reg)
{
    int i;

    if ((reg & REG_SWIZZLE_MASK) == REG_SWIZZLE_XYZW &&
	(reg & REG_NEGATE_MASK) == 0)
	return;

    fprintf (stderr, ".");

    for (i = 12; i >= 0; i -= 4) {
	if (reg & (8 << i))
	    fprintf (stderr, "-");

	switch ((reg >> i) & 0x7) {
	case 0:
	    fprintf (stderr, "x");
	    break;
	case 1:
	    fprintf (stderr, "y");
	    break;
	case 2:
	    fprintf (stderr, "z");
	    break;
	case 3:
	    fprintf (stderr, "w");
	    break;
	case 4:
	    fprintf (stderr, "0");
	    break;
	case 5:
	    fprintf (stderr, "1");
	    break;
	default:
	    fprintf (stderr, "?");
	    break;
	}
    }
}

static void
print_src_reg(uint32_t dword)
{
    uint32_t nr = (dword >> A2_SRC2_NR_SHIFT) & REG_NR_MASK;
    uint32_t type = (dword >> A2_SRC2_TYPE_SHIFT) & REG_TYPE_MASK;
    print_reg_type_nr(type, nr);
    print_reg_neg_swizzle(dword);
}

static void
print_dest_reg(uint32_t dword)
{
    uint32_t nr = (dword >> A0_DEST_NR_SHIFT) & REG_NR_MASK;
    uint32_t type = (dword >> A0_DEST_TYPE_SHIFT) & REG_TYPE_MASK;
    print_reg_type_nr(type, nr);
    if ((dword & A0_DEST_CHANNEL_ALL) == A0_DEST_CHANNEL_ALL)
	return;
    fprintf (stderr, ".");
    if (dword & A0_DEST_CHANNEL_X)
	fprintf (stderr, "x");
    if (dword & A0_DEST_CHANNEL_Y)
	fprintf (stderr, "y");
    if (dword & A0_DEST_CHANNEL_Z)
	fprintf (stderr, "z");
    if (dword & A0_DEST_CHANNEL_W)
	fprintf (stderr, "w");
}

#define GET_SRC0_REG(r0, r1) ((r0<<14)|(r1>>A1_SRC0_CHANNEL_W_SHIFT))
#define GET_SRC1_REG(r0, r1) ((r0<<8)|(r1>>A2_SRC1_CHANNEL_W_SHIFT))
#define GET_SRC2_REG(r)      (r)

static void
print_arith_op(uint32_t opcode, const uint32_t * program)
{
    if (opcode != A0_NOP) {
	print_dest_reg(program[0]);
	if (program[0] & A0_DEST_SATURATE)
	    fprintf (stderr, " = SATURATE ");
	else
	    fprintf (stderr, " = ");
    }

    fprintf (stderr, "%s ", opcodes[opcode]);

    print_src_reg(GET_SRC0_REG(program[0], program[1]));
    if (args[opcode] == 1) {
	fprintf (stderr, "\n");
	return;
    }

    fprintf (stderr, ", ");
    print_src_reg(GET_SRC1_REG(program[1], program[2]));
    if (args[opcode] == 2) {
	fprintf (stderr, "\n");
	return;
    }

    fprintf (stderr, ", ");
    print_src_reg(GET_SRC2_REG(program[2]));
    fprintf (stderr, "\n");
    return;
}

static void
print_tex_op(uint32_t opcode, const uint32_t * program)
{
    print_dest_reg(program[0] | A0_DEST_CHANNEL_ALL);
    fprintf (stderr, " = ");

    fprintf (stderr, "%s ", opcodes[opcode]);

    fprintf (stderr, "S[%d],", program[0] & T0_SAMPLER_NR_MASK);

    print_reg_type_nr((program[1] >> T1_ADDRESS_REG_TYPE_SHIFT) &
		      REG_TYPE_MASK,
		      (program[1] >> T1_ADDRESS_REG_NR_SHIFT) & REG_NR_MASK);
    fprintf (stderr, "\n");
}

static void
print_dcl_op(uint32_t opcode, const uint32_t * program)
{
    fprintf (stderr, "%s ", opcodes[opcode]);
    print_dest_reg(program[0] | A0_DEST_CHANNEL_ALL);
    fprintf (stderr, "\n");
}

static void
i915_disassemble_program (const uint32_t * program, uint32_t sz)
{
    uint32_t size = program[0] & 0x1ff;
    uint32_t i;

    fprintf (stderr, "\tPROGRAM\n");

    assert(size + 2 == sz);

    program++;
    for (i = 1; i < sz; i += 3, program += 3) {
	uint32_t opcode = program[0] & (0x1f << 24);

	fprintf (stderr, "\t\t");

	if ((int) opcode >= A0_NOP && opcode <= A0_SLT)
	    print_arith_op(opcode >> 24, program);
	else if (opcode >= T0_TEXLD && opcode <= T0_TEXKILL)
	    print_tex_op(opcode >> 24, program);
	else if (opcode == D0_DCL)
	    print_dcl_op(opcode >> 24, program);
	else
	    fprintf (stderr, "Unknown opcode 0x%x\n", opcode);
    }

    fprintf (stderr, "\tEND-PROGRAM\n\n");
}

static cairo_bool_t
debug_program (struct debug_stream *stream, const char *name, uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);

    if (len == 0) {
	fprintf (stderr, "Error - zero length packet (0x%08x)\n", stream->ptr[0]);
	ASSERT_NOT_REACHED;
	return FALSE;
    }

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    i915_disassemble_program (ptr, len);

    stream->offset += len * sizeof(uint32_t);
    return TRUE;
}

static cairo_bool_t
debug_chain (struct debug_stream *stream, const char *name, uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t old_offset = stream->offset + len * sizeof(uint32_t);
    uint32_t i;

    fprintf (stderr, "%s (%d dwords):\n", name, len);
    for (i = 0; i < len; i++)
	fprintf (stderr, "\t0x%08x\n",  ptr[i]);

    stream->offset = ptr[1] & ~0x3;

    if (stream->offset < old_offset)
	fprintf (stderr, "\n... skipping backwards from 0x%x --> 0x%x ...\n\n",
		 old_offset, stream->offset );
    else
	fprintf (stderr, "\n... skipping from 0x%x --> 0x%x ...\n\n",
		 old_offset, stream->offset );

    return TRUE;
}

static cairo_bool_t
debug_variable_length_prim (struct debug_stream *stream)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    const char *prim = get_prim_name( ptr[0] );
    uint32_t i, len;

    uint16_t *idx = (uint16_t *)(ptr+1);
    for (i = 0; idx[i] != 0xffff; i++)
	;

    len = 1+(i+2)/2;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "3DPRIM, %s variable length %d indices (%d dwords):\n", prim, i, len);
    for (i = 0; i < len; i++)
	fprintf (stderr, "\t0x%08x\n",  ptr[i]);
    fprintf (stderr, "\n");

    stream->offset += len * sizeof(uint32_t);
    return TRUE;
}

#define BITS(dw, hi, lo, ...)					\
    do {							\
	unsigned himask = 0xffffffffU >> (31 - (hi));		\
	fprintf (stderr, "\t\t ");				\
	fprintf (stderr, __VA_ARGS__);				\
	fprintf (stderr, ": 0x%x\n", ((dw) & himask) >> (lo));	\
    } while (0)

#define MBZ(dw, hi, lo) do {				\
    unsigned x = (dw) >> (lo);				\
    unsigned lomask = (1 << (lo)) - 1;			\
    unsigned himask;					\
    himask = (1UL << (hi)) - 1;				\
    assert ((x & himask & ~lomask) == 0);		\
} while (0)

#define FLAG(dw, bit, ... )					\
    do {							\
	if (((dw) >> (bit)) & 1) {				\
	    fprintf (stderr, "\t\t ");				\
	    fprintf (stderr, __VA_ARGS__);			\
	    fprintf (stderr, "\n");				\
	}							\
    } while (0)

static cairo_bool_t
debug_load_immediate (struct debug_stream *stream,
		      const char *name,
		      uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t bits = (ptr[0] >> 4) & 0xff;
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords, flags: %x):\n", name, len, bits);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    if (bits & (1<<0)) {
	fprintf (stderr, "\t  LIS0: 0x%08x\n", ptr[j]);
	fprintf (stderr, "\t vb address: 0x%08x\n", (ptr[j] & ~0x3));
	BITS (ptr[j], 0, 0, "vb invalidate disable");
	j++;
    }
    if (bits & (1<<1)) {
	fprintf (stderr, "\t  LIS1: 0x%08x\n", ptr[j]);
	BITS (ptr[j], 29, 24, "vb dword width");
	BITS (ptr[j], 21, 16, "vb dword pitch");
	BITS (ptr[j], 15, 0, "vb max index");
	j++;
    }
    if (bits & (1<<2)) {
	int i;
	fprintf (stderr, "\t  LIS2: 0x%08x\n", ptr[j]);
	for (i = 0; i < 8; i++) {
	    unsigned tc = (ptr[j] >> (i * 4)) & 0xf;
	    if (tc != 0xf)
		BITS (tc, 3, 0, "tex coord %d", i);
	}
	j++;
    }
    if (bits & (1<<3)) {
	fprintf (stderr, "\t  LIS3: 0x%08x\n", ptr[j]);
	j++;
    }
    if (bits & (1<<4)) {
	fprintf (stderr, "\t  LIS4: 0x%08x\n", ptr[j]);
	BITS (ptr[j], 31, 23, "point width");
	BITS (ptr[j], 22, 19, "line width");
	FLAG (ptr[j], 18, "alpha flatshade");
	FLAG (ptr[j], 17, "fog flatshade");
	FLAG (ptr[j], 16, "spec flatshade");
	FLAG (ptr[j], 15, "rgb flatshade");
	BITS (ptr[j], 14, 13, "cull mode");
	FLAG (ptr[j], 12, "vfmt: point width");
	FLAG (ptr[j], 11, "vfmt: specular/fog");
	FLAG (ptr[j], 10, "vfmt: rgba");
	FLAG (ptr[j], 9, "vfmt: depth offset");
	BITS (ptr[j], 8, 6, "vfmt: position (2==xyzw)");
	FLAG (ptr[j], 5, "force dflt diffuse");
	FLAG (ptr[j], 4, "force dflt specular");
	FLAG (ptr[j], 3, "local depth offset enable");
	FLAG (ptr[j], 2, "vfmt: fp32 fog coord");
	FLAG (ptr[j], 1, "sprite point");
	FLAG (ptr[j], 0, "antialiasing");
	j++;
    }
    if (bits & (1<<5)) {
	fprintf (stderr, "\t  LIS5: 0x%08x\n", ptr[j]);
	BITS (ptr[j], 31, 28, "rgba write disables");
	FLAG (ptr[j], 27,     "force dflt point width");
	FLAG (ptr[j], 26,     "last pixel enable");
	FLAG (ptr[j], 25,     "global z offset enable");
	FLAG (ptr[j], 24,     "fog enable");
	BITS (ptr[j], 23, 16, "stencil ref");
	BITS (ptr[j], 15, 13, "stencil test");
	BITS (ptr[j], 12, 10, "stencil fail op");
	BITS (ptr[j], 9, 7,   "stencil pass z fail op");
	BITS (ptr[j], 6, 4,   "stencil pass z pass op");
	FLAG (ptr[j], 3,      "stencil write enable");
	FLAG (ptr[j], 2,      "stencil test enable");
	FLAG (ptr[j], 1,      "color dither enable");
	FLAG (ptr[j], 0,      "logiop enable");
	j++;
    }
    if (bits & (1<<6)) {
	fprintf (stderr, "\t  LIS6: 0x%08x\n", ptr[j]);
	FLAG (ptr[j], 31,      "alpha test enable");
	BITS (ptr[j], 30, 28,  "alpha func");
	BITS (ptr[j], 27, 20,  "alpha ref");
	FLAG (ptr[j], 19,      "depth test enable");
	BITS (ptr[j], 18, 16,  "depth func");
	FLAG (ptr[j], 15,      "blend enable");
	BITS (ptr[j], 14, 12,  "blend func");
	BITS (ptr[j], 11, 8,   "blend src factor");
	BITS (ptr[j], 7,  4,   "blend dst factor");
	FLAG (ptr[j], 3,       "depth write enable");
	FLAG (ptr[j], 2,       "color write enable");
	BITS (ptr[j], 1,  0,   "provoking vertex");
	j++;
    }

    fprintf (stderr, "\n");

    assert(j == len);

    stream->offset += len * sizeof(uint32_t);
    return TRUE;
}

static cairo_bool_t
debug_load_indirect (struct debug_stream *stream,
		     const char *name,
		     uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t bits = (ptr[0] >> 8) & 0x3f;
    uint32_t i, j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    for (i = 0; i < 6; i++) {
	if (bits & (1<<i)) {
	    switch (1<<(8+i)) {
	    case LI0_STATE_STATIC_INDIRECT:
		fprintf (stderr, "        STATIC: 0x%08x | %x\n", ptr[j]&~3, ptr[j]&3); j++;
		fprintf (stderr, "                0x%08x\n", ptr[j++]);
		break;
	    case LI0_STATE_DYNAMIC_INDIRECT:
		fprintf (stderr, "       DYNAMIC: 0x%08x | %x\n", ptr[j]&~3, ptr[j]&3); j++;
		break;
	    case LI0_STATE_SAMPLER:
		fprintf (stderr, "       SAMPLER: 0x%08x | %x\n", ptr[j]&~3, ptr[j]&3); j++;
		fprintf (stderr, "                0x%08x\n", ptr[j++]);
		break;
	    case LI0_STATE_MAP:
		fprintf (stderr, "           MAP: 0x%08x | %x\n", ptr[j]&~3, ptr[j]&3); j++;
		fprintf (stderr, "                0x%08x\n", ptr[j++]);
		break;
	    case LI0_STATE_PROGRAM:
		fprintf (stderr, "       PROGRAM: 0x%08x | %x\n", ptr[j]&~3, ptr[j]&3); j++;
		fprintf (stderr, "                0x%08x\n", ptr[j++]);
		break;
	    case LI0_STATE_CONSTANTS:
		fprintf (stderr, "     CONSTANTS: 0x%08x | %x\n", ptr[j]&~3, ptr[j]&3); j++;
		fprintf (stderr, "                0x%08x\n", ptr[j++]);
		break;
	    default:
		ASSERT_NOT_REACHED;
		break;
	    }
	}
    }

    if (bits == 0) {
	fprintf (stderr, "\t  DUMMY: 0x%08x\n", ptr[j++]);
    }

    fprintf (stderr, "\n");

    assert(j == len);
    stream->offset += len * sizeof(uint32_t);
    return TRUE;
}

static void
BR13 (struct debug_stream *stream,
      uint32_t val)
{
    fprintf (stderr, "\t0x%08x\n",  val);
    FLAG (val, 30, "clipping enable");
    BITS (val, 25, 24, "color depth (3==32bpp)");
    BITS (val, 23, 16, "raster op");
    BITS (val, 15, 0,  "dest pitch");
}

static void
BR2223 (struct debug_stream *stream,
	uint32_t val22, uint32_t val23)
{
    union { uint32_t val; short field[2]; } BR22, BR23;

    BR22.val = val22;
    BR23.val = val23;

    fprintf (stderr, "\t0x%08x\n",  val22);
    BITS (val22, 31, 16, "dest y1");
    BITS (val22, 15, 0,  "dest x1");

    fprintf (stderr, "\t0x%08x\n",  val23);
    BITS (val23, 31, 16, "dest y2");
    BITS (val23, 15, 0,  "dest x2");

    /* The blit engine may produce unexpected results when these aren't met */
    assert(BR22.field[0] < BR23.field[0]);
    assert(BR22.field[1] < BR23.field[1]);
}

static void
BR09 (struct debug_stream *stream,
      uint32_t val)
{
    fprintf (stderr, "\t0x%08x -- dest address\n",  val);
}

static void
BR26 (struct debug_stream *stream,
      uint32_t val)
{
    fprintf (stderr, "\t0x%08x\n",  val);
    BITS (val, 31, 16, "src y1");
    BITS (val, 15, 0,  "src x1");
}

static void
BR11 (struct debug_stream *stream,
      uint32_t val)
{
    fprintf (stderr, "\t0x%08x\n",  val);
    BITS (val, 15, 0,  "src pitch");
}

static void
BR12 (struct debug_stream *stream,
      uint32_t val)
{
    fprintf (stderr, "\t0x%08x -- src address\n",  val);
}

static void
BR16 (struct debug_stream *stream,
      uint32_t val)
{
    fprintf (stderr, "\t0x%08x -- color\n",  val);
}

static cairo_bool_t
debug_copy_blit (struct debug_stream *stream,
		 const char *name,
		 uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    BR13(stream, ptr[j++]);
    BR2223(stream, ptr[j], ptr[j+1]);
    j += 2;
    BR09(stream, ptr[j++]);
    BR26(stream, ptr[j++]);
    BR11(stream, ptr[j++]);
    BR12(stream, ptr[j++]);

    stream->offset += len * sizeof(uint32_t);
    assert(j == len);
    return TRUE;
}

static cairo_bool_t
debug_color_blit (struct debug_stream *stream,
		  const char *name,
		  uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    BR13(stream, ptr[j++]);
    BR2223(stream, ptr[j], ptr[j+1]);
    j += 2;
    BR09(stream, ptr[j++]);
    BR16(stream, ptr[j++]);

    stream->offset += len * sizeof(uint32_t);
    assert(j == len);
    return TRUE;
}

static cairo_bool_t
debug_modes4 (struct debug_stream *stream,
	      const char *name,
	      uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j]);
    BITS (ptr[j], 21, 18, "logicop func");
    FLAG (ptr[j], 17, "stencil test mask modify-enable");
    FLAG (ptr[j], 16, "stencil write mask modify-enable");
    BITS (ptr[j], 15, 8, "stencil test mask");
    BITS (ptr[j], 7, 0,  "stencil write mask");
    fprintf (stderr, "\n");
    j++;

    stream->offset += len * sizeof(uint32_t);
    assert(j == len);
    return TRUE;
}

static cairo_bool_t
debug_map_state (struct debug_stream *stream,
		 const char *name,
		 uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    {
	fprintf (stderr, "\t0x%08x\n",  ptr[j]);
	BITS (ptr[j], 15, 0,   "map mask");
	j++;
    }

    while (j < len) {
	{
	    fprintf (stderr, "\t  TMn.0: 0x%08x\n", ptr[j]);
	    fprintf (stderr, "\t map address: 0x%08x\n", (ptr[j] & ~0x3));
	    FLAG (ptr[j], 1, "vertical line stride");
	    FLAG (ptr[j], 0, "vertical line stride offset");
	    j++;
	}

	{
	    fprintf (stderr, "\t  TMn.1: 0x%08x\n", ptr[j]);
	    BITS (ptr[j], 31, 21, "height");
	    BITS (ptr[j], 20, 10, "width");
	    BITS (ptr[j], 9, 7, "surface format");
	    BITS (ptr[j], 6, 3, "texel format");
	    FLAG (ptr[j], 2, "use fence regs");
	    FLAG (ptr[j], 1, "tiled surface");
	    FLAG (ptr[j], 0, "tile walk ymajor");
	    j++;
	}
	{
	    fprintf (stderr, "\t  TMn.2: 0x%08x\n", ptr[j]);
	    BITS (ptr[j], 31, 21, "dword pitch");
	    BITS (ptr[j], 20, 15, "cube face enables");
	    BITS (ptr[j], 14, 9, "max lod");
	    FLAG (ptr[j], 8,     "mip layout right");
	    BITS (ptr[j], 7, 0, "depth");
	    j++;
	}
    }

    stream->offset += len * sizeof(uint32_t);
    assert(j == len);
    return TRUE;
}

static cairo_bool_t
debug_sampler_state (struct debug_stream *stream,
		     const char *name,
		     uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    {
	fprintf (stderr, "\t0x%08x\n",  ptr[j]);
	BITS (ptr[j], 15, 0,   "sampler mask");
	j++;
    }

    while (j < len) {
	{
	    fprintf (stderr, "\t  TSn.0: 0x%08x\n", ptr[j]);
	    FLAG (ptr[j], 31, "reverse gamma");
	    FLAG (ptr[j], 30, "planar to packed");
	    FLAG (ptr[j], 29, "yuv->rgb");
	    BITS (ptr[j], 28, 27, "chromakey index");
	    BITS (ptr[j], 26, 22, "base mip level");
	    BITS (ptr[j], 21, 20, "mip mode filter");
	    BITS (ptr[j], 19, 17, "mag mode filter");
	    BITS (ptr[j], 16, 14, "min mode filter");
	    BITS (ptr[j], 13, 5,  "lod bias (s4.4)");
	    FLAG (ptr[j], 4,      "shadow enable");
	    FLAG (ptr[j], 3,      "max-aniso-4");
	    BITS (ptr[j], 2, 0,   "shadow func");
	    j++;
	}

	{
	    fprintf (stderr, "\t  TSn.1: 0x%08x\n", ptr[j]);
	    BITS (ptr[j], 31, 24, "min lod");
	    MBZ( ptr[j], 23, 18 );
	    FLAG (ptr[j], 17,     "kill pixel enable");
	    FLAG (ptr[j], 16,     "keyed tex filter mode");
	    FLAG (ptr[j], 15,     "chromakey enable");
	    BITS (ptr[j], 14, 12, "tcx wrap mode");
	    BITS (ptr[j], 11, 9,  "tcy wrap mode");
	    BITS (ptr[j], 8,  6,  "tcz wrap mode");
	    FLAG (ptr[j], 5,      "normalized coords");
	    BITS (ptr[j], 4,  1,  "map (surface) index");
	    FLAG (ptr[j], 0,      "EAST deinterlacer enable");
	    j++;
	}
	{
	    fprintf (stderr, "\t  TSn.2: 0x%08x  (default color)\n", ptr[j]);
	    j++;
	}
    }

    stream->offset += len * sizeof(uint32_t);
    assert(j == len);
    return TRUE;
}

static cairo_bool_t
debug_dest_vars (struct debug_stream *stream,
		 const char *name,
		 uint32_t len)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    {
	fprintf (stderr, "\t0x%08x\n",  ptr[j]);
	FLAG (ptr[j], 31,     "early classic ztest");
	FLAG (ptr[j], 30,     "opengl tex default color");
	FLAG (ptr[j], 29,     "bypass iz");
	FLAG (ptr[j], 28,     "lod preclamp");
	BITS (ptr[j], 27, 26, "dither pattern");
	FLAG (ptr[j], 25,     "linear gamma blend");
	FLAG (ptr[j], 24,     "debug dither");
	BITS (ptr[j], 23, 20, "dstorg x");
	BITS (ptr[j], 19, 16, "dstorg y");
	MBZ (ptr[j], 15, 15 );
	BITS (ptr[j], 14, 12, "422 write select");
	BITS (ptr[j], 11, 8,  "cbuf format");
	BITS (ptr[j], 3, 2,   "zbuf format");
	FLAG (ptr[j], 1,      "vert line stride");
	FLAG (ptr[j], 1,      "vert line stride offset");
	j++;
    }

    stream->offset += len * sizeof(uint32_t);
    assert(j == len);
    return TRUE;
}

static cairo_bool_t debug_buf_info( struct debug_stream *stream,
				    const char *name,
				    uint32_t len )
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t j = 0;

    fprintf (stderr, "%04x:  ", stream->offset);
    fprintf (stderr, "%s (%d dwords):\n", name, len);
    fprintf (stderr, "\t0x%08x\n",  ptr[j++]);

    {
	fprintf (stderr, "\t0x%08x\n",  ptr[j]);
	BITS (ptr[j], 28, 28, "aux buffer id");
	BITS (ptr[j], 27, 24, "buffer id (7=depth, 3=back)");
	FLAG (ptr[j], 23,     "use fence regs");
	FLAG (ptr[j], 22,     "tiled surface");
	FLAG (ptr[j], 21,     "tile walk ymajor");
	MBZ (ptr[j], 20, 14);
	BITS (ptr[j], 13, 2,  "dword pitch");
	MBZ (ptr[j], 2,  0);
	j++;
    }

    fprintf (stderr, "\t0x%08x -- buffer base address\n",  ptr[j++]);

    stream->offset += len * sizeof(uint32_t);
    assert(j == len);
    return TRUE;
}

static cairo_bool_t
decode_3d_i915 (struct debug_stream *stream)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t cmd = *ptr;

    switch ((cmd >> 24) & 0x1f) {
    case 0x6:
	return debug (stream, "3DSTATE_ANTI_ALIASING", 1);
    case 0x7:
	return debug (stream, "3DSTATE_RASTERIZATION_RULES", 1);
    case 0x8:
	return debug (stream, "3DSTATE_BACKFACE_STENCIL_OPS", 1);
    case 0x9:
	return debug (stream, "3DSTATE_BACKFACE_STENCIL_MASKS", 1);
    case 0xb:
	return debug (stream, "3DSTATE_INDEPENDENT_ALPHA_BLEND", 1);
    case 0xc:
	return debug (stream, "3DSTATE_MODES5", 1);
    case 0xd:
	return debug_modes4(stream, "3DSTATE_MODES4", 1);
    case 0x15:
	return debug (stream, "3DSTATE_FOG_COLOR", 1);
    case 0x16:
	return debug (stream, "3DSTATE_COORD_SET_BINDINGS", 1);
    case 0x1c:
	/* 3DState16NP */
	switch((cmd >> 19) & 0x1f) {
	case 0x10:
	    return debug (stream, "3DSTATE_SCISSOR_ENABLE", 1);
	case 0x11:
	    return debug (stream, "3DSTATE_DEPTH_SUBRECTANGLE_DISABLE", 1);
	default:
	    break;
	}
	break;
    case 0x1d:
	/* 3DStateMW */
	switch ((cmd >> 16) & 0xff) {
	case 0x0:
	    return debug_map_state(stream, "3DSTATE_MAP_STATE", (cmd & 0x1f) + 2);
	case 0x1:
	    return debug_sampler_state(stream, "3DSTATE_SAMPLER_STATE", (cmd & 0x1f) + 2);
	case 0x4:
	    return debug_load_immediate(stream, "3DSTATE_LOAD_STATE_IMMEDIATE", (cmd & 0xf) + 2);
	case 0x5:
	    return debug_program(stream, "3DSTATE_PIXEL_SHADER_PROGRAM", (cmd & 0x1ff) + 2);
	case 0x6:
	    return debug (stream, "3DSTATE_PIXEL_SHADER_CONSTANTS", (cmd & 0xff) + 2);
	case 0x7:
	    return debug_load_indirect(stream, "3DSTATE_LOAD_INDIRECT", (cmd & 0xff) + 2);
	case 0x80:
	    return debug (stream, "3DSTATE_DRAWING_RECTANGLE", (cmd & 0xffff) + 2);
	case 0x81:
	    return debug (stream, "3DSTATE_SCISSOR_RECTANGLE", (cmd & 0xffff) + 2);
	case 0x83:
	    return debug (stream, "3DSTATE_SPAN_STIPPLE", (cmd & 0xffff) + 2);
	case 0x85:
	    return debug_dest_vars(stream, "3DSTATE_DEST_BUFFER_VARS", (cmd & 0xffff) + 2);
	case 0x88:
	    return debug (stream, "3DSTATE_CONSTANT_BLEND_COLOR", (cmd & 0xffff) + 2);
	case 0x89:
	    return debug (stream, "3DSTATE_FOG_MODE", (cmd & 0xffff) + 2);
	case 0x8e:
	    return debug_buf_info(stream, "3DSTATE_BUFFER_INFO", (cmd & 0xffff) + 2);
	case 0x97:
	    return debug (stream, "3DSTATE_DEPTH_OFFSET_SCALE", (cmd & 0xffff) + 2);
	case 0x98:
	    return debug (stream, "3DSTATE_DEFAULT_Z", (cmd & 0xffff) + 2);
	case 0x99:
	    return debug (stream, "3DSTATE_DEFAULT_DIFFUSE", (cmd & 0xffff) + 2);
	case 0x9a:
	    return debug (stream, "3DSTATE_DEFAULT_SPECULAR", (cmd & 0xffff) + 2);
	case 0x9c:
	    return debug (stream, "3DSTATE_CLEAR_PARAMETERS", (cmd & 0xffff) + 2);
	default:
	    ASSERT_NOT_REACHED;
	    return 0;
	}
	break;
    case 0x1e:
	if (cmd & (1 << 23))
	    return debug (stream, "???", (cmd & 0xffff) + 1);
	else
	    return debug (stream, "", 1);
	break;
    case 0x1f:
	if ((cmd & (1 << 23)) == 0) {
	    return debug_prim (stream, "3DPRIM (inline)", 1, (cmd & 0x1ffff) + 2);
	} else if (cmd & (1 << 17)) {
	    if ((cmd & 0xffff) == 0)
		return debug_variable_length_prim (stream);
	    else
		return debug_prim (stream, "3DPRIM (indexed)", 0, (((cmd & 0xffff) + 1) / 2) + 1);
	} else
	    return debug_prim (stream, "3DPRIM  (indirect sequential)", 0, 2);
	break;
    default:
	return debug (stream, "", 0);
    }

    return FALSE;
}

static cairo_bool_t
decode_3d_i965 (struct debug_stream *stream)
{
    const uint32_t *data = (uint32_t *) (stream->ptr + stream->offset);
    const uint32_t opcode = (data[0] & 0xffff0000) >> 16;
    unsigned int idx;
    const struct {
	uint32_t opcode;
	int min_len;
	int max_len;
	const char *name;
    } opcodes_3d[] = {
	{ 0x6000, 3, 3, "URB_FENCE" },
	{ 0x6001, 2, 2, "CS_URB_STATE" },
	{ 0x6002, 2, 2, "CONSTANT_BUFFER" },
	{ 0x6101, 6, 6, "STATE_BASE_ADDRESS" },
	{ 0x6102, 2, 2 , "STATE_SIP" },
	{ 0x6104, 1, 1, "3DSTATE_PIPELINE_SELECT" },
	{ 0x680b, 1, 1, "3DSTATE_VF_STATISTICS" },
	{ 0x6904, 1, 1, "3DSTATE_PIPELINE_SELECT" },
	{ 0x7800, 7, 7, "3DSTATE_PIPELINED_POINTERS" },
	{ 0x7801, 6, 6, "3DSTATE_BINDING_TABLE_POINTERS" },
	{ 0x780b, 1, 1, "3DSTATE_VF_STATISTICS" },
	{ 0x7808, 5, 257, "3DSTATE_VERTEX_BUFFERS" },
	{ 0x7809, 3, 256, "3DSTATE_VERTEX_ELEMENTS" },
	{ 0x780a, 3, 3, "3DSTATE_INDEX_BUFFER" },
	{ 0x7900, 4, 4, "3DSTATE_DRAWING_RECTANGLE" },
	{ 0x7901, 5, 5, "3DSTATE_CONSTANT_COLOR" },
	{ 0x7905, 5, 7, "3DSTATE_DEPTH_BUFFER" },
	{ 0x7906, 2, 2, "3DSTATE_POLY_STIPPLE_OFFSET" },
	{ 0x7907, 33, 33, "3DSTATE_POLY_STIPPLE_PATTERN" },
	{ 0x7908, 3, 3, "3DSTATE_LINE_STIPPLE" },
	{ 0x7909, 2, 2, "3DSTATE_GLOBAL_DEPTH_OFFSET_CLAMP" },
	{ 0x790a, 3, 3, "3DSTATE_AA_LINE_PARAMETERS" },
	{ 0x7b00, 6, 6, "3DPRIMITIVE" },
    }, *opcode_3d;

    for (idx = 0; idx < ARRAY_LENGTH (opcodes_3d); idx++) {
	opcode_3d = &opcodes_3d[idx];
	if (opcode == opcode_3d->opcode) {
	    unsigned int len = 1;
	    if (opcode_3d->max_len != 1)
		len = (data[0] & 0x000000ff) + 2;
	    return debug (stream, opcode_3d->name, len);
	}
    }

    return FALSE;
}

static cairo_bool_t
decode_3d_i830 (struct debug_stream *stream)
{
    ASSERT_NOT_REACHED;
    return FALSE;
}

static cairo_bool_t
i915_debug_packet (struct debug_stream *stream,
		   int devid)
{
    uint32_t *ptr = (uint32_t *)(stream->ptr + stream->offset);
    uint32_t cmd = *ptr;

    switch (((cmd >> 29) & 0x7)) {
    case 0x0:
	switch ((cmd >> 23) & 0x3f) {
	case 0x0:
	    return debug (stream, "MI_NOOP", 1);
	case 0x3:
	    return debug (stream, "MI_WAIT_FOR_EVENT", 1);
	case 0x4:
	    return debug (stream, "MI_FLUSH", 1);
	case 0xA:
	    debug (stream, "MI_BATCH_BUFFER_END", 1);
	    return FALSE;
	case 0x22:
	    return debug (stream, "MI_LOAD_REGISTER_IMM", 3);
	case 0x31:
	    return debug_chain(stream, "MI_BATCH_BUFFER_START", 2);
	default:
	    break;
	}
	break;
    case 0x1:
	break;
    case 0x2:
	switch ((cmd >> 22) & 0xff) {
	case 0x50:
	    return debug_color_blit(stream, "XY_COLOR_BLT", (cmd & 0xff) + 2);
	case 0x53:
	    return debug_copy_blit(stream, "XY_SRC_COPY_BLT", (cmd & 0xff) + 2);
	default:
	    return debug (stream, "blit command", (cmd & 0xff) + 2);
	}
	break;
    case 0x3:
	if (IS_965(devid))
	    return decode_3d_i965 (stream);
	else if (IS_9XX(devid))
	    return decode_3d_i915 (stream);
	else
	    return decode_3d_i830 (stream);
    default:
	break;
    }

    fprintf (stderr, "Bogus cmd: %x [%x]\n", (cmd >> 29) & 7, cmd);
    ASSERT_NOT_REACHED;
    return 0;
}

void
intel_dump_batchbuffer (const void *batch,
			uint32_t length,
			int devid)
{
    struct debug_stream stream;
    cairo_bool_t done = FALSE;

    fprintf (stderr, "\nBATCH: (%d dwords)\n", length / 4);

    stream.offset = 0;
    stream.ptr = batch;

    while (! done && stream.offset < length) {
	if (! i915_debug_packet (&stream, devid))
	    break;

	assert (stream.offset <= length);
    }

    fprintf (stderr, "END-BATCH\n\n");
    fflush (stderr);
}

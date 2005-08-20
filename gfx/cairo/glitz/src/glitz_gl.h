/*
 * Copyright © 2004 David Reveman, Peter Nilsson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the names of
 * David Reveman and Peter Nilsson not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. David Reveman and Peter Nilsson
 * makes no representations about the suitability of this software for
 * any purpose. It is provided "as is" without express or implied warranty.
 *
 * DAVID REVEMAN AND PETER NILSSON DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL DAVID REVEMAN AND
 * PETER NILSSON BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: David Reveman <davidr@novell.com>
 *          Peter Nilsson <c99pnn@cs.umu.se>
 */

#ifndef GLITZ_GL_H_INCLUDED
#define GLITZ_GL_H_INCLUDED

#include <stddef.h>

#ifdef _WIN32
#define GLITZ_GL_API_ATTRIBUTE __stdcall
#else
#define GLITZ_GL_API_ATTRIBUTE
#endif

typedef unsigned int glitz_gl_enum_t;
typedef unsigned char glitz_gl_boolean_t;
typedef void glitz_gl_void_t;
typedef int glitz_gl_int_t;
typedef unsigned int glitz_gl_uint_t;
typedef int glitz_gl_sizei_t;
typedef double glitz_gl_double_t;
typedef float glitz_gl_float_t;
typedef unsigned short glitz_gl_ushort_t;
typedef short glitz_gl_short_t;
typedef unsigned int glitz_gl_bitfield_t;
typedef double glitz_gl_clampd_t;
typedef float glitz_gl_clampf_t;
typedef unsigned char glitz_gl_ubyte_t;
typedef ptrdiff_t glitz_gl_intptr_t;
typedef ptrdiff_t glitz_gl_sizeiptr_t;


#define GLITZ_GL_FALSE 0x0
#define GLITZ_GL_TRUE  0x1

#define GLITZ_GL_NO_ERROR          0x0
#define GLITZ_GL_INVALID_OPERATION 0x0502

#define GLITZ_GL_VERSION                     0x1F02
#define GLITZ_GL_EXTENSIONS                  0x1F03

#define GLITZ_GL_UNSIGNED_BYTE               0x1401
#define GLITZ_GL_UNSIGNED_BYTE_3_3_2         0x8032
#define GLITZ_GL_UNSIGNED_BYTE_2_3_3_REV     0x8362
#define GLITZ_GL_UNSIGNED_SHORT_5_6_5        0x8363
#define GLITZ_GL_UNSIGNED_SHORT_5_6_5_REV    0x8364
#define GLITZ_GL_UNSIGNED_SHORT_4_4_4_4      0x8033
#define GLITZ_GL_UNSIGNED_SHORT_4_4_4_4_REV  0x8365
#define GLITZ_GL_UNSIGNED_SHORT_5_5_5_1      0x8034
#define GLITZ_GL_UNSIGNED_SHORT_1_5_5_5_REV  0x8366
#define GLITZ_GL_UNSIGNED_INT_8_8_8_8_REV    0x8367
#define GLITZ_GL_UNSIGNED_INT_10_10_10_2     0x8036
#define GLITZ_GL_UNSIGNED_INT_2_10_10_10_REV 0x8368

#define GLITZ_GL_MODELVIEW  0x1700
#define GLITZ_GL_PROJECTION 0x1701

#define GLITZ_GL_SHORT  0x1402
#define GLITZ_GL_INT    0x1404
#define GLITZ_GL_FLOAT  0x1406
#define GLITZ_GL_DOUBLE 0x140A

#define GLITZ_GL_POINTS         0x0000
#define GLITZ_GL_LINES          0x0001
#define GLITZ_GL_LINE_LOOP      0x0002
#define GLITZ_GL_LINE_STRIP     0x0003
#define GLITZ_GL_TRIANGLES      0x0004
#define GLITZ_GL_TRIANGLE_STRIP 0x0005
#define GLITZ_GL_TRIANGLE_FAN   0x0006
#define GLITZ_GL_QUADS          0x0007
#define GLITZ_GL_QUAD_STRIP     0x0008
#define GLITZ_GL_POLYGON        0x0009

#define GLITZ_GL_VERTEX_ARRAY        0x8074
#define GLITZ_GL_TEXTURE_COORD_ARRAY 0x8078

#define GLITZ_GL_FILL           0x1B02
#define GLITZ_GL_FRONT          0x0404
#define GLITZ_GL_BACK           0x0405
#define GLITZ_GL_CULL_FACE      0x0B44

#define GLITZ_GL_POINT_SMOOTH   0x0B10
#define GLITZ_GL_LINE_SMOOTH    0x0B20
#define GLITZ_GL_POLYGON_SMOOTH 0x0B41

#define GLITZ_GL_SCISSOR_TEST 0x0C11

#define GLITZ_GL_MAX_TEXTURE_SIZE  0x0D33
#define GLITZ_GL_MAX_VIEWPORT_DIMS 0x0D3A

#define GLITZ_GL_TEXTURE_WIDTH  0x1000
#define GLITZ_GL_TEXTURE_HEIGHT 0x1001

#define GLITZ_GL_TEXTURE_ENV            0x2300
#define GLITZ_GL_TEXTURE_ENV_MODE       0x2200
#define GLITZ_GL_TEXTURE_2D             0x0DE1
#define GLITZ_GL_PROXY_TEXTURE_2D       0x8064
#define GLITZ_GL_TEXTURE_WRAP_S         0x2802
#define GLITZ_GL_TEXTURE_WRAP_T         0x2803
#define GLITZ_GL_TEXTURE_MAG_FILTER     0x2800
#define GLITZ_GL_TEXTURE_MIN_FILTER     0x2801
#define GLITZ_GL_TEXTURE_ENV_COLOR      0x2201
#define GLITZ_GL_TEXTURE_GEN_S          0x0C60
#define GLITZ_GL_TEXTURE_GEN_T          0x0C61
#define GLITZ_GL_TEXTURE_GEN_MODE       0x2500
#define GLITZ_GL_EYE_LINEAR             0x2400
#define GLITZ_GL_EYE_PLANE              0x2502
#define GLITZ_GL_S                      0x2000
#define GLITZ_GL_T                      0x2001

#define GLITZ_GL_MODULATE               0x2100
#define GLITZ_GL_NEAREST                0x2600
#define GLITZ_GL_LINEAR                 0x2601
#define GLITZ_GL_REPEAT                 0x2901
#define GLITZ_GL_CLAMP_TO_EDGE          0x812F
#define GLITZ_GL_CLAMP_TO_BORDER        0x812D
#define GLITZ_GL_TEXTURE_RED_SIZE       0x805C
#define GLITZ_GL_TEXTURE_GREEN_SIZE     0x805D
#define GLITZ_GL_TEXTURE_BLUE_SIZE      0x805E
#define GLITZ_GL_TEXTURE_ALPHA_SIZE     0x805F

#define GLITZ_GL_TEXTURE        0x1702
#define GLITZ_GL_SRC_COLOR      0x0300

#define GLITZ_GL_COMBINE        0x8570
#define GLITZ_GL_COMBINE_RGB    0x8571
#define GLITZ_GL_COMBINE_ALPHA  0x8572
#define GLITZ_GL_SOURCE0_RGB    0x8580
#define GLITZ_GL_SOURCE1_RGB    0x8581
#define GLITZ_GL_SOURCE2_RGB    0x8582
#define GLITZ_GL_SOURCE0_ALPHA  0x8588
#define GLITZ_GL_SOURCE1_ALPHA  0x8589
#define GLITZ_GL_SOURCE2_ALPHA  0x858A
#define GLITZ_GL_OPERAND0_RGB   0x8590
#define GLITZ_GL_OPERAND1_RGB   0x8591
#define GLITZ_GL_OPERAND2_RGB   0x8592
#define GLITZ_GL_OPERAND0_ALPHA	0x8598
#define GLITZ_GL_OPERAND1_ALPHA	0x8599
#define GLITZ_GL_OPERAND2_ALPHA	0x859A
#define GLITZ_GL_RGB_SCALE      0x8573
#define GLITZ_GL_ADD_SIGNED     0x8574
#define GLITZ_GL_INTERPOLATE    0x8575
#define GLITZ_GL_SUBTRACT       0x84E7
#define GLITZ_GL_CONSTANT       0x8576
#define GLITZ_GL_PRIMARY_COLOR  0x8577
#define GLITZ_GL_PREVIOUS       0x8578
#define GLITZ_GL_DOT3_RGB       0x86AE
#define GLITZ_GL_DOT3_RGBA      0x86AF

#define GLITZ_GL_STENCIL_TEST 0x0B90
#define GLITZ_GL_KEEP         0x1E00
#define GLITZ_GL_REPLACE      0x1E01
#define GLITZ_GL_INCR         0x1E02
#define GLITZ_GL_DECR         0x1E03

#define GLITZ_GL_LESS       0x0201
#define GLITZ_GL_EQUAL      0x0202
#define GLITZ_GL_LEQUAL     0x0203
#define GLITZ_GL_ALWAYS     0x0207
#define GLITZ_GL_DEPTH_TEST 0x0B71

#define GLITZ_GL_STENCIL_BUFFER_BIT 0x00000400
#define GLITZ_GL_VIEWPORT_BIT       0x00000800
#define GLITZ_GL_TRANSFORM_BIT      0x00001000
#define GLITZ_GL_COLOR_BUFFER_BIT   0x00004000

#define GLITZ_GL_ALPHA     0x1906
#define GLITZ_GL_RGB       0x1907
#define GLITZ_GL_LUMINANCE 0x1909
#define GLITZ_GL_COLOR     0x1800
#define GLITZ_GL_DITHER    0x0BD0
#define GLITZ_GL_RGBA      0x1908
#define GLITZ_GL_BGR       0x80E0
#define GLITZ_GL_BGRA      0x80E1

#define GLITZ_GL_ALPHA4   0x803B
#define GLITZ_GL_ALPHA8   0x803C
#define GLITZ_GL_ALPHA12  0x803D
#define GLITZ_GL_ALPHA16  0x803E
#define GLITZ_GL_R3_G3_B2 0x2A10
#define GLITZ_GL_RGB4     0x804F
#define GLITZ_GL_RGB5     0x8050
#define GLITZ_GL_RGB8     0x8051
#define GLITZ_GL_RGB10    0x8052
#define GLITZ_GL_RGB12    0x8053
#define GLITZ_GL_RGB16    0x8054
#define GLITZ_GL_RGBA2    0x8055
#define GLITZ_GL_RGBA4    0x8056
#define GLITZ_GL_RGB5_A1  0x8057
#define GLITZ_GL_RGBA8    0x8058
#define GLITZ_GL_RGB10_A2 0x8059
#define GLITZ_GL_RGBA12   0x805A
#define GLITZ_GL_RGBA16   0x805B

#define GLITZ_GL_FRONT_AND_BACK 0x0408
#define GLITZ_GL_FLAT           0x1D00
#define GLITZ_GL_SMOOTH         0x1D01

#define GLITZ_GL_BLEND               0x0BE2
#define GLITZ_GL_ZERO                0x0000
#define GLITZ_GL_ONE                 0x0001
#define GLITZ_GL_ONE_MINUS_SRC_COLOR 0x0301
#define GLITZ_GL_SRC_ALPHA           0x0302
#define GLITZ_GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GLITZ_GL_DST_ALPHA           0x0304
#define GLITZ_GL_ONE_MINUS_DST_ALPHA 0x0305
#define GLITZ_GL_SRC_ALPHA_SATURATE  0x0308
#define GLITZ_GL_CONSTANT_COLOR      0x8001

#define GLITZ_GL_PACK_ALIGNMENT      0x0D05
#define GLITZ_GL_PACK_LSB_FIRST      0x0D01
#define GLITZ_GL_PACK_ROW_LENGTH     0x0D02
#define GLITZ_GL_PACK_SKIP_PIXELS    0x0D04
#define GLITZ_GL_PACK_SKIP_ROWS      0x0D03
#define GLITZ_GL_UNPACK_ALIGNMENT    0x0CF5
#define GLITZ_GL_UNPACK_LSB_FIRST    0x0CF1
#define GLITZ_GL_UNPACK_ROW_LENGTH   0x0CF2
#define GLITZ_GL_UNPACK_SKIP_PIXELS  0x0CF4
#define GLITZ_GL_UNPACK_SKIP_ROWS    0x0CF3

#define GLITZ_GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#define GLITZ_GL_FASTEST                     0x1101
#define GLITZ_GL_NICEST                      0x1102

#define GLITZ_GL_COMPILE 0x1300

#define GLITZ_GL_TEXTURE_RECTANGLE          0x84F5
#define GLITZ_GL_PROXY_TEXTURE_RECTANGLE    0x84F7
#define GLITZ_GL_MAX_RECTANGLE_TEXTURE_SIZE 0x84F8

#define GLITZ_GL_MIRRORED_REPEAT 0x8370

#define GLITZ_GL_TEXTURE0          0x84C0
#define GLITZ_GL_TEXTURE1          0x84C1
#define GLITZ_GL_TEXTURE2          0x84C2
#define GLITZ_GL_ACTIVE_TEXTURE    0x84E0
#define GLITZ_GL_MAX_TEXTURE_UNITS 0x84E2

#define GLITZ_GL_MULTISAMPLE 0x809D

#define GLITZ_GL_MULTISAMPLE_FILTER_HINT 0x8534

#define GLITZ_GL_FRAGMENT_PROGRAM                    0x8804
#define GLITZ_GL_PROGRAM_STRING                      0x8628
#define GLITZ_GL_PROGRAM_FORMAT_ASCII                0x8875
#define GLITZ_GL_PROGRAM_ERROR_POSITION              0x864B
#define GLITZ_GL_MAX_PROGRAM_LOCAL_PARAMETERS        0x88B4
#define GLITZ_GL_PROGRAM_INSTRUCTIONS                0x88A0
#define GLITZ_GL_MAX_PROGRAM_INSTRUCTIONS            0x88A1
#define GLITZ_GL_PROGRAM_NATIVE_INSTRUCTIONS         0x88A2
#define GLITZ_GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS     0x88A3
#define GLITZ_GL_PROGRAM_PARAMETERS                  0x88A8
#define GLITZ_GL_MAX_PROGRAM_PARAMETERS              0x88A9
#define GLITZ_GL_PROGRAM_NATIVE_PARAMETERS           0x88AA
#define GLITZ_GL_MAX_PROGRAM_NATIVE_PARAMETERS       0x88AB
#define GLITZ_GL_PROGRAM_UNDER_NATIVE_LIMITS         0x88B6
#define GLITZ_GL_PROGRAM_ALU_INSTRUCTIONS            0x8805
#define GLITZ_GL_PROGRAM_TEX_INSTRUCTIONS            0x8806
#define GLITZ_GL_PROGRAM_TEX_INDIRECTIONS            0x8807
#define GLITZ_GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS     0x8808
#define GLITZ_GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS     0x8809
#define GLITZ_GL_PROGRAM_NATIVE_TEX_INDIRECTIONS     0x880A
#define GLITZ_GL_MAX_PROGRAM_ALU_INSTRUCTIONS        0x880B
#define GLITZ_GL_MAX_PROGRAM_TEX_INSTRUCTIONS        0x880C
#define GLITZ_GL_MAX_PROGRAM_TEX_INDIRECTIONS        0x880D
#define GLITZ_GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS 0x880E
#define GLITZ_GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS 0x880F
#define GLITZ_GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS 0x8810

#define GLITZ_GL_ARRAY_BUFFER         0x8892
#define GLITZ_GL_PIXEL_PACK_BUFFER    0x88EB
#define GLITZ_GL_PIXEL_UNPACK_BUFFER  0x88EC

#define GLITZ_GL_STREAM_DRAW  0x88E0
#define GLITZ_GL_STREAM_READ  0x88E1
#define GLITZ_GL_STREAM_COPY  0x88E2
#define GLITZ_GL_STATIC_DRAW  0x88E4
#define GLITZ_GL_STATIC_READ  0x88E5
#define GLITZ_GL_STATIC_COPY  0x88E6
#define GLITZ_GL_DYNAMIC_DRAW 0x88E8
#define GLITZ_GL_DYNAMIC_READ 0x88E9
#define GLITZ_GL_DYNAMIC_COPY 0x88EA

#define GLITZ_GL_READ_ONLY  0x88B8
#define GLITZ_GL_WRITE_ONLY 0x88B9
#define GLITZ_GL_READ_WRITE 0x88BA

#define GLITZ_GL_FRAMEBUFFER       0x8D40
#define GLITZ_GL_COLOR_ATTACHMENT0 0x8CE0
#define GLITZ_GL_FRAMEBUFFER_COMPLETE                        0x8CD5
#define GLITZ_GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT           0x8CD6
#define GLITZ_GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT   0x8CD7
#define GLITZ_GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT 0x8CD8
#define GLITZ_GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS           0x8CD9
#define GLITZ_GL_FRAMEBUFFER_INCOMPLETE_FORMATS              0x8CDA
#define GLITZ_GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER          0x8CDB
#define GLITZ_GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER          0x8CDC
#define GLITZ_GL_FRAMEBUFFER_UNSUPPORTED                     0x8CDD
#define GLITZ_GL_FRAMEBUFFER_STATUS_ERROR                    0x8CDE

typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_enable_t)
     (glitz_gl_enum_t cap);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_disable_t)
     (glitz_gl_enum_t cap);
typedef glitz_gl_enum_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_error_t)
     (glitz_gl_void_t);
typedef glitz_gl_ubyte_t *(GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_string_t)
     (glitz_gl_enum_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_enable_client_state_t)
     (glitz_gl_enum_t cap);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_disable_client_state_t)
     (glitz_gl_enum_t cap);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_vertex_pointer_t)
     (glitz_gl_int_t size, glitz_gl_enum_t type, glitz_gl_sizei_t stride,
      const glitz_gl_void_t *ptr);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_coord_pointer_t)
     (glitz_gl_int_t size, glitz_gl_enum_t type, glitz_gl_sizei_t stride,
      const glitz_gl_void_t *ptr);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_draw_arrays_t)
     (glitz_gl_enum_t mode, glitz_gl_int_t first, glitz_gl_sizei_t count);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_multi_draw_arrays_t)
     (glitz_gl_enum_t mode, glitz_gl_int_t *first, glitz_gl_sizei_t *count,
      glitz_gl_sizei_t primcount);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_env_f_t)
     (glitz_gl_enum_t target, glitz_gl_enum_t pname, glitz_gl_float_t param);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_env_fv_t)
     (glitz_gl_enum_t target, glitz_gl_enum_t pname,
      const glitz_gl_float_t *params);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_gen_i_t)
     (glitz_gl_enum_t coord, glitz_gl_enum_t pname, glitz_gl_int_t param);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_gen_fv_t)
     (glitz_gl_enum_t coord, glitz_gl_enum_t pname,
      const glitz_gl_float_t *params);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_scissor_t)
     (glitz_gl_int_t x, glitz_gl_int_t y,
      glitz_gl_sizei_t width, glitz_gl_sizei_t height);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_color_4us_t)
     (glitz_gl_ushort_t red, glitz_gl_ushort_t green, glitz_gl_ushort_t blue,
      glitz_gl_ushort_t alpha);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_color_4f_t)
     (glitz_gl_float_t red, glitz_gl_float_t green, glitz_gl_float_t blue,
      glitz_gl_float_t alpha);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_blend_func_t)
     (glitz_gl_enum_t sfactor, glitz_gl_enum_t dfactor);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_blend_color_t)
     (glitz_gl_clampf_t red, glitz_gl_clampf_t green, glitz_gl_clampf_t blue,
      glitz_gl_clampf_t alpha);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_clear_t)
     (glitz_gl_bitfield_t mask);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_clear_color_t)
     (glitz_gl_clampf_t red, glitz_gl_clampf_t green,
      glitz_gl_clampf_t blue, glitz_gl_clampf_t alpha);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_clear_stencil_t)
     (glitz_gl_int_t s);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_stencil_func_t)
     (glitz_gl_enum_t func, glitz_gl_int_t ref, glitz_gl_uint_t mask);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_stencil_op_t)
     (glitz_gl_enum_t fail, glitz_gl_enum_t zfail, glitz_gl_enum_t zpass);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_push_attrib_t)
     (glitz_gl_bitfield_t mask);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_pop_attrib_t)
     (glitz_gl_void_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_matrix_mode_t)
     (glitz_gl_enum_t mode);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_push_matrix_t)
     (glitz_gl_void_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_pop_matrix_t)
     (glitz_gl_void_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_load_identity_t)
     (glitz_gl_void_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_load_matrix_f_t)
     (const glitz_gl_float_t *m);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_depth_range_t)
     (glitz_gl_clampd_t near_val, glitz_gl_clampd_t far_val);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_viewport_t)
     (glitz_gl_int_t x, glitz_gl_int_t y,
      glitz_gl_sizei_t width, glitz_gl_sizei_t height);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_raster_pos_2f_t)
     (glitz_gl_float_t x, glitz_gl_float_t y);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_bitmap_t)
     (glitz_gl_sizei_t width, glitz_gl_sizei_t height,
      glitz_gl_float_t xorig, glitz_gl_float_t yorig,
      glitz_gl_float_t xmove, glitz_gl_float_t ymove,
      const glitz_gl_ubyte_t *bitmap);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_read_buffer_t)
     (glitz_gl_enum_t mode);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_draw_buffer_t)
     (glitz_gl_enum_t mode);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_copy_pixels_t)
     (glitz_gl_int_t x, glitz_gl_int_t y,
      glitz_gl_sizei_t width, glitz_gl_sizei_t height,
      glitz_gl_enum_t type);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_flush_t)
     (glitz_gl_void_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_finish_t)
     (glitz_gl_void_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_pixel_store_i_t)
     (glitz_gl_enum_t pname, glitz_gl_int_t param);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_ortho_t)
     (glitz_gl_double_t left, glitz_gl_double_t right,
      glitz_gl_double_t bottom, glitz_gl_double_t top,
      glitz_gl_double_t near_val, glitz_gl_double_t far_val);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_scale_f_t)
     (glitz_gl_float_t x, glitz_gl_float_t y, glitz_gl_float_t z);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_translate_f_t)
     (glitz_gl_float_t x, glitz_gl_float_t y, glitz_gl_float_t z);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_hint_t)
     (glitz_gl_enum_t target, glitz_gl_enum_t mode);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_depth_mask_t)
     (glitz_gl_boolean_t flag);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_polygon_mode_t)
     (glitz_gl_enum_t face, glitz_gl_enum_t mode);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_shade_model_t)
     (glitz_gl_enum_t mode);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_color_mask_t)
     (glitz_gl_boolean_t red,
      glitz_gl_boolean_t green,
      glitz_gl_boolean_t blue,
      glitz_gl_boolean_t alpha);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_read_pixels_t)
     (glitz_gl_int_t x, glitz_gl_int_t y,
      glitz_gl_sizei_t width, glitz_gl_sizei_t height,
      glitz_gl_enum_t format, glitz_gl_enum_t type,
      glitz_gl_void_t *pixels);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_tex_image_t)
     (glitz_gl_enum_t target, glitz_gl_int_t level,
      glitz_gl_enum_t format, glitz_gl_enum_t type,
      glitz_gl_void_t *pixels);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_sub_image_2d_t)
     (glitz_gl_enum_t target, glitz_gl_int_t level,
      glitz_gl_int_t xoffset, glitz_gl_int_t yoffset,
      glitz_gl_sizei_t width, glitz_gl_sizei_t height,
      glitz_gl_enum_t format, glitz_gl_enum_t type,
      const glitz_gl_void_t *pixels);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_gen_textures_t)
     (glitz_gl_sizei_t n, glitz_gl_uint_t *textures);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_delete_textures_t)
     (glitz_gl_sizei_t n, const glitz_gl_uint_t *textures);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_bind_texture_t)
     (glitz_gl_enum_t target, glitz_gl_uint_t texture);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_image_2d_t)
     (glitz_gl_enum_t target, glitz_gl_int_t level,
      glitz_gl_int_t internal_format,
      glitz_gl_sizei_t width, glitz_gl_sizei_t height,
      glitz_gl_int_t border, glitz_gl_enum_t format, glitz_gl_enum_t type,
      const glitz_gl_void_t *pixels);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_tex_parameter_i_t)
     (glitz_gl_enum_t target, glitz_gl_enum_t pname, glitz_gl_int_t param);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_tex_level_parameter_iv_t)
     (glitz_gl_enum_t target, glitz_gl_int_t level,
      glitz_gl_enum_t pname, glitz_gl_int_t *param);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_copy_tex_sub_image_2d_t)
     (glitz_gl_enum_t target, glitz_gl_int_t level,
      glitz_gl_int_t xoffset, glitz_gl_int_t yoffset,
      glitz_gl_int_t x, glitz_gl_int_t y,
      glitz_gl_sizei_t width, glitz_gl_sizei_t height);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_integer_v_t)
     (glitz_gl_enum_t pname, glitz_gl_int_t *params);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_pointer_v_t)
     (glitz_gl_enum_t pname, glitz_gl_void_t **params);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_active_texture_t)
     (glitz_gl_enum_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_client_active_texture_t)
     (glitz_gl_enum_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_gen_programs_t)
     (glitz_gl_sizei_t, glitz_gl_uint_t *);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_delete_programs_t)
     (glitz_gl_sizei_t, const glitz_gl_uint_t *);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_program_string_t)
     (glitz_gl_enum_t, glitz_gl_enum_t, glitz_gl_sizei_t,
      const glitz_gl_void_t *);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_bind_program_t)
     (glitz_gl_enum_t, glitz_gl_uint_t);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_program_local_param_4fv_t)
     (glitz_gl_enum_t, glitz_gl_uint_t, const glitz_gl_float_t *);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_program_iv_t)
     (glitz_gl_enum_t, glitz_gl_enum_t, glitz_gl_int_t *);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_gen_buffers_t)
     (glitz_gl_sizei_t, glitz_gl_uint_t *buffers);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_delete_buffers_t)
     (glitz_gl_sizei_t, const glitz_gl_uint_t *buffers);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_bind_buffer_t)
     (glitz_gl_enum_t, glitz_gl_uint_t buffer);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_buffer_data_t)
     (glitz_gl_enum_t, glitz_gl_sizeiptr_t, const glitz_gl_void_t *,
      glitz_gl_enum_t);
typedef glitz_gl_void_t *(GLITZ_GL_API_ATTRIBUTE * glitz_gl_buffer_sub_data_t)
     (glitz_gl_enum_t, glitz_gl_intptr_t, glitz_gl_sizeiptr_t,
      const glitz_gl_void_t *);
typedef glitz_gl_void_t *(GLITZ_GL_API_ATTRIBUTE * glitz_gl_get_buffer_sub_data_t)
     (glitz_gl_enum_t, glitz_gl_intptr_t, glitz_gl_sizeiptr_t,
      glitz_gl_void_t *);
typedef glitz_gl_void_t *(GLITZ_GL_API_ATTRIBUTE * glitz_gl_map_buffer_t)
     (glitz_gl_enum_t, glitz_gl_enum_t);
typedef glitz_gl_boolean_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_unmap_buffer_t)
     (glitz_gl_enum_t);
typedef void (GLITZ_GL_API_ATTRIBUTE * glitz_gl_gen_framebuffers_t)
     (glitz_gl_sizei_t, glitz_gl_uint_t *);
typedef void (GLITZ_GL_API_ATTRIBUTE * glitz_gl_delete_framebuffers_t)
     (glitz_gl_sizei_t, const glitz_gl_uint_t *);
typedef glitz_gl_void_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_bind_framebuffer_t)
     (glitz_gl_enum_t, glitz_gl_uint_t);
typedef glitz_gl_enum_t (GLITZ_GL_API_ATTRIBUTE * glitz_gl_check_framebuffer_status_t)
     (glitz_gl_enum_t);
typedef void (GLITZ_GL_API_ATTRIBUTE * glitz_gl_framebuffer_texture_2d_t)
    (glitz_gl_enum_t, glitz_gl_enum_t, glitz_gl_enum_t,
     glitz_gl_uint_t, glitz_gl_int_t);

#endif /* GLITZ_GL_H_INCLUDED */

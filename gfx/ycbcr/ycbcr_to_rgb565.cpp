/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <limits.h>
#include "nsDebug.h"
#include "ycbcr_to_rgb565.h"
#include "nsAlgorithm.h"



#ifdef HAVE_YCBCR_TO_RGB565

namespace mozilla {

namespace gfx {

/*This contains all of the parameters that are needed to convert a row.
  Passing them in a struct instead of as individual parameters saves the need
   to continually push onto the stack the ones that are fixed for every row.*/
struct yuv2rgb565_row_scale_bilinear_ctx{
  PRUint16 *rgb_row;
  const PRUint8 *y_row;
  const PRUint8 *u_row;
  const PRUint8 *v_row;
  int y_yweight;
  int y_pitch;
  int width;
  int source_x0_q16;
  int source_dx_q16;
  /*Not used for 4:4:4, except with chroma-nearest.*/
  int source_uv_xoffs_q16;
  /*Not used for 4:4:4 or chroma-nearest.*/
  int uv_pitch;
  /*Not used for 4:2:2, 4:4:4, or chroma-nearest.*/
  int uv_yweight;
};



/*This contains all of the parameters that are needed to convert a row.
  Passing them in a struct instead of as individual parameters saves the need
   to continually push onto the stack the ones that are fixed for every row.*/
struct yuv2rgb565_row_scale_nearest_ctx{
  PRUint16 *rgb_row;
  const PRUint8 *y_row;
  const PRUint8 *u_row;
  const PRUint8 *v_row;
  int width;
  int source_x0_q16;
  int source_dx_q16;
  /*Not used for 4:4:4.*/
  int source_uv_xoffs_q16;
};



typedef void (*yuv2rgb565_row_scale_bilinear_func)(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither);

typedef void (*yuv2rgb565_row_scale_nearest_func)(
 const yuv2rgb565_row_scale_nearest_ctx *ctx, int dither);



# if defined(MOZILLA_MAY_SUPPORT_NEON)

extern "C" void ScaleYCbCr42xToRGB565_BilinearY_Row_NEON(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither);

void __attribute((noinline)) yuv42x_to_rgb565_row_neon(uint16 *dst,
                                                       const uint8 *y,
                                                       const uint8 *u,
                                                       const uint8 *v,
                                                       int n,
                                                       int oddflag);

#endif



/*Bilinear interpolation of a single value.
  This uses the exact same formulas as the asm, even though it adds some extra
   shifts that do nothing but reduce accuracy.*/
static int bislerp(const PRUint8 *row,
                   int pitch,
                   int source_x,
                   int xweight,
                   int yweight) {
  int a;
  int b;
  int c;
  int d;
  a = row[source_x];
  b = row[source_x+1];
  c = row[source_x+pitch];
  d = row[source_x+pitch+1];
  a = ((a<<8)+(c-a)*yweight+128)>>8;
  b = ((b<<8)+(d-b)*yweight+128)>>8;
  return ((a<<8)+(b-a)*xweight+128)>>8;
}

/*Convert a single pixel from Y'CbCr to RGB565.
  This uses the exact same formulas as the asm, even though we could make the
   constants a lot more accurate with 32-bit wide registers.*/
static PRUint16 yu2rgb565(int y, int u, int v, int dither) {
  /*This combines the constant offset that needs to be added during the Y'CbCr
     conversion with a rounding offset that depends on the dither parameter.*/
  static const int DITHER_BIAS[4][3]={
    {-14240,    8704,    -17696},
    {-14240+128,8704+64, -17696+128},
    {-14240+256,8704+128,-17696+256},
    {-14240+384,8704+192,-17696+384}
  };
  int r;
  int g;
  int b;
  r = clamped((74*y+102*v+DITHER_BIAS[dither][0])>>9, 0, 31);
  g = clamped((74*y-25*u-52*v+DITHER_BIAS[dither][1])>>8, 0, 63);
  b = clamped((74*y+129*u+DITHER_BIAS[dither][2])>>9, 0, 31);
  return (PRUint16)(r<<11 | g<<5 | b);
}

static void ScaleYCbCr420ToRGB565_Bilinear_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    xweight = (((source_x_q16+ctx->source_uv_xoffs_q16)&0x1FFFF)+256)>>9;
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = bislerp(ctx->u_row, ctx->uv_pitch, source_x, xweight, ctx->uv_yweight);
    v = bislerp(ctx->v_row, ctx->uv_pitch, source_x, xweight, ctx->uv_yweight);
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static void ScaleYCbCr422ToRGB565_Bilinear_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    xweight = (((source_x_q16+ctx->source_uv_xoffs_q16)&0x1FFFF)+256)>>9;
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = bislerp(ctx->u_row, ctx->uv_pitch, source_x, xweight, ctx->y_yweight);
    v = bislerp(ctx->v_row, ctx->uv_pitch, source_x, xweight, ctx->y_yweight);
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static void ScaleYCbCr444ToRGB565_Bilinear_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    source_x_q16 += ctx->source_dx_q16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    u = bislerp(ctx->u_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    v = bislerp(ctx->v_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static void ScaleYCbCr42xToRGB565_BilinearY_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = ctx->u_row[source_x];
    v = ctx->v_row[source_x];
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static void ScaleYCbCr444ToRGB565_BilinearY_Row_C(
 const yuv2rgb565_row_scale_bilinear_ctx *ctx, int dither){
  int x;
  int source_x_q16;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    int source_x;
    int xweight;
    int y;
    int u;
    int v;
    xweight = ((source_x_q16&0xFFFF)+128)>>8;
    source_x = source_x_q16>>16;
    y = bislerp(ctx->y_row, ctx->y_pitch, source_x, xweight, ctx->y_yweight);
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>16;
    source_x_q16 += ctx->source_dx_q16;
    u = ctx->u_row[source_x];
    v = ctx->v_row[source_x];
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static void ScaleYCbCr42xToRGB565_Nearest_Row_C(
 const yuv2rgb565_row_scale_nearest_ctx *ctx, int dither){
  int y;
  int u;
  int v;
  int x;
  int source_x_q16;
  int source_x;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    source_x = source_x_q16>>16;
    y = ctx->y_row[source_x];
    source_x = (source_x_q16+ctx->source_uv_xoffs_q16)>>17;
    source_x_q16 += ctx->source_dx_q16;
    u = ctx->u_row[source_x];
    v = ctx->v_row[source_x];
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

static void ScaleYCbCr444ToRGB565_Nearest_Row_C(
 const yuv2rgb565_row_scale_nearest_ctx *ctx, int dither){
  int y;
  int u;
  int v;
  int x;
  int source_x_q16;
  int source_x;
  source_x_q16 = ctx->source_x0_q16;
  for (x = 0; x < ctx->width; x++) {
    source_x = source_x_q16>>16;
    source_x_q16 += ctx->source_dx_q16;
    y = ctx->y_row[source_x];
    u = ctx->u_row[source_x];
    v = ctx->v_row[source_x];
    ctx->rgb_row[x] = yu2rgb565(y, u, v, dither);
    dither ^= 3;
  }
}

NS_GFX_(void) ScaleYCbCrToRGB565(const PRUint8 *y_buf,
                                 const PRUint8 *u_buf,
                                 const PRUint8 *v_buf,
                                 PRUint8 *rgb_buf,
                                 int source_x0,
                                 int source_y0,
                                 int source_width,
                                 int source_height,
                                 int width,
                                 int height,
                                 int y_pitch,
                                 int uv_pitch,
                                 int rgb_pitch,
                                 YUVType yuv_type,
                                 ScaleFilter filter) {
  int source_x0_q16;
  int source_y0_q16;
  int source_dx_q16;
  int source_dy_q16;
  int source_uv_xoffs_q16;
  int source_uv_yoffs_q16;
  int x_shift;
  int y_shift;
  int ymin;
  int ymax;
  int uvmin;
  int uvmax;
  int dither;
  /*We don't support negative destination rectangles (just flip the source
     instead), and for empty ones there's nothing to do.*/
  if (width <= 0 || height <= 0)
    return;
  /*These bounds are required to avoid 16.16 fixed-point overflow.*/
  NS_ASSERTION(source_x0 > (INT_MIN>>16) && source_x0 < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source X offset out of bounds.");
  NS_ASSERTION(source_x0+source_width > (INT_MIN>>16)
            && source_x0+source_width < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source width out of bounds.");
  NS_ASSERTION(source_y0 > (INT_MIN>>16) && source_y0 < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source Y offset out of bounds.");
  NS_ASSERTION(source_y0+source_height > (INT_MIN>>16)
            && source_y0+source_height < (INT_MAX>>16),
    "ScaleYCbCrToRGB565 source height out of bounds.");
  /*We require the same stride for Y' and Cb and Cr for 4:4:4 content.*/
  NS_ASSERTION(yuv_type != YV24 || y_pitch == uv_pitch,
    "ScaleYCbCrToRGB565 luma stride differs from chroma for 4:4:4 content.");
  /*We assume we can read outside the bounds of the input, because it makes
     the code much simpler (and in practice is true: both Theora and VP8 return
     padded reference frames).
    In practice, we do not even _have_ the actual bounds of the source, as
     we are passed a crop rectangle from it, and not the dimensions of the full
     image.
    This assertion will not guarantee our out-of-bounds reads are safe, but it
     should at least catch the simple case of passing in an unpadded buffer.*/
  NS_ASSERTION(abs(y_pitch) >= abs(source_width)+16,
    "ScaleYCbCrToRGB565 source image unpadded?");
  /*The NEON code requires the pointers to be aligned to a 16-byte boundary at
     the start of each row.
    This should be true for all of our sources.
    We could try to fix this up if it's not true by adjusting source_x0, but
     that would require the mis-alignment to be the same for the U and V
     planes.*/
  NS_ASSERTION((y_pitch&15) == 0 && (uv_pitch&15) == 0 &&
   ((y_buf-(PRUint8 *)NULL)&15) == 0 &&
   ((u_buf-(PRUint8 *)NULL)&15) == 0 && ((v_buf-(PRUint8 *)NULL)&15) == 0,
   "ScaleYCbCrToRGB565 source image unaligned");
  /*We take an area-based approach to pixel coverage to avoid shifting by small
     amounts (or not so small, when up-scaling or down-scaling by a large
     factor).

    An illustrative example: scaling 4:2:0 up by 2, using JPEG chroma cositing^.

    + = RGB destination locations
    * = Y' source locations
    - = Cb, Cr source locations

    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +
          -              -
    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +

    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +
          -              -
    +   +   +   +  +   +   +   +
      *       *      *       *
    +   +   +   +  +   +   +   +

    So, the coordinates of the upper-left + (first destination site) should
     be (-0.25,-0.25) in the source Y' coordinate system.
    Similarly, the coordinates should be (-0.375,-0.375) in the source Cb, Cr
     coordinate system.
    Note that the origin and scale of these two coordinate systems is not the
     same!

    ^JPEG cositing is required for Theora; VP8 doesn't specify cositing rules,
     but nearly all software converters in existence (at least those that are
     open source, and many that are not) use JPEG cositing instead of MPEG.*/
  source_dx_q16 = (source_width<<16) / width;
  source_x0_q16 = (source_x0<<16)+(source_dx_q16>>1)-0x8000;
  source_dy_q16 = (source_height<<16) / height;
  source_y0_q16 = (source_y0<<16)+(source_dy_q16>>1)-0x8000;
  x_shift = (yuv_type != YV24);
  y_shift = (yuv_type == YV12);
  /*These two variables hold the difference between the origins of the Y' and
     the Cb, Cr coordinate systems, using the scale of the Y' coordinate
     system.*/
  source_uv_xoffs_q16 = -(x_shift<<15);
  source_uv_yoffs_q16 = -(y_shift<<15);
  /*Compute the range of source rows we'll actually use.
    This doesn't guarantee we won't read outside this range.*/
  ymin = source_height >= 0 ? source_y0 : source_y0+source_height-1;
  ymax = source_height >= 0 ? source_y0+source_height-1 : source_y0;
  uvmin = ymin>>y_shift;
  uvmax = ((ymax+1+y_shift)>>y_shift)-1;
  /*Pick a dithering pattern.
    The "&3" at the end is just in case RAND_MAX is lying.*/
  dither = (rand()/(RAND_MAX>>2))&3;
  /*Nearest-neighbor scaling.*/
  if (filter == FILTER_NONE) {
    yuv2rgb565_row_scale_nearest_ctx ctx;
    yuv2rgb565_row_scale_nearest_func scale_row;
    int y;
    /*Add rounding offsets once, in advance.*/
    source_x0_q16 += 0x8000;
    source_y0_q16 += 0x8000;
    source_uv_xoffs_q16 += (x_shift<<15);
    source_uv_yoffs_q16 += (y_shift<<15);
    if (yuv_type == YV12)
      scale_row = ScaleYCbCr42xToRGB565_Nearest_Row_C;
    else
      scale_row = ScaleYCbCr444ToRGB565_Nearest_Row_C;
    ctx.width = width;
    ctx.source_x0_q16 = source_x0_q16;
    ctx.source_dx_q16 = source_dx_q16;
    ctx.source_uv_xoffs_q16 = source_uv_xoffs_q16;
    for (y=0; y<height; y++) {
      int source_y;
      ctx.rgb_row = (PRUint16 *)(rgb_buf + y*rgb_pitch);
      source_y = source_y0_q16>>16;
      source_y = clamped(source_y, ymin, ymax);
      ctx.y_row = y_buf + source_y*y_pitch;
      source_y = (source_y0_q16+source_uv_yoffs_q16)>>(16+y_shift);
      source_y = clamped(source_y, uvmin, uvmax);
      source_y0_q16 += source_dy_q16;
      ctx.u_row = u_buf + source_y*uv_pitch;
      ctx.v_row = v_buf + source_y*uv_pitch;
      (*scale_row)(&ctx, dither);
      dither ^= 2;
    }
  }
  /*Bilinear scaling.*/
  else {
    yuv2rgb565_row_scale_bilinear_ctx ctx;
    yuv2rgb565_row_scale_bilinear_func scale_row;
    int uvxscale_min;
    int uvxscale_max;
    int uvyscale_min;
    int uvyscale_max;
    int y;
    /*Check how close the chroma scaling is to unity.
      If it's close enough, we can get away with nearest-neighbor chroma
       sub-sampling, and only doing bilinear on luma.
      If a given axis is subsampled, we use bounds on the luma step of
       [0.67...2], which is equivalent to scaling chroma by [1...3].
      If it's not subsampled, we use bounds of [0.5...1.33], which is
       equivalent to scaling chroma by [0.75...2].
      The lower bound is chosen as a trade-off between speed and how terrible
       nearest neighbor looks when upscaling.*/
# define CHROMA_NEAREST_SUBSAMP_STEP_MIN  0xAAAA
# define CHROMA_NEAREST_NORMAL_STEP_MIN   0x8000
# define CHROMA_NEAREST_SUBSAMP_STEP_MAX 0x20000
# define CHROMA_NEAREST_NORMAL_STEP_MAX  0x15555
    uvxscale_min = yuv_type != YV24 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MIN : CHROMA_NEAREST_NORMAL_STEP_MIN;
    uvxscale_max = yuv_type != YV24 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MAX : CHROMA_NEAREST_NORMAL_STEP_MAX;
    uvyscale_min = yuv_type == YV12 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MIN : CHROMA_NEAREST_NORMAL_STEP_MIN;
    uvyscale_max = yuv_type == YV12 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MAX : CHROMA_NEAREST_NORMAL_STEP_MAX;
    if (uvxscale_min <= abs(source_dx_q16)
     && abs(source_dx_q16) <= uvxscale_max
     && uvyscale_min <= abs(source_dy_q16)
     && abs(source_dy_q16) <= uvyscale_max) {
      /*Add the rounding offsets now.*/
      source_uv_xoffs_q16 += 1<<(15+x_shift);
      source_uv_yoffs_q16 += 1<<(15+y_shift);
      if (yuv_type != YV24) {
        scale_row =
#  if defined(MOZILLA_MAY_SUPPORT_NEON)
         supports_neon() ? ScaleYCbCr42xToRGB565_BilinearY_Row_NEON :
#  endif
         ScaleYCbCr42xToRGB565_BilinearY_Row_C;
      }
      else
        scale_row = ScaleYCbCr444ToRGB565_BilinearY_Row_C;
    }
    else {
      if (yuv_type == YV12)
        scale_row = ScaleYCbCr420ToRGB565_Bilinear_Row_C;
      else if (yuv_type == YV16)
        scale_row = ScaleYCbCr422ToRGB565_Bilinear_Row_C;
      else
        scale_row = ScaleYCbCr444ToRGB565_Bilinear_Row_C;
    }
    ctx.width = width;
    ctx.y_pitch = y_pitch;
    ctx.source_x0_q16 = source_x0_q16;
    ctx.source_dx_q16 = source_dx_q16;
    ctx.source_uv_xoffs_q16 = source_uv_xoffs_q16;
    ctx.uv_pitch = uv_pitch;
    for (y=0; y<height; y++) {
      int source_y;
      int yweight;
      int uvweight;
      ctx.rgb_row = (PRUint16 *)(rgb_buf + y*rgb_pitch);
      source_y = (source_y0_q16+128)>>16;
      yweight = ((source_y0_q16+128)>>8)&0xFF;
      if (source_y < ymin) {
        source_y = ymin;
        yweight = 0;
      }
      if (source_y > ymax) {
        source_y = ymax;
        yweight = 0;
      }
      ctx.y_row = y_buf + source_y*y_pitch;
      source_y = source_y0_q16+source_uv_yoffs_q16+(128<<y_shift);
      source_y0_q16 += source_dy_q16;
      uvweight = source_y>>(8+y_shift)&0xFF;
      source_y >>= 16+y_shift;
      if (source_y < uvmin) {
        source_y = uvmin;
        uvweight = 0;
      }
      if (source_y > uvmax) {
        source_y = uvmax;
        uvweight = 0;
      }
      ctx.u_row = u_buf + source_y*uv_pitch;
      ctx.v_row = v_buf + source_y*uv_pitch;
      ctx.y_yweight = yweight;
      ctx.uv_yweight = uvweight;
      (*scale_row)(&ctx, dither);
      dither ^= 2;
    }
  }
}

NS_GFX_(bool) IsScaleYCbCrToRGB565Fast(int source_x0,
                                       int source_y0,
                                       int source_width,
                                       int source_height,
                                       int width,
                                       int height,
                                       YUVType yuv_type,
                                       ScaleFilter filter)
{
  // Very fast.
  if (width <= 0 || height <= 0)
    return true;
#  if defined(MOZILLA_MAY_SUPPORT_NEON)
  if (filter != FILTER_NONE) {
    int source_dx_q16;
    int source_dy_q16;
    int uvxscale_min;
    int uvxscale_max;
    int uvyscale_min;
    int uvyscale_max;
    source_dx_q16 = (source_width<<16) / width;
    source_dy_q16 = (source_height<<16) / height;
    uvxscale_min = yuv_type != YV24 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MIN : CHROMA_NEAREST_NORMAL_STEP_MIN;
    uvxscale_max = yuv_type != YV24 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MAX : CHROMA_NEAREST_NORMAL_STEP_MAX;
    uvyscale_min = yuv_type == YV12 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MIN : CHROMA_NEAREST_NORMAL_STEP_MIN;
    uvyscale_max = yuv_type == YV12 ?
     CHROMA_NEAREST_SUBSAMP_STEP_MAX : CHROMA_NEAREST_NORMAL_STEP_MAX;
    if (uvxscale_min <= abs(source_dx_q16)
     && abs(source_dx_q16) <= uvxscale_max
     && uvyscale_min <= abs(source_dy_q16)
     && abs(source_dy_q16) <= uvyscale_max) {
      if (yuv_type != YV24)
        return supports_neon();
    }
  }
#  endif
  return false;
}



void yuv_to_rgb565_row_c(uint16 *dst,
                         const uint8 *y,
                         const uint8 *u,
                         const uint8 *v,
                         int x_shift,
                         int pic_x,
                         int pic_width)
{
  int x;
  for (x = 0; x < pic_width; x++)
  {
    dst[x] = yu2rgb565(y[pic_x+x],
                       u[(pic_x+x)>>x_shift],
                       v[(pic_x+x)>>x_shift],
                       2); // Disable dithering for now.
  }
}

NS_GFX_(void) ConvertYCbCrToRGB565(const uint8* y_buf,
                                   const uint8* u_buf,
                                   const uint8* v_buf,
                                   uint8* rgb_buf,
                                   int pic_x,
                                   int pic_y,
                                   int pic_width,
                                   int pic_height,
                                   int y_pitch,
                                   int uv_pitch,
                                   int rgb_pitch,
                                   YUVType yuv_type)
{
  int x_shift;
  int y_shift;
  x_shift = yuv_type != YV24;
  y_shift = yuv_type == YV12;
#  ifdef MOZILLA_MAY_SUPPORT_NEON
  if (yuv_type != YV24 && supports_neon())
  {
    for (int i = 0; i < pic_height; i++) {
      int yoffs;
      int uvoffs;
      yoffs = y_pitch * (pic_y+i) + pic_x;
      uvoffs = uv_pitch * ((pic_y+i)>>y_shift) + (pic_x>>x_shift);
      yuv42x_to_rgb565_row_neon((uint16*)(rgb_buf + rgb_pitch * i),
                                y_buf + yoffs,
                                u_buf + uvoffs,
                                v_buf + uvoffs,
                                pic_width,
                                pic_x&x_shift);
    }
  }
  else
#  endif
  {
    for (int i = 0; i < pic_height; i++) {
      int yoffs;
      int uvoffs;
      yoffs = y_pitch * (pic_y+i);
      uvoffs = uv_pitch * ((pic_y+i)>>y_shift);
      yuv_to_rgb565_row_c((uint16*)(rgb_buf + rgb_pitch * i),
                          y_buf + yoffs,
                          u_buf + uvoffs,
                          v_buf + uvoffs,
                          x_shift,
                          pic_x,
                          pic_width);
    }
  }
}

NS_GFX_(bool) IsConvertYCbCrToRGB565Fast(int pic_x,
                                         int pic_y,
                                         int pic_width,
                                         int pic_height,
                                         YUVType yuv_type)
{
#  if defined(MOZILLA_MAY_SUPPORT_NEON)
  return (yuv_type != YV24 && supports_neon());
#  else
  return false;
#  endif
}

} // namespace gfx

} // namespace mozilla

#endif // HAVE_YCBCR_TO_RGB565

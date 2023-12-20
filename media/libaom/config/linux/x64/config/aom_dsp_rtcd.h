// This file is generated. Do not edit.
#ifndef AOM_DSP_RTCD_H_
#define AOM_DSP_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * DSP
 */

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/enums.h"
#include "av1/common/blockd.h"


#ifdef __cplusplus
extern "C" {
#endif

void aom_blend_a64_hmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
void aom_blend_a64_hmask_sse4_1(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
RTCD_EXTERN void (*aom_blend_a64_hmask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);

void aom_blend_a64_mask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby);
void aom_blend_a64_mask_sse4_1(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby);
void aom_blend_a64_mask_avx2(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby);
RTCD_EXTERN void (*aom_blend_a64_mask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby);

void aom_blend_a64_vmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
void aom_blend_a64_vmask_sse4_1(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
RTCD_EXTERN void (*aom_blend_a64_vmask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);

void aom_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride);
#define aom_comp_avg_pred aom_comp_avg_pred_c

void aom_comp_avg_upsampled_pred_c(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                   const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                   int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                   int ref_stride, int subpel_search);
void aom_comp_avg_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                   const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                   int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                   int ref_stride, int subpel_search);
#define aom_comp_avg_upsampled_pred aom_comp_avg_upsampled_pred_sse2

void aom_comp_mask_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);
void aom_comp_mask_pred_ssse3(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);
void aom_comp_mask_pred_avx2(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);
RTCD_EXTERN void (*aom_comp_mask_pred)(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);

void aom_comp_mask_upsampled_pred_c(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                       const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                       int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                       int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask,
                                                       int subpel_search);
void aom_comp_mask_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                       const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                       int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                       int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask,
                                                       int subpel_search);
#define aom_comp_mask_upsampled_pred aom_comp_mask_upsampled_pred_sse2

void aom_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void aom_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void aom_convolve8_horiz_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void aom_convolve8_horiz_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*aom_convolve8_horiz)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void aom_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void aom_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void aom_convolve8_vert_ssse3(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void aom_convolve8_vert_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
RTCD_EXTERN void (*aom_convolve8_vert)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);

void aom_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
void aom_convolve_copy_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define aom_convolve_copy aom_convolve_copy_sse2

void aom_dc_128_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x16 aom_dc_128_predictor_16x16_sse2

void aom_dc_128_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x32 aom_dc_128_predictor_16x32_sse2

void aom_dc_128_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x4 aom_dc_128_predictor_16x4_sse2

void aom_dc_128_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x64 aom_dc_128_predictor_16x64_sse2

void aom_dc_128_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x8 aom_dc_128_predictor_16x8_sse2

void aom_dc_128_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_2x2 aom_dc_128_predictor_2x2_c

void aom_dc_128_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_32x8 aom_dc_128_predictor_32x8_sse2

void aom_dc_128_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_4x16 aom_dc_128_predictor_4x16_sse2

void aom_dc_128_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_4x4 aom_dc_128_predictor_4x4_sse2

void aom_dc_128_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_4x8 aom_dc_128_predictor_4x8_sse2

void aom_dc_128_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_8x16 aom_dc_128_predictor_8x16_sse2

void aom_dc_128_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_8x32 aom_dc_128_predictor_8x32_sse2

void aom_dc_128_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_8x4 aom_dc_128_predictor_8x4_sse2

void aom_dc_128_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_8x8 aom_dc_128_predictor_8x8_sse2

void aom_dc_left_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x16 aom_dc_left_predictor_16x16_sse2

void aom_dc_left_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x32 aom_dc_left_predictor_16x32_sse2

void aom_dc_left_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x4 aom_dc_left_predictor_16x4_sse2

void aom_dc_left_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x64 aom_dc_left_predictor_16x64_sse2

void aom_dc_left_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x8 aom_dc_left_predictor_16x8_sse2

void aom_dc_left_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_2x2 aom_dc_left_predictor_2x2_c

void aom_dc_left_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_32x8 aom_dc_left_predictor_32x8_sse2

void aom_dc_left_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_4x16 aom_dc_left_predictor_4x16_sse2

void aom_dc_left_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_4x4 aom_dc_left_predictor_4x4_sse2

void aom_dc_left_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_4x8 aom_dc_left_predictor_4x8_sse2

void aom_dc_left_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_8x16 aom_dc_left_predictor_8x16_sse2

void aom_dc_left_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_8x32 aom_dc_left_predictor_8x32_sse2

void aom_dc_left_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_8x4 aom_dc_left_predictor_8x4_sse2

void aom_dc_left_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_8x8 aom_dc_left_predictor_8x8_sse2

void aom_dc_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x16 aom_dc_predictor_16x16_sse2

void aom_dc_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x32 aom_dc_predictor_16x32_sse2

void aom_dc_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x4 aom_dc_predictor_16x4_sse2

void aom_dc_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x64 aom_dc_predictor_16x64_sse2

void aom_dc_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x8 aom_dc_predictor_16x8_sse2

void aom_dc_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_2x2 aom_dc_predictor_2x2_c

void aom_dc_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_32x8 aom_dc_predictor_32x8_sse2

void aom_dc_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_4x16 aom_dc_predictor_4x16_sse2

void aom_dc_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_4x4 aom_dc_predictor_4x4_sse2

void aom_dc_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_4x8 aom_dc_predictor_4x8_sse2

void aom_dc_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_8x16 aom_dc_predictor_8x16_sse2

void aom_dc_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_8x32 aom_dc_predictor_8x32_sse2

void aom_dc_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_8x4 aom_dc_predictor_8x4_sse2

void aom_dc_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_8x8 aom_dc_predictor_8x8_sse2

void aom_dc_top_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x16 aom_dc_top_predictor_16x16_sse2

void aom_dc_top_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x32 aom_dc_top_predictor_16x32_sse2

void aom_dc_top_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x4 aom_dc_top_predictor_16x4_sse2

void aom_dc_top_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x64 aom_dc_top_predictor_16x64_sse2

void aom_dc_top_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x8 aom_dc_top_predictor_16x8_sse2

void aom_dc_top_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_2x2 aom_dc_top_predictor_2x2_c

void aom_dc_top_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_32x8 aom_dc_top_predictor_32x8_sse2

void aom_dc_top_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_4x16 aom_dc_top_predictor_4x16_sse2

void aom_dc_top_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_4x4 aom_dc_top_predictor_4x4_sse2

void aom_dc_top_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_4x8 aom_dc_top_predictor_4x8_sse2

void aom_dc_top_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_8x16 aom_dc_top_predictor_8x16_sse2

void aom_dc_top_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_8x32 aom_dc_top_predictor_8x32_sse2

void aom_dc_top_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_8x4 aom_dc_top_predictor_8x4_sse2

void aom_dc_top_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_8x8 aom_dc_top_predictor_8x8_sse2

void aom_fdct8x8_c(const int16_t *input, tran_low_t *output, int stride);
void aom_fdct8x8_sse2(const int16_t *input, tran_low_t *output, int stride);
void aom_fdct8x8_ssse3(const int16_t *input, tran_low_t *output, int stride);
RTCD_EXTERN void (*aom_fdct8x8)(const int16_t *input, tran_low_t *output, int stride);

void aom_fft16x16_float_c(const float *input, float *temp, float *output);
void aom_fft16x16_float_sse2(const float *input, float *temp, float *output);
void aom_fft16x16_float_avx2(const float *input, float *temp, float *output);
RTCD_EXTERN void (*aom_fft16x16_float)(const float *input, float *temp, float *output);

void aom_fft2x2_float_c(const float *input, float *temp, float *output);
#define aom_fft2x2_float aom_fft2x2_float_c

void aom_fft32x32_float_c(const float *input, float *temp, float *output);
void aom_fft32x32_float_sse2(const float *input, float *temp, float *output);
void aom_fft32x32_float_avx2(const float *input, float *temp, float *output);
RTCD_EXTERN void (*aom_fft32x32_float)(const float *input, float *temp, float *output);

void aom_fft4x4_float_c(const float *input, float *temp, float *output);
void aom_fft4x4_float_sse2(const float *input, float *temp, float *output);
#define aom_fft4x4_float aom_fft4x4_float_sse2

void aom_fft8x8_float_c(const float *input, float *temp, float *output);
void aom_fft8x8_float_sse2(const float *input, float *temp, float *output);
void aom_fft8x8_float_avx2(const float *input, float *temp, float *output);
RTCD_EXTERN void (*aom_fft8x8_float)(const float *input, float *temp, float *output);

void aom_get16x16var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_get16x16var aom_get16x16var_c

unsigned int aom_get4x4sse_cs_c(const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int ref_stride);
#define aom_get4x4sse_cs aom_get4x4sse_cs_c

void aom_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_get8x8var aom_get8x8var_c

unsigned int aom_get_mb_ss_c(const int16_t *);
unsigned int aom_get_mb_ss_sse2(const int16_t *);
#define aom_get_mb_ss aom_get_mb_ss_sse2

void aom_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x16 aom_h_predictor_16x16_sse2

void aom_h_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x32 aom_h_predictor_16x32_sse2

void aom_h_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x4 aom_h_predictor_16x4_sse2

void aom_h_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x64 aom_h_predictor_16x64_sse2

void aom_h_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x8 aom_h_predictor_16x8_sse2

void aom_h_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_2x2 aom_h_predictor_2x2_c

void aom_h_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_32x16 aom_h_predictor_32x16_sse2

void aom_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_h_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_h_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_32x64 aom_h_predictor_32x64_sse2

void aom_h_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_32x8 aom_h_predictor_32x8_sse2

void aom_h_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_4x16 aom_h_predictor_4x16_sse2

void aom_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_4x4 aom_h_predictor_4x4_sse2

void aom_h_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_4x8 aom_h_predictor_4x8_sse2

void aom_h_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_64x16 aom_h_predictor_64x16_sse2

void aom_h_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_64x32 aom_h_predictor_64x32_sse2

void aom_h_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_64x64 aom_h_predictor_64x64_sse2

void aom_h_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_8x16 aom_h_predictor_8x16_sse2

void aom_h_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_8x32 aom_h_predictor_8x32_sse2

void aom_h_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_8x4 aom_h_predictor_8x4_sse2

void aom_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_8x8 aom_h_predictor_8x8_sse2

void aom_highbd_10_get16x16var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_highbd_10_get16x16var aom_highbd_10_get16x16var_c

void aom_highbd_10_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_highbd_10_get8x8var aom_highbd_10_get8x8var_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance128x128 aom_highbd_10_jnt_sub_pixel_avg_variance128x128_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance128x64 aom_highbd_10_jnt_sub_pixel_avg_variance128x64_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance16x16 aom_highbd_10_jnt_sub_pixel_avg_variance16x16_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance16x32 aom_highbd_10_jnt_sub_pixel_avg_variance16x32_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance16x4 aom_highbd_10_jnt_sub_pixel_avg_variance16x4_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance16x64 aom_highbd_10_jnt_sub_pixel_avg_variance16x64_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance16x8 aom_highbd_10_jnt_sub_pixel_avg_variance16x8_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance32x16 aom_highbd_10_jnt_sub_pixel_avg_variance32x16_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance32x32 aom_highbd_10_jnt_sub_pixel_avg_variance32x32_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance32x64 aom_highbd_10_jnt_sub_pixel_avg_variance32x64_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance32x8 aom_highbd_10_jnt_sub_pixel_avg_variance32x8_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance4x16 aom_highbd_10_jnt_sub_pixel_avg_variance4x16_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance4x4 aom_highbd_10_jnt_sub_pixel_avg_variance4x4_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance4x8 aom_highbd_10_jnt_sub_pixel_avg_variance4x8_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance64x128 aom_highbd_10_jnt_sub_pixel_avg_variance64x128_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance64x16 aom_highbd_10_jnt_sub_pixel_avg_variance64x16_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance64x32 aom_highbd_10_jnt_sub_pixel_avg_variance64x32_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance64x64 aom_highbd_10_jnt_sub_pixel_avg_variance64x64_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance8x16 aom_highbd_10_jnt_sub_pixel_avg_variance8x16_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance8x32 aom_highbd_10_jnt_sub_pixel_avg_variance8x32_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance8x4 aom_highbd_10_jnt_sub_pixel_avg_variance8x4_c

uint32_t aom_highbd_10_jnt_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_10_jnt_sub_pixel_avg_variance8x8 aom_highbd_10_jnt_sub_pixel_avg_variance8x8_c

unsigned int aom_highbd_10_masked_sub_pixel_variance128x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance128x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance128x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance128x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance128x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance128x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance16x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance16x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance16x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance16x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance16x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance16x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance16x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance16x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance16x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance16x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance16x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance16x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance16x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance16x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance16x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance32x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance32x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance32x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance32x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance32x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance32x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance32x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance32x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance32x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance32x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance32x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance32x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance4x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance4x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance4x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance4x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance4x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance4x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance4x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance4x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance4x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance64x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance64x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance64x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance64x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance64x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance64x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance64x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance64x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance64x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance64x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance64x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance64x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance8x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance8x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance8x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance8x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance8x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance8x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance8x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance8x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance8x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_masked_sub_pixel_variance8x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_10_masked_sub_pixel_variance8x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_masked_sub_pixel_variance8x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_10_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_highbd_10_mse16x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_10_mse16x16 aom_highbd_10_mse16x16_sse2

unsigned int aom_highbd_10_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_10_mse16x8 aom_highbd_10_mse16x8_c

unsigned int aom_highbd_10_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_10_mse8x16 aom_highbd_10_mse8x16_c

unsigned int aom_highbd_10_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_highbd_10_mse8x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_10_mse8x8 aom_highbd_10_mse8x8_sse2

unsigned int aom_highbd_10_obmc_sub_pixel_variance128x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance128x128 aom_highbd_10_obmc_sub_pixel_variance128x128_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance128x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance128x64 aom_highbd_10_obmc_sub_pixel_variance128x64_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance16x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance16x16 aom_highbd_10_obmc_sub_pixel_variance16x16_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance16x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance16x32 aom_highbd_10_obmc_sub_pixel_variance16x32_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance16x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance16x4 aom_highbd_10_obmc_sub_pixel_variance16x4_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance16x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance16x64 aom_highbd_10_obmc_sub_pixel_variance16x64_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance16x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance16x8 aom_highbd_10_obmc_sub_pixel_variance16x8_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance32x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance32x16 aom_highbd_10_obmc_sub_pixel_variance32x16_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance32x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance32x32 aom_highbd_10_obmc_sub_pixel_variance32x32_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance32x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance32x64 aom_highbd_10_obmc_sub_pixel_variance32x64_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance32x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance32x8 aom_highbd_10_obmc_sub_pixel_variance32x8_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance4x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance4x16 aom_highbd_10_obmc_sub_pixel_variance4x16_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance4x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance4x4 aom_highbd_10_obmc_sub_pixel_variance4x4_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance4x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance4x8 aom_highbd_10_obmc_sub_pixel_variance4x8_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance64x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance64x128 aom_highbd_10_obmc_sub_pixel_variance64x128_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance64x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance64x16 aom_highbd_10_obmc_sub_pixel_variance64x16_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance64x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance64x32 aom_highbd_10_obmc_sub_pixel_variance64x32_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance64x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance64x64 aom_highbd_10_obmc_sub_pixel_variance64x64_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance8x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance8x16 aom_highbd_10_obmc_sub_pixel_variance8x16_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance8x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance8x32 aom_highbd_10_obmc_sub_pixel_variance8x32_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance8x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance8x4 aom_highbd_10_obmc_sub_pixel_variance8x4_c

unsigned int aom_highbd_10_obmc_sub_pixel_variance8x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_10_obmc_sub_pixel_variance8x8 aom_highbd_10_obmc_sub_pixel_variance8x8_c

unsigned int aom_highbd_10_obmc_variance128x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance128x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance128x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance128x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance128x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance128x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance16x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance16x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance16x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance16x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance16x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance16x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance16x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance16x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance16x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance16x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance16x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance16x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance16x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance16x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance16x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance32x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance32x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance32x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance32x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance32x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance32x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance32x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance32x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance32x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance32x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance32x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance32x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance4x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance4x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance4x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance4x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance4x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance4x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance4x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance4x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance4x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance64x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance64x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance64x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance64x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance64x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance64x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance64x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance64x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance64x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance64x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance64x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance64x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance8x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance8x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance8x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance8x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance8x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance8x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance8x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance8x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance8x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_10_obmc_variance8x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_10_obmc_variance8x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_obmc_variance8x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

uint32_t aom_highbd_10_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance128x128 aom_highbd_10_sub_pixel_avg_variance128x128_c

uint32_t aom_highbd_10_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance128x64 aom_highbd_10_sub_pixel_avg_variance128x64_c

uint32_t aom_highbd_10_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance16x16 aom_highbd_10_sub_pixel_avg_variance16x16_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance16x32 aom_highbd_10_sub_pixel_avg_variance16x32_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance16x4 aom_highbd_10_sub_pixel_avg_variance16x4_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance16x64 aom_highbd_10_sub_pixel_avg_variance16x64_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance16x8 aom_highbd_10_sub_pixel_avg_variance16x8_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance32x16 aom_highbd_10_sub_pixel_avg_variance32x16_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance32x32 aom_highbd_10_sub_pixel_avg_variance32x32_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance32x64 aom_highbd_10_sub_pixel_avg_variance32x64_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance32x8 aom_highbd_10_sub_pixel_avg_variance32x8_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance4x16 aom_highbd_10_sub_pixel_avg_variance4x16_c

uint32_t aom_highbd_10_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_highbd_10_sub_pixel_avg_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_highbd_10_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance4x8 aom_highbd_10_sub_pixel_avg_variance4x8_c

uint32_t aom_highbd_10_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance64x128 aom_highbd_10_sub_pixel_avg_variance64x128_c

uint32_t aom_highbd_10_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance64x16 aom_highbd_10_sub_pixel_avg_variance64x16_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance64x32 aom_highbd_10_sub_pixel_avg_variance64x32_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance64x64 aom_highbd_10_sub_pixel_avg_variance64x64_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance8x16 aom_highbd_10_sub_pixel_avg_variance8x16_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance8x32 aom_highbd_10_sub_pixel_avg_variance8x32_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance8x4 aom_highbd_10_sub_pixel_avg_variance8x4_sse2

uint32_t aom_highbd_10_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_10_sub_pixel_avg_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_10_sub_pixel_avg_variance8x8 aom_highbd_10_sub_pixel_avg_variance8x8_sse2

uint32_t aom_highbd_10_sub_pixel_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance128x128 aom_highbd_10_sub_pixel_variance128x128_c

uint32_t aom_highbd_10_sub_pixel_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance128x64 aom_highbd_10_sub_pixel_variance128x64_c

uint32_t aom_highbd_10_sub_pixel_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance16x16 aom_highbd_10_sub_pixel_variance16x16_sse2

uint32_t aom_highbd_10_sub_pixel_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance16x32 aom_highbd_10_sub_pixel_variance16x32_sse2

uint32_t aom_highbd_10_sub_pixel_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance16x4 aom_highbd_10_sub_pixel_variance16x4_sse2

uint32_t aom_highbd_10_sub_pixel_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance16x64 aom_highbd_10_sub_pixel_variance16x64_sse2

uint32_t aom_highbd_10_sub_pixel_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance16x8 aom_highbd_10_sub_pixel_variance16x8_sse2

uint32_t aom_highbd_10_sub_pixel_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance32x16 aom_highbd_10_sub_pixel_variance32x16_sse2

uint32_t aom_highbd_10_sub_pixel_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance32x32 aom_highbd_10_sub_pixel_variance32x32_sse2

uint32_t aom_highbd_10_sub_pixel_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance32x64 aom_highbd_10_sub_pixel_variance32x64_sse2

uint32_t aom_highbd_10_sub_pixel_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance32x8 aom_highbd_10_sub_pixel_variance32x8_sse2

uint32_t aom_highbd_10_sub_pixel_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance4x16 aom_highbd_10_sub_pixel_variance4x16_c

uint32_t aom_highbd_10_sub_pixel_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_highbd_10_sub_pixel_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_highbd_10_sub_pixel_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance4x8 aom_highbd_10_sub_pixel_variance4x8_c

uint32_t aom_highbd_10_sub_pixel_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance64x128 aom_highbd_10_sub_pixel_variance64x128_c

uint32_t aom_highbd_10_sub_pixel_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance64x16 aom_highbd_10_sub_pixel_variance64x16_sse2

uint32_t aom_highbd_10_sub_pixel_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance64x32 aom_highbd_10_sub_pixel_variance64x32_sse2

uint32_t aom_highbd_10_sub_pixel_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance64x64 aom_highbd_10_sub_pixel_variance64x64_sse2

uint32_t aom_highbd_10_sub_pixel_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance8x16 aom_highbd_10_sub_pixel_variance8x16_sse2

uint32_t aom_highbd_10_sub_pixel_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance8x32 aom_highbd_10_sub_pixel_variance8x32_sse2

uint32_t aom_highbd_10_sub_pixel_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance8x4 aom_highbd_10_sub_pixel_variance8x4_sse2

uint32_t aom_highbd_10_sub_pixel_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_10_sub_pixel_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_sub_pixel_variance8x8 aom_highbd_10_sub_pixel_variance8x8_sse2

unsigned int aom_highbd_10_variance128x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance128x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance128x128_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance128x128)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance128x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance128x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance128x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance128x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance16x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance16x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance16x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance16x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance16x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_variance16x4 aom_highbd_10_variance16x4_c

unsigned int aom_highbd_10_variance16x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_10_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_variance16x64 aom_highbd_10_variance16x64_sse2

unsigned int aom_highbd_10_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance16x8_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance16x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance2x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_10_variance2x2 aom_highbd_10_variance2x2_c

unsigned int aom_highbd_10_variance2x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_10_variance2x4 aom_highbd_10_variance2x4_c

unsigned int aom_highbd_10_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance32x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance32x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance32x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance32x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance32x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance32x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_10_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_variance32x8 aom_highbd_10_variance32x8_sse2

unsigned int aom_highbd_10_variance4x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_variance4x16 aom_highbd_10_variance4x16_c

unsigned int aom_highbd_10_variance4x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_10_variance4x2 aom_highbd_10_variance4x2_c

unsigned int aom_highbd_10_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance4x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_10_variance4x8 aom_highbd_10_variance4x8_c

unsigned int aom_highbd_10_variance64x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance64x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance64x128_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance64x128)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance64x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_10_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_variance64x16 aom_highbd_10_variance64x16_sse2

unsigned int aom_highbd_10_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance64x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance64x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance64x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance8x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance8x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_10_variance8x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_10_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_10_variance8x32 aom_highbd_10_variance8x32_sse2

unsigned int aom_highbd_10_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_10_variance8x4 aom_highbd_10_variance8x4_c

unsigned int aom_highbd_10_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_10_variance8x8_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_10_variance8x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

void aom_highbd_12_get16x16var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_highbd_12_get16x16var aom_highbd_12_get16x16var_c

void aom_highbd_12_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_highbd_12_get8x8var aom_highbd_12_get8x8var_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance128x128 aom_highbd_12_jnt_sub_pixel_avg_variance128x128_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance128x64 aom_highbd_12_jnt_sub_pixel_avg_variance128x64_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance16x16 aom_highbd_12_jnt_sub_pixel_avg_variance16x16_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance16x32 aom_highbd_12_jnt_sub_pixel_avg_variance16x32_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance16x4 aom_highbd_12_jnt_sub_pixel_avg_variance16x4_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance16x64 aom_highbd_12_jnt_sub_pixel_avg_variance16x64_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance16x8 aom_highbd_12_jnt_sub_pixel_avg_variance16x8_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance32x16 aom_highbd_12_jnt_sub_pixel_avg_variance32x16_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance32x32 aom_highbd_12_jnt_sub_pixel_avg_variance32x32_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance32x64 aom_highbd_12_jnt_sub_pixel_avg_variance32x64_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance32x8 aom_highbd_12_jnt_sub_pixel_avg_variance32x8_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance4x16 aom_highbd_12_jnt_sub_pixel_avg_variance4x16_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance4x4 aom_highbd_12_jnt_sub_pixel_avg_variance4x4_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance4x8 aom_highbd_12_jnt_sub_pixel_avg_variance4x8_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance64x128 aom_highbd_12_jnt_sub_pixel_avg_variance64x128_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance64x16 aom_highbd_12_jnt_sub_pixel_avg_variance64x16_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance64x32 aom_highbd_12_jnt_sub_pixel_avg_variance64x32_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance64x64 aom_highbd_12_jnt_sub_pixel_avg_variance64x64_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance8x16 aom_highbd_12_jnt_sub_pixel_avg_variance8x16_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance8x32 aom_highbd_12_jnt_sub_pixel_avg_variance8x32_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance8x4 aom_highbd_12_jnt_sub_pixel_avg_variance8x4_c

uint32_t aom_highbd_12_jnt_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_12_jnt_sub_pixel_avg_variance8x8 aom_highbd_12_jnt_sub_pixel_avg_variance8x8_c

unsigned int aom_highbd_12_masked_sub_pixel_variance128x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance128x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance128x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance128x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance128x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance128x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance16x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance16x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance16x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance16x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance16x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance16x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance16x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance16x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance16x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance16x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance16x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance16x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance16x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance16x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance16x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance32x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance32x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance32x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance32x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance32x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance32x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance32x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance32x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance32x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance32x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance32x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance32x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance4x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance4x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance4x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance4x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance4x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance4x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance4x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance4x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance4x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance64x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance64x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance64x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance64x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance64x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance64x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance64x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance64x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance64x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance64x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance64x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance64x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance8x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance8x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance8x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance8x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance8x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance8x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance8x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance8x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance8x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_masked_sub_pixel_variance8x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_12_masked_sub_pixel_variance8x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_masked_sub_pixel_variance8x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_12_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_highbd_12_mse16x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_12_mse16x16 aom_highbd_12_mse16x16_sse2

unsigned int aom_highbd_12_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_12_mse16x8 aom_highbd_12_mse16x8_c

unsigned int aom_highbd_12_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_12_mse8x16 aom_highbd_12_mse8x16_c

unsigned int aom_highbd_12_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_highbd_12_mse8x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_12_mse8x8 aom_highbd_12_mse8x8_sse2

unsigned int aom_highbd_12_obmc_sub_pixel_variance128x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance128x128 aom_highbd_12_obmc_sub_pixel_variance128x128_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance128x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance128x64 aom_highbd_12_obmc_sub_pixel_variance128x64_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance16x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance16x16 aom_highbd_12_obmc_sub_pixel_variance16x16_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance16x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance16x32 aom_highbd_12_obmc_sub_pixel_variance16x32_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance16x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance16x4 aom_highbd_12_obmc_sub_pixel_variance16x4_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance16x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance16x64 aom_highbd_12_obmc_sub_pixel_variance16x64_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance16x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance16x8 aom_highbd_12_obmc_sub_pixel_variance16x8_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance32x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance32x16 aom_highbd_12_obmc_sub_pixel_variance32x16_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance32x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance32x32 aom_highbd_12_obmc_sub_pixel_variance32x32_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance32x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance32x64 aom_highbd_12_obmc_sub_pixel_variance32x64_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance32x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance32x8 aom_highbd_12_obmc_sub_pixel_variance32x8_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance4x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance4x16 aom_highbd_12_obmc_sub_pixel_variance4x16_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance4x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance4x4 aom_highbd_12_obmc_sub_pixel_variance4x4_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance4x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance4x8 aom_highbd_12_obmc_sub_pixel_variance4x8_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance64x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance64x128 aom_highbd_12_obmc_sub_pixel_variance64x128_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance64x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance64x16 aom_highbd_12_obmc_sub_pixel_variance64x16_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance64x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance64x32 aom_highbd_12_obmc_sub_pixel_variance64x32_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance64x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance64x64 aom_highbd_12_obmc_sub_pixel_variance64x64_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance8x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance8x16 aom_highbd_12_obmc_sub_pixel_variance8x16_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance8x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance8x32 aom_highbd_12_obmc_sub_pixel_variance8x32_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance8x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance8x4 aom_highbd_12_obmc_sub_pixel_variance8x4_c

unsigned int aom_highbd_12_obmc_sub_pixel_variance8x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_12_obmc_sub_pixel_variance8x8 aom_highbd_12_obmc_sub_pixel_variance8x8_c

unsigned int aom_highbd_12_obmc_variance128x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance128x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance128x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance128x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance128x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance128x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance16x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance16x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance16x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance16x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance16x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance16x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance16x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance16x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance16x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance16x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance16x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance16x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance16x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance16x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance16x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance32x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance32x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance32x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance32x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance32x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance32x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance32x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance32x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance32x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance32x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance32x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance32x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance4x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance4x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance4x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance4x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance4x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance4x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance4x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance4x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance4x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance64x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance64x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance64x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance64x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance64x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance64x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance64x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance64x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance64x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance64x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance64x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance64x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance8x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance8x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance8x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance8x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance8x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance8x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance8x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance8x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance8x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_12_obmc_variance8x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_12_obmc_variance8x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_obmc_variance8x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

uint32_t aom_highbd_12_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance128x128 aom_highbd_12_sub_pixel_avg_variance128x128_c

uint32_t aom_highbd_12_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance128x64 aom_highbd_12_sub_pixel_avg_variance128x64_c

uint32_t aom_highbd_12_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance16x16 aom_highbd_12_sub_pixel_avg_variance16x16_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance16x32 aom_highbd_12_sub_pixel_avg_variance16x32_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance16x4 aom_highbd_12_sub_pixel_avg_variance16x4_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance16x64 aom_highbd_12_sub_pixel_avg_variance16x64_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance16x8 aom_highbd_12_sub_pixel_avg_variance16x8_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance32x16 aom_highbd_12_sub_pixel_avg_variance32x16_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance32x32 aom_highbd_12_sub_pixel_avg_variance32x32_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance32x64 aom_highbd_12_sub_pixel_avg_variance32x64_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance32x8 aom_highbd_12_sub_pixel_avg_variance32x8_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance4x16 aom_highbd_12_sub_pixel_avg_variance4x16_c

uint32_t aom_highbd_12_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_highbd_12_sub_pixel_avg_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_highbd_12_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance4x8 aom_highbd_12_sub_pixel_avg_variance4x8_c

uint32_t aom_highbd_12_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance64x128 aom_highbd_12_sub_pixel_avg_variance64x128_c

uint32_t aom_highbd_12_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance64x16 aom_highbd_12_sub_pixel_avg_variance64x16_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance64x32 aom_highbd_12_sub_pixel_avg_variance64x32_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance64x64 aom_highbd_12_sub_pixel_avg_variance64x64_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance8x16 aom_highbd_12_sub_pixel_avg_variance8x16_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance8x32 aom_highbd_12_sub_pixel_avg_variance8x32_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance8x4 aom_highbd_12_sub_pixel_avg_variance8x4_sse2

uint32_t aom_highbd_12_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_12_sub_pixel_avg_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_12_sub_pixel_avg_variance8x8 aom_highbd_12_sub_pixel_avg_variance8x8_sse2

uint32_t aom_highbd_12_sub_pixel_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance128x128 aom_highbd_12_sub_pixel_variance128x128_c

uint32_t aom_highbd_12_sub_pixel_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance128x64 aom_highbd_12_sub_pixel_variance128x64_c

uint32_t aom_highbd_12_sub_pixel_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance16x16 aom_highbd_12_sub_pixel_variance16x16_sse2

uint32_t aom_highbd_12_sub_pixel_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance16x32 aom_highbd_12_sub_pixel_variance16x32_sse2

uint32_t aom_highbd_12_sub_pixel_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance16x4 aom_highbd_12_sub_pixel_variance16x4_sse2

uint32_t aom_highbd_12_sub_pixel_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance16x64 aom_highbd_12_sub_pixel_variance16x64_sse2

uint32_t aom_highbd_12_sub_pixel_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance16x8 aom_highbd_12_sub_pixel_variance16x8_sse2

uint32_t aom_highbd_12_sub_pixel_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance32x16 aom_highbd_12_sub_pixel_variance32x16_sse2

uint32_t aom_highbd_12_sub_pixel_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance32x32 aom_highbd_12_sub_pixel_variance32x32_sse2

uint32_t aom_highbd_12_sub_pixel_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance32x64 aom_highbd_12_sub_pixel_variance32x64_sse2

uint32_t aom_highbd_12_sub_pixel_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance32x8 aom_highbd_12_sub_pixel_variance32x8_sse2

uint32_t aom_highbd_12_sub_pixel_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance4x16 aom_highbd_12_sub_pixel_variance4x16_c

uint32_t aom_highbd_12_sub_pixel_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_highbd_12_sub_pixel_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_highbd_12_sub_pixel_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance4x8 aom_highbd_12_sub_pixel_variance4x8_c

uint32_t aom_highbd_12_sub_pixel_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance64x128 aom_highbd_12_sub_pixel_variance64x128_c

uint32_t aom_highbd_12_sub_pixel_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance64x16 aom_highbd_12_sub_pixel_variance64x16_sse2

uint32_t aom_highbd_12_sub_pixel_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance64x32 aom_highbd_12_sub_pixel_variance64x32_sse2

uint32_t aom_highbd_12_sub_pixel_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance64x64 aom_highbd_12_sub_pixel_variance64x64_sse2

uint32_t aom_highbd_12_sub_pixel_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance8x16 aom_highbd_12_sub_pixel_variance8x16_sse2

uint32_t aom_highbd_12_sub_pixel_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance8x32 aom_highbd_12_sub_pixel_variance8x32_sse2

uint32_t aom_highbd_12_sub_pixel_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance8x4 aom_highbd_12_sub_pixel_variance8x4_sse2

uint32_t aom_highbd_12_sub_pixel_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_12_sub_pixel_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_sub_pixel_variance8x8 aom_highbd_12_sub_pixel_variance8x8_sse2

unsigned int aom_highbd_12_variance128x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance128x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance128x128 aom_highbd_12_variance128x128_sse2

unsigned int aom_highbd_12_variance128x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance128x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance128x64 aom_highbd_12_variance128x64_sse2

unsigned int aom_highbd_12_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance16x16 aom_highbd_12_variance16x16_sse2

unsigned int aom_highbd_12_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance16x32 aom_highbd_12_variance16x32_sse2

unsigned int aom_highbd_12_variance16x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_variance16x4 aom_highbd_12_variance16x4_c

unsigned int aom_highbd_12_variance16x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_12_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_variance16x64 aom_highbd_12_variance16x64_sse2

unsigned int aom_highbd_12_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance16x8 aom_highbd_12_variance16x8_sse2

unsigned int aom_highbd_12_variance2x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance2x2 aom_highbd_12_variance2x2_c

unsigned int aom_highbd_12_variance2x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance2x4 aom_highbd_12_variance2x4_c

unsigned int aom_highbd_12_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance32x16 aom_highbd_12_variance32x16_sse2

unsigned int aom_highbd_12_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance32x32 aom_highbd_12_variance32x32_sse2

unsigned int aom_highbd_12_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance32x64 aom_highbd_12_variance32x64_sse2

unsigned int aom_highbd_12_variance32x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_12_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_variance32x8 aom_highbd_12_variance32x8_sse2

unsigned int aom_highbd_12_variance4x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_variance4x16 aom_highbd_12_variance4x16_c

unsigned int aom_highbd_12_variance4x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance4x2 aom_highbd_12_variance4x2_c

unsigned int aom_highbd_12_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_12_variance4x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_12_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance4x8 aom_highbd_12_variance4x8_c

unsigned int aom_highbd_12_variance64x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance64x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance64x128 aom_highbd_12_variance64x128_sse2

unsigned int aom_highbd_12_variance64x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_12_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_variance64x16 aom_highbd_12_variance64x16_sse2

unsigned int aom_highbd_12_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance64x32 aom_highbd_12_variance64x32_sse2

unsigned int aom_highbd_12_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance64x64 aom_highbd_12_variance64x64_sse2

unsigned int aom_highbd_12_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance8x16 aom_highbd_12_variance8x16_sse2

unsigned int aom_highbd_12_variance8x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_12_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_12_variance8x32 aom_highbd_12_variance8x32_sse2

unsigned int aom_highbd_12_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance8x4 aom_highbd_12_variance8x4_c

unsigned int aom_highbd_12_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_12_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_12_variance8x8 aom_highbd_12_variance8x8_sse2

void aom_highbd_8_get16x16var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_highbd_8_get16x16var aom_highbd_8_get16x16var_c

void aom_highbd_8_get8x8var_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum);
#define aom_highbd_8_get8x8var aom_highbd_8_get8x8var_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance128x128 aom_highbd_8_jnt_sub_pixel_avg_variance128x128_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance128x64 aom_highbd_8_jnt_sub_pixel_avg_variance128x64_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance16x16 aom_highbd_8_jnt_sub_pixel_avg_variance16x16_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance16x32 aom_highbd_8_jnt_sub_pixel_avg_variance16x32_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance16x4 aom_highbd_8_jnt_sub_pixel_avg_variance16x4_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance16x64 aom_highbd_8_jnt_sub_pixel_avg_variance16x64_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance16x8 aom_highbd_8_jnt_sub_pixel_avg_variance16x8_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance32x16 aom_highbd_8_jnt_sub_pixel_avg_variance32x16_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance32x32 aom_highbd_8_jnt_sub_pixel_avg_variance32x32_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance32x64 aom_highbd_8_jnt_sub_pixel_avg_variance32x64_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance32x8 aom_highbd_8_jnt_sub_pixel_avg_variance32x8_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance4x16 aom_highbd_8_jnt_sub_pixel_avg_variance4x16_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance4x4 aom_highbd_8_jnt_sub_pixel_avg_variance4x4_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance4x8 aom_highbd_8_jnt_sub_pixel_avg_variance4x8_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance64x128 aom_highbd_8_jnt_sub_pixel_avg_variance64x128_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance64x16 aom_highbd_8_jnt_sub_pixel_avg_variance64x16_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance64x32 aom_highbd_8_jnt_sub_pixel_avg_variance64x32_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance64x64 aom_highbd_8_jnt_sub_pixel_avg_variance64x64_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance8x16 aom_highbd_8_jnt_sub_pixel_avg_variance8x16_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance8x32 aom_highbd_8_jnt_sub_pixel_avg_variance8x32_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance8x4 aom_highbd_8_jnt_sub_pixel_avg_variance8x4_c

uint32_t aom_highbd_8_jnt_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_8_jnt_sub_pixel_avg_variance8x8 aom_highbd_8_jnt_sub_pixel_avg_variance8x8_c

unsigned int aom_highbd_8_masked_sub_pixel_variance128x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance128x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance128x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance128x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance128x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance128x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance16x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance16x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance16x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance16x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance16x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance16x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance16x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance16x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance16x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance16x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance16x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance16x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance16x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance16x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance16x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance32x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance32x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance32x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance32x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance32x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance32x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance32x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance32x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance32x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance32x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance32x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance32x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance4x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance4x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance4x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance4x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance4x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance4x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance4x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance4x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance4x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance64x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance64x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance64x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance64x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance64x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance64x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance64x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance64x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance64x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance64x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance64x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance64x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance8x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance8x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance8x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance8x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance8x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance8x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance8x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance8x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance8x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_masked_sub_pixel_variance8x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_highbd_8_masked_sub_pixel_variance8x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_masked_sub_pixel_variance8x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_highbd_8_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_highbd_8_mse16x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_8_mse16x16 aom_highbd_8_mse16x16_sse2

unsigned int aom_highbd_8_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_8_mse16x8 aom_highbd_8_mse16x8_c

unsigned int aom_highbd_8_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_8_mse8x16 aom_highbd_8_mse8x16_c

unsigned int aom_highbd_8_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_highbd_8_mse8x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_highbd_8_mse8x8 aom_highbd_8_mse8x8_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance128x128 aom_highbd_8_sub_pixel_avg_variance128x128_c

uint32_t aom_highbd_8_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance128x64 aom_highbd_8_sub_pixel_avg_variance128x64_c

uint32_t aom_highbd_8_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance16x16 aom_highbd_8_sub_pixel_avg_variance16x16_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance16x32 aom_highbd_8_sub_pixel_avg_variance16x32_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance16x4 aom_highbd_8_sub_pixel_avg_variance16x4_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance16x64 aom_highbd_8_sub_pixel_avg_variance16x64_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance16x8 aom_highbd_8_sub_pixel_avg_variance16x8_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance32x16 aom_highbd_8_sub_pixel_avg_variance32x16_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance32x32 aom_highbd_8_sub_pixel_avg_variance32x32_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance32x64 aom_highbd_8_sub_pixel_avg_variance32x64_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance32x8 aom_highbd_8_sub_pixel_avg_variance32x8_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance4x16 aom_highbd_8_sub_pixel_avg_variance4x16_c

uint32_t aom_highbd_8_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_highbd_8_sub_pixel_avg_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_highbd_8_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance4x8 aom_highbd_8_sub_pixel_avg_variance4x8_c

uint32_t aom_highbd_8_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance64x128 aom_highbd_8_sub_pixel_avg_variance64x128_c

uint32_t aom_highbd_8_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance64x16 aom_highbd_8_sub_pixel_avg_variance64x16_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance64x32 aom_highbd_8_sub_pixel_avg_variance64x32_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance64x64 aom_highbd_8_sub_pixel_avg_variance64x64_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance8x16 aom_highbd_8_sub_pixel_avg_variance8x16_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance8x32 aom_highbd_8_sub_pixel_avg_variance8x32_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance8x4 aom_highbd_8_sub_pixel_avg_variance8x4_sse2

uint32_t aom_highbd_8_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_highbd_8_sub_pixel_avg_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
#define aom_highbd_8_sub_pixel_avg_variance8x8 aom_highbd_8_sub_pixel_avg_variance8x8_sse2

uint32_t aom_highbd_8_sub_pixel_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance128x128 aom_highbd_8_sub_pixel_variance128x128_c

uint32_t aom_highbd_8_sub_pixel_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance128x64 aom_highbd_8_sub_pixel_variance128x64_c

uint32_t aom_highbd_8_sub_pixel_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance16x16 aom_highbd_8_sub_pixel_variance16x16_sse2

uint32_t aom_highbd_8_sub_pixel_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance16x32 aom_highbd_8_sub_pixel_variance16x32_sse2

uint32_t aom_highbd_8_sub_pixel_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance16x4 aom_highbd_8_sub_pixel_variance16x4_sse2

uint32_t aom_highbd_8_sub_pixel_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance16x64 aom_highbd_8_sub_pixel_variance16x64_sse2

uint32_t aom_highbd_8_sub_pixel_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance16x8 aom_highbd_8_sub_pixel_variance16x8_sse2

uint32_t aom_highbd_8_sub_pixel_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance32x16 aom_highbd_8_sub_pixel_variance32x16_sse2

uint32_t aom_highbd_8_sub_pixel_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance32x32 aom_highbd_8_sub_pixel_variance32x32_sse2

uint32_t aom_highbd_8_sub_pixel_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance32x64 aom_highbd_8_sub_pixel_variance32x64_sse2

uint32_t aom_highbd_8_sub_pixel_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance32x8 aom_highbd_8_sub_pixel_variance32x8_sse2

uint32_t aom_highbd_8_sub_pixel_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance4x16 aom_highbd_8_sub_pixel_variance4x16_c

uint32_t aom_highbd_8_sub_pixel_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_highbd_8_sub_pixel_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_highbd_8_sub_pixel_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance4x8 aom_highbd_8_sub_pixel_variance4x8_c

uint32_t aom_highbd_8_sub_pixel_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance64x128 aom_highbd_8_sub_pixel_variance64x128_c

uint32_t aom_highbd_8_sub_pixel_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance64x16 aom_highbd_8_sub_pixel_variance64x16_sse2

uint32_t aom_highbd_8_sub_pixel_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance64x32 aom_highbd_8_sub_pixel_variance64x32_sse2

uint32_t aom_highbd_8_sub_pixel_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance64x64 aom_highbd_8_sub_pixel_variance64x64_sse2

uint32_t aom_highbd_8_sub_pixel_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance8x16 aom_highbd_8_sub_pixel_variance8x16_sse2

uint32_t aom_highbd_8_sub_pixel_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance8x32 aom_highbd_8_sub_pixel_variance8x32_sse2

uint32_t aom_highbd_8_sub_pixel_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance8x4 aom_highbd_8_sub_pixel_variance8x4_sse2

uint32_t aom_highbd_8_sub_pixel_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_highbd_8_sub_pixel_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_sub_pixel_variance8x8 aom_highbd_8_sub_pixel_variance8x8_sse2

unsigned int aom_highbd_8_variance128x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance128x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance128x128 aom_highbd_8_variance128x128_sse2

unsigned int aom_highbd_8_variance128x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance128x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance128x64 aom_highbd_8_variance128x64_sse2

unsigned int aom_highbd_8_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance16x16 aom_highbd_8_variance16x16_sse2

unsigned int aom_highbd_8_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance16x32 aom_highbd_8_variance16x32_sse2

unsigned int aom_highbd_8_variance16x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_variance16x4 aom_highbd_8_variance16x4_c

unsigned int aom_highbd_8_variance16x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_8_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_variance16x64 aom_highbd_8_variance16x64_sse2

unsigned int aom_highbd_8_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance16x8 aom_highbd_8_variance16x8_sse2

unsigned int aom_highbd_8_variance2x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance2x2 aom_highbd_8_variance2x2_c

unsigned int aom_highbd_8_variance2x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance2x4 aom_highbd_8_variance2x4_c

unsigned int aom_highbd_8_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance32x16 aom_highbd_8_variance32x16_sse2

unsigned int aom_highbd_8_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance32x32 aom_highbd_8_variance32x32_sse2

unsigned int aom_highbd_8_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance32x64 aom_highbd_8_variance32x64_sse2

unsigned int aom_highbd_8_variance32x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_8_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_variance32x8 aom_highbd_8_variance32x8_sse2

unsigned int aom_highbd_8_variance4x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_variance4x16 aom_highbd_8_variance4x16_c

unsigned int aom_highbd_8_variance4x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance4x2 aom_highbd_8_variance4x2_c

unsigned int aom_highbd_8_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance4x4_sse4_1(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_8_variance4x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_highbd_8_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance4x8 aom_highbd_8_variance4x8_c

unsigned int aom_highbd_8_variance64x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance64x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance64x128 aom_highbd_8_variance64x128_sse2

unsigned int aom_highbd_8_variance64x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_8_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_variance64x16 aom_highbd_8_variance64x16_sse2

unsigned int aom_highbd_8_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance64x32 aom_highbd_8_variance64x32_sse2

unsigned int aom_highbd_8_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance64x64 aom_highbd_8_variance64x64_sse2

unsigned int aom_highbd_8_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance8x16 aom_highbd_8_variance8x16_sse2

unsigned int aom_highbd_8_variance8x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
unsigned int aom_highbd_8_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
#define aom_highbd_8_variance8x32 aom_highbd_8_variance8x32_sse2

unsigned int aom_highbd_8_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance8x4 aom_highbd_8_variance8x4_c

unsigned int aom_highbd_8_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_highbd_8_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_highbd_8_variance8x8 aom_highbd_8_variance8x8_sse2

void aom_highbd_blend_a64_d16_mask_c(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, ConvolveParams *conv_params, const int bd);
#define aom_highbd_blend_a64_d16_mask aom_highbd_blend_a64_d16_mask_c

void aom_highbd_blend_a64_hmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);
void aom_highbd_blend_a64_hmask_sse4_1(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);
RTCD_EXTERN void (*aom_highbd_blend_a64_hmask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);

void aom_highbd_blend_a64_mask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, int bd);
void aom_highbd_blend_a64_mask_sse4_1(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, int bd);
RTCD_EXTERN void (*aom_highbd_blend_a64_mask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, int bd);

void aom_highbd_blend_a64_vmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);
void aom_highbd_blend_a64_vmask_sse4_1(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);
RTCD_EXTERN void (*aom_highbd_blend_a64_vmask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);

void aom_highbd_comp_avg_pred_c(uint8_t *comp_pred8, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride);
#define aom_highbd_comp_avg_pred aom_highbd_comp_avg_pred_c

void aom_highbd_comp_avg_upsampled_pred_c(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                          const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
                                                          int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd, int subpel_search);
void aom_highbd_comp_avg_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                          const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
                                                          int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd, int subpel_search);
#define aom_highbd_comp_avg_upsampled_pred aom_highbd_comp_avg_upsampled_pred_sse2

void aom_highbd_comp_mask_pred_c(uint8_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);
void aom_highbd_comp_mask_pred_sse2(uint8_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);
void aom_highbd_comp_mask_pred_avx2(uint8_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);
RTCD_EXTERN void (*aom_highbd_comp_mask_pred)(uint8_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const uint8_t *mask, int mask_stride, int invert_mask);

void aom_highbd_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void aom_highbd_convolve8_horiz_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void aom_highbd_convolve8_horiz_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
RTCD_EXTERN void (*aom_highbd_convolve8_horiz)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);

void aom_highbd_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void aom_highbd_convolve8_vert_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void aom_highbd_convolve8_vert_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
RTCD_EXTERN void (*aom_highbd_convolve8_vert)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);

void aom_highbd_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void aom_highbd_convolve_copy_sse2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
void aom_highbd_convolve_copy_avx2(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
RTCD_EXTERN void (*aom_highbd_convolve_copy)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);

void aom_highbd_dc_128_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x16 aom_highbd_dc_128_predictor_16x16_sse2

void aom_highbd_dc_128_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x32 aom_highbd_dc_128_predictor_16x32_sse2

void aom_highbd_dc_128_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x4 aom_highbd_dc_128_predictor_16x4_c

void aom_highbd_dc_128_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x64 aom_highbd_dc_128_predictor_16x64_c

void aom_highbd_dc_128_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x8 aom_highbd_dc_128_predictor_16x8_sse2

void aom_highbd_dc_128_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_2x2 aom_highbd_dc_128_predictor_2x2_c

void aom_highbd_dc_128_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x16 aom_highbd_dc_128_predictor_32x16_sse2

void aom_highbd_dc_128_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x32 aom_highbd_dc_128_predictor_32x32_sse2

void aom_highbd_dc_128_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x64 aom_highbd_dc_128_predictor_32x64_c

void aom_highbd_dc_128_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x8 aom_highbd_dc_128_predictor_32x8_c

void aom_highbd_dc_128_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_4x16 aom_highbd_dc_128_predictor_4x16_c

void aom_highbd_dc_128_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_4x4 aom_highbd_dc_128_predictor_4x4_sse2

void aom_highbd_dc_128_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_4x8 aom_highbd_dc_128_predictor_4x8_sse2

void aom_highbd_dc_128_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_64x16 aom_highbd_dc_128_predictor_64x16_c

void aom_highbd_dc_128_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_64x32 aom_highbd_dc_128_predictor_64x32_c

void aom_highbd_dc_128_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_64x64 aom_highbd_dc_128_predictor_64x64_c

void aom_highbd_dc_128_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x16 aom_highbd_dc_128_predictor_8x16_sse2

void aom_highbd_dc_128_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x32 aom_highbd_dc_128_predictor_8x32_c

void aom_highbd_dc_128_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x4 aom_highbd_dc_128_predictor_8x4_sse2

void aom_highbd_dc_128_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_128_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x8 aom_highbd_dc_128_predictor_8x8_sse2

void aom_highbd_dc_left_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x16 aom_highbd_dc_left_predictor_16x16_sse2

void aom_highbd_dc_left_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x32 aom_highbd_dc_left_predictor_16x32_sse2

void aom_highbd_dc_left_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x4 aom_highbd_dc_left_predictor_16x4_c

void aom_highbd_dc_left_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x64 aom_highbd_dc_left_predictor_16x64_c

void aom_highbd_dc_left_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x8 aom_highbd_dc_left_predictor_16x8_sse2

void aom_highbd_dc_left_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_2x2 aom_highbd_dc_left_predictor_2x2_c

void aom_highbd_dc_left_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x16 aom_highbd_dc_left_predictor_32x16_sse2

void aom_highbd_dc_left_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x32 aom_highbd_dc_left_predictor_32x32_sse2

void aom_highbd_dc_left_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x64 aom_highbd_dc_left_predictor_32x64_c

void aom_highbd_dc_left_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x8 aom_highbd_dc_left_predictor_32x8_c

void aom_highbd_dc_left_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_4x16 aom_highbd_dc_left_predictor_4x16_c

void aom_highbd_dc_left_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_4x4 aom_highbd_dc_left_predictor_4x4_sse2

void aom_highbd_dc_left_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_4x8 aom_highbd_dc_left_predictor_4x8_sse2

void aom_highbd_dc_left_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_64x16 aom_highbd_dc_left_predictor_64x16_c

void aom_highbd_dc_left_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_64x32 aom_highbd_dc_left_predictor_64x32_c

void aom_highbd_dc_left_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_64x64 aom_highbd_dc_left_predictor_64x64_c

void aom_highbd_dc_left_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x16 aom_highbd_dc_left_predictor_8x16_sse2

void aom_highbd_dc_left_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x32 aom_highbd_dc_left_predictor_8x32_c

void aom_highbd_dc_left_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x4 aom_highbd_dc_left_predictor_8x4_sse2

void aom_highbd_dc_left_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_left_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x8 aom_highbd_dc_left_predictor_8x8_sse2

void aom_highbd_dc_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x16 aom_highbd_dc_predictor_16x16_sse2

void aom_highbd_dc_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x32 aom_highbd_dc_predictor_16x32_sse2

void aom_highbd_dc_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x4 aom_highbd_dc_predictor_16x4_c

void aom_highbd_dc_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x64 aom_highbd_dc_predictor_16x64_c

void aom_highbd_dc_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x8 aom_highbd_dc_predictor_16x8_sse2

void aom_highbd_dc_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_2x2 aom_highbd_dc_predictor_2x2_c

void aom_highbd_dc_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_32x16 aom_highbd_dc_predictor_32x16_sse2

void aom_highbd_dc_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_32x32 aom_highbd_dc_predictor_32x32_sse2

void aom_highbd_dc_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_32x64 aom_highbd_dc_predictor_32x64_c

void aom_highbd_dc_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_32x8 aom_highbd_dc_predictor_32x8_c

void aom_highbd_dc_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_4x16 aom_highbd_dc_predictor_4x16_c

void aom_highbd_dc_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_4x4 aom_highbd_dc_predictor_4x4_sse2

void aom_highbd_dc_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_4x8 aom_highbd_dc_predictor_4x8_sse2

void aom_highbd_dc_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_64x16 aom_highbd_dc_predictor_64x16_c

void aom_highbd_dc_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_64x32 aom_highbd_dc_predictor_64x32_c

void aom_highbd_dc_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_64x64 aom_highbd_dc_predictor_64x64_c

void aom_highbd_dc_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_8x16 aom_highbd_dc_predictor_8x16_sse2

void aom_highbd_dc_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_8x32 aom_highbd_dc_predictor_8x32_c

void aom_highbd_dc_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_8x4 aom_highbd_dc_predictor_8x4_sse2

void aom_highbd_dc_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_8x8 aom_highbd_dc_predictor_8x8_sse2

void aom_highbd_dc_top_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x16 aom_highbd_dc_top_predictor_16x16_sse2

void aom_highbd_dc_top_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x32 aom_highbd_dc_top_predictor_16x32_sse2

void aom_highbd_dc_top_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x4 aom_highbd_dc_top_predictor_16x4_c

void aom_highbd_dc_top_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x64 aom_highbd_dc_top_predictor_16x64_c

void aom_highbd_dc_top_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x8 aom_highbd_dc_top_predictor_16x8_sse2

void aom_highbd_dc_top_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_2x2 aom_highbd_dc_top_predictor_2x2_c

void aom_highbd_dc_top_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x16 aom_highbd_dc_top_predictor_32x16_sse2

void aom_highbd_dc_top_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x32 aom_highbd_dc_top_predictor_32x32_sse2

void aom_highbd_dc_top_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x64 aom_highbd_dc_top_predictor_32x64_c

void aom_highbd_dc_top_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x8 aom_highbd_dc_top_predictor_32x8_c

void aom_highbd_dc_top_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_4x16 aom_highbd_dc_top_predictor_4x16_c

void aom_highbd_dc_top_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_4x4 aom_highbd_dc_top_predictor_4x4_sse2

void aom_highbd_dc_top_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_4x8 aom_highbd_dc_top_predictor_4x8_sse2

void aom_highbd_dc_top_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_64x16 aom_highbd_dc_top_predictor_64x16_c

void aom_highbd_dc_top_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_64x32 aom_highbd_dc_top_predictor_64x32_c

void aom_highbd_dc_top_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_64x64 aom_highbd_dc_top_predictor_64x64_c

void aom_highbd_dc_top_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x16 aom_highbd_dc_top_predictor_8x16_sse2

void aom_highbd_dc_top_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x32 aom_highbd_dc_top_predictor_8x32_c

void aom_highbd_dc_top_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x4 aom_highbd_dc_top_predictor_8x4_sse2

void aom_highbd_dc_top_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_top_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x8 aom_highbd_dc_top_predictor_8x8_sse2

void aom_highbd_fdct8x8_c(const int16_t *input, tran_low_t *output, int stride);
void aom_highbd_fdct8x8_sse2(const int16_t *input, tran_low_t *output, int stride);
#define aom_highbd_fdct8x8 aom_highbd_fdct8x8_sse2

void aom_highbd_h_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x16 aom_highbd_h_predictor_16x16_sse2

void aom_highbd_h_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x32 aom_highbd_h_predictor_16x32_sse2

void aom_highbd_h_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x4 aom_highbd_h_predictor_16x4_c

void aom_highbd_h_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x64 aom_highbd_h_predictor_16x64_c

void aom_highbd_h_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x8 aom_highbd_h_predictor_16x8_sse2

void aom_highbd_h_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_2x2 aom_highbd_h_predictor_2x2_c

void aom_highbd_h_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x16 aom_highbd_h_predictor_32x16_sse2

void aom_highbd_h_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x32 aom_highbd_h_predictor_32x32_sse2

void aom_highbd_h_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x64 aom_highbd_h_predictor_32x64_c

void aom_highbd_h_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x8 aom_highbd_h_predictor_32x8_c

void aom_highbd_h_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_4x16 aom_highbd_h_predictor_4x16_c

void aom_highbd_h_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_4x4 aom_highbd_h_predictor_4x4_sse2

void aom_highbd_h_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_4x8 aom_highbd_h_predictor_4x8_sse2

void aom_highbd_h_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_64x16 aom_highbd_h_predictor_64x16_c

void aom_highbd_h_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_64x32 aom_highbd_h_predictor_64x32_c

void aom_highbd_h_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_64x64 aom_highbd_h_predictor_64x64_c

void aom_highbd_h_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x16 aom_highbd_h_predictor_8x16_sse2

void aom_highbd_h_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x32 aom_highbd_h_predictor_8x32_c

void aom_highbd_h_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x4 aom_highbd_h_predictor_8x4_sse2

void aom_highbd_h_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_h_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x8 aom_highbd_h_predictor_8x8_sse2

void aom_highbd_jnt_comp_avg_pred_c(uint8_t *comp_pred8, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const JNT_COMP_PARAMS *jcp_param);
void aom_highbd_jnt_comp_avg_pred_sse2(uint8_t *comp_pred8, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride, const JNT_COMP_PARAMS *jcp_param);
#define aom_highbd_jnt_comp_avg_pred aom_highbd_jnt_comp_avg_pred_sse2

void aom_highbd_jnt_comp_avg_upsampled_pred_c(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                              const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
                                                              int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
                                                              int ref_stride, int bd, const JNT_COMP_PARAMS *jcp_param, int subpel_search);
void aom_highbd_jnt_comp_avg_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                              const MV *const mv, uint8_t *comp_pred8, const uint8_t *pred8, int width,
                                                              int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref8,
                                                              int ref_stride, int bd, const JNT_COMP_PARAMS *jcp_param, int subpel_search);
#define aom_highbd_jnt_comp_avg_upsampled_pred aom_highbd_jnt_comp_avg_upsampled_pred_sse2

unsigned int aom_highbd_jnt_sad128x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad128x128_avg aom_highbd_jnt_sad128x128_avg_c

unsigned int aom_highbd_jnt_sad128x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad128x64_avg aom_highbd_jnt_sad128x64_avg_c

unsigned int aom_highbd_jnt_sad16x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad16x16_avg aom_highbd_jnt_sad16x16_avg_c

unsigned int aom_highbd_jnt_sad16x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad16x32_avg aom_highbd_jnt_sad16x32_avg_c

unsigned int aom_highbd_jnt_sad16x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad16x4_avg aom_highbd_jnt_sad16x4_avg_c

unsigned int aom_highbd_jnt_sad16x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad16x64_avg aom_highbd_jnt_sad16x64_avg_c

unsigned int aom_highbd_jnt_sad16x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad16x8_avg aom_highbd_jnt_sad16x8_avg_c

unsigned int aom_highbd_jnt_sad32x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad32x16_avg aom_highbd_jnt_sad32x16_avg_c

unsigned int aom_highbd_jnt_sad32x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad32x32_avg aom_highbd_jnt_sad32x32_avg_c

unsigned int aom_highbd_jnt_sad32x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad32x64_avg aom_highbd_jnt_sad32x64_avg_c

unsigned int aom_highbd_jnt_sad32x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad32x8_avg aom_highbd_jnt_sad32x8_avg_c

unsigned int aom_highbd_jnt_sad4x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad4x16_avg aom_highbd_jnt_sad4x16_avg_c

unsigned int aom_highbd_jnt_sad4x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad4x4_avg aom_highbd_jnt_sad4x4_avg_c

unsigned int aom_highbd_jnt_sad4x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad4x8_avg aom_highbd_jnt_sad4x8_avg_c

unsigned int aom_highbd_jnt_sad64x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad64x128_avg aom_highbd_jnt_sad64x128_avg_c

unsigned int aom_highbd_jnt_sad64x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad64x16_avg aom_highbd_jnt_sad64x16_avg_c

unsigned int aom_highbd_jnt_sad64x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad64x32_avg aom_highbd_jnt_sad64x32_avg_c

unsigned int aom_highbd_jnt_sad64x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad64x64_avg aom_highbd_jnt_sad64x64_avg_c

unsigned int aom_highbd_jnt_sad8x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad8x16_avg aom_highbd_jnt_sad8x16_avg_c

unsigned int aom_highbd_jnt_sad8x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad8x32_avg aom_highbd_jnt_sad8x32_avg_c

unsigned int aom_highbd_jnt_sad8x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad8x4_avg aom_highbd_jnt_sad8x4_avg_c

unsigned int aom_highbd_jnt_sad8x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS* jcp_param);
#define aom_highbd_jnt_sad8x8_avg aom_highbd_jnt_sad8x8_avg_c

void aom_highbd_lpf_horizontal_14_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_14_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_horizontal_14 aom_highbd_lpf_horizontal_14_sse2

void aom_highbd_lpf_horizontal_14_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limt1, const uint8_t *thresh1,int bd);
void aom_highbd_lpf_horizontal_14_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limt1, const uint8_t *thresh1,int bd);
void aom_highbd_lpf_horizontal_14_dual_avx2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limt1, const uint8_t *thresh1,int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_14_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limt1, const uint8_t *thresh1,int bd);

void aom_highbd_lpf_horizontal_4_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_4_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_horizontal_4 aom_highbd_lpf_horizontal_4_sse2

void aom_highbd_lpf_horizontal_4_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_4_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_4_dual_avx2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_4_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_horizontal_6_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_6_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_horizontal_6 aom_highbd_lpf_horizontal_6_sse2

void aom_highbd_lpf_horizontal_6_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_6_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
#define aom_highbd_lpf_horizontal_6_dual aom_highbd_lpf_horizontal_6_dual_sse2

void aom_highbd_lpf_horizontal_8_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_8_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_horizontal_8 aom_highbd_lpf_horizontal_8_sse2

void aom_highbd_lpf_horizontal_8_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_8_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_8_dual_avx2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_8_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_vertical_14_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_14_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_vertical_14 aom_highbd_lpf_vertical_14_sse2

void aom_highbd_lpf_vertical_14_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_14_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_14_dual_avx2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_14_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_vertical_4_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_4_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_vertical_4 aom_highbd_lpf_vertical_4_sse2

void aom_highbd_lpf_vertical_4_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_4_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_4_dual_avx2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_4_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_vertical_6_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_6_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_vertical_6 aom_highbd_lpf_vertical_6_sse2

void aom_highbd_lpf_vertical_6_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_6_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
#define aom_highbd_lpf_vertical_6_dual aom_highbd_lpf_vertical_6_dual_sse2

void aom_highbd_lpf_vertical_8_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_8_sse2(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
#define aom_highbd_lpf_vertical_8 aom_highbd_lpf_vertical_8_sse2

void aom_highbd_lpf_vertical_8_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_8_dual_sse2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_8_dual_avx2(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_8_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

unsigned int aom_highbd_masked_sad128x128_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad128x128_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad128x128_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad128x128)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad128x64_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad128x64_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad128x64_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad128x64)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad16x16_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x16_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x16_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad16x16)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad16x32_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x32_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x32_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad16x32)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad16x4_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x4_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x4_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad16x4)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad16x64_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x64_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x64_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad16x64)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad16x8_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x8_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad16x8_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad16x8)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad32x16_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x16_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x16_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad32x16)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad32x32_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x32_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x32_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad32x32)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad32x64_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x64_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x64_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad32x64)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad32x8_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x8_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad32x8_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad32x8)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad4x16_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad4x16_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad4x16_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad4x16)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad4x4_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad4x4_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad4x4_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad4x4)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad4x8_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad4x8_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad4x8_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad4x8)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad64x128_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x128_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x128_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad64x128)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad64x16_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x16_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x16_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad64x16)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad64x32_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x32_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x32_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad64x32)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad64x64_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x64_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad64x64_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad64x64)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad8x16_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x16_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x16_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad8x16)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad8x32_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x32_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x32_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad8x32)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad8x4_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x4_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x4_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad8x4)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_masked_sad8x8_c(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x8_ssse3(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_highbd_masked_sad8x8_avx2(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_highbd_masked_sad8x8)(const uint8_t *src8, int src_stride, const uint8_t *ref8, int ref_stride, const uint8_t *second_pred8, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_highbd_obmc_sad128x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad128x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad128x128_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad128x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad128x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad128x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad128x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad128x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad16x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad16x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad16x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad16x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad16x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad16x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad16x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad16x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad16x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad16x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad16x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad32x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad32x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad32x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad32x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad32x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad32x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad32x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad32x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad32x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad4x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad4x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad4x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad4x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad4x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad4x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad4x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad4x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad4x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad4x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad4x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad4x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad64x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x128_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad64x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad64x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad64x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad64x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad64x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad64x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad64x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad64x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad8x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad8x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad8x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad8x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad8x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad8x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sad8x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_highbd_obmc_sad8x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_sad8x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_highbd_obmc_sub_pixel_variance128x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance128x128 aom_highbd_obmc_sub_pixel_variance128x128_c

unsigned int aom_highbd_obmc_sub_pixel_variance128x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance128x64 aom_highbd_obmc_sub_pixel_variance128x64_c

unsigned int aom_highbd_obmc_sub_pixel_variance16x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance16x16 aom_highbd_obmc_sub_pixel_variance16x16_c

unsigned int aom_highbd_obmc_sub_pixel_variance16x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance16x32 aom_highbd_obmc_sub_pixel_variance16x32_c

unsigned int aom_highbd_obmc_sub_pixel_variance16x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance16x4 aom_highbd_obmc_sub_pixel_variance16x4_c

unsigned int aom_highbd_obmc_sub_pixel_variance16x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance16x64 aom_highbd_obmc_sub_pixel_variance16x64_c

unsigned int aom_highbd_obmc_sub_pixel_variance16x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance16x8 aom_highbd_obmc_sub_pixel_variance16x8_c

unsigned int aom_highbd_obmc_sub_pixel_variance32x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance32x16 aom_highbd_obmc_sub_pixel_variance32x16_c

unsigned int aom_highbd_obmc_sub_pixel_variance32x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance32x32 aom_highbd_obmc_sub_pixel_variance32x32_c

unsigned int aom_highbd_obmc_sub_pixel_variance32x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance32x64 aom_highbd_obmc_sub_pixel_variance32x64_c

unsigned int aom_highbd_obmc_sub_pixel_variance32x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance32x8 aom_highbd_obmc_sub_pixel_variance32x8_c

unsigned int aom_highbd_obmc_sub_pixel_variance4x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance4x16 aom_highbd_obmc_sub_pixel_variance4x16_c

unsigned int aom_highbd_obmc_sub_pixel_variance4x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance4x4 aom_highbd_obmc_sub_pixel_variance4x4_c

unsigned int aom_highbd_obmc_sub_pixel_variance4x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance4x8 aom_highbd_obmc_sub_pixel_variance4x8_c

unsigned int aom_highbd_obmc_sub_pixel_variance64x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance64x128 aom_highbd_obmc_sub_pixel_variance64x128_c

unsigned int aom_highbd_obmc_sub_pixel_variance64x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance64x16 aom_highbd_obmc_sub_pixel_variance64x16_c

unsigned int aom_highbd_obmc_sub_pixel_variance64x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance64x32 aom_highbd_obmc_sub_pixel_variance64x32_c

unsigned int aom_highbd_obmc_sub_pixel_variance64x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance64x64 aom_highbd_obmc_sub_pixel_variance64x64_c

unsigned int aom_highbd_obmc_sub_pixel_variance8x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance8x16 aom_highbd_obmc_sub_pixel_variance8x16_c

unsigned int aom_highbd_obmc_sub_pixel_variance8x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance8x32 aom_highbd_obmc_sub_pixel_variance8x32_c

unsigned int aom_highbd_obmc_sub_pixel_variance8x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance8x4 aom_highbd_obmc_sub_pixel_variance8x4_c

unsigned int aom_highbd_obmc_sub_pixel_variance8x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
#define aom_highbd_obmc_sub_pixel_variance8x8 aom_highbd_obmc_sub_pixel_variance8x8_c

unsigned int aom_highbd_obmc_variance128x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance128x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance128x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance128x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance128x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance128x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance16x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance16x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance16x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance16x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance16x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance16x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance16x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance16x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance16x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance16x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance16x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance16x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance16x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance16x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance16x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance32x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance32x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance32x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance32x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance32x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance32x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance32x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance32x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance32x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance32x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance32x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance32x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance4x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance4x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance4x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance4x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance4x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance4x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance4x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance4x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance4x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance64x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance64x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance64x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance64x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance64x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance64x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance64x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance64x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance64x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance64x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance64x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance64x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance8x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance8x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance8x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance8x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance8x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance8x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance8x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance8x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance8x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_highbd_obmc_variance8x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_highbd_obmc_variance8x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_highbd_obmc_variance8x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

void aom_highbd_paeth_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_16x16 aom_highbd_paeth_predictor_16x16_c

void aom_highbd_paeth_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_16x32 aom_highbd_paeth_predictor_16x32_c

void aom_highbd_paeth_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_16x4 aom_highbd_paeth_predictor_16x4_c

void aom_highbd_paeth_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_16x64 aom_highbd_paeth_predictor_16x64_c

void aom_highbd_paeth_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_16x8 aom_highbd_paeth_predictor_16x8_c

void aom_highbd_paeth_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_2x2 aom_highbd_paeth_predictor_2x2_c

void aom_highbd_paeth_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_32x16 aom_highbd_paeth_predictor_32x16_c

void aom_highbd_paeth_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_32x32 aom_highbd_paeth_predictor_32x32_c

void aom_highbd_paeth_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_32x64 aom_highbd_paeth_predictor_32x64_c

void aom_highbd_paeth_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_32x8 aom_highbd_paeth_predictor_32x8_c

void aom_highbd_paeth_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_4x16 aom_highbd_paeth_predictor_4x16_c

void aom_highbd_paeth_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_4x4 aom_highbd_paeth_predictor_4x4_c

void aom_highbd_paeth_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_4x8 aom_highbd_paeth_predictor_4x8_c

void aom_highbd_paeth_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_64x16 aom_highbd_paeth_predictor_64x16_c

void aom_highbd_paeth_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_64x32 aom_highbd_paeth_predictor_64x32_c

void aom_highbd_paeth_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_64x64 aom_highbd_paeth_predictor_64x64_c

void aom_highbd_paeth_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_8x16 aom_highbd_paeth_predictor_8x16_c

void aom_highbd_paeth_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_8x32 aom_highbd_paeth_predictor_8x32_c

void aom_highbd_paeth_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_8x4 aom_highbd_paeth_predictor_8x4_c

void aom_highbd_paeth_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_paeth_predictor_8x8 aom_highbd_paeth_predictor_8x8_c

void aom_highbd_quantize_b_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_highbd_quantize_b_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_highbd_quantize_b_avx2(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
RTCD_EXTERN void (*aom_highbd_quantize_b)(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);

void aom_highbd_quantize_b_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_highbd_quantize_b_32x32_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define aom_highbd_quantize_b_32x32 aom_highbd_quantize_b_32x32_sse2

void aom_highbd_quantize_b_64x64_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define aom_highbd_quantize_b_64x64 aom_highbd_quantize_b_64x64_c

unsigned int aom_highbd_sad128x128_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad128x128_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad128x128)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad128x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad128x128_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad128x128_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad128x128x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad128x128x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad128x128x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad128x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad128x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad128x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad128x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad128x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad128x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad128x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad128x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad128x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad16x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x16_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad16x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad16x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x16_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad16x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad16x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x16x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad16x16x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad16x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad16x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad16x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad16x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad16x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x32x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad16x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad16x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x4_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad16x4 aom_highbd_sad16x4_sse2

unsigned int aom_highbd_sad16x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x4_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad16x4_avg aom_highbd_sad16x4_avg_sse2

void aom_highbd_sad16x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad16x4x4d aom_highbd_sad16x4x4d_sse2

unsigned int aom_highbd_sad16x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad16x64 aom_highbd_sad16x64_sse2

unsigned int aom_highbd_sad16x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad16x64_avg aom_highbd_sad16x64_avg_sse2

void aom_highbd_sad16x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad16x64x4d aom_highbd_sad16x64x4d_sse2

unsigned int aom_highbd_sad16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad16x8_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad16x8)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad16x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad16x8_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad16x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad16x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad16x8x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad16x8x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad32x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad32x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad32x16_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad32x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad32x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad32x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad32x16_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad32x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad32x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad32x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad32x16x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad32x16x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad32x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad32x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad32x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad32x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad32x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad32x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad32x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad32x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad32x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad32x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad32x32x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad32x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad32x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad32x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad32x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad32x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad32x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad32x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad32x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad32x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad32x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad32x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad32x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad32x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad32x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad32x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad32x8 aom_highbd_sad32x8_sse2

unsigned int aom_highbd_sad32x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad32x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad32x8_avg aom_highbd_sad32x8_avg_sse2

void aom_highbd_sad32x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad32x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad32x8x4d aom_highbd_sad32x8x4d_sse2

unsigned int aom_highbd_sad4x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad4x16 aom_highbd_sad4x16_c

unsigned int aom_highbd_sad4x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad4x16_avg aom_highbd_sad4x16_avg_c

void aom_highbd_sad4x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad4x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad4x16x4d aom_highbd_sad4x16x4d_sse2

unsigned int aom_highbd_sad4x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad4x4 aom_highbd_sad4x4_c

unsigned int aom_highbd_sad4x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad4x4_avg aom_highbd_sad4x4_avg_c

void aom_highbd_sad4x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad4x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad4x4x4d aom_highbd_sad4x4x4d_sse2

unsigned int aom_highbd_sad4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad4x8 aom_highbd_sad4x8_c

unsigned int aom_highbd_sad4x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad4x8_avg aom_highbd_sad4x8_avg_c

void aom_highbd_sad4x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad4x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad4x8x4d aom_highbd_sad4x8x4d_sse2

unsigned int aom_highbd_sad64x128_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad64x128_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad64x128)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad64x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad64x128_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad64x128_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad64x128x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad64x128x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad64x128x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad64x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad64x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad64x16 aom_highbd_sad64x16_sse2

unsigned int aom_highbd_sad64x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad64x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad64x16_avg aom_highbd_sad64x16_avg_sse2

void aom_highbd_sad64x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad64x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad64x16x4d aom_highbd_sad64x16x4d_sse2

unsigned int aom_highbd_sad64x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad64x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad64x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad64x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad64x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad64x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad64x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad64x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad64x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad64x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad64x32x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad64x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad64x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad64x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad64x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_highbd_sad64x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_highbd_sad64x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad64x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad64x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_highbd_sad64x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_highbd_sad64x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad64x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad64x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_highbd_sad64x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_highbd_sad8x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad8x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad8x16 aom_highbd_sad8x16_sse2

unsigned int aom_highbd_sad8x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad8x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad8x16_avg aom_highbd_sad8x16_avg_sse2

void aom_highbd_sad8x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad8x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad8x16x4d aom_highbd_sad8x16x4d_sse2

unsigned int aom_highbd_sad8x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad8x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad8x32 aom_highbd_sad8x32_sse2

unsigned int aom_highbd_sad8x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad8x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad8x32_avg aom_highbd_sad8x32_avg_sse2

void aom_highbd_sad8x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad8x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad8x32x4d aom_highbd_sad8x32x4d_sse2

unsigned int aom_highbd_sad8x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad8x4_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad8x4 aom_highbd_sad8x4_sse2

unsigned int aom_highbd_sad8x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad8x4_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad8x4_avg aom_highbd_sad8x4_avg_sse2

void aom_highbd_sad8x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad8x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad8x4x4d aom_highbd_sad8x4x4d_sse2

unsigned int aom_highbd_sad8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_highbd_sad8x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_highbd_sad8x8 aom_highbd_sad8x8_sse2

unsigned int aom_highbd_sad8x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_highbd_sad8x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_highbd_sad8x8_avg aom_highbd_sad8x8_avg_sse2

void aom_highbd_sad8x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_highbd_sad8x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_highbd_sad8x8x4d aom_highbd_sad8x8x4d_sse2

void aom_highbd_smooth_h_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_16x16 aom_highbd_smooth_h_predictor_16x16_c

void aom_highbd_smooth_h_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_16x32 aom_highbd_smooth_h_predictor_16x32_c

void aom_highbd_smooth_h_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_16x4 aom_highbd_smooth_h_predictor_16x4_c

void aom_highbd_smooth_h_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_16x64 aom_highbd_smooth_h_predictor_16x64_c

void aom_highbd_smooth_h_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_16x8 aom_highbd_smooth_h_predictor_16x8_c

void aom_highbd_smooth_h_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_2x2 aom_highbd_smooth_h_predictor_2x2_c

void aom_highbd_smooth_h_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_32x16 aom_highbd_smooth_h_predictor_32x16_c

void aom_highbd_smooth_h_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_32x32 aom_highbd_smooth_h_predictor_32x32_c

void aom_highbd_smooth_h_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_32x64 aom_highbd_smooth_h_predictor_32x64_c

void aom_highbd_smooth_h_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_32x8 aom_highbd_smooth_h_predictor_32x8_c

void aom_highbd_smooth_h_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_4x16 aom_highbd_smooth_h_predictor_4x16_c

void aom_highbd_smooth_h_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_4x4 aom_highbd_smooth_h_predictor_4x4_c

void aom_highbd_smooth_h_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_4x8 aom_highbd_smooth_h_predictor_4x8_c

void aom_highbd_smooth_h_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_64x16 aom_highbd_smooth_h_predictor_64x16_c

void aom_highbd_smooth_h_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_64x32 aom_highbd_smooth_h_predictor_64x32_c

void aom_highbd_smooth_h_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_64x64 aom_highbd_smooth_h_predictor_64x64_c

void aom_highbd_smooth_h_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_8x16 aom_highbd_smooth_h_predictor_8x16_c

void aom_highbd_smooth_h_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_8x32 aom_highbd_smooth_h_predictor_8x32_c

void aom_highbd_smooth_h_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_8x4 aom_highbd_smooth_h_predictor_8x4_c

void aom_highbd_smooth_h_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_h_predictor_8x8 aom_highbd_smooth_h_predictor_8x8_c

void aom_highbd_smooth_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_16x16 aom_highbd_smooth_predictor_16x16_c

void aom_highbd_smooth_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_16x32 aom_highbd_smooth_predictor_16x32_c

void aom_highbd_smooth_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_16x4 aom_highbd_smooth_predictor_16x4_c

void aom_highbd_smooth_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_16x64 aom_highbd_smooth_predictor_16x64_c

void aom_highbd_smooth_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_16x8 aom_highbd_smooth_predictor_16x8_c

void aom_highbd_smooth_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_2x2 aom_highbd_smooth_predictor_2x2_c

void aom_highbd_smooth_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_32x16 aom_highbd_smooth_predictor_32x16_c

void aom_highbd_smooth_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_32x32 aom_highbd_smooth_predictor_32x32_c

void aom_highbd_smooth_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_32x64 aom_highbd_smooth_predictor_32x64_c

void aom_highbd_smooth_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_32x8 aom_highbd_smooth_predictor_32x8_c

void aom_highbd_smooth_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_4x16 aom_highbd_smooth_predictor_4x16_c

void aom_highbd_smooth_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_4x4 aom_highbd_smooth_predictor_4x4_c

void aom_highbd_smooth_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_4x8 aom_highbd_smooth_predictor_4x8_c

void aom_highbd_smooth_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_64x16 aom_highbd_smooth_predictor_64x16_c

void aom_highbd_smooth_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_64x32 aom_highbd_smooth_predictor_64x32_c

void aom_highbd_smooth_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_64x64 aom_highbd_smooth_predictor_64x64_c

void aom_highbd_smooth_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_8x16 aom_highbd_smooth_predictor_8x16_c

void aom_highbd_smooth_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_8x32 aom_highbd_smooth_predictor_8x32_c

void aom_highbd_smooth_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_8x4 aom_highbd_smooth_predictor_8x4_c

void aom_highbd_smooth_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_predictor_8x8 aom_highbd_smooth_predictor_8x8_c

void aom_highbd_smooth_v_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_16x16 aom_highbd_smooth_v_predictor_16x16_c

void aom_highbd_smooth_v_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_16x32 aom_highbd_smooth_v_predictor_16x32_c

void aom_highbd_smooth_v_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_16x4 aom_highbd_smooth_v_predictor_16x4_c

void aom_highbd_smooth_v_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_16x64 aom_highbd_smooth_v_predictor_16x64_c

void aom_highbd_smooth_v_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_16x8 aom_highbd_smooth_v_predictor_16x8_c

void aom_highbd_smooth_v_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_2x2 aom_highbd_smooth_v_predictor_2x2_c

void aom_highbd_smooth_v_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_32x16 aom_highbd_smooth_v_predictor_32x16_c

void aom_highbd_smooth_v_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_32x32 aom_highbd_smooth_v_predictor_32x32_c

void aom_highbd_smooth_v_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_32x64 aom_highbd_smooth_v_predictor_32x64_c

void aom_highbd_smooth_v_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_32x8 aom_highbd_smooth_v_predictor_32x8_c

void aom_highbd_smooth_v_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_4x16 aom_highbd_smooth_v_predictor_4x16_c

void aom_highbd_smooth_v_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_4x4 aom_highbd_smooth_v_predictor_4x4_c

void aom_highbd_smooth_v_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_4x8 aom_highbd_smooth_v_predictor_4x8_c

void aom_highbd_smooth_v_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_64x16 aom_highbd_smooth_v_predictor_64x16_c

void aom_highbd_smooth_v_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_64x32 aom_highbd_smooth_v_predictor_64x32_c

void aom_highbd_smooth_v_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_64x64 aom_highbd_smooth_v_predictor_64x64_c

void aom_highbd_smooth_v_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_8x16 aom_highbd_smooth_v_predictor_8x16_c

void aom_highbd_smooth_v_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_8x32 aom_highbd_smooth_v_predictor_8x32_c

void aom_highbd_smooth_v_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_8x4 aom_highbd_smooth_v_predictor_8x4_c

void aom_highbd_smooth_v_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_smooth_v_predictor_8x8 aom_highbd_smooth_v_predictor_8x8_c

int64_t aom_highbd_sse_c(const uint8_t *a8, int a_stride, const uint8_t *b8,int b_stride, int width, int height);
int64_t aom_highbd_sse_sse4_1(const uint8_t *a8, int a_stride, const uint8_t *b8,int b_stride, int width, int height);
int64_t aom_highbd_sse_avx2(const uint8_t *a8, int a_stride, const uint8_t *b8,int b_stride, int width, int height);
RTCD_EXTERN int64_t (*aom_highbd_sse)(const uint8_t *a8, int a_stride, const uint8_t *b8,int b_stride, int width, int height);

void aom_highbd_subtract_block_c(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride, int bd);
void aom_highbd_subtract_block_sse2(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride, int bd);
#define aom_highbd_subtract_block aom_highbd_subtract_block_sse2

void aom_highbd_upsampled_pred_c(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                 const MV *const mv, uint8_t *comp_pred8, int width, int height, int subpel_x_q3,
                                                 int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd, int subpel_search);
void aom_highbd_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                 const MV *const mv, uint8_t *comp_pred8, int width, int height, int subpel_x_q3,
                                                 int subpel_y_q3, const uint8_t *ref8, int ref_stride, int bd, int subpel_search);
#define aom_highbd_upsampled_pred aom_highbd_upsampled_pred_sse2

void aom_highbd_v_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_16x16 aom_highbd_v_predictor_16x16_sse2

void aom_highbd_v_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_16x32 aom_highbd_v_predictor_16x32_sse2

void aom_highbd_v_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_16x4 aom_highbd_v_predictor_16x4_c

void aom_highbd_v_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_16x64 aom_highbd_v_predictor_16x64_c

void aom_highbd_v_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_16x8 aom_highbd_v_predictor_16x8_sse2

void aom_highbd_v_predictor_2x2_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_2x2 aom_highbd_v_predictor_2x2_c

void aom_highbd_v_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_32x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_32x16 aom_highbd_v_predictor_32x16_sse2

void aom_highbd_v_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_32x32_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_32x32 aom_highbd_v_predictor_32x32_sse2

void aom_highbd_v_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_32x64 aom_highbd_v_predictor_32x64_c

void aom_highbd_v_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_32x8 aom_highbd_v_predictor_32x8_c

void aom_highbd_v_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_4x16 aom_highbd_v_predictor_4x16_c

void aom_highbd_v_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_4x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_4x4 aom_highbd_v_predictor_4x4_sse2

void aom_highbd_v_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_4x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_4x8 aom_highbd_v_predictor_4x8_sse2

void aom_highbd_v_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_64x16 aom_highbd_v_predictor_64x16_c

void aom_highbd_v_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_64x32 aom_highbd_v_predictor_64x32_c

void aom_highbd_v_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_64x64 aom_highbd_v_predictor_64x64_c

void aom_highbd_v_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_8x16_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_8x16 aom_highbd_v_predictor_8x16_sse2

void aom_highbd_v_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_8x32 aom_highbd_v_predictor_8x32_c

void aom_highbd_v_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_8x4_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_8x4 aom_highbd_v_predictor_8x4_sse2

void aom_highbd_v_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_8x8_sse2(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_v_predictor_8x8 aom_highbd_v_predictor_8x8_sse2

void aom_ifft16x16_float_c(const float *input, float *temp, float *output);
void aom_ifft16x16_float_sse2(const float *input, float *temp, float *output);
void aom_ifft16x16_float_avx2(const float *input, float *temp, float *output);
RTCD_EXTERN void (*aom_ifft16x16_float)(const float *input, float *temp, float *output);

void aom_ifft2x2_float_c(const float *input, float *temp, float *output);
#define aom_ifft2x2_float aom_ifft2x2_float_c

void aom_ifft32x32_float_c(const float *input, float *temp, float *output);
void aom_ifft32x32_float_sse2(const float *input, float *temp, float *output);
void aom_ifft32x32_float_avx2(const float *input, float *temp, float *output);
RTCD_EXTERN void (*aom_ifft32x32_float)(const float *input, float *temp, float *output);

void aom_ifft4x4_float_c(const float *input, float *temp, float *output);
void aom_ifft4x4_float_sse2(const float *input, float *temp, float *output);
#define aom_ifft4x4_float aom_ifft4x4_float_sse2

void aom_ifft8x8_float_c(const float *input, float *temp, float *output);
void aom_ifft8x8_float_sse2(const float *input, float *temp, float *output);
void aom_ifft8x8_float_avx2(const float *input, float *temp, float *output);
RTCD_EXTERN void (*aom_ifft8x8_float)(const float *input, float *temp, float *output);

void aom_jnt_comp_avg_pred_c(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const JNT_COMP_PARAMS *jcp_param);
void aom_jnt_comp_avg_pred_ssse3(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN void (*aom_jnt_comp_avg_pred)(uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride, const JNT_COMP_PARAMS *jcp_param);

void aom_jnt_comp_avg_upsampled_pred_c(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                       const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                       int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                       int ref_stride, const JNT_COMP_PARAMS *jcp_param, int subpel_search);
void aom_jnt_comp_avg_upsampled_pred_ssse3(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                       const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                       int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                       int ref_stride, const JNT_COMP_PARAMS *jcp_param, int subpel_search);
RTCD_EXTERN void (*aom_jnt_comp_avg_upsampled_pred)(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                                       const MV *const mv, uint8_t *comp_pred, const uint8_t *pred, int width,
                                                       int height, int subpel_x_q3, int subpel_y_q3, const uint8_t *ref,
                                                       int ref_stride, const JNT_COMP_PARAMS *jcp_param, int subpel_search);

unsigned int aom_jnt_sad128x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad128x128_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad128x128_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad128x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad128x64_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad128x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad16x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad16x16_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad16x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad16x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad16x32_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad16x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad16x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad16x4_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad16x4_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad16x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad16x64_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad16x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad16x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad16x8_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad16x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad32x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad32x16_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad32x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad32x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad32x32_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad32x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad32x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad32x64_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad32x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad32x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad32x8_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad32x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad4x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad4x16_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad4x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad4x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad4x4_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad4x4_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad4x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad4x8_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad4x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad64x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad64x128_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad64x128_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad64x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad64x16_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad64x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad64x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad64x32_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad64x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad64x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad64x64_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad64x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad8x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad8x16_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad8x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad8x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad8x32_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad8x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad8x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad8x4_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad8x4_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

unsigned int aom_jnt_sad8x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
unsigned int aom_jnt_sad8x8_avg_ssse3(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN unsigned int (*aom_jnt_sad8x8_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance128x128_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance128x128)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance128x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance128x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance16x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance16x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance16x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance16x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance16x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance16x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance32x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance32x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance32x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance32x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance32x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance32x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance32x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance32x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance4x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance4x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance4x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance4x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance4x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance64x128_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance64x128)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance64x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance64x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance64x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance64x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance64x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance64x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance8x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance8x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance8x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance8x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance8x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance8x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

uint32_t aom_jnt_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
uint32_t aom_jnt_sub_pixel_avg_variance8x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);
RTCD_EXTERN uint32_t (*aom_jnt_sub_pixel_avg_variance8x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred, const JNT_COMP_PARAMS *jcp_param);

void aom_lowbd_blend_a64_d16_mask_c(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, ConvolveParams *conv_params);
void aom_lowbd_blend_a64_d16_mask_sse4_1(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, ConvolveParams *conv_params);
void aom_lowbd_blend_a64_d16_mask_avx2(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, ConvolveParams *conv_params);
RTCD_EXTERN void (*aom_lowbd_blend_a64_d16_mask)(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subx, int suby, ConvolveParams *conv_params);

void aom_lpf_horizontal_14_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_14_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_horizontal_14 aom_lpf_horizontal_14_sse2

void aom_lpf_horizontal_14_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_14_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_horizontal_14_dual aom_lpf_horizontal_14_dual_sse2

void aom_lpf_horizontal_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_4_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_horizontal_4 aom_lpf_horizontal_4_sse2

void aom_lpf_horizontal_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_4_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_horizontal_4_dual aom_lpf_horizontal_4_dual_sse2

void aom_lpf_horizontal_6_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_6_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_horizontal_6 aom_lpf_horizontal_6_sse2

void aom_lpf_horizontal_6_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_6_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_horizontal_6_dual aom_lpf_horizontal_6_dual_sse2

void aom_lpf_horizontal_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_8_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_horizontal_8 aom_lpf_horizontal_8_sse2

void aom_lpf_horizontal_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_8_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_horizontal_8_dual aom_lpf_horizontal_8_dual_sse2

void aom_lpf_vertical_14_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_14_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_vertical_14 aom_lpf_vertical_14_sse2

void aom_lpf_vertical_14_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_14_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_vertical_14_dual aom_lpf_vertical_14_dual_sse2

void aom_lpf_vertical_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_4_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_vertical_4 aom_lpf_vertical_4_sse2

void aom_lpf_vertical_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_4_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_vertical_4_dual aom_lpf_vertical_4_dual_sse2

void aom_lpf_vertical_6_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_6_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_vertical_6 aom_lpf_vertical_6_sse2

void aom_lpf_vertical_6_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_6_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_vertical_6_dual aom_lpf_vertical_6_dual_sse2

void aom_lpf_vertical_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_8_sse2(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
#define aom_lpf_vertical_8 aom_lpf_vertical_8_sse2

void aom_lpf_vertical_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_8_dual_sse2(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
#define aom_lpf_vertical_8_dual aom_lpf_vertical_8_dual_sse2

unsigned int aom_masked_sad128x128_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad128x128_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad128x128_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad128x128)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad128x64_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad128x64_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad128x64_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad128x64)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad16x16_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x16_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x16_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad16x16)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad16x32_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x32_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x32_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad16x32)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad16x4_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x4_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x4_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad16x4)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad16x64_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x64_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x64_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad16x64)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad16x8_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x8_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad16x8_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad16x8)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad32x16_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x16_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x16_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad32x16)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad32x32_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x32_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x32_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad32x32)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad32x64_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x64_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x64_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad32x64)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad32x8_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x8_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad32x8_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad32x8)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad4x16_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad4x16_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad4x16_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad4x16)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad4x4_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad4x4_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad4x4_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad4x4)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad4x8_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad4x8_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad4x8_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad4x8)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad64x128_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x128_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x128_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad64x128)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad64x16_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x16_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x16_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad64x16)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad64x32_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x32_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x32_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad64x32)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad64x64_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x64_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad64x64_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad64x64)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad8x16_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x16_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x16_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad8x16)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad8x32_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x32_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x32_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad8x32)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad8x4_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x4_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x4_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad8x4)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sad8x8_c(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x8_ssse3(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
unsigned int aom_masked_sad8x8_avx2(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);
RTCD_EXTERN unsigned int (*aom_masked_sad8x8)(const uint8_t *src, int src_stride, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask);

unsigned int aom_masked_sub_pixel_variance128x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance128x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance128x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance128x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance128x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance128x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance16x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance16x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance16x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance16x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance16x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance16x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance16x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance16x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance16x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance16x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance16x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance16x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance16x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance16x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance16x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance32x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance32x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance32x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance32x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance32x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance32x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance32x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance32x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance32x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance32x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance32x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance32x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance4x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance4x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance4x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance4x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance4x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance4x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance4x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance4x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance4x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance64x128_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance64x128_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance64x128)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance64x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance64x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance64x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance64x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance64x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance64x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance64x64_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance64x64_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance64x64)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance8x16_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance8x16_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance8x16)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance8x32_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance8x32_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance8x32)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance8x4_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance8x4_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance8x4)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_masked_sub_pixel_variance8x8_c(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
unsigned int aom_masked_sub_pixel_variance8x8_ssse3(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_masked_sub_pixel_variance8x8)(const uint8_t *src, int src_stride, int xoffset, int yoffset, const uint8_t *ref, int ref_stride, const uint8_t *second_pred, const uint8_t *msk, int msk_stride, int invert_mask, unsigned int *sse);

unsigned int aom_mse16x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_mse16x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_mse16x16_avx2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_mse16x16)(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);

unsigned int aom_mse16x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_mse16x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_mse16x8 aom_mse16x8_sse2

unsigned int aom_mse8x16_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_mse8x16_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_mse8x16 aom_mse8x16_sse2

unsigned int aom_mse8x8_c(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
unsigned int aom_mse8x8_sse2(const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse);
#define aom_mse8x8 aom_mse8x8_sse2

unsigned int aom_obmc_sad128x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad128x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad128x128_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad128x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad128x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad128x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad128x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad128x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad16x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad16x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad16x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad16x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad16x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad16x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad16x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad16x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad16x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad16x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad16x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad32x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad32x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad32x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad32x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad32x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad32x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad32x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad32x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad32x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad4x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad4x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad4x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad4x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad4x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad4x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad4x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad4x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad4x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad4x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad4x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad4x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad64x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x128_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad64x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad64x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad64x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad64x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad64x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad64x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad64x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad64x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad8x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad8x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad8x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad8x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad8x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad8x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sad8x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
unsigned int aom_obmc_sad8x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);
RTCD_EXTERN unsigned int (*aom_obmc_sad8x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask);

unsigned int aom_obmc_sub_pixel_variance128x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance128x128_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance128x128)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance128x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance128x64_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance128x64)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance16x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance16x16_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance16x16)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance16x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance16x32_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance16x32)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance16x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance16x4_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance16x4)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance16x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance16x64_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance16x64)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance16x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance16x8_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance16x8)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance32x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance32x16_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance32x16)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance32x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance32x32_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance32x32)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance32x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance32x64_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance32x64)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance32x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance32x8_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance32x8)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance4x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance4x16_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance4x16)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance4x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance4x4_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance4x4)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance4x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance4x8_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance4x8)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance64x128_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance64x128_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance64x128)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance64x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance64x16_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance64x16)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance64x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance64x32_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance64x32)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance64x64_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance64x64_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance64x64)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance8x16_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance8x16_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance8x16)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance8x32_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance8x32_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance8x32)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance8x4_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance8x4_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance8x4)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_sub_pixel_variance8x8_c(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_sub_pixel_variance8x8_sse4_1(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_sub_pixel_variance8x8)(const uint8_t *pre, int pre_stride, int xoffset, int yoffset, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance128x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance128x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance128x128_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance128x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance128x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance128x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance128x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance128x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance16x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance16x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance16x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance16x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance16x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance16x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance16x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance16x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance16x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance16x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance16x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance32x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance32x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance32x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance32x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance32x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance32x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance32x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance32x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance32x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance4x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance4x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance4x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance4x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance4x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance4x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance4x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance4x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance4x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance4x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance4x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance4x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance64x128_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x128_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x128_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance64x128)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance64x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance64x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance64x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance64x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance64x64_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x64_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance64x64_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance64x64)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance8x16_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x16_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x16_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance8x16)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance8x32_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x32_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x32_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance8x32)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance8x4_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x4_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x4_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance8x4)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

unsigned int aom_obmc_variance8x8_c(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x8_sse4_1(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
unsigned int aom_obmc_variance8x8_avx2(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_obmc_variance8x8)(const uint8_t *pre, int pre_stride, const int32_t *wsrc, const int32_t *mask, unsigned int *sse);

void aom_paeth_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x8_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_paeth_predictor_2x2 aom_paeth_predictor_2x2_c

void aom_paeth_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_quantize_b_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_quantize_b_sse2(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_quantize_b_ssse3(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_quantize_b_avx(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
RTCD_EXTERN void (*aom_quantize_b)(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);

void aom_quantize_b_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_quantize_b_32x32_ssse3(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void aom_quantize_b_32x32_avx(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
RTCD_EXTERN void (*aom_quantize_b_32x32)(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);

void aom_quantize_b_64x64_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define aom_quantize_b_64x64 aom_quantize_b_64x64_c

unsigned int aom_sad128x128_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad128x128_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad128x128_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad128x128)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad128x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad128x128_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad128x128_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad128x128_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad128x128x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad128x128x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad128x128x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_sad128x128x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_sad128x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad128x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad128x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad128x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad128x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad128x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad128x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad128x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad128x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad128x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad128x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_sad128x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_sad128xh_c(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
unsigned int aom_sad128xh_sse2(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
#define aom_sad128xh aom_sad128xh_sse2

unsigned int aom_sad16x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad16x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad16x16 aom_sad16x16_sse2

unsigned int aom_sad16x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad16x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad16x16_avg aom_sad16x16_avg_sse2

void aom_sad16x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad16x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad16x16x4d aom_sad16x16x4d_sse2

unsigned int aom_sad16x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad16x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad16x32 aom_sad16x32_sse2

unsigned int aom_sad16x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad16x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad16x32_avg aom_sad16x32_avg_sse2

void aom_sad16x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad16x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad16x32x4d aom_sad16x32x4d_sse2

unsigned int aom_sad16x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad16x4_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad16x4 aom_sad16x4_sse2

unsigned int aom_sad16x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad16x4_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad16x4_avg aom_sad16x4_avg_sse2

void aom_sad16x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad16x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad16x4x4d aom_sad16x4x4d_sse2

unsigned int aom_sad16x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad16x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad16x64 aom_sad16x64_sse2

unsigned int aom_sad16x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad16x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad16x64_avg aom_sad16x64_avg_sse2

void aom_sad16x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad16x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad16x64x4d aom_sad16x64x4d_sse2

unsigned int aom_sad16x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad16x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad16x8 aom_sad16x8_sse2

unsigned int aom_sad16x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad16x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad16x8_avg aom_sad16x8_avg_sse2

void aom_sad16x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad16x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad16x8x4d aom_sad16x8x4d_sse2

unsigned int aom_sad16xh_c(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
unsigned int aom_sad16xh_sse2(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
#define aom_sad16xh aom_sad16xh_sse2

unsigned int aom_sad32x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad32x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad32x16_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad32x16)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad32x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad32x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad32x16_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad32x16_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad32x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad32x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad32x16x4d aom_sad32x16x4d_sse2

unsigned int aom_sad32x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad32x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad32x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad32x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad32x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad32x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad32x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad32x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad32x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad32x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad32x32x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_sad32x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_sad32x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad32x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad32x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad32x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad32x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad32x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad32x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad32x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad32x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad32x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad32x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_sad32x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_sad32x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad32x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad32x8 aom_sad32x8_sse2

unsigned int aom_sad32x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad32x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad32x8_avg aom_sad32x8_avg_sse2

void aom_sad32x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad32x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad32x8x4d aom_sad32x8x4d_sse2

unsigned int aom_sad32xh_c(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
unsigned int aom_sad32xh_sse2(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
#define aom_sad32xh aom_sad32xh_sse2

unsigned int aom_sad4x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad4x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad4x16 aom_sad4x16_sse2

unsigned int aom_sad4x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad4x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad4x16_avg aom_sad4x16_avg_sse2

void aom_sad4x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad4x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad4x16x4d aom_sad4x16x4d_sse2

unsigned int aom_sad4x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad4x4_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad4x4 aom_sad4x4_sse2

unsigned int aom_sad4x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad4x4_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad4x4_avg aom_sad4x4_avg_sse2

void aom_sad4x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad4x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad4x4x4d aom_sad4x4x4d_sse2

unsigned int aom_sad4x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad4x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad4x8 aom_sad4x8_sse2

unsigned int aom_sad4x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad4x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad4x8_avg aom_sad4x8_avg_sse2

void aom_sad4x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad4x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad4x8x4d aom_sad4x8x4d_sse2

unsigned int aom_sad4xh_c(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
unsigned int aom_sad4xh_sse2(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
#define aom_sad4xh aom_sad4xh_sse2

unsigned int aom_sad64x128_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad64x128_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad64x128_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad64x128)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad64x128_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad64x128_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad64x128_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad64x128_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad64x128x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad64x128x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad64x128x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_sad64x128x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_sad64x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad64x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad64x16 aom_sad64x16_sse2

unsigned int aom_sad64x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad64x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad64x16_avg aom_sad64x16_avg_sse2

void aom_sad64x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad64x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad64x16x4d aom_sad64x16x4d_sse2

unsigned int aom_sad64x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad64x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad64x32_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad64x32)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad64x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad64x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad64x32_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad64x32_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad64x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad64x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad64x32x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_sad64x32x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_sad64x64_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad64x64_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad64x64_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
RTCD_EXTERN unsigned int (*aom_sad64x64)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);

unsigned int aom_sad64x64_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad64x64_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad64x64_avg_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
RTCD_EXTERN unsigned int (*aom_sad64x64_avg)(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);

void aom_sad64x64x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad64x64x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad64x64x4d_avx2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
RTCD_EXTERN void (*aom_sad64x64x4d)(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);

unsigned int aom_sad64xh_c(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
unsigned int aom_sad64xh_sse2(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
#define aom_sad64xh aom_sad64xh_sse2

unsigned int aom_sad8x16_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad8x16_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad8x16 aom_sad8x16_sse2

unsigned int aom_sad8x16_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad8x16_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad8x16_avg aom_sad8x16_avg_sse2

void aom_sad8x16x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad8x16x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad8x16x4d aom_sad8x16x4d_sse2

unsigned int aom_sad8x32_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad8x32_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad8x32 aom_sad8x32_sse2

unsigned int aom_sad8x32_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad8x32_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad8x32_avg aom_sad8x32_avg_sse2

void aom_sad8x32x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad8x32x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad8x32x4d aom_sad8x32x4d_sse2

unsigned int aom_sad8x4_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad8x4_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad8x4 aom_sad8x4_sse2

unsigned int aom_sad8x4_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad8x4_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad8x4_avg aom_sad8x4_avg_sse2

void aom_sad8x4x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad8x4x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad8x4x4d aom_sad8x4x4d_sse2

unsigned int aom_sad8x8_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
unsigned int aom_sad8x8_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride);
#define aom_sad8x8 aom_sad8x8_sse2

unsigned int aom_sad8x8_avg_c(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
unsigned int aom_sad8x8_avg_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred);
#define aom_sad8x8_avg aom_sad8x8_avg_sse2

void aom_sad8x8x4d_c(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
void aom_sad8x8x4d_sse2(const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array);
#define aom_sad8x8x4d aom_sad8x8x4d_sse2

unsigned int aom_sad8xh_c(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
unsigned int aom_sad8xh_sse2(const uint8_t *a, int a_stride, const uint8_t *b, int b_stride, int width, int height);
#define aom_sad8xh aom_sad8xh_sse2

void aom_smooth_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_smooth_h_predictor_2x2 aom_smooth_h_predictor_2x2_c

void aom_smooth_h_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_smooth_predictor_2x2 aom_smooth_predictor_2x2_c

void aom_smooth_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_smooth_v_predictor_2x2 aom_smooth_v_predictor_2x2_c

void aom_smooth_v_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

int64_t aom_sse_c(const uint8_t *a, int a_stride, const uint8_t *b,int b_stride, int width, int height);
int64_t aom_sse_sse4_1(const uint8_t *a, int a_stride, const uint8_t *b,int b_stride, int width, int height);
int64_t aom_sse_avx2(const uint8_t *a, int a_stride, const uint8_t *b,int b_stride, int width, int height);
RTCD_EXTERN int64_t (*aom_sse)(const uint8_t *a, int a_stride, const uint8_t *b,int b_stride, int width, int height);

uint32_t aom_sub_pixel_avg_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance128x128_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance128x128_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance128x128_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance128x128)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance128x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance128x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance128x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance128x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance16x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance16x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance16x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x16_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance32x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance32x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance32x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance32x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance32x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance4x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance4x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance4x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance4x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance4x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance4x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance4x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance4x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x128_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x128_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x128_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance64x128)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance64x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x32_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance64x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance64x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance8x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance8x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance8x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_avg_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
uint32_t aom_sub_pixel_avg_variance8x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);
RTCD_EXTERN uint32_t (*aom_sub_pixel_avg_variance8x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred);

uint32_t aom_sub_pixel_variance128x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance128x128_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance128x128_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance128x128_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance128x128)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance128x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance128x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance128x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance128x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance128x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance16x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance16x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance16x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance16x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance16x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance16x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance16x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance16x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance16x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance16x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance16x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance32x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x16_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance32x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance32x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance32x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance32x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance32x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance32x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance32x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance32x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance4x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance4x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance4x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance4x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance4x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance4x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance4x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance4x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance4x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance4x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance4x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance4x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance64x128_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x128_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x128_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x128_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance64x128)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance64x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance64x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance64x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x32_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance64x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance64x64_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x64_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance64x64)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance8x16_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x16_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance8x16)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance8x32_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x32_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance8x32)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance8x4_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x4_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance8x4)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

uint32_t aom_sub_pixel_variance8x8_c(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
uint32_t aom_sub_pixel_variance8x8_ssse3(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);
RTCD_EXTERN uint32_t (*aom_sub_pixel_variance8x8)(const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse);

void aom_subtract_block_c(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);
void aom_subtract_block_sse2(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);
void aom_subtract_block_avx2(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);
RTCD_EXTERN void (*aom_subtract_block)(int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride);

uint64_t aom_sum_squares_2d_i16_c(const int16_t *src, int stride, int width, int height);
uint64_t aom_sum_squares_2d_i16_sse2(const int16_t *src, int stride, int width, int height);
uint64_t aom_sum_squares_2d_i16_avx2(const int16_t *src, int stride, int width, int height);
RTCD_EXTERN uint64_t (*aom_sum_squares_2d_i16)(const int16_t *src, int stride, int width, int height);

uint64_t aom_sum_squares_i16_c(const int16_t *src, uint32_t N);
uint64_t aom_sum_squares_i16_sse2(const int16_t *src, uint32_t N);
#define aom_sum_squares_i16 aom_sum_squares_i16_sse2

void aom_upsampled_pred_c(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                          const MV *const mv, uint8_t *comp_pred, int width, int height, int subpel_x_q3,
                                          int subpel_y_q3, const uint8_t *ref, int ref_stride, int subpel_search);
void aom_upsampled_pred_sse2(MACROBLOCKD *xd, const struct AV1Common *const cm, int mi_row, int mi_col,
                                          const MV *const mv, uint8_t *comp_pred, int width, int height, int subpel_x_q3,
                                          int subpel_y_q3, const uint8_t *ref, int ref_stride, int subpel_search);
#define aom_upsampled_pred aom_upsampled_pred_sse2

void aom_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_16x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x16 aom_v_predictor_16x16_sse2

void aom_v_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_16x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x32 aom_v_predictor_16x32_sse2

void aom_v_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_16x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x4 aom_v_predictor_16x4_sse2

void aom_v_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_16x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x64 aom_v_predictor_16x64_sse2

void aom_v_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_16x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x8 aom_v_predictor_16x8_sse2

void aom_v_predictor_2x2_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_2x2 aom_v_predictor_2x2_c

void aom_v_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_32x8 aom_v_predictor_32x8_sse2

void aom_v_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_4x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_4x16 aom_v_predictor_4x16_sse2

void aom_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_4x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_4x4 aom_v_predictor_4x4_sse2

void aom_v_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_4x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_4x8 aom_v_predictor_4x8_sse2

void aom_v_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_64x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_64x16_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_64x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_64x32_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_64x64_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_64x64_avx2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_8x16_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_8x16 aom_v_predictor_8x16_sse2

void aom_v_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_8x32_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_8x32 aom_v_predictor_8x32_sse2

void aom_v_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_8x4_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_8x4 aom_v_predictor_8x4_sse2

void aom_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_8x8_sse2(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_8x8 aom_v_predictor_8x8_sse2

unsigned int aom_variance128x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance128x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance128x128_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance128x128)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance128x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance128x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance128x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance128x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance16x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance16x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance16x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance16x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance16x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x4_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance16x4)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance16x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance16x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance16x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance16x8_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance16x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance2x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance2x2 aom_variance2x2_c

unsigned int aom_variance2x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance2x4 aom_variance2x4_c

unsigned int aom_variance32x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance32x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance32x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance32x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance32x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance32x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance32x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance32x8_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance32x8)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance4x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance4x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance4x16 aom_variance4x16_sse2

unsigned int aom_variance4x2_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance4x2 aom_variance4x2_c

unsigned int aom_variance4x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance4x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance4x4 aom_variance4x4_sse2

unsigned int aom_variance4x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance4x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance4x8 aom_variance4x8_sse2

unsigned int aom_variance64x128_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x128_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x128_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance64x128)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance64x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x16_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance64x16)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance64x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x32_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance64x32)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance64x64_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x64_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance64x64_avx2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
RTCD_EXTERN unsigned int (*aom_variance64x64)(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);

unsigned int aom_variance8x16_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance8x16_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance8x16 aom_variance8x16_sse2

unsigned int aom_variance8x32_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance8x32_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance8x32 aom_variance8x32_sse2

unsigned int aom_variance8x4_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance8x4_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance8x4 aom_variance8x4_sse2

unsigned int aom_variance8x8_c(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
unsigned int aom_variance8x8_sse2(const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse);
#define aom_variance8x8 aom_variance8x8_sse2

void av1_round_shift_array_c(int32_t *arr, int size, int bit);
void av1_round_shift_array_sse4_1(int32_t *arr, int size, int bit);
RTCD_EXTERN void (*av1_round_shift_array)(int32_t *arr, int size, int bit);

void aom_dsp_rtcd(void);

#ifdef RTCD_C
#include "aom_ports/x86.h"
static void setup_rtcd_internal(void)
{
    int flags = x86_simd_caps();

    (void)flags;

    aom_blend_a64_hmask = aom_blend_a64_hmask_c;
    if (flags & HAS_SSE4_1) aom_blend_a64_hmask = aom_blend_a64_hmask_sse4_1;
    aom_blend_a64_mask = aom_blend_a64_mask_c;
    if (flags & HAS_SSE4_1) aom_blend_a64_mask = aom_blend_a64_mask_sse4_1;
    if (flags & HAS_AVX2) aom_blend_a64_mask = aom_blend_a64_mask_avx2;
    aom_blend_a64_vmask = aom_blend_a64_vmask_c;
    if (flags & HAS_SSE4_1) aom_blend_a64_vmask = aom_blend_a64_vmask_sse4_1;
    aom_comp_mask_pred = aom_comp_mask_pred_c;
    if (flags & HAS_SSSE3) aom_comp_mask_pred = aom_comp_mask_pred_ssse3;
    if (flags & HAS_AVX2) aom_comp_mask_pred = aom_comp_mask_pred_avx2;
    aom_convolve8_horiz = aom_convolve8_horiz_sse2;
    if (flags & HAS_SSSE3) aom_convolve8_horiz = aom_convolve8_horiz_ssse3;
    if (flags & HAS_AVX2) aom_convolve8_horiz = aom_convolve8_horiz_avx2;
    aom_convolve8_vert = aom_convolve8_vert_sse2;
    if (flags & HAS_SSSE3) aom_convolve8_vert = aom_convolve8_vert_ssse3;
    if (flags & HAS_AVX2) aom_convolve8_vert = aom_convolve8_vert_avx2;
    aom_dc_128_predictor_32x16 = aom_dc_128_predictor_32x16_sse2;
    if (flags & HAS_AVX2) aom_dc_128_predictor_32x16 = aom_dc_128_predictor_32x16_avx2;
    aom_dc_128_predictor_32x32 = aom_dc_128_predictor_32x32_sse2;
    if (flags & HAS_AVX2) aom_dc_128_predictor_32x32 = aom_dc_128_predictor_32x32_avx2;
    aom_dc_128_predictor_32x64 = aom_dc_128_predictor_32x64_sse2;
    if (flags & HAS_AVX2) aom_dc_128_predictor_32x64 = aom_dc_128_predictor_32x64_avx2;
    aom_dc_128_predictor_64x16 = aom_dc_128_predictor_64x16_sse2;
    if (flags & HAS_AVX2) aom_dc_128_predictor_64x16 = aom_dc_128_predictor_64x16_avx2;
    aom_dc_128_predictor_64x32 = aom_dc_128_predictor_64x32_sse2;
    if (flags & HAS_AVX2) aom_dc_128_predictor_64x32 = aom_dc_128_predictor_64x32_avx2;
    aom_dc_128_predictor_64x64 = aom_dc_128_predictor_64x64_sse2;
    if (flags & HAS_AVX2) aom_dc_128_predictor_64x64 = aom_dc_128_predictor_64x64_avx2;
    aom_dc_left_predictor_32x16 = aom_dc_left_predictor_32x16_sse2;
    if (flags & HAS_AVX2) aom_dc_left_predictor_32x16 = aom_dc_left_predictor_32x16_avx2;
    aom_dc_left_predictor_32x32 = aom_dc_left_predictor_32x32_sse2;
    if (flags & HAS_AVX2) aom_dc_left_predictor_32x32 = aom_dc_left_predictor_32x32_avx2;
    aom_dc_left_predictor_32x64 = aom_dc_left_predictor_32x64_sse2;
    if (flags & HAS_AVX2) aom_dc_left_predictor_32x64 = aom_dc_left_predictor_32x64_avx2;
    aom_dc_left_predictor_64x16 = aom_dc_left_predictor_64x16_sse2;
    if (flags & HAS_AVX2) aom_dc_left_predictor_64x16 = aom_dc_left_predictor_64x16_avx2;
    aom_dc_left_predictor_64x32 = aom_dc_left_predictor_64x32_sse2;
    if (flags & HAS_AVX2) aom_dc_left_predictor_64x32 = aom_dc_left_predictor_64x32_avx2;
    aom_dc_left_predictor_64x64 = aom_dc_left_predictor_64x64_sse2;
    if (flags & HAS_AVX2) aom_dc_left_predictor_64x64 = aom_dc_left_predictor_64x64_avx2;
    aom_dc_predictor_32x16 = aom_dc_predictor_32x16_sse2;
    if (flags & HAS_AVX2) aom_dc_predictor_32x16 = aom_dc_predictor_32x16_avx2;
    aom_dc_predictor_32x32 = aom_dc_predictor_32x32_sse2;
    if (flags & HAS_AVX2) aom_dc_predictor_32x32 = aom_dc_predictor_32x32_avx2;
    aom_dc_predictor_32x64 = aom_dc_predictor_32x64_sse2;
    if (flags & HAS_AVX2) aom_dc_predictor_32x64 = aom_dc_predictor_32x64_avx2;
    aom_dc_predictor_64x16 = aom_dc_predictor_64x16_sse2;
    if (flags & HAS_AVX2) aom_dc_predictor_64x16 = aom_dc_predictor_64x16_avx2;
    aom_dc_predictor_64x32 = aom_dc_predictor_64x32_sse2;
    if (flags & HAS_AVX2) aom_dc_predictor_64x32 = aom_dc_predictor_64x32_avx2;
    aom_dc_predictor_64x64 = aom_dc_predictor_64x64_sse2;
    if (flags & HAS_AVX2) aom_dc_predictor_64x64 = aom_dc_predictor_64x64_avx2;
    aom_dc_top_predictor_32x16 = aom_dc_top_predictor_32x16_sse2;
    if (flags & HAS_AVX2) aom_dc_top_predictor_32x16 = aom_dc_top_predictor_32x16_avx2;
    aom_dc_top_predictor_32x32 = aom_dc_top_predictor_32x32_sse2;
    if (flags & HAS_AVX2) aom_dc_top_predictor_32x32 = aom_dc_top_predictor_32x32_avx2;
    aom_dc_top_predictor_32x64 = aom_dc_top_predictor_32x64_sse2;
    if (flags & HAS_AVX2) aom_dc_top_predictor_32x64 = aom_dc_top_predictor_32x64_avx2;
    aom_dc_top_predictor_64x16 = aom_dc_top_predictor_64x16_sse2;
    if (flags & HAS_AVX2) aom_dc_top_predictor_64x16 = aom_dc_top_predictor_64x16_avx2;
    aom_dc_top_predictor_64x32 = aom_dc_top_predictor_64x32_sse2;
    if (flags & HAS_AVX2) aom_dc_top_predictor_64x32 = aom_dc_top_predictor_64x32_avx2;
    aom_dc_top_predictor_64x64 = aom_dc_top_predictor_64x64_sse2;
    if (flags & HAS_AVX2) aom_dc_top_predictor_64x64 = aom_dc_top_predictor_64x64_avx2;
    aom_fdct8x8 = aom_fdct8x8_sse2;
    if (flags & HAS_SSSE3) aom_fdct8x8 = aom_fdct8x8_ssse3;
    aom_fft16x16_float = aom_fft16x16_float_sse2;
    if (flags & HAS_AVX2) aom_fft16x16_float = aom_fft16x16_float_avx2;
    aom_fft32x32_float = aom_fft32x32_float_sse2;
    if (flags & HAS_AVX2) aom_fft32x32_float = aom_fft32x32_float_avx2;
    aom_fft8x8_float = aom_fft8x8_float_sse2;
    if (flags & HAS_AVX2) aom_fft8x8_float = aom_fft8x8_float_avx2;
    aom_h_predictor_32x32 = aom_h_predictor_32x32_sse2;
    if (flags & HAS_AVX2) aom_h_predictor_32x32 = aom_h_predictor_32x32_avx2;
    aom_highbd_10_masked_sub_pixel_variance128x128 = aom_highbd_10_masked_sub_pixel_variance128x128_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance128x128 = aom_highbd_10_masked_sub_pixel_variance128x128_ssse3;
    aom_highbd_10_masked_sub_pixel_variance128x64 = aom_highbd_10_masked_sub_pixel_variance128x64_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance128x64 = aom_highbd_10_masked_sub_pixel_variance128x64_ssse3;
    aom_highbd_10_masked_sub_pixel_variance16x16 = aom_highbd_10_masked_sub_pixel_variance16x16_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance16x16 = aom_highbd_10_masked_sub_pixel_variance16x16_ssse3;
    aom_highbd_10_masked_sub_pixel_variance16x32 = aom_highbd_10_masked_sub_pixel_variance16x32_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance16x32 = aom_highbd_10_masked_sub_pixel_variance16x32_ssse3;
    aom_highbd_10_masked_sub_pixel_variance16x4 = aom_highbd_10_masked_sub_pixel_variance16x4_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance16x4 = aom_highbd_10_masked_sub_pixel_variance16x4_ssse3;
    aom_highbd_10_masked_sub_pixel_variance16x64 = aom_highbd_10_masked_sub_pixel_variance16x64_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance16x64 = aom_highbd_10_masked_sub_pixel_variance16x64_ssse3;
    aom_highbd_10_masked_sub_pixel_variance16x8 = aom_highbd_10_masked_sub_pixel_variance16x8_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance16x8 = aom_highbd_10_masked_sub_pixel_variance16x8_ssse3;
    aom_highbd_10_masked_sub_pixel_variance32x16 = aom_highbd_10_masked_sub_pixel_variance32x16_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance32x16 = aom_highbd_10_masked_sub_pixel_variance32x16_ssse3;
    aom_highbd_10_masked_sub_pixel_variance32x32 = aom_highbd_10_masked_sub_pixel_variance32x32_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance32x32 = aom_highbd_10_masked_sub_pixel_variance32x32_ssse3;
    aom_highbd_10_masked_sub_pixel_variance32x64 = aom_highbd_10_masked_sub_pixel_variance32x64_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance32x64 = aom_highbd_10_masked_sub_pixel_variance32x64_ssse3;
    aom_highbd_10_masked_sub_pixel_variance32x8 = aom_highbd_10_masked_sub_pixel_variance32x8_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance32x8 = aom_highbd_10_masked_sub_pixel_variance32x8_ssse3;
    aom_highbd_10_masked_sub_pixel_variance4x16 = aom_highbd_10_masked_sub_pixel_variance4x16_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance4x16 = aom_highbd_10_masked_sub_pixel_variance4x16_ssse3;
    aom_highbd_10_masked_sub_pixel_variance4x4 = aom_highbd_10_masked_sub_pixel_variance4x4_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance4x4 = aom_highbd_10_masked_sub_pixel_variance4x4_ssse3;
    aom_highbd_10_masked_sub_pixel_variance4x8 = aom_highbd_10_masked_sub_pixel_variance4x8_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance4x8 = aom_highbd_10_masked_sub_pixel_variance4x8_ssse3;
    aom_highbd_10_masked_sub_pixel_variance64x128 = aom_highbd_10_masked_sub_pixel_variance64x128_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance64x128 = aom_highbd_10_masked_sub_pixel_variance64x128_ssse3;
    aom_highbd_10_masked_sub_pixel_variance64x16 = aom_highbd_10_masked_sub_pixel_variance64x16_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance64x16 = aom_highbd_10_masked_sub_pixel_variance64x16_ssse3;
    aom_highbd_10_masked_sub_pixel_variance64x32 = aom_highbd_10_masked_sub_pixel_variance64x32_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance64x32 = aom_highbd_10_masked_sub_pixel_variance64x32_ssse3;
    aom_highbd_10_masked_sub_pixel_variance64x64 = aom_highbd_10_masked_sub_pixel_variance64x64_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance64x64 = aom_highbd_10_masked_sub_pixel_variance64x64_ssse3;
    aom_highbd_10_masked_sub_pixel_variance8x16 = aom_highbd_10_masked_sub_pixel_variance8x16_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance8x16 = aom_highbd_10_masked_sub_pixel_variance8x16_ssse3;
    aom_highbd_10_masked_sub_pixel_variance8x32 = aom_highbd_10_masked_sub_pixel_variance8x32_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance8x32 = aom_highbd_10_masked_sub_pixel_variance8x32_ssse3;
    aom_highbd_10_masked_sub_pixel_variance8x4 = aom_highbd_10_masked_sub_pixel_variance8x4_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance8x4 = aom_highbd_10_masked_sub_pixel_variance8x4_ssse3;
    aom_highbd_10_masked_sub_pixel_variance8x8 = aom_highbd_10_masked_sub_pixel_variance8x8_c;
    if (flags & HAS_SSSE3) aom_highbd_10_masked_sub_pixel_variance8x8 = aom_highbd_10_masked_sub_pixel_variance8x8_ssse3;
    aom_highbd_10_obmc_variance128x128 = aom_highbd_10_obmc_variance128x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance128x128 = aom_highbd_10_obmc_variance128x128_sse4_1;
    aom_highbd_10_obmc_variance128x64 = aom_highbd_10_obmc_variance128x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance128x64 = aom_highbd_10_obmc_variance128x64_sse4_1;
    aom_highbd_10_obmc_variance16x16 = aom_highbd_10_obmc_variance16x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance16x16 = aom_highbd_10_obmc_variance16x16_sse4_1;
    aom_highbd_10_obmc_variance16x32 = aom_highbd_10_obmc_variance16x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance16x32 = aom_highbd_10_obmc_variance16x32_sse4_1;
    aom_highbd_10_obmc_variance16x4 = aom_highbd_10_obmc_variance16x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance16x4 = aom_highbd_10_obmc_variance16x4_sse4_1;
    aom_highbd_10_obmc_variance16x64 = aom_highbd_10_obmc_variance16x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance16x64 = aom_highbd_10_obmc_variance16x64_sse4_1;
    aom_highbd_10_obmc_variance16x8 = aom_highbd_10_obmc_variance16x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance16x8 = aom_highbd_10_obmc_variance16x8_sse4_1;
    aom_highbd_10_obmc_variance32x16 = aom_highbd_10_obmc_variance32x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance32x16 = aom_highbd_10_obmc_variance32x16_sse4_1;
    aom_highbd_10_obmc_variance32x32 = aom_highbd_10_obmc_variance32x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance32x32 = aom_highbd_10_obmc_variance32x32_sse4_1;
    aom_highbd_10_obmc_variance32x64 = aom_highbd_10_obmc_variance32x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance32x64 = aom_highbd_10_obmc_variance32x64_sse4_1;
    aom_highbd_10_obmc_variance32x8 = aom_highbd_10_obmc_variance32x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance32x8 = aom_highbd_10_obmc_variance32x8_sse4_1;
    aom_highbd_10_obmc_variance4x16 = aom_highbd_10_obmc_variance4x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance4x16 = aom_highbd_10_obmc_variance4x16_sse4_1;
    aom_highbd_10_obmc_variance4x4 = aom_highbd_10_obmc_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance4x4 = aom_highbd_10_obmc_variance4x4_sse4_1;
    aom_highbd_10_obmc_variance4x8 = aom_highbd_10_obmc_variance4x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance4x8 = aom_highbd_10_obmc_variance4x8_sse4_1;
    aom_highbd_10_obmc_variance64x128 = aom_highbd_10_obmc_variance64x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance64x128 = aom_highbd_10_obmc_variance64x128_sse4_1;
    aom_highbd_10_obmc_variance64x16 = aom_highbd_10_obmc_variance64x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance64x16 = aom_highbd_10_obmc_variance64x16_sse4_1;
    aom_highbd_10_obmc_variance64x32 = aom_highbd_10_obmc_variance64x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance64x32 = aom_highbd_10_obmc_variance64x32_sse4_1;
    aom_highbd_10_obmc_variance64x64 = aom_highbd_10_obmc_variance64x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance64x64 = aom_highbd_10_obmc_variance64x64_sse4_1;
    aom_highbd_10_obmc_variance8x16 = aom_highbd_10_obmc_variance8x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance8x16 = aom_highbd_10_obmc_variance8x16_sse4_1;
    aom_highbd_10_obmc_variance8x32 = aom_highbd_10_obmc_variance8x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance8x32 = aom_highbd_10_obmc_variance8x32_sse4_1;
    aom_highbd_10_obmc_variance8x4 = aom_highbd_10_obmc_variance8x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance8x4 = aom_highbd_10_obmc_variance8x4_sse4_1;
    aom_highbd_10_obmc_variance8x8 = aom_highbd_10_obmc_variance8x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_obmc_variance8x8 = aom_highbd_10_obmc_variance8x8_sse4_1;
    aom_highbd_10_sub_pixel_avg_variance4x4 = aom_highbd_10_sub_pixel_avg_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_sub_pixel_avg_variance4x4 = aom_highbd_10_sub_pixel_avg_variance4x4_sse4_1;
    aom_highbd_10_sub_pixel_variance4x4 = aom_highbd_10_sub_pixel_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_sub_pixel_variance4x4 = aom_highbd_10_sub_pixel_variance4x4_sse4_1;
    aom_highbd_10_variance128x128 = aom_highbd_10_variance128x128_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance128x128 = aom_highbd_10_variance128x128_avx2;
    aom_highbd_10_variance128x64 = aom_highbd_10_variance128x64_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance128x64 = aom_highbd_10_variance128x64_avx2;
    aom_highbd_10_variance16x16 = aom_highbd_10_variance16x16_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance16x16 = aom_highbd_10_variance16x16_avx2;
    aom_highbd_10_variance16x32 = aom_highbd_10_variance16x32_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance16x32 = aom_highbd_10_variance16x32_avx2;
    aom_highbd_10_variance16x8 = aom_highbd_10_variance16x8_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance16x8 = aom_highbd_10_variance16x8_avx2;
    aom_highbd_10_variance32x16 = aom_highbd_10_variance32x16_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance32x16 = aom_highbd_10_variance32x16_avx2;
    aom_highbd_10_variance32x32 = aom_highbd_10_variance32x32_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance32x32 = aom_highbd_10_variance32x32_avx2;
    aom_highbd_10_variance32x64 = aom_highbd_10_variance32x64_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance32x64 = aom_highbd_10_variance32x64_avx2;
    aom_highbd_10_variance4x4 = aom_highbd_10_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_10_variance4x4 = aom_highbd_10_variance4x4_sse4_1;
    aom_highbd_10_variance64x128 = aom_highbd_10_variance64x128_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance64x128 = aom_highbd_10_variance64x128_avx2;
    aom_highbd_10_variance64x32 = aom_highbd_10_variance64x32_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance64x32 = aom_highbd_10_variance64x32_avx2;
    aom_highbd_10_variance64x64 = aom_highbd_10_variance64x64_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance64x64 = aom_highbd_10_variance64x64_avx2;
    aom_highbd_10_variance8x16 = aom_highbd_10_variance8x16_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance8x16 = aom_highbd_10_variance8x16_avx2;
    aom_highbd_10_variance8x8 = aom_highbd_10_variance8x8_sse2;
    if (flags & HAS_AVX2) aom_highbd_10_variance8x8 = aom_highbd_10_variance8x8_avx2;
    aom_highbd_12_masked_sub_pixel_variance128x128 = aom_highbd_12_masked_sub_pixel_variance128x128_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance128x128 = aom_highbd_12_masked_sub_pixel_variance128x128_ssse3;
    aom_highbd_12_masked_sub_pixel_variance128x64 = aom_highbd_12_masked_sub_pixel_variance128x64_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance128x64 = aom_highbd_12_masked_sub_pixel_variance128x64_ssse3;
    aom_highbd_12_masked_sub_pixel_variance16x16 = aom_highbd_12_masked_sub_pixel_variance16x16_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance16x16 = aom_highbd_12_masked_sub_pixel_variance16x16_ssse3;
    aom_highbd_12_masked_sub_pixel_variance16x32 = aom_highbd_12_masked_sub_pixel_variance16x32_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance16x32 = aom_highbd_12_masked_sub_pixel_variance16x32_ssse3;
    aom_highbd_12_masked_sub_pixel_variance16x4 = aom_highbd_12_masked_sub_pixel_variance16x4_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance16x4 = aom_highbd_12_masked_sub_pixel_variance16x4_ssse3;
    aom_highbd_12_masked_sub_pixel_variance16x64 = aom_highbd_12_masked_sub_pixel_variance16x64_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance16x64 = aom_highbd_12_masked_sub_pixel_variance16x64_ssse3;
    aom_highbd_12_masked_sub_pixel_variance16x8 = aom_highbd_12_masked_sub_pixel_variance16x8_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance16x8 = aom_highbd_12_masked_sub_pixel_variance16x8_ssse3;
    aom_highbd_12_masked_sub_pixel_variance32x16 = aom_highbd_12_masked_sub_pixel_variance32x16_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance32x16 = aom_highbd_12_masked_sub_pixel_variance32x16_ssse3;
    aom_highbd_12_masked_sub_pixel_variance32x32 = aom_highbd_12_masked_sub_pixel_variance32x32_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance32x32 = aom_highbd_12_masked_sub_pixel_variance32x32_ssse3;
    aom_highbd_12_masked_sub_pixel_variance32x64 = aom_highbd_12_masked_sub_pixel_variance32x64_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance32x64 = aom_highbd_12_masked_sub_pixel_variance32x64_ssse3;
    aom_highbd_12_masked_sub_pixel_variance32x8 = aom_highbd_12_masked_sub_pixel_variance32x8_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance32x8 = aom_highbd_12_masked_sub_pixel_variance32x8_ssse3;
    aom_highbd_12_masked_sub_pixel_variance4x16 = aom_highbd_12_masked_sub_pixel_variance4x16_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance4x16 = aom_highbd_12_masked_sub_pixel_variance4x16_ssse3;
    aom_highbd_12_masked_sub_pixel_variance4x4 = aom_highbd_12_masked_sub_pixel_variance4x4_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance4x4 = aom_highbd_12_masked_sub_pixel_variance4x4_ssse3;
    aom_highbd_12_masked_sub_pixel_variance4x8 = aom_highbd_12_masked_sub_pixel_variance4x8_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance4x8 = aom_highbd_12_masked_sub_pixel_variance4x8_ssse3;
    aom_highbd_12_masked_sub_pixel_variance64x128 = aom_highbd_12_masked_sub_pixel_variance64x128_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance64x128 = aom_highbd_12_masked_sub_pixel_variance64x128_ssse3;
    aom_highbd_12_masked_sub_pixel_variance64x16 = aom_highbd_12_masked_sub_pixel_variance64x16_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance64x16 = aom_highbd_12_masked_sub_pixel_variance64x16_ssse3;
    aom_highbd_12_masked_sub_pixel_variance64x32 = aom_highbd_12_masked_sub_pixel_variance64x32_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance64x32 = aom_highbd_12_masked_sub_pixel_variance64x32_ssse3;
    aom_highbd_12_masked_sub_pixel_variance64x64 = aom_highbd_12_masked_sub_pixel_variance64x64_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance64x64 = aom_highbd_12_masked_sub_pixel_variance64x64_ssse3;
    aom_highbd_12_masked_sub_pixel_variance8x16 = aom_highbd_12_masked_sub_pixel_variance8x16_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance8x16 = aom_highbd_12_masked_sub_pixel_variance8x16_ssse3;
    aom_highbd_12_masked_sub_pixel_variance8x32 = aom_highbd_12_masked_sub_pixel_variance8x32_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance8x32 = aom_highbd_12_masked_sub_pixel_variance8x32_ssse3;
    aom_highbd_12_masked_sub_pixel_variance8x4 = aom_highbd_12_masked_sub_pixel_variance8x4_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance8x4 = aom_highbd_12_masked_sub_pixel_variance8x4_ssse3;
    aom_highbd_12_masked_sub_pixel_variance8x8 = aom_highbd_12_masked_sub_pixel_variance8x8_c;
    if (flags & HAS_SSSE3) aom_highbd_12_masked_sub_pixel_variance8x8 = aom_highbd_12_masked_sub_pixel_variance8x8_ssse3;
    aom_highbd_12_obmc_variance128x128 = aom_highbd_12_obmc_variance128x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance128x128 = aom_highbd_12_obmc_variance128x128_sse4_1;
    aom_highbd_12_obmc_variance128x64 = aom_highbd_12_obmc_variance128x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance128x64 = aom_highbd_12_obmc_variance128x64_sse4_1;
    aom_highbd_12_obmc_variance16x16 = aom_highbd_12_obmc_variance16x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance16x16 = aom_highbd_12_obmc_variance16x16_sse4_1;
    aom_highbd_12_obmc_variance16x32 = aom_highbd_12_obmc_variance16x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance16x32 = aom_highbd_12_obmc_variance16x32_sse4_1;
    aom_highbd_12_obmc_variance16x4 = aom_highbd_12_obmc_variance16x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance16x4 = aom_highbd_12_obmc_variance16x4_sse4_1;
    aom_highbd_12_obmc_variance16x64 = aom_highbd_12_obmc_variance16x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance16x64 = aom_highbd_12_obmc_variance16x64_sse4_1;
    aom_highbd_12_obmc_variance16x8 = aom_highbd_12_obmc_variance16x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance16x8 = aom_highbd_12_obmc_variance16x8_sse4_1;
    aom_highbd_12_obmc_variance32x16 = aom_highbd_12_obmc_variance32x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance32x16 = aom_highbd_12_obmc_variance32x16_sse4_1;
    aom_highbd_12_obmc_variance32x32 = aom_highbd_12_obmc_variance32x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance32x32 = aom_highbd_12_obmc_variance32x32_sse4_1;
    aom_highbd_12_obmc_variance32x64 = aom_highbd_12_obmc_variance32x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance32x64 = aom_highbd_12_obmc_variance32x64_sse4_1;
    aom_highbd_12_obmc_variance32x8 = aom_highbd_12_obmc_variance32x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance32x8 = aom_highbd_12_obmc_variance32x8_sse4_1;
    aom_highbd_12_obmc_variance4x16 = aom_highbd_12_obmc_variance4x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance4x16 = aom_highbd_12_obmc_variance4x16_sse4_1;
    aom_highbd_12_obmc_variance4x4 = aom_highbd_12_obmc_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance4x4 = aom_highbd_12_obmc_variance4x4_sse4_1;
    aom_highbd_12_obmc_variance4x8 = aom_highbd_12_obmc_variance4x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance4x8 = aom_highbd_12_obmc_variance4x8_sse4_1;
    aom_highbd_12_obmc_variance64x128 = aom_highbd_12_obmc_variance64x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance64x128 = aom_highbd_12_obmc_variance64x128_sse4_1;
    aom_highbd_12_obmc_variance64x16 = aom_highbd_12_obmc_variance64x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance64x16 = aom_highbd_12_obmc_variance64x16_sse4_1;
    aom_highbd_12_obmc_variance64x32 = aom_highbd_12_obmc_variance64x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance64x32 = aom_highbd_12_obmc_variance64x32_sse4_1;
    aom_highbd_12_obmc_variance64x64 = aom_highbd_12_obmc_variance64x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance64x64 = aom_highbd_12_obmc_variance64x64_sse4_1;
    aom_highbd_12_obmc_variance8x16 = aom_highbd_12_obmc_variance8x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance8x16 = aom_highbd_12_obmc_variance8x16_sse4_1;
    aom_highbd_12_obmc_variance8x32 = aom_highbd_12_obmc_variance8x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance8x32 = aom_highbd_12_obmc_variance8x32_sse4_1;
    aom_highbd_12_obmc_variance8x4 = aom_highbd_12_obmc_variance8x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance8x4 = aom_highbd_12_obmc_variance8x4_sse4_1;
    aom_highbd_12_obmc_variance8x8 = aom_highbd_12_obmc_variance8x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_obmc_variance8x8 = aom_highbd_12_obmc_variance8x8_sse4_1;
    aom_highbd_12_sub_pixel_avg_variance4x4 = aom_highbd_12_sub_pixel_avg_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_sub_pixel_avg_variance4x4 = aom_highbd_12_sub_pixel_avg_variance4x4_sse4_1;
    aom_highbd_12_sub_pixel_variance4x4 = aom_highbd_12_sub_pixel_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_sub_pixel_variance4x4 = aom_highbd_12_sub_pixel_variance4x4_sse4_1;
    aom_highbd_12_variance4x4 = aom_highbd_12_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_12_variance4x4 = aom_highbd_12_variance4x4_sse4_1;
    aom_highbd_8_masked_sub_pixel_variance128x128 = aom_highbd_8_masked_sub_pixel_variance128x128_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance128x128 = aom_highbd_8_masked_sub_pixel_variance128x128_ssse3;
    aom_highbd_8_masked_sub_pixel_variance128x64 = aom_highbd_8_masked_sub_pixel_variance128x64_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance128x64 = aom_highbd_8_masked_sub_pixel_variance128x64_ssse3;
    aom_highbd_8_masked_sub_pixel_variance16x16 = aom_highbd_8_masked_sub_pixel_variance16x16_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance16x16 = aom_highbd_8_masked_sub_pixel_variance16x16_ssse3;
    aom_highbd_8_masked_sub_pixel_variance16x32 = aom_highbd_8_masked_sub_pixel_variance16x32_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance16x32 = aom_highbd_8_masked_sub_pixel_variance16x32_ssse3;
    aom_highbd_8_masked_sub_pixel_variance16x4 = aom_highbd_8_masked_sub_pixel_variance16x4_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance16x4 = aom_highbd_8_masked_sub_pixel_variance16x4_ssse3;
    aom_highbd_8_masked_sub_pixel_variance16x64 = aom_highbd_8_masked_sub_pixel_variance16x64_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance16x64 = aom_highbd_8_masked_sub_pixel_variance16x64_ssse3;
    aom_highbd_8_masked_sub_pixel_variance16x8 = aom_highbd_8_masked_sub_pixel_variance16x8_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance16x8 = aom_highbd_8_masked_sub_pixel_variance16x8_ssse3;
    aom_highbd_8_masked_sub_pixel_variance32x16 = aom_highbd_8_masked_sub_pixel_variance32x16_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance32x16 = aom_highbd_8_masked_sub_pixel_variance32x16_ssse3;
    aom_highbd_8_masked_sub_pixel_variance32x32 = aom_highbd_8_masked_sub_pixel_variance32x32_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance32x32 = aom_highbd_8_masked_sub_pixel_variance32x32_ssse3;
    aom_highbd_8_masked_sub_pixel_variance32x64 = aom_highbd_8_masked_sub_pixel_variance32x64_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance32x64 = aom_highbd_8_masked_sub_pixel_variance32x64_ssse3;
    aom_highbd_8_masked_sub_pixel_variance32x8 = aom_highbd_8_masked_sub_pixel_variance32x8_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance32x8 = aom_highbd_8_masked_sub_pixel_variance32x8_ssse3;
    aom_highbd_8_masked_sub_pixel_variance4x16 = aom_highbd_8_masked_sub_pixel_variance4x16_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance4x16 = aom_highbd_8_masked_sub_pixel_variance4x16_ssse3;
    aom_highbd_8_masked_sub_pixel_variance4x4 = aom_highbd_8_masked_sub_pixel_variance4x4_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance4x4 = aom_highbd_8_masked_sub_pixel_variance4x4_ssse3;
    aom_highbd_8_masked_sub_pixel_variance4x8 = aom_highbd_8_masked_sub_pixel_variance4x8_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance4x8 = aom_highbd_8_masked_sub_pixel_variance4x8_ssse3;
    aom_highbd_8_masked_sub_pixel_variance64x128 = aom_highbd_8_masked_sub_pixel_variance64x128_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance64x128 = aom_highbd_8_masked_sub_pixel_variance64x128_ssse3;
    aom_highbd_8_masked_sub_pixel_variance64x16 = aom_highbd_8_masked_sub_pixel_variance64x16_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance64x16 = aom_highbd_8_masked_sub_pixel_variance64x16_ssse3;
    aom_highbd_8_masked_sub_pixel_variance64x32 = aom_highbd_8_masked_sub_pixel_variance64x32_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance64x32 = aom_highbd_8_masked_sub_pixel_variance64x32_ssse3;
    aom_highbd_8_masked_sub_pixel_variance64x64 = aom_highbd_8_masked_sub_pixel_variance64x64_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance64x64 = aom_highbd_8_masked_sub_pixel_variance64x64_ssse3;
    aom_highbd_8_masked_sub_pixel_variance8x16 = aom_highbd_8_masked_sub_pixel_variance8x16_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance8x16 = aom_highbd_8_masked_sub_pixel_variance8x16_ssse3;
    aom_highbd_8_masked_sub_pixel_variance8x32 = aom_highbd_8_masked_sub_pixel_variance8x32_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance8x32 = aom_highbd_8_masked_sub_pixel_variance8x32_ssse3;
    aom_highbd_8_masked_sub_pixel_variance8x4 = aom_highbd_8_masked_sub_pixel_variance8x4_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance8x4 = aom_highbd_8_masked_sub_pixel_variance8x4_ssse3;
    aom_highbd_8_masked_sub_pixel_variance8x8 = aom_highbd_8_masked_sub_pixel_variance8x8_c;
    if (flags & HAS_SSSE3) aom_highbd_8_masked_sub_pixel_variance8x8 = aom_highbd_8_masked_sub_pixel_variance8x8_ssse3;
    aom_highbd_8_sub_pixel_avg_variance4x4 = aom_highbd_8_sub_pixel_avg_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_8_sub_pixel_avg_variance4x4 = aom_highbd_8_sub_pixel_avg_variance4x4_sse4_1;
    aom_highbd_8_sub_pixel_variance4x4 = aom_highbd_8_sub_pixel_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_8_sub_pixel_variance4x4 = aom_highbd_8_sub_pixel_variance4x4_sse4_1;
    aom_highbd_8_variance4x4 = aom_highbd_8_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_8_variance4x4 = aom_highbd_8_variance4x4_sse4_1;
    aom_highbd_blend_a64_hmask = aom_highbd_blend_a64_hmask_c;
    if (flags & HAS_SSE4_1) aom_highbd_blend_a64_hmask = aom_highbd_blend_a64_hmask_sse4_1;
    aom_highbd_blend_a64_mask = aom_highbd_blend_a64_mask_c;
    if (flags & HAS_SSE4_1) aom_highbd_blend_a64_mask = aom_highbd_blend_a64_mask_sse4_1;
    aom_highbd_blend_a64_vmask = aom_highbd_blend_a64_vmask_c;
    if (flags & HAS_SSE4_1) aom_highbd_blend_a64_vmask = aom_highbd_blend_a64_vmask_sse4_1;
    aom_highbd_comp_mask_pred = aom_highbd_comp_mask_pred_sse2;
    if (flags & HAS_AVX2) aom_highbd_comp_mask_pred = aom_highbd_comp_mask_pred_avx2;
    aom_highbd_convolve8_horiz = aom_highbd_convolve8_horiz_sse2;
    if (flags & HAS_AVX2) aom_highbd_convolve8_horiz = aom_highbd_convolve8_horiz_avx2;
    aom_highbd_convolve8_vert = aom_highbd_convolve8_vert_sse2;
    if (flags & HAS_AVX2) aom_highbd_convolve8_vert = aom_highbd_convolve8_vert_avx2;
    aom_highbd_convolve_copy = aom_highbd_convolve_copy_sse2;
    if (flags & HAS_AVX2) aom_highbd_convolve_copy = aom_highbd_convolve_copy_avx2;
    aom_highbd_lpf_horizontal_14_dual = aom_highbd_lpf_horizontal_14_dual_sse2;
    if (flags & HAS_AVX2) aom_highbd_lpf_horizontal_14_dual = aom_highbd_lpf_horizontal_14_dual_avx2;
    aom_highbd_lpf_horizontal_4_dual = aom_highbd_lpf_horizontal_4_dual_sse2;
    if (flags & HAS_AVX2) aom_highbd_lpf_horizontal_4_dual = aom_highbd_lpf_horizontal_4_dual_avx2;
    aom_highbd_lpf_horizontal_8_dual = aom_highbd_lpf_horizontal_8_dual_sse2;
    if (flags & HAS_AVX2) aom_highbd_lpf_horizontal_8_dual = aom_highbd_lpf_horizontal_8_dual_avx2;
    aom_highbd_lpf_vertical_14_dual = aom_highbd_lpf_vertical_14_dual_sse2;
    if (flags & HAS_AVX2) aom_highbd_lpf_vertical_14_dual = aom_highbd_lpf_vertical_14_dual_avx2;
    aom_highbd_lpf_vertical_4_dual = aom_highbd_lpf_vertical_4_dual_sse2;
    if (flags & HAS_AVX2) aom_highbd_lpf_vertical_4_dual = aom_highbd_lpf_vertical_4_dual_avx2;
    aom_highbd_lpf_vertical_8_dual = aom_highbd_lpf_vertical_8_dual_sse2;
    if (flags & HAS_AVX2) aom_highbd_lpf_vertical_8_dual = aom_highbd_lpf_vertical_8_dual_avx2;
    aom_highbd_masked_sad128x128 = aom_highbd_masked_sad128x128_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad128x128 = aom_highbd_masked_sad128x128_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad128x128 = aom_highbd_masked_sad128x128_avx2;
    aom_highbd_masked_sad128x64 = aom_highbd_masked_sad128x64_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad128x64 = aom_highbd_masked_sad128x64_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad128x64 = aom_highbd_masked_sad128x64_avx2;
    aom_highbd_masked_sad16x16 = aom_highbd_masked_sad16x16_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad16x16 = aom_highbd_masked_sad16x16_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad16x16 = aom_highbd_masked_sad16x16_avx2;
    aom_highbd_masked_sad16x32 = aom_highbd_masked_sad16x32_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad16x32 = aom_highbd_masked_sad16x32_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad16x32 = aom_highbd_masked_sad16x32_avx2;
    aom_highbd_masked_sad16x4 = aom_highbd_masked_sad16x4_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad16x4 = aom_highbd_masked_sad16x4_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad16x4 = aom_highbd_masked_sad16x4_avx2;
    aom_highbd_masked_sad16x64 = aom_highbd_masked_sad16x64_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad16x64 = aom_highbd_masked_sad16x64_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad16x64 = aom_highbd_masked_sad16x64_avx2;
    aom_highbd_masked_sad16x8 = aom_highbd_masked_sad16x8_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad16x8 = aom_highbd_masked_sad16x8_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad16x8 = aom_highbd_masked_sad16x8_avx2;
    aom_highbd_masked_sad32x16 = aom_highbd_masked_sad32x16_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad32x16 = aom_highbd_masked_sad32x16_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad32x16 = aom_highbd_masked_sad32x16_avx2;
    aom_highbd_masked_sad32x32 = aom_highbd_masked_sad32x32_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad32x32 = aom_highbd_masked_sad32x32_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad32x32 = aom_highbd_masked_sad32x32_avx2;
    aom_highbd_masked_sad32x64 = aom_highbd_masked_sad32x64_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad32x64 = aom_highbd_masked_sad32x64_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad32x64 = aom_highbd_masked_sad32x64_avx2;
    aom_highbd_masked_sad32x8 = aom_highbd_masked_sad32x8_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad32x8 = aom_highbd_masked_sad32x8_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad32x8 = aom_highbd_masked_sad32x8_avx2;
    aom_highbd_masked_sad4x16 = aom_highbd_masked_sad4x16_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad4x16 = aom_highbd_masked_sad4x16_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad4x16 = aom_highbd_masked_sad4x16_avx2;
    aom_highbd_masked_sad4x4 = aom_highbd_masked_sad4x4_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad4x4 = aom_highbd_masked_sad4x4_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad4x4 = aom_highbd_masked_sad4x4_avx2;
    aom_highbd_masked_sad4x8 = aom_highbd_masked_sad4x8_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad4x8 = aom_highbd_masked_sad4x8_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad4x8 = aom_highbd_masked_sad4x8_avx2;
    aom_highbd_masked_sad64x128 = aom_highbd_masked_sad64x128_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad64x128 = aom_highbd_masked_sad64x128_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad64x128 = aom_highbd_masked_sad64x128_avx2;
    aom_highbd_masked_sad64x16 = aom_highbd_masked_sad64x16_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad64x16 = aom_highbd_masked_sad64x16_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad64x16 = aom_highbd_masked_sad64x16_avx2;
    aom_highbd_masked_sad64x32 = aom_highbd_masked_sad64x32_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad64x32 = aom_highbd_masked_sad64x32_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad64x32 = aom_highbd_masked_sad64x32_avx2;
    aom_highbd_masked_sad64x64 = aom_highbd_masked_sad64x64_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad64x64 = aom_highbd_masked_sad64x64_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad64x64 = aom_highbd_masked_sad64x64_avx2;
    aom_highbd_masked_sad8x16 = aom_highbd_masked_sad8x16_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad8x16 = aom_highbd_masked_sad8x16_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad8x16 = aom_highbd_masked_sad8x16_avx2;
    aom_highbd_masked_sad8x32 = aom_highbd_masked_sad8x32_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad8x32 = aom_highbd_masked_sad8x32_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad8x32 = aom_highbd_masked_sad8x32_avx2;
    aom_highbd_masked_sad8x4 = aom_highbd_masked_sad8x4_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad8x4 = aom_highbd_masked_sad8x4_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad8x4 = aom_highbd_masked_sad8x4_avx2;
    aom_highbd_masked_sad8x8 = aom_highbd_masked_sad8x8_c;
    if (flags & HAS_SSSE3) aom_highbd_masked_sad8x8 = aom_highbd_masked_sad8x8_ssse3;
    if (flags & HAS_AVX2) aom_highbd_masked_sad8x8 = aom_highbd_masked_sad8x8_avx2;
    aom_highbd_obmc_sad128x128 = aom_highbd_obmc_sad128x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad128x128 = aom_highbd_obmc_sad128x128_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad128x128 = aom_highbd_obmc_sad128x128_avx2;
    aom_highbd_obmc_sad128x64 = aom_highbd_obmc_sad128x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad128x64 = aom_highbd_obmc_sad128x64_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad128x64 = aom_highbd_obmc_sad128x64_avx2;
    aom_highbd_obmc_sad16x16 = aom_highbd_obmc_sad16x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad16x16 = aom_highbd_obmc_sad16x16_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad16x16 = aom_highbd_obmc_sad16x16_avx2;
    aom_highbd_obmc_sad16x32 = aom_highbd_obmc_sad16x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad16x32 = aom_highbd_obmc_sad16x32_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad16x32 = aom_highbd_obmc_sad16x32_avx2;
    aom_highbd_obmc_sad16x4 = aom_highbd_obmc_sad16x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad16x4 = aom_highbd_obmc_sad16x4_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad16x4 = aom_highbd_obmc_sad16x4_avx2;
    aom_highbd_obmc_sad16x64 = aom_highbd_obmc_sad16x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad16x64 = aom_highbd_obmc_sad16x64_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad16x64 = aom_highbd_obmc_sad16x64_avx2;
    aom_highbd_obmc_sad16x8 = aom_highbd_obmc_sad16x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad16x8 = aom_highbd_obmc_sad16x8_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad16x8 = aom_highbd_obmc_sad16x8_avx2;
    aom_highbd_obmc_sad32x16 = aom_highbd_obmc_sad32x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad32x16 = aom_highbd_obmc_sad32x16_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad32x16 = aom_highbd_obmc_sad32x16_avx2;
    aom_highbd_obmc_sad32x32 = aom_highbd_obmc_sad32x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad32x32 = aom_highbd_obmc_sad32x32_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad32x32 = aom_highbd_obmc_sad32x32_avx2;
    aom_highbd_obmc_sad32x64 = aom_highbd_obmc_sad32x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad32x64 = aom_highbd_obmc_sad32x64_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad32x64 = aom_highbd_obmc_sad32x64_avx2;
    aom_highbd_obmc_sad32x8 = aom_highbd_obmc_sad32x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad32x8 = aom_highbd_obmc_sad32x8_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad32x8 = aom_highbd_obmc_sad32x8_avx2;
    aom_highbd_obmc_sad4x16 = aom_highbd_obmc_sad4x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad4x16 = aom_highbd_obmc_sad4x16_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad4x16 = aom_highbd_obmc_sad4x16_avx2;
    aom_highbd_obmc_sad4x4 = aom_highbd_obmc_sad4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad4x4 = aom_highbd_obmc_sad4x4_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad4x4 = aom_highbd_obmc_sad4x4_avx2;
    aom_highbd_obmc_sad4x8 = aom_highbd_obmc_sad4x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad4x8 = aom_highbd_obmc_sad4x8_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad4x8 = aom_highbd_obmc_sad4x8_avx2;
    aom_highbd_obmc_sad64x128 = aom_highbd_obmc_sad64x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad64x128 = aom_highbd_obmc_sad64x128_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad64x128 = aom_highbd_obmc_sad64x128_avx2;
    aom_highbd_obmc_sad64x16 = aom_highbd_obmc_sad64x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad64x16 = aom_highbd_obmc_sad64x16_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad64x16 = aom_highbd_obmc_sad64x16_avx2;
    aom_highbd_obmc_sad64x32 = aom_highbd_obmc_sad64x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad64x32 = aom_highbd_obmc_sad64x32_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad64x32 = aom_highbd_obmc_sad64x32_avx2;
    aom_highbd_obmc_sad64x64 = aom_highbd_obmc_sad64x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad64x64 = aom_highbd_obmc_sad64x64_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad64x64 = aom_highbd_obmc_sad64x64_avx2;
    aom_highbd_obmc_sad8x16 = aom_highbd_obmc_sad8x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad8x16 = aom_highbd_obmc_sad8x16_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad8x16 = aom_highbd_obmc_sad8x16_avx2;
    aom_highbd_obmc_sad8x32 = aom_highbd_obmc_sad8x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad8x32 = aom_highbd_obmc_sad8x32_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad8x32 = aom_highbd_obmc_sad8x32_avx2;
    aom_highbd_obmc_sad8x4 = aom_highbd_obmc_sad8x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad8x4 = aom_highbd_obmc_sad8x4_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad8x4 = aom_highbd_obmc_sad8x4_avx2;
    aom_highbd_obmc_sad8x8 = aom_highbd_obmc_sad8x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_sad8x8 = aom_highbd_obmc_sad8x8_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_obmc_sad8x8 = aom_highbd_obmc_sad8x8_avx2;
    aom_highbd_obmc_variance128x128 = aom_highbd_obmc_variance128x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance128x128 = aom_highbd_obmc_variance128x128_sse4_1;
    aom_highbd_obmc_variance128x64 = aom_highbd_obmc_variance128x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance128x64 = aom_highbd_obmc_variance128x64_sse4_1;
    aom_highbd_obmc_variance16x16 = aom_highbd_obmc_variance16x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance16x16 = aom_highbd_obmc_variance16x16_sse4_1;
    aom_highbd_obmc_variance16x32 = aom_highbd_obmc_variance16x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance16x32 = aom_highbd_obmc_variance16x32_sse4_1;
    aom_highbd_obmc_variance16x4 = aom_highbd_obmc_variance16x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance16x4 = aom_highbd_obmc_variance16x4_sse4_1;
    aom_highbd_obmc_variance16x64 = aom_highbd_obmc_variance16x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance16x64 = aom_highbd_obmc_variance16x64_sse4_1;
    aom_highbd_obmc_variance16x8 = aom_highbd_obmc_variance16x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance16x8 = aom_highbd_obmc_variance16x8_sse4_1;
    aom_highbd_obmc_variance32x16 = aom_highbd_obmc_variance32x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance32x16 = aom_highbd_obmc_variance32x16_sse4_1;
    aom_highbd_obmc_variance32x32 = aom_highbd_obmc_variance32x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance32x32 = aom_highbd_obmc_variance32x32_sse4_1;
    aom_highbd_obmc_variance32x64 = aom_highbd_obmc_variance32x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance32x64 = aom_highbd_obmc_variance32x64_sse4_1;
    aom_highbd_obmc_variance32x8 = aom_highbd_obmc_variance32x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance32x8 = aom_highbd_obmc_variance32x8_sse4_1;
    aom_highbd_obmc_variance4x16 = aom_highbd_obmc_variance4x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance4x16 = aom_highbd_obmc_variance4x16_sse4_1;
    aom_highbd_obmc_variance4x4 = aom_highbd_obmc_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance4x4 = aom_highbd_obmc_variance4x4_sse4_1;
    aom_highbd_obmc_variance4x8 = aom_highbd_obmc_variance4x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance4x8 = aom_highbd_obmc_variance4x8_sse4_1;
    aom_highbd_obmc_variance64x128 = aom_highbd_obmc_variance64x128_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance64x128 = aom_highbd_obmc_variance64x128_sse4_1;
    aom_highbd_obmc_variance64x16 = aom_highbd_obmc_variance64x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance64x16 = aom_highbd_obmc_variance64x16_sse4_1;
    aom_highbd_obmc_variance64x32 = aom_highbd_obmc_variance64x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance64x32 = aom_highbd_obmc_variance64x32_sse4_1;
    aom_highbd_obmc_variance64x64 = aom_highbd_obmc_variance64x64_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance64x64 = aom_highbd_obmc_variance64x64_sse4_1;
    aom_highbd_obmc_variance8x16 = aom_highbd_obmc_variance8x16_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance8x16 = aom_highbd_obmc_variance8x16_sse4_1;
    aom_highbd_obmc_variance8x32 = aom_highbd_obmc_variance8x32_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance8x32 = aom_highbd_obmc_variance8x32_sse4_1;
    aom_highbd_obmc_variance8x4 = aom_highbd_obmc_variance8x4_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance8x4 = aom_highbd_obmc_variance8x4_sse4_1;
    aom_highbd_obmc_variance8x8 = aom_highbd_obmc_variance8x8_c;
    if (flags & HAS_SSE4_1) aom_highbd_obmc_variance8x8 = aom_highbd_obmc_variance8x8_sse4_1;
    aom_highbd_quantize_b = aom_highbd_quantize_b_sse2;
    if (flags & HAS_AVX2) aom_highbd_quantize_b = aom_highbd_quantize_b_avx2;
    aom_highbd_sad128x128 = aom_highbd_sad128x128_c;
    if (flags & HAS_AVX2) aom_highbd_sad128x128 = aom_highbd_sad128x128_avx2;
    aom_highbd_sad128x128_avg = aom_highbd_sad128x128_avg_c;
    if (flags & HAS_AVX2) aom_highbd_sad128x128_avg = aom_highbd_sad128x128_avg_avx2;
    aom_highbd_sad128x128x4d = aom_highbd_sad128x128x4d_c;
    if (flags & HAS_AVX2) aom_highbd_sad128x128x4d = aom_highbd_sad128x128x4d_avx2;
    aom_highbd_sad128x64 = aom_highbd_sad128x64_c;
    if (flags & HAS_AVX2) aom_highbd_sad128x64 = aom_highbd_sad128x64_avx2;
    aom_highbd_sad128x64_avg = aom_highbd_sad128x64_avg_c;
    if (flags & HAS_AVX2) aom_highbd_sad128x64_avg = aom_highbd_sad128x64_avg_avx2;
    aom_highbd_sad128x64x4d = aom_highbd_sad128x64x4d_c;
    if (flags & HAS_AVX2) aom_highbd_sad128x64x4d = aom_highbd_sad128x64x4d_avx2;
    aom_highbd_sad16x16 = aom_highbd_sad16x16_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x16 = aom_highbd_sad16x16_avx2;
    aom_highbd_sad16x16_avg = aom_highbd_sad16x16_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x16_avg = aom_highbd_sad16x16_avg_avx2;
    aom_highbd_sad16x16x4d = aom_highbd_sad16x16x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x16x4d = aom_highbd_sad16x16x4d_avx2;
    aom_highbd_sad16x32 = aom_highbd_sad16x32_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x32 = aom_highbd_sad16x32_avx2;
    aom_highbd_sad16x32_avg = aom_highbd_sad16x32_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x32_avg = aom_highbd_sad16x32_avg_avx2;
    aom_highbd_sad16x32x4d = aom_highbd_sad16x32x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x32x4d = aom_highbd_sad16x32x4d_avx2;
    aom_highbd_sad16x8 = aom_highbd_sad16x8_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x8 = aom_highbd_sad16x8_avx2;
    aom_highbd_sad16x8_avg = aom_highbd_sad16x8_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x8_avg = aom_highbd_sad16x8_avg_avx2;
    aom_highbd_sad16x8x4d = aom_highbd_sad16x8x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad16x8x4d = aom_highbd_sad16x8x4d_avx2;
    aom_highbd_sad32x16 = aom_highbd_sad32x16_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x16 = aom_highbd_sad32x16_avx2;
    aom_highbd_sad32x16_avg = aom_highbd_sad32x16_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x16_avg = aom_highbd_sad32x16_avg_avx2;
    aom_highbd_sad32x16x4d = aom_highbd_sad32x16x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x16x4d = aom_highbd_sad32x16x4d_avx2;
    aom_highbd_sad32x32 = aom_highbd_sad32x32_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x32 = aom_highbd_sad32x32_avx2;
    aom_highbd_sad32x32_avg = aom_highbd_sad32x32_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x32_avg = aom_highbd_sad32x32_avg_avx2;
    aom_highbd_sad32x32x4d = aom_highbd_sad32x32x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x32x4d = aom_highbd_sad32x32x4d_avx2;
    aom_highbd_sad32x64 = aom_highbd_sad32x64_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x64 = aom_highbd_sad32x64_avx2;
    aom_highbd_sad32x64_avg = aom_highbd_sad32x64_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x64_avg = aom_highbd_sad32x64_avg_avx2;
    aom_highbd_sad32x64x4d = aom_highbd_sad32x64x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad32x64x4d = aom_highbd_sad32x64x4d_avx2;
    aom_highbd_sad64x128 = aom_highbd_sad64x128_c;
    if (flags & HAS_AVX2) aom_highbd_sad64x128 = aom_highbd_sad64x128_avx2;
    aom_highbd_sad64x128_avg = aom_highbd_sad64x128_avg_c;
    if (flags & HAS_AVX2) aom_highbd_sad64x128_avg = aom_highbd_sad64x128_avg_avx2;
    aom_highbd_sad64x128x4d = aom_highbd_sad64x128x4d_c;
    if (flags & HAS_AVX2) aom_highbd_sad64x128x4d = aom_highbd_sad64x128x4d_avx2;
    aom_highbd_sad64x32 = aom_highbd_sad64x32_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad64x32 = aom_highbd_sad64x32_avx2;
    aom_highbd_sad64x32_avg = aom_highbd_sad64x32_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad64x32_avg = aom_highbd_sad64x32_avg_avx2;
    aom_highbd_sad64x32x4d = aom_highbd_sad64x32x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad64x32x4d = aom_highbd_sad64x32x4d_avx2;
    aom_highbd_sad64x64 = aom_highbd_sad64x64_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad64x64 = aom_highbd_sad64x64_avx2;
    aom_highbd_sad64x64_avg = aom_highbd_sad64x64_avg_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad64x64_avg = aom_highbd_sad64x64_avg_avx2;
    aom_highbd_sad64x64x4d = aom_highbd_sad64x64x4d_sse2;
    if (flags & HAS_AVX2) aom_highbd_sad64x64x4d = aom_highbd_sad64x64x4d_avx2;
    aom_highbd_sse = aom_highbd_sse_c;
    if (flags & HAS_SSE4_1) aom_highbd_sse = aom_highbd_sse_sse4_1;
    if (flags & HAS_AVX2) aom_highbd_sse = aom_highbd_sse_avx2;
    aom_ifft16x16_float = aom_ifft16x16_float_sse2;
    if (flags & HAS_AVX2) aom_ifft16x16_float = aom_ifft16x16_float_avx2;
    aom_ifft32x32_float = aom_ifft32x32_float_sse2;
    if (flags & HAS_AVX2) aom_ifft32x32_float = aom_ifft32x32_float_avx2;
    aom_ifft8x8_float = aom_ifft8x8_float_sse2;
    if (flags & HAS_AVX2) aom_ifft8x8_float = aom_ifft8x8_float_avx2;
    aom_jnt_comp_avg_pred = aom_jnt_comp_avg_pred_c;
    if (flags & HAS_SSSE3) aom_jnt_comp_avg_pred = aom_jnt_comp_avg_pred_ssse3;
    aom_jnt_comp_avg_upsampled_pred = aom_jnt_comp_avg_upsampled_pred_c;
    if (flags & HAS_SSSE3) aom_jnt_comp_avg_upsampled_pred = aom_jnt_comp_avg_upsampled_pred_ssse3;
    aom_jnt_sad128x128_avg = aom_jnt_sad128x128_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad128x128_avg = aom_jnt_sad128x128_avg_ssse3;
    aom_jnt_sad128x64_avg = aom_jnt_sad128x64_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad128x64_avg = aom_jnt_sad128x64_avg_ssse3;
    aom_jnt_sad16x16_avg = aom_jnt_sad16x16_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad16x16_avg = aom_jnt_sad16x16_avg_ssse3;
    aom_jnt_sad16x32_avg = aom_jnt_sad16x32_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad16x32_avg = aom_jnt_sad16x32_avg_ssse3;
    aom_jnt_sad16x4_avg = aom_jnt_sad16x4_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad16x4_avg = aom_jnt_sad16x4_avg_ssse3;
    aom_jnt_sad16x64_avg = aom_jnt_sad16x64_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad16x64_avg = aom_jnt_sad16x64_avg_ssse3;
    aom_jnt_sad16x8_avg = aom_jnt_sad16x8_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad16x8_avg = aom_jnt_sad16x8_avg_ssse3;
    aom_jnt_sad32x16_avg = aom_jnt_sad32x16_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad32x16_avg = aom_jnt_sad32x16_avg_ssse3;
    aom_jnt_sad32x32_avg = aom_jnt_sad32x32_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad32x32_avg = aom_jnt_sad32x32_avg_ssse3;
    aom_jnt_sad32x64_avg = aom_jnt_sad32x64_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad32x64_avg = aom_jnt_sad32x64_avg_ssse3;
    aom_jnt_sad32x8_avg = aom_jnt_sad32x8_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad32x8_avg = aom_jnt_sad32x8_avg_ssse3;
    aom_jnt_sad4x16_avg = aom_jnt_sad4x16_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad4x16_avg = aom_jnt_sad4x16_avg_ssse3;
    aom_jnt_sad4x4_avg = aom_jnt_sad4x4_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad4x4_avg = aom_jnt_sad4x4_avg_ssse3;
    aom_jnt_sad4x8_avg = aom_jnt_sad4x8_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad4x8_avg = aom_jnt_sad4x8_avg_ssse3;
    aom_jnt_sad64x128_avg = aom_jnt_sad64x128_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad64x128_avg = aom_jnt_sad64x128_avg_ssse3;
    aom_jnt_sad64x16_avg = aom_jnt_sad64x16_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad64x16_avg = aom_jnt_sad64x16_avg_ssse3;
    aom_jnt_sad64x32_avg = aom_jnt_sad64x32_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad64x32_avg = aom_jnt_sad64x32_avg_ssse3;
    aom_jnt_sad64x64_avg = aom_jnt_sad64x64_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad64x64_avg = aom_jnt_sad64x64_avg_ssse3;
    aom_jnt_sad8x16_avg = aom_jnt_sad8x16_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad8x16_avg = aom_jnt_sad8x16_avg_ssse3;
    aom_jnt_sad8x32_avg = aom_jnt_sad8x32_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad8x32_avg = aom_jnt_sad8x32_avg_ssse3;
    aom_jnt_sad8x4_avg = aom_jnt_sad8x4_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad8x4_avg = aom_jnt_sad8x4_avg_ssse3;
    aom_jnt_sad8x8_avg = aom_jnt_sad8x8_avg_c;
    if (flags & HAS_SSSE3) aom_jnt_sad8x8_avg = aom_jnt_sad8x8_avg_ssse3;
    aom_jnt_sub_pixel_avg_variance128x128 = aom_jnt_sub_pixel_avg_variance128x128_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance128x128 = aom_jnt_sub_pixel_avg_variance128x128_ssse3;
    aom_jnt_sub_pixel_avg_variance128x64 = aom_jnt_sub_pixel_avg_variance128x64_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance128x64 = aom_jnt_sub_pixel_avg_variance128x64_ssse3;
    aom_jnt_sub_pixel_avg_variance16x16 = aom_jnt_sub_pixel_avg_variance16x16_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance16x16 = aom_jnt_sub_pixel_avg_variance16x16_ssse3;
    aom_jnt_sub_pixel_avg_variance16x32 = aom_jnt_sub_pixel_avg_variance16x32_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance16x32 = aom_jnt_sub_pixel_avg_variance16x32_ssse3;
    aom_jnt_sub_pixel_avg_variance16x4 = aom_jnt_sub_pixel_avg_variance16x4_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance16x4 = aom_jnt_sub_pixel_avg_variance16x4_ssse3;
    aom_jnt_sub_pixel_avg_variance16x64 = aom_jnt_sub_pixel_avg_variance16x64_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance16x64 = aom_jnt_sub_pixel_avg_variance16x64_ssse3;
    aom_jnt_sub_pixel_avg_variance16x8 = aom_jnt_sub_pixel_avg_variance16x8_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance16x8 = aom_jnt_sub_pixel_avg_variance16x8_ssse3;
    aom_jnt_sub_pixel_avg_variance32x16 = aom_jnt_sub_pixel_avg_variance32x16_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance32x16 = aom_jnt_sub_pixel_avg_variance32x16_ssse3;
    aom_jnt_sub_pixel_avg_variance32x32 = aom_jnt_sub_pixel_avg_variance32x32_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance32x32 = aom_jnt_sub_pixel_avg_variance32x32_ssse3;
    aom_jnt_sub_pixel_avg_variance32x64 = aom_jnt_sub_pixel_avg_variance32x64_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance32x64 = aom_jnt_sub_pixel_avg_variance32x64_ssse3;
    aom_jnt_sub_pixel_avg_variance32x8 = aom_jnt_sub_pixel_avg_variance32x8_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance32x8 = aom_jnt_sub_pixel_avg_variance32x8_ssse3;
    aom_jnt_sub_pixel_avg_variance4x16 = aom_jnt_sub_pixel_avg_variance4x16_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance4x16 = aom_jnt_sub_pixel_avg_variance4x16_ssse3;
    aom_jnt_sub_pixel_avg_variance4x4 = aom_jnt_sub_pixel_avg_variance4x4_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance4x4 = aom_jnt_sub_pixel_avg_variance4x4_ssse3;
    aom_jnt_sub_pixel_avg_variance4x8 = aom_jnt_sub_pixel_avg_variance4x8_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance4x8 = aom_jnt_sub_pixel_avg_variance4x8_ssse3;
    aom_jnt_sub_pixel_avg_variance64x128 = aom_jnt_sub_pixel_avg_variance64x128_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance64x128 = aom_jnt_sub_pixel_avg_variance64x128_ssse3;
    aom_jnt_sub_pixel_avg_variance64x16 = aom_jnt_sub_pixel_avg_variance64x16_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance64x16 = aom_jnt_sub_pixel_avg_variance64x16_ssse3;
    aom_jnt_sub_pixel_avg_variance64x32 = aom_jnt_sub_pixel_avg_variance64x32_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance64x32 = aom_jnt_sub_pixel_avg_variance64x32_ssse3;
    aom_jnt_sub_pixel_avg_variance64x64 = aom_jnt_sub_pixel_avg_variance64x64_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance64x64 = aom_jnt_sub_pixel_avg_variance64x64_ssse3;
    aom_jnt_sub_pixel_avg_variance8x16 = aom_jnt_sub_pixel_avg_variance8x16_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance8x16 = aom_jnt_sub_pixel_avg_variance8x16_ssse3;
    aom_jnt_sub_pixel_avg_variance8x32 = aom_jnt_sub_pixel_avg_variance8x32_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance8x32 = aom_jnt_sub_pixel_avg_variance8x32_ssse3;
    aom_jnt_sub_pixel_avg_variance8x4 = aom_jnt_sub_pixel_avg_variance8x4_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance8x4 = aom_jnt_sub_pixel_avg_variance8x4_ssse3;
    aom_jnt_sub_pixel_avg_variance8x8 = aom_jnt_sub_pixel_avg_variance8x8_c;
    if (flags & HAS_SSSE3) aom_jnt_sub_pixel_avg_variance8x8 = aom_jnt_sub_pixel_avg_variance8x8_ssse3;
    aom_lowbd_blend_a64_d16_mask = aom_lowbd_blend_a64_d16_mask_c;
    if (flags & HAS_SSE4_1) aom_lowbd_blend_a64_d16_mask = aom_lowbd_blend_a64_d16_mask_sse4_1;
    if (flags & HAS_AVX2) aom_lowbd_blend_a64_d16_mask = aom_lowbd_blend_a64_d16_mask_avx2;
    aom_masked_sad128x128 = aom_masked_sad128x128_c;
    if (flags & HAS_SSSE3) aom_masked_sad128x128 = aom_masked_sad128x128_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad128x128 = aom_masked_sad128x128_avx2;
    aom_masked_sad128x64 = aom_masked_sad128x64_c;
    if (flags & HAS_SSSE3) aom_masked_sad128x64 = aom_masked_sad128x64_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad128x64 = aom_masked_sad128x64_avx2;
    aom_masked_sad16x16 = aom_masked_sad16x16_c;
    if (flags & HAS_SSSE3) aom_masked_sad16x16 = aom_masked_sad16x16_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad16x16 = aom_masked_sad16x16_avx2;
    aom_masked_sad16x32 = aom_masked_sad16x32_c;
    if (flags & HAS_SSSE3) aom_masked_sad16x32 = aom_masked_sad16x32_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad16x32 = aom_masked_sad16x32_avx2;
    aom_masked_sad16x4 = aom_masked_sad16x4_c;
    if (flags & HAS_SSSE3) aom_masked_sad16x4 = aom_masked_sad16x4_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad16x4 = aom_masked_sad16x4_avx2;
    aom_masked_sad16x64 = aom_masked_sad16x64_c;
    if (flags & HAS_SSSE3) aom_masked_sad16x64 = aom_masked_sad16x64_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad16x64 = aom_masked_sad16x64_avx2;
    aom_masked_sad16x8 = aom_masked_sad16x8_c;
    if (flags & HAS_SSSE3) aom_masked_sad16x8 = aom_masked_sad16x8_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad16x8 = aom_masked_sad16x8_avx2;
    aom_masked_sad32x16 = aom_masked_sad32x16_c;
    if (flags & HAS_SSSE3) aom_masked_sad32x16 = aom_masked_sad32x16_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad32x16 = aom_masked_sad32x16_avx2;
    aom_masked_sad32x32 = aom_masked_sad32x32_c;
    if (flags & HAS_SSSE3) aom_masked_sad32x32 = aom_masked_sad32x32_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad32x32 = aom_masked_sad32x32_avx2;
    aom_masked_sad32x64 = aom_masked_sad32x64_c;
    if (flags & HAS_SSSE3) aom_masked_sad32x64 = aom_masked_sad32x64_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad32x64 = aom_masked_sad32x64_avx2;
    aom_masked_sad32x8 = aom_masked_sad32x8_c;
    if (flags & HAS_SSSE3) aom_masked_sad32x8 = aom_masked_sad32x8_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad32x8 = aom_masked_sad32x8_avx2;
    aom_masked_sad4x16 = aom_masked_sad4x16_c;
    if (flags & HAS_SSSE3) aom_masked_sad4x16 = aom_masked_sad4x16_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad4x16 = aom_masked_sad4x16_avx2;
    aom_masked_sad4x4 = aom_masked_sad4x4_c;
    if (flags & HAS_SSSE3) aom_masked_sad4x4 = aom_masked_sad4x4_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad4x4 = aom_masked_sad4x4_avx2;
    aom_masked_sad4x8 = aom_masked_sad4x8_c;
    if (flags & HAS_SSSE3) aom_masked_sad4x8 = aom_masked_sad4x8_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad4x8 = aom_masked_sad4x8_avx2;
    aom_masked_sad64x128 = aom_masked_sad64x128_c;
    if (flags & HAS_SSSE3) aom_masked_sad64x128 = aom_masked_sad64x128_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad64x128 = aom_masked_sad64x128_avx2;
    aom_masked_sad64x16 = aom_masked_sad64x16_c;
    if (flags & HAS_SSSE3) aom_masked_sad64x16 = aom_masked_sad64x16_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad64x16 = aom_masked_sad64x16_avx2;
    aom_masked_sad64x32 = aom_masked_sad64x32_c;
    if (flags & HAS_SSSE3) aom_masked_sad64x32 = aom_masked_sad64x32_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad64x32 = aom_masked_sad64x32_avx2;
    aom_masked_sad64x64 = aom_masked_sad64x64_c;
    if (flags & HAS_SSSE3) aom_masked_sad64x64 = aom_masked_sad64x64_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad64x64 = aom_masked_sad64x64_avx2;
    aom_masked_sad8x16 = aom_masked_sad8x16_c;
    if (flags & HAS_SSSE3) aom_masked_sad8x16 = aom_masked_sad8x16_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad8x16 = aom_masked_sad8x16_avx2;
    aom_masked_sad8x32 = aom_masked_sad8x32_c;
    if (flags & HAS_SSSE3) aom_masked_sad8x32 = aom_masked_sad8x32_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad8x32 = aom_masked_sad8x32_avx2;
    aom_masked_sad8x4 = aom_masked_sad8x4_c;
    if (flags & HAS_SSSE3) aom_masked_sad8x4 = aom_masked_sad8x4_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad8x4 = aom_masked_sad8x4_avx2;
    aom_masked_sad8x8 = aom_masked_sad8x8_c;
    if (flags & HAS_SSSE3) aom_masked_sad8x8 = aom_masked_sad8x8_ssse3;
    if (flags & HAS_AVX2) aom_masked_sad8x8 = aom_masked_sad8x8_avx2;
    aom_masked_sub_pixel_variance128x128 = aom_masked_sub_pixel_variance128x128_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance128x128 = aom_masked_sub_pixel_variance128x128_ssse3;
    aom_masked_sub_pixel_variance128x64 = aom_masked_sub_pixel_variance128x64_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance128x64 = aom_masked_sub_pixel_variance128x64_ssse3;
    aom_masked_sub_pixel_variance16x16 = aom_masked_sub_pixel_variance16x16_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance16x16 = aom_masked_sub_pixel_variance16x16_ssse3;
    aom_masked_sub_pixel_variance16x32 = aom_masked_sub_pixel_variance16x32_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance16x32 = aom_masked_sub_pixel_variance16x32_ssse3;
    aom_masked_sub_pixel_variance16x4 = aom_masked_sub_pixel_variance16x4_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance16x4 = aom_masked_sub_pixel_variance16x4_ssse3;
    aom_masked_sub_pixel_variance16x64 = aom_masked_sub_pixel_variance16x64_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance16x64 = aom_masked_sub_pixel_variance16x64_ssse3;
    aom_masked_sub_pixel_variance16x8 = aom_masked_sub_pixel_variance16x8_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance16x8 = aom_masked_sub_pixel_variance16x8_ssse3;
    aom_masked_sub_pixel_variance32x16 = aom_masked_sub_pixel_variance32x16_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance32x16 = aom_masked_sub_pixel_variance32x16_ssse3;
    aom_masked_sub_pixel_variance32x32 = aom_masked_sub_pixel_variance32x32_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance32x32 = aom_masked_sub_pixel_variance32x32_ssse3;
    aom_masked_sub_pixel_variance32x64 = aom_masked_sub_pixel_variance32x64_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance32x64 = aom_masked_sub_pixel_variance32x64_ssse3;
    aom_masked_sub_pixel_variance32x8 = aom_masked_sub_pixel_variance32x8_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance32x8 = aom_masked_sub_pixel_variance32x8_ssse3;
    aom_masked_sub_pixel_variance4x16 = aom_masked_sub_pixel_variance4x16_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance4x16 = aom_masked_sub_pixel_variance4x16_ssse3;
    aom_masked_sub_pixel_variance4x4 = aom_masked_sub_pixel_variance4x4_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance4x4 = aom_masked_sub_pixel_variance4x4_ssse3;
    aom_masked_sub_pixel_variance4x8 = aom_masked_sub_pixel_variance4x8_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance4x8 = aom_masked_sub_pixel_variance4x8_ssse3;
    aom_masked_sub_pixel_variance64x128 = aom_masked_sub_pixel_variance64x128_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance64x128 = aom_masked_sub_pixel_variance64x128_ssse3;
    aom_masked_sub_pixel_variance64x16 = aom_masked_sub_pixel_variance64x16_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance64x16 = aom_masked_sub_pixel_variance64x16_ssse3;
    aom_masked_sub_pixel_variance64x32 = aom_masked_sub_pixel_variance64x32_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance64x32 = aom_masked_sub_pixel_variance64x32_ssse3;
    aom_masked_sub_pixel_variance64x64 = aom_masked_sub_pixel_variance64x64_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance64x64 = aom_masked_sub_pixel_variance64x64_ssse3;
    aom_masked_sub_pixel_variance8x16 = aom_masked_sub_pixel_variance8x16_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance8x16 = aom_masked_sub_pixel_variance8x16_ssse3;
    aom_masked_sub_pixel_variance8x32 = aom_masked_sub_pixel_variance8x32_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance8x32 = aom_masked_sub_pixel_variance8x32_ssse3;
    aom_masked_sub_pixel_variance8x4 = aom_masked_sub_pixel_variance8x4_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance8x4 = aom_masked_sub_pixel_variance8x4_ssse3;
    aom_masked_sub_pixel_variance8x8 = aom_masked_sub_pixel_variance8x8_c;
    if (flags & HAS_SSSE3) aom_masked_sub_pixel_variance8x8 = aom_masked_sub_pixel_variance8x8_ssse3;
    aom_mse16x16 = aom_mse16x16_sse2;
    if (flags & HAS_AVX2) aom_mse16x16 = aom_mse16x16_avx2;
    aom_obmc_sad128x128 = aom_obmc_sad128x128_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad128x128 = aom_obmc_sad128x128_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad128x128 = aom_obmc_sad128x128_avx2;
    aom_obmc_sad128x64 = aom_obmc_sad128x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad128x64 = aom_obmc_sad128x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad128x64 = aom_obmc_sad128x64_avx2;
    aom_obmc_sad16x16 = aom_obmc_sad16x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad16x16 = aom_obmc_sad16x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad16x16 = aom_obmc_sad16x16_avx2;
    aom_obmc_sad16x32 = aom_obmc_sad16x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad16x32 = aom_obmc_sad16x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad16x32 = aom_obmc_sad16x32_avx2;
    aom_obmc_sad16x4 = aom_obmc_sad16x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad16x4 = aom_obmc_sad16x4_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad16x4 = aom_obmc_sad16x4_avx2;
    aom_obmc_sad16x64 = aom_obmc_sad16x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad16x64 = aom_obmc_sad16x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad16x64 = aom_obmc_sad16x64_avx2;
    aom_obmc_sad16x8 = aom_obmc_sad16x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad16x8 = aom_obmc_sad16x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad16x8 = aom_obmc_sad16x8_avx2;
    aom_obmc_sad32x16 = aom_obmc_sad32x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad32x16 = aom_obmc_sad32x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad32x16 = aom_obmc_sad32x16_avx2;
    aom_obmc_sad32x32 = aom_obmc_sad32x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad32x32 = aom_obmc_sad32x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad32x32 = aom_obmc_sad32x32_avx2;
    aom_obmc_sad32x64 = aom_obmc_sad32x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad32x64 = aom_obmc_sad32x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad32x64 = aom_obmc_sad32x64_avx2;
    aom_obmc_sad32x8 = aom_obmc_sad32x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad32x8 = aom_obmc_sad32x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad32x8 = aom_obmc_sad32x8_avx2;
    aom_obmc_sad4x16 = aom_obmc_sad4x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad4x16 = aom_obmc_sad4x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad4x16 = aom_obmc_sad4x16_avx2;
    aom_obmc_sad4x4 = aom_obmc_sad4x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad4x4 = aom_obmc_sad4x4_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad4x4 = aom_obmc_sad4x4_avx2;
    aom_obmc_sad4x8 = aom_obmc_sad4x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad4x8 = aom_obmc_sad4x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad4x8 = aom_obmc_sad4x8_avx2;
    aom_obmc_sad64x128 = aom_obmc_sad64x128_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad64x128 = aom_obmc_sad64x128_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad64x128 = aom_obmc_sad64x128_avx2;
    aom_obmc_sad64x16 = aom_obmc_sad64x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad64x16 = aom_obmc_sad64x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad64x16 = aom_obmc_sad64x16_avx2;
    aom_obmc_sad64x32 = aom_obmc_sad64x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad64x32 = aom_obmc_sad64x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad64x32 = aom_obmc_sad64x32_avx2;
    aom_obmc_sad64x64 = aom_obmc_sad64x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad64x64 = aom_obmc_sad64x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad64x64 = aom_obmc_sad64x64_avx2;
    aom_obmc_sad8x16 = aom_obmc_sad8x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad8x16 = aom_obmc_sad8x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad8x16 = aom_obmc_sad8x16_avx2;
    aom_obmc_sad8x32 = aom_obmc_sad8x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad8x32 = aom_obmc_sad8x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad8x32 = aom_obmc_sad8x32_avx2;
    aom_obmc_sad8x4 = aom_obmc_sad8x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad8x4 = aom_obmc_sad8x4_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad8x4 = aom_obmc_sad8x4_avx2;
    aom_obmc_sad8x8 = aom_obmc_sad8x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sad8x8 = aom_obmc_sad8x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_sad8x8 = aom_obmc_sad8x8_avx2;
    aom_obmc_sub_pixel_variance128x128 = aom_obmc_sub_pixel_variance128x128_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance128x128 = aom_obmc_sub_pixel_variance128x128_sse4_1;
    aom_obmc_sub_pixel_variance128x64 = aom_obmc_sub_pixel_variance128x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance128x64 = aom_obmc_sub_pixel_variance128x64_sse4_1;
    aom_obmc_sub_pixel_variance16x16 = aom_obmc_sub_pixel_variance16x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance16x16 = aom_obmc_sub_pixel_variance16x16_sse4_1;
    aom_obmc_sub_pixel_variance16x32 = aom_obmc_sub_pixel_variance16x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance16x32 = aom_obmc_sub_pixel_variance16x32_sse4_1;
    aom_obmc_sub_pixel_variance16x4 = aom_obmc_sub_pixel_variance16x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance16x4 = aom_obmc_sub_pixel_variance16x4_sse4_1;
    aom_obmc_sub_pixel_variance16x64 = aom_obmc_sub_pixel_variance16x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance16x64 = aom_obmc_sub_pixel_variance16x64_sse4_1;
    aom_obmc_sub_pixel_variance16x8 = aom_obmc_sub_pixel_variance16x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance16x8 = aom_obmc_sub_pixel_variance16x8_sse4_1;
    aom_obmc_sub_pixel_variance32x16 = aom_obmc_sub_pixel_variance32x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance32x16 = aom_obmc_sub_pixel_variance32x16_sse4_1;
    aom_obmc_sub_pixel_variance32x32 = aom_obmc_sub_pixel_variance32x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance32x32 = aom_obmc_sub_pixel_variance32x32_sse4_1;
    aom_obmc_sub_pixel_variance32x64 = aom_obmc_sub_pixel_variance32x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance32x64 = aom_obmc_sub_pixel_variance32x64_sse4_1;
    aom_obmc_sub_pixel_variance32x8 = aom_obmc_sub_pixel_variance32x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance32x8 = aom_obmc_sub_pixel_variance32x8_sse4_1;
    aom_obmc_sub_pixel_variance4x16 = aom_obmc_sub_pixel_variance4x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance4x16 = aom_obmc_sub_pixel_variance4x16_sse4_1;
    aom_obmc_sub_pixel_variance4x4 = aom_obmc_sub_pixel_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance4x4 = aom_obmc_sub_pixel_variance4x4_sse4_1;
    aom_obmc_sub_pixel_variance4x8 = aom_obmc_sub_pixel_variance4x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance4x8 = aom_obmc_sub_pixel_variance4x8_sse4_1;
    aom_obmc_sub_pixel_variance64x128 = aom_obmc_sub_pixel_variance64x128_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance64x128 = aom_obmc_sub_pixel_variance64x128_sse4_1;
    aom_obmc_sub_pixel_variance64x16 = aom_obmc_sub_pixel_variance64x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance64x16 = aom_obmc_sub_pixel_variance64x16_sse4_1;
    aom_obmc_sub_pixel_variance64x32 = aom_obmc_sub_pixel_variance64x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance64x32 = aom_obmc_sub_pixel_variance64x32_sse4_1;
    aom_obmc_sub_pixel_variance64x64 = aom_obmc_sub_pixel_variance64x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance64x64 = aom_obmc_sub_pixel_variance64x64_sse4_1;
    aom_obmc_sub_pixel_variance8x16 = aom_obmc_sub_pixel_variance8x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance8x16 = aom_obmc_sub_pixel_variance8x16_sse4_1;
    aom_obmc_sub_pixel_variance8x32 = aom_obmc_sub_pixel_variance8x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance8x32 = aom_obmc_sub_pixel_variance8x32_sse4_1;
    aom_obmc_sub_pixel_variance8x4 = aom_obmc_sub_pixel_variance8x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance8x4 = aom_obmc_sub_pixel_variance8x4_sse4_1;
    aom_obmc_sub_pixel_variance8x8 = aom_obmc_sub_pixel_variance8x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_sub_pixel_variance8x8 = aom_obmc_sub_pixel_variance8x8_sse4_1;
    aom_obmc_variance128x128 = aom_obmc_variance128x128_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance128x128 = aom_obmc_variance128x128_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance128x128 = aom_obmc_variance128x128_avx2;
    aom_obmc_variance128x64 = aom_obmc_variance128x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance128x64 = aom_obmc_variance128x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance128x64 = aom_obmc_variance128x64_avx2;
    aom_obmc_variance16x16 = aom_obmc_variance16x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance16x16 = aom_obmc_variance16x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance16x16 = aom_obmc_variance16x16_avx2;
    aom_obmc_variance16x32 = aom_obmc_variance16x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance16x32 = aom_obmc_variance16x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance16x32 = aom_obmc_variance16x32_avx2;
    aom_obmc_variance16x4 = aom_obmc_variance16x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance16x4 = aom_obmc_variance16x4_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance16x4 = aom_obmc_variance16x4_avx2;
    aom_obmc_variance16x64 = aom_obmc_variance16x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance16x64 = aom_obmc_variance16x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance16x64 = aom_obmc_variance16x64_avx2;
    aom_obmc_variance16x8 = aom_obmc_variance16x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance16x8 = aom_obmc_variance16x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance16x8 = aom_obmc_variance16x8_avx2;
    aom_obmc_variance32x16 = aom_obmc_variance32x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance32x16 = aom_obmc_variance32x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance32x16 = aom_obmc_variance32x16_avx2;
    aom_obmc_variance32x32 = aom_obmc_variance32x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance32x32 = aom_obmc_variance32x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance32x32 = aom_obmc_variance32x32_avx2;
    aom_obmc_variance32x64 = aom_obmc_variance32x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance32x64 = aom_obmc_variance32x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance32x64 = aom_obmc_variance32x64_avx2;
    aom_obmc_variance32x8 = aom_obmc_variance32x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance32x8 = aom_obmc_variance32x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance32x8 = aom_obmc_variance32x8_avx2;
    aom_obmc_variance4x16 = aom_obmc_variance4x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance4x16 = aom_obmc_variance4x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance4x16 = aom_obmc_variance4x16_avx2;
    aom_obmc_variance4x4 = aom_obmc_variance4x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance4x4 = aom_obmc_variance4x4_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance4x4 = aom_obmc_variance4x4_avx2;
    aom_obmc_variance4x8 = aom_obmc_variance4x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance4x8 = aom_obmc_variance4x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance4x8 = aom_obmc_variance4x8_avx2;
    aom_obmc_variance64x128 = aom_obmc_variance64x128_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance64x128 = aom_obmc_variance64x128_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance64x128 = aom_obmc_variance64x128_avx2;
    aom_obmc_variance64x16 = aom_obmc_variance64x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance64x16 = aom_obmc_variance64x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance64x16 = aom_obmc_variance64x16_avx2;
    aom_obmc_variance64x32 = aom_obmc_variance64x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance64x32 = aom_obmc_variance64x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance64x32 = aom_obmc_variance64x32_avx2;
    aom_obmc_variance64x64 = aom_obmc_variance64x64_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance64x64 = aom_obmc_variance64x64_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance64x64 = aom_obmc_variance64x64_avx2;
    aom_obmc_variance8x16 = aom_obmc_variance8x16_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance8x16 = aom_obmc_variance8x16_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance8x16 = aom_obmc_variance8x16_avx2;
    aom_obmc_variance8x32 = aom_obmc_variance8x32_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance8x32 = aom_obmc_variance8x32_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance8x32 = aom_obmc_variance8x32_avx2;
    aom_obmc_variance8x4 = aom_obmc_variance8x4_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance8x4 = aom_obmc_variance8x4_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance8x4 = aom_obmc_variance8x4_avx2;
    aom_obmc_variance8x8 = aom_obmc_variance8x8_c;
    if (flags & HAS_SSE4_1) aom_obmc_variance8x8 = aom_obmc_variance8x8_sse4_1;
    if (flags & HAS_AVX2) aom_obmc_variance8x8 = aom_obmc_variance8x8_avx2;
    aom_paeth_predictor_16x16 = aom_paeth_predictor_16x16_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_16x16 = aom_paeth_predictor_16x16_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_16x16 = aom_paeth_predictor_16x16_avx2;
    aom_paeth_predictor_16x32 = aom_paeth_predictor_16x32_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_16x32 = aom_paeth_predictor_16x32_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_16x32 = aom_paeth_predictor_16x32_avx2;
    aom_paeth_predictor_16x4 = aom_paeth_predictor_16x4_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_16x4 = aom_paeth_predictor_16x4_ssse3;
    aom_paeth_predictor_16x64 = aom_paeth_predictor_16x64_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_16x64 = aom_paeth_predictor_16x64_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_16x64 = aom_paeth_predictor_16x64_avx2;
    aom_paeth_predictor_16x8 = aom_paeth_predictor_16x8_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_16x8 = aom_paeth_predictor_16x8_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_16x8 = aom_paeth_predictor_16x8_avx2;
    aom_paeth_predictor_32x16 = aom_paeth_predictor_32x16_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_32x16 = aom_paeth_predictor_32x16_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_32x16 = aom_paeth_predictor_32x16_avx2;
    aom_paeth_predictor_32x32 = aom_paeth_predictor_32x32_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_32x32 = aom_paeth_predictor_32x32_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_32x32 = aom_paeth_predictor_32x32_avx2;
    aom_paeth_predictor_32x64 = aom_paeth_predictor_32x64_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_32x64 = aom_paeth_predictor_32x64_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_32x64 = aom_paeth_predictor_32x64_avx2;
    aom_paeth_predictor_32x8 = aom_paeth_predictor_32x8_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_32x8 = aom_paeth_predictor_32x8_ssse3;
    aom_paeth_predictor_4x16 = aom_paeth_predictor_4x16_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_4x16 = aom_paeth_predictor_4x16_ssse3;
    aom_paeth_predictor_4x4 = aom_paeth_predictor_4x4_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_4x4 = aom_paeth_predictor_4x4_ssse3;
    aom_paeth_predictor_4x8 = aom_paeth_predictor_4x8_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_4x8 = aom_paeth_predictor_4x8_ssse3;
    aom_paeth_predictor_64x16 = aom_paeth_predictor_64x16_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_64x16 = aom_paeth_predictor_64x16_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_64x16 = aom_paeth_predictor_64x16_avx2;
    aom_paeth_predictor_64x32 = aom_paeth_predictor_64x32_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_64x32 = aom_paeth_predictor_64x32_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_64x32 = aom_paeth_predictor_64x32_avx2;
    aom_paeth_predictor_64x64 = aom_paeth_predictor_64x64_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_64x64 = aom_paeth_predictor_64x64_ssse3;
    if (flags & HAS_AVX2) aom_paeth_predictor_64x64 = aom_paeth_predictor_64x64_avx2;
    aom_paeth_predictor_8x16 = aom_paeth_predictor_8x16_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_8x16 = aom_paeth_predictor_8x16_ssse3;
    aom_paeth_predictor_8x32 = aom_paeth_predictor_8x32_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_8x32 = aom_paeth_predictor_8x32_ssse3;
    aom_paeth_predictor_8x4 = aom_paeth_predictor_8x4_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_8x4 = aom_paeth_predictor_8x4_ssse3;
    aom_paeth_predictor_8x8 = aom_paeth_predictor_8x8_c;
    if (flags & HAS_SSSE3) aom_paeth_predictor_8x8 = aom_paeth_predictor_8x8_ssse3;
    aom_quantize_b = aom_quantize_b_sse2;
    if (flags & HAS_SSSE3) aom_quantize_b = aom_quantize_b_ssse3;
    if (flags & HAS_AVX) aom_quantize_b = aom_quantize_b_avx;
    aom_quantize_b_32x32 = aom_quantize_b_32x32_c;
    if (flags & HAS_SSSE3) aom_quantize_b_32x32 = aom_quantize_b_32x32_ssse3;
    if (flags & HAS_AVX) aom_quantize_b_32x32 = aom_quantize_b_32x32_avx;
    aom_sad128x128 = aom_sad128x128_sse2;
    if (flags & HAS_AVX2) aom_sad128x128 = aom_sad128x128_avx2;
    aom_sad128x128_avg = aom_sad128x128_avg_sse2;
    if (flags & HAS_AVX2) aom_sad128x128_avg = aom_sad128x128_avg_avx2;
    aom_sad128x128x4d = aom_sad128x128x4d_sse2;
    if (flags & HAS_AVX2) aom_sad128x128x4d = aom_sad128x128x4d_avx2;
    aom_sad128x64 = aom_sad128x64_sse2;
    if (flags & HAS_AVX2) aom_sad128x64 = aom_sad128x64_avx2;
    aom_sad128x64_avg = aom_sad128x64_avg_sse2;
    if (flags & HAS_AVX2) aom_sad128x64_avg = aom_sad128x64_avg_avx2;
    aom_sad128x64x4d = aom_sad128x64x4d_sse2;
    if (flags & HAS_AVX2) aom_sad128x64x4d = aom_sad128x64x4d_avx2;
    aom_sad32x16 = aom_sad32x16_sse2;
    if (flags & HAS_AVX2) aom_sad32x16 = aom_sad32x16_avx2;
    aom_sad32x16_avg = aom_sad32x16_avg_sse2;
    if (flags & HAS_AVX2) aom_sad32x16_avg = aom_sad32x16_avg_avx2;
    aom_sad32x32 = aom_sad32x32_sse2;
    if (flags & HAS_AVX2) aom_sad32x32 = aom_sad32x32_avx2;
    aom_sad32x32_avg = aom_sad32x32_avg_sse2;
    if (flags & HAS_AVX2) aom_sad32x32_avg = aom_sad32x32_avg_avx2;
    aom_sad32x32x4d = aom_sad32x32x4d_sse2;
    if (flags & HAS_AVX2) aom_sad32x32x4d = aom_sad32x32x4d_avx2;
    aom_sad32x64 = aom_sad32x64_sse2;
    if (flags & HAS_AVX2) aom_sad32x64 = aom_sad32x64_avx2;
    aom_sad32x64_avg = aom_sad32x64_avg_sse2;
    if (flags & HAS_AVX2) aom_sad32x64_avg = aom_sad32x64_avg_avx2;
    aom_sad32x64x4d = aom_sad32x64x4d_sse2;
    if (flags & HAS_AVX2) aom_sad32x64x4d = aom_sad32x64x4d_avx2;
    aom_sad64x128 = aom_sad64x128_sse2;
    if (flags & HAS_AVX2) aom_sad64x128 = aom_sad64x128_avx2;
    aom_sad64x128_avg = aom_sad64x128_avg_sse2;
    if (flags & HAS_AVX2) aom_sad64x128_avg = aom_sad64x128_avg_avx2;
    aom_sad64x128x4d = aom_sad64x128x4d_sse2;
    if (flags & HAS_AVX2) aom_sad64x128x4d = aom_sad64x128x4d_avx2;
    aom_sad64x32 = aom_sad64x32_sse2;
    if (flags & HAS_AVX2) aom_sad64x32 = aom_sad64x32_avx2;
    aom_sad64x32_avg = aom_sad64x32_avg_sse2;
    if (flags & HAS_AVX2) aom_sad64x32_avg = aom_sad64x32_avg_avx2;
    aom_sad64x32x4d = aom_sad64x32x4d_sse2;
    if (flags & HAS_AVX2) aom_sad64x32x4d = aom_sad64x32x4d_avx2;
    aom_sad64x64 = aom_sad64x64_sse2;
    if (flags & HAS_AVX2) aom_sad64x64 = aom_sad64x64_avx2;
    aom_sad64x64_avg = aom_sad64x64_avg_sse2;
    if (flags & HAS_AVX2) aom_sad64x64_avg = aom_sad64x64_avg_avx2;
    aom_sad64x64x4d = aom_sad64x64x4d_sse2;
    if (flags & HAS_AVX2) aom_sad64x64x4d = aom_sad64x64x4d_avx2;
    aom_smooth_h_predictor_16x16 = aom_smooth_h_predictor_16x16_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_16x16 = aom_smooth_h_predictor_16x16_ssse3;
    aom_smooth_h_predictor_16x32 = aom_smooth_h_predictor_16x32_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_16x32 = aom_smooth_h_predictor_16x32_ssse3;
    aom_smooth_h_predictor_16x4 = aom_smooth_h_predictor_16x4_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_16x4 = aom_smooth_h_predictor_16x4_ssse3;
    aom_smooth_h_predictor_16x64 = aom_smooth_h_predictor_16x64_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_16x64 = aom_smooth_h_predictor_16x64_ssse3;
    aom_smooth_h_predictor_16x8 = aom_smooth_h_predictor_16x8_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_16x8 = aom_smooth_h_predictor_16x8_ssse3;
    aom_smooth_h_predictor_32x16 = aom_smooth_h_predictor_32x16_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_32x16 = aom_smooth_h_predictor_32x16_ssse3;
    aom_smooth_h_predictor_32x32 = aom_smooth_h_predictor_32x32_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_32x32 = aom_smooth_h_predictor_32x32_ssse3;
    aom_smooth_h_predictor_32x64 = aom_smooth_h_predictor_32x64_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_32x64 = aom_smooth_h_predictor_32x64_ssse3;
    aom_smooth_h_predictor_32x8 = aom_smooth_h_predictor_32x8_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_32x8 = aom_smooth_h_predictor_32x8_ssse3;
    aom_smooth_h_predictor_4x16 = aom_smooth_h_predictor_4x16_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_4x16 = aom_smooth_h_predictor_4x16_ssse3;
    aom_smooth_h_predictor_4x4 = aom_smooth_h_predictor_4x4_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_4x4 = aom_smooth_h_predictor_4x4_ssse3;
    aom_smooth_h_predictor_4x8 = aom_smooth_h_predictor_4x8_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_4x8 = aom_smooth_h_predictor_4x8_ssse3;
    aom_smooth_h_predictor_64x16 = aom_smooth_h_predictor_64x16_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_64x16 = aom_smooth_h_predictor_64x16_ssse3;
    aom_smooth_h_predictor_64x32 = aom_smooth_h_predictor_64x32_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_64x32 = aom_smooth_h_predictor_64x32_ssse3;
    aom_smooth_h_predictor_64x64 = aom_smooth_h_predictor_64x64_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_64x64 = aom_smooth_h_predictor_64x64_ssse3;
    aom_smooth_h_predictor_8x16 = aom_smooth_h_predictor_8x16_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_8x16 = aom_smooth_h_predictor_8x16_ssse3;
    aom_smooth_h_predictor_8x32 = aom_smooth_h_predictor_8x32_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_8x32 = aom_smooth_h_predictor_8x32_ssse3;
    aom_smooth_h_predictor_8x4 = aom_smooth_h_predictor_8x4_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_8x4 = aom_smooth_h_predictor_8x4_ssse3;
    aom_smooth_h_predictor_8x8 = aom_smooth_h_predictor_8x8_c;
    if (flags & HAS_SSSE3) aom_smooth_h_predictor_8x8 = aom_smooth_h_predictor_8x8_ssse3;
    aom_smooth_predictor_16x16 = aom_smooth_predictor_16x16_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_16x16 = aom_smooth_predictor_16x16_ssse3;
    aom_smooth_predictor_16x32 = aom_smooth_predictor_16x32_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_16x32 = aom_smooth_predictor_16x32_ssse3;
    aom_smooth_predictor_16x4 = aom_smooth_predictor_16x4_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_16x4 = aom_smooth_predictor_16x4_ssse3;
    aom_smooth_predictor_16x64 = aom_smooth_predictor_16x64_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_16x64 = aom_smooth_predictor_16x64_ssse3;
    aom_smooth_predictor_16x8 = aom_smooth_predictor_16x8_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_16x8 = aom_smooth_predictor_16x8_ssse3;
    aom_smooth_predictor_32x16 = aom_smooth_predictor_32x16_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_32x16 = aom_smooth_predictor_32x16_ssse3;
    aom_smooth_predictor_32x32 = aom_smooth_predictor_32x32_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_32x32 = aom_smooth_predictor_32x32_ssse3;
    aom_smooth_predictor_32x64 = aom_smooth_predictor_32x64_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_32x64 = aom_smooth_predictor_32x64_ssse3;
    aom_smooth_predictor_32x8 = aom_smooth_predictor_32x8_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_32x8 = aom_smooth_predictor_32x8_ssse3;
    aom_smooth_predictor_4x16 = aom_smooth_predictor_4x16_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_4x16 = aom_smooth_predictor_4x16_ssse3;
    aom_smooth_predictor_4x4 = aom_smooth_predictor_4x4_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_4x4 = aom_smooth_predictor_4x4_ssse3;
    aom_smooth_predictor_4x8 = aom_smooth_predictor_4x8_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_4x8 = aom_smooth_predictor_4x8_ssse3;
    aom_smooth_predictor_64x16 = aom_smooth_predictor_64x16_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_64x16 = aom_smooth_predictor_64x16_ssse3;
    aom_smooth_predictor_64x32 = aom_smooth_predictor_64x32_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_64x32 = aom_smooth_predictor_64x32_ssse3;
    aom_smooth_predictor_64x64 = aom_smooth_predictor_64x64_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_64x64 = aom_smooth_predictor_64x64_ssse3;
    aom_smooth_predictor_8x16 = aom_smooth_predictor_8x16_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_8x16 = aom_smooth_predictor_8x16_ssse3;
    aom_smooth_predictor_8x32 = aom_smooth_predictor_8x32_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_8x32 = aom_smooth_predictor_8x32_ssse3;
    aom_smooth_predictor_8x4 = aom_smooth_predictor_8x4_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_8x4 = aom_smooth_predictor_8x4_ssse3;
    aom_smooth_predictor_8x8 = aom_smooth_predictor_8x8_c;
    if (flags & HAS_SSSE3) aom_smooth_predictor_8x8 = aom_smooth_predictor_8x8_ssse3;
    aom_smooth_v_predictor_16x16 = aom_smooth_v_predictor_16x16_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_16x16 = aom_smooth_v_predictor_16x16_ssse3;
    aom_smooth_v_predictor_16x32 = aom_smooth_v_predictor_16x32_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_16x32 = aom_smooth_v_predictor_16x32_ssse3;
    aom_smooth_v_predictor_16x4 = aom_smooth_v_predictor_16x4_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_16x4 = aom_smooth_v_predictor_16x4_ssse3;
    aom_smooth_v_predictor_16x64 = aom_smooth_v_predictor_16x64_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_16x64 = aom_smooth_v_predictor_16x64_ssse3;
    aom_smooth_v_predictor_16x8 = aom_smooth_v_predictor_16x8_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_16x8 = aom_smooth_v_predictor_16x8_ssse3;
    aom_smooth_v_predictor_32x16 = aom_smooth_v_predictor_32x16_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_32x16 = aom_smooth_v_predictor_32x16_ssse3;
    aom_smooth_v_predictor_32x32 = aom_smooth_v_predictor_32x32_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_32x32 = aom_smooth_v_predictor_32x32_ssse3;
    aom_smooth_v_predictor_32x64 = aom_smooth_v_predictor_32x64_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_32x64 = aom_smooth_v_predictor_32x64_ssse3;
    aom_smooth_v_predictor_32x8 = aom_smooth_v_predictor_32x8_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_32x8 = aom_smooth_v_predictor_32x8_ssse3;
    aom_smooth_v_predictor_4x16 = aom_smooth_v_predictor_4x16_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_4x16 = aom_smooth_v_predictor_4x16_ssse3;
    aom_smooth_v_predictor_4x4 = aom_smooth_v_predictor_4x4_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_4x4 = aom_smooth_v_predictor_4x4_ssse3;
    aom_smooth_v_predictor_4x8 = aom_smooth_v_predictor_4x8_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_4x8 = aom_smooth_v_predictor_4x8_ssse3;
    aom_smooth_v_predictor_64x16 = aom_smooth_v_predictor_64x16_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_64x16 = aom_smooth_v_predictor_64x16_ssse3;
    aom_smooth_v_predictor_64x32 = aom_smooth_v_predictor_64x32_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_64x32 = aom_smooth_v_predictor_64x32_ssse3;
    aom_smooth_v_predictor_64x64 = aom_smooth_v_predictor_64x64_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_64x64 = aom_smooth_v_predictor_64x64_ssse3;
    aom_smooth_v_predictor_8x16 = aom_smooth_v_predictor_8x16_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_8x16 = aom_smooth_v_predictor_8x16_ssse3;
    aom_smooth_v_predictor_8x32 = aom_smooth_v_predictor_8x32_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_8x32 = aom_smooth_v_predictor_8x32_ssse3;
    aom_smooth_v_predictor_8x4 = aom_smooth_v_predictor_8x4_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_8x4 = aom_smooth_v_predictor_8x4_ssse3;
    aom_smooth_v_predictor_8x8 = aom_smooth_v_predictor_8x8_c;
    if (flags & HAS_SSSE3) aom_smooth_v_predictor_8x8 = aom_smooth_v_predictor_8x8_ssse3;
    aom_sse = aom_sse_c;
    if (flags & HAS_SSE4_1) aom_sse = aom_sse_sse4_1;
    if (flags & HAS_AVX2) aom_sse = aom_sse_avx2;
    aom_sub_pixel_avg_variance128x128 = aom_sub_pixel_avg_variance128x128_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance128x128 = aom_sub_pixel_avg_variance128x128_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance128x128 = aom_sub_pixel_avg_variance128x128_avx2;
    aom_sub_pixel_avg_variance128x64 = aom_sub_pixel_avg_variance128x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance128x64 = aom_sub_pixel_avg_variance128x64_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance128x64 = aom_sub_pixel_avg_variance128x64_avx2;
    aom_sub_pixel_avg_variance16x16 = aom_sub_pixel_avg_variance16x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance16x16 = aom_sub_pixel_avg_variance16x16_ssse3;
    aom_sub_pixel_avg_variance16x32 = aom_sub_pixel_avg_variance16x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance16x32 = aom_sub_pixel_avg_variance16x32_ssse3;
    aom_sub_pixel_avg_variance16x4 = aom_sub_pixel_avg_variance16x4_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance16x4 = aom_sub_pixel_avg_variance16x4_ssse3;
    aom_sub_pixel_avg_variance16x64 = aom_sub_pixel_avg_variance16x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance16x64 = aom_sub_pixel_avg_variance16x64_ssse3;
    aom_sub_pixel_avg_variance16x8 = aom_sub_pixel_avg_variance16x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance16x8 = aom_sub_pixel_avg_variance16x8_ssse3;
    aom_sub_pixel_avg_variance32x16 = aom_sub_pixel_avg_variance32x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance32x16 = aom_sub_pixel_avg_variance32x16_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance32x16 = aom_sub_pixel_avg_variance32x16_avx2;
    aom_sub_pixel_avg_variance32x32 = aom_sub_pixel_avg_variance32x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance32x32 = aom_sub_pixel_avg_variance32x32_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance32x32 = aom_sub_pixel_avg_variance32x32_avx2;
    aom_sub_pixel_avg_variance32x64 = aom_sub_pixel_avg_variance32x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance32x64 = aom_sub_pixel_avg_variance32x64_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance32x64 = aom_sub_pixel_avg_variance32x64_avx2;
    aom_sub_pixel_avg_variance32x8 = aom_sub_pixel_avg_variance32x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance32x8 = aom_sub_pixel_avg_variance32x8_ssse3;
    aom_sub_pixel_avg_variance4x16 = aom_sub_pixel_avg_variance4x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance4x16 = aom_sub_pixel_avg_variance4x16_ssse3;
    aom_sub_pixel_avg_variance4x4 = aom_sub_pixel_avg_variance4x4_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance4x4 = aom_sub_pixel_avg_variance4x4_ssse3;
    aom_sub_pixel_avg_variance4x8 = aom_sub_pixel_avg_variance4x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance4x8 = aom_sub_pixel_avg_variance4x8_ssse3;
    aom_sub_pixel_avg_variance64x128 = aom_sub_pixel_avg_variance64x128_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance64x128 = aom_sub_pixel_avg_variance64x128_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance64x128 = aom_sub_pixel_avg_variance64x128_avx2;
    aom_sub_pixel_avg_variance64x16 = aom_sub_pixel_avg_variance64x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance64x16 = aom_sub_pixel_avg_variance64x16_ssse3;
    aom_sub_pixel_avg_variance64x32 = aom_sub_pixel_avg_variance64x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance64x32 = aom_sub_pixel_avg_variance64x32_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance64x32 = aom_sub_pixel_avg_variance64x32_avx2;
    aom_sub_pixel_avg_variance64x64 = aom_sub_pixel_avg_variance64x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance64x64 = aom_sub_pixel_avg_variance64x64_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_avg_variance64x64 = aom_sub_pixel_avg_variance64x64_avx2;
    aom_sub_pixel_avg_variance8x16 = aom_sub_pixel_avg_variance8x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance8x16 = aom_sub_pixel_avg_variance8x16_ssse3;
    aom_sub_pixel_avg_variance8x32 = aom_sub_pixel_avg_variance8x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance8x32 = aom_sub_pixel_avg_variance8x32_ssse3;
    aom_sub_pixel_avg_variance8x4 = aom_sub_pixel_avg_variance8x4_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance8x4 = aom_sub_pixel_avg_variance8x4_ssse3;
    aom_sub_pixel_avg_variance8x8 = aom_sub_pixel_avg_variance8x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_avg_variance8x8 = aom_sub_pixel_avg_variance8x8_ssse3;
    aom_sub_pixel_variance128x128 = aom_sub_pixel_variance128x128_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance128x128 = aom_sub_pixel_variance128x128_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance128x128 = aom_sub_pixel_variance128x128_avx2;
    aom_sub_pixel_variance128x64 = aom_sub_pixel_variance128x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance128x64 = aom_sub_pixel_variance128x64_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance128x64 = aom_sub_pixel_variance128x64_avx2;
    aom_sub_pixel_variance16x16 = aom_sub_pixel_variance16x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance16x16 = aom_sub_pixel_variance16x16_ssse3;
    aom_sub_pixel_variance16x32 = aom_sub_pixel_variance16x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance16x32 = aom_sub_pixel_variance16x32_ssse3;
    aom_sub_pixel_variance16x4 = aom_sub_pixel_variance16x4_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance16x4 = aom_sub_pixel_variance16x4_ssse3;
    aom_sub_pixel_variance16x64 = aom_sub_pixel_variance16x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance16x64 = aom_sub_pixel_variance16x64_ssse3;
    aom_sub_pixel_variance16x8 = aom_sub_pixel_variance16x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance16x8 = aom_sub_pixel_variance16x8_ssse3;
    aom_sub_pixel_variance32x16 = aom_sub_pixel_variance32x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance32x16 = aom_sub_pixel_variance32x16_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance32x16 = aom_sub_pixel_variance32x16_avx2;
    aom_sub_pixel_variance32x32 = aom_sub_pixel_variance32x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance32x32 = aom_sub_pixel_variance32x32_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance32x32 = aom_sub_pixel_variance32x32_avx2;
    aom_sub_pixel_variance32x64 = aom_sub_pixel_variance32x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance32x64 = aom_sub_pixel_variance32x64_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance32x64 = aom_sub_pixel_variance32x64_avx2;
    aom_sub_pixel_variance32x8 = aom_sub_pixel_variance32x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance32x8 = aom_sub_pixel_variance32x8_ssse3;
    aom_sub_pixel_variance4x16 = aom_sub_pixel_variance4x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance4x16 = aom_sub_pixel_variance4x16_ssse3;
    aom_sub_pixel_variance4x4 = aom_sub_pixel_variance4x4_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance4x4 = aom_sub_pixel_variance4x4_ssse3;
    aom_sub_pixel_variance4x8 = aom_sub_pixel_variance4x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance4x8 = aom_sub_pixel_variance4x8_ssse3;
    aom_sub_pixel_variance64x128 = aom_sub_pixel_variance64x128_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance64x128 = aom_sub_pixel_variance64x128_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance64x128 = aom_sub_pixel_variance64x128_avx2;
    aom_sub_pixel_variance64x16 = aom_sub_pixel_variance64x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance64x16 = aom_sub_pixel_variance64x16_ssse3;
    aom_sub_pixel_variance64x32 = aom_sub_pixel_variance64x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance64x32 = aom_sub_pixel_variance64x32_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance64x32 = aom_sub_pixel_variance64x32_avx2;
    aom_sub_pixel_variance64x64 = aom_sub_pixel_variance64x64_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance64x64 = aom_sub_pixel_variance64x64_ssse3;
    if (flags & HAS_AVX2) aom_sub_pixel_variance64x64 = aom_sub_pixel_variance64x64_avx2;
    aom_sub_pixel_variance8x16 = aom_sub_pixel_variance8x16_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance8x16 = aom_sub_pixel_variance8x16_ssse3;
    aom_sub_pixel_variance8x32 = aom_sub_pixel_variance8x32_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance8x32 = aom_sub_pixel_variance8x32_ssse3;
    aom_sub_pixel_variance8x4 = aom_sub_pixel_variance8x4_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance8x4 = aom_sub_pixel_variance8x4_ssse3;
    aom_sub_pixel_variance8x8 = aom_sub_pixel_variance8x8_sse2;
    if (flags & HAS_SSSE3) aom_sub_pixel_variance8x8 = aom_sub_pixel_variance8x8_ssse3;
    aom_subtract_block = aom_subtract_block_sse2;
    if (flags & HAS_AVX2) aom_subtract_block = aom_subtract_block_avx2;
    aom_sum_squares_2d_i16 = aom_sum_squares_2d_i16_sse2;
    if (flags & HAS_AVX2) aom_sum_squares_2d_i16 = aom_sum_squares_2d_i16_avx2;
    aom_v_predictor_32x16 = aom_v_predictor_32x16_sse2;
    if (flags & HAS_AVX2) aom_v_predictor_32x16 = aom_v_predictor_32x16_avx2;
    aom_v_predictor_32x32 = aom_v_predictor_32x32_sse2;
    if (flags & HAS_AVX2) aom_v_predictor_32x32 = aom_v_predictor_32x32_avx2;
    aom_v_predictor_32x64 = aom_v_predictor_32x64_sse2;
    if (flags & HAS_AVX2) aom_v_predictor_32x64 = aom_v_predictor_32x64_avx2;
    aom_v_predictor_64x16 = aom_v_predictor_64x16_sse2;
    if (flags & HAS_AVX2) aom_v_predictor_64x16 = aom_v_predictor_64x16_avx2;
    aom_v_predictor_64x32 = aom_v_predictor_64x32_sse2;
    if (flags & HAS_AVX2) aom_v_predictor_64x32 = aom_v_predictor_64x32_avx2;
    aom_v_predictor_64x64 = aom_v_predictor_64x64_sse2;
    if (flags & HAS_AVX2) aom_v_predictor_64x64 = aom_v_predictor_64x64_avx2;
    aom_variance128x128 = aom_variance128x128_sse2;
    if (flags & HAS_AVX2) aom_variance128x128 = aom_variance128x128_avx2;
    aom_variance128x64 = aom_variance128x64_sse2;
    if (flags & HAS_AVX2) aom_variance128x64 = aom_variance128x64_avx2;
    aom_variance16x16 = aom_variance16x16_sse2;
    if (flags & HAS_AVX2) aom_variance16x16 = aom_variance16x16_avx2;
    aom_variance16x32 = aom_variance16x32_sse2;
    if (flags & HAS_AVX2) aom_variance16x32 = aom_variance16x32_avx2;
    aom_variance16x4 = aom_variance16x4_sse2;
    if (flags & HAS_AVX2) aom_variance16x4 = aom_variance16x4_avx2;
    aom_variance16x64 = aom_variance16x64_sse2;
    if (flags & HAS_AVX2) aom_variance16x64 = aom_variance16x64_avx2;
    aom_variance16x8 = aom_variance16x8_sse2;
    if (flags & HAS_AVX2) aom_variance16x8 = aom_variance16x8_avx2;
    aom_variance32x16 = aom_variance32x16_sse2;
    if (flags & HAS_AVX2) aom_variance32x16 = aom_variance32x16_avx2;
    aom_variance32x32 = aom_variance32x32_sse2;
    if (flags & HAS_AVX2) aom_variance32x32 = aom_variance32x32_avx2;
    aom_variance32x64 = aom_variance32x64_sse2;
    if (flags & HAS_AVX2) aom_variance32x64 = aom_variance32x64_avx2;
    aom_variance32x8 = aom_variance32x8_sse2;
    if (flags & HAS_AVX2) aom_variance32x8 = aom_variance32x8_avx2;
    aom_variance64x128 = aom_variance64x128_sse2;
    if (flags & HAS_AVX2) aom_variance64x128 = aom_variance64x128_avx2;
    aom_variance64x16 = aom_variance64x16_sse2;
    if (flags & HAS_AVX2) aom_variance64x16 = aom_variance64x16_avx2;
    aom_variance64x32 = aom_variance64x32_sse2;
    if (flags & HAS_AVX2) aom_variance64x32 = aom_variance64x32_avx2;
    aom_variance64x64 = aom_variance64x64_sse2;
    if (flags & HAS_AVX2) aom_variance64x64 = aom_variance64x64_avx2;
    av1_round_shift_array = av1_round_shift_array_c;
    if (flags & HAS_SSE4_1) av1_round_shift_array = av1_round_shift_array_sse4_1;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif

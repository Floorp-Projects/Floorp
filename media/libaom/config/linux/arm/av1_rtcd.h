#ifndef AOM_RTCD_H_
#define AOM_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * AV1
 */

#include "aom/aom_integer.h"
#include "aom_dsp/txfm_common.h"
#include "av1/common/common.h"
#include "av1/common/enums.h"
#include "av1/common/quant_common.h"
#include "av1/common/filter.h"
#include "av1/common/convolve.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/odintrin.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct txfm_param;
struct aom_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;

#ifdef __cplusplus
extern "C" {
#endif

void apply_selfguided_restoration_c(uint8_t *dat, int width, int height, int stride, int eps, int *xqd, uint8_t *dst, int dst_stride, int32_t *tmpbuf);
#define apply_selfguided_restoration apply_selfguided_restoration_c

void apply_selfguided_restoration_highbd_c(uint16_t *dat, int width, int height, int stride, int bit_depth, int eps, int *xqd, uint16_t *dst, int dst_stride, int32_t *tmpbuf);
#define apply_selfguided_restoration_highbd apply_selfguided_restoration_highbd_c

int64_t av1_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz);
#define av1_block_error av1_block_error_c

void av1_convolve_2d_c(const uint8_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params);
#define av1_convolve_2d av1_convolve_2d_c

void av1_convolve_2d_scale_c(const uint8_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_qn, const int x_step_qn, const int subpel_y_q4, const int y_step_qn, ConvolveParams *conv_params);
#define av1_convolve_2d_scale av1_convolve_2d_scale_c

void av1_convolve_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params);
#define av1_convolve_horiz av1_convolve_horiz_c

void av1_convolve_rounding_c(const int32_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, int bits);
#define av1_convolve_rounding av1_convolve_rounding_c

void av1_convolve_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params);
#define av1_convolve_vert av1_convolve_vert_c

int av1_diamond_search_sad_c(struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv);
#define av1_diamond_search_sad av1_diamond_search_sad_c

void av1_fht16x16_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht16x16 av1_fht16x16_c

void av1_fht16x32_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht16x32 av1_fht16x32_c

void av1_fht16x4_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht16x4 av1_fht16x4_c

void av1_fht16x8_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht16x8 av1_fht16x8_c

void av1_fht32x16_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht32x16 av1_fht32x16_c

void av1_fht32x32_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht32x32 av1_fht32x32_c

void av1_fht32x8_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht32x8 av1_fht32x8_c

void av1_fht4x16_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht4x16 av1_fht4x16_c

void av1_fht4x4_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht4x4 av1_fht4x4_c

void av1_fht4x8_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht4x8 av1_fht4x8_c

void av1_fht8x16_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht8x16 av1_fht8x16_c

void av1_fht8x32_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht8x32 av1_fht8x32_c

void av1_fht8x4_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht8x4 av1_fht8x4_c

void av1_fht8x8_c(const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param);
#define av1_fht8x8 av1_fht8x8_c

void av1_filter_intra_edge_c(uint8_t *p, int sz, int strength);
#define av1_filter_intra_edge av1_filter_intra_edge_c

void av1_filter_intra_edge_high_c(uint16_t *p, int sz, int strength);
#define av1_filter_intra_edge_high av1_filter_intra_edge_high_c

int av1_full_range_search_c(const struct macroblock *x, const struct search_site_config *cfg, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv);
#define av1_full_range_search av1_full_range_search_c

int av1_full_search_sad_c(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
#define av1_full_search_sad av1_full_search_sad_c

void av1_fwd_idtx_c(const int16_t *src_diff, tran_low_t *coeff, int stride, int bsx, int bsy, TX_TYPE tx_type);
#define av1_fwd_idtx av1_fwd_idtx_c

void av1_fwd_txfm2d_16x16_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_16x16 av1_fwd_txfm2d_16x16_c

void av1_fwd_txfm2d_16x32_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_16x32 av1_fwd_txfm2d_16x32_c

void av1_fwd_txfm2d_16x8_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_16x8 av1_fwd_txfm2d_16x8_c

void av1_fwd_txfm2d_32x16_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_32x16 av1_fwd_txfm2d_32x16_c

void av1_fwd_txfm2d_32x32_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_32x32 av1_fwd_txfm2d_32x32_c

void av1_fwd_txfm2d_4x4_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_4x4 av1_fwd_txfm2d_4x4_c

void av1_fwd_txfm2d_4x8_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_4x8 av1_fwd_txfm2d_4x8_c

void av1_fwd_txfm2d_8x16_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_8x16 av1_fwd_txfm2d_8x16_c

void av1_fwd_txfm2d_8x4_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_8x4 av1_fwd_txfm2d_8x4_c

void av1_fwd_txfm2d_8x8_c(const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_fwd_txfm2d_8x8 av1_fwd_txfm2d_8x8_c

void av1_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride);
#define av1_fwht4x4 av1_fwht4x4_c

int64_t av1_highbd_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz, int bd);
#define av1_highbd_block_error av1_highbd_block_error_c

void av1_highbd_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve8 av1_highbd_convolve8_c

void av1_highbd_convolve8_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve8_avg av1_highbd_convolve8_avg_c

void av1_highbd_convolve8_avg_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve8_avg_horiz av1_highbd_convolve8_avg_horiz_c

void av1_highbd_convolve8_avg_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve8_avg_vert av1_highbd_convolve8_avg_vert_c

void av1_highbd_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve8_horiz av1_highbd_convolve8_horiz_c

void av1_highbd_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve8_vert av1_highbd_convolve8_vert_c

void av1_highbd_convolve_2d_c(const uint16_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params, int bd);
#define av1_highbd_convolve_2d av1_highbd_convolve_2d_c

void av1_highbd_convolve_2d_scale_c(const uint16_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int x_step_qn, const int subpel_y_q4, const int y_step_qn, ConvolveParams *conv_params, int bd);
#define av1_highbd_convolve_2d_scale av1_highbd_convolve_2d_scale_c

void av1_highbd_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve_avg av1_highbd_convolve_avg_c

void av1_highbd_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve_copy av1_highbd_convolve_copy_c

void av1_highbd_convolve_horiz_c(const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd);
#define av1_highbd_convolve_horiz av1_highbd_convolve_horiz_c

void av1_highbd_convolve_init_c(void);
#define av1_highbd_convolve_init av1_highbd_convolve_init_c

void av1_highbd_convolve_rounding_c(const int32_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, int bits, int bd);
#define av1_highbd_convolve_rounding av1_highbd_convolve_rounding_c

void av1_highbd_convolve_vert_c(const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd);
#define av1_highbd_convolve_vert av1_highbd_convolve_vert_c

void av1_highbd_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride);
#define av1_highbd_fwht4x4 av1_highbd_fwht4x4_c

void av1_highbd_iht16x16_256_add_c(const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param);
#define av1_highbd_iht16x16_256_add av1_highbd_iht16x16_256_add_c

void av1_highbd_iht16x32_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht16x32_512_add av1_highbd_iht16x32_512_add_c

void av1_highbd_iht16x4_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht16x4_64_add av1_highbd_iht16x4_64_add_c

void av1_highbd_iht16x8_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht16x8_128_add av1_highbd_iht16x8_128_add_c

void av1_highbd_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht32x16_512_add av1_highbd_iht32x16_512_add_c

void av1_highbd_iht32x8_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht32x8_256_add av1_highbd_iht32x8_256_add_c

void av1_highbd_iht4x16_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht4x16_64_add av1_highbd_iht4x16_64_add_c

void av1_highbd_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht4x4_16_add av1_highbd_iht4x4_16_add_c

void av1_highbd_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht4x8_32_add av1_highbd_iht4x8_32_add_c

void av1_highbd_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht8x16_128_add av1_highbd_iht8x16_128_add_c

void av1_highbd_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht8x32_256_add av1_highbd_iht8x32_256_add_c

void av1_highbd_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht8x4_32_add av1_highbd_iht8x4_32_add_c

void av1_highbd_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_highbd_iht8x8_64_add av1_highbd_iht8x8_64_add_c

void av1_highbd_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale);
#define av1_highbd_quantize_fp av1_highbd_quantize_fp_c

void av1_highbd_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define av1_highbd_temporal_filter_apply av1_highbd_temporal_filter_apply_c

void av1_highbd_warp_affine_c(const int32_t *mat, const uint16_t *ref, int width, int height, int stride, uint16_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, int bd, ConvolveParams *conv_params, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta);
#define av1_highbd_warp_affine av1_highbd_warp_affine_c

void av1_highpass_filter_c(uint8_t *dgd, int width, int height, int stride, int32_t *dst, int dst_stride, int r, int eps);
#define av1_highpass_filter av1_highpass_filter_c

void av1_highpass_filter_highbd_c(uint16_t *dgd, int width, int height, int stride, int32_t *dst, int dst_stride, int r, int eps);
#define av1_highpass_filter_highbd av1_highpass_filter_highbd_c

void av1_iht16x16_256_add_c(const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param);
#define av1_iht16x16_256_add av1_iht16x16_256_add_c

void av1_iht16x32_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht16x32_512_add av1_iht16x32_512_add_c

void av1_iht16x4_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht16x4_64_add av1_iht16x4_64_add_c

void av1_iht16x8_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht16x8_128_add av1_iht16x8_128_add_c

void av1_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht32x16_512_add av1_iht32x16_512_add_c

void av1_iht32x32_1024_add_c(const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param);
#define av1_iht32x32_1024_add av1_iht32x32_1024_add_c

void av1_iht32x8_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht32x8_256_add av1_iht32x8_256_add_c

void av1_iht4x16_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht4x16_64_add av1_iht4x16_64_add_c

void av1_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht4x4_16_add av1_iht4x4_16_add_c

void av1_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht4x8_32_add av1_iht4x8_32_add_c

void av1_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht8x16_128_add av1_iht8x16_128_add_c

void av1_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht8x32_256_add av1_iht8x32_256_add_c

void av1_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht8x4_32_add av1_iht8x4_32_add_c

void av1_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param);
#define av1_iht8x8_64_add av1_iht8x8_64_add_c

void av1_inv_txfm2d_add_16x16_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_16x16 av1_inv_txfm2d_add_16x16_c

void av1_inv_txfm2d_add_16x32_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_16x32 av1_inv_txfm2d_add_16x32_c

void av1_inv_txfm2d_add_16x8_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_16x8 av1_inv_txfm2d_add_16x8_c

void av1_inv_txfm2d_add_32x16_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_32x16 av1_inv_txfm2d_add_32x16_c

void av1_inv_txfm2d_add_32x32_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_32x32 av1_inv_txfm2d_add_32x32_c

void av1_inv_txfm2d_add_4x4_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_4x4 av1_inv_txfm2d_add_4x4_c

void av1_inv_txfm2d_add_4x8_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_4x8 av1_inv_txfm2d_add_4x8_c

void av1_inv_txfm2d_add_8x16_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_8x16 av1_inv_txfm2d_add_8x16_c

void av1_inv_txfm2d_add_8x4_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_8x4 av1_inv_txfm2d_add_8x4_c

void av1_inv_txfm2d_add_8x8_c(const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd);
#define av1_inv_txfm2d_add_8x8 av1_inv_txfm2d_add_8x8_c

void av1_lowbd_convolve_init_c(void);
#define av1_lowbd_convolve_init av1_lowbd_convolve_init_c

void av1_quantize_b_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr, int log_scale);
#define av1_quantize_b av1_quantize_b_c

void av1_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define av1_quantize_fp av1_quantize_fp_c

void av1_quantize_fp_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define av1_quantize_fp_32x32 av1_quantize_fp_32x32_c

void av1_selfguided_restoration_c(uint8_t *dgd, int width, int height, int stride, int32_t *dst, int dst_stride, int r, int eps);
#define av1_selfguided_restoration av1_selfguided_restoration_c

void av1_selfguided_restoration_highbd_c(uint16_t *dgd, int width, int height, int stride, int32_t *dst, int dst_stride, int bit_depth, int r, int eps);
#define av1_selfguided_restoration_highbd av1_selfguided_restoration_highbd_c

void av1_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define av1_temporal_filter_apply av1_temporal_filter_apply_c

void av1_upsample_intra_edge_c(uint8_t *p, int sz);
#define av1_upsample_intra_edge av1_upsample_intra_edge_c

void av1_upsample_intra_edge_high_c(uint16_t *p, int sz, int bd);
#define av1_upsample_intra_edge_high av1_upsample_intra_edge_high_c

void av1_warp_affine_c(const int32_t *mat, const uint8_t *ref, int width, int height, int stride, uint8_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, ConvolveParams *conv_params, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta);
#define av1_warp_affine av1_warp_affine_c

void av1_wedge_compute_delta_squares_c(int16_t *d, const int16_t *a, const int16_t *b, int N);
#define av1_wedge_compute_delta_squares av1_wedge_compute_delta_squares_c

int av1_wedge_sign_from_residuals_c(const int16_t *ds, const uint8_t *m, int N, int64_t limit);
#define av1_wedge_sign_from_residuals av1_wedge_sign_from_residuals_c

uint64_t av1_wedge_sse_from_residuals_c(const int16_t *r1, const int16_t *d, const uint8_t *m, int N);
#define av1_wedge_sse_from_residuals av1_wedge_sse_from_residuals_c

void cdef_filter_block_c(uint8_t *dst8, uint16_t *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int bsize, int max);
void cdef_filter_block_neon(uint8_t *dst8, uint16_t *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int bsize, int max);
RTCD_EXTERN void (*cdef_filter_block)(uint8_t *dst8, uint16_t *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int bsize, int max);

int cdef_find_dir_c(const uint16_t *img, int stride, int32_t *var, int coeff_shift);
int cdef_find_dir_neon(const uint16_t *img, int stride, int32_t *var, int coeff_shift);
RTCD_EXTERN int (*cdef_find_dir)(const uint16_t *img, int stride, int32_t *var, int coeff_shift);

double compute_cross_correlation_c(unsigned char *im1, int stride1, int x1, int y1, unsigned char *im2, int stride2, int x2, int y2);
#define compute_cross_correlation compute_cross_correlation_c

void copy_rect8_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h);
void copy_rect8_16bit_to_16bit_neon(uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h);
RTCD_EXTERN void (*copy_rect8_16bit_to_16bit)(uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h);

void copy_rect8_8bit_to_16bit_c(uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h);
void copy_rect8_8bit_to_16bit_neon(uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h);
RTCD_EXTERN void (*copy_rect8_8bit_to_16bit)(uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h);

void aom_rtcd(void);

#include "aom_config.h"

#ifdef RTCD_C
#include "aom_ports/arm.h"
static void setup_rtcd_internal(void)
{
    int flags = arm_cpu_caps();

    (void)flags;

    cdef_filter_block = cdef_filter_block_c;
    if (flags & HAS_NEON) cdef_filter_block = cdef_filter_block_neon;
    cdef_find_dir = cdef_find_dir_c;
    if (flags & HAS_NEON) cdef_find_dir = cdef_find_dir_neon;
    copy_rect8_16bit_to_16bit = copy_rect8_16bit_to_16bit_c;
    if (flags & HAS_NEON) copy_rect8_16bit_to_16bit = copy_rect8_16bit_to_16bit_neon;
    copy_rect8_8bit_to_16bit = copy_rect8_8bit_to_16bit_c;
    if (flags & HAS_NEON) copy_rect8_8bit_to_16bit = copy_rect8_8bit_to_16bit_neon;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif

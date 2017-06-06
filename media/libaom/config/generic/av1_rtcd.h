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
struct aom_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;
typedef uint16_t od_dering_in;

#ifdef __cplusplus
extern "C" {
#endif

void aom_clpf_block_c(uint8_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
#define aom_clpf_block aom_clpf_block_c

void aom_clpf_block_hbd_c(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
#define aom_clpf_block_hbd aom_clpf_block_hbd_c

void aom_clpf_hblock_c(uint8_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
#define aom_clpf_hblock aom_clpf_hblock_c

void aom_clpf_hblock_hbd_c(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
#define aom_clpf_hblock_hbd aom_clpf_hblock_hbd_c

int64_t av1_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz);
#define av1_block_error av1_block_error_c

void av1_convolve_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params);
#define av1_convolve_horiz av1_convolve_horiz_c

void av1_convolve_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params);
#define av1_convolve_vert av1_convolve_vert_c

int av1_diamond_search_sad_c(struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv);
#define av1_diamond_search_sad av1_diamond_search_sad_c

void av1_fdct8x8_quant_c(const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define av1_fdct8x8_quant av1_fdct8x8_quant_c

void av1_fht16x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht16x16 av1_fht16x16_c

void av1_fht16x32_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht16x32 av1_fht16x32_c

void av1_fht16x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht16x4 av1_fht16x4_c

void av1_fht16x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht16x8 av1_fht16x8_c

void av1_fht32x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht32x16 av1_fht32x16_c

void av1_fht32x32_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht32x32 av1_fht32x32_c

void av1_fht32x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht32x8 av1_fht32x8_c

void av1_fht4x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht4x16 av1_fht4x16_c

void av1_fht4x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht4x4 av1_fht4x4_c

void av1_fht4x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht4x8 av1_fht4x8_c

void av1_fht8x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht8x16 av1_fht8x16_c

void av1_fht8x32_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht8x32 av1_fht8x32_c

void av1_fht8x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht8x4 av1_fht8x4_c

void av1_fht8x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_fht8x8 av1_fht8x8_c

int av1_full_range_search_c(const struct macroblock *x, const struct search_site_config *cfg, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv);
#define av1_full_range_search av1_full_range_search_c

int av1_full_search_sad_c(const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv);
#define av1_full_search_sad av1_full_search_sad_c

void av1_fwd_idtx_c(const int16_t *src_diff, tran_low_t *coeff, int stride, int bs, int tx_type);
#define av1_fwd_idtx av1_fwd_idtx_c

void av1_fwd_txfm2d_16x16_c(const int16_t *input, int32_t *output, int stride, int tx_type, int bd);
#define av1_fwd_txfm2d_16x16 av1_fwd_txfm2d_16x16_c

void av1_fwd_txfm2d_32x32_c(const int16_t *input, int32_t *output, int stride, int tx_type, int bd);
#define av1_fwd_txfm2d_32x32 av1_fwd_txfm2d_32x32_c

void av1_fwd_txfm2d_4x4_c(const int16_t *input, int32_t *output, int stride, int tx_type, int bd);
#define av1_fwd_txfm2d_4x4 av1_fwd_txfm2d_4x4_c

void av1_fwd_txfm2d_64x64_c(const int16_t *input, int32_t *output, int stride, int tx_type, int bd);
#define av1_fwd_txfm2d_64x64 av1_fwd_txfm2d_64x64_c

void av1_fwd_txfm2d_8x8_c(const int16_t *input, int32_t *output, int stride, int tx_type, int bd);
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

void av1_highbd_convolve_avg_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve_avg av1_highbd_convolve_avg_c

void av1_highbd_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps);
#define av1_highbd_convolve_copy av1_highbd_convolve_copy_c

void av1_highbd_convolve_horiz_c(const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd);
#define av1_highbd_convolve_horiz av1_highbd_convolve_horiz_c

void av1_highbd_convolve_init_c(void);
#define av1_highbd_convolve_init av1_highbd_convolve_init_c

void av1_highbd_convolve_vert_c(const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd);
#define av1_highbd_convolve_vert av1_highbd_convolve_vert_c

void av1_highbd_fht16x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht16x16 av1_highbd_fht16x16_c

void av1_highbd_fht16x32_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht16x32 av1_highbd_fht16x32_c

void av1_highbd_fht16x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht16x4 av1_highbd_fht16x4_c

void av1_highbd_fht16x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht16x8 av1_highbd_fht16x8_c

void av1_highbd_fht32x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht32x16 av1_highbd_fht32x16_c

void av1_highbd_fht32x32_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht32x32 av1_highbd_fht32x32_c

void av1_highbd_fht32x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht32x8 av1_highbd_fht32x8_c

void av1_highbd_fht4x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht4x16 av1_highbd_fht4x16_c

void av1_highbd_fht4x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht4x4 av1_highbd_fht4x4_c

void av1_highbd_fht4x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht4x8 av1_highbd_fht4x8_c

void av1_highbd_fht8x16_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht8x16 av1_highbd_fht8x16_c

void av1_highbd_fht8x32_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht8x32 av1_highbd_fht8x32_c

void av1_highbd_fht8x4_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht8x4 av1_highbd_fht8x4_c

void av1_highbd_fht8x8_c(const int16_t *input, tran_low_t *output, int stride, int tx_type);
#define av1_highbd_fht8x8 av1_highbd_fht8x8_c

void av1_highbd_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride);
#define av1_highbd_fwht4x4 av1_highbd_fwht4x4_c

void av1_highbd_iht16x16_256_add_c(const tran_low_t *input, uint8_t *output, int pitch, int tx_type, int bd);
#define av1_highbd_iht16x16_256_add av1_highbd_iht16x16_256_add_c

void av1_highbd_iht16x32_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht16x32_512_add av1_highbd_iht16x32_512_add_c

void av1_highbd_iht16x4_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht16x4_64_add av1_highbd_iht16x4_64_add_c

void av1_highbd_iht16x8_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht16x8_128_add av1_highbd_iht16x8_128_add_c

void av1_highbd_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht32x16_512_add av1_highbd_iht32x16_512_add_c

void av1_highbd_iht32x8_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht32x8_256_add av1_highbd_iht32x8_256_add_c

void av1_highbd_iht4x16_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht4x16_64_add av1_highbd_iht4x16_64_add_c

void av1_highbd_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht4x4_16_add av1_highbd_iht4x4_16_add_c

void av1_highbd_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht4x8_32_add av1_highbd_iht4x8_32_add_c

void av1_highbd_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht8x16_128_add av1_highbd_iht8x16_128_add_c

void av1_highbd_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht8x32_256_add av1_highbd_iht8x32_256_add_c

void av1_highbd_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht8x4_32_add av1_highbd_iht8x4_32_add_c

void av1_highbd_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd);
#define av1_highbd_iht8x8_64_add av1_highbd_iht8x8_64_add_c

void av1_highbd_quantize_b_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale);
#define av1_highbd_quantize_b av1_highbd_quantize_b_c

void av1_highbd_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale);
#define av1_highbd_quantize_fp av1_highbd_quantize_fp_c

void av1_highbd_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define av1_highbd_temporal_filter_apply av1_highbd_temporal_filter_apply_c

void av1_highbd_warp_affine_c(const int32_t *mat, const uint16_t *ref, int width, int height, int stride, uint16_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, int bd, int comp_avg, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta);
#define av1_highbd_warp_affine av1_highbd_warp_affine_c

void av1_iht16x16_256_add_c(const tran_low_t *input, uint8_t *output, int pitch, int tx_type);
#define av1_iht16x16_256_add av1_iht16x16_256_add_c

void av1_iht16x32_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht16x32_512_add av1_iht16x32_512_add_c

void av1_iht16x4_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht16x4_64_add av1_iht16x4_64_add_c

void av1_iht16x8_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht16x8_128_add av1_iht16x8_128_add_c

void av1_iht32x16_512_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht32x16_512_add av1_iht32x16_512_add_c

void av1_iht32x32_1024_add_c(const tran_low_t *input, uint8_t *output, int pitch, int tx_type);
#define av1_iht32x32_1024_add av1_iht32x32_1024_add_c

void av1_iht32x8_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht32x8_256_add av1_iht32x8_256_add_c

void av1_iht4x16_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht4x16_64_add av1_iht4x16_64_add_c

void av1_iht4x4_16_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht4x4_16_add av1_iht4x4_16_add_c

void av1_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht4x8_32_add av1_iht4x8_32_add_c

void av1_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht8x16_128_add av1_iht8x16_128_add_c

void av1_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht8x32_256_add av1_iht8x32_256_add_c

void av1_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht8x4_32_add av1_iht8x4_32_add_c

void av1_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht8x8_64_add av1_iht8x8_64_add_c

void av1_inv_txfm2d_add_16x16_c(const int32_t *input, uint16_t *output, int stride, int tx_type, int bd);
#define av1_inv_txfm2d_add_16x16 av1_inv_txfm2d_add_16x16_c

void av1_inv_txfm2d_add_32x32_c(const int32_t *input, uint16_t *output, int stride, int tx_type, int bd);
#define av1_inv_txfm2d_add_32x32 av1_inv_txfm2d_add_32x32_c

void av1_inv_txfm2d_add_4x4_c(const int32_t *input, uint16_t *output, int stride, int tx_type, int bd);
#define av1_inv_txfm2d_add_4x4 av1_inv_txfm2d_add_4x4_c

void av1_inv_txfm2d_add_64x64_c(const int32_t *input, uint16_t *output, int stride, int tx_type, int bd);
#define av1_inv_txfm2d_add_64x64 av1_inv_txfm2d_add_64x64_c

void av1_inv_txfm2d_add_8x8_c(const int32_t *input, uint16_t *output, int stride, int tx_type, int bd);
#define av1_inv_txfm2d_add_8x8 av1_inv_txfm2d_add_8x8_c

void av1_lowbd_convolve_init_c(void);
#define av1_lowbd_convolve_init av1_lowbd_convolve_init_c

void av1_quantize_b_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale);
#define av1_quantize_b av1_quantize_b_c

void av1_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define av1_quantize_fp av1_quantize_fp_c

void av1_quantize_fp_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define av1_quantize_fp_32x32 av1_quantize_fp_32x32_c

void av1_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define av1_temporal_filter_apply av1_temporal_filter_apply_c

void av1_warp_affine_c(const int32_t *mat, const uint8_t *ref, int width, int height, int stride, uint8_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, int comp_avg, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta);
#define av1_warp_affine av1_warp_affine_c

void av1_wedge_compute_delta_squares_c(int16_t *d, const int16_t *a, const int16_t *b, int N);
#define av1_wedge_compute_delta_squares av1_wedge_compute_delta_squares_c

int av1_wedge_sign_from_residuals_c(const int16_t *ds, const uint8_t *m, int N, int64_t limit);
#define av1_wedge_sign_from_residuals av1_wedge_sign_from_residuals_c

uint64_t av1_wedge_sse_from_residuals_c(const int16_t *r1, const int16_t *d, const uint8_t *m, int N);
#define av1_wedge_sse_from_residuals av1_wedge_sse_from_residuals_c

double compute_cross_correlation_c(unsigned char *im1, int stride1, int x1, int y1, unsigned char *im2, int stride2, int x2, int y2);
#define compute_cross_correlation compute_cross_correlation_c

void copy_4x4_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src, int sstride);
#define copy_4x4_16bit_to_16bit copy_4x4_16bit_to_16bit_c

void copy_4x4_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src, int sstride);
#define copy_4x4_16bit_to_8bit copy_4x4_16bit_to_8bit_c

void copy_8x8_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src, int sstride);
#define copy_8x8_16bit_to_16bit copy_8x8_16bit_to_16bit_c

void copy_8x8_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src, int sstride);
#define copy_8x8_16bit_to_8bit copy_8x8_16bit_to_8bit_c

void copy_rect8_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h);
#define copy_rect8_16bit_to_16bit copy_rect8_16bit_to_16bit_c

void copy_rect8_8bit_to_16bit_c(uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h);
#define copy_rect8_8bit_to_16bit copy_rect8_8bit_to_16bit_c

int od_dir_find8_c(const od_dering_in *img, int stride, int32_t *var, int coeff_shift);
#define od_dir_find8 od_dir_find8_c

void od_filter_dering_direction_4x4_c(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);
#define od_filter_dering_direction_4x4 od_filter_dering_direction_4x4_c

void od_filter_dering_direction_8x8_c(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);
#define od_filter_dering_direction_8x8 od_filter_dering_direction_8x8_c

void aom_rtcd(void);

#include "aom_config.h"

#ifdef RTCD_C
static void setup_rtcd_internal(void)
{
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif

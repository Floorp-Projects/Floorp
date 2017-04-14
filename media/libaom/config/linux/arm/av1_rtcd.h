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
void aom_clpf_block_neon(uint8_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
RTCD_EXTERN void (*aom_clpf_block)(uint8_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);

void aom_clpf_block_hbd_c(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
void aom_clpf_block_hbd_neon(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
RTCD_EXTERN void (*aom_clpf_block_hbd)(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);

void aom_clpf_hblock_c(uint8_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
void aom_clpf_hblock_neon(uint8_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
RTCD_EXTERN void (*aom_clpf_hblock)(uint8_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);

void aom_clpf_hblock_hbd_c(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
void aom_clpf_hblock_hbd_neon(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);
RTCD_EXTERN void (*aom_clpf_hblock_hbd)(uint16_t *dst, const uint16_t *src, int dstride, int sstride, int sizex, int sizey, unsigned int strength, unsigned int bd);

int64_t av1_block_error_c(const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz);
#define av1_block_error av1_block_error_c

int64_t av1_block_error_fp_c(const int16_t *coeff, const int16_t *dqcoeff, int block_size);
int64_t av1_block_error_fp_neon(const int16_t *coeff, const int16_t *dqcoeff, int block_size);
RTCD_EXTERN int64_t (*av1_block_error_fp)(const int16_t *coeff, const int16_t *dqcoeff, int block_size);

void av1_convolve_horiz_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params);
#define av1_convolve_horiz av1_convolve_horiz_c

void av1_convolve_vert_c(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params);
#define av1_convolve_vert av1_convolve_vert_c

int av1_diamond_search_sad_c(struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv);
#define av1_diamond_search_sad av1_diamond_search_sad_c

void av1_fdct8x8_quant_c(const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void av1_fdct8x8_quant_neon(const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
RTCD_EXTERN void (*av1_fdct8x8_quant)(const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);

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

void av1_fwht4x4_c(const int16_t *input, tran_low_t *output, int stride);
#define av1_fwht4x4 av1_fwht4x4_c

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
void av1_iht4x4_16_add_neon(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
RTCD_EXTERN void (*av1_iht4x4_16_add)(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);

void av1_iht4x8_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht4x8_32_add av1_iht4x8_32_add_c

void av1_iht8x16_128_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht8x16_128_add av1_iht8x16_128_add_c

void av1_iht8x32_256_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht8x32_256_add av1_iht8x32_256_add_c

void av1_iht8x4_32_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
#define av1_iht8x4_32_add av1_iht8x4_32_add_c

void av1_iht8x8_64_add_c(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
void av1_iht8x8_64_add_neon(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);
RTCD_EXTERN void (*av1_iht8x8_64_add)(const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type);

void av1_lowbd_convolve_init_c(void);
#define av1_lowbd_convolve_init av1_lowbd_convolve_init_c

void av1_quantize_b_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale);
#define av1_quantize_b av1_quantize_b_c

void av1_quantize_fp_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
void av1_quantize_fp_neon(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
RTCD_EXTERN void (*av1_quantize_fp)(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);

void av1_quantize_fp_32x32_c(const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan);
#define av1_quantize_fp_32x32 av1_quantize_fp_32x32_c

void av1_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count);
#define av1_temporal_filter_apply av1_temporal_filter_apply_c

void av1_warp_affine_c(int32_t *mat, uint8_t *ref, int width, int height, int stride, uint8_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, int ref_frm, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta);
#define av1_warp_affine av1_warp_affine_c

void copy_4x4_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src, int sstride);
void copy_4x4_16bit_to_16bit_neon(uint16_t *dst, int dstride, const uint16_t *src, int sstride);
RTCD_EXTERN void (*copy_4x4_16bit_to_16bit)(uint16_t *dst, int dstride, const uint16_t *src, int sstride);

void copy_4x4_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src, int sstride);
void copy_4x4_16bit_to_8bit_neon(uint8_t *dst, int dstride, const uint16_t *src, int sstride);
RTCD_EXTERN void (*copy_4x4_16bit_to_8bit)(uint8_t *dst, int dstride, const uint16_t *src, int sstride);

void copy_8x8_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src, int sstride);
void copy_8x8_16bit_to_16bit_neon(uint16_t *dst, int dstride, const uint16_t *src, int sstride);
RTCD_EXTERN void (*copy_8x8_16bit_to_16bit)(uint16_t *dst, int dstride, const uint16_t *src, int sstride);

void copy_8x8_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src, int sstride);
void copy_8x8_16bit_to_8bit_neon(uint8_t *dst, int dstride, const uint16_t *src, int sstride);
RTCD_EXTERN void (*copy_8x8_16bit_to_8bit)(uint8_t *dst, int dstride, const uint16_t *src, int sstride);

void copy_rect8_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h);
void copy_rect8_16bit_to_16bit_neon(uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h);
RTCD_EXTERN void (*copy_rect8_16bit_to_16bit)(uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h);

void copy_rect8_8bit_to_16bit_c(uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h);
void copy_rect8_8bit_to_16bit_neon(uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h);
RTCD_EXTERN void (*copy_rect8_8bit_to_16bit)(uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h);

int od_dir_find8_c(const od_dering_in *img, int stride, int32_t *var, int coeff_shift);
int od_dir_find8_neon(const od_dering_in *img, int stride, int32_t *var, int coeff_shift);
RTCD_EXTERN int (*od_dir_find8)(const od_dering_in *img, int stride, int32_t *var, int coeff_shift);

void od_filter_dering_direction_4x4_c(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);
void od_filter_dering_direction_4x4_neon(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);
RTCD_EXTERN void (*od_filter_dering_direction_4x4)(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);

void od_filter_dering_direction_8x8_c(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);
void od_filter_dering_direction_8x8_neon(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);
RTCD_EXTERN void (*od_filter_dering_direction_8x8)(uint16_t *y, int ystride, const uint16_t *in, int threshold, int dir, int damping);

void aom_rtcd(void);

#include "aom_config.h"

#ifdef RTCD_C
#include "aom_ports/arm.h"
static void setup_rtcd_internal(void)
{
    int flags = arm_cpu_caps();

    (void)flags;

    aom_clpf_block = aom_clpf_block_c;
    if (flags & HAS_NEON) aom_clpf_block = aom_clpf_block_neon;
    aom_clpf_block_hbd = aom_clpf_block_hbd_c;
    if (flags & HAS_NEON) aom_clpf_block_hbd = aom_clpf_block_hbd_neon;
    aom_clpf_hblock = aom_clpf_hblock_c;
    if (flags & HAS_NEON) aom_clpf_hblock = aom_clpf_hblock_neon;
    aom_clpf_hblock_hbd = aom_clpf_hblock_hbd_c;
    if (flags & HAS_NEON) aom_clpf_hblock_hbd = aom_clpf_hblock_hbd_neon;
    av1_block_error_fp = av1_block_error_fp_c;
    if (flags & HAS_NEON) av1_block_error_fp = av1_block_error_fp_neon;
    av1_fdct8x8_quant = av1_fdct8x8_quant_c;
    if (flags & HAS_NEON) av1_fdct8x8_quant = av1_fdct8x8_quant_neon;
    av1_iht4x4_16_add = av1_iht4x4_16_add_c;
    if (flags & HAS_NEON) av1_iht4x4_16_add = av1_iht4x4_16_add_neon;
    av1_iht8x8_64_add = av1_iht8x8_64_add_c;
    if (flags & HAS_NEON) av1_iht8x8_64_add = av1_iht8x8_64_add_neon;
    av1_quantize_fp = av1_quantize_fp_c;
    if (flags & HAS_NEON) av1_quantize_fp = av1_quantize_fp_neon;
    copy_4x4_16bit_to_16bit = copy_4x4_16bit_to_16bit_c;
    if (flags & HAS_NEON) copy_4x4_16bit_to_16bit = copy_4x4_16bit_to_16bit_neon;
    copy_4x4_16bit_to_8bit = copy_4x4_16bit_to_8bit_c;
    if (flags & HAS_NEON) copy_4x4_16bit_to_8bit = copy_4x4_16bit_to_8bit_neon;
    copy_8x8_16bit_to_16bit = copy_8x8_16bit_to_16bit_c;
    if (flags & HAS_NEON) copy_8x8_16bit_to_16bit = copy_8x8_16bit_to_16bit_neon;
    copy_8x8_16bit_to_8bit = copy_8x8_16bit_to_8bit_c;
    if (flags & HAS_NEON) copy_8x8_16bit_to_8bit = copy_8x8_16bit_to_8bit_neon;
    copy_rect8_16bit_to_16bit = copy_rect8_16bit_to_16bit_c;
    if (flags & HAS_NEON) copy_rect8_16bit_to_16bit = copy_rect8_16bit_to_16bit_neon;
    copy_rect8_8bit_to_16bit = copy_rect8_8bit_to_16bit_c;
    if (flags & HAS_NEON) copy_rect8_8bit_to_16bit = copy_rect8_8bit_to_16bit_neon;
    od_dir_find8 = od_dir_find8_c;
    if (flags & HAS_NEON) od_dir_find8 = od_dir_find8_neon;
    od_filter_dering_direction_4x4 = od_filter_dering_direction_4x4_c;
    if (flags & HAS_NEON) od_filter_dering_direction_4x4 = od_filter_dering_direction_4x4_neon;
    od_filter_dering_direction_8x8 = od_filter_dering_direction_8x8_c;
    if (flags & HAS_NEON) od_filter_dering_direction_8x8 = od_filter_dering_direction_8x8_neon;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif

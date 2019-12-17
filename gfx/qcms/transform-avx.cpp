#include <emmintrin.h>
#include <immintrin.h>

#include "qcmsint.h"

template <size_t kRIndex, size_t kGIndex, size_t kBIndex, size_t kAIndex = NO_A_INDEX>
static void qcms_transform_data_template_lut_avx(const qcms_transform *transform,
                                                 const unsigned char *src,
                                                 unsigned char *dest,
                                                 size_t length)
{
    const float (*mat)[4] = transform->matrix;
    char input_back[64];
    /* Ensure we have a buffer that's 32 byte aligned regardless of the original
     * stack alignment. We can't use __attribute__((aligned(32))) or __declspec(align(32))
     * because they don't work on stack variables. gcc 4.4 does do the right thing
     * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
    float const * input = (float*)(((uintptr_t)&input_back[32]) & ~0x1f);
    /* share input and output locations to save having to keep the
     * locations in separate registers */
    uint32_t const * output = (uint32_t*)input;

    /* deref *transform now to avoid it in loop */
    const float *igtbl_r = transform->input_gamma_table_r;
    const float *igtbl_g = transform->input_gamma_table_g;
    const float *igtbl_b = transform->input_gamma_table_b;

    /* deref *transform now to avoid it in loop */
    const uint8_t *otdata_r = &transform->output_table_r->data[0];
    const uint8_t *otdata_g = &transform->output_table_g->data[0];
    const uint8_t *otdata_b = &transform->output_table_b->data[0];

    /* input matrix values never change */
    const __m256 mat0  = _mm256_broadcast_ps(reinterpret_cast<const __m128*>(mat[0]));
    const __m256 mat1  = _mm256_broadcast_ps(reinterpret_cast<const __m128*>(mat[1]));
    const __m256 mat2  = _mm256_broadcast_ps(reinterpret_cast<const __m128*>(mat[2]));

    /* these values don't change, either */
    const __m256 max   = _mm256_set1_ps(CLAMPMAXVAL);
    const __m256 min   = _mm256_setzero_ps();
    const __m256 scale = _mm256_set1_ps(FLOATSCALE);
    const unsigned int components = A_INDEX_COMPONENTS(kAIndex);

    /* working variables */
    __m256 vec_r, vec_g, vec_b, result;
    __m128 vec_r0, vec_g0, vec_b0, vec_r1, vec_g1, vec_b1;
    unsigned char alpha1, alpha2;

    /* CYA */
    if (!length)
        return;

    /* If there are at least 2 pixels, then we can load their components into
       a single 256-bit register for processing. */
    if (length > 1) {
        vec_r0 = _mm_broadcast_ss(&igtbl_r[src[kRIndex]]);
        vec_g0 = _mm_broadcast_ss(&igtbl_g[src[kGIndex]]);
        vec_b0 = _mm_broadcast_ss(&igtbl_b[src[kBIndex]]);
        vec_r1 = _mm_broadcast_ss(&igtbl_r[src[kRIndex + components]]);
        vec_g1 = _mm_broadcast_ss(&igtbl_g[src[kGIndex + components]]);
        vec_b1 = _mm_broadcast_ss(&igtbl_b[src[kBIndex + components]]);
        vec_r = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_r0), vec_r1, 1);
        vec_g = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_g0), vec_g1, 1);
        vec_b = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_b0), vec_b1, 1);

        if (kAIndex != NO_A_INDEX) {
            alpha1 = src[kAIndex];
            alpha2 = src[kAIndex + components];
        }
    }

    /* If there are at least 4 pixels, then we can iterate and preload the
       next 2 while we store the result of the current 2. */
    while (length > 3) {
        /* Ensure we are pointing at the next 2 pixels for the next load. */
        src += 2 * components;

        /* gamma * matrix */
        vec_r = _mm256_mul_ps(vec_r, mat0);
        vec_g = _mm256_mul_ps(vec_g, mat1);
        vec_b = _mm256_mul_ps(vec_b, mat2);

        /* store alpha for these pixels; load alpha for next two */
        if (kAIndex != NO_A_INDEX) {
            dest[kAIndex] = alpha1;
            dest[kAIndex + components] = alpha2;
            alpha1 = src[kAIndex];
            alpha2 = src[kAIndex + components];
        }

        /* crunch, crunch, crunch */
        vec_r = _mm256_add_ps(vec_r, _mm256_add_ps(vec_g, vec_b));
        vec_r = _mm256_max_ps(min, vec_r);
        vec_r = _mm256_min_ps(max, vec_r);
        result = _mm256_mul_ps(vec_r, scale);

        /* store calc'd output tables indices */
        _mm256_store_si256((__m256i*)output, _mm256_cvtps_epi32(result));

        /* load gamma values for next loop while store completes */
        vec_r0 = _mm_broadcast_ss(&igtbl_r[src[kRIndex]]);
        vec_g0 = _mm_broadcast_ss(&igtbl_g[src[kGIndex]]);
        vec_b0 = _mm_broadcast_ss(&igtbl_b[src[kBIndex]]);
        vec_r1 = _mm_broadcast_ss(&igtbl_r[src[kRIndex + components]]);
        vec_g1 = _mm_broadcast_ss(&igtbl_g[src[kGIndex + components]]);
        vec_b1 = _mm_broadcast_ss(&igtbl_b[src[kBIndex + components]]);
        vec_r = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_r0), vec_r1, 1);
        vec_g = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_g0), vec_g1, 1);
        vec_b = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_b0), vec_b1, 1);

        /* use calc'd indices to output RGB values */
        dest[kRIndex] = otdata_r[output[0]];
        dest[kGIndex] = otdata_g[output[1]];
        dest[kBIndex] = otdata_b[output[2]];
        dest[kRIndex + components] = otdata_r[output[4]];
        dest[kGIndex + components] = otdata_g[output[5]];
        dest[kBIndex + components] = otdata_b[output[6]];

        dest += 2 * components;
        length -= 2;
    }

    /* There are 0-3 pixels remaining. If there are 2-3 remaining, then we know
       we have already populated the necessary registers to start the transform. */
    if (length > 1) {
        vec_r = _mm256_mul_ps(vec_r, mat0);
        vec_g = _mm256_mul_ps(vec_g, mat1);
        vec_b = _mm256_mul_ps(vec_b, mat2);

        if (kAIndex != NO_A_INDEX) {
            dest[kAIndex] = alpha1;
            dest[kAIndex + components] = alpha2;
        }

        vec_r = _mm256_add_ps(vec_r, _mm256_add_ps(vec_g, vec_b));
        vec_r = _mm256_max_ps(min, vec_r);
        vec_r = _mm256_min_ps(max, vec_r);
        result = _mm256_mul_ps(vec_r, scale);

        _mm256_store_si256((__m256i*)output, _mm256_cvtps_epi32(result));

        dest[kRIndex] = otdata_r[output[0]];
        dest[kGIndex] = otdata_g[output[1]];
        dest[kBIndex] = otdata_b[output[2]];
        dest[kRIndex + components] = otdata_r[output[4]];
        dest[kGIndex + components] = otdata_g[output[5]];
        dest[kBIndex + components] = otdata_b[output[6]];

        src += 2 * components;
        dest += 2 * components;
        length -= 2;
    }

    /* There may be 0-1 pixels remaining. */
    if (length == 1) {
        vec_r0 = _mm_broadcast_ss(&igtbl_r[src[kRIndex]]);
        vec_g0 = _mm_broadcast_ss(&igtbl_g[src[kGIndex]]);
        vec_b0 = _mm_broadcast_ss(&igtbl_b[src[kBIndex]]);

        vec_r0 = _mm_mul_ps(vec_r0, _mm256_castps256_ps128(mat0));
        vec_g0 = _mm_mul_ps(vec_g0, _mm256_castps256_ps128(mat1));
        vec_b0 = _mm_mul_ps(vec_b0, _mm256_castps256_ps128(mat2));

        if (kAIndex != NO_A_INDEX) {
            dest[kAIndex] = src[kAIndex];
        }

        vec_r0 = _mm_add_ps(vec_r0, _mm_add_ps(vec_g0, vec_b0));
        vec_r0 = _mm_max_ps(_mm256_castps256_ps128(min), vec_r0);
        vec_r0 = _mm_min_ps(_mm256_castps256_ps128(max), vec_r0);
        vec_r0 = _mm_mul_ps(vec_r0, _mm256_castps256_ps128(scale));

        _mm_store_si128((__m128i*)output, _mm_cvtps_epi32(vec_r0));

        dest[kRIndex] = otdata_r[output[0]];
        dest[kGIndex] = otdata_g[output[1]];
        dest[kBIndex] = otdata_b[output[2]];
    }
}

void qcms_transform_data_rgb_out_lut_avx(const qcms_transform *transform,
                                         const unsigned char *src,
                                         unsigned char *dest,
                                         size_t length)
{
  qcms_transform_data_template_lut_avx<RGBA_R_INDEX, RGBA_G_INDEX, RGBA_B_INDEX>(transform, src, dest, length);
}

void qcms_transform_data_rgba_out_lut_avx(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length)
{
  qcms_transform_data_template_lut_avx<RGBA_R_INDEX, RGBA_G_INDEX, RGBA_B_INDEX, RGBA_A_INDEX>(transform, src, dest, length);
}

void qcms_transform_data_bgra_out_lut_avx(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length)
{
  qcms_transform_data_template_lut_avx<BGRA_R_INDEX, BGRA_G_INDEX, BGRA_B_INDEX, BGRA_A_INDEX>(transform, src, dest, length);
}

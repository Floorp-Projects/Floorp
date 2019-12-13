#include <emmintrin.h>

#include "qcmsint.h"

/* pre-shuffled: just load these into XMM reg instead of load-scalar/shufps sequence */
#define FLOATSCALE  (float)(PRECACHE_OUTPUT_SIZE)
#define CLAMPMAXVAL ( ((float) (PRECACHE_OUTPUT_SIZE - 1)) / PRECACHE_OUTPUT_SIZE )
static const ALIGN float floatScaleX4[4] =
    { FLOATSCALE, FLOATSCALE, FLOATSCALE, FLOATSCALE};
static const ALIGN float clampMaxValueX4[4] =
    { CLAMPMAXVAL, CLAMPMAXVAL, CLAMPMAXVAL, CLAMPMAXVAL};

template <size_t kRIndex, size_t kGIndex, size_t kBIndex, size_t kAIndex = NO_A_INDEX>
static void qcms_transform_data_template_lut_sse2(const qcms_transform *transform,
                                                  const unsigned char *src,
                                                  unsigned char *dest,
                                                  size_t length)
{
    unsigned int i;
    const float (*mat)[4] = transform->matrix;
    char input_back[32];
    /* Ensure we have a buffer that's 16 byte aligned regardless of the original
     * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
     * because they don't work on stack variables. gcc 4.4 does do the right thing
     * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
    float const * input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
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
    const __m128 mat0  = _mm_load_ps(mat[0]);
    const __m128 mat1  = _mm_load_ps(mat[1]);
    const __m128 mat2  = _mm_load_ps(mat[2]);

    /* these values don't change, either */
    const __m128 max   = _mm_load_ps(clampMaxValueX4);
    const __m128 min   = _mm_setzero_ps();
    const __m128 scale = _mm_load_ps(floatScaleX4);
    const unsigned int components = A_INDEX_COMPONENTS(kAIndex);

    /* working variables */
    __m128 vec_r, vec_g, vec_b, result;
    unsigned char alpha;

    /* CYA */
    if (!length)
        return;

    /* one pixel is handled outside of the loop */
    length--;

    /* setup for transforming 1st pixel */
    vec_r = _mm_load_ss(&igtbl_r[src[kRIndex]]);
    vec_g = _mm_load_ss(&igtbl_g[src[kGIndex]]);
    vec_b = _mm_load_ss(&igtbl_b[src[kBIndex]]);
    if (kAIndex != NO_A_INDEX) {
        alpha = src[kAIndex];
    }
    src += components;

    /* transform all but final pixel */

    for (i=0; i<length; i++)
    {
        /* position values from gamma tables */
        vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
        vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
        vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);

        /* gamma * matrix */
        vec_r = _mm_mul_ps(vec_r, mat0);
        vec_g = _mm_mul_ps(vec_g, mat1);
        vec_b = _mm_mul_ps(vec_b, mat2);

        /* store alpha for this pixel; load alpha for next */
        if (kAIndex != NO_A_INDEX) {
            dest[kAIndex] = alpha;
            alpha = src[kAIndex];
        }

        /* crunch, crunch, crunch */
        vec_r  = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
        vec_r  = _mm_max_ps(min, vec_r);
        vec_r  = _mm_min_ps(max, vec_r);
        result = _mm_mul_ps(vec_r, scale);

        /* store calc'd output tables indices */
        _mm_store_si128((__m128i*)output, _mm_cvtps_epi32(result));

        /* load gamma values for next loop while store completes */
        vec_r = _mm_load_ss(&igtbl_r[src[kRIndex]]);
        vec_g = _mm_load_ss(&igtbl_g[src[kGIndex]]);
        vec_b = _mm_load_ss(&igtbl_b[src[kBIndex]]);
        src += components;

        /* use calc'd indices to output RGB values */
        dest[kRIndex] = otdata_r[output[0]];
        dest[kGIndex] = otdata_g[output[1]];
        dest[kBIndex] = otdata_b[output[2]];
        dest += components;
    }

    /* handle final (maybe only) pixel */

    vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
    vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
    vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);

    vec_r = _mm_mul_ps(vec_r, mat0);
    vec_g = _mm_mul_ps(vec_g, mat1);
    vec_b = _mm_mul_ps(vec_b, mat2);

    if (kAIndex != NO_A_INDEX) {
        dest[kAIndex] = alpha;
    }

    vec_r  = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
    vec_r  = _mm_max_ps(min, vec_r);
    vec_r  = _mm_min_ps(max, vec_r);
    result = _mm_mul_ps(vec_r, scale);

    _mm_store_si128((__m128i*)output, _mm_cvtps_epi32(result));

    dest[kRIndex] = otdata_r[output[0]];
    dest[kGIndex] = otdata_g[output[1]];
    dest[kBIndex] = otdata_b[output[2]];
}

void qcms_transform_data_rgb_out_lut_sse2(const qcms_transform *transform,
                                          const unsigned char *src,
                                          unsigned char *dest,
                                          size_t length)
{
  qcms_transform_data_template_lut_sse2<RGBA_R_INDEX, RGBA_G_INDEX, RGBA_B_INDEX>(transform, src, dest, length);
}

void qcms_transform_data_rgba_out_lut_sse2(const qcms_transform *transform,
                                           const unsigned char *src,
                                           unsigned char *dest,
                                           size_t length)
{
  qcms_transform_data_template_lut_sse2<RGBA_R_INDEX, RGBA_G_INDEX, RGBA_B_INDEX, RGBA_A_INDEX>(transform, src, dest, length);
}

void qcms_transform_data_bgra_out_lut_sse2(const qcms_transform *transform,
                                           const unsigned char *src,
                                           unsigned char *dest,
                                           size_t length)
{
  qcms_transform_data_template_lut_sse2<BGRA_R_INDEX, BGRA_G_INDEX, BGRA_B_INDEX, BGRA_A_INDEX>(transform, src, dest, length);
}

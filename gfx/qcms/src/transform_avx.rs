use crate::transform::{qcms_transform, Format, BGRA, CLAMPMAXVAL, FLOATSCALE, RGB, RGBA};
#[cfg(target_arch = "x86")]
pub use std::arch::x86::{
    __m128, __m128i, __m256, __m256i, _mm256_add_ps, _mm256_broadcast_ps, _mm256_castps128_ps256,
    _mm256_castps256_ps128, _mm256_cvtps_epi32, _mm256_insertf128_ps, _mm256_max_ps, _mm256_min_ps,
    _mm256_mul_ps, _mm256_set1_ps, _mm256_setzero_ps, _mm256_store_si256, _mm_add_ps,
    _mm_broadcast_ss, _mm_cvtps_epi32, _mm_max_ps, _mm_min_ps, _mm_mul_ps, _mm_store_si128,
};
#[cfg(target_arch = "x86_64")]
pub use std::arch::x86_64::{
    __m128, __m128i, __m256, __m256i, _mm256_add_ps, _mm256_broadcast_ps, _mm256_castps128_ps256,
    _mm256_castps256_ps128, _mm256_cvtps_epi32, _mm256_insertf128_ps, _mm256_max_ps, _mm256_min_ps,
    _mm256_mul_ps, _mm256_set1_ps, _mm256_setzero_ps, _mm256_store_si256, _mm_add_ps,
    _mm_broadcast_ss, _mm_cvtps_epi32, _mm_max_ps, _mm_min_ps, _mm_mul_ps, _mm_store_si128,
};

#[repr(align(32))]
struct Output([u32; 8]);

#[target_feature(enable = "avx")]
unsafe extern "C" fn qcms_transform_data_template_lut_avx<F: Format>(
    transform: &qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    mut length: usize,
) {
    let mat: *const [f32; 4] = transform.matrix.as_ptr();
    let mut input: Output = std::mem::zeroed();
    /* share input and output locations to save having to keep the
     * locations in separate registers */
    let output: *const u32 = &mut input as *mut Output as *mut u32;
    /* deref *transform now to avoid it in loop */
    let igtbl_r: *const f32 = transform.input_gamma_table_r.as_ref().unwrap().as_ptr();
    let igtbl_g: *const f32 = transform.input_gamma_table_g.as_ref().unwrap().as_ptr();
    let igtbl_b: *const f32 = transform.input_gamma_table_b.as_ref().unwrap().as_ptr();
    /* deref *transform now to avoid it in loop */
    let otdata_r: *const u8 = transform
        .precache_output
        .as_deref()
        .unwrap()
        .lut_r
        .as_ptr();
    let otdata_g: *const u8 = (*transform)
        .precache_output
        .as_deref()
        .unwrap()
        .lut_g
        .as_ptr();
    let otdata_b: *const u8 = (*transform)
        .precache_output
        .as_deref()
        .unwrap()
        .lut_b
        .as_ptr();
    /* input matrix values never change */
    let mat0: __m256 = _mm256_broadcast_ps(&*((*mat.offset(0isize)).as_ptr() as *const __m128));
    let mat1: __m256 = _mm256_broadcast_ps(&*((*mat.offset(1isize)).as_ptr() as *const __m128));
    let mat2: __m256 = _mm256_broadcast_ps(&*((*mat.offset(2isize)).as_ptr() as *const __m128));
    /* these values don't change, either */
    let max: __m256 = _mm256_set1_ps(CLAMPMAXVAL);
    let min: __m256 = _mm256_setzero_ps();
    let scale: __m256 = _mm256_set1_ps(FLOATSCALE);
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;
    /* working variables */
    let mut vec_r: __m256 = _mm256_setzero_ps();
    let mut vec_g: __m256 = _mm256_setzero_ps();
    let mut vec_b: __m256 = _mm256_setzero_ps();
    let mut result: __m256;
    let mut vec_r0: __m128;
    let mut vec_g0: __m128;
    let mut vec_b0: __m128;
    let mut vec_r1: __m128;
    let mut vec_g1: __m128;
    let mut vec_b1: __m128;
    let mut alpha1: u8 = 0;
    let mut alpha2: u8 = 0;
    /* CYA */
    if length == 0 {
        return;
    }
    /* If there are at least 2 pixels, then we can load their components into
    a single 256-bit register for processing. */
    if length > 1 {
        vec_r0 = _mm_broadcast_ss(&*igtbl_r.offset(*src.add(F::kRIndex) as isize));
        vec_g0 = _mm_broadcast_ss(&*igtbl_g.offset(*src.add(F::kGIndex) as isize));
        vec_b0 = _mm_broadcast_ss(&*igtbl_b.offset(*src.add(F::kBIndex) as isize));
        vec_r1 =
            _mm_broadcast_ss(&*igtbl_r.offset(*src.add(F::kRIndex + components as usize) as isize));
        vec_g1 =
            _mm_broadcast_ss(&*igtbl_g.offset(*src.add(F::kGIndex + components as usize) as isize));
        vec_b1 =
            _mm_broadcast_ss(&*igtbl_b.offset(*src.add(F::kBIndex + components as usize) as isize));
        vec_r = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_r0), vec_r1, 1);
        vec_g = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_g0), vec_g1, 1);
        vec_b = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_b0), vec_b1, 1);
        if F::kAIndex != 0xff {
            alpha1 = *src.add(F::kAIndex);
            alpha2 = *src.add(F::kAIndex + components as usize)
        }
    }
    /* If there are at least 4 pixels, then we can iterate and preload the
    next 2 while we store the result of the current 2. */
    while length > 3 {
        /* Ensure we are pointing at the next 2 pixels for the next load. */
        src = src.offset((2 * components) as isize);
        /* gamma * matrix */
        vec_r = _mm256_mul_ps(vec_r, mat0);
        vec_g = _mm256_mul_ps(vec_g, mat1);
        vec_b = _mm256_mul_ps(vec_b, mat2);
        /* store alpha for these pixels; load alpha for next two */
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = alpha1;
            *dest.add(F::kAIndex + components as usize) = alpha2;
            alpha1 = *src.add(F::kAIndex);
            alpha2 = *src.add(F::kAIndex + components as usize)
        }
        /* crunch, crunch, crunch */
        vec_r = _mm256_add_ps(vec_r, _mm256_add_ps(vec_g, vec_b));
        vec_r = _mm256_max_ps(vec_r, min);
        vec_r = _mm256_min_ps(max, vec_r);
        result = _mm256_mul_ps(vec_r, scale);
        /* store calc'd output tables indices */
        _mm256_store_si256(output as *mut __m256i, _mm256_cvtps_epi32(result));
        /* load gamma values for next loop while store completes */
        vec_r0 = _mm_broadcast_ss(&*igtbl_r.offset(*src.add(F::kRIndex) as isize));
        vec_g0 = _mm_broadcast_ss(&*igtbl_g.offset(*src.add(F::kGIndex) as isize));
        vec_b0 = _mm_broadcast_ss(&*igtbl_b.offset(*src.add(F::kBIndex) as isize));
        vec_r1 =
            _mm_broadcast_ss(&*igtbl_r.offset(*src.add(F::kRIndex + components as usize) as isize));
        vec_g1 =
            _mm_broadcast_ss(&*igtbl_g.offset(*src.add(F::kGIndex + components as usize) as isize));
        vec_b1 =
            _mm_broadcast_ss(&*igtbl_b.offset(*src.add(F::kBIndex + components as usize) as isize));
        vec_r = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_r0), vec_r1, 1);
        vec_g = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_g0), vec_g1, 1);
        vec_b = _mm256_insertf128_ps(_mm256_castps128_ps256(vec_b0), vec_b1, 1);
        /* use calc'd indices to output RGB values */
        *dest.add(F::kRIndex) = *otdata_r.offset(*output.offset(0isize) as isize);
        *dest.add(F::kGIndex) = *otdata_g.offset(*output.offset(1isize) as isize);
        *dest.add(F::kBIndex) = *otdata_b.offset(*output.offset(2isize) as isize);
        *dest.add(F::kRIndex + components as usize) =
            *otdata_r.offset(*output.offset(4isize) as isize);
        *dest.add(F::kGIndex + components as usize) =
            *otdata_g.offset(*output.offset(5isize) as isize);
        *dest.add(F::kBIndex + components as usize) =
            *otdata_b.offset(*output.offset(6isize) as isize);
        dest = dest.offset((2 * components) as isize);
        length -= 2
    }
    /* There are 0-3 pixels remaining. If there are 2-3 remaining, then we know
    we have already populated the necessary registers to start the transform. */
    if length > 1 {
        vec_r = _mm256_mul_ps(vec_r, mat0);
        vec_g = _mm256_mul_ps(vec_g, mat1);
        vec_b = _mm256_mul_ps(vec_b, mat2);
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = alpha1;
            *dest.add(F::kAIndex + components as usize) = alpha2
        }
        vec_r = _mm256_add_ps(vec_r, _mm256_add_ps(vec_g, vec_b));
        vec_r = _mm256_max_ps(vec_r, min);
        vec_r = _mm256_min_ps(max, vec_r);
        result = _mm256_mul_ps(vec_r, scale);
        _mm256_store_si256(output as *mut __m256i, _mm256_cvtps_epi32(result));
        *dest.add(F::kRIndex) = *otdata_r.offset(*output.offset(0isize) as isize);
        *dest.add(F::kGIndex) = *otdata_g.offset(*output.offset(1isize) as isize);
        *dest.add(F::kBIndex) = *otdata_b.offset(*output.offset(2isize) as isize);
        *dest.add(F::kRIndex + components as usize) =
            *otdata_r.offset(*output.offset(4isize) as isize);
        *dest.add(F::kGIndex + components as usize) =
            *otdata_g.offset(*output.offset(5isize) as isize);
        *dest.add(F::kBIndex + components as usize) =
            *otdata_b.offset(*output.offset(6isize) as isize);
        src = src.offset((2 * components) as isize);
        dest = dest.offset((2 * components) as isize);
        length -= 2
    }
    /* There may be 0-1 pixels remaining. */
    if length == 1 {
        vec_r0 = _mm_broadcast_ss(&*igtbl_r.offset(*src.add(F::kRIndex) as isize));
        vec_g0 = _mm_broadcast_ss(&*igtbl_g.offset(*src.add(F::kGIndex) as isize));
        vec_b0 = _mm_broadcast_ss(&*igtbl_b.offset(*src.add(F::kBIndex) as isize));
        vec_r0 = _mm_mul_ps(vec_r0, _mm256_castps256_ps128(mat0));
        vec_g0 = _mm_mul_ps(vec_g0, _mm256_castps256_ps128(mat1));
        vec_b0 = _mm_mul_ps(vec_b0, _mm256_castps256_ps128(mat2));
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = *src.add(F::kAIndex)
        }
        vec_r0 = _mm_add_ps(vec_r0, _mm_add_ps(vec_g0, vec_b0));
        vec_r0 = _mm_max_ps(vec_r0, _mm256_castps256_ps128(min));
        vec_r0 = _mm_min_ps(_mm256_castps256_ps128(max), vec_r0);
        vec_r0 = _mm_mul_ps(vec_r0, _mm256_castps256_ps128(scale));
        _mm_store_si128(output as *mut __m128i, _mm_cvtps_epi32(vec_r0));
        *dest.add(F::kRIndex) = *otdata_r.offset(*output.offset(0isize) as isize);
        *dest.add(F::kGIndex) = *otdata_g.offset(*output.offset(1isize) as isize);
        *dest.add(F::kBIndex) = *otdata_b.offset(*output.offset(2isize) as isize)
    };
}
#[no_mangle]
#[target_feature(enable = "avx")]
pub unsafe fn qcms_transform_data_rgb_out_lut_avx(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_avx::<RGB>(transform, src, dest, length);
}
#[no_mangle]
#[target_feature(enable = "avx")]
pub unsafe fn qcms_transform_data_rgba_out_lut_avx(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_avx::<RGBA>(transform, src, dest, length);
}
#[no_mangle]
#[target_feature(enable = "avx")]
pub unsafe fn qcms_transform_data_bgra_out_lut_avx(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_avx::<BGRA>(transform, src, dest, length);
}

use crate::transform::{qcms_transform, Format, BGRA, CLAMPMAXVAL, FLOATSCALE, RGB, RGBA};
#[cfg(target_arch = "x86")]
pub use std::arch::x86::{
    __m128, __m128i, _mm_add_ps, _mm_cvtps_epi32, _mm_load_ps, _mm_load_ss, _mm_max_ps, _mm_min_ps,
    _mm_mul_ps, _mm_set1_ps, _mm_setzero_ps, _mm_shuffle_ps, _mm_store_si128,
};
#[cfg(target_arch = "x86_64")]
pub use std::arch::x86_64::{
    __m128, __m128i, _mm_add_ps, _mm_cvtps_epi32, _mm_load_ps, _mm_load_ss, _mm_max_ps, _mm_min_ps,
    _mm_mul_ps, _mm_set1_ps, _mm_setzero_ps, _mm_shuffle_ps, _mm_store_si128,
};

#[repr(align(16))]
struct Output([u32; 4]);

unsafe extern "C" fn qcms_transform_data_template_lut_sse2<F: Format>(
    transform: &qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    mut length: usize,
) {
    let mat: *const [f32; 4] = (*transform).matrix.as_ptr();
    let mut input: Output = std::mem::zeroed();
    /* share input and output locations to save having to keep the
     * locations in separate registers */
    let output: *const u32 = &mut input as *mut Output as *mut u32;
    /* deref *transform now to avoid it in loop */
    let igtbl_r: *const f32 = (*transform).input_gamma_table_r.as_ref().unwrap().as_ptr();
    let igtbl_g: *const f32 = (*transform).input_gamma_table_g.as_ref().unwrap().as_ptr();
    let igtbl_b: *const f32 = (*transform).input_gamma_table_b.as_ref().unwrap().as_ptr();
    /* deref *transform now to avoid it in loop */
    let otdata_r: *const u8 = (*transform)
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
    let mat0: __m128 = _mm_load_ps((*mat.offset(0isize)).as_ptr());
    let mat1: __m128 = _mm_load_ps((*mat.offset(1isize)).as_ptr());
    let mat2: __m128 = _mm_load_ps((*mat.offset(2isize)).as_ptr());
    /* these values don't change, either */
    let max: __m128 = _mm_set1_ps(CLAMPMAXVAL);
    let min: __m128 = _mm_setzero_ps();
    let scale: __m128 = _mm_set1_ps(FLOATSCALE);
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;
    /* working variables */
    let mut vec_r: __m128;
    let mut vec_g: __m128;
    let mut vec_b: __m128;
    let mut result: __m128;
    let mut alpha: u8 = 0;
    /* CYA */
    if length == 0 {
        return;
    }
    /* one pixel is handled outside of the loop */
    length -= 1;
    /* setup for transforming 1st pixel */
    vec_r = _mm_load_ss(&*igtbl_r.offset(*src.add(F::kRIndex) as isize));
    vec_g = _mm_load_ss(&*igtbl_g.offset(*src.add(F::kGIndex) as isize));
    vec_b = _mm_load_ss(&*igtbl_b.offset(*src.add(F::kBIndex) as isize));
    if F::kAIndex != 0xff {
        alpha = *src.add(F::kAIndex)
    }
    src = src.offset(components as isize);
    let mut i: u32 = 0;
    while (i as usize) < length {
        /* position values from gamma tables */
        vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
        vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
        vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);
        /* gamma * matrix */
        vec_r = _mm_mul_ps(vec_r, mat0);
        vec_g = _mm_mul_ps(vec_g, mat1);
        vec_b = _mm_mul_ps(vec_b, mat2);
        /* store alpha for this pixel; load alpha for next */
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = alpha;
            alpha = *src.add(F::kAIndex)
        }
        /* crunch, crunch, crunch */
        vec_r = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
        vec_r = _mm_max_ps(vec_r, min);
        vec_r = _mm_min_ps(max, vec_r);
        result = _mm_mul_ps(vec_r, scale);
        /* store calc'd output tables indices */
        _mm_store_si128(output as *mut __m128i, _mm_cvtps_epi32(result));
        /* load gamma values for next loop while store completes */
        vec_r = _mm_load_ss(&*igtbl_r.offset(*src.add(F::kRIndex) as isize));
        vec_g = _mm_load_ss(&*igtbl_g.offset(*src.add(F::kGIndex) as isize));
        vec_b = _mm_load_ss(&*igtbl_b.offset(*src.add(F::kBIndex) as isize));
        src = src.offset(components as isize);
        /* use calc'd indices to output RGB values */
        *dest.add(F::kRIndex) = *otdata_r.offset(*output.offset(0isize) as isize);
        *dest.add(F::kGIndex) = *otdata_g.offset(*output.offset(1isize) as isize);
        *dest.add(F::kBIndex) = *otdata_b.offset(*output.offset(2isize) as isize);
        dest = dest.offset(components as isize);
        i += 1
    }
    /* handle final (maybe only) pixel */
    vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
    vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
    vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);
    vec_r = _mm_mul_ps(vec_r, mat0);
    vec_g = _mm_mul_ps(vec_g, mat1);
    vec_b = _mm_mul_ps(vec_b, mat2);
    if F::kAIndex != 0xff {
        *dest.add(F::kAIndex) = alpha
    }
    vec_r = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));
    vec_r = _mm_max_ps(vec_r, min);
    vec_r = _mm_min_ps(max, vec_r);
    result = _mm_mul_ps(vec_r, scale);
    _mm_store_si128(output as *mut __m128i, _mm_cvtps_epi32(result));
    *dest.add(F::kRIndex) = *otdata_r.offset(*output.offset(0isize) as isize);
    *dest.add(F::kGIndex) = *otdata_g.offset(*output.offset(1isize) as isize);
    *dest.add(F::kBIndex) = *otdata_b.offset(*output.offset(2isize) as isize);
}
#[no_mangle]
pub unsafe fn qcms_transform_data_rgb_out_lut_sse2(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_sse2::<RGB>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe fn qcms_transform_data_rgba_out_lut_sse2(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_sse2::<RGBA>(transform, src, dest, length);
}

#[no_mangle]
pub unsafe fn qcms_transform_data_bgra_out_lut_sse2(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_sse2::<BGRA>(transform, src, dest, length);
}

use crate::transform::{qcms_transform, Format, BGRA, CLAMPMAXVAL, FLOATSCALE, RGB, RGBA};
#[cfg(target_arch = "aarch64")]
use core::arch::aarch64::{
    float32x4_t, int32x4_t, vaddq_f32, vcvtq_s32_f32, vgetq_lane_s32, vld1q_dup_f32, vld1q_f32,
    vmaxq_f32, vminq_f32, vmulq_f32,
};
#[cfg(target_arch = "arm")]
use core::arch::arm::{
    float32x4_t, int32x4_t, vaddq_f32, vcvtq_s32_f32, vgetq_lane_s32, vld1q_dup_f32, vld1q_f32,
    vmaxq_f32, vminq_f32, vmulq_f32,
};
use std::mem::zeroed;

static mut floatScale: f32 = FLOATSCALE;
static mut clampMaxValue: f32 = CLAMPMAXVAL;

#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
unsafe fn qcms_transform_data_template_lut_neon<F: Format>(
    transform: &qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    mut length: usize,
) {
    let mat: *const [f32; 4] = (*transform).matrix.as_ptr();
    /* deref *transform now to avoid it in loop */
    let igtbl_r: *const f32 = (*transform).input_gamma_table_r.as_ref().unwrap().as_ptr();
    let igtbl_g: *const f32 = (*transform).input_gamma_table_g.as_ref().unwrap().as_ptr();
    let igtbl_b: *const f32 = (*transform).input_gamma_table_b.as_ref().unwrap().as_ptr();
    /* deref *transform now to avoid it in loop */
    let otdata_r: *const u8 = (*transform)
        .output_table_r
        .as_deref()
        .unwrap()
        .data
        .as_ptr();
    let otdata_g: *const u8 = (*transform)
        .output_table_g
        .as_deref()
        .unwrap()
        .data
        .as_ptr();
    let otdata_b: *const u8 = (*transform)
        .output_table_b
        .as_deref()
        .unwrap()
        .data
        .as_ptr();
    /* input matrix values never change */
    let mat0: float32x4_t = vld1q_f32((*mat.offset(0isize)).as_ptr());
    let mat1: float32x4_t = vld1q_f32((*mat.offset(1isize)).as_ptr());
    let mat2: float32x4_t = vld1q_f32((*mat.offset(2isize)).as_ptr());
    /* these values don't change, either */
    let max: float32x4_t = vld1q_dup_f32(&clampMaxValue);
    let min: float32x4_t = zeroed();
    let scale: float32x4_t = vld1q_dup_f32(&floatScale);
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;
    /* working variables */
    let mut vec_r: float32x4_t;
    let mut vec_g: float32x4_t;
    let mut vec_b: float32x4_t;
    let mut result: int32x4_t;
    let mut alpha: u8 = 0;
    /* CYA */
    if length == 0 {
        return;
    }
    /* one pixel is handled outside of the loop */
    length = length.wrapping_sub(1);
    /* setup for transforming 1st pixel */
    vec_r = vld1q_dup_f32(&*igtbl_r.offset(*src.offset(F::kRIndex as isize) as isize));
    vec_g = vld1q_dup_f32(&*igtbl_g.offset(*src.offset(F::kGIndex as isize) as isize));
    vec_b = vld1q_dup_f32(&*igtbl_b.offset(*src.offset(F::kBIndex as isize) as isize));
    if F::kAIndex != 0xff {
        alpha = *src.offset(F::kAIndex as isize)
    }
    src = src.offset(components as isize);
    let mut i: u32 = 0;
    while (i as usize) < length {
        /* gamma * matrix */
        vec_r = vmulq_f32(vec_r, mat0);
        vec_g = vmulq_f32(vec_g, mat1);
        vec_b = vmulq_f32(vec_b, mat2);
        /* store alpha for this pixel; load alpha for next */
        if F::kAIndex != 0xff {
            *dest.offset(F::kAIndex as isize) = alpha;
            alpha = *src.offset(F::kAIndex as isize)
        }
        /* crunch, crunch, crunch */
        vec_r = vaddq_f32(vec_r, vaddq_f32(vec_g, vec_b));
        vec_r = vmaxq_f32(min, vec_r);
        vec_r = vminq_f32(max, vec_r);
        result = vcvtq_s32_f32(vmulq_f32(vec_r, scale));

        /* use calc'd indices to output RGB values */
        *dest.offset(F::kRIndex as isize) = *otdata_r.offset(vgetq_lane_s32(result, 0) as isize);
        *dest.offset(F::kGIndex as isize) = *otdata_g.offset(vgetq_lane_s32(result, 1) as isize);
        *dest.offset(F::kBIndex as isize) = *otdata_b.offset(vgetq_lane_s32(result, 2) as isize);

        /* load gamma values for next loop while store completes */
        vec_r = vld1q_dup_f32(&*igtbl_r.offset(*src.offset(F::kRIndex as isize) as isize));
        vec_g = vld1q_dup_f32(&*igtbl_g.offset(*src.offset(F::kGIndex as isize) as isize));
        vec_b = vld1q_dup_f32(&*igtbl_b.offset(*src.offset(F::kBIndex as isize) as isize));

        dest = dest.offset(components as isize);
        src = src.offset(components as isize);
        i = i.wrapping_add(1)
    }
    /* handle final (maybe only) pixel */
    vec_r = vmulq_f32(vec_r, mat0);
    vec_g = vmulq_f32(vec_g, mat1);
    vec_b = vmulq_f32(vec_b, mat2);
    if F::kAIndex != 0xff {
        *dest.offset(F::kAIndex as isize) = alpha
    }
    vec_r = vaddq_f32(vec_r, vaddq_f32(vec_g, vec_b));
    vec_r = vmaxq_f32(min, vec_r);
    vec_r = vminq_f32(max, vec_r);
    result = vcvtq_s32_f32(vmulq_f32(vec_r, scale));

    *dest.offset(F::kRIndex as isize) = *otdata_r.offset(vgetq_lane_s32(result, 0) as isize);
    *dest.offset(F::kGIndex as isize) = *otdata_g.offset(vgetq_lane_s32(result, 1) as isize);
    *dest.offset(F::kBIndex as isize) = *otdata_b.offset(vgetq_lane_s32(result, 2) as isize);
}
#[no_mangle]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn qcms_transform_data_rgb_out_lut_neon(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_neon::<RGB>(transform, src, dest, length);
}
#[no_mangle]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn qcms_transform_data_rgba_out_lut_neon(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_neon::<RGBA>(transform, src, dest, length);
}

#[no_mangle]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn qcms_transform_data_bgra_out_lut_neon(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_neon::<BGRA>(transform, src, dest, length);
}

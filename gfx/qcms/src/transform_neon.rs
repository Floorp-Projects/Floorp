use crate::transform::{qcms_transform, Format, BGRA, CLAMPMAXVAL, FLOATSCALE, RGB, RGBA};
use ::libc;
#[cfg(target_arch = "aarch64")]
use core::arch::aarch64::{float32x4_t, int32x4_t, vaddq_f32};
#[cfg(target_arch = "arm")]
use core::arch::arm::{float32x4_t, int32x4_t, vaddq_f32};
use std::mem::zeroed;

static mut floatScale: f32 = FLOATSCALE;
static mut clampMaxValue: f32 = CLAMPMAXVAL;

#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
unsafe extern "C" fn qcms_transform_data_template_lut_neon<F: Format>(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    let mut mat: *const [f32; 4] = (*transform).matrix.as_ptr();
    let mut input_back: [libc::c_char; 32] = [0; 32];
    /* Ensure we have a buffer that's 16 byte aligned regardless of the original
     * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
     * because they don't work on stack variables. gcc 4.4 does do the right thing
     * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
    let mut input: *const f32 = (&mut *input_back.as_mut_ptr().offset(16isize) as *mut libc::c_char
        as usize
        & !(0xf) as usize) as *mut f32;
    /* share input and output locations to save having to keep the
     * locations in separate registers */
    let mut output: *const u32 = input as *mut u32;
    /* deref *transform now to avoid it in loop */
    let mut igtbl_r: *const f32 = (*transform).input_gamma_table_r.as_ref().unwrap().as_ptr();
    let mut igtbl_g: *const f32 = (*transform).input_gamma_table_g.as_ref().unwrap().as_ptr();
    let mut igtbl_b: *const f32 = (*transform).input_gamma_table_b.as_ref().unwrap().as_ptr();
    /* deref *transform now to avoid it in loop */
    let mut otdata_r: *const u8 = (*transform)
        .output_table_r
        .as_deref()
        .unwrap()
        .data
        .as_ptr();
    let mut otdata_g: *const u8 = (*transform)
        .output_table_g
        .as_deref()
        .unwrap()
        .data
        .as_ptr();
    let mut otdata_b: *const u8 = (*transform)
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
    let components: libc::c_uint = if F::kAIndex == 0xff { 3 } else { 4 } as libc::c_uint;
    /* working variables */
    let mut vec_r: float32x4_t;
    let mut vec_g: float32x4_t;
    let mut vec_b: float32x4_t;
    let mut result: int32x4_t;
    let mut alpha: libc::c_uchar = 0;
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
    let mut i: libc::c_uint = 0;
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
pub unsafe extern "C" fn qcms_transform_data_rgb_out_lut_neon(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut_neon::<RGB>(transform, src, dest, length);
}
#[no_mangle]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe extern "C" fn qcms_transform_data_rgba_out_lut_neon(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut_neon::<RGBA>(transform, src, dest, length);
}

#[no_mangle]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe extern "C" fn qcms_transform_data_bgra_out_lut_neon(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut_neon::<BGRA>(transform, src, dest, length);
}

use std::mem::transmute;

#[inline]
#[target_feature(enable = "neon")]
#[cfg(target_arch = "aarch64")]
pub unsafe fn vld1q_f32(addr: *const f32) -> float32x4_t {
    transmute([*addr, *addr.offset(1), *addr.offset(2), *addr.offset(3)])
}

#[inline]
#[cfg(target_arch = "arm")]
#[target_feature(enable = "neon")]
#[target_feature(enable = "v7")]
pub unsafe fn vld1q_f32(addr: *const f32) -> float32x4_t {
    vld1q_v4f32(addr as *const u8, 4)
}

#[cfg(target_arch = "arm")]
#[allow(improper_ctypes)]
extern "C" {
    #[cfg_attr(target_arch = "arm", link_name = "llvm.arm.neon.vld1.v4f32.p0i8")]
    fn vld1q_v4f32(addr: *const u8, align: u32) -> float32x4_t;
}

#[cfg(target_arch = "aarch64")]
#[allow(improper_ctypes)]
extern "C" {
    #[link_name = "llvm.aarch64.neon.fcvtzs.v4.v4f32"]
    fn vcvtq_s32_f32_(a: float32x4_t) -> int32x4_t;
}

#[allow(improper_ctypes)]
extern "C" {
    #[cfg_attr(target_arch = "arm", link_name = "llvm.arm.neon.vmaxs.v4f32")]
    #[cfg_attr(target_arch = "aarch64", link_name = "llvm.aarch64.neon.fmax.v4f32")]
    fn vmaxq_f32_(a: float32x4_t, b: float32x4_t) -> float32x4_t;
    #[cfg_attr(target_arch = "arm", link_name = "llvm.arm.neon.vmins.v4f32")]
    #[cfg_attr(target_arch = "aarch64", link_name = "llvm.aarch64.neon.fmin.v4f32")]
    fn vminq_f32_(a: float32x4_t, b: float32x4_t) -> float32x4_t;
}

/// Move vector element to general-purpose register
#[inline]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn vgetq_lane_s32(v: int32x4_t, imm5: i32) -> i32 {
    assert!(imm5 >= 0 && imm5 <= 3);
    simd_extract(v, imm5 as u32)
}

/// Multiply
#[inline]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn vmulq_f32(a: float32x4_t, b: float32x4_t) -> float32x4_t {
    simd_mul(a, b)
}

/// Floating-point minimum (vector).
#[inline]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn vminq_f32(a: float32x4_t, b: float32x4_t) -> float32x4_t {
    vminq_f32_(a, b)
}

/// Floating-point maxmimum (vector).
#[inline]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn vmaxq_f32(a: float32x4_t, b: float32x4_t) -> float32x4_t {
    vmaxq_f32_(a, b)
}

#[inline]
#[cfg(target_arch = "aarch64")]
#[target_feature(enable = "neon")]
pub unsafe fn vcvtq_s32_f32(a: float32x4_t) -> int32x4_t {
    vcvtq_s32_f32_(a)
}
/// Floating-point Convert to Signed fixed-point, rounding toward Zero (vector)
#[inline]
#[cfg(target_arch = "arm")]
#[target_feature(enable = "neon")]
#[target_feature(enable = "v7")]
pub unsafe fn vcvtq_s32_f32(a: float32x4_t) -> int32x4_t {
    simd_cast::<_, int32x4_t>(a)
}

/// Load one single-element structure and Replicate to all lanes (of one register).
#[inline]
#[target_feature(enable = "neon")]
#[cfg_attr(target_arch = "arm", target_feature(enable = "v7"))]
pub unsafe fn vld1q_dup_f32(addr: *const f32) -> float32x4_t {
    let v = *addr;
    transmute([v, v, v, v])
}

extern "platform-intrinsic" {
    pub fn simd_mul<T>(x: T, y: T) -> T;
    pub fn simd_extract<T, U>(x: T, idx: u32) -> U;
    pub fn simd_cast<T, U>(x: T) -> U;
}

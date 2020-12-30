#![allow(dead_code)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
#![allow(unused_mut)]
#![allow(unused_variables)]
#![feature(stdsimd)]
// These are needed for the neon intrinsics implementation
// and can be removed once the MSRV is high enough (1.48)
#![feature(platform_intrinsics)]
#![feature(simd_ffi)]
#![feature(link_llvm_intrinsics)]
#![feature(aarch64_target_feature)]
#![feature(arm_target_feature)]
#![feature(raw_ref_op)]

#[repr(u32)]
#[derive(Clone, Copy)]
pub enum Intent {
    QCMS_INTENT_ABSOLUTE_COLORIMETRIC = 3,
    QCMS_INTENT_SATURATION = 2,
    QCMS_INTENT_RELATIVE_COLORIMETRIC = 1,
    QCMS_INTENT_PERCEPTUAL = 0,
}

use Intent::*;

impl Default for Intent {
    fn default() -> Self {
        QCMS_INTENT_PERCEPTUAL
    }
}

pub(crate) type s15Fixed16Number = i32;

/* produces the nearest float to 'a' with a maximum error
 * of 1/1024 which happens for large values like 0x40000040 */
#[inline]
fn s15Fixed16Number_to_float(mut a: s15Fixed16Number) -> f32 {
    a as f32 / 65536.0
}

#[inline]
fn double_to_s15Fixed16Number(mut v: f64) -> s15Fixed16Number {
    (v * 65536f64) as i32
}

#[cfg(feature = "c_bindings")]
extern crate libc;
#[cfg(feature = "c_bindings")]
pub mod c_bindings;
mod chain;
mod gtest;
mod iccread;
mod matrix;
mod transform;
pub use iccread::qcms_CIE_xyY as CIE_xyY;
pub use iccread::qcms_CIE_xyYTRIPLE as CIE_xyYTRIPLE;
pub use iccread::Profile;
pub use transform::qcms_data_type as DataType;
pub use transform::Transform;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
mod transform_avx;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
mod transform_neon;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
mod transform_sse2;
mod transform_util;

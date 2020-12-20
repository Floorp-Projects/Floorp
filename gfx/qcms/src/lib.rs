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

extern crate libc;

pub type qcms_intent = libc::c_uint;
pub const QCMS_INTENT_DEFAULT: qcms_intent = 0;
pub const QCMS_INTENT_MAX: qcms_intent = 3;
pub const QCMS_INTENT_ABSOLUTE_COLORIMETRIC: qcms_intent = 3;
pub const QCMS_INTENT_SATURATION: qcms_intent = 2;
pub const QCMS_INTENT_RELATIVE_COLORIMETRIC: qcms_intent = 1;
pub const QCMS_INTENT_PERCEPTUAL: qcms_intent = 0;
pub const QCMS_INTENT_MIN: qcms_intent = 0;

pub type s15Fixed16Number = i32;

/* produces the nearest float to 'a' with a maximum error
 * of 1/1024 which happens for large values like 0x40000040 */
#[inline]
fn s15Fixed16Number_to_float(mut a: s15Fixed16Number) -> f32 {
    return a as f32 / 65536.0;
}

#[inline]
fn double_to_s15Fixed16Number(mut v: f64) -> s15Fixed16Number {
    return (v * 65536f64) as i32;
}

pub mod c_bindings;
mod chain;
mod gtest;
pub mod iccread;
pub mod matrix;
pub mod transform;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
pub mod transform_avx;
#[cfg(any(target_arch = "aarch64", target_arch = "arm"))]
pub mod transform_neon;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
pub mod transform_sse2;
mod transform_util;

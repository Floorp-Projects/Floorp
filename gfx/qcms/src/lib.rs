/*! A pure Rust color management library.
*/

#![allow(dead_code)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]
// These are needed for the neon SIMD code and can be removed once the MSRV supports the
// instrinsics we use
#![cfg_attr(feature = "neon", feature(stdsimd))]
#![cfg_attr(
    feature = "neon",
    feature(arm_target_feature, raw_ref_op)
)]

/// These values match the Rendering Intent values from the ICC spec
#[repr(u32)]
#[derive(Clone, Copy, Debug)]
pub enum Intent {
    AbsoluteColorimetric = 3,
    Saturation = 2,
    RelativeColorimetric = 1,
    Perceptual = 0,
}

use Intent::*;

impl Default for Intent {
    fn default() -> Self {
        /* Chris Murphy (CM consultant) suggests this as a default in the event that we
         * cannot reproduce relative + Black Point Compensation.  BPC brings an
         * unacceptable performance overhead, so we go with perceptual. */
        Perceptual
    }
}

pub(crate) type s15Fixed16Number = i32;

/* produces the nearest float to 'a' with a maximum error
 * of 1/1024 which happens for large values like 0x40000040 */
#[inline]
fn s15Fixed16Number_to_float(a: s15Fixed16Number) -> f32 {
    a as f32 / 65536.0
}

#[inline]
fn double_to_s15Fixed16Number(v: f64) -> s15Fixed16Number {
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
pub use transform::DataType;
pub use transform::Transform;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
mod transform_avx;
#[cfg(all(any(target_arch = "aarch64", target_arch = "arm"), feature = "neon"))]
mod transform_neon;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
mod transform_sse2;
mod transform_util;

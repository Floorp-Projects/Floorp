/* vim: set ts=8 sw=8 noexpandtab: */
//  qcms
//  Copyright (C) 2009 Mozilla Foundation
//  Copyright (C) 1998-2007 Marti Maria
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
use crate::transform_neon::{
    qcms_transform_data_bgra_out_lut_neon, qcms_transform_data_rgb_out_lut_neon,
    qcms_transform_data_rgba_out_lut_neon,
};
use crate::{
    chain::qcms_chain_transform,
    double_to_s15Fixed16Number,
    iccread::qcms_supports_iccv4,
    matrix::*,
    transform_util::{
        build_colorant_matrix, build_input_gamma_table, build_output_lut, compute_precache,
        lut_interp_linear,
    },
};
use crate::{
    iccread::{curveType, qcms_CIE_xyY, qcms_CIE_xyYTRIPLE, qcms_profile},
    qcms_intent, s15Fixed16Number,
    transform_util::clamp_float,
};
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
use crate::{
    transform_avx::{
        qcms_transform_data_bgra_out_lut_avx, qcms_transform_data_rgb_out_lut_avx,
        qcms_transform_data_rgba_out_lut_avx,
    },
    transform_sse2::{
        qcms_transform_data_bgra_out_lut_sse2, qcms_transform_data_rgb_out_lut_sse2,
        qcms_transform_data_rgba_out_lut_sse2,
    },
};

use ::libc::{self, free, malloc};
use std::{ptr::null_mut, sync::atomic::Ordering};
use std::sync::Arc;

const PRECACHE_OUTPUT_SIZE: usize = 8192;
const PRECACHE_OUTPUT_MAX: usize = PRECACHE_OUTPUT_SIZE - 1;
pub const FLOATSCALE: f32 = PRECACHE_OUTPUT_SIZE as f32;
pub const CLAMPMAXVAL: f32 = ((PRECACHE_OUTPUT_SIZE - 1) as f32) / PRECACHE_OUTPUT_SIZE as f32;

#[repr(C)]
pub struct precache_output {
    /* We previously used a count of 65536 here but that seems like more
     * precision than we actually need.  By reducing the size we can
     * improve startup performance and reduce memory usage. ColorSync on
     * 10.5 uses 4097 which is perhaps because they use a fixed point
     * representation where 1. is represented by 0x1000. */
    pub data: [u8; PRECACHE_OUTPUT_SIZE],
}

impl Default for precache_output {
    fn default() -> precache_output {
        precache_output {
            data: [0; PRECACHE_OUTPUT_SIZE],
        }
    }
}

/* used as a lookup table for the output transformation.
 * we refcount them so we only need to have one around per output
 * profile, instead of duplicating them per transform */

#[repr(C)]
#[repr(align(16))]
#[derive(Clone)]
pub struct qcms_transform {
    pub matrix: [[f32; 4]; 3],
    pub input_gamma_table_r: *mut f32,
    pub input_gamma_table_g: *mut f32,
    pub input_gamma_table_b: *mut f32,
    pub input_clut_table_r: *mut f32,
    pub input_clut_table_g: *mut f32,
    pub input_clut_table_b: *mut f32,
    pub input_clut_table_length: u16,
    pub r_clut: *mut f32,
    pub g_clut: *mut f32,
    pub b_clut: *mut f32,
    pub grid_size: u16,
    pub output_clut_table_r: *mut f32,
    pub output_clut_table_g: *mut f32,
    pub output_clut_table_b: *mut f32,
    pub output_clut_table_length: u16,
    pub input_gamma_table_gray: *mut f32,
    pub out_gamma_r: f32,
    pub out_gamma_g: f32,
    pub out_gamma_b: f32,
    pub out_gamma_gray: f32,
    pub output_gamma_lut_r: *mut u16,
    pub output_gamma_lut_g: *mut u16,
    pub output_gamma_lut_b: *mut u16,
    pub output_gamma_lut_gray: *mut u16,
    pub output_gamma_lut_r_length: usize,
    pub output_gamma_lut_g_length: usize,
    pub output_gamma_lut_b_length: usize,
    pub output_gamma_lut_gray_length: usize,
    pub output_table_r: Option<Arc<precache_output>>,
    pub output_table_g: Option<Arc<precache_output>>,
    pub output_table_b: Option<Arc<precache_output>>,
    pub transform_fn: transform_fn_t,
}

impl Default for qcms_transform {
    fn default() -> qcms_transform {
        qcms_transform {
            matrix: Default::default(),
            input_gamma_table_r: null_mut(),
            input_gamma_table_b: null_mut(),
            input_gamma_table_g: null_mut(),
            input_clut_table_r: null_mut(),
            input_clut_table_g: null_mut(),
            input_clut_table_b: null_mut(),
            input_clut_table_length: Default::default(),
            r_clut: null_mut(),
            g_clut: null_mut(),
            b_clut: null_mut(),
            grid_size: Default::default(),
            output_clut_table_r: null_mut(),
            output_clut_table_g: null_mut(),
            output_clut_table_b: null_mut(),
            output_clut_table_length: Default::default(),
            input_gamma_table_gray: null_mut(),
            out_gamma_r: Default::default(),
            out_gamma_g: Default::default(),
            out_gamma_b: Default::default(),
            out_gamma_gray: Default::default(),
            output_gamma_lut_r: null_mut(),
            output_gamma_lut_g: null_mut(),
            output_gamma_lut_b: null_mut(),
            output_gamma_lut_gray: null_mut(),
            output_gamma_lut_r_length: Default::default(),
            output_gamma_lut_g_length: Default::default(),
            output_gamma_lut_b_length: Default::default(),
            output_gamma_lut_gray_length: Default::default(),
            output_table_r: Default::default(),
            output_table_g: Default::default(),
            output_table_b: Default::default(),
            transform_fn: Default::default(),
         }
    }
}

pub type transform_fn_t = Option<
    unsafe extern "C" fn(
        _: *const qcms_transform,
        _: *const libc::c_uchar,
        _: *mut libc::c_uchar,
        _: usize,
    ) -> (),
>;

// 16 is the upperbound, actual is 0..num_in_channels.
// reversed elements (for mBA)
/* should lut8Type and lut16Type be different types? */
// used by lut8Type/lut16Type (mft2) only

#[repr(C)]
#[derive(Copy, Clone)]
pub struct lutmABType {
    pub num_in_channels: u8,
    pub num_out_channels: u8,
    pub num_grid_points: [u8; 16],
    pub e00: s15Fixed16Number,
    pub e01: s15Fixed16Number,
    pub e02: s15Fixed16Number,
    pub e03: s15Fixed16Number,
    pub e10: s15Fixed16Number,
    pub e11: s15Fixed16Number,
    pub e12: s15Fixed16Number,
    pub e13: s15Fixed16Number,
    pub e20: s15Fixed16Number,
    pub e21: s15Fixed16Number,
    pub e22: s15Fixed16Number,
    pub e23: s15Fixed16Number,
    pub reversed: bool,
    pub clut_table: *mut f32,
    pub a_curves: [*mut curveType; 10],
    pub b_curves: [*mut curveType; 10],
    pub m_curves: [*mut curveType; 10],
    pub clut_table_data: [f32; 0],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct lutType {
    pub num_input_channels: u8,
    pub num_output_channels: u8,
    pub num_clut_grid_points: u8,
    pub e00: s15Fixed16Number,
    pub e01: s15Fixed16Number,
    pub e02: s15Fixed16Number,
    pub e10: s15Fixed16Number,
    pub e11: s15Fixed16Number,
    pub e12: s15Fixed16Number,
    pub e20: s15Fixed16Number,
    pub e21: s15Fixed16Number,
    pub e22: s15Fixed16Number,
    pub num_input_table_entries: u16,
    pub num_output_table_entries: u16,
    pub input_table: *mut f32,
    pub clut_table: *mut f32,
    pub output_table: *mut f32,
    pub table_data: [f32; 0],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct XYZNumber {
    pub X: s15Fixed16Number,
    pub Y: s15Fixed16Number,
    pub Z: s15Fixed16Number,
}

pub type qcms_data_type = libc::c_uint;
pub const QCMS_DATA_GRAYA_8: qcms_data_type = 4;
pub const QCMS_DATA_GRAY_8: qcms_data_type = 3;
pub const QCMS_DATA_BGRA_8: qcms_data_type = 2;
pub const QCMS_DATA_RGBA_8: qcms_data_type = 1;
pub const QCMS_DATA_RGB_8: qcms_data_type = 0;

#[repr(C)]
#[derive(Copy, Clone)]
pub struct CIE_XYZ {
    pub X: f64,
    pub Y: f64,
    pub Z: f64,
}

pub trait Format {
    const kRIndex: usize;
    const kGIndex: usize;
    const kBIndex: usize;
    const kAIndex: usize;
}

pub struct BGRA;
impl Format for BGRA {
    const kBIndex: usize = 0;
    const kGIndex: usize = 1;
    const kRIndex: usize = 2;
    const kAIndex: usize = 3;
}

pub struct RGBA;
impl Format for RGBA {
    const kRIndex: usize = 0;
    const kGIndex: usize = 1;
    const kBIndex: usize = 2;
    const kAIndex: usize = 3;
}

pub struct RGB;
impl Format for RGB {
    const kRIndex: usize = 0;
    const kGIndex: usize = 1;
    const kBIndex: usize = 2;
    const kAIndex: usize = 0xFF;
}

pub trait GrayFormat {
    const has_alpha: bool;
}

pub struct Gray;
impl GrayFormat for Gray {
    const has_alpha: bool = false;
}

pub struct GrayAlpha;
impl GrayFormat for GrayAlpha {
    const has_alpha: bool = true;
}

#[inline]
fn clamp_u8(mut v: f32) -> u8 {
    if v > 255. {
        return 255;
    } else if v < 0. {
        return 0;
    } else {
        return (v + 0.5).floor() as u8;
    };
}

// Build a White point, primary chromas transfer matrix from RGB to CIE XYZ
// This is just an approximation, I am not handling all the non-linear
// aspects of the RGB to XYZ process, and assumming that the gamma correction
// has transitive property in the tranformation chain.
//
// the alghoritm:
//
//            - First I build the absolute conversion matrix using
//              primaries in XYZ. This matrix is next inverted
//            - Then I eval the source white point across this matrix
//              obtaining the coeficients of the transformation
//            - Then, I apply these coeficients to the original matrix
fn build_RGB_to_XYZ_transfer_matrix(
    mut white: qcms_CIE_xyY,
    mut primrs: qcms_CIE_xyYTRIPLE,
) -> matrix {
    let mut primaries: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };

    let mut result: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    let mut white_point: vector = vector { v: [0.; 3] };

    let mut xn: f64 = white.x;
    let mut yn: f64 = white.y;
    if yn == 0.0f64 {
        return matrix_invalid();
    }

    let mut xr: f64 = primrs.red.x;
    let mut yr: f64 = primrs.red.y;
    let mut xg: f64 = primrs.green.x;
    let mut yg: f64 = primrs.green.y;
    let mut xb: f64 = primrs.blue.x;
    let mut yb: f64 = primrs.blue.y;
    primaries.m[0][0] = xr as f32;
    primaries.m[0][1] = xg as f32;
    primaries.m[0][2] = xb as f32;
    primaries.m[1][0] = yr as f32;
    primaries.m[1][1] = yg as f32;
    primaries.m[1][2] = yb as f32;
    primaries.m[2][0] = (1f64 - xr - yr) as f32;
    primaries.m[2][1] = (1f64 - xg - yg) as f32;
    primaries.m[2][2] = (1f64 - xb - yb) as f32;
    primaries.invalid = false;
    white_point.v[0] = (xn / yn) as f32;
    white_point.v[1] = 1.;
    white_point.v[2] = ((1.0f64 - xn - yn) / yn) as f32;
    let mut primaries_invert: matrix = matrix_invert(primaries);
    if primaries_invert.invalid {
        return matrix_invalid();
    }
    let mut coefs: vector = matrix_eval(primaries_invert, white_point);
    result.m[0][0] = (coefs.v[0] as f64 * xr) as f32;
    result.m[0][1] = (coefs.v[1] as f64 * xg) as f32;
    result.m[0][2] = (coefs.v[2] as f64 * xb) as f32;
    result.m[1][0] = (coefs.v[0] as f64 * yr) as f32;
    result.m[1][1] = (coefs.v[1] as f64 * yg) as f32;
    result.m[1][2] = (coefs.v[2] as f64 * yb) as f32;
    result.m[2][0] = (coefs.v[0] as f64 * (1.0f64 - xr - yr)) as f32;
    result.m[2][1] = (coefs.v[1] as f64 * (1.0f64 - xg - yg)) as f32;
    result.m[2][2] = (coefs.v[2] as f64 * (1.0f64 - xb - yb)) as f32;
    result.invalid = primaries_invert.invalid;
    return result;
}
/* CIE Illuminant D50 */
const D50_XYZ: CIE_XYZ = CIE_XYZ {
    X: 0.9642f64,
    Y: 1.0000f64,
    Z: 0.8249f64,
};
/* from lcms: xyY2XYZ()
 * corresponds to argyll: icmYxy2XYZ() */
fn xyY2XYZ(mut source: qcms_CIE_xyY) -> CIE_XYZ {
    let mut dest: CIE_XYZ = CIE_XYZ {
        X: 0.,
        Y: 0.,
        Z: 0.,
    };
    dest.X = source.x / source.y * source.Y;
    dest.Y = source.Y;
    dest.Z = (1f64 - source.x - source.y) / source.y * source.Y;
    return dest;
}
/* from lcms: ComputeChromaticAdaption */
// Compute chromatic adaption matrix using chad as cone matrix
fn compute_chromatic_adaption(
    mut source_white_point: CIE_XYZ,
    mut dest_white_point: CIE_XYZ,
    mut chad: matrix,
) -> matrix {
    let mut cone_source_XYZ: vector = vector { v: [0.; 3] };

    let mut cone_dest_XYZ: vector = vector { v: [0.; 3] };

    let mut cone: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };

    let mut tmp: matrix = chad;
    let mut chad_inv: matrix = matrix_invert(tmp);
    if chad_inv.invalid {
        return matrix_invalid();
    }
    cone_source_XYZ.v[0] = source_white_point.X as f32;
    cone_source_XYZ.v[1] = source_white_point.Y as f32;
    cone_source_XYZ.v[2] = source_white_point.Z as f32;
    cone_dest_XYZ.v[0] = dest_white_point.X as f32;
    cone_dest_XYZ.v[1] = dest_white_point.Y as f32;
    cone_dest_XYZ.v[2] = dest_white_point.Z as f32;

    let mut cone_source_rgb: vector = matrix_eval(chad, cone_source_XYZ);
    let mut cone_dest_rgb: vector = matrix_eval(chad, cone_dest_XYZ);
    cone.m[0][0] = cone_dest_rgb.v[0] / cone_source_rgb.v[0];
    cone.m[0][1] = 0.;
    cone.m[0][2] = 0.;
    cone.m[1][0] = 0.;
    cone.m[1][1] = cone_dest_rgb.v[1] / cone_source_rgb.v[1];
    cone.m[1][2] = 0.;
    cone.m[2][0] = 0.;
    cone.m[2][1] = 0.;
    cone.m[2][2] = cone_dest_rgb.v[2] / cone_source_rgb.v[2];
    cone.invalid = false;
    // Normalize
    return matrix_multiply(chad_inv, matrix_multiply(cone, chad));
}
/* from lcms: cmsAdaptionMatrix */
// Returns the final chrmatic adaptation from illuminant FromIll to Illuminant ToIll
// Bradford is assumed
fn adaption_matrix(mut source_illumination: CIE_XYZ, mut target_illumination: CIE_XYZ) -> matrix {
    let mut lam_rigg: matrix = {
        let mut init = matrix {
            m: [
                [0.8951, 0.2664, -0.1614],
                [-0.7502, 1.7135, 0.0367],
                [0.0389, -0.0685, 1.0296],
            ],
            invalid: false,
        };
        init
    };
    return compute_chromatic_adaption(source_illumination, target_illumination, lam_rigg);
}
/* from lcms: cmsAdaptMatrixToD50 */
fn adapt_matrix_to_D50(mut r: matrix, mut source_white_pt: qcms_CIE_xyY) -> matrix {
    if source_white_pt.y == 0.0f64 {
        return matrix_invalid();
    }

    let mut Dn: CIE_XYZ = xyY2XYZ(source_white_pt);
    let mut Bradford: matrix = adaption_matrix(Dn, D50_XYZ);
    if Bradford.invalid {
        return matrix_invalid();
    }
    return matrix_multiply(Bradford, r);
}
pub fn set_rgb_colorants(
    mut profile: &mut qcms_profile,
    mut white_point: qcms_CIE_xyY,
    mut primaries: qcms_CIE_xyYTRIPLE,
) -> bool {
    let mut colorants: matrix = build_RGB_to_XYZ_transfer_matrix(white_point, primaries);
    colorants = adapt_matrix_to_D50(colorants, white_point);
    if colorants.invalid {
        return false;
    }
    /* note: there's a transpose type of operation going on here */
    (*profile).redColorant.X = double_to_s15Fixed16Number(colorants.m[0][0] as f64);
    (*profile).redColorant.Y = double_to_s15Fixed16Number(colorants.m[1][0] as f64);
    (*profile).redColorant.Z = double_to_s15Fixed16Number(colorants.m[2][0] as f64);
    (*profile).greenColorant.X = double_to_s15Fixed16Number(colorants.m[0][1] as f64);
    (*profile).greenColorant.Y = double_to_s15Fixed16Number(colorants.m[1][1] as f64);
    (*profile).greenColorant.Z = double_to_s15Fixed16Number(colorants.m[2][1] as f64);
    (*profile).blueColorant.X = double_to_s15Fixed16Number(colorants.m[0][2] as f64);
    (*profile).blueColorant.Y = double_to_s15Fixed16Number(colorants.m[1][2] as f64);
    (*profile).blueColorant.Z = double_to_s15Fixed16Number(colorants.m[2][2] as f64);
    return true;
}
pub fn get_rgb_colorants(
    mut colorants: &mut matrix,
    mut white_point: qcms_CIE_xyY,
    mut primaries: qcms_CIE_xyYTRIPLE,
) -> bool {
    *colorants = build_RGB_to_XYZ_transfer_matrix(white_point, primaries);
    *colorants = adapt_matrix_to_D50(*colorants, white_point);
    return if (*colorants).invalid as i32 != 0 {
        1
    } else {
        0
    } != 0;
}
/* Alpha is not corrected.
   A rationale for this is found in Alvy Ray's "Should Alpha Be Nonlinear If
   RGB Is?" Tech Memo 17 (December 14, 1998).
    See: ftp://ftp.alvyray.com/Acrobat/17_Nonln.pdf
*/
unsafe extern "C" fn qcms_transform_data_gray_template_lut<I: GrayFormat, F: Format>(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    let components: libc::c_uint = if F::kAIndex == 0xff { 3 } else { 4 } as libc::c_uint;

    let mut i: libc::c_uint = 0;
    while (i as usize) < length {
        let fresh0 = src;
        src = src.offset(1);
        let mut device: libc::c_uchar = *fresh0;
        let mut alpha: libc::c_uchar = 0xffu8;
        if I::has_alpha {
            let fresh1 = src;
            src = src.offset(1);
            alpha = *fresh1
        }
        let mut linear: f32 = *(*transform).input_gamma_table_gray.offset(device as isize);

        let mut out_device_r: f32 = lut_interp_linear(
            linear as f64,
            (*transform).output_gamma_lut_r,
            (*transform).output_gamma_lut_r_length as i32,
        );
        let mut out_device_g: f32 = lut_interp_linear(
            linear as f64,
            (*transform).output_gamma_lut_g,
            (*transform).output_gamma_lut_g_length as i32,
        );
        let mut out_device_b: f32 = lut_interp_linear(
            linear as f64,
            (*transform).output_gamma_lut_b,
            (*transform).output_gamma_lut_b_length as i32,
        );
        *dest.offset(F::kRIndex as isize) = clamp_u8(out_device_r * 255f32);
        *dest.offset(F::kGIndex as isize) = clamp_u8(out_device_g * 255f32);
        *dest.offset(F::kBIndex as isize) = clamp_u8(out_device_b * 255f32);
        if F::kAIndex != 0xff {
            *dest.offset(F::kAIndex as isize) = alpha
        }
        dest = dest.offset(components as isize);
        i = i + 1
    }
}
unsafe extern "C" fn qcms_transform_data_gray_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_lut::<Gray, RGB>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_gray_rgba_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_lut::<Gray, RGBA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_gray_bgra_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_lut::<Gray, BGRA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_graya_rgba_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_lut::<GrayAlpha, RGBA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_graya_bgra_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_lut::<GrayAlpha, BGRA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_gray_template_precache<I: GrayFormat, F: Format>(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    let components: libc::c_uint = if F::kAIndex == 0xff { 3 } else { 4 } as libc::c_uint;
    let output_table_r = ((*transform).output_table_r).as_deref().unwrap();
    let output_table_g = ((*transform).output_table_g).as_deref().unwrap();
    let output_table_b = ((*transform).output_table_b).as_deref().unwrap();

    let mut i: libc::c_uint = 0;
    while (i as usize) < length {
        let fresh2 = src;
        src = src.offset(1);
        let mut device: libc::c_uchar = *fresh2;
        let mut alpha: libc::c_uchar = 0xffu8;
        if I::has_alpha {
            let fresh3 = src;
            src = src.offset(1);
            alpha = *fresh3
        }

        let mut linear: f32 = *(*transform).input_gamma_table_gray.offset(device as isize);
        /* we could round here... */
        let mut gray: u16 = (linear * (8192 - 1) as f32) as u16;
        *dest.offset(F::kRIndex as isize) = (output_table_r).data[gray as usize];
        *dest.offset(F::kGIndex as isize) = (output_table_g).data[gray as usize];
        *dest.offset(F::kBIndex as isize) = (output_table_b).data[gray as usize];
        if F::kAIndex != 0xff {
            *dest.offset(F::kAIndex as isize) = alpha
        }
        dest = dest.offset(components as isize);
        i = i + 1
    }
}
unsafe extern "C" fn qcms_transform_data_gray_out_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_precache::<Gray, RGB>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_gray_rgba_out_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_precache::<Gray, RGBA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_gray_bgra_out_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_precache::<Gray, BGRA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_graya_rgba_out_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_precache::<GrayAlpha, RGBA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_graya_bgra_out_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_gray_template_precache::<GrayAlpha, BGRA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_template_lut_precache<F: Format>(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    let components: libc::c_uint = if F::kAIndex == 0xff { 3 } else { 4 } as libc::c_uint;
    let output_table_r = ((*transform).output_table_r).as_deref().unwrap();
    let output_table_g = ((*transform).output_table_g).as_deref().unwrap();
    let output_table_b = ((*transform).output_table_b).as_deref().unwrap();

    let mut mat: *const [f32; 4] = (*transform).matrix.as_ptr();
    let mut i: libc::c_uint = 0;
    while (i as usize) < length {
        let mut device_r: libc::c_uchar = *src.offset(F::kRIndex as isize);
        let mut device_g: libc::c_uchar = *src.offset(F::kGIndex as isize);
        let mut device_b: libc::c_uchar = *src.offset(F::kBIndex as isize);
        let mut alpha: libc::c_uchar = 0;
        if F::kAIndex != 0xff {
            alpha = *src.offset(F::kAIndex as isize)
        }
        src = src.offset(components as isize);

        let mut linear_r: f32 = *(*transform).input_gamma_table_r.offset(device_r as isize);
        let mut linear_g: f32 = *(*transform).input_gamma_table_g.offset(device_g as isize);
        let mut linear_b: f32 = *(*transform).input_gamma_table_b.offset(device_b as isize);
        let mut out_linear_r: f32 = (*mat.offset(0isize))[0] * linear_r
            + (*mat.offset(1isize))[0] * linear_g
            + (*mat.offset(2isize))[0] * linear_b;
        let mut out_linear_g: f32 = (*mat.offset(0isize))[1] * linear_r
            + (*mat.offset(1isize))[1] * linear_g
            + (*mat.offset(2isize))[1] * linear_b;
        let mut out_linear_b: f32 = (*mat.offset(0isize))[2] * linear_r
            + (*mat.offset(1isize))[2] * linear_g
            + (*mat.offset(2isize))[2] * linear_b;
        out_linear_r = clamp_float(out_linear_r);
        out_linear_g = clamp_float(out_linear_g);
        out_linear_b = clamp_float(out_linear_b);
        /* we could round here... */

        let mut r: u16 = (out_linear_r * (8192 - 1) as f32) as u16;
        let mut g: u16 = (out_linear_g * (8192 - 1) as f32) as u16;
        let mut b: u16 = (out_linear_b * (8192 - 1) as f32) as u16;
        *dest.offset(F::kRIndex as isize) = (output_table_r).data[r as usize];
        *dest.offset(F::kGIndex as isize) = (output_table_g).data[g as usize];
        *dest.offset(F::kBIndex as isize) = (output_table_b).data[b as usize];
        if F::kAIndex != 0xff {
            *dest.offset(F::kAIndex as isize) = alpha
        }
        dest = dest.offset(components as isize);
        i = i + 1
    }
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data_rgb_out_lut_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut_precache::<RGB>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data_rgba_out_lut_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut_precache::<RGBA>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data_bgra_out_lut_precache(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut_precache::<BGRA>(transform, src, dest, length);
}
// Not used
/*
static void qcms_transform_data_clut(const qcms_transform *transform, const unsigned char *src, unsigned char *dest, size_t length) {
    unsigned int i;
    int xy_len = 1;
    int x_len = transform->grid_size;
    int len = x_len * x_len;
    const float* r_table = transform->r_clut;
    const float* g_table = transform->g_clut;
    const float* b_table = transform->b_clut;

    for (i = 0; i < length; i++) {
        unsigned char in_r = *src++;
        unsigned char in_g = *src++;
        unsigned char in_b = *src++;
        float linear_r = in_r/255.0f, linear_g=in_g/255.0f, linear_b = in_b/255.0f;

        int x = floorf(linear_r * (transform->grid_size-1));
        int y = floorf(linear_g * (transform->grid_size-1));
        int z = floorf(linear_b * (transform->grid_size-1));
        int x_n = ceilf(linear_r * (transform->grid_size-1));
        int y_n = ceilf(linear_g * (transform->grid_size-1));
        int z_n = ceilf(linear_b * (transform->grid_size-1));
        float x_d = linear_r * (transform->grid_size-1) - x;
        float y_d = linear_g * (transform->grid_size-1) - y;
        float z_d = linear_b * (transform->grid_size-1) - z;

        float r_x1 = lerp(CLU(r_table,x,y,z), CLU(r_table,x_n,y,z), x_d);
        float r_x2 = lerp(CLU(r_table,x,y_n,z), CLU(r_table,x_n,y_n,z), x_d);
        float r_y1 = lerp(r_x1, r_x2, y_d);
        float r_x3 = lerp(CLU(r_table,x,y,z_n), CLU(r_table,x_n,y,z_n), x_d);
        float r_x4 = lerp(CLU(r_table,x,y_n,z_n), CLU(r_table,x_n,y_n,z_n), x_d);
        float r_y2 = lerp(r_x3, r_x4, y_d);
        float clut_r = lerp(r_y1, r_y2, z_d);

        float g_x1 = lerp(CLU(g_table,x,y,z), CLU(g_table,x_n,y,z), x_d);
        float g_x2 = lerp(CLU(g_table,x,y_n,z), CLU(g_table,x_n,y_n,z), x_d);
        float g_y1 = lerp(g_x1, g_x2, y_d);
        float g_x3 = lerp(CLU(g_table,x,y,z_n), CLU(g_table,x_n,y,z_n), x_d);
        float g_x4 = lerp(CLU(g_table,x,y_n,z_n), CLU(g_table,x_n,y_n,z_n), x_d);
        float g_y2 = lerp(g_x3, g_x4, y_d);
        float clut_g = lerp(g_y1, g_y2, z_d);

        float b_x1 = lerp(CLU(b_table,x,y,z), CLU(b_table,x_n,y,z), x_d);
        float b_x2 = lerp(CLU(b_table,x,y_n,z), CLU(b_table,x_n,y_n,z), x_d);
        float b_y1 = lerp(b_x1, b_x2, y_d);
        float b_x3 = lerp(CLU(b_table,x,y,z_n), CLU(b_table,x_n,y,z_n), x_d);
        float b_x4 = lerp(CLU(b_table,x,y_n,z_n), CLU(b_table,x_n,y_n,z_n), x_d);
        float b_y2 = lerp(b_x3, b_x4, y_d);
        float clut_b = lerp(b_y1, b_y2, z_d);

        *dest++ = clamp_u8(clut_r*255.0f);
        *dest++ = clamp_u8(clut_g*255.0f);
        *dest++ = clamp_u8(clut_b*255.0f);
    }
}
*/
fn int_div_ceil(mut value: i32, mut div: i32) -> i32 {
    return (value + div - 1) / div;
}
// Using lcms' tetra interpolation algorithm.
unsafe extern "C" fn qcms_transform_data_tetra_clut_template<F: Format>(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    let components: libc::c_uint = if F::kAIndex == 0xff { 3 } else { 4 } as libc::c_uint;

    let mut xy_len: i32 = 1;
    let mut x_len: i32 = (*transform).grid_size as i32;
    let mut len: i32 = x_len * x_len;
    let mut r_table: *mut f32 = (*transform).r_clut;
    let mut g_table: *mut f32 = (*transform).g_clut;
    let mut b_table: *mut f32 = (*transform).b_clut;
    let mut c0_r: f32;
    let mut c1_r: f32;
    let mut c2_r: f32;
    let mut c3_r: f32;
    let mut c0_g: f32;
    let mut c1_g: f32;
    let mut c2_g: f32;
    let mut c3_g: f32;
    let mut c0_b: f32;
    let mut c1_b: f32;
    let mut c2_b: f32;
    let mut c3_b: f32;
    let mut clut_r: f32;
    let mut clut_g: f32;
    let mut clut_b: f32;
    let mut i: libc::c_uint = 0;
    while (i as usize) < length {
        let mut in_r: libc::c_uchar = *src.offset(F::kRIndex as isize);
        let mut in_g: libc::c_uchar = *src.offset(F::kGIndex as isize);
        let mut in_b: libc::c_uchar = *src.offset(F::kBIndex as isize);
        let mut in_a: libc::c_uchar = 0;
        if F::kAIndex != 0xff {
            in_a = *src.offset(F::kAIndex as isize)
        }
        src = src.offset(components as isize);
        let mut linear_r: f32 = in_r as i32 as f32 / 255.0;
        let mut linear_g: f32 = in_g as i32 as f32 / 255.0;
        let mut linear_b: f32 = in_b as i32 as f32 / 255.0;
        let mut x: i32 = in_r as i32 * ((*transform).grid_size as i32 - 1) / 255;
        let mut y: i32 = in_g as i32 * ((*transform).grid_size as i32 - 1) / 255;
        let mut z: i32 = in_b as i32 * ((*transform).grid_size as i32 - 1) / 255;
        let mut x_n: i32 = int_div_ceil(in_r as i32 * ((*transform).grid_size as i32 - 1), 255);
        let mut y_n: i32 = int_div_ceil(in_g as i32 * ((*transform).grid_size as i32 - 1), 255);
        let mut z_n: i32 = int_div_ceil(in_b as i32 * ((*transform).grid_size as i32 - 1), 255);
        let mut rx: f32 = linear_r * ((*transform).grid_size as i32 - 1) as f32 - x as f32;
        let mut ry: f32 = linear_g * ((*transform).grid_size as i32 - 1) as f32 - y as f32;
        let mut rz: f32 = linear_b * ((*transform).grid_size as i32 - 1) as f32 - z as f32;
        c0_r = *r_table.offset(((x * len + y * x_len + z * xy_len) * 3) as isize);
        c0_g = *g_table.offset(((x * len + y * x_len + z * xy_len) * 3) as isize);
        c0_b = *b_table.offset(((x * len + y * x_len + z * xy_len) * 3) as isize);
        if rx >= ry {
            if ry >= rz {
                //rx >= ry && ry >= rz
                c1_r = *r_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize) - c0_r; //rz > rx && rx >= ry
                c2_r = *r_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
                    - *r_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize);
                c3_r = *r_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *r_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize);
                c1_g = *g_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize) - c0_g;
                c2_g = *g_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
                    - *g_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize);
                c3_g = *g_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *g_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize);
                c1_b = *b_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize) - c0_b;
                c2_b = *b_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
                    - *b_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize);
                c3_b = *b_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *b_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
            } else if rx >= rz {
                //rx >= rz && rz >= ry
                c1_r = *r_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize) - c0_r;
                c2_r = *r_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *r_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize);
                c3_r = *r_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize)
                    - *r_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize);
                c1_g = *g_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize) - c0_g;
                c2_g = *g_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *g_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize);
                c3_g = *g_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize)
                    - *g_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize);
                c1_b = *b_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize) - c0_b;
                c2_b = *b_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *b_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize);
                c3_b = *b_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize)
                    - *b_table.offset(((x_n * len + y * x_len + z * xy_len) * 3) as isize)
            } else {
                c1_r = *r_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize)
                    - *r_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize);
                c2_r = *r_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *r_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize);
                c3_r = *r_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize) - c0_r;
                c1_g = *g_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize)
                    - *g_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize);
                c2_g = *g_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *g_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize);
                c3_g = *g_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize) - c0_g;
                c1_b = *b_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize)
                    - *b_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize);
                c2_b = *b_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                    - *b_table.offset(((x_n * len + y * x_len + z_n * xy_len) * 3) as isize);
                c3_b = *b_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize) - c0_b
            }
        } else if rx >= rz {
            //ry > rx && rx >= rz
            c1_r = *r_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
                - *r_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize); //rz > ry && ry > rx
            c2_r = *r_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize) - c0_r;
            c3_r = *r_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *r_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize);
            c1_g = *g_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
                - *g_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize);
            c2_g = *g_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize) - c0_g;
            c3_g = *g_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *g_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize);
            c1_b = *b_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
                - *b_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize);
            c2_b = *b_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize) - c0_b;
            c3_b = *b_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *b_table.offset(((x_n * len + y_n * x_len + z * xy_len) * 3) as isize)
        } else if ry >= rz {
            //ry >= rz && rz > rx
            c1_r = *r_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *r_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize);
            c2_r = *r_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize) - c0_r;
            c3_r = *r_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *r_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize);
            c1_g = *g_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *g_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize);
            c2_g = *g_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize) - c0_g;
            c3_g = *g_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *g_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize);
            c1_b = *b_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *b_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize);
            c2_b = *b_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize) - c0_b;
            c3_b = *b_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *b_table.offset(((x * len + y_n * x_len + z * xy_len) * 3) as isize)
        } else {
            c1_r = *r_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *r_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize);
            c2_r = *r_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *r_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize);
            c3_r = *r_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize) - c0_r;
            c1_g = *g_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *g_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize);
            c2_g = *g_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *g_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize);
            c3_g = *g_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize) - c0_g;
            c1_b = *b_table.offset(((x_n * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *b_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize);
            c2_b = *b_table.offset(((x * len + y_n * x_len + z_n * xy_len) * 3) as isize)
                - *b_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize);
            c3_b = *b_table.offset(((x * len + y * x_len + z_n * xy_len) * 3) as isize) - c0_b
        }
        clut_r = c0_r + c1_r * rx + c2_r * ry + c3_r * rz;
        clut_g = c0_g + c1_g * rx + c2_g * ry + c3_g * rz;
        clut_b = c0_b + c1_b * rx + c2_b * ry + c3_b * rz;
        *dest.offset(F::kRIndex as isize) = clamp_u8(clut_r * 255.0);
        *dest.offset(F::kGIndex as isize) = clamp_u8(clut_g * 255.0);
        *dest.offset(F::kBIndex as isize) = clamp_u8(clut_b * 255.0);
        if F::kAIndex != 0xff {
            *dest.offset(F::kAIndex as isize) = in_a
        }
        dest = dest.offset(components as isize);
        i = i + 1
    }
}
unsafe extern "C" fn qcms_transform_data_tetra_clut_rgb(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_tetra_clut_template::<RGB>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_tetra_clut_rgba(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_tetra_clut_template::<RGBA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_tetra_clut_bgra(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_tetra_clut_template::<BGRA>(transform, src, dest, length);
}
unsafe extern "C" fn qcms_transform_data_template_lut<F: Format>(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    let components: libc::c_uint = if F::kAIndex == 0xff { 3 } else { 4 } as libc::c_uint;

    let mut mat: *const [f32; 4] = (*transform).matrix.as_ptr();
    let mut i: libc::c_uint = 0;
    while (i as usize) < length {
        let mut device_r: libc::c_uchar = *src.offset(F::kRIndex as isize);
        let mut device_g: libc::c_uchar = *src.offset(F::kGIndex as isize);
        let mut device_b: libc::c_uchar = *src.offset(F::kBIndex as isize);
        let mut alpha: libc::c_uchar = 0;
        if F::kAIndex != 0xff {
            alpha = *src.offset(F::kAIndex as isize)
        }
        src = src.offset(components as isize);

        let mut linear_r: f32 = *(*transform).input_gamma_table_r.offset(device_r as isize);
        let mut linear_g: f32 = *(*transform).input_gamma_table_g.offset(device_g as isize);
        let mut linear_b: f32 = *(*transform).input_gamma_table_b.offset(device_b as isize);
        let mut out_linear_r: f32 = (*mat.offset(0isize))[0] * linear_r
            + (*mat.offset(1isize))[0] * linear_g
            + (*mat.offset(2isize))[0] * linear_b;
        let mut out_linear_g: f32 = (*mat.offset(0isize))[1] * linear_r
            + (*mat.offset(1isize))[1] * linear_g
            + (*mat.offset(2isize))[1] * linear_b;
        let mut out_linear_b: f32 = (*mat.offset(0isize))[2] * linear_r
            + (*mat.offset(1isize))[2] * linear_g
            + (*mat.offset(2isize))[2] * linear_b;
        out_linear_r = clamp_float(out_linear_r);
        out_linear_g = clamp_float(out_linear_g);
        out_linear_b = clamp_float(out_linear_b);

        let mut out_device_r: f32 = lut_interp_linear(
            out_linear_r as f64,
            (*transform).output_gamma_lut_r,
            (*transform).output_gamma_lut_r_length as i32,
        );
        let mut out_device_g: f32 = lut_interp_linear(
            out_linear_g as f64,
            (*transform).output_gamma_lut_g,
            (*transform).output_gamma_lut_g_length as i32,
        );
        let mut out_device_b: f32 = lut_interp_linear(
            out_linear_b as f64,
            (*transform).output_gamma_lut_b,
            (*transform).output_gamma_lut_b_length as i32,
        );
        *dest.offset(F::kRIndex as isize) = clamp_u8(out_device_r * 255f32);
        *dest.offset(F::kGIndex as isize) = clamp_u8(out_device_g * 255f32);
        *dest.offset(F::kBIndex as isize) = clamp_u8(out_device_b * 255f32);
        if F::kAIndex != 0xff {
            *dest.offset(F::kAIndex as isize) = alpha
        }
        dest = dest.offset(components as isize);
        i = i + 1
    }
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data_rgb_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut::<RGB>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data_rgba_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut::<RGBA>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data_bgra_out_lut(
    mut transform: *const qcms_transform,
    mut src: *const libc::c_uchar,
    mut dest: *mut libc::c_uchar,
    mut length: usize,
) {
    qcms_transform_data_template_lut::<BGRA>(transform, src, dest, length);
}

fn precache_create() -> Arc<precache_output> {
    Arc::new(precache_output::default())
}

unsafe extern "C" fn transform_alloc() -> *mut qcms_transform {
    Box::into_raw(Box::new(Default::default()))
}
unsafe extern "C" fn transform_free(mut t: *mut qcms_transform) {
    drop(Box::from_raw(t))
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_release(mut t: *mut qcms_transform) {
    /* ensure we only free the gamma tables once even if there are
     * multiple references to the same data */
    (*t).output_table_r = None;
    (*t).output_table_g = None;
    (*t).output_table_b = None;

    free((*t).input_gamma_table_r as *mut libc::c_void);
    if (*t).input_gamma_table_g != (*t).input_gamma_table_r {
        free((*t).input_gamma_table_g as *mut libc::c_void);
    }
    if (*t).input_gamma_table_g != (*t).input_gamma_table_r
        && (*t).input_gamma_table_g != (*t).input_gamma_table_b
    {
        free((*t).input_gamma_table_b as *mut libc::c_void);
    }
    free((*t).input_gamma_table_gray as *mut libc::c_void);
    free((*t).output_gamma_lut_r as *mut libc::c_void);
    free((*t).output_gamma_lut_g as *mut libc::c_void);
    free((*t).output_gamma_lut_b as *mut libc::c_void);
    /* r_clut points to beginning of buffer allocated in qcms_transform_precacheLUT_float */
    if !(*t).r_clut.is_null() {
        free((*t).r_clut as *mut libc::c_void);
    }
    transform_free(t);
}

fn sse_version_available() -> i32 {
    /* we know at build time that 64-bit CPUs always have SSE2
     * this tells the compiler that non-SSE2 branches will never be
     * taken (i.e. OK to optimze away the SSE1 and non-SIMD code */
    return 2;
}
const bradford_matrix: matrix = matrix {
    m: [
        [0.8951, 0.2664, -0.1614],
        [-0.7502, 1.7135, 0.0367],
        [0.0389, -0.0685, 1.0296],
    ],
    invalid: false,
};

const bradford_matrix_inv: matrix = matrix {
    m: [
        [0.9869929, -0.1470543, 0.1599627],
        [0.4323053, 0.5183603, 0.0492912],
        [-0.0085287, 0.0400428, 0.9684867],
    ],
    invalid: false,
};

// See ICCv4 E.3
fn compute_whitepoint_adaption(mut X: f32, mut Y: f32, mut Z: f32) -> matrix {
    let mut p: f32 = (0.96422 * bradford_matrix.m[0][0]
        + 1.000 * bradford_matrix.m[1][0]
        + 0.82521 * bradford_matrix.m[2][0])
        / (X * bradford_matrix.m[0][0] + Y * bradford_matrix.m[1][0] + Z * bradford_matrix.m[2][0]);
    let mut y: f32 = (0.96422 * bradford_matrix.m[0][1]
        + 1.000 * bradford_matrix.m[1][1]
        + 0.82521 * bradford_matrix.m[2][1])
        / (X * bradford_matrix.m[0][1] + Y * bradford_matrix.m[1][1] + Z * bradford_matrix.m[2][1]);
    let mut b: f32 = (0.96422 * bradford_matrix.m[0][2]
        + 1.000 * bradford_matrix.m[1][2]
        + 0.82521 * bradford_matrix.m[2][2])
        / (X * bradford_matrix.m[0][2] + Y * bradford_matrix.m[1][2] + Z * bradford_matrix.m[2][2]);
    let mut white_adaption: matrix = {
        let mut init = matrix {
            m: [[p, 0., 0.], [0., y, 0.], [0., 0., b]],
            invalid: false,
        };
        init
    };
    return matrix_multiply(
        bradford_matrix_inv,
        matrix_multiply(white_adaption, bradford_matrix),
    );
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_precache_output_transform(mut profile: *mut qcms_profile) {
    /* we only support precaching on rgb profiles */
    if (*profile).color_space != 0x52474220 {
        return;
    }
    if qcms_supports_iccv4.load(Ordering::Relaxed) {
        /* don't precache since we will use the B2A LUT */
        if !(*profile).B2A0.is_none() {
            return;
        }
        /* don't precache since we will use the mBA LUT */
        if !(*profile).mBA.is_none() {
            return;
        }
    }
    /* don't precache if we do not have the TRC curves */
    if (*profile).redTRC.is_none() || (*profile).greenTRC.is_none() || (*profile).blueTRC.is_none()
    {
        return;
    }
    if (*profile).output_table_r.is_none() {
        let mut output_table_r = precache_create();
        if compute_precache(
            (*profile).redTRC.as_deref().unwrap(),
            Arc::get_mut(&mut output_table_r).unwrap().data.as_mut_ptr(),
        ) {
            (*profile).output_table_r = Some(output_table_r);
        }
    }
    if (*profile).output_table_g.is_none() {
        let mut output_table_g = precache_create();
        if compute_precache(
            (*profile).greenTRC.as_deref().unwrap(),
            Arc::get_mut(&mut output_table_g).unwrap().data.as_mut_ptr(),
        ) {
            (*profile).output_table_g = Some(output_table_g);
        }
    }
    if (*profile).output_table_b.is_none() {
        let mut output_table_b = precache_create();
        if compute_precache(
            (*profile).blueTRC.as_deref().unwrap(),
            Arc::get_mut(&mut output_table_b).unwrap().data.as_mut_ptr(),
        ) {
            (*profile).output_table_b = Some(output_table_b);
        }
    };
}
/* Replace the current transformation with a LUT transformation using a given number of sample points */
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_precacheLUT_float(
    mut transform: *mut qcms_transform,
    mut in_0: &qcms_profile,
    mut out: &qcms_profile,
    mut samples: i32,
    mut in_type: qcms_data_type,
) -> *mut qcms_transform {
    /* The range between which 2 consecutive sample points can be used to interpolate */
    let mut x: u16;
    let mut y: u16;
    let mut z: u16;
    let mut l: u32;
    let mut lutSize: u32 = (3 * samples * samples * samples) as u32;

    let mut lut: *mut f32 = 0 as *mut f32;

    let mut src: *mut f32 = malloc(lutSize as usize * ::std::mem::size_of::<f32>()) as *mut f32;
    let mut dest: *mut f32 = malloc(lutSize as usize * ::std::mem::size_of::<f32>()) as *mut f32;
    if !src.is_null() && !dest.is_null() {
        /* Prepare a list of points we want to sample */
        l = 0;
        x = 0u16;
        while (x as i32) < samples {
            y = 0u16;
            while (y as i32) < samples {
                z = 0u16;
                while (z as i32) < samples {
                    let fresh8 = l;
                    l = l + 1;
                    *src.offset(fresh8 as isize) = x as i32 as f32 / (samples - 1) as f32;
                    let fresh9 = l;
                    l = l + 1;
                    *src.offset(fresh9 as isize) = y as i32 as f32 / (samples - 1) as f32;
                    let fresh10 = l;
                    l = l + 1;
                    *src.offset(fresh10 as isize) = z as i32 as f32 / (samples - 1) as f32;
                    z = z + 1
                }
                y = y + 1
            }
            x = x + 1
        }
        lut = qcms_chain_transform(in_0, out, src, dest, lutSize as usize);
        if !lut.is_null() {
            (*transform).r_clut = &mut *lut.offset(0isize) as *mut f32;
            (*transform).g_clut = &mut *lut.offset(1isize) as *mut f32;
            (*transform).b_clut = &mut *lut.offset(2isize) as *mut f32;
            (*transform).grid_size = samples as u16;
            if in_type == QCMS_DATA_RGBA_8 {
                (*transform).transform_fn = Some(qcms_transform_data_tetra_clut_rgba)
            } else if in_type == QCMS_DATA_BGRA_8 {
                (*transform).transform_fn = Some(qcms_transform_data_tetra_clut_bgra)
            } else if in_type == QCMS_DATA_RGB_8 {
                (*transform).transform_fn = Some(qcms_transform_data_tetra_clut_rgb)
            }
            debug_assert!((*transform).transform_fn.is_some());
        }
    }
    //XXX: qcms_modular_transform_data may return either the src or dest buffer. If so it must not be free-ed
    // It will be stored in r_clut, which will be cleaned up in qcms_transform_release.
    if !src.is_null() && lut != src {
        free(src as *mut libc::c_void);
    }
    if !dest.is_null() && lut != dest {
        free(dest as *mut libc::c_void);
    }
    if lut.is_null() {
        return 0 as *mut qcms_transform;
    }
    return transform;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_create(
    mut in_0: &qcms_profile,
    mut in_type: qcms_data_type,
    mut out: &qcms_profile,
    mut out_type: qcms_data_type,
    mut intent: qcms_intent,
) -> *mut qcms_transform {
    // Ensure the requested input and output types make sense.
    let mut match_0: bool = false;
    if in_type == QCMS_DATA_RGB_8 {
        match_0 = out_type == QCMS_DATA_RGB_8
    } else if in_type == QCMS_DATA_RGBA_8 {
        match_0 = out_type == QCMS_DATA_RGBA_8
    } else if in_type == QCMS_DATA_BGRA_8 {
        match_0 = out_type == QCMS_DATA_BGRA_8
    } else if in_type == QCMS_DATA_GRAY_8 {
        match_0 = out_type == QCMS_DATA_RGB_8
            || out_type == QCMS_DATA_RGBA_8
            || out_type == QCMS_DATA_BGRA_8
    } else if in_type == QCMS_DATA_GRAYA_8 {
        match_0 = out_type == QCMS_DATA_RGBA_8 || out_type == QCMS_DATA_BGRA_8
    }
    if !match_0 {
        debug_assert!(false, "input/output type");
        return 0 as *mut qcms_transform;
    }
    let mut transform: *mut qcms_transform = transform_alloc();
    if transform.is_null() {
        return 0 as *mut qcms_transform;
    }
    let mut precache: bool = false;
    if !(*out).output_table_r.is_none()
        && !(*out).output_table_g.is_none()
        && !(*out).output_table_b.is_none()
    {
        precache = true
    }
    // This precache assumes RGB_SIGNATURE (fails on GRAY_SIGNATURE, for instance)
    if qcms_supports_iccv4.load(Ordering::Relaxed) as i32 != 0
        && (in_type == QCMS_DATA_RGB_8
            || in_type == QCMS_DATA_RGBA_8
            || in_type == QCMS_DATA_BGRA_8)
        && (!(*in_0).A2B0.is_none()
            || !(*out).B2A0.is_none()
            || !(*in_0).mAB.is_none()
            || !(*out).mAB.is_none())
    {
        // Precache the transformation to a CLUT 33x33x33 in size.
        // 33 is used by many profiles and works well in pratice.
        // This evenly divides 256 into blocks of 8x8x8.
        // TODO For transforming small data sets of about 200x200 or less
        // precaching should be avoided.
        let mut result: *mut qcms_transform =
            qcms_transform_precacheLUT_float(transform, in_0, out, 33, in_type);
        if result.is_null() {
            debug_assert!(false, "precacheLUT failed");
            qcms_transform_release(transform);
            return 0 as *mut qcms_transform;
        }
        return result;
    }
    if precache {
        (*transform).output_table_r = Some(Arc::clone((*out).output_table_r.as_ref().unwrap()));
        (*transform).output_table_g = Some(Arc::clone((*out).output_table_g.as_ref().unwrap()));
        (*transform).output_table_b = Some(Arc::clone((*out).output_table_b.as_ref().unwrap()));
    } else {
        if (*out).redTRC.is_none() || (*out).greenTRC.is_none() || (*out).blueTRC.is_none() {
            qcms_transform_release(transform);
            return 0 as *mut qcms_transform;
        }
        build_output_lut(
            (*out).redTRC.as_deref().unwrap(),
            &mut (*transform).output_gamma_lut_r,
            &mut (*transform).output_gamma_lut_r_length,
        );
        build_output_lut(
            (*out).greenTRC.as_deref().unwrap(),
            &mut (*transform).output_gamma_lut_g,
            &mut (*transform).output_gamma_lut_g_length,
        );
        build_output_lut(
            (*out).blueTRC.as_deref().unwrap(),
            &mut (*transform).output_gamma_lut_b,
            &mut (*transform).output_gamma_lut_b_length,
        );
        if (*transform).output_gamma_lut_r.is_null()
            || (*transform).output_gamma_lut_g.is_null()
            || (*transform).output_gamma_lut_b.is_null()
        {
            qcms_transform_release(transform);
            return 0 as *mut qcms_transform;
        }
    }
    if (*in_0).color_space == 0x52474220 {
        if precache {
            if cfg!(any(target_arch = "x86", target_arch = "x86_64")) && qcms_supports_avx {
                #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
                {
                    if in_type == QCMS_DATA_RGB_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_rgb_out_lut_avx)
                    } else if in_type == QCMS_DATA_RGBA_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_rgba_out_lut_avx)
                    } else if in_type == QCMS_DATA_BGRA_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_bgra_out_lut_avx)
                    }
                }
            } else if cfg!(all(
                any(target_arch = "x86", target_arch = "x86_64"),
                not(miri)
            )) && sse_version_available() >= 2
            {
                #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
                {
                    if in_type == QCMS_DATA_RGB_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_rgb_out_lut_sse2)
                    } else if in_type == QCMS_DATA_RGBA_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_rgba_out_lut_sse2)
                    } else if in_type == QCMS_DATA_BGRA_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_bgra_out_lut_sse2)
                    }
                }
            } else if cfg!(any(target_arch = "arm", target_arch = "aarch64")) && qcms_supports_neon
            {
                #[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
                {
                    if in_type == QCMS_DATA_RGB_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_rgb_out_lut_neon)
                    } else if in_type == QCMS_DATA_RGBA_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_rgba_out_lut_neon)
                    } else if in_type == QCMS_DATA_BGRA_8 {
                        (*transform).transform_fn = Some(qcms_transform_data_bgra_out_lut_neon)
                    }
                }
            } else if in_type == QCMS_DATA_RGB_8 {
                (*transform).transform_fn = Some(qcms_transform_data_rgb_out_lut_precache)
            } else if in_type == QCMS_DATA_RGBA_8 {
                (*transform).transform_fn = Some(qcms_transform_data_rgba_out_lut_precache)
            } else if in_type == QCMS_DATA_BGRA_8 {
                (*transform).transform_fn = Some(qcms_transform_data_bgra_out_lut_precache)
            }
        } else if in_type == QCMS_DATA_RGB_8 {
            (*transform).transform_fn = Some(qcms_transform_data_rgb_out_lut)
        } else if in_type == QCMS_DATA_RGBA_8 {
            (*transform).transform_fn = Some(qcms_transform_data_rgba_out_lut)
        } else if in_type == QCMS_DATA_BGRA_8 {
            (*transform).transform_fn = Some(qcms_transform_data_bgra_out_lut)
        }
        //XXX: avoid duplicating tables if we can
        (*transform).input_gamma_table_r = build_input_gamma_table((*in_0).redTRC.as_deref());
        (*transform).input_gamma_table_g = build_input_gamma_table((*in_0).greenTRC.as_deref());
        (*transform).input_gamma_table_b = build_input_gamma_table((*in_0).blueTRC.as_deref());
        if (*transform).input_gamma_table_r.is_null()
            || (*transform).input_gamma_table_g.is_null()
            || (*transform).input_gamma_table_b.is_null()
        {
            qcms_transform_release(transform);
            return 0 as *mut qcms_transform;
        }
        /* build combined colorant matrix */

        let mut in_matrix: matrix = build_colorant_matrix(in_0);
        let mut out_matrix: matrix = build_colorant_matrix(out);
        out_matrix = matrix_invert(out_matrix);
        if out_matrix.invalid {
            qcms_transform_release(transform);
            return 0 as *mut qcms_transform;
        }
        let mut result_0: matrix = matrix_multiply(out_matrix, in_matrix);
        /* check for NaN values in the matrix and bail if we find any */
        let mut i: libc::c_uint = 0;
        while i < 3 {
            let mut j: libc::c_uint = 0;
            while j < 3 {
                if result_0.m[i as usize][j as usize] != result_0.m[i as usize][j as usize] {
                    qcms_transform_release(transform);
                    return 0 as *mut qcms_transform;
                }
                j = j + 1
            }
            i = i + 1
        }
        /* store the results in column major mode
         * this makes doing the multiplication with sse easier */
        (*transform).matrix[0][0] = result_0.m[0][0];
        (*transform).matrix[1][0] = result_0.m[0][1];
        (*transform).matrix[2][0] = result_0.m[0][2];
        (*transform).matrix[0][1] = result_0.m[1][0];
        (*transform).matrix[1][1] = result_0.m[1][1];
        (*transform).matrix[2][1] = result_0.m[1][2];
        (*transform).matrix[0][2] = result_0.m[2][0];
        (*transform).matrix[1][2] = result_0.m[2][1];
        (*transform).matrix[2][2] = result_0.m[2][2]
    } else if (*in_0).color_space == 0x47524159 {
        (*transform).input_gamma_table_gray = build_input_gamma_table((*in_0).grayTRC.as_deref());
        if (*transform).input_gamma_table_gray.is_null() {
            qcms_transform_release(transform);
            return 0 as *mut qcms_transform;
        }
        if precache {
            if out_type == QCMS_DATA_RGB_8 {
                (*transform).transform_fn = Some(qcms_transform_data_gray_out_precache)
            } else if out_type == QCMS_DATA_RGBA_8 {
                if in_type == QCMS_DATA_GRAY_8 {
                    (*transform).transform_fn = Some(qcms_transform_data_gray_rgba_out_precache)
                } else {
                    (*transform).transform_fn = Some(qcms_transform_data_graya_rgba_out_precache)
                }
            } else if out_type == QCMS_DATA_BGRA_8 {
                if in_type == QCMS_DATA_GRAY_8 {
                    (*transform).transform_fn = Some(qcms_transform_data_gray_bgra_out_precache)
                } else {
                    (*transform).transform_fn = Some(qcms_transform_data_graya_bgra_out_precache)
                }
            }
        } else if out_type == QCMS_DATA_RGB_8 {
            (*transform).transform_fn = Some(qcms_transform_data_gray_out_lut)
        } else if out_type == QCMS_DATA_RGBA_8 {
            if in_type == QCMS_DATA_GRAY_8 {
                (*transform).transform_fn = Some(qcms_transform_data_gray_rgba_out_lut)
            } else {
                (*transform).transform_fn = Some(qcms_transform_data_graya_rgba_out_lut)
            }
        } else if out_type == QCMS_DATA_BGRA_8 {
            if in_type == QCMS_DATA_GRAY_8 {
                (*transform).transform_fn = Some(qcms_transform_data_gray_bgra_out_lut)
            } else {
                (*transform).transform_fn = Some(qcms_transform_data_graya_bgra_out_lut)
            }
        }
    } else {
        debug_assert!(false, "unexpected colorspace");
        qcms_transform_release(transform);
        return 0 as *mut qcms_transform;
    }
    debug_assert!((*transform).transform_fn.is_some());
    return transform;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data(
    mut transform: *mut qcms_transform,
    mut src: *const libc::c_void,
    mut dest: *mut libc::c_void,
    mut length: usize,
) {
    (*transform)
        .transform_fn
        .expect("non-null function pointer")(
        transform,
        src as *const libc::c_uchar,
        dest as *mut libc::c_uchar,
        length,
    );
}

#[no_mangle]
pub unsafe extern "C" fn qcms_enable_iccv4() {
    qcms_supports_iccv4.store(true, Ordering::Relaxed);
}
#[no_mangle]
pub static mut qcms_supports_avx: bool = false;
#[no_mangle]
pub static mut qcms_supports_neon: bool = false;
#[no_mangle]
pub unsafe extern "C" fn qcms_enable_avx() {
    qcms_supports_avx = true;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_enable_neon() {
    qcms_supports_neon = true;
}

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

#[cfg(all(any(target_arch = "arm", target_arch = "aarch64"), feature = "neon"))]
use crate::transform_neon::{
    qcms_transform_data_bgra_out_lut_neon, qcms_transform_data_rgb_out_lut_neon,
    qcms_transform_data_rgba_out_lut_neon,
};
use crate::{
    chain::chain_transform,
    double_to_s15Fixed16Number,
    iccread::SUPPORTS_ICCV4,
    matrix::*,
    transform_util::{
        build_colorant_matrix, build_input_gamma_table, build_output_lut, compute_precache,
        lut_interp_linear,
    },
};
use crate::{
    iccread::{qcms_CIE_xyY, qcms_CIE_xyYTRIPLE, Profile, GRAY_SIGNATURE, RGB_SIGNATURE},
    transform_util::clamp_float,
    Intent,
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

use std::sync::atomic::Ordering;
use std::sync::Arc;

pub const PRECACHE_OUTPUT_SIZE: usize = 8192;
pub const PRECACHE_OUTPUT_MAX: usize = PRECACHE_OUTPUT_SIZE - 1;
pub const FLOATSCALE: f32 = PRECACHE_OUTPUT_SIZE as f32;
pub const CLAMPMAXVAL: f32 = ((PRECACHE_OUTPUT_SIZE - 1) as f32) / PRECACHE_OUTPUT_SIZE as f32;

#[repr(C)]
pub struct PrecacheOuput {
    /* We previously used a count of 65536 here but that seems like more
     * precision than we actually need.  By reducing the size we can
     * improve startup performance and reduce memory usage. ColorSync on
     * 10.5 uses 4097 which is perhaps because they use a fixed point
     * representation where 1. is represented by 0x1000. */
    pub data: [u8; PRECACHE_OUTPUT_SIZE],
}

impl Default for PrecacheOuput {
    fn default() -> PrecacheOuput {
        PrecacheOuput {
            data: [0; PRECACHE_OUTPUT_SIZE],
        }
    }
}

/* used as a lookup table for the output transformation.
 * we refcount them so we only need to have one around per output
 * profile, instead of duplicating them per transform */

#[repr(C)]
#[repr(align(16))]
#[derive(Clone, Default)]
pub struct qcms_transform {
    pub matrix: [[f32; 4]; 3],
    pub input_gamma_table_r: Option<Box<[f32; 256]>>,
    pub input_gamma_table_g: Option<Box<[f32; 256]>>,
    pub input_gamma_table_b: Option<Box<[f32; 256]>>,
    pub input_clut_table_length: u16,
    pub clut: Option<Vec<f32>>,
    pub grid_size: u16,
    pub output_clut_table_length: u16,
    pub input_gamma_table_gray: Option<Box<[f32; 256]>>,
    pub out_gamma_r: f32,
    pub out_gamma_g: f32,
    pub out_gamma_b: f32,
    pub out_gamma_gray: f32,
    pub output_gamma_lut_r: Option<Vec<u16>>,
    pub output_gamma_lut_g: Option<Vec<u16>>,
    pub output_gamma_lut_b: Option<Vec<u16>>,
    pub output_gamma_lut_gray: Option<Vec<u16>>,
    pub output_gamma_lut_r_length: usize,
    pub output_gamma_lut_g_length: usize,
    pub output_gamma_lut_b_length: usize,
    pub output_gamma_lut_gray_length: usize,
    pub output_table_r: Option<Arc<PrecacheOuput>>,
    pub output_table_g: Option<Arc<PrecacheOuput>>,
    pub output_table_b: Option<Arc<PrecacheOuput>>,
    pub transform_fn: transform_fn_t,
}

pub type transform_fn_t =
    Option<unsafe fn(_: &qcms_transform, _: *const u8, _: *mut u8, _: usize) -> ()>;
/// The format of pixel data
#[repr(u32)]
#[derive(PartialEq, Eq, Clone, Copy)]
pub enum DataType {
    RGB8 = 0,
    RGBA8 = 1,
    BGRA8 = 2,
    Gray8 = 3,
    GrayA8 = 4,
}

impl DataType {
    pub fn bytes_per_pixel(&self) -> usize {
        match self {
            RGB8 => 3,
            RGBA8 => 4,
            BGRA8 => 4,
            Gray8 => 1,
            GrayA8 => 2,
        }
    }
}

use DataType::*;

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
fn clamp_u8(v: f32) -> u8 {
    if v > 255. {
        255
    } else if v < 0. {
        0
    } else {
        (v + 0.5).floor() as u8
    }
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
fn build_RGB_to_XYZ_transfer_matrix(white: qcms_CIE_xyY, primrs: qcms_CIE_xyYTRIPLE) -> Matrix {
    let mut primaries: Matrix = Matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };

    let mut result: Matrix = Matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    let mut white_point: Vector = Vector { v: [0.; 3] };

    let xn: f64 = white.x;
    let yn: f64 = white.y;
    if yn == 0.0f64 {
        return Matrix::invalid();
    }

    let xr: f64 = primrs.red.x;
    let yr: f64 = primrs.red.y;
    let xg: f64 = primrs.green.x;
    let yg: f64 = primrs.green.y;
    let xb: f64 = primrs.blue.x;
    let yb: f64 = primrs.blue.y;
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
    let primaries_invert: Matrix = primaries.invert();
    if primaries_invert.invalid {
        return Matrix::invalid();
    }
    let coefs: Vector = primaries_invert.eval(white_point);
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
    result
}
/* CIE Illuminant D50 */
const D50_XYZ: CIE_XYZ = CIE_XYZ {
    X: 0.9642f64,
    Y: 1.0000f64,
    Z: 0.8249f64,
};
/* from lcms: xyY2XYZ()
 * corresponds to argyll: icmYxy2XYZ() */
fn xyY2XYZ(source: qcms_CIE_xyY) -> CIE_XYZ {
    let mut dest: CIE_XYZ = CIE_XYZ {
        X: 0.,
        Y: 0.,
        Z: 0.,
    };
    dest.X = source.x / source.y * source.Y;
    dest.Y = source.Y;
    dest.Z = (1f64 - source.x - source.y) / source.y * source.Y;
    dest
}
/* from lcms: ComputeChromaticAdaption */
// Compute chromatic adaption matrix using chad as cone matrix
fn compute_chromatic_adaption(
    source_white_point: CIE_XYZ,
    dest_white_point: CIE_XYZ,
    chad: Matrix,
) -> Matrix {
    let mut cone_source_XYZ: Vector = Vector { v: [0.; 3] };

    let mut cone_dest_XYZ: Vector = Vector { v: [0.; 3] };

    let mut cone: Matrix = Matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };

    let tmp: Matrix = chad;
    let chad_inv: Matrix = tmp.invert();
    if chad_inv.invalid {
        return Matrix::invalid();
    }
    cone_source_XYZ.v[0] = source_white_point.X as f32;
    cone_source_XYZ.v[1] = source_white_point.Y as f32;
    cone_source_XYZ.v[2] = source_white_point.Z as f32;
    cone_dest_XYZ.v[0] = dest_white_point.X as f32;
    cone_dest_XYZ.v[1] = dest_white_point.Y as f32;
    cone_dest_XYZ.v[2] = dest_white_point.Z as f32;

    let cone_source_rgb: Vector = chad.eval(cone_source_XYZ);
    let cone_dest_rgb: Vector = chad.eval(cone_dest_XYZ);
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
    Matrix::multiply(chad_inv, Matrix::multiply(cone, chad))
}
/* from lcms: cmsAdaptionMatrix */
// Returns the final chrmatic adaptation from illuminant FromIll to Illuminant ToIll
// Bradford is assumed
fn adaption_matrix(source_illumination: CIE_XYZ, target_illumination: CIE_XYZ) -> Matrix {
    let lam_rigg: Matrix = {
        let init = Matrix {
            m: [
                [0.8951, 0.2664, -0.1614],
                [-0.7502, 1.7135, 0.0367],
                [0.0389, -0.0685, 1.0296],
            ],
            invalid: false,
        };
        init
    };
    compute_chromatic_adaption(source_illumination, target_illumination, lam_rigg)
}
/* from lcms: cmsAdaptMatrixToD50 */
fn adapt_matrix_to_D50(r: Matrix, source_white_pt: qcms_CIE_xyY) -> Matrix {
    if source_white_pt.y == 0.0f64 {
        return Matrix::invalid();
    }

    let Dn: CIE_XYZ = xyY2XYZ(source_white_pt);
    let Bradford: Matrix = adaption_matrix(Dn, D50_XYZ);
    if Bradford.invalid {
        return Matrix::invalid();
    }
    Matrix::multiply(Bradford, r)
}
pub(crate) fn set_rgb_colorants(
    mut profile: &mut Profile,
    white_point: qcms_CIE_xyY,
    primaries: qcms_CIE_xyYTRIPLE,
) -> bool {
    let mut colorants: Matrix = build_RGB_to_XYZ_transfer_matrix(white_point, primaries);
    colorants = adapt_matrix_to_D50(colorants, white_point);
    if colorants.invalid {
        return false;
    }
    /* note: there's a transpose type of operation going on here */
    profile.redColorant.X = double_to_s15Fixed16Number(colorants.m[0][0] as f64);
    profile.redColorant.Y = double_to_s15Fixed16Number(colorants.m[1][0] as f64);
    profile.redColorant.Z = double_to_s15Fixed16Number(colorants.m[2][0] as f64);
    profile.greenColorant.X = double_to_s15Fixed16Number(colorants.m[0][1] as f64);
    profile.greenColorant.Y = double_to_s15Fixed16Number(colorants.m[1][1] as f64);
    profile.greenColorant.Z = double_to_s15Fixed16Number(colorants.m[2][1] as f64);
    profile.blueColorant.X = double_to_s15Fixed16Number(colorants.m[0][2] as f64);
    profile.blueColorant.Y = double_to_s15Fixed16Number(colorants.m[1][2] as f64);
    profile.blueColorant.Z = double_to_s15Fixed16Number(colorants.m[2][2] as f64);
    true
}
pub(crate) fn get_rgb_colorants(
    white_point: qcms_CIE_xyY,
    primaries: qcms_CIE_xyYTRIPLE,
) -> Matrix {
    let colorants = build_RGB_to_XYZ_transfer_matrix(white_point, primaries);
    let colorants = adapt_matrix_to_D50(colorants, white_point);
    colorants
}
/* Alpha is not corrected.
   A rationale for this is found in Alvy Ray's "Should Alpha Be Nonlinear If
   RGB Is?" Tech Memo 17 (December 14, 1998).
    See: ftp://ftp.alvyray.com/Acrobat/17_Nonln.pdf
*/
unsafe extern "C" fn qcms_transform_data_gray_template_lut<I: GrayFormat, F: Format>(
    transform: &qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    length: usize,
) {
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;
    let input_gamma_table_gray = transform.input_gamma_table_gray.as_ref().unwrap();

    let mut i: u32 = 0;
    while (i as usize) < length {
        let fresh0 = src;
        src = src.offset(1);
        let device: u8 = *fresh0;
        let mut alpha: u8 = 0xffu8;
        if I::has_alpha {
            let fresh1 = src;
            src = src.offset(1);
            alpha = *fresh1
        }
        let linear: f32 = input_gamma_table_gray[device as usize];

        let out_device_r: f32 = lut_interp_linear(
            linear as f64,
            &(*transform).output_gamma_lut_r.as_ref().unwrap(),
        );
        let out_device_g: f32 = lut_interp_linear(
            linear as f64,
            &(*transform).output_gamma_lut_g.as_ref().unwrap(),
        );
        let out_device_b: f32 = lut_interp_linear(
            linear as f64,
            &(*transform).output_gamma_lut_b.as_ref().unwrap(),
        );
        *dest.add(F::kRIndex) = clamp_u8(out_device_r * 255f32);
        *dest.add(F::kGIndex) = clamp_u8(out_device_g * 255f32);
        *dest.add(F::kBIndex) = clamp_u8(out_device_b * 255f32);
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = alpha
        }
        dest = dest.offset(components as isize);
        i += 1
    }
}
unsafe fn qcms_transform_data_gray_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_lut::<Gray, RGB>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_gray_rgba_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_lut::<Gray, RGBA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_gray_bgra_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_lut::<Gray, BGRA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_graya_rgba_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_lut::<GrayAlpha, RGBA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_graya_bgra_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_lut::<GrayAlpha, BGRA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_gray_template_precache<I: GrayFormat, F: Format>(
    transform: *const qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    length: usize,
) {
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;
    let output_table_r = ((*transform).output_table_r).as_deref().unwrap();
    let output_table_g = ((*transform).output_table_g).as_deref().unwrap();
    let output_table_b = ((*transform).output_table_b).as_deref().unwrap();

    let input_gamma_table_gray = (*transform)
        .input_gamma_table_gray
        .as_ref()
        .unwrap()
        .as_ptr();

    let mut i: u32 = 0;
    while (i as usize) < length {
        let fresh2 = src;
        src = src.offset(1);
        let device: u8 = *fresh2;
        let mut alpha: u8 = 0xffu8;
        if I::has_alpha {
            let fresh3 = src;
            src = src.offset(1);
            alpha = *fresh3
        }

        let linear: f32 = *input_gamma_table_gray.offset(device as isize);
        /* we could round here... */
        let gray: u16 = (linear * PRECACHE_OUTPUT_MAX as f32) as u16;
        *dest.add(F::kRIndex) = (output_table_r).data[gray as usize];
        *dest.add(F::kGIndex) = (output_table_g).data[gray as usize];
        *dest.add(F::kBIndex) = (output_table_b).data[gray as usize];
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = alpha
        }
        dest = dest.offset(components as isize);
        i += 1
    }
}
unsafe fn qcms_transform_data_gray_out_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_precache::<Gray, RGB>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_gray_rgba_out_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_precache::<Gray, RGBA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_gray_bgra_out_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_precache::<Gray, BGRA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_graya_rgba_out_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_precache::<GrayAlpha, RGBA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_graya_bgra_out_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_gray_template_precache::<GrayAlpha, BGRA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_template_lut_precache<F: Format>(
    transform: &qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    length: usize,
) {
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;
    let output_table_r = ((*transform).output_table_r).as_deref().unwrap();
    let output_table_g = ((*transform).output_table_g).as_deref().unwrap();
    let output_table_b = ((*transform).output_table_b).as_deref().unwrap();
    let input_gamma_table_r = (*transform).input_gamma_table_r.as_ref().unwrap().as_ptr();
    let input_gamma_table_g = (*transform).input_gamma_table_g.as_ref().unwrap().as_ptr();
    let input_gamma_table_b = (*transform).input_gamma_table_b.as_ref().unwrap().as_ptr();

    let mat = &transform.matrix;
    let mut i: u32 = 0;
    while (i as usize) < length {
        let device_r: u8 = *src.add(F::kRIndex);
        let device_g: u8 = *src.add(F::kGIndex);
        let device_b: u8 = *src.add(F::kBIndex);
        let mut alpha: u8 = 0;
        if F::kAIndex != 0xff {
            alpha = *src.add(F::kAIndex)
        }
        src = src.offset(components as isize);

        let linear_r: f32 = *input_gamma_table_r.offset(device_r as isize);
        let linear_g: f32 = *input_gamma_table_g.offset(device_g as isize);
        let linear_b: f32 = *input_gamma_table_b.offset(device_b as isize);
        let mut out_linear_r = mat[0][0] * linear_r + mat[1][0] * linear_g + mat[2][0] * linear_b;
        let mut out_linear_g = mat[0][1] * linear_r + mat[1][1] * linear_g + mat[2][1] * linear_b;
        let mut out_linear_b = mat[0][2] * linear_r + mat[1][2] * linear_g + mat[2][2] * linear_b;
        out_linear_r = clamp_float(out_linear_r);
        out_linear_g = clamp_float(out_linear_g);
        out_linear_b = clamp_float(out_linear_b);
        /* we could round here... */

        let r: u16 = (out_linear_r * PRECACHE_OUTPUT_MAX as f32) as u16;
        let g: u16 = (out_linear_g * PRECACHE_OUTPUT_MAX as f32) as u16;
        let b: u16 = (out_linear_b * PRECACHE_OUTPUT_MAX as f32) as u16;
        *dest.add(F::kRIndex) = (output_table_r).data[r as usize];
        *dest.add(F::kGIndex) = (output_table_g).data[g as usize];
        *dest.add(F::kBIndex) = (output_table_b).data[b as usize];
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = alpha
        }
        dest = dest.offset(components as isize);
        i += 1
    }
}
#[no_mangle]
pub unsafe fn qcms_transform_data_rgb_out_lut_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_precache::<RGB>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe fn qcms_transform_data_rgba_out_lut_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut_precache::<RGBA>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe fn qcms_transform_data_bgra_out_lut_precache(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
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
fn int_div_ceil(value: i32, div: i32) -> i32 {
    (value + div - 1) / div
}
// Using lcms' tetra interpolation algorithm.
unsafe extern "C" fn qcms_transform_data_tetra_clut_template<F: Format>(
    transform: *const qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    length: usize,
) {
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;

    let xy_len: i32 = 1;
    let x_len: i32 = (*transform).grid_size as i32;
    let len: i32 = x_len * x_len;
    let table = (*transform).clut.as_ref().unwrap().as_ptr();
    let r_table: *const f32 = table;
    let g_table: *const f32 = table.offset(1);
    let b_table: *const f32 = table.offset(2);

    let mut i: u32 = 0;
    while (i as usize) < length {
        let c0_r: f32;
        let c1_r: f32;
        let c2_r: f32;
        let c3_r: f32;
        let c0_g: f32;
        let c1_g: f32;
        let c2_g: f32;
        let c3_g: f32;
        let c0_b: f32;
        let c1_b: f32;
        let c2_b: f32;
        let c3_b: f32;
        let in_r: u8 = *src.add(F::kRIndex);
        let in_g: u8 = *src.add(F::kGIndex);
        let in_b: u8 = *src.add(F::kBIndex);
        let mut in_a: u8 = 0;
        if F::kAIndex != 0xff {
            in_a = *src.add(F::kAIndex)
        }
        src = src.offset(components as isize);
        let linear_r: f32 = in_r as i32 as f32 / 255.0;
        let linear_g: f32 = in_g as i32 as f32 / 255.0;
        let linear_b: f32 = in_b as i32 as f32 / 255.0;
        let x: i32 = in_r as i32 * ((*transform).grid_size as i32 - 1) / 255;
        let y: i32 = in_g as i32 * ((*transform).grid_size as i32 - 1) / 255;
        let z: i32 = in_b as i32 * ((*transform).grid_size as i32 - 1) / 255;
        let x_n: i32 = int_div_ceil(in_r as i32 * ((*transform).grid_size as i32 - 1), 255);
        let y_n: i32 = int_div_ceil(in_g as i32 * ((*transform).grid_size as i32 - 1), 255);
        let z_n: i32 = int_div_ceil(in_b as i32 * ((*transform).grid_size as i32 - 1), 255);
        let rx: f32 = linear_r * ((*transform).grid_size as i32 - 1) as f32 - x as f32;
        let ry: f32 = linear_g * ((*transform).grid_size as i32 - 1) as f32 - y as f32;
        let rz: f32 = linear_b * ((*transform).grid_size as i32 - 1) as f32 - z as f32;
        let CLU = |table: *const f32, x, y, z| {
            *table.offset(((x * len + y * x_len + z * xy_len) * 3) as isize)
        };

        c0_r = CLU(r_table, x, y, z);
        c0_g = CLU(g_table, x, y, z);
        c0_b = CLU(b_table, x, y, z);
        if rx >= ry {
            if ry >= rz {
                //rx >= ry && ry >= rz
                c1_r = CLU(r_table, x_n, y, z) - c0_r;
                c2_r = CLU(r_table, x_n, y_n, z) - CLU(r_table, x_n, y, z);
                c3_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y_n, z);
                c1_g = CLU(g_table, x_n, y, z) - c0_g;
                c2_g = CLU(g_table, x_n, y_n, z) - CLU(g_table, x_n, y, z);
                c3_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y_n, z);
                c1_b = CLU(b_table, x_n, y, z) - c0_b;
                c2_b = CLU(b_table, x_n, y_n, z) - CLU(b_table, x_n, y, z);
                c3_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y_n, z);
            } else if rx >= rz {
                //rx >= rz && rz >= ry
                c1_r = CLU(r_table, x_n, y, z) - c0_r;
                c2_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y, z_n);
                c3_r = CLU(r_table, x_n, y, z_n) - CLU(r_table, x_n, y, z);
                c1_g = CLU(g_table, x_n, y, z) - c0_g;
                c2_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y, z_n);
                c3_g = CLU(g_table, x_n, y, z_n) - CLU(g_table, x_n, y, z);
                c1_b = CLU(b_table, x_n, y, z) - c0_b;
                c2_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y, z_n);
                c3_b = CLU(b_table, x_n, y, z_n) - CLU(b_table, x_n, y, z);
            } else {
                //rz > rx && rx >= ry
                c1_r = CLU(r_table, x_n, y, z_n) - CLU(r_table, x, y, z_n);
                c2_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y, z_n);
                c3_r = CLU(r_table, x, y, z_n) - c0_r;
                c1_g = CLU(g_table, x_n, y, z_n) - CLU(g_table, x, y, z_n);
                c2_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y, z_n);
                c3_g = CLU(g_table, x, y, z_n) - c0_g;
                c1_b = CLU(b_table, x_n, y, z_n) - CLU(b_table, x, y, z_n);
                c2_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y, z_n);
                c3_b = CLU(b_table, x, y, z_n) - c0_b;
            }
        } else if rx >= rz {
            //ry > rx && rx >= rz
            c1_r = CLU(r_table, x_n, y_n, z) - CLU(r_table, x, y_n, z);
            c2_r = CLU(r_table, x, y_n, z) - c0_r;
            c3_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y_n, z);
            c1_g = CLU(g_table, x_n, y_n, z) - CLU(g_table, x, y_n, z);
            c2_g = CLU(g_table, x, y_n, z) - c0_g;
            c3_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y_n, z);
            c1_b = CLU(b_table, x_n, y_n, z) - CLU(b_table, x, y_n, z);
            c2_b = CLU(b_table, x, y_n, z) - c0_b;
            c3_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y_n, z);
        } else if ry >= rz {
            //ry >= rz && rz > rx
            c1_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x, y_n, z_n);
            c2_r = CLU(r_table, x, y_n, z) - c0_r;
            c3_r = CLU(r_table, x, y_n, z_n) - CLU(r_table, x, y_n, z);
            c1_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x, y_n, z_n);
            c2_g = CLU(g_table, x, y_n, z) - c0_g;
            c3_g = CLU(g_table, x, y_n, z_n) - CLU(g_table, x, y_n, z);
            c1_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x, y_n, z_n);
            c2_b = CLU(b_table, x, y_n, z) - c0_b;
            c3_b = CLU(b_table, x, y_n, z_n) - CLU(b_table, x, y_n, z);
        } else {
            //rz > ry && ry > rx
            c1_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x, y_n, z_n);
            c2_r = CLU(r_table, x, y_n, z_n) - CLU(r_table, x, y, z_n);
            c3_r = CLU(r_table, x, y, z_n) - c0_r;
            c1_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x, y_n, z_n);
            c2_g = CLU(g_table, x, y_n, z_n) - CLU(g_table, x, y, z_n);
            c3_g = CLU(g_table, x, y, z_n) - c0_g;
            c1_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x, y_n, z_n);
            c2_b = CLU(b_table, x, y_n, z_n) - CLU(b_table, x, y, z_n);
            c3_b = CLU(b_table, x, y, z_n) - c0_b;
        }
        let clut_r = c0_r + c1_r * rx + c2_r * ry + c3_r * rz;
        let clut_g = c0_g + c1_g * rx + c2_g * ry + c3_g * rz;
        let clut_b = c0_b + c1_b * rx + c2_b * ry + c3_b * rz;
        *dest.add(F::kRIndex) = clamp_u8(clut_r * 255.0);
        *dest.add(F::kGIndex) = clamp_u8(clut_g * 255.0);
        *dest.add(F::kBIndex) = clamp_u8(clut_b * 255.0);
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = in_a
        }
        dest = dest.offset(components as isize);
        i += 1
    }
}
unsafe fn qcms_transform_data_tetra_clut_rgb(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_tetra_clut_template::<RGB>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_tetra_clut_rgba(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_tetra_clut_template::<RGBA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_tetra_clut_bgra(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_tetra_clut_template::<BGRA>(transform, src, dest, length);
}
unsafe fn qcms_transform_data_template_lut<F: Format>(
    transform: &qcms_transform,
    mut src: *const u8,
    mut dest: *mut u8,
    length: usize,
) {
    let components: u32 = if F::kAIndex == 0xff { 3 } else { 4 } as u32;

    let mat = &transform.matrix;
    let mut i: u32 = 0;
    let input_gamma_table_r = (*transform).input_gamma_table_r.as_ref().unwrap().as_ptr();
    let input_gamma_table_g = (*transform).input_gamma_table_g.as_ref().unwrap().as_ptr();
    let input_gamma_table_b = (*transform).input_gamma_table_b.as_ref().unwrap().as_ptr();
    while (i as usize) < length {
        let device_r: u8 = *src.add(F::kRIndex);
        let device_g: u8 = *src.add(F::kGIndex);
        let device_b: u8 = *src.add(F::kBIndex);
        let mut alpha: u8 = 0;
        if F::kAIndex != 0xff {
            alpha = *src.add(F::kAIndex)
        }
        src = src.offset(components as isize);

        let linear_r: f32 = *input_gamma_table_r.offset(device_r as isize);
        let linear_g: f32 = *input_gamma_table_g.offset(device_g as isize);
        let linear_b: f32 = *input_gamma_table_b.offset(device_b as isize);
        let mut out_linear_r = mat[0][0] * linear_r + mat[1][0] * linear_g + mat[2][0] * linear_b;
        let mut out_linear_g = mat[0][1] * linear_r + mat[1][1] * linear_g + mat[2][1] * linear_b;
        let mut out_linear_b = mat[0][2] * linear_r + mat[1][2] * linear_g + mat[2][2] * linear_b;
        out_linear_r = clamp_float(out_linear_r);
        out_linear_g = clamp_float(out_linear_g);
        out_linear_b = clamp_float(out_linear_b);

        let out_device_r: f32 = lut_interp_linear(
            out_linear_r as f64,
            &(*transform).output_gamma_lut_r.as_ref().unwrap(),
        );
        let out_device_g: f32 = lut_interp_linear(
            out_linear_g as f64,
            (*transform).output_gamma_lut_g.as_ref().unwrap(),
        );
        let out_device_b: f32 = lut_interp_linear(
            out_linear_b as f64,
            (*transform).output_gamma_lut_b.as_ref().unwrap(),
        );
        *dest.add(F::kRIndex) = clamp_u8(out_device_r * 255f32);
        *dest.add(F::kGIndex) = clamp_u8(out_device_g * 255f32);
        *dest.add(F::kBIndex) = clamp_u8(out_device_b * 255f32);
        if F::kAIndex != 0xff {
            *dest.add(F::kAIndex) = alpha
        }
        dest = dest.offset(components as isize);
        i += 1
    }
}
#[no_mangle]
pub unsafe fn qcms_transform_data_rgb_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut::<RGB>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe fn qcms_transform_data_rgba_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut::<RGBA>(transform, src, dest, length);
}
#[no_mangle]
pub unsafe fn qcms_transform_data_bgra_out_lut(
    transform: &qcms_transform,
    src: *const u8,
    dest: *mut u8,
    length: usize,
) {
    qcms_transform_data_template_lut::<BGRA>(transform, src, dest, length);
}

fn precache_create() -> Arc<PrecacheOuput> {
    Arc::new(PrecacheOuput::default())
}

#[no_mangle]
pub unsafe extern "C" fn qcms_transform_release(t: *mut qcms_transform) {
    let t = Box::from_raw(t);
    drop(t)
}

const bradford_matrix: Matrix = Matrix {
    m: [
        [0.8951, 0.2664, -0.1614],
        [-0.7502, 1.7135, 0.0367],
        [0.0389, -0.0685, 1.0296],
    ],
    invalid: false,
};

const bradford_matrix_inv: Matrix = Matrix {
    m: [
        [0.9869929, -0.1470543, 0.1599627],
        [0.4323053, 0.5183603, 0.0492912],
        [-0.0085287, 0.0400428, 0.9684867],
    ],
    invalid: false,
};

// See ICCv4 E.3
fn compute_whitepoint_adaption(X: f32, Y: f32, Z: f32) -> Matrix {
    let p: f32 = (0.96422 * bradford_matrix.m[0][0]
        + 1.000 * bradford_matrix.m[1][0]
        + 0.82521 * bradford_matrix.m[2][0])
        / (X * bradford_matrix.m[0][0] + Y * bradford_matrix.m[1][0] + Z * bradford_matrix.m[2][0]);
    let y: f32 = (0.96422 * bradford_matrix.m[0][1]
        + 1.000 * bradford_matrix.m[1][1]
        + 0.82521 * bradford_matrix.m[2][1])
        / (X * bradford_matrix.m[0][1] + Y * bradford_matrix.m[1][1] + Z * bradford_matrix.m[2][1]);
    let b: f32 = (0.96422 * bradford_matrix.m[0][2]
        + 1.000 * bradford_matrix.m[1][2]
        + 0.82521 * bradford_matrix.m[2][2])
        / (X * bradford_matrix.m[0][2] + Y * bradford_matrix.m[1][2] + Z * bradford_matrix.m[2][2]);
    let white_adaption = Matrix {
        m: [[p, 0., 0.], [0., y, 0.], [0., 0., b]],
        invalid: false,
    };
    Matrix::multiply(
        bradford_matrix_inv,
        Matrix::multiply(white_adaption, bradford_matrix),
    )
}
#[no_mangle]
pub extern "C" fn qcms_profile_precache_output_transform(mut profile: &mut Profile) {
    /* we only support precaching on rgb profiles */
    if profile.color_space != RGB_SIGNATURE {
        return;
    }
    if SUPPORTS_ICCV4.load(Ordering::Relaxed) {
        /* don't precache since we will use the B2A LUT */
        if profile.B2A0.is_some() {
            return;
        }
        /* don't precache since we will use the mBA LUT */
        if profile.mBA.is_some() {
            return;
        }
    }
    /* don't precache if we do not have the TRC curves */
    if profile.redTRC.is_none() || profile.greenTRC.is_none() || profile.blueTRC.is_none() {
        return;
    }
    if profile.output_table_r.is_none() {
        let mut output_table_r = precache_create();
        if compute_precache(
            profile.redTRC.as_deref().unwrap(),
            &mut Arc::get_mut(&mut output_table_r).unwrap().data,
        ) {
            profile.output_table_r = Some(output_table_r);
        }
    }
    if profile.output_table_g.is_none() {
        let mut output_table_g = precache_create();
        if compute_precache(
            profile.greenTRC.as_deref().unwrap(),
            &mut Arc::get_mut(&mut output_table_g).unwrap().data,
        ) {
            profile.output_table_g = Some(output_table_g);
        }
    }
    if profile.output_table_b.is_none() {
        let mut output_table_b = precache_create();
        if compute_precache(
            profile.blueTRC.as_deref().unwrap(),
            &mut Arc::get_mut(&mut output_table_b).unwrap().data,
        ) {
            profile.output_table_b = Some(output_table_b);
        }
    };
}
/* Replace the current transformation with a LUT transformation using a given number of sample points */
fn transform_precacheLUT_float(
    mut transform: Box<qcms_transform>,
    input: &Profile,
    output: &Profile,
    samples: i32,
    in_type: DataType,
) -> Option<Box<qcms_transform>> {
    /* The range between which 2 consecutive sample points can be used to interpolate */
    let lutSize: u32 = (3 * samples * samples * samples) as u32;

    let mut src = Vec::with_capacity(lutSize as usize);
    let dest = vec![0.; lutSize as usize];
    /* Prepare a list of points we want to sample */
    for x in 0..samples {
        for y in 0..samples {
            for z in 0..samples {
                src.push(x as f32 / (samples - 1) as f32);
                src.push(y as f32 / (samples - 1) as f32);
                src.push(z as f32 / (samples - 1) as f32);
            }
        }
    }
    let lut = chain_transform(input, output, src, dest, lutSize as usize);
    if let Some(lut) = lut {
        (*transform).clut = Some(lut);
        (*transform).grid_size = samples as u16;
        if in_type == RGBA8 {
            (*transform).transform_fn = Some(qcms_transform_data_tetra_clut_rgba)
        } else if in_type == BGRA8 {
            (*transform).transform_fn = Some(qcms_transform_data_tetra_clut_bgra)
        } else if in_type == RGB8 {
            (*transform).transform_fn = Some(qcms_transform_data_tetra_clut_rgb)
        }
        debug_assert!((*transform).transform_fn.is_some());
    } else {
        return None;
    }

    Some(transform)
}

pub fn transform_create(
    input: &Profile,
    in_type: DataType,
    output: &Profile,
    out_type: DataType,
    _intent: Intent,
) -> Option<Box<qcms_transform>> {
    // Ensure the requested input and output types make sense.
    let matching_format = match (in_type, out_type) {
        (RGB8, RGB8) => true,
        (RGBA8, RGBA8) => true,
        (BGRA8, BGRA8) => true,
        (Gray8, out_type) => matches!(out_type, RGB8 | RGBA8 | BGRA8),
        (GrayA8, out_type) => matches!(out_type, RGBA8 | BGRA8),
        _ => false,
    };
    if !matching_format {
        debug_assert!(false, "input/output type");
        return None;
    }
    let mut transform: Box<qcms_transform> = Box::new(Default::default());
    let mut precache: bool = false;
    if output.output_table_r.is_some()
        && output.output_table_g.is_some()
        && output.output_table_b.is_some()
    {
        precache = true
    }
    // This precache assumes RGB_SIGNATURE (fails on GRAY_SIGNATURE, for instance)
    if SUPPORTS_ICCV4.load(Ordering::Relaxed)
        && (in_type == RGB8 || in_type == RGBA8 || in_type == BGRA8)
        && (input.A2B0.is_some()
            || output.B2A0.is_some()
            || input.mAB.is_some()
            || output.mAB.is_some())
    {
        // Precache the transformation to a CLUT 33x33x33 in size.
        // 33 is used by many profiles and works well in pratice.
        // This evenly divides 256 into blocks of 8x8x8.
        // TODO For transforming small data sets of about 200x200 or less
        // precaching should be avoided.
        let result = transform_precacheLUT_float(transform, input, output, 33, in_type);
        debug_assert!(result.is_some(), "precacheLUT failed");
        return result;
    }
    if precache {
        transform.output_table_r = Some(Arc::clone(output.output_table_r.as_ref().unwrap()));
        transform.output_table_g = Some(Arc::clone(output.output_table_g.as_ref().unwrap()));
        transform.output_table_b = Some(Arc::clone(output.output_table_b.as_ref().unwrap()));
    } else {
        if output.redTRC.is_none() || output.greenTRC.is_none() || output.blueTRC.is_none() {
            return None;
        }
        transform.output_gamma_lut_r = Some(build_output_lut(output.redTRC.as_deref().unwrap()));
        transform.output_gamma_lut_g = Some(build_output_lut(output.greenTRC.as_deref().unwrap()));
        transform.output_gamma_lut_b = Some(build_output_lut(output.blueTRC.as_deref().unwrap()));

        if transform.output_gamma_lut_r.is_none()
            || transform.output_gamma_lut_g.is_none()
            || transform.output_gamma_lut_b.is_none()
        {
            return None;
        }
    }
    if input.color_space == RGB_SIGNATURE {
        if precache {
            #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
            if is_x86_feature_detected!("avx") {
                if in_type == RGB8 {
                    transform.transform_fn = Some(qcms_transform_data_rgb_out_lut_avx)
                } else if in_type == RGBA8 {
                    transform.transform_fn = Some(qcms_transform_data_rgba_out_lut_avx)
                } else if in_type == BGRA8 {
                    transform.transform_fn = Some(qcms_transform_data_bgra_out_lut_avx)
                }
            } else if cfg!(not(miri)) && is_x86_feature_detected!("sse2") {
                if in_type == RGB8 {
                    transform.transform_fn = Some(qcms_transform_data_rgb_out_lut_sse2)
                } else if in_type == RGBA8 {
                    transform.transform_fn = Some(qcms_transform_data_rgba_out_lut_sse2)
                } else if in_type == BGRA8 {
                    transform.transform_fn = Some(qcms_transform_data_bgra_out_lut_sse2)
                }
            }

            #[cfg(all(target_arch = "arm", feature = "neon"))]
            let neon_supported = is_arm_feature_detected!("neon");
            #[cfg(all(target_arch = "aarch64", feature = "neon"))]
            let neon_supported = is_aarch64_feature_detected!("neon");

            #[cfg(all(any(target_arch = "arm", target_arch = "aarch64"), feature = "neon"))]
            if neon_supported {
                if in_type == RGB8 {
                    transform.transform_fn = Some(qcms_transform_data_rgb_out_lut_neon)
                } else if in_type == RGBA8 {
                    transform.transform_fn = Some(qcms_transform_data_rgba_out_lut_neon)
                } else if in_type == BGRA8 {
                    transform.transform_fn = Some(qcms_transform_data_bgra_out_lut_neon)
                }
            }

            if transform.transform_fn.is_none() {
                if in_type == RGB8 {
                    transform.transform_fn = Some(qcms_transform_data_rgb_out_lut_precache)
                } else if in_type == RGBA8 {
                    transform.transform_fn = Some(qcms_transform_data_rgba_out_lut_precache)
                } else if in_type == BGRA8 {
                    transform.transform_fn = Some(qcms_transform_data_bgra_out_lut_precache)
                }
            }
        } else if in_type == RGB8 {
            transform.transform_fn = Some(qcms_transform_data_rgb_out_lut)
        } else if in_type == RGBA8 {
            transform.transform_fn = Some(qcms_transform_data_rgba_out_lut)
        } else if in_type == BGRA8 {
            transform.transform_fn = Some(qcms_transform_data_bgra_out_lut)
        }
        //XXX: avoid duplicating tables if we can
        transform.input_gamma_table_r = build_input_gamma_table(input.redTRC.as_deref());
        transform.input_gamma_table_g = build_input_gamma_table(input.greenTRC.as_deref());
        transform.input_gamma_table_b = build_input_gamma_table(input.blueTRC.as_deref());
        if transform.input_gamma_table_r.is_none()
            || transform.input_gamma_table_g.is_none()
            || transform.input_gamma_table_b.is_none()
        {
            return None;
        }
        /* build combined colorant matrix */

        let in_matrix: Matrix = build_colorant_matrix(input);
        let mut out_matrix: Matrix = build_colorant_matrix(output);
        out_matrix = out_matrix.invert();
        if out_matrix.invalid {
            return None;
        }
        let result_0: Matrix = Matrix::multiply(out_matrix, in_matrix);
        /* check for NaN values in the matrix and bail if we find any */
        let mut i: u32 = 0;
        while i < 3 {
            let mut j: u32 = 0;
            while j < 3 {
                if result_0.m[i as usize][j as usize] != result_0.m[i as usize][j as usize] {
                    return None;
                }
                j += 1
            }
            i += 1
        }
        /* store the results in column major mode
         * this makes doing the multiplication with sse easier */
        transform.matrix[0][0] = result_0.m[0][0];
        transform.matrix[1][0] = result_0.m[0][1];
        transform.matrix[2][0] = result_0.m[0][2];
        transform.matrix[0][1] = result_0.m[1][0];
        transform.matrix[1][1] = result_0.m[1][1];
        transform.matrix[2][1] = result_0.m[1][2];
        transform.matrix[0][2] = result_0.m[2][0];
        transform.matrix[1][2] = result_0.m[2][1];
        transform.matrix[2][2] = result_0.m[2][2]
    } else if input.color_space == GRAY_SIGNATURE {
        transform.input_gamma_table_gray = build_input_gamma_table(input.grayTRC.as_deref());
        transform.input_gamma_table_gray.as_ref()?;
        if precache {
            if out_type == RGB8 {
                transform.transform_fn = Some(qcms_transform_data_gray_out_precache)
            } else if out_type == RGBA8 {
                if in_type == Gray8 {
                    transform.transform_fn = Some(qcms_transform_data_gray_rgba_out_precache)
                } else {
                    transform.transform_fn = Some(qcms_transform_data_graya_rgba_out_precache)
                }
            } else if out_type == BGRA8 {
                if in_type == Gray8 {
                    transform.transform_fn = Some(qcms_transform_data_gray_bgra_out_precache)
                } else {
                    transform.transform_fn = Some(qcms_transform_data_graya_bgra_out_precache)
                }
            }
        } else if out_type == RGB8 {
            transform.transform_fn = Some(qcms_transform_data_gray_out_lut)
        } else if out_type == RGBA8 {
            if in_type == Gray8 {
                transform.transform_fn = Some(qcms_transform_data_gray_rgba_out_lut)
            } else {
                transform.transform_fn = Some(qcms_transform_data_graya_rgba_out_lut)
            }
        } else if out_type == BGRA8 {
            if in_type == Gray8 {
                transform.transform_fn = Some(qcms_transform_data_gray_bgra_out_lut)
            } else {
                transform.transform_fn = Some(qcms_transform_data_graya_bgra_out_lut)
            }
        }
    } else {
        debug_assert!(false, "unexpected colorspace");
        return None;
    }
    debug_assert!(transform.transform_fn.is_some());
    Some(transform)
}
/// A transform from an input profile to an output one.
pub struct Transform {
    src_ty: DataType,
    dst_ty: DataType,
    xfm: Box<qcms_transform>,
}

impl Transform {
    /// Create a new transform from `input` to `output` for pixels of `DataType` `ty` with `intent`
    pub fn new(input: &Profile, output: &Profile, ty: DataType, intent: Intent) -> Option<Self> {
        transform_create(input, ty, output, ty, intent).map(|xfm| Transform {
            src_ty: ty,
            dst_ty: ty,
            xfm,
        })
    }

    /// Create a new transform from `input` to `output` for pixels of `DataType` `ty` with `intent`
    pub fn new_to(
        input: &Profile,
        output: &Profile,
        src_ty: DataType,
        dst_ty: DataType,
        intent: Intent,
    ) -> Option<Self> {
        transform_create(input, src_ty, output, dst_ty, intent).map(|xfm| Transform {
            src_ty,
            dst_ty,
            xfm,
        })
    }

    /// Apply the color space transform to `data`
    pub fn apply(&self, data: &mut [u8]) {
        if data.len() % self.src_ty.bytes_per_pixel() != 0 {
            panic!(
                "incomplete pixels: should be a multiple of {} got {}",
                self.src_ty.bytes_per_pixel(),
                data.len()
            )
        }
        unsafe {
            self.xfm.transform_fn.expect("non-null function pointer")(
                &*self.xfm,
                data.as_ptr(),
                data.as_mut_ptr(),
                data.len() / self.src_ty.bytes_per_pixel(),
            );
        }
    }

    /// Apply the color space transform to `data`
    pub fn convert(&self, src: &[u8], dst: &mut [u8]) {
        if src.len() % self.src_ty.bytes_per_pixel() != 0 {
            panic!(
                "incomplete pixels: should be a multiple of {} got {}",
                self.src_ty.bytes_per_pixel(),
                src.len()
            )
        }
        if dst.len() % self.dst_ty.bytes_per_pixel() != 0 {
            panic!(
                "incomplete pixels: should be a multiple of {} got {}",
                self.dst_ty.bytes_per_pixel(),
                dst.len()
            )
        }
        assert_eq!(
            src.len() / self.src_ty.bytes_per_pixel(),
            dst.len() / self.dst_ty.bytes_per_pixel()
        );
        unsafe {
            self.xfm.transform_fn.expect("non-null function pointer")(
                &*self.xfm,
                src.as_ptr(),
                dst.as_mut_ptr(),
                src.len() / self.src_ty.bytes_per_pixel(),
            );
        }
    }
}

#[no_mangle]
pub extern "C" fn qcms_enable_iccv4() {
    SUPPORTS_ICCV4.store(true, Ordering::Relaxed);
}

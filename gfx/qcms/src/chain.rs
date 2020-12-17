/* vim: set ts=8 sw=8 noexpandtab: */
//  qcms
//  Copyright (C) 2009 Mozilla Corporation
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
use crate::{
    iccread::LAB_SIGNATURE,
    iccread::RGB_SIGNATURE,
    iccread::XYZ_SIGNATURE,
    iccread::{lutType, lutmABType, qcms_profile},
    matrix::{matrix, matrix_invert},
    s15Fixed16Number_to_float,
    transform_util::clamp_float,
    transform_util::{
        build_colorant_matrix, build_input_gamma_table, build_output_lut, lut_interp_linear,
        lut_interp_linear_float,
    },
};

#[derive(Clone, Default)]
pub struct qcms_modular_transform {
    pub matrix: matrix,
    pub tx: f32,
    pub ty: f32,
    pub tz: f32,
    pub input_clut_table_r: Option<Vec<f32>>,
    pub input_clut_table_g: Option<Vec<f32>>,
    pub input_clut_table_b: Option<Vec<f32>>,
    pub input_clut_table_length: u16,
    pub clut: Option<Vec<f32>>,
    pub grid_size: u16,
    pub output_clut_table_r: Option<Vec<f32>>,
    pub output_clut_table_g: Option<Vec<f32>>,
    pub output_clut_table_b: Option<Vec<f32>>,
    pub output_clut_table_length: u16,
    pub output_gamma_lut_r: Option<Vec<u16>>,
    pub output_gamma_lut_g: Option<Vec<u16>>,
    pub output_gamma_lut_b: Option<Vec<u16>>,
    pub output_gamma_lut_r_length: usize,
    pub output_gamma_lut_g_length: usize,
    pub output_gamma_lut_b_length: usize,
    pub transform_module_fn: transform_module_fn_t,
    pub next_transform: Option<Box<qcms_modular_transform>>,
}
pub type transform_module_fn_t =
    Option<fn(_: &qcms_modular_transform, _: &[f32], _: &mut [f32]) -> ()>;

#[inline]
fn lerp(mut a: f32, mut b: f32, mut t: f32) -> f32 {
    return a * (1.0 - t) + b * t;
}

fn build_lut_matrix(mut lut: Option<&lutType>) -> matrix {
    let mut result: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    if let Some(lut) = lut {
        result.m[0][0] = s15Fixed16Number_to_float((*lut).e00);
        result.m[0][1] = s15Fixed16Number_to_float((*lut).e01);
        result.m[0][2] = s15Fixed16Number_to_float((*lut).e02);
        result.m[1][0] = s15Fixed16Number_to_float((*lut).e10);
        result.m[1][1] = s15Fixed16Number_to_float((*lut).e11);
        result.m[1][2] = s15Fixed16Number_to_float((*lut).e12);
        result.m[2][0] = s15Fixed16Number_to_float((*lut).e20);
        result.m[2][1] = s15Fixed16Number_to_float((*lut).e21);
        result.m[2][2] = s15Fixed16Number_to_float((*lut).e22);
        result.invalid = false
    } else {
        result.m = Default::default();
        result.invalid = true
    }
    return result;
}
fn build_mAB_matrix(lut: &lutmABType) -> matrix {
    let mut result: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };

    result.m[0][0] = s15Fixed16Number_to_float((*lut).e00);
    result.m[0][1] = s15Fixed16Number_to_float((*lut).e01);
    result.m[0][2] = s15Fixed16Number_to_float((*lut).e02);
    result.m[1][0] = s15Fixed16Number_to_float((*lut).e10);
    result.m[1][1] = s15Fixed16Number_to_float((*lut).e11);
    result.m[1][2] = s15Fixed16Number_to_float((*lut).e12);
    result.m[2][0] = s15Fixed16Number_to_float((*lut).e20);
    result.m[2][1] = s15Fixed16Number_to_float((*lut).e21);
    result.m[2][2] = s15Fixed16Number_to_float((*lut).e22);
    result.invalid = false;

    return result;
}
//Based on lcms cmsLab2XYZ
fn f(t: f32) -> f32 {
    if t <= 24. / 116. * (24. / 116.) * (24. / 116.) {
        (841. / 108. * t) + 16. / 116.
    } else {
        t.powf(1. / 3.)
    }
}
fn f_1(t: f32) -> f32 {
    if t <= 24.0 / 116.0 {
        (108.0 / 841.0) * (t - 16.0 / 116.0)
    } else {
        t * t * t
    }
}

fn transform_module_LAB_to_XYZ(
    transform: &qcms_modular_transform,
    src: &[f32],
    mut dest: &mut [f32],
) {
    // lcms: D50 XYZ values
    let mut WhitePointX: f32 = 0.9642;
    let mut WhitePointY: f32 = 1.0;
    let mut WhitePointZ: f32 = 0.8249;
    let mut i: usize = 0;

    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let mut device_L: f32 = src[0] * 100.0;
        let mut device_a: f32 = src[1] * 255.0 - 128.0;
        let mut device_b: f32 = src[2] * 255.0 - 128.0;

        let mut y: f32 = (device_L + 16.0) / 116.0;

        let mut X = f_1(y + 0.002 * device_a) * WhitePointX;
        let mut Y = f_1(y) * WhitePointY;
        let mut Z = f_1(y - 0.005 * device_b) * WhitePointZ;

        dest[0] = (X as f64 / (1.0f64 + 32767.0f64 / 32768.0f64)) as f32;
        dest[1] = (Y as f64 / (1.0f64 + 32767.0f64 / 32768.0f64)) as f32;
        dest[2] = (Z as f64 / (1.0f64 + 32767.0f64 / 32768.0f64)) as f32;
    }
}
//Based on lcms cmsXYZ2Lab
fn transform_module_XYZ_to_LAB(
    mut transform: &qcms_modular_transform,
    mut src: &[f32],
    mut dest: &mut [f32],
) {
    // lcms: D50 XYZ values
    let mut WhitePointX: f32 = 0.9642;
    let mut WhitePointY: f32 = 1.0;
    let mut WhitePointZ: f32 = 0.8249;
    let mut i: usize = 0;
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let mut device_x: f32 =
            (src[0] as f64 * (1.0f64 + 32767.0f64 / 32768.0f64) / WhitePointX as f64) as f32;
        let mut device_y: f32 =
            (src[1] as f64 * (1.0f64 + 32767.0f64 / 32768.0f64) / WhitePointY as f64) as f32;
        let mut device_z: f32 =
            (src[2] as f64 * (1.0f64 + 32767.0f64 / 32768.0f64) / WhitePointZ as f64) as f32;

        let mut fx = f(device_x);
        let mut fy = f(device_y);
        let mut fz = f(device_z);

        let mut L: f32 = 116.0 * fy - 16.0;
        let mut a: f32 = 500.0 * (fx - fy);
        let mut b: f32 = 200.0 * (fy - fz);

        dest[0] = L / 100.0;
        dest[1] = (a + 128.0) / 255.0;
        dest[2] = (b + 128.0) / 255.0;

    }
}
fn transform_module_clut_only(
    mut transform: &qcms_modular_transform,
    mut src: &[f32],
    mut dest: &mut [f32],
) {
    let mut xy_len: i32 = 1;
    let mut x_len: i32 = (*transform).grid_size as i32;
    let mut len: i32 = x_len * x_len;
    unsafe {

    let mut r_table: *const f32 = (*transform).clut.as_ref().unwrap().as_ptr().offset(0isize);
    let mut g_table: *const f32 = (*transform).clut.as_ref().unwrap().as_ptr().offset(1isize);
    let mut b_table: *const f32 = (*transform).clut.as_ref().unwrap().as_ptr().offset(2isize);

    let mut i: usize = 0;
    let CLU = |table: *const f32, x, y, z| {
        *table.offset(((x * len + y * x_len + z * xy_len) * 3) as isize)
    };

    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        debug_assert!((*transform).grid_size as i32 >= 1);
        let mut linear_r: f32 = src[0];
        let mut linear_g: f32 = src[1];
        let mut linear_b: f32 = src[2];
        let mut x: i32 = (linear_r * ((*transform).grid_size as i32 - 1) as f32).floor() as i32;
        let mut y: i32 = (linear_g * ((*transform).grid_size as i32 - 1) as f32).floor() as i32;
        let mut z: i32 = (linear_b * ((*transform).grid_size as i32 - 1) as f32).floor() as i32;
        let mut x_n: i32 = (linear_r * ((*transform).grid_size as i32 - 1) as f32).ceil() as i32;
        let mut y_n: i32 = (linear_g * ((*transform).grid_size as i32 - 1) as f32).ceil() as i32;
        let mut z_n: i32 = (linear_b * ((*transform).grid_size as i32 - 1) as f32).ceil() as i32;
        let mut x_d: f32 = linear_r * ((*transform).grid_size as i32 - 1) as f32 - x as f32;
        let mut y_d: f32 = linear_g * ((*transform).grid_size as i32 - 1) as f32 - y as f32;
        let mut z_d: f32 = linear_b * ((*transform).grid_size as i32 - 1) as f32 - z as f32;

        let mut r_x1: f32 = lerp(CLU(r_table, x, y, z), CLU(r_table, x_n, y, z), x_d);
        let mut r_x2: f32 = lerp(CLU(r_table, x, y_n, z), CLU(r_table, x_n, y_n, z), x_d);
        let mut r_y1: f32 = lerp(r_x1, r_x2, y_d);
        let mut r_x3: f32 = lerp(CLU(r_table, x, y, z_n), CLU(r_table, x_n, y, z_n), x_d);
        let mut r_x4: f32 = lerp(CLU(r_table, x, y_n, z_n), CLU(r_table, x_n, y_n, z_n), x_d);
        let mut r_y2: f32 = lerp(r_x3, r_x4, y_d);
        let mut clut_r: f32 = lerp(r_y1, r_y2, z_d);

        let mut g_x1: f32 = lerp(CLU(g_table, x, y, z), CLU(g_table, x_n, y, z), x_d);
        let mut g_x2: f32 = lerp(CLU(g_table, x, y_n, z), CLU(g_table, x_n, y_n, z), x_d);
        let mut g_y1: f32 = lerp(g_x1, g_x2, y_d);
        let mut g_x3: f32 = lerp(CLU(g_table, x, y, z_n), CLU(g_table, x_n, y, z_n), x_d);
        let mut g_x4: f32 = lerp(CLU(g_table, x, y_n, z_n), CLU(g_table, x_n, y_n, z_n), x_d);
        let mut g_y2: f32 = lerp(g_x3, g_x4, y_d);
        let mut clut_g: f32 = lerp(g_y1, g_y2, z_d);

        let mut b_x1: f32 = lerp(CLU(b_table, x, y, z), CLU(b_table, x_n, y, z), x_d);
        let mut b_x2: f32 = lerp(CLU(b_table, x, y_n, z), CLU(b_table, x_n, y_n, z), x_d);
        let mut b_y1: f32 = lerp(b_x1, b_x2, y_d);
        let mut b_x3: f32 = lerp(CLU(b_table, x, y, z_n), CLU(b_table, x_n, y, z_n), x_d);
        let mut b_x4: f32 = lerp(CLU(b_table, x, y_n, z_n), CLU(b_table, x_n, y_n, z_n), x_d);
        let mut b_y2: f32 = lerp(b_x3, b_x4, y_d);
        let mut clut_b: f32 = lerp(b_y1, b_y2, z_d);

        dest[0] = clamp_float(clut_r);
        dest[1] = clamp_float(clut_g);
        dest[2] = clamp_float(clut_b);
    }
    }
}
fn transform_module_clut(
    mut transform: &qcms_modular_transform,
    mut src: &[f32],
    mut dest: &mut [f32],
) {
    let mut xy_len: i32 = 1;
    let mut x_len: i32 = (*transform).grid_size as i32;
    let mut len: i32 = x_len * x_len;
    unsafe {
    let mut r_table: *const f32 = (*transform).clut.as_ref().unwrap().as_ptr().offset(0isize);
    let mut g_table: *const f32 = (*transform).clut.as_ref().unwrap().as_ptr().offset(1isize);
    let mut b_table: *const f32 = (*transform).clut.as_ref().unwrap().as_ptr().offset(2isize);
    let CLU = |table: *const f32, x, y, z| {
        *table.offset(((x * len + y * x_len + z * xy_len) * 3) as isize)
    };

    let mut i: usize = 0;
    let input_clut_table_r = (*transform).input_clut_table_r.as_ref().unwrap();
    let input_clut_table_g = (*transform).input_clut_table_g.as_ref().unwrap();
    let input_clut_table_b = (*transform).input_clut_table_b.as_ref().unwrap();
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        debug_assert!((*transform).grid_size as i32 >= 1);
        let mut device_r: f32 = src[0];
        let mut device_g: f32 = src[1];
        let mut device_b: f32 = src[2];
        let mut linear_r: f32 = lut_interp_linear_float(device_r, &input_clut_table_r);
        let mut linear_g: f32 = lut_interp_linear_float(device_g, &input_clut_table_g);
        let mut linear_b: f32 = lut_interp_linear_float(device_b, &input_clut_table_b);
        let mut x: i32 = (linear_r * ((*transform).grid_size as i32 - 1) as f32).floor() as i32;
        let mut y: i32 = (linear_g * ((*transform).grid_size as i32 - 1) as f32).floor() as i32;
        let mut z: i32 = (linear_b * ((*transform).grid_size as i32 - 1) as f32).floor() as i32;
        let mut x_n: i32 = (linear_r * ((*transform).grid_size as i32 - 1) as f32).ceil() as i32;
        let mut y_n: i32 = (linear_g * ((*transform).grid_size as i32 - 1) as f32).ceil() as i32;
        let mut z_n: i32 = (linear_b * ((*transform).grid_size as i32 - 1) as f32).ceil() as i32;
        let mut x_d: f32 = linear_r * ((*transform).grid_size as i32 - 1) as f32 - x as f32;
        let mut y_d: f32 = linear_g * ((*transform).grid_size as i32 - 1) as f32 - y as f32;
        let mut z_d: f32 = linear_b * ((*transform).grid_size as i32 - 1) as f32 - z as f32;

        let mut r_x1: f32 = lerp(CLU(r_table, x, y, z), CLU(r_table, x_n, y, z), x_d);
        let mut r_x2: f32 = lerp(CLU(r_table, x, y_n, z), CLU(r_table, x_n, y_n, z), x_d);
        let mut r_y1: f32 = lerp(r_x1, r_x2, y_d);
        let mut r_x3: f32 = lerp(CLU(r_table, x, y, z_n), CLU(r_table, x_n, y, z_n), x_d);
        let mut r_x4: f32 = lerp(CLU(r_table, x, y_n, z_n), CLU(r_table, x_n, y_n, z_n), x_d);
        let mut r_y2: f32 = lerp(r_x3, r_x4, y_d);
        let mut clut_r: f32 = lerp(r_y1, r_y2, z_d);

        let mut g_x1: f32 = lerp(CLU(g_table, x, y, z), CLU(g_table, x_n, y, z), x_d);
        let mut g_x2: f32 = lerp(CLU(g_table, x, y_n, z), CLU(g_table, x_n, y_n, z), x_d);
        let mut g_y1: f32 = lerp(g_x1, g_x2, y_d);
        let mut g_x3: f32 = lerp(CLU(g_table, x, y, z_n), CLU(g_table, x_n, y, z_n), x_d);
        let mut g_x4: f32 = lerp(CLU(g_table, x, y_n, z_n), CLU(g_table, x_n, y_n, z_n), x_d);
        let mut g_y2: f32 = lerp(g_x3, g_x4, y_d);
        let mut clut_g: f32 = lerp(g_y1, g_y2, z_d);

        let mut b_x1: f32 = lerp(CLU(b_table, x, y, z), CLU(b_table, x_n, y, z), x_d);
        let mut b_x2: f32 = lerp(CLU(b_table, x, y_n, z), CLU(b_table, x_n, y_n, z), x_d);
        let mut b_y1: f32 = lerp(b_x1, b_x2, y_d);
        let mut b_x3: f32 = lerp(CLU(b_table, x, y, z_n), CLU(b_table, x_n, y, z_n), x_d);
        let mut b_x4: f32 = lerp(CLU(b_table, x, y_n, z_n), CLU(b_table, x_n, y_n, z_n), x_d);
        let mut b_y2: f32 = lerp(b_x3, b_x4, y_d);
        let mut clut_b: f32 = lerp(b_y1, b_y2, z_d);
        let mut pcs_r: f32 =
            lut_interp_linear_float(clut_r, &(*transform).output_clut_table_r.as_ref().unwrap());
        let mut pcs_g: f32 =
            lut_interp_linear_float(clut_g, &(*transform).output_clut_table_g.as_ref().unwrap());
        let mut pcs_b: f32 =
            lut_interp_linear_float(clut_b, &(*transform).output_clut_table_b.as_ref().unwrap());
        dest[0] = clamp_float(pcs_r);
        dest[1] = clamp_float(pcs_g);
        dest[2] = clamp_float(pcs_b);
    }
    }
}
/* NOT USED
static void qcms_transform_module_tetra_clut(struct qcms_modular_transform *transform, float *src, float *dest, size_t length)
{
    size_t i;
    int xy_len = 1;
    int x_len = transform->grid_size;
    int len = x_len * x_len;
    float* r_table = transform->r_clut;
    float* g_table = transform->g_clut;
    float* b_table = transform->b_clut;
    float c0_r, c1_r, c2_r, c3_r;
    float c0_g, c1_g, c2_g, c3_g;
    float c0_b, c1_b, c2_b, c3_b;
    float clut_r, clut_g, clut_b;
    float pcs_r, pcs_g, pcs_b;
    for (i = 0; i < length; i++) {
        float device_r = *src++;
        float device_g = *src++;
        float device_b = *src++;
        float linear_r = lut_interp_linear_float(device_r,
                transform->input_clut_table_r, transform->input_clut_table_length);
        float linear_g = lut_interp_linear_float(device_g,
                transform->input_clut_table_g, transform->input_clut_table_length);
        float linear_b = lut_interp_linear_float(device_b,
                transform->input_clut_table_b, transform->input_clut_table_length);

        int x = floorf(linear_r * (transform->grid_size-1));
        int y = floorf(linear_g * (transform->grid_size-1));
        int z = floorf(linear_b * (transform->grid_size-1));
        int x_n = ceilf(linear_r * (transform->grid_size-1));
        int y_n = ceilf(linear_g * (transform->grid_size-1));
        int z_n = ceilf(linear_b * (transform->grid_size-1));
        float rx = linear_r * (transform->grid_size-1) - x;
        float ry = linear_g * (transform->grid_size-1) - y;
        float rz = linear_b * (transform->grid_size-1) - z;

        c0_r = CLU(r_table, x, y, z);
        c0_g = CLU(g_table, x, y, z);
        c0_b = CLU(b_table, x, y, z);
        if( rx >= ry ) {
            if (ry >= rz) { //rx >= ry && ry >= rz
                c1_r = CLU(r_table, x_n, y, z) - c0_r;
                c2_r = CLU(r_table, x_n, y_n, z) - CLU(r_table, x_n, y, z);
                c3_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y_n, z);
                c1_g = CLU(g_table, x_n, y, z) - c0_g;
                c2_g = CLU(g_table, x_n, y_n, z) - CLU(g_table, x_n, y, z);
                c3_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y_n, z);
                c1_b = CLU(b_table, x_n, y, z) - c0_b;
                c2_b = CLU(b_table, x_n, y_n, z) - CLU(b_table, x_n, y, z);
                c3_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y_n, z);
            } else {
                if (rx >= rz) { //rx >= rz && rz >= ry
                    c1_r = CLU(r_table, x_n, y, z) - c0_r;
                    c2_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y, z_n);
                    c3_r = CLU(r_table, x_n, y, z_n) - CLU(r_table, x_n, y, z);
                    c1_g = CLU(g_table, x_n, y, z) - c0_g;
                    c2_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y, z_n);
                    c3_g = CLU(g_table, x_n, y, z_n) - CLU(g_table, x_n, y, z);
                    c1_b = CLU(b_table, x_n, y, z) - c0_b;
                    c2_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y, z_n);
                    c3_b = CLU(b_table, x_n, y, z_n) - CLU(b_table, x_n, y, z);
                } else { //rz > rx && rx >= ry
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
            }
        } else {
            if (rx >= rz) { //ry > rx && rx >= rz
                c1_r = CLU(r_table, x_n, y_n, z) - CLU(r_table, x, y_n, z);
                c2_r = CLU(r_table, x_n, y_n, z) - c0_r;
                c3_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y_n, z);
                c1_g = CLU(g_table, x_n, y_n, z) - CLU(g_table, x, y_n, z);
                c2_g = CLU(g_table, x_n, y_n, z) - c0_g;
                c3_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y_n, z);
                c1_b = CLU(b_table, x_n, y_n, z) - CLU(b_table, x, y_n, z);
                c2_b = CLU(b_table, x_n, y_n, z) - c0_b;
                c3_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y_n, z);
            } else {
                if (ry >= rz) { //ry >= rz && rz > rx
                    c1_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x, y_n, z_n);
                    c2_r = CLU(r_table, x, y_n, z) - c0_r;
                    c3_r = CLU(r_table, x, y_n, z_n) - CLU(r_table, x, y_n, z);
                    c1_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x, y_n, z_n);
                    c2_g = CLU(g_table, x, y_n, z) - c0_g;
                    c3_g = CLU(g_table, x, y_n, z_n) - CLU(g_table, x, y_n, z);
                    c1_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x, y_n, z_n);
                    c2_b = CLU(b_table, x, y_n, z) - c0_b;
                    c3_b = CLU(b_table, x, y_n, z_n) - CLU(b_table, x, y_n, z);
                } else { //rz > ry && ry > rx
                    c1_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x, y_n, z_n);
                    c2_r = CLU(r_table, x, y_n, z) - c0_r;
                    c3_r = CLU(r_table, x_n, y_n, z_n) - CLU(r_table, x_n, y_n, z);
                    c1_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x, y_n, z_n);
                    c2_g = CLU(g_table, x, y_n, z) - c0_g;
                    c3_g = CLU(g_table, x_n, y_n, z_n) - CLU(g_table, x_n, y_n, z);
                    c1_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x, y_n, z_n);
                    c2_b = CLU(b_table, x, y_n, z) - c0_b;
                    c3_b = CLU(b_table, x_n, y_n, z_n) - CLU(b_table, x_n, y_n, z);
                }
            }
        }

        clut_r = c0_r + c1_r*rx + c2_r*ry + c3_r*rz;
        clut_g = c0_g + c1_g*rx + c2_g*ry + c3_g*rz;
        clut_b = c0_b + c1_b*rx + c2_b*ry + c3_b*rz;

        pcs_r = lut_interp_linear_float(clut_r,
                transform->output_clut_table_r, transform->output_clut_table_length);
        pcs_g = lut_interp_linear_float(clut_g,
                transform->output_clut_table_g, transform->output_clut_table_length);
        pcs_b = lut_interp_linear_float(clut_b,
                transform->output_clut_table_b, transform->output_clut_table_length);
        *dest++ = clamp_float(pcs_r);
        *dest++ = clamp_float(pcs_g);
        *dest++ = clamp_float(pcs_b);
    }
}
*/
fn transform_module_gamma_table(
    mut transform: &qcms_modular_transform,
    mut src: &[f32],
    mut dest: &mut [f32],
) {
    let mut out_r: f32;
    let mut out_g: f32;
    let mut out_b: f32;
    let mut i: usize = 0;
    let input_clut_table_r = (*transform).input_clut_table_r.as_ref().unwrap();
    let input_clut_table_g = (*transform).input_clut_table_g.as_ref().unwrap();
    let input_clut_table_b = (*transform).input_clut_table_b.as_ref().unwrap();

    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let mut in_r: f32 = src[0];
        let mut in_g: f32 = src[1];
        let mut in_b: f32 = src[2];
        out_r = lut_interp_linear_float(in_r, input_clut_table_r);
        out_g = lut_interp_linear_float(in_g, input_clut_table_g);
        out_b = lut_interp_linear_float(in_b, input_clut_table_b);

        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}
fn transform_module_gamma_lut(
    mut transform: &qcms_modular_transform,
    mut src: &[f32],
    mut dest: &mut [f32],
) {
    let mut out_r: f32;
    let mut out_g: f32;
    let mut out_b: f32;
    let mut i: usize = 0;
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let mut in_r: f32 = src[0];
        let mut in_g: f32 = src[1];
        let mut in_b: f32 = src[2];
        out_r = lut_interp_linear(
            in_r as f64,
            &(*transform).output_gamma_lut_r.as_ref().unwrap(),
        );
        out_g = lut_interp_linear(
            in_g as f64,
            &(*transform).output_gamma_lut_g.as_ref().unwrap(),
        );
        out_b = lut_interp_linear(
            in_b as f64,
            &(*transform).output_gamma_lut_b.as_ref().unwrap(),
        );
        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}
fn transform_module_matrix_translate(
    mut transform: &qcms_modular_transform,
    mut src: &[f32],
    mut dest: &mut [f32],
) {
    let mut mat: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    /* store the results in column major mode
     * this makes doing the multiplication with sse easier */
    mat.m[0][0] = (*transform).matrix.m[0][0];
    mat.m[1][0] = (*transform).matrix.m[0][1];
    mat.m[2][0] = (*transform).matrix.m[0][2];
    mat.m[0][1] = (*transform).matrix.m[1][0];
    mat.m[1][1] = (*transform).matrix.m[1][1];
    mat.m[2][1] = (*transform).matrix.m[1][2];
    mat.m[0][2] = (*transform).matrix.m[2][0];
    mat.m[1][2] = (*transform).matrix.m[2][1];
    mat.m[2][2] = (*transform).matrix.m[2][2];
    let mut i: usize = 0;
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let mut in_r: f32 = src[0];
        let mut in_g: f32 = src[1];
        let mut in_b: f32 = src[2];
        let mut out_r: f32 =
            mat.m[0][0] * in_r + mat.m[1][0] * in_g + mat.m[2][0] * in_b + (*transform).tx;
        let mut out_g: f32 =
            mat.m[0][1] * in_r + mat.m[1][1] * in_g + mat.m[2][1] * in_b + (*transform).ty;
        let mut out_b: f32 =
            mat.m[0][2] * in_r + mat.m[1][2] * in_g + mat.m[2][2] * in_b + (*transform).tz;
        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}

fn transform_module_matrix(
    mut transform: &qcms_modular_transform,
    mut src: &[f32],
    mut dest: &mut [f32],
) {
    let mut mat: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    /* store the results in column major mode
     * this makes doing the multiplication with sse easier */
    mat.m[0][0] = (*transform).matrix.m[0][0];
    mat.m[1][0] = (*transform).matrix.m[0][1];
    mat.m[2][0] = (*transform).matrix.m[0][2];
    mat.m[0][1] = (*transform).matrix.m[1][0];
    mat.m[1][1] = (*transform).matrix.m[1][1];
    mat.m[2][1] = (*transform).matrix.m[1][2];
    mat.m[0][2] = (*transform).matrix.m[2][0];
    mat.m[1][2] = (*transform).matrix.m[2][1];
    mat.m[2][2] = (*transform).matrix.m[2][2];
    let mut i: usize = 0;
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {

        let mut in_r: f32 = src[0];
        let mut in_g: f32 = src[1];
        let mut in_b: f32 = src[2];
        let mut out_r: f32 = mat.m[0][0] * in_r + mat.m[1][0] * in_g + mat.m[2][0] * in_b;
        let mut out_g: f32 = mat.m[0][1] * in_r + mat.m[1][1] * in_g + mat.m[2][1] * in_b;
        let mut out_b: f32 = mat.m[0][2] * in_r + mat.m[1][2] * in_g + mat.m[2][2] * in_b;
        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}
fn modular_transform_alloc() -> Option<Box<qcms_modular_transform>> {
    return Some(Box::new(Default::default()));
}
fn modular_transform_release(mut t: Option<Box<qcms_modular_transform>>) {
    // destroy a list of transforms non-recursively
    let mut next_transform;
    while let Some(mut transform) = t {
        next_transform = std::mem::replace(&mut (*transform).next_transform, None);
        t = next_transform
    }
}
/* Set transform to be the next element in the linked list. */
fn append_transform(
    mut transform: Option<Box<qcms_modular_transform>>,
    mut next_transform: &mut Option<Box<qcms_modular_transform>>,
) -> &mut Option<Box<qcms_modular_transform>> {
    *next_transform = transform;
    while next_transform.is_some() {
        next_transform = &mut next_transform.as_mut().unwrap().next_transform;
    }
    next_transform
}
/* reverse the transformation list (used by mBA) */
fn reverse_transform(
    mut transform: Option<Box<qcms_modular_transform>>,
) -> Option<Box<qcms_modular_transform>> {
    let mut prev_transform = None;
    while !transform.is_none() {
        let mut next_transform = std::mem::replace(
            &mut transform.as_mut().unwrap().next_transform,
            prev_transform,
        );
        prev_transform = transform;
        transform = next_transform
    }
    return prev_transform;
}
fn modular_transform_create_mAB(mut lut: &lutmABType) -> Option<Box<qcms_modular_transform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    let mut transform;
    if !(*lut).a_curves[0].is_none() {
        let mut clut_length: usize;
        // If the A curve is present this also implies the
        // presence of a CLUT.
        if (*lut).clut_table.is_none() {
            return None;
        }

        // Prepare A curve.
        transform = modular_transform_alloc();
        if transform.is_none() {
            return None;
        }
        transform.as_mut().unwrap().input_clut_table_r =
            build_input_gamma_table((*lut).a_curves[0].as_deref());
        transform.as_mut().unwrap().input_clut_table_g =
            build_input_gamma_table((*lut).a_curves[1].as_deref());
        transform.as_mut().unwrap().input_clut_table_b =
            build_input_gamma_table((*lut).a_curves[2].as_deref());
        transform.as_mut().unwrap().transform_module_fn = Some(transform_module_gamma_table);
        next_transform = append_transform(transform, next_transform);

        if (*lut).num_grid_points[0] as i32 != (*lut).num_grid_points[1] as i32
            || (*lut).num_grid_points[1] as i32 != (*lut).num_grid_points[2] as i32
        {
            //XXX: We don't currently support clut that are not squared!
            return None;
        }

        // Prepare CLUT
        transform = modular_transform_alloc();
        if transform.is_none() {
            return None;
        }

        clut_length = ((*lut).num_grid_points[0] as usize).pow(3) * 3;
        assert_eq!(clut_length, lut.clut_table.as_ref().unwrap().len());
        transform.as_mut().unwrap().clut = lut.clut_table.clone();
        transform.as_mut().unwrap().grid_size = (*lut).num_grid_points[0] as u16;
        transform.as_mut().unwrap().transform_module_fn = Some(transform_module_clut_only);
        next_transform = append_transform(transform, next_transform);
    }

    if !(*lut).m_curves[0].is_none() {
        // M curve imples the presence of a Matrix

        // Prepare M curve
        transform = modular_transform_alloc();
        if transform.is_none() {
            return None;
        }
        transform.as_mut().unwrap().input_clut_table_r =
            build_input_gamma_table((*lut).m_curves[0].as_deref());
        transform.as_mut().unwrap().input_clut_table_g =
            build_input_gamma_table((*lut).m_curves[1].as_deref());
        transform.as_mut().unwrap().input_clut_table_b =
            build_input_gamma_table((*lut).m_curves[2].as_deref());
        transform.as_mut().unwrap().transform_module_fn = Some(transform_module_gamma_table);
        next_transform = append_transform(transform, next_transform);

        // Prepare Matrix
        transform = modular_transform_alloc();
        if transform.is_none() {
            return None;
        }
        transform.as_mut().unwrap().matrix = build_mAB_matrix(lut);
        if transform.as_mut().unwrap().matrix.invalid {
            return None;
        }
        transform.as_mut().unwrap().tx = s15Fixed16Number_to_float((*lut).e03);
        transform.as_mut().unwrap().ty = s15Fixed16Number_to_float((*lut).e13);
        transform.as_mut().unwrap().tz = s15Fixed16Number_to_float((*lut).e23);
        transform.as_mut().unwrap().transform_module_fn = Some(transform_module_matrix_translate);
        next_transform = append_transform(transform, next_transform);
    }

    if !(*lut).b_curves[0].is_none() {
        // Prepare B curve
        transform = modular_transform_alloc();
        if transform.is_none() {
            return None;
        }
        transform.as_mut().unwrap().input_clut_table_r =
            build_input_gamma_table((*lut).b_curves[0].as_deref());
        transform.as_mut().unwrap().input_clut_table_g =
            build_input_gamma_table((*lut).b_curves[1].as_deref());
        transform.as_mut().unwrap().input_clut_table_b =
            build_input_gamma_table((*lut).b_curves[2].as_deref());
        transform.as_mut().unwrap().transform_module_fn = Some(transform_module_gamma_table);
        append_transform(transform, next_transform);
    } else {
        // B curve is mandatory
        return None;
    }

    if (*lut).reversed {
        // mBA are identical to mAB except that the transformation order
        // is reversed
        first_transform = reverse_transform(first_transform)
    }
    return first_transform;
}

fn modular_transform_create_lut(mut lut: &lutType) -> Option<Box<qcms_modular_transform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;

    let mut in_curve_len: usize;
    let mut clut_length: usize;
    let mut out_curve_len: usize;
    let mut in_curves: *mut f32;
    let mut out_curves: *mut f32;
    let mut transform = modular_transform_alloc();
    if !transform.is_none() {
        transform.as_mut().unwrap().matrix = build_lut_matrix(Some(lut));
        if !transform.as_mut().unwrap().matrix.invalid {
            transform.as_mut().unwrap().transform_module_fn = Some(transform_module_matrix);
            next_transform = append_transform(transform, next_transform);
            // Prepare input curves
            transform = modular_transform_alloc();
            if !transform.is_none() {
                if true {
                    transform.as_mut().unwrap().input_clut_table_r = Some(
                        (*lut).input_table[0..(*lut).num_input_table_entries as usize].to_vec(),
                    );
                    transform.as_mut().unwrap().input_clut_table_g = Some(
                        (*lut).input_table[(*lut).num_input_table_entries as usize
                            ..(*lut).num_input_table_entries as usize * 2]
                            .to_vec(),
                    );
                    transform.as_mut().unwrap().input_clut_table_b = Some(
                        (*lut).input_table[(*lut).num_input_table_entries as usize * 2
                            ..(*lut).num_input_table_entries as usize * 3]
                            .to_vec(),
                    );
                    transform.as_mut().unwrap().input_clut_table_length =
                        (*lut).num_input_table_entries;
                    // Prepare table
                    clut_length = ((*lut).num_clut_grid_points as usize).pow(3) * 3;
                    if true {
                        assert_eq!(clut_length, lut.clut_table.len());
                        transform.as_mut().unwrap().clut = Some((*lut).clut_table.clone());

                        transform.as_mut().unwrap().grid_size = (*lut).num_clut_grid_points as u16;
                        // Prepare output curves
                        if true {
                            transform.as_mut().unwrap().output_clut_table_r = Some(
                                (*lut).output_table[0..(*lut).num_output_table_entries as usize]
                                    .to_vec(),
                            );
                            transform.as_mut().unwrap().output_clut_table_g = Some(
                                (*lut).output_table[(*lut).num_output_table_entries as usize
                                    ..(*lut).num_output_table_entries as usize * 2]
                                    .to_vec(),
                            );
                            transform.as_mut().unwrap().output_clut_table_b = Some(
                                (*lut).output_table[(*lut).num_output_table_entries as usize * 2
                                    ..(*lut).num_output_table_entries as usize * 3]
                                    .to_vec(),
                            );
                            transform.as_mut().unwrap().output_clut_table_length =
                                (*lut).num_output_table_entries;
                            transform.as_mut().unwrap().transform_module_fn =
                                Some(transform_module_clut);
                            append_transform(transform, next_transform);
                            return first_transform;
                        }
                    }
                }
            }
        }
    }
    modular_transform_release(first_transform);
    return None;
}

fn modular_transform_create_input(mut in_0: &qcms_profile) -> Option<Box<qcms_modular_transform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    if !(*in_0).A2B0.is_none() {
        let mut lut_transform = modular_transform_create_lut((*in_0).A2B0.as_deref().unwrap());
        if lut_transform.is_none() {
            return None;
        } else {
            append_transform(lut_transform, next_transform);
        }
    } else if !(*in_0).mAB.is_none()
        && (*(*in_0).mAB.as_deref().unwrap()).num_in_channels as i32 == 3
        && (*(*in_0).mAB.as_deref().unwrap()).num_out_channels as i32 == 3
    {
        let mut mAB_transform = modular_transform_create_mAB((*in_0).mAB.as_deref().unwrap());
        if mAB_transform.is_none() {
            return None;
        } else {
            append_transform(mAB_transform, next_transform);
        }
    } else {
        let mut transform = modular_transform_alloc();
        if transform.is_none() {
            return None;
        } else {
            transform.as_mut().unwrap().input_clut_table_r =
                build_input_gamma_table((*in_0).redTRC.as_deref());
            transform.as_mut().unwrap().input_clut_table_g =
                build_input_gamma_table((*in_0).greenTRC.as_deref());
            transform.as_mut().unwrap().input_clut_table_b =
                build_input_gamma_table((*in_0).blueTRC.as_deref());
            transform.as_mut().unwrap().transform_module_fn = Some(transform_module_gamma_table);
            if transform.as_mut().unwrap().input_clut_table_r.is_none()
                || transform.as_mut().unwrap().input_clut_table_g.is_none()
                || transform.as_mut().unwrap().input_clut_table_b.is_none()
            {
                append_transform(transform, next_transform);
                return None;
            } else {
                next_transform = append_transform(transform, next_transform);
                transform = modular_transform_alloc();
                if transform.is_none() {
                    return None;
                } else {
                    transform.as_mut().unwrap().matrix.m[0][0] = 1. / 1.999969482421875;
                    transform.as_mut().unwrap().matrix.m[0][1] = 0.0;
                    transform.as_mut().unwrap().matrix.m[0][2] = 0.0;
                    transform.as_mut().unwrap().matrix.m[1][0] = 0.0;
                    transform.as_mut().unwrap().matrix.m[1][1] = 1. / 1.999969482421875;
                    transform.as_mut().unwrap().matrix.m[1][2] = 0.0;
                    transform.as_mut().unwrap().matrix.m[2][0] = 0.0;
                    transform.as_mut().unwrap().matrix.m[2][1] = 0.0;
                    transform.as_mut().unwrap().matrix.m[2][2] = 1. / 1.999969482421875;
                    transform.as_mut().unwrap().matrix.invalid = false;
                    transform.as_mut().unwrap().transform_module_fn = Some(transform_module_matrix);
                    next_transform = append_transform(transform, next_transform);
                    transform = modular_transform_alloc();
                    if transform.is_none() {
                        return None;
                    } else {
                        transform.as_mut().unwrap().matrix = build_colorant_matrix(in_0);
                        transform.as_mut().unwrap().transform_module_fn =
                            Some(transform_module_matrix);
                        append_transform(transform, next_transform);
                    }
                }
            }
        }
    }
    first_transform
}
fn modular_transform_create_output(mut out: &qcms_profile) -> Option<Box<qcms_modular_transform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    if !(*out).B2A0.is_none() {
        let mut lut_transform = modular_transform_create_lut((*out).B2A0.as_deref().unwrap());
        if lut_transform.is_none() {
            return None;
        } else {
            append_transform(lut_transform, next_transform);
        }
    } else if !(*out).mBA.is_none()
        && (*(*out).mBA.as_deref().unwrap()).num_in_channels as i32 == 3
        && (*(*out).mBA.as_deref().unwrap()).num_out_channels as i32 == 3
    {
        let mut lut_transform_0 = modular_transform_create_mAB((*out).mBA.as_deref().unwrap());
        if lut_transform_0.is_none() {
            return None;
        } else {
            append_transform(lut_transform_0, next_transform);
        }
    } else if !(*out).redTRC.is_none() && !(*out).greenTRC.is_none() && !(*out).blueTRC.is_none() {
        let mut transform = modular_transform_alloc();
        if transform.is_none() {
            return None;
        } else {
            transform.as_mut().unwrap().matrix = matrix_invert(build_colorant_matrix(out));
            transform.as_mut().unwrap().transform_module_fn = Some(transform_module_matrix);
            next_transform = append_transform(transform, next_transform);
            transform = modular_transform_alloc();
            if transform.is_none() {
                return None;
            } else {
                transform.as_mut().unwrap().matrix.m[0][0] = 1.999969482421875;
                transform.as_mut().unwrap().matrix.m[0][1] = 0.0;
                transform.as_mut().unwrap().matrix.m[0][2] = 0.0;
                transform.as_mut().unwrap().matrix.m[1][0] = 0.0;
                transform.as_mut().unwrap().matrix.m[1][1] = 1.999969482421875;
                transform.as_mut().unwrap().matrix.m[1][2] = 0.0;
                transform.as_mut().unwrap().matrix.m[2][0] = 0.0;
                transform.as_mut().unwrap().matrix.m[2][1] = 0.0;
                transform.as_mut().unwrap().matrix.m[2][2] = 1.999969482421875;
                transform.as_mut().unwrap().matrix.invalid = false;
                transform.as_mut().unwrap().transform_module_fn = Some(transform_module_matrix);
                next_transform = append_transform(transform, next_transform);
                transform = modular_transform_alloc();
                if transform.is_none() {
                    return None;
                } else {
                    transform.as_mut().unwrap().output_gamma_lut_r =
                        Some(build_output_lut((*out).redTRC.as_deref().unwrap()));
                    transform.as_mut().unwrap().output_gamma_lut_g =
                        Some(build_output_lut((*out).greenTRC.as_deref().unwrap()));
                    transform.as_mut().unwrap().output_gamma_lut_b =
                        Some(build_output_lut((*out).blueTRC.as_deref().unwrap()));
                    transform.as_mut().unwrap().transform_module_fn =
                        Some(transform_module_gamma_lut);
                    if transform.as_mut().unwrap().output_gamma_lut_r.is_none()
                        || transform.as_mut().unwrap().output_gamma_lut_g.is_none()
                        || transform.as_mut().unwrap().output_gamma_lut_b.is_none()
                    {
                        return None;
                    } else {
                        append_transform(transform, next_transform);
                    }
                }
            }
        }
    } else {
        debug_assert!(false, "Unsupported output profile workflow.");
        return None;
    }
    first_transform
}
/* Not Completed
// Simplify the transformation chain to an equivalent transformation chain
static struct qcms_modular_transform* qcms_modular_transform_reduce(struct qcms_modular_transform *transform)
{
    struct qcms_modular_transform *first_transform = NULL;
    struct qcms_modular_transform *curr_trans = transform;
    struct qcms_modular_transform *prev_trans = NULL;
    while (curr_trans) {
        struct qcms_modular_transform *next_trans = curr_trans->next_transform;
        if (curr_trans->transform_module_fn == qcms_transform_module_matrix) {
            if (next_trans && next_trans->transform_module_fn == qcms_transform_module_matrix) {
                curr_trans->matrix = matrix_multiply(curr_trans->matrix, next_trans->matrix);
                goto remove_next;
            }
        }
        if (curr_trans->transform_module_fn == qcms_transform_module_gamma_table) {
            bool isLinear = true;
            uint16_t i;
            for (i = 0; isLinear && i < 256; i++) {
                isLinear &= (int)(curr_trans->input_clut_table_r[i] * 255) == i;
                isLinear &= (int)(curr_trans->input_clut_table_g[i] * 255) == i;
                isLinear &= (int)(curr_trans->input_clut_table_b[i] * 255) == i;
            }
            goto remove_current;
        }

next_transform:
        if (!next_trans) break;
        prev_trans = curr_trans;
        curr_trans = next_trans;
        continue;
remove_current:
        if (curr_trans == transform) {
            //Update head
            transform = next_trans;
        } else {
            prev_trans->next_transform = next_trans;
        }
        curr_trans->next_transform = NULL;
        qcms_modular_transform_release(curr_trans);
        //return transform;
        return qcms_modular_transform_reduce(transform);
remove_next:
        curr_trans->next_transform = next_trans->next_transform;
        next_trans->next_transform = NULL;
        qcms_modular_transform_release(next_trans);
        continue;
    }
    return transform;
}
*/
fn modular_transform_create(
    mut in_0: &qcms_profile,
    mut out: &qcms_profile,
) -> Option<Box<qcms_modular_transform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    if (*in_0).color_space == RGB_SIGNATURE {
        let mut rgb_to_pcs = modular_transform_create_input(in_0);
        if rgb_to_pcs.is_none() {
            return None;
        }
        next_transform = append_transform(rgb_to_pcs, next_transform);
    } else {
        debug_assert!(false, "input color space not supported");
        return None;
    }

    if (*in_0).pcs == LAB_SIGNATURE && (*out).pcs == XYZ_SIGNATURE {
        let mut lab_to_pcs = modular_transform_alloc();
        if lab_to_pcs.is_none() {
            return None;
        }
        lab_to_pcs.as_mut().unwrap().transform_module_fn = Some(transform_module_LAB_to_XYZ);
        next_transform = append_transform(lab_to_pcs, next_transform);
    }

    // This does not improve accuracy in practice, something is wrong here.
    //if (in->chromaticAdaption.invalid == false) {
    //	struct qcms_modular_transform* chromaticAdaption;
    //	chromaticAdaption = qcms_modular_transform_alloc();
    //	if (!chromaticAdaption)
    //		goto fail;
    //	append_transform(chromaticAdaption, &next_transform);
    //	chromaticAdaption->matrix = matrix_invert(in->chromaticAdaption);
    //	chromaticAdaption->transform_module_fn = qcms_transform_module_matrix;
    //}

    if (*in_0).pcs == XYZ_SIGNATURE && (*out).pcs == LAB_SIGNATURE {
        let mut pcs_to_lab = modular_transform_alloc();
        if pcs_to_lab.is_none() {
            return None;
        }
        pcs_to_lab.as_mut().unwrap().transform_module_fn = Some(transform_module_XYZ_to_LAB);
        next_transform = append_transform(pcs_to_lab, next_transform);
    }

    if (*out).color_space == RGB_SIGNATURE {
        let mut pcs_to_rgb = modular_transform_create_output(out);
        if pcs_to_rgb.is_none() {
            return None;
        }
        append_transform(pcs_to_rgb, next_transform);
    } else {
        debug_assert!(false, "output color space not supported");
    }

    // Not Completed
    //return qcms_modular_transform_reduce(first_transform);
    return first_transform;
}
unsafe fn modular_transform_data(
    mut transform: Option<&qcms_modular_transform>,
    mut src: Vec<f32>,
    mut dest: Vec<f32>,
    mut len: usize,
) -> Option<Vec<f32>> {
    while !transform.is_none() {
        // Keep swaping src/dest when performing a transform to use less memory.
        let transform_fn: transform_module_fn_t = transform.unwrap().transform_module_fn;
        transform
            .unwrap()
            .transform_module_fn
            .expect("non-null function pointer")(
            transform.as_ref().unwrap(),
            &src,
            &mut dest,
        );
        std::mem::swap(&mut src, &mut dest);
        transform = transform.unwrap().next_transform.as_deref();
    }
    // The results end up in the src buffer because of the switching
    return Some(src);
}

pub unsafe fn chain_transform(
    mut in_0: &qcms_profile,
    mut out: &qcms_profile,
    mut src: Vec<f32>,
    mut dest: Vec<f32>,
    mut lutSize: usize,
) -> Option<Vec<f32>> {
    let mut transform_list = modular_transform_create(in_0, out);
    if !transform_list.is_none() {
        let mut lut = modular_transform_data(transform_list.as_deref(), src, dest, lutSize / 3);
        modular_transform_release(transform_list);
        return lut;
    }
    return None;
}

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
    iccread::{lutType, lutmABType, Profile},
    matrix::Matrix,
    s15Fixed16Number_to_float,
    transform_util::clamp_float,
    transform_util::{
        build_colorant_matrix, build_input_gamma_table, build_output_lut, lut_interp_linear,
        lut_interp_linear_float,
    },
};

#[derive(Clone, Default)]
pub struct ModularTransform {
    matrix: Matrix,
    tx: f32,
    ty: f32,
    tz: f32,
    input_clut_table: [Option<Vec<f32>>; 3],
    clut: Option<Vec<f32>>,
    grid_size: u16,
    output_clut_table: [Option<Vec<f32>>; 3],
    output_clut_table_length: u16,
    output_gamma_lut_r: Option<Vec<u16>>,
    output_gamma_lut_g: Option<Vec<u16>>,
    output_gamma_lut_b: Option<Vec<u16>>,
    output_gamma_lut_r_length: usize,
    output_gamma_lut_g_length: usize,
    output_gamma_lut_b_length: usize,
    transform_module_fn: TransformModuleFn,
    next_transform: Option<Box<ModularTransform>>,
}
pub type TransformModuleFn = Option<fn(_: &ModularTransform, _: &[f32], _: &mut [f32]) -> ()>;

#[inline]
fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a * (1.0 - t) + b * t
}

fn build_lut_matrix(lut: Option<&lutType>) -> Matrix {
    let mut result: Matrix = Matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    if let Some(lut) = lut {
        result.m[0][0] = s15Fixed16Number_to_float(lut.e00);
        result.m[0][1] = s15Fixed16Number_to_float(lut.e01);
        result.m[0][2] = s15Fixed16Number_to_float(lut.e02);
        result.m[1][0] = s15Fixed16Number_to_float(lut.e10);
        result.m[1][1] = s15Fixed16Number_to_float(lut.e11);
        result.m[1][2] = s15Fixed16Number_to_float(lut.e12);
        result.m[2][0] = s15Fixed16Number_to_float(lut.e20);
        result.m[2][1] = s15Fixed16Number_to_float(lut.e21);
        result.m[2][2] = s15Fixed16Number_to_float(lut.e22);
        result.invalid = false
    } else {
        result.m = Default::default();
        result.invalid = true
    }
    result
}
fn build_mAB_matrix(lut: &lutmABType) -> Matrix {
    let mut result: Matrix = Matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };

    result.m[0][0] = s15Fixed16Number_to_float(lut.e00);
    result.m[0][1] = s15Fixed16Number_to_float(lut.e01);
    result.m[0][2] = s15Fixed16Number_to_float(lut.e02);
    result.m[1][0] = s15Fixed16Number_to_float(lut.e10);
    result.m[1][1] = s15Fixed16Number_to_float(lut.e11);
    result.m[1][2] = s15Fixed16Number_to_float(lut.e12);
    result.m[2][0] = s15Fixed16Number_to_float(lut.e20);
    result.m[2][1] = s15Fixed16Number_to_float(lut.e21);
    result.m[2][2] = s15Fixed16Number_to_float(lut.e22);
    result.invalid = false;

    result
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

fn transform_module_LAB_to_XYZ(_transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    // lcms: D50 XYZ values
    let WhitePointX: f32 = 0.9642;
    let WhitePointY: f32 = 1.0;
    let WhitePointZ: f32 = 0.8249;

    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let device_L: f32 = src[0] * 100.0;
        let device_a: f32 = src[1] * 255.0 - 128.0;
        let device_b: f32 = src[2] * 255.0 - 128.0;

        let y: f32 = (device_L + 16.0) / 116.0;

        let X = f_1(y + 0.002 * device_a) * WhitePointX;
        let Y = f_1(y) * WhitePointY;
        let Z = f_1(y - 0.005 * device_b) * WhitePointZ;

        dest[0] = (X as f64 / (1.0f64 + 32767.0f64 / 32768.0f64)) as f32;
        dest[1] = (Y as f64 / (1.0f64 + 32767.0f64 / 32768.0f64)) as f32;
        dest[2] = (Z as f64 / (1.0f64 + 32767.0f64 / 32768.0f64)) as f32;
    }
}
//Based on lcms cmsXYZ2Lab
fn transform_module_XYZ_to_LAB(_transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    // lcms: D50 XYZ values
    let WhitePointX: f32 = 0.9642;
    let WhitePointY: f32 = 1.0;
    let WhitePointZ: f32 = 0.8249;
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let device_x: f32 =
            (src[0] as f64 * (1.0f64 + 32767.0f64 / 32768.0f64) / WhitePointX as f64) as f32;
        let device_y: f32 =
            (src[1] as f64 * (1.0f64 + 32767.0f64 / 32768.0f64) / WhitePointY as f64) as f32;
        let device_z: f32 =
            (src[2] as f64 * (1.0f64 + 32767.0f64 / 32768.0f64) / WhitePointZ as f64) as f32;

        let fx = f(device_x);
        let fy = f(device_y);
        let fz = f(device_z);

        let L: f32 = 116.0 * fy - 16.0;
        let a: f32 = 500.0 * (fx - fy);
        let b: f32 = 200.0 * (fy - fz);

        dest[0] = L / 100.0;
        dest[1] = (a + 128.0) / 255.0;
        dest[2] = (b + 128.0) / 255.0;
    }
}
fn transform_module_clut_only(transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    let xy_len: i32 = 1;
    let x_len: i32 = transform.grid_size as i32;
    let len: i32 = x_len * x_len;

    let r_table = &transform.clut.as_ref().unwrap()[0..];
    let g_table = &transform.clut.as_ref().unwrap()[1..];
    let b_table = &transform.clut.as_ref().unwrap()[2..];

    let CLU = |table: &[f32], x, y, z| table[((x * len + y * x_len + z * xy_len) * 3) as usize];

    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        debug_assert!(transform.grid_size as i32 >= 1);
        let linear_r: f32 = src[0];
        let linear_g: f32 = src[1];
        let linear_b: f32 = src[2];
        let x: i32 = (linear_r * (transform.grid_size as i32 - 1) as f32).floor() as i32;
        let y: i32 = (linear_g * (transform.grid_size as i32 - 1) as f32).floor() as i32;
        let z: i32 = (linear_b * (transform.grid_size as i32 - 1) as f32).floor() as i32;
        let x_n: i32 = (linear_r * (transform.grid_size as i32 - 1) as f32).ceil() as i32;
        let y_n: i32 = (linear_g * (transform.grid_size as i32 - 1) as f32).ceil() as i32;
        let z_n: i32 = (linear_b * (transform.grid_size as i32 - 1) as f32).ceil() as i32;
        let x_d: f32 = linear_r * (transform.grid_size as i32 - 1) as f32 - x as f32;
        let y_d: f32 = linear_g * (transform.grid_size as i32 - 1) as f32 - y as f32;
        let z_d: f32 = linear_b * (transform.grid_size as i32 - 1) as f32 - z as f32;

        let r_x1: f32 = lerp(CLU(r_table, x, y, z), CLU(r_table, x_n, y, z), x_d);
        let r_x2: f32 = lerp(CLU(r_table, x, y_n, z), CLU(r_table, x_n, y_n, z), x_d);
        let r_y1: f32 = lerp(r_x1, r_x2, y_d);
        let r_x3: f32 = lerp(CLU(r_table, x, y, z_n), CLU(r_table, x_n, y, z_n), x_d);
        let r_x4: f32 = lerp(CLU(r_table, x, y_n, z_n), CLU(r_table, x_n, y_n, z_n), x_d);
        let r_y2: f32 = lerp(r_x3, r_x4, y_d);
        let clut_r: f32 = lerp(r_y1, r_y2, z_d);

        let g_x1: f32 = lerp(CLU(g_table, x, y, z), CLU(g_table, x_n, y, z), x_d);
        let g_x2: f32 = lerp(CLU(g_table, x, y_n, z), CLU(g_table, x_n, y_n, z), x_d);
        let g_y1: f32 = lerp(g_x1, g_x2, y_d);
        let g_x3: f32 = lerp(CLU(g_table, x, y, z_n), CLU(g_table, x_n, y, z_n), x_d);
        let g_x4: f32 = lerp(CLU(g_table, x, y_n, z_n), CLU(g_table, x_n, y_n, z_n), x_d);
        let g_y2: f32 = lerp(g_x3, g_x4, y_d);
        let clut_g: f32 = lerp(g_y1, g_y2, z_d);

        let b_x1: f32 = lerp(CLU(b_table, x, y, z), CLU(b_table, x_n, y, z), x_d);
        let b_x2: f32 = lerp(CLU(b_table, x, y_n, z), CLU(b_table, x_n, y_n, z), x_d);
        let b_y1: f32 = lerp(b_x1, b_x2, y_d);
        let b_x3: f32 = lerp(CLU(b_table, x, y, z_n), CLU(b_table, x_n, y, z_n), x_d);
        let b_x4: f32 = lerp(CLU(b_table, x, y_n, z_n), CLU(b_table, x_n, y_n, z_n), x_d);
        let b_y2: f32 = lerp(b_x3, b_x4, y_d);
        let clut_b: f32 = lerp(b_y1, b_y2, z_d);

        dest[0] = clamp_float(clut_r);
        dest[1] = clamp_float(clut_g);
        dest[2] = clamp_float(clut_b);
    }
}
fn transform_module_clut(transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    let xy_len: i32 = 1;
    let x_len: i32 = transform.grid_size as i32;
    let len: i32 = x_len * x_len;

    let r_table = &transform.clut.as_ref().unwrap()[0..];
    let g_table = &transform.clut.as_ref().unwrap()[1..];
    let b_table = &transform.clut.as_ref().unwrap()[2..];
    let CLU = |table: &[f32], x, y, z| table[((x * len + y * x_len + z * xy_len) * 3) as usize];

    let input_clut_table_r = transform.input_clut_table[0].as_ref().unwrap();
    let input_clut_table_g = transform.input_clut_table[1].as_ref().unwrap();
    let input_clut_table_b = transform.input_clut_table[2].as_ref().unwrap();
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        debug_assert!(transform.grid_size as i32 >= 1);
        let device_r: f32 = src[0];
        let device_g: f32 = src[1];
        let device_b: f32 = src[2];
        let linear_r: f32 = lut_interp_linear_float(device_r, &input_clut_table_r);
        let linear_g: f32 = lut_interp_linear_float(device_g, &input_clut_table_g);
        let linear_b: f32 = lut_interp_linear_float(device_b, &input_clut_table_b);
        let x: i32 = (linear_r * (transform.grid_size as i32 - 1) as f32).floor() as i32;
        let y: i32 = (linear_g * (transform.grid_size as i32 - 1) as f32).floor() as i32;
        let z: i32 = (linear_b * (transform.grid_size as i32 - 1) as f32).floor() as i32;
        let x_n: i32 = (linear_r * (transform.grid_size as i32 - 1) as f32).ceil() as i32;
        let y_n: i32 = (linear_g * (transform.grid_size as i32 - 1) as f32).ceil() as i32;
        let z_n: i32 = (linear_b * (transform.grid_size as i32 - 1) as f32).ceil() as i32;
        let x_d: f32 = linear_r * (transform.grid_size as i32 - 1) as f32 - x as f32;
        let y_d: f32 = linear_g * (transform.grid_size as i32 - 1) as f32 - y as f32;
        let z_d: f32 = linear_b * (transform.grid_size as i32 - 1) as f32 - z as f32;

        let r_x1: f32 = lerp(CLU(r_table, x, y, z), CLU(r_table, x_n, y, z), x_d);
        let r_x2: f32 = lerp(CLU(r_table, x, y_n, z), CLU(r_table, x_n, y_n, z), x_d);
        let r_y1: f32 = lerp(r_x1, r_x2, y_d);
        let r_x3: f32 = lerp(CLU(r_table, x, y, z_n), CLU(r_table, x_n, y, z_n), x_d);
        let r_x4: f32 = lerp(CLU(r_table, x, y_n, z_n), CLU(r_table, x_n, y_n, z_n), x_d);
        let r_y2: f32 = lerp(r_x3, r_x4, y_d);
        let clut_r: f32 = lerp(r_y1, r_y2, z_d);

        let g_x1: f32 = lerp(CLU(g_table, x, y, z), CLU(g_table, x_n, y, z), x_d);
        let g_x2: f32 = lerp(CLU(g_table, x, y_n, z), CLU(g_table, x_n, y_n, z), x_d);
        let g_y1: f32 = lerp(g_x1, g_x2, y_d);
        let g_x3: f32 = lerp(CLU(g_table, x, y, z_n), CLU(g_table, x_n, y, z_n), x_d);
        let g_x4: f32 = lerp(CLU(g_table, x, y_n, z_n), CLU(g_table, x_n, y_n, z_n), x_d);
        let g_y2: f32 = lerp(g_x3, g_x4, y_d);
        let clut_g: f32 = lerp(g_y1, g_y2, z_d);

        let b_x1: f32 = lerp(CLU(b_table, x, y, z), CLU(b_table, x_n, y, z), x_d);
        let b_x2: f32 = lerp(CLU(b_table, x, y_n, z), CLU(b_table, x_n, y_n, z), x_d);
        let b_y1: f32 = lerp(b_x1, b_x2, y_d);
        let b_x3: f32 = lerp(CLU(b_table, x, y, z_n), CLU(b_table, x_n, y, z_n), x_d);
        let b_x4: f32 = lerp(CLU(b_table, x, y_n, z_n), CLU(b_table, x_n, y_n, z_n), x_d);
        let b_y2: f32 = lerp(b_x3, b_x4, y_d);
        let clut_b: f32 = lerp(b_y1, b_y2, z_d);
        let pcs_r: f32 =
            lut_interp_linear_float(clut_r, &transform.output_clut_table[0].as_ref().unwrap());
        let pcs_g: f32 =
            lut_interp_linear_float(clut_g, &transform.output_clut_table[1].as_ref().unwrap());
        let pcs_b: f32 =
            lut_interp_linear_float(clut_b, &transform.output_clut_table[2].as_ref().unwrap());
        dest[0] = clamp_float(pcs_r);
        dest[1] = clamp_float(pcs_g);
        dest[2] = clamp_float(pcs_b);
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
fn transform_module_gamma_table(transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    let mut out_r: f32;
    let mut out_g: f32;
    let mut out_b: f32;
    let input_clut_table_r = transform.input_clut_table[0].as_ref().unwrap();
    let input_clut_table_g = transform.input_clut_table[1].as_ref().unwrap();
    let input_clut_table_b = transform.input_clut_table[2].as_ref().unwrap();

    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let in_r: f32 = src[0];
        let in_g: f32 = src[1];
        let in_b: f32 = src[2];
        out_r = lut_interp_linear_float(in_r, input_clut_table_r);
        out_g = lut_interp_linear_float(in_g, input_clut_table_g);
        out_b = lut_interp_linear_float(in_b, input_clut_table_b);

        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}
fn transform_module_gamma_lut(transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    let mut out_r: f32;
    let mut out_g: f32;
    let mut out_b: f32;
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let in_r: f32 = src[0];
        let in_g: f32 = src[1];
        let in_b: f32 = src[2];
        out_r = lut_interp_linear(in_r as f64, &transform.output_gamma_lut_r.as_ref().unwrap());
        out_g = lut_interp_linear(in_g as f64, &transform.output_gamma_lut_g.as_ref().unwrap());
        out_b = lut_interp_linear(in_b as f64, &transform.output_gamma_lut_b.as_ref().unwrap());
        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}
fn transform_module_matrix_translate(transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    let mut mat: Matrix = Matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    /* store the results in column major mode
     * this makes doing the multiplication with sse easier */
    mat.m[0][0] = transform.matrix.m[0][0];
    mat.m[1][0] = transform.matrix.m[0][1];
    mat.m[2][0] = transform.matrix.m[0][2];
    mat.m[0][1] = transform.matrix.m[1][0];
    mat.m[1][1] = transform.matrix.m[1][1];
    mat.m[2][1] = transform.matrix.m[1][2];
    mat.m[0][2] = transform.matrix.m[2][0];
    mat.m[1][2] = transform.matrix.m[2][1];
    mat.m[2][2] = transform.matrix.m[2][2];
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let in_r: f32 = src[0];
        let in_g: f32 = src[1];
        let in_b: f32 = src[2];
        let out_r: f32 =
            mat.m[0][0] * in_r + mat.m[1][0] * in_g + mat.m[2][0] * in_b + transform.tx;
        let out_g: f32 =
            mat.m[0][1] * in_r + mat.m[1][1] * in_g + mat.m[2][1] * in_b + transform.ty;
        let out_b: f32 =
            mat.m[0][2] * in_r + mat.m[1][2] * in_g + mat.m[2][2] * in_b + transform.tz;
        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}

fn transform_module_matrix(transform: &ModularTransform, src: &[f32], dest: &mut [f32]) {
    let mut mat: Matrix = Matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    /* store the results in column major mode
     * this makes doing the multiplication with sse easier */
    mat.m[0][0] = transform.matrix.m[0][0];
    mat.m[1][0] = transform.matrix.m[0][1];
    mat.m[2][0] = transform.matrix.m[0][2];
    mat.m[0][1] = transform.matrix.m[1][0];
    mat.m[1][1] = transform.matrix.m[1][1];
    mat.m[2][1] = transform.matrix.m[1][2];
    mat.m[0][2] = transform.matrix.m[2][0];
    mat.m[1][2] = transform.matrix.m[2][1];
    mat.m[2][2] = transform.matrix.m[2][2];
    for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
        let in_r: f32 = src[0];
        let in_g: f32 = src[1];
        let in_b: f32 = src[2];
        let out_r: f32 = mat.m[0][0] * in_r + mat.m[1][0] * in_g + mat.m[2][0] * in_b;
        let out_g: f32 = mat.m[0][1] * in_r + mat.m[1][1] * in_g + mat.m[2][1] * in_b;
        let out_b: f32 = mat.m[0][2] * in_r + mat.m[1][2] * in_g + mat.m[2][2] * in_b;
        dest[0] = clamp_float(out_r);
        dest[1] = clamp_float(out_g);
        dest[2] = clamp_float(out_b);
    }
}
fn modular_transform_alloc() -> Box<ModularTransform> {
    Box::new(Default::default())
}
fn modular_transform_release(mut t: Option<Box<ModularTransform>>) {
    // destroy a list of transforms non-recursively
    let mut next_transform;
    while let Some(mut transform) = t {
        next_transform = std::mem::replace(&mut transform.next_transform, None);
        t = next_transform
    }
}
/* Set transform to be the next element in the linked list. */
fn append_transform(
    transform: Option<Box<ModularTransform>>,
    mut next_transform: &mut Option<Box<ModularTransform>>,
) -> &mut Option<Box<ModularTransform>> {
    *next_transform = transform;
    while next_transform.is_some() {
        next_transform = &mut next_transform.as_mut().unwrap().next_transform;
    }
    next_transform
}
/* reverse the transformation list (used by mBA) */
fn reverse_transform(
    mut transform: Option<Box<ModularTransform>>,
) -> Option<Box<ModularTransform>> {
    let mut prev_transform = None;
    while transform.is_some() {
        let next_transform = std::mem::replace(
            &mut transform.as_mut().unwrap().next_transform,
            prev_transform,
        );
        prev_transform = transform;
        transform = next_transform
    }
    prev_transform
}
fn modular_transform_create_mAB(lut: &lutmABType) -> Option<Box<ModularTransform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    let mut transform;
    if lut.a_curves[0].is_some() {
        let clut_length: usize;
        // If the A curve is present this also implies the
        // presence of a CLUT.
        lut.clut_table.as_ref()?;

        // Prepare A curve.
        transform = modular_transform_alloc();
        transform.input_clut_table[0] = build_input_gamma_table(lut.a_curves[0].as_deref());
        transform.input_clut_table[1] = build_input_gamma_table(lut.a_curves[1].as_deref());
        transform.input_clut_table[2] = build_input_gamma_table(lut.a_curves[2].as_deref());
        transform.transform_module_fn = Some(transform_module_gamma_table);
        next_transform = append_transform(Some(transform), next_transform);

        if lut.num_grid_points[0] as i32 != lut.num_grid_points[1] as i32
            || lut.num_grid_points[1] as i32 != lut.num_grid_points[2] as i32
        {
            //XXX: We don't currently support clut that are not squared!
            return None;
        }

        // Prepare CLUT
        transform = modular_transform_alloc();

        clut_length = (lut.num_grid_points[0] as usize).pow(3) * 3;
        assert_eq!(clut_length, lut.clut_table.as_ref().unwrap().len());
        transform.clut = lut.clut_table.clone();
        transform.grid_size = lut.num_grid_points[0] as u16;
        transform.transform_module_fn = Some(transform_module_clut_only);
        next_transform = append_transform(Some(transform), next_transform);
    }

    if lut.m_curves[0].is_some() {
        // M curve imples the presence of a Matrix

        // Prepare M curve
        transform = modular_transform_alloc();
        transform.input_clut_table[0] = build_input_gamma_table(lut.m_curves[0].as_deref());
        transform.input_clut_table[1] = build_input_gamma_table(lut.m_curves[1].as_deref());
        transform.input_clut_table[2] = build_input_gamma_table(lut.m_curves[2].as_deref());
        transform.transform_module_fn = Some(transform_module_gamma_table);
        next_transform = append_transform(Some(transform), next_transform);

        // Prepare Matrix
        transform = modular_transform_alloc();
        transform.matrix = build_mAB_matrix(lut);
        if transform.matrix.invalid {
            return None;
        }
        transform.tx = s15Fixed16Number_to_float(lut.e03);
        transform.ty = s15Fixed16Number_to_float(lut.e13);
        transform.tz = s15Fixed16Number_to_float(lut.e23);
        transform.transform_module_fn = Some(transform_module_matrix_translate);
        next_transform = append_transform(Some(transform), next_transform);
    }

    if lut.b_curves[0].is_some() {
        // Prepare B curve
        transform = modular_transform_alloc();
        transform.input_clut_table[0] = build_input_gamma_table(lut.b_curves[0].as_deref());
        transform.input_clut_table[1] = build_input_gamma_table(lut.b_curves[1].as_deref());
        transform.input_clut_table[2] = build_input_gamma_table(lut.b_curves[2].as_deref());
        transform.transform_module_fn = Some(transform_module_gamma_table);
        append_transform(Some(transform), next_transform);
    } else {
        // B curve is mandatory
        return None;
    }

    if lut.reversed {
        // mBA are identical to mAB except that the transformation order
        // is reversed
        first_transform = reverse_transform(first_transform)
    }
    first_transform
}

fn modular_transform_create_lut(lut: &lutType) -> Option<Box<ModularTransform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;

    let clut_length: usize;
    let mut transform = modular_transform_alloc();

    transform.matrix = build_lut_matrix(Some(lut));
    if !transform.matrix.invalid {
        transform.transform_module_fn = Some(transform_module_matrix);
        next_transform = append_transform(Some(transform), next_transform);
        // Prepare input curves
        transform = modular_transform_alloc();
        transform.input_clut_table[0] =
            Some(lut.input_table[0..lut.num_input_table_entries as usize].to_vec());
        transform.input_clut_table[1] = Some(
            lut.input_table
                [lut.num_input_table_entries as usize..lut.num_input_table_entries as usize * 2]
                .to_vec(),
        );
        transform.input_clut_table[2] = Some(
            lut.input_table[lut.num_input_table_entries as usize * 2
                ..lut.num_input_table_entries as usize * 3]
                .to_vec(),
        );
        // Prepare table
        clut_length = (lut.num_clut_grid_points as usize).pow(3) * 3;
        assert_eq!(clut_length, lut.clut_table.len());
        transform.clut = Some(lut.clut_table.clone());

        transform.grid_size = lut.num_clut_grid_points as u16;
        // Prepare output curves
        transform.output_clut_table[0] =
            Some(lut.output_table[0..lut.num_output_table_entries as usize].to_vec());
        transform.output_clut_table[1] = Some(
            lut.output_table
                [lut.num_output_table_entries as usize..lut.num_output_table_entries as usize * 2]
                .to_vec(),
        );
        transform.output_clut_table[2] = Some(
            lut.output_table[lut.num_output_table_entries as usize * 2
                ..lut.num_output_table_entries as usize * 3]
                .to_vec(),
        );
        transform.output_clut_table_length = lut.num_output_table_entries;
        transform.transform_module_fn = Some(transform_module_clut);
        append_transform(Some(transform), next_transform);
        return first_transform;
    }
    modular_transform_release(first_transform);
    None
}

fn modular_transform_create_input(input: &Profile) -> Option<Box<ModularTransform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    if input.A2B0.is_some() {
        let lut_transform = modular_transform_create_lut(input.A2B0.as_deref().unwrap());
        if lut_transform.is_none() {
            return None;
        } else {
            append_transform(lut_transform, next_transform);
        }
    } else if input.mAB.is_some()
        && (*input.mAB.as_deref().unwrap()).num_in_channels == 3
        && (*input.mAB.as_deref().unwrap()).num_out_channels == 3
    {
        let mAB_transform = modular_transform_create_mAB(input.mAB.as_deref().unwrap());
        if mAB_transform.is_none() {
            return None;
        } else {
            append_transform(mAB_transform, next_transform);
        }
    } else {
        let mut transform = modular_transform_alloc();
        transform.input_clut_table[0] = build_input_gamma_table(input.redTRC.as_deref());
        transform.input_clut_table[1] = build_input_gamma_table(input.greenTRC.as_deref());
        transform.input_clut_table[2] = build_input_gamma_table(input.blueTRC.as_deref());
        transform.transform_module_fn = Some(transform_module_gamma_table);
        if transform.input_clut_table[0].is_none()
            || transform.input_clut_table[1].is_none()
            || transform.input_clut_table[2].is_none()
        {
            append_transform(Some(transform), next_transform);
            return None;
        } else {
            next_transform = append_transform(Some(transform), next_transform);
            transform = modular_transform_alloc();

            transform.matrix.m[0][0] = 1. / 1.999_969_5;
            transform.matrix.m[0][1] = 0.0;
            transform.matrix.m[0][2] = 0.0;
            transform.matrix.m[1][0] = 0.0;
            transform.matrix.m[1][1] = 1. / 1.999_969_5;
            transform.matrix.m[1][2] = 0.0;
            transform.matrix.m[2][0] = 0.0;
            transform.matrix.m[2][1] = 0.0;
            transform.matrix.m[2][2] = 1. / 1.999_969_5;
            transform.matrix.invalid = false;
            transform.transform_module_fn = Some(transform_module_matrix);
            next_transform = append_transform(Some(transform), next_transform);

            transform = modular_transform_alloc();
            transform.matrix = build_colorant_matrix(input);
            transform.transform_module_fn = Some(transform_module_matrix);
            append_transform(Some(transform), next_transform);
        }
    }
    first_transform
}
fn modular_transform_create_output(out: &Profile) -> Option<Box<ModularTransform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    if let Some(B2A0) = &out.B2A0 {
        let lut_transform = modular_transform_create_lut(B2A0);
        if lut_transform.is_none() {
            return None;
        } else {
            append_transform(lut_transform, next_transform);
        }
    } else if out.mBA.is_some()
        && (*out.mBA.as_deref().unwrap()).num_in_channels == 3
        && (*out.mBA.as_deref().unwrap()).num_out_channels == 3
    {
        let lut_transform_0 = modular_transform_create_mAB(out.mBA.as_deref().unwrap());
        if lut_transform_0.is_none() {
            return None;
        } else {
            append_transform(lut_transform_0, next_transform);
        }
    } else if let (Some(redTRC), Some(greenTRC), Some(blueTRC)) =
        (&out.redTRC, &out.greenTRC, &out.blueTRC)
    {
        let mut transform = modular_transform_alloc();
        transform.matrix = build_colorant_matrix(out).invert();
        transform.transform_module_fn = Some(transform_module_matrix);
        next_transform = append_transform(Some(transform), next_transform);

        transform = modular_transform_alloc();
        transform.matrix.m[0][0] = 1.999_969_5;
        transform.matrix.m[0][1] = 0.0;
        transform.matrix.m[0][2] = 0.0;
        transform.matrix.m[1][0] = 0.0;
        transform.matrix.m[1][1] = 1.999_969_5;
        transform.matrix.m[1][2] = 0.0;
        transform.matrix.m[2][0] = 0.0;
        transform.matrix.m[2][1] = 0.0;
        transform.matrix.m[2][2] = 1.999_969_5;
        transform.matrix.invalid = false;
        transform.transform_module_fn = Some(transform_module_matrix);
        next_transform = append_transform(Some(transform), next_transform);

        transform = modular_transform_alloc();
        transform.output_gamma_lut_r = Some(build_output_lut(redTRC));
        transform.output_gamma_lut_g = Some(build_output_lut(greenTRC));
        transform.output_gamma_lut_b = Some(build_output_lut(blueTRC));
        transform.transform_module_fn = Some(transform_module_gamma_lut);
        append_transform(Some(transform), next_transform);
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
fn modular_transform_create(input: &Profile, output: &Profile) -> Option<Box<ModularTransform>> {
    let mut first_transform = None;
    let mut next_transform = &mut first_transform;
    if input.color_space == RGB_SIGNATURE {
        let rgb_to_pcs = modular_transform_create_input(input);
        rgb_to_pcs.as_ref()?;
        next_transform = append_transform(rgb_to_pcs, next_transform);
    } else {
        debug_assert!(false, "input color space not supported");
        return None;
    }

    if input.pcs == LAB_SIGNATURE && output.pcs == XYZ_SIGNATURE {
        let mut lab_to_pcs = modular_transform_alloc();
        lab_to_pcs.transform_module_fn = Some(transform_module_LAB_to_XYZ);
        next_transform = append_transform(Some(lab_to_pcs), next_transform);
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

    if input.pcs == XYZ_SIGNATURE && output.pcs == LAB_SIGNATURE {
        let mut pcs_to_lab = modular_transform_alloc();
        pcs_to_lab.transform_module_fn = Some(transform_module_XYZ_to_LAB);
        next_transform = append_transform(Some(pcs_to_lab), next_transform);
    }

    if output.color_space == RGB_SIGNATURE {
        let pcs_to_rgb = modular_transform_create_output(output);
        pcs_to_rgb.as_ref()?;
        append_transform(pcs_to_rgb, next_transform);
    } else {
        debug_assert!(false, "output color space not supported");
    }

    // Not Completed
    //return qcms_modular_transform_reduce(first_transform);
    first_transform
}
fn modular_transform_data(
    mut transform: Option<&ModularTransform>,
    mut src: Vec<f32>,
    mut dest: Vec<f32>,
    _len: usize,
) -> Option<Vec<f32>> {
    while transform.is_some() {
        // Keep swaping src/dest when performing a transform to use less memory.
        let _transform_fn: TransformModuleFn = transform.unwrap().transform_module_fn;
        transform
            .unwrap()
            .transform_module_fn
            .expect("non-null function pointer")(
            transform.as_ref().unwrap(), &src, &mut dest
        );
        std::mem::swap(&mut src, &mut dest);
        transform = transform.unwrap().next_transform.as_deref();
    }
    // The results end up in the src buffer because of the switching
    Some(src)
}

pub fn chain_transform(
    input: &Profile,
    output: &Profile,
    src: Vec<f32>,
    dest: Vec<f32>,
    lutSize: usize,
) -> Option<Vec<f32>> {
    let transform_list = modular_transform_create(input, output);
    if transform_list.is_some() {
        let lut = modular_transform_data(transform_list.as_deref(), src, dest, lutSize / 3);
        modular_transform_release(transform_list);
        return lut;
    }
    None
}

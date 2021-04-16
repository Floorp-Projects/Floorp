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

trait ModularTransform {
    fn transform(&self, src: &[f32], dst: &mut [f32]);
}

#[inline]
fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a * (1.0 - t) + b * t
}

fn build_lut_matrix(lut: &lutType) -> Matrix {
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

struct LABtoXYZ;
impl ModularTransform for LABtoXYZ {
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
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
}

struct XYZtoLAB;
impl ModularTransform for XYZtoLAB {
    //Based on lcms cmsXYZ2Lab
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
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
}
#[derive(Default)]
struct ClutOnly {
    clut: Option<Vec<f32>>,
    grid_size: u16,
}
impl ModularTransform for ClutOnly {
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
        let xy_len: i32 = 1;
        let x_len: i32 = self.grid_size as i32;
        let len: i32 = x_len * x_len;

        let r_table = &self.clut.as_ref().unwrap()[0..];
        let g_table = &self.clut.as_ref().unwrap()[1..];
        let b_table = &self.clut.as_ref().unwrap()[2..];

        let CLU = |table: &[f32], x, y, z| table[((x * len + y * x_len + z * xy_len) * 3) as usize];

        for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
            debug_assert!(self.grid_size as i32 >= 1);
            let linear_r: f32 = src[0];
            let linear_g: f32 = src[1];
            let linear_b: f32 = src[2];
            let x: i32 = (linear_r * (self.grid_size as i32 - 1) as f32).floor() as i32;
            let y: i32 = (linear_g * (self.grid_size as i32 - 1) as f32).floor() as i32;
            let z: i32 = (linear_b * (self.grid_size as i32 - 1) as f32).floor() as i32;
            let x_n: i32 = (linear_r * (self.grid_size as i32 - 1) as f32).ceil() as i32;
            let y_n: i32 = (linear_g * (self.grid_size as i32 - 1) as f32).ceil() as i32;
            let z_n: i32 = (linear_b * (self.grid_size as i32 - 1) as f32).ceil() as i32;
            let x_d: f32 = linear_r * (self.grid_size as i32 - 1) as f32 - x as f32;
            let y_d: f32 = linear_g * (self.grid_size as i32 - 1) as f32 - y as f32;
            let z_d: f32 = linear_b * (self.grid_size as i32 - 1) as f32 - z as f32;

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
}
#[derive(Default)]
struct Clut {
    input_clut_table: [Option<Vec<f32>>; 3],
    clut: Option<Vec<f32>>,
    grid_size: u16,
    output_clut_table: [Option<Vec<f32>>; 3],
}
impl ModularTransform for Clut {
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
        let xy_len: i32 = 1;
        let x_len: i32 = self.grid_size as i32;
        let len: i32 = x_len * x_len;

        let r_table = &self.clut.as_ref().unwrap()[0..];
        let g_table = &self.clut.as_ref().unwrap()[1..];
        let b_table = &self.clut.as_ref().unwrap()[2..];
        let CLU = |table: &[f32], x, y, z| table[((x * len + y * x_len + z * xy_len) * 3) as usize];

        let input_clut_table_r = self.input_clut_table[0].as_ref().unwrap();
        let input_clut_table_g = self.input_clut_table[1].as_ref().unwrap();
        let input_clut_table_b = self.input_clut_table[2].as_ref().unwrap();
        for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
            debug_assert!(self.grid_size as i32 >= 1);
            let device_r: f32 = src[0];
            let device_g: f32 = src[1];
            let device_b: f32 = src[2];
            let linear_r: f32 = lut_interp_linear_float(device_r, &input_clut_table_r);
            let linear_g: f32 = lut_interp_linear_float(device_g, &input_clut_table_g);
            let linear_b: f32 = lut_interp_linear_float(device_b, &input_clut_table_b);
            let x: i32 = (linear_r * (self.grid_size as i32 - 1) as f32).floor() as i32;
            let y: i32 = (linear_g * (self.grid_size as i32 - 1) as f32).floor() as i32;
            let z: i32 = (linear_b * (self.grid_size as i32 - 1) as f32).floor() as i32;
            let x_n: i32 = (linear_r * (self.grid_size as i32 - 1) as f32).ceil() as i32;
            let y_n: i32 = (linear_g * (self.grid_size as i32 - 1) as f32).ceil() as i32;
            let z_n: i32 = (linear_b * (self.grid_size as i32 - 1) as f32).ceil() as i32;
            let x_d: f32 = linear_r * (self.grid_size as i32 - 1) as f32 - x as f32;
            let y_d: f32 = linear_g * (self.grid_size as i32 - 1) as f32 - y as f32;
            let z_d: f32 = linear_b * (self.grid_size as i32 - 1) as f32 - z as f32;

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
                lut_interp_linear_float(clut_r, &self.output_clut_table[0].as_ref().unwrap());
            let pcs_g: f32 =
                lut_interp_linear_float(clut_g, &self.output_clut_table[1].as_ref().unwrap());
            let pcs_b: f32 =
                lut_interp_linear_float(clut_b, &self.output_clut_table[2].as_ref().unwrap());
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
#[derive(Default)]
struct GammaTable {
    input_clut_table: [Option<Vec<f32>>; 3],
}
impl ModularTransform for GammaTable {
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
        let mut out_r: f32;
        let mut out_g: f32;
        let mut out_b: f32;
        let input_clut_table_r = self.input_clut_table[0].as_ref().unwrap();
        let input_clut_table_g = self.input_clut_table[1].as_ref().unwrap();
        let input_clut_table_b = self.input_clut_table[2].as_ref().unwrap();

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
}
#[derive(Default)]
struct GammaLut {
    output_gamma_lut_r: Option<Vec<u16>>,
    output_gamma_lut_g: Option<Vec<u16>>,
    output_gamma_lut_b: Option<Vec<u16>>,
}
impl ModularTransform for GammaLut {
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
        let mut out_r: f32;
        let mut out_g: f32;
        let mut out_b: f32;
        for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
            let in_r: f32 = src[0];
            let in_g: f32 = src[1];
            let in_b: f32 = src[2];
            out_r = lut_interp_linear(in_r as f64, &self.output_gamma_lut_r.as_ref().unwrap());
            out_g = lut_interp_linear(in_g as f64, &self.output_gamma_lut_g.as_ref().unwrap());
            out_b = lut_interp_linear(in_b as f64, &self.output_gamma_lut_b.as_ref().unwrap());
            dest[0] = clamp_float(out_r);
            dest[1] = clamp_float(out_g);
            dest[2] = clamp_float(out_b);
        }
    }
}
#[derive(Default)]
struct MatrixTranslate {
    matrix: Matrix,
    tx: f32,
    ty: f32,
    tz: f32,
}
impl ModularTransform for MatrixTranslate {
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
        let mut mat: Matrix = Matrix {
            m: [[0.; 3]; 3],
            invalid: false,
        };
        /* store the results in column major mode
         * this makes doing the multiplication with sse easier */
        mat.m[0][0] = self.matrix.m[0][0];
        mat.m[1][0] = self.matrix.m[0][1];
        mat.m[2][0] = self.matrix.m[0][2];
        mat.m[0][1] = self.matrix.m[1][0];
        mat.m[1][1] = self.matrix.m[1][1];
        mat.m[2][1] = self.matrix.m[1][2];
        mat.m[0][2] = self.matrix.m[2][0];
        mat.m[1][2] = self.matrix.m[2][1];
        mat.m[2][2] = self.matrix.m[2][2];
        for (dest, src) in dest.chunks_exact_mut(3).zip(src.chunks_exact(3)) {
            let in_r: f32 = src[0];
            let in_g: f32 = src[1];
            let in_b: f32 = src[2];
            let out_r: f32 = mat.m[0][0] * in_r + mat.m[1][0] * in_g + mat.m[2][0] * in_b + self.tx;
            let out_g: f32 = mat.m[0][1] * in_r + mat.m[1][1] * in_g + mat.m[2][1] * in_b + self.ty;
            let out_b: f32 = mat.m[0][2] * in_r + mat.m[1][2] * in_g + mat.m[2][2] * in_b + self.tz;
            dest[0] = clamp_float(out_r);
            dest[1] = clamp_float(out_g);
            dest[2] = clamp_float(out_b);
        }
    }
}
#[derive(Default)]
struct MatrixTransform {
    matrix: Matrix,
}
impl ModularTransform for MatrixTransform {
    fn transform(&self, src: &[f32], dest: &mut [f32]) {
        let mut mat: Matrix = Matrix {
            m: [[0.; 3]; 3],
            invalid: false,
        };
        /* store the results in column major mode
         * this makes doing the multiplication with sse easier */
        mat.m[0][0] = self.matrix.m[0][0];
        mat.m[1][0] = self.matrix.m[0][1];
        mat.m[2][0] = self.matrix.m[0][2];
        mat.m[0][1] = self.matrix.m[1][0];
        mat.m[1][1] = self.matrix.m[1][1];
        mat.m[2][1] = self.matrix.m[1][2];
        mat.m[0][2] = self.matrix.m[2][0];
        mat.m[1][2] = self.matrix.m[2][1];
        mat.m[2][2] = self.matrix.m[2][2];
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
}

fn modular_transform_create_mAB(lut: &lutmABType) -> Option<Vec<Box<dyn ModularTransform>>> {
    let mut transforms: Vec<Box<dyn ModularTransform>> = Vec::new();
    if lut.a_curves[0].is_some() {
        let clut_length: usize;
        // If the A curve is present this also implies the
        // presence of a CLUT.
        lut.clut_table.as_ref()?;

        // Prepare A curve.
        let mut transform = Box::new(GammaTable::default());
        transform.input_clut_table[0] = build_input_gamma_table(lut.a_curves[0].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[1] = build_input_gamma_table(lut.a_curves[1].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[2] = build_input_gamma_table(lut.a_curves[2].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());

        if lut.num_grid_points[0] as i32 != lut.num_grid_points[1] as i32
            || lut.num_grid_points[1] as i32 != lut.num_grid_points[2] as i32
        {
            //XXX: We don't currently support clut that are not squared!
            return None;
        }
        transforms.push(transform);

        // Prepare CLUT
        let mut transform = Box::new(ClutOnly::default());
        clut_length = (lut.num_grid_points[0] as usize).pow(3) * 3;
        assert_eq!(clut_length, lut.clut_table.as_ref().unwrap().len());
        transform.clut = lut.clut_table.clone();
        transform.grid_size = lut.num_grid_points[0] as u16;
        transforms.push(transform);
    }

    if lut.m_curves[0].is_some() {
        // M curve imples the presence of a Matrix

        // Prepare M curve
        let mut transform = Box::new(GammaTable::default());
        transform.input_clut_table[0] = build_input_gamma_table(lut.m_curves[0].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[1] = build_input_gamma_table(lut.m_curves[1].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[2] = build_input_gamma_table(lut.m_curves[2].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transforms.push(transform);

        // Prepare Matrix
        let mut transform = Box::new(MatrixTranslate::default());
        transform.matrix = build_mAB_matrix(lut);
        if transform.matrix.invalid {
            return None;
        }
        transform.tx = s15Fixed16Number_to_float(lut.e03);
        transform.ty = s15Fixed16Number_to_float(lut.e13);
        transform.tz = s15Fixed16Number_to_float(lut.e23);
        transforms.push(transform);
    }

    if lut.b_curves[0].is_some() {
        // Prepare B curve
        let mut transform = Box::new(GammaTable::default());
        transform.input_clut_table[0] = build_input_gamma_table(lut.b_curves[0].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[1] = build_input_gamma_table(lut.b_curves[1].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[2] = build_input_gamma_table(lut.b_curves[2].as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transforms.push(transform);
    } else {
        // B curve is mandatory
        return None;
    }

    if lut.reversed {
        // mBA are identical to mAB except that the transformation order
        // is reversed
        transforms.reverse();
    }
    Some(transforms)
}

fn modular_transform_create_lut(lut: &lutType) -> Option<Vec<Box<dyn ModularTransform>>> {
    let mut transforms: Vec<Box<dyn ModularTransform>> = Vec::new();

    let clut_length: usize;
    let mut transform = Box::new(MatrixTransform::default());

    transform.matrix = build_lut_matrix(lut);
    if !transform.matrix.invalid {
        transforms.push(transform);

        // Prepare input curves
        let mut transform = Box::new(Clut::default());
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
        transforms.push(transform);
        return Some(transforms);
    }
    None
}

fn modular_transform_create_input(input: &Profile) -> Option<Vec<Box<dyn ModularTransform>>> {
    let mut transforms = Vec::new();
    if input.A2B0.is_some() {
        let lut_transform = modular_transform_create_lut(input.A2B0.as_deref().unwrap());
        if let Some(lut_transform) = lut_transform {
            transforms.extend(lut_transform);
        } else {
            return None;
        }
    } else if input.mAB.is_some()
        && (*input.mAB.as_deref().unwrap()).num_in_channels == 3
        && (*input.mAB.as_deref().unwrap()).num_out_channels == 3
    {
        let mAB_transform = modular_transform_create_mAB(input.mAB.as_deref().unwrap());
        if let Some(mAB_transform) = mAB_transform {
            transforms.extend(mAB_transform);
        } else {
            return None;
        }
    } else {
        let mut transform = Box::new(GammaTable::default());
        transform.input_clut_table[0] =
            build_input_gamma_table(input.redTRC.as_deref()).map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[1] = build_input_gamma_table(input.greenTRC.as_deref())
            .map(|x| (x as Box<[f32]>).into_vec());
        transform.input_clut_table[2] =
            build_input_gamma_table(input.blueTRC.as_deref()).map(|x| (x as Box<[f32]>).into_vec());
        if transform.input_clut_table[0].is_none()
            || transform.input_clut_table[1].is_none()
            || transform.input_clut_table[2].is_none()
        {
            return None;
        } else {
            transforms.push(transform);

            let mut transform = Box::new(MatrixTransform::default());
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
            transforms.push(transform);

            let mut transform = Box::new(MatrixTransform::default());
            transform.matrix = build_colorant_matrix(input);
            transforms.push(transform);
        }
    }
    Some(transforms)
}
fn modular_transform_create_output(out: &Profile) -> Option<Vec<Box<dyn ModularTransform>>> {
    let mut transforms = Vec::new();
    if let Some(B2A0) = &out.B2A0 {
        let lut_transform = modular_transform_create_lut(B2A0);
        if let Some(lut_transform) = lut_transform {
            transforms.extend(lut_transform);
        } else {
            return None;
        }
    } else if out.mBA.is_some()
        && (*out.mBA.as_deref().unwrap()).num_in_channels == 3
        && (*out.mBA.as_deref().unwrap()).num_out_channels == 3
    {
        let lut_transform = modular_transform_create_mAB(out.mBA.as_deref().unwrap());
        if let Some(lut_transform) = lut_transform {
            transforms.extend(lut_transform)
        } else {
            return None;
        }
    } else if let (Some(redTRC), Some(greenTRC), Some(blueTRC)) =
        (&out.redTRC, &out.greenTRC, &out.blueTRC)
    {
        let mut transform = Box::new(MatrixTransform::default());
        transform.matrix = build_colorant_matrix(out).invert();
        transforms.push(transform);

        let mut transform = Box::new(MatrixTransform::default());
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
        transforms.push(transform);

        let mut transform = Box::new(GammaLut::default());
        transform.output_gamma_lut_r = Some(build_output_lut(redTRC));
        transform.output_gamma_lut_g = Some(build_output_lut(greenTRC));
        transform.output_gamma_lut_b = Some(build_output_lut(blueTRC));
        transforms.push(transform);
    } else {
        debug_assert!(false, "Unsupported output profile workflow.");
        return None;
    }
    Some(transforms)
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
    input: &Profile,
    output: &Profile,
) -> Option<Vec<Box<dyn ModularTransform>>> {
    let mut transforms = Vec::new();
    if input.color_space == RGB_SIGNATURE {
        let rgb_to_pcs = modular_transform_create_input(input);
        if let Some(rgb_to_pcs) = rgb_to_pcs {
            transforms.extend(rgb_to_pcs);
        } else {
            return None;
        }
    } else {
        debug_assert!(false, "input color space not supported");
        return None;
    }

    if input.pcs == LAB_SIGNATURE && output.pcs == XYZ_SIGNATURE {
        transforms.push(Box::new(LABtoXYZ {}));
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
        transforms.push(Box::new(XYZtoLAB {}));
    }

    if output.color_space == RGB_SIGNATURE {
        let pcs_to_rgb = modular_transform_create_output(output);
        if let Some(pcs_to_rgb) = pcs_to_rgb {
            transforms.extend(pcs_to_rgb);
        } else {
            return None;
        }
    } else {
        debug_assert!(false, "output color space not supported");
    }

    // Not Completed
    //return qcms_modular_transform_reduce(first_transform);
    Some(transforms)
}
fn modular_transform_data(
    transforms: Vec<Box<dyn ModularTransform>>,
    mut src: Vec<f32>,
    mut dest: Vec<f32>,
    _len: usize,
) -> Option<Vec<f32>> {
    for transform in transforms {
        // Keep swaping src/dest when performing a transform to use less memory.
        transform.transform(&src, &mut dest);
        std::mem::swap(&mut src, &mut dest);
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
    if let Some(transform_list) = transform_list {
        let lut = modular_transform_data(transform_list, src, dest, lutSize / 3);
        return lut;
    }
    None
}

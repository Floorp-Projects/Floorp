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

#[derive(Copy, Clone, Default)]
pub struct Matrix {
    pub m: [[f32; 3]; 3],
}

#[derive(Copy, Clone)]
pub struct Vector {
    pub v: [f32; 3],
}

impl Matrix {
    pub fn eval(&self, v: Vector) -> Vector {
        let mut result: Vector = Vector { v: [0.; 3] };
        result.v[0] = self.m[0][0] * v.v[0] + self.m[0][1] * v.v[1] + self.m[0][2] * v.v[2];
        result.v[1] = self.m[1][0] * v.v[0] + self.m[1][1] * v.v[1] + self.m[1][2] * v.v[2];
        result.v[2] = self.m[2][0] * v.v[0] + self.m[2][1] * v.v[1] + self.m[2][2] * v.v[2];
        result
    }

    //probably reuse this computation in matrix_invert
    pub fn det(&self) -> f32 {
        let det: f32 = self.m[0][0] * self.m[1][1] * self.m[2][2]
            + self.m[0][1] * self.m[1][2] * self.m[2][0]
            + self.m[0][2] * self.m[1][0] * self.m[2][1]
            - self.m[0][0] * self.m[1][2] * self.m[2][1]
            - self.m[0][1] * self.m[1][0] * self.m[2][2]
            - self.m[0][2] * self.m[1][1] * self.m[2][0];
        det
    }
    /* from pixman and cairo and Mathematics for Game Programmers */
    /* lcms uses gauss-jordan elimination with partial pivoting which is
     * less efficient and not as numerically stable. See Mathematics for
     * Game Programmers. */
    pub fn invert(&self) -> Option<Matrix> {
        let mut dest_mat: Matrix = Matrix {
            m: [[0.; 3]; 3],
        };
        let mut i: i32;

        const a: [i32; 3] = [2, 2, 1];
        const b: [i32; 3] = [1, 0, 0];
        /* inv  (A) = 1/det (A) * adj (A) */
        let mut det: f32 = self.det();
        if det == 0. {
            return None;
        }
        det = 1. / det;
        let mut j: i32 = 0;
        while j < 3 {
            i = 0;
            while i < 3 {
                let ai: i32 = a[i as usize];
                let aj: i32 = a[j as usize];
                let bi: i32 = b[i as usize];
                let bj: i32 = b[j as usize];
                let mut p: f64 = (self.m[ai as usize][aj as usize]
                    * self.m[bi as usize][bj as usize]
                    - self.m[ai as usize][bj as usize] * self.m[bi as usize][aj as usize])
                    as f64;
                if ((i + j) & 1) != 0 {
                    p = -p
                }
                dest_mat.m[j as usize][i as usize] = (det as f64 * p) as f32;
                i += 1
            }
            j += 1
        }
        Some(dest_mat)
    }
    pub fn identity() -> Matrix {
        let mut i: Matrix = Matrix {
            m: [[0.; 3]; 3],
        };
        i.m[0][0] = 1.;
        i.m[0][1] = 0.;
        i.m[0][2] = 0.;
        i.m[1][0] = 0.;
        i.m[1][1] = 1.;
        i.m[1][2] = 0.;
        i.m[2][0] = 0.;
        i.m[2][1] = 0.;
        i.m[2][2] = 1.;
        i
    }
    pub fn invalid() -> Option<Matrix> {
        return None;
    }
    /* from pixman */
    /* MAT3per... */
    pub fn multiply(a: Matrix, b: Matrix) -> Matrix {
        let mut result: Matrix = Matrix {
            m: [[0.; 3]; 3],
        };
        let mut dx: i32;

        let mut o: i32;
        let mut dy: i32 = 0;
        while dy < 3 {
            dx = 0;
            while dx < 3 {
                let mut v: f64 = 0f64;
                o = 0;
                while o < 3 {
                    v += (a.m[dy as usize][o as usize] * b.m[o as usize][dx as usize]) as f64;
                    o += 1
                }
                result.m[dy as usize][dx as usize] = v as f32;
                dx += 1
            }
            dy += 1
        }
        result
    }
}

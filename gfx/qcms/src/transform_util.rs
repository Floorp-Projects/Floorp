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

use ::libc;
use libc::{free, malloc};

use crate::matrix::matrix;
use crate::{
    iccread::{curveType, qcms_profile},
    s15Fixed16Number_to_float,
};

pub type int32_t = i32;

pub type uint8_t = libc::c_uchar;
pub type uint16_t = libc::c_ushort;

//XXX: could use a bettername
pub type uint16_fract_t = u16;

#[inline]
fn u8Fixed8Number_to_float(mut x: u16) -> f32 {
    // 0x0000 = 0.
    // 0x0100 = 1.
    // 0xffff = 255  + 255/256
    return (x as i32 as f64 / 256.0f64) as f32;
}
#[inline]
pub fn clamp_float(mut a: f32) -> f32 {
    /* One would naturally write this function as the following:
    if (a > 1.)
      return 1.;
    else if (a < 0)
      return 0;
    else
      return a;

    However, that version will let NaNs pass through which is undesirable
    for most consumers.
    */
    if a as f64 > 1.0f64 {
        return 1.;
    } else if a >= 0. {
        return a;
    } else {
        // a < 0 or a is NaN
        return 0.;
    };
}
/* value must be a value between 0 and 1 */
//XXX: is the above a good restriction to have?
// the output range of this functions is 0..1
#[no_mangle]
pub unsafe extern "C" fn lut_interp_linear(
    mut input_value: f64,
    mut table: *const u16,
    mut length: i32,
) -> f32 {
    input_value = input_value * (length - 1) as f64;

    let mut upper: i32 = input_value.ceil() as i32;
    let mut lower: i32 = input_value.floor() as i32;
    let mut value: f32 = (*table.offset(upper as isize) as i32 as f64
        * (1.0f64 - (upper as f64 - input_value))
        + *table.offset(lower as isize) as i32 as f64 * (upper as f64 - input_value))
        as f32;
    /* scale the value */
    return value * (1.0 / 65535.0);
}
/* same as above but takes and returns a uint16_t value representing a range from 0..1 */
#[no_mangle]
pub fn lut_interp_linear16(mut input_value: u16, mut table: &[u16]) -> u16 {
    /* Start scaling input_value to the length of the array: 65535*(length-1).
     * We'll divide out the 65535 next */
    let mut value: u32 = (input_value as i32 * (table.len() as i32 - 1)) as u32; /* equivalent to ceil(value/65535) */
    let mut upper: u32 = (value + 65534) / 65535; /* equivalent to floor(value/65535) */
    let mut lower: u32 = value / 65535;
    /* interp is the distance from upper to value scaled to 0..65535 */
    let mut interp: u32 = value % 65535; // 0..65535*65535
    value = (table[upper as usize] as u32 * interp
        + table[lower as usize] as u32 * (65535 - interp))
        / 65535;
    return value as u16;
}
/* same as above but takes an input_value from 0..PRECACHE_OUTPUT_MAX
 * and returns a uint8_t value representing a range from 0..1 */
unsafe extern "C" fn lut_interp_linear_precache_output(
    mut input_value: u32,
    mut table: *const u16,
    mut length: i32,
) -> u8 {
    /* Start scaling input_value to the length of the array: PRECACHE_OUTPUT_MAX*(length-1).
     * We'll divide out the PRECACHE_OUTPUT_MAX next */
    let mut value: u32 = input_value * (length - 1) as libc::c_uint;
    /* equivalent to ceil(value/PRECACHE_OUTPUT_MAX) */
    let mut upper: u32 = (value + (8192 - 1) as libc::c_uint - 1) / (8192 - 1) as libc::c_uint;
    /* equivalent to floor(value/PRECACHE_OUTPUT_MAX) */
    let mut lower: u32 = value / (8192 - 1) as libc::c_uint;
    /* interp is the distance from upper to value scaled to 0..PRECACHE_OUTPUT_MAX */
    let mut interp: u32 = value % (8192 - 1) as libc::c_uint;
    /* the table values range from 0..65535 */
    value = *table.offset(upper as isize) as libc::c_uint * interp
        + *table.offset(lower as isize) as libc::c_uint * ((8192 - 1) as libc::c_uint - interp); // 0..(65535*PRECACHE_OUTPUT_MAX)
                                                                                                 /* round and scale */
    value = value + ((8192 - 1) * 65535 / 255 / 2) as libc::c_uint; // scale to 0..255
    value = value / ((8192 - 1) * 65535 / 255) as libc::c_uint;
    return value as u8;
}
/* value must be a value between 0 and 1 */
//XXX: is the above a good restriction to have?
#[no_mangle]
pub unsafe extern "C" fn lut_interp_linear_float(
    mut value: f32,
    mut table: *const f32,
    mut length: i32,
) -> f32 {
    value = value * (length - 1) as f32;

    let mut upper: i32 = value.ceil() as i32;
    let mut lower: i32 = value.floor() as i32;
    //XXX: can we be more performant here?
    value = (*table.offset(upper as isize) as f64 * (1.0f64 - (upper as f32 - value) as f64)
        + (*table.offset(lower as isize) * (upper as f32 - value)) as f64) as f32;
    /* scale the value */
    return value;
}
#[no_mangle]
pub unsafe extern "C" fn compute_curve_gamma_table_type1(
    mut gamma_table: *mut f32,
    mut gamma: u16,
) {
    let mut gamma_float: f32 = u8Fixed8Number_to_float(gamma);
    let mut i: libc::c_uint = 0;
    while i < 256 {
        // 0..1^(0..255 + 255/256) will always be between 0 and 1
        *gamma_table.offset(i as isize) = (i as f64 / 255.0f64).powf(gamma_float as f64) as f32;
        i = i + 1
    }
}
#[no_mangle]
pub unsafe fn compute_curve_gamma_table_type2(mut gamma_table: *mut f32, mut table: &[u16]) {
    let mut i: libc::c_uint = 0;
    while i < 256 {
        *gamma_table.offset(i as isize) =
            lut_interp_linear(i as f64 / 255.0f64, table.as_ptr(), table.len() as i32);
        i = i + 1
    }
}
#[no_mangle]
pub unsafe fn compute_curve_gamma_table_type_parametric(
    mut gamma_table: *mut f32,
    mut params: &[f32],
) {
    let mut interval: f32;
    let mut a: f32;
    let mut b: f32;
    let mut c: f32;
    let mut e: f32;
    let mut f: f32;
    let mut y: f32 = params[0];
    // XXX: this could probably be cleaner with slice patterns
    if params.len() == 1 {
        a = 1.;
        b = 0.;
        c = 0.;
        e = 0.;
        f = 0.;
        interval = -1.
    } else if params.len() == 3 {
        a = params[1];
        b = params[2];
        c = 0.;
        e = 0.;
        f = 0.;
        interval = -1. * params[2] / params[1]
    } else if params.len() == 4 {
        a = params[1];
        b = params[2];
        c = 0.;
        e = params[3];
        f = params[3];
        interval = -1. * params[2] / params[1]
    } else if params.len() == 5 {
        a = params[1];
        b = params[2];
        c = params[3];
        e = -c;
        f = 0.;
        interval = params[4]
    } else if params.len() == 7 {
        a = params[1];
        b = params[2];
        c = params[3];
        e = params[5] - c;
        f = params[6];
        interval = params[4]
    } else {
        debug_assert!(false, "invalid parametric function type.");
        a = 1.;
        b = 0.;
        c = 0.;
        e = 0.;
        f = 0.;
        interval = -1.
    }
    let mut X: usize = 0;
    while X < 256 {
        if X as f32 >= interval {
            // XXX The equations are not exactly as defined in the spec but are
            //     algebraically equivalent.
            // TODO Should division by 255 be for the whole expression.
            *gamma_table.offset(X as isize) = clamp_float(
                (((a * X as f32) as f64 / 255.0f64 + b as f64).powf(y as f64) + c as f64 + e as f64)
                    as f32,
            )
        } else {
            *gamma_table.offset(X as isize) =
                clamp_float(((c * X as f32) as f64 / 255.0f64 + f as f64) as f32)
        }
        X = X + 1
    }
}
#[no_mangle]
pub unsafe extern "C" fn compute_curve_gamma_table_type0(mut gamma_table: *mut f32) {
    let mut i: libc::c_uint = 0;
    while i < 256 {
        *gamma_table.offset(i as isize) = (i as f64 / 255.0f64) as f32;
        i = i + 1
    }
}
#[no_mangle]
pub unsafe extern "C" fn build_input_gamma_table(mut TRC: Option<&curveType>) -> *mut f32 {
    let TRC = match TRC {
        Some(TRC) => TRC,
        None => return 0 as *mut f32,
    };
    let mut gamma_table: *mut f32 = malloc(::std::mem::size_of::<f32>() * 256) as *mut f32;
    if !gamma_table.is_null() {
        match TRC {
            curveType::Parametric(params) => {
                compute_curve_gamma_table_type_parametric(gamma_table, params)
            }
            curveType::Curve(data) => {
                if data.len() == 0 {
                    compute_curve_gamma_table_type0(gamma_table);
                } else if data.len() == 1 {
                    compute_curve_gamma_table_type1(gamma_table, data[0]);
                } else {
                    compute_curve_gamma_table_type2(gamma_table, data);
                }
            }
        }
    }
    return gamma_table;
}
#[no_mangle]
pub unsafe extern "C" fn build_colorant_matrix(mut p: &qcms_profile) -> matrix {
    let mut result: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    result.m[0][0] = s15Fixed16Number_to_float((*p).redColorant.X);
    result.m[0][1] = s15Fixed16Number_to_float((*p).greenColorant.X);
    result.m[0][2] = s15Fixed16Number_to_float((*p).blueColorant.X);
    result.m[1][0] = s15Fixed16Number_to_float((*p).redColorant.Y);
    result.m[1][1] = s15Fixed16Number_to_float((*p).greenColorant.Y);
    result.m[1][2] = s15Fixed16Number_to_float((*p).blueColorant.Y);
    result.m[2][0] = s15Fixed16Number_to_float((*p).redColorant.Z);
    result.m[2][1] = s15Fixed16Number_to_float((*p).greenColorant.Z);
    result.m[2][2] = s15Fixed16Number_to_float((*p).blueColorant.Z);
    result.invalid = false;
    return result;
}
/* The following code is copied nearly directly from lcms.
 * I think it could be much better. For example, Argyll seems to have better code in
 * icmTable_lookup_bwd and icmTable_setup_bwd. However, for now this is a quick way
 * to a working solution and allows for easy comparing with lcms. */
#[no_mangle]
pub fn lut_inverse_interp16(mut Value: u16, mut LutTable: &[u16]) -> uint16_fract_t {
    let mut l: i32 = 1; // 'int' Give spacing for negative values
    let mut r: i32 = 0x10000;
    let mut x: i32 = 0;
    let mut res: i32;
    let length = LutTable.len() as i32;

    let mut NumZeroes: i32 = 0;
    while LutTable[NumZeroes as usize] as i32 == 0 && NumZeroes < length - 1 {
        NumZeroes += 1
    }
    // There are no zeros at the beginning and we are trying to find a zero, so
    // return anything. It seems zero would be the less destructive choice
    /* I'm not sure that this makes sense, but oh well... */
    if NumZeroes == 0 && Value as i32 == 0 {
        return 0u16;
    }
    let mut NumPoles: i32 = 0;
    while LutTable[(length - 1 - NumPoles) as usize] as i32 == 0xffff && NumPoles < length - 1 {
        NumPoles += 1
    }
    // Does the curve belong to this case?
    if NumZeroes > 1 || NumPoles > 1 {
        let mut a_0: i32;
        let mut b_0: i32;
        // Identify if value fall downto 0 or FFFF zone
        if Value as i32 == 0 {
            return 0u16;
        }
        // if (Value == 0xFFFF) return 0xFFFF;
        // else restrict to valid zone
        if NumZeroes > 1 {
            a_0 = (NumZeroes - 1) * 0xffff / (length - 1);
            l = a_0 - 1
        }
        if NumPoles > 1 {
            b_0 = (length - 1 - NumPoles) * 0xffff / (length - 1);
            r = b_0 + 1
        }
    }
    if r <= l {
        // If this happens LutTable is not invertible
        return 0u16;
    }
    // Seems not a degenerated case... apply binary search
    while r > l {
        x = (l + r) / 2;
        res = lut_interp_linear16((x - 1) as uint16_fract_t, LutTable) as i32;
        if res == Value as i32 {
            // Found exact match.
            return (x - 1) as uint16_fract_t;
        }
        if res > Value as i32 {
            r = x - 1
        } else {
            l = x + 1
        }
    }

    // Not found, should we interpolate?

    // Get surrounding nodes
    debug_assert!(x >= 1);

    let mut val2: f64 = (length - 1) as f64 * ((x - 1) as f64 / 65535.0f64);
    let mut cell0: i32 = val2.floor() as i32;
    let mut cell1: i32 = val2.ceil() as i32;
    if cell0 == cell1 {
        return x as uint16_fract_t;
    }

    let mut y0: f64 = LutTable[cell0 as usize] as f64;
    let mut x0: f64 = 65535.0f64 * cell0 as f64 / (length - 1) as f64;
    let mut y1: f64 = LutTable[cell1 as usize] as f64;
    let mut x1: f64 = 65535.0f64 * cell1 as f64 / (length - 1) as f64;
    let mut a: f64 = (y1 - y0) / (x1 - x0);
    let mut b: f64 = y0 - a * x0;
    if a.abs() < 0.01f64 {
        return x as uint16_fract_t;
    }
    let mut f: f64 = (Value as i32 as f64 - b) / a;
    if f < 0.0f64 {
        return 0u16;
    }
    if f >= 65535.0f64 {
        return 0xffffu16;
    }
    return (f + 0.5f64).floor() as uint16_fract_t;
}
/*
The number of entries needed to invert a lookup table should not
necessarily be the same as the original number of entries.  This is
especially true of lookup tables that have a small number of entries.

For example:
Using a table like:
   {0, 3104, 14263, 34802, 65535}
invert_lut will produce an inverse of:
   {3, 34459, 47529, 56801, 65535}
which has an maximum error of about 9855 (pixel difference of ~38.346)

For now, we punt the decision of output size to the caller. */
unsafe fn invert_lut(mut table: &[u16], mut out_length: i32) -> *mut u16 {
    /* for now we invert the lut by creating a lut of size out_length
     * and attempting to lookup a value for each entry using lut_inverse_interp16 */
    let mut output: *mut u16 =
        malloc(::std::mem::size_of::<u16>() * out_length as usize) as *mut u16;
    if output.is_null() {
        return 0 as *mut u16;
    }
    let mut i: i32 = 0;
    while i < out_length {
        let mut x: f64 = i as f64 * 65535.0f64 / (out_length - 1) as f64;
        let mut input: uint16_fract_t = (x + 0.5f64).floor() as uint16_fract_t;
        *output.offset(i as isize) = lut_inverse_interp16(input, table);
        i += 1
    }
    return output;
}
unsafe extern "C" fn compute_precache_pow(mut output: *mut u8, mut gamma: f32) {
    let mut v: u32 = 0;
    while v < 8192 {
        //XXX: don't do integer/float conversion... and round?
        *output.offset(v as isize) =
            (255.0f64 * (v as f64 / (8192 - 1) as f64).powf(gamma as f64)) as u8;
        v = v + 1
    }
}
#[no_mangle]
pub unsafe extern "C" fn compute_precache_lut(
    mut output: *mut u8,
    mut table: *mut u16,
    mut length: i32,
) {
    let mut v: u32 = 0;
    while v < 8192 {
        *output.offset(v as isize) = lut_interp_linear_precache_output(v, table, length);
        v = v + 1
    }
}
#[no_mangle]
pub unsafe extern "C" fn compute_precache_linear(mut output: *mut u8) {
    let mut v: u32 = 0;
    while v < 8192 {
        //XXX: round?
        *output.offset(v as isize) = (v / (8192 / 256) as libc::c_uint) as u8;
        v = v + 1
    }
}
#[no_mangle]
pub unsafe extern "C" fn compute_precache(mut trc: &curveType, mut output: *mut u8) -> bool {
    match trc {
        curveType::Parametric(params) => {
            let mut gamma_table: [f32; 256] = [0.; 256];
            let mut gamma_table_uint: [u16; 256] = [0; 256];

            let mut inverted_size: i32 = 256;
            compute_curve_gamma_table_type_parametric(gamma_table.as_mut_ptr(), params);
            let mut i: u16 = 0u16;
            while (i as i32) < 256 {
                gamma_table_uint[i as usize] = (gamma_table[i as usize] * 65535f32) as u16;
                i = i + 1
            }
            //XXX: the choice of a minimum of 256 here is not backed by any theory,
            //     measurement or data, howeve r it is what lcms uses.
            //     the maximum number we would need is 65535 because that's the
            //     accuracy used for computing the pre cache table
            if inverted_size < 256 {
                inverted_size = 256
            }
            let mut inverted: *mut u16 = invert_lut(&gamma_table_uint, inverted_size);
            if inverted.is_null() {
                return false;
            }
            compute_precache_lut(output, inverted, inverted_size);
            free(inverted as *mut libc::c_void);
        }
        curveType::Curve(data) => {
            if data.len() == 0 {
                compute_precache_linear(output);
            } else if data.len() == 1 {
                compute_precache_pow(
                    output,
                    (1.0f64 / u8Fixed8Number_to_float(data[0]) as f64) as f32,
                );
            } else {
                let mut inverted_size_0: i32 = data.len() as i32;
                //XXX: the choice of a minimum of 256 here is not backed by any theory,
                //     measurement or data, howeve r it is what lcms uses.
                //     the maximum number we would need is 65535 because that's the
                //     accuracy used for computing the pre cache table
                if inverted_size_0 < 256 {
                    inverted_size_0 = 256
                } //XXX turn this conversion into a function
                let mut inverted_0: *mut u16 = invert_lut(data, inverted_size_0);
                if inverted_0.is_null() {
                    return false;
                }
                compute_precache_lut(output, inverted_0, inverted_size_0);
                free(inverted_0 as *mut libc::c_void);
            }
        }
    }
    return true;
}
unsafe extern "C" fn build_linear_table(mut length: i32) -> *mut u16 {
    let mut output: *mut u16 = malloc(::std::mem::size_of::<u16>() * length as usize) as *mut u16;
    if output.is_null() {
        return 0 as *mut u16;
    }
    let mut i: i32 = 0;
    while i < length {
        let mut x: f64 = i as f64 * 65535.0f64 / (length - 1) as f64;
        let mut input: uint16_fract_t = (x + 0.5f64).floor() as uint16_fract_t;
        *output.offset(i as isize) = input;
        i += 1
    }
    return output;
}
unsafe extern "C" fn build_pow_table(mut gamma: f32, mut length: i32) -> *mut u16 {
    let mut output: *mut u16 = malloc(::std::mem::size_of::<u16>() * length as usize) as *mut u16;
    if output.is_null() {
        return 0 as *mut u16;
    }
    let mut i: i32 = 0;
    while i < length {
        let mut x: f64 = i as f64 / (length - 1) as f64;
        x = x.powf(gamma as f64);
        let mut result: uint16_fract_t = (x * 65535.0f64 + 0.5f64).floor() as uint16_fract_t;
        *output.offset(i as isize) = result;
        i += 1
    }
    return output;
}

#[no_mangle]
pub unsafe extern "C" fn build_output_lut(
    mut trc: &curveType,
    mut output_gamma_lut: *mut *mut u16,
    mut output_gamma_lut_length: *mut usize,
) {
    match trc {
        curveType::Parametric(params) => {
            let mut gamma_table: [f32; 256] = [0.; 256];

            let mut output: *mut u16 = malloc(::std::mem::size_of::<u16>() * 256) as *mut u16;
            if output.is_null() {
                *output_gamma_lut = 0 as *mut u16;
                return;
            }
            compute_curve_gamma_table_type_parametric(gamma_table.as_mut_ptr(), params);
            *output_gamma_lut_length = 256;
            let mut i: u16 = 0u16;
            while (i as i32) < 256 {
                *output.offset(i as isize) = (gamma_table[i as usize] * 65535f32) as u16;
                i = i + 1
            }
            *output_gamma_lut = output
        }
        curveType::Curve(data) => {
            if data.len() == 0 {
                *output_gamma_lut = build_linear_table(4096);
                *output_gamma_lut_length = 4096
            } else if data.len() == 1 {
                let mut gamma: f32 = (1.0f64 / u8Fixed8Number_to_float(data[0]) as f64) as f32;
                *output_gamma_lut = build_pow_table(gamma, 4096);
                *output_gamma_lut_length = 4096
            } else {
                //XXX: the choice of a minimum of 256 here is not backed by any theory,
                //     measurement or data, however it is what lcms uses.
                *output_gamma_lut_length = data.len();
                if *output_gamma_lut_length < 256 {
                    *output_gamma_lut_length = 256
                }
                *output_gamma_lut = invert_lut(data, *output_gamma_lut_length as i32)
            }
        }
    }
}

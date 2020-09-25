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

use std::{
    ptr::null_mut,
    slice,
    sync::atomic::{AtomicBool, Ordering},
};

use ::libc;
use libc::{fclose, fopen, fread, free, malloc, memset, FILE};

use crate::{
    double_to_s15Fixed16Number,
    transform::{get_rgb_colorants, precache_output, precache_release, set_rgb_colorants},
};
use crate::{
    matrix::matrix, qcms_intent, s15Fixed16Number, s15Fixed16Number_to_float,
    QCMS_INTENT_PERCEPTUAL,
};

pub static qcms_supports_iccv4: AtomicBool = AtomicBool::new(false);

pub type icColorSpaceSignature = libc::c_uint;
pub const icMaxEnumData: icColorSpaceSignature = 4294967295;
pub const icSig15colorData: icColorSpaceSignature = 1178815570;
pub const icSig14colorData: icColorSpaceSignature = 1162038354;
pub const icSig13colorData: icColorSpaceSignature = 1145261138;
pub const icSig12colorData: icColorSpaceSignature = 1128483922;
pub const icSig11colorData: icColorSpaceSignature = 1111706706;
pub const icSig10colorData: icColorSpaceSignature = 1094929490;
pub const icSig9colorData: icColorSpaceSignature = 960711762;
pub const icSig8colorData: icColorSpaceSignature = 943934546;
pub const icSig7colorData: icColorSpaceSignature = 927157330;
pub const icSig6colorData: icColorSpaceSignature = 910380114;
pub const icSig5colorData: icColorSpaceSignature = 893602898;
pub const icSig4colorData: icColorSpaceSignature = 876825682;
pub const icSig3colorData: icColorSpaceSignature = 860048466;
pub const icSig2colorData: icColorSpaceSignature = 843271250;
pub const icSigCmyData: icColorSpaceSignature = 1129142560;
pub const icSigCmykData: icColorSpaceSignature = 1129142603;
pub const icSigHlsData: icColorSpaceSignature = 1212961568;
pub const icSigHsvData: icColorSpaceSignature = 1213421088;
pub const icSigGrayData: icColorSpaceSignature = 1196573017;
pub const icSigRgbData: icColorSpaceSignature = 1380401696;
pub const icSigYxyData: icColorSpaceSignature = 1501067552;
pub const icSigYCbCrData: icColorSpaceSignature = 1497588338;
pub const icSigLuvData: icColorSpaceSignature = 1282766368;
pub const icSigLabData: icColorSpaceSignature = 1281450528;
pub const icSigXYZData: icColorSpaceSignature = 1482250784;

pub const RGB_SIGNATURE: u32 = 0x52474220;
pub const GRAY_SIGNATURE: u32 = 0x47524159;
pub const XYZ_SIGNATURE: u32 = 0x58595A20;
pub const LAB_SIGNATURE: u32 = 0x4C616220;

#[repr(C)]
#[derive(Clone)]
pub struct qcms_profile {
    pub class_type: u32,
    pub color_space: u32,
    pub pcs: u32,
    pub rendering_intent: qcms_intent,
    pub redColorant: XYZNumber,
    pub blueColorant: XYZNumber,
    pub greenColorant: XYZNumber,
    pub redTRC: *mut curveType,
    pub blueTRC: *mut curveType,
    pub greenTRC: *mut curveType,
    pub grayTRC: *mut curveType,
    pub A2B0: *mut lutType,
    pub B2A0: *mut lutType,
    pub mAB: *mut lutmABType,
    pub mBA: *mut lutmABType,
    pub chromaticAdaption: matrix,
    pub output_table_r: *mut precache_output,
    pub output_table_g: *mut precache_output,
    pub output_table_b: *mut precache_output,
}

impl Drop for qcms_profile {
    fn drop(&mut self) {
        unsafe {
            if !self.output_table_r.is_null() {
                precache_release(self.output_table_r);
            }
            if !self.output_table_g.is_null() {
                precache_release(self.output_table_g);
            }
            if !self.output_table_b.is_null() {
                precache_release(self.output_table_b);
            }
            if !self.A2B0.is_null() {
                lut_release(self.A2B0);
            }
            if !self.B2A0.is_null() {
                lut_release(self.B2A0);
            }
            if !self.mAB.is_null() {
                mAB_release(self.mAB);
            }
            if !self.mBA.is_null() {
                mAB_release(self.mBA);
            }
            free(self.redTRC as *mut libc::c_void);
            free(self.blueTRC as *mut libc::c_void);
            free(self.greenTRC as *mut libc::c_void);
            free(self.grayTRC as *mut libc::c_void);
        }
    }
}

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
pub struct curveType {
    pub type_0: u32,
    pub count: u32,
    pub parameter: [f32; 7],
    pub data: [uInt16Number; 0],
}
pub type uInt16Number = u16;

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

/* the names for the following two types are sort of ugly */
#[repr(C)]
#[derive(Copy, Clone)]
pub struct qcms_CIE_xyY {
    pub x: f64,
    pub y: f64,
    pub Y: f64,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct qcms_CIE_xyYTRIPLE {
    pub red: qcms_CIE_xyY,
    pub green: qcms_CIE_xyY,
    pub blue: qcms_CIE_xyY,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct tag {
    pub signature: u32,
    pub offset: u32,
    pub size: u32,
}

/* It might be worth having a unified limit on content controlled
 * allocation per profile. This would remove the need for many
 * of the arbitrary limits that we used */
pub type be32 = u32;
pub type be16 = u16;

type tag_index = [tag];

/* a wrapper around the memory that we are going to parse
 * into a qcms_profile */

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mem_source<'a> {
    pub buf: &'a [u8],
    pub valid: bool,
    pub invalid_reason: Option<&'static str>,
}
pub type uInt8Number = u8;
#[inline]
fn uInt8Number_to_float(mut a: uInt8Number) -> f32 {
    return a as i32 as f32 / 255.0;
}

#[inline]
fn uInt16Number_to_float(mut a: uInt16Number) -> f32 {
    return a as i32 as f32 / 65535.0;
}

fn cpu_to_be32(mut v: u32) -> be32 {
    v.to_be()
}
fn cpu_to_be16(mut v: u16) -> be16 {
    v.to_be()
}
fn be32_to_cpu(mut v: be32) -> u32 {
    u32::from_be(v)
}
fn be16_to_cpu(mut v: be16) -> u16 {
    u16::from_be(v)
}
fn invalid_source(mut mem: &mut mem_source, reason: &'static str) {
    (*mem).valid = false;
    (*mem).invalid_reason = Some(reason);
}
fn read_u32(mut mem: &mut mem_source, mut offset: usize) -> u32 {
    /* Subtract from mem->size instead of the more intuitive adding to offset.
     * This avoids overflowing offset. The subtraction is safe because
     * mem->size is guaranteed to be > 4 */
    if offset > (*mem).buf.len() - 4 {
        invalid_source(mem, "Invalid offset");
        return 0;
    } else {
        let k = unsafe {
            std::ptr::read_unaligned((*mem).buf.as_ptr().offset(offset as isize) as *const be32)
        };
        return be32_to_cpu(k);
    };
}
fn read_u16(mut mem: &mut mem_source, mut offset: usize) -> u16 {
    if offset > (*mem).buf.len() - 2 {
        invalid_source(mem, "Invalid offset");
        return 0u16;
    } else {
        let k = unsafe {
            std::ptr::read_unaligned((*mem).buf.as_ptr().offset(offset as isize) as *const be16)
        };
        return be16_to_cpu(k);
    };
}
fn read_u8(mut mem: &mut mem_source, mut offset: usize) -> u8 {
    if offset > (*mem).buf.len() - 1 {
        invalid_source(mem, "Invalid offset");
        return 0u8;
    } else {
        return unsafe { *((*mem).buf.as_ptr().offset(offset as isize) as *mut u8) };
    };
}
fn read_s15Fixed16Number(mut mem: &mut mem_source, mut offset: usize) -> s15Fixed16Number {
    return read_u32(mem, offset) as s15Fixed16Number;
}
fn read_uInt8Number(mut mem: &mut mem_source, mut offset: usize) -> uInt8Number {
    return read_u8(mem, offset);
}
fn read_uInt16Number(mut mem: &mut mem_source, mut offset: usize) -> uInt16Number {
    return read_u16(mem, offset);
}
unsafe extern "C" fn write_u32(mut mem: *mut libc::c_void, mut offset: usize, mut value: u32) {
    *((mem as *mut libc::c_uchar).offset(offset as isize) as *mut u32) = cpu_to_be32(value);
}
unsafe extern "C" fn write_u16(mut mem: *mut libc::c_void, mut offset: usize, mut value: u16) {
    *((mem as *mut libc::c_uchar).offset(offset as isize) as *mut u16) = cpu_to_be16(value);
}

/* An arbitrary 4MB limit on profile size */
const MAX_PROFILE_SIZE: usize = 1024 * 1024 * 4;
const MAX_TAG_COUNT: u32 = 1024;

fn check_CMM_type_signature(mut src: &mut mem_source) {
    //uint32_t CMM_type_signature = read_u32(src, 4);
    //TODO: do the check?
}
fn check_profile_version(mut src: &mut mem_source) {
    /*
    uint8_t major_revision = read_u8(src, 8 + 0);
    uint8_t minor_revision = read_u8(src, 8 + 1);
    */
    let mut reserved1: u8 = read_u8(src, (8 + 2) as usize);
    let mut reserved2: u8 = read_u8(src, (8 + 3) as usize);
    /* Checking the version doesn't buy us anything
    if (major_revision != 0x4) {
        if (major_revision > 0x2)
            invalid_source(src, "Unsupported major revision");
        if (minor_revision > 0x40)
            invalid_source(src, "Unsupported minor revision");
    }
    */
    if reserved1 as i32 != 0 || reserved2 as i32 != 0 {
        invalid_source(src, "Invalid reserved bytes");
    };
}

const INPUT_DEVICE_PROFILE: u32 = 0x73636e72; // 'scnr'
const DISPLAY_DEVICE_PROFILE: u32 = 0x6d6e7472; // 'mntr'
const OUTPUT_DEVICE_PROFILE: u32 = 0x70727472; // 'prtr'
const DEVICE_LINK_PROFILE: u32 = 0x6c696e6b; // 'link'
const COLOR_SPACE_PROFILE: u32 = 0x73706163; // 'spac'
const ABSTRACT_PROFILE: u32 = 0x61627374; // 'abst'
const NAMED_COLOR_PROFILE: u32 = 0x6e6d636c; // 'nmcl'

fn read_class_signature(mut profile: &mut qcms_profile, mut mem: &mut mem_source) {
    (*profile).class_type = read_u32(mem, 12);
    match (*profile).class_type {
        DISPLAY_DEVICE_PROFILE
        | INPUT_DEVICE_PROFILE
        | OUTPUT_DEVICE_PROFILE
        | COLOR_SPACE_PROFILE => {}
        _ => {
            invalid_source(mem, "Invalid  Profile/Device Class signature");
        }
    };
}
fn read_color_space(mut profile: &mut qcms_profile, mut mem: &mut mem_source) {
    (*profile).color_space = read_u32(mem, 16);
    match (*profile).color_space {
        RGB_SIGNATURE | GRAY_SIGNATURE => {}
        _ => {
            invalid_source(mem, "Unsupported colorspace");
        }
    };
}
fn read_pcs(mut profile: &mut qcms_profile, mut mem: &mut mem_source) {
    (*profile).pcs = read_u32(mem, 20);
    match (*profile).pcs {
        XYZ_SIGNATURE | LAB_SIGNATURE => {}
        _ => {
            invalid_source(mem, "Unsupported pcs");
        }
    };
}
fn read_tag_table(mut profile: &mut qcms_profile, mut mem: &mut mem_source) -> Vec<tag> {
    let count = read_u32(mem, 128);
    if count > MAX_TAG_COUNT {
        invalid_source(mem, "max number of tags exceeded");
        return Vec::new();
    }
    let mut index = Vec::with_capacity(count as usize);
    for i in 0..count {
        index.push(tag {
            signature: read_u32(mem, (128 + 4 + 4 * i * 3) as usize),
            offset: read_u32(mem, (128 + 4 + 4 * i * 3 + 4) as usize),
            size: read_u32(mem, (128 + 4 + 4 * i * 3 + 8) as usize),
        });
    }

    index
}
/* if we've already got an ICC_H header we can ignore the following */
/* icc34 defines */
/* ****************************************************************
 Copyright (c) 1994-1996 SunSoft, Inc.

                    Rights Reserved

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restrict-
ion, including without limitation the rights to use, copy, modify,
merge, publish distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished
to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-
INFRINGEMENT.  IN NO EVENT SHALL SUNSOFT, INC. OR ITS PARENT
COMPANY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of SunSoft, Inc.
shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without written
authorization from SunSoft Inc.
******************************************************************/
/*
 * QCMS, in general, is not threadsafe. However, it should be safe to create
 * profile and transformation objects on different threads, so long as you
 * don't use the same objects on different threads at the same time.
 */
/*
 * Color Space Signatures
 * Note that only icSigXYZData and icSigLabData are valid
 * Profile Connection Spaces (PCSs)
 */
/* 'XYZ ' */
/* 'Lab ' */
/* 'Luv ' */
/* 'YCbr' */
/* 'Yxy ' */
/* 'RGB ' */
/* 'GRAY' */
/* 'HSV ' */
/* 'HLS ' */
/* 'CMYK' */
/* 'CMY ' */
/* '2CLR' */
/* '3CLR' */
/* '4CLR' */
/* '5CLR' */
/* '6CLR' */
/* '7CLR' */
/* '8CLR' */
/* '9CLR' */
/* 'ACLR' */
/* 'BCLR' */
/* 'CCLR' */
/* 'DCLR' */
/* 'ECLR' */
/* 'FCLR' */
/* these values match the Rendering Intent values from the ICC spec */
/* Chris Murphy (CM consultant) suggests this as a default in the event that we
 * cannot reproduce relative + Black Point Compensation.  BPC brings an
 * unacceptable performance overhead, so we go with perceptual. */
//XXX: I don't really like the _DATA_ prefix

// Checks a profile for obvious inconsistencies and returns
// true if the profile looks bogus and should probably be
// ignored.
#[no_mangle]
pub extern "C" fn qcms_profile_is_bogus(mut profile: &mut qcms_profile) -> bool {
    let mut sum: [f32; 3] = [0.; 3];
    let mut target: [f32; 3] = [0.; 3];
    let mut tolerance: [f32; 3] = [0.; 3];
    let mut rX: f32;
    let mut rY: f32;
    let mut rZ: f32;
    let mut gX: f32;
    let mut gY: f32;
    let mut gZ: f32;
    let mut bX: f32;
    let mut bY: f32;
    let mut bZ: f32;
    let mut negative: bool;
    let mut i: libc::c_uint;
    // We currently only check the bogosity of RGB profiles
    if (*profile).color_space != RGB_SIGNATURE {
        return false;
    }
    if !(*profile).A2B0.is_null()
        || !(*profile).B2A0.is_null()
        || !(*profile).mAB.is_null()
        || !(*profile).mBA.is_null()
    {
        return false;
    }
    rX = s15Fixed16Number_to_float((*profile).redColorant.X);
    rY = s15Fixed16Number_to_float((*profile).redColorant.Y);
    rZ = s15Fixed16Number_to_float((*profile).redColorant.Z);
    gX = s15Fixed16Number_to_float((*profile).greenColorant.X);
    gY = s15Fixed16Number_to_float((*profile).greenColorant.Y);
    gZ = s15Fixed16Number_to_float((*profile).greenColorant.Z);
    bX = s15Fixed16Number_to_float((*profile).blueColorant.X);
    bY = s15Fixed16Number_to_float((*profile).blueColorant.Y);
    bZ = s15Fixed16Number_to_float((*profile).blueColorant.Z);
    // Sum the values; they should add up to something close to white
    sum[0] = rX + gX + bX;
    sum[1] = rY + gY + bY;
    sum[2] = rZ + gZ + bZ;
    // Build our target vector (see mozilla bug 460629)
    target[0] = 0.96420;
    target[1] = 1.00000;
    target[2] = 0.82491;
    // Our tolerance vector - Recommended by Chris Murphy based on
    // conversion from the LAB space criterion of no more than 3 in any one
    // channel. This is similar to, but slightly more tolerant than Adobe's
    // criterion.
    tolerance[0] = 0.02;
    tolerance[1] = 0.02;
    tolerance[2] = 0.04;
    // Compare with our tolerance
    i = 0;
    while i < 3 {
        if !(sum[i as usize] - tolerance[i as usize] <= target[i as usize]
            && sum[i as usize] + tolerance[i as usize] >= target[i as usize])
        {
            return true;
        }
        i = i + 1
    }
    if !cfg!(target_os = "macos") {
        negative = (rX < 0.)
            || (rY < 0.)
            || (rZ < 0.)
            || (gX < 0.)
            || (gY < 0.)
            || (gZ < 0.)
            || (bX < 0.)
            || (bY < 0.)
            || (bZ < 0.);
    } else {
        // Chromatic adaption to D50 can result in negative XYZ, but the white
        // point D50 tolerance test has passed. Accept negative values herein.
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=498245#c18 onwards
        // for discussion about whether profile XYZ can or cannot be negative,
        // per the spec. Also the https://bugzil.la/450923 user report.

        // FIXME: allow this relaxation on all ports?
        negative = false; // bogus
    }
    if negative {
        return true;
    }
    // All Good
    return false;
}

const TAG_bXYZ: u32 = 0x6258595a;
const TAG_gXYZ: u32 = 0x6758595a;
const TAG_rXYZ: u32 = 0x7258595a;
const TAG_rTRC: u32 = 0x72545243;
const TAG_bTRC: u32 = 0x62545243;
const TAG_gTRC: u32 = 0x67545243;
const TAG_kTRC: u32 = 0x6b545243;
const TAG_A2B0: u32 = 0x41324230;
const TAG_B2A0: u32 = 0x42324130;
const TAG_CHAD: u32 = 0x63686164;

fn find_tag(mut index: &tag_index, mut tag_id: u32) -> Option<&tag> {
    for t in index {
        if t.signature == tag_id {
            return Some(t);
        }
    }
    None
}

pub const XYZ_TYPE: u32 = 0x58595a20; // 'XYZ '
pub const CURVE_TYPE: u32 = 0x63757276; // 'curv'
pub const PARAMETRIC_CURVE_TYPE: u32 = 0x70617261; // 'para'
pub const LUT16_TYPE: u32 = 0x6d667432; // 'mft2'
pub const LUT8_TYPE: u32 = 0x6d667431; // 'mft1'
pub const LUT_MAB_TYPE: u32 = 0x6d414220; // 'mAB '
pub const LUT_MBA_TYPE: u32 = 0x6d424120; // 'mBA '
pub const CHROMATIC_TYPE: u32 = 0x73663332; // 'sf32'

fn read_tag_s15Fixed16ArrayType(
    mut src: &mut mem_source,
    mut index: &tag_index,
    mut tag_id: u32,
) -> matrix {
    let mut tag = find_tag(index, tag_id);
    let mut matrix: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    if let Some(tag) = tag {
        let mut i: u8;
        let mut offset: u32 = (*tag).offset;
        let mut type_0: u32 = read_u32(src, offset as usize);
        // Check mandatory type signature for s16Fixed16ArrayType
        if type_0 != CHROMATIC_TYPE {
            invalid_source(src, "unexpected type, expected \'sf32\'");
        }
        i = 0u8;
        while (i as i32) < 9 {
            matrix.m[(i as i32 / 3) as usize][(i as i32 % 3) as usize] = s15Fixed16Number_to_float(
                read_s15Fixed16Number(src, (offset + 8 + (i as i32 * 4) as libc::c_uint) as usize),
            );
            i = i + 1
        }
        matrix.invalid = false
    } else {
        matrix.invalid = true;
        invalid_source(src, "missing sf32tag");
    }
    return matrix;
}
fn read_tag_XYZType(mut src: &mut mem_source, mut index: &tag_index, mut tag_id: u32) -> XYZNumber {
    let mut num: XYZNumber = {
        let mut init = XYZNumber { X: 0, Y: 0, Z: 0 };
        init
    };
    let mut tag = find_tag(&index, tag_id);
    if let Some(tag) = tag {
        let mut offset: u32 = (*tag).offset;
        let mut type_0: u32 = read_u32(src, offset as usize);
        if type_0 != XYZ_TYPE {
            invalid_source(src, "unexpected type, expected XYZ");
        }
        num.X = read_s15Fixed16Number(src, (offset + 8) as usize);
        num.Y = read_s15Fixed16Number(src, (offset + 12) as usize);
        num.Z = read_s15Fixed16Number(src, (offset + 16) as usize)
    } else {
        invalid_source(src, "missing xyztag");
    }
    return num;
}
// Read the tag at a given offset rather then the tag_index.
// This method is used when reading mAB tags where nested curveType are
// present that are not part of the tag_index.
unsafe extern "C" fn read_curveType(
    mut src: &mut mem_source,
    mut offset: u32,
    mut len: *mut u32,
) -> *mut curveType {
    static mut COUNT_TO_LENGTH: [u32; 5] = [1, 3, 4, 5, 7]; //PARAMETRIC_CURVE_TYPE
    let mut curve: *mut curveType;
    let mut type_0: u32 = read_u32(src, offset as usize);
    let mut count: u32;
    let mut i: u32;
    if type_0 != CURVE_TYPE && type_0 != PARAMETRIC_CURVE_TYPE {
        invalid_source(src, "unexpected type, expected CURV or PARA");
        return 0 as *mut curveType;
    }
    if type_0 == CURVE_TYPE {
        count = read_u32(src, (offset + 8) as usize);
        //arbitrary
        if count > 40000 {
            invalid_source(src, "curve size too large");
            return 0 as *mut curveType;
        }
        curve = malloc(
            ::std::mem::size_of::<curveType>()
                + ::std::mem::size_of::<uInt16Number>() * count as usize,
        ) as *mut curveType;
        if curve.is_null() {
            return 0 as *mut curveType;
        }
        (*curve).count = count;
        (*curve).type_0 = CURVE_TYPE;
        i = 0;
        while i < count {
            *(*curve).data.as_mut_ptr().offset(i as isize) =
                read_u16(src, (offset + 12 + i * 2) as usize);
            i = i + 1
        }
        *len = 12 + count * 2
    } else {
        count = read_u16(src, (offset + 8) as usize) as u32;
        if count > 4 {
            invalid_source(src, "parametric function type not supported.");
            return 0 as *mut curveType;
        }
        curve = malloc(::std::mem::size_of::<curveType>()) as *mut curveType;
        if curve.is_null() {
            return 0 as *mut curveType;
        }
        (*curve).count = count;
        (*curve).type_0 = PARAMETRIC_CURVE_TYPE;
        i = 0;
        while i < COUNT_TO_LENGTH[count as usize] {
            (*curve).parameter[i as usize] = s15Fixed16Number_to_float(read_s15Fixed16Number(
                src,
                (offset + 12 + i * 4) as usize,
            ));
            i = i + 1
        }
        *len = 12 + COUNT_TO_LENGTH[count as usize] * 4;
        if count == 1 || count == 2 {
            /* we have a type 1 or type 2 function that has a division by 'a' */
            let mut a: f32 = (*curve).parameter[1];
            if a == 0.0 {
                invalid_source(src, "parametricCurve definition causes division by zero");
            }
        }
    }
    return curve;
}
unsafe fn read_tag_curveType(
    mut src: &mut mem_source,
    mut index: &tag_index,
    mut tag_id: u32,
) -> *mut curveType {
    let mut tag = find_tag(index, tag_id);
    let mut curve: *mut curveType = 0 as *mut curveType;
    if let Some(tag) = tag {
        let mut len: u32 = 0;
        return read_curveType(src, (*tag).offset, &mut len);
    } else {
        invalid_source(src, "missing curvetag");
    }
    return curve;
}
// arbitrary
unsafe extern "C" fn read_nested_curveType(
    mut src: &mut mem_source,
    mut curveArray: *mut [*mut curveType; 10],
    mut num_channels: u8,
    mut curve_offset: u32,
) {
    let mut channel_offset: u32 = 0;
    let mut i: i32;
    i = 0;
    while i < num_channels as i32 {
        let mut tag_len: u32 = 0;
        (*curveArray)[i as usize] =
            read_curveType(src, curve_offset + channel_offset, &mut tag_len);
        if (*curveArray)[i as usize].is_null() {
            invalid_source(src, "invalid nested curveType curve");
            break;
        } else {
            channel_offset = channel_offset + tag_len;
            // 4 byte aligned
            if tag_len % 4 != 0 {
                channel_offset = channel_offset + (4 - tag_len % 4)
            }
            i += 1
        }
    }
}
unsafe extern "C" fn mAB_release(mut lut: *mut lutmABType) {
    let mut i: u8;
    i = 0u8;
    while (i as i32) < (*lut).num_in_channels as i32 {
        free((*lut).a_curves[i as usize] as *mut libc::c_void);
        i = i + 1
    }
    i = 0u8;
    while (i as i32) < (*lut).num_out_channels as i32 {
        free((*lut).b_curves[i as usize] as *mut libc::c_void);
        free((*lut).m_curves[i as usize] as *mut libc::c_void);
        i = i + 1
    }
    free(lut as *mut libc::c_void);
}
/* See section 10.10 for specs */
unsafe fn read_tag_lutmABType(mut src: &mut mem_source, mut tag: &tag) -> *mut lutmABType {
    let mut offset: u32 = (*tag).offset;
    let mut a_curve_offset: u32;
    let mut b_curve_offset: u32;
    let mut m_curve_offset: u32;
    let mut matrix_offset: u32;
    let mut clut_offset: u32;
    let mut clut_size: u32 = 1;
    let mut clut_precision: u8;
    let mut type_0: u32 = read_u32(src, offset as usize);
    let mut num_in_channels: u8;
    let mut num_out_channels: u8;
    let mut lut: *mut lutmABType;
    let mut i: u32;
    if type_0 != LUT_MAB_TYPE && type_0 != LUT_MBA_TYPE {
        return 0 as *mut lutmABType;
    }
    num_in_channels = read_u8(src, (offset + 8) as usize);
    num_out_channels = read_u8(src, (offset + 9) as usize);
    if num_in_channels as i32 > 10 || num_out_channels as i32 > 10 {
        return 0 as *mut lutmABType;
    }
    // We require 3in/out channels since we only support RGB->XYZ (or RGB->LAB)
    // XXX: If we remove this restriction make sure that the number of channels
    //      is less or equal to the maximum number of mAB curves in qcmsint.h
    //      also check for clut_size overflow. Also make sure it's != 0
    if num_in_channels as i32 != 3 || num_out_channels as i32 != 3 {
        return 0 as *mut lutmABType;
    }
    // some of this data is optional and is denoted by a zero offset
    // we also use this to track their existance
    a_curve_offset = read_u32(src, (offset + 28) as usize);
    clut_offset = read_u32(src, (offset + 24) as usize);
    m_curve_offset = read_u32(src, (offset + 20) as usize);
    matrix_offset = read_u32(src, (offset + 16) as usize);
    b_curve_offset = read_u32(src, (offset + 12) as usize);
    // Convert offsets relative to the tag to relative to the profile
    // preserve zero for optional fields
    if a_curve_offset != 0 {
        a_curve_offset = a_curve_offset + offset
    }
    if clut_offset != 0 {
        clut_offset = clut_offset + offset
    }
    if m_curve_offset != 0 {
        m_curve_offset = m_curve_offset + offset
    }
    if matrix_offset != 0 {
        matrix_offset = matrix_offset + offset
    }
    if b_curve_offset != 0 {
        b_curve_offset = b_curve_offset + offset
    }
    if clut_offset != 0 {
        debug_assert!(num_in_channels as i32 == 3);
        // clut_size can not overflow since lg(256^num_in_channels) = 24 bits.
        i = 0;
        while i < num_in_channels as libc::c_uint {
            clut_size = clut_size * read_u8(src, (clut_offset + i) as usize) as libc::c_uint;
            if clut_size == 0 {
                invalid_source(src, "bad clut_size");
            }
            i = i + 1
        }
    } else {
        clut_size = 0
    }
    // 24bits * 3 won't overflow either
    clut_size = clut_size * num_out_channels as libc::c_uint;
    if clut_size > 500000 {
        return 0 as *mut lutmABType;
    }
    lut = malloc(
        ::std::mem::size_of::<lutmABType>() + clut_size as usize * ::std::mem::size_of::<f32>(),
    ) as *mut lutmABType;
    if lut.is_null() {
        return 0 as *mut lutmABType;
    }
    // we'll fill in the rest below
    std::ptr::write_bytes(lut, 0, 1);
    (*lut).clut_table = (*lut).clut_table_data.as_mut_ptr().offset(0isize) as *mut f32;
    if clut_offset != 0 {
        i = 0;
        while i < num_in_channels as libc::c_uint {
            (*lut).num_grid_points[i as usize] = read_u8(src, (clut_offset + i) as usize);
            if (*lut).num_grid_points[i as usize] as i32 == 0 {
                invalid_source(src, "bad grid_points");
            }
            i = i + 1
        }
    }
    // Reverse the processing of transformation elements for mBA type.
    (*lut).reversed = type_0 == LUT_MBA_TYPE;
    (*lut).num_in_channels = num_in_channels;
    (*lut).num_out_channels = num_out_channels;
    if matrix_offset != 0 {
        // read the matrix if we have it
        (*lut).e00 = read_s15Fixed16Number(src, (matrix_offset + (4 * 0) as libc::c_uint) as usize); // the caller checks that this doesn't happen
        (*lut).e01 = read_s15Fixed16Number(src, (matrix_offset + (4 * 1) as libc::c_uint) as usize);
        (*lut).e02 = read_s15Fixed16Number(src, (matrix_offset + (4 * 2) as libc::c_uint) as usize);
        (*lut).e10 = read_s15Fixed16Number(src, (matrix_offset + (4 * 3) as libc::c_uint) as usize);
        (*lut).e11 = read_s15Fixed16Number(src, (matrix_offset + (4 * 4) as libc::c_uint) as usize);
        (*lut).e12 = read_s15Fixed16Number(src, (matrix_offset + (4 * 5) as libc::c_uint) as usize);
        (*lut).e20 = read_s15Fixed16Number(src, (matrix_offset + (4 * 6) as libc::c_uint) as usize);
        (*lut).e21 = read_s15Fixed16Number(src, (matrix_offset + (4 * 7) as libc::c_uint) as usize);
        (*lut).e22 = read_s15Fixed16Number(src, (matrix_offset + (4 * 8) as libc::c_uint) as usize);
        (*lut).e03 = read_s15Fixed16Number(src, (matrix_offset + (4 * 9) as libc::c_uint) as usize);
        (*lut).e13 =
            read_s15Fixed16Number(src, (matrix_offset + (4 * 10) as libc::c_uint) as usize);
        (*lut).e23 = read_s15Fixed16Number(src, (matrix_offset + (4 * 11) as libc::c_uint) as usize)
    }
    if a_curve_offset != 0 {
        read_nested_curveType(src, &mut (*lut).a_curves, num_in_channels, a_curve_offset);
    }
    if m_curve_offset != 0 {
        read_nested_curveType(src, &mut (*lut).m_curves, num_out_channels, m_curve_offset);
    }
    if b_curve_offset != 0 {
        read_nested_curveType(src, &mut (*lut).b_curves, num_out_channels, b_curve_offset);
    } else {
        invalid_source(src, "B curves required");
    }
    if clut_offset != 0 {
        clut_precision = read_u8(src, (clut_offset + 16) as usize);
        if clut_precision as i32 == 1 {
            i = 0;
            while i < clut_size {
                *(*lut).clut_table.offset(i as isize) = uInt8Number_to_float(read_uInt8Number(
                    src,
                    (clut_offset + 20 + i * 1) as usize,
                ));
                i = i + 1
            }
        } else if clut_precision as i32 == 2 {
            i = 0;
            while i < clut_size {
                *(*lut).clut_table.offset(i as isize) = uInt16Number_to_float(read_uInt16Number(
                    src,
                    (clut_offset + 20 + i * 2) as usize,
                ));
                i = i + 1
            }
        } else {
            invalid_source(src, "Invalid clut precision");
        }
    }
    if !(*src).valid {
        mAB_release(lut);
        return 0 as *mut lutmABType;
    }
    return lut;
}
unsafe fn read_tag_lutType(mut src: &mut mem_source, mut tag: &tag) -> *mut lutType {
    let mut offset: u32 = (*tag).offset;
    let mut type_0: u32 = read_u32(src, offset as usize);
    let mut num_input_table_entries: u16;
    let mut num_output_table_entries: u16;
    let mut in_chan: u8;
    let mut grid_points: u8;
    let mut out_chan: u8;
    let mut input_offset: u32;
    let mut clut_offset: u32;
    let mut output_offset: u32;
    let mut clut_size: u32;
    let mut entry_size: usize;
    let mut lut: *mut lutType;
    let mut i: u32;
    if type_0 == LUT8_TYPE {
        num_input_table_entries = 256u16;
        num_output_table_entries = 256u16;
        entry_size = 1;
        input_offset = 48
    } else if type_0 == LUT16_TYPE {
        num_input_table_entries = read_u16(src, (offset + 48) as usize);
        num_output_table_entries = read_u16(src, (offset + 50) as usize);
        if num_input_table_entries as i32 == 0 || num_output_table_entries as i32 == 0 {
            invalid_source(src, "Bad channel count");
            return 0 as *mut lutType;
        }
        entry_size = 2;
        input_offset = 52
    } else {
        debug_assert!(false);
        invalid_source(src, "Unexpected lut type");
        return 0 as *mut lutType;
    }
    in_chan = read_u8(src, (offset + 8) as usize);
    out_chan = read_u8(src, (offset + 9) as usize);
    grid_points = read_u8(src, (offset + 10) as usize);
    clut_size = (grid_points as f64).powf(in_chan as f64) as u32;
    if clut_size > 500000 {
        invalid_source(src, "CLUT too large");
        return 0 as *mut lutType;
    }
    if clut_size <= 0 {
        invalid_source(src, "CLUT must not be empty.");
        return 0 as *mut lutType;
    }
    if in_chan as i32 != 3 || out_chan as i32 != 3 {
        invalid_source(src, "CLUT only supports RGB");
        return 0 as *mut lutType;
    }
    lut = malloc(
        ::std::mem::size_of::<lutType>()
            + ((num_input_table_entries as i32 * in_chan as i32) as libc::c_uint
                + clut_size * out_chan as libc::c_uint
                + (num_output_table_entries as i32 * out_chan as i32) as libc::c_uint)
                as usize
                * ::std::mem::size_of::<f32>(),
    ) as *mut lutType;
    if lut.is_null() {
        invalid_source(src, "CLUT too large");
        return 0 as *mut lutType;
    }
    /* compute the offsets of tables */
    (*lut).input_table = &mut *(*lut).table_data.as_mut_ptr().offset(0isize) as *mut f32;
    (*lut).clut_table = &mut *(*lut)
        .table_data
        .as_mut_ptr()
        .offset((in_chan as i32 * num_input_table_entries as i32) as isize)
        as *mut f32;
    (*lut).output_table = &mut *(*lut).table_data.as_mut_ptr().offset(
        ((in_chan as i32 * num_input_table_entries as i32) as libc::c_uint
            + clut_size * out_chan as libc::c_uint) as isize,
    ) as *mut f32;
    (*lut).num_input_table_entries = num_input_table_entries;
    (*lut).num_output_table_entries = num_output_table_entries;
    (*lut).num_input_channels = in_chan;
    (*lut).num_output_channels = out_chan;
    (*lut).num_clut_grid_points = grid_points;
    (*lut).e00 = read_s15Fixed16Number(src, (offset + 12) as usize);
    (*lut).e01 = read_s15Fixed16Number(src, (offset + 16) as usize);
    (*lut).e02 = read_s15Fixed16Number(src, (offset + 20) as usize);
    (*lut).e10 = read_s15Fixed16Number(src, (offset + 24) as usize);
    (*lut).e11 = read_s15Fixed16Number(src, (offset + 28) as usize);
    (*lut).e12 = read_s15Fixed16Number(src, (offset + 32) as usize);
    (*lut).e20 = read_s15Fixed16Number(src, (offset + 36) as usize);
    (*lut).e21 = read_s15Fixed16Number(src, (offset + 40) as usize);
    (*lut).e22 = read_s15Fixed16Number(src, (offset + 44) as usize);
    i = 0;
    while i < ((*lut).num_input_table_entries as i32 * in_chan as i32) as u32 {
        if type_0 == LUT8_TYPE {
            *(*lut).input_table.offset(i as isize) = uInt8Number_to_float(read_uInt8Number(
                src,
                (offset + input_offset) as usize + i as usize * entry_size,
            ))
        } else {
            *(*lut).input_table.offset(i as isize) = uInt16Number_to_float(read_uInt16Number(
                src,
                (offset + input_offset) as usize + i as usize * entry_size,
            ))
        }
        i = i + 1
    }
    clut_offset = ((offset + input_offset) as usize
        + ((*lut).num_input_table_entries as i32 * in_chan as i32) as usize * entry_size)
        as u32;
    i = 0;
    while i < clut_size * out_chan as libc::c_uint {
        if type_0 == LUT8_TYPE {
            *(*lut).clut_table.offset((i + 0) as isize) = uInt8Number_to_float(read_uInt8Number(
                src,
                clut_offset as usize + i as usize * entry_size + 0,
            ));
            *(*lut).clut_table.offset((i + 1) as isize) = uInt8Number_to_float(read_uInt8Number(
                src,
                clut_offset as usize + i as usize * entry_size + 1,
            ));
            *(*lut).clut_table.offset((i + 2) as isize) = uInt8Number_to_float(read_uInt8Number(
                src,
                clut_offset as usize + i as usize * entry_size + 2,
            ))
        } else {
            *(*lut).clut_table.offset((i + 0) as isize) = uInt16Number_to_float(read_uInt16Number(
                src,
                clut_offset as usize + i as usize * entry_size + 0,
            ));
            *(*lut).clut_table.offset((i + 1) as isize) = uInt16Number_to_float(read_uInt16Number(
                src,
                clut_offset as usize + i as usize * entry_size + 2,
            ));
            *(*lut).clut_table.offset((i + 2) as isize) = uInt16Number_to_float(read_uInt16Number(
                src,
                clut_offset as usize + i as usize * entry_size + 4,
            ))
        }
        i = i + 3
    }
    output_offset =
        (clut_offset as usize + (clut_size * out_chan as u32) as usize * entry_size) as u32;
    i = 0;
    while i < ((*lut).num_output_table_entries as i32 * out_chan as i32) as u32 {
        if type_0 == LUT8_TYPE {
            *(*lut).output_table.offset(i as isize) = uInt8Number_to_float(read_uInt8Number(
                src,
                output_offset as usize + i as usize * entry_size,
            ))
        } else {
            *(*lut).output_table.offset(i as isize) = uInt16Number_to_float(read_uInt16Number(
                src,
                output_offset as usize + i as usize * entry_size,
            ))
        }
        i = i + 1
    }
    return lut;
}
fn read_rendering_intent(mut profile: &mut qcms_profile, mut src: &mut mem_source) {
    (*profile).rendering_intent = read_u32(src, 64);
    match (*profile).rendering_intent {
        0 | 2 | 1 | 3 => {}
        _ => {
            invalid_source(src, "unknown rendering intent");
        }
    };
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create() -> Box<qcms_profile> {
    Box::new(std::mem::zeroed())
}
/* build sRGB gamma table */
/* based on cmsBuildParametricGamma() */
unsafe extern "C" fn build_sRGB_gamma_table(mut num_entries: i32) -> *mut u16 {
    let mut i: i32;
    /* taken from lcms: Build_sRGBGamma() */
    let mut gamma: f64 = 2.4f64;
    let mut a: f64 = 1.0f64 / 1.055f64;
    let mut b: f64 = 0.055f64 / 1.055f64;
    let mut c: f64 = 1.0f64 / 12.92f64;
    let mut d: f64 = 0.04045f64;
    let mut table: *mut u16 =
        malloc(::std::mem::size_of::<u16>() * num_entries as usize) as *mut u16;
    if table.is_null() {
        return 0 as *mut u16;
    }
    i = 0;
    while i < num_entries {
        let mut x: f64 = i as f64 / (num_entries - 1) as f64;
        let mut y: f64;
        let mut output: f64;
        // IEC 61966-2.1 (sRGB)
        // Y = (aX + b)^Gamma | X >= d
        // Y = cX             | X < d
        if x >= d {
            let mut e: f64 = a * x + b;
            if e > 0f64 {
                y = e.powf(gamma)
            } else {
                y = 0f64
            }
        } else {
            y = c * x
        }
        // Saturate -- this could likely move to a separate function
        output = y * 65535.0f64 + 0.5f64;
        if output > 65535.0f64 {
            output = 65535f64
        }
        if output < 0f64 {
            output = 0f64
        }
        *table.offset(i as isize) = output.floor() as u16;
        i += 1
    }
    return table;
}
unsafe extern "C" fn curve_from_table(mut table: *mut u16, mut num_entries: i32) -> *mut curveType {
    let mut curve: *mut curveType;
    let mut i: i32;
    curve = malloc(
        ::std::mem::size_of::<curveType>()
            + ::std::mem::size_of::<uInt16Number>() * num_entries as usize,
    ) as *mut curveType;
    if curve.is_null() {
        return 0 as *mut curveType;
    }
    (*curve).type_0 = CURVE_TYPE;
    (*curve).count = num_entries as u32;
    i = 0;
    while i < num_entries {
        *(*curve).data.as_mut_ptr().offset(i as isize) = *table.offset(i as isize);
        i += 1
    }
    return curve;
}
unsafe extern "C" fn float_to_u8Fixed8Number(mut a: f32) -> u16 {
    if a > 255.0 + 255.0 / 256f32 {
        return 0xffffu16;
    } else if a < 0.0 {
        return 0u16;
    } else {
        return (a * 256.0 + 0.5).floor() as u16;
    };
}
unsafe extern "C" fn curve_from_gamma(mut gamma: f32) -> *mut curveType {
    let mut curve: *mut curveType;
    let mut num_entries: i32 = 1;
    curve = malloc(
        ::std::mem::size_of::<curveType>()
            + ::std::mem::size_of::<uInt16Number>() * num_entries as usize,
    ) as *mut curveType;
    if curve.is_null() {
        return 0 as *mut curveType;
    }
    (*curve).count = num_entries as u32;
    *(*curve).data.as_mut_ptr().offset(0isize) = float_to_u8Fixed8Number(gamma);
    (*curve).type_0 = CURVE_TYPE;
    return curve;
}
//XXX: it would be nice if we had a way of ensuring
// everything in a profile was initialized regardless of how it was created
//XXX: should this also be taking a black_point?
/* similar to CGColorSpaceCreateCalibratedRGB */
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_rgb_with_gamma_set(
    mut white_point: qcms_CIE_xyY,
    mut primaries: qcms_CIE_xyYTRIPLE,
    mut redGamma: f32,
    mut greenGamma: f32,
    mut blueGamma: f32,
) -> *mut qcms_profile {
    let mut profile = qcms_profile_create();

    //XXX: should store the whitepoint
    if !set_rgb_colorants(&mut profile, white_point, primaries) {
        return 0 as *mut qcms_profile;
    }
    (*profile).redTRC = curve_from_gamma(redGamma);
    (*profile).blueTRC = curve_from_gamma(blueGamma);
    (*profile).greenTRC = curve_from_gamma(greenGamma);
    if (*profile).redTRC.is_null() || (*profile).blueTRC.is_null() || (*profile).greenTRC.is_null()
    {
        return 0 as *mut qcms_profile;
    }
    (*profile).class_type = DISPLAY_DEVICE_PROFILE;
    (*profile).rendering_intent = QCMS_INTENT_PERCEPTUAL;
    (*profile).color_space = RGB_SIGNATURE;
    (*profile).pcs = XYZ_TYPE;
    return Box::into_raw(profile);
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_gray_with_gamma(mut gamma: f32) -> *mut qcms_profile {
    let mut profile = qcms_profile_create();

    (*profile).grayTRC = curve_from_gamma(gamma);
    if (*profile).grayTRC.is_null() {
        return 0 as *mut qcms_profile;
    }
    (*profile).class_type = DISPLAY_DEVICE_PROFILE;
    (*profile).rendering_intent = QCMS_INTENT_PERCEPTUAL;
    (*profile).color_space = GRAY_SIGNATURE;
    (*profile).pcs = XYZ_TYPE;
    return Box::into_raw(profile);
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_rgb_with_gamma(
    mut white_point: qcms_CIE_xyY,
    mut primaries: qcms_CIE_xyYTRIPLE,
    mut gamma: f32,
) -> *mut qcms_profile {
    return qcms_profile_create_rgb_with_gamma_set(white_point, primaries, gamma, gamma, gamma);
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_rgb_with_table(
    mut white_point: qcms_CIE_xyY,
    mut primaries: qcms_CIE_xyYTRIPLE,
    mut table: *mut u16,
    mut num_entries: i32,
) -> *mut qcms_profile {
    let mut profile = qcms_profile_create();
    //XXX: should store the whitepoint
    if !set_rgb_colorants(&mut profile, white_point, primaries) {
        return 0 as *mut qcms_profile;
    }
    (*profile).redTRC = curve_from_table(table, num_entries);
    (*profile).blueTRC = curve_from_table(table, num_entries);
    (*profile).greenTRC = curve_from_table(table, num_entries);
    if (*profile).redTRC.is_null() || (*profile).blueTRC.is_null() || (*profile).greenTRC.is_null()
    {
        return 0 as *mut qcms_profile;
    }
    (*profile).class_type = DISPLAY_DEVICE_PROFILE;
    (*profile).rendering_intent = QCMS_INTENT_PERCEPTUAL;
    (*profile).color_space = RGB_SIGNATURE;
    (*profile).pcs = XYZ_TYPE;
    return Box::into_raw(profile);
}
/* from lcms: cmsWhitePointFromTemp */
/* tempK must be >= 4000. and <= 25000.
 * Invalid values of tempK will return
 * (x,y,Y) = (-1.0, -1.0, -1.0)
 * similar to argyll: icx_DTEMP2XYZ() */
fn white_point_from_temp(mut temp_K: i32) -> qcms_CIE_xyY {
    let mut white_point: qcms_CIE_xyY = qcms_CIE_xyY {
        x: 0.,
        y: 0.,
        Y: 0.,
    };
    let mut x: f64;
    let mut y: f64;
    let mut T: f64;
    let mut T2: f64;
    let mut T3: f64;
    // double M1, M2;
    // No optimization provided.
    T = temp_K as f64; // Square
    T2 = T * T; // Cube
    T3 = T2 * T;
    // For correlated color temperature (T) between 4000K and 7000K:
    if T >= 4000.0f64 && T <= 7000.0f64 {
        x = -4.6070f64 * (1E9f64 / T3)
            + 2.9678f64 * (1E6f64 / T2)
            + 0.09911f64 * (1E3f64 / T)
            + 0.244063f64
    } else if T > 7000.0f64 && T <= 25000.0f64 {
        x = -2.0064f64 * (1E9f64 / T3)
            + 1.9018f64 * (1E6f64 / T2)
            + 0.24748f64 * (1E3f64 / T)
            + 0.237040f64
    } else {
        // or for correlated color temperature (T) between 7000K and 25000K:
        // Invalid tempK
        white_point.x = -1.0f64;
        white_point.y = -1.0f64;
        white_point.Y = -1.0f64;
        debug_assert!(false, "invalid temp");
        return white_point;
    }
    // Obtain y(x)
    y = -3.000f64 * (x * x) + 2.870f64 * x - 0.275f64;
    // wave factors (not used, but here for futures extensions)
    // M1 = (-1.3515 - 1.7703*x + 5.9114 *y)/(0.0241 + 0.2562*x - 0.7341*y);
    // M2 = (0.0300 - 31.4424*x + 30.0717*y)/(0.0241 + 0.2562*x - 0.7341*y);
    // Fill white_point struct
    white_point.x = x;
    white_point.y = y;
    white_point.Y = 1.0f64;
    return white_point;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_white_point_sRGB() -> qcms_CIE_xyY {
    return white_point_from_temp(6504);
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_sRGB() -> *mut qcms_profile {
    let mut profile: *mut qcms_profile;
    let mut table: *mut u16;
    let mut Rec709Primaries: qcms_CIE_xyYTRIPLE = {
        let mut init = qcms_CIE_xyYTRIPLE {
            red: {
                let mut init = qcms_CIE_xyY {
                    x: 0.6400f64,
                    y: 0.3300f64,
                    Y: 1.0f64,
                };
                init
            },
            green: {
                let mut init = qcms_CIE_xyY {
                    x: 0.3000f64,
                    y: 0.6000f64,
                    Y: 1.0f64,
                };
                init
            },
            blue: {
                let mut init = qcms_CIE_xyY {
                    x: 0.1500f64,
                    y: 0.0600f64,
                    Y: 1.0f64,
                };
                init
            },
        };
        init
    };
    let D65 = qcms_white_point_sRGB();
    table = build_sRGB_gamma_table(1024);
    if table.is_null() {
        return 0 as *mut qcms_profile;
    }
    profile = qcms_profile_create_rgb_with_table(D65, Rec709Primaries, table, 1024);
    free(table as *mut libc::c_void);
    return profile;
}
/* qcms_profile_from_memory does not hold a reference to the memory passed in */
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_memory(
    mut mem: *const libc::c_void,
    mut size: usize,
) -> *mut qcms_profile {
    let mut length: u32;
    let mut source: mem_source = mem_source {
        buf: &[],
        valid: false,
        invalid_reason: None,
    };
    let mut index;
    source.buf = slice::from_raw_parts(mem as *const libc::c_uchar, size);
    source.valid = true;
    let mut src: &mut mem_source = &mut source;
    if size < 4 {
        return null_mut();
    }
    length = read_u32(src, 0);
    if length as usize <= size {
        // shrink the area that we can read if appropriate
        (*src).buf = &(*src).buf[0..length as usize];
    } else {
        return null_mut();
    }
    /* ensure that the profile size is sane so it's easier to reason about */
    if src.buf.len() <= 64 || src.buf.len() >= MAX_PROFILE_SIZE {
        return null_mut();
    }
    let mut profile = qcms_profile_create();

    check_CMM_type_signature(src);
    check_profile_version(src);
    read_class_signature(&mut profile, src);
    read_rendering_intent(&mut profile, src);
    read_color_space(&mut profile, src);
    read_pcs(&mut profile, src);
    //TODO read rest of profile stuff
    if !(*src).valid {
        return null_mut();
    }

    index = read_tag_table(&mut profile, src);
    if !(*src).valid || index.is_empty() {
        return null_mut();
    }

    if find_tag(&index, TAG_CHAD).is_some() {
        (*profile).chromaticAdaption = read_tag_s15Fixed16ArrayType(src, &index, TAG_CHAD)
    } else {
        (*profile).chromaticAdaption.invalid = true //Signal the data is not present
    }

    if (*profile).class_type == DISPLAY_DEVICE_PROFILE
        || (*profile).class_type == INPUT_DEVICE_PROFILE
        || (*profile).class_type == OUTPUT_DEVICE_PROFILE
        || (*profile).class_type == COLOR_SPACE_PROFILE
    {
        if (*profile).color_space == RGB_SIGNATURE {
            if let Some(A2B0) = find_tag(&index, TAG_A2B0) {
                let lut_type = read_u32(src, A2B0.offset as usize);
                if lut_type == LUT8_TYPE || lut_type == LUT16_TYPE {
                    (*profile).A2B0 = read_tag_lutType(src, A2B0)
                } else if lut_type == LUT_MAB_TYPE {
                    (*profile).mAB = read_tag_lutmABType(src, A2B0)
                }
            }
            if let Some(B2A0) = find_tag(&index, TAG_B2A0) {
                let lut_type = read_u32(src, B2A0.offset as usize);
                if lut_type == LUT8_TYPE || lut_type == LUT16_TYPE {
                    (*profile).B2A0 = read_tag_lutType(src, B2A0)
                } else if lut_type == LUT_MBA_TYPE {
                    (*profile).mBA = read_tag_lutmABType(src, B2A0)
                }
            }
            if find_tag(&index, TAG_rXYZ).is_some() || !qcms_supports_iccv4.load(Ordering::Relaxed)
            {
                (*profile).redColorant = read_tag_XYZType(src, &index, TAG_rXYZ);
                (*profile).greenColorant = read_tag_XYZType(src, &index, TAG_gXYZ);
                (*profile).blueColorant = read_tag_XYZType(src, &index, TAG_bXYZ)
            }
            if !(*src).valid {
                return null_mut();
            }

            if find_tag(&index, TAG_rTRC).is_some() || !qcms_supports_iccv4.load(Ordering::Relaxed)
            {
                (*profile).redTRC = read_tag_curveType(src, &index, TAG_rTRC);
                (*profile).greenTRC = read_tag_curveType(src, &index, TAG_gTRC);
                (*profile).blueTRC = read_tag_curveType(src, &index, TAG_bTRC);
                if (*profile).redTRC.is_null()
                    || (*profile).blueTRC.is_null()
                    || (*profile).greenTRC.is_null()
                {
                    return null_mut();
                }
            }
        } else if (*profile).color_space == GRAY_SIGNATURE {
            (*profile).grayTRC = read_tag_curveType(src, &index, TAG_kTRC);
            if (*profile).grayTRC.is_null() {
                return null_mut();
            }
        } else {
            debug_assert!(false, "read_color_space protects against entering here");
            return null_mut();
        }
    } else {
        return null_mut();
    }

    if !(*src).valid {
        return null_mut();
    }

    Box::into_raw(profile)
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_get_rendering_intent(
    mut profile: *mut qcms_profile,
) -> qcms_intent {
    return (*profile).rendering_intent;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_get_color_space(
    mut profile: *mut qcms_profile,
) -> icColorSpaceSignature {
    return (*profile).color_space;
}
unsafe extern "C" fn lut_release(mut lut: *mut lutType) {
    free(lut as *mut libc::c_void);
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_release(mut profile: *mut qcms_profile) {
    drop(Box::from_raw(profile));
}
unsafe extern "C" fn qcms_data_from_file(
    mut file: *mut FILE,
    mut mem: *mut *mut libc::c_void,
    mut size: *mut usize,
) {
    let mut length: u32;
    let mut remaining_length: u32;
    let mut read_length: usize;
    let mut length_be: be32 = 0;
    let mut data: *mut libc::c_void;
    *mem = 0 as *mut libc::c_void;
    *size = 0;
    if fread(
        &mut length_be as *mut be32 as *mut libc::c_void,
        1,
        ::std::mem::size_of::<be32>(),
        file,
    ) != ::std::mem::size_of::<be32>()
    {
        return;
    }
    length = be32_to_cpu(length_be);
    if length > MAX_PROFILE_SIZE as libc::c_uint
        || (length as libc::c_ulong) < ::std::mem::size_of::<be32>() as libc::c_ulong
    {
        return;
    }
    /* allocate room for the entire profile */
    data = malloc(length as usize);
    if data.is_null() {
        return;
    }
    /* copy in length to the front so that the buffer will contain the entire profile */
    *(data as *mut be32) = length_be;
    remaining_length =
        (length as libc::c_ulong - ::std::mem::size_of::<be32>() as libc::c_ulong) as u32;
    /* read the rest profile */
    read_length = fread(
        (data as *mut libc::c_uchar).offset(::std::mem::size_of::<be32>() as isize)
            as *mut libc::c_void,
        1,
        remaining_length as usize,
        file,
    ) as usize;
    if read_length != remaining_length as usize {
        free(data);
        return;
    }
    /* successfully get the profile.*/
    *mem = data;
    *size = length as usize;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_file(mut file: *mut FILE) -> *mut qcms_profile {
    let mut length: usize = 0;
    let mut profile: *mut qcms_profile;
    let mut data: *mut libc::c_void = 0 as *mut libc::c_void;
    qcms_data_from_file(file, &mut data, &mut length);
    if data.is_null() || length == 0 {
        return 0 as *mut qcms_profile;
    }
    profile = qcms_profile_from_memory(data, length);
    free(data);
    return profile;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_path(
    mut path: *const libc::c_char,
) -> *mut qcms_profile {
    let mut profile: *mut qcms_profile = 0 as *mut qcms_profile;
    let mut file = fopen(path, b"rb\x00" as *const u8 as *const libc::c_char);
    if !file.is_null() {
        profile = qcms_profile_from_file(file);
        fclose(file);
    }
    return profile;
}
#[no_mangle]
pub unsafe extern "C" fn qcms_data_from_path(
    mut path: *const libc::c_char,
    mut mem: *mut *mut libc::c_void,
    mut size: *mut usize,
) {
    *mem = 0 as *mut libc::c_void;
    *size = 0;
    let file = fopen(path, b"rb\x00" as *const u8 as *const libc::c_char);
    if !file.is_null() {
        qcms_data_from_file(file, mem, size);
        fclose(file);
    };
}

#[cfg(windows)]
extern "C" {
    pub fn _wfopen(filename: *const libc::wchar_t, mode: *const libc::wchar_t) -> *mut FILE;
}

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_unicode_path(mut path: *const libc::wchar_t) {
    let mut file = _wfopen(path, ['r' as u16, 'b' as u16, '\0' as u16].as_ptr());
    if !file.is_null() {
        qcms_profile_from_file(file);
        fclose(file);
    };
}

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "C" fn qcms_data_from_unicode_path(
    mut path: *const libc::wchar_t,
    mut mem: *mut *mut libc::c_void,
    mut size: *mut usize,
) {
    *mem = 0 as *mut libc::c_void;
    *size = 0;
    let mut file = _wfopen(path, ['r' as u16, 'b' as u16, '\0' as u16].as_ptr());
    if !file.is_null() {
        qcms_data_from_file(file, mem, size);
        fclose(file);
    };
}

#[no_mangle]
pub unsafe extern "C" fn qcms_data_create_rgb_with_gamma(
    mut white_point: qcms_CIE_xyY,
    mut primaries: qcms_CIE_xyYTRIPLE,
    mut gamma: f32,
    mut mem: *mut *mut libc::c_void,
    mut size: *mut usize,
) {
    let mut length: u32;
    let mut index: u32;
    let mut xyz_count: u32;
    let mut trc_count: u32;
    let mut tag_table_offset: usize;
    let mut tag_data_offset: usize;
    let mut data: *mut libc::c_void;
    let mut colorants: matrix = matrix {
        m: [[0.; 3]; 3],
        invalid: false,
    };
    let mut TAG_XYZ: [u32; 3] = [TAG_rXYZ, TAG_gXYZ, TAG_bXYZ];
    let mut TAG_TRC: [u32; 3] = [TAG_rTRC, TAG_gTRC, TAG_bTRC];
    if mem.is_null() || size.is_null() {
        return;
    }
    *mem = 0 as *mut libc::c_void;
    *size = 0;
    /*
    	* total length = icc profile header(128) + tag count(4) +
    	* (tag table item (12) * total tag (6 = 3 rTRC + 3 rXYZ)) + rTRC elements data (3 * 20)
    	* + rXYZ elements data (3*16), and all tag data elements must start at the 4-byte boundary.
    	*/
    xyz_count = 3; // rXYZ, gXYZ, bXYZ
    trc_count = 3; // rTRC, gTRC, bTRC
    length =
        (128 + 4) as libc::c_uint + 12 * (xyz_count + trc_count) + xyz_count * 20 + trc_count * 16;
    // reserve the total memory.
    data = malloc(length as usize);
    if data.is_null() {
        return;
    }
    memset(data, 0, length as usize);
    // Part1 : write rXYZ, gXYZ and bXYZ
    if !get_rgb_colorants(&mut colorants, white_point, primaries) {
        free(data);
        return;
    }
    // the position of first tag's signature in tag table
    tag_table_offset = (128 + 4) as usize; // the start of tag data elements.
    tag_data_offset = ((128 + 4) as libc::c_uint + 12 * (xyz_count + trc_count)) as usize;
    index = 0;
    while index < xyz_count {
        // tag table
        write_u32(data, tag_table_offset, TAG_XYZ[index as usize]); // 20 bytes per TAG_(r/g/b)XYZ tag element
        write_u32(data, tag_table_offset + 4, tag_data_offset as u32);
        write_u32(data, tag_table_offset + 8, 20);
        // tag data element
        write_u32(data, tag_data_offset, XYZ_TYPE);
        // reserved 4 bytes.
        write_u32(
            data,
            tag_data_offset + 8,
            double_to_s15Fixed16Number(colorants.m[0][index as usize] as f64) as u32,
        );
        write_u32(
            data,
            tag_data_offset + 12,
            double_to_s15Fixed16Number(colorants.m[1][index as usize] as f64) as u32,
        );
        write_u32(
            data,
            tag_data_offset + 16,
            double_to_s15Fixed16Number(colorants.m[2][index as usize] as f64) as u32,
        );
        tag_table_offset = tag_table_offset + 12;
        tag_data_offset = tag_data_offset + 20;
        index = index + 1
    }
    // Part2 : write rTRC, gTRC and bTRC
    index = 0;
    while index < trc_count {
        // tag table
        write_u32(data, tag_table_offset, TAG_TRC[index as usize]); // 14 bytes per TAG_(r/g/b)TRC element
        write_u32(data, tag_table_offset + 4, tag_data_offset as u32);
        write_u32(data, tag_table_offset + 8, 14);
        // tag data element
        write_u32(data, tag_data_offset, CURVE_TYPE);
        // reserved 4 bytes.
        write_u32(data, tag_data_offset + 8, 1); // count
        write_u16(data, tag_data_offset + 12, float_to_u8Fixed8Number(gamma));
        tag_table_offset = tag_table_offset + 12;
        tag_data_offset = tag_data_offset + 16;
        index = index + 1
    }
    /* Part3 : write profile header
     *
     * Important header fields are left empty. This generates a profile for internal use only.
     * We should be generating: Profile version (04300000h), Profile signature (acsp),
     * PCS illumiant field. Likewise mandatory profile tags are omitted.
     */
    write_u32(data, 0, length); // the total length of this memory
    write_u32(data, 12, DISPLAY_DEVICE_PROFILE); // profile->class_type
    write_u32(data, 16, RGB_SIGNATURE); // profile->color_space
    write_u32(data, 20, XYZ_TYPE); // profile->pcs
    write_u32(data, 64, QCMS_INTENT_PERCEPTUAL); // profile->rendering_intent
    write_u32(data, 128, 6); // total tag count
                             // prepare the result
    *mem = data;
    *size = length as usize;
}

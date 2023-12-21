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
    convert::{TryInto, TryFrom},
    sync::atomic::AtomicBool,
    sync::Arc,
};

use crate::{
    double_to_s15Fixed16Number,
    transform::{set_rgb_colorants, PrecacheOuput},
};
use crate::{matrix::Matrix, s15Fixed16Number, s15Fixed16Number_to_float, Intent, Intent::*};

pub static SUPPORTS_ICCV4: AtomicBool = AtomicBool::new(cfg!(feature = "iccv4-enabled"));

pub const RGB_SIGNATURE: u32 = 0x52474220;
pub const GRAY_SIGNATURE: u32 = 0x47524159;
pub const XYZ_SIGNATURE: u32 = 0x58595A20;
pub const LAB_SIGNATURE: u32 = 0x4C616220;
pub const CMYK_SIGNATURE: u32 = 0x434D594B; // 'CMYK'

/// A color profile
#[derive(Default, Debug)]
pub struct Profile {
    pub(crate) class_type: u32,
    pub(crate) color_space: u32,
    pub(crate) pcs: u32,
    pub(crate) rendering_intent: Intent,
    pub(crate) redColorant: XYZNumber,
    pub(crate) blueColorant: XYZNumber,
    pub(crate) greenColorant: XYZNumber,
    // "TRC" is EOTF, e.g. gamma->linear transfer function.
    // Because ICC profiles are phrased as decodings to the xyzd50-linear PCS.
    pub(crate) redTRC: Option<Box<curveType>>,
    pub(crate) blueTRC: Option<Box<curveType>>,
    pub(crate) greenTRC: Option<Box<curveType>>,
    pub(crate) grayTRC: Option<Box<curveType>>,
    pub(crate) A2B0: Option<Box<lutType>>,
    pub(crate) B2A0: Option<Box<lutType>>,
    pub(crate) mAB: Option<Box<lutmABType>>,
    pub(crate) mBA: Option<Box<lutmABType>>,
    pub(crate) chromaticAdaption: Option<Matrix>,
    pub(crate) output_table_r: Option<Arc<PrecacheOuput>>,
    pub(crate) output_table_g: Option<Arc<PrecacheOuput>>,
    pub(crate) output_table_b: Option<Arc<PrecacheOuput>>,
    is_srgb: bool,
}

#[derive(Debug, Default)]
#[allow(clippy::upper_case_acronyms)]
pub(crate) struct lutmABType {
    pub num_in_channels: u8,
    pub num_out_channels: u8,
    // 16 is the upperbound, actual is 0..num_in_channels.
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
    // reversed elements (for mBA)
    pub reversed: bool,
    pub clut_table: Option<Vec<f32>>,
    pub a_curves: [Option<Box<curveType>>; MAX_CHANNELS],
    pub b_curves: [Option<Box<curveType>>; MAX_CHANNELS],
    pub m_curves: [Option<Box<curveType>>; MAX_CHANNELS],
}
#[derive(Clone, Debug)]
pub(crate) enum curveType {
    Curve(Vec<uInt16Number>), // len=0 => Linear, len=1 => Gamma(v[0]), _ => lut
    /// The ICC parametricCurveType is specified in terms of s15Fixed16Number,
    /// so it's possible to use this variant to specify greater precision than
    /// any raw ICC profile could
    Parametric(Vec<f32>),
}
type uInt16Number = u16;

/* should lut8Type and lut16Type be different types? */
#[derive(Debug)]
pub(crate) struct lutType {
    // used by lut8Type/lut16Type (mft2) only
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
    pub input_table: Vec<f32>,
    pub clut_table: Vec<f32>,
    pub output_table: Vec<f32>,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default)]
#[allow(clippy::upper_case_acronyms)]
pub struct XYZNumber {
    pub X: s15Fixed16Number,
    pub Y: s15Fixed16Number,
    pub Z: s15Fixed16Number,
}

/// A color in the CIE xyY color space
/* the names for the following two types are sort of ugly */
#[repr(C)]
#[derive(Copy, Clone)]
#[allow(clippy::upper_case_acronyms)]
pub struct qcms_CIE_xyY {
    pub x: f64,
    pub y: f64,
    pub Y: f64,
}

/// A more convenient type for specifying primaries and white points where
/// luminosity is irrelevant
struct qcms_chromaticity {
    x: f64,
    y: f64,
}

impl qcms_chromaticity {
    const D65: Self = Self {
        x: 0.3127,
        y: 0.3290,
    };
}

impl From<qcms_chromaticity> for qcms_CIE_xyY {
    fn from(qcms_chromaticity { x, y }: qcms_chromaticity) -> Self {
        Self { x, y, Y: 1.0 }
    }
}

/// a set of CIE_xyY values that can use to describe the primaries of a color space
#[repr(C)]
#[derive(Copy, Clone)]
#[allow(clippy::upper_case_acronyms)]
pub struct qcms_CIE_xyYTRIPLE {
    pub red: qcms_CIE_xyY,
    pub green: qcms_CIE_xyY,
    pub blue: qcms_CIE_xyY,
}

struct Tag {
    signature: u32,
    offset: u32,
    size: u32,
}

/* It might be worth having a unified limit on content controlled
 * allocation per profile. This would remove the need for many
 * of the arbitrary limits that we used */

type TagIndex = [Tag];

/* a wrapper around the memory that we are going to parse
 * into a qcms_profile */
struct MemSource<'a> {
    buf: &'a [u8],
    valid: bool,
    invalid_reason: Option<&'static str>,
}
pub type uInt8Number = u8;
#[inline]
fn uInt8Number_to_float(a: uInt8Number) -> f32 {
    a as f32 / 255.0
}

#[inline]
fn uInt16Number_to_float(a: uInt16Number) -> f32 {
    a as f32 / 65535.0
}

fn invalid_source(mem: &mut MemSource, reason: &'static str) {
    mem.valid = false;
    mem.invalid_reason = Some(reason);
}
fn read_u32(mem: &mut MemSource, offset: usize) -> u32 {
    let val = mem.buf.get(offset..offset + 4);
    if let Some(val) = val {
        let val = val.try_into().unwrap();
        u32::from_be_bytes(val)
    } else {
        invalid_source(mem, "Invalid offset");
        0
    }
}
fn read_u16(mem: &mut MemSource, offset: usize) -> u16 {
    let val = mem.buf.get(offset..offset + 2);
    if let Some(val) = val {
        let val = val.try_into().unwrap();
        u16::from_be_bytes(val)
    } else {
        invalid_source(mem, "Invalid offset");
        0
    }
}
fn read_u8(mem: &mut MemSource, offset: usize) -> u8 {
    let val = mem.buf.get(offset);
    if let Some(val) = val {
        *val
    } else {
        invalid_source(mem, "Invalid offset");
        0
    }
}
fn read_s15Fixed16Number(mem: &mut MemSource, offset: usize) -> s15Fixed16Number {
    read_u32(mem, offset) as s15Fixed16Number
}
fn read_uInt8Number(mem: &mut MemSource, offset: usize) -> uInt8Number {
    read_u8(mem, offset)
}
fn read_uInt16Number(mem: &mut MemSource, offset: usize) -> uInt16Number {
    read_u16(mem, offset)
}
pub fn write_u32(mem: &mut [u8], offset: usize, value: u32) {
    // we use get() and expect() instead of [..] so there's only one call to panic
    // instead of two
    mem.get_mut(offset..offset + std::mem::size_of_val(&value))
        .expect("OOB")
        .copy_from_slice(&value.to_be_bytes());
}
pub fn write_u16(mem: &mut [u8], offset: usize, value: u16) {
    // we use get() and expect() instead of [..] so there's only one call to panic
    // intead of two
    mem.get_mut(offset..offset + std::mem::size_of_val(&value))
        .expect("OOB")
        .copy_from_slice(&value.to_be_bytes());
}

/* An arbitrary 4MB limit on profile size */
pub(crate) const MAX_PROFILE_SIZE: usize = 1024 * 1024 * 4;
const MAX_TAG_COUNT: u32 = 1024;

fn check_CMM_type_signature(_src: &mut MemSource) {
    //uint32_t CMM_type_signature = read_u32(src, 4);
    //TODO: do the check?
}
fn check_profile_version(src: &mut MemSource) {
    /*
    uint8_t major_revision = read_u8(src, 8 + 0);
    uint8_t minor_revision = read_u8(src, 8 + 1);
    */
    let reserved1: u8 = read_u8(src, (8 + 2) as usize);
    let reserved2: u8 = read_u8(src, (8 + 3) as usize);
    /* Checking the version doesn't buy us anything
    if (major_revision != 0x4) {
        if (major_revision > 0x2)
            invalid_source(src, "Unsupported major revision");
        if (minor_revision > 0x40)
            invalid_source(src, "Unsupported minor revision");
    }
    */
    if reserved1 != 0 || reserved2 != 0 {
        invalid_source(src, "Invalid reserved bytes");
    };
}

const INPUT_DEVICE_PROFILE: u32 = 0x73636e72; // 'scnr'
pub const DISPLAY_DEVICE_PROFILE: u32 = 0x6d6e7472; // 'mntr'
const OUTPUT_DEVICE_PROFILE: u32 = 0x70727472; // 'prtr'
const DEVICE_LINK_PROFILE: u32 = 0x6c696e6b; // 'link'
const COLOR_SPACE_PROFILE: u32 = 0x73706163; // 'spac'
const ABSTRACT_PROFILE: u32 = 0x61627374; // 'abst'
const NAMED_COLOR_PROFILE: u32 = 0x6e6d636c; // 'nmcl'

fn read_class_signature(profile: &mut Profile, mem: &mut MemSource) {
    profile.class_type = read_u32(mem, 12);
    match profile.class_type {
        DISPLAY_DEVICE_PROFILE
        | INPUT_DEVICE_PROFILE
        | OUTPUT_DEVICE_PROFILE
        | COLOR_SPACE_PROFILE => {}
        _ => {
            invalid_source(mem, "Invalid  Profile/Device Class signature");
        }
    };
}
fn read_color_space(profile: &mut Profile, mem: &mut MemSource) {
    profile.color_space = read_u32(mem, 16);
    match profile.color_space {
        RGB_SIGNATURE | GRAY_SIGNATURE => {}
        #[cfg(feature = "cmyk")]
        CMYK_SIGNATURE => {}
        _ => {
            invalid_source(mem, "Unsupported colorspace");
        }
    };
}
fn read_pcs(profile: &mut Profile, mem: &mut MemSource) {
    profile.pcs = read_u32(mem, 20);
    match profile.pcs {
        XYZ_SIGNATURE | LAB_SIGNATURE => {}
        _ => {
            invalid_source(mem, "Unsupported pcs");
        }
    };
}
fn read_tag_table(_profile: &mut Profile, mem: &mut MemSource) -> Vec<Tag> {
    let count = read_u32(mem, 128);
    if count > MAX_TAG_COUNT {
        invalid_source(mem, "max number of tags exceeded");
        return Vec::new();
    }
    let mut index = Vec::with_capacity(count as usize);
    for i in 0..count {
        let tag_start = (128 + 4 + 4 * i * 3) as usize;
        let offset = read_u32(mem, tag_start + 4);
        if offset as usize > mem.buf.len() {
            invalid_source(mem, "tag points beyond the end of the buffer");
        }
        index.push(Tag {
            signature: read_u32(mem, tag_start),
            offset,
            size: read_u32(mem, tag_start + 8),
        });
    }

    index
}

/// Checks a profile for obvious inconsistencies and returns
/// true if the profile looks bogus and should probably be
/// ignored.
#[no_mangle]
pub extern "C" fn qcms_profile_is_bogus(profile: &mut Profile) -> bool {
    let mut sum: [f32; 3] = [0.; 3];
    let mut target: [f32; 3] = [0.; 3];
    let mut tolerance: [f32; 3] = [0.; 3];
    let rX: f32;
    let rY: f32;
    let rZ: f32;
    let gX: f32;
    let gY: f32;
    let gZ: f32;
    let bX: f32;
    let bY: f32;
    let bZ: f32;
    let negative: bool;
    let mut i: u32;
    // We currently only check the bogosity of RGB profiles
    if profile.color_space != RGB_SIGNATURE {
        return false;
    }
    if profile.A2B0.is_some()
        || profile.B2A0.is_some()
        || profile.mAB.is_some()
        || profile.mBA.is_some()
    {
        return false;
    }
    rX = s15Fixed16Number_to_float(profile.redColorant.X);
    rY = s15Fixed16Number_to_float(profile.redColorant.Y);
    rZ = s15Fixed16Number_to_float(profile.redColorant.Z);
    gX = s15Fixed16Number_to_float(profile.greenColorant.X);
    gY = s15Fixed16Number_to_float(profile.greenColorant.Y);
    gZ = s15Fixed16Number_to_float(profile.greenColorant.Z);
    bX = s15Fixed16Number_to_float(profile.blueColorant.X);
    bY = s15Fixed16Number_to_float(profile.blueColorant.Y);
    bZ = s15Fixed16Number_to_float(profile.blueColorant.Z);
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
        i += 1
    }
    if false {
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
        // Also: https://bugzil.la/1799391 and https://bugzil.la/1792469
        negative = false; // bogus
    }
    if negative {
        return true;
    }
    // All Good
    false
}

pub const TAG_bXYZ: u32 = 0x6258595a;
pub const TAG_gXYZ: u32 = 0x6758595a;
pub const TAG_rXYZ: u32 = 0x7258595a;
pub const TAG_rTRC: u32 = 0x72545243;
pub const TAG_bTRC: u32 = 0x62545243;
pub const TAG_gTRC: u32 = 0x67545243;
pub const TAG_kTRC: u32 = 0x6b545243;
pub const TAG_A2B0: u32 = 0x41324230;
pub const TAG_B2A0: u32 = 0x42324130;
pub const TAG_CHAD: u32 = 0x63686164;

fn find_tag(index: &TagIndex, tag_id: u32) -> Option<&Tag> {
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

fn read_tag_s15Fixed16ArrayType(src: &mut MemSource, tag: &Tag) -> Matrix {
    let mut matrix: Matrix = Matrix { m: [[0.; 3]; 3] };
    let offset: u32 = tag.offset;
    let type_0: u32 = read_u32(src, offset as usize);
    // Check mandatory type signature for s16Fixed16ArrayType
    if type_0 != CHROMATIC_TYPE {
        invalid_source(src, "unexpected type, expected \'sf32\'");
    }
    for i in 0..=8 {
        matrix.m[(i / 3) as usize][(i % 3) as usize] = s15Fixed16Number_to_float(
            read_s15Fixed16Number(src, (offset + 8 + (i * 4) as u32) as usize),
        );
    }
    matrix
}
fn read_tag_XYZType(src: &mut MemSource, index: &TagIndex, tag_id: u32) -> XYZNumber {
    let mut num = XYZNumber { X: 0, Y: 0, Z: 0 };
    let tag = find_tag(&index, tag_id);
    if let Some(tag) = tag {
        let offset: u32 = tag.offset;
        let type_0: u32 = read_u32(src, offset as usize);
        if type_0 != XYZ_TYPE {
            invalid_source(src, "unexpected type, expected XYZ");
        }
        num.X = read_s15Fixed16Number(src, (offset + 8) as usize);
        num.Y = read_s15Fixed16Number(src, (offset + 12) as usize);
        num.Z = read_s15Fixed16Number(src, (offset + 16) as usize)
    } else {
        invalid_source(src, "missing xyztag");
    }
    num
}
// Read the tag at a given offset rather then the tag_index.
// This method is used when reading mAB tags where nested curveType are
// present that are not part of the tag_index.
fn read_curveType(src: &mut MemSource, offset: u32, len: &mut u32) -> Option<Box<curveType>> {
    const COUNT_TO_LENGTH: [u32; 5] = [1, 3, 4, 5, 7]; //PARAMETRIC_CURVE_TYPE
    let type_0: u32 = read_u32(src, offset as usize);
    let count: u32;
    if type_0 != CURVE_TYPE && type_0 != PARAMETRIC_CURVE_TYPE {
        invalid_source(src, "unexpected type, expected CURV or PARA");
        return None;
    }
    if type_0 == CURVE_TYPE {
        count = read_u32(src, (offset + 8) as usize);
        //arbitrary
        if count > 40000 {
            invalid_source(src, "curve size too large");
            return None;
        }
        let mut table = Vec::with_capacity(count as usize);
        for i in 0..count {
            table.push(read_u16(src, (offset + 12 + i * 2) as usize));
        }
        *len = 12 + count * 2;
        Some(Box::new(curveType::Curve(table)))
    } else {
        count = read_u16(src, (offset + 8) as usize) as u32;
        if count > 4 {
            invalid_source(src, "parametric function type not supported.");
            return None;
        }
        let mut params = Vec::with_capacity(count as usize);
        for i in 0..COUNT_TO_LENGTH[count as usize] {
            params.push(s15Fixed16Number_to_float(read_s15Fixed16Number(
                src,
                (offset + 12 + i * 4) as usize,
            )));
        }
        *len = 12 + COUNT_TO_LENGTH[count as usize] * 4;
        if count == 1 || count == 2 {
            /* we have a type 1 or type 2 function that has a division by 'a' */
            let a: f32 = params[1];
            if a == 0.0 {
                invalid_source(src, "parametricCurve definition causes division by zero");
            }
        }
        Some(Box::new(curveType::Parametric(params)))
    }
}
fn read_tag_curveType(
    src: &mut MemSource,
    index: &TagIndex,
    tag_id: u32,
) -> Option<Box<curveType>> {
    let tag = find_tag(index, tag_id);
    if let Some(tag) = tag {
        let mut len: u32 = 0;
        return read_curveType(src, tag.offset, &mut len);
    } else {
        invalid_source(src, "missing curvetag");
    }
    None
}

const MAX_LUT_SIZE: u32 = 500000; // arbitrary
const MAX_CHANNELS: usize = 10; // arbitrary
fn read_nested_curveType(
    src: &mut MemSource,
    curveArray: &mut [Option<Box<curveType>>; MAX_CHANNELS],
    num_channels: u8,
    curve_offset: u32,
) {
    let mut channel_offset: u32 = 0;
    #[allow(clippy::needless_range_loop)]
    for i in 0..usize::from(num_channels) {
        let mut tag_len: u32 = 0;
        curveArray[i] = read_curveType(src, curve_offset + channel_offset, &mut tag_len);
        if curveArray[i].is_none() {
            invalid_source(src, "invalid nested curveType curve");
            break;
        } else {
            channel_offset += tag_len;
            // 4 byte aligned
            if tag_len % 4 != 0 {
                channel_offset += 4 - tag_len % 4
            }
        }
    }
}

/* See section 10.10 for specs */
fn read_tag_lutmABType(src: &mut MemSource, tag: &Tag) -> Option<Box<lutmABType>> {
    let offset: u32 = tag.offset;
    let mut clut_size: u32 = 1;
    let type_0: u32 = read_u32(src, offset as usize);
    if type_0 != LUT_MAB_TYPE && type_0 != LUT_MBA_TYPE {
        return None;
    }
    let num_in_channels = read_u8(src, (offset + 8) as usize);
    let num_out_channels = read_u8(src, (offset + 9) as usize);
    if num_in_channels > 10 || num_out_channels > 10 {
        return None;
    }
    // We require 3in/out channels since we only support RGB->XYZ (or RGB->LAB)
    // XXX: If we remove this restriction make sure that the number of channels
    //      is less or equal to the maximum number of mAB curves in qcmsint.h
    //      also check for clut_size overflow. Also make sure it's != 0
    if num_in_channels != 3 || num_out_channels != 3 {
        return None;
    }
    // some of this data is optional and is denoted by a zero offset
    // we also use this to track their existance
    let mut a_curve_offset = read_u32(src, (offset + 28) as usize);
    let mut clut_offset = read_u32(src, (offset + 24) as usize);
    let mut m_curve_offset = read_u32(src, (offset + 20) as usize);
    let mut matrix_offset = read_u32(src, (offset + 16) as usize);
    let mut b_curve_offset = read_u32(src, (offset + 12) as usize);
    // Convert offsets relative to the tag to relative to the profile
    // preserve zero for optional fields
    if a_curve_offset != 0 {
        a_curve_offset += offset
    }
    if clut_offset != 0 {
        clut_offset += offset
    }
    if m_curve_offset != 0 {
        m_curve_offset += offset
    }
    if matrix_offset != 0 {
        matrix_offset += offset
    }
    if b_curve_offset != 0 {
        b_curve_offset += offset
    }
    if clut_offset != 0 {
        debug_assert!(num_in_channels == 3);
        // clut_size can not overflow since lg(256^num_in_channels) = 24 bits.
        for i in 0..u32::from(num_in_channels) {
            clut_size *= read_u8(src, (clut_offset + i) as usize) as u32;
            if clut_size == 0 {
                invalid_source(src, "bad clut_size");
            }
        }
    } else {
        clut_size = 0
    }
    // 24bits * 3 won't overflow either
    clut_size *= num_out_channels as u32;
    if clut_size > MAX_LUT_SIZE {
        return None;
    }

    let mut lut = Box::new(lutmABType::default());

    if clut_offset != 0 {
        for i in 0..usize::from(num_in_channels) {
            lut.num_grid_points[i] = read_u8(src, clut_offset as usize + i);
            if lut.num_grid_points[i] == 0 {
                invalid_source(src, "bad grid_points");
            }
        }
    }
    // Reverse the processing of transformation elements for mBA type.
    lut.reversed = type_0 == LUT_MBA_TYPE;
    lut.num_in_channels = num_in_channels;
    lut.num_out_channels = num_out_channels;
    #[allow(clippy::identity_op, clippy::erasing_op)]
    if matrix_offset != 0 {
        // read the matrix if we have it
        lut.e00 = read_s15Fixed16Number(src, (matrix_offset + (4 * 0) as u32) as usize); // the caller checks that this doesn't happen
        lut.e01 = read_s15Fixed16Number(src, (matrix_offset + (4 * 1) as u32) as usize);
        lut.e02 = read_s15Fixed16Number(src, (matrix_offset + (4 * 2) as u32) as usize);
        lut.e10 = read_s15Fixed16Number(src, (matrix_offset + (4 * 3) as u32) as usize);
        lut.e11 = read_s15Fixed16Number(src, (matrix_offset + (4 * 4) as u32) as usize);
        lut.e12 = read_s15Fixed16Number(src, (matrix_offset + (4 * 5) as u32) as usize);
        lut.e20 = read_s15Fixed16Number(src, (matrix_offset + (4 * 6) as u32) as usize);
        lut.e21 = read_s15Fixed16Number(src, (matrix_offset + (4 * 7) as u32) as usize);
        lut.e22 = read_s15Fixed16Number(src, (matrix_offset + (4 * 8) as u32) as usize);
        lut.e03 = read_s15Fixed16Number(src, (matrix_offset + (4 * 9) as u32) as usize);
        lut.e13 = read_s15Fixed16Number(src, (matrix_offset + (4 * 10) as u32) as usize);
        lut.e23 = read_s15Fixed16Number(src, (matrix_offset + (4 * 11) as u32) as usize)
    }
    if a_curve_offset != 0 {
        read_nested_curveType(src, &mut lut.a_curves, num_in_channels, a_curve_offset);
    }
    if m_curve_offset != 0 {
        read_nested_curveType(src, &mut lut.m_curves, num_out_channels, m_curve_offset);
    }
    if b_curve_offset != 0 {
        read_nested_curveType(src, &mut lut.b_curves, num_out_channels, b_curve_offset);
    } else {
        invalid_source(src, "B curves required");
    }
    if clut_offset != 0 {
        let clut_precision = read_u8(src, (clut_offset + 16) as usize);
        let mut clut_table = Vec::with_capacity(clut_size as usize);
        if clut_precision == 1 {
            for i in 0..clut_size {
                clut_table.push(uInt8Number_to_float(read_uInt8Number(
                    src,
                    (clut_offset + 20 + i) as usize,
                )));
            }
            lut.clut_table = Some(clut_table);
        } else if clut_precision == 2 {
            for i in 0..clut_size {
                clut_table.push(uInt16Number_to_float(read_uInt16Number(
                    src,
                    (clut_offset + 20 + i * 2) as usize,
                )));
            }
            lut.clut_table = Some(clut_table);
        } else {
            invalid_source(src, "Invalid clut precision");
        }
    }
    if !src.valid {
        return None;
    }
    Some(lut)
}
fn read_tag_lutType(src: &mut MemSource, tag: &Tag) -> Option<Box<lutType>> {
    let offset: u32 = tag.offset;
    let type_0: u32 = read_u32(src, offset as usize);
    let num_input_table_entries: u16;
    let num_output_table_entries: u16;
    let input_offset: u32;
    let entry_size: usize;
    if type_0 == LUT8_TYPE {
        num_input_table_entries = 256u16;
        num_output_table_entries = 256u16;
        entry_size = 1;
        input_offset = 48
    } else if type_0 == LUT16_TYPE {
        num_input_table_entries = read_u16(src, (offset + 48) as usize);
        num_output_table_entries = read_u16(src, (offset + 50) as usize);

        // these limits come from the spec
        if !(2..=4096).contains(&num_input_table_entries)
            || !(2..=4096).contains(&num_output_table_entries)
        {
            invalid_source(src, "Bad channel count");
            return None;
        }
        entry_size = 2;
        input_offset = 52
    } else {
        debug_assert!(false);
        invalid_source(src, "Unexpected lut type");
        return None;
    }
    let in_chan = read_u8(src, (offset + 8) as usize);
    let out_chan = read_u8(src, (offset + 9) as usize);
    if !(in_chan == 3 || in_chan == 4) || out_chan != 3 {
        invalid_source(src, "CLUT only supports RGB and CMYK");
        return None;
    }

    let grid_points = read_u8(src, (offset + 10) as usize);
    let clut_size = match (grid_points as u32).checked_pow(in_chan as u32) {
        Some(clut_size) => clut_size,
        _ => {
            invalid_source(src, "CLUT size overflow");
            return None;
        }
    };
    match clut_size {
        1..=MAX_LUT_SIZE => {} // OK
        0 => {
            invalid_source(src, "CLUT must not be empty.");
            return None;
        }
        _ => {
            invalid_source(src, "CLUT too large");
            return None;
        }
    }

    let e00 = read_s15Fixed16Number(src, (offset + 12) as usize);
    let e01 = read_s15Fixed16Number(src, (offset + 16) as usize);
    let e02 = read_s15Fixed16Number(src, (offset + 20) as usize);
    let e10 = read_s15Fixed16Number(src, (offset + 24) as usize);
    let e11 = read_s15Fixed16Number(src, (offset + 28) as usize);
    let e12 = read_s15Fixed16Number(src, (offset + 32) as usize);
    let e20 = read_s15Fixed16Number(src, (offset + 36) as usize);
    let e21 = read_s15Fixed16Number(src, (offset + 40) as usize);
    let e22 = read_s15Fixed16Number(src, (offset + 44) as usize);

    let mut input_table = Vec::with_capacity((num_input_table_entries * in_chan as u16) as usize);
    for i in 0..(num_input_table_entries * in_chan as u16) {
        if type_0 == LUT8_TYPE {
            input_table.push(uInt8Number_to_float(read_uInt8Number(
                src,
                (offset + input_offset) as usize + i as usize * entry_size,
            )))
        } else {
            input_table.push(uInt16Number_to_float(read_uInt16Number(
                src,
                (offset + input_offset) as usize + i as usize * entry_size,
            )))
        }
    }
    let clut_offset = ((offset + input_offset) as usize
        + (num_input_table_entries as i32 * in_chan as i32) as usize * entry_size)
        as u32;

    let mut clut_table = Vec::with_capacity((clut_size * out_chan as u32) as usize);
    for i in 0..clut_size * out_chan as u32 {
        if type_0 == LUT8_TYPE {
            clut_table.push(uInt8Number_to_float(read_uInt8Number(
                src,
                clut_offset as usize + i as usize * entry_size,
            )));
        } else if type_0 == LUT16_TYPE {
            clut_table.push(uInt16Number_to_float(read_uInt16Number(
                src,
                clut_offset as usize + i as usize * entry_size,
            )));
        }
    }

    let output_offset =
        (clut_offset as usize + (clut_size * out_chan as u32) as usize * entry_size) as u32;

    let mut output_table =
        Vec::with_capacity((num_output_table_entries * out_chan as u16) as usize);
    for i in 0..num_output_table_entries as i32 * out_chan as i32 {
        if type_0 == LUT8_TYPE {
            output_table.push(uInt8Number_to_float(read_uInt8Number(
                src,
                output_offset as usize + i as usize * entry_size,
            )))
        } else {
            output_table.push(uInt16Number_to_float(read_uInt16Number(
                src,
                output_offset as usize + i as usize * entry_size,
            )))
        }
    }
    Some(Box::new(lutType {
        num_input_table_entries,
        num_output_table_entries,
        num_input_channels: in_chan,
        num_output_channels: out_chan,
        num_clut_grid_points: grid_points,
        e00,
        e01,
        e02,
        e10,
        e11,
        e12,
        e20,
        e21,
        e22,
        input_table,
        clut_table,
        output_table,
    }))
}
fn read_rendering_intent(profile: &mut Profile, src: &mut MemSource) {
    let intent = read_u32(src, 64);
    profile.rendering_intent = match intent {
        x if x == Perceptual as u32 => Perceptual,
        x if x == RelativeColorimetric as u32 => RelativeColorimetric,
        x if x == Saturation as u32 => Saturation,
        x if x == AbsoluteColorimetric as u32 => AbsoluteColorimetric,
        _ => {
            invalid_source(src, "unknown rendering intent");
            Intent::default()
        }
    };
}
fn profile_create() -> Box<Profile> {
    Box::new(Profile::default())
}
/* build sRGB gamma table */
/* based on cmsBuildParametricGamma() */
#[allow(clippy::many_single_char_names)]
fn build_sRGB_gamma_table(num_entries: i32) -> Vec<u16> {
    /* taken from lcms: Build_sRGBGamma() */
    let gamma: f64 = 2.4;
    let a: f64 = 1.0 / 1.055;
    let b: f64 = 0.055 / 1.055;
    let c: f64 = 1.0 / 12.92;
    let d: f64 = 0.04045;

    build_trc_table(
        num_entries,
        // IEC 61966-2.1 (sRGB)
        // Y = (aX + b)^Gamma | X >= d
        // Y = cX             | X < d
        |x| {
            if x >= d {
                let e: f64 = a * x + b;
                if e > 0. {
                    e.powf(gamma)
                } else {
                    0.
                }
            } else {
                c * x
            }
        },
    )
}

/// eotf: electro-optical transfer characteristic function, maps from [0, 1]
/// in non-linear (voltage) space to [0, 1] in linear (optical) space. Should
/// generally be a concave up function.
fn build_trc_table(num_entries: i32, eotf: impl Fn(f64) -> f64) -> Vec<u16> {
    let mut table = Vec::with_capacity(num_entries as usize);

    for i in 0..num_entries {
        let x: f64 = i as f64 / (num_entries - 1) as f64;
        let y: f64 = eotf(x);
        let mut output: f64;
        // Saturate -- this could likely move to a separate function
        output = y * 65535.0 + 0.5;
        if output > 65535.0 {
            output = 65535.0
        }
        if output < 0.0 {
            output = 0.0
        }
        table.push(output.floor() as u16);
    }
    table
}
fn curve_from_table(table: &[u16]) -> Box<curveType> {
    Box::new(curveType::Curve(table.to_vec()))
}
pub fn float_to_u8Fixed8Number(a: f32) -> u16 {
    if a > 255.0 + 255.0 / 256f32 {
        0xffffu16
    } else if a < 0.0 {
        0u16
    } else {
        (a * 256.0 + 0.5).floor() as u16
    }
}

fn curve_from_gamma(gamma: f32) -> Box<curveType> {
    Box::new(curveType::Curve(vec![float_to_u8Fixed8Number(gamma)]))
}

fn identity_curve() -> Box<curveType> {
    Box::new(curveType::Curve(Vec::new()))
}

/* from lcms: cmsWhitePointFromTemp */
/* tempK must be >= 4000. and <= 25000.
 * Invalid values of tempK will return
 * (x,y,Y) = (-1.0, -1.0, -1.0)
 * similar to argyll: icx_DTEMP2XYZ() */
fn white_point_from_temp(temp_K: i32) -> qcms_CIE_xyY {
    let mut white_point: qcms_CIE_xyY = qcms_CIE_xyY {
        x: 0.,
        y: 0.,
        Y: 0.,
    };
    // No optimization provided.
    let T = temp_K as f64; // Square
    let T2 = T * T; // Cube
    let T3 = T2 * T;
    // For correlated color temperature (T) between 4000K and 7000K:
    let x = if (4000.0..=7000.0).contains(&T) {
        -4.6070 * (1E9 / T3) + 2.9678 * (1E6 / T2) + 0.09911 * (1E3 / T) + 0.244063
    } else if T > 7000.0 && T <= 25000.0 {
        -2.0064 * (1E9 / T3) + 1.9018 * (1E6 / T2) + 0.24748 * (1E3 / T) + 0.237040
    } else {
        // or for correlated color temperature (T) between 7000K and 25000K:
        // Invalid tempK
        white_point.x = -1.0;
        white_point.y = -1.0;
        white_point.Y = -1.0;
        debug_assert!(false, "invalid temp");
        return white_point;
    };
    // Obtain y(x)
    let y = -3.000 * (x * x) + 2.870 * x - 0.275;
    // wave factors (not used, but here for futures extensions)
    // let M1 = (-1.3515 - 1.7703*x + 5.9114 *y)/(0.0241 + 0.2562*x - 0.7341*y);
    // let M2 = (0.0300 - 31.4424*x + 30.0717*y)/(0.0241 + 0.2562*x - 0.7341*y);
    // Fill white_point struct
    white_point.x = x;
    white_point.y = y;
    white_point.Y = 1.0;
    white_point
}
#[no_mangle]
pub extern "C" fn qcms_white_point_sRGB() -> qcms_CIE_xyY {
    white_point_from_temp(6504)
}

/// See [Rec. ITU-T H.273 (12/2016)](https://www.itu.int/rec/T-REC-H.273-201612-I/en) Table 2
/// Values 0, 3, 13–21, 23–255 are all reserved so all map to the same variant
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ColourPrimaries {
    /// For future use by ITU-T | ISO/IEC
    Reserved,
    /// Rec. ITU-R BT.709-6<br />
    /// Rec. ITU-R BT.1361-0 conventional colour gamut system and extended colour gamut system (historical)<br />
    /// IEC 61966-2-1 sRGB or sYCC IEC 61966-2-4<br />
    /// Society of Motion Picture and Television Engineers (MPTE) RP 177 (1993) Annex B<br />
    Bt709 = 1,
    /// Unspecified<br />
    /// Image characteristics are unknown or are determined by the application.
    Unspecified = 2,
    /// Rec. ITU-R BT.470-6 System M (historical)<br />
    /// United States National Television System Committee 1953 Recommendation for transmission standards for color television<br />
    /// United States Federal Communications Commission (2003) Title 47 Code of Federal Regulations 73.682 (a) (20)<br />
    Bt470M = 4,
    /// Rec. ITU-R BT.470-6 System B, G (historical) Rec. ITU-R BT.601-7 625<br />
    /// Rec. ITU-R BT.1358-0 625 (historical)<br />
    /// Rec. ITU-R BT.1700-0 625 PAL and 625 SECAM<br />
    Bt470Bg = 5,
    /// Rec. ITU-R BT.601-7 525<br />
    /// Rec. ITU-R BT.1358-1 525 or 625 (historical) Rec. ITU-R BT.1700-0 NTSC<br />
    /// SMPTE 170M (2004)<br />
    /// (functionally the same as the value 7)<br />
    Bt601 = 6,
    /// SMPTE 240M (1999) (historical) (functionally the same as the value 6)<br />
    Smpte240 = 7,
    /// Generic film (colour filters using Illuminant C)<br />
    Generic_film = 8,
    /// Rec. ITU-R BT.2020-2<br />
    /// Rec. ITU-R BT.2100-0<br />
    Bt2020 = 9,
    /// SMPTE ST 428-1<br />
    /// (CIE 1931 XYZ as in ISO 11664-1)<br />
    Xyz = 10,
    /// SMPTE RP 431-2 (2011)<br />
    Smpte431 = 11,
    /// SMPTE EG 432-1 (2010)<br />
    Smpte432 = 12,
    /// EBU Tech. 3213-E (1975)<br />
    Ebu3213 = 22,
}

impl From<u8> for ColourPrimaries {
    fn from(value: u8) -> Self {
        match value {
            0 | 3 | 13..=21 | 23..=255 => Self::Reserved,
            1 => Self::Bt709,
            2 => Self::Unspecified,
            4 => Self::Bt470M,
            5 => Self::Bt470Bg,
            6 => Self::Bt601,
            7 => Self::Smpte240,
            8 => Self::Generic_film,
            9 => Self::Bt2020,
            10 => Self::Xyz,
            11 => Self::Smpte431,
            12 => Self::Smpte432,
            22 => Self::Ebu3213,
        }
    }
}

#[test]
fn colour_primaries() {
    for value in 0..=u8::MAX {
        match ColourPrimaries::from(value) {
            ColourPrimaries::Reserved => {}
            variant => assert_eq!(value, variant as u8),
        }
    }
}

impl From<ColourPrimaries> for qcms_CIE_xyYTRIPLE {
    fn from(value: ColourPrimaries) -> Self {
        let red;
        let green;
        let blue;

        match value {
            ColourPrimaries::Reserved => panic!("CP={} is reserved", value as u8),
            ColourPrimaries::Bt709 => {
                green = qcms_chromaticity { x: 0.300, y: 0.600 };
                blue = qcms_chromaticity { x: 0.150, y: 0.060 };
                red = qcms_chromaticity { x: 0.640, y: 0.330 };
            }
            ColourPrimaries::Unspecified => panic!("CP={} is unspecified", value as u8),
            ColourPrimaries::Bt470M => {
                green = qcms_chromaticity { x: 0.21, y: 0.71 };
                blue = qcms_chromaticity { x: 0.14, y: 0.08 };
                red = qcms_chromaticity { x: 0.67, y: 0.33 };
            }
            ColourPrimaries::Bt470Bg => {
                green = qcms_chromaticity { x: 0.29, y: 0.60 };
                blue = qcms_chromaticity { x: 0.15, y: 0.06 };
                red = qcms_chromaticity { x: 0.64, y: 0.33 };
            }
            ColourPrimaries::Bt601 | ColourPrimaries::Smpte240 => {
                green = qcms_chromaticity { x: 0.310, y: 0.595 };
                blue = qcms_chromaticity { x: 0.155, y: 0.070 };
                red = qcms_chromaticity { x: 0.630, y: 0.340 };
            }
            ColourPrimaries::Generic_film => {
                green = qcms_chromaticity { x: 0.243, y: 0.692 };
                blue = qcms_chromaticity { x: 0.145, y: 0.049 };
                red = qcms_chromaticity { x: 0.681, y: 0.319 };
            }
            ColourPrimaries::Bt2020 => {
                green = qcms_chromaticity { x: 0.170, y: 0.797 };
                blue = qcms_chromaticity { x: 0.131, y: 0.046 };
                red = qcms_chromaticity { x: 0.708, y: 0.292 };
            }
            ColourPrimaries::Xyz => {
                green = qcms_chromaticity { x: 0.0, y: 1.0 };
                blue = qcms_chromaticity { x: 0.0, y: 0.0 };
                red = qcms_chromaticity { x: 1.0, y: 0.0 };
            }
            // These two share primaries, but have distinct white points
            ColourPrimaries::Smpte431 | ColourPrimaries::Smpte432 => {
                green = qcms_chromaticity { x: 0.265, y: 0.690 };
                blue = qcms_chromaticity { x: 0.150, y: 0.060 };
                red = qcms_chromaticity { x: 0.680, y: 0.320 };
            }
            ColourPrimaries::Ebu3213 => {
                green = qcms_chromaticity { x: 0.295, y: 0.605 };
                blue = qcms_chromaticity { x: 0.155, y: 0.077 };
                red = qcms_chromaticity { x: 0.630, y: 0.340 };
            }
        }

        Self {
            red: red.into(),
            green: green.into(),
            blue: blue.into(),
        }
    }
}

impl ColourPrimaries {
    fn white_point(self) -> qcms_CIE_xyY {
        match self {
            Self::Reserved => panic!("CP={} is reserved", self as u8),
            Self::Bt709
            | Self::Bt470Bg
            | Self::Bt601
            | Self::Smpte240
            | Self::Bt2020
            | Self::Smpte432
            | Self::Ebu3213 => qcms_chromaticity::D65,
            Self::Unspecified => panic!("CP={} is unspecified", self as u8),
            Self::Bt470M => qcms_chromaticity { x: 0.310, y: 0.316 },
            Self::Generic_film => qcms_chromaticity { x: 0.310, y: 0.316 },
            Self::Xyz => qcms_chromaticity {
                x: 1. / 3.,
                y: 1. / 3.,
            },
            Self::Smpte431 => qcms_chromaticity { x: 0.314, y: 0.351 },
        }
        .into()
    }
}

/// See [Rec. ITU-T H.273 (12/2016)](https://www.itu.int/rec/T-REC-H.273-201612-I/en) Table 3
/// Values 0, 3, 19–255 are all reserved so all map to the same variant
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum TransferCharacteristics {
    /// For future use by ITU-T | ISO/IEC
    Reserved,
    /// Rec. ITU-R BT.709-6<br />
    /// Rec. ITU-R BT.1361-0 conventional colour gamut system (historical)<br />
    /// (functionally the same as the values 6, 14 and 15)    <br />
    Bt709 = 1,
    /// Image characteristics are unknown or are determined by the application.<br />
    Unspecified = 2,
    /// Rec. ITU-R BT.470-6 System M (historical)<br />
    /// United States National Television System Committee 1953 Recommendation for transmission standards for color television<br />
    /// United States Federal Communications Commission (2003) Title 47 Code of Federal Regulations 73.682 (a) (20)<br />
    /// Rec. ITU-R BT.1700-0 625 PAL and 625 SECAM<br />
    Bt470M = 4,
    /// Rec. ITU-R BT.470-6 System B, G (historical)<br />
    Bt470Bg = 5,
    /// Rec. ITU-R BT.601-7 525 or 625<br />
    /// Rec. ITU-R BT.1358-1 525 or 625 (historical)<br />
    /// Rec. ITU-R BT.1700-0 NTSC SMPTE 170M (2004)<br />
    /// (functionally the same as the values 1, 14 and 15)<br />
    Bt601 = 6,
    /// SMPTE 240M (1999) (historical)<br />
    Smpte240 = 7,
    /// Linear transfer characteristics<br />
    Linear = 8,
    /// Logarithmic transfer characteristic (100:1 range)<br />
    Log_100 = 9,
    /// Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)<br />
    Log_100_sqrt10 = 10,
    /// IEC 61966-2-4<br />
    Iec61966 = 11,
    /// Rec. ITU-R BT.1361-0 extended colour gamut system (historical)<br />
    Bt_1361 = 12,
    /// IEC 61966-2-1 sRGB or sYCC<br />
    Srgb = 13,
    /// Rec. ITU-R BT.2020-2 (10-bit system)<br />
    /// (functionally the same as the values 1, 6 and 15)<br />
    Bt2020_10bit = 14,
    /// Rec. ITU-R BT.2020-2 (12-bit system)<br />
    /// (functionally the same as the values 1, 6 and 14)<br />
    Bt2020_12bit = 15,
    /// SMPTE ST 2084 for 10-, 12-, 14- and 16-bitsystems<br />
    /// Rec. ITU-R BT.2100-0 perceptual quantization (PQ) system<br />
    Smpte2084 = 16,
    /// SMPTE ST 428-1<br />
    Smpte428 = 17,
    /// ARIB STD-B67<br />
    /// Rec. ITU-R BT.2100-0 hybrid log- gamma (HLG) system<br />
    Hlg = 18,
}

#[test]
fn transfer_characteristics() {
    for value in 0..=u8::MAX {
        match TransferCharacteristics::from(value) {
            TransferCharacteristics::Reserved => {}
            variant => assert_eq!(value, variant as u8),
        }
    }
}

impl From<u8> for TransferCharacteristics {
    fn from(value: u8) -> Self {
        match value {
            0 | 3 | 19..=255 => Self::Reserved,
            1 => Self::Bt709,
            2 => Self::Unspecified,
            4 => Self::Bt470M,
            5 => Self::Bt470Bg,
            6 => Self::Bt601,
            7 => Self::Smpte240, // unimplemented
            8 => Self::Linear,
            9 => Self::Log_100,
            10 => Self::Log_100_sqrt10,
            11 => Self::Iec61966, // unimplemented
            12 => Self::Bt_1361,  // unimplemented
            13 => Self::Srgb,
            14 => Self::Bt2020_10bit,
            15 => Self::Bt2020_12bit,
            16 => Self::Smpte2084,
            17 => Self::Smpte428, // unimplemented
            18 => Self::Hlg,
        }
    }
}

impl TryFrom<TransferCharacteristics> for curveType {
    type Error = ();
    /// See [ICC.1:2010](https://www.color.org/specification/ICC1v43_2010-12.pdf)
    /// See [Rec. ITU-R BT.2100-2](https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.2100-2-201807-I!!PDF-E.pdf)
    fn try_from(value: TransferCharacteristics) -> Result<Self, Self::Error> {
        const NUM_TRC_TABLE_ENTRIES: i32 = 1024;

        Ok(match value {
            TransferCharacteristics::Reserved => panic!("TC={} is reserved", value as u8),
            TransferCharacteristics::Bt709
            | TransferCharacteristics::Bt601
            | TransferCharacteristics::Bt2020_10bit
            | TransferCharacteristics::Bt2020_12bit => {
                // The opto-electronic transfer characteristic function (OETF)
                // as defined in ITU-T H.273 table 3, row 1:
                //
                // V = (α * Lc^0.45) − (α − 1)  for 1 >= Lc >= β
                // V = 4.500 * Lc               for β >  Lc >= 0
                //
                // Inverting gives the electro-optical transfer characteristic
                // function (EOTF) which can be represented as ICC
                // parametricCurveType with 4 parameters (ICC.1:2010 Table 5).
                // Converting between the two (Lc ↔︎ Y, V ↔︎ X):
                //
                // Y = (a * X + b)^g  for (X >= d)
                // Y = c * X          for (X < d)
                //
                // g, a, b, c, d can then be defined in terms of α and β:
                //
                // g = 1 / 0.45
                // a = 1 / α
                // b = 1 - α
                // c = 1 / 4.500
                // d = 4.500 * β
                //
                // α and β are determined by solving the piecewise equations to
                // ensure continuity of both value and slope at the value β.
                // We use the values specified for 10-bit systems in
                // https://www.itu.int/rec/R-REC-BT.2020-2-201510-I Table 4
                // since this results in the similar values as available ICC
                // profiles after converting to s15Fixed16Number, providing us
                // good test coverage.

                type Float = f32;

                const alpha: Float = 1.099;
                const beta: Float = 0.018;

                const linear_coef: Float = 4.500;
                const pow_exp: Float = 0.45;

                const g: Float = 1. / pow_exp;
                const a: Float = 1. / alpha;
                const b: Float = 1. - a;
                const c: Float = 1. / linear_coef;
                const d: Float = linear_coef * beta;

                curveType::Parametric(vec![g, a, b, c, d])
            }
            TransferCharacteristics::Unspecified => panic!("TC={} is unspecified", value as u8),
            TransferCharacteristics::Bt470M => *curve_from_gamma(2.2),
            TransferCharacteristics::Bt470Bg => *curve_from_gamma(2.8),
            TransferCharacteristics::Smpte240 => return Err(()),
            TransferCharacteristics::Linear => *curve_from_gamma(1.),
            TransferCharacteristics::Log_100 => {
                // See log_100_transfer_characteristics() for derivation
                // The opto-electronic transfer characteristic function (OETF)
                // as defined in ITU-T H.273 table 3, row 9:
                //
                // V = 1.0 + Log10(Lc) ÷ 2  for 1    >= Lc >= 0.01
                // V = 0.0                  for 0.01 >  Lc >= 0
                //
                // Inverting this to give the EOTF required for the profile gives
                //
                // Lc = 10^(2*V - 2)  for 1 >= V >= 0
                let table = build_trc_table(NUM_TRC_TABLE_ENTRIES, |v| 10f64.powf(2. * v - 2.));
                curveType::Curve(table)
            }
            TransferCharacteristics::Log_100_sqrt10 => {
                // The opto-electronic transfer characteristic function (OETF)
                // as defined in ITU-T H.273 table 3, row 10:
                //
                // V = 1.0 + Log10(Lc) ÷ 2.5  for               1 >= Lc >= Sqrt(10) ÷ 1000
                // V = 0.0                    for Sqrt(10) ÷ 1000 >  Lc >= 0
                //
                // Inverting this to give the EOTF required for the profile gives
                //
                // Lc = 10^(2.5*V - 2.5)  for 1 >= V >= 0
                let table = build_trc_table(NUM_TRC_TABLE_ENTRIES, |v| 10f64.powf(2.5 * v - 2.5));
                curveType::Curve(table)
            }
            TransferCharacteristics::Iec61966 => return Err(()),
            TransferCharacteristics::Bt_1361 => return Err(()),
            TransferCharacteristics::Srgb => {
                // Should we prefer this or curveType::Parametric?
                curveType::Curve(build_sRGB_gamma_table(NUM_TRC_TABLE_ENTRIES))
            }

            TransferCharacteristics::Smpte2084 => {
                // Despite using Lo rather than Lc, H.273 gives the OETF:
                //
                // V = ( ( c1 + c2 * (Lo)^n ) ÷ ( 1 + c3 * (Lo)^n ) )^m
                const c1: f64 = 0.8359375;
                const c2: f64 = 18.8515625;
                const c3: f64 = 18.6875;
                const m: f64 = 78.84375;
                const n: f64 = 0.1593017578125;

                // Inverting this to give the EOTF required for the profile
                // (and confirmed by Rec. ITU-R BT.2100-2, Table 4) gives
                //
                // Y = ( max[( X^(1/m) - c1 ), 0] ÷ ( c2 - c3 * X^(1/m) ) )^(1/n)
                let table = build_trc_table(NUM_TRC_TABLE_ENTRIES, |x| {
                    ((x.powf(1. / m) - c1).max(0.) / (c2 - c3 * x.powf(1. / m))).powf(1. / n)
                });
                curveType::Curve(table)
            }
            TransferCharacteristics::Smpte428 => return Err(()),
            TransferCharacteristics::Hlg => {
                // The opto-electronic transfer characteristic function (OETF)
                // as defined in ITU-T H.273 table 3, row 18:
                //
                // V = a * Ln(12 * Lc - b) + c  for 1      >= Lc >  1 ÷ 12
                // V = Sqrt(3) * Lc^0.5         for 1 ÷ 12 >= Lc >= 0
                const a: f64 = 0.17883277;
                const b: f64 = 0.28466892;
                const c: f64 = 0.55991073;

                // Inverting this to give the EOTF required for the profile
                // (and confirmed by Rec. ITU-R BT.2100-2, Table 4) gives
                //
                // Y = (X^2) / 3             for 0   <= X <= 0.5
                // Y = ((e^((X-c)/a))+b)/12  for 0.5 <  X <= 1
                let table = build_trc_table(NUM_TRC_TABLE_ENTRIES, |x| {
                    if x <= 0.5 {
                        let y1 = x.powf(2.) / 3.;
                        assert!((0. ..=1. / 12.).contains(&y1));
                        y1
                    } else {
                        (std::f64::consts::E.powf((x - c) / a) + b) / 12.
                    }
                });
                curveType::Curve(table)
            }
        })
    }
}

#[cfg(test)]
fn check_transfer_characteristics(cicp: TransferCharacteristics, icc_path: &str) {
    let mut cicp_out = [0u8; crate::transform::PRECACHE_OUTPUT_SIZE];
    let mut icc_out = [0u8; crate::transform::PRECACHE_OUTPUT_SIZE];
    let cicp_tc = curveType::try_from(cicp).unwrap();
    let icc = Profile::new_from_path(icc_path).unwrap();
    let icc_tc = icc.redTRC.as_ref().unwrap();

    eprintln!("cicp_tc: {:?}", cicp_tc);
    eprintln!("icc_tc: {:?}", icc_tc);

    crate::transform_util::compute_precache(icc_tc, &mut icc_out);
    crate::transform_util::compute_precache(&cicp_tc, &mut cicp_out);

    let mut off_by_one = 0;
    for i in 0..cicp_out.len() {
        match (cicp_out[i] as i16) - (icc_out[i] as i16) {
            0 => {}
            1 | -1 => {
                off_by_one += 1;
            }
            _ => assert_eq!(cicp_out[i], icc_out[i], "difference at index {}", i),
        }
    }
    eprintln!("{} / {} off by one", off_by_one, cicp_out.len());
}

#[test]
fn srgb_transfer_characteristics() {
    check_transfer_characteristics(TransferCharacteristics::Srgb, "sRGB_lcms.icc");
}

#[test]
fn bt709_transfer_characteristics() {
    check_transfer_characteristics(TransferCharacteristics::Bt709, "ITU-709.icc");
}

#[test]
fn bt2020_10bit_transfer_characteristics() {
    check_transfer_characteristics(TransferCharacteristics::Bt2020_10bit, "ITU-2020.icc");
}

#[test]
fn bt2020_12bit_transfer_characteristics() {
    check_transfer_characteristics(TransferCharacteristics::Bt2020_12bit, "ITU-2020.icc");
}

impl Profile {
    //XXX: it would be nice if we had a way of ensuring
    // everything in a profile was initialized regardless of how it was created
    //XXX: should this also be taking a black_point?
    /* similar to CGColorSpaceCreateCalibratedRGB */
    pub fn new_rgb_with_table(
        white_point: qcms_CIE_xyY,
        primaries: qcms_CIE_xyYTRIPLE,
        table: &[u16],
    ) -> Option<Box<Profile>> {
        let mut profile = profile_create();
        //XXX: should store the whitepoint
        if !set_rgb_colorants(&mut profile, white_point, primaries) {
            return None;
        }
        profile.redTRC = Some(curve_from_table(table));
        profile.blueTRC = Some(curve_from_table(table));
        profile.greenTRC = Some(curve_from_table(table));
        profile.class_type = DISPLAY_DEVICE_PROFILE;
        profile.rendering_intent = Perceptual;
        profile.color_space = RGB_SIGNATURE;
        profile.pcs = XYZ_TYPE;
        Some(profile)
    }
    pub fn new_sRGB() -> Box<Profile> {
        let D65 = qcms_white_point_sRGB();
        let table = build_sRGB_gamma_table(1024);

        let mut srgb = Profile::new_rgb_with_table(
            D65,
            qcms_CIE_xyYTRIPLE::from(ColourPrimaries::Bt709),
            &table,
        )
        .unwrap();
        srgb.is_srgb = true;
        srgb
    }

    /// Returns true if this profile is sRGB
    pub fn is_sRGB(&self) -> bool {
        self.is_srgb
    }

    pub(crate) fn new_sRGB_parametric() -> Box<Profile> {
        let primaries = qcms_CIE_xyYTRIPLE::from(ColourPrimaries::Bt709);
        let white_point = qcms_white_point_sRGB();
        let mut profile = profile_create();
        set_rgb_colorants(&mut profile, white_point, primaries);

        let curve = Box::new(curveType::Parametric(vec![
            2.4,
            1. / 1.055,
            0.055 / 1.055,
            1. / 12.92,
            0.04045,
        ]));
        profile.redTRC = Some(curve.clone());
        profile.blueTRC = Some(curve.clone());
        profile.greenTRC = Some(curve);
        profile.class_type = DISPLAY_DEVICE_PROFILE;
        profile.rendering_intent = Perceptual;
        profile.color_space = RGB_SIGNATURE;
        profile.pcs = XYZ_TYPE;
        profile.is_srgb = true;
        profile
    }

    pub(crate) fn new_displayP3() -> Box<Profile> {
        let primaries = qcms_CIE_xyYTRIPLE::from(ColourPrimaries::Smpte432);
        let white_point = qcms_white_point_sRGB();
        let mut profile = profile_create();
        set_rgb_colorants(&mut profile, white_point, primaries);

        let curve = Box::new(curveType::Parametric(vec![
            2.4,
            1. / 1.055,
            0.055 / 1.055,
            1. / 12.92,
            0.04045,
        ]));
        profile.redTRC = Some(curve.clone());
        profile.blueTRC = Some(curve.clone());
        profile.greenTRC = Some(curve);
        profile.class_type = DISPLAY_DEVICE_PROFILE;
        profile.rendering_intent = Perceptual;
        profile.color_space = RGB_SIGNATURE;
        profile.pcs = XYZ_TYPE;
        profile.is_srgb = false;
        profile
    }

    /// Create a new profile with D50 adopted white and identity transform functions
    pub fn new_XYZD50() -> Box<Profile> {
        let mut profile = profile_create();
        profile.redColorant.X = double_to_s15Fixed16Number(1.);
        profile.redColorant.Y = double_to_s15Fixed16Number(0.);
        profile.redColorant.Z = double_to_s15Fixed16Number(0.);
        profile.greenColorant.X = double_to_s15Fixed16Number(0.);
        profile.greenColorant.Y = double_to_s15Fixed16Number(1.);
        profile.greenColorant.Z = double_to_s15Fixed16Number(0.);
        profile.blueColorant.X = double_to_s15Fixed16Number(0.);
        profile.blueColorant.Y = double_to_s15Fixed16Number(0.);
        profile.blueColorant.Z = double_to_s15Fixed16Number(1.);
        profile.redTRC = Some(identity_curve());
        profile.blueTRC = Some(identity_curve());
        profile.greenTRC = Some(identity_curve());

        profile.class_type = DISPLAY_DEVICE_PROFILE;
        profile.rendering_intent = Perceptual;
        profile.color_space = RGB_SIGNATURE;
        profile.pcs = XYZ_TYPE;
        profile
    }

    pub fn new_cicp(cp: ColourPrimaries, tc: TransferCharacteristics) -> Option<Box<Profile>> {
        let mut profile = profile_create();
        //XXX: should store the whitepoint
        if !set_rgb_colorants(&mut profile, cp.white_point(), qcms_CIE_xyYTRIPLE::from(cp)) {
            return None;
        }
        let curve = curveType::try_from(tc).ok()?;
        profile.redTRC = Some(Box::new(curve.clone()));
        profile.blueTRC = Some(Box::new(curve.clone()));
        profile.greenTRC = Some(Box::new(curve));
        profile.class_type = DISPLAY_DEVICE_PROFILE;
        profile.rendering_intent = Perceptual;
        profile.color_space = RGB_SIGNATURE;
        profile.pcs = XYZ_TYPE;

        profile.is_srgb = (cp, tc) == (ColourPrimaries::Bt709, TransferCharacteristics::Srgb);
        Some(profile)
    }

    pub fn new_gray_with_gamma(gamma: f32) -> Box<Profile> {
        let mut profile = profile_create();

        profile.grayTRC = Some(curve_from_gamma(gamma));
        profile.class_type = DISPLAY_DEVICE_PROFILE;
        profile.rendering_intent = Perceptual;
        profile.color_space = GRAY_SIGNATURE;
        profile.pcs = XYZ_TYPE;
        profile
    }

    pub fn new_rgb_with_gamma_set(
        white_point: qcms_CIE_xyY,
        primaries: qcms_CIE_xyYTRIPLE,
        redGamma: f32,
        greenGamma: f32,
        blueGamma: f32,
    ) -> Option<Box<Profile>> {
        let mut profile = profile_create();

        //XXX: should store the whitepoint
        if !set_rgb_colorants(&mut profile, white_point, primaries) {
            return None;
        }
        profile.redTRC = Some(curve_from_gamma(redGamma));
        profile.blueTRC = Some(curve_from_gamma(blueGamma));
        profile.greenTRC = Some(curve_from_gamma(greenGamma));
        profile.class_type = DISPLAY_DEVICE_PROFILE;
        profile.rendering_intent = Perceptual;
        profile.color_space = RGB_SIGNATURE;
        profile.pcs = XYZ_TYPE;
        Some(profile)
    }

    pub fn new_from_path(file: &str) -> Option<Box<Profile>> {
        Profile::new_from_slice(&std::fs::read(file).ok()?, false)
    }

    pub fn new_from_slice(mem: &[u8], curves_only: bool) -> Option<Box<Profile>> {
        let length: u32;
        let mut source: MemSource = MemSource {
            buf: mem,
            valid: false,
            invalid_reason: None,
        };
        let index;
        source.valid = true;
        let src: &mut MemSource = &mut source;
        if mem.len() < 4 {
            return None;
        }
        length = read_u32(src, 0);
        if length as usize <= mem.len() {
            // shrink the area that we can read if appropriate
            src.buf = &src.buf[0..length as usize];
        } else {
            return None;
        }
        /* ensure that the profile size is sane so it's easier to reason about */
        if src.buf.len() <= 64 || src.buf.len() >= MAX_PROFILE_SIZE {
            return None;
        }
        let mut profile = profile_create();

        check_CMM_type_signature(src);
        check_profile_version(src);
        read_class_signature(&mut profile, src);
        read_rendering_intent(&mut profile, src);
        read_color_space(&mut profile, src);
        read_pcs(&mut profile, src);
        //TODO read rest of profile stuff
        if !src.valid {
            return None;
        }

        index = read_tag_table(&mut profile, src);
        if !src.valid || index.is_empty() {
            return None;
        }

        if let Some(chad) = find_tag(&index, TAG_CHAD) {
            profile.chromaticAdaption = Some(read_tag_s15Fixed16ArrayType(src, chad))
        } else {
            profile.chromaticAdaption = None; //Signal the data is not present
        }

        if profile.class_type == DISPLAY_DEVICE_PROFILE
            || profile.class_type == INPUT_DEVICE_PROFILE
            || profile.class_type == OUTPUT_DEVICE_PROFILE
            || profile.class_type == COLOR_SPACE_PROFILE
        {
            if profile.color_space == RGB_SIGNATURE {
                if !curves_only {
                    if let Some(A2B0) = find_tag(&index, TAG_A2B0) {
                        let lut_type = read_u32(src, A2B0.offset as usize);
                        if lut_type == LUT8_TYPE || lut_type == LUT16_TYPE {
                            profile.A2B0 = read_tag_lutType(src, A2B0)
                        } else if lut_type == LUT_MAB_TYPE {
                            profile.mAB = read_tag_lutmABType(src, A2B0)
                        }
                    }
                    if let Some(B2A0) = find_tag(&index, TAG_B2A0) {
                        let lut_type = read_u32(src, B2A0.offset as usize);
                        if lut_type == LUT8_TYPE || lut_type == LUT16_TYPE {
                            profile.B2A0 = read_tag_lutType(src, B2A0)
                        } else if lut_type == LUT_MBA_TYPE {
                            profile.mBA = read_tag_lutmABType(src, B2A0)
                        }
                    }
                }
                if find_tag(&index, TAG_rXYZ).is_some() || curves_only {
                    profile.redColorant = read_tag_XYZType(src, &index, TAG_rXYZ);
                    profile.greenColorant = read_tag_XYZType(src, &index, TAG_gXYZ);
                    profile.blueColorant = read_tag_XYZType(src, &index, TAG_bXYZ)
                }
                if !src.valid {
                    return None;
                }

                if find_tag(&index, TAG_rTRC).is_some() || curves_only {
                    profile.redTRC = read_tag_curveType(src, &index, TAG_rTRC);
                    profile.greenTRC = read_tag_curveType(src, &index, TAG_gTRC);
                    profile.blueTRC = read_tag_curveType(src, &index, TAG_bTRC);
                    if profile.redTRC.is_none()
                        || profile.blueTRC.is_none()
                        || profile.greenTRC.is_none()
                    {
                        return None;
                    }
                }
            } else if profile.color_space == GRAY_SIGNATURE {
                profile.grayTRC = read_tag_curveType(src, &index, TAG_kTRC);
                profile.grayTRC.as_ref()?;
            } else if profile.color_space == CMYK_SIGNATURE {
                if let Some(A2B0) = find_tag(&index, TAG_A2B0) {
                    let lut_type = read_u32(src, A2B0.offset as usize);
                    if lut_type == LUT8_TYPE || lut_type == LUT16_TYPE {
                        profile.A2B0 = read_tag_lutType(src, A2B0)
                    } else if lut_type == LUT_MBA_TYPE {
                        profile.mAB = read_tag_lutmABType(src, A2B0)
                    }
                }
            } else {
                debug_assert!(false, "read_color_space protects against entering here");
                return None;
            }
        } else {
            return None;
        }

        if !src.valid {
            return None;
        }
        Some(profile)
    }
    /// Precomputes the information needed for this profile to be
    /// used as the output profile when constructing a `Transform`.
    pub fn precache_output_transform(&mut self) {
        crate::transform::qcms_profile_precache_output_transform(self);
    }
}

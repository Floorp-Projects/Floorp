#![allow(clippy::missing_safety_doc)]

use std::{ptr::null_mut, slice};

use libc::{fclose, fopen, fread, free, malloc, memset, FILE};

use crate::{
    double_to_s15Fixed16Number,
    iccread::*,
    s15Fixed16Number_to_float,
    transform::get_rgb_colorants,
    transform::DataType,
    transform::{qcms_transform, transform_create},
    transform_util,
    Intent,
};

#[no_mangle]
pub extern "C" fn qcms_profile_sRGB() -> *mut Profile {
    let profile = Profile::new_sRGB();
    Box::into_raw(profile)
}

#[no_mangle]
pub extern "C" fn qcms_profile_displayP3() -> *mut Profile {
    let profile = Profile::new_displayP3();
    Box::into_raw(profile)
}


//XXX: it would be nice if we had a way of ensuring
// everything in a profile was initialized regardless of how it was created
//XXX: should this also be taking a black_point?
/* similar to CGColorSpaceCreateCalibratedRGB */
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_rgb_with_gamma_set(
    white_point: qcms_CIE_xyY,
    primaries: qcms_CIE_xyYTRIPLE,
    redGamma: f32,
    greenGamma: f32,
    blueGamma: f32,
) -> *mut Profile {
    let profile =
        Profile::new_rgb_with_gamma_set(white_point, primaries, redGamma, greenGamma, blueGamma);
    profile.map_or_else(null_mut, Box::into_raw)
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_gray_with_gamma(gamma: f32) -> *mut Profile {
    let profile = Profile::new_gray_with_gamma(gamma);
    Box::into_raw(profile)
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_rgb_with_gamma(
    white_point: qcms_CIE_xyY,
    primaries: qcms_CIE_xyYTRIPLE,
    gamma: f32,
) -> *mut Profile {
    qcms_profile_create_rgb_with_gamma_set(white_point, primaries, gamma, gamma, gamma)
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_rgb_with_table(
    white_point: qcms_CIE_xyY,
    primaries: qcms_CIE_xyYTRIPLE,
    table: *const u16,
    num_entries: i32,
) -> *mut Profile {
    let table = slice::from_raw_parts(table, num_entries as usize);
    let profile = Profile::new_rgb_with_table(white_point, primaries, table);
    profile.map_or_else(null_mut, Box::into_raw)
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_cicp(
    colour_primaries: u8,
    transfer_characteristics: u8,
) -> *mut Profile {
    Profile::new_cicp(colour_primaries.into(), transfer_characteristics.into())
        .map_or_else(null_mut, Box::into_raw)
}

/* qcms_profile_from_memory does not hold a reference to the memory passed in */
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_memory(
    mem: *const libc::c_void,
    size: usize,
) -> *mut Profile {
    let mem = slice::from_raw_parts(mem as *const libc::c_uchar, size);
    let profile = Profile::new_from_slice(mem, false);
    profile.map_or_else(null_mut, Box::into_raw)
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_memory_curves_only(
    mem: *const libc::c_void,
    size: usize,
) -> *mut Profile {
    let mem = slice::from_raw_parts(mem as *const libc::c_uchar, size);
    let profile = Profile::new_from_slice(mem, true);
    profile.map_or_else(null_mut, Box::into_raw)
}


#[no_mangle]
pub extern "C" fn qcms_profile_get_rendering_intent(profile: &Profile) -> Intent {
    profile.rendering_intent
}
#[no_mangle]
pub extern "C" fn qcms_profile_get_color_space(profile: &Profile) -> icColorSpaceSignature {
    profile.color_space
}
#[no_mangle]
pub extern "C" fn qcms_profile_is_sRGB(profile: &Profile) -> bool {
    profile.is_sRGB()
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_release(profile: *mut Profile) {
    drop(Box::from_raw(profile));
}
unsafe extern "C" fn qcms_data_from_file(
    file: *mut FILE,
    mem: *mut *mut libc::c_void,
    size: *mut usize,
) {
    let length: u32;
    let remaining_length: u32;
    let read_length: usize;
    let mut length_be: u32 = 0;
    let data: *mut libc::c_void;
    *mem = std::ptr::null_mut::<libc::c_void>();
    *size = 0;
    if fread(
        &mut length_be as *mut u32 as *mut libc::c_void,
        1,
        ::std::mem::size_of::<u32>(),
        file,
    ) != ::std::mem::size_of::<u32>()
    {
        return;
    }
    length = u32::from_be(length_be);
    if length > MAX_PROFILE_SIZE as libc::c_uint
        || (length as libc::c_ulong) < ::std::mem::size_of::<u32>() as libc::c_ulong
    {
        return;
    }
    /* allocate room for the entire profile */
    data = malloc(length as usize);
    if data.is_null() {
        return;
    }
    /* copy in length to the front so that the buffer will contain the entire profile */
    *(data as *mut u32) = length_be;
    remaining_length =
        (length as libc::c_ulong - ::std::mem::size_of::<u32>() as libc::c_ulong) as u32;
    /* read the rest profile */
    read_length = fread(
        (data as *mut libc::c_uchar).add(::std::mem::size_of::<u32>()) as *mut libc::c_void,
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
pub unsafe extern "C" fn qcms_profile_from_file(file: *mut FILE) -> *mut Profile {
    let mut length: usize = 0;
    let profile: *mut Profile;
    let mut data: *mut libc::c_void = std::ptr::null_mut::<libc::c_void>();
    qcms_data_from_file(file, &mut data, &mut length);
    if data.is_null() || length == 0 {
        return std::ptr::null_mut::<Profile>();
    }
    profile = qcms_profile_from_memory(data, length);
    free(data);
    profile
}
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_path(path: *const libc::c_char) -> *mut Profile {
    if let Ok(Some(boxed_profile)) = std::ffi::CStr::from_ptr(path)
        .to_str()
        .map(Profile::new_from_path)
    {
        Box::into_raw(boxed_profile)
    } else {
        std::ptr::null_mut()
    }
}

#[no_mangle]
pub unsafe extern "C" fn qcms_data_from_path(
    path: *const libc::c_char,
    mem: *mut *mut libc::c_void,
    size: *mut usize,
) {
    *mem = std::ptr::null_mut::<libc::c_void>();
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
pub unsafe extern "C" fn qcms_profile_from_unicode_path(path: *const libc::wchar_t) {
    let file = _wfopen(path, ['r' as u16, 'b' as u16, '\0' as u16].as_ptr());
    if !file.is_null() {
        qcms_profile_from_file(file);
        fclose(file);
    };
}

#[cfg(windows)]
#[no_mangle]
pub unsafe extern "C" fn qcms_data_from_unicode_path(
    path: *const libc::wchar_t,
    mem: *mut *mut libc::c_void,
    size: *mut usize,
) {
    *mem = 0 as *mut libc::c_void;
    *size = 0;
    let file = _wfopen(path, ['r' as u16, 'b' as u16, '\0' as u16].as_ptr());
    if !file.is_null() {
        qcms_data_from_file(file, mem, size);
        fclose(file);
    };
}

#[no_mangle]
pub extern "C" fn qcms_transform_create(
    in_0: &Profile,
    in_type: DataType,
    out: &Profile,
    out_type: DataType,
    intent: Intent,
) -> *mut qcms_transform {
    let transform = transform_create(in_0, in_type, out, out_type, intent);
    match transform {
        Some(transform) => Box::into_raw(transform),
        None => null_mut(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn qcms_data_create_rgb_with_gamma(
    white_point: qcms_CIE_xyY,
    primaries: qcms_CIE_xyYTRIPLE,
    gamma: f32,
    mem: *mut *mut libc::c_void,
    size: *mut usize,
) {
    let length: u32;
    let mut index: u32;
    let xyz_count: u32;
    let trc_count: u32;
    let mut tag_table_offset: usize;
    let mut tag_data_offset: usize;
    let data: *mut libc::c_void;

    let TAG_XYZ: [u32; 3] = [TAG_rXYZ, TAG_gXYZ, TAG_bXYZ];
    let TAG_TRC: [u32; 3] = [TAG_rTRC, TAG_gTRC, TAG_bTRC];
    if mem.is_null() || size.is_null() {
        return;
    }
    *mem = std::ptr::null_mut::<libc::c_void>();
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
    let colorants = match get_rgb_colorants(white_point, primaries) {
        Some(colorants) => colorants,
        None => {
            free(data);
            return;
        }
    };

    let data = std::slice::from_raw_parts_mut(data as *mut u8, length as usize);
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
        tag_table_offset += 12;
        tag_data_offset += 20;
        index += 1
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
        tag_table_offset += 12;
        tag_data_offset += 16;
        index += 1
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
    write_u32(data, 64, Intent::Perceptual as u32); // profile->rendering_intent
    write_u32(data, 128, 6); // total tag count
                             // prepare the result
    *mem = data.as_mut_ptr() as *mut libc::c_void;
    *size = length as usize;
}

#[no_mangle]
pub unsafe extern "C" fn qcms_transform_data(
    transform: &qcms_transform,
    src: *const libc::c_void,
    dest: *mut libc::c_void,
    length: usize,
) {
    transform.transform_fn.expect("non-null function pointer")(
        transform,
        src as *const u8,
        dest as *mut u8,
        length,
    );
}
/*
use crate::matrix;
#[repr(C)]
#[derive(Clone, Debug, Default)]
pub struct qcms_mat3r3 {
    pub rows: [[f32; 3] ; 3],
}
impl qcms_mat3r3 {
    fn from(m: matrix::Matrix) -> qcms_mat3r3 {
        qcms_mat3r3{
            rows: [
                m.row(0),
                m.row(1),
                m.row(2),
            ],
        }
    }
}
*/
#[repr(C)]
#[derive(Clone, Debug, Default)]
#[allow(clippy::upper_case_acronyms)]
pub struct qcms_profile_data {
    pub class_type: u32,
    pub color_space: u32,
    pub pcs: u32,
    pub rendering_intent: Intent,
    pub red_colorant_xyzd50: [f32; 3],
    pub blue_colorant_xyzd50: [f32; 3],
    pub green_colorant_xyzd50: [f32; 3],
     // Number of samples in the e.g. gamma->linear LUT.
    pub linear_from_trc_red_samples: i32,
    pub linear_from_trc_blue_samples: i32,
    pub linear_from_trc_green_samples: i32,
}

pub use crate::iccread::Profile as qcms_profile;

#[no_mangle]
pub extern "C" fn qcms_profile_get_data(
    profile: &qcms_profile,
    out_data: &mut qcms_profile_data,
) {
    out_data.class_type = profile.class_type;
    out_data.color_space = profile.color_space;
    out_data.pcs = profile.pcs;
    out_data.rendering_intent = profile.rendering_intent;

    fn colorant(c: &XYZNumber) -> [f32;3] {
        [c.X, c.Y, c.Z].map(s15Fixed16Number_to_float)
    }
    out_data.red_colorant_xyzd50 = colorant(&profile.redColorant);
    out_data.blue_colorant_xyzd50 = colorant(&profile.blueColorant);
    out_data.green_colorant_xyzd50 = colorant(&profile.greenColorant);

    fn trc_to_samples(trc: &Option<Box<curveType>>) -> i32 {
        if let Some(ref trc) = *trc {
            match &**trc {
                curveType::Curve(v) => {
                    let len = v.len();
                    if len <= 1 {
                        -1
                    } else {
                        len as i32
                    }
                },
                curveType::Parametric(_) => -1,
            }
        } else {
            0
        }
    }
    out_data.linear_from_trc_red_samples = trc_to_samples(&profile.redTRC);
    out_data.linear_from_trc_blue_samples = trc_to_samples(&profile.blueTRC);
    out_data.linear_from_trc_green_samples = trc_to_samples(&profile.greenTRC);
}

#[repr(u8)]
pub enum qcms_color_channel {
    Red,
    Green,
    Blue,
}

#[no_mangle]
pub extern "C" fn qcms_profile_get_lut(
    profile: &qcms_profile,
    channel: qcms_color_channel, // FYI: UB if you give Rust something out of range!
    out_begin: *mut f32,
    out_end: *mut f32,
) {
    let out_slice = unsafe {
        std::slice::from_raw_parts_mut(out_begin, out_end.offset_from(out_begin) as usize)
    };

    let trc = match channel {
        qcms_color_channel::Red => &profile.redTRC,
        qcms_color_channel::Green => &profile.greenTRC,
        qcms_color_channel::Blue => &profile.blueTRC,
    };

    let samples_u16 = if let Some(trc) = trc {
        let trc = &*trc;
        // Yes, sub-optimal, but easier to implement, and these aren't big or hot:
        // 1. Ask for a new vec<u16> lut based on the trc.
        //   * (eat the extra alloc)
        // 2. Convert the u16s back out to f32s in our slice.
        //   * (eat the copy and quantization error from f32->u16->f32 roundtrip)
        transform_util::build_lut_for_linear_from_tf(trc, Some(out_slice.len()))
    } else {
        Vec::new()
    };

    assert_eq!(samples_u16.len(), out_slice.len());
    for (d, s) in out_slice.iter_mut().zip(samples_u16.into_iter()) {
        *d = (s as f32) / (u16::MAX as f32);
    }
}

pub type icColorSpaceSignature = u32;
pub const icSigGrayData: icColorSpaceSignature = 0x47524159; // 'GRAY'
pub const icSigRgbData: icColorSpaceSignature = 0x52474220; // 'RGB '
pub const icSigCmykData: icColorSpaceSignature = 0x434d594b; // 'CMYK'

pub use crate::iccread::qcms_profile_is_bogus;
pub use crate::transform::{
    qcms_enable_iccv4, qcms_profile_precache_output_transform, qcms_transform_release,
};

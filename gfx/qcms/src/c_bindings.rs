use std::{ptr::null_mut, slice};

use libc::{fclose, fopen, fread, free, malloc, FILE};

use crate::{
    iccread::*,
    qcms_intent,
    transform::qcms_data_type,
    transform::{qcms_transform, transform_create},
};

#[no_mangle]
pub extern "C" fn qcms_profile_sRGB() -> *mut qcms_profile {
    let profile = profile_sRGB();
    match profile {
        Some(profile) => Box::into_raw(profile),
        None => null_mut(),
    }
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
    let profile =
        profile_create_rgb_with_gamma_set(white_point, primaries, redGamma, greenGamma, blueGamma);
    match profile {
        Some(profile) => Box::into_raw(profile),
        None => null_mut(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn qcms_profile_create_gray_with_gamma(mut gamma: f32) -> *mut qcms_profile {
    let profile = profile_create_gray_with_gamma(gamma);
    match profile {
        Some(profile) => Box::into_raw(profile),
        None => null_mut(),
    }
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
    mut table: *const u16,
    mut num_entries: i32,
) -> *mut qcms_profile {
    let table = slice::from_raw_parts(table, num_entries as usize);
    let profile = profile_create_rgb_with_table(white_point, primaries, table);
    match profile {
        Some(profile) => Box::into_raw(profile),
        None => null_mut(),
    }
}

/* qcms_profile_from_memory does not hold a reference to the memory passed in */
#[no_mangle]
pub unsafe extern "C" fn qcms_profile_from_memory(
    mut mem: *const libc::c_void,
    mut size: usize,
) -> *mut qcms_profile {
    let mem = slice::from_raw_parts(mem as *const libc::c_uchar, size);
    let profile = profile_from_slice(mem);
    match profile {
        Some(profile) => Box::into_raw(profile),
        None => null_mut(),
    }
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
    length = u32::from_be(length_be);
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
pub extern "C" fn qcms_transform_create(
    mut in_0: &qcms_profile,
    mut in_type: qcms_data_type,
    mut out: &qcms_profile,
    mut out_type: qcms_data_type,
    mut intent: qcms_intent,
) -> *mut qcms_transform {
    let transform = transform_create(in_0, in_type, out, out_type, intent);
    match transform {
        Some(transform) => Box::into_raw(transform),
        None => null_mut(),
    }
}

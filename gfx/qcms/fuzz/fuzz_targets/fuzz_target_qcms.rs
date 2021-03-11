#![no_main]
use libfuzzer_sys::fuzz_target;
extern crate qcms;
extern crate libc;
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use qcms::c_bindings::{qcms_profile, icSigRgbData, qcms_profile_is_bogus, icSigGrayData};
use qcms::c_bindings::{qcms_profile_get_color_space, qcms_profile_get_rendering_intent, qcms_profile_from_memory, qcms_profile_release, qcms_profile_sRGB, qcms_transform_create};
use qcms::c_bindings::{qcms_profile_precache_output_transform, qcms_transform_data, qcms_transform_release, qcms_enable_iccv4};

use qcms::DataType::*;

 unsafe fn transform(src_profile: *const qcms_profile, dst_profile: *mut qcms_profile, size: usize)
 {
   // qcms supports GRAY and RGB profiles as input, and RGB as output.
 
   let src_color_space = qcms_profile_get_color_space(&*src_profile);
   let mut src_type = if (size & 1) != 0 { RGBA8 } else { RGB8 };
   if src_color_space == icSigGrayData {
     src_type = if (size & 1) != 0 { GrayA8 } else { Gray8 };
   } else if src_color_space != icSigRgbData {
     return;
   }
 
   let dst_color_space = qcms_profile_get_color_space(&*dst_profile);
   if dst_color_space != icSigRgbData {
     return;
   }
   let dst_type = if (size & 2) != 0 { RGBA8 } else { RGB8 };
 
   let intent = qcms_profile_get_rendering_intent(&*src_profile);
   // Firefox calls this on the display profile to increase performance.
   // Skip with low probability to increase coverage.
   if (size % 15) != 0 {
     qcms_profile_precache_output_transform(&mut *dst_profile);
   }
 
   let transform =
     qcms_transform_create(&*src_profile, src_type, &*dst_profile, dst_type, intent);
   if transform == std::ptr::null_mut() {
     return;
   }
 
   const SRC_SIZE: usize = 36;
   let src:[u8; SRC_SIZE] = [
     0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x7F, 0x7F, 0xFF, 0x7F, 0x10, 0x20, 0x30,
     0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0xBF, 0xEF, 0x6F,
     0x3F, 0xC0, 0x9F, 0xE0, 0x90, 0xCF, 0x40, 0xAF, 0x0F, 0x01, 0x60, 0xF0,
   ];
   let mut dst: [u8; 36 * 4] = [0; 144]; // 4x in case of GRAY to RGBA
 
   qcms_transform_data(&*transform, src.as_ptr() as *const libc::c_void, dst.as_mut_ptr() as *mut libc::c_void, (SRC_SIZE / src_type.bytes_per_pixel()) as usize);
   qcms_transform_release(transform);
 }
 
 unsafe fn do_fuzz(data: &[u8])
 {
   let size = data.len();
   qcms_enable_iccv4();
 
   let profile = qcms_profile_from_memory(data.as_ptr() as *const libc::c_void, size);
   if profile == std::ptr::null_mut() {
     return;
   }
 
   let srgb_profile = qcms_profile_sRGB();
   if srgb_profile == std::ptr::null_mut() {
     qcms_profile_release(profile);
     return;
   }
 
   transform(profile, srgb_profile, size);
 
   // Firefox only checks the display (destination) profile.
   if !qcms_profile_is_bogus(&mut *profile) {
 
     transform(srgb_profile, profile, size);
     let identity = qcms::Profile::new_XYZD50();
     transform(&*identity, profile, size);
   }
   qcms_profile_release(profile);
   qcms_profile_release(srgb_profile);
 
   return;
 }
 
 

fuzz_target!(|data: &[u8]| {
    unsafe { do_fuzz(data) }
});

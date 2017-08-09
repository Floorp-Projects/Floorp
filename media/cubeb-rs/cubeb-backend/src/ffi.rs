// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#![allow(non_camel_case_types)]

use cubeb_core::ffi;
use std::fmt::{self, Debug};
use std::os::raw::{c_char, c_int, c_long, c_uint, c_void};

macro_rules! cubeb_enum {
    (pub enum $name:ident { $($variants:tt)* }) => {
        pub type $name = i32;
        cubeb_enum!(gen, $name, 0, $($variants)*);
    };
    (pub enum $name:ident: $t:ty { $($variants:tt)* }) => {
        pub type $name = $t;
        cubeb_enum!(gen, $name, 0, $($variants)*);
    };
    (gen, $name:ident, $val:expr, $variant:ident, $($rest:tt)*) => {
        pub const $variant: $name = $val;
        cubeb_enum!(gen, $name, $val+1, $($rest)*);
    };
    (gen, $name:ident, $val:expr, $variant:ident = $e:expr, $($rest:tt)*) => {
        pub const $variant: $name = $e;
        cubeb_enum!(gen, $name, $e+1, $($rest)*);
    };
    (gen, $name:ident, $val:expr, ) => {}
}

// cubeb_internal.h
#[repr(C)]
pub struct cubeb_layout_map {
    name: *const c_char,
    channels: c_uint,
    layout: ffi::cubeb_channel_layout
}

#[repr(C)]
pub struct Ops {
    pub init: Option<
        unsafe extern fn(context: *mut *mut ffi::cubeb,
                         context_name: *const c_char)
                         -> c_int>,
    pub get_backend_id: Option<
        unsafe extern fn(context: *mut ffi::cubeb) -> *const c_char>,
    pub get_max_channel_count: Option<
        unsafe extern fn(context: *mut ffi::cubeb,
                         max_channels: *mut u32)
                         -> c_int>,
    pub get_min_latency: Option<
        unsafe extern fn(context: *mut ffi::cubeb,
                         params: ffi::cubeb_stream_params,
                         latency_ms: *mut u32)
                         -> c_int>,
    pub get_preferred_sample_rate: Option<
        unsafe extern fn(context: *mut ffi::cubeb, rate: *mut u32) -> c_int>,
    pub get_preferred_channel_layout: Option<
        unsafe extern fn(context: *mut ffi::cubeb,
                         layout: *mut ffi::cubeb_channel_layout)
                         -> c_int>,
    pub enumerate_devices: Option<
        unsafe extern fn(context: *mut ffi::cubeb,
                         devtype: ffi::cubeb_device_type,
                         collection: *mut ffi::cubeb_device_collection)
                         -> c_int>,
    pub device_collection_destroy: Option<
        unsafe extern fn(context: *mut ffi::cubeb,
                         collection: *mut ffi::cubeb_device_collection)
                         -> c_int>,
    pub destroy: Option<unsafe extern "C" fn(context: *mut ffi::cubeb)>,
    pub stream_init: Option<
        unsafe extern fn(context: *mut ffi::cubeb,
                         stream: *mut *mut ffi::cubeb_stream,
                         stream_name: *const c_char,
                         input_device: ffi::cubeb_devid,
                         input_stream_params: *const ffi::cubeb_stream_params,
                         output_device: ffi::cubeb_devid,
                         output_stream_params: *const ffi::cubeb_stream_params,
                         latency: u32,
                         data_callback: ffi::cubeb_data_callback,
                         state_callback: ffi::cubeb_state_callback,
                         user_ptr: *mut c_void)
                         -> c_int>,
    pub stream_destroy: Option<unsafe extern "C" fn(stream: *mut ffi::cubeb_stream)>,
    pub stream_start: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream) -> c_int>,
    pub stream_stop: Option<
            unsafe extern fn(stream: *mut ffi::cubeb_stream) -> c_int>,
    pub stream_reset_default_device: Option<
            unsafe extern fn(stream: *mut ffi::cubeb_stream) -> c_int>,
    pub stream_get_position: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream,
                         position: *mut u64)
                         -> c_int>,
    pub stream_get_latency: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream,
                         latency: *mut u32)
                         -> c_int>,
    pub stream_set_volume: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream, volumes: f32)
                         -> c_int>,
    pub stream_set_panning: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream, panning: f32)
                         -> c_int>,
    pub stream_get_current_device: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream,
                         device: *mut *const ffi::cubeb_device)
                         -> c_int>,
    pub stream_device_destroy: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream,
                         device: *const ffi::cubeb_device)
                         -> c_int>,
    pub stream_register_device_changed_callback: Option<
        unsafe extern fn(stream: *mut ffi::cubeb_stream,
                         device_changed_callback:
                         ffi::cubeb_device_changed_callback)
                         -> c_int>,
    pub register_device_collection_changed: Option<
        unsafe extern fn(context: *mut ffi::cubeb,
                         devtype: ffi::cubeb_device_type,
                         callback: ffi::cubeb_device_collection_changed_callback,
                         user_ptr: *mut c_void)
                             -> c_int>
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct LayoutMap {
    pub name: *const c_char,
    pub channels: u32,
    pub layout: ffi::cubeb_channel_layout
}

// cubeb_mixer.h
cubeb_enum! {
    pub enum cubeb_channel {
        // These need to match cubeb_channel
        CHANNEL_INVALID = -1,
        CHANNEL_MONO = 0,
        CHANNEL_LEFT,
        CHANNEL_RIGHT,
        CHANNEL_CENTER,
        CHANNEL_LS,
        CHANNEL_RS,
        CHANNEL_RLS,
        CHANNEL_RCENTER,
        CHANNEL_RRS,
        CHANNEL_LFE,
        CHANNEL_UNMAPPED,
        CHANNEL_MAX = 256,
    }
}

#[repr(C)]
#[derive(Copy)]
pub struct cubeb_channel_map {
    pub channels: c_uint,
    pub map: [cubeb_channel; CHANNEL_MAX as usize]
}

impl Clone for cubeb_channel_map {
    /// Returns a deep copy of the value.
    #[inline]
    fn clone(&self) -> Self {
        *self
    }
}

impl Debug for cubeb_channel_map {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        fmt.debug_struct("cubeb_channel_map")
            .field("channels", &self.channels)
            .field("map", &self.map.iter().take(self.channels as usize))
            .finish()
    }
}

extern "C" {
    pub fn cubeb_channel_map_to_layout(
        channel_map: *const cubeb_channel_map,
    ) -> ffi::cubeb_channel_layout;
    pub fn cubeb_should_upmix(
        stream: *const ffi::cubeb_stream_params,
        mixer: *const ffi::cubeb_stream_params,
    ) -> bool;
    pub fn cubeb_should_downmix(
        stream: *const ffi::cubeb_stream_params,
        mixer: *const ffi::cubeb_stream_params,
    ) -> bool;
    pub fn cubeb_downmix_float(
        input: *const f32,
        inframes: c_long,
        output: *mut f32,
        in_channels: u32,
        out_channels: u32,
        in_layout: ffi::cubeb_channel_layout,
        out_layout: ffi::cubeb_channel_layout,
    );
    pub fn cubeb_upmix_float(
        input: *const f32,
        inframes: c_long,
        output: *mut f32,
        in_channels: u32,
        out_channels: u32,
    );

    pub static CHANNEL_INDEX_TO_ORDER: [[cubeb_channel; CHANNEL_MAX as usize];
                                           ffi::CUBEB_LAYOUT_MAX as usize];
}

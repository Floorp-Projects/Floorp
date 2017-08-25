#![allow(non_camel_case_types)]

extern crate cubeb_core;

use cubeb_core::ffi::{cubeb, cubeb_channel_layout, cubeb_data_callback,
                      cubeb_device, cubeb_device_changed_callback,
                      cubeb_device_collection,
                      cubeb_device_collection_changed_callback, cubeb_device_type,
                      cubeb_devid, cubeb_log_callback, cubeb_log_level,
                      cubeb_state_callback, cubeb_stream, cubeb_stream_params};
use std::os::raw::{c_char, c_float, c_int, c_uint, c_void};

extern "C" {
    pub fn cubeb_init(
        context: *mut *mut cubeb,
        context_name: *const c_char,
        backend_name: *const c_char,
    ) -> c_int;
    pub fn cubeb_get_backend_id(context: *mut cubeb) -> *const c_char;
    pub fn cubeb_get_max_channel_count(
        context: *mut cubeb,
        max_channels: *mut c_uint,
    ) -> c_int;
    pub fn cubeb_get_min_latency(
        context: *mut cubeb,
        params: *const cubeb_stream_params,
        latency_frames: *mut c_uint,
    ) -> c_int;
    pub fn cubeb_get_preferred_sample_rate(
        context: *mut cubeb,
        rate: *mut c_uint,
    ) -> c_int;
    pub fn cubeb_get_preferred_channel_layout(
        context: *mut cubeb,
        layout: *mut cubeb_channel_layout,
    ) -> c_int;
    pub fn cubeb_destroy(context: *mut cubeb);
    pub fn cubeb_stream_init(
        context: *mut cubeb,
        stream: *mut *mut cubeb_stream,
        stream_name: *const c_char,
        input_device: cubeb_devid,
        input_stream_params: *const cubeb_stream_params,
        output_device: cubeb_devid,
        output_stream_params: *const cubeb_stream_params,
        latency_frames: c_uint,
        data_callback: cubeb_data_callback,
        state_callback: cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> c_int;
    pub fn cubeb_stream_destroy(stream: *mut cubeb_stream);
    pub fn cubeb_stream_start(stream: *mut cubeb_stream) -> c_int;
    pub fn cubeb_stream_stop(stream: *mut cubeb_stream) -> c_int;
    pub fn cubeb_stream_get_position(
        stream: *mut cubeb_stream,
        position: *mut u64,
    ) -> c_int;
    pub fn cubeb_stream_get_latency(
        stream: *mut cubeb_stream,
        latency: *mut c_uint,
    ) -> c_int;
    pub fn cubeb_stream_set_volume(
        stream: *mut cubeb_stream,
        volume: c_float,
    ) -> c_int;
    pub fn cubeb_stream_set_panning(
        stream: *mut cubeb_stream,
        panning: c_float,
    ) -> c_int;
    pub fn cubeb_stream_get_current_device(
        stream: *mut cubeb_stream,
        device: *mut *const cubeb_device,
    ) -> c_int;
    pub fn cubeb_stream_device_destroy(
        stream: *mut cubeb_stream,
        devices: *const cubeb_device,
    ) -> c_int;
    pub fn cubeb_stream_register_device_changed_callback(
        stream: *mut cubeb_stream,
        device_changed_callback: cubeb_device_changed_callback,
    ) -> c_int;
    pub fn cubeb_enumerate_devices(
        context: *mut cubeb,
        devtype: cubeb_device_type,
        collection: *mut cubeb_device_collection,
    ) -> c_int;
    pub fn cubeb_device_collection_destroy(
        context: *mut cubeb,
        collection: *mut cubeb_device_collection,
    ) -> c_int;
    pub fn cubeb_register_device_collection_changed(
        context: *mut cubeb,
        devtype: cubeb_device_type,
        callback: cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> c_int;
    pub fn cubeb_set_log_callback(
        log_level: cubeb_log_level,
        log_callback: cubeb_log_callback,
    ) -> c_int;

    pub static g_cubeb_log_level: cubeb_log_level;
    pub static g_cubeb_log_callback: Option<cubeb_log_callback>;
}

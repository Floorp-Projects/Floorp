// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use cubeb_core::{DeviceId, DeviceType, Result, StreamParams};
use cubeb_core::ffi;
use std::ffi::CStr;
use std::os::raw::c_void;

pub trait Context {
    fn init(context_name: Option<&CStr>) -> Result<*mut ffi::cubeb>;
    fn backend_id(&self) -> &'static CStr;
    fn max_channel_count(&self) -> Result<u32>;
    fn min_latency(&self, params: &StreamParams) -> Result<u32>;
    fn preferred_sample_rate(&self) -> Result<u32>;
    fn preferred_channel_layout(&self) -> Result<ffi::cubeb_channel_layout>;
    fn enumerate_devices(
        &self,
        devtype: DeviceType,
    ) -> Result<ffi::cubeb_device_collection>;
    fn device_collection_destroy(
        &self,
        collection: *mut ffi::cubeb_device_collection,
    );
    fn stream_init(
        &self,
        stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&ffi::cubeb_stream_params>,
        output_device: DeviceId,
        output_stream_params: Option<&ffi::cubeb_stream_params>,
        latency_frames: u32,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<*mut ffi::cubeb_stream>;
    fn register_device_collection_changed(
        &self,
        devtype: DeviceType,
        cb: ffi::cubeb_device_collection_changed_callback,
        user_ptr: *mut c_void,
    ) -> Result<()>;
}

pub trait Stream {
    fn start(&self) -> Result<()>;
    fn stop(&self) -> Result<()>;
    fn reset_default_device(&self) -> Result<()>;
    fn position(&self) -> Result<u64>;
    fn latency(&self) -> Result<u32>;
    fn set_volume(&self, volume: f32) -> Result<()>;
    fn set_panning(&self, panning: f32) -> Result<()>;
    fn current_device(&self) -> Result<*const ffi::cubeb_device>;
    fn device_destroy(&self, device: *const ffi::cubeb_device) -> Result<()>;
    fn register_device_changed_callback(
        &self,
        device_changed_callback: ffi::cubeb_device_changed_callback,
    ) -> Result<()>;
}

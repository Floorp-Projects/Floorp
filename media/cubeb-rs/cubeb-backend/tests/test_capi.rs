// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

#[macro_use]
extern crate cubeb_backend;
extern crate cubeb_core;

use cubeb_backend::{Context, Ops, Stream};
use cubeb_core::{ChannelLayout, DeviceId, DeviceType, Result, StreamParams, ffi};
use std::ffi::CStr;
use std::os::raw::c_void;
use std::ptr;

pub const OPS: Ops = capi_new!(TestContext, TestStream);

struct TestContext {
    pub ops: *const Ops
}

impl Context for TestContext {
    fn init(_context_name: Option<&CStr>) -> Result<*mut ffi::cubeb> {
        let ctx = Box::new(TestContext {
            ops: &OPS as *const _
        });
        Ok(Box::into_raw(ctx) as *mut _)
    }

    fn backend_id(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(b"remote\0".as_ptr() as *const _) }
    }
    fn max_channel_count(&self) -> Result<u32> {
        Ok(0u32)
    }
    fn min_latency(&self, _params: &StreamParams) -> Result<u32> {
        Ok(0u32)
    }
    fn preferred_sample_rate(&self) -> Result<u32> {
        Ok(0u32)
    }
    fn preferred_channel_layout(&self) -> Result<ffi::cubeb_channel_layout> {
        Ok(ChannelLayout::Mono as _)
    }
    fn enumerate_devices(
        &self,
        _devtype: DeviceType,
    ) -> Result<ffi::cubeb_device_collection> {
        Ok(ffi::cubeb_device_collection {
            device: 0xDEADBEEF as *const _,
            count: usize::max_value()
        })
    }
    fn device_collection_destroy(
        &self,
        collection: *mut ffi::cubeb_device_collection,
    ) {
        let mut coll = unsafe { &mut *collection };
        assert_eq!(coll.device, 0xDEADBEEF as *const _);
        assert_eq!(coll.count, usize::max_value());
        coll.device = ptr::null_mut();
        coll.count = 0;
    }
    fn stream_init(
        &self,
        _stream_name: Option<&CStr>,
        _input_device: DeviceId,
        _input_stream_params: Option<&ffi::cubeb_stream_params>,
        _output_device: DeviceId,
        _output_stream_params: Option<&ffi::cubeb_stream_params>,
        _latency_frame: u32,
        _data_callback: ffi::cubeb_data_callback,
        _state_callback: ffi::cubeb_state_callback,
        _user_ptr: *mut c_void,
    ) -> Result<*mut ffi::cubeb_stream> {
        Ok(ptr::null_mut())
    }
    fn register_device_collection_changed(
        &self,
        _dev_type: DeviceType,
        _collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
        _user_ptr: *mut c_void,
    ) -> Result<()> {
        Ok(())
    }
}

struct TestStream {}

impl Stream for TestStream {
    fn start(&self) -> Result<()> {
        Ok(())
    }
    fn stop(&self) -> Result<()> {
        Ok(())
    }
    fn reset_default_device(&self) -> Result<()> {
        Ok(())
    }
    fn position(&self) -> Result<u64> {
        Ok(0u64)
    }
    fn latency(&self) -> Result<u32> {
        Ok(0u32)
    }
    fn set_volume(&self, volume: f32) -> Result<()> {
        assert_eq!(volume, 0.5);
        Ok(())
    }
    fn set_panning(&self, panning: f32) -> Result<()> {
        assert_eq!(panning, 0.5);
        Ok(())
    }
    fn current_device(&self) -> Result<*const ffi::cubeb_device> {
        Ok(0xDEADBEEF as *const _)
    }
    fn device_destroy(&self, device: *const ffi::cubeb_device) -> Result<()> {
        assert_eq!(device, 0xDEADBEEF as *const _);
        Ok(())
    }
    fn register_device_changed_callback(
        &self,
        _: ffi::cubeb_device_changed_callback,
    ) -> Result<()> {
        Ok(())
    }
}

#[test]
fn test_ops_context_init() {
    let mut c: *mut ffi::cubeb = ptr::null_mut();
    assert_eq!(
        unsafe { OPS.init.unwrap()(&mut c, ptr::null()) },
        ffi::CUBEB_OK
    );
}

#[test]
fn test_ops_context_max_channel_count() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut max_channel_count = u32::max_value();
    assert_eq!(
        unsafe { OPS.get_max_channel_count.unwrap()(c, &mut max_channel_count) },
        ffi::CUBEB_OK
    );
    assert_eq!(max_channel_count, 0);
}

#[test]
fn test_ops_context_min_latency() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let params: ffi::cubeb_stream_params = unsafe { ::std::mem::zeroed() };
    let mut latency = u32::max_value();
    assert_eq!(
        unsafe { OPS.get_min_latency.unwrap()(c, params, &mut latency) },
        ffi::CUBEB_OK
    );
    assert_eq!(latency, 0);
}

#[test]
fn test_ops_context_preferred_sample_rate() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut rate = u32::max_value();
    assert_eq!(
        unsafe { OPS.get_preferred_sample_rate.unwrap()(c, &mut rate) },
        ffi::CUBEB_OK
    );
    assert_eq!(rate, 0);
}

#[test]
fn test_ops_context_preferred_channel_layout() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut layout = ChannelLayout::Undefined;
    assert_eq!(
        unsafe {
            OPS.get_preferred_channel_layout.unwrap()(
                c,
                &mut layout as *mut _ as *mut _
            )
        },
        ffi::CUBEB_OK
    );
    assert_eq!(layout, ChannelLayout::Mono);
}

#[test]
fn test_ops_context_enumerate_devices() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut coll = ffi::cubeb_device_collection {
        device: ptr::null(),
        count: 0
    };
    assert_eq!(
        unsafe { OPS.enumerate_devices.unwrap()(c, 0, &mut coll) },
        ffi::CUBEB_OK
    );
    assert_eq!(coll.device, 0xDEADBEEF as *const _);
    assert_eq!(coll.count, usize::max_value())
}

#[test]
fn test_ops_context_device_collection_destroy() {
    let c: *mut ffi::cubeb = ptr::null_mut();
    let mut coll = ffi::cubeb_device_collection {
        device: 0xDEADBEEF as *const _,
        count: usize::max_value()
    };
    assert_eq!(
        unsafe { OPS.device_collection_destroy.unwrap()(c, &mut coll) },
        ffi::CUBEB_OK
    );
    assert_eq!(coll.device, ptr::null_mut());
    assert_eq!(coll.count, 0);
}

// stream_init: Some($crate::capi::capi_stream_init::<$ctx>),
// stream_destroy: Some($crate::capi::capi_stream_destroy::<$stm>),
// stream_start: Some($crate::capi::capi_stream_start::<$stm>),
// stream_stop: Some($crate::capi::capi_stream_stop::<$stm>),
// stream_get_position: Some($crate::capi::capi_stream_get_position::<$stm>),

#[test]
fn test_ops_stream_latency() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    let mut latency = u32::max_value();
    assert_eq!(
        unsafe { OPS.stream_get_latency.unwrap()(s, &mut latency) },
        ffi::CUBEB_OK
    );
    assert_eq!(latency, 0);
}

#[test]
fn test_ops_stream_set_volume() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    unsafe {
        OPS.stream_set_volume.unwrap()(s, 0.5);
    }
}

#[test]
fn test_ops_stream_set_panning() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    unsafe {
        OPS.stream_set_panning.unwrap()(s, 0.5);
    }
}

#[test]
fn test_ops_stream_current_device() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    let mut device: *const ffi::cubeb_device = ptr::null();
    assert_eq!(
        unsafe { OPS.stream_get_current_device.unwrap()(s, &mut device) },
        ffi::CUBEB_OK
    );
    assert_eq!(device, 0xDEADBEEF as *const _);
}

#[test]
fn test_ops_stream_device_destroy() {
    let s: *mut ffi::cubeb_stream = ptr::null_mut();
    unsafe {
        OPS.stream_device_destroy.unwrap()(s, 0xDEADBEEF as *const _);
    }
}

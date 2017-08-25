// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use {Context, Stream};
use cubeb_core::{DeviceId, DeviceType, StreamParams, ffi};
use cubeb_core::binding::Binding;
use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_void};

// Helper macro for unwrapping `Result` values from rust-api calls
// while returning early with a c-api error code if the value of the
// expression is `Err`.
macro_rules! _try(
    ($e:expr) => (match $e {
        Ok(e) => e,
        Err(e) => return e.raw_code()
    })
);

#[macro_export]
macro_rules! capi_new(
    ($ctx:ident, $stm:ident) => (
        Ops {
            init: Some($crate::capi::capi_init::<$ctx>),
            get_backend_id: Some($crate::capi::capi_get_backend_id::<$ctx>),
            get_max_channel_count: Some($crate::capi::capi_get_max_channel_count::<$ctx>),
            get_min_latency: Some($crate::capi::capi_get_min_latency::<$ctx>),
            get_preferred_sample_rate: Some($crate::capi::capi_get_preferred_sample_rate::<$ctx>),
            get_preferred_channel_layout: Some($crate::capi::capi_get_preferred_channel_layout::<$ctx>),
            enumerate_devices: Some($crate::capi::capi_enumerate_devices::<$ctx>),
            device_collection_destroy: Some($crate::capi::capi_device_collection_destroy::<$ctx>),
            destroy: Some($crate::capi::capi_destroy::<$ctx>),
            stream_init: Some($crate::capi::capi_stream_init::<$ctx>),
            stream_destroy: Some($crate::capi::capi_stream_destroy::<$stm>),
            stream_start: Some($crate::capi::capi_stream_start::<$stm>),
            stream_stop: Some($crate::capi::capi_stream_stop::<$stm>),
            stream_reset_default_device: Some($crate::capi::capi_stream_reset_default_device::<$stm>),
            stream_get_position: Some($crate::capi::capi_stream_get_position::<$stm>),
            stream_get_latency: Some($crate::capi::capi_stream_get_latency::<$stm>),
            stream_set_volume: Some($crate::capi::capi_stream_set_volume::<$stm>),
            stream_set_panning: Some($crate::capi::capi_stream_set_panning::<$stm>),
            stream_get_current_device: Some($crate::capi::capi_stream_get_current_device::<$stm>),
            stream_device_destroy: Some($crate::capi::capi_stream_device_destroy::<$stm>),
            stream_register_device_changed_callback: None,
            register_device_collection_changed: Some($crate::capi::capi_register_device_collection_changed::<$ctx>)
        }));

pub unsafe extern "C" fn capi_init<CTX: Context>(
    c: *mut *mut ffi::cubeb,
    context_name: *const c_char,
) -> c_int {
    let anchor = &();
    let context_name = opt_cstr(anchor, context_name);
    *c = _try!(CTX::init(context_name));
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_get_backend_id<CTX: Context>(
    c: *mut ffi::cubeb,
) -> *const c_char {
    let ctx = &mut *(c as *mut CTX);
    ctx.backend_id().as_ptr()
}

pub unsafe extern "C" fn capi_get_max_channel_count<CTX: Context>(
    c: *mut ffi::cubeb,
    max_channels: *mut u32,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);

    *max_channels = _try!(ctx.max_channel_count());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_get_min_latency<CTX: Context>(
    c: *mut ffi::cubeb,
    param: ffi::cubeb_stream_params,
    latency_frames: *mut u32,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);
    let param = StreamParams::from_raw(&param);
    *latency_frames = _try!(ctx.min_latency(&param));
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_get_preferred_sample_rate<CTX: Context>(
    c: *mut ffi::cubeb,
    rate: *mut u32,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);

    *rate = _try!(ctx.preferred_sample_rate());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_get_preferred_channel_layout<CTX: Context>(
    c: *mut ffi::cubeb,
    layout: *mut ffi::cubeb_channel_layout,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);

    *layout = _try!(ctx.preferred_channel_layout()) as _;
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_enumerate_devices<CTX: Context>(
    c: *mut ffi::cubeb,
    devtype: ffi::cubeb_device_type,
    collection: *mut ffi::cubeb_device_collection,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);
    let devtype = DeviceType::from_bits_truncate(devtype);
    *collection = _try!(ctx.enumerate_devices(devtype));
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_device_collection_destroy<CTX: Context>(
    c: *mut ffi::cubeb,
    collection: *mut ffi::cubeb_device_collection,
) -> c_int {
    let ctx = &mut *(c as *mut CTX);

    ctx.device_collection_destroy(collection);
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_destroy<CTX>(c: *mut ffi::cubeb) {
    let _: Box<CTX> = Box::from_raw(c as *mut _);
}

pub unsafe extern "C" fn capi_stream_init<CTX: Context>(
    c: *mut ffi::cubeb,
    s: *mut *mut ffi::cubeb_stream,
    stream_name: *const c_char,
    input_device: ffi::cubeb_devid,
    input_stream_params: *const ffi::cubeb_stream_params,
    output_device: ffi::cubeb_devid,
    output_stream_params: *const ffi::cubeb_stream_params,
    latency_frames: u32,
    data_callback: ffi::cubeb_data_callback,
    state_callback: ffi::cubeb_state_callback,
    user_ptr: *mut c_void,
) -> c_int {
    let ctx = &*(c as *const CTX);
    let anchor = &(); // for lifetime of stream_name as CStr

    let input_device = DeviceId::from_raw(input_device);
    let input_stream_params = input_stream_params.as_opt_ref();
    let output_device = DeviceId::from_raw(output_device);
    let output_stream_params = output_stream_params.as_opt_ref();

    *s = _try!(ctx.stream_init(
        opt_cstr(anchor, stream_name),
        input_device,
        input_stream_params,
        output_device,
        output_stream_params,
        latency_frames,
        data_callback,
        state_callback,
        user_ptr
    ));
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_destroy<STM>(s: *mut ffi::cubeb_stream) {
    let _ = Box::from_raw(s as *mut STM);
}

pub unsafe extern "C" fn capi_stream_start<STM: Stream>(
    s: *mut ffi::cubeb_stream,
) -> c_int {
    let stm = &*(s as *const STM);

    let _ = _try!(stm.start());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_stop<STM: Stream>(
    s: *mut ffi::cubeb_stream,
) -> c_int {
    let stm = &*(s as *const STM);

    let _ = _try!(stm.stop());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_reset_default_device<STM: Stream>(
    s: *mut ffi::cubeb_stream,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    let _ = _try!(stm.reset_default_device());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_get_position<STM: Stream>(
    s: *mut ffi::cubeb_stream,
    position: *mut u64,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    *position = _try!(stm.position());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_get_latency<STM: Stream>(
    s: *mut ffi::cubeb_stream,
    latency: *mut u32,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    *latency = _try!(stm.latency());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_set_volume<STM: Stream>(
    s: *mut ffi::cubeb_stream,
    volume: f32,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    let _ = _try!(stm.set_volume(volume));
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_set_panning<STM: Stream>(
    s: *mut ffi::cubeb_stream,
    panning: f32,
) -> c_int {
    let stm = &mut *(s as *mut STM);

    let _ = _try!(stm.set_panning(panning));
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_get_current_device<STM: Stream>(
    s: *mut ffi::cubeb_stream,
    device: *mut *const ffi::cubeb_device,
) -> i32 {
    let stm = &mut *(s as *mut STM);

    *device = _try!(stm.current_device());
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_stream_device_destroy<STM: Stream>(
    s: *mut ffi::cubeb_stream,
    device: *const ffi::cubeb_device,
) -> c_int {
    let stm = &mut *(s as *mut STM);
    let _ = stm.device_destroy(device);
    ffi::CUBEB_OK
}

pub unsafe extern "C" fn capi_register_device_collection_changed<CTX: Context>(
    c: *mut ffi::cubeb,
    devtype: ffi::cubeb_device_type,
    collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
    user_ptr: *mut c_void,
) -> i32 {
    let ctx = &*(c as *const CTX);
    let devtype = DeviceType::from_bits_truncate(devtype);
    let _ = _try!(ctx.register_device_collection_changed(
        devtype,
        collection_changed_callback,
        user_ptr
    ));
    ffi::CUBEB_OK
}

trait AsOptRef<'a, T: 'a> {
    fn as_opt_ref(self) -> Option<&'a T>;
}

impl<'a, T> AsOptRef<'a, T> for *const T
where
    T: 'a,
{
    fn as_opt_ref(self) -> Option<&'a T> {
        if self.is_null() {
            None
        } else {
            Some(unsafe { &*self })
        }
    }
}

fn opt_cstr<'a, T: 'a>(_anchor: &'a T, ptr: *const c_char) -> Option<&'a CStr> {
    if ptr.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(ptr) })
    }
}

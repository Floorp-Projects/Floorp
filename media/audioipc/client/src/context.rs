// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use ClientStream;
use audioipc::{self, ClientMessage, Connection, ServerMessage, messages};
use cubeb_backend::{Context, Ops};
use cubeb_core::{DeviceId, DeviceType, Error, Result, StreamParams, ffi};
use cubeb_core::binding::Binding;
use std::ffi::{CStr, CString};
use std::mem;
use std::os::raw::c_void;
use std::os::unix::net::UnixStream;
use std::sync::{Mutex, MutexGuard};
use stream;

#[derive(Debug)]
pub struct ClientContext {
    _ops: *const Ops,
    connection: Mutex<Connection>
}

macro_rules! t(
    ($e:expr) => (
        match $e {
            Ok(e) => e,
            Err(_) => return Err(Error::default())
        }
    ));

pub const CLIENT_OPS: Ops = capi_new!(ClientContext, ClientStream);

impl ClientContext {
    #[doc(hidden)]
    pub fn conn(&self) -> MutexGuard<Connection> {
        self.connection.lock().unwrap()
    }
}

impl Context for ClientContext {
    fn init(_context_name: Option<&CStr>) -> Result<*mut ffi::cubeb> {
        // TODO: encapsulate connect, etc inside audioipc.
        let stream = t!(UnixStream::connect(audioipc::get_uds_path()));
        let ctx = Box::new(ClientContext {
            _ops: &CLIENT_OPS as *const _,
            connection: Mutex::new(Connection::new(stream))
        });
        Ok(Box::into_raw(ctx) as *mut _)
    }

    fn backend_id(&self) -> &'static CStr {
        unsafe { CStr::from_ptr(b"remote\0".as_ptr() as *const _) }
    }

    fn max_channel_count(&self) -> Result<u32> {
        send_recv!(self.conn(), ContextGetMaxChannelCount => ContextMaxChannelCount())
    }

    fn min_latency(&self, params: &StreamParams) -> Result<u32> {
        let params = messages::StreamParams::from(unsafe { &*params.raw() });
        send_recv!(self.conn(), ContextGetMinLatency(params) => ContextMinLatency())
    }

    fn preferred_sample_rate(&self) -> Result<u32> {
        send_recv!(self.conn(), ContextGetPreferredSampleRate => ContextPreferredSampleRate())
    }

    fn preferred_channel_layout(&self) -> Result<ffi::cubeb_channel_layout> {
        send_recv!(self.conn(), ContextGetPreferredChannelLayout => ContextPreferredChannelLayout())
    }

    fn enumerate_devices(&self, devtype: DeviceType) -> Result<ffi::cubeb_device_collection> {
        let v: Vec<ffi::cubeb_device_info> =
            match send_recv!(self.conn(), ContextGetDeviceEnumeration(devtype.bits()) => ContextEnumeratedDevices()) {
                Ok(mut v) => v.drain(..).map(|i| i.into()).collect(),
                Err(e) => return Err(e),
            };
        let vs = v.into_boxed_slice();
        let coll = ffi::cubeb_device_collection {
            count: vs.len(),
            device: vs.as_ptr()
        };
        // Giving away the memory owned by vs.  Don't free it!
        // Reclaimed in `device_collection_destroy`.
        mem::forget(vs);
        Ok(coll)
    }

    fn device_collection_destroy(&self, collection: *mut ffi::cubeb_device_collection) {
        unsafe {
            let coll = &*collection;
            let mut devices = Vec::from_raw_parts(
                coll.device as *mut ffi::cubeb_device_info,
                coll.count,
                coll.count
            );
            for dev in devices.iter_mut() {
                if !dev.device_id.is_null() {
                    let _ = CString::from_raw(dev.device_id as *mut _);
                }
                if !dev.group_id.is_null() {
                    let _ = CString::from_raw(dev.group_id as *mut _);
                }
                if !dev.vendor_name.is_null() {
                    let _ = CString::from_raw(dev.vendor_name as *mut _);
                }
                if !dev.friendly_name.is_null() {
                    let _ = CString::from_raw(dev.friendly_name as *mut _);
                }
            }
        }
    }

    fn stream_init(
        &self,
        stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&ffi::cubeb_stream_params>,
        output_device: DeviceId,
        output_stream_params: Option<&ffi::cubeb_stream_params>,
        latency_frame: u32,
        // These params aren't sent to the server
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<*mut ffi::cubeb_stream> {

        fn opt_stream_params(p: Option<&ffi::cubeb_stream_params>) -> Option<messages::StreamParams> {
            match p {
                Some(raw) => Some(messages::StreamParams::from(raw)),
                None => None,
            }
        }

        let stream_name = match stream_name {
            Some(s) => Some(s.to_bytes().to_vec()),
            None => None,
        };

        let input_stream_params = opt_stream_params(input_stream_params);
        let output_stream_params = opt_stream_params(output_stream_params);

        let init_params = messages::StreamInitParams {
            stream_name: stream_name,
            input_device: input_device.raw() as _,
            input_stream_params: input_stream_params,
            output_device: output_device.raw() as _,
            output_stream_params: output_stream_params,
            latency_frames: latency_frame
        };
        stream::init(&self, init_params, data_callback, state_callback, user_ptr)
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

impl Drop for ClientContext {
    fn drop(&mut self) {
        info!("ClientContext drop...");
        let _: Result<()> = send_recv!(self.conn(), ClientDisconnect => ClientDisconnected);
    }
}

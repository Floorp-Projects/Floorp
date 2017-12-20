// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use ClientStream;
use assert_not_in_callback;
use audioipc::{ClientMessage, Connection, ServerMessage, messages};
use cubeb_backend::{Context, Ops};
use cubeb_core::{DeviceId, DeviceType, Error, ErrorCode, Result, StreamParams, ffi};
use cubeb_core::binding::Binding;
use std::ffi::{CStr, CString};
use std::mem;
use std::os::raw::c_void;
use std::os::unix::net::UnixStream;
use std::os::unix::io::FromRawFd;
use std::sync::{Mutex, MutexGuard};
use stream;
use libc;

#[derive(Debug)]
pub struct ClientContext {
    _ops: *const Ops,
    connection: Mutex<Connection>
}

pub const CLIENT_OPS: Ops = capi_new!(ClientContext, ClientStream);

impl ClientContext {
    #[doc(hidden)]
    pub fn connection(&self) -> MutexGuard<Connection> {
        self.connection.lock().unwrap()
    }
}

// TODO: encapsulate connect, etc inside audioipc.
fn open_server_stream() -> Result<UnixStream> {
    unsafe {
        if let Some(fd) = super::G_SERVER_FD {
            return Ok(UnixStream::from_raw_fd(fd));
        }

        Err(Error::default())
    }
}

impl Context for ClientContext {
    fn init(_context_name: Option<&CStr>) -> Result<*mut ffi::cubeb> {
        assert_not_in_callback();
        let ctx = Box::new(ClientContext {
            _ops: &CLIENT_OPS as *const _,
            connection: Mutex::new(Connection::new(open_server_stream()?))
        });
        Ok(Box::into_raw(ctx) as *mut _)
    }

    fn backend_id(&self) -> &'static CStr {
        assert_not_in_callback();
        unsafe { CStr::from_ptr(b"remote\0".as_ptr() as *const _) }
    }

    fn max_channel_count(&self) -> Result<u32> {
        assert_not_in_callback();
        let mut conn = self.connection();
        send_recv!(conn, ContextGetMaxChannelCount => ContextMaxChannelCount())
    }

    fn min_latency(&self, params: &StreamParams) -> Result<u32> {
        assert_not_in_callback();
        let params = messages::StreamParams::from(unsafe { &*params.raw() });
        let mut conn = self.connection();
        send_recv!(conn, ContextGetMinLatency(params) => ContextMinLatency())
    }

    fn preferred_sample_rate(&self) -> Result<u32> {
        assert_not_in_callback();
        let mut conn = self.connection();
        send_recv!(conn, ContextGetPreferredSampleRate => ContextPreferredSampleRate())
    }

    fn preferred_channel_layout(&self) -> Result<ffi::cubeb_channel_layout> {
        assert_not_in_callback();
        let mut conn = self.connection();
        send_recv!(conn, ContextGetPreferredChannelLayout => ContextPreferredChannelLayout())
    }

    fn enumerate_devices(&self, devtype: DeviceType) -> Result<ffi::cubeb_device_collection> {
        assert_not_in_callback();
        let mut conn = self.connection();
        let v: Vec<ffi::cubeb_device_info> =
            match send_recv!(conn, ContextGetDeviceEnumeration(devtype.bits()) => ContextEnumeratedDevices()) {
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
        assert_not_in_callback();
        unsafe {
            let coll = &*collection;
            let mut devices = Vec::from_raw_parts(
                coll.device as *mut ffi::cubeb_device_info,
                coll.count,
                coll.count
            );
            for dev in &mut devices {
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
        assert_not_in_callback();

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
        stream::init(self, init_params, data_callback, state_callback, user_ptr)
    }

    fn register_device_collection_changed(
        &self,
        _dev_type: DeviceType,
        _collection_changed_callback: ffi::cubeb_device_collection_changed_callback,
        _user_ptr: *mut c_void,
    ) -> Result<()> {
        assert_not_in_callback();
        Ok(())
    }
}

impl Drop for ClientContext {
    fn drop(&mut self) {
        let mut conn = self.connection();
        info!("ClientContext drop...");
        let r = conn.send(ServerMessage::ClientDisconnect);
        if r.is_err() {
            debug!("ClientContext::Drop send error={:?}", r);
        } else {
            let r = conn.receive();
            if let Ok(ClientMessage::ClientDisconnected) = r {
            } else {
                debug!("ClientContext::Drop receive error={:?}", r);
            }
        }
        unsafe {
            if super::G_SERVER_FD.is_some() {
                libc::close(super::G_SERVER_FD.take().unwrap());
            }
        }
    }
}

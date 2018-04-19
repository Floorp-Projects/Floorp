// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use {ClientStream, CPUPOOL_INIT_PARAMS, G_SERVER_FD};
use assert_not_in_callback;
use audioipc::{messages, ClientMessage, ServerMessage};
use audioipc::{core, rpc};
use audioipc::codec::LengthDelimitedCodec;
use audioipc::fd_passing::{framed_with_fds, FramedWithFds};
use cubeb_backend::{ffi, Context, ContextOps, DeviceCollectionRef, DeviceId, DeviceType, Error,
                    Ops, Result, Stream, StreamParams, StreamParamsRef};
use futures::Future;
use futures_cpupool::{self, CpuPool};
use libc;
use std::{fmt, io, mem, ptr};
use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::os::unix::io::FromRawFd;
use std::os::unix::net;
use std::sync::mpsc;
use stream;
use tokio_core::reactor::{Handle, Remote};
use tokio_uds::UnixStream;

struct CubebClient;

impl rpc::Client for CubebClient {
    type Request = ServerMessage;
    type Response = ClientMessage;
    type Transport = FramedWithFds<UnixStream, LengthDelimitedCodec<Self::Request, Self::Response>>;
}

macro_rules! t(
    ($e:expr) => (
        match $e {
            Ok(e) => e,
            Err(_) => return Err(Error::default())
        }
    ));

pub const CLIENT_OPS: Ops = capi_new!(ClientContext, ClientStream);

// ClientContext's layout *must* match cubeb.c's `struct cubeb` for the
// common fields.
#[repr(C)]
pub struct ClientContext {
    _ops: *const Ops,
    rpc: rpc::ClientProxy<ServerMessage, ClientMessage>,
    core: core::CoreThread,
    cpu_pool: CpuPool,
}

impl ClientContext {
    #[doc(hidden)]
    pub fn remote(&self) -> Remote {
        self.core.remote()
    }

    #[doc(hidden)]
    pub fn rpc(&self) -> rpc::ClientProxy<ServerMessage, ClientMessage> {
        self.rpc.clone()
    }

    #[doc(hidden)]
    pub fn cpu_pool(&self) -> CpuPool {
        self.cpu_pool.clone()
    }
}

// TODO: encapsulate connect, etc inside audioipc.
fn open_server_stream() -> Result<net::UnixStream> {
    unsafe {
        if let Some(fd) = G_SERVER_FD {
            return Ok(net::UnixStream::from_raw_fd(fd));
        }

        Err(Error::default())
    }
}

impl ContextOps for ClientContext {
    fn init(_context_name: Option<&CStr>) -> Result<Context> {
        fn bind_and_send_client(
            stream: UnixStream,
            handle: &Handle,
            tx_rpc: &mpsc::Sender<rpc::ClientProxy<ServerMessage, ClientMessage>>,
        ) -> Option<()> {
            let transport = framed_with_fds(stream, Default::default());
            let rpc = rpc::bind_client::<CubebClient>(transport, handle);
            // If send fails then the rx end has closed
            // which is unlikely here.
            let _ = tx_rpc.send(rpc);
            Some(())
        }

        assert_not_in_callback();

        let (tx_rpc, rx_rpc) = mpsc::channel();

        let core = t!(core::spawn_thread("AudioIPC Client RPC", move || {
            let handle = core::handle();

            open_server_stream()
                .ok()
                .and_then(|stream| UnixStream::from_stream(stream, &handle).ok())
                .and_then(|stream| bind_and_send_client(stream, &handle, &tx_rpc))
                .ok_or_else(|| {
                    io::Error::new(
                        io::ErrorKind::Other,
                        "Failed to open stream and create rpc.",
                    )
                })
        }));

        let rpc = t!(rx_rpc.recv());

        let cpupool = CPUPOOL_INIT_PARAMS.with(|p| {
            let params = p.replace(None).unwrap();
            futures_cpupool::Builder::new()
                .name_prefix("AudioIPC")
                .pool_size(params.pool_size)
                .stack_size(params.stack_size)
                .create()
        });

        let ctx = Box::new(ClientContext {
            _ops: &CLIENT_OPS as *const _,
            rpc: rpc,
            core: core,
            cpu_pool: cpupool,
        });
        Ok(unsafe { Context::from_ptr(Box::into_raw(ctx) as *mut _) })
    }

    fn backend_id(&mut self) -> &'static CStr {
        assert_not_in_callback();
        unsafe { CStr::from_ptr(b"remote\0".as_ptr() as *const _) }
    }

    fn max_channel_count(&mut self) -> Result<u32> {
        assert_not_in_callback();
        send_recv!(self.rpc(), ContextGetMaxChannelCount => ContextMaxChannelCount())
    }

    fn min_latency(&mut self, params: StreamParams) -> Result<u32> {
        assert_not_in_callback();
        let params = messages::StreamParams::from(params.as_ref());
        send_recv!(self.rpc(), ContextGetMinLatency(params) => ContextMinLatency())
    }

    fn preferred_sample_rate(&mut self) -> Result<u32> {
        assert_not_in_callback();
        send_recv!(self.rpc(), ContextGetPreferredSampleRate => ContextPreferredSampleRate())
    }

    fn enumerate_devices(
        &mut self,
        devtype: DeviceType,
        collection: &DeviceCollectionRef,
    ) -> Result<()> {
        assert_not_in_callback();
        let v: Vec<ffi::cubeb_device_info> = match send_recv!(self.rpc(),
                             ContextGetDeviceEnumeration(devtype.bits()) =>
                             ContextEnumeratedDevices())
        {
            Ok(mut v) => v.drain(..).map(|i| i.into()).collect(),
            Err(e) => return Err(e),
        };
        let mut vs = v.into_boxed_slice();
        let coll = unsafe { &mut *collection.as_ptr() };
        coll.device = vs.as_mut_ptr();
        coll.count = vs.len();
        // Giving away the memory owned by vs.  Don't free it!
        // Reclaimed in `device_collection_destroy`.
        mem::forget(vs);
        Ok(())
    }

    fn device_collection_destroy(&mut self, collection: &mut DeviceCollectionRef) -> Result<()> {
        assert_not_in_callback();
        unsafe {
            let coll = &mut *collection.as_ptr();
            let mut devices = Vec::from_raw_parts(
                coll.device as *mut ffi::cubeb_device_info,
                coll.count,
                coll.count,
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
            coll.device = ptr::null_mut();
            coll.count = 0;
            Ok(())
        }
    }

    fn stream_init(
        &mut self,
        stream_name: Option<&CStr>,
        input_device: DeviceId,
        input_stream_params: Option<&StreamParamsRef>,
        output_device: DeviceId,
        output_stream_params: Option<&StreamParamsRef>,
        latency_frame: u32,
        // These params aren't sent to the server
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Stream> {
        assert_not_in_callback();

        fn opt_stream_params(p: Option<&StreamParamsRef>) -> Option<messages::StreamParams> {
            match p {
                Some(p) => Some(messages::StreamParams::from(p)),
                None => None,
            }
        }

        let stream_name = match stream_name {
            Some(s) => Some(s.to_bytes_with_nul().to_vec()),
            None => None,
        };

        let input_stream_params = opt_stream_params(input_stream_params);
        let output_stream_params = opt_stream_params(output_stream_params);

        let init_params = messages::StreamInitParams {
            stream_name: stream_name,
            input_device: input_device as usize,
            input_stream_params: input_stream_params,
            output_device: output_device as usize,
            output_stream_params: output_stream_params,
            latency_frames: latency_frame,
        };
        stream::init(self, init_params, data_callback, state_callback, user_ptr)
    }

    fn register_device_collection_changed(
        &mut self,
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
        debug!("ClientContext dropped...");
        let _ = send_recv!(self.rpc(), ClientDisconnect => ClientDisconnected);
        unsafe {
            if G_SERVER_FD.is_some() {
                libc::close(super::G_SERVER_FD.take().unwrap());
            }
        }
    }
}

impl fmt::Debug for ClientContext {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("ClientContext")
            .field("_ops", &self._ops)
            .field("rpc", &self.rpc)
            .field("core", &self.core)
            .field("cpu_pool", &"...")
            .finish()
    }
}

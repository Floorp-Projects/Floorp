// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use {assert_not_in_callback, set_in_callback};
use ClientContext;
use audioipc::codec::LengthDelimitedCodec;
use audioipc::frame::{framed, Framed};
use audioipc::messages::{self, CallbackReq, CallbackResp, ClientMessage, ServerMessage};
use audioipc::rpc;
use audioipc::shm::{SharedMemMutSlice, SharedMemSlice};
use cubeb_backend::{ffi, DeviceRef, Error, Result, Stream, StreamOps};
use futures::Future;
use futures_cpupool::{CpuFuture, CpuPool};
use std::ffi::CString;
use std::fs::File;
use std::os::raw::c_void;
use std::os::unix::io::FromRawFd;
use std::os::unix::net;
use std::ptr;
use std::sync::mpsc;
use tokio_uds::UnixStream;

// TODO: Remove and let caller allocate based on cubeb backend requirements.
const SHM_AREA_SIZE: usize = 2 * 1024 * 1024;

pub struct Device(ffi::cubeb_device);

impl Drop for Device {
    fn drop(&mut self) {
        unsafe {
            if !self.0.input_name.is_null() {
                let _ = CString::from_raw(self.0.input_name as *mut _);
            }
            if !self.0.output_name.is_null() {
                let _ = CString::from_raw(self.0.output_name as *mut _);
            }
        }
    }
}

// ClientStream's layout *must* match cubeb.c's `struct cubeb_stream` for the
// common fields.
#[repr(C)]
#[derive(Debug)]
pub struct ClientStream<'ctx> {
    // This must be a reference to Context for cubeb, cubeb accesses
    // stream methods via stream->context->ops
    context: &'ctx ClientContext,
    user_ptr: *mut c_void,
    token: usize,
}

struct CallbackServer {
    input_shm: SharedMemSlice,
    output_shm: SharedMemMutSlice,
    data_cb: ffi::cubeb_data_callback,
    state_cb: ffi::cubeb_state_callback,
    user_ptr: usize,
    cpu_pool: CpuPool,
}

impl rpc::Server for CallbackServer {
    type Request = CallbackReq;
    type Response = CallbackResp;
    type Future = CpuFuture<Self::Response, ()>;
    type Transport = Framed<UnixStream, LengthDelimitedCodec<Self::Response, Self::Request>>;

    fn process(&mut self, req: Self::Request) -> Self::Future {
        match req {
            CallbackReq::Data(nframes, frame_size) => {
                trace!(
                    "stream_thread: Data Callback: nframes={} frame_size={}",
                    nframes,
                    frame_size
                );

                // Clone values that need to be moved into the cpu pool thread.
                let input_shm = unsafe { self.input_shm.clone_view() };
                let mut output_shm = unsafe { self.output_shm.clone_view() };
                let user_ptr = self.user_ptr;
                let cb = self.data_cb.unwrap();

                self.cpu_pool.spawn_fn(move || {
                    // TODO: This is proof-of-concept. Make it better.
                    let input_ptr: *const u8 = input_shm
                        .get_slice(nframes as usize * frame_size)
                        .unwrap()
                        .as_ptr();
                    let output_ptr: *mut u8 = output_shm
                        .get_mut_slice(nframes as usize * frame_size)
                        .unwrap()
                        .as_mut_ptr();

                    set_in_callback(true);
                    let nframes = unsafe {
                        cb(
                            ptr::null_mut(),
                            user_ptr as *mut c_void,
                            input_ptr as *const _,
                            output_ptr as *mut _,
                            nframes as _,
                        )
                    };
                    set_in_callback(false);

                    Ok(CallbackResp::Data(nframes as isize))
                })
            }
            CallbackReq::State(state) => {
                trace!("stream_thread: State Callback: {:?}", state);
                let user_ptr = self.user_ptr;
                let cb = self.state_cb.unwrap();
                self.cpu_pool.spawn_fn(move || {
                    set_in_callback(true);
                    unsafe {
                        cb(ptr::null_mut(), user_ptr as *mut _, state);
                    }
                    set_in_callback(false);

                    Ok(CallbackResp::State)
                })
            }
        }
    }
}

impl<'ctx> ClientStream<'ctx> {
    fn init(
        ctx: &'ctx ClientContext,
        init_params: messages::StreamInitParams,
        data_callback: ffi::cubeb_data_callback,
        state_callback: ffi::cubeb_state_callback,
        user_ptr: *mut c_void,
    ) -> Result<Stream> {
        assert_not_in_callback();

        let rpc = ctx.rpc();
        let data = try!(send_recv!(rpc, StreamInit(init_params) => StreamCreated()));

        debug!("token = {}, fds = {:?}", data.token, data.fds);

        let stm = data.fds[0];
        let stream = unsafe { net::UnixStream::from_raw_fd(stm) };

        let input = data.fds[1];
        let input_file = unsafe { File::from_raw_fd(input) };
        let input_shm = SharedMemSlice::from(&input_file, SHM_AREA_SIZE).unwrap();

        let output = data.fds[2];
        let output_file = unsafe { File::from_raw_fd(output) };
        let output_shm = SharedMemMutSlice::from(&output_file, SHM_AREA_SIZE).unwrap();

        let user_data = user_ptr as usize;

        let cpu_pool = ctx.cpu_pool();

        let server = CallbackServer {
            input_shm: input_shm,
            output_shm: output_shm,
            data_cb: data_callback,
            state_cb: state_callback,
            user_ptr: user_data,
            cpu_pool: cpu_pool,
        };

        let (wait_tx, wait_rx) = mpsc::channel();
        ctx.remote().spawn(move |handle| {
            let stream = UnixStream::from_stream(stream, handle).unwrap();
            let transport = framed(stream, Default::default());
            rpc::bind_server(transport, server, handle);
            wait_tx.send(()).unwrap();
            Ok(())
        });
        wait_rx.recv().unwrap();

        let stream = Box::into_raw(Box::new(ClientStream {
            context: ctx,
            user_ptr: user_ptr,
            token: data.token,
        }));
        Ok(unsafe { Stream::from_ptr(stream as *mut _) })
    }
}

impl<'ctx> Drop for ClientStream<'ctx> {
    fn drop(&mut self) {
        debug!("ClientStream dropped...");
        let rpc = self.context.rpc();
        let _ = send_recv!(rpc, StreamDestroy(self.token) => StreamDestroyed);
    }
}

impl<'ctx> StreamOps for ClientStream<'ctx> {
    fn start(&mut self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamStart(self.token) => StreamStarted)
    }

    fn stop(&mut self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamStop(self.token) => StreamStopped)
    }

    fn reset_default_device(&mut self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamResetDefaultDevice(self.token) => StreamDefaultDeviceReset)
    }

    fn position(&mut self) -> Result<u64> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamGetPosition(self.token) => StreamPosition())
    }

    fn latency(&mut self) -> Result<u32> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamGetLatency(self.token) => StreamLatency())
    }

    fn set_volume(&mut self, volume: f32) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetVolume(self.token, volume) => StreamVolumeSet)
    }

    fn set_panning(&mut self, panning: f32) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetPanning(self.token, panning) => StreamPanningSet)
    }

    fn current_device(&mut self) -> Result<&DeviceRef> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        match send_recv!(rpc, StreamGetCurrentDevice(self.token) => StreamCurrentDevice()) {
            Ok(d) => Ok(unsafe { DeviceRef::from_ptr(Box::into_raw(Box::new(d.into()))) }),
            Err(e) => Err(e),
        }
    }

    fn device_destroy(&mut self, device: &DeviceRef) -> Result<()> {
        assert_not_in_callback();
        // It's all unsafe...
        if device.as_ptr().is_null() {
            Err(Error::error())
        } else {
            unsafe {
                let _: Box<Device> = Box::from_raw(device.as_ptr() as *mut _);
            }
            Ok(())
        }
    }

    // TODO: How do we call this back? On what thread?
    fn register_device_changed_callback(
        &mut self,
        _device_changed_callback: ffi::cubeb_device_changed_callback,
    ) -> Result<()> {
        assert_not_in_callback();
        Ok(())
    }
}

pub fn init(
    ctx: &ClientContext,
    init_params: messages::StreamInitParams,
    data_callback: ffi::cubeb_data_callback,
    state_callback: ffi::cubeb_state_callback,
    user_ptr: *mut c_void,
) -> Result<Stream> {
    let stm = try!(ClientStream::init(
        ctx,
        init_params,
        data_callback,
        state_callback,
        user_ptr
    ));
    debug_assert_eq!(stm.user_ptr(), user_ptr);
    Ok(stm)
}

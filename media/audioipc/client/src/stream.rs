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
use cubeb_backend::Stream;
use cubeb_core::{ffi, Result};
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

pub struct ClientStream<'ctx> {
    // This must be a reference to Context for cubeb, cubeb accesses stream methods via stream->context->ops
    context: &'ctx ClientContext,
    token: usize
}

struct CallbackServer {
    input_shm: SharedMemSlice,
    output_shm: SharedMemMutSlice,
    data_cb: ffi::cubeb_data_callback,
    state_cb: ffi::cubeb_state_callback,
    user_ptr: usize,
    cpu_pool: CpuPool
}

impl rpc::Server for CallbackServer {
    type Request = CallbackReq;
    type Response = CallbackResp;
    type Future = CpuFuture<Self::Response, ()>;
    type Transport = Framed<UnixStream, LengthDelimitedCodec<Self::Response, Self::Request>>;

    fn process(&mut self, req: Self::Request) -> Self::Future {
        match req {
            CallbackReq::Data(nframes, frame_size) => {
                info!(
                    "stream_thread: Data Callback: nframes={} frame_size={}",
                    nframes,
                    frame_size
                );

                // Clone values that need to be moved into the cpu pool thread.
                let input_shm = unsafe { self.input_shm.clone_view() };
                let mut output_shm = unsafe { self.output_shm.clone_view() };
                let user_ptr = self.user_ptr;
                let cb = self.data_cb;

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
                    let nframes = cb(
                        ptr::null_mut(),
                        user_ptr as *mut c_void,
                        input_ptr as *const _,
                        output_ptr as *mut _,
                        nframes as _
                    );
                    set_in_callback(false);

                    Ok(CallbackResp::Data(nframes as isize))
                })
            },
            CallbackReq::State(state) => {
                info!("stream_thread: State Callback: {:?}", state);
                let user_ptr = self.user_ptr;
                let cb = self.state_cb;
                self.cpu_pool.spawn_fn(move || {
                    set_in_callback(true);
                    cb(ptr::null_mut(), user_ptr as *mut _, state);
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
        user_ptr: *mut c_void
    ) -> Result<*mut ffi::cubeb_stream> {
        assert_not_in_callback();

        let rpc = ctx.rpc();
        let data = try!(send_recv!(rpc, StreamInit(init_params) => StreamCreated()));

        trace!("token = {}, fds = {:?}", data.token, data.fds);

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
            cpu_pool: cpu_pool
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

        Ok(Box::into_raw(Box::new(ClientStream {
            context: ctx,
            token: data.token
        })) as _)
    }
}

impl<'ctx> Drop for ClientStream<'ctx> {
    fn drop(&mut self) {
        trace!("ClientStream drop...");
        let rpc = self.context.rpc();
        let _ = send_recv!(rpc, StreamDestroy(self.token) => StreamDestroyed);
    }
}

impl<'ctx> Stream for ClientStream<'ctx> {
    fn start(&self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamStart(self.token) => StreamStarted)
    }

    fn stop(&self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamStop(self.token) => StreamStopped)
    }

    fn reset_default_device(&self) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamResetDefaultDevice(self.token) => StreamDefaultDeviceReset)
    }

    fn position(&self) -> Result<u64> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamGetPosition(self.token) => StreamPosition())
    }

    fn latency(&self) -> Result<u32> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamGetLatency(self.token) => StreamLatency())
    }

    fn set_volume(&self, volume: f32) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetVolume(self.token, volume) => StreamVolumeSet)
    }

    fn set_panning(&self, panning: f32) -> Result<()> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        send_recv!(rpc, StreamSetPanning(self.token, panning) => StreamPanningSet)
    }

    fn current_device(&self) -> Result<*const ffi::cubeb_device> {
        assert_not_in_callback();
        let rpc = self.context.rpc();
        match send_recv!(rpc, StreamGetCurrentDevice(self.token) => StreamCurrentDevice()) {
            Ok(d) => Ok(Box::into_raw(Box::new(d.into()))),
            Err(e) => Err(e)
        }
    }

    fn device_destroy(&self, device: *const ffi::cubeb_device) -> Result<()> {
        assert_not_in_callback();
        // It's all unsafe...
        if !device.is_null() {
            unsafe {
                if !(*device).output_name.is_null() {
                    let _ = CString::from_raw((*device).output_name as *mut _);
                }
                if !(*device).input_name.is_null() {
                    let _ = CString::from_raw((*device).input_name as *mut _);
                }
                let _: Box<ffi::cubeb_device> = Box::from_raw(device as *mut _);
            }
        }
        Ok(())
    }

    // TODO: How do we call this back? On what thread?
    fn register_device_changed_callback(
        &self,
        _device_changed_callback: ffi::cubeb_device_changed_callback
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
    user_ptr: *mut c_void
) -> Result<*mut ffi::cubeb_stream> {
    ClientStream::init(ctx, init_params, data_callback, state_callback, user_ptr)
}

#[macro_use]
extern crate error_chain;

#[macro_use]
extern crate log;

extern crate audioipc;
extern crate bytes;
extern crate cubeb_core as cubeb;
extern crate futures;
extern crate lazycell;
extern crate libc;
extern crate slab;
extern crate tokio_core;
extern crate tokio_uds;

use audioipc::codec::LengthDelimitedCodec;
use audioipc::core;
use audioipc::fd_passing::{framed_with_fds, FramedWithFds};
use audioipc::frame::{framed, Framed};
use audioipc::messages::{CallbackReq, CallbackResp, ClientMessage, Device, DeviceInfo,
                         ServerMessage, StreamCreate, StreamInitParams, StreamParams};
use audioipc::rpc;
use audioipc::shm::{SharedMemReader, SharedMemWriter};
use cubeb::ffi;
use futures::future::{self, FutureResult};
use futures::sync::oneshot;
use futures::Future;
use std::cell::RefCell;
use std::convert::From;
use std::error::Error;
use std::ffi::{CStr, CString};
use std::mem::{size_of, ManuallyDrop};
use std::os::raw::{c_long, c_void};
use std::os::unix::net;
use std::os::unix::prelude::*;
use std::{panic, ptr, slice};
use tokio_core::reactor::Remote;
use tokio_uds::UnixStream;

pub mod errors {
    error_chain! {
        links {
            AudioIPC(::audioipc::errors::Error, ::audioipc::errors::ErrorKind);
        }
        foreign_links {
            Cubeb(::cubeb::Error);
            Io(::std::io::Error);
            Canceled(::futures::sync::oneshot::Canceled);
        }
    }
}

use errors::*;

type ContextKey = RefCell<Option<cubeb::Result<cubeb::Context>>>;
thread_local!(static CONTEXT_KEY:ContextKey = RefCell::new(None));

fn with_local_context<T, F>(f: F) -> T
where
    F: FnOnce(&cubeb::Result<cubeb::Context>) -> T,
{
    CONTEXT_KEY.with(|k| {
        let mut context = k.borrow_mut();
        if context.is_none() {
            let name = CString::new("AudioIPC Server").unwrap();
            *context = Some(cubeb::Context::init(Some(name.as_c_str()), None));
        }
        f(context.as_ref().unwrap())
    })
}

// TODO: Remove and let caller allocate based on cubeb backend requirements.
const SHM_AREA_SIZE: usize = 2 * 1024 * 1024;

// The size in which the stream slab is grown.
const STREAM_CONN_CHUNK_SIZE: usize = 64;

struct CallbackClient;

impl rpc::Client for CallbackClient {
    type Request = CallbackReq;
    type Response = CallbackResp;
    type Transport = Framed<UnixStream, LengthDelimitedCodec<Self::Request, Self::Response>>;
}

struct ServerStreamCallbacks {
    /// Size of input frame in bytes
    input_frame_size: u16,
    /// Size of output frame in bytes
    output_frame_size: u16,
    /// Shared memory buffer for sending input data to client
    input_shm: SharedMemWriter,
    /// Shared memory buffer for receiving output data from client
    output_shm: SharedMemReader,
    /// RPC interface to callback server running in client
    rpc: rpc::ClientProxy<CallbackReq, CallbackResp>,
}

impl ServerStreamCallbacks {
    fn data_callback(&mut self, input: &[u8], output: &mut [u8]) -> isize {
        trace!("Stream data callback: {} {}", input.len(), output.len());

        // len is of input and output is frame len. Turn these into the real lengths.
        let real_input = unsafe {
            let nbytes = input.len() * self.input_frame_size as usize;
            slice::from_raw_parts(input.as_ptr(), nbytes)
        };

        self.input_shm.write(real_input).unwrap();

        let r = self.rpc
            .call(CallbackReq::Data(
                output.len() as isize,
                self.output_frame_size as usize,
            ))
            .wait();

        match r {
            Ok(CallbackResp::Data(frames)) => {
                if frames >= 0 {
                    let nbytes = frames as usize * self.output_frame_size as usize;
                    let real_output = unsafe {
                        trace!("Resize output to {}", nbytes);
                        slice::from_raw_parts_mut(output.as_mut_ptr(), nbytes)
                    };
                    self.output_shm.read(&mut real_output[..nbytes]).unwrap();
                }
                frames
            }
            _ => {
                debug!("Unexpected message {:?} during data_callback", r);
                -1
            }
        }
    }

    fn state_callback(&mut self, state: cubeb::State) {
        trace!("Stream state callback: {:?}", state);
        let r = self.rpc.call(CallbackReq::State(state.into())).wait();
        match r {
            Ok(CallbackResp::State) => {}
            _ => {
                debug!("Unexpected message {:?} during callback", r);
            }
        }
    }
}

struct ServerStream {
    stream: ManuallyDrop<cubeb::Stream>,
    cbs: ManuallyDrop<Box<ServerStreamCallbacks>>,
}

impl Drop for ServerStream {
    fn drop(&mut self) {
        unsafe {
            ManuallyDrop::drop(&mut self.stream);
            ManuallyDrop::drop(&mut self.cbs);
        }
    }
}

type StreamSlab = slab::Slab<ServerStream, usize>;

pub struct CubebServer {
    cb_remote: Remote,
    streams: StreamSlab,
}

impl rpc::Server for CubebServer {
    type Request = ServerMessage;
    type Response = ClientMessage;
    type Future = FutureResult<Self::Response, ()>;
    type Transport = FramedWithFds<UnixStream, LengthDelimitedCodec<Self::Response, Self::Request>>;

    fn process(&mut self, req: Self::Request) -> Self::Future {
        let resp = with_local_context(|context| match *context {
            Err(_) => error(cubeb::Error::error()),
            Ok(ref context) => self.process_msg(context, &req),
        });
        future::ok(resp)
    }
}

impl CubebServer {
    pub fn new(cb_remote: Remote) -> Self {
        CubebServer {
            cb_remote: cb_remote,
            streams: StreamSlab::with_capacity(STREAM_CONN_CHUNK_SIZE),
        }
    }

    // Process a request coming from the client.
    fn process_msg(&mut self, context: &cubeb::Context, msg: &ServerMessage) -> ClientMessage {
        let resp: ClientMessage = match *msg {
            ServerMessage::ClientConnect => panic!("already connected"),

            ServerMessage::ClientDisconnect => {
                // TODO:
                //self.connection.client_disconnect();
                ClientMessage::ClientDisconnected
            }

            ServerMessage::ContextGetBackendId => ClientMessage::ContextBackendId(),

            ServerMessage::ContextGetMaxChannelCount => context
                .max_channel_count()
                .map(ClientMessage::ContextMaxChannelCount)
                .unwrap_or_else(error),

            ServerMessage::ContextGetMinLatency(ref params) => {
                let format = cubeb::SampleFormat::from(params.format);
                let layout = cubeb::ChannelLayout::from(params.layout);

                let params = cubeb::StreamParamsBuilder::new()
                    .format(format)
                    .rate(u32::from(params.rate))
                    .channels(u32::from(params.channels))
                    .layout(layout)
                    .take();

                context
                    .min_latency(&params)
                    .map(ClientMessage::ContextMinLatency)
                    .unwrap_or_else(error)
            }

            ServerMessage::ContextGetPreferredSampleRate => context
                .preferred_sample_rate()
                .map(ClientMessage::ContextPreferredSampleRate)
                .unwrap_or_else(error),

            ServerMessage::ContextGetDeviceEnumeration(device_type) => context
                .enumerate_devices(cubeb::DeviceType::from_bits_truncate(device_type))
                .map(|devices| {
                    let v: Vec<DeviceInfo> = devices.iter().map(|i| i.as_ref().into()).collect();
                    ClientMessage::ContextEnumeratedDevices(v)
                })
                .unwrap_or_else(error),

            ServerMessage::StreamInit(ref params) => self.process_stream_init(context, params)
                .unwrap_or_else(|_| error(cubeb::Error::error())),

            ServerMessage::StreamDestroy(stm_tok) => {
                self.streams.remove(stm_tok);
                ClientMessage::StreamDestroyed
            }

            ServerMessage::StreamStart(stm_tok) => self.streams[stm_tok]
                .stream
                .start()
                .map(|_| ClientMessage::StreamStarted)
                .unwrap_or_else(error),

            ServerMessage::StreamStop(stm_tok) => self.streams[stm_tok]
                .stream
                .stop()
                .map(|_| ClientMessage::StreamStopped)
                .unwrap_or_else(error),

            ServerMessage::StreamResetDefaultDevice(stm_tok) => self.streams[stm_tok]
                .stream
                .reset_default_device()
                .map(|_| ClientMessage::StreamDefaultDeviceReset)
                .unwrap_or_else(error),

            ServerMessage::StreamGetPosition(stm_tok) => self.streams[stm_tok]
                .stream
                .position()
                .map(ClientMessage::StreamPosition)
                .unwrap_or_else(error),

            ServerMessage::StreamGetLatency(stm_tok) => self.streams[stm_tok]
                .stream
                .latency()
                .map(ClientMessage::StreamLatency)
                .unwrap_or_else(error),

            ServerMessage::StreamSetVolume(stm_tok, volume) => self.streams[stm_tok]
                .stream
                .set_volume(volume)
                .map(|_| ClientMessage::StreamVolumeSet)
                .unwrap_or_else(error),

            ServerMessage::StreamSetPanning(stm_tok, panning) => self.streams[stm_tok]
                .stream
                .set_panning(panning)
                .map(|_| ClientMessage::StreamPanningSet)
                .unwrap_or_else(error),

            ServerMessage::StreamGetCurrentDevice(stm_tok) => self.streams[stm_tok]
                .stream
                .current_device()
                .map(|device| ClientMessage::StreamCurrentDevice(Device::from(device)))
                .unwrap_or_else(error),
        };

        trace!("process_msg: req={:?}, resp={:?}", msg, resp);

        resp
    }

    // Stream init is special, so it's been separated from process_msg.
    fn process_stream_init(
        &mut self,
        context: &cubeb::Context,
        params: &StreamInitParams,
    ) -> Result<ClientMessage> {
        fn frame_size_in_bytes(params: Option<&StreamParams>) -> u16 {
            params
                .map(|p| {
                    let format = p.format.into();
                    let sample_size = match format {
                        cubeb::SampleFormat::S16LE
                        | cubeb::SampleFormat::S16BE
                        | cubeb::SampleFormat::S16NE => 2,
                        cubeb::SampleFormat::Float32LE
                        | cubeb::SampleFormat::Float32BE
                        | cubeb::SampleFormat::Float32NE => 4,
                    };
                    let channel_count = p.channels as u16;
                    sample_size * channel_count
                })
                .unwrap_or(0u16)
        }

        // Create the callback handling struct which is attached the cubeb stream.
        let input_frame_size = frame_size_in_bytes(params.input_stream_params.as_ref());
        let output_frame_size = frame_size_in_bytes(params.output_stream_params.as_ref());

        let (stm1, stm2) = net::UnixStream::pair()?;
        debug!("Created callback pair: {:?}-{:?}", stm1, stm2);
        let (input_shm, input_file) =
            SharedMemWriter::new(&audioipc::get_shm_path("input"), SHM_AREA_SIZE)?;
        let (output_shm, output_file) =
            SharedMemReader::new(&audioipc::get_shm_path("output"), SHM_AREA_SIZE)?;

        // This code is currently running on the Client/Server RPC
        // handling thread.  We need to move the registration of the
        // bind_client to the callback RPC handling thread.  This is
        // done by spawning a future on cb_remote.

        let id = core::handle().id();

        let (tx, rx) = oneshot::channel();
        self.cb_remote.spawn(move |handle| {
            // Ensure we're running on a loop different to the one
            // invoking spawn_fn.
            assert_ne!(id, handle.id());
            let stream = UnixStream::from_stream(stm2, handle).unwrap();
            let transport = framed(stream, Default::default());
            let rpc = rpc::bind_client::<CallbackClient>(transport, handle);
            drop(tx.send(rpc));
            Ok(())
        });

        let rpc: rpc::ClientProxy<CallbackReq, CallbackResp> = match rx.wait() {
            Ok(rpc) => rpc,
            Err(_) => bail!("Failed to create callback rpc."),
        };

        let cbs = Box::new(ServerStreamCallbacks {
            input_frame_size,
            output_frame_size,
            input_shm,
            output_shm,
            rpc,
        });

        // Create cubeb stream from params
        let stream_name = params
            .stream_name
            .as_ref()
            .and_then(|name| CStr::from_bytes_with_nul(name).ok());

        let input_device = params.input_device as *const _;
        let input_stream_params = params.input_stream_params.as_ref().map(|isp| unsafe {
            cubeb::StreamParamsRef::from_ptr(isp as *const StreamParams as *mut _)
        });

        let output_device = params.output_device as *const _;
        let output_stream_params = params.output_stream_params.as_ref().map(|osp| unsafe {
            cubeb::StreamParamsRef::from_ptr(osp as *const StreamParams as *mut _)
        });

        let latency = params.latency_frames;
        assert!(size_of::<Box<ServerStreamCallbacks>>() == size_of::<usize>());
        let user_ptr = cbs.as_ref() as *const ServerStreamCallbacks as *mut c_void;

        unsafe {
            context
                .stream_init(
                    stream_name,
                    input_device,
                    input_stream_params,
                    output_device,
                    output_stream_params,
                    latency,
                    Some(data_cb_c),
                    Some(state_cb_c),
                    user_ptr,
                )
                .and_then(|stream| {
                    if !self.streams.has_available() {
                        trace!(
                            "server connection ran out of stream slots. reserving {} more.",
                            STREAM_CONN_CHUNK_SIZE
                        );
                        self.streams.reserve_exact(STREAM_CONN_CHUNK_SIZE);
                    }

                    let stm_tok = match self.streams.vacant_entry() {
                        Some(entry) => {
                            debug!("Registering stream {:?}", entry.index(),);

                            entry
                                .insert(ServerStream {
                                    stream: ManuallyDrop::new(stream),
                                    cbs: ManuallyDrop::new(cbs),
                                })
                                .index()
                        }
                        None => {
                            // TODO: Turn into error
                            panic!("Failed to insert stream into slab. No entries")
                        }
                    };

                    Ok(ClientMessage::StreamCreated(StreamCreate {
                        token: stm_tok,
                        fds: [
                            stm1.into_raw_fd(),
                            input_file.into_raw_fd(),
                            output_file.into_raw_fd(),
                        ],
                    }))
                })
                .map_err(|e| e.into())
        }
    }
}

struct ServerWrapper {
    core_thread: core::CoreThread,
    callback_thread: core::CoreThread,
}

fn run() -> Result<ServerWrapper> {
    trace!("Starting up cubeb audio server event loop thread...");

    let callback_thread = try!(
        core::spawn_thread("AudioIPC Callback RPC", || {
            trace!("Starting up cubeb audio callback event loop thread...");
            Ok(())
        }).or_else(|e| {
            debug!(
                "Failed to start cubeb audio callback event loop thread: {:?}",
                e.description()
            );
            Err(e)
        })
    );

    let core_thread = try!(
        core::spawn_thread("AudioIPC Server RPC", move || Ok(())).or_else(|e| {
            debug!(
                "Failed to cubeb audio core event loop thread: {:?}",
                e.description()
            );
            Err(e)
        })
    );

    Ok(ServerWrapper {
        core_thread: core_thread,
        callback_thread: callback_thread,
    })
}

#[no_mangle]
pub extern "C" fn audioipc_server_start() -> *mut c_void {
    match run() {
        Ok(server) => Box::into_raw(Box::new(server)) as *mut _,
        Err(_) => ptr::null_mut() as *mut _,
    }
}

#[no_mangle]
pub extern "C" fn audioipc_server_new_client(p: *mut c_void) -> libc::c_int {
    let (wait_tx, wait_rx) = oneshot::channel();
    let wrapper: &ServerWrapper = unsafe { &*(p as *mut _) };

    let cb_remote = wrapper.callback_thread.remote();

    // We create a pair of connected unix domain sockets. One socket is
    // registered with the reactor core, the other is returned to the
    // caller.
    net::UnixStream::pair()
        .and_then(|(sock1, sock2)| {
            // Spawn closure to run on same thread as reactor::Core
            // via remote handle.
            wrapper.core_thread.remote().spawn(|handle| {
                trace!("Incoming connection");
                UnixStream::from_stream(sock2, handle)
                    .and_then(|sock| {
                        let transport = framed_with_fds(sock, Default::default());
                        rpc::bind_server(transport, CubebServer::new(cb_remote), handle);
                        Ok(())
                    })
                    .map_err(|_| ())
                    // Notify waiting thread that sock2 has been registered.
                    .and_then(|_| wait_tx.send(()))
            });
            // Wait for notification that sock2 has been registered
            // with reactor::Core.
            let _ = wait_rx.wait();
            Ok(sock1.into_raw_fd())
        })
        .unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn audioipc_server_stop(p: *mut c_void) {
    let wrapper = unsafe { Box::<ServerWrapper>::from_raw(p as *mut _) };
    drop(wrapper);
}

fn error(error: cubeb::Error) -> ClientMessage {
    ClientMessage::Error(error.raw_code())
}

// C callable callbacks
unsafe extern "C" fn data_cb_c(
    _: *mut ffi::cubeb_stream,
    user_ptr: *mut c_void,
    input_buffer: *const c_void,
    output_buffer: *mut c_void,
    nframes: c_long,
) -> c_long {
    let ok = panic::catch_unwind(|| {
        let cbs = &mut *(user_ptr as *mut ServerStreamCallbacks);
        let input = if input_buffer.is_null() {
            &[]
        } else {
            slice::from_raw_parts(input_buffer as *const u8, nframes as usize)
        };
        let output: &mut [u8] = if output_buffer.is_null() {
            &mut []
        } else {
            slice::from_raw_parts_mut(output_buffer as *mut u8, nframes as usize)
        };
        cbs.data_callback(input, output) as c_long
    });
    ok.unwrap_or(0)
}

unsafe extern "C" fn state_cb_c(
    _: *mut ffi::cubeb_stream,
    user_ptr: *mut c_void,
    state: ffi::cubeb_state,
) {
    let ok = panic::catch_unwind(|| {
        let state = cubeb::State::from(state);
        let cbs = &mut *(user_ptr as *mut ServerStreamCallbacks);
        cbs.state_callback(state);
    });
    ok.expect("State callback panicked");
}

// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

#[cfg(target_os = "linux")]
use audio_thread_priority::{promote_thread_to_real_time, RtPriorityThreadInfo};
use audioipc;
use audioipc::codec::LengthDelimitedCodec;
use audioipc::frame::{framed, Framed};
use audioipc::messages::{
    CallbackReq, CallbackResp, ClientMessage, Device, DeviceCollectionReq, DeviceCollectionResp,
    DeviceInfo, RegisterDeviceCollectionChanged, ServerMessage, StreamCreate, StreamInitParams,
    StreamParams,
};
use audioipc::platformhandle_passing::FramedWithPlatformHandles;
use audioipc::rpc;
use audioipc::shm::{SharedMemReader, SharedMemWriter};
use audioipc::{MessageStream, PlatformHandle};
use cubeb_core as cubeb;
use cubeb_core::ffi;
use futures::future::{self, FutureResult};
use futures::sync::oneshot;
use futures::Future;
use slab;
use std::cell::RefCell;
use std::convert::From;
use std::ffi::CStr;
use std::mem::{size_of, ManuallyDrop};
use std::os::raw::{c_long, c_void};
use std::rc::Rc;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::{panic, slice};
use tokio::reactor;
use tokio::runtime::current_thread;

use crate::errors::*;

fn error(error: cubeb::Error) -> ClientMessage {
    ClientMessage::Error(error.raw_code())
}

struct CubebDeviceCollectionManager {
    servers: Vec<Rc<RefCell<CubebServerCallbacks>>>,
    devtype: cubeb::DeviceType,
}

impl CubebDeviceCollectionManager {
    fn new() -> CubebDeviceCollectionManager {
        CubebDeviceCollectionManager {
            servers: Vec::new(),
            devtype: cubeb::DeviceType::empty(),
        }
    }

    fn register(
        &mut self,
        context: &cubeb::Context,
        server: &Rc<RefCell<CubebServerCallbacks>>,
    ) -> cubeb::Result<()> {
        if self
            .servers
            .iter()
            .find(|s| Rc::ptr_eq(s, server))
            .is_none()
        {
            self.servers.push(server.clone());
        }
        self.update(context)
    }

    fn unregister(
        &mut self,
        context: &cubeb::Context,
        server: &Rc<RefCell<CubebServerCallbacks>>,
    ) -> cubeb::Result<()> {
        self.servers
            .retain(|s| !(Rc::ptr_eq(&s, server) && s.borrow().devtype.is_empty()));
        self.update(context)
    }

    fn update(&mut self, context: &cubeb::Context) -> cubeb::Result<()> {
        let mut devtype = cubeb::DeviceType::empty();
        for s in &self.servers {
            devtype |= s.borrow().devtype;
        }
        for &dir in &[cubeb::DeviceType::INPUT, cubeb::DeviceType::OUTPUT] {
            if devtype.contains(dir) != self.devtype.contains(dir) {
                self.internal_register(context, dir, devtype.contains(dir))?;
            }
        }
        Ok(())
    }

    fn internal_register(
        &mut self,
        context: &cubeb::Context,
        devtype: cubeb::DeviceType,
        enable: bool,
    ) -> cubeb::Result<()> {
        let user_ptr = if enable {
            self as *const CubebDeviceCollectionManager as *mut c_void
        } else {
            std::ptr::null_mut()
        };
        for &(dir, cb) in &[
            (
                cubeb::DeviceType::INPUT,
                device_collection_changed_input_cb_c as _,
            ),
            (
                cubeb::DeviceType::OUTPUT,
                device_collection_changed_output_cb_c as _,
            ),
        ] {
            if devtype.contains(dir) {
                assert_eq!(self.devtype.contains(dir), !enable);
                unsafe {
                    context.register_device_collection_changed(
                        dir,
                        if enable { Some(cb) } else { None },
                        user_ptr,
                    )?;
                }
                if enable {
                    self.devtype.insert(dir);
                } else {
                    self.devtype.remove(dir);
                }
            }
        }
        Ok(())
    }

    // Warning: this is called from an internal cubeb thread, so we must not mutate unprotected shared state.
    unsafe fn device_collection_changed_callback(&self, device_type: ffi::cubeb_device_type) {
        self.servers.iter().for_each(|server| {
            if server
                .borrow()
                .devtype
                .contains(cubeb::DeviceType::from_bits_truncate(device_type))
            {
                server
                    .borrow_mut()
                    .device_collection_changed_callback(device_type)
            }
        });
    }
}

struct DevIdMap {
    devices: Vec<usize>,
}

// A cubeb_devid is an opaque type which may be implemented with a stable
// pointer in a cubeb backend.  cubeb_devids received remotely must be
// validated before use, so DevIdMap provides a simple 1:1 mapping between a
// cubeb_devid and an IPC-transportable value suitable for use as a unique
// handle.
impl DevIdMap {
    fn new() -> DevIdMap {
        let mut d = DevIdMap {
            devices: Vec::with_capacity(32),
        };
        // A null cubeb_devid is used for selecting the default device.
        // Pre-populate the mapping with 0 -> 0 to handle nulls.
        d.devices.push(0);
        d
    }

    // Given a cubeb_devid, return a unique stable value suitable for use
    // over IPC.
    fn to_handle(&mut self, devid: usize) -> usize {
        if let Some(i) = self.devices.iter().position(|&d| d == devid) {
            return i;
        }
        self.devices.push(devid);
        self.devices.len() - 1
    }

    // Given a handle produced by `to_handle`, return the associated
    // cubeb_devid.  Invalid handles result in a panic.
    fn from_handle(&self, handle: usize) -> usize {
        self.devices[handle]
    }
}

struct CubebContextState {
    context: cubeb::Result<cubeb::Context>,
    manager: CubebDeviceCollectionManager,
}

thread_local!(static CONTEXT_KEY: RefCell<Option<CubebContextState>> = RefCell::new(None));

fn with_local_context<T, F>(f: F) -> T
where
    F: FnOnce(&cubeb::Result<cubeb::Context>, &mut CubebDeviceCollectionManager) -> T,
{
    CONTEXT_KEY.with(|k| {
        let mut state = k.borrow_mut();
        if state.is_none() {
            let params = super::G_CUBEB_CONTEXT_PARAMS.lock().unwrap();
            let context_name = Some(params.context_name.as_c_str());
            let backend_name = if let Some(ref name) = params.backend_name {
                Some(name.as_c_str())
            } else {
                None
            };
            audioipc::server_platform_init();
            let context = cubeb::Context::init(context_name, backend_name);
            let manager = CubebDeviceCollectionManager::new();
            *state = Some(CubebContextState { context, manager });
        }
        let state = state.as_mut().unwrap();
        f(&state.context, &mut state.manager)
    })
}

// The size in which the stream slab is grown.
const STREAM_CONN_CHUNK_SIZE: usize = 64;

struct DeviceCollectionClient;

impl rpc::Client for DeviceCollectionClient {
    type Request = DeviceCollectionReq;
    type Response = DeviceCollectionResp;
    type Transport =
        Framed<audioipc::AsyncMessageStream, LengthDelimitedCodec<Self::Request, Self::Response>>;
}

struct CallbackClient;

impl rpc::Client for CallbackClient {
    type Request = CallbackReq;
    type Response = CallbackResp;
    type Transport =
        Framed<audioipc::AsyncMessageStream, LengthDelimitedCodec<Self::Request, Self::Response>>;
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
    fn data_callback(&mut self, input: &[u8], output: &mut [u8], nframes: isize) -> isize {
        trace!(
            "Stream data callback: {} {} {}",
            nframes,
            input.len(),
            output.len()
        );

        self.input_shm.write(input).unwrap();

        let r = self
            .rpc
            .call(CallbackReq::Data {
                nframes,
                input_frame_size: self.input_frame_size as usize,
                output_frame_size: self.output_frame_size as usize,
            })
            .wait();

        match r {
            Ok(CallbackResp::Data(frames)) => {
                if frames >= 0 {
                    let nbytes = frames as usize * self.output_frame_size as usize;
                    trace!("Reslice output to {}", nbytes);
                    self.output_shm.read(&mut output[..nbytes]).unwrap();
                }
                frames
            }
            _ => {
                debug!("Unexpected message {:?} during data_callback", r);
                // TODO: Return a CUBEB_ERROR result here once
                // https://github.com/kinetiknz/cubeb/issues/553 is
                // fixed.
                0
            }
        }
    }

    fn state_callback(&mut self, state: cubeb::State) {
        trace!("Stream state callback: {:?}", state);
        let r = self.rpc.call(CallbackReq::State(state.into())).wait();
        match r {
            Ok(CallbackResp::State) => {}
            _ => {
                debug!("Unexpected message {:?} during state callback", r);
            }
        }
    }

    fn device_change_callback(&mut self) {
        trace!("Stream device change callback");
        let r = self.rpc.call(CallbackReq::DeviceChange).wait();
        match r {
            Ok(CallbackResp::DeviceChange) => {}
            _ => {
                debug!("Unexpected message {:?} during device change callback", r);
            }
        }
    }
}

static SHM_ID: AtomicUsize = AtomicUsize::new(0);

// Generate a temporary shm_id fragment that is unique to the process.  This
// path is used temporarily to create a shm segment, which is then
// immediately deleted from the filesystem while retaining handles to the
// shm to be shared between the server and client.
fn get_shm_id() -> String {
    format!(
        "cubeb-shm-{}-{}",
        std::process::id(),
        SHM_ID.fetch_add(1, Ordering::SeqCst)
    )
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

type StreamSlab = slab::Slab<ServerStream>;

struct CubebServerCallbacks {
    rpc: rpc::ClientProxy<DeviceCollectionReq, DeviceCollectionResp>,
    devtype: cubeb::DeviceType,
}

impl CubebServerCallbacks {
    fn device_collection_changed_callback(&mut self, device_type: ffi::cubeb_device_type) {
        // TODO: Assert device_type is in devtype.
        debug!(
            "Sending device collection ({:?}) changed event",
            device_type
        );
        let _ = self
            .rpc
            .call(DeviceCollectionReq::DeviceChange(device_type))
            .wait();
    }
}

pub struct CubebServer {
    handle: current_thread::Handle,
    streams: StreamSlab,
    remote_pid: Option<u32>,
    cbs: Option<Rc<RefCell<CubebServerCallbacks>>>,
    devidmap: DevIdMap,
}

impl rpc::Server for CubebServer {
    type Request = ServerMessage;
    type Response = ClientMessage;
    type Future = FutureResult<Self::Response, ()>;
    type Transport = FramedWithPlatformHandles<
        audioipc::AsyncMessageStream,
        LengthDelimitedCodec<Self::Response, Self::Request>,
    >;

    fn process(&mut self, req: Self::Request) -> Self::Future {
        let resp = with_local_context(|context, manager| match *context {
            Err(_) => error(cubeb::Error::error()),
            Ok(ref context) => self.process_msg(context, manager, &req),
        });
        future::ok(resp)
    }
}

// Debugging for BMO 1594216/1612044.
macro_rules! try_stream {
    ($self:expr, $stm_tok:expr) => {
        if $self.streams.contains($stm_tok) {
            &mut $self.streams[$stm_tok]
        } else {
            error!(
                "{}:{}:{} - Stream({}): invalid token",
                file!(),
                line!(),
                column!(),
                $stm_tok
            );
            return error(cubeb::Error::invalid_parameter());
        }
    };
}

impl CubebServer {
    pub fn new(handle: current_thread::Handle) -> Self {
        CubebServer {
            handle,
            streams: StreamSlab::with_capacity(STREAM_CONN_CHUNK_SIZE),
            remote_pid: None,
            cbs: None,
            devidmap: DevIdMap::new(),
        }
    }

    // Process a request coming from the client.
    fn process_msg(
        &mut self,
        context: &cubeb::Context,
        manager: &mut CubebDeviceCollectionManager,
        msg: &ServerMessage,
    ) -> ClientMessage {
        let resp: ClientMessage = match *msg {
            ServerMessage::ClientConnect(pid) => {
                self.remote_pid = Some(pid);
                ClientMessage::ClientConnected
            }

            ServerMessage::ClientDisconnect => {
                // TODO:
                //self.connection.client_disconnect();
                ClientMessage::ClientDisconnected
            }

            ServerMessage::ContextGetBackendId => {
                ClientMessage::ContextBackendId(context.backend_id().to_string())
            }

            ServerMessage::ContextGetMaxChannelCount => context
                .max_channel_count()
                .map(ClientMessage::ContextMaxChannelCount)
                .unwrap_or_else(error),

            ServerMessage::ContextGetMinLatency(ref params) => {
                let format = cubeb::SampleFormat::from(params.format);
                let layout = cubeb::ChannelLayout::from(params.layout);

                let params = cubeb::StreamParamsBuilder::new()
                    .format(format)
                    .rate(params.rate)
                    .channels(params.channels)
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
                    let v: Vec<DeviceInfo> = devices
                        .iter()
                        .map(|i| {
                            let mut tmp: DeviceInfo = i.as_ref().into();
                            // Replace each cubeb_devid with a unique handle suitable for IPC.
                            tmp.devid = self.devidmap.to_handle(tmp.devid);
                            tmp
                        })
                        .collect();
                    ClientMessage::ContextEnumeratedDevices(v)
                })
                .unwrap_or_else(error),

            ServerMessage::StreamInit(ref params) => self
                .process_stream_init(context, params)
                .unwrap_or_else(|_| error(cubeb::Error::error())),

            ServerMessage::StreamDestroy(stm_tok) => {
                if self.streams.contains(stm_tok) {
                    debug!("Unregistering stream {:?}", stm_tok);
                    self.streams.remove(stm_tok);
                } else {
                    // Debugging for BMO 1594216/1612044.
                    error!("StreamDestroy({}): invalid token", stm_tok);
                    return error(cubeb::Error::invalid_parameter());
                }
                ClientMessage::StreamDestroyed
            }

            ServerMessage::StreamStart(stm_tok) => try_stream!(self, stm_tok)
                .stream
                .start()
                .map(|_| ClientMessage::StreamStarted)
                .unwrap_or_else(error),

            ServerMessage::StreamStop(stm_tok) => try_stream!(self, stm_tok)
                .stream
                .stop()
                .map(|_| ClientMessage::StreamStopped)
                .unwrap_or_else(error),

            ServerMessage::StreamResetDefaultDevice(stm_tok) => try_stream!(self, stm_tok)
                .stream
                .reset_default_device()
                .map(|_| ClientMessage::StreamDefaultDeviceReset)
                .unwrap_or_else(error),

            ServerMessage::StreamGetPosition(stm_tok) => try_stream!(self, stm_tok)
                .stream
                .position()
                .map(ClientMessage::StreamPosition)
                .unwrap_or_else(error),

            ServerMessage::StreamGetLatency(stm_tok) => try_stream!(self, stm_tok)
                .stream
                .latency()
                .map(ClientMessage::StreamLatency)
                .unwrap_or_else(error),

            ServerMessage::StreamGetInputLatency(stm_tok) => try_stream!(self, stm_tok)
                .stream
                .input_latency()
                .map(ClientMessage::StreamLatency)
                .unwrap_or_else(error),

            ServerMessage::StreamSetVolume(stm_tok, volume) => try_stream!(self, stm_tok)
                .stream
                .set_volume(volume)
                .map(|_| ClientMessage::StreamVolumeSet)
                .unwrap_or_else(error),

            ServerMessage::StreamGetCurrentDevice(stm_tok) => try_stream!(self, stm_tok)
                .stream
                .current_device()
                .map(|device| ClientMessage::StreamCurrentDevice(Device::from(device)))
                .unwrap_or_else(error),

            ServerMessage::StreamRegisterDeviceChangeCallback(stm_tok, enable) => {
                try_stream!(self, stm_tok)
                    .stream
                    .register_device_changed_callback(if enable {
                        Some(device_change_cb_c)
                    } else {
                        None
                    })
                    .map(|_| ClientMessage::StreamRegisterDeviceChangeCallback)
                    .unwrap_or_else(error)
            }

            ServerMessage::ContextSetupDeviceCollectionCallback => {
                if let Ok((ipc_server, ipc_client)) = MessageStream::anonymous_ipc_pair() {
                    debug!(
                        "Created device collection RPC pair: {:?}-{:?}",
                        ipc_server, ipc_client
                    );

                    // This code is currently running on the Client/Server RPC
                    // handling thread.  We need to move the registration of the
                    // bind_client to the callback RPC handling thread.  This is
                    // done by spawning a future on `handle`.

                    let (tx, rx) = oneshot::channel();
                    self.handle
                        .spawn(futures::future::lazy(move || {
                            let handle = reactor::Handle::default();
                            let stream = ipc_server.into_tokio_ipc(&handle).unwrap();
                            let transport = framed(stream, Default::default());
                            let rpc = rpc::bind_client::<DeviceCollectionClient>(transport);
                            drop(tx.send(rpc));
                            Ok(())
                        }))
                        .expect("Failed to spawn DeviceCollectionClient");

                    // TODO: The lowest comms layer expects exactly 3 PlatformHandles, but we only
                    // need one here.  Send some dummy handles over for the other side to discard.
                    let (dummy1, dummy2) =
                        MessageStream::anonymous_ipc_pair().expect("need dummy IPC pair");
                    if let Ok(rpc) = rx.wait() {
                        self.cbs = Some(Rc::new(RefCell::new(CubebServerCallbacks {
                            rpc,
                            devtype: cubeb::DeviceType::empty(),
                        })));
                        let fds = RegisterDeviceCollectionChanged {
                            platform_handles: [
                                PlatformHandle::from(ipc_client),
                                PlatformHandle::from(dummy1),
                                PlatformHandle::from(dummy2),
                            ],
                            target_pid: self.remote_pid.unwrap(),
                        };

                        ClientMessage::ContextSetupDeviceCollectionCallback(fds)
                    } else {
                        warn!("Failed to setup RPC client");
                        error(cubeb::Error::error())
                    }
                } else {
                    warn!("Failed to create RPC pair");
                    error(cubeb::Error::error())
                }
            }

            ServerMessage::ContextRegisterDeviceCollectionChanged(device_type, enable) => self
                .process_register_device_collection_changed(
                    context,
                    manager,
                    cubeb::DeviceType::from_bits_truncate(device_type),
                    enable,
                )
                .unwrap_or_else(error),

            #[cfg(target_os = "linux")]
            ServerMessage::PromoteThreadToRealTime(thread_info) => {
                let info = RtPriorityThreadInfo::deserialize(thread_info);
                match promote_thread_to_real_time(info, 0, 48000) {
                    Ok(_) => {
                        info!("Promotion of content process thread to real-time OK");
                    }
                    Err(_) => {
                        warn!("Promotion of content process thread to real-time error");
                    }
                }
                ClientMessage::ThreadPromoted
            }
        };

        trace!("process_msg: req={:?}, resp={:?}", msg, resp);

        resp
    }

    fn process_register_device_collection_changed(
        &mut self,
        context: &cubeb::Context,
        manager: &mut CubebDeviceCollectionManager,
        devtype: cubeb::DeviceType,
        enable: bool,
    ) -> cubeb::Result<ClientMessage> {
        if devtype == cubeb::DeviceType::UNKNOWN {
            return Err(cubeb::Error::invalid_parameter());
        }

        assert!(self.cbs.is_some());
        let cbs = self.cbs.as_ref().unwrap();

        if enable {
            cbs.borrow_mut().devtype.insert(devtype);
            manager.register(context, cbs)
        } else {
            cbs.borrow_mut().devtype.remove(devtype);
            manager.unregister(context, cbs)
        }
        .map(|_| ClientMessage::ContextRegisteredDeviceCollectionChanged)
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

        let (ipc_server, ipc_client) = MessageStream::anonymous_ipc_pair()?;
        debug!("Created callback pair: {:?}-{:?}", ipc_server, ipc_client);
        let shm_id = get_shm_id();
        let (input_shm, input_file) =
            SharedMemWriter::new(&format!("{}-input", shm_id), audioipc::SHM_AREA_SIZE)?;
        let (output_shm, output_file) =
            SharedMemReader::new(&format!("{}-output", shm_id), audioipc::SHM_AREA_SIZE)?;

        // This code is currently running on the Client/Server RPC
        // handling thread.  We need to move the registration of the
        // bind_client to the callback RPC handling thread.  This is
        // done by spawning a future on `handle`.

        let (tx, rx) = oneshot::channel();
        self.handle
            .spawn(futures::future::lazy(move || {
                let handle = reactor::Handle::default();
                let stream = ipc_server.into_tokio_ipc(&handle).unwrap();
                let transport = framed(stream, Default::default());
                let rpc = rpc::bind_client::<CallbackClient>(transport);
                drop(tx.send(rpc));
                Ok(())
            }))
            .expect("Failed to spawn CallbackClient");

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

        // Map IPC handle back to cubeb_devid.
        let input_device = self.devidmap.from_handle(params.input_device) as *const _;
        let input_stream_params = params.input_stream_params.as_ref().map(|isp| unsafe {
            cubeb::StreamParamsRef::from_ptr(isp as *const StreamParams as *mut _)
        });

        // Map IPC handle back to cubeb_devid.
        let output_device = self.devidmap.from_handle(params.output_device) as *const _;
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
                    if self.streams.len() == self.streams.capacity() {
                        trace!(
                            "server connection ran out of stream slots. reserving {} more.",
                            STREAM_CONN_CHUNK_SIZE
                        );
                        self.streams.reserve_exact(STREAM_CONN_CHUNK_SIZE);
                    }

                    let entry = self.streams.vacant_entry();
                    let key = entry.key();
                    debug!("Registering stream {:?}", key);

                    entry.insert(ServerStream {
                        stream: ManuallyDrop::new(stream),
                        cbs: ManuallyDrop::new(cbs),
                    });

                    Ok(ClientMessage::StreamCreated(StreamCreate {
                        token: key,
                        platform_handles: [
                            PlatformHandle::from(ipc_client),
                            PlatformHandle::from(input_file),
                            PlatformHandle::from(output_file),
                        ],
                        target_pid: self.remote_pid.unwrap(),
                    }))
                })
                .map_err(|e| e.into())
        }
    }
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
            let nbytes = nframes * c_long::from(cbs.input_frame_size);
            slice::from_raw_parts(input_buffer as *const u8, nbytes as usize)
        };
        let output: &mut [u8] = if output_buffer.is_null() {
            &mut []
        } else {
            let nbytes = nframes * c_long::from(cbs.output_frame_size);
            slice::from_raw_parts_mut(output_buffer as *mut u8, nbytes as usize)
        };
        cbs.data_callback(input, output, nframes as isize) as c_long
    });
    // TODO: Return a CUBEB_ERROR result here once
    // https://github.com/kinetiknz/cubeb/issues/553 is fixed.
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

unsafe extern "C" fn device_change_cb_c(user_ptr: *mut c_void) {
    let ok = panic::catch_unwind(|| {
        let cbs = &mut *(user_ptr as *mut ServerStreamCallbacks);
        cbs.device_change_callback();
    });
    ok.expect("Device change callback panicked");
}

unsafe extern "C" fn device_collection_changed_input_cb_c(
    _: *mut ffi::cubeb,
    user_ptr: *mut c_void,
) {
    let ok = panic::catch_unwind(|| {
        let manager = &mut *(user_ptr as *mut CubebDeviceCollectionManager);
        manager.device_collection_changed_callback(ffi::CUBEB_DEVICE_TYPE_INPUT);
    });
    ok.expect("Collection changed (input) callback panicked");
}

unsafe extern "C" fn device_collection_changed_output_cb_c(
    _: *mut ffi::cubeb,
    user_ptr: *mut c_void,
) {
    let ok = panic::catch_unwind(|| {
        let manager = &mut *(user_ptr as *mut CubebDeviceCollectionManager);
        manager.device_collection_changed_callback(ffi::CUBEB_DEVICE_TYPE_OUTPUT);
    });
    ok.expect("Collection changed (output) callback panicked");
}

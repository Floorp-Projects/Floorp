#[macro_use]
extern crate error_chain;

#[macro_use]
extern crate log;

extern crate audioipc;
extern crate bytes;
extern crate cubeb;
extern crate cubeb_core;
extern crate lazycell;
extern crate libc;
extern crate mio;
extern crate mio_uds;
extern crate slab;

use audioipc::AutoCloseFd;
use audioipc::async::{Async, AsyncRecvFd, AsyncSendFd};
use audioipc::codec::{Decoder, encode};
use audioipc::messages::{ClientMessage, DeviceInfo, ServerMessage, StreamInitParams, StreamParams};
use audioipc::shm::{SharedMemReader, SharedMemWriter};
use bytes::{Bytes, BytesMut};
use cubeb_core::binding::Binding;
use cubeb_core::ffi;
use mio::{Ready, Token};
use mio_uds::UnixStream;
use std::{slice, thread};
use std::collections::{HashSet, VecDeque};
use std::convert::From;
use std::io::Cursor;
use std::os::raw::c_void;
use std::os::unix::prelude::*;
use std::sync::{Mutex, MutexGuard};

mod channel;

pub mod errors {
    error_chain! {
        links {
            AudioIPC(::audioipc::errors::Error, ::audioipc::errors::ErrorKind);
        }
        foreign_links {
            Cubeb(::cubeb_core::Error);
            Io(::std::io::Error);
        }
    }
}

use errors::*;

// TODO: Remove and let caller allocate based on cubeb backend requirements.
const SHM_AREA_SIZE: usize = 2 * 1024 * 1024;

// The size in which the server conns slab is grown.
const SERVER_CONN_CHUNK_SIZE: usize = 16;

// The size in which the stream slab is grown.
const STREAM_CONN_CHUNK_SIZE: usize = 64;

// TODO: this should forward to the client.
struct Callback {
    /// Size of input frame in bytes
    input_frame_size: u16,
    /// Size of output frame in bytes
    output_frame_size: u16,
    connection: Mutex<audioipc::Connection>,
    input_shm: SharedMemWriter,
    output_shm: SharedMemReader
}

impl Callback {
    #[doc(hidden)]
    fn connection(&self) -> MutexGuard<audioipc::Connection> {
        self.connection.lock().unwrap()
    }
}

impl cubeb::StreamCallback for Callback {
    type Frame = u8;

    fn data_callback(&mut self, input: &[u8], output: &mut [u8]) -> isize {
        trace!("Stream data callback: {} {}", input.len(), output.len());

        // len is of input and output is frame len. Turn these into the real lengths.
        let real_input = unsafe {
            let size_bytes = input.len() * self.input_frame_size as usize;
            slice::from_raw_parts(input.as_ptr(), size_bytes)
        };
        let real_output = unsafe {
            let size_bytes = output.len() * self.output_frame_size as usize;
            trace!("Resize output to {}", size_bytes);
            slice::from_raw_parts_mut(output.as_mut_ptr(), size_bytes)
        };

        self.input_shm.write(real_input).unwrap();

        let mut conn = self.connection();
        let r = conn.send(ClientMessage::StreamDataCallback(
            output.len() as isize,
            self.output_frame_size as usize
        ));
        if r.is_err() {
            debug!("data_callback: Failed to send to client - got={:?}", r);
            return -1;
        }

        let r = conn.receive();
        match r {
            Ok(ServerMessage::StreamDataCallback(cb_result)) => {
                if cb_result >= 0 {
                    let len = cb_result as usize * self.output_frame_size as usize;
                    self.output_shm.read(&mut real_output[..len]).unwrap();
                    cb_result
                } else {
                    cb_result
                }
            },
            _ => {
                debug!("Unexpected message {:?} during data_callback", r);
                -1
            },
        }
    }

    fn state_callback(&mut self, state: cubeb::State) {
        info!("Stream state callback: {:?}", state);
        // TODO: Share this conversion with the same code in cubeb-rs?
        let state = match state {
            cubeb::State::Started => ffi::CUBEB_STATE_STARTED,
            cubeb::State::Stopped => ffi::CUBEB_STATE_STOPPED,
            cubeb::State::Drained => ffi::CUBEB_STATE_DRAINED,
            cubeb::State::Error => ffi::CUBEB_STATE_ERROR,
        };
        let mut conn = self.connection();
        let r = conn.send(ClientMessage::StreamStateCallback(state));
        if r.is_err() {
            debug!("state_callback: Failed to send to client - got={:?}", r);
        }

        // Bug 1414623 - We need to block on an ACK from the client
        // side to make state_callback synchronous. If not, then there
        // exists a race on cubeb_stream_stop()/cubeb_stream_destroy()
        // in Gecko that results in a UAF.
        let r = conn.receive();
        match r {
            Ok(ServerMessage::StreamStateCallback) => {},
            _ => {
                debug!("Unexpected message {:?} during state_callback", r);
            },
        }
    }
}

impl Drop for Callback {
    fn drop(&mut self) {
        let mut conn = self.connection();
        let r = conn.send(ClientMessage::StreamDestroyed);
        if r.is_err() {
            debug!("Callback::drop failed to send StreamDestroyed = {:?}", r);
        }
    }
}

type Slab<T> = slab::Slab<T, Token>;
type StreamSlab = slab::Slab<cubeb::Stream<Callback>, usize>;

// TODO: Server command token must be outside range used by server.connections slab.
// usize::MAX is already used internally in mio.
const CMD: Token = Token(std::usize::MAX - 1);

struct ServerConn {
    //connection: audioipc::Connection,
    io: UnixStream,
    token: Option<Token>,
    streams: StreamSlab,
    decoder: Decoder,
    recv_buffer: BytesMut,
    send_buffer: BytesMut,
    pending_send: VecDeque<(Bytes, Option<AutoCloseFd>)>,
    device_ids: HashSet<usize>
}

impl ServerConn {
    fn new(io: UnixStream) -> ServerConn {
        let mut sc = ServerConn {
            io: io,
            token: None,
            // TODO: Handle increasing slab size. Pick a good default size.
            streams: StreamSlab::with_capacity(STREAM_CONN_CHUNK_SIZE),
            decoder: Decoder::new(),
            recv_buffer: BytesMut::with_capacity(4096),
            send_buffer: BytesMut::with_capacity(4096),
            pending_send: VecDeque::new(),
            device_ids: HashSet::new()
        };
        sc.device_ids.insert(0); // nullptr is always a valid (default) device id.
        sc
    }

    fn process_read(&mut self, context: &Result<cubeb::Context>) -> Result<Ready> {
        // According to *something*, processing non-blocking stream
        // should attempt to read until EWOULDBLOCK is returned.
        while let Async::Ready((n, fd)) = try!(self.io.recv_buf_fd(&mut self.recv_buffer)) {
            trace!("Received {} bytes and fd {:?}", n, fd);

            // Reading 0 signifies EOF
            if n == 0 {
                return Err(
                    ::errors::ErrorKind::AudioIPC(::audioipc::errors::ErrorKind::Disconnected).into()
                );
            }

            if let Some(fd) = fd {
                trace!("Unexpectedly received an fd from client.");
                let _ = unsafe { AutoCloseFd::from_raw_fd(fd) };
            }

            // Process all the complete messages contained in
            // send.recv_buffer.  It's possible that a read might not
            // return a complete message, so self.decoder.decode
            // returns Ok(None).
            loop {
                match self.decoder.decode::<ServerMessage>(&mut self.recv_buffer) {
                    Ok(Some(msg)) => {
                        info!("ServerConn::process: got {:?}", msg);
                        try!(self.process_msg(&msg, context));
                    },
                    Ok(None) => {
                        break;
                    },
                    Err(e) => {
                        return Err(e).chain_err(|| "Failed to decoder ServerMessage");
                    },
                }
            }
        }

        // Send any pending responses to client.
        self.flush_pending_send()
    }

    // Process a request coming from the client.
    fn process_msg(&mut self, msg: &ServerMessage, context: &Result<cubeb::Context>) -> Result<()> {
        let resp: ClientMessage = if let Ok(ref context) = *context {
            if let ServerMessage::StreamInit(ref params) = *msg {
                return self.process_stream_init(context, params);
            };

            match *msg {
                ServerMessage::ClientConnect => {
                    panic!("already connected");
                },

                ServerMessage::ClientDisconnect => {
                    // TODO:
                    //self.connection.client_disconnect();
                    ClientMessage::ClientDisconnected
                },

                ServerMessage::ContextGetBackendId => ClientMessage::ContextBackendId(),

                ServerMessage::ContextGetMaxChannelCount => {
                    context
                        .max_channel_count()
                        .map(ClientMessage::ContextMaxChannelCount)
                        .unwrap_or_else(error)
                },

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
                },

                ServerMessage::ContextGetPreferredSampleRate => {
                    context
                        .preferred_sample_rate()
                        .map(ClientMessage::ContextPreferredSampleRate)
                        .unwrap_or_else(error)
                },

                ServerMessage::ContextGetPreferredChannelLayout => {
                    context
                        .preferred_channel_layout()
                        .map(|l| ClientMessage::ContextPreferredChannelLayout(l as _))
                        .unwrap_or_else(error)
                },

                ServerMessage::ContextGetDeviceEnumeration(device_type) => {
                    context
                        .enumerate_devices(cubeb::DeviceType::from_bits_truncate(device_type))
                        .map(|devices| {
                            let v: Vec<DeviceInfo> = devices.iter()
                                                            .map(|i| i.raw().into())
                                                            .collect();
                            for i in &v {
                                self.device_ids.insert(i.devid);
                            }
                            ClientMessage::ContextEnumeratedDevices(v)
                        })
                        .unwrap_or_else(error)
                },

                ServerMessage::StreamInit(_) => {
                    panic!("StreamInit should have already been handled.");
                },

                ServerMessage::StreamDestroy(stm_tok) => {
                    self.streams.remove(stm_tok);
                    ClientMessage::StreamDestroyed
                },

                ServerMessage::StreamStart(stm_tok) => {
                    let _ = self.streams[stm_tok].start();
                    ClientMessage::StreamStarted
                },

                ServerMessage::StreamStop(stm_tok) => {
                    let _ = self.streams[stm_tok].stop();
                    ClientMessage::StreamStopped
                },

                ServerMessage::StreamGetPosition(stm_tok) => {
                    self.streams[stm_tok]
                        .position()
                        .map(ClientMessage::StreamPosition)
                        .unwrap_or_else(error)
                },

                ServerMessage::StreamGetLatency(stm_tok) => {
                    self.streams[stm_tok]
                        .latency()
                        .map(ClientMessage::StreamLatency)
                        .unwrap_or_else(error)
                },

                ServerMessage::StreamSetVolume(stm_tok, volume) => {
                    self.streams[stm_tok]
                        .set_volume(volume)
                        .map(|_| ClientMessage::StreamVolumeSet)
                        .unwrap_or_else(error)
                },

                ServerMessage::StreamSetPanning(stm_tok, panning) => {
                    self.streams[stm_tok]
                        .set_panning(panning)
                        .map(|_| ClientMessage::StreamPanningSet)
                        .unwrap_or_else(error)
                },

                ServerMessage::StreamGetCurrentDevice(stm_tok) => {
                    self.streams[stm_tok]
                        .current_device()
                        .map(|device| ClientMessage::StreamCurrentDevice(device.into()))
                        .unwrap_or_else(error)
                },

                _ => {
                    bail!("Unexpected Message");
                },
            }
        } else {
            error(cubeb::Error::new())
        };

        debug!("process_msg: req={:?}, resp={:?}", msg, resp);

        self.queue_message(resp)
    }

    // Stream init is special, so it's been separated from process_msg.
    fn process_stream_init(&mut self, context: &cubeb::Context, params: &StreamInitParams) -> Result<()> {
        fn opt_stream_params(params: Option<&StreamParams>) -> Option<cubeb::StreamParams> {
            params.and_then(|p| {
                let raw = ffi::cubeb_stream_params::from(p);
                Some(unsafe { cubeb::StreamParams::from_raw(&raw as *const _) })
            })
        }

        fn frame_size_in_bytes(params: Option<cubeb::StreamParams>) -> u16 {
            params
                .map(|p| {
                    let sample_size = match p.format() {
                        cubeb::SampleFormat::S16LE |
                        cubeb::SampleFormat::S16BE |
                        cubeb::SampleFormat::S16NE => 2,
                        cubeb::SampleFormat::Float32LE |
                        cubeb::SampleFormat::Float32BE |
                        cubeb::SampleFormat::Float32NE => 4,
                    };
                    let channel_count = p.channels() as u16;
                    sample_size * channel_count
                })
                .unwrap_or(0u16)
        }


        if !self.device_ids.contains(&params.input_device) {
            bail!("Invalid input_device passed to stream_init");
        }
        // TODO: Yuck!
        let input_device = unsafe { cubeb::DeviceId::from_raw(params.input_device as *const _) };

        if !self.device_ids.contains(&params.output_device) {
            bail!("Invalid output_device passed to stream_init");
        }
        // TODO: Yuck!
        let output_device = unsafe { cubeb::DeviceId::from_raw(params.output_device as *const _) };

        let latency = params.latency_frames;
        let mut builder = cubeb::StreamInitOptionsBuilder::new();
        builder
            .input_device(input_device)
            .output_device(output_device)
            .latency(latency);

        if let Some(ref stream_name) = params.stream_name {
            builder.stream_name(stream_name);
        }
        let input_stream_params = opt_stream_params(params.input_stream_params.as_ref());
        if let Some(ref isp) = input_stream_params {
            builder.input_stream_param(isp);
        }
        let output_stream_params = opt_stream_params(params.output_stream_params.as_ref());
        if let Some(ref osp) = output_stream_params {
            builder.output_stream_param(osp);
        }
        let params = builder.take();

        let input_frame_size = frame_size_in_bytes(input_stream_params);
        let output_frame_size = frame_size_in_bytes(output_stream_params);

        let (conn1, conn2) = audioipc::Connection::pair()?;
        info!("Created connection pair: {:?}-{:?}", conn1, conn2);

        let (input_shm, input_file) = SharedMemWriter::new(&audioipc::get_shm_path("input"), SHM_AREA_SIZE)?;
        let (output_shm, output_file) = SharedMemReader::new(&audioipc::get_shm_path("output"), SHM_AREA_SIZE)?;

        let err = match context.stream_init(
            &params,
            Callback {
                input_frame_size: input_frame_size,
                output_frame_size: output_frame_size,
                connection: Mutex::new(conn2),
                input_shm: input_shm,
                output_shm: output_shm
            }
        ) {
            Ok(stream) => {
                if !self.streams.has_available() {
                    trace!(
                        "server connection ran out of stream slots. reserving {} more.",
                        STREAM_CONN_CHUNK_SIZE
                    );
                    self.streams.reserve_exact(STREAM_CONN_CHUNK_SIZE);
                }

                let stm_tok = match self.streams.vacant_entry() {
                    Some(entry) => {
                        debug!(
                            "Registering stream {:?}",
                            entry.index(),
                        );

                        entry.insert(stream).index()
                    },
                    None => {
                        // TODO: Turn into error
                        panic!("Failed to insert stream into slab. No entries");
                    },
                };

                try!(self.queue_init_messages(
                    stm_tok,
                    conn1,
                    input_file,
                    output_file
                ));
                None
            },
            Err(e) => Some(error(e)),
        };

        if let Some(err) = err {
            try!(self.queue_message(err))
        }

        Ok(())
    }

    fn queue_init_messages<T, U, V>(&mut self, stm_tok: usize, conn: T, input_file: U, output_file: V) -> Result<()>
    where
        T: IntoRawFd,
        U: IntoRawFd,
        V: IntoRawFd,
    {
        try!(self.queue_message_fd(
            ClientMessage::StreamCreated(stm_tok),
            conn
        ));
        try!(self.queue_message_fd(
            ClientMessage::StreamCreatedInputShm,
            input_file
        ));
        try!(self.queue_message_fd(
            ClientMessage::StreamCreatedOutputShm,
            output_file
        ));
        Ok(())
    }

    fn queue_message(&mut self, msg: ClientMessage) -> Result<()> {
        debug!("queue_message: {:?}", msg);
        encode::<ClientMessage>(&mut self.send_buffer, &msg).or_else(|e| {
            Err(e).chain_err(|| "Failed to encode msg into send buffer")
        })
    }

    // Since send_fd supports sending one RawFd at a time, queuing a
    // message with a RawFd forces use to take the current send_buffer
    // and move it pending queue.
    fn queue_message_fd<FD: IntoRawFd>(&mut self, msg: ClientMessage, fd: FD) -> Result<()> {
        let fd = fd.into_raw_fd();
        debug!("queue_message_fd: {:?} {:?}", msg, fd);
        try!(self.queue_message(msg));
        self.take_pending_send(Some(fd));
        Ok(())
    }

    // Take the current messages in the send_buffer and move them to
    // pending queue.
    fn take_pending_send(&mut self, fd: Option<RawFd>) {
        let pending = self.send_buffer.take().freeze();
        debug!("take_pending_send: ({:?} {:?})", pending, fd);
        self.pending_send.push_back((
            pending,
            fd.map(|fd| unsafe { AutoCloseFd::from_raw_fd(fd) })
        ));
    }

    // Process the pending queue and send them to client.
    fn flush_pending_send(&mut self) -> Result<Ready> {
        debug!("flush_pending_send");
        // take any pending messages in the send buffer.
        if !self.send_buffer.is_empty() {
            self.take_pending_send(None);
        }

        trace!("pending queue: {:?}", self.pending_send);

        let mut result = Ready::readable();
        let mut processed = 0;

        for &mut (ref mut buf, ref mut fd) in &mut self.pending_send {
            trace!("sending buf {:?}, fd {:?}", buf, fd);
            let r = {
                let mut src = Cursor::new(buf.as_ref());
                let fd = match *fd {
                    Some(ref fd) => Some(fd.as_raw_fd()),
                    None => None,
                };
                try!(self.io.send_buf_fd(&mut src, fd))
            };
            match r {
                Async::Ready(n) if n == buf.len() => {
                    processed += 1;
                },
                Async::Ready(n) => {
                    let _ = buf.split_to(n);
                    let _ = fd.take();
                    result.insert(Ready::writable());
                    break;
                },
                Async::NotReady => {
                    result.insert(Ready::writable());
                },
            }
        }

        debug!("processed {} buffers", processed);

        self.pending_send = self.pending_send.split_off(processed);

        trace!("pending queue: {:?}", self.pending_send);

        Ok(result)
    }
}

pub struct Server {
    // Ok(None)      - Server hasn't tried to create cubeb::Context.
    // Ok(Some(ctx)) - Server has successfully created cubeb::Context.
    // Err(_)        - Server has tried and failed to create cubeb::Context.
    //                 Don't try again.
    context: Option<Result<cubeb::Context>>,
    conns: Slab<ServerConn>,
    rx: channel::Receiver<Command>,
    tx: std::sync::mpsc::Sender<Response>,
}

impl Drop for Server {
    fn drop(&mut self) {
        // ServerConns rely on the cubeb context, so we must free them
        // explicitly before the context is dropped.
        if !self.conns.is_empty() {
            debug!("dropping Server with {} live ServerConns", self.conns.len());
            self.conns.clear();
        }
    }
}

impl Server {
    pub fn new(rx: channel::Receiver<Command>,
               tx: std::sync::mpsc::Sender<Response>) -> Server {
        Server {
            context: None,
            conns: slab::Slab::with_capacity(SERVER_CONN_CHUNK_SIZE),
            rx: rx,
            tx: tx,
        }
    }

    fn handle_new_connection(&mut self, poll: &mut mio::Poll, client_socket: UnixStream) -> Result<()> {
        if !self.conns.has_available() {
            trace!(
                "server ran out of connection slots. reserving {} more.",
                SERVER_CONN_CHUNK_SIZE
            );
            self.conns.reserve_exact(SERVER_CONN_CHUNK_SIZE);
        }

        let token = match self.conns.vacant_entry() {
            Some(entry) => {
                debug!("registering {:?}", entry.index());
                let cxn = ServerConn::new(client_socket);
                entry.insert(cxn).index()
            },
            None => {
                panic!("failed to insert connection");
            },
        };

        // Register the connection
        self.conns[token].token = Some(token);
        poll.register(
            &self.conns[token].io,
            token,
            mio::Ready::readable(),
            mio::PollOpt::edge() | mio::PollOpt::oneshot()
        ).unwrap();
        /*
        let r = self.conns[token].receive();
        debug!("received {:?}", r);
        let r = self.conns[token].send(ClientMessage::ClientConnected);
        debug!("sent {:?} (ClientConnected)", r);
         */

        // Since we have a connection try creating a cubeb context. If
        // it fails, mark the failure with Err.
        if self.context.is_none() {
            self.context = Some(cubeb::Context::init("AudioIPC Server", None).or_else(|e| {
                Err(e).chain_err(|| "Unable to create cubeb context.")
            }))
        }

        Ok(())
    }

    fn new_connection(&mut self, poll: &mut mio::Poll) -> Result<AutoCloseFd> {
        let (s1, s2) = UnixStream::pair()?;
        self.handle_new_connection(poll, s1)?;
        unsafe {
            Ok(audioipc::AutoCloseFd::from_raw_fd(s2.into_raw_fd()))
        }
    }

    pub fn poll(&mut self, poll: &mut mio::Poll) -> Result<()> {
        let mut events = mio::Events::with_capacity(16);

        match poll.poll(&mut events, None) {
            Ok(_) => {},
            Err(ref e) if e.kind() == std::io::ErrorKind::Interrupted => return Ok(()),
            Err(e) => warn!("server poll error: {}", e),
        }

        for event in events.iter() {
            match event.token() {
                CMD => {
                    match self.rx.try_recv().unwrap() {
                        Command::Quit => {
                            info!("Quitting Audio Server loop");
                            self.tx.send(Response::Quit).unwrap();
                            bail!("quit");
                        },
                        Command::NewConnection => {
                            info!("Creating new connection");
                            let fd = self.new_connection(poll)?;
                            self.tx.send(Response::Connection(fd)).unwrap();
                        }
                    }
                },
                token => {
                    trace!("token {:?} ready", token);

                    let context = self.context.as_ref().expect(
                        "Shouldn't receive a message before accepting connection."
                    );

                    let mut readiness = Ready::readable();

                    if event.readiness().is_readable() {
                        let r = self.conns[token].process_read(context);
                        trace!("got {:?}", r);

                        if let Err(e) = r {
                            debug!("dropped client {:?} due to error {:?}", token, e);
                            self.conns.remove(token);
                            continue;
                        }
                    };

                    if event.readiness().is_writable() {
                        let r = self.conns[token].flush_pending_send();
                        trace!("got {:?}", r);

                        match r {
                            Ok(r) => readiness = r,
                            Err(e) => {
                                debug!("dropped client {:?} due to error {:?}", token, e);
                                self.conns.remove(token);
                                continue;
                            },
                        }
                    };

                    poll.reregister(
                        &self.conns[token].io,
                        token,
                        readiness,
                        mio::PollOpt::edge() | mio::PollOpt::oneshot()
                    ).unwrap();
                },
            }
        }

        Ok(())
    }
}

fn error(error: cubeb::Error) -> ClientMessage {
    ClientMessage::Error(error.raw_code())
}

struct ServerWrapper {
    thread_handle: std::thread::JoinHandle<()>,
    tx: channel::Sender<Command>,
    rx: std::sync::mpsc::Receiver<Response>,
}

#[derive(Debug)]
pub enum Command {
    Quit,
    NewConnection,
}

#[derive(Debug)]
pub enum Response {
    Quit,
    Connection(AutoCloseFd)
}

impl ServerWrapper {
    fn shutdown(self) {
        let _ = self.tx.send(Command::Quit);
        let r = self.rx.recv();
        match r {
            Ok(Response::Quit) => info!("server quit response received"),
            e => warn!("unexpected response to server quit: {:?}", e),
        };
        self.thread_handle.join().unwrap();
    }
}

#[no_mangle]
pub extern "C" fn audioipc_server_start() -> *mut c_void {
    let (tx, rx) = channel::channel();
    let (tx2, rx2) = std::sync::mpsc::channel();

    let handle = thread::spawn(move || {
        let mut poll = mio::Poll::new().unwrap();
        let mut server = Server::new(rx, tx2);

        poll.register(&server.rx, CMD, mio::Ready::readable(), mio::PollOpt::edge())
            .unwrap();

        loop {
            if server.poll(&mut poll).is_err() {
                return;
            }
        }
    });

    let wrapper = ServerWrapper {
        thread_handle: handle,
        tx: tx,
        rx: rx2,
    };

    Box::into_raw(Box::new(wrapper)) as *mut _
}

#[no_mangle]
pub extern "C" fn audioipc_server_new_client(p: *mut c_void) -> libc::c_int {
    let wrapper: &mut ServerWrapper = unsafe { &mut *(p as *mut _) };

    wrapper.tx.send(Command::NewConnection).unwrap();

    if let Response::Connection(fd) = wrapper.rx.recv().unwrap() {
        return fd.into_raw_fd();
    }

    -1
}

#[no_mangle]
pub extern "C" fn audioipc_server_stop(p: *mut c_void) {
    let wrapper = unsafe { Box::<ServerWrapper>::from_raw(p as *mut _) };
    wrapper.shutdown();
}

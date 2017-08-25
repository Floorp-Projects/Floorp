#[macro_use]
extern crate error_chain;

#[macro_use]
extern crate log;

extern crate audioipc;
extern crate cubeb;
extern crate cubeb_core;
extern crate lazycell;
extern crate mio;
extern crate mio_uds;
extern crate slab;

use audioipc::messages::{ClientMessage, DeviceInfo, ServerMessage, StreamParams};
use audioipc::shm::{SharedMemReader, SharedMemWriter};
use cubeb_core::binding::Binding;
use cubeb_core::ffi;
use mio::Token;
use mio_uds::UnixListener;
use std::{slice, thread};
use std::convert::From;
use std::os::raw::c_void;
use std::os::unix::prelude::*;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};

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

// TODO: this should forward to the client.
struct Callback {
    /// Size of input frame in bytes
    input_frame_size: u16,
    /// Size of output frame in bytes
    output_frame_size: u16,
    connection: audioipc::Connection,
    input_shm: SharedMemWriter,
    output_shm: SharedMemReader,
}

impl cubeb::StreamCallback for Callback {
    type Frame = u8;

    fn data_callback(&mut self, input: &[u8], output: &mut [u8]) -> isize {
        info!("Stream data callback: {} {}", input.len(), output.len());

        // len is of input and output is frame len. Turn these into the real lengths.
        let real_input = unsafe {
            let size_bytes = input.len() * self.input_frame_size as usize;
            slice::from_raw_parts(input.as_ptr(), size_bytes)
        };
        let real_output = unsafe {
            let size_bytes = output.len() * self.output_frame_size as usize;
            info!("Resize output to {}", size_bytes);
            slice::from_raw_parts_mut(output.as_mut_ptr(), size_bytes)
        };

        self.input_shm.write(&real_input).unwrap();

        self.connection
            .send(ClientMessage::StreamDataCallback(
                output.len() as isize,
                self.output_frame_size as usize
            ))
            .unwrap();

        let r = self.connection.receive();
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
                debug!("Unexpected message {:?} during callback", r);
                -1
            },
        }
    }

    fn state_callback(&mut self, state: cubeb::State) {
        info!("Stream state callback: {:?}", state);
    }
}

impl Drop for Callback {
    fn drop(&mut self) {
        self.connection
            .send(ClientMessage::StreamDestroyed)
            .unwrap();
    }
}

type Slab<T> = slab::Slab<T, Token>;
type StreamSlab = slab::Slab<cubeb::Stream<Callback>, usize>;

// TODO: Server token must be outside range used by server.connections slab.
// usize::MAX is already used internally in mio.
const QUIT: Token = Token(std::usize::MAX - 2);
const SERVER: Token = Token(std::usize::MAX - 1);

struct ServerConn {
    connection: audioipc::Connection,
    token: Option<Token>,
    streams: StreamSlab
}

impl ServerConn {
    fn new<FD>(fd: FD) -> ServerConn
    where
        FD: IntoRawFd,
    {
        ServerConn {
            connection: unsafe { audioipc::Connection::from_raw_fd(fd.into_raw_fd()) },
            token: None,
            // TODO: Handle increasing slab size. Pick a good default size.
            streams: StreamSlab::with_capacity(64)
        }
    }

    fn process(&mut self, poll: &mut mio::Poll, context: &Result<Option<cubeb::Context>>) -> Result<()> {
        let r = self.connection.receive();
        info!("ServerConn::process: got {:?}", r);

        if let &Ok(Some(ref ctx)) = context {
            // TODO: Might need a simple state machine to deal with
            // create/use/destroy ordering, etc.
            // TODO: receive() and all this handling should be moved out
            // of this event loop code.
            let msg = try!(r);
            let _ = try!(self.process_msg(&msg, ctx));
        } else {
            self.send_error(cubeb::Error::new());
        }

        poll.reregister(
            &self.connection,
            self.token.unwrap(),
            mio::Ready::readable(),
            mio::PollOpt::edge() | mio::PollOpt::oneshot()
        ).unwrap();

        Ok(())
    }

    fn process_msg(&mut self, msg: &ServerMessage, context: &cubeb::Context) -> Result<()> {
        match msg {
            &ServerMessage::ClientConnect => {
                panic!("already connected");
            },
            &ServerMessage::ClientDisconnect => {
                // TODO:
                //self.connection.client_disconnect();
                self.connection
                    .send(ClientMessage::ClientDisconnected)
                    .unwrap();
            },

            &ServerMessage::ContextGetBackendId => {},

            &ServerMessage::ContextGetMaxChannelCount => {
                match context.max_channel_count() {
                    Ok(channel_count) => {
                        self.connection
                            .send(ClientMessage::ContextMaxChannelCount(channel_count))
                            .unwrap();
                    },
                    Err(e) => {
                        self.send_error(e);
                    },
                }
            },

            &ServerMessage::ContextGetMinLatency(ref params) => {

                let format = cubeb::SampleFormat::from(params.format);
                let layout = cubeb::ChannelLayout::from(params.layout);

                let params = cubeb::StreamParamsBuilder::new()
                    .format(format)
                    .rate(params.rate as _)
                    .channels(params.channels as _)
                    .layout(layout)
                    .take();

                match context.min_latency(&params) {
                    Ok(latency) => {
                        self.connection
                            .send(ClientMessage::ContextMinLatency(latency))
                            .unwrap();
                    },
                    Err(e) => {
                        self.send_error(e);
                    },
                }
            },

            &ServerMessage::ContextGetPreferredSampleRate => {
                match context.preferred_sample_rate() {
                    Ok(rate) => {
                        self.connection
                            .send(ClientMessage::ContextPreferredSampleRate(rate))
                            .unwrap();
                    },
                    Err(e) => {
                        self.send_error(e);
                    },
                }
            },

            &ServerMessage::ContextGetPreferredChannelLayout => {
                match context.preferred_channel_layout() {
                    Ok(layout) => {
                        self.connection
                            .send(ClientMessage::ContextPreferredChannelLayout(layout as _))
                            .unwrap();
                    },
                    Err(e) => {
                        self.send_error(e);
                    },
                }
            },

            &ServerMessage::ContextGetDeviceEnumeration(device_type) => {
                match context.enumerate_devices(cubeb::DeviceType::from_bits_truncate(device_type)) {
                    Ok(devices) => {
                        let v: Vec<DeviceInfo> = devices.iter().map(|i| i.raw().into()).collect();
                        self.connection
                            .send(ClientMessage::ContextEnumeratedDevices(v))
                            .unwrap();
                    },
                    Err(e) => {
                        self.send_error(e);
                    },
                }
            },

            &ServerMessage::StreamInit(ref params) => {
                fn opt_stream_params(params: Option<&StreamParams>) -> Option<cubeb::StreamParams> {
                    match params {
                        Some(p) => {
                            let raw = ffi::cubeb_stream_params::from(p);
                            Some(unsafe { cubeb::StreamParams::from_raw(&raw as *const _) })
                        },
                        None => None,
                    }
                }

                fn frame_size_in_bytes(params: Option<cubeb::StreamParams>) -> u16 {
                    match params.as_ref() {
                        Some(p) => {
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
                        },
                        None => 0,
                    }
                }

                // TODO: Yuck!
                let input_device = unsafe { cubeb::DeviceId::from_raw(params.input_device as *const _) };
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

                let (input_shm, input_file) =
                    SharedMemWriter::new(&audioipc::get_shm_path("input"), SHM_AREA_SIZE)?;
                let (output_shm, output_file) =
                    SharedMemReader::new(&audioipc::get_shm_path("output"), SHM_AREA_SIZE)?;

                match context.stream_init(
                    &params,
                    Callback {
                        input_frame_size: input_frame_size,
                        output_frame_size: output_frame_size,
                        connection: conn2,
                        input_shm: input_shm,
                        output_shm: output_shm,
                    }
                ) {
                    Ok(stream) => {
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

                        self.connection
                            .send_with_fd(ClientMessage::StreamCreated(stm_tok), Some(conn1))
                            .unwrap();
                        // TODO: It'd be nicer to send these as part of
                        // StreamCreated, but that requires changing
                        // sendmsg/recvmsg to support multiple fds.
                        self.connection
                            .send_with_fd(ClientMessage::StreamCreatedInputShm, Some(input_file))
                            .unwrap();
                        self.connection
                            .send_with_fd(ClientMessage::StreamCreatedOutputShm, Some(output_file))
                            .unwrap();
                    },
                    Err(e) => {
                        self.send_error(e);
                    },
                }
            },

            &ServerMessage::StreamDestroy(stm_tok) => {
                self.streams.remove(stm_tok);
                self.connection
                    .send(ClientMessage::StreamDestroyed)
                    .unwrap();
            },

            &ServerMessage::StreamStart(stm_tok) => {
                let _ = self.streams[stm_tok].start();
                self.connection.send(ClientMessage::StreamStarted).unwrap();
            },
            &ServerMessage::StreamStop(stm_tok) => {
                let _ = self.streams[stm_tok].stop();
                self.connection.send(ClientMessage::StreamStopped).unwrap();
            },
            &ServerMessage::StreamGetPosition(stm_tok) => {
                match self.streams[stm_tok].position() {
                    Ok(position) => {
                        self.connection
                            .send(ClientMessage::StreamPosition(position))
                            .unwrap();
                    },
                    Err(e) => {
                        self.send_error(e);
                    },
                }
            },
            &ServerMessage::StreamGetLatency(stm_tok) => {
                match self.streams[stm_tok].latency() {
                    Ok(latency) => {
                        self.connection
                            .send(ClientMessage::StreamLatency(latency))
                            .unwrap();
                    },
                    Err(e) => self.send_error(e),
                }
            },
            &ServerMessage::StreamSetVolume(stm_tok, volume) => {
                let _ = self.streams[stm_tok].set_volume(volume);
                self.connection
                    .send(ClientMessage::StreamVolumeSet)
                    .unwrap();
            },
            &ServerMessage::StreamSetPanning(stm_tok, panning) => {
                let _ = self.streams[stm_tok].set_panning(panning);
                self.connection
                    .send(ClientMessage::StreamPanningSet)
                    .unwrap();
            },
            &ServerMessage::StreamGetCurrentDevice(stm_tok) => {
                let err = match self.streams[stm_tok].current_device() {
                    Ok(device) => {
                        // TODO: Yuck!
                        self.connection
                            .send(ClientMessage::StreamCurrentDevice(device.into()))
                            .unwrap();
                        None
                    },
                    Err(e) => Some(e),
                };
                if let Some(e) = err {
                    self.send_error(e);
                }
            },
            _ => {
                bail!("Unexpected Message");
            },
        }
        Ok(())
    }

    fn send_error(&mut self, error: cubeb::Error) {
        self.connection
            .send(ClientMessage::ContextError(error.raw_code()))
            .unwrap();
    }
}

pub struct Server {
    socket: UnixListener,
    // Ok(None)      - Server hasn't tried to create cubeb::Context.
    // Ok(Some(ctx)) - Server has successfully created cubeb::Context.
    // Err(_)        - Server has tried and failed to create cubeb::Context.
    //                 Don't try again.
    context: Result<Option<cubeb::Context>>,
    conns: Slab<ServerConn>
}

impl Server {
    pub fn new(socket: UnixListener) -> Server {
        Server {
            socket: socket,
            context: Ok(None),
            conns: Slab::with_capacity(16)
        }
    }

    fn accept(&mut self, poll: &mut mio::Poll) -> Result<()> {
        debug!("Server accepting connection");

        let client_socket = match self.socket.accept() {
            Err(e) => {
                error!("server accept error: {}", e);
                return Err(e.into());
            },
            Ok(None) => panic!("accept returned EAGAIN unexpectedly"),
            Ok(Some((socket, _))) => socket,
        };
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
            &self.conns[token].connection,
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
        if let Ok(None) = self.context {
            self.context = cubeb::Context::init("AudioIPC Server", None)
                .and_then(|ctx| Ok(Some(ctx)))
                .or_else(|err| Err(err.into()));
        }

        Ok(())
    }

    pub fn poll(&mut self, poll: &mut mio::Poll) -> Result<()> {
        let mut events = mio::Events::with_capacity(16);

        match poll.poll(&mut events, None) {
            Ok(_) => {},
            Err(e) => error!("server poll error: {}", e),
        }

        for event in events.iter() {
            match event.token() {
                SERVER => {
                    match self.accept(poll) {
                        Err(e) => {
                            error!("server accept error: {}", e);
                        },
                        _ => {},
                    };
                },
                QUIT => {
                    info!("Quitting Audio Server loop");
                    bail!("quit");
                },
                token => {
                    debug!("token {:?} ready", token);

                    let r = self.conns[token].process(poll, &self.context);

                    debug!("got {:?}", r);

                    // TODO: Handle disconnection etc.
                    // TODO: Should be handled at a higher level by a
                    // disconnect message.
                    if let Err(e) = r {
                        debug!("dropped client {:?} due to error {:?}", token, e);
                        self.conns.remove(token);
                        continue;
                    }

                    // poll.reregister(
                    //     &self.conn(token).connection,
                    //     token,
                    //     mio::Ready::readable(),
                    //     mio::PollOpt::edge() | mio::PollOpt::oneshot()
                    // ).unwrap();
                },
            }
        }

        Ok(())
    }
}


// TODO: This should take an "Evented" instead of opening the UDS path
// directly (and let caller set up the Evented), but need a way to describe
// it as an Evented that we can send/recv file descriptors (or HANDLEs on
// Windows) over.
pub fn run(running: Arc<AtomicBool>) -> Result<()> {

    // Ignore result.
    let _ = std::fs::remove_file(audioipc::get_uds_path());

    // TODO: Use a SEQPACKET, wrap it in UnixStream?
    let mut poll = mio::Poll::new()?;
    let mut server = Server::new(UnixListener::bind(audioipc::get_uds_path())?);

    poll.register(
        &server.socket,
        SERVER,
        mio::Ready::readable(),
        mio::PollOpt::edge()
    ).unwrap();

    loop {
        if !running.load(Ordering::SeqCst) {
            bail!("server quit due to ctrl-c");
        }

        let _ = try!(server.poll(&mut poll));
    }

    //poll.deregister(&server.socket).unwrap();
}

#[no_mangle]
pub extern "C" fn audioipc_server_start() -> *mut c_void {

    let (tx, rx) = channel::ctl_pair();

    thread::spawn(move || {
        // Ignore result.
        let _ = std::fs::remove_file(audioipc::get_uds_path());

        // TODO: Use a SEQPACKET, wrap it in UnixStream?
        let mut poll = mio::Poll::new().unwrap();
        let mut server = Server::new(UnixListener::bind(audioipc::get_uds_path()).unwrap());

        poll.register(
            &server.socket,
            SERVER,
            mio::Ready::readable(),
            mio::PollOpt::edge()
        ).unwrap();

        poll.register(&rx, QUIT, mio::Ready::readable(), mio::PollOpt::edge())
            .unwrap();

        loop {
            match server.poll(&mut poll) {
                Err(_) => {
                    return;
                },
                _ => (),
            }
        }
    });

    Box::into_raw(Box::new(tx)) as *mut _
}

#[no_mangle]
pub extern "C" fn audioipc_server_stop(p: *mut c_void) {
    // Dropping SenderCtl here will notify the other end.
    let _ = unsafe { Box::<channel::SenderCtl>::from_raw(p as *mut _) };
}

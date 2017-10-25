use {AutoCloseFd, RecvFd, SendFd};
use async::{Async, AsyncRecvFd};
use bytes::{BufMut, BytesMut};
use codec::{Decoder, encode};
use errors::*;
use mio::{Poll, PollOpt, Ready, Token};
use mio::event::Evented;
use mio::unix::EventedFd;
use serde::de::DeserializeOwned;
use serde::ser::Serialize;
use std::collections::VecDeque;
use std::error::Error;
use std::fmt::Debug;
use std::io::{self, Read};
use std::os::unix::io::{AsRawFd, RawFd};
use std::os::unix::net;
use std::os::unix::prelude::*;

// Because of the trait implementation rules in Rust, this needs to be
// a wrapper class to allow implementation of a trait from another
// crate on a struct from yet another crate.
//
// This class is effectively net::UnixStream.

#[derive(Debug)]
pub struct Connection {
    stream: net::UnixStream,
    recv_buffer: BytesMut,
    recv_fd: VecDeque<AutoCloseFd>,
    send_buffer: BytesMut,
    decoder: Decoder
}

impl Connection {
    pub fn new(stream: net::UnixStream) -> Connection {
        info!("Create new connection");
        stream.set_nonblocking(false).unwrap();
        Connection {
            stream: stream,
            recv_buffer: BytesMut::with_capacity(1024),
            recv_fd: VecDeque::new(),
            send_buffer: BytesMut::with_capacity(1024),
            decoder: Decoder::new()
        }
    }

    /// Creates an unnamed pair of connected sockets.
    ///
    /// Returns two `Connection`s which are connected to each other.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// use audioipc::Connection;
    ///
    /// let (conn1, conn2) = match Connection::pair() {
    ///     Ok((conn1, conn2)) => (conn1, conn2),
    ///     Err(e) => {
    ///         println!("Couldn't create a pair of connections: {:?}", e);
    ///         return
    ///     }
    /// };
    /// ```
    pub fn pair() -> io::Result<(Connection, Connection)> {
        let (s1, s2) = net::UnixStream::pair()?;
        Ok((Connection::new(s1), Connection::new(s2)))
    }

    pub fn take_fd(&mut self) -> Option<RawFd> {
        self.recv_fd.pop_front().map(|fd| fd.into_raw_fd())
    }

    pub fn receive<RT>(&mut self) -> Result<RT>
    where
        RT: DeserializeOwned + Debug,
    {
        self.receive_with_fd()
    }

    pub fn receive_with_fd<RT>(&mut self) -> Result<RT>
    where
        RT: DeserializeOwned + Debug,
    {
        trace!("received_with_fd...");
        loop {
            trace!("   recv_buffer = {:?}", self.recv_buffer);
            if !self.recv_buffer.is_empty() {
                let r = self.decoder.decode(&mut self.recv_buffer);
                trace!("receive {:?}", r);
                match r {
                    Ok(Some(r)) => return Ok(r),
                    Ok(None) => {
                        /* Buffer doesn't contain enough data for a complete
                         * message, so need to enter recv_buf_fd to get more. */
                    },
                    Err(e) => return Err(e).chain_err(|| "Failed to deserialize message"),
                }
            }

            // Otherwise, try to read more data and try again. Make sure we've
            // got room for at least one byte to read to ensure that we don't
            // get a spurious 0 that looks like EOF

            // The decoder.decode should have reserved an amount for
            // the next bit it needs to read.  Check that we reserved
            // enough space for, at least the 2 byte size prefix.
            assert!(self.recv_buffer.remaining_mut() > 2);

            // TODO: Read until block, EOF, or error.
            // TODO: Switch back to recv_fd.
            match self.stream.recv_buf_fd(&mut self.recv_buffer) {
                Ok(Async::Ready((0, _))) => return Err(ErrorKind::Disconnected.into()),
                // TODO: Handle partial read?
                Ok(Async::Ready((_, fd))) => {
                    trace!(
                        "   recv_buf_fd: recv_buffer: {:?}, recv_fd: {:?}, fd: {:?}",
                        self.recv_buffer,
                        self.recv_fd,
                        fd
                    );
                    if let Some(fd) = fd {
                        self.recv_fd.push_back(
                            unsafe { AutoCloseFd::from_raw_fd(fd) }
                        );
                    }
                },
                Ok(Async::NotReady) => bail!("Socket should be blocking."),
                // TODO: Handle dropped message.
                // Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => panic!("wouldblock"),
                Err(e) => bail!("stream recv_buf_fd returned: {}", e.description()),
            }
        }
    }

    pub fn send<ST>(&mut self, msg: ST) -> Result<usize>
    where
        ST: Serialize + Debug,
    {
        self.send_with_fd::<ST, Connection>(msg, None)
    }

    pub fn send_with_fd<ST, FD>(&mut self, msg: ST, fd_to_send: Option<FD>) -> Result<usize>
    where
        ST: Serialize + Debug,
        FD: IntoRawFd + Debug,
    {
        trace!("send_with_fd {:?}, {:?}", msg, fd_to_send);
        try!(encode(&mut self.send_buffer, &msg));
        let fd_to_send = fd_to_send.map(|fd| fd.into_raw_fd());
        let send = self.send_buffer.take().freeze();
        self.stream.send_fd(send.as_ref(), fd_to_send).chain_err(
            || "Failed to send message with fd"
        )
    }
}

impl Evented for Connection {
    fn register(&self, poll: &Poll, token: Token, events: Ready, opts: PollOpt) -> io::Result<()> {
        EventedFd(&self.stream.as_raw_fd()).register(poll, token, events, opts)
    }

    fn reregister(&self, poll: &Poll, token: Token, events: Ready, opts: PollOpt) -> io::Result<()> {
        EventedFd(&self.stream.as_raw_fd()).reregister(poll, token, events, opts)
    }

    fn deregister(&self, poll: &Poll) -> io::Result<()> {
        EventedFd(&self.stream.as_raw_fd()).deregister(poll)
    }
}

impl Read for Connection {
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        self.stream.read(bytes)
    }
}

// TODO: Is this required?
impl<'a> Read for &'a Connection {
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        (&self.stream).read(bytes)
    }
}

impl RecvFd for Connection {
    fn recv_fd(&mut self, buf_to_recv: &mut [u8]) -> io::Result<(usize, Option<RawFd>)> {
        self.stream.recv_fd(buf_to_recv)
    }
}

impl FromRawFd for Connection {
    unsafe fn from_raw_fd(fd: RawFd) -> Connection {
        Connection::new(net::UnixStream::from_raw_fd(fd))
    }
}

impl IntoRawFd for Connection {
    fn into_raw_fd(self) -> RawFd {
        self.stream.into_raw_fd()
    }
}

impl SendFd for Connection {
    fn send_fd(&mut self, buf_to_send: &[u8], fd_to_send: Option<RawFd>) -> io::Result<usize> {
        self.stream.send_fd(buf_to_send, fd_to_send)
    }
}

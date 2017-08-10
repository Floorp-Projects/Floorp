use bincode::{self, deserialize, serialize};
use errors::*;
use msg;
use mio::{Poll, PollOpt, Ready, Token};
use mio::event::Evented;
use mio::unix::EventedFd;
use serde::de::DeserializeOwned;
use serde::ser::Serialize;
use std::fmt::Debug;
use std::io::{self, Read};
use std::os::unix::io::{AsRawFd, RawFd};
use std::os::unix::net;
use std::os::unix::prelude::*;
use libc;
use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};

pub trait RecvFd {
    fn recv_fd(&mut self, bytes: &mut [u8]) -> io::Result<(usize, Option<RawFd>)>;
}

pub trait SendFd {
    fn send_fd<FD: IntoRawFd>(&mut self, bytes: &[u8], fd: Option<FD>) -> io::Result<(usize)>;
}

// Because of the trait implementation rules in Rust, this needs to be
// a wrapper class to allow implementation of a trait from another
// crate on a struct from yet another crate.
//
// This class is effectively net::UnixStream.

#[derive(Debug)]
pub struct Connection {
    stream: net::UnixStream
}

impl Connection {
    pub fn new(stream: net::UnixStream) -> Connection {
        info!("Create new connection");
        Connection {
            stream: stream
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
        Ok((
            Connection {
                stream: s1
            },
            Connection {
                stream: s2
            }
        ))
    }

    pub fn receive<RT>(&mut self) -> Result<RT>
    where
        RT: DeserializeOwned + Debug,
    {
        match self.receive_with_fd() {
            Ok((r, None)) => Ok(r),
            Ok((_, Some(_))) => panic!("unexpected fd received"),
            Err(e) => Err(e),
        }
    }

    pub fn receive_with_fd<RT>(&mut self) -> Result<(RT, Option<RawFd>)>
    where
        RT: DeserializeOwned + Debug,
    {
        // TODO: Check deserialize_from and serialize_into.
        let mut encoded = vec![0; 32 * 1024]; // TODO: Get max size from bincode, or at least assert.
        // TODO: Read until block, EOF, or error.
        // TODO: Switch back to recv_fd.
        match self.stream.recv_fd(&mut encoded) {
            Ok((0, _)) => Err(ErrorKind::Disconnected.into()),
            // TODO: Handle partial read?
            Ok((n, fd)) => {
                let r = deserialize(&encoded[..n]);
                debug!("receive {:?}", r);
                match r {
                    Ok(r) => Ok((r, fd)),
                    Err(e) => Err(e).chain_err(|| "Failed to deserialize message"),
                }
            },
            // TODO: Handle dropped message.
            // Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => panic!("wouldblock"),
            _ => bail!("socket write"),
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
        let encoded: Vec<u8> = serialize(&msg, bincode::Infinite)?;
        info!("send_with_fd {:?}, {:?}", msg, fd_to_send);
        self.stream.send_fd(&encoded, fd_to_send).chain_err(
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

impl RecvFd for net::UnixStream {
    fn recv_fd(&mut self, buf_to_recv: &mut [u8]) -> io::Result<(usize, Option<RawFd>)> {
        let length = self.read_u32::<LittleEndian>()?;

        msg::recvmsg(self.as_raw_fd(), &mut buf_to_recv[..length as usize])
    }
}

impl RecvFd for Connection {
    fn recv_fd(&mut self, buf_to_recv: &mut [u8]) -> io::Result<(usize, Option<RawFd>)> {
        self.stream.recv_fd(buf_to_recv)
    }
}

impl FromRawFd for Connection {
    unsafe fn from_raw_fd(fd: RawFd) -> Connection {
        Connection {
            stream: net::UnixStream::from_raw_fd(fd)
        }
    }
}

impl IntoRawFd for Connection {
    fn into_raw_fd(self) -> RawFd {
        self.stream.into_raw_fd()
    }
}

impl SendFd for net::UnixStream {
    fn send_fd<FD: IntoRawFd>(&mut self, buf_to_send: &[u8], fd_to_send: Option<FD>) -> io::Result<usize> {
        self.write_u32::<LittleEndian>(buf_to_send.len() as u32)?;

        let fd_to_send = fd_to_send.map(|fd| fd.into_raw_fd());
        let r = msg::sendmsg(self.as_raw_fd(), buf_to_send, fd_to_send);
        fd_to_send.map(|fd| unsafe { libc::close(fd) });
        r
    }
}

impl SendFd for Connection {
    fn send_fd<FD: IntoRawFd>(&mut self, buf_to_send: &[u8], fd_to_send: Option<FD>) -> io::Result<usize> {
        self.stream.send_fd(buf_to_send, fd_to_send)
    }
}

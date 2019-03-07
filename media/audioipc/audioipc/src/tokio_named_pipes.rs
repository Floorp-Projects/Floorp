// From https://github.com/alexcrichton/tokio-named-pipes/commit/3a22f8fc9a441b548aec25bd5df3b1e0ab99fabe
// License MIT/Apache-2.0
// Sloppily updated to be compatible with tokio_io
// To be replaced with tokio_named_pipes crate after tokio 0.1 update.
#![cfg(windows)]

extern crate bytes;
extern crate tokio_core;
extern crate mio_named_pipes;
extern crate futures;

use std::ffi::OsStr;
use std::fmt;
use std::io::{self, Read, Write};
use std::os::windows::io::*;

use futures::{Async, Poll};
use bytes::{BufMut, Buf};
#[allow(deprecated)]
use tokio_core::io::Io;
use tokio_io::{AsyncRead, AsyncWrite};
use tokio_core::reactor::{PollEvented, Handle};

pub struct NamedPipe {
    io: PollEvented<mio_named_pipes::NamedPipe>,
}

impl NamedPipe {
    pub fn new<P: AsRef<OsStr>>(p: P, handle: &Handle) -> io::Result<NamedPipe> {
        NamedPipe::_new(p.as_ref(), handle)
    }

    fn _new(p: &OsStr, handle: &Handle) -> io::Result<NamedPipe> {
        let inner = try!(mio_named_pipes::NamedPipe::new(p));
        NamedPipe::from_pipe(inner, handle)
    }

    pub fn from_pipe(pipe: mio_named_pipes::NamedPipe,
                     handle: &Handle)
                     -> io::Result<NamedPipe> {
        Ok(NamedPipe {
            io: try!(PollEvented::new(pipe, handle)),
        })
    }

    pub fn connect(&self) -> io::Result<()> {
        self.io.get_ref().connect()
    }

    pub fn disconnect(&self) -> io::Result<()> {
        self.io.get_ref().disconnect()
    }

    pub fn need_read(&self) {
        self.io.need_read()
    }

    pub fn need_write(&self) {
        self.io.need_write()
    }

    pub fn poll_read(&self) -> Async<()> {
        self.io.poll_read()
    }

    pub fn poll_write(&self) -> Async<()> {
        self.io.poll_write()
    }
}

impl Read for NamedPipe {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.io.read(buf)
    }
}

impl Write for NamedPipe {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        self.io.write(buf)
    }
    fn flush(&mut self) -> io::Result<()> {
        self.io.flush()
    }
}

#[allow(deprecated)]
impl Io for NamedPipe {
    fn poll_read(&mut self) -> Async<()> {
        <NamedPipe>::poll_read(self)
    }

    fn poll_write(&mut self) -> Async<()> {
        <NamedPipe>::poll_write(self)
    }
}

impl AsyncRead for NamedPipe {
    unsafe fn prepare_uninitialized_buffer(&self, _: &mut [u8]) -> bool {
        false
    }

    fn read_buf<B: BufMut>(&mut self, buf: &mut B) -> Poll<usize, io::Error> {
        if NamedPipe::poll_read(self).is_not_ready() {
            return Ok(Async::NotReady)
        }

        let mut stack_buf = [0u8; 1024];
        let bytes_read = self.io.read(&mut stack_buf);
        match bytes_read {
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                self.io.need_read();
                return Ok(Async::NotReady);
            },
            Err(e) => Err(e),
            Ok(bytes_read) => {
                buf.put_slice(&stack_buf[0..bytes_read]);
                Ok(Async::Ready(bytes_read))
            }
        }
    }
}

impl AsyncWrite for NamedPipe {
    fn shutdown(&mut self) -> Poll<(), io::Error> {
        Ok(().into())
    }

    fn write_buf<B: Buf>(&mut self, buf: &mut B) -> Poll<usize, io::Error> {
        if NamedPipe::poll_write(self).is_not_ready() {
            return Ok(Async::NotReady)
        }

        let bytes_wrt = self.io.write(buf.bytes());
        match bytes_wrt {
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                self.io.need_write();
                return Ok(Async::NotReady);
            },
            Err(e) => Err(e),
            Ok(bytes_wrt) => {
                buf.advance(bytes_wrt);
                Ok(Async::Ready(bytes_wrt))
            }
        }
    }

}

impl<'a> Read for &'a NamedPipe {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        (&self.io).read(buf)
    }
}

impl<'a> Write for &'a NamedPipe {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        (&self.io).write(buf)
    }

    fn flush(&mut self) -> io::Result<()> {
        (&self.io).flush()
    }
}

impl fmt::Debug for NamedPipe {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.io.get_ref().fmt(f)
    }
}

impl AsRawHandle for NamedPipe {
    fn as_raw_handle(&self) -> RawHandle {
        self.io.get_ref().as_raw_handle()
    }
}

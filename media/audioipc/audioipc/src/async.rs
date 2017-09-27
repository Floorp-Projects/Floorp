// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

//! Various async helpers modelled after futures-rs and tokio-io.

use {RecvFd, SendFd};
use bytes::{Buf, BufMut};
use mio_uds;
use std::io as std_io;
use std::os::unix::io::RawFd;
use std::os::unix::net;

/// A convenience macro for working with `io::Result<T>` from the
/// `std::io::Read` and `std::io::Write` traits.
///
/// This macro takes `io::Result<T>` as input, and returns `T` as the output. If
/// the input type is of the `Err` variant, then `Async::NotReady` is returned if
/// it indicates `WouldBlock` or otherwise `Err` is returned.
#[macro_export]
macro_rules! try_nb {
    ($e:expr) => (match $e {
        Ok(t) => t,
        Err(ref e) if e.kind() == ::std::io::ErrorKind::WouldBlock => {
            return Ok(Async::NotReady)
        }
        Err(e) => return Err(e.into()),
    })
}

/////////////////////////////////////////////////////////////////////////////////////////
// Async support - Handle EWOULDBLOCK/EAGAIN from non-blocking I/O operations.

/// Return type for async methods, indicates whether the operation was
/// ready or not.
///
/// * `Ok(Async::Ready(t))` means that the operation has completed successfully.
/// * `Ok(Async::NotReady)` means that the underlying system is not ready to handle operation.
/// * `Err(e)` means that the operation has completed with the given error `e`.
pub type AsyncResult<T, E> = Result<Async<T>, E>;

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum Async<T> {
    /// Represents that a value is immediately ready.
    Ready(T),
    /// Represents that a value is not ready yet, but may be so later.
    NotReady
}

impl<T> Async<T> {
    pub fn is_ready(&self) -> bool {
        match *self {
            Async::Ready(_) => true,
            Async::NotReady => false,
        }
    }

    pub fn is_not_ready(&self) -> bool {
        !self.is_ready()
    }
}

/// Return type for an async attempt to send a value.
///
/// * `Ok(AsyncSend::Ready)` means that the operation has completed successfully.
/// * `Ok(AsyncSend::NotReady(t))` means that the underlying system is not ready to handle
///    send. returns the value that tried to be sent in `t`.
/// * `Err(e)` means that operation has completed with the given error `e`.
pub type AsyncSendResult<T, E> = Result<AsyncSend<T>, E>;

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum AsyncSend<T> {
    Ready,
    NotReady(T)
}

pub trait AsyncRecvFd: RecvFd {
    unsafe fn prepare_uninitialized_buffer(&self, bytes: &mut [u8]) -> bool {
        for byte in bytes.iter_mut() {
            *byte = 0;
        }

        true
    }

    /// Pull some bytes from this source into the specified `Buf`, returning
    /// how many bytes were read.
    ///
    /// The `buf` provided will have bytes read into it and the internal cursor
    /// will be advanced if any bytes were read. Note that this method typically
    /// will not reallocate the buffer provided.
    fn recv_buf_fd<B>(&mut self, buf: &mut B) -> AsyncResult<(usize, Option<RawFd>), std_io::Error>
    where
        Self: Sized,
        B: BufMut,
    {
        if !buf.has_remaining_mut() {
            return Ok(Async::Ready((0, None)));
        }

        unsafe {
            let (n, fd) = {
                let bytes = buf.bytes_mut();
                self.prepare_uninitialized_buffer(bytes);
                try_nb!(self.recv_fd(bytes))
            };

            buf.advance_mut(n);
            Ok(Async::Ready((n, fd)))
        }
    }
}

impl AsyncRecvFd for net::UnixStream {}
impl AsyncRecvFd for mio_uds::UnixStream {}

/// A trait for writable objects which operated in an async fashion.
///
/// This trait inherits from `std::io::Write` and indicates that an I/O object is
/// **nonblocking**, meaning that it will return an error instead of blocking
/// when bytes cannot currently be written, but hasn't closed. Specifically
/// this means that the `write` function for types that implement this trait
/// can have a few return values:
///
/// * `Ok(n)` means that `n` bytes of data was immediately written .
/// * `Err(e) if e.kind() == ErrorKind::WouldBlock` means that no data was
///   written from the buffer provided. The I/O object is not currently
///   writable but may become writable in the future.
/// * `Err(e)` for other errors are standard I/O errors coming from the
///   underlying object.
pub trait AsyncSendFd: SendFd {
    /// Write a `Buf` into this value, returning how many bytes were written.
    ///
    /// Note that this method will advance the `buf` provided automatically by
    /// the number of bytes written.
    fn send_buf_fd<B>(&mut self, buf: &mut B, fd: Option<RawFd>) -> AsyncResult<usize, std_io::Error>
    where
        Self: Sized,
        B: Buf,
    {
        if !buf.has_remaining() {
            return Ok(Async::Ready(0));
        }

        let n = try_nb!(self.send_fd(buf.bytes(), fd));
        buf.advance(n);
        Ok(Async::Ready(n))
    }
}

impl AsyncSendFd for net::UnixStream {}
impl AsyncSendFd for mio_uds::UnixStream {}

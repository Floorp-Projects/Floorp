//! Thread safe communication channel implementing `Evented`

use lazycell::{AtomicLazyCell, LazyCell};
use mio::{Evented, Poll, PollOpt, Ready, Registration, SetReadiness, Token};
use std::{fmt, io};
use std::any::Any;
use std::error;
use std::sync::{Arc, mpsc};
use std::sync::atomic::{AtomicUsize, Ordering};

pub fn ctl_pair() -> (SenderCtl, ReceiverCtl) {
    let inner = Arc::new(Inner {
        pending: AtomicUsize::new(0),
        senders: AtomicUsize::new(1),
        set_readiness: AtomicLazyCell::new()
    });

    let tx = SenderCtl {
        inner: inner.clone()
    };

    let rx = ReceiverCtl {
        registration: LazyCell::new(),
        inner: inner
    };

    (tx, rx)
}

/// Tracks messages sent on a channel in order to update readiness.
pub struct SenderCtl {
    inner: Arc<Inner>
}

/// Tracks messages received on a channel in order to track readiness.
pub struct ReceiverCtl {
    registration: LazyCell<Registration>,
    inner: Arc<Inner>
}

pub enum SendError<T> {
    Io(io::Error),
    Disconnected(T)
}

pub enum TrySendError<T> {
    Io(io::Error),
    Full(T),
    Disconnected(T)
}

struct Inner {
    // The number of outstanding messages for the receiver to read
    pending: AtomicUsize,
    // The number of sender handles
    senders: AtomicUsize,
    // The set readiness handle
    set_readiness: AtomicLazyCell<SetReadiness>
}

/*
 *
 * ===== SenderCtl / ReceiverCtl =====
 *
 */

impl SenderCtl {
    /// Call to track that a message has been sent
    pub fn inc(&self) -> io::Result<()> {
        let cnt = self.inner.pending.fetch_add(1, Ordering::Acquire);

        if 0 == cnt {
            // Toggle readiness to readable
            if let Some(set_readiness) = self.inner.set_readiness.borrow() {
                try!(set_readiness.set_readiness(Ready::readable()));
            }
        }

        Ok(())
    }
}

impl Clone for SenderCtl {
    fn clone(&self) -> SenderCtl {
        self.inner.senders.fetch_add(1, Ordering::Relaxed);
        SenderCtl {
            inner: self.inner.clone()
        }
    }
}

impl Drop for SenderCtl {
    fn drop(&mut self) {
        if self.inner.senders.fetch_sub(1, Ordering::Release) == 1 {
            let _ = self.inc();
        }
    }
}

impl Evented for ReceiverCtl {
    fn register(&self, poll: &Poll, token: Token, interest: Ready, opts: PollOpt) -> io::Result<()> {
        if self.registration.borrow().is_some() {
            return Err(io::Error::new(
                io::ErrorKind::Other,
                "receiver already registered"
            ));
        }

        let (registration, set_readiness) = Registration::new2();
        try!(registration.register(poll, token, interest, opts));

        if self.inner.pending.load(Ordering::Relaxed) > 0 {
            // TODO: Don't drop readiness
            let _ = set_readiness.set_readiness(Ready::readable());
        }

        self.registration.fill(registration).expect(
            "unexpected state encountered"
        );
        self.inner.set_readiness.fill(set_readiness).expect(
            "unexpected state encountered"
        );

        Ok(())
    }

    fn reregister(&self, poll: &Poll, token: Token, interest: Ready, opts: PollOpt) -> io::Result<()> {
        match self.registration.borrow() {
            Some(registration) => registration.reregister(poll, token, interest, opts),
            None => Err(io::Error::new(
                io::ErrorKind::Other,
                "receiver not registered"
            )),
        }
    }

    fn deregister(&self, poll: &Poll) -> io::Result<()> {
        match self.registration.borrow() {
            Some(registration) => <Registration as Evented>::deregister(registration, poll),
            None => Err(io::Error::new(
                io::ErrorKind::Other,
                "receiver not registered"
            )),
        }
    }
}

/*
 *
 * ===== Error conversions =====
 *
 */

impl<T> From<mpsc::SendError<T>> for SendError<T> {
    fn from(src: mpsc::SendError<T>) -> SendError<T> {
        SendError::Disconnected(src.0)
    }
}

impl<T> From<io::Error> for SendError<T> {
    fn from(src: io::Error) -> SendError<T> {
        SendError::Io(src)
    }
}

impl<T> From<mpsc::TrySendError<T>> for TrySendError<T> {
    fn from(src: mpsc::TrySendError<T>) -> TrySendError<T> {
        match src {
            mpsc::TrySendError::Full(v) => TrySendError::Full(v),
            mpsc::TrySendError::Disconnected(v) => TrySendError::Disconnected(v),
        }
    }
}

impl<T> From<mpsc::SendError<T>> for TrySendError<T> {
    fn from(src: mpsc::SendError<T>) -> TrySendError<T> {
        TrySendError::Disconnected(src.0)
    }
}

impl<T> From<io::Error> for TrySendError<T> {
    fn from(src: io::Error) -> TrySendError<T> {
        TrySendError::Io(src)
    }
}

/*
 *
 * ===== Implement Error, Debug and Display for Errors =====
 *
 */

impl<T: Any> error::Error for SendError<T> {
    fn description(&self) -> &str {
        match *self {
            SendError::Io(ref io_err) => io_err.description(),
            SendError::Disconnected(..) => "Disconnected",
        }
    }
}

impl<T: Any> error::Error for TrySendError<T> {
    fn description(&self) -> &str {
        match *self {
            TrySendError::Io(ref io_err) => io_err.description(),
            TrySendError::Full(..) => "Full",
            TrySendError::Disconnected(..) => "Disconnected",
        }
    }
}

impl<T> fmt::Debug for SendError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        format_send_error(self, f)
    }
}

impl<T> fmt::Display for SendError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        format_send_error(self, f)
    }
}

impl<T> fmt::Debug for TrySendError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        format_try_send_error(self, f)
    }
}

impl<T> fmt::Display for TrySendError<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        format_try_send_error(self, f)
    }
}

#[inline]
fn format_send_error<T>(e: &SendError<T>, f: &mut fmt::Formatter) -> fmt::Result {
    match *e {
        SendError::Io(ref io_err) => write!(f, "{}", io_err),
        SendError::Disconnected(..) => write!(f, "Disconnected"),
    }
}

#[inline]
fn format_try_send_error<T>(e: &TrySendError<T>, f: &mut fmt::Formatter) -> fmt::Result {
    match *e {
        TrySendError::Io(ref io_err) => write!(f, "{}", io_err),
        TrySendError::Full(..) => write!(f, "Full"),
        TrySendError::Disconnected(..) => write!(f, "Disconnected"),
    }
}

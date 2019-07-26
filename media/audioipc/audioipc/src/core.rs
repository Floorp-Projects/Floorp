// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

// Ease accessing reactor::Core handles.

use futures::sync::oneshot;
use std::sync::mpsc;
use std::{fmt, io, thread};
use tokio::runtime::current_thread;

struct Inner {
    join: thread::JoinHandle<()>,
    shutdown: oneshot::Sender<()>,
}

pub struct CoreThread {
    inner: Option<Inner>,
    handle: current_thread::Handle,
}

impl CoreThread {
    pub fn handle(&self) -> current_thread::Handle {
        self.handle.clone()
    }
}

impl Drop for CoreThread {
    fn drop(&mut self) {
        debug!("Shutting down {:?}", self);
        if let Some(inner) = self.inner.take() {
            let _ = inner.shutdown.send(());
            drop(inner.join.join());
        }
    }
}

impl fmt::Debug for CoreThread {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // f.debug_tuple("CoreThread").field(&"...").finish()
        f.debug_tuple("CoreThread").field(&self.handle).finish()
    }
}

pub fn spawn_thread<S, F>(name: S, f: F) -> io::Result<CoreThread>
where
    S: Into<String>,
    F: FnOnce() -> io::Result<()> + Send + 'static,
{
    let (shutdown_tx, shutdown_rx) = oneshot::channel::<()>();
    let (handle_tx, handle_rx) = mpsc::channel::<current_thread::Handle>();

    let join = thread::Builder::new().name(name.into()).spawn(move || {
        let mut rt =
            current_thread::Runtime::new().expect("Failed to create current_thread::Runtime");
        let handle = rt.handle();
        drop(handle_tx.send(handle.clone()));

        rt.spawn(futures::future::lazy(|| {
            let _ = f();
            Ok(())
        }));

        let _ = rt.block_on(shutdown_rx);
        trace!("thread shutdown...");
    })?;

    let handle = handle_rx.recv().or_else(|_| {
        Err(io::Error::new(
            io::ErrorKind::Other,
            "Failed to receive remote handle from spawned thread",
        ))
    })?;

    Ok(CoreThread {
        inner: Some(Inner {
            join,
            shutdown: shutdown_tx,
        }),
        handle,
    })
}

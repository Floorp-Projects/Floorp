// Ease accessing reactor::Core handles.

use futures::{Future, IntoFuture};
use futures::sync::oneshot;
use std::{fmt, io, thread};
use std::sync::mpsc;
use tokio_core::reactor::{Core, Handle, Remote};

scoped_thread_local! {
    static HANDLE: Handle
}

pub fn handle() -> Handle {
    HANDLE.with(|handle| handle.clone())
}

pub fn spawn<F>(f: F)
where
    F: Future<Item = (), Error = ()> + 'static,
{
    HANDLE.with(|handle| handle.spawn(f))
}

pub fn spawn_fn<F, R>(f: F)
where
    F: FnOnce() -> R + 'static,
    R: IntoFuture<Item = (), Error = ()> + 'static,
{
    HANDLE.with(|handle| handle.spawn_fn(f))
}

struct Inner {
    join: thread::JoinHandle<()>,
    shutdown: oneshot::Sender<()>,
}

pub struct CoreThread {
    inner: Option<Inner>,
    remote: Remote,
}

impl CoreThread {
    pub fn remote(&self) -> Remote {
        self.remote.clone()
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
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // f.debug_tuple("CoreThread").field(&"...").finish()
        f.debug_tuple("CoreThread").field(&self.remote).finish()
    }
}

pub fn spawn_thread<S, F>(name: S, f: F) -> io::Result<CoreThread>
where
    S: Into<String>,
    F: FnOnce() -> io::Result<()> + Send + 'static,
{
    let (shutdown_tx, shutdown_rx) = oneshot::channel::<()>();
    let (remote_tx, remote_rx) = mpsc::channel::<Remote>();

    let join = try!(thread::Builder::new().name(name.into()).spawn(move || {
        let mut core = Core::new().expect("Failed to create reactor::Core");
        let handle = core.handle();
        let remote = handle.remote().clone();
        drop(remote_tx.send(remote));

        drop(HANDLE.set(&handle, || {
            f().and_then(|_| {
                let _ = core.run(shutdown_rx);
                Ok(())
            })
        }));
        trace!("thread shutdown...");
    }));

    let remote = try!(remote_rx.recv().or_else(|_| Err(io::Error::new(
        io::ErrorKind::Other,
        "Failed to receive remote handle from spawned thread"
    ))));

    Ok(CoreThread {
        inner: Some(Inner {
            join: join,
            shutdown: shutdown_tx,
        }),
        remote: remote,
    })
}

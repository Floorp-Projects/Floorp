/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 use futures_channel::oneshot::channel;
use libc::c_void;
use nserror::nsresult;
use nsstring::{nsACString, nsCString, nsCStringLike};
use std::io::{self, Error, ErrorKind};

trait ToIoResult {
    fn to_io_result(self) -> io::Result<()>;
}

impl ToIoResult for nsresult {
    fn to_io_result(self) -> io::Result<()> {
        if self.failed() {
            return Err(Error::new(ErrorKind::Other, self));
        } else {
            Ok(())
        }
    }
}

mod ffi {
    use super::*;

    pub fn load(
        path: &nsACString,
        callback: Option<extern "C" fn(*mut c_void, *mut nsACString, nsresult)>,
        closure: *mut c_void,
    ) -> io::Result<()> {
        extern "C" {
            fn L10nRegistryLoad(
                aPath: *const nsACString,
                callback: Option<extern "C" fn(*mut c_void, *mut nsACString, nsresult)>,
                closure: *mut c_void,
            ) -> nsresult;
        }
        unsafe { L10nRegistryLoad(path, callback, closure) }.to_io_result()
    }

    pub fn load_sync(path: &nsACString, result: &mut nsACString) -> io::Result<()> {
        extern "C" {
            fn L10nRegistryLoadSync(aPath: *const nsACString, aRetVal: *mut nsACString) -> nsresult;
        }
        unsafe { L10nRegistryLoadSync(path, result) }.to_io_result()
    }

    /// Swap the memory to take ownership of the string data
    #[allow(non_snake_case)]
    pub fn take_nsACString(s: &mut nsACString) -> nsCString {
        let mut result = nsCString::new();
        result.take_from(s);
        result
    }
}

struct CallbackClosure {
    cb: Box<dyn FnOnce(io::Result<&mut nsACString>)>,
}

impl CallbackClosure {
    fn new<CB>(cb: CB) -> *mut Self
    where
        CB: FnOnce(io::Result<&mut nsACString>) + 'static,
    {
        let boxed = Box::new(Self {
            cb: Box::new(cb) as Box<dyn FnOnce(io::Result<&mut nsACString>)>,
        });
        Box::into_raw(boxed)
    }

    fn call(self: Box<Self>, s: io::Result<&mut nsACString>) {
        (self.cb)(s)
    }
}

extern "C" fn load_async_cb(closure: *mut c_void, string: *mut nsACString, success: nsresult) {
    let result = success.to_io_result().map(|_| unsafe { &mut *string });
    let closure = unsafe {
        debug_assert_ne!(closure, std::ptr::null_mut());
        Box::from_raw(closure as *mut CallbackClosure)
    };
    closure.call(result);
}

pub async fn load_async(path: impl nsCStringLike) -> io::Result<nsCString> {
    let (sender, receiver) = channel::<io::Result<nsCString>>();

    let closure = CallbackClosure::new(move |result| {
        let result = result.map(ffi::take_nsACString);
        sender.send(result).expect("Failed to send result");
    });

    ffi::load(&*path.adapt(), Some(load_async_cb), closure as *mut c_void)?;

    let result = receiver
        .await
        .map_err(|_| Error::new(ErrorKind::Interrupted, "canceled"))?;
    result
}

pub fn load_sync(path: impl nsCStringLike) -> io::Result<nsCString> {
    let mut result = nsCString::new();
    ffi::load_sync(&*path.adapt(), &mut result)?;
    Ok(result)
}

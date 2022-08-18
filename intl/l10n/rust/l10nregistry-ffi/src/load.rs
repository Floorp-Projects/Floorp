/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use futures_channel::oneshot;
use nserror::{nsresult, NS_OK, NS_SUCCESS_ADOPTED_DATA};
use nsstring::{nsACString, nsCStringLike};
use std::{
    cell::Cell,
    ffi::c_void,
    io::{self, Error, ErrorKind},
    ptr,
};
use xpcom::{
    interfaces::{nsIStreamLoader, nsIStreamLoaderObserver, nsISupports},
    xpcom,
};

unsafe fn boxed_slice_from_raw(ptr: *mut u8, len: usize) -> Box<[u8]> {
    if ptr.is_null() {
        // It is undefined behaviour to create a `Box<[u8]>` with a null pointer,
        // so avoid that case.
        assert_eq!(len, 0);
        Box::new([])
    } else {
        Box::from_raw(ptr::slice_from_raw_parts_mut(ptr, len))
    }
}

#[xpcom(implement(nsIStreamLoaderObserver), nonatomic)]
struct StreamLoaderObserver {
    sender: Cell<Option<oneshot::Sender<Result<Box<[u8]>, nsresult>>>>,
}

impl StreamLoaderObserver {
    #[allow(non_snake_case)]
    unsafe fn OnStreamComplete(
        &self,
        _loader: *const nsIStreamLoader,
        _ctxt: *const nsISupports,
        status: nsresult,
        result_length: u32,
        result: *const u8,
    ) -> nsresult {
        let sender = match self.sender.take() {
            Some(sender) => sender,
            None => return NS_OK,
        };

        if status.failed() {
            sender.send(Err(status)).expect("Failed to send data");
            return NS_OK;
        }

        // safety: take ownership of the data passed in. This is OK because we
        // have configured Rust and C++ to use the same allocator, and our
        // caller won't free the `result` pointer if we return
        // NS_SUCCESS_ADOPTED_DATA.
        sender
            .send(Ok(boxed_slice_from_raw(
                result as *mut u8,
                result_length as usize,
            )))
            .expect("Failed to send data");
        NS_SUCCESS_ADOPTED_DATA
    }
}

extern "C" {
    fn L10nRegistryLoad(
        path: *const nsACString,
        observer: *const nsIStreamLoaderObserver,
    ) -> nsresult;

    fn L10nRegistryLoadSync(
        aPath: *const nsACString,
        aData: *mut *mut c_void,
        aSize: *mut u64,
    ) -> nsresult;
}

pub async fn load_async(path: impl nsCStringLike) -> io::Result<Box<[u8]>> {
    let (sender, receiver) = oneshot::channel::<Result<Box<[u8]>, nsresult>>();
    let observer = StreamLoaderObserver::allocate(InitStreamLoaderObserver {
        sender: Cell::new(Some(sender)),
    });
    unsafe {
        L10nRegistryLoad(&*path.adapt(), observer.coerce())
            .to_result()
            .map_err(|err| Error::new(ErrorKind::Other, err))?;
    }
    receiver
        .await
        .expect("Failed to receive from observer.")
        .map_err(|err| Error::new(ErrorKind::Other, err))
}

pub fn load_sync(path: impl nsCStringLike) -> io::Result<Box<[u8]>> {
    let mut data_ptr: *mut c_void = ptr::null_mut();
    let mut data_length: u64 = 0;
    unsafe {
        L10nRegistryLoadSync(&*path.adapt(), &mut data_ptr, &mut data_length)
            .to_result()
            .map_err(|err| Error::new(ErrorKind::Other, err))?;

        // The call succeeded, meaning `data_ptr` and `size` have been filled in with owning pointers to actual data payloads (or null).
        // If we get a null, return a successful read of the empty file.
        Ok(boxed_slice_from_raw(
            data_ptr as *mut u8,
            data_length as usize,
        ))
    }
}

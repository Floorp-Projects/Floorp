/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Nicer, Rust-y APIs for structured cloning.

use glue;
use jsapi;
use rust::Runtime;
use std::ptr;

/// An RAII owned buffer for structured cloning into and out of.
pub struct StructuredCloneBuffer {
    raw: *mut jsapi::JSAutoStructuredCloneBuffer,
}

impl StructuredCloneBuffer {
    /// Construct a new `StructuredCloneBuffer`.
    ///
    /// # Panics
    ///
    /// Panics if the underlying JSAPI calls fail.
    pub fn new(scope: jsapi::JS::StructuredCloneScope,
               callbacks: &jsapi::JSStructuredCloneCallbacks)
               -> StructuredCloneBuffer {
        let raw = unsafe {
            glue::NewJSAutoStructuredCloneBuffer(scope, callbacks)
        };
        assert!(!raw.is_null());
        StructuredCloneBuffer {
            raw: raw,
        }
    }

    /// Get the raw `*mut JSStructuredCloneData` owned by this buffer.
    pub fn data(&self) -> *mut jsapi::JSStructuredCloneData {
        unsafe {
            &mut (*self.raw).data_
        }
    }

    /// Copy this buffer's data into a vec.
    pub fn copy_to_vec(&self) -> Vec<u8> {
        let len = unsafe {
            glue::GetLengthOfJSStructuredCloneData(self.data())
        };
        let mut vec = Vec::with_capacity(len);
        unsafe {
            glue::CopyJSStructuredCloneData(self.data(), vec.as_mut_ptr());
        }
        vec
    }

    /// Read a JS value out of this buffer.
    pub fn read(&mut self,
                vp: jsapi::JS::MutableHandleValue,
                callbacks: &jsapi::JSStructuredCloneCallbacks)
                -> Result<(), ()> {
        if unsafe {
            (*self.raw).read(Runtime::get(), vp, jsapi::JS::CloneDataPolicy{ allowIntraClusterClonableSharedObjects_: false }, callbacks, ptr::null_mut())
        } {
            Ok(())
        } else {
            Err(())
        }
    }

    /// Write a JS value into this buffer.
    pub fn write(&mut self,
                 v: jsapi::JS::HandleValue,
                 callbacks: &jsapi::JSStructuredCloneCallbacks)
                 -> Result<(), ()> {
        if unsafe {
            (*self.raw).write(Runtime::get(), v, callbacks, ptr::null_mut())
        } {
            Ok(())
        } else {
            Err(())
        }
    }

    /// Copy the given slice into this buffer.
    pub fn write_bytes(&mut self, bytes: &[u8]) -> Result<(), ()> {
        let len = bytes.len();
        let src = bytes.as_ptr();
        if unsafe {
            glue::WriteBytesToJSStructuredCloneData(src, len, self.data())
        } {
            Ok(())
        } else {
            Err(())
        }
    }
}

impl Drop for StructuredCloneBuffer {
    fn drop(&mut self) {
        unsafe {
            glue::DeleteJSAutoStructuredCloneBuffer(self.raw);
        }
    }
}

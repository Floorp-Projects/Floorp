/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Stubs for the types contained in webgl_types.rs
//!
//! The API surface provided here should be roughly the same to the one provided
//! in webgl_types, modulo completely compiled-out stuff.

use api::DeviceIntSize;
use api::{GLContextAttributes, GLLimits};
use api::WebGLCommand;

pub struct GLContextHandleWrapper;

impl GLContextHandleWrapper {
    pub fn new_context(&self,
                       _: DeviceIntSize,
                       _: GLContextAttributes,
                       _: Option<Box<GLContextDispatcher>>) -> Result<GLContextWrapper, &'static str> {
        unreachable!()
    }

    pub fn current_native_handle() -> Option<GLContextHandleWrapper> {
        None
    }

    pub fn current_osmesa_handle() -> Option<GLContextHandleWrapper> {
        None
    }
}

pub struct GLContextWrapper;

impl GLContextWrapper {
    pub fn make_current(&self) {
        unreachable!()
    }

    pub fn unbind(&self) {
        unreachable!()
    }

    pub fn apply_command(&self, _: WebGLCommand) {
        unreachable!()
    }

    pub fn get_info(&self) -> (DeviceIntSize, u32, GLLimits) {
        unreachable!()
    }

    pub fn resize(&mut self, _: &DeviceIntSize) -> Result<(), &'static str> {
        unreachable!()
    }
}

pub trait GLContextDispatcher {
    fn dispatch(&self, Box<Fn() + Send>);
}

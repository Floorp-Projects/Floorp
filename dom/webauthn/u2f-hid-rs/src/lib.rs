/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
mod util;

#[cfg(any(target_os = "linux"))]
extern crate libudev;

#[cfg(any(target_os = "linux"))]
#[path = "linux/mod.rs"]
pub mod platform;

#[cfg(any(target_os = "macos"))]
extern crate core_foundation_sys;

#[cfg(any(target_os = "macos"))]
#[path = "macos/mod.rs"]
pub mod platform;

#[cfg(any(target_os = "windows"))]
#[path = "windows/mod.rs"]
pub mod platform;

#[cfg(any(target_os = "android"))]
#[path = "android/mod.rs"]
pub mod platform;

#[macro_use]
extern crate log;
extern crate rand;
extern crate libc;
extern crate boxfnonce;

mod consts;
mod runloop;
mod u2ftypes;
mod u2fprotocol;

mod manager;
pub use manager::U2FManager;

mod capi;
pub use capi::*;

#[cfg(fuzzing)]
pub use u2fprotocol::*;
#[cfg(fuzzing)]
pub use u2ftypes::*;
#[cfg(fuzzing)]
pub use consts::*;

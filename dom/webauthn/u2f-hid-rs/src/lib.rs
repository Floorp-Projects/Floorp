/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
mod util;

#[cfg(any(target_os = "linux", target_os = "freebsd"))]
pub mod hidproto;

#[cfg(any(target_os = "linux"))]
extern crate libudev;

#[cfg(any(target_os = "linux"))]
#[path = "linux/mod.rs"]
pub mod platform;

#[cfg(any(target_os = "freebsd"))]
extern crate devd_rs;

#[cfg(any(target_os = "freebsd"))]
#[path = "freebsd/mod.rs"]
pub mod platform;

#[cfg(any(target_os = "macos"))]
extern crate core_foundation_sys;

#[cfg(any(target_os = "macos"))]
extern crate core_foundation;

#[cfg(any(target_os = "macos"))]
#[path = "macos/mod.rs"]
pub mod platform;

#[cfg(any(target_os = "windows"))]
#[path = "windows/mod.rs"]
pub mod platform;

#[cfg(
    not(
        any(target_os = "linux", target_os = "freebsd", target_os = "macos", target_os = "windows")
    )
)]
#[path = "stub/mod.rs"]
pub mod platform;

extern crate boxfnonce;
extern crate libc;
#[macro_use]
extern crate log;
extern crate rand;
extern crate runloop;

#[macro_use]
extern crate bitflags;

mod consts;
mod statemachine;
mod u2fprotocol;
mod u2ftypes;

mod manager;
pub use manager::U2FManager;

mod capi;
pub use capi::*;

// Keep this in sync with the constants in u2fhid-capi.h.
bitflags! {
    pub struct RegisterFlags: u64 {
        const REQUIRE_RESIDENT_KEY        = 1;
        const REQUIRE_USER_VERIFICATION   = 2;
        const REQUIRE_PLATFORM_ATTACHMENT = 4;
    }
}
bitflags! {
    pub struct SignFlags: u64 {
        const REQUIRE_USER_VERIFICATION = 1;
    }
}
bitflags! {
    pub struct AuthenticatorTransports: u8 {
        const USB = 1;
        const NFC = 2;
        const BLE = 4;
    }
}

#[derive(Clone)]
pub struct KeyHandle {
    pub credential: Vec<u8>,
    pub transports: AuthenticatorTransports,
}

pub type AppId = Vec<u8>;
pub type RegisterResult = Vec<u8>;
pub type SignResult = (AppId, Vec<u8>, Vec<u8>);

#[derive(Debug, Clone, Copy)]
pub enum Error {
    Unknown = 1,
    NotSupported = 2,
    InvalidState = 3,
    ConstraintError = 4,
    NotAllowed = 5,
}

#[cfg(fuzzing)]
pub use consts::*;
#[cfg(fuzzing)]
pub use u2fprotocol::*;
#[cfg(fuzzing)]
pub use u2ftypes::*;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![cfg_attr(feature = "nightly", feature(nonzero))]
#![cfg_attr(feature = "cargo-clippy", allow(float_cmp, too_many_arguments, unreadable_literal))]

extern crate app_units;
extern crate bincode;
#[macro_use]
extern crate bitflags;
extern crate byteorder;
#[cfg(feature = "nightly")]
extern crate core;
#[cfg(target_os = "macos")]
extern crate core_foundation;
#[cfg(target_os = "macos")]
extern crate core_graphics;
#[cfg(target_os = "windows")]
extern crate dwrote;
extern crate euclid;
#[cfg(feature = "ipc")]
extern crate ipc_channel;
extern crate serde;
#[macro_use]
extern crate serde_derive;
extern crate time;


mod api;
pub mod channel;
mod color;
mod display_item;
mod display_list;
mod font;
mod image;
mod units;

pub use api::*;
pub use color::*;
pub use display_item::*;
pub use display_list::*;
pub use font::*;
pub use image::*;
pub use units::*;

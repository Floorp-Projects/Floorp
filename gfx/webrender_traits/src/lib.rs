/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![cfg_attr(feature = "nightly", feature(nonzero))]

extern crate app_units;
extern crate byteorder;
#[cfg(feature = "nightly")]
extern crate core;
extern crate euclid;
extern crate gleam;
#[macro_use]
extern crate heapsize;
#[cfg(feature = "ipc")]
extern crate ipc_channel;
extern crate offscreen_gl_context;
extern crate serde;
#[macro_use]
extern crate serde_derive;

#[cfg(target_os = "macos")]
extern crate core_graphics;

#[cfg(target_os = "windows")]
extern crate dwrote;

include!("types.rs");

mod units;
mod api;
pub mod channel;
mod display_item;
mod display_list;
mod stacking_context;
mod webgl;

pub use api::RenderApi;
pub use display_list::DisplayListBuilder;
pub use units::*;

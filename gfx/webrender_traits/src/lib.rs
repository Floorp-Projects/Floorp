/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![cfg_attr(feature = "nightly", feature(nonzero))]
#![cfg_attr(feature = "serde_derive", feature(proc_macro, rustc_attrs, structural_match))]

extern crate app_units;
extern crate byteorder;
#[cfg(feature = "nightly")]
extern crate core;
extern crate euclid;
extern crate gleam;
#[macro_use]
extern crate heapsize;
extern crate ipc_channel;
extern crate offscreen_gl_context;
extern crate serde;
#[cfg(feature = "serde_derive")]
#[macro_use]
extern crate serde_derive;

#[cfg(target_os = "macos")]
extern crate core_graphics;

#[cfg(target_os = "windows")]
extern crate dwrote;

#[cfg(feature = "serde_codegen")]
include!(concat!(env!("OUT_DIR"), "/types.rs"));

#[cfg(feature = "serde_derive")]
include!("types.rs");

mod api;
mod display_item;
mod display_list;
mod stacking_context;
mod webgl;

pub use api::RenderApi;
pub use display_list::{AuxiliaryListsBuilder, DisplayListBuilder};

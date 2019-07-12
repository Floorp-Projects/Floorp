/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The `webrender_api` crate contains an assortment types and functions used
//! by WebRender consumers as well as, in many cases, WebRender itself.
//!
//! This separation allows Servo to parallelize compilation across `webrender`
//! and other crates that depend on `webrender_api`. So in practice, we put
//! things in this crate when Servo needs to use them. Firefox depends on the
//! `webrender` crate directly, and so this distinction is not really relevant
//! there.

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
#[macro_use]
extern crate derive_more;
pub extern crate euclid;
#[cfg(feature = "ipc")]
extern crate ipc_channel;
#[macro_use]
extern crate malloc_size_of_derive;
extern crate serde;
#[macro_use]
extern crate serde_derive;
extern crate time;

extern crate malloc_size_of;

mod api;
pub mod channel;
mod color;
mod display_item;
mod display_list;
mod font;
mod gradient_builder;
mod image;
pub mod units;

pub use crate::api::*;
pub use crate::color::*;
pub use crate::display_item::*;
pub use crate::display_list::*;
pub use crate::font::*;
pub use crate::gradient_builder::*;
pub use crate::image::*;

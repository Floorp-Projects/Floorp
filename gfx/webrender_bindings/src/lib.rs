/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![deny(warnings)]

extern crate app_units;
extern crate bincode;
extern crate euclid;
extern crate fxhash;
extern crate gecko_profiler;
extern crate gleam;
extern crate nsstring;
extern crate num_cpus;
extern crate rayon;
extern crate swgl;
extern crate thin_vec;
extern crate tracy_rs;
extern crate uuid;
extern crate webrender;
extern crate wr_malloc_size_of;

#[macro_use]
extern crate log;

#[cfg(target_os = "windows")]
extern crate dwrote;
#[cfg(target_os = "windows")]
extern crate winapi;

#[cfg(target_os = "macos")]
extern crate core_foundation;
#[cfg(target_os = "macos")]
extern crate core_graphics;
#[cfg(target_os = "macos")]
extern crate foreign_types;

mod program_cache;

#[allow(non_snake_case)]
pub mod bindings;
pub mod moz2d_renderer;
mod swgl_bindings;

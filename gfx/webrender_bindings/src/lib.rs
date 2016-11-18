/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate webrender;
extern crate webrender_traits;
extern crate euclid;
extern crate app_units;
extern crate gleam;

#[cfg(target_os="macos")]
extern crate core_foundation;

#[cfg(target_os="windows")]
extern crate kernel32;
#[cfg(target_os="windows")]
extern crate winapi;

#[allow(non_snake_case)]
pub mod bindings;

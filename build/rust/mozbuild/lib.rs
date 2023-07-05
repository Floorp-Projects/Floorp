/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::path::Path;

// Path::new is not const at the moment. This is a non-generic version
// of Path::new, similar to libstd's implementation of Path::new.
#[inline(always)]
const fn const_path(s: &'static str) -> &'static std::path::Path {
    unsafe { &*(s as *const str as *const std::path::Path) }
}

pub const TOPOBJDIR: &Path = const_path(config::TOPOBJDIR);

pub const TOPSRCDIR: &Path = const_path(config::TOPSRCDIR);

pub mod config {
    include!(env!("BUILDCONFIG_RS"));
}

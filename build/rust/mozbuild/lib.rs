/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use once_cell::sync::Lazy;
use std::path::Path;

pub static TOPOBJDIR: Lazy<&'static Path> = Lazy::new(|| Path::new(config::TOPOBJDIR));

pub static TOPSRCDIR: Lazy<&'static Path> = Lazy::new(|| Path::new(config::TOPSRCDIR));

pub mod config {
    include!(env!("BUILDCONFIG_RS"));
}

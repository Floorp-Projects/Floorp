/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A glyph rasterizer for webrender
//!
//! ## Overview
//!
//! ## Usage
//!

#[cfg(any(target_os = "macos", target_os = "ios", target_os = "windows"))]
mod gamma_lut;
mod rasterizer;
mod telemetry;
mod types;

pub mod profiler;

pub use rasterizer::*;
pub use types::*;

#[macro_use]
extern crate malloc_size_of_derive;
#[macro_use]
extern crate tracy_rs;
#[macro_use]
extern crate log;
#[macro_use]
extern crate lazy_static;
#[macro_use]
extern crate smallvec;

#[cfg(any(feature = "serde"))]
#[macro_use]
extern crate serde;

extern crate malloc_size_of;

pub mod platform {
    #[cfg(any(target_os = "macos", target_os = "ios"))]
    pub use crate::platform::macos::font;
    #[cfg(any(target_os = "android", all(unix, not(any(target_os = "ios", target_os = "macos")))))]
    pub use crate::platform::unix::font;
    #[cfg(target_os = "windows")]
    pub use crate::platform::windows::font;

    #[cfg(any(target_os = "ios", target_os = "macos"))]
    pub mod macos {
        pub mod font;
    }
    #[cfg(any(target_os = "android", all(unix, not(any(target_os = "macos", target_os = "ios")))))]
    pub mod unix {
        pub mod font;
    }
    #[cfg(target_os = "windows")]
    pub mod windows {
        pub mod font;
    }
}

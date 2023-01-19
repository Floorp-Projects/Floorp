/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(feature = "gecko")]
use glean::TimerId;
#[cfg(feature = "gecko")]
use firefox_on_glean::metrics::wr;

#[cfg(not(feature = "gecko"))]
pub struct TimerId;

pub struct Telemetry;

/// Defines the interface for hooking up an external telemetry reporter to WR.
#[cfg(not(feature = "gecko"))]
impl Telemetry {
    // Start rasterize glyph time collection
    pub fn start_rasterize_glyphs_time() -> TimerId { return TimerId {}; }
    // End rasterize glyph time collection
    pub fn stop_and_accumulate_rasterize_glyphs_time(_id: TimerId) { }
}

#[cfg(feature = "gecko")]
impl Telemetry {
    pub fn start_rasterize_glyphs_time() -> TimerId { wr::rasterize_glyphs_time.start() }
    pub fn stop_and_accumulate_rasterize_glyphs_time(id: TimerId) { wr::rasterize_glyphs_time.stop_and_accumulate(id); }
}

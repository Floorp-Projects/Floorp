/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// A profiler for profiling rasterize time.
pub trait GlyphRasterizeProfiler {
    fn start_time(&mut self);

    fn end_time(&mut self) -> f64;

    fn set(&mut self, value: f64);
}

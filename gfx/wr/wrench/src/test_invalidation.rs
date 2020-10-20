/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::NotifierEvent;
use crate::WindowWrapper;
use std::path::PathBuf;
use std::sync::mpsc::Receiver;
use crate::wrench::{Wrench, WrenchThing};
use crate::yaml_frame_reader::YamlFrameReader;
use webrender::{PictureCacheDebugInfo, TileDebugInfo};
use webrender::api::units::*;

pub struct TestHarness<'a> {
    wrench: &'a mut Wrench,
    window: &'a mut WindowWrapper,
    rx: Receiver<NotifierEvent>,
}

// Convenience method to build a picture rect
fn pr(x: f32, y: f32, w: f32, h: f32) -> PictureRect {
    PictureRect::new(
        PicturePoint::new(x, y),
        PictureSize::new(w, h),
    )
}

impl<'a> TestHarness<'a> {
    pub fn new(
        wrench: &'a mut Wrench,
        window: &'a mut WindowWrapper,
        rx: Receiver<NotifierEvent>
    ) -> Self {
        TestHarness {
            wrench,
            window,
            rx,
        }
    }

    /// Main entry point for invalidation tests
    pub fn run(
        mut self,
    ) {
        // List all invalidation tests here
        self.test_basic();
    }

    /// Simple validation / proof of concept of invalidation testing
    fn test_basic(
        &mut self,
    ) {
        // Render basic.yaml, ensure that the valid/dirty rects are as expected
        let results = self.render_yaml("basic");
        let tile_info = results.slice(0).tile(0, 0).as_dirty();
        assert_eq!(
            tile_info.local_valid_rect,
            pr(100.0, 100.0, 500.0, 100.0),
        );
        assert_eq!(
            tile_info.local_dirty_rect,
            pr(100.0, 100.0, 500.0, 100.0),
        );

        // Render it again and ensure the tile was considered valid (no rasterization was done)
        let results = self.render_yaml("basic");
        assert_eq!(*results.slice(0).tile(0, 0), TileDebugInfo::Valid);
    }

    /// Render a YAML file, and return the picture cache debug info
    fn render_yaml(
        &mut self,
        filename: &str,
    ) -> PictureCacheDebugInfo {
        let path = format!("invalidation/{}.yaml", filename);
        let mut reader = YamlFrameReader::new(&PathBuf::from(path));

        reader.do_frame(self.wrench);
        self.rx.recv().unwrap();
        let results = self.wrench.render();
        self.window.swap_buffers();

        results.picture_cache_debug
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use std::collections::HashMap;
use webrender_traits::{FontKey, FontRenderMode, GlyphDimensions};

use dwrote;

lazy_static! {
    static ref DEFAULT_FONT_DESCRIPTOR: dwrote::FontDescriptor = dwrote::FontDescriptor {
        family_name: "Arial".to_owned(),
        weight: dwrote::FontWeight::Regular,
        stretch: dwrote::FontStretch::Normal,
        style: dwrote::FontStyle::Normal,
    };
}

pub struct FontContext {
    fonts: HashMap<FontKey, dwrote::Font>,
    gdi_interop: dwrote::GdiInterop,
    main_display_rendering_params: dwrote::RenderingParams,
}

pub struct RasterizedGlyph {
    pub width: u32,
    pub height: u32,
    pub bytes: Vec<u8>,
}

impl FontContext {
    pub fn new() -> FontContext {
        FontContext {
            fonts: HashMap::new(),
            gdi_interop: dwrote::GdiInterop::create(),
            main_display_rendering_params: dwrote::RenderingParams::create_for_primary_monitor(),
        }
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, _: &[u8]) {
        if self.fonts.contains_key(font_key) {
            return
        }

        debug!("DWrite WR backend can't do raw fonts yet, using Arial instead");

        self.add_native_font(font_key, DEFAULT_FONT_DESCRIPTOR.clone());
    }

    pub fn add_native_font(&mut self, font_key: &FontKey, font_handle: dwrote::FontDescriptor) {
        if self.fonts.contains_key(font_key) {
            return
        }

        let system_fc = dwrote::FontCollection::system();
        let realized_font = system_fc.get_font_from_descriptor(&font_handle).unwrap();
        self.fonts.insert((*font_key).clone(), realized_font);
    }

    fn get_glyph_dimensions_and_maybe_rasterize(&self,
                                                font_key: FontKey,
                                                size: Au,
                                                glyph: u32,
                                                device_pixel_ratio: f32,
                                                render_mode: Option<FontRenderMode>)
                                                -> (Option<GlyphDimensions>, Option<RasterizedGlyph>)
    {
        let font = self.fonts.get(&font_key).unwrap();
        let face = font.create_font_face();
        let glyph = glyph as u16;

        let gm = face.get_design_glyph_metrics(&[glyph], false)[0];

        let em_size = size.to_f32_px() / 16.; // (16px per em)
        let du_per_pixel = face.metrics().designUnitsPerEm as f32 / 16.; // (once again, 16px per em)
        let scaled_du_to_pixels = (em_size * device_pixel_ratio) / du_per_pixel;

        let width = (gm.advanceWidth as i32 - (gm.leftSideBearing + gm.rightSideBearing)) as f32
            * scaled_du_to_pixels;
        let height = (gm.advanceHeight as i32 - (gm.topSideBearing + gm.bottomSideBearing)) as f32
            * scaled_du_to_pixels;
        let x = (gm.leftSideBearing) as f32
            * scaled_du_to_pixels;
        let y = (gm.verticalOriginY - gm.topSideBearing) as f32
            * scaled_du_to_pixels;

        // Round the glyph rect outwards.  These values are in device pixels.
        let x0_i = x.floor() as i32;
        let y0_i = y.floor() as i32;
        let x1_i = (x + width).ceil() as i32;
        let y1_i = (y + height).ceil() as i32;
        let width_u = (x1_i - x0_i) as u32;
        let height_u = (y1_i - y0_i) as u32;

        let dims = GlyphDimensions {
            left: x0_i, top: y0_i,
            width: width_u, height: height_u,
        };

        // blank glyphs are None
        if width_u == 0 || height_u == 0 {
            return (None, None)
        }

        // if we weren't asked to rasterize, we're done
        if render_mode.is_none() {
            return (Some(dims), None);
        }

        // the coords and size to draw_glyph_run are in 1/96ths of an inch

        // size is in app units, which we convert to CSS pixels (1/96"), which
        // is the same as DIPs.
        let size_dip = size.to_f32_px() * device_pixel_ratio;

        let rt = self.gdi_interop.create_bitmap_render_target(width_u, height_u);
        rt.set_pixels_per_dip(1.);
        rt.draw_glyph_run(-x, y,
                          dwrote::DWRITE_MEASURING_MODE_NATURAL,
                          &face, size_dip,
                          &[glyph], &[0.0],
                          &[dwrote::GlyphOffset { advanceOffset: 0., ascenderOffset: 0. }],
                          &self.main_display_rendering_params,
                          &(1.0, 1.0, 1.0));
        let bytes = rt.get_opaque_values_as_mask();

        (Some(dims), Some(RasterizedGlyph {
            width: width_u,
            height: height_u,
            bytes: bytes
        }))
    }

    pub fn get_glyph_dimensions(&self,
                                font_key: FontKey,
                                size: Au,
                                glyph: u32,
                                device_pixel_ratio: f32) -> Option<GlyphDimensions> {
        let (maybe_dims, _) =
            self.get_glyph_dimensions_and_maybe_rasterize(font_key, size, glyph, device_pixel_ratio, None);
        maybe_dims
    }

    pub fn rasterize_glyph(&mut self,
                           font_key: FontKey,
                           size: Au,
                           glyph: u32,
                           device_pixel_ratio: f32,
                           render_mode: FontRenderMode) -> Option<RasterizedGlyph> {
        let (_, maybe_glyph) =
            self.get_glyph_dimensions_and_maybe_rasterize(font_key, size, glyph, device_pixel_ratio, Some(render_mode));
        maybe_glyph
    }
}

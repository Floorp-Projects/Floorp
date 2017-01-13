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
                                                render_mode: Option<FontRenderMode>)
                                                -> (Option<GlyphDimensions>, Option<RasterizedGlyph>)
    {
        let font = self.fonts.get(&font_key).unwrap();
        let face = font.create_font_face();
        let glyph = glyph as u16;

        let glyph = glyph as u16;
        let advance = 0.0f32;
        let offset = dwrote::GlyphOffset { advanceOffset: 0.0, ascenderOffset: 0.0 };

        let glyph_run = dwrote::DWRITE_GLYPH_RUN {
            fontFace: unsafe { face.as_ptr() },
            fontEmSize: size.to_f32_px(), // size in DIPs (1/96", same as CSS pixels)
            glyphCount: 1,
            glyphIndices: &glyph,
            glyphAdvances: &advance,
            glyphOffsets: &offset,
            isSideways: 0,
            bidiLevel: 0,
        };

        // dwrite requires DWRITE_RENDERING_MODE_ALIASED if the texture
        // type is DWRITE_TEXTURE_ALIASED_1x1.  If CLEARTYPE_3x1,
        // then the other modes can be used.

        // TODO(vlad): get_glyph_dimensions needs to take the render mode into account
        // but the API doesn't give it to us right now.  Just assume subpixel.
        let (r_mode, m_mode, tex_type) = match render_mode {
            Some(FontRenderMode::Mono) => (dwrote::DWRITE_RENDERING_MODE_ALIASED,
                                           dwrote::DWRITE_MEASURING_MODE_GDI_NATURAL,
                                           dwrote::DWRITE_TEXTURE_ALIASED_1x1),
            Some(FontRenderMode::Alpha) => (dwrote::DWRITE_RENDERING_MODE_GDI_NATURAL,
                                            dwrote::DWRITE_MEASURING_MODE_GDI_NATURAL,
                                            dwrote::DWRITE_TEXTURE_CLEARTYPE_3x1),
            Some(FontRenderMode::Subpixel) | None => (dwrote::DWRITE_RENDERING_MODE_CLEARTYPE_GDI_NATURAL,
                                                      dwrote::DWRITE_MEASURING_MODE_GDI_NATURAL,
                                                      dwrote::DWRITE_TEXTURE_CLEARTYPE_3x1),
        };

        // XX use the xform to handle subpixel positioning (what skia does), I believe that keeps
        //let xform = dwrote::DWRITE_MATRIX { m11: 1.0, m12: 0.0, m21: 0.0, m22: 1.0, dx: 0.0, dy: 0.0 };
        let analysis = dwrote::GlyphRunAnalysis::create(&glyph_run, 1.0, None, r_mode, m_mode, 0.0, 0.0);
        let bounds = analysis.get_alpha_texture_bounds(tex_type);

        let width = (bounds.right - bounds.left) as u32;
        let height = (bounds.bottom - bounds.top) as u32;
        let dims = GlyphDimensions {
            left: bounds.left,
            top: -bounds.top,
            width: width,
            height: height,
        };

        // if empty, then nothing
        if dims.width == 0 || dims.height == 0 {
            return (None, None);
        }

        // if we weren't asked to rasterize, we're done
        if render_mode.is_none() {
            return (Some(dims), None);
        }

        let pixels = analysis.create_alpha_texture(tex_type, bounds);
        let rgba_pixels = match render_mode.unwrap() {
            FontRenderMode::Mono => {
                let mut rgba_pixels = vec![0; pixels.len() * 4];
                for i in 0..pixels.len() {
                    rgba_pixels[i*4+0] = 0xff;
                    rgba_pixels[i*4+1] = 0xff;
                    rgba_pixels[i*4+2] = 0xff;
                    rgba_pixels[i*4+3] = pixels[i];
                }
                rgba_pixels
            }
            FontRenderMode::Alpha => {
                let mut rgba_pixels = vec![0; pixels.len()/3 * 4];
                for i in 0..pixels.len()/3 {
                    // TODO(vlad): we likely need to do something smarter
                    let alpha = (pixels[i*3+0] as u32 + pixels[i*3+0] as u32 + pixels[i*3+0] as u32) / 3;
                    rgba_pixels[i*4+0] = 0xff;
                    rgba_pixels[i*4+1] = 0xff;
                    rgba_pixels[i*4+2] = 0xff;
                    rgba_pixels[i*4+3] = alpha as u8;
                }
                rgba_pixels
            }
            FontRenderMode::Subpixel => {
                let mut rgba_pixels = vec![0; pixels.len()/3 * 4];
                for i in 0..pixels.len()/3 {
                    rgba_pixels[i*4+0] = pixels[i*3+0];
                    rgba_pixels[i*4+1] = pixels[i*3+1];
                    rgba_pixels[i*4+2] = pixels[i*3+2];
                    rgba_pixels[i*4+3] = 0xff;
                }
                rgba_pixels
            }
        };

        (Some(dims), Some(RasterizedGlyph {
            width: dims.width,
            height: dims.height,
            bytes: rgba_pixels,
        }))
    }

    pub fn get_glyph_dimensions(&self,
                                font_key: FontKey,
                                size: Au,
                                glyph: u32) -> Option<GlyphDimensions> {
        let (maybe_dims, _) =
            self.get_glyph_dimensions_and_maybe_rasterize(font_key, size, glyph, None);
        maybe_dims
    }

    pub fn rasterize_glyph(&mut self,
                           font_key: FontKey,
                           size: Au,
                           glyph: u32,
                           render_mode: FontRenderMode) -> Option<RasterizedGlyph> {
        let (_, maybe_glyph) =
            self.get_glyph_dimensions_and_maybe_rasterize(font_key, size, glyph, Some(render_mode));
        maybe_glyph
    }
}

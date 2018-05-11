/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{FontInstanceFlags, FontKey, FontRenderMode};
use api::{ColorU, GlyphDimensions, GlyphKey, SubpixelDirection};
use dwrote;
use gamma_lut::ColorLut;
use glyph_rasterizer::{FontInstance, FontTransform};
use internal_types::{FastHashMap, ResourceCacheError};
use std::collections::hash_map::Entry;
use std::sync::Arc;
cfg_if! {
    if #[cfg(feature = "pathfinder")] {
        use pathfinder_font_renderer::{PathfinderComPtr, IDWriteFontFace};
        use glyph_rasterizer::NativeFontHandleWrapper;
    } else if #[cfg(not(feature = "pathfinder"))] {
        use glyph_rasterizer::{GlyphFormat, GlyphRasterResult, RasterizedGlyph};
        use gamma_lut::GammaLut;
    }
}

lazy_static! {
    static ref DEFAULT_FONT_DESCRIPTOR: dwrote::FontDescriptor = dwrote::FontDescriptor {
        family_name: "Arial".to_owned(),
        weight: dwrote::FontWeight::Regular,
        stretch: dwrote::FontStretch::Normal,
        style: dwrote::FontStyle::Normal,
    };
}

pub struct FontContext {
    fonts: FastHashMap<FontKey, dwrote::FontFace>,
    simulations: FastHashMap<(FontKey, dwrote::DWRITE_FONT_SIMULATIONS), dwrote::FontFace>,
    #[cfg(not(feature = "pathfinder"))]
    gamma_lut: GammaLut,
    #[cfg(not(feature = "pathfinder"))]
    gdi_gamma_lut: GammaLut,
}

// DirectWrite is safe to use on multiple threads and non-shareable resources are
// all hidden inside their font context.
unsafe impl Send for FontContext {}

fn dwrite_texture_type(render_mode: FontRenderMode) -> dwrote::DWRITE_TEXTURE_TYPE {
    match render_mode {
        FontRenderMode::Mono => dwrote::DWRITE_TEXTURE_ALIASED_1x1,
        FontRenderMode::Alpha |
        FontRenderMode::Subpixel => dwrote::DWRITE_TEXTURE_CLEARTYPE_3x1,
    }
}

fn dwrite_measure_mode(
    font: &FontInstance,
    bitmaps: bool,
) -> dwrote::DWRITE_MEASURING_MODE {
    if bitmaps || font.flags.contains(FontInstanceFlags::FORCE_GDI) {
        dwrote::DWRITE_MEASURING_MODE_GDI_CLASSIC
    } else {
      match font.render_mode {
          FontRenderMode::Mono => dwrote::DWRITE_MEASURING_MODE_GDI_CLASSIC,
          FontRenderMode::Alpha | FontRenderMode::Subpixel => dwrote::DWRITE_MEASURING_MODE_NATURAL,
      }
    }
}

fn dwrite_render_mode(
    font_face: &dwrote::FontFace,
    font: &FontInstance,
    em_size: f32,
    measure_mode: dwrote::DWRITE_MEASURING_MODE,
    bitmaps: bool,
) -> dwrote::DWRITE_RENDERING_MODE {
    let dwrite_render_mode = match font.render_mode {
        FontRenderMode::Mono => dwrote::DWRITE_RENDERING_MODE_ALIASED,
        FontRenderMode::Alpha | FontRenderMode::Subpixel => {
            if bitmaps || font.flags.contains(FontInstanceFlags::FORCE_GDI) {
                dwrote::DWRITE_RENDERING_MODE_GDI_CLASSIC
            } else {
                font_face.get_recommended_rendering_mode_default_params(em_size, 1.0, measure_mode)
            }
        }
    };

    if dwrite_render_mode == dwrote::DWRITE_RENDERING_MODE_OUTLINE {
        // Outline mode is not supported
        return dwrote::DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC;
    }

    dwrite_render_mode
}

fn is_bitmap_font(font: &FontInstance) -> bool {
    // If bitmaps are requested, then treat as a bitmap font to disable transforms.
    // If mono AA is requested, let that take priority over using bitmaps.
    font.render_mode != FontRenderMode::Mono &&
        font.flags.contains(FontInstanceFlags::EMBEDDED_BITMAPS)
}

// Skew factor matching Gecko/DWrite.
const OBLIQUE_SKEW_FACTOR: f32 = 0.3;

impl FontContext {
    pub fn new() -> Result<FontContext, ResourceCacheError> {
        // These are the default values we use in Gecko.
        // We use a gamma value of 2.3 for gdi fonts
        // TODO: Fetch this data from Gecko itself.
        cfg_if! {
            if #[cfg(not(feature = "pathfinder"))] {
                const CONTRAST: f32 = 1.0;
                const GAMMA: f32 = 1.8;
                const GDI_GAMMA: f32 = 2.3;
            }
        }

        Ok(FontContext {
            fonts: FastHashMap::default(),
            simulations: FastHashMap::default(),
            #[cfg(not(feature = "pathfinder"))]
            gamma_lut: GammaLut::new(CONTRAST, GAMMA, GAMMA),
            #[cfg(not(feature = "pathfinder"))]
            gdi_gamma_lut: GammaLut::new(CONTRAST, GDI_GAMMA, GDI_GAMMA),
        })
    }

    pub fn has_font(&self, font_key: &FontKey) -> bool {
        self.fonts.contains_key(font_key)
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, data: Arc<Vec<u8>>, index: u32) {
        if self.fonts.contains_key(font_key) {
            return;
        }

        if let Some(font_file) = dwrote::FontFile::new_from_data(&**data) {
            let face = font_file.create_face(index, dwrote::DWRITE_FONT_SIMULATIONS_NONE);
            self.fonts.insert(*font_key, face);
        } else {
            // XXX add_raw_font needs to have a way to return an error
            debug!("DWrite WR failed to load font from data, using Arial instead");
            self.add_native_font(font_key, DEFAULT_FONT_DESCRIPTOR.clone());
        }
    }

    pub fn add_native_font(&mut self, font_key: &FontKey, font_handle: dwrote::FontDescriptor) {
        if self.fonts.contains_key(font_key) {
            return;
        }

        let system_fc = dwrote::FontCollection::system();
        let font = match system_fc.get_font_from_descriptor(&font_handle) {
            Some(font) => font,
            None => { panic!("missing descriptor {:?}", font_handle) }
        };
        let face = font.create_font_face();
        self.fonts.insert(*font_key, face);
    }

    pub fn delete_font(&mut self, font_key: &FontKey) {
        if let Some(_) = self.fonts.remove(font_key) {
            self.simulations.retain(|k, _| k.0 != *font_key);
        }
    }

    // Assumes RGB format from dwrite, which is 3 bytes per pixel as dwrite
    // doesn't output an alpha value via GlyphRunAnalysis::CreateAlphaTexture
    #[allow(dead_code)]
    fn print_glyph_data(&self, data: &[u8], width: usize, height: usize) {
        // Rust doesn't have step_by support on stable :(
        for i in 0 .. height {
            let current_height = i * width * 3;

            for pixel in data[current_height .. current_height + (width * 3)].chunks(3) {
                let r = pixel[0];
                let g = pixel[1];
                let b = pixel[2];
                print!("({}, {}, {}) ", r, g, b,);
            }
            println!("");
        }
    }

    fn get_font_face(
        &mut self,
        font: &FontInstance,
    ) -> &dwrote::FontFace {
        if !font.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) {
            return self.fonts.get(&font.font_key).unwrap();
        }
        let sims = dwrote::DWRITE_FONT_SIMULATIONS_BOLD;
        match self.simulations.entry((font.font_key, sims)) {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => {
                let normal_face = self.fonts.get(&font.font_key).unwrap();
                entry.insert(normal_face.create_font_face_with_simulations(sims))
            }
        }
    }

    fn create_glyph_analysis(
        &mut self,
        font: &FontInstance,
        key: &GlyphKey,
        size: f32,
        transform: Option<dwrote::DWRITE_MATRIX>,
        bitmaps: bool,
    ) -> dwrote::GlyphRunAnalysis {
        let face = self.get_font_face(font);
        let glyph = key.index as u16;
        let advance = 0.0f32;
        let offset = dwrote::GlyphOffset {
            advanceOffset: 0.0,
            ascenderOffset: 0.0,
        };

        let glyph_run = dwrote::DWRITE_GLYPH_RUN {
            fontFace: unsafe { face.as_ptr() },
            fontEmSize: size, // size in DIPs (1/96", same as CSS pixels)
            glyphCount: 1,
            glyphIndices: &glyph,
            glyphAdvances: &advance,
            glyphOffsets: &offset,
            isSideways: 0,
            bidiLevel: 0,
        };

        let dwrite_measure_mode = dwrite_measure_mode(font, bitmaps);
        let dwrite_render_mode = dwrite_render_mode(
            face,
            font,
            size,
            dwrite_measure_mode,
            bitmaps,
        );

        dwrote::GlyphRunAnalysis::create(
            &glyph_run,
            1.0,
            transform,
            dwrite_render_mode,
            dwrite_measure_mode,
            0.0,
            0.0,
        )
    }

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        let face = self.fonts.get(&font_key).unwrap();
        let indices = face.get_glyph_indices(&[ch as u32]);
        indices.first().map(|idx| *idx as u32)
    }

    pub fn get_glyph_dimensions(
        &mut self,
        font: &FontInstance,
        key: &GlyphKey,
    ) -> Option<GlyphDimensions> {
        let size = font.size.to_f32_px();
        let bitmaps = is_bitmap_font(font);
        let transform = if font.flags.intersects(FontInstanceFlags::SYNTHETIC_ITALICS |
                                                 FontInstanceFlags::TRANSPOSE |
                                                 FontInstanceFlags::FLIP_X |
                                                 FontInstanceFlags::FLIP_Y) {
            let mut shape = FontTransform::identity();
            if font.flags.contains(FontInstanceFlags::FLIP_X) {
                shape = shape.flip_x();
            }
            if font.flags.contains(FontInstanceFlags::FLIP_Y) {
                shape = shape.flip_y();
            }
            if font.flags.contains(FontInstanceFlags::TRANSPOSE) {
                shape = shape.swap_xy();
            }
            if font.flags.contains(FontInstanceFlags::SYNTHETIC_ITALICS) {
                shape = shape.synthesize_italics(OBLIQUE_SKEW_FACTOR);
            }
            Some(dwrote::DWRITE_MATRIX {
                m11: shape.scale_x,
                m12: shape.skew_y,
                m21: shape.skew_x,
                m22: shape.scale_y,
                dx: 0.0,
                dy: 0.0,
            })
        } else {
            None
        };
        let analysis = self.create_glyph_analysis(font, key, size, transform, bitmaps);

        let texture_type = dwrite_texture_type(font.render_mode);

        let bounds = analysis.get_alpha_texture_bounds(texture_type);

        let width = (bounds.right - bounds.left) as u32;
        let height = (bounds.bottom - bounds.top) as u32;

        // Alpha texture bounds can sometimes return an empty rect
        // Such as for spaces
        if width == 0 || height == 0 {
            return None;
        }

        let face = self.get_font_face(font);
        face.get_design_glyph_metrics(&[key.index as u16], false)
            .first()
            .map(|metrics| {
                let em_size = size / 16.;
                let design_units_per_pixel = face.metrics().designUnitsPerEm as f32 / 16. as f32;
                let scaled_design_units_to_pixels = em_size / design_units_per_pixel;
                let advance = metrics.advanceWidth as f32 * scaled_design_units_to_pixels;

                GlyphDimensions {
                    left: bounds.left,
                    top: -bounds.top,
                    width,
                    height,
                    advance: advance,
                }
            })
    }

    // DWrite ClearType gives us values in RGB, but WR expects BGRA.
    #[cfg(not(feature = "pathfinder"))]
    fn convert_to_bgra(
        &self,
        pixels: &[u8],
        render_mode: FontRenderMode,
        bitmaps: bool,
    ) -> Vec<u8> {
        match (render_mode, bitmaps) {
            (FontRenderMode::Mono, _) => {
                let mut bgra_pixels: Vec<u8> = vec![0; pixels.len() * 4];
                for i in 0 .. pixels.len() {
                    let alpha = pixels[i];
                    bgra_pixels[i * 4 + 0] = alpha;
                    bgra_pixels[i * 4 + 1] = alpha;
                    bgra_pixels[i * 4 + 2] = alpha;
                    bgra_pixels[i * 4 + 3] = alpha;
                }
                bgra_pixels
            }
            (FontRenderMode::Alpha, _) | (_, true) => {
                let length = pixels.len() / 3;
                let mut bgra_pixels: Vec<u8> = vec![0; length * 4];
                for i in 0 .. length {
                    // Only take the G channel, as its closest to D2D
                    let alpha = pixels[i * 3 + 1] as u8;
                    bgra_pixels[i * 4 + 0] = alpha;
                    bgra_pixels[i * 4 + 1] = alpha;
                    bgra_pixels[i * 4 + 2] = alpha;
                    bgra_pixels[i * 4 + 3] = alpha;
                }
                bgra_pixels
            }
            (FontRenderMode::Subpixel, false) => {
                let length = pixels.len() / 3;
                let mut bgra_pixels: Vec<u8> = vec![0; length * 4];
                for i in 0 .. length {
                    bgra_pixels[i * 4 + 0] = pixels[i * 3 + 2];
                    bgra_pixels[i * 4 + 1] = pixels[i * 3 + 1];
                    bgra_pixels[i * 4 + 2] = pixels[i * 3 + 0];
                    bgra_pixels[i * 4 + 3] = 0xff;
                }
                bgra_pixels
            }
        }
    }

    pub fn prepare_font(font: &mut FontInstance) {
        match font.render_mode {
            FontRenderMode::Mono => {
                // In mono mode the color of the font is irrelevant.
                font.color = ColorU::new(255, 255, 255, 255);
                // Subpixel positioning is disabled in mono mode.
                font.subpx_dir = SubpixelDirection::None;
            }
            FontRenderMode::Alpha => {
                font.color = font.color.luminance_color().quantize();
            }
            FontRenderMode::Subpixel => {
                font.color = font.color.quantize();
            }
        }
    }

    #[cfg(not(feature = "pathfinder"))]
    pub fn rasterize_glyph(&mut self, font: &FontInstance, key: &GlyphKey) -> GlyphRasterResult {
        let (.., y_scale) = font.transform.compute_scale().unwrap_or((1.0, 1.0));
        let size = (font.size.to_f64_px() * y_scale) as f32;
        let bitmaps = is_bitmap_font(font);
        let (mut shape, (x_offset, y_offset)) = if bitmaps {
            (FontTransform::identity(), (0.0, 0.0))
        } else {
            (font.transform.invert_scale(y_scale, y_scale), font.get_subpx_offset(key))
        };
        if font.flags.contains(FontInstanceFlags::FLIP_X) {
            shape = shape.flip_x();
        }
        if font.flags.contains(FontInstanceFlags::FLIP_Y) {
            shape = shape.flip_y();
        }
        if font.flags.contains(FontInstanceFlags::TRANSPOSE) {
            shape = shape.swap_xy();
        }
        if font.flags.contains(FontInstanceFlags::SYNTHETIC_ITALICS) {
            shape = shape.synthesize_italics(OBLIQUE_SKEW_FACTOR);
        }
        let transform = if !shape.is_identity() || (x_offset, y_offset) != (0.0, 0.0) {
            Some(dwrote::DWRITE_MATRIX {
                m11: shape.scale_x,
                m12: shape.skew_y,
                m21: shape.skew_x,
                m22: shape.scale_y,
                dx: x_offset as f32,
                dy: y_offset as f32,
            })
        } else {
            None
        };

        let analysis = self.create_glyph_analysis(font, key, size, transform, bitmaps);
        let texture_type = dwrite_texture_type(font.render_mode);

        let bounds = analysis.get_alpha_texture_bounds(texture_type);
        let width = (bounds.right - bounds.left) as u32;
        let height = (bounds.bottom - bounds.top) as u32;

        // Alpha texture bounds can sometimes return an empty rect
        // Such as for spaces
        if width == 0 || height == 0 {
            return GlyphRasterResult::LoadFailed;
        }

        let pixels = analysis.create_alpha_texture(texture_type, bounds);
        let mut bgra_pixels = self.convert_to_bgra(&pixels, font.render_mode, bitmaps);

        let lut_correction = match font.render_mode {
            FontRenderMode::Mono => &self.gdi_gamma_lut,
            FontRenderMode::Alpha | FontRenderMode::Subpixel => {
                if bitmaps || font.flags.contains(FontInstanceFlags::FORCE_GDI) {
                    &self.gdi_gamma_lut
                } else {
                    &self.gamma_lut
                }
            }
        };
        lut_correction.preblend(&mut bgra_pixels, font.color);

        GlyphRasterResult::Bitmap(RasterizedGlyph {
            left: bounds.left as f32,
            top: -bounds.top as f32,
            width,
            height,
            scale: if bitmaps { y_scale.recip() as f32 } else { 1.0 },
            format: if bitmaps { GlyphFormat::Bitmap } else { font.get_glyph_format() },
            bytes: bgra_pixels,
        })
    }
}

#[cfg(feature = "pathfinder")]
impl<'a> From<NativeFontHandleWrapper<'a>> for PathfinderComPtr<IDWriteFontFace> {
    fn from(font_handle: NativeFontHandleWrapper<'a>) -> Self {
        let system_fc = ::dwrote::FontCollection::system();
        let font = match system_fc.get_font_from_descriptor(&font_handle.0) {
            Some(font) => font,
            None => panic!("missing descriptor {:?}", font_handle.0),
        };
        let face = font.create_font_face();
        unsafe { PathfinderComPtr::new(face.as_ptr()) }
    }
}
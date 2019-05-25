/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{FontInstanceFlags, FontKey, FontRenderMode, FontVariation};
use api::{ColorU, GlyphDimensions, NativeFontHandle};
use dwrote;
use crate::gamma_lut::ColorLut;
use crate::glyph_rasterizer::{FontInstance, FontTransform, GlyphKey};
use crate::internal_types::{FastHashMap, FastHashSet, ResourceCacheError};
use std::borrow::Borrow;
use std::collections::hash_map::Entry;
use std::hash::{Hash, Hasher};
use std::path::Path;
use std::sync::{Arc, Mutex};

cfg_if! {
    if #[cfg(feature = "pathfinder")] {
        use pathfinder_font_renderer::{PathfinderComPtr, IDWriteFontFace};
        use crate::glyph_rasterizer::NativeFontHandleWrapper;
    } else if #[cfg(not(feature = "pathfinder"))] {
        use api::FontInstancePlatformOptions;
        use crate::glyph_rasterizer::{GlyphFormat, GlyphRasterError, GlyphRasterResult, RasterizedGlyph};
        use crate::gamma_lut::GammaLut;
        use std::mem;
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

type CachedFontKey = Arc<Path>;

// A cached dwrote font file that is shared among all faces.
// Each face holds a CachedFontKey to keep track of how many users of the font there are.
struct CachedFont {
    key: CachedFontKey,
    file: dwrote::FontFile,
}

impl PartialEq for CachedFont {
    fn eq(&self, other: &CachedFont) -> bool {
        self.key == other.key
    }
}
impl Eq for CachedFont {}

impl Hash for CachedFont {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.key.hash(state);
    }
}

impl Borrow<Path> for CachedFont {
    fn borrow(&self) -> &Path {
        &*self.key
    }
}

lazy_static! {
    // This is effectively a weak map of dwrote FontFiles. CachedFonts are entered into the
    // cache when there are any FontFaces using them. CachedFonts are removed from the cache
    // when there are no more FontFaces using them at all.
    static ref FONT_CACHE: Mutex<FastHashSet<CachedFont>> = Mutex::new(FastHashSet::default());
}

struct FontFace {
    cached: Option<CachedFontKey>,
    file: dwrote::FontFile,
    index: u32,
    face: dwrote::FontFace,
}

pub struct FontContext {
    fonts: FastHashMap<FontKey, FontFace>,
    variations: FastHashMap<(FontKey, dwrote::DWRITE_FONT_SIMULATIONS, Vec<FontVariation>), dwrote::FontFace>,
    #[cfg(not(feature = "pathfinder"))]
    gamma_luts: FastHashMap<(u16, u16), GammaLut>,
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

impl FontContext {
    pub fn new() -> Result<FontContext, ResourceCacheError> {
        Ok(FontContext {
            fonts: FastHashMap::default(),
            variations: FastHashMap::default(),
            #[cfg(not(feature = "pathfinder"))]
            gamma_luts: FastHashMap::default(),
        })
    }

    pub fn has_font(&self, font_key: &FontKey) -> bool {
        self.fonts.contains_key(font_key)
    }

    fn add_font_descriptor(&mut self, font_key: &FontKey, desc: &dwrote::FontDescriptor) {
        let system_fc = dwrote::FontCollection::get_system(false);
        if let Some(font) = system_fc.get_font_from_descriptor(desc) {
            let face = font.create_font_face();
            let file = face.get_files().pop().unwrap();
            let index = face.get_index();
            self.fonts.insert(*font_key, FontFace { cached: None, file, index, face });
        }
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, data: Arc<Vec<u8>>, index: u32) {
        if self.fonts.contains_key(font_key) {
            return;
        }

        if let Some(file) = dwrote::FontFile::new_from_data(data) {
            if let Ok(face) = file.create_face(index, dwrote::DWRITE_FONT_SIMULATIONS_NONE) {
                self.fonts.insert(*font_key, FontFace { cached: None, file, index, face });
                return;
            }
        }
        // XXX add_raw_font needs to have a way to return an error
        debug!("DWrite WR failed to load font from data, using Arial instead");
        self.add_font_descriptor(font_key, &DEFAULT_FONT_DESCRIPTOR);
    }

    pub fn add_native_font(&mut self, font_key: &FontKey, font_handle: NativeFontHandle) {
        if self.fonts.contains_key(font_key) {
            return;
        }

        let index = font_handle.index;
        let mut cache = FONT_CACHE.lock().unwrap();
        // Check to see if the font is already in the cache. If so, reuse it.
        if let Some(font) = cache.get(font_handle.path.as_path()) {
            if let Ok(face) = font.file.create_face(index, dwrote::DWRITE_FONT_SIMULATIONS_NONE) {
                self.fonts.insert(
                    *font_key,
                    FontFace { cached: Some(font.key.clone()), file: font.file.clone(), index, face },
                );
                return;
            }
        }
        if let Some(file) = dwrote::FontFile::new_from_path(&font_handle.path) {
            // The font is not in the cache yet, so try to create the font and insert it in the cache.
            if let Ok(face) = file.create_face(index, dwrote::DWRITE_FONT_SIMULATIONS_NONE) {
                let key: CachedFontKey = font_handle.path.into();
                self.fonts.insert(
                    *font_key,
                    FontFace { cached: Some(key.clone()), file: file.clone(), index, face },
                );
                cache.insert(CachedFont { key, file });
                return;
            }
        }

        // XXX add_native_font needs to have a way to return an error
        debug!("DWrite WR failed to load font from path, using Arial instead");
        self.add_font_descriptor(font_key, &DEFAULT_FONT_DESCRIPTOR);
    }

    pub fn delete_font(&mut self, font_key: &FontKey) {
        if let Some(face) = self.fonts.remove(font_key) {
            self.variations.retain(|k, _| k.0 != *font_key);
            // Check if this was a cached font.
            if let Some(key) = face.cached {
                let mut cache = FONT_CACHE.lock().unwrap();
                // If there are only two references left, that means only this face and
                // the cache are using the font. So remove it from the cache.
                if Arc::strong_count(&key) == 2 {
                    cache.remove(&*key);
                }
            }
        }
    }

    pub fn delete_font_instance(&mut self, instance: &FontInstance) {
        // Ensure we don't keep around excessive amounts of stale variations.
        if !instance.variations.is_empty() {
            let sims = if instance.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) {
                dwrote::DWRITE_FONT_SIMULATIONS_BOLD
            } else {
                dwrote::DWRITE_FONT_SIMULATIONS_NONE
            };
            self.variations.remove(&(instance.font_key, sims, instance.variations.clone()));
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
        if !font.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) &&
           font.variations.is_empty() {
            return &self.fonts.get(&font.font_key).unwrap().face;
        }
        let sims = if font.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) {
            dwrote::DWRITE_FONT_SIMULATIONS_BOLD
        } else {
            dwrote::DWRITE_FONT_SIMULATIONS_NONE
        };
        match self.variations.entry((font.font_key, sims, font.variations.clone())) {
            Entry::Occupied(entry) => entry.into_mut(),
            Entry::Vacant(entry) => {
                let normal_face = self.fonts.get(&font.font_key).unwrap();
                if !font.variations.is_empty() {
                    if let Some(var_face) = normal_face.face.create_font_face_with_variations(
                        sims,
                        &font.variations.iter().map(|var| {
                            dwrote::DWRITE_FONT_AXIS_VALUE {
                                // OpenType tags are big-endian, but DWrite wants little-endian.
                                axisTag: var.tag.swap_bytes(),
                                value: var.value,
                            }
                        }).collect::<Vec<_>>(),
                    ) {
                        return entry.insert(var_face);
                    }
                }
                let var_face = normal_face.file
                    .create_face(normal_face.index, sims)
                    .unwrap_or_else(|_| normal_face.face.clone());
                entry.insert(var_face)
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
    ) -> Result<(dwrote::GlyphRunAnalysis, dwrote::DWRITE_TEXTURE_TYPE, dwrote::RECT), dwrote::HRESULT> {
        let face = self.get_font_face(font);
        let glyph = key.index() as u16;
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

        let analysis = dwrote::GlyphRunAnalysis::create(
            &glyph_run,
            1.0,
            transform,
            dwrite_render_mode,
            dwrite_measure_mode,
            0.0,
            0.0,
        )?;
        let texture_type = dwrite_texture_type(font.render_mode);
        let bounds = analysis.get_alpha_texture_bounds(texture_type)?;
        // If the bounds are empty, then we might not be able to render the glyph with cleartype.
        // Try again with aliased rendering to check if that works instead.
        if font.render_mode != FontRenderMode::Mono &&
           (bounds.left == bounds.right || bounds.top == bounds.bottom) {
            let analysis2 = dwrote::GlyphRunAnalysis::create(
                &glyph_run,
                1.0,
                transform,
                dwrote::DWRITE_RENDERING_MODE_ALIASED,
                dwrite_measure_mode,
                0.0,
                0.0,
            )?;
            let bounds2 = analysis2.get_alpha_texture_bounds(dwrote::DWRITE_TEXTURE_ALIASED_1x1)?;
            if bounds2.left != bounds2.right && bounds2.top != bounds2.bottom {
                return Ok((analysis2, dwrote::DWRITE_TEXTURE_ALIASED_1x1, bounds2));
            }
        }
        Ok((analysis, texture_type, bounds))
    }

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        let face = &self.fonts.get(&font_key).unwrap().face;
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
        let transform = if font.synthetic_italics.is_enabled() ||
                           font.flags.intersects(FontInstanceFlags::TRANSPOSE |
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
            if font.synthetic_italics.is_enabled() {
                shape = shape.synthesize_italics(font.synthetic_italics);
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
        let (_, _, bounds) = self.create_glyph_analysis(font, key, size, transform, bitmaps).ok()?;

        let width = (bounds.right - bounds.left) as i32;
        let height = (bounds.bottom - bounds.top) as i32;

        // Alpha texture bounds can sometimes return an empty rect
        // Such as for spaces
        if width == 0 || height == 0 {
            return None;
        }

        let face = self.get_font_face(font);
        face.get_design_glyph_metrics(&[key.index() as u16], false)
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
        texture_type: dwrote::DWRITE_TEXTURE_TYPE,
        render_mode: FontRenderMode,
        bitmaps: bool,
        subpixel_bgr: bool,
    ) -> Vec<u8> {
        match (texture_type, render_mode, bitmaps) {
            (dwrote::DWRITE_TEXTURE_ALIASED_1x1, _, _) => {
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
            (_, FontRenderMode::Subpixel, false) => {
                let length = pixels.len() / 3;
                let mut bgra_pixels: Vec<u8> = vec![0; length * 4];
                for i in 0 .. length {
                    let (mut r, g, mut b) = (pixels[i * 3 + 0], pixels[i * 3 + 1], pixels[i * 3 + 2]);
                    if subpixel_bgr {
                        mem::swap(&mut r, &mut b);
                    }
                    bgra_pixels[i * 4 + 0] = b;
                    bgra_pixels[i * 4 + 1] = g;
                    bgra_pixels[i * 4 + 2] = r;
                    bgra_pixels[i * 4 + 3] = 0xff;
                }
                bgra_pixels
            }
            _ => {
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
        }
    }

    pub fn prepare_font(font: &mut FontInstance) {
        match font.render_mode {
            FontRenderMode::Mono => {
                // In mono mode the color of the font is irrelevant.
                font.color = ColorU::new(255, 255, 255, 255);
                // Subpixel positioning is disabled in mono mode.
                font.disable_subpixel_position();
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
        let (x_scale, y_scale) = font.transform.compute_scale().unwrap_or((1.0, 1.0));
        let scale = font.oversized_scale_factor(x_scale, y_scale);
        let size = (font.size.to_f64_px() * y_scale / scale) as f32;
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
        if font.synthetic_italics.is_enabled() {
            shape = shape.synthesize_italics(font.synthetic_italics);
        }
        let transform = if !shape.is_identity() || (x_offset, y_offset) != (0.0, 0.0) {
            Some(dwrote::DWRITE_MATRIX {
                m11: shape.scale_x,
                m12: shape.skew_y,
                m21: shape.skew_x,
                m22: shape.scale_y,
                dx: (x_offset / scale) as f32,
                dy: (y_offset / scale) as f32,
            })
        } else {
            None
        };

        let (analysis, texture_type, bounds) = self.create_glyph_analysis(font, key, size, transform, bitmaps)
                                                   .or(Err(GlyphRasterError::LoadFailed))?;
        let width = (bounds.right - bounds.left) as i32;
        let height = (bounds.bottom - bounds.top) as i32;
        // Alpha texture bounds can sometimes return an empty rect
        // Such as for spaces
        if width == 0 || height == 0 {
            return Err(GlyphRasterError::LoadFailed);
        }

        let pixels = analysis.create_alpha_texture(texture_type, bounds).or(Err(GlyphRasterError::LoadFailed))?;
        let mut bgra_pixels = self.convert_to_bgra(&pixels, texture_type, font.render_mode, bitmaps,
                                                   font.flags.contains(FontInstanceFlags::SUBPIXEL_BGR));

        // These are the default values we use in Gecko.
        // We use a gamma value of 2.3 for gdi fonts
        const GDI_GAMMA: u16 = 230;

        let FontInstancePlatformOptions { gamma, contrast, .. } = font.platform_options.unwrap_or_default();
        let gdi_gamma = match font.render_mode {
            FontRenderMode::Mono => GDI_GAMMA,
            FontRenderMode::Alpha | FontRenderMode::Subpixel => {
                if bitmaps || font.flags.contains(FontInstanceFlags::FORCE_GDI) {
                    GDI_GAMMA
                } else {
                    gamma
                }
            }
        };
        let gamma_lut = self.gamma_luts
            .entry((gdi_gamma, contrast))
            .or_insert_with(||
                GammaLut::new(
                    contrast as f32 / 100.0,
                    gdi_gamma as f32 / 100.0,
                    gdi_gamma as f32 / 100.0,
                ));
        gamma_lut.preblend(&mut bgra_pixels, font.color);

        let format = if bitmaps {
            GlyphFormat::Bitmap
        } else if texture_type == dwrote::DWRITE_TEXTURE_ALIASED_1x1 {
            font.get_alpha_glyph_format()
        } else {
            font.get_glyph_format()
        };

        Ok(RasterizedGlyph {
            left: bounds.left as f32,
            top: -bounds.top as f32,
            width,
            height,
            scale: (if bitmaps { scale / y_scale } else { scale }) as f32,
            format,
            bytes: bgra_pixels,
        })
    }
}

#[cfg(feature = "pathfinder")]
impl<'a> From<NativeFontHandleWrapper<'a>> for PathfinderComPtr<IDWriteFontFace> {
    fn from(font_handle: NativeFontHandleWrapper<'a>) -> Self {
        if let Some(file) = dwrote::FontFile::new_from_path(&font_handle.0.path) {
            let index = font_handle.0.index;
            if let Ok(face) = file.create_face(index, dwrote::DWRITE_FONT_SIMULATIONS_NONE) {
                return unsafe { PathfinderComPtr::new(face.as_ptr()) };
            }
        }
        panic!("missing font {:?}", font_handle.0)
    }
}

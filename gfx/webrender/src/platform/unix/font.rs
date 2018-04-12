/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorU, GlyphDimensions, GlyphKey, FontKey, FontRenderMode};
use api::{FontInstancePlatformOptions, FontLCDFilter, FontHinting};
use api::{FontInstanceFlags, NativeFontHandle, SubpixelDirection};
use freetype::freetype::{FT_BBox, FT_Outline_Translate, FT_Pixel_Mode, FT_Render_Mode};
use freetype::freetype::{FT_Done_Face, FT_Error, FT_Get_Char_Index, FT_Int32};
use freetype::freetype::{FT_Done_FreeType, FT_Library_SetLcdFilter, FT_Pos};
use freetype::freetype::{FT_F26Dot6, FT_Face, FT_Glyph_Format, FT_Long, FT_UInt};
use freetype::freetype::{FT_GlyphSlot, FT_LcdFilter, FT_New_Face, FT_New_Memory_Face};
use freetype::freetype::{FT_Init_FreeType, FT_Load_Glyph, FT_Render_Glyph};
use freetype::freetype::{FT_Library, FT_Outline_Get_CBox, FT_Set_Char_Size, FT_Select_Size};
use freetype::freetype::{FT_Fixed, FT_Matrix, FT_Set_Transform};
use freetype::freetype::{FT_LOAD_COLOR, FT_LOAD_DEFAULT, FT_LOAD_FORCE_AUTOHINT};
use freetype::freetype::{FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH, FT_LOAD_NO_AUTOHINT};
use freetype::freetype::{FT_LOAD_NO_BITMAP, FT_LOAD_NO_HINTING, FT_LOAD_VERTICAL_LAYOUT};
use freetype::freetype::{FT_FACE_FLAG_SCALABLE, FT_FACE_FLAG_FIXED_SIZES};
use freetype::succeeded;
use glyph_rasterizer::{FontInstance, GlyphFormat, GlyphRasterResult, RasterizedGlyph};
#[cfg(feature = "pathfinder")]
use glyph_rasterizer::NativeFontHandleWrapper;
use internal_types::{FastHashMap, ResourceCacheError};
#[cfg(feature = "pathfinder")]
use pathfinder_font_renderer::freetype as pf_freetype;
use std::{cmp, mem, ptr, slice};
use std::cmp::max;
use std::ffi::CString;
use std::sync::Arc;

// These constants are not present in the freetype
// bindings due to bindgen not handling the way
// the macros are defined.
//const FT_LOAD_TARGET_NORMAL: FT_UInt = 0 << 16;
const FT_LOAD_TARGET_LIGHT: FT_UInt  = 1 << 16;
const FT_LOAD_TARGET_MONO: FT_UInt   = 2 << 16;
const FT_LOAD_TARGET_LCD: FT_UInt    = 3 << 16;
const FT_LOAD_TARGET_LCD_V: FT_UInt  = 4 << 16;

struct Face {
    face: FT_Face,
    // Raw byte data has to live until the font is deleted, according to
    // https://www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#FT_New_Memory_Face
    _bytes: Option<Arc<Vec<u8>>>,
}

pub struct FontContext {
    lib: FT_Library,
    faces: FastHashMap<FontKey, Face>,
    lcd_extra_pixels: i64,
}

// FreeType resources are safe to move between threads as long as they
// are not concurrently accessed. In our case, everything is hidden inside
// a given FontContext so it is safe to move the latter between threads.
unsafe impl Send for FontContext {}

extern "C" {
    fn FT_GlyphSlot_Embolden(slot: FT_GlyphSlot);
}

// Skew factor matching Gecko/FreeType.
const OBLIQUE_SKEW_FACTOR: f32 = 0.2;

fn get_skew_bounds(bottom: i32, top: i32) -> (f32, f32) {
    let skew_min = ((bottom as f32 + 0.5) * OBLIQUE_SKEW_FACTOR).floor();
    let skew_max = ((top as f32 - 0.5) * OBLIQUE_SKEW_FACTOR).ceil();
    (skew_min, skew_max)
}

fn skew_bitmap(bitmap: &[u8], width: usize, height: usize, left: i32, top: i32) -> (Vec<u8>, usize, i32) {
    let stride = width * 4;
    // Calculate the skewed horizontal offsets of the bottom and top of the glyph.
    let (skew_min, skew_max) = get_skew_bounds(top - height as i32, top);
    // Allocate enough extra width for the min/max skew offsets.
    let skew_width = width + (skew_max - skew_min) as usize;
    let mut skew_buffer = vec![0u8; skew_width * height * 4];
    for y in 0 .. height {
        // Calculate a skew offset at the vertical center of the current row.
        let offset = (top as f32 - y as f32 - 0.5) * OBLIQUE_SKEW_FACTOR - skew_min;
        // Get a blend factor in 0..256 constant across all pixels in the row.
        let blend = (offset.fract() * 256.0) as u32;
        let src_row = y * stride;
        let dest_row = (y * skew_width + offset.floor() as usize) * 4;
        let mut prev_px = [0u32; 4];
        for (src, dest) in
            bitmap[src_row .. src_row + stride].chunks(4).zip(
                skew_buffer[dest_row .. dest_row + stride].chunks_mut(4)
            ) {
            let px = [src[0] as u32, src[1] as u32, src[2] as u32, src[3] as u32];
            // Blend current pixel with previous pixel based on blend factor.
            let next_px = [px[0] * blend, px[1] * blend, px[2] * blend, px[3] * blend];
            dest[0] = ((((px[0] << 8) - next_px[0]) + prev_px[0] + 128) >> 8) as u8;
            dest[1] = ((((px[1] << 8) - next_px[1]) + prev_px[1] + 128) >> 8) as u8;
            dest[2] = ((((px[2] << 8) - next_px[2]) + prev_px[2] + 128) >> 8) as u8;
            dest[3] = ((((px[3] << 8) - next_px[3]) + prev_px[3] + 128) >> 8) as u8;
            // Save the remainder for blending onto the next pixel.
            prev_px = next_px;
        }
        // If the skew misaligns the final pixel, write out the remainder.
        if blend > 0 {
            let dest = &mut skew_buffer[dest_row + stride .. dest_row + stride + 4];
            dest[0] = ((prev_px[0] + 128) >> 8) as u8;
            dest[1] = ((prev_px[1] + 128) >> 8) as u8;
            dest[2] = ((prev_px[2] + 128) >> 8) as u8;
            dest[3] = ((prev_px[3] + 128) >> 8) as u8;
        }
    }
    (skew_buffer, skew_width, left + skew_min as i32)
}

fn transpose_bitmap(bitmap: &[u8], width: usize, height: usize) -> Vec<u8> {
    let mut transposed = vec![0u8; width * height * 4];
    for (y, row) in bitmap.chunks(width * 4).enumerate() {
        let mut offset = y * 4;
        for src in row.chunks(4) {
            transposed[offset .. offset + 4].copy_from_slice(src);
            offset += height * 4;
        }
    }
    transposed
}

fn flip_bitmap_x(bitmap: &mut [u8], width: usize, height: usize) {
    assert!(bitmap.len() == width * height * 4);
    let pixels = unsafe { slice::from_raw_parts_mut(bitmap.as_mut_ptr() as *mut u32, width * height) };
    for row in pixels.chunks_mut(width) {
        row.reverse();
    }
}

fn flip_bitmap_y(bitmap: &mut [u8], width: usize, height: usize) {
    assert!(bitmap.len() == width * height * 4);
    let pixels = unsafe { slice::from_raw_parts_mut(bitmap.as_mut_ptr() as *mut u32, width * height) };
    for y in 0 .. height / 2 {
        let low_row = y * width;
        let high_row = (height - 1 - y) * width;
        for x in 0 .. width {
            pixels.swap(low_row + x, high_row + x);
        }
    }
}

impl FontContext {
    pub fn new() -> Result<FontContext, ResourceCacheError> {
        let mut lib: FT_Library = ptr::null_mut();

        // Using an LCD filter may add one full pixel to each side if support is built in.
        // As of FreeType 2.8.1, an LCD filter is always used regardless of settings
        // if support for the patent-encumbered LCD filter algorithms is not built in.
        // Thus, the only reasonable way to guess padding is to unconditonally add it if
        // subpixel AA is used.
        let lcd_extra_pixels = 1;

        let result = unsafe {
            FT_Init_FreeType(&mut lib)
        };

        if succeeded(result) {
            Ok(FontContext {
                lib,
                faces: FastHashMap::default(),
                lcd_extra_pixels,
            })
        } else {
            // TODO(gw): Provide detailed error values.
            Err(ResourceCacheError::new(
                format!("Failed to initialize FreeType - {}", result)
            ))
        }
    }

    pub fn has_font(&self, font_key: &FontKey) -> bool {
        self.faces.contains_key(font_key)
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, bytes: Arc<Vec<u8>>, index: u32) {
        if !self.faces.contains_key(&font_key) {
            let mut face: FT_Face = ptr::null_mut();
            let result = unsafe {
                FT_New_Memory_Face(
                    self.lib,
                    bytes.as_ptr(),
                    bytes.len() as FT_Long,
                    index as FT_Long,
                    &mut face,
                )
            };
            if succeeded(result) && !face.is_null() {
                self.faces.insert(
                    *font_key,
                    Face {
                        face,
                        _bytes: Some(bytes),
                    },
                );
            } else {
                warn!("WARN: webrender failed to load font");
                debug!("font={:?}", font_key);
            }
        }
    }

    pub fn add_native_font(&mut self, font_key: &FontKey, native_font_handle: NativeFontHandle) {
        if !self.faces.contains_key(&font_key) {
            let mut face: FT_Face = ptr::null_mut();
            let pathname = CString::new(native_font_handle.pathname).unwrap();
            let result = unsafe {
                FT_New_Face(
                    self.lib,
                    pathname.as_ptr(),
                    native_font_handle.index as FT_Long,
                    &mut face,
                )
            };
            if succeeded(result) && !face.is_null() {
                self.faces.insert(
                    *font_key,
                    Face {
                        face,
                        _bytes: None,
                    },
                );
            } else {
                warn!("WARN: webrender failed to load font");
                debug!("font={:?}, path={:?}", font_key, pathname);
            }
        }
    }

    pub fn delete_font(&mut self, font_key: &FontKey) {
        if let Some(face) = self.faces.remove(font_key) {
            let result = unsafe { FT_Done_Face(face.face) };
            assert!(succeeded(result));
        }
    }

    fn load_glyph(&self, font: &FontInstance, glyph: &GlyphKey) -> Option<FT_GlyphSlot> {
        debug_assert!(self.faces.contains_key(&font.font_key));
        let face = self.faces.get(&font.font_key).unwrap();

        let mut load_flags = FT_LOAD_DEFAULT;
        let FontInstancePlatformOptions { mut hinting, .. } = font.platform_options.unwrap_or_default();
        // Disable hinting if there is a non-axis-aligned transform.
        if font.flags.contains(FontInstanceFlags::SYNTHETIC_ITALICS) ||
           ((font.transform.scale_x != 0.0 || font.transform.scale_y != 0.0) &&
            (font.transform.skew_x != 0.0 || font.transform.skew_y != 0.0)) {
            hinting = FontHinting::None;
        }
        match (hinting, font.render_mode) {
            (FontHinting::None, _) => load_flags |= FT_LOAD_NO_HINTING,
            (FontHinting::Mono, _) => load_flags = FT_LOAD_TARGET_MONO,
            (FontHinting::Light, _) => load_flags = FT_LOAD_TARGET_LIGHT,
            (FontHinting::LCD, FontRenderMode::Subpixel) => {
                load_flags = match font.subpx_dir {
                    SubpixelDirection::Vertical => FT_LOAD_TARGET_LCD_V,
                    _ => FT_LOAD_TARGET_LCD,
                };
                if font.flags.contains(FontInstanceFlags::FORCE_AUTOHINT) {
                    load_flags |= FT_LOAD_FORCE_AUTOHINT;
                }
            }
            _ => {
                if font.flags.contains(FontInstanceFlags::FORCE_AUTOHINT) {
                    load_flags |= FT_LOAD_FORCE_AUTOHINT;
                }
            }
        }

        if font.flags.contains(FontInstanceFlags::NO_AUTOHINT) {
            load_flags |= FT_LOAD_NO_AUTOHINT;
        }
        if !font.flags.contains(FontInstanceFlags::EMBEDDED_BITMAPS) {
            load_flags |= FT_LOAD_NO_BITMAP;
        }
        if font.flags.contains(FontInstanceFlags::VERTICAL_LAYOUT) {
            load_flags |= FT_LOAD_VERTICAL_LAYOUT;
        }

        load_flags |= FT_LOAD_COLOR;
        load_flags |= FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH;

        let (x_scale, y_scale) = font.transform.compute_scale().unwrap_or((1.0, 1.0));
        let req_size = font.size.to_f64_px();
        let face_flags = unsafe { (*face.face).face_flags };
        let mut result = if (face_flags & (FT_FACE_FLAG_FIXED_SIZES as FT_Long)) != 0 &&
                            (face_flags & (FT_FACE_FLAG_SCALABLE as FT_Long)) == 0 &&
                            (load_flags & FT_LOAD_NO_BITMAP) == 0 {
            unsafe { FT_Set_Transform(face.face, ptr::null_mut(), ptr::null_mut()) };
            self.choose_bitmap_size(face.face, req_size * y_scale)
        } else {
            let mut shape = font.transform.invert_scale(x_scale, y_scale);
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
            };
            let mut ft_shape = FT_Matrix {
                xx: (shape.scale_x * 65536.0) as FT_Fixed,
                xy: (shape.skew_x * -65536.0) as FT_Fixed,
                yx: (shape.skew_y * -65536.0) as FT_Fixed,
                yy: (shape.scale_y * 65536.0) as FT_Fixed,
            };
            unsafe {
                FT_Set_Transform(face.face, &mut ft_shape, ptr::null_mut());
                FT_Set_Char_Size(
                    face.face,
                    (req_size * x_scale * 64.0 + 0.5) as FT_F26Dot6,
                    (req_size * y_scale * 64.0 + 0.5) as FT_F26Dot6,
                    0,
                    0,
                )
            }
        };

        if succeeded(result) {
            result = unsafe { FT_Load_Glyph(face.face, glyph.index as FT_UInt, load_flags as FT_Int32) };
        };

        if succeeded(result) {
            let slot = unsafe { (*face.face).glyph };
            assert!(slot != ptr::null_mut());

            if font.flags.contains(FontInstanceFlags::SYNTHETIC_BOLD) {
                unsafe { FT_GlyphSlot_Embolden(slot) };
            }

            let format = unsafe { (*slot).format };
            match format {
                FT_Glyph_Format::FT_GLYPH_FORMAT_OUTLINE |
                FT_Glyph_Format::FT_GLYPH_FORMAT_BITMAP => Some(slot),
                _ => {
                    error!("Unsupported format");
                    debug!("format={:?}", format);
                    None
                }
            }
        } else {
            error!("Unable to load glyph");
            debug!(
                "{} of size {:?} from font {:?}, {:?}",
                glyph.index,
                font.size,
                font.font_key,
                result
            );
            None
        }
    }

    // Get the bounding box for a glyph, accounting for sub-pixel positioning.
    fn get_bounding_box(
        &self,
        slot: FT_GlyphSlot,
        font: &FontInstance,
        glyph: &GlyphKey,
    ) -> FT_BBox {
        let mut cbox: FT_BBox = unsafe { mem::uninitialized() };

        // Get the estimated bounding box from FT (control points).
        unsafe {
            FT_Outline_Get_CBox(&(*slot).outline, &mut cbox);

            // For spaces and other non-printable characters, early out.
            if (*slot).outline.n_contours == 0 {
                return cbox;
            }
        }

        // Convert the subpixel offset to floats.
        let (dx, dy) = font.get_subpx_offset(glyph);

        // Apply extra pixel of padding for subpixel AA, due to the filter.
        let padding = match font.render_mode {
            FontRenderMode::Subpixel => (self.lcd_extra_pixels * 64) as FT_Pos,
            FontRenderMode::Alpha |
            FontRenderMode::Mono => 0 as FT_Pos,
        };

        // Offset the bounding box by subpixel positioning.
        // Convert to 26.6 fixed point format for FT.
        match font.subpx_dir {
            SubpixelDirection::None => {}
            SubpixelDirection::Horizontal => {
                let dx = (dx * 64.0 + 0.5) as FT_Long;
                cbox.xMin += dx - padding;
                cbox.xMax += dx + padding;
            }
            SubpixelDirection::Vertical => {
                let dy = (dy * 64.0 + 0.5) as FT_Long;
                cbox.yMin += dy - padding;
                cbox.yMax += dy + padding;
            }
        }

        // Outset the box to device pixel boundaries
        cbox.xMin &= !63;
        cbox.yMin &= !63;
        cbox.xMax = (cbox.xMax + 63) & !63;
        cbox.yMax = (cbox.yMax + 63) & !63;

        cbox
    }

    fn get_glyph_dimensions_impl(
        &self,
        slot: FT_GlyphSlot,
        font: &FontInstance,
        glyph: &GlyphKey,
        transform_bitmaps: bool,
    ) -> Option<GlyphDimensions> {
        let metrics = unsafe { &(*slot).metrics };

        let mut advance = metrics.horiAdvance as f32 / 64.0;
        match unsafe { (*slot).format } {
            FT_Glyph_Format::FT_GLYPH_FORMAT_BITMAP => {
                let mut left = unsafe { (*slot).bitmap_left };
                let mut top = unsafe { (*slot).bitmap_top };
                let mut width = unsafe { (*slot).bitmap.width };
                let mut height = unsafe { (*slot).bitmap.rows };
                if transform_bitmaps {
                    let y_size = unsafe { (*(*(*slot).face).size).metrics.y_ppem };
                    let scale = font.size.to_f32_px() / y_size as f32;
                    let x0 = left as f32 * scale;
                    let x1 = width as f32 * scale + x0;
                    let y1 = top as f32 * scale;
                    let y0 = y1 - height as f32 * scale;
                    left = x0.round() as i32;
                    top = y1.round() as i32;
                    width = (x1.ceil() - x0.floor()) as u32;
                    height = (y1.ceil() - y0.floor()) as u32;
                    advance *= scale;
                    if font.flags.contains(FontInstanceFlags::SYNTHETIC_ITALICS) {
                        let (skew_min, skew_max) = get_skew_bounds(top - height as i32, top);
                        left += skew_min as i32;
                        width += (skew_max - skew_min) as u32;
                    }
                    if font.flags.contains(FontInstanceFlags::TRANSPOSE) {
                        mem::swap(&mut width, &mut height);
                        mem::swap(&mut left, &mut top);
                        left -= width as i32;
                        top += height as i32;
                    }
                    if font.flags.contains(FontInstanceFlags::FLIP_X) {
                        left = -(left + width as i32);
                    }
                    if font.flags.contains(FontInstanceFlags::FLIP_Y) {
                        top = -(top - height as i32);
                    }
                }
                Some(GlyphDimensions {
                    left,
                    top,
                    width,
                    height,
                    advance,
                })
            }
            FT_Glyph_Format::FT_GLYPH_FORMAT_OUTLINE => {
                let cbox = self.get_bounding_box(slot, font, glyph);
                Some(GlyphDimensions {
                    left: (cbox.xMin >> 6) as i32,
                    top: (cbox.yMax >> 6) as i32,
                    width: ((cbox.xMax - cbox.xMin) >> 6) as u32,
                    height: ((cbox.yMax - cbox.yMin) >> 6) as u32,
                    advance,
                })
            }
            _ => None,
        }
    }

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        let face = self.faces.get(&font_key).expect("Unknown font key!");
        unsafe {
            let idx = FT_Get_Char_Index(face.face, ch as _);
            if idx != 0 {
                Some(idx)
            } else {
                None
            }
        }
    }

    pub fn get_glyph_dimensions(
        &mut self,
        font: &FontInstance,
        key: &GlyphKey,
    ) -> Option<GlyphDimensions> {
        let slot = self.load_glyph(font, key);
        slot.and_then(|slot| self.get_glyph_dimensions_impl(slot, font, key, true))
    }

    fn choose_bitmap_size(&self, face: FT_Face, requested_size: f64) -> FT_Error {
        let mut best_dist = unsafe { *(*face).available_sizes.offset(0) }.y_ppem as f64 / 64.0 - requested_size;
        let mut best_size = 0;
        let num_fixed_sizes = unsafe { (*face).num_fixed_sizes };
        for i in 1 .. num_fixed_sizes {
            // Distance is positive if strike is larger than desired size,
            // or negative if smaller. If previously a found smaller strike,
            // then prefer a larger strike. Otherwise, minimize distance.
            let dist = unsafe { *(*face).available_sizes.offset(i as isize) }.y_ppem as f64 / 64.0 - requested_size;
            if (best_dist < 0.0 && dist >= best_dist) || dist.abs() <= best_dist {
                best_dist = dist;
                best_size = i;
            }
        }
        unsafe { FT_Select_Size(face, best_size) }
    }

    pub fn prepare_font(font: &mut FontInstance) {
        match font.render_mode {
            FontRenderMode::Mono => {
                // In mono mode the color of the font is irrelevant.
                font.color = ColorU::new(0xFF, 0xFF, 0xFF, 0xFF);
                // Subpixel positioning is disabled in mono mode.
                font.subpx_dir = SubpixelDirection::None;
            }
            FontRenderMode::Alpha | FontRenderMode::Subpixel => {
                // We don't do any preblending with FreeType currently, so the color is not used.
                font.color = ColorU::new(0xFF, 0xFF, 0xFF, 0xFF);
            }
        }
    }

    fn rasterize_glyph_outline(
        &mut self,
        slot: FT_GlyphSlot,
        font: &FontInstance,
        key: &GlyphKey,
    ) -> bool {
        // Get the subpixel offsets in FT 26.6 format.
        let (dx, dy) = font.get_subpx_offset(key);
        let dx = (dx * 64.0 + 0.5) as FT_Long;
        let dy = (dy * 64.0 + 0.5) as FT_Long;

        // Move the outline curves to be at the origin, taking
        // into account the subpixel positioning.
        unsafe {
            let outline = &(*slot).outline;
            let mut cbox: FT_BBox = mem::uninitialized();
            FT_Outline_Get_CBox(outline, &mut cbox);
            FT_Outline_Translate(
                outline,
                dx - ((cbox.xMin + dx) & !63),
                dy - ((cbox.yMin + dy) & !63),
            );
        }

        if font.render_mode == FontRenderMode::Subpixel {
            let FontInstancePlatformOptions { lcd_filter, .. } = font.platform_options.unwrap_or_default();
            let filter = match lcd_filter {
                FontLCDFilter::None => FT_LcdFilter::FT_LCD_FILTER_NONE,
                FontLCDFilter::Default => FT_LcdFilter::FT_LCD_FILTER_DEFAULT,
                FontLCDFilter::Light => FT_LcdFilter::FT_LCD_FILTER_LIGHT,
                FontLCDFilter::Legacy => FT_LcdFilter::FT_LCD_FILTER_LEGACY,
            };
            unsafe { FT_Library_SetLcdFilter(self.lib, filter) };
        }
        let render_mode = match (font.render_mode, font.subpx_dir) {
            (FontRenderMode::Mono, _) => FT_Render_Mode::FT_RENDER_MODE_MONO,
            (FontRenderMode::Alpha, _) => FT_Render_Mode::FT_RENDER_MODE_NORMAL,
            (FontRenderMode::Subpixel, SubpixelDirection::Vertical) => FT_Render_Mode::FT_RENDER_MODE_LCD_V,
            (FontRenderMode::Subpixel, _) => FT_Render_Mode::FT_RENDER_MODE_LCD,
        };
        let result = unsafe { FT_Render_Glyph(slot, render_mode) };
        if !succeeded(result) {
            error!("Unable to rasterize");
            debug!(
                "{:?} with {:?}, {:?}",
                key,
                render_mode,
                result
            );
            false
        } else {
            true
        }
    }

    #[cfg(not(feature = "pathfinder"))]
    pub fn rasterize_glyph(&mut self, font: &FontInstance, key: &GlyphKey) -> GlyphRasterResult {
        let slot = match self.load_glyph(font, key) {
            Some(slot) => slot,
            None => return GlyphRasterResult::LoadFailed,
        };

        // Get dimensions of the glyph, to see if we need to rasterize it.
        let dimensions = match self.get_glyph_dimensions_impl(slot, font, key, false) {
            Some(val) => val,
            None => return GlyphRasterResult::LoadFailed,
        };
        let GlyphDimensions { mut left, mut top, width, height, .. } = dimensions;

        // For spaces and other non-printable characters, early out.
        if width == 0 || height == 0 {
            return GlyphRasterResult::LoadFailed;
        }

        let format = unsafe { (*slot).format };
        let mut scale = 1.0;
        match format {
            FT_Glyph_Format::FT_GLYPH_FORMAT_BITMAP => {
                let y_size = unsafe { (*(*(*slot).face).size).metrics.y_ppem };
                scale = font.size.to_f32_px() / y_size as f32;
            }
            FT_Glyph_Format::FT_GLYPH_FORMAT_OUTLINE => {
                if !self.rasterize_glyph_outline(slot, font, key) {
                    return GlyphRasterResult::LoadFailed;
                }
            }
            _ => {
                error!("Unsupported format");
                debug!("format={:?}", format);
                return GlyphRasterResult::LoadFailed;
            }
        };

        debug!(
            "Rasterizing {:?} as {:?} with dimensions {:?}",
            key,
            font.render_mode,
            dimensions
        );

        let bitmap = unsafe { &(*slot).bitmap };
        let pixel_mode = unsafe { mem::transmute(bitmap.pixel_mode as u32) };
        let (mut actual_width, mut actual_height) = match pixel_mode {
            FT_Pixel_Mode::FT_PIXEL_MODE_LCD => {
                assert!(bitmap.width % 3 == 0);
                ((bitmap.width / 3) as usize, bitmap.rows as usize)
            }
            FT_Pixel_Mode::FT_PIXEL_MODE_LCD_V => {
                assert!(bitmap.rows % 3 == 0);
                (bitmap.width as usize, (bitmap.rows / 3) as usize)
            }
            FT_Pixel_Mode::FT_PIXEL_MODE_MONO |
            FT_Pixel_Mode::FT_PIXEL_MODE_GRAY |
            FT_Pixel_Mode::FT_PIXEL_MODE_BGRA => {
                (bitmap.width as usize, bitmap.rows as usize)
            }
            _ => panic!("Unsupported mode"),
        };
        let mut final_buffer = vec![0u8; actual_width * actual_height * 4];

        // Extract the final glyph from FT format into BGRA8 format, which is
        // what WR expects.
        let subpixel_bgr = font.flags.contains(FontInstanceFlags::SUBPIXEL_BGR);
        let mut src_row = bitmap.buffer;
        let mut dest: usize = 0;
        while dest < final_buffer.len() {
            let mut src = src_row;
            let row_end = dest + actual_width * 4;
            match pixel_mode {
                FT_Pixel_Mode::FT_PIXEL_MODE_MONO => {
                    while dest < row_end {
                        // Cast the byte to signed so that we can left shift each bit into
                        // the top bit, then right shift to fill out the bits with 0s or 1s.
                        let mut byte: i8 = unsafe { *src as i8 };
                        src = unsafe { src.offset(1) };
                        let byte_end = cmp::min(row_end, dest + 8 * 4);
                        while dest < byte_end {
                            let alpha = (byte >> 7) as u8;
                            final_buffer[dest + 0] = alpha;
                            final_buffer[dest + 1] = alpha;
                            final_buffer[dest + 2] = alpha;
                            final_buffer[dest + 3] = alpha;
                            dest += 4;
                            byte <<= 1;
                        }
                    }
                }
                FT_Pixel_Mode::FT_PIXEL_MODE_GRAY => {
                    while dest < row_end {
                        let alpha = unsafe { *src };
                        final_buffer[dest + 0] = alpha;
                        final_buffer[dest + 1] = alpha;
                        final_buffer[dest + 2] = alpha;
                        final_buffer[dest + 3] = alpha;
                        src = unsafe { src.offset(1) };
                        dest += 4;
                    }
                }
                FT_Pixel_Mode::FT_PIXEL_MODE_LCD => {
                    while dest < row_end {
                        let (mut r, g, mut b) = unsafe { (*src, *src.offset(1), *src.offset(2)) };
                        if subpixel_bgr {
                            mem::swap(&mut r, &mut b);
                        }
                        final_buffer[dest + 0] = b;
                        final_buffer[dest + 1] = g;
                        final_buffer[dest + 2] = r;
                        final_buffer[dest + 3] = max(max(b, g), r);
                        src = unsafe { src.offset(3) };
                        dest += 4;
                    }
                }
                FT_Pixel_Mode::FT_PIXEL_MODE_LCD_V => {
                    while dest < row_end {
                        let (mut r, g, mut b) =
                            unsafe { (*src, *src.offset(bitmap.pitch as isize), *src.offset((2 * bitmap.pitch) as isize)) };
                        if subpixel_bgr {
                            mem::swap(&mut r, &mut b);
                        }
                        final_buffer[dest + 0] = b;
                        final_buffer[dest + 1] = g;
                        final_buffer[dest + 2] = r;
                        final_buffer[dest + 3] = max(max(b, g), r);
                        src = unsafe { src.offset(1) };
                        dest += 4;
                    }
                    src_row = unsafe { src_row.offset((2 * bitmap.pitch) as isize) };
                }
                FT_Pixel_Mode::FT_PIXEL_MODE_BGRA => {
                    // The source is premultiplied BGRA data.
                    let dest_slice = &mut final_buffer[dest .. row_end];
                    let src_slice = unsafe { slice::from_raw_parts(src, dest_slice.len()) };
                    dest_slice.copy_from_slice(src_slice);
                }
                _ => panic!("Unsupported mode"),
            }
            src_row = unsafe { src_row.offset(bitmap.pitch as isize) };
            dest = row_end;
        }

        match format {
            FT_Glyph_Format::FT_GLYPH_FORMAT_BITMAP => {
                if font.flags.contains(FontInstanceFlags::SYNTHETIC_ITALICS) {
                    let (skew_buffer, skew_width, skew_left) =
                        skew_bitmap(&final_buffer, actual_width, actual_height, left, top);
                    final_buffer = skew_buffer;
                    actual_width = skew_width;
                    left = skew_left;
                }
                if font.flags.contains(FontInstanceFlags::TRANSPOSE) {
                    final_buffer = transpose_bitmap(&final_buffer, actual_width, actual_height);
                    mem::swap(&mut actual_width, &mut actual_height);
                    mem::swap(&mut left, &mut top);
                    left -= actual_width as i32;
                    top += actual_height as i32;
                }
                if font.flags.contains(FontInstanceFlags::FLIP_X) {
                    flip_bitmap_x(&mut final_buffer, actual_width, actual_height);
                    left = -(left + actual_width as i32);
                }
                if font.flags.contains(FontInstanceFlags::FLIP_Y) {
                    flip_bitmap_y(&mut final_buffer, actual_width, actual_height);
                    top = -(top - actual_height as i32);
                }
            }
            FT_Glyph_Format::FT_GLYPH_FORMAT_OUTLINE => {
                unsafe {
                    left += (*slot).bitmap_left;
                    top += (*slot).bitmap_top - height as i32;
                }
            }
            _ => {}
        }

        let glyph_format = match (pixel_mode, format) {
            (FT_Pixel_Mode::FT_PIXEL_MODE_LCD, _) |
            (FT_Pixel_Mode::FT_PIXEL_MODE_LCD_V, _) => font.get_subpixel_glyph_format(),
            (FT_Pixel_Mode::FT_PIXEL_MODE_BGRA, _) => GlyphFormat::ColorBitmap,
            (_, FT_Glyph_Format::FT_GLYPH_FORMAT_BITMAP) => GlyphFormat::Bitmap,
            _ => font.get_alpha_glyph_format(),
        };

        GlyphRasterResult::Bitmap(RasterizedGlyph {
            left: left as f32,
            top: top as f32,
            width: actual_width as u32,
            height: actual_height as u32,
            scale,
            format: glyph_format,
            bytes: final_buffer,
        })
    }
}

impl Drop for FontContext {
    fn drop(&mut self) {
        unsafe {
            FT_Done_FreeType(self.lib);
        }
    }
}

#[cfg(feature = "pathfinder")]
impl<'a> Into<pf_freetype::FontDescriptor> for NativeFontHandleWrapper<'a> {
    fn into(self) -> pf_freetype::FontDescriptor {
        let NativeFontHandleWrapper(font_handle) = self;
        pf_freetype::FontDescriptor::new(font_handle.pathname.clone().into(), font_handle.index)
    }
}

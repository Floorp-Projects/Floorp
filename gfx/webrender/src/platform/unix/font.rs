/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use api::{FontKey, FontRenderMode, GlyphDimensions};
use api::{NativeFontHandle, GlyphOptions};
use api::{GlyphKey};

use freetype::freetype::{FT_Render_Mode, FT_Pixel_Mode};
use freetype::freetype::{FT_Done_FreeType, FT_Library_SetLcdFilter};
use freetype::freetype::{FT_Library, FT_Set_Char_Size};
use freetype::freetype::{FT_Face, FT_Long, FT_UInt, FT_F26Dot6};
use freetype::freetype::{FT_Init_FreeType, FT_Load_Glyph, FT_Render_Glyph};
use freetype::freetype::{FT_New_Memory_Face, FT_GlyphSlot, FT_LcdFilter};
use freetype::freetype::{FT_Done_Face, FT_Error, FT_Int32};

use std::{cmp, mem, ptr, slice};
use std::collections::HashMap;

// This constant is not present in the freetype
// bindings due to bindgen not handling the way
// the macro is defined.
const FT_LOAD_TARGET_LIGHT: FT_Int32 = 1 << 16;

// Default to slight hinting, which is what most
// Linux distros use by default, and is a better
// default than no hinting.
// TODO(gw): Make this configurable.
const GLYPH_LOAD_FLAGS: FT_Int32 = FT_LOAD_TARGET_LIGHT;

struct Face {
    face: FT_Face,
}

pub struct FontContext {
    lib: FT_Library,
    faces: HashMap<FontKey, Face>,
}

// FreeType resources are safe to move between threads as long as they
// are not concurrently accessed. In our case, everything is hidden inside
// a given FontContext so it is safe to move the latter between threads.
unsafe impl Send for FontContext {}

pub struct RasterizedGlyph {
    pub top: f32,
    pub left: f32,
    pub width: u32,
    pub height: u32,
    pub bytes: Vec<u8>,
}

const SUCCESS: FT_Error = FT_Error(0);

impl FontContext {
    pub fn new() -> FontContext {
        let mut lib: FT_Library = ptr::null_mut();
        unsafe {
            let result = FT_Init_FreeType(&mut lib);
            assert!(result.succeeded(), "Unable to initialize FreeType library {:?}", result);

            // TODO(gw): Check result of this to determine if freetype build supports subpixel.
            let result = FT_Library_SetLcdFilter(lib, FT_LcdFilter::FT_LCD_FILTER_DEFAULT);
            if !result.succeeded() {
                println!("WARN: Initializing a FreeType library build without subpixel AA enabled!");
            }
        }

        FontContext {
            lib,
            faces: HashMap::new(),
        }
    }

    pub fn has_font(&self, font_key: &FontKey) -> bool {
        self.faces.contains_key(font_key)
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, bytes: &[u8], index: u32) {
        if !self.faces.contains_key(&font_key) {
            let mut face: FT_Face = ptr::null_mut();
            let result = unsafe {
                FT_New_Memory_Face(self.lib,
                                   bytes.as_ptr(),
                                   bytes.len() as FT_Long,
                                   index as FT_Long,
                                   &mut face)
            };
            if result.succeeded() && !face.is_null() {
                self.faces.insert(*font_key, Face {
                    face,
                    //_bytes: bytes
                });
            } else {
                println!("WARN: webrender failed to load font {:?}", font_key);
            }
        }
    }

    pub fn add_native_font(&mut self, _font_key: &FontKey, _native_font_handle: NativeFontHandle) {
        panic!("TODO: Not supported on Linux");
    }

    pub fn delete_font(&mut self, font_key: &FontKey) {
        if let Some(face) = self.faces.remove(font_key) {
            let result = unsafe {
                FT_Done_Face(face.face)
            };
            assert!(result.succeeded());
        }
    }

    fn load_glyph(&self,
                  font_key: FontKey,
                  size: Au,
                  character: u32) -> Option<FT_GlyphSlot> {

        debug_assert!(self.faces.contains_key(&font_key));
        let face = self.faces.get(&font_key).unwrap();
        let char_size = size.to_f64_px() * 64.0 + 0.5;

        assert_eq!(SUCCESS, unsafe {
            FT_Set_Char_Size(face.face, char_size as FT_F26Dot6, 0, 0, 0)
        });

        let result = unsafe {
            FT_Load_Glyph(face.face,
                          character as FT_UInt,
                          GLYPH_LOAD_FLAGS)
        };

        if result == SUCCESS {
            let slot = unsafe { (*face.face).glyph };
            assert!(slot != ptr::null_mut());
            Some(slot)
        } else {
            error!("Unable to load glyph for {} of size {:?} from font {:?}, {:?}",
                character, size, font_key, result);
            None
        }
    }

    fn get_glyph_dimensions_impl(slot: FT_GlyphSlot) -> Option<GlyphDimensions> {
        let metrics = unsafe { &(*slot).metrics };
        if metrics.width == 0 || metrics.height == 0 {
            None
        } else {
            let left = metrics.horiBearingX >> 6;
            let top = metrics.horiBearingY >> 6;
            let right = (metrics.horiBearingX + metrics.width + 0x3f) >> 6;
            let bottom = (metrics.horiBearingY + metrics.height + 0x3f) >> 6;
            Some(GlyphDimensions {
                left: left as i32,
                top: top as i32,
                width: (right - left) as u32,
                height: (bottom - top) as u32,
            })
        }
    }

    pub fn get_glyph_dimensions(&mut self,
                                key: &GlyphKey) -> Option<GlyphDimensions> {
        self.load_glyph(key.font_key, key.size, key.index)
            .and_then(Self::get_glyph_dimensions_impl)
    }

    pub fn rasterize_glyph(&mut self,
                           key: &GlyphKey,
                           render_mode: FontRenderMode,
                           _glyph_options: Option<GlyphOptions>)
                           -> Option<RasterizedGlyph> {

        let slot = match self.load_glyph(key.font_key, key.size, key.index) {
            Some(slot) => slot,
            None => return None,
        };
        let render_mode = match render_mode {
            FontRenderMode::Mono => FT_Render_Mode::FT_RENDER_MODE_MONO,
            FontRenderMode::Alpha => FT_Render_Mode::FT_RENDER_MODE_NORMAL,
            FontRenderMode::Subpixel => FT_Render_Mode::FT_RENDER_MODE_LCD,
        };

        let result = unsafe { FT_Render_Glyph(slot, render_mode) };
        if result != SUCCESS {
            error!("Unable to rasterize {:?} with {:?}, {:?}", key, render_mode, result);
            return None;
        }

        let dimensions = match Self::get_glyph_dimensions_impl(slot) {
            Some(val) => val,
            None => return None,
        };

        let bitmap = unsafe { &(*slot).bitmap };
        let pixel_mode = unsafe { mem::transmute(bitmap.pixel_mode as u32) };
        info!("Rasterizing {:?} as {:?} with dimensions {:?}", key, render_mode, dimensions);
        // we may be filling only a part of the buffer, so initialize the whole thing with 0
        let mut final_buffer = Vec::with_capacity(dimensions.width as usize *
                                                  dimensions.height as usize *
                                                  4);

        let offset_x = dimensions.left - unsafe { (*slot).bitmap_left };
        let src_pixel_width = match pixel_mode {
            FT_Pixel_Mode::FT_PIXEL_MODE_LCD => {
                assert!(bitmap.width % 3 == 0);
                bitmap.width / 3
            },
            _ => bitmap.width,
        };
        // determine the destination range of texels that `bitmap` provides data for
        let dst_start = cmp::max(0, -offset_x);
        let dst_end = cmp::min(dimensions.width as i32, src_pixel_width as i32 - offset_x);

        for y in 0 .. dimensions.height {
            let src_y = y as i32 - dimensions.top + unsafe { (*slot).bitmap_top };
            if src_y < 0 || src_y >= bitmap.rows as i32 {
                for _x in 0 .. dimensions.width {
                    final_buffer.extend_from_slice(&[0xff, 0xff, 0xff, 0]);
                }
                continue
            }
            let base = unsafe {
                bitmap.buffer.offset((src_y * bitmap.pitch) as isize)
            };
            for _x in 0 .. dst_start {
                final_buffer.extend_from_slice(&[0xff, 0xff, 0xff, 0]);
            }
            match pixel_mode {
                FT_Pixel_Mode::FT_PIXEL_MODE_MONO => {
                    for x in dst_start .. dst_end {
                        let src_x = x + offset_x;
                        let mask = 0x80 >> (src_x & 0x7);
                        let byte = unsafe {
                            *base.offset((src_x >> 3) as isize)
                        };
                        let alpha = if byte & mask != 0 { 0xff } else { 0 };
                        final_buffer.extend_from_slice(&[0xff, 0xff, 0xff, alpha]);
                    }
                }
                FT_Pixel_Mode::FT_PIXEL_MODE_GRAY => {
                    for x in dst_start .. dst_end {
                        let alpha = unsafe {
                            *base.offset((x + offset_x) as isize)
                        };
                        final_buffer.extend_from_slice(&[0xff, 0xff, 0xff, alpha]);
                    }
                }
                FT_Pixel_Mode::FT_PIXEL_MODE_LCD => {
                    for x in dst_start .. dst_end {
                        let src_x = ((x + offset_x) * 3) as isize;
                        assert!(src_x+2 < bitmap.pitch as isize);
                        let t = unsafe {
                            slice::from_raw_parts(base.offset(src_x), 3)
                        };
                        final_buffer.extend_from_slice(&[t[2], t[1], t[0], 0xff]);
                    }
                }
                _ => panic!("Unsupported {:?}", pixel_mode)
            }
            for _x in dst_end .. dimensions.width as i32 {
                final_buffer.extend_from_slice(&[0xff, 0xff, 0xff, 0]);
            }
            assert_eq!(final_buffer.len(), ((y+1) * dimensions.width * 4) as usize);
        }

        Some(RasterizedGlyph {
            left: dimensions.left as f32,
            top: dimensions.top as f32,
            width: dimensions.width as u32,
            height: dimensions.height as u32,
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

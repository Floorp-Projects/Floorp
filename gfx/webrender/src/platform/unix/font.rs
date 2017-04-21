/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use webrender_traits::{FontKey, FontRenderMode, GlyphDimensions};
use webrender_traits::{NativeFontHandle, GlyphOptions};
use webrender_traits::{GlyphKey};

use freetype::freetype::{FT_Render_Mode, FT_Pixel_Mode};
use freetype::freetype::{FT_Done_FreeType, FT_Library_SetLcdFilter};
use freetype::freetype::{FT_Library, FT_Set_Char_Size};
use freetype::freetype::{FT_Face, FT_Long, FT_UInt, FT_F26Dot6};
use freetype::freetype::{FT_Init_FreeType, FT_Load_Glyph, FT_Render_Glyph};
use freetype::freetype::{FT_New_Memory_Face, FT_GlyphSlot, FT_LcdFilter};
use freetype::freetype::{FT_Done_Face};

use std::{mem, ptr, slice};
use std::collections::HashMap;

struct Face {
    face: FT_Face,
}

pub struct FontContext {
    lib: FT_Library,
    faces: HashMap<FontKey, Face>,
}

pub struct RasterizedGlyph {
    pub width: u32,
    pub height: u32,
    pub bytes: Vec<u8>,
}

fn float_to_fixed(before: usize, f: f64) -> i32 {
    ((1i32 << before) as f64 * f) as i32
}

fn float_to_fixed_ft(f: f64) -> i32 {
    float_to_fixed(6, f)
}

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
            lib: lib,
            faces: HashMap::new(),
        }
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
                    face: face,
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

        unsafe {
            let char_size = float_to_fixed_ft(size.to_f64_px());
            let result = FT_Set_Char_Size(face.face, char_size as FT_F26Dot6, 0, 0, 0);
            assert!(result.succeeded());

            let result =  FT_Load_Glyph(face.face, character as FT_UInt, 0);
            if result.succeeded() {
                let void_glyph = (*face.face).glyph;
                let slot_ptr: FT_GlyphSlot = mem::transmute(void_glyph);
                assert!(!slot_ptr.is_null());
                return Some(slot_ptr);
            }
        }

        None
    }

     pub fn get_glyph_dimensions(&mut self,
                                 key: &GlyphKey) -> Option<GlyphDimensions> {
        self.load_glyph(key.font_key, key.size, key.index).and_then(|slot| {
            let metrics = unsafe { &(*slot).metrics };
            if metrics.width == 0 || metrics.height == 0 {
                None
            } else {
                Some(GlyphDimensions {
                    left: (metrics.horiBearingX >> 6) as i32,
                    top: (metrics.horiBearingY >> 6) as i32,
                    width: (metrics.width >> 6) as u32,
                    height: (metrics.height >> 6) as u32,
                })
            }
        })
    }

    pub fn rasterize_glyph(&mut self,
                           key: &GlyphKey,
                           render_mode: FontRenderMode,
                           _glyph_options: Option<GlyphOptions>)
                           -> Option<RasterizedGlyph> {
        let mut glyph = None;

        if let Some(slot) = self.load_glyph(key.font_key,
                                            key.size,
                                            key.index) {
            let render_mode = match render_mode {
                FontRenderMode::Mono => FT_Render_Mode::FT_RENDER_MODE_MONO,
                FontRenderMode::Alpha => FT_Render_Mode::FT_RENDER_MODE_NORMAL,
                FontRenderMode::Subpixel => FT_Render_Mode::FT_RENDER_MODE_LCD,
            };

            let result = unsafe { FT_Render_Glyph(slot, render_mode) };

            if result.succeeded() {
                let bitmap = unsafe { &(*slot).bitmap };

                let metrics = unsafe { &(*slot).metrics };
                let mut glyph_width = (metrics.width >> 6) as i32;
                let glyph_height = (metrics.height >> 6) as i32;
                let mut final_buffer = Vec::with_capacity(glyph_width as usize *
                                                          glyph_height as usize *
                                                          4);

                if bitmap.pixel_mode == FT_Pixel_Mode::FT_PIXEL_MODE_MONO as u8 {
                    // This is not exactly efficient... but it's only used by the
                    // reftest pass when we have AA disabled on glyphs.
                    let offset_x = unsafe { (metrics.horiBearingX >> 6) as i32 - (*slot).bitmap_left };
                    let offset_y = unsafe { (metrics.horiBearingY >> 6) as i32 - (*slot).bitmap_top };

                    // Due to AA being disabled, the bitmap produced for mono
                    // glyphs is often smaller than the reported glyph dimensions.
                    // To account for this, place the rendered glyph within the
                    // box of the glyph dimensions, filling in invalid pixels with
                    // zero alpha.
                    for iy in 0..glyph_height {
                        let y = iy - offset_y;
                        for ix in 0..glyph_width {
                            let x = ix + offset_x;
                            let valid_byte = x >= 0 &&
                                y >= 0 &&
                                x < bitmap.width as i32 &&
                                y < bitmap.rows as i32;
                            let byte_value = if valid_byte {
                                let byte_index = (y * bitmap.pitch as i32) + (x >> 3);

                                unsafe {
                                    let bit_index = x & 7;
                                    let byte_ptr = bitmap.buffer.offset(byte_index as isize);
                                    let bit = (*byte_ptr & (0x80 >> bit_index)) != 0;
                                    if bit {
                                        0xff
                                    } else {
                                        0
                                    }
                                }
                            } else {
                                0
                            };

                            final_buffer.extend_from_slice(&[ 0xff, 0xff, 0xff, byte_value ]);
                        }
                    }
                } else if bitmap.pixel_mode == FT_Pixel_Mode::FT_PIXEL_MODE_GRAY as u8 {
                    // We can assume that the reported glyph dimensions exactly
                    // match the rasterized bitmap for normal alpha coverage glyphs.

                    let buffer = unsafe {
                        slice::from_raw_parts(
                            bitmap.buffer,
                            (bitmap.width * bitmap.rows) as usize
                        )
                    };

                    // Convert to RGBA.
                    for &byte in buffer.iter() {
                        final_buffer.extend_from_slice(&[ 0xff, 0xff, 0xff, byte ]);
                    }
                } else if bitmap.pixel_mode == FT_Pixel_Mode::FT_PIXEL_MODE_LCD as u8 {
                    // Extra subpixel on each side of the glyph.
                    glyph_width += 2;

                    for y in 0..bitmap.rows {
                        for x in 0..(bitmap.width / 3) {
                            let index = (y as i32 * bitmap.pitch) + (x as i32 * 3);

                            unsafe {
                                let ptr = bitmap.buffer.offset(index as isize);
                                let b = *ptr;
                                let g = *(ptr.offset(1));
                                let r = *(ptr.offset(2));

                                final_buffer.extend_from_slice(&[r, g, b, 0xff]);
                            }
                        }
                    }
                } else {
                    panic!("Unexpected render mode: {}!", bitmap.pixel_mode);
                }

                glyph = Some(RasterizedGlyph {
                    width: glyph_width as u32,
                    height: glyph_height as u32,
                    bytes: final_buffer,
                });
            }
        }

        glyph
    }
}

impl Drop for FontContext {
    fn drop(&mut self) {
        unsafe {
            FT_Done_FreeType(self.lib);
        }
    }
}

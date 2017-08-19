/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{FontInstance, FontKey, FontRenderMode, GlyphDimensions};
use api::{NativeFontHandle, SubpixelDirection};
use api::{GlyphKey};
use internal_types::FastHashMap;

use freetype::freetype::{FT_Render_Mode, FT_Pixel_Mode, FT_BBox, FT_Outline_Translate};
use freetype::freetype::{FT_Done_FreeType, FT_Library_SetLcdFilter, FT_Pos};
use freetype::freetype::{FT_Library, FT_Set_Char_Size, FT_Outline_Get_CBox};
use freetype::freetype::{FT_Face, FT_Long, FT_UInt, FT_F26Dot6, FT_Glyph_Format};
use freetype::freetype::{FT_Init_FreeType, FT_Load_Glyph, FT_Render_Glyph};
use freetype::freetype::{FT_New_Memory_Face, FT_GlyphSlot, FT_LcdFilter};
use freetype::freetype::{FT_Done_Face, FT_Error, FT_Int32, FT_Get_Char_Index};

use std::{mem, ptr, slice};
use std::sync::Arc;

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
    // Raw byte data has to live until the font is deleted, according to
    // https://www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#FT_New_Memory_Face
    _bytes: Arc<Vec<u8>>,
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

        // Per Skia, using a filter adds one full pixel to each side.
        let mut lcd_extra_pixels = 1;

        unsafe {
            let result = FT_Init_FreeType(&mut lib);
            assert!(result.succeeded(), "Unable to initialize FreeType library {:?}", result);

            // TODO(gw): Check result of this to determine if freetype build supports subpixel.
            let result = FT_Library_SetLcdFilter(lib, FT_LcdFilter::FT_LCD_FILTER_DEFAULT);

            if !result.succeeded() {
                println!("WARN: Initializing a FreeType library build without subpixel AA enabled!");
                lcd_extra_pixels = 0;
            }
        }

        FontContext {
            lib,
            faces: FastHashMap::default(),
            lcd_extra_pixels: lcd_extra_pixels,
        }
    }

    pub fn has_font(&self, font_key: &FontKey) -> bool {
        self.faces.contains_key(font_key)
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, bytes: Arc<Vec<u8>>, index: u32) {
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
                    _bytes: bytes,
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
                  font: &FontInstance,
                  glyph: &GlyphKey) -> Option<FT_GlyphSlot> {

        debug_assert!(self.faces.contains_key(&font.font_key));
        let face = self.faces.get(&font.font_key).unwrap();
        let char_size = font.size.to_f64_px() * 64.0 + 0.5;

        assert_eq!(SUCCESS, unsafe {
            FT_Set_Char_Size(face.face, char_size as FT_F26Dot6, 0, 0, 0)
        });

        let result = unsafe {
            FT_Load_Glyph(face.face,
                          glyph.index as FT_UInt,
                          GLYPH_LOAD_FLAGS)
        };

        if result == SUCCESS {
            let slot = unsafe { (*face.face).glyph };
            assert!(slot != ptr::null_mut());

            // TODO(gw): We use the FT_Outline_* APIs to manage sub-pixel offsets.
            //           We will need a custom code path for bitmap fonts (which
            //           are very rare).
            match unsafe { (*slot).format } {
                FT_Glyph_Format::FT_GLYPH_FORMAT_OUTLINE => {
                    Some(slot)
                }
                _ => {
                    error!("TODO: Support bitmap fonts!");
                    None
                }
            }
        } else {
            error!("Unable to load glyph for {} of size {:?} from font {:?}, {:?}",
                glyph.index, font.size, font.font_key, result);
            None
        }
    }

    // Get the bounding box for a glyph, accounting for sub-pixel positioning.
    fn get_bounding_box(&self,
                        slot: FT_GlyphSlot,
                        font: &FontInstance,
                        glyph: &GlyphKey) -> FT_BBox {
        let mut cbox: FT_BBox = unsafe { mem::uninitialized() };

        // Get the estimated bounding box from FT (control points).
        unsafe {
            FT_Outline_Get_CBox(&(*slot).outline, &mut cbox);
        }

        // Convert the subpixel offset to floats.
        let (dx, dy) = font.get_subpx_offset(glyph);

        // Apply extra pixel of padding for subpixel AA, due to the filter.
        let padding = match font.render_mode {
            FontRenderMode::Subpixel => self.lcd_extra_pixels * 64,
            FontRenderMode::Alpha | FontRenderMode::Mono => 0,
        };
        cbox.xMin -= padding as FT_Pos;
        cbox.xMax += padding as FT_Pos;

        // Offset the bounding box by subpixel positioning.
        // Convert to 26.6 fixed point format for FT.
        match font.subpx_dir {
            SubpixelDirection::None => {}
            SubpixelDirection::Horizontal => {
                let dx = (dx * 64.0 + 0.5) as FT_Long;
                cbox.xMin += dx;
                cbox.xMax += dx;
            }
            SubpixelDirection::Vertical => {
                let dy = (dy * 64.0 + 0.5) as FT_Long;
                cbox.yMin += dy;
                cbox.yMax += dy;
            }
        }

        // Outset the box to device pixel boundaries
        cbox.xMin &= !63;
        cbox.yMin &= !63;
        cbox.xMax = (cbox.xMax + 63) & !63;
        cbox.yMax = (cbox.yMax + 63) & !63;

        cbox
    }

    fn get_glyph_dimensions_impl(&self,
                                 slot: FT_GlyphSlot,
                                 font: &FontInstance,
                                 glyph: &GlyphKey) -> Option<GlyphDimensions> {
        let metrics = unsafe { &(*slot).metrics };

        // If there's no advance, no need to consider this glyph
        // for layout.
        if metrics.horiAdvance == 0 {
            None
        } else {
            let cbox = self.get_bounding_box(slot, font, glyph);

            Some(GlyphDimensions {
                left: (cbox.xMin >> 6) as i32,
                top: (cbox.yMax >> 6) as i32,
                width: ((cbox.xMax - cbox.xMin) >> 6) as u32,
                height: ((cbox.yMax - cbox.yMin) >> 6) as u32,
                advance: metrics.horiAdvance as f32 / 64.0,
            })
        }
    }

    pub fn get_glyph_index(&mut self, font_key: FontKey, ch: char) -> Option<u32> {
        let face = self.faces
                       .get(&font_key)
                       .expect("Unknown font key!");
        unsafe {
            let idx = FT_Get_Char_Index(face.face, ch as _);
            if idx != 0 {
                Some(idx)
            } else {
                None
            }
        }
    }

    pub fn get_glyph_dimensions(&mut self,
                                font: &FontInstance,
                                key: &GlyphKey) -> Option<GlyphDimensions> {
        let slot = self.load_glyph(font, key);
        slot.and_then(|slot| {
            self.get_glyph_dimensions_impl(slot, font, key)
        })
    }

    pub fn rasterize_glyph(&mut self,
                           font: &FontInstance,
                           key: &GlyphKey)
                           -> Option<RasterizedGlyph> {

        let slot = match self.load_glyph(font, key) {
            Some(slot) => slot,
            None => return None,
        };

        let render_mode = match font.render_mode {
            FontRenderMode::Mono => FT_Render_Mode::FT_RENDER_MODE_MONO,
            FontRenderMode::Alpha => FT_Render_Mode::FT_RENDER_MODE_NORMAL,
            FontRenderMode::Subpixel => FT_Render_Mode::FT_RENDER_MODE_LCD,
        };

        // Get dimensions of the glyph, to see if we need to rasterize it.
        let dimensions = match self.get_glyph_dimensions_impl(slot, font, key) {
            Some(val) => val,
            None => return None,
        };

        // For spaces and other non-printable characters, early out.
        if dimensions.width == 0 || dimensions.height == 0 {
            return None;
        }

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
            FT_Outline_Translate(outline, dx - ((cbox.xMin + dx) & !63),
                                          dy - ((cbox.yMin + dy) & !63));
        }

        let result = unsafe { FT_Render_Glyph(slot, render_mode) };
        if result != SUCCESS {
            error!("Unable to rasterize {:?} with {:?}, {:?}", key, render_mode, result);
            return None;
        }

        let bitmap = unsafe { &(*slot).bitmap };
        let pixel_mode = unsafe { mem::transmute(bitmap.pixel_mode as u32) };
        info!("Rasterizing {:?} as {:?} with dimensions {:?}", key, render_mode, dimensions);

        let actual_width = match pixel_mode {
            FT_Pixel_Mode::FT_PIXEL_MODE_LCD => {
                assert!(bitmap.width % 3 == 0);
                bitmap.width / 3
            },
            FT_Pixel_Mode::FT_PIXEL_MODE_MONO |
            FT_Pixel_Mode::FT_PIXEL_MODE_GRAY => {
                bitmap.width
            }
            _ => {
                panic!("Unexpected pixel mode!");
            }
        } as i32;

        let actual_height = bitmap.rows as i32;
        let top = unsafe { (*slot).bitmap_top };
        let left = unsafe { (*slot).bitmap_left };
        let mut final_buffer = vec![0; (actual_width * actual_height * 4) as usize];

        // Extract the final glyph from FT format into RGBA8 format, which is
        // what WR expects.
        for y in 0..actual_height {
            // Get pointer to the bytes for this row.
            let mut src = unsafe {
                bitmap.buffer.offset((y * bitmap.pitch) as isize)
            };

            for x in 0..actual_width {
                let value = match pixel_mode {
                    FT_Pixel_Mode::FT_PIXEL_MODE_MONO => {
                        let mask = 0x80 >> (x & 0x7);
                        let byte = unsafe { *src.offset((x >> 3) as isize) };
                        let alpha = if byte & mask != 0 { 0xff } else { 0 };
                        [0xff, 0xff, 0xff, alpha]
                    }
                    FT_Pixel_Mode::FT_PIXEL_MODE_GRAY => {
                        let alpha = unsafe { *src };
                        src = unsafe { src.offset(1) };
                        [0xff, 0xff, 0xff, alpha]
                    }
                    FT_Pixel_Mode::FT_PIXEL_MODE_LCD => {
                        let t = unsafe { slice::from_raw_parts(src, 3) };
                        src = unsafe { src.offset(3) };
                        [t[2], t[1], t[0], 0xff]
                    }
                    _ => panic!("Unsupported {:?}", pixel_mode)
                };
                let i = 4 * (y * actual_width + x) as usize;
                let dest = &mut final_buffer[i..i+4];
                dest.clone_from_slice(&value);
            }
        }

        Some(RasterizedGlyph {
            left: (dimensions.left + left) as f32,
            top: (dimensions.top + top - actual_height) as f32,
            width: actual_width as u32,
            height: actual_height as u32,
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

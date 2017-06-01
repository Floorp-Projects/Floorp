/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use core_graphics::base::{kCGImageAlphaNoneSkipFirst, kCGImageAlphaPremultipliedLast};
use core_graphics::base::kCGBitmapByteOrder32Little;
use core_graphics::color_space::CGColorSpace;
use core_graphics::context::{CGContext, CGTextDrawingMode};
use core_graphics::data_provider::CGDataProvider;
use core_graphics::font::{CGFont, CGGlyph};
use core_graphics::geometry::{CGPoint, CGSize, CGRect};
use core_text::font::CTFont;
use core_text::font_descriptor::kCTFontDefaultOrientation;
use core_text;
use std::collections::HashMap;
use std::collections::hash_map::Entry;
use webrender_traits::{ColorU, FontKey, FontRenderMode, GlyphDimensions};
use webrender_traits::{GlyphKey, GlyphOptions, SubpixelPoint};
use webrender_traits::NativeFontHandle;
use gamma_lut::{GammaLut, Color as ColorLut};

pub struct FontContext {
    cg_fonts: HashMap<FontKey, CGFont>,
    ct_fonts: HashMap<(FontKey, Au), CTFont>,
    gamma_lut: GammaLut,
}

// core text is safe to use on multiple threads and non-shareable resources are
// all hidden inside their font context.
unsafe impl Send for FontContext {}

pub struct RasterizedGlyph {
    pub width: u32,
    pub height: u32,
    pub bytes: Vec<u8>,
}

impl RasterizedGlyph {
    pub fn blank() -> RasterizedGlyph {
        RasterizedGlyph {
            width: 0,
            height: 0,
            bytes: vec![],
        }
    }
}

struct GlyphMetrics {
    rasterized_left: i32,
    rasterized_descent: i32,
    rasterized_ascent: i32,
    rasterized_width: u32,
    rasterized_height: u32,
}

// According to the Skia source code, there's no public API to
// determine if subpixel AA is supported. So jrmuizel ported
// this function from Skia which is used to check if a glyph
// can be rendered with subpixel AA.
fn supports_subpixel_aa() -> bool {
    let mut cg_context = CGContext::create_bitmap_context(1, 1, 8, 4,
                                                          &CGColorSpace::create_device_rgb(),
                                                          kCGImageAlphaNoneSkipFirst |
                                                          kCGBitmapByteOrder32Little);
    let ct_font = core_text::font::new_from_name("Helvetica", 16.).unwrap();
    cg_context.set_should_smooth_fonts(true);
    cg_context.set_should_antialias(true);
    cg_context.set_allows_font_smoothing(true);
    cg_context.set_rgb_fill_color(1.0, 1.0, 1.0, 1.0);
    let point = CGPoint {x: -1., y: 0.};
    let glyph = '|' as CGGlyph;
    ct_font.draw_glyphs(&[glyph], &[point], cg_context.clone());
    let data = cg_context.data();
    data[0] != data[1] || data[1] != data[2]
}

fn get_glyph_metrics(ct_font: &CTFont,
                     glyph: CGGlyph,
                     subpixel_point: &SubpixelPoint) -> GlyphMetrics {
    let bounds = ct_font.get_bounding_rects_for_glyphs(kCTFontDefaultOrientation, &[glyph]);

    if bounds.origin.x.is_nan() || bounds.origin.y.is_nan() ||
       bounds.size.width.is_nan() || bounds.size.height.is_nan() {
        // If an unexpected glyph index is requested, core text will return NaN values
        // which causes us to do bad thing as the value is cast into an integer and
        // overflow when expanding the bounds a few lines below.
        // Instead we are better off returning zero-sized metrics because this special
        // case is handled by the callers of this method.
        return GlyphMetrics {
            rasterized_left: 0,
            rasterized_width: 0,
            rasterized_height: 0,
            rasterized_ascent: 0,
            rasterized_descent: 0,
        };
    }

    let (x_offset, y_offset) = subpixel_point.to_f64();

    // First round out to pixel boundaries
    // CG Origin is bottom left
    let mut left = bounds.origin.x.floor() as i32;
    let mut bottom = bounds.origin.y.floor() as i32;
    let mut right = (bounds.origin.x
                    + bounds.size.width
                    + x_offset).ceil() as i32;
    let mut top = (bounds.origin.y
                  + bounds.size.height
                  + y_offset).ceil() as i32;

    // Expand the bounds by 1 pixel, to give CG room for anti-aliasing.
    // Note that this outset is to allow room for LCD smoothed glyphs. However, the correct outset
    // is not currently known, as CG dilates the outlines by some percentage.
    // This is taken from Skia.
    left -= 1;
    bottom -= 1;
    right += 1;
    top += 1;

    let width = right - left;
    let height = top - bottom;

    let metrics = GlyphMetrics {
        rasterized_left: left,
        rasterized_width: width as u32,
        rasterized_height: height as u32,
        rasterized_ascent: top,
        rasterized_descent: -bottom,
    };

    metrics
}

impl FontContext {
    pub fn new() -> FontContext {
        debug!("Test for subpixel AA support: {}", supports_subpixel_aa());

        // Force CG to use sRGB color space to gamma correct.
        let contrast = 0.0;
        let gamma = 0.0;

        FontContext {
            cg_fonts: HashMap::new(),
            ct_fonts: HashMap::new(),
            gamma_lut: GammaLut::new(contrast, gamma, gamma),
        }
    }

    pub fn has_font(&self, font_key: &FontKey) -> bool {
        self.cg_fonts.contains_key(font_key)
    }

    pub fn add_raw_font(&mut self, font_key: &FontKey, bytes: &[u8], index: u32) {
        if self.cg_fonts.contains_key(font_key) {
            return
        }

        assert_eq!(index, 0);
        let data_provider = CGDataProvider::from_buffer(bytes);
        let cg_font = match CGFont::from_data_provider(data_provider) {
            Err(_) => return,
            Ok(cg_font) => cg_font,
        };
        self.cg_fonts.insert((*font_key).clone(), cg_font);
    }

    pub fn add_native_font(&mut self, font_key: &FontKey, native_font_handle: NativeFontHandle) {
        if self.cg_fonts.contains_key(font_key) {
            return
        }

        self.cg_fonts.insert((*font_key).clone(), native_font_handle.0);
    }

    pub fn delete_font(&mut self, font_key: &FontKey) {
        if let Some(_) = self.cg_fonts.remove(font_key) {
            // Unstable Rust has a retain() method on HashMap that will
            // let us do this in-place. https://github.com/rust-lang/rust/issues/36648
            let ct_font_keys = self.ct_fonts.keys()
                                            .filter(|k| k.0 == *font_key)
                                            .cloned()
                                            .collect::<Vec<_>>();
            for ct_font_key in ct_font_keys {
                self.ct_fonts.remove(&ct_font_key);
            }
        }
    }

    fn get_ct_font(&mut self,
                   font_key: FontKey,
                   size: Au) -> Option<CTFont> {
        match self.ct_fonts.entry(((font_key).clone(), size)) {
            Entry::Occupied(entry) => Some((*entry.get()).clone()),
            Entry::Vacant(entry) => {
                let cg_font = match self.cg_fonts.get(&font_key) {
                    None => return None,
                    Some(cg_font) => cg_font,
                };
                let ct_font = core_text::font::new_from_CGFont(
                        cg_font,
                        size.to_f64_px());
                entry.insert(ct_font.clone());
                Some(ct_font)
            }
        }
    }

    pub fn get_glyph_dimensions(&mut self,
                                key: &GlyphKey) -> Option<GlyphDimensions> {
        self.get_ct_font(key.font_key, key.size).and_then(|ref ct_font| {
            let glyph = key.index as CGGlyph;
            let metrics = get_glyph_metrics(ct_font, glyph, &key.subpixel_point);
            if metrics.rasterized_width == 0 || metrics.rasterized_height == 0 {
                None
            } else {
                Some(GlyphDimensions {
                    left: metrics.rasterized_left,
                    top: metrics.rasterized_ascent,
                    width: metrics.rasterized_width as u32,
                    height: metrics.rasterized_height as u32,
                })
            }
        })
    }

    // Assumes the pixels here are linear values from CG
    fn gamma_correct_pixels(&self, pixels: &mut Vec<u8>, width: usize,
                            height: usize, render_mode: FontRenderMode,
                            color: ColorU) {
        // Then convert back to gamma corrected values.
        let color_lut = ColorLut::new(color.r,
                                     color.g,
                                     color.b,
                                     color.a);
        match render_mode {
            FontRenderMode::Alpha => {
                self.gamma_lut.preblend_grayscale_bgra(pixels, width,
                                                       height, color_lut);
            },
            FontRenderMode::Subpixel => {
                self.gamma_lut.preblend_bgra(pixels, width, height, color_lut);
            },
            _ => {} // Again, give mono untouched since only the alpha matters.
        }
    }

    #[allow(dead_code)]
    fn print_glyph_data(&mut self, data: &[u8], width: usize, height: usize) {
        // Rust doesn't have step_by support on stable :(
        println!("Width is: {:?} height: {:?}", width, height);
        for i in 0..height {
            let current_height = i * width * 4;

            for pixel in data[current_height .. current_height + (width * 4)].chunks(4) {
                let b = pixel[0];
                let g = pixel[1];
                let r = pixel[2];
                let a = pixel[3];
                print!("({}, {}, {}, {}) ", r, g, b, a);
            }
            println!("");
        }
    }

    pub fn rasterize_glyph(&mut self,
                           key: &GlyphKey,
                           render_mode: FontRenderMode,
                           _glyph_options: Option<GlyphOptions>)
                           -> Option<RasterizedGlyph> {

        let ct_font = match self.get_ct_font(key.font_key, key.size) {
            Some(font) => font,
            None => return Some(RasterizedGlyph::blank())
        };

        let glyph = key.index as CGGlyph;
        let metrics = get_glyph_metrics(&ct_font, glyph, &key.subpixel_point);
        if metrics.rasterized_width == 0 || metrics.rasterized_height == 0 {
            return Some(RasterizedGlyph::blank())
        }

        let context_flags = match render_mode {
            FontRenderMode::Subpixel => kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
            FontRenderMode::Alpha | FontRenderMode::Mono => kCGImageAlphaPremultipliedLast,
        };

        let mut cg_context = CGContext::create_bitmap_context(metrics.rasterized_width as usize,
                                                              metrics.rasterized_height as usize,
                                                              8,
                                                              metrics.rasterized_width as usize * 4,
                                                              &CGColorSpace::create_device_rgb(),
                                                              context_flags);


        // Tested on mac OS Sierra, 10.12
        // For Mono + alpha, the only values that matter are the alpha values.
        // For subpixel, we need each individual rgb channel.
        // CG has two individual glyphs for subpixel AA (pre-10.11, this is not true):
        // 1) black text on white opaque background
        // 2) white text on black opaque background
        // Gecko does (1). Note, the BG must be opaque for subpixel AA to work.
        // See https://bugzilla.mozilla.org/show_bug.cgi?id=1230366#c35
        //
        // For grayscale / mono, CG still produces two glyphs, but it doesn't matter
        // 1) black text on transparent white - only alpha values filled
        // 2) white text on transparent black - channels == alpha
        //
        // If we draw grayscale/mono on an opaque background
        // the RGB channels are the alpha values from transparent backgrounds
        // with the alpha set as opaque.
        // At the end of all this, WR expects individual RGB channels and ignores alpha
        // for subpixel AA.
        // For alpha/mono, WR ignores all channels other than alpha.
        // Also note that WR expects text to be black bg with white text, so invert
        // when we draw the glyphs.
        let (antialias, smooth) = match render_mode {
            FontRenderMode::Subpixel => (true, true),
            FontRenderMode::Alpha => (true, false),
            FontRenderMode::Mono => (false, false),
        };

        // These are always true in Gecko, even for non-AA fonts
        cg_context.set_allows_font_subpixel_positioning(true);
        cg_context.set_should_subpixel_position_fonts(true);

        // Don't quantize because we're doing it already.
        cg_context.set_allows_font_subpixel_quantization(false);
        cg_context.set_should_subpixel_quantize_fonts(false);

        cg_context.set_allows_font_smoothing(smooth);
        cg_context.set_should_smooth_fonts(smooth);
        cg_context.set_allows_antialiasing(antialias);
        cg_context.set_should_antialias(antialias);

        let (x_offset, y_offset) = key.subpixel_point.to_f64();

        // CG Origin is bottom left, WR is top left. Need -y offset
        let rasterization_origin = CGPoint {
            x: -metrics.rasterized_left as f64 + x_offset,
            y: metrics.rasterized_descent as f64 - y_offset,
        };

        // Always draw black text on a white background
        // Fill the background
        cg_context.set_rgb_fill_color(1.0, 1.0, 1.0, 1.0);
        let rect = CGRect {
            origin: CGPoint {
                x: 0.0,
                y: 0.0,
            },
            size: CGSize {
                width: metrics.rasterized_width as f64,
                height: metrics.rasterized_height as f64,
            }
        };
        cg_context.fill_rect(rect);

        // Set the text color
        cg_context.set_rgb_fill_color(0.0, 0.0, 0.0, 1.0);
        cg_context.set_text_drawing_mode(CGTextDrawingMode::CGTextFill);
        ct_font.draw_glyphs(&[glyph], &[rasterization_origin], cg_context.clone());

        let mut rasterized_pixels = cg_context.data().to_vec();

        // Convert to linear space for subpixel AA.
        // We explicitly do not do this for grayscale AA
        if render_mode == FontRenderMode::Subpixel {
            self.gamma_lut.coregraphics_convert_to_linear_bgra(&mut rasterized_pixels,
                                                               metrics.rasterized_width as usize,
                                                               metrics.rasterized_height as usize);
        }

        // We need to invert the pixels back since right now
        // transparent pixels are actually opaque white.
        for i in 0..metrics.rasterized_height {
            let current_height = (i * metrics.rasterized_width * 4) as usize;
            let end_row = current_height + (metrics.rasterized_width as usize * 4);

            for mut pixel in rasterized_pixels[current_height .. end_row].chunks_mut(4) {
                pixel[0] = 255 - pixel[0];
                pixel[1] = 255 - pixel[1];
                pixel[2] = 255 - pixel[2];

                pixel[3] = match render_mode {
                    FontRenderMode::Subpixel => 255,
                    _ => {
                        assert_eq!(pixel[0], pixel[1]);
                        assert_eq!(pixel[0], pixel[2]);
                        pixel[0]
                    }
                }; // end match
            } // end row
        } // end height

        self.gamma_correct_pixels(&mut rasterized_pixels,
                                  metrics.rasterized_width as usize,
                                  metrics.rasterized_height as usize,
                                  render_mode,
                                  key.color);

        Some(RasterizedGlyph {
            width: metrics.rasterized_width,
            height: metrics.rasterized_height,
            bytes: rasterized_pixels,
        })
    }
}


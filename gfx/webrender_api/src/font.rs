/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use {ColorU, ColorF, LayoutPoint};
use std::sync::Arc;

#[cfg(target_os = "macos")] use core_foundation::string::CFString;
#[cfg(target_os = "macos")] use core_graphics::font::CGFont;
#[cfg(target_os = "macos")] use serde::de::{self, Deserialize, Deserializer};
#[cfg(target_os = "macos")] use serde::ser::{Serialize, Serializer};
#[cfg(target_os = "windows")] use dwrote::FontDescriptor;


#[cfg(target_os = "macos")]
#[derive(Clone)]
pub struct NativeFontHandle(pub CGFont);

#[cfg(target_os = "macos")]
impl Serialize for NativeFontHandle {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: Serializer {
        let postscript_name = self.0.postscript_name().to_string();
        postscript_name.serialize(serializer)
    }
}

#[cfg(target_os = "macos")]
impl<'de> Deserialize<'de> for NativeFontHandle {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error> where D: Deserializer<'de> {
        let postscript_name: String = try!(Deserialize::deserialize(deserializer));

        match CGFont::from_name(&CFString::new(&*postscript_name)) {
            Ok(font) => Ok(NativeFontHandle(font)),
            _ => Err(de::Error::custom("Couldn't find a font with that PostScript name!")),
        }
    }
}

/// Native fonts are not used on Linux; all fonts are raw.
#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[cfg_attr(not(any(target_os = "macos", target_os = "windows")), derive(Clone, Serialize, Deserialize))]
pub struct NativeFontHandle;

#[cfg(target_os = "windows")]
pub type NativeFontHandle = FontDescriptor;

#[repr(C)]
#[derive(Copy, Clone, Deserialize, Serialize, Debug)]
pub struct GlyphDimensions {
    pub left: i32,
    pub top: i32,
    pub width: u32,
    pub height: u32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize, Ord, PartialOrd)]
pub struct FontKey(pub u32, pub u32);

impl FontKey {
    pub fn new(key0: u32, key1: u32) -> FontKey {
        FontKey(key0, key1)
    }
}


#[derive(Clone)]
pub enum FontTemplate {
    Raw(Arc<Vec<u8>>, u32),
    Native(NativeFontHandle),
}

#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq, Serialize, Deserialize, Ord, PartialOrd)]
pub enum FontRenderMode {
    Mono,
    Alpha,
    Subpixel,
}

const FIXED16_SHIFT: i32 = 16;

// This matches the behaviour of SkScalarToFixed
fn f32_truncate_to_fixed16(x: f32) -> i32 {
    let fixed1 = (1 << FIXED16_SHIFT) as f32;
    (x * fixed1) as i32
}

impl FontRenderMode {
    // Skia quantizes subpixel offets into 1/4 increments.
    // Given the absolute position, return the quantized increment
    fn subpixel_quantize_offset(&self, pos: f32) -> SubpixelOffset {
        if *self != FontRenderMode::Subpixel {
            return SubpixelOffset::Zero;
        }

        const SUBPIXEL_BITS: i32 = 2;
        const SUBPIXEL_FIXED16_MASK: i32 = ((1 << SUBPIXEL_BITS) - 1) << (FIXED16_SHIFT - SUBPIXEL_BITS);

        const SUBPIXEL_ROUNDING: f32 = 0.5 / (1 << SUBPIXEL_BITS) as f32;
        let pos = pos + SUBPIXEL_ROUNDING;
        let fraction = (f32_truncate_to_fixed16(pos) & SUBPIXEL_FIXED16_MASK) >> (FIXED16_SHIFT - SUBPIXEL_BITS);

        match fraction {
            0 => SubpixelOffset::Zero,
            1 => SubpixelOffset::Quarter,
            2 => SubpixelOffset::Half,
            3 => SubpixelOffset::ThreeQuarters,
            _ => panic!("Should only be given the fractional part"),
        }
    }
}

#[repr(u8)]
#[derive(Hash, Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd, Serialize, Deserialize)]
pub enum SubpixelOffset {
    Zero            = 0,
    Quarter         = 1,
    Half            = 2,
    ThreeQuarters   = 3,
}

impl Into<f64> for SubpixelOffset {
    fn into(self) -> f64 {
        match self {
            SubpixelOffset::Zero => 0.0,
            SubpixelOffset::Quarter => 0.25,
            SubpixelOffset::Half => 0.5,
            SubpixelOffset::ThreeQuarters => 0.75,
        }
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize, Ord, PartialOrd)]
pub struct SubpixelPoint {
    pub x: SubpixelOffset,
    pub y: SubpixelOffset,
}

impl SubpixelPoint {
    pub fn new(point: LayoutPoint,
               render_mode: FontRenderMode) -> SubpixelPoint {
        SubpixelPoint {
            x: render_mode.subpixel_quantize_offset(point.x),
            y: render_mode.subpixel_quantize_offset(point.y),
        }
    }

    pub fn to_f64(&self) -> (f64, f64) {
        (self.x.into(), self.y.into())
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize, Ord, PartialOrd)]
pub struct GlyphKey {
    pub font_key: FontKey,
    // The font size is in *device* pixels, not logical pixels.
    // It is stored as an Au since we need sub-pixel sizes, but
    // can't store as a f32 due to use of this type as a hash key.
    // TODO(gw): Perhaps consider having LogicalAu and DeviceAu
    //           or something similar to that.
    pub size: Au,
    pub index: u32,
    pub color: ColorU,
    pub subpixel_point: SubpixelPoint,
}

impl GlyphKey {
    pub fn new(font_key: FontKey,
               size: Au,
               color: ColorF,
               index: u32,
               point: LayoutPoint,
               render_mode: FontRenderMode) -> GlyphKey {
        GlyphKey {
            font_key,
            size,
            color: ColorU::from(color),
            index,
            subpixel_point: SubpixelPoint::new(point, render_mode),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GlyphInstance {
    pub index: u32,
    pub point: LayoutPoint,
}

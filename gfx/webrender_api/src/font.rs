/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use {ColorF, ColorU, IdNamespace, LayoutPoint, ToBits};
use app_units::Au;
#[cfg(target_os = "macos")]
use core_foundation::string::CFString;
#[cfg(target_os = "macos")]
use core_graphics::font::CGFont;
#[cfg(target_os = "windows")]
use dwrote::FontDescriptor;
#[cfg(target_os = "macos")]
use serde::de::{self, Deserialize, Deserializer};
#[cfg(target_os = "macos")]
use serde::ser::{Serialize, Serializer};
use std::cmp::Ordering;
use std::hash::{Hash, Hasher};
use std::sync::Arc;


#[cfg(target_os = "macos")]
#[derive(Clone)]
pub struct NativeFontHandle(pub CGFont);

#[cfg(target_os = "macos")]
impl Serialize for NativeFontHandle {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let postscript_name = self.0.postscript_name().to_string();
        postscript_name.serialize(serializer)
    }
}

#[cfg(target_os = "macos")]
impl<'de> Deserialize<'de> for NativeFontHandle {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        let postscript_name: String = try!(Deserialize::deserialize(deserializer));

        match CGFont::from_name(&CFString::new(&*postscript_name)) {
            Ok(font) => Ok(NativeFontHandle(font)),
            _ => Err(de::Error::custom(
                "Couldn't find a font with that PostScript name!",
            )),
        }
    }
}

/// Native fonts are not used on Linux; all fonts are raw.
#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[cfg_attr(not(any(target_os = "macos", target_os = "windows")),
           derive(Clone, Serialize, Deserialize))]
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
    pub advance: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize, Ord, PartialOrd)]
pub struct FontKey(pub IdNamespace, pub u32);

impl FontKey {
    pub fn new(namespace: IdNamespace, key: u32) -> FontKey {
        FontKey(namespace, key)
    }
}


#[derive(Clone)]
pub enum FontTemplate {
    Raw(Arc<Vec<u8>>, u32),
    Native(NativeFontHandle),
}

#[repr(u32)]
#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq, Serialize, Deserialize, Ord, PartialOrd)]
pub enum FontRenderMode {
    Mono = 0,
    Alpha,
    Subpixel,
}

#[repr(u32)]
#[derive(Copy, Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize, Ord, PartialOrd)]
pub enum SubpixelDirection {
    None = 0,
    Horizontal,
    Vertical,
}

impl FontRenderMode {
    // Skia quantizes subpixel offets into 1/4 increments.
    // Given the absolute position, return the quantized increment
    fn subpixel_quantize_offset(&self, pos: f32) -> SubpixelOffset {
        // Following the conventions of Gecko and Skia, we want
        // to quantize the subpixel position, such that abs(pos) gives:
        // [0.0, 0.125) -> Zero
        // [0.125, 0.375) -> Quarter
        // [0.375, 0.625) -> Half
        // [0.625, 0.875) -> ThreeQuarters,
        // [0.875, 1.0) -> Zero
        // The unit tests below check for this.
        let apos = ((pos - pos.floor()) * 8.0) as i32;

        match apos {
            0 | 7 => SubpixelOffset::Zero,
            1...2 => SubpixelOffset::Quarter,
            3...4 => SubpixelOffset::Half,
            5...6 => SubpixelOffset::ThreeQuarters,
            _ => unreachable!("bug: unexpected quantized result"),
        }
    }

    // Combine two font render modes such that the lesser amount of AA limits the AA of the result.
    pub fn limit_by(self, other: FontRenderMode) -> FontRenderMode {
        match (self, other) {
            (FontRenderMode::Subpixel, _) | (_, FontRenderMode::Mono) => other,
            _ => self,
        }
    }
}

#[repr(u8)]
#[derive(Hash, Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd, Serialize, Deserialize)]
pub enum SubpixelOffset {
    Zero = 0,
    Quarter = 1,
    Half = 2,
    ThreeQuarters = 3,
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

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialOrd, Deserialize, Serialize)]
pub struct FontVariation {
    pub tag: u32,
    pub value: f32,
}

impl Ord for FontVariation {
    fn cmp(&self, other: &FontVariation) -> Ordering {
        self.tag.cmp(&other.tag)
            .then(self.value._to_bits().cmp(&other.value._to_bits()))
    }
}

impl PartialEq for FontVariation {
    fn eq(&self, other: &FontVariation) -> bool {
        self.tag == other.tag &&
        self.value._to_bits() == other.value._to_bits()
    }
}

impl Eq for FontVariation {}

impl Hash for FontVariation {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.tag.hash(state);
        self.value._to_bits().hash(state);
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct GlyphOptions {
    pub render_mode: FontRenderMode,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct FontInstanceOptions {
    pub render_mode: Option<FontRenderMode>,
    pub synthetic_italics: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct FontInstancePlatformOptions {
    // These are currently only used on windows for dwrite fonts.
    pub use_embedded_bitmap: bool,
    pub force_gdi_rendering: bool,
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize, Ord, PartialOrd)]
pub struct FontInstance {
    pub font_key: FontKey,
    // The font size is in *device* pixels, not logical pixels.
    // It is stored as an Au since we need sub-pixel sizes, but
    // can't store as a f32 due to use of this type as a hash key.
    // TODO(gw): Perhaps consider having LogicalAu and DeviceAu
    //           or something similar to that.
    pub size: Au,
    pub color: ColorU,
    pub render_mode: FontRenderMode,
    pub subpx_dir: SubpixelDirection,
    pub platform_options: Option<FontInstancePlatformOptions>,
    pub variations: Vec<FontVariation>,
    pub synthetic_italics: bool,
}

impl FontInstance {
    pub fn new(
        font_key: FontKey,
        size: Au,
        mut color: ColorF,
        render_mode: FontRenderMode,
        subpx_dir: SubpixelDirection,
        platform_options: Option<FontInstancePlatformOptions>,
        variations: Vec<FontVariation>,
        synthetic_italics: bool,
    ) -> FontInstance {
        // In alpha/mono mode, the color of the font is irrelevant.
        // Forcing it to black in those cases saves rasterizing glyphs
        // of different colors when not needed.
        if render_mode != FontRenderMode::Subpixel {
            color = ColorF::new(0.0, 0.0, 0.0, 1.0);
        }

        FontInstance {
            font_key,
            size,
            color: color.into(),
            render_mode,
            subpx_dir,
            platform_options,
            variations,
            synthetic_italics,
        }
    }

    pub fn get_subpx_offset(&self, glyph: &GlyphKey) -> (f64, f64) {
        match self.subpx_dir {
            SubpixelDirection::None => (0.0, 0.0),
            SubpixelDirection::Horizontal => (glyph.subpixel_offset.into(), 0.0),
            SubpixelDirection::Vertical => (0.0, glyph.subpixel_offset.into()),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize, Ord, PartialOrd)]
pub struct FontInstanceKey(pub IdNamespace, pub u32);

impl FontInstanceKey {
    pub fn new(namespace: IdNamespace, key: u32) -> FontInstanceKey {
        FontInstanceKey(namespace, key)
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize, Ord, PartialOrd)]
pub struct GlyphKey {
    pub index: u32,
    pub subpixel_offset: SubpixelOffset,
}

impl GlyphKey {
    pub fn new(
        index: u32,
        point: LayoutPoint,
        render_mode: FontRenderMode,
        subpx_dir: SubpixelDirection,
    ) -> GlyphKey {
        let pos = match subpx_dir {
            SubpixelDirection::None => 0.0,
            SubpixelDirection::Horizontal => point.x,
            SubpixelDirection::Vertical => point.y,
        };

        GlyphKey {
            index,
            subpixel_offset: render_mode.subpixel_quantize_offset(pos),
        }
    }
}

pub type GlyphIndex = u32;

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GlyphInstance {
    pub index: GlyphIndex,
    pub point: LayoutPoint,
}

#[cfg(test)]
mod test {
    use super::{FontRenderMode, SubpixelOffset};

    #[test]
    fn test_subpx_quantize() {
        let rm = FontRenderMode::Subpixel;

        assert_eq!(rm.subpixel_quantize_offset(0.0), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(-0.0), SubpixelOffset::Zero);

        assert_eq!(rm.subpixel_quantize_offset(0.1), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.01), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.05), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.12), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.124), SubpixelOffset::Zero);

        assert_eq!(rm.subpixel_quantize_offset(0.125), SubpixelOffset::Quarter);
        assert_eq!(rm.subpixel_quantize_offset(0.2), SubpixelOffset::Quarter);
        assert_eq!(rm.subpixel_quantize_offset(0.25), SubpixelOffset::Quarter);
        assert_eq!(rm.subpixel_quantize_offset(0.33), SubpixelOffset::Quarter);
        assert_eq!(rm.subpixel_quantize_offset(0.374), SubpixelOffset::Quarter);

        assert_eq!(rm.subpixel_quantize_offset(0.375), SubpixelOffset::Half);
        assert_eq!(rm.subpixel_quantize_offset(0.4), SubpixelOffset::Half);
        assert_eq!(rm.subpixel_quantize_offset(0.5), SubpixelOffset::Half);
        assert_eq!(rm.subpixel_quantize_offset(0.58), SubpixelOffset::Half);
        assert_eq!(rm.subpixel_quantize_offset(0.624), SubpixelOffset::Half);

        assert_eq!(rm.subpixel_quantize_offset(0.625), SubpixelOffset::ThreeQuarters);
        assert_eq!(rm.subpixel_quantize_offset(0.67), SubpixelOffset::ThreeQuarters);
        assert_eq!(rm.subpixel_quantize_offset(0.7), SubpixelOffset::ThreeQuarters);
        assert_eq!(rm.subpixel_quantize_offset(0.78), SubpixelOffset::ThreeQuarters);
        assert_eq!(rm.subpixel_quantize_offset(0.874), SubpixelOffset::ThreeQuarters);

        assert_eq!(rm.subpixel_quantize_offset(0.875), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.89), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.91), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.967), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(0.999), SubpixelOffset::Zero);

        assert_eq!(rm.subpixel_quantize_offset(-1.0), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(1.0), SubpixelOffset::Zero);
        assert_eq!(rm.subpixel_quantize_offset(1.5), SubpixelOffset::Half);
        assert_eq!(rm.subpixel_quantize_offset(-1.625), SubpixelOffset::Half);
        assert_eq!(rm.subpixel_quantize_offset(-4.33), SubpixelOffset::ThreeQuarters);

    }
}

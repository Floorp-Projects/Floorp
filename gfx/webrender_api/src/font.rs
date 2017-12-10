/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use {ColorU, IdNamespace, LayoutPoint};
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

#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[derive(Clone, Serialize, Deserialize)]
pub struct NativeFontHandle {
    pub pathname: String,
    pub index: u32,
}

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

impl SubpixelDirection {
    // Limit the subpixel direction to what is supported by the render mode.
    pub fn limit_by(self, render_mode: FontRenderMode) -> SubpixelDirection {
        match render_mode {
            FontRenderMode::Mono => SubpixelDirection::None,
            FontRenderMode::Alpha | FontRenderMode::Subpixel => self,
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
            .then(self.value.to_bits().cmp(&other.value.to_bits()))
    }
}

impl PartialEq for FontVariation {
    fn eq(&self, other: &FontVariation) -> bool {
        self.tag == other.tag &&
        self.value.to_bits() == other.value.to_bits()
    }
}

impl Eq for FontVariation {}

impl Hash for FontVariation {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.tag.hash(state);
        self.value.to_bits().hash(state);
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct GlyphOptions {
    pub render_mode: FontRenderMode,
}

bitflags! {
    #[repr(C)]
    #[derive(Deserialize, Serialize)]
    pub struct FontInstanceFlags: u32 {
        // Common flags
        const SYNTHETIC_ITALICS = 1 << 0;
        const SYNTHETIC_BOLD    = 1 << 1;
        const EMBEDDED_BITMAPS  = 1 << 2;
        const SUBPIXEL_BGR      = 1 << 3;

        // Windows flags
        const FORCE_GDI         = 1 << 16;

        // Mac flags
        const FONT_SMOOTHING    = 1 << 16;

        // FreeType flags
        const FORCE_AUTOHINT    = 1 << 16;
        const NO_AUTOHINT       = 1 << 17;
        const VERTICAL_LAYOUT   = 1 << 18;
    }
}

impl Default for FontInstanceFlags {
    #[cfg(target_os = "windows")]
    fn default() -> FontInstanceFlags {
        FontInstanceFlags::empty()
    }

    #[cfg(target_os = "macos")]
    fn default() -> FontInstanceFlags {
        FontInstanceFlags::FONT_SMOOTHING
    }

    #[cfg(not(any(target_os = "macos", target_os = "windows")))]
    fn default() -> FontInstanceFlags {
        FontInstanceFlags::empty()
    }
}


#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct FontInstanceOptions {
    pub render_mode: FontRenderMode,
    pub subpx_dir: SubpixelDirection,
    pub flags: FontInstanceFlags,
    /// When bg_color.a is != 0 and render_mode is FontRenderMode::Subpixel,
    /// the text will be rendered with bg_color.r/g/b as an opaque estimated
    /// background color.
    pub bg_color: ColorU,
}

impl Default for FontInstanceOptions {
    fn default() -> FontInstanceOptions {
        FontInstanceOptions {
            render_mode: FontRenderMode::Subpixel,
            subpx_dir: SubpixelDirection::Horizontal,
            flags: Default::default(),
            bg_color: ColorU::new(0, 0, 0, 0),
        }
    }
}

#[cfg(target_os = "windows")]
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct FontInstancePlatformOptions {
    pub unused: u32,
}

#[cfg(target_os = "windows")]
impl Default for FontInstancePlatformOptions {
    fn default() -> FontInstancePlatformOptions {
        FontInstancePlatformOptions {
            unused: 0,
        }
    }
}

#[cfg(target_os = "macos")]
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct FontInstancePlatformOptions {
    pub unused: u32,
}

#[cfg(target_os = "macos")]
impl Default for FontInstancePlatformOptions {
    fn default() -> FontInstancePlatformOptions {
        FontInstancePlatformOptions {
            unused: 0,
        }
    }
}

#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[repr(u8)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, PartialOrd, Ord, Serialize)]
pub enum FontLCDFilter {
    None,
    Default,
    Light,
    Legacy,
}

#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[repr(u8)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, PartialOrd, Ord, Serialize)]
pub enum FontHinting {
    None,
    Mono,
    Light,
    Normal,
    LCD,
}

#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct FontInstancePlatformOptions {
    pub lcd_filter: FontLCDFilter,
    pub hinting: FontHinting,
}

#[cfg(not(any(target_os = "macos", target_os = "windows")))]
impl Default for FontInstancePlatformOptions {
    fn default() -> FontInstancePlatformOptions {
        FontInstancePlatformOptions {
            lcd_filter: FontLCDFilter::Default,
            hinting: FontHinting::LCD,
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

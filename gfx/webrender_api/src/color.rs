/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ColorF {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}
known_heap_size!(0, ColorF);

impl ColorF {
    pub fn premultiplied(&self) -> ColorF {
        ColorF {
            r: self.r * self.a,
            g: self.g * self.a,
            b: self.b * self.a,
            a: self.a,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Hash, Eq, Debug, Deserialize, PartialEq, PartialOrd, Ord, Serialize)]
pub struct ColorU {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl From<ColorF> for ColorU {
    fn from(color: ColorF) -> ColorU {
        ColorU {
            r: ColorU::round_to_int(color.r),
            g: ColorU::round_to_int(color.g),
            b: ColorU::round_to_int(color.b),
            a: ColorU::round_to_int(color.a),
        }
    }
}

impl Into<ColorF> for ColorU {
    fn into(self) -> ColorF {
        ColorF {
            r: self.r as f32 / 255.0,
            g: self.g as f32 / 255.0,
            b: self.b as f32 / 255.0,
            a: self.a as f32 / 255.0,
        }
    }
}

impl ColorU {
    fn round_to_int(x: f32) -> u8 {
        debug_assert!((0.0 <= x) && (x <= 1.0));
        let f = (255.0 * x) + 0.5;
        let val = f.floor();
        debug_assert!(val <= 255.0);
        val as u8
    }

    pub fn new(r: u8, g: u8, b: u8, a: u8) -> ColorU {
        ColorU {
            r,
            g,
            b,
            a,
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, PremultipliedColorF};

#[repr(u32)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum PatternKind {
    ColorOrTexture = 0,

    Mask = 1,
    // When adding patterns, don't forget to update the NUM_PATTERNS constant.
}

pub const NUM_PATTERNS: u32 = 2;

impl PatternKind {
    pub fn from_u32(val: u32) -> Self {
        assert!(val < NUM_PATTERNS);
        unsafe { std::mem::transmute(val) }
    }
}

/// A 32bit payload used as input for the pattern-specific logic in the shader.
///
/// Patterns typically use it as a GpuBuffer offset to fetch their data.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct PatternShaderInput(pub i32, pub i32);

impl Default for PatternShaderInput {
    fn default() -> Self {
        PatternShaderInput(0, 0)
    }
}

#[derive(Copy, Clone, Debug)]
pub struct Pattern {
    pub kind: PatternKind,
    pub shader_input: PatternShaderInput,
    pub base_color: PremultipliedColorF,
    pub is_opaque: bool,
}

impl Pattern {
    pub fn color(color: ColorF) -> Self {
        Pattern {
            kind: PatternKind::ColorOrTexture,
            shader_input: PatternShaderInput::default(),
            base_color: color.premultiplied(),
            is_opaque: color.a >= 1.0,
        }
    }

    pub fn clear() -> Self {
        // Opaque black with operator dest out
        Pattern {
            kind: PatternKind::ColorOrTexture,
            shader_input: PatternShaderInput::default(),
            base_color: PremultipliedColorF::BLACK,
            is_opaque: false,
        }
    }
}

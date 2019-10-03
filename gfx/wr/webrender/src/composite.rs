/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::ColorF;
use api::units::DeviceRect;
use crate::gpu_types::{ZBufferId, ZBufferIdGenerator};
use crate::internal_types::TextureSource;

/*
 Types and definitions related to compositing picture cache tiles
 and/or OS compositor integration.
 */

/// Describes the source surface information for a tile to be composited
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum CompositeTileSurface {
    Texture {
        texture_id: TextureSource,
        texture_layer: i32,
    },
    Color {
        color: ColorF,
    },
}

/// Describes the geometry and surface of a tile to be composited
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CompositeTile {
    pub surface: CompositeTileSurface,
    pub rect: DeviceRect,
    pub clip_rect: DeviceRect,
    pub z_id: ZBufferId,
}

/// The list of tiles to be drawn this frame
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CompositeConfig {
    pub opaque_tiles: Vec<CompositeTile>,
    pub alpha_tiles: Vec<CompositeTile>,
    pub z_generator: ZBufferIdGenerator,
}

impl CompositeConfig {
    pub fn new() -> Self {
        CompositeConfig {
            opaque_tiles: Vec::new(),
            alpha_tiles: Vec::new(),
            z_generator: ZBufferIdGenerator::new(0),
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::DebugCommand;
use device::TextureFilter;
use fxhash::FxHasher;
use profiler::BackendProfileCounters;
use std::collections::{HashMap, HashSet};
use std::f32;
use std::hash::BuildHasherDefault;
use std::{i32, usize};
use std::path::PathBuf;
use std::sync::Arc;
use tiling;
use renderer::BlendMode;
use api::{ClipId, DevicePoint, DeviceUintRect, DocumentId, Epoch};
use api::{ExternalImageData, ExternalImageId};
use api::{ImageFormat, PipelineId};

pub type FastHashMap<K, V> = HashMap<K, V, BuildHasherDefault<FxHasher>>;
pub type FastHashSet<K> = HashSet<K, BuildHasherDefault<FxHasher>>;

// An ID for a texture that is owned by the
// texture cache module. This can include atlases
// or standalone textures allocated via the
// texture cache (e.g. if an image is too large
// to be added to an atlas). The texture cache
// manages the allocation and freeing of these
// IDs, and the rendering thread maintains a
// map from cache texture ID to native texture.

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct CacheTextureId(pub usize);

// Represents the source for a texture.
// These are passed from throughout the
// pipeline until they reach the rendering
// thread, where they are resolved to a
// native texture ID.

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum SourceTexture {
    Invalid,
    TextureCache(CacheTextureId),
    External(ExternalImageData),
    CacheA8,
    CacheRGBA8,
}

pub const ORTHO_NEAR_PLANE: f32 = -1000000.0;
pub const ORTHO_FAR_PLANE: f32 = 1000000.0;

/// Optional textures that can be used as a source in the shaders.
/// Textures that are not used by the batch are equal to TextureId::invalid().
#[derive(Copy, Clone, Debug)]
pub struct BatchTextures {
    pub colors: [SourceTexture; 3],
}

impl BatchTextures {
    pub fn no_texture() -> Self {
        BatchTextures {
            colors: [SourceTexture::Invalid; 3],
        }
    }

    pub fn render_target_cache() -> Self {
        BatchTextures {
            colors: [
                SourceTexture::CacheRGBA8,
                SourceTexture::Invalid,
                SourceTexture::Invalid,
            ]
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum RenderTargetMode {
    None,
    RenderTarget,
}

#[derive(Debug)]
pub enum TextureUpdateSource {
    External { id: ExternalImageId, channel_index: u8 },
    Bytes { data: Arc<Vec<u8>> },
}

#[derive(Debug)]
pub enum TextureUpdateOp {
    Create {
        width: u32,
        height: u32,
        format: ImageFormat,
        filter: TextureFilter,
        mode: RenderTargetMode,
        layer_count: i32,
    },
    Update {
        rect: DeviceUintRect,
        stride: Option<u32>,
        offset: u32,
        layer_index: i32,
        source: TextureUpdateSource,
    },
    Free,
}

#[derive(Debug)]
pub struct TextureUpdate {
    pub id: CacheTextureId,
    pub op: TextureUpdateOp,
}

pub struct TextureUpdateList {
    pub updates: Vec<TextureUpdate>,
}

impl TextureUpdateList {
    pub fn new() -> TextureUpdateList {
        TextureUpdateList {
            updates: Vec::new(),
        }
    }

    #[inline]
    pub fn push(&mut self, update: TextureUpdate) {
        self.updates.push(update);
    }
}

/// Mostly wraps a tiling::Frame, adding a bit of extra information.
pub struct RendererFrame {
    /// The last rendered epoch for each pipeline present in the frame.
    /// This information is used to know if a certain transformation on the layout has
    /// been rendered, which is necessary for reftests.
    pub pipeline_epoch_map: FastHashMap<PipelineId, Epoch>,
    /// The layers that are currently affected by the over-scrolling animation.
    pub layers_bouncing_back: FastHashSet<ClipId>,

    pub frame: Option<tiling::Frame>,
}

impl RendererFrame {
    pub fn new(pipeline_epoch_map: FastHashMap<PipelineId, Epoch>,
               layers_bouncing_back: FastHashSet<ClipId>,
               frame: Option<tiling::Frame>)
               -> RendererFrame {
        RendererFrame {
            pipeline_epoch_map,
            layers_bouncing_back,
            frame,
        }
    }
}

pub enum ResultMsg {
    DebugCommand(DebugCommand),
    RefreshShader(PathBuf),
    NewFrame(DocumentId, RendererFrame, TextureUpdateList, BackendProfileCounters),
    UpdateResources { updates: TextureUpdateList, cancel_rendering: bool },
}

#[derive(Debug, Clone, Copy, Eq, Hash, PartialEq)]
pub struct StackingContextIndex(pub usize);

#[derive(Clone, Copy, Debug)]
pub struct UvRect {
    pub uv0: DevicePoint,
    pub uv1: DevicePoint,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum HardwareCompositeOp {
    PremultipliedAlpha,
}

impl HardwareCompositeOp {
    pub fn to_blend_mode(&self) -> BlendMode {
        match *self {
            HardwareCompositeOp::PremultipliedAlpha => BlendMode::PremultipliedAlpha,
        }
    }
}

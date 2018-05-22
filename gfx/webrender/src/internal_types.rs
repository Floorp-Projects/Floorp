/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DebugCommand, DeviceUintRect, DocumentId, ExternalImageData, ExternalImageId};
use api::ImageFormat;
use device::TextureFilter;
use renderer::PipelineInfo;
use gpu_cache::GpuCacheUpdateList;
use fxhash::FxHasher;
use profiler::BackendProfileCounters;
use std::{usize, i32};
use std::collections::{HashMap, HashSet};
use std::f32;
use std::hash::BuildHasherDefault;
use std::path::PathBuf;
use std::sync::Arc;

#[cfg(feature = "capture")]
use capture::{CaptureConfig, ExternalCaptureImage};
#[cfg(feature = "replay")]
use capture::PlainExternalImage;
use tiling;

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
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheTextureId(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SavedTargetIndex(pub usize);

impl SavedTargetIndex {
    pub const PENDING: Self = SavedTargetIndex(!0);
}

// Represents the source for a texture.
// These are passed from throughout the
// pipeline until they reach the rendering
// thread, where they are resolved to a
// native texture ID.

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum SourceTexture {
    Invalid,
    TextureCache(CacheTextureId),
    External(ExternalImageData),
    CacheA8,
    CacheRGBA8,
    RenderTaskCache(SavedTargetIndex),
}

pub const ORTHO_NEAR_PLANE: f32 = -1000000.0;
pub const ORTHO_FAR_PLANE: f32 = 1000000.0;

#[derive(Copy, Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTargetInfo {
    pub has_depth: bool,
}

#[derive(Debug)]
pub enum TextureUpdateSource {
    External {
        id: ExternalImageId,
        channel_index: u8,
    },
    Bytes { data: Arc<Vec<u8>> },
}

#[derive(Debug)]
pub enum TextureUpdateOp {
    Create {
        width: u32,
        height: u32,
        format: ImageFormat,
        filter: TextureFilter,
        render_target: Option<RenderTargetInfo>,
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

#[derive(Default)]
pub struct TextureUpdateList {
    pub updates: Vec<TextureUpdate>,
}

impl TextureUpdateList {
    pub fn new() -> Self {
        TextureUpdateList {
            updates: Vec::new(),
        }
    }

    #[inline]
    pub fn push(&mut self, update: TextureUpdate) {
        self.updates.push(update);
    }
}

/// Wraps a tiling::Frame, but conceptually could hold more information
pub struct RenderedDocument {
    pub frame: tiling::Frame,
}

impl RenderedDocument {
    pub fn new(
        frame: tiling::Frame,
    ) -> Self {
        RenderedDocument {
            frame,
        }
    }
}

pub enum DebugOutput {
    FetchDocuments(String),
    FetchClipScrollTree(String),
    #[cfg(feature = "capture")]
    SaveCapture(CaptureConfig, Vec<ExternalCaptureImage>),
    #[cfg(feature = "replay")]
    LoadCapture(PathBuf, Vec<PlainExternalImage>),
}

pub enum ResultMsg {
    DebugCommand(DebugCommand),
    DebugOutput(DebugOutput),
    RefreshShader(PathBuf),
    UpdateGpuCache(GpuCacheUpdateList),
    UpdateResources {
        updates: TextureUpdateList,
        cancel_rendering: bool,
    },
    PublishPipelineInfo(PipelineInfo),
    PublishDocument(
        DocumentId,
        RenderedDocument,
        TextureUpdateList,
        BackendProfileCounters,
    ),
}

#[derive(Clone, Debug)]
pub struct ResourceCacheError {
    description: String,
}

impl ResourceCacheError {
    pub fn new(description: String) -> ResourceCacheError {
        ResourceCacheError {
            description,
        }
    }
}

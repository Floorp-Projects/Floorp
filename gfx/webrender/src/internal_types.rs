/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ClipId, DevicePoint, DeviceUintRect, DocumentId, Epoch};
use api::{ExternalImageData, ExternalImageId};
use api::{ImageFormat, PipelineId};
#[cfg(feature = "capture")]
use api::ImageDescriptor;
use api::DebugCommand;
use device::TextureFilter;
use fxhash::FxHasher;
use profiler::BackendProfileCounters;
use std::{usize, i32};
use std::collections::{HashMap, HashSet};
use std::f32;
use std::hash::BuildHasherDefault;
use std::path::PathBuf;
use std::sync::Arc;
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
pub struct CacheTextureId(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct RenderPassIndex(pub usize);

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
    // XXX Remove this once RenderTaskCacheA8 is used.
    #[allow(dead_code)]
    RenderTaskCacheA8(RenderPassIndex),
    RenderTaskCacheRGBA8(RenderPassIndex),
}

pub const ORTHO_NEAR_PLANE: f32 = -1000000.0;
pub const ORTHO_FAR_PLANE: f32 = 1000000.0;

#[derive(Copy, Clone, Debug, PartialEq)]
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
pub struct RenderedDocument {
    /// The last rendered epoch for each pipeline present in the frame.
    /// This information is used to know if a certain transformation on the layout has
    /// been rendered, which is necessary for reftests.
    pub pipeline_epoch_map: FastHashMap<PipelineId, Epoch>,
    /// The layers that are currently affected by the over-scrolling animation.
    pub layers_bouncing_back: FastHashSet<ClipId>,

    pub frame: tiling::Frame,
}

impl RenderedDocument {
    pub fn new(
        pipeline_epoch_map: FastHashMap<PipelineId, Epoch>,
        layers_bouncing_back: FastHashSet<ClipId>,
        frame: tiling::Frame,
    ) -> Self {
        RenderedDocument {
            pipeline_epoch_map,
            layers_bouncing_back,
            frame,
        }
    }
}

#[cfg(feature = "capture")]
pub struct ExternalCaptureImage {
    pub short_path: String,
    pub descriptor: ImageDescriptor,
    pub external: ExternalImageData,
}

pub enum DebugOutput {
    FetchDocuments(String),
    FetchClipScrollTree(String),
    #[cfg(feature = "capture")]
    SaveCapture(PathBuf, Vec<ExternalCaptureImage>),
    #[cfg(feature = "capture")]
    LoadCapture,
}

pub enum ResultMsg {
    DebugCommand(DebugCommand),
    DebugOutput(DebugOutput),
    RefreshShader(PathBuf),
    PublishDocument(
        DocumentId,
        RenderedDocument,
        TextureUpdateList,
        BackendProfileCounters,
    ),
    UpdateResources {
        updates: TextureUpdateList,
        cancel_rendering: bool,
    },
}

#[derive(Clone, Copy, Debug)]
pub struct UvRect {
    pub uv0: DevicePoint,
    pub uv1: DevicePoint,
}

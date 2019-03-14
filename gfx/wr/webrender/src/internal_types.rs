/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DebugCommand, DocumentId, ExternalImageData, ExternalImageId};
use api::{ImageFormat, NotificationRequest};
use api::units::*;
use device::TextureFilter;
use renderer::PipelineInfo;
use gpu_cache::GpuCacheUpdateList;
use fxhash::FxHasher;
use plane_split::BspSplitter;
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

/// A concrete plane splitter type used in WebRender.
pub type PlaneSplitter = BspSplitter<f64, WorldPixel>;

/// An ID for a texture that is owned by the `texture_cache` module.
///
/// This can include atlases or standalone textures allocated via the texture
/// cache (e.g.  if an image is too large to be added to an atlas). The texture
/// cache manages the allocation and freeing of these IDs, and the rendering
/// thread maintains a map from cache texture ID to native texture.
///
/// We never reuse IDs, so we use a u64 here to be safe.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheTextureId(pub u64);

/// Canonical type for texture layer indices.
///
/// WebRender is currently not very consistent about layer index types. Some
/// places use i32 (since that's the type used in various OpenGL APIs), some
/// places use u32 (since having it be signed is non-sensical, but the
/// underlying graphics APIs generally operate on 32-bit integers) and some
/// places use usize (since that's most natural in Rust).
///
/// Going forward, we aim to us usize throughout the codebase, since that allows
/// operations like indexing without a cast, and convert to the required type in
/// the device module when making calls into the platform layer.
pub type LayerIndex = usize;

/// Identifies a render pass target that is persisted until the end of the frame.
///
/// By default, only the targets of the immediately-preceding pass are bound as
/// inputs to the next pass. However, tasks can opt into having their target
/// preserved in a list until the end of the frame, and this type specifies the
/// index in that list.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SavedTargetIndex(pub usize);

impl SavedTargetIndex {
    pub const PENDING: Self = SavedTargetIndex(!0);
}

/// Identifies the source of an input texture to a shader.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TextureSource {
    /// Equivalent to `None`, allowing us to avoid using `Option`s everywhere.
    Invalid,
    /// An entry in the texture cache.
    TextureCache(CacheTextureId),
    /// An external image texture, mananged by the embedding.
    External(ExternalImageData),
    /// The alpha target of the immediately-preceding pass.
    PrevPassAlpha,
    /// The color target of the immediately-preceding pass.
    PrevPassColor,
    /// A render target from an earlier pass. Unlike the immediately-preceding
    /// passes, these are not made available automatically, but are instead
    /// opt-in by the `RenderTask` (see `mark_for_saving()`).
    RenderTaskCache(SavedTargetIndex),
}

pub const ORTHO_NEAR_PLANE: f32 = -100000.0;
pub const ORTHO_FAR_PLANE: f32 = 100000.0;

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
    /// Clears the target area, rather than uploading any pixels. Used when the
    /// texture cache debug display is active.
    DebugClear,
}

/// Command to allocate, reallocate, or free a texture for the texture cache.
#[derive(Debug)]
pub struct TextureCacheAllocation {
    /// The virtual ID (i.e. distinct from device ID) of the texture.
    pub id: CacheTextureId,
    /// Details corresponding to the operation in question.
    pub kind: TextureCacheAllocationKind,
}

/// Information used when allocating / reallocating.
#[derive(Debug)]
pub struct TextureCacheAllocInfo {
    pub width: i32,
    pub height: i32,
    pub layer_count: i32,
    pub format: ImageFormat,
    pub filter: TextureFilter,
    /// Indicates whether this corresponds to one of the shared texture caches.
    pub is_shared_cache: bool,
}

/// Sub-operation-specific information for allocation operations.
#[derive(Debug)]
pub enum TextureCacheAllocationKind {
    /// Performs an initial texture allocation.
    Alloc(TextureCacheAllocInfo),
    /// Reallocates the texture. The existing live texture with the same id
    /// will be deallocated and its contents blitted over. The new size must
    /// be greater than the old size.
    Realloc(TextureCacheAllocInfo),
    /// Frees the texture and the corresponding cache ID.
    Free,
}

/// Command to update the contents of the texture cache.
#[derive(Debug)]
pub struct TextureCacheUpdate {
    pub id: CacheTextureId,
    pub rect: DeviceIntRect,
    pub stride: Option<i32>,
    pub offset: i32,
    pub layer_index: i32,
    pub source: TextureUpdateSource,
}

/// Atomic set of commands to manipulate the texture cache, generated on the
/// RenderBackend thread and executed on the Renderer thread.
///
/// The list of allocation operations is processed before the updates. This is
/// important to allow coalescing of certain allocation operations.
#[derive(Default)]
pub struct TextureUpdateList {
    /// Commands to alloc/realloc/free the textures. Processed first.
    pub allocations: Vec<TextureCacheAllocation>,
    /// Commands to update the contents of the textures. Processed second.
    pub updates: Vec<TextureCacheUpdate>,
}

impl TextureUpdateList {
    /// Mints a new `TextureUpdateList`.
    pub fn new() -> Self {
        TextureUpdateList {
            allocations: Vec::new(),
            updates: Vec::new(),
        }
    }

    /// Pushes an update operation onto the list.
    #[inline]
    pub fn push_update(&mut self, update: TextureCacheUpdate) {
        self.updates.push(update);
    }

    /// Sends a command to the Renderer to clear the portion of the shared region
    /// we just freed. Used when the texture cache debugger is enabled.
    #[cold]
    pub fn push_debug_clear(
        &mut self,
        id: CacheTextureId,
        origin: DeviceIntPoint,
        width: i32,
        height: i32,
        layer_index: usize
    ) {
        let size = DeviceIntSize::new(width, height);
        let rect = DeviceIntRect::new(origin, size);
        self.push_update(TextureCacheUpdate {
            id,
            rect,
            source: TextureUpdateSource::DebugClear,
            stride: None,
            offset: 0,
            layer_index: layer_index as i32,
        });
    }


    /// Pushes an allocation operation onto the list.
    pub fn push_alloc(&mut self, id: CacheTextureId, info: TextureCacheAllocInfo) {
        debug_assert!(!self.allocations.iter().any(|x| x.id == id));
        self.allocations.push(TextureCacheAllocation {
            id,
            kind: TextureCacheAllocationKind::Alloc(info),
        });
    }

    /// Pushes a reallocation operation onto the list, potentially coalescing
    /// with previous operations.
    pub fn push_realloc(&mut self, id: CacheTextureId, info: TextureCacheAllocInfo) {
        self.debug_assert_coalesced(id);

        // Coallesce this realloc into a previous alloc or realloc, if available.
        if let Some(cur) = self.allocations.iter_mut().find(|x| x.id == id) {
            match cur.kind {
                TextureCacheAllocationKind::Alloc(ref mut i) => *i = info,
                TextureCacheAllocationKind::Realloc(ref mut i) => *i = info,
                TextureCacheAllocationKind::Free => panic!("Reallocating freed texture"),
            }

            return;
        }

        self.allocations.push(TextureCacheAllocation {
            id,
            kind: TextureCacheAllocationKind::Realloc(info),
        });
    }

    /// Pushes a free operation onto the list, potentially coalescing with
    /// previous operations.
    pub fn push_free(&mut self, id: CacheTextureId) {
        self.debug_assert_coalesced(id);

        // Drop any unapplied updates to the to-be-freed texture.
        self.updates.retain(|x| x.id != id);

        // Drop any allocations for it as well. If we happen to be allocating and
        // freeing in the same batch, we can collapse them to a no-op.
        let idx = self.allocations.iter().position(|x| x.id == id);
        let removed_kind = idx.map(|i| self.allocations.remove(i).kind);
        match removed_kind {
            Some(TextureCacheAllocationKind::Alloc(..)) => { /* no-op! */ },
            Some(TextureCacheAllocationKind::Free) => panic!("Double free"),
            Some(TextureCacheAllocationKind::Realloc(..)) | None => {
                self.allocations.push(TextureCacheAllocation {
                    id,
                    kind: TextureCacheAllocationKind::Free,
                });
            }
        };
    }

    fn debug_assert_coalesced(&self, id: CacheTextureId) {
        debug_assert!(
            self.allocations.iter().filter(|x| x.id == id).count() <= 1,
            "Allocations should have been coalesced",
        );
    }
}

/// Wraps a tiling::Frame, but conceptually could hold more information
pub struct RenderedDocument {
    pub frame: tiling::Frame,
    pub is_new_scene: bool,
}

pub enum DebugOutput {
    FetchDocuments(String),
    FetchClipScrollTree(String),
    #[cfg(feature = "capture")]
    SaveCapture(CaptureConfig, Vec<ExternalCaptureImage>),
    #[cfg(feature = "replay")]
    LoadCapture(PathBuf, Vec<PlainExternalImage>),
}

#[allow(dead_code)]
pub enum ResultMsg {
    DebugCommand(DebugCommand),
    DebugOutput(DebugOutput),
    RefreshShader(PathBuf),
    UpdateGpuCache(GpuCacheUpdateList),
    UpdateResources {
        updates: TextureUpdateList,
        memory_pressure: bool,
    },
    PublishPipelineInfo(PipelineInfo),
    PublishDocument(
        DocumentId,
        RenderedDocument,
        TextureUpdateList,
        BackendProfileCounters,
    ),
    AppendNotificationRequests(Vec<NotificationRequest>),
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

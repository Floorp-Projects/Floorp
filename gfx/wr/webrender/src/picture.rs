/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A picture represents a dynamically rendered image.
//!
//! # Overview
//!
//! Pictures consists of:
//!
//! - A number of primitives that are drawn onto the picture.
//! - A composite operation describing how to composite this
//!   picture into its parent.
//! - A configuration describing how to draw the primitives on
//!   this picture (e.g. in screen space or local space).
//!
//! The tree of pictures are generated during scene building.
//!
//! Depending on their composite operations pictures can be rendered into
//! intermediate targets or folded into their parent picture.
//!
//! ## Picture caching
//!
//! Pictures can be cached to reduce the amount of rasterization happening per
//! frame.
//!
//! When picture caching is enabled, the scene is cut into a small number of slices,
//! typically:
//!
//! - content slice
//! - UI slice
//! - background UI slice which is hidden by the other two slices most of the time.
//!
//! Each of these slice is made up of fixed-size large tiles of 2048x512 pixels
//! (or 128x128 for the UI slice).
//!
//! Tiles can be either cached rasterized content into a texture or "clear tiles"
//! that contain only a solid color rectangle rendered directly during the composite
//! pass.
//!
//! ## Invalidation
//!
//! Each tile keeps track of the elements that affect it, which can be:
//!
//! - primitives
//! - clips
//! - image keys
//! - opacity bindings
//! - transforms
//!
//! These dependency lists are built each frame and compared to the previous frame to
//! see if the tile changed.
//!
//! The tile's primitive dependency information is organized in a quadtree, each node
//! storing an index buffer of tile primitive dependencies.
//!
//! The union of the invalidated leaves of each quadtree produces a per-tile dirty rect
//! which defines the scissor rect used when replaying the tile's drawing commands and
//! can be used for partial present.
//!
//! ## Display List shape
//!
//! WR will first look for an iframe item in the root stacking context to apply
//! picture caching to. If that's not found, it will apply to the entire root
//! stacking context of the display list. Apart from that, the format of the
//! display list is not important to picture caching. Each time a new scroll root
//! is encountered, a new picture cache slice will be created. If the display
//! list contains more than some arbitrary number of slices (currently 8), the
//! content will all be squashed into a single slice, in order to save GPU memory
//! and compositing performance.

use api::{MixBlendMode, PipelineId, PremultipliedColorF, FilterPrimitiveKind};
use api::{PropertyBinding, PropertyBindingId, FilterPrimitive, FontRenderMode};
use api::{DebugFlags, RasterSpace, ImageKey, ColorF, PrimitiveFlags};
use api::units::*;
use crate::box_shadow::{BLUR_SAMPLE_SCALE};
use crate::clip::{ClipStore, ClipChainInstance, ClipDataHandle, ClipChainId};
use crate::clip_scroll_tree::{ROOT_SPATIAL_NODE_INDEX,
    ClipScrollTree, CoordinateSpaceMapping, SpatialNodeIndex, VisibleFace
};
use crate::composite::{CompositorKind, CompositeState, NativeSurfaceId, NativeTileId};
use crate::debug_colors;
use euclid::{vec3, Point2D, Scale, Size2D, Vector2D, Rect};
use euclid::approxeq::ApproxEq;
use crate::filterdata::SFilterData;
use crate::frame_builder::{FrameVisibilityContext, FrameVisibilityState};
use crate::intern::ItemUid;
use crate::internal_types::{FastHashMap, FastHashSet, PlaneSplitter, Filter, PlaneSplitAnchor, TextureSource};
use crate::frame_builder::{FrameBuildingContext, FrameBuildingState, PictureState, PictureContext};
use crate::gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use crate::gpu_types::UvRectKind;
use plane_split::{Clipper, Polygon, Splitter};
use crate::prim_store::{SpaceMapper, PrimitiveVisibilityMask, PointKey, PrimitiveTemplateKind};
use crate::prim_store::{SpaceSnapper, PictureIndex, PrimitiveInstance, PrimitiveInstanceKind};
use crate::prim_store::{get_raster_rects, PrimitiveScratchBuffer, RectangleKey};
use crate::prim_store::{OpacityBindingStorage, ImageInstanceStorage, OpacityBindingIndex};
use crate::print_tree::{PrintTree, PrintTreePrinter};
use crate::render_backend::DataStores;
use crate::render_task_graph::RenderTaskId;
use crate::render_target::RenderTargetKind;
use crate::render_task::{RenderTask, RenderTaskLocation, BlurTaskCache, ClearMode};
use crate::resource_cache::{ResourceCache, ImageGeneration};
use crate::scene::SceneProperties;
use smallvec::SmallVec;
use std::{mem, u8, marker, u32};
use std::sync::atomic::{AtomicUsize, Ordering};
use crate::texture_cache::TextureCacheHandle;
use crate::util::{TransformedRectKind, MatrixHelpers, MaxRect, scale_factors, VecHelper, RectHelpers};
use crate::filterdata::{FilterDataHandle};
#[cfg(feature = "capture")]
use ron;
#[cfg(feature = "capture")]
use crate::scene_builder_thread::InternerUpdates;
#[cfg(feature = "capture")]
use crate::intern::Update;


#[cfg(feature = "capture")]
use std::fs::File;
#[cfg(feature = "capture")]
use std::io::prelude::*;
#[cfg(feature = "capture")]
use std::path::PathBuf;

/// Specify whether a surface allows subpixel AA text rendering.
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum SubpixelMode {
    /// This surface allows subpixel AA text
    Allow,
    /// Subpixel AA text cannot be drawn on this surface
    Deny,
}

/// A comparable transform matrix, that compares with epsilon checks.
#[derive(Debug, Clone)]
struct MatrixKey {
    m: [f32; 16],
}

impl PartialEq for MatrixKey {
    fn eq(&self, other: &Self) -> bool {
        const EPSILON: f32 = 0.001;

        // TODO(gw): It's possible that we may need to adjust the epsilon
        //           to be tighter on most of the matrix, except the
        //           translation parts?
        for (i, j) in self.m.iter().zip(other.m.iter()) {
            if !i.approx_eq_eps(j, &EPSILON) {
                return false;
            }
        }

        true
    }
}

/// A comparable / hashable version of a coordinate space mapping. Used to determine
/// if a transform dependency for a tile has changed.
#[derive(Debug, PartialEq, Clone)]
enum TransformKey {
    Local,
    ScaleOffset {
        scale_x: f32,
        scale_y: f32,
        offset_x: f32,
        offset_y: f32,
    },
    Transform {
        m: MatrixKey,
    }
}

impl<Src, Dst> From<CoordinateSpaceMapping<Src, Dst>> for TransformKey {
    fn from(transform: CoordinateSpaceMapping<Src, Dst>) -> TransformKey {
        match transform {
            CoordinateSpaceMapping::Local => {
                TransformKey::Local
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                TransformKey::ScaleOffset {
                    scale_x: scale_offset.scale.x,
                    scale_y: scale_offset.scale.y,
                    offset_x: scale_offset.offset.x,
                    offset_y: scale_offset.offset.y,
                }
            }
            CoordinateSpaceMapping::Transform(ref m) => {
                TransformKey::Transform {
                    m: MatrixKey {
                        m: m.to_row_major_array(),
                    },
                }
            }
        }
    }
}

/// Information about a picture that is pushed / popped on the
/// PictureUpdateState during picture traversal pass.
struct PictureInfo {
    /// The spatial node for this picture.
    _spatial_node_index: SpatialNodeIndex,
}

/// Picture-caching state to keep between scenes.
pub struct PictureCacheState {
    /// The tiles retained by this picture cache.
    pub tiles: FastHashMap<TileOffset, Tile>,
    /// State of the spatial nodes from previous frame
    spatial_nodes: FastHashMap<SpatialNodeIndex, SpatialNodeDependency>,
    /// State of opacity bindings from previous frame
    opacity_bindings: FastHashMap<PropertyBindingId, OpacityBindingInfo>,
    /// The current transform of the picture cache root spatial node
    root_transform: TransformKey,
    /// The current tile size in device pixels
    current_tile_size: DeviceIntSize,
    /// Various allocations we want to avoid re-doing.
    allocations: PictureCacheRecycledAllocations,
    /// Currently allocated native compositor surface for this picture cache.
    pub native_surface_id: Option<NativeSurfaceId>,
    /// True if the entire picture cache is opaque.
    is_opaque: bool,
}

pub struct PictureCacheRecycledAllocations {
    old_tiles: FastHashMap<TileOffset, Tile>,
    old_opacity_bindings: FastHashMap<PropertyBindingId, OpacityBindingInfo>,
    compare_cache: FastHashMap<PrimitiveComparisonKey, PrimitiveCompareResult>,
}

/// Stores a list of cached picture tiles that are retained
/// between new scenes.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct RetainedTiles {
    /// The tiles retained between display lists.
    #[cfg_attr(feature = "capture", serde(skip))] //TODO
    pub caches: FastHashMap<usize, PictureCacheState>,
}

impl RetainedTiles {
    pub fn new() -> Self {
        RetainedTiles {
            caches: FastHashMap::default(),
        }
    }

    /// Merge items from one retained tiles into another.
    pub fn merge(&mut self, other: RetainedTiles) {
        assert!(self.caches.is_empty() || other.caches.is_empty());
        if self.caches.is_empty() {
            self.caches = other.caches;
        }
    }
}

/// Unit for tile coordinates.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct TileCoordinate;

// Geometry types for tile coordinates.
pub type TileOffset = Point2D<i32, TileCoordinate>;
pub type TileSize = Size2D<i32, TileCoordinate>;
pub type TileRect = Rect<i32, TileCoordinate>;

/// The size in device pixels of a normal cached tile.
pub const TILE_SIZE_DEFAULT: DeviceIntSize = DeviceIntSize {
    width: 1024,
    height: 512,
    _unit: marker::PhantomData,
};

/// The size in device pixels of a tile for horizontal scroll bars
pub const TILE_SIZE_SCROLLBAR_HORIZONTAL: DeviceIntSize = DeviceIntSize {
    width: 512,
    height: 16,
    _unit: marker::PhantomData,
};

/// The size in device pixels of a tile for vertical scroll bars
pub const TILE_SIZE_SCROLLBAR_VERTICAL: DeviceIntSize = DeviceIntSize {
    width: 16,
    height: 512,
    _unit: marker::PhantomData,
};

const TILE_SIZE_FOR_TESTS: [DeviceIntSize; 6] = [
    DeviceIntSize {
        width: 128,
        height: 128,
        _unit: marker::PhantomData,
    },
    DeviceIntSize {
        width: 256,
        height: 256,
        _unit: marker::PhantomData,
    },
    DeviceIntSize {
        width: 512,
        height: 512,
        _unit: marker::PhantomData,
    },
    TILE_SIZE_DEFAULT,
    TILE_SIZE_SCROLLBAR_VERTICAL,
    TILE_SIZE_SCROLLBAR_HORIZONTAL,
];

// Return the list of tile sizes for the renderer to allocate texture arrays for.
pub fn tile_cache_sizes(testing: bool) -> &'static [DeviceIntSize] {
    if testing {
        &TILE_SIZE_FOR_TESTS
    } else {
        &[
            TILE_SIZE_DEFAULT,
            TILE_SIZE_SCROLLBAR_HORIZONTAL,
            TILE_SIZE_SCROLLBAR_VERTICAL,
        ]
    }
}

/// The maximum size per axis of a surface,
///  in WorldPixel coordinates.
const MAX_SURFACE_SIZE: f32 = 4096.0;

/// The maximum number of sub-dependencies (e.g. clips, transforms) we can handle
/// per-primitive. If a primitive has more than this, it will invalidate every frame.
const MAX_PRIM_SUB_DEPS: usize = u8::MAX as usize;

/// Used to get unique tile IDs, even when the tile cache is
/// destroyed between display lists / scenes.
static NEXT_TILE_ID: AtomicUsize = AtomicUsize::new(0);

fn clamp(value: i32, low: i32, high: i32) -> i32 {
    value.max(low).min(high)
}

fn clampf(value: f32, low: f32, high: f32) -> f32 {
    value.max(low).min(high)
}

/// An index into the prims array in a TileDescriptor.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct PrimitiveDependencyIndex(u32);

/// Information about the state of an opacity binding.
#[derive(Debug)]
pub struct OpacityBindingInfo {
    /// The current value retrieved from dynamic scene properties.
    value: f32,
    /// True if it was changed (or is new) since the last frame build.
    changed: bool,
}

/// Information stored in a tile descriptor for an opacity binding.
#[derive(Debug, PartialEq, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum OpacityBinding {
    Value(f32),
    Binding(PropertyBindingId),
}

impl From<PropertyBinding<f32>> for OpacityBinding {
    fn from(binding: PropertyBinding<f32>) -> OpacityBinding {
        match binding {
            PropertyBinding::Binding(key, _) => OpacityBinding::Binding(key.id),
            PropertyBinding::Value(value) => OpacityBinding::Value(value),
        }
    }
}

/// Information about the state of a spatial node value
#[derive(Debug)]
pub struct SpatialNodeDependency {
    /// The current value retrieved from the clip-scroll tree.
    value: TransformKey,
    /// True if it was changed (or is new) since the last frame build.
    changed: bool,
}

// Immutable context passed to picture cache tiles during pre_update
struct TilePreUpdateContext {
    /// The local rect of the overall picture cache
    local_rect: PictureRect,

    /// The local clip rect (in picture space) of the entire picture cache
    local_clip_rect: PictureRect,

    /// Maps from picture cache coords -> world space coords.
    pic_to_world_mapper: SpaceMapper<PicturePixel, WorldPixel>,

    /// The fractional position of the picture cache, which may
    /// require invalidation of all tiles.
    fract_offset: PictureVector2D,

    /// The optional background color of the picture cache instance
    background_color: Option<ColorF>,

    /// The visible part of the screen in world coords.
    global_screen_world_rect: WorldRect,
}

// Immutable context passed to picture cache tiles during post_update
struct TilePostUpdateContext<'a> {
    /// The calculated backdrop information for this cache instance.
    backdrop: BackdropInfo,

    /// Information about transform node differences from last frame.
    spatial_nodes: &'a FastHashMap<SpatialNodeIndex, SpatialNodeDependency>,

    /// Information about opacity bindings from the picture cache.
    opacity_bindings: &'a FastHashMap<PropertyBindingId, OpacityBindingInfo>,

    /// Current size in device pixels of tiles for this cache
    current_tile_size: DeviceIntSize,
}

// Mutable state passed to picture cache tiles during post_update
struct TilePostUpdateState<'a> {
    /// Allow access to the texture cache for requesting tiles
    resource_cache: &'a mut ResourceCache,

    /// Current configuration and setup for compositing all the picture cache tiles in renderer.
    composite_state: &'a mut CompositeState,

    /// A cache of comparison results to avoid re-computation during invalidation.
    compare_cache: &'a mut FastHashMap<PrimitiveComparisonKey, PrimitiveCompareResult>,
}

/// Information about the dependencies of a single primitive instance.
struct PrimitiveDependencyInfo {
    /// If true, we should clip the prim rect to the tile boundaries.
    clip_by_tile: bool,

    /// Unique content identifier of the primitive.
    prim_uid: ItemUid,

    /// The picture space origin of this primitive.
    prim_origin: PicturePoint,

    /// The (conservative) clipped area in picture space this primitive occupies.
    prim_clip_rect: PictureRect,

    /// Image keys this primitive depends on.
    images: SmallVec<[ImageDependency; 8]>,

    /// Opacity bindings this primitive depends on.
    opacity_bindings: SmallVec<[OpacityBinding; 4]>,

    /// Clips that this primitive depends on.
    clips: SmallVec<[ItemUid; 8]>,

    /// Spatial nodes references by the clip dependencies of this primitive.
    spatial_nodes: SmallVec<[SpatialNodeIndex; 4]>,
}

impl PrimitiveDependencyInfo {
    /// Construct dependency info for a new primitive.
    fn new(
        prim_uid: ItemUid,
        prim_origin: PicturePoint,
        prim_clip_rect: PictureRect,
    ) -> Self {
        PrimitiveDependencyInfo {
            prim_uid,
            prim_origin,
            images: SmallVec::new(),
            opacity_bindings: SmallVec::new(),
            clip_by_tile: false,
            prim_clip_rect,
            clips: SmallVec::new(),
            spatial_nodes: SmallVec::new(),
        }
    }
}

/// A stable ID for a given tile, to help debugging. These are also used
/// as unique identifiers for tile surfaces when using a native compositor.
#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TileId(pub usize);

/// A descriptor for the kind of texture that a picture cache tile will
/// be drawn into.
#[derive(Debug)]
pub enum SurfaceTextureDescriptor {
    /// When using the WR compositor, the tile is drawn into an entry
    /// in the WR texture cache.
    TextureCache {
        handle: TextureCacheHandle
    },
    /// When using an OS compositor, the tile is drawn into a native
    /// surface identified by arbitrary id.
    Native {
        /// The arbitrary id of this tile.
        id: Option<NativeTileId>,
    },
}

/// This is the same as a `SurfaceTextureDescriptor` but has been resolved
/// into a texture cache handle (if appropriate) that can be used by the
/// batching and compositing code in the renderer.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ResolvedSurfaceTexture {
    TextureCache {
        /// The texture ID to draw to.
        texture: TextureSource,
        /// Slice index in the texture array to draw to.
        layer: i32,
    },
    Native {
        /// The arbitrary id of this tile.
        id: NativeTileId,
        /// The size of the tile in device pixels.
        size: DeviceIntSize,
    }
}

impl SurfaceTextureDescriptor {
    /// Create a resolved surface texture for this descriptor
    pub fn resolve(
        &self,
        resource_cache: &ResourceCache,
        size: DeviceIntSize,
    ) -> ResolvedSurfaceTexture {
        match self {
            SurfaceTextureDescriptor::TextureCache { handle } => {
                let cache_item = resource_cache.texture_cache.get(handle);

                ResolvedSurfaceTexture::TextureCache {
                    texture: cache_item.texture_id,
                    layer: cache_item.texture_layer,
                }
            }
            SurfaceTextureDescriptor::Native { id } => {
                ResolvedSurfaceTexture::Native {
                    id: id.expect("bug: native surface not allocated"),
                    size,
                }
            }
        }
    }
}

/// The backing surface for this tile.
#[derive(Debug)]
pub enum TileSurface {
    Texture {
        /// Descriptor for the surface that this tile draws into.
        descriptor: SurfaceTextureDescriptor,
        /// Bitfield specifying the dirty region(s) that are relevant to this tile.
        visibility_mask: PrimitiveVisibilityMask,
    },
    Color {
        color: ColorF,
    },
    Clear,
}

impl TileSurface {
    fn kind(&self) -> &'static str {
        match *self {
            TileSurface::Color { .. } => "Color",
            TileSurface::Texture { .. } => "Texture",
            TileSurface::Clear => "Clear",
        }
    }
}

/// The result of a primitive dependency comparison. Size is a u8
/// since this is a hot path in the code, and keeping the data small
/// is a performance win.
#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(u8)]
pub enum PrimitiveCompareResult {
    /// Primitives match
    Equal,
    /// Something in the PrimitiveDescriptor was different
    Descriptor,
    /// The clip node content or spatial node changed
    Clip,
    /// The value of the transform changed
    Transform,
    /// An image dependency was dirty
    Image,
    /// The value of an opacity binding changed
    OpacityBinding,
}

/// Debugging information about why a tile was invalidated
#[derive(Debug,Copy,Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum InvalidationReason {
    /// The fractional offset changed
    FractionalOffset,
    /// The background color changed
    BackgroundColor,
    /// The opaque state of the backing native surface changed
    SurfaceOpacityChanged,
    /// There was no backing texture (evicted or never rendered)
    NoTexture,
    /// There was no backing native surface (never rendered, or recreated)
    NoSurface,
    /// The primitive count in the dependency list was different
    PrimCount,
    /// The content of one of the primitives was different
    Content {
        /// What changed in the primitive that was different
        prim_compare_result: PrimitiveCompareResult,
    },
}

/// A minimal subset of Tile for debug capturing
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TileSerializer {
    pub rect: PictureRect,
    pub current_descriptor: TileDescriptor,
    pub fract_offset: PictureVector2D,
    pub id: TileId,
    pub root: TileNode,
    pub background_color: Option<ColorF>,
    pub invalidation_reason: Option<InvalidationReason>
}

/// A minimal subset of TileCacheInstance for debug capturing
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TileCacheInstanceSerializer {
    pub slice: usize,
    pub tiles: FastHashMap<TileOffset, TileSerializer>,
    pub background_color: Option<ColorF>,
    pub fract_offset: PictureVector2D,
}

/// Information about a cached tile.
pub struct Tile {
    /// The current world rect of this tile.
    pub world_rect: WorldRect,
    /// The current local rect of this tile.
    pub rect: PictureRect,
    /// The local rect of the tile clipped to the overall picture local rect.
    clipped_rect: PictureRect,
    /// Uniquely describes the content of this tile, in a way that can be
    /// (reasonably) efficiently hashed and compared.
    pub current_descriptor: TileDescriptor,
    /// The content descriptor for this tile from the previous frame.
    pub prev_descriptor: TileDescriptor,
    /// Handle to the backing surface for this tile.
    pub surface: Option<TileSurface>,
    /// If true, this tile is marked valid, and the existing texture
    /// cache handle can be used. Tiles are invalidated during the
    /// build_dirty_regions method.
    pub is_valid: bool,
    /// If true, this tile intersects with the currently visible screen
    /// rect, and will be drawn.
    pub is_visible: bool,
    /// The current fractional offset of the cache transform root. If this changes,
    /// all tiles need to be invalidated and redrawn, since snapping differences are
    /// likely to occur.
    fract_offset: PictureVector2D,
    /// The tile id is stable between display lists and / or frames,
    /// if the tile is retained. Useful for debugging tile evictions.
    pub id: TileId,
    /// If true, the tile was determined to be opaque, which means blending
    /// can be disabled when drawing it.
    pub is_opaque: bool,
    /// Root node of the quadtree dirty rect tracker.
    root: TileNode,
    /// The picture space dirty rect for this tile.
    dirty_rect: PictureRect,
    /// The world space dirty rect for this tile.
    /// TODO(gw): We have multiple dirty rects available due to the quadtree above. In future,
    ///           expose these as multiple dirty rects, which will help in some cases.
    pub world_dirty_rect: WorldRect,
    /// The last rendered background color on this tile.
    background_color: Option<ColorF>,
    /// The first reason the tile was invalidated this frame.
    invalidation_reason: Option<InvalidationReason>,
}

impl Tile {
    /// Construct a new, invalid tile.
    fn new(
        id: TileId,
    ) -> Self {
        Tile {
            rect: PictureRect::zero(),
            clipped_rect: PictureRect::zero(),
            world_rect: WorldRect::zero(),
            surface: None,
            current_descriptor: TileDescriptor::new(),
            prev_descriptor: TileDescriptor::new(),
            is_valid: false,
            is_visible: false,
            fract_offset: PictureVector2D::zero(),
            id,
            is_opaque: false,
            root: TileNode::new_leaf(Vec::new()),
            dirty_rect: PictureRect::zero(),
            world_dirty_rect: WorldRect::zero(),
            background_color: None,
            invalidation_reason: None,
        }
    }

    /// Print debug information about this tile to a tree printer.
    fn print(&self, pt: &mut dyn PrintTreePrinter) {
        pt.new_level(format!("Tile {:?}", self.id));
        pt.add_item(format!("rect: {}", self.rect));
        pt.add_item(format!("fract_offset: {:?}", self.fract_offset));
        pt.add_item(format!("background_color: {:?}", self.background_color));
        pt.add_item(format!("invalidation_reason: {:?}", self.invalidation_reason));
        self.current_descriptor.print(pt);
        pt.end_level();
    }

    /// Check if the content of the previous and current tile descriptors match
    fn update_dirty_rects(
        &mut self,
        ctx: &TilePostUpdateContext,
        state: &mut TilePostUpdateState,
        invalidation_reason: &mut Option<InvalidationReason>,
    ) -> PictureRect {
        let mut prim_comparer = PrimitiveComparer::new(
            &self.prev_descriptor,
            &self.current_descriptor,
            state.resource_cache,
            ctx.spatial_nodes,
            ctx.opacity_bindings,
        );

        let mut dirty_rect = PictureRect::zero();
        self.root.update_dirty_rects(
            &self.prev_descriptor.prims,
            &self.current_descriptor.prims,
            &mut prim_comparer,
            &mut dirty_rect,
            state.compare_cache,
            invalidation_reason,
        );

        dirty_rect
    }

    /// Invalidate a tile based on change in content. This
    /// must be called even if the tile is not currently
    /// visible on screen. We might be able to improve this
    /// later by changing how ComparableVec is used.
    fn update_content_validity(
        &mut self,
        ctx: &TilePostUpdateContext,
        state: &mut TilePostUpdateState,
    ) {
        // Check if the contents of the primitives, clips, and
        // other dependencies are the same.
        state.compare_cache.clear();
        let mut invalidation_reason = None;
        let dirty_rect = self.update_dirty_rects(
            ctx,
            state,
            &mut invalidation_reason,
        );
        if !dirty_rect.is_empty() {
            self.invalidate(
                Some(dirty_rect),
                invalidation_reason.expect("bug: no invalidation_reason"),
            );
        }
    }

    /// Invalidate this tile. If `invalidation_rect` is None, the entire
    /// tile is invalidated.
    fn invalidate(
        &mut self,
        invalidation_rect: Option<PictureRect>,
        reason: InvalidationReason,
    ) {
        self.is_valid = false;

        match invalidation_rect {
            Some(rect) => {
                self.dirty_rect = self.dirty_rect.union(&rect);
            }
            None => {
                self.dirty_rect = self.rect;
            }
        }

        if self.invalidation_reason.is_none() {
            self.invalidation_reason = Some(reason);
        }
    }

    /// Called during pre_update of a tile cache instance. Allows the
    /// tile to setup state before primitive dependency calculations.
    fn pre_update(
        &mut self,
        rect: PictureRect,
        ctx: &TilePreUpdateContext,
    ) {
        self.rect = rect;
        self.invalidation_reason  = None;

        self.clipped_rect = self.rect
            .intersection(&ctx.local_rect)
            .and_then(|r| r.intersection(&ctx.local_clip_rect))
            .unwrap_or_else(PictureRect::zero);

        self.world_rect = ctx.pic_to_world_mapper
            .map(&self.rect)
            .expect("bug: map local tile rect");

        // Check if this tile is currently on screen.
        self.is_visible = self.world_rect.intersects(&ctx.global_screen_world_rect);

        // If the tile isn't visible, early exit, skipping the normal set up to
        // validate dependencies. Instead, we will only compare the current tile
        // dependencies the next time it comes into view.
        if !self.is_visible {
            return;
        }

        // Determine if the fractional offset of the transform is different this frame
        // from the currently cached tile set.
        let fract_changed = (self.fract_offset.x - ctx.fract_offset.x).abs() > 0.01 ||
                            (self.fract_offset.y - ctx.fract_offset.y).abs() > 0.01;
        if fract_changed {
            self.invalidate(None, InvalidationReason::FractionalOffset);
            self.fract_offset = ctx.fract_offset;
        }

        if ctx.background_color != self.background_color {
            self.invalidate(None, InvalidationReason::BackgroundColor);
            self.background_color = ctx.background_color;
        }

        // Clear any dependencies so that when we rebuild them we
        // can compare if the tile has the same content.
        mem::swap(
            &mut self.current_descriptor,
            &mut self.prev_descriptor,
        );
        self.current_descriptor.clear();
        self.root.clear(rect);
    }

    /// Add dependencies for a given primitive to this tile.
    fn add_prim_dependency(
        &mut self,
        info: &PrimitiveDependencyInfo,
    ) {
        // If this tile isn't currently visible, we don't want to update the dependencies
        // for this tile, as an optimization, since it won't be drawn anyway.
        if !self.is_visible {
            return;
        }

        // Include any image keys this tile depends on.
        self.current_descriptor.images.extend_from_slice(&info.images);

        // Include any opacity bindings this primitive depends on.
        self.current_descriptor.opacity_bindings.extend_from_slice(&info.opacity_bindings);

        // Include any clip nodes that this primitive depends on.
        self.current_descriptor.clips.extend_from_slice(&info.clips);

        // Include any transforms that this primitive depends on.
        self.current_descriptor.transforms.extend_from_slice(&info.spatial_nodes);

        // TODO(gw): The origin of background rects produced by APZ changes
        //           in Gecko during scrolling. Consider investigating this so the
        //           hack / workaround below is not required.
        let (prim_origin, prim_clip_rect) = if info.clip_by_tile {
            let tile_p0 = self.rect.origin;
            let tile_p1 = self.rect.bottom_right();

            let clip_p0 = PicturePoint::new(
                clampf(info.prim_clip_rect.origin.x, tile_p0.x, tile_p1.x),
                clampf(info.prim_clip_rect.origin.y, tile_p0.y, tile_p1.y),
            );

            let clip_p1 = PicturePoint::new(
                clampf(info.prim_clip_rect.origin.x + info.prim_clip_rect.size.width, tile_p0.x, tile_p1.x),
                clampf(info.prim_clip_rect.origin.y + info.prim_clip_rect.size.height, tile_p0.y, tile_p1.y),
            );

            (
                PicturePoint::new(
                    clampf(info.prim_origin.x, tile_p0.x, tile_p1.x),
                    clampf(info.prim_origin.y, tile_p0.y, tile_p1.y),
                ),
                PictureRect::new(
                    clip_p0,
                    PictureSize::new(
                        clip_p1.x - clip_p0.x,
                        clip_p1.y - clip_p0.y,
                    ),
                ),
            )
        } else {
            (info.prim_origin, info.prim_clip_rect)
        };

        // Update the tile descriptor, used for tile comparison during scene swaps.
        let prim_index = PrimitiveDependencyIndex(self.current_descriptor.prims.len() as u32);

        // We know that the casts below will never overflow because the array lengths are
        // truncated to MAX_PRIM_SUB_DEPS during update_prim_dependencies.
        debug_assert!(info.spatial_nodes.len() <= MAX_PRIM_SUB_DEPS);
        debug_assert!(info.clips.len() <= MAX_PRIM_SUB_DEPS);
        debug_assert!(info.images.len() <= MAX_PRIM_SUB_DEPS);
        debug_assert!(info.opacity_bindings.len() <= MAX_PRIM_SUB_DEPS);

        self.current_descriptor.prims.push(PrimitiveDescriptor {
            prim_uid: info.prim_uid,
            origin: prim_origin.into(),
            prim_clip_rect: prim_clip_rect.into(),
            transform_dep_count: info.spatial_nodes.len()  as u8,
            clip_dep_count: info.clips.len() as u8,
            image_dep_count: info.images.len() as u8,
            opacity_binding_dep_count: info.opacity_bindings.len() as u8,
        });

        // Add this primitive to the dirty rect quadtree.
        self.root.add_prim(prim_index, &info.prim_clip_rect);
    }

    /// Called during tile cache instance post_update. Allows invalidation and dirty
    /// rect calculation after primitive dependencies have been updated.
    fn post_update(
        &mut self,
        ctx: &TilePostUpdateContext,
        state: &mut TilePostUpdateState,
    ) -> bool {
        // If tile is not visible, just early out from here - we don't update dependencies
        // so don't want to invalidate, merge, split etc. The tile won't need to be drawn
        // (and thus updated / invalidated) until it is on screen again.
        if !self.is_visible {
            return false;
        }

        // Invalidate the tile based on the content changing.
        self.update_content_validity(ctx, state);

        // If there are no primitives there is no need to draw or cache it.
        if self.current_descriptor.prims.is_empty() {
            // If there is a native compositor surface allocated for this (now empty) tile
            // it must be freed here, otherwise the stale tile with previous contents will
            // be composited. If the tile subsequently gets new primitives added to it, the
            // surface will be re-allocated when it's added to the composite draw list.
            if let Some(TileSurface::Texture { descriptor: SurfaceTextureDescriptor::Native { mut id, .. }, .. }) = self.surface.take() {
                if let Some(id) = id.take() {
                    state.resource_cache.destroy_compositor_tile(id);
                }
            }

            return false;
        }

        // Check if this tile can be considered opaque. Opacity state must be updated only
        // after all early out checks have been performed. Otherwise, we might miss updating
        // the native surface next time this tile becomes visible.
        self.is_opaque = ctx.backdrop.rect.contains_rect(&self.clipped_rect);

        // Check if the selected composite mode supports dirty rect updates. For Draw composite
        // mode, we can always update the content with smaller dirty rects. For native composite
        // mode, we can only use dirty rects if the compositor supports partial surface updates.
        let (supports_dirty_rects, supports_simple_prims) = match state.composite_state.compositor_kind {
            CompositorKind::Draw { .. } => {
                (true, true)
            }
            CompositorKind::Native { max_update_rects, .. } => {
                (max_update_rects > 0, false)
            }
        };

        // TODO(gw): Consider using smaller tiles and/or tile splits for
        //           native compositors that don't support dirty rects.
        if supports_dirty_rects {
            // Only allow splitting for normal content sized tiles
            if ctx.current_tile_size == TILE_SIZE_DEFAULT {
                let max_split_level = 3;

                // Consider splitting / merging dirty regions
                self.root.maybe_merge_or_split(
                    0,
                    &self.current_descriptor.prims,
                    max_split_level,
                );
            }
        }

        // The dirty rect will be set correctly by now. If the underlying platform
        // doesn't support partial updates, and this tile isn't valid, force the dirty
        // rect to be the size of the entire tile.
        if !self.is_valid && !supports_dirty_rects {
            self.dirty_rect = self.rect;
        }

        // Ensure that the dirty rect doesn't extend outside the local tile rect.
        self.dirty_rect = self.dirty_rect
            .intersection(&self.rect)
            .unwrap_or_else(PictureRect::zero);

        // See if this tile is a simple color, in which case we can just draw
        // it as a rect, and avoid allocating a texture surface and drawing it.
        // TODO(gw): Initial native compositor interface doesn't support simple
        //           color tiles. We can definitely support this in DC, so this
        //           should be added as a follow up.
        let is_simple_prim =
            ctx.backdrop.kind.can_be_promoted_to_compositor_surface() &&
            self.current_descriptor.prims.len() == 1 &&
            self.is_opaque &&
            supports_simple_prims;

        // Set up the backing surface for this tile.
        let surface = if is_simple_prim {
            // If we determine the tile can be represented by a color, set the
            // surface unconditionally (this will drop any previously used
            // texture cache backing surface).
            match ctx.backdrop.kind {
                BackdropKind::Color { color } => {
                    TileSurface::Color {
                        color,
                    }
                }
                BackdropKind::Clear => {
                    TileSurface::Clear
                }
                BackdropKind::Image => {
                    // This should be prevented by the is_simple_prim check above.
                    unreachable!();
                }
            }
        } else {
            // If this tile will be backed by a surface, we want to retain
            // the texture handle from the previous frame, if possible. If
            // the tile was previously a color, or not set, then just set
            // up a new texture cache handle.
            match self.surface.take() {
                Some(TileSurface::Texture { descriptor, visibility_mask }) => {
                    // Reuse the existing descriptor and vis mask
                    TileSurface::Texture {
                        descriptor,
                        visibility_mask,
                    }
                }
                Some(TileSurface::Color { .. }) | Some(TileSurface::Clear) | None => {
                    // This is the case where we are constructing a tile surface that
                    // involves drawing to a texture. Create the correct surface
                    // descriptor depending on the compositing mode that will read
                    // the output.
                    let descriptor = match state.composite_state.compositor_kind {
                        CompositorKind::Draw { .. } => {
                            // For a texture cache entry, create an invalid handle that
                            // will be allocated when update_picture_cache is called.
                            SurfaceTextureDescriptor::TextureCache {
                                handle: TextureCacheHandle::invalid(),
                            }
                        }
                        CompositorKind::Native { .. } => {
                            // Create a native surface surface descriptor, but don't allocate
                            // a surface yet. The surface is allocated *after* occlusion
                            // culling occurs, so that only visible tiles allocate GPU memory.
                            SurfaceTextureDescriptor::Native {
                                id: None,
                            }
                        }
                    };

                    TileSurface::Texture {
                        descriptor,
                        visibility_mask: PrimitiveVisibilityMask::empty(),
                    }
                }
            }
        };

        // Store the current surface backing info for use during batching.
        self.surface = Some(surface);

        true
    }
}

/// Defines a key that uniquely identifies a primitive instance.
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveDescriptor {
    /// Uniquely identifies the content of the primitive template.
    prim_uid: ItemUid,
    /// The origin in world space of this primitive.
    origin: PointKey,
    /// The clip rect for this primitive. Included here in
    /// dependencies since there is no entry in the clip chain
    /// dependencies for the local clip rect.
    pub prim_clip_rect: RectangleKey,
    /// The number of extra dependencies that this primitive has.
    transform_dep_count: u8,
    image_dep_count: u8,
    opacity_binding_dep_count: u8,
    clip_dep_count: u8,
}

impl PartialEq for PrimitiveDescriptor {
    fn eq(&self, other: &Self) -> bool {
        const EPSILON: f32 = 0.001;

        if self.prim_uid != other.prim_uid {
            return false;
        }

        if !self.origin.x.approx_eq_eps(&other.origin.x, &EPSILON) {
            return false;
        }
        if !self.origin.y.approx_eq_eps(&other.origin.y, &EPSILON) {
            return false;
        }

        if !self.prim_clip_rect.x.approx_eq_eps(&other.prim_clip_rect.x, &EPSILON) {
            return false;
        }
        if !self.prim_clip_rect.y.approx_eq_eps(&other.prim_clip_rect.y, &EPSILON) {
            return false;
        }
        if !self.prim_clip_rect.w.approx_eq_eps(&other.prim_clip_rect.w, &EPSILON) {
            return false;
        }
        if !self.prim_clip_rect.h.approx_eq_eps(&other.prim_clip_rect.h, &EPSILON) {
            return false;
        }

        true
    }
}

/// A small helper to compare two arrays of primitive dependencies.
struct CompareHelper<'a, T> {
    offset_curr: usize,
    offset_prev: usize,
    curr_items: &'a [T],
    prev_items: &'a [T],
}

impl<'a, T> CompareHelper<'a, T> where T: PartialEq {
    /// Construct a new compare helper for a current / previous set of dependency information.
    fn new(
        prev_items: &'a [T],
        curr_items: &'a [T],
    ) -> Self {
        CompareHelper {
            offset_curr: 0,
            offset_prev: 0,
            curr_items,
            prev_items,
        }
    }

    /// Reset the current position in the dependency array to the start
    fn reset(&mut self) {
        self.offset_prev = 0;
        self.offset_curr = 0;
    }

    /// Test if two sections of the dependency arrays are the same, by checking both
    /// item equality, and a user closure to see if the content of the item changed.
    fn is_same<F>(
        &self,
        prev_count: u8,
        curr_count: u8,
        f: F,
    ) -> bool where F: Fn(&T) -> bool {
        // If the number of items is different, trivial reject.
        if prev_count != curr_count {
            return false;
        }
        // If both counts are 0, then no need to check these dependencies.
        if curr_count == 0 {
            return true;
        }
        // If both counts are u8::MAX, this is a sentinel that we can't compare these
        // deps, so just trivial reject.
        if curr_count as usize == MAX_PRIM_SUB_DEPS {
            return false;
        }

        let end_prev = self.offset_prev + prev_count as usize;
        let end_curr = self.offset_curr + curr_count as usize;

        let curr_items = &self.curr_items[self.offset_curr .. end_curr];
        let prev_items = &self.prev_items[self.offset_prev .. end_prev];

        for (curr, prev) in curr_items.iter().zip(prev_items.iter()) {
            if prev != curr {
                return false;
            }

            if f(curr) {
                return false;
            }
        }

        true
    }

    // Advance the prev dependency array by a given amount
    fn advance_prev(&mut self, count: u8) {
        self.offset_prev += count as usize;
    }

    // Advance the current dependency array by a given amount
    fn advance_curr(&mut self, count: u8) {
        self.offset_curr  += count as usize;
    }
}

/// Uniquely describes the content of this tile, in a way that can be
/// (reasonably) efficiently hashed and compared.
#[cfg_attr(any(feature="capture",feature="replay"), derive(Clone))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TileDescriptor {
    /// List of primitive instance unique identifiers. The uid is guaranteed
    /// to uniquely describe the content of the primitive template, while
    /// the other parameters describe the clip chain and instance params.
    pub prims: Vec<PrimitiveDescriptor>,

    /// List of clip node descriptors.
    clips: Vec<ItemUid>,

    /// List of image keys that this tile depends on.
    images: Vec<ImageDependency>,

    /// The set of opacity bindings that this tile depends on.
    // TODO(gw): Ugh, get rid of all opacity binding support!
    opacity_bindings: Vec<OpacityBinding>,

    /// List of the effects of transforms that we care about
    /// tracking for this tile.
    transforms: Vec<SpatialNodeIndex>,
}

impl TileDescriptor {
    fn new() -> Self {
        TileDescriptor {
            prims: Vec::new(),
            clips: Vec::new(),
            opacity_bindings: Vec::new(),
            images: Vec::new(),
            transforms: Vec::new(),
        }
    }

    /// Print debug information about this tile descriptor to a tree printer.
    fn print(&self, pt: &mut dyn PrintTreePrinter) {
        pt.new_level("current_descriptor".to_string());

        pt.new_level("prims".to_string());
        for prim in &self.prims {
            pt.new_level(format!("prim uid={}", prim.prim_uid.get_uid()));
            pt.add_item(format!("origin: {},{}", prim.origin.x, prim.origin.y));
            pt.add_item(format!("clip: origin={},{} size={}x{}",
                prim.prim_clip_rect.x,
                prim.prim_clip_rect.y,
                prim.prim_clip_rect.w,
                prim.prim_clip_rect.h,
            ));
            pt.add_item(format!("deps: t={} i={} o={} c={}",
                prim.transform_dep_count,
                prim.image_dep_count,
                prim.opacity_binding_dep_count,
                prim.clip_dep_count,
            ));
            pt.end_level();
        }
        pt.end_level();

        if !self.clips.is_empty() {
            pt.new_level("clips".to_string());
            for clip in &self.clips {
                pt.new_level(format!("clip uid={}", clip.get_uid()));
                pt.end_level();
            }
            pt.end_level();
        }

        if !self.images.is_empty() {
            pt.new_level("images".to_string());
            for info in &self.images {
                pt.new_level(format!("key={:?}", info.key));
                pt.add_item(format!("generation={:?}", info.generation));
                pt.end_level();
            }
            pt.end_level();
        }

        if !self.opacity_bindings.is_empty() {
            pt.new_level("opacity_bindings".to_string());
            for opacity_binding in &self.opacity_bindings {
                pt.new_level(format!("binding={:?}", opacity_binding));
                pt.end_level();
            }
            pt.end_level();
        }

        if !self.transforms.is_empty() {
            pt.new_level("transforms".to_string());
            for transform in &self.transforms {
                pt.new_level(format!("spatial_node={:?}", transform));
                pt.end_level();
            }
            pt.end_level();
        }

        pt.end_level();
    }

    /// Clear the dependency information for a tile, when the dependencies
    /// are being rebuilt.
    fn clear(&mut self) {
        self.prims.clear();
        self.clips.clear();
        self.opacity_bindings.clear();
        self.images.clear();
        self.transforms.clear();
    }
}

/// Stores both the world and devices rects for a single dirty rect.
#[derive(Debug, Clone)]
pub struct DirtyRegionRect {
    /// World rect of this dirty region
    pub world_rect: WorldRect,
    /// Bitfield for picture render tasks that draw this dirty region.
    pub visibility_mask: PrimitiveVisibilityMask,
}

/// Represents the dirty region of a tile cache picture.
#[derive(Debug, Clone)]
pub struct DirtyRegion {
    /// The individual dirty rects of this region.
    pub dirty_rects: Vec<DirtyRegionRect>,

    /// The overall dirty rect, a combination of dirty_rects
    pub combined: WorldRect,
}

impl DirtyRegion {
    /// Construct a new dirty region tracker.
    pub fn new(
    ) -> Self {
        DirtyRegion {
            dirty_rects: Vec::with_capacity(PrimitiveVisibilityMask::MAX_DIRTY_REGIONS),
            combined: WorldRect::zero(),
        }
    }

    /// Reset the dirty regions back to empty
    pub fn clear(&mut self) {
        self.dirty_rects.clear();
        self.combined = WorldRect::zero();
    }

    /// Push a dirty rect into this region
    pub fn push(
        &mut self,
        rect: WorldRect,
        visibility_mask: PrimitiveVisibilityMask,
    ) {
        // Include this in the overall dirty rect
        self.combined = self.combined.union(&rect);

        // Store the individual dirty rect.
        self.dirty_rects.push(DirtyRegionRect {
            world_rect: rect,
            visibility_mask,
        });
    }

    /// Include another rect into an existing dirty region.
    pub fn include_rect(
        &mut self,
        region_index: usize,
        rect: WorldRect,
    ) {
        self.combined = self.combined.union(&rect);

        let region = &mut self.dirty_rects[region_index];
        region.world_rect = region.world_rect.union(&rect);
    }

    // TODO(gw): This returns a heap allocated object. Perhaps we can simplify this
    //           logic? Although - it's only used very rarely so it may not be an issue.
    pub fn inflate(
        &self,
        inflate_amount: f32,
    ) -> DirtyRegion {
        let mut dirty_rects = Vec::with_capacity(self.dirty_rects.len());
        let mut combined = WorldRect::zero();

        for rect in &self.dirty_rects {
            let world_rect = rect.world_rect.inflate(inflate_amount, inflate_amount);
            combined = combined.union(&world_rect);
            dirty_rects.push(DirtyRegionRect {
                world_rect,
                visibility_mask: rect.visibility_mask,
            });
        }

        DirtyRegion {
            dirty_rects,
            combined,
        }
    }

    /// Creates a record of this dirty region for exporting to test infrastructure.
    pub fn record(&self) -> RecordedDirtyRegion {
        let mut rects: Vec<WorldRect> =
            self.dirty_rects.iter().map(|r| r.world_rect).collect();
        rects.sort_unstable_by_key(|r| (r.origin.y as usize, r.origin.x as usize));
        RecordedDirtyRegion { rects }
    }
}

/// A recorded copy of the dirty region for exporting to test infrastructure.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct RecordedDirtyRegion {
    pub rects: Vec<WorldRect>,
}

impl ::std::fmt::Display for RecordedDirtyRegion {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        for r in self.rects.iter() {
            let (x, y, w, h) = (r.origin.x, r.origin.y, r.size.width, r.size.height);
            write!(f, "[({},{}):{}x{}]", x, y, w, h)?;
        }
        Ok(())
    }
}

impl ::std::fmt::Debug for RecordedDirtyRegion {
    fn fmt(&self, f: &mut ::std::fmt::Formatter) -> ::std::fmt::Result {
        ::std::fmt::Display::fmt(self, f)
    }
}

#[derive(Debug, Copy, Clone)]
enum BackdropKind {
    Color {
        color: ColorF,
    },
    Clear,
    Image,
}

impl BackdropKind {
    /// Returns true if the compositor can directly draw this backdrop.
    fn can_be_promoted_to_compositor_surface(&self) -> bool {
        match self {
            BackdropKind::Color { .. } | BackdropKind::Clear => true,
            BackdropKind::Image => false,
        }
    }
}

/// Stores information about the calculated opaque backdrop of this slice.
#[derive(Debug, Copy, Clone)]
struct BackdropInfo {
    /// The picture space rectangle that is known to be opaque. This is used
    /// to determine where subpixel AA can be used, and where alpha blending
    /// can be disabled.
    rect: PictureRect,
    /// Kind of the backdrop
    kind: BackdropKind,
}

impl BackdropInfo {
    fn empty() -> Self {
        BackdropInfo {
            rect: PictureRect::zero(),
            kind: BackdropKind::Color {
                color: ColorF::BLACK,
            },
        }
    }
}

#[derive(Clone)]
pub struct TileCacheLoggerSlice {
    pub serialized_slice: String,
    pub local_to_world_transform: DeviceRect
}

#[cfg(any(feature = "capture", feature = "replay"))]
macro_rules! declare_tile_cache_logger_updatelists {
    ( $( $name:ident : $ty:ty, )+ ) => {
        #[cfg_attr(feature = "capture", derive(Serialize))]
        #[cfg_attr(feature = "replay", derive(Deserialize))]
        struct TileCacheLoggerUpdateListsSerializer {
            pub ron_string: Vec<String>,
        }

        pub struct TileCacheLoggerUpdateLists {
            $(
                /// Generate storage, one per interner.
                /// the tuple is a workaround to avoid the need for multiple
                /// fields that start with $name (macro concatenation).
                /// the string is .ron serialized updatelist at capture time;
                /// the updates is the list of DataStore updates (avoid UpdateList
                /// due to Default() requirements on the Keys) reconstructed at
                /// load time.
                pub $name: (String, Vec<Update>),
            )+
        }

        impl TileCacheLoggerUpdateLists {
            pub fn new() -> Self {
                TileCacheLoggerUpdateLists {
                    $(
                        $name : ( String::new(), Vec::<Update>::new() ),
                    )+
                }
            }

            /// serialize all interners in updates to .ron
            fn serialize_updates(
                &mut self,
                updates: &InternerUpdates
            ) {
                $(
                    self.$name.0 = ron::ser::to_string_pretty(&updates.$name.updates, Default::default()).unwrap();
                )+
            }

            fn is_empty(&self) -> bool {
                $(
                    if !self.$name.0.is_empty() { return false; }
                )+
                true
            }

            #[cfg(feature = "capture")]
            fn to_ron(&self) -> String {
                let mut serializer =
                    TileCacheLoggerUpdateListsSerializer { ron_string: Vec::new() };
                $(
                    serializer.ron_string.push(
                        if self.$name.0.is_empty() {
                            "[]".to_string()
                        } else {
                            self.$name.0.clone()
                        });
                )+
                ron::ser::to_string_pretty(&serializer, Default::default()).unwrap()
            }

            #[cfg(feature = "replay")]
            pub fn from_ron(&mut self, text: &str) {
                let serializer : TileCacheLoggerUpdateListsSerializer = 
                    match ron::de::from_str(&text) {
                        Ok(data) => { data }
                        Err(e) => {
                            println!("ERROR: failed to deserialize updatelist: {:?}\n{:?}", &text, e);
                            return;
                        }
                    };
                let mut index = 0;
                $(
                    self.$name.1 = ron::de::from_str(&serializer.ron_string[index]).unwrap();
                    index = index + 1;
                )+
                // error: value assigned to `index` is never read
                let _ = index;
            }
        }
    }
}

#[cfg(any(feature = "capture", feature = "replay"))]
enumerate_interners!(declare_tile_cache_logger_updatelists);

#[cfg(not(feature = "capture"))]
#[derive(Clone)]
pub struct TileCacheLoggerUpdateLists {
}

#[cfg(not(feature = "capture"))]
impl TileCacheLoggerUpdateLists {
    pub fn new() -> Self { TileCacheLoggerUpdateLists {} }
    fn is_empty(&self) -> bool { true }
}

/// Log tile cache activity for one single frame.
/// Also stores the commands sent to the interning data_stores
/// so we can see which items were created or destroyed this frame,
/// and correlate that with tile invalidation activity.
pub struct TileCacheLoggerFrame {
    /// slices in the frame, one per take_context call
    pub slices: Vec<TileCacheLoggerSlice>,
    /// interning activity
    pub update_lists: TileCacheLoggerUpdateLists
}

impl TileCacheLoggerFrame {
    pub fn new() -> Self {
        TileCacheLoggerFrame {
            slices: Vec::new(),
            update_lists: TileCacheLoggerUpdateLists::new()
        }
    }

    pub fn is_empty(&self) -> bool {
        self.slices.is_empty() && self.update_lists.is_empty()
    }
}

/// Log tile cache activity whenever anything happens in take_context.
pub struct TileCacheLogger {
    /// next write pointer
    pub write_index : usize,
    /// ron serialization of tile caches;
    pub frames: Vec<TileCacheLoggerFrame>
}

impl TileCacheLogger {
    pub fn new(
        num_frames: usize
    ) -> Self {
        let mut frames = Vec::with_capacity(num_frames);
        for _i in 0..num_frames { // no Clone so no resize
            frames.push(TileCacheLoggerFrame::new());
        }
        TileCacheLogger {
            write_index: 0,
            frames
        }
    }

    pub fn is_enabled(&self) -> bool {
        !self.frames.is_empty()
    }

    #[cfg(feature = "capture")]
    pub fn add(&mut self, serialized_slice: String, local_to_world_transform: DeviceRect) {
        if !self.is_enabled() {
            return;
        }
        self.frames[self.write_index].slices.push(
            TileCacheLoggerSlice {
                serialized_slice,
                local_to_world_transform });
    }

    #[cfg(feature = "capture")]
    pub fn serialize_updates(&mut self, updates: &InternerUpdates) {
        if !self.is_enabled() {
            return;
        }
        self.frames[self.write_index].update_lists.serialize_updates(updates);
    }

    /// see if anything was written in this frame, and if so,
    /// advance the write index in a circular way and clear the
    /// recorded string.
    pub fn advance(&mut self) {
        if !self.is_enabled() || self.frames[self.write_index].is_empty() {
            return;
        }
        self.write_index = self.write_index + 1;
        if self.write_index >= self.frames.len() {
            self.write_index = 0;
        }
        self.frames[self.write_index] = TileCacheLoggerFrame::new();
    }

    #[cfg(feature = "capture")]
    pub fn save_capture(
        &self, root: &PathBuf
    ) {
        if !self.is_enabled() {
            return;
        }
        use std::fs;

        info!("saving tile cache log");
        let path_tile_cache = root.join("tile_cache");
        if !path_tile_cache.is_dir() {
            fs::create_dir(&path_tile_cache).unwrap();
        }

        let mut files_written = 0;
        for ix in 0..self.frames.len() {
            // ...and start with write_index, since that's the oldest entry
            // that we're about to overwrite. However when we get to
            // save_capture, we've add()ed entries but haven't advance()d yet,
            // so the actual oldest entry is write_index + 1
            let index = (self.write_index + 1 + ix) % self.frames.len();
            if self.frames[index].is_empty() {
                continue;
            }

            let filename = path_tile_cache.join(format!("frame{:05}.ron", files_written));
            let mut output = File::create(filename).unwrap();
            output.write_all(b"// slice data\n").unwrap();
            output.write_all(b"[\n").unwrap();
            for item in &self.frames[index].slices {
                output.write_all(format!("( x: {}, y: {},\n",
                                         item.local_to_world_transform.origin.x,
                                         item.local_to_world_transform.origin.y)
                                 .as_bytes()).unwrap();
                output.write_all(b"tile_cache:\n").unwrap();
                output.write_all(item.serialized_slice.as_bytes()).unwrap();
                output.write_all(b"\n),\n").unwrap();
            }
            output.write_all(b"]\n\n").unwrap();

            output.write_all(b"// @@@ chunk @@@\n\n").unwrap();

            output.write_all(b"// interning data\n").unwrap();
            output.write_all(self.frames[index].update_lists.to_ron().as_bytes()).unwrap();

            files_written = files_written + 1;
        }
    }
}

/// Represents a cache of tiles that make up a picture primitives.
pub struct TileCacheInstance {
    /// Index of the tile cache / slice for this frame builder. It's determined
    /// by the setup_picture_caching method during flattening, which splits the
    /// picture tree into multiple slices. It's used as a simple input to the tile
    /// keys. It does mean we invalidate tiles if a new layer gets inserted / removed
    /// between display lists - this seems very unlikely to occur on most pages, but
    /// can be revisited if we ever notice that.
    pub slice: usize,
    /// The currently selected tile size to use for this cache
    pub current_tile_size: DeviceIntSize,
    /// The positioning node for this tile cache.
    pub spatial_node_index: SpatialNodeIndex,
    /// Hash of tiles present in this picture.
    pub tiles: FastHashMap<TileOffset, Tile>,
    /// Switch back and forth between old and new tiles hashmaps to avoid re-allocating.
    old_tiles: FastHashMap<TileOffset, Tile>,
    /// A helper struct to map local rects into surface coords.
    map_local_to_surface: SpaceMapper<LayoutPixel, PicturePixel>,
    /// A helper struct to map child picture rects into picture cache surface coords.
    map_child_pic_to_surface: SpaceMapper<PicturePixel, PicturePixel>,
    /// List of opacity bindings, with some extra information
    /// about whether they changed since last frame.
    opacity_bindings: FastHashMap<PropertyBindingId, OpacityBindingInfo>,
    /// Switch back and forth between old and new bindings hashmaps to avoid re-allocating.
    old_opacity_bindings: FastHashMap<PropertyBindingId, OpacityBindingInfo>,
    /// List of spatial nodes, with some extra information
    /// about whether they changed since last frame.
    spatial_nodes: FastHashMap<SpatialNodeIndex, SpatialNodeDependency>,
    /// Switch back and forth between old and new spatial nodes hashmaps to avoid re-allocating.
    old_spatial_nodes: FastHashMap<SpatialNodeIndex, SpatialNodeDependency>,
    /// A set of spatial nodes that primitives / clips depend on found
    /// during dependency creation. This is used to avoid trying to
    /// calculate invalid relative transforms when building the spatial
    /// nodes hash above.
    used_spatial_nodes: FastHashSet<SpatialNodeIndex>,
    /// The current dirty region tracker for this picture.
    pub dirty_region: DirtyRegion,
    /// Current size of tiles in picture units.
    tile_size: PictureSize,
    /// Tile coords of the currently allocated grid.
    tile_rect: TileRect,
    /// Pre-calculated versions of the tile_rect above, used to speed up the
    /// calculations in get_tile_coords_for_rect.
    tile_bounds_p0: TileOffset,
    tile_bounds_p1: TileOffset,
    /// Local rect (unclipped) of the picture this cache covers.
    pub local_rect: PictureRect,
    /// The local clip rect, from the shared clips of this picture.
    local_clip_rect: PictureRect,
    /// A list of tiles that are valid and visible, which should be drawn to the main scene.
    pub tiles_to_draw: Vec<TileOffset>,
    /// The surface index that this tile cache will be drawn into.
    surface_index: SurfaceIndex,
    /// The background color from the renderer. If this is set opaque, we know it's
    /// fine to clear the tiles to this and allow subpixel text on the first slice.
    pub background_color: Option<ColorF>,
    /// Information about the calculated backdrop content of this cache.
    backdrop: BackdropInfo,
    /// The allowed subpixel mode for this surface, which depends on the detected
    /// opacity of the background.
    pub subpixel_mode: SubpixelMode,
    /// A list of clip handles that exist on every (top-level) primitive in this picture.
    /// It's often the case that these are root / fixed position clips. By handling them
    /// here, we can avoid applying them to the items, which reduces work, but more importantly
    /// reduces invalidations.
    pub shared_clips: Vec<ClipDataHandle>,
    /// The clip chain that represents the shared_clips above. Used to build the local
    /// clip rect for this tile cache.
    shared_clip_chain: ClipChainId,
    /// The current transform of the picture cache root spatial node
    root_transform: TransformKey,
    /// The number of frames until this cache next evaluates what tile size to use.
    /// If a picture rect size is regularly changing just around a size threshold,
    /// we don't want to constantly invalidate and reallocate different tile size
    /// configuration each frame.
    frames_until_size_eval: usize,
    /// The current fractional offset of the cached picture
    fract_offset: PictureVector2D,
    /// keep around the hash map used as compare_cache to avoid reallocating it each
    /// frame.
    compare_cache: FastHashMap<PrimitiveComparisonKey, PrimitiveCompareResult>,
    /// The allocated compositor surface for this picture cache. May be None if
    /// not using native compositor, or if the surface was destroyed and needs
    /// to be reallocated next time this surface contains valid tiles.
    pub native_surface_id: Option<NativeSurfaceId>,
    /// The current device position of this cache. Used to set the compositor
    /// offset of the surface when building the visual tree.
    pub device_position: DevicePoint,
    /// True if the entire picture cache surface is opaque.
    is_opaque: bool,
    /// The currently considered tile size override. Used to check if we should
    /// re-evaluate tile size, even if the frame timer hasn't expired.
    tile_size_override: Option<DeviceIntSize>,
}

impl TileCacheInstance {
    pub fn new(
        slice: usize,
        spatial_node_index: SpatialNodeIndex,
        background_color: Option<ColorF>,
        shared_clips: Vec<ClipDataHandle>,
        shared_clip_chain: ClipChainId,
    ) -> Self {
        TileCacheInstance {
            slice,
            spatial_node_index,
            tiles: FastHashMap::default(),
            old_tiles: FastHashMap::default(),
            map_local_to_surface: SpaceMapper::new(
                ROOT_SPATIAL_NODE_INDEX,
                PictureRect::zero(),
            ),
            map_child_pic_to_surface: SpaceMapper::new(
                ROOT_SPATIAL_NODE_INDEX,
                PictureRect::zero(),
            ),
            opacity_bindings: FastHashMap::default(),
            old_opacity_bindings: FastHashMap::default(),
            spatial_nodes: FastHashMap::default(),
            old_spatial_nodes: FastHashMap::default(),
            used_spatial_nodes: FastHashSet::default(),
            dirty_region: DirtyRegion::new(),
            tile_size: PictureSize::zero(),
            tile_rect: TileRect::zero(),
            tile_bounds_p0: TileOffset::zero(),
            tile_bounds_p1: TileOffset::zero(),
            local_rect: PictureRect::zero(),
            local_clip_rect: PictureRect::zero(),
            tiles_to_draw: Vec::new(),
            surface_index: SurfaceIndex(0),
            background_color,
            backdrop: BackdropInfo::empty(),
            subpixel_mode: SubpixelMode::Allow,
            root_transform: TransformKey::Local,
            shared_clips,
            shared_clip_chain,
            current_tile_size: DeviceIntSize::zero(),
            frames_until_size_eval: 0,
            fract_offset: PictureVector2D::zero(),
            compare_cache: FastHashMap::default(),
            native_surface_id: None,
            device_position: DevicePoint::zero(),
            is_opaque: true,
            tile_size_override: None,
        }
    }

    /// Returns true if this tile cache is considered opaque.
    pub fn is_opaque(&self) -> bool {
        // If known opaque due to background clear color and being the first slice.
        // The background_color will only be Some(..) if this is the first slice.
        match self.background_color {
            Some(color) => color.a >= 1.0,
            None => false
        }
    }

    /// Get the tile coordinates for a given rectangle.
    fn get_tile_coords_for_rect(
        &self,
        rect: &PictureRect,
    ) -> (TileOffset, TileOffset) {
        // Get the tile coordinates in the picture space.
        let mut p0 = TileOffset::new(
            (rect.origin.x / self.tile_size.width).floor() as i32,
            (rect.origin.y / self.tile_size.height).floor() as i32,
        );

        let mut p1 = TileOffset::new(
            ((rect.origin.x + rect.size.width) / self.tile_size.width).ceil() as i32,
            ((rect.origin.y + rect.size.height) / self.tile_size.height).ceil() as i32,
        );

        // Clamp the tile coordinates here to avoid looping over irrelevant tiles later on.
        p0.x = clamp(p0.x, self.tile_bounds_p0.x, self.tile_bounds_p1.x);
        p0.y = clamp(p0.y, self.tile_bounds_p0.y, self.tile_bounds_p1.y);
        p1.x = clamp(p1.x, self.tile_bounds_p0.x, self.tile_bounds_p1.x);
        p1.y = clamp(p1.y, self.tile_bounds_p0.y, self.tile_bounds_p1.y);

        (p0, p1)
    }

    /// Update transforms, opacity bindings and tile rects.
    pub fn pre_update(
        &mut self,
        pic_rect: PictureRect,
        surface_index: SurfaceIndex,
        frame_context: &FrameVisibilityContext,
        frame_state: &mut FrameVisibilityState,
    ) -> WorldRect {
        self.surface_index = surface_index;
        self.local_rect = pic_rect;
        self.local_clip_rect = PictureRect::max_rect();

        // Reset the opaque rect + subpixel mode, as they are calculated
        // during the prim dependency checks.
        self.backdrop = BackdropInfo::empty();
        self.subpixel_mode = SubpixelMode::Allow;

        self.map_local_to_surface = SpaceMapper::new(
            self.spatial_node_index,
            PictureRect::from_untyped(&pic_rect.to_untyped()),
        );
        self.map_child_pic_to_surface = SpaceMapper::new(
            self.spatial_node_index,
            PictureRect::from_untyped(&pic_rect.to_untyped()),
        );

        let pic_to_world_mapper = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            self.spatial_node_index,
            frame_context.global_screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        // If there is a valid set of shared clips, build a clip chain instance for this,
        // which will provide a local clip rect. This is useful for establishing things
        // like whether the backdrop rect supplied by Gecko can be considered opaque.
        if self.shared_clip_chain != ClipChainId::NONE {
            let mut shared_clips = Vec::new();
            let mut current_clip_chain_id = self.shared_clip_chain;
            while current_clip_chain_id != ClipChainId::NONE {
                shared_clips.push(current_clip_chain_id);
                let clip_chain_node = &frame_state.clip_store.clip_chain_nodes[current_clip_chain_id.0 as usize];
                current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
            }

            frame_state.clip_store.set_active_clips(
                LayoutRect::max_rect(),
                self.spatial_node_index,
                &shared_clips,
                frame_context.clip_scroll_tree,
                &mut frame_state.data_stores.clip,
            );

            let clip_chain_instance = frame_state.clip_store.build_clip_chain_instance(
                LayoutRect::from_untyped(&pic_rect.to_untyped()),
                &self.map_local_to_surface,
                &pic_to_world_mapper,
                frame_context.clip_scroll_tree,
                frame_state.gpu_cache,
                frame_state.resource_cache,
                frame_context.global_device_pixel_scale,
                &frame_context.global_screen_world_rect,
                &mut frame_state.data_stores.clip,
                true,
                false,
            );

            // Ensure that if the entire picture cache is clipped out, the local
            // clip rect is zero. This makes sure we don't register any occluders
            // that are actually off-screen.
            self.local_clip_rect = clip_chain_instance.map_or(PictureRect::zero(), |clip_chain_instance| {
                clip_chain_instance.pic_clip_rect
            });
        }

        // If there are pending retained state, retrieve it.
        if let Some(prev_state) = frame_state.retained_tiles.caches.remove(&self.slice) {
            self.tiles.extend(prev_state.tiles);
            self.root_transform = prev_state.root_transform;
            self.spatial_nodes = prev_state.spatial_nodes;
            self.opacity_bindings = prev_state.opacity_bindings;
            self.current_tile_size = prev_state.current_tile_size;
            self.native_surface_id = prev_state.native_surface_id;
            self.is_opaque = prev_state.is_opaque;

            fn recycle_map<K: std::cmp::Eq + std::hash::Hash, V>(
                ideal_len: usize,
                dest: &mut FastHashMap<K, V>,
                src: FastHashMap<K, V>,
            ) {
                if dest.capacity() < src.capacity() {
                    if src.capacity() < 3 * ideal_len {
                        *dest = src;
                    } else {
                        dest.clear();
                        dest.reserve(ideal_len);
                    }
                }
            }
            recycle_map(
                self.tiles.len(),
                &mut self.old_tiles,
                prev_state.allocations.old_tiles,
            );
            recycle_map(
                self.opacity_bindings.len(),
                &mut self.old_opacity_bindings,
                prev_state.allocations.old_opacity_bindings,
            );
            recycle_map(
                prev_state.allocations.compare_cache.len(),
                &mut self.compare_cache,
                prev_state.allocations.compare_cache,
            );
        }

        // Only evaluate what tile size to use fairly infrequently, so that we don't end
        // up constantly invalidating and reallocating tiles if the picture rect size is
        // changing near a threshold value.
        if self.frames_until_size_eval == 0 ||
           self.tile_size_override != frame_context.config.tile_size_override {
            const TILE_SIZE_TINY: f32 = 32.0;

            // Work out what size tile is appropriate for this picture cache.
            let desired_tile_size = match frame_context.config.tile_size_override {
                Some(tile_size_override) => {
                    tile_size_override
                }
                None => {
                    // There's no need to check the other dimension. If we encounter a picture
                    // that is small on one dimension, it's a reasonable choice to use a scrollbar
                    // sized tile configuration regardless of the other dimension.
                    if pic_rect.size.width <= TILE_SIZE_TINY {
                        TILE_SIZE_SCROLLBAR_VERTICAL
                    } else if pic_rect.size.height <= TILE_SIZE_TINY {
                        TILE_SIZE_SCROLLBAR_HORIZONTAL
                    } else {
                        TILE_SIZE_DEFAULT
                    }
                }
            };

            // If the desired tile size has changed, then invalidate and drop any
            // existing tiles.
            if desired_tile_size != self.current_tile_size {
                // Destroy any native surfaces on the tiles that will be dropped due
                // to resizing.
                if let Some(native_surface_id) = self.native_surface_id.take() {
                    frame_state.resource_cache.destroy_compositor_surface(native_surface_id);
                }
                self.tiles.clear();
                self.current_tile_size = desired_tile_size;
            }

            // Reset counter until next evaluating the desired tile size. This is an
            // arbitrary value.
            self.frames_until_size_eval = 120;
            self.tile_size_override = frame_context.config.tile_size_override;
        }

        // Map an arbitrary point in picture space to world space, to work out
        // what the fractional translation is that's applied by this scroll root.
        // TODO(gw): I'm not 100% sure this is right. At least, in future, we should
        //           make a specific API for this, and/or enforce that the picture
        //           cache transform only includes scale and/or translation (we
        //           already ensure it doesn't have perspective).
        let world_origin = pic_to_world_mapper
            .map(&PictureRect::new(PicturePoint::zero(), PictureSize::new(1.0, 1.0)))
            .expect("bug: unable to map origin to world space")
            .origin;

        // Get the desired integer device coordinate
        let device_origin = world_origin * frame_context.global_device_pixel_scale;
        let desired_device_origin = device_origin.round();
        self.device_position = desired_device_origin;

        // Unmap from device space to world space rect
        let ref_world_rect = WorldRect::new(
            desired_device_origin / frame_context.global_device_pixel_scale,
            WorldSize::new(1.0, 1.0),
        );

        // Unmap from world space to picture space
        let ref_point = pic_to_world_mapper
            .unmap(&ref_world_rect)
            .expect("bug: unable to unmap ref world rect")
            .origin;

        // Extract the fractional offset required in picture space to align in device space
        self.fract_offset = PictureVector2D::new(
            ref_point.x.fract(),
            ref_point.y.fract(),
        );

        // Do a hacky diff of opacity binding values from the last frame. This is
        // used later on during tile invalidation tests.
        let current_properties = frame_context.scene_properties.float_properties();
        mem::swap(&mut self.opacity_bindings, &mut self.old_opacity_bindings);

        self.opacity_bindings.clear();
        for (id, value) in current_properties {
            let changed = match self.old_opacity_bindings.get(id) {
                Some(old_property) => !old_property.value.approx_eq(value),
                None => true,
            };
            self.opacity_bindings.insert(*id, OpacityBindingInfo {
                value: *value,
                changed,
            });
        }

        let world_tile_size = WorldSize::new(
            self.current_tile_size.width as f32 / frame_context.global_device_pixel_scale.0,
            self.current_tile_size.height as f32 / frame_context.global_device_pixel_scale.0,
        );

        // We know that this is an exact rectangle, since we (for now) only support tile
        // caches where the scroll root is in the root coordinate system.
        let local_tile_rect = pic_to_world_mapper
            .unmap(&WorldRect::new(WorldPoint::zero(), world_tile_size))
            .expect("bug: unable to get local tile rect");

        self.tile_size = local_tile_rect.size;

        let screen_rect_in_pic_space = pic_to_world_mapper
            .unmap(&frame_context.global_screen_world_rect)
            .expect("unable to unmap screen rect");

        // Inflate the needed rect a bit, so that we retain tiles that we have drawn
        // but have just recently gone off-screen. This means that we avoid re-drawing
        // tiles if the user is scrolling up and down small amounts, at the cost of
        // a bit of extra texture memory.
        let desired_rect_in_pic_space = screen_rect_in_pic_space
            .inflate(0.0, 3.0 * self.tile_size.height);

        let needed_rect_in_pic_space = desired_rect_in_pic_space
            .intersection(&pic_rect)
            .unwrap_or_else(PictureRect::zero);

        let p0 = needed_rect_in_pic_space.origin;
        let p1 = needed_rect_in_pic_space.bottom_right();

        let x0 = (p0.x / local_tile_rect.size.width).floor() as i32;
        let x1 = (p1.x / local_tile_rect.size.width).ceil() as i32;

        let y0 = (p0.y / local_tile_rect.size.height).floor() as i32;
        let y1 = (p1.y / local_tile_rect.size.height).ceil() as i32;

        let x_tiles = x1 - x0;
        let y_tiles = y1 - y0;
        self.tile_rect = TileRect::new(
            TileOffset::new(x0, y0),
            TileSize::new(x_tiles, y_tiles),
        );
        // This is duplicated information from tile_rect, but cached here to avoid
        // redundant calculations during get_tile_coords_for_rect
        self.tile_bounds_p0 = TileOffset::new(x0, y0);
        self.tile_bounds_p1 = TileOffset::new(x1, y1);

        let mut world_culling_rect = WorldRect::zero();

        mem::swap(&mut self.tiles, &mut self.old_tiles);

        let ctx = TilePreUpdateContext {
            local_rect: self.local_rect,
            local_clip_rect: self.local_clip_rect,
            pic_to_world_mapper,
            fract_offset: self.fract_offset,
            background_color: self.background_color,
            global_screen_world_rect: frame_context.global_screen_world_rect,
        };

        self.tiles.clear();
        for y in y0 .. y1 {
            for x in x0 .. x1 {
                let key = TileOffset::new(x, y);

                let mut tile = self.old_tiles
                    .remove(&key)
                    .unwrap_or_else(|| {
                        let next_id = TileId(NEXT_TILE_ID.fetch_add(1, Ordering::Relaxed));
                        Tile::new(next_id)
                    });

                // Ensure each tile is offset by the appropriate amount from the
                // origin, such that the content origin will be a whole number and
                // the snapping will be consistent.
                let rect = PictureRect::new(
                    PicturePoint::new(
                        x as f32 * self.tile_size.width + self.fract_offset.x,
                        y as f32 * self.tile_size.height + self.fract_offset.y,
                    ),
                    self.tile_size,
                );

                tile.pre_update(
                    rect,
                    &ctx,
                );

                // Only include the tiles that are currently in view into the world culling
                // rect. This is a very important optimization for a couple of reasons:
                // (1) Primitives that intersect with tiles in the grid that are not currently
                //     visible can be skipped from primitive preparation, clip chain building
                //     and tile dependency updates.
                // (2) When we need to allocate an off-screen surface for a child picture (for
                //     example a CSS filter) we clip the size of the GPU surface to the world
                //     culling rect below (to ensure we draw enough of it to be sampled by any
                //     tiles that reference it). Making the world culling rect only affected
                //     by visible tiles (rather than the entire virtual tile display port) can
                //     result in allocating _much_ smaller GPU surfaces for cases where the
                //     true off-screen surface size is very large.
                if tile.is_visible {
                    world_culling_rect = world_culling_rect.union(&tile.world_rect);
                }

                self.tiles.insert(key, tile);
            }
        }

        // Any old tiles that remain after the loop above are going to be dropped. For
        // simple composite mode, the texture cache handle will expire and be collected
        // by the texture cache. For native compositor mode, we need to explicitly
        // invoke a callback to the client to destroy that surface.
        frame_state.composite_state.destroy_native_tiles(
            self.old_tiles.values_mut(),
            frame_state.resource_cache,
        );

        world_culling_rect
    }

    /// Update the dependencies for each tile for a given primitive instance.
    pub fn update_prim_dependencies(
        &mut self,
        prim_instance: &PrimitiveInstance,
        prim_spatial_node_index: SpatialNodeIndex,
        prim_clip_chain: Option<&ClipChainInstance>,
        local_prim_rect: LayoutRect,
        clip_scroll_tree: &ClipScrollTree,
        data_stores: &DataStores,
        clip_store: &ClipStore,
        pictures: &[PicturePrimitive],
        resource_cache: &ResourceCache,
        opacity_binding_store: &OpacityBindingStorage,
        image_instances: &ImageInstanceStorage,
        surface_index: SurfaceIndex,
        surface_spatial_node_index: SpatialNodeIndex,
    ) -> bool {
        // If the primitive is completely clipped out by the clip chain, there
        // is no need to add it to any primitive dependencies.
        let prim_clip_chain = match prim_clip_chain {
            Some(prim_clip_chain) => prim_clip_chain,
            None => return false,
        };

        self.map_local_to_surface.set_target_spatial_node(
            prim_spatial_node_index,
            clip_scroll_tree,
        );

        // Map the primitive local rect into picture space.
        let prim_rect = match self.map_local_to_surface.map(&local_prim_rect) {
            Some(rect) => rect,
            None => return false,
        };

        // If the rect is invalid, no need to create dependencies.
        if prim_rect.size.is_empty_or_negative() {
            return false;
        }

        // If the primitive is directly drawn onto this picture cache surface, then
        // the pic_clip_rect is in the same space. If not, we need to map it from
        // the surface space into the picture cache space.
        let on_picture_surface = surface_index == self.surface_index;
        let pic_clip_rect = if on_picture_surface {
            prim_clip_chain.pic_clip_rect
        } else {
            self.map_child_pic_to_surface.set_target_spatial_node(
                surface_spatial_node_index,
                clip_scroll_tree,
            );
            self.map_child_pic_to_surface
                .map(&prim_clip_chain.pic_clip_rect)
                .expect("bug: unable to map clip rect to picture cache space")
        };

        // Get the tile coordinates in the picture space.
        let (p0, p1) = self.get_tile_coords_for_rect(&pic_clip_rect);

        // If the primitive is outside the tiling rects, it's known to not
        // be visible.
        if p0.x == p1.x || p0.y == p1.y {
            return false;
        }

        // Build the list of resources that this primitive has dependencies on.
        let mut prim_info = PrimitiveDependencyInfo::new(
            prim_instance.uid(),
            prim_rect.origin,
            pic_clip_rect,
        );

        // Include the prim spatial node, if differs relative to cache root.
        if prim_spatial_node_index != self.spatial_node_index {
            prim_info.spatial_nodes.push(prim_spatial_node_index);
        }

        // If there was a clip chain, add any clip dependencies to the list for this tile.
        let clip_instances = &clip_store
            .clip_node_instances[prim_clip_chain.clips_range.to_range()];
        for clip_instance in clip_instances {
            prim_info.clips.push(clip_instance.handle.uid());

            // If the clip has the same spatial node, the relative transform
            // will always be the same, so there's no need to depend on it.
            let clip_node = &data_stores.clip[clip_instance.handle];
            if clip_node.item.spatial_node_index != self.spatial_node_index
                && !prim_info.spatial_nodes.contains(&clip_node.item.spatial_node_index) {
                prim_info.spatial_nodes.push(clip_node.item.spatial_node_index);
            }
        }

        // Certain primitives may select themselves to be a backdrop candidate, which is
        // then applied below.
        let mut backdrop_candidate = None;

        // For pictures, we don't (yet) know the valid clip rect, so we can't correctly
        // use it to calculate the local bounding rect for the tiles. If we include them
        // then we may calculate a bounding rect that is too large, since it won't include
        // the clip bounds of the picture. Excluding them from the bounding rect here
        // fixes any correctness issues (the clips themselves are considered when we
        // consider the bounds of the primitives that are *children* of the picture),
        // however it does potentially result in some un-necessary invalidations of a
        // tile (in cases where the picture local rect affects the tile, but the clip
        // rect eventually means it doesn't affect that tile).
        // TODO(gw): Get picture clips earlier (during the initial picture traversal
        //           pass) so that we can calculate these correctly.
        match prim_instance.kind {
            PrimitiveInstanceKind::Picture { pic_index,.. } => {
                // Pictures can depend on animated opacity bindings.
                let pic = &pictures[pic_index.0];
                if let Some(PictureCompositeMode::Filter(Filter::Opacity(binding, _))) = pic.requested_composite_mode {
                    prim_info.opacity_bindings.push(binding.into());
                }
            }
            PrimitiveInstanceKind::Rectangle { data_handle, opacity_binding_index, .. } => {
                if opacity_binding_index == OpacityBindingIndex::INVALID {
                    // Rectangles can only form a backdrop candidate if they are known opaque.
                    // TODO(gw): We could resolve the opacity binding here, but the common
                    //           case for background rects is that they don't have animated opacity.
                    let color = match data_stores.prim[data_handle].kind {
                        PrimitiveTemplateKind::Rectangle { color, .. } => color,
                        _ => unreachable!(),
                    };
                    if color.a >= 1.0 {
                        backdrop_candidate = Some(BackdropKind::Color { color });
                    }
                } else {
                    let opacity_binding = &opacity_binding_store[opacity_binding_index];
                    for binding in &opacity_binding.bindings {
                        prim_info.opacity_bindings.push(OpacityBinding::from(*binding));
                    }
                }

                prim_info.clip_by_tile = true;
            }
            PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
                let image_data = &data_stores.image[data_handle].kind;
                let image_instance = &image_instances[image_instance_index];
                let opacity_binding_index = image_instance.opacity_binding_index;

                if opacity_binding_index == OpacityBindingIndex::INVALID {
                    if let Some(image_properties) = resource_cache.get_image_properties(image_data.key) {
                        // If this image is opaque, it can be considered as a possible opaque backdrop
                        if image_properties.descriptor.is_opaque() {
                            backdrop_candidate = Some(BackdropKind::Image);
                        }
                    }
                } else {
                    let opacity_binding = &opacity_binding_store[opacity_binding_index];
                    for binding in &opacity_binding.bindings {
                        prim_info.opacity_bindings.push(OpacityBinding::from(*binding));
                    }
                }

                prim_info.images.push(ImageDependency {
                    key: image_data.key,
                    generation: resource_cache.get_image_generation(image_data.key),
                });
            }
            PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                let yuv_image_data = &data_stores.yuv_image[data_handle].kind;
                prim_info.images.extend(
                    yuv_image_data.yuv_key.iter().map(|key| {
                        ImageDependency {
                            key: *key,
                            generation: resource_cache.get_image_generation(*key),
                        }
                    })
                );
            }
            PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                let border_data = &data_stores.image_border[data_handle].kind;
                prim_info.images.push(ImageDependency {
                    key: border_data.request.key,
                    generation: resource_cache.get_image_generation(border_data.request.key),
                });
            }
            PrimitiveInstanceKind::PushClipChain |
            PrimitiveInstanceKind::PopClipChain => {
                // Early exit to ensure this doesn't get added as a dependency on the tile.
                return false;
            }
            PrimitiveInstanceKind::TextRun { data_handle, .. } => {
                // Only do these checks if we haven't already disabled subpx
                // text rendering for this slice.
                if self.subpixel_mode == SubpixelMode::Allow && !self.is_opaque() {
                    let run_data = &data_stores.text_run[data_handle];

                    // Only care about text runs that have requested subpixel rendering.
                    // This is conservative - it may still end up that a subpx requested
                    // text run doesn't get subpx for other reasons (e.g. glyph size).
                    let subpx_requested = match run_data.font.render_mode {
                        FontRenderMode::Subpixel => true,
                        FontRenderMode::Alpha | FontRenderMode::Mono => false,
                    };

                    // If a text run is on a child surface, the subpx mode will be
                    // correctly determined as we recurse through pictures in take_context.
                    if on_picture_surface
                        && subpx_requested
                        && !self.backdrop.rect.contains_rect(&pic_clip_rect) {
                        self.subpixel_mode = SubpixelMode::Deny;
                    }
                }
            }
            PrimitiveInstanceKind::Clear { .. } => {
                backdrop_candidate = Some(BackdropKind::Clear);
            }
            PrimitiveInstanceKind::LineDecoration { .. } |
            PrimitiveInstanceKind::NormalBorder { .. } |
            PrimitiveInstanceKind::LinearGradient { .. } |
            PrimitiveInstanceKind::RadialGradient { .. } |
            PrimitiveInstanceKind::Backdrop { .. } => {
                // These don't contribute dependencies
            }
        };

        // If this primitive considers itself a backdrop candidate, apply further
        // checks to see if it matches all conditions to be a backdrop.
        if let Some(backdrop_candidate) = backdrop_candidate {
            let is_suitable_backdrop = match backdrop_candidate {
                BackdropKind::Clear => {
                    // Clear prims are special - they always end up in their own slice,
                    // and always set the backdrop. In future, we hope to completely
                    // remove clear prims, since they don't integrate with the compositing
                    // system cleanly.
                    true
                }
                BackdropKind::Image | BackdropKind::Color { .. } => {
                    // Check a number of conditions to see if we can consider this
                    // primitive as an opaque backdrop rect. Several of these are conservative
                    // checks and could be relaxed in future. However, these checks
                    // are quick and capture the common cases of background rects and images.
                    // Specifically, we currently require:
                    //  - The primitive is on the main picture cache surface.
                    //  - Same coord system as picture cache (ensures rects are axis-aligned).
                    //  - No clip masks exist.
                    let same_coord_system = {
                        let prim_spatial_node = &clip_scroll_tree
                            .spatial_nodes[prim_spatial_node_index.0 as usize];
                        let surface_spatial_node = &clip_scroll_tree
                            .spatial_nodes[self.spatial_node_index.0 as usize];

                        prim_spatial_node.coordinate_system_id == surface_spatial_node.coordinate_system_id
                    };

                    same_coord_system && on_picture_surface
                }
            };

            if is_suitable_backdrop
                && !prim_clip_chain.needs_mask
                && pic_clip_rect.contains_rect(&self.backdrop.rect) {
                self.backdrop = BackdropInfo {
                    rect: pic_clip_rect,
                    kind: backdrop_candidate,
                };
            }
        }

        // Record any new spatial nodes in the used list.
        self.used_spatial_nodes.extend(&prim_info.spatial_nodes);

        // Truncate the lengths of dependency arrays to the max size we can handle.
        // Any arrays this size or longer will invalidate every frame.
        prim_info.clips.truncate(MAX_PRIM_SUB_DEPS);
        prim_info.opacity_bindings.truncate(MAX_PRIM_SUB_DEPS);
        prim_info.spatial_nodes.truncate(MAX_PRIM_SUB_DEPS);
        prim_info.images.truncate(MAX_PRIM_SUB_DEPS);

        // Normalize the tile coordinates before adding to tile dependencies.
        // For each affected tile, mark any of the primitive dependencies.
        for y in p0.y .. p1.y {
            for x in p0.x .. p1.x {
                // TODO(gw): Convert to 2d array temporarily to avoid hash lookups per-tile?
                let key = TileOffset::new(x, y);
                let tile = self.tiles.get_mut(&key).expect("bug: no tile");

                tile.add_prim_dependency(&prim_info);
            }
        }

        true
    }

    /// Print debug information about this picture cache to a tree printer.
    fn print(&self) {
        // TODO(gw): This initial implementation is very basic - just printing
        //           the picture cache state to stdout. In future, we can
        //           make this dump each frame to a file, and produce a report
        //           stating which frames had invalidations. This will allow
        //           diff'ing the invalidation states in a visual tool.
        let mut pt = PrintTree::new("Picture Cache");

        pt.new_level(format!("Slice {}", self.slice));

        pt.add_item(format!("fract_offset: {:?}", self.fract_offset));
        pt.add_item(format!("background_color: {:?}", self.background_color));

        for y in self.tile_bounds_p0.y .. self.tile_bounds_p1.y {
            for x in self.tile_bounds_p0.x .. self.tile_bounds_p1.x {
                let key = TileOffset::new(x, y);
                let tile = &self.tiles[&key];
                tile.print(&mut pt);
            }
        }

        pt.end_level();
    }

    /// Apply any updates after prim dependency updates. This applies
    /// any late tile invalidations, and sets up the dirty rect and
    /// set of tile blits.
    pub fn post_update(
        &mut self,
        frame_context: &FrameVisibilityContext,
        frame_state: &mut FrameVisibilityState,
    ) {
        self.tiles_to_draw.clear();
        self.dirty_region.clear();

        // Register the opaque region of this tile cache as an occluder, which
        // is used later in the frame to occlude other tiles.
        if self.backdrop.rect.is_well_formed_and_nonempty() {
            let backdrop_rect = self.backdrop.rect
                .intersection(&self.local_rect)
                .and_then(|r| {
                    r.intersection(&self.local_clip_rect)
                });

            if let Some(backdrop_rect) = backdrop_rect {
                let map_pic_to_world = SpaceMapper::new_with_target(
                    ROOT_SPATIAL_NODE_INDEX,
                    self.spatial_node_index,
                    frame_context.global_screen_world_rect,
                    frame_context.clip_scroll_tree,
                );

                let world_backdrop_rect = map_pic_to_world
                    .map(&backdrop_rect)
                    .expect("bug: unable to map backdrop to world space");

                frame_state.composite_state.register_occluder(
                    self.slice,
                    world_backdrop_rect,
                );
            }
        }

        // Detect if the picture cache was scrolled or scaled. In this case,
        // the device space dirty rects aren't applicable (until we properly
        // integrate with OS compositors that can handle scrolling slices).
        let root_transform = frame_context
            .clip_scroll_tree
            .get_relative_transform(
                self.spatial_node_index,
                ROOT_SPATIAL_NODE_INDEX,
            )
            .into();
        let root_transform_changed = root_transform != self.root_transform;
        if root_transform_changed {
            self.root_transform = root_transform;
            frame_state.composite_state.dirty_rects_are_valid = false;
        }

        // Diff the state of the spatial nodes between last frame build and now.
        mem::swap(&mut self.spatial_nodes, &mut self.old_spatial_nodes);

        // TODO(gw): Maybe remove the used_spatial_nodes set and just mutate / create these
        //           diffs inside add_prim_dependency?
        self.spatial_nodes.clear();
        for spatial_node_index in self.used_spatial_nodes.drain() {
            // Get the current relative transform.
            let mut value = get_transform_key(
                spatial_node_index,
                self.spatial_node_index,
                frame_context.clip_scroll_tree,
            );

            // Check if the transform has changed from last frame
            let mut changed = true;
            if let Some(old_info) = self.old_spatial_nodes.remove(&spatial_node_index) {
                if old_info.value == value {
                    // Since the transform key equality check applies epsilon, if we
                    // consider the value to be the same, store that old value to avoid
                    // missing very slow drifts in the value over time.
                    // TODO(gw): We should change ComparableVec to use a trait for comparison
                    //           rather than PartialEq.
                    value = old_info.value;
                    changed = false;
                }
            }

            self.spatial_nodes.insert(spatial_node_index, SpatialNodeDependency {
                changed,
                value,
            });
        }

        let ctx = TilePostUpdateContext {
            backdrop: self.backdrop,
            spatial_nodes: &self.spatial_nodes,
            opacity_bindings: &self.opacity_bindings,
            current_tile_size: self.current_tile_size,
        };

        let mut state = TilePostUpdateState {
            resource_cache: frame_state.resource_cache,
            composite_state: frame_state.composite_state,
            compare_cache: &mut self.compare_cache,
        };

        // Step through each tile and invalidate if the dependencies have changed. Determine
        // the current opacity setting and whether it's changed.
        let mut tile_cache_is_opaque = true;
        for (key, tile) in self.tiles.iter_mut() {
            if tile.post_update(&ctx, &mut state) {
                self.tiles_to_draw.push(*key);
                tile_cache_is_opaque &= tile.is_opaque;
            }
        }

        // If opacity changed, the native compositor surface and all tiles get invalidated.
        // (this does nothing if not using native compositor mode).
        // TODO(gw): This property probably changes very rarely, so it is OK to invalidate
        //           everything in this case. If it turns out that this isn't true, we could
        //           consider other options, such as per-tile opacity (natively supported
        //           on CoreAnimation, and supported if backed by non-virtual surfaces in
        //           DirectComposition).
        if self.is_opaque != tile_cache_is_opaque {
            if let Some(native_surface_id) = self.native_surface_id.take() {
                // Since the native surface will be destroyed, need to clear the compositor tile
                // handle for all tiles. This means the tiles will be reallocated on demand
                // when the tiles are added to render tasks.
                for tile in self.tiles.values_mut() {
                    if let Some(TileSurface::Texture { descriptor: SurfaceTextureDescriptor::Native { ref mut id, .. }, .. }) = tile.surface {
                        *id = None;
                    }
                    // Invalidate the entire tile to force a redraw.
                    tile.invalidate(None, InvalidationReason::SurfaceOpacityChanged);
                }
                // Destroy the compositor surface. It will be reallocated with the correct
                // opacity flag when render tasks are generated for tiles.
                frame_state.resource_cache.destroy_compositor_surface(native_surface_id);
            }
            self.is_opaque = tile_cache_is_opaque;
        }

        // When under test, record a copy of the dirty region to support
        // invalidation testing in wrench.
        if frame_context.config.testing {
            frame_state.scratch.recorded_dirty_regions.push(self.dirty_region.record());
        }
    }
}

/// Maintains a stack of picture and surface information, that
/// is used during the initial picture traversal.
pub struct PictureUpdateState<'a> {
    surfaces: &'a mut Vec<SurfaceInfo>,
    surface_stack: Vec<SurfaceIndex>,
    picture_stack: Vec<PictureInfo>,
    are_raster_roots_assigned: bool,
    composite_state: &'a CompositeState,
}

impl<'a> PictureUpdateState<'a> {
    pub fn update_all(
        surfaces: &'a mut Vec<SurfaceInfo>,
        pic_index: PictureIndex,
        picture_primitives: &mut [PicturePrimitive],
        frame_context: &FrameBuildingContext,
        gpu_cache: &mut GpuCache,
        clip_store: &ClipStore,
        data_stores: &mut DataStores,
        composite_state: &CompositeState,
    ) {
        profile_marker!("UpdatePictures");

        let mut state = PictureUpdateState {
            surfaces,
            surface_stack: vec![SurfaceIndex(0)],
            picture_stack: Vec::new(),
            are_raster_roots_assigned: true,
            composite_state,
        };

        state.update(
            pic_index,
            picture_primitives,
            frame_context,
            gpu_cache,
            clip_store,
            data_stores,
        );

        if !state.are_raster_roots_assigned {
            state.assign_raster_roots(
                pic_index,
                picture_primitives,
                ROOT_SPATIAL_NODE_INDEX,
            );
        }
    }

    /// Return the current surface
    fn current_surface(&self) -> &SurfaceInfo {
        &self.surfaces[self.surface_stack.last().unwrap().0]
    }

    /// Return the current surface (mutable)
    fn current_surface_mut(&mut self) -> &mut SurfaceInfo {
        &mut self.surfaces[self.surface_stack.last().unwrap().0]
    }

    /// Push a new surface onto the update stack.
    fn push_surface(
        &mut self,
        surface: SurfaceInfo,
    ) -> SurfaceIndex {
        let surface_index = SurfaceIndex(self.surfaces.len());
        self.surfaces.push(surface);
        self.surface_stack.push(surface_index);
        surface_index
    }

    /// Pop a surface on the way up the picture traversal
    fn pop_surface(&mut self) -> SurfaceIndex{
        self.surface_stack.pop().unwrap()
    }

    /// Push information about a picture on the update stack
    fn push_picture(
        &mut self,
        info: PictureInfo,
    ) {
        self.picture_stack.push(info);
    }

    /// Pop the picture info off, on the way up the picture traversal
    fn pop_picture(
        &mut self,
    ) -> PictureInfo {
        self.picture_stack.pop().unwrap()
    }

    /// Update a picture, determining surface configuration,
    /// rasterization roots, and (in future) whether there
    /// are cached surfaces that can be used by this picture.
    fn update(
        &mut self,
        pic_index: PictureIndex,
        picture_primitives: &mut [PicturePrimitive],
        frame_context: &FrameBuildingContext,
        gpu_cache: &mut GpuCache,
        clip_store: &ClipStore,
        data_stores: &mut DataStores,
    ) {
        if let Some(prim_list) = picture_primitives[pic_index.0].pre_update(
            self,
            frame_context,
        ) {
            for cluster in &prim_list.clusters {
                if cluster.flags.contains(ClusterFlags::IS_PICTURE) {
                    for prim_instance in &cluster.prim_instances {
                        let child_pic_index = match prim_instance.kind {
                            PrimitiveInstanceKind::Picture { pic_index, .. } => pic_index,
                            _ => unreachable!(),
                        };

                        self.update(
                            child_pic_index,
                            picture_primitives,
                            frame_context,
                            gpu_cache,
                            clip_store,
                            data_stores,
                        );
                    }
                }
            }

            picture_primitives[pic_index.0].post_update(
                prim_list,
                self,
                frame_context,
                data_stores,
            );
        }
    }

    /// Process the picture tree again in a depth-first order,
    /// and adjust the raster roots of the pictures that want to establish
    /// their own roots but are not able to due to the size constraints.
    fn assign_raster_roots(
        &mut self,
        pic_index: PictureIndex,
        picture_primitives: &[PicturePrimitive],
        fallback_raster_spatial_node: SpatialNodeIndex,
    ) {
        let picture = &picture_primitives[pic_index.0];
        if !picture.is_visible() {
            return
        }

        let new_fallback = match picture.raster_config {
            Some(ref config) => {
                let surface = &mut self.surfaces[config.surface_index.0];
                if !config.establishes_raster_root {
                    surface.raster_spatial_node_index = fallback_raster_spatial_node;
                }
                surface.raster_spatial_node_index
            }
            None => fallback_raster_spatial_node,
        };

        for cluster in &picture.prim_list.clusters {
            if cluster.flags.contains(ClusterFlags::IS_PICTURE) {
                for instance in &cluster.prim_instances {
                    let child_pic_index = match instance.kind {
                        PrimitiveInstanceKind::Picture { pic_index, .. } => pic_index,
                        _ => unreachable!(),
                    };
                    self.assign_raster_roots(
                        child_pic_index,
                        picture_primitives,
                        new_fallback,
                    );
                }
            }
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct SurfaceIndex(pub usize);

pub const ROOT_SURFACE_INDEX: SurfaceIndex = SurfaceIndex(0);

#[derive(Debug, Copy, Clone)]
pub struct SurfaceRenderTasks {
    /// The root of the render task chain for this surface. This
    /// is attached to parent tasks, and also the surface that
    /// gets added during batching.
    pub root: RenderTaskId,
    /// The port of the render task change for this surface. This
    /// is where child tasks for this surface get attached to.
    pub port: RenderTaskId,
}

/// Information about an offscreen surface. For now,
/// it contains information about the size and coordinate
/// system of the surface. In the future, it will contain
/// information about the contents of the surface, which
/// will allow surfaces to be cached / retained between
/// frames and display lists.
#[derive(Debug)]
pub struct SurfaceInfo {
    /// A local rect defining the size of this surface, in the
    /// coordinate system of the surface itself.
    pub rect: PictureRect,
    /// Helper structs for mapping local rects in different
    /// coordinate systems into the surface coordinates.
    pub map_local_to_surface: SpaceMapper<LayoutPixel, PicturePixel>,
    /// Defines the positioning node for the surface itself,
    /// and the rasterization root for this surface.
    pub raster_spatial_node_index: SpatialNodeIndex,
    pub surface_spatial_node_index: SpatialNodeIndex,
    /// This is set when the render task is created.
    pub render_tasks: Option<SurfaceRenderTasks>,
    /// How much the local surface rect should be inflated (for blur radii).
    pub inflation_factor: f32,
    /// The device pixel ratio specific to this surface.
    pub device_pixel_scale: DevicePixelScale,
}

impl SurfaceInfo {
    pub fn new(
        surface_spatial_node_index: SpatialNodeIndex,
        raster_spatial_node_index: SpatialNodeIndex,
        inflation_factor: f32,
        world_rect: WorldRect,
        clip_scroll_tree: &ClipScrollTree,
        device_pixel_scale: DevicePixelScale,
    ) -> Self {
        let map_surface_to_world = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface_spatial_node_index,
            world_rect,
            clip_scroll_tree,
        );

        let pic_bounds = map_surface_to_world
            .unmap(&map_surface_to_world.bounds)
            .unwrap_or_else(PictureRect::max_rect);

        let map_local_to_surface = SpaceMapper::new(
            surface_spatial_node_index,
            pic_bounds,
        );

        SurfaceInfo {
            rect: PictureRect::zero(),
            map_local_to_surface,
            render_tasks: None,
            raster_spatial_node_index,
            surface_spatial_node_index,
            inflation_factor,
            device_pixel_scale,
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct RasterConfig {
    /// How this picture should be composited into
    /// the parent surface.
    pub composite_mode: PictureCompositeMode,
    /// Index to the surface descriptor for this
    /// picture.
    pub surface_index: SurfaceIndex,
    /// Whether this picture establishes a rasterization root.
    pub establishes_raster_root: bool,
}

bitflags! {
    /// A set of flags describing why a picture may need a backing surface.
    #[cfg_attr(feature = "capture", derive(Serialize))]
    pub struct BlitReason: u32 {
        /// Mix-blend-mode on a child that requires isolation.
        const ISOLATE = 1;
        /// Clip node that _might_ require a surface.
        const CLIP = 2;
        /// Preserve-3D requires a surface for plane-splitting.
        const PRESERVE3D = 4;
        /// A backdrop that is reused which requires a surface.
        const BACKDROP = 8;
    }
}

/// Specifies how this Picture should be composited
/// onto the target it belongs to.
#[allow(dead_code)]
#[derive(Debug, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub enum PictureCompositeMode {
    /// Apply CSS mix-blend-mode effect.
    MixBlend(MixBlendMode),
    /// Apply a CSS filter (except component transfer).
    Filter(Filter),
    /// Apply a component transfer filter.
    ComponentTransferFilter(FilterDataHandle),
    /// Draw to intermediate surface, copy straight across. This
    /// is used for CSS isolation, and plane splitting.
    Blit(BlitReason),
    /// Used to cache a picture as a series of tiles.
    TileCache {
    },
    /// Apply an SVG filter
    SvgFilter(Vec<FilterPrimitive>, Vec<SFilterData>),
}

impl PictureCompositeMode {
    pub fn inflate_picture_rect(&self, picture_rect: PictureRect, inflation_factor: f32) -> PictureRect {
        let mut result_rect = picture_rect;
        match self {
            PictureCompositeMode::Filter(filter) => match filter {
                Filter::Blur(_) => {
                    result_rect = picture_rect.inflate(inflation_factor, inflation_factor);
                },
                Filter::DropShadows(shadows) => {
                    let mut max_inflation: f32 = 0.0;
                    for shadow in shadows {
                        let inflation_factor = shadow.blur_radius.round() * BLUR_SAMPLE_SCALE;
                        max_inflation = max_inflation.max(inflation_factor);
                    }
                    result_rect = picture_rect.inflate(max_inflation, max_inflation);
                },
                _ => {}
            }
            PictureCompositeMode::SvgFilter(primitives, _) => {
                let mut output_rects = Vec::with_capacity(primitives.len());
                for (cur_index, primitive) in primitives.iter().enumerate() {
                    let output_rect = match primitive.kind {
                        FilterPrimitiveKind::Blur(ref primitive) => {
                            let input = primitive.input.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect);
                            let inflation_factor = primitive.radius.round() * BLUR_SAMPLE_SCALE;
                            input.inflate(inflation_factor, inflation_factor)
                        }
                        FilterPrimitiveKind::DropShadow(ref primitive) => {
                            let inflation_factor = primitive.shadow.blur_radius.round() * BLUR_SAMPLE_SCALE;
                            let input = primitive.input.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect);
                            let shadow_rect = input.inflate(inflation_factor, inflation_factor);
                            input.union(&shadow_rect.translate(primitive.shadow.offset * Scale::new(1.0)))
                        }
                        FilterPrimitiveKind::Blend(ref primitive) => {
                            primitive.input1.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect)
                                .union(&primitive.input2.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect))
                        }
                        FilterPrimitiveKind::Composite(ref primitive) => {
                            primitive.input1.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect)
                                .union(&primitive.input2.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect))
                        }
                        FilterPrimitiveKind::Identity(ref primitive) =>
                            primitive.input.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect),
                        FilterPrimitiveKind::Opacity(ref primitive) =>
                            primitive.input.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect),
                        FilterPrimitiveKind::ColorMatrix(ref primitive) =>
                            primitive.input.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect),
                        FilterPrimitiveKind::ComponentTransfer(ref primitive) =>
                            primitive.input.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect),
                        FilterPrimitiveKind::Offset(ref primitive) => {
                            let input_rect = primitive.input.to_index(cur_index).map(|index| output_rects[index]).unwrap_or(picture_rect);
                            input_rect.translate(primitive.offset * Scale::new(1.0))
                        },

                        FilterPrimitiveKind::Flood(..) => picture_rect,
                    };
                    output_rects.push(output_rect);
                    result_rect = result_rect.union(&output_rect);
                }
            }
            _ => {},
        }
        result_rect
    }
}

/// Enum value describing the place of a picture in a 3D context.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub enum Picture3DContext<C> {
    /// The picture is not a part of 3D context sub-hierarchy.
    Out,
    /// The picture is a part of 3D context.
    In {
        /// Additional data per child for the case of this a root of 3D hierarchy.
        root_data: Option<Vec<C>>,
        /// The spatial node index of an "ancestor" element, i.e. one
        /// that establishes the transformed element’s containing block.
        ///
        /// See CSS spec draft for more details:
        /// https://drafts.csswg.org/css-transforms-2/#accumulated-3d-transformation-matrix-computation
        ancestor_index: SpatialNodeIndex,
    },
}

/// Information about a preserve-3D hierarchy child that has been plane-split
/// and ordered according to the view direction.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct OrderedPictureChild {
    pub anchor: PlaneSplitAnchor,
    pub spatial_node_index: SpatialNodeIndex,
    pub gpu_address: GpuCacheAddress,
}

bitflags! {
    /// A set of flags describing why a picture may need a backing surface.
    #[cfg_attr(feature = "capture", derive(Serialize))]
    pub struct ClusterFlags: u32 {
        /// This cluster is a picture
        const IS_PICTURE = 1;
        /// Whether this cluster is visible when the position node is a backface.
        const IS_BACKFACE_VISIBLE = 2;
        /// This flag is set during the first pass picture traversal, depending on whether
        /// the cluster is visible or not. It's read during the second pass when primitives
        /// consult their owning clusters to see if the primitive itself is visible.
        const IS_VISIBLE = 4;
        /// Is a backdrop-filter cluster that requires special handling during post_update.
        const IS_BACKDROP_FILTER = 8;
        /// Force creation of a picture caching slice before this cluster.
        const CREATE_PICTURE_CACHE_PRE = 16;
        /// Force creation of a picture caching slice after this cluster.
        const CREATE_PICTURE_CACHE_POST = 32;
        /// If set, this cluster represents a scroll bar container.
        const SCROLLBAR_CONTAINER = 64;
        /// If set, this cluster contains clear rectangle primitives.
        const IS_CLEAR_PRIMITIVE = 128;
        /// This is used as a performance hint - this primitive may be promoted to a native
        /// compositor surface under certain (implementation specific) conditions. This
        /// is typically used for large videos, and canvas elements.
        const PREFER_COMPOSITOR_SURFACE = 256;
    }
}

/// Descriptor for a cluster of primitives. For now, this is quite basic but will be
/// extended to handle more spatial clustering of primitives.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PrimitiveCluster {
    /// The positioning node for this cluster.
    pub spatial_node_index: SpatialNodeIndex,
    /// The bounding rect of the cluster, in the local space of the spatial node.
    /// This is used to quickly determine the overall bounding rect for a picture
    /// during the first picture traversal, which is needed for local scale
    /// determination, and render task size calculations.
    bounding_rect: LayoutRect,
    /// The list of primitive instances in this cluster.
    pub prim_instances: Vec<PrimitiveInstance>,
    /// Various flags / state for this cluster.
    pub flags: ClusterFlags,
    /// An optional scroll root to use if this cluster establishes a picture cache slice.
    pub cache_scroll_root: Option<SpatialNodeIndex>,
}

/// Where to insert a prim instance in a primitive list.
#[derive(Debug, Copy, Clone)]
enum PrimitiveListPosition {
    Begin,
    End,
}

impl PrimitiveCluster {
    /// Construct a new primitive cluster for a given positioning node.
    fn new(
        spatial_node_index: SpatialNodeIndex,
        flags: ClusterFlags,
    ) -> Self {
        PrimitiveCluster {
            bounding_rect: LayoutRect::zero(),
            spatial_node_index,
            flags,
            prim_instances: Vec::new(),
            cache_scroll_root: None,
        }
    }

    /// Return true if this cluster is compatible with the given params
    pub fn is_compatible(
        &self,
        spatial_node_index: SpatialNodeIndex,
        flags: ClusterFlags,
    ) -> bool {
        self.flags == flags && self.spatial_node_index == spatial_node_index
    }

    /// Add a primitive instance to this cluster, at the start or end
    fn push(
        &mut self,
        prim_instance: PrimitiveInstance,
        prim_size: LayoutSize,
    ) {
        let prim_rect = LayoutRect::new(
            prim_instance.prim_origin,
            prim_size,
        );
        let culling_rect = prim_instance.local_clip_rect
            .intersection(&prim_rect)
            .unwrap_or_else(LayoutRect::zero);

        self.bounding_rect = self.bounding_rect.union(&culling_rect);
        self.prim_instances.push(prim_instance);
    }
}

/// A list of primitive instances that are added to a picture
/// This ensures we can keep a list of primitives that
/// are pictures, for a fast initial traversal of the picture
/// tree without walking the instance list.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PrimitiveList {
    /// List of primitives grouped into clusters.
    pub clusters: Vec<PrimitiveCluster>,
}

impl PrimitiveList {
    /// Construct an empty primitive list. This is
    /// just used during the take_context / restore_context
    /// borrow check dance, which will be removed as the
    /// picture traversal pass is completed.
    pub fn empty() -> Self {
        PrimitiveList {
            clusters: Vec::new(),
        }
    }

    /// Add a primitive instance to this list, at the start or end
    fn push(
        &mut self,
        prim_instance: PrimitiveInstance,
        prim_size: LayoutSize,
        spatial_node_index: SpatialNodeIndex,
        prim_flags: PrimitiveFlags,
        insert_position: PrimitiveListPosition,
    ) {
        let mut flags = ClusterFlags::empty();

        // Pictures are always put into a new cluster, to make it faster to
        // iterate all pictures in a given primitive list.
        match prim_instance.kind {
            PrimitiveInstanceKind::Picture { .. } => {
                flags.insert(ClusterFlags::IS_PICTURE);
            }
            PrimitiveInstanceKind::Backdrop { .. } => {
                flags.insert(ClusterFlags::IS_BACKDROP_FILTER);
            }
            PrimitiveInstanceKind::Clear { .. } => {
                flags.insert(ClusterFlags::IS_CLEAR_PRIMITIVE);
            }
            _ => {}
        }

        if prim_flags.contains(PrimitiveFlags::IS_BACKFACE_VISIBLE) {
            flags.insert(ClusterFlags::IS_BACKFACE_VISIBLE);
        }

        if prim_flags.contains(PrimitiveFlags::IS_SCROLLBAR_CONTAINER) {
            flags.insert(ClusterFlags::SCROLLBAR_CONTAINER);
        }

        if prim_flags.contains(PrimitiveFlags::PREFER_COMPOSITOR_SURFACE) {
            flags.insert(ClusterFlags::PREFER_COMPOSITOR_SURFACE);
        }

        // Insert the primitive into the first or last cluster as required
        match insert_position {
            PrimitiveListPosition::Begin => {
                let mut cluster = PrimitiveCluster::new(
                    spatial_node_index,
                    flags,
                );
                cluster.push(prim_instance, prim_size);
                self.clusters.insert(0, cluster);
            }
            PrimitiveListPosition::End => {
                if let Some(cluster) = self.clusters.last_mut() {
                    if cluster.is_compatible(spatial_node_index, flags) {
                        cluster.push(prim_instance, prim_size);
                        return;
                    }
                }

                let mut cluster = PrimitiveCluster::new(
                    spatial_node_index,
                    flags,
                );
                cluster.push(prim_instance, prim_size);
                self.clusters.push(cluster);
            }
        }
    }

    /// Add a primitive instance to the start of the list
    pub fn add_prim_to_start(
        &mut self,
        prim_instance: PrimitiveInstance,
        prim_size: LayoutSize,
        spatial_node_index: SpatialNodeIndex,
        flags: PrimitiveFlags,
    ) {
        self.push(
            prim_instance,
            prim_size,
            spatial_node_index,
            flags,
            PrimitiveListPosition::Begin,
        )
    }

    /// Add a primitive instance to the end of the list
    pub fn add_prim(
        &mut self,
        prim_instance: PrimitiveInstance,
        prim_size: LayoutSize,
        spatial_node_index: SpatialNodeIndex,
        flags: PrimitiveFlags,
    ) {
        self.push(
            prim_instance,
            prim_size,
            spatial_node_index,
            flags,
            PrimitiveListPosition::End,
        )
    }

    /// Returns true if there are no clusters (and thus primitives)
    pub fn is_empty(&self) -> bool {
        self.clusters.is_empty()
    }

    /// Add an existing cluster to this prim list
    pub fn add_cluster(&mut self, cluster: PrimitiveCluster) {
        self.clusters.push(cluster);
    }

    /// Merge another primitive list into this one
    pub fn extend(&mut self, prim_list: PrimitiveList) {
        self.clusters.extend(prim_list.clusters);
    }
}

/// Defines configuration options for a given picture primitive.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PictureOptions {
    /// If true, WR should inflate the bounding rect of primitives when
    /// using a filter effect that requires inflation.
    pub inflate_if_required: bool,
}

impl Default for PictureOptions {
    fn default() -> Self {
        PictureOptions {
            inflate_if_required: true,
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PicturePrimitive {
    /// List of primitives, and associated info for this picture.
    pub prim_list: PrimitiveList,

    #[cfg_attr(feature = "capture", serde(skip))]
    pub state: Option<PictureState>,

    /// If true, apply the local clip rect to primitive drawn
    /// in this picture.
    pub apply_local_clip_rect: bool,
    /// If false and transform ends up showing the back of the picture,
    /// it will be considered invisible.
    pub is_backface_visible: bool,

    // If a mix-blend-mode, contains the render task for
    // the readback of the framebuffer that we use to sample
    // from in the mix-blend-mode shader.
    // For drop-shadow filter, this will store the original
    // picture task which would be rendered on screen after
    // blur pass.
    pub secondary_render_task_id: Option<RenderTaskId>,
    /// How this picture should be composited.
    /// If None, don't composite - just draw directly on parent surface.
    pub requested_composite_mode: Option<PictureCompositeMode>,
    /// Requested rasterization space for this picture. It is
    /// a performance hint only.
    pub requested_raster_space: RasterSpace,

    pub raster_config: Option<RasterConfig>,
    pub context_3d: Picture3DContext<OrderedPictureChild>,

    // If requested as a frame output (for rendering
    // pages to a texture), this is the pipeline this
    // picture is the root of.
    pub frame_output_pipeline_id: Option<PipelineId>,
    // Optional cache handles for storing extra data
    // in the GPU cache, depending on the type of
    // picture.
    pub extra_gpu_data_handles: SmallVec<[GpuCacheHandle; 1]>,

    /// The spatial node index of this picture when it is
    /// composited into the parent picture.
    pub spatial_node_index: SpatialNodeIndex,

    /// The conservative local rect of this picture. It is
    /// built dynamically during the first picture traversal.
    /// It is composed of already snapped primitives.
    pub estimated_local_rect: LayoutRect,

    /// The local rect of this picture. It is built
    /// dynamically during the frame visibility update. It
    /// differs from the estimated_local_rect because it
    /// will not contain culled primitives, takes into
    /// account surface inflation and the whole clip chain.
    /// It is frequently the same, but may be quite
    /// different depending on how much was culled.
    pub precise_local_rect: LayoutRect,

    /// If false, this picture needs to (re)build segments
    /// if it supports segment rendering. This can occur
    /// if the local rect of the picture changes due to
    /// transform animation and/or scrolling.
    pub segments_are_valid: bool,

    /// If Some(..) the tile cache that is associated with this picture.
    #[cfg_attr(feature = "capture", serde(skip))] //TODO
    pub tile_cache: Option<Box<TileCacheInstance>>,

    /// The config options for this picture.
    pub options: PictureOptions,

    /// Keep track of the number of render tasks dependencies to pre-allocate
    /// the dependency array next frame.
    num_render_tasks: usize,
}

impl PicturePrimitive {
    pub fn print<T: PrintTreePrinter>(
        &self,
        pictures: &[Self],
        self_index: PictureIndex,
        pt: &mut T,
    ) {
        pt.new_level(format!("{:?}", self_index));
        pt.add_item(format!("cluster_count: {:?}", self.prim_list.clusters.len()));
        pt.add_item(format!("estimated_local_rect: {:?}", self.estimated_local_rect));
        pt.add_item(format!("precise_local_rect: {:?}", self.precise_local_rect));
        pt.add_item(format!("spatial_node_index: {:?}", self.spatial_node_index));
        pt.add_item(format!("raster_config: {:?}", self.raster_config));
        pt.add_item(format!("requested_composite_mode: {:?}", self.requested_composite_mode));

        for cluster in &self.prim_list.clusters {
            if cluster.flags.contains(ClusterFlags::IS_PICTURE) {
                for instance in &cluster.prim_instances {
                    let index = match instance.kind {
                        PrimitiveInstanceKind::Picture { pic_index, .. } => pic_index,
                        _ => unreachable!(),
                    };
                    pictures[index.0].print(pictures, index, pt);
                }
            }
        }

        pt.end_level();
    }

    /// Returns true if this picture supports segmented rendering.
    pub fn can_use_segments(&self) -> bool {
        match self.raster_config {
            // TODO(gw): Support brush segment rendering for filter and mix-blend
            //           shaders. It's possible this already works, but I'm just
            //           applying this optimization to Blit mode for now.
            Some(RasterConfig { composite_mode: PictureCompositeMode::MixBlend(..), .. }) |
            Some(RasterConfig { composite_mode: PictureCompositeMode::Filter(..), .. }) |
            Some(RasterConfig { composite_mode: PictureCompositeMode::ComponentTransferFilter(..), .. }) |
            Some(RasterConfig { composite_mode: PictureCompositeMode::TileCache { .. }, .. }) |
            Some(RasterConfig { composite_mode: PictureCompositeMode::SvgFilter(..), .. }) |
            None => {
                false
            }
            Some(RasterConfig { composite_mode: PictureCompositeMode::Blit(reason), ..}) => {
                reason == BlitReason::CLIP
            }
        }
    }

    fn resolve_scene_properties(&mut self, properties: &SceneProperties) -> bool {
        match self.requested_composite_mode {
            Some(PictureCompositeMode::Filter(ref mut filter)) => {
                match *filter {
                    Filter::Opacity(ref binding, ref mut value) => {
                        *value = properties.resolve_float(binding);
                    }
                    _ => {}
                }

                filter.is_visible()
            }
            _ => true,
        }
    }

    pub fn is_visible(&self) -> bool {
        match self.requested_composite_mode {
            Some(PictureCompositeMode::Filter(ref filter)) => {
                filter.is_visible()
            }
            _ => true,
        }
    }

    /// Destroy an existing picture. This is called just before
    /// a frame builder is replaced with a newly built scene. It
    /// gives a picture a chance to retain any cached tiles that
    /// may be useful during the next scene build.
    pub fn destroy(
        &mut self,
        retained_tiles: &mut RetainedTiles,
    ) {
        if let Some(mut tile_cache) = self.tile_cache.take() {
            if !tile_cache.tiles.is_empty() {
                retained_tiles.caches.insert(
                    tile_cache.slice,
                    PictureCacheState {
                        tiles: tile_cache.tiles,
                        spatial_nodes: tile_cache.spatial_nodes,
                        opacity_bindings: tile_cache.opacity_bindings,
                        root_transform: tile_cache.root_transform,
                        current_tile_size: tile_cache.current_tile_size,
                        native_surface_id: tile_cache.native_surface_id.take(),
                        is_opaque: tile_cache.is_opaque,
                        allocations: PictureCacheRecycledAllocations {
                            old_tiles: tile_cache.old_tiles,
                            old_opacity_bindings: tile_cache.old_opacity_bindings,
                            compare_cache: tile_cache.compare_cache,
                        },
                    },
                );
            }
        }
    }

    // TODO(gw): We have the PictureOptions struct available. We
    //           should move some of the parameter list in this
    //           method to be part of the PictureOptions, and
    //           avoid adding new parameters here.
    pub fn new_image(
        requested_composite_mode: Option<PictureCompositeMode>,
        context_3d: Picture3DContext<OrderedPictureChild>,
        frame_output_pipeline_id: Option<PipelineId>,
        apply_local_clip_rect: bool,
        flags: PrimitiveFlags,
        requested_raster_space: RasterSpace,
        prim_list: PrimitiveList,
        spatial_node_index: SpatialNodeIndex,
        tile_cache: Option<Box<TileCacheInstance>>,
        options: PictureOptions,
    ) -> Self {
        PicturePrimitive {
            prim_list,
            state: None,
            secondary_render_task_id: None,
            requested_composite_mode,
            raster_config: None,
            context_3d,
            frame_output_pipeline_id,
            extra_gpu_data_handles: SmallVec::new(),
            apply_local_clip_rect,
            is_backface_visible: flags.contains(PrimitiveFlags::IS_BACKFACE_VISIBLE),
            requested_raster_space,
            spatial_node_index,
            estimated_local_rect: LayoutRect::zero(),
            precise_local_rect: LayoutRect::zero(),
            tile_cache,
            options,
            segments_are_valid: false,
            num_render_tasks: 0,
        }
    }

    /// Gets the raster space to use when rendering the picture.
    /// Usually this would be the requested raster space. However, if the
    /// picture's spatial node or one of its ancestors is being pinch zoomed
    /// then we round it. This prevents us rasterizing glyphs for every minor
    /// change in zoom level, as that would be too expensive.
    pub fn get_raster_space(&self, clip_scroll_tree: &ClipScrollTree) -> RasterSpace {
        let spatial_node = &clip_scroll_tree.spatial_nodes[self.spatial_node_index.0 as usize];
        if spatial_node.is_ancestor_or_self_zooming {
            let scale_factors = clip_scroll_tree
                .get_relative_transform(self.spatial_node_index, ROOT_SPATIAL_NODE_INDEX)
                .scale_factors();

            // Round the scale up to the nearest power of 2, but don't exceed 8.
            let scale = scale_factors.0.max(scale_factors.1).min(8.0);
            let rounded_up = 2.0f32.powf(scale.log2().ceil());

            RasterSpace::Local(rounded_up)
        } else {
            self.requested_raster_space
        }
    }

    pub fn take_context(
        &mut self,
        pic_index: PictureIndex,
        clipped_prim_bounding_rect: WorldRect,
        surface_spatial_node_index: SpatialNodeIndex,
        raster_spatial_node_index: SpatialNodeIndex,
        parent_surface_index: SurfaceIndex,
        parent_subpixel_mode: SubpixelMode,
        frame_state: &mut FrameBuildingState,
        frame_context: &FrameBuildingContext,
        scratch: &mut PrimitiveScratchBuffer,
        tile_cache_logger: &mut TileCacheLogger,
    ) -> Option<(PictureContext, PictureState, PrimitiveList)> {
        if !self.is_visible() {
            return None;
        }

        let task_id = frame_state.surfaces[parent_surface_index.0].render_tasks.unwrap().port;
        frame_state.render_tasks[task_id].children.reserve(self.num_render_tasks);

        // Extract the raster and surface spatial nodes from the raster
        // config, if this picture establishes a surface. Otherwise just
        // pass in the spatial node indices from the parent context.
        let (raster_spatial_node_index, surface_spatial_node_index, surface_index, inflation_factor) = match self.raster_config {
            Some(ref raster_config) => {
                let surface = &frame_state.surfaces[raster_config.surface_index.0];

                (
                    surface.raster_spatial_node_index,
                    self.spatial_node_index,
                    raster_config.surface_index,
                    surface.inflation_factor,
                )
            }
            None => {
                (
                    raster_spatial_node_index,
                    surface_spatial_node_index,
                    parent_surface_index,
                    0.0,
                )
            }
        };

        let map_pic_to_world = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface_spatial_node_index,
            frame_context.global_screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        let pic_bounds = map_pic_to_world.unmap(&map_pic_to_world.bounds)
                                         .unwrap_or_else(PictureRect::max_rect);

        let map_local_to_pic = SpaceMapper::new(
            surface_spatial_node_index,
            pic_bounds,
        );

        let (map_raster_to_world, map_pic_to_raster) = create_raster_mappers(
            surface_spatial_node_index,
            raster_spatial_node_index,
            frame_context.global_screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        let plane_splitter = match self.context_3d {
            Picture3DContext::Out => {
                None
            }
            Picture3DContext::In { root_data: Some(_), .. } => {
                Some(PlaneSplitter::new())
            }
            Picture3DContext::In { root_data: None, .. } => {
                None
            }
        };

        let unclipped =
        match self.raster_config {
            Some(ref raster_config) => {
                let pic_rect = PictureRect::from_untyped(&self.precise_local_rect.to_untyped());

                let device_pixel_scale = frame_state
                    .surfaces[raster_config.surface_index.0]
                    .device_pixel_scale;

                let (clipped, unclipped) = match get_raster_rects(
                    pic_rect,
                    &map_pic_to_raster,
                    &map_raster_to_world,
                    clipped_prim_bounding_rect,
                    device_pixel_scale,
                ) {
                    Some(info) => info,
                    None => {
                        return None
                    }
                };
                let transform = map_pic_to_raster.get_transform();

                let dep_info = match raster_config.composite_mode {
                    PictureCompositeMode::Filter(Filter::Blur(blur_radius)) => {
                        let blur_std_deviation = blur_radius * device_pixel_scale.0;
                        let scale_factors = scale_factors(&transform);
                        let blur_std_deviation = DeviceSize::new(
                            blur_std_deviation * scale_factors.0,
                            blur_std_deviation * scale_factors.1
                        );
                        let mut device_rect = if self.options.inflate_if_required {
                            let inflation_factor = frame_state.surfaces[raster_config.surface_index.0].inflation_factor;
                            let inflation_factor = (inflation_factor * device_pixel_scale.0).ceil();

                            // The clipped field is the part of the picture that is visible
                            // on screen. The unclipped field is the screen-space rect of
                            // the complete picture, if no screen / clip-chain was applied
                            // (this includes the extra space for blur region). To ensure
                            // that we draw a large enough part of the picture to get correct
                            // blur results, inflate that clipped area by the blur range, and
                            // then intersect with the total screen rect, to minimize the
                            // allocation size.
                            // We cast clipped to f32 instead of casting unclipped to i32
                            // because unclipped can overflow an i32.
                            let device_rect = clipped.to_f32()
                                .inflate(inflation_factor, inflation_factor)
                                .intersection(&unclipped)
                                .unwrap();

                            match device_rect.try_cast::<i32>() {
                                Some(rect) => rect,
                                None => {
                                    return None
                                }
                            }
                        } else {
                            clipped
                        };

                        let original_size = device_rect.size;

                        // Adjust the size to avoid introducing sampling errors during the down-scaling passes.
                        // what would be even better is to rasterize the picture at the down-scaled size
                        // directly.
                        device_rect.size = RenderTask::adjusted_blur_source_size(
                            device_rect.size,
                            blur_std_deviation,
                        );

                        let uv_rect_kind = calculate_uv_rect_kind(
                            &pic_rect,
                            &transform,
                            &device_rect,
                            device_pixel_scale,
                            true,
                        );

                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, device_rect.size),
                            unclipped.size,
                            pic_index,
                            device_rect.origin,
                            uv_rect_kind,
                            surface_spatial_node_index,
                            device_pixel_scale,
                            PrimitiveVisibilityMask::all(),
                            None,
                        );

                        let picture_task_id = frame_state.render_tasks.add(picture_task);

                        let blur_render_task_id = RenderTask::new_blur(
                            blur_std_deviation,
                            picture_task_id,
                            frame_state.render_tasks,
                            RenderTargetKind::Color,
                            ClearMode::Transparent,
                            None,
                            original_size,
                        );

                        Some((blur_render_task_id, picture_task_id))
                    }
                    PictureCompositeMode::Filter(Filter::DropShadows(ref shadows)) => {
                        let mut max_std_deviation = 0.0;
                        for shadow in shadows {
                            // TODO(nical) presumably we should compute the clipped rect for each shadow
                            // and compute the union of them to determine what we need to rasterize and blur?
                            max_std_deviation = f32::max(max_std_deviation, shadow.blur_radius * device_pixel_scale.0);
                        }

                        max_std_deviation = max_std_deviation.round();
                        let max_blur_range = (max_std_deviation * BLUR_SAMPLE_SCALE).ceil();
                        // We cast clipped to f32 instead of casting unclipped to i32
                        // because unclipped can overflow an i32.
                        let device_rect = clipped.to_f32()
                                .inflate(max_blur_range, max_blur_range)
                                .intersection(&unclipped)
                                .unwrap();

                        let mut device_rect = match device_rect.try_cast::<i32>() {
                            Some(rect) => rect,
                            None => {
                                return None
                            }
                        };

                        device_rect.size = RenderTask::adjusted_blur_source_size(
                            device_rect.size,
                            DeviceSize::new(max_std_deviation, max_std_deviation),
                        );

                        let uv_rect_kind = calculate_uv_rect_kind(
                            &pic_rect,
                            &transform,
                            &device_rect,
                            device_pixel_scale,
                            true,
                        );

                        let mut picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, device_rect.size),
                            unclipped.size,
                            pic_index,
                            device_rect.origin,
                            uv_rect_kind,
                            surface_spatial_node_index,
                            device_pixel_scale,
                            PrimitiveVisibilityMask::all(),
                            None,
                        );
                        picture_task.mark_for_saving();

                        let picture_task_id = frame_state.render_tasks.add(picture_task);

                        self.secondary_render_task_id = Some(picture_task_id);

                        let mut blur_tasks = BlurTaskCache::default();

                        self.extra_gpu_data_handles.resize(shadows.len(), GpuCacheHandle::new());

                        let mut blur_render_task_id = picture_task_id;
                        for shadow in shadows {
                            let std_dev = f32::round(shadow.blur_radius * device_pixel_scale.0);
                            blur_render_task_id = RenderTask::new_blur(
                                DeviceSize::new(std_dev, std_dev),
                                picture_task_id,
                                frame_state.render_tasks,
                                RenderTargetKind::Color,
                                ClearMode::Transparent,
                                Some(&mut blur_tasks),
                                device_rect.size,
                            );
                        }

                        // TODO(nical) the second one should to be the blur's task id but we have several blurs now
                        Some((blur_render_task_id, picture_task_id))
                    }
                    PictureCompositeMode::MixBlend(..) if !frame_context.fb_config.gpu_supports_advanced_blend => {
                        let uv_rect_kind = calculate_uv_rect_kind(
                            &pic_rect,
                            &transform,
                            &clipped,
                            device_pixel_scale,
                            true,
                        );

                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, clipped.size),
                            unclipped.size,
                            pic_index,
                            clipped.origin,
                            uv_rect_kind,
                            surface_spatial_node_index,
                            device_pixel_scale,
                            PrimitiveVisibilityMask::all(),
                            None,
                        );

                        let readback_task_id = frame_state.render_tasks.add(
                            RenderTask::new_readback(clipped)
                        );

                        frame_state.render_tasks.add_dependency(
                            frame_state.surfaces[parent_surface_index.0].render_tasks.unwrap().port,
                            readback_task_id,
                        );

                        self.secondary_render_task_id = Some(readback_task_id);

                        let render_task_id = frame_state.render_tasks.add(picture_task);

                        Some((render_task_id, render_task_id))
                    }
                    PictureCompositeMode::Filter(..) => {
                        let uv_rect_kind = calculate_uv_rect_kind(
                            &pic_rect,
                            &transform,
                            &clipped,
                            device_pixel_scale,
                            true,
                        );

                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, clipped.size),
                            unclipped.size,
                            pic_index,
                            clipped.origin,
                            uv_rect_kind,
                            surface_spatial_node_index,
                            device_pixel_scale,
                            PrimitiveVisibilityMask::all(),
                            None,
                        );

                        let render_task_id = frame_state.render_tasks.add(picture_task);

                        Some((render_task_id, render_task_id))
                    }
                    PictureCompositeMode::ComponentTransferFilter(..) => {
                        let uv_rect_kind = calculate_uv_rect_kind(
                            &pic_rect,
                            &transform,
                            &clipped,
                            device_pixel_scale,
                            true,
                        );

                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, clipped.size),
                            unclipped.size,
                            pic_index,
                            clipped.origin,
                            uv_rect_kind,
                            surface_spatial_node_index,
                            device_pixel_scale,
                            PrimitiveVisibilityMask::all(),
                            None,
                        );

                        let render_task_id = frame_state.render_tasks.add(picture_task);

                        Some((render_task_id, render_task_id))
                    }
                    PictureCompositeMode::TileCache { .. } => {
                        let tile_cache = self.tile_cache.as_mut().unwrap();
                        let mut first = true;

                        // Get the overall world space rect of the picture cache. Used to clip
                        // the tile rects below for occlusion testing to the relevant area.
                        let local_clip_rect = tile_cache.local_rect
                            .intersection(&tile_cache.local_clip_rect)
                            .unwrap_or_else(PictureRect::zero);

                        let world_clip_rect = map_pic_to_world
                            .map(&local_clip_rect)
                            .expect("bug: unable to map clip rect");

                        for key in &tile_cache.tiles_to_draw {
                            let tile = tile_cache.tiles.get_mut(key).expect("bug: no tile found!");

                            // Get the world space rect that this tile will actually occupy on screem
                            let tile_draw_rect = match world_clip_rect.intersection(&tile.world_rect) {
                                Some(rect) => rect,
                                None => {
                                    tile.is_visible = false;
                                    continue;
                                }
                            };

                            // If that draw rect is occluded by some set of tiles in front of it,
                            // then mark it as not visible and skip drawing. When it's not occluded
                            // it will fail this test, and get rasterized by the render task setup
                            // code below.
                            if frame_state.composite_state.is_tile_occluded(tile_cache.slice, tile_draw_rect) {
                                // If this tile has an allocated native surface, free it, since it's completely
                                // occluded. We will need to re-allocate this surface if it becomes visible,
                                // but that's likely to be rare (e.g. when there is no content display list
                                // for a frame or two during a tab switch).
                                let surface = tile.surface.as_mut().expect("no tile surface set!");

                                if let TileSurface::Texture { descriptor: SurfaceTextureDescriptor::Native { id, .. }, .. } = surface {
                                    if let Some(id) = id.take() {
                                        frame_state.resource_cache.destroy_compositor_tile(id);
                                    }
                                }

                                tile.is_visible = false;
                                continue;
                            }

                            if frame_context.debug_flags.contains(DebugFlags::PICTURE_CACHING_DBG) {
                                tile.root.draw_debug_rects(
                                    &map_pic_to_world,
                                    tile.is_opaque,
                                    scratch,
                                    frame_context.global_device_pixel_scale,
                                );

                                let label_offset = DeviceVector2D::new(20.0, 30.0);
                                let tile_device_rect = tile.world_rect * frame_context.global_device_pixel_scale;
                                if tile_device_rect.size.height >= label_offset.y {
                                    let surface = tile.surface.as_ref().expect("no tile surface set!");

                                    scratch.push_debug_string(
                                        tile_device_rect.origin + label_offset,
                                        debug_colors::RED,
                                        format!("{:?}: s={} is_opaque={} surface={}",
                                                tile.id,
                                                tile_cache.slice,
                                                tile.is_opaque,
                                                surface.kind(),
                                        ),
                                    );
                                }
                            }

                            if let TileSurface::Texture { descriptor, .. } = tile.surface.as_mut().unwrap() {
                                match descriptor {
                                    SurfaceTextureDescriptor::TextureCache { ref handle, .. } => {
                                        // Invalidate if the backing texture was evicted.
                                        if frame_state.resource_cache.texture_cache.is_allocated(handle) {
                                            // Request the backing texture so it won't get evicted this frame.
                                            // We specifically want to mark the tile texture as used, even
                                            // if it's detected not visible below and skipped. This is because
                                            // we maintain the set of tiles we care about based on visibility
                                            // during pre_update. If a tile still exists after that, we are
                                            // assuming that it's either visible or we want to retain it for
                                            // a while in case it gets scrolled back onto screen soon.
                                            // TODO(gw): Consider switching to manual eviction policy?
                                            frame_state.resource_cache.texture_cache.request(handle, frame_state.gpu_cache);
                                        } else {
                                            // If the texture was evicted on a previous frame, we need to assume
                                            // that the entire tile rect is dirty.
                                            tile.invalidate(None, InvalidationReason::NoTexture);
                                        }
                                    }
                                    SurfaceTextureDescriptor::Native { id, .. } => {
                                        if id.is_none() {
                                            // There is no current surface allocation, so ensure the entire tile is invalidated
                                            tile.invalidate(None, InvalidationReason::NoSurface);
                                        }
                                    }
                                }
                            }

                            // Update the world dirty rect
                            tile.world_dirty_rect = map_pic_to_world.map(&tile.dirty_rect).expect("bug");

                            if tile.is_valid {
                                continue;
                            }

                            // Ensure that this texture is allocated.
                            if let TileSurface::Texture { ref mut descriptor, ref mut visibility_mask } = tile.surface.as_mut().unwrap() {
                                match descriptor {
                                    SurfaceTextureDescriptor::TextureCache { ref mut handle } => {
                                        if !frame_state.resource_cache.texture_cache.is_allocated(handle) {
                                            frame_state.resource_cache.texture_cache.update_picture_cache(
                                                tile_cache.current_tile_size,
                                                handle,
                                                frame_state.gpu_cache,
                                            );
                                        }
                                    }
                                    SurfaceTextureDescriptor::Native { id } => {
                                        if id.is_none() {
                                            // Allocate a native surface id if we're in native compositing mode,
                                            // and we don't have a surface yet (due to first frame, or destruction
                                            // due to tile size changing etc).
                                            if tile_cache.native_surface_id.is_none() {
                                                let surface_id = frame_state
                                                    .resource_cache
                                                    .create_compositor_surface(
                                                        tile_cache.current_tile_size,
                                                        tile_cache.is_opaque,
                                                    );

                                                tile_cache.native_surface_id = Some(surface_id);
                                            }

                                            // Create the tile identifier and allocate it.
                                            let tile_id = NativeTileId {
                                                surface_id: tile_cache.native_surface_id.unwrap(),
                                                x: key.x,
                                                y: key.y,
                                            };

                                            frame_state.resource_cache.create_compositor_tile(tile_id);

                                            *id = Some(tile_id);
                                        }
                                    }
                                }

                                *visibility_mask = PrimitiveVisibilityMask::empty();
                                let dirty_region_index = tile_cache.dirty_region.dirty_rects.len();

                                // If we run out of dirty regions, then force the last dirty region to
                                // be a union of any remaining regions. This is an inefficiency, in that
                                // we'll add items to batches later on that are redundant / outside this
                                // tile, but it's really rare except in pathological cases (even on a
                                // 4k screen, the typical dirty region count is < 16).
                                if dirty_region_index < PrimitiveVisibilityMask::MAX_DIRTY_REGIONS {
                                    visibility_mask.set_visible(dirty_region_index);

                                    tile_cache.dirty_region.push(
                                        tile.world_dirty_rect,
                                        *visibility_mask,
                                    );
                                } else {
                                    visibility_mask.set_visible(PrimitiveVisibilityMask::MAX_DIRTY_REGIONS - 1);

                                    tile_cache.dirty_region.include_rect(
                                        PrimitiveVisibilityMask::MAX_DIRTY_REGIONS - 1,
                                        tile.world_dirty_rect,
                                    );
                                }

                                let content_origin_f = tile.world_rect.origin * device_pixel_scale;
                                let content_origin = content_origin_f.round();
                                debug_assert!((content_origin_f.x - content_origin.x).abs() < 0.01);
                                debug_assert!((content_origin_f.y - content_origin.y).abs() < 0.01);

                                // Get a task-local scissor rect for the dirty region of this
                                // picture cache task.
                                let scissor_rect = tile.world_dirty_rect.translate(
                                    -tile.world_rect.origin.to_vector()
                                );
                                // The world rect is guaranteed to be device pixel aligned, by the tile
                                // sizing code in tile::pre_update. However, there might be some
                                // small floating point accuracy issues (these were observed on ARM
                                // CPUs). Round the rect here before casting to integer device pixels
                                // to ensure the scissor rect is correct.
                                let scissor_rect = (scissor_rect * device_pixel_scale).round();
                                let surface = descriptor.resolve(
                                    frame_state.resource_cache,
                                    tile_cache.current_tile_size,
                                );

                                let task = RenderTask::new_picture(
                                    RenderTaskLocation::PictureCache {
                                        size: tile_cache.current_tile_size,
                                        surface,
                                    },
                                    tile_cache.current_tile_size.to_f32(),
                                    pic_index,
                                    content_origin.to_i32(),
                                    UvRectKind::Rect,
                                    surface_spatial_node_index,
                                    device_pixel_scale,
                                    *visibility_mask,
                                    Some(scissor_rect.to_i32()),
                                );

                                let render_task_id = frame_state.render_tasks.add(task);

                                frame_state.render_tasks.add_dependency(
                                    frame_state.surfaces[parent_surface_index.0].render_tasks.unwrap().port,
                                    render_task_id,
                                );

                                if first {
                                    // TODO(gw): Maybe we can restructure this code to avoid the
                                    //           first hack here. Or at least explain it with a follow up
                                    //           bug.
                                    frame_state.surfaces[raster_config.surface_index.0].render_tasks = Some(SurfaceRenderTasks {
                                        root: render_task_id,
                                        port: render_task_id,
                                    });

                                    first = false;
                                }
                            }

                            // Now that the tile is valid, reset the dirty rect.
                            tile.dirty_rect = PictureRect::zero();
                            tile.is_valid = true;
                        }

                        // If invalidation debugging is enabled, dump the picture cache state to a tree printer.
                        if frame_context.debug_flags.contains(DebugFlags::INVALIDATION_DBG) {
                            tile_cache.print();
                        }

                        None
                    }
                    PictureCompositeMode::MixBlend(..) |
                    PictureCompositeMode::Blit(_) => {
                        // The SplitComposite shader used for 3d contexts doesn't snap
                        // to pixels, so we shouldn't snap our uv coordinates either.
                        let supports_snapping = match self.context_3d {
                            Picture3DContext::In{ .. } => false,
                            _ => true,
                        };

                        let uv_rect_kind = calculate_uv_rect_kind(
                            &pic_rect,
                            &transform,
                            &clipped,
                            device_pixel_scale,
                            supports_snapping,
                        );

                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, clipped.size),
                            unclipped.size,
                            pic_index,
                            clipped.origin,
                            uv_rect_kind,
                            surface_spatial_node_index,
                            device_pixel_scale,
                            PrimitiveVisibilityMask::all(),
                            None,
                        );

                        let render_task_id = frame_state.render_tasks.add(picture_task);

                        Some((render_task_id, render_task_id))
                    }
                    PictureCompositeMode::SvgFilter(ref primitives, ref filter_datas) => {
                        let uv_rect_kind = calculate_uv_rect_kind(
                            &pic_rect,
                            &transform,
                            &clipped,
                            device_pixel_scale,
                            true,
                        );

                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, clipped.size),
                            unclipped.size,
                            pic_index,
                            clipped.origin,
                            uv_rect_kind,
                            surface_spatial_node_index,
                            device_pixel_scale,
                            PrimitiveVisibilityMask::all(),
                            None,
                        );

                        let picture_task_id = frame_state.render_tasks.add(picture_task);

                        let filter_task_id = RenderTask::new_svg_filter(
                            primitives,
                            filter_datas,
                            &mut frame_state.render_tasks,
                            clipped.size,
                            uv_rect_kind,
                            picture_task_id,
                            device_pixel_scale,
                        );

                        Some((filter_task_id, picture_task_id))
                    }
                };

                if let Some((root, port)) = dep_info {
                    frame_state.surfaces[raster_config.surface_index.0].render_tasks = Some(SurfaceRenderTasks {
                        root,
                        port,
                    });

                    frame_state.render_tasks.add_dependency(
                        frame_state.surfaces[parent_surface_index.0].render_tasks.unwrap().port,
                        root,
                    );
                }

                unclipped
            }
            None => DeviceRect::zero()
        };

        #[cfg(feature = "capture")]
        {
            if frame_context.debug_flags.contains(DebugFlags::TILE_CACHE_LOGGING_DBG) {
                if let Some(ref tile_cache) = self.tile_cache
                {
                    // extract just the fields that we're interested in
                    let mut tile_cache_tiny = TileCacheInstanceSerializer {
                        slice: tile_cache.slice,
                        tiles: FastHashMap::default(),
                        background_color: tile_cache.background_color,
                        fract_offset: tile_cache.fract_offset
                    };
                    for (key, tile) in &tile_cache.tiles {
                        tile_cache_tiny.tiles.insert(*key, TileSerializer {
                            rect: tile.rect,
                            current_descriptor: tile.current_descriptor.clone(),
                            fract_offset: tile.fract_offset,
                            id: tile.id,
                            root: tile.root.clone(),
                            background_color: tile.background_color,
                            invalidation_reason: tile.invalidation_reason.clone()
                        });
                    }
                    let text = ron::ser::to_string_pretty(&tile_cache_tiny, Default::default()).unwrap();
                    tile_cache_logger.add(text, unclipped);
                }
            }
        }
        #[cfg(not(feature = "capture"))]
        {
            let _tile_cache_logger = tile_cache_logger;   // unused variable fix
            let _unclipped = unclipped;
        }

        let state = PictureState {
            //TODO: check for MAX_CACHE_SIZE here?
            map_local_to_pic,
            map_pic_to_world,
            map_pic_to_raster,
            map_raster_to_world,
            plane_splitter,
        };

        let mut dirty_region_count = 0;

        // If this is a picture cache, push the dirty region to ensure any
        // child primitives are culled and clipped to the dirty rect(s).
        if let Some(RasterConfig { composite_mode: PictureCompositeMode::TileCache { .. }, .. }) = self.raster_config {
            let dirty_region = self.tile_cache.as_ref().unwrap().dirty_region.clone();
            frame_state.push_dirty_region(dirty_region);
            dirty_region_count += 1;
        }

        if inflation_factor > 0.0 {
            let inflated_region = frame_state.current_dirty_region().inflate(inflation_factor);
            frame_state.push_dirty_region(inflated_region);
            dirty_region_count += 1;
        }

        // Disallow subpixel AA if an intermediate surface is needed.
        // TODO(lsalzman): allow overriding parent if intermediate surface is opaque
        let (is_passthrough, subpixel_mode) = match self.raster_config {
            Some(RasterConfig { ref composite_mode, .. }) => {
                let subpixel_mode = match composite_mode {
                    PictureCompositeMode::TileCache { .. } => {
                        self.tile_cache.as_ref().unwrap().subpixel_mode
                    }
                    PictureCompositeMode::Blit(..) |
                    PictureCompositeMode::ComponentTransferFilter(..) |
                    PictureCompositeMode::Filter(..) |
                    PictureCompositeMode::MixBlend(..) |
                    PictureCompositeMode::SvgFilter(..) => {
                        // TODO(gw): We can take advantage of the same logic that
                        //           exists in the opaque rect detection for tile
                        //           caches, to allow subpixel text on other surfaces
                        //           that can be detected as opaque.
                        SubpixelMode::Deny
                    }
                };

                (false, subpixel_mode)
            }
            None => {
                (true, SubpixelMode::Allow)
            }
        };

        // Still disable subpixel AA if parent forbids it
        let subpixel_mode = match (parent_subpixel_mode, subpixel_mode) {
            (SubpixelMode::Allow, SubpixelMode::Allow) => SubpixelMode::Allow,
            _ => SubpixelMode::Deny,
        };

        let context = PictureContext {
            pic_index,
            apply_local_clip_rect: self.apply_local_clip_rect,
            is_passthrough,
            raster_spatial_node_index,
            surface_spatial_node_index,
            surface_index,
            dirty_region_count,
            subpixel_mode,
        };

        let prim_list = mem::replace(&mut self.prim_list, PrimitiveList::empty());

        Some((context, state, prim_list))
    }

    pub fn restore_context(
        &mut self,
        parent_surface_index: SurfaceIndex,
        prim_list: PrimitiveList,
        context: PictureContext,
        state: PictureState,
        frame_state: &mut FrameBuildingState,
    ) {
        // Pop any dirty regions this picture set
        for _ in 0 .. context.dirty_region_count {
            frame_state.pop_dirty_region();
        }

        let task_id = frame_state.surfaces[parent_surface_index.0].render_tasks.unwrap().port;
        self.num_render_tasks = frame_state.render_tasks[task_id].children.len();

        self.prim_list = prim_list;
        self.state = Some(state);
    }

    pub fn take_state(&mut self) -> PictureState {
        self.state.take().expect("bug: no state present!")
    }

    /// Add a primitive instance to the plane splitter. The function would generate
    /// an appropriate polygon, clip it against the frustum, and register with the
    /// given plane splitter.
    pub fn add_split_plane(
        splitter: &mut PlaneSplitter,
        clip_scroll_tree: &ClipScrollTree,
        prim_spatial_node_index: SpatialNodeIndex,
        original_local_rect: LayoutRect,
        combined_local_clip_rect: &LayoutRect,
        world_rect: WorldRect,
        plane_split_anchor: PlaneSplitAnchor,
    ) -> bool {
        let transform = clip_scroll_tree
            .get_world_transform(prim_spatial_node_index);
        let matrix = transform.clone().into_transform().cast();

        // Apply the local clip rect here, before splitting. This is
        // because the local clip rect can't be applied in the vertex
        // shader for split composites, since we are drawing polygons
        // rather that rectangles. The interpolation still works correctly
        // since we determine the UVs by doing a bilerp with a factor
        // from the original local rect.
        let local_rect = match original_local_rect
            .intersection(combined_local_clip_rect)
        {
            Some(rect) => rect.cast(),
            None => return false,
        };
        let world_rect = world_rect.cast();

        match transform {
            CoordinateSpaceMapping::Local => {
                let polygon = Polygon::from_rect(
                    local_rect * Scale::new(1.0),
                    plane_split_anchor,
                );
                splitter.add(polygon);
            }
            CoordinateSpaceMapping::ScaleOffset(scale_offset) if scale_offset.scale == Vector2D::new(1.0, 1.0) => {
                let inv_matrix = scale_offset.inverse().to_transform().cast();
                let polygon = Polygon::from_transformed_rect_with_inverse(
                    local_rect,
                    &matrix,
                    &inv_matrix,
                    plane_split_anchor,
                ).unwrap();
                splitter.add(polygon);
            }
            CoordinateSpaceMapping::ScaleOffset(_) |
            CoordinateSpaceMapping::Transform(_) => {
                let mut clipper = Clipper::new();
                let results = clipper.clip_transformed(
                    Polygon::from_rect(
                        local_rect,
                        plane_split_anchor,
                    ),
                    &matrix,
                    Some(world_rect),
                );
                if let Ok(results) = results {
                    for poly in results {
                        splitter.add(poly);
                    }
                }
            }
        }

        true
    }

    pub fn resolve_split_planes(
        &mut self,
        splitter: &mut PlaneSplitter,
        gpu_cache: &mut GpuCache,
        clip_scroll_tree: &ClipScrollTree,
    ) {
        let ordered = match self.context_3d {
            Picture3DContext::In { root_data: Some(ref mut list), .. } => list,
            _ => panic!("Expected to find 3D context root"),
        };
        ordered.clear();

        // Process the accumulated split planes and order them for rendering.
        // Z axis is directed at the screen, `sort` is ascending, and we need back-to-front order.
        for poly in splitter.sort(vec3(0.0, 0.0, 1.0)) {
            let cluster = &self.prim_list.clusters[poly.anchor.cluster_index];
            let spatial_node_index = cluster.spatial_node_index;
            let transform = match clip_scroll_tree
                .get_world_transform(spatial_node_index)
                .inverse()
            {
                Some(transform) => transform.into_transform(),
                // logging this would be a bit too verbose
                None => continue,
            };

            let local_points = [
                transform.transform_point3d(poly.points[0].cast()).unwrap(),
                transform.transform_point3d(poly.points[1].cast()).unwrap(),
                transform.transform_point3d(poly.points[2].cast()).unwrap(),
                transform.transform_point3d(poly.points[3].cast()).unwrap(),
            ];
            let gpu_blocks = [
                [local_points[0].x, local_points[0].y, local_points[1].x, local_points[1].y].into(),
                [local_points[2].x, local_points[2].y, local_points[3].x, local_points[3].y].into(),
            ];
            let gpu_handle = gpu_cache.push_per_frame_blocks(&gpu_blocks);
            let gpu_address = gpu_cache.get_address(&gpu_handle);

            ordered.push(OrderedPictureChild {
                anchor: poly.anchor,
                spatial_node_index,
                gpu_address,
            });
        }
    }

    /// Called during initial picture traversal, before we know the
    /// bounding rect of children. It is possible to determine the
    /// surface / raster config now though.
    fn pre_update(
        &mut self,
        state: &mut PictureUpdateState,
        frame_context: &FrameBuildingContext,
    ) -> Option<PrimitiveList> {
        // Reset raster config in case we early out below.
        self.raster_config = None;

        // Resolve animation properties, and early out if the filter
        // properties make this picture invisible.
        if !self.resolve_scene_properties(frame_context.scene_properties) {
            return None;
        }

        // For out-of-preserve-3d pictures, the backface visibility is determined by
        // the local transform only.
        // Note: we aren't taking the transform relativce to the parent picture,
        // since picture tree can be more dense than the corresponding spatial tree.
        if !self.is_backface_visible {
            if let Picture3DContext::Out = self.context_3d {
                match frame_context.clip_scroll_tree.get_local_visible_face(self.spatial_node_index) {
                    VisibleFace::Front => {}
                    VisibleFace::Back => return None,
                }
            }
        }

        // Push information about this pic on stack for children to read.
        state.push_picture(PictureInfo {
            _spatial_node_index: self.spatial_node_index,
        });

        // See if this picture actually needs a surface for compositing.
        let actual_composite_mode = match self.requested_composite_mode {
            Some(PictureCompositeMode::Filter(ref filter)) if filter.is_noop() => None,
            Some(PictureCompositeMode::TileCache { .. }) => {
                // Only allow picture caching composite mode if global picture caching setting
                // is enabled this frame.
                if state.composite_state.picture_caching_is_enabled {
                    Some(PictureCompositeMode::TileCache { })
                } else {
                    None
                }
            },
            ref mode => mode.clone(),
        };

        if let Some(composite_mode) = actual_composite_mode {
            // Retrieve the positioning node information for the parent surface.
            let parent_raster_node_index = state.current_surface().raster_spatial_node_index;
            let surface_spatial_node_index = self.spatial_node_index;

            // This inflation factor is to be applied to all primitives within the surface.
            let inflation_factor = match composite_mode {
                PictureCompositeMode::Filter(Filter::Blur(blur_radius)) => {
                    // Only inflate if the caller hasn't already inflated
                    // the bounding rects for this filter.
                    if self.options.inflate_if_required {
                        // The amount of extra space needed for primitives inside
                        // this picture to ensure the visibility check is correct.
                        BLUR_SAMPLE_SCALE * blur_radius
                    } else {
                        0.0
                    }
                }
                PictureCompositeMode::SvgFilter(ref primitives, _) if self.options.inflate_if_required => {
                    let mut max = 0.0;
                    for primitive in primitives {
                        if let FilterPrimitiveKind::Blur(ref blur) = primitive.kind {
                            max = f32::max(max, blur.radius * BLUR_SAMPLE_SCALE);
                        }
                    }
                    max
                }
                _ => {
                    0.0
                }
            };

            // Filters must be applied before transforms, to do this, we can mark this picture as establishing a raster root.
            let has_svg_filter = if let PictureCompositeMode::SvgFilter(..) = composite_mode {
                true
            } else {
                false
            };

            // Check if there is perspective or if an SVG filter is applied, and thus whether a new
            // rasterization root should be established.
            let establishes_raster_root = has_svg_filter || frame_context.clip_scroll_tree
                .get_relative_transform(surface_spatial_node_index, parent_raster_node_index)
                .is_perspective();

            let surface = SurfaceInfo::new(
                surface_spatial_node_index,
                if establishes_raster_root {
                    surface_spatial_node_index
                } else {
                    parent_raster_node_index
                },
                inflation_factor,
                frame_context.global_screen_world_rect,
                &frame_context.clip_scroll_tree,
                frame_context.global_device_pixel_scale,
            );

            self.raster_config = Some(RasterConfig {
                composite_mode,
                establishes_raster_root,
                surface_index: state.push_surface(surface),
            });
        }

        Some(mem::replace(&mut self.prim_list, PrimitiveList::empty()))
    }

    /// Called after updating child pictures during the initial
    /// picture traversal.
    fn post_update(
        &mut self,
        prim_list: PrimitiveList,
        state: &mut PictureUpdateState,
        frame_context: &FrameBuildingContext,
        data_stores: &mut DataStores,
    ) {
        // Restore the pictures list used during recursion.
        self.prim_list = prim_list;

        // Pop the state information about this picture.
        state.pop_picture();

        for cluster in &mut self.prim_list.clusters {
            cluster.flags.remove(ClusterFlags::IS_VISIBLE);

            // Skip the cluster if backface culled.
            if !cluster.flags.contains(ClusterFlags::IS_BACKFACE_VISIBLE) {
                // For in-preserve-3d primitives and pictures, the backface visibility is
                // evaluated relative to the containing block.
                if let Picture3DContext::In { ancestor_index, .. } = self.context_3d {
                    match frame_context.clip_scroll_tree
                        .get_relative_transform(cluster.spatial_node_index, ancestor_index)
                        .visible_face()
                    {
                        VisibleFace::Back => continue,
                        VisibleFace::Front => (),
                    }
                }
            }

            // No point including this cluster if it can't be transformed
            let spatial_node = &frame_context
                .clip_scroll_tree
                .spatial_nodes[cluster.spatial_node_index.0 as usize];
            if !spatial_node.invertible {
                continue;
            }

            // Update any primitives/cluster bounding rects that can only be done
            // with information available during frame building.
            if cluster.flags.contains(ClusterFlags::IS_BACKDROP_FILTER) {
                let backdrop_to_world_mapper = SpaceMapper::new_with_target(
                    ROOT_SPATIAL_NODE_INDEX,
                    cluster.spatial_node_index,
                    LayoutRect::max_rect(),
                    frame_context.clip_scroll_tree,
                );

                for prim_instance in &mut cluster.prim_instances {
                    match prim_instance.kind {
                        PrimitiveInstanceKind::Backdrop { data_handle, .. } => {
                            // The actual size and clip rect of this primitive are determined by computing the bounding
                            // box of the projected rect of the backdrop-filter element onto the backdrop.
                            let prim_data = &mut data_stores.backdrop[data_handle];
                            let spatial_node_index = prim_data.kind.spatial_node_index;

                            // We cannot use the relative transform between the backdrop and the element because
                            // that doesn't take into account any projection transforms that both spatial nodes are children of.
                            // Instead, we first project from the element to the world space and get a flattened 2D bounding rect
                            // in the screen space, we then map this rect from the world space to the backdrop space to get the
                            // proper bounding box where the backdrop-filter needs to be processed.

                            let prim_to_world_mapper = SpaceMapper::new_with_target(
                                ROOT_SPATIAL_NODE_INDEX,
                                spatial_node_index,
                                LayoutRect::max_rect(),
                                frame_context.clip_scroll_tree,
                            );

                            // First map to the screen and get a flattened rect
                            let prim_rect = prim_to_world_mapper.map(&prim_data.kind.border_rect).unwrap_or_else(LayoutRect::zero);
                            // Backwards project the flattened rect onto the backdrop
                            let prim_rect = backdrop_to_world_mapper.unmap(&prim_rect).unwrap_or_else(LayoutRect::zero);

                            // TODO(aosmond): Is this safe? Updating the primitive size during
                            // frame building is usually problematic since scene building will cache
                            // the primitive information in the GPU already.
                            prim_instance.prim_origin = prim_rect.origin;
                            prim_data.common.prim_size = prim_rect.size;
                            prim_instance.local_clip_rect = prim_rect;

                            // Update the cluster bounding rect now that we have the backdrop rect.
                            cluster.bounding_rect = cluster.bounding_rect.union(&prim_rect);
                        }
                        _ => {
                            panic!("BUG: unexpected deferred primitive kind for cluster updates");
                        }
                    }
                }
            }

            // Map the cluster bounding rect into the space of the surface, and
            // include it in the surface bounding rect.
            let surface = state.current_surface_mut();
            surface.map_local_to_surface.set_target_spatial_node(
                cluster.spatial_node_index,
                frame_context.clip_scroll_tree,
            );

            // Mark the cluster visible, since it passed the invertible and
            // backface checks. In future, this will include spatial clustering
            // which will allow the frame building code to skip most of the
            // current per-primitive culling code.
            cluster.flags.insert(ClusterFlags::IS_VISIBLE);
            if let Some(cluster_rect) = surface.map_local_to_surface.map(&cluster.bounding_rect) {
                surface.rect = surface.rect.union(&cluster_rect);
            }
        }

        // If this picture establishes a surface, then map the surface bounding
        // rect into the parent surface coordinate space, and propagate that up
        // to the parent.
        if let Some(ref mut raster_config) = self.raster_config {
            let surface = state.current_surface_mut();
            // Inflate the local bounding rect if required by the filter effect.
            // This inflaction factor is to be applied to the surface itself.
            if self.options.inflate_if_required {
                surface.rect = raster_config.composite_mode.inflate_picture_rect(surface.rect, surface.inflation_factor);

                // The picture's local rect is calculated as the union of the
                // snapped primitive rects, which should result in a snapped
                // local rect, unless it was inflated. This is also done during
                // update visibility when calculating the picture's precise
                // local rect.
                let snap_surface_to_raster = SpaceSnapper::new_with_target(
                    surface.raster_spatial_node_index,
                    self.spatial_node_index,
                    surface.device_pixel_scale,
                    frame_context.clip_scroll_tree,
                );

                surface.rect = snap_surface_to_raster.snap_rect(&surface.rect);
            }

            let mut surface_rect = surface.rect * Scale::new(1.0);

            // Pop this surface from the stack
            let surface_index = state.pop_surface();
            debug_assert_eq!(surface_index, raster_config.surface_index);

            // Check if any of the surfaces can't be rasterized in local space but want to.
            if raster_config.establishes_raster_root
                && (surface_rect.size.width > MAX_SURFACE_SIZE
                    || surface_rect.size.height > MAX_SURFACE_SIZE) {
                raster_config.establishes_raster_root = false;
                state.are_raster_roots_assigned = false;
            }

            // Set the estimated and precise local rects. The precise local rect
            // may be changed again during frame visibility.
            self.estimated_local_rect = surface_rect;
            self.precise_local_rect = surface_rect;

            // Drop shadows draw both a content and shadow rect, so need to expand the local
            // rect of any surfaces to be composited in parent surfaces correctly.
            match raster_config.composite_mode {
                PictureCompositeMode::Filter(Filter::DropShadows(ref shadows)) => {
                    for shadow in shadows {
                        let shadow_rect = self.estimated_local_rect.translate(shadow.offset);
                        surface_rect = surface_rect.union(&shadow_rect);
                    }
                }
                _ => {}
            }

            // Propagate up to parent surface, now that we know this surface's static rect
            let parent_surface = state.current_surface_mut();
            parent_surface.map_local_to_surface.set_target_spatial_node(
                self.spatial_node_index,
                frame_context.clip_scroll_tree,
            );
            if let Some(parent_surface_rect) = parent_surface
                .map_local_to_surface
                .map(&surface_rect)
            {
                parent_surface.rect = parent_surface.rect.union(&parent_surface_rect);
            }
        }
    }

    pub fn prepare_for_render(
        &mut self,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        data_stores: &mut DataStores,
    ) -> bool {
        let mut pic_state_for_children = self.take_state();

        if let Some(ref mut splitter) = pic_state_for_children.plane_splitter {
            self.resolve_split_planes(
                splitter,
                &mut frame_state.gpu_cache,
                &frame_context.clip_scroll_tree,
            );
        }

        let raster_config = match self.raster_config {
            Some(ref mut raster_config) => raster_config,
            None => {
                return true
            }
        };

        // TODO(gw): Almost all of the Picture types below use extra_gpu_cache_data
        //           to store the same type of data. The exception is the filter
        //           with a ColorMatrix, which stores the color matrix here. It's
        //           probably worth tidying this code up to be a bit more consistent.
        //           Perhaps store the color matrix after the common data, even though
        //           it's not used by that shader.

        match raster_config.composite_mode {
            PictureCompositeMode::TileCache { .. } => {}
            PictureCompositeMode::Filter(Filter::Blur(..)) => {}
            PictureCompositeMode::Filter(Filter::DropShadows(ref shadows)) => {
                self.extra_gpu_data_handles.resize(shadows.len(), GpuCacheHandle::new());
                for (shadow, extra_handle) in shadows.iter().zip(self.extra_gpu_data_handles.iter_mut()) {
                    if let Some(mut request) = frame_state.gpu_cache.request(extra_handle) {
                        // Basic brush primitive header is (see end of prepare_prim_for_render_inner in prim_store.rs)
                        //  [brush specific data]
                        //  [segment_rect, segment data]
                        let shadow_rect = self.precise_local_rect.translate(shadow.offset);

                        // ImageBrush colors
                        request.push(shadow.color.premultiplied());
                        request.push(PremultipliedColorF::WHITE);
                        request.push([
                            self.precise_local_rect.size.width,
                            self.precise_local_rect.size.height,
                            0.0,
                            0.0,
                        ]);

                        // segment rect / extra data
                        request.push(shadow_rect);
                        request.push([0.0, 0.0, 0.0, 0.0]);
                    }
                }
            }
            PictureCompositeMode::MixBlend(..) if !frame_context.fb_config.gpu_supports_advanced_blend => {}
            PictureCompositeMode::Filter(ref filter) => {
                match *filter {
                    Filter::ColorMatrix(ref m) => {
                        if self.extra_gpu_data_handles.is_empty() {
                            self.extra_gpu_data_handles.push(GpuCacheHandle::new());
                        }
                        if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handles[0]) {
                            for i in 0..5 {
                                request.push([m[i*4], m[i*4+1], m[i*4+2], m[i*4+3]]);
                            }
                        }
                    }
                    Filter::Flood(ref color) => {
                        if self.extra_gpu_data_handles.is_empty() {
                            self.extra_gpu_data_handles.push(GpuCacheHandle::new());
                        }
                        if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handles[0]) {
                            request.push(color.to_array());
                        }
                    }
                    _ => {}
                }
            }
            PictureCompositeMode::ComponentTransferFilter(handle) => {
                let filter_data = &mut data_stores.filter_data[handle];
                filter_data.update(frame_state);
            }
            PictureCompositeMode::MixBlend(..) |
            PictureCompositeMode::Blit(_) |
            PictureCompositeMode::SvgFilter(..) => {}
        }

        true
    }
}

// Calculate a single homogeneous screen-space UV for a picture.
fn calculate_screen_uv(
    local_pos: &PicturePoint,
    transform: &PictureToRasterTransform,
    rendered_rect: &DeviceRect,
    device_pixel_scale: DevicePixelScale,
    supports_snapping: bool,
) -> DeviceHomogeneousVector {
    let raster_pos = transform.transform_point2d_homogeneous(*local_pos);

    let mut device_vec = DeviceHomogeneousVector::new(
        raster_pos.x * device_pixel_scale.0,
        raster_pos.y * device_pixel_scale.0,
        0.0,
        raster_pos.w,
    );

    // Apply snapping for axis-aligned scroll nodes, as per prim_shared.glsl.
    if transform.transform_kind() == TransformedRectKind::AxisAligned && supports_snapping {
        device_vec = DeviceHomogeneousVector::new(
            (device_vec.x / device_vec.w + 0.5).floor(),
            (device_vec.y / device_vec.w + 0.5).floor(),
            0.0,
            1.0,
        );
    }

    DeviceHomogeneousVector::new(
        (device_vec.x - rendered_rect.origin.x * device_vec.w) / rendered_rect.size.width,
        (device_vec.y - rendered_rect.origin.y * device_vec.w) / rendered_rect.size.height,
        0.0,
        device_vec.w,
    )
}

// Calculate a UV rect within an image based on the screen space
// vertex positions of a picture.
fn calculate_uv_rect_kind(
    pic_rect: &PictureRect,
    transform: &PictureToRasterTransform,
    rendered_rect: &DeviceIntRect,
    device_pixel_scale: DevicePixelScale,
    supports_snapping: bool,
) -> UvRectKind {
    let rendered_rect = rendered_rect.to_f32();

    let top_left = calculate_screen_uv(
        &pic_rect.origin,
        transform,
        &rendered_rect,
        device_pixel_scale,
        supports_snapping,
    );

    let top_right = calculate_screen_uv(
        &pic_rect.top_right(),
        transform,
        &rendered_rect,
        device_pixel_scale,
        supports_snapping,
    );

    let bottom_left = calculate_screen_uv(
        &pic_rect.bottom_left(),
        transform,
        &rendered_rect,
        device_pixel_scale,
        supports_snapping,
    );

    let bottom_right = calculate_screen_uv(
        &pic_rect.bottom_right(),
        transform,
        &rendered_rect,
        device_pixel_scale,
        supports_snapping,
    );

    UvRectKind::Quad {
        top_left,
        top_right,
        bottom_left,
        bottom_right,
    }
}

fn create_raster_mappers(
    surface_spatial_node_index: SpatialNodeIndex,
    raster_spatial_node_index: SpatialNodeIndex,
    world_rect: WorldRect,
    clip_scroll_tree: &ClipScrollTree,
) -> (SpaceMapper<RasterPixel, WorldPixel>, SpaceMapper<PicturePixel, RasterPixel>) {
    let map_raster_to_world = SpaceMapper::new_with_target(
        ROOT_SPATIAL_NODE_INDEX,
        raster_spatial_node_index,
        world_rect,
        clip_scroll_tree,
    );

    let raster_bounds = map_raster_to_world.unmap(&world_rect)
                                           .unwrap_or_else(RasterRect::max_rect);

    let map_pic_to_raster = SpaceMapper::new_with_target(
        raster_spatial_node_index,
        surface_spatial_node_index,
        raster_bounds,
        clip_scroll_tree,
    );

    (map_raster_to_world, map_pic_to_raster)
}

fn get_transform_key(
    spatial_node_index: SpatialNodeIndex,
    cache_spatial_node_index: SpatialNodeIndex,
    clip_scroll_tree: &ClipScrollTree,
) -> TransformKey {
    // Note: this is the only place where we don't know beforehand if the tile-affecting
    // spatial node is below or above the current picture.
    let transform = if cache_spatial_node_index >= spatial_node_index {
        clip_scroll_tree
            .get_relative_transform(
                cache_spatial_node_index,
                spatial_node_index,
            )
    } else {
        clip_scroll_tree
            .get_relative_transform(
                spatial_node_index,
                cache_spatial_node_index,
            )
    };
    transform.into()
}

/// A key for storing primitive comparison results during tile dependency tests.
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
struct PrimitiveComparisonKey {
    prev_index: PrimitiveDependencyIndex,
    curr_index: PrimitiveDependencyIndex,
}

/// Information stored an image dependency
#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct ImageDependency {
    key: ImageKey,
    generation: ImageGeneration,
}

/// A helper struct to compare a primitive and all its sub-dependencies.
struct PrimitiveComparer<'a> {
    clip_comparer: CompareHelper<'a, ItemUid>,
    transform_comparer: CompareHelper<'a, SpatialNodeIndex>,
    image_comparer: CompareHelper<'a, ImageDependency>,
    opacity_comparer: CompareHelper<'a, OpacityBinding>,
    resource_cache: &'a ResourceCache,
    spatial_nodes: &'a FastHashMap<SpatialNodeIndex, SpatialNodeDependency>,
    opacity_bindings: &'a FastHashMap<PropertyBindingId, OpacityBindingInfo>,
}

impl<'a> PrimitiveComparer<'a> {
    fn new(
        prev: &'a TileDescriptor,
        curr: &'a TileDescriptor,
        resource_cache: &'a ResourceCache,
        spatial_nodes: &'a FastHashMap<SpatialNodeIndex, SpatialNodeDependency>,
        opacity_bindings: &'a FastHashMap<PropertyBindingId, OpacityBindingInfo>,
    ) -> Self {
        let clip_comparer = CompareHelper::new(
            &prev.clips,
            &curr.clips,
        );

        let transform_comparer = CompareHelper::new(
            &prev.transforms,
            &curr.transforms,
        );

        let image_comparer = CompareHelper::new(
            &prev.images,
            &curr.images,
        );

        let opacity_comparer = CompareHelper::new(
            &prev.opacity_bindings,
            &curr.opacity_bindings,
        );

        PrimitiveComparer {
            clip_comparer,
            transform_comparer,
            image_comparer,
            opacity_comparer,
            resource_cache,
            spatial_nodes,
            opacity_bindings,
        }
    }

    fn reset(&mut self) {
        self.clip_comparer.reset();
        self.transform_comparer.reset();
        self.image_comparer.reset();
        self.opacity_comparer.reset();
    }

    fn advance_prev(&mut self, prim: &PrimitiveDescriptor) {
        self.clip_comparer.advance_prev(prim.clip_dep_count);
        self.transform_comparer.advance_prev(prim.transform_dep_count);
        self.image_comparer.advance_prev(prim.image_dep_count);
        self.opacity_comparer.advance_prev(prim.opacity_binding_dep_count);
    }

    fn advance_curr(&mut self, prim: &PrimitiveDescriptor) {
        self.clip_comparer.advance_curr(prim.clip_dep_count);
        self.transform_comparer.advance_curr(prim.transform_dep_count);
        self.image_comparer.advance_curr(prim.image_dep_count);
        self.opacity_comparer.advance_curr(prim.opacity_binding_dep_count);
    }

    /// Check if two primitive descriptors are the same.
    fn compare_prim(
        &mut self,
        prev: &PrimitiveDescriptor,
        curr: &PrimitiveDescriptor,
    ) -> PrimitiveCompareResult {
        let resource_cache = self.resource_cache;
        let spatial_nodes = self.spatial_nodes;
        let opacity_bindings = self.opacity_bindings;

        // Check equality of the PrimitiveDescriptor
        if prev != curr {
            return PrimitiveCompareResult::Descriptor;
        }

        // Check if any of the clips  this prim has are different.
        if !self.clip_comparer.is_same(
            prev.clip_dep_count,
            curr.clip_dep_count,
            |_| {
                false
            }
        ) {
            return PrimitiveCompareResult::Clip;
        }

        // Check if any of the transforms  this prim has are different.
        if !self.transform_comparer.is_same(
            prev.transform_dep_count,
            curr.transform_dep_count,
            |curr| {
                spatial_nodes[curr].changed
            }
        ) {
            return PrimitiveCompareResult::Transform;
        }

        // Check if any of the images this prim has are different.
        if !self.image_comparer.is_same(
            prev.image_dep_count,
            curr.image_dep_count,
            |curr| {
                resource_cache.get_image_generation(curr.key) != curr.generation
            }
        ) {
            return PrimitiveCompareResult::Image;
        }

        // Check if any of the opacity bindings this prim has are different.
        if !self.opacity_comparer.is_same(
            prev.opacity_binding_dep_count,
            curr.opacity_binding_dep_count,
            |curr| {
                if let OpacityBinding::Binding(id) = curr {
                    if opacity_bindings
                        .get(id)
                        .map_or(true, |info| info.changed) {
                        return true;
                    }
                }

                false
            }
        ) {
            return PrimitiveCompareResult::OpacityBinding;
        }

        PrimitiveCompareResult::Equal
    }
}

/// Details for a node in a quadtree that tracks dirty rects for a tile.
#[cfg_attr(any(feature="capture",feature="replay"), derive(Clone))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TileNodeKind {
    Leaf {
        /// The index buffer of primitives that affected this tile previous frame
        #[cfg_attr(any(feature = "capture", feature = "replay"), serde(skip))]
        prev_indices: Vec<PrimitiveDependencyIndex>,
        /// The index buffer of primitives that affect this tile on this frame
        #[cfg_attr(any(feature = "capture", feature = "replay"), serde(skip))]
        curr_indices: Vec<PrimitiveDependencyIndex>,
        /// A bitset of which of the last 64 frames have been dirty for this leaf.
        #[cfg_attr(any(feature = "capture", feature = "replay"), serde(skip))]
        dirty_tracker: u64,
        /// The number of frames since this node split or merged.
        #[cfg_attr(any(feature = "capture", feature = "replay"), serde(skip))]
        frames_since_modified: usize,
    },
    Node {
        /// The four children of this node
        children: Vec<TileNode>,
    },
}

/// The kind of modification that a tile wants to do
#[derive(Copy, Clone, PartialEq, Debug)]
enum TileModification {
    Split,
    Merge,
}

/// A node in the dirty rect tracking quadtree.
#[cfg_attr(any(feature="capture",feature="replay"), derive(Clone))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TileNode {
    /// Leaf or internal node
    pub kind: TileNodeKind,
    /// Rect of this node in the same space as the tile cache picture
    pub rect: PictureRect,
}

impl TileNode {
    /// Construct a new leaf node, with the given primitive dependency index buffer
    fn new_leaf(curr_indices: Vec<PrimitiveDependencyIndex>) -> Self {
        TileNode {
            kind: TileNodeKind::Leaf {
                prev_indices: Vec::new(),
                curr_indices,
                dirty_tracker: 0,
                frames_since_modified: 0,
            },
            rect: PictureRect::zero(),
        }
    }

    /// Draw debug information about this tile node
    fn draw_debug_rects(
        &self,
        pic_to_world_mapper: &SpaceMapper<PicturePixel, WorldPixel>,
        is_opaque: bool,
        scratch: &mut PrimitiveScratchBuffer,
        global_device_pixel_scale: DevicePixelScale,
    ) {
        match self.kind {
            TileNodeKind::Leaf { dirty_tracker, .. } => {
                let color = if (dirty_tracker & 1) != 0 {
                    debug_colors::RED
                } else if is_opaque {
                    debug_colors::GREEN
                } else {
                    debug_colors::YELLOW
                };

                let world_rect = pic_to_world_mapper.map(&self.rect).unwrap();
                let device_rect = world_rect * global_device_pixel_scale;

                let outer_color = color.scale_alpha(0.3);
                let inner_color = outer_color.scale_alpha(0.5);
                scratch.push_debug_rect(
                    device_rect.inflate(-3.0, -3.0),
                    outer_color,
                    inner_color
                );
            }
            TileNodeKind::Node { ref children, .. } => {
                for child in children.iter() {
                    child.draw_debug_rects(
                        pic_to_world_mapper,
                        is_opaque,
                        scratch,
                        global_device_pixel_scale,
                    );
                }
            }
        }
    }

    /// Calculate the four child rects for a given node
    fn get_child_rects(
        rect: &PictureRect,
        result: &mut [PictureRect; 4],
    ) {
        let p0 = rect.origin;
        let half_size = PictureSize::new(rect.size.width * 0.5, rect.size.height * 0.5);

        *result = [
            PictureRect::new(
                PicturePoint::new(p0.x, p0.y),
                half_size,
            ),
            PictureRect::new(
                PicturePoint::new(p0.x + half_size.width, p0.y),
                half_size,
            ),
            PictureRect::new(
                PicturePoint::new(p0.x, p0.y + half_size.height),
                half_size,
            ),
            PictureRect::new(
                PicturePoint::new(p0.x + half_size.width, p0.y + half_size.height),
                half_size,
            ),
        ];
    }

    /// Called during pre_update, to clear the current dependencies
    fn clear(
        &mut self,
        rect: PictureRect,
    ) {
        self.rect = rect;

        match self.kind {
            TileNodeKind::Leaf { ref mut prev_indices, ref mut curr_indices, ref mut dirty_tracker, ref mut frames_since_modified } => {
                // Swap current dependencies to be the previous frame
                mem::swap(prev_indices, curr_indices);
                curr_indices.clear();
                // Note that another frame has passed in the dirty bit trackers
                *dirty_tracker = *dirty_tracker << 1;
                *frames_since_modified += 1;
            }
            TileNodeKind::Node { ref mut children, .. } => {
                let mut child_rects = [PictureRect::zero(); 4];
                TileNode::get_child_rects(&rect, &mut child_rects);
                assert_eq!(child_rects.len(), children.len());

                for (child, rect) in children.iter_mut().zip(child_rects.iter()) {
                    child.clear(*rect);
                }
            }
        }
    }

    /// Add a primitive dependency to this node
    fn add_prim(
        &mut self,
        index: PrimitiveDependencyIndex,
        prim_rect: &PictureRect,
    ) {
        match self.kind {
            TileNodeKind::Leaf { ref mut curr_indices, .. } => {
                curr_indices.push(index);
            }
            TileNodeKind::Node { ref mut children, .. } => {
                for child in children.iter_mut() {
                    if child.rect.intersects(prim_rect) {
                        child.add_prim(index, prim_rect);
                    }
                }
            }
        }
    }

    /// Apply a merge or split operation to this tile, if desired
    fn maybe_merge_or_split(
        &mut self,
        level: i32,
        curr_prims: &[PrimitiveDescriptor],
        max_split_levels: i32,
    ) {
        // Determine if this tile wants to split or merge
        let mut tile_mod = None;

        fn get_dirty_frames(
            dirty_tracker: u64,
            frames_since_modified: usize,
        ) -> Option<u32> {
            // Only consider splitting or merging at least 64 frames since we last changed
            if frames_since_modified > 64 {
                // Each bit in the tracker is a frame that was recently invalidated
                Some(dirty_tracker.count_ones())
            } else {
                None
            }
        }

        match self.kind {
            TileNodeKind::Leaf { dirty_tracker, frames_since_modified, .. } => {
                // Only consider splitting if the tree isn't too deep.
                if level < max_split_levels {
                    if let Some(dirty_frames) = get_dirty_frames(dirty_tracker, frames_since_modified) {
                        // If the tile has invalidated > 50% of the recent number of frames, split.
                        if dirty_frames > 32 {
                            tile_mod = Some(TileModification::Split);
                        }
                    }
                }
            }
            TileNodeKind::Node { ref children, .. } => {
                // There's two conditions that cause a node to merge its children:
                // (1) If _all_ the child nodes are constantly invalidating, then we are wasting
                //     CPU time tracking dependencies for each child, so merge them.
                // (2) If _none_ of the child nodes are recently invalid, then the page content
                //     has probably changed, and we no longer need to track fine grained dependencies here.

                let mut static_count = 0;
                let mut changing_count = 0;

                for child in children {
                    // Only consider merging nodes at the edge of the tree.
                    if let TileNodeKind::Leaf { dirty_tracker, frames_since_modified, .. } = child.kind {
                        if let Some(dirty_frames) = get_dirty_frames(dirty_tracker, frames_since_modified) {
                            if dirty_frames == 0 {
                                // Hasn't been invalidated for some time
                                static_count += 1;
                            } else if dirty_frames == 64 {
                                // Is constantly being invalidated
                                changing_count += 1;
                            }
                        }
                    }

                    // Only merge if all the child tiles are in agreement. Otherwise, we have some
                    // that are invalidating / static, and it's worthwhile tracking dependencies for
                    // them individually.
                    if static_count == 4 || changing_count == 4 {
                        tile_mod = Some(TileModification::Merge);
                    }
                }
            }
        }

        match tile_mod {
            Some(TileModification::Split) => {
                // To split a node, take the current dependency index buffer for this node, and
                // split it into child index buffers.
                let curr_indices = match self.kind {
                    TileNodeKind::Node { .. } => {
                        unreachable!("bug - only leaves can split");
                    }
                    TileNodeKind::Leaf { ref mut curr_indices, .. } => {
                        curr_indices.take()
                    }
                };

                let mut child_rects = [PictureRect::zero(); 4];
                TileNode::get_child_rects(&self.rect, &mut child_rects);

                let mut child_indices = [
                    Vec::new(),
                    Vec::new(),
                    Vec::new(),
                    Vec::new(),
                ];

                // Step through the index buffer, and add primitives to each of the children
                // that they intersect.
                for index in curr_indices {
                    let prim = &curr_prims[index.0 as usize];
                    for (child_rect, indices) in child_rects.iter().zip(child_indices.iter_mut()) {
                        let child_rect_key: RectangleKey = (*child_rect).into();
                        if prim.prim_clip_rect.intersects(&child_rect_key) {
                            indices.push(index);
                        }
                    }
                }

                // Create the child nodes and switch from leaf -> node.
                let children = child_indices
                    .iter_mut()
                    .map(|i| TileNode::new_leaf(mem::replace(i, Vec::new())))
                    .collect();

                self.kind = TileNodeKind::Node {
                    children,
                };
            }
            Some(TileModification::Merge) => {
                // Construct a merged index buffer by collecting the dependency index buffers
                // from each child, and merging them into a de-duplicated index buffer.
                let merged_indices = match self.kind {
                    TileNodeKind::Node { ref mut children, .. } => {
                        let mut merged_indices = Vec::new();

                        for child in children.iter() {
                            let child_indices = match child.kind {
                                TileNodeKind::Leaf { ref curr_indices, .. } => {
                                    curr_indices
                                }
                                TileNodeKind::Node { .. } => {
                                    unreachable!("bug: child is not a leaf");
                                }
                            };
                            merged_indices.extend_from_slice(child_indices);
                        }

                        merged_indices.sort();
                        merged_indices.dedup();

                        merged_indices
                    }
                    TileNodeKind::Leaf { .. } => {
                        unreachable!("bug - trying to merge a leaf");
                    }
                };

                // Switch from a node to a leaf, with the combined index buffer
                self.kind = TileNodeKind::Leaf {
                    prev_indices: Vec::new(),
                    curr_indices: merged_indices,
                    dirty_tracker: 0,
                    frames_since_modified: 0,
                };
            }
            None => {
                // If this node didn't merge / split, then recurse into children
                // to see if they want to split / merge.
                if let TileNodeKind::Node { ref mut children, .. } = self.kind {
                    for child in children.iter_mut() {
                        child.maybe_merge_or_split(
                            level+1,
                            curr_prims,
                            max_split_levels,
                        );
                    }
                }
            }
        }
    }

    /// Update the dirty state of this node, building the overall dirty rect
    fn update_dirty_rects(
        &mut self,
        prev_prims: &[PrimitiveDescriptor],
        curr_prims: &[PrimitiveDescriptor],
        prim_comparer: &mut PrimitiveComparer,
        dirty_rect: &mut PictureRect,
        compare_cache: &mut FastHashMap<PrimitiveComparisonKey, PrimitiveCompareResult>,
        invalidation_reason: &mut Option<InvalidationReason>,
    ) {
        match self.kind {
            TileNodeKind::Node { ref mut children, .. } => {
                for child in children.iter_mut() {
                    child.update_dirty_rects(
                        prev_prims,
                        curr_prims,
                        prim_comparer,
                        dirty_rect,
                        compare_cache,
                        invalidation_reason,
                    );
                }
            }
            TileNodeKind::Leaf { ref prev_indices, ref curr_indices, ref mut dirty_tracker, .. } => {
                // If the index buffers are of different length, they must be different
                if prev_indices.len() == curr_indices.len() {
                    let mut prev_i0 = 0;
                    let mut prev_i1 = 0;
                    prim_comparer.reset();

                    // Walk each index buffer, comparing primitives
                    for (prev_index, curr_index) in prev_indices.iter().zip(curr_indices.iter()) {
                        let i0 = prev_index.0 as usize;
                        let i1 = curr_index.0 as usize;

                        // Advance the dependency arrays for each primitive (this handles
                        // prims that may be skipped by these index buffers).
                        for i in prev_i0 .. i0 {
                            prim_comparer.advance_prev(&prev_prims[i]);
                        }
                        for i in prev_i1 .. i1 {
                            prim_comparer.advance_curr(&curr_prims[i]);
                        }

                        // Compare the primitives, caching the result in a hash map
                        // to save comparisons in other tree nodes.
                        let key = PrimitiveComparisonKey {
                            prev_index: *prev_index,
                            curr_index: *curr_index,
                        };

                        let prim_compare_result = *compare_cache
                            .entry(key)
                            .or_insert_with(|| {
                                let prev = &prev_prims[i0];
                                let curr = &curr_prims[i1];
                                prim_comparer.compare_prim(prev, curr)
                            });

                        // If not the same, mark this node as dirty and update the dirty rect
                        if prim_compare_result != PrimitiveCompareResult::Equal {
                            if invalidation_reason.is_none() {
                                *invalidation_reason = Some(InvalidationReason::Content {
                                    prim_compare_result,
                                });
                            }
                            *dirty_rect = self.rect.union(dirty_rect);
                            *dirty_tracker = *dirty_tracker | 1;
                            break;
                        }

                        prev_i0 = i0;
                        prev_i1 = i1;
                    }
                } else {
                    if invalidation_reason.is_none() {
                        *invalidation_reason = Some(InvalidationReason::PrimCount);
                    }
                    *dirty_rect = self.rect.union(dirty_rect);
                    *dirty_tracker = *dirty_tracker | 1;
                }
            }
        }
    }
}

impl CompositeState {
    // A helper function to destroy all native surfaces for a given list of tiles
    pub fn destroy_native_tiles<'a, I: Iterator<Item = &'a mut Tile>>(
        &mut self,
        tiles_iter: I,
        resource_cache: &mut ResourceCache,
    ) {
        // Any old tiles that remain after the loop above are going to be dropped. For
        // simple composite mode, the texture cache handle will expire and be collected
        // by the texture cache. For native compositor mode, we need to explicitly
        // invoke a callback to the client to destroy that surface.
        if let CompositorKind::Native { .. } = self.compositor_kind {
            for tile in tiles_iter {
                // Only destroy native surfaces that have been allocated. It's
                // possible for display port tiles to be created that never
                // come on screen, and thus never get a native surface allocated.
                if let Some(TileSurface::Texture { descriptor: SurfaceTextureDescriptor::Native { ref mut id, .. }, .. }) = tile.surface {
                    if let Some(id) = id.take() {
                        resource_cache.destroy_compositor_tile(id);
                    }
                }
            }
        }
    }
}

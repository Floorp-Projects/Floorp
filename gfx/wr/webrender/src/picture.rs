/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceRect, FilterOp, MixBlendMode, PipelineId, PremultipliedColorF, PictureRect, PicturePoint, WorldPoint};
use api::{DeviceIntRect, DevicePoint, LayoutRect, PictureToRasterTransform, LayoutPixel, PropertyBinding, PropertyBindingId};
use api::{DevicePixelScale, RasterRect, RasterSpace, ColorF, ImageKey, DirtyRect, WorldSize, ClipMode, LayoutSize};
use api::{PicturePixel, RasterPixel, WorldPixel, WorldRect, ImageFormat, ImageDescriptor, WorldVector2D, LayoutPoint};
#[cfg(feature = "debug_renderer")]
use api::{DebugFlags, DeviceVector2D};
use box_shadow::{BLUR_SAMPLE_SCALE};
use clip::{ClipStore, ClipChainId, ClipChainNode, ClipItem};
use clip_scroll_tree::{ROOT_SPATIAL_NODE_INDEX, ClipScrollTree, SpatialNodeIndex, CoordinateSystemId};
#[cfg(feature = "debug_renderer")]
use debug_colors;
use device::TextureFilter;
use euclid::{TypedScale, vec3, TypedRect, TypedPoint2D, TypedSize2D};
use euclid::approxeq::ApproxEq;
use frame_builder::{FrameVisibilityContext, FrameVisibilityState};
use intern::ItemUid;
use internal_types::{FastHashMap, FastHashSet, PlaneSplitter};
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureState, PictureContext};
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use gpu_types::{TransformPalette, TransformPaletteId, UvRectKind};
use plane_split::{Clipper, Polygon, Splitter};
use prim_store::{PictureIndex, PrimitiveInstance, SpaceMapper, VisibleFace, PrimitiveInstanceKind};
use prim_store::{get_raster_rects, PrimitiveScratchBuffer, VectorKey, PointKey};
use prim_store::{OpacityBindingStorage, ImageInstanceStorage, OpacityBindingIndex, RectangleKey};
use print_tree::PrintTreePrinter;
use render_backend::DataStores;
use render_task::{ClearMode, RenderTask, RenderTaskCacheEntryHandle, TileBlit};
use render_task::{RenderTaskCacheKey, RenderTaskCacheKeyKind, RenderTaskId, RenderTaskLocation};
use resource_cache::ResourceCache;
use scene::{FilterOpHelpers, SceneProperties};
use scene_builder::Interners;
use smallvec::SmallVec;
use surface::{SurfaceDescriptor};
use std::{mem, u16};
use std::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT, Ordering};
use texture_cache::{Eviction, TextureCacheHandle};
use tiling::RenderTargetKind;
use util::{ComparableVec, TransformedRectKind, MatrixHelpers, MaxRect};

/*
 A picture represents a dynamically rendered image. It consists of:

 * A number of primitives that are drawn onto the picture.
 * A composite operation describing how to composite this
   picture into its parent.
 * A configuration describing how to draw the primitives on
   this picture (e.g. in screen space or local space).
 */

/// Information about a picture that is pushed / popped on the
/// PictureUpdateState during picture traversal pass.
struct PictureInfo {
    /// The spatial node for this picture.
    spatial_node_index: SpatialNodeIndex,
}

/// Stores a list of cached picture tiles that are retained
/// between new scenes.
pub struct RetainedTiles {
    /// The tiles retained between display lists.
    pub tiles: Vec<Tile>,
    /// List of reference primitives that we will compare
    /// to try and correlate the positioning of items
    /// between display lists.
    pub ref_prims: FastHashMap<ItemUid, WorldPoint>,
}

impl RetainedTiles {
    pub fn new() -> Self {
        RetainedTiles {
            tiles: Vec::new(),
            ref_prims: FastHashMap::default(),
        }
    }

    /// Merge items from one retained tiles into another.
    pub fn merge(&mut self, other: RetainedTiles) {
        assert!(self.tiles.is_empty() || other.tiles.is_empty());
        self.tiles.extend(other.tiles);
        self.ref_prims.extend(other.ref_prims);
    }
}

/// Unit for tile coordinates.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct TileCoordinate;

// Geometry types for tile coordinates.
pub type TileOffset = TypedPoint2D<i32, TileCoordinate>;
pub type TileSize = TypedSize2D<i32, TileCoordinate>;
pub struct TileIndex(pub usize);

/// The size in device pixels of a cached tile. The currently chosen
/// size is arbitrary. We should do some profiling to find the best
/// size for real world pages.
pub const TILE_SIZE_WIDTH: i32 = 1024;
pub const TILE_SIZE_HEIGHT: i32 = 256;
const FRAMES_BEFORE_CACHING: usize = 2;
const MAX_DIRTY_RECTS: usize = 3;

/// The maximum size per axis of texture cache item,
///  in WorldPixel coordinates.
// TODO(gw): This size is quite arbitrary - we should do some
//           profiling / telemetry to see when it makes sense
//           to cache a picture.
const MAX_CACHE_SIZE: f32 = 2048.0;
/// The maximum size per axis of a surface,
///  in WorldPixel coordinates.
const MAX_SURFACE_SIZE: f32 = 4096.0;


/// The maximum number of primitives to look for in a display
/// list, trying to find unique primitives.
const MAX_PRIMS_TO_SEARCH: usize = 128;

/// Used to get unique tile IDs, even when the tile cache is
/// destroyed between display lists / scenes.
static NEXT_TILE_ID: AtomicUsize = ATOMIC_USIZE_INIT;

fn clamp(value: i32, low: i32, high: i32) -> i32 {
    value.max(low).min(high)
}

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

/// A stable ID for a given tile, to help debugging.
#[derive(Debug, Copy, Clone, PartialEq)]
struct TileId(usize);

/// Information about a cached tile.
#[derive(Debug)]
pub struct Tile {
    /// The current world rect of thie tile.
    world_rect: WorldRect,
    /// The current local rect of this tile.
    pub local_rect: LayoutRect,
    /// The currently visible rect within this tile, updated per frame.
    /// If None, this tile is not currently visible.
    visible_rect: Option<WorldRect>,
    /// The currently valid rect of the tile, used to invalidate
    /// tiles that were only partially drawn.
    valid_rect: WorldRect,
    /// Uniquely describes the content of this tile, in a way that can be
    /// (reasonably) efficiently hashed and compared.
    descriptor: TileDescriptor,
    /// Handle to the cached texture for this tile.
    pub handle: TextureCacheHandle,
    /// If true, this tile is marked valid, and the existing texture
    /// cache handle can be used. Tiles are invalidated during the
    /// build_dirty_regions method.
    is_valid: bool,
    /// If true, the content on this tile is the same as last frame.
    is_same_content: bool,
    /// The number of frames this tile has had the same content.
    same_frames: usize,
    /// The tile id is stable between display lists and / or frames,
    /// if the tile is retained. Useful for debugging tile evictions.
    id: TileId,
    /// The set of transforms that affect primitives on this tile we
    /// care about. Stored as a set here, and then collected, sorted
    /// and converted to transform key values during post_update.
    transforms: FastHashSet<SpatialNodeIndex>,
    /// A list of potentially important clips. We can't know if
    /// they were important or can be discarded until we know the
    /// tile cache bounding rect.
    potential_clips: FastHashMap<RectangleKey, SpatialNodeIndex>,
    /// If true, this tile should still be considered as part of
    /// the dirty rect calculations.
    consider_for_dirty_rect: bool,
}

impl Tile {
    /// Construct a new, invalid tile.
    fn new(
        id: TileId,
    ) -> Self {
        Tile {
            local_rect: LayoutRect::zero(),
            world_rect: WorldRect::zero(),
            visible_rect: None,
            valid_rect: WorldRect::zero(),
            handle: TextureCacheHandle::invalid(),
            descriptor: TileDescriptor::new(),
            is_same_content: false,
            is_valid: false,
            same_frames: 0,
            transforms: FastHashSet::default(),
            potential_clips: FastHashMap::default(),
            id,
            consider_for_dirty_rect: false,
        }
    }

    /// Clear the dependencies for a tile.
    fn clear(&mut self) {
        self.transforms.clear();
        self.descriptor.clear();
        self.potential_clips.clear();
    }

    /// Invalidate a tile based on change in content. This
    /// muct be called even if the tile is not currently
    /// visible on screen. We might be able to improve this
    /// later by changing how ComparableVec is used.
    fn update_content_validity(&mut self) {
        // Check if the contents of the primitives, clips, and
        // other dependencies are the same.
        self.is_same_content &= self.descriptor.is_same_content();
        self.is_valid &= self.is_same_content;
    }

    /// Update state related to whether a tile has a valid rect that
    /// covers the required visible part of the tile.
    fn update_rect_validity(&mut self, tile_bounding_rect: &WorldRect) {
        // The tile is only valid if:
        // - The content is the same *and*
        // - The valid part of the tile includes the needed part.
        self.is_valid &= self.valid_rect.contains_rect(tile_bounding_rect);

        // Update count of how many times this tile has had the same content.
        if !self.is_same_content {
            self.same_frames = 0;
        }
        self.same_frames += 1;
    }
}

/// Defines a key that uniquely identifies a primitive instance.
#[derive(Debug, Clone, PartialEq)]
pub struct PrimitiveDescriptor {
    /// Uniquely identifies the content of the primitive template.
    prim_uid: ItemUid,
    /// The origin in world space of this primitive.
    origin: WorldPoint,
    /// The first clip in the clip_uids array of clips that affect this tile.
    first_clip: u16,
    /// The number of clips that affect this primitive instance.
    clip_count: u16,
    /// The combined local clips + prim rect for this primitive.
    world_culling_rect: WorldRect,
}

/// Uniquely describes the content of this tile, in a way that can be
/// (reasonably) efficiently hashed and compared.
#[derive(Debug)]
pub struct TileDescriptor {
    /// List of primitive instance unique identifiers. The uid is guaranteed
    /// to uniquely describe the content of the primitive template, while
    /// the other parameters describe the clip chain and instance params.
    prims: ComparableVec<PrimitiveDescriptor>,

    /// List of clip node unique identifiers. The uid is guaranteed
    /// to uniquely describe the content of the clip node.
    clip_uids: ComparableVec<ItemUid>,

    /// List of local offsets of the clip node origins. This
    /// ensures that if a clip node is supplied but has a different
    /// transform between frames that the tile is invalidated.
    clip_vertices: ComparableVec<PointKey>,

    /// List of image keys that this tile depends on.
    image_keys: ComparableVec<ImageKey>,

    /// The set of opacity bindings that this tile depends on.
    // TODO(gw): Ugh, get rid of all opacity binding support!
    opacity_bindings: ComparableVec<OpacityBinding>,

    /// List of the effects of transforms that we care about
    /// tracking for this tile.
    transforms: ComparableVec<PointKey>,
}

impl TileDescriptor {
    fn new() -> Self {
        TileDescriptor {
            prims: ComparableVec::new(),
            clip_uids: ComparableVec::new(),
            clip_vertices: ComparableVec::new(),
            opacity_bindings: ComparableVec::new(),
            image_keys: ComparableVec::new(),
            transforms: ComparableVec::new(),
        }
    }

    /// Clear the dependency information for a tile, when the dependencies
    /// are being rebuilt.
    fn clear(&mut self) {
        self.prims.reset();
        self.clip_uids.reset();
        self.clip_vertices.reset();
        self.opacity_bindings.reset();
        self.image_keys.reset();
        self.transforms.reset();
    }

    /// Return true if the content of the tile is the same
    /// as last frame. This doesn't check validity of the
    /// tile based on the currently valid regions.
    fn is_same_content(&self) -> bool {
        if !self.image_keys.is_valid() {
            return false;
        }
        if !self.opacity_bindings.is_valid() {
            return false;
        }
        if !self.clip_uids.is_valid() {
            return false;
        }
        if !self.clip_vertices.is_valid() {
            return false;
        }
        if !self.prims.is_valid() {
            return false;
        }
        if !self.transforms.is_valid() {
            return false;
        }

        true
    }
}

/// Stores both the world and devices rects for a single dirty rect.
#[derive(Debug, Clone)]
pub struct DirtyRegionRect {
    pub world_rect: WorldRect,
    pub device_rect: DeviceIntRect,
}

/// Represents the dirty region of a tile cache picture.
#[derive(Debug, Clone)]
pub struct DirtyRegion {
    /// The individual dirty rects of this region.
    pub dirty_rects: Vec<DirtyRegionRect>,

    /// The overall dirty rect, a combination of dirty_rects
    pub combined: DirtyRegionRect,
}

impl DirtyRegion {
    /// Construct a new dirty region tracker.
    pub fn new() -> Self {
        DirtyRegion {
            dirty_rects: Vec::with_capacity(MAX_DIRTY_RECTS),
            combined: DirtyRegionRect {
                world_rect: WorldRect::zero(),
                device_rect: DeviceIntRect::zero(),
            },
        }
    }

    /// Reset the dirty regions back to empty
    pub fn clear(&mut self) {
        self.dirty_rects.clear();
        self.combined = DirtyRegionRect {
            world_rect: WorldRect::zero(),
            device_rect: DeviceIntRect::zero(),
        }
    }

    /// Push a dirty rect into this region
    pub fn push(
        &mut self,
        rect: WorldRect,
        device_pixel_scale: DevicePixelScale,
    ) {
        let device_rect = (rect * device_pixel_scale).round().to_i32();

        // Include this in the overall dirty rect
        self.combined.world_rect = self.combined.world_rect.union(&rect);
        self.combined.device_rect = self.combined.device_rect.union(&device_rect);

        // Store the individual dirty rect.
        self.dirty_rects.push(DirtyRegionRect {
            world_rect: rect,
            device_rect,
        });
    }

    /// Returns true if this region has no dirty rects
    pub fn is_empty(&self) -> bool {
        self.dirty_rects.is_empty()
    }

    /// Collapse all dirty rects into a single dirty rect.
    pub fn collapse(&mut self) {
        self.dirty_rects.clear();
        self.dirty_rects.push(self.combined.clone());
    }
}

/// A helper struct to build a (roughly) minimal set of dirty rectangles
/// from a list of individual dirty rectangles. This minimizes the number
/// of scissors rects and batch resubmissions that are needed.
struct DirtyRegionBuilder<'a> {
    tiles: &'a mut [Tile],
    tile_count: TileSize,
    device_pixel_scale: DevicePixelScale,
}

impl<'a> DirtyRegionBuilder<'a> {
    fn new(
        tiles: &'a mut [Tile],
        tile_count: TileSize,
        device_pixel_scale: DevicePixelScale,
    ) -> Self {
        DirtyRegionBuilder {
            tiles,
            tile_count,
            device_pixel_scale,
        }
    }

    fn tile_index(&self, x: i32, y: i32) -> usize {
        (y * self.tile_count.width + x) as usize
    }

    fn is_dirty(&self, x: i32, y: i32) -> bool {
        if x == self.tile_count.width || y == self.tile_count.height {
            return false;
        }

        self.get_tile(x, y).consider_for_dirty_rect
    }

    fn get_tile(&self, x: i32, y: i32) -> &Tile {
        &self.tiles[self.tile_index(x, y)]
    }

    fn get_tile_mut(&mut self, x: i32, y: i32) -> &mut Tile {
        &mut self.tiles[self.tile_index(x, y)]
    }

    /// Return true if the entire column is dirty
    fn column_is_dirty(&self, x: i32, y0: i32, y1: i32) -> bool {
        for y in y0 .. y1 {
            if !self.is_dirty(x, y) {
                return false;
            }
        }

        true
    }

    /// Push a dirty rect into the final region list.
    fn push_dirty_rect(
        &mut self,
        x0: i32,
        y0: i32,
        x1: i32,
        y1: i32,
        dirty_region: &mut DirtyRegion,
    ) {
        // Construct the overall dirty rect by combining the visible
        // parts of the dirty rects that were combined.
        let mut dirty_world_rect = WorldRect::zero();

        for y in y0 .. y1 {
            for x in x0 .. x1 {
                let tile = self.get_tile_mut(x, y);
                tile.consider_for_dirty_rect = false;
                if let Some(visible_rect) = tile.visible_rect {
                    dirty_world_rect = dirty_world_rect.union(&visible_rect);
                }
            }
        }

        dirty_region.push(dirty_world_rect, self.device_pixel_scale);
    }

    /// Simple sweep through the tile grid to try and coalesce individual
    /// dirty rects into a smaller number of larger dirty rectangles.
    fn build(&mut self, dirty_region: &mut DirtyRegion) {
        for x0 in 0 .. self.tile_count.width {
            for y0 in 0 .. self.tile_count.height {
                let mut y1 = y0;

                while self.is_dirty(x0, y1) {
                    y1 += 1;
                }

                if y1 > y0 {
                    let mut x1 = x0;

                    while self.column_is_dirty(x1, y0, y1) {
                        x1 += 1;
                    }

                    self.push_dirty_rect(x0, y0, x1, y1, dirty_region);
                }
            }
        }
    }
}

/// Represents a cache of tiles that make up a picture primitives.
pub struct TileCache {
    /// The positioning node for this tile cache.
    spatial_node_index: SpatialNodeIndex,
    /// List of tiles present in this picture (stored as a 2D array)
    pub tiles: Vec<Tile>,
    /// A helper struct to map local rects into world coords.
    map_local_to_world: SpaceMapper<LayoutPixel, WorldPixel>,
    /// A list of tiles to draw during batching.
    pub tiles_to_draw: Vec<TileIndex>,
    /// List of opacity bindings, with some extra information
    /// about whether they changed since last frame.
    opacity_bindings: FastHashMap<PropertyBindingId, OpacityBindingInfo>,
    /// The current dirty region tracker for this picture.
    pub dirty_region: DirtyRegion,
    /// The current world reference point that tiles are created around.
    world_origin: WorldPoint,
    /// Current size of tiles in world units.
    world_tile_size: WorldSize,
    /// Current number of tiles in the allocated grid.
    tile_count: TileSize,
    /// The current scroll offset for this frame builder. Reset when
    /// a new scene arrives.
    scroll_offset: Option<WorldVector2D>,
    /// A list of blits from the framebuffer to be applied during this frame.
    pub pending_blits: Vec<TileBlit>,
    /// The current world bounding rect of this tile cache. This is used
    /// to derive a local clip rect, such that we don't obscure in the
    /// z-buffer any items placed earlier in the render order (such as
    /// scroll bars in gecko, when the content overflows under the
    /// scroll bar).
    world_bounding_rect: WorldRect,
    /// World space clip rect of the root clipping node. Every primitive
    /// has this as the root of the clip chain attached to the primitive.
    root_clip_rect: WorldRect,
    /// List of reference primitive information used for
    /// correlating the position between display lists.
    reference_prims: ReferencePrimitiveList,
    /// The root clip chain for this tile cache.
    root_clip_chain_id: ClipChainId,
}

/// Stores information about a primitive in the cache that we will
/// try to use to correlate positions between display lists.
#[derive(Clone)]
struct ReferencePrimitive {
    uid: ItemUid,
    local_pos: LayoutPoint,
    spatial_node_index: SpatialNodeIndex,
    ref_count: usize,
}

/// A list of primitive with uids that only exist once in a display
/// list. Used to obtain reference points to correlate the offset
/// between two similar display lists.
struct ReferencePrimitiveList {
    ref_prims: Vec<ReferencePrimitive>,
}

impl ReferencePrimitiveList {
    fn new(
        prim_instances: &[PrimitiveInstance],
        pictures: &[PicturePrimitive],
    ) -> Self {
        let mut map = FastHashMap::default();
        let mut search_count = 0;

        // Collect a set of primitives that we can
        // potentially use for correlation.
        collect_ref_prims(
            prim_instances,
            pictures,
            &mut map,
            &mut search_count,
        );

        // Select only primitives where the uid is unique
        // in the display list, giving the best chance
        // of finding correct correlations.
        let ref_prims = map.values().filter(|prim| {
            prim.ref_count == 1
        }).cloned().collect();

        ReferencePrimitiveList {
            ref_prims,
        }
    }
}

/// Collect a sample of primitives from the prim list that can
/// be used to correlate positions.
fn collect_ref_prims(
    prim_instances: &[PrimitiveInstance],
    pictures: &[PicturePrimitive],
    map: &mut FastHashMap<ItemUid, ReferencePrimitive>,
    search_count: &mut usize,
) {
    for prim_instance in prim_instances {
        if *search_count > MAX_PRIMS_TO_SEARCH {
            return;
        }

        match prim_instance.kind {
            PrimitiveInstanceKind::Picture { pic_index, .. } => {
                collect_ref_prims(
                    &pictures[pic_index.0].prim_list.prim_instances,
                    pictures,
                    map,
                    search_count,
                );
            }
            _ => {
                let uid = prim_instance.uid();

                let entry = map.entry(uid).or_insert_with(|| {
                    ReferencePrimitive {
                        uid,
                        local_pos: prim_instance.prim_origin,
                        spatial_node_index: prim_instance.spatial_node_index,
                        ref_count: 0,
                    }
                });
                entry.ref_count += 1;

                *search_count = *search_count + 1;
            }
        }
    }
}

impl TileCache {
    pub fn new(
        spatial_node_index: SpatialNodeIndex,
        prim_instances: &[PrimitiveInstance],
        root_clip_chain_id: ClipChainId,
        pictures: &[PicturePrimitive],
    ) -> Self {
        // Build the list of reference primitives
        // for this picture cache.
        let reference_prims = ReferencePrimitiveList::new(
            prim_instances,
            pictures,
        );

        TileCache {
            spatial_node_index,
            tiles: Vec::new(),
            map_local_to_world: SpaceMapper::new(
                ROOT_SPATIAL_NODE_INDEX,
                WorldRect::zero(),
            ),
            tiles_to_draw: Vec::new(),
            opacity_bindings: FastHashMap::default(),
            dirty_region: DirtyRegion::new(),
            world_origin: WorldPoint::zero(),
            world_tile_size: WorldSize::zero(),
            tile_count: TileSize::zero(),
            scroll_offset: None,
            pending_blits: Vec::new(),
            world_bounding_rect: WorldRect::zero(),
            root_clip_rect: WorldRect::max_rect(),
            reference_prims,
            root_clip_chain_id,
        }
    }

    /// Get the tile coordinates for a given rectangle.
    fn get_tile_coords_for_rect(
        &self,
        rect: &WorldRect,
    ) -> (TileOffset, TileOffset) {
        // Translate the rectangle into the virtual tile space
        let origin = rect.origin - self.world_origin;

        // Get the tile coordinates in the picture space.
        let mut p0 = TileOffset::new(
            (origin.x / self.world_tile_size.width).floor() as i32,
            (origin.y / self.world_tile_size.height).floor() as i32,
        );

        let mut p1 = TileOffset::new(
            ((origin.x + rect.size.width) / self.world_tile_size.width).ceil() as i32,
            ((origin.y + rect.size.height) / self.world_tile_size.height).ceil() as i32,
        );

        // Clamp the tile coordinates here to avoid looping over irrelevant tiles later on.
        p0.x = clamp(p0.x, 0, self.tile_count.width);
        p0.y = clamp(p0.y, 0, self.tile_count.height);
        p1.x = clamp(p1.x, 0, self.tile_count.width);
        p1.y = clamp(p1.y, 0, self.tile_count.height);

        (p0, p1)
    }

    /// Update transforms, opacity bindings and tile rects.
    pub fn pre_update(
        &mut self,
        pic_rect: LayoutRect,
        frame_context: &FrameVisibilityContext,
        frame_state: &mut FrameVisibilityState,
    ) {
        // Work out the scroll offset to apply to the world reference point.
        let scroll_transform = frame_context.clip_scroll_tree.get_relative_transform(
            ROOT_SPATIAL_NODE_INDEX,
            self.spatial_node_index,
        ).expect("bug: unable to get scroll transform");
        let scroll_offset = WorldVector2D::new(
            scroll_transform.m41,
            scroll_transform.m42,
        );
        let scroll_delta = match self.scroll_offset {
            Some(prev) => prev - scroll_offset,
            None => WorldVector2D::zero(),
        };
        self.scroll_offset = Some(scroll_offset);

        // Pull any retained tiles from the previous scene.
        let world_offset = if frame_state.retained_tiles.tiles.is_empty() {
            None
        } else {
            assert!(self.tiles.is_empty());
            self.tiles = mem::replace(&mut frame_state.retained_tiles.tiles, Vec::new());

            // Get the positions of the reference primitives for this
            // new display list.
            let mut new_prim_map = FastHashMap::default();
            build_ref_prims(
                &self.reference_prims.ref_prims,
                &mut new_prim_map,
                frame_context.clip_scroll_tree,
            );

            // Attempt to correlate them to work out which offset to apply.
            correlate_prim_maps(
                &frame_state.retained_tiles.ref_prims,
                &new_prim_map,
            )
        }.unwrap_or(WorldVector2D::zero());

        // Assume no tiles are valid to draw by default
        self.tiles_to_draw.clear();

        self.map_local_to_world = SpaceMapper::new(
            ROOT_SPATIAL_NODE_INDEX,
            frame_context.screen_world_rect,
        );

        let world_mapper = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            self.spatial_node_index,
            frame_context.screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        // Do a hacky diff of opacity binding values from the last frame. This is
        // used later on during tile invalidation tests.
        let current_properties = frame_context.scene_properties.float_properties();
        let old_properties = mem::replace(&mut self.opacity_bindings, FastHashMap::default());

        for (id, value) in current_properties {
            let changed = match old_properties.get(id) {
                Some(old_property) => !old_property.value.approx_eq(value),
                None => true,
            };
            self.opacity_bindings.insert(*id, OpacityBindingInfo {
                value: *value,
                changed,
            });
        }

        // Map the picture rect to world space and work out the tiles that we need
        // in order to ensure the screen is covered.
        let pic_world_rect = world_mapper
            .map(&pic_rect)
            .expect("bug: unable to map picture rect to world");

        // If the bounding rect of the picture to cache doesn't intersect with
        // the visible world rect at all, just take the screen world rect as
        // a reference for the area to create tiles for. This allows existing
        // tiles to be retained in case they are still valid if / when they
        // get scrolled back onto the screen.

        let needed_world_rect = frame_context
            .screen_world_rect
            .intersection(&pic_world_rect)
            .unwrap_or(frame_context.screen_world_rect);

        // Get a reference point that serves as an origin that all tiles we create
        // must be aligned to. This ensures that tiles get reused correctly between
        // scrolls and display list changes, even with the different local coord
        // systems that gecko supplies.
        let mut world_ref_point = if self.tiles.is_empty() {
            needed_world_rect.origin.floor()
        } else {
            self.tiles[0].world_rect.origin + world_offset
        };

        // Apply the scroll delta so that existing tiles still get used.
        world_ref_point += scroll_delta;

        // Work out the required device rect that we need to cover the screen,
        // given the world reference point constraint.
        let device_ref_point = world_ref_point * frame_context.device_pixel_scale;
        let device_world_rect = frame_context.screen_world_rect * frame_context.device_pixel_scale;
        let pic_device_rect = pic_world_rect * frame_context.device_pixel_scale;
        let needed_device_rect = pic_device_rect
            .intersection(&device_world_rect)
            .unwrap_or(device_world_rect);

        // Expand the needed device rect vertically by a small number of tiles. This
        // ensures that as tiles are scrolled in/out of view, they are retained for
        // a while before being discarded.
        // TODO(gw): On some pages it might be worth also inflating horizontally.
        //           (is this locale specific?). It might be possible to make a good
        //           guess based on the size of the picture rect for the tile cache.
        let needed_device_rect = needed_device_rect.inflate(
            0.0,
            3.0 * TILE_SIZE_HEIGHT as f32,
        );

        let p0 = needed_device_rect.origin;
        let p1 = needed_device_rect.bottom_right();

        let p0 = DevicePoint::new(
            device_ref_point.x + ((p0.x - device_ref_point.x) / TILE_SIZE_WIDTH as f32).floor() * TILE_SIZE_WIDTH as f32,
            device_ref_point.y + ((p0.y - device_ref_point.y) / TILE_SIZE_HEIGHT as f32).floor() * TILE_SIZE_HEIGHT as f32,
        );

        let p1 = DevicePoint::new(
            device_ref_point.x + ((p1.x - device_ref_point.x) / TILE_SIZE_WIDTH as f32).ceil() * TILE_SIZE_WIDTH as f32,
            device_ref_point.y + ((p1.y - device_ref_point.y) / TILE_SIZE_HEIGHT as f32).ceil() * TILE_SIZE_HEIGHT as f32,
        );

        // And now the number of tiles from that device rect.
        let x_tiles = ((p1.x - p0.x) / TILE_SIZE_WIDTH as f32).round() as i32;
        let y_tiles = ((p1.y - p0.y) / TILE_SIZE_HEIGHT as f32).round() as i32;

        // Step through any old tiles, and retain them if we can. They are keyed only on
        // the (scroll adjusted) world position, relying on the descriptor content checks
        // later to invalidate them if the content has changed.
        let mut old_tiles = FastHashMap::default();
        for tile in self.tiles.drain(..) {
            let tile_device_pos = (tile.world_rect.origin + scroll_delta) * frame_context.device_pixel_scale;
            let key = (
                (tile_device_pos.x + world_offset.x).round() as i32,
                (tile_device_pos.y + world_offset.y).round() as i32,
            );
            old_tiles.insert(key, tile);
        }

        // Store parameters about the current tiling rect for use during dependency updates.
        self.world_origin = WorldPoint::new(
            p0.x / frame_context.device_pixel_scale.0,
            p0.y / frame_context.device_pixel_scale.0,
        );
        self.world_tile_size = WorldSize::new(
            TILE_SIZE_WIDTH as f32 / frame_context.device_pixel_scale.0,
            TILE_SIZE_HEIGHT as f32 / frame_context.device_pixel_scale.0,
        );
        self.tile_count = TileSize::new(x_tiles, y_tiles);

        // Step through each tile and try to retain an old tile from the
        // previous frame, and update bounding rects.
        for y in 0 .. y_tiles {
            for x in 0 .. x_tiles {
                let px = p0.x + x as f32 * TILE_SIZE_WIDTH as f32;
                let py = p0.y + y as f32 * TILE_SIZE_HEIGHT as f32;
                let key = (px.round() as i32, py.round() as i32);

                let mut tile = match old_tiles.remove(&key) {
                    Some(tile) => tile,
                    None => {
                        let next_id = TileId(NEXT_TILE_ID.fetch_add(1, Ordering::Relaxed));
                        Tile::new(next_id)
                    }
                };

                tile.world_rect = WorldRect::new(
                    WorldPoint::new(
                        px / frame_context.device_pixel_scale.0,
                        py / frame_context.device_pixel_scale.0,
                    ),
                    self.world_tile_size,
                );

                tile.local_rect = world_mapper
                    .unmap(&tile.world_rect)
                    .expect("bug: can't unmap world rect");

                tile.visible_rect = tile.world_rect.intersection(&frame_context.screen_world_rect);

                self.tiles.push(tile);
            }
        }

        if !old_tiles.is_empty() {
            // TODO(gw): Should we explicitly drop the tile texture cache handles here?
        }

        self.world_bounding_rect = WorldRect::zero();
        self.root_clip_rect = WorldRect::max_rect();

        // Calculate the world space of the root clip node, that every primitive has
        // at the root of its clip chain (this is enforced by the per-pipeline-root
        // clip node added implicitly during display list flattening). Doing it once
        // here saves doing it for every primitive during update_prim_dependencies.
        let root_clip_chain_node = &frame_state
            .clip_store
            .clip_chain_nodes[self.root_clip_chain_id.0 as usize];
        let root_clip_node = &frame_state
            .data_stores
            .clip[root_clip_chain_node.handle];
        if let Some(clip_rect) = root_clip_node.item.get_local_clip_rect(root_clip_chain_node.local_pos) {
            self.map_local_to_world.set_target_spatial_node(
                root_clip_chain_node.spatial_node_index,
                frame_context.clip_scroll_tree,
            );

            if let Some(world_clip_rect) = self.map_local_to_world.map(&clip_rect) {
                self.root_clip_rect = world_clip_rect;
            }
        }

        // Do tile invalidation for any dependencies that we know now.
        for tile in &mut self.tiles {
            // Start frame assuming that the tile has the same content.
            tile.is_same_content = true;

            // Content has changed if any images have changed
            for image_key in tile.descriptor.image_keys.items() {
                if frame_state.resource_cache.is_image_dirty(*image_key) {
                    tile.is_same_content = false;
                    break;
                }
            }

            // Content has changed if any opacity bindings changed.
            for binding in tile.descriptor.opacity_bindings.items() {
                if let OpacityBinding::Binding(id) = binding {
                    let changed = match self.opacity_bindings.get(id) {
                        Some(info) => info.changed,
                        None => true,
                    };
                    if changed {
                        tile.is_same_content = false;
                        break;
                    }
                }
            }

            // Clear any dependencies so that when we rebuild them we
            // can compare if the tile has the same content.
            tile.clear();
        }
    }

    /// Update the dependencies for each tile for a given primitive instance.
    pub fn update_prim_dependencies(
        &mut self,
        prim_instance: &PrimitiveInstance,
        prim_rect: LayoutRect,
        clip_scroll_tree: &ClipScrollTree,
        data_stores: &DataStores,
        clip_chain_nodes: &[ClipChainNode],
        pictures: &[PicturePrimitive],
        resource_cache: &ResourceCache,
        opacity_binding_store: &OpacityBindingStorage,
        image_instances: &ImageInstanceStorage,
    ) -> bool {
        self.map_local_to_world.set_target_spatial_node(
            prim_instance.spatial_node_index,
            clip_scroll_tree,
        );

        // Map the primitive local rect into world space.
        let world_rect = match self.map_local_to_world.map(&prim_rect) {
            Some(rect) => rect,
            None => return false,
        };

        // If the rect is invalid, no need to create dependencies.
        if world_rect.size.width <= 0.0 || world_rect.size.height <= 0.0 {
            return false;
        }

        // Get the tile coordinates in the picture space.
        let (p0, p1) = self.get_tile_coords_for_rect(&world_rect);

        // If the primitive is outside the tiling rects, it's known to not
        // be visible.
        if p0.x == p1.x || p0.y == p1.y {
            return false;
        }

        // Build the list of resources that this primitive has dependencies on.
        let mut opacity_bindings: SmallVec<[OpacityBinding; 4]> = SmallVec::new();
        let mut clip_chain_uids: SmallVec<[ItemUid; 8]> = SmallVec::new();
        let mut clip_vertices: SmallVec<[WorldPoint; 8]> = SmallVec::new();
        let mut image_keys: SmallVec<[ImageKey; 8]> = SmallVec::new();
        let mut clip_spatial_nodes = FastHashSet::default();

        // TODO(gw): We only care about world clip rects that don't have the main
        //           scroll root as an ancestor. It may be a worthwhile optimization
        //           to check for these and skip them.
        let mut world_clips: SmallVec<[(RectangleKey, SpatialNodeIndex); 4]> = SmallVec::default();

        // Some primitives can not be cached (e.g. external video images)
        let is_cacheable = prim_instance.is_cacheable(
            &data_stores,
            resource_cache,
        );

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
        let include_clip_rect = match prim_instance.kind {
            PrimitiveInstanceKind::Picture { pic_index,.. } => {
                // Pictures can depend on animated opacity bindings.
                let pic = &pictures[pic_index.0];
                if let Some(PictureCompositeMode::Filter(FilterOp::Opacity(binding, _))) = pic.requested_composite_mode {
                    opacity_bindings.push(binding.into());
                }

                false
            }
            PrimitiveInstanceKind::Rectangle { opacity_binding_index, .. } => {
                if opacity_binding_index != OpacityBindingIndex::INVALID {
                    let opacity_binding = &opacity_binding_store[opacity_binding_index];
                    for binding in &opacity_binding.bindings {
                        opacity_bindings.push(OpacityBinding::from(*binding));
                    }
                }

                true
            }
            PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
                let image_data = &data_stores.image[data_handle].kind;
                let image_instance = &image_instances[image_instance_index];
                let opacity_binding_index = image_instance.opacity_binding_index;

                if opacity_binding_index != OpacityBindingIndex::INVALID {
                    let opacity_binding = &opacity_binding_store[opacity_binding_index];
                    for binding in &opacity_binding.bindings {
                        opacity_bindings.push(OpacityBinding::from(*binding));
                    }
                }

                image_keys.push(image_data.key);
                true
            }
            PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                let yuv_image_data = &data_stores.yuv_image[data_handle].kind;
                image_keys.extend_from_slice(&yuv_image_data.yuv_key);
                true
            }
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } |
            PrimitiveInstanceKind::Clear { .. } |
            PrimitiveInstanceKind::NormalBorder { .. } |
            PrimitiveInstanceKind::LinearGradient { .. } |
            PrimitiveInstanceKind::RadialGradient { .. } |
            PrimitiveInstanceKind::ImageBorder { .. } => {
                // These don't contribute dependencies
                true
            }
        };

        // The transforms of any clips that are relative to the picture may affect
        // the content rendered by this primitive.
        let mut world_clip_rect = world_rect;
        let mut culling_rect = prim_rect
            .intersection(&prim_instance.local_clip_rect)
            .unwrap_or(LayoutRect::zero());

        let mut current_clip_chain_id = prim_instance.clip_chain_id;
        while current_clip_chain_id != ClipChainId::NONE {
            let clip_chain_node = &clip_chain_nodes[current_clip_chain_id.0 as usize];
            let clip_node = &data_stores.clip[clip_chain_node.handle];

            // We can skip the root clip node - it will be taken care of by the
            // world bounding rect calculated for the cache.
            if current_clip_chain_id == self.root_clip_chain_id {
                current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
                continue;
            }

            self.map_local_to_world.set_target_spatial_node(
                clip_chain_node.spatial_node_index,
                clip_scroll_tree,
            );

            // Clips that are simple rects and handled by collapsing them into a single
            // clip rect. This avoids the need to store vertices for these cases, and also
            // allows easy calculation of the overall bounds of the tile cache.
            let add_to_clip_deps = match clip_node.item {
                ClipItem::Rectangle(size, ClipMode::Clip) => {
                    let clip_spatial_node = &clip_scroll_tree.spatial_nodes[clip_chain_node.spatial_node_index.0 as usize];

                    let local_clip_rect = LayoutRect::new(
                        clip_chain_node.local_pos,
                        size,
                    );

                    if clip_spatial_node.coordinate_system_id == CoordinateSystemId(0) {
                        // Clips that are not in the root coordinate system are not axis-aligned,
                        // so we need to treat them as normal style clips with vertices.
                        match self.map_local_to_world.map(&local_clip_rect) {
                            Some(clip_world_rect) => {
                                // Even if this ends up getting clipped out by the current clip
                                // stack, we want to ensure the primitive gets added to the tiles
                                // below, to ensure invalidation isn't tripped up by the wrong
                                // number of primitives that affect this tile.
                                world_clip_rect = world_clip_rect
                                    .intersection(&clip_world_rect)
                                    .unwrap_or(WorldRect::zero());

                                // If the clip rect is in the same spatial node, it can be handled by the
                                // local clip rect.
                                if clip_chain_node.spatial_node_index == prim_instance.spatial_node_index {
                                    culling_rect = culling_rect.intersection(&local_clip_rect).unwrap_or(LayoutRect::zero());
                                } else {
                                    world_clips.push((
                                        clip_world_rect.into(),
                                        clip_chain_node.spatial_node_index,
                                    ));
                                }

                                false
                            }
                            None => {
                                true
                            }
                        }
                    } else {
                        true
                    }
                }
                ClipItem::Rectangle(_, ClipMode::ClipOut) |
                ClipItem::RoundedRectangle(..) |
                ClipItem::Image { .. } |
                ClipItem::BoxShadow(..) => {
                    true
                }
            };

            if add_to_clip_deps {
                clip_chain_uids.push(clip_chain_node.handle.uid());

                // If the clip has the same spatial node, the relative transform
                // will always be the same, so there's no need to depend on it.
                if clip_chain_node.spatial_node_index != self.spatial_node_index {
                    clip_spatial_nodes.insert(clip_chain_node.spatial_node_index);
                }

                let local_clip_rect = LayoutRect::new(
                    clip_chain_node.local_pos,
                    LayoutSize::zero(),
                );
                if let Some(world_clip_rect) = self.map_local_to_world.map(&local_clip_rect) {
                    clip_vertices.push(world_clip_rect.origin);
                }
            }

            current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
        }

        if include_clip_rect {
            // Intersect the calculated prim bounds with the root clip rect, to save
            // having to process and transform the root clip rect in every primitive.
            if let Some(clipped_world_rect) = world_clip_rect.intersection(&self.root_clip_rect) {
                self.world_bounding_rect = self.world_bounding_rect.union(&clipped_world_rect);
            }
        }

        self.map_local_to_world.set_target_spatial_node(
            prim_instance.spatial_node_index,
            clip_scroll_tree,
        );
        let world_culling_rect = self
            .map_local_to_world
            .map(&culling_rect)
            .expect("bug: unable to map local clip rect");

        // Normalize the tile coordinates before adding to tile dependencies.
        // For each affected tile, mark any of the primitive dependencies.
        for y in p0.y .. p1.y {
            for x in p0.x .. p1.x {
                let index = (y * self.tile_count.width + x) as usize;
                let tile = &mut self.tiles[index];

                // Store the local clip rect by calculating what portion
                // of the tile it covers.
                let world_culling_rect = world_culling_rect
                    .intersection(&tile.world_rect)
                    .map(|rect| {
                        rect.translate(&-tile.world_rect.origin.to_vector())
                    })
                    .unwrap_or(WorldRect::zero())
                    .round();

                // Work out the needed rect for the primitive on this tile.
                // TODO(gw): We should be able to remove this for any tile that is not
                //           a partially clipped tile, which would be a significant
                //           optimization for the common case (non-clipped tiles).

                // Mark if the tile is cacheable at all.
                tile.is_same_content &= is_cacheable;

                // Include any image keys this tile depends on.
                tile.descriptor.image_keys.extend_from_slice(&image_keys);

                // // Include any opacity bindings this primitive depends on.
                tile.descriptor.opacity_bindings.extend_from_slice(&opacity_bindings);

                // Update the tile descriptor, used for tile comparison during scene swaps.
                tile.descriptor.prims.push(PrimitiveDescriptor {
                    prim_uid: prim_instance.uid(),
                    origin: (world_rect.origin - tile.world_rect.origin.to_vector()).round(),
                    first_clip: tile.descriptor.clip_uids.len() as u16,
                    clip_count: clip_chain_uids.len() as u16,
                    world_culling_rect,
                });
                tile.descriptor.clip_uids.extend_from_slice(&clip_chain_uids);
                for clip_vertex in &clip_vertices {
                    let clip_vertex = (*clip_vertex - tile.world_rect.origin.to_vector()).round();
                    tile.descriptor.clip_vertices.push(clip_vertex.into());
                }

                // If the primitive has the same spatial node, the relative transform
                // will always be the same, so there's no need to depend on it.
                if prim_instance.spatial_node_index != self.spatial_node_index {
                    tile.transforms.insert(prim_instance.spatial_node_index);
                }

                for spatial_node_index in &clip_spatial_nodes {
                    tile.transforms.insert(*spatial_node_index);
                }
                for (world_rect, spatial_node_index) in &world_clips {
                    tile.potential_clips.insert(world_rect.clone(), *spatial_node_index);
                }
            }
        }

        true
    }

    /// Apply any updates after prim dependency updates. This applies
    /// any late tile invalidations, and sets up the dirty rect and
    /// set of tile blits.
    pub fn post_update(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        frame_context: &FrameVisibilityContext,
        _scratch: &mut PrimitiveScratchBuffer,
    ) -> LayoutRect {
        self.dirty_region.clear();
        self.pending_blits.clear();

        let descriptor = ImageDescriptor::new(
            TILE_SIZE_WIDTH,
            TILE_SIZE_HEIGHT,
            ImageFormat::BGRA8,
            true,
            false,
        );

        // Skip all tiles if completely off-screen.
        if !self.world_bounding_rect.intersects(&frame_context.screen_world_rect) {
            return LayoutRect::zero();
        }

        let map_surface_to_world: SpaceMapper<LayoutPixel, WorldPixel> = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            self.spatial_node_index,
            frame_context.screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        let local_clip_rect = map_surface_to_world
            .unmap(&self.world_bounding_rect)
            .expect("bug: unable to map local clip rect");

        // Step through each tile and invalidate if the dependencies have changed.
        for (i, tile) in self.tiles.iter_mut().enumerate() {
            // Deal with any potential world clips. Check to see if they are
            // outside the tile cache bounding rect. If they are, they're not
            // relevant and we don't care if they move relative to the content
            // itself. This avoids a lot of redundant invalidations.
            for (clip_world_rect, spatial_node_index) in &tile.potential_clips {
                let clip_world_rect = WorldRect::from(clip_world_rect.clone());
                if !clip_world_rect.contains_rect(&self.world_bounding_rect) {
                    tile.transforms.insert(*spatial_node_index);
                }
            }

            // Update tile transforms
            let mut transform_spatial_nodes: Vec<SpatialNodeIndex> = tile.transforms.drain().collect();
            transform_spatial_nodes.sort();
            for spatial_node_index in transform_spatial_nodes {
                let xf = frame_context.clip_scroll_tree.get_relative_transform(
                    self.spatial_node_index,
                    spatial_node_index,
                ).expect("BUG: unable to get relative transform");
                // Store the result of transforming a fixed point by this
                // transform.
                // TODO(gw): This could in theory give incorrect results for a
                //           primitive behind the near plane.
                let key = xf.transform_point2d(&LayoutPoint::zero()).unwrap_or(LayoutPoint::zero()).round();
                tile.descriptor.transforms.push(key.into());
            }

            // Invalidate if the backing texture was evicted.
            if resource_cache.texture_cache.is_allocated(&tile.handle) {
                // Request the backing texture so it won't get evicted this frame.
                // We specifically want to mark the tile texture as used, even
                // if it's detected not visible below and skipped. This is because
                // we maintain the set of tiles we care about based on visibility
                // during pre_update. If a tile still exists after that, we are
                // assuming that it's either visible or we want to retain it for
                // a while in case it gets scrolled back onto screen soon.
                // TODO(gw): Consider switching to manual eviction policy?
                resource_cache.texture_cache.request(&tile.handle, gpu_cache);
            } else {
                tile.is_valid = false;
            }

            // Invalidate the tile based on the content changing.
            tile.update_content_validity();

            let visible_rect = match tile.visible_rect {
                Some(rect) => rect,
                None => continue,
            };

            // Check the valid rect of the primitive is sufficient.
            let tile_bounding_rect = match visible_rect.intersection(&self.world_bounding_rect) {
                Some(rect) => rect.translate(&-tile.world_rect.origin.to_vector()),
                None => continue,
            };

            tile.update_rect_validity(&tile_bounding_rect);

            // If there are no primitives there is no need to draw or cache it.
            if tile.descriptor.prims.is_empty() {
                continue;
            }

            // Decide how to handle this tile when drawing this frame.
            if tile.is_valid {
                // No need to include this is any dirty rect calculations.
                tile.consider_for_dirty_rect = false;
                self.tiles_to_draw.push(TileIndex(i));

                #[cfg(feature = "debug_renderer")]
                {
                    if frame_context.debug_flags.contains(DebugFlags::PICTURE_CACHING_DBG) {
                        let tile_device_rect = tile.world_rect * frame_context.device_pixel_scale;
                        let mut label_pos = tile_device_rect.origin + DeviceVector2D::new(20.0, 30.0);
                        _scratch.push_debug_rect(
                            tile_device_rect,
                            debug_colors::GREEN,
                        );
                        _scratch.push_debug_string(
                            label_pos,
                            debug_colors::RED,
                            format!("{:?} {:?} {:?}", tile.id, tile.handle, tile.world_rect),
                        );
                        label_pos.y += 20.0;
                        _scratch.push_debug_string(
                            label_pos,
                            debug_colors::RED,
                            format!("same: {} frames", tile.same_frames),
                        );
                    }
                }
            } else {
                #[cfg(feature = "debug_renderer")]
                {
                    if frame_context.debug_flags.contains(DebugFlags::PICTURE_CACHING_DBG) {
                        _scratch.push_debug_rect(
                            visible_rect * frame_context.device_pixel_scale,
                            debug_colors::RED,
                        );
                    }
                }

                // Only cache tiles that have had the same content for at least two
                // frames. This skips caching on pages / benchmarks that are changing
                // every frame, which is wasteful.
                if tile.same_frames > FRAMES_BEFORE_CACHING {
                    // Ensure that this texture is allocated.
                    resource_cache.texture_cache.update(
                        &mut tile.handle,
                        descriptor,
                        TextureFilter::Linear,
                        None,
                        [0.0; 3],
                        DirtyRect::All,
                        gpu_cache,
                        None,
                        UvRectKind::Rect,
                        Eviction::Eager,
                    );

                    let cache_item = resource_cache
                        .get_texture_cache_item(&tile.handle);

                    let src_origin = (visible_rect.origin * frame_context.device_pixel_scale).round().to_i32();
                    let valid_rect = visible_rect.translate(&-tile.world_rect.origin.to_vector());

                    tile.valid_rect = visible_rect
                        .intersection(&self.world_bounding_rect)
                        .map(|rect| rect.translate(&-tile.world_rect.origin.to_vector()))
                        .unwrap_or(WorldRect::zero());

                    // Store a blit operation to be done after drawing the
                    // frame in order to update the cached texture tile.
                    let dest_rect = (valid_rect * frame_context.device_pixel_scale).round().to_i32();
                    self.pending_blits.push(TileBlit {
                        target: cache_item,
                        src_offset: src_origin,
                        dest_offset: dest_rect.origin,
                        size: dest_rect.size,
                    });

                    // We can consider this tile valid now.
                    tile.is_valid = true;
                }

                // This tile should be considered as part of the dirty rect calculations.
                tile.consider_for_dirty_rect = true;
            }
        }

        // Build a minimal set of dirty rects from the set of dirty tiles that
        // were found above.
        let mut builder = DirtyRegionBuilder::new(
            &mut self.tiles,
            self.tile_count,
            frame_context.device_pixel_scale,
        );

        builder.build(&mut self.dirty_region);

        // If we end up with too many dirty rects, then it's going to be a lot
        // of extra draw calls to submit (since we currently just submit every
        // draw call for every dirty rect). In this case, bail out and work
        // with a single, large dirty rect. In future we can consider improving
        // on this by supporting batching per dirty region.
        if self.dirty_region.dirty_rects.len() > MAX_DIRTY_RECTS {
            self.dirty_region.collapse();
        }

        local_clip_rect
    }
}

/// Maintains a stack of picture and surface information, that
/// is used during the initial picture traversal.
pub struct PictureUpdateState<'a> {
    pub surfaces: &'a mut Vec<SurfaceInfo>,
    surface_stack: Vec<SurfaceIndex>,
    picture_stack: Vec<PictureInfo>,
}

impl<'a> PictureUpdateState<'a> {
    pub fn new(surfaces: &'a mut Vec<SurfaceInfo>) -> Self {
        PictureUpdateState {
            surfaces,
            surface_stack: vec![SurfaceIndex(0)],
            picture_stack: Vec::new(),
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
    fn pop_surface(&mut self) {
        self.surface_stack.pop().unwrap();
    }

    /// Return the current picture, or None if stack is empty.
    fn current_picture(&self) -> Option<&PictureInfo> {
        self.picture_stack.last()
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
}

#[derive(Debug, Copy, Clone)]
pub struct SurfaceIndex(pub usize);

pub const ROOT_SURFACE_INDEX: SurfaceIndex = SurfaceIndex(0);

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
    pub surface: Option<PictureSurface>,
    /// A list of render tasks that are dependencies of this surface.
    pub tasks: Vec<RenderTaskId>,
    /// How much the local surface rect should be inflated (for blur radii).
    pub inflation_factor: f32,
}

impl SurfaceInfo {
    pub fn new(
        surface_spatial_node_index: SpatialNodeIndex,
        raster_spatial_node_index: SpatialNodeIndex,
        inflation_factor: f32,
        world_rect: WorldRect,
        clip_scroll_tree: &ClipScrollTree,
    ) -> Self {
        let map_surface_to_world = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface_spatial_node_index,
            world_rect,
            clip_scroll_tree,
        );

        let pic_bounds = map_surface_to_world
            .unmap(&map_surface_to_world.bounds)
            .unwrap_or(PictureRect::max_rect());

        let map_local_to_surface = SpaceMapper::new(
            surface_spatial_node_index,
            pic_bounds,
        );

        SurfaceInfo {
            rect: PictureRect::zero(),
            map_local_to_surface,
            surface: None,
            raster_spatial_node_index,
            surface_spatial_node_index,
            tasks: Vec::new(),
            inflation_factor,
        }
    }

    pub fn fits_surface_size_limits(&self) -> bool {
        self.map_local_to_surface.bounds.size.width <= MAX_SURFACE_SIZE &&
        self.map_local_to_surface.bounds.size.height <= MAX_SURFACE_SIZE
    }

    /// Take the set of child render tasks for this surface. This is
    /// used when constructing the render task tree.
    pub fn take_render_tasks(&mut self) -> Vec<RenderTaskId> {
        mem::replace(&mut self.tasks, Vec::new())
    }
}

#[derive(Debug)]
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

/// Specifies how this Picture should be composited
/// onto the target it belongs to.
#[allow(dead_code)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum PictureCompositeMode {
    /// Apply CSS mix-blend-mode effect.
    MixBlend(MixBlendMode),
    /// Apply a CSS filter.
    Filter(FilterOp),
    /// Draw to intermediate surface, copy straight across. This
    /// is used for CSS isolation, and plane splitting.
    Blit,
    /// Used to cache a picture as a series of tiles.
    TileCache {
        clear_color: ColorF,
    },
}

// Stores the location of the picture if it is drawn to
// an intermediate surface. This can be a render task if
// it is not persisted, or a texture cache item if the
// picture is cached in the texture cache.
#[derive(Debug)]
pub enum PictureSurface {
    RenderTask(RenderTaskId),
    TextureCache(RenderTaskCacheEntryHandle),
}

/// Enum value describing the place of a picture in a 3D context.
#[derive(Clone, Debug)]
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
pub struct OrderedPictureChild {
    pub anchor: usize,
    pub transform_id: TransformPaletteId,
    pub gpu_address: GpuCacheAddress,
}

/// Defines the grouping key for a cluster of primitives in a picture.
/// In future this will also contain spatial grouping details.
#[derive(Hash, Eq, PartialEq, Copy, Clone)]
struct PrimitiveClusterKey {
    /// Grouping primitives by spatial node ensures that we can calculate a local
    /// bounding volume for the cluster, and then transform that by the spatial
    /// node transform once to get an updated bounding volume for the entire cluster.
    spatial_node_index: SpatialNodeIndex,
    /// We want to separate clusters that have different backface visibility properties
    /// so that we can accept / reject an entire cluster at once if the backface is not
    /// visible.
    is_backface_visible: bool,
}

/// Descriptor for a cluster of primitives. For now, this is quite basic but will be
/// extended to handle more spatial clustering of primitives.
pub struct PrimitiveCluster {
    /// The positioning node for this cluster.
    spatial_node_index: SpatialNodeIndex,
    /// Whether this cluster is visible when the position node is a backface.
    is_backface_visible: bool,
    /// The bounding rect of the cluster, in the local space of the spatial node.
    /// This is used to quickly determine the overall bounding rect for a picture
    /// during the first picture traversal, which is needed for local scale
    /// determination, and render task size calculations.
    bounding_rect: LayoutRect,
    /// This flag is set during the first pass picture traversal, depending on whether
    /// the cluster is visible or not. It's read during the second pass when primitives
    /// consult their owning clusters to see if the primitive itself is visible.
    pub is_visible: bool,
}

impl PrimitiveCluster {
    fn new(
        spatial_node_index: SpatialNodeIndex,
        is_backface_visible: bool,
    ) -> Self {
        PrimitiveCluster {
            bounding_rect: LayoutRect::zero(),
            spatial_node_index,
            is_backface_visible,
            is_visible: false,
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub struct PrimitiveClusterIndex(pub u32);

#[derive(Debug, Copy, Clone)]
pub struct ClusterIndex(pub u16);

impl ClusterIndex {
    pub const INVALID: ClusterIndex = ClusterIndex(u16::MAX);
}

/// A list of pictures, stored by the PrimitiveList to enable a
/// fast traversal of just the pictures.
pub type PictureList = SmallVec<[PictureIndex; 4]>;

/// A list of primitive instances that are added to a picture
/// This ensures we can keep a list of primitives that
/// are pictures, for a fast initial traversal of the picture
/// tree without walking the instance list.
pub struct PrimitiveList {
    /// The primitive instances, in render order.
    pub prim_instances: Vec<PrimitiveInstance>,
    /// List of pictures that are part of this list.
    /// Used to implement the picture traversal pass.
    pub pictures: PictureList,
    /// List of primitives grouped into clusters.
    pub clusters: SmallVec<[PrimitiveCluster; 4]>,
}

impl PrimitiveList {
    /// Construct an empty primitive list. This is
    /// just used during the take_context / restore_context
    /// borrow check dance, which will be removed as the
    /// picture traversal pass is completed.
    pub fn empty() -> Self {
        PrimitiveList {
            prim_instances: Vec::new(),
            pictures: SmallVec::new(),
            clusters: SmallVec::new(),
        }
    }

    /// Construct a new prim list from a list of instances
    /// in render order. This does some work during scene
    /// building which makes the frame building traversals
    /// significantly faster.
    pub fn new(
        mut prim_instances: Vec<PrimitiveInstance>,
        interners: &Interners
    ) -> Self {
        let mut pictures = SmallVec::new();
        let mut clusters_map = FastHashMap::default();
        let mut clusters: SmallVec<[PrimitiveCluster; 4]> = SmallVec::new();

        // Walk the list of primitive instances and extract any that
        // are pictures.
        for prim_instance in &mut prim_instances {
            // Check if this primitive is a picture. In future we should
            // remove this match and embed this info directly in the primitive instance.
            let is_pic = match prim_instance.kind {
                PrimitiveInstanceKind::Picture { pic_index, .. } => {
                    pictures.push(pic_index);
                    true
                }
                _ => {
                    false
                }
            };

            let prim_data = match prim_instance.kind {
                PrimitiveInstanceKind::Rectangle { data_handle, .. } |
                PrimitiveInstanceKind::Clear { data_handle, .. } => {
                    &interners.prim[data_handle]
                }
                PrimitiveInstanceKind::Image { data_handle, .. } => {
                    &interners.image[data_handle]
                }
                PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                    &interners.image_border[data_handle]
                }
                PrimitiveInstanceKind::LineDecoration { data_handle, .. } => {
                    &interners.line_decoration[data_handle]
                }
                PrimitiveInstanceKind::LinearGradient { data_handle, .. } => {
                    &interners.linear_grad[data_handle]
                }
                PrimitiveInstanceKind::NormalBorder { data_handle, .. } => {
                    &interners.normal_border[data_handle]
                }
                PrimitiveInstanceKind::Picture { data_handle, .. } => {
                    &interners.picture[data_handle]
                }
                PrimitiveInstanceKind::RadialGradient { data_handle, ..} => {
                    &interners.radial_grad[data_handle]
                }
                PrimitiveInstanceKind::TextRun { data_handle, .. } => {
                    &interners.text_run[data_handle]
                }
                PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                    &interners.yuv_image[data_handle]
                }
            };

            // Get the key for the cluster that this primitive should
            // belong to.
            let key = PrimitiveClusterKey {
                spatial_node_index: prim_instance.spatial_node_index,
                is_backface_visible: prim_data.is_backface_visible,
            };

            // Find the cluster, or create a new one.
            let cluster_index = *clusters_map
                .entry(key)
                .or_insert_with(|| {
                    let index = clusters.len();
                    clusters.push(PrimitiveCluster::new(
                        prim_instance.spatial_node_index,
                        prim_data.is_backface_visible,
                    ));
                    index
                }
            );

            // Pictures don't have a known static local bounding rect (they are
            // calculated during the picture traversal dynamically). If not
            // a picture, include a minimal bounding rect in the cluster bounds.
            let cluster = &mut clusters[cluster_index];
            if !is_pic {
                let prim_rect = LayoutRect::new(
                    prim_instance.prim_origin,
                    prim_data.prim_size,
                );
                let culling_rect = prim_instance.local_clip_rect
                    .intersection(&prim_rect)
                    .unwrap_or(LayoutRect::zero());

                cluster.bounding_rect = cluster.bounding_rect.union(&culling_rect);
            }

            prim_instance.cluster_index = ClusterIndex(cluster_index as u16);
        }

        PrimitiveList {
            prim_instances,
            pictures,
            clusters,
        }
    }
}

pub struct PicturePrimitive {
    /// List of primitives, and associated info for this picture.
    pub prim_list: PrimitiveList,

    pub state: Option<(PictureState, PictureContext)>,

    // The pipeline that the primitives on this picture belong to.
    pub pipeline_id: PipelineId,

    // If true, apply the local clip rect to primitive drawn
    // in this picture.
    pub apply_local_clip_rect: bool,

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
    // An optional cache handle for storing extra data
    // in the GPU cache, depending on the type of
    // picture.
    pub extra_gpu_data_handle: GpuCacheHandle,

    /// The spatial node index of this picture when it is
    /// composited into the parent picture.
    pub spatial_node_index: SpatialNodeIndex,

    /// The local rect of this picture. It is built
    /// dynamically during the first picture traversal.
    pub local_rect: LayoutRect,

    /// Local clip rect for this picture.
    pub local_clip_rect: LayoutRect,

    /// A descriptor for this surface that can be used as a cache key.
    surface_desc: Option<SurfaceDescriptor>,

    pub gpu_location: GpuCacheHandle,

    /// If Some(..) the tile cache that is associated with this picture.
    pub tile_cache: Option<TileCache>,
}

impl PicturePrimitive {
    pub fn print<T: PrintTreePrinter>(
        &self,
        pictures: &[Self],
        self_index: PictureIndex,
        pt: &mut T,
    ) {
        pt.new_level(format!("{:?}", self_index));
        pt.add_item(format!("prim_count: {:?}", self.prim_list.prim_instances.len()));
        pt.add_item(format!("local_rect: {:?}", self.local_rect));
        if self.apply_local_clip_rect {
            pt.add_item(format!("local_clip_rect: {:?}", self.local_clip_rect));
        }
        pt.add_item(format!("spatial_node_index: {:?}", self.spatial_node_index));
        pt.add_item(format!("raster_config: {:?}", self.raster_config));
        pt.add_item(format!("requested_composite_mode: {:?}", self.requested_composite_mode));

        for index in &self.prim_list.pictures {
            pictures[index.0].print(pictures, *index, pt);
        }

        pt.end_level();
    }

    fn resolve_scene_properties(&mut self, properties: &SceneProperties) -> bool {
        match self.requested_composite_mode {
            Some(PictureCompositeMode::Filter(ref mut filter)) => {
                match *filter {
                    FilterOp::Opacity(ref binding, ref mut value) => {
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
        mut self,
        retained_tiles: &mut RetainedTiles,
        clip_scroll_tree: &ClipScrollTree,
    ) {
        if let Some(tile_cache) = self.tile_cache.take() {
            // Calculate and store positions of the reference
            // primitives for this tile cache.
            build_ref_prims(
                &tile_cache.reference_prims.ref_prims,
                &mut retained_tiles.ref_prims,
                clip_scroll_tree,
            );

            for tile in tile_cache.tiles {
                retained_tiles.tiles.push(tile);
            }
        }
    }

    pub fn new_image(
        requested_composite_mode: Option<PictureCompositeMode>,
        context_3d: Picture3DContext<OrderedPictureChild>,
        pipeline_id: PipelineId,
        frame_output_pipeline_id: Option<PipelineId>,
        apply_local_clip_rect: bool,
        requested_raster_space: RasterSpace,
        prim_list: PrimitiveList,
        spatial_node_index: SpatialNodeIndex,
        local_clip_rect: LayoutRect,
        clip_store: &ClipStore,
        tile_cache: Option<TileCache>,
    ) -> Self {
        // For now, only create a cache descriptor for blur filters (which
        // includes text shadows). We can incrementally expand this to
        // handle more composite modes.
        let create_cache_descriptor = match requested_composite_mode {
            Some(PictureCompositeMode::Filter(FilterOp::Blur(blur_radius))) => {
                blur_radius > 0.0
            }
            Some(_) | None => {
                false
            }
        };

        let surface_desc = if create_cache_descriptor {
            SurfaceDescriptor::new(
                &prim_list.prim_instances,
                spatial_node_index,
                clip_store,
            )
        } else {
            None
        };

        PicturePrimitive {
            surface_desc,
            prim_list,
            state: None,
            secondary_render_task_id: None,
            requested_composite_mode,
            raster_config: None,
            context_3d,
            frame_output_pipeline_id,
            extra_gpu_data_handle: GpuCacheHandle::new(),
            apply_local_clip_rect,
            pipeline_id,
            requested_raster_space,
            spatial_node_index,
            local_rect: LayoutRect::zero(),
            local_clip_rect,
            gpu_location: GpuCacheHandle::new(),
            tile_cache,
        }
    }

    pub fn take_context(
        &mut self,
        pic_index: PictureIndex,
        surface_spatial_node_index: SpatialNodeIndex,
        raster_spatial_node_index: SpatialNodeIndex,
        surface_index: SurfaceIndex,
        parent_allows_subpixel_aa: bool,
        frame_state: &mut FrameBuildingState,
        frame_context: &FrameBuildingContext,
    ) -> Option<(PictureContext, PictureState, PrimitiveList)> {
        if !self.is_visible() {
            return None;
        }

        // Extract the raster and surface spatial nodes from the raster
        // config, if this picture establishes a surface. Otherwise just
        // pass in the spatial node indices from the parent context.
        let (raster_spatial_node_index, surface_spatial_node_index, surface_index) = match self.raster_config {
            Some(ref raster_config) => {
                let surface = &frame_state.surfaces[raster_config.surface_index.0];

                (surface.raster_spatial_node_index, self.spatial_node_index, raster_config.surface_index)
            }
            None => {
                (raster_spatial_node_index, surface_spatial_node_index, surface_index)
            }
        };

        let map_pic_to_world = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface_spatial_node_index,
            frame_context.screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        let pic_bounds = map_pic_to_world.unmap(&map_pic_to_world.bounds)
                                         .unwrap_or(PictureRect::max_rect());

        let map_local_to_pic = SpaceMapper::new(
            surface_spatial_node_index,
            pic_bounds,
        );

        let (map_raster_to_world, map_pic_to_raster) = create_raster_mappers(
            surface_spatial_node_index,
            raster_spatial_node_index,
            frame_context.screen_world_rect,
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

        let state = PictureState {
            //TODO: check for MAX_CACHE_SIZE here?
            is_cacheable: true,
            map_local_to_pic,
            map_pic_to_world,
            map_pic_to_raster,
            map_raster_to_world,
            plane_splitter,
        };

        // Disallow subpixel AA if an intermediate surface is needed.
        // TODO(lsalzman): allow overriding parent if intermediate surface is opaque
        let allow_subpixel_aa = match self.raster_config {
            Some(RasterConfig { composite_mode: PictureCompositeMode::TileCache { clear_color, .. }, .. }) => {
                // If the tile cache has an opaque background, then it's fine to use
                // subpixel rendering (this is the common case).
                clear_color.a >= 1.0
            },
            Some(_) => {
                false
            }
            None => {
                true
            }
        };
        // Still disable subpixel AA if parent forbids it
        let allow_subpixel_aa = parent_allows_subpixel_aa && allow_subpixel_aa;

        let context = PictureContext {
            pic_index,
            pipeline_id: self.pipeline_id,
            apply_local_clip_rect: self.apply_local_clip_rect,
            allow_subpixel_aa,
            is_passthrough: self.raster_config.is_none(),
            raster_space: self.requested_raster_space,
            raster_spatial_node_index,
            surface_spatial_node_index,
            surface_index,
        };

        // If this is a picture cache, push the dirty region to ensure any
        // child primitives are culled and clipped to the dirty rect(s).
        if let Some(ref tile_cache) = self.tile_cache {
            frame_state.push_dirty_region(tile_cache.dirty_region.clone());
        }

        let prim_list = mem::replace(&mut self.prim_list, PrimitiveList::empty());

        Some((context, state, prim_list))
    }

    pub fn restore_context(
        &mut self,
        prim_list: PrimitiveList,
        context: PictureContext,
        state: PictureState,
        frame_state: &mut FrameBuildingState,
    ) {
        // Pop the dirty region for this picture cache
        if self.tile_cache.is_some() {
            frame_state.pop_dirty_region();
        }

        self.prim_list = prim_list;
        self.state = Some((state, context));
    }

    pub fn take_state_and_context(&mut self) -> (PictureState, PictureContext) {
        self.state.take().expect("bug: no state present!")
    }

    /// Add a primitive instance to the plane splitter. The function would generate
    /// an appropriate polygon, clip it against the frustum, and register with the
    /// given plane splitter.
    pub fn add_split_plane(
        splitter: &mut PlaneSplitter,
        transforms: &TransformPalette,
        prim_instance: &PrimitiveInstance,
        original_local_rect: LayoutRect,
        combined_local_clip_rect: &LayoutRect,
        world_rect: WorldRect,
        plane_split_anchor: usize,
    ) -> bool {
        let transform = transforms
            .get_world_transform(prim_instance.spatial_node_index);
        let matrix = transform.cast();

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

        match transform.transform_kind() {
            TransformedRectKind::AxisAligned => {
                let inv_transform = transforms
                    .get_world_inv_transform(prim_instance.spatial_node_index);
                let polygon = Polygon::from_transformed_rect_with_inverse(
                    local_rect,
                    &matrix,
                    &inv_transform.cast(),
                    plane_split_anchor,
                ).unwrap();
                splitter.add(polygon);
            }
            TransformedRectKind::Complex => {
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
        frame_state: &mut FrameBuildingState,
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
            let spatial_node_index = self.prim_list.prim_instances[poly.anchor].spatial_node_index;

            let transform = frame_state.transforms.get_world_inv_transform(spatial_node_index);
            let transform_id = frame_state.transforms.get_id(
                spatial_node_index,
                ROOT_SPATIAL_NODE_INDEX,
                clip_scroll_tree,
            );

            let local_points = [
                transform.transform_point3d(&poly.points[0].cast()).unwrap(),
                transform.transform_point3d(&poly.points[1].cast()).unwrap(),
                transform.transform_point3d(&poly.points[2].cast()).unwrap(),
                transform.transform_point3d(&poly.points[3].cast()).unwrap(),
            ];
            let gpu_blocks = [
                [local_points[0].x, local_points[0].y, local_points[1].x, local_points[1].y].into(),
                [local_points[2].x, local_points[2].y, local_points[3].x, local_points[3].y].into(),
            ];
            let gpu_handle = frame_state.gpu_cache.push_per_frame_blocks(&gpu_blocks);
            let gpu_address = frame_state.gpu_cache.get_address(&gpu_handle);

            ordered.push(OrderedPictureChild {
                anchor: poly.anchor,
                transform_id,
                gpu_address,
            });
        }
    }

    /// Called during initial picture traversal, before we know the
    /// bounding rect of children. It is possible to determine the
    /// surface / raster config now though.
    pub fn pre_update(
        &mut self,
        state: &mut PictureUpdateState,
        frame_context: &FrameBuildingContext,
    ) -> Option<PictureList> {
        // Reset raster config in case we early out below.
        self.raster_config = None;

        // Resolve animation properties, and early out if the filter
        // properties make this picture invisible.
        if !self.resolve_scene_properties(frame_context.scene_properties) {
            return None;
        }

        // Push information about this pic on stack for children to read.
        state.push_picture(PictureInfo {
            spatial_node_index: self.spatial_node_index,
        });

        // See if this picture actually needs a surface for compositing.
        let actual_composite_mode = match self.requested_composite_mode {
            Some(PictureCompositeMode::Filter(filter)) if filter.is_noop() => None,
            mode => mode,
        };

        if let Some(composite_mode) = actual_composite_mode {
            // Retrieve the positioning node information for the parent surface.
            let parent_raster_spatial_node_index = state.current_surface().raster_spatial_node_index;
            let surface_spatial_node_index = self.spatial_node_index;

            // TODO(gw): For now, we always raster in screen space. Soon,
            //           we will be able to respect the requested raster
            //           space, and/or override the requested raster root
            //           if it makes sense to.
            let raster_space = RasterSpace::Screen;

            let inflation_factor = match composite_mode {
                PictureCompositeMode::Filter(FilterOp::Blur(blur_radius)) => {
                    // The amount of extra space needed for primitives inside
                    // this picture to ensure the visibility check is correct.
                    BLUR_SAMPLE_SCALE * blur_radius
                }
                _ => {
                    0.0
                }
            };

            let mut surface = {
                // Check if there is perspective, and thus whether a new
                // rasterization root should be established.
                let xf = frame_context.clip_scroll_tree.get_relative_transform(
                    parent_raster_spatial_node_index,
                    surface_spatial_node_index,
                ).expect("BUG: unable to get relative transform");

                let establishes_raster_root = xf.has_perspective_component();

                SurfaceInfo::new(
                    surface_spatial_node_index,
                    if establishes_raster_root {
                        surface_spatial_node_index
                    } else {
                        parent_raster_spatial_node_index
                    },
                    inflation_factor,
                    frame_context.screen_world_rect,
                    &frame_context.clip_scroll_tree,
                )
            };

            if surface_spatial_node_index != parent_raster_spatial_node_index &&
                !surface.fits_surface_size_limits()
            {
                // fall back to the parent raster root
                surface = SurfaceInfo::new(
                    surface_spatial_node_index,
                    parent_raster_spatial_node_index,
                    inflation_factor,
                    frame_context.screen_world_rect,
                    &frame_context.clip_scroll_tree,
                );
            };

            // If we have a cache key / descriptor for this surface,
            // update any transforms it cares about.
            if let Some(ref mut surface_desc) = self.surface_desc {
                surface_desc.update(
                    surface_spatial_node_index,
                    surface.raster_spatial_node_index,
                    frame_context.clip_scroll_tree,
                    raster_space,
                );
            }

            self.raster_config = Some(RasterConfig {
                composite_mode,
                establishes_raster_root: surface_spatial_node_index == surface.raster_spatial_node_index,
                surface_index: state.push_surface(surface),
            });
        }

        Some(mem::replace(&mut self.prim_list.pictures, SmallVec::new()))
    }

    /// Called after updating child pictures during the initial
    /// picture traversal.
    pub fn post_update(
        &mut self,
        child_pictures: PictureList,
        state: &mut PictureUpdateState,
        frame_context: &FrameBuildingContext,
        gpu_cache: &mut GpuCache,
    ) {
        // Pop the state information about this picture.
        state.pop_picture();

        for cluster in &mut self.prim_list.clusters {
            // Skip the cluster if backface culled.
            if !cluster.is_backface_visible {
                let containing_block_index = match self.context_3d {
                    Picture3DContext::Out => {
                        state.current_picture().map_or(ROOT_SPATIAL_NODE_INDEX, |info| {
                            info.spatial_node_index
                        })
                    }
                    Picture3DContext::In { root_data: Some(_), ancestor_index } => {
                        ancestor_index
                    }
                    Picture3DContext::In { root_data: None, ancestor_index } => {
                        ancestor_index
                    }
                };

                let map_local_to_containing_block: SpaceMapper<LayoutPixel, LayoutPixel> = SpaceMapper::new_with_target(
                    containing_block_index,
                    cluster.spatial_node_index,
                    LayoutRect::zero(), // bounds aren't going to be used for this mapping
                    &frame_context.clip_scroll_tree,
                );

                match map_local_to_containing_block.visible_face() {
                    VisibleFace::Back => continue,
                    VisibleFace::Front => {}
                }
            }

            // No point including this cluster if it can't be transformed
            let spatial_node = &frame_context
                .clip_scroll_tree
                .spatial_nodes[cluster.spatial_node_index.0 as usize];
            if !spatial_node.invertible {
                continue;
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
            cluster.is_visible = true;
            if let Some(cluster_rect) = surface.map_local_to_surface.map(&cluster.bounding_rect) {
                surface.rect = surface.rect.union(&cluster_rect);
            }
        }

        // Inflate the local bounding rect if required by the filter effect.
        let inflation_size = match self.raster_config {
            Some(RasterConfig { composite_mode: PictureCompositeMode::Filter(FilterOp::Blur(blur_radius)), .. }) |
            Some(RasterConfig { composite_mode: PictureCompositeMode::Filter(FilterOp::DropShadow(_, blur_radius, _)), .. }) => {
                Some((blur_radius * BLUR_SAMPLE_SCALE).ceil())
            }
            _ => {
                None
            }
        };
        if let Some(inflation_size) = inflation_size {
            let surface = state.current_surface_mut();
            surface.rect = surface.rect.inflate(inflation_size, inflation_size);
        }

        // Restore the pictures list used during recursion.
        self.prim_list.pictures = child_pictures;

        // If this picture establishes a surface, then map the surface bounding
        // rect into the parent surface coordinate space, and propagate that up
        // to the parent.
        if let Some(ref mut raster_config) = self.raster_config {
            let surface_rect = state.current_surface().rect;

            let mut surface_rect = TypedRect::from_untyped(&surface_rect.to_untyped());

            // Pop this surface from the stack
            state.pop_surface();

            // If the local rect changed (due to transforms in child primitives) then
            // invalidate the GPU cache location to re-upload the new local rect
            // and stretch size. Drop shadow filters also depend on the local rect
            // size for the extra GPU cache data handle.
            // TODO(gw): In future, if we support specifying a flag which gets the
            //           stretch size from the segment rect in the shaders, we can
            //           remove this invalidation here completely.
            if self.local_rect != surface_rect {
                gpu_cache.invalidate(&self.gpu_location);
                if let PictureCompositeMode::Filter(FilterOp::DropShadow(..)) = raster_config.composite_mode {
                    gpu_cache.invalidate(&self.extra_gpu_data_handle);
                }
                self.local_rect = surface_rect;
            }

            // Drop shadows draw both a content and shadow rect, so need to expand the local
            // rect of any surfaces to be composited in parent surfaces correctly.
            if let PictureCompositeMode::Filter(FilterOp::DropShadow(offset, ..)) = raster_config.composite_mode {
                let content_rect = surface_rect;
                let shadow_rect = surface_rect.translate(&offset);
                surface_rect = content_rect.union(&shadow_rect);
            }

            // Propagate up to parent surface, now that we know this surface's static rect
            let parent_surface = state.current_surface_mut();
            parent_surface.map_local_to_surface.set_target_spatial_node(
                self.spatial_node_index,
                frame_context.clip_scroll_tree,
            );
            if let Some(parent_surface_rect) = parent_surface
                .map_local_to_surface
                .map(&surface_rect) {
                parent_surface.rect = parent_surface.rect.union(&parent_surface_rect);
            }
        }
    }

    pub fn prepare_for_render(
        &mut self,
        pic_index: PictureIndex,
        prim_instance: &PrimitiveInstance,
        clipped_prim_bounding_rect: WorldRect,
        surface_index: SurfaceIndex,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
    ) -> bool {
        let (mut pic_state_for_children, pic_context) = self.take_state_and_context();

        if let Some(ref mut splitter) = pic_state_for_children.plane_splitter {
            self.resolve_split_planes(
                splitter,
                frame_state,
                frame_context.clip_scroll_tree,
            );
        }

        let raster_config = match self.raster_config {
            Some(ref mut raster_config) => raster_config,
            None => {
                return true
            }
        };

        let (raster_spatial_node_index, child_tasks) = {
            let surface_info = &mut frame_state.surfaces[raster_config.surface_index.0];
            (surface_info.raster_spatial_node_index, surface_info.take_render_tasks())
        };
        let surfaces = &mut frame_state.surfaces;

        let (map_raster_to_world, map_pic_to_raster) = create_raster_mappers(
            prim_instance.spatial_node_index,
            raster_spatial_node_index,
            frame_context.screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        let pic_rect = PictureRect::from_untyped(&self.local_rect.to_untyped());

        let (clipped, unclipped) = match get_raster_rects(
            pic_rect,
            &map_pic_to_raster,
            &map_raster_to_world,
            clipped_prim_bounding_rect,
            frame_context.device_pixel_scale,
        ) {
            Some(info) => info,
            None => return false,
        };
        let transform = map_pic_to_raster.get_transform();

        // TODO(gw): Almost all of the Picture types below use extra_gpu_cache_data
        //           to store the same type of data. The exception is the filter
        //           with a ColorMatrix, which stores the color matrix here. It's
        //           probably worth tidying this code up to be a bit more consistent.
        //           Perhaps store the color matrix after the common data, even though
        //           it's not used by that shader.

        let surface = match raster_config.composite_mode {
            PictureCompositeMode::TileCache { .. } => {
                // For a picture surface, just push any child tasks and tile
                // blits up to the parent surface.
                let surface = &mut surfaces[surface_index.0];
                surface.tasks.extend(child_tasks);

                return true;
            }
            PictureCompositeMode::Filter(FilterOp::Blur(blur_radius)) => {
                let blur_std_deviation = blur_radius * frame_context.device_pixel_scale.0;
                let blur_range = (blur_std_deviation * BLUR_SAMPLE_SCALE).ceil() as i32;

                // We need to choose whether to cache this picture, or draw
                // it into a temporary render target each frame. If we draw
                // it into a persistently cached texture, then we want to
                // draw the whole picture, without clipping it to the screen
                // dimensions, so that it can be reused as it scrolls into
                // view etc. However, if the unclipped size of the surface is
                // too big, then it will be very expensive to draw, and may
                // even be bigger than the maximum hardware render target
                // size. In these cases, it's probably best to not cache the
                // picture, and just draw a minimal portion of the picture
                // (clipped to screen bounds) to a temporary target each frame.

                let too_big_to_cache = unclipped.size.width > MAX_CACHE_SIZE ||
                                       unclipped.size.height > MAX_CACHE_SIZE;

                // If we can't create a valid cache key for this descriptor (e.g.
                // due to it referencing old non-interned style primitives), then
                // don't try to cache it.
                let has_valid_cache_key = self.surface_desc.is_some();

                if !has_valid_cache_key ||
                   too_big_to_cache ||
                   !pic_state_for_children.is_cacheable {
                    // The clipped field is the part of the picture that is visible
                    // on screen. The unclipped field is the screen-space rect of
                    // the complete picture, if no screen / clip-chain was applied
                    // (this includes the extra space for blur region). To ensure
                    // that we draw a large enough part of the picture to get correct
                    // blur results, inflate that clipped area by the blur range, and
                    // then intersect with the total screen rect, to minimize the
                    // allocation size.
                    let device_rect = clipped
                        .inflate(blur_range, blur_range)
                        .intersection(&unclipped.to_i32())
                        .unwrap();

                    let uv_rect_kind = calculate_uv_rect_kind(
                        &pic_rect,
                        &transform,
                        &device_rect,
                        frame_context.device_pixel_scale,
                        true,
                    );

                    let picture_task = RenderTask::new_picture(
                        RenderTaskLocation::Dynamic(None, device_rect.size),
                        unclipped.size,
                        pic_index,
                        device_rect.origin,
                        child_tasks,
                        uv_rect_kind,
                        pic_context.raster_spatial_node_index,
                        None,
                    );

                    let picture_task_id = frame_state.render_tasks.add(picture_task);

                    let blur_render_task = RenderTask::new_blur(
                        blur_std_deviation,
                        picture_task_id,
                        frame_state.render_tasks,
                        RenderTargetKind::Color,
                        ClearMode::Transparent,
                    );

                    let render_task_id = frame_state.render_tasks.add(blur_render_task);

                    surfaces[surface_index.0].tasks.push(render_task_id);

                    PictureSurface::RenderTask(render_task_id)
                } else {
                    // Request a render task that will cache the output in the
                    // texture cache.
                    let device_rect = unclipped.to_i32();

                    let uv_rect_kind = calculate_uv_rect_kind(
                        &pic_rect,
                        &transform,
                        &device_rect,
                        frame_context.device_pixel_scale,
                        true,
                    );

                    // TODO(gw): Probably worth changing the render task caching API
                    //           so that we don't need to always clone the key.
                    let cache_key = self.surface_desc
                        .as_ref()
                        .expect("bug: no cache key for surface")
                        .cache_key
                        .clone();

                    let cache_item = frame_state.resource_cache.request_render_task(
                        RenderTaskCacheKey {
                            size: device_rect.size,
                            kind: RenderTaskCacheKeyKind::Picture(cache_key),
                        },
                        frame_state.gpu_cache,
                        frame_state.render_tasks,
                        None,
                        false,
                        |render_tasks| {
                            let picture_task = RenderTask::new_picture(
                                RenderTaskLocation::Dynamic(None, device_rect.size),
                                unclipped.size,
                                pic_index,
                                device_rect.origin,
                                child_tasks,
                                uv_rect_kind,
                                pic_context.raster_spatial_node_index,
                                None,
                            );

                            let picture_task_id = render_tasks.add(picture_task);

                            let blur_render_task = RenderTask::new_blur(
                                blur_std_deviation,
                                picture_task_id,
                                render_tasks,
                                RenderTargetKind::Color,
                                ClearMode::Transparent,
                            );

                            let render_task_id = render_tasks.add(blur_render_task);

                            surfaces[surface_index.0].tasks.push(render_task_id);

                            render_task_id
                        }
                    );

                    PictureSurface::TextureCache(cache_item)
                }
            }
            PictureCompositeMode::Filter(FilterOp::DropShadow(offset, blur_radius, color)) => {
                let blur_std_deviation = blur_radius * frame_context.device_pixel_scale.0;
                let blur_range = (blur_std_deviation * BLUR_SAMPLE_SCALE).ceil() as i32;

                // The clipped field is the part of the picture that is visible
                // on screen. The unclipped field is the screen-space rect of
                // the complete picture, if no screen / clip-chain was applied
                // (this includes the extra space for blur region). To ensure
                // that we draw a large enough part of the picture to get correct
                // blur results, inflate that clipped area by the blur range, and
                // then intersect with the total screen rect, to minimize the
                // allocation size.
                let device_rect = clipped
                    .inflate(blur_range, blur_range)
                    .intersection(&unclipped.to_i32())
                    .unwrap();

                let uv_rect_kind = calculate_uv_rect_kind(
                    &pic_rect,
                    &transform,
                    &device_rect,
                    frame_context.device_pixel_scale,
                    true,
                );

                let mut picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, device_rect.size),
                    unclipped.size,
                    pic_index,
                    device_rect.origin,
                    child_tasks,
                    uv_rect_kind,
                    pic_context.raster_spatial_node_index,
                    None,
                );
                picture_task.mark_for_saving();

                let picture_task_id = frame_state.render_tasks.add(picture_task);

                let blur_render_task = RenderTask::new_blur(
                    blur_std_deviation.round(),
                    picture_task_id,
                    frame_state.render_tasks,
                    RenderTargetKind::Color,
                    ClearMode::Transparent,
                );

                self.secondary_render_task_id = Some(picture_task_id);

                let render_task_id = frame_state.render_tasks.add(blur_render_task);
                surfaces[surface_index.0].tasks.push(render_task_id);

                if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handle) {
                    // TODO(gw): This is very hacky code below! It stores an extra
                    //           brush primitive below for the special case of a
                    //           drop-shadow where we need a different local
                    //           rect for the shadow. To tidy this up in future,
                    //           we could consider abstracting the code in prim_store.rs
                    //           that writes a brush primitive header.

                    // Basic brush primitive header is (see end of prepare_prim_for_render_inner in prim_store.rs)
                    //  [brush specific data]
                    //  [segment_rect, segment data]
                    let shadow_rect = self.local_rect.translate(&offset);

                    // ImageBrush colors
                    request.push(color.premultiplied());
                    request.push(PremultipliedColorF::WHITE);
                    request.push([
                        self.local_rect.size.width,
                        self.local_rect.size.height,
                        0.0,
                        0.0,
                    ]);

                    // segment rect / extra data
                    request.push(shadow_rect);
                    request.push([0.0, 0.0, 0.0, 0.0]);
                }

                PictureSurface::RenderTask(render_task_id)
            }
            PictureCompositeMode::MixBlend(..) => {
                let uv_rect_kind = calculate_uv_rect_kind(
                    &pic_rect,
                    &transform,
                    &clipped,
                    frame_context.device_pixel_scale,
                    true,
                );

                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, clipped.size),
                    unclipped.size,
                    pic_index,
                    clipped.origin,
                    child_tasks,
                    uv_rect_kind,
                    pic_context.raster_spatial_node_index,
                    None,
                );

                let readback_task_id = frame_state.render_tasks.add(
                    RenderTask::new_readback(clipped)
                );

                self.secondary_render_task_id = Some(readback_task_id);
                surfaces[surface_index.0].tasks.push(readback_task_id);

                let render_task_id = frame_state.render_tasks.add(picture_task);
                surfaces[surface_index.0].tasks.push(render_task_id);
                PictureSurface::RenderTask(render_task_id)
            }
            PictureCompositeMode::Filter(filter) => {
                if let FilterOp::ColorMatrix(m) = filter {
                    if let Some(mut request) = frame_state.gpu_cache.request(&mut self.extra_gpu_data_handle) {
                        for i in 0..5 {
                            request.push([m[i*4], m[i*4+1], m[i*4+2], m[i*4+3]]);
                        }
                    }
                }

                let uv_rect_kind = calculate_uv_rect_kind(
                    &pic_rect,
                    &transform,
                    &clipped,
                    frame_context.device_pixel_scale,
                    true,
                );

                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, clipped.size),
                    unclipped.size,
                    pic_index,
                    clipped.origin,
                    child_tasks,
                    uv_rect_kind,
                    pic_context.raster_spatial_node_index,
                    None,
                );

                let render_task_id = frame_state.render_tasks.add(picture_task);
                surfaces[surface_index.0].tasks.push(render_task_id);
                PictureSurface::RenderTask(render_task_id)
            }
            PictureCompositeMode::Blit => {
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
                    frame_context.device_pixel_scale,
                    supports_snapping,
                );

                let picture_task = RenderTask::new_picture(
                    RenderTaskLocation::Dynamic(None, clipped.size),
                    unclipped.size,
                    pic_index,
                    clipped.origin,
                    child_tasks,
                    uv_rect_kind,
                    pic_context.raster_spatial_node_index,
                    None,
                );

                let render_task_id = frame_state.render_tasks.add(picture_task);
                surfaces[surface_index.0].tasks.push(render_task_id);
                PictureSurface::RenderTask(render_task_id)
            }
        };

        surfaces[raster_config.surface_index.0].surface = Some(surface);

        true
    }
}

// Calculate a single screen-space UV for a picture.
fn calculate_screen_uv(
    local_pos: &PicturePoint,
    transform: &PictureToRasterTransform,
    rendered_rect: &DeviceRect,
    device_pixel_scale: DevicePixelScale,
    supports_snapping: bool,
) -> DevicePoint {
    let raster_pos = match transform.transform_point2d(local_pos) {
        Some(pos) => pos,
        None => {
            //Warning: this is incorrect and needs to be fixed properly.
            // The transformation has put a local vertex behind the near clipping plane...
            // Proper solution would be to keep the near-clipping-plane results around
            // (currently produced by calculate_screen_bounding_rect) and use them here.
            return DevicePoint::new(0.5, 0.5);
        }
    };

    let raster_to_device_space = TypedScale::new(1.0) * device_pixel_scale;

    let mut device_pos = raster_pos * raster_to_device_space;

    // Apply snapping for axis-aligned scroll nodes, as per prim_shared.glsl.
    if transform.transform_kind() == TransformedRectKind::AxisAligned && supports_snapping {
        device_pos.x = (device_pos.x + 0.5).floor();
        device_pos.y = (device_pos.y + 0.5).floor();
    }

    DevicePoint::new(
        (device_pos.x - rendered_rect.origin.x) / rendered_rect.size.width,
        (device_pos.y - rendered_rect.origin.y) / rendered_rect.size.height,
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
                                           .unwrap_or(RasterRect::max_rect());

    let map_pic_to_raster = SpaceMapper::new_with_target(
        raster_spatial_node_index,
        surface_spatial_node_index,
        raster_bounds,
        clip_scroll_tree,
    );

    (map_raster_to_world, map_pic_to_raster)
}

// Convert a list of reference primitives into a map of prim uid -> world position.
fn build_ref_prims(
    ref_prims: &[ReferencePrimitive],
    prim_map: &mut FastHashMap<ItemUid, WorldPoint>,
    clip_scroll_tree: &ClipScrollTree,
) {
    prim_map.clear();

    let mut map_local_to_world = SpaceMapper::new(
        ROOT_SPATIAL_NODE_INDEX,
        WorldRect::zero(),
    );

    for ref_prim in ref_prims {
        map_local_to_world.set_target_spatial_node(
            ref_prim.spatial_node_index,
            clip_scroll_tree,
        );

        // We only care about the origin.
        // TODO(gw): Consider adding a map_point to SpaceMapper.
        let rect = LayoutRect::new(
            ref_prim.local_pos,
            LayoutSize::zero(),
        );

        if let Some(rect) = map_local_to_world.map(&rect) {
            prim_map.insert(ref_prim.uid, rect.origin);
        }
    }
}

// Attempt to correlate the offset between two display lists by
// comparing the offsets between a small number of primitives in
// each display list.
// TODO(gw): This is basically a horrible hack - there must be a better
//           way to achieve this!
fn correlate_prim_maps(
    old_prims: &FastHashMap<ItemUid, WorldPoint>,
    new_prims: &FastHashMap<ItemUid, WorldPoint>,
) -> Option<WorldVector2D> {
    let mut map: FastHashMap<VectorKey, usize> = FastHashMap::default();

    // Find primitives with the same uid, find the difference
    // between them and store the frequency of this offset
    // in a hash map.
    for (uid, old_point) in old_prims {
        if let Some(new_point) = new_prims.get(uid) {
            let key = (*new_point - *old_point).round().into();

            let key_count = map.entry(key).or_insert(0);
            *key_count += 1;
        }
    }

    // Calculate the mode (the most common frequency of offset). This
    // can be different for some primitives, if they've animated, or
    // are attached to a different scroll node etc.
    map.into_iter()
        .max_by_key(|&(_, count)| count)
        .and_then(|(offset, count)| {
            // We will assume we can use the calculated offset if we
            // found more than one quarter of the selected reference
            // primitives to have the same offset.
            let prims_available = new_prims.len().min(old_prims.len());
            if count >= prims_available / 4 {
                Some(offset.into())
            } else {
                None
            }
        })
}

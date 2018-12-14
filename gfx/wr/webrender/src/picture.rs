/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceRect, FilterOp, MixBlendMode, PipelineId, PremultipliedColorF, PictureRect, PicturePoint};
use api::{DeviceIntRect, DevicePoint, LayoutRect, PictureToRasterTransform, LayoutPixel, PropertyBinding, PropertyBindingId};
use api::{DevicePixelScale, RasterRect, RasterSpace, DeviceIntPoint, ColorF, ImageKey, DirtyRect};
use api::{PicturePixel, RasterPixel, WorldPixel, WorldRect, ImageFormat, ImageDescriptor, LayoutSize, LayoutPoint};
use box_shadow::{BLUR_SAMPLE_SCALE};
use clip::{ClipNodeCollector, ClipStore, ClipChainId, ClipChainNode};
use clip_scroll_tree::{ROOT_SPATIAL_NODE_INDEX, ClipScrollTree, SpatialNodeIndex};
use device::TextureFilter;
use euclid::{TypedScale, vec3, TypedRect, TypedPoint2D, TypedSize2D};
use euclid::approxeq::ApproxEq;
use intern::ItemUid;
use internal_types::{FastHashMap, PlaneSplitter};
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureState, PictureContext};
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use gpu_types::{TransformPalette, TransformPaletteId, UvRectKind};
use internal_types::FastHashSet;
use plane_split::{Clipper, Polygon, Splitter};
use prim_store::{PictureIndex, PrimitiveInstance, SpaceMapper, VisibleFace, PrimitiveInstanceKind};
use prim_store::{get_raster_rects, CoordinateSpaceMapping};
use prim_store::{OpacityBindingStorage, PrimitiveTemplateKind, ImageInstanceStorage, OpacityBindingIndex, SizeKey};
use render_backend::FrameResources;
use render_task::{ClearMode, RenderTask, RenderTaskCacheEntryHandle, TileBlit};
use render_task::{RenderTaskCacheKey, RenderTaskCacheKeyKind, RenderTaskId, RenderTaskLocation};
use resource_cache::ResourceCache;
use scene::{FilterOpHelpers, SceneProperties};
use scene_builder::DocumentResources;
use smallvec::SmallVec;
use surface::{SurfaceDescriptor, TransformKey};
use std::{mem, ops};
use texture_cache::{Eviction, TextureCacheHandle};
use tiling::RenderTargetKind;
use util::{TransformedRectKind, MatrixHelpers, MaxRect, RectHelpers};

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

/// Stores a map of cached picture tiles that are retained
/// between new scenes.
pub struct RetainedTiles {
    pub tiles: FastHashMap<TileDescriptor, TextureCacheHandle>,
}

impl RetainedTiles {
    pub fn new() -> Self {
        RetainedTiles {
            tiles: FastHashMap::default(),
        }
    }
}

/// Unit for tile coordinates.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct TileCoordinate;

// Geometry types for tile coordinates.
pub type TileOffset = TypedPoint2D<i32, TileCoordinate>;
pub type TileSize = TypedSize2D<i32, TileCoordinate>;
pub type TileRect = TypedRect<i32, TileCoordinate>;

/// The size in device pixels of a cached tile. The currently chosen
/// size is arbitrary. We should do some profiling to find the best
/// size for real world pages.
pub const TILE_SIZE_DP: i32 = 512;

/// Information about the state of a transform dependency.
#[derive(Debug)]
pub struct TileTransformInfo {
    /// The spatial node in the current clip-scroll tree that
    /// this transform maps to.
    spatial_node_index: SpatialNodeIndex,
    /// Tiles check this to see if the dependencies have changed.
    changed: bool,
}

#[derive(Debug)]
pub struct GlobalTransformInfo {
    /// Current (quantized) value of the transform, that is
    /// independent of the value of the spatial node index.
    /// Only calculated on first use.
    current: Option<TransformKey>,
    /// Tiles check this to see if the dependencies have changed.
    changed: bool,
}

/// Information about the state of an opacity binding.
#[derive(Debug)]
pub struct OpacityBindingInfo {
    /// The current value retrieved from dynamic scene properties.
    value: f32,
    /// True if it was changed (or is new) since the last frame build.
    changed: bool,
}

/// Information about a cached tile.
#[derive(Debug)]
pub struct Tile {
    /// The set of opacity bindings that this tile depends on.
    opacity_bindings: FastHashSet<PropertyBindingId>,
    /// Set of image keys that this tile depends on.
    image_keys: FastHashSet<ImageKey>,
    /// If true, this tile is marked valid, and the existing texture
    /// cache handle can be used. Tiles are invalidated during the
    /// build_dirty_regions method.
    is_valid: bool,
    /// If false, this tile cannot be cached (e.g. it has an external
    /// video image attached to it). In future, we could add an API
    /// for the embedder to tell us if the external image changed.
    /// This is set during primitive dependency updating.
    is_cacheable: bool,
    /// If true, this tile is currently in use by the cache. It
    /// may be false if the tile is outside the bounding rect of
    /// the current picture, but hasn't been discarded yet. This
    /// is calculated during primitive dependency updating.
    pub in_use: bool,
    /// If true, this tile is currently visible on screen. This
    /// is calculated during build_dirty_regions.
    pub is_visible: bool,
    /// Handle to the cached texture for this tile.
    pub handle: TextureCacheHandle,
    /// A map from clip-scroll tree spatial node indices to the tile
    /// transforms. This allows the tile transforms to be stable
    /// if the content of the tile is the same, but the shape of the
    /// clip-scroll tree changes between scenes in other areas.
    tile_transform_map: FastHashMap<SpatialNodeIndex, TileTransformIndex>,
    /// Information about the transforms that is not part of the cache key.
    transform_info: Vec<TileTransformInfo>,
    /// Uniquely describes the content of this tile, in a way that can be
    /// (reasonably) efficiently hashed and compared.
    descriptor: TileDescriptor,
}

impl Tile {
    /// Construct a new, invalid tile.
    fn new(
        tile_offset: TileOffset,
        local_tile_size: SizeKey,
        raster_transform: TransformKey,
    ) -> Self {
        Tile {
            opacity_bindings: FastHashSet::default(),
            image_keys: FastHashSet::default(),
            is_valid: false,
            is_visible: false,
            is_cacheable: true,
            in_use: false,
            handle: TextureCacheHandle::invalid(),
            descriptor: TileDescriptor::new(
                tile_offset,
                local_tile_size,
                raster_transform,
            ),
            tile_transform_map: FastHashMap::default(),
            transform_info: Vec::new(),
        }
    }

    /// Add a (possibly) new transform dependency to this tile.
    fn push_transform_dependency(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        surface_spatial_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
        global_transforms: &mut [GlobalTransformInfo],
    ) {
        // If the primitive is positioned by the same spatial
        // node as the surface, we don't care about it since
        // the primitive can never move to a different position
        // relative to the surface.
        if spatial_node_index == surface_spatial_node_index {
            return;
        }

        let transform_info = &mut self.transform_info;
        let descriptor = &mut self.descriptor;

        // Get the mapping from unstable spatial node index to
        // a local transform index within this tile.
        let tile_transform_index = self
            .tile_transform_map
            .entry(spatial_node_index)
            .or_insert_with(|| {
                let index = transform_info.len();

                let mapping: CoordinateSpaceMapping<LayoutPixel, PicturePixel> = CoordinateSpaceMapping::new(
                    surface_spatial_node_index,
                    spatial_node_index,
                    clip_scroll_tree,
                ).expect("todo: handle invalid mappings");

                // See if the transform changed, and cache the current
                // transform if not set before.
                let changed = get_global_transform_changed(
                    global_transforms,
                    spatial_node_index,
                    clip_scroll_tree,
                    surface_spatial_node_index,
                );

                transform_info.push(TileTransformInfo {
                    changed,
                    spatial_node_index,
                });

                let key = mapping.into();

                descriptor.transforms.push(key);

                TileTransformIndex(index as u32)
            });

        // Record the transform for this primitive / clip node.
        // TODO(gw): It might be worth storing these in runs, since they
        //           probably don't change very often between prims.
        descriptor.transform_ids.push(*tile_transform_index);
    }

    /// Destroy a tile, optionally returning a handle and cache descriptor,
    /// if this surface was valid and may be useful on the next scene.
    fn destroy(self) -> Option<(TileDescriptor, TextureCacheHandle)> {
        if self.is_valid {
            Some((self.descriptor, self.handle))
        } else {
            None
        }
    }
}

/// Index of a transform array local to the tile.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct TileTransformIndex(u32);

/// Uniquely describes the content of this tile, in a way that can be
/// (reasonably) efficiently hashed and compared.
#[derive(Debug, Eq, PartialEq, Hash)]
pub struct TileDescriptor {
    /// List of primitive unique identifiers. The uid is guaranteed
    /// to uniquely describe the content of the primitive.
    pub prim_uids: Vec<ItemUid>,

    /// List of clip node unique identifiers. The uid is guaranteed
    /// to uniquely describe the content of the clip node.
    pub clip_uids: Vec<ItemUid>,

    /// List of local tile transform ids that are used to position
    /// the primitive and clip items above.
    pub transform_ids: Vec<TileTransformIndex>,

    /// List of transforms used by this tile, along with the current
    /// quantized value.
    pub transforms: Vec<TransformKey>,

    /// The set of opacity bindings that this tile depends on.
    // TODO(gw): Ugh, get rid of all opacity binding support!
    pub opacity_bindings: Vec<PropertyBindingId>,

    /// Ensures that we hash to a tile in the same local position.
    pub tile_offset: TileOffset,
    pub local_tile_size: SizeKey,

    /// Identifies the raster configuration of the rasterization
    /// root, to ensure tiles are invalidated if they are drawn in
    /// screen-space with an incompatible transform.
    pub raster_transform: TransformKey,
}

impl TileDescriptor {
    fn new(
        tile_offset: TileOffset,
        local_tile_size: SizeKey,
        raster_transform: TransformKey,
    ) -> Self {
        TileDescriptor {
            prim_uids: Vec::new(),
            clip_uids: Vec::new(),
            transform_ids: Vec::new(),
            opacity_bindings: Vec::new(),
            transforms: Vec::new(),
            tile_offset,
            raster_transform,
            local_tile_size,
        }
    }

    /// Clear the dependency information for a tile, when the dependencies
    /// are being rebuilt.
    fn clear(&mut self) {
        self.prim_uids.clear();
        self.clip_uids.clear();
        self.transform_ids.clear();
        self.transforms.clear();
        self.opacity_bindings.clear();
    }
}

/// Represents the dirty region of a tile cache picture.
/// In future, we will want to support multiple dirty
/// regions.
#[derive(Debug)]
pub struct DirtyRegion {
    tile_offset: DeviceIntPoint,
    dirty_rect: LayoutRect,
    dirty_world_rect: WorldRect,
}

/// Represents a cache of tiles that make up a picture primitives.
pub struct TileCache {
    /// The size of each tile in local-space coordinates of the picture.
    pub local_tile_size: LayoutSize,
    /// List of tiles present in this picture (stored as a 2D array)
    pub tiles: Vec<Tile>,
    /// A set of tiles that were used last time we built
    /// the tile grid, that may be reused or discarded next time.
    pub old_tiles: FastHashMap<TileOffset, Tile>,
    /// The current size of the rect in tile coordinates.
    pub tile_rect: TileRect,
    /// List of transform keys - used to check if transforms
    /// have changed.
    pub transforms: Vec<GlobalTransformInfo>,
    /// List of opacity bindings, with some extra information
    /// about whether they changed since last frame.
    pub opacity_bindings: FastHashMap<PropertyBindingId, OpacityBindingInfo>,
    /// A helper struct to map local rects into picture coords.
    pub space_mapper: SpaceMapper<LayoutPixel, LayoutPixel>,
    /// If true, we need to update the prim dependencies, due
    /// to relative transforms changing. The dependencies are
    /// stored in each tile, and are a list of things that
    /// force the tile to re-rasterize if they change (e.g.
    /// images, transforms).
    pub needs_update: bool,
    /// If Some(..) the region that is dirty in this picture.
    pub dirty_region: Option<DirtyRegion>,
    /// The current transform of the surface itself, to allow
    /// invalidating tiles if the surface transform changes.
    /// This is only relevant when raster_space == RasterSpace::Screen.
    raster_transform: TransformKey,

    /// Contains the offset between the local picture rect that this
    /// tile cache covers, and the aligned origin where tiles are
    /// placed. This ensures that tiles are placed on correctly
    /// aligned locations between new scenes when the enclosing
    /// picture rect has a different local origin.
    local_origin: LayoutPoint,
}

impl TileCache {
    /// Construct a new tile cache.
    pub fn new() -> Self {
        TileCache {
            tiles: Vec::new(),
            old_tiles: FastHashMap::default(),
            tile_rect: TileRect::zero(),
            local_tile_size: LayoutSize::zero(),
            transforms: Vec::new(),
            opacity_bindings: FastHashMap::default(),
            needs_update: true,
            dirty_region: None,
            space_mapper: SpaceMapper::new(
                ROOT_SPATIAL_NODE_INDEX,
                LayoutRect::zero(),
            ),
            raster_transform: TransformKey::Local,
            local_origin: LayoutPoint::zero(),
        }
    }

    /// Update the transforms array for this tile cache from the clip-scroll
    /// tree. This marks each transform as changed for later use during
    /// tile invalidation.
    pub fn update_transforms(
        &mut self,
        surface_spatial_node_index: SpatialNodeIndex,
        raster_spatial_node_index: SpatialNodeIndex,
        raster_space: RasterSpace,
        frame_context: &FrameBuildingContext,
        pic_rect: LayoutRect,
    ) {
        // Initialize the space mapper with current bounds,
        // which is used during primitive dependency updates.
        let world_mapper = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface_spatial_node_index,
            frame_context.screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        let pic_bounds = world_mapper
            .unmap(&frame_context.screen_world_rect)
            .unwrap_or(LayoutRect::max_rect());

        self.space_mapper = SpaceMapper::new(
            surface_spatial_node_index,
            pic_bounds,
        );

        // Work out the local space size of each tile, based on the
        // device pixel size of tiles.
        // TODO(gw): Perhaps add a map_point API to SpaceMapper.
        let world_tile_rect = WorldRect::from_floats(
            0.0,
            0.0,
            TILE_SIZE_DP as f32 / frame_context.device_pixel_scale.0,
            TILE_SIZE_DP as f32 / frame_context.device_pixel_scale.0,
        );
        let local_tile_rect = world_mapper
            .unmap(&world_tile_rect)
            .expect("bug: unable to get local tile size");
        self.local_tile_size = local_tile_rect.size;
        self.local_origin = pic_rect.origin;

        // Walk the transforms and see if we need to rebuild the primitive
        // dependencies for each tile.
        // TODO(gw): We could be smarter here and only rebuild for the primitives
        //           which are affected by transforms that have changed.
        if self.transforms.len() == frame_context.clip_scroll_tree.spatial_nodes.len() {
            for (i, transform) in self.transforms.iter_mut().enumerate() {
                // If this relative transform was used on the previous frame,
                // update it and store whether it changed for use during
                // tile invalidation later.
                if let Some(ref mut current) = transform.current {
                    let mapping: CoordinateSpaceMapping<LayoutPixel, PicturePixel> = CoordinateSpaceMapping::new(
                        surface_spatial_node_index,
                        SpatialNodeIndex(i),
                        frame_context.clip_scroll_tree,
                    ).expect("todo: handle invalid mappings");

                    let key = mapping.into();
                    transform.changed = key != *current;
                    *current = key;
                }
            }
        } else {
            // If the size of the transforms array changed, just invalidate all the transforms for now.
            self.transforms.clear();

            for _ in 0 .. frame_context.clip_scroll_tree.spatial_nodes.len() {
                self.transforms.push(GlobalTransformInfo {
                    current: None,
                    changed: true,
                });
            }
        };

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

        // Update the state of the transform for compositing this picture.
        self.raster_transform = match raster_space {
            RasterSpace::Screen => {
                // In general cases, if we're rasterizing a picture in screen space, then the
                // value of the surface spatial node will affect the contents of the picture
                // itself. However, if the surface and raster spatial nodes are in the same
                // coordinate system (which is the common case!) then we are effectively drawing
                // in a local space anyway, so don't care about that transform for the purposes
                // of validating the surface cache contents.

                let mut key = CoordinateSpaceMapping::<LayoutPixel, PicturePixel>::new(
                    raster_spatial_node_index,
                    surface_spatial_node_index,
                    frame_context.clip_scroll_tree,
                ).expect("bug: unable to get coord mapping").into();

                if let TransformKey::ScaleOffset(ref mut key) = key {
                    key.offset_x = 0.0;
                    key.offset_y = 0.0;
                }

                key
            }
            RasterSpace::Local(..) => {
                TransformKey::local()
            }
        };

        // Walk the transforms and see if we need to rebuild the primitive
        // dependencies for each tile.
        // TODO(gw): We could be smarter here and only rebuild for the primitives
        //           which are affected by transforms that have changed.
        for tile in &mut self.tiles {
            tile.descriptor.local_tile_size = self.local_tile_size.into();
            tile.descriptor.raster_transform = self.raster_transform.clone();

            debug_assert_eq!(tile.transform_info.len(), tile.descriptor.transforms.len());
            for (info, transform) in tile.transform_info.iter_mut().zip(tile.descriptor.transforms.iter_mut()) {
                let mapping: CoordinateSpaceMapping<LayoutPixel, PicturePixel> = CoordinateSpaceMapping::new(
                    surface_spatial_node_index,
                    info.spatial_node_index,
                    frame_context.clip_scroll_tree,
                ).expect("todo: handle invalid mappings");
                let new_transform = mapping.into();

                info.changed = *transform != new_transform;
                *transform = new_transform;

                self.needs_update |= info.changed;
            }
        }

        // If we need to update the dependencies for tiles, walk each tile
        // and clear the transforms and opacity bindings arrays.
        if self.needs_update {
            debug_assert!(self.old_tiles.is_empty());

            // Clear any dependencies on the set of existing tiles, since
            // they are going to be rebuilt. Drain the tiles list and add
            // them to the old_tiles hash, for re-use next frame build.
            for (i, mut tile) in self.tiles.drain(..).enumerate() {
                let y = i as i32 / self.tile_rect.size.width;
                let x = i as i32 % self.tile_rect.size.width;
                tile.descriptor.clear();
                tile.transform_info.clear();
                tile.tile_transform_map.clear();
                tile.opacity_bindings.clear();
                tile.image_keys.clear();
                tile.in_use = false;
                let key = TileOffset::new(
                    x + self.tile_rect.origin.x,
                    y + self.tile_rect.origin.y,
                );
                self.old_tiles.insert(key, tile);
            }

            // Reset the size of the tile grid.
            self.tile_rect = TileRect::zero();
        }

        // Get the tile coordinates in the picture space.
        let pic_rect = TypedRect::from_untyped(&pic_rect.to_untyped());
        let local_pic_rect = pic_rect.translate(&-self.local_origin.to_vector());

        let x0 = (local_pic_rect.origin.x / self.local_tile_size.width).floor() as i32;
        let y0 = (local_pic_rect.origin.y / self.local_tile_size.height).floor() as i32;
        let x1 = ((local_pic_rect.origin.x + local_pic_rect.size.width) / self.local_tile_size.width).ceil() as i32;
        let y1 = ((local_pic_rect.origin.y + local_pic_rect.size.height) / self.local_tile_size.height).ceil() as i32;

        // Update the tile array allocation if needed.
        self.reconfigure_tiles_if_required(x0, y0, x1, y1);
    }

    /// Resize the 2D tiles array if needed in order to fit dependencies
    /// for a given primitive.
    fn reconfigure_tiles_if_required(
        &mut self,
        mut x0: i32,
        mut y0: i32,
        mut x1: i32,
        mut y1: i32,
    ) {
        // Determine and store the tile bounds that are now required.
        if self.tile_rect.size.width > 0 {
            x0 = x0.min(self.tile_rect.origin.x);
            x1 = x1.max(self.tile_rect.origin.x + self.tile_rect.size.width);
        }
        if self.tile_rect.size.height > 0 {
            y0 = y0.min(self.tile_rect.origin.y);
            y1 = y1.max(self.tile_rect.origin.y + self.tile_rect.size.height);
        }

        // See how many tiles are now required, and if that's different from current config.
        let x_tiles = x1 - x0;
        let y_tiles = y1 - y0;

        // Early exit if the tile configuration is the same.
        if self.tile_rect.size.width == x_tiles &&
           self.tile_rect.size.height == y_tiles &&
           self.tile_rect.origin.x == x0 &&
           self.tile_rect.origin.y == y0 {
            return;
        }

        // We will need to create a new tile array.
        let mut new_tiles = Vec::with_capacity((x_tiles * y_tiles) as usize);

        for y in 0 .. y_tiles {
            for x in 0 .. x_tiles {
                // See if we can re-use an existing tile from the old array, by mapping
                // the tile address. This saves invalidating existing tiles when we
                // just resize the picture by adding / remove primitives.
                let tx = x0 - self.tile_rect.origin.x + x;
                let ty = y0 - self.tile_rect.origin.y + y;
                let tile_offset = TileOffset::new(x + x0, y + y0);

                let tile = if tx >= 0 && ty >= 0 && tx < self.tile_rect.size.width && ty < self.tile_rect.size.height {
                    let index = (ty * self.tile_rect.size.width + tx) as usize;
                    mem::replace(
                        &mut self.tiles[index],
                        Tile::new(
                            tile_offset,
                            self.local_tile_size.into(),
                            self.raster_transform.clone(),
                        )
                    )
                } else {
                    self.old_tiles.remove(&tile_offset).unwrap_or_else(|| {
                        Tile::new(
                            tile_offset,
                            self.local_tile_size.into(),
                            self.raster_transform.clone(),
                        )
                    })
                };
                new_tiles.push(tile);
            }
        }

        self.tiles = new_tiles;
        self.tile_rect.origin = TileOffset::new(x0, y0);
        self.tile_rect.size = TileSize::new(x_tiles, y_tiles);
    }

    /// Update the dependencies for each tile for a given primitive instance.
    pub fn update_prim_dependencies(
        &mut self,
        prim_instance: &PrimitiveInstance,
        surface_spatial_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
        resources: &FrameResources,
        clip_chain_nodes: &[ClipChainNode],
        pictures: &[PicturePrimitive],
        resource_cache: &ResourceCache,
        opacity_binding_store: &OpacityBindingStorage,
        image_instances: &ImageInstanceStorage,
    ) {
        if !self.needs_update {
            return;
        }

        self.space_mapper.set_target_spatial_node(
            prim_instance.spatial_node_index,
            clip_scroll_tree,
        );

        let prim_data = &resources.as_common_data(&prim_instance);

        let prim_rect = LayoutRect::new(
            prim_instance.prim_origin,
            prim_data.prim_size,
        );
        let clip_rect = prim_data
            .prim_relative_clip_rect
            .translate(&prim_instance.prim_origin.to_vector());

        // Map the primitive local rect into the picture space.
        // TODO(gw): We should maybe store this in the primitive template
        //           during interning so that we never have to calculate
        //           it during frame building.
        let culling_rect = match prim_rect.intersection(&clip_rect) {
            Some(rect) => rect,
            None => return,
        };

        let rect = match self.space_mapper.map(&culling_rect) {
            Some(rect) => rect,
            None => {
                return;
            }
        };

        // If the rect is invalid, no need to create dependencies.
        // TODO(gw): Need to handle pictures with filters here.
        if rect.size.width <= 0.0 || rect.size.height <= 0.0 {
            return;
        }

        // Translate the rectangle into the virtual tile space
        let origin = rect.origin - self.local_origin;

        // Get the tile coordinates in the picture space.
        let x0 = (origin.x / self.local_tile_size.width).floor() as i32;
        let y0 = (origin.y / self.local_tile_size.height).floor() as i32;
        let x1 = ((origin.x + rect.size.width) / self.local_tile_size.width).ceil() as i32;
        let y1 = ((origin.y + rect.size.height) / self.local_tile_size.height).ceil() as i32;

        // Build the list of resources that this primitive has dependencies on.
        let mut opacity_bindings: SmallVec<[PropertyBindingId; 4]> = SmallVec::new();
        let mut clip_chain_spatial_nodes: SmallVec<[SpatialNodeIndex; 8]> = SmallVec::new();
        let mut clip_chain_uids: SmallVec<[ItemUid; 8]> = SmallVec::new();
        let mut image_keys: SmallVec<[ImageKey; 8]> = SmallVec::new();
        let mut current_clip_chain_id = prim_instance.clip_chain_id;

        // Some primitives can not be cached (e.g. external video images)
        let is_cacheable = prim_instance.is_cacheable(
            &resources.prim_data_store,
            resource_cache,
        );

        match prim_instance.kind {
            PrimitiveInstanceKind::Picture { pic_index,.. } => {
                // Pictures can depend on animated opacity bindings.
                let pic = &pictures[pic_index.0];
                if let Some(PictureCompositeMode::Filter(FilterOp::Opacity(binding, _))) = pic.requested_composite_mode {
                    if let PropertyBinding::Binding(key, _) = binding {
                        opacity_bindings.push(key.id);
                    }
                }
            }
            PrimitiveInstanceKind::Rectangle { opacity_binding_index, .. } => {
                if opacity_binding_index != OpacityBindingIndex::INVALID {
                    let opacity_binding = &opacity_binding_store[opacity_binding_index];
                    for binding in &opacity_binding.bindings {
                        if let PropertyBinding::Binding(key, _) = binding {
                            opacity_bindings.push(key.id);
                        }
                    }
                }
            }
            PrimitiveInstanceKind::Image { data_handle, image_instance_index, .. } => {
                let prim_data = &resources.prim_data_store[data_handle];
                let image_instance = &image_instances[image_instance_index];
                let opacity_binding_index = image_instance.opacity_binding_index;

                if opacity_binding_index != OpacityBindingIndex::INVALID {
                    let opacity_binding = &opacity_binding_store[opacity_binding_index];
                    for binding in &opacity_binding.bindings {
                        if let PropertyBinding::Binding(key, _) = binding {
                            opacity_bindings.push(key.id);
                        }
                    }
                }

                match prim_data.kind {
                    PrimitiveTemplateKind::Image { key, .. } => {
                        image_keys.push(key);
                    }
                    _ => {
                        unreachable!();
                    }
                }
            }
            PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                let prim_data = &resources.prim_data_store[data_handle];
                match prim_data.kind {
                    PrimitiveTemplateKind::YuvImage { ref yuv_key, .. } => {
                        image_keys.extend_from_slice(yuv_key);
                    }
                    _ => {
                        unreachable!();
                    }
                }
            }
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } |
            PrimitiveInstanceKind::Clear { .. } |
            PrimitiveInstanceKind::NormalBorder { .. } |
            PrimitiveInstanceKind::LinearGradient { .. } |
            PrimitiveInstanceKind::RadialGradient { .. } |
            PrimitiveInstanceKind::ImageBorder { .. } => {
                // These don't contribute dependencies
            }
        }

        // The transforms of any clips that are relative to the picture may affect
        // the content rendered by this primitive.
        while current_clip_chain_id != ClipChainId::NONE {
            let clip_chain_node = &clip_chain_nodes[current_clip_chain_id.0 as usize];
            // We only care about clip nodes that have transforms that are children
            // of the surface, since clips that are positioned by parents will be
            // handled by the clip collector when these tiles are composited.
            if clip_chain_node.spatial_node_index > surface_spatial_node_index {
                clip_chain_spatial_nodes.push(clip_chain_node.spatial_node_index);
                clip_chain_uids.push(clip_chain_node.handle.uid());
            }
            current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
        }

        // Normalize the tile coordinates before adding to tile dependencies.
        // For each affected tile, mark any of the primitive dependencies.
        for y in y0 - self.tile_rect.origin.y .. y1 - self.tile_rect.origin.y {
            for x in x0 - self.tile_rect.origin.x .. x1 - self.tile_rect.origin.x {
                let index = (y * self.tile_rect.size.width + x) as usize;
                let tile = &mut self.tiles[index];

                // Mark if the tile is cacheable at all.
                tile.is_cacheable &= is_cacheable;
                tile.in_use = true;

                // Include any image keys this tile depends on.
                for image_key in &image_keys {
                    tile.image_keys.insert(*image_key);
                }

                // Include the transform of the primitive itself.
                tile.push_transform_dependency(
                    prim_instance.spatial_node_index,
                    surface_spatial_node_index,
                    clip_scroll_tree,
                    &mut self.transforms,
                );

                // Include the transforms of any relevant clip nodes for this primitive.
                for clip_chain_spatial_node in &clip_chain_spatial_nodes {
                    tile.push_transform_dependency(
                        *clip_chain_spatial_node,
                        surface_spatial_node_index,
                        clip_scroll_tree,
                        &mut self.transforms,
                    );
                }

                // Include any opacity bindings this primitive depends on.
                for id in &opacity_bindings {
                    if tile.opacity_bindings.insert(*id) {
                        tile.descriptor.opacity_bindings.push(*id);
                    }
                }

                // Update the tile descriptor, used for tile comparison during scene swaps.
                tile.descriptor.prim_uids.push(prim_instance.uid());
                tile.descriptor.clip_uids.extend_from_slice(&clip_chain_uids);
            }
        }
    }

    /// Get a local space rectangle for a given tile coordinate.
    pub fn get_tile_rect(&self, x: i32, y: i32) -> LayoutRect {
        LayoutRect::new(
            LayoutPoint::new(
                self.local_origin.x + (self.tile_rect.origin.x + x) as f32 * self.local_tile_size.width,
                self.local_origin.y + (self.tile_rect.origin.y + y) as f32 * self.local_tile_size.height,
            ),
            self.local_tile_size,
        )
    }

    /// Build the dirty region(s) for the tile cache after all primitive
    /// dependencies have been updated.
    pub fn build_dirty_regions(
        &mut self,
        surface_spatial_node_index: SpatialNodeIndex,
        frame_context: &FrameBuildingContext,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        retained_tiles: &mut RetainedTiles,
    ) {
        self.needs_update = false;

        for (_, tile) in self.old_tiles.drain() {
            resource_cache.texture_cache.mark_unused(&tile.handle);
        }

        let world_mapper = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface_spatial_node_index,
            frame_context.screen_world_rect,
            frame_context.clip_scroll_tree,
        );

        let mut tile_offset = DeviceIntPoint::new(
            self.tile_rect.size.width,
            self.tile_rect.size.height,
        );

        let mut dirty_rect = LayoutRect::zero();

        // Step through each tile and invalidate if the dependencies have changed.
        for y in 0 .. self.tile_rect.size.height {
            for x in 0 .. self.tile_rect.size.width {
                let i = y * self.tile_rect.size.width + x;
                let tile_rect = self.get_tile_rect(x, y);
                let tile = &mut self.tiles[i as usize];

                // If this tile is unused (has no primitives on it), we can just
                // skip any invalidation / dirty region work for it.
                if !tile.in_use {
                    continue;
                }

                // Check if this tile is actually visible.
                let tile_world_rect = world_mapper
                    .map(&tile_rect)
                    .expect("bug: unable to map tile to world coords");
                tile.is_visible = frame_context.screen_world_rect.intersects(&tile_world_rect);

                // Try to reuse cached tiles from the previous scene in this new
                // scene, if possible.
                if tile.is_visible && !resource_cache.texture_cache.is_allocated(&tile.handle) {
                    // See if we have a retained tile from last scene that matches the
                    // exact content of this tile.

                    if let Some(retained_handle) = retained_tiles.tiles.remove(&tile.descriptor) {
                        // Only use if not evicted from texture cache in the meantime.
                        if resource_cache.texture_cache.is_allocated(&retained_handle) {
                            // We found a matching tile from the previous scene, so use it!
                            tile.handle = retained_handle;
                            tile.is_valid = true;
                            // We know that the hash key of the descriptor validates that
                            // the local transforms in this tile exactly match the value
                            // of the current relative transforms needed for this tile,
                            // so we can mark those transforms as valid to avoid the
                            // retained tile being invalidated below.
                            for info in &mut tile.transform_info {
                                info.changed = false;
                            }
                        }
                    }
                }

                // Invalidate the tile if not cacheable
                if !tile.is_cacheable {
                    tile.is_valid = false;
                }

                // Invalidate the tile if any images have changed
                for image_key in &tile.image_keys {
                    if resource_cache.is_image_dirty(*image_key) {
                        tile.is_valid = false;
                        break;
                    }
                }

                // Invalidate the tile if any opacity bindings changed.
                for id in &tile.opacity_bindings {
                    let changed = match self.opacity_bindings.get(id) {
                        Some(info) => info.changed,
                        None => true,
                    };
                    if changed {
                        tile.is_valid = false;
                        break;
                    }
                }

                // Invalidate the tile if any dependent transforms changed
                for info in &tile.transform_info {
                    if info.changed {
                        tile.is_valid = false;
                        break;
                    }
                }

                // Invalidate the tile if it was evicted by the texture cache.
                if !resource_cache.texture_cache.is_allocated(&tile.handle) {
                    tile.is_valid = false;
                }

                if tile.is_visible {
                    // Ensure we request the texture cache handle for this tile
                    // each frame it will be used so the texture cache doesn't
                    // decide to evict tiles that we currently want to use.
                    resource_cache.texture_cache.request(&tile.handle, gpu_cache);

                    // If we have an invalid tile, which is also visible, add it to the
                    // dirty rect we will need to draw.
                    if !tile.is_valid {
                        dirty_rect = dirty_rect.union(&tile_rect);
                        tile_offset.x = tile_offset.x.min(x);
                        tile_offset.y = tile_offset.y.min(y);
                    }
                }
            }
        }

        self.dirty_region = if dirty_rect.is_empty() {
            None
        } else {
            let dirty_world_rect = world_mapper.map(&dirty_rect).expect("todo");
            Some(DirtyRegion {
                dirty_rect,
                tile_offset,
                dirty_world_rect,
            })
        };
    }
}

/// State structure that is used during the tile cache update picture traversal.
pub struct TileCacheUpdateState {
    pub tile_cache: Option<(TileCache, SpatialNodeIndex)>,
}

impl TileCacheUpdateState {
    pub fn new() -> Self {
        TileCacheUpdateState {
            tile_cache: None,
        }
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
    fn pop_surface(
        &mut self,
    ) {
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

pub type ClusterRange = ops::Range<u32>;

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
    /// This maps from the cluster_range in a primitive
    /// instance to a set of cluster(s) that the
    /// primitive instance belongs to.
    pub prim_cluster_map: Vec<PrimitiveClusterIndex>,
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
            prim_cluster_map: Vec::new(),
        }
    }

    /// Construct a new prim list from a list of instances
    /// in render order. This does some work during scene
    /// building which makes the frame building traversals
    /// significantly faster.
    pub fn new(
        mut prim_instances: Vec<PrimitiveInstance>,
        resources: &DocumentResources
    ) -> Self {
        let mut pictures = SmallVec::new();
        let mut clusters_map = FastHashMap::default();
        let mut clusters: SmallVec<[PrimitiveCluster; 4]> = SmallVec::new();
        let mut prim_cluster_map = Vec::new();

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
                PrimitiveInstanceKind::Picture { data_handle, .. } |
                PrimitiveInstanceKind::LineDecoration { data_handle, .. } |
                PrimitiveInstanceKind::NormalBorder { data_handle, .. } |
                PrimitiveInstanceKind::ImageBorder { data_handle, .. } |
                PrimitiveInstanceKind::Rectangle { data_handle, .. } |
                PrimitiveInstanceKind::YuvImage { data_handle, .. } |
                PrimitiveInstanceKind::Image { data_handle, .. } |
                PrimitiveInstanceKind::Clear { data_handle, .. } => {
                    &resources.prim_interner[data_handle]
                }
                PrimitiveInstanceKind::LinearGradient { data_handle, .. } => {
                    &resources.linear_grad_interner[data_handle]
                }
                PrimitiveInstanceKind::RadialGradient { data_handle, ..} => {
                    &resources.radial_grad_interner[data_handle]
                }
                PrimitiveInstanceKind::TextRun { data_handle, .. } => {
                    &resources.text_run_interner[data_handle]
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
                let clip_rect = prim_data
                    .prim_relative_clip_rect
                    .translate(&prim_instance.prim_origin.to_vector());
                let culling_rect = clip_rect
                    .intersection(&prim_rect)
                    .unwrap_or(LayoutRect::zero());

                cluster.bounding_rect = cluster.bounding_rect.union(&culling_rect);
            }

            // Define a range of clusters that this primitive belongs to. For now, this
            // seems like overkill, since a primitive only ever belongs to one cluster.
            // However, in the future the clusters will include spatial information. It
            // will often be the case that a primitive may overlap more than one cluster,
            // and belong to several.
            let start = prim_cluster_map.len() as u32;
            let cluster_range = ClusterRange {
                start,
                end: start + 1,
            };

            // Store the cluster index in the map, and the range in the instance.
            prim_cluster_map.push(PrimitiveClusterIndex(cluster_index as u32));
            prim_instance.cluster_range = cluster_range;
        }

        PrimitiveList {
            prim_instances,
            pictures,
            clusters,
            prim_cluster_map,
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

    fn is_visible(&self) -> bool {
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
    ) {
        if let Some(tile_cache) = self.tile_cache.take() {
            debug_assert!(tile_cache.old_tiles.is_empty());
            for tile in tile_cache.tiles {
                if let Some((descriptor, handle)) = tile.destroy() {
                    retained_tiles.tiles.insert(
                        descriptor,
                        handle,
                    );
                }
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

        let tile_cache = match requested_composite_mode {
            Some(PictureCompositeMode::TileCache { .. }) => {
                Some(TileCache::new())
            }
            Some(_) | None => {
                None
            }
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

        // Work out the dirty world rect for this picture.
        let dirty_world_rect = match self.tile_cache {
            Some(ref tile_cache) => {
                // If a tile cache is present, extract the dirty
                // world rect from the dirty region. If there is
                // no dirty region there is nothing to render.
                // TODO(gw): We could early out here in that case?
                tile_cache
                    .dirty_region
                    .as_ref()
                    .map_or(WorldRect::zero(), |region| {
                        region.dirty_world_rect
                    })
            }
            None => {
                // No tile cache - just assume the world rect of the screen.
                frame_context.screen_world_rect
            }
        };

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

        if self.raster_config.is_some() {
            frame_state.clip_store
                .push_surface(surface_spatial_node_index);
        }

        let map_pic_to_world = SpaceMapper::new_with_target(
            ROOT_SPATIAL_NODE_INDEX,
            surface_spatial_node_index,
            dirty_world_rect,
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
            dirty_world_rect,
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
            dirty_world_rect,
        };

        let prim_list = mem::replace(&mut self.prim_list, PrimitiveList::empty());

        Some((context, state, prim_list))
    }

    pub fn restore_context(
        &mut self,
        prim_list: PrimitiveList,
        context: PictureContext,
        state: PictureState,
        frame_state: &mut FrameBuildingState,
    ) -> Option<ClipNodeCollector> {
        self.prim_list = prim_list;
        self.state = Some((state, context));

        self.raster_config.as_ref().map(|_| {
            frame_state.clip_store.pop_surface()
        })
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
            .intersection(&prim_instance.combined_local_clip_rect)
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

            // Check if there is perspective, and thus whether a new
            // rasterization root should be established.
            let xf = frame_context.clip_scroll_tree.get_relative_transform(
                parent_raster_spatial_node_index,
                surface_spatial_node_index,
            ).expect("BUG: unable to get relative transform");

            // TODO(gw): A temporary hack here to revert behavior to
            //           always raster in screen-space. This is not
            //           a problem yet, since we're not taking advantage
            //           of this for caching yet. This is a workaround
            //           for some existing issues with handling scale
            //           when rasterizing in local space mode. Once
            //           the fixes for those are in-place, we can
            //           remove this hack!
            //let local_scale = raster_space.local_scale();
            // let wants_raster_root = xf.has_perspective_component() ||
            //                         local_scale.is_some();
            let establishes_raster_root = xf.has_perspective_component();

            // TODO(gw): For now, we always raster in screen space. Soon,
            //           we will be able to respect the requested raster
            //           space, and/or override the requested raster root
            //           if it makes sense to.
            let raster_space = RasterSpace::Screen;

            let raster_spatial_node_index = if establishes_raster_root {
                surface_spatial_node_index
            } else {
                parent_raster_spatial_node_index
            };

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

            let surface_index = state.push_surface(
                SurfaceInfo::new(
                    surface_spatial_node_index,
                    raster_spatial_node_index,
                    inflation_factor,
                    frame_context.screen_world_rect,
                    &frame_context.clip_scroll_tree,
                )
            );

            self.raster_config = Some(RasterConfig {
                composite_mode,
                surface_index,
            });

            // If we have a cache key / descriptor for this surface,
            // update any transforms it cares about.
            if let Some(ref mut surface_desc) = self.surface_desc {
                surface_desc.update(
                    surface_spatial_node_index,
                    raster_spatial_node_index,
                    frame_context.clip_scroll_tree,
                    raster_space,
                );
            }
        }

        Some(mem::replace(&mut self.prim_list.pictures, SmallVec::new()))
    }

    /// Update the primitive dependencies for any active tile caches,
    /// but only *if* the transforms have made the mappings out of date.
    pub fn update_prim_dependencies(
        &self,
        tile_cache: &mut TileCache,
        surface_spatial_node_index: SpatialNodeIndex,
        frame_context: &FrameBuildingContext,
        resource_cache: &mut ResourceCache,
        resources: &FrameResources,
        pictures: &[PicturePrimitive],
        clip_store: &ClipStore,
        opacity_binding_store: &OpacityBindingStorage,
        image_instances: &ImageInstanceStorage,
    ) {
        for prim_instance in &self.prim_list.prim_instances {
            tile_cache.update_prim_dependencies(
                prim_instance,
                surface_spatial_node_index,
                &frame_context.clip_scroll_tree,
                resources,
                &clip_store.clip_chain_nodes,
                pictures,
                resource_cache,
                opacity_binding_store,
                image_instances,
            );
        }
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
                    LayoutRect::zero(),     // bounds aren't going to be used for this mapping
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
                .spatial_nodes[cluster.spatial_node_index.0];
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
        if let Some(ref raster_config) = self.raster_config {
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
        prim_local_rect: &LayoutRect,
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
            pic_context.dirty_world_rect,
            frame_context.clip_scroll_tree,
        );

        let pic_rect = PictureRect::from_untyped(&prim_local_rect.to_untyped());

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
            PictureCompositeMode::TileCache { clear_color, .. } => {
                let tile_cache = self.tile_cache.as_mut().unwrap();

                // Build the render task for a tile cache picture, if there is
                // any dirty rect.

                match tile_cache.dirty_region {
                    Some(ref dirty_region) => {
                        // Texture cache descriptor for each tile.
                        // TODO(gw): If / when we start to use tile caches with
                        //           clip masks and/or transparent backgrounds,
                        //           we will need to correctly select an opacity
                        //           here and a blend mode in batch.rs.
                        let descriptor = ImageDescriptor::new(
                            TILE_SIZE_DP,
                            TILE_SIZE_DP,
                            ImageFormat::BGRA8,
                            true,
                            false,
                        );

                        // Get a picture rect, expanded to tile boundaries.
                        let p0 = pic_rect.origin;
                        let p1 = pic_rect.bottom_right();
                        let local_tile_size = tile_cache.local_tile_size;
                        let aligned_pic_rect = PictureRect::from_floats(
                            (p0.x / local_tile_size.width).floor() * local_tile_size.width,
                            (p0.y / local_tile_size.height).floor() * local_tile_size.height,
                            (p1.x / local_tile_size.width).ceil() * local_tile_size.width,
                            (p1.y / local_tile_size.height).ceil() * local_tile_size.height,
                        );

                        let mut blits = Vec::new();

                        // Step through each tile and build the dirty rect
                        for y in 0 .. tile_cache.tile_rect.size.height {
                            for x in 0 .. tile_cache.tile_rect.size.width {
                                let i = y * tile_cache.tile_rect.size.width + x;
                                let tile = &mut tile_cache.tiles[i as usize];

                                // If tile is invalidated, and on-screen, then we will
                                // need to rasterize it.
                                if !tile.is_valid && tile.is_visible && tile.in_use {
                                    // Notify the texture cache that we want to use this handle
                                    // and make sure it is allocated.
                                    frame_state.resource_cache.texture_cache.update(
                                        &mut tile.handle,
                                        descriptor,
                                        TextureFilter::Linear,
                                        None,
                                        [0.0; 3],
                                        DirtyRect::All,
                                        frame_state.gpu_cache,
                                        None,
                                        UvRectKind::Rect,
                                        Eviction::Eager,
                                    );

                                    let cache_item = frame_state
                                        .resource_cache
                                        .get_texture_cache_item(&tile.handle);

                                    // Set up the blit command now that we know where the dest
                                    // rect is in the texture cache.
                                    let offset = DeviceIntPoint::new(
                                        (x - dirty_region.tile_offset.x) * TILE_SIZE_DP,
                                        (y - dirty_region.tile_offset.y) * TILE_SIZE_DP,
                                    );

                                    blits.push(TileBlit {
                                        target: cache_item,
                                        offset,
                                    });

                                    tile.is_valid = true;
                                }
                            }
                        }

                        // We want to clip the drawing of this and any children to the
                        // dirty rect.
                        let clipped_rect = dirty_region.dirty_world_rect;

                        let (clipped, unclipped) = match get_raster_rects(
                            aligned_pic_rect,
                            &map_pic_to_raster,
                            &map_raster_to_world,
                            clipped_rect,
                            frame_context.device_pixel_scale,
                        ) {
                            Some(info) => info,
                            None => {
                                return false;
                            }
                        };

                        let picture_task = RenderTask::new_picture(
                            RenderTaskLocation::Dynamic(None, clipped.size),
                            unclipped.size,
                            pic_index,
                            clipped.origin,
                            child_tasks,
                            UvRectKind::Rect,
                            pic_context.raster_spatial_node_index,
                            Some(clear_color),
                            blits,
                        );

                        let render_task_id = frame_state.render_tasks.add(picture_task);
                        surfaces[surface_index.0].tasks.push(render_task_id);

                        PictureSurface::RenderTask(render_task_id)
                    }
                    None => {
                        // None of the tiles have changed, so we can skip any drawing!
                        return true;
                    }
                }
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

                // TODO(gw): This size is quite arbitrary - we should do some
                //           profiling / telemetry to see when it makes sense
                //           to cache a picture.
                const MAX_CACHE_SIZE: f32 = 2048.0;
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
                        Vec::new(),
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
                                Vec::new(),
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
                    Vec::new(),
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
                    let shadow_rect = prim_local_rect.translate(&offset);

                    // ImageBrush colors
                    request.push(color.premultiplied());
                    request.push(PremultipliedColorF::WHITE);
                    request.push([
                        prim_local_rect.size.width,
                        prim_local_rect.size.height,
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
                    Vec::new(),
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
                    Vec::new(),
                );

                let render_task_id = frame_state.render_tasks.add(picture_task);
                surfaces[surface_index.0].tasks.push(render_task_id);
                PictureSurface::RenderTask(render_task_id)
            }
            PictureCompositeMode::Blit => {
                let uv_rect_kind = calculate_uv_rect_kind(
                    &pic_rect,
                    &transform,
                    &clipped,
                    frame_context.device_pixel_scale,
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
                    Vec::new(),
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
    if transform.transform_kind() == TransformedRectKind::AxisAligned {
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
) -> UvRectKind {
    let rendered_rect = rendered_rect.to_f32();

    let top_left = calculate_screen_uv(
        &pic_rect.origin,
        transform,
        &rendered_rect,
        device_pixel_scale,
    );

    let top_right = calculate_screen_uv(
        &pic_rect.top_right(),
        transform,
        &rendered_rect,
        device_pixel_scale,
    );

    let bottom_left = calculate_screen_uv(
        &pic_rect.bottom_left(),
        transform,
        &rendered_rect,
        device_pixel_scale,
    );

    let bottom_right = calculate_screen_uv(
        &pic_rect.bottom_right(),
        transform,
        &rendered_rect,
        device_pixel_scale,
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
    dirty_world_rect: WorldRect,
    clip_scroll_tree: &ClipScrollTree,
) -> (SpaceMapper<RasterPixel, WorldPixel>, SpaceMapper<PicturePixel, RasterPixel>) {
    let map_raster_to_world = SpaceMapper::new_with_target(
        ROOT_SPATIAL_NODE_INDEX,
        raster_spatial_node_index,
        dirty_world_rect,
        clip_scroll_tree,
    );

    let raster_bounds = map_raster_to_world.unmap(&dirty_world_rect)
                                           .unwrap_or(RasterRect::max_rect());

    let map_pic_to_raster = SpaceMapper::new_with_target(
        raster_spatial_node_index,
        surface_spatial_node_index,
        raster_bounds,
        clip_scroll_tree,
    );

    (map_raster_to_world, map_pic_to_raster)
}

// Check whether a relative transform between two spatial nodes has changed
// since last frame. If that relative transform hasn't been calculated, then
// do that now and store it for later use.
fn get_global_transform_changed(
    global_transforms: &mut [GlobalTransformInfo],
    spatial_node_index: SpatialNodeIndex,
    clip_scroll_tree: &ClipScrollTree,
    surface_spatial_node_index: SpatialNodeIndex,
) -> bool {
    let transform = &mut global_transforms[spatial_node_index.0];

    if transform.current.is_none() {
        let mapping: CoordinateSpaceMapping<LayoutPixel, PicturePixel> = CoordinateSpaceMapping::new(
            surface_spatial_node_index,
            spatial_node_index,
            clip_scroll_tree,
        ).expect("todo: handle invalid mappings");

        transform.current = Some(mapping.into());
        transform.changed = true;
    }

    transform.changed
}

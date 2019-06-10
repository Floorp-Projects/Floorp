/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipIntern, ClipMode, ComplexClipRegion, ImageMask};
use api::{BoxShadowClipMode, ImageKey, ImageRendering};
use api::units::*;
use crate::border::{ensure_no_corner_overlap, BorderRadiusAu};
use crate::box_shadow::{BLUR_SAMPLE_SCALE, BoxShadowClipSource, BoxShadowCacheKey};
use crate::clip_scroll_tree::{ROOT_SPATIAL_NODE_INDEX, CoordinateSystemId, ClipScrollTree, SpatialNodeIndex};
use crate::ellipse::Ellipse;
use crate::gpu_cache::{GpuCache, GpuCacheHandle, ToGpuBlocks};
use crate::gpu_types::{BoxShadowStretchMode};
use crate::image::{self, Repetition};
use crate::intern;
use crate::prim_store::{ClipData, ImageMaskData, SpaceMapper, VisibleMaskImageTile};
use crate::prim_store::{PointKey, SizeKey, RectangleKey};
use crate::render_task::to_cache_size;
use crate::resource_cache::{ImageRequest, ResourceCache};
use std::{cmp, u32};
use crate::util::{extract_inner_rect_safe, project_rect, ScaleOffset};

/*

 Module Overview

 There are a number of data structures involved in the clip module:

 ClipStore - Main interface used by other modules.

 ClipItem - A single clip item (e.g. a rounded rect, or a box shadow).
            These are an exposed API type, stored inline in a ClipNode.

 ClipNode - A ClipItem with an attached GPU handle. The GPU handle is populated
            when a ClipNodeInstance is built from this node (which happens while
            preparing primitives for render).

 ClipNodeInstance - A ClipNode with attached positioning information (a spatial
                    node index). This is stored as a contiguous array of nodes
                    within the ClipStore.

    +-----------------------+-----------------------+-----------------------+
    | ClipNodeInstance      | ClipNodeInstance      | ClipNodeInstance      |
    +-----------------------+-----------------------+-----------------------+
    | ClipItem              | ClipItem              | ClipItem              |
    | Spatial Node Index    | Spatial Node Index    | Spatial Node Index    |
    | GPU cache handle      | GPU cache handle      | GPU cache handle      |
    | ...                   | ...                   | ...                   |
    +-----------------------+-----------------------+-----------------------+
               0                        1                       2

       +----------------+    |                                              |
       | ClipNodeRange  |____|                                              |
       |    index: 1    |                                                   |
       |    count: 2    |___________________________________________________|
       +----------------+

 ClipNodeRange - A clip item range identifies a range of clip nodes instances.
                 It is stored as an (index, count).

 ClipChainNode - A clip chain node contains a handle to an interned clip item,
                 positioning information (from where the clip was defined), and
                 an optional parent link to another ClipChainNode. ClipChainId
                 is an index into an array, or ClipChainId::NONE for no parent.

    +----------------+    ____+----------------+    ____+----------------+   /---> ClipChainId::NONE
    | ClipChainNode  |   |    | ClipChainNode  |   |    | ClipChainNode  |   |
    +----------------+   |    +----------------+   |    +----------------+   |
    | ClipDataHandle |   |    | ClipDataHandle |   |    | ClipDataHandle |   |
    | Spatial index  |   |    | Spatial index  |   |    | Spatial index  |   |
    | Parent Id      |___|    | Parent Id      |___|    | Parent Id      |___|
    | ...            |        | ...            |        | ...            |
    +----------------+        +----------------+        +----------------+

 ClipChainInstance - A ClipChain that has been built for a specific primitive + positioning node.

    When given a clip chain ID, and a local primitive rect and its spatial node, the clip module
    creates a clip chain instance. This is a struct with various pieces of useful information
    (such as a local clip rect). It also contains a (index, count)
    range specifier into an index buffer of the ClipNodeInstance structures that are actually relevant
    for this clip chain instance. The index buffer structure allows a single array to be used for
    all of the clip-chain instances built in a single frame. Each entry in the index buffer
    also stores some flags relevant to the clip node in this positioning context.

    +----------------------+
    | ClipChainInstance    |
    +----------------------+
    | ...                  |
    | local_clip_rect      |________________________________________________________________________
    | clips_range          |_______________                                                        |
    +----------------------+              |                                                        |
                                          |                                                        |
    +------------------+------------------+------------------+------------------+------------------+
    | ClipNodeInstance | ClipNodeInstance | ClipNodeInstance | ClipNodeInstance | ClipNodeInstance |
    +------------------+------------------+------------------+------------------+------------------+
    | flags            | flags            | flags            | flags            | flags            |
    | ...              | ...              | ...              | ...              | ...              |
    +------------------+------------------+------------------+------------------+------------------+

 */

// Type definitions for interning clip nodes.

pub type ClipDataStore = intern::DataStore<ClipIntern>;
type ClipDataHandle = intern::Handle<ClipIntern>;

// Result of comparing a clip node instance against a local rect.
#[derive(Debug)]
enum ClipResult {
    // The clip does not affect the region at all.
    Accept,
    // The clip prevents the region from being drawn.
    Reject,
    // The clip affects part of the region. This may
    // require a clip mask, depending on other factors.
    Partial,
}

// A clip node is a single clip source, along with some
// positioning information and implementation details
// that control where the GPU data for this clip source
// can be found.
#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct ClipNode {
    pub item: ClipItem,
    pub gpu_cache_handle: GpuCacheHandle,
}

// Convert from an interning key for a clip item
// to a clip node, which is cached in the document.
// TODO(gw): These enums are a bit messy - we should
//           convert them to use named fields.
impl From<ClipItemKey> for ClipNode {
    fn from(item: ClipItemKey) -> Self {
        let item = match item {
            ClipItemKey::Rectangle(size, mode) => {
                ClipItem::Rectangle(size.into(), mode)
            }
            ClipItemKey::RoundedRectangle(size, radius, mode) => {
                ClipItem::RoundedRectangle(
                    size.into(),
                    radius.into(),
                    mode,
                )
            }
            ClipItemKey::ImageMask(size, image, repeat) => {
                ClipItem::Image {
                    image,
                    size: size.into(),
                    repeat,
                }
            }
            ClipItemKey::BoxShadow(shadow_rect_fract_offset, shadow_rect_size, shadow_radius, prim_shadow_rect, blur_radius, clip_mode) => {
                ClipItem::new_box_shadow(
                    shadow_rect_fract_offset.into(),
                    shadow_rect_size.into(),
                    shadow_radius.into(),
                    prim_shadow_rect.into(),
                    blur_radius.to_f32_px(),
                    clip_mode,
                )
            }
        };

        ClipNode {
            item,
            gpu_cache_handle: GpuCacheHandle::new(),
        }
    }
}

// Flags that are attached to instances of clip nodes.
bitflags! {
    #[cfg_attr(feature = "capture", derive(Serialize))]
    #[cfg_attr(feature = "replay", derive(Deserialize))]
    #[derive(MallocSizeOf)]
    pub struct ClipNodeFlags: u8 {
        const SAME_SPATIAL_NODE = 0x1;
        const SAME_COORD_SYSTEM = 0x2;
        const USE_FAST_PATH = 0x4;
    }
}

// Identifier for a clip chain. Clip chains are stored
// in a contiguous array in the clip store. They are
// identified by a simple index into that array.
#[derive(Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct ClipChainId(pub u32);

// The root of each clip chain is the NONE id. The
// value is specifically set to u32::MAX so that if
// any code accidentally tries to access the root
// node, a bounds error will occur.
impl ClipChainId {
    pub const NONE: Self = ClipChainId(u32::MAX);
    pub const INVALID: Self = ClipChainId(0xDEADBEEF);
}

// A clip chain node is an id for a range of clip sources,
// and a link to a parent clip chain node, or ClipChainId::NONE.
#[derive(Clone, Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct ClipChainNode {
    pub handle: ClipDataHandle,
    pub local_pos: LayoutPoint,
    pub spatial_node_index: SpatialNodeIndex,
    pub parent_clip_chain_id: ClipChainId,
}

// When a clip node is found to be valid for a
// clip chain instance, it's stored in an index
// buffer style structure. This struct contains
// an index to the node data itself, as well as
// some flags describing how this clip node instance
// is positioned.
#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ClipNodeInstance {
    pub handle: ClipDataHandle,
    pub flags: ClipNodeFlags,
    pub spatial_node_index: SpatialNodeIndex,
    pub local_pos: LayoutPoint,

    pub visible_tiles: Option<Vec<VisibleMaskImageTile>>,
}

// A range of clip node instances that were found by
// building a clip chain instance.
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ClipNodeRange {
    pub first: u32,
    pub count: u32,
}

/// A helper struct for converting between coordinate systems
/// of clip sources and primitives.
// todo(gw): optimize:
//  separate arrays for matrices
//  cache and only build as needed.
//TODO: merge with `CoordinateSpaceMapping`?
#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
enum ClipSpaceConversion {
    Local,
    ScaleOffset(ScaleOffset),
    Transform(LayoutToWorldTransform),
}

// Temporary information that is cached and reused
// during building of a clip chain instance.
#[derive(MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
struct ClipNodeInfo {
    conversion: ClipSpaceConversion,
    handle: ClipDataHandle,
    local_pos: LayoutPoint,
    spatial_node_index: SpatialNodeIndex,
}

impl ClipNodeInfo {
    fn create_instance(
        &self,
        node: &ClipNode,
        clipped_rect: &LayoutRect,
        gpu_cache: &mut GpuCache,
        resource_cache: &mut ResourceCache,
        clip_scroll_tree: &ClipScrollTree,
        request_resources: bool,
    ) -> Option<ClipNodeInstance> {
        // Calculate some flags that are required for the segment
        // building logic.
        let mut flags = match self.conversion {
            ClipSpaceConversion::Local => {
                ClipNodeFlags::SAME_SPATIAL_NODE | ClipNodeFlags::SAME_COORD_SYSTEM
            }
            ClipSpaceConversion::ScaleOffset(..) => {
                ClipNodeFlags::SAME_COORD_SYSTEM
            }
            ClipSpaceConversion::Transform(..) => {
                ClipNodeFlags::empty()
            }
        };

        // Some clip shaders support a fast path mode for simple clips.
        // For now, the fast path is only selected if:
        //  - The clip item content supports fast path
        //  - Both clip and primitive are in the root coordinate system (no need for AA along edges)
        // TODO(gw): We could also apply fast path when segments are created, since we only write
        //           the mask for a single corner at a time then, so can always consider radii uniform.
        let clip_spatial_node = &clip_scroll_tree.spatial_nodes[self.spatial_node_index.0 as usize];
        if clip_spatial_node.coordinate_system_id == CoordinateSystemId::root() &&
           flags.contains(ClipNodeFlags::SAME_COORD_SYSTEM) &&
           node.item.supports_fast_path_rendering() {
            flags |= ClipNodeFlags::USE_FAST_PATH;
        }

        let mut visible_tiles = None;

        if let ClipItem::Image { size, image, repeat } = node.item {
            let request = ImageRequest {
                key: image,
                rendering: ImageRendering::Auto,
                tile: None,
            };

            if let Some(props) = resource_cache.get_image_properties(image) {
                if let Some(tile_size) = props.tiling {
                    let mut mask_tiles = Vec::new();
                    let mask_rect = LayoutRect::new(self.local_pos, size);

                    let visible_rect = if repeat {
                        *clipped_rect
                    } else {
                        clipped_rect.intersection(&mask_rect).unwrap()
                    };

                    let repetitions = image::repetitions(
                        &mask_rect,
                        &visible_rect,
                        size,
                    );

                    // TODO: As a followup, if the image is a tiled blob, the device_image_rect below
                    // will be set to the blob's visible area.
                    let device_image_rect = DeviceIntRect::from_size(props.descriptor.size);

                    for Repetition { origin, .. } in repetitions {
                        let layout_image_rect = LayoutRect {
                            origin,
                            size,
                        };
                        let tiles = image::tiles(
                            &layout_image_rect,
                            &visible_rect,
                            &device_image_rect,
                            tile_size as i32,
                        );
                        for tile in tiles {
                            if request_resources {
                                resource_cache.request_image(
                                    request.with_tile(tile.offset),
                                    gpu_cache,
                                );
                            }
                            mask_tiles.push(VisibleMaskImageTile {
                                tile_offset: tile.offset,
                                tile_rect: tile.rect,
                            });
                        }
                    }
                    visible_tiles = Some(mask_tiles);
                } else if request_resources {
                    resource_cache.request_image(request, gpu_cache);
                }
            } else {
                // If the supplied image key doesn't exist in the resource cache,
                // skip the clip node since there is nothing to mask with.
                warn!("Clip mask with missing image key {:?}", request.key);
                return None;
            }
        }

        Some(ClipNodeInstance {
            handle: self.handle,
            flags,
            spatial_node_index: self.spatial_node_index,
            local_pos: self.local_pos,
            visible_tiles,
        })
    }
}

impl ClipNode {
    pub fn update(
        &mut self,
        gpu_cache: &mut GpuCache,
        device_pixel_scale: DevicePixelScale,
    ) {
        match self.item {
            ClipItem::Image { size, .. } => {
                if let Some(request) = gpu_cache.request(&mut self.gpu_cache_handle) {
                    let data = ImageMaskData {
                        local_mask_size: size,
                    };
                    data.write_gpu_blocks(request);
                }
            }
            ClipItem::BoxShadow(ref mut info) => {
                if let Some(mut request) = gpu_cache.request(&mut self.gpu_cache_handle) {
                    request.push([
                        info.original_alloc_size.width,
                        info.original_alloc_size.height,
                        info.clip_mode as i32 as f32,
                        0.0,
                    ]);
                    request.push([
                        info.stretch_mode_x as i32 as f32,
                        info.stretch_mode_y as i32 as f32,
                        0.0,
                        0.0,
                    ]);
                    request.push(info.prim_shadow_rect);
                }

                // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
                // "the image that would be generated by applying to the shadow a
                // Gaussian blur with a standard deviation equal to half the blur radius."
                let blur_radius_dp = info.blur_radius * 0.5;

                // Create scaling from requested size to cache size.
                let content_scale = LayoutToWorldScale::new(1.0) * device_pixel_scale;

                // Create the cache key for this box-shadow render task.
                let cache_size = to_cache_size(info.shadow_rect_alloc_size * content_scale);
                let bs_cache_key = BoxShadowCacheKey {
                    blur_radius_dp: (blur_radius_dp * content_scale.0).round() as i32,
                    clip_mode: info.clip_mode,
                    original_alloc_size: (info.original_alloc_size * content_scale).round().to_i32(),
                    br_top_left: (info.shadow_radius.top_left * content_scale).round().to_i32(),
                    br_top_right: (info.shadow_radius.top_right * content_scale).round().to_i32(),
                    br_bottom_right: (info.shadow_radius.bottom_right * content_scale).round().to_i32(),
                    br_bottom_left: (info.shadow_radius.bottom_left * content_scale).round().to_i32(),
                };

                info.cache_key = Some((cache_size, bs_cache_key));

                if let Some(mut request) = gpu_cache.request(&mut info.clip_data_handle) {
                    let data = ClipData::rounded_rect(
                        info.minimal_shadow_rect.size,
                        &info.shadow_radius,
                        ClipMode::Clip,
                    );

                    data.write(&mut request);
                }
            }
            ClipItem::Rectangle(size, mode) => {
                if let Some(mut request) = gpu_cache.request(&mut self.gpu_cache_handle) {
                    let data = ClipData::uniform(size, 0.0, mode);
                    data.write(&mut request);
                }
            }
            ClipItem::RoundedRectangle(size, ref radius, mode) => {
                if let Some(mut request) = gpu_cache.request(&mut self.gpu_cache_handle) {
                    let data = ClipData::rounded_rect(size, radius, mode);
                    data.write(&mut request);
                }
            }
        }
    }
}

/// The main clipping public interface that other modules access.
#[derive(MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct ClipStore {
    pub clip_chain_nodes: Vec<ClipChainNode>,
    clip_node_instances: Vec<ClipNodeInstance>,
    clip_node_info: Vec<ClipNodeInfo>,
}

// A clip chain instance is what gets built for a given clip
// chain id + local primitive region + positioning node.
#[derive(Debug)]
pub struct ClipChainInstance {
    pub clips_range: ClipNodeRange,
    // Combined clip rect for clips that are in the
    // same coordinate system as the primitive.
    pub local_clip_rect: LayoutRect,
    pub has_non_local_clips: bool,
    // If true, this clip chain requires allocation
    // of a clip mask.
    pub needs_mask: bool,
    // Combined clip rect in picture space (may
    // be more conservative that local_clip_rect).
    pub pic_clip_rect: PictureRect,
    // Space, in which the `pic_clip_rect` is defined.
    pub pic_spatial_node_index: SpatialNodeIndex,
}

impl ClipChainInstance {
    pub fn empty() -> Self {
        ClipChainInstance {
            clips_range: ClipNodeRange {
                first: 0,
                count: 0,
            },
            local_clip_rect: LayoutRect::zero(),
            has_non_local_clips: false,
            needs_mask: false,
            pic_clip_rect: PictureRect::zero(),
            pic_spatial_node_index: ROOT_SPATIAL_NODE_INDEX,
        }
    }
}

/// Maintains a stack of clip chain ids that are currently active,
/// when a clip exists on a picture that has no surface, and is passed
/// on down to the child primitive(s).
pub struct ClipChainStack {
    // TODO(gw): Consider using SmallVec, or recycling the clip stacks here.
    /// A stack of clip chain lists. Each time a new surface is pushed,
    /// a new entry is added to the main stack. Each time a new picture
    /// without surface is pushed, it adds the picture clip chain to the
    /// current stack list.
    pub stack: Vec<Vec<ClipChainId>>,
}

impl ClipChainStack {
    pub fn new() -> Self {
        ClipChainStack {
            stack: vec![vec![]],
        }
    }

    /// Push a clip chain root onto the currently active list.
    pub fn push_clip(&mut self, clip_chain_id: ClipChainId) {
        self.stack.last_mut().unwrap().push(clip_chain_id);
    }

    /// Pop a clip chain root from the currently active list.
    pub fn pop_clip(&mut self) {
        self.stack.last_mut().unwrap().pop().unwrap();
    }

    /// When a surface is created, it takes all clips and establishes a new
    /// stack of clips to be propagated.
    pub fn push_surface(&mut self) {
        self.stack.push(Vec::new());
    }

    /// Pop a surface from the clip chain stack
    pub fn pop_surface(&mut self) {
        self.stack.pop().unwrap();
    }

    /// Get the list of currently active clip chains
    pub fn current_clips(&self) -> &[ClipChainId] {
        self.stack.last().unwrap()
    }
}

impl ClipStore {
    pub fn new() -> Self {
        ClipStore {
            clip_chain_nodes: Vec::new(),
            clip_node_instances: Vec::new(),
            clip_node_info: Vec::new(),
        }
    }

    pub fn get_clip_chain(&self, clip_chain_id: ClipChainId) -> &ClipChainNode {
        &self.clip_chain_nodes[clip_chain_id.0 as usize]
    }

    pub fn add_clip_chain_node(
        &mut self,
        handle: ClipDataHandle,
        local_pos: LayoutPoint,
        spatial_node_index: SpatialNodeIndex,
        parent_clip_chain_id: ClipChainId,
    ) -> ClipChainId {
        let id = ClipChainId(self.clip_chain_nodes.len() as u32);
        self.clip_chain_nodes.push(ClipChainNode {
            handle,
            spatial_node_index,
            local_pos,
            parent_clip_chain_id,
        });
        id
    }

    pub fn get_instance_from_range(
        &self,
        node_range: &ClipNodeRange,
        index: u32,
    ) -> &ClipNodeInstance {
        &self.clip_node_instances[(node_range.first + index) as usize]
    }

    // The main interface other code uses. Given a local primitive, positioning
    // information, and a clip chain id, build an optimized clip chain instance.
    pub fn build_clip_chain_instance(
        &mut self,
        clip_chains: &[ClipChainId],
        local_prim_rect: LayoutRect,
        local_prim_clip_rect: LayoutRect,
        spatial_node_index: SpatialNodeIndex,
        prim_to_pic_mapper: &SpaceMapper<LayoutPixel, PicturePixel>,
        pic_to_world_mapper: &SpaceMapper<PicturePixel, WorldPixel>,
        clip_scroll_tree: &ClipScrollTree,
        gpu_cache: &mut GpuCache,
        resource_cache: &mut ResourceCache,
        device_pixel_scale: DevicePixelScale,
        world_rect: &WorldRect,
        clip_data_store: &mut ClipDataStore,
        request_resources: bool,
    ) -> Option<ClipChainInstance> {
        let mut local_clip_rect = local_prim_clip_rect;

        // Walk the clip chain to build local rects, and collect the
        // smallest possible local/device clip area.

        self.clip_node_info.clear();

        for clip_chain_root in clip_chains {
            let mut current_clip_chain_id = *clip_chain_root;

            // for each clip chain node
            while current_clip_chain_id != ClipChainId::NONE {
                let clip_chain_node = &self.clip_chain_nodes[current_clip_chain_id.0 as usize];

                if !add_clip_node_to_current_chain(
                    clip_chain_node,
                    spatial_node_index,
                    &mut local_clip_rect,
                    &mut self.clip_node_info,
                    clip_data_store,
                    clip_scroll_tree,
                ) {
                    return None;
                }

                current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
            }
        }

        let local_bounding_rect = local_prim_rect.intersection(&local_clip_rect)?;
        let pic_clip_rect = prim_to_pic_mapper.map(&local_bounding_rect)?;
        let world_clip_rect = pic_to_world_mapper.map(&pic_clip_rect)?;

        // Now, we've collected all the clip nodes that *potentially* affect this
        // primitive region, and reduced the size of the prim region as much as possible.

        // Run through the clip nodes, and see which ones affect this prim region.

        let first_clip_node_index = self.clip_node_instances.len() as u32;
        let mut has_non_local_clips = false;
        let mut needs_mask = false;

        // For each potential clip node
        for node_info in self.clip_node_info.drain(..) {
            let node = &mut clip_data_store[node_info.handle];

            // See how this clip affects the prim region.
            let clip_result = match node_info.conversion {
                ClipSpaceConversion::Local => {
                    node.item.get_clip_result(node_info.local_pos, &local_bounding_rect)
                }
                ClipSpaceConversion::ScaleOffset(ref scale_offset) => {
                    has_non_local_clips = true;
                    node.item.get_clip_result(node_info.local_pos, &scale_offset.unmap_rect(&local_bounding_rect))
                }
                ClipSpaceConversion::Transform(ref transform) => {
                    has_non_local_clips = true;
                    node.item.get_clip_result_complex(
                        node_info.local_pos,
                        transform,
                        &world_clip_rect,
                        world_rect,
                    )
                }
            };

            match clip_result {
                ClipResult::Accept => {
                    // Doesn't affect the primitive at all, so skip adding to list
                }
                ClipResult::Reject => {
                    // Completely clips the supplied prim rect
                    return None;
                }
                ClipResult::Partial => {
                    // Needs a mask -> add to clip node indices

                    // TODO(gw): Ensure this only runs once on each node per frame?
                    node.update(
                        gpu_cache,
                        device_pixel_scale,
                    );

                    // Create the clip node instance for this clip node
                    if let Some(instance) = node_info.create_instance(
                        node,
                        &local_bounding_rect,
                        gpu_cache,
                        resource_cache,
                        clip_scroll_tree,
                        request_resources,
                    ) {
                        // As a special case, a partial accept of a clip rect that is
                        // in the same coordinate system as the primitive doesn't need
                        // a clip mask. Instead, it can be handled by the primitive
                        // vertex shader as part of the local clip rect. This is an
                        // important optimization for reducing the number of clip
                        // masks that are allocated on common pages.
                        needs_mask |= match node.item {
                            ClipItem::Rectangle(_, ClipMode::ClipOut) |
                            ClipItem::RoundedRectangle(..) |
                            ClipItem::Image { .. } |
                            ClipItem::BoxShadow(..) => {
                                true
                            }

                            ClipItem::Rectangle(_, ClipMode::Clip) => {
                                !instance.flags.contains(ClipNodeFlags::SAME_COORD_SYSTEM)
                            }
                        };

                        // Store this in the index buffer for this clip chain instance.
                        self.clip_node_instances.push(instance);
                    }
                }
            }
        }

        // Get the range identifying the clip nodes in the index buffer.
        let clips_range = ClipNodeRange {
            first: first_clip_node_index,
            count: self.clip_node_instances.len() as u32 - first_clip_node_index,
        };

        // Return a valid clip chain instance
        Some(ClipChainInstance {
            clips_range,
            has_non_local_clips,
            local_clip_rect,
            pic_clip_rect,
            pic_spatial_node_index: prim_to_pic_mapper.ref_spatial_node_index,
            needs_mask,
        })
    }

    pub fn clear_old_instances(&mut self) {
        self.clip_node_instances.clear();
    }
}

pub struct ComplexTranslateIter<I> {
    source: I,
    offset: LayoutVector2D,
}

impl<I: Iterator<Item = ComplexClipRegion>> Iterator for ComplexTranslateIter<I> {
    type Item = ComplexClipRegion;
    fn next(&mut self) -> Option<Self::Item> {
        self.source
            .next()
            .map(|mut complex| {
                complex.rect = complex.rect.translate(&self.offset);
                complex
            })
    }
}

#[derive(Clone, Debug)]
pub struct ClipRegion<I> {
    pub main: LayoutRect,
    pub image_mask: Option<ImageMask>,
    pub complex_clips: I,
}

impl<J> ClipRegion<ComplexTranslateIter<J>> {
    pub fn create_for_clip_node(
        rect: LayoutRect,
        complex_clips: J,
        mut image_mask: Option<ImageMask>,
        reference_frame_relative_offset: &LayoutVector2D,
    ) -> Self
    where
        J: Iterator<Item = ComplexClipRegion>
    {
        if let Some(ref mut image_mask) = image_mask {
            image_mask.rect = image_mask.rect.translate(reference_frame_relative_offset);
        }

        ClipRegion {
            main: rect.translate(reference_frame_relative_offset),
            image_mask,
            complex_clips: ComplexTranslateIter {
                source: complex_clips,
                offset: *reference_frame_relative_offset,
            },
        }
    }
}

impl ClipRegion<Option<ComplexClipRegion>> {
    pub fn create_for_clip_node_with_local_clip(
        local_clip: &LayoutRect,
        reference_frame_relative_offset: &LayoutVector2D
    ) -> Self {
        ClipRegion {
            main: local_clip.translate(reference_frame_relative_offset),
            image_mask: None,
            complex_clips: None,
        }
    }
}

// The ClipItemKey is a hashable representation of the contents
// of a clip item. It is used during interning to de-duplicate
// clip nodes between frames and display lists. This allows quick
// comparison of clip node equality by handle, and also allows
// the uploaded GPU cache handle to be retained between display lists.
// TODO(gw): Maybe we should consider constructing these directly
//           in the DL builder?
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ClipItemKey {
    Rectangle(SizeKey, ClipMode),
    RoundedRectangle(SizeKey, BorderRadiusAu, ClipMode),
    ImageMask(SizeKey, ImageKey, bool),
    BoxShadow(PointKey, SizeKey, BorderRadiusAu, RectangleKey, Au, BoxShadowClipMode),
}

impl ClipItemKey {
    pub fn rectangle(size: LayoutSize, mode: ClipMode) -> Self {
        ClipItemKey::Rectangle(size.into(), mode)
    }

    pub fn rounded_rect(size: LayoutSize, mut radii: BorderRadius, mode: ClipMode) -> Self {
        if radii.is_zero() {
            ClipItemKey::rectangle(size, mode)
        } else {
            ensure_no_corner_overlap(&mut radii, size);
            ClipItemKey::RoundedRectangle(
                size.into(),
                radii.into(),
                mode,
            )
        }
    }

    pub fn image_mask(image_mask: &ImageMask) -> Self {
        ClipItemKey::ImageMask(
            image_mask.rect.size.into(),
            image_mask.image,
            image_mask.repeat,
        )
    }

    pub fn box_shadow(
        shadow_rect: LayoutRect,
        shadow_radius: BorderRadius,
        prim_shadow_rect: LayoutRect,
        blur_radius: f32,
        clip_mode: BoxShadowClipMode,
    ) -> Self {
        // Get the fractional offsets required to match the
        // source rect with a minimal rect.
        let fract_offset = LayoutPoint::new(
            shadow_rect.origin.x.fract().abs(),
            shadow_rect.origin.y.fract().abs(),
        );

        ClipItemKey::BoxShadow(
            fract_offset.into(),
            shadow_rect.size.into(),
            shadow_radius.into(),
            prim_shadow_rect.into(),
            Au::from_f32_px(blur_radius),
            clip_mode,
        )
    }
}

impl intern::InternDebug for ClipItemKey {}

impl intern::Internable for ClipIntern {
    type Key = ClipItemKey;
    type StoreData = ClipNode;
    type InternData = ();
}

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ClipItem {
    Rectangle(LayoutSize, ClipMode),
    RoundedRectangle(LayoutSize, BorderRadius, ClipMode),
    Image {
        image: ImageKey,
        size: LayoutSize,
        repeat: bool,
    },
    BoxShadow(BoxShadowClipSource),
}

fn compute_box_shadow_parameters(
    shadow_rect_fract_offset: LayoutPoint,
    shadow_rect_size: LayoutSize,
    mut shadow_radius: BorderRadius,
    prim_shadow_rect: LayoutRect,
    blur_radius: f32,
    clip_mode: BoxShadowClipMode,
) -> BoxShadowClipSource {
    // Make sure corners don't overlap.
    ensure_no_corner_overlap(&mut shadow_radius, shadow_rect_size);

    let fract_size = LayoutSize::new(
        shadow_rect_size.width.fract().abs(),
        shadow_rect_size.height.fract().abs(),
    );

    // Create a minimal size primitive mask to blur. In this
    // case, we ensure the size of each corner is the same,
    // to simplify the shader logic that stretches the blurred
    // result across the primitive.
    let max_corner_width = shadow_radius.top_left.width
                                .max(shadow_radius.bottom_left.width)
                                .max(shadow_radius.top_right.width)
                                .max(shadow_radius.bottom_right.width);
    let max_corner_height = shadow_radius.top_left.height
                                .max(shadow_radius.bottom_left.height)
                                .max(shadow_radius.top_right.height)
                                .max(shadow_radius.bottom_right.height);

    // Get maximum distance that can be affected by given blur radius.
    let blur_region = (BLUR_SAMPLE_SCALE * blur_radius).ceil();

    // If the largest corner is smaller than the blur radius, we need to ensure
    // that it's big enough that the corners don't affect the middle segments.
    let used_corner_width = max_corner_width.max(blur_region);
    let used_corner_height = max_corner_height.max(blur_region);

    // Minimal nine-patch size, corner + internal + corner.
    let min_shadow_rect_size = LayoutSize::new(
        2.0 * used_corner_width + blur_region,
        2.0 * used_corner_height + blur_region,
    );

    // The minimal rect to blur.
    let mut minimal_shadow_rect = LayoutRect::new(
        LayoutPoint::new(
            blur_region + shadow_rect_fract_offset.x,
            blur_region + shadow_rect_fract_offset.y,
        ),
        LayoutSize::new(
            min_shadow_rect_size.width + fract_size.width,
            min_shadow_rect_size.height + fract_size.height,
        ),
    );

    // If the width or height ends up being bigger than the original
    // primitive shadow rect, just blur the entire rect along that
    // axis and draw that as a simple blit. This is necessary for
    // correctness, since the blur of one corner may affect the blur
    // in another corner.
    let mut stretch_mode_x = BoxShadowStretchMode::Stretch;
    if shadow_rect_size.width < minimal_shadow_rect.size.width {
        minimal_shadow_rect.size.width = shadow_rect_size.width;
        stretch_mode_x = BoxShadowStretchMode::Simple;
    }

    let mut stretch_mode_y = BoxShadowStretchMode::Stretch;
    if shadow_rect_size.height < minimal_shadow_rect.size.height {
        minimal_shadow_rect.size.height = shadow_rect_size.height;
        stretch_mode_y = BoxShadowStretchMode::Simple;
    }

    // Expand the shadow rect by enough room for the blur to take effect.
    let shadow_rect_alloc_size = LayoutSize::new(
        2.0 * blur_region + minimal_shadow_rect.size.width.ceil(),
        2.0 * blur_region + minimal_shadow_rect.size.height.ceil(),
    );

    BoxShadowClipSource {
        original_alloc_size: shadow_rect_alloc_size,
        shadow_rect_alloc_size,
        shadow_radius,
        prim_shadow_rect,
        blur_radius,
        clip_mode,
        stretch_mode_x,
        stretch_mode_y,
        cache_handle: None,
        cache_key: None,
        clip_data_handle: GpuCacheHandle::new(),
        minimal_shadow_rect,
    }
}

impl ClipItem {
    pub fn new_box_shadow(
        shadow_rect_fract_offset: LayoutPoint,
        shadow_rect_size: LayoutSize,
        mut shadow_radius: BorderRadius,
        prim_shadow_rect: LayoutRect,
        blur_radius: f32,
        clip_mode: BoxShadowClipMode,
    ) -> Self {
        let mut source = compute_box_shadow_parameters(
            shadow_rect_fract_offset,
            shadow_rect_size,
            shadow_radius,
            prim_shadow_rect,
            blur_radius,
            clip_mode,
        );

        fn needed_downscaling(source: &BoxShadowClipSource) -> Option<f32> {
            // This size is fairly arbitrary, but it's the same as the size that
            // we use to avoid caching big blurred stacking contexts.
            //
            // If you change it, ensure that the reftests
            // box-shadow-large-blur-radius-* still hit the downscaling path,
            // and that they render correctly.
            const MAX_SIZE: f32 = 2048.;

            let max_dimension =
                source.shadow_rect_alloc_size.width.max(source.shadow_rect_alloc_size.height);

            if max_dimension > MAX_SIZE {
                Some(MAX_SIZE / max_dimension)
            } else {
                None
            }
        }

        if let Some(downscale) = needed_downscaling(&source) {
            shadow_radius.bottom_left.height *= downscale;
            shadow_radius.bottom_left.width *= downscale;
            shadow_radius.bottom_right.height *= downscale;
            shadow_radius.bottom_right.width *= downscale;
            shadow_radius.top_left.height *= downscale;
            shadow_radius.top_left.width *= downscale;
            shadow_radius.top_right.height *= downscale;
            shadow_radius.top_right.width *= downscale;

            let original_alloc_size = source.shadow_rect_alloc_size;

            source = compute_box_shadow_parameters(
                shadow_rect_fract_offset * downscale,
                shadow_rect_size * downscale,
                shadow_radius,
                prim_shadow_rect,
                blur_radius * downscale,
                clip_mode,
            );
            source.original_alloc_size = original_alloc_size;
        }
        ClipItem::BoxShadow(source)
    }

    /// Returns true if this clip mask can run through the fast path
    /// for the given clip item type.
    fn supports_fast_path_rendering(&self) -> bool {
        match *self {
            ClipItem::Rectangle(..) |
            ClipItem::Image { .. } |
            ClipItem::BoxShadow(..) => {
                false
            }
            ClipItem::RoundedRectangle(_, ref radius, _) => {
                // The rounded clip rect fast path shader can only work
                // if the radii are uniform.
                radius.is_uniform().is_some()
            }
        }
    }

    // Get an optional clip rect that a clip source can provide to
    // reduce the size of a primitive region. This is typically
    // used to eliminate redundant clips, and reduce the size of
    // any clip mask that eventually gets drawn.
    pub fn get_local_clip_rect(&self, local_pos: LayoutPoint) -> Option<LayoutRect> {
        let size = match *self {
            ClipItem::Rectangle(size, ClipMode::Clip) => Some(size),
            ClipItem::Rectangle(_, ClipMode::ClipOut) => None,
            ClipItem::RoundedRectangle(size, _, ClipMode::Clip) => Some(size),
            ClipItem::RoundedRectangle(_, _, ClipMode::ClipOut) => None,
            ClipItem::Image { repeat, size, .. } => {
                if repeat {
                    None
                } else {
                    Some(size)
                }
            }
            ClipItem::BoxShadow(..) => None,
        };

        size.map(|size| {
            LayoutRect::new(local_pos, size)
        })
    }

    fn get_clip_result_complex(
        &self,
        local_pos: LayoutPoint,
        transform: &LayoutToWorldTransform,
        prim_world_rect: &WorldRect,
        world_rect: &WorldRect,
    ) -> ClipResult {
        let (clip_rect, inner_rect) = match *self {
            ClipItem::Rectangle(size, ClipMode::Clip) => {
                let clip_rect = LayoutRect::new(local_pos, size);
                (clip_rect, Some(clip_rect))
            }
            ClipItem::RoundedRectangle(size, ref radius, ClipMode::Clip) => {
                let clip_rect = LayoutRect::new(local_pos, size);
                let inner_clip_rect = extract_inner_rect_safe(&clip_rect, radius);
                (clip_rect, inner_clip_rect)
            }
            ClipItem::Rectangle(_, ClipMode::ClipOut) |
            ClipItem::RoundedRectangle(_, _, ClipMode::ClipOut) |
            ClipItem::Image { .. } |
            ClipItem::BoxShadow(..) => {
                return ClipResult::Partial
            }
        };

        let inner_clip_rect = inner_rect.and_then(|ref inner_rect| {
            project_inner_rect(transform, inner_rect)
        });

        if let Some(inner_clip_rect) = inner_clip_rect {
            if inner_clip_rect.contains_rect(prim_world_rect) {
                return ClipResult::Accept;
            }
        }

        let outer_clip_rect = match project_rect(
            transform,
            &clip_rect,
            world_rect,
        ) {
            Some(outer_clip_rect) => outer_clip_rect,
            None => return ClipResult::Partial,
        };

        match outer_clip_rect.intersection(prim_world_rect) {
            Some(..) => {
                ClipResult::Partial
            }
            None => {
                ClipResult::Reject
            }
        }
    }

    // Check how a given clip source affects a local primitive region.
    fn get_clip_result(
        &self,
        local_pos: LayoutPoint,
        prim_rect: &LayoutRect,
    ) -> ClipResult {
        match *self {
            ClipItem::Rectangle(size, ClipMode::Clip) => {
                let clip_rect = LayoutRect::new(local_pos, size);

                if clip_rect.contains_rect(prim_rect) {
                    return ClipResult::Accept;
                }

                match clip_rect.intersection(prim_rect) {
                    Some(..) => {
                        ClipResult::Partial
                    }
                    None => {
                        ClipResult::Reject
                    }
                }
            }
            ClipItem::Rectangle(size, ClipMode::ClipOut) => {
                let clip_rect = LayoutRect::new(local_pos, size);

                if clip_rect.contains_rect(prim_rect) {
                    return ClipResult::Reject;
                }

                match clip_rect.intersection(prim_rect) {
                    Some(_) => {
                        ClipResult::Partial
                    }
                    None => {
                        ClipResult::Accept
                    }
                }
            }
            ClipItem::RoundedRectangle(size, ref radius, ClipMode::Clip) => {
                let clip_rect = LayoutRect::new(local_pos, size);

                // TODO(gw): Consider caching this in the ClipNode
                //           if it ever shows in profiles.
                // TODO(gw): extract_inner_rect_safe is overly
                //           conservative for this code!
                let inner_clip_rect = extract_inner_rect_safe(&clip_rect, radius);
                if let Some(inner_clip_rect) = inner_clip_rect {
                    if inner_clip_rect.contains_rect(prim_rect) {
                        return ClipResult::Accept;
                    }
                }

                match clip_rect.intersection(prim_rect) {
                    Some(..) => {
                        ClipResult::Partial
                    }
                    None => {
                        ClipResult::Reject
                    }
                }
            }
            ClipItem::RoundedRectangle(size, ref radius, ClipMode::ClipOut) => {
                let clip_rect = LayoutRect::new(local_pos, size);

                // TODO(gw): Consider caching this in the ClipNode
                //           if it ever shows in profiles.
                // TODO(gw): extract_inner_rect_safe is overly
                //           conservative for this code!
                let inner_clip_rect = extract_inner_rect_safe(&clip_rect, radius);
                if let Some(inner_clip_rect) = inner_clip_rect {
                    if inner_clip_rect.contains_rect(prim_rect) {
                        return ClipResult::Reject;
                    }
                }

                match clip_rect.intersection(prim_rect) {
                    Some(_) => {
                        ClipResult::Partial
                    }
                    None => {
                        ClipResult::Accept
                    }
                }
            }
            ClipItem::Image { size, repeat, .. } => {
                if repeat {
                    ClipResult::Partial
                } else {
                    let mask_rect = LayoutRect::new(local_pos, size);
                    match mask_rect.intersection(prim_rect) {
                        Some(..) => {
                            ClipResult::Partial
                        }
                        None => {
                            ClipResult::Reject
                        }
                    }
                }
            }
            ClipItem::BoxShadow(..) => {
                ClipResult::Partial
            }
        }
    }
}

/// Represents a local rect and a device space
/// rectangles that are either outside or inside bounds.
#[derive(Clone, Debug, PartialEq)]
pub struct Geometry {
    pub local_rect: LayoutRect,
    pub device_rect: DeviceIntRect,
}

impl From<LayoutRect> for Geometry {
    fn from(local_rect: LayoutRect) -> Self {
        Geometry {
            local_rect,
            device_rect: DeviceIntRect::zero(),
        }
    }
}

pub fn rounded_rectangle_contains_point(
    point: &LayoutPoint,
    rect: &LayoutRect,
    radii: &BorderRadius
) -> bool {
    if !rect.contains(point) {
        return false;
    }

    let top_left_center = rect.origin + radii.top_left.to_vector();
    if top_left_center.x > point.x && top_left_center.y > point.y &&
       !Ellipse::new(radii.top_left).contains(*point - top_left_center.to_vector()) {
        return false;
    }

    let bottom_right_center = rect.bottom_right() - radii.bottom_right.to_vector();
    if bottom_right_center.x < point.x && bottom_right_center.y < point.y &&
       !Ellipse::new(radii.bottom_right).contains(*point - bottom_right_center.to_vector()) {
        return false;
    }

    let top_right_center = rect.top_right() +
                           LayoutVector2D::new(-radii.top_right.width, radii.top_right.height);
    if top_right_center.x < point.x && top_right_center.y > point.y &&
       !Ellipse::new(radii.top_right).contains(*point - top_right_center.to_vector()) {
        return false;
    }

    let bottom_left_center = rect.bottom_left() +
                             LayoutVector2D::new(radii.bottom_left.width, -radii.bottom_left.height);
    if bottom_left_center.x > point.x && bottom_left_center.y < point.y &&
       !Ellipse::new(radii.bottom_left).contains(*point - bottom_left_center.to_vector()) {
        return false;
    }

    true
}

pub fn project_inner_rect(
    transform: &LayoutToWorldTransform,
    rect: &LayoutRect,
) -> Option<WorldRect> {
    let points = [
        transform.transform_point2d(&rect.origin)?,
        transform.transform_point2d(&rect.top_right())?,
        transform.transform_point2d(&rect.bottom_left())?,
        transform.transform_point2d(&rect.bottom_right())?,
    ];

    let mut xs = [points[0].x, points[1].x, points[2].x, points[3].x];
    let mut ys = [points[0].y, points[1].y, points[2].y, points[3].y];
    xs.sort_by(|a, b| a.partial_cmp(b).unwrap_or(cmp::Ordering::Equal));
    ys.sort_by(|a, b| a.partial_cmp(b).unwrap_or(cmp::Ordering::Equal));
    Some(WorldRect::new(
        WorldPoint::new(xs[1], ys[1]),
        WorldSize::new(xs[2] - xs[1], ys[2] - ys[1]),
    ))
}

// Add a clip node into the list of clips to be processed
// for the current clip chain. Returns false if the clip
// results in the entire primitive being culled out.
fn add_clip_node_to_current_chain(
    node: &ClipChainNode,
    spatial_node_index: SpatialNodeIndex,
    local_clip_rect: &mut LayoutRect,
    clip_node_info: &mut Vec<ClipNodeInfo>,
    clip_data_store: &ClipDataStore,
    clip_scroll_tree: &ClipScrollTree,
) -> bool {
    let clip_node = &clip_data_store[node.handle];
    let clip_spatial_node = &clip_scroll_tree.spatial_nodes[node.spatial_node_index.0 as usize];
    let ref_spatial_node = &clip_scroll_tree.spatial_nodes[spatial_node_index.0 as usize];

    // Determine the most efficient way to convert between coordinate
    // systems of the primitive and clip node.
    //Note: this code is different from `get_relative_transform` in a way that we only try
    // getting the relative transform if it's Local or ScaleOffset,
    // falling back to the world transform otherwise.
    let conversion = if spatial_node_index == node.spatial_node_index {
        ClipSpaceConversion::Local
    } else if ref_spatial_node.coordinate_system_id == clip_spatial_node.coordinate_system_id {
        let scale_offset = ref_spatial_node.coordinate_system_relative_scale_offset
            .inverse()
            .accumulate(&clip_spatial_node.coordinate_system_relative_scale_offset);
        ClipSpaceConversion::ScaleOffset(scale_offset)
    } else {
        ClipSpaceConversion::Transform(
            clip_scroll_tree
                .get_world_transform(node.spatial_node_index)
                .into_transform()
        )
    };

    // If we can convert spaces, try to reduce the size of the region
    // requested, and cache the conversion information for the next step.
    if let Some(clip_rect) = clip_node.item.get_local_clip_rect(node.local_pos) {
        match conversion {
            ClipSpaceConversion::Local => {
                *local_clip_rect = match local_clip_rect.intersection(&clip_rect) {
                    Some(rect) => rect,
                    None => return false,
                };
            }
            ClipSpaceConversion::ScaleOffset(ref scale_offset) => {
                let clip_rect = scale_offset.map_rect(&clip_rect);
                *local_clip_rect = match local_clip_rect.intersection(&clip_rect) {
                    Some(rect) => rect,
                    None => return false,
                };
            }
            ClipSpaceConversion::Transform(..) => {
                // TODO(gw): In the future, we can reduce the size
                //           of the pic_clip_rect here. To do this,
                //           we can use project_rect or the
                //           inverse_rect_footprint method, depending
                //           on the relationship of the clip, pic
                //           and primitive spatial nodes.
                //           I have left this for now until we
                //           find some good test cases where this
                //           would be a worthwhile perf win.
            }
        }
    }

    clip_node_info.push(ClipNodeInfo {
        conversion,
        local_pos: node.local_pos,
        handle: node.handle,
        spatial_node_index: node.spatial_node_index,
    });

    true
}

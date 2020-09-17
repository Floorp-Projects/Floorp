/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, PrimitiveFlags, RasterSpace, QualitySettings};
use api::units::*;
use crate::clip::{ClipChainId, ClipNodeKind, ClipStore, ClipInstance};
use crate::frame_builder::FrameBuilderConfig;
use crate::internal_types::{FastHashMap, FastHashSet};
use crate::picture::{PrimitiveList, PictureCompositeMode, PictureOptions, PicturePrimitive, SliceId};
use crate::picture::{Picture3DContext, TileCacheParams};
use crate::prim_store::{PrimitiveInstance, PrimitiveInstanceKind, PrimitiveStore, PictureIndex, SegmentInstanceIndex};
use crate::prim_store::picture::{Picture, PictureKey, PictureCompositeKey};
use crate::scene_building::SliceFlags;
use crate::scene_builder_thread::Interners;
use crate::spatial_tree::{ROOT_SPATIAL_NODE_INDEX, SpatialNodeIndex, SpatialTree};
use crate::util::{MaxRect, VecHelper};

/*
 Types and functionality related to picture caching. In future, we'll
 move more and more of the existing functionality out of picture.rs
 and into here.
 */

/// Created during scene building, describes how to create a tile cache for a given slice.
pub struct PendingTileCache {
    /// List of primitives that are part of this slice
    pub prim_list: PrimitiveList,
    /// Parameters that define the tile cache (such as background color, shared clips, reference spatial node)
    pub params: TileCacheParams,
}

/// Used during scene building to construct the list of pending tile caches.
pub struct TileCacheBuilder {
    /// When true, a new tile cache will be created for the next primitive.
    need_new_tile_cache: bool,
    /// List of tile caches that have been created so far (last in the list is currently active).
    pending_tile_caches: Vec<PendingTileCache>,

    /// Cache the previous scroll root search for a spatial node, since they are often the same.
    prev_scroll_root_cache: (SpatialNodeIndex, SpatialNodeIndex),
    /// A buffer for collecting clips for a clip-chain. Retained here to avoid memory allocations in add_prim.
    prim_clips_buffer: Vec<ClipInstance>,
    /// Cache the last clip-chain that was added to the shared clips as it's often the same between prims.
    last_checked_clip_chain: ClipChainId,
}

/// The output of a tile cache builder, containing all details needed to construct the
/// tile cache(s) for the next scene, and retain tiles from the previous frame when sent
/// send to the frame builder.
pub struct TileCacheConfig {
    /// Mapping of slice id to the parameters needed to construct this tile cache.
    pub tile_caches: FastHashMap<SliceId, TileCacheParams>,
    /// A set of any spatial nodes that are attached to either a picture cache
    /// root, or a clip node on the picture cache primitive. These are used
    /// to detect cases where picture caching must be disabled. This is mostly
    /// a temporary workaround for some existing wrench tests. I don't think
    /// Gecko ever produces picture cache slices with complex transforms, so
    /// in future we should prevent this in the public API and remove this hack.
    pub picture_cache_spatial_nodes: FastHashSet<SpatialNodeIndex>,
    /// Number of picture cache slices that were created (for profiler)
    pub picture_cache_slice_count: usize,
}

impl TileCacheConfig {
    pub fn new(picture_cache_slice_count: usize) -> Self {
        TileCacheConfig {
            tile_caches: FastHashMap::default(),
            picture_cache_spatial_nodes: FastHashSet::default(),
            picture_cache_slice_count,
        }
    }
}

impl TileCacheBuilder {
    /// Construct a new tile cache builder.
    pub fn new() -> Self {
        TileCacheBuilder {
            need_new_tile_cache: true,
            pending_tile_caches: Vec::new(),
            prev_scroll_root_cache: (ROOT_SPATIAL_NODE_INDEX, ROOT_SPATIAL_NODE_INDEX),
            prim_clips_buffer: Vec::new(),
            last_checked_clip_chain: ClipChainId::INVALID,
        }
    }

    /// Set a barrier that forces a new tile cache next time a prim is added.
    pub fn add_tile_cache_barrier(&mut self) {
        self.need_new_tile_cache = true;
    }

    /// Add a primitive, either to the current tile cache, or a new one, depending on various conditions.
    pub fn add_prim(
        &mut self,
        prim_instance: PrimitiveInstance,
        prim_rect: LayoutRect,
        spatial_node_index: SpatialNodeIndex,
        prim_flags: PrimitiveFlags,
        spatial_tree: &SpatialTree,
        clip_store: &ClipStore,
        interners: &Interners,
        config: &FrameBuilderConfig,
        quality_settings: &QualitySettings,
    ) {
        // Scrollbars and clear primitives always get their own slice
        let is_scrollbar_container = prim_flags.contains(PrimitiveFlags::IS_SCROLLBAR_CONTAINER);
        let is_clear_prim = match prim_instance.kind {
            PrimitiveInstanceKind::Clear { .. } => true,
            _ => false,
        };
        let requires_own_slice = is_scrollbar_container || is_clear_prim;

        // Also create a new slice if there was a barrier previously set
        let mut want_new_tile_cache = self.need_new_tile_cache || requires_own_slice;

        // Check if we want to create a new slice based on the current / next scroll root
        let scroll_root = self.find_scroll_root(spatial_node_index, spatial_tree);
        let current_scroll_root = self.pending_tile_caches
            .last()
            .map(|p| p.params.spatial_node_index)
            .unwrap_or(ROOT_SPATIAL_NODE_INDEX);

        want_new_tile_cache |= match (current_scroll_root, scroll_root) {
            (ROOT_SPATIAL_NODE_INDEX, ROOT_SPATIAL_NODE_INDEX) => {
                // Both current slice and this cluster are fixed position, no need to cut
                false
            }
            (ROOT_SPATIAL_NODE_INDEX, _) => {
                // A real scroll root is being established, so create a cache slice
                true
            }
            (_, ROOT_SPATIAL_NODE_INDEX) => {
                // If quality settings force subpixel AA over performance, skip creating
                // a slice for the fixed position element(s) here.
                if quality_settings.force_subpixel_aa_where_possible {
                    false
                } else {
                    // A fixed position slice is encountered within a scroll root. Only create
                    // a slice in this case if all the clips referenced by this cluster are also
                    // fixed position. There's no real point in creating slices for these cases,
                    // since we'll have to rasterize them as the scrolling clip moves anyway. It
                    // also allows us to retain subpixel AA in these cases. For these types of
                    // slices, the intra-slice dirty rect handling typically works quite well
                    // (a common case is parallax scrolling effects).
                    let mut create_slice = true;
                    let mut current_clip_chain_id = prim_instance.clip_set.clip_chain_id;

                    while current_clip_chain_id != ClipChainId::NONE {
                        let clip_chain_node = &clip_store.clip_chain_nodes[current_clip_chain_id.0 as usize];
                        let spatial_root = self.find_scroll_root(clip_chain_node.spatial_node_index, spatial_tree);
                        if spatial_root != ROOT_SPATIAL_NODE_INDEX {
                            create_slice = false;
                            break;
                        }
                        current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
                    }

                    create_slice
                }
            }
            (curr_scroll_root, scroll_root) => {
                // Two scrolling roots - only need a new slice if they differ
                curr_scroll_root != scroll_root
            }
        };

        if want_new_tile_cache {
            // If the page would create too many slices (an arbitrary definition where
            // it's assumed the GPU memory + compositing overhead would be too high)
            // then create a single picture cache for the remaining content. This at
            // least means that we can cache small content changes efficiently when
            // scrolling isn't occurring. Scrolling regions will be handled reasonably
            // efficiently by the dirty rect tracking (since it's likely that if the
            // page has so many slices there isn't a single major scroll region).
            const MAX_CACHE_SLICES: usize = 12;
            let slice = self.pending_tile_caches.len();

            // If we have exceeded the maximum number of slices, skip creating a new
            // one and the primitive will be added to the last slice.
            if slice < MAX_CACHE_SLICES {
                // When we reach the last valid slice that can be created, it is created as
                // a fixed slice without shared clips, ensuring that we can safely add any
                // subsequent primitives to it. This doesn't seem to occur on any real
                // world content (only contrived test cases), where this acts as a fail safe
                // to ensure we don't allocate too much GPU memory for surface caches.
                // However, if we _do_ ever see this occur on real world content, we could
                // probably consider increasing the max cache slices a bit more than the
                // current limit.
                let params = if slice == MAX_CACHE_SLICES-1 {
                    TileCacheParams {
                        slice,
                        slice_flags: SliceFlags::empty(),
                        spatial_node_index: ROOT_SPATIAL_NODE_INDEX,
                        background_color: None,
                        shared_clips: Vec::new(),
                        shared_clip_chain: ClipChainId::NONE,
                        virtual_surface_size: config.compositor_kind.get_virtual_surface_size(),
                    }
                } else {
                    let slice_flags = if is_scrollbar_container {
                        SliceFlags::IS_SCROLLBAR
                    } else {
                        SliceFlags::empty()
                    };

                    let background_color = if slice == 0 {
                        config.background_color
                    } else {
                        None
                    };

                    let mut shared_clips = Vec::new();
                    add_clips(
                        scroll_root,
                        prim_instance.clip_set.clip_chain_id,
                        &mut shared_clips,
                        clip_store,
                        interners,
                        spatial_tree,
                    );

                    self.last_checked_clip_chain = prim_instance.clip_set.clip_chain_id;

                    TileCacheParams {
                        slice,
                        slice_flags,
                        spatial_node_index: scroll_root,
                        background_color,
                        shared_clips,
                        shared_clip_chain: ClipChainId::NONE,
                        virtual_surface_size: config.compositor_kind.get_virtual_surface_size(),
                    }
                };

                self.pending_tile_caches.push(PendingTileCache {
                    prim_list: PrimitiveList::empty(),
                    params,
                });

                self.need_new_tile_cache = requires_own_slice;
            }
        }

        let pending_tile_cache = self.pending_tile_caches.last_mut().unwrap();

        // Update the list of clips that apply to this primitive instance, to track which are the
        // shared clips for this tile cache that can be applied during compositing.
        if self.last_checked_clip_chain != prim_instance.clip_set.clip_chain_id {
            let prim_clips_buffer = &mut self.prim_clips_buffer;
            prim_clips_buffer.clear();
            add_clips(
                pending_tile_cache.params.spatial_node_index,
                prim_instance.clip_set.clip_chain_id,
                prim_clips_buffer,
                clip_store,
                interners,
                spatial_tree,
            );

            pending_tile_cache.params.shared_clips.retain(|h1: &ClipInstance| {
                let uid = h1.handle.uid();
                prim_clips_buffer.iter().any(|h2| {
                    uid == h2.handle.uid() &&
                    h1.spatial_node_index == h2.spatial_node_index
                })
            });

            self.last_checked_clip_chain = prim_instance.clip_set.clip_chain_id;
        }

        pending_tile_cache.prim_list.add_prim(
            prim_instance,
            prim_rect,
            spatial_node_index,
            prim_flags,
        );
    }

    /// Consume this object and build the list of tile cache primitives
    pub fn build(
        self,
        config: &FrameBuilderConfig,
        interners: &mut Interners,
        clip_store: &mut ClipStore,
        prim_store: &mut PrimitiveStore,
    ) -> (TileCacheConfig, PrimitiveList) {
        let mut result = TileCacheConfig::new(self.pending_tile_caches.len());
        let mut root_prim_list = PrimitiveList::empty();

        for pending_tile_cache in self.pending_tile_caches {
            let prim_instance = create_tile_cache(
                pending_tile_cache.params.slice,
                pending_tile_cache.params.slice_flags,
                pending_tile_cache.params.spatial_node_index,
                pending_tile_cache.prim_list,
                pending_tile_cache.params.background_color,
                pending_tile_cache.params.shared_clips,
                interners,
                prim_store,
                clip_store,
                &mut result.picture_cache_spatial_nodes,
                config,
                &mut result.tile_caches,
            );

            root_prim_list.add_prim(
                prim_instance,
                LayoutRect::zero(),
                pending_tile_cache.params.spatial_node_index,
                PrimitiveFlags::IS_BACKFACE_VISIBLE,
            );
        }

        (result, root_prim_list)
    }

    /// Find the scroll root for a given spatial node
    fn find_scroll_root(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        spatial_tree: &SpatialTree,
    ) -> SpatialNodeIndex {
        if self.prev_scroll_root_cache.0 == spatial_node_index {
            return self.prev_scroll_root_cache.1;
        }

        let scroll_root = spatial_tree.find_scroll_root(spatial_node_index);
        self.prev_scroll_root_cache = (spatial_node_index, scroll_root);

        scroll_root
    }
}

// Helper fn to collect clip handles from a given clip chain.
fn add_clips(
    _scroll_root: SpatialNodeIndex,
    clip_chain_id: ClipChainId,
    prim_clips: &mut Vec<ClipInstance>,
    clip_store: &ClipStore,
    interners: &Interners,
    _spatial_tree: &SpatialTree,
) {
    let mut current_clip_chain_id = clip_chain_id;

    while current_clip_chain_id != ClipChainId::NONE {
        let clip_chain_node = &clip_store
            .clip_chain_nodes[current_clip_chain_id.0 as usize];

        let clip_node_data = &interners.clip[clip_chain_node.handle];
        if let ClipNodeKind::Rectangle = clip_node_data.clip_node_kind {
            // TODO(gw): Disable this check temporarily as it causes a regression
            //           to scrollbar drawing on mac, while I figure out the root
            //           cause of the issue.
            // if spatial_tree.is_ancestor(
            //     clip_chain_node.spatial_node_index,
            //     scroll_root,
            // ) {
                prim_clips.push(ClipInstance::new(clip_chain_node.handle, clip_chain_node.spatial_node_index));
            // }
        }

        current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
    }
}

/// Given a PrimitiveList and scroll root, construct a tile cache primitive instance
/// that wraps the primitive list.
fn create_tile_cache(
    slice: usize,
    slice_flags: SliceFlags,
    scroll_root: SpatialNodeIndex,
    prim_list: PrimitiveList,
    background_color: Option<ColorF>,
    shared_clips: Vec<ClipInstance>,
    interners: &mut Interners,
    prim_store: &mut PrimitiveStore,
    clip_store: &mut ClipStore,
    picture_cache_spatial_nodes: &mut FastHashSet<SpatialNodeIndex>,
    frame_builder_config: &FrameBuilderConfig,
    tile_caches: &mut FastHashMap<SliceId, TileCacheParams>,
) -> PrimitiveInstance {
    // Add this spatial node to the list to check for complex transforms
    // at the start of a frame build.
    picture_cache_spatial_nodes.insert(scroll_root);

    // Now, create a picture with tile caching enabled that will hold all
    // of the primitives selected as belonging to the main scroll root.
    let pic_key = PictureKey::new(
        Picture {
            composite_mode_key: PictureCompositeKey::Identity,
        },
    );

    let pic_data_handle = interners
        .picture
        .intern(&pic_key, || ());

    // Build a clip-chain for the tile cache, that contains any of the shared clips
    // we will apply when drawing the tiles. In all cases provided by Gecko, these
    // are rectangle clips with a scale/offset transform only, and get handled as
    // a simple local clip rect in the vertex shader. However, this should in theory
    // also work with any complex clips, such as rounded rects and image masks, by
    // producing a clip mask that is applied to the picture cache tiles.
    let mut parent_clip_chain_id = ClipChainId::NONE;
    for clip_instance in &shared_clips {
        // Add this spatial node to the list to check for complex transforms
        // at the start of a frame build.
        picture_cache_spatial_nodes.insert(clip_instance.spatial_node_index);

        parent_clip_chain_id = clip_store.add_clip_chain_node(
            clip_instance.handle,
            clip_instance.spatial_node_index,
            parent_clip_chain_id,
        );
    }

    let slice_id = SliceId::new(slice);

    // Store some information about the picture cache slice. This is used when we swap the
    // new scene into the frame builder to either reuse existing slices, or create new ones.
    tile_caches.insert(slice_id, TileCacheParams {
        slice,
        slice_flags,
        spatial_node_index: scroll_root,
        background_color,
        shared_clips,
        shared_clip_chain: parent_clip_chain_id,
        virtual_surface_size: frame_builder_config.compositor_kind.get_virtual_surface_size(),
    });

    let pic_index = prim_store.pictures.alloc().init(PicturePrimitive::new_image(
        Some(PictureCompositeMode::TileCache { slice_id }),
        Picture3DContext::Out,
        true,
        PrimitiveFlags::IS_BACKFACE_VISIBLE,
        RasterSpace::Screen,
        prim_list,
        scroll_root,
        PictureOptions::default(),
    ));

    PrimitiveInstance::new(
        LayoutRect::max_rect(),
        PrimitiveInstanceKind::Picture {
            data_handle: pic_data_handle,
            pic_index: PictureIndex(pic_index),
            segment_instance_index: SegmentInstanceIndex::INVALID,
        },
        parent_clip_chain_id,
    )
}

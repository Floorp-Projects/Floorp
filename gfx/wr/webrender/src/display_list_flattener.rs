/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderDetails, BorderDisplayItem, BuiltDisplayListIter};
use api::{ClipId, ColorF, CommonItemProperties, ComplexClipRegion, ComponentTransferFuncType, RasterSpace};
use api::{DisplayItem, DisplayItemRef, ExtendMode, ExternalScrollId, FilterData};
use api::{FilterOp, FilterPrimitive, FontInstanceKey, GlyphInstance, GlyphOptions, GradientStop};
use api::{IframeDisplayItem, ImageKey, ImageRendering, ItemRange, ColorDepth};
use api::{LineOrientation, LineStyle, NinePatchBorderSource, PipelineId, MixBlendMode};
use api::{PropertyBinding, ReferenceFrame, ReferenceFrameKind, ScrollFrameDisplayItem, ScrollSensitivity};
use api::{Shadow, SpaceAndClipInfo, SpatialId, StackingContext, StickyFrameDisplayItem};
use api::{ClipMode, PrimitiveKeyKind, TransformStyle, YuvColorSpace, ColorRange, YuvData, TempFilterData};
use api::units::*;
use crate::clip::{ClipChainId, ClipRegion, ClipItemKey, ClipStore, ClipItemKeyKind, ClipDataHandle, ClipNodeKind};
use crate::clip_scroll_tree::{ROOT_SPATIAL_NODE_INDEX, ClipScrollTree, SpatialNodeIndex};
use crate::frame_builder::{ChasePrimitive, FrameBuilder, FrameBuilderConfig};
use crate::glyph_rasterizer::FontInstance;
use crate::hit_test::{HitTestingItem, HitTestingScene};
use crate::image::simplify_repeated_primitive;
use crate::intern::Interner;
use crate::internal_types::{FastHashMap, FastHashSet, LayoutPrimitiveInfo, Filter};
use crate::picture::{Picture3DContext, PictureCompositeMode, PicturePrimitive, PictureOptions};
use crate::picture::{BlitReason, OrderedPictureChild, PrimitiveList, TileCacheInstance};
use crate::prim_store::{PrimitiveInstance, PrimitiveSceneData};
use crate::prim_store::{PrimitiveInstanceKind, NinePatchDescriptor, PrimitiveStore};
use crate::prim_store::{ScrollNodeAndClipChain, PictureIndex};
use crate::prim_store::{InternablePrimitive, SegmentInstanceIndex};
use crate::prim_store::{register_prim_chase_id, get_line_decoration_sizes};
use crate::prim_store::backdrop::Backdrop;
use crate::prim_store::borders::{ImageBorder, NormalBorderPrim};
use crate::prim_store::gradient::{GradientStopKey, LinearGradient, RadialGradient, RadialGradientParams};
use crate::prim_store::image::{Image, YuvImage};
use crate::prim_store::line_dec::{LineDecoration, LineDecorationCacheKey};
use crate::prim_store::picture::{Picture, PictureCompositeKey, PictureKey};
use crate::prim_store::text_run::TextRun;
use crate::render_backend::{DocumentView};
use crate::resource_cache::{FontInstanceMap, ImageRequest};
use crate::scene::{Scene, StackingContextHelpers};
use crate::scene_builder::{DocumentStats, Interners};
use crate::spatial_node::{StickyFrameInfo, ScrollFrameKind};
use std::{f32, mem, usize, ops};
use std::collections::vec_deque::VecDeque;
use std::sync::Arc;
use crate::util::{MaxRect, VecHelper};
use crate::filterdata::{SFilterDataComponent, SFilterData, SFilterDataKey};

#[derive(Debug, Copy, Clone)]
struct ClipNode {
    id: ClipChainId,
    count: usize,
}



impl ClipNode {
    fn new(id: ClipChainId, count: usize) -> Self {
        ClipNode {
            id,
            count,
        }
    }
}

/// The offset stack for a given reference frame.
struct ReferenceFrameState {
    /// A stack of current offsets from the current reference frame scope.
    offsets: Vec<LayoutVector2D>,
}

/// Maps from stacking context layout coordinates into reference frame
/// relative coordinates.
struct ReferenceFrameMapper {
    /// A stack of reference frame scopes.
    frames: Vec<ReferenceFrameState>,
}

impl ReferenceFrameMapper {
    fn new() -> Self {
        ReferenceFrameMapper {
            frames: vec![
                ReferenceFrameState {
                    offsets: vec![
                        LayoutVector2D::zero(),
                    ],
                }
            ],
        }
    }

    /// Push a new scope. This resets the current offset to zero, and is
    /// used when a new reference frame or iframe is pushed.
    fn push_scope(&mut self) {
        self.frames.push(ReferenceFrameState {
            offsets: vec![
                LayoutVector2D::zero(),
            ],
        });
    }

    /// Pop a reference frame scope off the stack.
    fn pop_scope(&mut self) {
        self.frames.pop().unwrap();
    }

    /// Push a new offset for the current scope. This is used when
    /// a new stacking context is pushed.
    fn push_offset(&mut self, offset: LayoutVector2D) {
        let frame = self.frames.last_mut().unwrap();
        let current_offset = *frame.offsets.last().unwrap();
        frame.offsets.push(current_offset + offset);
    }

    /// Pop a local stacking context offset from the current scope.
    fn pop_offset(&mut self) {
        let frame = self.frames.last_mut().unwrap();
        frame.offsets.pop().unwrap();
    }

    /// Retrieve the current offset to allow converting a stacking context
    /// relative coordinate to be relative to the owing reference frame.
    /// TODO(gw): We could perhaps have separate coordinate spaces for this,
    ///           however that's going to either mean a lot of changes to
    ///           public API code, or a lot of changes to internal code.
    ///           Before doing that, we should revisit how Gecko would
    ///           prefer to provide coordinates.
    /// TODO(gw): For now, this includes only the reference frame relative
    ///           offset. Soon, we will expand this to include the initial
    ///           scroll offsets that are now available on scroll nodes. This
    ///           will allow normalizing the coordinates even between display
    ///           lists where APZ has scrolled the content.
    fn current_offset(&self) -> LayoutVector2D {
        *self.frames.last().unwrap().offsets.last().unwrap()
    }
}

/// Offsets primitives (and clips) by the external scroll offset
/// supplied to scroll nodes.
pub struct ScrollOffsetMapper {
    pub current_spatial_node: SpatialNodeIndex,
    pub current_offset: LayoutVector2D,
}

impl ScrollOffsetMapper {
    fn new() -> Self {
        ScrollOffsetMapper {
            current_spatial_node: SpatialNodeIndex::INVALID,
            current_offset: LayoutVector2D::zero(),
        }
    }

    /// Return the accumulated external scroll offset for a spatial
    /// node. This caches the last result, which is the common case,
    /// or defers to the clip scroll tree to build the value.
    fn external_scroll_offset(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) -> LayoutVector2D {
        if spatial_node_index != self.current_spatial_node {
            self.current_spatial_node = spatial_node_index;
            self.current_offset = clip_scroll_tree.external_scroll_offset(spatial_node_index);
        }

        self.current_offset
    }
}

/// A data structure that keeps track of mapping between API Ids for clips/spatials and the indices
/// used internally in the ClipScrollTree to avoid having to do HashMap lookups. NodeIdToIndexMapper
/// is responsible for mapping both ClipId to ClipChainIndex and SpatialId to SpatialNodeIndex.
#[derive(Default)]
pub struct NodeIdToIndexMapper {
    clip_node_map: FastHashMap<ClipId, ClipNode>,
    spatial_node_map: FastHashMap<SpatialId, SpatialNodeIndex>,
}

impl NodeIdToIndexMapper {
    pub fn add_clip_chain(
        &mut self,
        id: ClipId,
        index: ClipChainId,
        count: usize,
    ) {
        let _old_value = self.clip_node_map.insert(id, ClipNode::new(index, count));
        debug_assert!(_old_value.is_none());
    }

    pub fn map_spatial_node(&mut self, id: SpatialId, index: SpatialNodeIndex) {
        let _old_value = self.spatial_node_map.insert(id, index);
        debug_assert!(_old_value.is_none());
    }

    fn get_clip_node(&self, id: &ClipId) -> ClipNode {
        self.clip_node_map[id]
    }

    pub fn get_clip_chain_id(&self, id: ClipId) -> ClipChainId {
        self.clip_node_map[&id].id
    }

    pub fn get_spatial_node_index(&self, id: SpatialId) -> SpatialNodeIndex {
        self.spatial_node_map[&id]
    }
}

#[derive(Debug, Clone, Default)]
pub struct CompositeOps {
    // Requires only a single texture as input (e.g. most filters)
    pub filters: Vec<Filter>,
    pub filter_datas: Vec<FilterData>,
    pub filter_primitives: Vec<FilterPrimitive>,

    // Requires two source textures (e.g. mix-blend-mode)
    pub mix_blend_mode: Option<MixBlendMode>,
}

impl CompositeOps {
    pub fn new(
        filters: Vec<Filter>,
        filter_datas: Vec<FilterData>,
        filter_primitives: Vec<FilterPrimitive>,
        mix_blend_mode: Option<MixBlendMode>
    ) -> Self {
        CompositeOps {
            filters,
            filter_datas,
            filter_primitives,
            mix_blend_mode,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.filters.is_empty() &&
            self.filter_primitives.is_empty() &&
            self.mix_blend_mode.is_none()
    }
}

/// A structure that converts a serialized display list into a form that WebRender
/// can use to later build a frame. This structure produces a FrameBuilder. Public
/// members are typically those that are destructured into the FrameBuilder.
pub struct DisplayListFlattener<'a> {
    /// The scene that we are currently flattening.
    scene: &'a Scene,

    /// The ClipScrollTree that we are currently building during flattening.
    clip_scroll_tree: &'a mut ClipScrollTree,

    /// The map of all font instances.
    font_instances: FontInstanceMap,

    /// A set of pipelines that the caller has requested be made available as
    /// output textures.
    output_pipelines: &'a FastHashSet<PipelineId>,

    /// The data structure that converting between ClipId/SpatialId and the various
    /// index types that the ClipScrollTree uses.
    id_to_index_mapper: NodeIdToIndexMapper,

    /// A stack of stacking context properties.
    sc_stack: Vec<FlattenedStackingContext>,

    /// Maintains state for any currently active shadows
    pending_shadow_items: VecDeque<ShadowItem>,

    /// The stack keeping track of the root clip chains associated with pipelines.
    pipeline_clip_chain_stack: Vec<ClipChainId>,

    /// The store of primitives.
    pub prim_store: PrimitiveStore,

    /// Information about all primitives involved in hit testing.
    pub hit_testing_scene: HitTestingScene,

    /// The store which holds all complex clipping information.
    pub clip_store: ClipStore,

    /// The configuration to use for the FrameBuilder. We consult this in
    /// order to determine the default font.
    pub config: FrameBuilderConfig,

    /// Reference to the set of data that is interned across display lists.
    interners: &'a mut Interners,

    /// The root picture index for this flattener. This is the picture
    /// to start the culling phase from.
    pub root_pic_index: PictureIndex,

    /// Helper struct to map stacking context coords <-> reference frame coords.
    rf_mapper: ReferenceFrameMapper,

    /// Helper struct to map spatial nodes to external scroll offsets.
    external_scroll_mapper: ScrollOffsetMapper,

    /// If true, a stacking context with create_tile_cache set to true was found
    /// during flattening.
    found_explicit_tile_cache: bool,
}

impl<'a> DisplayListFlattener<'a> {
    pub fn create_frame_builder(
        scene: &Scene,
        clip_scroll_tree: &mut ClipScrollTree,
        font_instances: FontInstanceMap,
        view: &DocumentView,
        output_pipelines: &FastHashSet<PipelineId>,
        frame_builder_config: &FrameBuilderConfig,
        new_scene: &mut Scene,
        interners: &mut Interners,
        doc_stats: &DocumentStats,
    ) -> FrameBuilder {
        // We checked that the root pipeline is available on the render backend.
        let root_pipeline_id = scene.root_pipeline_id.unwrap();
        let root_pipeline = scene.pipelines.get(&root_pipeline_id).unwrap();

        let background_color = root_pipeline
            .background_color
            .and_then(|color| if color.a > 0.0 { Some(color) } else { None });

        let mut flattener = DisplayListFlattener {
            scene,
            clip_scroll_tree,
            font_instances,
            config: *frame_builder_config,
            output_pipelines,
            id_to_index_mapper: NodeIdToIndexMapper::default(),
            hit_testing_scene: HitTestingScene::new(&doc_stats.hit_test_stats),
            pending_shadow_items: VecDeque::new(),
            sc_stack: Vec::new(),
            pipeline_clip_chain_stack: vec![ClipChainId::NONE],
            prim_store: PrimitiveStore::new(&doc_stats.prim_store_stats),
            clip_store: ClipStore::new(),
            interners,
            root_pic_index: PictureIndex(0),
            rf_mapper: ReferenceFrameMapper::new(),
            external_scroll_mapper: ScrollOffsetMapper::new(),
            found_explicit_tile_cache: false,
        };

        flattener.push_root(
            root_pipeline_id,
            &root_pipeline.viewport_size,
            &root_pipeline.content_size,
        );

        // In order to ensure we have a single root stacking context for the
        // entire display list, we push one here. Gecko _almost_ wraps its
        // entire display list within a single stacking context, but sometimes
        // appends a few extra items in AddWindowOverlayWebRenderCommands. We
        // could fix it there, but it's easier and more robust for WebRender
        // to just ensure there's a context on the stack whenever we append
        // primitives (since otherwise we'd panic).
        //
        // Note that we don't do this for iframes, even if they're pipeline
        // roots, because they should be entirely contained within a stacking
        // context, and we probably wouldn't crash if they weren't.
        flattener.push_stacking_context(
            root_pipeline.pipeline_id,
            CompositeOps::default(),
            TransformStyle::Flat,
            /* is_backface_visible = */ true,
            /* create_tile_cache = */ false,
            ROOT_SPATIAL_NODE_INDEX,
            ClipChainId::NONE,
            RasterSpace::Screen,
            /* is_backdrop_root = */ true,
        );

        flattener.flatten_items(
            &mut root_pipeline.display_list.iter(),
            root_pipeline.pipeline_id,
            true,
        );

        flattener.pop_stacking_context();

        debug_assert!(flattener.sc_stack.is_empty());

        new_scene.root_pipeline_id = Some(root_pipeline_id);
        new_scene.pipeline_epochs = scene.pipeline_epochs.clone();
        new_scene.pipelines = scene.pipelines.clone();

        FrameBuilder::with_display_list_flattener(
            view.device_rect.size.into(),
            background_color,
            flattener,
        )
    }

    /// Retrieve the current offset to allow converting a stacking context
    /// relative coordinate to be relative to the owing reference frame,
    /// also considering any external scroll offset on the provided
    /// spatial node.
    fn current_offset(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
    ) -> LayoutVector2D {
        // Get the current offset from stacking context <-> reference frame space.
        let rf_offset = self.rf_mapper.current_offset();

        // Get the external scroll offset, if applicable.
        let scroll_offset = self
            .external_scroll_mapper
            .external_scroll_offset(
                spatial_node_index,
                &self.clip_scroll_tree,
            );

        rf_offset + scroll_offset
    }

    fn setup_picture_caching(
        &mut self,
        primitives: &mut Vec<PrimitiveInstance>,
    ) {
        if !self.config.enable_picture_caching {
            return;
        }

        // This method is basically a hack to set up picture caching in a minimal
        // way without having to check the public API (yet). The intent is to
        // work out a good API for this and switch to using it. In the mean
        // time, this allows basic picture caching to be enabled and used for
        // ironing out remaining bugs, fixing performance issues and profiling.

        //
        // We know that the display list will contain something like the following:
        //  [Some number of primitives attached to root scroll now]
        //  [IFrame for the content]
        //  [A scroll root for the content (what we're interested in)]
        //  [Primitives attached to the scroll root, possibly with sub-scroll roots]
        //  [Some number of trailing primitives attached to root scroll frame]
        //
        // So we want to slice that stacking context up into:
        //  [root primitives]
        //  [tile cache picture]
        //     [primitives attached to cached scroll root]
        //  [trailing root primitives]
        //
        // This step is typically very quick, because there are only
        // a small number of items in the root stacking context, since
        // most of the content is embedded in its own picture.
        //

        // Find the first primitive which has the desired scroll root.
        let mut first_index = None;
        let mut main_scroll_root = None;

        // If the main primitive list that we split for picture caching has
        // clip chain instances at the top level, it's possible that we will
        // create a split resulting in one list having unmatched PushClipChain
        // or PopClipChain instances. This causes panics in pop_surface(),
        // which asserts that the clip chain instances are matched.

        // The code below is a (very inelegant) solution to that. It records
        // clip chain instances found during the initial scan for scroll roots.
        // Later, it uses this information to fix up unopened or unclosed clip
        // instances on the split primitive lists.

        // This is far from ideal - but it fixes the crash for now. In future
        // we will be simplifying and/or removing how clip chain instances
        // and picture cache splitting works, so we can tidy this up as
        // part of those future changes.

        let mut clip_chain_instances = Vec::new();
        let mut clip_chain_instance_stack = Vec::new();

        // Maintain a list of clip node handles that are shared by every primitive
        // in this list. These are often root / fixed position clips. We collect these
        // and handle them when compositing the picture cache tiles. This saves per-item
        // clip work, but more importantly, it avoids lots of invalidations due to
        // content being clipped by fixed iframe/scrollframe rects.

        // TODO(gw): The logic to build the shared clips list is quite complicated, because:
        //           (a) We want to cache result and not do a heap of extra work per primitive,
        //               since stress tests like dl_mutate have 50000+ primitives. Since each
        //               primitive often shares the same clip chain as the previous primitive,
        //               caching the last set of prim clips is generally sufficient.
        //           (b) The general logic to set up picture caching is still complicated due to
        //               not caching multiple slices (e.g. scroll bars). Once we enable
        //               multiple picture cache slices, this logic becomes much simpler.

        // The clips we have found that exist on every primitive we are going to cache.
        let mut shared_clips = Vec::new();
        // The clips found the last time we traversed a set of clip chains. Stored and cleared
        // here to avoid constant allocations.
        let mut prim_clips = Vec::new();
        // If true, the cache is out of date and needs to be rebuilt.
        let mut update_shared_clips = true;
        // The last prim clip chain we build prim_clips for.
        let mut last_prim_clip_chain_id = ClipChainId::NONE;

        /// Records the indices in the list of a push/pop clip chain instance pair.
        #[derive(Debug)]
        struct ClipChainPairInfo {
            push_index: usize,
            pop_index: usize,
            spatial_node_index: SpatialNodeIndex,
            clip_chain_id: ClipChainId,
        }

        // Helper fn to collect clip handles from a given clip chain.
        fn add_clips(
            clip_chain_id: ClipChainId,
            prim_clips: &mut Vec<ClipDataHandle>,
            clip_store: &ClipStore,
        ) {
            let mut current_clip_chain_id = clip_chain_id;

            while current_clip_chain_id != ClipChainId::NONE {
                let clip_chain_node = &clip_store
                    .clip_chain_nodes[current_clip_chain_id.0 as usize];

                prim_clips.push(clip_chain_node.handle);

                current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
            }
        }

        for (i, instance) in primitives.iter().enumerate() {
            // If we encounter a push/pop clip, record where they occurred in the
            // primitive list for later processing.
            match instance.kind {
                PrimitiveInstanceKind::PushClipChain => {
                    clip_chain_instance_stack.push(clip_chain_instances.len());
                    clip_chain_instances.push(ClipChainPairInfo {
                        push_index: i,
                        pop_index: usize::MAX,
                        spatial_node_index: instance.spatial_node_index,
                        clip_chain_id: instance.clip_chain_id,
                    });
                    // Invalidate the prim_clips cache - there is a new clip chain.
                    update_shared_clips = true;
                    continue;
                }
                PrimitiveInstanceKind::PopClipChain => {
                    let index = clip_chain_instance_stack.pop().unwrap();
                    let clip_chain_instance = &mut clip_chain_instances[index];
                    debug_assert_eq!(clip_chain_instance.pop_index, usize::MAX);
                    debug_assert_eq!(
                        clip_chain_instance.clip_chain_id,
                        instance.clip_chain_id,
                    );
                    debug_assert_eq!(
                        clip_chain_instance.spatial_node_index,
                        instance.spatial_node_index,
                    );
                    clip_chain_instance.pop_index = i;
                    // Invalidate the prim_clips cache - a clip chain was removed.
                    update_shared_clips = true;
                    continue;
                }
                _ => {}
            }

            // If the primitive clip chain is different, then we need to rebuild prim_clips.
            update_shared_clips |= last_prim_clip_chain_id != instance.clip_chain_id;
            last_prim_clip_chain_id = instance.clip_chain_id;

            if update_shared_clips {
                prim_clips.clear();
                // Collect any clips from the clip chain stack that will affect this prim.
                for clip_instance_index in &clip_chain_instance_stack {
                    let clip_instance = &clip_chain_instances[*clip_instance_index];
                    add_clips(
                        clip_instance.clip_chain_id,
                        &mut prim_clips,
                        &self.clip_store,
                    );
                }
                // Collect any clips from the primitive's specific clip chain.
                add_clips(
                    instance.clip_chain_id,
                    &mut prim_clips,
                    &self.clip_store,
                );

                // We want to only retain clips that are shared across all primitives.
                // TODO(gw): We could consider using a HashSet here, but:
                //           (a) The sizes of these arrays are typically very small (<< 10).
                //           (b) We would have to impl Ord/Eq on interner handles, which we
                //               otherwise don't need / want.
                shared_clips.retain(|h1: &ClipDataHandle| {
                    let uid = h1.uid();
                    prim_clips.iter().any(|h2| {
                        uid == h2.uid()
                    })
                });

                update_shared_clips = false;
            }

            let scroll_root = self.clip_scroll_tree.find_scroll_root(
                instance.spatial_node_index,
            );

            if scroll_root != ROOT_SPATIAL_NODE_INDEX {
                // If we find multiple scroll roots in this page, then skip
                // picture caching for now. In future, we can handle picture
                // caching on these sites by creating a tile cache per
                // scroll root, or (more likely) selecting the common parent
                // scroll root between the detected scroll roots.
                match main_scroll_root {
                    Some(main_scroll_root) => {
                        if main_scroll_root != scroll_root {
                            return;
                        }
                    }
                    None => {
                        main_scroll_root = Some(scroll_root);
                    }
                }

                if first_index.is_none() {
                    // The first time we identify a prim that will be cached, set the prim_clips
                    // array to this, such that the retain() logic above works.
                    shared_clips = prim_clips.clone();
                    first_index = Some(i);
                }
            }
        }

        let main_scroll_root = match main_scroll_root {
            Some(main_scroll_root) => main_scroll_root,
            None => ROOT_SPATIAL_NODE_INDEX,
        };

        // Get the list of existing primitives in the main stacking context.
        let mut old_prim_list = primitives.take();

        // In the simple case, there are no preceding or trailing primitives,
        // because everything is anchored to the root scroll node. Handle
        // this case specially to avoid underflow error in the Some(..)
        // path below.

        let mut preceding_prims;
        let mut remaining_prims;

        match first_index {
            Some(first_index) => {
                // Split off the preceding primtives.
                remaining_prims = old_prim_list.split_off(first_index);
                preceding_prims = old_prim_list;
            }
            None => {
                preceding_prims = Vec::new();
                remaining_prims = old_prim_list;
            }
        }

        let mid_index = preceding_prims.len();

        // Step through each clip chain pair, and see if it crosses a slice boundary.
        for clip_chain_instance in clip_chain_instances {
            if clip_chain_instance.push_index < mid_index && clip_chain_instance.pop_index >= mid_index {
                preceding_prims.push(
                    create_clip_prim_instance(
                        clip_chain_instance.spatial_node_index,
                        clip_chain_instance.clip_chain_id,
                        PrimitiveInstanceKind::PopClipChain,
                    )
                );

                remaining_prims.insert(
                    0,
                    create_clip_prim_instance(
                        clip_chain_instance.spatial_node_index,
                        clip_chain_instance.clip_chain_id,
                        PrimitiveInstanceKind::PushClipChain,
                    )
                );
            }
        }

        let prim_list = PrimitiveList::new(
            remaining_prims,
            &self.interners,
        );

        // Now, create a picture with tile caching enabled that will hold all
        // of the primitives selected as belonging to the main scroll root.
        let pic_key = PictureKey::new(
            true,
            LayoutSize::zero(),
            Picture {
                composite_mode_key: PictureCompositeKey::Identity,
            },
        );

        let pic_data_handle = self.interners
            .picture
            .intern(&pic_key, || {
                PrimitiveSceneData {
                    prim_size: LayoutSize::zero(),
                    is_backface_visible: true,
                }
            }
            );

        // Build a clip-chain for the tile cache, that contains any of the shared clips
        // we will apply when drawing the tiles. In all cases provided by Gecko, these
        // are rectangle clips with a scale/offset transform only, and get handled as
        // a simple local clip rect in the vertex shader. However, this should in theory
        // also work with any complex clips, such as rounded rects and image masks, by
        // producing a clip mask that is applied to the picture cache tiles.
        let mut parent_clip_chain_id = ClipChainId::NONE;
        for clip_handle in &shared_clips {
            parent_clip_chain_id = self.clip_store.add_clip_chain_node(
                *clip_handle,
                parent_clip_chain_id,
            );
        }

        let tile_cache = Box::new(TileCacheInstance::new(
            0,
            main_scroll_root,
            self.config.background_color,
            shared_clips,
            parent_clip_chain_id,
        ));

        let pic_index = self.prim_store.pictures.alloc().init(PicturePrimitive::new_image(
            Some(PictureCompositeMode::TileCache { }),
            Picture3DContext::Out,
            None,
            true,
            true,
            RasterSpace::Screen,
            prim_list,
            main_scroll_root,
            Some(tile_cache),
            PictureOptions::default(),
        ));

        let instance = PrimitiveInstance::new(
            LayoutPoint::zero(),
            LayoutRect::max_rect(),
            PrimitiveInstanceKind::Picture {
                data_handle: pic_data_handle,
                pic_index: PictureIndex(pic_index),
                segment_instance_index: SegmentInstanceIndex::INVALID,
            },
            parent_clip_chain_id,
            main_scroll_root,
        );

        // This contains the tile caching picture, with preceding and
        // trailing primitives outside the main scroll root.
        primitives.reserve(preceding_prims.len() + 1);
        primitives.extend(preceding_prims);
        primitives.push(instance);
    }

    fn flatten_items(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        apply_pipeline_clip: bool,
    ) {
        loop {
            let subtraversal = {
                let item = match traversal.next() {
                    Some(item) => item,
                    None => break,
                };

                match item.item() {
                    DisplayItem::PopReferenceFrame |
                    DisplayItem::PopStackingContext => return,
                    _ => (),
                }

                self.flatten_item(
                    item,
                    pipeline_id,
                    apply_pipeline_clip,
                )
            };

            // If flatten_item created a sub-traversal, we need `traversal` to have the
            // same state as the completed subtraversal, so we reinitialize it here.
            if let Some(mut subtraversal) = subtraversal {
                subtraversal.merge_debug_stats_from(traversal);
                *traversal = subtraversal;
            }
        }

        // TODO: factor this out to be part of capture
        if cfg!(feature = "display_list_stats") {
            let stats = traversal.debug_stats();
            let total_bytes: usize = stats.iter().map(|(_, stats)| stats.num_bytes).sum();
            println!("item, total count, total bytes, % of DL bytes, bytes per item");
            for (label, stats) in stats {
                println!("{}, {}, {}kb, {}%, {}",
                    label,
                    stats.total_count,
                    stats.num_bytes / 1000,
                    ((stats.num_bytes as f32 / total_bytes.max(1) as f32) * 100.0) as usize,
                    stats.num_bytes / stats.total_count.max(1));
            }
            println!("");
        }
    }

    fn flatten_sticky_frame(
        &mut self,
        info: &StickyFrameDisplayItem,
        parent_node_index: SpatialNodeIndex,
    ) {
        let current_offset = self.current_offset(parent_node_index);
        let frame_rect = info.bounds.translate(current_offset);
        let sticky_frame_info = StickyFrameInfo::new(
            frame_rect,
            info.margins,
            info.vertical_offset_bounds,
            info.horizontal_offset_bounds,
            info.previously_applied_offset,
        );

        let index = self.clip_scroll_tree.add_sticky_frame(
            parent_node_index,
            sticky_frame_info,
            info.id.pipeline_id(),
        );
        self.id_to_index_mapper.map_spatial_node(info.id, index);
    }

    fn flatten_scroll_frame(
        &mut self,
        item: &DisplayItemRef,
        info: &ScrollFrameDisplayItem,
        parent_node_index: SpatialNodeIndex,
        pipeline_id: PipelineId,
    ) {
        let current_offset = self.current_offset(parent_node_index);
        let clip_region = ClipRegion::create_for_clip_node(
            info.clip_rect,
            item.complex_clip().iter(),
            info.image_mask,
            &current_offset,
        );
        // Just use clip rectangle as the frame rect for this scroll frame.
        // This is useful when calculating scroll extents for the
        // SpatialNode::scroll(..) API as well as for properly setting sticky
        // positioning offsets.
        let frame_rect = clip_region.main;
        let content_size = info.content_rect.size;

        self.add_clip_node(info.clip_id, &info.parent_space_and_clip, clip_region);

        self.add_scroll_frame(
            info.scroll_frame_id,
            parent_node_index,
            info.external_id,
            pipeline_id,
            &frame_rect,
            &content_size,
            info.scroll_sensitivity,
            ScrollFrameKind::Explicit,
            info.external_scroll_offset,
        );
    }

    fn flatten_reference_frame(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        parent_spatial_node: SpatialNodeIndex,
        origin: LayoutPoint,
        reference_frame: &ReferenceFrame,
        apply_pipeline_clip: bool,
    ) {
        let current_offset = self.current_offset(parent_spatial_node);
        self.push_reference_frame(
            reference_frame.id,
            Some(parent_spatial_node),
            pipeline_id,
            reference_frame.transform_style,
            reference_frame.transform,
            reference_frame.kind,
            current_offset + origin.to_vector(),
        );

        self.rf_mapper.push_scope();
        self.flatten_items(
            traversal,
            pipeline_id,
            apply_pipeline_clip,
        );
        self.rf_mapper.pop_scope();
    }


    fn flatten_stacking_context(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        stacking_context: &StackingContext,
        spatial_node_index: SpatialNodeIndex,
        origin: LayoutPoint,
        filters: ItemRange<FilterOp>,
        filter_datas: &[TempFilterData],
        filter_primitives: ItemRange<FilterPrimitive>,
        is_backface_visible: bool,
        apply_pipeline_clip: bool,
    ) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        let composition_operations = {
            CompositeOps::new(
                filter_ops_for_compositing(filters),
                filter_datas_for_compositing(filter_datas),
                filter_primitives_for_compositing(filter_primitives),
                stacking_context.mix_blend_mode_for_compositing(),
            )
        };

        let clip_chain_id = match stacking_context.clip_id {
            Some(clip_id) => self.id_to_index_mapper.get_clip_chain_id(clip_id),
            None => ClipChainId::NONE,
        };

        self.push_stacking_context(
            pipeline_id,
            composition_operations,
            stacking_context.transform_style,
            is_backface_visible,
            stacking_context.cache_tiles,
            spatial_node_index,
            clip_chain_id,
            stacking_context.raster_space,
            stacking_context.is_backdrop_root,
        );

        if cfg!(debug_assertions) && apply_pipeline_clip && clip_chain_id != ClipChainId::NONE {
            // This is the rootmost stacking context in this pipeline that has
            // a clip set. Check that the clip chain includes the pipeline clip
            // as well, because this where we recurse with `apply_pipeline_clip`
            // set to false and stop explicitly adding the pipeline clip to
            // individual items.
            let pipeline_clip = self.pipeline_clip_chain_stack.last().unwrap();
            let mut found_root = *pipeline_clip == ClipChainId::NONE;
            let mut cur_clip = clip_chain_id.clone();
            while cur_clip != ClipChainId::NONE {
                if cur_clip == *pipeline_clip {
                    found_root = true;
                    break;
                }
                cur_clip = self.clip_store.get_clip_chain(cur_clip).parent_clip_chain_id;
            }
            debug_assert!(found_root);
        }

        self.rf_mapper.push_offset(origin.to_vector());
        self.flatten_items(
            traversal,
            pipeline_id,
            apply_pipeline_clip && clip_chain_id == ClipChainId::NONE,
        );
        self.rf_mapper.pop_offset();

        self.pop_stacking_context();
    }

    fn flatten_iframe(
        &mut self,
        info: &IframeDisplayItem,
        spatial_node_index: SpatialNodeIndex,
    ) {
        let iframe_pipeline_id = info.pipeline_id;
        let pipeline = match self.scene.pipelines.get(&iframe_pipeline_id) {
            Some(pipeline) => pipeline,
            None => {
                debug_assert!(info.ignore_missing_pipeline);
                return
            },
        };

        let current_offset = self.current_offset(spatial_node_index);
        let clip_chain_index = self.add_clip_node(
            ClipId::root(iframe_pipeline_id),
            &info.space_and_clip,
            ClipRegion::create_for_clip_node_with_local_clip(
                &info.clip_rect,
                &current_offset,
            ),
        );
        self.pipeline_clip_chain_stack.push(clip_chain_index);

        let bounds = info.bounds;
        let origin = current_offset + bounds.origin.to_vector();
        let spatial_node_index = self.push_reference_frame(
            SpatialId::root_reference_frame(iframe_pipeline_id),
            Some(spatial_node_index),
            iframe_pipeline_id,
            TransformStyle::Flat,
            PropertyBinding::Value(LayoutTransform::identity()),
            ReferenceFrameKind::Transform,
            origin,
        );

        let iframe_rect = LayoutRect::new(LayoutPoint::zero(), bounds.size);
        self.add_scroll_frame(
            SpatialId::root_scroll_node(iframe_pipeline_id),
            spatial_node_index,
            Some(ExternalScrollId(0, iframe_pipeline_id)),
            iframe_pipeline_id,
            &iframe_rect,
            &pipeline.content_size,
            ScrollSensitivity::ScriptAndInputEvents,
            ScrollFrameKind::PipelineRoot,
            LayoutVector2D::zero(),
        );

        self.rf_mapper.push_scope();
        self.flatten_items(
            &mut pipeline.display_list.iter(),
            pipeline.pipeline_id,
            true,
        );
        self.rf_mapper.pop_scope();

        self.pipeline_clip_chain_stack.pop();
    }

    fn get_space(&mut self, spatial_id: &SpatialId) -> SpatialNodeIndex {
        self.id_to_index_mapper.get_spatial_node_index(*spatial_id)
    }

    fn get_clip_and_scroll(
        &mut self,
        clip_id: &ClipId,
        spatial_id: &SpatialId,
        apply_pipeline_clip: bool
    ) -> ScrollNodeAndClipChain {
        ScrollNodeAndClipChain::new(
            self.id_to_index_mapper.get_spatial_node_index(*spatial_id),
            if !apply_pipeline_clip && clip_id.is_root() {
                ClipChainId::NONE
            } else if clip_id.is_valid() {
                self.id_to_index_mapper.get_clip_chain_id(*clip_id)
            } else {
                ClipChainId::INVALID
            },
        )
    }

    fn process_common_properties(
        &mut self,
        common: &CommonItemProperties,
        apply_pipeline_clip: bool
    ) -> (LayoutPrimitiveInfo, ScrollNodeAndClipChain) {
        self.process_common_properties_with_bounds(common, &common.clip_rect, apply_pipeline_clip)
    }

    fn process_common_properties_with_bounds(
        &mut self,
        common: &CommonItemProperties,
        bounds: &LayoutRect,
        apply_pipeline_clip: bool
    ) -> (LayoutPrimitiveInfo, ScrollNodeAndClipChain) {
        let clip_and_scroll = self.get_clip_and_scroll(
            &common.clip_id,
            &common.spatial_id,
            apply_pipeline_clip
        );

        let current_offset = self.current_offset(clip_and_scroll.spatial_node_index);

        let clip_rect = common.clip_rect.translate(current_offset);
        let rect = bounds.translate(current_offset);
        let layout = LayoutPrimitiveInfo {
            rect,
            clip_rect,
            is_backface_visible: common.is_backface_visible,
            hit_info: common.hit_info,
        };

        (layout, clip_and_scroll)
    }

    fn flatten_item<'b>(
        &'b mut self,
        item: DisplayItemRef<'a, 'b>,
        pipeline_id: PipelineId,
        apply_pipeline_clip: bool,
    ) -> Option<BuiltDisplayListIter<'a>> {
        match *item.item() {
            DisplayItem::Image(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                    apply_pipeline_clip,
                );

                self.add_image(
                    clip_and_scroll,
                    &layout,
                    info.stretch_size,
                    info.tile_spacing,
                    None,
                    info.image_key,
                    info.image_rendering,
                    info.alpha_type,
                    info.color,
                );
            }
            DisplayItem::YuvImage(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                    apply_pipeline_clip,
                );

                self.add_yuv_image(
                    clip_and_scroll,
                    &layout,
                    info.yuv_data,
                    info.color_depth,
                    info.color_space,
                    info.color_range,
                    info.image_rendering,
                );
            }
            DisplayItem::Text(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                    apply_pipeline_clip,
                );

                self.add_text(
                    clip_and_scroll,
                    &layout,
                    &info.font_key,
                    &info.color,
                    item.glyphs(),
                    info.glyph_options,
                );
            }
            DisplayItem::Rectangle(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties(
                    &info.common,
                    apply_pipeline_clip,
                );

                self.add_solid_rectangle(
                    clip_and_scroll,
                    &layout,
                    info.color,
                );
            }
            DisplayItem::HitTest(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties(
                    &info.common,
                    apply_pipeline_clip,
                );

                self.add_solid_rectangle(
                    clip_and_scroll,
                    &layout,
                    ColorF::TRANSPARENT,
                );
            }
            DisplayItem::ClearRectangle(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties(
                    &info.common,
                    apply_pipeline_clip,
                );

                self.add_clear_rectangle(
                    clip_and_scroll,
                    &layout,
                );
            }
            DisplayItem::Line(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.area,
                    apply_pipeline_clip,
                );

                self.add_line(
                    clip_and_scroll,
                    &layout,
                    info.wavy_line_thickness,
                    info.orientation,
                    info.color,
                    info.style,
                );
            }
            DisplayItem::Gradient(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                    apply_pipeline_clip,
                );

                if let Some(prim_key_kind) = self.create_linear_gradient_prim(
                    &layout,
                    info.gradient.start_point,
                    info.gradient.end_point,
                    item.gradient_stops(),
                    info.gradient.extend_mode,
                    info.tile_size,
                    info.tile_spacing,
                    None,
                ) {
                    self.add_nonshadowable_primitive(
                        clip_and_scroll,
                        &layout,
                        Vec::new(),
                        prim_key_kind,
                    );
                }
            }
            DisplayItem::RadialGradient(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                    apply_pipeline_clip,
                );

                let prim_key_kind = self.create_radial_gradient_prim(
                    &layout,
                    info.gradient.center,
                    info.gradient.start_offset * info.gradient.radius.width,
                    info.gradient.end_offset * info.gradient.radius.width,
                    info.gradient.radius.width / info.gradient.radius.height,
                    item.gradient_stops(),
                    info.gradient.extend_mode,
                    info.tile_size,
                    info.tile_spacing,
                    None,
                );

                self.add_nonshadowable_primitive(
                    clip_and_scroll,
                    &layout,
                    Vec::new(),
                    prim_key_kind,
                );
            }
            DisplayItem::BoxShadow(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.box_bounds,
                    apply_pipeline_clip,
                );

                self.add_box_shadow(
                    clip_and_scroll,
                    &layout,
                    &info.offset,
                    info.color,
                    info.blur_radius,
                    info.spread_radius,
                    info.border_radius,
                    info.clip_mode,
                );
            }
            DisplayItem::Border(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                    apply_pipeline_clip,
                );

                self.add_border(
                    clip_and_scroll,
                    &layout,
                    info,
                    item.gradient_stops(),
                );
            }
            DisplayItem::PushStackingContext(ref info) => {
                let space = self.get_space(&info.spatial_id);
                let mut subtraversal = item.sub_iter();
                self.flatten_stacking_context(
                    &mut subtraversal,
                    pipeline_id,
                    &info.stacking_context,
                    space,
                    info.origin,
                    item.filters(),
                    item.filter_datas(),
                    item.filter_primitives(),
                    info.is_backface_visible,
                    apply_pipeline_clip,
                );
                return Some(subtraversal);
            }
            DisplayItem::PushReferenceFrame(ref info) => {
                let parent_space = self.get_space(&info.parent_spatial_id);
                let mut subtraversal = item.sub_iter();
                self.flatten_reference_frame(
                    &mut subtraversal,
                    pipeline_id,
                    parent_space,
                    info.origin,
                    &info.reference_frame,
                    apply_pipeline_clip,
                );
                return Some(subtraversal);
            }
            DisplayItem::Iframe(ref info) => {
                let space = self.get_space(&info.space_and_clip.spatial_id);
                self.flatten_iframe(
                    info,
                    space,
                );
            }
            DisplayItem::Clip(ref info) => {
                let parent_space = self.get_space(&info.parent_space_and_clip.spatial_id);
                let current_offset = self.current_offset(parent_space);
                let clip_region = ClipRegion::create_for_clip_node(
                    info.clip_rect,
                    item.complex_clip().iter(),
                    info.image_mask,
                    &current_offset,
                );
                self.add_clip_node(info.id, &info.parent_space_and_clip, clip_region);
            }
            DisplayItem::ClipChain(ref info) => {
                // For a user defined clip-chain the parent (if specified) must
                // refer to another user defined clip-chain. If none is specified,
                // the parent is the root clip-chain for the given pipeline. This
                // is used to provide a root clip chain for iframes.
                let parent_clip_chain_id = match info.parent {
                    Some(id) => {
                        self.id_to_index_mapper.get_clip_chain_id(ClipId::ClipChain(id))
                    }
                    None => {
                        self.pipeline_clip_chain_stack.last().cloned().unwrap()
                    }
                };

                // Create a linked list of clip chain nodes. To do this, we will
                // create a clip chain node + clip source for each listed clip id,
                // and link these together, with the root of this list parented to
                // the parent clip chain node found above. For this API, the clip
                // id that is specified for an existing clip chain node is used to
                // get the index of the clip sources that define that clip node.
                let mut clip_chain_id = parent_clip_chain_id;

                // For each specified clip id
                for clip_item in item.clip_chain_items() {
                    // Map the ClipId to an existing clip chain node.
                    let item_clip_node = self
                        .id_to_index_mapper
                        .get_clip_node(&clip_item);

                    let mut clip_node_clip_chain_id = item_clip_node.id;

                    // Each 'clip node' (as defined by the WR API) can contain one or
                    // more clip items (e.g. rects, image masks, rounded rects). When
                    // each of these clip nodes is stored internally, they are stored
                    // as a clip chain (one clip item per node), eventually parented
                    // to the parent clip node. For a user defined clip chain, we will
                    // need to walk the linked list of clip chain nodes for each clip
                    // node, accumulating them into one clip chain that is then
                    // parented to the clip chain parent.

                    for _ in 0 .. item_clip_node.count {
                        // Get the id of the clip sources entry for that clip chain node.
                        let handle = {
                            let clip_chain = self
                                .clip_store
                                .get_clip_chain(clip_node_clip_chain_id);

                            clip_node_clip_chain_id = clip_chain.parent_clip_chain_id;

                            clip_chain.handle
                        };

                        // Add a new clip chain node, which references the same clip sources, and
                        // parent it to the current parent.
                        clip_chain_id = self
                            .clip_store
                            .add_clip_chain_node(
                                handle,
                                clip_chain_id,
                            );
                    }
                }

                // Map the last entry in the clip chain to the supplied ClipId. This makes
                // this ClipId available as a source to other user defined clip chains.
                self.id_to_index_mapper.add_clip_chain(ClipId::ClipChain(info.id), clip_chain_id, 0);
            },
            DisplayItem::ScrollFrame(ref info) => {
                let parent_space = self.get_space(&info.parent_space_and_clip.spatial_id);
                self.flatten_scroll_frame(
                    &item,
                    info,
                    parent_space,
                    pipeline_id,
                );
            }
            DisplayItem::StickyFrame(ref info) => {
                let parent_space = self.get_space(&info.parent_spatial_id);
                self.flatten_sticky_frame(
                    info,
                    parent_space,
                );
            }
            DisplayItem::BackdropFilter(ref info) => {
                let (layout, clip_and_scroll) = self.process_common_properties(
                    &info.common,
                    apply_pipeline_clip,
                );

                let filters = filter_ops_for_compositing(item.filters());
                let filter_datas = filter_datas_for_compositing(item.filter_datas());
                let filter_primitives = filter_primitives_for_compositing(item.filter_primitives());

                self.add_backdrop_filter(
                    clip_and_scroll,
                    &layout,
                    filters,
                    filter_datas,
                    filter_primitives,
                );
            }

            // Do nothing; these are dummy items for the display list parser
            DisplayItem::SetGradientStops |
            DisplayItem::SetFilterOps |
            DisplayItem::SetFilterData |
            DisplayItem::SetFilterPrimitives => {}

            DisplayItem::PopReferenceFrame |
            DisplayItem::PopStackingContext => {
                unreachable!("Should have returned in parent method.")
            }
            DisplayItem::PushShadow(info) => {
                let clip_and_scroll = self.get_clip_and_scroll(
                    &info.space_and_clip.clip_id,
                    &info.space_and_clip.spatial_id,
                    apply_pipeline_clip
                );

                self.push_shadow(info.shadow, clip_and_scroll, info.should_inflate);
            }
            DisplayItem::PopAllShadows => {
                self.pop_all_shadows();
            }
        }

        None
    }

    // Given a list of clip sources, a positioning node and
    // a parent clip chain, return a new clip chain entry.
    // If the supplied list of clip sources is empty, then
    // just return the parent clip chain id directly.
    fn build_clip_chain(
        &mut self,
        clip_items: Vec<ClipItemKey>,
        parent_clip_chain_id: ClipChainId,
    ) -> ClipChainId {
        if clip_items.is_empty() {
            parent_clip_chain_id
        } else {
            let mut clip_chain_id = parent_clip_chain_id;

            for item in clip_items {
                // Intern this clip item, and store the handle
                // in the clip chain node.
                let handle = self.interners
                    .clip
                    .intern(&item, || item.kind.node_kind());

                clip_chain_id = self.clip_store.add_clip_chain_node(
                    handle,
                    clip_chain_id,
                );
            }

            clip_chain_id
        }
    }

    /// Create a primitive and add it to the prim store. This method doesn't
    /// add the primitive to the draw list, so can be used for creating
    /// sub-primitives.
    ///
    /// TODO(djg): Can this inline into `add_interned_prim_to_draw_list`
    fn create_primitive<P>(
        &mut self,
        info: &LayoutPrimitiveInfo,
        clip_chain_id: ClipChainId,
        spatial_node_index: SpatialNodeIndex,
        prim: P,
    ) -> PrimitiveInstance
    where
        P: InternablePrimitive,
        Interners: AsMut<Interner<P>>,
    {
        // Build a primitive key.
        let prim_key = prim.into_key(info);

        let current_offset = self.current_offset(spatial_node_index);
        let interner = self.interners.as_mut();
        let prim_data_handle = interner
            .intern(&prim_key, || {
                PrimitiveSceneData {
                    prim_size: info.rect.size,
                    is_backface_visible: info.is_backface_visible,
                }
            });

        let instance_kind = P::make_instance_kind(
            prim_key,
            prim_data_handle,
            &mut self.prim_store,
            current_offset,
        );

        PrimitiveInstance::new(
            info.rect.origin,
            info.clip_rect,
            instance_kind,
            clip_chain_id,
            spatial_node_index,
        )
    }

    pub fn add_primitive_to_hit_testing_list(
        &mut self,
        info: &LayoutPrimitiveInfo,
        clip_and_scroll: ScrollNodeAndClipChain
    ) {
        let tag = match info.hit_info {
            Some(tag) => tag,
            None => return,
        };

        // We want to get a range of clip chain roots that apply to this
        // hit testing primitive.

        // Get the start index for the clip chain root range for this primitive.
        let start = self.hit_testing_scene.next_clip_chain_index();

        // Add the clip chain root for the primitive itself.
        self.hit_testing_scene.add_clip_chain(clip_and_scroll.clip_chain_id);

        // Append any clip chain roots from enclosing stacking contexts.
        for sc in &self.sc_stack {
            self.hit_testing_scene.add_clip_chain(sc.clip_chain_id);
        }

        // Construct a clip chain roots range to be stored with the item.
        let clip_chain_range = ops::Range {
            start,
            end: self.hit_testing_scene.next_clip_chain_index(),
        };

        // Create and store the hit testing primitive itself.
        let new_item = HitTestingItem::new(
            tag,
            info,
            clip_and_scroll.spatial_node_index,
            clip_chain_range,
        );
        self.hit_testing_scene.add_item(new_item);
    }

    /// Add an already created primitive to the draw lists.
    pub fn add_primitive_to_draw_list(
        &mut self,
        prim_instance: PrimitiveInstance,
    ) {
        // Add primitive to the top-most stacking context on the stack.
        if prim_instance.is_chased() {
            println!("\tadded to stacking context at {}", self.sc_stack.len());
        }

        let stacking_context = self.sc_stack.last_mut().unwrap();
        stacking_context.primitives.push(prim_instance);
    }

    /// Convenience interface that creates a primitive entry and adds it
    /// to the draw list.
    fn add_nonshadowable_primitive<P>(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        clip_items: Vec<ClipItemKey>,
        prim: P,
    )
    where
        P: InternablePrimitive + IsVisible,
        Interners: AsMut<Interner<P>>,
    {
        if prim.is_visible() {
            let clip_chain_id = self.build_clip_chain(
                clip_items,
                clip_and_scroll.clip_chain_id,
            );
            self.add_prim_to_draw_list(
                info,
                clip_chain_id,
                clip_and_scroll,
                prim,
            );
        }
    }

    pub fn add_primitive<P>(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        clip_items: Vec<ClipItemKey>,
        prim: P,
    )
    where
        P: InternablePrimitive + IsVisible,
        Interners: AsMut<Interner<P>>,
        ShadowItem: From<PendingPrimitive<P>>
    {
        // If a shadow context is not active, then add the primitive
        // directly to the parent picture.
        if self.pending_shadow_items.is_empty() {
            self.add_nonshadowable_primitive(
                clip_and_scroll,
                info,
                clip_items,
                prim,
            );
        } else {
            debug_assert!(clip_items.is_empty(), "No per-prim clips expected for shadowed primitives");

            // There is an active shadow context. Store as a pending primitive
            // for processing during pop_all_shadows.
            self.pending_shadow_items.push_back(PendingPrimitive {
                clip_and_scroll,
                info: *info,
                prim,
            }.into());
        }
    }

    fn add_prim_to_draw_list<P>(
        &mut self,
        info: &LayoutPrimitiveInfo,
        clip_chain_id: ClipChainId,
        clip_and_scroll: ScrollNodeAndClipChain,
        prim: P,
    )
    where
        P: InternablePrimitive,
        Interners: AsMut<Interner<P>>,
    {
        let prim_instance = self.create_primitive(
            info,
            clip_chain_id,
            clip_and_scroll.spatial_node_index,
            prim,
        );
        self.register_chase_primitive_by_rect(
            &info.rect,
            &prim_instance,
        );
        self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
        self.add_primitive_to_draw_list(prim_instance);
    }

    pub fn push_stacking_context(
        &mut self,
        pipeline_id: PipelineId,
        composite_ops: CompositeOps,
        transform_style: TransformStyle,
        is_backface_visible: bool,
        create_tile_cache: bool,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
        requested_raster_space: RasterSpace,
        is_backdrop_root: bool,
    ) {
        // Check if this stacking context is the root of a pipeline, and the caller
        // has requested it as an output frame.
        let is_pipeline_root =
            self.sc_stack.last().map_or(true, |sc| sc.pipeline_id != pipeline_id);
        let frame_output_pipeline_id = if is_pipeline_root && self.output_pipelines.contains(&pipeline_id) {
            Some(pipeline_id)
        } else {
            None
        };

        // Mark if a user supplied tile cache was specified.
        self.found_explicit_tile_cache |= create_tile_cache;

        if is_pipeline_root && create_tile_cache && self.config.enable_picture_caching {
            // we don't expect any nested tile-cache-enabled stacking contexts
            debug_assert!(!self.sc_stack.iter().any(|sc| sc.create_tile_cache));
        }

        // Get the transform-style of the parent stacking context,
        // which determines if we *might* need to draw this on
        // an intermediate surface for plane splitting purposes.
        let (parent_is_3d, extra_3d_instance) = match self.sc_stack.last_mut() {
            Some(ref mut sc) if sc.is_3d() => {
                let flat_items_context_3d = match sc.context_3d {
                    Picture3DContext::In { ancestor_index, .. } => Picture3DContext::In {
                        root_data: None,
                        ancestor_index,
                    },
                    Picture3DContext::Out => panic!("Unexpected out of 3D context"),
                };
                // Cut the sequence of flat children before starting a child stacking context,
                // so that the relative order between them and our current SC is preserved.
                let extra_instance = sc.cut_item_sequence(
                    &mut self.prim_store,
                    &mut self.interners,
                    Some(PictureCompositeMode::Blit(BlitReason::PRESERVE3D)),
                    flat_items_context_3d,
                );
                (true, extra_instance.map(|(_, instance)| instance))
            },
            _ => (false, None),
        };

        if let Some(instance) = extra_3d_instance {
            self.add_primitive_instance_to_3d_root(instance);
        }

        // If this is preserve-3d *or* the parent is, then this stacking
        // context is participating in the 3d rendering context. In that
        // case, hoist the picture up to the 3d rendering context
        // container, so that it's rendered as a sibling with other
        // elements in this context.
        let participating_in_3d_context =
            composite_ops.is_empty() &&
            (parent_is_3d || transform_style == TransformStyle::Preserve3D);

        let context_3d = if participating_in_3d_context {
            // Find the spatial node index of the containing block, which
            // defines the context of backface-visibility.
            let ancestor_context = self.sc_stack
                .iter()
                .rfind(|sc| !sc.is_3d());
            Picture3DContext::In {
                root_data: if parent_is_3d {
                    None
                } else {
                    Some(Vec::new())
                },
                ancestor_index: match ancestor_context {
                    Some(sc) => sc.spatial_node_index,
                    None => ROOT_SPATIAL_NODE_INDEX,
                },
            }
        } else {
            Picture3DContext::Out
        };

        // Force an intermediate surface if the stacking context has a
        // complex clip node. In the future, we may decide during
        // prepare step to skip the intermediate surface if the
        // clip node doesn't affect the stacking context rect.
        let mut blit_reason = BlitReason::empty();
        let mut current_clip_chain_id = clip_chain_id;

        // Walk each clip in this chain, to see whether any of the clips
        // require that we draw this to an intermediate surface.
        while current_clip_chain_id != ClipChainId::NONE {
            let clip_chain_node = &self
                .clip_store
                .clip_chain_nodes[current_clip_chain_id.0 as usize];

            let clip_kind = self.interners.clip[clip_chain_node.handle];

            if let ClipNodeKind::Complex = clip_kind {
                blit_reason = BlitReason::CLIP;
                break;
            }

            current_clip_chain_id = clip_chain_node.parent_clip_chain_id;
        }

        // Push the SC onto the stack, so we know how to handle things in
        // pop_stacking_context.
        self.sc_stack.push(FlattenedStackingContext {
            primitives: Vec::new(),
            pipeline_id,
            is_backface_visible,
            requested_raster_space,
            spatial_node_index,
            clip_chain_id,
            frame_output_pipeline_id,
            composite_ops,
            blit_reason,
            transform_style,
            context_3d,
            create_tile_cache,
            is_backdrop_root,
        });
    }

    pub fn pop_stacking_context(&mut self) {
        let mut stacking_context = self.sc_stack.pop().unwrap();

        // If we encounter a stacking context that is effectively a no-op, then instead
        // of creating a picture, just append the primitive list to the parent stacking
        // context as a short cut. This serves two purposes:
        // (a) It's an optimization to reduce picture count and allocations, as display lists
        //     often contain a lot of these stacking contexts that don't require pictures or
        //     off-screen surfaces.
        // (b) It's useful for the initial version of picture caching in gecko, by enabling
        //     is to just look for interesting scroll roots on the root stacking context,
        //     without having to consider cuts at stacking context boundaries.
        let parent_is_empty = match self.sc_stack.last_mut() {
            Some(parent_sc) => {
                if stacking_context.is_redundant(parent_sc) {
                    if stacking_context.clip_chain_id != ClipChainId::NONE {
                        let prim = create_clip_prim_instance(
                            stacking_context.spatial_node_index,
                            stacking_context.clip_chain_id,
                            PrimitiveInstanceKind::PushClipChain,
                        );
                        parent_sc.primitives.push(prim);
                    }

                    // If the parent context primitives list is empty, it's faster
                    // to assign the storage of the popped context instead of paying
                    // the copying cost for extend.
                    if parent_sc.primitives.is_empty() {
                        parent_sc.primitives = stacking_context.primitives;
                    } else {
                        parent_sc.primitives.extend(stacking_context.primitives);
                    }

                    if stacking_context.clip_chain_id != ClipChainId::NONE {
                        let prim = create_clip_prim_instance(
                            stacking_context.spatial_node_index,
                            stacking_context.clip_chain_id,
                            PrimitiveInstanceKind::PopClipChain,
                        );
                        parent_sc.primitives.push(prim);
                    }

                    return;
                }
                parent_sc.primitives.is_empty()
            },
            None => true,
        };

        if stacking_context.create_tile_cache {
            self.setup_picture_caching(
                &mut stacking_context.primitives,
            );
        }

        let (leaf_context_3d, leaf_composite_mode, leaf_output_pipeline_id) = match stacking_context.context_3d {
            // TODO(gw): For now, as soon as this picture is in
            //           a 3D context, we draw it to an intermediate
            //           surface and apply plane splitting. However,
            //           there is a large optimization opportunity here.
            //           During culling, we can check if there is actually
            //           perspective present, and skip the plane splitting
            //           completely when that is not the case.
            Picture3DContext::In { ancestor_index, .. } => (
                Picture3DContext::In { root_data: None, ancestor_index },
                Some(PictureCompositeMode::Blit(BlitReason::PRESERVE3D | stacking_context.blit_reason)),
                None,
            ),
            Picture3DContext::Out => (
                Picture3DContext::Out,
                if stacking_context.blit_reason.is_empty() {
                    // By default, this picture will be collapsed into
                    // the owning target.
                    None
                } else {
                    // Add a dummy composite filter if the SC has to be isolated.
                    Some(PictureCompositeMode::Blit(stacking_context.blit_reason))
                },
                stacking_context.frame_output_pipeline_id
            ),
        };

        // If no user supplied tile cache was specified, and picture caching is enabled,
        // create an implicit tile cache for the whole frame builder.
        // TODO(gw): This is only needed temporarily - once we support multiple slices
        //           correctly, this will be handled by setup_picture_caching.
        if self.sc_stack.is_empty() &&
            !self.found_explicit_tile_cache &&
            self.config.enable_picture_caching {

            let scroll_root = ROOT_SPATIAL_NODE_INDEX;

            let prim_list = PrimitiveList::new(
                stacking_context.primitives,
                &self.interners,
            );

            // Now, create a picture with tile caching enabled that will hold all
            // of the primitives selected as belonging to the main scroll root.
            let pic_key = PictureKey::new(
                true,
                LayoutSize::zero(),
                Picture {
                    composite_mode_key: PictureCompositeKey::Identity,
                },
            );

            let pic_data_handle = self.interners
                .picture
                .intern(&pic_key, || {
                    PrimitiveSceneData {
                        prim_size: LayoutSize::zero(),
                        is_backface_visible: true,
                    }
                }
                );

            // TODO(gw): For now, we don't bother trying to share any clips if we create an
            //           implicit picture cache. This cache always caches with a fixed position
            //           root scroll node, so it won't save any invalidations. It might save
            //           some per-item clipping work though. We should handle this by unifying
            //           the implicit cache creation code here to work as part of the normal
            //           setup_picture_caching method.
            let tile_cache = TileCacheInstance::new(
                0,
                ROOT_SPATIAL_NODE_INDEX,
                self.config.background_color,
                Vec::new(),
                ClipChainId::NONE,
            );

            let pic_index = self.prim_store.pictures.alloc().init(PicturePrimitive::new_image(
                Some(PictureCompositeMode::TileCache {}),
                Picture3DContext::Out,
                None,
                true,
                true,
                RasterSpace::Screen,
                prim_list,
                scroll_root,
                Some(Box::new(tile_cache)),
                PictureOptions::default(),
            ));

            let instance = PrimitiveInstance::new(
                LayoutPoint::zero(),
                LayoutRect::max_rect(),
                PrimitiveInstanceKind::Picture {
                    data_handle: pic_data_handle,
                    pic_index: PictureIndex(pic_index),
                    segment_instance_index: SegmentInstanceIndex::INVALID,
                },
                ClipChainId::NONE,
                scroll_root,
            );

            stacking_context.primitives = vec![instance];
        }

        // Add picture for this actual stacking context contents to render into.
        let leaf_pic_index = PictureIndex(self.prim_store.pictures
            .alloc()
            .init(PicturePrimitive::new_image(
                leaf_composite_mode.clone(),
                leaf_context_3d,
                leaf_output_pipeline_id,
                true,
                stacking_context.is_backface_visible,
                stacking_context.requested_raster_space,
                PrimitiveList::new(
                    stacking_context.primitives,
                    &self.interners,
                ),
                stacking_context.spatial_node_index,
                None,
                PictureOptions::default(),
            ))
        );

        // Create a chain of pictures based on presence of filters,
        // mix-blend-mode and/or 3d rendering context containers.

        let mut current_pic_index = leaf_pic_index;
        let mut cur_instance = create_prim_instance(
            leaf_pic_index,
            leaf_composite_mode.into(),
            stacking_context.is_backface_visible,
            ClipChainId::NONE,
            stacking_context.spatial_node_index,
            &mut self.interners,
        );

        if cur_instance.is_chased() {
            println!("\tis a leaf primitive for a stacking context");
        }

        // If establishing a 3d context, the `cur_instance` represents
        // a picture with all the *trailing* immediate children elements.
        // We append this to the preserve-3D picture set and make a container picture of them.
        if let Picture3DContext::In { root_data: Some(mut prims), ancestor_index } = stacking_context.context_3d {
            prims.push(cur_instance);

            // This is the acttual picture representing our 3D hierarchy root.
            current_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    None,
                    Picture3DContext::In {
                        root_data: Some(Vec::new()),
                        ancestor_index,
                    },
                    stacking_context.frame_output_pipeline_id,
                    true,
                    stacking_context.is_backface_visible,
                    stacking_context.requested_raster_space,
                    PrimitiveList::new(
                        prims,
                        &self.interners,
                    ),
                    stacking_context.spatial_node_index,
                    None,
                    PictureOptions::default(),
                ))
            );

            cur_instance = create_prim_instance(
                current_pic_index,
                PictureCompositeKey::Identity,
                stacking_context.is_backface_visible,
                ClipChainId::NONE,
                stacking_context.spatial_node_index,
                &mut self.interners,
            );
        }

        let (filtered_pic_index, filtered_instance) = self.wrap_prim_with_filters(
            cur_instance,
            current_pic_index,
            stacking_context.composite_ops.filters,
            stacking_context.composite_ops.filter_primitives,
            stacking_context.composite_ops.filter_datas,
            stacking_context.is_backface_visible,
            stacking_context.requested_raster_space,
            stacking_context.spatial_node_index,
            true,
        );

        current_pic_index = filtered_pic_index;
        cur_instance = filtered_instance;

        // Same for mix-blend-mode, except we can skip if this primitive is the first in the parent
        // stacking context.
        // From https://drafts.fxtf.org/compositing-1/#generalformula, the formula for blending is:
        // Cs = (1 - ab) x Cs + ab x Blend(Cb, Cs)
        // where
        // Cs = Source color
        // ab = Backdrop alpha
        // Cb = Backdrop color
        //
        // If we're the first primitive within a stacking context, then we can guarantee that the
        // backdrop alpha will be 0, and then the blend equation collapses to just
        // Cs = Cs, and the blend mode isn't taken into account at all.
        let has_mix_blend = if let (Some(mix_blend_mode), false) = (stacking_context.composite_ops.mix_blend_mode, parent_is_empty) {
            let composite_mode = Some(PictureCompositeMode::MixBlend(mix_blend_mode));

            let blend_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    composite_mode.clone(),
                    Picture3DContext::Out,
                    None,
                    true,
                    stacking_context.is_backface_visible,
                    stacking_context.requested_raster_space,
                    PrimitiveList::new(
                        vec![cur_instance.clone()],
                        &self.interners,
                    ),
                    stacking_context.spatial_node_index,
                    None,
                    PictureOptions::default(),
                ))
            );

            current_pic_index = blend_pic_index;
            cur_instance = create_prim_instance(
                blend_pic_index,
                composite_mode.into(),
                stacking_context.is_backface_visible,
                ClipChainId::NONE,
                stacking_context.spatial_node_index,
                &mut self.interners,
            );

            if cur_instance.is_chased() {
                println!("\tis a mix-blend picture for a stacking context with {:?}", mix_blend_mode);
            }
            true
        } else {
            false
        };

        // Set the stacking context clip on the outermost picture in the chain,
        // unless we already set it on the leaf picture.
        cur_instance.clip_chain_id = stacking_context.clip_chain_id;

        // The primitive instance for the remainder of flat children of this SC
        // if it's a part of 3D hierarchy but not the root of it.
        let trailing_children_instance = match self.sc_stack.last_mut() {
            // Preserve3D path (only relevant if there are no filters/mix-blend modes)
            Some(ref parent_sc) if parent_sc.is_3d() => {
                Some(cur_instance)
            }
            // Regular parenting path
            Some(ref mut parent_sc) => {
                // If we have a mix-blend-mode, the stacking context needs to be isolated
                // to blend correctly as per the CSS spec.
                // If not already isolated for some other reason,
                // make this picture as isolated.
                if has_mix_blend {
                    parent_sc.blit_reason |= BlitReason::ISOLATE;
                }
                parent_sc.primitives.push(cur_instance);
                None
            }
            // This must be the root stacking context
            None => {
                self.root_pic_index = current_pic_index;
                None
            }
        };

        // finally, if there any outstanding 3D primitive instances,
        // find the 3D hierarchy root and add them there.
        if let Some(instance) = trailing_children_instance {
            self.add_primitive_instance_to_3d_root(instance);
        }

        assert!(
            self.pending_shadow_items.is_empty(),
            "Found unpopped shadows when popping stacking context!"
        );
    }

    pub fn push_reference_frame(
        &mut self,
        reference_frame_id: SpatialId,
        parent_index: Option<SpatialNodeIndex>,
        pipeline_id: PipelineId,
        transform_style: TransformStyle,
        source_transform: PropertyBinding<LayoutTransform>,
        kind: ReferenceFrameKind,
        origin_in_parent_reference_frame: LayoutVector2D,
    ) -> SpatialNodeIndex {
        let index = self.clip_scroll_tree.add_reference_frame(
            parent_index,
            transform_style,
            source_transform,
            kind,
            origin_in_parent_reference_frame,
            pipeline_id,
        );
        self.id_to_index_mapper.map_spatial_node(reference_frame_id, index);

        index
    }

    pub fn push_root(
        &mut self,
        pipeline_id: PipelineId,
        viewport_size: &LayoutSize,
        content_size: &LayoutSize,
    ) {
        if let ChasePrimitive::Id(id) = self.config.chase_primitive {
            println!("Chasing {:?} by index", id);
            register_prim_chase_id(id);
        }

        self.id_to_index_mapper.add_clip_chain(ClipId::root(pipeline_id), ClipChainId::NONE, 0);

        let spatial_node_index = self.push_reference_frame(
            SpatialId::root_reference_frame(pipeline_id),
            None,
            pipeline_id,
            TransformStyle::Flat,
            PropertyBinding::Value(LayoutTransform::identity()),
            ReferenceFrameKind::Transform,
            LayoutVector2D::zero(),
        );

        self.add_scroll_frame(
            SpatialId::root_scroll_node(pipeline_id),
            spatial_node_index,
            Some(ExternalScrollId(0, pipeline_id)),
            pipeline_id,
            &LayoutRect::new(LayoutPoint::zero(), *viewport_size),
            content_size,
            ScrollSensitivity::ScriptAndInputEvents,
            ScrollFrameKind::PipelineRoot,
            LayoutVector2D::zero(),
        );
    }

    pub fn add_clip_node<I>(
        &mut self,
        new_node_id: ClipId,
        space_and_clip: &SpaceAndClipInfo,
        clip_region: ClipRegion<I>,
    ) -> ClipChainId
    where
        I: IntoIterator<Item = ComplexClipRegion>
    {
        // Add a new ClipNode - this is a ClipId that identifies a list of clip items,
        // and the positioning node associated with those clip sources.

        // Map from parent ClipId to existing clip-chain.
        let mut parent_clip_chain_index = self.id_to_index_mapper.get_clip_chain_id(space_and_clip.clip_id);
        // Map the ClipId for the positioning node to a spatial node index.
        let spatial_node_index = self.id_to_index_mapper.get_spatial_node_index(space_and_clip.spatial_id);

        let mut clip_count = 0;

        // Intern each clip item in this clip node, and add the interned
        // handle to a clip chain node, parented to form a chain.
        // TODO(gw): We could re-structure this to share some of the
        //           interning and chaining code.

        // Build the clip sources from the supplied region.
        let item = ClipItemKey {
            kind: ClipItemKeyKind::rectangle(clip_region.main, ClipMode::Clip),
            spatial_node_index,
        };
        let handle = self
            .interners
            .clip
            .intern(&item, || ClipNodeKind::Rectangle);

        parent_clip_chain_index = self
            .clip_store
            .add_clip_chain_node(
                handle,
                parent_clip_chain_index,
            );
        clip_count += 1;

        if let Some(ref image_mask) = clip_region.image_mask {
            let item = ClipItemKey {
                kind: ClipItemKeyKind::image_mask(image_mask),
                spatial_node_index,
            };

            let handle = self
                .interners
                .clip
                .intern(&item, || ClipNodeKind::Complex);

            parent_clip_chain_index = self
                .clip_store
                .add_clip_chain_node(
                    handle,
                    parent_clip_chain_index,
                );
            clip_count += 1;
        }

        for region in clip_region.complex_clips {
            let item = ClipItemKey {
                kind: ClipItemKeyKind::rounded_rect(
                    region.rect,
                    region.radii,
                    region.mode,
                ),
                spatial_node_index,
            };

            let handle = self
                .interners
                .clip
                .intern(&item, || ClipNodeKind::Complex);

            parent_clip_chain_index = self
                .clip_store
                .add_clip_chain_node(
                    handle,
                    parent_clip_chain_index,
                );
            clip_count += 1;
        }

        // Map the supplied ClipId -> clip chain id.
        self.id_to_index_mapper.add_clip_chain(
            new_node_id,
            parent_clip_chain_index,
            clip_count,
        );

        parent_clip_chain_index
    }

    pub fn add_scroll_frame(
        &mut self,
        new_node_id: SpatialId,
        parent_node_index: SpatialNodeIndex,
        external_id: Option<ExternalScrollId>,
        pipeline_id: PipelineId,
        frame_rect: &LayoutRect,
        content_size: &LayoutSize,
        scroll_sensitivity: ScrollSensitivity,
        frame_kind: ScrollFrameKind,
        external_scroll_offset: LayoutVector2D,
    ) -> SpatialNodeIndex {
        let node_index = self.clip_scroll_tree.add_scroll_frame(
            parent_node_index,
            external_id,
            pipeline_id,
            frame_rect,
            content_size,
            scroll_sensitivity,
            frame_kind,
            external_scroll_offset,
        );
        self.id_to_index_mapper.map_spatial_node(new_node_id, node_index);
        node_index
    }

    pub fn push_shadow(
        &mut self,
        shadow: Shadow,
        clip_and_scroll: ScrollNodeAndClipChain,
        should_inflate: bool,
    ) {
        // Store this shadow in the pending list, for processing
        // during pop_all_shadows.
        self.pending_shadow_items.push_back(ShadowItem::Shadow(PendingShadow {
            shadow,
            clip_and_scroll,
            should_inflate,
        }));
    }

    pub fn pop_all_shadows(
        &mut self,
    ) {
        assert!(!self.pending_shadow_items.is_empty(), "popped shadows, but none were present");

        let mut items = mem::replace(&mut self.pending_shadow_items, VecDeque::new());

        //
        // The pending_shadow_items queue contains a list of shadows and primitives
        // that were pushed during the active shadow context. To process these, we:
        //
        // Iterate the list, popping an item from the front each iteration.
        //
        // If the item is a shadow:
        //      - Create a shadow picture primitive.
        //      - Add *any* primitives that remain in the item list to this shadow.
        // If the item is a primitive:
        //      - Add that primitive as a normal item (if alpha > 0)
        //

        while let Some(item) = items.pop_front() {
            match item {
                ShadowItem::Shadow(pending_shadow) => {
                    // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
                    // "the image that would be generated by applying to the shadow a
                    // Gaussian blur with a standard deviation equal to half the blur radius."
                    let std_deviation = pending_shadow.shadow.blur_radius * 0.5;

                    // If the shadow has no blur, any elements will get directly rendered
                    // into the parent picture surface, instead of allocating and drawing
                    // into an intermediate surface. In this case, we will need to apply
                    // the local clip rect to primitives.
                    let is_passthrough = pending_shadow.shadow.blur_radius == 0.0;

                    // shadows always rasterize in local space.
                    // TODO(gw): expose API for clients to specify a raster scale
                    let raster_space = if is_passthrough {
                        self.sc_stack.last().unwrap().requested_raster_space
                    } else {
                        RasterSpace::Local(1.0)
                    };

                    // Add any primitives that come after this shadow in the item
                    // list to this shadow.
                    let mut prims = Vec::new();

                    for item in &items {
                        match item {
                            ShadowItem::Image(ref pending_image) => {
                                self.add_shadow_prim(
                                    &pending_shadow,
                                    pending_image,
                                    &mut prims,
                                )
                            }
                            ShadowItem::LineDecoration(ref pending_line_dec) => {
                                self.add_shadow_prim(
                                    &pending_shadow,
                                    pending_line_dec,
                                    &mut prims,
                                )
                            }
                            ShadowItem::NormalBorder(ref pending_border) => {
                                self.add_shadow_prim(
                                    &pending_shadow,
                                    pending_border,
                                    &mut prims,
                                )
                            }
                            ShadowItem::Primitive(ref pending_primitive) => {
                                self.add_shadow_prim(
                                    &pending_shadow,
                                    pending_primitive,
                                    &mut prims,
                                )
                            }
                            ShadowItem::TextRun(ref pending_text_run) => {
                                self.add_shadow_prim(
                                    &pending_shadow,
                                    pending_text_run,
                                    &mut prims,
                                )
                            }
                            _ => {}
                        }
                    }

                    // No point in adding a shadow here if there were no primitives
                    // added to the shadow.
                    if !prims.is_empty() {
                        // Create a picture that the shadow primitives will be added to. If the
                        // blur radius is 0, the code in Picture::prepare_for_render will
                        // detect this and mark the picture to be drawn directly into the
                        // parent picture, which avoids an intermediate surface and blur.
                        let mut blur_filter = Filter::Blur(std_deviation);
                        blur_filter.sanitize();
                        let composite_mode = PictureCompositeMode::Filter(blur_filter);
                        let composite_mode_key = Some(composite_mode.clone()).into();
                        let is_backface_visible = true; //TODO: double check this

                        // Pass through configuration information about whether WR should
                        // do the bounding rect inflation for text shadows.
                        let options = PictureOptions {
                            inflate_if_required: pending_shadow.should_inflate,
                        };

                        // Create the primitive to draw the shadow picture into the scene.
                        let shadow_pic_index = PictureIndex(self.prim_store.pictures
                            .alloc()
                            .init(PicturePrimitive::new_image(
                                Some(composite_mode),
                                Picture3DContext::Out,
                                None,
                                is_passthrough,
                                is_backface_visible,
                                raster_space,
                                PrimitiveList::new(
                                    prims,
                                    &self.interners,
                                ),
                                pending_shadow.clip_and_scroll.spatial_node_index,
                                None,
                                options,
                            ))
                        );

                        let shadow_pic_key = PictureKey::new(
                            true,
                            LayoutSize::zero(),
                            Picture { composite_mode_key },
                        );

                        let shadow_prim_data_handle = self.interners
                            .picture
                            .intern(&shadow_pic_key, || {
                                PrimitiveSceneData {
                                    prim_size: LayoutSize::zero(),
                                    is_backface_visible: true,
                                }
                            }
                        );

                        let shadow_prim_instance = PrimitiveInstance::new(
                            LayoutPoint::zero(),
                            LayoutRect::max_rect(),
                            PrimitiveInstanceKind::Picture {
                                data_handle: shadow_prim_data_handle,
                                pic_index: shadow_pic_index,
                                segment_instance_index: SegmentInstanceIndex::INVALID,
                            },
                            pending_shadow.clip_and_scroll.clip_chain_id,
                            pending_shadow.clip_and_scroll.spatial_node_index,
                        );

                        // Add the shadow primitive. This must be done before pushing this
                        // picture on to the shadow stack, to avoid infinite recursion!
                        self.add_primitive_to_draw_list(shadow_prim_instance);
                    }
                }
                ShadowItem::Image(pending_image) => {
                    self.add_shadow_prim_to_draw_list(
                        pending_image,
                    )
                },
                ShadowItem::LineDecoration(pending_line_dec) => {
                    self.add_shadow_prim_to_draw_list(
                        pending_line_dec,
                    )
                },
                ShadowItem::NormalBorder(pending_border) => {
                    self.add_shadow_prim_to_draw_list(
                        pending_border,
                    )
                },
                ShadowItem::Primitive(pending_primitive) => {
                    self.add_shadow_prim_to_draw_list(
                        pending_primitive,
                    )
                },
                ShadowItem::TextRun(pending_text_run) => {
                    self.add_shadow_prim_to_draw_list(
                        pending_text_run,
                    )
                },
            }
        }

        debug_assert!(items.is_empty());
        self.pending_shadow_items = items;
    }

    fn add_shadow_prim<P>(
        &mut self,
        pending_shadow: &PendingShadow,
        pending_primitive: &PendingPrimitive<P>,
        prims: &mut Vec<PrimitiveInstance>,
    )
    where
        P: InternablePrimitive + CreateShadow,
        Interners: AsMut<Interner<P>>,
    {
        // Offset the local rect and clip rect by the shadow offset.
        let mut info = pending_primitive.info.clone();
        info.rect = info.rect.translate(pending_shadow.shadow.offset);
        info.clip_rect = info.clip_rect.translate(
            pending_shadow.shadow.offset
        );

        // Construct and add a primitive for the given shadow.
        let shadow_prim_instance = self.create_primitive(
            &info,
            pending_primitive.clip_and_scroll.clip_chain_id,
            pending_primitive.clip_and_scroll.spatial_node_index,
            pending_primitive.prim.create_shadow(&pending_shadow.shadow),
        );

        // Add the new primitive to the shadow picture.
        prims.push(shadow_prim_instance);
    }

    fn add_shadow_prim_to_draw_list<P>(
        &mut self,
        pending_primitive: PendingPrimitive<P>,
    ) where
        P: InternablePrimitive + IsVisible,
        Interners: AsMut<Interner<P>>,
    {
        // For a normal primitive, if it has alpha > 0, then we add this
        // as a normal primitive to the parent picture.
        if pending_primitive.prim.is_visible() {
            self.add_prim_to_draw_list(
                &pending_primitive.info,
                pending_primitive.clip_and_scroll.clip_chain_id,
                pending_primitive.clip_and_scroll,
                pending_primitive.prim,
            );
        }
    }

    #[cfg(debug_assertions)]
    fn register_chase_primitive_by_rect(
        &mut self,
        rect: &LayoutRect,
        prim_instance: &PrimitiveInstance,
    ) {
        if ChasePrimitive::LocalRect(*rect) == self.config.chase_primitive {
            println!("Chasing {:?} by local rect", prim_instance.id);
            register_prim_chase_id(prim_instance.id);
        }
    }

    #[cfg(not(debug_assertions))]
    fn register_chase_primitive_by_rect(
        &mut self,
        _rect: &LayoutRect,
        _prim_instance: &PrimitiveInstance,
    ) {
    }

    pub fn add_solid_rectangle(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        color: ColorF,
    ) {
        if color.a == 0.0 {
            // Don't add transparent rectangles to the draw list, but do consider them for hit
            // testing. This allows specifying invisible hit testing areas.
            self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
            return;
        }

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveKeyKind::Rectangle {
                color: color.into(),
            },
        );
    }

    pub fn add_clear_rectangle(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
    ) {
        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveKeyKind::Clear,
        );
    }

    pub fn add_line(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        wavy_line_thickness: f32,
        orientation: LineOrientation,
        color: ColorF,
        style: LineStyle,
    ) {
        // For line decorations, we can construct the render task cache key
        // here during scene building, since it doesn't depend on device
        // pixel ratio or transform.
        let mut info = info.clone();

        let size = get_line_decoration_sizes(
            &info.rect.size,
            orientation,
            style,
            wavy_line_thickness,
        );

        let cache_key = size.map(|(inline_size, block_size)| {
            let size = match orientation {
                LineOrientation::Horizontal => LayoutSize::new(inline_size, block_size),
                LineOrientation::Vertical => LayoutSize::new(block_size, inline_size),
            };

            // If dotted, adjust the clip rect to ensure we don't draw a final
            // partial dot.
            if style == LineStyle::Dotted {
                let clip_size = match orientation {
                    LineOrientation::Horizontal => {
                        LayoutSize::new(
                            inline_size * (info.rect.size.width / inline_size).floor(),
                            info.rect.size.height,
                        )
                    }
                    LineOrientation::Vertical => {
                        LayoutSize::new(
                            info.rect.size.width,
                            inline_size * (info.rect.size.height / inline_size).floor(),
                        )
                    }
                };
                let clip_rect = LayoutRect::new(
                    info.rect.origin,
                    clip_size,
                );
                info.clip_rect = clip_rect
                    .intersection(&info.clip_rect)
                    .unwrap_or_else(LayoutRect::zero);
            }

            LineDecorationCacheKey {
                style,
                orientation,
                wavy_line_thickness: Au::from_f32_px(wavy_line_thickness),
                size: size.to_au(),
            }
        });

        self.add_primitive(
            clip_and_scroll,
            &info,
            Vec::new(),
            LineDecoration {
                cache_key,
                color: color.into(),
            },
        );
    }

    pub fn add_border(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        border_item: &BorderDisplayItem,
        gradient_stops: ItemRange<GradientStop>,
    ) {
        match border_item.details {
            BorderDetails::NinePatch(ref border) => {
                let nine_patch = NinePatchDescriptor {
                    width: border.width,
                    height: border.height,
                    slice: border.slice,
                    fill: border.fill,
                    repeat_horizontal: border.repeat_horizontal,
                    repeat_vertical: border.repeat_vertical,
                    outset: border.outset.into(),
                    widths: border_item.widths.into(),
                };

                match border.source {
                    NinePatchBorderSource::Image(image_key) => {
                        let prim = ImageBorder {
                            request: ImageRequest {
                                key: image_key,
                                rendering: ImageRendering::Auto,
                                tile: None,
                            },
                            nine_patch,
                        };

                        self.add_nonshadowable_primitive(
                            clip_and_scroll,
                            info,
                            Vec::new(),
                            prim,
                        );
                    }
                    NinePatchBorderSource::Gradient(gradient) => {
                        let prim = match self.create_linear_gradient_prim(
                            &info,
                            gradient.start_point,
                            gradient.end_point,
                            gradient_stops,
                            gradient.extend_mode,
                            LayoutSize::new(border.height as f32, border.width as f32),
                            LayoutSize::zero(),
                            Some(Box::new(nine_patch)),
                        ) {
                            Some(prim) => prim,
                            None => return,
                        };

                        self.add_nonshadowable_primitive(
                            clip_and_scroll,
                            info,
                            Vec::new(),
                            prim,
                        );
                    }
                    NinePatchBorderSource::RadialGradient(gradient) => {
                        let prim = self.create_radial_gradient_prim(
                            &info,
                            gradient.center,
                            gradient.start_offset * gradient.radius.width,
                            gradient.end_offset * gradient.radius.width,
                            gradient.radius.width / gradient.radius.height,
                            gradient_stops,
                            gradient.extend_mode,
                            LayoutSize::new(border.height as f32, border.width as f32),
                            LayoutSize::zero(),
                            Some(Box::new(nine_patch)),
                        );

                        self.add_nonshadowable_primitive(
                            clip_and_scroll,
                            info,
                            Vec::new(),
                            prim,
                        );
                    }
                };
            }
            BorderDetails::Normal(ref border) => {
                self.add_normal_border(
                    info,
                    border,
                    border_item.widths,
                    clip_and_scroll,
                );
            }
        }
    }

    pub fn create_linear_gradient_prim(
        &mut self,
        info: &LayoutPrimitiveInfo,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        stops: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
        nine_patch: Option<Box<NinePatchDescriptor>>,
    ) -> Option<LinearGradient> {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        let mut max_alpha: f32 = 0.0;

        let stops = stops.iter().map(|stop| {
            max_alpha = max_alpha.max(stop.color.a);
            GradientStopKey {
                offset: stop.offset,
                color: stop.color.into(),
            }
        }).collect();

        // If all the stops have no alpha, then this
        // gradient can't contribute to the scene.
        if max_alpha <= 0.0 {
            return None;
        }

        // Try to ensure that if the gradient is specified in reverse, then so long as the stops
        // are also supplied in reverse that the rendered result will be equivalent. To do this,
        // a reference orientation for the gradient line must be chosen, somewhat arbitrarily, so
        // just designate the reference orientation as start < end. Aligned gradient rendering
        // manages to produce the same result regardless of orientation, so don't worry about
        // reversing in that case.
        let reverse_stops = start_point.x > end_point.x ||
            (start_point.x == end_point.x && start_point.y > end_point.y);

        // To get reftests exactly matching with reverse start/end
        // points, it's necessary to reverse the gradient
        // line in some cases.
        let (sp, ep) = if reverse_stops {
            (end_point, start_point)
        } else {
            (start_point, end_point)
        };

        Some(LinearGradient {
            extend_mode,
            start_point: sp.into(),
            end_point: ep.into(),
            stretch_size: stretch_size.into(),
            tile_spacing: tile_spacing.into(),
            stops,
            reverse_stops,
            nine_patch,
        })
    }

    pub fn create_radial_gradient_prim(
        &mut self,
        info: &LayoutPrimitiveInfo,
        center: LayoutPoint,
        start_radius: f32,
        end_radius: f32,
        ratio_xy: f32,
        stops: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
        nine_patch: Option<Box<NinePatchDescriptor>>,
    ) -> RadialGradient {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        let params = RadialGradientParams {
            start_radius,
            end_radius,
            ratio_xy,
        };

        let stops = stops.iter().map(|stop| {
            GradientStopKey {
                offset: stop.offset,
                color: stop.color.into(),
            }
        }).collect();

        RadialGradient {
            extend_mode,
            center: center.into(),
            params,
            stretch_size: stretch_size.into(),
            tile_spacing: tile_spacing.into(),
            nine_patch,
            stops,
        }
    }

    pub fn add_text(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        prim_info: &LayoutPrimitiveInfo,
        font_instance_key: &FontInstanceKey,
        text_color: &ColorF,
        glyph_range: ItemRange<GlyphInstance>,
        glyph_options: Option<GlyphOptions>,
    ) {
        let offset = self.current_offset(clip_and_scroll.spatial_node_index);

        let text_run = {
            let instance_map = self.font_instances.read().unwrap();
            let font_instance = match instance_map.get(font_instance_key) {
                Some(instance) => instance,
                None => {
                    warn!("Unknown font instance key");
                    debug!("key={:?}", font_instance_key);
                    return;
                }
            };

            // Trivial early out checks
            if font_instance.size.0 <= 0 {
                return;
            }

            // TODO(gw): Use a proper algorithm to select
            // whether this item should be rendered with
            // subpixel AA!
            let mut render_mode = self.config
                .default_font_render_mode
                .limit_by(font_instance.render_mode);
            let mut flags = font_instance.flags;
            if let Some(options) = glyph_options {
                render_mode = render_mode.limit_by(options.render_mode);
                flags |= options.flags;
            }

            let font = FontInstance::new(
                Arc::clone(font_instance),
                (*text_color).into(),
                render_mode,
                flags,
            );

            // TODO(gw): It'd be nice not to have to allocate here for creating
            //           the primitive key, when the common case is that the
            //           hash will match and we won't end up creating a new
            //           primitive template.
            let prim_offset = prim_info.rect.origin.to_vector() - offset;
            let glyphs = glyph_range
                .iter()
                .map(|glyph| {
                    GlyphInstance {
                        index: glyph.index,
                        point: glyph.point - prim_offset,
                    }
                })
                .collect();

            TextRun {
                glyphs: Arc::new(glyphs),
                font,
                shadow: false,
            }
        };

        self.add_primitive(
            clip_and_scroll,
            prim_info,
            Vec::new(),
            text_run,
        );
    }

    pub fn add_image(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
        sub_rect: Option<TexelRect>,
        image_key: ImageKey,
        image_rendering: ImageRendering,
        alpha_type: AlphaType,
        color: ColorF,
    ) {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);
        let info = LayoutPrimitiveInfo {
            rect: prim_rect,
            .. *info
        };

        let sub_rect = sub_rect.map(|texel_rect| {
            DeviceIntRect::new(
                DeviceIntPoint::new(
                    texel_rect.uv0.x as i32,
                    texel_rect.uv0.y as i32,
                ),
                DeviceIntSize::new(
                    (texel_rect.uv1.x - texel_rect.uv0.x) as i32,
                    (texel_rect.uv1.y - texel_rect.uv0.y) as i32,
                ),
            )
        });

        self.add_primitive(
            clip_and_scroll,
            &info,
            Vec::new(),
            Image {
                key: image_key,
                tile_spacing: tile_spacing.into(),
                stretch_size: stretch_size.into(),
                color: color.into(),
                sub_rect,
                image_rendering,
                alpha_type,
            },
        );
    }

    pub fn add_yuv_image(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        yuv_data: YuvData,
        color_depth: ColorDepth,
        color_space: YuvColorSpace,
        color_range: ColorRange,
        image_rendering: ImageRendering,
    ) {
        let format = yuv_data.get_format();
        let yuv_key = match yuv_data {
            YuvData::NV12(plane_0, plane_1) => [plane_0, plane_1, ImageKey::DUMMY],
            YuvData::PlanarYCbCr(plane_0, plane_1, plane_2) => [plane_0, plane_1, plane_2],
            YuvData::InterleavedYCbCr(plane_0) => [plane_0, ImageKey::DUMMY, ImageKey::DUMMY],
        };

        self.add_nonshadowable_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            YuvImage {
                color_depth,
                yuv_key,
                format,
                color_space,
                color_range,
                image_rendering,
            },
        );
    }

    pub fn add_primitive_instance_to_3d_root(&mut self, instance: PrimitiveInstance) {
        // find the 3D root and append to the children list
        for sc in self.sc_stack.iter_mut().rev() {
            match sc.context_3d {
                Picture3DContext::In { root_data: Some(ref mut prims), .. } => {
                    prims.push(instance);
                    break;
                }
                Picture3DContext::In { .. } => {}
                Picture3DContext::Out => panic!("Unable to find 3D root"),
            }
        }
    }

    pub fn add_backdrop_filter(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        filters: Vec<Filter>,
        filter_datas: Vec<FilterData>,
        filter_primitives: Vec<FilterPrimitive>,
    ) {
        let mut backdrop_pic_index = match self.cut_backdrop_picture() {
            // Backdrop contains no content, so no need to add backdrop-filter
            None => return,
            Some(backdrop_pic_index) => backdrop_pic_index,
        };

        let backdrop_spatial_node_index = self.prim_store.pictures[backdrop_pic_index.0].spatial_node_index;
        let requested_raster_space = self.sc_stack.last().expect("no active stacking context").requested_raster_space;

        let mut instance = self.create_primitive(
            info,
            // TODO(cbrewster): This is a bit of a hack to help figure out the correct sizing of the backdrop
            // region. By makings sure to include this, the clip chain instance computes the correct clip rect,
            // but we don't actually apply the filtered backdrop clip yet (this is done to the last instance in
            // the filter chain below).
            clip_and_scroll.clip_chain_id,
            backdrop_spatial_node_index,
            Backdrop {
                pic_index: backdrop_pic_index,
                spatial_node_index: clip_and_scroll.spatial_node_index,
                border_rect: info.rect.into(),
            },
        );

        // We will append the filtered backdrop to the backdrop root, but we need to
        // make sure all clips between the current stacking context and backdrop root
        // are taken into account. So we wrap the backdrop filter instance with a picture with
        // a clip for each stacking context.
        for stacking_context in self.sc_stack.iter().rev().take_while(|sc| !sc.is_backdrop_root) {
            let clip_chain_id = stacking_context.clip_chain_id;
            let is_backface_visible = stacking_context.is_backface_visible;
            let composite_mode = None;

            backdrop_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    composite_mode.clone(),
                    Picture3DContext::Out,
                    None,
                    true,
                    is_backface_visible,
                    requested_raster_space,
                    PrimitiveList::new(
                        vec![instance],
                        &mut self.interners,
                    ),
                    backdrop_spatial_node_index,
                    None,
                    PictureOptions {
                       inflate_if_required: false,
                    },
                ))
            );

            instance = create_prim_instance(
                backdrop_pic_index,
                composite_mode.into(),
                is_backface_visible,
                clip_chain_id,
                backdrop_spatial_node_index,
                &mut self.interners,
            );
        }

        let (mut filtered_pic_index, mut filtered_instance) = self.wrap_prim_with_filters(
            instance,
            backdrop_pic_index,
            filters,
            filter_primitives,
            filter_datas,
            info.is_backface_visible,
            requested_raster_space,
            backdrop_spatial_node_index,
            false,
        );

        // Apply filters from all stacking contexts up to, but not including the backdrop root.
        // Gecko pushes separate stacking contexts for filters and opacity,
        // so we must iterate through multiple stacking contexts to find all effects
        // that need to be applied to the filtered backdrop.
        let backdrop_root_pos = self.sc_stack.iter().rposition(|sc| sc.is_backdrop_root).expect("no backdrop root?");
        for i in ((backdrop_root_pos + 1)..self.sc_stack.len()).rev() {
            let stacking_context = &self.sc_stack[i];
            let filters = stacking_context.composite_ops.filters.clone();
            let filter_primitives = stacking_context.composite_ops.filter_primitives.clone();
            let filter_datas = stacking_context.composite_ops.filter_datas.clone();

            let (pic_index, instance) = self.wrap_prim_with_filters(
                filtered_instance,
                filtered_pic_index,
                filters,
                filter_primitives,
                filter_datas,
                info.is_backface_visible,
                requested_raster_space,
                backdrop_spatial_node_index,
                false,
            );

            filtered_instance = instance;
            filtered_pic_index = pic_index;
        }

        filtered_instance.clip_chain_id = clip_and_scroll.clip_chain_id;

        self.sc_stack.iter_mut().rev().find(|sc| sc.is_backdrop_root).unwrap().primitives.push(filtered_instance);
    }

    pub fn cut_backdrop_picture(&mut self) -> Option<PictureIndex> {
        let mut flattened_items = None;
        let mut backdrop_root =  None;
        for sc in self.sc_stack.iter_mut().rev() {
            // Add child contents to parent stacking context
            if let Some((_, flattened_instance)) = flattened_items.take() {
                sc.primitives.push(flattened_instance);
            }
            flattened_items = sc.cut_item_sequence(
                &mut self.prim_store,
                &mut self.interners,
                None,
                Picture3DContext::Out,
            );
            if sc.is_backdrop_root {
                backdrop_root = Some(sc);
                break;
            }
        }

        let (pic_index, instance) = flattened_items?;
        self.prim_store.pictures[pic_index.0].requested_composite_mode = Some(PictureCompositeMode::Blit(BlitReason::BACKDROP));
        backdrop_root.expect("no backdrop root found").primitives.push(instance);

        Some(pic_index)
    }

    fn wrap_prim_with_filters(
        &mut self,
        mut cur_instance: PrimitiveInstance,
        mut current_pic_index: PictureIndex,
        mut filter_ops: Vec<Filter>,
        mut filter_primitives: Vec<FilterPrimitive>,
        filter_datas: Vec<FilterData>,
        is_backface_visible: bool,
        requested_raster_space: RasterSpace,
        spatial_node_index: SpatialNodeIndex,
        inflate_if_required: bool,
    ) -> (PictureIndex, PrimitiveInstance) {
        // TODO(cbrewster): Currently CSS and SVG filters live side by side in WebRender, but unexpected results will
        // happen if they are used simulataneously. Gecko only provides either filter ops or filter primitives.
        // At some point, these two should be combined and CSS filters should be expressed in terms of SVG filters.
        assert!(filter_ops.is_empty() || filter_primitives.is_empty(),
            "Filter ops and filter primitives are not allowed on the same stacking context.");

        // For each filter, create a new image with that composite mode.
        let mut current_filter_data_index = 0;
        for filter in &mut filter_ops {
            filter.sanitize();

            let composite_mode = Some(match *filter {
                Filter::ComponentTransfer => {
                    let filter_data =
                        &filter_datas[current_filter_data_index];
                    let filter_data = filter_data.sanitize();
                    current_filter_data_index = current_filter_data_index + 1;
                    if filter_data.is_identity() {
                        continue
                    } else {
                        let filter_data_key = SFilterDataKey {
                            data:
                                SFilterData {
                                    r_func: SFilterDataComponent::from_functype_values(
                                        filter_data.func_r_type, &filter_data.r_values),
                                    g_func: SFilterDataComponent::from_functype_values(
                                        filter_data.func_g_type, &filter_data.g_values),
                                    b_func: SFilterDataComponent::from_functype_values(
                                        filter_data.func_b_type, &filter_data.b_values),
                                    a_func: SFilterDataComponent::from_functype_values(
                                        filter_data.func_a_type, &filter_data.a_values),
                                },
                        };

                        let handle = self.interners
                            .filter_data
                            .intern(&filter_data_key, || ());
                        PictureCompositeMode::ComponentTransferFilter(handle)
                    }
                }
                _ => PictureCompositeMode::Filter(filter.clone()),
            });

            let filter_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    composite_mode.clone(),
                    Picture3DContext::Out,
                    None,
                    true,
                    is_backface_visible,
                    requested_raster_space,
                    PrimitiveList::new(
                        vec![cur_instance.clone()],
                        &mut self.interners,
                    ),
                    spatial_node_index,
                    None,
                    PictureOptions {
                       inflate_if_required,
                    },
                ))
            );

            current_pic_index = filter_pic_index;
            cur_instance = create_prim_instance(
                current_pic_index,
                composite_mode.into(),
                is_backface_visible,
                ClipChainId::NONE,
                spatial_node_index,
                &mut self.interners,
            );

            if cur_instance.is_chased() {
                println!("\tis a composite picture for a stacking context with {:?}", filter);
            }

            // Run the optimize pass on this picture, to see if we can
            // collapse opacity and avoid drawing to an off-screen surface.
            self.prim_store.optimize_picture_if_possible(current_pic_index);
        }

        if !filter_primitives.is_empty() {
            let filter_datas = filter_datas.iter()
                .map(|filter_data| filter_data.sanitize())
                .map(|filter_data| {
                    SFilterData {
                        r_func: SFilterDataComponent::from_functype_values(
                            filter_data.func_r_type, &filter_data.r_values),
                        g_func: SFilterDataComponent::from_functype_values(
                            filter_data.func_g_type, &filter_data.g_values),
                        b_func: SFilterDataComponent::from_functype_values(
                            filter_data.func_b_type, &filter_data.b_values),
                        a_func: SFilterDataComponent::from_functype_values(
                            filter_data.func_a_type, &filter_data.a_values),
                    }
                })
                .collect();

            // Sanitize filter inputs
            for primitive in &mut filter_primitives {
                primitive.sanitize();
            }

            let composite_mode = PictureCompositeMode::SvgFilter(
                filter_primitives,
                filter_datas,
            );

            let filter_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    Some(composite_mode.clone()),
                    Picture3DContext::Out,
                    None,
                    true,
                    is_backface_visible,
                    requested_raster_space,
                    PrimitiveList::new(
                        vec![cur_instance.clone()],
                        &mut self.interners,
                    ),
                    spatial_node_index,
                    None,
                    PictureOptions {
                        inflate_if_required,
                    },
                ))
            );

            current_pic_index = filter_pic_index;
            cur_instance = create_prim_instance(
                current_pic_index,
                Some(composite_mode).into(),
                is_backface_visible,
                ClipChainId::NONE,
                spatial_node_index,
                &mut self.interners,
            );

            if cur_instance.is_chased() {
                println!("\tis a composite picture for a stacking context with an SVG filter");
            }

            // Run the optimize pass on this picture, to see if we can
            // collapse opacity and avoid drawing to an off-screen surface.
            self.prim_store.optimize_picture_if_possible(current_pic_index);
        }
        (current_pic_index, cur_instance)
    }
}


pub trait CreateShadow {
    fn create_shadow(&self, shadow: &Shadow) -> Self;
}

pub trait IsVisible {
    fn is_visible(&self) -> bool;
}

/// Properties of a stacking context that are maintained
/// during creation of the scene. These structures are
/// not persisted after the initial scene build.
struct FlattenedStackingContext {
    /// The list of primitive instances added to this stacking context.
    primitives: Vec<PrimitiveInstance>,

    /// Whether this stacking context is visible when backfacing
    is_backface_visible: bool,

    /// Whether or not the caller wants this drawn in
    /// screen space (quality) or local space (performance)
    requested_raster_space: RasterSpace,

    /// The positioning node for this stacking context
    spatial_node_index: SpatialNodeIndex,

    /// The clip chain for this stacking context
    clip_chain_id: ClipChainId,

    /// If set, this should be provided to caller
    /// as an output texture.
    frame_output_pipeline_id: Option<PipelineId>,

    /// The list of filters / mix-blend-mode for this
    /// stacking context.
    composite_ops: CompositeOps,

    /// Bitfield of reasons this stacking context needs to
    /// be an offscreen surface.
    blit_reason: BlitReason,

    /// Pipeline this stacking context belongs to.
    pipeline_id: PipelineId,

    /// CSS transform-style property.
    transform_style: TransformStyle,

    /// Defines the relationship to a preserve-3D hiearachy.
    context_3d: Picture3DContext<PrimitiveInstance>,

    /// If true, create a tile cache for this stacking context.
    create_tile_cache: bool,

    /// True if this stacking context is a backdrop root.
    is_backdrop_root: bool,
}

impl FlattenedStackingContext {
    /// Return true if the stacking context has a valid preserve-3d property
    pub fn is_3d(&self) -> bool {
        self.transform_style == TransformStyle::Preserve3D && self.composite_ops.is_empty()
    }

    /// Return true if the stacking context isn't needed.
    pub fn is_redundant(
        &self,
        parent: &FlattenedStackingContext,
    ) -> bool {
        // Any 3d context is required
        if let Picture3DContext::In { .. } = self.context_3d {
            return false;
        }

        // If there are filters / mix-blend-mode
        if !self.composite_ops.filters.is_empty() {
            return false;
        }

        // If there are svg filters
        if !self.composite_ops.filter_primitives.is_empty() {
            return false;
        }

        // We can skip mix-blend modes if they are the first primitive in a stacking context,
        // see pop_stacking_context for a full explanation.
        if !self.composite_ops.mix_blend_mode.is_none() &&
            !parent.primitives.is_empty() {
            return false;
        }

        // If backface visibility is explicitly set.
        if !self.is_backface_visible {
            return false;
        }

        // If rasterization space is different
        if self.requested_raster_space != parent.requested_raster_space {
            return false;
        }

        // If need to isolate in surface due to clipping / mix-blend-mode
        if !self.blit_reason.is_empty() {
            return false;
        }

        // If this stacking context gets picture caching, we need it.
        if self.create_tile_cache {
            return false;
        }

        // It is redundant!
        true
    }

    /// Cut the sequence of the immediate children recorded so far and generate a picture from them.
    pub fn cut_item_sequence(
        &mut self,
        prim_store: &mut PrimitiveStore,
        interners: &mut Interners,
        composite_mode: Option<PictureCompositeMode>,
        flat_items_context_3d: Picture3DContext<OrderedPictureChild>,
    ) -> Option<(PictureIndex, PrimitiveInstance)> {
        if self.primitives.is_empty() {
            return None
        }

        let pic_index = PictureIndex(prim_store.pictures
            .alloc()
            .init(PicturePrimitive::new_image(
                composite_mode.clone(),
                flat_items_context_3d,
                None,
                true,
                self.is_backface_visible,
                self.requested_raster_space,
                PrimitiveList::new(
                    mem::replace(&mut self.primitives, Vec::new()),
                    interners,
                ),
                self.spatial_node_index,
                None,
                PictureOptions::default(),
            ))
        );

        let prim_instance = create_prim_instance(
            pic_index,
            composite_mode.into(),
            self.is_backface_visible,
            self.clip_chain_id,
            self.spatial_node_index,
            interners,
        );

        Some((pic_index, prim_instance))
    }
}

/// A primitive that is added while a shadow context is
/// active is stored as a pending primitive and only
/// added to pictures during pop_all_shadows.
pub struct PendingPrimitive<T> {
    clip_and_scroll: ScrollNodeAndClipChain,
    info: LayoutPrimitiveInfo,
    prim: T,
}

/// As shadows are pushed, they are stored as pending
/// shadows, and handled at once during pop_all_shadows.
pub struct PendingShadow {
    shadow: Shadow,
    should_inflate: bool,
    clip_and_scroll: ScrollNodeAndClipChain,
}

pub enum ShadowItem {
    Shadow(PendingShadow),
    Image(PendingPrimitive<Image>),
    LineDecoration(PendingPrimitive<LineDecoration>),
    NormalBorder(PendingPrimitive<NormalBorderPrim>),
    Primitive(PendingPrimitive<PrimitiveKeyKind>),
    TextRun(PendingPrimitive<TextRun>),
}

impl From<PendingPrimitive<Image>> for ShadowItem {
    fn from(image: PendingPrimitive<Image>) -> Self {
        ShadowItem::Image(image)
    }
}

impl From<PendingPrimitive<LineDecoration>> for ShadowItem {
    fn from(line_dec: PendingPrimitive<LineDecoration>) -> Self {
        ShadowItem::LineDecoration(line_dec)
    }
}

impl From<PendingPrimitive<NormalBorderPrim>> for ShadowItem {
    fn from(border: PendingPrimitive<NormalBorderPrim>) -> Self {
        ShadowItem::NormalBorder(border)
    }
}

impl From<PendingPrimitive<PrimitiveKeyKind>> for ShadowItem {
    fn from(container: PendingPrimitive<PrimitiveKeyKind>) -> Self {
        ShadowItem::Primitive(container)
    }
}

impl From<PendingPrimitive<TextRun>> for ShadowItem {
    fn from(text_run: PendingPrimitive<TextRun>) -> Self {
        ShadowItem::TextRun(text_run)
    }
}

fn create_prim_instance(
    pic_index: PictureIndex,
    composite_mode_key: PictureCompositeKey,
    is_backface_visible: bool,
    clip_chain_id: ClipChainId,
    spatial_node_index: SpatialNodeIndex,
    interners: &mut Interners,
) -> PrimitiveInstance {
    let pic_key = PictureKey::new(
        is_backface_visible,
        LayoutSize::zero(),
        Picture { composite_mode_key },
    );

    let data_handle = interners
        .picture
        .intern(&pic_key, || {
            PrimitiveSceneData {
                prim_size: LayoutSize::zero(),
                is_backface_visible,
            }
        }
    );

    PrimitiveInstance::new(
        LayoutPoint::zero(),
        LayoutRect::max_rect(),
        PrimitiveInstanceKind::Picture {
            data_handle,
            pic_index,
            segment_instance_index: SegmentInstanceIndex::INVALID,
        },
        clip_chain_id,
        spatial_node_index,
    )
}

fn create_clip_prim_instance(
    spatial_node_index: SpatialNodeIndex,
    clip_chain_id: ClipChainId,
    kind: PrimitiveInstanceKind,
) -> PrimitiveInstance {
    PrimitiveInstance::new(
        LayoutPoint::zero(),
        LayoutRect::max_rect(),
        kind,
        clip_chain_id,
        spatial_node_index,
    )
}


fn filter_ops_for_compositing(
    input_filters: ItemRange<FilterOp>,
) -> Vec<Filter> {
    // TODO(gw): Now that we resolve these later on,
    //           we could probably make it a bit
    //           more efficient than cloning these here.
    input_filters.iter().map(|filter| filter.into()).collect()
}

fn filter_datas_for_compositing(
    input_filter_datas: &[TempFilterData],
) -> Vec<FilterData> {
    // TODO(gw): Now that we resolve these later on,
    //           we could probably make it a bit
    //           more efficient than cloning these here.
    let mut filter_datas = vec![];
    for temp_filter_data in input_filter_datas {
        let func_types : Vec<ComponentTransferFuncType> = temp_filter_data.func_types.iter().collect();
        debug_assert!(func_types.len() == 4);
        filter_datas.push( FilterData {
            func_r_type: func_types[0],
            r_values: temp_filter_data.r_values.iter().collect(),
            func_g_type: func_types[1],
            g_values: temp_filter_data.g_values.iter().collect(),
            func_b_type: func_types[2],
            b_values: temp_filter_data.b_values.iter().collect(),
            func_a_type: func_types[3],
            a_values: temp_filter_data.a_values.iter().collect(),
        });
    }
    filter_datas
}

fn filter_primitives_for_compositing(
    input_filter_primitives: ItemRange<FilterPrimitive>,
) -> Vec<FilterPrimitive> {
    // Resolve these in the flattener?
    // TODO(gw): Now that we resolve these later on,
    //           we could probably make it a bit
    //           more efficient than cloning these here.
    input_filter_primitives.iter().map(|primitive| primitive.into()).collect()
}

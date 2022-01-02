/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! # Scene building
//!
//! Scene building is the phase during which display lists, a representation built for
//! serialization, are turned into a scene, webrender's internal representation that is
//! suited for rendering frames.
//!
//! This phase is happening asynchronously on the scene builder thread.
//!
//! # General algorithm
//!
//! The important aspects of scene building are:
//! - Building up primitive lists (much of the cost of scene building goes here).
//! - Creating pictures for content that needs to be rendered into a surface, be it so that
//!   filters can be applied or for caching purposes.
//! - Maintaining a temporary stack of stacking contexts to keep track of some of the
//!   drawing states.
//! - Stitching multiple display lists which reference each other (without cycles) into
//!   a single scene (see build_reference_frame).
//! - Interning, which detects when some of the retained state stays the same between display
//!   lists.
//!
//! The scene builder linearly traverses the serialized display list which is naturally
//! ordered back-to-front, accumulating primitives in the top-most stacking context's
//! primitive list.
//! At the end of each stacking context (see pop_stacking_context), its primitive list is
//! either handed over to a picture if one is created, or it is concatenated into the parent
//! stacking context's primitive list.
//!
//! The flow of the algorithm is mostly linear except when handling:
//!  - shadow stacks (see push_shadow and pop_all_shadows),
//!  - backdrop filters (see add_backdrop_filter)
//!

use api::{AlphaType, BorderDetails, BorderDisplayItem, BuiltDisplayListIter, BuiltDisplayList, PrimitiveFlags};
use api::{ClipId, ColorF, CommonItemProperties, ComplexClipRegion, ComponentTransferFuncType, RasterSpace};
use api::{DisplayItem, DisplayItemRef, ExtendMode, ExternalScrollId, FilterData, SharedFontInstanceMap};
use api::{FilterOp, FilterPrimitive, FontInstanceKey, FontSize, GlyphInstance, GlyphOptions, GradientStop};
use api::{IframeDisplayItem, ImageKey, ImageRendering, ItemRange, ColorDepth, QualitySettings};
use api::{LineOrientation, LineStyle, NinePatchBorderSource, PipelineId, MixBlendMode, StackingContextFlags};
use api::{PropertyBinding, ReferenceFrameKind, ScrollFrameDescriptor, ReferenceFrameMapper};
use api::{Shadow, SpaceAndClipInfo, SpatialId, StickyFrameDescriptor, ImageMask, ItemTag};
use api::{ClipMode, PrimitiveKeyKind, TransformStyle, YuvColorSpace, ColorRange, YuvData, TempFilterData};
use api::{ReferenceTransformBinding, Rotation, FillRule, SpatialTreeItem, ReferenceFrameDescriptor};
use api::units::*;
use crate::image_tiling::simplify_repeated_primitive;
use crate::clip::{ClipChainId, ClipItemKey, ClipStore, ClipItemKeyKind};
use crate::clip::{ClipInternData, ClipNodeKind, ClipInstance, SceneClipInstance};
use crate::clip::{PolygonDataHandle};
use crate::spatial_tree::{SceneSpatialTree, SpatialNodeIndex, get_external_scroll_offset};
use crate::frame_builder::{ChasePrimitive, FrameBuilderConfig};
use crate::glyph_rasterizer::FontInstance;
use crate::hit_test::HitTestingScene;
use crate::intern::Interner;
use crate::internal_types::{FastHashMap, LayoutPrimitiveInfo, Filter, PlaneSplitter, PlaneSplitterIndex, PipelineInstanceId};
use crate::picture::{Picture3DContext, PictureCompositeMode, PicturePrimitive, PictureOptions};
use crate::picture::{BlitReason, OrderedPictureChild, PrimitiveList};
use crate::picture_graph::PictureGraph;
use crate::prim_store::{PrimitiveInstance, register_prim_chase_id};
use crate::prim_store::{PrimitiveInstanceKind, NinePatchDescriptor, PrimitiveStore};
use crate::prim_store::{InternablePrimitive, SegmentInstanceIndex, PictureIndex};
use crate::prim_store::PolygonKey;
use crate::prim_store::backdrop::Backdrop;
use crate::prim_store::borders::{ImageBorder, NormalBorderPrim};
use crate::prim_store::gradient::{
    GradientStopKey, LinearGradient, RadialGradient, RadialGradientParams, ConicGradient,
    ConicGradientParams, optimize_radial_gradient, apply_gradient_local_clip,
    optimize_linear_gradient, self,
};
use crate::prim_store::image::{Image, YuvImage};
use crate::prim_store::line_dec::{LineDecoration, LineDecorationCacheKey, get_line_decoration_size};
use crate::prim_store::picture::{Picture, PictureCompositeKey, PictureKey};
use crate::prim_store::text_run::TextRun;
use crate::render_backend::SceneView;
use crate::resource_cache::ImageRequest;
use crate::scene::{Scene, ScenePipeline, BuiltScene, SceneStats, StackingContextHelpers};
use crate::scene_builder_thread::Interners;
use crate::space::SpaceSnapper;
use crate::spatial_node::{StickyFrameInfo, ScrollFrameKind, SpatialNodeUid};
use crate::tile_cache::TileCacheBuilder;
use euclid::approxeq::ApproxEq;
use std::{f32, mem, usize};
use std::collections::vec_deque::VecDeque;
use std::sync::Arc;
use crate::util::{MaxRect, VecHelper};
use crate::filterdata::{SFilterDataComponent, SFilterData, SFilterDataKey};
use smallvec::SmallVec;

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
    /// or defers to the spatial tree to build the value.
    fn external_scroll_offset(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        spatial_tree: &SceneSpatialTree,
    ) -> LayoutVector2D {
        if spatial_node_index != self.current_spatial_node {
            self.current_spatial_node = spatial_node_index;
            self.current_offset = get_external_scroll_offset(spatial_tree, spatial_node_index);
        }

        self.current_offset
    }
}

/// A data structure that keeps track of mapping between API Ids for spatials and the indices
/// used internally in the SpatialTree to avoid having to do HashMap lookups for primitives
/// and clips during frame building.
#[derive(Default)]
pub struct NodeIdToIndexMapper {
    spatial_node_map: FastHashMap<SpatialId, SpatialNodeIndex>,
}

impl NodeIdToIndexMapper {
    fn add_spatial_node(&mut self, id: SpatialId, index: SpatialNodeIndex) {
        let _old_value = self.spatial_node_map.insert(id, index);
        assert!(_old_value.is_none());
    }

    fn get_spatial_node_index(&self, id: SpatialId) -> SpatialNodeIndex {
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

    /// Returns true if this CompositeOps contains any filters that affect
    /// the content (false if no filters, or filters are all no-ops).
    fn has_valid_filters(&self) -> bool {
        // For each filter, create a new image with that composite mode.
        let mut current_filter_data_index = 0;
        for filter in &self.filters {
            match filter {
                Filter::ComponentTransfer => {
                    let filter_data =
                        &self.filter_datas[current_filter_data_index];
                    let filter_data = filter_data.sanitize();
                    current_filter_data_index = current_filter_data_index + 1;
                    if filter_data.is_identity() {
                        continue
                    } else {
                        return true;
                    }
                }
                _ => {
                    if filter.is_noop() {
                        continue;
                    } else {
                        return true;
                    }
                }
            }
        }

        if !self.filter_primitives.is_empty() {
            return true;
        }

        false
    }
}

/// Represents the current input for a picture chain builder (either a
/// prim list from the stacking context, or a wrapped picture instance).
enum PictureSource {
    PrimitiveList {
        prim_list: PrimitiveList,
    },
    WrappedPicture {
        instance: PrimitiveInstance,
    },
}

/// Helper struct to build picture chains during scene building from
/// a flattened stacking context struct.
struct PictureChainBuilder {
    /// The current input source for the next picture
    current: PictureSource,

    /// Positioning node for this picture chain
    spatial_node_index: SpatialNodeIndex,
    /// Prim flags for any pictures in this chain
    flags: PrimitiveFlags,
}

impl PictureChainBuilder {
    /// Create a new picture chain builder, from a primitive list
    fn from_prim_list(
        prim_list: PrimitiveList,
        flags: PrimitiveFlags,
        spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        PictureChainBuilder {
            current: PictureSource::PrimitiveList {
                prim_list,
            },
            spatial_node_index,
            flags,
        }
    }

    /// Create a new picture chain builder, from a picture wrapper instance
    fn from_instance(
        instance: PrimitiveInstance,
        flags: PrimitiveFlags,
        spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        PictureChainBuilder {
            current: PictureSource::WrappedPicture {
                instance,
            },
            flags,
            spatial_node_index,
        }
    }

    /// Wrap the existing content with a new picture with the given parameters
    #[must_use]
    fn add_picture(
        self,
        composite_mode: PictureCompositeMode,
        context_3d: Picture3DContext<OrderedPictureChild>,
        options: PictureOptions,
        interners: &mut Interners,
        prim_store: &mut PrimitiveStore,
        prim_instances: &mut Vec<PrimitiveInstance>,
    ) -> PictureChainBuilder {
        let prim_list = match self.current {
            PictureSource::PrimitiveList { prim_list } => {
                prim_list
            }
            PictureSource::WrappedPicture { instance } => {
                let mut prim_list = PrimitiveList::empty();

                prim_list.add_prim(
                    instance,
                    LayoutRect::zero(),
                    self.spatial_node_index,
                    self.flags,
                    prim_instances,
                );

                prim_list
            }
        };

        let pic_index = PictureIndex(prim_store.pictures
            .alloc()
            .init(PicturePrimitive::new_image(
                Some(composite_mode.clone()),
                context_3d,
                true,
                self.flags,
                prim_list,
                self.spatial_node_index,
                options,
            ))
        );

        let instance = create_prim_instance(
            pic_index,
            Some(composite_mode).into(),
            ClipChainId::NONE,
            interners,
        );

        PictureChainBuilder {
            current: PictureSource::WrappedPicture {
                instance,
            },
            spatial_node_index: self.spatial_node_index,
            flags: self.flags,
        }
    }

    /// Finish building this picture chain. Set the clip chain on the outermost picture
    fn finalize(
        self,
        clip_chain_id: ClipChainId,
        interners: &mut Interners,
        prim_store: &mut PrimitiveStore,
    ) -> PrimitiveInstance {
        match self.current {
            PictureSource::WrappedPicture { mut instance } => {
                instance.clip_set.clip_chain_id = clip_chain_id;
                instance
            }
            PictureSource::PrimitiveList { prim_list } => {
                // If no picture was created for this stacking context, create a
                // pass-through wrapper now. This is only needed in 1-2 edge cases
                // now, and will be removed as a follow up.
                let pic_index = PictureIndex(prim_store.pictures
                    .alloc()
                    .init(PicturePrimitive::new_image(
                        None,
                        Picture3DContext::Out,
                        true,
                        self.flags,
                        prim_list,
                        self.spatial_node_index,
                        PictureOptions::default(),
                    ))
                );

                create_prim_instance(
                    pic_index,
                    None.into(),
                    clip_chain_id,
                    interners,
                )
            }
        }
    }
}

bitflags! {
    /// Slice flags
    pub struct SliceFlags : u8 {
        /// Slice created by a prim that has PrimitiveFlags::IS_SCROLLBAR_CONTAINER
        const IS_SCROLLBAR = 1;
        /// Represents a mix-blend container (can't split out compositor surfaces in this slice)
        const IS_BLEND_CONTAINER = 2;
    }
}

/// A structure that converts a serialized display list into a form that WebRender
/// can use to later build a frame. This structure produces a BuiltScene. Public
/// members are typically those that are destructured into the BuiltScene.
pub struct SceneBuilder<'a> {
    /// The scene that we are currently building.
    scene: &'a Scene,

    /// The map of all font instances.
    font_instances: SharedFontInstanceMap,

    /// The data structure that converts between ClipId/SpatialId and the various
    /// index types that the SpatialTree uses.
    id_to_index_mapper_stack: Vec<NodeIdToIndexMapper>,

    /// A stack of stacking context properties.
    sc_stack: Vec<FlattenedStackingContext>,

    /// Stack of spatial node indices forming containing block for 3d contexts
    containing_block_stack: Vec<SpatialNodeIndex>,

    /// Stack of requested raster spaces for stacking contexts
    raster_space_stack: Vec<RasterSpace>,

    /// Maintains state for any currently active shadows
    pending_shadow_items: VecDeque<ShadowItem>,

    /// The SpatialTree that we are currently building during building.
    pub spatial_tree: &'a mut SceneSpatialTree,

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

    /// Helper struct to map stacking context coords <-> reference frame coords.
    rf_mapper: ReferenceFrameMapper,

    /// Helper struct to map spatial nodes to external scroll offsets.
    external_scroll_mapper: ScrollOffsetMapper,

    /// The current recursion depth of iframes encountered. Used to restrict picture
    /// caching slices to only the top-level content frame.
    iframe_size: Vec<LayoutSize>,

    /// Clip-chain for root iframes applied to any tile caches created within this iframe
    root_iframe_clip: Option<ClipChainId>,

    /// The current quality / performance settings for this scene.
    quality_settings: QualitySettings,

    /// Maintains state about the list of tile caches being built for this scene.
    tile_cache_builder: TileCacheBuilder,

    /// A helper struct to snap local rects in device space. During frame
    /// building we may establish new raster roots, however typically that is in
    /// cases where we won't be applying snapping (e.g. has perspective), or in
    /// edge cases (e.g. SVG filter) where we can accept slightly incorrect
    /// behaviour in favour of getting the common case right.
    snap_to_device: SpaceSnapper,

    /// A DAG that represents dependencies between picture primitives. This builds
    /// a set of passes to run various picture processing passes in during frame
    /// building, in a way that pictures are processed before (or after) their
    /// dependencies, without relying on recursion for those passes.
    picture_graph: PictureGraph,

    /// A list of all the allocated plane splitters for this scene. A plane
    /// splitter is allocated whenever we encounter a new 3d rendering context.
    /// They are stored outside the picture since it makes it easier for them
    /// to be referenced by both the owning 3d rendering context and the child
    /// pictures that contribute to the splitter.
    plane_splitters: Vec<PlaneSplitter>,

    /// A list of all primitive instances in the scene. We store them as a single
    /// array so that multiple different systems (e.g. tile-cache, visibility, property
    /// animation bindings) can store index buffers to prim instances.
    prim_instances: Vec<PrimitiveInstance>,

    /// A map of pipeline ids encountered during scene build - used to create unique
    /// pipeline instance ids as they are encountered.
    pipeline_instance_ids: FastHashMap<PipelineId, u32>,
}

impl<'a> SceneBuilder<'a> {
    pub fn build(
        scene: &Scene,
        font_instances: SharedFontInstanceMap,
        view: &SceneView,
        frame_builder_config: &FrameBuilderConfig,
        interners: &mut Interners,
        spatial_tree: &mut SceneSpatialTree,
        stats: &SceneStats,
    ) -> BuiltScene {
        profile_scope!("build_scene");

        // We checked that the root pipeline is available on the render backend.
        let root_pipeline_id = scene.root_pipeline_id.unwrap();
        let root_pipeline = scene.pipelines.get(&root_pipeline_id).unwrap();

        let background_color = root_pipeline
            .background_color
            .and_then(|color| if color.a > 0.0 { Some(color) } else { None });

        let root_reference_frame_index = spatial_tree.root_reference_frame_index();

        // During scene building, we assume a 1:1 picture -> raster pixel scale
        let snap_to_device = SpaceSnapper::new(
            root_reference_frame_index,
            RasterPixelScale::new(1.0),
        );

        let mut builder = SceneBuilder {
            scene,
            spatial_tree,
            font_instances,
            config: *frame_builder_config,
            id_to_index_mapper_stack: Vec::new(),
            hit_testing_scene: HitTestingScene::new(&stats.hit_test_stats),
            pending_shadow_items: VecDeque::new(),
            sc_stack: Vec::new(),
            containing_block_stack: Vec::new(),
            raster_space_stack: vec![RasterSpace::Screen],
            prim_store: PrimitiveStore::new(&stats.prim_store_stats),
            clip_store: ClipStore::new(&stats.clip_store_stats),
            interners,
            rf_mapper: ReferenceFrameMapper::new(),
            external_scroll_mapper: ScrollOffsetMapper::new(),
            iframe_size: Vec::new(),
            root_iframe_clip: None,
            quality_settings: view.quality_settings,
            tile_cache_builder: TileCacheBuilder::new(root_reference_frame_index),
            snap_to_device,
            picture_graph: PictureGraph::new(),
            plane_splitters: Vec::new(),
            prim_instances: Vec::new(),
            pipeline_instance_ids: FastHashMap::default(),
        };

        builder.build_all(&root_pipeline);

        // Construct the picture cache primitive instance(s) from the tile cache builder
        let (tile_cache_config, tile_cache_pictures) = builder.tile_cache_builder.build(
            &builder.config,
            &mut builder.clip_store,
            &mut builder.prim_store,
            builder.interners,
        );

        // Add all the tile cache pictures as roots of the picture graph
        for pic_index in &tile_cache_pictures {
            builder.picture_graph.add_root(*pic_index);
        }

        BuiltScene {
            has_root_pipeline: scene.has_root_pipeline(),
            pipeline_epochs: scene.pipeline_epochs.clone(),
            output_rect: view.device_rect.size().into(),
            background_color,
            hit_testing_scene: Arc::new(builder.hit_testing_scene),
            prim_store: builder.prim_store,
            clip_store: builder.clip_store,
            config: builder.config,
            tile_cache_config,
            tile_cache_pictures,
            picture_graph: builder.picture_graph,
            plane_splitters: builder.plane_splitters,
            prim_instances: builder.prim_instances,
        }
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
                self.spatial_tree,
            );

        rf_offset + scroll_offset
    }

    fn build_spatial_tree_for_display_list(
        &mut self,
        dl: &BuiltDisplayList,
        pipeline_id: PipelineId,
        instance_id: PipelineInstanceId,
    ) {
        dl.iter_spatial_tree(|item| {
            match item {
                SpatialTreeItem::ScrollFrame(descriptor) => {
                    let parent_space = self.get_space(descriptor.parent_space);
                    self.build_scroll_frame(
                        descriptor,
                        parent_space,
                        pipeline_id,
                        instance_id,
                    );
                }
                SpatialTreeItem::ReferenceFrame(descriptor) => {
                    let parent_space = self.get_space(descriptor.parent_spatial_id);
                    self.build_reference_frame(
                        descriptor,
                        parent_space,
                        pipeline_id,
                        instance_id,
                    );
                }
                SpatialTreeItem::StickyFrame(descriptor) => {
                    let parent_space = self.get_space(descriptor.parent_spatial_id);
                    self.build_sticky_frame(
                        descriptor,
                        parent_space,
                        instance_id,
                    );
                }
                SpatialTreeItem::Invalid => {
                    unreachable!();
                }
            }
        });
    }

    fn build_all(&mut self, root_pipeline: &ScenePipeline) {
        enum ContextKind<'a> {
            Root,
            StackingContext {
                sc_info: StackingContextInfo,
            },
            ReferenceFrame,
            Iframe {
                parent_traversal: BuiltDisplayListIter<'a>,
            }
        }
        struct BuildContext<'a> {
            pipeline_id: PipelineId,
            kind: ContextKind<'a>,
        }

        let root_clip_id = ClipId::root(root_pipeline.pipeline_id);
        self.clip_store.register_clip_template(root_clip_id, root_clip_id, &[]);
        self.clip_store.push_clip_root(Some(root_clip_id), false);
        self.id_to_index_mapper_stack.push(NodeIdToIndexMapper::default());

        let instance_id = self.get_next_instance_id_for_pipeline(root_pipeline.pipeline_id);

        self.push_root(
            root_pipeline.pipeline_id,
            &root_pipeline.viewport_size,
            instance_id,
        );
        self.build_spatial_tree_for_display_list(
            &root_pipeline.display_list.display_list,
            root_pipeline.pipeline_id,
            instance_id,
        );

        let mut stack = vec![BuildContext {
            pipeline_id: root_pipeline.pipeline_id,
            kind: ContextKind::Root,
        }];
        let mut traversal = root_pipeline.display_list.iter();

        'outer: while let Some(bc) = stack.pop() {
            loop {
                let item = match traversal.next() {
                    Some(item) => item,
                    None => break,
                };

                match item.item() {
                    DisplayItem::PushStackingContext(ref info) => {
                        profile_scope!("build_stacking_context");
                        let spatial_node_index = self.get_space(info.spatial_id);
                        let mut subtraversal = item.sub_iter();
                        // Avoid doing unnecessary work for empty stacking contexts.
                        if subtraversal.current_stacking_context_empty() {
                            subtraversal.skip_current_stacking_context();
                            traversal = subtraversal;
                            continue;
                        }

                        let composition_operations = CompositeOps::new(
                            filter_ops_for_compositing(item.filters()),
                            filter_datas_for_compositing(item.filter_datas()),
                            filter_primitives_for_compositing(item.filter_primitives()),
                            info.stacking_context.mix_blend_mode_for_compositing(),
                        );

                        let sc_info = self.push_stacking_context(
                            composition_operations,
                            info.stacking_context.transform_style,
                            info.prim_flags,
                            spatial_node_index,
                            info.stacking_context.clip_id,
                            info.stacking_context.raster_space,
                            info.stacking_context.flags,
                            bc.pipeline_id,
                        );

                        self.rf_mapper.push_offset(info.origin.to_vector());
                        let new_context = BuildContext {
                            pipeline_id: bc.pipeline_id,
                            kind: ContextKind::StackingContext {
                                sc_info,
                            },
                        };
                        stack.push(bc);
                        stack.push(new_context);

                        subtraversal.merge_debug_stats_from(&mut traversal);
                        traversal = subtraversal;
                        continue 'outer;
                    }
                    DisplayItem::PushReferenceFrame(..) => {
                        profile_scope!("build_reference_frame");
                        let mut subtraversal = item.sub_iter();

                        self.rf_mapper.push_scope();
                        let new_context = BuildContext {
                            pipeline_id: bc.pipeline_id,
                            kind: ContextKind::ReferenceFrame,
                        };
                        stack.push(bc);
                        stack.push(new_context);

                        subtraversal.merge_debug_stats_from(&mut traversal);
                        traversal = subtraversal;
                        continue 'outer;
                    }
                    DisplayItem::PopReferenceFrame |
                    DisplayItem::PopStackingContext => break,
                    DisplayItem::Iframe(ref info) => {
                        profile_scope!("iframe");

                        let space = self.get_space(info.space_and_clip.spatial_id);
                        let subtraversal = match self.push_iframe(info, space) {
                            Some(pair) => pair,
                            None => continue,
                        };

                        let new_context = BuildContext {
                            pipeline_id: info.pipeline_id,
                            kind: ContextKind::Iframe {
                                parent_traversal: mem::replace(&mut traversal, subtraversal),
                            },
                        };
                        stack.push(bc);
                        stack.push(new_context);
                        continue 'outer;
                    }
                    _ => {
                        self.build_item(item, bc.pipeline_id);
                    }
                };
            }

            match bc.kind {
                ContextKind::Root => {}
                ContextKind::StackingContext { sc_info } => {
                    self.rf_mapper.pop_offset();
                    self.pop_stacking_context(sc_info);
                }
                ContextKind::ReferenceFrame => {
                    self.rf_mapper.pop_scope();
                }
                ContextKind::Iframe { parent_traversal } => {
                    self.iframe_size.pop();
                    self.rf_mapper.pop_scope();

                    self.clip_store.pop_clip_root();
                    if self.iframe_size.is_empty() {
                        assert!(self.root_iframe_clip.is_some());
                        self.root_iframe_clip = None;
                        self.add_tile_cache_barrier_if_needed(SliceFlags::empty());
                    }

                    self.id_to_index_mapper_stack.pop().unwrap();

                    traversal = parent_traversal;
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
                println!();
            }
        }

        self.clip_store.pop_clip_root();
        debug_assert!(self.sc_stack.is_empty());

        self.id_to_index_mapper_stack.pop().unwrap();
        assert!(self.id_to_index_mapper_stack.is_empty());
    }

    fn build_sticky_frame(
        &mut self,
        info: &StickyFrameDescriptor,
        parent_node_index: SpatialNodeIndex,
        instance_id: PipelineInstanceId,
    ) {
        let sticky_frame_info = StickyFrameInfo::new(
            info.bounds,
            info.margins,
            info.vertical_offset_bounds,
            info.horizontal_offset_bounds,
            info.previously_applied_offset,
        );

        let index = self.spatial_tree.add_sticky_frame(
            parent_node_index,
            sticky_frame_info,
            info.id.pipeline_id(),
            info.key,
            instance_id,
        );
        self.id_to_index_mapper_stack.last_mut().unwrap().add_spatial_node(info.id, index);
    }

    fn build_reference_frame(
        &mut self,
        info: &ReferenceFrameDescriptor,
        parent_space: SpatialNodeIndex,
        pipeline_id: PipelineId,
        instance_id: PipelineInstanceId,
    ) {
        let transform = match info.reference_frame.transform {
            ReferenceTransformBinding::Static { binding } => binding,
            ReferenceTransformBinding::Computed { scale_from, vertical_flip, rotation } => {
                let content_size = &self.iframe_size.last().unwrap();

                let mut transform = if let Some(scale_from) = scale_from {
                    // If we have a 90/270 degree rotation, then scale_from
                    // and content_size are in different coordinate spaces and
                    // we need to swap width/height for them to be correct.
                    match rotation {
                        Rotation::Degree0 |
                        Rotation::Degree180 => {
                            LayoutTransform::scale(
                                content_size.width / scale_from.width,
                                content_size.height / scale_from.height,
                                1.0
                            )
                        },
                        Rotation::Degree90 |
                        Rotation::Degree270 => {
                            LayoutTransform::scale(
                                content_size.height / scale_from.width,
                                content_size.width / scale_from.height,
                                1.0
                            )

                        }
                    }
                } else {
                    LayoutTransform::identity()
                };

                if vertical_flip {
                    let content_size = &self.iframe_size.last().unwrap();
                    transform = transform
                        .then_translate(LayoutVector3D::new(0.0, content_size.height, 0.0))
                        .pre_scale(1.0, -1.0, 1.0);
                }

                let rotate = rotation.to_matrix(**content_size);
                let transform = transform.then(&rotate);

                PropertyBinding::Value(transform)
            },
        };

        self.push_reference_frame(
            info.reference_frame.id,
            parent_space,
            pipeline_id,
            info.reference_frame.transform_style,
            transform,
            info.reference_frame.kind,
            info.origin.to_vector(),
            SpatialNodeUid::external(info.reference_frame.key, pipeline_id, instance_id),
        );
    }

    fn build_scroll_frame(
        &mut self,
        info: &ScrollFrameDescriptor,
        parent_node_index: SpatialNodeIndex,
        pipeline_id: PipelineId,
        instance_id: PipelineInstanceId,
    ) {
        // This is useful when calculating scroll extents for the
        // SpatialNode::scroll(..) API as well as for properly setting sticky
        // positioning offsets.
        let content_size = info.content_rect.size();

        self.add_scroll_frame(
            info.scroll_frame_id,
            parent_node_index,
            info.external_id,
            pipeline_id,
            &info.frame_rect,
            &content_size,
            ScrollFrameKind::Explicit,
            info.external_scroll_offset,
            SpatialNodeUid::external(info.key, pipeline_id, instance_id),
        );
    }

    /// Advance and return the next instance id for a given pipeline id
    fn get_next_instance_id_for_pipeline(
        &mut self,
        pipeline_id: PipelineId,
    ) -> PipelineInstanceId {
        let next_instance = self.pipeline_instance_ids
            .entry(pipeline_id)
            .or_insert(0);

        let instance_id = PipelineInstanceId::new(*next_instance);
        *next_instance += 1;

        instance_id
    }

    fn push_iframe(
        &mut self,
        info: &IframeDisplayItem,
        spatial_node_index: SpatialNodeIndex,
    ) -> Option<BuiltDisplayListIter<'a>> {
        let iframe_pipeline_id = info.pipeline_id;
        let pipeline = match self.scene.pipelines.get(&iframe_pipeline_id) {
            Some(pipeline) => pipeline,
            None => {
                debug_assert!(info.ignore_missing_pipeline);
                return None
            },
        };

        self.add_rect_clip_node(
            ClipId::root(iframe_pipeline_id),
            &info.space_and_clip,
            &info.clip_rect,
        );

        self.clip_store.push_clip_root(
            Some(ClipId::root(iframe_pipeline_id)),
            true,
        );

        let instance_id = self.get_next_instance_id_for_pipeline(iframe_pipeline_id);

        self.id_to_index_mapper_stack.push(NodeIdToIndexMapper::default());

        let bounds = self.snap_rect(
            &info.bounds,
            spatial_node_index,
        );

        let spatial_node_index = self.push_reference_frame(
            SpatialId::root_reference_frame(iframe_pipeline_id),
            spatial_node_index,
            iframe_pipeline_id,
            TransformStyle::Flat,
            PropertyBinding::Value(LayoutTransform::identity()),
            ReferenceFrameKind::Transform {
                is_2d_scale_translation: true,
                should_snap: true,
            },
            bounds.min.to_vector(),
            SpatialNodeUid::root_reference_frame(iframe_pipeline_id, instance_id),
        );

        let iframe_rect = LayoutRect::from_size(bounds.size());
        let is_root_pipeline = self.iframe_size.is_empty();

        self.add_scroll_frame(
            SpatialId::root_scroll_node(iframe_pipeline_id),
            spatial_node_index,
            ExternalScrollId(0, iframe_pipeline_id),
            iframe_pipeline_id,
            &iframe_rect,
            &bounds.size(),
            ScrollFrameKind::PipelineRoot {
                is_root_pipeline,
            },
            LayoutVector2D::zero(),
            SpatialNodeUid::root_scroll_frame(iframe_pipeline_id, instance_id),
        );

        // Get a clip-chain id for the root clip for this pipeline. We will
        // add that as an unconditional clip to any tile cache created within
        // this iframe. This ensures these clips are handled by the tile cache
        // compositing code, which is more efficient and accurate than applying
        // these clips individually to each primitive.
        let clip_id = ClipId::root(info.pipeline_id);
        let clip_chain_id = self.get_clip_chain(clip_id);

        // If this is a root iframe, force a new tile cache both before and after
        // adding primitives for this iframe.
        if self.iframe_size.is_empty() {
            self.add_tile_cache_barrier_if_needed(SliceFlags::empty());
            assert!(self.root_iframe_clip.is_none());
            self.root_iframe_clip = Some(clip_chain_id);
        }
        self.iframe_size.push(info.bounds.size());
        self.rf_mapper.push_scope();

        self.build_spatial_tree_for_display_list(
            &pipeline.display_list.display_list,
            iframe_pipeline_id,
            instance_id,
        );

        Some(pipeline.display_list.iter())
    }

    fn get_space(
        &self,
        spatial_id: SpatialId,
    ) -> SpatialNodeIndex {
        self.id_to_index_mapper_stack.last().unwrap().get_spatial_node_index(spatial_id)
    }

    fn get_clip_chain(
        &mut self,
        clip_id: ClipId,
    ) -> ClipChainId {
        self.clip_store.get_or_build_clip_chain_id(clip_id)
    }

    fn process_common_properties(
        &mut self,
        common: &CommonItemProperties,
        bounds: Option<&LayoutRect>,
    ) -> (LayoutPrimitiveInfo, LayoutRect, SpatialNodeIndex, ClipChainId) {
        let spatial_node_index = self.get_space(common.spatial_id);
        let clip_chain_id = self.get_clip_chain(common.clip_id);

        let current_offset = self.current_offset(spatial_node_index);

        let unsnapped_clip_rect = common.clip_rect.translate(current_offset);
        let clip_rect = self.snap_rect(
            &unsnapped_clip_rect,
            spatial_node_index,
        );

        let unsnapped_rect = bounds.map(|bounds| {
            bounds.translate(current_offset)
        });

        // If no bounds rect is given, default to clip rect.
        let rect = unsnapped_rect.map_or(clip_rect, |bounds| {
            self.snap_rect(
                &bounds,
                spatial_node_index,
            )
        });

        let layout = LayoutPrimitiveInfo {
            rect,
            clip_rect,
            flags: common.flags,
        };

        (layout, unsnapped_rect.unwrap_or(unsnapped_clip_rect), spatial_node_index, clip_chain_id)
    }

    fn process_common_properties_with_bounds(
        &mut self,
        common: &CommonItemProperties,
        bounds: &LayoutRect,
    ) -> (LayoutPrimitiveInfo, LayoutRect, SpatialNodeIndex, ClipChainId) {
        self.process_common_properties(
            common,
            Some(bounds),
        )
    }

    pub fn snap_rect(
        &mut self,
        rect: &LayoutRect,
        target_spatial_node: SpatialNodeIndex,
    ) -> LayoutRect {
        self.snap_to_device.set_target_spatial_node(
            target_spatial_node,
            self.spatial_tree,
        );
        self.snap_to_device.snap_rect(&rect)
    }

    fn build_item<'b>(
        &'b mut self,
        item: DisplayItemRef,
        pipeline_id: PipelineId,
    ) {
        match *item.item() {
            DisplayItem::Image(ref info) => {
                profile_scope!("image");

                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                self.add_image(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    layout.rect.size(),
                    LayoutSize::zero(),
                    info.image_key,
                    info.image_rendering,
                    info.alpha_type,
                    info.color,
                );
            }
            DisplayItem::RepeatingImage(ref info) => {
                profile_scope!("repeating_image");

                let (layout, unsnapped_rect, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                let stretch_size = process_repeat_size(
                    &layout.rect,
                    &unsnapped_rect,
                    info.stretch_size,
                );

                self.add_image(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    stretch_size,
                    info.tile_spacing,
                    info.image_key,
                    info.image_rendering,
                    info.alpha_type,
                    info.color,
                );
            }
            DisplayItem::YuvImage(ref info) => {
                profile_scope!("yuv_image");

                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                self.add_yuv_image(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    info.yuv_data,
                    info.color_depth,
                    info.color_space,
                    info.color_range,
                    info.image_rendering,
                );
            }
            DisplayItem::Text(ref info) => {
                profile_scope!("text");

                // TODO(aosmond): Snapping text primitives does not make much sense, given the
                // primitive bounds and clip are supposed to be conservative, not definitive.
                // E.g. they should be able to grow and not impact the output. However there
                // are subtle interactions between the primitive origin and the glyph offset
                // which appear to be significant (presumably due to some sort of accumulated
                // error throughout the layers). We should fix this at some point.
                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                self.add_text(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    &info.font_key,
                    &info.color,
                    item.glyphs(),
                    info.glyph_options,
                );
            }
            DisplayItem::Rectangle(ref info) => {
                profile_scope!("rect");

                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                self.add_primitive(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    Vec::new(),
                    PrimitiveKeyKind::Rectangle {
                        color: info.color.into(),
                    },
                );
            }
            DisplayItem::HitTest(ref info) => {
                profile_scope!("hit_test");

                // TODO(gw): We could skip building the clip-chain here completely, as it's not used by
                //           hit-test items.
                let (layout, _, spatial_node_index, _) = self.process_common_properties(
                    &info.common,
                    None,
                );

                // Don't add transparent rectangles to the draw list,
                // but do consider them for hit testing. This allows
                // specifying invisible hit testing areas.
                self.add_primitive_to_hit_testing_list(
                    &layout,
                    spatial_node_index,
                    info.common.clip_id,
                    info.tag,
                );
            }
            DisplayItem::ClearRectangle(ref info) => {
                profile_scope!("clear");

                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                self.add_clear_rectangle(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                );
            }
            DisplayItem::Line(ref info) => {
                profile_scope!("line");

                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.area,
                );

                self.add_line(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    info.wavy_line_thickness,
                    info.orientation,
                    info.color,
                    info.style,
                );
            }
            DisplayItem::Gradient(ref info) => {
                profile_scope!("gradient");

                if !info.gradient.is_valid() {
                    return;
                }

                let (mut layout, unsnapped_rect, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                let mut tile_size = process_repeat_size(
                    &layout.rect,
                    &unsnapped_rect,
                    info.tile_size,
                );

                let mut stops = read_gradient_stops(item.gradient_stops());
                let mut start = info.gradient.start_point;
                let mut end = info.gradient.end_point;
                let flags = layout.flags;

                let optimized = optimize_linear_gradient(
                    &mut layout.rect,
                    &mut tile_size,
                    info.tile_spacing,
                    &layout.clip_rect,
                    &mut start,
                    &mut end,
                    info.gradient.extend_mode,
                    &mut stops,
                    &mut |rect, start, end, stops| {
                        let layout = LayoutPrimitiveInfo { rect: *rect, clip_rect: *rect, flags };
                        if let Some(prim_key_kind) = self.create_linear_gradient_prim(
                            &layout,
                            start,
                            end,
                            stops.to_vec(),
                            ExtendMode::Clamp,
                            rect.size(),
                            LayoutSize::zero(),
                            None,
                        ) {
                            self.add_nonshadowable_primitive(
                                spatial_node_index,
                                clip_chain_id,
                                &layout,
                                Vec::new(),
                                prim_key_kind,
                            );
                        }
                    }
                );

                if !optimized && !tile_size.ceil().is_empty() {
                    if let Some(prim_key_kind) = self.create_linear_gradient_prim(
                        &layout,
                        start,
                        end,
                        stops,
                        info.gradient.extend_mode,
                        tile_size,
                        info.tile_spacing,
                        None,
                    ) {
                        self.add_nonshadowable_primitive(
                            spatial_node_index,
                            clip_chain_id,
                            &layout,
                            Vec::new(),
                            prim_key_kind,
                        );
                    }
                }
            }
            DisplayItem::RadialGradient(ref info) => {
                profile_scope!("radial");

                if !info.gradient.is_valid() {
                    return;
                }

                let (mut layout, unsnapped_rect, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                let mut center = info.gradient.center;

                let stops = read_gradient_stops(item.gradient_stops());

                let mut tile_size = process_repeat_size(
                    &layout.rect,
                    &unsnapped_rect,
                    info.tile_size,
                );

                let mut prim_rect = layout.rect;
                let mut tile_spacing = info.tile_spacing;
                optimize_radial_gradient(
                    &mut prim_rect,
                    &mut tile_size,
                    &mut center,
                    &mut tile_spacing,
                    &layout.clip_rect,
                    info.gradient.radius,
                    info.gradient.end_offset,
                    info.gradient.extend_mode,
                    &stops,
                    &mut |solid_rect, color| {
                        self.add_nonshadowable_primitive(
                            spatial_node_index,
                            clip_chain_id,
                            &LayoutPrimitiveInfo {
                                rect: *solid_rect,
                                .. layout
                            },
                            Vec::new(),
                            PrimitiveKeyKind::Rectangle { color: PropertyBinding::Value(color) },
                        );
                    }
                );

                // TODO: create_radial_gradient_prim already calls
                // this, but it leaves the info variable that is
                // passed to add_nonshadowable_primitive unmodified
                // which can cause issues.
                simplify_repeated_primitive(&tile_size, &mut tile_spacing, &mut prim_rect);

                if !tile_size.ceil().is_empty() {
                    layout.rect = prim_rect;
                    let prim_key_kind = self.create_radial_gradient_prim(
                        &layout,
                        center,
                        info.gradient.start_offset * info.gradient.radius.width,
                        info.gradient.end_offset * info.gradient.radius.width,
                        info.gradient.radius.width / info.gradient.radius.height,
                        stops,
                        info.gradient.extend_mode,
                        tile_size,
                        tile_spacing,
                        None,
                    );

                    self.add_nonshadowable_primitive(
                        spatial_node_index,
                        clip_chain_id,
                        &layout,
                        Vec::new(),
                        prim_key_kind,
                    );
                }
            }
            DisplayItem::ConicGradient(ref info) => {
                profile_scope!("conic");

                if !info.gradient.is_valid() {
                    return;
                }

                let (mut layout, unsnapped_rect, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                let tile_size = process_repeat_size(
                    &layout.rect,
                    &unsnapped_rect,
                    info.tile_size,
                );

                let offset = apply_gradient_local_clip(
                    &mut layout.rect,
                    &tile_size,
                    &info.tile_spacing,
                    &layout.clip_rect,
                );
                let center = info.gradient.center + offset;

                if !tile_size.ceil().is_empty() {
                    let prim_key_kind = self.create_conic_gradient_prim(
                        &layout,
                        center,
                        info.gradient.angle,
                        info.gradient.start_offset,
                        info.gradient.end_offset,
                        item.gradient_stops(),
                        info.gradient.extend_mode,
                        tile_size,
                        info.tile_spacing,
                        None,
                    );

                    self.add_nonshadowable_primitive(
                        spatial_node_index,
                        clip_chain_id,
                        &layout,
                        Vec::new(),
                        prim_key_kind,
                    );
                }
            }
            DisplayItem::BoxShadow(ref info) => {
                profile_scope!("box_shadow");

                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.box_bounds,
                );

                self.add_box_shadow(
                    spatial_node_index,
                    clip_chain_id,
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
                profile_scope!("border");

                let (layout, _, spatial_node_index, clip_chain_id) = self.process_common_properties_with_bounds(
                    &info.common,
                    &info.bounds,
                );

                self.add_border(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    info,
                    item.gradient_stops(),
                );
            }
            DisplayItem::ImageMaskClip(ref info) => {
                profile_scope!("image_clip");

                self.add_image_mask_clip_node(
                    info.id,
                    &info.parent_space_and_clip,
                    &info.image_mask,
                    info.fill_rule,
                    item.points(),
                );
            }
            DisplayItem::RoundedRectClip(ref info) => {
                profile_scope!("rounded_clip");

                self.add_rounded_rect_clip_node(
                    info.id,
                    &info.parent_space_and_clip,
                    &info.clip,
                );
            }
            DisplayItem::RectClip(ref info) => {
                profile_scope!("rect_clip");

                self.add_rect_clip_node(
                    info.id,
                    &info.parent_space_and_clip,
                    &info.clip_rect,
                );
            }
            DisplayItem::ClipChain(ref info) => {
                profile_scope!("clip_chain");

                let parent = info.parent.map_or(ClipId::root(pipeline_id), |id| ClipId::ClipChain(id));
                let mut clips: SmallVec<[SceneClipInstance; 4]> = SmallVec::new();

                for clip_item in item.clip_chain_items() {
                    let template = self.clip_store.get_template(clip_item);
                    let instances = &self.clip_store.instances[template.clips.start as usize .. template.clips.end as usize];
                    clips.extend_from_slice(instances);
                }

                self.clip_store.register_clip_template(
                    ClipId::ClipChain(info.id),
                    parent,
                    &clips,
                );
            },
            DisplayItem::BackdropFilter(ref info) => {
                profile_scope!("backdrop");

                let (_layout, _, _spatial_node_index, _clip_chain_id) = self.process_common_properties(
                    &info.common,
                    None,
                );

                let _filters = filter_ops_for_compositing(item.filters());
                let _filter_datas = filter_datas_for_compositing(item.filter_datas());
                let _filter_primitives = filter_primitives_for_compositing(item.filter_primitives());

                /*
                self.add_backdrop_filter(
                    spatial_node_index,
                    clip_chain_id,
                    &layout,
                    filters,
                    filter_datas,
                    filter_primitives,
                );
                */
            }

            // Do nothing; these are dummy items for the display list parser
            DisplayItem::SetGradientStops |
            DisplayItem::SetFilterOps |
            DisplayItem::SetFilterData |
            DisplayItem::SetFilterPrimitives |
            DisplayItem::SetPoints => {}

            // Special items that are handled in the parent method
            DisplayItem::PushStackingContext(..) |
            DisplayItem::PushReferenceFrame(..) |
            DisplayItem::PopReferenceFrame |
            DisplayItem::PopStackingContext |
            DisplayItem::Iframe(_) => {
                unreachable!("Handled in `build_all`")
            }

            DisplayItem::ReuseItems(key) |
            DisplayItem::RetainedItems(key) => {
                unreachable!("Iterator logic error: {:?}", key);
            }

            DisplayItem::PushShadow(info) => {
                profile_scope!("push_shadow");

                let spatial_node_index = self.get_space(info.space_and_clip.spatial_id);
                let clip_chain_id = self.get_clip_chain(
                    info.space_and_clip.clip_id,
                );

                self.push_shadow(
                    info.shadow,
                    spatial_node_index,
                    clip_chain_id,
                    info.should_inflate,
                );
            }
            DisplayItem::PopAllShadows => {
                profile_scope!("pop_all_shadows");

                self.pop_all_shadows();
            }
        }
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
                    .intern(&item, || {
                        ClipInternData {
                            clip_node_kind: item.kind.node_kind(),
                            spatial_node_index: item.spatial_node_index,
                        }
                    });

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
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
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
            .intern(&prim_key, || ());

        let instance_kind = P::make_instance_kind(
            prim_key,
            prim_data_handle,
            &mut self.prim_store,
            current_offset,
        );

        PrimitiveInstance::new(
            info.clip_rect,
            instance_kind,
            clip_chain_id,
        )
    }

    pub fn add_primitive_to_hit_testing_list(
        &mut self,
        info: &LayoutPrimitiveInfo,
        spatial_node_index: SpatialNodeIndex,
        clip_id: ClipId,
        tag: ItemTag,
    ) {
        self.hit_testing_scene.add_item(
            tag,
            info,
            spatial_node_index,
            clip_id,
            &self.clip_store,
            self.interners,
        );
    }

    /// Add an already created primitive to the draw lists.
    pub fn add_primitive_to_draw_list(
        &mut self,
        prim_instance: PrimitiveInstance,
        prim_rect: LayoutRect,
        spatial_node_index: SpatialNodeIndex,
        flags: PrimitiveFlags,
    ) {
        // Add primitive to the top-most stacking context on the stack.
        if prim_instance.is_chased() {
            println!("\tadded to stacking context at {}", self.sc_stack.len());
        }

        // If we have a valid stacking context, the primitive gets added to that.
        // Otherwise, it gets added to a top-level picture cache slice.

        match self.sc_stack.last_mut() {
            Some(stacking_context) => {
                stacking_context.prim_list.add_prim(
                    prim_instance,
                    prim_rect,
                    spatial_node_index,
                    flags,
                    &mut self.prim_instances,
                );
            }
            None => {
                self.tile_cache_builder.add_prim(
                    prim_instance,
                    prim_rect,
                    spatial_node_index,
                    flags,
                    self.spatial_tree,
                    &self.clip_store,
                    self.interners,
                    &self.config,
                    &self.quality_settings,
                    self.root_iframe_clip,
                    &mut self.prim_instances,
                );
            }
        }
    }

    /// Convenience interface that creates a primitive entry and adds it
    /// to the draw list.
    fn add_nonshadowable_primitive<P>(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
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
                clip_chain_id,
            );
            self.add_prim_to_draw_list(
                info,
                spatial_node_index,
                clip_chain_id,
                prim,
            );
        }
    }

    pub fn add_primitive<P>(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
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
                spatial_node_index,
                clip_chain_id,
                info,
                clip_items,
                prim,
            );
        } else {
            debug_assert!(clip_items.is_empty(), "No per-prim clips expected for shadowed primitives");

            // There is an active shadow context. Store as a pending primitive
            // for processing during pop_all_shadows.
            self.pending_shadow_items.push_back(PendingPrimitive {
                spatial_node_index,
                clip_chain_id,
                info: *info,
                prim,
            }.into());
        }
    }

    fn add_prim_to_draw_list<P>(
        &mut self,
        info: &LayoutPrimitiveInfo,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
        prim: P,
    )
    where
        P: InternablePrimitive,
        Interners: AsMut<Interner<P>>,
    {
        let prim_instance = self.create_primitive(
            info,
            spatial_node_index,
            clip_chain_id,
            prim,
        );
        self.register_chase_primitive_by_rect(
            &info.rect,
            &prim_instance,
        );
        self.add_primitive_to_draw_list(
            prim_instance,
            info.rect,
            spatial_node_index,
            info.flags,
        );
    }

    /// If no stacking contexts are present (i.e. we are adding prims to a tile
    /// cache), set a barrier to force creation of a slice before the next prim
    fn add_tile_cache_barrier_if_needed(
        &mut self,
        slice_flags: SliceFlags,
    ) {
        if self.sc_stack.is_empty() {
            // Shadows can only exist within a stacking context
            assert!(self.pending_shadow_items.is_empty());

            self.tile_cache_builder.add_tile_cache_barrier(slice_flags);
        }
    }

    /// Push a new stacking context. Returns context that must be passed to pop_stacking_context().
    fn push_stacking_context(
        &mut self,
        composite_ops: CompositeOps,
        transform_style: TransformStyle,
        prim_flags: PrimitiveFlags,
        spatial_node_index: SpatialNodeIndex,
        clip_id: Option<ClipId>,
        requested_raster_space: RasterSpace,
        flags: StackingContextFlags,
        pipeline_id: PipelineId,
    ) -> StackingContextInfo {
        profile_scope!("push_stacking_context");

        let new_space = match requested_raster_space {
            RasterSpace::Local(_) => requested_raster_space,
            RasterSpace::Screen => match self.raster_space_stack.last() {
                Some(RasterSpace::Local(scale)) => RasterSpace::Local(*scale),
                Some(RasterSpace::Screen) | None => requested_raster_space,
            }
        };
        self.raster_space_stack.push(new_space);

        // Get the transform-style of the parent stacking context,
        // which determines if we *might* need to draw this on
        // an intermediate surface for plane splitting purposes.
        let (parent_is_3d, extra_3d_instance, plane_splitter_index) = match self.sc_stack.last_mut() {
            Some(ref mut sc) if sc.is_3d() => {
                let (flat_items_context_3d, plane_splitter_index) = match sc.context_3d {
                    Picture3DContext::In { ancestor_index, plane_splitter_index, .. } => {
                        (
                            Picture3DContext::In {
                                root_data: None,
                                ancestor_index,
                                plane_splitter_index,
                            },
                            plane_splitter_index,
                        )
                    }
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
                let extra_instance = extra_instance.map(|(_, instance)| {
                    ExtendedPrimitiveInstance {
                        instance,
                        spatial_node_index: sc.spatial_node_index,
                        flags: sc.prim_flags,
                    }
                });
                (true, extra_instance, Some(plane_splitter_index))
            },
            _ => (false, None, None),
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
            // Get the spatial node index of the containing block, which
            // defines the context of backface-visibility.
            let ancestor_index = self.containing_block_stack
                .last()
                .cloned()
                .unwrap_or(self.spatial_tree.root_reference_frame_index());

            let plane_splitter_index = plane_splitter_index.unwrap_or_else(|| {
                let index = self.plane_splitters.len();
                self.plane_splitters.push(PlaneSplitter::new());
                PlaneSplitterIndex(index)
            });

            Picture3DContext::In {
                root_data: if parent_is_3d {
                    None
                } else {
                    Some(Vec::new())
                },
                plane_splitter_index,
                ancestor_index,
            }
        } else {
            Picture3DContext::Out
        };

        // Force an intermediate surface if the stacking context has a
        // complex clip node. In the future, we may decide during
        // prepare step to skip the intermediate surface if the
        // clip node doesn't affect the stacking context rect.
        let mut blit_reason = BlitReason::empty();

        if flags.contains(StackingContextFlags::IS_BLEND_CONTAINER) {
            blit_reason |= BlitReason::ISOLATE;
        }

        // If this stacking context has any complex clips, we need to draw it
        // to an off-screen surface.
        if let Some(clip_id) = clip_id {
            if self.clip_store.has_complex_clips(clip_id) {
                blit_reason |= BlitReason::CLIP;
            }
        }

        let is_redundant = FlattenedStackingContext::is_redundant(
            flags,
            &context_3d,
            &composite_ops,
            blit_reason,
            self.sc_stack.last(),
            prim_flags,
        );

        // If stacking context is a scrollbar, force a new slice for the primitives
        // within. The stacking context will be redundant and removed by above check.
        let set_tile_cache_barrier = prim_flags.contains(PrimitiveFlags::IS_SCROLLBAR_CONTAINER);

        if set_tile_cache_barrier {
            self.add_tile_cache_barrier_if_needed(SliceFlags::IS_SCROLLBAR);
        }

        let mut sc_info = StackingContextInfo {
            pop_hit_testing_clip: false,
            pop_stacking_context: false,
            pop_containing_block: false,
            set_tile_cache_barrier,
        };

        // If this is not 3d, then it establishes an ancestor root for child 3d contexts.
        if !participating_in_3d_context {
            sc_info.pop_containing_block = true;
            self.containing_block_stack.push(spatial_node_index);
        }

        // If this stacking context is redundant, we don't care about getting a clip-chain for it.
        // However, if we _do_ have a clip, we must build it here before the `push_clip_root`
        // calls below, to ensure we get the clips for drawing this stacking context itself.
        let clip_chain_id = if is_redundant {
            ClipChainId::NONE
        }  else {
            // Get a clip-chain for this stacking context - even if the stacking context
            // itself has no clips, it's possible that there are clips to collect from
            // the previous clip-chain builder.
            let clip_id = clip_id.unwrap_or(ClipId::root(pipeline_id));
            self.clip_store.get_or_build_clip_chain_id(clip_id)
        };

        // If this has a valid clip, register with the hit-testing scene
        if let Some(clip_id) = clip_id {
            self.hit_testing_scene.push_clip(clip_id);
            sc_info.pop_hit_testing_clip = true;
        }

        // If this stacking context is redundant (prims will be pushed into
        // the parent during pop) but it has a valid clip, then we need to
        // add that clip to the current clip chain builder, so it's correctly
        // applied to any primitives within this redundant stacking context.
        // For the normal case, we start a new clip root, knowing that the
        // clip on this stacking context will be pushed onto the stack during
        // frame building.
        if is_redundant {
            self.clip_store.push_clip_root(clip_id, true);
        } else {
            self.clip_store.push_clip_root(None, false);
        }

        // If not redundant, create a stacking context to hold primitive clusters
        if !is_redundant {
            sc_info.pop_stacking_context = true;

            // Push the SC onto the stack, so we know how to handle things in
            // pop_stacking_context.
            self.sc_stack.push(FlattenedStackingContext {
                prim_list: PrimitiveList::empty(),
                prim_flags,
                spatial_node_index,
                clip_chain_id,
                composite_ops,
                blit_reason,
                transform_style,
                context_3d,
                is_redundant,
                is_backdrop_root: flags.contains(StackingContextFlags::IS_BACKDROP_ROOT),
                flags,
            });
        }

        sc_info
    }

    fn pop_stacking_context(
        &mut self,
        info: StackingContextInfo,
    ) {
        profile_scope!("pop_stacking_context");

        // Pop off current raster space (pushed unconditionally in push_stacking_context)
        self.raster_space_stack.pop().unwrap();

        // Pop off clip builder root (pushed unconditionally in push_stacking_context)
        self.clip_store.pop_clip_root();

        // If the stacking context formed a containing block, pop off the stack
        if info.pop_containing_block {
            self.containing_block_stack.pop().unwrap();
        }

        if info.set_tile_cache_barrier {
            self.add_tile_cache_barrier_if_needed(SliceFlags::empty());
        }

        // If the stacking context established a clip root, pop off the stack
        if info.pop_hit_testing_clip {
            self.hit_testing_scene.pop_clip();
        }

        // If the stacking context was otherwise redundant, early exit
        if !info.pop_stacking_context {
            return;
        }

        let stacking_context = self.sc_stack.pop().unwrap();

        // If the stacking context is a blend container, and if we're at the top level
        // of the stacking context tree, we can make this blend container into a tile
        // cache. This means that we get caching and correct scrolling invalidation for
        // root level blend containers. For these cases, the readbacks of the backdrop
        // are handled by doing partial reads of the picture cache tiles during rendering.
        if stacking_context.flags.contains(StackingContextFlags::IS_BLEND_CONTAINER) &&
           self.sc_stack.is_empty() &&
           self.tile_cache_builder.can_add_container_tile_cache() &&
           self.spatial_tree.is_root_coord_system(stacking_context.spatial_node_index)
        {
            self.tile_cache_builder.add_tile_cache(
                stacking_context.prim_list,
                stacking_context.clip_chain_id,
                self.spatial_tree,
                &self.clip_store,
                self.interners,
                &self.config,
                self.root_iframe_clip,
                SliceFlags::IS_BLEND_CONTAINER,
                &self.prim_instances,
            );

            return;
        }

        let parent_is_empty = match self.sc_stack.last() {
            Some(parent_sc) => {
                assert!(!stacking_context.is_redundant);
                parent_sc.prim_list.is_empty()
            },
            None => true,
        };

        let mut source = match stacking_context.context_3d {
            // TODO(gw): For now, as soon as this picture is in
            //           a 3D context, we draw it to an intermediate
            //           surface and apply plane splitting. However,
            //           there is a large optimization opportunity here.
            //           During culling, we can check if there is actually
            //           perspective present, and skip the plane splitting
            //           completely when that is not the case.
            Picture3DContext::In { ancestor_index, plane_splitter_index, .. } => {
                let composite_mode = Some(
                    PictureCompositeMode::Blit(BlitReason::PRESERVE3D | stacking_context.blit_reason)
                );

                // Add picture for this actual stacking context contents to render into.
                let pic_index = PictureIndex(self.prim_store.pictures
                    .alloc()
                    .init(PicturePrimitive::new_image(
                        composite_mode.clone(),
                        Picture3DContext::In { root_data: None, ancestor_index, plane_splitter_index },
                        true,
                        stacking_context.prim_flags,
                        stacking_context.prim_list,
                        stacking_context.spatial_node_index,
                        PictureOptions::default(),
                    ))
                );

                let instance = create_prim_instance(
                    pic_index,
                    composite_mode.into(),
                    ClipChainId::NONE,
                    &mut self.interners,
                );

                PictureChainBuilder::from_instance(
                    instance,
                    stacking_context.prim_flags,
                    stacking_context.spatial_node_index,
                )
            }
            Picture3DContext::Out => {
                if stacking_context.blit_reason.is_empty() {
                    PictureChainBuilder::from_prim_list(
                        stacking_context.prim_list,
                        stacking_context.prim_flags,
                        stacking_context.spatial_node_index,
                    )
                } else {
                    let composite_mode = Some(
                        PictureCompositeMode::Blit(stacking_context.blit_reason)
                    );

                    // Add picture for this actual stacking context contents to render into.
                    let pic_index = PictureIndex(self.prim_store.pictures
                        .alloc()
                        .init(PicturePrimitive::new_image(
                            composite_mode.clone(),
                            Picture3DContext::Out,
                            true,
                            stacking_context.prim_flags,
                            stacking_context.prim_list,
                            stacking_context.spatial_node_index,
                            PictureOptions::default(),
                        ))
                    );

                    let instance = create_prim_instance(
                        pic_index,
                        composite_mode.into(),
                        ClipChainId::NONE,
                        &mut self.interners,
                    );

                    PictureChainBuilder::from_instance(
                        instance,
                        stacking_context.prim_flags,
                        stacking_context.spatial_node_index,
                    )
                }
            }
        };

        // If establishing a 3d context, the `cur_instance` represents
        // a picture with all the *trailing* immediate children elements.
        // We append this to the preserve-3D picture set and make a container picture of them.
        if let Picture3DContext::In { root_data: Some(mut prims), ancestor_index, plane_splitter_index } = stacking_context.context_3d {
            let instance = source.finalize(
                ClipChainId::NONE,
                &mut self.interners,
                &mut self.prim_store,
            );

            prims.push(ExtendedPrimitiveInstance {
                instance,
                spatial_node_index: stacking_context.spatial_node_index,
                flags: stacking_context.prim_flags,
            });

            let mut prim_list = PrimitiveList::empty();
            for ext_prim in prims.drain(..) {
                prim_list.add_prim(
                    ext_prim.instance,
                    LayoutRect::zero(),
                    ext_prim.spatial_node_index,
                    ext_prim.flags,
                    &mut self.prim_instances,
                );
            }

            // This is the acttual picture representing our 3D hierarchy root.
            let pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    None,
                    Picture3DContext::In {
                        root_data: Some(Vec::new()),
                        ancestor_index,
                        plane_splitter_index,
                    },
                    true,
                    stacking_context.prim_flags,
                    prim_list,
                    stacking_context.spatial_node_index,
                    PictureOptions::default(),
                ))
            );

            let instance = create_prim_instance(
                pic_index,
                PictureCompositeKey::Identity,
                ClipChainId::NONE,
                &mut self.interners,
            );

            source = PictureChainBuilder::from_instance(
                instance,
                stacking_context.prim_flags,
                stacking_context.spatial_node_index,
            );
        }

        let has_filters = stacking_context.composite_ops.has_valid_filters();

        source = self.wrap_prim_with_filters(
            source,
            stacking_context.composite_ops.filters,
            stacking_context.composite_ops.filter_primitives,
            stacking_context.composite_ops.filter_datas,
            true,
        );

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
        if let (Some(mix_blend_mode), false) = (stacking_context.composite_ops.mix_blend_mode, parent_is_empty) {
            let parent_is_isolated = match self.sc_stack.last() {
                Some(parent_sc) => parent_sc.blit_reason.contains(BlitReason::ISOLATE),
                None => false,
            };
            if parent_is_isolated {
                let composite_mode = PictureCompositeMode::MixBlend(mix_blend_mode);

                source = source.add_picture(
                    composite_mode,
                    Picture3DContext::Out,
                    PictureOptions::default(),
                    &mut self.interners,
                    &mut self.prim_store,
                    &mut self.prim_instances,
                );
            } else {
                // If we have a mix-blend-mode, the stacking context needs to be isolated
                // to blend correctly as per the CSS spec.
                // If not already isolated, we can't correctly blend.
                warn!("found a mix-blend-mode outside a blend container, ignoring");
            }
        }

        // Set the stacking context clip on the outermost picture in the chain,
        // unless we already set it on the leaf picture.
        let cur_instance = source.finalize(
            stacking_context.clip_chain_id,
            &mut self.interners,
            &mut self.prim_store,
        );

        // The primitive instance for the remainder of flat children of this SC
        // if it's a part of 3D hierarchy but not the root of it.
        let trailing_children_instance = match self.sc_stack.last_mut() {
            // Preserve3D path (only relevant if there are no filters/mix-blend modes)
            Some(ref parent_sc) if !has_filters && parent_sc.is_3d() => {
                Some(cur_instance)
            }
            // Regular parenting path
            Some(ref mut parent_sc) => {
                parent_sc.prim_list.add_prim(
                    cur_instance,
                    LayoutRect::zero(),
                    stacking_context.spatial_node_index,
                    stacking_context.prim_flags,
                    &mut self.prim_instances,
                );
                None
            }
            // This must be the root stacking context
            None => {
                self.add_primitive_to_draw_list(
                    cur_instance,
                    LayoutRect::zero(),
                    stacking_context.spatial_node_index,
                    stacking_context.prim_flags,
                );

                None
            }
        };

        // finally, if there any outstanding 3D primitive instances,
        // find the 3D hierarchy root and add them there.
        if let Some(instance) = trailing_children_instance {
            self.add_primitive_instance_to_3d_root(ExtendedPrimitiveInstance {
                instance,
                spatial_node_index: stacking_context.spatial_node_index,
                flags: stacking_context.prim_flags,
            });
        }

        assert!(
            self.pending_shadow_items.is_empty(),
            "Found unpopped shadows when popping stacking context!"
        );
    }

    pub fn push_reference_frame(
        &mut self,
        reference_frame_id: SpatialId,
        parent_index: SpatialNodeIndex,
        pipeline_id: PipelineId,
        transform_style: TransformStyle,
        source_transform: PropertyBinding<LayoutTransform>,
        kind: ReferenceFrameKind,
        origin_in_parent_reference_frame: LayoutVector2D,
        uid: SpatialNodeUid,
    ) -> SpatialNodeIndex {
        let index = self.spatial_tree.add_reference_frame(
            parent_index,
            transform_style,
            source_transform,
            kind,
            origin_in_parent_reference_frame,
            pipeline_id,
            uid,
        );
        self.id_to_index_mapper_stack.last_mut().unwrap().add_spatial_node(reference_frame_id, index);

        index
    }

    fn push_root(
        &mut self,
        pipeline_id: PipelineId,
        viewport_size: &LayoutSize,
        instance: PipelineInstanceId,
    ) {
        if let ChasePrimitive::Id(id) = self.config.chase_primitive {
            println!("Chasing {:?} by index", id);
            register_prim_chase_id(id);
        }

        let spatial_node_index = self.push_reference_frame(
            SpatialId::root_reference_frame(pipeline_id),
            self.spatial_tree.root_reference_frame_index(),
            pipeline_id,
            TransformStyle::Flat,
            PropertyBinding::Value(LayoutTransform::identity()),
            ReferenceFrameKind::Transform {
                is_2d_scale_translation: true,
                should_snap: true,
            },
            LayoutVector2D::zero(),
            SpatialNodeUid::root_reference_frame(pipeline_id, instance),
        );

        let viewport_rect = self.snap_rect(
            &LayoutRect::from_size(*viewport_size),
            spatial_node_index,
        );

        self.add_scroll_frame(
            SpatialId::root_scroll_node(pipeline_id),
            spatial_node_index,
            ExternalScrollId(0, pipeline_id),
            pipeline_id,
            &viewport_rect,
            &viewport_rect.size(),
            ScrollFrameKind::PipelineRoot {
                is_root_pipeline: true,
            },
            LayoutVector2D::zero(),
            SpatialNodeUid::root_scroll_frame(pipeline_id, instance),
        );
    }

    fn add_image_mask_clip_node(
        &mut self,
        new_node_id: ClipId,
        space_and_clip: &SpaceAndClipInfo,
        image_mask: &ImageMask,
        fill_rule: FillRule,
        points_range: ItemRange<LayoutPoint>,
    ) {
        let spatial_node_index = self.get_space(space_and_clip.spatial_id);

        let snapped_mask_rect = self.snap_rect(
            &image_mask.rect,
            spatial_node_index,
        );
        let points: Vec<LayoutPoint> = points_range.iter().collect();

        // If any points are provided, then intern a polygon with the points and fill rule.
        let mut polygon_handle: Option<PolygonDataHandle> = None;
        if points.len() > 0 {
            let item = PolygonKey::new(&points, fill_rule);

            let handle = self
                .interners
                .polygon
                .intern(&item, || item);
            polygon_handle = Some(handle);
        }

        let item = ClipItemKey {
            kind: ClipItemKeyKind::image_mask(image_mask, snapped_mask_rect, polygon_handle),
            spatial_node_index,
        };

        let handle = self
            .interners
            .clip
            .intern(&item, || {
                ClipInternData {
                    clip_node_kind: ClipNodeKind::Complex,
                    spatial_node_index,
                }
            });

        let instance = SceneClipInstance {
            key: item,
            clip: ClipInstance::new(handle),
        };

        self.clip_store.register_clip_template(
            new_node_id,
            space_and_clip.clip_id,
            &[instance],
        );
    }

    /// Add a new rectangle clip, positioned by the spatial node in the `space_and_clip`.
    pub fn add_rect_clip_node(
        &mut self,
        new_node_id: ClipId,
        space_and_clip: &SpaceAndClipInfo,
        clip_rect: &LayoutRect,
    ) {
        let spatial_node_index = self.get_space(space_and_clip.spatial_id);

        let snapped_clip_rect = self.snap_rect(
            clip_rect,
            spatial_node_index,
        );

        let item = ClipItemKey {
            kind: ClipItemKeyKind::rectangle(snapped_clip_rect, ClipMode::Clip),
            spatial_node_index,
        };
        let handle = self
            .interners
            .clip
            .intern(&item, || {
                ClipInternData {
                    clip_node_kind: ClipNodeKind::Rectangle,
                    spatial_node_index,
                }
            });

        let instance = SceneClipInstance {
            key: item,
            clip: ClipInstance::new(handle),
        };

        self.clip_store.register_clip_template(
            new_node_id,
            space_and_clip.clip_id,
            &[instance],
        );
    }

    fn add_rounded_rect_clip_node(
        &mut self,
        new_node_id: ClipId,
        space_and_clip: &SpaceAndClipInfo,
        clip: &ComplexClipRegion,
    ) {
        let spatial_node_index = self.get_space(space_and_clip.spatial_id);

        let snapped_region_rect = self.snap_rect(
            &clip.rect,
            spatial_node_index,
        );
        let item = ClipItemKey {
            kind: ClipItemKeyKind::rounded_rect(
                snapped_region_rect,
                clip.radii,
                clip.mode,
            ),
            spatial_node_index,
        };

        let handle = self
            .interners
            .clip
            .intern(&item, || {
                ClipInternData {
                    clip_node_kind: ClipNodeKind::Complex,
                    spatial_node_index,
                }
            });

        let instance = SceneClipInstance {
            key: item,
            clip: ClipInstance::new(handle),
        };

        self.clip_store.register_clip_template(
            new_node_id,
            space_and_clip.clip_id,
            &[instance],
        );
    }

    pub fn add_scroll_frame(
        &mut self,
        new_node_id: SpatialId,
        parent_node_index: SpatialNodeIndex,
        external_id: ExternalScrollId,
        pipeline_id: PipelineId,
        frame_rect: &LayoutRect,
        content_size: &LayoutSize,
        frame_kind: ScrollFrameKind,
        external_scroll_offset: LayoutVector2D,
        uid: SpatialNodeUid,
    ) -> SpatialNodeIndex {
        let node_index = self.spatial_tree.add_scroll_frame(
            parent_node_index,
            external_id,
            pipeline_id,
            frame_rect,
            content_size,
            frame_kind,
            external_scroll_offset,
            uid,
        );
        self.id_to_index_mapper_stack.last_mut().unwrap().add_spatial_node(new_node_id, node_index);
        node_index
    }

    pub fn push_shadow(
        &mut self,
        shadow: Shadow,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
        should_inflate: bool,
    ) {
        // Store this shadow in the pending list, for processing
        // during pop_all_shadows.
        self.pending_shadow_items.push_back(ShadowItem::Shadow(PendingShadow {
            shadow,
            spatial_node_index,
            clip_chain_id,
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

                    // Add any primitives that come after this shadow in the item
                    // list to this shadow.
                    let mut prim_list = PrimitiveList::empty();
                    let blur_filter = Filter::Blur(std_deviation, std_deviation);
                    let blur_is_noop = blur_filter.is_noop();

                    for item in &items {
                        let (instance, info, spatial_node_index) = match item {
                            ShadowItem::Image(ref pending_image) => {
                                self.create_shadow_prim(
                                    &pending_shadow,
                                    pending_image,
                                    blur_is_noop,
                                )
                            }
                            ShadowItem::LineDecoration(ref pending_line_dec) => {
                                self.create_shadow_prim(
                                    &pending_shadow,
                                    pending_line_dec,
                                    blur_is_noop,
                                )
                            }
                            ShadowItem::NormalBorder(ref pending_border) => {
                                self.create_shadow_prim(
                                    &pending_shadow,
                                    pending_border,
                                    blur_is_noop,
                                )
                            }
                            ShadowItem::Primitive(ref pending_primitive) => {
                                self.create_shadow_prim(
                                    &pending_shadow,
                                    pending_primitive,
                                    blur_is_noop,
                                )
                            }
                            ShadowItem::TextRun(ref pending_text_run) => {
                                self.create_shadow_prim(
                                    &pending_shadow,
                                    pending_text_run,
                                    blur_is_noop,
                                )
                            }
                            _ => {
                                continue;
                            }
                        };

                        if blur_is_noop {
                            self.add_primitive_to_draw_list(
                                instance,
                                info.rect,
                                spatial_node_index,
                                info.flags,
                            );
                        } else {
                            prim_list.add_prim(
                                instance,
                                info.rect,
                                spatial_node_index,
                                info.flags,
                                &mut self.prim_instances,
                            );
                        }
                    }

                    // No point in adding a shadow here if there were no primitives
                    // added to the shadow.
                    if !prim_list.is_empty() {
                        // Create a picture that the shadow primitives will be added to. If the
                        // blur radius is 0, the code in Picture::prepare_for_render will
                        // detect this and mark the picture to be drawn directly into the
                        // parent picture, which avoids an intermediate surface and blur.
                        let blur_filter = Filter::Blur(std_deviation, std_deviation);
                        assert!(!blur_filter.is_noop());
                        let composite_mode = Some(PictureCompositeMode::Filter(blur_filter));
                        let composite_mode_key = composite_mode.clone().into();

                        // Pass through configuration information about whether WR should
                        // do the bounding rect inflation for text shadows.
                        let options = PictureOptions {
                            inflate_if_required: pending_shadow.should_inflate,
                        };

                        // Create the primitive to draw the shadow picture into the scene.
                        let shadow_pic_index = PictureIndex(self.prim_store.pictures
                            .alloc()
                            .init(PicturePrimitive::new_image(
                                composite_mode,
                                Picture3DContext::Out,
                                false,
                                PrimitiveFlags::IS_BACKFACE_VISIBLE,
                                prim_list,
                                pending_shadow.spatial_node_index,
                                options,
                            ))
                        );

                        let shadow_pic_key = PictureKey::new(
                            Picture { composite_mode_key },
                        );

                        let shadow_prim_data_handle = self.interners
                            .picture
                            .intern(&shadow_pic_key, || ());

                        let shadow_prim_instance = PrimitiveInstance::new(
                            LayoutRect::max_rect(),
                            PrimitiveInstanceKind::Picture {
                                data_handle: shadow_prim_data_handle,
                                pic_index: shadow_pic_index,
                                segment_instance_index: SegmentInstanceIndex::INVALID,
                            },
                            pending_shadow.clip_chain_id,
                        );

                        // Add the shadow primitive. This must be done before pushing this
                        // picture on to the shadow stack, to avoid infinite recursion!
                        self.add_primitive_to_draw_list(
                            shadow_prim_instance,
                            LayoutRect::zero(),
                            pending_shadow.spatial_node_index,
                            PrimitiveFlags::IS_BACKFACE_VISIBLE,
                        );
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

    fn create_shadow_prim<P>(
        &mut self,
        pending_shadow: &PendingShadow,
        pending_primitive: &PendingPrimitive<P>,
        blur_is_noop: bool,
    ) -> (PrimitiveInstance, LayoutPrimitiveInfo, SpatialNodeIndex)
    where
        P: InternablePrimitive + CreateShadow,
        Interners: AsMut<Interner<P>>,
    {
        // Offset the local rect and clip rect by the shadow offset. The pending
        // primitive has already been snapped, but we will need to snap the
        // shadow after translation. We don't need to worry about the size
        // changing because the shadow has the same raster space as the
        // primitive, and thus we know the size is already rounded.
        let mut info = pending_primitive.info.clone();
        info.rect = info.rect.translate(pending_shadow.shadow.offset);
        info.clip_rect = info.clip_rect.translate(pending_shadow.shadow.offset);

        // Construct and add a primitive for the given shadow.
        let shadow_prim_instance = self.create_primitive(
            &info,
            pending_primitive.spatial_node_index,
            pending_primitive.clip_chain_id,
            pending_primitive.prim.create_shadow(
                &pending_shadow.shadow,
                blur_is_noop,
                self.raster_space_stack.last().cloned().unwrap(),
            ),
        );

        (shadow_prim_instance, info, pending_primitive.spatial_node_index)
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
                pending_primitive.spatial_node_index,
                pending_primitive.clip_chain_id,
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

    pub fn add_clear_rectangle(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
        info: &LayoutPrimitiveInfo,
    ) {
        // Clear prims must be in their own picture cache slice to
        // be composited correctly.
        self.add_tile_cache_barrier_if_needed(SliceFlags::empty());

        self.add_primitive(
            spatial_node_index,
            clip_chain_id,
            info,
            Vec::new(),
            PrimitiveKeyKind::Clear,
        );

        self.add_tile_cache_barrier_if_needed(SliceFlags::empty());
    }

    pub fn add_line(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
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

        let size = get_line_decoration_size(
            &info.rect.size(),
            orientation,
            style,
            wavy_line_thickness,
        );

        let cache_key = size.map(|size| {
            // If dotted, adjust the clip rect to ensure we don't draw a final
            // partial dot.
            if style == LineStyle::Dotted {
                let clip_size = match orientation {
                    LineOrientation::Horizontal => {
                        LayoutSize::new(
                            size.width * (info.rect.width() / size.width).floor(),
                            info.rect.height(),
                        )
                    }
                    LineOrientation::Vertical => {
                        LayoutSize::new(
                            info.rect.width(),
                            size.height * (info.rect.height() / size.height).floor(),
                        )
                    }
                };
                let clip_rect = LayoutRect::from_origin_and_size(
                    info.rect.min,
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
            spatial_node_index,
            clip_chain_id,
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
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
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
                            spatial_node_index,
                            clip_chain_id,
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
                            read_gradient_stops(gradient_stops),
                            gradient.extend_mode,
                            LayoutSize::new(border.height as f32, border.width as f32),
                            LayoutSize::zero(),
                            Some(Box::new(nine_patch)),
                        ) {
                            Some(prim) => prim,
                            None => return,
                        };

                        self.add_nonshadowable_primitive(
                            spatial_node_index,
                            clip_chain_id,
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
                            read_gradient_stops(gradient_stops),
                            gradient.extend_mode,
                            LayoutSize::new(border.height as f32, border.width as f32),
                            LayoutSize::zero(),
                            Some(Box::new(nine_patch)),
                        );

                        self.add_nonshadowable_primitive(
                            spatial_node_index,
                            clip_chain_id,
                            info,
                            Vec::new(),
                            prim,
                        );
                    }
                    NinePatchBorderSource::ConicGradient(gradient) => {
                        let prim = self.create_conic_gradient_prim(
                            &info,
                            gradient.center,
                            gradient.angle,
                            gradient.start_offset,
                            gradient.end_offset,
                            gradient_stops,
                            gradient.extend_mode,
                            LayoutSize::new(border.height as f32, border.width as f32),
                            LayoutSize::zero(),
                            Some(Box::new(nine_patch)),
                        );

                        self.add_nonshadowable_primitive(
                            spatial_node_index,
                            clip_chain_id,
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
                    spatial_node_index,
                    clip_chain_id,
                );
            }
        }
    }

    pub fn create_linear_gradient_prim(
        &mut self,
        info: &LayoutPrimitiveInfo,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        stops: Vec<GradientStopKey>,
        extend_mode: ExtendMode,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
        nine_patch: Option<Box<NinePatchDescriptor>>,
    ) -> Option<LinearGradient> {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        let mut has_hard_stops = false;
        let mut is_entirely_transparent = true;
        let mut prev_stop = None;
        for stop in &stops {
            if Some(stop.offset) == prev_stop {
                has_hard_stops = true;
            }
            prev_stop = Some(stop.offset);
            if stop.color.a > 0 {
                is_entirely_transparent = false;
            }
        }

        // If all the stops have no alpha, then this
        // gradient can't contribute to the scene.
        if is_entirely_transparent {
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

        // We set a limit to the resolution at which cached gradients are rendered.
        // For most gradients this is fine but when there are hard stops this causes
        // noticeable artifacts. If so, fall back to non-cached gradients.
        let max = gradient::LINEAR_MAX_CACHED_SIZE;
        let caching_causes_artifacts = has_hard_stops && (stretch_size.width > max || stretch_size.height > max);

        let is_tiled = prim_rect.width() > stretch_size.width
         || prim_rect.height() > stretch_size.height;
        // SWGL has a fast-path that can render gradients faster than it can sample from the
        // texture cache so we disable caching in this configuration. Cached gradients are
        // faster on hardware.
        let cached = (!self.config.is_software || is_tiled) && !caching_causes_artifacts;

        Some(LinearGradient {
            extend_mode,
            start_point: sp.into(),
            end_point: ep.into(),
            stretch_size: stretch_size.into(),
            tile_spacing: tile_spacing.into(),
            stops,
            reverse_stops,
            nine_patch,
            cached,
        })
    }

    pub fn create_radial_gradient_prim(
        &mut self,
        info: &LayoutPrimitiveInfo,
        center: LayoutPoint,
        start_radius: f32,
        end_radius: f32,
        ratio_xy: f32,
        stops: Vec<GradientStopKey>,
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

    pub fn create_conic_gradient_prim(
        &mut self,
        info: &LayoutPrimitiveInfo,
        center: LayoutPoint,
        angle: f32,
        start_offset: f32,
        end_offset: f32,
        stops: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
        nine_patch: Option<Box<NinePatchDescriptor>>,
    ) -> ConicGradient {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        let stops = stops.iter().map(|stop| {
            GradientStopKey {
                offset: stop.offset,
                color: stop.color.into(),
            }
        }).collect();

        ConicGradient {
            extend_mode,
            center: center.into(),
            params: ConicGradientParams { angle, start_offset, end_offset },
            stretch_size: stretch_size.into(),
            tile_spacing: tile_spacing.into(),
            nine_patch,
            stops,
        }
    }

    pub fn add_text(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
        prim_info: &LayoutPrimitiveInfo,
        font_instance_key: &FontInstanceKey,
        text_color: &ColorF,
        glyph_range: ItemRange<GlyphInstance>,
        glyph_options: Option<GlyphOptions>,
    ) {
        let offset = self.current_offset(spatial_node_index);

        let text_run = {
            let instance_map = self.font_instances.lock().unwrap();
            let font_instance = match instance_map.get(font_instance_key) {
                Some(instance) => instance,
                None => {
                    warn!("Unknown font instance key");
                    debug!("key={:?}", font_instance_key);
                    return;
                }
            };

            // Trivial early out checks
            if font_instance.size <= FontSize::zero() {
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
            let prim_offset = prim_info.rect.min.to_vector() - offset;
            let glyphs = glyph_range
                .iter()
                .map(|glyph| {
                    GlyphInstance {
                        index: glyph.index,
                        point: glyph.point - prim_offset,
                    }
                })
                .collect();

            // Query the current requested raster space (stack handled by push/pop
            // stacking context).
            let requested_raster_space = self.raster_space_stack
                .last()
                .cloned()
                .unwrap();

            TextRun {
                glyphs: Arc::new(glyphs),
                font,
                shadow: false,
                requested_raster_space,
            }
        };

        self.add_primitive(
            spatial_node_index,
            clip_chain_id,
            prim_info,
            Vec::new(),
            text_run,
        );
    }

    pub fn add_image(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
        info: &LayoutPrimitiveInfo,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
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

        self.add_primitive(
            spatial_node_index,
            clip_chain_id,
            &info,
            Vec::new(),
            Image {
                key: image_key,
                tile_spacing: tile_spacing.into(),
                stretch_size: stretch_size.into(),
                color: color.into(),
                image_rendering,
                alpha_type,
            },
        );
    }

    pub fn add_yuv_image(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
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
            spatial_node_index,
            clip_chain_id,
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

    fn add_primitive_instance_to_3d_root(
        &mut self,
        prim: ExtendedPrimitiveInstance,
    ) {
        // find the 3D root and append to the children list
        for sc in self.sc_stack.iter_mut().rev() {
            match sc.context_3d {
                Picture3DContext::In { root_data: Some(ref mut prims), .. } => {
                    prims.push(prim);
                    break;
                }
                Picture3DContext::In { .. } => {}
                Picture3DContext::Out => panic!("Unable to find 3D root"),
            }
        }
    }

    #[allow(dead_code)]
    pub fn add_backdrop_filter(
        &mut self,
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId,
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

        let mut instance = self.create_primitive(
            info,
            // TODO(cbrewster): This is a bit of a hack to help figure out the correct sizing of the backdrop
            // region. By makings sure to include this, the clip chain instance computes the correct clip rect,
            // but we don't actually apply the filtered backdrop clip yet (this is done to the last instance in
            // the filter chain below).
            backdrop_spatial_node_index,
            clip_chain_id,
            Backdrop {
                pic_index: backdrop_pic_index,
                spatial_node_index,
                border_rect: info.rect.into(),
            },
        );

        // We will append the filtered backdrop to the backdrop root, but we need to
        // make sure all clips between the current stacking context and backdrop root
        // are taken into account. So we wrap the backdrop filter instance with a picture with
        // a clip for each stacking context.
        for stacking_context in self.sc_stack.iter().rev().take_while(|sc| !sc.is_backdrop_root) {
            let clip_chain_id = stacking_context.clip_chain_id;
            let prim_flags = stacking_context.prim_flags;
            let composite_mode = None;

            let mut prim_list = PrimitiveList::empty();
            prim_list.add_prim(
                instance,
                LayoutRect::zero(),
                backdrop_spatial_node_index,
                prim_flags,
                &mut self.prim_instances,
            );

            backdrop_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    composite_mode.clone(),
                    Picture3DContext::Out,
                    true,
                    prim_flags,
                    prim_list,
                    backdrop_spatial_node_index,
                    PictureOptions {
                       inflate_if_required: false,
                    },
                ))
            );

            instance = create_prim_instance(
                backdrop_pic_index,
                composite_mode.into(),
                clip_chain_id,
                &mut self.interners,
            );
        }

        let mut source = PictureChainBuilder::from_instance(
            instance,
            info.flags,
            backdrop_spatial_node_index,
        );

        source = self.wrap_prim_with_filters(
            source,
            filters,
            filter_primitives,
            filter_datas,
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

            source = self.wrap_prim_with_filters(
                source,
                filters,
                filter_primitives,
                filter_datas,
                false,
            );
        }

        let filtered_instance = source.finalize(
            clip_chain_id,
            &mut self.interners,
            &mut self.prim_store,
        );

        self.sc_stack
            .iter_mut()
            .rev()
            .find(|sc| sc.is_backdrop_root)
            .unwrap()
            .prim_list
            .add_prim(
                filtered_instance,
                LayoutRect::zero(),
                backdrop_spatial_node_index,
                info.flags,
                &mut self.prim_instances,
            );
    }

    /// Create pictures for each stacking context rendered into their parents, down to the nearest
    /// backdrop root until we have a picture that represents the contents of all primitives added
    /// since the backdrop root
    #[allow(dead_code)]
    pub fn cut_backdrop_picture(&mut self) -> Option<PictureIndex> {
        let mut flattened_items = None;
        let mut backdrop_root =  None;
        let mut spatial_node_index = SpatialNodeIndex::INVALID;
        let mut prim_flags = PrimitiveFlags::default();
        for sc in self.sc_stack.iter_mut().rev() {
            // Add child contents to parent stacking context
            if let Some((_, flattened_instance)) = flattened_items.take() {
                sc.prim_list.add_prim(
                    flattened_instance,
                    LayoutRect::zero(),
                    spatial_node_index,
                    prim_flags,
                    &mut self.prim_instances,
                );
            }
            flattened_items = sc.cut_item_sequence(
                &mut self.prim_store,
                &mut self.interners,
                None,
                Picture3DContext::Out,
            );
            spatial_node_index = sc.spatial_node_index;
            prim_flags = sc.prim_flags;
            if sc.is_backdrop_root {
                backdrop_root = Some(sc);
                break;
            }
        }

        let (pic_index, instance) = flattened_items?;
        self.prim_store.pictures[pic_index.0].composite_mode = Some(PictureCompositeMode::Blit(BlitReason::BACKDROP));
        backdrop_root.expect("no backdrop root found")
            .prim_list
            .add_prim(
                instance,
                LayoutRect::zero(),
                spatial_node_index,
                prim_flags,
                &mut self.prim_instances,
            );

        Some(pic_index)
    }

    #[must_use]
    fn wrap_prim_with_filters(
        &mut self,
        mut source: PictureChainBuilder,
        mut filter_ops: Vec<Filter>,
        mut filter_primitives: Vec<FilterPrimitive>,
        filter_datas: Vec<FilterData>,
        inflate_if_required: bool,
    ) -> PictureChainBuilder {
        // TODO(cbrewster): Currently CSS and SVG filters live side by side in WebRender, but unexpected results will
        // happen if they are used simulataneously. Gecko only provides either filter ops or filter primitives.
        // At some point, these two should be combined and CSS filters should be expressed in terms of SVG filters.
        assert!(filter_ops.is_empty() || filter_primitives.is_empty(),
            "Filter ops and filter primitives are not allowed on the same stacking context.");

        // For each filter, create a new image with that composite mode.
        let mut current_filter_data_index = 0;
        for filter in &mut filter_ops {
            let composite_mode = match filter {
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
                _ => {
                    if filter.is_noop() {
                        continue;
                    } else {
                        PictureCompositeMode::Filter(filter.clone())
                    }
                }
            };

            source = source.add_picture(
                composite_mode,
                Picture3DContext::Out,
                PictureOptions { inflate_if_required },
                &mut self.interners,
                &mut self.prim_store,
                &mut self.prim_instances,
            );
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

            source = source.add_picture(
                composite_mode,
                Picture3DContext::Out,
                PictureOptions { inflate_if_required },
                &mut self.interners,
                &mut self.prim_store,
                &mut self.prim_instances,
            );
        }

        source
    }
}


pub trait CreateShadow {
    fn create_shadow(
        &self,
        shadow: &Shadow,
        blur_is_noop: bool,
        current_raster_space: RasterSpace,
    ) -> Self;
}

pub trait IsVisible {
    fn is_visible(&self) -> bool;
}

/// A primitive instance + some extra information about the primitive. This is
/// stored when constructing 3d rendering contexts, which involve cutting
/// primitive lists.
struct ExtendedPrimitiveInstance {
    instance: PrimitiveInstance,
    spatial_node_index: SpatialNodeIndex,
    flags: PrimitiveFlags,
}

/// Internal tracking information about the currently pushed stacking context.
/// Used to track what operations need to happen when a stacking context is popped.
struct StackingContextInfo {
    /// If true, pop an entry from the hit-testing scene.
    pop_hit_testing_clip: bool,
    /// If true, pop and entry from the containing block stack.
    pop_containing_block: bool,
    /// If true, pop an entry from the flattened stacking context stack.
    pop_stacking_context: bool,
    /// If true, set a tile cache barrier when popping the stacking context.
    set_tile_cache_barrier: bool,
}

/// Properties of a stacking context that are maintained
/// during creation of the scene. These structures are
/// not persisted after the initial scene build.
struct FlattenedStackingContext {
    /// The list of primitive instances added to this stacking context.
    prim_list: PrimitiveList,

    /// Primitive instance flags for compositing this stacking context
    prim_flags: PrimitiveFlags,

    /// The positioning node for this stacking context
    spatial_node_index: SpatialNodeIndex,

    /// The clip chain for this stacking context
    clip_chain_id: ClipChainId,

    /// The list of filters / mix-blend-mode for this
    /// stacking context.
    composite_ops: CompositeOps,

    /// Bitfield of reasons this stacking context needs to
    /// be an offscreen surface.
    blit_reason: BlitReason,

    /// CSS transform-style property.
    transform_style: TransformStyle,

    /// Defines the relationship to a preserve-3D hiearachy.
    context_3d: Picture3DContext<ExtendedPrimitiveInstance>,

    /// True if this stacking context is a backdrop root.
    #[allow(dead_code)]
    is_backdrop_root: bool,

    /// True if this stacking context is redundant (i.e. doesn't require a surface)
    is_redundant: bool,

    /// Flags identifying the type of container (among other things) this stacking context is
    flags: StackingContextFlags,
}

impl FlattenedStackingContext {
    /// Return true if the stacking context has a valid preserve-3d property
    pub fn is_3d(&self) -> bool {
        self.transform_style == TransformStyle::Preserve3D && self.composite_ops.is_empty()
    }

    /// Return true if the stacking context isn't needed.
    pub fn is_redundant(
        sc_flags: StackingContextFlags,
        context_3d: &Picture3DContext<ExtendedPrimitiveInstance>,
        composite_ops: &CompositeOps,
        blit_reason: BlitReason,
        parent: Option<&FlattenedStackingContext>,
        prim_flags: PrimitiveFlags,
    ) -> bool {
        // If this is a backdrop or blend container, it's needed
        if sc_flags.intersects(StackingContextFlags::IS_BACKDROP_ROOT | StackingContextFlags::IS_BLEND_CONTAINER) {
            return false;
        }

        // Any 3d context is required
        if let Picture3DContext::In { .. } = context_3d {
            return false;
        }

        // If any filters are present that affect the output
        if composite_ops.has_valid_filters() {
            return false;
        }

        // We can skip mix-blend modes if they are the first primitive in a stacking context,
        // see pop_stacking_context for a full explanation.
        if composite_ops.mix_blend_mode.is_some() {
            if let Some(parent) = parent {
                if !parent.prim_list.is_empty() {
                    return false;
                }
            }
        }

        // If backface visibility is explicitly set.
        if !prim_flags.contains(PrimitiveFlags::IS_BACKFACE_VISIBLE) {
            return false;
        }

        // If need to isolate in surface due to clipping / mix-blend-mode
        if !blit_reason.is_empty() {
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
        if self.prim_list.is_empty() {
            return None
        }

        let pic_index = PictureIndex(prim_store.pictures
            .alloc()
            .init(PicturePrimitive::new_image(
                composite_mode.clone(),
                flat_items_context_3d,
                true,
                self.prim_flags,
                mem::replace(&mut self.prim_list, PrimitiveList::empty()),
                self.spatial_node_index,
                PictureOptions::default(),
            ))
        );

        let prim_instance = create_prim_instance(
            pic_index,
            composite_mode.into(),
            self.clip_chain_id,
            interners,
        );

        Some((pic_index, prim_instance))
    }
}

/// A primitive that is added while a shadow context is
/// active is stored as a pending primitive and only
/// added to pictures during pop_all_shadows.
pub struct PendingPrimitive<T> {
    spatial_node_index: SpatialNodeIndex,
    clip_chain_id: ClipChainId,
    info: LayoutPrimitiveInfo,
    prim: T,
}

/// As shadows are pushed, they are stored as pending
/// shadows, and handled at once during pop_all_shadows.
pub struct PendingShadow {
    shadow: Shadow,
    should_inflate: bool,
    spatial_node_index: SpatialNodeIndex,
    clip_chain_id: ClipChainId,
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
    clip_chain_id: ClipChainId,
    interners: &mut Interners,
) -> PrimitiveInstance {
    let pic_key = PictureKey::new(
        Picture { composite_mode_key },
    );

    let data_handle = interners
        .picture
        .intern(&pic_key, || ());

    PrimitiveInstance::new(
        LayoutRect::max_rect(),
        PrimitiveInstanceKind::Picture {
            data_handle,
            pic_index,
            segment_instance_index: SegmentInstanceIndex::INVALID,
        },
        clip_chain_id,
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
    input_filter_primitives.iter().map(|primitive| primitive).collect()
}

fn process_repeat_size(
    snapped_rect: &LayoutRect,
    unsnapped_rect: &LayoutRect,
    repeat_size: LayoutSize,
) -> LayoutSize {
    // FIXME(aosmond): The tile size is calculated based on several parameters
    // during display list building. It may produce a slightly different result
    // than the bounds due to floating point error accumulation, even though in
    // theory they should be the same. We do a fuzzy check here to paper over
    // that. It may make more sense to push the original parameters into scene
    // building and let it do a saner calculation with more information (e.g.
    // the snapped values).
    const EPSILON: f32 = 0.001;
    LayoutSize::new(
        if repeat_size.width.approx_eq_eps(&unsnapped_rect.width(), &EPSILON) {
            snapped_rect.width()
        } else {
            repeat_size.width
        },
        if repeat_size.height.approx_eq_eps(&unsnapped_rect.height(), &EPSILON) {
            snapped_rect.height()
        } else {
            repeat_size.height
        },
    )
}

fn read_gradient_stops(stops: ItemRange<GradientStop>) -> Vec<GradientStopKey> {
    stops.iter().map(|stop| {
        GradientStopKey {
            offset: stop.offset,
            color: stop.color.into(),
        }
    }).collect()
}

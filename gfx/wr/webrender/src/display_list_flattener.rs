/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderDetails, BorderDisplayItem, BuiltDisplayListIter, ClipAndScrollInfo};
use api::{ClipId, ColorF, ComplexClipRegion, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{DisplayItemRef, ExtendMode, ExternalScrollId, AuHelpers};
use api::{FilterOp, FontInstanceKey, GlyphInstance, GlyphOptions, RasterSpace, GradientStop};
use api::{IframeDisplayItem, ImageKey, ImageRendering, ItemRange, LayoutPoint, ColorDepth};
use api::{LayoutPrimitiveInfo, LayoutRect, LayoutSize, LayoutTransform, LayoutVector2D};
use api::{LineOrientation, LineStyle, NinePatchBorderSource, PipelineId};
use api::{PropertyBinding, ReferenceFrame, ScrollFrameDisplayItem, ScrollSensitivity};
use api::{Shadow, SpecificDisplayItem, StackingContext, StickyFrameDisplayItem, TexelRect};
use api::{ClipMode, TransformStyle, YuvColorSpace, YuvData};
use app_units::Au;
use clip::{ClipChainId, ClipRegion, ClipItemKey, ClipStore, ClipItemSceneData};
use clip_scroll_tree::{ROOT_SPATIAL_NODE_INDEX, ClipScrollTree, SpatialNodeIndex};
use frame_builder::{ChasePrimitive, FrameBuilder, FrameBuilderConfig};
use glyph_rasterizer::FontInstance;
use hit_test::{HitTestingItem, HitTestingRun};
use image::simplify_repeated_primitive;
use intern::{Handle, Internable};
use internal_types::{FastHashMap, FastHashSet};
use picture::{Picture3DContext, PictureCompositeMode, PicturePrimitive, PrimitiveList};
use prim_store::{PrimitiveInstance, PrimitiveKeyKind};
use prim_store::{PrimitiveKey, PrimitiveSceneData, PrimitiveInstanceKind, NinePatchDescriptor};
use prim_store::{PrimitiveDataHandle, PrimitiveStore, PrimitiveStoreStats, LineDecorationCacheKey};
use prim_store::{ScrollNodeAndClipChain, PictureIndex, register_prim_chase_id, get_line_decoration_sizes};
use prim_store::gradient::{GradientStopKey, LinearGradient, RadialGradient, RadialGradientParams};
use prim_store::text_run::TextRun;
use render_backend::{DocumentView};
use resource_cache::{FontInstanceMap, ImageRequest};
use scene::{Scene, ScenePipeline, StackingContextHelpers};
use scene_builder::{DocumentResources, InternerMut};
use spatial_node::{StickyFrameInfo, ScrollFrameKind, SpatialNodeType};
use std::{f32, mem, usize};
use std::collections::vec_deque::VecDeque;
use tiling::{CompositeOps};
use util::{MaxRect, VecHelper};

#[derive(Debug, Copy, Clone)]
struct ClipNode {
    id: ClipChainId,
    count: usize,
}

impl ClipNode {
    fn new(id: ClipChainId, count: usize) -> ClipNode {
        ClipNode {
            id,
            count,
        }
    }
}

/// A data structure that keeps track of mapping between API ClipIds and the indices used
/// internally in the ClipScrollTree to avoid having to do HashMap lookups. ClipIdToIndexMapper is
/// responsible for mapping both ClipId to ClipChainIndex and ClipId to SpatialNodeIndex.
#[derive(Default)]
pub struct ClipIdToIndexMapper {
    clip_node_map: FastHashMap<ClipId, ClipNode>,
    spatial_node_map: FastHashMap<ClipId, SpatialNodeIndex>,
}

impl ClipIdToIndexMapper {
    pub fn add_clip_chain(
        &mut self,
        id: ClipId,
        index: ClipChainId,
        count: usize,
    ) {
        let _old_value = self.clip_node_map.insert(id, ClipNode::new(index, count));
        debug_assert!(_old_value.is_none());
    }

    pub fn map_to_parent_clip_chain(
        &mut self,
        id: ClipId,
        parent_id: &ClipId,
    ) {
        let parent_node = self.clip_node_map[parent_id];
        self.add_clip_chain(id, parent_node.id, parent_node.count);
    }

    pub fn map_spatial_node(&mut self, id: ClipId, index: SpatialNodeIndex) {
        let _old_value = self.spatial_node_map.insert(id, index);
        debug_assert!(_old_value.is_none());
    }

    fn get_clip_node(&self, id: &ClipId) -> ClipNode {
        self.clip_node_map[id]
    }

    pub fn get_clip_chain_id(&self, id: &ClipId) -> ClipChainId {
        self.clip_node_map[id].id
    }

    pub fn get_spatial_node_index(&self, id: ClipId) -> SpatialNodeIndex {
        match id {
            ClipId::Clip(..) |
            ClipId::Spatial(..) => {
                self.spatial_node_map[&id]
            }
            ClipId::ClipChain(_) => panic!("Tried to use ClipChain as scroll node."),
        }
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

    /// The data structure that converting between ClipId and the various index
    /// types that the ClipScrollTree uses.
    id_to_index_mapper: ClipIdToIndexMapper,

    /// A stack of stacking context properties.
    sc_stack: Vec<FlattenedStackingContext>,

    /// Maintains state for any currently active shadows
    pending_shadow_items: VecDeque<ShadowItem>,

    /// The stack keeping track of the root clip chains associated with pipelines.
    pipeline_clip_chain_stack: Vec<ClipChainId>,

    /// The store of primitives.
    pub prim_store: PrimitiveStore,

    /// Information about all primitives involved in hit testing.
    pub hit_testing_runs: Vec<HitTestingRun>,

    /// The store which holds all complex clipping information.
    pub clip_store: ClipStore,

    /// The configuration to use for the FrameBuilder. We consult this in
    /// order to determine the default font.
    pub config: FrameBuilderConfig,

    /// Reference to the document resources, which contains
    /// shared (interned) data between display lists.
    resources: &'a mut DocumentResources,

    /// The root picture index for this flattener. This is the picture
    /// to start the culling phase from.
    pub root_pic_index: PictureIndex,

    /// TODO(gw): This is a complete hack that relies on knowledge of
    ///           what the Gecko display list looks like. It's used
    ///           for now to work out which scroll root to use to
    ///           create the picture cache for the content. It's only
    ///           ever used if picture caching is enabled in the
    ///           RendererOptions struct. We will need to work out
    ///           a better API to avoid this, before we enable it
    ///           for all users. Another alternative is that this
    ///           will disappear itself when document splitting is used.
    picture_cache_scroll_root: Option<SpatialNodeIndex>,
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
        resources: &mut DocumentResources,
        prim_store_stats: &PrimitiveStoreStats,
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
            id_to_index_mapper: ClipIdToIndexMapper::default(),
            hit_testing_runs: Vec::new(),
            pending_shadow_items: VecDeque::new(),
            sc_stack: Vec::new(),
            pipeline_clip_chain_stack: vec![ClipChainId::NONE],
            prim_store: PrimitiveStore::new(&prim_store_stats),
            clip_store: ClipStore::new(),
            resources,
            root_pic_index: PictureIndex(0),
            picture_cache_scroll_root: None,
        };

        flattener.push_root(
            root_pipeline_id,
            &root_pipeline.viewport_size,
            &root_pipeline.content_size,
        );

        flattener.flatten_root(
            root_pipeline,
            &root_pipeline.viewport_size,
        );

        debug_assert!(flattener.sc_stack.is_empty());

        // If picture caching is enabled, splice up the root
        // stacking context to enable correct surface caching.
        flattener.setup_picture_caching(
            root_pipeline_id,
        );

        new_scene.root_pipeline_id = Some(root_pipeline_id);
        new_scene.pipeline_epochs = scene.pipeline_epochs.clone();
        new_scene.pipelines = scene.pipelines.clone();

        FrameBuilder::with_display_list_flattener(
            view.inner_rect,
            background_color,
            view.window_size,
            flattener,
        )
    }

    /// Cut the primitives in the root stacking context based on the picture
    /// caching scroll root. This is a temporary solution for the initial
    /// implementation of picture caching. We need to work out the specifics
    /// of how WR should decide (or Gecko should communicate) where the main
    /// content frame is that should get surface caching.
    fn setup_picture_caching(
        &mut self,
        root_pipeline_id: PipelineId,
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

        // See if we found a scroll root for the cached surface root.
        if let Some(picture_cache_scroll_root) = self.picture_cache_scroll_root {
            // Get the list of existing primitives in the main stacking context.
            let mut old_prim_list = mem::replace(
                &mut self.prim_store.pictures[self.root_pic_index.0].prim_list,
                PrimitiveList::empty(),
            );

            // Find the first primitive which has the desired scroll root.
            let first_index = old_prim_list.prim_instances.iter().position(|instance| {
                let scroll_root = self.find_scroll_root(
                    instance.spatial_node_index,
                );

                scroll_root == picture_cache_scroll_root
            }).unwrap_or(old_prim_list.prim_instances.len());

            // Split off the preceding primtives.
            let mut remaining_prims = old_prim_list.prim_instances.split_off(first_index);

            // Find the first primitive in reverse order that is not the root scroll node.
            let last_index = remaining_prims.iter().rposition(|instance| {
                let scroll_root = self.find_scroll_root(
                    instance.spatial_node_index,
                );

                scroll_root != ROOT_SPATIAL_NODE_INDEX
            }).unwrap_or(remaining_prims.len() - 1);

            let preceding_prims = old_prim_list.prim_instances;
            let trailing_prims = remaining_prims.split_off(last_index + 1);

            let prim_list = PrimitiveList::new(
                remaining_prims,
                &self.resources,
            );

            // Now, create a picture with tile caching enabled that will hold all
            // of the primitives selected as belonging to the main scroll root.
            let prim_key = PrimitiveKey::new(
                true,
                LayoutSize::zero(),
                LayoutRect::max_rect(),
                PrimitiveKeyKind::Unused,
            );

            let primitive_data_handle = self.resources
                .prim_interner
                .intern(&prim_key, || {
                    PrimitiveSceneData {
                        prim_relative_clip_rect: LayoutRect::max_rect(),
                        prim_size: LayoutSize::zero(),
                        is_backface_visible: true,
                    }
                }
            );

            let pic_index = self.prim_store.pictures.alloc().init(PicturePrimitive::new_image(
                Some(PictureCompositeMode::TileCache { clear_color: ColorF::new(1.0, 1.0, 1.0, 1.0) }),
                Picture3DContext::Out,
                root_pipeline_id,
                None,
                true,
                RasterSpace::Screen,
                prim_list,
                picture_cache_scroll_root,
                LayoutRect::max_rect(),
                &self.clip_store,
            ));

            let instance = PrimitiveInstance::new(
                LayoutPoint::zero(),
                PrimitiveInstanceKind::Picture {
                    data_handle: primitive_data_handle,
                    pic_index: PictureIndex(pic_index)
                },
                ClipChainId::NONE,
                picture_cache_scroll_root,
            );

            // This contains the tile caching picture, with preceding and
            // trailing primitives outside the main scroll root.
            let mut new_prim_list = preceding_prims;
            new_prim_list.push(instance);
            new_prim_list.extend(trailing_prims);

            // Finally, store the sliced primitive list in the root picture.
            self.prim_store.pictures[self.root_pic_index.0].prim_list = PrimitiveList::new(
                new_prim_list,
                &self.resources,
            );
        }
    }

    /// Find the spatial node that is the scroll root for a given
    /// spatial node.
    fn find_scroll_root(
        &self,
        spatial_node_index: SpatialNodeIndex,
    ) -> SpatialNodeIndex {
        let mut scroll_root = ROOT_SPATIAL_NODE_INDEX;
        let mut node_index = spatial_node_index;

        while node_index != ROOT_SPATIAL_NODE_INDEX {
            let node = &self.clip_scroll_tree.spatial_nodes[node_index.0];
            match node.node_type {
                SpatialNodeType::ReferenceFrame(..) |
                SpatialNodeType::StickyFrame(..) => {
                    // TODO(gw): In future, we may need to consider sticky frames.
                }
                SpatialNodeType::ScrollFrame(ref info) => {
                    // If we found an explicit scroll root, store that
                    // and keep looking up the tree.
                    if let ScrollFrameKind::Explicit = info.frame_kind {
                        scroll_root = node_index;
                    }
                }
            }
            node_index = node.parent.expect("unable to find parent node");
        }

        scroll_root
    }

    fn get_complex_clips(
        &self,
        pipeline_id: PipelineId,
        complex_clips: ItemRange<ComplexClipRegion>,
    ) -> impl 'a + Iterator<Item = ComplexClipRegion> {
        //Note: we could make this a bit more complex to early out
        // on `complex_clips.is_empty()` if it's worth it
        self.scene
            .get_display_list_for_pipeline(pipeline_id)
            .get(complex_clips)
    }

    fn get_clip_chain_items(
        &self,
        pipeline_id: PipelineId,
        items: ItemRange<ClipId>,
    ) -> impl 'a + Iterator<Item = ClipId> {
        self.scene
            .get_display_list_for_pipeline(pipeline_id)
            .get(items)
    }

    fn flatten_root(
        &mut self,
        pipeline: &'a ScenePipeline,
        frame_size: &LayoutSize,
    ) {
        let pipeline_id = pipeline.pipeline_id;
        let reference_frame_info = self.simple_scroll_and_clip_chain(
            &ClipId::root_reference_frame(pipeline_id),
        );

        let root_scroll_node = ClipId::root_scroll_node(pipeline_id);

        self.push_stacking_context(
            pipeline_id,
            CompositeOps::default(),
            TransformStyle::Flat,
            true,
            true,
            root_scroll_node,
            None,
            RasterSpace::Screen,
        );

        // For the root pipeline, there's no need to add a full screen rectangle
        // here, as it's handled by the framebuffer clear.
        // TODO(gw): In future, we can probably remove this code completely and handle
        //           it as part of the tile cache background color clearing.
        if self.scene.root_pipeline_id != Some(pipeline_id) {
            if let Some(pipeline) = self.scene.pipelines.get(&pipeline_id) {
                if let Some(bg_color) = pipeline.background_color {
                    let root_bounds = LayoutRect::new(LayoutPoint::zero(), *frame_size);
                    let info = LayoutPrimitiveInfo::new(root_bounds);
                    self.add_solid_rectangle(
                        reference_frame_info,
                        &info,
                        bg_color,
                    );
                }
            }
        }

        self.flatten_items(&mut pipeline.display_list.iter(), pipeline_id, LayoutVector2D::zero());

        self.pop_stacking_context();
    }

    fn flatten_items(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        reference_frame_relative_offset: LayoutVector2D,
    ) {
        loop {
            let subtraversal = {
                let item = match traversal.next() {
                    Some(item) => item,
                    None => break,
                };

                if SpecificDisplayItem::PopReferenceFrame == *item.item() {
                    return;
                }

                if SpecificDisplayItem::PopStackingContext == *item.item() {
                    return;
                }

                self.flatten_item(
                    item,
                    pipeline_id,
                    reference_frame_relative_offset,
                )
            };

            // If flatten_item created a sub-traversal, we need `traversal` to have the
            // same state as the completed subtraversal, so we reinitialize it here.
            if let Some(subtraversal) = subtraversal {
                *traversal = subtraversal;
            }
        }
    }

    fn flatten_sticky_frame(
        &mut self,
        item: &DisplayItemRef,
        info: &StickyFrameDisplayItem,
        clip_and_scroll: &ScrollNodeAndClipChain,
        parent_id: &ClipId,
        reference_frame_relative_offset: &LayoutVector2D,
    ) {
        let frame_rect = item.rect().translate(reference_frame_relative_offset);
        let sticky_frame_info = StickyFrameInfo::new(
            frame_rect,
            info.margins,
            info.vertical_offset_bounds,
            info.horizontal_offset_bounds,
            info.previously_applied_offset,
        );

        let index = self.clip_scroll_tree.add_sticky_frame(
            clip_and_scroll.spatial_node_index, /* parent id */
            sticky_frame_info,
            info.id.pipeline_id(),
        );
        self.id_to_index_mapper.map_spatial_node(info.id, index);
        self.id_to_index_mapper.map_to_parent_clip_chain(info.id, parent_id);
    }

    fn flatten_scroll_frame(
        &mut self,
        item: &DisplayItemRef,
        info: &ScrollFrameDisplayItem,
        pipeline_id: PipelineId,
        clip_and_scroll_ids: &ClipAndScrollInfo,
        reference_frame_relative_offset: &LayoutVector2D,
    ) {
        let complex_clips = self.get_complex_clips(pipeline_id, item.complex_clip().0);
        let clip_region = ClipRegion::create_for_clip_node(
            *item.clip_rect(),
            complex_clips,
            info.image_mask,
            reference_frame_relative_offset,
        );
        // Just use clip rectangle as the frame rect for this scroll frame.
        // This is useful when calculating scroll extents for the
        // SpatialNode::scroll(..) API as well as for properly setting sticky
        // positioning offsets.
        let frame_rect = item.clip_rect().translate(reference_frame_relative_offset);
        let content_rect = item.rect().translate(reference_frame_relative_offset);

        debug_assert!(info.clip_id != info.scroll_frame_id);

        self.add_clip_node(info.clip_id, clip_and_scroll_ids, clip_region);

        let node_index = self.add_scroll_frame(
            info.scroll_frame_id,
            info.clip_id,
            info.external_id,
            pipeline_id,
            &frame_rect,
            &content_rect.size,
            info.scroll_sensitivity,
            ScrollFrameKind::Explicit,
        );

        // TODO(gw): See description of picture_cache_scroll_root field for information
        //           about this temporary hack. What it's trying to identify is the first
        //           scroll root within the first iframe that we encounter in the display
        //           list.
        if self.picture_cache_scroll_root.is_none() && pipeline_id != self.scene.root_pipeline_id.unwrap() {
            self.picture_cache_scroll_root = Some(node_index);
        }
    }

    fn flatten_reference_frame(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        item: &DisplayItemRef,
        reference_frame: &ReferenceFrame,
        clip_and_scroll_ids: &ClipAndScrollInfo,
        reference_frame_relative_offset: LayoutVector2D,
    ) {
        self.push_reference_frame(
            reference_frame.id,
            Some(clip_and_scroll_ids.scroll_node_id),
            clip_and_scroll_ids.clip_node_id,
            pipeline_id,
            reference_frame.transform,
            reference_frame.perspective,
            reference_frame_relative_offset + item.rect().origin.to_vector(),
        );

        self.flatten_items(traversal, pipeline_id, LayoutVector2D::zero());
    }

    fn flatten_stacking_context(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        item: &DisplayItemRef,
        stacking_context: &StackingContext,
        scroll_node_id: ClipId,
        reference_frame_relative_offset: LayoutVector2D,
        is_backface_visible: bool,
    ) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        let composition_operations = {
            // TODO(optimization?): self.traversal.display_list()
            let display_list = self.scene.get_display_list_for_pipeline(pipeline_id);
            CompositeOps::new(
                stacking_context.filter_ops_for_compositing(display_list, item.filters()),
                stacking_context.mix_blend_mode_for_compositing(),
            )
        };

        self.push_stacking_context(
            pipeline_id,
            composition_operations,
            stacking_context.transform_style,
            is_backface_visible,
            false,
            scroll_node_id,
            stacking_context.clip_node_id,
            stacking_context.raster_space,
        );

        self.flatten_items(
            traversal,
            pipeline_id,
            reference_frame_relative_offset + item.rect().origin.to_vector(),
        );

        self.pop_stacking_context();
    }

    fn flatten_iframe(
        &mut self,
        item: &DisplayItemRef,
        info: &IframeDisplayItem,
        clip_and_scroll_ids: &ClipAndScrollInfo,
        reference_frame_relative_offset: &LayoutVector2D,
    ) {
        let iframe_pipeline_id = info.pipeline_id;
        let pipeline = match self.scene.pipelines.get(&iframe_pipeline_id) {
            Some(pipeline) => pipeline,
            None => {
                debug_assert!(info.ignore_missing_pipeline);
                return
            },
        };

        let clip_chain_index = self.add_clip_node(
            info.clip_id,
            clip_and_scroll_ids,
            ClipRegion::create_for_clip_node_with_local_clip(
                item.clip_rect(),
                reference_frame_relative_offset
            ),
        );
        self.pipeline_clip_chain_stack.push(clip_chain_index);

        let bounds = item.rect();
        let origin = *reference_frame_relative_offset + bounds.origin.to_vector();
        self.push_reference_frame(
            ClipId::root_reference_frame(iframe_pipeline_id),
            Some(info.clip_id),
            None,
            iframe_pipeline_id,
            None,
            None,
            origin,
        );

        let iframe_rect = LayoutRect::new(LayoutPoint::zero(), bounds.size);
        self.add_scroll_frame(
            ClipId::root_scroll_node(iframe_pipeline_id),
            ClipId::root_reference_frame(iframe_pipeline_id),
            Some(ExternalScrollId(0, iframe_pipeline_id)),
            iframe_pipeline_id,
            &iframe_rect,
            &pipeline.content_size,
            ScrollSensitivity::ScriptAndInputEvents,
            ScrollFrameKind::PipelineRoot,
        );

        self.flatten_root(
            pipeline,
            &iframe_rect.size,
        );

        self.pipeline_clip_chain_stack.pop();
    }

    fn flatten_item<'b>(
        &'b mut self,
        item: DisplayItemRef<'a, 'b>,
        pipeline_id: PipelineId,
        reference_frame_relative_offset: LayoutVector2D,
    ) -> Option<BuiltDisplayListIter<'a>> {
        let clip_and_scroll_ids = item.clip_and_scroll();
        let clip_and_scroll = self.map_clip_and_scroll(&clip_and_scroll_ids);

        let prim_info = item.get_layout_primitive_info(&reference_frame_relative_offset);
        match *item.item() {
            SpecificDisplayItem::Image(ref info) => {
                self.add_image(
                    clip_and_scroll,
                    &prim_info,
                    info.stretch_size,
                    info.tile_spacing,
                    None,
                    info.image_key,
                    info.image_rendering,
                    info.alpha_type,
                    info.color,
                );
            }
            SpecificDisplayItem::YuvImage(ref info) => {
                self.add_yuv_image(
                    clip_and_scroll,
                    &prim_info,
                    info.yuv_data,
                    info.color_depth,
                    info.color_space,
                    info.image_rendering,
                );
            }
            SpecificDisplayItem::Text(ref text_info) => {
                self.add_text(
                    clip_and_scroll,
                    reference_frame_relative_offset,
                    &prim_info,
                    &text_info.font_key,
                    &text_info.color,
                    item.glyphs(),
                    text_info.glyph_options,
                    pipeline_id,
                );
            }
            SpecificDisplayItem::Rectangle(ref info) => {
                self.add_solid_rectangle(
                    clip_and_scroll,
                    &prim_info,
                    info.color,
                );
            }
            SpecificDisplayItem::ClearRectangle => {
                self.add_clear_rectangle(
                    clip_and_scroll,
                    &prim_info,
                );
            }
            SpecificDisplayItem::Line(ref info) => {
                self.add_line(
                    clip_and_scroll,
                    &prim_info,
                    info.wavy_line_thickness,
                    info.orientation,
                    info.color,
                    info.style,
                );
            }
            SpecificDisplayItem::Gradient(ref info) => {
                if let Some(prim_key_kind) = self.create_linear_gradient_prim(
                    &prim_info,
                    info.gradient.start_point,
                    info.gradient.end_point,
                    item.gradient_stops(),
                    info.gradient.extend_mode,
                    info.tile_size,
                    info.tile_spacing,
                    pipeline_id,
                    None,
                ) {
                    self.add_nonshadowable_primitive(
                        clip_and_scroll,
                        &prim_info,
                        Vec::new(),
                        prim_key_kind,
                    );
                }
            }
            SpecificDisplayItem::RadialGradient(ref info) => {
                let prim_key_kind = self.create_radial_gradient_prim(
                    &prim_info,
                    info.gradient.center,
                    info.gradient.start_offset * info.gradient.radius.width,
                    info.gradient.end_offset * info.gradient.radius.width,
                    info.gradient.radius.width / info.gradient.radius.height,
                    item.gradient_stops(),
                    info.gradient.extend_mode,
                    info.tile_size,
                    info.tile_spacing,
                    pipeline_id,
                    None,
                );
                self.add_nonshadowable_primitive(
                    clip_and_scroll,
                    &prim_info,
                    Vec::new(),
                    prim_key_kind,
                );
            }
            SpecificDisplayItem::BoxShadow(ref box_shadow_info) => {
                let bounds = box_shadow_info
                    .box_bounds
                    .translate(&reference_frame_relative_offset);
                let mut prim_info = prim_info.clone();
                prim_info.rect = bounds;
                self.add_box_shadow(
                    clip_and_scroll,
                    &prim_info,
                    &box_shadow_info.offset,
                    box_shadow_info.color,
                    box_shadow_info.blur_radius,
                    box_shadow_info.spread_radius,
                    box_shadow_info.border_radius,
                    box_shadow_info.clip_mode,
                );
            }
            SpecificDisplayItem::Border(ref info) => {
                self.add_border(
                    clip_and_scroll,
                    &prim_info,
                    info,
                    item.gradient_stops(),
                    pipeline_id,
                );
            }
            SpecificDisplayItem::PushStackingContext(ref info) => {
                let mut subtraversal = item.sub_iter();
                self.flatten_stacking_context(
                    &mut subtraversal,
                    pipeline_id,
                    &item,
                    &info.stacking_context,
                    clip_and_scroll_ids.scroll_node_id,
                    reference_frame_relative_offset,
                    prim_info.is_backface_visible,
                );
                return Some(subtraversal);
            }
            SpecificDisplayItem::PushReferenceFrame(ref info) => {
                let mut subtraversal = item.sub_iter();
                self.flatten_reference_frame(
                    &mut subtraversal,
                    pipeline_id,
                    &item,
                    &info.reference_frame,
                    &clip_and_scroll_ids,
                    reference_frame_relative_offset,
                );
                return Some(subtraversal);

            }
            SpecificDisplayItem::Iframe(ref info) => {
                self.flatten_iframe(
                    &item,
                    info,
                    &clip_and_scroll_ids,
                    &reference_frame_relative_offset
                );
            }
            SpecificDisplayItem::Clip(ref info) => {
                let complex_clips = self.get_complex_clips(pipeline_id, item.complex_clip().0);
                let clip_region = ClipRegion::create_for_clip_node(
                    *item.clip_rect(),
                    complex_clips,
                    info.image_mask,
                    &reference_frame_relative_offset,
                );
                self.add_clip_node(info.id, &clip_and_scroll_ids, clip_region);
            }
            SpecificDisplayItem::ClipChain(ref info) => {
                // For a user defined clip-chain the parent (if specified) must
                // refer to another user defined clip-chain. If none is specified,
                // the parent is the root clip-chain for the given pipeline. This
                // is used to provide a root clip chain for iframes.
                let mut parent_clip_chain_id = match info.parent {
                    Some(id) => {
                        self.id_to_index_mapper.get_clip_chain_id(&ClipId::ClipChain(id))
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
                for item in self.get_clip_chain_items(pipeline_id, item.clip_chain_items()) {
                    // Map the ClipId to an existing clip chain node.
                    let item_clip_node = self
                        .id_to_index_mapper
                        .get_clip_node(&item);

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
                        let (handle, spatial_node_index, local_pos) = {
                            let clip_chain = self
                                .clip_store
                                .get_clip_chain(clip_node_clip_chain_id);

                            clip_node_clip_chain_id = clip_chain.parent_clip_chain_id;

                            (clip_chain.handle, clip_chain.spatial_node_index, clip_chain.local_pos)
                        };

                        // Add a new clip chain node, which references the same clip sources, and
                        // parent it to the current parent.
                        clip_chain_id = self
                            .clip_store
                            .add_clip_chain_node(
                                handle,
                                local_pos,
                                spatial_node_index,
                                clip_chain_id,
                            );
                    }
                }

                // Map the last entry in the clip chain to the supplied ClipId. This makes
                // this ClipId available as a source to other user defined clip chains.
                self.id_to_index_mapper.add_clip_chain(ClipId::ClipChain(info.id), clip_chain_id, 0);
            },
            SpecificDisplayItem::ScrollFrame(ref info) => {
                self.flatten_scroll_frame(
                    &item,
                    info,
                    pipeline_id,
                    &clip_and_scroll_ids,
                    &reference_frame_relative_offset
                );
            }
            SpecificDisplayItem::StickyFrame(ref info) => {
                self.flatten_sticky_frame(
                    &item,
                    info,
                    &clip_and_scroll,
                    &clip_and_scroll_ids.scroll_node_id,
                    &reference_frame_relative_offset
                );
            }

            // Do nothing; these are dummy items for the display list parser
            SpecificDisplayItem::SetGradientStops => {}

            SpecificDisplayItem::PopStackingContext | SpecificDisplayItem::PopReferenceFrame => {
                unreachable!("Should have returned in parent method.")
            }
            SpecificDisplayItem::PushShadow(shadow) => {
                self.push_shadow(shadow, clip_and_scroll);
            }
            SpecificDisplayItem::PopAllShadows => {
                self.pop_all_shadows();
            }
            SpecificDisplayItem::PushCacheMarker(_marker) => {
            }
            SpecificDisplayItem::PopCacheMarker => {
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
        clip_items: Vec<(LayoutPoint, ClipItemKey)>,
        spatial_node_index: SpatialNodeIndex,
        parent_clip_chain_id: ClipChainId,
    ) -> ClipChainId {
        if clip_items.is_empty() {
            parent_clip_chain_id
        } else {
            let mut clip_chain_id = parent_clip_chain_id;

            for (local_pos, item) in clip_items {
                // Intern this clip item, and store the handle
                // in the clip chain node.
                let handle = self.resources
                    .clip_interner
                    .intern(&item, || {
                        ClipItemSceneData {
                            // The only type of clip items that exist in the per-primitive
                            // clip items are box shadows, and they don't contribute a
                            // local clip rect, so just provide max_rect here. In the future,
                            // we intend to make box shadows a primitive effect, in which
                            // case the entire clip_items API on primitives can be removed.
                            clip_rect: LayoutRect::max_rect(),
                        }
                    });

                clip_chain_id = self.clip_store
                                    .add_clip_chain_node(
                                        handle,
                                        local_pos,
                                        spatial_node_index,
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
        P: Internable<InternData=PrimitiveSceneData>,
        P::Source: AsInstanceKind<Handle<P::Marker>>,
        DocumentResources: InternerMut<P>,
    {
        let offset = info.rect.origin.to_vector();
        let prim_relative_clip_rect = info.clip_rect
            .translate(&-offset)
            .into();

        // Build a primitive key.
        let prim_key = prim.build_key(info, prim_relative_clip_rect);

        let interner = self.resources.interner_mut();
        let prim_data_handle =
            interner
            .intern(&prim_key, || {
                PrimitiveSceneData {
                    prim_relative_clip_rect,
                    prim_size: info.rect.size,
                    is_backface_visible: info.is_backface_visible,
                }
            });

        let instance_kind = prim_key.as_instance_kind(prim_data_handle,
                                                      &mut self.prim_store);

        PrimitiveInstance::new(
            info.rect.origin,
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
        let tag = match info.tag {
            Some(tag) => tag,
            None => return,
        };

        let new_item = HitTestingItem::new(tag, info);
        match self.hit_testing_runs.last_mut() {
            Some(&mut HitTestingRun(ref mut items, prev_clip_and_scroll))
                if prev_clip_and_scroll == clip_and_scroll => {
                items.push(new_item);
                return;
            }
            _ => {}
        }

        self.hit_testing_runs.push(HitTestingRun(vec![new_item], clip_and_scroll));
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
        clip_items: Vec<(LayoutPoint, ClipItemKey)>,
        prim: P,
    )
    where
        P: Internable<InternData = PrimitiveSceneData> + IsVisible,
        P::Source: AsInstanceKind<Handle<P::Marker>>,
        DocumentResources: InternerMut<P>,
    {
        if prim.is_visible() {
            let clip_chain_id = self.build_clip_chain(
                clip_items,
                clip_and_scroll.spatial_node_index,
                clip_and_scroll.clip_chain_id,
            );
            self.add_prim_to_draw_list(
                info,
                clip_chain_id,
                clip_and_scroll,
                prim
            );
        }
    }

    pub fn add_primitive<P>(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        clip_items: Vec<(LayoutPoint, ClipItemKey)>,
        prim: P,
    )
    where
        P: Internable<InternData = PrimitiveSceneData> + IsVisible,
        P::Source: AsInstanceKind<Handle<P::Marker>>,
        DocumentResources: InternerMut<P>,
        ShadowItem: From<PendingPrimitive<P>>
    {
        // If a shadow context is not active, then add the primitive
        // directly to the parent picture.
        if self.pending_shadow_items.is_empty() {
            self.add_nonshadowable_primitive(clip_and_scroll, info, clip_items, prim);
        } else {
            debug_assert!(clip_items.is_empty(), "No per-prim clips expected for shadowed primitives");

            // There is an active shadow context. Store as a pending primitive
            // for processing during pop_all_shadows.
            self.pending_shadow_items.push_back(PendingPrimitive {
                clip_and_scroll,
                info: *info,
                prim: prim.into(),
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
        P: Internable<InternData = PrimitiveSceneData>,
        P::Source: AsInstanceKind<Handle<P::Marker>>,
        DocumentResources: InternerMut<P>,
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
        is_pipeline_root: bool,
        spatial_node: ClipId,
        clipping_node: Option<ClipId>,
        requested_raster_space: RasterSpace,
    ) {
        let spatial_node_index = self.id_to_index_mapper.get_spatial_node_index(spatial_node);
        let clip_chain_id = match clipping_node {
            Some(ref clipping_node) => self.id_to_index_mapper.get_clip_chain_id(clipping_node),
            None => ClipChainId::NONE,
        };

        // Check if this stacking context is the root of a pipeline, and the caller
        // has requested it as an output frame.
        let frame_output_pipeline_id = if is_pipeline_root && self.output_pipelines.contains(&pipeline_id) {
            Some(pipeline_id)
        } else {
            None
        };

        // Get the transform-style of the parent stacking context,
        // which determines if we *might* need to draw this on
        // an intermediate surface for plane splitting purposes.
        let (parent_is_3d, extra_3d_instance) = match self.sc_stack.last_mut() {
            Some(sc) => {
                // Cut the sequence of flat children before starting a child stacking context,
                // so that the relative order between them and our current SC is preserved.
                let extra_instance = sc.cut_flat_item_sequence(
                    &mut self.prim_store,
                    &self.resources,
                    &self.clip_store,
                );
                (sc.is_3d(), extra_instance)
            },
            None => (false, None),
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

        // Force an intermediate surface if the stacking context
        // has a clip node. In the future, we may decide during
        // prepare step to skip the intermediate surface if the
        // clip node doesn't affect the stacking context rect.
        let should_isolate = clipping_node.is_some();

        let prim_key = PrimitiveKey::new(
            is_backface_visible,
            LayoutSize::zero(),
            LayoutRect::max_rect(),
            PrimitiveKeyKind::Unused,
        );

        let primitive_data_handle = self.resources
            .prim_interner
            .intern(&prim_key, || {
                PrimitiveSceneData {
                    prim_relative_clip_rect: LayoutRect::max_rect(),
                    prim_size: LayoutSize::zero(),
                    is_backface_visible,
                }
            }
        );

        // Push the SC onto the stack, so we know how to handle things in
        // pop_stacking_context.
        self.sc_stack.push(FlattenedStackingContext {
            primitives: Vec::new(),
            pipeline_id,
            primitive_data_handle,
            requested_raster_space,
            spatial_node_index,
            clip_chain_id,
            frame_output_pipeline_id,
            composite_ops,
            should_isolate,
            transform_style,
            context_3d,
        });
    }

    pub fn pop_stacking_context(&mut self) {
        let stacking_context = self.sc_stack.pop().unwrap();

        // If we encounter a stacking context that is effectively a no-op, then instead
        // of creating a picture, just append the primitive list to the parent stacking
        // context as a short cut. This serves two purposes:
        // (a) It's an optimization to reduce picture count and allocations, as display lists
        //     often contain a lot of these stacking contexts that don't require pictures or
        //     off-screen surfaces.
        // (b) It's useful for the initial version of picture caching in gecko, by enabling
        //     is to just look for interesting scroll roots on the root stacking context,
        //     without having to consider cuts at stacking context boundaries.
        if let Some(parent_sc) = self.sc_stack.last_mut() {
            if stacking_context.is_redundant(
                parent_sc,
                self.clip_scroll_tree,
            ) {
                parent_sc.primitives.extend(stacking_context.primitives);
                return;
            }
        }

        // An arbitrary large clip rect. For now, we don't
        // specify a clip specific to the stacking context.
        // However, now that they are represented as Picture
        // primitives, we can apply any kind of clip mask
        // to them, as for a normal primitive. This is needed
        // to correctly handle some CSS cases (see #1957).
        let max_clip = LayoutRect::max_rect();

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
                Some(PictureCompositeMode::Blit),
                None,
            ),
            Picture3DContext::Out => (
                Picture3DContext::Out,
                if stacking_context.should_isolate {
                    // Add a dummy composite filter if the SC has to be isolated.
                    Some(PictureCompositeMode::Blit)
                } else {
                    // By default, this picture will be collapsed into
                    // the owning target.
                    None
                },
                stacking_context.frame_output_pipeline_id
            ),
        };

        // Add picture for this actual stacking context contents to render into.
        let leaf_pic_index = PictureIndex(self.prim_store.pictures
            .alloc()
            .init(PicturePrimitive::new_image(
                leaf_composite_mode,
                leaf_context_3d,
                stacking_context.pipeline_id,
                leaf_output_pipeline_id,
                true,
                stacking_context.requested_raster_space,
                PrimitiveList::new(
                    stacking_context.primitives,
                    &self.resources,
                ),
                stacking_context.spatial_node_index,
                max_clip,
                &self.clip_store,
            ))
        );

        // Create a chain of pictures based on presence of filters,
        // mix-blend-mode and/or 3d rendering context containers.

        let mut current_pic_index = leaf_pic_index;
        let data_handle = stacking_context.primitive_data_handle;
        let mut cur_instance = PrimitiveInstance::new(
            LayoutPoint::zero(),
            PrimitiveInstanceKind::Picture {
                data_handle,
                pic_index: leaf_pic_index
            },
            stacking_context.clip_chain_id,
            stacking_context.spatial_node_index,
        );

        if cur_instance.is_chased() {
            println!("\tis a leaf primitive for a stacking context");
        }

        // If establishing a 3d context, the `cur_instance` represents
        // a picture with all the *trailing* immediate children elements.
        // We append this to the preserve-3D picture set and make a container picture of them.
        if let Picture3DContext::In { root_data: Some(mut prims), ancestor_index } = stacking_context.context_3d {
            prims.push(cur_instance.clone());

            // This is the acttual picture representing our 3D hierarchy root.
            current_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    None,
                    Picture3DContext::In {
                        root_data: Some(Vec::new()),
                        ancestor_index,
                    },
                    stacking_context.pipeline_id,
                    stacking_context.frame_output_pipeline_id,
                    true,
                    stacking_context.requested_raster_space,
                    PrimitiveList::new(
                        prims,
                        &self.resources,
                    ),
                    stacking_context.spatial_node_index,
                    max_clip,
                    &self.clip_store,
                ))
            );

            cur_instance.kind = PrimitiveInstanceKind::Picture {
                data_handle,
                pic_index: current_pic_index
            };
        }

        // For each filter, create a new image with that composite mode.
        for filter in &stacking_context.composite_ops.filters {
            let filter = filter.sanitize();

            let filter_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    Some(PictureCompositeMode::Filter(filter)),
                    Picture3DContext::Out,
                    stacking_context.pipeline_id,
                    None,
                    true,
                    stacking_context.requested_raster_space,
                    PrimitiveList::new(
                        vec![cur_instance.clone()],
                        &self.resources,
                    ),
                    stacking_context.spatial_node_index,
                    max_clip,
                    &self.clip_store,
                ))
            );

            current_pic_index = filter_pic_index;
            cur_instance.kind = PrimitiveInstanceKind::Picture {
                data_handle,
                pic_index: current_pic_index
            };

            if cur_instance.is_chased() {
                println!("\tis a composite picture for a stacking context with {:?}", filter);
            }

            // Run the optimize pass on this picture, to see if we can
            // collapse opacity and avoid drawing to an off-screen surface.
            self.prim_store.optimize_picture_if_possible(current_pic_index);
        }

        // Same for mix-blend-mode.
        if let Some(mix_blend_mode) = stacking_context.composite_ops.mix_blend_mode {
            let blend_pic_index = PictureIndex(self.prim_store.pictures
                .alloc()
                .init(PicturePrimitive::new_image(
                    Some(PictureCompositeMode::MixBlend(mix_blend_mode)),
                    Picture3DContext::Out,
                    stacking_context.pipeline_id,
                    None,
                    true,
                    stacking_context.requested_raster_space,
                    PrimitiveList::new(
                        vec![cur_instance.clone()],
                        &self.resources,
                    ),
                    stacking_context.spatial_node_index,
                    max_clip,
                    &self.clip_store,
                ))
            );

            current_pic_index = blend_pic_index;
            cur_instance.kind = PrimitiveInstanceKind::Picture {
                data_handle,
                pic_index: blend_pic_index
            };

            if cur_instance.is_chased() {
                println!("\tis a mix-blend picture for a stacking context with {:?}", mix_blend_mode);
            }
        }

        let has_mix_blend_on_secondary_framebuffer =
            stacking_context.composite_ops.mix_blend_mode.is_some() &&
            self.sc_stack.len() > 2;

        // The primitive instance for the remainder of flat children of this SC
        // if it's a part of 3D hierarchy but not the root of it.
        let trailing_children_instance = match self.sc_stack.last_mut() {
            // Preserve3D path (only relevant if there are no filters/mix-blend modes)
            Some(ref parent_sc) if parent_sc.is_3d() => {
                Some(cur_instance)
            }
            // Regular parenting path
            Some(ref mut parent_sc) => {
                // If we have a mix-blend-mode, and we aren't the primary framebuffer,
                // the stacking context needs to be isolated to blend correctly as per
                // the CSS spec.
                // If not already isolated for some other reason,
                // make this picture as isolated.
                if has_mix_blend_on_secondary_framebuffer {
                    parent_sc.should_isolate = true;
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
        reference_frame_id: ClipId,
        parent_scroll_id: Option<ClipId>,
        parent_clip_id: Option<ClipId>,
        pipeline_id: PipelineId,
        source_transform: Option<PropertyBinding<LayoutTransform>>,
        source_perspective: Option<LayoutTransform>,
        origin_in_parent_reference_frame: LayoutVector2D,
    ) -> SpatialNodeIndex {
        let parent_index = parent_scroll_id.map(|id| self.id_to_index_mapper.get_spatial_node_index(id));
        let index = self.clip_scroll_tree.add_reference_frame(
            parent_index,
            source_transform,
            source_perspective,
            origin_in_parent_reference_frame,
            pipeline_id,
        );
        self.id_to_index_mapper.map_spatial_node(reference_frame_id, index);

        match parent_clip_id.or(parent_scroll_id) {
            Some(ref parent_id) =>
                self.id_to_index_mapper.map_to_parent_clip_chain(reference_frame_id, parent_id),
            _ => self.id_to_index_mapper.add_clip_chain(reference_frame_id, ClipChainId::NONE, 0),
        }
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

        self.push_reference_frame(
            ClipId::root_reference_frame(pipeline_id),
            None,
            None,
            pipeline_id,
            None,
            None,
            LayoutVector2D::zero(),
        );

        self.add_scroll_frame(
            ClipId::root_scroll_node(pipeline_id),
            ClipId::root_reference_frame(pipeline_id),
            Some(ExternalScrollId(0, pipeline_id)),
            pipeline_id,
            &LayoutRect::new(LayoutPoint::zero(), *viewport_size),
            content_size,
            ScrollSensitivity::ScriptAndInputEvents,
            ScrollFrameKind::PipelineRoot,
        );
    }

    pub fn add_clip_node<I>(
        &mut self,
        new_node_id: ClipId,
        parent: &ClipAndScrollInfo,
        clip_region: ClipRegion<I>,
    ) -> ClipChainId
    where
        I: IntoIterator<Item = ComplexClipRegion>
    {
        // Add a new ClipNode - this is a ClipId that identifies a list of clip items,
        // and the positioning node associated with those clip sources.

        // Map from parent ClipId to existing clip-chain.
        let mut parent_clip_chain_index = self
            .id_to_index_mapper
            .get_clip_chain_id(&parent.clip_node_id());
        // Map the ClipId for the positioning node to a spatial node index.
        let spatial_node = self.id_to_index_mapper.get_spatial_node_index(parent.scroll_node_id);

        // Add a mapping for this ClipId in case it's referenced as a positioning node.
        self.id_to_index_mapper
            .map_spatial_node(new_node_id, spatial_node);

        let mut clip_count = 0;

        // Intern each clip item in this clip node, and add the interned
        // handle to a clip chain node, parented to form a chain.
        // TODO(gw): We could re-structure this to share some of the
        //           interning and chaining code.

        // Build the clip sources from the supplied region.
        let handle = self
            .resources
            .clip_interner
            .intern(&ClipItemKey::rectangle(clip_region.main.size, ClipMode::Clip), || {
                ClipItemSceneData {
                    clip_rect: clip_region.main,
                }
            });

        parent_clip_chain_index = self
            .clip_store
            .add_clip_chain_node(
                handle,
                clip_region.main.origin,
                spatial_node,
                parent_clip_chain_index,
            );
        clip_count += 1;

        if let Some(ref image_mask) = clip_region.image_mask {
            let handle = self
                .resources
                .clip_interner
                .intern(&ClipItemKey::image_mask(image_mask), || {
                    ClipItemSceneData {
                        clip_rect: image_mask.get_local_clip_rect().unwrap_or(LayoutRect::max_rect()),
                    }
                });

            parent_clip_chain_index = self
                .clip_store
                .add_clip_chain_node(
                    handle,
                    image_mask.rect.origin,
                    spatial_node,
                    parent_clip_chain_index,
                );
            clip_count += 1;
        }

        for region in clip_region.complex_clips {
            let handle = self
                .resources
                .clip_interner
                .intern(&ClipItemKey::rounded_rect(region.rect.size, region.radii, region.mode), || {
                    ClipItemSceneData {
                        clip_rect: region.get_local_clip_rect().unwrap_or(LayoutRect::max_rect()),
                    }
                });

            parent_clip_chain_index = self
                .clip_store
                .add_clip_chain_node(
                    handle,
                    region.rect.origin,
                    spatial_node,
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
        new_node_id: ClipId,
        parent_id: ClipId,
        external_id: Option<ExternalScrollId>,
        pipeline_id: PipelineId,
        frame_rect: &LayoutRect,
        content_size: &LayoutSize,
        scroll_sensitivity: ScrollSensitivity,
        frame_kind: ScrollFrameKind,
    ) -> SpatialNodeIndex {
        let parent_node_index = self.id_to_index_mapper.get_spatial_node_index(parent_id);
        let node_index = self.clip_scroll_tree.add_scroll_frame(
            parent_node_index,
            external_id,
            pipeline_id,
            frame_rect,
            content_size,
            scroll_sensitivity,
            frame_kind,
        );
        self.id_to_index_mapper.map_spatial_node(new_node_id, node_index);
        self.id_to_index_mapper.map_to_parent_clip_chain(new_node_id, &parent_id);
        node_index
    }

    pub fn push_shadow(
        &mut self,
        shadow: Shadow,
        clip_and_scroll: ScrollNodeAndClipChain,
    ) {
        // Store this shadow in the pending list, for processing
        // during pop_all_shadows.
        self.pending_shadow_items.push_back(ShadowItem::Shadow(PendingShadow {
            shadow,
            clip_and_scroll,
        }));
    }

    pub fn pop_all_shadows(&mut self) {
        assert!(!self.pending_shadow_items.is_empty(), "popped shadows, but none were present");

        let pipeline_id = self.sc_stack.last().unwrap().pipeline_id;
        let max_clip = LayoutRect::max_rect();
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
                            // TODO(djg): ugh. de-duplicate this code.
                            ShadowItem::Primitive(ref pending_primitive) => {
                                self.add_shadow_prim(&pending_shadow, pending_primitive, &mut prims)
                            }
                            ShadowItem::TextRun(ref pending_text_run) => {
                                self.add_shadow_prim(&pending_shadow, pending_text_run, &mut prims)
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
                        let blur_filter = FilterOp::Blur(std_deviation).sanitize();

                        // Create the primitive to draw the shadow picture into the scene.
                        let shadow_pic_index = PictureIndex(self.prim_store.pictures
                            .alloc()
                            .init(PicturePrimitive::new_image(
                                Some(PictureCompositeMode::Filter(blur_filter)),
                                Picture3DContext::Out,
                                pipeline_id,
                                None,
                                is_passthrough,
                                raster_space,
                                PrimitiveList::new(
                                    prims,
                                    &self.resources,
                                ),
                                pending_shadow.clip_and_scroll.spatial_node_index,
                                max_clip,
                                &self.clip_store,
                            ))
                        );

                        let shadow_prim_key = PrimitiveKey::new(
                            true,
                            LayoutSize::zero(),
                            LayoutRect::max_rect(),
                            PrimitiveKeyKind::Unused,
                        );

                        let shadow_prim_data_handle = self.resources
                            .prim_interner
                            .intern(&shadow_prim_key, || {
                                PrimitiveSceneData {
                                    prim_relative_clip_rect: LayoutRect::max_rect(),
                                    prim_size: LayoutSize::zero(),
                                    is_backface_visible: true,
                                }
                            }
                        );

                        let shadow_prim_instance = PrimitiveInstance::new(
                            LayoutPoint::zero(),
                            PrimitiveInstanceKind::Picture {
                                data_handle: shadow_prim_data_handle,
                                pic_index: shadow_pic_index
                            },
                            pending_shadow.clip_and_scroll.clip_chain_id,
                            pending_shadow.clip_and_scroll.spatial_node_index,
                        );

                        // Add the shadow primitive. This must be done before pushing this
                        // picture on to the shadow stack, to avoid infinite recursion!
                        self.add_primitive_to_draw_list(shadow_prim_instance);
                    }
                }
                ShadowItem::Primitive(pending_primitive) => {
                    self.add_shadow_prim_to_draw_list(pending_primitive)
                },
                ShadowItem::TextRun(pending_text_run) => {
                    self.add_shadow_prim_to_draw_list(pending_text_run)
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
        P: Internable<InternData=PrimitiveSceneData> + CreateShadow,
        P::Source: AsInstanceKind<Handle<P::Marker>>,
        DocumentResources: InternerMut<P>,
    {
        // Offset the local rect and clip rect by the shadow offset.
        let mut info = pending_primitive.info.clone();
        info.rect = info.rect.translate(&pending_shadow.shadow.offset);
        info.clip_rect = info.clip_rect.translate(&pending_shadow.shadow.offset);

        // Construct and add a primitive for the given shadow.
        let shadow_prim_instance = self.create_primitive(
            &info,
            pending_primitive.clip_and_scroll.clip_chain_id,
            pending_primitive.clip_and_scroll.spatial_node_index,
            pending_primitive.prim.create_shadow(
                &pending_shadow.shadow,
            ),
        );

        // Add the new primitive to the shadow picture.
        prims.push(shadow_prim_instance);
    }

    fn add_shadow_prim_to_draw_list<P>(&mut self, pending_primitive: PendingPrimitive<P>)
    where
        P: Internable<InternData = PrimitiveSceneData> + IsVisible,
        P::Source: AsInstanceKind<Handle<P::Marker>>,
        DocumentResources: InternerMut<P>,
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
                    .unwrap_or(LayoutRect::zero());
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
            PrimitiveKeyKind::LineDecoration {
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
        pipeline_id: PipelineId,
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
                        let prim = PrimitiveKeyKind::ImageBorder {
                            request: ImageRequest {
                                key: image_key,
                                rendering: ImageRendering::Auto,
                                tile: None,
                            },
                            nine_patch,
                        };

                        self.add_primitive(
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
                            pipeline_id,
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
                            pipeline_id,
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
        pipeline_id: PipelineId,
        nine_patch: Option<Box<NinePatchDescriptor>>,
    ) -> Option<LinearGradient> {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        // TODO(gw): It seems like we should be able to look this up once in
        //           flatten_root() and pass to all children here to avoid
        //           some hash lookups?
        let display_list = self.scene.get_display_list_for_pipeline(pipeline_id);
        let mut max_alpha: f32 = 0.0;

        let stops = display_list.get(stops).map(|stop| {
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
        pipeline_id: PipelineId,
        nine_patch: Option<Box<NinePatchDescriptor>>,
    ) -> RadialGradient {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        // TODO(gw): It seems like we should be able to look this up once in
        //           flatten_root() and pass to all children here to avoid
        //           some hash lookups?
        let display_list = self.scene.get_display_list_for_pipeline(pipeline_id);

        let params = RadialGradientParams {
            start_radius,
            end_radius,
            ratio_xy,
        };

        let stops = display_list.get(stops).map(|stop| {
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
        offset: LayoutVector2D,
        prim_info: &LayoutPrimitiveInfo,
        font_instance_key: &FontInstanceKey,
        text_color: &ColorF,
        glyph_range: ItemRange<GlyphInstance>,
        glyph_options: Option<GlyphOptions>,
        pipeline_id: PipelineId,
    ) {
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
                font_instance.font_key,
                font_instance.size,
                *text_color,
                font_instance.bg_color,
                render_mode,
                flags,
                font_instance.synthetic_italics,
                font_instance.platform_options,
                font_instance.variations.clone(),
            );

            // TODO(gw): We can do better than a hash lookup here...
            let display_list = self.scene.get_display_list_for_pipeline(pipeline_id);

            // TODO(gw): It'd be nice not to have to allocate here for creating
            //           the primitive key, when the common case is that the
            //           hash will match and we won't end up creating a new
            //           primitive template.
            let glyphs = display_list.get(glyph_range).collect();

            TextRun {
                glyphs,
                font,
                offset: offset.to_au(),
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
            PrimitiveKeyKind::Image {
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
        image_rendering: ImageRendering,
    ) {
        let format = yuv_data.get_format();
        let yuv_key = match yuv_data {
            YuvData::NV12(plane_0, plane_1) => [plane_0, plane_1, ImageKey::DUMMY],
            YuvData::PlanarYCbCr(plane_0, plane_1, plane_2) => [plane_0, plane_1, plane_2],
            YuvData::InterleavedYCbCr(plane_0) => [plane_0, ImageKey::DUMMY, ImageKey::DUMMY],
        };

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveKeyKind::YuvImage {
                color_depth,
                yuv_key,
                format,
                color_space,
                image_rendering,
            },
        );
    }

    pub fn map_clip_and_scroll(&mut self, info: &ClipAndScrollInfo) -> ScrollNodeAndClipChain {
        ScrollNodeAndClipChain::new(
            self.id_to_index_mapper.get_spatial_node_index(info.scroll_node_id),
            self.id_to_index_mapper.get_clip_chain_id(&info.clip_node_id())
        )
    }

    pub fn simple_scroll_and_clip_chain(&mut self, id: &ClipId) -> ScrollNodeAndClipChain {
        self.map_clip_and_scroll(&ClipAndScrollInfo::simple(*id))
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
}

pub trait AsInstanceKind<H> {
    fn as_instance_kind(
        &self,
        data_handle: H,
        prim_store: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind;
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

    /// The interned key for all the primitive instances associated with this
    /// SC (but not its children);
    primitive_data_handle: PrimitiveDataHandle,

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

    /// If true, this stacking context should be
    /// isolated by forcing an off-screen surface.
    should_isolate: bool,

    /// Pipeline this stacking context belongs to.
    pipeline_id: PipelineId,

    /// CSS transform-style property.
    transform_style: TransformStyle,

    /// Defines the relationship to a preserve-3D hiearachy.
    context_3d: Picture3DContext<PrimitiveInstance>,
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
        clip_scroll_tree: &ClipScrollTree,
    ) -> bool {
        // Any 3d context is required
        if let Picture3DContext::In { .. } = self.context_3d {
            return false;
        }

        // If there are filters / mix-blend-mode
        if !self.composite_ops.is_empty() {
            return false;
        }

        // If backface visibility is different
        if self.primitive_data_handle.uid() != parent.primitive_data_handle.uid() {
            return false;
        }

        // If rasterization space is different
        if self.requested_raster_space != parent.requested_raster_space {
            return false;
        }

        // If different clipp chains
        if self.clip_chain_id != parent.clip_chain_id {
            return false;
        }

        // If need to isolate in surface due to clipping / mix-blend-mode
        if self.should_isolate {
            return false;
        }

        // If represents a transform, it may affect backface visibility of children
        if !clip_scroll_tree.node_is_identity(self.spatial_node_index) {
            return false;
        }

        // It is redundant!
        true
    }

    /// For a Preserve3D context, cut the sequence of the immediate flat children
    /// recorded so far and generate a picture from them.
    pub fn cut_flat_item_sequence(
        &mut self,
        prim_store: &mut PrimitiveStore,
        resources: &DocumentResources,
        clip_store: &ClipStore,
    ) -> Option<PrimitiveInstance> {
        if !self.is_3d() || self.primitives.is_empty() {
            return None
        }
        let flat_items_context_3d = match self.context_3d {
            Picture3DContext::In { ancestor_index, .. } => Picture3DContext::In {
                root_data: None,
                ancestor_index,
            },
            Picture3DContext::Out => panic!("Unexpected out of 3D context"),
        };

        let pic_index = PictureIndex(prim_store.pictures
            .alloc()
            .init(PicturePrimitive::new_image(
                Some(PictureCompositeMode::Blit),
                flat_items_context_3d,
                self.pipeline_id,
                None,
                true,
                self.requested_raster_space,
                PrimitiveList::new(
                    mem::replace(&mut self.primitives, Vec::new()),
                    resources,
                ),
                self.spatial_node_index,
                LayoutRect::max_rect(),
                clip_store,
            ))
        );

        Some(PrimitiveInstance::new(
            LayoutPoint::zero(),
            PrimitiveInstanceKind::Picture {
                data_handle: self.primitive_data_handle,
                pic_index
            },
            self.clip_chain_id,
            self.spatial_node_index,
        ))
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
    clip_and_scroll: ScrollNodeAndClipChain,
}

pub enum ShadowItem {
    Shadow(PendingShadow),
    Primitive(PendingPrimitive<PrimitiveKeyKind>),
    TextRun(PendingPrimitive<TextRun>),
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

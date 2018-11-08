
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderDetails, BorderDisplayItem, BuiltDisplayListIter, ClipAndScrollInfo};
use api::{ClipId, ColorF, ComplexClipRegion, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{DisplayItemRef, ExtendMode, ExternalScrollId};
use api::{FilterOp, FontInstanceKey, GlyphInstance, GlyphOptions, RasterSpace, GradientStop};
use api::{IframeDisplayItem, ImageKey, ImageRendering, ItemRange, LayoutPoint, ColorDepth};
use api::{LayoutPrimitiveInfo, LayoutRect, LayoutSize, LayoutTransform, LayoutVector2D};
use api::{LineOrientation, LineStyle, NinePatchBorderSource, PipelineId};
use api::{PropertyBinding, ReferenceFrame, RepeatMode, ScrollFrameDisplayItem, ScrollSensitivity};
use api::{Shadow, SpecificDisplayItem, StackingContext, StickyFrameDisplayItem, TexelRect};
use api::{ClipMode, TransformStyle, YuvColorSpace, YuvData};
use clip::{ClipChainId, ClipRegion, ClipItemKey, ClipStore, ClipItemSceneData};
use clip_scroll_tree::{ROOT_SPATIAL_NODE_INDEX, ClipScrollTree, SpatialNodeIndex};
use euclid::vec2;
use frame_builder::{ChasePrimitive, FrameBuilder, FrameBuilderConfig};
use glyph_rasterizer::FontInstance;
use gpu_cache::GpuCacheHandle;
use gpu_types::BrushFlags;
use hit_test::{HitTestingItem, HitTestingRun};
use image::simplify_repeated_primitive;
use internal_types::{FastHashMap, FastHashSet};
use picture::{Picture3DContext, PictureCompositeMode, PictureIdGenerator, PicturePrimitive, PrimitiveList};
use prim_store::{BrushKind, BrushPrimitive, BrushSegmentDescriptor, PrimitiveInstance, PrimitiveDataInterner, PrimitiveKeyKind};
use prim_store::{EdgeAaSegmentMask, ImageSource, PrimitiveOpacity, PrimitiveKey, PrimitiveSceneData, PrimitiveInstanceKind};
use prim_store::{BorderSource, BrushSegment, BrushSegmentVec, PrimitiveContainer, PrimitiveDataHandle, PrimitiveStore};
use prim_store::{OpacityBinding, ScrollNodeAndClipChain, PictureIndex, register_prim_chase_id};
use render_backend::{DocumentView};
use resource_cache::{FontInstanceMap, ImageRequest};
use scene::{Scene, ScenePipeline, StackingContextHelpers};
use scene_builder::DocumentResources;
use spatial_node::{StickyFrameInfo};
use std::{f32, mem};
use std::collections::vec_deque::VecDeque;
use tiling::{CompositeOps};
use util::{MaxRect, RectHelpers};

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

    /// A counter for generating unique picture ids.
    picture_id_generator: &'a mut PictureIdGenerator,

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

    /// The estimated count of primtives we expect to encounter during flattening.
    prim_count_estimate: usize,

    /// The root picture index for this flattener. This is the picture
    /// to start the culling phase from.
    pub root_pic_index: PictureIndex,
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
        picture_id_generator: &mut PictureIdGenerator,
        resources: &mut DocumentResources,
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
            prim_store: PrimitiveStore::new(),
            clip_store: ClipStore::new(),
            picture_id_generator,
            resources,
            prim_count_estimate: 0,
            root_pic_index: PictureIndex(0),
        };

        flattener.push_root(
            root_pipeline_id,
            &root_pipeline.viewport_size,
            &root_pipeline.content_size,
        );
        flattener.flatten_root(root_pipeline, &root_pipeline.viewport_size);

        debug_assert!(flattener.sc_stack.is_empty());

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

    fn flatten_root(&mut self, pipeline: &'a ScenePipeline, frame_size: &LayoutSize) {
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

        self.prim_count_estimate += pipeline.display_list.prim_count_estimate();
        self.prim_store.primitives.reserve(self.prim_count_estimate);

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

        self.add_clip_node(info.clip_id, clip_and_scroll_ids.scroll_node_id, clip_region);

        self.add_scroll_frame(
            info.scroll_frame_id,
            info.clip_id,
            info.external_id,
            pipeline_id,
            &frame_rect,
            &content_rect.size,
            info.scroll_sensitivity,
        );
    }

    fn flatten_reference_frame(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        item: &DisplayItemRef,
        reference_frame: &ReferenceFrame,
        scroll_node_id: ClipId,
        reference_frame_relative_offset: LayoutVector2D,
    ) {
        self.push_reference_frame(
            reference_frame.id,
            Some(scroll_node_id),
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

        //TODO: use or assert on `clip_and_scroll_ids.clip_node_id` ?
        let clip_chain_index = self.add_clip_node(
            info.clip_id,
            clip_and_scroll_ids.scroll_node_id,
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
        );

        self.flatten_root(pipeline, &iframe_rect.size);

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
                    &info.color,
                    info.style,
                );
            }
            SpecificDisplayItem::Gradient(ref info) => {
                if let Some(brush_kind) = self.create_brush_kind_for_gradient(
                    &prim_info,
                    info.gradient.start_point,
                    info.gradient.end_point,
                    item.gradient_stops(),
                    info.gradient.extend_mode,
                    info.tile_size,
                    info.tile_spacing,
                    pipeline_id,
                ) {
                    let prim = PrimitiveContainer::Brush(BrushPrimitive::new(brush_kind, None));
                    self.add_primitive(clip_and_scroll, &prim_info, Vec::new(), prim);
                }
            }
            SpecificDisplayItem::RadialGradient(ref info) => {
                let brush_kind = self.create_brush_kind_for_radial_gradient(
                    &prim_info,
                    info.gradient.center,
                    info.gradient.start_offset * info.gradient.radius.width,
                    info.gradient.end_offset * info.gradient.radius.width,
                    info.gradient.radius.width / info.gradient.radius.height,
                    item.gradient_stops(),
                    info.gradient.extend_mode,
                    info.tile_size,
                    info.tile_spacing,
                );
                let prim = PrimitiveContainer::Brush(BrushPrimitive::new(brush_kind, None));
                self.add_primitive(clip_and_scroll, &prim_info, Vec::new(), prim);
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
                    &box_shadow_info.color,
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
                    clip_and_scroll_ids.scroll_node_id,
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
                self.add_clip_node(info.id, clip_and_scroll_ids.scroll_node_id, clip_region);
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
                        let (handle, spatial_node_index) = {
                            let clip_chain = self
                                .clip_store
                                .get_clip_chain(clip_node_clip_chain_id);

                            clip_node_clip_chain_id = clip_chain.parent_clip_chain_id;

                            (clip_chain.handle, clip_chain.spatial_node_index)
                        };

                        // Add a new clip chain node, which references the same clip sources, and
                        // parent it to the current parent.
                        clip_chain_id = self
                            .clip_store
                            .add_clip_chain_node(
                                handle,
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
        spatial_node_index: SpatialNodeIndex,
        parent_clip_chain_id: ClipChainId,
    ) -> ClipChainId {
        if clip_items.is_empty() {
            parent_clip_chain_id
        } else {
            let mut clip_chain_id = parent_clip_chain_id;

            for item in clip_items {
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
    pub fn create_primitive(
        &mut self,
        info: &LayoutPrimitiveInfo,
        clip_chain_id: ClipChainId,
        spatial_node_index: SpatialNodeIndex,
        container: PrimitiveContainer,
    ) -> PrimitiveInstance {
        // Build a primitive key, and optionally an old
        // style PrimitiveDetails structure from the
        // source primitive container.
        let mut info = info.clone();
        let (prim_key_kind, prim_details) = container.build(&mut info);

        let prim_key = PrimitiveKey::new(
            info.is_backface_visible,
            info.rect,
            info.clip_rect,
            prim_key_kind,
        );

        // Get a tight bounding / culling rect for this primitive
        // from its local rect intersection with minimal local
        // clip rect.
        let culling_rect = info.clip_rect
            .intersection(&info.rect)
            .unwrap_or(LayoutRect::zero());

        let prim_data_handle = self.resources
            .prim_interner
            .intern(&prim_key, || {
                PrimitiveSceneData {
                    culling_rect,
                    is_backface_visible: info.is_backface_visible,
                }
            });

        // If we are building an old style primitive, add it to
        // the prim store, and create a primitive index for it.
        // For an interned primitive, use the primitive key to
        // create a matching primitive instance kind.
        let instance_kind = match prim_details {
            Some(prim_details) => {
                let prim_index = self.prim_store.add_primitive(
                    &info.rect,
                    &info.clip_rect,
                    prim_details,
                );

                PrimitiveInstanceKind::LegacyPrimitive { prim_index }
            }
            None => {
                prim_key.to_instance_kind(&mut self.prim_store)
            }
        };

        PrimitiveInstance::new(
            instance_kind,
            prim_data_handle,
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
    pub fn add_primitive(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        clip_items: Vec<ClipItemKey>,
        container: PrimitiveContainer,
    ) {
        // If a shadow context is not active, then add the primitive
        // directly to the parent picture.
        if self.pending_shadow_items.is_empty() {
            if container.is_visible() {
                let clip_chain_id = self.build_clip_chain(
                    clip_items,
                    clip_and_scroll.spatial_node_index,
                    clip_and_scroll.clip_chain_id,
                );
                let prim_instance = self.create_primitive(
                    info,
                    clip_chain_id,
                    clip_and_scroll.spatial_node_index,
                    container,
                );
                self.register_chase_primitive_by_rect(
                    &info.rect,
                    &prim_instance,
                );
                self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
                self.add_primitive_to_draw_list(prim_instance);
            }
        } else {
            debug_assert!(clip_items.is_empty(), "No per-prim clips expected for shadowed primitives");

            // There is an active shadow context. Store as a pending primitive
            // for processing during pop_all_shadows.
            self.pending_shadow_items.push_back(ShadowItem::Primitive(PendingPrimitive {
                clip_and_scroll,
                info: *info,
                container,
            }));
        }
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
                    &mut self.picture_id_generator,
                    &mut self.prim_store,
                    &self.resources.prim_interner,
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
            LayoutRect::zero(),
            LayoutRect::max_rect(),
            PrimitiveKeyKind::Unused,
        );

        let primitive_data_handle = self.resources
            .prim_interner
            .intern(&prim_key, || {
                PrimitiveSceneData {
                    culling_rect: LayoutRect::zero(),
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
        let prim_list = PrimitiveList::new(
            stacking_context.primitives,
            &self.resources.prim_interner,
        );
        let leaf_picture = PicturePrimitive::new_image(
            self.picture_id_generator.next(),
            leaf_composite_mode,
            leaf_context_3d,
            stacking_context.pipeline_id,
            leaf_output_pipeline_id,
            true,
            stacking_context.requested_raster_space,
            prim_list,
            stacking_context.spatial_node_index,
            max_clip,
            &self.clip_store,
        );
        let leaf_pic_index = self.prim_store.create_picture(leaf_picture);

        // Create a chain of pictures based on presence of filters,
        // mix-blend-mode and/or 3d rendering context containers.

        let mut current_pic_index = leaf_pic_index;
        let mut cur_instance = PrimitiveInstance::new(
            PrimitiveInstanceKind::Picture { pic_index: leaf_pic_index },
            stacking_context.primitive_data_handle,
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

            let prim_list = PrimitiveList::new(
                prims,
                &self.resources.prim_interner,
            );

            // This is the acttual picture representing our 3D hierarchy root.
            let container_picture = PicturePrimitive::new_image(
                self.picture_id_generator.next(),
                None,
                Picture3DContext::In {
                    root_data: Some(Vec::new()),
                    ancestor_index,
                },
                stacking_context.pipeline_id,
                stacking_context.frame_output_pipeline_id,
                true,
                stacking_context.requested_raster_space,
                prim_list,
                stacking_context.spatial_node_index,
                max_clip,
                &self.clip_store,
            );

            current_pic_index = self.prim_store.create_picture(container_picture);

            cur_instance.kind = PrimitiveInstanceKind::Picture { pic_index: current_pic_index };
        }

        // For each filter, create a new image with that composite mode.
        for filter in &stacking_context.composite_ops.filters {
            let filter = filter.sanitize();
            let prim_list = PrimitiveList::new(
                vec![cur_instance.clone()],
                &self.resources.prim_interner,
            );

            let filter_picture = PicturePrimitive::new_image(
                self.picture_id_generator.next(),
                Some(PictureCompositeMode::Filter(filter)),
                Picture3DContext::Out,
                stacking_context.pipeline_id,
                None,
                true,
                stacking_context.requested_raster_space,
                prim_list,
                stacking_context.spatial_node_index,
                max_clip,
                &self.clip_store,
            );
            let filter_pic_index = self.prim_store.create_picture(filter_picture);
            current_pic_index = filter_pic_index;

            cur_instance.kind = PrimitiveInstanceKind::Picture { pic_index: current_pic_index };

            if cur_instance.is_chased() {
                println!("\tis a composite picture for a stacking context with {:?}", filter);
            }

            // Run the optimize pass on this picture, to see if we can
            // collapse opacity and avoid drawing to an off-screen surface.
            self.prim_store.optimize_picture_if_possible(current_pic_index);
        }

        // Same for mix-blend-mode.
        if let Some(mix_blend_mode) = stacking_context.composite_ops.mix_blend_mode {
            let prim_list = PrimitiveList::new(
                vec![cur_instance.clone()],
                &self.resources.prim_interner,
            );

            let blend_picture = PicturePrimitive::new_image(
                self.picture_id_generator.next(),
                Some(PictureCompositeMode::MixBlend(mix_blend_mode)),
                Picture3DContext::Out,
                stacking_context.pipeline_id,
                None,
                true,
                stacking_context.requested_raster_space,
                prim_list,
                stacking_context.spatial_node_index,
                max_clip,
                &self.clip_store,
            );
            let blend_pic_index = self.prim_store.create_picture(blend_picture);
            current_pic_index = blend_pic_index;

            cur_instance.kind = PrimitiveInstanceKind::Picture { pic_index: blend_pic_index };

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
        parent_id: Option<ClipId>,
        pipeline_id: PipelineId,
        source_transform: Option<PropertyBinding<LayoutTransform>>,
        source_perspective: Option<LayoutTransform>,
        origin_in_parent_reference_frame: LayoutVector2D,
    ) -> SpatialNodeIndex {
        let parent_index = parent_id.map(|id| self.id_to_index_mapper.get_spatial_node_index(id));
        let index = self.clip_scroll_tree.add_reference_frame(
            parent_index,
            source_transform,
            source_perspective,
            origin_in_parent_reference_frame,
            pipeline_id,
        );
        self.id_to_index_mapper.map_spatial_node(reference_frame_id, index);

        match parent_id {
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
        );
    }

    pub fn add_clip_node<I>(
        &mut self,
        new_node_id: ClipId,
        parent_id: ClipId,
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
            .get_clip_chain_id(&parent_id);
        // Map the ClipId for the positioning node to a spatial node index.
        let spatial_node = self.id_to_index_mapper.get_spatial_node_index(parent_id);

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
            .intern(&ClipItemKey::rectangle(clip_region.main, ClipMode::Clip), || {
                ClipItemSceneData {
                    clip_rect: clip_region.main,
                }
            });

        parent_clip_chain_index = self
            .clip_store
            .add_clip_chain_node(
                handle,
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
                    spatial_node,
                    parent_clip_chain_index,
                );
            clip_count += 1;
        }

        for region in clip_region.complex_clips {
            let handle = self
                .resources
                .clip_interner
                .intern(&ClipItemKey::rounded_rect(region.rect, region.radii, region.mode), || {
                    ClipItemSceneData {
                        clip_rect: region.get_local_clip_rect().unwrap_or(LayoutRect::max_rect()),
                    }
                });

            parent_clip_chain_index = self
                .clip_store
                .add_clip_chain_node(
                    handle,
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
    ) -> SpatialNodeIndex {
        let parent_node_index = self.id_to_index_mapper.get_spatial_node_index(parent_id);
        let node_index = self.clip_scroll_tree.add_scroll_frame(
            parent_node_index,
            external_id,
            pipeline_id,
            frame_rect,
            content_size,
            scroll_sensitivity,
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
                        if let ShadowItem::Primitive(ref pending_primitive) = item {
                            // Offset the local rect and clip rect by the shadow offset.
                            let mut info = pending_primitive.info.clone();
                            info.rect = info.rect.translate(&pending_shadow.shadow.offset);
                            info.clip_rect = info.clip_rect.translate(&pending_shadow.shadow.offset);

                            // Construct and add a primitive for the given shadow.
                            let shadow_prim_instance = self.create_primitive(
                                &info,
                                pending_primitive.clip_and_scroll.clip_chain_id,
                                pending_primitive.clip_and_scroll.spatial_node_index,
                                pending_primitive.container.create_shadow(
                                    &pending_shadow.shadow,
                                    &info.rect,
                                ),
                            );

                            // Add the new primitive to the shadow picture.
                            prims.push(shadow_prim_instance);
                        }
                    }

                    // No point in adding a shadow here if there were no primitives
                    // added to the shadow.
                    if !prims.is_empty() {
                        let prim_list = PrimitiveList::new(
                            prims,
                            &self.resources.prim_interner,
                        );

                        // Create a picture that the shadow primitives will be added to. If the
                        // blur radius is 0, the code in Picture::prepare_for_render will
                        // detect this and mark the picture to be drawn directly into the
                        // parent picture, which avoids an intermediate surface and blur.
                        let blur_filter = FilterOp::Blur(std_deviation).sanitize();
                        let mut shadow_pic = PicturePrimitive::new_image(
                            self.picture_id_generator.next(),
                            Some(PictureCompositeMode::Filter(blur_filter)),
                            Picture3DContext::Out,
                            pipeline_id,
                            None,
                            is_passthrough,
                            raster_space,
                            prim_list,
                            pending_shadow.clip_and_scroll.spatial_node_index,
                            max_clip,
                            &self.clip_store,
                        );

                        // Create the primitive to draw the shadow picture into the scene.
                        let shadow_pic_index = self.prim_store.create_picture(shadow_pic);

                        let shadow_prim_key = PrimitiveKey::new(
                            true,
                            LayoutRect::zero(),
                            LayoutRect::max_rect(),
                            PrimitiveKeyKind::Unused,
                        );

                        let shadow_prim_data_handle = self.resources
                            .prim_interner
                            .intern(&shadow_prim_key, || {
                                PrimitiveSceneData {
                                    culling_rect: LayoutRect::zero(),
                                    is_backface_visible: true,
                                }
                            }
                        );

                        let shadow_prim_instance = PrimitiveInstance::new(
                            PrimitiveInstanceKind::Picture { pic_index: shadow_pic_index },
                            shadow_prim_data_handle,
                            pending_shadow.clip_and_scroll.clip_chain_id,
                            pending_shadow.clip_and_scroll.spatial_node_index,
                        );

                        // Add the shadow primitive. This must be done before pushing this
                        // picture on to the shadow stack, to avoid infinite recursion!
                        self.add_primitive_to_draw_list(shadow_prim_instance);
                    }
                }
                ShadowItem::Primitive(pending_primitive) => {
                    // For a normal primitive, if it has alpha > 0, then we add this
                    // as a normal primitive to the parent picture.
                    if pending_primitive.container.is_visible() {
                        let prim_instance = self.create_primitive(
                            &pending_primitive.info,
                            pending_primitive.clip_and_scroll.clip_chain_id,
                            pending_primitive.clip_and_scroll.spatial_node_index,
                            pending_primitive.container,
                        );
                        self.register_chase_primitive_by_rect(
                            &pending_primitive.info.rect,
                            &prim_instance,
                        );
                        self.add_primitive_to_hit_testing_list(&pending_primitive.info, pending_primitive.clip_and_scroll);
                        self.add_primitive_to_draw_list(prim_instance);
                    }
                }
            }
        }

        debug_assert!(items.is_empty());
        self.pending_shadow_items = items;
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

        let prim = BrushPrimitive::new(
            BrushKind::new_solid(color),
            None,
        );

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Brush(prim),
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
            PrimitiveContainer::Clear,
        );
    }

    pub fn add_line(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        wavy_line_thickness: f32,
        orientation: LineOrientation,
        color: &ColorF,
        style: LineStyle,
    ) {
        let container = PrimitiveContainer::LineDecoration {
            color: *color,
            style,
            orientation,
            wavy_line_thickness,
        };

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            container,
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
        let rect = info.rect;
        match border_item.details {
            BorderDetails::NinePatch(ref border) => {
                // Calculate the modified rect as specific by border-image-outset
                let origin = LayoutPoint::new(
                    rect.origin.x - border.outset.left,
                    rect.origin.y - border.outset.top,
                );
                let size = LayoutSize::new(
                    rect.size.width + border.outset.left + border.outset.right,
                    rect.size.height + border.outset.top + border.outset.bottom,
                );
                let rect = LayoutRect::new(origin, size);

                // Calculate the local texel coords of the slices.
                let px0 = 0.0;
                let px1 = border.slice.left as f32;
                let px2 = border.width as f32 - border.slice.right as f32;
                let px3 = border.width as f32;

                let py0 = 0.0;
                let py1 = border.slice.top as f32;
                let py2 = border.height as f32 - border.slice.bottom as f32;
                let py3 = border.height as f32;

                let tl_outer = LayoutPoint::new(rect.origin.x, rect.origin.y);
                let tl_inner = tl_outer + vec2(border_item.widths.left, border_item.widths.top);

                let tr_outer = LayoutPoint::new(rect.origin.x + rect.size.width, rect.origin.y);
                let tr_inner = tr_outer + vec2(-border_item.widths.right, border_item.widths.top);

                let bl_outer = LayoutPoint::new(rect.origin.x, rect.origin.y + rect.size.height);
                let bl_inner = bl_outer + vec2(border_item.widths.left, -border_item.widths.bottom);

                let br_outer = LayoutPoint::new(
                    rect.origin.x + rect.size.width,
                    rect.origin.y + rect.size.height,
                );
                let br_inner = br_outer - vec2(border_item.widths.right, border_item.widths.bottom);

                fn add_segment(
                    segments: &mut BrushSegmentVec,
                    rect: LayoutRect,
                    uv_rect: TexelRect,
                    repeat_horizontal: RepeatMode,
                    repeat_vertical: RepeatMode
                ) {
                    if uv_rect.uv1.x > uv_rect.uv0.x &&
                       uv_rect.uv1.y > uv_rect.uv0.y {

                        // Use segment relative interpolation for all
                        // instances in this primitive.
                        let mut brush_flags =
                            BrushFlags::SEGMENT_RELATIVE |
                            BrushFlags::SEGMENT_TEXEL_RECT;

                        // Enable repeat modes on the segment.
                        if repeat_horizontal == RepeatMode::Repeat {
                            brush_flags |= BrushFlags::SEGMENT_REPEAT_X;
                        }
                        if repeat_vertical == RepeatMode::Repeat {
                            brush_flags |= BrushFlags::SEGMENT_REPEAT_Y;
                        }

                        let segment = BrushSegment::new(
                            rect,
                            true,
                            EdgeAaSegmentMask::empty(),
                            [
                                uv_rect.uv0.x,
                                uv_rect.uv0.y,
                                uv_rect.uv1.x,
                                uv_rect.uv1.y,
                            ],
                            brush_flags,
                        );

                        segments.push(segment);
                    }
                }

                // Build the list of image segments
                let mut segments = BrushSegmentVec::new();

                // Top left
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(tl_outer.x, tl_outer.y, tl_inner.x, tl_inner.y),
                    TexelRect::new(px0, py0, px1, py1),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );
                // Top right
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(tr_inner.x, tr_outer.y, tr_outer.x, tr_inner.y),
                    TexelRect::new(px2, py0, px3, py1),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );
                // Bottom right
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(br_inner.x, br_inner.y, br_outer.x, br_outer.y),
                    TexelRect::new(px2, py2, px3, py3),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );
                // Bottom left
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(bl_outer.x, bl_inner.y, bl_inner.x, bl_outer.y),
                    TexelRect::new(px0, py2, px1, py3),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );

                // Center
                if border.fill {
                    add_segment(
                        &mut segments,
                        LayoutRect::from_floats(tl_inner.x, tl_inner.y, tr_inner.x, bl_inner.y),
                        TexelRect::new(px1, py1, px2, py2),
                        border.repeat_horizontal,
                        border.repeat_vertical
                    );
                }

                // Add edge segments.

                // Top
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(tl_inner.x, tl_outer.y, tr_inner.x, tl_inner.y),
                    TexelRect::new(px1, py0, px2, py1),
                    border.repeat_horizontal,
                    RepeatMode::Stretch,
                );
                // Bottom
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(bl_inner.x, bl_inner.y, br_inner.x, bl_outer.y),
                    TexelRect::new(px1, py2, px2, py3),
                    border.repeat_horizontal,
                    RepeatMode::Stretch,
                );
                // Left
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(tl_outer.x, tl_inner.y, tl_inner.x, bl_inner.y),
                    TexelRect::new(px0, py1, px1, py2),
                    RepeatMode::Stretch,
                    border.repeat_vertical,
                );
                // Right
                add_segment(
                    &mut segments,
                    LayoutRect::from_floats(tr_inner.x, tr_inner.y, br_outer.x, br_inner.y),
                    TexelRect::new(px2, py1, px3, py2),
                    RepeatMode::Stretch,
                    border.repeat_vertical,
                );
                let descriptor = BrushSegmentDescriptor {
                    segments,
                };

                let brush_kind = match border.source {
                    NinePatchBorderSource::Image(image_key) => {
                        BrushKind::Border {
                            source: BorderSource::Image(ImageRequest {
                                key: image_key,
                                rendering: ImageRendering::Auto,
                                tile: None,
                            })
                        }
                    }
                    NinePatchBorderSource::Gradient(gradient) => {
                        match self.create_brush_kind_for_gradient(
                            &info,
                            gradient.start_point,
                            gradient.end_point,
                            gradient_stops,
                            gradient.extend_mode,
                            LayoutSize::new(border.height as f32, border.width as f32),
                            LayoutSize::zero(),
                            pipeline_id,
                        ) {
                            Some(brush_kind) => brush_kind,
                            None => return,
                        }
                    }
                    NinePatchBorderSource::RadialGradient(gradient) => {
                        self.create_brush_kind_for_radial_gradient(
                            &info,
                            gradient.center,
                            gradient.start_offset * gradient.radius.width,
                            gradient.end_offset * gradient.radius.width,
                            gradient.radius.width / gradient.radius.height,
                            gradient_stops,
                            gradient.extend_mode,
                            LayoutSize::new(border.height as f32, border.width as f32),
                            LayoutSize::zero(),
                        )
                    }
                };

                let prim = PrimitiveContainer::Brush(
                    BrushPrimitive::new(brush_kind, Some(descriptor))
                );
                self.add_primitive(clip_and_scroll, info, Vec::new(), prim);
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

    pub fn create_brush_kind_for_gradient(
        &mut self,
        info: &LayoutPrimitiveInfo,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        stops: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
        pipeline_id: PipelineId,
    ) -> Option<BrushKind> {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        // TODO(gw): It seems like we should be able to look this up once in
        //           flatten_root() and pass to all children here to avoid
        //           some hash lookups?
        let display_list = self.scene.get_display_list_for_pipeline(pipeline_id);

        let mut max_alpha: f32 = 0.0;
        let mut min_alpha: f32 = 1.0;
        for stop in display_list.get(stops) {
            max_alpha = max_alpha.max(stop.color.a);
            min_alpha = min_alpha.min(stop.color.a);
        }

        // If all the stops have no alpha, then this
        // gradient can't contribute to the scene.
        if max_alpha <= 0.0 {
            return None;
        }

        // Save opacity of the stops for use in
        // selecting which pass this gradient
        // should be drawn in.
        let stops_opacity = PrimitiveOpacity::from_alpha(min_alpha);

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

        Some(BrushKind::LinearGradient {
            stops_range: stops,
            extend_mode,
            reverse_stops,
            start_point: sp,
            end_point: ep,
            stops_handle: GpuCacheHandle::new(),
            stretch_size,
            tile_spacing,
            visible_tiles: Vec::new(),
            stops_opacity,
        })
    }

    pub fn create_brush_kind_for_radial_gradient(
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
    ) -> BrushKind {
        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);

        BrushKind::RadialGradient {
            stops_range: stops,
            extend_mode,
            center,
            start_radius,
            end_radius,
            ratio_xy,
            stops_handle: GpuCacheHandle::new(),
            stretch_size,
            tile_spacing,
            visible_tiles: Vec::new(),
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
        let container = {
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

            PrimitiveContainer::TextRun {
                glyphs,
                font,
                offset,
                shadow: false,
            }
        };

        self.add_primitive(
            clip_and_scroll,
            prim_info,
            Vec::new(),
            container,
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

        let prim = BrushPrimitive::new(
            BrushKind::Image {
                request: ImageRequest {
                    key: image_key,
                    rendering: image_rendering,
                    tile: None,
                },
                alpha_type,
                stretch_size,
                tile_spacing,
                color,
                source: ImageSource::Default,
                sub_rect,
                visible_tiles: Vec::new(),
                opacity_binding: OpacityBinding::new(),
            },
            None,
        );

        self.add_primitive(
            clip_and_scroll,
            &info,
            Vec::new(),
            PrimitiveContainer::Brush(prim),
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

        let prim = BrushPrimitive::new(
            BrushKind::YuvImage {
                yuv_key,
                format,
                color_depth,
                color_space,
                image_rendering,
            },
            None,
        );

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Brush(prim),
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

    /// For a Preserve3D context, cut the sequence of the immediate flat children
    /// recorded so far and generate a picture from them.
    pub fn cut_flat_item_sequence(
        &mut self,
        picture_id_generator: &mut PictureIdGenerator,
        prim_store: &mut PrimitiveStore,
        prim_interner: &PrimitiveDataInterner,
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

        let prim_list = PrimitiveList::new(
            mem::replace(&mut self.primitives, Vec::new()),
            prim_interner,
        );

        let container_picture = PicturePrimitive::new_image(
            picture_id_generator.next(),
            Some(PictureCompositeMode::Blit),
            flat_items_context_3d,
            self.pipeline_id,
            None,
            true,
            self.requested_raster_space,
            prim_list,
            self.spatial_node_index,
            LayoutRect::max_rect(),
            clip_store,
        );

        let pic_index = prim_store.create_picture(container_picture);

        Some(PrimitiveInstance::new(
            PrimitiveInstanceKind::Picture { pic_index },
            self.primitive_data_handle,
            self.clip_chain_id,
            self.spatial_node_index,
        ))
    }
}

/// A primitive that is added while a shadow context is
/// active is stored as a pending primitive and only
/// added to pictures during pop_all_shadows.
struct PendingPrimitive {
    clip_and_scroll: ScrollNodeAndClipChain,
    info: LayoutPrimitiveInfo,
    container: PrimitiveContainer,
}

/// As shadows are pushed, they are stored as pending
/// shadows, and handled at once during pop_all_shadows.
struct PendingShadow {
    shadow: Shadow,
    clip_and_scroll: ScrollNodeAndClipChain,
}

enum ShadowItem {
    Shadow(PendingShadow),
    Primitive(PendingPrimitive),
}


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderDetails, BorderDisplayItem, BuiltDisplayListIter, ClipAndScrollInfo};
use api::{ClipId, ColorF, ComplexClipRegion, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{DevicePixelScale, DeviceUintRect, DisplayItemRef, Epoch, ExtendMode, ExternalScrollId};
use api::{FilterOp, FontInstanceKey, GlyphInstance, GlyphOptions, GlyphRasterSpace, GradientStop};
use api::{IframeDisplayItem, ImageKey, ImageRendering, ItemRange, LayoutPoint};
use api::{LayoutPrimitiveInfo, LayoutRect, LayoutVector2D, LayoutSize, LayoutTransform};
use api::{LineOrientation, LineStyle, LocalClip, NinePatchBorderSource, PipelineId};
use api::{PropertyBinding, RepeatMode, ScrollFrameDisplayItem, ScrollSensitivity, Shadow};
use api::{SpecificDisplayItem, StackingContext, StickyFrameDisplayItem, TexelRect};
use api::{TransformStyle, YuvColorSpace, YuvData};
use app_units::Au;
use clip::{ClipRegion, ClipSource, ClipSources, ClipStore};
use clip_scroll_node::{ClipScrollNode, NodeType, StickyFrameInfo};
use clip_scroll_tree::{ClipChainIndex, ClipScrollNodeIndex, ClipScrollTree};
use euclid::{SideOffsets2D, vec2};
use frame_builder::{FrameBuilder, FrameBuilderConfig};
use glyph_rasterizer::FontInstance;
use gpu_types::BrushFlags;
use hit_test::{HitTestingItem, HitTestingRun};
use image::simplify_repeated_primitive;
use internal_types::{FastHashMap, FastHashSet};
use picture::PictureCompositeMode;
use prim_store::{BrushClipMaskKind, BrushKind, BrushPrimitive, BrushSegmentDescriptor, CachedGradient};
use prim_store::{CachedGradientIndex, EdgeAaSegmentMask, ImageSource};
use prim_store::{BrushSegment, PictureIndex, PrimitiveContainer, PrimitiveIndex, PrimitiveStore};
use prim_store::{OpacityBinding, ScrollNodeAndClipChain, TextRunPrimitiveCpu};
use render_backend::{DocumentView};
use resource_cache::{FontInstanceMap, ImageRequest};
use scene::{Scene, ScenePipeline, StackingContextHelpers};
use scene_builder::{BuiltScene, SceneRequest};
use std::{f32, mem, usize};
use tiling::{CompositeOps, ScrollbarPrimitive};
use util::{MaxRect, RectHelpers, recycle_vec};

static DEFAULT_SCROLLBAR_COLOR: ColorF = ColorF {
    r: 0.3,
    g: 0.3,
    b: 0.3,
    a: 0.6,
};

/// A data structure that keeps track of mapping between API ClipIds and the indices used
/// internally in the ClipScrollTree to avoid having to do HashMap lookups. ClipIdToIndexMapper is
/// responsible for mapping both ClipId to ClipChainIndex and ClipId to ClipScrollNodeIndex.  We
/// also include two small LRU caches. Currently the caches are small (1 entry), but in the future
/// we could use uluru here to do something more involved.
#[derive(Default)]
pub struct ClipIdToIndexMapper {
    /// A map which converts a ClipId for a clipping node or an API-defined ClipChain into
    /// a ClipChainIndex, which is the index used internally in the ClipScrollTree to
    /// identify ClipChains.
    clip_chain_map: FastHashMap<ClipId, ClipChainIndex>,

    /// The last mapped ClipChainIndex, used to avoid having to do lots of consecutive
    /// HashMap lookups.
    cached_clip_chain_index: Option<(ClipId, ClipChainIndex)>,

    /// The offset in the ClipScrollTree's array of ClipScrollNodes for a particular pipeline.
    /// This is used to convert a ClipId into a ClipScrollNodeIndex.
    pipeline_offsets: FastHashMap<PipelineId, usize>,

    /// The last mapped pipeline offset for this mapper. This is used to avoid having to
    /// consult `pipeline_offsets` repeatedly when flattening the display list.
    cached_pipeline_offset: Option<(PipelineId, usize)>,

    /// The next available pipeline offset for ClipScrollNodeIndex. When we encounter a pipeline
    /// we will use this value and increment it by the total number of ClipScrollNodes in the
    /// pipeline's display list.
    next_available_offset: usize,
}

impl ClipIdToIndexMapper {
    pub fn add_clip_chain(&mut self, id: ClipId, index: ClipChainIndex) {
        debug_assert!(!self.clip_chain_map.contains_key(&id));
        self.clip_chain_map.insert(id, index);
    }

    pub fn map_to_parent_clip_chain(&mut self, id: ClipId, parent_id: &ClipId) {
        let parent_chain_index = self.get_clip_chain_index(parent_id);
        self.add_clip_chain(id, parent_chain_index);
    }

    pub fn get_clip_chain_index(&mut self, id: &ClipId) -> ClipChainIndex {
        match self.cached_clip_chain_index {
            Some((cached_id, cached_clip_chain_index)) if cached_id == *id =>
                return cached_clip_chain_index,
            _ => {}
        }

        self.clip_chain_map[id]
    }

    pub fn get_clip_chain_index_and_cache_result(&mut self, id: &ClipId) -> ClipChainIndex {
        let index = self.get_clip_chain_index(id);
        self.cached_clip_chain_index = Some((*id, index));
        index
    }

    pub fn map_clip_and_scroll(&mut self, info: &ClipAndScrollInfo) -> ScrollNodeAndClipChain {
        ScrollNodeAndClipChain::new(
            self.get_node_index(info.scroll_node_id),
            self.get_clip_chain_index_and_cache_result(&info.clip_node_id())
        )
    }

    pub fn simple_scroll_and_clip_chain(&mut self, id: &ClipId) -> ScrollNodeAndClipChain {
        self.map_clip_and_scroll(&ClipAndScrollInfo::simple(*id))
    }

    pub fn initialize_for_pipeline(&mut self, pipeline: &ScenePipeline) {
        debug_assert!(!self.pipeline_offsets.contains_key(&pipeline.pipeline_id));
        self.pipeline_offsets.insert(pipeline.pipeline_id, self.next_available_offset);
        self.next_available_offset += pipeline.display_list.total_clip_ids();
    }

    pub fn get_node_index(&mut self, id: ClipId) -> ClipScrollNodeIndex {
        let (index, pipeline_id) = match id {
            ClipId::Clip(index, pipeline_id) => (index, pipeline_id),
            ClipId::ClipChain(_) => panic!("Tried to use ClipChain as scroll node."),
        };

        let pipeline_offset = match self.cached_pipeline_offset {
            Some((last_used_id, offset)) if last_used_id == pipeline_id => offset,
            _ => {
                let offset = self.pipeline_offsets[&pipeline_id];
                self.cached_pipeline_offset = Some((pipeline_id, offset));
                offset
            }
        };

        ClipScrollNodeIndex(pipeline_offset + index)
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

    /// Used to track the latest flattened epoch for each pipeline.
    pipeline_epochs: Vec<(PipelineId, Epoch)>,

    /// A set of pipelines that the caller has requested be made available as
    /// output textures.
    output_pipelines: &'a FastHashSet<PipelineId>,

    /// A list of replacements to make in order to properly handle fixed position
    /// content as well as stacking contexts that create reference frames.
    replacements: Vec<(ClipId, ClipId)>,

    /// The data structure that converting between ClipId and the various index
    /// types that the ClipScrollTree uses.
    id_to_index_mapper: ClipIdToIndexMapper,

    /// A stack of scroll nodes used during display list processing to properly
    /// parent new scroll nodes.
    reference_frame_stack: Vec<(ClipId, ClipScrollNodeIndex)>,

    /// A stack of stacking context properties.
    sc_stack: Vec<FlattenedStackingContext>,

    /// A stack of the current pictures.
    picture_stack: Vec<PictureIndex>,

    /// A stack of the currently active shadows
    shadow_stack: Vec<(Shadow, PictureIndex)>,

    /// A list of scrollbar primitives.
    pub scrollbar_prims: Vec<ScrollbarPrimitive>,

    /// The store of primitives.
    pub prim_store: PrimitiveStore,

    /// Information about all primitives involved in hit testing.
    pub hit_testing_runs: Vec<HitTestingRun>,

    /// The store which holds all complex clipping information.
    pub clip_store: ClipStore,

    /// The configuration to use for the FrameBuilder. We consult this in
    /// order to determine the default font.
    pub config: FrameBuilderConfig,

    /// The gradients collecting during display list flattening.
    pub cached_gradients: Vec<CachedGradient>,
}

impl<'a> DisplayListFlattener<'a> {
    pub fn create_frame_builder(
        old_builder: FrameBuilder,
        scene: &Scene,
        clip_scroll_tree: &mut ClipScrollTree,
        font_instances: FontInstanceMap,
        view: &DocumentView,
        output_pipelines: &FastHashSet<PipelineId>,
        frame_builder_config: &FrameBuilderConfig,
        new_scene: &mut Scene,
    ) -> FrameBuilder {
        // We checked that the root pipeline is available on the render backend.
        let root_pipeline_id = scene.root_pipeline_id.unwrap();
        let root_pipeline = scene.pipelines.get(&root_pipeline_id).unwrap();

        let root_epoch = scene.pipeline_epochs[&root_pipeline_id];

        let background_color = root_pipeline
            .background_color
            .and_then(|color| if color.a > 0.0 { Some(color) } else { None });

        let mut flattener = DisplayListFlattener {
            scene,
            clip_scroll_tree,
            font_instances,
            config: *frame_builder_config,
            pipeline_epochs: Vec::new(),
            replacements: Vec::new(),
            output_pipelines,
            id_to_index_mapper: ClipIdToIndexMapper::default(),
            hit_testing_runs: recycle_vec(old_builder.hit_testing_runs),
            cached_gradients: recycle_vec(old_builder.cached_gradients),
            scrollbar_prims: recycle_vec(old_builder.scrollbar_prims),
            reference_frame_stack: Vec::new(),
            picture_stack: Vec::new(),
            shadow_stack: Vec::new(),
            sc_stack: Vec::new(),
            prim_store: old_builder.prim_store.recycle(),
            clip_store: old_builder.clip_store.recycle(),
        };

        flattener.id_to_index_mapper.initialize_for_pipeline(root_pipeline);
        flattener.push_root(
            root_pipeline_id,
            &root_pipeline.viewport_size,
            &root_pipeline.content_size,
        );
        flattener.setup_viewport_offset(view.inner_rect, view.accumulated_scale_factor());
        flattener.flatten_root(root_pipeline, &root_pipeline.viewport_size);

        debug_assert!(flattener.picture_stack.is_empty());

        new_scene.root_pipeline_id = Some(root_pipeline_id);
        new_scene.pipeline_epochs.insert(root_pipeline_id, root_epoch);
        new_scene.pipeline_epochs.extend(flattener.pipeline_epochs.drain(..));
        new_scene.pipelines = scene.pipelines.clone();

        FrameBuilder::with_display_list_flattener(
            view.inner_rect,
            background_color,
            view.window_size,
            flattener
        )
    }

    /// Since WebRender still handles fixed position and reference frame content internally
    /// we need to apply this table of id replacements only to the id that affects the
    /// position of a node. We can eventually remove this when clients start handling
    /// reference frames themselves. This method applies these replacements.
    fn apply_scroll_frame_id_replacement(&self, index: ClipId) -> ClipId {
        match self.replacements.last() {
            Some(&(to_replace, replacement)) if to_replace == index => replacement,
            _ => index,
        }
    }

    fn get_complex_clips(
        &self,
        pipeline_id: PipelineId,
        complex_clips: ItemRange<ComplexClipRegion>,
    ) -> Vec<ComplexClipRegion> {
        if complex_clips.is_empty() {
            return vec![];
        }

        self.scene
            .pipelines
            .get(&pipeline_id)
            .expect("No display list?")
            .display_list
            .get(complex_clips)
            .collect()
    }

    fn get_clip_chain_items(
        &self,
        pipeline_id: PipelineId,
        items: ItemRange<ClipId>,
    ) -> Vec<ClipId> {
        if items.is_empty() {
            return vec![];
        }

        self.scene
            .pipelines
            .get(&pipeline_id)
            .expect("No display list?")
            .display_list
            .get(items)
            .collect()
    }

    fn flatten_root(&mut self, pipeline: &'a ScenePipeline, frame_size: &LayoutSize) {
        let pipeline_id = pipeline.pipeline_id;
        let reference_frame_info = self.id_to_index_mapper.simple_scroll_and_clip_chain(
            &ClipId::root_reference_frame(pipeline_id)
        );

        let root_scroll_node = ClipId::root_scroll_node(pipeline_id);
        let scroll_frame_info = self.id_to_index_mapper.simple_scroll_and_clip_chain(
            &root_scroll_node,
        );

        self.push_stacking_context(
            pipeline_id,
            CompositeOps::default(),
            TransformStyle::Flat,
            true,
            true,
            root_scroll_node,
            None,
            GlyphRasterSpace::Screen,
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
                        None,
                        Vec::new(),
                    );
                }
            }
        }

        self.flatten_items(&mut pipeline.display_list.iter(), pipeline_id, LayoutVector2D::zero());

        if self.config.enable_scrollbars {
            let scrollbar_rect = LayoutRect::new(LayoutPoint::zero(), LayoutSize::new(10.0, 70.0));
            let container_rect = LayoutRect::new(LayoutPoint::zero(), *frame_size);
            self.add_scroll_bar(
                reference_frame_info,
                &LayoutPrimitiveInfo::new(scrollbar_rect),
                DEFAULT_SCROLLBAR_COLOR,
                ScrollbarInfo(scroll_frame_info.scroll_node_id, container_rect),
            );
        }

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

        let index = self.id_to_index_mapper.get_node_index(info.id);
        self.clip_scroll_tree.add_sticky_frame(
            index,
            clip_and_scroll.scroll_node_id, /* parent id */
            sticky_frame_info,
            info.id.pipeline_id(),
        );
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
        // ClipScrollNode::scroll(..) API as well as for properly setting sticky
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

    fn flatten_stacking_context(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        item: &DisplayItemRef,
        stacking_context: &StackingContext,
        unreplaced_scroll_id: ClipId,
        scroll_node_id: ClipId,
        mut reference_frame_relative_offset: LayoutVector2D,
        is_backface_visible: bool,
    ) {
        // Avoid doing unnecessary work for empty stacking contexts.
        if traversal.current_stacking_context_empty() {
            traversal.skip_current_stacking_context();
            return;
        }

        let composition_operations = {
            // TODO(optimization?): self.traversal.display_list()
            let display_list = &self
                .scene
                .pipelines
                .get(&pipeline_id)
                .expect("No display list?!")
                .display_list;
            CompositeOps::new(
                stacking_context.filter_ops_for_compositing(display_list, item.filters()),
                stacking_context.mix_blend_mode_for_compositing(),
            )
        };

        let bounds = item.rect();
        reference_frame_relative_offset += bounds.origin.to_vector();

        // If we have a transformation or a perspective, we should have been assigned a new
        // reference frame id. This means this stacking context establishes a new reference frame.
        // Descendant fixed position content will be positioned relative to us.
        if let Some(reference_frame_id) = stacking_context.reference_frame_id {
            debug_assert!(
                stacking_context.transform.is_some() ||
                stacking_context.perspective.is_some()
            );

            self.push_reference_frame(
                reference_frame_id,
                Some(scroll_node_id),
                pipeline_id,
                stacking_context.transform,
                stacking_context.perspective,
                reference_frame_relative_offset,
            );
            self.replacements.push((unreplaced_scroll_id, reference_frame_id));
            reference_frame_relative_offset = LayoutVector2D::zero();
        }

        // We apply the replacements one more time in case we need to set it to a replacement
        // that we just pushed above.
        let final_scroll_node = self.apply_scroll_frame_id_replacement(unreplaced_scroll_id);
        self.push_stacking_context(
            pipeline_id,
            composition_operations,
            stacking_context.transform_style,
            is_backface_visible,
            false,
            final_scroll_node,
            stacking_context.clip_node_id,
            stacking_context.glyph_raster_space,
        );

        self.flatten_items(
            traversal,
            pipeline_id,
            reference_frame_relative_offset,
        );

        if stacking_context.reference_frame_id.is_some() {
            self.replacements.pop();
            self.pop_reference_frame();
        }

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
                //TODO: assert/debug_assert?
                error!("Unknown pipeline used for iframe {:?}", info);
                return
            },
        };

        self.id_to_index_mapper.initialize_for_pipeline(pipeline);

        self.add_clip_node(
            info.clip_id,
            clip_and_scroll_ids.scroll_node_id,
            ClipRegion::create_for_clip_node_with_local_clip(
                &LocalClip::from(*item.clip_rect()),
                reference_frame_relative_offset
            ),
        );

        let epoch = self.scene.pipeline_epochs[&iframe_pipeline_id];
        self.pipeline_epochs.push((iframe_pipeline_id, epoch));

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

        self.pop_reference_frame();
    }

    fn flatten_item<'b>(
        &'b mut self,
        item: DisplayItemRef<'a, 'b>,
        pipeline_id: PipelineId,
        reference_frame_relative_offset: LayoutVector2D,
    ) -> Option<BuiltDisplayListIter<'a>> {
        let mut clip_and_scroll_ids = item.clip_and_scroll();
        let unreplaced_scroll_id = clip_and_scroll_ids.scroll_node_id;
        clip_and_scroll_ids.scroll_node_id =
            self.apply_scroll_frame_id_replacement(clip_and_scroll_ids.scroll_node_id);
        let clip_and_scroll = self.id_to_index_mapper.map_clip_and_scroll(&clip_and_scroll_ids);

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
                );
            }
            SpecificDisplayItem::YuvImage(ref info) => {
                self.add_yuv_image(
                    clip_and_scroll,
                    &prim_info,
                    info.yuv_data,
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
                );
            }
            SpecificDisplayItem::Rectangle(ref info) => {
                self.add_solid_rectangle(
                    clip_and_scroll,
                    &prim_info,
                    info.color,
                    None,
                    Vec::new(),
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
                self.add_gradient(
                    clip_and_scroll,
                    &prim_info,
                    info.gradient.start_point,
                    info.gradient.end_point,
                    item.gradient_stops(),
                    item.display_list().get(item.gradient_stops()).count(),
                    info.gradient.extend_mode,
                    info.tile_size,
                    info.tile_spacing,
                );
            }
            SpecificDisplayItem::RadialGradient(ref info) => {
                self.add_radial_gradient(
                    clip_and_scroll,
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
                    item.display_list().get(item.gradient_stops()).count(),
                );
            }
            SpecificDisplayItem::PushStackingContext(ref info) => {
                let mut subtraversal = item.sub_iter();
                self.flatten_stacking_context(
                    &mut subtraversal,
                    pipeline_id,
                    &item,
                    &info.stacking_context,
                    unreplaced_scroll_id,
                    clip_and_scroll_ids.scroll_node_id,
                    reference_frame_relative_offset,
                    prim_info.is_backface_visible,
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
                let items = self.get_clip_chain_items(pipeline_id, item.clip_chain_items())
                                .iter()
                                .map(|id| self.id_to_index_mapper.get_node_index(*id))
                                .collect();
                let parent = info.parent.map(|id|
                     self.id_to_index_mapper.get_clip_chain_index(&ClipId::ClipChain(id))
                );
                let clip_chain_index =
                    self.clip_scroll_tree.add_clip_chain_descriptor(parent, items);
                self.id_to_index_mapper.add_clip_chain(ClipId::ClipChain(info.id), clip_chain_index);
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

            SpecificDisplayItem::PopStackingContext => {
                unreachable!("Should have returned in parent method.")
            }
            SpecificDisplayItem::PushShadow(shadow) => {
                let mut prim_info = prim_info.clone();
                prim_info.rect = LayoutRect::zero();
                self
                    .push_shadow(shadow, clip_and_scroll, &prim_info);
            }
            SpecificDisplayItem::PopAllShadows => {
                self.pop_all_shadows();
            }
        }
        None
    }

    /// Create a primitive and add it to the prim store. This method doesn't
    /// add the primitive to the draw list, so can be used for creating
    /// sub-primitives.
    pub fn create_primitive(
        &mut self,
        info: &LayoutPrimitiveInfo,
        clip_sources: Vec<ClipSource>,
        container: PrimitiveContainer,
    ) -> PrimitiveIndex {
        let stacking_context = self.sc_stack.last().expect("bug: no stacking context!");

        let clip_sources = if clip_sources.is_empty() {
            None
        } else {
            Some(self.clip_store.insert(ClipSources::new(clip_sources)))
        };

        self.prim_store.add_primitive(
            &info.rect,
            &info.clip_rect,
            info.is_backface_visible && stacking_context.is_backface_visible,
            clip_sources,
            info.tag,
            container,
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
        prim_index: PrimitiveIndex,
        clip_and_scroll: ScrollNodeAndClipChain,
    ) {
        // Add primitive to the top-most Picture on the stack.
        let pic_index = self.picture_stack.last().unwrap();
        let pic = &mut self.prim_store.pictures[pic_index.0];
        pic.add_primitive(prim_index, clip_and_scroll);
    }

    /// Convenience interface that creates a primitive entry and adds it
    /// to the draw list.
    pub fn add_primitive(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        clip_sources: Vec<ClipSource>,
        container: PrimitiveContainer,
    ) {
        if !self.shadow_stack.is_empty() {
            // TODO(gw): Restructure this so we don't need to move the shadow
            //           stack out (borrowck due to create_primitive below).
            let shadow_stack = mem::replace(&mut self.shadow_stack, Vec::new());
            for &(ref shadow, shadow_pic_index) in &shadow_stack {
                // Offset the local rect and clip rect by the shadow offset.
                let mut info = info.clone();
                info.rect = info.rect.translate(&shadow.offset);
                info.clip_rect = info.clip_rect.translate(&shadow.offset);

                // Offset any local clip sources by the shadow offset.
                let clip_sources: Vec<ClipSource> = clip_sources
                    .iter()
                    .map(|cs| cs.offset(&shadow.offset))
                    .collect();

                // Construct and add a primitive for the given shadow.
                let shadow_prim_index = self.create_primitive(
                    &info,
                    clip_sources,
                    container.create_shadow(shadow),
                );

                // Add the new primitive to the shadow picture.
                let shadow_pic = &mut self.prim_store.pictures[shadow_pic_index.0];
                shadow_pic.add_primitive(shadow_prim_index, clip_and_scroll);
            }
            self.shadow_stack = shadow_stack;
        }

        if container.is_visible() {
            let prim_index = self.create_primitive(info, clip_sources, container);
            self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
            self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
        }
    }

    pub fn push_stacking_context(
        &mut self,
        pipeline_id: PipelineId,
        composite_ops: CompositeOps,
        transform_style: TransformStyle,
        is_backface_visible: bool,
        is_pipeline_root: bool,
        positioning_node: ClipId,
        clipping_node: Option<ClipId>,
        glyph_raster_space: GlyphRasterSpace,
    ) {
        let clip_chain_id = match clipping_node {
            Some(ref clipping_node) => self.id_to_index_mapper.get_clip_chain_index(clipping_node),
            None => ClipChainIndex(0), // This means no clipping.
        };
        let clip_and_scroll = ScrollNodeAndClipChain::new(
            self.id_to_index_mapper.get_node_index(positioning_node),
            clip_chain_id
        );

        // Construct the necessary set of Picture primitives
        // to draw this stacking context.
        let current_reference_frame_index = self.current_reference_frame_index();

        // An arbitrary large clip rect. For now, we don't
        // specify a clip specific to the stacking context.
        // However, now that they are represented as Picture
        // primitives, we can apply any kind of clip mask
        // to them, as for a normal primitive. This is needed
        // to correctly handle some CSS cases (see #1957).
        let max_clip = LayoutRect::max_rect();

        // If there is no root picture, create one for the main framebuffer.
        if self.sc_stack.is_empty() {
            // Should be no pictures at all if the stack is empty...
            debug_assert!(self.prim_store.pictures.is_empty());
            debug_assert_eq!(transform_style, TransformStyle::Flat);

            // This picture stores primitive runs for items on the
            // main framebuffer.
            let pic_index = self.prim_store.add_image_picture(
                None,
                false,
                pipeline_id,
                current_reference_frame_index,
                None,
                true,
            );

            self.picture_stack.push(pic_index);
        } else if composite_ops.mix_blend_mode.is_some() && self.sc_stack.len() > 2 {
            // If we have a mix-blend-mode, and we aren't the primary framebuffer,
            // the stacking context needs to be isolated to blend correctly as per
            // the CSS spec.
            // TODO(gw): The way we detect not being the primary framebuffer (len > 2)
            //           is hacky and depends on how we create a root stacking context
            //           during flattening.
            let parent_pic_index = self.picture_stack.last().unwrap();
            let parent_pic = &mut self.prim_store.pictures[parent_pic_index.0];

            // If not already isolated for some other reason,
            // make this picture as isolated.
            if parent_pic.composite_mode.is_none() {
                parent_pic.composite_mode = Some(PictureCompositeMode::Blit);
            }
        }

        // Get the transform-style of the parent stacking context,
        // which determines if we *might* need to draw this on
        // an intermediate surface for plane splitting purposes.
        let parent_transform_style = match self.sc_stack.last() {
            Some(sc) => sc.transform_style,
            None => TransformStyle::Flat,
        };

        // If this is preserve-3d *or* the parent is, then this stacking
        // context is participating in the 3d rendering context. In that
        // case, hoist the picture up to the 3d rendering context
        // container, so that it's rendered as a sibling with other
        // elements in this context.
        let participating_in_3d_context =
            composite_ops.count() == 0 &&
            (parent_transform_style == TransformStyle::Preserve3D ||
             transform_style == TransformStyle::Preserve3D);

        // If this is participating in a 3d context *and* the
        // parent was not a 3d context, then this must be the
        // element that establishes a new 3d context.
        let establishes_3d_context =
            participating_in_3d_context &&
            parent_transform_style == TransformStyle::Flat;

        let rendering_context_3d_pic_index = if establishes_3d_context {
            // If establishing a 3d context, we need to add a picture
            // that will be the container for all the planes and any
            // un-transformed content.
            let container_index = self.prim_store.add_image_picture(
                None,
                false,
                pipeline_id,
                current_reference_frame_index,
                None,
                true,
            );

            let prim = BrushPrimitive::new_picture(container_index);

            let prim_index = self.prim_store.add_primitive(
                &LayoutRect::zero(),
                &max_clip,
                is_backface_visible,
                None,
                None,
                PrimitiveContainer::Brush(prim),
            );

            let parent_pic_index = *self.picture_stack.last().unwrap();

            let pic = &mut self.prim_store.pictures[parent_pic_index.0];
            pic.add_primitive(prim_index, clip_and_scroll);

            self.picture_stack.push(container_index);

            Some(container_index)
        } else {
            None
        };

        let mut parent_pic_index = if !establishes_3d_context && participating_in_3d_context {
            // If we're in a 3D context, we will parent the picture
            // to the first stacking context we find that is a
            // 3D rendering context container. This follows the spec
            // by hoisting these items out into the same 3D context
            // for plane splitting.
            self.sc_stack
                .iter()
                .rev()
                .find(|sc| sc.rendering_context_3d_pic_index.is_some())
                .map(|sc| sc.rendering_context_3d_pic_index.unwrap())
                .unwrap()
        } else {
            *self.picture_stack.last().unwrap()
        };

        // For each filter, create a new image with that composite mode.
        for filter in composite_ops.filters.iter().rev() {
            let src_pic_index = self.prim_store.add_image_picture(
                Some(PictureCompositeMode::Filter(*filter)),
                false,
                pipeline_id,
                current_reference_frame_index,
                None,
                true,
            );

            let src_prim = BrushPrimitive::new_picture(src_pic_index);
            let src_prim_index = self.prim_store.add_primitive(
                &LayoutRect::zero(),
                &max_clip,
                is_backface_visible,
                None,
                None,
                PrimitiveContainer::Brush(src_prim),
            );

            let parent_pic = &mut self.prim_store.pictures[parent_pic_index.0];
            parent_pic_index = src_pic_index;

            parent_pic.add_primitive(src_prim_index, clip_and_scroll);

            self.picture_stack.push(src_pic_index);
        }

        // Same for mix-blend-mode.
        if let Some(mix_blend_mode) = composite_ops.mix_blend_mode {
            let src_pic_index = self.prim_store.add_image_picture(
                Some(PictureCompositeMode::MixBlend(mix_blend_mode)),
                false,
                pipeline_id,
                current_reference_frame_index,
                None,
                true,
            );

            let src_prim = BrushPrimitive::new_picture(src_pic_index);

            let src_prim_index = self.prim_store.add_primitive(
                &LayoutRect::zero(),
                &max_clip,
                is_backface_visible,
                None,
                None,
                PrimitiveContainer::Brush(src_prim),
            );

            let parent_pic = &mut self.prim_store.pictures[parent_pic_index.0];
            parent_pic_index = src_pic_index;
            parent_pic.add_primitive(src_prim_index, clip_and_scroll);

            self.picture_stack.push(src_pic_index);
        }

        // By default, this picture will be collapsed into
        // the owning target.
        let mut composite_mode = None;
        let mut frame_output_pipeline_id = None;

        // If this stacking context if the root of a pipeline, and the caller
        // has requested it as an output frame, create a render task to isolate it.
        if is_pipeline_root && self.output_pipelines.contains(&pipeline_id) {
            composite_mode = Some(PictureCompositeMode::Blit);
            frame_output_pipeline_id = Some(pipeline_id);
        }

        // Force an intermediate surface if the stacking context
        // has a clip node. In the future, we may decide during
        // prepare step to skip the intermediate surface if the
        // clip node doesn't affect the stacking context rect.
        if participating_in_3d_context || clipping_node.is_some() {
            // TODO(gw): For now, as soon as this picture is in
            //           a 3D context, we draw it to an intermediate
            //           surface and apply plane splitting. However,
            //           there is a large optimization opportunity here.
            //           During culling, we can check if there is actually
            //           perspective present, and skip the plane splitting
            //           completely when that is not the case.
            composite_mode = Some(PictureCompositeMode::Blit);
        }

        // Add picture for this actual stacking context contents to render into.
        let pic_index = self.prim_store.add_image_picture(
            composite_mode,
            participating_in_3d_context,
            pipeline_id,
            current_reference_frame_index,
            frame_output_pipeline_id,
            true,
        );

        // Create a brush primitive that draws this picture.
        let sc_prim = BrushPrimitive::new_picture(pic_index);

        // Add the brush to the parent picture.
        let sc_prim_index = self.prim_store.add_primitive(
            &LayoutRect::zero(),
            &max_clip,
            is_backface_visible,
            None,
            None,
            PrimitiveContainer::Brush(sc_prim),
        );

        let parent_pic = &mut self.prim_store.pictures[parent_pic_index.0];
        parent_pic.add_primitive(sc_prim_index, clip_and_scroll);

        // Add this as the top-most picture for primitives to be added to.
        self.picture_stack.push(pic_index);

        // Push the SC onto the stack, so we know how to handle things in
        // pop_stacking_context.
        let sc = FlattenedStackingContext {
            composite_ops,
            is_backface_visible,
            pipeline_id,
            transform_style,
            rendering_context_3d_pic_index,
            glyph_raster_space,
        };

        self.sc_stack.push(sc);
    }

    pub fn pop_stacking_context(&mut self) {
        let sc = self.sc_stack.pop().unwrap();

        // Always pop at least the main picture for this stacking context.
        let mut pop_count = 1;

        // Remove the picture for any filter/mix-blend-mode effects.
        pop_count += sc.composite_ops.count();

        // Remove the 3d context container if created
        if sc.rendering_context_3d_pic_index.is_some() {
            pop_count += 1;
        }

        for _ in 0 .. pop_count {
            let pic_index = self
                .picture_stack
                .pop()
                .expect("bug: mismatched picture stack");
            self.prim_store.optimize_picture_if_possible(pic_index);
        }

        // By the time the stacking context stack is empty, we should
        // also have cleared the picture stack.
        if self.sc_stack.is_empty() {
            self.picture_stack.pop().expect("bug: picture stack invalid");
            debug_assert!(self.picture_stack.is_empty());
        }

        assert!(
            self.shadow_stack.is_empty(),
            "Found unpopped text shadows when popping stacking context!"
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
    ) -> ClipScrollNodeIndex {
        let index = self.id_to_index_mapper.get_node_index(reference_frame_id);
        let node = ClipScrollNode::new_reference_frame(
            parent_id.map(|id| self.id_to_index_mapper.get_node_index(id)),
            source_transform,
            source_perspective,
            origin_in_parent_reference_frame,
            pipeline_id,
        );
        self.clip_scroll_tree.add_node(node, index);
        self.reference_frame_stack.push((reference_frame_id, index));

        match parent_id {
            Some(ref parent_id) =>
                self.id_to_index_mapper.map_to_parent_clip_chain(reference_frame_id, parent_id),
            _ => self.id_to_index_mapper.add_clip_chain(reference_frame_id, ClipChainIndex(0)),
        }
        index
    }

    pub fn current_reference_frame_index(&self) -> ClipScrollNodeIndex {
        self.reference_frame_stack.last().unwrap().1
    }

    pub fn setup_viewport_offset(
        &mut self,
        inner_rect: DeviceUintRect,
        device_pixel_scale: DevicePixelScale,
    ) {
        let viewport_offset = (inner_rect.origin.to_vector().to_f32() / device_pixel_scale).round();
        let root_id = self.clip_scroll_tree.root_reference_frame_index();
        let root_node = &mut self.clip_scroll_tree.nodes[root_id.0];
        if let NodeType::ReferenceFrame(ref mut info) = root_node.node_type {
            info.resolved_transform =
                LayoutVector2D::new(viewport_offset.x, viewport_offset.y).into();
        }
    }

    pub fn push_root(
        &mut self,
        pipeline_id: PipelineId,
        viewport_size: &LayoutSize,
        content_size: &LayoutSize,
    ) {
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

    pub fn add_clip_node(
        &mut self,
        new_node_id: ClipId,
        parent_id: ClipId,
        clip_region: ClipRegion,
    ) -> ClipScrollNodeIndex {
        let clip_sources = ClipSources::from(clip_region);
        let handle = self.clip_store.insert(clip_sources);

        let node_index = self.id_to_index_mapper.get_node_index(new_node_id);
        let clip_chain_index = self.clip_scroll_tree.add_clip_node(
            node_index,
            self.id_to_index_mapper.get_node_index(parent_id),
            handle,
            new_node_id.pipeline_id(),
        );
        self.id_to_index_mapper.add_clip_chain(new_node_id, clip_chain_index);
        node_index
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
    ) -> ClipScrollNodeIndex {
        let node_index = self.id_to_index_mapper.get_node_index(new_node_id);
        let node = ClipScrollNode::new_scroll_frame(
            pipeline_id,
            self.id_to_index_mapper.get_node_index(parent_id),
            external_id,
            frame_rect,
            content_size,
            scroll_sensitivity,
        );

        self.clip_scroll_tree.add_node(node, node_index);
        self.id_to_index_mapper.map_to_parent_clip_chain(new_node_id, &parent_id);
        node_index
    }

    pub fn pop_reference_frame(&mut self) {
        self.reference_frame_stack.pop();
    }

    pub fn push_shadow(
        &mut self,
        shadow: Shadow,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
    ) {
        let pipeline_id = self.sc_stack.last().unwrap().pipeline_id;
        let current_reference_frame_index = self.current_reference_frame_index();
        let max_clip = LayoutRect::max_rect();

        // Quote from https://drafts.csswg.org/css-backgrounds-3/#shadow-blur
        // "the image that would be generated by applying to the shadow a
        // Gaussian blur with a standard deviation equal to half the blur radius."
        let std_deviation = shadow.blur_radius * 0.5;

        // If the shadow has no blur, any elements will get directly rendered
        // into the parent picture surface, instead of allocating and drawing
        // into an intermediate surface. In this case, we will need to apply
        // the local clip rect to primitives.
        let apply_local_clip_rect = shadow.blur_radius == 0.0;

        // Create a picture that the shadow primitives will be added to. If the
        // blur radius is 0, the code in Picture::prepare_for_render will
        // detect this and mark the picture to be drawn directly into the
        // parent picture, which avoids an intermediate surface and blur.
        let shadow_pic_index = self.prim_store.add_image_picture(
            Some(PictureCompositeMode::Filter(FilterOp::Blur(std_deviation))),
            false,
            pipeline_id,
            current_reference_frame_index,
            None,
            apply_local_clip_rect,
        );

        // Create the primitive to draw the shadow picture into the scene.
        let shadow_prim = BrushPrimitive::new_picture(shadow_pic_index);
        let shadow_prim_index = self.prim_store.add_primitive(
            &LayoutRect::zero(),
            &max_clip,
            info.is_backface_visible,
            None,
            None,
            PrimitiveContainer::Brush(shadow_prim),
        );

        // Add the shadow primitive. This must be done before pushing this
        // picture on to the shadow stack, to avoid infinite recursion!
        self.add_primitive_to_draw_list(shadow_prim_index, clip_and_scroll);
        self.shadow_stack.push((shadow, shadow_pic_index));
    }

    pub fn pop_all_shadows(&mut self) {
        assert!(self.shadow_stack.len() > 0, "popped shadows, but none were present");
        self.shadow_stack.clear();
    }

    pub fn add_solid_rectangle(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        color: ColorF,
        segments: Option<BrushSegmentDescriptor>,
        extra_clips: Vec<ClipSource>,
    ) {
        if color.a == 0.0 {
            // Don't add transparent rectangles to the draw list, but do consider them for hit
            // testing. This allows specifying invisible hit testing areas.
            self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
            return;
        }

        let prim = BrushPrimitive::new(
            BrushKind::new_solid(color),
            segments,
        );

        self.add_primitive(
            clip_and_scroll,
            info,
            extra_clips,
            PrimitiveContainer::Brush(prim),
        );
    }

    pub fn add_clear_rectangle(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
    ) {
        let prim = BrushPrimitive::new(
            BrushKind::Clear,
            None,
        );

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Brush(prim),
        );
    }

    pub fn add_scroll_bar(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        color: ColorF,
        scrollbar_info: ScrollbarInfo,
    ) {
        if color.a == 0.0 {
            return;
        }

        let prim = BrushPrimitive::new(
            BrushKind::new_solid(color),
            None,
        );

        let prim_index = self.create_primitive(
            info,
            Vec::new(),
            PrimitiveContainer::Brush(prim),
        );

        self.add_primitive_to_draw_list(
            prim_index,
            clip_and_scroll,
        );

        self.scrollbar_prims.push(ScrollbarPrimitive {
            prim_index,
            scroll_frame_index: scrollbar_info.0,
            frame_rect: scrollbar_info.1,
        });
    }

    pub fn add_line(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        wavy_line_thickness: f32,
        orientation: LineOrientation,
        line_color: &ColorF,
        style: LineStyle,
    ) {
        let prim = BrushPrimitive::new(
            BrushKind::new_solid(*line_color),
            None,
        );

        let extra_clips = match style {
            LineStyle::Solid => {
                Vec::new()
            }
            LineStyle::Wavy |
            LineStyle::Dotted |
            LineStyle::Dashed => {
                vec![
                    ClipSource::new_line_decoration(
                        info.rect,
                        style,
                        orientation,
                        wavy_line_thickness,
                    ),
                ]
            }
        };

        self.add_primitive(
            clip_and_scroll,
            info,
            extra_clips,
            PrimitiveContainer::Brush(prim),
        );
    }

    pub fn add_border(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        border_item: &BorderDisplayItem,
        gradient_stops: ItemRange<GradientStop>,
        gradient_stops_count: usize,
    ) {
        let rect = info.rect;
        let create_segments = |outset: SideOffsets2D<f32>| {
            // Calculate the modified rect as specific by border-image-outset
            let origin = LayoutPoint::new(rect.origin.x - outset.left, rect.origin.y - outset.top);
            let size = LayoutSize::new(
                rect.size.width + outset.left + outset.right,
                rect.size.height + outset.top + outset.bottom,
            );
            let rect = LayoutRect::new(origin, size);

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

            // Build the list of gradient segments
            vec![
                // Top left
                LayoutRect::from_floats(tl_outer.x, tl_outer.y, tl_inner.x, tl_inner.y),
                // Top right
                LayoutRect::from_floats(tr_inner.x, tr_outer.y, tr_outer.x, tr_inner.y),
                // Bottom right
                LayoutRect::from_floats(br_inner.x, br_inner.y, br_outer.x, br_outer.y),
                // Bottom left
                LayoutRect::from_floats(bl_outer.x, bl_inner.y, bl_inner.x, bl_outer.y),
                // Top
                LayoutRect::from_floats(tl_inner.x, tl_outer.y, tr_inner.x, tl_inner.y),
                // Bottom
                LayoutRect::from_floats(bl_inner.x, bl_inner.y, br_inner.x, bl_outer.y),
                // Left
                LayoutRect::from_floats(tl_outer.x, tl_inner.y, tl_inner.x, bl_inner.y),
                // Right
                LayoutRect::from_floats(tr_inner.x, tr_inner.y, br_outer.x, br_inner.y),
            ]
        };

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
                    segments: &mut Vec<BrushSegment>,
                    rect: LayoutRect,
                    uv_rect: TexelRect,
                    repeat_horizontal: RepeatMode,
                    repeat_vertical: RepeatMode
                ) {
                    if uv_rect.uv1.x > uv_rect.uv0.x &&
                       uv_rect.uv1.y > uv_rect.uv0.y {

                        // Use segment relative interpolation for all
                        // instances in this primitive.
                        let mut brush_flags = BrushFlags::SEGMENT_RELATIVE;

                        // Enable repeat modes on the segment.
                        if repeat_horizontal == RepeatMode::Repeat {
                            brush_flags |= BrushFlags::SEGMENT_REPEAT_X;
                        }
                        if repeat_vertical == RepeatMode::Repeat {
                            brush_flags |= BrushFlags::SEGMENT_REPEAT_Y;
                        }

                        let segment = BrushSegment::new(
                            rect.origin,
                            rect.size,
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
                let mut segments = vec![];

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
                    clip_mask_kind: BrushClipMaskKind::Unknown,
                };

                let prim = PrimitiveContainer::Brush(match border.source {
                    NinePatchBorderSource::Image(image_key) => {
                        BrushPrimitive::new(
                            BrushKind::Border {
                                request: ImageRequest {
                                    key: image_key,
                                    rendering: ImageRendering::Auto,
                                    tile: None,
                                },
                            },
                            Some(descriptor),
                        )
                    }
                });

                self.add_primitive(clip_and_scroll, info, Vec::new(), prim);
            }
            BorderDetails::Normal(ref border) => {
                self.add_normal_border(info, border, &border_item.widths, clip_and_scroll);
            }
            BorderDetails::Gradient(ref border) => for segment in create_segments(border.outset) {
                let segment_rel = segment.origin - rect.origin;
                let mut info = info.clone();
                info.rect = segment;

                self.add_gradient(
                    clip_and_scroll,
                    &info,
                    border.gradient.start_point - segment_rel,
                    border.gradient.end_point - segment_rel,
                    gradient_stops,
                    gradient_stops_count,
                    border.gradient.extend_mode,
                    segment.size,
                    LayoutSize::zero(),
                );
            },
            BorderDetails::RadialGradient(ref border) => {
                for segment in create_segments(border.outset) {
                    let segment_rel = segment.origin - rect.origin;
                    let mut info = info.clone();
                    info.rect = segment;

                    self.add_radial_gradient(
                        clip_and_scroll,
                        &info,
                        border.gradient.center - segment_rel,
                        border.gradient.start_offset * border.gradient.radius.width,
                        border.gradient.end_offset * border.gradient.radius.width,
                        border.gradient.radius.width / border.gradient.radius.height,
                        gradient_stops,
                        border.gradient.extend_mode,
                        segment.size,
                        LayoutSize::zero(),
                    );
                }
            }
        }
    }

    pub fn add_gradient(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        stops: ItemRange<GradientStop>,
        stops_count: usize,
        extend_mode: ExtendMode,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
    ) {
        let gradient_index = CachedGradientIndex(self.cached_gradients.len());
        self.cached_gradients.push(CachedGradient::new());

        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);
        let info = LayoutPrimitiveInfo {
            rect: prim_rect,
            .. *info
        };

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

        let prim = BrushPrimitive::new(
            BrushKind::LinearGradient {
                stops_range: stops,
                stops_count,
                extend_mode,
                reverse_stops,
                start_point: sp,
                end_point: ep,
                gradient_index,
                stretch_size,
                tile_spacing,
                visible_tiles: Vec::new(),
            },
            None,
        );

        let prim = PrimitiveContainer::Brush(prim);

        self.add_primitive(clip_and_scroll, &info, Vec::new(), prim);
    }

    pub fn add_radial_gradient(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        info: &LayoutPrimitiveInfo,
        center: LayoutPoint,
        start_radius: f32,
        end_radius: f32,
        ratio_xy: f32,
        stops: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        stretch_size: LayoutSize,
        mut tile_spacing: LayoutSize,
    ) {
        let gradient_index = CachedGradientIndex(self.cached_gradients.len());
        self.cached_gradients.push(CachedGradient::new());

        let mut prim_rect = info.rect;
        simplify_repeated_primitive(&stretch_size, &mut tile_spacing, &mut prim_rect);
        let info = LayoutPrimitiveInfo {
            rect: prim_rect,
            .. *info
        };

        let prim = BrushPrimitive::new(
            BrushKind::RadialGradient {
                stops_range: stops,
                extend_mode,
                center,
                start_radius,
                end_radius,
                ratio_xy,
                gradient_index,
                stretch_size,
                tile_spacing,
                visible_tiles: Vec::new(),
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

    pub fn add_text(
        &mut self,
        clip_and_scroll: ScrollNodeAndClipChain,
        run_offset: LayoutVector2D,
        prim_info: &LayoutPrimitiveInfo,
        font_instance_key: &FontInstanceKey,
        text_color: &ColorF,
        glyph_range: ItemRange<GlyphInstance>,
        glyph_options: Option<GlyphOptions>,
    ) {
        let prim = {
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

            // Sanity check - anything with glyphs bigger than this
            // is probably going to consume too much memory to render
            // efficiently anyway. This is specifically to work around
            // the font_advance.html reftest, which creates a very large
            // font as a crash test - the rendering is also ignored
            // by the azure renderer.
            if font_instance.size >= Au::from_px(4096) {
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

            let glyph_raster_space = match self.sc_stack.last() {
                Some(stacking_context) => stacking_context.glyph_raster_space,
                None => GlyphRasterSpace::Screen,
            };

            let prim_font = FontInstance::new(
                font_instance.font_key,
                font_instance.size,
                *text_color,
                font_instance.bg_color,
                render_mode,
                font_instance.subpx_dir,
                flags,
                font_instance.platform_options,
                font_instance.variations.clone(),
            );
            TextRunPrimitiveCpu {
                font: prim_font,
                glyph_range,
                glyph_gpu_blocks: Vec::new(),
                glyph_keys: Vec::new(),
                offset: run_offset,
                shadow: false,
                glyph_raster_space,
            }
        };

        self.add_primitive(
            clip_and_scroll,
            prim_info,
            Vec::new(),
            PrimitiveContainer::TextRun(prim),
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
                current_epoch: Epoch::invalid(),
                alpha_type,
                stretch_size,
                tile_spacing,
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
}

pub fn build_scene(config: &FrameBuilderConfig, request: SceneRequest) -> BuiltScene {

    let mut clip_scroll_tree = ClipScrollTree::new();
    let mut new_scene = Scene::new();

    let frame_builder = DisplayListFlattener::create_frame_builder(
        FrameBuilder::empty(), // WIP, we're not really recycling anything here, clean this up.
        &request.scene,
        &mut clip_scroll_tree,
        request.font_instances,
        &request.view,
        &request.output_pipelines,
        config,
        &mut new_scene
    );

    BuiltScene {
        scene: new_scene,
        frame_builder,
        clip_scroll_tree,
        removed_pipelines: request.removed_pipelines,
    }
}

/// Properties of a stacking context that are maintained
/// during creation of the scene. These structures are
/// not persisted after the initial scene build.
struct FlattenedStackingContext {
    /// Pipeline this stacking context belongs to.
    pipeline_id: PipelineId,

    /// Filters / mix-blend-mode effects
    composite_ops: CompositeOps,

    /// If true, visible when backface is visible.
    is_backface_visible: bool,

    /// The rasterization mode for any text runs that are part
    /// of this stacking context.
    glyph_raster_space: GlyphRasterSpace,

    /// CSS transform-style property.
    transform_style: TransformStyle,

    /// If Some(..), this stacking context establishes a new
    /// 3d rendering context, and the value is the picture
    // index of the 3d context container.
    rendering_context_3d_pic_index: Option<PictureIndex>,
}

#[derive(Debug)]
pub struct ScrollbarInfo(pub ClipScrollNodeIndex, pub LayoutRect);

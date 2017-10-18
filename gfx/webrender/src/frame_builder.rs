/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderDetails, BorderDisplayItem, BorderRadius, BoxShadowClipMode, BuiltDisplayList};
use api::{ComplexClipRegion, ClipAndScrollInfo, ClipId, ColorF};
use api::{DeviceIntPoint, DeviceIntRect, DeviceIntSize, DeviceUintRect, DeviceUintSize};
use api::{ExtendMode, FilterOp, FontInstance, FontRenderMode};
use api::{GlyphInstance, GlyphOptions, GradientStop, HitTestFlags, HitTestItem, HitTestResult};
use api::{ImageKey, ImageRendering, ItemRange, ItemTag, LayerPoint, LayerPrimitiveInfo, LayerRect};
use api::{LayerPixel, LayerSize, LayerToScrollTransform, LayerVector2D, LayoutVector2D, LineOrientation};
use api::{LineStyle, LocalClip, PipelineId, RepeatMode};
use api::{ScrollSensitivity, Shadow, TileOffset, TransformStyle};
use api::{WorldPixel, WorldPoint, YuvColorSpace, YuvData, device_length};
use app_units::Au;
use border::ImageBorderSegment;
use clip::{ClipMode, ClipRegion, ClipSource, ClipSources, ClipStore, Contains};
use clip_scroll_node::{ClipInfo, ClipScrollNode, NodeType};
use clip_scroll_tree::{ClipScrollTree, CoordinateSystemId};
use euclid::{SideOffsets2D, TypedTransform3D, vec2, vec3};
use frame::FrameId;
use gpu_cache::GpuCache;
use internal_types::{FastHashMap, FastHashSet, HardwareCompositeOp};
use picture::{PicturePrimitive};
use plane_split::{BspSplitter, Polygon, Splitter};
use prim_store::{BrushPrimitive, TexelRect, YuvImagePrimitiveCpu};
use prim_store::{GradientPrimitiveCpu, ImagePrimitiveCpu, LinePrimitive, PrimitiveKind};
use prim_store::{PrimitiveContainer, PrimitiveIndex};
use prim_store::{PrimitiveStore, RadialGradientPrimitiveCpu};
use prim_store::{RectanglePrimitive, TextRunPrimitiveCpu};
use profiler::{FrameProfileCounters, GpuCacheProfileCounters, TextureCacheProfileCounters};
use render_task::{AlphaRenderItem, ClipChain, RenderTask, RenderTaskId, RenderTaskLocation};
use render_task::RenderTaskTree;
use resource_cache::ResourceCache;
use scene::ScenePipeline;
use std::{mem, usize, f32, i32};
use tiling::{ClipScrollGroup, ClipScrollGroupIndex, CompositeOps, Frame};
use tiling::{ContextIsolation, RenderTargetKind, StackingContextIndex};
use tiling::{PackedLayer, PackedLayerIndex, PrimitiveFlags, PrimitiveRunCmd, RenderPass};
use tiling::{RenderTargetContext, ScrollbarPrimitive, StackingContext};
use util::{self, pack_as_float, RectHelpers, recycle_vec};

/// Construct a polygon from stacking context boundaries.
/// `anchor` here is an index that's going to be preserved in all the
/// splits of the polygon.
fn make_polygon(
    stacking_context: &StackingContext,
    node: &ClipScrollNode,
    anchor: usize,
) -> Polygon<f64, WorldPixel> {
    //TODO: only work with `isolated_items_bounds.size` worth of space
    // This can be achieved by moving the `origin` shift
    // from the primitive local coordinates into the layer transformation.
    // Which in turn needs it to be a render task property obeyed by all primitives
    // upon rendering, possibly not limited to `write_*_vertex` implementations.
    let size = stacking_context.isolated_items_bounds.bottom_right();
    let bounds = LayerRect::new(LayerPoint::zero(), LayerSize::new(size.x, size.y));
    let mat = TypedTransform3D::row_major(
        node.world_content_transform.m11 as f64,
        node.world_content_transform.m12 as f64,
        node.world_content_transform.m13 as f64,
        node.world_content_transform.m14 as f64,
        node.world_content_transform.m21 as f64,
        node.world_content_transform.m22 as f64,
        node.world_content_transform.m23 as f64,
        node.world_content_transform.m24 as f64,
        node.world_content_transform.m31 as f64,
        node.world_content_transform.m32 as f64,
        node.world_content_transform.m33 as f64,
        node.world_content_transform.m34 as f64,
        node.world_content_transform.m41 as f64,
        node.world_content_transform.m42 as f64,
        node.world_content_transform.m43 as f64,
        node.world_content_transform.m44 as f64);
    Polygon::from_transformed_rect(bounds.cast().unwrap(), mat, anchor)
}

#[derive(Clone, Copy)]
pub struct FrameBuilderConfig {
    pub enable_scrollbars: bool,
    pub default_font_render_mode: FontRenderMode,
    pub debug: bool,
}

#[derive(Debug)]
pub struct HitTestingItem {
    rect: LayerRect,
    clip: LocalClip,
    tag: ItemTag,
}

impl HitTestingItem {
    fn new(tag: ItemTag, info: &LayerPrimitiveInfo) -> HitTestingItem {
        HitTestingItem {
            rect: info.rect,
            clip: info.local_clip,
            tag: tag,
        }
    }
}

pub struct HitTestingRun(Vec<HitTestingItem>, ClipAndScrollInfo);

pub struct FrameBuilder {
    screen_size: DeviceUintSize,
    background_color: Option<ColorF>,
    prim_store: PrimitiveStore,
    pub clip_store: ClipStore,
    cmds: Vec<PrimitiveRunCmd>,
    hit_testing_runs: Vec<HitTestingRun>,
    config: FrameBuilderConfig,

    stacking_context_store: Vec<StackingContext>,
    clip_scroll_group_store: Vec<ClipScrollGroup>,
    // Note: value here is meant to be `ClipScrollGroupIndex`,
    // but we already have `ClipAndScrollInfo` in the key
    clip_scroll_group_indices: FastHashMap<ClipAndScrollInfo, usize>,
    packed_layers: Vec<PackedLayer>,

    // A stack of the current shadow primitives.
    // The sub-Vec stores a buffer of fast-path primitives to be appended on pop.
    shadow_prim_stack: Vec<(PrimitiveIndex, Vec<(PrimitiveIndex, ClipAndScrollInfo)>)>,
    // If we're doing any fast-path shadows, we buffer the "real"
    // content here, to be appended when the shadow stack is empty.
    pending_shadow_contents: Vec<(PrimitiveIndex, ClipAndScrollInfo, LayerPrimitiveInfo)>,

    scrollbar_prims: Vec<ScrollbarPrimitive>,

    /// A stack of scroll nodes used during display list processing to properly
    /// parent new scroll nodes.
    reference_frame_stack: Vec<ClipId>,

    /// A stack of stacking contexts used for creating ClipScrollGroups as
    /// primitives are added to the frame.
    stacking_context_stack: Vec<StackingContextIndex>,

    /// Whether or not we've pushed a root stacking context for the current pipeline.
    has_root_stacking_context: bool,
}

pub struct PrimitiveContext<'a> {
    pub packed_layer_index: PackedLayerIndex,
    pub packed_layer: &'a PackedLayer,
    pub device_pixel_ratio: f32,
    pub clip_chain: ClipChain,
    pub clip_bounds: DeviceIntRect,
    pub clip_id: ClipId,
    pub coordinate_system_id: CoordinateSystemId,
    pub display_list: &'a BuiltDisplayList,
}

impl<'a> PrimitiveContext<'a> {
    fn new(
        packed_layer_index: PackedLayerIndex,
        packed_layer: &'a PackedLayer,
        clip_id: ClipId,
        clip_chain: ClipChain,
        clip_bounds: DeviceIntRect,
        coordinate_system_id: CoordinateSystemId,
        device_pixel_ratio: f32,
        display_list: &'a BuiltDisplayList,
    ) -> Self {
        PrimitiveContext {
            packed_layer_index,
            packed_layer,
            clip_chain,
            clip_bounds,
            coordinate_system_id,
            device_pixel_ratio,
            clip_id,
            display_list,
        }
    }
}

impl FrameBuilder {
    pub fn new(
        previous: Option<FrameBuilder>,
        screen_size: DeviceUintSize,
        background_color: Option<ColorF>,
        config: FrameBuilderConfig,
    ) -> FrameBuilder {
        match previous {
            Some(prev) => FrameBuilder {
                stacking_context_store: recycle_vec(prev.stacking_context_store),
                clip_scroll_group_store: recycle_vec(prev.clip_scroll_group_store),
                clip_scroll_group_indices: FastHashMap::default(),
                cmds: recycle_vec(prev.cmds),
                hit_testing_runs: recycle_vec(prev.hit_testing_runs),
                packed_layers: recycle_vec(prev.packed_layers),
                shadow_prim_stack: recycle_vec(prev.shadow_prim_stack),
                pending_shadow_contents: recycle_vec(prev.pending_shadow_contents),
                scrollbar_prims: recycle_vec(prev.scrollbar_prims),
                reference_frame_stack: recycle_vec(prev.reference_frame_stack),
                stacking_context_stack: recycle_vec(prev.stacking_context_stack),
                prim_store: prev.prim_store.recycle(),
                clip_store: prev.clip_store.recycle(),
                screen_size,
                background_color,
                config,
                has_root_stacking_context: false,
            },
            None => FrameBuilder {
                stacking_context_store: Vec::new(),
                clip_scroll_group_store: Vec::new(),
                clip_scroll_group_indices: FastHashMap::default(),
                cmds: Vec::new(),
                hit_testing_runs: Vec::new(),
                packed_layers: Vec::new(),
                shadow_prim_stack: Vec::new(),
                pending_shadow_contents: Vec::new(),
                scrollbar_prims: Vec::new(),
                reference_frame_stack: Vec::new(),
                stacking_context_stack: Vec::new(),
                prim_store: PrimitiveStore::new(),
                clip_store: ClipStore::new(),
                screen_size,
                background_color,
                config,
                has_root_stacking_context: false,
            },
        }
    }

    /// Create a primitive and add it to the prim store. This method doesn't
    /// add the primitive to the draw list, so can be used for creating
    /// sub-primitives.
    fn create_primitive(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        mut clip_sources: Vec<ClipSource>,
        container: PrimitiveContainer,
    ) -> PrimitiveIndex {
        if !self.clip_scroll_group_indices.contains_key(&clip_and_scroll) {
            let group_id = self.create_clip_scroll_group(&clip_and_scroll);
            self.clip_scroll_group_indices.insert(clip_and_scroll, group_id);
        }

        if let &LocalClip::RoundedRect(main, region) = &info.local_clip {
            clip_sources.push(ClipSource::Rectangle(main));
            clip_sources.push(ClipSource::RoundedRectangle(
                region.rect,
                region.radii,
                ClipMode::Clip,
            ));
        }

        let clip_sources = self.clip_store.insert(ClipSources::new(clip_sources));
        let prim_index = self.prim_store.add_primitive(
            &info.rect,
            &info.local_clip.clip_rect(),
            info.is_backface_visible,
            clip_sources,
            info.tag,
            container,
        );

        prim_index
    }

    pub fn add_primitive_to_hit_testing_list(
        &mut self,
        info: &LayerPrimitiveInfo,
        clip_and_scroll: ClipAndScrollInfo
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
        clip_and_scroll: ClipAndScrollInfo,
    ) {
        match self.cmds.last_mut().unwrap() {
            &mut PrimitiveRunCmd::PrimitiveRun(
                run_prim_index,
                ref mut count,
                run_clip_and_scroll,
            ) => if run_clip_and_scroll == clip_and_scroll &&
                run_prim_index.0 + *count == prim_index.0
            {
                *count += 1;
                return;
            },
            &mut PrimitiveRunCmd::PushStackingContext(..) |
            &mut PrimitiveRunCmd::PopStackingContext => {}
        }

        self.cmds.push(PrimitiveRunCmd::PrimitiveRun(
            prim_index,
            1,
            clip_and_scroll,
        ));
    }

    /// Convenience interface that creates a primitive entry and adds it
    /// to the draw list.
    pub fn add_primitive(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        clip_sources: Vec<ClipSource>,
        container: PrimitiveContainer,
    ) -> PrimitiveIndex {
        self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
        let prim_index = self.create_primitive(clip_and_scroll, info, clip_sources, container);

        self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
        prim_index
    }

    fn create_clip_scroll_group(&mut self, info: &ClipAndScrollInfo) -> usize {
        let packed_layer_index = PackedLayerIndex(self.packed_layers.len());
        self.packed_layers.push(PackedLayer::empty());

        let group_id = self.clip_scroll_group_store.len();
        self.clip_scroll_group_store.push(ClipScrollGroup {
            scroll_node_id: info.scroll_node_id,
            clip_node_id: info.clip_node_id(),
            packed_layer_index,
            screen_bounding_rect: None,
            coordinate_system_id: CoordinateSystemId(0),
        });

        group_id
    }

    pub fn notify_waiting_for_root_stacking_context(&mut self) {
        self.has_root_stacking_context = false;
    }

    pub fn push_stacking_context(
        &mut self,
        reference_frame_offset: &LayerVector2D,
        pipeline_id: PipelineId,
        composite_ops: CompositeOps,
        transform_style: TransformStyle,
        is_backface_visible: bool,
        is_pipeline_root: bool,
    ) {
        if let Some(parent_index) = self.stacking_context_stack.last() {
            let parent_is_root = self.stacking_context_store[parent_index.0].is_page_root;

            if composite_ops.mix_blend_mode.is_some() && !parent_is_root {
                // the parent stacking context of a stacking context with mix-blend-mode
                // must be drawn with a transparent background, unless the parent stacking context
                // is the root of the page
                let isolation = &mut self.stacking_context_store[parent_index.0].isolation;
                if *isolation != ContextIsolation::None {
                    error!(
                        "Isolation conflict detected on {:?}: {:?}",
                        parent_index,
                        *isolation
                    );
                }
                *isolation = ContextIsolation::Full;
            }
        }

        let stacking_context_index = StackingContextIndex(self.stacking_context_store.len());
        let reference_frame_id = self.current_reference_frame_id();
        self.stacking_context_store.push(StackingContext::new(
            pipeline_id,
            *reference_frame_offset,
            !self.has_root_stacking_context,
            is_pipeline_root,
            reference_frame_id,
            transform_style,
            composite_ops,
            is_backface_visible,
        ));
        self.has_root_stacking_context = true;
        self.cmds
            .push(PrimitiveRunCmd::PushStackingContext(stacking_context_index));
        self.stacking_context_stack.push(stacking_context_index);
    }

    pub fn pop_stacking_context(&mut self) {
        self.cmds.push(PrimitiveRunCmd::PopStackingContext);
        self.stacking_context_stack.pop();
        assert!(
            self.shadow_prim_stack.is_empty(),
            "Found unpopped text shadows when popping stacking context!"
        );
    }

    pub fn push_reference_frame(
        &mut self,
        parent_id: Option<ClipId>,
        pipeline_id: PipelineId,
        rect: &LayerRect,
        transform: &LayerToScrollTransform,
        origin_in_parent_reference_frame: LayerVector2D,
        root_for_pipeline: bool,
        clip_scroll_tree: &mut ClipScrollTree,
    ) -> ClipId {
        let new_id = clip_scroll_tree.add_reference_frame(
            rect,
            transform,
            origin_in_parent_reference_frame,
            pipeline_id,
            parent_id,
            root_for_pipeline,
        );
        self.reference_frame_stack.push(new_id);
        new_id
    }

    pub fn current_reference_frame_id(&self) -> ClipId {
        *self.reference_frame_stack.last().unwrap()
    }

    pub fn setup_viewport_offset(
        &mut self,
        window_size: DeviceUintSize,
        inner_rect: DeviceUintRect,
        device_pixel_ratio: f32,
        clip_scroll_tree: &mut ClipScrollTree,
    ) {
        let inner_origin = inner_rect.origin.to_f32();
        let viewport_offset = LayerPoint::new(
            (inner_origin.x / device_pixel_ratio).round(),
            (inner_origin.y / device_pixel_ratio).round(),
        );
        let outer_size = window_size.to_f32();
        let outer_size = LayerSize::new(
            (outer_size.width / device_pixel_ratio).round(),
            (outer_size.height / device_pixel_ratio).round(),
        );
        let clip_size = LayerSize::new(
            outer_size.width + 2.0 * viewport_offset.x,
            outer_size.height + 2.0 * viewport_offset.y,
        );

        let viewport_clip = LayerRect::new(
            LayerPoint::new(-viewport_offset.x, -viewport_offset.y),
            LayerSize::new(clip_size.width, clip_size.height),
        );

        let root_id = clip_scroll_tree.root_reference_frame_id();
        if let Some(root_node) = clip_scroll_tree.nodes.get_mut(&root_id) {
            if let NodeType::ReferenceFrame(ref mut info) = root_node.node_type {
                info.transform = LayerToScrollTransform::create_translation(
                    viewport_offset.x,
                    viewport_offset.y,
                    0.0,
                );
            }
            root_node.local_clip_rect = viewport_clip;
        }

        let clip_id = clip_scroll_tree.topmost_scrolling_node_id();
        if let Some(root_node) = clip_scroll_tree.nodes.get_mut(&clip_id) {
            root_node.local_clip_rect = viewport_clip;
        }
    }

    pub fn push_root(
        &mut self,
        pipeline_id: PipelineId,
        viewport_size: &LayerSize,
        content_size: &LayerSize,
        clip_scroll_tree: &mut ClipScrollTree,
    ) -> ClipId {
        let viewport_rect = LayerRect::new(LayerPoint::zero(), *viewport_size);
        let identity = &LayerToScrollTransform::identity();
        self.push_reference_frame(
            None,
            pipeline_id,
            &viewport_rect,
            identity,
            LayerVector2D::zero(),
            true,
            clip_scroll_tree,
        );

        let topmost_scrolling_node_id = ClipId::root_scroll_node(pipeline_id);
        clip_scroll_tree.topmost_scrolling_node_id = topmost_scrolling_node_id;

        self.add_scroll_frame(
            topmost_scrolling_node_id,
            clip_scroll_tree.root_reference_frame_id,
            pipeline_id,
            &viewport_rect,
            content_size,
            ScrollSensitivity::ScriptAndInputEvents,
            clip_scroll_tree,
        );

        topmost_scrolling_node_id
    }

    pub fn add_clip_node(
        &mut self,
        new_node_id: ClipId,
        parent_id: ClipId,
        pipeline_id: PipelineId,
        clip_region: ClipRegion,
        clip_scroll_tree: &mut ClipScrollTree,
    ) {
        let clip_rect = LayerRect::new(clip_region.origin, clip_region.main.size);
        let clip_info = ClipInfo::new(
            clip_region,
            PackedLayerIndex(self.packed_layers.len()),
            &mut self.clip_store,
        );
        let node = ClipScrollNode::new_clip_node(pipeline_id, parent_id, clip_info, clip_rect);
        clip_scroll_tree.add_node(node, new_node_id);
        self.packed_layers.push(PackedLayer::empty());
    }

    pub fn add_scroll_frame(
        &mut self,
        new_node_id: ClipId,
        parent_id: ClipId,
        pipeline_id: PipelineId,
        frame_rect: &LayerRect,
        content_size: &LayerSize,
        scroll_sensitivity: ScrollSensitivity,
        clip_scroll_tree: &mut ClipScrollTree,
    ) {
        let node = ClipScrollNode::new_scroll_frame(
            pipeline_id,
            parent_id,
            frame_rect,
            content_size,
            scroll_sensitivity,
        );

        clip_scroll_tree.add_node(node, new_node_id);
    }

    pub fn pop_reference_frame(&mut self) {
        self.reference_frame_stack.pop();
    }

    pub fn push_shadow(
        &mut self,
        shadow: Shadow,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
    ) {
        let prim = PicturePrimitive::new_shadow(shadow, RenderTargetKind::Color);

        // Create an empty shadow primitive. Insert it into
        // the draw lists immediately so that it will be drawn
        // before any visual text elements that are added as
        // part of this shadow context.
        let prim_index = self.create_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Picture(prim),
        );

        let pending = vec![(prim_index, clip_and_scroll)];
        self.shadow_prim_stack.push((prim_index, pending));
    }

    pub fn pop_all_shadows(&mut self) {
        assert!(self.shadow_prim_stack.len() > 0, "popped shadows, but none were present");

        // Borrowcheck dance
        let mut shadows = mem::replace(&mut self.shadow_prim_stack, Vec::new());
        for (prim_index, pending_primitives) in shadows.drain(..) {
            {
                // By now, the local rect of the text shadow has been calculated. It
                // is calculated as the items in the shadow are added. It's now
                // safe to offset the local rect by the offset of the shadow, which
                // is then used when blitting the shadow to the final location.
                let metadata = &mut self.prim_store.cpu_metadata[prim_index.0];
                let prim = &self.prim_store.cpu_pictures[metadata.cpu_prim_index.0];
                let shadow = prim.as_shadow();

                metadata.local_rect = metadata.local_rect.translate(&shadow.offset);
            }

            // Push any fast-path shadows now
            for (prim_index, clip_and_scroll) in pending_primitives {
                self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
            }
        }

        let mut pending_primitives = mem::replace(&mut self.pending_shadow_contents, Vec::new());
        for (prim_index, clip_and_scroll, info) in pending_primitives.drain(..) {
            self.add_primitive_to_hit_testing_list(&info, clip_and_scroll);
            self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
        }

        mem::replace(&mut self.pending_shadow_contents, pending_primitives);
        mem::replace(&mut self.shadow_prim_stack, shadows);
    }

    pub fn add_solid_rectangle(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        color: &ColorF,
        flags: PrimitiveFlags,
    ) {
        let prim = RectanglePrimitive { color: *color };

        // Don't add transparent rectangles to the draw list, but do consider them for hit
        // testing. This allows specifying invisible hit testing areas.
        if color.a == 0.0 {
            self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
            return;
        }

        let prim_index = self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Rectangle(prim),
        );

        match flags {
            PrimitiveFlags::None => {}
            PrimitiveFlags::Scrollbar(clip_id, border_radius) => {
                self.scrollbar_prims.push(ScrollbarPrimitive {
                    prim_index,
                    clip_id,
                    border_radius,
                });
            }
        }
    }

    pub fn add_line(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        baseline: f32,
        start: f32,
        end: f32,
        orientation: LineOrientation,
        width: f32,
        color: &ColorF,
        style: LineStyle,
    ) {
        let new_rect = match orientation {
            LineOrientation::Horizontal => LayerRect::new(
                LayerPoint::new(start, baseline),
                LayerSize::new(end - start, width),
            ),
            LineOrientation::Vertical => LayerRect::new(
                LayerPoint::new(baseline, start),
                LayerSize::new(width, end - start),
            ),
        };

        let line = LinePrimitive {
            color: *color,
            style: style,
            orientation: orientation,
        };

        let mut fast_shadow_prims = Vec::new();
        for (idx, &(shadow_prim_index, _)) in self.shadow_prim_stack.iter().enumerate() {
            let shadow_metadata = &self.prim_store.cpu_metadata[shadow_prim_index.0];
            let picture = &self.prim_store.cpu_pictures[shadow_metadata.cpu_prim_index.0];
            let shadow = picture.as_shadow();
            if shadow.blur_radius == 0.0 {
                fast_shadow_prims.push((idx, shadow.clone()));
            }
        }

        for (idx, shadow) in fast_shadow_prims {
            let mut line = line.clone();
            line.color = shadow.color;
            let mut info = info.clone();
            info.rect = new_rect.translate(&shadow.offset);
            let prim_index = self.create_primitive(
                clip_and_scroll,
                &info,
                Vec::new(),
                PrimitiveContainer::Line(line),
            );
            self.shadow_prim_stack[idx].1.push((prim_index, clip_and_scroll));
        }

        let mut info = info.clone();
        info.rect = new_rect;
        let prim_index = self.create_primitive(
            clip_and_scroll,
            &info,
            Vec::new(),
            PrimitiveContainer::Line(line),
        );

        if color.a > 0.0 {
            if self.shadow_prim_stack.is_empty() {
                self.add_primitive_to_hit_testing_list(&info, clip_and_scroll);
                self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
            } else {
                self.pending_shadow_contents.push((prim_index, clip_and_scroll, info));
            }
        }

        for &(shadow_prim_index, _) in &self.shadow_prim_stack {
            let shadow_metadata = &mut self.prim_store.cpu_metadata[shadow_prim_index.0];
            debug_assert_eq!(shadow_metadata.prim_kind, PrimitiveKind::Picture);
            let picture =
                &mut self.prim_store.cpu_pictures[shadow_metadata.cpu_prim_index.0];
            let blur_radius = picture.as_shadow().blur_radius;

            // Only run real blurs here (fast path zero blurs are handled above).
            if blur_radius > 0.0 {
                let shadow_rect = new_rect.inflate(
                    blur_radius,
                    blur_radius,
                );
                shadow_metadata.local_rect = shadow_metadata.local_rect.union(&shadow_rect);
                picture.add_primitive(prim_index, clip_and_scroll);
            }
        }
    }

    pub fn add_border(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        border_item: &BorderDisplayItem,
        gradient_stops: ItemRange<GradientStop>,
        gradient_stops_count: usize,
    ) {
        let rect = info.rect;
        let create_segments = |outset: SideOffsets2D<f32>| {
            // Calculate the modified rect as specific by border-image-outset
            let origin = LayerPoint::new(rect.origin.x - outset.left, rect.origin.y - outset.top);
            let size = LayerSize::new(
                rect.size.width + outset.left + outset.right,
                rect.size.height + outset.top + outset.bottom,
            );
            let rect = LayerRect::new(origin, size);

            let tl_outer = LayerPoint::new(rect.origin.x, rect.origin.y);
            let tl_inner = tl_outer + vec2(border_item.widths.left, border_item.widths.top);

            let tr_outer = LayerPoint::new(rect.origin.x + rect.size.width, rect.origin.y);
            let tr_inner = tr_outer + vec2(-border_item.widths.right, border_item.widths.top);

            let bl_outer = LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height);
            let bl_inner = bl_outer + vec2(border_item.widths.left, -border_item.widths.bottom);

            let br_outer = LayerPoint::new(
                rect.origin.x + rect.size.width,
                rect.origin.y + rect.size.height,
            );
            let br_inner = br_outer - vec2(border_item.widths.right, border_item.widths.bottom);

            // Build the list of gradient segments
            vec![
                // Top left
                LayerRect::from_floats(tl_outer.x, tl_outer.y, tl_inner.x, tl_inner.y),
                // Top right
                LayerRect::from_floats(tr_inner.x, tr_outer.y, tr_outer.x, tr_inner.y),
                // Bottom right
                LayerRect::from_floats(br_inner.x, br_inner.y, br_outer.x, br_outer.y),
                // Bottom left
                LayerRect::from_floats(bl_outer.x, bl_inner.y, bl_inner.x, bl_outer.y),
                // Top
                LayerRect::from_floats(tl_inner.x, tl_outer.y, tr_inner.x, tl_inner.y),
                // Bottom
                LayerRect::from_floats(bl_inner.x, bl_inner.y, br_inner.x, bl_outer.y),
                // Left
                LayerRect::from_floats(tl_outer.x, tl_inner.y, tl_inner.x, bl_inner.y),
                // Right
                LayerRect::from_floats(tr_inner.x, tr_inner.y, br_outer.x, br_inner.y),
            ]
        };

        match border_item.details {
            BorderDetails::Image(ref border) => {
                // Calculate the modified rect as specific by border-image-outset
                let origin = LayerPoint::new(
                    rect.origin.x - border.outset.left,
                    rect.origin.y - border.outset.top,
                );
                let size = LayerSize::new(
                    rect.size.width + border.outset.left + border.outset.right,
                    rect.size.height + border.outset.top + border.outset.bottom,
                );
                let rect = LayerRect::new(origin, size);

                // Calculate the local texel coords of the slices.
                let px0 = 0.0;
                let px1 = border.patch.slice.left as f32;
                let px2 = border.patch.width as f32 - border.patch.slice.right as f32;
                let px3 = border.patch.width as f32;

                let py0 = 0.0;
                let py1 = border.patch.slice.top as f32;
                let py2 = border.patch.height as f32 - border.patch.slice.bottom as f32;
                let py3 = border.patch.height as f32;

                let tl_outer = LayerPoint::new(rect.origin.x, rect.origin.y);
                let tl_inner = tl_outer + vec2(border_item.widths.left, border_item.widths.top);

                let tr_outer = LayerPoint::new(rect.origin.x + rect.size.width, rect.origin.y);
                let tr_inner = tr_outer + vec2(-border_item.widths.right, border_item.widths.top);

                let bl_outer = LayerPoint::new(rect.origin.x, rect.origin.y + rect.size.height);
                let bl_inner = bl_outer + vec2(border_item.widths.left, -border_item.widths.bottom);

                let br_outer = LayerPoint::new(
                    rect.origin.x + rect.size.width,
                    rect.origin.y + rect.size.height,
                );
                let br_inner = br_outer - vec2(border_item.widths.right, border_item.widths.bottom);

                fn add_segment(
                    segments: &mut Vec<ImageBorderSegment>,
                    rect: LayerRect,
                    uv_rect: TexelRect,
                    repeat_horizontal: RepeatMode,
                    repeat_vertical: RepeatMode) {
                    if uv_rect.uv1.x > uv_rect.uv0.x &&
                       uv_rect.uv1.y > uv_rect.uv0.y {
                        segments.push(ImageBorderSegment::new(
                            rect,
                            uv_rect,
                            repeat_horizontal,
                            repeat_vertical,
                        ));
                    }
                }

                // Build the list of image segments
                let mut segments = vec![];

                // Top left
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(tl_outer.x, tl_outer.y, tl_inner.x, tl_inner.y),
                    TexelRect::new(px0, py0, px1, py1),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );
                // Top right
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(tr_inner.x, tr_outer.y, tr_outer.x, tr_inner.y),
                    TexelRect::new(px2, py0, px3, py1),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );
                // Bottom right
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(br_inner.x, br_inner.y, br_outer.x, br_outer.y),
                    TexelRect::new(px2, py2, px3, py3),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );
                // Bottom left
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(bl_outer.x, bl_inner.y, bl_inner.x, bl_outer.y),
                    TexelRect::new(px0, py2, px1, py3),
                    RepeatMode::Stretch,
                    RepeatMode::Stretch
                );

                // Center
                if border.fill {
                    add_segment(
                        &mut segments,
                        LayerRect::from_floats(tl_inner.x, tl_inner.y, tr_inner.x, bl_inner.y),
                        TexelRect::new(px1, py1, px2, py2),
                        border.repeat_horizontal,
                        border.repeat_vertical
                    );
                }

                // Add edge segments.

                // Top
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(tl_inner.x, tl_outer.y, tr_inner.x, tl_inner.y),
                    TexelRect::new(px1, py0, px2, py1),
                    border.repeat_horizontal,
                    RepeatMode::Stretch,
                );
                // Bottom
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(bl_inner.x, bl_inner.y, br_inner.x, bl_outer.y),
                    TexelRect::new(px1, py2, px2, py3),
                    border.repeat_horizontal,
                    RepeatMode::Stretch,
                );
                // Left
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(tl_outer.x, tl_inner.y, tl_inner.x, bl_inner.y),
                    TexelRect::new(px0, py1, px1, py2),
                    RepeatMode::Stretch,
                    border.repeat_vertical,
                );
                // Right
                add_segment(
                    &mut segments,
                    LayerRect::from_floats(tr_inner.x, tr_inner.y, br_outer.x, br_inner.y),
                    TexelRect::new(px2, py1, px3, py2),
                    RepeatMode::Stretch,
                    border.repeat_vertical,
                );

                for segment in segments {
                    let mut info = info.clone();
                    info.rect = segment.geom_rect;
                    self.add_image(
                        clip_and_scroll,
                        &info,
                        &segment.stretch_size,
                        &segment.tile_spacing,
                        Some(segment.sub_rect),
                        border.image_key,
                        ImageRendering::Auto,
                        None,
                    );
                }
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
                    LayerSize::zero(),
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
                        border.gradient.start_center - segment_rel,
                        border.gradient.start_radius,
                        border.gradient.end_center - segment_rel,
                        border.gradient.end_radius,
                        border.gradient.ratio_xy,
                        gradient_stops,
                        border.gradient.extend_mode,
                        segment.size,
                        LayerSize::zero(),
                    );
                }
            }
        }
    }

    pub fn add_gradient(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        start_point: LayerPoint,
        end_point: LayerPoint,
        stops: ItemRange<GradientStop>,
        stops_count: usize,
        extend_mode: ExtendMode,
        tile_size: LayerSize,
        tile_spacing: LayerSize,
    ) {
        let tile_repeat = tile_size + tile_spacing;
        let is_not_tiled = tile_repeat.width >= info.rect.size.width &&
            tile_repeat.height >= info.rect.size.height;

        let aligned_and_fills_rect = (start_point.x == end_point.x &&
            start_point.y.min(end_point.y) <= 0.0 &&
            start_point.y.max(end_point.y) >= info.rect.size.height) ||
            (start_point.y == end_point.y && start_point.x.min(end_point.x) <= 0.0 &&
                start_point.x.max(end_point.x) >= info.rect.size.width);

        // Fast path for clamped, axis-aligned gradients, with gradient lines intersecting all of rect:
        let aligned = extend_mode == ExtendMode::Clamp && is_not_tiled && aligned_and_fills_rect;

        // Try to ensure that if the gradient is specified in reverse, then so long as the stops
        // are also supplied in reverse that the rendered result will be equivalent. To do this,
        // a reference orientation for the gradient line must be chosen, somewhat arbitrarily, so
        // just designate the reference orientation as start < end. Aligned gradient rendering
        // manages to produce the same result regardless of orientation, so don't worry about
        // reversing in that case.
        let reverse_stops = !aligned &&
            (start_point.x > end_point.x ||
                (start_point.x == end_point.x && start_point.y > end_point.y));

        // To get reftests exactly matching with reverse start/end
        // points, it's necessary to reverse the gradient
        // line in some cases.
        let (sp, ep) = if reverse_stops {
            (end_point, start_point)
        } else {
            (start_point, end_point)
        };

        let gradient_cpu = GradientPrimitiveCpu {
            stops_range: stops,
            stops_count,
            extend_mode,
            reverse_stops,
            gpu_blocks: [
                [sp.x, sp.y, ep.x, ep.y].into(),
                [
                    tile_size.width,
                    tile_size.height,
                    tile_repeat.width,
                    tile_repeat.height,
                ].into(),
                [pack_as_float(extend_mode as u32), 0.0, 0.0, 0.0].into(),
            ],
        };

        let prim = if aligned {
            PrimitiveContainer::AlignedGradient(gradient_cpu)
        } else {
            PrimitiveContainer::AngleGradient(gradient_cpu)
        };

        self.add_primitive(clip_and_scroll, info, Vec::new(), prim);
    }

    pub fn add_radial_gradient(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        start_center: LayerPoint,
        start_radius: f32,
        end_center: LayerPoint,
        end_radius: f32,
        ratio_xy: f32,
        stops: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        tile_size: LayerSize,
        tile_spacing: LayerSize,
    ) {
        let tile_repeat = tile_size + tile_spacing;

        let radial_gradient_cpu = RadialGradientPrimitiveCpu {
            stops_range: stops,
            extend_mode,
            gpu_data_count: 0,
            gpu_blocks: [
                [start_center.x, start_center.y, end_center.x, end_center.y].into(),
                [
                    start_radius,
                    end_radius,
                    ratio_xy,
                    pack_as_float(extend_mode as u32),
                ].into(),
                [
                    tile_size.width,
                    tile_size.height,
                    tile_repeat.width,
                    tile_repeat.height,
                ].into(),
            ],
        };

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::RadialGradient(radial_gradient_cpu),
        );
    }

    pub fn add_text(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        run_offset: LayoutVector2D,
        info: &LayerPrimitiveInfo,
        font: &FontInstance,
        color: &ColorF,
        glyph_range: ItemRange<GlyphInstance>,
        glyph_count: usize,
        glyph_options: Option<GlyphOptions>,
    ) {
        let rect = info.rect;
        // Trivial early out checks
        if font.size.0 <= 0 {
            return;
        }

        // Sanity check - anything with glyphs bigger than this
        // is probably going to consume too much memory to render
        // efficiently anyway. This is specifically to work around
        // the font_advance.html reftest, which creates a very large
        // font as a crash test - the rendering is also ignored
        // by the azure renderer.
        if font.size >= Au::from_px(4096) {
            return;
        }

        // TODO(gw): Use a proper algorithm to select
        // whether this item should be rendered with
        // subpixel AA!
        let mut render_mode = self.config
            .default_font_render_mode
            .limit_by(font.render_mode);
        if let Some(options) = glyph_options {
            render_mode = render_mode.limit_by(options.render_mode);
        }

        // There are some conditions under which we can't use
        // subpixel text rendering, even if enabled.
        if render_mode == FontRenderMode::Subpixel {
            // text on a stacking context that has filters
            // (e.g. opacity) can't use sub-pixel.
            // TODO(gw): It's possible we can relax this in
            //           the future, if we modify the way
            //           we handle subpixel blending.
            if let Some(sc_index) = self.stacking_context_stack.last() {
                let stacking_context = &self.stacking_context_store[sc_index.0];
                if !stacking_context.allow_subpixel_aa {
                    render_mode = FontRenderMode::Alpha;
                }
            }
        }

        let prim_font = FontInstance::new(
            font.font_key,
            font.size,
            *color,
            render_mode,
            font.subpx_dir,
            font.platform_options,
            font.variations.clone(),
            font.synthetic_italics,
        );
        let prim = TextRunPrimitiveCpu {
            font: prim_font,
            glyph_range,
            glyph_count,
            glyph_gpu_blocks: Vec::new(),
            glyph_keys: Vec::new(),
            offset: run_offset,
        };

        // Text shadows that have a blur radius of 0 need to be rendered as normal
        // text elements to get pixel perfect results for reftests. It's also a big
        // performance win to avoid blurs and render target allocations where
        // possible. For any text shadows that have zero blur, create a normal text
        // primitive with the shadow's color and offset. These need to be added
        // *before* the visual text primitive in order to get the correct paint
        // order. Store them in a Vec first to work around borrowck issues.
        // TODO(gw): Refactor to avoid having to store them in a Vec first.
        let mut fast_shadow_prims = Vec::new();
        for (idx, &(shadow_prim_index, _)) in self.shadow_prim_stack.iter().enumerate() {
            let shadow_metadata = &self.prim_store.cpu_metadata[shadow_prim_index.0];
            let picture_prim = &self.prim_store.cpu_pictures[shadow_metadata.cpu_prim_index.0];
            let shadow = picture_prim.as_shadow();
            if shadow.blur_radius == 0.0 {
                let mut text_prim = prim.clone();
                text_prim.font.color = shadow.color.into();
                text_prim.offset += shadow.offset;
                fast_shadow_prims.push((idx, text_prim));
            }
        }

        for (idx, text_prim) in fast_shadow_prims {
            let rect = info.rect;
            let mut info = info.clone();
            info.rect = rect.translate(&text_prim.offset);
            let prim_index = self.create_primitive(
                clip_and_scroll,
                &info,
                Vec::new(),
                PrimitiveContainer::TextRun(text_prim),
            );
            self.shadow_prim_stack[idx].1.push((prim_index, clip_and_scroll));
        }

        // Create (and add to primitive store) the primitive that will be
        // used for both the visual element and also the shadow(s).
        let prim_index = self.create_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::TextRun(prim),
        );

        // Only add a visual element if it can contribute to the scene.
        if color.a > 0.0 {
            if self.shadow_prim_stack.is_empty() {
                self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
                self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
            } else {
                self.pending_shadow_contents.push((prim_index, clip_and_scroll, *info));
            }
        }

        // Now add this primitive index to all the currently active text shadow
        // primitives. Although we're adding the indices *after* the visual
        // primitive here, they will still draw before the visual text, since
        // the shadow primitive itself has been added to the draw cmd
        // list *before* the visual element, during push_shadow. We need
        // the primitive index of the visual element here before we can add
        // the indices as sub-primitives to the shadow primitives.
        for &(shadow_prim_index, _) in &self.shadow_prim_stack {
            let shadow_metadata = &mut self.prim_store.cpu_metadata[shadow_prim_index.0];
            debug_assert_eq!(shadow_metadata.prim_kind, PrimitiveKind::Picture);
            let picture_prim =
                &mut self.prim_store.cpu_pictures[shadow_metadata.cpu_prim_index.0];

            // Only run real blurs here (fast path zero blurs are handled above).
            let blur_radius = picture_prim.as_shadow().blur_radius;
            if blur_radius > 0.0 {
                let shadow_rect = rect.inflate(
                    blur_radius,
                    blur_radius,
                );
                shadow_metadata.local_rect = shadow_metadata.local_rect.union(&shadow_rect);
                picture_prim.add_primitive(prim_index, clip_and_scroll);
            }
        }
    }

    pub fn add_box_shadow(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        prim_info: &LayerPrimitiveInfo,
        box_offset: &LayerVector2D,
        color: &ColorF,
        blur_radius: f32,
        spread_radius: f32,
        border_radius: f32,
        clip_mode: BoxShadowClipMode,
    ) {
        if color.a == 0.0 {
            return;
        }

        let spread_amount = match clip_mode {
            BoxShadowClipMode::Outset => {
                spread_radius
            }
            BoxShadowClipMode::Inset => {
                -spread_radius
            }
        };

        // Adjust the shadow box radius as per:
        // https://drafts.csswg.org/css-backgrounds-3/#shadow-shape
        let sharpness_scale = if border_radius < spread_radius {
            let r = border_radius / spread_amount;
            1.0 + (r - 1.0) * (r - 1.0) * (r - 1.0)
        } else {
            1.0
        };
        let shadow_radius = (border_radius + spread_amount * sharpness_scale).max(0.0);
        let shadow_rect = prim_info.rect
                                   .translate(box_offset)
                                   .inflate(spread_amount, spread_amount);

        if blur_radius == 0.0 {
            let mut clips = Vec::new();

            let fast_info = match clip_mode {
                BoxShadowClipMode::Outset => {
                    // TODO(gw): Add a fast path for ClipOut + zero border radius!
                    clips.push(ClipSource::RoundedRectangle(
                        prim_info.rect,
                        BorderRadius::uniform(border_radius),
                        ClipMode::ClipOut
                    ));

                    LayerPrimitiveInfo::with_clip(
                        shadow_rect,
                        LocalClip::RoundedRect(
                            shadow_rect,
                            ComplexClipRegion::new(shadow_rect, BorderRadius::uniform(shadow_radius)),
                        ),
                    )
                }
                BoxShadowClipMode::Inset => {
                    clips.push(ClipSource::RoundedRectangle(
                        shadow_rect,
                        BorderRadius::uniform(shadow_radius),
                        ClipMode::ClipOut
                    ));

                    LayerPrimitiveInfo::with_clip(
                        prim_info.rect,
                        LocalClip::RoundedRect(
                            prim_info.rect,
                            ComplexClipRegion::new(prim_info.rect, BorderRadius::uniform(border_radius)),
                        ),
                    )
                }
            };

            self.add_primitive(
                clip_and_scroll,
                &fast_info,
                clips,
                PrimitiveContainer::Rectangle(RectanglePrimitive {
                    color: *color,
                }),
            );
        } else {
            let shadow = Shadow {
                blur_radius,
                color: *color,
                offset: LayerVector2D::zero(),
            };

            let blur_offset = 2.0 * blur_radius;
            let mut extra_clips = vec![];
            let mut pic_prim = PicturePrimitive::new_shadow(shadow, RenderTargetKind::Alpha);

            let pic_info = match clip_mode {
                BoxShadowClipMode::Outset => {
                    let brush_prim = BrushPrimitive {
                        clip_mode: ClipMode::Clip,
                        radius: shadow_radius,
                    };

                    let brush_rect = LayerRect::new(LayerPoint::new(blur_offset, blur_offset),
                                                    shadow_rect.size);

                    let brush_info = LayerPrimitiveInfo::new(brush_rect);

                    let brush_prim_index = self.create_primitive(
                        clip_and_scroll,
                        &brush_info,
                        Vec::new(),
                        PrimitiveContainer::Brush(brush_prim),
                    );

                    pic_prim.add_primitive(brush_prim_index, clip_and_scroll);

                    extra_clips.push(ClipSource::RoundedRectangle(
                        prim_info.rect,
                        BorderRadius::uniform(border_radius),
                        ClipMode::ClipOut,
                    ));

                    let pic_rect = shadow_rect.inflate(blur_offset, blur_offset);
                    LayerPrimitiveInfo::new(pic_rect)
                }
                BoxShadowClipMode::Inset => {
                    let brush_prim = BrushPrimitive {
                        clip_mode: ClipMode::ClipOut,
                        radius: shadow_radius,
                    };

                    let mut brush_rect = shadow_rect;
                    brush_rect.origin.x = brush_rect.origin.x - prim_info.rect.origin.x + blur_offset;
                    brush_rect.origin.y = brush_rect.origin.y - prim_info.rect.origin.y + blur_offset;

                    let brush_info = LayerPrimitiveInfo::new(brush_rect);

                    let brush_prim_index = self.create_primitive(
                        clip_and_scroll,
                        &brush_info,
                        Vec::new(),
                        PrimitiveContainer::Brush(brush_prim),
                    );

                    pic_prim.add_primitive(brush_prim_index, clip_and_scroll);

                    extra_clips.push(ClipSource::RoundedRectangle(
                        prim_info.rect,
                        BorderRadius::uniform(border_radius),
                        ClipMode::Clip,
                    ));

                    let pic_rect = prim_info.rect.inflate(blur_offset, blur_offset);
                    LayerPrimitiveInfo::with_clip_rect(pic_rect, prim_info.rect)
                }
            };

            self.add_primitive(
                clip_and_scroll,
                &pic_info,
                extra_clips,
                PrimitiveContainer::Picture(pic_prim),
            );
        }
    }

    pub fn add_image(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        stretch_size: &LayerSize,
        tile_spacing: &LayerSize,
        sub_rect: Option<TexelRect>,
        image_key: ImageKey,
        image_rendering: ImageRendering,
        tile: Option<TileOffset>,
    ) {
        let sub_rect_block = sub_rect.unwrap_or(TexelRect::invalid()).into();

        let prim_cpu = ImagePrimitiveCpu {
            image_key,
            image_rendering,
            tile_offset: tile,
            tile_spacing: *tile_spacing,
            gpu_blocks: [
                [
                    stretch_size.width,
                    stretch_size.height,
                    tile_spacing.width,
                    tile_spacing.height,
                ].into(),
                sub_rect_block,
            ],
        };

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Image(prim_cpu),
        );
    }

    pub fn add_yuv_image(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        yuv_data: YuvData,
        color_space: YuvColorSpace,
        image_rendering: ImageRendering,
    ) {
        let format = yuv_data.get_format();
        let yuv_key = match yuv_data {
            YuvData::NV12(plane_0, plane_1) => [plane_0, plane_1, ImageKey::dummy()],
            YuvData::PlanarYCbCr(plane_0, plane_1, plane_2) => [plane_0, plane_1, plane_2],
            YuvData::InterleavedYCbCr(plane_0) => [plane_0, ImageKey::dummy(), ImageKey::dummy()],
        };

        let prim_cpu = YuvImagePrimitiveCpu {
            yuv_key,
            format,
            color_space,
            image_rendering,
            gpu_block: [info.rect.size.width, info.rect.size.height, 0.0, 0.0].into(),
        };

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::YuvImage(prim_cpu),
        );
    }

    fn handle_push_stacking_context(&mut self, stacking_context_index: StackingContextIndex) {
        self.stacking_context_stack.push(stacking_context_index);

        // Reset bounding rect to zero. We will calculate it as we collect primitives
        // from various scroll layers. In handle_pop_stacking_context , we use this to
        // calculate the device bounding rect. In the future, we could cache this during
        // the initial adding of items for the common case (where there is only a single
        // scroll layer for items in a stacking context).
        let stacking_context =
            &mut self.stacking_context_store[stacking_context_index.0];
        stacking_context.screen_bounds = DeviceIntRect::zero();
        stacking_context.isolated_items_bounds = LayerRect::zero();
    }

    pub fn get_packed_layer_index_if_visible(
        &self,
        clip_and_scroll: &ClipAndScrollInfo
    ) -> Option<PackedLayerIndex> {
        let group_id = self.clip_scroll_group_indices[&clip_and_scroll];
        let clip_scroll_group = &self.clip_scroll_group_store[group_id];
        if clip_scroll_group.is_visible() {
            Some(clip_scroll_group.packed_layer_index)
        } else {
            None
        }
    }

    pub fn hit_test(
        &self,
        clip_scroll_tree: &ClipScrollTree,
        pipeline_id: Option<PipelineId>,
        point: WorldPoint,
        flags: HitTestFlags
    ) -> HitTestResult {
        let point = if flags.contains(HitTestFlags::POINT_RELATIVE_TO_PIPELINE_VIEWPORT) {
            let point = LayerPoint::new(point.x, point.y);
            clip_scroll_tree.make_node_relative_point_absolute(pipeline_id, &point)
        } else {
            point
        };

        let mut node_cache = FastHashMap::default();
        let mut result = HitTestResult::default();
        for &HitTestingRun(ref items, ref clip_and_scroll) in self.hit_testing_runs.iter().rev() {
            let scroll_node = &clip_scroll_tree.nodes[&clip_and_scroll.scroll_node_id];
            match (pipeline_id, scroll_node.pipeline_id) {
                (Some(id), node_id) if node_id != id => continue,
                _ => {},
            }

            let transform = scroll_node.world_content_transform;
            let point_in_layer = match transform.inverse() {
                Some(inverted) => inverted.transform_point2d(&point),
                None => continue,
            };

            let mut clipped_in = false;
            for item in items.iter().rev() {
                if !item.rect.contains(&point_in_layer) || !item.clip.contains(&point_in_layer) {
                    continue;
                }

                let clip_id = &clip_and_scroll.clip_node_id();
                if !clipped_in {
                    clipped_in = clip_scroll_tree.is_point_clipped_in_for_node(point,
                                                                               clip_id,
                                                                               &mut node_cache,
                                                                               &self.clip_store);
                    if !clipped_in {
                        break;
                    }
                }

                let root_pipeline_reference_frame_id =
                    ClipId::root_reference_frame(clip_id.pipeline_id());
                let point_in_viewport = match node_cache.get(&root_pipeline_reference_frame_id) {
                    Some(&Some(point)) => point,
                    _ => unreachable!("Hittest target's root reference frame not hit."),
                };

                result.items.push(HitTestItem {
                    pipeline: clip_and_scroll.clip_node_id().pipeline_id(),
                    tag: item.tag,
                    point_in_viewport,
                    point_relative_to_item: point_in_layer - item.rect.origin.to_vector(),
                });
                if !flags.contains(HitTestFlags::FIND_ALL) {
                    return result;
                }
            }
        }

        result.items.dedup();
        return result;
    }


    fn handle_primitive_run(
        &mut self,
        base_prim_index: PrimitiveIndex,
        prim_count: usize,
        clip_and_scroll: ClipAndScrollInfo,
        render_tasks: &mut RenderTaskTree,
        gpu_cache: &mut GpuCache,
        resource_cache: &mut ResourceCache,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        clip_scroll_tree: &ClipScrollTree,
        device_pixel_ratio: f32,
        profile_counters: &mut FrameProfileCounters,
    ) -> bool {
        let stacking_context_index = *self.stacking_context_stack.last().unwrap();
        let packed_layer_index =
            match self.get_packed_layer_index_if_visible(&clip_and_scroll) {
            Some(index) => index,
            None => {
                debug!("{:?} of invisible {:?}", base_prim_index, stacking_context_index);
                return false;
            }
        };

        let (clip_chain, clip_bounds, coordinate_system_id) =
            match clip_scroll_tree.nodes.get(&clip_and_scroll.clip_node_id()) {
            Some(node) if node.combined_clip_outer_bounds != DeviceIntRect::zero() => {
                let group_id = self.clip_scroll_group_indices[&clip_and_scroll];
                (
                    node.clip_chain_node.clone(),
                    node.combined_clip_outer_bounds,
                    self.clip_scroll_group_store[group_id].coordinate_system_id,
                )
            }
            _ => {
                let group_id = self.clip_scroll_group_indices[&clip_and_scroll];
                self.clip_scroll_group_store[group_id].screen_bounding_rect = None;

                debug!("{:?} of clipped out {:?}", base_prim_index, stacking_context_index);
                return false;
            }
        };

        let stacking_context = &mut self.stacking_context_store[stacking_context_index.0];
        let pipeline_id = {
            if !stacking_context.can_contribute_to_scene() {
                return false;
            }

            // At least one primitive in this stacking context is visible, so the stacking
            // context is visible.
            stacking_context.is_visible = true;
            stacking_context.pipeline_id
        };

        debug!(
            "\t{:?} of {:?} at {:?}",
            base_prim_index,
            stacking_context_index,
            packed_layer_index
        );

        let packed_layer = &self.packed_layers[packed_layer_index.0];
        let display_list = &pipelines
            .get(&pipeline_id)
            .expect("No display list?")
            .display_list;

        if !stacking_context.is_backface_visible && packed_layer.transform.is_backface_visible() {
            return false;
        }

        let prim_context = PrimitiveContext::new(
            packed_layer_index,
            packed_layer,
            clip_and_scroll.clip_node_id(),
            clip_chain,
            clip_bounds,
            coordinate_system_id,
            device_pixel_ratio,
            display_list,
        );

        debug!(
            "\tclip_bounds {:?}, layer_local_clip {:?}",
            prim_context.clip_bounds,
            packed_layer.local_clip_rect
        );

        for i in 0 .. prim_count {
            let prim_index = PrimitiveIndex(base_prim_index.0 + i);

            if let Some(prim_geom) = self.prim_store.prepare_prim_for_render(
                prim_index,
                &prim_context,
                resource_cache,
                gpu_cache,
                render_tasks,
                &mut self.clip_store,
            ) {
                stacking_context.screen_bounds = stacking_context
                    .screen_bounds
                    .union(&prim_geom.device_rect);
                stacking_context.isolated_items_bounds = stacking_context
                    .isolated_items_bounds
                    .union(&prim_geom.local_rect);
                stacking_context.has_any_primitive = true;

                profile_counters.visible_primitives.inc();
            }
        }

        true //visible
    }

    fn handle_pop_stacking_context(
        &mut self,
        screen_rect: &DeviceIntRect,
        clip_scroll_tree: &ClipScrollTree) {
        let stacking_context_index = self.stacking_context_stack.pop().unwrap();

        let (bounding_rect, is_visible, is_preserve_3d, reference_id, reference_bounds) = {
            let stacking_context =
                &mut self.stacking_context_store[stacking_context_index.0];
            if !stacking_context.has_any_primitive {
                stacking_context.isolated_items_bounds = stacking_context.children_sc_bounds;
            } else if stacking_context.isolation != ContextIsolation::Items {
                stacking_context.isolated_items_bounds = stacking_context
                    .isolated_items_bounds
                    .union(&stacking_context.children_sc_bounds);
            }
            stacking_context.screen_bounds = stacking_context
                .screen_bounds
                .intersection(screen_rect)
                .unwrap_or(DeviceIntRect::zero());
            (
                stacking_context.screen_bounds.clone(),
                stacking_context.is_visible,
                stacking_context.isolation == ContextIsolation::Items,
                stacking_context.reference_frame_id,
                stacking_context
                    .isolated_items_bounds
                    .translate(&stacking_context.reference_frame_offset),
            )
        };

        if let Some(ref mut parent_index) = self.stacking_context_stack.last_mut() {
            let parent = &mut self.stacking_context_store[parent_index.0];
            parent.screen_bounds = parent.screen_bounds.union(&bounding_rect);
            let child_bounds = reference_bounds.translate(&-parent.reference_frame_offset);
            let frame_node = clip_scroll_tree
                .nodes
                .get(&reference_id)
                .unwrap();
            let local_transform = match frame_node.node_type {
                NodeType::ReferenceFrame(ref info) => info.transform,
                _ => LayerToScrollTransform::identity(),
            };
            let transformed_bounds = local_transform
                .with_destination::<LayerPixel>()
                .transform_rect(&child_bounds);
            parent.children_sc_bounds = parent.children_sc_bounds.union(&transformed_bounds);
            // add children local bounds only for non-item-isolated contexts
            if !is_preserve_3d && parent.reference_frame_id == reference_id {
                parent.isolated_items_bounds = parent.isolated_items_bounds.union(&child_bounds);
            }
            // Per-primitive stacking context visibility checks do not take into account
            // visibility of child stacking contexts, so do that now.
            parent.is_visible = parent.is_visible || is_visible;
        }
    }

    fn recalculate_clip_scroll_groups(
        &mut self,
        clip_scroll_tree: &ClipScrollTree,
        screen_rect: &DeviceIntRect,
        device_pixel_ratio: f32
    ) {
        debug!("recalculate_clip_scroll_groups");
        for ref mut group in &mut self.clip_scroll_group_store {
            let scroll_node = &clip_scroll_tree.nodes[&group.scroll_node_id];
            let clip_node = &clip_scroll_tree.nodes[&group.clip_node_id];
            let packed_layer = &mut self.packed_layers[group.packed_layer_index.0];

            debug!(
                "\tProcessing group scroll={:?}, clip={:?}",
                group.scroll_node_id,
                group.clip_node_id
            );

            let transform = scroll_node.world_content_transform;
            if !packed_layer.set_transform(transform) {
                debug!("\t\tUnable to set transform {:?}", transform);
                return;
            }

            // Here we move the viewport rectangle into the coordinate system
            // of the stacking context content.
            let local_viewport_rect = clip_node
                .combined_local_viewport_rect
                .translate(&clip_node.reference_frame_relative_scroll_offset)
                .translate(&-scroll_node.reference_frame_relative_scroll_offset)
                .translate(&-scroll_node.scroll_offset());

            group.screen_bounding_rect = packed_layer.set_rect(
                &local_viewport_rect,
                screen_rect,
                device_pixel_ratio,
            );

            group.coordinate_system_id = scroll_node.coordinate_system_id;

            debug!(
                "\t\tlocal viewport {:?} screen bound {:?}",
                local_viewport_rect,
                group.screen_bounding_rect
            );
        }
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn build_layer_screen_rects_and_cull_layers(
        &mut self,
        screen_rect: &DeviceIntRect,
        clip_scroll_tree: &mut ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        profile_counters: &mut FrameProfileCounters,
        device_pixel_ratio: f32,
    ) {
        profile_scope!("cull");

        self.recalculate_clip_scroll_groups(
            clip_scroll_tree,
            screen_rect,
            device_pixel_ratio
        );

        debug!("processing commands...");
        let commands = mem::replace(&mut self.cmds, Vec::new());
        for cmd in &commands {
            match *cmd {
                PrimitiveRunCmd::PushStackingContext(stacking_context_index) => {
                    self.handle_push_stacking_context(stacking_context_index)
                }
                PrimitiveRunCmd::PrimitiveRun(prim_index, prim_count, clip_and_scroll) => {
                    self.handle_primitive_run(
                        prim_index,
                        prim_count,
                        clip_and_scroll,
                        render_tasks,
                        gpu_cache,
                        resource_cache,
                        pipelines,
                        clip_scroll_tree,
                        device_pixel_ratio,
                        profile_counters,
                    );
                }
                PrimitiveRunCmd::PopStackingContext => {
                    self.handle_pop_stacking_context(screen_rect, clip_scroll_tree);
                }
            }
        }

        mem::replace(&mut self.cmds, commands);
    }

    fn update_scroll_bars(&mut self, clip_scroll_tree: &ClipScrollTree, gpu_cache: &mut GpuCache) {
        let distance_from_edge = 8.0;

        for scrollbar_prim in &self.scrollbar_prims {
            let metadata = &mut self.prim_store.cpu_metadata[scrollbar_prim.prim_index.0];
            let clip_scroll_node = &clip_scroll_tree.nodes[&scrollbar_prim.clip_id];

            // Invalidate what's in the cache so it will get rebuilt.
            gpu_cache.invalidate(&metadata.gpu_location);

            let scrollable_distance = clip_scroll_node.scrollable_size().height;

            if scrollable_distance <= 0.0 {
                metadata.local_clip_rect.size = LayerSize::zero();
                continue;
            }

            let scroll_offset = clip_scroll_node.scroll_offset();
            let f = -scroll_offset.y / scrollable_distance;

            let min_y = clip_scroll_node.local_viewport_rect.origin.y - scroll_offset.y +
                distance_from_edge;

            let max_y = clip_scroll_node.local_viewport_rect.origin.y +
                clip_scroll_node.local_viewport_rect.size.height -
                scroll_offset.y - metadata.local_rect.size.height -
                distance_from_edge;

            metadata.local_rect.origin.x = clip_scroll_node.local_viewport_rect.origin.x +
                clip_scroll_node.local_viewport_rect.size.width -
                metadata.local_rect.size.width -
                distance_from_edge;

            metadata.local_rect.origin.y = util::lerp(min_y, max_y, f);
            metadata.local_clip_rect = metadata.local_rect;

            // TODO(gw): The code to set / update border clips on scroll bars
            //           has been broken for a long time, so I've removed it
            //           for now. We can re-add that code once the clips
            //           data is moved over to the GPU cache!
        }
    }

    fn build_render_task(
        &mut self,
        clip_scroll_tree: &ClipScrollTree,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        output_pipelines: &FastHashSet<PipelineId>,
        device_pixel_ratio: f32,
    ) -> RenderTaskId {
        profile_scope!("build_render_task");

        let mut next_z = 0;
        let mut sc_stack: Vec<StackingContextIndex> = Vec::new();
        let mut current_task =
            RenderTask::new_alpha_batch(DeviceIntPoint::zero(), RenderTaskLocation::Fixed, None);
        // A stack of the alpha batcher tasks. We create them on the way down,
        // and then actually populate with items and dependencies on the way up.
        let mut alpha_task_stack = Vec::new();
        // A map of "preserve-3d" contexts. We are baking these into render targets
        // and only compositing once we are out of "preserve-3d" hierarchy.
        // The stacking contexts that fall into this category are
        //  - ones with `ContextIsolation::Items`, for their actual items to be backed
        //  - immediate children of `ContextIsolation::Items`
        let mut preserve_3d_map_stack: Vec<FastHashMap<StackingContextIndex, RenderTaskId>> =
            Vec::new();
        // The plane splitter stack, using a simple BSP tree.
        let mut splitter_stack = Vec::new();

        debug!("build_render_task()");

        for cmd in &self.cmds {
            match *cmd {
                PrimitiveRunCmd::PushStackingContext(stacking_context_index) => {
                    let parent_isolation = sc_stack
                        .last()
                        .map(|index| self.stacking_context_store[index.0].isolation);
                    let stacking_context = &self.stacking_context_store[stacking_context_index.0];
                    sc_stack.push(stacking_context_index);

                    if !stacking_context.is_visible {
                        continue;
                    }

                    debug!(
                        "\tpush {:?} {:?}",
                        stacking_context_index,
                        stacking_context.isolation
                    );

                    let stacking_context_rect = &stacking_context.screen_bounds;
                    let composite_count = stacking_context.composite_ops.count();

                    // If this stacking context if the root of a pipeline, and the caller
                    // has requested it as an output frame, create a render task to isolate it.
                    if stacking_context.is_pipeline_root &&
                        output_pipelines.contains(&stacking_context.pipeline_id)
                    {
                        alpha_task_stack.push(current_task);
                        current_task = RenderTask::new_dynamic_alpha_batch(
                            stacking_context_rect,
                            Some(stacking_context.pipeline_id),
                        );
                    }

                    if stacking_context.isolation == ContextIsolation::Full && composite_count == 0
                    {
                        alpha_task_stack.push(current_task);
                        current_task =
                            RenderTask::new_dynamic_alpha_batch(stacking_context_rect, None);
                    }

                    if parent_isolation == Some(ContextIsolation::Items) ||
                        stacking_context.isolation == ContextIsolation::Items
                    {
                        if parent_isolation != Some(ContextIsolation::Items) {
                            splitter_stack.push(BspSplitter::new());
                            preserve_3d_map_stack.push(FastHashMap::default());
                        }
                        alpha_task_stack.push(current_task);
                        current_task =
                            RenderTask::new_dynamic_alpha_batch(stacking_context_rect, None);
                        //Note: technically, we shouldn't make a new alpha task for "preserve-3d" contexts
                        // that have no child items (only other stacking contexts). However, we don't know if
                        // there are any items at this time (in `PushStackingContext`).
                        //Note: the reason we add the polygon for splitting during `Push*` as opposed to `Pop*`
                        // is because we need to preserve the order of drawing for planes that match together.
                        let frame_node = clip_scroll_tree
                            .nodes
                            .get(&stacking_context.reference_frame_id)
                            .unwrap();
                        let sc_polygon =
                            make_polygon(stacking_context, frame_node, stacking_context_index.0);
                        debug!(
                            "\tsplitter[{}]: add {:?} -> {:?} with bounds {:?}",
                            splitter_stack.len(),
                            stacking_context_index,
                            sc_polygon,
                            stacking_context.isolated_items_bounds
                        );
                        splitter_stack.last_mut().unwrap().add(sc_polygon);
                    }

                    for _ in 0 .. composite_count {
                        alpha_task_stack.push(current_task);
                        current_task =
                            RenderTask::new_dynamic_alpha_batch(stacking_context_rect, None);
                    }
                }
                PrimitiveRunCmd::PopStackingContext => {
                    let stacking_context_index = sc_stack.pop().unwrap();
                    let stacking_context = &self.stacking_context_store[stacking_context_index.0];
                    let composite_count = stacking_context.composite_ops.count();

                    if !stacking_context.is_visible {
                        continue;
                    }

                    debug!("\tpop {:?}", stacking_context_index);
                    let parent_isolation = sc_stack
                        .last()
                        .map(|index| self.stacking_context_store[index.0].isolation);

                    if stacking_context.isolation == ContextIsolation::Full && composite_count == 0
                    {
                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let screen_origin = current_task.as_alpha_batch().screen_origin;
                        let current_task_id = render_tasks.add(current_task);
                        let item = AlphaRenderItem::HardwareComposite(
                            stacking_context_index,
                            current_task_id,
                            HardwareCompositeOp::PremultipliedAlpha,
                            screen_origin,
                            next_z,
                        );
                        next_z += 1;
                        prev_task.as_alpha_batch_mut().items.push(item);
                        prev_task.children.push(current_task_id);
                        current_task = prev_task;
                    }

                    for filter in &stacking_context.composite_ops.filters {
                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let screen_origin = current_task.as_alpha_batch().screen_origin;
                        let current_task_id = render_tasks.add(current_task);
                        match *filter {
                            FilterOp::Blur(blur_radius) => {
                                let blur_radius = device_length(blur_radius, device_pixel_ratio);
                                render_tasks.get_mut(current_task_id)
                                            .inflate(blur_radius.0);
                                let blur_render_task = RenderTask::new_blur(
                                    blur_radius,
                                    current_task_id,
                                    render_tasks,
                                    RenderTargetKind::Color,
                                );
                                let blur_render_task_id = render_tasks.add(blur_render_task);
                                let item = AlphaRenderItem::HardwareComposite(
                                    stacking_context_index,
                                    blur_render_task_id,
                                    HardwareCompositeOp::PremultipliedAlpha,
                                    DeviceIntPoint::new(
                                        screen_origin.x - blur_radius.0,
                                        screen_origin.y - blur_radius.0,
                                    ),
                                    next_z,
                                );
                                prev_task.as_alpha_batch_mut().items.push(item);
                                prev_task.children.push(blur_render_task_id);
                                current_task = prev_task;
                            }
                            _ => {
                                let item = AlphaRenderItem::Blend(
                                    stacking_context_index,
                                    current_task_id,
                                    *filter,
                                    next_z,
                                );
                                prev_task.as_alpha_batch_mut().items.push(item);
                                prev_task.children.push(current_task_id);
                                current_task = prev_task;
                            }
                        }
                        next_z += 1;
                    }

                    if let Some(mix_blend_mode) = stacking_context.composite_ops.mix_blend_mode {
                        let backdrop_task =
                            RenderTask::new_readback(stacking_context.screen_bounds);
                        let source_task_id = render_tasks.add(current_task);
                        let backdrop_task_id = render_tasks.add(backdrop_task);

                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let item = AlphaRenderItem::Composite(
                            stacking_context_index,
                            source_task_id,
                            backdrop_task_id,
                            mix_blend_mode,
                            next_z,
                        );
                        next_z += 1;
                        prev_task.as_alpha_batch_mut().items.push(item);
                        prev_task.children.push(source_task_id);
                        prev_task.children.push(backdrop_task_id);
                        current_task = prev_task;
                    }

                    if parent_isolation == Some(ContextIsolation::Items) ||
                        stacking_context.isolation == ContextIsolation::Items
                    {
                        //Note: we don't register the dependent tasks here. It's only done
                        // when we are out of the `preserve-3d` branch (see the code below),
                        // since this is only where the parent task is known.
                        let current_task_id = render_tasks.add(current_task);
                        preserve_3d_map_stack
                            .last_mut()
                            .unwrap()
                            .insert(stacking_context_index, current_task_id);
                        current_task = alpha_task_stack.pop().unwrap();
                    }

                    if parent_isolation != Some(ContextIsolation::Items) &&
                        stacking_context.isolation == ContextIsolation::Items
                    {
                        debug!("\tsplitter[{}]: flush", splitter_stack.len());
                        let mut splitter = splitter_stack.pop().unwrap();
                        // Flush the accumulated plane splits onto the task tree.
                        // Notice how this is done before splitting in order to avoid duplicate tasks.
                        current_task
                            .children
                            .extend(preserve_3d_map_stack.last().unwrap().values().cloned());
                        // Z axis is directed at the screen, `sort` is ascending, and we need back-to-front order.
                        for poly in splitter.sort(vec3(0.0, 0.0, 1.0)) {
                            let sc_index = StackingContextIndex(poly.anchor);
                            let task_id = preserve_3d_map_stack.last().unwrap()[&sc_index];
                            debug!("\t\tproduce {:?} -> {:?} for {:?}", sc_index, poly, task_id);
                            let pp = &poly.points;
                            let gpu_blocks = [
                                [pp[0].x as f32, pp[0].y as f32, pp[0].z as f32, pp[1].x as f32].into(),
                                [pp[1].y as f32, pp[1].z as f32, pp[2].x as f32, pp[2].y as f32].into(),
                                [pp[2].z as f32, pp[3].x as f32, pp[3].y as f32, pp[3].z as f32].into(),
                            ];
                            let handle = gpu_cache.push_per_frame_blocks(&gpu_blocks);
                            let item =
                                AlphaRenderItem::SplitComposite(sc_index, task_id, handle, next_z);
                            current_task.as_alpha_batch_mut().items.push(item);
                        }
                        preserve_3d_map_stack.pop();
                        next_z += 1;
                    }

                    if stacking_context.is_pipeline_root &&
                        output_pipelines.contains(&stacking_context.pipeline_id)
                    {
                        let mut prev_task = alpha_task_stack.pop().unwrap();
                        let screen_origin = current_task.as_alpha_batch().screen_origin;
                        let current_task_id = render_tasks.add(current_task);
                        let item = AlphaRenderItem::HardwareComposite(
                            stacking_context_index,
                            current_task_id,
                            HardwareCompositeOp::PremultipliedAlpha,
                            screen_origin,
                            next_z,
                        );
                        next_z += 1;
                        prev_task.as_alpha_batch_mut().items.push(item);
                        prev_task.children.push(current_task_id);
                        current_task = prev_task;
                    }
                }
                PrimitiveRunCmd::PrimitiveRun(first_prim_index, prim_count, clip_and_scroll) => {
                    let stacking_context_index = *sc_stack.last().unwrap();
                    if !self.stacking_context_store[stacking_context_index.0].is_visible {
                        continue;
                    }

                    let group_id = self.clip_scroll_group_indices[&clip_and_scroll];
                    let group_index = ClipScrollGroupIndex(group_id, clip_and_scroll);

                    if self.clip_scroll_group_store[group_id]
                        .screen_bounding_rect
                        .is_none()
                    {
                        debug!("\tcs-group {:?} screen rect is None", group_index);
                        continue;
                    }

                    debug!("\trun of {} items", prim_count);

                    for i in 0 .. prim_count {
                        let prim_index = PrimitiveIndex(first_prim_index.0 + i);

                        if self.prim_store.cpu_metadata[prim_index.0].screen_rect.is_some() {
                            self.prim_store
                                .add_render_tasks_for_prim(prim_index, &mut current_task);
                            let item =
                                AlphaRenderItem::Primitive(Some(group_index), prim_index, next_z);
                            current_task.as_alpha_batch_mut().items.push(item);
                            next_z += 1;
                        }
                    }
                }
            }
        }

        debug_assert!(alpha_task_stack.is_empty());
        debug_assert!(preserve_3d_map_stack.is_empty());
        render_tasks.add(current_task)
    }

    pub fn build(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        frame_id: FrameId,
        clip_scroll_tree: &mut ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        device_pixel_ratio: f32,
        pan: LayerPoint,
        output_pipelines: &FastHashSet<PipelineId>,
        texture_cache_profile: &mut TextureCacheProfileCounters,
        gpu_cache_profile: &mut GpuCacheProfileCounters,
    ) -> Frame {
        profile_scope!("build");

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters
            .total_primitives
            .set(self.prim_store.prim_count());

        resource_cache.begin_frame(frame_id);
        gpu_cache.begin_frame();

        let screen_rect = DeviceIntRect::new(
            DeviceIntPoint::zero(),
            DeviceIntSize::new(
                self.screen_size.width as i32,
                self.screen_size.height as i32,
            ),
        );

        clip_scroll_tree.update_all_node_transforms(
            &screen_rect,
            device_pixel_ratio,
            &mut self.packed_layers,
            &mut self.clip_store,
            resource_cache,
            gpu_cache,
            pan
        );

        self.update_scroll_bars(clip_scroll_tree, gpu_cache);

        let mut render_tasks = RenderTaskTree::new();

        self.build_layer_screen_rects_and_cull_layers(
            &screen_rect,
            clip_scroll_tree,
            pipelines,
            resource_cache,
            gpu_cache,
            &mut render_tasks,
            &mut profile_counters,
            device_pixel_ratio,
        );

        let main_render_task_id = self.build_render_task(
            clip_scroll_tree,
            gpu_cache,
            &mut render_tasks,
            output_pipelines,
            device_pixel_ratio,
        );

        let mut required_pass_count = 0;
        render_tasks.max_depth(main_render_task_id, 0, &mut required_pass_count);

        resource_cache.block_until_all_resources_added(gpu_cache, texture_cache_profile);

        let mut deferred_resolves = vec![];

        let mut passes = Vec::new();

        // Do the allocations now, assigning each tile's tasks to a render
        // pass and target as required.
        for index in 0 .. required_pass_count {
            passes.push(RenderPass::new(index == required_pass_count - 1));
        }

        render_tasks.assign_to_passes(main_render_task_id, passes.len() - 1, &mut passes);

        for pass in &mut passes {
            let ctx = RenderTargetContext {
                device_pixel_ratio,
                stacking_context_store: &self.stacking_context_store,
                clip_scroll_group_store: &self.clip_scroll_group_store,
                prim_store: &self.prim_store,
                resource_cache,
            };

            pass.build(
                &ctx,
                gpu_cache,
                &mut render_tasks,
                &mut deferred_resolves,
                &self.clip_store,
            );

            profile_counters.passes.inc();
            profile_counters
                .color_targets
                .add(pass.color_targets.target_count());
            profile_counters
                .alpha_targets
                .add(pass.alpha_targets.target_count());
        }

        let gpu_cache_updates = gpu_cache.end_frame(gpu_cache_profile);

        render_tasks.build();

        resource_cache.end_frame();

        Frame {
            device_pixel_ratio,
            background_color: self.background_color,
            window_size: self.screen_size,
            profile_counters,
            passes,
            layer_texture_data: self.packed_layers.clone(),
            render_tasks,
            deferred_resolves,
            gpu_cache_updates: Some(gpu_cache_updates),
        }
    }
}

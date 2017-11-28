/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderDetails, BorderDisplayItem, BuiltDisplayList};
use api::{ClipAndScrollInfo, ClipId, ColorF, PropertyBinding};
use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{DocumentLayer, ExtendMode, FontRenderMode, LayoutTransform};
use api::{GlyphInstance, GlyphOptions, GradientStop, HitTestFlags, HitTestItem, HitTestResult};
use api::{ImageKey, ImageRendering, ItemRange, ItemTag, LayerPoint, LayerPrimitiveInfo, LayerRect};
use api::{LayerSize, LayerToScrollTransform, LayerVector2D, LayoutVector2D, LineOrientation};
use api::{LineStyle, LocalClip, PipelineId, RepeatMode};
use api::{ScrollSensitivity, Shadow, TileOffset, TransformStyle};
use api::{PremultipliedColorF, WorldPoint, YuvColorSpace, YuvData};
use app_units::Au;
use border::ImageBorderSegment;
use clip::{ClipRegion, ClipSource, ClipSources, ClipStore, Contains};
use clip_scroll_node::{ClipScrollNode, NodeType};
use clip_scroll_tree::ClipScrollTree;
use euclid::{SideOffsets2D, vec2};
use frame::FrameId;
use glyph_rasterizer::FontInstance;
use gpu_cache::GpuCache;
use internal_types::{FastHashMap, FastHashSet};
use picture::{PictureCompositeMode, PictureKind, PicturePrimitive, RasterizationSpace};
use prim_store::{TexelRect, YuvImagePrimitiveCpu};
use prim_store::{GradientPrimitiveCpu, ImagePrimitiveCpu, LinePrimitive, PrimitiveKind};
use prim_store::{PrimitiveContainer, PrimitiveIndex, SpecificPrimitiveIndex};
use prim_store::{PrimitiveStore, RadialGradientPrimitiveCpu};
use prim_store::{RectangleContent, RectanglePrimitive, TextRunPrimitiveCpu};
use profiler::{FrameProfileCounters, GpuCacheProfileCounters, TextureCacheProfileCounters};
use render_task::{ClearMode, RenderTask, RenderTaskId, RenderTaskTree};
use resource_cache::ResourceCache;
use scene::{ScenePipeline, SceneProperties};
use std::{mem, usize, f32};
use tiling::{CompositeOps, Frame};
use tiling::{RenderPass, RenderPassKind, RenderTargetKind};
use tiling::{RenderTargetContext, ScrollbarPrimitive};
use util::{self, MaxRect, pack_as_float, RectHelpers, recycle_vec};

#[derive(Debug)]
pub struct ScrollbarInfo(pub ClipId, pub LayerRect);

/// Properties of a stacking context that are maintained
/// during creation of the scene. These structures are
/// not persisted after the initial scene build.
struct StackingContext {
    /// Pipeline this stacking context belongs to.
    pipeline_id: PipelineId,

    /// Filters / mix-blend-mode effects
    composite_ops: CompositeOps,

    /// If true, visible when backface is visible.
    is_backface_visible: bool,

    /// Allow subpixel AA for text runs on this stacking context.
    /// This is a temporary hack while we don't support subpixel AA
    /// on transparent stacking contexts.
    allow_subpixel_aa: bool,

    /// CSS transform-style property.
    transform_style: TransformStyle,

    /// The primitive index for the root Picture primitive
    /// that this stacking context is mapped to.
    pic_prim_index: PrimitiveIndex,
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

/// A builder structure for `tiling::Frame`
pub struct FrameBuilder {
    screen_rect: DeviceUintRect,
    background_color: Option<ColorF>,
    prim_store: PrimitiveStore,
    pub clip_store: ClipStore,
    hit_testing_runs: Vec<HitTestingRun>,
    pub config: FrameBuilderConfig,

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

    /// A stack of the current pictures, used during scene building.
    pub picture_stack: Vec<PrimitiveIndex>,

    /// A temporary stack of stacking context properties, used only
    /// during scene building.
    sc_stack: Vec<StackingContext>,
}

pub struct PrimitiveContext<'a> {
    pub device_pixel_ratio: f32,
    pub display_list: &'a BuiltDisplayList,
    pub clip_node: &'a ClipScrollNode,
    pub scroll_node: &'a ClipScrollNode,
}

impl<'a> PrimitiveContext<'a> {
    pub fn new(
        device_pixel_ratio: f32,
        display_list: &'a BuiltDisplayList,
        clip_node: &'a ClipScrollNode,
        scroll_node: &'a ClipScrollNode,
    ) -> Self {
        PrimitiveContext {
            device_pixel_ratio,
            display_list,
            clip_node,
            scroll_node,
        }
    }
}

impl FrameBuilder {
    pub fn empty() -> Self {
        FrameBuilder {
            hit_testing_runs: Vec::new(),
            shadow_prim_stack: Vec::new(),
            pending_shadow_contents: Vec::new(),
            scrollbar_prims: Vec::new(),
            reference_frame_stack: Vec::new(),
            picture_stack: Vec::new(),
            sc_stack: Vec::new(),
            prim_store: PrimitiveStore::new(),
            clip_store: ClipStore::new(),
            screen_rect: DeviceUintRect::zero(),
            background_color: None,
            config: FrameBuilderConfig {
                enable_scrollbars: false,
                default_font_render_mode: FontRenderMode::Mono,
                debug: false,
            },
        }
    }

    pub fn recycle(
        self,
        screen_rect: DeviceUintRect,
        background_color: Option<ColorF>,
        config: FrameBuilderConfig,
    ) -> Self {
        FrameBuilder {
            hit_testing_runs: recycle_vec(self.hit_testing_runs),
            shadow_prim_stack: recycle_vec(self.shadow_prim_stack),
            pending_shadow_contents: recycle_vec(self.pending_shadow_contents),
            scrollbar_prims: recycle_vec(self.scrollbar_prims),
            reference_frame_stack: recycle_vec(self.reference_frame_stack),
            picture_stack: recycle_vec(self.picture_stack),
            sc_stack: recycle_vec(self.sc_stack),
            prim_store: self.prim_store.recycle(),
            clip_store: self.clip_store.recycle(),
            screen_rect,
            background_color,
            config,
        }
    }

    /// Create a primitive and add it to the prim store. This method doesn't
    /// add the primitive to the draw list, so can be used for creating
    /// sub-primitives.
    pub fn create_primitive(
        &mut self,
        info: &LayerPrimitiveInfo,
        mut clip_sources: Vec<ClipSource>,
        container: PrimitiveContainer,
    ) -> PrimitiveIndex {
        if let &LocalClip::RoundedRect(main, region) = &info.local_clip {
            clip_sources.push(ClipSource::Rectangle(main));
            clip_sources.push(ClipSource::RoundedRectangle(
                region.rect,
                region.radii,
                region.mode,
            ));
        }

        let stacking_context = self.sc_stack.last().expect("bug: no stacking context!");

        let clip_sources = self.clip_store.insert(ClipSources::new(clip_sources));
        let prim_index = self.prim_store.add_primitive(
            &info.rect,
            &info.local_clip.clip_rect(),
            info.is_backface_visible && stacking_context.is_backface_visible,
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
        // Add primitive to the top-most Picture on the stack.
        // TODO(gw): Let's consider removing the extra indirection
        //           needed to get a specific primitive index...
        let pic_prim_index = self.picture_stack.last().unwrap();
        let metadata = &self.prim_store.cpu_metadata[pic_prim_index.0];
        let pic = &mut self.prim_store.cpu_pictures[metadata.cpu_prim_index.0];
        pic.add_primitive(
            prim_index,
            clip_and_scroll
        );
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
        let prim_index = self.create_primitive(info, clip_sources, container);

        self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
        prim_index
    }

    pub fn push_stacking_context(
        &mut self,
        pipeline_id: PipelineId,
        composite_ops: CompositeOps,
        transform_style: TransformStyle,
        is_backface_visible: bool,
        is_pipeline_root: bool,
        clip_and_scroll: ClipAndScrollInfo,
        output_pipelines: &FastHashSet<PipelineId>,
    ) {
        // Construct the necessary set of Picture primitives
        // to draw this stacking context.
        let current_reference_frame_id = self.current_reference_frame_id();

        // An arbitrary large clip rect. For now, we don't
        // specify a clip specific to the stacking context.
        // However, now that they are represented as Picture
        // primitives, we can apply any kind of clip mask
        // to them, as for a normal primitive. This is needed
        // to correctly handle some CSS cases (see #1957).
        let max_clip = LayerRect::max_rect();

        // If there is no root picture, create one for the main framebuffer.
        if self.sc_stack.is_empty() {
            // Should be no pictures at all if the stack is empty...
            debug_assert!(self.prim_store.cpu_pictures.is_empty());
            debug_assert_eq!(transform_style, TransformStyle::Flat);

            // This picture stores primitive runs for items on the
            // main framebuffer.
            let pic = PicturePrimitive::new_image(
                None,
                false,
                pipeline_id,
                current_reference_frame_id,
                None,
            );

            // No clip sources needed for the main framebuffer.
            let clip_sources = self.clip_store.insert(ClipSources::new(Vec::new()));

            // Add root picture primitive. The provided layer rect
            // is zero, because we don't yet know the size of the
            // picture. Instead, this is calculated recursively
            // when we cull primitives.
            let prim_index = self.prim_store.add_primitive(
                &LayerRect::zero(),
                &max_clip,
                true,
                clip_sources,
                None,
                PrimitiveContainer::Picture(pic),
            );

            self.picture_stack.push(prim_index);
        } else if composite_ops.mix_blend_mode.is_some() && self.sc_stack.len() > 2 {
            // If we have a mix-blend-mode, and we aren't the primary framebuffer,
            // the stacking context needs to be isolated to blend correctly as per
            // the CSS spec.
            // TODO(gw): The way we detect not being the primary framebuffer (len > 2)
            //           is hacky and depends on how we create a root stacking context
            //           during flattening.
            let current_pic_prim_index = self.picture_stack.last().unwrap();
            let pic_cpu_prim_index = self.prim_store.cpu_metadata[current_pic_prim_index.0].cpu_prim_index;
            let parent_pic = &mut self.prim_store.cpu_pictures[pic_cpu_prim_index.0];

            match parent_pic.kind {
                PictureKind::Image { ref mut composite_mode, .. } => {
                    // If not already isolated for some other reason,
                    // make this picture as isolated.
                    if composite_mode.is_none() {
                        *composite_mode = Some(PictureCompositeMode::Blit);
                    }
                }
                PictureKind::TextShadow { .. } |
                PictureKind::BoxShadow { .. } => {
                    panic!("bug: text/box pictures invalid here");
                }
            }
        }

        // Get the transform-style of the parent stacking context,
        // which determines if we *might* need to draw this on
        // an intermediate surface for plane splitting purposes.
        let parent_transform_style = match self.sc_stack.last() {
            Some(sc) => sc.transform_style,
            None => TransformStyle::Flat,
        };

        // If either the parent or this stacking context is preserve-3d
        // then we are in a 3D context.
        let is_in_3d_context = composite_ops.count() == 0 &&
                               (parent_transform_style == TransformStyle::Preserve3D ||
                                transform_style == TransformStyle::Preserve3D);

        // TODO(gw): For now, we don't handle filters and mix-blend-mode when there
        //           is a 3D rendering context. We can easily do this in the future
        //           by creating a chain of pictures for the effects, and ensuring
        //           that the last composited picture is what's used as the input to
        //           the plane splitting code.
        let mut parent_pic_prim_index = if is_in_3d_context {
            // If we're in a 3D context, we will parent the picture
            // to the first stacking context we find in the stack that
            // is transform-style: flat. This follows the spec
            // by hoisting these items out into the same 3D context
            // for plane splitting.
            self.sc_stack
                .iter()
                .rev()
                .find(|sc| sc.transform_style == TransformStyle::Flat)
                .map(|sc| sc.pic_prim_index)
                .unwrap()
        } else {
            *self.picture_stack.last().unwrap()
        };

        // For each filter, create a new image with that composite mode.
        for filter in &composite_ops.filters {
            let src_prim = PicturePrimitive::new_image(
                Some(PictureCompositeMode::Filter(*filter)),
                false,
                pipeline_id,
                current_reference_frame_id,
                None,
            );
            let src_clip_sources = self.clip_store.insert(ClipSources::new(Vec::new()));

            let src_prim_index = self.prim_store.add_primitive(
                &LayerRect::zero(),
                &max_clip,
                is_backface_visible,
                src_clip_sources,
                None,
                PrimitiveContainer::Picture(src_prim),
            );

            let pic_prim_index = self.prim_store.cpu_metadata[parent_pic_prim_index.0].cpu_prim_index;
            parent_pic_prim_index = src_prim_index;
            let pic = &mut self.prim_store.cpu_pictures[pic_prim_index.0];
            pic.add_primitive(
                src_prim_index,
                clip_and_scroll,
            );

            self.picture_stack.push(src_prim_index);
        }

        // Same for mix-blend-mode.
        if let Some(mix_blend_mode) = composite_ops.mix_blend_mode {
            let src_prim = PicturePrimitive::new_image(
                Some(PictureCompositeMode::MixBlend(mix_blend_mode)),
                false,
                pipeline_id,
                current_reference_frame_id,
                None,
            );
            let src_clip_sources = self.clip_store.insert(ClipSources::new(Vec::new()));

            let src_prim_index = self.prim_store.add_primitive(
                &LayerRect::zero(),
                &max_clip,
                is_backface_visible,
                src_clip_sources,
                None,
                PrimitiveContainer::Picture(src_prim),
            );

            let pic_prim_index = self.prim_store.cpu_metadata[parent_pic_prim_index.0].cpu_prim_index;
            parent_pic_prim_index = src_prim_index;
            let pic = &mut self.prim_store.cpu_pictures[pic_prim_index.0];
            pic.add_primitive(
                src_prim_index,
                clip_and_scroll,
            );

            self.picture_stack.push(src_prim_index);
        }

        // By default, this picture will be collapsed into
        // the owning target.
        let mut composite_mode = None;
        let mut frame_output_pipeline_id = None;

        // If this stacking context if the root of a pipeline, and the caller
        // has requested it as an output frame, create a render task to isolate it.
        if is_pipeline_root && output_pipelines.contains(&pipeline_id) {
            composite_mode = Some(PictureCompositeMode::Blit);
            frame_output_pipeline_id = Some(pipeline_id);
        }

        if is_in_3d_context {
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
        let sc_prim = PicturePrimitive::new_image(
            composite_mode,
            is_in_3d_context,
            pipeline_id,
            current_reference_frame_id,
            frame_output_pipeline_id,
        );

        let sc_clip_sources = self.clip_store.insert(ClipSources::new(Vec::new()));
        let sc_prim_index = self.prim_store.add_primitive(
            &LayerRect::zero(),
            &max_clip,
            is_backface_visible,
            sc_clip_sources,
            None,
            PrimitiveContainer::Picture(sc_prim),
        );

        let pic_prim_index = self.prim_store.cpu_metadata[parent_pic_prim_index.0].cpu_prim_index;
        let sc_pic = &mut self.prim_store.cpu_pictures[pic_prim_index.0];
        sc_pic.add_primitive(
            sc_prim_index,
            clip_and_scroll,
        );

        // Add this as the top-most picture for primitives to be added to.
        self.picture_stack.push(sc_prim_index);

        // TODO(gw): This is super conservative. We can expand on this a lot
        //           once all the picture code is in place and landed.
        let allow_subpixel_aa = composite_ops.count() == 0 &&
                                transform_style == TransformStyle::Flat;

        // Push the SC onto the stack, so we know how to handle things in
        // pop_stacking_context.
        let sc = StackingContext {
            composite_ops,
            is_backface_visible,
            pipeline_id,
            allow_subpixel_aa,
            transform_style,
            // TODO(gw): This is not right when filters are present (but we
            //           don't handle that right now, per comment above).
            pic_prim_index: sc_prim_index,
        };

        self.sc_stack.push(sc);
    }

    pub fn pop_stacking_context(&mut self) {
        let sc = self.sc_stack.pop().unwrap();

        // Remove the picture for this stacking contents.
        self.picture_stack.pop().expect("bug");

        // Remove the picture for any filter/mix-blend-mode effects.
        for _ in 0 .. sc.composite_ops.count() {
            self.picture_stack.pop().expect("bug: mismatched picture stack");
        }

        // By the time the stacking context stack is empty, we should
        // also have cleared the picture stack.
        if self.sc_stack.is_empty() {
            self.picture_stack.pop().expect("bug: picture stack invalid");
            debug_assert!(self.picture_stack.is_empty());
        }

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
        source_transform: Option<PropertyBinding<LayoutTransform>>,
        source_perspective: Option<LayoutTransform>,
        origin_in_parent_reference_frame: LayerVector2D,
        root_for_pipeline: bool,
        clip_scroll_tree: &mut ClipScrollTree,
    ) -> ClipId {
        let new_id = clip_scroll_tree.add_reference_frame(
            rect,
            source_transform,
            source_perspective,
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
                info.resolved_transform = LayerToScrollTransform::create_translation(
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
        self.push_reference_frame(
            None,
            pipeline_id,
            &viewport_rect,
            None,
            None,
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
        let clip_rect = clip_region.main;
        let clip_sources = ClipSources::from(clip_region);
        debug_assert!(clip_sources.has_clips());

        let handle = self.clip_store.insert(clip_sources);

        let node = ClipScrollNode::new_clip_node(pipeline_id, parent_id, handle, clip_rect);
        clip_scroll_tree.add_node(node, new_node_id);
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
        let pipeline_id = self.sc_stack.last().unwrap().pipeline_id;
        let prim = PicturePrimitive::new_text_shadow(shadow, pipeline_id);

        // Create an empty shadow primitive. Insert it into
        // the draw lists immediately so that it will be drawn
        // before any visual text elements that are added as
        // part of this shadow context.
        let prim_index = self.create_primitive(
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
        for (_, pending_primitives) in shadows.drain(..) {
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
        content: RectangleContent,
        scrollbar_info: Option<ScrollbarInfo>,
    ) {
        if let RectangleContent::Fill(ColorF{a, ..}) = content {
            if a == 0.0 {
                // Don't add transparent rectangles to the draw list, but do consider them for hit
                // testing. This allows specifying invisible hit testing areas.
                self.add_primitive_to_hit_testing_list(info, clip_and_scroll);
                return;
            }
        }
        let prim = RectanglePrimitive {
            content,
            edge_aa_segment_mask: info.edge_aa_segment_mask,
        };

        let prim_index = self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Rectangle(prim),
        );

        if let Some(ScrollbarInfo(clip_id, frame_rect)) = scrollbar_info {
            self.scrollbar_prims.push(ScrollbarPrimitive {
                prim_index,
                clip_id,
                frame_rect,
            });
        }
    }

    pub fn add_line(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        info: &LayerPrimitiveInfo,
        wavy_line_thickness: f32,
        orientation: LineOrientation,
        line_color: &ColorF,
        style: LineStyle,
    ) {
        let line = LinePrimitive {
            wavy_line_thickness,
            color: line_color.premultiplied(),
            style,
            orientation,
        };

        let mut fast_shadow_prims = Vec::new();
        for (idx, &(shadow_prim_index, _)) in self.shadow_prim_stack.iter().enumerate() {
            let shadow_metadata = &self.prim_store.cpu_metadata[shadow_prim_index.0];
            let picture = &self.prim_store.cpu_pictures[shadow_metadata.cpu_prim_index.0];
            match picture.kind {
                PictureKind::TextShadow { offset, color, blur_radius, .. } if blur_radius == 0.0 => {
                    fast_shadow_prims.push((idx, offset, color));
                }
                _ => {}
            }
        }

        for (idx, shadow_offset, shadow_color) in fast_shadow_prims {
            let mut line = line.clone();
            line.color = shadow_color.premultiplied();
            let mut info = info.clone();
            info.rect = info.rect.translate(&shadow_offset);
            let prim_index = self.create_primitive(
                &info,
                Vec::new(),
                PrimitiveContainer::Line(line),
            );
            self.shadow_prim_stack[idx].1.push((prim_index, clip_and_scroll));
        }

        let prim_index = self.create_primitive(
            &info,
            Vec::new(),
            PrimitiveContainer::Line(line),
        );

        if line_color.a > 0.0 {
            if self.shadow_prim_stack.is_empty() {
                self.add_primitive_to_hit_testing_list(&info, clip_and_scroll);
                self.add_primitive_to_draw_list(prim_index, clip_and_scroll);
            } else {
                self.pending_shadow_contents.push((prim_index, clip_and_scroll, *info));
            }
        }

        for &(shadow_prim_index, _) in &self.shadow_prim_stack {
            let shadow_metadata = &mut self.prim_store.cpu_metadata[shadow_prim_index.0];
            debug_assert_eq!(shadow_metadata.prim_kind, PrimitiveKind::Picture);
            let picture =
                &mut self.prim_store.cpu_pictures[shadow_metadata.cpu_prim_index.0];

            match picture.kind {
                // Only run real blurs here (fast path zero blurs are handled above).
                PictureKind::TextShadow { blur_radius, .. } if blur_radius > 0.0 => {
                    picture.add_primitive(
                        prim_index,
                        clip_and_scroll,
                    );
                }
                _ => {}
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
        text_color: &ColorF,
        glyph_range: ItemRange<GlyphInstance>,
        glyph_count: usize,
        glyph_options: Option<GlyphOptions>,
    ) {
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
            // text on a picture that has filters
            // (e.g. opacity) can't use sub-pixel.
            // TODO(gw): It's possible we can relax this in
            //           the future, if we modify the way
            //           we handle subpixel blending.
            if let Some(ref stacking_context) = self.sc_stack.last() {
                if !stacking_context.allow_subpixel_aa {
                    render_mode = FontRenderMode::Alpha;
                }
            }
        }

        let prim_font = FontInstance::new(
            font.font_key,
            font.size,
            *text_color,
            font.bg_color,
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
            match picture_prim.kind {
                PictureKind::TextShadow { offset, color, blur_radius, .. } if blur_radius == 0.0 => {
                    let mut text_prim = prim.clone();
                    text_prim.font.color = color.into();
                    text_prim.offset += offset;
                    fast_shadow_prims.push((idx, text_prim));
                }
                _ => {}
            }
        }

        for (idx, text_prim) in fast_shadow_prims {
            let rect = info.rect;
            let mut info = info.clone();
            info.rect = rect.translate(&text_prim.offset);
            let prim_index = self.create_primitive(
                &info,
                Vec::new(),
                PrimitiveContainer::TextRun(text_prim),
            );
            self.shadow_prim_stack[idx].1.push((prim_index, clip_and_scroll));
        }

        // Create (and add to primitive store) the primitive that will be
        // used for both the visual element and also the shadow(s).
        let prim_index = self.create_primitive(
            info,
            Vec::new(),
            PrimitiveContainer::TextRun(prim),
        );

        // Only add a visual element if it can contribute to the scene.
        if text_color.a > 0.0 {
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
            let picture =
                &mut self.prim_store.cpu_pictures[shadow_metadata.cpu_prim_index.0];

            match picture.kind {
                // Only run real blurs here (fast path zero blurs are handled above).
                PictureKind::TextShadow { blur_radius, .. } if blur_radius > 0.0 => {
                    picture.add_primitive(
                        prim_index,
                        clip_and_scroll,
                    );
                }
                _ => {}
            }
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

        // If the tile spacing is the same as the rect size,
        // then it is effectively zero. We use this later on
        // in prim_store to detect if an image can be considered
        // opaque.
        let tile_spacing = if *tile_spacing == info.rect.size {
            LayerSize::zero()
        } else {
            *tile_spacing
        };

        let prim_cpu = ImagePrimitiveCpu {
            image_key,
            image_rendering,
            tile_offset: tile,
            tile_spacing,
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
        result
    }

    /// Compute the contribution (bounding rectangles, and resources) of layers and their
    /// primitives in screen space.
    fn build_layer_screen_rects_and_cull_layers(
        &mut self,
        clip_scroll_tree: &mut ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        profile_counters: &mut FrameProfileCounters,
        device_pixel_ratio: f32,
        scene_properties: &SceneProperties,
    ) -> Option<RenderTaskId> {
        profile_scope!("cull");

        if self.prim_store.cpu_pictures.is_empty() {
            return None
        }

        // The root picture is always the first one added.
        let prim_run_cmds = mem::replace(&mut self.prim_store.cpu_pictures[0].runs, Vec::new());
        let root_clip_scroll_node = &clip_scroll_tree.nodes[&clip_scroll_tree.root_reference_frame_id()];

        let display_list = &pipelines
            .get(&root_clip_scroll_node.pipeline_id)
            .expect("No display list?")
            .display_list;

        let root_prim_context = PrimitiveContext::new(
            device_pixel_ratio,
            display_list,
            root_clip_scroll_node,
            root_clip_scroll_node,
        );

        let mut child_tasks = Vec::new();
        self.prim_store.reset_prim_visibility();
        self.prim_store.prepare_prim_runs(
            &prim_run_cmds,
            root_clip_scroll_node.pipeline_id,
            gpu_cache,
            resource_cache,
            render_tasks,
            &mut self.clip_store,
            clip_scroll_tree,
            pipelines,
            &root_prim_context,
            true,
            &mut child_tasks,
            profile_counters,
            None,
            scene_properties,
            SpecificPrimitiveIndex(0),
            &self.screen_rect.to_i32(),
        );

        let pic = &mut self.prim_store.cpu_pictures[0];
        pic.runs = prim_run_cmds;

        let root_render_task = RenderTask::new_picture(
            None,
            PrimitiveIndex(0),
            RenderTargetKind::Color,
            0.0,
            0.0,
            PremultipliedColorF::TRANSPARENT,
            ClearMode::Transparent,
            RasterizationSpace::Screen,
            child_tasks,
        );

        pic.render_task_id = Some(render_tasks.add(root_render_task));
        pic.render_task_id
    }

    fn update_scroll_bars(&mut self, clip_scroll_tree: &ClipScrollTree, gpu_cache: &mut GpuCache) {
        static SCROLLBAR_PADDING: f32 = 8.0;

        for scrollbar_prim in &self.scrollbar_prims {
            let metadata = &mut self.prim_store.cpu_metadata[scrollbar_prim.prim_index.0];
            let scroll_frame = &clip_scroll_tree.nodes[&scrollbar_prim.clip_id];

            // Invalidate what's in the cache so it will get rebuilt.
            gpu_cache.invalidate(&metadata.gpu_location);

            let scrollable_distance = scroll_frame.scrollable_size().height;
            if scrollable_distance <= 0.0 {
                metadata.local_clip_rect.size = LayerSize::zero();
                continue;
            }
            let amount_scrolled = -scroll_frame.scroll_offset().y / scrollable_distance;

            let frame_rect = scrollbar_prim.frame_rect;
            let min_y = frame_rect.origin.y + SCROLLBAR_PADDING;
            let max_y = frame_rect.origin.y + frame_rect.size.height -
                (SCROLLBAR_PADDING + metadata.local_rect.size.height);

            metadata.local_rect.origin.x = frame_rect.origin.x + frame_rect.size.width -
                (metadata.local_rect.size.width + SCROLLBAR_PADDING);
            metadata.local_rect.origin.y = util::lerp(min_y, max_y, amount_scrolled);
            metadata.local_clip_rect = metadata.local_rect;
        }
    }

    pub fn build(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        frame_id: FrameId,
        clip_scroll_tree: &mut ClipScrollTree,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        window_size: DeviceUintSize,
        device_pixel_ratio: f32,
        layer: DocumentLayer,
        pan: LayerPoint,
        texture_cache_profile: &mut TextureCacheProfileCounters,
        gpu_cache_profile: &mut GpuCacheProfileCounters,
        scene_properties: &SceneProperties,
    ) -> Frame {
        profile_scope!("build");
        debug_assert!(
            DeviceUintRect::new(DeviceUintPoint::zero(), window_size)
                .contains_rect(&self.screen_rect)
        );

        let mut profile_counters = FrameProfileCounters::new();
        profile_counters
            .total_primitives
            .set(self.prim_store.prim_count());

        resource_cache.begin_frame(frame_id);
        gpu_cache.begin_frame();

        let mut node_data = Vec::new();
        clip_scroll_tree.update_tree(
            &self.screen_rect.to_i32(),
            device_pixel_ratio,
            &mut self.clip_store,
            resource_cache,
            gpu_cache,
            pan,
            &mut node_data,
            scene_properties,
        );

        self.update_scroll_bars(clip_scroll_tree, gpu_cache);

        let mut render_tasks = RenderTaskTree::new();

        let main_render_task_id = self.build_layer_screen_rects_and_cull_layers(
            clip_scroll_tree,
            pipelines,
            resource_cache,
            gpu_cache,
            &mut render_tasks,
            &mut profile_counters,
            device_pixel_ratio,
            scene_properties,
        );

        let mut passes = Vec::new();
        resource_cache.block_until_all_resources_added(gpu_cache, texture_cache_profile);

        if let Some(main_render_task_id) = main_render_task_id {
            let mut required_pass_count = 0;
            render_tasks.max_depth(main_render_task_id, 0, &mut required_pass_count);
            assert_ne!(required_pass_count, 0);

            // Do the allocations now, assigning each tile's tasks to a render
            // pass and target as required.
            for _ in 0 .. required_pass_count - 1 {
                passes.push(RenderPass::new_off_screen(self.screen_rect.size.to_i32()));
            }
            passes.push(RenderPass::new_main_framebuffer(self.screen_rect.size.to_i32()));

            render_tasks.assign_to_passes(
                main_render_task_id,
                required_pass_count - 1,
                &mut passes,
            );
        }

        let mut deferred_resolves = vec![];

        for pass in &mut passes {
            let ctx = RenderTargetContext {
                device_pixel_ratio,
                prim_store: &self.prim_store,
                resource_cache,
                node_data: &node_data,
                clip_scroll_tree,
            };

            pass.build(
                &ctx,
                gpu_cache,
                &mut render_tasks,
                &mut deferred_resolves,
                &self.clip_store,
            );

            profile_counters.passes.inc();

            match pass.kind {
                RenderPassKind::MainFramebuffer(_) => {
                    profile_counters.color_targets.add(1);
                }
                RenderPassKind::OffScreen { ref color, ref alpha } => {
                    profile_counters
                        .color_targets
                        .add(color.targets.len());
                    profile_counters
                        .alpha_targets
                        .add(alpha.targets.len());
                }
            }
        }

        let gpu_cache_updates = gpu_cache.end_frame(gpu_cache_profile);

        render_tasks.build();

        resource_cache.end_frame();

        Frame {
            window_size,
            inner_rect: self.screen_rect,
            device_pixel_ratio,
            background_color: self.background_color,
            layer,
            profile_counters,
            passes,
            node_data,
            render_tasks,
            deferred_resolves,
            gpu_cache_updates: Some(gpu_cache_updates),
        }
    }
}

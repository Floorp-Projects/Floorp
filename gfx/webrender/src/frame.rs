
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BuiltDisplayListIter, ClipAndScrollInfo, ClipId, ColorF, ComplexClipRegion};
use api::{DeviceUintRect, DeviceUintSize, DisplayItemRef, DocumentLayer, Epoch, FilterOp};
use api::{ImageDisplayItem, ItemRange, LayerPoint, LayerPrimitiveInfo, LayerRect};
use api::{LayerSize, LayerVector2D, LayoutSize};
use api::{LocalClip, PipelineId, ScrollClamping, ScrollEventPhase, ScrollLayerState};
use api::{ScrollLocation, ScrollPolicy, ScrollSensitivity, SpecificDisplayItem, StackingContext};
use api::{TileOffset, TransformStyle, WorldPoint};
use clip::ClipRegion;
use clip_scroll_node::StickyFrameInfo;
use clip_scroll_tree::{ClipScrollTree, ScrollStates};
use euclid::rect;
use frame_builder::{FrameBuilder, FrameBuilderConfig, ScrollbarInfo};
use gpu_cache::GpuCache;
use internal_types::{FastHashMap, FastHashSet, RenderedDocument};
use prim_store::{BrushAntiAliasMode};
use profiler::{GpuCacheProfileCounters, TextureCacheProfileCounters};
use resource_cache::{FontInstanceMap,ResourceCache, TiledImageMap};
use scene::{Scene, StackingContextHelpers, ScenePipeline, SceneProperties};
use tiling::CompositeOps;

#[derive(Copy, Clone, PartialEq, PartialOrd, Debug, Eq, Ord)]
pub struct FrameId(pub u32);

static DEFAULT_SCROLLBAR_COLOR: ColorF = ColorF {
    r: 0.3,
    g: 0.3,
    b: 0.3,
    a: 0.6,
};

struct FlattenContext<'a> {
    scene: &'a Scene,
    builder: FrameBuilder,
    clip_scroll_tree: &'a mut ClipScrollTree,
    font_instances: FontInstanceMap,
    tiled_image_map: TiledImageMap,
    pipeline_epochs: Vec<(PipelineId, Epoch)>,
    replacements: Vec<(ClipId, ClipId)>,
    output_pipelines: &'a FastHashSet<PipelineId>,
}

impl<'a> FlattenContext<'a> {
    /// Since WebRender still handles fixed position and reference frame content internally
    /// we need to apply this table of id replacements only to the id that affects the
    /// position of a node. We can eventually remove this when clients start handling
    /// reference frames themselves. This method applies these replacements.
    fn apply_scroll_frame_id_replacement(&self, id: ClipId) -> ClipId {
        match self.replacements.last() {
            Some(&(to_replace, replacement)) if to_replace == id => replacement,
            _ => id,
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

    fn flatten_root(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        frame_size: &LayoutSize,
        root_reference_frame_id: ClipId,
        root_scroll_frame_id: ClipId,
    ) {
        let clip_id = ClipId::root_scroll_node(pipeline_id);

        self.builder.push_stacking_context(
            pipeline_id,
            CompositeOps::default(),
            TransformStyle::Flat,
            true,
            true,
            ClipAndScrollInfo::simple(clip_id),
            self.output_pipelines,
        );

        // For the root pipeline, there's no need to add a full screen rectangle
        // here, as it's handled by the framebuffer clear.
        if self.scene.root_pipeline_id != Some(pipeline_id) {
            if let Some(pipeline) = self.scene.pipelines.get(&pipeline_id) {
                if let Some(bg_color) = pipeline.background_color {
                    let root_bounds = LayerRect::new(LayerPoint::zero(), *frame_size);
                    let info = LayerPrimitiveInfo::new(root_bounds);
                    self.builder.add_solid_rectangle(
                        ClipAndScrollInfo::simple(root_reference_frame_id),
                        &info,
                        bg_color,
                        None,
                        BrushAntiAliasMode::Primitive,
                    );
                }
            }
        }


        self.flatten_items(
            traversal,
            pipeline_id,
            LayerVector2D::zero(),
        );

        if self.builder.config.enable_scrollbars {
            let scrollbar_rect = LayerRect::new(LayerPoint::zero(), LayerSize::new(10.0, 70.0));
            let container_rect = LayerRect::new(LayerPoint::zero(), *frame_size);
            self.builder.add_scroll_bar(
                ClipAndScrollInfo::simple(root_reference_frame_id),
                &LayerPrimitiveInfo::new(scrollbar_rect),
                DEFAULT_SCROLLBAR_COLOR,
                ScrollbarInfo(root_scroll_frame_id, container_rect),
            );
        }

        self.builder.pop_stacking_context();
    }

    fn flatten_items(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        reference_frame_relative_offset: LayerVector2D,
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

    fn flatten_clip(
        &mut self,
        pipeline_id: PipelineId,
        parent_id: &ClipId,
        new_clip_id: &ClipId,
        clip_region: ClipRegion,
    ) {
        self.builder.add_clip_node(
            *new_clip_id,
            *parent_id,
            pipeline_id,
            clip_region,
            self.clip_scroll_tree,
        );
    }

    fn flatten_scroll_frame(
        &mut self,
        pipeline_id: PipelineId,
        parent_id: &ClipId,
        new_scroll_frame_id: &ClipId,
        frame_rect: &LayerRect,
        content_rect: &LayerRect,
        clip_region: ClipRegion,
        scroll_sensitivity: ScrollSensitivity,
    ) {
        let clip_id = self.clip_scroll_tree.generate_new_clip_id(pipeline_id);
        self.builder.add_clip_node(
            clip_id,
            *parent_id,
            pipeline_id,
            clip_region,
            self.clip_scroll_tree,
        );

        self.builder.add_scroll_frame(
            *new_scroll_frame_id,
            clip_id,
            pipeline_id,
            &frame_rect,
            &content_rect.size,
            scroll_sensitivity,
            self.clip_scroll_tree,
        );
    }

    fn flatten_stacking_context(
        &mut self,
        traversal: &mut BuiltDisplayListIter<'a>,
        pipeline_id: PipelineId,
        context_scroll_node_id: ClipId,
        mut reference_frame_relative_offset: LayerVector2D,
        bounds: &LayerRect,
        stacking_context: &StackingContext,
        filters: ItemRange<FilterOp>,
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
                stacking_context.filter_ops_for_compositing(
                    display_list,
                    filters,
                ),
                stacking_context.mix_blend_mode_for_compositing(),
            )
        };

        if stacking_context.scroll_policy == ScrollPolicy::Fixed {
            self.replacements.push((
                context_scroll_node_id,
                self.builder.current_reference_frame_id(),
            ));
        }

        // If we have a transformation, we establish a new reference frame. This means
        // that fixed position stacking contexts are positioned relative to us.
        let is_reference_frame =
            stacking_context.transform.is_some() || stacking_context.perspective.is_some();
        let origin = reference_frame_relative_offset + bounds.origin.to_vector();
        reference_frame_relative_offset = if is_reference_frame {
            let reference_frame_bounds = LayerRect::new(LayerPoint::zero(), bounds.size);
            let mut clip_id = self.apply_scroll_frame_id_replacement(context_scroll_node_id);
            clip_id = self.builder.push_reference_frame(
                Some(clip_id),
                pipeline_id,
                &reference_frame_bounds,
                stacking_context.transform,
                stacking_context.perspective,
                origin,
                false,
                self.clip_scroll_tree,
            );
            self.replacements.push((context_scroll_node_id, clip_id));
            LayerVector2D::zero()
        } else {
            origin
        };

        let sc_scroll_node_id = self.apply_scroll_frame_id_replacement(context_scroll_node_id);

        self.builder.push_stacking_context(
            pipeline_id,
            composition_operations,
            stacking_context.transform_style,
            is_backface_visible,
            false,
            ClipAndScrollInfo::simple(sc_scroll_node_id),
            self.output_pipelines,
        );

        self.flatten_items(
            traversal,
            pipeline_id,
            reference_frame_relative_offset,
        );

        if stacking_context.scroll_policy == ScrollPolicy::Fixed {
            self.replacements.pop();
        }

        if is_reference_frame {
            self.replacements.pop();
            self.builder.pop_reference_frame();
        }

        self.builder.pop_stacking_context();
    }

    fn flatten_iframe(
        &mut self,
        pipeline_id: PipelineId,
        parent_id: ClipId,
        bounds: &LayerRect,
        local_clip: &LocalClip,
        reference_frame_relative_offset: LayerVector2D,
    ) {
        let pipeline = match self.scene.pipelines.get(&pipeline_id) {
            Some(pipeline) => pipeline,
            None => return,
        };

        let clip_region = ClipRegion::create_for_clip_node_with_local_clip(
            local_clip,
            &reference_frame_relative_offset
        );
        let parent_pipeline_id = parent_id.pipeline_id();
        let clip_id = self.clip_scroll_tree
            .generate_new_clip_id(parent_pipeline_id);
        self.builder.add_clip_node(
            clip_id,
            parent_id,
            parent_pipeline_id,
            clip_region,
            self.clip_scroll_tree,
        );

        self.pipeline_epochs.push((pipeline_id, pipeline.epoch));

        let iframe_rect = LayerRect::new(LayerPoint::zero(), bounds.size);
        let origin = reference_frame_relative_offset + bounds.origin.to_vector();
        let iframe_reference_frame_id = self.builder.push_reference_frame(
            Some(clip_id),
            pipeline_id,
            &iframe_rect,
            None,
            None,
            origin,
            true,
            self.clip_scroll_tree,
        );

        self.builder.add_scroll_frame(
            ClipId::root_scroll_node(pipeline_id),
            iframe_reference_frame_id,
            pipeline_id,
            &iframe_rect,
            &pipeline.content_size,
            ScrollSensitivity::ScriptAndInputEvents,
            self.clip_scroll_tree,
        );

        self.flatten_root(
            &mut pipeline.display_list.iter(),
            pipeline_id,
            &iframe_rect.size,
            iframe_reference_frame_id,
            ClipId::root_scroll_node(pipeline_id),
        );

        self.builder.pop_reference_frame();
    }

    fn flatten_item<'b>(
        &'b mut self,
        item: DisplayItemRef<'a, 'b>,
        pipeline_id: PipelineId,
        reference_frame_relative_offset: LayerVector2D,
    ) -> Option<BuiltDisplayListIter<'a>> {
        let mut clip_and_scroll = item.clip_and_scroll();

        let unreplaced_scroll_id = clip_and_scroll.scroll_node_id;
        clip_and_scroll.scroll_node_id =
            self.apply_scroll_frame_id_replacement(clip_and_scroll.scroll_node_id);

        let prim_info = item.get_layer_primitive_info(&reference_frame_relative_offset);
        match *item.item() {
            SpecificDisplayItem::Image(ref info) => {
                match self.tiled_image_map.get(&info.image_key).cloned() {
                    Some(tiling) => {
                        // The image resource is tiled. We have to generate an image primitive
                        // for each tile.
                        self.decompose_image(
                            clip_and_scroll,
                            &prim_info,
                            info,
                            tiling.image_size,
                            tiling.tile_size as u32,
                        );
                    }
                    None => {
                        self.builder.add_image(
                            clip_and_scroll,
                            &prim_info,
                            &info.stretch_size,
                            &info.tile_spacing,
                            None,
                            info.image_key,
                            info.image_rendering,
                            None,
                        );
                    }
                }
            }
            SpecificDisplayItem::YuvImage(ref info) => {
                self.builder.add_yuv_image(
                    clip_and_scroll,
                    &prim_info,
                    info.yuv_data,
                    info.color_space,
                    info.image_rendering,
                );
            }
            SpecificDisplayItem::Text(ref text_info) => {
                let instance_map = self.font_instances
                    .read()
                    .unwrap();
                match instance_map.get(&text_info.font_key) {
                    Some(instance) => {
                        self.builder.add_text(
                            clip_and_scroll,
                            reference_frame_relative_offset,
                            &prim_info,
                            instance,
                            &text_info.color,
                            item.glyphs(),
                            item.display_list().get(item.glyphs()).count(),
                            text_info.glyph_options,
                        );
                    }
                    None => {
                        warn!("Unknown font instance key: {:?}", text_info.font_key);
                    }
                }
            }
            SpecificDisplayItem::Rectangle(ref info) => {
                self.builder.add_solid_rectangle(
                    clip_and_scroll,
                    &prim_info,
                    info.color,
                    None,
                    BrushAntiAliasMode::Primitive,
                );
            }
            SpecificDisplayItem::ClearRectangle => {
                self.builder.add_clear_rectangle(
                    clip_and_scroll,
                    &prim_info,
                );
            }
            SpecificDisplayItem::Line(ref info) => {
                self.builder.add_line(
                    clip_and_scroll,
                    &prim_info,
                    info.wavy_line_thickness,
                    info.orientation,
                    &info.color,
                    info.style,
                );
            }
            SpecificDisplayItem::Gradient(ref info) => {
                self.builder.add_gradient(
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
                self.builder.add_radial_gradient(
                    clip_and_scroll,
                    &prim_info,
                    info.gradient.start_center,
                    info.gradient.start_radius,
                    info.gradient.end_center,
                    info.gradient.end_radius,
                    info.gradient.ratio_xy,
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
                self.builder.add_box_shadow(
                    pipeline_id,
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
                self.builder.add_border(
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
                    unreplaced_scroll_id,
                    reference_frame_relative_offset,
                    &item.rect(),
                    &info.stacking_context,
                    item.filters(),
                    prim_info.is_backface_visible,
                );
                return Some(subtraversal);
            }
            SpecificDisplayItem::Iframe(ref info) => {
                self.flatten_iframe(
                    info.pipeline_id,
                    clip_and_scroll.scroll_node_id,
                    &item.rect(),
                    &item.local_clip(),
                    reference_frame_relative_offset,
                );
            }
            SpecificDisplayItem::Clip(ref info) => {
                let complex_clips = self.get_complex_clips(pipeline_id, item.complex_clip().0);
                let clip_region = ClipRegion::create_for_clip_node(
                    *item.local_clip().clip_rect(),
                    complex_clips,
                    info.image_mask,
                    &reference_frame_relative_offset,
                );
                self.flatten_clip(
                    pipeline_id,
                    &clip_and_scroll.scroll_node_id,
                    &info.id,
                    clip_region,
                );
            }
            SpecificDisplayItem::ScrollFrame(ref info) => {
                let complex_clips = self.get_complex_clips(pipeline_id, item.complex_clip().0);
                let clip_region = ClipRegion::create_for_clip_node(
                    *item.local_clip().clip_rect(),
                    complex_clips,
                    info.image_mask,
                    &reference_frame_relative_offset,
                );
                // Just use clip rectangle as the frame rect for this scroll frame.
                // This is useful when calculating scroll extents for the
                // ClipScrollNode::scroll(..) API as well as for properly setting sticky
                // positioning offsets.
                let frame_rect = item.local_clip()
                    .clip_rect()
                    .translate(&reference_frame_relative_offset);
                let content_rect = item.rect().translate(&reference_frame_relative_offset);
                self.flatten_scroll_frame(
                    pipeline_id,
                    &clip_and_scroll.scroll_node_id,
                    &info.id,
                    &frame_rect,
                    &content_rect,
                    clip_region,
                    info.scroll_sensitivity,
                );
            }
            SpecificDisplayItem::StickyFrame(ref info) => {
                let frame_rect = item.rect().translate(&reference_frame_relative_offset);
                let sticky_frame_info = StickyFrameInfo::new(
                    info.margins,
                    info.vertical_offset_bounds,
                    info.horizontal_offset_bounds,
                    info.previously_applied_offset,
                );
                self.clip_scroll_tree.add_sticky_frame(
                    info.id,
                    clip_and_scroll.scroll_node_id, /* parent id */
                    frame_rect,
                    sticky_frame_info
                );
            }

            // Do nothing; these are dummy items for the display list parser
            SpecificDisplayItem::SetGradientStops => {}

            SpecificDisplayItem::PopStackingContext => {
                unreachable!("Should have returned in parent method.")
            }
            SpecificDisplayItem::PushShadow(shadow) => {
                let mut prim_info = prim_info.clone();
                prim_info.rect = LayerRect::zero();
                self.builder
                    .push_shadow(shadow, clip_and_scroll, &prim_info);
            }
            SpecificDisplayItem::PopAllShadows => {
                self.builder.pop_all_shadows();
            }
        }
        None
    }

    /// Decomposes an image display item that is repeated into an image per individual repetition.
    /// We need to do this when we are unable to perform the repetition in the shader,
    /// for example if the image is tiled.
    ///
    /// In all of the "decompose" methods below, we independently handle horizontal and vertical
    /// decomposition. This lets us generate the minimum amount of primitives by, for  example,
    /// decompositing the repetition horizontally while repeating vertically in the shader (for
    /// an image where the width is too bug but the height is not).
    ///
    /// decompose_image and decompose_image_row handle image repetitions while decompose_tiled_image
    /// takes care of the decomposition required by the internal tiling of the image.
    fn decompose_image(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        prim_info: &LayerPrimitiveInfo,
        info: &ImageDisplayItem,
        image_size: DeviceUintSize,
        tile_size: u32,
    ) {
        let no_vertical_tiling = image_size.height <= tile_size;
        let no_vertical_spacing = info.tile_spacing.height == 0.0;
        let item_rect = prim_info.rect;
        if no_vertical_tiling && no_vertical_spacing {
            self.decompose_image_row(
                clip_and_scroll,
                prim_info,
                info,
                image_size,
                tile_size,
            );
            return;
        }

        // Decompose each vertical repetition into rows.
        let layout_stride = info.stretch_size.height + info.tile_spacing.height;
        let num_repetitions = (item_rect.size.height / layout_stride).ceil() as u32;
        for i in 0 .. num_repetitions {
            if let Some(row_rect) = rect(
                item_rect.origin.x,
                item_rect.origin.y + (i as f32) * layout_stride,
                item_rect.size.width,
                info.stretch_size.height,
            ).intersection(&item_rect)
            {
                let mut prim_info = prim_info.clone();
                prim_info.rect = row_rect;
                self.decompose_image_row(
                    clip_and_scroll,
                    &prim_info,
                    info,
                    image_size,
                    tile_size,
                );
            }
        }
    }

    fn decompose_image_row(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        prim_info: &LayerPrimitiveInfo,
        info: &ImageDisplayItem,
        image_size: DeviceUintSize,
        tile_size: u32,
    ) {
        let no_horizontal_tiling = image_size.width <= tile_size;
        let no_horizontal_spacing = info.tile_spacing.width == 0.0;
        if no_horizontal_tiling && no_horizontal_spacing {
            self.decompose_tiled_image(
                clip_and_scroll,
                prim_info,
                info,
                image_size,
                tile_size,
            );
            return;
        }

        // Decompose each horizontal repetition.
        let item_rect = prim_info.rect;
        let layout_stride = info.stretch_size.width + info.tile_spacing.width;
        let num_repetitions = (item_rect.size.width / layout_stride).ceil() as u32;
        for i in 0 .. num_repetitions {
            if let Some(decomposed_rect) = rect(
                item_rect.origin.x + (i as f32) * layout_stride,
                item_rect.origin.y,
                info.stretch_size.width,
                item_rect.size.height,
            ).intersection(&item_rect)
            {
                let mut prim_info = prim_info.clone();
                prim_info.rect = decomposed_rect;
                self.decompose_tiled_image(
                    clip_and_scroll,
                    &prim_info,
                    info,
                    image_size,
                    tile_size,
                );
            }
        }
    }

    fn decompose_tiled_image(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        prim_info: &LayerPrimitiveInfo,
        info: &ImageDisplayItem,
        image_size: DeviceUintSize,
        tile_size: u32,
    ) {
        // The image resource is tiled. We have to generate an image primitive
        // for each tile.
        // We need to do this because the image is broken up into smaller tiles in the texture
        // cache and the image shader is not able to work with this type of sparse representation.

        // The tiling logic works as follows:
        //
        //  ###################-+  -+
        //  #    |    |    |//# |   | image size
        //  #    |    |    |//# |   |
        //  #----+----+----+--#-+   |  -+
        //  #    |    |    |//# |   |   | regular tile size
        //  #    |    |    |//# |   |   |
        //  #----+----+----+--#-+   |  -+-+
        //  #////|////|////|//# |   |     | "leftover" height
        //  ################### |  -+  ---+
        //  #----+----+----+----+
        //
        // In the ascii diagram above, a large image is plit into tiles of almost regular size.
        // The tiles on the right and bottom edges (hatched in the diagram) are smaller than
        // the regular tiles and are handled separately in the code see leftover_width/height.
        // each generated image primitive corresponds to a tile in the texture cache, with the
        // assumption that the smaller tiles with leftover sizes are sized to fit their own
        // irregular size in the texture cache.
        //
        // For the case where we don't tile along an axis, we can still perform the repetition in
        // the shader (for this particular axis), and it is worth special-casing for this to avoid
        // generating many primitives.
        // This can happen with very tall and thin images used as a repeating background.
        // Apparently web authors do that...

        let item_rect = prim_info.rect;
        let needs_repeat_x = info.stretch_size.width < item_rect.size.width;
        let needs_repeat_y = info.stretch_size.height < item_rect.size.height;

        let tiled_in_x = image_size.width > tile_size;
        let tiled_in_y = image_size.height > tile_size;

        // If we don't actually tile in this dimension, repeating can be done in the shader.
        let shader_repeat_x = needs_repeat_x && !tiled_in_x;
        let shader_repeat_y = needs_repeat_y && !tiled_in_y;

        let tile_size_f32 = tile_size as f32;

        // Note: this rounds down so it excludes the partially filled tiles on the right and
        // bottom edges (we handle them separately below).
        let num_tiles_x = (image_size.width / tile_size) as u16;
        let num_tiles_y = (image_size.height / tile_size) as u16;

        // Ratio between (image space) tile size and image size.
        let img_dw = tile_size_f32 / (image_size.width as f32);
        let img_dh = tile_size_f32 / (image_size.height as f32);

        // Strected size of the tile in layout space.
        let stretched_tile_size = LayerSize::new(
            img_dw * info.stretch_size.width,
            img_dh * info.stretch_size.height,
        );

        // The size in pixels of the tiles on the right and bottom edges, smaller
        // than the regular tile size if the image is not a multiple of the tile size.
        // Zero means the image size is a multiple of the tile size.
        let leftover =
            DeviceUintSize::new(image_size.width % tile_size, image_size.height % tile_size);

        for ty in 0 .. num_tiles_y {
            for tx in 0 .. num_tiles_x {
                self.add_tile_primitive(
                    clip_and_scroll,
                    prim_info,
                    info,
                    TileOffset::new(tx, ty),
                    stretched_tile_size,
                    1.0,
                    1.0,
                    shader_repeat_x,
                    shader_repeat_y,
                );
            }
            if leftover.width != 0 {
                // Tiles on the right edge that are smaller than the tile size.
                self.add_tile_primitive(
                    clip_and_scroll,
                    prim_info,
                    info,
                    TileOffset::new(num_tiles_x, ty),
                    stretched_tile_size,
                    (leftover.width as f32) / tile_size_f32,
                    1.0,
                    shader_repeat_x,
                    shader_repeat_y,
                );
            }
        }

        if leftover.height != 0 {
            for tx in 0 .. num_tiles_x {
                // Tiles on the bottom edge that are smaller than the tile size.
                self.add_tile_primitive(
                    clip_and_scroll,
                    prim_info,
                    info,
                    TileOffset::new(tx, num_tiles_y),
                    stretched_tile_size,
                    1.0,
                    (leftover.height as f32) / tile_size_f32,
                    shader_repeat_x,
                    shader_repeat_y,
                );
            }

            if leftover.width != 0 {
                // Finally, the bottom-right tile with a "leftover" size.
                self.add_tile_primitive(
                    clip_and_scroll,
                    prim_info,
                    info,
                    TileOffset::new(num_tiles_x, num_tiles_y),
                    stretched_tile_size,
                    (leftover.width as f32) / tile_size_f32,
                    (leftover.height as f32) / tile_size_f32,
                    shader_repeat_x,
                    shader_repeat_y,
                );
            }
        }
    }

    fn add_tile_primitive(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        prim_info: &LayerPrimitiveInfo,
        info: &ImageDisplayItem,
        tile_offset: TileOffset,
        stretched_tile_size: LayerSize,
        tile_ratio_width: f32,
        tile_ratio_height: f32,
        shader_repeat_x: bool,
        shader_repeat_y: bool,
    ) {
        // If the the image is tiled along a given axis, we can't have the shader compute
        // the image repetition pattern. In this case we base the primitive's rectangle size
        // on the stretched tile size which effectively cancels the repetion (and repetition
        // has to be emulated by generating more primitives).
        // If the image is not tiled along this axis, we can perform the repetition in the
        // shader. in this case we use the item's size in the primitive (on that particular
        // axis).
        // See the shader_repeat_x/y code below.

        let stretched_size = LayerSize::new(
            stretched_tile_size.width * tile_ratio_width,
            stretched_tile_size.height * tile_ratio_height,
        );

        let mut prim_rect = LayerRect::new(
            prim_info.rect.origin +
                LayerVector2D::new(
                    tile_offset.x as f32 * stretched_tile_size.width,
                    tile_offset.y as f32 * stretched_tile_size.height,
                ),
            stretched_size,
        );

        if shader_repeat_x {
            assert_eq!(tile_offset.x, 0);
            prim_rect.size.width = prim_info.rect.size.width;
        }

        if shader_repeat_y {
            assert_eq!(tile_offset.y, 0);
            prim_rect.size.height = prim_info.rect.size.height;
        }

        // Fix up the primitive's rect if it overflows the original item rect.
        if let Some(prim_rect) = prim_rect.intersection(&prim_info.rect) {
            let mut prim_info = prim_info.clone();
            prim_info.rect = prim_rect;
            self.builder.add_image(
                clip_and_scroll,
                &prim_info,
                &stretched_size,
                &info.tile_spacing,
                None,
                info.image_key,
                info.image_rendering,
                Some(tile_offset),
            );
        }
    }
}

/// Frame context contains the information required to update
/// (e.g. scroll) a renderer frame builder (`FrameBuilder`).
pub struct FrameContext {
    window_size: DeviceUintSize,
    clip_scroll_tree: ClipScrollTree,
    pipeline_epoch_map: FastHashMap<PipelineId, Epoch>,
    id: FrameId,
    frame_builder_config: FrameBuilderConfig,
}

impl FrameContext {
    pub fn new(config: FrameBuilderConfig) -> Self {
        FrameContext {
            window_size: DeviceUintSize::zero(),
            pipeline_epoch_map: FastHashMap::default(),
            clip_scroll_tree: ClipScrollTree::new(),
            id: FrameId(0),
            frame_builder_config: config,
        }
    }

    pub fn reset(&mut self) -> ScrollStates {
        self.pipeline_epoch_map.clear();

        // Advance to the next frame.
        self.id.0 += 1;

        self.clip_scroll_tree.drain()
    }

    pub fn get_clip_scroll_tree(&self) -> &ClipScrollTree {
        &self.clip_scroll_tree
    }

    pub fn get_scroll_node_state(&self) -> Vec<ScrollLayerState> {
        self.clip_scroll_tree.get_scroll_node_state()
    }

    /// Returns true if the node actually changed position or false otherwise.
    pub fn scroll_node(&mut self, origin: LayerPoint, id: ClipId, clamp: ScrollClamping) -> bool {
        self.clip_scroll_tree.scroll_node(origin, id, clamp)
    }

    /// Returns true if any nodes actually changed position or false otherwise.
    pub fn scroll(
        &mut self,
        scroll_location: ScrollLocation,
        cursor: WorldPoint,
        phase: ScrollEventPhase,
    ) -> bool {
        self.clip_scroll_tree.scroll(scroll_location, cursor, phase)
    }

    pub fn tick_scrolling_bounce_animations(&mut self) {
        self.clip_scroll_tree.tick_scrolling_bounce_animations();
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.clip_scroll_tree
            .discard_frame_state_for_pipeline(pipeline_id);
    }

    pub fn create(
        &mut self,
        old_builder: FrameBuilder,
        scene: &Scene,
        resource_cache: &mut ResourceCache,
        window_size: DeviceUintSize,
        inner_rect: DeviceUintRect,
        device_pixel_ratio: f32,
        output_pipelines: &FastHashSet<PipelineId>,
    ) -> FrameBuilder {
        let root_pipeline_id = match scene.root_pipeline_id {
            Some(root_pipeline_id) => root_pipeline_id,
            None => return old_builder,
        };

        let root_pipeline = match scene.pipelines.get(&root_pipeline_id) {
            Some(root_pipeline) => root_pipeline,
            None => return old_builder,
        };

        if window_size.width == 0 || window_size.height == 0 {
            error!("ERROR: Invalid window dimensions! Please call api.set_window_size()");
        }
        self.window_size = window_size;

        let old_scrolling_states = self.reset();

        self.pipeline_epoch_map
            .insert(root_pipeline_id, root_pipeline.epoch);

        let background_color = root_pipeline
            .background_color
            .and_then(|color| if color.a > 0.0 { Some(color) } else { None });

        let frame_builder = {
            let mut roller = FlattenContext {
                scene,
                builder: old_builder.recycle(
                    inner_rect,
                    background_color,
                    self.frame_builder_config,
                ),
                clip_scroll_tree: &mut self.clip_scroll_tree,
                font_instances: resource_cache.get_font_instances(),
                tiled_image_map: resource_cache.get_tiled_image_map(),
                pipeline_epochs: Vec::new(),
                replacements: Vec::new(),
                output_pipelines,
            };

            roller.builder.push_root(
                root_pipeline_id,
                &root_pipeline.viewport_size,
                &root_pipeline.content_size,
                roller.clip_scroll_tree,
            );

            roller.builder.setup_viewport_offset(
                window_size,
                inner_rect,
                device_pixel_ratio,
                roller.clip_scroll_tree,
            );

            let reference_frame_id = roller.clip_scroll_tree.root_reference_frame_id;
            let scroll_frame_id = roller.clip_scroll_tree.topmost_scrolling_node_id;
            roller.flatten_root(
                &mut root_pipeline.display_list.iter(),
                root_pipeline_id,
                &root_pipeline.viewport_size,
                reference_frame_id,
                scroll_frame_id,
            );

            debug_assert!(roller.builder.picture_stack.is_empty());

            self.pipeline_epoch_map.extend(roller.pipeline_epochs.drain(..));
            roller.builder
        };

        self.clip_scroll_tree
            .finalize_and_apply_pending_scroll_offsets(old_scrolling_states);
        frame_builder
    }

    pub fn update_epoch(&mut self, pipeline_id: PipelineId, epoch: Epoch) {
        self.pipeline_epoch_map.insert(pipeline_id, epoch);
    }

    pub fn build_rendered_document(
        &mut self,
        frame_builder: &mut FrameBuilder,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        pipelines: &FastHashMap<PipelineId, ScenePipeline>,
        device_pixel_ratio: f32,
        layer: DocumentLayer,
        pan: LayerPoint,
        texture_cache_profile: &mut TextureCacheProfileCounters,
        gpu_cache_profile: &mut GpuCacheProfileCounters,
		scene_properties: &SceneProperties,
    ) -> RenderedDocument {
        let frame = frame_builder.build(
            resource_cache,
            gpu_cache,
            self.id,
            &mut self.clip_scroll_tree,
            pipelines,
            self.window_size,
            device_pixel_ratio,
            layer,
            pan,
            texture_cache_profile,
            gpu_cache_profile,
            scene_properties,
        );

        let nodes_bouncing_back = self.clip_scroll_tree.collect_nodes_bouncing_back();
        RenderedDocument::new(self.pipeline_epoch_map.clone(), nodes_bouncing_back, frame)
    }
}

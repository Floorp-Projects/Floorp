/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ClipId, DeviceIntRect, LayerPixel, LayerPoint, LayerRect, LayerSize};
use api::{LayerToScrollTransform, LayerToWorldTransform, LayerVector2D, LayoutVector2D, PipelineId};
use api::{ScrollClamping, ScrollEventPhase, ScrollLocation, ScrollSensitivity};
use api::{LayoutTransform, PropertyBinding, StickyOffsetBounds, WorldPoint};
use clip::{ClipSourcesHandle, ClipStore};
use clip_scroll_tree::{CoordinateSystemId, TransformUpdateState};
use euclid::SideOffsets2D;
use geometry::ray_intersects_rect;
use gpu_cache::GpuCache;
use gpu_types::{ClipScrollNodeIndex, ClipScrollNodeData};
use render_task::{ClipChain, ClipChainNode, ClipWorkItem};
use resource_cache::ResourceCache;
use scene::SceneProperties;
use spring::{DAMPING, STIFFNESS, Spring};
use std::rc::Rc;
use util::{MatrixHelpers, MaxRect, TransformedRectKind};

#[cfg(target_os = "macos")]
const CAN_OVERSCROLL: bool = true;

#[cfg(not(target_os = "macos"))]
const CAN_OVERSCROLL: bool = false;

#[derive(Debug)]
pub struct StickyFrameInfo {
    pub margins: SideOffsets2D<Option<f32>>,
    pub vertical_offset_bounds: StickyOffsetBounds,
    pub horizontal_offset_bounds: StickyOffsetBounds,
    pub previously_applied_offset: LayoutVector2D,
    pub current_offset: LayerVector2D,
}

impl StickyFrameInfo {
    pub fn new(
        margins: SideOffsets2D<Option<f32>>,
        vertical_offset_bounds: StickyOffsetBounds,
        horizontal_offset_bounds: StickyOffsetBounds,
        previously_applied_offset: LayoutVector2D
    ) -> StickyFrameInfo {
        StickyFrameInfo {
            margins,
            vertical_offset_bounds,
            horizontal_offset_bounds,
            previously_applied_offset,
            current_offset: LayerVector2D::zero(),
        }
    }
}

#[derive(Debug)]
pub enum NodeType {
    /// A reference frame establishes a new coordinate space in the tree.
    ReferenceFrame(ReferenceFrameInfo),

    /// Other nodes just do clipping, but no transformation.
    Clip(ClipSourcesHandle),

    /// Transforms it's content, but doesn't clip it. Can also be adjusted
    /// by scroll events or setting scroll offsets.
    ScrollFrame(ScrollingState),

    /// A special kind of node that adjusts its position based on the position
    /// of its parent node and a given set of sticky positioning offset bounds.
    /// Sticky positioned is described in the CSS Positioned Layout Module Level 3 here:
    /// https://www.w3.org/TR/css-position-3/#sticky-pos
    StickyFrame(StickyFrameInfo),
}

impl NodeType {
    fn is_reference_frame(&self) -> bool {
        match *self {
            NodeType::ReferenceFrame(_) => true,
            _ => false,
        }
    }
}

/// Contains information common among all types of ClipScrollTree nodes.
#[derive(Debug)]
pub struct ClipScrollNode {
    /// Viewing rectangle in the coordinate system of the parent reference frame.
    pub local_viewport_rect: LayerRect,

    /// Clip rect of this node - typically the same as viewport rect, except
    /// in overscroll cases.
    pub local_clip_rect: LayerRect,

    /// The transformation for this viewport in world coordinates is the transformation for
    /// our parent reference frame, plus any accumulated scrolling offsets from nodes
    /// between our reference frame and this node. For reference frames, we also include
    /// whatever local transformation this reference frame provides. This can be combined
    /// with the local_viewport_rect to get its position in world space.
    pub world_viewport_transform: LayerToWorldTransform,

    /// World transform for content transformed by this node.
    pub world_content_transform: LayerToWorldTransform,

    /// Pipeline that this layer belongs to
    pub pipeline_id: PipelineId,

    /// Parent layer. If this is None, we are the root node.
    pub parent: Option<ClipId>,

    /// Child layers
    pub children: Vec<ClipId>,

    /// The type of this node and any data associated with that node type.
    pub node_type: NodeType,

    /// The node in the chain of clips that are necessary to clip display items
    /// that have this ClipScrollNode as their clip parent. This will be used to
    /// generate clip tasks.
    pub clip_chain_node: ClipChain,

    /// The intersected outer bounds of the clips for this node.
    pub combined_clip_outer_bounds: DeviceIntRect,

    /// The axis-aligned coordinate system id of this node.
    pub coordinate_system_id: CoordinateSystemId,

    /// A linear ID / index of this clip-scroll node. Used as a reference to
    /// pass to shaders, to allow them to fetch a given clip-scroll node.
    pub node_data_index: ClipScrollNodeIndex,
}

impl ClipScrollNode {
    fn new(
        pipeline_id: PipelineId,
        parent_id: Option<ClipId>,
        rect: &LayerRect,
        node_type: NodeType
    ) -> Self {
        ClipScrollNode {
            local_viewport_rect: *rect,
            local_clip_rect: *rect,
            world_viewport_transform: LayerToWorldTransform::identity(),
            world_content_transform: LayerToWorldTransform::identity(),
            parent: parent_id,
            children: Vec::new(),
            pipeline_id,
            node_type: node_type,
            clip_chain_node: None,
            combined_clip_outer_bounds: DeviceIntRect::max_rect(),
            coordinate_system_id: CoordinateSystemId(0),
            node_data_index: ClipScrollNodeIndex(0),
        }
    }

    pub fn new_scroll_frame(
        pipeline_id: PipelineId,
        parent_id: ClipId,
        frame_rect: &LayerRect,
        content_size: &LayerSize,
        scroll_sensitivity: ScrollSensitivity,
    ) -> Self {
        let node_type = NodeType::ScrollFrame(ScrollingState::new(
            scroll_sensitivity,
            LayerSize::new(
                (content_size.width - frame_rect.size.width).max(0.0),
                (content_size.height - frame_rect.size.height).max(0.0)
            )
        ));

        Self::new(pipeline_id, Some(parent_id), frame_rect, node_type)
    }

    pub fn new_clip_node(
        pipeline_id: PipelineId,
        parent_id: ClipId,
        handle: ClipSourcesHandle,
        clip_rect: LayerRect,
    ) -> Self {
        Self::new(pipeline_id, Some(parent_id), &clip_rect, NodeType::Clip(handle))
    }

    pub fn new_reference_frame(
        parent_id: Option<ClipId>,
        frame_rect: &LayerRect,
        source_transform: Option<PropertyBinding<LayoutTransform>>,
        source_perspective: Option<LayoutTransform>,
        origin_in_parent_reference_frame: LayerVector2D,
        pipeline_id: PipelineId,
    ) -> Self {
        let identity = LayoutTransform::identity();
        let info = ReferenceFrameInfo {
            resolved_transform: LayerToScrollTransform::identity(),
            source_transform: source_transform.unwrap_or(PropertyBinding::Value(identity)),
            source_perspective: source_perspective.unwrap_or(identity),
            origin_in_parent_reference_frame,
        };
        Self::new(pipeline_id, parent_id, frame_rect, NodeType::ReferenceFrame(info))
    }

    pub fn new_sticky_frame(
        parent_id: ClipId,
        frame_rect: LayerRect,
        sticky_frame_info: StickyFrameInfo,
        pipeline_id: PipelineId,
    ) -> Self {
        let node_type = NodeType::StickyFrame(sticky_frame_info);
        Self::new(pipeline_id, Some(parent_id), &frame_rect, node_type)
    }


    pub fn add_child(&mut self, child: ClipId) {
        self.children.push(child);
    }

    pub fn apply_old_scrolling_state(&mut self, old_scrolling_state: &ScrollingState) {
        match self.node_type {
            NodeType::ScrollFrame(ref mut scrolling) => {
                let scroll_sensitivity = scrolling.scroll_sensitivity;
                let scrollable_size = scrolling.scrollable_size;
                *scrolling = *old_scrolling_state;
                scrolling.scroll_sensitivity = scroll_sensitivity;
                scrolling.scrollable_size = scrollable_size;
            }
            _ if old_scrolling_state.offset != LayerVector2D::zero() => {
                warn!("Tried to scroll a non-scroll node.")
            }
            _ => {}
        }
    }

    pub fn set_scroll_origin(&mut self, origin: &LayerPoint, clamp: ScrollClamping) -> bool {
        let scrollable_size = self.scrollable_size();
        let scrollable_width = scrollable_size.width;
        let scrollable_height = scrollable_size.height;

        let scrolling = match self.node_type {
            NodeType::ScrollFrame(ref mut scrolling) => scrolling,
            _ => {
                warn!("Tried to scroll a non-scroll node.");
                return false;
            }
        };

        let new_offset = match clamp {
            ScrollClamping::ToContentBounds => {
                if scrollable_height <= 0. && scrollable_width <= 0. {
                    return false;
                }

                let origin = LayerPoint::new(origin.x.max(0.0), origin.y.max(0.0));
                LayerVector2D::new(
                    (-origin.x).max(-scrollable_width).min(0.0).round(),
                    (-origin.y).max(-scrollable_height).min(0.0).round(),
                )
            }
            ScrollClamping::NoClamping => LayerPoint::zero() - *origin,
        };

        if new_offset == scrolling.offset {
            return false;
        }

        scrolling.offset = new_offset;
        scrolling.bouncing_back = false;
        scrolling.started_bouncing_back = false;
        true
    }

    pub fn update_to_empty_rect(&mut self) {
        self.combined_clip_outer_bounds = DeviceIntRect::zero();
        self.world_content_transform = LayerToWorldTransform::identity();
        self.world_viewport_transform = LayerToWorldTransform::identity();
        self.clip_chain_node = None;
    }

    pub fn push_gpu_node_data(
        &mut self,
        state: &TransformUpdateState,
        node_data: &mut Vec<ClipScrollNodeData>
    ) {
        if self.combined_clip_outer_bounds.is_empty() {
            node_data.push(ClipScrollNodeData::invalid());
            return;
        }

        let local_clip_rect = match self.node_type {
            _ if self.world_content_transform.has_perspective_component() => LayerRect::max_rect(),
            NodeType::ReferenceFrame(ref info) => {
                info.resolved_transform.with_destination::<LayerPixel>()
                    .inverse_rect_footprint(&state.parent_combined_viewport_rect)
            }
            NodeType::Clip(_) | NodeType::ScrollFrame(_) => {
                state.parent_combined_viewport_rect
                    .intersection(&self.local_clip_rect)
                    .unwrap_or(LayerRect::zero())
            }
            NodeType::StickyFrame(ref sticky_info) => {
                state.parent_combined_viewport_rect
                    .translate(&-sticky_info.current_offset)
                    .intersection(&self.local_clip_rect)
                    .unwrap_or(LayerRect::zero())
            }
        };

        let transform_kind = if self.world_content_transform.preserves_2d_axis_alignment() {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        };

        let reference_frame_relative_scroll_offset = match self.node_type {
            NodeType::ReferenceFrame(_) => LayerVector2D::zero(),
            NodeType::Clip(_) | NodeType::ScrollFrame(_) => state.parent_accumulated_scroll_offset,
            NodeType::StickyFrame(ref sticky_info) =>
                    state.parent_accumulated_scroll_offset + sticky_info.current_offset,
        };

        let data = ClipScrollNodeData {
            transform: self.world_content_transform,
            local_clip_rect,
            reference_frame_relative_scroll_offset,
            scroll_offset: self.scroll_offset(),
            transform_kind: transform_kind as u32 as f32,
            padding: [0.0; 3],
        };

        // Write the data that will be made available to the GPU for this node.
        node_data.push(data);
    }

    pub fn update(
        &mut self,
        state: &mut TransformUpdateState,
        device_pixel_ratio: f32,
        clip_store: &mut ClipStore,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        scene_properties: &SceneProperties,
    ) {
        // If any of our parents was not rendered, we are not rendered either and can just
        // quit here.
        if state.combined_outer_clip_bounds.is_empty() {
            self.update_to_empty_rect();
            return;
        }

        // If this node is a reference frame, we check if the determinant is 0, which means it
        // has a non-invertible matrix. For non-reference-frames we assume that they will
        // produce only additional translations which should be invertible.
        if self.node_type.is_reference_frame() {
            if self.world_content_transform.determinant() == 0.0 {
                self.update_to_empty_rect();
                return;
            }
        }

        self.update_transform(state, scene_properties);
        self.update_clip_work_item(
            state,
            device_pixel_ratio,
            clip_store,
            resource_cache,
            gpu_cache,
        );

        // This indicates that we are entirely clipped out.
        if state.combined_outer_clip_bounds.is_empty() {
            self.update_to_empty_rect();
            return;
        }

    }

    pub fn update_clip_work_item(
        &mut self,
        state: &mut TransformUpdateState,
        device_pixel_ratio: f32,
        clip_store: &mut ClipStore,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
    ) {
        let mut current_clip_chain = state.parent_clip_chain.clone();
        let clip_sources_handle = match self.node_type {
            NodeType::Clip(ref handle) => handle,
            _ => {
                self.clip_chain_node = current_clip_chain;
                self.combined_clip_outer_bounds = state.combined_outer_clip_bounds;
                return;
            }
        };

        let clip_sources = clip_store.get_mut(clip_sources_handle);
        clip_sources.update(gpu_cache, resource_cache);
        let (screen_inner_rect, screen_outer_rect) =
            clip_sources.get_screen_bounds(&self.world_viewport_transform, device_pixel_ratio);

        // If this clip's inner rectangle completely surrounds the existing clip
        // chain's outer rectangle, we can discard this clip entirely since it isn't
        // going to affect anything.
        if screen_inner_rect.contains_rect(&state.combined_outer_clip_bounds) {
            self.clip_chain_node = current_clip_chain;
            self.combined_clip_outer_bounds = state.combined_outer_clip_bounds;
            return;
        }

        let combined_outer_screen_rect = match screen_outer_rect {
            Some(outer_rect) => {
                // If this clips outer rectangle is completely enclosed by the clip
                // chain's inner rectangle, then the only clip that matters from this point
                // on is this clip. We can disconnect this clip from the parent clip chain.
                if state.combined_inner_clip_bounds.contains_rect(&outer_rect) {
                    current_clip_chain = None;
                }
                outer_rect.intersection(&state.combined_outer_clip_bounds)
                    .unwrap_or_else(DeviceIntRect::zero)
            }
            None => state.combined_outer_clip_bounds,
        };

        let combined_inner_screen_rect =
            state.combined_inner_clip_bounds.intersection(&screen_inner_rect)
            .unwrap_or_else(DeviceIntRect::zero);

        state.combined_outer_clip_bounds = combined_outer_screen_rect;
        state.combined_inner_clip_bounds = combined_inner_screen_rect;
        self.combined_clip_outer_bounds = combined_outer_screen_rect;

        self.clip_chain_node = Some(Rc::new(ClipChainNode {
            work_item: ClipWorkItem {
                scroll_node_data_index: self.node_data_index,
                clip_sources: clip_sources_handle.weak(),
                coordinate_system_id: state.current_coordinate_system_id,
            },
            screen_inner_rect,
            combined_outer_screen_rect,
            combined_inner_screen_rect,
            prev: current_clip_chain,
        }));

        state.parent_clip_chain = self.clip_chain_node.clone();
    }

    pub fn update_transform(
        &mut self,
        state: &mut TransformUpdateState,
        scene_properties: &SceneProperties,
    ) {
        if self.node_type.is_reference_frame() {
            self.update_transform_for_reference_frame(state, scene_properties);
            return;
        }

        // We calculate this here to avoid a double-borrow later.
        let sticky_offset = self.calculate_sticky_offset(
            &state.nearest_scrolling_ancestor_offset,
            &state.nearest_scrolling_ancestor_viewport,
        );

        // The transformation for the bounds of our viewport is the parent reference frame
        // transform, plus any accumulated scroll offset from our parents, plus any offset
        // provided by our own sticky positioning.
        let accumulated_offset = state.parent_accumulated_scroll_offset + sticky_offset;
        self.world_viewport_transform = if accumulated_offset != LayerVector2D::zero() {
            state.parent_reference_frame_transform.pre_translate(accumulated_offset.to_3d())
        } else {
            state.parent_reference_frame_transform
        };

        // The transformation for any content inside of us is the viewport transformation, plus
        // whatever scrolling offset we supply as well.
        let scroll_offset = self.scroll_offset();
        self.world_content_transform = if scroll_offset != LayerVector2D::zero() {
            self.world_viewport_transform.pre_translate(scroll_offset.to_3d())
        } else {
            self.world_viewport_transform
        };

        match self.node_type {
            NodeType::StickyFrame(ref mut info) => info.current_offset = sticky_offset,
            _ => {},
        }

        self.coordinate_system_id = state.current_coordinate_system_id;
    }

    pub fn update_transform_for_reference_frame(
        &mut self,
        state: &mut TransformUpdateState,
        scene_properties: &SceneProperties,
    ) {
        let info = match self.node_type {
            NodeType::ReferenceFrame(ref mut info) => info,
            _ => unreachable!("Called update_transform_for_reference_frame on non-ReferenceFrame"),
        };

        // Resolve the transform against any property bindings.
        let source_transform = scene_properties.resolve_layout_transform(&info.source_transform);
        info.resolved_transform = LayerToScrollTransform::create_translation(
            info.origin_in_parent_reference_frame.x,
            info.origin_in_parent_reference_frame.y,
            0.0
        ).pre_mul(&source_transform)
         .pre_mul(&info.source_perspective);

        if !info.resolved_transform.preserves_2d_axis_alignment() ||
           info.resolved_transform.has_perspective_component() {
            state.current_coordinate_system_id = state.next_coordinate_system_id;
            state.next_coordinate_system_id = state.next_coordinate_system_id.next();
            self.coordinate_system_id = state.current_coordinate_system_id;
        }

        // The transformation for this viewport in world coordinates is the transformation for
        // our parent reference frame, plus any accumulated scrolling offsets from nodes
        // between our reference frame and this node. Finally, we also include
        // whatever local transformation this reference frame provides. This can be combined
        // with the local_viewport_rect to get its position in world space.
        self.world_viewport_transform = state
            .parent_reference_frame_transform
            .pre_translate(state.parent_accumulated_scroll_offset.to_3d())
            .pre_mul(&info.resolved_transform.with_destination::<LayerPixel>());
        self.world_content_transform = self.world_viewport_transform;
    }

    fn calculate_sticky_offset(
        &self,
        viewport_scroll_offset: &LayerVector2D,
        viewport_rect: &LayerRect,
    ) -> LayerVector2D {
        let info = match self.node_type {
            NodeType::StickyFrame(ref info) => info,
            _ => return LayerVector2D::zero(),
        };

        if info.margins.top.is_none() && info.margins.bottom.is_none() &&
            info.margins.left.is_none() && info.margins.right.is_none() {
            return LayerVector2D::zero();
        }

        // The viewport and margins of the item establishes the maximum amount that it can
        // be offset in order to keep it on screen. Since we care about the relationship
        // between the scrolled content and unscrolled viewport we adjust the viewport's
        // position by the scroll offset in order to work with their relative positions on the
        // page.
        let sticky_rect = self.local_viewport_rect.translate(viewport_scroll_offset);

        let mut sticky_offset = LayerVector2D::zero();
        if let Some(margin) = info.margins.top {
            let top_viewport_edge = viewport_rect.min_y() + margin;
            if sticky_rect.min_y() < top_viewport_edge {
                // If the sticky rect is positioned above the top edge of the viewport (plus margin)
                // we move it down so that it is fully inside the viewport.
                sticky_offset.y = top_viewport_edge - sticky_rect.min_y();
            } else if info.previously_applied_offset.y > 0.0 &&
                sticky_rect.min_y() > top_viewport_edge {
                // However, if the sticky rect is positioned *below* the top edge of the viewport
                // and there is already some offset applied to the sticky rect's position, then
                // we need to move it up so that it remains at the correct position. This
                // makes sticky_offset.y negative and effectively reduces the amount of the
                // offset that was already applied. We limit the reduction so that it can, at most,
                // cancel out the already-applied offset, but should never end up adjusting the
                // position the other way.
                sticky_offset.y = top_viewport_edge - sticky_rect.min_y();
                sticky_offset.y = sticky_offset.y.max(-info.previously_applied_offset.y);
            }
            debug_assert!(sticky_offset.y + info.previously_applied_offset.y >= 0.0);
        }

        // If we don't have a sticky-top offset (sticky_offset.y + info.previously_applied_offset.y
        // == 0), or if we have a previously-applied bottom offset (previously_applied_offset.y < 0)
        // then we check for handling the bottom margin case.
        if sticky_offset.y + info.previously_applied_offset.y <= 0.0 {
            if let Some(margin) = info.margins.bottom {
                // Same as the above case, but inverted for bottom-sticky items. Here
                // we adjust items upwards, resulting in a negative sticky_offset.y,
                // or reduce the already-present upward adjustment, resulting in a positive
                // sticky_offset.y.
                let bottom_viewport_edge = viewport_rect.max_y() - margin;
                if sticky_rect.max_y() > bottom_viewport_edge {
                    sticky_offset.y = bottom_viewport_edge - sticky_rect.max_y();
                } else if info.previously_applied_offset.y < 0.0 &&
                    sticky_rect.max_y() < bottom_viewport_edge {
                    sticky_offset.y = bottom_viewport_edge - sticky_rect.max_y();
                    sticky_offset.y = sticky_offset.y.min(-info.previously_applied_offset.y);
                }
                debug_assert!(sticky_offset.y + info.previously_applied_offset.y <= 0.0);
            }
        }

        // Same as above, but for the x-axis.
        if let Some(margin) = info.margins.left {
            let left_viewport_edge = viewport_rect.min_x() + margin;
            if sticky_rect.min_x() < left_viewport_edge {
                sticky_offset.x = left_viewport_edge - sticky_rect.min_x();
            } else if info.previously_applied_offset.x > 0.0 &&
                sticky_rect.min_x() > left_viewport_edge {
                sticky_offset.x = left_viewport_edge - sticky_rect.min_x();
                sticky_offset.x = sticky_offset.x.max(-info.previously_applied_offset.x);
            }
            debug_assert!(sticky_offset.x + info.previously_applied_offset.x >= 0.0);
        }

        if sticky_offset.x + info.previously_applied_offset.x <= 0.0 {
            if let Some(margin) = info.margins.right {
                let right_viewport_edge = viewport_rect.max_x() - margin;
                if sticky_rect.max_x() > right_viewport_edge {
                    sticky_offset.x = right_viewport_edge - sticky_rect.max_x();
                } else if info.previously_applied_offset.x < 0.0 &&
                    sticky_rect.max_x() < right_viewport_edge {
                    sticky_offset.x = right_viewport_edge - sticky_rect.max_x();
                    sticky_offset.x = sticky_offset.x.min(-info.previously_applied_offset.x);
                }
                debug_assert!(sticky_offset.x + info.previously_applied_offset.x <= 0.0);
            }
        }

        // The total "sticky offset" (which is the sum that was already applied by
        // the calling code, stored in info.previously_applied_offset, and the extra amount we
        // computed as a result of scrolling, stored in sticky_offset) needs to be
        // clamped to the provided bounds.
        let clamp_adjusted = |value: f32, adjust: f32, bounds: &StickyOffsetBounds| {
            (value + adjust).max(bounds.min).min(bounds.max) - adjust
        };
        sticky_offset.y = clamp_adjusted(sticky_offset.y,
                                         info.previously_applied_offset.y,
                                         &info.vertical_offset_bounds);
        sticky_offset.x = clamp_adjusted(sticky_offset.x,
                                         info.previously_applied_offset.x,
                                         &info.horizontal_offset_bounds);

        sticky_offset
    }

    pub fn prepare_state_for_children(
        &self,
        state: &mut TransformUpdateState,
        node_data: &Vec<ClipScrollNodeData>
    ) {
        if self.combined_clip_outer_bounds.is_empty() {
            state.parent_combined_viewport_rect = LayerRect::zero();
            state.combined_outer_clip_bounds = DeviceIntRect::zero();
            state.parent_clip_chain = None;
            return;
        }

        let combined_local_viewport_rect =
            node_data[self.node_data_index.0 as usize].local_clip_rect;

        // The transformation we are passing is the transformation of the parent
        // reference frame and the offset is the accumulated offset of all the nodes
        // between us and the parent reference frame. If we are a reference frame,
        // we need to reset both these values.
        match self.node_type {
            NodeType::ReferenceFrame(ref info) => {
                state.parent_reference_frame_transform = self.world_viewport_transform;
                state.parent_combined_viewport_rect = combined_local_viewport_rect;
                state.parent_accumulated_scroll_offset = LayerVector2D::zero();
                state.nearest_scrolling_ancestor_viewport =
                    state.nearest_scrolling_ancestor_viewport
                       .translate(&info.origin_in_parent_reference_frame);
            }
            NodeType::Clip(..) => {
                state.parent_combined_viewport_rect = combined_local_viewport_rect;
            },
            NodeType::ScrollFrame(ref scrolling) => {
                state.parent_combined_viewport_rect =
                        combined_local_viewport_rect.translate(&-scrolling.offset);
                state.parent_accumulated_scroll_offset =
                    scrolling.offset + state.parent_accumulated_scroll_offset;
                state.nearest_scrolling_ancestor_offset = scrolling.offset;
                state.nearest_scrolling_ancestor_viewport = self.local_viewport_rect;
            }
            NodeType::StickyFrame(ref info) => {
                // We don't translate the combined rect by the sticky offset, because sticky
                // offsets actually adjust the node position itself, whereas scroll offsets
                // only apply to contents inside the node.
                state.parent_combined_viewport_rect = combined_local_viewport_rect;
                state.parent_accumulated_scroll_offset =
                    info.current_offset + state.parent_accumulated_scroll_offset;
            }
        }
    }

    pub fn scrollable_size(&self) -> LayerSize {
        match self.node_type {
           NodeType:: ScrollFrame(state) => state.scrollable_size,
            _ => LayerSize::zero(),
        }
    }


    pub fn scroll(&mut self, scroll_location: ScrollLocation, phase: ScrollEventPhase) -> bool {
        let scrolling = match self.node_type {
            NodeType::ScrollFrame(ref mut scrolling) => scrolling,
            _ => return false,
        };

        if scrolling.started_bouncing_back && phase == ScrollEventPhase::Move(false) {
            return false;
        }

        let mut delta = match scroll_location {
            ScrollLocation::Delta(delta) => delta,
            ScrollLocation::Start => {
                if scrolling.offset.y.round() >= 0.0 {
                    // Nothing to do on this layer.
                    return false;
                }

                scrolling.offset.y = 0.0;
                return true;
            }
            ScrollLocation::End => {
                let end_pos = -scrolling.scrollable_size.height;
                if scrolling.offset.y.round() <= end_pos {
                    // Nothing to do on this layer.
                    return false;
                }

                scrolling.offset.y = end_pos;
                return true;
            }
        };

        let overscroll_amount = scrolling.overscroll_amount();
        let overscrolling = CAN_OVERSCROLL && (overscroll_amount != LayerVector2D::zero());
        if overscrolling {
            if overscroll_amount.x != 0.0 {
                delta.x /= overscroll_amount.x.abs()
            }
            if overscroll_amount.y != 0.0 {
                delta.y /= overscroll_amount.y.abs()
            }
        }

        let scrollable_width = scrolling.scrollable_size.width;
        let scrollable_height = scrolling.scrollable_size.height;
        let is_unscrollable = scrollable_width <= 0. && scrollable_height <= 0.;
        let original_layer_scroll_offset = scrolling.offset;

        if scrollable_width > 0. {
            scrolling.offset.x = scrolling.offset.x + delta.x;
            if is_unscrollable || !CAN_OVERSCROLL {
                scrolling.offset.x = scrolling.offset.x.min(0.0).max(-scrollable_width).round();
            }
        }

        if scrollable_height > 0. {
            scrolling.offset.y = scrolling.offset.y + delta.y;
            if is_unscrollable || !CAN_OVERSCROLL {
                scrolling.offset.y = scrolling.offset.y.min(0.0).max(-scrollable_height).round();
            }
        }

        if phase == ScrollEventPhase::Start || phase == ScrollEventPhase::Move(true) {
            scrolling.started_bouncing_back = false
        } else if overscrolling &&
            ((delta.x < 1.0 && delta.y < 1.0) || phase == ScrollEventPhase::End)
        {
            scrolling.started_bouncing_back = true;
            scrolling.bouncing_back = true
        }

        if CAN_OVERSCROLL {
            scrolling.stretch_overscroll_spring(overscroll_amount);
        }

        scrolling.offset != original_layer_scroll_offset || scrolling.started_bouncing_back
    }

    pub fn tick_scrolling_bounce_animation(&mut self) {
        if let NodeType::ScrollFrame(ref mut scrolling) = self.node_type {
            scrolling.tick_scrolling_bounce_animation();
        }
    }

    pub fn ray_intersects_node(&self, cursor: &WorldPoint) -> bool {
        let inv = match self.world_viewport_transform.inverse() {
            Some(inv) => inv,
            None => return false,
        };

        let z0 = -10000.0;
        let z1 = 10000.0;

        let p0 = inv.transform_point3d(&cursor.extend(z0));
        let p1 = inv.transform_point3d(&cursor.extend(z1));

        if self.scrollable_size() == LayerSize::zero() {
            return false;
        }

        ray_intersects_rect(
            p0.to_untyped(),
            p1.to_untyped(),
            self.local_viewport_rect.to_untyped(),
        )
    }

    pub fn scroll_offset(&self) -> LayerVector2D {
        match self.node_type {
            NodeType::ScrollFrame(ref scrolling) => scrolling.offset,
            _ => LayerVector2D::zero(),
        }
    }

    pub fn is_overscrolling(&self) -> bool {
        match self.node_type {
            NodeType::ScrollFrame(ref state) => state.overscroll_amount() != LayerVector2D::zero(),
            _ => false,
        }
    }

    pub fn is_visible(&self) -> bool {
        self.combined_clip_outer_bounds != DeviceIntRect::zero()
    }
}

#[derive(Copy, Clone, Debug)]
pub struct ScrollingState {
    pub offset: LayerVector2D,
    pub spring: Spring,
    pub started_bouncing_back: bool,
    pub bouncing_back: bool,
    pub should_handoff_scroll: bool,
    pub scroll_sensitivity: ScrollSensitivity,

    /// Amount that this ScrollFrame can scroll in both directions.
    pub scrollable_size: LayerSize,

}

/// Manages scrolling offset, overscroll state, etc.
impl ScrollingState {
    pub fn new(scroll_sensitivity: ScrollSensitivity,
               scrollable_size: LayerSize
    ) -> ScrollingState {
        ScrollingState {
            offset: LayerVector2D::zero(),
            spring: Spring::at(LayerPoint::zero(), STIFFNESS, DAMPING),
            started_bouncing_back: false,
            bouncing_back: false,
            should_handoff_scroll: false,
            scroll_sensitivity,
            scrollable_size,
        }
    }

    pub fn sensitive_to_input_events(&self) -> bool {
        match self.scroll_sensitivity {
            ScrollSensitivity::ScriptAndInputEvents => true,
            ScrollSensitivity::Script => false,
        }
    }

    pub fn stretch_overscroll_spring(&mut self, overscroll_amount: LayerVector2D) {
        let offset = self.offset.to_point();
        self.spring
            .coords(offset, offset, offset + overscroll_amount);
    }

    pub fn tick_scrolling_bounce_animation(&mut self) {
        let finished = self.spring.animate();
        self.offset = self.spring.current().to_vector();
        if finished {
            self.bouncing_back = false
        }
    }

    pub fn overscroll_amount(&self) -> LayerVector2D {
        let overscroll_x = if self.offset.x > 0.0 {
            -self.offset.x
        } else if self.offset.x < -self.scrollable_size.width {
            -self.scrollable_size.width - self.offset.x
        } else {
            0.0
        };

        let overscroll_y = if self.offset.y > 0.0 {
            -self.offset.y
        } else if self.offset.y < -self.scrollable_size.height {
            -self.scrollable_size.height - self.offset.y
        } else {
            0.0
        };

        LayerVector2D::new(overscroll_x, overscroll_y)
    }
}

/// Contains information about reference frames.
#[derive(Copy, Clone, Debug)]
pub struct ReferenceFrameInfo {
    /// The transformation that establishes this reference frame, relative to the parent
    /// reference frame. The origin of the reference frame is included in the transformation.
    pub resolved_transform: LayerToScrollTransform,

    /// The source transform and perspective matrices provided by the stacking context
    /// that forms this reference frame. We maintain the property binding information
    /// here so that we can resolve the animated transform and update the tree each
    /// frame.
    pub source_transform: PropertyBinding<LayoutTransform>,
    pub source_perspective: LayoutTransform,

    /// The original, not including the transform and relative to the parent reference frame,
    /// origin of this reference frame. This is already rolled into the `transform' property, but
    /// we also store it here to properly transform the viewport for sticky positioning.
    pub origin_in_parent_reference_frame: LayerVector2D,
}

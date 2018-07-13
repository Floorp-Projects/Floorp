
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ExternalScrollId, LayoutPixel, LayoutPoint, LayoutRect, LayoutSize, LayoutTransform};
use api::{LayoutVector2D, PipelineId, PropertyBinding, ScrollClamping, ScrollLocation};
use api::{ScrollSensitivity, StickyOffsetBounds};
use clip_scroll_tree::{CoordinateSystemId, SpatialNodeIndex, TransformUpdateState};
use euclid::SideOffsets2D;
use gpu_types::{TransformData, TransformIndex, TransformPalette};
use scene::SceneProperties;
use util::{LayoutFastTransform, LayoutToWorldFastTransform, TransformedRectKind};

#[derive(Clone, Debug)]
pub enum SpatialNodeType {
    /// A special kind of node that adjusts its position based on the position
    /// of its parent node and a given set of sticky positioning offset bounds.
    /// Sticky positioned is described in the CSS Positioned Layout Module Level 3 here:
    /// https://www.w3.org/TR/css-position-3/#sticky-pos
    StickyFrame(StickyFrameInfo),

    /// Transforms it's content, but doesn't clip it. Can also be adjusted
    /// by scroll events or setting scroll offsets.
    ScrollFrame(ScrollFrameInfo),

    /// A reference frame establishes a new coordinate space in the tree.
    ReferenceFrame(ReferenceFrameInfo),

    /// An empty node, used to pad the ClipScrollTree's array of nodes so that
    /// we can immediately use each assigned SpatialNodeIndex. After display
    /// list flattening this node type should never be used.
    Empty,
}

impl SpatialNodeType {
    fn is_reference_frame(&self) -> bool {
        match *self {
            SpatialNodeType::ReferenceFrame(_) => true,
            _ => false,
        }
    }
}

/// Contains information common among all types of ClipScrollTree nodes.
#[derive(Clone, Debug)]
pub struct SpatialNode {
    /// The transformation for this viewport in world coordinates is the transformation for
    /// our parent reference frame, plus any accumulated scrolling offsets from nodes
    /// between our reference frame and this node. For reference frames, we also include
    /// whatever local transformation this reference frame provides.
    pub world_viewport_transform: LayoutToWorldFastTransform,

    /// World transform for content transformed by this node.
    pub world_content_transform: LayoutToWorldFastTransform,

    /// The current transform kind of world_content_transform.
    pub transform_kind: TransformedRectKind,

    /// Pipeline that this layer belongs to
    pub pipeline_id: PipelineId,

    /// Parent layer. If this is None, we are the root node.
    pub parent: Option<SpatialNodeIndex>,

    /// Child layers
    pub children: Vec<SpatialNodeIndex>,

    /// The type of this node and any data associated with that node type.
    pub node_type: SpatialNodeType,

    /// True if this node is transformed by an invertible transform.  If not, display items
    /// transformed by this node will not be displayed and display items not transformed by this
    /// node will not be clipped by clips that are transformed by this node.
    pub invertible: bool,

    /// The axis-aligned coordinate system id of this node.
    pub coordinate_system_id: CoordinateSystemId,

    /// The transformation from the coordinate system which established our compatible coordinate
    /// system (same coordinate system id) and us. This can change via scroll offsets and via new
    /// reference frame transforms.
    pub coordinate_system_relative_transform: LayoutFastTransform,
}

impl SpatialNode {
    pub fn new(
        pipeline_id: PipelineId,
        parent_index: Option<SpatialNodeIndex>,
        node_type: SpatialNodeType,
    ) -> Self {
        SpatialNode {
            world_viewport_transform: LayoutToWorldFastTransform::identity(),
            world_content_transform: LayoutToWorldFastTransform::identity(),
            transform_kind: TransformedRectKind::AxisAligned,
            parent: parent_index,
            children: Vec::new(),
            pipeline_id,
            node_type,
            invertible: true,
            coordinate_system_id: CoordinateSystemId(0),
            coordinate_system_relative_transform: LayoutFastTransform::identity(),
        }
    }

    pub fn empty() -> SpatialNode {
        Self::new(PipelineId::dummy(), None, SpatialNodeType::Empty)
    }

    pub fn new_scroll_frame(
        pipeline_id: PipelineId,
        parent_index: SpatialNodeIndex,
        external_id: Option<ExternalScrollId>,
        frame_rect: &LayoutRect,
        content_size: &LayoutSize,
        scroll_sensitivity: ScrollSensitivity,
    ) -> Self {
        let node_type = SpatialNodeType::ScrollFrame(ScrollFrameInfo::new(
                *frame_rect,
                scroll_sensitivity,
                LayoutSize::new(
                    (content_size.width - frame_rect.size.width).max(0.0),
                    (content_size.height - frame_rect.size.height).max(0.0)
                ),
                external_id,
            )
        );

        Self::new(pipeline_id, Some(parent_index), node_type)
    }

    pub fn new_reference_frame(
        parent_index: Option<SpatialNodeIndex>,
        source_transform: Option<PropertyBinding<LayoutTransform>>,
        source_perspective: Option<LayoutTransform>,
        origin_in_parent_reference_frame: LayoutVector2D,
        pipeline_id: PipelineId,
    ) -> Self {
        let identity = LayoutTransform::identity();
        let source_perspective = source_perspective.map_or_else(
            LayoutFastTransform::identity, |perspective| perspective.into());
        let info = ReferenceFrameInfo {
            resolved_transform: LayoutFastTransform::identity(),
            source_transform: source_transform.unwrap_or(PropertyBinding::Value(identity)),
            source_perspective,
            origin_in_parent_reference_frame,
            invertible: true,
        };
        Self::new(pipeline_id, parent_index, SpatialNodeType:: ReferenceFrame(info))
    }

    pub fn new_sticky_frame(
        parent_index: SpatialNodeIndex,
        sticky_frame_info: StickyFrameInfo,
        pipeline_id: PipelineId,
    ) -> Self {
        Self::new(pipeline_id, Some(parent_index), SpatialNodeType::StickyFrame(sticky_frame_info))
    }


    pub fn add_child(&mut self, child: SpatialNodeIndex) {
        self.children.push(child);
    }

    pub fn apply_old_scrolling_state(&mut self, old_scroll_info: &ScrollFrameInfo) {
        match self.node_type {
            SpatialNodeType::ScrollFrame(ref mut scrolling) => {
                *scrolling = scrolling.combine_with_old_scroll_info(old_scroll_info);
            }
            _ if old_scroll_info.offset != LayoutVector2D::zero() => {
                warn!("Tried to scroll a non-scroll node.")
            }
            _ => {}
        }
    }

    pub fn set_scroll_origin(&mut self, origin: &LayoutPoint, clamp: ScrollClamping) -> bool {
        let scrollable_size = self.scrollable_size();
        let scrollable_width = scrollable_size.width;
        let scrollable_height = scrollable_size.height;

        let scrolling = match self.node_type {
            SpatialNodeType::ScrollFrame(ref mut scrolling) => scrolling,
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

                let origin = LayoutPoint::new(origin.x.max(0.0), origin.y.max(0.0));
                LayoutVector2D::new(
                    (-origin.x).max(-scrollable_width).min(0.0).round(),
                    (-origin.y).max(-scrollable_height).min(0.0).round(),
                )
            }
            ScrollClamping::NoClamping => LayoutPoint::zero() - *origin,
        };

        if new_offset == scrolling.offset {
            return false;
        }

        scrolling.offset = new_offset;
        true
    }

    pub fn mark_uninvertible(&mut self) {
        self.invertible = false;
        self.world_content_transform = LayoutToWorldFastTransform::identity();
        self.world_viewport_transform = LayoutToWorldFastTransform::identity();
    }

    pub fn push_gpu_data(
        &mut self,
        transform_palette: &mut TransformPalette,
        node_index: SpatialNodeIndex,
    ) {
        let transform_index = TransformIndex(node_index.0 as u32);
        if !self.invertible {
            transform_palette.set(transform_index, TransformData::invalid());
            return;
        }

        let inv_transform = match self.world_content_transform.inverse() {
            Some(inverted) => inverted.to_transform(),
            None => {
                transform_palette.set(transform_index, TransformData::invalid());
                return;
            }
        };

        let data = TransformData {
            transform: self.world_content_transform.into(),
            inv_transform,
        };

        // Write the data that will be made available to the GPU for this node.
        transform_palette.set(transform_index, data);
    }

    pub fn update(
        &mut self,
        state: &mut TransformUpdateState,
        next_coordinate_system_id: &mut CoordinateSystemId,
        scene_properties: &SceneProperties,
    ) {
        // If any of our parents was not rendered, we are not rendered either and can just
        // quit here.
        if !state.invertible {
            self.mark_uninvertible();
            return;
        }

        self.update_transform(state, next_coordinate_system_id, scene_properties);

        self.transform_kind = if self.world_content_transform.preserves_2d_axis_alignment() {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        };

        // If this node is a reference frame, we check if it has a non-invertible matrix.
        // For non-reference-frames we assume that they will produce only additional
        // translations which should be invertible.
        match self.node_type {
            SpatialNodeType::ReferenceFrame(info) if !info.invertible => {
                self.mark_uninvertible();
                return;
            }
            _ => self.invertible = true,
        }
    }

    pub fn update_transform(
        &mut self,
        state: &mut TransformUpdateState,
        next_coordinate_system_id: &mut CoordinateSystemId,
        scene_properties: &SceneProperties,
    ) {
        if self.node_type.is_reference_frame() {
            self.update_transform_for_reference_frame(
                state,
                next_coordinate_system_id,
                scene_properties
            );
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
        self.world_viewport_transform = if accumulated_offset != LayoutVector2D::zero() {
            state.parent_reference_frame_transform.pre_translate(&accumulated_offset)
        } else {
            state.parent_reference_frame_transform
        };

        // The transformation for any content inside of us is the viewport transformation, plus
        // whatever scrolling offset we supply as well.
        let scroll_offset = self.scroll_offset();
        self.world_content_transform = if scroll_offset != LayoutVector2D::zero() {
            self.world_viewport_transform.pre_translate(&scroll_offset)
        } else {
            self.world_viewport_transform
        };

        let added_offset = state.parent_accumulated_scroll_offset + sticky_offset + scroll_offset;
        self.coordinate_system_relative_transform =
            state.coordinate_system_relative_transform.offset(added_offset);

        if let SpatialNodeType::StickyFrame(ref mut info) = self.node_type {
            info.current_offset = sticky_offset;
        }

        self.coordinate_system_id = state.current_coordinate_system_id;
    }

    pub fn update_transform_for_reference_frame(
        &mut self,
        state: &mut TransformUpdateState,
        next_coordinate_system_id: &mut CoordinateSystemId,
        scene_properties: &SceneProperties,
    ) {
        let info = match self.node_type {
            SpatialNodeType::ReferenceFrame(ref mut info) => info,
            _ => unreachable!("Called update_transform_for_reference_frame on non-ReferenceFrame"),
        };

        // Resolve the transform against any property bindings.
        let source_transform = scene_properties.resolve_layout_transform(&info.source_transform);
        info.resolved_transform =
            LayoutFastTransform::with_vector(info.origin_in_parent_reference_frame)
            .pre_mul(&source_transform.into())
            .pre_mul(&info.source_perspective);

        // The transformation for this viewport in world coordinates is the transformation for
        // our parent reference frame, plus any accumulated scrolling offsets from nodes
        // between our reference frame and this node. Finally, we also include
        // whatever local transformation this reference frame provides.
        let relative_transform = info.resolved_transform
            .post_translate(state.parent_accumulated_scroll_offset)
            .to_transform()
            .with_destination::<LayoutPixel>();
        self.world_viewport_transform =
            state.parent_reference_frame_transform.pre_mul(&relative_transform.into());
        self.world_content_transform = self.world_viewport_transform;

        info.invertible = self.world_viewport_transform.is_invertible();
        if !info.invertible {
            return;
        }

        // Try to update our compatible coordinate system transform. If we cannot, start a new
        // incompatible coordinate system.
        match state.coordinate_system_relative_transform.update(relative_transform) {
            Some(offset) => self.coordinate_system_relative_transform = offset,
            None => {
                self.coordinate_system_relative_transform = LayoutFastTransform::identity();
                state.current_coordinate_system_id = *next_coordinate_system_id;
                next_coordinate_system_id.advance();
            }
        }

        self.coordinate_system_id = state.current_coordinate_system_id;
    }

    fn calculate_sticky_offset(
        &self,
        viewport_scroll_offset: &LayoutVector2D,
        viewport_rect: &LayoutRect,
    ) -> LayoutVector2D {
        let info = match self.node_type {
            SpatialNodeType::StickyFrame(ref info) => info,
            _ => return LayoutVector2D::zero(),
        };

        if info.margins.top.is_none() && info.margins.bottom.is_none() &&
            info.margins.left.is_none() && info.margins.right.is_none() {
            return LayoutVector2D::zero();
        }

        // The viewport and margins of the item establishes the maximum amount that it can
        // be offset in order to keep it on screen. Since we care about the relationship
        // between the scrolled content and unscrolled viewport we adjust the viewport's
        // position by the scroll offset in order to work with their relative positions on the
        // page.
        let sticky_rect = info.frame_rect.translate(viewport_scroll_offset);

        let mut sticky_offset = LayoutVector2D::zero();
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

    pub fn prepare_state_for_children(&self, state: &mut TransformUpdateState) {
        if !self.invertible {
            state.invertible = false;
            return;
        }

        // The transformation we are passing is the transformation of the parent
        // reference frame and the offset is the accumulated offset of all the nodes
        // between us and the parent reference frame. If we are a reference frame,
        // we need to reset both these values.
        match self.node_type {
            SpatialNodeType::StickyFrame(ref info) => {
                // We don't translate the combined rect by the sticky offset, because sticky
                // offsets actually adjust the node position itself, whereas scroll offsets
                // only apply to contents inside the node.
                state.parent_accumulated_scroll_offset =
                    info.current_offset + state.parent_accumulated_scroll_offset;
            }
            SpatialNodeType::ScrollFrame(ref scrolling) => {
                state.parent_accumulated_scroll_offset =
                    scrolling.offset + state.parent_accumulated_scroll_offset;
                state.nearest_scrolling_ancestor_offset = scrolling.offset;
                state.nearest_scrolling_ancestor_viewport = scrolling.viewport_rect;
            }
            SpatialNodeType::ReferenceFrame(ref info) => {
                state.parent_reference_frame_transform = self.world_viewport_transform;
                state.parent_accumulated_scroll_offset = LayoutVector2D::zero();
                state.coordinate_system_relative_transform =
                    self.coordinate_system_relative_transform.clone();
                let translation = -info.origin_in_parent_reference_frame;
                state.nearest_scrolling_ancestor_viewport =
                    state.nearest_scrolling_ancestor_viewport
                       .translate(&translation);
            }
            SpatialNodeType::Empty => unreachable!("Empty node remaining in ClipScrollTree."),
        }
    }

    pub fn scrollable_size(&self) -> LayoutSize {
        match self.node_type {
           SpatialNodeType::ScrollFrame(state) => state.scrollable_size,
            _ => LayoutSize::zero(),
        }
    }

    pub fn scroll(&mut self, scroll_location: ScrollLocation) -> bool {
        let scrolling = match self.node_type {
            SpatialNodeType::ScrollFrame(ref mut scrolling) => scrolling,
            _ => return false,
        };

        let delta = match scroll_location {
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

        let scrollable_width = scrolling.scrollable_size.width;
        let scrollable_height = scrolling.scrollable_size.height;
        let original_layer_scroll_offset = scrolling.offset;

        if scrollable_width > 0. {
            scrolling.offset.x = (scrolling.offset.x + delta.x)
                .min(0.0)
                .max(-scrollable_width)
                .round();
        }

        if scrollable_height > 0. {
            scrolling.offset.y = (scrolling.offset.y + delta.y)
                .min(0.0)
                .max(-scrollable_height)
                .round();
        }

        scrolling.offset != original_layer_scroll_offset
    }

    pub fn scroll_offset(&self) -> LayoutVector2D {
        match self.node_type {
            SpatialNodeType::ScrollFrame(ref scrolling) => scrolling.offset,
            _ => LayoutVector2D::zero(),
        }
    }

    pub fn matches_external_id(&self, external_id: ExternalScrollId) -> bool {
        match self.node_type {
            SpatialNodeType::ScrollFrame(info) if info.external_id == Some(external_id) => true,
            _ => false,
        }
    }
}

#[derive(Copy, Clone, Debug)]
pub struct ScrollFrameInfo {
    /// The rectangle of the viewport of this scroll frame. This is important for
    /// positioning of items inside child StickyFrames.
    pub viewport_rect: LayoutRect,

    pub offset: LayoutVector2D,
    pub scroll_sensitivity: ScrollSensitivity,

    /// Amount that this ScrollFrame can scroll in both directions.
    pub scrollable_size: LayoutSize,

    /// An external id to identify this scroll frame to API clients. This
    /// allows setting scroll positions via the API without relying on ClipsIds
    /// which may change between frames.
    pub external_id: Option<ExternalScrollId>,

}

/// Manages scrolling offset.
impl ScrollFrameInfo {
    pub fn new(
        viewport_rect: LayoutRect,
        scroll_sensitivity: ScrollSensitivity,
        scrollable_size: LayoutSize,
        external_id: Option<ExternalScrollId>,
    ) -> ScrollFrameInfo {
        ScrollFrameInfo {
            viewport_rect,
            offset: LayoutVector2D::zero(),
            scroll_sensitivity,
            scrollable_size,
            external_id,
        }
    }

    pub fn sensitive_to_input_events(&self) -> bool {
        match self.scroll_sensitivity {
            ScrollSensitivity::ScriptAndInputEvents => true,
            ScrollSensitivity::Script => false,
        }
    }

    pub fn combine_with_old_scroll_info(
        self,
        old_scroll_info: &ScrollFrameInfo
    ) -> ScrollFrameInfo {
        ScrollFrameInfo {
            viewport_rect: self.viewport_rect,
            offset: old_scroll_info.offset,
            scroll_sensitivity: self.scroll_sensitivity,
            scrollable_size: self.scrollable_size,
            external_id: self.external_id,
        }
    }
}

/// Contains information about reference frames.
#[derive(Copy, Clone, Debug)]
pub struct ReferenceFrameInfo {
    /// The transformation that establishes this reference frame, relative to the parent
    /// reference frame. The origin of the reference frame is included in the transformation.
    pub resolved_transform: LayoutFastTransform,

    /// The source transform and perspective matrices provided by the stacking context
    /// that forms this reference frame. We maintain the property binding information
    /// here so that we can resolve the animated transform and update the tree each
    /// frame.
    pub source_transform: PropertyBinding<LayoutTransform>,
    pub source_perspective: LayoutFastTransform,

    /// The original, not including the transform and relative to the parent reference frame,
    /// origin of this reference frame. This is already rolled into the `transform' property, but
    /// we also store it here to properly transform the viewport for sticky positioning.
    pub origin_in_parent_reference_frame: LayoutVector2D,

    /// True if the resolved transform is invertible.
    pub invertible: bool,
}

#[derive(Clone, Debug)]
pub struct StickyFrameInfo {
    pub frame_rect: LayoutRect,
    pub margins: SideOffsets2D<Option<f32>>,
    pub vertical_offset_bounds: StickyOffsetBounds,
    pub horizontal_offset_bounds: StickyOffsetBounds,
    pub previously_applied_offset: LayoutVector2D,
    pub current_offset: LayoutVector2D,
}

impl StickyFrameInfo {
    pub fn new(
        frame_rect: LayoutRect,
        margins: SideOffsets2D<Option<f32>>,
        vertical_offset_bounds: StickyOffsetBounds,
        horizontal_offset_bounds: StickyOffsetBounds,
        previously_applied_offset: LayoutVector2D
    ) -> StickyFrameInfo {
        StickyFrameInfo {
            frame_rect,
            margins,
            vertical_offset_bounds,
            horizontal_offset_bounds,
            previously_applied_offset,
            current_offset: LayoutVector2D::zero(),
        }
    }
}

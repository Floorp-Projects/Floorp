
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ExternalScrollId, PipelineId, PropertyBinding, PropertyBindingId, ReferenceFrameKind};
use api::{APZScrollGeneration, HasScrollLinkedEffect, SampledScrollOffset};
use api::{TransformStyle, StickyOffsetBounds, SpatialTreeItemKey};
use api::units::*;
use crate::internal_types::PipelineInstanceId;
use crate::spatial_tree::{CoordinateSystem, SpatialNodeIndex, TransformUpdateState};
use crate::spatial_tree::{CoordinateSystemId};
use euclid::{Vector2D, SideOffsets2D};
use crate::scene::SceneProperties;
use crate::util::{LayoutFastTransform, MatrixHelpers, ScaleOffset, TransformedRectKind, PointHelpers};

/// The kind of a spatial node uid. These are required because we currently create external
/// nodes during DL building, but the internal nodes aren't created until scene building.
/// TODO(gw): The internal scroll and reference frames are not used in any important way
//            by Gecko - they were primarily useful for Servo. So we should plan to remove
//            them completely.
#[derive(Copy, Clone, Eq, PartialEq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum SpatialNodeUidKind {
    /// The root node of the entire spatial tree
    Root,
    /// Internal scroll frame created during scene building for each iframe
    InternalScrollFrame,
    /// Internal reference frame created during scene building for each iframe
    InternalReferenceFrame,
    /// A normal spatial node uid, defined by a caller provided unique key
    External {
        key: SpatialTreeItemKey,
    },
}

/// A unique identifier for a spatial node, that is stable across display lists
#[derive(Copy, Clone, Eq, PartialEq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SpatialNodeUid {
    /// The unique key for a given pipeline for this uid
    pub kind: SpatialNodeUidKind,
    /// Pipeline id to namespace key kinds
    pub pipeline_id: PipelineId,
    /// Instance of this pipeline id
    pub instance_id: PipelineInstanceId,
}

impl SpatialNodeUid {
    pub fn root() -> Self {
        SpatialNodeUid {
            kind: SpatialNodeUidKind::Root,
            pipeline_id: PipelineId::dummy(),
            instance_id: PipelineInstanceId::new(0),
        }
    }

    pub fn root_scroll_frame(
        pipeline_id: PipelineId,
        instance_id: PipelineInstanceId,
    ) -> Self {
        SpatialNodeUid {
            kind: SpatialNodeUidKind::InternalScrollFrame,
            pipeline_id,
            instance_id,
        }
    }

    pub fn root_reference_frame(
        pipeline_id: PipelineId,
        instance_id: PipelineInstanceId,
    ) -> Self {
        SpatialNodeUid {
            kind: SpatialNodeUidKind::InternalReferenceFrame,
            pipeline_id,
            instance_id,
        }
    }

    pub fn external(
        key: SpatialTreeItemKey,
        pipeline_id: PipelineId,
        instance_id: PipelineInstanceId,
    ) -> Self {
        SpatialNodeUid {
            kind: SpatialNodeUidKind::External {
                key,
            },
            pipeline_id,
            instance_id,
        }
    }
}

/// Defines the content of a spatial node. If the values in the descriptor don't
/// change, that means the rest of the fields in a spatial node will end up with
/// the same result
#[derive(Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SpatialNodeDescriptor {
    /// The type of this node and any data associated with that node type.
    pub node_type: SpatialNodeType,

    /// Pipeline that this layer belongs to
    pub pipeline_id: PipelineId,
}

#[derive(Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
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
}

/// Information about a spatial node that can be queried during either scene of
/// frame building.
pub struct SpatialNodeInfo<'a> {
    /// The type of this node and any data associated with that node type.
    pub node_type: &'a SpatialNodeType,

    /// Parent spatial node. If this is None, we are the root node.
    pub parent: Option<SpatialNodeIndex>,

    /// Snapping scale/offset relative to the coordinate system. If None, then
    /// we should not snap entities bound to this spatial node.
    pub snapping_transform: Option<ScaleOffset>,
}

/// Scene building specific representation of a spatial node, which is a much
/// lighter subset of a full spatial node constructed and used for frame building
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(PartialEq)]
pub struct SceneSpatialNode {
    /// Snapping scale/offset relative to the coordinate system. If None, then
    /// we should not snap entities bound to this spatial node.
    pub snapping_transform: Option<ScaleOffset>,

    /// Parent spatial node. If this is None, we are the root node.
    pub parent: Option<SpatialNodeIndex>,

    /// Descriptor describing how this spatial node behaves
    pub descriptor: SpatialNodeDescriptor,

    /// If true, this spatial node is known to exist in the root coordinate
    /// system in all cases (it has no animated or complex transforms)
    pub is_root_coord_system: bool,
}

impl SceneSpatialNode {
    pub fn new_reference_frame(
        parent_index: Option<SpatialNodeIndex>,
        transform_style: TransformStyle,
        source_transform: PropertyBinding<LayoutTransform>,
        kind: ReferenceFrameKind,
        origin_in_parent_reference_frame: LayoutVector2D,
        pipeline_id: PipelineId,
        is_root_coord_system: bool,
        is_pipeline_root: bool,
    ) -> Self {
        let info = ReferenceFrameInfo {
            transform_style,
            source_transform,
            kind,
            origin_in_parent_reference_frame,
            is_pipeline_root,
        };
        Self::new(
            pipeline_id,
            parent_index,
            SpatialNodeType::ReferenceFrame(info),
            is_root_coord_system,
        )
    }

    pub fn new_scroll_frame(
        pipeline_id: PipelineId,
        parent_index: SpatialNodeIndex,
        external_id: ExternalScrollId,
        frame_rect: &LayoutRect,
        content_size: &LayoutSize,
        frame_kind: ScrollFrameKind,
        external_scroll_offset: LayoutVector2D,
        offset_generation: APZScrollGeneration,
        has_scroll_linked_effect: HasScrollLinkedEffect,
        is_root_coord_system: bool,
    ) -> Self {
        let node_type = SpatialNodeType::ScrollFrame(ScrollFrameInfo::new(
                *frame_rect,
                LayoutSize::new(
                    (content_size.width - frame_rect.width()).max(0.0),
                    (content_size.height - frame_rect.height()).max(0.0)
                ),
                external_id,
                frame_kind,
                external_scroll_offset,
                offset_generation,
                has_scroll_linked_effect,
            )
        );

        Self::new(
            pipeline_id,
            Some(parent_index),
            node_type,
            is_root_coord_system,
        )
    }

    pub fn new_sticky_frame(
        parent_index: SpatialNodeIndex,
        sticky_frame_info: StickyFrameInfo,
        pipeline_id: PipelineId,
        is_root_coord_system: bool,
    ) -> Self {
        Self::new(
            pipeline_id,
            Some(parent_index),
            SpatialNodeType::StickyFrame(sticky_frame_info),
            is_root_coord_system,
        )
    }

    fn new(
        pipeline_id: PipelineId,
        parent_index: Option<SpatialNodeIndex>,
        node_type: SpatialNodeType,
        is_root_coord_system: bool,
    ) -> Self {
        SceneSpatialNode {
            parent: parent_index,
            descriptor: SpatialNodeDescriptor {
                pipeline_id,
                node_type,
            },
            snapping_transform: None,
            is_root_coord_system,
        }
    }
}

/// Contains information common among all types of SpatialTree nodes.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SpatialNode {
    /// The scale/offset of the viewport for this spatial node, relative to the
    /// coordinate system. Includes any accumulated scrolling offsets from nodes
    /// between our reference frame and this node.
    pub viewport_transform: ScaleOffset,

    /// Content scale/offset relative to the coordinate system.
    pub content_transform: ScaleOffset,

    /// Snapping scale/offset relative to the coordinate system. If None, then
    /// we should not snap entities bound to this spatial node.
    pub snapping_transform: Option<ScaleOffset>,

    /// The axis-aligned coordinate system id of this node.
    pub coordinate_system_id: CoordinateSystemId,

    /// The current transform kind of this node.
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

    /// Whether this specific node is currently being async zoomed.
    /// Should be set when a SetIsTransformAsyncZooming FrameMsg is received.
    pub is_async_zooming: bool,

    /// Whether this node or any of its ancestors is being pinch zoomed.
    /// This is calculated in update(). This will be used to decide whether
    /// to override corresponding picture's raster space as an optimisation.
    pub is_ancestor_or_self_zooming: bool,
}

/// Snap an offset to be incorporated into a transform, where the local space
/// may be considered the world space. We assume raster scale is 1.0, which
/// may not always be correct if there are intermediate surfaces used, however
/// those are either cases where snapping is not important (e.g. has perspective
/// or is not axis aligned), or an edge case (e.g. SVG filters) which we can accept
/// imperfection for now.
fn snap_offset<OffsetUnits, ScaleUnits>(
    offset: Vector2D<f32, OffsetUnits>,
    scale: Vector2D<f32, ScaleUnits>,
) -> Vector2D<f32, OffsetUnits> {
    let world_offset = WorldPoint::new(offset.x * scale.x, offset.y * scale.y);
    let snapped_world_offset = world_offset.snap();
    Vector2D::new(
        if scale.x != 0.0 { snapped_world_offset.x / scale.x } else { offset.x },
        if scale.y != 0.0 { snapped_world_offset.y / scale.y } else { offset.y },
    )
}

impl SpatialNode {
    pub fn add_child(&mut self, child: SpatialNodeIndex) {
        self.children.push(child);
    }

    pub fn set_scroll_offsets(&mut self, mut offsets: Vec<SampledScrollOffset>) -> bool {
        debug_assert!(offsets.len() > 0);

        let scrolling = match self.node_type {
            SpatialNodeType::ScrollFrame(ref mut scrolling) => scrolling,
            _ => {
                warn!("Tried to scroll a non-scroll node.");
                return false;
            }
        };

        for element in offsets.iter_mut() {
            element.offset = -element.offset - scrolling.external_scroll_offset;
        }

        if scrolling.offsets == offsets {
            return false;
        }

        scrolling.offsets = offsets;
        true
    }

    pub fn mark_uninvertible(
        &mut self,
        state: &TransformUpdateState,
    ) {
        self.invertible = false;
        self.viewport_transform = ScaleOffset::identity();
        self.content_transform = ScaleOffset::identity();
        self.coordinate_system_id = state.current_coordinate_system_id;
    }

    pub fn update(
        &mut self,
        state_stack: &[TransformUpdateState],
        coord_systems: &mut Vec<CoordinateSystem>,
        scene_properties: &SceneProperties,
    ) {
        let state = state_stack.last().unwrap();

        self.is_ancestor_or_self_zooming = self.is_async_zooming | state.is_ancestor_or_self_zooming;

        // If any of our parents was not rendered, we are not rendered either and can just
        // quit here.
        if !state.invertible {
            self.mark_uninvertible(state);
            return;
        }

        self.update_transform(
            state_stack,
            coord_systems,
            scene_properties,
        );

        if !self.invertible {
            self.mark_uninvertible(state);
        }
    }

    pub fn update_transform(
        &mut self,
        state_stack: &[TransformUpdateState],
        coord_systems: &mut Vec<CoordinateSystem>,
        scene_properties: &SceneProperties,
    ) {
        let state = state_stack.last().unwrap();

        // Start by assuming we're invertible
        self.invertible = true;

        match self.node_type {
            SpatialNodeType::ReferenceFrame(ref mut info) => {
                let mut cs_scale_offset = ScaleOffset::identity();
                let mut coordinate_system_id = state.current_coordinate_system_id;

                // Resolve the transform against any property bindings.
                let source_transform = {
                    let source_transform = scene_properties.resolve_layout_transform(&info.source_transform);
                    if let ReferenceFrameKind::Transform { is_2d_scale_translation: true, .. } = info.kind {
                        assert!(source_transform.is_2d_scale_translation(), "Reference frame was marked as only having 2d scale or translation");
                    }

                    LayoutFastTransform::from(source_transform)
                };

                // Do a change-basis operation on the perspective matrix using
                // the scroll offset.
                let source_transform = match info.kind {
                    ReferenceFrameKind::Perspective { scrolling_relative_to: Some(external_id) } => {
                        let mut scroll_offset = LayoutVector2D::zero();

                        for parent_state in state_stack.iter().rev() {
                            if let Some(parent_external_id) = parent_state.external_id {
                                if parent_external_id == external_id {
                                    break;
                                }
                            }

                            scroll_offset += parent_state.scroll_offset;
                        }

                        // Do a change-basis operation on the
                        // perspective matrix using the scroll offset.
                        source_transform
                            .pre_translate(scroll_offset)
                            .then_translate(-scroll_offset)
                    }
                    ReferenceFrameKind::Perspective { scrolling_relative_to: None } |
                    ReferenceFrameKind::Transform { .. } => source_transform,
                };

                let resolved_transform =
                    LayoutFastTransform::with_vector(info.origin_in_parent_reference_frame)
                        .pre_transform(&source_transform);

                // The transformation for this viewport in world coordinates is the transformation for
                // our parent reference frame, plus any accumulated scrolling offsets from nodes
                // between our reference frame and this node. Finally, we also include
                // whatever local transformation this reference frame provides.
                let relative_transform = resolved_transform
                    .then_translate(snap_offset(state.parent_accumulated_scroll_offset, state.coordinate_system_relative_scale_offset.scale))
                    .to_transform()
                    .with_destination::<LayoutPixel>();

                let mut reset_cs_id = match info.transform_style {
                    TransformStyle::Preserve3D => !state.preserves_3d,
                    TransformStyle::Flat => state.preserves_3d,
                };

                // We reset the coordinate system upon either crossing the preserve-3d context boundary,
                // or simply a 3D transformation.
                if !reset_cs_id {
                    // Try to update our compatible coordinate system transform. If we cannot, start a new
                    // incompatible coordinate system.
                    match ScaleOffset::from_transform(&relative_transform) {
                        Some(ref scale_offset) => {
                            // We generally do not want to snap animated transforms as it causes jitter.
                            // However, we do want to snap the visual viewport offset when scrolling.
                            // This may still cause jitter when zooming, unfortunately.
                            let mut maybe_snapped = scale_offset.clone();
                            if let ReferenceFrameKind::Transform { should_snap: true, .. } = info.kind {
                                maybe_snapped.offset = snap_offset(
                                    scale_offset.offset,
                                    state.coordinate_system_relative_scale_offset.scale,
                                );
                            }
                            cs_scale_offset =
                                state.coordinate_system_relative_scale_offset.accumulate(&maybe_snapped);
                        }
                        None => reset_cs_id = true,
                    }
                }
                if reset_cs_id {
                    // If we break 2D axis alignment or have a perspective component, we need to start a
                    // new incompatible coordinate system with which we cannot share clips without masking.
                    let transform = relative_transform.then(
                        &state.coordinate_system_relative_scale_offset.to_transform()
                    );

                    // Push that new coordinate system and record the new id.
                    let coord_system = {
                        let parent_system = &coord_systems[state.current_coordinate_system_id.0 as usize];
                        let mut cur_transform = transform;
                        if parent_system.should_flatten {
                            cur_transform.flatten_z_output();
                        }
                        let world_transform = cur_transform.then(&parent_system.world_transform);
                        let determinant = world_transform.determinant();
                        self.invertible = determinant != 0.0 && !determinant.is_nan();

                        CoordinateSystem {
                            transform,
                            world_transform,
                            should_flatten: match (info.transform_style, info.kind) {
                                (TransformStyle::Flat, ReferenceFrameKind::Transform { .. }) => true,
                                (_, _) => false,
                            },
                            parent: Some(state.current_coordinate_system_id),
                        }
                    };
                    coordinate_system_id = CoordinateSystemId(coord_systems.len() as u32);
                    coord_systems.push(coord_system);
                }

                // Ensure that the current coordinate system ID is propagated to child
                // nodes, even if we encounter a node that is not invertible. This ensures
                // that the invariant in get_relative_transform is not violated.
                self.coordinate_system_id = coordinate_system_id;
                self.viewport_transform = cs_scale_offset;
                self.content_transform = cs_scale_offset;
            }
            _ => {
                // We calculate this here to avoid a double-borrow later.
                let sticky_offset = self.calculate_sticky_offset(
                    &state.nearest_scrolling_ancestor_offset,
                    &state.nearest_scrolling_ancestor_viewport,
                );

                // The transformation for the bounds of our viewport is the parent reference frame
                // transform, plus any accumulated scroll offset from our parents, plus any offset
                // provided by our own sticky positioning.
                let accumulated_offset = state.parent_accumulated_scroll_offset + sticky_offset;
                self.viewport_transform = state.coordinate_system_relative_scale_offset
                    .offset(snap_offset(accumulated_offset, state.coordinate_system_relative_scale_offset.scale).to_untyped());

                // The transformation for any content inside of us is the viewport transformation, plus
                // whatever scrolling offset we supply as well.
                let added_offset = accumulated_offset + self.scroll_offset();
                self.content_transform = state.coordinate_system_relative_scale_offset
                    .offset(snap_offset(added_offset, state.coordinate_system_relative_scale_offset.scale).to_untyped());

                if let SpatialNodeType::StickyFrame(ref mut info) = self.node_type {
                    info.current_offset = sticky_offset;
                }

                self.coordinate_system_id = state.current_coordinate_system_id;
            }
        }

        //TODO: remove the field entirely?
        self.transform_kind = if self.coordinate_system_id.0 == 0 {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        };
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
        let mut sticky_rect = info.frame_rect.translate(*viewport_scroll_offset);

        let mut sticky_offset = LayoutVector2D::zero();
        if let Some(margin) = info.margins.top {
            let top_viewport_edge = viewport_rect.min.y + margin;
            if sticky_rect.min.y < top_viewport_edge {
                // If the sticky rect is positioned above the top edge of the viewport (plus margin)
                // we move it down so that it is fully inside the viewport.
                sticky_offset.y = top_viewport_edge - sticky_rect.min.y;
            } else if info.previously_applied_offset.y > 0.0 &&
                sticky_rect.min.y > top_viewport_edge {
                // However, if the sticky rect is positioned *below* the top edge of the viewport
                // and there is already some offset applied to the sticky rect's position, then
                // we need to move it up so that it remains at the correct position. This
                // makes sticky_offset.y negative and effectively reduces the amount of the
                // offset that was already applied. We limit the reduction so that it can, at most,
                // cancel out the already-applied offset, but should never end up adjusting the
                // position the other way.
                sticky_offset.y = top_viewport_edge - sticky_rect.min.y;
                sticky_offset.y = sticky_offset.y.max(-info.previously_applied_offset.y);
            }
        }

        // If we don't have a sticky-top offset (sticky_offset.y + info.previously_applied_offset.y
        // == 0), or if we have a previously-applied bottom offset (previously_applied_offset.y < 0)
        // then we check for handling the bottom margin case. Note that the "don't have a sticky-top
        // offset" case includes the case where we *had* a sticky-top offset but we reduced it to
        // zero in the above block.
        if sticky_offset.y + info.previously_applied_offset.y <= 0.0 {
            if let Some(margin) = info.margins.bottom {
                // If sticky_offset.y is nonzero that means we must have set it
                // in the sticky-top handling code above, so this item must have
                // both top and bottom sticky margins. We adjust the item's rect
                // by the top-sticky offset, and then combine any offset from
                // the bottom-sticky calculation into sticky_offset below.
                sticky_rect.min.y += sticky_offset.y;
                sticky_rect.max.y += sticky_offset.y;

                // Same as the above case, but inverted for bottom-sticky items. Here
                // we adjust items upwards, resulting in a negative sticky_offset.y,
                // or reduce the already-present upward adjustment, resulting in a positive
                // sticky_offset.y.
                let bottom_viewport_edge = viewport_rect.max.y - margin;
                if sticky_rect.max.y > bottom_viewport_edge {
                    sticky_offset.y += bottom_viewport_edge - sticky_rect.max.y;
                } else if info.previously_applied_offset.y < 0.0 &&
                    sticky_rect.max.y < bottom_viewport_edge {
                    sticky_offset.y += bottom_viewport_edge - sticky_rect.max.y;
                    sticky_offset.y = sticky_offset.y.min(-info.previously_applied_offset.y);
                }
            }
        }

        // Same as above, but for the x-axis.
        if let Some(margin) = info.margins.left {
            let left_viewport_edge = viewport_rect.min.x + margin;
            if sticky_rect.min.x < left_viewport_edge {
                sticky_offset.x = left_viewport_edge - sticky_rect.min.x;
            } else if info.previously_applied_offset.x > 0.0 &&
                sticky_rect.min.x > left_viewport_edge {
                sticky_offset.x = left_viewport_edge - sticky_rect.min.x;
                sticky_offset.x = sticky_offset.x.max(-info.previously_applied_offset.x);
            }
        }

        if sticky_offset.x + info.previously_applied_offset.x <= 0.0 {
            if let Some(margin) = info.margins.right {
                sticky_rect.min.x += sticky_offset.x;
                sticky_rect.max.x += sticky_offset.x;
                let right_viewport_edge = viewport_rect.max.x - margin;
                if sticky_rect.max.x > right_viewport_edge {
                    sticky_offset.x += right_viewport_edge - sticky_rect.max.x;
                } else if info.previously_applied_offset.x < 0.0 &&
                    sticky_rect.max.x < right_viewport_edge {
                    sticky_offset.x += right_viewport_edge - sticky_rect.max.x;
                    sticky_offset.x = sticky_offset.x.min(-info.previously_applied_offset.x);
                }
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
        state.current_coordinate_system_id = self.coordinate_system_id;
        state.is_ancestor_or_self_zooming = self.is_async_zooming;
        state.invertible &= self.invertible;

        // The transformation we are passing is the transformation of the parent
        // reference frame and the offset is the accumulated offset of all the nodes
        // between us and the parent reference frame. If we are a reference frame,
        // we need to reset both these values.
        match self.node_type {
            SpatialNodeType::StickyFrame(ref info) => {
                // We don't translate the combined rect by the sticky offset, because sticky
                // offsets actually adjust the node position itself, whereas scroll offsets
                // only apply to contents inside the node.
                state.parent_accumulated_scroll_offset += info.current_offset;
                // We want nested sticky items to take into account the shift
                // we applied as well.
                state.nearest_scrolling_ancestor_offset += info.current_offset;
                state.preserves_3d = false;
                state.external_id = None;
                state.scroll_offset = info.current_offset;
            }
            SpatialNodeType::ScrollFrame(ref scrolling) => {
                state.parent_accumulated_scroll_offset += scrolling.offset();
                state.nearest_scrolling_ancestor_offset = scrolling.offset();
                state.nearest_scrolling_ancestor_viewport = scrolling.viewport_rect;
                state.preserves_3d = false;
                state.external_id = Some(scrolling.external_id);
                state.scroll_offset = scrolling.offset() + scrolling.external_scroll_offset;
            }
            SpatialNodeType::ReferenceFrame(ref info) => {
                state.external_id = None;
                state.scroll_offset = LayoutVector2D::zero();
                state.preserves_3d = info.transform_style == TransformStyle::Preserve3D;
                state.parent_accumulated_scroll_offset = LayoutVector2D::zero();
                state.coordinate_system_relative_scale_offset = self.content_transform;
                let translation = -info.origin_in_parent_reference_frame;
                state.nearest_scrolling_ancestor_viewport =
                    state.nearest_scrolling_ancestor_viewport
                       .translate(translation);
            }
        }
    }

    pub fn scroll_offset(&self) -> LayoutVector2D {
        match self.node_type {
            SpatialNodeType::ScrollFrame(ref scrolling) => scrolling.offset(),
            _ => LayoutVector2D::zero(),
        }
    }

    pub fn matches_external_id(&self, external_id: ExternalScrollId) -> bool {
        match self.node_type {
            SpatialNodeType::ScrollFrame(ref info) if info.external_id == external_id => true,
            _ => false,
        }
    }

    /// Returns true for ReferenceFrames whose source_transform is
    /// bound to the property binding id.
    pub fn is_transform_bound_to_property(&self, id: PropertyBindingId) -> bool {
        if let SpatialNodeType::ReferenceFrame(ref info) = self.node_type {
            if let PropertyBinding::Binding(key, _) = info.source_transform {
                id == key.id
            } else {
                false
            }
        } else {
            false
        }
    }
}

/// Defines whether we have an implicit scroll frame for a pipeline root,
/// or an explicitly defined scroll frame from the display list.
#[derive(Copy, Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum ScrollFrameKind {
    PipelineRoot {
        is_root_pipeline: bool,
    },
    Explicit,
}

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ScrollFrameInfo {
    /// The rectangle of the viewport of this scroll frame. This is important for
    /// positioning of items inside child StickyFrames.
    pub viewport_rect: LayoutRect,

    /// Amount that this ScrollFrame can scroll in both directions.
    pub scrollable_size: LayoutSize,

    /// An external id to identify this scroll frame to API clients. This
    /// allows setting scroll positions via the API without relying on ClipsIds
    /// which may change between frames.
    pub external_id: ExternalScrollId,

    /// Stores whether this is a scroll frame added implicitly by WR when adding
    /// a pipeline (either the root or an iframe). We need to exclude these
    /// when searching for scroll roots we care about for picture caching.
    /// TODO(gw): I think we can actually completely remove the implicit
    ///           scroll frame being added by WR, and rely on the embedder
    ///           to define scroll frames. However, that involves API changes
    ///           so we will use this as a temporary hack!
    pub frame_kind: ScrollFrameKind,

    /// Amount that visual components attached to this scroll node have been
    /// pre-scrolled in their local coordinates.
    pub external_scroll_offset: LayoutVector2D,

    /// A set of a pair of negated scroll offset and scroll generation of this
    /// scroll node. The negated scroll offset is including the pre-scrolled
    /// amount. If, for example, a scroll node was pre-scrolled to y=10 (10
    /// pixels down from the initial unscrolled position), then
    /// `external_scroll_offset` would be (0,10), and this `offset` field would
    /// be (0,-10). If WebRender is then asked to change the scroll position by
    /// an additional 10 pixels (without changing the pre-scroll amount in the
    /// display list), `external_scroll_offset` would remain at (0,10) and
    /// `offset` would change to (0,-20).
    pub offsets: Vec<SampledScrollOffset>,

    /// The generation of the external_scroll_offset.
    /// This is used to pick up the most appropriate scroll offset sampled
    /// off the main thread.
    pub offset_generation: APZScrollGeneration,

    /// Whether the document containing this scroll frame has any scroll-linked
    /// effect or not.
    pub has_scroll_linked_effect: HasScrollLinkedEffect,
}

/// Manages scrolling offset.
impl ScrollFrameInfo {
    pub fn new(
        viewport_rect: LayoutRect,
        scrollable_size: LayoutSize,
        external_id: ExternalScrollId,
        frame_kind: ScrollFrameKind,
        external_scroll_offset: LayoutVector2D,
        offset_generation: APZScrollGeneration,
        has_scroll_linked_effect: HasScrollLinkedEffect,
    ) -> ScrollFrameInfo {
        ScrollFrameInfo {
            viewport_rect,
            scrollable_size,
            external_id,
            frame_kind,
            external_scroll_offset,
            offsets: vec![SampledScrollOffset{
                // If this scroll frame is a newly created one, using
                // `external_scroll_offset` and `offset_generation` is correct.
                // If this scroll frame is a result of updating an existing
                // scroll frame and if there have already been sampled async
                // scroll offsets by APZ, then these offsets will be replaced in
                // SpatialTree::set_scroll_offsets via a
                // RenderBackend::update_document call.
                offset: -external_scroll_offset,
                generation: offset_generation.clone(),
            }],
            offset_generation,
            has_scroll_linked_effect,
        }
    }

    pub fn offset(&self) -> LayoutVector2D {
        debug_assert!(self.offsets.len() > 0, "There should be at least one sampled offset!");

        if self.has_scroll_linked_effect == HasScrollLinkedEffect::No {
            // If there's no scroll-linked effect, use the one-frame delay offset.
            return self.offsets.first().map_or(LayoutVector2D::zero(), |sampled| sampled.offset);
        }

        match self.offsets.iter().find(|sampled| sampled.generation == self.offset_generation) {
            // If we found an offset having the same generation, use it.
            Some(sampled) => sampled.offset,
            // If we don't have any offset having the same generation, i.e.
            // the generation of this scroll frame is behind sampled offsets,
            // use the first queued sampled offset.
            _ => self.offsets.first().map_or(LayoutVector2D::zero(), |sampled| sampled.offset),
        }
    }
}

/// Contains information about reference frames.
#[derive(Copy, Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ReferenceFrameInfo {
    /// The source transform and perspective matrices provided by the stacking context
    /// that forms this reference frame. We maintain the property binding information
    /// here so that we can resolve the animated transform and update the tree each
    /// frame.
    pub source_transform: PropertyBinding<LayoutTransform>,
    pub transform_style: TransformStyle,
    pub kind: ReferenceFrameKind,

    /// The original, not including the transform and relative to the parent reference frame,
    /// origin of this reference frame. This is already rolled into the `transform' property, but
    /// we also store it here to properly transform the viewport for sticky positioning.
    pub origin_in_parent_reference_frame: LayoutVector2D,

    /// True if this is the root reference frame for a given pipeline. This is only used
    /// by the hit-test code, perhaps we can change the interface to not require this.
    pub is_pipeline_root: bool,
}

#[derive(Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct StickyFrameInfo {
    pub frame_rect: LayoutRect,
    pub margins: SideOffsets2D<Option<f32>, LayoutPixel>,
    pub vertical_offset_bounds: StickyOffsetBounds,
    pub horizontal_offset_bounds: StickyOffsetBounds,
    pub previously_applied_offset: LayoutVector2D,
    pub current_offset: LayoutVector2D,
}

impl StickyFrameInfo {
    pub fn new(
        frame_rect: LayoutRect,
        margins: SideOffsets2D<Option<f32>, LayoutPixel>,
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

#[test]
fn test_cst_perspective_relative_scroll() {
    // Verify that when computing the offset from a perspective transform
    // to a relative scroll node that any external scroll offset is
    // ignored. This is because external scroll offsets are not
    // propagated across reference frame boundaries.

    // It's not currently possible to verify this with a wrench reftest,
    // since wrench doesn't understand external scroll ids. When wrench
    // supports this, we could also verify with a reftest.

    use crate::spatial_tree::{SceneSpatialTree, SpatialTree};
    use euclid::Angle;

    let mut cst = SceneSpatialTree::new();
    let pipeline_id = PipelineId::dummy();
    let ext_scroll_id = ExternalScrollId(1, pipeline_id);
    let transform = LayoutTransform::rotation(0.0, 0.0, 1.0, Angle::degrees(45.0));
    let pid = PipelineInstanceId::new(0);

    let root = cst.add_reference_frame(
        cst.root_reference_frame_index(),
        TransformStyle::Flat,
        PropertyBinding::Value(LayoutTransform::identity()),
        ReferenceFrameKind::Transform {
            is_2d_scale_translation: false,
            should_snap: false,
            paired_with_perspective: false,
        },
        LayoutVector2D::zero(),
        pipeline_id,
        SpatialNodeUid::external(SpatialTreeItemKey::new(0, 0), PipelineId::dummy(), pid),
    );

    let scroll_frame_1 = cst.add_scroll_frame(
        root,
        ext_scroll_id,
        pipeline_id,
        &LayoutRect::from_size(LayoutSize::new(100.0, 100.0)),
        &LayoutSize::new(100.0, 500.0),
        ScrollFrameKind::Explicit,
        LayoutVector2D::zero(),
        APZScrollGeneration::default(),
        HasScrollLinkedEffect::No,
        SpatialNodeUid::external(SpatialTreeItemKey::new(0, 1), PipelineId::dummy(), pid),
    );

    let scroll_frame_2 = cst.add_scroll_frame(
        scroll_frame_1,
        ExternalScrollId(2, pipeline_id),
        pipeline_id,
        &LayoutRect::from_size(LayoutSize::new(100.0, 100.0)),
        &LayoutSize::new(100.0, 500.0),
        ScrollFrameKind::Explicit,
        LayoutVector2D::new(0.0, 50.0),
        APZScrollGeneration::default(),
        HasScrollLinkedEffect::No,
        SpatialNodeUid::external(SpatialTreeItemKey::new(0, 3), PipelineId::dummy(), pid),
    );

    let ref_frame = cst.add_reference_frame(
        scroll_frame_2,
        TransformStyle::Preserve3D,
        PropertyBinding::Value(transform),
        ReferenceFrameKind::Perspective {
            scrolling_relative_to: Some(ext_scroll_id),
        },
        LayoutVector2D::zero(),
        pipeline_id,
        SpatialNodeUid::external(SpatialTreeItemKey::new(0, 4), PipelineId::dummy(), pid),
    );

    let mut st = SpatialTree::new();
    st.apply_updates(cst.end_frame_and_get_pending_updates());
    st.update_tree(&SceneProperties::new());

    let world_transform = st.get_world_transform(ref_frame).into_transform().cast_unit();
    let ref_transform = transform.then_translate(LayoutVector3D::new(0.0, -50.0, 0.0));
    assert!(world_transform.approx_eq(&ref_transform));
}


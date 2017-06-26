/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use geometry::ray_intersects_rect;
use mask_cache::{ClipSource, MaskCacheInfo, RegionMode};
use prim_store::GpuBlock32;
use renderer::VertexDataStore;
use spring::{DAMPING, STIFFNESS, Spring};
use tiling::PackedLayerIndex;
use util::TransformedRectKind;
use webrender_traits::{ClipId, ClipRegion, DeviceIntRect, LayerPixel, LayerPoint, LayerRect};
use webrender_traits::{LayerSize, LayerToScrollTransform, LayerToWorldTransform, PipelineId};
use webrender_traits::{ScrollClamping, ScrollEventPhase, ScrollLayerRect, ScrollLocation};
use webrender_traits::{WorldPoint, LayerVector2D};
use webrender_traits::{as_scroll_parent_vector};

#[cfg(target_os = "macos")]
const CAN_OVERSCROLL: bool = true;

#[cfg(not(target_os = "macos"))]
const CAN_OVERSCROLL: bool = false;

#[derive(Clone, Debug)]
pub struct ClipInfo {
    /// The ClipSource for this node, which is used to generate mask_cache_info.
    pub clip_sources: Vec<ClipSource>,

    /// The MaskCacheInfo for this node, which is produced by processing the
    /// provided ClipSource.
    pub mask_cache_info: Option<MaskCacheInfo>,

    /// The packed layer index for this node, which is used to render a clip mask
    /// for it, if necessary.
    pub packed_layer_index: PackedLayerIndex,

    /// The final transformed rectangle of this clipping region for this node,
    /// which depends on the screen rectangle and the transformation of all of
    /// the parents.
    pub screen_bounding_rect: Option<(TransformedRectKind, DeviceIntRect)>,
}

impl ClipInfo {
    pub fn new(clip_region: &ClipRegion,
               clip_store: &mut VertexDataStore<GpuBlock32>,
               packed_layer_index: PackedLayerIndex,)
               -> ClipInfo {
        // We pass true here for the MaskCacheInfo because this type of
        // mask needs an extra clip for the clip rectangle.
        let clip_sources = vec![ClipSource::Region(clip_region.clone(), RegionMode::IncludeRect)];
        ClipInfo {
            mask_cache_info: MaskCacheInfo::new(&clip_sources, clip_store),
            clip_sources: clip_sources,
            packed_layer_index: packed_layer_index,
            screen_bounding_rect: None,
        }
    }

    pub fn is_masking(&self) -> bool {
        match self.mask_cache_info {
            Some(ref info) => info.is_masking(),
            _ => false,
        }
    }

}
#[derive(Clone, Debug)]
pub enum NodeType {
    /// Transform for this layer, relative to parent reference frame. A reference
    /// frame establishes a new coordinate space in the tree.
    ReferenceFrame(LayerToScrollTransform),

    /// Other nodes just do clipping, but no transformation.
    Clip(ClipInfo),

    /// Other nodes just do clipping, but no transformation.
    ScrollFrame(ScrollingState),
}

/// Contains information common among all types of ClipScrollTree nodes.
#[derive(Clone, Debug)]
pub struct ClipScrollNode {
    /// Size of the content inside the scroll region (in logical pixels)
    pub content_size: LayerSize,

    /// Viewing rectangle in the coordinate system of the parent reference frame.
    pub local_viewport_rect: LayerRect,

    /// Clip rect of this node - typically the same as viewport rect, except
    /// in overscroll cases.
    pub local_clip_rect: LayerRect,

    /// Viewport rectangle clipped against parent layer(s) viewport rectangles.
    /// This is in the coordinate system which starts at our origin.
    pub combined_local_viewport_rect: LayerRect,

    /// World transform for the viewport rect itself. This is the parent
    /// reference frame transformation plus the scrolling offsets provided by
    /// the nodes in between the reference frame and this node.
    pub world_viewport_transform: LayerToWorldTransform,

    /// World transform for content transformed by this node.
    pub world_content_transform: LayerToWorldTransform,

    /// The scroll offset of all the nodes between us and our parent reference frame.
    /// This is used to calculate intersections between us and content or nodes that
    /// are also direct children of our reference frame.
    pub reference_frame_relative_scroll_offset: LayerVector2D,

    /// Pipeline that this layer belongs to
    pub pipeline_id: PipelineId,

    /// Parent layer. If this is None, we are the root node.
    pub parent: Option<ClipId>,

    /// Child layers
    pub children: Vec<ClipId>,

    /// Whether or not this node is a reference frame.
    pub node_type: NodeType,
}

impl ClipScrollNode {
    pub fn new_scroll_frame(pipeline_id: PipelineId,
                            parent_id: ClipId,
                            content_rect: &LayerRect,
                            frame_rect: &LayerRect)
                            -> ClipScrollNode {
        ClipScrollNode {
            content_size: content_rect.size,
            local_viewport_rect: *frame_rect,
            local_clip_rect: *frame_rect,
            combined_local_viewport_rect: LayerRect::zero(),
            world_viewport_transform: LayerToWorldTransform::identity(),
            world_content_transform: LayerToWorldTransform::identity(),
            reference_frame_relative_scroll_offset: LayerVector2D::zero(),
            parent: Some(parent_id),
            children: Vec::new(),
            pipeline_id: pipeline_id,
            node_type: NodeType::ScrollFrame(ScrollingState::new()),
        }
    }

    pub fn new(pipeline_id: PipelineId,
               parent_id: ClipId,
               content_rect: &LayerRect,
               clip_rect: &LayerRect,
               clip_info: ClipInfo)
               -> ClipScrollNode {
        // FIXME(mrobinson): We don't yet handle clipping rectangles that don't start at the origin
        // of the node.
        let local_viewport_rect = LayerRect::new(content_rect.origin, clip_rect.size);
        ClipScrollNode {
            content_size: content_rect.size,
            local_viewport_rect: local_viewport_rect,
            local_clip_rect: local_viewport_rect,
            combined_local_viewport_rect: LayerRect::zero(),
            world_viewport_transform: LayerToWorldTransform::identity(),
            world_content_transform: LayerToWorldTransform::identity(),
            reference_frame_relative_scroll_offset: LayerVector2D::zero(),
            parent: Some(parent_id),
            children: Vec::new(),
            pipeline_id: pipeline_id,
            node_type: NodeType::Clip(clip_info),
        }
    }

    pub fn new_reference_frame(parent_id: Option<ClipId>,
                               local_viewport_rect: &LayerRect,
                               content_size: LayerSize,
                               local_transform: &LayerToScrollTransform,
                               pipeline_id: PipelineId)
                               -> ClipScrollNode {
        ClipScrollNode {
            content_size: content_size,
            local_viewport_rect: *local_viewport_rect,
            local_clip_rect: *local_viewport_rect,
            combined_local_viewport_rect: LayerRect::zero(),
            world_viewport_transform: LayerToWorldTransform::identity(),
            world_content_transform: LayerToWorldTransform::identity(),
            reference_frame_relative_scroll_offset: LayerVector2D::zero(),
            parent: parent_id,
            children: Vec::new(),
            pipeline_id: pipeline_id,
            node_type: NodeType::ReferenceFrame(*local_transform),
        }
    }

    pub fn add_child(&mut self, child: ClipId) {
        self.children.push(child);
    }

    pub fn finalize(&mut self, new_scrolling: &ScrollingState) {
        match self.node_type {
            NodeType::ReferenceFrame(_) | NodeType::Clip(_) => (),
            NodeType::ScrollFrame(ref mut scrolling) => *scrolling = *new_scrolling,
        }
    }

    pub fn set_scroll_origin(&mut self, origin: &LayerPoint, clamp: ScrollClamping) -> bool {
        let scrollable_height = self.scrollable_height();
        let scrollable_width = self.scrollable_width();

        let scrolling = match self.node_type {
            NodeType::ReferenceFrame(_) | NodeType::Clip(_) => {
                warn!("Tried to scroll a non-scroll node.");
                return false;
            }
             NodeType::ScrollFrame(ref mut scrolling) => scrolling,
        };

        let new_offset = match clamp {
            ScrollClamping::ToContentBounds => {
                if scrollable_height <= 0. && scrollable_width <= 0. {
                    return false;
                }

                let origin = LayerPoint::new(origin.x.max(0.0), origin.y.max(0.0));
                LayerVector2D::new((-origin.x).max(-scrollable_width).min(0.0).round(),
                                   (-origin.y).max(-scrollable_height).min(0.0).round())
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

    pub fn update_transform(&mut self,
                            parent_reference_frame_transform: &LayerToWorldTransform,
                            parent_combined_viewport_rect: &ScrollLayerRect,
                            parent_scroll_offset: LayerVector2D,
                            parent_accumulated_scroll_offset: LayerVector2D) {
        self.reference_frame_relative_scroll_offset = match self.node_type {
            NodeType::ReferenceFrame(_) => LayerVector2D::zero(),
            NodeType::Clip(_) | NodeType::ScrollFrame(..) => parent_accumulated_scroll_offset,
        };

        let local_transform = match self.node_type {
            NodeType::ReferenceFrame(transform) => transform,
            NodeType::Clip(_) | NodeType::ScrollFrame(..) => LayerToScrollTransform::identity(),
        };

        let inv_transform = match local_transform.inverse() {
            Some(transform) => transform,
            None => {
                // If a transform function causes the current transformation matrix of an object
                // to be non-invertible, the object and its content do not get displayed.
                self.combined_local_viewport_rect = LayerRect::zero();
                return;
            }
        };

        // We are trying to move the combined viewport rectangle of our parent nodes into the
        // coordinate system of this node, so we must invert our transformation (only for
        // reference frames) and then apply the scroll offset the parent layer. The combined
        // local viewport rect doesn't include scrolling offsets so the only one that matters
        // is the relative offset between us and the parent.
        let parent_combined_viewport_in_local_space =
            inv_transform.pre_translate(-as_scroll_parent_vector(&parent_scroll_offset).to_3d())
                         .transform_rect(parent_combined_viewport_rect);

        // Now that we have the combined viewport rectangle of the parent nodes in local space,
        // we do the intersection and get our combined viewport rect in the coordinate system
        // starting from our origin.
        self.combined_local_viewport_rect = match self.node_type {
            NodeType::Clip(_) | NodeType::ScrollFrame(..) => {
                parent_combined_viewport_in_local_space.intersection(&self.local_clip_rect)
                                                       .unwrap_or(LayerRect::zero())
            }
            NodeType::ReferenceFrame(_) => parent_combined_viewport_in_local_space,
        };

        // HACK: prevent the code above for non-AA transforms, it's incorrect.
        if (local_transform.m13, local_transform.m23) != (0.0, 0.0) {
            self.combined_local_viewport_rect = self.local_clip_rect;
        }

        // The transformation for this viewport in world coordinates is the transformation for
        // our parent reference frame, plus any accumulated scrolling offsets from nodes
        // between our reference frame and this node. For reference frames, we also include
        // whatever local transformation this reference frame provides. This can be combined
        // with the local_viewport_rect to get its position in world space.
        self.world_viewport_transform =
            parent_reference_frame_transform
                .pre_translate(parent_accumulated_scroll_offset.to_3d())
                .pre_mul(&local_transform.with_destination::<LayerPixel>());

        // The transformation for any content inside of us is the viewport transformation, plus
        // whatever scrolling offset we supply as well.
        let scroll_offset = self.scroll_offset();
        self.world_content_transform =
            self.world_viewport_transform.pre_translate(scroll_offset.to_3d());
    }

    pub fn scrollable_height(&self) -> f32 {
        self.content_size.height - self.local_viewport_rect.size.height
    }

    pub fn scrollable_width(&self) -> f32 {
        self.content_size.width - self.local_viewport_rect.size.width
    }

    pub fn scroll(&mut self, scroll_location: ScrollLocation, phase: ScrollEventPhase) -> bool {
        let scrollable_width = self.scrollable_width();
        let scrollable_height = self.scrollable_height();

        let scrolling = match self.node_type {
            NodeType::ReferenceFrame(_) | NodeType::Clip(_) => return false,
            NodeType::ScrollFrame(ref mut scrolling) => scrolling,
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
            },
            ScrollLocation::End => {
                let end_pos = self.local_viewport_rect.size.height - self.content_size.height;

                if scrolling.offset.y.round() <= end_pos {
                    // Nothing to do on this layer.
                    return false;
                }

                scrolling.offset.y = end_pos;
                return true;
            }
        };

        let overscroll_amount = scrolling.overscroll_amount(scrollable_width, scrollable_height);
        let overscrolling = CAN_OVERSCROLL && (overscroll_amount.x != 0.0 ||
                                               overscroll_amount.y != 0.0);
        if overscrolling {
            if overscroll_amount.x != 0.0 {
                delta.x /= overscroll_amount.x.abs()
            }
            if overscroll_amount.y != 0.0 {
                delta.y /= overscroll_amount.y.abs()
            }
        }

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
                ((delta.x < 1.0 && delta.y < 1.0) || phase == ScrollEventPhase::End) {
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
        let inv = self.world_viewport_transform.inverse().unwrap();
        let z0 = -10000.0;
        let z1 =  10000.0;

        let p0 = inv.transform_point3d(&cursor.extend(z0));
        let p1 = inv.transform_point3d(&cursor.extend(z1));

        if self.scrollable_width() <= 0. && self.scrollable_height() <= 0. {
            return false;
        }
        ray_intersects_rect(p0.to_untyped(), p1.to_untyped(), self.local_viewport_rect.to_untyped())
    }

    pub fn scroll_offset(&self) -> LayerVector2D {
        match self.node_type {
            NodeType::ScrollFrame(ref scrolling) => scrolling.offset,
            _ => LayerVector2D::zero(),
        }
    }

    pub fn is_overscrolling(&self) -> bool {
        match self.node_type {
            NodeType::ScrollFrame(ref scrolling) => {
                let overscroll_amount = scrolling.overscroll_amount(self.scrollable_width(),
                                                                    self.scrollable_height());
                overscroll_amount.x != 0.0 || overscroll_amount.y != 0.0
            }
            _ => false,
        }
    }
}

#[derive(Copy, Clone, Debug)]
pub struct ScrollingState {
    pub offset: LayerVector2D,
    pub spring: Spring,
    pub started_bouncing_back: bool,
    pub bouncing_back: bool,
    pub should_handoff_scroll: bool
}

/// Manages scrolling offset, overscroll state, etc.
impl ScrollingState {
    pub fn new() -> ScrollingState {
        ScrollingState {
            offset: LayerVector2D::zero(),
            spring: Spring::at(LayerPoint::zero(), STIFFNESS, DAMPING),
            started_bouncing_back: false,
            bouncing_back: false,
            should_handoff_scroll: false
        }
    }

    pub fn stretch_overscroll_spring(&mut self, overscroll_amount: LayerVector2D) {
        let offset = self.offset.to_point();
        self.spring.coords(offset, offset, offset + overscroll_amount);
    }

    pub fn tick_scrolling_bounce_animation(&mut self) {
        let finished = self.spring.animate();
        self.offset = self.spring.current().to_vector();
        if finished {
            self.bouncing_back = false
        }
    }

    pub fn overscroll_amount(&self,
                             scrollable_width: f32,
                             scrollable_height: f32)
                             -> LayerVector2D {
        let overscroll_x = if self.offset.x > 0.0 {
            -self.offset.x
        } else if self.offset.x < -scrollable_width {
            -scrollable_width - self.offset.x
        } else {
            0.0
        };

        let overscroll_y = if self.offset.y > 0.0 {
            -self.offset.y
        } else if self.offset.y < -scrollable_height {
            -scrollable_height - self.offset.y
        } else {
            0.0
        };

        LayerVector2D::new(overscroll_x, overscroll_y)
    }
}


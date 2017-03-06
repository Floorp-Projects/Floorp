/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::Point3D;
use geometry::ray_intersects_rect;
use spring::{DAMPING, STIFFNESS, Spring};
use webrender_traits::{LayerPixel, LayerPoint, LayerRect, LayerSize, LayerToScrollTransform};
use webrender_traits::{LayerToWorldTransform, PipelineId, ScrollEventPhase, ScrollLayerId};
use webrender_traits::{ScrollLayerRect, ScrollLocation, WorldPoint, WorldPoint4D};

#[cfg(target_os = "macos")]
const CAN_OVERSCROLL: bool = true;

#[cfg(not(target_os = "macos"))]
const CAN_OVERSCROLL: bool = false;

#[derive(Clone)]
pub enum NodeType {
    /// Transform for this layer, relative to parent reference frame. A reference
    /// frame establishes a new coordinate space in the tree.
    ReferenceFrame(LayerToScrollTransform),

    /// Other nodes just do clipping, but no transformation.
    ClipRect,
}

/// Contains scrolling and transform information stacking contexts.
#[derive(Clone)]
pub struct ClipScrollNode {
    /// Manages scrolling offset, overscroll state etc.
    pub scrolling: ScrollingState,

    /// Size of the content inside the scroll region (in logical pixels)
    pub content_size: LayerSize,

    /// Viewing rectangle in the coordinate system of the parent reference frame.
    pub local_viewport_rect: LayerRect,

    /// Clip rect of this node - typically the same as viewport rect, except
    /// in overscroll cases.
    pub local_clip_rect: LayerRect,

    /// Viewport rectangle clipped against parent layer(s) viewport rectangles.
    /// This is in the coordinate system of the parent reference frame.
    pub combined_local_viewport_rect: LayerRect,

    /// World transform for the viewport rect itself. This is the parent
    /// reference frame transformation plus the scrolling offsets provided by
    /// the nodes in between the reference frame and this node.
    pub world_viewport_transform: LayerToWorldTransform,

    /// World transform for content transformed by this node.
    pub world_content_transform: LayerToWorldTransform,

    /// Pipeline that this layer belongs to
    pub pipeline_id: PipelineId,

    /// Child layers
    pub children: Vec<ScrollLayerId>,

    /// Whether or not this node is a reference frame.
    pub node_type: NodeType,
}

impl ClipScrollNode {
    pub fn new(local_viewport_rect: &LayerRect,
               local_clip_rect: &LayerRect,
               content_size: LayerSize,
               pipeline_id: PipelineId)
               -> ClipScrollNode {
        ClipScrollNode {
            scrolling: ScrollingState::new(),
            content_size: content_size,
            local_viewport_rect: *local_viewport_rect,
            local_clip_rect: *local_clip_rect,
            combined_local_viewport_rect: *local_clip_rect,
            world_viewport_transform: LayerToWorldTransform::identity(),
            world_content_transform: LayerToWorldTransform::identity(),
            children: Vec::new(),
            pipeline_id: pipeline_id,
            node_type: NodeType::ClipRect,
        }
    }

    pub fn new_reference_frame(local_viewport_rect: &LayerRect,
                               local_clip_rect: &LayerRect,
                               content_size: LayerSize,
                               local_transform: &LayerToScrollTransform,
                               pipeline_id: PipelineId)
                               -> ClipScrollNode {
        ClipScrollNode {
            scrolling: ScrollingState::new(),
            content_size: content_size,
            local_viewport_rect: *local_viewport_rect,
            local_clip_rect: *local_clip_rect,
            combined_local_viewport_rect: *local_clip_rect,
            world_viewport_transform: LayerToWorldTransform::identity(),
            world_content_transform: LayerToWorldTransform::identity(),
            children: Vec::new(),
            pipeline_id: pipeline_id,
            node_type: NodeType::ReferenceFrame(*local_transform),
        }
    }

    pub fn add_child(&mut self, child: ScrollLayerId) {
        self.children.push(child);
    }

    pub fn finalize(&mut self, scrolling: &ScrollingState) {
        self.scrolling = *scrolling;
    }

    pub fn overscroll_amount(&self) -> LayerSize {
        let scrollable_width = self.scrollable_width();
        let overscroll_x = if self.scrolling.offset.x > 0.0 {
            -self.scrolling.offset.x
        } else if self.scrolling.offset.x < -scrollable_width {
            -scrollable_width - self.scrolling.offset.x
        } else {
            0.0
        };

        let scrollable_height = self.scrollable_height();
        let overscroll_y = if self.scrolling.offset.y > 0.0 {
            -self.scrolling.offset.y
        } else if self.scrolling.offset.y < -scrollable_height {
            -scrollable_height - self.scrolling.offset.y
        } else {
            0.0
        };

        LayerSize::new(overscroll_x, overscroll_y)
    }

    pub fn set_scroll_origin(&mut self, origin: &LayerPoint) -> bool {
        let scrollable_height = self.scrollable_height();
        let scrollable_width = self.scrollable_width();
        if scrollable_height <= 0. && scrollable_width <= 0. {
            return false;
        }

        let new_offset = LayerPoint::new((-origin.x).max(-scrollable_width).min(0.0).round(),
                                         (-origin.y).max(-scrollable_height).min(0.0).round());
        if new_offset == self.scrolling.offset {
            return false;
        }

        self.scrolling.offset = new_offset;
        self.scrolling.bouncing_back = false;
        self.scrolling.started_bouncing_back = false;
        return true;
    }

    pub fn update_transform(&mut self,
                            parent_reference_frame_transform: &LayerToWorldTransform,
                            parent_combined_viewport_rect: &ScrollLayerRect,
                            parent_accumulated_scroll_offset: LayerPoint) {

        let local_transform = match self.node_type {
            NodeType::ReferenceFrame(transform) => transform,
            NodeType::ClipRect => LayerToScrollTransform::identity(),
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
        // reference frames) and then apply the scroll offset of all the parent layers.
        let parent_combined_viewport_in_local_space =
            inv_transform.pre_translated(-parent_accumulated_scroll_offset.x,
                                         -parent_accumulated_scroll_offset.y,
                                         0.0)
                         .transform_rect(parent_combined_viewport_rect);

        // Now that we have the combined viewport rectangle of the parent nodes in local space,
        // we do the intersection and get our combined viewport rect in the coordinate system
        // starting from our origin.
        self.combined_local_viewport_rect =
            parent_combined_viewport_in_local_space.intersection(&self.local_clip_rect)
                                                    .unwrap_or(LayerRect::zero());

        // The transformation for this viewport in world coordinates is the transformation for
        // our parent reference frame, plus any accumulated scrolling offsets from nodes
        // between our reference frame and this node. For reference frames, we also include
        // whatever local transformation this reference frame provides. This can be combined
        // with the local_viewport_rect to get its position in world space.
        self.world_viewport_transform =
            parent_reference_frame_transform
                .pre_translated(parent_accumulated_scroll_offset.x,
                                parent_accumulated_scroll_offset.y,
                                0.0)
                .pre_mul(&local_transform.with_destination::<LayerPixel>());

        // The transformation for any content inside of us is the viewport transformation, plus
        // whatever scrolling offset we supply as well.
        self.world_content_transform =
            self.world_viewport_transform.pre_translated(self.scrolling.offset.x,
                                                         self.scrolling.offset.y,
                                                         0.0);
    }

    pub fn scrollable_height(&self) -> f32 {
        self.content_size.height - self.local_viewport_rect.size.height
    }

    pub fn scrollable_width(&self) -> f32 {
        self.content_size.width - self.local_viewport_rect.size.width
    }

    pub fn scroll(&mut self, scroll_location: ScrollLocation, phase: ScrollEventPhase) -> bool {
        if self.scrolling.started_bouncing_back && phase == ScrollEventPhase::Move(false) {
            return false;
        }

        let mut delta = match scroll_location {
            ScrollLocation::Delta(delta) => delta,
            ScrollLocation::Start => {
                if self.scrolling.offset.y.round() >= 0.0 {
                    // Nothing to do on this layer.
                    return false;
                }

                self.scrolling.offset.y = 0.0;
                return true;
            },
            ScrollLocation::End => {
                let end_pos = self.local_viewport_rect.size.height - self.content_size.height;

                if self.scrolling.offset.y.round() <= end_pos {
                    // Nothing to do on this layer.
                    return false;
                }

                self.scrolling.offset.y = end_pos;
                return true;
            }
        };

        let overscroll_amount = self.overscroll_amount();
        let overscrolling = CAN_OVERSCROLL && (overscroll_amount.width != 0.0 ||
                                               overscroll_amount.height != 0.0);
        if overscrolling {
            if overscroll_amount.width != 0.0 {
                delta.x /= overscroll_amount.width.abs()
            }
            if overscroll_amount.height != 0.0 {
                delta.y /= overscroll_amount.height.abs()
            }
        }

        let scrollable_width = self.scrollable_width();
        let scrollable_height = self.scrollable_height();
        let is_unscrollable = scrollable_width <= 0. && scrollable_height <= 0.;
        let original_layer_scroll_offset = self.scrolling.offset;

        if scrollable_width > 0. {
            self.scrolling.offset.x = self.scrolling.offset.x + delta.x;
            if is_unscrollable || !CAN_OVERSCROLL {
                self.scrolling.offset.x =
                    self.scrolling.offset.x.min(0.0).max(-scrollable_width).round();
            }
        }

        if scrollable_height > 0. {
            self.scrolling.offset.y = self.scrolling.offset.y + delta.y;
            if is_unscrollable || !CAN_OVERSCROLL {
                self.scrolling.offset.y =
                    self.scrolling.offset.y.min(0.0).max(-scrollable_height).round();
            }
        }

        if phase == ScrollEventPhase::Start || phase == ScrollEventPhase::Move(true) {
            self.scrolling.started_bouncing_back = false
        } else if overscrolling &&
                ((delta.x < 1.0 && delta.y < 1.0) || phase == ScrollEventPhase::End) {
            self.scrolling.started_bouncing_back = true;
            self.scrolling.bouncing_back = true
        }

        if CAN_OVERSCROLL {
            self.stretch_overscroll_spring();
        }

        self.scrolling.offset != original_layer_scroll_offset ||
            self.scrolling.started_bouncing_back
    }

    pub fn stretch_overscroll_spring(&mut self) {
        let overscroll_amount = self.overscroll_amount();
        self.scrolling.spring.coords(self.scrolling.offset,
                                     self.scrolling.offset,
                                     self.scrolling.offset + overscroll_amount);
    }

    pub fn tick_scrolling_bounce_animation(&mut self) {
        let finished = self.scrolling.spring.animate();
        self.scrolling.offset = self.scrolling.spring.current();
        if finished {
            self.scrolling.bouncing_back = false
        }
    }

    pub fn ray_intersects_node(&self, cursor: &WorldPoint) -> bool {
        let inv = self.world_viewport_transform.inverse().unwrap();
        let z0 = -10000.0;
        let z1 =  10000.0;

        let p0 = inv.transform_point4d(&WorldPoint4D::new(cursor.x, cursor.y, z0, 1.0));
        let p0 = Point3D::new(p0.x / p0.w,
                              p0.y / p0.w,
                              p0.z / p0.w);
        let p1 = inv.transform_point4d(&WorldPoint4D::new(cursor.x, cursor.y, z1, 1.0));
        let p1 = Point3D::new(p1.x / p1.w,
                              p1.y / p1.w,
                              p1.z / p1.w);

        if self.scrollable_width() <= 0. && self.scrollable_height() <= 0. {
            return false;
        }
        ray_intersects_rect(p0, p1, self.local_viewport_rect.to_untyped())
    }
}

#[derive(Copy, Clone)]
pub struct ScrollingState {
    pub offset: LayerPoint,
    pub spring: Spring,
    pub started_bouncing_back: bool,
    pub bouncing_back: bool,
    pub should_handoff_scroll: bool
}

impl ScrollingState {
    pub fn new() -> ScrollingState {
        ScrollingState {
            offset: LayerPoint::zero(),
            spring: Spring::at(LayerPoint::zero(), STIFFNESS, DAMPING),
            started_bouncing_back: false,
            bouncing_back: false,
            should_handoff_scroll: false
        }
    }
}


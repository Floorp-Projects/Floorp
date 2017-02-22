/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::Point3D;
use geometry::ray_intersects_rect;
use spring::{DAMPING, STIFFNESS, Spring};
use webrender_traits::{LayerPoint, LayerRect, LayerSize, LayerToScrollTransform};
use webrender_traits::{LayerToWorldTransform, PipelineId, ScrollEventPhase, ScrollLayerId};
use webrender_traits::{ScrollLayerRect, ScrollLocation, ScrollToWorldTransform, WorldPoint};
use webrender_traits::{WorldPoint4D};

#[cfg(target_os = "macos")]
const CAN_OVERSCROLL: bool = true;

#[cfg(not(target_os = "macos"))]
const CAN_OVERSCROLL: bool = false;


/// Contains scrolling and transform information stacking contexts.
#[derive(Clone)]
pub struct Layer {
    /// Manages scrolling offset, overscroll state etc.
    pub scrolling: ScrollingState,

    /// Size of the content inside the scroll region (in logical pixels)
    pub content_size: LayerSize,

    /// Viewing rectangle
    pub local_viewport_rect: LayerRect,

    /// Viewing rectangle clipped against parent layer(s)
    pub combined_local_viewport_rect: LayerRect,

    /// World transform for the viewport rect itself.
    pub world_viewport_transform: LayerToWorldTransform,

    /// World transform for content within this layer
    pub world_content_transform: LayerToWorldTransform,

    /// Transform for this layer, relative to parent scrollable layer.
    pub local_transform: LayerToScrollTransform,

    /// Pipeline that this layer belongs to
    pub pipeline_id: PipelineId,

    /// Child layers
    pub children: Vec<ScrollLayerId>,
}

impl Layer {
    pub fn new(local_viewport_rect: &LayerRect,
               content_size: LayerSize,
               local_transform: &LayerToScrollTransform,
               pipeline_id: PipelineId)
               -> Layer {
        Layer {
            scrolling: ScrollingState::new(),
            content_size: content_size,
            local_viewport_rect: *local_viewport_rect,
            combined_local_viewport_rect: *local_viewport_rect,
            world_viewport_transform: LayerToWorldTransform::identity(),
            world_content_transform: LayerToWorldTransform::identity(),
            local_transform: *local_transform,
            children: Vec::new(),
            pipeline_id: pipeline_id,
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
                            parent_world_transform: &ScrollToWorldTransform,
                            parent_viewport_rect: &ScrollLayerRect) {
        let inv_transform = match self.local_transform.inverse() {
            Some(transform) => transform,
            None => {
                // If a transform function causes the current transformation matrix of an object
                // to be non-invertible, the object and its content do not get displayed.
                self.combined_local_viewport_rect = LayerRect::zero();
                return;
            }
        };

        let parent_viewport_rect_in_local_space = inv_transform.transform_rect(parent_viewport_rect)
                                                               .translate(&-self.scrolling.offset);
        let local_viewport_rect = self.local_viewport_rect.translate(&-self.scrolling.offset);
        let viewport_rect = parent_viewport_rect_in_local_space.intersection(&local_viewport_rect)
                                                               .unwrap_or(LayerRect::zero());

        self.combined_local_viewport_rect = viewport_rect;
        self.world_viewport_transform = parent_world_transform.pre_mul(&self.local_transform);
        self.world_content_transform = self.world_viewport_transform
                                                     .pre_translated(self.scrolling.offset.x,
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

    pub fn ray_intersects_layer(&self, cursor: &WorldPoint) -> bool {
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


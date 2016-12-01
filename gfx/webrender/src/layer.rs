/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use spring::{DAMPING, STIFFNESS, Spring};
use webrender_traits::{PipelineId, ScrollLayerId};
use webrender_traits::{LayerRect, LayerPoint, LayerSize};
use webrender_traits::{LayerToScrollTransform, LayerToWorldTransform};

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
        let overscroll_x = if self.scrolling.offset.x > 0.0 {
            -self.scrolling.offset.x
        } else if self.scrolling.offset.x < self.local_viewport_rect.size.width - self.content_size.width {
            self.local_viewport_rect.size.width - self.content_size.width - self.scrolling.offset.x
        } else {
            0.0
        };

        let overscroll_y = if self.scrolling.offset.y > 0.0 {
            -self.scrolling.offset.y
        } else if self.scrolling.offset.y < self.local_viewport_rect.size.height -
                self.content_size.height {
            self.local_viewport_rect.size.height - self.content_size.height - self.scrolling.offset.y
        } else {
            0.0
        };

        LayerSize::new(overscroll_x, overscroll_y)
    }

    pub fn set_scroll_origin(&mut self, origin: &LayerPoint) -> bool {
        if self.content_size.width <= self.local_viewport_rect.size.width &&
           self.content_size.height <= self.local_viewport_rect.size.height {
            return false;
        }

        let new_offset = LayerPoint::new(
            (-origin.x).max(-self.content_size.width + self.local_viewport_rect.size.width),
            (-origin.y).max(-self.content_size.height + self.local_viewport_rect.size.height));
        let new_offset = LayerPoint::new(new_offset.x.min(0.0).round(), new_offset.y.min(0.0).round());
        if new_offset == self.scrolling.offset {
            return false;
        }

        self.scrolling.offset = new_offset;
        self.scrolling.bouncing_back = false;
        self.scrolling.started_bouncing_back = false;
        return true;
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
}

#[derive(Copy, Clone)]
pub struct ScrollingState {
    pub offset: LayerPoint,
    pub spring: Spring,
    pub started_bouncing_back: bool,
    pub bouncing_back: bool,
}

impl ScrollingState {
    pub fn new() -> ScrollingState {
        ScrollingState {
            offset: LayerPoint::zero(),
            spring: Spring::at(LayerPoint::zero(), STIFFNESS, DAMPING),
            started_bouncing_back: false,
            bouncing_back: false,
        }
    }
}


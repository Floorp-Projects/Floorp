/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::{Matrix4D, Point2D, Rect, Size2D};
use spring::{DAMPING, STIFFNESS, Spring};
use webrender_traits::{PipelineId, ScrollLayerId};

/// Contains scroll and transform information for scrollable and root stacking contexts.
pub struct Layer {
    /// Manages scrolling offset, overscroll state etc.
    pub scrolling: ScrollingState,

    /// Size of the content inside the scroll region (in logical pixels)
    pub content_size: Size2D<f32>,

    /// Viewing rectangle
    pub local_viewport_rect: Rect<f32>,

    /// Viewing rectangle clipped against parent layer(s)
    pub combined_local_viewport_rect: Rect<f32>,

    /// World transform for the viewport rect itself.
    pub world_viewport_transform: Matrix4D<f32>,

    /// World transform for content within this layer
    pub world_content_transform: Matrix4D<f32>,

    /// Transform for this layer, relative to parent layer.
    pub local_transform: Matrix4D<f32>,

    /// Pipeline that this layer belongs to
    pub pipeline_id: PipelineId,

    /// Child layers
    pub children: Vec<ScrollLayerId>,
}

impl Layer {
    pub fn new(local_viewport_rect: &Rect<f32>,
               content_size: Size2D<f32>,
               local_transform: &Matrix4D<f32>,
               pipeline_id: PipelineId)
               -> Layer {
        Layer {
            scrolling: ScrollingState::new(),
            content_size: content_size,
            local_viewport_rect: *local_viewport_rect,
            combined_local_viewport_rect: *local_viewport_rect,
            world_viewport_transform: Matrix4D::identity(),
            world_content_transform: Matrix4D::identity(),
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

    pub fn overscroll_amount(&self) -> Size2D<f32> {
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

        Size2D::new(overscroll_x, overscroll_y)
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
    pub offset: Point2D<f32>,
    pub spring: Spring,
    pub started_bouncing_back: bool,
    pub bouncing_back: bool,
}

impl ScrollingState {
    pub fn new() -> ScrollingState {
        ScrollingState {
            offset: Point2D::new(0.0, 0.0),
            spring: Spring::at(Point2D::new(0.0, 0.0), STIFFNESS, DAMPING),
            started_bouncing_back: false,
            bouncing_back: false,
        }
    }
}


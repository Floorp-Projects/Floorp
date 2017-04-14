/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use frame_builder::FrameBuilder;
use tiling::PrimitiveFlags;
use webrender_traits::{BorderSide, BorderStyle, BorderWidths, NormalBorder};
use webrender_traits::{ClipRegion, LayerPoint, LayerRect, LayerSize, ScrollLayerId};

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BorderCornerKind {
    None,
    Solid,
    Complex,
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BorderEdgeKind {
    None,
    Solid,
    Complex,
}

pub trait NormalBorderHelpers {
    fn get_corner(&self,
                  edge0: &BorderSide,
                  width0: f32,
                  edge1: &BorderSide,
                  width1: f32,
                  radius: &LayerSize) -> BorderCornerKind;

    fn get_edge(&self,
                edge: &BorderSide,
                width: f32) -> (BorderEdgeKind, f32);
}

impl NormalBorderHelpers for NormalBorder {
    fn get_corner(&self,
                  edge0: &BorderSide,
                  width0: f32,
                  edge1: &BorderSide,
                  width1: f32,
                  radius: &LayerSize) -> BorderCornerKind {
        // If either width is zero, a corner isn't formed.
        if width0 == 0.0 || width1 == 0.0 {
            return BorderCornerKind::None;
        }

        // If both edges are transparent, no corner is formed.
        if edge0.color.a == 0.0 && edge1.color.a == 0.0 {
            return BorderCornerKind::None;
        }

        match (edge0.style, edge1.style) {
            // If either edge is none or hidden, no corner is needed.
            (BorderStyle::None, _) | (_, BorderStyle::None) => BorderCornerKind::None,
            (BorderStyle::Hidden, _) | (_, BorderStyle::Hidden) => BorderCornerKind::None,

            // If both borders are solid, we can draw them with a simple rectangle if
            // both the colors match and there is no radius.
            (BorderStyle::Solid, BorderStyle::Solid) => {
                if edge0.color == edge1.color && radius.width == 0.0 && radius.height == 0.0 {
                    BorderCornerKind::Solid
                } else {
                    BorderCornerKind::Complex
                }
            }

            // Assume complex for these cases.
            // TODO(gw): There are some cases in here that can be handled with a fast path.
            // For example, with inset/outset borders, two of the four corners are solid.
            (BorderStyle::Dotted, _) | (_, BorderStyle::Dotted) => BorderCornerKind::Complex,
            (BorderStyle::Dashed, _) | (_, BorderStyle::Dashed) => BorderCornerKind::Complex,
            (BorderStyle::Double, _) | (_, BorderStyle::Double) => BorderCornerKind::Complex,
            (BorderStyle::Groove, _) | (_, BorderStyle::Groove) => BorderCornerKind::Complex,
            (BorderStyle::Ridge, _) | (_, BorderStyle::Ridge) => BorderCornerKind::Complex,
            (BorderStyle::Outset, _) | (_, BorderStyle::Outset) => BorderCornerKind::Complex,
            (BorderStyle::Inset, _) | (_, BorderStyle::Inset) => BorderCornerKind::Complex,
        }
    }

    fn get_edge(&self,
                edge: &BorderSide,
                width: f32) -> (BorderEdgeKind, f32) {
        if width == 0.0 {
            return (BorderEdgeKind::None, 0.0);
        }

        match edge.style {
            BorderStyle::None |
            BorderStyle::Hidden => (BorderEdgeKind::None, 0.0),

            BorderStyle::Solid |
            BorderStyle::Inset |
            BorderStyle::Outset => (BorderEdgeKind::Solid, width),

            BorderStyle::Double |
            BorderStyle::Dotted |
            BorderStyle::Dashed |
            BorderStyle::Groove |
            BorderStyle::Ridge => (BorderEdgeKind::Complex, width),
        }
    }
}

impl FrameBuilder {
    // TODO(gw): This allows us to move border types over to the
    // simplified shader model one at a time. Once all borders
    // are converted, this can be removed, along with the complex
    // border code path.
    pub fn add_simple_border(&mut self,
                             rect: &LayerRect,
                             border: &NormalBorder,
                             widths: &BorderWidths,
                             scroll_layer_id: ScrollLayerId,
                             clip_region: &ClipRegion) -> bool {
        // The border shader is quite expensive. For simple borders, we can just draw
        // the border with a few rectangles. This generally gives better batching, and
        // a GPU win in fragment shader time.
        // More importantly, the software (OSMesa) implementation we run tests on is
        // particularly slow at running our complex border shader, compared to the
        // rectangle shader. This has the effect of making some of our tests time
        // out more often on CI (the actual cause is simply too many Servo processes and
        // threads being run on CI at once).

        let radius = &border.radius;
        let left = &border.left;
        let right = &border.right;
        let top = &border.top;
        let bottom = &border.bottom;

        // If any of the corners are complex, fall back to slow path for now.
        let tl = border.get_corner(left, widths.left, top, widths.top, &radius.top_left);
        let tr = border.get_corner(top, widths.top, right, widths.right, &radius.top_right);
        let br = border.get_corner(right, widths.right, bottom, widths.bottom, &radius.bottom_right);
        let bl = border.get_corner(bottom, widths.bottom, left, widths.left, &radius.bottom_left);

        if tl == BorderCornerKind::Complex ||
           tr == BorderCornerKind::Complex ||
           br == BorderCornerKind::Complex ||
           bl == BorderCornerKind::Complex {
            return false;
        }

        // If any of the edges are complex, fall back to slow path for now.
        let (left_edge, left_len) = border.get_edge(left, widths.left);
        let (top_edge, top_len) = border.get_edge(top, widths.top);
        let (right_edge, right_len) = border.get_edge(right, widths.right);
        let (bottom_edge, bottom_len) = border.get_edge(bottom, widths.bottom);

        if left_edge == BorderEdgeKind::Complex ||
           top_edge == BorderEdgeKind::Complex ||
           right_edge == BorderEdgeKind::Complex ||
           bottom_edge == BorderEdgeKind::Complex {
            return false;
        }

        let p0 = rect.origin;
        let p1 = rect.bottom_right();
        let rect_width = rect.size.width;
        let rect_height = rect.size.height;

        // Add a solid rectangle for each visible edge/corner combination.
        if top_edge == BorderEdgeKind::Solid {
            self.add_solid_rectangle(scroll_layer_id,
                                     &LayerRect::new(p0,
                                                     LayerSize::new(rect.size.width, top_len)),
                                     clip_region,
                                     &border.top.color,
                                     PrimitiveFlags::None);
        }
        if left_edge == BorderEdgeKind::Solid {
            self.add_solid_rectangle(scroll_layer_id,
                                     &LayerRect::new(LayerPoint::new(p0.x, p0.y + top_len),
                                                     LayerSize::new(left_len,
                                                                    rect_height - top_len - bottom_len)),
                                     clip_region,
                                     &border.left.color,
                                     PrimitiveFlags::None);
        }
        if right_edge == BorderEdgeKind::Solid {
            self.add_solid_rectangle(scroll_layer_id,
                                     &LayerRect::new(LayerPoint::new(p1.x - right_len,
                                                                     p0.y + top_len),
                                                     LayerSize::new(right_len,
                                                                    rect_height - top_len - bottom_len)),
                                     clip_region,
                                     &border.right.color,
                                     PrimitiveFlags::None);
        }
        if bottom_edge == BorderEdgeKind::Solid {
            self.add_solid_rectangle(scroll_layer_id,
                                     &LayerRect::new(LayerPoint::new(p0.x, p1.y - bottom_len),
                                                     LayerSize::new(rect_width, bottom_len)),
                                     clip_region,
                                     &border.bottom.color,
                                     PrimitiveFlags::None);
        }

        true
    }
}

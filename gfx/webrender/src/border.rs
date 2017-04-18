/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use frame_builder::FrameBuilder;
use prim_store::{BorderPrimitiveCpu, BorderPrimitiveGpu, PrimitiveContainer};
use tiling::PrimitiveFlags;
use util::pack_as_float;
use webrender_traits::{BorderSide, BorderStyle, BorderWidths, ColorF, NormalBorder};
use webrender_traits::{ClipId, ClipRegion, LayerPoint, LayerRect, LayerSize};

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BorderCornerKind {
    None,
    Solid,
    Clip,
    Unhandled,
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BorderEdgeKind {
    None,
    Solid,
    Unhandled,
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
                    BorderCornerKind::Clip
                }
            }

            // Inset / outset borders just modtify the color of edges, so can be
            // drawn with the normal border corner shader.
            (BorderStyle::Outset, BorderStyle::Outset) |
            (BorderStyle::Inset, BorderStyle::Inset) => BorderCornerKind::Clip,

            // Assume complex for these cases.
            // TODO(gw): There are some cases in here that can be handled with a fast path.
            // For example, with inset/outset borders, two of the four corners are solid.
            (BorderStyle::Dotted, _) | (_, BorderStyle::Dotted) => BorderCornerKind::Unhandled,
            (BorderStyle::Dashed, _) | (_, BorderStyle::Dashed) => BorderCornerKind::Unhandled,
            (BorderStyle::Double, _) | (_, BorderStyle::Double) => BorderCornerKind::Unhandled,
            (BorderStyle::Groove, _) | (_, BorderStyle::Groove) => BorderCornerKind::Unhandled,
            (BorderStyle::Ridge, _) | (_, BorderStyle::Ridge) => BorderCornerKind::Unhandled,
            (BorderStyle::Outset, _) | (_, BorderStyle::Outset) => BorderCornerKind::Unhandled,
            (BorderStyle::Inset, _) | (_, BorderStyle::Inset) => BorderCornerKind::Unhandled,
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
            BorderStyle::Ridge => (BorderEdgeKind::Unhandled, width),
        }
    }
}

impl FrameBuilder {
    fn add_normal_border_primitive(&mut self,
                                   rect: &LayerRect,
                                   border: &NormalBorder,
                                   widths: &BorderWidths,
                                   clip_id: ClipId,
                                   clip_region: &ClipRegion,
                                   use_new_border_path: bool) {
        let radius = &border.radius;
        let left = &border.left;
        let right = &border.right;
        let top = &border.top;
        let bottom = &border.bottom;

        // These colors are used during inset/outset scaling.
        let left_color      = left.border_color(1.0, 2.0/3.0, 0.3, 0.7);
        let top_color       = top.border_color(1.0, 2.0/3.0, 0.3, 0.7);
        let right_color     = right.border_color(2.0/3.0, 1.0, 0.7, 0.3);
        let bottom_color    = bottom.border_color(2.0/3.0, 1.0, 0.7, 0.3);

        let prim_cpu = BorderPrimitiveCpu {
            use_new_border_path: use_new_border_path,
        };

        let prim_gpu = BorderPrimitiveGpu {
            colors: [ left_color, top_color, right_color, bottom_color ],
            widths: [ widths.left,
                      widths.top,
                      widths.right,
                      widths.bottom ],
            style: [
                pack_as_float(left.style as u32),
                pack_as_float(top.style as u32),
                pack_as_float(right.style as u32),
                pack_as_float(bottom.style as u32),
            ],
            radii: [
                radius.top_left,
                radius.top_right,
                radius.bottom_right,
                radius.bottom_left,
            ],
        };

        self.add_primitive(clip_id,
                           &rect,
                           clip_region,
                           &[],
                           PrimitiveContainer::Border(prim_cpu, prim_gpu));
    }

    // TODO(gw): This allows us to move border types over to the
    // simplified shader model one at a time. Once all borders
    // are converted, this can be removed, along with the complex
    // border code path.
    pub fn add_normal_border(&mut self,
                             rect: &LayerRect,
                             border: &NormalBorder,
                             widths: &BorderWidths,
                             clip_id: ClipId,
                             clip_region: &ClipRegion) {
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

        let corners = [
            border.get_corner(left, widths.left, top, widths.top, &radius.top_left),
            border.get_corner(top, widths.top, right, widths.right, &radius.top_right),
            border.get_corner(right, widths.right, bottom, widths.bottom, &radius.bottom_right),
            border.get_corner(bottom, widths.bottom, left, widths.left, &radius.bottom_left),
        ];

        // If any of the corners are unhandled, fall back to slow path for now.
        if corners.iter().any(|c| *c == BorderCornerKind::Unhandled) {
            self.add_normal_border_primitive(rect,
                                             border,
                                             widths,
                                             clip_id,
                                             clip_region,
                                             false);
            return;
        }

        let (left_edge, left_len) = border.get_edge(left, widths.left);
        let (top_edge, top_len) = border.get_edge(top, widths.top);
        let (right_edge, right_len) = border.get_edge(right, widths.right);
        let (bottom_edge, bottom_len) = border.get_edge(bottom, widths.bottom);

        let edges = [
            left_edge,
            top_edge,
            right_edge,
            bottom_edge,
        ];

        // If any of the edges are unhandled, fall back to slow path for now.
        if edges.iter().any(|e| *e == BorderEdgeKind::Unhandled) {
            self.add_normal_border_primitive(rect,
                                             border,
                                             widths,
                                             clip_id,
                                             clip_region,
                                             false);
            return;
        }

        // Use a simple rectangle case when all edges and corners are either
        // solid or none.
        let all_corners_simple = corners.iter().all(|c| {
            *c == BorderCornerKind::Solid || *c == BorderCornerKind::None
        });
        let all_edges_simple = edges.iter().all(|e| {
            *e == BorderEdgeKind::Solid || *e == BorderEdgeKind::None
        });

        if all_corners_simple && all_edges_simple {
            let p0 = rect.origin;
            let p1 = rect.bottom_right();
            let rect_width = rect.size.width;
            let rect_height = rect.size.height;

            // Add a solid rectangle for each visible edge/corner combination.
            if top_edge == BorderEdgeKind::Solid {
                self.add_solid_rectangle(clip_id,
                                         &LayerRect::new(p0,
                                                         LayerSize::new(rect_width, top_len)),
                                         clip_region,
                                         &border.top.color,
                                         PrimitiveFlags::None);
            }
            if left_edge == BorderEdgeKind::Solid {
                self.add_solid_rectangle(clip_id,
                                         &LayerRect::new(LayerPoint::new(p0.x, p0.y + top_len),
                                                         LayerSize::new(left_len,
                                                                        rect_height - top_len - bottom_len)),
                                         clip_region,
                                         &border.left.color,
                                         PrimitiveFlags::None);
            }
            if right_edge == BorderEdgeKind::Solid {
                self.add_solid_rectangle(clip_id,
                                         &LayerRect::new(LayerPoint::new(p1.x - right_len,
                                                                         p0.y + top_len),
                                                         LayerSize::new(right_len,
                                                                        rect_height - top_len - bottom_len)),
                                         clip_region,
                                         &border.right.color,
                                         PrimitiveFlags::None);
            }
            if bottom_edge == BorderEdgeKind::Solid {
                self.add_solid_rectangle(clip_id,
                                         &LayerRect::new(LayerPoint::new(p0.x, p1.y - bottom_len),
                                                         LayerSize::new(rect_width, bottom_len)),
                                         clip_region,
                                         &border.bottom.color,
                                         PrimitiveFlags::None);
            }
        } else {
            self.add_normal_border_primitive(rect,
                                             border,
                                             widths,
                                             clip_id,
                                             clip_region,
                                             true);
        }
    }
}

pub trait BorderSideHelpers {
    fn border_color(&self,
                    scale_factor_0: f32,
                    scale_factor_1: f32,
                    black_color_0: f32,
                    black_color_1: f32) -> ColorF;
}

impl BorderSideHelpers for BorderSide {
    fn border_color(&self,
                    scale_factor_0: f32,
                    scale_factor_1: f32,
                    black_color_0: f32,
                    black_color_1: f32) -> ColorF {
        match self.style {
            BorderStyle::Inset => {
                if self.color.r != 0.0 || self.color.g != 0.0 || self.color.b != 0.0 {
                    self.color.scale_rgb(scale_factor_1)
                } else {
                    ColorF::new(black_color_0, black_color_0, black_color_0, self.color.a)
                }
            }
            BorderStyle::Outset => {
                if self.color.r != 0.0 || self.color.g != 0.0 || self.color.b != 0.0 {
                    self.color.scale_rgb(scale_factor_0)
                } else {
                    ColorF::new(black_color_1, black_color_1, black_color_1, self.color.a)
                }
            }
            _ => self.color,
        }
    }
}

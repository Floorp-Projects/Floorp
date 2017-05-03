/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use ellipse::Ellipse;
use frame_builder::FrameBuilder;
use mask_cache::{ClipSource};
use prim_store::{BorderPrimitiveCpu, BorderPrimitiveGpu, GpuBlock32, PrimitiveContainer};
use tiling::PrimitiveFlags;
use util::pack_as_float;
use webrender_traits::{BorderSide, BorderStyle, BorderWidths, ClipAndScrollInfo, ClipRegion};
use webrender_traits::{ColorF, LayerPoint, LayerRect, LayerSize, NormalBorder};

enum BorderCorner {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
}

#[derive(Clone, Debug, PartialEq)]
pub enum BorderCornerKind {
    None,
    Solid,
    Clip,
    Mask(BorderCornerClipData, LayerSize, LayerSize),
    Unhandled,
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BorderEdgeKind {
    None,
    Solid,
    Clip,
    Unhandled,
}

trait NormalBorderHelpers {
    fn get_corner(&self,
                  edge0: &BorderSide,
                  width0: f32,
                  edge1: &BorderSide,
                  width1: f32,
                  radius: &LayerSize,
                  corner: BorderCorner,
                  border_rect: &LayerRect) -> BorderCornerKind;

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
                  radius: &LayerSize,
                  corner: BorderCorner,
                  border_rect: &LayerRect) -> BorderCornerKind {
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
            (BorderStyle::Inset, BorderStyle::Inset) |
            (BorderStyle::Double, BorderStyle::Double) |
            (BorderStyle::Groove, BorderStyle::Groove) |
            (BorderStyle::Ridge, BorderStyle::Ridge) => BorderCornerKind::Clip,

            // Dashed border corners get drawn into a clip mask.
            (BorderStyle::Dashed, BorderStyle::Dashed) => {
                let size = LayerSize::new(width0.max(radius.width), width1.max(radius.height));
                let (origin, clip_center, sign_modifier) = match corner {
                    BorderCorner::TopLeft => {
                        let origin = border_rect.origin;
                        let clip_center = origin + size;
                        (origin, clip_center, LayerPoint::new(-1.0, -1.0))
                    }
                    BorderCorner::TopRight => {
                        let origin = LayerPoint::new(border_rect.origin.x +
                                                     border_rect.size.width -
                                                     size.width,
                                                     border_rect.origin.y);
                        let clip_center = origin + LayerSize::new(0.0, size.height);
                        (origin, clip_center, LayerPoint::new(1.0, -1.0))
                    }
                    BorderCorner::BottomRight => {
                        let origin = border_rect.origin + (border_rect.size - size);
                        let clip_center = origin;
                        (origin, clip_center, LayerPoint::new(1.0, 1.0))
                    }
                    BorderCorner::BottomLeft => {
                        let origin = LayerPoint::new(border_rect.origin.x,
                                                     border_rect.origin.y +
                                                     border_rect.size.height -
                                                     size.height);
                        let clip_center = origin + LayerSize::new(size.width, 0.0);
                        (origin, clip_center, LayerPoint::new(-1.0, 1.0))
                    }
                };
                let clip_data = BorderCornerClipData {
                    corner_rect: LayerRect::new(origin, size),
                    clip_center: clip_center,
                    sign_modifier: sign_modifier,
                };
                BorderCornerKind::Mask(clip_data, *radius, LayerSize::new(width0, width1))
            }

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
            BorderStyle::Groove |
            BorderStyle::Ridge |
            BorderStyle::Dashed => (BorderEdgeKind::Clip, width),

            BorderStyle::Dotted => (BorderEdgeKind::Unhandled, width),
        }
    }
}

impl FrameBuilder {
    fn add_normal_border_primitive(&mut self,
                                   rect: &LayerRect,
                                   border: &NormalBorder,
                                   widths: &BorderWidths,
                                   clip_and_scroll: ClipAndScrollInfo,
                                   clip_region: &ClipRegion,
                                   use_new_border_path: bool,
                                   extra_clips: &[ClipSource]) {
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

        self.add_primitive(clip_and_scroll,
                           &rect,
                           clip_region,
                           extra_clips,
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
                             clip_and_scroll: ClipAndScrollInfo,
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
            border.get_corner(left,
                              widths.left,
                              top,
                              widths.top,
                              &radius.top_left,
                              BorderCorner::TopLeft,
                              rect),
            border.get_corner(right,
                              widths.right,
                              top,
                              widths.top,
                              &radius.top_right,
                              BorderCorner::TopRight,
                              rect),
            border.get_corner(right,
                              widths.right,
                              bottom,
                              widths.bottom,
                              &radius.bottom_right,
                              BorderCorner::BottomRight,
                              rect),
            border.get_corner(left,
                              widths.left,
                              bottom,
                              widths.bottom,
                              &radius.bottom_left,
                              BorderCorner::BottomLeft,
                              rect),
        ];

        // If any of the corners are unhandled, fall back to slow path for now.
        if corners.iter().any(|c| *c == BorderCornerKind::Unhandled) {
            self.add_normal_border_primitive(rect,
                                             border,
                                             widths,
                                             clip_and_scroll,
                                             clip_region,
                                             false,
                                             &[]);
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
                                             clip_and_scroll,
                                             clip_region,
                                             false,
                                             &[]);
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
                self.add_solid_rectangle(clip_and_scroll,
                                         &LayerRect::new(p0, LayerSize::new(rect_width, top_len)),
                                         clip_region,
                                         &border.top.color,
                                         PrimitiveFlags::None);
            }
            if left_edge == BorderEdgeKind::Solid {
                self.add_solid_rectangle(clip_and_scroll,
                                         &LayerRect::new(LayerPoint::new(p0.x, p0.y + top_len),
                                                         LayerSize::new(left_len,
                                                                        rect_height - top_len - bottom_len)),
                                         clip_region,
                                         &border.left.color,
                                         PrimitiveFlags::None);
            }
            if right_edge == BorderEdgeKind::Solid {
                self.add_solid_rectangle(clip_and_scroll,
                                         &LayerRect::new(LayerPoint::new(p1.x - right_len,
                                                                         p0.y + top_len),
                                                         LayerSize::new(right_len,
                                                                        rect_height - top_len - bottom_len)),
                                         clip_region,
                                         &border.right.color,
                                         PrimitiveFlags::None);
            }
            if bottom_edge == BorderEdgeKind::Solid {
                self.add_solid_rectangle(clip_and_scroll,
                                         &LayerRect::new(LayerPoint::new(p0.x, p1.y - bottom_len),
                                                         LayerSize::new(rect_width, bottom_len)),
                                         clip_region,
                                         &border.bottom.color,
                                         PrimitiveFlags::None);
            }
        } else {
            // Create clip masks for border corners, if required.
            let mut extra_clips = Vec::new();

            for corner in corners.iter() {
                if let &BorderCornerKind::Mask(corner_data, corner_radius, widths) = corner {
                    let clip_source = BorderCornerClipSource::new(corner_data,
                                                                  corner_radius,
                                                                  widths);
                    extra_clips.push(ClipSource::BorderCorner(clip_source));
                }
            }

            self.add_normal_border_primitive(rect,
                                             border,
                                             widths,
                                             clip_and_scroll,
                                             clip_region,
                                             true,
                                             &extra_clips);
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

/// The source data for a border corner clip mask.
#[derive(Debug, Clone)]
pub struct BorderCornerClipSource {
    pub corner_data: BorderCornerClipData,
    pub dash_count: usize,
    dash_arc_length: f32,
    ellipse: Ellipse,
}

impl BorderCornerClipSource {
    pub fn new(corner_data: BorderCornerClipData,
               corner_radius: LayerSize,
               widths: LayerSize) -> BorderCornerClipSource {
        let ellipse = Ellipse::new(corner_radius);

        // Work out a dash length (and therefore dash count)
        // based on the width of the border edges. The "correct"
        // dash length is not mentioned in the CSS borders
        // spec. The calculation below is similar, but not exactly
        // the same as what Gecko uses.
        // TODO(gw): Iterate on this to get it closer to what Gecko
        //           uses for dash length.

        // Approximate the total arc length of the quarter ellipse.
        let total_arc_length = ellipse.get_quarter_arc_length();

        // The desired dash length is ~3x the border width.
        let average_border_width = 0.5 * (widths.width + widths.height);
        let desired_dash_arc_length = average_border_width * 3.0;

        // Get the ideal number of dashes for that arc length.
        // This is scaled by 0.5 since there is an on/off length
        // for each dash.
        let desired_count = 0.5 * total_arc_length / desired_dash_arc_length;

        // Round that up to the nearest integer, so that the dash length
        // doesn't exceed the ratio above.
        let actual_count = desired_count.ceil();

        // Get the correct dash arc length.
        let dash_arc_length = 0.5 * total_arc_length / actual_count;

        // Get the number of dashes we'll need to fit.
        let dash_count = actual_count as usize;

        BorderCornerClipSource {
            corner_data: corner_data,
            dash_count: dash_count,
            ellipse: ellipse,
            dash_arc_length: dash_arc_length,
        }
    }

    pub fn populate_gpu_data(&self, slice: &mut [GpuBlock32]) {
        let (header, dashes) = slice.split_first_mut().unwrap();
        *header = self.corner_data.into();

        let mut current_arc_length = self.dash_arc_length * 0.5;
        for dash_index in 0..self.dash_count {
            let arc_length0 = current_arc_length;
            current_arc_length += self.dash_arc_length;

            let arc_length1 = current_arc_length;
            current_arc_length += self.dash_arc_length;

            let dash_data = BorderCornerDashClipData::new(arc_length0,
                                                          arc_length1,
                                                          &self.ellipse);
            dashes[dash_index] = dash_data.into();
        }
    }
}

/// Represents the common GPU data for writing a
/// clip mask for a border corner.
#[derive(Debug, Copy, Clone, PartialEq)]
#[repr(C)]
pub struct BorderCornerClipData {
    /// Local space rect of the border corner.
    corner_rect: LayerRect,
    /// Local space point that is the center of the
    /// circle or ellipse that we are clipping against.
    clip_center: LayerPoint,
    /// A constant that flips the local space points
    /// and tangents of the ellipse for this specific
    /// corner. This is used since the ellipse points
    /// and tangents are always generated for a single
    /// quadrant only.
    sign_modifier: LayerPoint,
}

/// Represents the GPU data for drawing a single dash
/// to a clip mask. A dash clip is defined by two lines.
/// We store a point on the ellipse curve, and a tangent
/// to that point, which allows for efficient line-distance
/// calculations in the fragment shader.
#[derive(Debug, Clone)]
#[repr(C)]
pub struct BorderCornerDashClipData {
    pub point0: LayerPoint,
    pub tangent0: LayerPoint,
    pub point1: LayerPoint,
    pub tangent1: LayerPoint,
}

impl BorderCornerDashClipData {
    pub fn new(arc_length0: f32,
               arc_length1: f32,
               ellipse: &Ellipse) -> BorderCornerDashClipData {
        let alpha = ellipse.find_angle_for_arc_length(arc_length0);
        let beta = ellipse.find_angle_for_arc_length(arc_length1);

        let (p0, t0) = ellipse.get_point_and_tangent(alpha);
        let (p1, t1) = ellipse.get_point_and_tangent(beta);

        BorderCornerDashClipData {
            point0: p0,
            tangent0: t0,
            point1: p1,
            tangent1: t1,
        }
    }
}

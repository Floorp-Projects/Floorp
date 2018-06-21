/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, BorderSide, BorderStyle, BorderWidths, ColorF};
use api::{ColorU, DeviceRect, DeviceSize, LayoutSizeAu, LayoutPrimitiveInfo, LayoutToDeviceScale};
use api::{DevicePixel, DeviceVector2D, DevicePoint, DeviceIntSize, LayoutRect, LayoutSize, NormalBorder};
use app_units::Au;
use ellipse::Ellipse;
use display_list_flattener::DisplayListFlattener;
use gpu_types::{BorderInstance, BorderSegment, BrushFlags};
use prim_store::{BrushKind, BrushPrimitive, BrushSegment};
use prim_store::{BorderSource, EdgeAaSegmentMask, PrimitiveContainer, ScrollNodeAndClipChain};
use util::{lerp, RectHelpers};

trait AuSizeConverter {
    fn to_au(&self) -> LayoutSizeAu;
}

impl AuSizeConverter for LayoutSize {
    fn to_au(&self) -> LayoutSizeAu {
        LayoutSizeAu::new(
            Au::from_f32_px(self.width),
            Au::from_f32_px(self.height),
        )
    }
}

// TODO(gw): Perhaps there is a better way to store
//           the border cache key than duplicating
//           all the border structs with hashable
//           variants...

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BorderRadiusAu {
    pub top_left: LayoutSizeAu,
    pub top_right: LayoutSizeAu,
    pub bottom_left: LayoutSizeAu,
    pub bottom_right: LayoutSizeAu,
}

impl From<BorderRadius> for BorderRadiusAu {
    fn from(radius: BorderRadius) -> BorderRadiusAu {
        BorderRadiusAu {
            top_left: radius.top_left.to_au(),
            top_right: radius.top_right.to_au(),
            bottom_right: radius.bottom_right.to_au(),
            bottom_left: radius.bottom_left.to_au(),
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BorderWidthsAu {
    pub left: Au,
    pub top: Au,
    pub right: Au,
    pub bottom: Au,
}

impl From<BorderWidths> for BorderWidthsAu {
    fn from(widths: BorderWidths) -> Self {
        BorderWidthsAu {
            left: Au::from_f32_px(widths.left),
            top: Au::from_f32_px(widths.top),
            right: Au::from_f32_px(widths.right),
            bottom: Au::from_f32_px(widths.bottom),
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BorderSideAu {
    pub color: ColorU,
    pub style: BorderStyle,
}

impl From<BorderSide> for BorderSideAu {
    fn from(side: BorderSide) -> Self {
        BorderSideAu {
            color: side.color.into(),
            style: side.style,
        }
    }
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BorderCacheKey {
    pub left: BorderSideAu,
    pub right: BorderSideAu,
    pub top: BorderSideAu,
    pub bottom: BorderSideAu,
    pub radius: BorderRadiusAu,
    pub widths: BorderWidthsAu,
    pub scale: Au,
}

pub fn ensure_no_corner_overlap(
    radius: &mut BorderRadius,
    rect: &LayoutRect,
) {
    let mut ratio = 1.0;
    let top_left_radius = &mut radius.top_left;
    let top_right_radius = &mut radius.top_right;
    let bottom_right_radius = &mut radius.bottom_right;
    let bottom_left_radius = &mut radius.bottom_left;

    let sum = top_left_radius.width + top_right_radius.width;
    if rect.size.width < sum {
        ratio = f32::min(ratio, rect.size.width / sum);
    }

    let sum = bottom_left_radius.width + bottom_right_radius.width;
    if rect.size.width < sum {
        ratio = f32::min(ratio, rect.size.width / sum);
    }

    let sum = top_left_radius.height + bottom_left_radius.height;
    if rect.size.height < sum {
        ratio = f32::min(ratio, rect.size.height / sum);
    }

    let sum = top_right_radius.height + bottom_right_radius.height;
    if rect.size.height < sum {
        ratio = f32::min(ratio, rect.size.height / sum);
    }

    if ratio < 1. {
        top_left_radius.width *= ratio;
        top_left_radius.height *= ratio;

        top_right_radius.width *= ratio;
        top_right_radius.height *= ratio;

        bottom_left_radius.width *= ratio;
        bottom_left_radius.height *= ratio;

        bottom_right_radius.width *= ratio;
        bottom_right_radius.height *= ratio;
    }
}

impl<'a> DisplayListFlattener<'a> {
    pub fn add_normal_border(
        &mut self,
        info: &LayoutPrimitiveInfo,
        border: &NormalBorder,
        widths: &BorderWidths,
        clip_and_scroll: ScrollNodeAndClipChain,
    ) {
        let mut border = *border;
        ensure_no_corner_overlap(&mut border.radius, &info.rect);

        let prim = BrushPrimitive::new(
            BrushKind::Border {
                source: BorderSource::Border {
                    border,
                    widths: *widths,
                    cache_key: BorderCacheKey {
                        left: border.left.into(),
                        top: border.top.into(),
                        right: border.right.into(),
                        bottom: border.bottom.into(),
                        widths: (*widths).into(),
                        radius: border.radius.into(),
                        scale: Au::from_f32_px(0.0),
                    },
                    task_info: None,
                    handle: None,
                },
            },
            None,
        );

        self.add_primitive(
            clip_and_scroll,
            info,
            Vec::new(),
            PrimitiveContainer::Brush(prim),
        );
    }
}

pub trait BorderSideHelpers {
    fn border_color(
        &self,
        scale_factor_0: f32,
        scale_factor_1: f32,
        black_color_0: f32,
        black_color_1: f32,
    ) -> ColorF;
}

impl BorderSideHelpers for BorderSide {
    fn border_color(
        &self,
        scale_factor_0: f32,
        scale_factor_1: f32,
        black_color_0: f32,
        black_color_1: f32,
    ) -> ColorF {
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

/// The kind of border corner clip.
#[repr(C)]
#[derive(Copy, Debug, Clone, PartialEq)]
pub enum BorderCornerClipKind {
    Dash = 1,
    Dot = 2,
}

/// The source data for a border corner clip mask.
#[derive(Debug, Clone)]
pub struct BorderCornerClipSource {
    pub max_clip_count: usize,
    kind: BorderCornerClipKind,
    widths: DeviceSize,
    radius: DeviceSize,
    ellipse: Ellipse<DevicePixel>,
}

impl BorderCornerClipSource {
    pub fn new(
        corner_radius: DeviceSize,
        widths: DeviceSize,
        kind: BorderCornerClipKind,
    ) -> BorderCornerClipSource {
        // Work out a dash length (and therefore dash count)
        // based on the width of the border edges. The "correct"
        // dash length is not mentioned in the CSS borders
        // spec. The calculation below is similar, but not exactly
        // the same as what Gecko uses.
        // TODO(gw): Iterate on this to get it closer to what Gecko
        //           uses for dash length.

        let (ellipse, max_clip_count) = match kind {
            BorderCornerClipKind::Dash => {
                let ellipse = Ellipse::new(corner_radius);

                // The desired dash length is ~3x the border width.
                let average_border_width = 0.5 * (widths.width + widths.height);
                let desired_dash_arc_length = average_border_width * 3.0;

                // Get the ideal number of dashes for that arc length.
                // This is scaled by 0.5 since there is an on/off length
                // for each dash.
                let desired_count = 0.5 * ellipse.total_arc_length / desired_dash_arc_length;

                // Round that up to the nearest integer, so that the dash length
                // doesn't exceed the ratio above. Add one extra dash to cover
                // the last half-dash of the arc.
                (ellipse, desired_count.ceil() as usize)
            }
            BorderCornerClipKind::Dot => {
                let mut corner_radius = corner_radius;
                if corner_radius.width < (widths.width / 2.0) {
                    corner_radius.width = 0.0;
                }
                if corner_radius.height < (widths.height / 2.0) {
                    corner_radius.height = 0.0;
                }

                if corner_radius.width == 0. && corner_radius.height == 0. {
                    (Ellipse::new(corner_radius), 1)
                } else {
                    // The centers of dots follow an ellipse along the middle of the
                    // border radius.
                    let inner_radius = (corner_radius - widths * 0.5).abs();
                    let ellipse = Ellipse::new(inner_radius);

                    // Allocate a "worst case" number of dot clips. This can be
                    // calculated by taking the minimum edge radius, since that
                    // will result in the maximum number of dots along the path.
                    let min_diameter = widths.width.min(widths.height);

                    // Get the number of circles (assuming spacing of one diameter
                    // between dots).
                    let max_dot_count = 0.5 * ellipse.total_arc_length / min_diameter;

                    // Add space for one extra dot since they are centered at the
                    // start of the arc.
                    (ellipse, max_dot_count.ceil() as usize)
                }
            }
        };

        BorderCornerClipSource {
            kind,
            max_clip_count,
            ellipse,
            widths,
            radius: corner_radius,
        }
    }

    // TODO(gw): The naming and structure of BorderCornerClipSource
    //           don't really make sense. I've left it this way
    //           for now in order to reduce the size of the
    //           patch a bit. In the future, when we spent some
    //           time working on dot/dash placement, we should
    //           restructure this code to be more consistent
    //           with how border rendering works now.
    pub fn write(self, segment: BorderSegment) -> Vec<[f32; 8]> {
        let mut dot_dash_data = Vec::new();

        let outer_scale = match segment {
            BorderSegment::TopLeft => DeviceVector2D::new(0.0, 0.0),
            BorderSegment::TopRight => DeviceVector2D::new(1.0, 0.0),
            BorderSegment::BottomRight => DeviceVector2D::new(1.0, 1.0),
            BorderSegment::BottomLeft => DeviceVector2D::new(0.0, 1.0),
            _ => unreachable!(),
        };
        let outer = DevicePoint::new(
            outer_scale.x * self.radius.width,
            outer_scale.y * self.radius.height,
        );
        let clip_sign = DeviceVector2D::new(
            1.0 - 2.0 * outer_scale.x,
            1.0 - 2.0 * outer_scale.y,
        );

        match self.kind {
            BorderCornerClipKind::Dash => {
                // Get the correct dash arc length.
                let dash_arc_length =
                    0.5 * self.ellipse.total_arc_length / self.max_clip_count as f32;
                // Start the first dash at one quarter the length of a single dash
                // along the arc line. This is arbitrary but looks reasonable in
                // most cases. We need to spend some time working on a more
                // sophisticated dash placement algorithm that takes into account
                // the offset of the dashes along edge segments.
                let mut current_arc_length = 0.25 * dash_arc_length;
                for _ in 0 .. self.max_clip_count {
                    let arc_length0 = current_arc_length;
                    current_arc_length += dash_arc_length;

                    let arc_length1 = current_arc_length;
                    current_arc_length += dash_arc_length;

                    let alpha = self.ellipse.find_angle_for_arc_length(arc_length0);
                    let beta =  self.ellipse.find_angle_for_arc_length(arc_length1);

                    let (point0, tangent0) =  self.ellipse.get_point_and_tangent(alpha);
                    let (point1, tangent1) =  self.ellipse.get_point_and_tangent(beta);

                    let point0 = DevicePoint::new(
                        outer.x + clip_sign.x * (self.radius.width - point0.x),
                        outer.y + clip_sign.y * (self.radius.height - point0.y),
                    );

                    let tangent0 = DeviceVector2D::new(
                        -tangent0.x * clip_sign.x,
                        -tangent0.y * clip_sign.y,
                    );

                    let point1 = DevicePoint::new(
                        outer.x + clip_sign.x * (self.radius.width - point1.x),
                        outer.y + clip_sign.y * (self.radius.height - point1.y),
                    );

                    let tangent1 = DeviceVector2D::new(
                        -tangent1.x * clip_sign.x,
                        -tangent1.y * clip_sign.y,
                    );

                    dot_dash_data.push([
                        point0.x,
                        point0.y,
                        tangent0.x,
                        tangent0.y,
                        point1.x,
                        point1.y,
                        tangent1.x,
                        tangent1.y,
                    ]);
                }
            }
            BorderCornerClipKind::Dot if self.max_clip_count == 1 => {
                let dot_diameter = lerp(self.widths.width, self.widths.height, 0.5);
                dot_dash_data.push([
                    self.widths.width / 2.0, self.widths.height / 2.0, 0.5 * dot_diameter, 0.,
                    0., 0., 0., 0.,
                ]);
            }
            BorderCornerClipKind::Dot => {
                let mut forward_dots = Vec::new();
                let mut back_dots = Vec::new();
                let mut leftover_arc_length = 0.0;

                // Alternate between adding dots at the start and end of the
                // ellipse arc. This ensures that we always end up with an exact
                // half dot at each end of the arc, to match up with the edges.
                forward_dots.push(DotInfo::new(self.widths.width, self.widths.width));
                back_dots.push(DotInfo::new(
                    self.ellipse.total_arc_length - self.widths.height,
                    self.widths.height,
                ));

                for dot_index in 0 .. self.max_clip_count {
                    let prev_forward_pos = *forward_dots.last().unwrap();
                    let prev_back_pos = *back_dots.last().unwrap();

                    // Select which end of the arc to place a dot from.
                    // This just alternates between the start and end of
                    // the arc, which ensures that there is always an
                    // exact half-dot at each end of the ellipse.
                    let going_forward = dot_index & 1 == 0;

                    let (next_dot_pos, leftover) = if going_forward {
                        let next_dot_pos =
                            prev_forward_pos.arc_pos + 2.0 * prev_forward_pos.diameter;
                        (next_dot_pos, prev_back_pos.arc_pos - next_dot_pos)
                    } else {
                        let next_dot_pos = prev_back_pos.arc_pos - 2.0 * prev_back_pos.diameter;
                        (next_dot_pos, next_dot_pos - prev_forward_pos.arc_pos)
                    };

                    // Use a lerp between each edge's dot
                    // diameter, based on the linear distance
                    // along the arc to get the diameter of the
                    // dot at this arc position.
                    let t = next_dot_pos / self.ellipse.total_arc_length;
                    let dot_diameter = lerp(self.widths.width, self.widths.height, t);

                    // If we can't fit a dot, bail out.
                    if leftover < dot_diameter {
                        leftover_arc_length = leftover;
                        break;
                    }

                    // We can place a dot!
                    let dot = DotInfo::new(next_dot_pos, dot_diameter);
                    if going_forward {
                        forward_dots.push(dot);
                    } else {
                        back_dots.push(dot);
                    }
                }

                // Now step through the dots, and distribute any extra
                // leftover space on the arc between them evenly. Once
                // the final arc position is determined, generate the correct
                // arc positions and angles that get passed to the clip shader.
                let number_of_dots = forward_dots.len() + back_dots.len();
                let extra_space_per_dot = leftover_arc_length / (number_of_dots - 1) as f32;

                let create_dot_data = |ellipse: &Ellipse<DevicePixel>, arc_length: f32, radius: f32| -> [f32; 8] {
                    // Represents the GPU data for drawing a single dot to a clip mask. The order
                    // these are specified must stay in sync with the way this data is read in the
                    // dot clip shader.
                    let theta = ellipse.find_angle_for_arc_length(arc_length);
                    let (center, _) = ellipse.get_point_and_tangent(theta);

                    let center = DevicePoint::new(
                        outer.x + clip_sign.x * (self.radius.width - center.x),
                        outer.y + clip_sign.y * (self.radius.height - center.y),
                    );

                    [center.x, center.y, radius, 0.0, 0.0, 0.0, 0.0, 0.0]
                };

                for (i, dot) in forward_dots.iter().enumerate() {
                    let extra_dist = i as f32 * extra_space_per_dot;
                    let dot_data = create_dot_data(&self.ellipse, dot.arc_pos + extra_dist, 0.5 * dot.diameter);
                    dot_dash_data.push(dot_data);
                }

                for (i, dot) in back_dots.iter().enumerate() {
                    let extra_dist = i as f32 * extra_space_per_dot;
                    let dot_data = create_dot_data(&self.ellipse, dot.arc_pos - extra_dist, 0.5 * dot.diameter);
                    dot_dash_data.push(dot_data);
                }
            }
        }

        dot_dash_data
    }
}

#[derive(Copy, Clone, Debug)]
struct DotInfo {
    arc_pos: f32,
    diameter: f32,
}

impl DotInfo {
    fn new(arc_pos: f32, diameter: f32) -> DotInfo {
        DotInfo { arc_pos, diameter }
    }
}

#[derive(Debug)]
pub struct BorderSegmentInfo {
    task_rect: DeviceRect,
    segment: BorderSegment,
    radius: DeviceSize,
    widths: DeviceSize,
}

#[derive(Debug)]
pub struct BorderRenderTaskInfo {
    pub border_segments: Vec<BorderSegmentInfo>,
    pub size: DeviceIntSize,
}

// Information needed to place and draw a border edge.
struct EdgeInfo {
    // Offset in local space to place the edge from origin.
    local_offset: f32,
    // Size of the edge in local space.
    local_size: f32,
    // Size in device pixels needed in the render task.
    device_size: f32,
}

impl EdgeInfo {
    fn new(
        local_offset: f32,
        local_size: f32,
        device_size: f32,
    ) -> EdgeInfo {
        EdgeInfo {
            local_offset,
            local_size,
            device_size,
        }
    }
}

// Get the needed size in device pixels for an edge,
// based on the border style of that edge. This is used
// to determine how big the render task should be.
fn get_edge_info(
    style: BorderStyle,
    side_width: f32,
    avail_size: f32,
    scale: f32,
) -> EdgeInfo {
    // To avoid division by zero below.
    if side_width <= 0.0 {
        return EdgeInfo::new(0.0, 0.0, 0.0);
    }

    match style {
        BorderStyle::Dashed => {
            let dash_size = 3.0 * side_width;
            let approx_dash_count = (avail_size - dash_size) / dash_size;
            let dash_count = 1.0 + 2.0 * (approx_dash_count / 2.0).floor();
            let used_size = dash_count * dash_size;
            let extra_space = avail_size - used_size;
            let device_size = 2.0 * dash_size * scale;
            let offset = (extra_space * 0.5).round();
            EdgeInfo::new(offset, used_size, device_size)
        }
        BorderStyle::Dotted => {
            let dot_and_space_size = 2.0 * side_width;
            if avail_size < dot_and_space_size * 0.75 {
                return EdgeInfo::new(0.0, 0.0, 0.0);
            }
            let approx_dot_count = avail_size / dot_and_space_size;
            let dot_count = approx_dot_count.floor().max(1.0);
            let used_size = dot_count * dot_and_space_size;
            let extra_space = avail_size - used_size;
            let device_size = dot_and_space_size * scale;
            let offset = (extra_space * 0.5).round();
            EdgeInfo::new(offset, used_size, device_size)
        }
        _ => {
            EdgeInfo::new(0.0, avail_size, 8.0)
        }
    }
}

impl BorderRenderTaskInfo {
    pub fn new(
        rect: &LayoutRect,
        border: &NormalBorder,
        widths: &BorderWidths,
        scale: LayoutToDeviceScale,
        brush_segments: &mut Vec<BrushSegment>,
    ) -> Option<Self> {
        let mut border_segments = Vec::new();

        let dp_width_top = (widths.top * scale.0).ceil();
        let dp_width_bottom = (widths.bottom * scale.0).ceil();
        let dp_width_left = (widths.left * scale.0).ceil();
        let dp_width_right = (widths.right * scale.0).ceil();

        let dp_corner_tl = (border.radius.top_left * scale).ceil();
        let dp_corner_tr = (border.radius.top_right * scale).ceil();
        let dp_corner_bl = (border.radius.bottom_left * scale).ceil();
        let dp_corner_br = (border.radius.bottom_right * scale).ceil();

        let dp_size_tl = DeviceSize::new(
            dp_corner_tl.width.max(dp_width_left),
            dp_corner_tl.height.max(dp_width_top),
        );
        let dp_size_tr = DeviceSize::new(
            dp_corner_tr.width.max(dp_width_right),
            dp_corner_tr.height.max(dp_width_top),
        );
        let dp_size_br = DeviceSize::new(
            dp_corner_br.width.max(dp_width_right),
            dp_corner_br.height.max(dp_width_bottom),
        );
        let dp_size_bl = DeviceSize::new(
            dp_corner_bl.width.max(dp_width_left),
            dp_corner_bl.height.max(dp_width_bottom),
        );

        let local_size_tl = LayoutSize::new(
            border.radius.top_left.width.max(widths.left),
            border.radius.top_left.height.max(widths.top),
        );
        let local_size_tr = LayoutSize::new(
            border.radius.top_right.width.max(widths.right),
            border.radius.top_right.height.max(widths.top),
        );
        let local_size_br = LayoutSize::new(
            border.radius.bottom_right.width.max(widths.right),
            border.radius.bottom_right.height.max(widths.bottom),
        );
        let local_size_bl = LayoutSize::new(
            border.radius.bottom_left.width.max(widths.left),
            border.radius.bottom_left.height.max(widths.bottom),
        );

        let top_edge_info = get_edge_info(
            border.top.style,
            widths.top,
            rect.size.width - local_size_tl.width - local_size_tr.width,
            scale.0,
        );
        let bottom_edge_info = get_edge_info(
            border.bottom.style,
            widths.bottom,
            rect.size.width - local_size_bl.width - local_size_br.width,
            scale.0,
        );
        let inner_width = top_edge_info.device_size.max(bottom_edge_info.device_size).ceil();

        let left_edge_info = get_edge_info(
            border.left.style,
            widths.left,
            rect.size.height - local_size_tl.height - local_size_bl.height,
            scale.0,
        );
        let right_edge_info = get_edge_info(
            border.right.style,
            widths.right,
            rect.size.height - local_size_tr.height - local_size_br.height,
            scale.0,
        );
        let inner_height = left_edge_info.device_size.max(right_edge_info.device_size).ceil();

        let size = DeviceSize::new(
            dp_size_tl.width.max(dp_size_bl.width) + inner_width + dp_size_tr.width.max(dp_size_br.width),
            dp_size_tl.height.max(dp_size_tr.height) + inner_height + dp_size_bl.height.max(dp_size_br.height),
        );

        if size.width == 0.0 || size.height == 0.0 {
            return None;
        }

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x,
                rect.origin.y + local_size_tl.height + left_edge_info.local_offset,
                rect.origin.x + widths.left,
                rect.origin.y + local_size_tl.height + left_edge_info.local_offset + left_edge_info.local_size,
            ),
            DeviceRect::from_floats(
                0.0,
                dp_size_tl.height,
                dp_width_left,
                dp_size_tl.height + left_edge_info.device_size,
            ),
            &border.left,
            BorderSegment::Left,
            EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::RIGHT,
            &mut border_segments,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_Y,
            brush_segments,
        );

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x + local_size_tl.width + top_edge_info.local_offset,
                rect.origin.y,
                rect.origin.x + local_size_tl.width + top_edge_info.local_offset + top_edge_info.local_size,
                rect.origin.y + widths.top,
            ),
            DeviceRect::from_floats(
                dp_size_tl.width,
                0.0,
                dp_size_tl.width + top_edge_info.device_size,
                dp_width_top,
            ),
            &border.top,
            BorderSegment::Top,
            EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::BOTTOM,
            &mut border_segments,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_X,
            brush_segments,
        );

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x + rect.size.width - widths.right,
                rect.origin.y + local_size_tr.height + right_edge_info.local_offset,
                rect.origin.x + rect.size.width,
                rect.origin.y + local_size_tr.height + right_edge_info.local_offset + right_edge_info.local_size,
            ),
            DeviceRect::from_floats(
                size.width - dp_width_right,
                dp_size_tr.height,
                size.width,
                dp_size_tr.height + right_edge_info.device_size,
            ),
            &border.right,
            BorderSegment::Right,
            EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::LEFT,
            &mut border_segments,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_Y,
            brush_segments,
        );

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x + local_size_bl.width + bottom_edge_info.local_offset,
                rect.origin.y + rect.size.height - widths.bottom,
                rect.origin.x + local_size_bl.width + bottom_edge_info.local_offset + bottom_edge_info.local_size,
                rect.origin.y + rect.size.height,
            ),
            DeviceRect::from_floats(
                dp_size_bl.width,
                size.height - dp_width_bottom,
                dp_size_bl.width + bottom_edge_info.device_size,
                size.height,
            ),
            &border.bottom,
            BorderSegment::Bottom,
            EdgeAaSegmentMask::BOTTOM | EdgeAaSegmentMask::TOP,
            &mut border_segments,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_X,
            brush_segments,
        );

        add_corner_segment(
            LayoutRect::from_floats(
                rect.origin.x,
                rect.origin.y,
                rect.origin.x + local_size_tl.width,
                rect.origin.y + local_size_tl.height,
            ),
            DeviceRect::from_floats(
                0.0,
                0.0,
                dp_size_tl.width,
                dp_size_tl.height,
            ),
            &border.left,
            &border.top,
            DeviceSize::new(dp_width_left, dp_width_top),
            dp_corner_tl,
            BorderSegment::TopLeft,
            EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::LEFT,
            &mut border_segments,
            brush_segments,
        );

        add_corner_segment(
            LayoutRect::from_floats(
                rect.origin.x + rect.size.width - local_size_tr.width,
                rect.origin.y,
                rect.origin.x + rect.size.width,
                rect.origin.y + local_size_tr.height,
            ),
            DeviceRect::from_floats(
                size.width - dp_size_tr.width,
                0.0,
                size.width,
                dp_size_tr.height,
            ),
            &border.top,
            &border.right,
            DeviceSize::new(dp_width_right, dp_width_top),
            dp_corner_tr,
            BorderSegment::TopRight,
            EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::RIGHT,
            &mut border_segments,
            brush_segments,
        );

        add_corner_segment(
            LayoutRect::from_floats(
                rect.origin.x + rect.size.width - local_size_br.width,
                rect.origin.y + rect.size.height - local_size_br.height,
                rect.origin.x + rect.size.width,
                rect.origin.y + rect.size.height,
            ),
            DeviceRect::from_floats(
                size.width - dp_size_br.width,
                size.height - dp_size_br.height,
                size.width,
                size.height,
            ),
            &border.right,
            &border.bottom,
            DeviceSize::new(dp_width_right, dp_width_bottom),
            dp_corner_br,
            BorderSegment::BottomRight,
            EdgeAaSegmentMask::BOTTOM | EdgeAaSegmentMask::RIGHT,
            &mut border_segments,
            brush_segments,
        );

        add_corner_segment(
            LayoutRect::from_floats(
                rect.origin.x,
                rect.origin.y + rect.size.height - local_size_bl.height,
                rect.origin.x + local_size_bl.width,
                rect.origin.y + rect.size.height,
            ),
            DeviceRect::from_floats(
                0.0,
                size.height - dp_size_bl.height,
                dp_size_bl.width,
                size.height,
            ),
            &border.bottom,
            &border.left,
            DeviceSize::new(dp_width_left, dp_width_bottom),
            dp_corner_bl,
            BorderSegment::BottomLeft,
            EdgeAaSegmentMask::BOTTOM | EdgeAaSegmentMask::LEFT,
            &mut border_segments,
            brush_segments,
        );

        Some(BorderRenderTaskInfo {
            border_segments,
            size: size.to_i32(),
        })
    }

    pub fn build_instances(&self, border: &NormalBorder) -> Vec<BorderInstance> {
        let mut instances = Vec::new();

        for info in &self.border_segments {
            let (side0, side1, flip0, flip1) = match info.segment {
                BorderSegment::Left => (&border.left, &border.left, false, false),
                BorderSegment::Top => (&border.top, &border.top, false, false),
                BorderSegment::Right => (&border.right, &border.right, true, true),
                BorderSegment::Bottom => (&border.bottom, &border.bottom, true, true),
                BorderSegment::TopLeft => (&border.left, &border.top, false, false),
                BorderSegment::TopRight => (&border.top, &border.right, false, true),
                BorderSegment::BottomRight => (&border.right, &border.bottom, true, true),
                BorderSegment::BottomLeft => (&border.bottom, &border.left, true, false),
            };

            let style0 = if side0.style.is_hidden() {
                side1.style
            } else {
                side0.style
            };
            let style1 = if side1.style.is_hidden() {
                side0.style
            } else {
                side1.style
            };

            // These modulate colors are not part of the specification. They
            // are derived from the Gecko source code and experimentation, and
            // used to modulate the colors in order to generate colors for
            // the inset/outset and groove/ridge border styles.
            let color0 = if flip0 {
                side0.border_color(2.0 / 3.0, 1.0, 0.7, 0.3)
            } else {
                side0.border_color(1.0, 2.0 / 3.0, 0.3, 0.7)
            };

            let color1 = if flip1 {
                side1.border_color(2.0 / 3.0, 1.0, 0.7, 0.3)
            } else {
                side1.border_color(1.0, 2.0 / 3.0, 0.3, 0.7)
            };

            add_segment(
                info.task_rect,
                style0,
                style1,
                color0,
                color1,
                info.segment,
                &mut instances,
                info.widths,
                info.radius,
            );
        }

        instances
    }
}

fn add_brush_segment(
    image_rect: LayoutRect,
    task_rect: DeviceRect,
    brush_flags: BrushFlags,
    edge_flags: EdgeAaSegmentMask,
    brush_segments: &mut Vec<BrushSegment>,
) {
    brush_segments.push(
        BrushSegment::new(
            image_rect,
            true,
            edge_flags,
            [
                task_rect.origin.x,
                task_rect.origin.y,
                task_rect.origin.x + task_rect.size.width,
                task_rect.origin.y + task_rect.size.height,
            ],
            brush_flags,
        )
    );
}

fn add_segment(
    task_rect: DeviceRect,
    style0: BorderStyle,
    style1: BorderStyle,
    color0: ColorF,
    color1: ColorF,
    segment: BorderSegment,
    instances: &mut Vec<BorderInstance>,
    widths: DeviceSize,
    radius: DeviceSize,
) {
    let base_flags = (segment as i32) |
                     ((style0 as i32) << 8) |
                     ((style1 as i32) << 16);

    let base_instance = BorderInstance {
        task_origin: DevicePoint::zero(),
        local_rect: task_rect,
        flags: base_flags,
        color0: color0.premultiplied(),
        color1: color1.premultiplied(),
        widths,
        radius,
        clip_params: [0.0; 8],
    };

    match segment {
        BorderSegment::TopLeft |
        BorderSegment::TopRight |
        BorderSegment::BottomLeft |
        BorderSegment::BottomRight => {
            // TODO(gw): Similarly to the old border code, we don't correctly handle a a corner
            //           that is dashed on one edge, and dotted on another. We can handle this
            //           in the future by submitting two instances, each one with one side
            //           color set to have an alpha of 0.
            if (style0 == BorderStyle::Dotted && style1 == BorderStyle::Dashed) ||
               (style0 == BorderStyle::Dashed && style0 == BorderStyle::Dotted) {
                warn!("TODO: Handle a corner with dotted / dashed transition.");
            }

            let clip_kind = match style0 {
                BorderStyle::Dashed => Some(BorderCornerClipKind::Dash),
                BorderStyle::Dotted => Some(BorderCornerClipKind::Dot),
                _ => None,
            };

            match clip_kind {
                Some(clip_kind) => {
                    let clip_source = BorderCornerClipSource::new(
                        radius,
                        widths,
                        clip_kind,
                    );

                    // TODO(gw): Restructure the BorderCornerClipSource code
                    //           so that we don't allocate a Vec here.
                    let clip_list = clip_source.write(segment);

                    for params in clip_list {
                        instances.push(BorderInstance {
                            flags: base_flags | ((clip_kind as i32) << 24),
                            clip_params: params,
                            ..base_instance
                        });
                    }
                }
                None => {
                    instances.push(base_instance);
                }
            }
        }
        BorderSegment::Top |
        BorderSegment::Bottom |
        BorderSegment::Right |
        BorderSegment::Left => {
            let is_vertical = segment == BorderSegment::Left ||
                              segment == BorderSegment::Right;

            match style0 {
                BorderStyle::Dashed => {
                    let rect = if is_vertical {
                        let half_dash_size = task_rect.size.height * 0.5;
                        let y0 = task_rect.origin.y;
                        let y1 = y0 + half_dash_size.round();

                        DeviceRect::from_floats(
                            task_rect.origin.x,
                            y0,
                            task_rect.origin.x + task_rect.size.width,
                            y1,
                        )
                    } else {
                        let half_dash_size = task_rect.size.width * 0.5;
                        let x0 = task_rect.origin.x;
                        let x1 = x0 + half_dash_size.round();

                        DeviceRect::from_floats(
                            x0,
                            task_rect.origin.y,
                            x1,
                            task_rect.origin.y + task_rect.size.height,
                        )
                    };

                    instances.push(BorderInstance {
                        local_rect: rect,
                        ..base_instance
                    });
                }
                BorderStyle::Dotted => {
                    let (x, y, r) = if is_vertical {
                        (widths.width * 0.5,
                         widths.width,
                         widths.width * 0.5)
                    } else {
                        (widths.height,
                         widths.height * 0.5,
                         widths.height * 0.5)
                    };

                    instances.push(BorderInstance {
                        flags: base_flags | ((BorderCornerClipKind::Dot as i32) << 24),
                        clip_params: [
                            x, y, r, 0.0, 0.0, 0.0, 0.0, 0.0,
                        ],
                        ..base_instance
                    });
                }
                _ => {
                    instances.push(base_instance);
                }
            }
        }
    }
}

fn add_corner_segment(
    image_rect: LayoutRect,
    task_rect: DeviceRect,
    side0: &BorderSide,
    side1: &BorderSide,
    widths: DeviceSize,
    radius: DeviceSize,
    segment: BorderSegment,
    edge_flags: EdgeAaSegmentMask,
    border_segments: &mut Vec<BorderSegmentInfo>,
    brush_segments: &mut Vec<BrushSegment>,
) {
    if side0.color.a <= 0.0 && side1.color.a <= 0.0 {
        return;
    }

    if widths.width <= 0.0 && widths.height <= 0.0 {
        return;
    }

    if side0.style.is_hidden() && side1.style.is_hidden() {
        return;
    }

    border_segments.push(BorderSegmentInfo {
        task_rect,
        segment,
        radius,
        widths,
    });

    add_brush_segment(
        image_rect,
        task_rect,
        BrushFlags::SEGMENT_RELATIVE,
        edge_flags,
        brush_segments,
    );
}

fn add_edge_segment(
    image_rect: LayoutRect,
    task_rect: DeviceRect,
    side: &BorderSide,
    segment: BorderSegment,
    edge_flags: EdgeAaSegmentMask,
    border_segments: &mut Vec<BorderSegmentInfo>,
    brush_flags: BrushFlags,
    brush_segments: &mut Vec<BrushSegment>,
) {
    if side.color.a <= 0.0 {
        return;
    }

    if side.style.is_hidden() {
        return;
    }

    border_segments.push(BorderSegmentInfo {
        task_rect,
        segment,
        radius: DeviceSize::zero(),
        widths: task_rect.size,
    });

    add_brush_segment(
        image_rect,
        task_rect,
        brush_flags,
        edge_flags,
        brush_segments,
    );
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, BorderSide, BorderStyle, BorderWidths, ColorF, LayoutPoint};
use api::{ColorU, DeviceRect, DeviceSize, LayoutSizeAu, LayoutPrimitiveInfo, LayoutToDeviceScale};
use api::{DevicePoint, DeviceIntSize, LayoutRect, LayoutSize, NormalBorder};
use app_units::Au;
use clip::ClipSource;
use ellipse::Ellipse;
use display_list_flattener::DisplayListFlattener;
use gpu_types::{BorderInstance, BorderSegment, BrushFlags};
use gpu_cache::GpuDataRequest;
use prim_store::{BorderPrimitiveCpu, BrushClipMaskKind, BrushKind, BrushPrimitive, BrushSegment, BrushSegmentDescriptor};
use prim_store::{BorderSource, EdgeAaSegmentMask, PrimitiveContainer, ScrollNodeAndClipChain};
use util::{lerp, pack_as_float, RectHelpers};

#[repr(u8)]
#[derive(Debug, Copy, Clone, PartialEq)]
pub enum BorderCornerInstance {
    None,
    Single, // Single instance needed - corner styles are same or similar.
    Double, // Different corner styles. Draw two instances, one per style.
}

#[repr(C)]
pub enum BorderCornerSide {
    Both,
    First,
    Second,
}

#[repr(C)]
enum BorderCorner {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
}

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

#[derive(Clone, Debug, PartialEq)]
pub enum BorderCornerKind {
    None,
    Solid,
    Clip(BorderCornerInstance),
    Mask(
        BorderCornerClipData,
        LayoutSize,
        LayoutSize,
        BorderCornerClipKind,
    ),
}

impl BorderCornerKind {
    fn new_mask(
        kind: BorderCornerClipKind,
        width0: f32,
        width1: f32,
        corner: BorderCorner,
        radius: LayoutSize,
        border_rect: LayoutRect,
    ) -> BorderCornerKind {
        let size = LayoutSize::new(width0.max(radius.width), width1.max(radius.height));
        let (origin, clip_center) = match corner {
            BorderCorner::TopLeft => {
                let origin = border_rect.origin;
                let clip_center = origin + size;
                (origin, clip_center)
            }
            BorderCorner::TopRight => {
                let origin = LayoutPoint::new(
                    border_rect.origin.x + border_rect.size.width - size.width,
                    border_rect.origin.y,
                );
                let clip_center = origin + LayoutSize::new(0.0, size.height);
                (origin, clip_center)
            }
            BorderCorner::BottomRight => {
                let origin = border_rect.origin + (border_rect.size - size);
                let clip_center = origin;
                (origin, clip_center)
            }
            BorderCorner::BottomLeft => {
                let origin = LayoutPoint::new(
                    border_rect.origin.x,
                    border_rect.origin.y + border_rect.size.height - size.height,
                );
                let clip_center = origin + LayoutSize::new(size.width, 0.0);
                (origin, clip_center)
            }
        };
        let clip_data = BorderCornerClipData {
            corner_rect: LayoutRect::new(origin, size),
            clip_center,
            corner: pack_as_float(corner as u32),
            kind: pack_as_float(kind as u32),
        };
        BorderCornerKind::Mask(clip_data, radius, LayoutSize::new(width0, width1), kind)
    }

    fn get_radius(&self, original_radius: &LayoutSize) -> LayoutSize {
        match *self {
            BorderCornerKind::Solid => *original_radius,
            BorderCornerKind::Clip(..) => *original_radius,
            BorderCornerKind::Mask(_, ref radius, _, _) => *radius,
            BorderCornerKind::None => *original_radius,
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum BorderEdgeKind {
    None,
    Solid,
    Clip,
}

fn get_corner(
    edge0: &BorderSide,
    width0: f32,
    edge1: &BorderSide,
    width1: f32,
    radius: &LayoutSize,
    corner: BorderCorner,
    border_rect: &LayoutRect,
) -> BorderCornerKind {
    // If both widths are zero, a corner isn't formed.
    if width0 == 0.0 && width1 == 0.0 {
        return BorderCornerKind::None;
    }

    // If both edges are transparent, no corner is formed.
    if edge0.color.a == 0.0 && edge1.color.a == 0.0 {
        return BorderCornerKind::None;
    }

    match (edge0.style, edge1.style) {
        // If both edges are none or hidden, no corner is needed.
        (BorderStyle::None, BorderStyle::None) |
        (BorderStyle::None, BorderStyle::Hidden) |
        (BorderStyle::Hidden, BorderStyle::None) |
        (BorderStyle::Hidden, BorderStyle::Hidden) => {
            BorderCornerKind::None
        }

        // If one of the edges is none or hidden, we just draw one style.
        (BorderStyle::None, _) |
        (_, BorderStyle::None) |
        (BorderStyle::Hidden, _) |
        (_, BorderStyle::Hidden) => {
            BorderCornerKind::Clip(BorderCornerInstance::Single)
        }

        // If both borders are solid, we can draw them with a simple rectangle if
        // both the colors match and there is no radius.
        (BorderStyle::Solid, BorderStyle::Solid) => {
            if edge0.color == edge1.color && radius.width == 0.0 && radius.height == 0.0 {
                BorderCornerKind::Solid
            } else {
                BorderCornerKind::Clip(BorderCornerInstance::Single)
            }
        }

        // Inset / outset borders just modify the color of edges, so can be
        // drawn with the normal border corner shader.
        (BorderStyle::Outset, BorderStyle::Outset) |
        (BorderStyle::Inset, BorderStyle::Inset) |
        (BorderStyle::Double, BorderStyle::Double) |
        (BorderStyle::Groove, BorderStyle::Groove) |
        (BorderStyle::Ridge, BorderStyle::Ridge) => {
            BorderCornerKind::Clip(BorderCornerInstance::Single)
        }

        // Dashed and dotted border corners get drawn into a clip mask.
        (BorderStyle::Dashed, BorderStyle::Dashed) => BorderCornerKind::new_mask(
            BorderCornerClipKind::Dash,
            width0,
            width1,
            corner,
            *radius,
            *border_rect,
        ),
        (BorderStyle::Dotted, BorderStyle::Dotted) => {
            let mut radius = *radius;
            if radius.width < width0 {
                radius.width = 0.0;
            }
            if radius.height < width1 {
                radius.height = 0.0;
            }
            BorderCornerKind::new_mask(
                BorderCornerClipKind::Dot,
                width0,
                width1,
                corner,
                radius,
                *border_rect,
             )
        }

        // Draw border transitions with dots and/or dashes as
        // solid segments. The old border path didn't support
        // this anyway, so we might as well start using the new
        // border path here, since the dashing in the edges is
        // much higher quality anyway.
        (BorderStyle::Dotted, _) |
        (_, BorderStyle::Dotted) |
        (BorderStyle::Dashed, _) |
        (_, BorderStyle::Dashed) => BorderCornerKind::Clip(BorderCornerInstance::Single),

        // Everything else can be handled by drawing the corner twice,
        // where the shader outputs zero alpha for the side it's not
        // drawing. This is somewhat inefficient in terms of pixels
        // written, but it's a fairly rare case, and we can optimize
        // this case later.
        _ => BorderCornerKind::Clip(BorderCornerInstance::Double),
    }
}

fn get_edge(edge: &BorderSide, width: f32, height: f32) -> (BorderEdgeKind, f32) {
    if width == 0.0 || height <= 0.0 {
        return (BorderEdgeKind::None, 0.0);
    }

    match edge.style {
        BorderStyle::None | BorderStyle::Hidden => (BorderEdgeKind::None, 0.0),

        BorderStyle::Solid | BorderStyle::Inset | BorderStyle::Outset => {
            (BorderEdgeKind::Solid, width)
        }

        BorderStyle::Double |
        BorderStyle::Groove |
        BorderStyle::Ridge |
        BorderStyle::Dashed |
        BorderStyle::Dotted => (BorderEdgeKind::Clip, width),
    }
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
    fn add_normal_border_primitive(
        &mut self,
        info: &LayoutPrimitiveInfo,
        border: &NormalBorder,
        radius: &BorderRadius,
        widths: &BorderWidths,
        clip_and_scroll: ScrollNodeAndClipChain,
        corner_instances: [BorderCornerInstance; 4],
        edges: [BorderEdgeKind; 4],
        clip_sources: Vec<ClipSource>,
    ) {
        let left = &border.left;
        let right = &border.right;
        let top = &border.top;
        let bottom = &border.bottom;

        // These colors are used during inset/outset scaling.
        let left_color = left.border_color(1.0, 2.0 / 3.0, 0.3, 0.7).premultiplied();
        let top_color = top.border_color(1.0, 2.0 / 3.0, 0.3, 0.7).premultiplied();
        let right_color = right.border_color(2.0 / 3.0, 1.0, 0.7, 0.3).premultiplied();
        let bottom_color = bottom.border_color(2.0 / 3.0, 1.0, 0.7, 0.3).premultiplied();

        let prim_cpu = BorderPrimitiveCpu {
            corner_instances,
            edges,

            // TODO(gw): In the future, we will build these on demand
            //           from the deserialized display list, rather
            //           than creating it immediately.
            gpu_blocks: [
                [
                    pack_as_float(left.style as u32),
                    pack_as_float(top.style as u32),
                    pack_as_float(right.style as u32),
                    pack_as_float(bottom.style as u32),
                ].into(),
                [widths.left, widths.top, widths.right, widths.bottom].into(),
                left_color.into(),
                top_color.into(),
                right_color.into(),
                bottom_color.into(),
                [
                    radius.top_left.width,
                    radius.top_left.height,
                    radius.top_right.width,
                    radius.top_right.height,
                ].into(),
                [
                    radius.bottom_right.width,
                    radius.bottom_right.height,
                    radius.bottom_left.width,
                    radius.bottom_left.height,
                ].into(),
            ],
        };

        self.add_primitive(
            clip_and_scroll,
            info,
            clip_sources,
            PrimitiveContainer::Border(prim_cpu),
        );
    }

    // TODO(gw): This allows us to move border types over to the
    // simplified shader model one at a time. Once all borders
    // are converted, this can be removed, along with the complex
    // border code path.
    pub fn add_normal_border(
        &mut self,
        info: &LayoutPrimitiveInfo,
        border: &NormalBorder,
        widths: &BorderWidths,
        clip_and_scroll: ScrollNodeAndClipChain,
    ) {
        // The border shader is quite expensive. For simple borders, we can just draw
        // the border with a few rectangles. This generally gives better batching, and
        // a GPU win in fragment shader time.
        // More importantly, the software (OSMesa) implementation we run tests on is
        // particularly slow at running our complex border shader, compared to the
        // rectangle shader. This has the effect of making some of our tests time
        // out more often on CI (the actual cause is simply too many Servo processes and
        // threads being run on CI at once).

        let mut border = *border;
        ensure_no_corner_overlap(&mut border.radius, &info.rect);

        let radius = &border.radius;
        let left = &border.left;
        let right = &border.right;
        let top = &border.top;
        let bottom = &border.bottom;

        let brush_border_supported = [left, top, right, bottom].iter().all(|edge| {
            match edge.style {
                BorderStyle::Solid |
                BorderStyle::Hidden |
                BorderStyle::None |
                BorderStyle::Inset |
                BorderStyle::Outset => {
                    true
                }

                BorderStyle::Double |
                BorderStyle::Dotted |
                BorderStyle::Dashed |
                BorderStyle::Groove |
                BorderStyle::Ridge => {
                    false
                }
            }
        });

        if brush_border_supported {
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
            return;
        }

        let corners = [
            get_corner(
                left,
                widths.left,
                top,
                widths.top,
                &radius.top_left,
                BorderCorner::TopLeft,
                &info.rect,
            ),
            get_corner(
                right,
                widths.right,
                top,
                widths.top,
                &radius.top_right,
                BorderCorner::TopRight,
                &info.rect,
            ),
            get_corner(
                right,
                widths.right,
                bottom,
                widths.bottom,
                &radius.bottom_right,
                BorderCorner::BottomRight,
                &info.rect,
            ),
            get_corner(
                left,
                widths.left,
                bottom,
                widths.bottom,
                &radius.bottom_left,
                BorderCorner::BottomLeft,
                &info.rect,
            ),
        ];

        let (left_edge, left_len) = get_edge(left, widths.left,
            info.rect.size.height - radius.top_left.height - radius.bottom_left.height);
        let (top_edge, top_len) = get_edge(top, widths.top,
            info.rect.size.width - radius.top_left.width - radius.top_right.width);
        let (right_edge, right_len) = get_edge(right, widths.right,
            info.rect.size.height - radius.top_right.height - radius.bottom_right.height);
        let (bottom_edge, bottom_len) = get_edge(bottom, widths.bottom,
            info.rect.size.width - radius.bottom_right.width - radius.bottom_left.width);

        let edges = [left_edge, top_edge, right_edge, bottom_edge];

        // Use a simple rectangle case when all edges and corners are either
        // solid or none.
        let all_corners_simple = corners.iter().all(|c| {
            *c == BorderCornerKind::Solid || *c == BorderCornerKind::None
        });
        let all_edges_simple = edges.iter().all(|e| {
            *e == BorderEdgeKind::Solid || *e == BorderEdgeKind::None
        });

        let has_no_curve = radius.is_zero();

        if has_no_curve && all_corners_simple && all_edges_simple {
            let p0 = info.rect.origin;
            let p1 = LayoutPoint::new(
                info.rect.origin.x + left_len,
                info.rect.origin.y + top_len,
            );
            let p2 = LayoutPoint::new(
                info.rect.origin.x + info.rect.size.width - right_len,
                info.rect.origin.y + info.rect.size.height - bottom_len,
            );
            let p3 = info.rect.bottom_right();

            let segment = |x0, y0, x1, y1| BrushSegment::new(
                LayoutRect::from_floats(x0, y0, x1, y1),
                true,
                EdgeAaSegmentMask::all(), // Note: this doesn't seem right, needs revision
                [0.0; 4],
                BrushFlags::empty(),
            );

            // Add a solid rectangle for each visible edge/corner combination.
            if top_edge == BorderEdgeKind::Solid {
                let descriptor = BrushSegmentDescriptor {
                    segments: vec![
                        segment(p0.x, p0.y, p1.x, p1.y),
                        segment(p2.x, p0.y, p3.x, p1.y),
                        segment(p1.x, p0.y, p2.x, p1.y),
                    ],
                    clip_mask_kind: BrushClipMaskKind::Unknown,
                };

                self.add_solid_rectangle(
                    clip_and_scroll,
                    info,
                    border.top.color,
                    Some(descriptor),
                    Vec::new(),
                );
            }

            if left_edge == BorderEdgeKind::Solid {
                let descriptor = BrushSegmentDescriptor {
                    segments: vec![
                        segment(p0.x, p1.y, p1.x, p2.y),
                    ],
                    clip_mask_kind: BrushClipMaskKind::Unknown,
                };

                self.add_solid_rectangle(
                    clip_and_scroll,
                    info,
                    border.left.color,
                    Some(descriptor),
                    Vec::new(),
                );
            }

            if right_edge == BorderEdgeKind::Solid {
                let descriptor = BrushSegmentDescriptor {
                    segments: vec![
                        segment(p2.x, p1.y, p3.x, p2.y),
                    ],
                    clip_mask_kind: BrushClipMaskKind::Unknown,
                };

                self.add_solid_rectangle(
                    clip_and_scroll,
                    info,
                    border.right.color,
                    Some(descriptor),
                    Vec::new(),
                );
            }

            if bottom_edge == BorderEdgeKind::Solid {
                let descriptor = BrushSegmentDescriptor {
                    segments: vec![
                        segment(p1.x, p2.y, p2.x, p3.y),
                        segment(p2.x, p2.y, p3.x, p3.y),
                        segment(p0.x, p2.y, p1.x, p3.y),
                    ],
                    clip_mask_kind: BrushClipMaskKind::Unknown,
                };

                self.add_solid_rectangle(
                    clip_and_scroll,
                    info,
                    border.bottom.color,
                    Some(descriptor),
                    Vec::new(),
                );
            }
        } else {
            // Create clip masks for border corners, if required.
            let mut extra_clips = Vec::new();
            let mut corner_instances = [BorderCornerInstance::Single; 4];

            let radius = &border.radius;
            let radius = BorderRadius {
                top_left: corners[0].get_radius(&radius.top_left),
                top_right: corners[1].get_radius(&radius.top_right),
                bottom_right: corners[2].get_radius(&radius.bottom_right),
                bottom_left: corners[3].get_radius(&radius.bottom_left),
            };

            for (i, corner) in corners.iter().enumerate() {
                match *corner {
                    BorderCornerKind::Mask(corner_data, mut corner_radius, widths, kind) => {
                        let clip_source =
                            BorderCornerClipSource::new(corner_data, corner_radius, widths, kind);
                        extra_clips.push(ClipSource::BorderCorner(clip_source));
                    }
                    BorderCornerKind::Clip(instance_kind) => {
                        corner_instances[i] = instance_kind;
                    }
                    BorderCornerKind::Solid => {}
                    BorderCornerKind::None => {
                        corner_instances[i] = BorderCornerInstance::None;
                    }
                }
            }

            self.add_normal_border_primitive(
                info,
                &border,
                &radius,
                widths,
                clip_and_scroll,
                corner_instances,
                edges,
                extra_clips,
            );
        }
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
    Dash,
    Dot,
}

/// The source data for a border corner clip mask.
#[derive(Debug, Clone)]
pub struct BorderCornerClipSource {
    pub corner_data: BorderCornerClipData,
    pub max_clip_count: usize,
    kind: BorderCornerClipKind,
    widths: LayoutSize,
    ellipse: Ellipse,
    pub dot_dash_data: Vec<[f32; 8]>,
}

impl BorderCornerClipSource {
    pub fn new(
        corner_data: BorderCornerClipData,
        corner_radius: LayoutSize,
        widths: LayoutSize,
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
                (ellipse, 1 + desired_count.ceil() as usize)
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
                    (ellipse, 1 + max_dot_count.ceil() as usize)
                }
            }
        };

        BorderCornerClipSource {
            kind,
            corner_data,
            max_clip_count,
            ellipse,
            widths,
            dot_dash_data: Vec::new(),
        }
    }

    pub fn write(&mut self, mut request: GpuDataRequest) {
        self.corner_data.write(&mut request);
        assert_eq!(request.close(), 2);

        match self.kind {
            BorderCornerClipKind::Dash => {
                // Get the correct dash arc length.
                let dash_arc_length =
                    0.5 * self.ellipse.total_arc_length / (self.max_clip_count - 1) as f32;
                self.dot_dash_data.clear();
                let mut current_arc_length = -0.5 * dash_arc_length;
                for _ in 0 .. self.max_clip_count {
                    let arc_length0 = current_arc_length;
                    current_arc_length += dash_arc_length;

                    let arc_length1 = current_arc_length;
                    current_arc_length += dash_arc_length;

                    let alpha = self.ellipse.find_angle_for_arc_length(arc_length0);
                    let beta =  self.ellipse.find_angle_for_arc_length(arc_length1);

                    let (point0, tangent0) =  self.ellipse.get_point_and_tangent(alpha);
                    let (point1, tangent1) =  self.ellipse.get_point_and_tangent(beta);

                    self.dot_dash_data.push([
                        point0.x, point0.y, tangent0.x, tangent0.y,
                        point1.x, point1.y, tangent1.x, tangent1.y
                    ]);
                }
            }
            BorderCornerClipKind::Dot if self.max_clip_count == 1 => {
                let dot_diameter = lerp(self.widths.width, self.widths.height, 0.5);
                self.dot_dash_data.clear();
                self.dot_dash_data.push([
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
                forward_dots.push(DotInfo::new(0.0, self.widths.width));
                back_dots.push(DotInfo::new(
                    self.ellipse.total_arc_length,
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

                self.dot_dash_data.clear();

                let create_dot_data = |ellipse: &Ellipse, arc_length: f32, radius: f32| -> [f32; 8] {
                    // Represents the GPU data for drawing a single dot to a clip mask. The order
                    // these are specified must stay in sync with the way this data is read in the
                    // dot clip shader.
                    let theta = ellipse.find_angle_for_arc_length(arc_length);
                    let (center, _) = ellipse.get_point_and_tangent(theta);
                    [center.x, center.y, radius, 0., 0., 0., 0., 0.,]
                };

                for (i, dot) in forward_dots.iter().enumerate() {
                    let extra_dist = i as f32 * extra_space_per_dot;
                    let dot_data = create_dot_data(&self.ellipse, dot.arc_pos + extra_dist, 0.5 * dot.diameter);
                    self.dot_dash_data.push(dot_data);
                }

                for (i, dot) in back_dots.iter().enumerate() {
                    let extra_dist = i as f32 * extra_space_per_dot;
                    let dot_data = create_dot_data(&self.ellipse, dot.arc_pos - extra_dist, 0.5 * dot.diameter);
                    self.dot_dash_data.push(dot_data);
                }
            }
        }
    }
}

/// Represents the common GPU data for writing a
/// clip mask for a border corner.
#[derive(Debug, Copy, Clone, PartialEq)]
#[repr(C)]
pub struct BorderCornerClipData {
    /// Local space rect of the border corner.
    corner_rect: LayoutRect,
    /// Local space point that is the center of the
    /// circle or ellipse that we are clipping against.
    clip_center: LayoutPoint,
    /// The shader needs to know which corner, to
    /// be able to flip the dash tangents to the
    /// right orientation.
    corner: f32, // Of type BorderCorner enum
    kind: f32, // Of type BorderCornerClipKind enum
}

impl BorderCornerClipData {
    fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.corner_rect);
        request.push([
            self.clip_center.x,
            self.clip_center.y,
            self.corner,
            self.kind,
        ]);
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
pub struct BorderRenderTaskInfo {
    pub instances: Vec<BorderInstance>,
    pub segments: Vec<BrushSegment>,
    pub size: DeviceIntSize,
}

impl BorderRenderTaskInfo {
    pub fn new(
        rect: &LayoutRect,
        border: &NormalBorder,
        widths: &BorderWidths,
        scale: LayoutToDeviceScale,
    ) -> Self {
        let mut instances = Vec::new();
        let mut segments = Vec::new();

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

        // TODO(gw): The inner and outer widths don't matter for simple
        //           border types. Once we push dashing and dotted styles
        //           through border brushes, we need to calculate an
        //           appropriate length here.
        let width_inner = 16.0;
        let height_inner = 16.0;

        let size = DeviceSize::new(
            dp_size_tl.width.max(dp_size_bl.width) + width_inner + dp_size_tr.width.max(dp_size_br.width),
            dp_size_tl.height.max(dp_size_tr.height) + height_inner + dp_size_bl.height.max(dp_size_br.height),
        );

        // These modulate colors are not part of the specification. They
        // are derived from the Gecko source code and experimentation, and
        // used to modulate the colors in order to generate colors for
        // the inset/outset and groove/ridge border styles.
        let left_color = border.left.border_color(1.0, 2.0 / 3.0, 0.3, 0.7);
        let top_color = border.top.border_color(1.0, 2.0 / 3.0, 0.3, 0.7);
        let right_color = border.right.border_color(2.0 / 3.0, 1.0, 0.7, 0.3);
        let bottom_color = border.bottom.border_color(2.0 / 3.0, 1.0, 0.7, 0.3);

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x,
                rect.origin.y + local_size_tl.height,
                rect.origin.x + widths.left,
                rect.origin.y + rect.size.height - local_size_bl.height,
            ),
            DeviceRect::from_floats(
                0.0,
                dp_size_tl.height,
                dp_width_left,
                size.height - dp_size_bl.height,
            ),
            border.left.style,
            left_color,
            BorderSegment::Left,
            EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::RIGHT,
            &mut instances,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_Y,
            &mut segments,
        );

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x + local_size_tl.width,
                rect.origin.y,
                rect.origin.x + rect.size.width - local_size_tr.width,
                rect.origin.y + widths.top,
            ),
            DeviceRect::from_floats(
                dp_size_tl.width,
                0.0,
                size.width - dp_size_tr.width,
                dp_width_top,
            ),
            border.top.style,
            top_color,
            BorderSegment::Top,
            EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::BOTTOM,
            &mut instances,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_X,
            &mut segments,
        );

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x + rect.size.width - widths.right,
                rect.origin.y + local_size_tr.height,
                rect.origin.x + rect.size.width,
                rect.origin.y + rect.size.height - local_size_br.height,
            ),
            DeviceRect::from_floats(
                size.width - dp_width_right,
                dp_size_tr.height,
                size.width,
                size.height - dp_size_br.height,
            ),
            border.right.style,
            right_color,
            BorderSegment::Right,
            EdgeAaSegmentMask::RIGHT | EdgeAaSegmentMask::LEFT,
            &mut instances,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_Y,
            &mut segments,
        );

        add_edge_segment(
            LayoutRect::from_floats(
                rect.origin.x + local_size_bl.width,
                rect.origin.y + rect.size.height - widths.bottom,
                rect.origin.x + rect.size.width - local_size_br.width,
                rect.origin.y + rect.size.height,
            ),
            DeviceRect::from_floats(
                dp_size_bl.width,
                size.height - dp_width_bottom,
                size.width - dp_size_br.width,
                size.height,
            ),
            border.bottom.style,
            bottom_color,
            BorderSegment::Bottom,
            EdgeAaSegmentMask::BOTTOM | EdgeAaSegmentMask::TOP,
            &mut instances,
            BrushFlags::SEGMENT_RELATIVE | BrushFlags::SEGMENT_REPEAT_X,
            &mut segments,
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
            border.left.style,
            left_color,
            border.top.style,
            top_color,
            DeviceSize::new(dp_width_left, dp_width_top),
            dp_corner_tl,
            BorderSegment::TopLeft,
            EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::LEFT,
            &mut instances,
            &mut segments,
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
            border.top.style,
            top_color,
            border.right.style,
            right_color,
            DeviceSize::new(dp_width_right, dp_width_top),
            dp_corner_tr,
            BorderSegment::TopRight,
            EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::RIGHT,
            &mut instances,
            &mut segments,
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
            border.right.style,
            right_color,
            border.bottom.style,
            bottom_color,
            DeviceSize::new(dp_width_right, dp_width_bottom),
            dp_corner_br,
            BorderSegment::BottomRight,
            EdgeAaSegmentMask::BOTTOM | EdgeAaSegmentMask::RIGHT,
            &mut instances,
            &mut segments,
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
            border.bottom.style,
            bottom_color,
            border.left.style,
            left_color,
            DeviceSize::new(dp_width_left, dp_width_bottom),
            dp_corner_bl,
            BorderSegment::BottomLeft,
            EdgeAaSegmentMask::BOTTOM | EdgeAaSegmentMask::LEFT,
            &mut instances,
            &mut segments,
        );

        BorderRenderTaskInfo {
            segments,
            instances,
            size: size.to_i32(),
        }
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
    let flags = (segment as i32) |
                ((style0 as i32) << 8) |
                ((style1 as i32) << 16);

    let base_instance = BorderInstance {
        task_origin: DevicePoint::zero(),
        local_rect: task_rect,
        flags,
        color0: color0.premultiplied(),
        color1: color1.premultiplied(),
        widths,
        radius,
    };

    instances.push(base_instance);
}

fn add_corner_segment(
    image_rect: LayoutRect,
    task_rect: DeviceRect,
    mut style0: BorderStyle,
    color0: ColorF,
    mut style1: BorderStyle,
    color1: ColorF,
    widths: DeviceSize,
    radius: DeviceSize,
    segment: BorderSegment,
    edge_flags: EdgeAaSegmentMask,
    instances: &mut Vec<BorderInstance>,
    brush_segments: &mut Vec<BrushSegment>,
) {
    // TODO(gw): This will need to be a bit more involved when
    //           we support other border types here. For example,
    //           groove / ridge borders will always need to
    //           use two instances.

    if color0.a <= 0.0 && color1.a <= 0.0 {
        return;
    }

    if widths.width <= 0.0 && widths.height <= 0.0 {
        return;
    }

    let style0_hidden = style0 == BorderStyle::Hidden || style0 == BorderStyle::None;
    let style1_hidden = style1 == BorderStyle::Hidden || style1 == BorderStyle::None;

    if style0_hidden && style1_hidden {
        return;
    }

    if style0_hidden {
        style0 = style1;
    }
    if style1_hidden {
        style1 = style0;
    }

    add_segment(
        task_rect,
        style0,
        style1,
        color0,
        color1,
        segment,
        instances,
        widths,
        radius,
    );

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
    style: BorderStyle,
    color: ColorF,
    segment: BorderSegment,
    edge_flags: EdgeAaSegmentMask,
    instances: &mut Vec<BorderInstance>,
    brush_flags: BrushFlags,
    brush_segments: &mut Vec<BrushSegment>,
) {
    if color.a <= 0.0 {
        return;
    }

    if style == BorderStyle::Hidden || style == BorderStyle::None {
        return;
    }

    add_segment(
        task_rect,
        style,
        style,
        color,
        color,
        segment,
        instances,
        DeviceSize::zero(),
        DeviceSize::zero(),
    );

    add_brush_segment(
        image_rect,
        task_rect,
        brush_flags,
        edge_flags,
        brush_segments,
    );
}

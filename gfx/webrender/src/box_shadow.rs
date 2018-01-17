/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, LayerPoint, LayerRect, LayerSize, LayerVector2D};
use api::{BorderRadius, BoxShadowClipMode, LayoutSize, LayerPrimitiveInfo};
use api::{ClipMode, ClipAndScrollInfo, ComplexClipRegion, LocalClip};
use api::{PipelineId};
use app_units::Au;
use clip::ClipSource;
use frame_builder::FrameBuilder;
use gpu_types::BrushImageKind;
use prim_store::{PrimitiveContainer};
use prim_store::{BrushMaskKind, BrushKind, BrushPrimitive};
use picture::PicturePrimitive;
use util::RectHelpers;
use render_task::MAX_BLUR_STD_DEVIATION;

// The blur shader samples BLUR_SAMPLE_SCALE * blur_radius surrounding texels.
pub const BLUR_SAMPLE_SCALE: f32 = 3.0;

// Maximum blur radius.
// Taken from https://searchfox.org/mozilla-central/rev/c633ffa4c4611f202ca11270dcddb7b29edddff8/layout/painting/nsCSSRendering.cpp#4412
pub const MAX_BLUR_RADIUS : f32 = 300.;

// The amount of padding added to the border corner drawn in the box shadow
// mask. This ensures that we get a few pixels past the corner that can be
// blurred without being affected by the border radius.
pub const MASK_CORNER_PADDING: f32 = 4.0;

#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
pub struct BoxShadowCacheKey {
    pub width: Au,
    pub height: Au,
    pub blur_radius: Au,
    pub spread_radius: Au,
    pub offset_x: Au,
    pub offset_y: Au,
    pub br_top_left_w: Au,
    pub br_top_left_h: Au,
    pub br_top_right_w: Au,
    pub br_top_right_h: Au,
    pub br_bottom_left_w: Au,
    pub br_bottom_left_h: Au,
    pub br_bottom_right_w: Au,
    pub br_bottom_right_h: Au,
    pub clip_mode: BoxShadowClipMode,
}

impl FrameBuilder {
    pub fn add_box_shadow(
        &mut self,
        pipeline_id: PipelineId,
        clip_and_scroll: ClipAndScrollInfo,
        prim_info: &LayerPrimitiveInfo,
        box_offset: &LayerVector2D,
        color: &ColorF,
        mut blur_radius: f32,
        spread_radius: f32,
        border_radius: BorderRadius,
        clip_mode: BoxShadowClipMode,
    ) {
        if color.a == 0.0 {
            return;
        }

        let (spread_amount, brush_clip_mode) = match clip_mode {
            BoxShadowClipMode::Outset => {
                (spread_radius, ClipMode::Clip)
            }
            BoxShadowClipMode::Inset => {
                (-spread_radius, ClipMode::ClipOut)
            }
        };

        blur_radius = f32::min(blur_radius, MAX_BLUR_RADIUS);
        let shadow_radius = adjust_border_radius_for_box_shadow(
            border_radius,
            spread_amount,
        );
        let shadow_rect = prim_info.rect
            .translate(box_offset)
            .inflate(spread_amount, spread_amount);

        if blur_radius == 0.0 {
            if box_offset.x == 0.0 && box_offset.y == 0.0 && spread_amount == 0.0 {
                return;
            }
            let mut clips = Vec::new();

            let fast_info = match clip_mode {
                BoxShadowClipMode::Outset => {
                    // TODO(gw): Add a fast path for ClipOut + zero border radius!
                    clips.push(ClipSource::new_rounded_rect(
                        prim_info.rect,
                        border_radius,
                        ClipMode::ClipOut
                    ));

                    LayerPrimitiveInfo::with_clip(
                        shadow_rect,
                        LocalClip::RoundedRect(
                            shadow_rect,
                            ComplexClipRegion::new(
                                shadow_rect,
                                shadow_radius,
                                ClipMode::Clip,
                            ),
                        ),
                    )
                }
                BoxShadowClipMode::Inset => {
                    clips.push(ClipSource::new_rounded_rect(
                        shadow_rect,
                        shadow_radius,
                        ClipMode::ClipOut
                    ));

                    LayerPrimitiveInfo::with_clip(
                        prim_info.rect,
                        LocalClip::RoundedRect(
                            prim_info.rect,
                            ComplexClipRegion::new(
                                prim_info.rect,
                                border_radius,
                                ClipMode::Clip
                            ),
                        ),
                    )
                }
            };

            self.add_primitive(
                clip_and_scroll,
                &fast_info,
                clips,
                PrimitiveContainer::Brush(
                    BrushPrimitive::new(BrushKind::Solid {
                            color: *color,
                        },
                        None,
                    )
                ),
            );
        } else {
            let blur_offset = BLUR_SAMPLE_SCALE * blur_radius;
            let mut extra_clips = vec![];

            let cache_key = BoxShadowCacheKey {
                width: Au::from_f32_px(shadow_rect.size.width),
                height: Au::from_f32_px(shadow_rect.size.height),
                blur_radius: Au::from_f32_px(blur_radius),
                spread_radius: Au::from_f32_px(spread_radius),
                offset_x: Au::from_f32_px(box_offset.x),
                offset_y: Au::from_f32_px(box_offset.y),
                br_top_left_w: Au::from_f32_px(border_radius.top_left.width),
                br_top_left_h: Au::from_f32_px(border_radius.top_left.height),
                br_top_right_w: Au::from_f32_px(border_radius.top_right.width),
                br_top_right_h: Au::from_f32_px(border_radius.top_right.height),
                br_bottom_left_w: Au::from_f32_px(border_radius.bottom_left.width),
                br_bottom_left_h: Au::from_f32_px(border_radius.bottom_left.height),
                br_bottom_right_w: Au::from_f32_px(border_radius.bottom_right.width),
                br_bottom_right_h: Au::from_f32_px(border_radius.bottom_right.height),
                clip_mode,
            };

            match clip_mode {
                BoxShadowClipMode::Outset => {
                    let mut width;
                    let mut height;
                    let brush_prim;
                    let corner_size = shadow_radius.is_uniform_size();
                    let mut image_kind;

                    if !shadow_rect.is_well_formed_and_nonempty() {
                        return;
                    }

                    // If the outset box shadow has a uniform corner side, we can
                    // just blur the top left corner, and stretch / mirror that
                    // across the primitive.
                    if let Some(corner_size) = corner_size {
                        image_kind = BrushImageKind::Mirror;
                        width = MASK_CORNER_PADDING + corner_size.width.max(BLUR_SAMPLE_SCALE * blur_radius);
                        height = MASK_CORNER_PADDING + corner_size.height.max(BLUR_SAMPLE_SCALE * blur_radius);

                        brush_prim = BrushPrimitive::new(
                            BrushKind::Mask {
                                clip_mode: brush_clip_mode,
                                kind: BrushMaskKind::Corner(corner_size),
                            },
                            None,
                        );
                    } else {
                        // Create a minimal size primitive mask to blur. In this
                        // case, we ensure the size of each corner is the same,
                        // to simplify the shader logic that stretches the blurred
                        // result across the primitive.
                        image_kind = BrushImageKind::NinePatch;
                        let max_width = shadow_radius.top_left.width
                                            .max(shadow_radius.bottom_left.width)
                                            .max(shadow_radius.top_right.width)
                                            .max(shadow_radius.bottom_right.width);
                        let max_height = shadow_radius.top_left.height
                                            .max(shadow_radius.bottom_left.height)
                                            .max(shadow_radius.top_right.height)
                                            .max(shadow_radius.bottom_right.height);

                        width = 2.0 * max_width + BLUR_SAMPLE_SCALE * blur_radius;
                        height = 2.0 * max_height + BLUR_SAMPLE_SCALE * blur_radius;

                        // If the width or height ends up being bigger than the original
                        // primitive shadow rect, just blur the entire rect and draw that
                        // as a simple blit.
                        if width > prim_info.rect.size.width || height > prim_info.rect.size.height {
                            image_kind = BrushImageKind::Simple;
                            width = prim_info.rect.size.width;
                            height = prim_info.rect.size.height;
                        }

                        let clip_rect = LayerRect::new(LayerPoint::zero(),
                                                       LayerSize::new(width, height));

                        brush_prim = BrushPrimitive::new(
                            BrushKind::Mask {
                                clip_mode: brush_clip_mode,
                                kind: BrushMaskKind::RoundedRect(clip_rect, shadow_radius),
                            },
                            None,
                        );
                    };

                    // Construct a mask primitive to add to the picture.
                    let brush_rect = LayerRect::new(LayerPoint::zero(),
                                                    LayerSize::new(width, height));
                    let brush_info = LayerPrimitiveInfo::new(brush_rect);
                    let brush_prim_index = self.create_primitive(
                        &brush_info,
                        Vec::new(),
                        PrimitiveContainer::Brush(brush_prim),
                    );

                    // Create a box shadow picture and add the mask primitive to it.
                    let pic_rect = shadow_rect.inflate(blur_offset, blur_offset);
                    let mut pic_prim = PicturePrimitive::new_box_shadow(
                        blur_radius,
                        *color,
                        clip_mode,
                        image_kind,
                        cache_key,
                        pipeline_id,
                    );
                    pic_prim.add_primitive(
                        brush_prim_index,
                        clip_and_scroll
                    );

                    // TODO(gw): Right now, we always use a clip out
                    //           mask for outset shadows. We can make this
                    //           much more efficient when we have proper
                    //           segment logic, by avoiding drawing
                    //           most of the pixels inside and just
                    //           clipping out along the edges.
                    extra_clips.push(ClipSource::new_rounded_rect(
                        prim_info.rect,
                        border_radius,
                        ClipMode::ClipOut,
                    ));

                    let pic_info = LayerPrimitiveInfo::new(pic_rect);
                    self.add_primitive(
                        clip_and_scroll,
                        &pic_info,
                        extra_clips,
                        PrimitiveContainer::Picture(pic_prim),
                    );
                }
                BoxShadowClipMode::Inset => {
                    // TODO(gw): Inset shadows still need an optimization pass.
                    //           We draw and blur way more pixels than needed.

                    // Draw a picture that covers the area of the primitive rect.
                    let brush_rect = LayerRect::new(
                        LayerPoint::zero(),
                        prim_info.rect.size
                    );

                    // Define where the inset box shadow rect is, local
                    // to the brush rect above.
                    let clip_rect = brush_rect.translate(box_offset)
                                              .inflate(spread_amount, spread_amount);

                    // Ensure there are more than one pixel around the edges, so that there
                    // is non-zero data to blur, in the case of an inset shadow
                    // with zero spread and zero offset.
                    // The size of inflation edge is determined by std deviation because large
                    // std deviation blur would be downscaled first. Thus, we need more thick
                    // edge to prevent edge get blurred after downscled.
                    let mut adjusted_blur_std_deviation = blur_radius * 0.5;
                    let mut inflate_size = 1.0;
                    while adjusted_blur_std_deviation > MAX_BLUR_STD_DEVIATION {
                        adjusted_blur_std_deviation *= 0.5;
                        inflate_size *= 2.0;
                    }

                    let brush_rect = brush_rect.inflate(inflate_size + box_offset.x.abs(), inflate_size + box_offset.y.abs());
                    let brush_prim = BrushPrimitive::new(
                        BrushKind::Mask {
                            clip_mode: brush_clip_mode,
                            kind: BrushMaskKind::RoundedRect(clip_rect, shadow_radius),
                        },
                        None,
                    );
                    let brush_info = LayerPrimitiveInfo::new(brush_rect);
                    let brush_prim_index = self.create_primitive(
                        &brush_info,
                        Vec::new(),
                        PrimitiveContainer::Brush(brush_prim),
                    );

                    // Create a box shadow picture primitive and add
                    // the brush primitive to it.
                    let mut pic_prim = PicturePrimitive::new_box_shadow(
                        blur_radius,
                        *color,
                        BoxShadowClipMode::Inset,
                        // TODO(gw): Make use of optimization for inset.
                        BrushImageKind::NinePatch,
                        cache_key,
                        pipeline_id,
                    );
                    pic_prim.add_primitive(
                        brush_prim_index,
                        clip_and_scroll
                    );

                    // Draw the picture one pixel outside the original
                    // rect to account for the inflate above. This
                    // extra edge will be clipped by the local clip
                    // rect set below.
                    let pic_rect = prim_info.rect.inflate(inflate_size + box_offset.x.abs(), inflate_size + box_offset.y.abs());
                    let pic_info = LayerPrimitiveInfo::with_clip_rect(
                        pic_rect,
                        prim_info.rect
                    );

                    // Add a normal clip to ensure nothing gets drawn
                    // outside the primitive rect.
                    if !border_radius.is_zero() {
                        extra_clips.push(ClipSource::new_rounded_rect(
                            prim_info.rect,
                            border_radius,
                            ClipMode::Clip,
                        ));
                    }

                    // Add the picture primitive to the frame.
                    self.add_primitive(
                        clip_and_scroll,
                        &pic_info,
                        extra_clips,
                        PrimitiveContainer::Picture(pic_prim),
                    );
                }
            }
        }
    }
}

fn adjust_border_radius_for_box_shadow(
    radius: BorderRadius,
    spread_amount: f32,
) -> BorderRadius {
    BorderRadius {
        top_left: adjust_corner_for_box_shadow(
            radius.top_left,
            spread_amount,
        ),
        top_right: adjust_corner_for_box_shadow(
            radius.top_right,
            spread_amount,
        ),
        bottom_right: adjust_corner_for_box_shadow(
            radius.bottom_right,
            spread_amount,
        ),
        bottom_left: adjust_corner_for_box_shadow(
            radius.bottom_left,
            spread_amount,
        ),
    }
}

fn adjust_corner_for_box_shadow(
    corner: LayoutSize,
    spread_amount: f32,
) -> LayoutSize {
    LayoutSize::new(
        adjust_radius_for_box_shadow(
            corner.width,
            spread_amount
        ),
        adjust_radius_for_box_shadow(
            corner.height,
            spread_amount
        ),
    )
}

fn adjust_radius_for_box_shadow(
    border_radius: f32,
    spread_amount: f32,
) -> f32 {
    if border_radius > 0.0 {
        (border_radius + spread_amount).max(0.0)
    } else {
        0.0
    }
}

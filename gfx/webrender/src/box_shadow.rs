/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, LayerPoint, LayerRect, LayerSize, LayerVector2D};
use api::{BorderRadius, BoxShadowClipMode, LayoutSize, LayerPrimitiveInfo};
use api::{ClipMode, ComplexClipRegion, LocalClip, ClipAndScrollInfo};
use clip::ClipSource;
use frame_builder::FrameBuilder;
use prim_store::{PrimitiveContainer, RectanglePrimitive, BrushPrimitive};
use picture::PicturePrimitive;
use util::RectHelpers;

// The blur shader samples BLUR_SAMPLE_SCALE * blur_radius surrounding texels.
pub const BLUR_SAMPLE_SCALE: f32 = 3.0;

impl FrameBuilder {
    pub fn add_box_shadow(
        &mut self,
        clip_and_scroll: ClipAndScrollInfo,
        prim_info: &LayerPrimitiveInfo,
        box_offset: &LayerVector2D,
        color: &ColorF,
        blur_radius: f32,
        spread_radius: f32,
        border_radius: BorderRadius,
        clip_mode: BoxShadowClipMode,
    ) {
        if color.a == 0.0 {
            return;
        }

        let spread_amount = match clip_mode {
            BoxShadowClipMode::Outset => {
                spread_radius
            }
            BoxShadowClipMode::Inset => {
                -spread_radius
            }
        };

        let shadow_radius = adjust_border_radius_for_box_shadow(
            border_radius,
            spread_amount,
        );
        let shadow_rect = prim_info.rect
                                   .translate(box_offset)
                                   .inflate(spread_amount, spread_amount);

        if blur_radius == 0.0 {
            let mut clips = Vec::new();

            let fast_info = match clip_mode {
                BoxShadowClipMode::Outset => {
                    // TODO(gw): Add a fast path for ClipOut + zero border radius!
                    clips.push(ClipSource::RoundedRectangle(
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
                    clips.push(ClipSource::RoundedRectangle(
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
                PrimitiveContainer::Rectangle(RectanglePrimitive {
                    color: *color,
                }),
            );
        } else {
            let blur_offset = 2.0 * blur_radius;
            let mut extra_clips = vec![];
            let mut blur_regions = vec![];

            match clip_mode {
                BoxShadowClipMode::Outset => {
                    let brush_prim = BrushPrimitive {
                        clip_mode: ClipMode::Clip,
                        radius: shadow_radius,
                    };

                    let brush_rect = LayerRect::new(LayerPoint::new(blur_offset, blur_offset),
                                                    shadow_rect.size);

                    let brush_info = LayerPrimitiveInfo::new(brush_rect);

                    let brush_prim_index = self.create_primitive(
                        clip_and_scroll,
                        &brush_info,
                        Vec::new(),
                        PrimitiveContainer::Brush(brush_prim),
                    );

                    let pic_rect = shadow_rect.inflate(blur_offset, blur_offset);
                    let blur_range = BLUR_SAMPLE_SCALE * blur_radius;

                    let size = pic_rect.size;

                    let tl = LayerSize::new(
                        blur_radius.max(border_radius.top_left.width),
                        blur_radius.max(border_radius.top_left.height)
                    ) * BLUR_SAMPLE_SCALE;
                    let tr = LayerSize::new(
                        blur_radius.max(border_radius.top_right.width),
                        blur_radius.max(border_radius.top_right.height)
                    ) * BLUR_SAMPLE_SCALE;
                    let br = LayerSize::new(
                        blur_radius.max(border_radius.bottom_right.width),
                        blur_radius.max(border_radius.bottom_right.height)
                    ) * BLUR_SAMPLE_SCALE;
                    let bl = LayerSize::new(
                        blur_radius.max(border_radius.bottom_left.width),
                        blur_radius.max(border_radius.bottom_left.height)
                    ) * BLUR_SAMPLE_SCALE;

                    let max_width = tl.width.max(tr.width.max(bl.width.max(br.width)));
                    let max_height = tl.height.max(tr.height.max(bl.height.max(br.height)));

                    // Apply a conservative test that if any of the blur regions below
                    // will overlap, we won't bother applying the region optimization
                    // and will just blur the entire thing. This should only happen
                    // in rare cases, where either the blur radius or border radius
                    // is very large, in which case there's no real point in trying
                    // to only blur a small region anyway.
                    if max_width < 0.5 * size.width && max_height < 0.5 * size.height {
                        blur_regions.push(LayerRect::from_floats(0.0, 0.0, tl.width, tl.height));
                        blur_regions.push(LayerRect::from_floats(size.width - tr.width, 0.0, size.width, tr.height));
                        blur_regions.push(LayerRect::from_floats(size.width - br.width, size.height - br.height, size.width, size.height));
                        blur_regions.push(LayerRect::from_floats(0.0, size.height - bl.height, bl.width, size.height));

                        blur_regions.push(LayerRect::from_floats(0.0, tl.height, blur_range, size.height - bl.height));
                        blur_regions.push(LayerRect::from_floats(size.width - blur_range, tr.height, size.width, size.height - br.height));
                        blur_regions.push(LayerRect::from_floats(tl.width, 0.0, size.width - tr.width, blur_range));
                        blur_regions.push(LayerRect::from_floats(bl.width, size.height - blur_range, size.width - br.width, size.height));
                    }

                    let mut pic_prim = PicturePrimitive::new_box_shadow(
                        blur_radius,
                        *color,
                        blur_regions,
                        BoxShadowClipMode::Outset,
                    );

                    pic_prim.add_primitive(
                        brush_prim_index,
                        &brush_rect,
                        clip_and_scroll
                    );

                    pic_prim.build();

                    extra_clips.push(ClipSource::RoundedRectangle(
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
                    let brush_prim = BrushPrimitive {
                        clip_mode: ClipMode::ClipOut,
                        radius: shadow_radius,
                    };

                    let mut brush_rect = shadow_rect;
                    brush_rect.origin.x = brush_rect.origin.x - prim_info.rect.origin.x + blur_offset;
                    brush_rect.origin.y = brush_rect.origin.y - prim_info.rect.origin.y + blur_offset;

                    let brush_info = LayerPrimitiveInfo::new(brush_rect);

                    let brush_prim_index = self.create_primitive(
                        clip_and_scroll,
                        &brush_info,
                        Vec::new(),
                        PrimitiveContainer::Brush(brush_prim),
                    );

                    let pic_rect = prim_info.rect.inflate(blur_offset, blur_offset);

                    // TODO(gw): Apply minimal blur regions for inset box shadows.

                    let mut pic_prim = PicturePrimitive::new_box_shadow(
                        blur_radius,
                        *color,
                        blur_regions,
                        BoxShadowClipMode::Inset,
                    );

                    pic_prim.add_primitive(
                        brush_prim_index,
                        &prim_info.rect,
                        clip_and_scroll
                    );

                    pic_prim.build();

                    extra_clips.push(ClipSource::RoundedRectangle(
                        prim_info.rect,
                        border_radius,
                        ClipMode::Clip,
                    ));

                    let pic_info = LayerPrimitiveInfo::with_clip_rect(pic_rect, prim_info.rect);

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

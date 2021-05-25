/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate euclid;
extern crate gleam;
extern crate glutin;
extern crate webrender;
extern crate winit;

#[path = "common/boilerplate.rs"]
mod boilerplate;

use crate::boilerplate::{Example, HandyDandyRectBuilder};
use euclid::vec2;
use webrender::ShaderPrecacheFlags;
use webrender::api::*;
use webrender::render_api::*;
use webrender::api::units::*;

fn main() {
    let mut app = App {
    };
    boilerplate::main_wrapper(&mut app, None);
}

struct App {
}

impl Example for App {
    // Make this the only example to test all shaders for compile errors.
    const PRECACHE_SHADER_FLAGS: ShaderPrecacheFlags = ShaderPrecacheFlags::FULL_COMPILE;

    fn render(
        &mut self,
        api: &mut RenderApi,
        builder: &mut DisplayListBuilder,
        txn: &mut Transaction,
        _: DeviceIntSize,
        pipeline_id: PipelineId,
        _document_id: DocumentId,
    ) {
        let content_bounds = LayoutRect::new(LayoutPoint::zero(), LayoutSize::new(800.0, 600.0));
        let root_space_and_clip = SpaceAndClipInfo::root_scroll(pipeline_id);
        let spatial_id = root_space_and_clip.spatial_id;

        builder.push_simple_stacking_context(
            content_bounds.origin,
            spatial_id,
            PrimitiveFlags::IS_BACKFACE_VISIBLE,
        );

        let image_mask_key = api.generate_image_key();
        txn.add_image(
            image_mask_key,
            ImageDescriptor::new(2, 2, ImageFormat::R8, ImageDescriptorFlags::IS_OPAQUE),
            ImageData::new(vec![0, 80, 180, 255]),
            None,
        );
        let mask = ImageMask {
            image: image_mask_key,
            rect: (75, 75).by(100, 100),
            repeat: false,
        };
        let complex = ComplexClipRegion::new(
            (50, 50).to(150, 150),
            BorderRadius::uniform(20.0),
            ClipMode::Clip
        );
        let mask_clip_id = builder.define_clip_image_mask(
            &root_space_and_clip,
            mask,
            &vec![],
            FillRule::Nonzero,
        );
        let clip_id = builder.define_clip_rounded_rect(
            &SpaceAndClipInfo {
                spatial_id: root_space_and_clip.spatial_id,
                clip_id: mask_clip_id,
            },
            complex,
        );

        builder.push_rect(
            &CommonItemProperties::new(
                (100, 100).to(200, 200),
                SpaceAndClipInfo { spatial_id, clip_id },
            ),
            (100, 100).to(200, 200),
            ColorF::new(0.0, 1.0, 0.0, 1.0),
        );

        builder.push_rect(
            &CommonItemProperties::new(
                (250, 100).to(350, 200),
                SpaceAndClipInfo { spatial_id, clip_id },
            ),
            (250, 100).to(350, 200),
            ColorF::new(0.0, 1.0, 0.0, 1.0),
        );
        let border_side = BorderSide {
            color: ColorF::new(0.0, 0.0, 1.0, 1.0),
            style: BorderStyle::Groove,
        };
        let border_widths = LayoutSideOffsets::new_all_same(10.0);
        let border_details = BorderDetails::Normal(NormalBorder {
            top: border_side,
            right: border_side,
            bottom: border_side,
            left: border_side,
            radius: BorderRadius::uniform(20.0),
            do_aa: true,
        });

        let bounds = (100, 100).to(200, 200);
        builder.push_border(
            &CommonItemProperties::new(
                bounds,
                SpaceAndClipInfo { spatial_id, clip_id },
            ),
            bounds,
            border_widths,
            border_details,
        );

        if false {
            // draw box shadow?
            let simple_box_bounds = (20, 200).by(50, 50);
            let offset = vec2(10.0, 10.0);
            let color = ColorF::new(1.0, 1.0, 1.0, 1.0);
            let blur_radius = 0.0;
            let spread_radius = 0.0;
            let simple_border_radius = 8.0;
            let box_shadow_type = BoxShadowClipMode::Inset;

            builder.push_box_shadow(
                &CommonItemProperties::new(content_bounds, root_space_and_clip),
                simple_box_bounds,
                offset,
                color,
                blur_radius,
                spread_radius,
                BorderRadius::uniform(simple_border_radius),
                box_shadow_type,
            );
        }

        builder.pop_stacking_context();
    }
}

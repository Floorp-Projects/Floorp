/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, GlyphInstance, RasterSpace, Shadow};
use api::units::{DevicePixelScale, LayoutToWorldTransform, LayoutVector2D};
use crate::display_list_flattener::{CreateShadow, IsVisible};
use crate::frame_builder::FrameBuildingState;
use crate::glyph_rasterizer::{FontInstance, FontTransform, GlyphKey, FONT_SIZE_LIMIT};
use crate::gpu_cache::GpuCache;
use crate::intern;
use crate::internal_types::LayoutPrimitiveInfo;
use crate::picture::SurfaceInfo;
use crate::prim_store::{PrimitiveOpacity, PrimitiveSceneData,  PrimitiveScratchBuffer};
use crate::prim_store::{PrimitiveStore, PrimKeyCommonData, PrimTemplateCommonData};
use crate::render_task::{RenderTaskGraph};
use crate::renderer::{MAX_VERTEX_TEXTURE_WIDTH};
use crate::resource_cache::{ResourceCache};
use crate::util::{MatrixHelpers};
use crate::prim_store::{InternablePrimitive, PrimitiveInstanceKind};
use std::ops;
use std::sync::Arc;
use crate::storage;
use crate::util::PrimaryArc;

/// A run of glyphs, with associated font information.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct TextRunKey {
    pub common: PrimKeyCommonData,
    pub font: FontInstance,
    pub glyphs: PrimaryArc<Vec<GlyphInstance>>,
    pub shadow: bool,
}

impl TextRunKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        text_run: TextRun,
    ) -> Self {
        TextRunKey {
            common: PrimKeyCommonData::with_info(
                info,
            ),
            font: text_run.font,
            glyphs: PrimaryArc(text_run.glyphs),
            shadow: text_run.shadow,
        }
    }
}

impl intern::InternDebug for TextRunKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct TextRunTemplate {
    pub common: PrimTemplateCommonData,
    pub font: FontInstance,
    #[ignore_malloc_size_of = "Measured via PrimaryArc"]
    pub glyphs: Arc<Vec<GlyphInstance>>,
}

impl ops::Deref for TextRunTemplate {
    type Target = PrimTemplateCommonData;
    fn deref(&self) -> &Self::Target {
        &self.common
    }
}

impl ops::DerefMut for TextRunTemplate {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.common
    }
}

impl From<TextRunKey> for TextRunTemplate {
    fn from(item: TextRunKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(item.common);
        TextRunTemplate {
            common,
            font: item.font,
            glyphs: item.glyphs.0,
        }
    }
}

impl TextRunTemplate {
    /// Update the GPU cache for a given primitive template. This may be called multiple
    /// times per frame, by each primitive reference that refers to this interned
    /// template. The initial request call to the GPU cache ensures that work is only
    /// done if the cache entry is invalid (due to first use or eviction).
    pub fn update(
        &mut self,
        frame_state: &mut FrameBuildingState,
    ) {
        self.write_prim_gpu_blocks(frame_state);
        self.opacity = PrimitiveOpacity::translucent();
    }

    fn write_prim_gpu_blocks(
        &mut self,
        frame_state: &mut FrameBuildingState,
    ) {
        if let Some(mut request) = frame_state.gpu_cache.request(&mut self.common.gpu_cache_handle) {
            request.push(ColorF::from(self.font.color).premultiplied());
            // this is the only case where we need to provide plain color to GPU
            let bg_color = ColorF::from(self.font.bg_color);
            request.push([bg_color.r, bg_color.g, bg_color.b, 1.0]);

            let mut gpu_block = [0.0; 4];
            for (i, src) in self.glyphs.iter().enumerate() {
                // Two glyphs are packed per GPU block.

                if (i & 1) == 0 {
                    gpu_block[0] = src.point.x;
                    gpu_block[1] = src.point.y;
                } else {
                    gpu_block[2] = src.point.x;
                    gpu_block[3] = src.point.y;
                    request.push(gpu_block);
                }
            }

            // Ensure the last block is added in the case
            // of an odd number of glyphs.
            if (self.glyphs.len() & 1) != 0 {
                request.push(gpu_block);
            }

            assert!(request.current_used_block_num() <= MAX_VERTEX_TEXTURE_WIDTH);
        }
    }
}

pub type TextRunDataHandle = intern::Handle<TextRun>;

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextRun {
    pub font: FontInstance,
    #[ignore_malloc_size_of = "Measured via PrimaryArc"]
    pub glyphs: Arc<Vec<GlyphInstance>>,
    pub shadow: bool,
}

impl intern::Internable for TextRun {
    type Key = TextRunKey;
    type StoreData = TextRunTemplate;
    type InternData = PrimitiveSceneData;
}

impl InternablePrimitive for TextRun {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> TextRunKey {
        TextRunKey::new(
            info,
            self,
        )
    }

    fn make_instance_kind(
        key: TextRunKey,
        data_handle: TextRunDataHandle,
        prim_store: &mut PrimitiveStore,
        reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        let run_index = prim_store.text_runs.push(TextRunPrimitive {
            used_font: key.font.clone(),
            glyph_keys_range: storage::Range::empty(),
            reference_frame_relative_offset,
            shadow: key.shadow,
            raster_space: RasterSpace::Screen,
        });

        PrimitiveInstanceKind::TextRun{ data_handle, run_index }
    }
}

impl CreateShadow for TextRun {
    fn create_shadow(&self, shadow: &Shadow) -> Self {
        let mut font = FontInstance {
            color: shadow.color.into(),
            ..self.font.clone()
        };
        if shadow.blur_radius > 0.0 {
            font.disable_subpixel_aa();
        }

        TextRun {
            font,
            glyphs: self.glyphs.clone(),
            shadow: true,
        }
    }
}

impl IsVisible for TextRun {
    fn is_visible(&self) -> bool {
        self.font.color.a > 0
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct TextRunPrimitive {
    pub used_font: FontInstance,
    pub glyph_keys_range: storage::Range<GlyphKey>,
    pub reference_frame_relative_offset: LayoutVector2D,
    pub shadow: bool,
    pub raster_space: RasterSpace,
}

impl TextRunPrimitive {
    pub fn update_font_instance(
        &mut self,
        specified_font: &FontInstance,
        device_pixel_scale: DevicePixelScale,
        transform: &LayoutToWorldTransform,
        allow_subpixel_aa: bool,
        raster_space: RasterSpace,
    ) -> bool {
        // If local raster space is specified, include that in the scale
        // of the glyphs that get rasterized.
        // TODO(gw): Once we support proper local space raster modes, this
        //           will implicitly be part of the device pixel ratio for
        //           the (cached) local space surface, and so this code
        //           will no longer be required.
        let raster_scale = raster_space.local_scale().unwrap_or(1.0).max(0.001);

        // Get the current font size in device pixels
        let device_font_size = specified_font.size.scale_by(device_pixel_scale.0 * raster_scale);

        // Determine if rasterizing glyphs in local or screen space.
        // Only support transforms that can be coerced to simple 2D transforms.
        let transform_glyphs = if transform.has_perspective_component() ||
           !transform.has_2d_inverse() ||
           // Font sizes larger than the limit need to be scaled, thus can't use subpixels.
           transform.exceeds_2d_scale(FONT_SIZE_LIMIT / device_font_size.to_f64_px()) ||
           // Otherwise, ensure the font is rasterized in screen-space.
           raster_space != RasterSpace::Screen {
            false
        } else {
            true
        };

        // Get the font transform matrix (skew / scale) from the complete transform.
        let font_transform = if transform_glyphs {
            FontTransform::from(transform)
        } else {
            FontTransform::identity()
        };

        // Record the raster space the text needs to be snapped in.
        self.raster_space = raster_space;

        // If the transform or device size is different, then the caller of
        // this method needs to know to rebuild the glyphs.
        let cache_dirty =
            self.used_font.transform != font_transform ||
            self.used_font.size != device_font_size;

        // Construct used font instance from the specified font instance
        self.used_font = FontInstance {
            transform: font_transform,
            size: device_font_size,
            ..specified_font.clone()
        };

        // If subpixel AA is disabled due to the backing surface the glyphs
        // are being drawn onto, disable it (unless we are using the
        // specifial subpixel mode that estimates background color).
        if (!allow_subpixel_aa && self.used_font.bg_color.a == 0) ||
            // If using local space glyphs, we don't want subpixel AA.
            !transform_glyphs {
            self.used_font.disable_subpixel_aa();
        }

        cache_dirty
    }

    pub fn request_resources(
        &mut self,
        prim_offset: LayoutVector2D,
        specified_font: &FontInstance,
        glyphs: &[GlyphInstance],
        transform: &LayoutToWorldTransform,
        surface: &SurfaceInfo,
        raster_space: RasterSpace,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskGraph,
        scratch: &mut PrimitiveScratchBuffer,
    ) {
        let device_pixel_scale = surface.device_pixel_scale;

        let cache_dirty = self.update_font_instance(
            specified_font,
            device_pixel_scale,
            transform,
            surface.allow_subpixel_aa,
            raster_space,
        );

        if self.glyph_keys_range.is_empty() || cache_dirty {
            let subpx_dir = self.used_font.get_subpx_dir();

            self.glyph_keys_range = scratch.glyph_keys.extend(
                glyphs.iter().map(|src| {
                    let src_point = src.point + prim_offset;
                    let world_offset = self.used_font.transform.transform(&src_point);
                    let device_offset = device_pixel_scale.transform_point(&world_offset);
                    GlyphKey::new(src.index, device_offset, subpx_dir)
                }));
        }

        resource_cache.request_glyphs(
            self.used_font.clone(),
            &scratch.glyph_keys[self.glyph_keys_range],
            gpu_cache,
            render_tasks,
        );
    }
}

/// These are linux only because FontInstancePlatformOptions varies in size by platform.
#[test]
#[cfg(target_os = "linux")]
fn test_struct_sizes() {
    use std::mem;
    // The sizes of these structures are critical for performance on a number of
    // talos stress tests. If you get a failure here on CI, there's two possibilities:
    // (a) You made a structure smaller than it currently is. Great work! Update the
    //     test expectations and move on.
    // (b) You made a structure larger. This is not necessarily a problem, but should only
    //     be done with care, and after checking if talos performance regresses badly.
    assert_eq!(mem::size_of::<TextRun>(), 56, "TextRun size changed");
    assert_eq!(mem::size_of::<TextRunTemplate>(), 72, "TextRunTemplate size changed");
    assert_eq!(mem::size_of::<TextRunKey>(), 64, "TextRunKey size changed");
    assert_eq!(mem::size_of::<TextRunPrimitive>(), 72, "TextRunPrimitive size changed");
}

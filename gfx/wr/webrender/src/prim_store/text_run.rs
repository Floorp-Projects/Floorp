/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AuHelpers, ColorF, DevicePixelScale, GlyphInstance, LayoutPrimitiveInfo};
use api::{LayoutRect, LayoutToWorldTransform, LayoutVector2DAu, RasterSpace};
use api::Shadow;
use display_list_flattener::{AsInstanceKind, CreateShadow, IsVisible};
use frame_builder::{FrameBuildingState, PictureContext};
use glyph_rasterizer::{FontInstance, FontTransform, GlyphKey, FONT_SIZE_LIMIT};
use gpu_cache::GpuCache;
use intern;
use prim_store::{PrimitiveOpacity, PrimitiveSceneData,  PrimitiveScratchBuffer};
use prim_store::{PrimitiveStore, PrimKeyCommonData, PrimTemplateCommonData};
use render_task::{RenderTaskTree};
use renderer::{MAX_VERTEX_TEXTURE_WIDTH};
use resource_cache::{ResourceCache};
use tiling::SpecialRenderPasses;
use util::{MatrixHelpers};
use prim_store::PrimitiveInstanceKind;
use std::ops;
use storage;

/// A run of glyphs, with associated font information.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct TextRunKey {
    pub common: PrimKeyCommonData,
    pub font: FontInstance,
    pub offset: LayoutVector2DAu,
    pub glyphs: Vec<GlyphInstance>,
    pub shadow: bool,
}

impl TextRunKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        prim_relative_clip_rect: LayoutRect,
        text_run: TextRun,
    ) -> Self {
        TextRunKey {
            common: PrimKeyCommonData::with_info(
                info,
                prim_relative_clip_rect,
            ),
            font: text_run.font,
            offset: text_run.offset.into(),
            glyphs: text_run.glyphs,
            shadow: text_run.shadow,
        }
    }
}

impl AsInstanceKind<TextRunDataHandle> for TextRunKey {
    /// Construct a primitive instance that matches the type
    /// of primitive key.
    fn as_instance_kind(
        &self,
        data_handle: TextRunDataHandle,
        prim_store: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind {
        let run_index = prim_store.text_runs.push(TextRunPrimitive {
            used_font: self.font.clone(),
            glyph_keys_range: storage::Range::empty(),
            shadow: self.shadow,
        });

        PrimitiveInstanceKind::TextRun{ data_handle, run_index }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextRunTemplate {
    pub common: PrimTemplateCommonData,
    pub font: FontInstance,
    pub offset: LayoutVector2DAu,
    pub glyphs: Vec<GlyphInstance>,
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
            offset: item.offset,
            glyphs: item.glyphs,
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
            request.push([
                self.offset.x.to_f32_px(),
                self.offset.y.to_f32_px(),
                0.0,
                0.0,
            ]);

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

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub struct TextRunDataMarker;

pub type TextRunDataStore = intern::DataStore<TextRunKey, TextRunTemplate, TextRunDataMarker>;
pub type TextRunDataHandle = intern::Handle<TextRunDataMarker>;
pub type TextRunDataUpdateList = intern::UpdateList<TextRunKey>;
pub type TextRunDataInterner = intern::Interner<TextRunKey, PrimitiveSceneData, TextRunDataMarker>;

pub struct TextRun {
    pub font: FontInstance,
    pub offset: LayoutVector2DAu,
    pub glyphs: Vec<GlyphInstance>,
    pub shadow: bool,
}

impl intern::Internable for TextRun {
    type Marker = TextRunDataMarker;
    type Source = TextRunKey;
    type StoreData = TextRunTemplate;
    type InternData = PrimitiveSceneData;

    /// Build a new key from self with `info`.
    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
        prim_relative_clip_rect: LayoutRect,
    ) -> TextRunKey {
        TextRunKey::new(
            info,
            prim_relative_clip_rect,
            self,
        )
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
            offset: self.offset + shadow.offset.to_au(),
            shadow: true
        }
    }
}

impl IsVisible for TextRun {
    fn is_visible(&self) -> bool {
        self.font.color.a > 0
    }
}

#[derive(Debug)]
pub struct TextRunPrimitive {
    pub used_font: FontInstance,
    pub glyph_keys_range: storage::Range<GlyphKey>,
    pub shadow: bool,
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
        // Get the current font size in device pixels
        let device_font_size = specified_font.size.scale_by(device_pixel_scale.0);

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
            // Quantize the transform to minimize thrashing of the glyph cache.
            FontTransform::from(transform).quantize()
        } else {
            FontTransform::identity()
        };

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

    pub fn prepare_for_render(
        &mut self,
        specified_font: &FontInstance,
        glyphs: &[GlyphInstance],
        device_pixel_scale: DevicePixelScale,
        transform: &LayoutToWorldTransform,
        pic_context: &PictureContext,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        special_render_passes: &mut SpecialRenderPasses,
        scratch: &mut PrimitiveScratchBuffer,
    ) {
        let cache_dirty = self.update_font_instance(
            specified_font,
            device_pixel_scale,
            transform,
            pic_context.allow_subpixel_aa,
            pic_context.raster_space,
        );

        if self.glyph_keys_range.is_empty() || cache_dirty {
            let subpx_dir = self.used_font.get_subpx_dir();

            self.glyph_keys_range = scratch.glyph_keys.extend(
                glyphs.iter().map(|src| {
                    let world_offset = self.used_font.transform.transform(&src.point);
                    let device_offset = device_pixel_scale.transform_point(&world_offset);
                    GlyphKey::new(src.index, device_offset, subpx_dir)
                }));
        }

        resource_cache.request_glyphs(
            self.used_font.clone(),
            &scratch.glyph_keys[self.glyph_keys_range],
            gpu_cache,
            render_tasks,
            special_render_passes,
        );
    }
}

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
    assert_eq!(mem::size_of::<TextRun>(), 112, "TextRun size changed");
    assert_eq!(mem::size_of::<TextRunTemplate>(), 160, "TextRunTemplate size changed");
    assert_eq!(mem::size_of::<TextRunKey>(), 136, "TextRunKey size changed");
    assert_eq!(mem::size_of::<TextRunPrimitive>(), 88, "TextRunPrimitive size changed");
}

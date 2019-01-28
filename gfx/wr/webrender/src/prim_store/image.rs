/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{
    AlphaType, ColorDepth, ColorF, ColorU, DeviceIntRect, DeviceIntSideOffsets,
    DeviceIntSize, ImageRendering, LayoutRect, LayoutSize, LayoutPrimitiveInfo,
    PremultipliedColorF, Shadow, TileOffset, YuvColorSpace, YuvFormat, LayoutVector2D,
};
use api::ImageKey as ApiImageKey;
use display_list_flattener::{AsInstanceKind, CreateShadow, IsVisible};
use frame_builder::FrameBuildingState;
use gpu_cache::{GpuCacheHandle, GpuDataRequest};
use intern::{Internable, InternDebug};
use intern_types;
use prim_store::{
    EdgeAaSegmentMask, OpacityBindingIndex, PrimitiveInstanceKind,
    PrimitiveOpacity, PrimitiveSceneData, PrimKey, PrimKeyCommonData,
    PrimTemplate, PrimTemplateCommonData, PrimitiveStore, SegmentInstanceIndex,
    SizeKey
};
use render_task::{
    BlitSource, RenderTask, RenderTaskCacheEntryHandle, RenderTaskCacheKey,
    RenderTaskCacheKeyKind
};
use resource_cache::ImageRequest;
use util::pack_as_float;

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct VisibleImageTile {
    pub tile_offset: TileOffset,
    pub handle: GpuCacheHandle,
    pub edge_flags: EdgeAaSegmentMask,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

// Key that identifies a unique (partial) image that is being
// stored in the render task cache.
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ImageCacheKey {
    pub request: ImageRequest,
    pub texel_rect: Option<DeviceIntRect>,
}

/// Instance specific fields for an image primitive. These are
/// currently stored in a separate array to avoid bloating the
/// size of PrimitiveInstance. In the future, we should be able
/// to remove this and store the information inline, by:
/// (a) Removing opacity collapse / binding support completely.
///     Once we have general picture caching, we don't need this.
/// (b) Change visible_tiles to use Storage in the primitive
///     scratch buffer. This will reduce the size of the
///     visible_tiles field here, and save memory allocation
///     when image tiling is used. I've left it as a Vec for
///     now to reduce the number of changes, and because image
///     tiling is very rare on real pages.
#[derive(Debug)]
pub struct ImageInstance {
    pub opacity_binding_index: OpacityBindingIndex,
    pub segment_instance_index: SegmentInstanceIndex,
    pub visible_tiles: Vec<VisibleImageTile>,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, MallocSizeOf, Hash)]
pub struct Image {
    pub key: ApiImageKey,
    pub stretch_size: SizeKey,
    pub tile_spacing: SizeKey,
    pub color: ColorU,
    pub sub_rect: Option<DeviceIntRect>,
    pub image_rendering: ImageRendering,
    pub alpha_type: AlphaType,
}

pub type ImageKey = PrimKey<Image>;

impl ImageKey {
    pub fn new(
        is_backface_visible: bool,
        prim_size: LayoutSize,
        image: Image,
    ) -> Self {

        ImageKey {
            common: PrimKeyCommonData {
                is_backface_visible,
                prim_size: prim_size.into(),
            },
            kind: image,
        }
    }
}

impl InternDebug for ImageKey {}

impl AsInstanceKind<ImageDataHandle> for ImageKey {
    /// Construct a primitive instance that matches the type
    /// of primitive key.
    fn as_instance_kind(
        &self,
        data_handle: ImageDataHandle,
        prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        // TODO(gw): Refactor this to not need a separate image
        //           instance (see ImageInstance struct).
        let image_instance_index = prim_store.images.push(ImageInstance {
            opacity_binding_index: OpacityBindingIndex::INVALID,
            segment_instance_index: SegmentInstanceIndex::INVALID,
            visible_tiles: Vec::new(),
        });

        PrimitiveInstanceKind::Image {
            data_handle,
            image_instance_index,
        }
    }
}

// Where to find the texture data for an image primitive.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub enum ImageSource {
    // A normal image - just reference the texture cache.
    Default,
    // An image that is pre-rendered into the texture cache
    // via a render task.
    Cache {
        size: DeviceIntSize,
        handle: Option<RenderTaskCacheEntryHandle>,
    },
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct ImageData {
    pub key: ApiImageKey,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub color: ColorF,
    pub source: ImageSource,
    pub image_rendering: ImageRendering,
    pub sub_rect: Option<DeviceIntRect>,
    pub alpha_type: AlphaType,
}

impl From<Image> for ImageData {
    fn from(image: Image) -> Self {
        ImageData {
            key: image.key,
            color: image.color.into(),
            stretch_size: image.stretch_size.into(),
            tile_spacing: image.tile_spacing.into(),
            source: ImageSource::Default,
            sub_rect: image.sub_rect,
            image_rendering: image.image_rendering,
            alpha_type: image.alpha_type,
        }
    }
}

impl ImageData {
    /// Update the GPU cache for a given primitive template. This may be called multiple
    /// times per frame, by each primitive reference that refers to this interned
    /// template. The initial request call to the GPU cache ensures that work is only
    /// done if the cache entry is invalid (due to first use or eviction).
    pub fn update(
        &mut self,
        common: &mut PrimTemplateCommonData,
        frame_state: &mut FrameBuildingState,
    ) {
        if let Some(mut request) = frame_state.gpu_cache.request(&mut common.gpu_cache_handle) {
            self.write_prim_gpu_blocks(&mut request);
        }

        common.opacity = {
            let image_properties = frame_state
                .resource_cache
                .get_image_properties(self.key);

            match image_properties {
                Some(image_properties) => {
                    let is_tiled = image_properties.tiling.is_some();

                    if self.tile_spacing != LayoutSize::zero() && !is_tiled {
                        self.source = ImageSource::Cache {
                            // Size in device-pixels we need to allocate in render task cache.
                            size: image_properties.descriptor.size.to_i32(),
                            handle: None,
                        };
                    }

                    // Work out whether this image is a normal / simple type, or if
                    // we need to pre-render it to the render task cache.
                    if let Some(rect) = self.sub_rect {
                        // We don't properly support this right now.
                        debug_assert!(!is_tiled);
                        self.source = ImageSource::Cache {
                            // Size in device-pixels we need to allocate in render task cache.
                            size: rect.size,
                            handle: None,
                        };
                    }

                    let mut request_source_image = false;
                    let mut is_opaque = image_properties.descriptor.is_opaque;
                    let request = ImageRequest {
                        key: self.key,
                        rendering: self.image_rendering,
                        tile: None,
                    };

                    // Every frame, for cached items, we need to request the render
                    // task cache item. The closure will be invoked on the first
                    // time through, and any time the render task output has been
                    // evicted from the texture cache.
                    match self.source {
                        ImageSource::Cache { ref mut size, ref mut handle } => {
                            let padding = DeviceIntSideOffsets::new(
                                0,
                                (self.tile_spacing.width * size.width as f32 / self.stretch_size.width) as i32,
                                (self.tile_spacing.height * size.height as f32 / self.stretch_size.height) as i32,
                                0,
                            );

                            let inner_size = *size;
                            size.width += padding.horizontal();
                            size.height += padding.vertical();

                            is_opaque &= padding == DeviceIntSideOffsets::zero();

                            let image_cache_key = ImageCacheKey {
                                request,
                                texel_rect: self.sub_rect,
                            };

                            // Request a pre-rendered image task.
                            *handle = Some(frame_state.resource_cache.request_render_task(
                                RenderTaskCacheKey {
                                    size: *size,
                                    kind: RenderTaskCacheKeyKind::Image(image_cache_key),
                                },
                                frame_state.gpu_cache,
                                frame_state.render_tasks,
                                None,
                                image_properties.descriptor.is_opaque,
                                |render_tasks| {
                                    // We need to render the image cache this frame,
                                    // so will need access to the source texture.
                                    request_source_image = true;

                                    // Create a task to blit from the texture cache to
                                    // a normal transient render task surface. This will
                                    // copy only the sub-rect, if specified.
                                    let cache_to_target_task = RenderTask::new_blit_with_padding(
                                        inner_size,
                                        &padding,
                                        BlitSource::Image { key: image_cache_key },
                                    );
                                    let cache_to_target_task_id = render_tasks.add(cache_to_target_task);

                                    // Create a task to blit the rect from the child render
                                    // task above back into the right spot in the persistent
                                        // render target cache.
                                    let target_to_cache_task = RenderTask::new_blit(
                                        *size,
                                        BlitSource::RenderTask {
                                            task_id: cache_to_target_task_id,
                                        },
                                    );

                                    render_tasks.add(target_to_cache_task)
                                }
                            ));
                        }
                        ImageSource::Default => {
                            // Normal images just reference the source texture each frame.
                            request_source_image = true;
                        }
                    }

                    if request_source_image && !is_tiled {
                        frame_state.resource_cache.request_image(
                            request,
                            frame_state.gpu_cache,
                        );
                    }

                    if is_opaque {
                        PrimitiveOpacity::from_alpha(self.color.a)
                    } else {
                        PrimitiveOpacity::translucent()
                    }
                }
                None => {
                    PrimitiveOpacity::opaque()
                }
            }
        };
    }

    pub fn write_prim_gpu_blocks(&self, request: &mut GpuDataRequest) {
        // Images are drawn as a white color, modulated by the total
        // opacity coming from any collapsed property bindings.
        request.push(self.color.premultiplied());
        request.push(PremultipliedColorF::WHITE);
        request.push([
            self.stretch_size.width + self.tile_spacing.width,
            self.stretch_size.height + self.tile_spacing.height,
            0.0,
            0.0,
        ]);
    }
}

pub type ImageTemplate = PrimTemplate<ImageData>;

impl From<ImageKey> for ImageTemplate {
    fn from(image: ImageKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(image.common);

        ImageTemplate {
            common,
            kind: image.kind.into(),
        }
    }
}

pub use intern_types::image::Handle as ImageDataHandle;

impl Internable for Image {
    type Marker = intern_types::image::Marker;
    type Source = ImageKey;
    type StoreData = ImageTemplate;
    type InternData = PrimitiveSceneData;

    /// Build a new key from self with `info`.
    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> ImageKey {
        ImageKey::new(
            info.is_backface_visible,
            info.rect.size,
            self
        )
    }
}

impl CreateShadow for Image {
    fn create_shadow(&self, shadow: &Shadow) -> Self {
        Image {
            tile_spacing: self.tile_spacing,
            stretch_size: self.stretch_size,
            key: self.key,
            sub_rect: self.sub_rect,
            image_rendering: self.image_rendering,
            alpha_type: self.alpha_type,
            color: shadow.color.into(),
        }
    }
}

impl IsVisible for Image {
    fn is_visible(&self) -> bool {
        true
    }
}

////////////////////////////////////////////////////////////////////////////////

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct YuvImage {
    pub color_depth: ColorDepth,
    pub yuv_key: [ApiImageKey; 3],
    pub format: YuvFormat,
    pub color_space: YuvColorSpace,
    pub image_rendering: ImageRendering,
}

pub type YuvImageKey = PrimKey<YuvImage>;

impl YuvImageKey {
    pub fn new(
        is_backface_visible: bool,
        prim_size: LayoutSize,
        yuv_image: YuvImage,
    ) -> Self {

        YuvImageKey {
            common: PrimKeyCommonData {
                is_backface_visible,
                prim_size: prim_size.into(),
            },
            kind: yuv_image,
        }
    }
}

impl InternDebug for YuvImageKey {}

impl AsInstanceKind<YuvImageDataHandle> for YuvImageKey {
    /// Construct a primitive instance that matches the type
    /// of primitive key.
    fn as_instance_kind(
        &self,
        data_handle: YuvImageDataHandle,
        _prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::YuvImage {
            data_handle,
            segment_instance_index: SegmentInstanceIndex::INVALID
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct YuvImageData {
    pub color_depth: ColorDepth,
    pub yuv_key: [ApiImageKey; 3],
    pub format: YuvFormat,
    pub color_space: YuvColorSpace,
    pub image_rendering: ImageRendering,
}

impl From<YuvImage> for YuvImageData {
    fn from(image: YuvImage) -> Self {
        YuvImageData {
            color_depth: image.color_depth,
            yuv_key: image.yuv_key,
            format: image.format,
            color_space: image.color_space,
            image_rendering: image.image_rendering,
        }
    }
}

impl YuvImageData {
    /// Update the GPU cache for a given primitive template. This may be called multiple
    /// times per frame, by each primitive reference that refers to this interned
    /// template. The initial request call to the GPU cache ensures that work is only
    /// done if the cache entry is invalid (due to first use or eviction).
    pub fn update(
        &mut self,
        common: &mut PrimTemplateCommonData,
        frame_state: &mut FrameBuildingState,
    ) {
        if let Some(mut request) = frame_state.gpu_cache.request(&mut common.gpu_cache_handle) {
            self.write_prim_gpu_blocks(&mut request);
        };

        let channel_num = self.format.get_plane_num();
        debug_assert!(channel_num <= 3);
        for channel in 0 .. channel_num {
            frame_state.resource_cache.request_image(
                ImageRequest {
                    key: self.yuv_key[channel],
                    rendering: self.image_rendering,
                    tile: None,
                },
                frame_state.gpu_cache,
            );
        }

        common.opacity = PrimitiveOpacity::translucent();
    }

    pub fn write_prim_gpu_blocks(&self, request: &mut GpuDataRequest) {
        request.push([
            self.color_depth.rescaling_factor(),
            pack_as_float(self.color_space as u32),
            pack_as_float(self.format as u32),
            0.0
        ]);
    }
}

pub type YuvImageTemplate = PrimTemplate<YuvImageData>;

impl From<YuvImageKey> for YuvImageTemplate {
    fn from(image: YuvImageKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(image.common);

        YuvImageTemplate {
            common,
            kind: image.kind.into(),
        }
    }
}

pub use intern_types::yuv_image::Handle as YuvImageDataHandle;

impl Internable for YuvImage {
    type Marker = intern_types::yuv_image::Marker;
    type Source = YuvImageKey;
    type StoreData = YuvImageTemplate;
    type InternData = PrimitiveSceneData;

    /// Build a new key from self with `info`.
    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> YuvImageKey {
        YuvImageKey::new(
            info.is_backface_visible,
            info.rect.size,
            self
        )
    }
}

impl IsVisible for YuvImage {
    fn is_visible(&self) -> bool {
        true
    }
}

#[test]
#[cfg(target_pointer_width = "64")]
fn test_struct_sizes() {
    use std::mem;
    // The sizes of these structures are critical for performance on a number of
    // talos stress tests. If you get a failure here on CI, there's two possibilities:
    // (a) You made a structure smaller than it currently is. Great work! Update the
    //     test expectations and move on.
    // (b) You made a structure larger. This is not necessarily a problem, but should only
    //     be done with care, and after checking if talos performance regresses badly.
    assert_eq!(mem::size_of::<Image>(), 56, "Image size changed");
    assert_eq!(mem::size_of::<ImageTemplate>(), 108, "ImageTemplate size changed");
    assert_eq!(mem::size_of::<ImageKey>(), 68, "ImageKey size changed");
    assert_eq!(mem::size_of::<YuvImage>(), 36, "YuvImage size changed");
    assert_eq!(mem::size_of::<YuvImageTemplate>(), 56, "YuvImageTemplate size changed");
    assert_eq!(mem::size_of::<YuvImageKey>(), 48, "YuvImageKey size changed");
}

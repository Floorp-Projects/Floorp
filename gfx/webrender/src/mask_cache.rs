/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceIntRect, ImageMask, LayerPoint, LayerRect};
use api::{LayerSize, LayerToWorldTransform};
use border::BorderCornerClipSource;
use clip::{ClipMode, ClipSource};
use gpu_cache::{GpuCache, GpuCacheHandle, ToGpuBlocks};
use prim_store::{CLIP_DATA_GPU_BLOCKS, ClipData, ImageMaskData};
use util::{extract_inner_rect_safe, TransformedRect};

const MAX_CLIP: f32 = 1000000.0;

#[derive(Debug, Copy, Clone)]
pub struct ClipAddressRange {
    pub location: GpuCacheHandle,
    item_count: usize,
}

impl ClipAddressRange {
    fn new(count: usize) -> Self {
        ClipAddressRange {
            location: GpuCacheHandle::new(),
            item_count: count,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.item_count == 0
    }

    pub fn get_count(&self) -> usize {
        self.item_count
    }

    fn get_block_count(&self) -> Option<usize> {
        if self.item_count != 0 {
            Some(self.item_count * CLIP_DATA_GPU_BLOCKS)
        } else {
            None
        }
    }
}

/// Represents a local rect and a device space
/// rectangles that are either outside or inside bounds.
#[derive(Clone, Debug, PartialEq)]
pub struct Geometry {
    pub local_rect: LayerRect,
    pub device_rect: DeviceIntRect,
}

impl From<LayerRect> for Geometry {
    fn from(local_rect: LayerRect) -> Self {
        Geometry {
            local_rect,
            device_rect: DeviceIntRect::zero(),
        }
    }
}

/// Depending on the complexity of the clip, we may either
/// know the outer and/or inner rect, or neither or these.
/// In the case of a clip-out, we currently set the mask
/// bounds to be unknown. This is conservative, but ensures
/// correctness. In the future we can make this a lot
/// more clever with some proper region handling.
#[derive(Clone, Debug, PartialEq)]
pub struct MaskBounds {
    pub outer: Option<Geometry>,
    pub inner: Option<Geometry>,
}

impl MaskBounds {
    pub fn update(&mut self, transform: &LayerToWorldTransform, device_pixel_ratio: f32) {
        if let Some(ref mut outer) = self.outer {
            let transformed = TransformedRect::new(&outer.local_rect,
                                                   transform,
                                                   device_pixel_ratio);
            outer.device_rect = transformed.bounding_rect;
        }
        if let Some(ref mut inner) = self.inner {
            let transformed = TransformedRect::new(&inner.local_rect,
                                                   transform,
                                                   device_pixel_ratio);
            inner.device_rect = transformed.inner_rect;
        }
    }
}

#[derive(Clone, Debug)]
pub struct MaskCacheInfo {
    /// Clip items that are always applied
    pub complex_clip_range: ClipAddressRange,
    /// Clip items that are only applied if the clip space is transformed from
    /// the local space of target primitive/layer.
    pub layer_clip_range: ClipAddressRange,
    pub image: Option<(ImageMask, GpuCacheHandle)>,
    pub border_corners: Vec<(BorderCornerClipSource, GpuCacheHandle)>,
    pub bounds: MaskBounds,
}

impl MaskCacheInfo {
    /// Create a new mask cache info. It allocates the GPU store data but leaves
    /// it uninitialized for the following `update()` call to deal with.
    pub fn new(clips: &[ClipSource]) -> MaskCacheInfo {
        let mut image = None;
        let mut border_corners = Vec::new();
        let mut complex_clip_count = 0;
        let mut layer_clip_count = 0;

        // Work out how much clip data space we need to allocate
        // and if we have an image mask.
        for clip in clips {
            match *clip {
                ClipSource::RoundedRectangle(..) => {
                    complex_clip_count += 1;
                }
                ClipSource::Rectangle(..) => {
                    layer_clip_count += 1;
                }
                ClipSource::Image(image_mask) => {
                    debug_assert!(image.is_none());     // TODO(gw): Support >1 image mask!
                    image = Some((image_mask, GpuCacheHandle::new()));
                }
                ClipSource::BorderCorner(ref source) => {
                    border_corners.push((source.clone(), GpuCacheHandle::new()));
                }
            }
        }

        MaskCacheInfo {
            complex_clip_range: ClipAddressRange::new(complex_clip_count),
            layer_clip_range: ClipAddressRange::new(layer_clip_count),
            image,
            border_corners,
            bounds: MaskBounds {
                inner: None,
                outer: None,
            },
        }
    }

    pub fn update(&mut self,
                  sources: &[ClipSource],
                  transform: &LayerToWorldTransform,
                  gpu_cache: &mut GpuCache,
                  device_pixel_ratio: f32)
                  -> &MaskBounds {
        // Step[1] - compute the local bounds
        //TODO: move to initialization stage?
        if self.bounds.inner.is_none() {
            let mut local_rect = Some(LayerRect::new(LayerPoint::new(-MAX_CLIP, -MAX_CLIP),
                                                     LayerSize::new(2.0 * MAX_CLIP, 2.0 * MAX_CLIP)));
            let mut local_inner = local_rect;
            let mut has_clip_out = false;
            let has_border_clip = !self.border_corners.is_empty();

            for source in sources {
                match *source {
                    ClipSource::Image(ref mask) => {
                        if !mask.repeat {
                            local_rect = local_rect.and_then(|r| r.intersection(&mask.rect));
                        }
                        local_inner = None;
                    }
                    ClipSource::Rectangle(rect) => {
                        local_rect = local_rect.and_then(|r| r.intersection(&rect));
                        local_inner = local_inner.and_then(|r| r.intersection(&rect));
                    }
                    ClipSource::RoundedRectangle(ref rect, ref radius, mode) => {
                        // Once we encounter a clip-out, we just assume the worst
                        // case clip mask size, for now.
                        if mode == ClipMode::ClipOut {
                            has_clip_out = true;
                            break;
                        }

                        local_rect = local_rect.and_then(|r| r.intersection(rect));

                        let inner_rect = extract_inner_rect_safe(rect, radius);
                        local_inner = local_inner.and_then(|r| inner_rect.and_then(|ref inner| r.intersection(inner)));
                    }
                    ClipSource::BorderCorner{..} => {}
                }
            }

            // Work out the type of mask geometry we have, based on the
            // list of clip sources above.
            self.bounds = if has_clip_out || has_border_clip {
                // For clip-out, the mask rect is not known.
                MaskBounds {
                    outer: None,
                    inner: Some(LayerRect::zero().into()),
                }
            } else {
                MaskBounds {
                    outer: Some(local_rect.unwrap_or(LayerRect::zero()).into()),
                    inner: Some(local_inner.unwrap_or(LayerRect::zero()).into()),
                }
            };
        }

        // Step[2] - update GPU cache data

        if let Some(block_count) = self.complex_clip_range.get_block_count() {
            if let Some(mut request) = gpu_cache.request(&mut self.complex_clip_range.location) {
                for source in sources {
                    if let ClipSource::RoundedRectangle(ref rect, ref radius, mode) = *source {
                        let data = ClipData::rounded_rect(rect, radius, mode);
                        data.write(&mut request);
                    }
                }
                assert_eq!(request.close(), block_count);
            }
        }

        if let Some(block_count) = self.layer_clip_range.get_block_count() {
            if let Some(mut request) = gpu_cache.request(&mut self.layer_clip_range.location) {
                for source in sources {
                    if let ClipSource::Rectangle(rect) = *source {
                        let data = ClipData::uniform(rect, 0.0, ClipMode::Clip);
                        data.write(&mut request);
                    }
                }
                assert_eq!(request.close(), block_count);
            }
        }

        for &mut (ref mut border_source, ref mut gpu_location) in &mut self.border_corners {
            if let Some(request) = gpu_cache.request(gpu_location) {
                border_source.write(request);
            }
        }

        if let Some((ref mask, ref mut gpu_location)) = self.image {
            if let Some(request) = gpu_cache.request(gpu_location) {
                let data = ImageMaskData {
                    local_rect: mask.rect,
                };
                data.write_gpu_blocks(request);
            }
        }

        // Step[3] - update the screen bounds
        self.bounds.update(transform, device_pixel_ratio);
        &self.bounds
    }

    /// Check if this `MaskCacheInfo` actually carries any masks.
    pub fn is_masking(&self) -> bool {
        self.image.is_some() ||
        self.complex_clip_range.item_count != 0 ||
        self.layer_clip_range.item_count != 0 ||
        !self.border_corners.is_empty()
    }

    /// Return a clone of this object without any layer-aligned clip items
    pub fn strip_aligned(&self) -> Self {
        MaskCacheInfo {
            layer_clip_range: ClipAddressRange::new(0),
            .. self.clone()
        }
    }
}

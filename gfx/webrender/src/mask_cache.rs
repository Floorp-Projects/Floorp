/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ComplexClipRegion, DeviceIntRect, ImageMask, LayerPoint, LayerRect};
use api::{LayerSize, LayerToWorldTransform, LocalClip};
use border::BorderCornerClipSource;
use gpu_cache::{GpuCache, GpuCacheHandle, ToGpuBlocks};
use prim_store::{CLIP_DATA_GPU_BLOCKS, ClipData, ImageMaskData};
use util::{ComplexClipRegionHelpers, TransformedRect};
use std::ops::Not;

const MAX_CLIP: f32 = 1000000.0;

#[derive(Clone, Debug)]
pub struct ClipRegion {
    pub origin: LayerPoint,
    pub main: LayerRect,
    pub image_mask: Option<ImageMask>,
    pub complex_clips: Vec<ComplexClipRegion>,
}

impl ClipRegion {
    pub fn for_clip_node(rect: LayerRect,
                         mut complex_clips: Vec<ComplexClipRegion>,
                         mut image_mask: Option<ImageMask>)
                         -> ClipRegion {
        // All the coordinates we receive are relative to the stacking context, but we want
        // to convert them to something relative to the origin of the clip.
        let negative_origin = -rect.origin.to_vector();
        if let Some(ref mut image_mask) = image_mask {
            image_mask.rect = image_mask.rect.translate(&negative_origin);
        }

        for complex_clip in complex_clips.iter_mut() {
            complex_clip.rect = complex_clip.rect.translate(&negative_origin);
        }

        ClipRegion {
            origin: rect.origin,
            main: LayerRect::new(LayerPoint::zero(), rect.size),
            image_mask,
            complex_clips,
        }
    }

    pub fn create_for_clip_node_with_local_clip(local_clip: &LocalClip) -> ClipRegion {
        let complex_clips = match local_clip {
            &LocalClip::Rect(_) => Vec::new(),
            &LocalClip::RoundedRect(_, ref region) => vec![region.clone()],
        };
        ClipRegion::for_clip_node(*local_clip.clip_rect(), complex_clips, None)
    }

    pub fn for_local_clip(local_clip: &LocalClip) -> ClipRegion {
        let complex_clips = match local_clip {
            &LocalClip::Rect(_) => Vec::new(),
            &LocalClip::RoundedRect(_, ref region) => vec![region.clone()],
        };

        ClipRegion {
            origin: LayerPoint::zero(),
            main: *local_clip.clip_rect(),
            image_mask: None,
            complex_clips,
        }
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum ClipMode {
    Clip,           // Pixels inside the region are visible.
    ClipOut,        // Pixels outside the region are visible.
}

impl Not for ClipMode {
    type Output = ClipMode;

    fn not(self) -> ClipMode {
        match self {
            ClipMode::Clip => ClipMode::ClipOut,
            ClipMode::ClipOut => ClipMode::Clip
        }
    }
}

#[derive(Clone, Debug)]
pub enum ClipSource {
    Complex(LayerRect, f32, ClipMode),
    Region(ClipRegion),
    /// TODO(gw): This currently only handles dashed style
    /// clips, where the border style is dashed for both
    /// adjacent border edges. Expand to handle dotted style
    /// and different styles per edge.
    BorderCorner(BorderCornerClipSource),
}

impl ClipSource {
    pub fn image_mask(&self) -> Option<ImageMask> {
        match *self {
            ClipSource::Complex(..) |
            ClipSource::BorderCorner(..) => None,
            ClipSource::Region(ref region) => region.image_mask,
        }
    }
}

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
                ClipSource::Complex(..) => {
                    complex_clip_count += 1;
                }
                ClipSource::Region(ref region) => {
                    if let Some(info) = region.image_mask {
                        debug_assert!(image.is_none());     // TODO(gw): Support >1 image mask!
                        image = Some((info, GpuCacheHandle::new()));
                    }
                    complex_clip_count += region.complex_clips.len();
                    layer_clip_count += 1;
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
            let mut local_inner: Option<LayerRect> = None;
            let mut has_clip_out = false;
            let has_border_clip = !self.border_corners.is_empty();

            for source in sources {
                match *source {
                    ClipSource::Complex(rect, radius, mode) => {
                        // Once we encounter a clip-out, we just assume the worst
                        // case clip mask size, for now.
                        if mode == ClipMode::ClipOut {
                            has_clip_out = true;
                            break;
                        }
                        local_rect = local_rect.and_then(|r| r.intersection(&rect));
                        local_inner = ComplexClipRegion::new(rect, BorderRadius::uniform(radius))
                                                        .get_inner_rect_safe();
                    }
                    ClipSource::Region(ref region) => {
                        local_rect = local_rect.and_then(|r| r.intersection(&region.main));
                        local_inner = match region.image_mask {
                            Some(ref mask) => {
                                if !mask.repeat {
                                    local_rect = local_rect.and_then(|r| r.intersection(&mask.rect));
                                }
                                None
                            },
                            None => local_rect,
                        };

                        for clip in &region.complex_clips {
                            local_rect = local_rect.and_then(|r| r.intersection(&clip.rect));
                            local_inner = local_inner.and_then(|r| clip.get_inner_rect_safe()
                                                                       .and_then(|ref inner| r.intersection(inner)));
                        }
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
                // TODO(gw): local inner is only valid if there's a single clip (for now).
                // This can be improved in the future, with some proper
                // rectangle region handling.
                if sources.len() > 1 {
                    local_inner = None;
                }

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
                    match *source {
                        ClipSource::Complex(rect, radius, mode) => {
                            let data = ClipData::uniform(rect, radius, mode);
                            data.write(&mut request);
                        }
                        ClipSource::Region(ref region) => {
                            for clip in &region.complex_clips {
                                let data = ClipData::from_clip_region(&clip);
                                data.write(&mut request);
                            }
                        }
                        ClipSource::BorderCorner{..} => {}
                    }
                }
                assert_eq!(request.close(), block_count);
            }
        }

        if let Some(block_count) = self.layer_clip_range.get_block_count() {
            if let Some(mut request) = gpu_cache.request(&mut self.layer_clip_range.location) {
                for source in sources {
                    if let ClipSource::Region(ref region) = *source {
                        let data = ClipData::uniform(region.main, 0.0, ClipMode::Clip);
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

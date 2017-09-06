/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ComplexClipRegion, ImageMask, ImageRendering};
use api::{LayerPoint, LayerRect, LayerToWorldTransform, LocalClip};
use border::BorderCornerClipSource;
use gpu_cache::GpuCache;
use mask_cache::MaskCacheInfo;
use resource_cache::ResourceCache;
use std::ops::Not;

#[derive(Clone, Debug)]
pub struct ClipRegion {
    pub origin: LayerPoint,
    pub main: LayerRect,
    pub image_mask: Option<ImageMask>,
    pub complex_clips: Vec<ComplexClipRegion>,
}

impl ClipRegion {
    pub fn create_for_clip_node(rect: LayerRect,
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
        ClipRegion::create_for_clip_node(*local_clip.clip_rect(), complex_clips, None)
    }

    pub fn create_for_local_clip(local_clip: &LocalClip) -> ClipRegion {
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

#[derive(Debug)]
pub struct ClipSources {
    clips: Vec<ClipSource>,
    mask_cache_info: MaskCacheInfo,
}

impl ClipSources {
    pub fn new(clips: Vec<ClipSource>) -> ClipSources {
        let mask_cache_info = MaskCacheInfo::new(&clips);

        ClipSources {
            clips,
            mask_cache_info,
        }
    }

    pub fn clips(&self) -> &[ClipSource] {
        &self.clips
    }

    pub fn update(&mut self,
                  layer_transform: &LayerToWorldTransform,
                  gpu_cache: &mut GpuCache,
                  resource_cache: &mut ResourceCache,
                  device_pixel_ratio: f32) {
        if self.clips.is_empty() {
            return;
        }

        self.mask_cache_info
            .update(&self.clips,
                    layer_transform,
                    gpu_cache,
                    device_pixel_ratio);

        for clip in &self.clips {
            if let ClipSource::Region(ClipRegion{ image_mask: Some(ref mask), .. }, ..) = *clip {
                resource_cache.request_image(mask.image,
                                             ImageRendering::Auto,
                                             None,
                                             gpu_cache);
            }
        }
    }

    pub fn is_masking(&self) -> bool {
        self.mask_cache_info.is_masking()
    }

    pub fn clone_mask_cache_info(&self, keep_aligned: bool) -> MaskCacheInfo {
        if keep_aligned {
            self.mask_cache_info.clone()
        } else {
            self.mask_cache_info.strip_aligned()
        }
    }
}

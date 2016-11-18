/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::{Rect, Matrix4D};
use gpu_store::{GpuStore, GpuStoreAddress};
use internal_types::{DeviceRect, DeviceSize};
use prim_store::{ClipData, GpuBlock32, PrimitiveClipSource, PrimitiveStore};
use prim_store::{CLIP_DATA_GPU_SIZE, MASK_DATA_GPU_SIZE};
use std::i32;
use tiling::StackingContextIndex;
use util::{rect_from_points_f, TransformedRect};
use webrender_traits::{AuxiliaryLists, BorderRadius, ComplexClipRegion, ImageMask};

const MAX_COORD: f32 = 1.0e+16;
pub const OPAQUE_TASK_INDEX: i32 = i32::MAX;

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct ClipAddressRange {
    pub start: GpuStoreAddress,
    pub item_count: u32,
}

type ImageMaskIndex = u16;

#[derive(Debug, Clone, Copy, Eq, PartialEq, Hash)]
pub struct MaskCacheKey {
    pub layer_id: StackingContextIndex,
    pub clip_range: ClipAddressRange,
    pub image: Option<GpuStoreAddress>,
}

impl MaskCacheKey {
    pub fn empty(layer_id: StackingContextIndex) -> MaskCacheKey {
        MaskCacheKey {
            layer_id: layer_id,
            clip_range: ClipAddressRange {
                start: GpuStoreAddress(0),
                item_count: 0,
            },
            image: None,
        }
    }
}

#[derive(Debug)]
pub struct MaskCacheInfo {
    pub key: MaskCacheKey,
    // this is needed to update the ImageMaskData after the
    // ResourceCache allocates/load the actual data
    // will be simplified after the TextureCache upgrade
    pub image: Option<ImageMask>,
    pub local_rect: Option<Rect<f32>>,
    pub local_inner: Option<Rect<f32>>,
    pub inner_rect: DeviceRect,
    pub outer_rect: DeviceRect,
}

impl MaskCacheInfo {
    /// Create a new mask cache info. It allocates the GPU store data but leaves
    /// it unitialized for the following `update()` call to deal with.
    pub fn new(source: &PrimitiveClipSource,
               layer_id: StackingContextIndex,
               clip_store: &mut GpuStore<GpuBlock32>)
               -> Option<MaskCacheInfo> {
        let mut clip_key = MaskCacheKey::empty(layer_id);

        let image = match source {
            &PrimitiveClipSource::NoClip => None,
            &PrimitiveClipSource::Complex(..) => {
                clip_key.clip_range.item_count = 1;
                clip_key.clip_range.start = clip_store.alloc(CLIP_DATA_GPU_SIZE);
                None
            }
            &PrimitiveClipSource::Region(ref region) => {
                let num = region.complex.length;
                if num != 0 {
                    clip_key.clip_range.item_count = num as u32;
                    clip_key.clip_range.start = clip_store.alloc(CLIP_DATA_GPU_SIZE * num);
                }
                if region.image_mask.is_some() {
                    let address = clip_store.alloc(MASK_DATA_GPU_SIZE);
                    clip_key.image = Some(address);
                }
                region.image_mask
            }
        };

        if clip_key.clip_range.item_count != 0 || clip_key.image.is_some() {
            Some(MaskCacheInfo {
                key: clip_key,
                image: image,
                local_rect: None,
                local_inner: None,
                inner_rect: DeviceRect::zero(),
                outer_rect: DeviceRect::zero(),
            })
        } else {
            None
        }
    }

    pub fn update(&mut self,
                  source: &PrimitiveClipSource,
                  transform: &Matrix4D<f32>,
                  clip_store: &mut GpuStore<GpuBlock32>,
                  device_pixel_ratio: f32,
                  aux_lists: &AuxiliaryLists) {

        if self.local_rect.is_none() {
            let (mut local_rect, mut local_inner);
            match source {
                &PrimitiveClipSource::NoClip => unreachable!(),
                &PrimitiveClipSource::Complex(rect, radius) => {
                    let slice = clip_store.get_slice_mut(self.key.clip_range.start, CLIP_DATA_GPU_SIZE);
                    let data = ClipData::uniform(rect, radius);
                    PrimitiveStore::populate_clip_data(slice, data);
                    debug_assert_eq!(self.key.clip_range.item_count, 1);
                    local_rect = Some(rect);
                    local_inner = ComplexClipRegion::new(rect,
                                                         BorderRadius::uniform(radius))
                                                    .get_inner_rect();
                }
                &PrimitiveClipSource::Region(ref region) => {
                    local_rect = Some(rect_from_points_f(-MAX_COORD, -MAX_COORD, MAX_COORD, MAX_COORD));
                    local_inner = match region.image_mask {
                        Some(ref mask) if !mask.repeat => {
                            local_rect = local_rect.and_then(|r| r.intersection(&mask.rect));
                            None
                        },
                        Some(_) => None,
                        None => local_rect,
                    };
                    let clips = aux_lists.complex_clip_regions(&region.complex);
                    assert_eq!(self.key.clip_range.item_count, clips.len() as u32);
                    let slice = clip_store.get_slice_mut(self.key.clip_range.start, CLIP_DATA_GPU_SIZE * clips.len());
                    for (clip, chunk) in clips.iter().zip(slice.chunks_mut(CLIP_DATA_GPU_SIZE)) {
                        let data = ClipData::from_clip_region(clip);
                        PrimitiveStore::populate_clip_data(chunk, data);
                        local_rect = local_rect.and_then(|r| r.intersection(&clip.rect));
                        local_inner = local_inner.and_then(|r| clip.get_inner_rect()
                                                                   .and_then(|ref inner| r.intersection(inner)));
                    }
                }
            };
            self.local_rect = Some(local_rect.unwrap_or(Rect::zero()));
            self.local_inner = local_inner;
        }

        let transformed = TransformedRect::new(self.local_rect.as_ref().unwrap(),
                                               &transform,
                                               device_pixel_ratio);
        self.outer_rect = transformed.bounding_rect;

        self.inner_rect = if let Some(ref inner_rect) = self.local_inner {
            let transformed = TransformedRect::new(inner_rect,
                                                   &transform,
                                                   device_pixel_ratio);
            transformed.inner_rect
        } else {
            DeviceRect::new(self.outer_rect.origin, DeviceSize::zero())
        }
    }
}

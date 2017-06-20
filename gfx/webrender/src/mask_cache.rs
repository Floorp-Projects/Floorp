/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use border::BorderCornerClipSource;
use gpu_store::GpuStoreAddress;
use prim_store::{ClipData, GpuBlock32, ImageMaskData, PrimitiveStore};
use prim_store::{CLIP_DATA_GPU_SIZE, MASK_DATA_GPU_SIZE};
use renderer::VertexDataStore;
use util::{ComplexClipRegionHelpers, MatrixHelpers, TransformedRect};
use webrender_traits::{BorderRadius, BuiltDisplayList, ClipRegion, ComplexClipRegion, ImageMask};
use webrender_traits::{DeviceIntRect, LayerToWorldTransform};
use webrender_traits::{DeviceRect, LayerRect, LayerPoint, LayerSize};
use std::ops::Not;

const MAX_CLIP: f32 = 1000000.0;

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

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum RegionMode {
    IncludeRect,
    ExcludeRect,
}

#[derive(Clone, Debug)]
pub enum ClipSource {
    Complex(LayerRect, f32, ClipMode),
    // The RegionMode here specifies whether to consider the rect
    // from the clip region as part of the mask. This is true
    // for clip/scroll nodes, but false for primitives, where
    // the clip rect is handled in local space.
    Region(ClipRegion, RegionMode),

    // TODO(gw): This currently only handles dashed style
    // clips, where the border style is dashed for both
    // adjacent border edges. Expand to handle dotted style
    // and different styles per edge.
    BorderCorner(BorderCornerClipSource),
}

impl ClipSource {
    pub fn image_mask(&self) -> Option<ImageMask> {
        match *self {
            ClipSource::Complex(..) |
            ClipSource::BorderCorner{..} => None,
            ClipSource::Region(ref region, _) => region.image_mask,
        }
    }
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct ClipAddressRange {
    pub start: GpuStoreAddress,
    item_count: usize,
}

/// Represents a local rect and a device space
/// bounding rect that can be updated when the
/// transform changes.
#[derive(Clone, Debug, PartialEq)]
pub struct Geometry {
    pub local_rect: LayerRect,
    pub bounding_rect: DeviceIntRect,
}

impl Geometry {
    fn new(local_rect: LayerRect) -> Geometry {
        Geometry {
            local_rect: local_rect,
            bounding_rect: DeviceIntRect::zero(),
        }
    }

    fn update(&mut self,
              transform: &LayerToWorldTransform,
              device_pixel_ratio: f32) {
        let transformed = TransformedRect::new(&self.local_rect,
                                               transform,
                                               device_pixel_ratio);
        self.bounding_rect = transformed.bounding_rect;
    }
}

/// Depending on the complexity of the clip, we may either
/// know the outer and/or inner rect, or neither or these.
/// In the case of a clip-out, we currently set the mask
/// bounds to be unknown. This is conservative, but ensures
/// correctness. In the future we can make this a lot
/// more clever with some proper region handling.
#[derive(Clone, Debug, PartialEq)]
pub enum MaskBounds {
    /// We know both the outer and inner rect. This is the
    /// fast path for, e.g. a simple rounded rect.
    OuterInner(Geometry, Geometry),
    /// We know the outer rect only.
    Outer(Geometry),
    /// We can't determine the bounds - draw mask over entire rect.
    /// This is currently used for clip-out operations on
    /// box shadows.
    None,
}

#[derive(Clone, Debug)]
pub struct MaskCacheInfo {
    pub complex_clip_range: ClipAddressRange,
    pub effective_complex_clip_count: usize,
    pub image: Option<(ImageMask, GpuStoreAddress)>,
    pub border_corners: Vec<(BorderCornerClipSource, GpuStoreAddress)>,
    pub bounds: Option<MaskBounds>,
    pub is_aligned: bool,
}

impl MaskCacheInfo {
    /// Create a new mask cache info. It allocates the GPU store data but leaves
    /// it unitialized for the following `update()` call to deal with.
    pub fn new(clips: &[ClipSource],
               clip_store: &mut VertexDataStore<GpuBlock32>)
               -> Option<MaskCacheInfo> {
        if clips.is_empty() {
            return None;
        }

        let mut image = None;
        let mut border_corners = Vec::new();
        let mut complex_clip_count = 0;

        // Work out how much clip data space we need to allocate
        // and if we have an image mask.
        for clip in clips {
            match *clip {
                ClipSource::Complex(..) => {
                    complex_clip_count += 1;
                }
                ClipSource::Region(ref region, region_mode) => {
                    if let Some(info) = region.image_mask {
                        debug_assert!(image.is_none());     // TODO(gw): Support >1 image mask!
                        image = Some((info, clip_store.alloc(MASK_DATA_GPU_SIZE)));
                    }
                    complex_clip_count += region.complex_clip_count;
                    if region_mode == RegionMode::IncludeRect {
                        complex_clip_count += 1;
                    }
                }
                ClipSource::BorderCorner(ref source) => {
                    // One block for the corner header, plus one
                    // block per dash to clip out.
                    let gpu_address = clip_store.alloc(1 + source.max_clip_count);
                    border_corners.push((source.clone(), gpu_address));
                }
            }
        }

        let complex_clip_range = ClipAddressRange {
            start: if complex_clip_count > 0 {
                clip_store.alloc(CLIP_DATA_GPU_SIZE * complex_clip_count)
            } else {
                GpuStoreAddress(0)
            },
            item_count: complex_clip_count,
        };

        Some(MaskCacheInfo {
            complex_clip_range: complex_clip_range,
            effective_complex_clip_count: complex_clip_range.item_count,
            image: image,
            border_corners: border_corners,
            bounds: None,
            is_aligned: true,
        })
    }

    pub fn update(&mut self,
                  sources: &[ClipSource],
                  transform: &LayerToWorldTransform,
                  clip_store: &mut VertexDataStore<GpuBlock32>,
                  device_pixel_ratio: f32,
                  display_list: &BuiltDisplayList) {
        let is_aligned = transform.preserves_2d_axis_alignment();

        // If we haven't cached this info, or if the transform type has changed
        // we need to re-calculate the number of clips.
        if self.bounds.is_none() || self.is_aligned != is_aligned {
            let mut local_rect = Some(LayerRect::new(LayerPoint::new(-MAX_CLIP, -MAX_CLIP),
                                                     LayerSize::new(2.0 * MAX_CLIP, 2.0 * MAX_CLIP)));
            let mut local_inner: Option<LayerRect> = None;
            let mut has_clip_out = false;
            let mut has_border_clip = false;

            self.effective_complex_clip_count = 0;
            self.is_aligned = is_aligned;

            for source in sources {
                match *source {
                    ClipSource::Complex(rect, radius, mode) => {
                        // Once we encounter a clip-out, we just assume the worst
                        // case clip mask size, for now.
                        if mode == ClipMode::ClipOut {
                            has_clip_out = true;
                        }
                        debug_assert!(self.effective_complex_clip_count < self.complex_clip_range.item_count);
                        let address = self.complex_clip_range.start + self.effective_complex_clip_count * CLIP_DATA_GPU_SIZE;
                        self.effective_complex_clip_count += 1;

                        let slice = clip_store.get_slice_mut(address, CLIP_DATA_GPU_SIZE);
                        let data = ClipData::uniform(rect, radius, mode);
                        PrimitiveStore::populate_clip_data(slice, data);
                        local_rect = local_rect.and_then(|r| r.intersection(&rect));
                        local_inner = ComplexClipRegion::new(rect, BorderRadius::uniform(radius))
                                                        .get_inner_rect_safe();
                    }
                    ClipSource::Region(ref region, region_mode) => {
                        local_rect = local_rect.and_then(|r| r.intersection(&region.main));
                        local_inner = match region.image_mask {
                            Some(ref mask) if !mask.repeat => {
                                local_rect = local_rect.and_then(|r| r.intersection(&mask.rect));
                                None
                            },
                            Some(_) => None,
                            None => local_rect,
                        };

                        let clips = display_list.get(region.complex_clips);
                        if !self.is_aligned && region_mode == RegionMode::IncludeRect {
                            // we have an extra clip rect coming from the transformed layer
                            debug_assert!(self.effective_complex_clip_count < self.complex_clip_range.item_count);
                            let address = self.complex_clip_range.start + self.effective_complex_clip_count * CLIP_DATA_GPU_SIZE;
                            self.effective_complex_clip_count += 1;

                            let slice = clip_store.get_slice_mut(address, CLIP_DATA_GPU_SIZE);
                            PrimitiveStore::populate_clip_data(slice, ClipData::uniform(region.main, 0.0, ClipMode::Clip));
                        }

                        debug_assert!(self.effective_complex_clip_count + clips.len() <= self.complex_clip_range.item_count);
                        let address = self.complex_clip_range.start + self.effective_complex_clip_count * CLIP_DATA_GPU_SIZE;
                        self.effective_complex_clip_count += clips.len();

                        let slice = clip_store.get_slice_mut(address, CLIP_DATA_GPU_SIZE * clips.len());
                        for (clip, chunk) in clips.zip(slice.chunks_mut(CLIP_DATA_GPU_SIZE)) {
                            let data = ClipData::from_clip_region(&clip);
                            PrimitiveStore::populate_clip_data(chunk, data);
                            local_rect = local_rect.and_then(|r| r.intersection(&clip.rect));
                            local_inner = local_inner.and_then(|r| clip.get_inner_rect_safe()
                                                                       .and_then(|ref inner| r.intersection(inner)));
                        }
                    }
                    ClipSource::BorderCorner{..} => {}
                }
            }

            for &mut (ref mut source, gpu_address) in &mut self.border_corners {
                has_border_clip = true;
                let slice = clip_store.get_slice_mut(gpu_address,
                                                     1 + source.max_clip_count);
                source.populate_gpu_data(slice);
            }

            if let Some((ref mask, gpu_address)) = self.image {
                let mask_data = clip_store.get_slice_mut(gpu_address, MASK_DATA_GPU_SIZE);
                mask_data[0] = GpuBlock32::from(ImageMaskData {
                    padding: DeviceRect::zero(),
                    local_rect: mask.rect,
                });
            }

            // Work out the type of mask geometry we have, based on the
            // list of clip sources above.
            if has_clip_out || has_border_clip {
                // For clip-out, the mask rect is not known.
                self.bounds = Some(MaskBounds::None);
            } else {
                // TODO(gw): local inner is only valid if there's a single clip (for now).
                // This can be improved in the future, with some proper
                // rectangle region handling.
                if sources.len() > 1 {
                    local_inner = None;
                }

                let local_rect = local_rect.unwrap_or(LayerRect::zero());

                self.bounds = match local_inner {
                    Some(local_inner) => {
                        Some(MaskBounds::OuterInner(Geometry::new(local_rect),
                                                    Geometry::new(local_inner)))
                    }
                    None => {
                        Some(MaskBounds::Outer(Geometry::new(local_rect)))
                    }
                };
            }
        }

        // Update the device space bounding rects of the mask
        // geometry.
        match self.bounds.as_mut().unwrap() {
            &mut MaskBounds::None => {}
            &mut MaskBounds::Outer(ref mut outer) => {
                outer.update(transform, device_pixel_ratio);
            }
            &mut MaskBounds::OuterInner(ref mut outer, ref mut inner) => {
                outer.update(transform, device_pixel_ratio);
                inner.update(transform, device_pixel_ratio);
            }
        }
    }

    /// Check if this `MaskCacheInfo` actually carries any masks. `effective_complex_clip_count`
    /// can change during the `update` call depending on the transformation, so the mask may
    /// appear to be empty.
    pub fn is_masking(&self) -> bool {
        self.image.is_some() ||
        self.effective_complex_clip_count != 0 ||
        !self.border_corners.is_empty()
    }
}

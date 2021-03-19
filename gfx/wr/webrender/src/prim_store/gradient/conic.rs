/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Conic gradients
//!
//! Specification: https://drafts.csswg.org/css-images-4/#conic-gradients
//!
//! Conic gradients are rendered via a brush shader directly into the picture pass.
//!
//! TODO: The plan is to use render tasks and the image brush instead.

use api::{ExtendMode, GradientStop};
use api::units::{LayoutPoint, LayoutSize, LayoutVector2D};
use crate::scene_building::IsVisible;
use crate::frame_builder::FrameBuildingState;
use crate::gpu_cache::GpuCacheHandle;
use crate::intern::{Internable, InternDebug, Handle as InternHandle};
use crate::internal_types::LayoutPrimitiveInfo;
use crate::prim_store::{BrushSegment, GradientTileRange};
use crate::prim_store::{PrimitiveInstanceKind, PrimitiveOpacity};
use crate::prim_store::{PrimKeyCommonData, PrimTemplateCommonData, PrimitiveStore};
use crate::prim_store::{NinePatchDescriptor, PointKey, SizeKey, InternablePrimitive};
use std::{hash, ops::{Deref, DerefMut}};
use crate::util::pack_as_float;
use super::{get_gradient_opacity, stops_and_min_alpha, GradientStopKey, GradientGpuBlockBuilder};

/// Hashable conic gradient parameters, for use during prim interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq)]
pub struct ConicGradientParams {
    pub angle: f32, // in radians
    pub start_offset: f32,
    pub end_offset: f32,
}

impl Eq for ConicGradientParams {}

impl hash::Hash for ConicGradientParams {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.angle.to_bits().hash(state);
        self.start_offset.to_bits().hash(state);
        self.end_offset.to_bits().hash(state);
    }
}

/// Identifying key for a line decoration.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash, MallocSizeOf)]
pub struct ConicGradientKey {
    pub common: PrimKeyCommonData,
    pub extend_mode: ExtendMode,
    pub center: PointKey,
    pub params: ConicGradientParams,
    pub stretch_size: SizeKey,
    pub stops: Vec<GradientStopKey>,
    pub tile_spacing: SizeKey,
    pub nine_patch: Option<Box<NinePatchDescriptor>>,
}

impl ConicGradientKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        conic_grad: ConicGradient,
    ) -> Self {
        ConicGradientKey {
            common: info.into(),
            extend_mode: conic_grad.extend_mode,
            center: conic_grad.center,
            params: conic_grad.params,
            stretch_size: conic_grad.stretch_size,
            stops: conic_grad.stops,
            tile_spacing: conic_grad.tile_spacing,
            nine_patch: conic_grad.nine_patch,
        }
    }
}

impl InternDebug for ConicGradientKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct ConicGradientTemplate {
    pub common: PrimTemplateCommonData,
    pub extend_mode: ExtendMode,
    pub center: LayoutPoint,
    pub params: ConicGradientParams,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub brush_segments: Vec<BrushSegment>,
    pub stops_opacity: PrimitiveOpacity,
    pub stops: Vec<GradientStop>,
    pub stops_handle: GpuCacheHandle,
}

impl Deref for ConicGradientTemplate {
    type Target = PrimTemplateCommonData;
    fn deref(&self) -> &Self::Target {
        &self.common
    }
}

impl DerefMut for ConicGradientTemplate {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.common
    }
}

impl From<ConicGradientKey> for ConicGradientTemplate {
    fn from(item: ConicGradientKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(item.common);
        let mut brush_segments = Vec::new();

        if let Some(ref nine_patch) = item.nine_patch {
            brush_segments = nine_patch.create_segments(common.prim_rect.size);
        }

        let (stops, min_alpha) = stops_and_min_alpha(&item.stops);

        // Save opacity of the stops for use in
        // selecting which pass this gradient
        // should be drawn in.
        let stops_opacity = PrimitiveOpacity::from_alpha(min_alpha);

        ConicGradientTemplate {
            common,
            center: item.center.into(),
            extend_mode: item.extend_mode,
            params: item.params,
            stretch_size: item.stretch_size.into(),
            tile_spacing: item.tile_spacing.into(),
            brush_segments,
            stops_opacity,
            stops,
            stops_handle: GpuCacheHandle::new(),
        }
    }
}

impl ConicGradientTemplate {
    /// Update the GPU cache for a given primitive template. This may be called multiple
    /// times per frame, by each primitive reference that refers to this interned
    /// template. The initial request call to the GPU cache ensures that work is only
    /// done if the cache entry is invalid (due to first use or eviction).
    pub fn update(
        &mut self,
        frame_state: &mut FrameBuildingState,
    ) {
        if let Some(mut request) =
            frame_state.gpu_cache.request(&mut self.common.gpu_cache_handle) {
            // write_prim_gpu_blocks
            request.push([
                self.center.x,
                self.center.y,
                self.params.start_offset,
                self.params.end_offset,
            ]);
            request.push([
                self.params.angle,
                pack_as_float(self.extend_mode as u32),
                self.stretch_size.width,
                self.stretch_size.height,
            ]);

            // write_segment_gpu_blocks
            for segment in &self.brush_segments {
                // has to match VECS_PER_SEGMENT
                request.write_segment(
                    segment.local_rect,
                    segment.extra_data,
                );
            }
        }

        if let Some(mut request) = frame_state.gpu_cache.request(&mut self.stops_handle) {
            GradientGpuBlockBuilder::build(
                false,
                &mut request,
                &self.stops,
            );
        }

        self.opacity = get_gradient_opacity(
            self.common.prim_rect,
            self.stretch_size,
            self.tile_spacing,
            self.stops_opacity,
        );
    }
}

pub type ConicGradientDataHandle = InternHandle<ConicGradient>;

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ConicGradient {
    pub extend_mode: ExtendMode,
    pub center: PointKey,
    pub params: ConicGradientParams,
    pub stretch_size: SizeKey,
    pub stops: Vec<GradientStopKey>,
    pub tile_spacing: SizeKey,
    pub nine_patch: Option<Box<NinePatchDescriptor>>,
}

impl Internable for ConicGradient {
    type Key = ConicGradientKey;
    type StoreData = ConicGradientTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_CONIC_GRADIENTS;
}

impl InternablePrimitive for ConicGradient {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> ConicGradientKey {
        ConicGradientKey::new(info, self)
    }

    fn make_instance_kind(
        _key: ConicGradientKey,
        data_handle: ConicGradientDataHandle,
        _prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::ConicGradient {
            data_handle,
            visible_tiles_range: GradientTileRange::empty(),
        }
    }
}

impl IsVisible for ConicGradient {
    fn is_visible(&self) -> bool {
        true
    }
}


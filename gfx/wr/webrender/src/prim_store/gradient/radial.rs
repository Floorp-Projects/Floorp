/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Radial gradients
//!
//! Specification: https://drafts.csswg.org/css-images-4/#radial-gradients
//!
//! Radial gradients are rendered via a brush shader directly into the picture pass.
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

/// Hashable radial gradient parameters, for use during prim interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq)]
pub struct RadialGradientParams {
    pub start_radius: f32,
    pub end_radius: f32,
    pub ratio_xy: f32,
}

impl Eq for RadialGradientParams {}

impl hash::Hash for RadialGradientParams {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.start_radius.to_bits().hash(state);
        self.end_radius.to_bits().hash(state);
        self.ratio_xy.to_bits().hash(state);
    }
}

/// Identifying key for a radial gradient.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash, MallocSizeOf)]
pub struct RadialGradientKey {
    pub common: PrimKeyCommonData,
    pub extend_mode: ExtendMode,
    pub center: PointKey,
    pub params: RadialGradientParams,
    pub stretch_size: SizeKey,
    pub stops: Vec<GradientStopKey>,
    pub tile_spacing: SizeKey,
    pub nine_patch: Option<Box<NinePatchDescriptor>>,
}

impl RadialGradientKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        radial_grad: RadialGradient,
    ) -> Self {
        RadialGradientKey {
            common: info.into(),
            extend_mode: radial_grad.extend_mode,
            center: radial_grad.center,
            params: radial_grad.params,
            stretch_size: radial_grad.stretch_size,
            stops: radial_grad.stops,
            tile_spacing: radial_grad.tile_spacing,
            nine_patch: radial_grad.nine_patch,
        }
    }
}

impl InternDebug for RadialGradientKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct RadialGradientTemplate {
    pub common: PrimTemplateCommonData,
    pub extend_mode: ExtendMode,
    pub center: LayoutPoint,
    pub params: RadialGradientParams,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub brush_segments: Vec<BrushSegment>,
    pub stops_opacity: PrimitiveOpacity,
    pub stops: Vec<GradientStop>,
    pub stops_handle: GpuCacheHandle,
}

impl Deref for RadialGradientTemplate {
    type Target = PrimTemplateCommonData;
    fn deref(&self) -> &Self::Target {
        &self.common
    }
}

impl DerefMut for RadialGradientTemplate {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.common
    }
}

impl From<RadialGradientKey> for RadialGradientTemplate {
    fn from(item: RadialGradientKey) -> Self {
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

        RadialGradientTemplate {
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

impl RadialGradientTemplate {
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
                self.params.start_radius,
                self.params.end_radius,
            ]);
            request.push([
                self.params.ratio_xy,
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

pub type RadialGradientDataHandle = InternHandle<RadialGradient>;

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RadialGradient {
    pub extend_mode: ExtendMode,
    pub center: PointKey,
    pub params: RadialGradientParams,
    pub stretch_size: SizeKey,
    pub stops: Vec<GradientStopKey>,
    pub tile_spacing: SizeKey,
    pub nine_patch: Option<Box<NinePatchDescriptor>>,
}

impl Internable for RadialGradient {
    type Key = RadialGradientKey;
    type StoreData = RadialGradientTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_RADIAL_GRADIENTS;
}

impl InternablePrimitive for RadialGradient {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> RadialGradientKey {
        RadialGradientKey::new(info, self)
    }

    fn make_instance_kind(
        _key: RadialGradientKey,
        data_handle: RadialGradientDataHandle,
        _prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::RadialGradient {
            data_handle,
            visible_tiles_range: GradientTileRange::empty(),
        }
    }
}

impl IsVisible for RadialGradient {
    fn is_visible(&self) -> bool {
        true
    }
}

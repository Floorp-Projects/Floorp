/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Linear gradients
//!
//! Specification: https://drafts.csswg.org/css-images-4/#linear-gradients
//!
//! Linear gradients can be rendered in one of the two following ways:
//! - Via fast-path that caches segments of the gradients via render tasks
//!   and is composited with the image brush in some cases.
//! - In the general case, via gradient brush shader.
//!
//! TODO: The plan is to always use render tasks + image brush for linear gradients.

use euclid::approxeq::ApproxEq;
use api::{ExtendMode, GradientStop, LineOrientation, PremultipliedColorF};
use api::units::*;
use crate::scene_building::IsVisible;
use crate::frame_builder::FrameBuildingState;
use crate::gpu_cache::GpuCacheHandle;
use crate::intern::{Internable, InternDebug, Handle as InternHandle};
use crate::internal_types::LayoutPrimitiveInfo;
use crate::prim_store::{BrushSegment, GradientTileRange, VectorKey};
use crate::prim_store::{PrimitiveInstanceKind, PrimitiveOpacity};
use crate::prim_store::{PrimKeyCommonData, PrimTemplateCommonData, PrimitiveStore};
use crate::prim_store::{NinePatchDescriptor, PointKey, SizeKey, InternablePrimitive};
use crate::render_task_graph::RenderTaskId;
use crate::util::pack_as_float;
use crate::texture_cache::TEXTURE_REGION_DIMENSIONS;
use super::{get_gradient_opacity, stops_and_min_alpha, GradientStopKey, GradientGpuBlockBuilder};
use std::ops::{Deref, DerefMut};

/// The maximum number of stops a gradient may have to use the fast path.
pub const GRADIENT_FP_STOPS: usize = 4;

/// Identifying key for a linear gradient.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash, MallocSizeOf)]
pub struct LinearGradientKey {
    pub common: PrimKeyCommonData,
    pub extend_mode: ExtendMode,
    pub start_point: PointKey,
    pub end_point: PointKey,
    pub stretch_size: SizeKey,
    pub tile_spacing: SizeKey,
    pub stops: Vec<GradientStopKey>,
    pub reverse_stops: bool,
    pub nine_patch: Option<Box<NinePatchDescriptor>>,
}

impl LinearGradientKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        linear_grad: LinearGradient,
    ) -> Self {
        LinearGradientKey {
            common: info.into(),
            extend_mode: linear_grad.extend_mode,
            start_point: linear_grad.start_point,
            end_point: linear_grad.end_point,
            stretch_size: linear_grad.stretch_size,
            tile_spacing: linear_grad.tile_spacing,
            stops: linear_grad.stops,
            reverse_stops: linear_grad.reverse_stops,
            nine_patch: linear_grad.nine_patch,
        }
    }
}

impl InternDebug for LinearGradientKey {}

#[derive(Clone, Debug, Hash, MallocSizeOf, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct GradientCacheKey {
    pub orientation: LineOrientation,
    pub start_stop_point: VectorKey,
    pub stops: [GradientStopKey; GRADIENT_FP_STOPS],
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct LinearGradientTemplate {
    pub common: PrimTemplateCommonData,
    pub extend_mode: ExtendMode,
    pub start_point: LayoutPoint,
    pub end_point: LayoutPoint,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub stops_opacity: PrimitiveOpacity,
    pub stops: Vec<GradientStop>,
    pub brush_segments: Vec<BrushSegment>,
    pub reverse_stops: bool,
    pub stops_handle: GpuCacheHandle,
    /// If true, this gradient can be drawn via the fast path
    /// (cache gradient, and draw as image).
    pub supports_caching: bool,
}

impl Deref for LinearGradientTemplate {
    type Target = PrimTemplateCommonData;
    fn deref(&self) -> &Self::Target {
        &self.common
    }
}

impl DerefMut for LinearGradientTemplate {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.common
    }
}

impl From<LinearGradientKey> for LinearGradientTemplate {
    fn from(item: LinearGradientKey) -> Self {
        let common = PrimTemplateCommonData::with_key_common(item.common);

        // Check if we can draw this gradient via a fast path by caching the
        // gradient in a smaller task, and drawing as an image.
        // TODO(gw): Aim to reduce the constraints on fast path gradients in future,
        //           although this catches the vast majority of gradients on real pages.
        let mut supports_caching =
            // Gradient must cover entire primitive
            item.tile_spacing.w + item.stretch_size.w >= common.prim_rect.size.width &&
            item.tile_spacing.h + item.stretch_size.h >= common.prim_rect.size.height &&
            // Must be a vertical or horizontal gradient
            (item.start_point.x.approx_eq(&item.end_point.x) ||
             item.start_point.y.approx_eq(&item.end_point.y)) &&
            // Fast path not supported on segmented (border-image) gradients.
            item.nine_patch.is_none();

        // if we support caching and the gradient uses repeat, we might potentially
        // emit a lot of quads to cover the primitive. each quad will still cover
        // the entire gradient along the other axis, so the effect is linear in
        // display resolution, not quadratic (unlike say a tiny background image
        // tiling the display). in addition, excessive minification may lead to
        // texture trashing. so use the minification as a proxy heuristic for both
        // cases.
        //
        // note that the actual number of quads may be further increased due to
        // hard-stops and/or more than GRADIENT_FP_STOPS stops per gradient.
        if supports_caching && item.extend_mode == ExtendMode::Repeat {
            let single_repeat_size =
                if item.start_point.x.approx_eq(&item.end_point.x) {
                    item.end_point.y - item.start_point.y
                } else {
                    item.end_point.x - item.start_point.x
                };
            let downscaling = single_repeat_size as f32 / TEXTURE_REGION_DIMENSIONS as f32;
            if downscaling < 0.1 {
                // if a single copy of the gradient is this small relative to its baked
                // gradient cache, we have bad texture caching and/or too many quads.
                supports_caching = false;
            }
        }

        let (stops, min_alpha) = stops_and_min_alpha(&item.stops);

        let mut brush_segments = Vec::new();

        if let Some(ref nine_patch) = item.nine_patch {
            brush_segments = nine_patch.create_segments(common.prim_rect.size);
        }

        // Save opacity of the stops for use in
        // selecting which pass this gradient
        // should be drawn in.
        let stops_opacity = PrimitiveOpacity::from_alpha(min_alpha);

        LinearGradientTemplate {
            common,
            extend_mode: item.extend_mode,
            start_point: item.start_point.into(),
            end_point: item.end_point.into(),
            stretch_size: item.stretch_size.into(),
            tile_spacing: item.tile_spacing.into(),
            stops_opacity,
            stops,
            brush_segments,
            reverse_stops: item.reverse_stops,
            stops_handle: GpuCacheHandle::new(),
            supports_caching,
        }
    }
}

impl LinearGradientTemplate {
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
                self.start_point.x,
                self.start_point.y,
                self.end_point.x,
                self.end_point.y,
            ]);
            request.push([
                pack_as_float(self.extend_mode as u32),
                self.stretch_size.width,
                self.stretch_size.height,
                0.0,
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
                self.reverse_stops,
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

pub type LinearGradientDataHandle = InternHandle<LinearGradient>;

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct LinearGradient {
    pub extend_mode: ExtendMode,
    pub start_point: PointKey,
    pub end_point: PointKey,
    pub stretch_size: SizeKey,
    pub tile_spacing: SizeKey,
    pub stops: Vec<GradientStopKey>,
    pub reverse_stops: bool,
    pub nine_patch: Option<Box<NinePatchDescriptor>>,
}

impl Internable for LinearGradient {
    type Key = LinearGradientKey;
    type StoreData = LinearGradientTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_LINEAR_GRADIENTS;
}

impl InternablePrimitive for LinearGradient {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> LinearGradientKey {
        LinearGradientKey::new(info, self)
    }

    fn make_instance_kind(
        _key: LinearGradientKey,
        data_handle: LinearGradientDataHandle,
        prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        let gradient_index = prim_store.linear_gradients.push(LinearGradientPrimitive {
            cache_segments: Vec::new(),
            visible_tiles_range: GradientTileRange::empty(),
        });

        PrimitiveInstanceKind::LinearGradient {
            data_handle,
            gradient_index,
        }
    }
}

impl IsVisible for LinearGradient {
    fn is_visible(&self) -> bool {
        true
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct LinearGradientPrimitive {
    pub cache_segments: Vec<CachedGradientSegment>,
    pub visible_tiles_range: GradientTileRange,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct CachedGradientSegment {
    pub render_task: RenderTaskId,
    pub local_rect: LayoutRect,
}

/// The per-instance shader input of a fast-path linear gradient render task.
///
/// Must match the FAST_LINEAR_GRADIENT instance description in renderer/vertex.rs.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
#[derive(Clone, Debug)]
pub struct FastLinearGradientInstance {
    pub task_rect: DeviceRect,
    pub stops: [f32; GRADIENT_FP_STOPS],
    pub colors: [PremultipliedColorF; GRADIENT_FP_STOPS],
    pub axis_select: f32,
    pub start_stop: [f32; 2],
}

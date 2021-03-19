/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Radial gradients
//!
//! Specification: https://drafts.csswg.org/css-images-4/#radial-gradients
//!
//! Radial gradients are rendered via cached render tasks and composited with the image brush.

use api::{ExtendMode, GradientStop, PremultipliedColorF};
use api::units::*;
use crate::scene_building::IsVisible;
use crate::frame_builder::FrameBuildingState;
use crate::gpu_cache::{GpuCache, GpuCacheHandle};
use crate::intern::{Internable, InternDebug, Handle as InternHandle};
use crate::internal_types::LayoutPrimitiveInfo;
use crate::prim_store::{BrushSegment, GradientTileRange, InternablePrimitive};
use crate::prim_store::{PrimitiveInstanceKind, PrimitiveOpacity};
use crate::prim_store::{PrimKeyCommonData, PrimTemplateCommonData, PrimitiveStore};
use crate::prim_store::{NinePatchDescriptor, PointKey, SizeKey, FloatKey};
use crate::render_task::{RenderTask, RenderTaskKind};
use crate::render_task_graph::RenderTaskId;
use crate::render_task_cache::{RenderTaskCacheKeyKind, RenderTaskCacheKey, RenderTaskParent};
use crate::picture::{SurfaceIndex};

use std::{hash, ops::{Deref, DerefMut}};
use super::{stops_and_min_alpha, GradientStopKey, GradientGpuBlockBuilder};

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
#[derive(Debug)]
pub struct RadialGradientTemplate {
    pub common: PrimTemplateCommonData,
    pub extend_mode: ExtendMode,
    pub params: RadialGradientParams,
    pub center: DevicePoint,
    pub task_size: DeviceIntSize,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub brush_segments: Vec<BrushSegment>,
    pub stops_opacity: PrimitiveOpacity,
    pub stops: Vec<GradientStop>,
    pub stops_handle: GpuCacheHandle,
    pub src_color: Option<RenderTaskId>,
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

        // Avoid rendering enormous gradients. Radial gradients are mostly made of soft transitions,
        // so it is unlikely that rendering at a higher resolution that 1024 would produce noticeable
        // differences, especially with 8 bits per channel.
        const MAX_SIZE: f32 = 1024.0;
        let mut task_size = DeviceSize::new(item.stretch_size.w, item.stretch_size.h);
        let mut center = DevicePoint::new(item.center.x, item.center.y);
        let mut params = item.params;
        if task_size.width > MAX_SIZE {
            let sx = MAX_SIZE / task_size.width;
            task_size.width = MAX_SIZE;
            params.ratio_xy *= sx;
            center.x *= sx;
            params.start_radius *= sx;
            params.end_radius *= sx;
        }
        if task_size.height > MAX_SIZE {
            let sy = MAX_SIZE / task_size.height;
            task_size.height = MAX_SIZE;
            params.ratio_xy /= sy;
            center.y *= sy;
        }

        RadialGradientTemplate {
            common,
            center,
            extend_mode: item.extend_mode,
            params,
            stretch_size: item.stretch_size.into(),
            task_size: task_size.to_i32(),
            tile_spacing: item.tile_spacing.into(),
            brush_segments,
            stops_opacity,
            stops,
            stops_handle: GpuCacheHandle::new(),
            src_color: None,
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
        parent_surface: SurfaceIndex,
    ) {
        if let Some(mut request) =
            frame_state.gpu_cache.request(&mut self.common.gpu_cache_handle) {
            // write_prim_gpu_blocks
            request.push(PremultipliedColorF::WHITE);
            request.push(PremultipliedColorF::WHITE);
            request.push([
                self.stretch_size.width,
                self.stretch_size.height,
                0.0,
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
                false,
                &mut request,
                &self.stops,
            );
        }

        let task_size = self.task_size;
        let cache_key = RadialGradientCacheKey {
            size: task_size,
            center: PointKey { x: self.center.x, y: self.center.y },
            start_radius: FloatKey(self.params.start_radius),
            end_radius: FloatKey(self.params.end_radius),
            ratio_xy: FloatKey(self.params.ratio_xy),
            extend_mode: self.extend_mode,
            stops: self.stops.iter().map(|stop| (*stop).into()).collect(),
        };

        let task_id = frame_state.resource_cache.request_render_task(
            RenderTaskCacheKey {
                size: task_size,
                kind: RenderTaskCacheKeyKind::RadialGradient(cache_key),
            },
            frame_state.gpu_cache,
            frame_state.rg_builder,
            None,
            false,
            RenderTaskParent::Surface(parent_surface),
            frame_state.surfaces,
            |rg_builder| {
                rg_builder.add().init(RenderTask::new_dynamic(
                    task_size,
                    RenderTaskKind::RadialGradient(RadialGradientTask {
                        extend_mode: self.extend_mode,
                        center: self.center,
                        params: self.params.clone(),
                        stops: self.stops_handle,
                    }),
                ))
            }
        );

        self.src_color = Some(task_id);

        // Tile spacing is always handled by decomposing into separate draw calls so the
        // primitive opacity is equivalent to stops opacity. This might change to being
        // set to non-opaque in the presence of tile spacing if/when tile spacing is handled
        // in the same way as with the image primitive.
        self.opacity = self.stops_opacity;
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

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RadialGradientTask {
    pub extend_mode: ExtendMode,
    pub center: DevicePoint,
    pub params: RadialGradientParams,
    pub stops: GpuCacheHandle,
}

impl RadialGradientTask {
    pub fn to_instance(&self, target_rect: &DeviceIntRect, gpu_cache: &mut GpuCache) -> RadialGradientInstance {
        RadialGradientInstance {
            task_rect: target_rect.to_f32(),
            center: self.center,
            start_radius: self.params.start_radius,
            end_radius: self.params.end_radius,
            ratio_xy: self.params.ratio_xy,
            extend_mode: self.extend_mode as i32,
            gradient_stops_address: self.stops.as_int(gpu_cache),
        }
    }
}

/// The per-instance shader input of a radial gradient render task.
///
/// Must match the RADIAL_GRADIENT instance description in renderer/vertex.rs.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[repr(C)]
#[derive(Clone, Debug)]
pub struct RadialGradientInstance {
    pub task_rect: DeviceRect,
    pub center: DevicePoint,
    pub start_radius: f32,
    pub end_radius: f32,
    pub ratio_xy: f32,
    pub extend_mode: i32,
    pub gradient_stops_address: i32,
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RadialGradientCacheKey {
    pub size: DeviceIntSize,
    pub center: PointKey,
    pub start_radius: FloatKey,
    pub end_radius: FloatKey,
    pub ratio_xy: FloatKey,
    pub extend_mode: ExtendMode,
    pub stops: Vec<GradientStopKey>,
}


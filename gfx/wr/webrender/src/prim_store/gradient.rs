/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{
    ColorF, ColorU,ExtendMode, GradientStop, LayoutPoint, LayoutSize,
    LayoutPrimitiveInfo, LayoutRect, PremultipliedColorF
};
use display_list_flattener::{AsInstanceKind, IsVisible};
use frame_builder::FrameBuildingState;
use gpu_cache::{GpuCacheHandle, GpuDataRequest};
use intern::{DataStore, Handle, Internable, Interner, UpdateList};
use prim_store::{BrushSegment, GradientTileRange};
use prim_store::{PrimitiveInstanceKind, PrimitiveOpacity, PrimitiveSceneData};
use prim_store::{PrimKeyCommonData, PrimTemplateCommonData, PrimitiveStore};
use prim_store::{NinePatchDescriptor, PointKey, SizeKey};
use std::{hash, ops::{Deref, DerefMut}, mem};
use util::pack_as_float;

/// A hashable gradient stop that can be used in primitive keys.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, PartialEq)]
pub struct GradientStopKey {
    pub offset: f32,
    pub color: ColorU,
}

impl Eq for GradientStopKey {}

impl hash::Hash for GradientStopKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.offset.to_bits().hash(state);
        self.color.hash(state);
    }
}

/// Identifying key for a line decoration.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
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
        is_backface_visible: bool,
        prim_size: LayoutSize,
        prim_relative_clip_rect: LayoutRect,
        linear_grad: LinearGradient,
    ) -> Self {
        LinearGradientKey {
            common: PrimKeyCommonData {
                is_backface_visible,
                prim_size: prim_size.into(),
                prim_relative_clip_rect: prim_relative_clip_rect.into(),
            },
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

impl AsInstanceKind<LinearGradientDataHandle> for LinearGradientKey {
    /// Construct a primitive instance that matches the type
    /// of primitive key.
    fn as_instance_kind(
        &self,
        data_handle: LinearGradientDataHandle,
        _prim_store: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::LinearGradient {
            data_handle,
            visible_tiles_range: GradientTileRange::empty(),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
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
        let mut min_alpha: f32 = 1.0;

        // Convert the stops to more convenient representation
        // for the current gradient builder.
        let stops = item.stops.iter().map(|stop| {
            let color: ColorF = stop.color.into();
            min_alpha = min_alpha.min(color.a);

            GradientStop {
                offset: stop.offset,
                color,
            }
        }).collect();

        let mut brush_segments = Vec::new();

        if let Some(ref nine_patch) = item.nine_patch {
            brush_segments = nine_patch.create_segments(common.prim_size);
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

        self.opacity = {
            // If the coverage of the gradient extends to or beyond
            // the primitive rect, then the opacity can be determined
            // by the colors of the stops. If we have tiling / spacing
            // then we just assume the gradient is translucent for now.
            // (In the future we could consider segmenting in some cases).
            let stride = self.stretch_size + self.tile_spacing;
            if stride.width >= self.common.prim_size.width &&
               stride.height >= self.common.prim_size.height {
                self.stops_opacity
            } else {
               PrimitiveOpacity::translucent()
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub struct LinearGradientDataMarker;

pub type LinearGradientDataStore = DataStore<LinearGradientKey, LinearGradientTemplate, LinearGradientDataMarker>;
pub type LinearGradientDataHandle = Handle<LinearGradientDataMarker>;
pub type LinearGradientDataUpdateList = UpdateList<LinearGradientKey>;
pub type LinearGradientDataInterner = Interner<LinearGradientKey, PrimitiveSceneData, LinearGradientDataMarker>;

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
    type Marker = LinearGradientDataMarker;
    type Source = LinearGradientKey;
    type StoreData = LinearGradientTemplate;
    type InternData = PrimitiveSceneData;

    /// Build a new key from self with `info`.
    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
        prim_relative_clip_rect: LayoutRect
    ) -> LinearGradientKey {
        LinearGradientKey::new(
            info.is_backface_visible,
            info.rect.size,
            prim_relative_clip_rect,
            self
        )
    }
}

impl IsVisible for LinearGradient {
    fn is_visible(&self) -> bool {
        true
    }
}

////////////////////////////////////////////////////////////////////////////////

/// Hashable radial gradient parameters, for use during prim interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, PartialEq)]
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

/// Identifying key for a line decoration.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
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
        is_backface_visible: bool,
        prim_size: LayoutSize,
        prim_relative_clip_rect: LayoutRect,
        radial_grad: RadialGradient,
    ) -> Self {
        RadialGradientKey {
            common: PrimKeyCommonData {
                is_backface_visible,
                prim_size: prim_size.into(),
                prim_relative_clip_rect: prim_relative_clip_rect.into(),
            },
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

impl AsInstanceKind<RadialGradientDataHandle> for RadialGradientKey {
    /// Construct a primitive instance that matches the type
    /// of primitive key.
    fn as_instance_kind(
        &self,
        data_handle: RadialGradientDataHandle,
        _prim_store: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind {
        PrimitiveInstanceKind::RadialGradient {
            data_handle,
            visible_tiles_range: GradientTileRange::empty(),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RadialGradientTemplate {
    pub common: PrimTemplateCommonData,
    pub extend_mode: ExtendMode,
    pub center: LayoutPoint,
    pub params: RadialGradientParams,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub brush_segments: Vec<BrushSegment>,
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
            brush_segments = nine_patch.create_segments(common.prim_size);
        }

        let stops = item.stops.iter().map(|stop| {
            GradientStop {
                offset: stop.offset,
                color: stop.color.into(),
            }
        }).collect();

        RadialGradientTemplate {
            common,
            center: item.center.into(),
            extend_mode: item.extend_mode,
            params: item.params,
            stretch_size: item.stretch_size.into(),
            tile_spacing: item.tile_spacing.into(),
            brush_segments: brush_segments,
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

        self.opacity = PrimitiveOpacity::translucent();
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub struct RadialGradientDataMarker;

pub type RadialGradientDataStore = DataStore<RadialGradientKey, RadialGradientTemplate, RadialGradientDataMarker>;
pub type RadialGradientDataHandle = Handle<RadialGradientDataMarker>;
pub type RadialGradientDataUpdateList = UpdateList<RadialGradientKey>;
pub type RadialGradientDataInterner = Interner<RadialGradientKey, PrimitiveSceneData, RadialGradientDataMarker>;

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
    type Marker = RadialGradientDataMarker;
    type Source = RadialGradientKey;
    type StoreData = RadialGradientTemplate;
    type InternData = PrimitiveSceneData;

    /// Build a new key from self with `info`.
    fn build_key(
        self,
        info: &LayoutPrimitiveInfo,
        prim_relative_clip_rect: LayoutRect,
    ) -> RadialGradientKey {
        RadialGradientKey::new(
            info.is_backface_visible,
            info.rect.size,
            prim_relative_clip_rect,
            self,
        )
    }
}

impl IsVisible for RadialGradient {
    fn is_visible(&self) -> bool {
        true
    }
}

////////////////////////////////////////////////////////////////////////////////

// The gradient entry index for the first color stop
pub const GRADIENT_DATA_FIRST_STOP: usize = 0;
// The gradient entry index for the last color stop
pub const GRADIENT_DATA_LAST_STOP: usize = GRADIENT_DATA_SIZE - 1;

// The start of the gradient data table
pub const GRADIENT_DATA_TABLE_BEGIN: usize = GRADIENT_DATA_FIRST_STOP + 1;
// The exclusive bound of the gradient data table
pub const GRADIENT_DATA_TABLE_END: usize = GRADIENT_DATA_LAST_STOP;
// The number of entries in the gradient data table.
pub const GRADIENT_DATA_TABLE_SIZE: usize = 128;

// The number of entries in a gradient data: GRADIENT_DATA_TABLE_SIZE + first stop entry + last stop entry
pub const GRADIENT_DATA_SIZE: usize = GRADIENT_DATA_TABLE_SIZE + 2;

#[derive(Debug)]
#[repr(C)]
// An entry in a gradient data table representing a segment of the gradient color space.
pub struct GradientDataEntry {
    pub start_color: PremultipliedColorF,
    pub end_color: PremultipliedColorF,
}

// TODO(gw): Tidy this up to be a free function / module?
struct GradientGpuBlockBuilder {}

impl GradientGpuBlockBuilder {
    /// Generate a color ramp filling the indices in [start_idx, end_idx) and interpolating
    /// from start_color to end_color.
    fn fill_colors(
        start_idx: usize,
        end_idx: usize,
        start_color: &PremultipliedColorF,
        end_color: &PremultipliedColorF,
        entries: &mut [GradientDataEntry; GRADIENT_DATA_SIZE],
    ) {
        // Calculate the color difference for individual steps in the ramp.
        let inv_steps = 1.0 / (end_idx - start_idx) as f32;
        let step_r = (end_color.r - start_color.r) * inv_steps;
        let step_g = (end_color.g - start_color.g) * inv_steps;
        let step_b = (end_color.b - start_color.b) * inv_steps;
        let step_a = (end_color.a - start_color.a) * inv_steps;

        let mut cur_color = *start_color;

        // Walk the ramp writing start and end colors for each entry.
        for index in start_idx .. end_idx {
            let entry = &mut entries[index];
            entry.start_color = cur_color;
            cur_color.r += step_r;
            cur_color.g += step_g;
            cur_color.b += step_b;
            cur_color.a += step_a;
            entry.end_color = cur_color;
        }
    }

    /// Compute an index into the gradient entry table based on a gradient stop offset. This
    /// function maps offsets from [0, 1] to indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END].
    #[inline]
    fn get_index(offset: f32) -> usize {
        (offset.max(0.0).min(1.0) * GRADIENT_DATA_TABLE_SIZE as f32 +
            GRADIENT_DATA_TABLE_BEGIN as f32)
            .round() as usize
    }

    // Build the gradient data from the supplied stops, reversing them if necessary.
    fn build(
        reverse_stops: bool,
        request: &mut GpuDataRequest,
        src_stops: &[GradientStop],
    ) {
        // Preconditions (should be ensured by DisplayListBuilder):
        // * we have at least two stops
        // * first stop has offset 0.0
        // * last stop has offset 1.0
        let mut src_stops = src_stops.into_iter();
        let mut cur_color = match src_stops.next() {
            Some(stop) => {
                debug_assert_eq!(stop.offset, 0.0);
                stop.color.premultiplied()
            }
            None => {
                error!("Zero gradient stops found!");
                PremultipliedColorF::BLACK
            }
        };

        // A table of gradient entries, with two colors per entry, that specify the start and end color
        // within the segment of the gradient space represented by that entry. To lookup a gradient result,
        // first the entry index is calculated to determine which two colors to interpolate between, then
        // the offset within that entry bucket is used to interpolate between the two colors in that entry.
        // This layout preserves hard stops, as the end color for a given entry can differ from the start
        // color for the following entry, despite them being adjacent. Colors are stored within in BGRA8
        // format for texture upload. This table requires the gradient color stops to be normalized to the
        // range [0, 1]. The first and last entries hold the first and last color stop colors respectively,
        // while the entries in between hold the interpolated color stop values for the range [0, 1].
        let mut entries: [GradientDataEntry; GRADIENT_DATA_SIZE] = unsafe { mem::uninitialized() };

        if reverse_stops {
            // Fill in the first entry (for reversed stops) with the first color stop
            GradientGpuBlockBuilder::fill_colors(
                GRADIENT_DATA_LAST_STOP,
                GRADIENT_DATA_LAST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );

            // Fill in the center of the gradient table, generating a color ramp between each consecutive pair
            // of gradient stops. Each iteration of a loop will fill the indices in [next_idx, cur_idx). The
            // loop will then fill indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END).
            let mut cur_idx = GRADIENT_DATA_TABLE_END;
            for next in src_stops {
                let next_color = next.color.premultiplied();
                let next_idx = Self::get_index(1.0 - next.offset);

                if next_idx < cur_idx {
                    GradientGpuBlockBuilder::fill_colors(
                        next_idx,
                        cur_idx,
                        &next_color,
                        &cur_color,
                        &mut entries,
                    );
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            if cur_idx != GRADIENT_DATA_TABLE_BEGIN {
                error!("Gradient stops abruptly at {}, auto-completing to white", cur_idx);
                GradientGpuBlockBuilder::fill_colors(
                    GRADIENT_DATA_TABLE_BEGIN,
                    cur_idx,
                    &PremultipliedColorF::WHITE,
                    &cur_color,
                    &mut entries,
                );
            }

            // Fill in the last entry (for reversed stops) with the last color stop
            GradientGpuBlockBuilder::fill_colors(
                GRADIENT_DATA_FIRST_STOP,
                GRADIENT_DATA_FIRST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );
        } else {
            // Fill in the first entry with the first color stop
            GradientGpuBlockBuilder::fill_colors(
                GRADIENT_DATA_FIRST_STOP,
                GRADIENT_DATA_FIRST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );

            // Fill in the center of the gradient table, generating a color ramp between each consecutive pair
            // of gradient stops. Each iteration of a loop will fill the indices in [cur_idx, next_idx). The
            // loop will then fill indices in [GRADIENT_DATA_TABLE_BEGIN, GRADIENT_DATA_TABLE_END).
            let mut cur_idx = GRADIENT_DATA_TABLE_BEGIN;
            for next in src_stops {
                let next_color = next.color.premultiplied();
                let next_idx = Self::get_index(next.offset);

                if next_idx > cur_idx {
                    GradientGpuBlockBuilder::fill_colors(
                        cur_idx,
                        next_idx,
                        &cur_color,
                        &next_color,
                        &mut entries,
                    );
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            if cur_idx != GRADIENT_DATA_TABLE_END {
                error!("Gradient stops abruptly at {}, auto-completing to white", cur_idx);
                GradientGpuBlockBuilder::fill_colors(
                    cur_idx,
                    GRADIENT_DATA_TABLE_END,
                    &PremultipliedColorF::WHITE,
                    &cur_color,
                    &mut entries,
                );
            }

            // Fill in the last entry with the last color stop
            GradientGpuBlockBuilder::fill_colors(
                GRADIENT_DATA_LAST_STOP,
                GRADIENT_DATA_LAST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );
        }

        for entry in entries.iter() {
            request.push(entry.start_color);
            request.push(entry.end_color);
        }
    }
}

#[test]
#[cfg(target_os = "linux")]
fn test_struct_sizes() {
    use std::mem;
    // The sizes of these structures are critical for performance on a number of
    // talos stress tests. If you get a failure here on CI, there's two possibilities:
    // (a) You made a structure smaller than it currently is. Great work! Update the
    //     test expectations and move on.
    // (b) You made a structure larger. This is not necessarily a problem, but should only
    //     be done with care, and after checking if talos performance regresses badly.
    assert_eq!(mem::size_of::<LinearGradient>(), 72, "LinearGradient size changed");
    assert_eq!(mem::size_of::<LinearGradientTemplate>(), 168, "LinearGradientTemplate size changed");
    assert_eq!(mem::size_of::<LinearGradientKey>(), 96, "LinearGradientKey size changed");

    assert_eq!(mem::size_of::<RadialGradient>(), 72, "RadialGradient size changed");
    assert_eq!(mem::size_of::<RadialGradientTemplate>(), 168, "RadialGradientTemplate size changed");
    assert_eq!(mem::size_of::<RadialGradientKey>(), 104, "RadialGradientKey size changed");
}

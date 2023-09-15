/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipMode, ColorF, ColorU, RasterSpace};
use api::{ImageRendering, RepeatMode, PrimitiveFlags};
use api::{PremultipliedColorF, PropertyBinding, Shadow};
use api::{PrimitiveKeyKind, FillRule, POLYGON_CLIP_VERTEX_MAX};
use api::units::*;
use euclid::{SideOffsets2D, Size2D};
use malloc_size_of::MallocSizeOf;
use crate::composite::CompositorSurfaceKind;
use crate::clip::ClipLeafId;
use crate::segment::EdgeAaSegmentMask;
use crate::border::BorderSegmentCacheKey;
use crate::debug_item::{DebugItem, DebugMessage};
use crate::debug_colors;
use crate::scene_building::{CreateShadow, IsVisible};
use crate::frame_builder::FrameBuildingState;
use glyph_rasterizer::GlyphKey;
use crate::gpu_cache::{GpuCacheAddress, GpuCacheHandle, GpuDataRequest};
use crate::gpu_types::{BrushFlags, QuadSegment};
use crate::intern;
use crate::picture::PicturePrimitive;
use crate::render_task_graph::RenderTaskId;
use crate::resource_cache::ImageProperties;
use crate::scene::SceneProperties;
use std::{hash, ops, u32, usize};
use crate::util::Recycler;
use crate::internal_types::{FastHashSet, LayoutPrimitiveInfo};
use crate::visibility::PrimitiveVisibility;

pub mod backdrop;
pub mod borders;
pub mod gradient;
pub mod image;
pub mod line_dec;
pub mod picture;
pub mod text_run;
pub mod interned;

mod storage;

use backdrop::{BackdropCaptureDataHandle, BackdropRenderDataHandle};
use borders::{ImageBorderDataHandle, NormalBorderDataHandle};
use gradient::{LinearGradientPrimitive, LinearGradientDataHandle, RadialGradientDataHandle, ConicGradientDataHandle};
use image::{ImageDataHandle, ImageInstance, YuvImageDataHandle};
use line_dec::LineDecorationDataHandle;
use picture::PictureDataHandle;
use text_run::{TextRunDataHandle, TextRunPrimitive};

pub const VECS_PER_SEGMENT: usize = 2;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, MallocSizeOf)]
pub struct PrimitiveOpacity {
    pub is_opaque: bool,
}

impl PrimitiveOpacity {
    pub fn opaque() -> PrimitiveOpacity {
        PrimitiveOpacity { is_opaque: true }
    }

    pub fn translucent() -> PrimitiveOpacity {
        PrimitiveOpacity { is_opaque: false }
    }

    pub fn from_alpha(alpha: f32) -> PrimitiveOpacity {
        PrimitiveOpacity {
            is_opaque: alpha >= 1.0,
        }
    }
}

/// For external images, it's not possible to know the
/// UV coords of the image (or the image data itself)
/// until the render thread receives the frame and issues
/// callbacks to the client application. For external
/// images that are visible, a DeferredResolve is created
/// that is stored in the frame. This allows the render
/// thread to iterate this list and update any changed
/// texture data and update the UV rect. Any filtering
/// is handled externally for NativeTexture external
/// images.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct DeferredResolve {
    pub address: GpuCacheAddress,
    pub image_properties: ImageProperties,
    pub rendering: ImageRendering,
}

#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct ClipTaskIndex(pub u32);

impl ClipTaskIndex {
    pub const INVALID: ClipTaskIndex = ClipTaskIndex(0);
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, MallocSizeOf, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureIndex(pub usize);

impl PictureIndex {
    pub const INVALID: PictureIndex = PictureIndex(!0);
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Debug, Clone, MallocSizeOf, PartialEq)]
pub struct RectangleKey {
    pub x0: f32,
    pub y0: f32,
    pub x1: f32,
    pub y1: f32,
}

impl RectangleKey {
    pub fn intersects(&self, other: &Self) -> bool {
        self.x0 < other.x1
            && other.x0 < self.x1
            && self.y0 < other.y1
            && other.y0 < self.y1
    }
}

impl Eq for RectangleKey {}

impl hash::Hash for RectangleKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.x0.to_bits().hash(state);
        self.y0.to_bits().hash(state);
        self.x1.to_bits().hash(state);
        self.y1.to_bits().hash(state);
    }
}

impl From<RectangleKey> for LayoutRect {
    fn from(key: RectangleKey) -> LayoutRect {
        LayoutRect {
            min: LayoutPoint::new(key.x0, key.y0),
            max: LayoutPoint::new(key.x1, key.y1),
        }
    }
}

impl From<RectangleKey> for WorldRect {
    fn from(key: RectangleKey) -> WorldRect {
        WorldRect {
            min: WorldPoint::new(key.x0, key.y0),
            max: WorldPoint::new(key.x1, key.y1),
        }
    }
}

impl From<LayoutRect> for RectangleKey {
    fn from(rect: LayoutRect) -> RectangleKey {
        RectangleKey {
            x0: rect.min.x,
            y0: rect.min.y,
            x1: rect.max.x,
            y1: rect.max.y,
        }
    }
}

impl From<PictureRect> for RectangleKey {
    fn from(rect: PictureRect) -> RectangleKey {
        RectangleKey {
            x0: rect.min.x,
            y0: rect.min.y,
            x1: rect.max.x,
            y1: rect.max.y,
        }
    }
}

impl From<WorldRect> for RectangleKey {
    fn from(rect: WorldRect) -> RectangleKey {
        RectangleKey {
            x0: rect.min.x,
            y0: rect.min.y,
            x1: rect.max.x,
            y1: rect.max.y,
        }
    }
}

/// To create a fixed-size representation of a polygon, we use a fixed
/// number of points. Our initialization method restricts us to values
/// <= 32. If our constant POLYGON_CLIP_VERTEX_MAX is > 32, the Rust
/// compiler will complain.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Debug, Clone, Hash, MallocSizeOf, PartialEq)]
pub struct PolygonKey {
    pub point_count: u8,
    pub points: [PointKey; POLYGON_CLIP_VERTEX_MAX],
    pub fill_rule: FillRule,
}

impl PolygonKey {
    pub fn new(
        points_layout: &Vec<LayoutPoint>,
        fill_rule: FillRule,
    ) -> Self {
        // We have to fill fixed-size arrays with data from a Vec.
        // We'll do this by initializing the arrays to known-good
        // values then overwriting those values as long as our
        // iterator provides values.
        let mut points: [PointKey; POLYGON_CLIP_VERTEX_MAX] = [PointKey { x: 0.0, y: 0.0}; POLYGON_CLIP_VERTEX_MAX];

        let mut point_count: u8 = 0;
        for (src, dest) in points_layout.iter().zip(points.iter_mut()) {
            *dest = (*src as LayoutPoint).into();
            point_count = point_count + 1;
        }

        PolygonKey {
            point_count,
            points,
            fill_rule,
        }
    }
}

impl Eq for PolygonKey {}

/// A hashable SideOffset2D that can be used in primitive keys.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq)]
pub struct SideOffsetsKey {
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub left: f32,
}

impl Eq for SideOffsetsKey {}

impl hash::Hash for SideOffsetsKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.top.to_bits().hash(state);
        self.right.to_bits().hash(state);
        self.bottom.to_bits().hash(state);
        self.left.to_bits().hash(state);
    }
}

impl From<SideOffsetsKey> for LayoutSideOffsets {
    fn from(key: SideOffsetsKey) -> LayoutSideOffsets {
        LayoutSideOffsets::new(
            key.top,
            key.right,
            key.bottom,
            key.left,
        )
    }
}

impl<U> From<SideOffsets2D<f32, U>> for SideOffsetsKey {
    fn from(offsets: SideOffsets2D<f32, U>) -> SideOffsetsKey {
        SideOffsetsKey {
            top: offsets.top,
            right: offsets.right,
            bottom: offsets.bottom,
            left: offsets.left,
        }
    }
}

/// A hashable size for using as a key during primitive interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Debug, Clone, MallocSizeOf, PartialEq)]
pub struct SizeKey {
    w: f32,
    h: f32,
}

impl Eq for SizeKey {}

impl hash::Hash for SizeKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.w.to_bits().hash(state);
        self.h.to_bits().hash(state);
    }
}

impl From<SizeKey> for LayoutSize {
    fn from(key: SizeKey) -> LayoutSize {
        LayoutSize::new(key.w, key.h)
    }
}

impl<U> From<Size2D<f32, U>> for SizeKey {
    fn from(size: Size2D<f32, U>) -> SizeKey {
        SizeKey {
            w: size.width,
            h: size.height,
        }
    }
}

/// A hashable vec for using as a key during primitive interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Debug, Clone, MallocSizeOf, PartialEq)]
pub struct VectorKey {
    pub x: f32,
    pub y: f32,
}

impl Eq for VectorKey {}

impl hash::Hash for VectorKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.x.to_bits().hash(state);
        self.y.to_bits().hash(state);
    }
}

impl From<VectorKey> for LayoutVector2D {
    fn from(key: VectorKey) -> LayoutVector2D {
        LayoutVector2D::new(key.x, key.y)
    }
}

impl From<VectorKey> for WorldVector2D {
    fn from(key: VectorKey) -> WorldVector2D {
        WorldVector2D::new(key.x, key.y)
    }
}

impl From<LayoutVector2D> for VectorKey {
    fn from(vec: LayoutVector2D) -> VectorKey {
        VectorKey {
            x: vec.x,
            y: vec.y,
        }
    }
}

impl From<WorldVector2D> for VectorKey {
    fn from(vec: WorldVector2D) -> VectorKey {
        VectorKey {
            x: vec.x,
            y: vec.y,
        }
    }
}

/// A hashable point for using as a key during primitive interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, MallocSizeOf, PartialEq)]
pub struct PointKey {
    pub x: f32,
    pub y: f32,
}

impl Eq for PointKey {}

impl hash::Hash for PointKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.x.to_bits().hash(state);
        self.y.to_bits().hash(state);
    }
}

impl From<PointKey> for LayoutPoint {
    fn from(key: PointKey) -> LayoutPoint {
        LayoutPoint::new(key.x, key.y)
    }
}

impl From<LayoutPoint> for PointKey {
    fn from(p: LayoutPoint) -> PointKey {
        PointKey {
            x: p.x,
            y: p.y,
        }
    }
}

impl From<PicturePoint> for PointKey {
    fn from(p: PicturePoint) -> PointKey {
        PointKey {
            x: p.x,
            y: p.y,
        }
    }
}

impl From<WorldPoint> for PointKey {
    fn from(p: WorldPoint) -> PointKey {
        PointKey {
            x: p.x,
            y: p.y,
        }
    }
}

/// A hashable float for using as a key during primitive interning.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone, MallocSizeOf, PartialEq)]
pub struct FloatKey(f32);

impl Eq for FloatKey {}

impl hash::Hash for FloatKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.0.to_bits().hash(state);
    }
}


#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct PrimKeyCommonData {
    pub flags: PrimitiveFlags,
    pub prim_rect: RectangleKey,
}

impl From<&LayoutPrimitiveInfo> for PrimKeyCommonData {
    fn from(info: &LayoutPrimitiveInfo) -> Self {
        PrimKeyCommonData {
            flags: info.flags,
            prim_rect: info.rect.into(),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct PrimKey<T: MallocSizeOf> {
    pub common: PrimKeyCommonData,
    pub kind: T,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct PrimitiveKey {
    pub common: PrimKeyCommonData,
    pub kind: PrimitiveKeyKind,
}

impl PrimitiveKey {
    pub fn new(
        info: &LayoutPrimitiveInfo,
        kind: PrimitiveKeyKind,
    ) -> Self {
        PrimitiveKey {
            common: info.into(),
            kind,
        }
    }
}

impl intern::InternDebug for PrimitiveKey {}

/// The shared information for a given primitive. This is interned and retained
/// both across frames and display lists, by comparing the matching PrimitiveKey.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub enum PrimitiveTemplateKind {
    Rectangle {
        color: PropertyBinding<ColorF>,
    },
    Clear,
}

impl PrimitiveTemplateKind {
    /// Write any GPU blocks for the primitive template to the given request object.
    pub fn write_prim_gpu_blocks(
        &self,
        request: &mut GpuDataRequest,
        scene_properties: &SceneProperties,
    ) {
        match *self {
            PrimitiveTemplateKind::Clear => {
                // Opaque black with operator dest out
                request.push(PremultipliedColorF::BLACK);
            }
            PrimitiveTemplateKind::Rectangle { ref color, .. } => {
                request.push(scene_properties.resolve_color(color).premultiplied())
            }
        }
    }
}

/// Construct the primitive template data from a primitive key. This
/// is invoked when a primitive key is created and the interner
/// doesn't currently contain a primitive with this key.
impl From<PrimitiveKeyKind> for PrimitiveTemplateKind {
    fn from(kind: PrimitiveKeyKind) -> Self {
        match kind {
            PrimitiveKeyKind::Clear => {
                PrimitiveTemplateKind::Clear
            }
            PrimitiveKeyKind::Rectangle { color, .. } => {
                PrimitiveTemplateKind::Rectangle {
                    color: color.into(),
                }
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
#[derive(Debug)]
pub struct PrimTemplateCommonData {
    pub flags: PrimitiveFlags,
    pub may_need_repetition: bool,
    pub prim_rect: LayoutRect,
    pub opacity: PrimitiveOpacity,
    /// The GPU cache handle for a primitive template. Since this structure
    /// is retained across display lists by interning, this GPU cache handle
    /// also remains valid, which reduces the number of updates to the GPU
    /// cache when a new display list is processed.
    pub gpu_cache_handle: GpuCacheHandle,
    /// Specifies the edges that are *allowed* to have anti-aliasing.
    /// In other words EdgeAaSegmentFlags::all() does not necessarily mean all edges will
    /// be anti-aliased, only that they could be.
    ///
    /// Use this to force disable anti-alasing on edges of the primitives.
    pub edge_aa_mask: EdgeAaSegmentMask,
}

impl PrimTemplateCommonData {
    pub fn with_key_common(common: PrimKeyCommonData) -> Self {
        PrimTemplateCommonData {
            flags: common.flags,
            may_need_repetition: true,
            prim_rect: common.prim_rect.into(),
            gpu_cache_handle: GpuCacheHandle::new(),
            opacity: PrimitiveOpacity::translucent(),
            edge_aa_mask: EdgeAaSegmentMask::all(),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PrimTemplate<T> {
    pub common: PrimTemplateCommonData,
    pub kind: T,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PrimitiveTemplate {
    pub common: PrimTemplateCommonData,
    pub kind: PrimitiveTemplateKind,
}

impl ops::Deref for PrimitiveTemplate {
    type Target = PrimTemplateCommonData;
    fn deref(&self) -> &Self::Target {
        &self.common
    }
}

impl ops::DerefMut for PrimitiveTemplate {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.common
    }
}

impl From<PrimitiveKey> for PrimitiveTemplate {
    fn from(item: PrimitiveKey) -> Self {
        PrimitiveTemplate {
            common: PrimTemplateCommonData::with_key_common(item.common),
            kind: item.kind.into(),
        }
    }
}

impl PrimitiveTemplate {
    /// Update the GPU cache for a given primitive template. This may be called multiple
    /// times per frame, by each primitive reference that refers to this interned
    /// template. The initial request call to the GPU cache ensures that work is only
    /// done if the cache entry is invalid (due to first use or eviction).
    pub fn update(
        &mut self,
        frame_state: &mut FrameBuildingState,
        scene_properties: &SceneProperties,
    ) {
        if let Some(mut request) = frame_state.gpu_cache.request(&mut self.common.gpu_cache_handle) {
            self.kind.write_prim_gpu_blocks(&mut request, scene_properties);
        }

        self.opacity = match self.kind {
            PrimitiveTemplateKind::Clear => {
                PrimitiveOpacity::translucent()
            }
            PrimitiveTemplateKind::Rectangle { ref color, .. } => {
                PrimitiveOpacity::from_alpha(scene_properties.resolve_color(color).a)
            }
        };
    }
}

type PrimitiveDataHandle = intern::Handle<PrimitiveKeyKind>;

impl intern::Internable for PrimitiveKeyKind {
    type Key = PrimitiveKey;
    type StoreData = PrimitiveTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_PRIMITIVES;
}

impl InternablePrimitive for PrimitiveKeyKind {
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> PrimitiveKey {
        PrimitiveKey::new(info, self)
    }

    fn make_instance_kind(
        key: PrimitiveKey,
        data_handle: PrimitiveDataHandle,
        prim_store: &mut PrimitiveStore,
        _reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind {
        match key.kind {
            PrimitiveKeyKind::Clear => {
                PrimitiveInstanceKind::Clear {
                    data_handle
                }
            }
            PrimitiveKeyKind::Rectangle { color, .. } => {
                let color_binding_index = match color {
                    PropertyBinding::Binding(..) => {
                        prim_store.color_bindings.push(color)
                    }
                    PropertyBinding::Value(..) => ColorBindingIndex::INVALID,
                };
                PrimitiveInstanceKind::Rectangle {
                    data_handle,
                    segment_instance_index: SegmentInstanceIndex::INVALID,
                    color_binding_index,
                    use_legacy_path: false,
                }
            }
        }
    }
}

#[derive(Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct VisibleMaskImageTile {
    pub tile_offset: TileOffset,
    pub tile_rect: LayoutRect,
    pub task_id: RenderTaskId,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct VisibleGradientTile {
    pub handle: GpuCacheHandle,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

/// Information about how to cache a border segment,
/// along with the current render task cache entry.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, MallocSizeOf)]
pub struct BorderSegmentInfo {
    pub local_task_size: LayoutSize,
    pub cache_key: BorderSegmentCacheKey,
}

/// Represents the visibility state of a segment (wrt clip masks).
#[cfg_attr(feature = "capture", derive(Serialize))]
#[derive(Debug, Clone)]
pub enum ClipMaskKind {
    /// The segment has a clip mask, specified by the render task.
    Mask(RenderTaskId),
    /// The segment has no clip mask.
    None,
    /// The segment is made invisible / clipped completely.
    Clipped,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf)]
pub struct BrushSegment {
    pub local_rect: LayoutRect,
    pub may_need_clip_mask: bool,
    pub edge_flags: EdgeAaSegmentMask,
    pub extra_data: [f32; 4],
    pub brush_flags: BrushFlags,
}

impl BrushSegment {
    pub fn new(
        local_rect: LayoutRect,
        may_need_clip_mask: bool,
        edge_flags: EdgeAaSegmentMask,
        extra_data: [f32; 4],
        brush_flags: BrushFlags,
    ) -> Self {
        Self {
            local_rect,
            may_need_clip_mask,
            edge_flags,
            extra_data,
            brush_flags,
        }
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct ClipRect {
    rect: LayoutRect,
    mode: f32,
}

#[derive(Debug, Clone)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct ClipCorner {
    rect: LayoutRect,
    outer_radius_x: f32,
    outer_radius_y: f32,
    inner_radius_x: f32,
    inner_radius_y: f32,
}

impl ClipCorner {
    fn uniform(rect: LayoutRect, outer_radius: f32, inner_radius: f32) -> ClipCorner {
        ClipCorner {
            rect,
            outer_radius_x: outer_radius,
            outer_radius_y: outer_radius,
            inner_radius_x: inner_radius,
            inner_radius_y: inner_radius,
        }
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ClipData {
    rect: ClipRect,
    top_left: ClipCorner,
    top_right: ClipCorner,
    bottom_left: ClipCorner,
    bottom_right: ClipCorner,
}

impl ClipData {
    pub fn rounded_rect(size: LayoutSize, radii: &BorderRadius, mode: ClipMode) -> ClipData {
        // TODO(gw): For simplicity, keep most of the clip GPU structs the
        //           same as they were, even though the origin is now always
        //           zero, since they are in the clip's local space. In future,
        //           we could reduce the GPU cache size of ClipData.
        let rect = LayoutRect::from_size(size);

        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner {
                rect: LayoutRect::from_origin_and_size(
                    LayoutPoint::new(rect.min.x, rect.min.y),
                    LayoutSize::new(radii.top_left.width, radii.top_left.height),
                ),
                outer_radius_x: radii.top_left.width,
                outer_radius_y: radii.top_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            top_right: ClipCorner {
                rect: LayoutRect::from_origin_and_size(
                    LayoutPoint::new(
                        rect.max.x - radii.top_right.width,
                        rect.min.y,
                    ),
                    LayoutSize::new(radii.top_right.width, radii.top_right.height),
                ),
                outer_radius_x: radii.top_right.width,
                outer_radius_y: radii.top_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_left: ClipCorner {
                rect: LayoutRect::from_origin_and_size(
                    LayoutPoint::new(
                        rect.min.x,
                        rect.max.y - radii.bottom_left.height,
                    ),
                    LayoutSize::new(radii.bottom_left.width, radii.bottom_left.height),
                ),
                outer_radius_x: radii.bottom_left.width,
                outer_radius_y: radii.bottom_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_right: ClipCorner {
                rect: LayoutRect::from_origin_and_size(
                    LayoutPoint::new(
                        rect.max.x - radii.bottom_right.width,
                        rect.max.y - radii.bottom_right.height,
                    ),
                    LayoutSize::new(radii.bottom_right.width, radii.bottom_right.height),
                ),
                outer_radius_x: radii.bottom_right.width,
                outer_radius_y: radii.bottom_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
        }
    }

    pub fn uniform(size: LayoutSize, radius: f32, mode: ClipMode) -> ClipData {
        // TODO(gw): For simplicity, keep most of the clip GPU structs the
        //           same as they were, even though the origin is now always
        //           zero, since they are in the clip's local space. In future,
        //           we could reduce the GPU cache size of ClipData.
        let rect = LayoutRect::from_size(size);

        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner::uniform(
                LayoutRect::from_origin_and_size(
                    LayoutPoint::new(rect.min.x, rect.min.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            top_right: ClipCorner::uniform(
                LayoutRect::from_origin_and_size(
                    LayoutPoint::new(rect.max.x - radius, rect.min.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_left: ClipCorner::uniform(
                LayoutRect::from_origin_and_size(
                    LayoutPoint::new(rect.min.x, rect.max.y - radius),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_right: ClipCorner::uniform(
                LayoutRect::from_origin_and_size(
                    LayoutPoint::new(
                        rect.max.x - radius,
                        rect.max.y - radius,
                    ),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
        }
    }
}

/// A hashable descriptor for nine-patches, used by image and
/// gradient borders.
#[derive(Debug, Clone, PartialEq, Eq, Hash, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct NinePatchDescriptor {
    pub width: i32,
    pub height: i32,
    pub slice: DeviceIntSideOffsets,
    pub fill: bool,
    pub repeat_horizontal: RepeatMode,
    pub repeat_vertical: RepeatMode,
    pub widths: SideOffsetsKey,
}

impl IsVisible for PrimitiveKeyKind {
    // Return true if the primary primitive is visible.
    // Used to trivially reject non-visible primitives.
    // TODO(gw): Currently, primitives other than those
    //           listed here are handled before the
    //           add_primitive() call. In the future
    //           we should move the logic for all other
    //           primitive types to use this.
    fn is_visible(&self) -> bool {
        match *self {
            PrimitiveKeyKind::Clear => {
                true
            }
            PrimitiveKeyKind::Rectangle { ref color, .. } => {
                match *color {
                    PropertyBinding::Value(value) => value.a > 0,
                    PropertyBinding::Binding(..) => true,
                }
            }
        }
    }
}

impl CreateShadow for PrimitiveKeyKind {
    // Create a clone of this PrimitiveContainer, applying whatever
    // changes are necessary to the primitive to support rendering
    // it as part of the supplied shadow.
    fn create_shadow(
        &self,
        shadow: &Shadow,
        _: bool,
        _: RasterSpace,
    ) -> PrimitiveKeyKind {
        match *self {
            PrimitiveKeyKind::Rectangle { .. } => {
                PrimitiveKeyKind::Rectangle {
                    color: PropertyBinding::Value(shadow.color.into()),
                }
            }
            PrimitiveKeyKind::Clear => {
                panic!("bug: this prim is not supported in shadow contexts");
            }
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub enum PrimitiveInstanceKind {
    /// Direct reference to a Picture
    Picture {
        /// Handle to the common interned data for this primitive.
        data_handle: PictureDataHandle,
        pic_index: PictureIndex,
    },
    /// A run of glyphs, with associated font parameters.
    TextRun {
        /// Handle to the common interned data for this primitive.
        data_handle: TextRunDataHandle,
        /// Index to the per instance scratch data for this primitive.
        run_index: TextRunIndex,
    },
    /// A line decoration. cache_handle refers to a cached render
    /// task handle, if this line decoration is not a simple solid.
    LineDecoration {
        /// Handle to the common interned data for this primitive.
        data_handle: LineDecorationDataHandle,
        // TODO(gw): For now, we need to store some information in
        //           the primitive instance that is created during
        //           prepare_prims and read during the batching pass.
        //           Once we unify the prepare_prims and batching to
        //           occur at the same time, we can remove most of
        //           the things we store here in the instance, and
        //           use them directly. This will remove cache_handle,
        //           but also the opacity, clip_task_id etc below.
        render_task: Option<RenderTaskId>,
    },
    NormalBorder {
        /// Handle to the common interned data for this primitive.
        data_handle: NormalBorderDataHandle,
        render_task_ids: storage::Range<RenderTaskId>,
    },
    ImageBorder {
        /// Handle to the common interned data for this primitive.
        data_handle: ImageBorderDataHandle,
    },
    Rectangle {
        /// Handle to the common interned data for this primitive.
        data_handle: PrimitiveDataHandle,
        segment_instance_index: SegmentInstanceIndex,
        color_binding_index: ColorBindingIndex,
        use_legacy_path: bool,
    },
    YuvImage {
        /// Handle to the common interned data for this primitive.
        data_handle: YuvImageDataHandle,
        segment_instance_index: SegmentInstanceIndex,
        compositor_surface_kind: CompositorSurfaceKind,
    },
    Image {
        /// Handle to the common interned data for this primitive.
        data_handle: ImageDataHandle,
        image_instance_index: ImageInstanceIndex,
        compositor_surface_kind: CompositorSurfaceKind,
    },
    /// Always rendered directly into the picture. This tends to be
    /// faster with SWGL.
    LinearGradient {
        /// Handle to the common interned data for this primitive.
        data_handle: LinearGradientDataHandle,
        visible_tiles_range: GradientTileRange,
    },
    /// Always rendered via a cached render task. Usually faster with
    /// a GPU.
    CachedLinearGradient {
        /// Handle to the common interned data for this primitive.
        data_handle: LinearGradientDataHandle,
        visible_tiles_range: GradientTileRange,
    },
    RadialGradient {
        /// Handle to the common interned data for this primitive.
        data_handle: RadialGradientDataHandle,
        visible_tiles_range: GradientTileRange,
    },
    ConicGradient {
        /// Handle to the common interned data for this primitive.
        data_handle: ConicGradientDataHandle,
        visible_tiles_range: GradientTileRange,
    },
    /// Clear out a rect, used for special effects.
    Clear {
        /// Handle to the common interned data for this primitive.
        data_handle: PrimitiveDataHandle,
    },
    /// Render a portion of a specified backdrop.
    BackdropCapture {
        data_handle: BackdropCaptureDataHandle,
    },
    BackdropRender {
        data_handle: BackdropRenderDataHandle,
        pic_index: PictureIndex,
    },
}

impl PrimitiveInstanceKind {
    pub fn as_pic(&self) -> PictureIndex {
        match self {
            PrimitiveInstanceKind::Picture { pic_index, .. } => *pic_index,
            _ => panic!("bug: as_pic called on a prim that is not a picture"),
        }
    }
}

#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveInstanceIndex(pub u32);

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PrimitiveInstance {
    /// Identifies the kind of primitive this
    /// instance is, and references to where
    /// the relevant information for the primitive
    /// can be found.
    pub kind: PrimitiveInstanceKind,

    /// All information and state related to clip(s) for this primitive
    pub clip_leaf_id: ClipLeafId,

    /// Information related to the current visibility state of this
    /// primitive.
    // TODO(gw): Currently built each frame, but can be retained.
    pub vis: PrimitiveVisibility,
}

impl PrimitiveInstance {
    pub fn new(
        kind: PrimitiveInstanceKind,
        clip_leaf_id: ClipLeafId,
    ) -> Self {
        PrimitiveInstance {
            kind,
            vis: PrimitiveVisibility::new(),
            clip_leaf_id,
        }
    }

    // Reset any pre-frame state for this primitive.
    pub fn reset(&mut self) {
        self.vis.reset();
    }

    pub fn clear_visibility(&mut self) {
        self.vis.reset();
    }

    pub fn uid(&self) -> intern::ItemUid {
        match &self.kind {
            PrimitiveInstanceKind::Clear { data_handle, .. } |
            PrimitiveInstanceKind::Rectangle { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::Image { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::LineDecoration { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::LinearGradient { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::CachedLinearGradient { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::NormalBorder { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::Picture { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::RadialGradient { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::ConicGradient { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::TextRun { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::BackdropCapture { data_handle, .. } => {
                data_handle.uid()
            }
            PrimitiveInstanceKind::BackdropRender { data_handle, .. } => {
                data_handle.uid()
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[derive(Debug)]
pub struct SegmentedInstance {
    pub gpu_cache_handle: GpuCacheHandle,
    pub segments_range: SegmentsRange,
}

pub type GlyphKeyStorage = storage::Storage<GlyphKey>;
pub type TextRunIndex = storage::Index<TextRunPrimitive>;
pub type TextRunStorage = storage::Storage<TextRunPrimitive>;
pub type ColorBindingIndex = storage::Index<PropertyBinding<ColorU>>;
pub type ColorBindingStorage = storage::Storage<PropertyBinding<ColorU>>;
pub type BorderHandleStorage = storage::Storage<RenderTaskId>;
pub type SegmentStorage = storage::Storage<BrushSegment>;
pub type SegmentsRange = storage::Range<BrushSegment>;
pub type SegmentInstanceStorage = storage::Storage<SegmentedInstance>;
pub type SegmentInstanceIndex = storage::Index<SegmentedInstance>;
pub type ImageInstanceStorage = storage::Storage<ImageInstance>;
pub type ImageInstanceIndex = storage::Index<ImageInstance>;
pub type GradientTileStorage = storage::Storage<VisibleGradientTile>;
pub type GradientTileRange = storage::Range<VisibleGradientTile>;
pub type LinearGradientStorage = storage::Storage<LinearGradientPrimitive>;

/// Contains various vecs of data that is used only during frame building,
/// where we want to recycle the memory each new display list, to avoid constantly
/// re-allocating and moving memory around. Written during primitive preparation,
/// and read during batching.
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PrimitiveScratchBuffer {
    /// Contains a list of clip mask instance parameters
    /// per segment generated.
    pub clip_mask_instances: Vec<ClipMaskKind>,

    /// List of glyphs keys that are allocated by each
    /// text run instance.
    pub glyph_keys: GlyphKeyStorage,

    /// List of render task handles for border segment instances
    /// that have been added this frame.
    pub border_cache_handles: BorderHandleStorage,

    /// A list of brush segments that have been built for this scene.
    pub segments: SegmentStorage,

    /// A list of segment ranges and GPU cache handles for prim instances
    /// that have opted into segment building. In future, this should be
    /// removed in favor of segment building during primitive interning.
    pub segment_instances: SegmentInstanceStorage,

    /// A list of visible tiles that tiled gradients use to store
    /// per-tile information.
    pub gradient_tiles: GradientTileStorage,

    /// List of debug display items for rendering.
    pub debug_items: Vec<DebugItem>,

    /// List of current debug messages to log on screen
    messages: Vec<DebugMessage>,

    /// Set of sub-graphs that are required, determined during visibility pass
    pub required_sub_graphs: FastHashSet<PictureIndex>,

    /// Temporary buffer for building segments in to during prepare pass
    pub quad_segments: Vec<QuadSegment>,
}

impl Default for PrimitiveScratchBuffer {
    fn default() -> Self {
        PrimitiveScratchBuffer {
            clip_mask_instances: Vec::new(),
            glyph_keys: GlyphKeyStorage::new(0),
            border_cache_handles: BorderHandleStorage::new(0),
            segments: SegmentStorage::new(0),
            segment_instances: SegmentInstanceStorage::new(0),
            gradient_tiles: GradientTileStorage::new(0),
            debug_items: Vec::new(),
            messages: Vec::new(),
            required_sub_graphs: FastHashSet::default(),
            quad_segments: Vec::new(),
        }
    }
}

impl PrimitiveScratchBuffer {
    pub fn recycle(&mut self, recycler: &mut Recycler) {
        recycler.recycle_vec(&mut self.clip_mask_instances);
        self.glyph_keys.recycle(recycler);
        self.border_cache_handles.recycle(recycler);
        self.segments.recycle(recycler);
        self.segment_instances.recycle(recycler);
        self.gradient_tiles.recycle(recycler);
        recycler.recycle_vec(&mut self.debug_items);
        recycler.recycle_vec(&mut self.quad_segments);
    }

    pub fn begin_frame(&mut self) {
        // Clear the clip mask tasks for the beginning of the frame. Append
        // a single kind representing no clip mask, at the ClipTaskIndex::INVALID
        // location.
        self.clip_mask_instances.clear();
        self.clip_mask_instances.push(ClipMaskKind::None);
        self.quad_segments.clear();

        self.border_cache_handles.clear();

        // TODO(gw): As in the previous code, the gradient tiles store GPU cache
        //           handles that are cleared (and thus invalidated + re-uploaded)
        //           every frame. This maintains the existing behavior, but we
        //           should fix this in the future to retain handles.
        self.gradient_tiles.clear();

        self.required_sub_graphs.clear();

        self.debug_items.clear();
    }

    pub fn end_frame(&mut self) {
        const MSGS_TO_RETAIN: usize = 32;
        const TIME_TO_RETAIN: u64 = 2000000000;
        const LINE_HEIGHT: f32 = 20.0;
        const X0: f32 = 32.0;
        const Y0: f32 = 32.0;
        let now = time::precise_time_ns();

        let msgs_to_remove = self.messages.len().max(MSGS_TO_RETAIN) - MSGS_TO_RETAIN;
        let mut msgs_removed = 0;

        self.messages.retain(|msg| {
            if msgs_removed < msgs_to_remove {
                msgs_removed += 1;
                return false;
            }

            if msg.timestamp + TIME_TO_RETAIN < now {
                return false;
            }

            true
        });

        let mut y = Y0 + self.messages.len() as f32 * LINE_HEIGHT;
        let shadow_offset = 1.0;

        for msg in &self.messages {
            self.debug_items.push(DebugItem::Text {
                position: DevicePoint::new(X0 + shadow_offset, y + shadow_offset),
                color: debug_colors::BLACK,
                msg: msg.msg.clone(),
            });

            self.debug_items.push(DebugItem::Text {
                position: DevicePoint::new(X0, y),
                color: debug_colors::RED,
                msg: msg.msg.clone(),
            });

            y -= LINE_HEIGHT;
        }
    }

    #[allow(dead_code)]
    pub fn push_debug_rect(
        &mut self,
        rect: DeviceRect,
        outer_color: ColorF,
        inner_color: ColorF,
    ) {
        self.debug_items.push(DebugItem::Rect {
            rect,
            outer_color,
            inner_color,
        });
    }

    #[allow(dead_code)]
    pub fn push_debug_string(
        &mut self,
        position: DevicePoint,
        color: ColorF,
        msg: String,
    ) {
        self.debug_items.push(DebugItem::Text {
            position,
            color,
            msg,
        });
    }

    #[allow(dead_code)]
    pub fn log(
        &mut self,
        msg: String,
    ) {
        self.messages.push(DebugMessage {
            msg,
            timestamp: time::precise_time_ns(),
        })
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Debug)]
pub struct PrimitiveStoreStats {
    picture_count: usize,
    text_run_count: usize,
    image_count: usize,
    linear_gradient_count: usize,
    color_binding_count: usize,
}

impl PrimitiveStoreStats {
    pub fn empty() -> Self {
        PrimitiveStoreStats {
            picture_count: 0,
            text_run_count: 0,
            image_count: 0,
            linear_gradient_count: 0,
            color_binding_count: 0,
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PrimitiveStore {
    pub pictures: Vec<PicturePrimitive>,
    pub text_runs: TextRunStorage,
    pub linear_gradients: LinearGradientStorage,

    /// A list of image instances. These are stored separately as
    /// storing them inline in the instance makes the structure bigger
    /// for other types.
    pub images: ImageInstanceStorage,

    /// animated color bindings for this primitive.
    pub color_bindings: ColorBindingStorage,
}

impl PrimitiveStore {
    pub fn new(stats: &PrimitiveStoreStats) -> PrimitiveStore {
        PrimitiveStore {
            pictures: Vec::with_capacity(stats.picture_count),
            text_runs: TextRunStorage::new(stats.text_run_count),
            images: ImageInstanceStorage::new(stats.image_count),
            color_bindings: ColorBindingStorage::new(stats.color_binding_count),
            linear_gradients: LinearGradientStorage::new(stats.linear_gradient_count),
        }
    }

    pub fn get_stats(&self) -> PrimitiveStoreStats {
        PrimitiveStoreStats {
            picture_count: self.pictures.len(),
            text_run_count: self.text_runs.len(),
            image_count: self.images.len(),
            linear_gradient_count: self.linear_gradients.len(),
            color_binding_count: self.color_bindings.len(),
        }
    }

    #[allow(unused)]
    pub fn print_picture_tree(&self, root: PictureIndex) {
        use crate::print_tree::PrintTree;
        let mut pt = PrintTree::new("picture tree");
        self.pictures[root.0].print(&self.pictures, root, &mut pt);
    }
}

/// Trait for primitives that are directly internable.
/// see SceneBuilder::add_primitive<P>
pub trait InternablePrimitive: intern::Internable<InternData = ()> + Sized {
    /// Build a new key from self with `info`.
    fn into_key(
        self,
        info: &LayoutPrimitiveInfo,
    ) -> Self::Key;

    fn make_instance_kind(
        key: Self::Key,
        data_handle: intern::Handle<Self>,
        prim_store: &mut PrimitiveStore,
        reference_frame_relative_offset: LayoutVector2D,
    ) -> PrimitiveInstanceKind;
}


#[test]
#[cfg(target_pointer_width = "64")]
fn test_struct_sizes() {
    use std::mem;
    // The sizes of these structures are critical for performance on a number of
    // talos stress tests. If you get a failure here on CI, there's two possibilities:
    // (a) You made a structure smaller than it currently is. Great work! Update the
    //     test expectations and move on.
    // (b) You made a structure larger. This is not necessarily a problem, but should only
    //     be done with care, and after checking if talos performance regresses badly.
    assert_eq!(mem::size_of::<PrimitiveInstance>(), 88, "PrimitiveInstance size changed");
    assert_eq!(mem::size_of::<PrimitiveInstanceKind>(), 24, "PrimitiveInstanceKind size changed");
    assert_eq!(mem::size_of::<PrimitiveTemplate>(), 56, "PrimitiveTemplate size changed");
    assert_eq!(mem::size_of::<PrimitiveTemplateKind>(), 28, "PrimitiveTemplateKind size changed");
    assert_eq!(mem::size_of::<PrimitiveKey>(), 36, "PrimitiveKey size changed");
    assert_eq!(mem::size_of::<PrimitiveKeyKind>(), 16, "PrimitiveKeyKind size changed");
}

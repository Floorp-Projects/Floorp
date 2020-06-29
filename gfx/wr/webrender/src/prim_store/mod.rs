/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipMode, ColorF, ColorU};
use api::{ImageRendering, RepeatMode, PrimitiveFlags};
use api::{PremultipliedColorF, PropertyBinding, Shadow};
use api::{LineStyle, LineOrientation};
use api::{PrimitiveKeyKind, EdgeAaSegmentMask};
use api::units::*;
use euclid::{SideOffsets2D, Transform3D, Rect, Size2D, Point2D, Vector2D};
use malloc_size_of::MallocSizeOf;
use crate::border::BorderSegmentCacheKey;
use crate::spatial_tree::{SpatialTree, CoordinateSpaceMapping, SpatialNodeIndex, VisibleFace};
use crate::clip::ClipChainId;
use crate::debug_render::DebugItem;
use crate::scene_building::{CreateShadow, IsVisible};
use crate::frame_builder::FrameBuildingState;
use crate::glyph_rasterizer::GlyphKey;
use crate::gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle, GpuDataRequest, ToGpuBlocks};
use crate::gpu_types::{BrushFlags};
use crate::intern;
use crate::picture::{PicturePrimitive, RecordedDirtyRegion};
use crate::prim_store::backdrop::BackdropDataHandle;
use crate::prim_store::borders::{ImageBorderDataHandle, NormalBorderDataHandle};
use crate::prim_store::gradient::{LinearGradientPrimitive, LinearGradientDataHandle, RadialGradientDataHandle, ConicGradientDataHandle};
use crate::prim_store::image::{ImageDataHandle, ImageInstance, YuvImageDataHandle};
use crate::prim_store::line_dec::LineDecorationDataHandle;
use crate::prim_store::picture::PictureDataHandle;
use crate::prim_store::text_run::{TextRunDataHandle, TextRunPrimitive};
#[cfg(debug_assertions)]
use crate::render_backend::{FrameId};
use crate::render_task_graph::RenderTaskId;
use crate::render_task_cache::RenderTaskCacheEntryHandle;
use crate::renderer::{MAX_VERTEX_TEXTURE_WIDTH};
use crate::resource_cache::ImageProperties;
use crate::scene::SceneProperties;
use std::{fmt, hash, ops, u32, usize};
#[cfg(debug_assertions)]
use std::sync::atomic::{AtomicUsize, Ordering};
use crate::storage;
use crate::util::{MatrixHelpers, Recycler, ScaleOffset, RectHelpers, PointHelpers};
use crate::util::{project_rect, raster_rect_to_device_pixels};
use crate::internal_types::LayoutPrimitiveInfo;
use crate::visibility::{PrimitiveVisibility, PrimitiveVisibilityIndex};

pub mod backdrop;
pub mod borders;
pub mod gradient;
pub mod image;
pub mod line_dec;
pub mod picture;
pub mod text_run;
pub mod interned;

pub const VECS_PER_SEGMENT: usize = 2;

/// Counter for unique primitive IDs for debug tracing.
#[cfg(debug_assertions)]
static NEXT_PRIM_ID: AtomicUsize = AtomicUsize::new(0);

#[cfg(debug_assertions)]
static PRIM_CHASE_ID: AtomicUsize = AtomicUsize::new(usize::MAX);

#[cfg(debug_assertions)]
pub fn register_prim_chase_id(id: PrimitiveDebugId) {
    PRIM_CHASE_ID.store(id.0, Ordering::SeqCst);
}

#[cfg(not(debug_assertions))]
pub fn register_prim_chase_id(_: PrimitiveDebugId) {
}

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

    pub fn combine(self, other: PrimitiveOpacity) -> PrimitiveOpacity {
        PrimitiveOpacity{
            is_opaque: self.is_opaque && other.is_opaque
        }
    }
}

#[derive(Clone, Debug)]
pub struct SpaceSnapper {
    pub ref_spatial_node_index: SpatialNodeIndex,
    current_target_spatial_node_index: SpatialNodeIndex,
    snapping_transform: Option<ScaleOffset>,
    pub device_pixel_scale: DevicePixelScale,
}

impl SpaceSnapper {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        device_pixel_scale: DevicePixelScale,
    ) -> Self {
        SpaceSnapper {
            ref_spatial_node_index,
            current_target_spatial_node_index: SpatialNodeIndex::INVALID,
            snapping_transform: None,
            device_pixel_scale,
        }
    }

    pub fn new_with_target(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        device_pixel_scale: DevicePixelScale,
        spatial_tree: &SpatialTree,
    ) -> Self {
        let mut snapper = SpaceSnapper {
            ref_spatial_node_index,
            current_target_spatial_node_index: SpatialNodeIndex::INVALID,
            snapping_transform: None,
            device_pixel_scale,
        };

        snapper.set_target_spatial_node(target_node_index, spatial_tree);
        snapper
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        spatial_tree: &SpatialTree,
    ) {
        if target_node_index == self.current_target_spatial_node_index {
            return
        }

        let ref_spatial_node = &spatial_tree.spatial_nodes[self.ref_spatial_node_index.0 as usize];
        let target_spatial_node = &spatial_tree.spatial_nodes[target_node_index.0 as usize];

        self.current_target_spatial_node_index = target_node_index;
        self.snapping_transform = match (ref_spatial_node.snapping_transform, target_spatial_node.snapping_transform) {
            (Some(ref ref_scale_offset), Some(ref target_scale_offset)) => {
                Some(ref_scale_offset
                    .inverse()
                    .accumulate(target_scale_offset)
                    .scale(self.device_pixel_scale.0))
            }
            _ => None,
        };
    }

    pub fn snap_rect<F>(&self, rect: &Rect<f32, F>) -> Rect<f32, F> where F: fmt::Debug {
        debug_assert!(self.current_target_spatial_node_index != SpatialNodeIndex::INVALID);
        match self.snapping_transform {
            Some(ref scale_offset) => {
                let snapped_device_rect : DeviceRect = scale_offset.map_rect(rect).snap();
                scale_offset.unmap_rect(&snapped_device_rect)
            }
            None => *rect,
        }
    }

    pub fn snap_point<F>(&self, point: &Point2D<f32, F>) -> Point2D<f32, F> where F: fmt::Debug {
        debug_assert!(self.current_target_spatial_node_index != SpatialNodeIndex::INVALID);
        match self.snapping_transform {
            Some(ref scale_offset) => {
                let snapped_device_vector : DevicePoint = scale_offset.map_point(point).snap();
                scale_offset.unmap_point(&snapped_device_vector)
            }
            None => *point,
        }
    }

    pub fn snap_size<F>(&self, size: &Size2D<f32, F>) -> Size2D<f32, F> where F: fmt::Debug {
        debug_assert!(self.current_target_spatial_node_index != SpatialNodeIndex::INVALID);
        match self.snapping_transform {
            Some(ref scale_offset) => {
                let rect = Rect::<f32, F>::new(Point2D::<f32, F>::zero(), *size);
                let snapped_device_rect : DeviceRect = scale_offset.map_rect(&rect).snap();
                scale_offset.unmap_rect(&snapped_device_rect).size
            }
            None => *size,
        }
    }
}

#[derive(Debug, Clone)]
pub struct SpaceMapper<F, T> {
    kind: CoordinateSpaceMapping<F, T>,
    pub ref_spatial_node_index: SpatialNodeIndex,
    pub current_target_spatial_node_index: SpatialNodeIndex,
    pub bounds: Rect<f32, T>,
    visible_face: VisibleFace,
}

impl<F, T> SpaceMapper<F, T> where F: fmt::Debug {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        bounds: Rect<f32, T>,
    ) -> Self {
        SpaceMapper {
            kind: CoordinateSpaceMapping::Local,
            ref_spatial_node_index,
            current_target_spatial_node_index: ref_spatial_node_index,
            bounds,
            visible_face: VisibleFace::Front,
        }
    }

    pub fn new_with_target(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        bounds: Rect<f32, T>,
        spatial_tree: &SpatialTree,
    ) -> Self {
        let mut mapper = Self::new(ref_spatial_node_index, bounds);
        mapper.set_target_spatial_node(target_node_index, spatial_tree);
        mapper
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        spatial_tree: &SpatialTree,
    ) {
        if target_node_index == self.current_target_spatial_node_index {
            return
        }

        let ref_spatial_node = &spatial_tree.spatial_nodes[self.ref_spatial_node_index.0 as usize];
        let target_spatial_node = &spatial_tree.spatial_nodes[target_node_index.0 as usize];

        self.kind = if self.ref_spatial_node_index == target_node_index {
            CoordinateSpaceMapping::Local
        } else if ref_spatial_node.coordinate_system_id == target_spatial_node.coordinate_system_id {
            let scale_offset = ref_spatial_node.content_transform
                .inverse()
                .accumulate(&target_spatial_node.content_transform);
            CoordinateSpaceMapping::ScaleOffset(scale_offset)
        } else {
            let transform = spatial_tree
                .get_relative_transform(target_node_index, self.ref_spatial_node_index)
                .into_transform()
                .with_source::<F>()
                .with_destination::<T>();
            CoordinateSpaceMapping::Transform(transform)
        };

        self.visible_face = self.kind.visible_face();
        self.current_target_spatial_node_index = target_node_index;
    }

    pub fn get_transform(&self) -> Transform3D<f32, F, T> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Transform3D::identity()
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                scale_offset.to_transform()
            }
            CoordinateSpaceMapping::Transform(transform) => {
                transform
            }
        }
    }

    pub fn unmap(&self, rect: &Rect<f32, T>) -> Option<Rect<f32, F>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(rect.cast_unit())
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.unmap_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.inverse_rect_footprint(rect)
            }
        }
    }

    pub fn map(&self, rect: &Rect<f32, F>) -> Option<Rect<f32, T>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(rect.cast_unit())
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.map_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                match project_rect(transform, rect, &self.bounds) {
                    Some(bounds) => {
                        Some(bounds)
                    }
                    None => {
                        warn!("parent relative transform can't transform the primitive rect for {:?}", rect);
                        None
                    }
                }
            }
        }
    }

    pub fn map_vector(&self, v: Vector2D<f32, F>) -> Vector2D<f32, T> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                v.cast_unit()
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                scale_offset.map_vector(&v)
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.transform_vector2d(v)
            }
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
pub struct ClipTaskIndex(pub u16);

impl ClipTaskIndex {
    pub const INVALID: ClipTaskIndex = ClipTaskIndex(0);
}

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, MallocSizeOf, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureIndex(pub usize);

impl GpuCacheHandle {
    pub fn as_int(self, gpu_cache: &GpuCache) -> i32 {
        gpu_cache.get_address(&self).as_int()
    }
}

impl GpuCacheAddress {
    pub fn as_int(self) -> i32 {
        // TODO(gw): Temporarily encode GPU Cache addresses as a single int.
        //           In the future, we can change the PrimitiveInstanceData struct
        //           to use 2x u16 for the vertex attribute instead of an i32.
        self.v as i32 * MAX_VERTEX_TEXTURE_WIDTH as i32 + self.u as i32
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Debug, Clone, MallocSizeOf, PartialEq)]
pub struct RectangleKey {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
}

impl RectangleKey {
    pub fn intersects(&self, other: &Self) -> bool {
        self.x < other.x + other.w
            && other.x < self.x + self.w
            && self.y < other.y + other.h
            && other.y < self.y + self.h
    }
}

impl Eq for RectangleKey {}

impl hash::Hash for RectangleKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.x.to_bits().hash(state);
        self.y.to_bits().hash(state);
        self.w.to_bits().hash(state);
        self.h.to_bits().hash(state);
    }
}

impl From<RectangleKey> for LayoutRect {
    fn from(key: RectangleKey) -> LayoutRect {
        LayoutRect {
            origin: LayoutPoint::new(key.x, key.y),
            size: LayoutSize::new(key.w, key.h),
        }
    }
}

impl From<RectangleKey> for WorldRect {
    fn from(key: RectangleKey) -> WorldRect {
        WorldRect {
            origin: WorldPoint::new(key.x, key.y),
            size: WorldSize::new(key.w, key.h),
        }
    }
}

impl From<LayoutRect> for RectangleKey {
    fn from(rect: LayoutRect) -> RectangleKey {
        RectangleKey {
            x: rect.origin.x,
            y: rect.origin.y,
            w: rect.size.width,
            h: rect.size.height,
        }
    }
}

impl From<PictureRect> for RectangleKey {
    fn from(rect: PictureRect) -> RectangleKey {
        RectangleKey {
            x: rect.origin.x,
            y: rect.origin.y,
            w: rect.size.width,
            h: rect.size.height,
        }
    }
}

impl From<WorldRect> for RectangleKey {
    fn from(rect: WorldRect) -> RectangleKey {
        RectangleKey {
            x: rect.origin.x,
            y: rect.origin.y,
            w: rect.size.width,
            h: rect.size.height,
        }
    }
}

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
}

impl PrimTemplateCommonData {
    pub fn with_key_common(common: PrimKeyCommonData) -> Self {
        PrimTemplateCommonData {
            flags: common.flags,
            may_need_repetition: true,
            prim_rect: common.prim_rect.into(),
            gpu_cache_handle: GpuCacheHandle::new(),
            opacity: PrimitiveOpacity::translucent(),
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
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct VisibleGradientTile {
    pub handle: GpuCacheHandle,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct CachedGradientSegment {
    pub handle: RenderTaskCacheEntryHandle,
    pub local_rect: LayoutRect,
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

#[derive(Debug)]
#[repr(C)]
struct ClipRect {
    rect: LayoutRect,
    mode: f32,
}

#[derive(Debug)]
#[repr(C)]
struct ClipCorner {
    rect: LayoutRect,
    outer_radius_x: f32,
    outer_radius_y: f32,
    inner_radius_x: f32,
    inner_radius_y: f32,
}

impl ToGpuBlocks for ClipCorner {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        self.write(&mut request)
    }
}

impl ClipCorner {
    fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.rect);
        request.push([
            self.outer_radius_x,
            self.outer_radius_y,
            self.inner_radius_x,
            self.inner_radius_y,
        ]);
    }

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

#[derive(Debug)]
#[repr(C)]
pub struct ImageMaskData {
    /// The local size of the whole masked area.
    pub local_mask_size: LayoutSize,
}

impl ToGpuBlocks for ImageMaskData {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push([
            self.local_mask_size.width,
            self.local_mask_size.height,
            0.0,
            0.0,
        ]);
    }
}

#[derive(Debug)]
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
        let rect = LayoutRect::new(
            LayoutPoint::zero(),
            size,
        );

        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y),
                    LayoutSize::new(radii.top_left.width, radii.top_left.height),
                ),
                outer_radius_x: radii.top_left.width,
                outer_radius_y: radii.top_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            top_right: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radii.top_right.width,
                        rect.origin.y,
                    ),
                    LayoutSize::new(radii.top_right.width, radii.top_right.height),
                ),
                outer_radius_x: radii.top_right.width,
                outer_radius_y: radii.top_right.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_left: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x,
                        rect.origin.y + rect.size.height - radii.bottom_left.height,
                    ),
                    LayoutSize::new(radii.bottom_left.width, radii.bottom_left.height),
                ),
                outer_radius_x: radii.bottom_left.width,
                outer_radius_y: radii.bottom_left.height,
                inner_radius_x: 0.0,
                inner_radius_y: 0.0,
            },
            bottom_right: ClipCorner {
                rect: LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radii.bottom_right.width,
                        rect.origin.y + rect.size.height - radii.bottom_right.height,
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
        let rect = LayoutRect::new(
            LayoutPoint::zero(),
            size,
        );

        ClipData {
            rect: ClipRect {
                rect,
                mode: mode as u32 as f32,
            },
            top_left: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            top_right: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x + rect.size.width - radius, rect.origin.y),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_left: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(rect.origin.x, rect.origin.y + rect.size.height - radius),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
            bottom_right: ClipCorner::uniform(
                LayoutRect::new(
                    LayoutPoint::new(
                        rect.origin.x + rect.size.width - radius,
                        rect.origin.y + rect.size.height - radius,
                    ),
                    LayoutSize::new(radius, radius),
                ),
                radius,
                0.0,
            ),
        }
    }

    pub fn write(&self, request: &mut GpuDataRequest) {
        request.push(self.rect.rect);
        request.push([self.rect.mode, 0.0, 0.0, 0.0]);
        for corner in &[
            &self.top_left,
            &self.top_right,
            &self.bottom_left,
            &self.bottom_right,
        ] {
            corner.write(request);
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
    pub outset: SideOffsetsKey,
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

#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveDebugId(pub usize);

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub enum PrimitiveInstanceKind {
    /// Direct reference to a Picture
    Picture {
        /// Handle to the common interned data for this primitive.
        data_handle: PictureDataHandle,
        pic_index: PictureIndex,
        segment_instance_index: SegmentInstanceIndex,
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
        cache_handle: Option<RenderTaskCacheEntryHandle>,
    },
    NormalBorder {
        /// Handle to the common interned data for this primitive.
        data_handle: NormalBorderDataHandle,
        cache_handles: storage::Range<RenderTaskCacheEntryHandle>,
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
    },
    YuvImage {
        /// Handle to the common interned data for this primitive.
        data_handle: YuvImageDataHandle,
        segment_instance_index: SegmentInstanceIndex,
        is_compositor_surface: bool,
    },
    Image {
        /// Handle to the common interned data for this primitive.
        data_handle: ImageDataHandle,
        image_instance_index: ImageInstanceIndex,
        is_compositor_surface: bool,
    },
    LinearGradient {
        /// Handle to the common interned data for this primitive.
        data_handle: LinearGradientDataHandle,
        gradient_index: LinearGradientIndex,
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
    Backdrop {
        data_handle: BackdropDataHandle,
    },
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PrimitiveInstance {
    /// Identifies the kind of primitive this
    /// instance is, and references to where
    /// the relevant information for the primitive
    /// can be found.
    pub kind: PrimitiveInstanceKind,

    /// Local space clip rect for this instance
    pub local_clip_rect: LayoutRect,

    #[cfg(debug_assertions)]
    pub id: PrimitiveDebugId,

    /// The last frame ID (of the `RenderTaskGraph`) this primitive
    /// was prepared for rendering in.
    #[cfg(debug_assertions)]
    pub prepared_frame_id: FrameId,

    /// If this primitive is visible, an index into the instance
    /// visibility scratch buffer. If not visible, INVALID.
    pub visibility_info: PrimitiveVisibilityIndex,

    /// ID of the clip chain that this primitive is clipped by.
    pub clip_chain_id: ClipChainId,
}

impl PrimitiveInstance {
    pub fn new(
        local_clip_rect: LayoutRect,
        kind: PrimitiveInstanceKind,
        clip_chain_id: ClipChainId,
    ) -> Self {
        PrimitiveInstance {
            local_clip_rect,
            kind,
            #[cfg(debug_assertions)]
            prepared_frame_id: FrameId::INVALID,
            #[cfg(debug_assertions)]
            id: PrimitiveDebugId(NEXT_PRIM_ID.fetch_add(1, Ordering::Relaxed)),
            visibility_info: PrimitiveVisibilityIndex::INVALID,
            clip_chain_id,
        }
    }

    // Reset any pre-frame state for this primitive.
    pub fn reset(&mut self) {
        self.visibility_info = PrimitiveVisibilityIndex::INVALID;
    }

    #[cfg(debug_assertions)]
    pub fn is_chased(&self) -> bool {
        PRIM_CHASE_ID.load(Ordering::SeqCst) == self.id.0
    }

    #[cfg(not(debug_assertions))]
    pub fn is_chased(&self) -> bool {
        false
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
            PrimitiveInstanceKind::Backdrop { data_handle, .. } => {
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
pub type BorderHandleStorage = storage::Storage<RenderTaskCacheEntryHandle>;
pub type SegmentStorage = storage::Storage<BrushSegment>;
pub type SegmentsRange = storage::Range<BrushSegment>;
pub type SegmentInstanceStorage = storage::Storage<SegmentedInstance>;
pub type SegmentInstanceIndex = storage::Index<SegmentedInstance>;
pub type ImageInstanceStorage = storage::Storage<ImageInstance>;
pub type ImageInstanceIndex = storage::Index<ImageInstance>;
pub type GradientTileStorage = storage::Storage<VisibleGradientTile>;
pub type GradientTileRange = storage::Range<VisibleGradientTile>;
pub type LinearGradientIndex = storage::Index<LinearGradientPrimitive>;
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

    /// List of the visibility information for currently visible primitives.
    pub prim_info: Vec<PrimitiveVisibility>,

    /// List of dirty regions for the cached pictures in this document, used to
    /// verify invalidation in wrench reftests. Only collected in testing.
    pub recorded_dirty_regions: Vec<RecordedDirtyRegion>,

    /// List of debug display items for rendering.
    pub debug_items: Vec<DebugItem>,
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
            recorded_dirty_regions: Vec::new(),
            debug_items: Vec::new(),
            prim_info: Vec::new(),
        }
    }
}

impl PrimitiveScratchBuffer {
    pub fn recycle(&mut self, recycler: &mut Recycler) {
        recycler.recycle_vec(&mut self.clip_mask_instances);
        recycler.recycle_vec(&mut self.prim_info);
        self.glyph_keys.recycle(recycler);
        self.border_cache_handles.recycle(recycler);
        self.segments.recycle(recycler);
        self.segment_instances.recycle(recycler);
        self.gradient_tiles.recycle(recycler);
        recycler.recycle_vec(&mut self.debug_items);
    }

    pub fn begin_frame(&mut self) {
        // Clear the clip mask tasks for the beginning of the frame. Append
        // a single kind representing no clip mask, at the ClipTaskIndex::INVALID
        // location.
        self.clip_mask_instances.clear();
        self.clip_mask_instances.push(ClipMaskKind::None);

        self.border_cache_handles.clear();

        // TODO(gw): As in the previous code, the gradient tiles store GPU cache
        //           handles that are cleared (and thus invalidated + re-uploaded)
        //           every frame. This maintains the existing behavior, but we
        //           should fix this in the future to retain handles.
        self.gradient_tiles.clear();

        self.prim_info.clear();

        self.debug_items.clear();

        assert!(self.recorded_dirty_regions.is_empty(), "Should have sent to Renderer");
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

    /// Returns the total count of primitive instances contained in pictures.
    pub fn prim_count(&self) -> usize {
        let mut prim_count = 0;
        for pic in &self.pictures {
            prim_count += pic.prim_list.prim_instances.len();
        }
        prim_count
    }
}

pub fn get_raster_rects(
    pic_rect: PictureRect,
    map_to_raster: &SpaceMapper<PicturePixel, RasterPixel>,
    map_to_world: &SpaceMapper<RasterPixel, WorldPixel>,
    prim_bounding_rect: WorldRect,
    device_pixel_scale: DevicePixelScale,
) -> Option<(DeviceRect, DeviceRect)> {
    let unclipped_raster_rect = map_to_raster.map(&pic_rect)?;

    let unclipped = raster_rect_to_device_pixels(
        unclipped_raster_rect,
        device_pixel_scale,
    );

    let unclipped_world_rect = map_to_world.map(&unclipped_raster_rect)?;

    let clipped_world_rect = unclipped_world_rect.intersection(&prim_bounding_rect)?;

    let clipped_raster_rect = map_to_world.unmap(&clipped_world_rect)?;

    let clipped_raster_rect = clipped_raster_rect.intersection(&unclipped_raster_rect)?;

    let clipped = raster_rect_to_device_pixels(
        clipped_raster_rect,
        device_pixel_scale,
    );

    // Ensure that we won't try to allocate a zero-sized clip render task.
    if clipped.is_empty() {
        return None;
    }

    Some((clipped, unclipped))
}

/// Choose the decoration mask tile size for a given line.
///
/// Given a line with overall size `rect_size` and the given `orientation`,
/// return the dimensions of a single mask tile for the decoration pattern
/// described by `style` and `wavy_line_thickness`.
///
/// If `style` is `Solid`, no mask tile is necessary; return `None`. The other
/// styles each have their own characteristic periods of repetition, so for each
/// one, this function returns a `LayoutSize` with the right aspect ratio and
/// whose specific size is convenient for the `cs_line_decoration.glsl` fragment
/// shader to work with. The shader uses a local coordinate space in which the
/// tile fills a rectangle with one corner at the origin, and with the size this
/// function returns.
///
/// The returned size is not necessarily in pixels; device scaling and other
/// concerns can still affect the actual task size.
///
/// Regardless of whether `orientation` is `Vertical` or `Horizontal`, the
/// `width` and `height` of the returned size are always horizontal and
/// vertical, respectively.
pub fn get_line_decoration_size(
    rect_size: &LayoutSize,
    orientation: LineOrientation,
    style: LineStyle,
    wavy_line_thickness: f32,
) -> Option<LayoutSize> {
    let h = match orientation {
        LineOrientation::Horizontal => rect_size.height,
        LineOrientation::Vertical => rect_size.width,
    };

    // TODO(gw): The formulae below are based on the existing gecko and line
    //           shader code. They give reasonable results for most inputs,
    //           but could definitely do with a detailed pass to get better
    //           quality on a wider range of inputs!
    //           See nsCSSRendering::PaintDecorationLine in Gecko.

    let (parallel, perpendicular) = match style {
        LineStyle::Solid => {
            return None;
        }
        LineStyle::Dashed => {
            let dash_length = (3.0 * h).min(64.0).max(1.0);

            (2.0 * dash_length, 4.0)
        }
        LineStyle::Dotted => {
            let diameter = h.min(64.0).max(1.0);
            let period = 2.0 * diameter;

            (period, diameter)
        }
        LineStyle::Wavy => {
            let line_thickness = wavy_line_thickness.max(1.0);
            let slope_length = h - line_thickness;
            let flat_length = ((line_thickness - 1.0) * 2.0).max(1.0);
            let approx_period = 2.0 * (slope_length + flat_length);

            (approx_period, h)
        }
    };

    Some(match orientation {
        LineOrientation::Horizontal => LayoutSize::new(parallel, perpendicular),
        LineOrientation::Vertical => LayoutSize::new(perpendicular, parallel),
    })
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
    assert_eq!(mem::size_of::<PrimitiveInstance>(), 80, "PrimitiveInstance size changed");
    assert_eq!(mem::size_of::<PrimitiveInstanceKind>(), 40, "PrimitiveInstanceKind size changed");
    assert_eq!(mem::size_of::<PrimitiveTemplate>(), 56, "PrimitiveTemplate size changed");
    assert_eq!(mem::size_of::<PrimitiveTemplateKind>(), 28, "PrimitiveTemplateKind size changed");
    assert_eq!(mem::size_of::<PrimitiveKey>(), 36, "PrimitiveKey size changed");
    assert_eq!(mem::size_of::<PrimitiveKeyKind>(), 16, "PrimitiveKeyKind size changed");
}

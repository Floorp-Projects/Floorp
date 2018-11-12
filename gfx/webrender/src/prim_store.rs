/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AlphaType, BorderRadius, BuiltDisplayList, ClipMode, ColorF, PictureRect, ColorU, LayoutPrimitiveInfo};
use api::{DeviceIntRect, DeviceIntSize, DevicePixelScale, ExtendMode, DeviceRect, PictureToRasterTransform};
use api::{FilterOp, GlyphInstance, GradientStop, ImageKey, ImageRendering, ItemRange, TileOffset};
use api::{RasterSpace, LayoutPoint, LayoutRect, LayoutSideOffsets, LayoutSize, LayoutToWorldTransform};
use api::{LayoutVector2D, PremultipliedColorF, PropertyBinding, Shadow, YuvColorSpace, YuvFormat, LayoutRectAu};
use api::{DeviceIntSideOffsets, WorldPixel, BoxShadowClipMode, NormalBorder, WorldRect, LayoutToWorldScale};
use api::{PicturePixel, RasterPixel, ColorDepth, LineStyle, LineOrientation, LayoutSizeAu, AuHelpers, LayoutVector2DAu};
use app_units::Au;
use border::{get_max_scale_for_border, build_border_instances, create_normal_border_prim};
use clip_scroll_tree::{ClipScrollTree, SpatialNodeIndex};
use clip::{ClipNodeFlags, ClipChainId, ClipChainInstance, ClipItem, ClipNodeCollector};
use euclid::{TypedTransform3D, TypedRect, TypedScale};
use frame_builder::{FrameBuildingContext, FrameBuildingState, PictureContext, PictureState};
use frame_builder::PrimitiveContext;
use glyph_rasterizer::{FontInstance, FontTransform, GlyphKey, FONT_SIZE_LIMIT};
use gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle, GpuDataRequest, ToGpuBlocks};
use gpu_types::BrushFlags;
use image::{self, Repetition};
use intern;
use picture::{ClusterRange, PictureCompositeMode, PicturePrimitive, PictureUpdateContext};
use picture::{PrimitiveList, SurfaceInfo, SurfaceIndex};
#[cfg(debug_assertions)]
use render_backend::FrameId;
use render_task::{BlitSource, RenderTask, RenderTaskCacheKey, RenderTaskTree, to_cache_size};
use render_task::{RenderTaskCacheKeyKind, RenderTaskId, RenderTaskCacheEntryHandle};
use renderer::{MAX_VERTEX_TEXTURE_WIDTH};
use resource_cache::{ImageProperties, ImageRequest, ResourceCache};
use scene::SceneProperties;
use std::{cmp, fmt, mem, ops, usize};
#[cfg(debug_assertions)]
use std::sync::atomic::{AtomicUsize, Ordering};
use tiling::SpecialRenderPasses;
use util::{ScaleOffset, MatrixHelpers, MaxRect};
use util::{pack_as_float, project_rect, raster_rect_to_device_pixels};
use smallvec::SmallVec;

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

const MIN_BRUSH_SPLIT_AREA: f32 = 256.0 * 256.0;
pub const VECS_PER_SEGMENT: usize = 2;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ScrollNodeAndClipChain {
    pub spatial_node_index: SpatialNodeIndex,
    pub clip_chain_id: ClipChainId,
}

impl ScrollNodeAndClipChain {
    pub fn new(
        spatial_node_index: SpatialNodeIndex,
        clip_chain_id: ClipChainId
    ) -> Self {
        ScrollNodeAndClipChain {
            spatial_node_index,
            clip_chain_id,
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Copy, Clone)]
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

#[derive(Debug, Copy, Clone)]
pub enum VisibleFace {
    Front,
    Back,
}

impl ops::Not for VisibleFace {
    type Output = Self;
    fn not(self) -> Self {
        match self {
            VisibleFace::Front => VisibleFace::Back,
            VisibleFace::Back => VisibleFace::Front,
        }
    }
}

#[derive(Debug)]
pub enum CoordinateSpaceMapping<F, T> {
    Local,
    ScaleOffset(ScaleOffset),
    Transform(TypedTransform3D<f32, F, T>),
}

impl<F, T> CoordinateSpaceMapping<F, T> {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) -> Self {
        let spatial_nodes = &clip_scroll_tree.spatial_nodes;
        let ref_spatial_node = &spatial_nodes[ref_spatial_node_index.0];
        let target_spatial_node = &spatial_nodes[target_node_index.0];

        if ref_spatial_node_index == target_node_index {
            CoordinateSpaceMapping::Local
        } else if ref_spatial_node.coordinate_system_id == target_spatial_node.coordinate_system_id {
            CoordinateSpaceMapping::ScaleOffset(
                ref_spatial_node.coordinate_system_relative_scale_offset
                    .inverse()
                    .accumulate(
                        &target_spatial_node.coordinate_system_relative_scale_offset
                    )
            )
        } else {
            let transform = clip_scroll_tree.get_relative_transform(
                target_node_index,
                ref_spatial_node_index,
            ).expect("bug: should have already been culled");

            CoordinateSpaceMapping::Transform(
                transform.with_source::<F>().with_destination::<T>()
            )
        }
    }
}

#[derive(Debug)]
pub struct SpaceMapper<F, T> {
    kind: CoordinateSpaceMapping<F, T>,
    pub ref_spatial_node_index: SpatialNodeIndex,
    pub current_target_spatial_node_index: SpatialNodeIndex,
    pub bounds: TypedRect<f32, T>,
}

impl<F, T> SpaceMapper<F, T> where F: fmt::Debug {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        bounds: TypedRect<f32, T>,
    ) -> Self {
        SpaceMapper {
            kind: CoordinateSpaceMapping::Local,
            ref_spatial_node_index,
            current_target_spatial_node_index: ref_spatial_node_index,
            bounds,
        }
    }

    pub fn new_with_target(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        bounds: TypedRect<f32, T>,
        clip_scroll_tree: &ClipScrollTree,
    ) -> Self {
        let mut mapper = SpaceMapper::new(ref_spatial_node_index, bounds);
        mapper.set_target_spatial_node(target_node_index, clip_scroll_tree);
        mapper
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
    ) {
        if target_node_index != self.current_target_spatial_node_index {
            self.current_target_spatial_node_index = target_node_index;

            self.kind = CoordinateSpaceMapping::new(
                self.ref_spatial_node_index,
                target_node_index,
                clip_scroll_tree,
            );
        }
    }

    pub fn get_transform(&self) -> TypedTransform3D<f32, F, T> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                TypedTransform3D::identity()
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                scale_offset.to_transform()
            }
            CoordinateSpaceMapping::Transform(transform) => {
                transform
            }
        }
    }

    pub fn unmap(&self, rect: &TypedRect<f32, T>) -> Option<TypedRect<f32, F>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(TypedRect::from_untyped(&rect.to_untyped()))
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.unmap_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.inverse_rect_footprint(rect)
            }
        }
    }

    pub fn map(&self, rect: &TypedRect<f32, F>) -> Option<TypedRect<f32, T>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(TypedRect::from_untyped(&rect.to_untyped()))
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

    pub fn visible_face(&self) -> VisibleFace {
        match self.kind {
            CoordinateSpaceMapping::Local => VisibleFace::Front,
            CoordinateSpaceMapping::ScaleOffset(_) => VisibleFace::Front,
            CoordinateSpaceMapping::Transform(ref transform) => {
                if transform.is_backface_visible() {
                    VisibleFace::Back
                } else {
                    VisibleFace::Front
                }
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

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TextRunIndex(pub usize);

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash, Ord, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureIndex(pub usize);

impl GpuCacheHandle {
    pub fn as_int(&self, gpu_cache: &GpuCache) -> i32 {
        gpu_cache.get_address(self).as_int()
    }
}

impl GpuCacheAddress {
    pub fn as_int(&self) -> i32 {
        // TODO(gw): Temporarily encode GPU Cache addresses as a single int.
        //           In the future, we can change the PrimitiveInstanceData struct
        //           to use 2x u16 for the vertex attribute instead of an i32.
        self.v as i32 * MAX_VERTEX_TEXTURE_WIDTH as i32 + self.u as i32
    }
}

/// The information about an interned primitive that
/// is stored and available in the scene builder
/// thread.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveSceneData {
    pub culling_rect: LayoutRect,
    pub is_backface_visible: bool,
}

/// Information specific to a primitive type that
/// uniquely identifies a primitive template by key.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub enum PrimitiveKeyKind {
    /// Pictures and old style (non-interned) primitives specify the
    /// Unused primitive key kind. In the future it might make sense
    /// to instead have Option<PrimitiveKeyKind>. It should become
    /// clearer as we port more primitives to be interned.
    Unused,
    /// A run of glyphs, with associated font information.
    TextRun {
        font: FontInstance,
        offset: LayoutVector2DAu,
        glyphs: Vec<GlyphInstance>,
        shadow: bool,
    },
    /// Identifying key for a line decoration.
    LineDecoration {
        // If the cache_key is Some(..) it is a line decoration
        // that relies on a render task (e.g. wavy). If the
        // cache key is None, it uses a fast path to draw the
        // line decoration as a solid rect.
        cache_key: Option<LineDecorationCacheKey>,
        color: ColorU,
    },
    /// Clear an existing rect, used for special effects on some platforms.
    Clear,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, PartialEq, Hash)]
pub struct PrimitiveKey {
    pub is_backface_visible: bool,
    pub prim_rect: LayoutRectAu,
    pub clip_rect: LayoutRectAu,
    pub kind: PrimitiveKeyKind,
}

impl PrimitiveKey {
    pub fn new(
        is_backface_visible: bool,
        prim_rect: LayoutRect,
        clip_rect: LayoutRect,
        kind: PrimitiveKeyKind,
    ) -> Self {
        PrimitiveKey {
            is_backface_visible,
            prim_rect: prim_rect.to_au(),
            clip_rect: clip_rect.to_au(),
            kind,
        }
    }

    /// Construct a primitive instance that matches the type
    /// of primitive key.
    pub fn to_instance_kind(
        &self,
        prim_store: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind {
        match self.kind {
            PrimitiveKeyKind::LineDecoration { .. } => {
                PrimitiveInstanceKind::LineDecoration {
                    cache_handle: None,
                }
            }
            PrimitiveKeyKind::TextRun { ref font, shadow, .. } => {
                let run = TextRunPrimitive {
                    used_font: font.clone(),
                    glyph_keys: Vec::new(),
                    shadow,
                };

                let run_index = TextRunIndex(prim_store.text_runs.len());
                prim_store.text_runs.push(run);

                PrimitiveInstanceKind::TextRun {
                    run_index
                }
            }
            PrimitiveKeyKind::Clear => {
                PrimitiveInstanceKind::Clear
            }
            PrimitiveKeyKind::Unused => {
                // Should never be hit as this method should not be
                // called for old style primitives.
                unreachable!();
            }
        }
    }
}

/// The shared information for a given primitive. This is interned and retained
/// both across frames and display lists, by comparing the matching PrimitiveKey.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum PrimitiveTemplateKind {
    LineDecoration {
        cache_key: Option<LineDecorationCacheKey>,
        color: ColorF,
    },
    TextRun {
        font: FontInstance,
        offset: LayoutVector2DAu,
        glyphs: Vec<GlyphInstance>,
    },
    Clear,
    Unused,
}

/// Construct the primitive template data from a primitive key. This
/// is invoked when a primitive key is created and the interner
/// doesn't currently contain a primitive with this key.
impl From<PrimitiveKeyKind> for PrimitiveTemplateKind {
    fn from(item: PrimitiveKeyKind) -> Self {
        match item {
            PrimitiveKeyKind::Unused => PrimitiveTemplateKind::Unused,
            PrimitiveKeyKind::TextRun { glyphs, font, offset, .. } => {
                PrimitiveTemplateKind::TextRun {
                    font,
                    offset,
                    glyphs,
                }
            }
            PrimitiveKeyKind::Clear => {
                PrimitiveTemplateKind::Clear
            }
            PrimitiveKeyKind::LineDecoration { cache_key, color } => {
                PrimitiveTemplateKind::LineDecoration {
                    cache_key,
                    color: color.into(),
                }
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveTemplate {
    pub is_backface_visible: bool,
    pub prim_rect: LayoutRect,
    pub clip_rect: LayoutRect,
    pub kind: PrimitiveTemplateKind,
    pub opacity: PrimitiveOpacity,
    /// The GPU cache handle for a primitive template. Since this structure
    /// is retained across display lists by interning, this GPU cache handle
    /// also remains valid, which reduces the number of updates to the GPU
    /// cache when a new display list is processed.
    pub gpu_cache_handle: GpuCacheHandle,
}

impl From<PrimitiveKey> for PrimitiveTemplate {
    fn from(item: PrimitiveKey) -> Self {
        PrimitiveTemplate {
            is_backface_visible: item.is_backface_visible,
            prim_rect: LayoutRect::from_au(item.prim_rect),
            clip_rect: LayoutRect::from_au(item.clip_rect),
            kind: item.kind.into(),
            gpu_cache_handle: GpuCacheHandle::new(),
            opacity: PrimitiveOpacity::translucent(),
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
        gpu_cache: &mut GpuCache,
    ) {
        self.opacity = match self.kind {
            PrimitiveTemplateKind::Clear => {
                if let Some(mut request) = gpu_cache.request(&mut self.gpu_cache_handle) {
                    // Opaque black with operator dest out
                    request.push(PremultipliedColorF::BLACK);

                    request.write_segment(
                        self.prim_rect,
                        [0.0; 4],
                    );
                }

                PrimitiveOpacity::translucent()
            }
            PrimitiveTemplateKind::LineDecoration { ref cache_key, ref color } => {
                if let Some(mut request) = gpu_cache.request(&mut self.gpu_cache_handle) {
                    // Work out the stretch parameters (for image repeat) based on the
                    // line decoration parameters.

                    match cache_key {
                        Some(cache_key) => {
                            request.push(color.premultiplied());
                            request.push(PremultipliedColorF::WHITE);
                            request.push([
                                cache_key.size.width.to_f32_px(),
                                cache_key.size.height.to_f32_px(),
                                0.0,
                                0.0,
                            ]);
                        }
                        None => {
                            request.push(color.premultiplied());
                        }
                    }

                    request.write_segment(
                        self.prim_rect,
                        [0.0; 4],
                    );
                }

                match cache_key {
                    Some(..) => PrimitiveOpacity::translucent(),
                    None => PrimitiveOpacity::from_alpha(color.a),
                }
            }
            PrimitiveTemplateKind::TextRun { ref glyphs, ref font, ref offset, .. } => {
                if let Some(mut request) = gpu_cache.request(&mut self.gpu_cache_handle) {
                    request.push(ColorF::from(font.color).premultiplied());
                    // this is the only case where we need to provide plain color to GPU
                    let bg_color = ColorF::from(font.bg_color);
                    request.push([bg_color.r, bg_color.g, bg_color.b, 1.0]);
                    request.push([
                        offset.x.to_f32_px(),
                        offset.y.to_f32_px(),
                        0.0,
                        0.0,
                    ]);

                    let mut gpu_block = [0.0; 4];
                    for (i, src) in glyphs.iter().enumerate() {
                        // Two glyphs are packed per GPU block.

                        if (i & 1) == 0 {
                            gpu_block[0] = src.point.x;
                            gpu_block[1] = src.point.y;
                        } else {
                            gpu_block[2] = src.point.x;
                            gpu_block[3] = src.point.y;
                            request.push(gpu_block);
                        }
                    }

                    // Ensure the last block is added in the case
                    // of an odd number of glyphs.
                    if (glyphs.len() & 1) != 0 {
                        request.push(gpu_block);
                    }

                    assert!(request.current_used_block_num() <= MAX_VERTEX_TEXTURE_WIDTH);
                }

                PrimitiveOpacity::translucent()
            }
            PrimitiveTemplateKind::Unused => {
                PrimitiveOpacity::translucent()
            }
        };
    }
}

// Type definitions for interning primitives.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Copy, Debug, Hash, Eq, PartialEq)]
pub struct PrimitiveDataMarker;

pub type PrimitiveDataStore = intern::DataStore<PrimitiveKey, PrimitiveTemplate, PrimitiveDataMarker>;
pub type PrimitiveDataHandle = intern::Handle<PrimitiveDataMarker>;
pub type PrimitiveDataUpdateList = intern::UpdateList<PrimitiveKey>;
pub type PrimitiveDataInterner = intern::Interner<PrimitiveKey, PrimitiveSceneData, PrimitiveDataMarker>;
pub type PrimitiveUid = intern::ItemUid<PrimitiveDataMarker>;

// Maintains a list of opacity bindings that have been collapsed into
// the color of a single primitive. This is an important optimization
// that avoids allocating an intermediate surface for most common
// uses of opacity filters.
#[derive(Debug)]
pub struct OpacityBinding {
    bindings: Vec<PropertyBinding<f32>>,
    pub current: f32,
}

impl OpacityBinding {
    pub fn new() -> OpacityBinding {
        OpacityBinding {
            bindings: Vec::new(),
            current: 1.0,
        }
    }

    // Add a new opacity value / binding to the list
    pub fn push(&mut self, binding: PropertyBinding<f32>) {
        self.bindings.push(binding);
    }

    // Resolve the current value of each opacity binding, and
    // store that as a single combined opacity. Returns true
    // if the opacity value changed from last time.
    pub fn update(&mut self, scene_properties: &SceneProperties) {
        let mut new_opacity = 1.0;

        for binding in &self.bindings {
            let opacity = scene_properties.resolve_float(binding);
            new_opacity = new_opacity * opacity;
        }

        self.current = new_opacity;
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct VisibleImageTile {
    pub tile_offset: TileOffset,
    pub handle: GpuCacheHandle,
    pub edge_flags: EdgeAaSegmentMask,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct VisibleMaskImageTile {
    pub tile_offset: TileOffset,
    pub handle: GpuCacheHandle,
}

#[derive(Debug)]
pub struct VisibleGradientTile {
    pub handle: GpuCacheHandle,
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
}

/// Information about how to cache a border segment,
/// along with the current render task cache entry.
#[derive(Debug)]
pub struct BorderSegmentInfo {
    pub handle: Option<RenderTaskCacheEntryHandle>,
    pub local_task_size: LayoutSize,
    pub cache_key: RenderTaskCacheKey,
}

#[derive(Debug)]
pub enum BorderSource {
    Image(ImageRequest),
    Border {
        segments: SmallVec<[BorderSegmentInfo; 8]>,
        border: NormalBorder,
        widths: LayoutSideOffsets,
    },
}

pub enum BrushKind {
    Solid {
        color: ColorF,
        opacity_binding: OpacityBinding,
    },
    Image {
        request: ImageRequest,
        alpha_type: AlphaType,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        color: ColorF,
        source: ImageSource,
        sub_rect: Option<DeviceIntRect>,
        opacity_binding: OpacityBinding,
        visible_tiles: Vec<VisibleImageTile>,
    },
    YuvImage {
        yuv_key: [ImageKey; 3],
        format: YuvFormat,
        color_depth: ColorDepth,
        color_space: YuvColorSpace,
        image_rendering: ImageRendering,
    },
    RadialGradient {
        stops_handle: GpuCacheHandle,
        stops_range: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        center: LayoutPoint,
        start_radius: f32,
        end_radius: f32,
        ratio_xy: f32,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        visible_tiles: Vec<VisibleGradientTile>,
    },
    LinearGradient {
        stops_handle: GpuCacheHandle,
        stops_range: ItemRange<GradientStop>,
        extend_mode: ExtendMode,
        reverse_stops: bool,
        start_point: LayoutPoint,
        end_point: LayoutPoint,
        stretch_size: LayoutSize,
        tile_spacing: LayoutSize,
        visible_tiles: Vec<VisibleGradientTile>,
        stops_opacity: PrimitiveOpacity,
    },
    Border {
        source: BorderSource,
    },
}

impl BrushKind {
    fn supports_segments(&self, resource_cache: &ResourceCache) -> bool {
        match *self {
            BrushKind::Image { ref request, .. } => {
                // tiled images don't support segmentation
                resource_cache
                    .get_image_properties(request.key)
                    .and_then(|properties| properties.tiling)
                    .is_none()
            }

            BrushKind::Solid { .. } |
            BrushKind::YuvImage { .. } |
            BrushKind::RadialGradient { .. } |
            BrushKind::Border { .. } |
            BrushKind::LinearGradient { .. } => true,
        }
    }

    // Construct a brush that is a solid color rectangle.
    pub fn new_solid(color: ColorF) -> BrushKind {
        BrushKind::Solid {
            color,
            opacity_binding: OpacityBinding::new(),
        }
    }

    // Construct a brush that is a border with `border` style and `widths`
    // dimensions.
    pub fn new_border(
        mut border: NormalBorder,
        widths: LayoutSideOffsets,
        segments: SmallVec<[BorderSegmentInfo; 8]>,
    ) -> BrushKind {
        // FIXME(emilio): Is this the best place to do this?
        border.normalize(&widths);

        BrushKind::Border {
            source: BorderSource::Border {
                border,
                widths,
                segments,
            }
        }
    }

    // Construct a brush that is an image wisth `stretch_size` dimensions and
    // `color`.
    pub fn new_image(
        request: ImageRequest,
        stretch_size: LayoutSize,
        color: ColorF
    ) -> BrushKind {
        BrushKind::Image {
            request,
            alpha_type: AlphaType::PremultipliedAlpha,
            stretch_size,
            tile_spacing: LayoutSize::new(0., 0.),
            color,
            source: ImageSource::Default,
            sub_rect: None,
            opacity_binding: OpacityBinding::new(),
            visible_tiles: Vec::new(),
        }
    }
}

bitflags! {
    /// Each bit of the edge AA mask is:
    /// 0, when the edge of the primitive needs to be considered for AA
    /// 1, when the edge of the segment needs to be considered for AA
    ///
    /// *Note*: the bit values have to match the shader logic in
    /// `write_transform_vertex()` function.
    #[cfg_attr(feature = "capture", derive(Serialize))]
    #[cfg_attr(feature = "replay", derive(Deserialize))]
    pub struct EdgeAaSegmentMask: u8 {
        const LEFT = 0x1;
        const TOP = 0x2;
        const RIGHT = 0x4;
        const BOTTOM = 0x8;
    }
}

#[derive(Debug, Clone)]
pub enum BrushSegmentTaskId {
    RenderTaskId(RenderTaskId),
    Opaque,
    Empty,
}

impl BrushSegmentTaskId {
    pub fn needs_blending(&self) -> bool {
        match *self {
            BrushSegmentTaskId::RenderTaskId(..) => true,
            BrushSegmentTaskId::Opaque | BrushSegmentTaskId::Empty => false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct BrushSegment {
    pub local_rect: LayoutRect,
    pub clip_task_id: BrushSegmentTaskId,
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
            clip_task_id: BrushSegmentTaskId::Opaque,
            may_need_clip_mask,
            edge_flags,
            extra_data,
            brush_flags,
        }
    }

    pub fn update_clip_task(
        &mut self,
        clip_chain: Option<&ClipChainInstance>,
        prim_bounding_rect: WorldRect,
        root_spatial_node_index: SpatialNodeIndex,
        surface_index: SurfaceIndex,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
    ) {
        match clip_chain {
            Some(clip_chain) => {
                if !clip_chain.needs_mask ||
                   (!self.may_need_clip_mask && !clip_chain.has_non_local_clips) {
                    self.clip_task_id = BrushSegmentTaskId::Opaque;
                    return;
                }

                let (device_rect, _, _) = match get_raster_rects(
                    clip_chain.pic_clip_rect,
                    &pic_state.map_pic_to_raster,
                    &pic_state.map_raster_to_world,
                    prim_bounding_rect,
                    frame_context.device_pixel_scale,
                ) {
                    Some(info) => info,
                    None => {
                        self.clip_task_id = BrushSegmentTaskId::Empty;
                        return;
                    }
                };

                let clip_task = RenderTask::new_mask(
                    device_rect.to_i32(),
                    clip_chain.clips_range,
                    root_spatial_node_index,
                    frame_state.clip_store,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_state.render_tasks,
                    &mut frame_state.resources.clip_data_store,
                );

                let clip_task_id = frame_state.render_tasks.add(clip_task);
                frame_state.surfaces[surface_index.0].tasks.push(clip_task_id);
                self.clip_task_id = BrushSegmentTaskId::RenderTaskId(clip_task_id);
            }
            None => {
                self.clip_task_id = BrushSegmentTaskId::Empty;
            }
        }
    }
}

pub type BrushSegmentVec = SmallVec<[BrushSegment; 1]>;

#[derive(Debug)]
pub struct BrushSegmentDescriptor {
    pub segments: BrushSegmentVec,
}

pub struct BrushPrimitive {
    pub kind: BrushKind,
    pub opacity: PrimitiveOpacity,
    pub segment_desc: Option<BrushSegmentDescriptor>,
}

impl BrushPrimitive {
    pub fn new(
        kind: BrushKind,
        segment_desc: Option<BrushSegmentDescriptor>,
    ) -> Self {
        BrushPrimitive {
            kind,
            opacity: PrimitiveOpacity::translucent(),
            segment_desc,
        }
    }

    fn write_gpu_blocks(
        &self,
        request: &mut GpuDataRequest,
        local_rect: LayoutRect,
    ) {
        // has to match VECS_PER_SPECIFIC_BRUSH
        match self.kind {
            BrushKind::Border { .. } => {
                // Border primitives currently used for
                // image borders, and run through the
                // normal brush_image shader.
                request.push(PremultipliedColorF::WHITE);
                request.push(PremultipliedColorF::WHITE);
                request.push([
                    local_rect.size.width,
                    local_rect.size.height,
                    0.0,
                    0.0,
                ]);
            }
            BrushKind::YuvImage { color_depth, .. } => {
                request.push([
                    color_depth.rescaling_factor(),
                    0.0,
                    0.0,
                    0.0
                ]);
            }
            // Images are drawn as a white color, modulated by the total
            // opacity coming from any collapsed property bindings.
            BrushKind::Image { stretch_size, tile_spacing, color, .. } => {
                request.push(color.premultiplied());
                request.push(PremultipliedColorF::WHITE);
                request.push([
                    stretch_size.width + tile_spacing.width,
                    stretch_size.height + tile_spacing.height,
                    0.0,
                    0.0,
                ]);
            }
            // Solid rects also support opacity collapsing.
            BrushKind::Solid { ref color, .. } => {
                request.push(color.premultiplied());
            }
            BrushKind::LinearGradient { stretch_size, start_point, end_point, extend_mode, .. } => {
                request.push([
                    start_point.x,
                    start_point.y,
                    end_point.x,
                    end_point.y,
                ]);
                request.push([
                    pack_as_float(extend_mode as u32),
                    stretch_size.width,
                    stretch_size.height,
                    0.0,
                ]);
            }
            BrushKind::RadialGradient { stretch_size, center, start_radius, end_radius, ratio_xy, extend_mode, .. } => {
                request.push([
                    center.x,
                    center.y,
                    start_radius,
                    end_radius,
                ]);
                request.push([
                    ratio_xy,
                    pack_as_float(extend_mode as u32),
                    stretch_size.width,
                    stretch_size.height,
                ]);
            }
        }
    }
}

// Key that identifies a unique (partial) image that is being
// stored in the render task cache.
#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ImageCacheKey {
    pub request: ImageRequest,
    pub texel_rect: Option<DeviceIntRect>,
}

#[derive(Clone, Debug, Hash, PartialEq, Eq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct LineDecorationCacheKey {
    style: LineStyle,
    orientation: LineOrientation,
    wavy_line_thickness: Au,
    size: LayoutSizeAu,
}

// Where to find the texture data for an image primitive.
#[derive(Debug)]
pub enum ImageSource {
    // A normal image - just reference the texture cache.
    Default,
    // An image that is pre-rendered into the texture cache
    // via a render task.
    Cache {
        size: DeviceIntSize,
        handle: Option<RenderTaskCacheEntryHandle>,
    },
}

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

struct GradientGpuBlockBuilder<'a> {
    stops_range: ItemRange<GradientStop>,
    display_list: &'a BuiltDisplayList,
}

impl<'a> GradientGpuBlockBuilder<'a> {
    fn new(
        stops_range: ItemRange<GradientStop>,
        display_list: &'a BuiltDisplayList,
    ) -> Self {
        GradientGpuBlockBuilder {
            stops_range,
            display_list,
        }
    }

    /// Generate a color ramp filling the indices in [start_idx, end_idx) and interpolating
    /// from start_color to end_color.
    fn fill_colors(
        &self,
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
    fn build(&self, reverse_stops: bool, request: &mut GpuDataRequest) {
        let src_stops = self.display_list.get(self.stops_range);

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
            self.fill_colors(
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
                    self.fill_colors(next_idx, cur_idx, &next_color, &cur_color, &mut entries);
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            if cur_idx != GRADIENT_DATA_TABLE_BEGIN {
                error!("Gradient stops abruptly at {}, auto-completing to white", cur_idx);
                self.fill_colors(GRADIENT_DATA_TABLE_BEGIN, cur_idx, &PremultipliedColorF::WHITE, &cur_color, &mut entries);
            }

            // Fill in the last entry (for reversed stops) with the last color stop
            self.fill_colors(
                GRADIENT_DATA_FIRST_STOP,
                GRADIENT_DATA_FIRST_STOP + 1,
                &cur_color,
                &cur_color,
                &mut entries,
            );
        } else {
            // Fill in the first entry with the first color stop
            self.fill_colors(
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
                    self.fill_colors(cur_idx, next_idx, &cur_color, &next_color, &mut entries);
                    cur_idx = next_idx;
                }

                cur_color = next_color;
            }
            if cur_idx != GRADIENT_DATA_TABLE_END {
                error!("Gradient stops abruptly at {}, auto-completing to white", cur_idx);
                self.fill_colors(cur_idx, GRADIENT_DATA_TABLE_END, &PremultipliedColorF::WHITE, &cur_color, &mut entries);
            }

            // Fill in the last entry with the last color stop
            self.fill_colors(
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

#[derive(Debug, Clone)]
pub struct TextRunPrimitive {
    pub used_font: FontInstance,
    pub glyph_keys: Vec<GlyphKey>,
    pub shadow: bool,
}

impl TextRunPrimitive {
    pub fn update_font_instance(
        &mut self,
        specified_font: &FontInstance,
        device_pixel_scale: DevicePixelScale,
        transform: &LayoutToWorldTransform,
        allow_subpixel_aa: bool,
        raster_space: RasterSpace,
    ) -> bool {
        // Get the current font size in device pixels
        let device_font_size = specified_font.size.scale_by(device_pixel_scale.0);

        // Determine if rasterizing glyphs in local or screen space.
        // Only support transforms that can be coerced to simple 2D transforms.
        let transform_glyphs = if transform.has_perspective_component() ||
           !transform.has_2d_inverse() ||
           // Font sizes larger than the limit need to be scaled, thus can't use subpixels.
           transform.exceeds_2d_scale(FONT_SIZE_LIMIT / device_font_size.to_f64_px()) ||
           // Otherwise, ensure the font is rasterized in screen-space.
           raster_space != RasterSpace::Screen {
            false
        } else {
            true
        };

        // Get the font transform matrix (skew / scale) from the complete transform.
        let font_transform = if transform_glyphs {
            // Quantize the transform to minimize thrashing of the glyph cache.
            FontTransform::from(transform).quantize()
        } else {
            FontTransform::identity()
        };

        // If the transform or device size is different, then the caller of
        // this method needs to know to rebuild the glyphs.
        let cache_dirty =
            self.used_font.transform != font_transform ||
            self.used_font.size != device_font_size;

        // Construct used font instance from the specified font instance
        self.used_font = FontInstance {
            transform: font_transform,
            size: device_font_size,
            ..specified_font.clone()
        };

        // If subpixel AA is disabled due to the backing surface the glyphs
        // are being drawn onto, disable it (unless we are using the
        // specifial subpixel mode that estimates background color).
        if !allow_subpixel_aa && self.used_font.bg_color.a == 0 {
            self.used_font.disable_subpixel_aa();
        }

        // If using local space glyphs, we don't want subpixel AA
        // or positioning.
        if !transform_glyphs {
            self.used_font.disable_subpixel_aa();
            self.used_font.disable_subpixel_position();
        }

        cache_dirty
    }

    fn prepare_for_render(
        &mut self,
        specified_font: &FontInstance,
        glyphs: &[GlyphInstance],
        device_pixel_scale: DevicePixelScale,
        transform: &LayoutToWorldTransform,
        allow_subpixel_aa: bool,
        raster_space: RasterSpace,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        render_tasks: &mut RenderTaskTree,
        special_render_passes: &mut SpecialRenderPasses,
    ) {
        let cache_dirty = self.update_font_instance(
            specified_font,
            device_pixel_scale,
            transform,
            allow_subpixel_aa,
            raster_space,
        );

        if self.glyph_keys.is_empty() || cache_dirty {
            let subpx_dir = self.used_font.get_subpx_dir();

            for src in glyphs {
                let world_offset = self.used_font.transform.transform(&src.point);
                let device_offset = device_pixel_scale.transform_point(&world_offset);
                let key = GlyphKey::new(src.index, device_offset, subpx_dir);
                self.glyph_keys.push(key);
            }
        }

        resource_cache.request_glyphs(
            self.used_font.clone(),
            &self.glyph_keys,
            gpu_cache,
            render_tasks,
            special_render_passes,
        );
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
    /// The local rect of the whole masked area.
    pub local_mask_rect: LayoutRect,
    /// The local rect of an individual tile.
    pub local_tile_rect: LayoutRect,
}

impl ToGpuBlocks for ImageMaskData {
    fn write_gpu_blocks(&self, mut request: GpuDataRequest) {
        request.push(self.local_mask_rect);
        request.push(self.local_tile_rect);
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
    pub fn rounded_rect(rect: &LayoutRect, radii: &BorderRadius, mode: ClipMode) -> ClipData {
        ClipData {
            rect: ClipRect {
                rect: *rect,
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

    pub fn uniform(rect: LayoutRect, radius: f32, mode: ClipMode) -> ClipData {
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

pub enum PrimitiveContainer {
    TextRun {
        font: FontInstance,
        offset: LayoutVector2D,
        glyphs: Vec<GlyphInstance>,
        shadow: bool,
    },
    Clear,
    Brush(BrushPrimitive),
    LineDecoration {
        color: ColorF,
        style: LineStyle,
        orientation: LineOrientation,
        wavy_line_thickness: f32,
    },
}

impl PrimitiveContainer {
    // Return true if the primary primitive is visible.
    // Used to trivially reject non-visible primitives.
    // TODO(gw): Currently, primitives other than those
    //           listed here are handled before the
    //           add_primitive() call. In the future
    //           we should move the logic for all other
    //           primitive types to use this.
    pub fn is_visible(&self) -> bool {
        match *self {
            PrimitiveContainer::TextRun { ref font, .. } => {
                font.color.a > 0
            }
            PrimitiveContainer::Brush(ref brush) => {
                match brush.kind {
                    BrushKind::Solid { ref color, .. } => {
                        color.a > 0.0
                    }
                    BrushKind::Image { .. } |
                    BrushKind::YuvImage { .. } |
                    BrushKind::RadialGradient { .. } |
                    BrushKind::Border { .. } |
                    BrushKind::LinearGradient { .. } => {
                        true
                    }
                }
            }
            PrimitiveContainer::Clear => {
                true
            }
            PrimitiveContainer::LineDecoration { ref color, .. } => {
                color.a > 0.0
            }
        }
    }

    /// Convert a source primitive container into a key, and optionally
    /// an old style PrimitiveDetails structure.
    pub fn build(
        self,
        info: &mut LayoutPrimitiveInfo,
    ) -> (PrimitiveKeyKind, Option<PrimitiveDetails>) {
        match self {
            PrimitiveContainer::TextRun { font, offset, glyphs, shadow, .. } => {
                let key = PrimitiveKeyKind::TextRun {
                    font,
                    offset: offset.to_au(),
                    glyphs,
                    shadow,
                };

                (key, None)
            }
            PrimitiveContainer::Clear => {
                (PrimitiveKeyKind::Clear, None)
            }
            PrimitiveContainer::LineDecoration { color, style, orientation, wavy_line_thickness } => {
                // For line decorations, we can construct the render task cache key
                // here during scene building, since it doesn't depend on device
                // pixel ratio or transform.

                let size = get_line_decoration_sizes(
                    &info.rect.size,
                    orientation,
                    style,
                    wavy_line_thickness,
                );

                let cache_key = size.map(|(inline_size, block_size)| {
                    let size = match orientation {
                        LineOrientation::Horizontal => LayoutSize::new(inline_size, block_size),
                        LineOrientation::Vertical => LayoutSize::new(block_size, inline_size),
                    };

                    // If dotted, adjust the clip rect to ensure we don't draw a final
                    // partial dot.
                    if style == LineStyle::Dotted {
                        let clip_size = match orientation {
                            LineOrientation::Horizontal => {
                                LayoutSize::new(
                                    inline_size * (info.rect.size.width / inline_size).floor(),
                                    info.rect.size.height,
                                )
                            }
                            LineOrientation::Vertical => {
                                LayoutSize::new(
                                    info.rect.size.width,
                                    inline_size * (info.rect.size.height / inline_size).floor(),
                                )
                            }
                        };
                        let clip_rect = LayoutRect::new(
                            info.rect.origin,
                            clip_size,
                        );
                        info.clip_rect = clip_rect
                            .intersection(&info.clip_rect)
                            .unwrap_or(LayoutRect::zero());
                    }

                    LineDecorationCacheKey {
                        style,
                        orientation,
                        wavy_line_thickness: Au::from_f32_px(wavy_line_thickness),
                        size: size.to_au(),
                    }
                });

                let key = PrimitiveKeyKind::LineDecoration {
                    cache_key,
                    color: color.into(),
                };

                (key, None)
            }
            PrimitiveContainer::Brush(prim) => {
                (PrimitiveKeyKind::Unused, Some(PrimitiveDetails::Brush(prim)))
            }
        }
    }

    // Create a clone of this PrimitiveContainer, applying whatever
    // changes are necessary to the primitive to support rendering
    // it as part of the supplied shadow.
    pub fn create_shadow(
        &self,
        shadow: &Shadow,
        prim_rect: &LayoutRect,
    ) -> PrimitiveContainer {
        match *self {
            PrimitiveContainer::TextRun { ref font, offset, ref glyphs, .. } => {
                let mut font = FontInstance {
                    color: shadow.color.into(),
                    ..font.clone()
                };
                if shadow.blur_radius > 0.0 {
                    font.disable_subpixel_aa();
                }

                PrimitiveContainer::TextRun {
                    font,
                    glyphs: glyphs.clone(),
                    offset: offset + shadow.offset,
                    shadow: true,
                }
            }
            PrimitiveContainer::LineDecoration { style, orientation, wavy_line_thickness, .. } => {
                PrimitiveContainer::LineDecoration {
                    color: shadow.color,
                    style,
                    orientation,
                    wavy_line_thickness,
                }
            }
            PrimitiveContainer::Brush(ref brush) => {
                match brush.kind {
                    BrushKind::Solid { .. } => {
                        PrimitiveContainer::Brush(BrushPrimitive::new(
                            BrushKind::new_solid(shadow.color),
                            None,
                        ))
                    }
                    BrushKind::Border { ref source } => {
                        let prim = match *source {
                            BorderSource::Image(request) => {
                                BrushPrimitive::new(
                                    BrushKind::Border {
                                        source: BorderSource::Image(request)
                                    },
                                    None,
                                )
                            }
                            BorderSource::Border { border, widths, .. } => {
                                let border = border.with_color(shadow.color);
                                create_normal_border_prim(
                                    prim_rect,
                                    border,
                                    widths,
                                )
                            }
                        };
                        PrimitiveContainer::Brush(prim)
                    }
                    BrushKind::Image { request, stretch_size, .. } => {
                        PrimitiveContainer::Brush(BrushPrimitive::new(
                            BrushKind::new_image(request.clone(),
                                                 stretch_size.clone(),
                                                 shadow.color),
                            None,
                        ))
                    }
                    BrushKind::YuvImage { .. } |
                    BrushKind::RadialGradient { .. } |
                    BrushKind::LinearGradient { .. } => {
                        panic!("bug: other brush kinds not expected here yet");
                    }
                }
            }
            PrimitiveContainer::Clear => {
                panic!("bug: clear rects are not supported in shadow contexts");
            }
        }
    }
}

pub enum PrimitiveDetails {
    Brush(BrushPrimitive),
}

pub struct Primitive {
    pub local_rect: LayoutRect,
    pub local_clip_rect: LayoutRect,
    pub details: PrimitiveDetails,
}

#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimitiveDebugId(pub usize);

#[derive(Clone, Debug)]
pub enum PrimitiveInstanceKind {
    /// Direct reference to a Picture
    Picture {
        pic_index: PictureIndex,
    },
    /// An old style, non-interned primitive. Uses prim_index to
    /// access the primitive details in the prim_store.
    LegacyPrimitive {
        prim_index: PrimitiveIndex,
    },
    /// A run of glyphs, with associated font parameters.
    TextRun {
        run_index: TextRunIndex,
    },
    /// A line decoration. cache_handle refers to a cached render
    /// task handle, if this line decoration is not a simple solid.
    LineDecoration {
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
    /// Clear out a rect, used for special effects.
    Clear,
}

#[derive(Clone, Debug)]
pub struct PrimitiveInstance {
    /// Identifies the kind of primitive this
    /// instance is, and references to where
    /// the relevant information for the primitive
    /// can be found.
    pub kind: PrimitiveInstanceKind,

    /// Handle to the common interned data for this primitive.
    pub prim_data_handle: PrimitiveDataHandle,

    /// The current combined local clip for this primitive, from
    /// the primitive local clip above and the current clip chain.
    pub combined_local_clip_rect: LayoutRect,

    #[cfg(debug_assertions)]
    pub id: PrimitiveDebugId,

    /// The last frame ID (of the `RenderTaskTree`) this primitive
    /// was prepared for rendering in.
    #[cfg(debug_assertions)]
    pub prepared_frame_id: FrameId,

    /// The current world rect of this primitive, clipped to the
    /// world rect of the screen. None means the primitive is
    /// completely off-screen.
    pub clipped_world_rect: Option<WorldRect>,

    /// If this primitive has a global clip mask, this identifies
    /// the render task for it.
    pub clip_task_id: Option<RenderTaskId>,

    /// The main GPU cache handle that this primitive uses to
    /// store data accessible to shaders. This should be moved
    /// into the interned data in order to retain this between
    /// display list changes, but needs to be split into shared
    /// and per-instance data.
    pub gpu_location: GpuCacheHandle,

    /// ID of the clip chain that this primitive is clipped by.
    pub clip_chain_id: ClipChainId,

    /// ID of the spatial node that this primitive is positioned by.
    pub spatial_node_index: SpatialNodeIndex,

    /// A range of clusters that this primitive instance belongs to.
    pub cluster_range: ClusterRange,
}

impl PrimitiveInstance {
    pub fn new(
        kind: PrimitiveInstanceKind,
        prim_data_handle: PrimitiveDataHandle,
        clip_chain_id: ClipChainId,
        spatial_node_index: SpatialNodeIndex,
    ) -> Self {
        PrimitiveInstance {
            kind,
            prim_data_handle,
            combined_local_clip_rect: LayoutRect::zero(),
            clipped_world_rect: None,
            #[cfg(debug_assertions)]
            prepared_frame_id: FrameId::invalid(),
            #[cfg(debug_assertions)]
            id: PrimitiveDebugId(NEXT_PRIM_ID.fetch_add(1, Ordering::Relaxed)),
            clip_task_id: None,
            gpu_location: GpuCacheHandle::new(),
            clip_chain_id,
            spatial_node_index,
            cluster_range: ClusterRange { start: 0, end: 0 },
        }
    }

    #[cfg(debug_assertions)]
    pub fn is_chased(&self) -> bool {
        PRIM_CHASE_ID.load(Ordering::SeqCst) == self.id.0
    }

    #[cfg(not(debug_assertions))]
    pub fn is_chased(&self) -> bool {
        false
    }
}

pub struct PrimitiveStore {
    pub primitives: Vec<Primitive>,
    pub pictures: Vec<PicturePrimitive>,
    pub text_runs: Vec<TextRunPrimitive>,
}

impl PrimitiveStore {
    pub fn new() -> PrimitiveStore {
        PrimitiveStore {
            primitives: Vec::new(),
            pictures: Vec::new(),
            text_runs: Vec::new(),
        }
    }

    pub fn create_picture(
        &mut self,
        prim: PicturePrimitive,
    ) -> PictureIndex {
        let index = PictureIndex(self.pictures.len());
        self.pictures.push(prim);
        index
    }

    /// Update a picture, determining surface configuration,
    /// rasterization roots, and (in future) whether there
    /// are cached surfaces that can be used by this picture.
    pub fn update_picture(
        &mut self,
        pic_index: PictureIndex,
        context: &PictureUpdateContext,
        frame_context: &FrameBuildingContext,
        surfaces: &mut Vec<SurfaceInfo>,
    ) {
        if let Some((child_context, children)) = self.pictures[pic_index.0].pre_update(
            context,
            frame_context,
            surfaces,
        ) {
            for child_pic_index in &children {
                self.update_picture(
                    *child_pic_index,
                    &child_context,
                    frame_context,
                    surfaces,
                );
            }

            self.pictures[pic_index.0].post_update(
                context,
                &child_context,
                children,
                surfaces,
                frame_context,
            );
        }
    }

    pub fn add_primitive(
        &mut self,
        local_rect: &LayoutRect,
        local_clip_rect: &LayoutRect,
        details: PrimitiveDetails,
    ) -> PrimitiveIndex {
        let prim_index = self.primitives.len();

        let prim = Primitive {
            local_rect: *local_rect,
            local_clip_rect: *local_clip_rect,
            details,
        };

        self.primitives.push(prim);

        PrimitiveIndex(prim_index)
    }

    // Internal method that retrieves the primitive index of a primitive
    // that can be the target for collapsing parent opacity filters into.
    fn get_opacity_collapse_prim(
        &self,
        pic_index: PictureIndex,
    ) -> Option<PrimitiveIndex> {
        let pic = &self.pictures[pic_index.0];

        // We can only collapse opacity if there is a single primitive, otherwise
        // the opacity needs to be applied to the primitives as a group.
        if pic.prim_list.prim_instances.len() != 1 {
            return None;
        }

        let prim_instance = &pic.prim_list.prim_instances[0];

        // For now, we only support opacity collapse on solid rects and images.
        // This covers the most common types of opacity filters that can be
        // handled by this optimization. In the future, we can easily extend
        // this to other primitives, such as text runs and gradients.
        match prim_instance.kind {
            PrimitiveInstanceKind::Clear |
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                // TODO: Once rectangles and/or images are ported
                //       to use interned primitives, we will need
                //       to handle opacity collapse here.
            }
            PrimitiveInstanceKind::Picture { pic_index } => {
                let pic = &self.pictures[pic_index.0];

                // If we encounter a picture that is a pass-through
                // (i.e. no composite mode), then we can recurse into
                // that to try and find a primitive to collapse to.
                if pic.requested_composite_mode.is_none() {
                    return self.get_opacity_collapse_prim(pic_index);
                }
            }
            PrimitiveInstanceKind::LegacyPrimitive { prim_index } => {
                let prim = &self.primitives[prim_index.0];
                match prim.details {
                    PrimitiveDetails::Brush(ref brush) => {
                        match brush.kind {
                            // If we find a single rect or image, we can use that
                            // as the primitive to collapse the opacity into.
                            BrushKind::Solid { .. } | BrushKind::Image { .. } => {
                                return Some(prim_index)
                            }
                            BrushKind::Border { .. } |
                            BrushKind::YuvImage { .. } |
                            BrushKind::LinearGradient { .. } |
                            BrushKind::RadialGradient { .. } => {}
                        }
                    }
                }
            }
        }

        None
    }

    // Apply any optimizations to drawing this picture. Currently,
    // we just support collapsing pictures with an opacity filter
    // by pushing that opacity value into the color of a primitive
    // if that picture contains one compatible primitive.
    pub fn optimize_picture_if_possible(
        &mut self,
        pic_index: PictureIndex,
    ) {
        // Only handle opacity filters for now.
        let binding = match self.pictures[pic_index.0].requested_composite_mode {
            Some(PictureCompositeMode::Filter(FilterOp::Opacity(binding, _))) => {
                binding
            }
            _ => {
                return;
            }
        };

        // See if this picture contains a single primitive that supports
        // opacity collapse.
        match self.get_opacity_collapse_prim(pic_index) {
            Some(prim_index) => {
                let prim = &mut self.primitives[prim_index.0];
                match prim.details {
                    PrimitiveDetails::Brush(ref mut brush) => {
                        // By this point, we know we should only have found a primitive
                        // that supports opacity collapse.
                        match brush.kind {
                            BrushKind::Solid { ref mut opacity_binding, .. } |
                            BrushKind::Image { ref mut opacity_binding, .. } => {
                                opacity_binding.push(binding);
                            }
                            BrushKind::YuvImage { .. } |
                            BrushKind::Border { .. } |
                            BrushKind::LinearGradient { .. } |
                            BrushKind::RadialGradient { .. } => {
                                unreachable!("bug: invalid prim type for opacity collapse");
                            }
                        }
                    }
                }
            }
            None => {
                return;
            }
        }

        // The opacity filter has been collapsed, so mark this picture
        // as a pass though. This means it will no longer allocate an
        // intermediate surface or incur an extra blend / blit. Instead,
        // the collapsed primitive will be drawn directly into the
        // parent picture.
        self.pictures[pic_index.0].requested_composite_mode = None;
    }

    pub fn prim_count(&self) -> usize {
        self.primitives.len()
    }

    pub fn prepare_prim_for_render(
        &mut self,
        prim_instance: &mut PrimitiveInstance,
        prim_context: &PrimitiveContext,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        display_list: &BuiltDisplayList,
        plane_split_anchor: usize,
    ) -> bool {
        // If we have dependencies, we need to prepare them first, in order
        // to know the actual rect of this primitive.
        // For example, scrolling may affect the location of an item in
        // local space, which may force us to render this item on a larger
        // picture target, if being composited.
        let pic_info = {
            match prim_instance.kind {
                PrimitiveInstanceKind::Picture { pic_index } => {
                    let pic = &mut self.pictures[pic_index.0];

                    match pic.take_context(
                        pic_index,
                        pic_context.surface_spatial_node_index,
                        pic_context.raster_spatial_node_index,
                        pic_context.surface_index,
                        pic_context.allow_subpixel_aa,
                        frame_state,
                        frame_context,
                    ) {
                        Some(info) => Some(info),
                        None => {
                            if prim_instance.is_chased() {
                                println!("\tculled for carrying an invisible composite filter");
                            }

                            return false;
                        }
                    }
                }
                PrimitiveInstanceKind::TextRun { .. } |
                PrimitiveInstanceKind::LineDecoration { .. } |
                PrimitiveInstanceKind::LegacyPrimitive { .. } |
                PrimitiveInstanceKind::Clear => {
                    None
                }
            }
        };

        let (is_passthrough, clip_node_collector) = match pic_info {
            Some((pic_context_for_children, mut pic_state_for_children, mut prim_list)) => {
                // Mark whether this picture has a complex coordinate system.
                let is_passthrough = pic_context_for_children.is_passthrough;

                self.prepare_primitives(
                    &mut prim_list,
                    &pic_context_for_children,
                    &mut pic_state_for_children,
                    frame_context,
                    frame_state,
                );

                if !pic_state_for_children.is_cacheable {
                    pic_state.is_cacheable = false;
                }

                // Restore the dependencies (borrow check dance)
                let (local_rect_changed, clip_node_collector) = self
                    .pictures[pic_context_for_children.pic_index.0]
                    .restore_context(
                        prim_list,
                        pic_context_for_children,
                        pic_state_for_children,
                        frame_state,
                    );

                if local_rect_changed {
                    frame_state.gpu_cache.invalidate(&mut prim_instance.gpu_location);
                    pic_state.local_rect_changed = true;
                }

                (is_passthrough, clip_node_collector)
            }
            None => {
                (false, None)
            }
        };

        let (prim_local_rect, prim_local_clip_rect) = match prim_instance.kind {
            PrimitiveInstanceKind::Picture { pic_index } => {
                let pic = &self.pictures[pic_index.0];
                (pic.local_rect, LayoutRect::max_rect())
            }
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::Clear |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                let prim_data = &frame_state
                    .resources
                    .prim_data_store[prim_instance.prim_data_handle];
                (prim_data.prim_rect, prim_data.clip_rect)
            }
            PrimitiveInstanceKind::LegacyPrimitive { prim_index } => {
                let prim = &self.primitives[prim_index.0];
                (prim.local_rect, prim.local_clip_rect)
            }
        };

        if is_passthrough {
            prim_instance.clipped_world_rect = Some(pic_state.map_pic_to_world.bounds);
        } else {
            if prim_local_rect.size.width <= 0.0 ||
               prim_local_rect.size.height <= 0.0
            {
                if prim_instance.is_chased() {
                    println!("\tculled for zero local rectangle");
                }
                return false;
            }

            // Inflate the local rect for this primitive by the inflation factor of
            // the picture context. This ensures that even if the primitive itself
            // is not visible, any effects from the blur radius will be correctly
            // taken into account.
            let local_rect = prim_local_rect
                .inflate(pic_context.inflation_factor, pic_context.inflation_factor)
                .intersection(&prim_local_clip_rect);
            let local_rect = match local_rect {
                Some(local_rect) => local_rect,
                None => {
                    if prim_instance.is_chased() {
                        println!("\tculled for being out of the local clip rectangle: {:?}",
                            prim_local_clip_rect);
                    }
                    return false;
                }
            };

            let clip_chain = frame_state
                .clip_store
                .build_clip_chain_instance(
                    prim_instance.clip_chain_id,
                    local_rect,
                    prim_local_clip_rect,
                    prim_context.spatial_node_index,
                    &pic_state.map_local_to_pic,
                    &pic_state.map_pic_to_world,
                    &frame_context.clip_scroll_tree,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_context.device_pixel_scale,
                    &frame_context.world_rect,
                    &clip_node_collector,
                    &mut frame_state.resources.clip_data_store,
                );

            let clip_chain = match clip_chain {
                Some(clip_chain) => clip_chain,
                None => {
                    if prim_instance.is_chased() {
                        println!("\tunable to build the clip chain, skipping");
                    }
                    prim_instance.clipped_world_rect = None;
                    return false;
                }
            };

            if prim_instance.is_chased() {
                println!("\teffective clip chain from {:?} {}",
                    clip_chain.clips_range,
                    if pic_context.apply_local_clip_rect { "(applied)" } else { "" },
                );
            }

            prim_instance.combined_local_clip_rect = if pic_context.apply_local_clip_rect {
                clip_chain.local_clip_rect
            } else {
                prim_local_clip_rect
            };

            // Check if the clip bounding rect (in pic space) is visible on screen
            // This includes both the prim bounding rect + local prim clip rect!
            let world_rect = match pic_state
                .map_pic_to_world
                .map(&clip_chain.pic_clip_rect)
            {
                Some(world_rect) => world_rect,
                None => {
                    return false;
                }
            };

            let clipped_world_rect = match world_rect.intersection(&frame_context.world_rect) {
                Some(rect) => rect,
                None => {
                    return false;
                }
            };

            prim_instance.clipped_world_rect = Some(clipped_world_rect);

            prim_instance.update_clip_task(
                prim_local_rect,
                prim_local_clip_rect,
                prim_context,
                clipped_world_rect,
                pic_context.raster_spatial_node_index,
                &clip_chain,
                pic_context.surface_index,
                pic_state,
                frame_context,
                frame_state,
                &clip_node_collector,
                &mut self.primitives,
            );

            if prim_instance.is_chased() {
                println!("\tconsidered visible and ready with local rect {:?}", local_rect);
            }
        }

        #[cfg(debug_assertions)]
        {
            prim_instance.prepared_frame_id = frame_state.render_tasks.frame_id();
        }

        match prim_instance.kind {
            PrimitiveInstanceKind::Picture { pic_index } => {
                let pic = &mut self.pictures[pic_index.0];
                if pic.prepare_for_render(
                    pic_index,
                    prim_instance,
                    &prim_local_rect,
                    pic_context.surface_index,
                    pic_state,
                    frame_context,
                    frame_state,
                ) {
                    if let Some(ref mut splitter) = pic_state.plane_splitter {
                        PicturePrimitive::add_split_plane(
                            splitter,
                            frame_state.transforms,
                            prim_instance,
                            prim_local_rect,
                            plane_split_anchor,
                        );
                    }
                } else {
                    prim_instance.clipped_world_rect = None;
                }

                if let Some(mut request) = frame_state.gpu_cache.request(&mut prim_instance.gpu_location) {
                    request.push(PremultipliedColorF::WHITE);
                    request.push(PremultipliedColorF::WHITE);
                    request.push([
                        pic.local_rect.size.width,
                        pic.local_rect.size.height,
                        0.0,
                        0.0,
                    ]);
                    request.write_segment(
                        pic.local_rect,
                        [0.0; 4],
                    );
                }
            }
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::Clear |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                prim_instance.prepare_interned_prim_for_render(
                    prim_context,
                    pic_context,
                    frame_context,
                    frame_state,
                    &mut self.text_runs,
                );
            }
            PrimitiveInstanceKind::LegacyPrimitive { prim_index } => {
                let prim_details = &mut self.primitives[prim_index.0].details;

                prim_instance.prepare_prim_for_render_inner(
                    prim_local_rect,
                    prim_details,
                    prim_context,
                    pic_context.surface_index,
                    pic_state,
                    frame_context,
                    frame_state,
                    display_list,
                );
            }
        }

        true
    }

    pub fn prepare_primitives(
        &mut self,
        prim_list: &mut PrimitiveList,
        pic_context: &PictureContext,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
    ) {
        let display_list = &frame_context
            .pipelines
            .get(&pic_context.pipeline_id)
            .expect("No display list?")
            .display_list;

        for (plane_split_anchor, prim_instance) in prim_list.prim_instances.iter_mut().enumerate() {
            prim_instance.clipped_world_rect = None;

            if prim_instance.is_chased() {
                #[cfg(debug_assertions)]
                println!("\tpreparing {:?} in {:?}",
                    prim_instance.id, pic_context.pipeline_id);
            }

            // Run through the list of cluster(s) this primitive belongs
            // to. As soon as we find one visible cluster that this
            // primitive belongs to, then the primitive itself can be
            // considered visible.
            // TODO(gw): Initially, primitive clusters are only used
            //           to group primitives by backface visibility and
            //           whether a spatial node is invertible or not.
            //           In the near future, clusters will also act as
            //           a simple spatial hash for grouping.
            // TODO(gw): For now, walk the primitive list and check if
            //           it is visible in any clusters, as this is a
            //           simple way to retain correct render order. In
            //           the future, it might make sense to invert this
            //           and have the cluster visibility pass produce
            //           an index buffer / set of primitive instances
            //           that we sort into render order.
            let mut in_visible_cluster = false;
            for ci in prim_instance.cluster_range.start .. prim_instance.cluster_range.end {
                // Map from the cluster range index to a cluster index
                let cluster_index = prim_list.prim_cluster_map[ci as usize];

                // Get the cluster and see if is visible
                let cluster = &prim_list.clusters[cluster_index.0 as usize];
                in_visible_cluster |= cluster.is_visible;

                // As soon as a primitive is in a visible cluster, it's considered
                // visible and we don't need to consult other clusters.
                if cluster.is_visible {
                    break;
                }
            }

            // If the primitive wasn't in any visible clusters, it can be skipped.
            if !in_visible_cluster {
                continue;
            }

            let spatial_node = &frame_context
                .clip_scroll_tree
                .spatial_nodes[prim_instance.spatial_node_index.0];

            // TODO(gw): Although constructing these is cheap, they are often
            //           the same for many consecutive primitives, so it may
            //           be worth caching the most recent context.
            let prim_context = PrimitiveContext::new(
                spatial_node,
                prim_instance.spatial_node_index,
            );

            pic_state.map_local_to_pic.set_target_spatial_node(
                prim_instance.spatial_node_index,
                frame_context.clip_scroll_tree,
            );

            if self.prepare_prim_for_render(
                prim_instance,
                &prim_context,
                pic_context,
                pic_state,
                frame_context,
                frame_state,
                display_list,
                plane_split_anchor,
            ) {
                frame_state.profile_counters.visible_primitives.inc();
            }
        }
    }
}

fn build_gradient_stops_request(
    stops_handle: &mut GpuCacheHandle,
    stops_range: ItemRange<GradientStop>,
    reverse_stops: bool,
    frame_state: &mut FrameBuildingState,
    display_list: &BuiltDisplayList,
) {
    if let Some(mut request) = frame_state.gpu_cache.request(stops_handle) {
        let gradient_builder = GradientGpuBlockBuilder::new(
            stops_range,
            display_list,
        );
        gradient_builder.build(
            reverse_stops,
            &mut request,
        );
    }
}

fn decompose_repeated_primitive(
    visible_tiles: &mut Vec<VisibleGradientTile>,
    instance: &mut PrimitiveInstance,
    prim_local_rect: &LayoutRect,
    stretch_size: &LayoutSize,
    tile_spacing: &LayoutSize,
    prim_context: &PrimitiveContext,
    frame_state: &mut FrameBuildingState,
    callback: &mut FnMut(&LayoutRect, GpuDataRequest),
) {
    visible_tiles.clear();

    // Tighten the clip rect because decomposing the repeated image can
    // produce primitives that are partially covering the original image
    // rect and we want to clip these extra parts out.
    let tight_clip_rect = instance
        .combined_local_clip_rect
        .intersection(prim_local_rect).unwrap();

    let clipped_world_rect = &instance
        .clipped_world_rect
        .unwrap();

    let visible_rect = compute_conservative_visible_rect(
        prim_context,
        clipped_world_rect,
        &tight_clip_rect
    );
    let stride = *stretch_size + *tile_spacing;

    let repetitions = image::repetitions(prim_local_rect, &visible_rect, stride);
    for Repetition { origin, .. } in repetitions {
        let mut handle = GpuCacheHandle::new();
        let rect = LayoutRect {
            origin: origin,
            size: *stretch_size,
        };

        if let Some(request) = frame_state.gpu_cache.request(&mut handle) {
            callback(&rect, request);
        }

        visible_tiles.push(VisibleGradientTile {
            local_rect: rect,
            local_clip_rect: tight_clip_rect,
            handle
        });
    }

    if visible_tiles.is_empty() {
        // At this point if we don't have tiles to show it means we could probably
        // have done a better a job at culling during an earlier stage.
        // Clearing the screen rect has the effect of "culling out" the primitive
        // from the point of view of the batch builder, and ensures we don't hit
        // assertions later on because we didn't request any image.
        instance.clipped_world_rect = None;
    }
}

fn compute_conservative_visible_rect(
    prim_context: &PrimitiveContext,
    clipped_world_rect: &WorldRect,
    local_clip_rect: &LayoutRect,
) -> LayoutRect {
    if let Some(layer_screen_rect) = prim_context
        .spatial_node
        .world_content_transform
        .unapply(clipped_world_rect) {

        return local_clip_rect.intersection(&layer_screen_rect).unwrap_or(LayoutRect::zero());
    }

    *local_clip_rect
}

fn edge_flags_for_tile_spacing(tile_spacing: &LayoutSize) -> EdgeAaSegmentMask {
    let mut flags = EdgeAaSegmentMask::empty();

    if tile_spacing.width > 0.0 {
        flags |= EdgeAaSegmentMask::LEFT | EdgeAaSegmentMask::RIGHT;
    }
    if tile_spacing.height > 0.0 {
        flags |= EdgeAaSegmentMask::TOP | EdgeAaSegmentMask::BOTTOM;
    }

    flags
}

impl<'a> GpuDataRequest<'a> {
    // Write the GPU cache data for an individual segment.
    fn write_segment(
        &mut self,
        local_rect: LayoutRect,
        extra_data: [f32; 4],
    ) {
        let _ = VECS_PER_SEGMENT;
        self.push(local_rect);
        self.push(extra_data);
    }
}

impl BrushPrimitive {
    fn write_brush_segment_description(
        &mut self,
        prim_local_rect: LayoutRect,
        prim_local_clip_rect: LayoutRect,
        clip_chain: &ClipChainInstance,
        frame_state: &mut FrameBuildingState,
    ) {
        match self.segment_desc {
            Some(..) => {
                // If we already have a segment descriptor, skip segment build.
                return;
            }
            None => {
                // If no segment descriptor built yet, see if it is a brush
                // type that wants to be segmented.
                if !self.kind.supports_segments(frame_state.resource_cache) {
                    return;
                }
            }
        }

        // If the brush is small, we generally want to skip building segments
        // and just draw it as a single primitive with clip mask. However,
        // if the clips are purely rectangles that have no per-fragment
        // clip masks, we will segment anyway. This allows us to completely
        // skip allocating a clip mask in these cases.
        let is_large = prim_local_rect.size.area() > MIN_BRUSH_SPLIT_AREA;

        // TODO(gw): We should probably detect and store this on each
        //           ClipSources instance, to avoid having to iterate
        //           the clip sources here.
        let mut rect_clips_only = true;

        let segment_builder = &mut frame_state.segment_builder;
        segment_builder.initialize(
            prim_local_rect,
            None,
            prim_local_clip_rect
        );

        // Segment the primitive on all the local-space clip sources that we can.
        let mut local_clip_count = 0;
        for i in 0 .. clip_chain.clips_range.count {
            let clip_instance = frame_state
                .clip_store
                .get_instance_from_range(&clip_chain.clips_range, i);
            let clip_node = &frame_state.resources.clip_data_store[clip_instance.handle];

            // If this clip item is positioned by another positioning node, its relative position
            // could change during scrolling. This means that we would need to resegment. Instead
            // of doing that, only segment with clips that have the same positioning node.
            // TODO(mrobinson, #2858): It may make sense to include these nodes, resegmenting only
            // when necessary while scrolling.
            if !clip_instance.flags.contains(ClipNodeFlags::SAME_SPATIAL_NODE) {
                continue;
            }

            local_clip_count += 1;

            let (local_clip_rect, radius, mode) = match clip_node.item {
                ClipItem::RoundedRectangle(rect, radii, clip_mode) => {
                    rect_clips_only = false;
                    (rect, Some(radii), clip_mode)
                }
                ClipItem::Rectangle(rect, mode) => {
                    (rect, None, mode)
                }
                ClipItem::BoxShadow(ref info) => {
                    rect_clips_only = false;

                    // For inset box shadows, we can clip out any
                    // pixels that are inside the shadow region
                    // and are beyond the inner rect, as they can't
                    // be affected by the blur radius.
                    let inner_clip_mode = match info.clip_mode {
                        BoxShadowClipMode::Outset => None,
                        BoxShadowClipMode::Inset => Some(ClipMode::ClipOut),
                    };

                    // Push a region into the segment builder where the
                    // box-shadow can have an effect on the result. This
                    // ensures clip-mask tasks get allocated for these
                    // pixel regions, even if no other clips affect them.
                    segment_builder.push_mask_region(
                        info.prim_shadow_rect,
                        info.prim_shadow_rect.inflate(
                            -0.5 * info.shadow_rect_alloc_size.width,
                            -0.5 * info.shadow_rect_alloc_size.height,
                        ),
                        inner_clip_mode,
                    );

                    continue;
                }
                ClipItem::Image { .. } => {
                    rect_clips_only = false;
                    continue;
                }
            };

            segment_builder.push_clip_rect(local_clip_rect, radius, mode);
        }

        if is_large || rect_clips_only {
            // If there were no local clips, then we will subdivide the primitive into
            // a uniform grid (up to 8x8 segments). This will typically result in
            // a significant number of those segments either being completely clipped,
            // or determined to not need a clip mask for that segment.
            if local_clip_count == 0 && clip_chain.clips_range.count > 0 {
                let x_clip_count = cmp::min(8, (prim_local_rect.size.width / 128.0).ceil() as i32);
                let y_clip_count = cmp::min(8, (prim_local_rect.size.height / 128.0).ceil() as i32);

                for y in 0 .. y_clip_count {
                    let y0 = prim_local_rect.size.height * y as f32 / y_clip_count as f32;
                    let y1 = prim_local_rect.size.height * (y+1) as f32 / y_clip_count as f32;

                    for x in 0 .. x_clip_count {
                        let x0 = prim_local_rect.size.width * x as f32 / x_clip_count as f32;
                        let x1 = prim_local_rect.size.width * (x+1) as f32 / x_clip_count as f32;

                        let rect = LayoutRect::new(
                            LayoutPoint::new(
                                x0 + prim_local_rect.origin.x,
                                y0 + prim_local_rect.origin.y,
                            ),
                            LayoutSize::new(
                                x1 - x0,
                                y1 - y0,
                            ),
                        );

                        segment_builder.push_mask_region(rect, LayoutRect::zero(), None);
                    }
                }
            }

            match self.segment_desc {
                Some(..) => panic!("bug: should not already have descriptor"),
                None => {
                    // TODO(gw): We can probably make the allocation
                    //           patterns of this and the segment
                    //           builder significantly better, by
                    //           retaining it across primitives.
                    let mut segments = BrushSegmentVec::new();

                    segment_builder.build(|segment| {
                        segments.push(
                            BrushSegment::new(
                                segment.rect,
                                segment.has_mask,
                                segment.edge_flags,
                                [0.0; 4],
                                BrushFlags::empty(),
                            ),
                        );
                    });

                    self.segment_desc = Some(BrushSegmentDescriptor {
                        segments,
                    });
                }
            }
        }
    }
}

impl PrimitiveInstance {
    fn update_clip_task_for_brush(
        &mut self,
        prim_local_rect: LayoutRect,
        prim_local_clip_rect: LayoutRect,
        root_spatial_node_index: SpatialNodeIndex,
        prim_bounding_rect: WorldRect,
        prim_context: &PrimitiveContext,
        prim_clip_chain: &ClipChainInstance,
        surface_index: SurfaceIndex,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        clip_node_collector: &Option<ClipNodeCollector>,
        primitives: &mut [Primitive],
    ) -> bool {
        let brush = match self.kind {
            PrimitiveInstanceKind::Picture { .. } |
            PrimitiveInstanceKind::TextRun { .. } |
            PrimitiveInstanceKind::Clear |
            PrimitiveInstanceKind::LineDecoration { .. } => {
                return false;
            }
            PrimitiveInstanceKind::LegacyPrimitive { prim_index } => {
                let prim = &mut primitives[prim_index.0];
                match prim.details {
                    PrimitiveDetails::Brush(ref mut brush) => brush,
                }
            }
        };

        // Reset clip tasks from previous frame
        if let Some(ref mut desc) = brush.segment_desc {
            for segment in &mut desc.segments {
                segment.clip_task_id = BrushSegmentTaskId::Opaque;
            }
        }

        brush.write_brush_segment_description(
            prim_local_rect,
            prim_local_clip_rect,
            prim_clip_chain,
            frame_state,
        );

        let segment_desc = match brush.segment_desc {
            Some(ref mut description) => description,
            None => return false,
        };

        // If we only built 1 segment, there is no point in re-running
        // the clip chain builder. Instead, just use the clip chain
        // instance that was built for the main primitive. This is a
        // significant optimization for the common case.
        if segment_desc.segments.len() == 1 {
            segment_desc.segments[0].update_clip_task(
                Some(prim_clip_chain),
                prim_bounding_rect,
                root_spatial_node_index,
                surface_index,
                pic_state,
                frame_context,
                frame_state,
            );
        } else {
            for segment in &mut segment_desc.segments {
                // Build a clip chain for the smaller segment rect. This will
                // often manage to eliminate most/all clips, and sometimes
                // clip the segment completely.
                let segment_clip_chain = frame_state
                    .clip_store
                    .build_clip_chain_instance(
                        self.clip_chain_id,
                        segment.local_rect,
                        prim_local_clip_rect,
                        prim_context.spatial_node_index,
                        &pic_state.map_local_to_pic,
                        &pic_state.map_pic_to_world,
                        &frame_context.clip_scroll_tree,
                        frame_state.gpu_cache,
                        frame_state.resource_cache,
                        frame_context.device_pixel_scale,
                        &frame_context.world_rect,
                        clip_node_collector,
                        &mut frame_state.resources.clip_data_store,
                    );

                segment.update_clip_task(
                    segment_clip_chain.as_ref(),
                    prim_bounding_rect,
                    root_spatial_node_index,
                    surface_index,
                    pic_state,
                    frame_context,
                    frame_state,
                );
            }
        }

        true
    }

    /// Prepare an interned primitive for rendering, by requesting
    /// resources, render tasks etc. This is equivalent to the
    /// prepare_prim_for_render_inner call for old style primitives.
    fn prepare_interned_prim_for_render(
        &mut self,
        prim_context: &PrimitiveContext,
        pic_context: &PictureContext,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        text_runs: &mut [TextRunPrimitive],
    ) {
        let prim_data = &mut frame_state
            .resources
            .prim_data_store[self.prim_data_handle];

        // Update the template this instane references, which may refresh the GPU
        // cache with any shared template data.
        prim_data.update(
            frame_state.gpu_cache,
        );

        let is_chased = self.is_chased();

        match (&mut self.kind, &mut prim_data.kind) {
            (
                PrimitiveInstanceKind::LineDecoration { ref mut cache_handle, .. },
                PrimitiveTemplateKind::LineDecoration { ref cache_key, .. }
            ) => {
                // Work out the device pixel size to be used to cache this line decoration.
                if is_chased {
                    println!("\tline decoration key={:?}", cache_key);
                }

                // If we have a cache key, it's a wavy / dashed / dotted line. Otherwise, it's
                // a simple solid line.
                if let Some(cache_key) = cache_key {
                    // TODO(gw): Do we ever need / want to support scales for text decorations
                    //           based on the current transform?
                    let scale_factor = TypedScale::new(1.0) * frame_context.device_pixel_scale;
                    let task_size = (LayoutSize::from_au(cache_key.size) * scale_factor).ceil().to_i32();
                    let surfaces = &mut frame_state.surfaces;

                    // Request a pre-rendered image task.
                    // TODO(gw): This match is a bit untidy, but it should disappear completely
                    //           once the prepare_prims and batching are unified. When that
                    //           happens, we can use the cache handle immediately, and not need
                    //           to temporarily store it in the primitive instance.
                    *cache_handle = Some(frame_state.resource_cache.request_render_task(
                        RenderTaskCacheKey {
                            size: task_size,
                            kind: RenderTaskCacheKeyKind::LineDecoration(cache_key.clone()),
                        },
                        frame_state.gpu_cache,
                        frame_state.render_tasks,
                        None,
                        false,
                        |render_tasks| {
                            let task = RenderTask::new_line_decoration(
                                task_size,
                                cache_key.style,
                                cache_key.orientation,
                                cache_key.wavy_line_thickness.to_f32_px(),
                                LayoutSize::from_au(cache_key.size),
                            );
                            let task_id = render_tasks.add(task);
                            surfaces[pic_context.surface_index.0].tasks.push(task_id);
                            task_id
                        }
                    ));
                }
            }
            (
                PrimitiveInstanceKind::TextRun { run_index, .. },
                PrimitiveTemplateKind::TextRun { ref font, ref glyphs, .. }
            ) => {
                // The transform only makes sense for screen space rasterization
                let transform = prim_context.spatial_node.world_content_transform.to_transform();

                // TODO(gw): This match is a bit untidy, but it should disappear completely
                //           once the prepare_prims and batching are unified. When that
                //           happens, we can use the cache handle immediately, and not need
                //           to temporarily store it in the primitive instance.
                let run = &mut text_runs[run_index.0];
                run.prepare_for_render(
                    font,
                    glyphs,
                    frame_context.device_pixel_scale,
                    &transform,
                    pic_context.allow_subpixel_aa,
                    pic_context.raster_space,
                    frame_state.resource_cache,
                    frame_state.gpu_cache,
                    frame_state.render_tasks,
                    frame_state.special_render_passes,
                );
            }
            (
                PrimitiveInstanceKind::Clear,
                PrimitiveTemplateKind::Clear
            ) => {
                // Nothing specific to prepare for clear rects, since the
                // GPU cache is updated by the template earlier.
            }
            _ => {
                unreachable!();
            }
        }
    }

    fn prepare_prim_for_render_inner(
        &mut self,
        prim_local_rect: LayoutRect,
        prim_details: &mut PrimitiveDetails,
        prim_context: &PrimitiveContext,
        surface_index: SurfaceIndex,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        display_list: &BuiltDisplayList,
    ) {
        let mut is_tiled = false;

        pic_state.is_cacheable &= self.is_cacheable(
            prim_details,
            frame_state.resource_cache,
        );

        match *prim_details {
            PrimitiveDetails::Brush(ref mut brush) => {
                brush.opacity = match brush.kind {
                    BrushKind::Image {
                        request,
                        sub_rect,
                        stretch_size,
                        color,
                        ref mut tile_spacing,
                        ref mut source,
                        ref mut opacity_binding,
                        ref mut visible_tiles,
                        ..
                    } => {
                        let image_properties = frame_state
                            .resource_cache
                            .get_image_properties(request.key);


                        // Set if we need to request the source image from the cache this frame.
                        if let Some(image_properties) = image_properties {
                            is_tiled = image_properties.tiling.is_some();
                            opacity_binding.update(frame_context.scene_properties);

                            if *tile_spacing != LayoutSize::zero() && !is_tiled {
                                *source = ImageSource::Cache {
                                    // Size in device-pixels we need to allocate in render task cache.
                                    size: image_properties.descriptor.size.to_i32(),
                                    handle: None,
                                };
                            }

                            // Work out whether this image is a normal / simple type, or if
                            // we need to pre-render it to the render task cache.
                            if let Some(rect) = sub_rect {
                                // We don't properly support this right now.
                                debug_assert!(!is_tiled);
                                *source = ImageSource::Cache {
                                    // Size in device-pixels we need to allocate in render task cache.
                                    size: rect.size,
                                    handle: None,
                                };
                            }

                            let mut request_source_image = false;
                            let mut is_opaque = image_properties.descriptor.is_opaque;

                            // Every frame, for cached items, we need to request the render
                            // task cache item. The closure will be invoked on the first
                            // time through, and any time the render task output has been
                            // evicted from the texture cache.
                            match *source {
                                ImageSource::Cache { ref mut size, ref mut handle } => {
                                    let padding = DeviceIntSideOffsets::new(
                                        0,
                                        (tile_spacing.width * size.width as f32 / stretch_size.width) as i32,
                                        (tile_spacing.height * size.height as f32 / stretch_size.height) as i32,
                                        0,
                                    );

                                    let inner_size = *size;
                                    size.width += padding.horizontal();
                                    size.height += padding.vertical();

                                    is_opaque &= padding == DeviceIntSideOffsets::zero();

                                    let image_cache_key = ImageCacheKey {
                                        request,
                                        texel_rect: sub_rect,
                                    };
                                    let surfaces = &mut frame_state.surfaces;

                                    // Request a pre-rendered image task.
                                    *handle = Some(frame_state.resource_cache.request_render_task(
                                        RenderTaskCacheKey {
                                            size: *size,
                                            kind: RenderTaskCacheKeyKind::Image(image_cache_key),
                                        },
                                        frame_state.gpu_cache,
                                        frame_state.render_tasks,
                                        None,
                                        image_properties.descriptor.is_opaque,
                                        |render_tasks| {
                                            // We need to render the image cache this frame,
                                            // so will need access to the source texture.
                                            request_source_image = true;

                                            // Create a task to blit from the texture cache to
                                            // a normal transient render task surface. This will
                                            // copy only the sub-rect, if specified.
                                            let cache_to_target_task = RenderTask::new_blit_with_padding(
                                                inner_size,
                                                &padding,
                                                BlitSource::Image { key: image_cache_key },
                                            );
                                            let cache_to_target_task_id = render_tasks.add(cache_to_target_task);

                                            // Create a task to blit the rect from the child render
                                            // task above back into the right spot in the persistent
                                            // render target cache.
                                            let target_to_cache_task = RenderTask::new_blit(
                                                *size,
                                                BlitSource::RenderTask {
                                                    task_id: cache_to_target_task_id,
                                                },
                                            );
                                            let target_to_cache_task_id = render_tasks.add(target_to_cache_task);

                                            // Hook this into the render task tree at the right spot.
                                            surfaces[surface_index.0].tasks.push(target_to_cache_task_id);

                                            // Pass the image opacity, so that the cached render task
                                            // item inherits the same opacity properties.
                                            target_to_cache_task_id
                                        }
                                    ));
                                }
                                ImageSource::Default => {
                                    // Normal images just reference the source texture each frame.
                                    request_source_image = true;
                                }
                            }

                            if let Some(tile_size) = image_properties.tiling {
                                let device_image_size = image_properties.descriptor.size;

                                // Tighten the clip rect because decomposing the repeated image can
                                // produce primitives that are partially covering the original image
                                // rect and we want to clip these extra parts out.
                                let tight_clip_rect = self
                                    .combined_local_clip_rect
                                    .intersection(&prim_local_rect).unwrap();

                                let visible_rect = compute_conservative_visible_rect(
                                    prim_context,
                                    &self.clipped_world_rect.unwrap(),
                                    &tight_clip_rect
                                );

                                let base_edge_flags = edge_flags_for_tile_spacing(tile_spacing);

                                let stride = stretch_size + *tile_spacing;

                                visible_tiles.clear();

                                let repetitions = image::repetitions(
                                    &prim_local_rect,
                                    &visible_rect,
                                    stride,
                                );

                                for Repetition { origin, edge_flags } in repetitions {
                                    let edge_flags = base_edge_flags | edge_flags;

                                    let image_rect = LayoutRect {
                                        origin,
                                        size: stretch_size,
                                    };

                                    let tiles = image::tiles(
                                        &image_rect,
                                        &visible_rect,
                                        &device_image_size,
                                        tile_size as u32,
                                    );

                                    for tile in tiles {
                                        frame_state.resource_cache.request_image(
                                            request.with_tile(tile.offset),
                                            frame_state.gpu_cache,
                                        );

                                        let mut handle = GpuCacheHandle::new();
                                        if let Some(mut request) = frame_state.gpu_cache.request(&mut handle) {
                                            request.push(PremultipliedColorF::WHITE);
                                            request.push(PremultipliedColorF::WHITE);
                                            request.push([tile.rect.size.width, tile.rect.size.height, 0.0, 0.0]);
                                            request.write_segment(tile.rect, [0.0; 4]);
                                        }

                                        visible_tiles.push(VisibleImageTile {
                                            tile_offset: tile.offset,
                                            handle,
                                            edge_flags: tile.edge_flags & edge_flags,
                                            local_rect: tile.rect,
                                            local_clip_rect: tight_clip_rect,
                                        });
                                    }
                                }

                                if visible_tiles.is_empty() {
                                    // At this point if we don't have tiles to show it means we could probably
                                    // have done a better a job at culling during an earlier stage.
                                    // Clearing the screen rect has the effect of "culling out" the primitive
                                    // from the point of view of the batch builder, and ensures we don't hit
                                    // assertions later on because we didn't request any image.
                                    self.clipped_world_rect = None;
                                }
                            } else if request_source_image {
                                frame_state.resource_cache.request_image(
                                    request,
                                    frame_state.gpu_cache,
                                );
                            }

                            if is_opaque {
                                PrimitiveOpacity::from_alpha(opacity_binding.current * color.a)
                            } else {
                                PrimitiveOpacity::translucent()
                            }
                        } else {
                            PrimitiveOpacity::opaque()
                        }
                    }
                    BrushKind::YuvImage { format, yuv_key, image_rendering, .. } => {
                        let channel_num = format.get_plane_num();
                        debug_assert!(channel_num <= 3);
                        for channel in 0 .. channel_num {
                            frame_state.resource_cache.request_image(
                                ImageRequest {
                                    key: yuv_key[channel],
                                    rendering: image_rendering,
                                    tile: None,
                                },
                                frame_state.gpu_cache,
                            );
                        }

                        PrimitiveOpacity::opaque()
                    }
                    BrushKind::Border { ref mut source, .. } => {
                        match *source {
                            BorderSource::Image(request) => {
                                let image_properties = frame_state
                                    .resource_cache
                                    .get_image_properties(request.key);

                                if let Some(image_properties) = image_properties {
                                    frame_state.resource_cache.request_image(
                                        request,
                                        frame_state.gpu_cache,
                                    );
                                    PrimitiveOpacity {
                                        is_opaque: image_properties.descriptor.is_opaque,
                                    }
                                } else {
                                    PrimitiveOpacity::opaque()
                                }
                            }
                            BorderSource::Border { ref border, ref widths, ref mut segments, .. } => {
                                // TODO(gw): When drawing in screen raster mode, we should also incorporate a
                                //           scale factor from the world transform to get an appropriately
                                //           sized border task.
                                let world_scale = LayoutToWorldScale::new(1.0);
                                let mut scale = world_scale * frame_context.device_pixel_scale;
                                let max_scale = get_max_scale_for_border(&border.radius, widths);
                                scale.0 = scale.0.min(max_scale.0);

                                // For each edge and corner, request the render task by content key
                                // from the render task cache. This ensures that the render task for
                                // this segment will be available for batching later in the frame.
                                for segment in segments {
                                    // Update the cache key device size based on requested scale.
                                    segment.cache_key.size = to_cache_size(segment.local_task_size * scale);

                                    let surfaces = &mut frame_state.surfaces;

                                    segment.handle = Some(frame_state.resource_cache.request_render_task(
                                        segment.cache_key.clone(),
                                        frame_state.gpu_cache,
                                        frame_state.render_tasks,
                                        None,
                                        false,          // TODO(gw): We don't calculate opacity for borders yet!
                                        |render_tasks| {
                                            let task = RenderTask::new_border_segment(
                                                segment.cache_key.size,
                                                build_border_instances(
                                                    &segment.cache_key,
                                                    border,
                                                    scale,
                                                ),
                                            );

                                            let task_id = render_tasks.add(task);

                                            surfaces[surface_index.0].tasks.push(task_id);

                                            task_id
                                        }
                                    ));
                                }

                                // Shouldn't matter, since the segment opacity is used instead
                                PrimitiveOpacity::translucent()
                            }
                        }
                    }
                    BrushKind::RadialGradient {
                        stops_range,
                        center,
                        start_radius,
                        end_radius,
                        ratio_xy,
                        extend_mode,
                        stretch_size,
                        tile_spacing,
                        ref mut stops_handle,
                        ref mut visible_tiles,
                        ..
                    } => {
                        build_gradient_stops_request(
                            stops_handle,
                            stops_range,
                            false,
                            frame_state,
                            display_list,
                        );

                        if tile_spacing != LayoutSize::zero() {
                            is_tiled = true;

                            decompose_repeated_primitive(
                                visible_tiles,
                                self,
                                &prim_local_rect,
                                &stretch_size,
                                &tile_spacing,
                                prim_context,
                                frame_state,
                                &mut |rect, mut request| {
                                    request.push([
                                        center.x,
                                        center.y,
                                        start_radius,
                                        end_radius,
                                    ]);
                                    request.push([
                                        ratio_xy,
                                        pack_as_float(extend_mode as u32),
                                        stretch_size.width,
                                        stretch_size.height,
                                    ]);
                                    request.write_segment(*rect, [0.0; 4]);
                                },
                            );
                        }

                        //TODO: can we make it opaque in some cases?
                        PrimitiveOpacity::translucent()
                    }
                    BrushKind::LinearGradient {
                        stops_range,
                        reverse_stops,
                        start_point,
                        end_point,
                        extend_mode,
                        stretch_size,
                        tile_spacing,
                        stops_opacity,
                        ref mut stops_handle,
                        ref mut visible_tiles,
                        ..
                    } => {
                        build_gradient_stops_request(
                            stops_handle,
                            stops_range,
                            reverse_stops,
                            frame_state,
                            display_list,
                        );

                        if tile_spacing != LayoutSize::zero() {
                            is_tiled = true;

                            decompose_repeated_primitive(
                                visible_tiles,
                                self,
                                &prim_local_rect,
                                &stretch_size,
                                &tile_spacing,
                                prim_context,
                                frame_state,
                                &mut |rect, mut request| {
                                    request.push([
                                        start_point.x,
                                        start_point.y,
                                        end_point.x,
                                        end_point.y,
                                    ]);
                                    request.push([
                                        pack_as_float(extend_mode as u32),
                                        stretch_size.width,
                                        stretch_size.height,
                                        0.0,
                                    ]);
                                    request.write_segment(*rect, [0.0; 4]);
                                }
                            );
                        }

                        // If the coverage of the gradient extends to or beyond
                        // the primitive rect, then the opacity can be determined
                        // by the colors of the stops. If we have tiling / spacing
                        // then we just assume the gradient is translucent for now.
                        // (In the future we could consider segmenting in some cases).
                        let stride = stretch_size + tile_spacing;
                        if stride.width >= prim_local_rect.size.width &&
                           stride.height >= prim_local_rect.size.height {
                            stops_opacity
                        } else {
                            PrimitiveOpacity::translucent()
                        }
                    }
                    BrushKind::Solid { ref color, ref mut opacity_binding, .. } => {
                        opacity_binding.update(frame_context.scene_properties);
                        PrimitiveOpacity::from_alpha(opacity_binding.current * color.a)
                    }
                };
            }
        }

        if is_tiled {
            // we already requested each tile's gpu data.
            return;
        }

        // Mark this GPU resource as required for this frame.
        let is_chased = self.is_chased();
        if let Some(mut request) = frame_state.gpu_cache.request(&mut self.gpu_location) {
            match *prim_details {
                PrimitiveDetails::Brush(ref mut brush) => {
                    brush.write_gpu_blocks(&mut request, prim_local_rect);

                    match brush.segment_desc {
                        Some(ref segment_desc) => {
                            for segment in &segment_desc.segments {
                                if is_chased {
                                    println!("\t\t{:?}", segment);
                                }
                                // has to match VECS_PER_SEGMENT
                                request.write_segment(
                                    segment.local_rect,
                                    segment.extra_data,
                                );
                            }
                        }
                        None => {
                            request.write_segment(
                                prim_local_rect,
                                [0.0; 4],
                            );
                        }
                    }
                }
            }
        }
    }

    fn update_clip_task(
        &mut self,
        prim_local_rect: LayoutRect,
        prim_local_clip_rect: LayoutRect,
        prim_context: &PrimitiveContext,
        prim_bounding_rect: WorldRect,
        root_spatial_node_index: SpatialNodeIndex,
        clip_chain: &ClipChainInstance,
        surface_index: SurfaceIndex,
        pic_state: &mut PictureState,
        frame_context: &FrameBuildingContext,
        frame_state: &mut FrameBuildingState,
        clip_node_collector: &Option<ClipNodeCollector>,
        primitives: &mut [Primitive],
    ) {
        if self.is_chased() {
            println!("\tupdating clip task with pic rect {:?}", clip_chain.pic_clip_rect);
        }

        // Reset clips from previous frames since we may clip differently each frame.
        self.clip_task_id = None;

        // First try to  render this primitive's mask using optimized brush rendering.
        if self.update_clip_task_for_brush(
            prim_local_rect,
            prim_local_clip_rect,
            root_spatial_node_index,
            prim_bounding_rect,
            prim_context,
            &clip_chain,
            surface_index,
            pic_state,
            frame_context,
            frame_state,
            clip_node_collector,
            primitives,
        ) {
            if self.is_chased() {
                println!("\tsegment tasks have been created for clipping");
            }
            return;
        }

        if clip_chain.needs_mask {
            if let Some((device_rect, _, _)) = get_raster_rects(
                clip_chain.pic_clip_rect,
                &pic_state.map_pic_to_raster,
                &pic_state.map_raster_to_world,
                prim_bounding_rect,
                frame_context.device_pixel_scale,
            ) {
                let clip_task = RenderTask::new_mask(
                    device_rect,
                    clip_chain.clips_range,
                    root_spatial_node_index,
                    frame_state.clip_store,
                    frame_state.gpu_cache,
                    frame_state.resource_cache,
                    frame_state.render_tasks,
                    &mut frame_state.resources.clip_data_store,
                );

                let clip_task_id = frame_state.render_tasks.add(clip_task);
                if self.is_chased() {
                    println!("\tcreated task {:?} with device rect {:?}",
                        clip_task_id, device_rect);
                }
                self.clip_task_id = Some(clip_task_id);
                frame_state.surfaces[surface_index.0].tasks.push(clip_task_id);
            }
        }
    }
}

pub fn get_raster_rects(
    pic_rect: PictureRect,
    map_to_raster: &SpaceMapper<PicturePixel, RasterPixel>,
    map_to_world: &SpaceMapper<RasterPixel, WorldPixel>,
    prim_bounding_rect: WorldRect,
    device_pixel_scale: DevicePixelScale,
) -> Option<(DeviceIntRect, DeviceRect, PictureToRasterTransform)> {
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

    let transform = map_to_raster.get_transform();

    Some((clipped.to_i32(), unclipped, transform))
}

/// Get the inline (horizontal) and block (vertical) sizes
/// for a given line decoration.
fn get_line_decoration_sizes(
    rect_size: &LayoutSize,
    orientation: LineOrientation,
    style: LineStyle,
    wavy_line_thickness: f32,
) -> Option<(f32, f32)> {
    let h = match orientation {
        LineOrientation::Horizontal => rect_size.height,
        LineOrientation::Vertical => rect_size.width,
    };

    // TODO(gw): The formulae below are based on the existing gecko and line
    //           shader code. They give reasonable results for most inputs,
    //           but could definitely do with a detailed pass to get better
    //           quality on a wider range of inputs!
    //           See nsCSSRendering::PaintDecorationLine in Gecko.

    match style {
        LineStyle::Solid => {
            None
        }
        LineStyle::Dashed => {
            let dash_length = (3.0 * h).min(64.0).max(1.0);

            Some((2.0 * dash_length, 4.0))
        }
        LineStyle::Dotted => {
            let diameter = h.min(64.0).max(1.0);
            let period = 2.0 * diameter;

            Some((period, diameter))
        }
        LineStyle::Wavy => {
            let line_thickness = wavy_line_thickness.max(1.0);
            let slope_length = h - line_thickness;
            let flat_length = ((line_thickness - 1.0) * 2.0).max(1.0);
            let approx_period = 2.0 * (slope_length + flat_length);

            Some((approx_period, h))
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{CompositeOperator, FilterPrimitive, FilterPrimitiveInput, FilterPrimitiveKind};
use api::{LineStyle, LineOrientation, ClipMode, MixBlendMode, ColorF, ColorSpace, FilterOpGraphPictureBufferId};
use api::MAX_RENDER_TASK_SIZE;
use api::units::*;
use crate::box_shadow::BLUR_SAMPLE_SCALE;
use crate::clip::{ClipDataStore, ClipItemKind, ClipStore, ClipNodeRange};
use crate::command_buffer::{CommandBufferIndex, QuadFlags};
use crate::pattern::{PatternKind, PatternShaderInput};
use crate::spatial_tree::SpatialNodeIndex;
use crate::filterdata::SFilterData;
use crate::frame_builder::{FrameBuilderConfig, FrameBuildingState};
use crate::gpu_cache::{GpuCache, GpuCacheAddress, GpuCacheHandle};
use crate::gpu_types::{BorderInstance, ImageSource, UvRectKind, TransformPaletteId};
use crate::internal_types::{CacheTextureId, FastHashMap, FilterGraphNode, FilterGraphOp, FilterGraphPictureReference, SVGFE_CONVOLVE_VALUES_LIMIT, TextureSource, Swizzle};
use crate::picture::{ResolvedSurfaceTexture, MAX_SURFACE_SIZE};
use crate::prim_store::ClipData;
use crate::prim_store::gradient::{
    FastLinearGradientTask, RadialGradientTask,
    ConicGradientTask, LinearGradientTask,
};
use crate::resource_cache::{ResourceCache, ImageRequest};
use std::{usize, f32, i32, u32};
use crate::renderer::{GpuBufferAddress, GpuBufferBuilderF};
use crate::render_backend::DataStores;
use crate::render_target::{ResolveOp, RenderTargetKind};
use crate::render_task_graph::{PassId, RenderTaskId, RenderTaskGraphBuilder};
use crate::render_task_cache::{RenderTaskCacheEntryHandle, RenderTaskCacheKey, RenderTaskCacheKeyKind, RenderTaskParent};
use crate::segment::EdgeAaSegmentMask;
use crate::surface::SurfaceBuilder;
use smallvec::SmallVec;

const FLOATS_PER_RENDER_TASK_INFO: usize = 8;
pub const MAX_BLUR_STD_DEVIATION: f32 = 4.0;
pub const MIN_DOWNSCALING_RT_SIZE: i32 = 8;

fn render_task_sanity_check(size: &DeviceIntSize) {
    if size.width > MAX_RENDER_TASK_SIZE ||
        size.height > MAX_RENDER_TASK_SIZE {
        error!("Attempting to create a render task of size {}x{}", size.width, size.height);
        panic!();
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
#[repr(C)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskAddress(pub i32);

impl Into<RenderTaskAddress> for RenderTaskId {
    fn into(self) -> RenderTaskAddress {
        RenderTaskAddress(self.index as i32)
    }
}

/// A render task location that targets a persistent output buffer which
/// will be retained over multiple frames.
#[derive(Clone, Debug, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum StaticRenderTaskSurface {
    /// The output of the `RenderTask` will be persisted beyond this frame, and
    /// thus should be drawn into the `TextureCache`.
    TextureCache {
        /// Which texture in the texture cache should be drawn into.
        texture: CacheTextureId,
        /// What format this texture cache surface is
        target_kind: RenderTargetKind,
    },
    /// Only used as a source for render tasks, can be any texture including an
    /// external one.
    ReadOnly {
        source: TextureSource,
    },
    /// This render task will be drawn to a picture cache texture that is
    /// persisted between both frames and scenes, if the content remains valid.
    PictureCache {
        /// Describes either a WR texture or a native OS compositor target
        surface: ResolvedSurfaceTexture,
    },
}

/// Identifies the output buffer location for a given `RenderTask`.
#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTaskLocation {
    // Towards the beginning of the frame, most task locations are typically not
    // known yet, in which case they are set to one of the following variants:

    /// A dynamic task that has not yet been allocated a texture and rect.
    Unallocated {
        /// Requested size of this render task
        size: DeviceIntSize,
    },
    /// Will be replaced by a Static location after the texture cache update.
    CacheRequest {
        size: DeviceIntSize,
    },
    /// Same allocation as an existing task deeper in the dependency graph
    Existing {
        parent_task_id: RenderTaskId,
        /// Requested size of this render task
        size: DeviceIntSize,
    },

    // Before batching begins, we expect that locations have been resolved to
    // one of the following variants:

    /// The `RenderTask` should be drawn to a target provided by the atlas
    /// allocator. This is the most common case.
    Dynamic {
        /// Texture that this task was allocated to render on
        texture_id: CacheTextureId,
        /// Rectangle in the texture this task occupies
        rect: DeviceIntRect,
    },
    /// A task that is output to a persistent / retained target.
    Static {
        /// Target to draw to
        surface: StaticRenderTaskSurface,
        /// Rectangle in the texture this task occupies
        rect: DeviceIntRect,
    },
}

impl RenderTaskLocation {
    /// Returns true if this is a dynamic location.
    pub fn is_dynamic(&self) -> bool {
        match *self {
            RenderTaskLocation::Dynamic { .. } => true,
            _ => false,
        }
    }

    pub fn size(&self) -> DeviceIntSize {
        match self {
            RenderTaskLocation::Unallocated { size } => *size,
            RenderTaskLocation::Dynamic { rect, .. } => rect.size(),
            RenderTaskLocation::Static { rect, .. } => rect.size(),
            RenderTaskLocation::CacheRequest { size } => *size,
            RenderTaskLocation::Existing { size, .. } => *size,
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CachedTask {
    pub target_kind: RenderTargetKind,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheMaskTask {
    pub actual_rect: DeviceRect,
    pub root_spatial_node_index: SpatialNodeIndex,
    pub clip_node_range: ClipNodeRange,
    pub device_pixel_scale: DevicePixelScale,
    pub clear_to_one: bool,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ClipRegionTask {
    pub local_pos: LayoutPoint,
    pub device_pixel_scale: DevicePixelScale,
    pub clip_data: ClipData,
    pub clear_to_one: bool,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct EmptyTask {
    pub content_origin: DevicePoint,
    pub device_pixel_scale: DevicePixelScale,
    pub raster_spatial_node_index: SpatialNodeIndex,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PrimTask {
    pub pattern: PatternKind,
    pub pattern_input: PatternShaderInput,
    pub device_pixel_scale: DevicePixelScale,
    pub content_origin: DevicePoint,
    pub prim_address_f: GpuBufferAddress,
    pub raster_spatial_node_index: SpatialNodeIndex,
    pub transform_id: TransformPaletteId,
    pub edge_flags: EdgeAaSegmentMask,
    pub quad_flags: QuadFlags,
    pub clip_node_range: ClipNodeRange,
    pub prim_needs_scissor_rect: bool,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct TileCompositeTask {
    pub clear_color: ColorF,
    pub scissor_rect: DeviceIntRect,
    pub valid_rect: DeviceIntRect,
    pub task_id: Option<RenderTaskId>,
    pub sub_rect_offset: DeviceIntVector2D,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct PictureTask {
    pub can_merge: bool,
    pub content_origin: DevicePoint,
    pub surface_spatial_node_index: SpatialNodeIndex,
    pub raster_spatial_node_index: SpatialNodeIndex,
    pub device_pixel_scale: DevicePixelScale,
    pub clear_color: Option<ColorF>,
    pub scissor_rect: Option<DeviceIntRect>,
    pub valid_rect: Option<DeviceIntRect>,
    pub cmd_buffer_index: CommandBufferIndex,
    pub resolve_op: Option<ResolveOp>,

    pub can_use_shared_surface: bool,
}

impl PictureTask {
    /// Copy an existing picture task, but set a new command buffer for it to build in to.
    /// Used for pictures that are split between render tasks (e.g. pre/post a backdrop
    /// filter). Subsequent picture tasks never have a clear color as they are by definition
    /// going to write to an existing target
    pub fn duplicate(
        &self,
        cmd_buffer_index: CommandBufferIndex,
    ) -> Self {
        assert_eq!(self.resolve_op, None);

        PictureTask {
            clear_color: None,
            cmd_buffer_index,
            resolve_op: None,
            can_use_shared_surface: false,
            ..*self
        }
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlurTask {
    pub blur_std_deviation: f32,
    pub target_kind: RenderTargetKind,
    pub blur_region: DeviceIntSize,
}

impl BlurTask {
    // In order to do the blur down-scaling passes without introducing errors, we need the
    // source of each down-scale pass to be a multuple of two. If need be, this inflates
    // the source size so that each down-scale pass will sample correctly.
    pub fn adjusted_blur_source_size(original_size: DeviceSize, mut std_dev: DeviceSize) -> DeviceIntSize {
        let mut adjusted_size = original_size;
        let mut scale_factor = 1.0;
        while std_dev.width > MAX_BLUR_STD_DEVIATION && std_dev.height > MAX_BLUR_STD_DEVIATION {
            if adjusted_size.width < MIN_DOWNSCALING_RT_SIZE as f32 ||
               adjusted_size.height < MIN_DOWNSCALING_RT_SIZE as f32 {
                break;
            }
            std_dev = std_dev * 0.5;
            scale_factor *= 2.0;
            adjusted_size = (original_size.to_f32() / scale_factor).ceil();
        }

        (adjusted_size * scale_factor).round().to_i32()
    }
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ScalingTask {
    pub target_kind: RenderTargetKind,
    pub padding: DeviceIntSideOffsets,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BorderTask {
    pub instances: Vec<BorderInstance>,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct BlitTask {
    pub source: RenderTaskId,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct LineDecorationTask {
    pub wavy_line_thickness: f32,
    pub style: LineStyle,
    pub orientation: LineOrientation,
    pub local_size: LayoutSize,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum SvgFilterInfo {
    Blend(MixBlendMode),
    Flood(ColorF),
    LinearToSrgb,
    SrgbToLinear,
    Opacity(f32),
    ColorMatrix(Box<[f32; 20]>),
    DropShadow(ColorF),
    Offset(DeviceVector2D),
    ComponentTransfer(SFilterData),
    Composite(CompositeOperator),
    // TODO: This is used as a hack to ensure that a blur task's input is always in the blur's previous pass.
    Identity,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SvgFilterTask {
    pub info: SvgFilterInfo,
    pub extra_gpu_cache_handle: Option<GpuCacheHandle>,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SVGFEFilterTask {
    pub node: FilterGraphNode,
    pub op: FilterGraphOp,
    pub content_origin: DevicePoint,
    pub extra_gpu_cache_handle: Option<GpuCacheHandle>,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct ReadbackTask {
    // The offset of the rect that needs to be read back, in the
    // device space of the surface that will be read back from.
    // If this is None, there is no readback surface available
    // and this is a dummy (empty) readback.
    pub readback_origin: Option<DevicePoint>,
}

#[derive(Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskData {
    pub data: [f32; FLOATS_PER_RENDER_TASK_INFO],
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum RenderTaskKind {
    Image(ImageRequest),
    Cached(CachedTask),
    Picture(PictureTask),
    CacheMask(CacheMaskTask),
    ClipRegion(ClipRegionTask),
    VerticalBlur(BlurTask),
    HorizontalBlur(BlurTask),
    Readback(ReadbackTask),
    Scaling(ScalingTask),
    Blit(BlitTask),
    Border(BorderTask),
    LineDecoration(LineDecorationTask),
    FastLinearGradient(FastLinearGradientTask),
    LinearGradient(LinearGradientTask),
    RadialGradient(RadialGradientTask),
    ConicGradient(ConicGradientTask),
    SvgFilter(SvgFilterTask),
    SVGFENode(SVGFEFilterTask),
    TileComposite(TileCompositeTask),
    Prim(PrimTask),
    Empty(EmptyTask),
    #[cfg(test)]
    Test(RenderTargetKind),
}

impl RenderTaskKind {
    pub fn is_a_rendering_operation(&self) -> bool {
        match self {
            &RenderTaskKind::Image(..) => false,
            &RenderTaskKind::Cached(..) => false,
            _ => true,
        }
    }

    /// Whether this task can be allocated on a shared render target surface
    pub fn can_use_shared_surface(&self) -> bool {
        match self {
            &RenderTaskKind::Picture(ref info) => info.can_use_shared_surface,
            _ => true,
        }
    }

    pub fn should_advance_pass(&self) -> bool {
        match self {
            &RenderTaskKind::Image(..) => false,
            &RenderTaskKind::Cached(..) => false,
            _ => true,
        }
    }

    pub fn as_str(&self) -> &'static str {
        match *self {
            RenderTaskKind::Image(..) => "Image",
            RenderTaskKind::Cached(..) => "Cached",
            RenderTaskKind::Picture(..) => "Picture",
            RenderTaskKind::CacheMask(..) => "CacheMask",
            RenderTaskKind::ClipRegion(..) => "ClipRegion",
            RenderTaskKind::VerticalBlur(..) => "VerticalBlur",
            RenderTaskKind::HorizontalBlur(..) => "HorizontalBlur",
            RenderTaskKind::Readback(..) => "Readback",
            RenderTaskKind::Scaling(..) => "Scaling",
            RenderTaskKind::Blit(..) => "Blit",
            RenderTaskKind::Border(..) => "Border",
            RenderTaskKind::LineDecoration(..) => "LineDecoration",
            RenderTaskKind::FastLinearGradient(..) => "FastLinearGradient",
            RenderTaskKind::LinearGradient(..) => "LinearGradient",
            RenderTaskKind::RadialGradient(..) => "RadialGradient",
            RenderTaskKind::ConicGradient(..) => "ConicGradient",
            RenderTaskKind::SvgFilter(..) => "SvgFilter",
            RenderTaskKind::SVGFENode(..) => "SVGFENode",
            RenderTaskKind::TileComposite(..) => "TileComposite",
            RenderTaskKind::Prim(..) => "Prim",
            RenderTaskKind::Empty(..) => "Empty",
            #[cfg(test)]
            RenderTaskKind::Test(..) => "Test",
        }
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        match *self {
            RenderTaskKind::Image(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::FastLinearGradient(..) |
            RenderTaskKind::LinearGradient(..) |
            RenderTaskKind::RadialGradient(..) |
            RenderTaskKind::ConicGradient(..) |
            RenderTaskKind::Picture(..) |
            RenderTaskKind::Blit(..) |
            RenderTaskKind::TileComposite(..) |
            RenderTaskKind::Prim(..) |
            RenderTaskKind::SvgFilter(..) => {
                RenderTargetKind::Color
            }
            RenderTaskKind::SVGFENode(..) => {
                RenderTargetKind::Color
            }

            RenderTaskKind::ClipRegion(..) |
            RenderTaskKind::CacheMask(..) |
            RenderTaskKind::Empty(..) => {
                RenderTargetKind::Alpha
            }

            RenderTaskKind::VerticalBlur(ref task_info) |
            RenderTaskKind::HorizontalBlur(ref task_info) => {
                task_info.target_kind
            }

            RenderTaskKind::Scaling(ref task_info) => {
                task_info.target_kind
            }

            RenderTaskKind::Cached(ref task_info) => {
                task_info.target_kind
            }

            #[cfg(test)]
            RenderTaskKind::Test(kind) => kind,
        }
    }

    pub fn new_tile_composite(
        sub_rect_offset: DeviceIntVector2D,
        scissor_rect: DeviceIntRect,
        valid_rect: DeviceIntRect,
        clear_color: ColorF,
    ) -> Self {
        RenderTaskKind::TileComposite(TileCompositeTask {
            task_id: None,
            sub_rect_offset,
            scissor_rect,
            valid_rect,
            clear_color,
        })
    }

    pub fn new_picture(
        size: DeviceIntSize,
        needs_scissor_rect: bool,
        content_origin: DevicePoint,
        surface_spatial_node_index: SpatialNodeIndex,
        raster_spatial_node_index: SpatialNodeIndex,
        device_pixel_scale: DevicePixelScale,
        scissor_rect: Option<DeviceIntRect>,
        valid_rect: Option<DeviceIntRect>,
        clear_color: Option<ColorF>,
        cmd_buffer_index: CommandBufferIndex,
        can_use_shared_surface: bool,
    ) -> Self {
        render_task_sanity_check(&size);

        RenderTaskKind::Picture(PictureTask {
            content_origin,
            can_merge: !needs_scissor_rect,
            surface_spatial_node_index,
            raster_spatial_node_index,
            device_pixel_scale,
            scissor_rect,
            valid_rect,
            clear_color,
            cmd_buffer_index,
            resolve_op: None,
            can_use_shared_surface,
        })
    }

    pub fn new_prim(
        pattern: PatternKind,
        pattern_input: PatternShaderInput,
        raster_spatial_node_index: SpatialNodeIndex,
        device_pixel_scale: DevicePixelScale,
        content_origin: DevicePoint,
        prim_address_f: GpuBufferAddress,
        transform_id: TransformPaletteId,
        edge_flags: EdgeAaSegmentMask,
        quad_flags: QuadFlags,
        clip_node_range: ClipNodeRange,
        prim_needs_scissor_rect: bool,
    ) -> Self {
        RenderTaskKind::Prim(PrimTask {
            pattern,
            pattern_input,
            raster_spatial_node_index,
            device_pixel_scale,
            content_origin,
            prim_address_f,
            transform_id,
            edge_flags,
            quad_flags,
            clip_node_range,
            prim_needs_scissor_rect,
        })
    }

    pub fn new_readback(
        readback_origin: Option<DevicePoint>,
    ) -> Self {
        RenderTaskKind::Readback(
            ReadbackTask {
                readback_origin,
            }
        )
    }

    pub fn new_line_decoration(
        style: LineStyle,
        orientation: LineOrientation,
        wavy_line_thickness: f32,
        local_size: LayoutSize,
    ) -> Self {
        RenderTaskKind::LineDecoration(LineDecorationTask {
            style,
            orientation,
            wavy_line_thickness,
            local_size,
        })
    }

    pub fn new_border_segment(
        instances: Vec<BorderInstance>,
    ) -> Self {
        RenderTaskKind::Border(BorderTask {
            instances,
        })
    }

    pub fn new_rounded_rect_mask(
        local_pos: LayoutPoint,
        clip_data: ClipData,
        device_pixel_scale: DevicePixelScale,
        fb_config: &FrameBuilderConfig,
    ) -> Self {
        RenderTaskKind::ClipRegion(ClipRegionTask {
            local_pos,
            device_pixel_scale,
            clip_data,
            clear_to_one: fb_config.gpu_supports_fast_clears,
        })
    }

    pub fn new_mask(
        outer_rect: DeviceIntRect,
        clip_node_range: ClipNodeRange,
        root_spatial_node_index: SpatialNodeIndex,
        clip_store: &mut ClipStore,
        gpu_cache: &mut GpuCache,
        gpu_buffer_builder: &mut GpuBufferBuilderF,
        resource_cache: &mut ResourceCache,
        rg_builder: &mut RenderTaskGraphBuilder,
        clip_data_store: &mut ClipDataStore,
        device_pixel_scale: DevicePixelScale,
        fb_config: &FrameBuilderConfig,
        surface_builder: &mut SurfaceBuilder,
    ) -> RenderTaskId {
        // Step through the clip sources that make up this mask. If we find
        // any box-shadow clip sources, request that image from the render
        // task cache. This allows the blurred box-shadow rect to be cached
        // in the texture cache across frames.
        // TODO(gw): Consider moving this logic outside this function, especially
        //           as we add more clip sources that depend on render tasks.
        // TODO(gw): If this ever shows up in a profile, we could pre-calculate
        //           whether a ClipSources contains any box-shadows and skip
        //           this iteration for the majority of cases.
        let task_size = outer_rect.size();

        // If we have a potentially tiled clip mask, clear the mask area first. Otherwise,
        // the first (primary) clip mask will overwrite all the clip mask pixels with
        // blending disabled to set to the initial value.

        let clip_task_id = rg_builder.add().init(
            RenderTask::new_dynamic(
                task_size,
                RenderTaskKind::CacheMask(CacheMaskTask {
                    actual_rect: outer_rect.to_f32(),
                    clip_node_range,
                    root_spatial_node_index,
                    device_pixel_scale,
                    clear_to_one: fb_config.gpu_supports_fast_clears,
                }),
            )
        );

        for i in 0 .. clip_node_range.count {
            let clip_instance = clip_store.get_instance_from_range(&clip_node_range, i);
            let clip_node = &mut clip_data_store[clip_instance.handle];
            match clip_node.item.kind {
                ClipItemKind::BoxShadow { ref mut source } => {
                    let (cache_size, cache_key) = source.cache_key
                        .as_ref()
                        .expect("bug: no cache key set")
                        .clone();
                    let blur_radius_dp = cache_key.blur_radius_dp as f32;
                    let device_pixel_scale = DevicePixelScale::new(cache_key.device_pixel_scale.to_f32_px());

                    // Request a cacheable render task with a blurred, minimal
                    // sized box-shadow rect.
                    source.render_task = Some(resource_cache.request_render_task(
                        RenderTaskCacheKey {
                            size: cache_size,
                            kind: RenderTaskCacheKeyKind::BoxShadow(cache_key),
                        },
                        gpu_cache,
                        gpu_buffer_builder,
                        rg_builder,
                        None,
                        false,
                        RenderTaskParent::RenderTask(clip_task_id),
                        surface_builder,
                        |rg_builder, _| {
                            let clip_data = ClipData::rounded_rect(
                                source.minimal_shadow_rect.size(),
                                &source.shadow_radius,
                                ClipMode::Clip,
                            );

                            // Draw the rounded rect.
                            let mask_task_id = rg_builder.add().init(RenderTask::new_dynamic(
                                cache_size,
                                RenderTaskKind::new_rounded_rect_mask(
                                    source.minimal_shadow_rect.min,
                                    clip_data,
                                    device_pixel_scale,
                                    fb_config,
                                ),
                            ));

                            // Blur it
                            RenderTask::new_blur(
                                DeviceSize::new(blur_radius_dp, blur_radius_dp),
                                mask_task_id,
                                rg_builder,
                                RenderTargetKind::Alpha,
                                None,
                                cache_size,
                            )
                        }
                    ));
                }
                ClipItemKind::Rectangle { .. } |
                ClipItemKind::RoundedRectangle { .. } |
                ClipItemKind::Image { .. } => {}
            }
        }

        clip_task_id
    }

    // Write (up to) 8 floats of data specific to the type
    // of render task that is provided to the GPU shaders
    // via a vertex texture.
    pub fn write_task_data(
        &self,
        target_rect: DeviceIntRect,
    ) -> RenderTaskData {
        // NOTE: The ordering and layout of these structures are
        //       required to match both the GPU structures declared
        //       in prim_shared.glsl, and also the uses in submit_batch()
        //       in renderer.rs.
        // TODO(gw): Maybe there's a way to make this stuff a bit
        //           more type-safe. Although, it will always need
        //           to be kept in sync with the GLSL code anyway.

        let data = match self {
            RenderTaskKind::Picture(ref task) => {
                // Note: has to match `PICTURE_TYPE_*` in shaders
                [
                    task.device_pixel_scale.0,
                    task.content_origin.x,
                    task.content_origin.y,
                    0.0,
                ]
            }
            RenderTaskKind::Prim(ref task) => {
                [
                    // NOTE: This must match the render task data format for Picture tasks currently
                    task.device_pixel_scale.0,
                    task.content_origin.x,
                    task.content_origin.y,
                    0.0,
                ]
            }
            RenderTaskKind::Empty(ref task) => {
                [
                    // NOTE: This must match the render task data format for Picture tasks currently
                    task.device_pixel_scale.0,
                    task.content_origin.x,
                    task.content_origin.y,
                    0.0,
                ]
            }
            RenderTaskKind::CacheMask(ref task) => {
                [
                    task.device_pixel_scale.0,
                    task.actual_rect.min.x,
                    task.actual_rect.min.y,
                    0.0,
                ]
            }
            RenderTaskKind::ClipRegion(ref task) => {
                [
                    task.device_pixel_scale.0,
                    0.0,
                    0.0,
                    0.0,
                ]
            }
            RenderTaskKind::VerticalBlur(ref task) |
            RenderTaskKind::HorizontalBlur(ref task) => {
                [
                    task.blur_std_deviation,
                    task.blur_region.width as f32,
                    task.blur_region.height as f32,
                    0.0,
                ]
            }
            RenderTaskKind::Image(..) |
            RenderTaskKind::Cached(..) |
            RenderTaskKind::Readback(..) |
            RenderTaskKind::Scaling(..) |
            RenderTaskKind::Border(..) |
            RenderTaskKind::LineDecoration(..) |
            RenderTaskKind::FastLinearGradient(..) |
            RenderTaskKind::LinearGradient(..) |
            RenderTaskKind::RadialGradient(..) |
            RenderTaskKind::ConicGradient(..) |
            RenderTaskKind::TileComposite(..) |
            RenderTaskKind::Blit(..) => {
                [0.0; 4]
            }

            RenderTaskKind::SvgFilter(ref task) => {
                match task.info {
                    SvgFilterInfo::Opacity(opacity) => [opacity, 0.0, 0.0, 0.0],
                    SvgFilterInfo::Offset(offset) => [offset.x, offset.y, 0.0, 0.0],
                    _ => [0.0; 4]
                }
            }
            RenderTaskKind::SVGFENode(_task) => {
                // we don't currently use this for SVGFE filters.
                // see SVGFEFilterInstance instead
                [0.0; 4]
            }

            #[cfg(test)]
            RenderTaskKind::Test(..) => {
                [0.0; 4]
            }
        };

        RenderTaskData {
            data: [
                target_rect.min.x as f32,
                target_rect.min.y as f32,
                target_rect.max.x as f32,
                target_rect.max.y as f32,
                data[0],
                data[1],
                data[2],
                data[3],
            ]
        }
    }

    pub fn write_gpu_blocks(
        &mut self,
        gpu_cache: &mut GpuCache,
    ) {
        match self {
            RenderTaskKind::SvgFilter(ref mut filter_task) => {
                match filter_task.info {
                    SvgFilterInfo::ColorMatrix(ref matrix) => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            for i in 0..5 {
                                request.push([matrix[i*4], matrix[i*4+1], matrix[i*4+2], matrix[i*4+3]]);
                            }
                        }
                    }
                    SvgFilterInfo::DropShadow(color) |
                    SvgFilterInfo::Flood(color) => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            request.push(color.to_array());
                        }
                    }
                    SvgFilterInfo::ComponentTransfer(ref data) => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(request) = gpu_cache.request(handle) {
                            data.update(request);
                        }
                    }
                    SvgFilterInfo::Composite(ref operator) => {
                        if let CompositeOperator::Arithmetic(k_vals) = operator {
                            let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                            if let Some(mut request) = gpu_cache.request(handle) {
                                request.push(*k_vals);
                            }
                        }
                    }
                    _ => {},
                }
            }
            RenderTaskKind::SVGFENode(ref mut filter_task) => {
                match filter_task.op {
                    FilterGraphOp::SVGFEBlendDarken => {}
                    FilterGraphOp::SVGFEBlendLighten => {}
                    FilterGraphOp::SVGFEBlendMultiply => {}
                    FilterGraphOp::SVGFEBlendNormal => {}
                    FilterGraphOp::SVGFEBlendScreen => {}
                    FilterGraphOp::SVGFEBlendOverlay => {}
                    FilterGraphOp::SVGFEBlendColorDodge => {}
                    FilterGraphOp::SVGFEBlendColorBurn => {}
                    FilterGraphOp::SVGFEBlendHardLight => {}
                    FilterGraphOp::SVGFEBlendSoftLight => {}
                    FilterGraphOp::SVGFEBlendDifference => {}
                    FilterGraphOp::SVGFEBlendExclusion => {}
                    FilterGraphOp::SVGFEBlendHue => {}
                    FilterGraphOp::SVGFEBlendSaturation => {}
                    FilterGraphOp::SVGFEBlendColor => {}
                    FilterGraphOp::SVGFEBlendLuminosity => {}
                    FilterGraphOp::SVGFEColorMatrix{values: matrix} => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            for i in 0..5 {
                                request.push([matrix[i*4], matrix[i*4+1], matrix[i*4+2], matrix[i*4+3]]);
                            }
                        }
                    }
                    FilterGraphOp::SVGFEComponentTransfer => unreachable!(),
                    FilterGraphOp::SVGFEComponentTransferInterned{..} => {}
                    FilterGraphOp::SVGFECompositeArithmetic{k1, k2, k3, k4} => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            request.push([k1, k2, k3, k4]);
                        }
                    }
                    FilterGraphOp::SVGFECompositeATop => {}
                    FilterGraphOp::SVGFECompositeIn => {}
                    FilterGraphOp::SVGFECompositeLighter => {}
                    FilterGraphOp::SVGFECompositeOut => {}
                    FilterGraphOp::SVGFECompositeOver => {}
                    FilterGraphOp::SVGFECompositeXOR => {}
                    FilterGraphOp::SVGFEConvolveMatrixEdgeModeDuplicate{order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha} |
                    FilterGraphOp::SVGFEConvolveMatrixEdgeModeNone{order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha} |
                    FilterGraphOp::SVGFEConvolveMatrixEdgeModeWrap{order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha} => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            request.push([-target_x as f32, -target_y as f32, order_x as f32, order_y as f32]);
                            request.push([kernel_unit_length_x as f32, kernel_unit_length_y as f32, 1.0 / divisor, bias]);
                            assert!(SVGFE_CONVOLVE_VALUES_LIMIT == 25);
                            request.push([kernel[0], kernel[1], kernel[2], kernel[3]]);
                            request.push([kernel[4], kernel[5], kernel[6], kernel[7]]);
                            request.push([kernel[8], kernel[9], kernel[10], kernel[11]]);
                            request.push([kernel[12], kernel[13], kernel[14], kernel[15]]);
                            request.push([kernel[16], kernel[17], kernel[18], kernel[19]]);
                            request.push([kernel[20], 0.0, 0.0, preserve_alpha as f32]);
                        }
                    }
                    FilterGraphOp::SVGFEDiffuseLightingDistant{..} => {}
                    FilterGraphOp::SVGFEDiffuseLightingPoint{..} => {}
                    FilterGraphOp::SVGFEDiffuseLightingSpot{..} => {}
                    FilterGraphOp::SVGFEDisplacementMap{scale, x_channel_selector, y_channel_selector} => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            request.push([x_channel_selector as f32, y_channel_selector as f32, scale, 0.0]);
                        }
                    }
                    FilterGraphOp::SVGFEDropShadow{color, ..} |
                    FilterGraphOp::SVGFEFlood{color} => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            request.push(color.to_array());
                        }
                    }
                    FilterGraphOp::SVGFEGaussianBlur{..} => {}
                    FilterGraphOp::SVGFEIdentity => {}
                    FilterGraphOp::SVGFEImage{..} => {}
                    FilterGraphOp::SVGFEMorphologyDilate{radius_x, radius_y} |
                    FilterGraphOp::SVGFEMorphologyErode{radius_x, radius_y} => {
                        let handle = filter_task.extra_gpu_cache_handle.get_or_insert_with(GpuCacheHandle::new);
                        if let Some(mut request) = gpu_cache.request(handle) {
                            request.push([radius_x, radius_y, 0.0, 0.0]);
                        }
                    }
                    FilterGraphOp::SVGFEOpacity{..} => {}
                    FilterGraphOp::SVGFESourceAlpha => {}
                    FilterGraphOp::SVGFESourceGraphic => {}
                    FilterGraphOp::SVGFESpecularLightingDistant{..} => {}
                    FilterGraphOp::SVGFESpecularLightingPoint{..} => {}
                    FilterGraphOp::SVGFESpecularLightingSpot{..} => {}
                    FilterGraphOp::SVGFETile => {}
                    FilterGraphOp::SVGFEToAlpha{..} => {}
                    FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{..} => {}
                    FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithStitching{..} => {}
                    FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{..} => {}
                    FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching{..} => {}
                }
            }
            _ => {}
        }
    }
}

/// In order to avoid duplicating the down-scaling and blur passes when a picture has several blurs,
/// we use a local (primitive-level) cache of the render tasks generated for a single shadowed primitive
/// in a single frame.
pub type BlurTaskCache = FastHashMap<BlurTaskKey, RenderTaskId>;

/// Since we only use it within a single primitive, the key only needs to contain the down-scaling level
/// and the blur std deviation.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub enum BlurTaskKey {
    DownScale(u32),
    Blur { downscale_level: u32, stddev_x: u32, stddev_y: u32 },
}

impl BlurTaskKey {
    fn downscale_and_blur(downscale_level: u32, blur_stddev: DeviceSize) -> Self {
        // Quantise the std deviations and store it as integers to work around
        // Eq and Hash's f32 allergy.
        // The blur radius is rounded before RenderTask::new_blur so we don't need
        // a lot of precision.
        const QUANTIZATION_FACTOR: f32 = 1024.0;
        let stddev_x = (blur_stddev.width * QUANTIZATION_FACTOR) as u32;
        let stddev_y = (blur_stddev.height * QUANTIZATION_FACTOR) as u32;
        BlurTaskKey::Blur { downscale_level, stddev_x, stddev_y }
    }
}

// The majority of render tasks have 0, 1 or 2 dependencies, except for pictures that
// typically have dozens to hundreds of dependencies. SmallVec with 2 inline elements
// avoids many tiny heap allocations in pages with a lot of text shadows and other
// types of render tasks.
pub type TaskDependencies = SmallVec<[RenderTaskId;2]>;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct MaskSubPass {
    pub clip_node_range: ClipNodeRange,
    pub prim_spatial_node_index: SpatialNodeIndex,
    pub prim_address_f: GpuBufferAddress,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum SubPass {
    Masks {
        masks: MaskSubPass,
    },
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTask {
    pub location: RenderTaskLocation,
    pub children: TaskDependencies,
    pub kind: RenderTaskKind,
    pub sub_pass: Option<SubPass>,

    // TODO(gw): These fields and perhaps others can become private once the
    //           frame_graph / render_task source files are unified / cleaned up.
    pub free_after: PassId,
    pub render_on: PassId,

    /// The gpu cache handle for the render task's destination rect.
    ///
    /// Will be set to None if the render task is cached, in which case the texture cache
    /// manages the handle.
    pub uv_rect_handle: GpuCacheHandle,
    pub cache_handle: Option<RenderTaskCacheEntryHandle>,
    uv_rect_kind: UvRectKind,
}

impl RenderTask {
    pub fn new(
        location: RenderTaskLocation,
        kind: RenderTaskKind,
    ) -> Self {
        render_task_sanity_check(&location.size());

        RenderTask {
            location,
            children: TaskDependencies::new(),
            kind,
            free_after: PassId::MAX,
            render_on: PassId::MIN,
            uv_rect_handle: GpuCacheHandle::new(),
            uv_rect_kind: UvRectKind::Rect,
            cache_handle: None,
            sub_pass: None,
        }
    }

    pub fn new_dynamic(
        size: DeviceIntSize,
        kind: RenderTaskKind,
    ) -> Self {
        assert!(!size.is_empty(), "Bad {} render task size: {:?}", kind.as_str(), size);
        RenderTask::new(
            RenderTaskLocation::Unallocated { size },
            kind,
        )
    }

    pub fn with_uv_rect_kind(mut self, uv_rect_kind: UvRectKind) -> Self {
        self.uv_rect_kind = uv_rect_kind;
        self
    }

    pub fn new_image(
        size: DeviceIntSize,
        request: ImageRequest,
    ) -> Self {
        // Note: this is a special constructor for image render tasks that does not
        // do the render task size sanity check. This is because with SWGL we purposefully
        // avoid tiling large images. There is no upload with SWGL so whatever was
        // successfully allocated earlier will be what shaders read, regardless of the size
        // and copying into tiles would only slow things down.
        // As a result we can run into very large images being added to the frame graph
        // (this is covered by a few reftests on the CI).

        RenderTask {
            location: RenderTaskLocation::CacheRequest { size, },
            children: TaskDependencies::new(),
            kind: RenderTaskKind::Image(request),
            free_after: PassId::MAX,
            render_on: PassId::MIN,
            uv_rect_handle: GpuCacheHandle::new(),
            uv_rect_kind: UvRectKind::Rect,
            cache_handle: None,
            sub_pass: None,
        }
    }


    #[cfg(test)]
    pub fn new_test(
        location: RenderTaskLocation,
        target: RenderTargetKind,
    ) -> Self {
        RenderTask {
            location,
            children: TaskDependencies::new(),
            kind: RenderTaskKind::Test(target),
            free_after: PassId::MAX,
            render_on: PassId::MIN,
            uv_rect_handle: GpuCacheHandle::new(),
            uv_rect_kind: UvRectKind::Rect,
            cache_handle: None,
            sub_pass: None,
        }
    }

    pub fn new_blit(
        size: DeviceIntSize,
        source: RenderTaskId,
        rg_builder: &mut RenderTaskGraphBuilder,
    ) -> RenderTaskId {
        // If this blit uses a render task as a source,
        // ensure it's added as a child task. This will
        // ensure it gets allocated in the correct pass
        // and made available as an input when this task
        // executes.

        let blit_task_id = rg_builder.add().init(RenderTask::new_dynamic(
            size,
            RenderTaskKind::Blit(BlitTask { source }),
        ));

        rg_builder.add_dependency(blit_task_id, source);

        blit_task_id
    }

    // Construct a render task to apply a blur to a primitive.
    // The render task chain that is constructed looks like:
    //
    //    PrimitiveCacheTask: Draw the primitives.
    //           ^
    //           |
    //    DownscalingTask(s): Each downscaling task reduces the size of render target to
    //           ^            half. Also reduce the std deviation to half until the std
    //           |            deviation less than 4.0.
    //           |
    //           |
    //    VerticalBlurTask: Apply the separable vertical blur to the primitive.
    //           ^
    //           |
    //    HorizontalBlurTask: Apply the separable horizontal blur to the vertical blur.
    //           |
    //           +---- This is stored as the input task to the primitive shader.
    //
    pub fn new_blur(
        blur_std_deviation: DeviceSize,
        src_task_id: RenderTaskId,
        rg_builder: &mut RenderTaskGraphBuilder,
        target_kind: RenderTargetKind,
        mut blur_cache: Option<&mut BlurTaskCache>,
        blur_region: DeviceIntSize,
    ) -> RenderTaskId {
        // Adjust large std deviation value.
        let mut adjusted_blur_std_deviation = blur_std_deviation;
        let (blur_target_size, uv_rect_kind) = {
            let src_task = rg_builder.get_task(src_task_id);
            (src_task.location.size(), src_task.uv_rect_kind())
        };
        let mut adjusted_blur_target_size = blur_target_size;
        let mut downscaling_src_task_id = src_task_id;
        let mut scale_factor = 1.0;
        let mut n_downscales = 1;
        while adjusted_blur_std_deviation.width > MAX_BLUR_STD_DEVIATION &&
              adjusted_blur_std_deviation.height > MAX_BLUR_STD_DEVIATION {
            if adjusted_blur_target_size.width < MIN_DOWNSCALING_RT_SIZE ||
               adjusted_blur_target_size.height < MIN_DOWNSCALING_RT_SIZE {
                break;
            }
            adjusted_blur_std_deviation = adjusted_blur_std_deviation * 0.5;
            scale_factor *= 2.0;
            adjusted_blur_target_size = (blur_target_size.to_f32() / scale_factor).to_i32();

            let cached_task = match blur_cache {
                Some(ref mut cache) => cache.get(&BlurTaskKey::DownScale(n_downscales)).cloned(),
                None => None,
            };

            downscaling_src_task_id = cached_task.unwrap_or_else(|| {
                RenderTask::new_scaling(
                    downscaling_src_task_id,
                    rg_builder,
                    target_kind,
                    adjusted_blur_target_size,
                )
            });

            if let Some(ref mut cache) = blur_cache {
                cache.insert(BlurTaskKey::DownScale(n_downscales), downscaling_src_task_id);
            }

            n_downscales += 1;
        }


        let blur_key = BlurTaskKey::downscale_and_blur(n_downscales, adjusted_blur_std_deviation);

        let cached_task = match blur_cache {
            Some(ref mut cache) => cache.get(&blur_key).cloned(),
            None => None,
        };

        let blur_region = blur_region / (scale_factor as i32);

        let blur_task_id = cached_task.unwrap_or_else(|| {
            let blur_task_v = rg_builder.add().init(RenderTask::new_dynamic(
                adjusted_blur_target_size,
                RenderTaskKind::VerticalBlur(BlurTask {
                    blur_std_deviation: adjusted_blur_std_deviation.height,
                    target_kind,
                    blur_region,
                }),
            ).with_uv_rect_kind(uv_rect_kind));
            rg_builder.add_dependency(blur_task_v, downscaling_src_task_id);

            let task_id = rg_builder.add().init(RenderTask::new_dynamic(
                adjusted_blur_target_size,
                RenderTaskKind::HorizontalBlur(BlurTask {
                    blur_std_deviation: adjusted_blur_std_deviation.width,
                    target_kind,
                    blur_region,
                }),
            ).with_uv_rect_kind(uv_rect_kind));
            rg_builder.add_dependency(task_id, blur_task_v);

            task_id
        });

        if let Some(ref mut cache) = blur_cache {
            cache.insert(blur_key, blur_task_id);
        }

        blur_task_id
    }

    pub fn new_scaling(
        src_task_id: RenderTaskId,
        rg_builder: &mut RenderTaskGraphBuilder,
        target_kind: RenderTargetKind,
        size: DeviceIntSize,
    ) -> RenderTaskId {
        Self::new_scaling_with_padding(
            src_task_id,
            rg_builder,
            target_kind,
            size,
            DeviceIntSideOffsets::zero(),
        )
    }

    pub fn new_scaling_with_padding(
        source: RenderTaskId,
        rg_builder: &mut RenderTaskGraphBuilder,
        target_kind: RenderTargetKind,
        padded_size: DeviceIntSize,
        padding: DeviceIntSideOffsets,
    ) -> RenderTaskId {
        let uv_rect_kind = rg_builder.get_task(source).uv_rect_kind();

        let task_id = rg_builder.add().init(
            RenderTask::new_dynamic(
                padded_size,
                RenderTaskKind::Scaling(ScalingTask {
                    target_kind,
                    padding,
                }),
            ).with_uv_rect_kind(uv_rect_kind)
        );

        rg_builder.add_dependency(task_id, source);

        task_id
    }

    pub fn new_svg_filter(
        filter_primitives: &[FilterPrimitive],
        filter_datas: &[SFilterData],
        rg_builder: &mut RenderTaskGraphBuilder,
        content_size: DeviceIntSize,
        uv_rect_kind: UvRectKind,
        original_task_id: RenderTaskId,
        device_pixel_scale: DevicePixelScale,
    ) -> RenderTaskId {

        if filter_primitives.is_empty() {
            return original_task_id;
        }

        // Resolves the input to a filter primitive
        let get_task_input = |
            input: &FilterPrimitiveInput,
            filter_primitives: &[FilterPrimitive],
            rg_builder: &mut RenderTaskGraphBuilder,
            cur_index: usize,
            outputs: &[RenderTaskId],
            original: RenderTaskId,
            color_space: ColorSpace,
        | {
            // TODO(cbrewster): Not sure we can assume that the original input is sRGB.
            let (mut task_id, input_color_space) = match input.to_index(cur_index) {
                Some(index) => (outputs[index], filter_primitives[index].color_space),
                None => (original, ColorSpace::Srgb),
            };

            match (input_color_space, color_space) {
                (ColorSpace::Srgb, ColorSpace::LinearRgb) => {
                    task_id = RenderTask::new_svg_filter_primitive(
                        smallvec![task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::SrgbToLinear,
                        rg_builder,
                    );
                },
                (ColorSpace::LinearRgb, ColorSpace::Srgb) => {
                    task_id = RenderTask::new_svg_filter_primitive(
                        smallvec![task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::LinearToSrgb,
                        rg_builder,
                    );
                },
                _ => {},
            }

            task_id
        };

        let mut outputs = vec![];
        let mut cur_filter_data = 0;
        for (cur_index, primitive) in filter_primitives.iter().enumerate() {
            let render_task_id = match primitive.kind {
                FilterPrimitiveKind::Identity(ref identity) => {
                    // Identity does not create a task, it provides its input's render task
                    get_task_input(
                        &identity.input,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    )
                }
                FilterPrimitiveKind::Blend(ref blend) => {
                    let input_1_task_id = get_task_input(
                        &blend.input1,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );
                    let input_2_task_id = get_task_input(
                        &blend.input2,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    RenderTask::new_svg_filter_primitive(
                        smallvec![input_1_task_id, input_2_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Blend(blend.mode),
                        rg_builder,
                    )
                },
                FilterPrimitiveKind::Flood(ref flood) => {
                    RenderTask::new_svg_filter_primitive(
                        smallvec![],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Flood(flood.color),
                        rg_builder,
                    )
                }
                FilterPrimitiveKind::Blur(ref blur) => {
                    let width_std_deviation = blur.width * device_pixel_scale.0;
                    let height_std_deviation = blur.height * device_pixel_scale.0;
                    let input_task_id = get_task_input(
                        &blur.input,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    RenderTask::new_blur(
                        DeviceSize::new(width_std_deviation, height_std_deviation),
                        // TODO: This is a hack to ensure that a blur task's input is always
                        // in the blur's previous pass.
                        RenderTask::new_svg_filter_primitive(
                            smallvec![input_task_id],
                            content_size,
                            uv_rect_kind,
                            SvgFilterInfo::Identity,
                            rg_builder,
                        ),
                        rg_builder,
                        RenderTargetKind::Color,
                        None,
                        content_size,
                    )
                }
                FilterPrimitiveKind::Opacity(ref opacity) => {
                    let input_task_id = get_task_input(
                        &opacity.input,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    RenderTask::new_svg_filter_primitive(
                        smallvec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Opacity(opacity.opacity),
                        rg_builder,
                    )
                }
                FilterPrimitiveKind::ColorMatrix(ref color_matrix) => {
                    let input_task_id = get_task_input(
                        &color_matrix.input,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    RenderTask::new_svg_filter_primitive(
                        smallvec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::ColorMatrix(Box::new(color_matrix.matrix)),
                        rg_builder,
                    )
                }
                FilterPrimitiveKind::DropShadow(ref drop_shadow) => {
                    let input_task_id = get_task_input(
                        &drop_shadow.input,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let blur_std_deviation = drop_shadow.shadow.blur_radius * device_pixel_scale.0;
                    let offset = drop_shadow.shadow.offset * LayoutToWorldScale::new(1.0) * device_pixel_scale;

                    let offset_task_id = RenderTask::new_svg_filter_primitive(
                        smallvec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Offset(offset),
                        rg_builder,
                    );

                    let blur_task_id = RenderTask::new_blur(
                        DeviceSize::new(blur_std_deviation, blur_std_deviation),
                        offset_task_id,
                        rg_builder,
                        RenderTargetKind::Color,
                        None,
                        content_size,
                    );

                    RenderTask::new_svg_filter_primitive(
                        smallvec![input_task_id, blur_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::DropShadow(drop_shadow.shadow.color),
                        rg_builder,
                    )
                }
                FilterPrimitiveKind::ComponentTransfer(ref component_transfer) => {
                    let input_task_id = get_task_input(
                        &component_transfer.input,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let filter_data = &filter_datas[cur_filter_data];
                    cur_filter_data += 1;
                    if filter_data.is_identity() {
                        input_task_id
                    } else {
                        RenderTask::new_svg_filter_primitive(
                            smallvec![input_task_id],
                            content_size,
                            uv_rect_kind,
                            SvgFilterInfo::ComponentTransfer(filter_data.clone()),
                            rg_builder,
                        )
                    }
                }
                FilterPrimitiveKind::Offset(ref info) => {
                    let input_task_id = get_task_input(
                        &info.input,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    let offset = info.offset * LayoutToWorldScale::new(1.0) * device_pixel_scale;
                    RenderTask::new_svg_filter_primitive(
                        smallvec![input_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Offset(offset),
                        rg_builder,
                    )
                }
                FilterPrimitiveKind::Composite(info) => {
                    let input_1_task_id = get_task_input(
                        &info.input1,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );
                    let input_2_task_id = get_task_input(
                        &info.input2,
                        filter_primitives,
                        rg_builder,
                        cur_index,
                        &outputs,
                        original_task_id,
                        primitive.color_space
                    );

                    RenderTask::new_svg_filter_primitive(
                        smallvec![input_1_task_id, input_2_task_id],
                        content_size,
                        uv_rect_kind,
                        SvgFilterInfo::Composite(info.operator),
                        rg_builder,
                    )
                }
            };
            outputs.push(render_task_id);
        }

        // The output of a filter is the output of the last primitive in the chain.
        let mut render_task_id = *outputs.last().unwrap();

        // Convert to sRGB if needed
        if filter_primitives.last().unwrap().color_space == ColorSpace::LinearRgb {
            render_task_id = RenderTask::new_svg_filter_primitive(
                smallvec![render_task_id],
                content_size,
                uv_rect_kind,
                SvgFilterInfo::LinearToSrgb,
                rg_builder,
            );
        }

        render_task_id
    }

    pub fn new_svg_filter_primitive(
        tasks: TaskDependencies,
        target_size: DeviceIntSize,
        uv_rect_kind: UvRectKind,
        info: SvgFilterInfo,
        rg_builder: &mut RenderTaskGraphBuilder,
    ) -> RenderTaskId {
        let task_id = rg_builder.add().init(RenderTask::new_dynamic(
            target_size,
            RenderTaskKind::SvgFilter(SvgFilterTask {
                extra_gpu_cache_handle: None,
                info,
            }),
        ).with_uv_rect_kind(uv_rect_kind));

        for child_id in tasks {
            rg_builder.add_dependency(task_id, child_id);
        }

        task_id
    }

    pub fn add_sub_pass(
        &mut self,
        sub_pass: SubPass,
    ) {
        assert!(self.sub_pass.is_none(), "multiple sub-passes are not supported for now");
        self.sub_pass = Some(sub_pass);
    }

    /// Creates render tasks from PictureCompositeMode::SVGFEGraph.
    ///
    /// The interesting parts of the handling of SVG filters are:
    /// * scene_building.rs : wrap_prim_with_filters
    /// * picture.rs : get_coverage_svgfe
    /// * render_task.rs : new_svg_filter_graph (you are here)
    /// * render_target.rs : add_svg_filter_node_instances
    pub fn new_svg_filter_graph(
        filter_nodes: &[(FilterGraphNode, FilterGraphOp)],
        frame_state: &mut FrameBuildingState,
        data_stores: &mut DataStores,
        uv_rect_kind: UvRectKind,
        original_task_id: RenderTaskId,
        _surface_rects_task_size: DeviceIntSize,
        surface_rects_clipped: DeviceRect,
        surface_rects_clipped_local: PictureRect,
    ) -> RenderTaskId {
        const BUFFER_LIMIT: usize = 256;
        let mut task_by_buffer_id: [RenderTaskId; BUFFER_LIMIT] = [RenderTaskId::INVALID; BUFFER_LIMIT];
        let mut subregion_by_buffer_id: [LayoutRect; BUFFER_LIMIT] = [LayoutRect::zero(); BUFFER_LIMIT];
        // If nothing replaces this value (all node subregions are empty), we
        // can just return the original picture
        let mut output_task_id = original_task_id;

        // By this point we assume the following about the graph:
        // * BUFFER_LIMIT here should be >= BUFFER_LIMIT in the scene_building.rs code.
        // * input buffer id < output buffer id
        // * output buffer id between 0 and BUFFER_LIMIT
        // * the number of filter_datas matches the number of kept nodes with op
        //   SVGFEComponentTransfer.
        //
        // These assumptions are verified with asserts in this function as
        // appropriate.

        // Converts a UvRectKind::Quad to a subregion, we need this for
        // SourceGraphic because it could source from a larger image when doing
        // a dirty rect update.  In theory this can be used for blur output as
        // well but it doesn't seem to be necessary from early testing.
        //
        // See calculate_uv_rect_kind in picture.rs for how these were generated.
        fn subregion_for_uvrectkind(kind: &UvRectKind, rect: LayoutRect) -> LayoutRect {
            let used =
            match kind {
                UvRectKind::Quad{top_left: tl, top_right: _tr, bottom_left: _bl, bottom_right: br} => {
                    LayoutRect::new(
                        LayoutPoint::new(
                            rect.min.x + rect.width() * tl.x / tl.w,
                            rect.min.y + rect.height() * tl.y / tl.w,
                        ),
                        LayoutPoint::new(
                            rect.min.x + rect.width() * br.x / br.w,
                            rect.min.y + rect.height() * br.y / br.w,
                        ),
                    )
                }
                UvRectKind::Rect => {
                    rect
                }
            };
            // For some reason, the following test passes a uv_rect_kind that
            // resolves to [-.2, -.2, -.2, -.2]
            // reftest layout/reftests/svg/filters/dynamic-filter-invalidation-01.svg
            match used.is_empty() {
                true => rect,
                false => used,
            }
        }

        // Make a UvRectKind::Quad that represents a task for a node, which may
        // have an inflate border, must be a Quad because the surface_rects
        // compositing shader expects it to be one, we don't actually use this
        // internally as we use subregions, see calculate_uv_rect_kind for how
        // this works, it projects from clipped rect to unclipped rect, where
        // our clipped rect is simply task_size minus the inflate, and unclipped
        // is our full task_size
        fn uv_rect_kind_for_task_size(task_size: DeviceIntSize, inflate: i16) -> UvRectKind {
            let unclipped = DeviceRect::new(
                DevicePoint::new(
                    inflate as f32,
                    inflate as f32,
                ),
                DevicePoint::new(
                    task_size.width as f32 - inflate as f32,
                    task_size.height as f32 - inflate as f32,
                ),
            );
            let clipped = DeviceRect::new(
                DevicePoint::zero(),
                DevicePoint::new(
                    task_size.width as f32,
                    task_size.height as f32,
                ),
            );
            let scale_x = 1.0 / clipped.width();
            let scale_y = 1.0 / clipped.height();
            UvRectKind::Quad{
                top_left: DeviceHomogeneousVector::new(
                    (unclipped.min.x - clipped.min.x) * scale_x,
                    (unclipped.min.y - clipped.min.y) * scale_y,
                    0.0, 1.0),
                top_right: DeviceHomogeneousVector::new(
                    (unclipped.max.x - clipped.min.x) * scale_x,
                    (unclipped.min.y - clipped.min.y) * scale_y,
                    0.0, 1.0),
                bottom_left: DeviceHomogeneousVector::new(
                    (unclipped.min.x - clipped.min.x) * scale_x,
                    (unclipped.max.y - clipped.min.y) * scale_y,
                    0.0, 1.0),
                bottom_right: DeviceHomogeneousVector::new(
                    (unclipped.max.x - clipped.min.x) * scale_x,
                    (unclipped.max.y - clipped.min.y) * scale_y,
                    0.0, 1.0),
            }
        }

        // Determine the local space to device pixel scaling in the most robust
        // way, this accounts for local to device transform and
        // device_pixel_scale (if the task is shrunk in get_surface_rects).
        //
        // This has some precision issues because surface_rects_clipped was
        // rounded already, so it's not exactly the same transform that
        // get_surface_rects performed, but it is very close, since it is not
        // quite the same we have to round the offset a certain way to avoid
        // introducing subpixel offsets caused by the slight deviation.
        let subregion_to_device_scale_x = surface_rects_clipped.width() / surface_rects_clipped_local.width();
        let subregion_to_device_scale_y = surface_rects_clipped.height() / surface_rects_clipped_local.height();
        let subregion_to_device_offset_x = surface_rects_clipped.min.x - (surface_rects_clipped_local.min.x * subregion_to_device_scale_x).floor();
        let subregion_to_device_offset_y = surface_rects_clipped.min.y - (surface_rects_clipped_local.min.y * subregion_to_device_scale_y).floor();

        // We will treat the entire SourceGraphic coordinate space as being this
        // subregion, which is how large the source picture task is.
        let filter_subregion: LayoutRect = surface_rects_clipped.cast_unit();

        // Calculate the used subregion (invalidation rect) for SourceGraphic
        // that we are painting for, the intermediate task sizes are based on
        // this portion of SourceGraphic, this also serves as a clip on the
        // SourceGraphic, which is necessary for this reftest:
        // layout/reftests/svg/filters/svg-filter-chains/clip-original-SourceGraphic.svg
        let source_subregion =
            subregion_for_uvrectkind(
                &uv_rect_kind,
                surface_rects_clipped.cast_unit(),
            )
            .intersection(&filter_subregion)
            .unwrap_or(LayoutRect::zero())
            .round_out();

        // This is the rect for the output picture we are producing
        let output_rect = filter_subregion.to_i32();
        // Output to the same subregion we were provided
        let output_subregion = filter_subregion;

        // Iterate the filter nodes and create tasks
        let mut made_dependency_on_source = false;
        for (filter_index, (filter_node, op)) in filter_nodes.iter().enumerate() {
            let node = &filter_node;
            let is_output = filter_index == filter_nodes.len() - 1;

            // Note that this is never set on the final output by design.
            if !node.kept_by_optimizer {
                continue;
            }

            // Certain ops have parameters that need to be scaled to device
            // space.
            let op = match op {
                FilterGraphOp::SVGFEBlendColor => op.clone(),
                FilterGraphOp::SVGFEBlendColorBurn => op.clone(),
                FilterGraphOp::SVGFEBlendColorDodge => op.clone(),
                FilterGraphOp::SVGFEBlendDarken => op.clone(),
                FilterGraphOp::SVGFEBlendDifference => op.clone(),
                FilterGraphOp::SVGFEBlendExclusion => op.clone(),
                FilterGraphOp::SVGFEBlendHardLight => op.clone(),
                FilterGraphOp::SVGFEBlendHue => op.clone(),
                FilterGraphOp::SVGFEBlendLighten => op.clone(),
                FilterGraphOp::SVGFEBlendLuminosity => op.clone(),
                FilterGraphOp::SVGFEBlendMultiply => op.clone(),
                FilterGraphOp::SVGFEBlendNormal => op.clone(),
                FilterGraphOp::SVGFEBlendOverlay => op.clone(),
                FilterGraphOp::SVGFEBlendSaturation => op.clone(),
                FilterGraphOp::SVGFEBlendScreen => op.clone(),
                FilterGraphOp::SVGFEBlendSoftLight => op.clone(),
                FilterGraphOp::SVGFEColorMatrix{..} => op.clone(),
                FilterGraphOp::SVGFEComponentTransfer => unreachable!(),
                FilterGraphOp::SVGFEComponentTransferInterned{..} => op.clone(),
                FilterGraphOp::SVGFECompositeArithmetic{..} => op.clone(),
                FilterGraphOp::SVGFECompositeATop => op.clone(),
                FilterGraphOp::SVGFECompositeIn => op.clone(),
                FilterGraphOp::SVGFECompositeLighter => op.clone(),
                FilterGraphOp::SVGFECompositeOut => op.clone(),
                FilterGraphOp::SVGFECompositeOver => op.clone(),
                FilterGraphOp::SVGFECompositeXOR => op.clone(),
                FilterGraphOp::SVGFEConvolveMatrixEdgeModeDuplicate{
                    kernel_unit_length_x, kernel_unit_length_y, order_x,
                    order_y, kernel, divisor, bias, target_x, target_y,
                    preserve_alpha} => {
                    FilterGraphOp::SVGFEConvolveMatrixEdgeModeDuplicate{
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        order_x: *order_x, order_y: *order_y, kernel: *kernel,
                        divisor: *divisor, bias: *bias, target_x: *target_x,
                        target_y: *target_y, preserve_alpha: *preserve_alpha}
                },
                FilterGraphOp::SVGFEConvolveMatrixEdgeModeNone{
                    kernel_unit_length_x, kernel_unit_length_y, order_x,
                    order_y, kernel, divisor, bias, target_x, target_y,
                    preserve_alpha} => {
                    FilterGraphOp::SVGFEConvolveMatrixEdgeModeNone{
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        order_x: *order_x, order_y: *order_y, kernel: *kernel,
                        divisor: *divisor, bias: *bias, target_x: *target_x,
                        target_y: *target_y, preserve_alpha: *preserve_alpha}
                },
                FilterGraphOp::SVGFEConvolveMatrixEdgeModeWrap{
                    kernel_unit_length_x, kernel_unit_length_y, order_x,
                    order_y, kernel, divisor, bias, target_x, target_y,
                    preserve_alpha} => {
                    FilterGraphOp::SVGFEConvolveMatrixEdgeModeWrap{
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        order_x: *order_x, order_y: *order_y, kernel: *kernel,
                        divisor: *divisor, bias: *bias, target_x: *target_x,
                        target_y: *target_y, preserve_alpha: *preserve_alpha}
                },
                FilterGraphOp::SVGFEDiffuseLightingDistant{
                    surface_scale, diffuse_constant, kernel_unit_length_x,
                    kernel_unit_length_y, azimuth, elevation} => {
                    FilterGraphOp::SVGFEDiffuseLightingDistant{
                        surface_scale: *surface_scale,
                        diffuse_constant: *diffuse_constant,
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        azimuth: *azimuth, elevation: *elevation}
                },
                FilterGraphOp::SVGFEDiffuseLightingPoint{
                    surface_scale, diffuse_constant, kernel_unit_length_x,
                    kernel_unit_length_y, x, y, z} => {
                    FilterGraphOp::SVGFEDiffuseLightingPoint{
                        surface_scale: *surface_scale,
                        diffuse_constant: *diffuse_constant,
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        x: x * subregion_to_device_scale_x + subregion_to_device_offset_x,
                        y: y * subregion_to_device_scale_y + subregion_to_device_offset_y,
                        z: *z}
                },
                FilterGraphOp::SVGFEDiffuseLightingSpot{
                    surface_scale, diffuse_constant, kernel_unit_length_x,
                    kernel_unit_length_y, x, y, z, points_at_x, points_at_y,
                    points_at_z, cone_exponent, limiting_cone_angle} => {
                    FilterGraphOp::SVGFEDiffuseLightingSpot{
                        surface_scale: *surface_scale,
                        diffuse_constant: *diffuse_constant,
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        x: x * subregion_to_device_scale_x + subregion_to_device_offset_x,
                        y: y * subregion_to_device_scale_y + subregion_to_device_offset_y,
                        z: *z,
                        points_at_x: points_at_x * subregion_to_device_scale_x + subregion_to_device_offset_x,
                        points_at_y: points_at_y * subregion_to_device_scale_y + subregion_to_device_offset_y,
                        points_at_z: *points_at_z,
                        cone_exponent: *cone_exponent,
                        limiting_cone_angle: *limiting_cone_angle}
                },
                FilterGraphOp::SVGFEFlood{..} => op.clone(),
                FilterGraphOp::SVGFEDisplacementMap{
                    scale, x_channel_selector, y_channel_selector} => {
                    FilterGraphOp::SVGFEDisplacementMap{
                        scale: scale * subregion_to_device_scale_x,
                        x_channel_selector: *x_channel_selector,
                        y_channel_selector: *y_channel_selector}
                },
                FilterGraphOp::SVGFEDropShadow{
                    color, dx, dy, std_deviation_x, std_deviation_y} => {
                    FilterGraphOp::SVGFEDropShadow{
                        color: *color,
                        dx: dx * subregion_to_device_scale_x,
                        dy: dy * subregion_to_device_scale_y,
                        std_deviation_x: std_deviation_x * subregion_to_device_scale_x,
                        std_deviation_y: std_deviation_y * subregion_to_device_scale_y}
                },
                FilterGraphOp::SVGFEGaussianBlur{std_deviation_x, std_deviation_y} => {
                    let std_deviation_x = std_deviation_x * subregion_to_device_scale_x;
                    let std_deviation_y = std_deviation_y * subregion_to_device_scale_y;
                    // For blurs that effectively have no radius in display
                    // space, we can convert to identity.
                    if std_deviation_x + std_deviation_y >= 0.125 {
                        FilterGraphOp::SVGFEGaussianBlur{
                            std_deviation_x,
                            std_deviation_y}
                    } else {
                        FilterGraphOp::SVGFEIdentity
                    }
                },
                FilterGraphOp::SVGFEIdentity => op.clone(),
                FilterGraphOp::SVGFEImage{..} => op.clone(),
                FilterGraphOp::SVGFEMorphologyDilate{radius_x, radius_y} => {
                    FilterGraphOp::SVGFEMorphologyDilate{
                        radius_x: (radius_x * subregion_to_device_scale_x).round(),
                        radius_y: (radius_y * subregion_to_device_scale_y).round()}
                },
                FilterGraphOp::SVGFEMorphologyErode{radius_x, radius_y} => {
                    FilterGraphOp::SVGFEMorphologyErode{
                        radius_x: (radius_x * subregion_to_device_scale_x).round(),
                        radius_y: (radius_y * subregion_to_device_scale_y).round()}
                },
                FilterGraphOp::SVGFEOpacity{..} => op.clone(),
                FilterGraphOp::SVGFESourceAlpha => op.clone(),
                FilterGraphOp::SVGFESourceGraphic => op.clone(),
                FilterGraphOp::SVGFESpecularLightingDistant{
                    surface_scale, specular_constant, specular_exponent,
                    kernel_unit_length_x, kernel_unit_length_y, azimuth,
                    elevation} => {
                    FilterGraphOp::SVGFESpecularLightingDistant{
                        surface_scale: *surface_scale,
                        specular_constant: *specular_constant,
                        specular_exponent: *specular_exponent,
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        azimuth: *azimuth, elevation: *elevation}
                },
                FilterGraphOp::SVGFESpecularLightingPoint{
                    surface_scale, specular_constant, specular_exponent,
                    kernel_unit_length_x, kernel_unit_length_y, x, y, z } => {
                    FilterGraphOp::SVGFESpecularLightingPoint{
                        surface_scale: *surface_scale,
                        specular_constant: *specular_constant,
                        specular_exponent: *specular_exponent,
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        x: x * subregion_to_device_scale_x + subregion_to_device_offset_x,
                        y: y * subregion_to_device_scale_y + subregion_to_device_offset_y,
                        z: *z }
                },
                FilterGraphOp::SVGFESpecularLightingSpot{
                    surface_scale, specular_constant, specular_exponent,
                    kernel_unit_length_x, kernel_unit_length_y, x, y, z,
                    points_at_x, points_at_y, points_at_z, cone_exponent,
                    limiting_cone_angle} => {
                    FilterGraphOp::SVGFESpecularLightingSpot{
                        surface_scale: *surface_scale,
                        specular_constant: *specular_constant,
                        specular_exponent: *specular_exponent,
                        kernel_unit_length_x:
                            (kernel_unit_length_x * subregion_to_device_scale_x).round(),
                        kernel_unit_length_y:
                            (kernel_unit_length_y * subregion_to_device_scale_y).round(),
                        x: x * subregion_to_device_scale_x + subregion_to_device_offset_x,
                        y: y * subregion_to_device_scale_y + subregion_to_device_offset_y,
                        z: *z,
                        points_at_x: points_at_x * subregion_to_device_scale_x + subregion_to_device_offset_x,
                        points_at_y: points_at_y * subregion_to_device_scale_y + subregion_to_device_offset_y,
                        points_at_z: *points_at_z,
                        cone_exponent: *cone_exponent,
                        limiting_cone_angle: *limiting_cone_angle}
                },
                FilterGraphOp::SVGFETile => op.clone(),
                FilterGraphOp::SVGFEToAlpha => op.clone(),
                FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{
                    base_frequency_x, base_frequency_y, num_octaves, seed} => {
                    FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{
                        base_frequency_x:
                            base_frequency_x * subregion_to_device_scale_x,
                        base_frequency_y:
                            base_frequency_y * subregion_to_device_scale_y,
                        num_octaves: *num_octaves, seed: *seed}
                },
                FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithStitching{
                    base_frequency_x, base_frequency_y, num_octaves, seed} => {
                    FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{
                        base_frequency_x:
                            base_frequency_x * subregion_to_device_scale_x,
                        base_frequency_y:
                            base_frequency_y * subregion_to_device_scale_y,
                        num_octaves: *num_octaves, seed: *seed}
                },
                FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{
                    base_frequency_x, base_frequency_y, num_octaves, seed} => {
                    FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{
                        base_frequency_x:
                            base_frequency_x * subregion_to_device_scale_x,
                        base_frequency_y:
                            base_frequency_y * subregion_to_device_scale_y,
                        num_octaves: *num_octaves, seed: *seed}
                },
                FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching{
                    base_frequency_x, base_frequency_y, num_octaves, seed} => {
                    FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{
                        base_frequency_x:
                            base_frequency_x * subregion_to_device_scale_x,
                        base_frequency_y:
                            base_frequency_y * subregion_to_device_scale_y,
                        num_octaves: *num_octaves, seed: *seed}
                },
            };

            // Process the inputs and figure out their new subregion, because
            // the SourceGraphic subregion is smaller than it was in scene build
            // now that it reflects the invalidation rect
            //
            // Also look up the child tasks while we are here.
            let mut used_subregion = LayoutRect::zero();
            let node_inputs: Vec<(FilterGraphPictureReference, RenderTaskId)> = node.inputs.iter().map(|input| {
                let (subregion, task) =
                    match input.buffer_id {
                        FilterOpGraphPictureBufferId::BufferId(id) => {
                            (subregion_by_buffer_id[id as usize], task_by_buffer_id[id as usize])
                        }
                        FilterOpGraphPictureBufferId::None => {
                            // Task must resolve so we use the SourceGraphic as
                            // a placeholder for these, they don't actually
                            // contribute anything to the output
                            (LayoutRect::zero(), original_task_id)
                        }
                    };
                // Convert offset to device coordinates.
                let offset = LayoutVector2D::new(
                        (input.offset.x * subregion_to_device_scale_x).round(),
                        (input.offset.y * subregion_to_device_scale_y).round(),
                    );
                // To figure out the portion of the node subregion used by this
                // source image we need to apply the target padding.  Note that
                // this does not affect the subregion of the input, as that
                // can't be modified as it is used for placement (offset).
                let target_padding = input.target_padding
                    .scale(subregion_to_device_scale_x, subregion_to_device_scale_y)
                    .round();
                let target_subregion =
                    LayoutRect::new(
                        LayoutPoint::new(
                            subregion.min.x + target_padding.min.x,
                            subregion.min.y + target_padding.min.y,
                        ),
                        LayoutPoint::new(
                            subregion.max.x + target_padding.max.x,
                            subregion.max.y + target_padding.max.y,
                        ),
                    );
                used_subregion = used_subregion.union(&target_subregion);
                (FilterGraphPictureReference{
                    buffer_id: input.buffer_id,
                    // Apply offset to the placement of the input subregion.
                    subregion: subregion.translate(offset),
                    offset: LayoutVector2D::zero(),
                    inflate: input.inflate,
                    // Nothing past this point uses the padding.
                    source_padding: LayoutRect::zero(),
                    target_padding: LayoutRect::zero(),
                }, task)
            }).collect();

            // Convert subregion from PicturePixels to DevicePixels and round.
            let full_subregion = node.subregion
                .scale(subregion_to_device_scale_x, subregion_to_device_scale_y)
                .translate(LayoutVector2D::new(subregion_to_device_offset_x, subregion_to_device_offset_y))
                .round();

            // Clip the used subregion we calculated from the inputs to fit
            // within the node's specified subregion.
            used_subregion = used_subregion
                .intersection(&full_subregion)
                .unwrap_or(LayoutRect::zero())
                .round();

            // Certain filters need to override the used_subregion directly.
            match op {
                FilterGraphOp::SVGFEBlendColor => {},
                FilterGraphOp::SVGFEBlendColorBurn => {},
                FilterGraphOp::SVGFEBlendColorDodge => {},
                FilterGraphOp::SVGFEBlendDarken => {},
                FilterGraphOp::SVGFEBlendDifference => {},
                FilterGraphOp::SVGFEBlendExclusion => {},
                FilterGraphOp::SVGFEBlendHardLight => {},
                FilterGraphOp::SVGFEBlendHue => {},
                FilterGraphOp::SVGFEBlendLighten => {},
                FilterGraphOp::SVGFEBlendLuminosity => {},
                FilterGraphOp::SVGFEBlendMultiply => {},
                FilterGraphOp::SVGFEBlendNormal => {},
                FilterGraphOp::SVGFEBlendOverlay => {},
                FilterGraphOp::SVGFEBlendSaturation => {},
                FilterGraphOp::SVGFEBlendScreen => {},
                FilterGraphOp::SVGFEBlendSoftLight => {},
                FilterGraphOp::SVGFEColorMatrix{values} => {
                    if values[3] != 0.0 ||
                        values[7] != 0.0 ||
                        values[11] != 0.0 ||
                        values[15] != 1.0 ||
                        values[19] != 0.0 {
                        // Manipulating alpha can easily create new
                        // pixels outside of input subregions
                        used_subregion = full_subregion;
                    }
                },
                FilterGraphOp::SVGFEComponentTransfer => unreachable!(),
                FilterGraphOp::SVGFEComponentTransferInterned{handle: _, creates_pixels} => {
                    // Check if the value of alpha[0] is modified, if so
                    // the whole subregion is used because it will be
                    // creating new pixels outside of input subregions
                    if creates_pixels {
                        used_subregion = full_subregion;
                    }
                },
                FilterGraphOp::SVGFECompositeArithmetic { k1, k2, k3, k4 } => {
                    // Optimize certain cases of Arithmetic operator
                    //
                    // See logic for SVG_FECOMPOSITE_OPERATOR_ARITHMETIC
                    // in FilterSupport.cpp for more information.
                    //
                    // Any other case uses the union of input subregions
                    if k4 != 0.0 {
                        // Can produce pixels anywhere in the subregion.
                        used_subregion = full_subregion;
                    } else  if k1 != 0.0 && k2 == 0.0 && k3 == 0.0 && k4 == 0.0 {
                        // Can produce pixels where both exist.
                        used_subregion = full_subregion
                            .intersection(&node_inputs[0].0.subregion)
                            .unwrap_or(LayoutRect::zero())
                            .intersection(&node_inputs[1].0.subregion)
                            .unwrap_or(LayoutRect::zero());
                    }
                    else if k2 != 0.0 && k3 == 0.0 && k4 == 0.0 {
                        // Can produce pixels where source exists.
                        used_subregion = full_subregion
                            .intersection(&node_inputs[0].0.subregion)
                            .unwrap_or(LayoutRect::zero());
                    }
                    else if k2 == 0.0 && k3 != 0.0 && k4 == 0.0 {
                        // Can produce pixels where background exists.
                        used_subregion = full_subregion
                            .intersection(&node_inputs[1].0.subregion)
                            .unwrap_or(LayoutRect::zero());
                    }
                },
                FilterGraphOp::SVGFECompositeATop => {
                    // Can only produce pixels where background exists.
                    used_subregion = full_subregion
                        .intersection(&node_inputs[1].0.subregion)
                        .unwrap_or(LayoutRect::zero());
                },
                FilterGraphOp::SVGFECompositeIn => {
                    // Can only produce pixels where both exist.
                    used_subregion = used_subregion
                        .intersection(&node_inputs[0].0.subregion)
                        .unwrap_or(LayoutRect::zero())
                        .intersection(&node_inputs[1].0.subregion)
                        .unwrap_or(LayoutRect::zero());
                },
                FilterGraphOp::SVGFECompositeLighter => {},
                FilterGraphOp::SVGFECompositeOut => {
                    // Can only produce pixels where source exists.
                    used_subregion = full_subregion
                        .intersection(&node_inputs[0].0.subregion)
                        .unwrap_or(LayoutRect::zero());
                },
                FilterGraphOp::SVGFECompositeOver => {},
                FilterGraphOp::SVGFECompositeXOR => {},
                FilterGraphOp::SVGFEConvolveMatrixEdgeModeDuplicate{..} => {},
                FilterGraphOp::SVGFEConvolveMatrixEdgeModeNone{..} => {},
                FilterGraphOp::SVGFEConvolveMatrixEdgeModeWrap{..} => {},
                FilterGraphOp::SVGFEDiffuseLightingDistant{..} => {},
                FilterGraphOp::SVGFEDiffuseLightingPoint{..} => {},
                FilterGraphOp::SVGFEDiffuseLightingSpot{..} => {},
                FilterGraphOp::SVGFEDisplacementMap{..} => {},
                FilterGraphOp::SVGFEDropShadow{..} => {},
                FilterGraphOp::SVGFEFlood { color } => {
                    // Subregion needs to be set to the full node
                    // subregion for fills (unless the fill is a no-op),
                    // we know at this point that it has no inputs, so the
                    // used_region is empty unless we set it here.
                    if color.a > 0.0 {
                        used_subregion = full_subregion;
                    }
                },
                FilterGraphOp::SVGFEIdentity => {},
                FilterGraphOp::SVGFEImage { sampling_filter: _sampling_filter, matrix: _matrix } => {
                    // TODO: calculate the actual subregion
                    used_subregion = full_subregion;
                },
                FilterGraphOp::SVGFEGaussianBlur{..} => {},
                FilterGraphOp::SVGFEMorphologyDilate{..} => {},
                FilterGraphOp::SVGFEMorphologyErode{..} => {},
                FilterGraphOp::SVGFEOpacity{valuebinding: _valuebinding, value} => {
                    // If fully transparent, we can ignore this node
                    if value <= 0.0 {
                        used_subregion = LayoutRect::zero();
                    }
                },
                FilterGraphOp::SVGFESourceAlpha |
                FilterGraphOp::SVGFESourceGraphic => {
                    used_subregion = source_subregion;
                },
                FilterGraphOp::SVGFESpecularLightingDistant{..} => {},
                FilterGraphOp::SVGFESpecularLightingPoint{..} => {},
                FilterGraphOp::SVGFESpecularLightingSpot{..} => {},
                FilterGraphOp::SVGFETile => {
                    if !used_subregion.is_empty() {
                        // This fills the entire target, at least if there are
                        // any input pixels to work with.
                        used_subregion = full_subregion;
                    }
                },
                FilterGraphOp::SVGFEToAlpha => {},
                FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{..} |
                FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithStitching{..} |
                FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{..} |
                FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching{..} => {
                    // Turbulence produces pixel values throughout the
                    // node subregion.
                    used_subregion = full_subregion;
                },
            }

            // If this is the output node, we have to match the provided filter
            // subregion as the primitive it is applied to is already placed (it
            // was calculated in get_surface_rects using get_coverage_svgfe).
            let node_subregion = match is_output {
                true => output_subregion,
                false => used_subregion,
            };

            // Convert subregion from layout pixels to integer device pixels and
            // then calculate size afterwards so it reflects the used pixel area
            //
            // In case of the output node we preserve the exact filter_subregion
            // task size.
            //
            // This can be an empty rect if the source_subregion invalidation
            // rect didn't request any pixels of this node, but we can't skip
            // creating tasks that have no size because they would leak in the
            // render task graph with no consumers
            let node_task_rect =
                match is_output {
                    true => output_rect,
                    false => node_subregion.to_i32(),
                };

            // SVG spec requires that a later node sampling pixels outside
            // this node's subregion will receive a transparent black color
            // for those samples, we achieve this by adding a 1 pixel border
            // around the target rect, which works fine with the clamping of the
            // texture fetch in the shader, and to account for the offset we
            // have to make a UvRectKind::Quad mapping for later nodes to use
            // when sampling this output, if they use feOffset or have a
            // larger target rect those samples will be clamped to the
            // transparent black border and thus meet spec.
            let mut node_task_size = node_task_rect.size().cast_unit();

            // We have to limit the render target sizes we're asking for on the
            // intermediate nodes; it's not feasible to allocate extremely large
            // surfaces.  Note that the SVGFEFilterTask code can adapt to any
            // scaling that we use here, input subregions simply have to be in
            // the same space as the target subregion, which we're not changing,
            // and operator parameters like kernel_unit_length are also in that
            // space.  Blurs will do this same logic if their intermediate is
            // too large.  We use a simple halving calculation here so that
            // pixel alignment is still vaguely sensible.
            while node_task_size.width as usize + node.inflate as usize * 2 > MAX_SURFACE_SIZE ||
                node_task_size.height as usize + node.inflate as usize * 2 > MAX_SURFACE_SIZE {
                node_task_size.width >>= 1;
                node_task_size.height >>= 1;
            }
            // Add the inflate border
            node_task_size.width += node.inflate as i32 * 2;
            node_task_size.height += node.inflate as i32 * 2;

            // Make the uv_rect_kind for this node's task to use, this matters
            // only on the final node because we don't use it internally
            let node_uv_rect_kind =
                uv_rect_kind_for_task_size(node_task_size, node.inflate);

            // Create task for this node
            let task_id;
            match op {
                FilterGraphOp::SVGFEGaussianBlur { std_deviation_x, std_deviation_y } => {
                    // Note: wrap_prim_with_filters copies the SourceGraphic to
                    // a node to apply the transparent border around the image,
                    // we rely on that behavior here as the Blur filter is a
                    // different shader without awareness of the subregion
                    // rules in the SVG spec.

                    // Find the input task id
                    assert!(node_inputs.len() == 1);
                    let blur_input = &node_inputs[0].0;
                    let source_task_id = node_inputs[0].1;

                    // We have to make a copy of the input that is padded with
                    // transparent black for the area outside the subregion, so
                    // that the blur task does not duplicate at the edges, and
                    // this is also where we have to adjust size to account for
                    // for downscaling of the image in the blur task to avoid
                    // introducing sampling artifacts on the downscale
                    let mut adjusted_blur_std_deviation = DeviceSize::new(
                        std_deviation_x,
                        std_deviation_y,
                    );
                    let blur_subregion = blur_input.subregion
                        .inflate(
                            std_deviation_x.ceil() * BLUR_SAMPLE_SCALE,
                            std_deviation_y.ceil() * BLUR_SAMPLE_SCALE);
                    let blur_task_size = blur_subregion.size().cast_unit();
                    // Adjust task size to prevent potential sampling errors
                    let mut adjusted_blur_task_size =
                        BlurTask::adjusted_blur_source_size(
                            blur_task_size,
                            adjusted_blur_std_deviation,
                        );
                    // Now change the subregion to match the revised task size,
                    // keeping it centered should keep animated radius smooth.
                    let corner = LayoutPoint::new(
                            blur_subregion.min.x + ((
                                blur_task_size.width as i32 -
                                adjusted_blur_task_size.width) / 2) as f32,
                            blur_subregion.min.y + ((
                                blur_task_size.height as i32 -
                                adjusted_blur_task_size.height) / 2) as f32,
                        )
                        .floor();
                    // Recalculate the blur_subregion to match, note that if the
                    // task was downsized it doesn't affect the size of this
                    // rect, so we don't have to scale blur_input.subregion for
                    // input purposes as they are the same scale.
                    let blur_subregion = LayoutRect::new(
                        corner,
                        LayoutPoint::new(
                            corner.x + adjusted_blur_task_size.width as f32,
                            corner.y + adjusted_blur_task_size.height as f32,
                        ),
                    );
                    // For extremely large blur radius we have to limit size,
                    // see comments on node_task_size above for more details.
                    while adjusted_blur_task_size.to_i32().width as usize > MAX_SURFACE_SIZE ||
                        adjusted_blur_task_size.to_i32().height as usize > MAX_SURFACE_SIZE {
                        adjusted_blur_task_size.width >>= 1;
                        adjusted_blur_task_size.height >>= 1;
                        adjusted_blur_std_deviation.width *= 0.5;
                        adjusted_blur_std_deviation.height *= 0.5;
                        if adjusted_blur_task_size.width < 2 {
                            adjusted_blur_task_size.width = 2;
                        }
                        if adjusted_blur_task_size.height < 2 {
                            adjusted_blur_task_size.height = 2;
                        }
                    }

                    let input_subregion_task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
                        adjusted_blur_task_size,
                        RenderTaskKind::SVGFENode(
                            SVGFEFilterTask{
                                node: FilterGraphNode{
                                    kept_by_optimizer: true,
                                    linear: false,
                                    inflate: 0,
                                    inputs: [blur_input.clone()].to_vec(),
                                    subregion: blur_subregion,
                                },
                                op: FilterGraphOp::SVGFEIdentity,
                                content_origin: DevicePoint::zero(),
                                extra_gpu_cache_handle: None,
                            }
                        ),
                    ).with_uv_rect_kind(UvRectKind::Rect));
                    // Adding the dependencies sets the inputs for this task
                    frame_state.rg_builder.add_dependency(input_subregion_task_id, source_task_id);

                    // TODO: We should do this blur in the correct
                    // colorspace, linear=true is the default in SVG and
                    // new_blur does not currently support it.  If the nodes
                    // that consume the result only use the alpha channel, it
                    // does not matter, but when they use the RGB it matters.
                    let blur_task_id =
                        RenderTask::new_blur(
                            adjusted_blur_std_deviation,
                            input_subregion_task_id,
                            frame_state.rg_builder,
                            RenderTargetKind::Color,
                            None,
                            adjusted_blur_task_size,
                        );

                    task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
                        node_task_size,
                        RenderTaskKind::SVGFENode(
                            SVGFEFilterTask{
                                node: FilterGraphNode{
                                    kept_by_optimizer: true,
                                    linear: node.linear,
                                    inflate: node.inflate,
                                    inputs: [
                                        FilterGraphPictureReference{
                                            buffer_id: blur_input.buffer_id,
                                            subregion: blur_subregion,
                                            inflate: 0,
                                            offset: LayoutVector2D::zero(),
                                            source_padding: LayoutRect::zero(),
                                            target_padding: LayoutRect::zero(),
                                        }].to_vec(),
                                    subregion: node_subregion,
                                },
                                op: FilterGraphOp::SVGFEIdentity,
                                content_origin: DevicePoint::zero(),
                                extra_gpu_cache_handle: None,
                            }
                        ),
                    ).with_uv_rect_kind(node_uv_rect_kind));
                    // Adding the dependencies sets the inputs for this task
                    frame_state.rg_builder.add_dependency(task_id, blur_task_id);
                }
                FilterGraphOp::SVGFEDropShadow { color, dx, dy, std_deviation_x, std_deviation_y } => {
                    // Note: wrap_prim_with_filters copies the SourceGraphic to
                    // a node to apply the transparent border around the image,
                    // we rely on that behavior here as the Blur filter is a
                    // different shader without awareness of the subregion
                    // rules in the SVG spec.

                    // Find the input task id
                    assert!(node_inputs.len() == 1);
                    let blur_input = &node_inputs[0].0;
                    let source_task_id = node_inputs[0].1;

                    // We have to make a copy of the input that is padded with
                    // transparent black for the area outside the subregion, so
                    // that the blur task does not duplicate at the edges, and
                    // this is also where we have to adjust size to account for
                    // for downscaling of the image in the blur task to avoid
                    // introducing sampling artifacts on the downscale
                    let mut adjusted_blur_std_deviation = DeviceSize::new(
                        std_deviation_x,
                        std_deviation_y,
                    );
                    let blur_subregion = blur_input.subregion
                        .inflate(
                            std_deviation_x.ceil() * BLUR_SAMPLE_SCALE,
                            std_deviation_y.ceil() * BLUR_SAMPLE_SCALE);
                    let blur_task_size = blur_subregion.size().cast_unit();
                    // Adjust task size to prevent potential sampling errors
                    let mut adjusted_blur_task_size =
                        BlurTask::adjusted_blur_source_size(
                            blur_task_size,
                            adjusted_blur_std_deviation,
                        );
                    // Now change the subregion to match the revised task size,
                    // keeping it centered should keep animated radius smooth.
                    let corner = LayoutPoint::new(
                            blur_subregion.min.x + ((
                                blur_task_size.width as i32 -
                                adjusted_blur_task_size.width) / 2) as f32,
                            blur_subregion.min.y + ((
                                blur_task_size.height as i32 -
                                adjusted_blur_task_size.height) / 2) as f32,
                        )
                        .floor();
                    // Recalculate the blur_subregion to match, note that if the
                    // task was downsized it doesn't affect the size of this
                    // rect, so we don't have to scale blur_input.subregion for
                    // input purposes as they are the same scale.
                    let blur_subregion = LayoutRect::new(
                        corner,
                        LayoutPoint::new(
                            corner.x + adjusted_blur_task_size.width as f32,
                            corner.y + adjusted_blur_task_size.height as f32,
                        ),
                    );
                    // For extremely large blur radius we have to limit size,
                    // see comments on node_task_size above for more details.
                    while adjusted_blur_task_size.to_i32().width as usize > MAX_SURFACE_SIZE ||
                        adjusted_blur_task_size.to_i32().height as usize > MAX_SURFACE_SIZE {
                        adjusted_blur_task_size.width >>= 1;
                        adjusted_blur_task_size.height >>= 1;
                        adjusted_blur_std_deviation.width *= 0.5;
                        adjusted_blur_std_deviation.height *= 0.5;
                        if adjusted_blur_task_size.width < 2 {
                            adjusted_blur_task_size.width = 2;
                        }
                        if adjusted_blur_task_size.height < 2 {
                            adjusted_blur_task_size.height = 2;
                        }
                    }

                    let input_subregion_task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
                        adjusted_blur_task_size,
                        RenderTaskKind::SVGFENode(
                            SVGFEFilterTask{
                                node: FilterGraphNode{
                                    kept_by_optimizer: true,
                                    linear: false,
                                    inputs: [
                                        FilterGraphPictureReference{
                                            buffer_id: blur_input.buffer_id,
                                            subregion: blur_input.subregion,
                                            offset: LayoutVector2D::zero(),
                                            inflate: blur_input.inflate,
                                            source_padding: LayoutRect::zero(),
                                            target_padding: LayoutRect::zero(),
                                        }].to_vec(),
                                    subregion: blur_subregion,
                                    inflate: 0,
                                },
                                op: FilterGraphOp::SVGFEIdentity,
                                content_origin: DevicePoint::zero(),
                                extra_gpu_cache_handle: None,
                            }
                        ),
                    ).with_uv_rect_kind(UvRectKind::Rect));
                    // Adding the dependencies sets the inputs for this task
                    frame_state.rg_builder.add_dependency(input_subregion_task_id, source_task_id);

                    // The shadow compositing only cares about alpha channel
                    // which is always linear, so we can blur this in sRGB or
                    // linear color space and the result is the same as we will
                    // be replacing the rgb completely.
                    let blur_task_id =
                        RenderTask::new_blur(
                            adjusted_blur_std_deviation,
                            input_subregion_task_id,
                            frame_state.rg_builder,
                            RenderTargetKind::Color,
                            None,
                            adjusted_blur_task_size,
                        );

                    // Now we make the compositing task, for this we need to put
                    // the blurred shadow image at the correct subregion offset
                    let blur_subregion = blur_subregion
                        .translate(LayoutVector2D::new(dx, dy));
                    task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
                        node_task_size,
                        RenderTaskKind::SVGFENode(
                            SVGFEFilterTask{
                                node: FilterGraphNode{
                                    kept_by_optimizer: true,
                                    linear: node.linear,
                                    inflate: node.inflate,
                                    inputs: [
                                        // Original picture
                                        *blur_input,
                                        // Shadow picture
                                        FilterGraphPictureReference{
                                            buffer_id: blur_input.buffer_id,
                                            subregion: blur_subregion,
                                            inflate: 0,
                                            offset: LayoutVector2D::zero(),
                                            source_padding: LayoutRect::zero(),
                                            target_padding: LayoutRect::zero(),
                                        }].to_vec(),
                                    subregion: node_subregion,
                                },
                                op: FilterGraphOp::SVGFEDropShadow{
                                    color,
                                    // These parameters don't matter here
                                    dx: 0.0, dy: 0.0,
                                    std_deviation_x: 0.0, std_deviation_y: 0.0,
                                },
                                content_origin: DevicePoint::zero(),
                                extra_gpu_cache_handle: None,
                            }
                        ),
                    ).with_uv_rect_kind(node_uv_rect_kind));
                    // Adding the dependencies sets the inputs for this task
                    frame_state.rg_builder.add_dependency(task_id, source_task_id);
                    frame_state.rg_builder.add_dependency(task_id, blur_task_id);
                }
                FilterGraphOp::SVGFESourceAlpha |
                FilterGraphOp::SVGFESourceGraphic => {
                    // These copy from the original task, we have to synthesize
                    // a fake input binding to make the shader do the copy.  In
                    // the case of SourceAlpha the shader will zero the RGB but
                    // we don't have to care about that distinction here.
                    task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
                        node_task_size,
                        RenderTaskKind::SVGFENode(
                            SVGFEFilterTask{
                                node: FilterGraphNode{
                                    kept_by_optimizer: true,
                                    linear: node.linear,
                                    inflate: node.inflate,
                                    inputs: [
                                        FilterGraphPictureReference{
                                            buffer_id: FilterOpGraphPictureBufferId::None,
                                            // This is what makes the mapping
                                            // actually work - this has to be
                                            // the subregion of the whole filter
                                            // because that is the size of the
                                            // input task, it will be cropped to
                                            // the used area (source_subregion).
                                            subregion: filter_subregion,
                                            offset: LayoutVector2D::zero(),
                                            inflate: 0,
                                            source_padding: LayoutRect::zero(),
                                            target_padding: LayoutRect::zero(),
                                        }
                                    ].to_vec(),
                                    subregion: node_subregion,
                                },
                                op: op.clone(),
                                content_origin: DevicePoint::zero(),
                                extra_gpu_cache_handle: None,
                            }
                        ),
                    ).with_uv_rect_kind(node_uv_rect_kind));
                    frame_state.rg_builder.add_dependency(task_id, original_task_id);
                    made_dependency_on_source = true;
                }
                FilterGraphOp::SVGFEComponentTransferInterned { handle, creates_pixels: _ } => {
                    // FIXME: Doing this in prepare_interned_prim_for_render
                    // doesn't seem to be enough, where should it be done?
                    let filter_data = &mut data_stores.filter_data[handle];
                    filter_data.update(frame_state);
                    // ComponentTransfer has a gpu_cache_handle that we need to
                    // pass along
                    task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
                        node_task_size,
                        RenderTaskKind::SVGFENode(
                            SVGFEFilterTask{
                                node: FilterGraphNode{
                                    kept_by_optimizer: true,
                                    linear: node.linear,
                                    inputs: node_inputs.iter().map(|input| {input.0}).collect(),
                                    subregion: node_subregion,
                                    inflate: node.inflate,
                                },
                                op: op.clone(),
                                content_origin: DevicePoint::zero(),
                                extra_gpu_cache_handle: Some(filter_data.gpu_cache_handle),
                            }
                        ),
                    ).with_uv_rect_kind(node_uv_rect_kind));

                    // Add the dependencies for inputs of this node, which will
                    // be used by add_svg_filter_node_instances later
                    for (_input, input_task) in &node_inputs {
                        if *input_task == original_task_id {
                            made_dependency_on_source = true;
                        }
                        if *input_task != RenderTaskId::INVALID {
                            frame_state.rg_builder.add_dependency(task_id, *input_task);
                        }
                    }
                }
                _ => {
                    // This is the usual case - zero, one or two inputs that
                    // reference earlier node results.
                    task_id = frame_state.rg_builder.add().init(RenderTask::new_dynamic(
                        node_task_size,
                        RenderTaskKind::SVGFENode(
                            SVGFEFilterTask{
                                node: FilterGraphNode{
                                    kept_by_optimizer: true,
                                    linear: node.linear,
                                    inputs: node_inputs.iter().map(|input| {input.0}).collect(),
                                    subregion: node_subregion,
                                    inflate: node.inflate,
                                },
                                op: op.clone(),
                                content_origin: DevicePoint::zero(),
                                extra_gpu_cache_handle: None,
                            }
                        ),
                    ).with_uv_rect_kind(node_uv_rect_kind));

                    // Add the dependencies for inputs of this node, which will
                    // be used by add_svg_filter_node_instances later
                    for (_input, input_task) in &node_inputs {
                        if *input_task == original_task_id {
                            made_dependency_on_source = true;
                        }
                        if *input_task != RenderTaskId::INVALID {
                            frame_state.rg_builder.add_dependency(task_id, *input_task);
                        }
                    }
                }
            }

            // We track the tasks we created by output buffer id to make it easy
            // to look them up quickly, since nodes can only depend on previous
            // nodes in the same list
            task_by_buffer_id[filter_index] = task_id;
            subregion_by_buffer_id[filter_index] = node_subregion;

            if is_output {
                output_task_id = task_id;
            }
        }

        // If no tasks referenced the SourceGraphic, we actually have to create
        // a fake dependency so that it does not leak.
        if !made_dependency_on_source && output_task_id != original_task_id {
            frame_state.rg_builder.add_dependency(output_task_id, original_task_id);
        }

        output_task_id
   }

    pub fn uv_rect_kind(&self) -> UvRectKind {
        self.uv_rect_kind
    }

    pub fn get_texture_address(&self, gpu_cache: &GpuCache) -> GpuCacheAddress {
        gpu_cache.get_address(&self.uv_rect_handle)
    }

    pub fn get_target_texture(&self) -> CacheTextureId {
        match self.location {
            RenderTaskLocation::Dynamic { texture_id, .. } => {
                assert_ne!(texture_id, CacheTextureId::INVALID);
                texture_id
            }
            RenderTaskLocation::Existing { .. } |
            RenderTaskLocation::CacheRequest { .. } |
            RenderTaskLocation::Unallocated { .. } |
            RenderTaskLocation::Static { .. } => {
                unreachable!();
            }
        }
    }

    pub fn get_texture_source(&self) -> TextureSource {
        match self.location {
            RenderTaskLocation::Dynamic { texture_id, .. } => {
                assert_ne!(texture_id, CacheTextureId::INVALID);
                TextureSource::TextureCache(texture_id, Swizzle::default())
            }
            RenderTaskLocation::Static { surface:  StaticRenderTaskSurface::ReadOnly { source }, .. } => {
                source
            }
            RenderTaskLocation::Static { surface: StaticRenderTaskSurface::TextureCache { texture, .. }, .. } => {
                TextureSource::TextureCache(texture, Swizzle::default())
            }
            RenderTaskLocation::Existing { .. } |
            RenderTaskLocation::Static { .. } |
            RenderTaskLocation::CacheRequest { .. } |
            RenderTaskLocation::Unallocated { .. } => {
                unreachable!();
            }
        }
    }

    pub fn get_target_rect(&self) -> DeviceIntRect {
        match self.location {
            // Previously, we only added render tasks after the entire
            // primitive chain was determined visible. This meant that
            // we could assert any render task in the list was also
            // allocated (assigned to passes). Now, we add render
            // tasks earlier, and the picture they belong to may be
            // culled out later, so we can't assert that the task
            // has been allocated.
            // Render tasks that are created but not assigned to
            // passes consume a row in the render task texture, but
            // don't allocate any space in render targets nor
            // draw any pixels.
            // TODO(gw): Consider some kind of tag or other method
            //           to mark a task as unused explicitly. This
            //           would allow us to restore this debug check.
            RenderTaskLocation::Dynamic { rect, .. } => rect,
            RenderTaskLocation::Static { rect, .. } => rect,
            RenderTaskLocation::Existing { .. } |
            RenderTaskLocation::CacheRequest { .. } |
            RenderTaskLocation::Unallocated { .. } => {
                panic!("bug: get_target_rect called before allocating");
            }
        }
    }

    pub fn get_target_size(&self) -> DeviceIntSize {
        match self.location {
            RenderTaskLocation::Dynamic { rect, .. } => rect.size(),
            RenderTaskLocation::Static { rect, .. } => rect.size(),
            RenderTaskLocation::Existing { size, .. } => size,
            RenderTaskLocation::CacheRequest { size } => size,
            RenderTaskLocation::Unallocated { size } => size,
        }
    }

    pub fn target_kind(&self) -> RenderTargetKind {
        self.kind.target_kind()
    }

    pub fn write_gpu_blocks(
        &mut self,
        target_rect: DeviceIntRect,
        gpu_cache: &mut GpuCache,
    ) {
        profile_scope!("write_gpu_blocks");

        self.kind.write_gpu_blocks(gpu_cache);

        if self.cache_handle.is_some() {
            // The uv rect handle of cached render tasks is requested and set by the
            // render task cache.
            return;
        }

        if let Some(mut request) = gpu_cache.request(&mut self.uv_rect_handle) {
            let p0 = target_rect.min.to_f32();
            let p1 = target_rect.max.to_f32();
            let image_source = ImageSource {
                p0,
                p1,
                user_data: [0.0; 4],
                uv_rect_kind: self.uv_rect_kind,
            };
            image_source.write_gpu_blocks(&mut request);
        }
    }

    /// Called by the render task cache.
    ///
    /// Tells the render task that it is cached (which means its gpu cache
    /// handle is managed by the texture cache).
    pub fn mark_cached(&mut self, handle: RenderTaskCacheEntryHandle) {
        self.cache_handle = Some(handle);
    }
}

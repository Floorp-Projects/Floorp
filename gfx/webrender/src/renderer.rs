/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The webrender API.
//!
//! The `webrender::renderer` module provides the interface to webrender, which
//! is accessible through [`Renderer`][renderer]
//!
//! [renderer]: struct.Renderer.html

use api::{BlobImageRenderer, ColorF, ColorU, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize, DocumentId, Epoch, ExternalImageId};
use api::{ExternalImageType, FontRenderMode, ImageFormat, PipelineId, RenderApiSender};
use api::{RenderNotifier, YUV_COLOR_SPACES, YUV_FORMATS, YuvColorSpace, YuvFormat, channel};
#[cfg(not(feature = "debugger"))]
use api::ApiMsg;
use api::DebugCommand;
#[cfg(not(feature = "debugger"))]
use api::channel::MsgSender;
use batch::{BatchKey, BatchKind, BatchTextures, BrushBatchKind};
use batch::{BrushImageSourceKind, TransformBatchKind};
#[cfg(feature = "capture")]
use capture::{CaptureConfig, ExternalCaptureImage};
use debug_colors;
use debug_render::DebugRenderer;
#[cfg(feature = "debugger")]
use debug_server::{self, DebugServer};
use device::{DepthFunction, Device, FrameId, Program, UploadMethod, Texture,
             VertexDescriptor, PBO};
use device::{ExternalTexture, FBOId, TextureSlot, VertexAttribute, VertexAttributeKind};
use device::{FileWatcherHandler, ShaderError, TextureFilter, TextureTarget,
             VertexUsageHint, VAO, VBO, CustomVAO};
use device::{ProgramCache, ReadPixelsFormat};
use euclid::{rect, Transform3D};
use frame_builder::FrameBuilderConfig;
use gleam::gl;
use glyph_rasterizer::GlyphFormat;
use gpu_cache::{GpuBlockData, GpuCacheUpdate, GpuCacheUpdateList};
use gpu_types::PrimitiveInstance;
use internal_types::{SourceTexture, ORTHO_FAR_PLANE, ORTHO_NEAR_PLANE};
use internal_types::{CacheTextureId, FastHashMap, RenderedDocument, ResultMsg, TextureUpdateOp};
use internal_types::{DebugOutput, RenderPassIndex, RenderTargetInfo, TextureUpdateList, TextureUpdateSource};
use picture::ContentOrigin;
use prim_store::DeferredResolve;
use profiler::{BackendProfileCounters, Profiler};
use profiler::{GpuProfileTag, RendererProfileCounters, RendererProfileTimers};
use query::{GpuProfiler, GpuTimer};
use rayon::Configuration as ThreadPoolConfig;
use rayon::ThreadPool;
use record::ApiRecordingReceiver;
use render_backend::RenderBackend;
use render_task::{RenderTaskKind, RenderTaskTree};
#[cfg(feature = "debugger")]
use serde_json;
use std;
use std::cmp;
use std::collections::VecDeque;
use std::collections::hash_map::Entry;
use std::f32;
use std::mem;
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::Arc;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread;
use texture_cache::TextureCache;
use thread_profiler::{register_thread_with_profiler, write_profile};
use tiling::{AlphaRenderTarget, ColorRenderTarget};
use tiling::{RenderPass, RenderPassKind, RenderTargetList};
use tiling::{Frame, RenderTarget, ScalingInfo, TextureCacheRenderTarget};
use time::precise_time_ns;
use util::TransformedRectKind;

pub const MAX_VERTEX_TEXTURE_WIDTH: usize = 1024;
/// Enabling this toggle would force the GPU cache scattered texture to
/// be resized every frame, which enables GPU debuggers to see if this
/// is performed correctly.
const GPU_CACHE_RESIZE_TEST: bool = false;

/// Number of GPU blocks per UV rectangle provided for an image.
pub const BLOCKS_PER_UV_RECT: usize = 2;

const GPU_TAG_BRUSH_SOLID: GpuProfileTag = GpuProfileTag {
    label: "B_Solid",
    color: debug_colors::RED,
};
const GPU_TAG_BRUSH_MASK: GpuProfileTag = GpuProfileTag {
    label: "B_Mask",
    color: debug_colors::BLACK,
};
const GPU_TAG_BRUSH_PICTURE: GpuProfileTag = GpuProfileTag {
    label: "B_Picture",
    color: debug_colors::SILVER,
};
const GPU_TAG_BRUSH_LINE: GpuProfileTag = GpuProfileTag {
    label: "Line",
    color: debug_colors::DARKRED,
};
const GPU_TAG_CACHE_CLIP: GpuProfileTag = GpuProfileTag {
    label: "C_Clip",
    color: debug_colors::PURPLE,
};
const GPU_TAG_CACHE_TEXT_RUN: GpuProfileTag = GpuProfileTag {
    label: "C_TextRun",
    color: debug_colors::MISTYROSE,
};
const GPU_TAG_SETUP_TARGET: GpuProfileTag = GpuProfileTag {
    label: "target init",
    color: debug_colors::SLATEGREY,
};
const GPU_TAG_SETUP_DATA: GpuProfileTag = GpuProfileTag {
    label: "data init",
    color: debug_colors::LIGHTGREY,
};
const GPU_TAG_PRIM_IMAGE: GpuProfileTag = GpuProfileTag {
    label: "Image",
    color: debug_colors::GREEN,
};
const GPU_TAG_PRIM_YUV_IMAGE: GpuProfileTag = GpuProfileTag {
    label: "YuvImage",
    color: debug_colors::DARKGREEN,
};
const GPU_TAG_PRIM_BLEND: GpuProfileTag = GpuProfileTag {
    label: "Blend",
    color: debug_colors::LIGHTBLUE,
};
const GPU_TAG_PRIM_HW_COMPOSITE: GpuProfileTag = GpuProfileTag {
    label: "HwComposite",
    color: debug_colors::DODGERBLUE,
};
const GPU_TAG_PRIM_SPLIT_COMPOSITE: GpuProfileTag = GpuProfileTag {
    label: "SplitComposite",
    color: debug_colors::DARKBLUE,
};
const GPU_TAG_PRIM_COMPOSITE: GpuProfileTag = GpuProfileTag {
    label: "Composite",
    color: debug_colors::MAGENTA,
};
const GPU_TAG_PRIM_TEXT_RUN: GpuProfileTag = GpuProfileTag {
    label: "TextRun",
    color: debug_colors::BLUE,
};
const GPU_TAG_PRIM_GRADIENT: GpuProfileTag = GpuProfileTag {
    label: "Gradient",
    color: debug_colors::YELLOW,
};
const GPU_TAG_PRIM_ANGLE_GRADIENT: GpuProfileTag = GpuProfileTag {
    label: "AngleGradient",
    color: debug_colors::POWDERBLUE,
};
const GPU_TAG_PRIM_RADIAL_GRADIENT: GpuProfileTag = GpuProfileTag {
    label: "RadialGradient",
    color: debug_colors::LIGHTPINK,
};
const GPU_TAG_PRIM_BORDER_CORNER: GpuProfileTag = GpuProfileTag {
    label: "BorderCorner",
    color: debug_colors::DARKSLATEGREY,
};
const GPU_TAG_PRIM_BORDER_EDGE: GpuProfileTag = GpuProfileTag {
    label: "BorderEdge",
    color: debug_colors::LAVENDER,
};
const GPU_TAG_BLUR: GpuProfileTag = GpuProfileTag {
    label: "Blur",
    color: debug_colors::VIOLET,
};

const GPU_SAMPLER_TAG_ALPHA: GpuProfileTag = GpuProfileTag {
    label: "Alpha Targets",
    color: debug_colors::BLACK,
};
const GPU_SAMPLER_TAG_OPAQUE: GpuProfileTag = GpuProfileTag {
    label: "Opaque Pass",
    color: debug_colors::BLACK,
};
const GPU_SAMPLER_TAG_TRANSPARENT: GpuProfileTag = GpuProfileTag {
    label: "Transparent Pass",
    color: debug_colors::BLACK,
};

impl TransformBatchKind {
    #[cfg(feature = "debugger")]
    fn debug_name(&self) -> &'static str {
        match *self {
            TransformBatchKind::TextRun(..) => "TextRun",
            TransformBatchKind::Image(image_buffer_kind, ..) => match image_buffer_kind {
                ImageBufferKind::Texture2D => "Image (2D)",
                ImageBufferKind::TextureRect => "Image (Rect)",
                ImageBufferKind::TextureExternal => "Image (External)",
                ImageBufferKind::Texture2DArray => "Image (Array)",
            },
            TransformBatchKind::YuvImage(..) => "YuvImage",
            TransformBatchKind::AlignedGradient => "AlignedGradient",
            TransformBatchKind::AngleGradient => "AngleGradient",
            TransformBatchKind::RadialGradient => "RadialGradient",
            TransformBatchKind::BorderCorner => "BorderCorner",
            TransformBatchKind::BorderEdge => "BorderEdge",
        }
    }

    fn gpu_sampler_tag(&self) -> GpuProfileTag {
        match *self {
            TransformBatchKind::TextRun(..) => GPU_TAG_PRIM_TEXT_RUN,
            TransformBatchKind::Image(..) => GPU_TAG_PRIM_IMAGE,
            TransformBatchKind::YuvImage(..) => GPU_TAG_PRIM_YUV_IMAGE,
            TransformBatchKind::BorderCorner => GPU_TAG_PRIM_BORDER_CORNER,
            TransformBatchKind::BorderEdge => GPU_TAG_PRIM_BORDER_EDGE,
            TransformBatchKind::AlignedGradient => GPU_TAG_PRIM_GRADIENT,
            TransformBatchKind::AngleGradient => GPU_TAG_PRIM_ANGLE_GRADIENT,
            TransformBatchKind::RadialGradient => GPU_TAG_PRIM_RADIAL_GRADIENT,
        }
    }
}

impl BatchKind {
    #[cfg(feature = "debugger")]
    fn debug_name(&self) -> &'static str {
        match *self {
            BatchKind::Composite { .. } => "Composite",
            BatchKind::HardwareComposite => "HardwareComposite",
            BatchKind::SplitComposite => "SplitComposite",
            BatchKind::Blend => "Blend",
            BatchKind::Brush(kind) => {
                match kind {
                    BrushBatchKind::Picture(..) => "Brush (Picture)",
                    BrushBatchKind::Solid => "Brush (Solid)",
                    BrushBatchKind::Line => "Brush (Line)",
                }
            }
            BatchKind::Transformable(_, batch_kind) => batch_kind.debug_name(),
        }
    }

    fn gpu_sampler_tag(&self) -> GpuProfileTag {
        match *self {
            BatchKind::Composite { .. } => GPU_TAG_PRIM_COMPOSITE,
            BatchKind::HardwareComposite => GPU_TAG_PRIM_HW_COMPOSITE,
            BatchKind::SplitComposite => GPU_TAG_PRIM_SPLIT_COMPOSITE,
            BatchKind::Blend => GPU_TAG_PRIM_BLEND,
            BatchKind::Brush(kind) => {
                match kind {
                    BrushBatchKind::Picture(..) => GPU_TAG_BRUSH_PICTURE,
                    BrushBatchKind::Solid => GPU_TAG_BRUSH_SOLID,
                    BrushBatchKind::Line => GPU_TAG_BRUSH_LINE,
                }
            }
            BatchKind::Transformable(_, batch_kind) => batch_kind.gpu_sampler_tag(),
        }
    }
}

bitflags! {
    #[derive(Default)]
    pub struct DebugFlags: u32 {
        const PROFILER_DBG      = 1 << 0;
        const RENDER_TARGET_DBG = 1 << 1;
        const TEXTURE_CACHE_DBG = 1 << 2;
        const ALPHA_PRIM_DBG    = 1 << 3;
        const GPU_TIME_QUERIES  = 1 << 4;
        const GPU_SAMPLE_QUERIES= 1 << 5;
        const DISABLE_BATCHING  = 1 << 6;
        const EPOCHS            = 1 << 7;
        const COMPACT_PROFILER  = 1 << 8;
    }
}

fn flag_changed(before: DebugFlags, after: DebugFlags, select: DebugFlags) -> Option<bool> {
    if before & select != after & select {
        Some(after.contains(select))
    } else {
        None
    }
}

// A generic mode that can be passed to shaders to change
// behaviour per draw-call.
type ShaderMode = i32;

#[repr(C)]
enum TextShaderMode {
    Alpha = 0,
    SubpixelConstantTextColor = 1,
    SubpixelPass0 = 2,
    SubpixelPass1 = 3,
    SubpixelWithBgColorPass0 = 4,
    SubpixelWithBgColorPass1 = 5,
    SubpixelWithBgColorPass2 = 6,
    SubpixelDualSource = 7,
    Bitmap = 8,
    ColorBitmap = 9,
}

impl Into<ShaderMode> for TextShaderMode {
    fn into(self) -> i32 {
        self as i32
    }
}

impl From<GlyphFormat> for TextShaderMode {
    fn from(format: GlyphFormat) -> TextShaderMode {
        match format {
            GlyphFormat::Alpha | GlyphFormat::TransformedAlpha => TextShaderMode::Alpha,
            GlyphFormat::Subpixel | GlyphFormat::TransformedSubpixel => {
                panic!("Subpixel glyph formats must be handled separately.");
            }
            GlyphFormat::Bitmap => TextShaderMode::Bitmap,
            GlyphFormat::ColorBitmap => TextShaderMode::ColorBitmap,
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
enum TextureSampler {
    Color0,
    Color1,
    Color2,
    CacheA8,
    CacheRGBA8,
    ResourceCache,
    ClipScrollNodes,
    RenderTasks,
    Dither,
    // A special sampler that is bound to the A8 output of
    // the *first* pass. Items rendered in this target are
    // available as inputs to tasks in any subsequent pass.
    SharedCacheA8,
    LocalClipRects
}

impl TextureSampler {
    fn color(n: usize) -> TextureSampler {
        match n {
            0 => TextureSampler::Color0,
            1 => TextureSampler::Color1,
            2 => TextureSampler::Color2,
            _ => {
                panic!("There are only 3 color samplers.");
            }
        }
    }
}

impl Into<TextureSlot> for TextureSampler {
    fn into(self) -> TextureSlot {
        match self {
            TextureSampler::Color0 => TextureSlot(0),
            TextureSampler::Color1 => TextureSlot(1),
            TextureSampler::Color2 => TextureSlot(2),
            TextureSampler::CacheA8 => TextureSlot(3),
            TextureSampler::CacheRGBA8 => TextureSlot(4),
            TextureSampler::ResourceCache => TextureSlot(5),
            TextureSampler::ClipScrollNodes => TextureSlot(6),
            TextureSampler::RenderTasks => TextureSlot(7),
            TextureSampler::Dither => TextureSlot(8),
            TextureSampler::SharedCacheA8 => TextureSlot(9),
            TextureSampler::LocalClipRects => TextureSlot(10),
        }
    }
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct PackedVertex {
    pub pos: [f32; 2],
}

const DESC_PRIM_INSTANCES: VertexDescriptor = VertexDescriptor {
    vertex_attributes: &[
        VertexAttribute {
            name: "aPosition",
            count: 2,
            kind: VertexAttributeKind::F32,
        },
    ],
    instance_attributes: &[
        VertexAttribute {
            name: "aData0",
            count: 4,
            kind: VertexAttributeKind::I32,
        },
        VertexAttribute {
            name: "aData1",
            count: 4,
            kind: VertexAttributeKind::I32,
        },
    ],
};

const DESC_BLUR: VertexDescriptor = VertexDescriptor {
    vertex_attributes: &[
        VertexAttribute {
            name: "aPosition",
            count: 2,
            kind: VertexAttributeKind::F32,
        },
    ],
    instance_attributes: &[
        VertexAttribute {
            name: "aBlurRenderTaskAddress",
            count: 1,
            kind: VertexAttributeKind::I32,
        },
        VertexAttribute {
            name: "aBlurSourceTaskAddress",
            count: 1,
            kind: VertexAttributeKind::I32,
        },
        VertexAttribute {
            name: "aBlurDirection",
            count: 1,
            kind: VertexAttributeKind::I32,
        },
    ],
};

const DESC_CLIP: VertexDescriptor = VertexDescriptor {
    vertex_attributes: &[
        VertexAttribute {
            name: "aPosition",
            count: 2,
            kind: VertexAttributeKind::F32,
        },
    ],
    instance_attributes: &[
        VertexAttribute {
            name: "aClipRenderTaskAddress",
            count: 1,
            kind: VertexAttributeKind::I32,
        },
        VertexAttribute {
            name: "aScrollNodeId",
            count: 1,
            kind: VertexAttributeKind::I32,
        },
        VertexAttribute {
            name: "aClipSegment",
            count: 1,
            kind: VertexAttributeKind::I32,
        },
        VertexAttribute {
            name: "aClipDataResourceAddress",
            count: 4,
            kind: VertexAttributeKind::U16,
        },
    ],
};

const DESC_GPU_CACHE_UPDATE: VertexDescriptor = VertexDescriptor {
    vertex_attributes: &[
        VertexAttribute {
            name: "aPosition",
            count: 2,
            kind: VertexAttributeKind::U16Norm,
        },
        VertexAttribute {
            name: "aValue",
            count: 4,
            kind: VertexAttributeKind::F32,
        },
    ],
    instance_attributes: &[],
};

#[derive(Debug, Copy, Clone)]
enum VertexArrayKind {
    Primitive,
    Blur,
    Clip,
}

#[derive(Clone, Debug, PartialEq)]
pub enum GraphicsApi {
    OpenGL,
}

#[derive(Clone, Debug)]
pub struct GraphicsApiInfo {
    pub kind: GraphicsApi,
    pub renderer: String,
    pub version: String,
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
pub enum ImageBufferKind {
    Texture2D = 0,
    TextureRect = 1,
    TextureExternal = 2,
    Texture2DArray = 3,
}

pub const IMAGE_BUFFER_KINDS: [ImageBufferKind; 4] = [
    ImageBufferKind::Texture2D,
    ImageBufferKind::TextureRect,
    ImageBufferKind::TextureExternal,
    ImageBufferKind::Texture2DArray,
];

impl ImageBufferKind {
    pub fn get_feature_string(&self) -> &'static str {
        match *self {
            ImageBufferKind::Texture2D => "TEXTURE_2D",
            ImageBufferKind::Texture2DArray => "",
            ImageBufferKind::TextureRect => "TEXTURE_RECT",
            ImageBufferKind::TextureExternal => "TEXTURE_EXTERNAL",
        }
    }

    pub fn has_platform_support(&self, gl_type: &gl::GlType) -> bool {
        match *gl_type {
            gl::GlType::Gles => match *self {
                ImageBufferKind::Texture2D => true,
                ImageBufferKind::Texture2DArray => true,
                ImageBufferKind::TextureRect => true,
                ImageBufferKind::TextureExternal => true,
            },
            gl::GlType::Gl => match *self {
                ImageBufferKind::Texture2D => true,
                ImageBufferKind::Texture2DArray => true,
                ImageBufferKind::TextureRect => true,
                ImageBufferKind::TextureExternal => false,
            },
        }
    }
}

#[derive(Debug, Copy, Clone)]
pub enum RendererKind {
    Native,
    OSMesa,
}

#[derive(Debug)]
pub struct GpuProfile {
    pub frame_id: FrameId,
    pub paint_time_ns: u64,
}

impl GpuProfile {
    fn new<T>(frame_id: FrameId, timers: &[GpuTimer<T>]) -> GpuProfile {
        let mut paint_time_ns = 0;
        for timer in timers {
            paint_time_ns += timer.time_ns;
        }
        GpuProfile {
            frame_id,
            paint_time_ns,
        }
    }
}

#[derive(Debug)]
pub struct CpuProfile {
    pub frame_id: FrameId,
    pub backend_time_ns: u64,
    pub composite_time_ns: u64,
    pub draw_calls: usize,
}

impl CpuProfile {
    fn new(
        frame_id: FrameId,
        backend_time_ns: u64,
        composite_time_ns: u64,
        draw_calls: usize,
    ) -> CpuProfile {
        CpuProfile {
            frame_id,
            backend_time_ns,
            composite_time_ns,
            draw_calls,
        }
    }
}

struct RenderTargetPoolId(usize);

struct SourceTextureResolver {
    /// A vector for fast resolves of texture cache IDs to
    /// native texture IDs. This maps to a free-list managed
    /// by the backend thread / texture cache. We free the
    /// texture memory associated with a TextureId when its
    /// texture cache ID is freed by the texture cache, but
    /// reuse the TextureId when the texture caches's free
    /// list reuses the texture cache ID. This saves having to
    /// use a hashmap, and allows a flat vector for performance.
    cache_texture_map: Vec<Texture>,

    /// Map of external image IDs to native textures.
    external_images: FastHashMap<(ExternalImageId, u8), ExternalTexture>,

    /// A special 1x1 dummy cache texture used for shaders that expect to work
    /// with the cache but are actually running in the first pass
    /// when no target is yet provided as a cache texture input.
    dummy_cache_texture: Texture,

    /// The current cache textures.
    cache_rgba8_texture: Option<Texture>,
    cache_a8_texture: Option<Texture>,

    pass_rgba8_textures: FastHashMap<RenderPassIndex, RenderTargetPoolId>,
    pass_a8_textures: FastHashMap<RenderPassIndex, RenderTargetPoolId>,

    render_target_pool: Vec<Texture>,
}

impl SourceTextureResolver {
    fn new(device: &mut Device) -> SourceTextureResolver {
        let mut dummy_cache_texture = device
            .create_texture(TextureTarget::Array, ImageFormat::BGRA8);
        device.init_texture(
            &mut dummy_cache_texture,
            1,
            1,
            TextureFilter::Linear,
            None,
            1,
            None,
        );

        SourceTextureResolver {
            cache_texture_map: Vec::new(),
            external_images: FastHashMap::default(),
            dummy_cache_texture,
            cache_a8_texture: None,
            cache_rgba8_texture: None,
            pass_rgba8_textures: FastHashMap::default(),
            pass_a8_textures: FastHashMap::default(),
            render_target_pool: Vec::new(),
        }
    }

    fn deinit(self, device: &mut Device) {
        device.delete_texture(self.dummy_cache_texture);

        for texture in self.cache_texture_map {
            device.delete_texture(texture);
        }

        for texture in self.render_target_pool {
            device.delete_texture(texture);
        }
    }

    fn begin_frame(&mut self) {
        assert!(self.cache_rgba8_texture.is_none());
        assert!(self.cache_a8_texture.is_none());

        self.pass_rgba8_textures.clear();
        self.pass_a8_textures.clear();
    }

    fn end_frame(&mut self, pass_index: RenderPassIndex) {
        // return the cached targets to the pool
        self.end_pass(None, None, pass_index)
    }

    fn end_pass(
        &mut self,
        a8_texture: Option<Texture>,
        rgba8_texture: Option<Texture>,
        pass_index: RenderPassIndex,
    ) {
        // If we have cache textures from previous pass, return them to the pool.
        // Also assign the pool index of those cache textures to last pass's index because this is
        // the result of last pass.
        if let Some(texture) = self.cache_rgba8_texture.take() {
            self.pass_rgba8_textures.insert(
                RenderPassIndex(pass_index.0 - 1), RenderTargetPoolId(self.render_target_pool.len()));
            self.render_target_pool.push(texture);
        }
        if let Some(texture) = self.cache_a8_texture.take() {
            self.pass_a8_textures.insert(
                RenderPassIndex(pass_index.0 - 1), RenderTargetPoolId(self.render_target_pool.len()));
            self.render_target_pool.push(texture);
        }

        // We have another pass to process, make these textures available
        // as inputs to the next pass.
        self.cache_rgba8_texture = rgba8_texture;
        self.cache_a8_texture = a8_texture;
    }

    // Bind a source texture to the device.
    fn bind(&self, texture_id: &SourceTexture, sampler: TextureSampler, device: &mut Device) {
        match *texture_id {
            SourceTexture::Invalid => {}
            SourceTexture::CacheA8 => {
                let texture = self.cache_a8_texture
                    .as_ref()
                    .unwrap_or(&self.dummy_cache_texture);
                device.bind_texture(sampler, texture);
            }
            SourceTexture::CacheRGBA8 => {
                let texture = self.cache_rgba8_texture
                    .as_ref()
                    .unwrap_or(&self.dummy_cache_texture);
                device.bind_texture(sampler, texture);
            }
            SourceTexture::External(external_image) => {
                let texture = self.external_images
                    .get(&(external_image.id, external_image.channel_index))
                    .expect(&format!("BUG: External image should be resolved by now: {:?}", external_image));
                device.bind_external_texture(sampler, texture);
            }
            SourceTexture::TextureCache(index) => {
                let texture = &self.cache_texture_map[index.0];
                device.bind_texture(sampler, texture);
            }
            SourceTexture::RenderTaskCacheRGBA8(pass_index) => {
                let pool_index = self.pass_rgba8_textures
                    .get(&pass_index)
                    .expect("BUG: pass_index doesn't map to pool_index");
                device.bind_texture(sampler, &self.render_target_pool[pool_index.0])
            }
            SourceTexture::RenderTaskCacheA8(pass_index) => {
                let pool_index = self.pass_a8_textures
                    .get(&pass_index)
                    .expect("BUG: pass_index doesn't map to pool_index");
                device.bind_texture(sampler, &self.render_target_pool[pool_index.0])
            }
        }
    }

    // Get the real (OpenGL) texture ID for a given source texture.
    // For a texture cache texture, the IDs are stored in a vector
    // map for fast access.
    fn resolve(&self, texture_id: &SourceTexture) -> Option<&Texture> {
        match *texture_id {
            SourceTexture::Invalid => None,
            SourceTexture::CacheA8 => Some(
                self.cache_a8_texture
                    .as_ref()
                    .unwrap_or(&self.dummy_cache_texture),
            ),
            SourceTexture::CacheRGBA8 => Some(
                self.cache_rgba8_texture
                    .as_ref()
                    .unwrap_or(&self.dummy_cache_texture),
            ),
            SourceTexture::External(..) => {
                panic!("BUG: External textures cannot be resolved, they can only be bound.");
            }
            SourceTexture::TextureCache(index) => Some(&self.cache_texture_map[index.0]),
            SourceTexture::RenderTaskCacheRGBA8(pass_index) => {
                let pool_index = self.pass_rgba8_textures
                    .get(&pass_index)
                    .expect("BUG: pass_index doesn't map to pool_index");
                Some(&self.render_target_pool[pool_index.0])
            },
            SourceTexture::RenderTaskCacheA8(pass_index) => {
                let pool_index = self.pass_a8_textures
                    .get(&pass_index)
                    .expect("BUG: pass_index doesn't map to pool_index");
                Some(&self.render_target_pool[pool_index.0])
            },
        }
    }
}

#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Deserialize, Serialize))]
#[allow(dead_code)] // SubpixelVariableTextColor is not used at the moment.
pub enum BlendMode {
    None,
    Alpha,
    PremultipliedAlpha,
    PremultipliedDestOut,
    SubpixelDualSource,
    SubpixelConstantTextColor(ColorF),
    SubpixelWithBgColor,
    SubpixelVariableTextColor,
}

// Tracks the state of each row in the GPU cache texture.
struct CacheRow {
    is_dirty: bool,
}

impl CacheRow {
    fn new() -> Self {
        CacheRow { is_dirty: false }
    }
}

/// The bus over which CPU and GPU versions of the cache
/// get synchronized.
enum CacheBus {
    /// PBO-based updates, currently operate on a row granularity.
    /// Therefore, are subject to fragmentation issues.
    PixelBuffer {
        /// PBO used for transfers.
        buffer: PBO,
        /// Meta-data about the cached rows.
        rows: Vec<CacheRow>,
        /// Mirrored block data on CPU.
        cpu_blocks: Vec<GpuBlockData>,
    },
    /// Shader-based scattering updates. Currently rendered by a set
    /// of points into the GPU texture, each carrying a `GpuBlockData`.
    Scatter {
        /// Special program to run the scattered update.
        program: Program,
        /// VAO containing the source vertex buffers.
        vao: CustomVAO,
        /// VBO for positional data, supplied as normalized `u16`.
        buf_position: VBO<[u16; 2]>,
        /// VBO for gpu block data.
        buf_value: VBO<GpuBlockData>,
        /// Currently stored block count.
        count: usize,
    },
}

/// The device-specific representation of the cache texture in gpu_cache.rs
struct CacheTexture {
    texture: Texture,
    bus: CacheBus,
}

impl CacheTexture {
    fn new(device: &mut Device, use_scatter: bool) -> Result<Self, RendererError> {
        let texture = device.create_texture(TextureTarget::Default, ImageFormat::RGBAF32);

        let bus = if use_scatter {
            let program = device
                .create_program("gpu_cache_update", "", &DESC_GPU_CACHE_UPDATE)?;
            let buf_position = device.create_vbo();
            let buf_value = device.create_vbo();
            //Note: the vertex attributes have to be supplied in the same order
            // as for program creation, but each assigned to a different stream.
            let vao = device.create_custom_vao(&[
                buf_position.stream_with(&DESC_GPU_CACHE_UPDATE.vertex_attributes[0..1]),
                buf_value   .stream_with(&DESC_GPU_CACHE_UPDATE.vertex_attributes[1..2]),
            ]);
            CacheBus::Scatter {
                program,
                vao,
                buf_position,
                buf_value,
                count: 0,
            }
        } else {
            let buffer = device.create_pbo();
            CacheBus::PixelBuffer {
                buffer,
                rows: Vec::new(),
                cpu_blocks: Vec::new(),
            }
        };

        Ok(CacheTexture {
            texture,
            bus,
        })
    }

    fn deinit(self, device: &mut Device) {
        device.delete_texture(self.texture);
        match self.bus {
            CacheBus::PixelBuffer { buffer, ..} => {
                device.delete_pbo(buffer);
            }
            CacheBus::Scatter { program, vao, buf_position, buf_value, ..} => {
                device.delete_program(program);
                device.delete_custom_vao(vao);
                device.delete_vbo(buf_position);
                device.delete_vbo(buf_value);
            }
        }
    }

    fn get_height(&self) -> u32 {
        self.texture.get_dimensions().height
    }

    fn prepare_for_updates(
        &mut self,
        device: &mut Device,
        total_block_count: usize,
        max_height: u32,
    ) {
        // See if we need to create or resize the texture.
        let old_size = self.texture.get_dimensions();
        let new_size = DeviceUintSize::new(MAX_VERTEX_TEXTURE_WIDTH as _, max_height);

        match self.bus {
            CacheBus::PixelBuffer { ref mut rows, .. } => {
                if max_height > old_size.height {
                    // Create a f32 texture that can be used for the vertex shader
                    // to fetch data from.
                    device.init_texture(
                        &mut self.texture,
                        new_size.width,
                        new_size.height,
                        TextureFilter::Nearest,
                        None,
                        1,
                        None,
                    );

                    // If we had to resize the texture, just mark all rows
                    // as dirty so they will be uploaded to the texture
                    // during the next flush.
                    for row in rows.iter_mut() {
                        row.is_dirty = true;
                    }
                }
            }
            CacheBus::Scatter {
                ref mut buf_position,
                ref mut buf_value,
                ref mut count,
                ..
            } => {
                *count = 0;
                if total_block_count > buf_value.allocated_count() {
                    device.allocate_vbo(buf_position, total_block_count, VertexUsageHint::Stream);
                    device.allocate_vbo(buf_value,    total_block_count, VertexUsageHint::Stream);
                }

                if new_size.height > old_size.height || GPU_CACHE_RESIZE_TEST {
                    if old_size.height > 0 {
                        device.resize_renderable_texture(&mut self.texture, new_size);
                    } else {
                        device.init_texture(
                            &mut self.texture,
                            new_size.width,
                            new_size.height,
                            TextureFilter::Nearest,
                            Some(RenderTargetInfo {
                                has_depth: false,
                            }),
                            1,
                            None,
                        );
                    }
                }
            }
        }
    }

    fn update(&mut self, device: &mut Device, updates: &GpuCacheUpdateList) {
        match self.bus {
            CacheBus::PixelBuffer { ref mut rows, ref mut cpu_blocks, .. } => {
                for update in &updates.updates {
                    match update {
                        &GpuCacheUpdate::Copy {
                            block_index,
                            block_count,
                            address,
                        } => {
                            let row = address.v as usize;

                            // Ensure that the CPU-side shadow copy of the GPU cache data has enough
                            // rows to apply this patch.
                            while rows.len() <= row {
                                // Add a new row.
                                rows.push(CacheRow::new());
                                // Add enough GPU blocks for this row.
                                cpu_blocks
                                    .extend_from_slice(&[GpuBlockData::empty(); MAX_VERTEX_TEXTURE_WIDTH]);
                            }

                            // This row is dirty (needs to be updated in GPU texture).
                            rows[row].is_dirty = true;

                            // Copy the blocks from the patch array in the shadow CPU copy.
                            let block_offset = row * MAX_VERTEX_TEXTURE_WIDTH + address.u as usize;
                            let data = &mut cpu_blocks[block_offset .. (block_offset + block_count)];
                            for i in 0 .. block_count {
                                data[i] = updates.blocks[block_index + i];
                            }
                        }
                    }
                }
            }
            CacheBus::Scatter {
                ref buf_position,
                ref buf_value,
                ref mut count,
                ..
            } => {
                //TODO: re-use this heap allocation
                // Unused positions will be left as 0xFFFF, which translates to
                // (1.0, 1.0) in the vertex output position and gets culled out
                let mut position_data = vec![[!0u16; 2]; updates.blocks.len()];
                let size = self.texture.get_dimensions().to_usize();

                for update in &updates.updates {
                    match update {
                        &GpuCacheUpdate::Copy {
                            block_index,
                            block_count,
                            address,
                        } => {
                            // Convert the absolute texel position into normalized
                            let y = ((2*address.v as usize + 1) << 15) / size.height;
                            for i in 0 .. block_count {
                                let x = ((2*address.u as usize + 2*i + 1) << 15) / size.width;
                                position_data[block_index + i] = [x as _, y as _];
                            }
                        }
                    }
                }

                device.fill_vbo(buf_value, &updates.blocks, *count);
                device.fill_vbo(buf_position, &position_data, *count);
                *count += position_data.len();
            }
        }
    }

    fn flush(&mut self, device: &mut Device) -> usize {
        match self.bus {
            CacheBus::PixelBuffer { ref buffer, ref mut rows, ref cpu_blocks } => {
                let rows_dirty = rows
                    .iter()
                    .filter(|row| row.is_dirty)
                    .count();
                if rows_dirty == 0 {
                    return 0
                }

                let mut uploader = device.upload_texture(
                    &self.texture,
                    buffer,
                    rows_dirty * MAX_VERTEX_TEXTURE_WIDTH,
                );

                for (row_index, row) in rows.iter_mut().enumerate() {
                    if !row.is_dirty {
                        continue;
                    }

                    let block_index = row_index * MAX_VERTEX_TEXTURE_WIDTH;
                    let cpu_blocks =
                        &cpu_blocks[block_index .. (block_index + MAX_VERTEX_TEXTURE_WIDTH)];
                    let rect = DeviceUintRect::new(
                        DeviceUintPoint::new(0, row_index as u32),
                        DeviceUintSize::new(MAX_VERTEX_TEXTURE_WIDTH as u32, 1),
                    );

                    uploader.upload(rect, 0, None, cpu_blocks);

                    row.is_dirty = false;
                }

                rows_dirty
            }
            CacheBus::Scatter { ref program, ref vao, count, .. } => {
                device.disable_depth();
                device.set_blend(false);
                device.bind_program(program);
                device.bind_custom_vao(vao);
                device.bind_draw_target(
                    Some((&self.texture, 0)),
                    Some(self.texture.get_dimensions()),
                );
                device.draw_nonindexed_points(0, count as _);
                0
            }
        }
    }
}

struct VertexDataTexture {
    texture: Texture,
    pbo: PBO,
}

impl VertexDataTexture {
    fn new(device: &mut Device) -> VertexDataTexture {
        let texture = device.create_texture(TextureTarget::Default, ImageFormat::RGBAF32);
        let pbo = device.create_pbo();

        VertexDataTexture { texture, pbo }
    }

    fn update<T>(&mut self, device: &mut Device, data: &mut Vec<T>) {
        if data.is_empty() {
            return;
        }

        debug_assert!(mem::size_of::<T>() % 16 == 0);
        let texels_per_item = mem::size_of::<T>() / 16;
        let items_per_row = MAX_VERTEX_TEXTURE_WIDTH / texels_per_item;

        // Extend the data array to be a multiple of the row size.
        // This ensures memory safety when the array is passed to
        // OpenGL to upload to the GPU.
        if items_per_row != 0 {
            while data.len() % items_per_row != 0 {
                data.push(unsafe { mem::uninitialized() });
            }
        }

        let width =
            (MAX_VERTEX_TEXTURE_WIDTH - (MAX_VERTEX_TEXTURE_WIDTH % texels_per_item)) as u32;
        let needed_height = (data.len() / items_per_row) as u32;

        // Determine if the texture needs to be resized.
        let texture_size = self.texture.get_dimensions();

        if needed_height > texture_size.height {
            let new_height = (needed_height + 127) & !127;

            device.init_texture(
                &mut self.texture,
                width,
                new_height,
                TextureFilter::Nearest,
                None,
                1,
                None,
            );
        }

        let rect = DeviceUintRect::new(
            DeviceUintPoint::zero(),
            DeviceUintSize::new(width, needed_height),
        );
        device
            .upload_texture(&self.texture, &self.pbo, 0)
            .upload(rect, 0, None, data);
    }

    fn deinit(self, device: &mut Device) {
        device.delete_pbo(self.pbo);
        device.delete_texture(self.texture);
    }
}

const TRANSFORM_FEATURE: &str = "TRANSFORM";
const ALPHA_FEATURE: &str = "ALPHA_PASS";

enum ShaderKind {
    Primitive,
    Cache(VertexArrayKind),
    ClipCache,
    Brush,
    Text,
}

struct LazilyCompiledShader {
    program: Option<Program>,
    name: &'static str,
    kind: ShaderKind,
    features: Vec<&'static str>,
}

impl LazilyCompiledShader {
    fn new(
        kind: ShaderKind,
        name: &'static str,
        features: &[&'static str],
        device: &mut Device,
        precache: bool,
    ) -> Result<LazilyCompiledShader, ShaderError> {
        let mut shader = LazilyCompiledShader {
            program: None,
            name,
            kind,
            features: features.to_vec(),
        };

        if precache {
            let t0 = precise_time_ns();
            let program = try!{ shader.get(device) };
            let t1 = precise_time_ns();
            device.bind_program(program);
            device.draw_triangles_u16(0, 3);
            let t2 = precise_time_ns();
            println!("[C: {:.1} ms D: {:.1} ms] Precache {} {:?}",
                (t1 - t0) as f64 / 1000000.0,
                (t2 - t1) as f64 / 1000000.0,
                name,
                features
            );
        }

        Ok(shader)
    }

    fn bind<M>(
        &mut self,
        device: &mut Device,
        projection: &Transform3D<f32>,
        mode: M,
        renderer_errors: &mut Vec<RendererError>,
    ) where M: Into<ShaderMode> {
        let program = match self.get(device) {
            Ok(program) => program,
            Err(e) => {
                renderer_errors.push(RendererError::from(e));
                return;
            }
        };
        device.bind_program(program);
        device.set_uniforms(program, projection, mode.into());
    }

    fn get(&mut self, device: &mut Device) -> Result<&Program, ShaderError> {
        if self.program.is_none() {
            let program = try!{
                match self.kind {
                    ShaderKind::Primitive | ShaderKind::Brush | ShaderKind::Text => {
                        create_prim_shader(self.name,
                                           device,
                                           &self.features,
                                           VertexArrayKind::Primitive)
                    }
                    ShaderKind::Cache(format) => {
                        create_prim_shader(self.name,
                                           device,
                                           &self.features,
                                           format)
                    }
                    ShaderKind::ClipCache => {
                        create_clip_shader(self.name, device)
                    }
                }
            };
            self.program = Some(program);
        }

        Ok(self.program.as_ref().unwrap())
    }

    fn deinit(self, device: &mut Device) {
        if let Some(program) = self.program {
            device.delete_program(program);
        }
    }
}

// A brush shader supports two modes:
// opaque:
//   Used for completely opaque primitives,
//   or inside segments of partially
//   opaque primitives. Assumes no need
//   for clip masks, AA etc.
// alpha:
//   Used for brush primitives in the alpha
//   pass. Assumes that AA should be applied
//   along the primitive edge, and also that
//   clip mask is present.
struct BrushShader {
    opaque: LazilyCompiledShader,
    alpha: LazilyCompiledShader,
}

impl BrushShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache: bool,
    ) -> Result<Self, ShaderError> {
        let opaque = try!{
            LazilyCompiledShader::new(ShaderKind::Brush,
                                      name,
                                      features,
                                      device,
                                      precache)
        };

        let mut alpha_features = features.to_vec();
        alpha_features.push(ALPHA_FEATURE);

        let alpha = try!{
            LazilyCompiledShader::new(ShaderKind::Brush,
                                      name,
                                      &alpha_features,
                                      device,
                                      precache)
        };

        Ok(BrushShader { opaque, alpha })
    }

    fn bind<M>(
        &mut self,
        device: &mut Device,
        blend_mode: BlendMode,
        projection: &Transform3D<f32>,
        mode: M,
        renderer_errors: &mut Vec<RendererError>,
    ) where M: Into<ShaderMode> {
        match blend_mode {
            BlendMode::None => {
                self.opaque.bind(device, projection, mode, renderer_errors)
            }
            BlendMode::Alpha |
            BlendMode::PremultipliedAlpha |
            BlendMode::PremultipliedDestOut |
            BlendMode::SubpixelDualSource |
            BlendMode::SubpixelConstantTextColor(..) |
            BlendMode::SubpixelVariableTextColor |
            BlendMode::SubpixelWithBgColor => {
                self.alpha.bind(device, projection, mode, renderer_errors)
            }
        }
    }

    fn deinit(self, device: &mut Device) {
        self.opaque.deinit(device);
        self.alpha.deinit(device);
    }
}

struct PrimitiveShader {
    simple: LazilyCompiledShader,
    transform: LazilyCompiledShader,
}

impl PrimitiveShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache: bool,
    ) -> Result<Self, ShaderError> {
        let simple = try!{
            LazilyCompiledShader::new(ShaderKind::Primitive,
                                      name,
                                      features,
                                      device,
                                      precache)
        };

        let mut transform_features = features.to_vec();
        transform_features.push(TRANSFORM_FEATURE);

        let transform = try!{
            LazilyCompiledShader::new(ShaderKind::Primitive,
                                      name,
                                      &transform_features,
                                      device,
                                      precache)
        };

        Ok(PrimitiveShader { simple, transform })
    }

    fn bind<M>(
        &mut self,
        device: &mut Device,
        transform_kind: TransformedRectKind,
        projection: &Transform3D<f32>,
        mode: M,
        renderer_errors: &mut Vec<RendererError>,
    ) where M: Into<ShaderMode> {
        match transform_kind {
            TransformedRectKind::AxisAligned => {
                self.simple.bind(device, projection, mode, renderer_errors)
            }
            TransformedRectKind::Complex => {
                self.transform.bind(device, projection, mode, renderer_errors)
            }
        }
    }

    fn deinit(self, device: &mut Device) {
        self.simple.deinit(device);
        self.transform.deinit(device);
    }
}

struct TextShader {
    simple: LazilyCompiledShader,
    transform: LazilyCompiledShader,
    glyph_transform: LazilyCompiledShader,
}

impl TextShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache: bool,
    ) -> Result<Self, ShaderError> {
        let simple = try!{
            LazilyCompiledShader::new(ShaderKind::Text,
                                      name,
                                      features,
                                      device,
                                      precache)
        };

        let mut transform_features = features.to_vec();
        transform_features.push("TRANSFORM");

        let transform = try!{
            LazilyCompiledShader::new(ShaderKind::Text,
                                      name,
                                      &transform_features,
                                      device,
                                      precache)
        };

        let mut glyph_transform_features = features.to_vec();
        glyph_transform_features.push("GLYPH_TRANSFORM");

        let glyph_transform = try!{
            LazilyCompiledShader::new(ShaderKind::Text,
                                      name,
                                      &glyph_transform_features,
                                      device,
                                      precache)
        };

        Ok(TextShader { simple, transform, glyph_transform })
    }

    fn bind<M>(
        &mut self,
        device: &mut Device,
        glyph_format: GlyphFormat,
        transform_kind: TransformedRectKind,
        projection: &Transform3D<f32>,
        mode: M,
        renderer_errors: &mut Vec<RendererError>,
    ) where M: Into<ShaderMode> {
        match glyph_format {
            GlyphFormat::Alpha |
            GlyphFormat::Subpixel |
            GlyphFormat::Bitmap |
            GlyphFormat::ColorBitmap => {
                match transform_kind {
                    TransformedRectKind::AxisAligned => {
                        self.simple.bind(device, projection, mode, renderer_errors)
                    }
                    TransformedRectKind::Complex => {
                        self.transform.bind(device, projection, mode, renderer_errors)
                    }
                }
            }
            GlyphFormat::TransformedAlpha |
            GlyphFormat::TransformedSubpixel => {
                self.glyph_transform.bind(device, projection, mode, renderer_errors)
            }
        }
    }

    fn deinit(self, device: &mut Device) {
        self.simple.deinit(device);
        self.transform.deinit(device);
        self.glyph_transform.deinit(device);
    }
}

fn create_prim_shader(
    name: &'static str,
    device: &mut Device,
    features: &[&'static str],
    vertex_format: VertexArrayKind,
) -> Result<Program, ShaderError> {
    let mut prefix = format!(
        "#define WR_MAX_VERTEX_TEXTURE_WIDTH {}\n",
        MAX_VERTEX_TEXTURE_WIDTH
    );

    for feature in features {
        prefix.push_str(&format!("#define WR_FEATURE_{}\n", feature));
    }

    debug!("PrimShader {}", name);

    let vertex_descriptor = match vertex_format {
        VertexArrayKind::Primitive => DESC_PRIM_INSTANCES,
        VertexArrayKind::Blur => DESC_BLUR,
        VertexArrayKind::Clip => DESC_CLIP,
    };

    let program = device.create_program(name, &prefix, &vertex_descriptor);

    if let Ok(ref program) = program {
        device.bind_shader_samplers(
            program,
            &[
                ("sColor0", TextureSampler::Color0),
                ("sColor1", TextureSampler::Color1),
                ("sColor2", TextureSampler::Color2),
                ("sDither", TextureSampler::Dither),
                ("sCacheA8", TextureSampler::CacheA8),
                ("sCacheRGBA8", TextureSampler::CacheRGBA8),
                ("sClipScrollNodes", TextureSampler::ClipScrollNodes),
                ("sRenderTasks", TextureSampler::RenderTasks),
                ("sResourceCache", TextureSampler::ResourceCache),
                ("sSharedCacheA8", TextureSampler::SharedCacheA8),
                ("sLocalClipRects", TextureSampler::LocalClipRects),
            ],
        );
    }

    program
}

fn create_clip_shader(name: &'static str, device: &mut Device) -> Result<Program, ShaderError> {
    let prefix = format!(
        "#define WR_MAX_VERTEX_TEXTURE_WIDTH {}\n
                          #define WR_FEATURE_TRANSFORM\n",
        MAX_VERTEX_TEXTURE_WIDTH
    );

    debug!("ClipShader {}", name);

    let program = device.create_program(name, &prefix, &DESC_CLIP);

    if let Ok(ref program) = program {
        device.bind_shader_samplers(
            program,
            &[
                ("sColor0", TextureSampler::Color0),
                ("sClipScrollNodes", TextureSampler::ClipScrollNodes),
                ("sRenderTasks", TextureSampler::RenderTasks),
                ("sResourceCache", TextureSampler::ResourceCache),
                ("sSharedCacheA8", TextureSampler::SharedCacheA8),
                ("sLocalClipRects", TextureSampler::LocalClipRects),
            ],
        );
    }

    program
}

fn get_external_image_target(ext_type: ExternalImageType) -> Option<TextureTarget> {
    Some(match ext_type {
        ExternalImageType::Texture2DHandle => TextureTarget::Default,
        ExternalImageType::Texture2DArrayHandle => TextureTarget::Array,
        ExternalImageType::TextureRectHandle => TextureTarget::Rect,
        ExternalImageType::TextureExternalHandle => TextureTarget::External,
        ExternalImageType::ExternalBuffer => return None,
    })
}

struct FileWatcher {
    notifier: Box<RenderNotifier>,
    result_tx: Sender<ResultMsg>,
}

impl FileWatcherHandler for FileWatcher {
    fn file_changed(&self, path: PathBuf) {
        self.result_tx.send(ResultMsg::RefreshShader(path)).ok();
        self.notifier.wake_up();
    }
}

struct FrameOutput {
    last_access: FrameId,
    fbo_id: FBOId,
}

#[derive(PartialEq)]
struct TargetSelector {
    size: DeviceUintSize,
    num_layers: usize,
    format: ImageFormat,
}

#[cfg(feature = "capture")]
struct RendererCapture {
    read_fbo: FBOId,
    owned_external_images: FastHashMap<(ExternalImageId, u8), ExternalTexture>,
}

// Note: we can't just feature-gate the fields because `cbindgen` fails on those.
// see https://github.com/eqrion/cbindgen/issues/116
#[cfg(not(feature = "capture"))]
struct RendererCapture;

/// The renderer is responsible for submitting to the GPU the work prepared by the
/// RenderBackend.
pub struct Renderer {
    result_rx: Receiver<ResultMsg>,
    debug_server: DebugServer,
    device: Device,
    pending_texture_updates: Vec<TextureUpdateList>,
    pending_gpu_cache_updates: Vec<GpuCacheUpdateList>,
    pending_shader_updates: Vec<PathBuf>,
    active_documents: Vec<(DocumentId, RenderedDocument)>,

    // These are "cache shaders". These shaders are used to
    // draw intermediate results to cache targets. The results
    // of these shaders are then used by the primitive shaders.
    cs_text_run: LazilyCompiledShader,
    cs_blur_a8: LazilyCompiledShader,
    cs_blur_rgba8: LazilyCompiledShader,

    // Brush shaders
    brush_mask_corner: LazilyCompiledShader,
    brush_mask_rounded_rect: LazilyCompiledShader,
    brush_picture_rgba8: BrushShader,
    brush_picture_rgba8_alpha_mask: BrushShader,
    brush_picture_a8: BrushShader,
    brush_solid: BrushShader,
    brush_line: BrushShader,

    /// These are "cache clip shaders". These shaders are used to
    /// draw clip instances into the cached clip mask. The results
    /// of these shaders are also used by the primitive shaders.
    cs_clip_rectangle: LazilyCompiledShader,
    cs_clip_image: LazilyCompiledShader,
    cs_clip_border: LazilyCompiledShader,

    // The are "primitive shaders". These shaders draw and blend
    // final results on screen. They are aware of tile boundaries.
    // Most draw directly to the framebuffer, but some use inputs
    // from the cache shaders to draw. Specifically, the box
    // shadow primitive shader stretches the box shadow cache
    // output, and the cache_image shader blits the results of
    // a cache shader (e.g. blur) to the screen.
    ps_text_run: TextShader,
    ps_text_run_dual_source: TextShader,
    ps_image: Vec<Option<PrimitiveShader>>,
    ps_yuv_image: Vec<Option<PrimitiveShader>>,
    ps_border_corner: PrimitiveShader,
    ps_border_edge: PrimitiveShader,
    ps_gradient: PrimitiveShader,
    ps_angle_gradient: PrimitiveShader,
    ps_radial_gradient: PrimitiveShader,

    ps_blend: LazilyCompiledShader,
    ps_hw_composite: LazilyCompiledShader,
    ps_split_composite: LazilyCompiledShader,
    ps_composite: LazilyCompiledShader,

    max_texture_size: u32,

    max_recorded_profiles: usize,
    clear_color: Option<ColorF>,
    enable_clear_scissor: bool,
    debug: DebugRenderer,
    debug_flags: DebugFlags,
    backend_profile_counters: BackendProfileCounters,
    profile_counters: RendererProfileCounters,
    profiler: Profiler,
    last_time: u64,

    gpu_profile: GpuProfiler<GpuProfileTag>,
    prim_vao: VAO,
    blur_vao: VAO,
    clip_vao: VAO,

    node_data_texture: VertexDataTexture,
    local_clip_rects_texture: VertexDataTexture,
    render_task_texture: VertexDataTexture,
    gpu_cache_texture: CacheTexture,

    pipeline_epoch_map: FastHashMap<PipelineId, Epoch>,

    // Manages and resolves source textures IDs to real texture IDs.
    texture_resolver: SourceTextureResolver,

    // A PBO used to do asynchronous texture cache uploads.
    texture_cache_upload_pbo: PBO,

    dither_matrix_texture: Option<Texture>,

    /// Optional trait object that allows the client
    /// application to provide external buffers for image data.
    external_image_handler: Option<Box<ExternalImageHandler>>,

    /// Optional trait object that allows the client
    /// application to provide a texture handle to
    /// copy the WR output to.
    output_image_handler: Option<Box<OutputImageHandler>>,

    // Currently allocated FBOs for output frames.
    output_targets: FastHashMap<u32, FrameOutput>,

    renderer_errors: Vec<RendererError>,

    /// List of profile results from previous frames. Can be retrieved
    /// via get_frame_profiles().
    cpu_profiles: VecDeque<CpuProfile>,
    gpu_profiles: VecDeque<GpuProfile>,

    #[cfg_attr(not(feature = "capture"), allow(dead_code))]
    capture: RendererCapture,
}

#[derive(Debug)]
pub enum RendererError {
    Shader(ShaderError),
    Thread(std::io::Error),
    MaxTextureSize,
}

impl From<ShaderError> for RendererError {
    fn from(err: ShaderError) -> Self {
        RendererError::Shader(err)
    }
}

impl From<std::io::Error> for RendererError {
    fn from(err: std::io::Error) -> Self {
        RendererError::Thread(err)
    }
}

impl Renderer {
    /// Initializes webrender and creates a `Renderer` and `RenderApiSender`.
    ///
    /// # Examples
    /// Initializes a `Renderer` with some reasonable values. For more information see
    /// [`RendererOptions`][rendereroptions].
    ///
    /// ```rust,ignore
    /// # use webrender::renderer::Renderer;
    /// # use std::path::PathBuf;
    /// let opts = webrender::RendererOptions {
    ///    device_pixel_ratio: 1.0,
    ///    resource_override_path: None,
    ///    enable_aa: false,
    /// };
    /// let (renderer, sender) = Renderer::new(opts);
    /// ```
    /// [rendereroptions]: struct.RendererOptions.html
    pub fn new(
        gl: Rc<gl::Gl>,
        notifier: Box<RenderNotifier>,
        mut options: RendererOptions,
    ) -> Result<(Renderer, RenderApiSender), RendererError> {
        let (api_tx, api_rx) = try!{ channel::msg_channel() };
        let (payload_tx, payload_rx) = try!{ channel::payload_channel() };
        let (result_tx, result_rx) = channel();
        let gl_type = gl.get_type();

        let debug_server = DebugServer::new(api_tx.clone());

        let file_watch_handler = FileWatcher {
            result_tx: result_tx.clone(),
            notifier: notifier.clone(),
        };

        let mut device = Device::new(
            gl,
            options.resource_override_path.clone(),
            options.upload_method,
            Box::new(file_watch_handler),
            options.cached_programs,
        );

        let ext_dual_source_blending = !options.disable_dual_source_blending &&
            device.supports_extension("GL_ARB_blend_func_extended");

        let device_max_size = device.max_texture_size();
        // 512 is the minimum that the texture cache can work with.
        // Broken GL contexts can return a max texture size of zero (See #1260). Better to
        // gracefully fail now than panic as soon as a texture is allocated.
        let min_texture_size = 512;
        if device_max_size < min_texture_size {
            println!(
                "Device reporting insufficient max texture size ({})",
                device_max_size
            );
            return Err(RendererError::MaxTextureSize);
        }
        let max_device_size = cmp::max(
            cmp::min(
                device_max_size,
                options.max_texture_size.unwrap_or(device_max_size),
            ),
            min_texture_size,
        );

        register_thread_with_profiler("Compositor".to_owned());

        device.begin_frame();

        let cs_text_run = try!{
            LazilyCompiledShader::new(ShaderKind::Cache(VertexArrayKind::Primitive),
                                      "cs_text_run",
                                      &[],
                                      &mut device,
                                      options.precache_shaders)
        };

        let brush_mask_corner = try!{
            LazilyCompiledShader::new(ShaderKind::Brush,
                                      "brush_mask_corner",
                                      &[],
                                      &mut device,
                                      options.precache_shaders)
        };

        let brush_mask_rounded_rect = try!{
            LazilyCompiledShader::new(ShaderKind::Brush,
                                      "brush_mask_rounded_rect",
                                      &[],
                                      &mut device,
                                      options.precache_shaders)
        };

        let brush_solid = try!{
            BrushShader::new("brush_solid",
                             &mut device,
                             &[],
                             options.precache_shaders)
        };

        let brush_line = try!{
            BrushShader::new("brush_line",
                             &mut device,
                             &[],
                             options.precache_shaders)
        };

        let brush_picture_a8 = try!{
            BrushShader::new("brush_picture",
                             &mut device,
                             &["ALPHA_TARGET"],
                             options.precache_shaders)
        };

        let brush_picture_rgba8 = try!{
            BrushShader::new("brush_picture",
                             &mut device,
                             &["COLOR_TARGET"],
                             options.precache_shaders)
        };

        let brush_picture_rgba8_alpha_mask = try!{
            BrushShader::new("brush_picture",
                             &mut device,
                             &["COLOR_TARGET_ALPHA_MASK"],
                             options.precache_shaders)
        };

        let cs_blur_a8 = try!{
            LazilyCompiledShader::new(ShaderKind::Cache(VertexArrayKind::Blur),
                                     "cs_blur",
                                      &["ALPHA_TARGET"],
                                      &mut device,
                                      options.precache_shaders)
        };

        let cs_blur_rgba8 = try!{
            LazilyCompiledShader::new(ShaderKind::Cache(VertexArrayKind::Blur),
                                     "cs_blur",
                                      &["COLOR_TARGET"],
                                      &mut device,
                                      options.precache_shaders)
        };

        let cs_clip_rectangle = try!{
            LazilyCompiledShader::new(ShaderKind::ClipCache,
                                      "cs_clip_rectangle",
                                      &[],
                                      &mut device,
                                      options.precache_shaders)
        };

        let cs_clip_image = try!{
            LazilyCompiledShader::new(ShaderKind::ClipCache,
                                      "cs_clip_image",
                                      &[],
                                      &mut device,
                                      options.precache_shaders)
        };

        let cs_clip_border = try!{
            LazilyCompiledShader::new(ShaderKind::ClipCache,
                                      "cs_clip_border",
                                      &[],
                                      &mut device,
                                      options.precache_shaders)
        };

        let ps_text_run = try!{
            TextShader::new("ps_text_run",
                            &mut device,
                            &[],
                            options.precache_shaders)
        };

        let ps_text_run_dual_source = try!{
            TextShader::new("ps_text_run",
                            &mut device,
                            &["DUAL_SOURCE_BLENDING"],
                            options.precache_shaders)
        };

        // All image configuration.
        let mut image_features = Vec::new();
        let mut ps_image: Vec<Option<PrimitiveShader>> = Vec::new();
        // PrimitiveShader is not clonable. Use push() to initialize the vec.
        for _ in 0 .. IMAGE_BUFFER_KINDS.len() {
            ps_image.push(None);
        }
        for buffer_kind in 0 .. IMAGE_BUFFER_KINDS.len() {
            if IMAGE_BUFFER_KINDS[buffer_kind].has_platform_support(&gl_type) {
                let feature_string = IMAGE_BUFFER_KINDS[buffer_kind].get_feature_string();
                if feature_string != "" {
                    image_features.push(feature_string);
                }
                let shader = try!{
                    PrimitiveShader::new("ps_image",
                                         &mut device,
                                         &image_features,
                                         options.precache_shaders)
                };
                ps_image[buffer_kind] = Some(shader);
            }
            image_features.clear();
        }

        // All yuv_image configuration.
        let mut yuv_features = Vec::new();
        let yuv_shader_num = IMAGE_BUFFER_KINDS.len() * YUV_FORMATS.len() * YUV_COLOR_SPACES.len();
        let mut ps_yuv_image: Vec<Option<PrimitiveShader>> = Vec::new();
        // PrimitiveShader is not clonable. Use push() to initialize the vec.
        for _ in 0 .. yuv_shader_num {
            ps_yuv_image.push(None);
        }
        for buffer_kind in 0 .. IMAGE_BUFFER_KINDS.len() {
            if IMAGE_BUFFER_KINDS[buffer_kind].has_platform_support(&gl_type) {
                for format_kind in 0 .. YUV_FORMATS.len() {
                    for color_space_kind in 0 .. YUV_COLOR_SPACES.len() {
                        let feature_string = IMAGE_BUFFER_KINDS[buffer_kind].get_feature_string();
                        if feature_string != "" {
                            yuv_features.push(feature_string);
                        }
                        let feature_string = YUV_FORMATS[format_kind].get_feature_string();
                        if feature_string != "" {
                            yuv_features.push(feature_string);
                        }
                        let feature_string =
                            YUV_COLOR_SPACES[color_space_kind].get_feature_string();
                        if feature_string != "" {
                            yuv_features.push(feature_string);
                        }

                        let shader = try!{
                            PrimitiveShader::new("ps_yuv_image",
                                                 &mut device,
                                                 &yuv_features,
                                                 options.precache_shaders)
                        };
                        let index = Renderer::get_yuv_shader_index(
                            IMAGE_BUFFER_KINDS[buffer_kind],
                            YUV_FORMATS[format_kind],
                            YUV_COLOR_SPACES[color_space_kind],
                        );
                        ps_yuv_image[index] = Some(shader);
                        yuv_features.clear();
                    }
                }
            }
        }

        let ps_border_corner = try!{
            PrimitiveShader::new("ps_border_corner",
                                 &mut device,
                                 &[],
                                 options.precache_shaders)
        };

        let ps_border_edge = try!{
            PrimitiveShader::new("ps_border_edge",
                                 &mut device,
                                 &[],
                                 options.precache_shaders)
        };

        let dithering_feature = ["DITHERING"];

        let ps_gradient = try!{
            PrimitiveShader::new("ps_gradient",
                                 &mut device,
                                 if options.enable_dithering {
                                    &dithering_feature
                                 } else {
                                    &[]
                                 },
                                 options.precache_shaders)
        };

        let ps_angle_gradient = try!{
            PrimitiveShader::new("ps_angle_gradient",
                                 &mut device,
                                 if options.enable_dithering {
                                    &dithering_feature
                                 } else {
                                    &[]
                                 },
                                 options.precache_shaders)
        };

        let ps_radial_gradient = try!{
            PrimitiveShader::new("ps_radial_gradient",
                                 &mut device,
                                 if options.enable_dithering {
                                    &dithering_feature
                                 } else {
                                    &[]
                                 },
                                 options.precache_shaders)
        };

        let ps_blend = try!{
            LazilyCompiledShader::new(ShaderKind::Primitive,
                                     "ps_blend",
                                     &[],
                                     &mut device,
                                     options.precache_shaders)
        };

        let ps_composite = try!{
            LazilyCompiledShader::new(ShaderKind::Primitive,
                                      "ps_composite",
                                      &[],
                                      &mut device,
                                      options.precache_shaders)
        };

        let ps_hw_composite = try!{
            LazilyCompiledShader::new(ShaderKind::Primitive,
                                     "ps_hardware_composite",
                                     &[],
                                     &mut device,
                                     options.precache_shaders)
        };

        let ps_split_composite = try!{
            LazilyCompiledShader::new(ShaderKind::Primitive,
                                     "ps_split_composite",
                                     &[],
                                     &mut device,
                                     options.precache_shaders)
        };

        let texture_cache = TextureCache::new(max_device_size);
        let max_texture_size = texture_cache.max_texture_size();

        let backend_profile_counters = BackendProfileCounters::new();

        let dither_matrix_texture = if options.enable_dithering {
            let dither_matrix: [u8; 64] = [
                00,
                48,
                12,
                60,
                03,
                51,
                15,
                63,
                32,
                16,
                44,
                28,
                35,
                19,
                47,
                31,
                08,
                56,
                04,
                52,
                11,
                59,
                07,
                55,
                40,
                24,
                36,
                20,
                43,
                27,
                39,
                23,
                02,
                50,
                14,
                62,
                01,
                49,
                13,
                61,
                34,
                18,
                46,
                30,
                33,
                17,
                45,
                29,
                10,
                58,
                06,
                54,
                09,
                57,
                05,
                53,
                42,
                26,
                38,
                22,
                41,
                25,
                37,
                21,
            ];

            let mut texture = device
                .create_texture(TextureTarget::Default, ImageFormat::R8);
            device.init_texture(
                &mut texture,
                8,
                8,
                TextureFilter::Nearest,
                None,
                1,
                Some(&dither_matrix),
            );

            Some(texture)
        } else {
            None
        };

        let debug_renderer = DebugRenderer::new(&mut device);

        let x0 = 0.0;
        let y0 = 0.0;
        let x1 = 1.0;
        let y1 = 1.0;

        let quad_indices: [u16; 6] = [0, 1, 2, 2, 1, 3];
        let quad_vertices = [
            PackedVertex { pos: [x0, y0] },
            PackedVertex { pos: [x1, y0] },
            PackedVertex { pos: [x0, y1] },
            PackedVertex { pos: [x1, y1] },
        ];

        let prim_vao = device.create_vao(&DESC_PRIM_INSTANCES);
        device.bind_vao(&prim_vao);
        device.update_vao_indices(&prim_vao, &quad_indices, VertexUsageHint::Static);
        device.update_vao_main_vertices(&prim_vao, &quad_vertices, VertexUsageHint::Static);

        let blur_vao = device.create_vao_with_new_instances(&DESC_BLUR, &prim_vao);
        let clip_vao = device.create_vao_with_new_instances(&DESC_CLIP, &prim_vao);

        let texture_cache_upload_pbo = device.create_pbo();

        let texture_resolver = SourceTextureResolver::new(&mut device);

        let node_data_texture = VertexDataTexture::new(&mut device);
        let local_clip_rects_texture = VertexDataTexture::new(&mut device);
        let render_task_texture = VertexDataTexture::new(&mut device);

        let gpu_cache_texture = CacheTexture::new(
            &mut device,
            options.scatter_gpu_cache_updates,
        )?;

        device.end_frame();

        let backend_notifier = notifier.clone();

        let default_font_render_mode = match (options.enable_aa, options.enable_subpixel_aa) {
            (true, true) => FontRenderMode::Subpixel,
            (true, false) => FontRenderMode::Alpha,
            (false, _) => FontRenderMode::Mono,
        };

        let config = FrameBuilderConfig {
            enable_scrollbars: options.enable_scrollbars,
            default_font_render_mode,
            debug: options.debug,
            dual_source_blending_is_enabled: true,
            dual_source_blending_is_supported: ext_dual_source_blending,
        };

        let device_pixel_ratio = options.device_pixel_ratio;
        // First set the flags to default and later call set_debug_flags to ensure any
        // potential transition when enabling a flag is run.
        let debug_flags = DebugFlags::default();
        let payload_tx_for_backend = payload_tx.clone();
        let recorder = options.recorder;
        let thread_listener = Arc::new(options.thread_listener);
        let thread_listener_for_rayon_start = thread_listener.clone();
        let thread_listener_for_rayon_end = thread_listener.clone();
        let workers = options
            .workers
            .take()
            .unwrap_or_else(|| {
                let worker_config = ThreadPoolConfig::new()
                    .thread_name(|idx|{ format!("WRWorker#{}", idx) })
                    .start_handler(move |idx| {
                        register_thread_with_profiler(format!("WRWorker#{}", idx));
                        if let Some(ref thread_listener) = *thread_listener_for_rayon_start {
                            thread_listener.thread_started(&format!("WRWorker#{}", idx));
                        }
                    })
                    .exit_handler(move |idx| {
                        if let Some(ref thread_listener) = *thread_listener_for_rayon_end {
                            thread_listener.thread_stopped(&format!("WRWorker#{}", idx));
                        }
                    });
                Arc::new(ThreadPool::new(worker_config).unwrap())
            });
        let enable_render_on_scroll = options.enable_render_on_scroll;

        let blob_image_renderer = options.blob_image_renderer.take();
        let thread_listener_for_render_backend = thread_listener.clone();
        let thread_name = format!("WRRenderBackend#{}", options.renderer_id.unwrap_or(0));
        try!{
            thread::Builder::new().name(thread_name.clone()).spawn(move || {
                register_thread_with_profiler(thread_name.clone());
                if let Some(ref thread_listener) = *thread_listener_for_render_backend {
                    thread_listener.thread_started(&thread_name);
                }
                let mut backend = RenderBackend::new(
                    api_rx,
                    payload_rx,
                    payload_tx_for_backend,
                    result_tx,
                    device_pixel_ratio,
                    texture_cache,
                    workers,
                    backend_notifier,
                    config,
                    recorder,
                    blob_image_renderer,
                    enable_render_on_scroll,
                );
                backend.run(backend_profile_counters);
                if let Some(ref thread_listener) = *thread_listener_for_render_backend {
                    thread_listener.thread_stopped(&thread_name);
                }
            })
        };

        #[cfg(feature = "capture")]
        let capture = RendererCapture {
            read_fbo: device.create_fbo_for_external_texture(0),
            owned_external_images: FastHashMap::default(),
        };
        #[cfg(not(feature = "capture"))]
        let capture = RendererCapture;

        let gpu_profile = GpuProfiler::new(Rc::clone(device.rc_gl()));

        let mut renderer = Renderer {
            result_rx,
            debug_server,
            device,
            active_documents: Vec::new(),
            pending_texture_updates: Vec::new(),
            pending_gpu_cache_updates: Vec::new(),
            pending_shader_updates: Vec::new(),
            cs_text_run,
            cs_blur_a8,
            cs_blur_rgba8,
            brush_mask_corner,
            brush_mask_rounded_rect,
            brush_picture_rgba8,
            brush_picture_rgba8_alpha_mask,
            brush_picture_a8,
            brush_solid,
            brush_line,
            cs_clip_rectangle,
            cs_clip_border,
            cs_clip_image,
            ps_text_run,
            ps_text_run_dual_source,
            ps_image,
            ps_yuv_image,
            ps_border_corner,
            ps_border_edge,
            ps_gradient,
            ps_angle_gradient,
            ps_radial_gradient,
            ps_blend,
            ps_hw_composite,
            ps_split_composite,
            ps_composite,
            debug: debug_renderer,
            debug_flags,
            backend_profile_counters: BackendProfileCounters::new(),
            profile_counters: RendererProfileCounters::new(),
            profiler: Profiler::new(),
            max_texture_size: max_texture_size,
            max_recorded_profiles: options.max_recorded_profiles,
            clear_color: options.clear_color,
            enable_clear_scissor: options.enable_clear_scissor,
            last_time: 0,
            gpu_profile,
            prim_vao,
            blur_vao,
            clip_vao,
            node_data_texture,
            local_clip_rects_texture,
            render_task_texture,
            pipeline_epoch_map: FastHashMap::default(),
            dither_matrix_texture,
            external_image_handler: None,
            output_image_handler: None,
            output_targets: FastHashMap::default(),
            cpu_profiles: VecDeque::new(),
            gpu_profiles: VecDeque::new(),
            gpu_cache_texture,
            texture_cache_upload_pbo,
            texture_resolver,
            renderer_errors: Vec::new(),
            capture,
        };

        renderer.set_debug_flags(options.debug_flags);

        let sender = RenderApiSender::new(api_tx, payload_tx);
        Ok((renderer, sender))
    }

    pub fn get_max_texture_size(&self) -> u32 {
        self.max_texture_size
    }

    pub fn get_graphics_api_info(&self) -> GraphicsApiInfo {
        GraphicsApiInfo {
            kind: GraphicsApi::OpenGL,
            version: self.device.gl().get_string(gl::VERSION),
            renderer: self.device.gl().get_string(gl::RENDERER),
        }
    }

    fn get_yuv_shader_index(
        buffer_kind: ImageBufferKind,
        format: YuvFormat,
        color_space: YuvColorSpace,
    ) -> usize {
        ((buffer_kind as usize) * YUV_FORMATS.len() + (format as usize)) * YUV_COLOR_SPACES.len() +
            (color_space as usize)
    }

    /// Returns the Epoch of the current frame in a pipeline.
    pub fn current_epoch(&self, pipeline_id: PipelineId) -> Option<Epoch> {
        self.pipeline_epoch_map.get(&pipeline_id).cloned()
    }

    /// Returns a HashMap containing the pipeline ids that have been received by the renderer and
    /// their respective epochs since the last time the method was called.
    pub fn flush_rendered_epochs(&mut self) -> FastHashMap<PipelineId, Epoch> {
        mem::replace(&mut self.pipeline_epoch_map, FastHashMap::default())
    }

    // update the program cache with new binaries, e.g. when some of the lazy loaded
    // shader programs got activated in the mean time
    pub fn update_program_cache(&mut self, cached_programs: Rc<ProgramCache>) {
        self.device.update_program_cache(cached_programs);
    }

    /// Processes the result queue.
    ///
    /// Should be called before `render()`, as texture cache updates are done here.
    pub fn update(&mut self) {
        profile_scope!("update");

        // Pull any pending results and return the most recent.
        while let Ok(msg) = self.result_rx.try_recv() {
            match msg {
                ResultMsg::PublishDocument(
                    document_id,
                    mut doc,
                    texture_update_list,
                    profile_counters,
                ) => {
                    //TODO: associate `document_id` with target window
                    self.pending_texture_updates.push(texture_update_list);
                    self.pending_gpu_cache_updates.extend(doc.frame.gpu_cache_updates.take());
                    self.backend_profile_counters = profile_counters;

                    // Update the list of available epochs for use during reftests.
                    // This is a workaround for https://github.com/servo/servo/issues/13149.
                    for (pipeline_id, epoch) in &doc.pipeline_epoch_map {
                        self.pipeline_epoch_map.insert(*pipeline_id, *epoch);
                    }

                    // Add a new document to the active set, expressed as a `Vec` in order
                    // to re-order based on `DocumentLayer` during rendering.
                    match self.active_documents.iter().position(|&(id, _)| id == document_id) {
                        Some(pos) => {
                            // If the document we are replacing must be drawn
                            // (in order to update the texture cache), issue
                            // a render just to off-screen targets.
                            if self.active_documents[pos].1.frame.must_be_drawn() {
                                self.render_impl(None).ok();
                            }
                            self.active_documents[pos].1 = doc;
                        }
                        None => self.active_documents.push((document_id, doc)),
                    }
                }
                ResultMsg::UpdateResources {
                    updates,
                    cancel_rendering,
                } => {
                    self.pending_texture_updates.push(updates);
                    self.update_texture_cache();
                    // If we receive a `PublishDocument` message followed by this one
                    // within the same update we need ot cancel the frame because we
                    // might have deleted the resources in use in the frame due to a
                    // memory pressure event.
                    if cancel_rendering {
                        self.active_documents.clear();
                    }
                }
                ResultMsg::RefreshShader(path) => {
                    self.pending_shader_updates.push(path);
                }
                ResultMsg::DebugOutput(output) => match output {
                    DebugOutput::FetchDocuments(string) |
                    DebugOutput::FetchClipScrollTree(string) => {
                        self.debug_server.send(string);
                    }
                    #[cfg(feature = "capture")]
                    DebugOutput::SaveCapture(config, deferred) => {
                        self.save_capture(config, deferred);
                    }
                    #[cfg(feature = "capture")]
                    DebugOutput::LoadCapture(root) => {
                        self.active_documents.clear();
                        self.load_capture(root);
                    }
                },
                ResultMsg::DebugCommand(command) => {
                    self.handle_debug_command(command);
                }
            }
        }
    }

    #[cfg(not(feature = "debugger"))]
    fn get_screenshot_for_debugger(&mut self) -> String {
        // Avoid unused param warning.
        let _ = &self.debug_server;
        String::new()
    }


    #[cfg(feature = "debugger")]
    fn get_screenshot_for_debugger(&mut self) -> String {
        use api::ImageDescriptor;

        let desc = ImageDescriptor::new(1024, 768, ImageFormat::BGRA8, true);
        let data = self.device.read_pixels(&desc);
        let screenshot = debug_server::Screenshot::new(desc.width, desc.height, data);

        serde_json::to_string(&screenshot).unwrap()
    }

    #[cfg(not(feature = "debugger"))]
    fn get_passes_for_debugger(&self) -> String {
        // Avoid unused param warning.
        let _ = &self.debug_server;
        String::new()
    }

    #[cfg(feature = "debugger")]
    fn debug_alpha_target(target: &AlphaRenderTarget) -> debug_server::Target {
        let mut debug_target = debug_server::Target::new("A8");

        debug_target.add(
            debug_server::BatchKind::Cache,
            "Scalings",
            target.scalings.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Zero Clears",
            target.zero_clears.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Clip,
            "Clear",
            target.clip_batcher.border_clears.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Clip,
            "Borders",
            target.clip_batcher.borders.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Vertical Blur",
            target.vertical_blurs.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Horizontal Blur",
            target.horizontal_blurs.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Clip,
            "Rectangles",
            target.clip_batcher.rectangles.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Rectangle Brush (Corner)",
            target.brush_mask_corners.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Rectangle Brush (Rounded Rect)",
            target.brush_mask_rounded_rects.len(),
        );
        for (_, items) in target.clip_batcher.images.iter() {
            debug_target.add(debug_server::BatchKind::Clip, "Image mask", items.len());
        }

        debug_target
    }

    #[cfg(feature = "debugger")]
    fn debug_color_target(target: &ColorRenderTarget) -> debug_server::Target {
        let mut debug_target = debug_server::Target::new("RGBA8");

        debug_target.add(
            debug_server::BatchKind::Cache,
            "Scalings",
            target.scalings.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Readbacks",
            target.readbacks.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Vertical Blur",
            target.vertical_blurs.len(),
        );
        debug_target.add(
            debug_server::BatchKind::Cache,
            "Horizontal Blur",
            target.horizontal_blurs.len(),
        );
        for (_, batch) in &target.alpha_batcher.text_run_cache_prims {
            debug_target.add(
                debug_server::BatchKind::Cache,
                "Text Shadow",
                batch.len(),
            );
        }

        for batch in target
            .alpha_batcher
            .batch_list
            .opaque_batch_list
            .batches
            .iter()
            .rev()
        {
            debug_target.add(
                debug_server::BatchKind::Opaque,
                batch.key.kind.debug_name(),
                batch.instances.len(),
            );
        }

        for batch in &target.alpha_batcher.batch_list.alpha_batch_list.batches {
            debug_target.add(
                debug_server::BatchKind::Alpha,
                batch.key.kind.debug_name(),
                batch.instances.len(),
            );
        }

        debug_target
    }

    #[cfg(feature = "debugger")]
    fn debug_texture_cache_target(target: &TextureCacheRenderTarget) -> debug_server::Target {
        let mut debug_target = debug_server::Target::new("Texture Cache");

        debug_target.add(
            debug_server::BatchKind::Cache,
            "Horizontal Blur",
            target.horizontal_blurs.len(),
        );

        debug_target
    }

    #[cfg(feature = "debugger")]
    fn get_passes_for_debugger(&self) -> String {
        let mut debug_passes = debug_server::PassList::new();

        for &(_, ref render_doc) in &self.active_documents {
            for pass in &render_doc.frame.passes {
                let mut debug_targets = Vec::new();
                match pass.kind {
                    RenderPassKind::MainFramebuffer(ref target) => {
                        debug_targets.push(Self::debug_color_target(target));
                    }
                    RenderPassKind::OffScreen { ref alpha, ref color, ref texture_cache } => {
                        debug_targets.extend(alpha.targets.iter().map(Self::debug_alpha_target));
                        debug_targets.extend(color.targets.iter().map(Self::debug_color_target));
                        debug_targets.extend(texture_cache.iter().map(|(_, target)| Self::debug_texture_cache_target(target)))
                    }
                }

                debug_passes.add(debug_server::Pass { targets: debug_targets });
            }
        }

        serde_json::to_string(&debug_passes).unwrap()
    }

    #[cfg(not(feature = "debugger"))]
    fn get_render_tasks_for_debugger(&self) -> String {
        String::new()
    }

    #[cfg(feature = "debugger")]
    fn get_render_tasks_for_debugger(&self) -> String {
        let mut debug_root = debug_server::RenderTaskList::new();

        for &(_, ref render_doc) in &self.active_documents {
            let debug_node = debug_server::TreeNode::new("document render tasks");
            let mut builder = debug_server::TreeNodeBuilder::new(debug_node);

            let render_tasks = &render_doc.frame.render_tasks;
            match render_tasks.tasks.last() {
                Some(main_task) => main_task.print_with(&mut builder, render_tasks),
                None => continue,
            };

            debug_root.add(builder.build());
        }

        serde_json::to_string(&debug_root).unwrap()
    }

    fn handle_debug_command(&mut self, command: DebugCommand) {
        match command {
            DebugCommand::EnableProfiler(enable) => {
                self.set_debug_flag(DebugFlags::PROFILER_DBG, enable);
            }
            DebugCommand::EnableTextureCacheDebug(enable) => {
                self.set_debug_flag(DebugFlags::TEXTURE_CACHE_DBG, enable);
            }
            DebugCommand::EnableRenderTargetDebug(enable) => {
                self.set_debug_flag(DebugFlags::RENDER_TARGET_DBG, enable);
            }
            DebugCommand::EnableAlphaRectsDebug(enable) => {
                self.set_debug_flag(DebugFlags::ALPHA_PRIM_DBG, enable);
            }
            DebugCommand::EnableGpuTimeQueries(enable) => {
                self.set_debug_flag(DebugFlags::GPU_TIME_QUERIES, enable);
            }
            DebugCommand::EnableGpuSampleQueries(enable) => {
                self.set_debug_flag(DebugFlags::GPU_SAMPLE_QUERIES, enable);
            }
            DebugCommand::FetchDocuments |
            DebugCommand::FetchClipScrollTree => {}
            DebugCommand::FetchRenderTasks => {
                let json = self.get_render_tasks_for_debugger();
                self.debug_server.send(json);
            }
            DebugCommand::FetchPasses => {
                let json = self.get_passes_for_debugger();
                self.debug_server.send(json);
            }
            DebugCommand::FetchScreenshot => {
                let json = self.get_screenshot_for_debugger();
                self.debug_server.send(json);
            }
            DebugCommand::SaveCapture(..) |
            DebugCommand::LoadCapture(..) => {
                panic!("Capture commands are not welcome here! Did you build with 'capture' feature?")
            }
            DebugCommand::EnableDualSourceBlending(_) => {
                panic!("Should be handled by render backend");
            }
        }
    }

    /// Set a callback for handling external images.
    pub fn set_external_image_handler(&mut self, handler: Box<ExternalImageHandler>) {
        self.external_image_handler = Some(handler);
    }

    /// Set a callback for handling external outputs.
    pub fn set_output_image_handler(&mut self, handler: Box<OutputImageHandler>) {
        self.output_image_handler = Some(handler);
    }

    /// Retrieve (and clear) the current list of recorded frame profiles.
    pub fn get_frame_profiles(&mut self) -> (Vec<CpuProfile>, Vec<GpuProfile>) {
        let cpu_profiles = self.cpu_profiles.drain(..).collect();
        let gpu_profiles = self.gpu_profiles.drain(..).collect();
        (cpu_profiles, gpu_profiles)
    }

    /// Returns `true` if the active rendered documents (that need depth buffer)
    /// intersect on the main framebuffer, in which case we don't clear
    /// the whole depth and instead clear each document area separately.
    fn are_documents_intersecting_depth(&self) -> bool {
        let document_rects = self.active_documents
            .iter()
            .filter_map(|&(_, ref render_doc)| {
                match render_doc.frame.passes.last() {
                    Some(&RenderPass { kind: RenderPassKind::MainFramebuffer(ref target), .. })
                        if target.needs_depth() => Some(render_doc.frame.inner_rect),
                    _ => None,
                }
            })
            .collect::<Vec<_>>();

        for (i, rect) in document_rects.iter().enumerate() {
            for other in &document_rects[i+1 ..] {
                if rect.intersects(other) {
                    return true
                }
            }
        }

        false
    }

    /// Renders the current frame.
    ///
    /// A Frame is supplied by calling [`generate_frame()`][genframe].
    /// [genframe]: ../../webrender_api/struct.DocumentApi.html#method.generate_frame
    pub fn render(
        &mut self,
        framebuffer_size: DeviceUintSize,
    ) -> Result<RendererStats, Vec<RendererError>> {
        self.render_impl(Some(framebuffer_size))
    }

    // If framebuffer_size is None, don't render
    // to the main frame buffer. This is useful
    // to update texture cache render tasks but
    // avoid doing a full frame render.
    fn render_impl(
        &mut self,
        framebuffer_size: Option<DeviceUintSize>
    ) -> Result<RendererStats, Vec<RendererError>> {
        profile_scope!("render");

        if self.active_documents.is_empty() {
            self.last_time = precise_time_ns();
            return Ok(RendererStats::empty());
        }

        let mut stats = RendererStats::empty();
        let mut frame_profiles = Vec::new();
        let mut profile_timers = RendererProfileTimers::new();

        let profile_samplers = {
            let _gm = self.gpu_profile.start_marker("build samples");
            // Block CPU waiting for last frame's GPU profiles to arrive.
            // In general this shouldn't block unless heavily GPU limited.
            let (gpu_frame_id, timers, samplers) = self.gpu_profile.build_samples();

            if self.max_recorded_profiles > 0 {
                while self.gpu_profiles.len() >= self.max_recorded_profiles {
                    self.gpu_profiles.pop_front();
                }
                self.gpu_profiles
                    .push_back(GpuProfile::new(gpu_frame_id, &timers));
            }
            profile_timers.gpu_samples = timers;
            samplers
        };


        let cpu_frame_id = profile_timers.cpu_time.profile(|| {
            let _gm = self.gpu_profile.start_marker("begin frame");
            let frame_id = self.device.begin_frame();
            self.gpu_profile.begin_frame(frame_id);

            self.device.disable_scissor();
            self.device.disable_depth();
            self.device.set_blend(false);
            //self.update_shaders();

            self.update_texture_cache();

            frame_id
        });

        profile_timers.cpu_time.profile(|| {
            let clear_depth_value = if self.are_documents_intersecting_depth() {
                None
            } else {
                Some(1.0)
            };

            //Note: another borrowck dance
            let mut active_documents = mem::replace(&mut self.active_documents, Vec::default());
            // sort by the document layer id
            active_documents.sort_by_key(|&(_, ref render_doc)| render_doc.frame.layer);

            // don't clear the framebuffer if one of the rendered documents will overwrite it
            if let Some(framebuffer_size) = framebuffer_size {
                let needs_color_clear = !active_documents
                    .iter()
                    .any(|&(_, RenderedDocument { ref frame, .. })| {
                        frame.background_color.is_some() &&
                        frame.inner_rect.origin == DeviceUintPoint::zero() &&
                        frame.inner_rect.size == framebuffer_size
                    });

                if needs_color_clear || clear_depth_value.is_some() {
                    let clear_color = if needs_color_clear {
                        self.clear_color.map(|color| color.to_array())
                    } else {
                        None
                    };
                    self.device.bind_draw_target(None, None);
                    self.device.enable_depth_write();
                    self.device.clear_target(clear_color, clear_depth_value, None);
                    self.device.disable_depth_write();
                }
            }

            // Re-use whatever targets possible from the pool, before
            // they get changed/re-allocated by the rendered frames.
            for doc_with_id in &mut active_documents {
                self.prepare_tile_frame(&mut doc_with_id.1.frame);
            }

            #[cfg(feature = "capture")]
            self.texture_resolver.external_images.extend(
                self.capture.owned_external_images.iter().map(|(key, value)| (*key, value.clone()))
            );

            for &mut (_, RenderedDocument { ref mut frame, .. }) in &mut active_documents {
                self.prepare_gpu_cache(frame);

                self.draw_tile_frame(
                    frame,
                    framebuffer_size,
                    clear_depth_value.is_some(),
                    cpu_frame_id,
                    &mut stats
                );

                if self.debug_flags.contains(DebugFlags::PROFILER_DBG) {
                    frame_profiles.push(frame.profile_counters.clone());
                }
            }

            self.unlock_external_images();
            self.active_documents = active_documents;
        });

        let current_time = precise_time_ns();
        let ns = current_time - self.last_time;
        self.profile_counters.frame_time.set(ns);

        if self.max_recorded_profiles > 0 {
            while self.cpu_profiles.len() >= self.max_recorded_profiles {
                self.cpu_profiles.pop_front();
            }
            let cpu_profile = CpuProfile::new(
                cpu_frame_id,
                self.backend_profile_counters.total_time.get(),
                profile_timers.cpu_time.get(),
                self.profile_counters.draw_calls.get(),
            );
            self.cpu_profiles.push_back(cpu_profile);
        }

        if self.debug_flags.contains(DebugFlags::PROFILER_DBG) {
            if let Some(framebuffer_size) = framebuffer_size {
                //TODO: take device/pixel ratio into equation?
                let screen_fraction = 1.0 / framebuffer_size.to_f32().area();
                self.profiler.draw_profile(
                    &frame_profiles,
                    &self.backend_profile_counters,
                    &self.profile_counters,
                    &mut profile_timers,
                    &profile_samplers,
                    screen_fraction,
                    &mut self.debug,
                    self.debug_flags.contains(DebugFlags::COMPACT_PROFILER),
                );
            }
        }

        self.profile_counters.reset();
        self.profile_counters.frame_counter.inc();

        profile_timers.cpu_time.profile(|| {
            let _gm = self.gpu_profile.start_marker("end frame");
            self.gpu_profile.end_frame();
            self.debug.render(&mut self.device, framebuffer_size);
            self.device.end_frame();
        });
        self.last_time = current_time;

        if self.renderer_errors.is_empty() {
            Ok(stats)
        } else {
            Err(mem::replace(&mut self.renderer_errors, Vec::new()))
        }
    }

    pub fn layers_are_bouncing_back(&self) -> bool {
        self.active_documents
            .iter()
            .any(|&(_, ref render_doc)| !render_doc.layers_bouncing_back.is_empty())
    }

    fn prepare_gpu_cache(&mut self, frame: &Frame) {
        let _gm = self.gpu_profile.start_marker("gpu cache update");

        let deferred_update_list = self.update_deferred_resolves(&frame.deferred_resolves);
        self.pending_gpu_cache_updates.extend(deferred_update_list);

        // For an artificial stress test of GPU cache resizing,
        // always pass an extra update list with at least one block in it.
        let gpu_cache_height = self.gpu_cache_texture.get_height();
        if gpu_cache_height != 0 &&  GPU_CACHE_RESIZE_TEST {
            self.pending_gpu_cache_updates.push(GpuCacheUpdateList {
                height: gpu_cache_height,
                blocks: vec![[1f32; 4].into()],
                updates: Vec::new(),
            });
        }

        let (updated_blocks, max_requested_height) = self
            .pending_gpu_cache_updates
            .iter()
            .fold((0, gpu_cache_height), |(count, height), list| {
                (count + list.blocks.len(), cmp::max(height, list.height))
            });

        //Note: if we decide to switch to scatter-style GPU cache update
        // permanently, we can have this code nicer with `BufferUploader` kind
        // of helper, similarly to how `TextureUploader` API is used.
        self.gpu_cache_texture.prepare_for_updates(
            &mut self.device,
            updated_blocks,
            max_requested_height,
        );

        for update_list in self.pending_gpu_cache_updates.drain(..) {
            assert!(update_list.height <= max_requested_height);
            self.gpu_cache_texture
                .update(&mut self.device, &update_list);
        }

        let updated_rows = self.gpu_cache_texture.flush(&mut self.device);

        // Note: the texture might have changed during the `update`,
        // so we need to bind it here.
        self.device.bind_texture(
            TextureSampler::ResourceCache,
            &self.gpu_cache_texture.texture,
        );

        let counters = &mut self.backend_profile_counters.resources.gpu_cache;
        counters.updated_rows.set(updated_rows);
        counters.updated_blocks.set(updated_blocks);
    }

    fn update_texture_cache(&mut self) {
        let _gm = self.gpu_profile.start_marker("texture cache update");
        let mut pending_texture_updates = mem::replace(&mut self.pending_texture_updates, vec![]);

        for update_list in pending_texture_updates.drain(..) {
            for update in update_list.updates {
                match update.op {
                    TextureUpdateOp::Create {
                        width,
                        height,
                        layer_count,
                        format,
                        filter,
                        render_target,
                    } => {
                        let CacheTextureId(cache_texture_index) = update.id;
                        if self.texture_resolver.cache_texture_map.len() == cache_texture_index {
                            // Create a new native texture, as requested by the texture cache.
                            let texture = self.device.create_texture(TextureTarget::Array, format);
                            self.texture_resolver.cache_texture_map.push(texture);
                        }
                        let texture =
                            &mut self.texture_resolver.cache_texture_map[cache_texture_index];
                        assert_eq!(texture.get_format(), format);

                        // Ensure no PBO is bound when creating the texture storage,
                        // or GL will attempt to read data from there.
                        self.device.init_texture(
                            texture,
                            width,
                            height,
                            filter,
                            render_target,
                            layer_count,
                            None,
                        );
                    }
                    TextureUpdateOp::Update {
                        rect,
                        source,
                        stride,
                        layer_index,
                        offset,
                    } => {
                        let texture = &self.texture_resolver.cache_texture_map[update.id.0];
                        let mut uploader = self.device.upload_texture(
                            texture,
                            &self.texture_cache_upload_pbo,
                            0,
                        );

                        match source {
                            TextureUpdateSource::Bytes { data } => {
                                uploader.upload(
                                    rect, layer_index, stride,
                                    &data[offset as usize ..],
                                );
                            }
                            TextureUpdateSource::External { id, channel_index } => {
                                let handler = self.external_image_handler
                                    .as_mut()
                                    .expect("Found external image, but no handler set!");
                                match handler.lock(id, channel_index).source {
                                    ExternalImageSource::RawData(data) => {
                                        uploader.upload(
                                            rect, layer_index, stride,
                                            &data[offset as usize ..],
                                        );
                                    }
                                    ExternalImageSource::Invalid => {
                                        // Create a local buffer to fill the pbo.
                                        let bpp = texture.get_format().bytes_per_pixel();
                                        let width = stride.unwrap_or(rect.size.width * bpp);
                                        let total_size = width * rect.size.height;
                                        // WR haven't support RGBAF32 format in texture_cache, so
                                        // we use u8 type here.
                                        let dummy_data: Vec<u8> = vec![255; total_size as usize];
                                        uploader.upload(rect, layer_index, stride, &dummy_data);
                                    }
                                    _ => panic!("No external buffer found"),
                                };
                                handler.unlock(id, channel_index);
                            }
                        }
                    }
                    TextureUpdateOp::Free => {
                        let texture = &mut self.texture_resolver.cache_texture_map[update.id.0];
                        self.device.free_texture_storage(texture);
                    }
                }
            }
        }
    }

    fn draw_instanced_batch<T>(
        &mut self,
        data: &[T],
        vertex_array_kind: VertexArrayKind,
        textures: &BatchTextures,
        stats: &mut RendererStats,
    ) {
        for i in 0 .. textures.colors.len() {
            self.texture_resolver.bind(
                &textures.colors[i],
                TextureSampler::color(i),
                &mut self.device,
            );
        }

        // TODO: this probably isn't the best place for this.
        if let Some(ref texture) = self.dither_matrix_texture {
            self.device.bind_texture(TextureSampler::Dither, texture);
        }

        let vao = match vertex_array_kind {
            VertexArrayKind::Primitive => &self.prim_vao,
            VertexArrayKind::Clip => &self.clip_vao,
            VertexArrayKind::Blur => &self.blur_vao,
        };

        self.device.bind_vao(vao);

        let batched = !self.debug_flags.contains(DebugFlags::DISABLE_BATCHING);

        if batched {
            self.device
                .update_vao_instances(vao, data, VertexUsageHint::Stream);
            self.device
                .draw_indexed_triangles_instanced_u16(6, data.len() as i32);
            self.profile_counters.draw_calls.inc();
            stats.total_draw_calls += 1;
        } else {
            for i in 0 .. data.len() {
                self.device
                    .update_vao_instances(vao, &data[i .. i + 1], VertexUsageHint::Stream);
                self.device.draw_triangles_u16(0, 6);
                self.profile_counters.draw_calls.inc();
                stats.total_draw_calls += 1;
            }
        }

        self.profile_counters.vertices.add(6 * data.len());
    }

    fn submit_batch(
        &mut self,
        key: &BatchKey,
        instances: &[PrimitiveInstance],
        projection: &Transform3D<f32>,
        render_tasks: &RenderTaskTree,
        render_target: Option<(&Texture, i32)>,
        framebuffer_size: DeviceUintSize,
        stats: &mut RendererStats,
    ) {
        match key.kind {
            BatchKind::Composite { .. } => {
                self.ps_composite.bind(&mut self.device, projection, 0, &mut self.renderer_errors);
            }
            BatchKind::HardwareComposite => {
                self.ps_hw_composite
                    .bind(&mut self.device, projection, 0, &mut self.renderer_errors);
            }
            BatchKind::SplitComposite => {
                self.ps_split_composite.bind(
                    &mut self.device,
                    projection,
                    0,
                    &mut self.renderer_errors,
                );
            }
            BatchKind::Blend => {
                self.ps_blend.bind(&mut self.device, projection, 0, &mut self.renderer_errors);
            }
            BatchKind::Brush(brush_kind) => {
                match brush_kind {
                    BrushBatchKind::Solid => {
                        self.brush_solid.bind(
                            &mut self.device,
                            key.blend_mode,
                            projection,
                            0,
                            &mut self.renderer_errors,
                        );
                    }
                    BrushBatchKind::Picture(target_kind) => {
                        let shader = match target_kind {
                            BrushImageSourceKind::Alpha => &mut self.brush_picture_a8,
                            BrushImageSourceKind::Color => &mut self.brush_picture_rgba8,
                            BrushImageSourceKind::ColorAlphaMask => &mut self.brush_picture_rgba8_alpha_mask,
                        };
                        shader.bind(
                            &mut self.device,
                            key.blend_mode,
                            projection,
                            0,
                            &mut self.renderer_errors,
                        );
                    }
                    BrushBatchKind::Line => {
                        self.brush_line.bind(
                            &mut self.device,
                            key.blend_mode,
                            projection,
                            0,
                            &mut self.renderer_errors,
                        );
                    }
                }
            }
            BatchKind::Transformable(transform_kind, batch_kind) => match batch_kind {
                TransformBatchKind::TextRun(..) => {
                    unreachable!("bug: text batches are special cased");
                }
                TransformBatchKind::Image(image_buffer_kind) => {
                    self.ps_image[image_buffer_kind as usize]
                        .as_mut()
                        .expect("Unsupported image shader kind")
                        .bind(
                            &mut self.device,
                            transform_kind,
                            projection,
                            0,
                            &mut self.renderer_errors,
                        );
                }
                TransformBatchKind::YuvImage(image_buffer_kind, format, color_space) => {
                    let shader_index =
                        Renderer::get_yuv_shader_index(image_buffer_kind, format, color_space);
                    self.ps_yuv_image[shader_index]
                        .as_mut()
                        .expect("Unsupported YUV shader kind")
                        .bind(
                            &mut self.device,
                            transform_kind,
                            projection,
                            0,
                            &mut self.renderer_errors,
                        );
                }
                TransformBatchKind::BorderCorner => {
                    self.ps_border_corner.bind(
                        &mut self.device,
                        transform_kind,
                        projection,
                        0,
                        &mut self.renderer_errors,
                    );
                }
                TransformBatchKind::BorderEdge => {
                    self.ps_border_edge.bind(
                        &mut self.device,
                        transform_kind,
                        projection,
                        0,
                        &mut self.renderer_errors,
                    );
                }
                TransformBatchKind::AlignedGradient => {
                    self.ps_gradient.bind(
                        &mut self.device,
                        transform_kind,
                        projection,
                        0,
                        &mut self.renderer_errors,
                    );
                }
                TransformBatchKind::AngleGradient => {
                    self.ps_angle_gradient.bind(
                        &mut self.device,
                        transform_kind,
                        projection,
                        0,
                        &mut self.renderer_errors,
                    );
                }
                TransformBatchKind::RadialGradient => {
                    self.ps_radial_gradient.bind(
                        &mut self.device,
                        transform_kind,
                        projection,
                        0,
                        &mut self.renderer_errors,
                    );
                }
            },
        };

        // Handle special case readback for composites.
        if let BatchKind::Composite { task_id, source_id, backdrop_id } = key.kind {
            // composites can't be grouped together because
            // they may overlap and affect each other.
            debug_assert_eq!(instances.len(), 1);
            let cache_texture = self.texture_resolver
                .resolve(&SourceTexture::CacheRGBA8)
                .unwrap();

            // Before submitting the composite batch, do the
            // framebuffer readbacks that are needed for each
            // composite operation in this batch.
            let source = &render_tasks[source_id];
            let backdrop = &render_tasks[task_id];
            let readback = &render_tasks[backdrop_id];

            let (readback_rect, readback_layer) = readback.get_target_rect();
            let (backdrop_rect, _) = backdrop.get_target_rect();
            let backdrop_screen_origin = match backdrop.kind {
                RenderTaskKind::Picture(ref task_info) => match task_info.content_origin {
                    ContentOrigin::Local(_) => panic!("bug: composite from a local-space rasterized picture?"),
                    ContentOrigin::Screen(p) => p,
                },
                _ => panic!("bug: composite on non-picture?"),
            };
            let source_screen_origin = match source.kind {
                RenderTaskKind::Picture(ref task_info) => match task_info.content_origin {
                    ContentOrigin::Local(_) => panic!("bug: composite from a local-space rasterized picture?"),
                    ContentOrigin::Screen(p) => p,
                },
                _ => panic!("bug: composite on non-picture?"),
            };

            // Bind the FBO to blit the backdrop to.
            // Called per-instance in case the layer (and therefore FBO)
            // changes. The device will skip the GL call if the requested
            // target is already bound.
            let cache_draw_target = (cache_texture, readback_layer.0 as i32);
            self.device.bind_draw_target(Some(cache_draw_target), None);

            let mut src = DeviceIntRect::new(
                source_screen_origin + (backdrop_rect.origin - backdrop_screen_origin),
                readback_rect.size,
            );
            let mut dest = readback_rect.to_i32();

            // Need to invert the y coordinates and flip the image vertically when
            // reading back from the framebuffer.
            if render_target.is_none() {
                src.origin.y = framebuffer_size.height as i32 - src.size.height - src.origin.y;
                dest.origin.y += dest.size.height;
                dest.size.height = -dest.size.height;
            }

            self.device.bind_read_target(render_target);
            self.device.blit_render_target(src, dest);

            // Restore draw target to current pass render target + layer.
            // Note: leaving the viewport unchanged, it's not a part of FBO state
            self.device.bind_draw_target(render_target, None);
        }

        let _timer = self.gpu_profile.start_timer(key.kind.gpu_sampler_tag());
        self.draw_instanced_batch(
            instances,
            VertexArrayKind::Primitive,
            &key.textures,
            stats
        );
    }

    fn handle_scaling(
        &mut self,
        render_tasks: &RenderTaskTree,
        scalings: &Vec<ScalingInfo>,
        source: SourceTexture,
    ) {
        let cache_texture = self.texture_resolver
            .resolve(&source)
            .unwrap();
        for scaling in scalings {
            let source = &render_tasks[scaling.src_task_id];
            let dest = &render_tasks[scaling.dest_task_id];

            let (source_rect, source_layer) = source.get_target_rect();
            let (dest_rect, _) = dest.get_target_rect();

            let cache_draw_target = (cache_texture, source_layer.0 as i32);
            self.device
                .bind_read_target(Some(cache_draw_target));

            self.device.blit_render_target(source_rect, dest_rect);
        }
    }

    fn draw_color_target(
        &mut self,
        render_target: Option<(&Texture, i32)>,
        target: &ColorRenderTarget,
        framebuffer_target_rect: DeviceUintRect,
        target_size: DeviceUintSize,
        depth_is_ready: bool,
        clear_color: Option<[f32; 4]>,
        render_tasks: &RenderTaskTree,
        projection: &Transform3D<f32>,
        frame_id: FrameId,
        stats: &mut RendererStats,
    ) {
        self.profile_counters.color_targets.inc();
        let _gm = self.gpu_profile.start_marker("color target");

        // sanity check for the depth buffer
        if let Some((texture, _)) = render_target {
            assert!(texture.has_depth() >= target.needs_depth());
        }

        {
            let _timer = self.gpu_profile.start_timer(GPU_TAG_SETUP_TARGET);
            self.device
                .bind_draw_target(render_target, Some(target_size));
            self.device.disable_depth();
            self.device.set_blend(false);

            let depth_clear = if !depth_is_ready && target.needs_depth() {
                self.device.enable_depth_write();
                Some(1.0)
            } else {
                None
            };

            let clear_rect = if render_target.is_some() {
                if self.enable_clear_scissor {
                    // TODO(gw): Applying a scissor rect and minimal clear here
                    // is a very large performance win on the Intel and nVidia
                    // GPUs that I have tested with. It's possible it may be a
                    // performance penalty on other GPU types - we should test this
                    // and consider different code paths.
                    Some(target.used_rect())
                } else {
                    None
                }
            } else if framebuffer_target_rect == DeviceUintRect::new(DeviceUintPoint::zero(), target_size) {
                // whole screen is covered, no need for scissor
                None
            } else {
                let mut rect = framebuffer_target_rect.to_i32();
                // Note: `framebuffer_target_rect` needs a Y-flip before going to GL
                // Note: at this point, the target rectangle is not guaranteed to be within the main framebuffer bounds
                // but `clear_target_rect` is totally fine with negative origin, as long as width & height are positive
                rect.origin.y = target_size.height as i32 - rect.origin.y - rect.size.height;
                Some(rect)
            };

            self.device.clear_target(clear_color, depth_clear, clear_rect);

            if depth_clear.is_some() {
                self.device.disable_depth_write();
            }
        }

        // Draw any blurs for this target.
        // Blurs are rendered as a standard 2-pass
        // separable implementation.
        // TODO(gw): In the future, consider having
        //           fast path blur shaders for common
        //           blur radii with fixed weights.
        if !target.vertical_blurs.is_empty() || !target.horizontal_blurs.is_empty() {
            let _timer = self.gpu_profile.start_timer(GPU_TAG_BLUR);

            self.device.set_blend(false);
            self.cs_blur_rgba8
                .bind(&mut self.device, projection, 0, &mut self.renderer_errors);

            if !target.vertical_blurs.is_empty() {
                self.draw_instanced_batch(
                    &target.vertical_blurs,
                    VertexArrayKind::Blur,
                    &BatchTextures::no_texture(),
                    stats,
                );
            }

            if !target.horizontal_blurs.is_empty() {
                self.draw_instanced_batch(
                    &target.horizontal_blurs,
                    VertexArrayKind::Blur,
                    &BatchTextures::no_texture(),
                    stats,
                );
            }
        }

        self.handle_scaling(render_tasks, &target.scalings, SourceTexture::CacheRGBA8);

        // Draw any textrun caches for this target. For now, this
        // is only used to cache text runs that are to be blurred
        // for shadow support. In the future it may be worth
        // considering using this for (some) other text runs, since
        // it removes the overhead of submitting many small glyphs
        // to multiple tiles in the normal text run case.
        if !target.alpha_batcher.text_run_cache_prims.is_empty() {
            self.device.set_blend(true);
            self.device.set_blend_mode_premultiplied_alpha();

            let _timer = self.gpu_profile.start_timer(GPU_TAG_CACHE_TEXT_RUN);
            self.cs_text_run
                .bind(&mut self.device, projection, 0, &mut self.renderer_errors);
            for (texture_id, instances) in &target.alpha_batcher.text_run_cache_prims {
                self.draw_instanced_batch(
                    instances,
                    VertexArrayKind::Primitive,
                    &BatchTextures::color(*texture_id),
                    stats,
                );
            }
        }

        //TODO: record the pixel count for cached primitives

        if !target.alpha_batcher.is_empty() {
            let _gl = self.gpu_profile.start_marker("alpha batches");
            self.device.set_blend(false);
            let mut prev_blend_mode = BlendMode::None;

            if target.needs_depth() {
                let opaque_sampler = self.gpu_profile.start_sampler(GPU_SAMPLER_TAG_OPAQUE);

                //Note: depth equality is needed for split planes
                self.device.set_depth_func(DepthFunction::LessEqual);
                self.device.enable_depth();
                self.device.enable_depth_write();

                // Draw opaque batches front-to-back for maximum
                // z-buffer efficiency!
                for batch in target
                    .alpha_batcher
                    .batch_list
                    .opaque_batch_list
                    .batches
                    .iter()
                    .rev()
                {
                    self.submit_batch(
                        &batch.key,
                        &batch.instances,
                        &projection,
                        render_tasks,
                        render_target,
                        target_size,
                        stats,
                    );
                }

                self.device.disable_depth_write();
                self.gpu_profile.finish_sampler(opaque_sampler);
            }

            let transparent_sampler = self.gpu_profile.start_sampler(GPU_SAMPLER_TAG_TRANSPARENT);

            for batch in &target.alpha_batcher.batch_list.alpha_batch_list.batches {
                if self.debug_flags.contains(DebugFlags::ALPHA_PRIM_DBG) {
                    let color = match batch.key.blend_mode {
                        BlendMode::None => debug_colors::BLACK,
                        BlendMode::Alpha |
                        BlendMode::PremultipliedAlpha => debug_colors::GREY,
                        BlendMode::PremultipliedDestOut => debug_colors::SALMON,
                        BlendMode::SubpixelConstantTextColor(..) => debug_colors::GREEN,
                        BlendMode::SubpixelVariableTextColor => debug_colors::RED,
                        BlendMode::SubpixelWithBgColor => debug_colors::BLUE,
                        BlendMode::SubpixelDualSource => debug_colors::YELLOW,
                    }.into();
                    for item_rect in &batch.item_rects {
                        self.debug.add_rect(item_rect, color);
                    }
                }

                match batch.key.kind {
                    BatchKind::Transformable(transform_kind, TransformBatchKind::TextRun(glyph_format)) => {
                        // Text run batches are handled by this special case branch.
                        // In the case of subpixel text, we draw it as a two pass
                        // effect, to ensure we can apply clip masks correctly.
                        // In the future, there are several optimizations available:
                        // 1) Use dual source blending where available (almost all recent hardware).
                        // 2) Use frame buffer fetch where available (most modern hardware).
                        // 3) Consider the old constant color blend method where no clip is applied.
                        let _timer = self.gpu_profile.start_timer(GPU_TAG_PRIM_TEXT_RUN);

                        self.device.set_blend(true);

                        match batch.key.blend_mode {
                            BlendMode::Alpha => panic!("Attempt to composite non-premultiplied text primitives."),
                            BlendMode::PremultipliedAlpha => {
                                self.device.set_blend_mode_premultiplied_alpha();

                                self.ps_text_run.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::from(glyph_format),
                                    &mut self.renderer_errors,
                                );

                                self.draw_instanced_batch(
                                    &batch.instances,
                                    VertexArrayKind::Primitive,
                                    &batch.key.textures,
                                    stats,
                                );
                            }
                            BlendMode::SubpixelDualSource => {
                                self.device.set_blend_mode_subpixel_dual_source();

                                self.ps_text_run_dual_source.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::SubpixelDualSource,
                                    &mut self.renderer_errors,
                                );

                                self.draw_instanced_batch(
                                    &batch.instances,
                                    VertexArrayKind::Primitive,
                                    &batch.key.textures,
                                    stats,
                                );
                            }
                            BlendMode::SubpixelConstantTextColor(color) => {
                                self.device.set_blend_mode_subpixel_constant_text_color(color);

                                self.ps_text_run.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::SubpixelConstantTextColor,
                                    &mut self.renderer_errors,
                                );

                                self.draw_instanced_batch(
                                    &batch.instances,
                                    VertexArrayKind::Primitive,
                                    &batch.key.textures,
                                    stats,
                                );
                            }
                            BlendMode::SubpixelVariableTextColor => {
                                // Using the two pass component alpha rendering technique:
                                //
                                // http://anholt.livejournal.com/32058.html
                                //
                                self.device.set_blend_mode_subpixel_pass0();

                                self.ps_text_run.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::SubpixelPass0,
                                    &mut self.renderer_errors,
                                );

                                self.draw_instanced_batch(
                                    &batch.instances,
                                    VertexArrayKind::Primitive,
                                    &batch.key.textures,
                                    stats,
                                );

                                self.device.set_blend_mode_subpixel_pass1();

                                self.ps_text_run.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::SubpixelPass1,
                                    &mut self.renderer_errors,
                                );

                                // When drawing the 2nd pass, we know that the VAO, textures etc
                                // are all set up from the previous draw_instanced_batch call,
                                // so just issue a draw call here to avoid re-uploading the
                                // instances and re-binding textures etc.
                                self.device
                                    .draw_indexed_triangles_instanced_u16(6, batch.instances.len() as i32);
                            }
                            BlendMode::SubpixelWithBgColor => {
                                // Using the three pass "component alpha with font smoothing
                                // background color" rendering technique:
                                //
                                // /webrender/doc/text-rendering.md
                                //
                                self.device.set_blend_mode_subpixel_with_bg_color_pass0();

                                self.ps_text_run.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::SubpixelWithBgColorPass0,
                                    &mut self.renderer_errors,
                                );

                                self.draw_instanced_batch(
                                    &batch.instances,
                                    VertexArrayKind::Primitive,
                                    &batch.key.textures,
                                    stats,
                                );

                                self.device.set_blend_mode_subpixel_with_bg_color_pass1();

                                self.ps_text_run.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::SubpixelWithBgColorPass1,
                                    &mut self.renderer_errors,
                                );

                                // When drawing the 2nd and 3rd passes, we know that the VAO, textures etc
                                // are all set up from the previous draw_instanced_batch call,
                                // so just issue a draw call here to avoid re-uploading the
                                // instances and re-binding textures etc.
                                self.device
                                    .draw_indexed_triangles_instanced_u16(6, batch.instances.len() as i32);

                                self.device.set_blend_mode_subpixel_with_bg_color_pass2();

                                self.ps_text_run.bind(
                                    &mut self.device,
                                    glyph_format,
                                    transform_kind,
                                    projection,
                                    TextShaderMode::SubpixelWithBgColorPass2,
                                    &mut self.renderer_errors,
                                );

                                self.device
                                    .draw_indexed_triangles_instanced_u16(6, batch.instances.len() as i32);
                            }
                            BlendMode::PremultipliedDestOut | BlendMode::None => {
                                unreachable!("bug: bad blend mode for text");
                            }
                        }

                        prev_blend_mode = BlendMode::None;
                        self.device.set_blend(false);
                    }
                    _ => {
                        if batch.key.blend_mode != prev_blend_mode {
                            match batch.key.blend_mode {
                                BlendMode::None => {
                                    self.device.set_blend(false);
                                }
                                BlendMode::Alpha => {
                                    self.device.set_blend(true);
                                    self.device.set_blend_mode_alpha();
                                }
                                BlendMode::PremultipliedAlpha => {
                                    self.device.set_blend(true);
                                    self.device.set_blend_mode_premultiplied_alpha();
                                }
                                BlendMode::PremultipliedDestOut => {
                                    self.device.set_blend(true);
                                    self.device.set_blend_mode_premultiplied_dest_out();
                                }
                                BlendMode::SubpixelConstantTextColor(..) |
                                BlendMode::SubpixelVariableTextColor |
                                BlendMode::SubpixelWithBgColor |
                                BlendMode::SubpixelDualSource => {
                                    unreachable!("bug: subpx text handled earlier");
                                }
                            }
                            prev_blend_mode = batch.key.blend_mode;
                        }

                        self.submit_batch(
                            &batch.key,
                            &batch.instances,
                            &projection,
                            render_tasks,
                            render_target,
                            target_size,
                            stats,
                        );
                    }
                }
            }

            self.device.disable_depth();
            self.device.set_blend(false);
            self.gpu_profile.finish_sampler(transparent_sampler);
        }

        // For any registered image outputs on this render target,
        // get the texture from caller and blit it.
        for output in &target.outputs {
            let handler = self.output_image_handler
                .as_mut()
                .expect("Found output image, but no handler set!");
            if let Some((texture_id, output_size)) = handler.lock(output.pipeline_id) {
                let fbo_id = match self.output_targets.entry(texture_id) {
                    Entry::Vacant(entry) => {
                        let fbo_id = self.device.create_fbo_for_external_texture(texture_id);
                        entry.insert(FrameOutput {
                            fbo_id,
                            last_access: frame_id,
                        });
                        fbo_id
                    }
                    Entry::Occupied(mut entry) => {
                        let target = entry.get_mut();
                        target.last_access = frame_id;
                        target.fbo_id
                    }
                };
                let (src_rect, _) = render_tasks[output.task_id].get_target_rect();
                let dest_rect = DeviceIntRect::new(DeviceIntPoint::zero(), output_size);
                self.device.bind_read_target(render_target);
                self.device.bind_external_draw_target(fbo_id);
                self.device.blit_render_target(src_rect, dest_rect);
                handler.unlock(output.pipeline_id);
            }
        }
    }

    fn draw_alpha_target(
        &mut self,
        render_target: (&Texture, i32),
        target: &AlphaRenderTarget,
        target_size: DeviceUintSize,
        projection: &Transform3D<f32>,
        render_tasks: &RenderTaskTree,
        stats: &mut RendererStats,
    ) {
        self.profile_counters.alpha_targets.inc();
        let _gm = self.gpu_profile.start_marker("alpha target");
        let alpha_sampler = self.gpu_profile.start_sampler(GPU_SAMPLER_TAG_ALPHA);

        {
            let _timer = self.gpu_profile.start_timer(GPU_TAG_SETUP_TARGET);
            self.device
                .bind_draw_target(Some(render_target), Some(target_size));
            self.device.disable_depth();
            self.device.disable_depth_write();

            // TODO(gw): Applying a scissor rect and minimal clear here
            // is a very large performance win on the Intel and nVidia
            // GPUs that I have tested with. It's possible it may be a
            // performance penalty on other GPU types - we should test this
            // and consider different code paths.
            let clear_color = [1.0, 1.0, 1.0, 0.0];
            self.device.clear_target(
                Some(clear_color),
                None,
                Some(target.used_rect()),
            );

            let zero_color = [0.0, 0.0, 0.0, 0.0];
            for &task_id in &target.zero_clears {
                let (rect, _) = render_tasks[task_id].get_target_rect();
                self.device.clear_target(
                    Some(zero_color),
                    None,
                    Some(rect),
                );
            }
        }

        // Draw any blurs for this target.
        // Blurs are rendered as a standard 2-pass
        // separable implementation.
        // TODO(gw): In the future, consider having
        //           fast path blur shaders for common
        //           blur radii with fixed weights.
        if !target.vertical_blurs.is_empty() || !target.horizontal_blurs.is_empty() {
            let _timer = self.gpu_profile.start_timer(GPU_TAG_BLUR);

            self.device.set_blend(false);
            self.cs_blur_a8
                .bind(&mut self.device, projection, 0, &mut self.renderer_errors);

            if !target.vertical_blurs.is_empty() {
                self.draw_instanced_batch(
                    &target.vertical_blurs,
                    VertexArrayKind::Blur,
                    &BatchTextures::no_texture(),
                    stats,
                );
            }

            if !target.horizontal_blurs.is_empty() {
                self.draw_instanced_batch(
                    &target.horizontal_blurs,
                    VertexArrayKind::Blur,
                    &BatchTextures::no_texture(),
                    stats,
                );
            }
        }

        self.handle_scaling(render_tasks, &target.scalings, SourceTexture::CacheA8);

        if !target.brush_mask_corners.is_empty() {
            self.device.set_blend(false);

            let _timer = self.gpu_profile.start_timer(GPU_TAG_BRUSH_MASK);
            self.brush_mask_corner
                .bind(&mut self.device, projection, 0, &mut self.renderer_errors);
            self.draw_instanced_batch(
                &target.brush_mask_corners,
                VertexArrayKind::Primitive,
                &BatchTextures::no_texture(),
                stats,
            );
        }

        if !target.brush_mask_rounded_rects.is_empty() {
            self.device.set_blend(false);

            let _timer = self.gpu_profile.start_timer(GPU_TAG_BRUSH_MASK);
            self.brush_mask_rounded_rect
                .bind(&mut self.device, projection, 0, &mut self.renderer_errors);
            self.draw_instanced_batch(
                &target.brush_mask_rounded_rects,
                VertexArrayKind::Primitive,
                &BatchTextures::no_texture(),
                stats,
            );
        }

        // Draw the clip items into the tiled alpha mask.
        {
            let _timer = self.gpu_profile.start_timer(GPU_TAG_CACHE_CLIP);

            // If we have border corner clips, the first step is to clear out the
            // area in the clip mask. This allows drawing multiple invididual clip
            // in regions below.
            if !target.clip_batcher.border_clears.is_empty() {
                let _gm2 = self.gpu_profile.start_marker("clip borders [clear]");
                self.device.set_blend(false);
                self.cs_clip_border
                    .bind(&mut self.device, projection, 0, &mut self.renderer_errors);
                self.draw_instanced_batch(
                    &target.clip_batcher.border_clears,
                    VertexArrayKind::Clip,
                    &BatchTextures::no_texture(),
                    stats,
                );
            }

            // Draw any dots or dashes for border corners.
            if !target.clip_batcher.borders.is_empty() {
                let _gm2 = self.gpu_profile.start_marker("clip borders");
                // We are masking in parts of the corner (dots or dashes) here.
                // Blend mode is set to max to allow drawing multiple dots.
                // The individual dots and dashes in a border never overlap, so using
                // a max blend mode here is fine.
                self.device.set_blend(true);
                self.device.set_blend_mode_max();
                self.cs_clip_border
                    .bind(&mut self.device, projection, 0, &mut self.renderer_errors);
                self.draw_instanced_batch(
                    &target.clip_batcher.borders,
                    VertexArrayKind::Clip,
                    &BatchTextures::no_texture(),
                    stats,
                );
            }

            // switch to multiplicative blending
            self.device.set_blend(true);
            self.device.set_blend_mode_multiply();

            // draw rounded cornered rectangles
            if !target.clip_batcher.rectangles.is_empty() {
                let _gm2 = self.gpu_profile.start_marker("clip rectangles");
                self.cs_clip_rectangle.bind(
                    &mut self.device,
                    projection,
                    0,
                    &mut self.renderer_errors,
                );
                self.draw_instanced_batch(
                    &target.clip_batcher.rectangles,
                    VertexArrayKind::Clip,
                    &BatchTextures::no_texture(),
                    stats,
                );
            }
            // draw image masks
            for (mask_texture_id, items) in target.clip_batcher.images.iter() {
                let _gm2 = self.gpu_profile.start_marker("clip images");
                let textures = BatchTextures {
                    colors: [
                        mask_texture_id.clone(),
                        SourceTexture::Invalid,
                        SourceTexture::Invalid,
                    ],
                };
                self.cs_clip_image
                    .bind(&mut self.device, projection, 0, &mut self.renderer_errors);
                self.draw_instanced_batch(
                    items,
                    VertexArrayKind::Clip,
                    &textures,
                    stats,
                );
            }
        }

        self.gpu_profile.finish_sampler(alpha_sampler);
    }

    fn draw_texture_cache_target(
        &mut self,
        texture: &SourceTexture,
        layer: i32,
        target: &TextureCacheRenderTarget,
        stats: &mut RendererStats,
    ) {
        let projection = {
            let texture = self.texture_resolver
                .resolve(texture)
                .expect("BUG: invalid target texture");
            let target_size = texture.get_dimensions();

            self.device
                .bind_draw_target(Some((texture, layer)), Some(target_size));
            self.device.disable_depth();
            self.device.disable_depth_write();
            self.device.set_blend(false);

            Transform3D::ortho(
                0.0,
                target_size.width as f32,
                0.0,
                target_size.height as f32,
                ORTHO_NEAR_PLANE,
                ORTHO_FAR_PLANE,
            )
        };

        // Draw any blurs for this target.
        if !target.horizontal_blurs.is_empty() {
            let _timer = self.gpu_profile.start_timer(GPU_TAG_BLUR);

            self.cs_blur_a8
                .bind(&mut self.device, &projection, 0, &mut self.renderer_errors);

            self.draw_instanced_batch(
                &target.horizontal_blurs,
                VertexArrayKind::Blur,
                &BatchTextures::no_texture(),
                stats,
            );
        }
    }

    fn update_deferred_resolves(&mut self, deferred_resolves: &[DeferredResolve]) -> Option<GpuCacheUpdateList> {
        // The first thing we do is run through any pending deferred
        // resolves, and use a callback to get the UV rect for this
        // custom item. Then we patch the resource_rects structure
        // here before it's uploaded to the GPU.
        if deferred_resolves.is_empty() {
            return None;
        }

        let handler = self.external_image_handler
            .as_mut()
            .expect("Found external image, but no handler set!");

        let mut list = GpuCacheUpdateList {
            height: self.gpu_cache_texture.get_height(),
            blocks: Vec::new(),
            updates: Vec::new(),
        };

        for deferred_resolve in deferred_resolves {
            self.gpu_profile.place_marker("deferred resolve");
            let props = &deferred_resolve.image_properties;
            let ext_image = props
                .external_image
                .expect("BUG: Deferred resolves must be external images!");
            let image = handler.lock(ext_image.id, ext_image.channel_index);
            let texture_target = get_external_image_target(ext_image.image_type)
                .expect(&format!("{:?} is not a suitable image type in update_deferred_resolves()", ext_image.image_type));

            // In order to produce the handle, the external image handler may call into
            // the GL context and change some states.
            self.device.reset_state();

            let texture = match image.source {
                ExternalImageSource::NativeTexture(texture_id) => {
                    ExternalTexture::new(texture_id, texture_target)
                }
                ExternalImageSource::Invalid => {
                    warn!(
                        "Invalid ext-image for ext_id:{:?}, channel:{}.",
                        ext_image.id,
                        ext_image.channel_index
                    );
                    // Just use 0 as the gl handle for this failed case.
                    ExternalTexture::new(0, texture_target)
                }
                _ => panic!("No native texture found."),
            };

            self.texture_resolver
                .external_images
                .insert((ext_image.id, ext_image.channel_index), texture);

            list.updates.push(GpuCacheUpdate::Copy {
                block_index: list.blocks.len(),
                block_count: BLOCKS_PER_UV_RECT,
                address: deferred_resolve.address,
            });
            list.blocks.push([image.u0, image.v0, image.u1, image.v1].into());
            list.blocks.push([0f32; 4].into());
        }

        Some(list)
    }

    fn unlock_external_images(&mut self) {
        if !self.texture_resolver.external_images.is_empty() {
            let handler = self.external_image_handler
                .as_mut()
                .expect("Found external image, but no handler set!");

            for (ext_data, _) in self.texture_resolver.external_images.drain() {
                handler.unlock(ext_data.0, ext_data.1);
            }
        }
    }

    fn prepare_target_list<T: RenderTarget>(
        &mut self,
        list: &mut RenderTargetList<T>,
        perfect_only: bool,
    ) {
        debug_assert_ne!(list.max_size, DeviceUintSize::zero());
        if list.targets.is_empty() {
            return;
        }
        let mut texture = if perfect_only {
            debug_assert!(list.texture.is_none());

            let selector = TargetSelector {
                size: list.max_size,
                num_layers: list.targets.len() as _,
                format: list.format,
            };
            let index = self.texture_resolver.render_target_pool
                .iter()
                .position(|texture| {
                    selector == TargetSelector {
                        size: texture.get_dimensions(),
                        num_layers: texture.get_render_target_layer_count(),
                        format: texture.get_format(),
                    }
                });
            match index {
                Some(pos) => self.texture_resolver.render_target_pool.swap_remove(pos),
                None => return,
            }
        } else {
            if list.texture.is_some() {
                list.check_ready();
                return
            }
            let index = self.texture_resolver.render_target_pool
                .iter()
                .position(|texture| texture.get_format() == list.format);
            match index {
                Some(pos) => self.texture_resolver.render_target_pool.swap_remove(pos),
                None => self.device.create_texture(TextureTarget::Array, list.format),
            }
        };

        self.device.init_texture(
            &mut texture,
            list.max_size.width,
            list.max_size.height,
            TextureFilter::Linear,
            Some(RenderTargetInfo {
                has_depth: list.needs_depth(),
            }),
            list.targets.len() as _,
            None,
        );
        list.texture = Some(texture);
        list.check_ready();
    }

    fn prepare_tile_frame(&mut self, frame: &mut Frame) {
        // Init textures and render targets to match this scene.
        // First pass grabs all the perfectly matching targets from the pool.
        for pass in &mut frame.passes {
            if let RenderPassKind::OffScreen { ref mut alpha, ref mut color, .. } = pass.kind {
                self.prepare_target_list(alpha, true);
                self.prepare_target_list(color, true);
            }
        }
    }

    fn bind_frame_data(&mut self, frame: &mut Frame) {
        let _timer = self.gpu_profile.start_timer(GPU_TAG_SETUP_DATA);
        self.device.set_device_pixel_ratio(frame.device_pixel_ratio);

        // Some of the textures are already assigned by `prepare_frame`.
        // Now re-allocate the space for the rest of the target textures.
        for pass in &mut frame.passes {
            if let RenderPassKind::OffScreen { ref mut alpha, ref mut color, .. } = pass.kind {
                self.prepare_target_list(alpha, false);
                self.prepare_target_list(color, false);
            }
        }

        self.node_data_texture.update(&mut self.device, &mut frame.node_data);
        self.device.bind_texture(TextureSampler::ClipScrollNodes, &self.node_data_texture.texture);

        self.local_clip_rects_texture.update(
            &mut self.device,
            &mut frame.clip_chain_local_clip_rects
        );
        self.device.bind_texture(
            TextureSampler::LocalClipRects,
            &self.local_clip_rects_texture.texture
        );

        self.render_task_texture
            .update(&mut self.device, &mut frame.render_tasks.task_data);
        self.device.bind_texture(
            TextureSampler::RenderTasks,
            &self.render_task_texture.texture,
        );

        debug_assert!(self.texture_resolver.cache_a8_texture.is_none());
        debug_assert!(self.texture_resolver.cache_rgba8_texture.is_none());
    }

    fn draw_tile_frame(
        &mut self,
        frame: &mut Frame,
        framebuffer_size: Option<DeviceUintSize>,
        framebuffer_depth_is_ready: bool,
        frame_id: FrameId,
        stats: &mut RendererStats,
    ) {
        let _gm = self.gpu_profile.start_marker("tile frame draw");
        frame.has_been_rendered = true;

        if frame.passes.is_empty() {
            return;
        }

        self.device.disable_depth_write();
        self.device.disable_stencil();
        self.device.set_blend(false);

        self.bind_frame_data(frame);
        self.texture_resolver.begin_frame();

        for (pass_index, pass) in frame.passes.iter_mut().enumerate() {
            self.gpu_profile.place_marker(&format!("pass {}", pass_index));

            self.texture_resolver.bind(
                &SourceTexture::CacheA8,
                TextureSampler::CacheA8,
                &mut self.device,
            );
            self.texture_resolver.bind(
                &SourceTexture::CacheRGBA8,
                TextureSampler::CacheRGBA8,
                &mut self.device,
            );

            let (cur_alpha, cur_color) = match pass.kind {
                RenderPassKind::MainFramebuffer(ref target) => {
                    if let Some(framebuffer_size) = framebuffer_size {
                        stats.color_target_count += 1;

                        let clear_color = frame.background_color.map(|color| color.to_array());
                        let projection = Transform3D::ortho(
                            0.0,
                            framebuffer_size.width as f32,
                            framebuffer_size.height as f32,
                            0.0,
                            ORTHO_NEAR_PLANE,
                            ORTHO_FAR_PLANE,
                        );

                        self.draw_color_target(
                            None,
                            target,
                            frame.inner_rect,
                            framebuffer_size,
                            framebuffer_depth_is_ready,
                            clear_color,
                            &frame.render_tasks,
                            &projection,
                            frame_id,
                            stats,
                        );
                    }

                    (None, None)
                }
                RenderPassKind::OffScreen { ref mut alpha, ref mut color, ref mut texture_cache } => {
                    alpha.check_ready();
                    color.check_ready();
                    for (&(texture_id, target_index), target) in texture_cache {
                        self.draw_texture_cache_target(
                            &texture_id,
                            target_index,
                            target,
                            stats,
                        );
                    }

                    for (target_index, target) in alpha.targets.iter().enumerate() {
                        stats.alpha_target_count += 1;

                        let projection = Transform3D::ortho(
                            0.0,
                            alpha.max_size.width as f32,
                            0.0,
                            alpha.max_size.height as f32,
                            ORTHO_NEAR_PLANE,
                            ORTHO_FAR_PLANE,
                        );

                        self.draw_alpha_target(
                            (alpha.texture.as_ref().unwrap(), target_index as i32),
                            target,
                            alpha.max_size,
                            &projection,
                            &frame.render_tasks,
                            stats,
                        );
                    }

                    for (target_index, target) in color.targets.iter().enumerate() {
                        stats.color_target_count += 1;

                        let projection = Transform3D::ortho(
                            0.0,
                            color.max_size.width as f32,
                            0.0,
                            color.max_size.height as f32,
                            ORTHO_NEAR_PLANE,
                            ORTHO_FAR_PLANE,
                        );

                        self.draw_color_target(
                            Some((color.texture.as_ref().unwrap(), target_index as i32)),
                            target,
                            frame.inner_rect,
                            color.max_size,
                            false,
                            Some([0.0, 0.0, 0.0, 0.0]),
                            &frame.render_tasks,
                            &projection,
                            frame_id,
                            stats,
                        );
                    }

                    (alpha.texture.take(), color.texture.take())
                }
            };

            self.texture_resolver.end_pass(
                cur_alpha,
                cur_color,
                RenderPassIndex(pass_index),
            );

            // After completing the first pass, make the A8 target available as an
            // input to any subsequent passes.
            if pass_index == 0 {
                if let Some(shared_alpha_texture) =
                    self.texture_resolver.resolve(&SourceTexture::CacheA8)
                {
                    self.device
                        .bind_texture(TextureSampler::SharedCacheA8, shared_alpha_texture);
                }
            }
        }

        self.texture_resolver.end_frame(RenderPassIndex(frame.passes.len()));
        if let Some(framebuffer_size) = framebuffer_size {
            self.draw_render_target_debug(framebuffer_size);
            self.draw_texture_cache_debug(framebuffer_size);
        }
        self.draw_epoch_debug();

        // Garbage collect any frame outputs that weren't used this frame.
        let device = &mut self.device;
        self.output_targets
            .retain(|_, target| if target.last_access != frame_id {
                device.delete_fbo(target.fbo_id);
                false
            } else {
                true
            });
    }

    pub fn debug_renderer<'b>(&'b mut self) -> &'b mut DebugRenderer {
        &mut self.debug
    }

    pub fn get_debug_flags(&self) -> DebugFlags {
        self.debug_flags
    }

    pub fn set_debug_flags(&mut self, flags: DebugFlags) {
        if let Some(enabled) = flag_changed(self.debug_flags, flags, DebugFlags::GPU_TIME_QUERIES) {
            if enabled {
                self.gpu_profile.enable_timers();
            } else {
                self.gpu_profile.disable_timers();
            }
        }
        if let Some(enabled) = flag_changed(self.debug_flags, flags, DebugFlags::GPU_SAMPLE_QUERIES) {
            if enabled {
                self.gpu_profile.enable_samplers();
            } else {
                self.gpu_profile.disable_samplers();
            }
        }

        self.debug_flags = flags;
    }

    pub fn set_debug_flag(&mut self, flag: DebugFlags, enabled: bool) {
        let mut new_flags = self.debug_flags;
        new_flags.set(flag, enabled);
        self.set_debug_flags(new_flags);
    }

    pub fn toggle_debug_flags(&mut self, toggle: DebugFlags) {
        let mut new_flags = self.debug_flags;
        new_flags.toggle(toggle);
        self.set_debug_flags(new_flags);
    }

    pub fn save_cpu_profile(&self, filename: &str) {
        write_profile(filename);
    }

    fn draw_render_target_debug(&mut self, framebuffer_size: DeviceUintSize) {
        if !self.debug_flags.contains(DebugFlags::RENDER_TARGET_DBG) {
            return;
        }

        let mut spacing = 16;
        let mut size = 512;
        let fb_width = framebuffer_size.width as i32;
        let num_layers: i32 = self.texture_resolver.render_target_pool
            .iter()
            .map(|texture| texture.get_render_target_layer_count() as i32)
            .sum();

        if num_layers * (size + spacing) > fb_width {
            let factor = fb_width as f32 / (num_layers * (size + spacing)) as f32;
            size = (size as f32 * factor) as i32;
            spacing = (spacing as f32 * factor) as i32;
        }

        let mut target_index = 0;
        for texture in &self.texture_resolver.render_target_pool {
            let dimensions = texture.get_dimensions();
            let src_rect = DeviceIntRect::new(DeviceIntPoint::zero(), dimensions.to_i32());

            let layer_count = texture.get_render_target_layer_count();
            for layer_index in 0 .. layer_count {
                self.device
                    .bind_read_target(Some((texture, layer_index as i32)));
                let x = fb_width - (spacing + size) * (target_index + 1);
                let y = spacing;

                let dest_rect = rect(x, y, size, size);
                self.device.blit_render_target(src_rect, dest_rect);
                target_index += 1;
            }
        }
    }

    fn draw_texture_cache_debug(&mut self, framebuffer_size: DeviceUintSize) {
        if !self.debug_flags.contains(DebugFlags::TEXTURE_CACHE_DBG) {
            return;
        }

        let mut spacing = 16;
        let mut size = 512;
        let fb_width = framebuffer_size.width as i32;
        let num_layers: i32 = self.texture_resolver
            .cache_texture_map
            .iter()
            .map(|texture| texture.get_layer_count())
            .sum();

        if num_layers * (size + spacing) > fb_width {
            let factor = fb_width as f32 / (num_layers * (size + spacing)) as f32;
            size = (size as f32 * factor) as i32;
            spacing = (spacing as f32 * factor) as i32;
        }

        let mut i = 0;
        for texture in &self.texture_resolver.cache_texture_map {
            let y = spacing + if self.debug_flags.contains(DebugFlags::RENDER_TARGET_DBG) {
                528
            } else {
                0
            };
            let dimensions = texture.get_dimensions();
            let src_rect = DeviceIntRect::new(
                DeviceIntPoint::zero(),
                DeviceIntSize::new(dimensions.width as i32, dimensions.height as i32),
            );

            let layer_count = texture.get_layer_count();
            for layer_index in 0 .. layer_count {
                self.device.bind_read_target(Some((texture, layer_index)));

                let x = fb_width - (spacing + size) * (i as i32 + 1);

                // If we have more targets than fit on one row in screen, just early exit.
                if x > fb_width {
                    return;
                }

                let dest_rect = rect(x, y, size, size);
                self.device.blit_render_target(src_rect, dest_rect);
                i += 1;
            }
        }
    }

    fn draw_epoch_debug(&mut self) {
        if !self.debug_flags.contains(DebugFlags::EPOCHS) {
            return;
        }

        let dy = self.debug.line_height();
        let x0: f32 = 30.0;
        let y0: f32 = 30.0;
        let mut y = y0;
        let mut text_width = 0.0;
        for (pipeline, epoch) in  &self.pipeline_epoch_map {
            y += dy;
            let w = self.debug.add_text(
                x0, y,
                &format!("{:?}: {:?}", pipeline, epoch),
                ColorU::new(255, 255, 0, 255),
            ).size.width;
            text_width = f32::max(text_width, w);
        }

        let margin = 10.0;
        self.debug.add_quad(
            &x0 - margin,
            y0 - margin,
            x0 + text_width + margin,
            y + margin,
            ColorU::new(25, 25, 25, 200),
            ColorU::new(51, 51, 51, 200),
        );
    }

    /// Pass-through to `Device::read_pixels_into`, used by Gecko's WR bindings.
    pub fn read_pixels_into(&mut self, rect: DeviceUintRect, format: ReadPixelsFormat, output: &mut [u8]) {
        self.device.read_pixels_into(rect, format, output);
    }

    pub fn read_pixels_rgba8(&mut self, rect: DeviceUintRect) -> Vec<u8> {
        let mut pixels = vec![0; (rect.size.width * rect.size.height * 4) as usize];
        self.device.read_pixels_into(rect, ReadPixelsFormat::Rgba8, &mut pixels);
        pixels
    }

    pub fn read_gpu_cache(&mut self) -> (DeviceUintSize, Vec<u8>) {
        let size = self.gpu_cache_texture.texture.get_dimensions();
        let mut texels = vec![0; (size.width * size.height * 16) as usize];
        self.device.begin_frame();
        self.device.bind_read_target(Some((&self.gpu_cache_texture.texture, 0)));
        self.device.read_pixels_into(
            DeviceUintRect::new(DeviceUintPoint::zero(), size),
            ReadPixelsFormat::Standard(ImageFormat::RGBAF32),
            &mut texels,
        );
        self.device.bind_read_target(None);
        self.device.end_frame();
        (size, texels)
    }

    // De-initialize the Renderer safely, assuming the GL is still alive and active.
    pub fn deinit(mut self) {
        //Note: this is a fake frame, only needed because texture deletion is require to happen inside a frame
        self.device.begin_frame();
        self.gpu_cache_texture.deinit(&mut self.device);
        if let Some(dither_matrix_texture) = self.dither_matrix_texture {
            self.device.delete_texture(dither_matrix_texture);
        }
        self.node_data_texture.deinit(&mut self.device);
        self.local_clip_rects_texture.deinit(&mut self.device);
        self.render_task_texture.deinit(&mut self.device);
        self.device.delete_pbo(self.texture_cache_upload_pbo);
        self.texture_resolver.deinit(&mut self.device);
        self.device.delete_vao(self.prim_vao);
        self.device.delete_vao(self.clip_vao);
        self.device.delete_vao(self.blur_vao);
        self.debug.deinit(&mut self.device);
        self.cs_text_run.deinit(&mut self.device);
        self.cs_blur_a8.deinit(&mut self.device);
        self.cs_blur_rgba8.deinit(&mut self.device);
        self.brush_mask_rounded_rect.deinit(&mut self.device);
        self.brush_mask_corner.deinit(&mut self.device);
        self.brush_picture_rgba8.deinit(&mut self.device);
        self.brush_picture_rgba8_alpha_mask.deinit(&mut self.device);
        self.brush_picture_a8.deinit(&mut self.device);
        self.brush_solid.deinit(&mut self.device);
        self.brush_line.deinit(&mut self.device);
        self.cs_clip_rectangle.deinit(&mut self.device);
        self.cs_clip_image.deinit(&mut self.device);
        self.cs_clip_border.deinit(&mut self.device);
        self.ps_text_run.deinit(&mut self.device);
        self.ps_text_run_dual_source.deinit(&mut self.device);
        for shader in self.ps_image {
            if let Some(shader) = shader {
                shader.deinit(&mut self.device);
            }
        }
        for shader in self.ps_yuv_image {
            if let Some(shader) = shader {
                shader.deinit(&mut self.device);
            }
        }
        for (_, target) in self.output_targets {
            self.device.delete_fbo(target.fbo_id);
        }
        self.ps_border_corner.deinit(&mut self.device);
        self.ps_border_edge.deinit(&mut self.device);
        self.ps_gradient.deinit(&mut self.device);
        self.ps_angle_gradient.deinit(&mut self.device);
        self.ps_radial_gradient.deinit(&mut self.device);
        self.ps_blend.deinit(&mut self.device);
        self.ps_hw_composite.deinit(&mut self.device);
        self.ps_split_composite.deinit(&mut self.device);
        self.ps_composite.deinit(&mut self.device);
        #[cfg(feature = "capture")]
        self.device.delete_fbo(self.capture.read_fbo);
        #[cfg(feature = "capture")]
        for (_, ext) in self.capture.owned_external_images {
            self.device.delete_external_texture(ext);
        }
        self.device.end_frame();
    }
}

pub enum ExternalImageSource<'a> {
    RawData(&'a [u8]),  // raw buffers.
    NativeTexture(u32), // It's a gl::GLuint texture handle
    Invalid,
}

/// The data that an external client should provide about
/// an external image. The timestamp is used to test if
/// the renderer should upload new texture data this
/// frame. For instance, if providing video frames, the
/// application could call wr.render() whenever a new
/// video frame is ready. If the callback increments
/// the returned timestamp for a given image, the renderer
/// will know to re-upload the image data to the GPU.
/// Note that the UV coords are supplied in texel-space!
pub struct ExternalImage<'a> {
    pub u0: f32,
    pub v0: f32,
    pub u1: f32,
    pub v1: f32,
    pub source: ExternalImageSource<'a>,
}

/// The interfaces that an application can implement to support providing
/// external image buffers.
/// When the the application passes an external image to WR, it should kepp that
/// external image life time. People could check the epoch id in RenderNotifier
/// at the client side to make sure that the external image is not used by WR.
/// Then, do the clean up for that external image.
pub trait ExternalImageHandler {
    /// Lock the external image. Then, WR could start to read the image content.
    /// The WR client should not change the image content until the unlock()
    /// call.
    fn lock(&mut self, key: ExternalImageId, channel_index: u8) -> ExternalImage;
    /// Unlock the external image. The WR should not read the image content
    /// after this call.
    fn unlock(&mut self, key: ExternalImageId, channel_index: u8);
}

/// Allows callers to receive a texture with the contents of a specific
/// pipeline copied to it. Lock should return the native texture handle
/// and the size of the texture. Unlock will only be called if the lock()
/// call succeeds, when WR has issued the GL commands to copy the output
/// to the texture handle.
pub trait OutputImageHandler {
    fn lock(&mut self, pipeline_id: PipelineId) -> Option<(u32, DeviceIntSize)>;
    fn unlock(&mut self, pipeline_id: PipelineId);
}

pub trait ThreadListener {
    fn thread_started(&self, thread_name: &str);
    fn thread_stopped(&self, thread_name: &str);
}

pub struct RendererOptions {
    pub device_pixel_ratio: f32,
    pub resource_override_path: Option<PathBuf>,
    pub enable_aa: bool,
    pub enable_dithering: bool,
    pub max_recorded_profiles: usize,
    pub debug: bool,
    pub enable_scrollbars: bool,
    pub precache_shaders: bool,
    pub renderer_kind: RendererKind,
    pub enable_subpixel_aa: bool,
    pub clear_color: Option<ColorF>,
    pub enable_clear_scissor: bool,
    pub max_texture_size: Option<u32>,
    pub scatter_gpu_cache_updates: bool,
    pub upload_method: UploadMethod,
    pub workers: Option<Arc<ThreadPool>>,
    pub blob_image_renderer: Option<Box<BlobImageRenderer>>,
    pub recorder: Option<Box<ApiRecordingReceiver>>,
    pub thread_listener: Option<Box<ThreadListener + Send + Sync>>,
    pub enable_render_on_scroll: bool,
    pub cached_programs: Option<Rc<ProgramCache>>,
    pub debug_flags: DebugFlags,
    pub renderer_id: Option<u64>,
    pub disable_dual_source_blending: bool,
}

impl Default for RendererOptions {
    fn default() -> Self {
        RendererOptions {
            device_pixel_ratio: 1.0,
            resource_override_path: None,
            enable_aa: true,
            enable_dithering: true,
            debug_flags: DebugFlags::empty(),
            max_recorded_profiles: 0,
            debug: false,
            enable_scrollbars: false,
            precache_shaders: false,
            renderer_kind: RendererKind::Native,
            enable_subpixel_aa: false,
            clear_color: Some(ColorF::new(1.0, 1.0, 1.0, 1.0)),
            enable_clear_scissor: true,
            max_texture_size: None,
            // Scattered GPU cache updates haven't met a test that would show their superiority yet.
            scatter_gpu_cache_updates: false,
            // This is best as `Immediate` on Angle, or `Pixelbuffer(Dynamic)` on GL,
            // but we are unable to make this decision here, so picking the reasonable medium.
            upload_method: UploadMethod::PixelBuffer(VertexUsageHint::Stream),
            workers: None,
            blob_image_renderer: None,
            recorder: None,
            thread_listener: None,
            enable_render_on_scroll: true,
            renderer_id: None,
            cached_programs: None,
            disable_dual_source_blending: false,
        }
    }
}

#[cfg(not(feature = "debugger"))]
pub struct DebugServer;

#[cfg(not(feature = "debugger"))]
impl DebugServer {
    pub fn new(_: MsgSender<ApiMsg>) -> Self {
        DebugServer
    }

    pub fn send(&mut self, _: String) {}
}

// Some basic statistics about the rendered scene
// that we can use in wrench reftests to ensure that
// tests are batching and/or allocating on render
// targets as we expect them to.
pub struct RendererStats {
    pub total_draw_calls: usize,
    pub alpha_target_count: usize,
    pub color_target_count: usize,
}

impl RendererStats {
    pub fn empty() -> Self {
        RendererStats {
            total_draw_calls: 0,
            alpha_target_count: 0,
            color_target_count: 0,
        }
    }
}


#[cfg(feature = "capture")]
#[derive(Deserialize, Serialize)]
struct PlainTexture {
    data: String,
    size: (u32, u32, i32),
    format: ImageFormat,
    filter: TextureFilter,
    render_target: Option<RenderTargetInfo>,
}

#[cfg(feature = "capture")]
#[derive(Deserialize, Serialize)]
struct PlainRenderer {
    gpu_cache: PlainTexture,
    textures: Vec<PlainTexture>,
    external_images: Vec<ExternalCaptureImage>
}

#[cfg(feature = "capture")]
struct DummyExternalImageHandler {
    data: FastHashMap<(ExternalImageId, u8), Vec<u8>>,
}

#[cfg(feature = "capture")]
impl ExternalImageHandler for DummyExternalImageHandler {
    fn lock(&mut self, key: ExternalImageId, channel_index: u8) -> ExternalImage {
        let slice = &self.data[&(key, channel_index)];
        ExternalImage {
            u0: 0.0,
            v0: 0.0,
            u1: 1.0,
            v1: 1.0,
            source: ExternalImageSource::RawData(slice),
        }
    }
    fn unlock(&mut self, _key: ExternalImageId, _channel_index: u8) {}
}

#[cfg(feature = "capture")]
impl OutputImageHandler for () {
    fn lock(&mut self, _: PipelineId) -> Option<(u32, DeviceIntSize)> {
        None
    }
    fn unlock(&mut self, _: PipelineId) {
        unreachable!()
    }
}

#[cfg(feature = "capture")]
impl Renderer {
    fn save_texture(
        texture: &Texture, name: &str, root: &PathBuf, device: &mut Device
    ) -> PlainTexture {
        use std::fs;
        use std::io::Write;

        let short_path = format!("textures/{}.raw", name);

        let bytes_per_pixel = texture.get_format().bytes_per_pixel();
        let read_format = ReadPixelsFormat::Standard(texture.get_format());
        let rect = DeviceUintRect::new(
            DeviceUintPoint::zero(),
            texture.get_dimensions(),
        );

        let mut file = fs::File::create(root.join(&short_path))
            .expect(&format!("Unable to create {}", short_path));
        let bytes_per_layer = (rect.size.width * rect.size.height * bytes_per_pixel) as usize;
        let mut data = vec![0; bytes_per_layer];

        for layer_id in 0 .. texture.get_layer_count() {
            device.attach_read_texture(texture, layer_id);
            device.read_pixels_into(rect, read_format, &mut data);
            file.write_all(&data)
                .unwrap();
        }

        PlainTexture {
            data: short_path,
            size: (rect.size.width, rect.size.height, texture.get_layer_count()),
            format: texture.get_format(),
            filter: texture.get_filter(),
            render_target: texture.get_render_target(),
        }
    }

    fn load_texture(texture: &mut Texture, plain: &PlainTexture, root: &PathBuf, device: &mut Device) -> Vec<u8> {
        use std::fs::File;
        use std::io::Read;

        let mut texels = Vec::new();
        assert_eq!(plain.format, texture.get_format());
        File::open(root.join(&plain.data))
            .unwrap()
            .read_to_end(&mut texels)
            .unwrap();

        device.init_texture(
            texture, plain.size.0, plain.size.1,
            plain.filter, plain.render_target,
            plain.size.2, Some(texels.as_slice()),
        );

        texels
    }

    fn save_capture(&mut self, config: CaptureConfig, deferred_images: Vec<ExternalCaptureImage>) {
        use std::fs;
        use std::io::Write;
        use api::{CaptureBits, ExternalImageData};

        self.device.begin_frame();
        self.device.bind_read_target_impl(self.capture.read_fbo);

        if !deferred_images.is_empty() {
            info!("saving external images");
            let handler = self.external_image_handler
                .as_mut()
                .expect("Unable to lock the external image handler!");
            for def in &deferred_images {
                let ExternalImageData { id, channel_index, image_type } = def.external;
                let data = match handler.lock(id, channel_index).source {
                    ExternalImageSource::RawData(data) => data.to_vec(),
                    ExternalImageSource::NativeTexture(gl_id) => {
                        let target = get_external_image_target(image_type).unwrap();
                        self.device.attach_read_texture_external(gl_id, target, 0);
                        self.device.read_pixels(&def.descriptor)
                    }
                    ExternalImageSource::Invalid => {
                        // Create a dummy buffer...
                        let stride = def.descriptor.compute_stride();
                        let total_size = def.descriptor.height * stride;
                        vec![0xFF; total_size as usize]
                    }
                };
                handler.unlock(id, channel_index);

                fs::File::create(config.root.join(&def.short_path))
                    .expect(&format!("Unable to create {}", def.short_path))
                    .write_all(&data)
                    .unwrap();
            }
        }

        if config.bits.contains(CaptureBits::FRAME) {
            let path_textures = config.root.join("textures");
            if !path_textures.is_dir() {
                fs::create_dir(&path_textures).unwrap();
            }

            info!("saving GPU cache");
            let mut plain_self = PlainRenderer {
                gpu_cache: Self::save_texture(
                    &self.gpu_cache_texture.texture,
                    "gpu", &config.root, &mut self.device,
                ),
                textures: Vec::new(),
                external_images: deferred_images,
            };

            info!("saving cached textures");
            for texture in &self.texture_resolver.cache_texture_map {
                let file_name = format!("cache-{}", plain_self.textures.len() + 1);
                info!("\t{}", file_name);
                let plain = Self::save_texture(texture, &file_name, &config.root, &mut self.device);
                plain_self.textures.push(plain);
            }

            config.serialize(&plain_self, "renderer");
        }

        self.device.bind_read_target(None);
        self.device.end_frame();
        info!("done.");
    }

    fn load_capture(&mut self, root: PathBuf) {
        let renderer = match CaptureConfig::deserialize::<PlainRenderer, _>(&root, "renderer") {
            Some(r) => r,
            None => return,
        };

        self.device.begin_frame();
        info!("loading cached textures");

        for texture in self.texture_resolver.cache_texture_map.drain(..) {
            self.device.delete_texture(texture);
        }
        for texture in renderer.textures {
            info!("\t{}", texture.data);
            let mut t = self.device.create_texture(TextureTarget::Array, texture.format);
            Self::load_texture(&mut t, &texture, &root, &mut self.device);
            self.texture_resolver.cache_texture_map.push(t);
        }

        info!("loading gpu cache");
        Self::load_texture(
            &mut self.gpu_cache_texture.texture,
            &renderer.gpu_cache,
            &root,
            &mut self.device,
        );
        match self.gpu_cache_texture.bus {
            CacheBus::PixelBuffer { ref mut rows, ref mut cpu_blocks, .. } => {
                rows.clear();
                cpu_blocks.clear();
            }
            CacheBus::Scatter { .. } => {}
        }

        info!("loading external images");
        assert!(self.texture_resolver.external_images.is_empty());
        let mut image_handler = DummyExternalImageHandler {
            data: FastHashMap::default(),
        };

        for ExternalCaptureImage { short_path, external, descriptor } in renderer.external_images {
            let target = match get_external_image_target(external.image_type) {
                Some(target) => target,
                None => continue,
            };
            //TODO: provide a way to query both the layer count and the filter from external images
            let (layer_count, filter) = (1, TextureFilter::Linear);
            let plain = PlainTexture {
                data: short_path,
                size: (descriptor.width, descriptor.height, layer_count),
                format: descriptor.format,
                filter,
                render_target: None,
            };

            let mut t = self.device.create_texture(target, plain.format);
            let data = Self::load_texture(&mut t, &plain, &root, &mut self.device);
            let key = (external.id, external.channel_index);
            self.capture.owned_external_images.insert(key, t.into_external());
            image_handler.data.insert(key, data);
        }

        self.device.end_frame();
        self.external_image_handler = Some(Box::new(image_handler) as Box<_>);
        self.output_image_handler = Some(Box::new(()) as Box<_>);
        info!("done.");
    }
}

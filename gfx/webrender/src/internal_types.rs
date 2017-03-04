/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use device::TextureFilter;
use euclid::{TypedPoint2D, UnknownUnit};
use fnv::FnvHasher;
use offscreen_gl_context::{NativeGLContext, NativeGLContextHandle};
use offscreen_gl_context::{GLContext, NativeGLContextMethods, GLContextDispatcher};
use offscreen_gl_context::{OSMesaContext, OSMesaContextHandle};
use offscreen_gl_context::{ColorAttachmentType, GLContextAttributes, GLLimits};
use profiler::BackendProfileCounters;
use std::collections::{HashMap, HashSet};
use std::f32;
use std::hash::BuildHasherDefault;
use std::{i32, usize};
use std::path::PathBuf;
use std::sync::Arc;
use tiling;
use renderer::BlendMode;
use webrender_traits::{Epoch, ColorF, PipelineId, DeviceIntSize};
use webrender_traits::{ImageFormat, NativeFontHandle, MixBlendMode};
use webrender_traits::{ExternalImageId, ScrollLayerId, WebGLCommand};
use webrender_traits::{ImageData};
use webrender_traits::{DeviceUintRect};

// An ID for a texture that is owned by the
// texture cache module. This can include atlases
// or standalone textures allocated via the
// texture cache (e.g. if an image is too large
// to be added to an atlas). The texture cache
// manages the allocation and freeing of these
// IDs, and the rendering thread maintains a
// map from cache texture ID to native texture.

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub struct CacheTextureId(pub usize);

// Represents the source for a texture.
// These are passed from throughout the
// pipeline until they reach the rendering
// thread, where they are resolved to a
// native texture ID.

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
pub enum SourceTexture {
    Invalid,
    TextureCache(CacheTextureId),
    WebGL(u32),                         // Is actually a gl::GLuint
    External(ExternalImageId),
}

pub enum GLContextHandleWrapper {
    Native(NativeGLContextHandle),
    OSMesa(OSMesaContextHandle),
}

impl GLContextHandleWrapper {
    pub fn current_native_handle() -> Option<GLContextHandleWrapper> {
        NativeGLContext::current_handle().map(GLContextHandleWrapper::Native)
    }

    pub fn current_osmesa_handle() -> Option<GLContextHandleWrapper> {
        OSMesaContext::current_handle().map(GLContextHandleWrapper::OSMesa)
    }

    pub fn new_context(&self,
                       size: DeviceIntSize,
                       attributes: GLContextAttributes,
                       dispatcher: Option<Box<GLContextDispatcher>>) -> Result<GLContextWrapper, &'static str> {
        match *self {
            GLContextHandleWrapper::Native(ref handle) => {
                let ctx = GLContext::<NativeGLContext>::new_shared_with_dispatcher(size.to_untyped(),
                                                                                   attributes,
                                                                                   ColorAttachmentType::Texture,
                                                                                   Some(handle),
                                                                                   dispatcher);
                ctx.map(GLContextWrapper::Native)
            }
            GLContextHandleWrapper::OSMesa(ref handle) => {
                let ctx = GLContext::<OSMesaContext>::new_shared_with_dispatcher(size.to_untyped(),
                                                                                 attributes,
                                                                                 ColorAttachmentType::Texture,
                                                                                 Some(handle),
                                                                                 dispatcher);
                ctx.map(GLContextWrapper::OSMesa)
            }
        }
    }
}

pub enum GLContextWrapper {
    Native(GLContext<NativeGLContext>),
    OSMesa(GLContext<OSMesaContext>),
}

impl GLContextWrapper {
    pub fn make_current(&self) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                ctx.make_current().unwrap();
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                ctx.make_current().unwrap();
            }
        }
    }

    pub fn unbind(&self) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                ctx.unbind().unwrap();
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                ctx.unbind().unwrap();
            }
        }
    }

    pub fn apply_command(&self, cmd: WebGLCommand) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                cmd.apply(ctx);
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                cmd.apply(ctx);
            }
        }
    }

    pub fn get_info(&self) -> (DeviceIntSize, u32, GLLimits) {
        match *self {
            GLContextWrapper::Native(ref ctx) => {
                let (real_size, texture_id) = {
                    let draw_buffer = ctx.borrow_draw_buffer().unwrap();
                    (draw_buffer.size(), draw_buffer.get_bound_texture_id().unwrap())
                };

                let limits = ctx.borrow_limits().clone();

                (DeviceIntSize::from_untyped(&real_size), texture_id, limits)
            }
            GLContextWrapper::OSMesa(ref ctx) => {
                let (real_size, texture_id) = {
                    let draw_buffer = ctx.borrow_draw_buffer().unwrap();
                    (draw_buffer.size(), draw_buffer.get_bound_texture_id().unwrap())
                };

                let limits = ctx.borrow_limits().clone();

                (DeviceIntSize::from_untyped(&real_size), texture_id, limits)
            }
        }
    }

    pub fn resize(&mut self, size: &DeviceIntSize) -> Result<(), &'static str> {
        match *self {
            GLContextWrapper::Native(ref mut ctx) => {
                ctx.resize(size.to_untyped())
            }
            GLContextWrapper::OSMesa(ref mut ctx) => {
                ctx.resize(size.to_untyped())
            }
        }
    }
}

const COLOR_FLOAT_TO_FIXED: f32 = 255.0;
pub const ANGLE_FLOAT_TO_FIXED: f32 = 65535.0;

pub const ORTHO_NEAR_PLANE: f32 = -1000000.0;
pub const ORTHO_FAR_PLANE: f32 = 1000000.0;

#[derive(Clone)]
pub enum FontTemplate {
    Raw(Arc<Vec<u8>>),
    Native(NativeFontHandle),
}

#[derive(Debug, PartialEq, Eq)]
pub enum TextureSampler {
    Color0,
    Color1,
    Color2,
    Mask,
    Cache,
    Data16,
    Data32,
    Data64,
    Data128,
    Layers,
    RenderTasks,
    Geometry,
    ResourceRects,
    Gradients,
}

impl TextureSampler {
    pub fn color(n: usize) -> TextureSampler {
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

/// Optional textures that can be used as a source in the shaders.
/// Textures that are not used by the batch are equal to TextureId::invalid().
#[derive(Copy, Clone, Debug)]
pub struct BatchTextures {
    pub colors: [SourceTexture; 3],
}

impl BatchTextures {
    pub fn no_texture() -> Self {
        BatchTextures {
            colors: [SourceTexture::Invalid; 3],
        }
    }
}

// In some places we need to temporarily bind a texture to any slot.
pub const DEFAULT_TEXTURE: TextureSampler = TextureSampler::Color0;

#[derive(Clone, Copy, Debug)]
pub enum VertexAttribute {
    // vertex-frequency basic attributes
    Position,
    Color,
    ColorTexCoord,
    // instance-frequency primitive attributes
    GlobalPrimId,
    PrimitiveAddress,
    TaskIndex,
    ClipTaskIndex,
    LayerIndex,
    ElementIndex,
    UserData,
    ZIndex,
}

#[derive(Clone, Copy, Debug)]
pub enum ClearAttribute {
    // vertex frequency
    Position,
    // instance frequency
    Rectangle,
}

#[derive(Clone, Copy, Debug)]
pub enum BlurAttribute {
    // vertex frequency
    Position,
    // instance frequency
    RenderTaskIndex,
    SourceTaskIndex,
    Direction,
}

#[derive(Clone, Copy, Debug)]
pub enum ClipAttribute {
    // vertex frequency
    Position,
    // instance frequency
    RenderTaskIndex,
    LayerIndex,
    DataIndex,
    SegmentIndex,
}

// A packed RGBA8 color ordered for vertex data or similar.
// Use PackedTexel instead if intending to upload to a texture.

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct PackedColor {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl PackedColor {
    pub fn from_color(color: &ColorF) -> PackedColor {
        PackedColor {
            r: (0.5 + color.r * COLOR_FLOAT_TO_FIXED).floor() as u8,
            g: (0.5 + color.g * COLOR_FLOAT_TO_FIXED).floor() as u8,
            b: (0.5 + color.b * COLOR_FLOAT_TO_FIXED).floor() as u8,
            a: (0.5 + color.a * COLOR_FLOAT_TO_FIXED).floor() as u8,
        }
    }
}

// RGBA8 textures currently pack texels in BGRA format for upload.
// PackedTexel abstracts away this difference from PackedColor.

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct PackedTexel {
    pub b: u8,
    pub g: u8,
    pub r: u8,
    pub a: u8,
}

impl PackedTexel {
    pub fn from_color(color: &ColorF) -> PackedTexel {
        PackedTexel {
            b: (0.5 + color.b * COLOR_FLOAT_TO_FIXED).floor() as u8,
            g: (0.5 + color.g * COLOR_FLOAT_TO_FIXED).floor() as u8,
            r: (0.5 + color.r * COLOR_FLOAT_TO_FIXED).floor() as u8,
            a: (0.5 + color.a * COLOR_FLOAT_TO_FIXED).floor() as u8,
        }
    }
}

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct PackedVertex {
    pub pos: [f32; 2],
}

#[derive(Debug)]
#[repr(C)]
pub struct DebugFontVertex {
    pub x: f32,
    pub y: f32,
    pub color: PackedColor,
    pub u: f32,
    pub v: f32,
}

impl DebugFontVertex {
    pub fn new(x: f32, y: f32, u: f32, v: f32, color: PackedColor) -> DebugFontVertex {
        DebugFontVertex {
            x: x,
            y: y,
            color: color,
            u: u,
            v: v,
        }
    }
}

#[repr(C)]
pub struct DebugColorVertex {
    pub x: f32,
    pub y: f32,
    pub color: PackedColor,
}

impl DebugColorVertex {
    pub fn new(x: f32, y: f32, color: PackedColor) -> DebugColorVertex {
        DebugColorVertex {
            x: x,
            y: y,
            color: color,
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum RenderTargetMode {
    None,
    SimpleRenderTarget,
    LayerRenderTarget(i32),      // Number of texture layers
}

pub enum TextureUpdateOp {
    Create {
      width: u32,
      height: u32,
      format: ImageFormat,
      filter: TextureFilter,
      mode: RenderTargetMode,
      data: Option<ImageData>,
    },
    Update {
        page_pos_x: u32,    // the texture page position which we want to upload
        page_pos_y: u32,
        width: u32,
        height: u32,
        data: Arc<Vec<u8>>,
        stride: Option<u32>,
        offset: u32,
    },
    UpdateForExternalBuffer {
        rect: DeviceUintRect,
        id: ExternalImageId,
        stride: Option<u32>,
    },
    Grow {
        width: u32,
        height: u32,
        format: ImageFormat,
        filter: TextureFilter,
        mode: RenderTargetMode,
    },
    Free,
}

pub type ExternalImageUpdateList = Vec<ExternalImageId>;

pub struct TextureUpdate {
    pub id: CacheTextureId,
    pub op: TextureUpdateOp,
}

pub struct TextureUpdateList {
    pub updates: Vec<TextureUpdate>,
}

impl TextureUpdateList {
    pub fn new() -> TextureUpdateList {
        TextureUpdateList {
            updates: Vec::new(),
        }
    }

    #[inline]
    pub fn push(&mut self, update: TextureUpdate) {
        self.updates.push(update);
    }
}

/// Mostly wraps a tiling::Frame, adding a bit of extra information.
pub struct RendererFrame {
    /// The last rendered epoch for each pipeline present in the frame.
    /// This information is used to know if a certain transformation on the layout has
    /// been rendered, which is necessary for reftests.
    pub pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
    /// The layers that are currently affected by the over-scrolling animation.
    pub layers_bouncing_back: HashSet<ScrollLayerId, BuildHasherDefault<FnvHasher>>,

    pub frame: Option<tiling::Frame>,
}

impl RendererFrame {
    pub fn new(pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
               layers_bouncing_back: HashSet<ScrollLayerId, BuildHasherDefault<FnvHasher>>,
               frame: Option<tiling::Frame>)
               -> RendererFrame {
        RendererFrame {
            pipeline_epoch_map: pipeline_epoch_map,
            layers_bouncing_back: layers_bouncing_back,
            frame: frame,
        }
    }
}

pub enum ResultMsg {
    RefreshShader(PathBuf),
    NewFrame(RendererFrame, TextureUpdateList, ExternalImageUpdateList, BackendProfileCounters),
}

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum AxisDirection {
    Horizontal,
    Vertical,
}

#[derive(Debug, Clone, Copy, Eq, Hash, PartialEq)]
pub struct StackingContextIndex(pub usize);

#[derive(Clone, Copy, Debug)]
pub struct RectUv<T, U = UnknownUnit> {
    pub top_left: TypedPoint2D<T, U>,
    pub top_right: TypedPoint2D<T, U>,
    pub bottom_left: TypedPoint2D<T, U>,
    pub bottom_right: TypedPoint2D<T, U>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum LowLevelFilterOp {
    Blur(Au, AxisDirection),
    Brightness(Au),
    Contrast(Au),
    Grayscale(Au),
    /// Fixed-point in `ANGLE_FLOAT_TO_FIXED` units.
    HueRotate(i32),
    Invert(Au),
    Opacity(Au),
    Saturate(Au),
    Sepia(Au),
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum HardwareCompositeOp {
    Multiply,
    Max,
    Min,
}

impl HardwareCompositeOp {
    pub fn from_mix_blend_mode(mix_blend_mode: MixBlendMode) -> Option<HardwareCompositeOp> {
        match mix_blend_mode {
            MixBlendMode::Multiply => Some(HardwareCompositeOp::Multiply),
            MixBlendMode::Lighten => Some(HardwareCompositeOp::Max),
            MixBlendMode::Darken => Some(HardwareCompositeOp::Min),
            _ => None,
        }
    }

    pub fn to_blend_mode(&self) -> BlendMode {
        match self {
            &HardwareCompositeOp::Multiply => BlendMode::Multiply,
            &HardwareCompositeOp::Max => BlendMode::Max,
            &HardwareCompositeOp::Min => BlendMode::Min,
        }
    }
}

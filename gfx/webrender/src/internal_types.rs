/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use device::TextureFilter;
use euclid::{TypedPoint2D, UnknownUnit};
use fnv::FnvHasher;
use profiler::BackendProfileCounters;
use std::collections::{HashMap, HashSet};
use std::f32;
use std::hash::BuildHasherDefault;
use std::{i32, usize};
use std::path::PathBuf;
use std::sync::Arc;
use tiling;
use renderer::BlendMode;
use webrender_traits::{ClipId, ColorF, DeviceUintRect, Epoch, ExternalImageData, ExternalImageId};
use webrender_traits::{ImageData, ImageFormat, NativeFontHandle, PipelineId};

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
    External(ExternalImageData),
    #[cfg_attr(not(feature = "webgl"), allow(dead_code))]
    /// This is actually a gl::GLuint, with the shared texture id between the
    /// main context and the WebGL context.
    WebGL(u32),
}

const COLOR_FLOAT_TO_FIXED: f32 = 255.0;
const COLOR_FLOAT_TO_FIXED_WIDE: f32 = 65535.0;
pub const ANGLE_FLOAT_TO_FIXED: f32 = 65535.0;

pub const ORTHO_NEAR_PLANE: f32 = -1000000.0;
pub const ORTHO_FAR_PLANE: f32 = 1000000.0;

#[derive(Clone)]
pub enum FontTemplate {
    Raw(Arc<Vec<u8>>, u32),
    Native(NativeFontHandle),
}

#[derive(Debug, PartialEq, Eq)]
pub enum TextureSampler {
    Color0,
    Color1,
    Color2,
    CacheA8,
    CacheRGBA8,
    Data16,
    Data32,
    ResourceCache,
    Layers,
    RenderTasks,
    Geometry,
    ResourceRects,
    Gradients,
    SplitGeometry,
    Dither,
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
    Data0,
    Data1,
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
    pub fn high_bytes(color: &ColorF) -> PackedTexel {
        Self::extract_bytes(color, 8)
    }

    pub fn low_bytes(color: &ColorF) -> PackedTexel {
        Self::extract_bytes(color, 0)
    }

    fn extract_bytes(color: &ColorF, shift_by: i32) -> PackedTexel {
        PackedTexel {
            b: ((0.5 + color.b * COLOR_FLOAT_TO_FIXED_WIDE).floor() as u32 >> shift_by & 0xff) as u8,
            g: ((0.5 + color.g * COLOR_FLOAT_TO_FIXED_WIDE).floor() as u32 >> shift_by & 0xff) as u8,
            r: ((0.5 + color.r * COLOR_FLOAT_TO_FIXED_WIDE).floor() as u32 >> shift_by & 0xff) as u8,
            a: ((0.5 + color.a * COLOR_FLOAT_TO_FIXED_WIDE).floor() as u32 >> shift_by & 0xff) as u8,
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
        channel_index: u8,
        stride: Option<u32>,
        offset: u32,
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
    pub layers_bouncing_back: HashSet<ClipId, BuildHasherDefault<FnvHasher>>,

    pub frame: Option<tiling::Frame>,
}

impl RendererFrame {
    pub fn new(pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
               layers_bouncing_back: HashSet<ClipId, BuildHasherDefault<FnvHasher>>,
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
    NewFrame(RendererFrame, TextureUpdateList, BackendProfileCounters),
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
    PremultipliedAlpha,
}

impl HardwareCompositeOp {
    pub fn to_blend_mode(&self) -> BlendMode {
        match *self {
            HardwareCompositeOp::PremultipliedAlpha => BlendMode::PremultipliedAlpha,
        }
    }
}

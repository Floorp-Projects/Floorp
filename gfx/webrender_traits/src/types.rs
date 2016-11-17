/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Every serialisable type is defined in this file to only codegen one file
// for the serde implementations.

use app_units::Au;
#[cfg(feature = "nightly")]
use core::nonzero::NonZero;
use euclid::{Matrix4D, Point2D, Rect, Size2D};
use ipc_channel::ipc::{IpcBytesSender, IpcSender};
use offscreen_gl_context::{GLContextAttributes, GLLimits};

#[cfg(target_os = "macos")] use core_graphics::font::CGFont;
#[cfg(target_os = "windows")] use dwrote::FontDescriptor;

#[derive(Debug, Copy, Clone)]
pub enum RendererKind {
    Native,
    OSMesa,
}

#[derive(Deserialize, Serialize)]
pub enum ApiMsg {
    AddRawFont(FontKey, Vec<u8>),
    AddNativeFont(FontKey, NativeFontHandle),
    /// Gets the glyph dimensions
    GetGlyphDimensions(Vec<GlyphKey>, IpcSender<Vec<Option<GlyphDimensions>>>),
    /// Adds an image from the resource cache.
    AddImage(ImageKey, u32, u32, Option<u32>, ImageFormat, Vec<u8>),
    /// Updates the the resource cache with the new image data.
    UpdateImage(ImageKey, u32, u32, ImageFormat, Vec<u8>),
    /// Drops an image from the resource cache.
    DeleteImage(ImageKey),
    CloneApi(IpcSender<IdNamespace>),
    // Flushes all messages
    Flush,
    /// Supplies a new frame to WebRender.
    ///
    /// After receiving this message, WebRender will read the display list, followed by the
    /// auxiliary lists, from the payload channel.
    SetRootDisplayList(ColorF,
                       Epoch,
                       PipelineId,
                       Size2D<f32>,
                       BuiltDisplayListDescriptor,
                       AuxiliaryListsDescriptor),
    SetRootPipeline(PipelineId),
    Scroll(Point2D<f32>, Point2D<f32>, ScrollEventPhase),
    TickScrollingBounce,
    GenerateFrame,
    TranslatePointToLayerSpace(Point2D<f32>, IpcSender<(Point2D<f32>, PipelineId)>),
    GetScrollLayerState(IpcSender<Vec<ScrollLayerState>>),
    RequestWebGLContext(Size2D<i32>, GLContextAttributes, IpcSender<Result<(WebGLContextId, GLLimits), String>>),
    ResizeWebGLContext(WebGLContextId, Size2D<i32>),
    WebGLCommand(WebGLContextId, WebGLCommand),
}

#[derive(Copy, Clone, Deserialize, Serialize, Debug)]
pub struct GlyphDimensions {
    pub left: i32,
    pub top: i32,
    pub width: u32,
    pub height: u32,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct AuxiliaryLists {
    /// The concatenation of: gradient stops, complex clip regions, filters, and glyph instances,
    /// in that order.
    data: Vec<u8>,
    descriptor: AuxiliaryListsDescriptor,
}

/// Describes the memory layout of the auxiliary lists.
///
/// Auxiliary lists consist of some number of gradient stops, complex clip regions, filters, and
/// glyph instances, in that order.
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct AuxiliaryListsDescriptor {
    gradient_stops_size: usize,
    complex_clip_regions_size: usize,
    filters_size: usize,
    glyph_instances_size: usize,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderDisplayItem {
    pub left: BorderSide,
    pub right: BorderSide,
    pub top: BorderSide,
    pub bottom: BorderSide,
    pub radius: BorderRadius,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderRadius {
    pub top_left: Size2D<f32>,
    pub top_right: Size2D<f32>,
    pub bottom_left: Size2D<f32>,
    pub bottom_right: Size2D<f32>,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderSide {
    pub width: f32,
    pub color: ColorF,
    pub style: BorderStyle,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BorderStyle {
    None,
    Solid,
    Double,
    Dotted,
    Dashed,
    Hidden,
    Groove,
    Ridge,
    Inset,
    Outset,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BoxShadowClipMode {
    None,
    Outset,
    Inset,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BoxShadowDisplayItem {
    pub box_bounds: Rect<f32>,
    pub offset: Point2D<f32>,
    pub color: ColorF,
    pub blur_radius: f32,
    pub spread_radius: f32,
    pub border_radius: f32,
    pub clip_mode: BoxShadowClipMode,
}

/// A display list.
#[derive(Clone, Deserialize, Serialize)]
pub struct BuiltDisplayList {
    data: Vec<u8>,
    descriptor: BuiltDisplayListDescriptor,
}

/// Describes the memory layout of a display list.
///
/// A display list consists of some number of display list items, followed by a number of display
/// items.
#[derive(Copy, Clone, Deserialize, Serialize)]
pub struct BuiltDisplayListDescriptor {
    pub mode: DisplayListMode,

    /// The size in bytes of the display list items in this display list.
    display_list_items_size: usize,
    /// The size in bytes of the display items in this display list.
    display_items_size: usize,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ColorF {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}
known_heap_size!(0, ColorF);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageMask {
    pub image: ImageKey,
    pub rect: Rect<f32>,
    pub repeat: bool,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ClipRegion {
    pub main: Rect<f32>,
    pub complex: ItemRange,
    pub image_mask: Option<ImageMask>,
}

impl ClipRegion {
    pub fn is_complex(&self) -> bool {
        self.complex.length !=0 || self.image_mask.is_some()
    }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ComplexClipRegion {
    /// The boundaries of the rectangle.
    pub rect: Rect<f32>,
    /// Border radii of this rectangle.
    pub radii: BorderRadius,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct DisplayItem {
    pub item: SpecificDisplayItem,
    pub rect: Rect<f32>,
    pub clip: ClipRegion,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum DisplayListMode {
    Default,
    PseudoFloat,
    PseudoPositionedContent,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
pub struct Epoch(pub u32);

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub enum FilterOp {
    Blur(Au),
    Brightness(f32),
    Contrast(f32),
    Grayscale(f32),
    HueRotate(f32),
    Invert(f32),
    Opacity(f32),
    Saturate(f32),
    Sepia(f32),
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct FontKey(u32, u32);

#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq, Serialize, Deserialize)]
pub enum FontRenderMode {
    Mono,
    Alpha,
    Subpixel,
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize)]
pub struct GlyphKey {
    pub font_key: FontKey,
    pub size: Au,
    pub index: u32,
}

impl GlyphKey {
    pub fn new(font_key: FontKey,
               size: Au,
               index: u32) -> GlyphKey {
        GlyphKey {
            font_key: font_key,
            size: size,
            index: index,
        }
    }
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum FragmentType {
    FragmentBody,
    BeforePseudoContent,
    AfterPseudoContent,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GlyphInstance {
    pub index: u32,
    pub x: f32,
    pub y: f32,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GradientDisplayItem {
    pub start_point: Point2D<f32>,
    pub end_point: Point2D<f32>,
    pub stops: ItemRange,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GradientStop {
    pub offset: f32,
    pub color: ColorF,
}
known_heap_size!(0, GradientStop);


#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PushStackingContextDisplayItem {
    pub stacking_context: StackingContext,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct IframeDisplayItem {
    pub pipeline_id: PipelineId,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct IdNamespace(pub u32);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageDisplayItem {
    pub image_key: ImageKey,
    pub stretch_size: Size2D<f32>,
    pub tile_spacing: Size2D<f32>,
    pub image_rendering: ImageRendering,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ImageFormat {
    Invalid,
    A8,
    RGB8,
    RGBA8,
    RGBAF32,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ImageKey(u32, u32);

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ImageRendering {
    Auto,
    CrispEdges,
    Pixelated,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ItemRange {
    pub start: usize,
    pub length: usize,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum MixBlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
}

#[cfg(target_os = "macos")]
pub type NativeFontHandle = CGFont;

/// Native fonts are not used on Linux; all fonts are raw.
#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[cfg_attr(not(any(target_os = "macos", target_os = "windows")), derive(Clone, Serialize, Deserialize))]
pub struct NativeFontHandle;

#[cfg(target_os = "windows")]
pub type NativeFontHandle = FontDescriptor;

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct PipelineId(pub u32, pub u32);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RectangleDisplayItem {
    pub color: ColorF,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct RenderApiSender {
    api_sender: IpcSender<ApiMsg>,
    payload_sender: IpcBytesSender,
}

pub trait RenderNotifier: Send {
    fn new_frame_ready(&mut self);
    fn new_scroll_frame_ready(&mut self, composite_needed: bool);
    fn pipeline_size_changed(&mut self, pipeline_id: PipelineId, size: Option<Size2D<f32>>);
}

pub trait FlushNotifier: Send {
    fn all_messages_flushed(&mut self);
}

// Trait to allow dispatching functions to a specific thread or event loop.
pub trait RenderDispatcher: Send {
    fn dispatch(&self, Box<Fn() + Send>);
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct ResourceId(pub u32);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum ScrollEventPhase {
    /// The user started scrolling.
    Start,
    /// The user performed a scroll. The Boolean flag indicates whether the user's fingers are
    /// down, if a touchpad is in use. (If false, the event is a touchpad fling.)
    Move(bool),
    /// The user ended scrolling.
    End,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ScrollLayerId {
    pub pipeline_id: PipelineId,
    pub info: ScrollLayerInfo,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ScrollLayerInfo {
    Fixed,
    Scrollable(usize, ServoScrollRootId)
}

#[derive(Clone, Deserialize, Serialize)]
pub struct ScrollLayerState {
    pub pipeline_id: PipelineId,
    pub scroll_root_id: ServoScrollRootId,
    pub scroll_offset: Point2D<f32>,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, PartialEq, Serialize)]
pub enum ScrollPolicy {
    Scrollable,
    Fixed,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ServoScrollRootId(pub usize);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum SpecificDisplayItem {
    Rectangle(RectangleDisplayItem),
    Text(TextDisplayItem),
    Image(ImageDisplayItem),
    WebGL(WebGLDisplayItem),
    Border(BorderDisplayItem),
    BoxShadow(BoxShadowDisplayItem),
    Gradient(GradientDisplayItem),
    Iframe(IframeDisplayItem),
    PushStackingContext(PushStackingContextDisplayItem),
    PopStackingContext,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct StackingContext {
    pub scroll_layer_id: Option<ScrollLayerId>,
    pub scroll_policy: ScrollPolicy,
    pub bounds: Rect<f32>,
    pub overflow: Rect<f32>,
    pub z_index: i32,
    pub transform: Matrix4D<f32>,
    pub perspective: Matrix4D<f32>,
    pub mix_blend_mode: MixBlendMode,
    pub filters: ItemRange,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct TextDisplayItem {
    pub glyphs: ItemRange,
    pub font_key: FontKey,
    pub size: Au,
    pub color: ColorF,
    pub blur_radius: Au,
}

#[derive(Clone, Deserialize, Serialize)]
pub enum WebGLCommand {
    GetContextAttributes(IpcSender<GLContextAttributes>),
    ActiveTexture(u32),
    BlendColor(f32, f32, f32, f32),
    BlendEquation(u32),
    BlendEquationSeparate(u32, u32),
    BlendFunc(u32, u32),
    BlendFuncSeparate(u32, u32, u32, u32),
    AttachShader(WebGLProgramId, WebGLShaderId),
    DetachShader(WebGLProgramId, WebGLShaderId),
    BindAttribLocation(WebGLProgramId, u32, String),
    BufferData(u32, Vec<u8>, u32),
    BufferSubData(u32, isize, Vec<u8>),
    Clear(u32),
    ClearColor(f32, f32, f32, f32),
    ClearDepth(f64),
    ClearStencil(i32),
    ColorMask(bool, bool, bool, bool),
    CullFace(u32),
    FrontFace(u32),
    DepthFunc(u32),
    DepthMask(bool),
    DepthRange(f64, f64),
    Enable(u32),
    Disable(u32),
    CompileShader(WebGLShaderId, String),
    CopyTexImage2D(u32, i32, u32, i32, i32, i32, i32, i32),
    CopyTexSubImage2D(u32, i32, i32, i32, i32, i32, i32, i32),
    CreateBuffer(IpcSender<Option<WebGLBufferId>>),
    CreateFramebuffer(IpcSender<Option<WebGLFramebufferId>>),
    CreateRenderbuffer(IpcSender<Option<WebGLRenderbufferId>>),
    CreateTexture(IpcSender<Option<WebGLTextureId>>),
    CreateProgram(IpcSender<Option<WebGLProgramId>>),
    CreateShader(u32, IpcSender<Option<WebGLShaderId>>),
    DeleteBuffer(WebGLBufferId),
    DeleteFramebuffer(WebGLFramebufferId),
    DeleteRenderbuffer(WebGLRenderbufferId),
    DeleteTexture(WebGLTextureId),
    DeleteProgram(WebGLProgramId),
    DeleteShader(WebGLShaderId),
    BindBuffer(u32, Option<WebGLBufferId>),
    BindFramebuffer(u32, WebGLFramebufferBindingRequest),
    BindRenderbuffer(u32, Option<WebGLRenderbufferId>),
    BindTexture(u32, Option<WebGLTextureId>),
    DisableVertexAttribArray(u32),
    DrawArrays(u32, i32, i32),
    DrawElements(u32, i32, u32, i64),
    EnableVertexAttribArray(u32),
    FramebufferRenderbuffer(u32, u32, u32, Option<WebGLRenderbufferId>),
    FramebufferTexture2D(u32, u32, u32, Option<WebGLTextureId>, i32),
    GetBufferParameter(u32, u32, IpcSender<WebGLResult<WebGLParameter>>),
    GetParameter(u32, IpcSender<WebGLResult<WebGLParameter>>),
    GetProgramParameter(WebGLProgramId, u32, IpcSender<WebGLResult<WebGLParameter>>),
    GetShaderParameter(WebGLShaderId, u32, IpcSender<WebGLResult<WebGLParameter>>),
    GetActiveAttrib(WebGLProgramId, u32, IpcSender<WebGLResult<(i32, u32, String)>>),
    GetActiveUniform(WebGLProgramId, u32, IpcSender<WebGLResult<(i32, u32, String)>>),
    GetAttribLocation(WebGLProgramId, String, IpcSender<Option<i32>>),
    GetUniformLocation(WebGLProgramId, String, IpcSender<Option<i32>>),
    GetVertexAttrib(u32, u32, IpcSender<WebGLResult<WebGLParameter>>),
    GetShaderInfoLog(WebGLShaderId, IpcSender<String>),
    GetProgramInfoLog(WebGLProgramId, IpcSender<String>),
    PolygonOffset(f32, f32),
    RenderbufferStorage(u32, u32, i32, i32),
    ReadPixels(i32, i32, i32, i32, u32, u32, IpcSender<Vec<u8>>),
    SampleCoverage(f32, bool),
    Scissor(i32, i32, i32, i32),
    StencilFunc(u32, i32, u32),
    StencilFuncSeparate(u32, u32, i32, u32),
    StencilMask(u32),
    StencilMaskSeparate(u32, u32),
    StencilOp(u32, u32, u32),
    StencilOpSeparate(u32, u32, u32, u32),
    Hint(u32, u32),
    IsEnabled(u32, IpcSender<bool>),
    LineWidth(f32),
    PixelStorei(u32, i32),
    LinkProgram(WebGLProgramId),
    Uniform1f(i32, f32),
    Uniform1fv(i32, Vec<f32>),
    Uniform1i(i32, i32),
    Uniform1iv(i32, Vec<i32>),
    Uniform2f(i32, f32, f32),
    Uniform2fv(i32, Vec<f32>),
    Uniform2i(i32, i32, i32),
    Uniform2iv(i32, Vec<i32>),
    Uniform3f(i32, f32, f32, f32),
    Uniform3fv(i32, Vec<f32>),
    Uniform3i(i32, i32, i32, i32),
    Uniform3iv(i32, Vec<i32>),
    Uniform4f(i32, f32, f32, f32, f32),
    Uniform4fv(i32, Vec<f32>),
    Uniform4i(i32, i32, i32, i32, i32),
    Uniform4iv(i32, Vec<i32>),
    UniformMatrix2fv(i32, bool, Vec<f32>),
    UniformMatrix3fv(i32, bool, Vec<f32>),
    UniformMatrix4fv(i32, bool, Vec<f32>),
    UseProgram(WebGLProgramId),
    ValidateProgram(WebGLProgramId),
    VertexAttrib(u32, f32, f32, f32, f32),
    VertexAttribPointer(u32, i32, u32, bool, i32, u32),
    VertexAttribPointer2f(u32, i32, bool, i32, u32),
    Viewport(i32, i32, i32, i32),
    TexImage2D(u32, i32, i32, i32, i32, u32, u32, Vec<u8>),
    TexParameteri(u32, u32, i32),
    TexParameterf(u32, u32, f32),
    TexSubImage2D(u32, i32, i32, i32, i32, i32, u32, u32, Vec<u8>),
    DrawingBufferWidth(IpcSender<i32>),
    DrawingBufferHeight(IpcSender<i32>),
    Finish(IpcSender<()>),
    Flush,
    GenerateMipmap(u32),
}

#[cfg(feature = "nightly")]
macro_rules! define_resource_id_struct {
    ($name:ident) => {
        #[derive(Clone, Copy, PartialEq)]
        pub struct $name(NonZero<u32>);

        impl $name {
            #[inline]
            unsafe fn new(id: u32) -> Self {
                $name(NonZero::new(id))
            }

            #[inline]
            fn get(self) -> u32 {
                *self.0
            }
        }

    };
}

#[cfg(not(feature = "nightly"))]
macro_rules! define_resource_id_struct {
    ($name:ident) => {
        #[derive(Clone, Copy, PartialEq)]
        pub struct $name(u32);

        impl $name {
            #[inline]
            unsafe fn new(id: u32) -> Self {
                $name(id)
            }

            #[inline]
            fn get(self) -> u32 {
                self.0
            }
        }
    };
}

macro_rules! define_resource_id {
    ($name:ident) => {
        define_resource_id_struct!($name);

        impl ::serde::Deserialize for $name {
            fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
                where D: ::serde::Deserializer
            {
                let id = try!(u32::deserialize(deserializer));
                if id == 0 {
                    Err(::serde::Error::invalid_value("expected a non-zero value"))
                } else {
                    Ok(unsafe { $name::new(id) })
                }
            }
        }

        impl ::serde::Serialize for $name {
            fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
                where S: ::serde::Serializer
            {
                self.get().serialize(serializer)
            }
        }

        impl ::std::fmt::Debug for $name {
            fn fmt(&self, fmt: &mut ::std::fmt::Formatter)
                  -> Result<(), ::std::fmt::Error> {
                fmt.debug_tuple(stringify!($name))
                   .field(&self.get())
                   .finish()
            }
        }

        impl ::std::fmt::Display for $name {
            fn fmt(&self, fmt: &mut ::std::fmt::Formatter)
                  -> Result<(), ::std::fmt::Error> {
                write!(fmt, "{}", self.get())
            }
        }

        impl ::heapsize::HeapSizeOf for $name {
            fn heap_size_of_children(&self) -> usize { 0 }
        }
    }
}

define_resource_id!(WebGLBufferId);
define_resource_id!(WebGLFramebufferId);
define_resource_id!(WebGLRenderbufferId);
define_resource_id!(WebGLTextureId);
define_resource_id!(WebGLProgramId);
define_resource_id!(WebGLShaderId);

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
pub struct WebGLContextId(pub usize);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct WebGLDisplayItem {
    pub context_id: WebGLContextId,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum WebGLError {
    InvalidEnum,
    InvalidFramebufferOperation,
    InvalidOperation,
    InvalidValue,
    OutOfMemory,
    ContextLost,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum WebGLFramebufferBindingRequest {
    Explicit(WebGLFramebufferId),
    Default,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum WebGLParameter {
    Int(i32),
    Bool(bool),
    String(String),
    Float(f32),
    FloatArray(Vec<f32>),
    Invalid,
}

pub type WebGLResult<T> = Result<T, WebGLError>;

#[derive(Clone, Debug, Deserialize, Serialize)]
pub enum WebGLShaderParameter {
    Int(i32),
    Bool(bool),
    Invalid,
}

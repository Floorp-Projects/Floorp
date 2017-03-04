/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Every serialisable type is defined in this file to only codegen one file
// for the serde implementations.

use app_units::Au;
use euclid::{Point2D, SideOffsets2D};
use channel::{PayloadSender, MsgSender};
#[cfg(feature = "nightly")]
use core::nonzero::NonZero;
use offscreen_gl_context::{GLContextAttributes, GLLimits};
use std::fmt;
use std::marker::PhantomData;
use std::sync::Arc;

#[cfg(target_os = "macos")] use core_graphics::font::CGFont;
#[cfg(target_os = "windows")] use dwrote::FontDescriptor;

#[derive(Debug, Copy, Clone)]
pub enum RendererKind {
    Native,
    OSMesa,
}

pub type TileSize = u16;

#[derive(Clone, Deserialize, Serialize)]
pub enum ApiMsg {
    AddRawFont(FontKey, Vec<u8>),
    AddNativeFont(FontKey, NativeFontHandle),
    /// Gets the glyph dimensions
    GetGlyphDimensions(Vec<GlyphKey>, MsgSender<Vec<Option<GlyphDimensions>>>),
    /// Adds an image from the resource cache.
    AddImage(ImageKey, ImageDescriptor, ImageData, Option<TileSize>),
    /// Updates the the resource cache with the new image data.
    UpdateImage(ImageKey, ImageDescriptor, Vec<u8>),
    /// Drops an image from the resource cache.
    DeleteImage(ImageKey),
    CloneApi(MsgSender<IdNamespace>),
    /// Supplies a new frame to WebRender.
    ///
    /// After receiving this message, WebRender will read the display list, followed by the
    /// auxiliary lists, from the payload channel.
    SetRootDisplayList(Option<ColorF>,
                       Epoch,
                       PipelineId,
                       LayoutSize,
                       BuiltDisplayListDescriptor,
                       AuxiliaryListsDescriptor,
                       bool),
    SetRootPipeline(PipelineId),
    Scroll(ScrollLocation, WorldPoint, ScrollEventPhase),
    ScrollLayersWithScrollId(LayoutPoint, PipelineId, ServoScrollRootId),
    TickScrollingBounce,
    TranslatePointToLayerSpace(WorldPoint, MsgSender<(LayoutPoint, PipelineId)>),
    GetScrollLayerState(MsgSender<Vec<ScrollLayerState>>),
    RequestWebGLContext(DeviceIntSize, GLContextAttributes, MsgSender<Result<(WebGLContextId, GLLimits), String>>),
    ResizeWebGLContext(WebGLContextId, DeviceIntSize),
    WebGLCommand(WebGLContextId, WebGLCommand),
    GenerateFrame(Option<DynamicProperties>),
    // WebVR commands that must be called in the WebGL render thread.
    VRCompositorCommand(WebGLContextId, VRCompositorCommand),
    /// An opaque handle that must be passed to the render notifier. It is used by Gecko
    /// to forward gecko-specific messages to the render thread preserving the ordering
    /// within the other messages.
    ExternalEvent(ExternalEvent),
    ShutDown,
}

/// An opaque pointer-sized value.
#[repr(C)]
#[derive(Clone, Deserialize, Serialize)]
pub struct ExternalEvent {
    raw: usize,
}

unsafe impl Send for ExternalEvent {}

impl ExternalEvent {
    pub fn from_raw(raw: usize) -> Self { ExternalEvent { raw: raw } }
    /// Consumes self to make it obvious that the event should be forwarded only once.
    pub fn unwrap(self) -> usize { self.raw }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize, Eq, Hash)]
pub struct PropertyBindingId {
    namespace: u32,
    uid: u32,
}

/// A unique key that is used for connecting animated property
/// values to bindings in the display list.
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PropertyBindingKey<T> {
    pub id: PropertyBindingId,
    _phantom: PhantomData<T>,
}

/// Construct a property value from a given key and value.
impl<T: Copy> PropertyBindingKey<T> {
    pub fn with(&self, value: T) -> PropertyValue<T> {
        PropertyValue {
            key: *self,
            value: value,
        }
    }
}

/// A binding property can either be a specific value
/// (the normal, non-animated case) or point to a binding location
/// to fetch the current value from.
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum PropertyBinding<T> {
    Value(T),
    Binding(PropertyBindingKey<T>),
}

impl<T> From<T> for PropertyBinding<T> {
    fn from(value: T) -> PropertyBinding<T> {
        PropertyBinding::Value(value)
    }
}

impl<T> From<PropertyBindingKey<T>> for PropertyBinding<T> {
    fn from(key: PropertyBindingKey<T>) -> PropertyBinding<T> {
        PropertyBinding::Binding(key)
    }
}

/// The current value of an animated property. This is
/// supplied by the calling code.
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct PropertyValue<T> {
    pub key: PropertyBindingKey<T>,
    pub value: T,
}

/// When using generate_frame(), a list of PropertyValue structures
/// can optionally be supplied to provide the current value of any
/// animated properties.
#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct DynamicProperties {
    pub transforms: Vec<PropertyValue<LayoutTransform>>,
    pub floats: Vec<PropertyValue<f32>>,
}

#[repr(C)]
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
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct AuxiliaryListsDescriptor {
    gradient_stops_size: usize,
    complex_clip_regions_size: usize,
    filters_size: usize,
    glyph_instances_size: usize,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct NormalBorder {
    pub left: BorderSide,
    pub right: BorderSide,
    pub top: BorderSide,
    pub bottom: BorderSide,
    pub radius: BorderRadius,
}

#[derive(Debug, Copy, Clone, PartialEq, Serialize, Deserialize)]
pub enum RepeatMode {
    Stretch,
    Repeat,
    Round,
    Space,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct NinePatchDescriptor {
    pub width: u32,
    pub height: u32,
    pub slice: SideOffsets2D<u32>,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageBorder {
    pub image_key: ImageKey,
    pub patch: NinePatchDescriptor,
    pub outset: SideOffsets2D<f32>,
    pub repeat_horizontal: RepeatMode,
    pub repeat_vertical: RepeatMode,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BorderDetails {
    Normal(NormalBorder),
    Image(ImageBorder),
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderDisplayItem {
    pub widths: BorderWidths,
    pub details: BorderDetails,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderRadius {
    pub top_left: LayoutSize,
    pub top_right: LayoutSize,
    pub bottom_left: LayoutSize,
    pub bottom_right: LayoutSize,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderWidths {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderSide {
    pub color: ColorF,
    pub style: BorderStyle,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BorderStyle {
    None    = 0,
    Solid   = 1,
    Double  = 2,
    Dotted  = 3,
    Dashed  = 4,
    Hidden  = 5,
    Groove  = 6,
    Ridge   = 7,
    Inset   = 8,
    Outset  = 9,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BoxShadowClipMode {
    None    = 0,
    Outset  = 1,
    Inset   = 2,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BoxShadowDisplayItem {
    pub box_bounds: LayoutRect,
    pub offset: LayoutPoint,
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
#[repr(C)]
#[derive(Copy, Clone, Deserialize, Serialize)]
pub struct BuiltDisplayListDescriptor {
    /// The size in bytes of the display list items in this display list.
    display_list_items_size: usize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ColorF {
    pub r: f32,
    pub g: f32,
    pub b: f32,
    pub a: f32,
}
known_heap_size!(0, ColorF);

#[repr(C)]
#[derive(Clone, Copy, Hash, Eq, Debug, Deserialize, PartialEq, PartialOrd, Ord, Serialize)]
pub struct ColorU {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl From<ColorF> for ColorU {
    fn from(color: ColorF) -> ColorU {
        ColorU {
            r: ColorU::round_to_int(color.r),
            g: ColorU::round_to_int(color.g),
            b: ColorU::round_to_int(color.b),
            a: ColorU::round_to_int(color.a),
        }
    }
}

impl Into<ColorF> for ColorU {
    fn into(self) -> ColorF {
        ColorF {
            r: self.r as f32 / 255.0,
            g: self.g as f32 / 255.0,
            b: self.b as f32 / 255.0,
            a: self.a as f32 / 255.0,
        }
    }
}

impl ColorU {
    fn round_to_int(x: f32) -> u8 {
        debug_assert!((0.0 <= x) && (x <= 1.0));
        let f = (255.0 * x) + 0.5;
        let val = f.floor();
        debug_assert!(val <= 255.0);
        val as u8
    }

    pub fn new(r: u8, g: u8, b: u8, a: u8) -> ColorU {
        ColorU {
            r: r,
            g: g,
            b: b,
            a: a,
        }
    }
}

#[derive(Copy, Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageDescriptor {
    pub format: ImageFormat,
    pub width: u32,
    pub height: u32,
    pub stride: Option<u32>,
    pub offset: u32,
    pub is_opaque: bool,
}

impl ImageDescriptor {
    pub fn new(width: u32, height: u32, format: ImageFormat, is_opaque: bool) -> Self {
        ImageDescriptor {
            width: width,
            height: height,
            format: format,
            stride: None,
            offset: 0,
            is_opaque: is_opaque,
        }
    }

    pub fn compute_stride(&self) -> u32 {
        self.stride.unwrap_or(self.width * self.format.bytes_per_pixel().unwrap())
    }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageMask {
    pub image: ImageKey,
    pub rect: LayoutRect,
    pub repeat: bool,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ClipRegion {
    pub main: LayoutRect,
    pub complex: ItemRange,
    pub image_mask: Option<ImageMask>,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ComplexClipRegion {
    /// The boundaries of the rectangle.
    pub rect: LayoutRect,
    /// Border radii of this rectangle.
    pub radii: BorderRadius,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct DisplayItem {
    pub item: SpecificDisplayItem,
    pub rect: LayoutRect,
    pub clip: ClipRegion,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
pub struct Epoch(pub u32);

#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq, Serialize, Deserialize, Ord, PartialOrd)]
pub enum ExtendMode {
    Clamp,
    Repeat,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub enum FilterOp {
    Blur(Au),
    Brightness(f32),
    Contrast(f32),
    Grayscale(f32),
    HueRotate(f32),
    Invert(f32),
    Opacity(PropertyBinding<f32>),
    Saturate(f32),
    Sepia(f32),
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize, Ord, PartialOrd)]
pub struct FontKey(pub u32, pub u32);

#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq, Serialize, Deserialize, Ord, PartialOrd)]
pub enum FontRenderMode {
    Mono,
    Alpha,
    Subpixel,
}

impl FontRenderMode {
    // Skia quantizes subpixel offets into 1/4 increments.
    // Given the absolute position, return the quantized increment
    fn subpixel_quantize_offset(&self, pos: f32) -> SubpixelOffset {
        if *self != FontRenderMode::Subpixel {
            return SubpixelOffset::Zero;
        }

        const SUBPIXEL_ROUNDING :f32 = 0.125; // Skia chosen value.
        let fraction = (pos + SUBPIXEL_ROUNDING).fract();

        match fraction {
            0.0...0.25 => SubpixelOffset::Zero,
            0.25...0.5 => SubpixelOffset::Quarter,
            0.5...0.75 => SubpixelOffset::Half,
            0.75...1.0 => SubpixelOffset::ThreeQuarters,
            _ => panic!("Should only be given the fractional part"),
        }
    }
}

#[repr(u8)]
#[derive(Hash, Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd, Serialize, Deserialize)]
pub enum SubpixelOffset {
    Zero            = 0,
    Quarter         = 1,
    Half            = 2,
    ThreeQuarters   = 3,
}

impl Into<f64> for SubpixelOffset {
    fn into(self) -> f64 {
        match self {
            SubpixelOffset::Zero => 0.0,
            SubpixelOffset::Quarter => 0.25,
            SubpixelOffset::Half => 0.5,
            SubpixelOffset::ThreeQuarters => 0.75,
        }
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize, Ord, PartialOrd)]
pub struct SubpixelPoint {
    pub x: SubpixelOffset,
    pub y: SubpixelOffset,
}

impl SubpixelPoint {
    pub fn new(point: Point2D<f32>,
               render_mode: FontRenderMode) -> SubpixelPoint {
        SubpixelPoint {
            x: render_mode.subpixel_quantize_offset(point.x),
            y: render_mode.subpixel_quantize_offset(point.y),
        }
    }

    pub fn to_f64(&self) -> (f64, f64) {
        return (self.x.into(), self.y.into());
    }

    pub fn set_offset(&mut self, point: Point2D<f32>, render_mode: FontRenderMode) {
        self.x = render_mode.subpixel_quantize_offset(point.x);
        self.y = render_mode.subpixel_quantize_offset(point.y);
    }
}

#[derive(Clone, Hash, PartialEq, Eq, Debug, Deserialize, Serialize, Ord, PartialOrd)]
pub struct GlyphKey {
    pub font_key: FontKey,
    // The font size is in *device* pixels, not logical pixels.
    // It is stored as an Au since we need sub-pixel sizes, but
    // can't store as a f32 due to use of this type as a hash key.
    // TODO(gw): Perhaps consider having LogicalAu and DeviceAu
    //           or something similar to that.
    pub size: Au,
    pub index: u32,
    pub color: ColorU,
    pub subpixel_point: SubpixelPoint,
}

impl GlyphKey {
    pub fn new(font_key: FontKey,
               size: Au,
               color: ColorF,
               index: u32,
               point: Point2D<f32>,
               render_mode: FontRenderMode) -> GlyphKey {
        GlyphKey {
            font_key: font_key,
            size: size,
            color: ColorU::from(color),
            index: index,
            subpixel_point: SubpixelPoint::new(point, render_mode),
        }
    }
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum FragmentType {
    FragmentBody        = 0,
    BeforePseudoContent = 1,
    AfterPseudoContent  = 2,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GlyphInstance {
    pub index: u32,
    pub point: Point2D<f32>,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GradientDisplayItem {
    pub start_point: LayoutPoint,
    pub end_point: LayoutPoint,
    pub stops: ItemRange,
    pub extend_mode: ExtendMode,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GradientStop {
    pub offset: f32,
    pub color: ColorF,
}
known_heap_size!(0, GradientStop);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RadialGradientDisplayItem {
    pub start_center: LayoutPoint,
    pub start_radius: f32,
    pub end_center: LayoutPoint,
    pub end_radius: f32,
    pub stops: ItemRange,
    pub extend_mode: ExtendMode,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PushStackingContextDisplayItem {
    pub stacking_context: StackingContext,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PushScrollLayerItem {
    pub content_size: LayoutSize,
    pub id: ScrollLayerId,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct IframeDisplayItem {
    pub pipeline_id: PipelineId,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct IdNamespace(pub u32);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageDisplayItem {
    pub image_key: ImageKey,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub image_rendering: ImageRendering,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct YuvImageDisplayItem {
    pub y_image_key: ImageKey,
    pub u_image_key: ImageKey,
    pub v_image_key: ImageKey,
    pub color_space: YuvColorSpace,
}

#[repr(C)]
#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ImageFormat {
    Invalid  = 0,
    A8       = 1,
    RGB8     = 2,
    RGBA8    = 3,
    RGBAF32  = 4,
}

impl ImageFormat {
    pub fn bytes_per_pixel(self) -> Option<u32> {
        match self {
            ImageFormat::A8 => Some(1),
            ImageFormat::RGB8 => Some(3),
            ImageFormat::RGBA8 => Some(4),
            ImageFormat::RGBAF32 => Some(16),
            ImageFormat::Invalid => None,
        }
    }
}

#[repr(C)]
#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum YuvColorSpace {
    Rec601 = 1, // The values must match the ones in prim_shared.glsl
    Rec709 = 2,
}

/// An arbitrary identifier for an external image provided by the
/// application. It must be a unique identifier for each external
/// image.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash, Serialize, Deserialize)]
pub struct ExternalImageId(pub u64);

pub trait BlobImageRenderer: Send {
    fn request_blob_image(&mut self,
                            key: ImageKey,
                            data: Arc<BlobImageData>,
                            descriptor: &BlobImageDescriptor);
    fn resolve_blob_image(&mut self, key: ImageKey) -> BlobImageResult;
}

pub type BlobImageData = Vec<u8>;

#[derive(Copy, Clone, Debug)]
pub struct BlobImageDescriptor {
    pub width: u32,
    pub height: u32,
    pub format: ImageFormat,
    pub scale_factor: f32,
}

pub struct RasterizedBlobImage {
    pub width: u32,
    pub height: u32,
    pub data: Vec<u8>,
}

#[derive(Clone, Debug)]
pub enum BlobImageError {
    Oom,
    InvalidKey,
    InvalidData,
    Other(String),
}

pub type BlobImageResult = Result<RasterizedBlobImage, BlobImageError>;

#[derive(Clone, Serialize, Deserialize)]
pub enum ImageData {
    Raw(Arc<Vec<u8>>),
    Blob(Arc<BlobImageData>),
    ExternalHandle(ExternalImageId),
    ExternalBuffer(ExternalImageId),
}

impl ImageData {
    pub fn new(bytes: Vec<u8>) -> ImageData {
        ImageData::Raw(Arc::new(bytes))
    }

    pub fn new_shared(bytes: Arc<Vec<u8>>) -> ImageData {
        ImageData::Raw(bytes)
    }

    pub fn new_blob_image(commands: Vec<u8>) -> ImageData {
        ImageData::Blob(Arc::new(commands))
    }

    pub fn new_shared_blob_image(commands: Arc<Vec<u8>>) -> ImageData {
        ImageData::Blob(commands)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ImageKey(pub u32, pub u32);

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ImageRendering {
    Auto        = 0,
    CrispEdges  = 1,
    Pixelated   = 2,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ItemRange {
    pub start: usize,
    pub length: usize,
}

#[repr(C)]
#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum MixBlendMode {
    Normal      = 0,
    Multiply    = 1,
    Screen      = 2,
    Overlay     = 3,
    Darken      = 4,
    Lighten     = 5,
    ColorDodge  = 6,
    ColorBurn   = 7,
    HardLight   = 8,
    SoftLight   = 9,
    Difference  = 10,
    Exclusion   = 11,
    Hue         = 12,
    Saturation  = 13,
    Color       = 14,
    Luminosity  = 15,
}

#[cfg(target_os = "macos")]
pub type NativeFontHandle = CGFont;

/// Native fonts are not used on Linux; all fonts are raw.
#[cfg(not(any(target_os = "macos", target_os = "windows")))]
#[cfg_attr(not(any(target_os = "macos", target_os = "windows")), derive(Clone, Serialize, Deserialize))]
pub struct NativeFontHandle;

#[cfg(target_os = "windows")]
pub type NativeFontHandle = FontDescriptor;

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct PipelineId(pub u32, pub u32);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RectangleDisplayItem {
    pub color: ColorF,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct RenderApiSender {
    api_sender: MsgSender<ApiMsg>,
    payload_sender: PayloadSender,
}

pub trait RenderNotifier: Send {
    fn new_frame_ready(&mut self);
    fn new_scroll_frame_ready(&mut self, composite_needed: bool);
    fn external_event(&mut self, _evt: ExternalEvent) { unimplemented!() }
    fn shut_down(&mut self) {}
}

// Trait to allow dispatching functions to a specific thread or event loop.
pub trait RenderDispatcher: Send {
    fn dispatch(&self, Box<Fn() + Send>);
}

#[repr(C)]
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

impl ScrollLayerId {
    pub fn root_scroll_layer(pipeline_id: PipelineId) -> ScrollLayerId {
        ScrollLayerId {
            pipeline_id: pipeline_id,
            info: ScrollLayerInfo::Scrollable(0, ServoScrollRootId(0)),
        }
    }

    pub fn root_reference_frame(pipeline_id: PipelineId) -> ScrollLayerId {
        ScrollLayerId {
            pipeline_id: pipeline_id,
            info: ScrollLayerInfo::ReferenceFrame(0),
        }
    }

    pub fn scroll_root_id(&self) -> Option<ServoScrollRootId> {
        match self.info {
            ScrollLayerInfo::Scrollable(_, scroll_root_id) => Some(scroll_root_id),
            ScrollLayerInfo::ReferenceFrame(..) => None,
        }
    }
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ScrollLayerInfo {
    Scrollable(usize, ServoScrollRootId),
    ReferenceFrame(usize),
}

#[derive(Clone, Deserialize, Serialize)]
pub struct ScrollLayerState {
    pub pipeline_id: PipelineId,
    pub scroll_root_id: ServoScrollRootId,
    pub scroll_offset: LayoutPoint,
}

#[repr(C)]
#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, PartialEq, Serialize)]
pub enum ScrollPolicy {
    Scrollable  = 0,
    Fixed       = 1,
}
known_heap_size!(0, ScrollPolicy);

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub enum ScrollLocation {
    /// Scroll by a certain amount.
    Delta(LayoutPoint), 
    /// Scroll to very top of element.
    Start,
    /// Scroll to very bottom of element. 
    End 
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ServoScrollRootId(pub usize);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum SpecificDisplayItem {
    Rectangle(RectangleDisplayItem),
    Text(TextDisplayItem),
    Image(ImageDisplayItem),
    YuvImage(YuvImageDisplayItem),
    WebGL(WebGLDisplayItem),
    Border(BorderDisplayItem),
    BoxShadow(BoxShadowDisplayItem),
    Gradient(GradientDisplayItem),
    RadialGradient(RadialGradientDisplayItem),
    Iframe(IframeDisplayItem),
    PushStackingContext(PushStackingContextDisplayItem),
    PopStackingContext,
    PushScrollLayer(PushScrollLayerItem),
    PopScrollLayer,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct StackingContext {
    pub scroll_policy: ScrollPolicy,
    pub bounds: LayoutRect,
    pub z_index: i32,
    pub transform: PropertyBinding<LayoutTransform>,
    pub perspective: LayoutTransform,
    pub mix_blend_mode: MixBlendMode,
    pub filters: ItemRange,
}

#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct GlyphOptions {
    // These are currently only used on windows for dwrite fonts.
    pub use_embedded_bitmap: bool,
    pub force_gdi_rendering: bool,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct TextDisplayItem {
    pub glyphs: ItemRange,
    pub font_key: FontKey,
    pub size: Au,
    pub color: ColorF,
    pub blur_radius: Au,
    pub glyph_options: Option<GlyphOptions>,
}

#[derive(Clone, Deserialize, Serialize)]
pub enum WebGLCommand {
    GetContextAttributes(MsgSender<GLContextAttributes>),
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
    CreateBuffer(MsgSender<Option<WebGLBufferId>>),
    CreateFramebuffer(MsgSender<Option<WebGLFramebufferId>>),
    CreateRenderbuffer(MsgSender<Option<WebGLRenderbufferId>>),
    CreateTexture(MsgSender<Option<WebGLTextureId>>),
    CreateProgram(MsgSender<Option<WebGLProgramId>>),
    CreateShader(u32, MsgSender<Option<WebGLShaderId>>),
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
    GetBufferParameter(u32, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetParameter(u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetProgramParameter(WebGLProgramId, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetShaderParameter(WebGLShaderId, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetActiveAttrib(WebGLProgramId, u32, MsgSender<WebGLResult<(i32, u32, String)>>),
    GetActiveUniform(WebGLProgramId, u32, MsgSender<WebGLResult<(i32, u32, String)>>),
    GetAttribLocation(WebGLProgramId, String, MsgSender<Option<i32>>),
    GetUniformLocation(WebGLProgramId, String, MsgSender<Option<i32>>),
    GetVertexAttrib(u32, u32, MsgSender<WebGLResult<WebGLParameter>>),
    GetShaderInfoLog(WebGLShaderId, MsgSender<String>),
    GetProgramInfoLog(WebGLProgramId, MsgSender<String>),
    PolygonOffset(f32, f32),
    RenderbufferStorage(u32, u32, i32, i32),
    ReadPixels(i32, i32, i32, i32, u32, u32, MsgSender<Vec<u8>>),
    SampleCoverage(f32, bool),
    Scissor(i32, i32, i32, i32),
    StencilFunc(u32, i32, u32),
    StencilFuncSeparate(u32, u32, i32, u32),
    StencilMask(u32),
    StencilMaskSeparate(u32, u32),
    StencilOp(u32, u32, u32),
    StencilOpSeparate(u32, u32, u32, u32),
    Hint(u32, u32),
    IsEnabled(u32, MsgSender<bool>),
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
    DrawingBufferWidth(MsgSender<i32>),
    DrawingBufferHeight(MsgSender<i32>),
    Finish(MsgSender<()>),
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
            fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
                where D: ::serde::Deserializer
            {
                let id = try!(u32::deserialize(deserializer));
                if id == 0 {
                    Err(::serde::de::Error::custom("expected a non-zero value"))
                } else {
                    Ok(unsafe { $name::new(id) })
                }
            }
        }

        impl ::serde::Serialize for $name {
            fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
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

pub type VRCompositorId = u64;

// WebVR commands that must be called in the WebGL render thread.
#[derive(Clone, Deserialize, Serialize)]
pub enum VRCompositorCommand {
    Create(VRCompositorId),
    SyncPoses(VRCompositorId, f64, f64, MsgSender<Result<Vec<u8>,()>>),
    SubmitFrame(VRCompositorId, [f32; 4], [f32; 4]),
    Release(VRCompositorId)
}

// Trait object that handles WebVR commands.
// Receives the texture_id associated to the WebGLContext.
pub trait VRCompositorHandler: Send {
    fn handle(&mut self, command: VRCompositorCommand, texture_id: Option<u32>);
}

impl fmt::Debug for ApiMsg {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            &ApiMsg::AddRawFont(..) => { write!(f, "ApiMsg::AddRawFont") }
            &ApiMsg::AddNativeFont(..) => { write!(f, "ApiMsg::AddNativeFont") }
            &ApiMsg::GetGlyphDimensions(..) => { write!(f, "ApiMsg::GetGlyphDimensions") }
            &ApiMsg::AddImage(..) => { write!(f, "ApiMsg::AddImage") }
            &ApiMsg::UpdateImage(..) => { write!(f, "ApiMsg::UpdateImage") }
            &ApiMsg::DeleteImage(..) => { write!(f, "ApiMsg::DeleteImage") }
            &ApiMsg::CloneApi(..) => { write!(f, "ApiMsg::CloneApi") }
            &ApiMsg::SetRootDisplayList(..) => { write!(f, "ApiMsg::SetRootDisplayList") }
            &ApiMsg::SetRootPipeline(..) => { write!(f, "ApiMsg::SetRootPipeline") }
            &ApiMsg::Scroll(..) => { write!(f, "ApiMsg::Scroll") }
            &ApiMsg::ScrollLayersWithScrollId(..) => { write!(f, "ApiMsg::ScrollLayersWithScrollId") }
            &ApiMsg::TickScrollingBounce => { write!(f, "ApiMsg::TickScrollingBounce") }
            &ApiMsg::TranslatePointToLayerSpace(..) => { write!(f, "ApiMsg::TranslatePointToLayerSpace") }
            &ApiMsg::GetScrollLayerState(..) => { write!(f, "ApiMsg::GetScrollLayerState") }
            &ApiMsg::RequestWebGLContext(..) => { write!(f, "ApiMsg::RequestWebGLContext") }
            &ApiMsg::ResizeWebGLContext(..) => { write!(f, "ApiMsg::ResizeWebGLContext") }
            &ApiMsg::WebGLCommand(..) => { write!(f, "ApiMsg::WebGLCommand") }
            &ApiMsg::GenerateFrame(..) => { write!(f, "ApiMsg::GenerateFrame") }
            &ApiMsg::VRCompositorCommand(..) => { write!(f, "ApiMsg::VRCompositorCommand") }
            &ApiMsg::ExternalEvent(..) => { write!(f, "ApiMsg::ExternalEvent") }
            &ApiMsg::ShutDown => { write!(f, "ApiMsg::ShutDown") }
        }
    }
}

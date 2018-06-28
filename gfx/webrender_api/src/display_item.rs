/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[cfg(any(feature = "serialize", feature = "deserialize"))]
use GlyphInstance;
use euclid::{SideOffsets2D, TypedRect};
use std::ops::Not;
use {ColorF, FontInstanceKey, GlyphOptions, ImageKey, LayoutPixel, LayoutPoint};
use {LayoutRect, LayoutSize, LayoutTransform, LayoutVector2D, PipelineId, PropertyBinding};


// NOTE: some of these structs have an "IMPLICIT" comment.
// This indicates that the BuiltDisplayList will have serialized
// a list of values nearby that this item consumes. The traversal
// iterator should handle finding these.

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ClipAndScrollInfo {
    pub scroll_node_id: ClipId,
    pub clip_node_id: Option<ClipId>,
}

impl ClipAndScrollInfo {
    pub fn simple(node_id: ClipId) -> ClipAndScrollInfo {
        ClipAndScrollInfo {
            scroll_node_id: node_id,
            clip_node_id: None,
        }
    }

    pub fn new(scroll_node_id: ClipId, clip_node_id: ClipId) -> ClipAndScrollInfo {
        ClipAndScrollInfo {
            scroll_node_id,
            clip_node_id: Some(clip_node_id),
        }
    }

    pub fn clip_node_id(&self) -> ClipId {
        self.clip_node_id.unwrap_or(self.scroll_node_id)
    }
}

/// A tag that can be used to identify items during hit testing. If the tag
/// is missing then the item doesn't take part in hit testing at all. This
/// is composed of two numbers. In Servo, the first is an identifier while the
/// second is used to select the cursor that should be used during mouse
/// movement. In Gecko, the first is a scrollframe identifier, while the second
/// is used to store various flags that APZ needs to properly process input
/// events.
pub type ItemTag = (u64, u16);

/// The DI is generic over the specifics, while allows to use
/// the "complete" version of it for convenient serialization.
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GenericDisplayItem<T> {
    pub item: T,
    pub clip_and_scroll: ClipAndScrollInfo,
    pub info: LayoutPrimitiveInfo,
}

pub type DisplayItem = GenericDisplayItem<SpecificDisplayItem>;

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PrimitiveInfo<T> {
    pub rect: TypedRect<f32, T>,
    pub clip_rect: TypedRect<f32, T>,
    pub is_backface_visible: bool,
    pub tag: Option<ItemTag>,
}

impl LayoutPrimitiveInfo {
    pub fn new(rect: TypedRect<f32, LayoutPixel>) -> Self {
        Self::with_clip_rect(rect, rect)
    }

    pub fn with_clip_rect(
        rect: TypedRect<f32, LayoutPixel>,
        clip_rect: TypedRect<f32, LayoutPixel>,
    ) -> Self {
        PrimitiveInfo {
            rect,
            clip_rect,
            is_backface_visible: true,
            tag: None,
        }
    }
}

pub type LayoutPrimitiveInfo = PrimitiveInfo<LayoutPixel>;

#[repr(u64)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum SpecificDisplayItem {
    Clip(ClipDisplayItem),
    ScrollFrame(ScrollFrameDisplayItem),
    StickyFrame(StickyFrameDisplayItem),
    Rectangle(RectangleDisplayItem),
    ClearRectangle,
    Line(LineDisplayItem),
    Text(TextDisplayItem),
    Image(ImageDisplayItem),
    YuvImage(YuvImageDisplayItem),
    Border(BorderDisplayItem),
    BoxShadow(BoxShadowDisplayItem),
    Gradient(GradientDisplayItem),
    RadialGradient(RadialGradientDisplayItem),
    ClipChain(ClipChainItem),
    Iframe(IframeDisplayItem),
    PushStackingContext(PushStackingContextDisplayItem),
    PopStackingContext,
    PushReferenceFrame(PushReferenceFrameDisplayListItem),
    PopReferenceFrame,
    SetGradientStops,
    PushShadow(Shadow),
    PopAllShadows,
}

/// This is a "complete" version of the DI specifics,
/// containing the auxiliary data within the corresponding
/// enumeration variants, to be used for debug serialization.
#[cfg(any(feature = "serialize", feature = "deserialize"))]
#[cfg_attr(feature = "serialize", derive(Serialize))]
#[cfg_attr(feature = "deserialize", derive(Deserialize))]
pub enum CompletelySpecificDisplayItem {
    Clip(ClipDisplayItem, Vec<ComplexClipRegion>),
    ClipChain(ClipChainItem, Vec<ClipId>),
    ScrollFrame(ScrollFrameDisplayItem, Vec<ComplexClipRegion>),
    StickyFrame(StickyFrameDisplayItem),
    Rectangle(RectangleDisplayItem),
    ClearRectangle,
    Line(LineDisplayItem),
    Text(TextDisplayItem, Vec<GlyphInstance>),
    Image(ImageDisplayItem),
    YuvImage(YuvImageDisplayItem),
    Border(BorderDisplayItem),
    BoxShadow(BoxShadowDisplayItem),
    Gradient(GradientDisplayItem),
    RadialGradient(RadialGradientDisplayItem),
    Iframe(IframeDisplayItem),
    PushStackingContext(PushStackingContextDisplayItem, Vec<FilterOp>),
    PopStackingContext,
    PushReferenceFrame(PushReferenceFrameDisplayListItem),
    PopReferenceFrame,
    SetGradientStops(Vec<GradientStop>),
    PushShadow(Shadow),
    PopAllShadows,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ClipDisplayItem {
    pub id: ClipId,
    pub image_mask: Option<ImageMask>,
}

/// The minimum and maximum allowable offset for a sticky frame in a single dimension.
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct StickyOffsetBounds {
    /// The minimum offset for this frame, typically a negative value, which specifies how
    /// far in the negative direction the sticky frame can offset its contents in this
    /// dimension.
    pub min: f32,

    /// The maximum offset for this frame, typically a positive value, which specifies how
    /// far in the positive direction the sticky frame can offset its contents in this
    /// dimension.
    pub max: f32,
}

impl StickyOffsetBounds {
    pub fn new(min: f32, max: f32) -> StickyOffsetBounds {
        StickyOffsetBounds { min, max }
    }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct StickyFrameDisplayItem {
    pub id: ClipId,

    /// The margins that should be maintained between the edge of the parent viewport and this
    /// sticky frame. A margin of None indicates that the sticky frame should not stick at all
    /// to that particular edge of the viewport.
    pub margins: SideOffsets2D<Option<f32>>,

    /// The minimum and maximum vertical offsets for this sticky frame. Ignoring these constraints,
    /// the sticky frame will continue to stick to the edge of the viewport as its original
    /// position is scrolled out of view. Constraints specify a maximum and minimum offset from the
    /// original position relative to non-sticky content within the same scrolling frame.
    pub vertical_offset_bounds: StickyOffsetBounds,

    /// The minimum and maximum horizontal offsets for this sticky frame. Ignoring these constraints,
    /// the sticky frame will continue to stick to the edge of the viewport as its original
    /// position is scrolled out of view. Constraints specify a maximum and minimum offset from the
    /// original position relative to non-sticky content within the same scrolling frame.
    pub horizontal_offset_bounds: StickyOffsetBounds,

    /// The amount of offset that has already been applied to the sticky frame. A positive y
    /// component this field means that a top-sticky item was in a scrollframe that has been
    /// scrolled down, such that the sticky item's position needed to be offset downwards by
    /// `previously_applied_offset.y`. A negative y component corresponds to the upward offset
    /// applied due to bottom-stickiness. The x-axis works analogously.
    pub previously_applied_offset: LayoutVector2D,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum ScrollSensitivity {
    ScriptAndInputEvents,
    Script,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ScrollFrameDisplayItem {
    pub clip_id: ClipId,
    pub scroll_frame_id: ClipId,
    pub external_id: Option<ExternalScrollId>,
    pub image_mask: Option<ImageMask>,
    pub scroll_sensitivity: ScrollSensitivity,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RectangleDisplayItem {
    pub color: ColorF,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct LineDisplayItem {
    pub orientation: LineOrientation, // toggles whether above values are interpreted as x/y values
    pub wavy_line_thickness: f32,
    pub color: ColorF,
    pub style: LineStyle,
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum LineOrientation {
    Vertical,
    Horizontal,
}

#[repr(u8)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum LineStyle {
    Solid,
    Dotted,
    Dashed,
    Wavy,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct TextDisplayItem {
    pub font_key: FontInstanceKey,
    pub color: ColorF,
    pub glyph_options: Option<GlyphOptions>,
} // IMPLICIT: glyphs: Vec<GlyphInstance>

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct NormalBorder {
    pub left: BorderSide,
    pub right: BorderSide,
    pub top: BorderSide,
    pub bottom: BorderSide,
    pub radius: BorderRadius,
}

#[repr(u32)]
#[derive(Debug, Copy, Clone, PartialEq, Serialize, Deserialize)]
pub enum RepeatMode {
    Stretch,
    Repeat,
    Round,
    Space,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum NinePatchBorderSource {
    Image(ImageKey),
    Gradient(Gradient),
    RadialGradient(RadialGradient),
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct NinePatchBorder {
    /// Describes what to use as the 9-patch source image. If this is an image,
    /// it will be stretched to fill the size given by width x height.
    pub source: NinePatchBorderSource,

    /// The width of the 9-part image.
    pub width: u32,

    /// The height of the 9-part image.
    pub height: u32,

    /// Distances from each edge where the image should be sliced up. These
    /// values are in 9-part-image space (the same space as width and height),
    /// and the resulting image parts will be used to fill the corresponding
    /// parts of the border as given by the border widths. This can lead to
    /// stretching.
    /// Slices can be overlapping. In that case, the same pixels from the
    /// 9-part image will show up in multiple parts of the resulting border.
    pub slice: SideOffsets2D<u32>,

    /// Controls whether the center of the 9 patch image is rendered or
    /// ignored. The center is never rendered if the slices are overlapping.
    pub fill: bool,

    /// Determines what happens if the horizontal side parts of the 9-part
    /// image have a different size than the horizontal parts of the border.
    pub repeat_horizontal: RepeatMode,

    /// Determines what happens if the vertical side parts of the 9-part
    /// image have a different size than the vertical parts of the border.
    pub repeat_vertical: RepeatMode,

    /// The outset for the border.
    /// TODO(mrobinson): This should be removed and handled by the client.
    pub outset: SideOffsets2D<f32>,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BorderDetails {
    Normal(NormalBorder),
    NinePatch(NinePatchBorder),
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderDisplayItem {
    pub widths: BorderWidths,
    pub details: BorderDetails,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BorderRadiusKind {
    Uniform,
    NonUniform,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BorderRadius {
    pub top_left: LayoutSize,
    pub top_right: LayoutSize,
    pub bottom_left: LayoutSize,
    pub bottom_right: LayoutSize,
}

#[repr(C)]
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
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize, Hash, Eq)]
pub enum BorderStyle {
    None = 0,
    Solid = 1,
    Double = 2,
    Dotted = 3,
    Dashed = 4,
    Hidden = 5,
    Groove = 6,
    Ridge = 7,
    Inset = 8,
    Outset = 9,
}

impl BorderStyle {
    pub fn is_hidden(&self) -> bool {
        *self == BorderStyle::Hidden || *self == BorderStyle::None
    }
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum BoxShadowClipMode {
    Outset = 0,
    Inset = 1,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct BoxShadowDisplayItem {
    pub box_bounds: LayoutRect,
    pub offset: LayoutVector2D,
    pub color: ColorF,
    pub blur_radius: f32,
    pub spread_radius: f32,
    pub border_radius: BorderRadius,
    pub clip_mode: BoxShadowClipMode,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct Shadow {
    pub offset: LayoutVector2D,
    pub color: ColorF,
    pub blur_radius: f32,
}

#[repr(u32)]
#[derive(Debug, Copy, Clone, Hash, Eq, PartialEq, Serialize, Deserialize, Ord, PartialOrd)]
pub enum ExtendMode {
    Clamp,
    Repeat,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct Gradient {
    pub start_point: LayoutPoint,
    pub end_point: LayoutPoint,
    pub extend_mode: ExtendMode,
} // IMPLICIT: stops: Vec<GradientStop>

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GradientDisplayItem {
    pub gradient: Gradient,
    pub tile_size: LayoutSize,
    pub tile_spacing: LayoutSize,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct GradientStop {
    pub offset: f32,
    pub color: ColorF,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RadialGradient {
    pub center: LayoutPoint,
    pub radius: LayoutSize,
    pub start_offset: f32,
    pub end_offset: f32,
    pub extend_mode: ExtendMode,
} // IMPLICIT stops: Vec<GradientStop>

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ClipChainItem {
    pub id: ClipChainId,
    pub parent: Option<ClipChainId>,
} // IMPLICIT stops: Vec<ClipId>

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RadialGradientDisplayItem {
    pub gradient: RadialGradient,
    pub tile_size: LayoutSize,
    pub tile_spacing: LayoutSize,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PushReferenceFrameDisplayListItem {
    pub reference_frame: ReferenceFrame,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ReferenceFrame {
    pub transform: Option<PropertyBinding<LayoutTransform>>,
    pub perspective: Option<LayoutTransform>,
    pub id: ClipId,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PushStackingContextDisplayItem {
    pub stacking_context: StackingContext,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct StackingContext {
    pub transform_style: TransformStyle,
    pub mix_blend_mode: MixBlendMode,
    pub clip_node_id: Option<ClipId>,
    pub glyph_raster_space: GlyphRasterSpace,
} // IMPLICIT: filters: Vec<FilterOp>


#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum TransformStyle {
    Flat = 0,
    Preserve3D = 1,
}

// TODO(gw): In the future, we may modify this to apply to all elements
//           within a stacking context, rather than just the glyphs. If
//           this change occurs, we'll update the naming of this.
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
#[repr(u32)]
pub enum GlyphRasterSpace {
    // Rasterize glyphs in local-space, applying supplied scale to glyph sizes.
    // Best performance, but lower quality.
    Local(f32),

    // Rasterize the glyphs in screen-space, including rotation / skew etc in
    // the rasterized glyph. Best quality, but slower performance. Note that
    // any stacking context with a perspective transform will be rasterized
    // in local-space, even if this is set.
    Screen,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum MixBlendMode {
    Normal = 0,
    Multiply = 1,
    Screen = 2,
    Overlay = 3,
    Darken = 4,
    Lighten = 5,
    ColorDodge = 6,
    ColorBurn = 7,
    HardLight = 8,
    SoftLight = 9,
    Difference = 10,
    Exclusion = 11,
    Hue = 12,
    Saturation = 13,
    Color = 14,
    Luminosity = 15,
}

#[derive(Clone, Copy, Debug, PartialEq, Deserialize, Serialize)]
pub enum FilterOp {
    Blur(f32),
    Brightness(f32),
    Contrast(f32),
    Grayscale(f32),
    HueRotate(f32),
    Invert(f32),
    Opacity(PropertyBinding<f32>, f32),
    Saturate(f32),
    Sepia(f32),
    DropShadow(LayoutVector2D, f32, ColorF),
    ColorMatrix([f32; 20]),
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct IframeDisplayItem {
    pub clip_id: ClipId,
    pub pipeline_id: PipelineId,
    pub ignore_missing_pipeline: bool,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageDisplayItem {
    pub image_key: ImageKey,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub image_rendering: ImageRendering,
    pub alpha_type: AlphaType,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ImageRendering {
    Auto = 0,
    CrispEdges = 1,
    Pixelated = 2,
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum AlphaType {
    Alpha = 0,
    PremultipliedAlpha = 1,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct YuvImageDisplayItem {
    pub yuv_data: YuvData,
    pub color_space: YuvColorSpace,
    pub image_rendering: ImageRendering,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum YuvColorSpace {
    Rec601 = 0,
    Rec709 = 1,
}
pub const YUV_COLOR_SPACES: [YuvColorSpace; 2] = [YuvColorSpace::Rec601, YuvColorSpace::Rec709];

impl YuvColorSpace {
    pub fn get_feature_string(&self) -> &'static str {
        match *self {
            YuvColorSpace::Rec601 => "YUV_REC601",
            YuvColorSpace::Rec709 => "YUV_REC709",
        }
    }
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum YuvData {
    NV12(ImageKey, ImageKey), // (Y channel, CbCr interleaved channel)
    PlanarYCbCr(ImageKey, ImageKey, ImageKey), // (Y channel, Cb channel, Cr Channel)
    InterleavedYCbCr(ImageKey), // (YCbCr interleaved channel)
}

impl YuvData {
    pub fn get_format(&self) -> YuvFormat {
        match *self {
            YuvData::NV12(..) => YuvFormat::NV12,
            YuvData::PlanarYCbCr(..) => YuvFormat::PlanarYCbCr,
            YuvData::InterleavedYCbCr(..) => YuvFormat::InterleavedYCbCr,
        }
    }
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum YuvFormat {
    NV12 = 0,
    PlanarYCbCr = 1,
    InterleavedYCbCr = 2,
}
pub const YUV_FORMATS: [YuvFormat; 3] = [
    YuvFormat::NV12,
    YuvFormat::PlanarYCbCr,
    YuvFormat::InterleavedYCbCr,
];

impl YuvFormat {
    pub fn get_plane_num(&self) -> usize {
        match *self {
            YuvFormat::NV12 => 2,
            YuvFormat::PlanarYCbCr => 3,
            YuvFormat::InterleavedYCbCr => 1,
        }
    }

    pub fn get_feature_string(&self) -> &'static str {
        match *self {
            YuvFormat::NV12 => "YUV_NV12",
            YuvFormat::PlanarYCbCr => "YUV_PLANAR",
            YuvFormat::InterleavedYCbCr => "YUV_INTERLEAVED",
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageMask {
    pub image: ImageKey,
    pub rect: LayoutRect,
    pub repeat: bool,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum LocalClip {
    Rect(LayoutRect),
    RoundedRect(LayoutRect, ComplexClipRegion),
}

impl From<LayoutRect> for LocalClip {
    fn from(rect: LayoutRect) -> Self {
        LocalClip::Rect(rect)
    }
}

impl LocalClip {
    pub fn clip_rect(&self) -> &LayoutRect {
        match *self {
            LocalClip::Rect(ref rect) => rect,
            LocalClip::RoundedRect(ref rect, _) => rect,
        }
    }

    pub fn create_with_offset(&self, offset: &LayoutVector2D) -> LocalClip {
        match *self {
            LocalClip::Rect(rect) => LocalClip::from(rect.translate(offset)),
            LocalClip::RoundedRect(rect, complex) => LocalClip::RoundedRect(
                rect.translate(offset),
                ComplexClipRegion {
                    rect: complex.rect.translate(offset),
                    radii: complex.radii,
                    mode: complex.mode,
                },
            ),
        }
    }

    pub fn clip_by(&self, rect: &LayoutRect) -> LocalClip {
        match *self {
            LocalClip::Rect(clip_rect) => {
                LocalClip::Rect(
                    clip_rect.intersection(rect).unwrap_or_else(LayoutRect::zero)
                )
            }
            LocalClip::RoundedRect(clip_rect, complex) => {
                LocalClip::RoundedRect(
                    clip_rect.intersection(rect).unwrap_or_else(LayoutRect::zero),
                    complex,
                )
            }
        }
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Serialize, Deserialize)]
pub enum ClipMode {
    Clip,    // Pixels inside the region are visible.
    ClipOut, // Pixels outside the region are visible.
}

impl Not for ClipMode {
    type Output = ClipMode;

    fn not(self) -> ClipMode {
        match self {
            ClipMode::Clip => ClipMode::ClipOut,
            ClipMode::ClipOut => ClipMode::Clip,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ComplexClipRegion {
    /// The boundaries of the rectangle.
    pub rect: LayoutRect,
    /// Border radii of this rectangle.
    pub radii: BorderRadius,
    /// Whether we are clipping inside or outside
    /// the region.
    pub mode: ClipMode,
}

impl BorderRadius {
    pub fn zero() -> BorderRadius {
        BorderRadius {
            top_left: LayoutSize::new(0.0, 0.0),
            top_right: LayoutSize::new(0.0, 0.0),
            bottom_left: LayoutSize::new(0.0, 0.0),
            bottom_right: LayoutSize::new(0.0, 0.0),
        }
    }

    pub fn uniform(radius: f32) -> BorderRadius {
        BorderRadius {
            top_left: LayoutSize::new(radius, radius),
            top_right: LayoutSize::new(radius, radius),
            bottom_left: LayoutSize::new(radius, radius),
            bottom_right: LayoutSize::new(radius, radius),
        }
    }

    pub fn uniform_size(radius: LayoutSize) -> BorderRadius {
        BorderRadius {
            top_left: radius,
            top_right: radius,
            bottom_left: radius,
            bottom_right: radius,
        }
    }

    pub fn is_uniform(&self) -> Option<f32> {
        match self.is_uniform_size() {
            Some(radius) if radius.width == radius.height => Some(radius.width),
            _ => None,
        }
    }

    pub fn is_uniform_size(&self) -> Option<LayoutSize> {
        let uniform_radius = self.top_left;
        if self.top_right == uniform_radius && self.bottom_left == uniform_radius &&
            self.bottom_right == uniform_radius
        {
            Some(uniform_radius)
        } else {
            None
        }
    }

    pub fn is_zero(&self) -> bool {
        if let Some(radius) = self.is_uniform() {
            radius == 0.0
        } else {
            false
        }
    }
}

impl ComplexClipRegion {
    /// Create a new complex clip region.
    pub fn new(
        rect: LayoutRect,
        radii: BorderRadius,
        mode: ClipMode,
    ) -> Self {
        ComplexClipRegion { rect, radii, mode }
    }
}


#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ClipChainId(pub u64, pub PipelineId);

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ClipId {
    Clip(usize, PipelineId),
    ClipChain(ClipChainId),
}

const ROOT_REFERENCE_FRAME_CLIP_ID: usize = 0;
const ROOT_SCROLL_NODE_CLIP_ID: usize = 1;

impl ClipId {
    pub fn root_scroll_node(pipeline_id: PipelineId) -> ClipId {
        ClipId::Clip(ROOT_SCROLL_NODE_CLIP_ID, pipeline_id)
    }

    pub fn root_reference_frame(pipeline_id: PipelineId) -> ClipId {
        ClipId::Clip(ROOT_REFERENCE_FRAME_CLIP_ID, pipeline_id)
    }

    pub fn pipeline_id(&self) -> PipelineId {
        match *self {
            ClipId::Clip(_, pipeline_id) |
            ClipId::ClipChain(ClipChainId(_, pipeline_id)) => pipeline_id,
        }
    }

    pub fn is_root_scroll_node(&self) -> bool {
        match *self {
            ClipId::Clip(1, _) => true,
            _ => false,
        }
    }

    pub fn is_root_reference_frame(&self) -> bool {
        match *self {
            ClipId::Clip(1, _) => true,
            _ => false,
        }
    }
}

/// An external identifier that uniquely identifies a scroll frame independent of its ClipId, which
/// may change from frame to frame. This should be unique within a pipeline. WebRender makes no
/// attempt to ensure uniqueness. The zero value is reserved for use by the root scroll node of
/// every pipeline, which always has an external id.
///
/// When setting display lists with the `preserve_frame_state` this id is used to preserve scroll
/// offsets between different sets of ClipScrollNodes which are ScrollFrames.
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ExternalScrollId(pub u64, pub PipelineId);

impl ExternalScrollId {
    pub fn pipeline_id(&self) -> PipelineId {
        self.1
    }

    pub fn is_root(&self) -> bool {
        self.0 == 0
    }
}

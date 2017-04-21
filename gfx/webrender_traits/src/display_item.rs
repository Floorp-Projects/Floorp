/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use app_units::Au;
use euclid::SideOffsets2D;
use display_list::AuxiliaryListsBuilder;
use {ColorF, FontKey, ImageKey, PipelineId, WebGLContextId};
use {LayoutPoint, LayoutRect, LayoutSize, LayoutTransform};
use {PropertyBinding};

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct DisplayItem {
    pub item: SpecificDisplayItem,
    pub rect: LayoutRect,
    pub clip: ClipRegion,
    pub clip_id: ClipId,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum SpecificDisplayItem {
    Clip(ClipDisplayItem),
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
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct ItemRange {
    pub start: usize,
    pub length: usize,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ClipDisplayItem {
    pub id: ClipId,
    pub parent_id: ClipId,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RectangleDisplayItem {
    pub color: ColorF,
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

#[derive(Clone, Copy, Debug, Deserialize, Hash, Eq, PartialEq, PartialOrd, Ord, Serialize)]
pub struct GlyphOptions {
    // These are currently only used on windows for dwrite fonts.
    pub use_embedded_bitmap: bool,
    pub force_gdi_rendering: bool,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct WebGLDisplayItem {
    pub context_id: WebGLContextId,
}


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
pub struct GradientBorder {
    pub gradient: Gradient,
    pub outset: SideOffsets2D<f32>,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RadialGradientBorder {
    pub gradient: RadialGradient,
    pub outset: SideOffsets2D<f32>,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum BorderDetails {
    Normal(NormalBorder),
    Image(ImageBorder),
    Gradient(GradientBorder),
    RadialGradient(RadialGradientBorder),
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
    pub stops: ItemRange,
    pub extend_mode: ExtendMode,
}

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
known_heap_size!(0, GradientStop);

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RadialGradient {
    pub start_center: LayoutPoint,
    pub start_radius: f32,
    pub end_center: LayoutPoint,
    pub end_radius: f32,
    pub ratio_xy: f32,
    pub stops: ItemRange,
    pub extend_mode: ExtendMode,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct RadialGradientDisplayItem {
    pub gradient: RadialGradient,
    pub tile_size: LayoutSize,
    pub tile_spacing: LayoutSize,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PushStackingContextDisplayItem {
    pub stacking_context: StackingContext,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct StackingContext {
    pub scroll_policy: ScrollPolicy,
    pub z_index: i32,
    pub transform: Option<PropertyBinding<LayoutTransform>>,
    pub transform_style: TransformStyle,
    pub perspective: Option<LayoutTransform>,
    pub mix_blend_mode: MixBlendMode,
    pub filters: ItemRange,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, PartialEq, Serialize)]
pub enum ScrollPolicy {
    Scrollable  = 0,
    Fixed       = 1,
}

known_heap_size!(0, ScrollPolicy);

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum TransformStyle {
    Flat,
    Preserve3D,
}

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

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct IframeDisplayItem {
    pub pipeline_id: PipelineId,
}


#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct ImageDisplayItem {
    pub image_key: ImageKey,
    pub stretch_size: LayoutSize,
    pub tile_spacing: LayoutSize,
    pub image_rendering: ImageRendering,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ImageRendering {
    Auto        = 0,
    CrispEdges  = 1,
    Pixelated   = 2,
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct YuvImageDisplayItem {
    pub y_image_key: ImageKey,
    pub u_image_key: ImageKey,
    pub v_image_key: ImageKey,
    pub color_space: YuvColorSpace,
}

#[repr(u32)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum YuvColorSpace {
    Rec601 = 1, // The values must match the ones in prim_shared.glsl
    Rec709 = 2,
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

impl StackingContext {
    pub fn new(scroll_policy: ScrollPolicy,
               z_index: i32,
               transform: Option<PropertyBinding<LayoutTransform>>,
               transform_style: TransformStyle,
               perspective: Option<LayoutTransform>,
               mix_blend_mode: MixBlendMode,
               filters: Vec<FilterOp>,
               auxiliary_lists_builder: &mut AuxiliaryListsBuilder)
               -> StackingContext {
        StackingContext {
            scroll_policy: scroll_policy,
            z_index: z_index,
            transform: transform,
            transform_style: transform_style,
            perspective: perspective,
            mix_blend_mode: mix_blend_mode,
            filters: auxiliary_lists_builder.add_filters(&filters),
        }
    }
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
            _ => None
        }
    }

    pub fn is_uniform_size(&self) -> Option<LayoutSize> {
        let uniform_radius = self.top_left;
        if self.top_right == uniform_radius &&
           self.bottom_left == uniform_radius &&
           self.bottom_right == uniform_radius {
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

impl ClipRegion {
    pub fn new(rect: &LayoutRect,
               complex: Vec<ComplexClipRegion>,
               image_mask: Option<ImageMask>,
               auxiliary_lists_builder: &mut AuxiliaryListsBuilder)
               -> ClipRegion {
        ClipRegion {
            main: *rect,
            complex: auxiliary_lists_builder.add_complex_clip_regions(&complex),
            image_mask: image_mask,
        }
    }

    pub fn simple(rect: &LayoutRect) -> ClipRegion {
        ClipRegion {
            main: *rect,
            complex: ItemRange::empty(),
            image_mask: None,
        }
    }

    pub fn is_complex(&self) -> bool {
        self.complex.length !=0 || self.image_mask.is_some()
    }
}

impl ColorF {
    pub fn new(r: f32, g: f32, b: f32, a: f32) -> ColorF {
        ColorF {
            r: r,
            g: g,
            b: b,
            a: a,
        }
    }

    pub fn scale_rgb(&self, scale: f32) -> ColorF {
        ColorF {
            r: self.r * scale,
            g: self.g * scale,
            b: self.b * scale,
            a: self.a,
        }
    }

    pub fn to_array(&self) -> [f32; 4] {
        [self.r, self.g, self.b, self.a]
    }
}

impl ComplexClipRegion {
    /// Create a new complex clip region.
    pub fn new(rect: LayoutRect, radii: BorderRadius) -> ComplexClipRegion {
        ComplexClipRegion {
            rect: rect,
            radii: radii,
        }
    }
}

#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub enum ClipId {
    Clip(u64, PipelineId),
    ClipExternalId(u64, PipelineId),
    ReferenceFrame(u64, PipelineId),
}

impl ClipId {
    pub fn root_scroll_node(pipeline_id: PipelineId) -> ClipId {
        ClipId::Clip(0, pipeline_id)
    }

    pub fn root_reference_frame(pipeline_id: PipelineId) -> ClipId {
        ClipId::ReferenceFrame(0, pipeline_id)
    }

    pub fn new(id: u64, pipeline_id: PipelineId) -> ClipId {
        ClipId::ClipExternalId(id, pipeline_id)
    }

    pub fn pipeline_id(&self) -> PipelineId {
        match *self {
            ClipId::Clip(_, pipeline_id) |
            ClipId::ClipExternalId(_, pipeline_id) |
            ClipId::ReferenceFrame(_, pipeline_id) => pipeline_id,
        }
    }

    pub fn is_reference_frame(&self) -> bool {
        match *self {
            ClipId::ReferenceFrame(..) => true,
            _ => false,
        }
    }

    pub fn external_id(&self) -> Option<u64> {
        match *self {
            ClipId::ClipExternalId(id, _) => Some(id),
            _ => None,
        }
    }

    pub fn is_root_scroll_node(&self) -> bool {
        match *self {
            ClipId::Clip(id, _) if id == 0 => true,
            _ => false,
        }
    }
}

macro_rules! define_empty_heap_size_of {
    ($name:ident) => {
        impl ::heapsize::HeapSizeOf for $name {
            fn heap_size_of_children(&self) -> usize { 0 }
        }
    }
}

define_empty_heap_size_of!(ClipId);
define_empty_heap_size_of!(RepeatMode);
define_empty_heap_size_of!(ImageKey);

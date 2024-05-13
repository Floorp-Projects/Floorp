/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{
    ColorU, MixBlendMode, FilterPrimitiveInput, FilterPrimitiveKind,
    ColorSpace, PropertyBinding, PropertyBindingId, CompositeOperator,
    RasterSpace, FilterOpGraphPictureBufferId,
};
use api::units::Au;
use crate::scene_building::IsVisible;
use crate::filterdata::SFilterData;
use crate::intern::ItemUid;
use crate::intern::{Internable, InternDebug, Handle as InternHandle};
use crate::internal_types::{LayoutPrimitiveInfo, FilterGraphPictureReference,
    FilterGraphOp, FilterGraphNode, SVGFE_CONVOLVE_VALUES_LIMIT, Filter};
use crate::picture::PictureCompositeMode;
use crate::prim_store::{
    PrimitiveInstanceKind, PrimitiveStore, VectorKey,
    InternablePrimitive,
};

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq, Hash, Eq)]
pub enum CompositeOperatorKey {
    Over,
    In,
    Out,
    Atop,
    Xor,
    Lighter,
    Arithmetic([Au; 4]),
}

impl From<CompositeOperator> for CompositeOperatorKey {
    fn from(operator: CompositeOperator) -> Self {
        match operator {
            CompositeOperator::Over => CompositeOperatorKey::Over,
            CompositeOperator::In => CompositeOperatorKey::In,
            CompositeOperator::Out => CompositeOperatorKey::Out,
            CompositeOperator::Atop => CompositeOperatorKey::Atop,
            CompositeOperator::Xor => CompositeOperatorKey::Xor,
            CompositeOperator::Lighter => CompositeOperatorKey::Lighter,
            CompositeOperator::Arithmetic(k_vals) => {
                let k_vals = [
                    Au::from_f32_px(k_vals[0]),
                    Au::from_f32_px(k_vals[1]),
                    Au::from_f32_px(k_vals[2]),
                    Au::from_f32_px(k_vals[3]),
                ];
                CompositeOperatorKey::Arithmetic(k_vals)
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq, Hash, Eq)]
pub enum FilterPrimitiveKey {
    Identity(ColorSpace, FilterPrimitiveInput),
    Flood(ColorSpace, ColorU),
    Blend(ColorSpace, MixBlendMode, FilterPrimitiveInput, FilterPrimitiveInput),
    Blur(ColorSpace, Au, Au, FilterPrimitiveInput),
    Opacity(ColorSpace, Au, FilterPrimitiveInput),
    ColorMatrix(ColorSpace, [Au; 20], FilterPrimitiveInput),
    DropShadow(ColorSpace, (VectorKey, Au, ColorU), FilterPrimitiveInput),
    ComponentTransfer(ColorSpace, FilterPrimitiveInput, Vec<SFilterData>),
    Offset(ColorSpace, FilterPrimitiveInput, VectorKey),
    Composite(ColorSpace, FilterPrimitiveInput, FilterPrimitiveInput, CompositeOperatorKey),
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Copy, Default, MallocSizeOf, PartialEq, Hash, Eq)]
pub enum FilterGraphPictureBufferIdKey {
    #[default]
    /// empty slot in feMerge inputs
    None,
    /// reference to another (earlier) node in filter graph
    BufferId(i16),
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Copy, Default, MallocSizeOf, PartialEq, Hash, Eq)]
pub struct FilterGraphPictureReferenceKey {
    /// Id of the picture in question in a namespace unique to this filter DAG,
    /// some are special values like
    /// FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic.
    pub buffer_id: FilterGraphPictureBufferIdKey,
    /// Place the input image here in Layout space (like node.subregion)
    pub subregion: [Au; 4],
    /// Translate the subregion by this amount
    pub offset: [Au; 2],
}

impl From<FilterGraphPictureReference> for FilterGraphPictureReferenceKey {
    fn from(pic: FilterGraphPictureReference) -> Self {
        FilterGraphPictureReferenceKey{
            buffer_id: match pic.buffer_id {
                FilterOpGraphPictureBufferId::None => FilterGraphPictureBufferIdKey::None,
                FilterOpGraphPictureBufferId::BufferId(id) => FilterGraphPictureBufferIdKey::BufferId(id),
            },
            subregion: [
                Au::from_f32_px(pic.subregion.min.x),
                Au::from_f32_px(pic.subregion.min.y),
                Au::from_f32_px(pic.subregion.max.x),
                Au::from_f32_px(pic.subregion.max.y),
            ],
            offset: [
                Au::from_f32_px(pic.offset.x),
                Au::from_f32_px(pic.offset.y),
            ],
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq, Hash, Eq)]
pub enum FilterGraphOpKey {
    /// combine 2 images with SVG_FEBLEND_MODE_DARKEN
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendDarken,
    /// combine 2 images with SVG_FEBLEND_MODE_LIGHTEN
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendLighten,
    /// combine 2 images with SVG_FEBLEND_MODE_MULTIPLY
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendMultiply,
    /// combine 2 images with SVG_FEBLEND_MODE_NORMAL
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendNormal,
    /// combine 2 images with SVG_FEBLEND_MODE_SCREEN
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendScreen,
    /// combine 2 images with SVG_FEBLEND_MODE_OVERLAY
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendOverlay,
    /// combine 2 images with SVG_FEBLEND_MODE_COLOR_DODGE
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendColorDodge,
    /// combine 2 images with SVG_FEBLEND_MODE_COLOR_BURN
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendColorBurn,
    /// combine 2 images with SVG_FEBLEND_MODE_HARD_LIGHT
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendHardLight,
    /// combine 2 images with SVG_FEBLEND_MODE_SOFT_LIGHT
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendSoftLight,
    /// combine 2 images with SVG_FEBLEND_MODE_DIFFERENCE
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendDifference,
    /// combine 2 images with SVG_FEBLEND_MODE_EXCLUSION
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendExclusion,
    /// combine 2 images with SVG_FEBLEND_MODE_HUE
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendHue,
    /// combine 2 images with SVG_FEBLEND_MODE_SATURATION
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendSaturation,
    /// combine 2 images with SVG_FEBLEND_MODE_COLOR
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendColor,
    /// combine 2 images with SVG_FEBLEND_MODE_LUMINOSITY
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Source: https://developer.mozilla.org/en-US/docs/Web/CSS/mix-blend-mode
    SVGFEBlendLuminosity,
    /// transform colors of image through 5x4 color matrix (transposed for
    /// efficiency)
    /// parameters: FilterOpGraphNode, matrix[5][4]
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feColorMatrixElement
    SVGFEColorMatrix{values: [Au; 20]},
    /// transform colors of image through configurable gradients with component
    /// swizzle
    /// parameters: FilterOpGraphNode, FilterData
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feComponentTransferElement
    SVGFEComponentTransferInterned{handle: ItemUid, creates_pixels: bool},
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterOpGraphNode, k1, k2, k3, k4
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeArithmetic{k1: Au, k2: Au, k3: Au, k4: Au},
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeATop,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeIn,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Docs: https://developer.mozilla.org/en-US/docs/Web/SVG/Element/feComposite
    SVGFECompositeLighter,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeOut,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeOver,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeXOR,
    /// transform image through convolution matrix of up to 25 values (spec
    /// allows more but for performance reasons we do not)
    /// parameters: FilterOpGraphNode, orderX, orderY, kernelValues[25],
    ///  divisor, bias, targetX, targetY, kernelUnitLengthX, kernelUnitLengthY,
    ///  preserveAlpha
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feConvolveMatrixElement
    SVGFEConvolveMatrixEdgeModeDuplicate{order_x: i32, order_y: i32,
        kernel: [Au; SVGFE_CONVOLVE_VALUES_LIMIT], divisor: Au, bias: Au,
        target_x: i32, target_y: i32, kernel_unit_length_x: Au,
        kernel_unit_length_y: Au, preserve_alpha: i32},
    /// transform image through convolution matrix of up to 25 values (spec
    /// allows more but for performance reasons we do not)
    /// parameters: FilterOpGraphNode, orderX, orderY, kernelValues[25],
    /// divisor, bias, targetX, targetY, kernelUnitLengthX, kernelUnitLengthY,
    /// preserveAlpha
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feConvolveMatrixElement
    SVGFEConvolveMatrixEdgeModeNone{order_x: i32, order_y: i32,
        kernel: [Au; SVGFE_CONVOLVE_VALUES_LIMIT], divisor: Au, bias: Au,
        target_x: i32, target_y: i32, kernel_unit_length_x: Au,
        kernel_unit_length_y: Au, preserve_alpha: i32},
    /// transform image through convolution matrix of up to 25 values (spec
    /// allows more but for performance reasons we do not)
    /// parameters: FilterOpGraphNode, orderX, orderY, kernelValues[25],
    ///  divisor, bias, targetX, targetY, kernelUnitLengthX, kernelUnitLengthY,
    ///  preserveAlpha
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feConvolveMatrixElement
    SVGFEConvolveMatrixEdgeModeWrap{order_x: i32, order_y: i32,
        kernel: [Au; SVGFE_CONVOLVE_VALUES_LIMIT], divisor: Au, bias: Au,
        target_x: i32, target_y: i32, kernel_unit_length_x: Au,
        kernel_unit_length_y: Au, preserve_alpha: i32},
    /// calculate lighting based on heightmap image with provided values for a
    /// distant light source with specified direction
    /// parameters: FilterOpGraphNode, surfaceScale, diffuseConstant,
    ///  kernelUnitLengthX, kernelUnitLengthY, azimuth, elevation
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDiffuseLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDistantLightElement
    SVGFEDiffuseLightingDistant{surface_scale: Au, diffuse_constant: Au,
        kernel_unit_length_x: Au, kernel_unit_length_y: Au, azimuth: Au,
        elevation: Au},
    /// calculate lighting based on heightmap image with provided values for a
    /// point light source at specified location
    /// parameters: FilterOpGraphNode, surfaceScale, diffuseConstant,
    ///  kernelUnitLengthX, kernelUnitLengthY, x, y, z
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDiffuseLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEPointLightElement
    SVGFEDiffuseLightingPoint{surface_scale: Au, diffuse_constant: Au,
        kernel_unit_length_x: Au, kernel_unit_length_y: Au, x: Au, y: Au,
        z: Au},
    /// calculate lighting based on heightmap image with provided values for a
    /// spot light source at specified location pointing at specified target
    /// location with specified hotspot sharpness and cone angle
    /// parameters: FilterOpGraphNode, surfaceScale, diffuseConstant,
    ///  kernelUnitLengthX, kernelUnitLengthY, x, y, z, pointsAtX, pointsAtY,
    ///  pointsAtZ, specularExponent, limitingConeAngle
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDiffuseLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpotLightElement
    SVGFEDiffuseLightingSpot{surface_scale: Au, diffuse_constant: Au,
        kernel_unit_length_x: Au, kernel_unit_length_y: Au, x: Au, y: Au, z: Au,
        points_at_x: Au, points_at_y: Au, points_at_z: Au, cone_exponent: Au,
        limiting_cone_angle: Au},
    /// calculate a distorted version of first input image using offset values
    /// from second input image at specified intensity
    /// parameters: FilterOpGraphNode, scale, xChannelSelector, yChannelSelector
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDisplacementMapElement
    SVGFEDisplacementMap{scale: Au, x_channel_selector: u32,
        y_channel_selector: u32},
    /// create and merge a dropshadow version of the specified image's alpha
    /// channel with specified offset and blur radius
    /// parameters: FilterOpGraphNode, flood_color, flood_opacity, dx, dy,
    ///  stdDeviationX, stdDeviationY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDropShadowElement
    SVGFEDropShadow{color: ColorU, dx: Au, dy: Au, std_deviation_x: Au,
        std_deviation_y: Au},
    /// synthesize a new image of specified size containing a solid color
    /// parameters: FilterOpGraphNode, color
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEFloodElement
    SVGFEFlood{color: ColorU},
    /// create a blurred version of the input image
    /// parameters: FilterOpGraphNode, stdDeviationX, stdDeviationY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEGaussianBlurElement
    SVGFEGaussianBlur{std_deviation_x: Au, std_deviation_y: Au},
    /// Filter that does no transformation of the colors, needed for
    /// debug purposes, and is the default value in impl_default_for_enums.
    SVGFEIdentity,
    /// synthesize a new image based on a url (i.e. blob image source)
    /// parameters: FilterOpGraphNode, sampling_filter (see SamplingFilter in
    /// Types.h), transform
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEImageElement
    SVGFEImage{sampling_filter: u32, matrix: [Au; 6]},
    /// create a new image based on the input image with the contour stretched
    /// outward (dilate operator)
    /// parameters: FilterOpGraphNode, radiusX, radiusY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEMorphologyElement
    SVGFEMorphologyDilate{radius_x: Au, radius_y: Au},
    /// create a new image based on the input image with the contour shrunken
    /// inward (erode operator)
    /// parameters: FilterOpGraphNode, radiusX, radiusY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEMorphologyElement
    SVGFEMorphologyErode{radius_x: Au, radius_y: Au},
    /// represents CSS opacity property as a graph node like the rest of the
    /// SVGFE* filters
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    SVGFEOpacity{value: Au},
    /// represents CSS opacity property as a graph node like the rest of the
    /// SVGFE* filters
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    SVGFEOpacityBinding{valuebindingid: PropertyBindingId, value: Au},
    /// Filter that copies the SourceGraphic image into the specified subregion,
    /// This is intentionally the only way to get SourceGraphic into the graph,
    /// as the filter region must be applied before it is used.
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - no inputs, no linear
    SVGFESourceGraphic,
    /// Filter that copies the SourceAlpha image into the specified subregion,
    /// This is intentionally the only way to get SourceAlpha into the graph,
    /// as the filter region must be applied before it is used.
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - no inputs, no linear
    SVGFESourceAlpha,
    /// calculate lighting based on heightmap image with provided values for a
    /// distant light source with specified direction
    /// parameters: FilerData, surfaceScale, specularConstant, specularExponent,
    ///  kernelUnitLengthX, kernelUnitLengthY, azimuth, elevation
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpecularLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDistantLightElement
    SVGFESpecularLightingDistant{surface_scale: Au, specular_constant: Au,
        specular_exponent: Au, kernel_unit_length_x: Au,
        kernel_unit_length_y: Au, azimuth: Au, elevation: Au},
    /// calculate lighting based on heightmap image with provided values for a
    /// point light source at specified location
    /// parameters: FilterOpGraphNode, surfaceScale, specularConstant,
    ///  specularExponent, kernelUnitLengthX, kernelUnitLengthY, x, y, z
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpecularLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEPointLightElement
    SVGFESpecularLightingPoint{surface_scale: Au, specular_constant: Au,
        specular_exponent: Au, kernel_unit_length_x: Au,
        kernel_unit_length_y: Au, x: Au, y: Au, z: Au},
    /// calculate lighting based on heightmap image with provided values for a
    /// spot light source at specified location pointing at specified target
    /// location with specified hotspot sharpness and cone angle
    /// parameters: FilterOpGraphNode, surfaceScale, specularConstant,
    ///  specularExponent, kernelUnitLengthX, kernelUnitLengthY, x, y, z,
    ///  pointsAtX, pointsAtY, pointsAtZ, specularExponent, limitingConeAngle
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpecularLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpotLightElement
    SVGFESpecularLightingSpot{surface_scale: Au, specular_constant: Au,
        specular_exponent: Au, kernel_unit_length_x: Au,
        kernel_unit_length_y: Au, x: Au, y: Au, z: Au, points_at_x: Au,
        points_at_y: Au, points_at_z: Au, cone_exponent: Au,
        limiting_cone_angle: Au},
    /// create a new image based on the input image, repeated throughout the
    /// output rectangle
    /// parameters: FilterOpGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETileElement
    SVGFETile,
    /// convert a color image to an alpha channel - internal use; generated by
    /// SVGFilterInstance::GetOrCreateSourceAlphaIndex().
    SVGFEToAlpha,
    /// synthesize a new image based on Fractal Noise (Perlin) with the chosen
    /// stitching mode
    /// parameters: FilterOpGraphNode, baseFrequencyX, baseFrequencyY,
    ///  numOctaves, seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithFractalNoiseWithNoStitching{base_frequency_x: Au,
        base_frequency_y: Au, num_octaves: u32, seed: u32},
    /// synthesize a new image based on Fractal Noise (Perlin) with the chosen
    /// stitching mode
    /// parameters: FilterOpGraphNode, baseFrequencyX, baseFrequencyY,
    ///  numOctaves, seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithFractalNoiseWithStitching{base_frequency_x: Au,
        base_frequency_y: Au, num_octaves: u32, seed: u32},
    /// synthesize a new image based on Turbulence Noise (offset vectors)
    /// parameters: FilterOpGraphNode, baseFrequencyX, baseFrequencyY,
    ///  numOctaves, seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{base_frequency_x: Au,
        base_frequency_y: Au, num_octaves: u32, seed: u32},
    /// synthesize a new image based on Turbulence Noise (offset vectors)
    /// parameters: FilterOpGraphNode, baseFrequencyX, baseFrequencyY,
    ///  numOctaves, seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithTurbulenceNoiseWithStitching{base_frequency_x: Au,
        base_frequency_y: Au, num_octaves: u32, seed: u32},
}

impl From<FilterGraphOp> for FilterGraphOpKey {
    fn from(op: FilterGraphOp) -> Self {
        match op {
            FilterGraphOp::SVGFEBlendDarken => FilterGraphOpKey::SVGFEBlendDarken,
            FilterGraphOp::SVGFEBlendLighten => FilterGraphOpKey::SVGFEBlendLighten,
            FilterGraphOp::SVGFEBlendMultiply => FilterGraphOpKey::SVGFEBlendMultiply,
            FilterGraphOp::SVGFEBlendNormal => FilterGraphOpKey::SVGFEBlendNormal,
            FilterGraphOp::SVGFEBlendScreen => FilterGraphOpKey::SVGFEBlendScreen,
            FilterGraphOp::SVGFEBlendOverlay => FilterGraphOpKey::SVGFEBlendOverlay,
            FilterGraphOp::SVGFEBlendColorDodge => FilterGraphOpKey::SVGFEBlendColorDodge,
            FilterGraphOp::SVGFEBlendColorBurn => FilterGraphOpKey::SVGFEBlendColorBurn,
            FilterGraphOp::SVGFEBlendHardLight => FilterGraphOpKey::SVGFEBlendHardLight,
            FilterGraphOp::SVGFEBlendSoftLight => FilterGraphOpKey::SVGFEBlendSoftLight,
            FilterGraphOp::SVGFEBlendDifference => FilterGraphOpKey::SVGFEBlendDifference,
            FilterGraphOp::SVGFEBlendExclusion => FilterGraphOpKey::SVGFEBlendExclusion,
            FilterGraphOp::SVGFEBlendHue => FilterGraphOpKey::SVGFEBlendHue,
            FilterGraphOp::SVGFEBlendSaturation => FilterGraphOpKey::SVGFEBlendSaturation,
            FilterGraphOp::SVGFEBlendColor => FilterGraphOpKey::SVGFEBlendColor,
            FilterGraphOp::SVGFEBlendLuminosity => FilterGraphOpKey::SVGFEBlendLuminosity,
            FilterGraphOp::SVGFEColorMatrix { values: color_matrix } => {
                let mut quantized_values: [Au; 20] = [Au(0); 20];
                for (value, result) in color_matrix.iter().zip(quantized_values.iter_mut()) {
                    *result = Au::from_f32_px(*value);
                }
                FilterGraphOpKey::SVGFEColorMatrix{values: quantized_values}
            }
            FilterGraphOp::SVGFEComponentTransfer => unreachable!(),
            FilterGraphOp::SVGFEComponentTransferInterned { handle, creates_pixels } => FilterGraphOpKey::SVGFEComponentTransferInterned{
                handle: handle.uid(),
                creates_pixels,
            },
            FilterGraphOp::SVGFECompositeArithmetic { k1, k2, k3, k4 } => {
                FilterGraphOpKey::SVGFECompositeArithmetic{
                    k1: Au::from_f32_px(k1),
                    k2: Au::from_f32_px(k2),
                    k3: Au::from_f32_px(k3),
                    k4: Au::from_f32_px(k4),
                }
            }
            FilterGraphOp::SVGFECompositeATop => FilterGraphOpKey::SVGFECompositeATop,
            FilterGraphOp::SVGFECompositeIn => FilterGraphOpKey::SVGFECompositeIn,
            FilterGraphOp::SVGFECompositeLighter => FilterGraphOpKey::SVGFECompositeLighter,
            FilterGraphOp::SVGFECompositeOut => FilterGraphOpKey::SVGFECompositeOut,
            FilterGraphOp::SVGFECompositeOver => FilterGraphOpKey::SVGFECompositeOver,
            FilterGraphOp::SVGFECompositeXOR => FilterGraphOpKey::SVGFECompositeXOR,
            FilterGraphOp::SVGFEConvolveMatrixEdgeModeDuplicate { order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha } => {
                let mut values: [Au; SVGFE_CONVOLVE_VALUES_LIMIT] = [Au(0); SVGFE_CONVOLVE_VALUES_LIMIT];
                for (value, result) in kernel.iter().zip(values.iter_mut()) {
                    *result = Au::from_f32_px(*value)
                }
                FilterGraphOpKey::SVGFEConvolveMatrixEdgeModeDuplicate{
                    order_x,
                    order_y,
                    kernel: values,
                    divisor: Au::from_f32_px(divisor),
                    bias: Au::from_f32_px(bias),
                    target_x,
                    target_y,
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    preserve_alpha,
                }
            }
            FilterGraphOp::SVGFEConvolveMatrixEdgeModeNone { order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha } => {
                let mut values: [Au; SVGFE_CONVOLVE_VALUES_LIMIT] = [Au(0); SVGFE_CONVOLVE_VALUES_LIMIT];
                for (value, result) in kernel.iter().zip(values.iter_mut()) {
                    *result = Au::from_f32_px(*value)
                }
                FilterGraphOpKey::SVGFEConvolveMatrixEdgeModeNone{
                    order_x,
                    order_y,
                    kernel: values,
                    divisor: Au::from_f32_px(divisor),
                    bias: Au::from_f32_px(bias),
                    target_x,
                    target_y,
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    preserve_alpha,
                }
            }
            FilterGraphOp::SVGFEConvolveMatrixEdgeModeWrap { order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha } => {
                let mut values: [Au; SVGFE_CONVOLVE_VALUES_LIMIT] = [Au(0); SVGFE_CONVOLVE_VALUES_LIMIT];
                for (value, result) in kernel.iter().zip(values.iter_mut()) {
                    *result = Au::from_f32_px(*value)
                }
                FilterGraphOpKey::SVGFEConvolveMatrixEdgeModeWrap{
                    order_x,
                    order_y,
                    kernel: values,
                    divisor: Au::from_f32_px(divisor),
                    bias: Au::from_f32_px(bias),
                    target_x,
                    target_y,
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    preserve_alpha,
                }
            }
            FilterGraphOp::SVGFEDiffuseLightingDistant { surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, azimuth, elevation } => {
                FilterGraphOpKey::SVGFEDiffuseLightingDistant{
                    surface_scale: Au::from_f32_px(surface_scale),
                    diffuse_constant: Au::from_f32_px(diffuse_constant),
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    azimuth: Au::from_f32_px(azimuth),
                    elevation: Au::from_f32_px(elevation),
                }
            }
            FilterGraphOp::SVGFEDiffuseLightingPoint { surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, x, y, z } => {
                FilterGraphOpKey::SVGFEDiffuseLightingPoint{
                    surface_scale: Au::from_f32_px(surface_scale),
                    diffuse_constant: Au::from_f32_px(diffuse_constant),
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    x: Au::from_f32_px(x),
                    y: Au::from_f32_px(y),
                    z: Au::from_f32_px(z),
                }
            }
            FilterGraphOp::SVGFEDiffuseLightingSpot { surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, x, y, z, points_at_x, points_at_y, points_at_z, cone_exponent, limiting_cone_angle } => {
                FilterGraphOpKey::SVGFEDiffuseLightingSpot{
                    surface_scale: Au::from_f32_px(surface_scale),
                    diffuse_constant: Au::from_f32_px(diffuse_constant),
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    x: Au::from_f32_px(x),
                    y: Au::from_f32_px(y),
                    z: Au::from_f32_px(z),
                    points_at_x: Au::from_f32_px(points_at_x),
                    points_at_y: Au::from_f32_px(points_at_y),
                    points_at_z: Au::from_f32_px(points_at_z),
                    cone_exponent: Au::from_f32_px(cone_exponent),
                    limiting_cone_angle: Au::from_f32_px(limiting_cone_angle),
                }
            }
            FilterGraphOp::SVGFEDisplacementMap { scale, x_channel_selector, y_channel_selector } => {
                FilterGraphOpKey::SVGFEDisplacementMap{
                    scale: Au::from_f32_px(scale),
                    x_channel_selector,
                    y_channel_selector,
                }
            }
            FilterGraphOp::SVGFEDropShadow { color, dx, dy, std_deviation_x, std_deviation_y } => {
                FilterGraphOpKey::SVGFEDropShadow{
                    color: color.into(),
                    dx: Au::from_f32_px(dx),
                    dy: Au::from_f32_px(dy),
                    std_deviation_x: Au::from_f32_px(std_deviation_x),
                    std_deviation_y: Au::from_f32_px(std_deviation_y),
                }
            }
            FilterGraphOp::SVGFEFlood { color } => FilterGraphOpKey::SVGFEFlood{color: color.into()},
            FilterGraphOp::SVGFEGaussianBlur { std_deviation_x, std_deviation_y } => {
                FilterGraphOpKey::SVGFEGaussianBlur{
                    std_deviation_x: Au::from_f32_px(std_deviation_x),
                    std_deviation_y: Au::from_f32_px(std_deviation_y),
                }
            }
            FilterGraphOp::SVGFEIdentity => FilterGraphOpKey::SVGFEIdentity,
            FilterGraphOp::SVGFEImage { sampling_filter, matrix } => {
                let mut values: [Au; 6] = [Au(0); 6];
                for (value, result) in matrix.iter().zip(values.iter_mut()) {
                    *result = Au::from_f32_px(*value)
                }
                FilterGraphOpKey::SVGFEImage{
                    sampling_filter,
                    matrix: values,
                }
            }
            FilterGraphOp::SVGFEMorphologyDilate { radius_x, radius_y } => {
                FilterGraphOpKey::SVGFEMorphologyDilate{
                    radius_x: Au::from_f32_px(radius_x),
                    radius_y: Au::from_f32_px(radius_y),
                }
            }
            FilterGraphOp::SVGFEMorphologyErode { radius_x, radius_y } => {
                FilterGraphOpKey::SVGFEMorphologyErode{
                    radius_x: Au::from_f32_px(radius_x),
                    radius_y: Au::from_f32_px(radius_y),
                }
            }
            FilterGraphOp::SVGFEOpacity{valuebinding: binding, value: _} => {
                match binding {
                    PropertyBinding::Value(value) => {
                        FilterGraphOpKey::SVGFEOpacity{value: Au::from_f32_px(value)}
                    }
                    PropertyBinding::Binding(key, default) => {
                        FilterGraphOpKey::SVGFEOpacityBinding{valuebindingid: key.id, value: Au::from_f32_px(default)}
                    }
                }
            }
            FilterGraphOp::SVGFESourceAlpha => FilterGraphOpKey::SVGFESourceAlpha,
            FilterGraphOp::SVGFESourceGraphic => FilterGraphOpKey::SVGFESourceGraphic,
            FilterGraphOp::SVGFESpecularLightingDistant { surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, azimuth, elevation } => {
                FilterGraphOpKey::SVGFESpecularLightingDistant{
                    surface_scale: Au::from_f32_px(surface_scale),
                    specular_constant: Au::from_f32_px(specular_constant),
                    specular_exponent: Au::from_f32_px(specular_exponent),
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    azimuth: Au::from_f32_px(azimuth),
                    elevation: Au::from_f32_px(elevation),
                }
            }
            FilterGraphOp::SVGFESpecularLightingPoint { surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, x, y, z } => {
                FilterGraphOpKey::SVGFESpecularLightingPoint{
                    surface_scale: Au::from_f32_px(surface_scale),
                    specular_constant: Au::from_f32_px(specular_constant),
                    specular_exponent: Au::from_f32_px(specular_exponent),
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    x: Au::from_f32_px(x),
                    y: Au::from_f32_px(y),
                    z: Au::from_f32_px(z),
                }
            }
            FilterGraphOp::SVGFESpecularLightingSpot { surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, x, y, z, points_at_x, points_at_y, points_at_z, cone_exponent, limiting_cone_angle } => {
                FilterGraphOpKey::SVGFESpecularLightingSpot{
                    surface_scale: Au::from_f32_px(surface_scale),
                    specular_constant: Au::from_f32_px(specular_constant),
                    specular_exponent: Au::from_f32_px(specular_exponent),
                    kernel_unit_length_x: Au::from_f32_px(kernel_unit_length_x),
                    kernel_unit_length_y: Au::from_f32_px(kernel_unit_length_y),
                    x: Au::from_f32_px(x),
                    y: Au::from_f32_px(y),
                    z: Au::from_f32_px(z),
                    points_at_x: Au::from_f32_px(points_at_x),
                    points_at_y: Au::from_f32_px(points_at_y),
                    points_at_z: Au::from_f32_px(points_at_z),
                    cone_exponent: Au::from_f32_px(cone_exponent),
                    limiting_cone_angle: Au::from_f32_px(limiting_cone_angle),
                }
            }
            FilterGraphOp::SVGFETile => FilterGraphOpKey::SVGFETile,
            FilterGraphOp::SVGFEToAlpha => FilterGraphOpKey::SVGFEToAlpha,
            FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching { base_frequency_x, base_frequency_y, num_octaves, seed } => {
                FilterGraphOpKey::SVGFETurbulenceWithFractalNoiseWithNoStitching {
                    base_frequency_x: Au::from_f32_px(base_frequency_x),
                    base_frequency_y: Au::from_f32_px(base_frequency_y),
                    num_octaves,
                    seed,
                }
            }
            FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithStitching { base_frequency_x, base_frequency_y, num_octaves, seed } => {
                FilterGraphOpKey::SVGFETurbulenceWithFractalNoiseWithStitching {
                    base_frequency_x: Au::from_f32_px(base_frequency_x),
                    base_frequency_y: Au::from_f32_px(base_frequency_y),
                    num_octaves,
                    seed,
                }
            }
            FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching { base_frequency_x, base_frequency_y, num_octaves, seed } => {
                FilterGraphOpKey::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching {
                    base_frequency_x: Au::from_f32_px(base_frequency_x),
                    base_frequency_y: Au::from_f32_px(base_frequency_y),
                    num_octaves,
                    seed,
                }
            }
            FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching { base_frequency_x, base_frequency_y, num_octaves, seed } => {
                FilterGraphOpKey::SVGFETurbulenceWithTurbulenceNoiseWithStitching {
                    base_frequency_x: Au::from_f32_px(base_frequency_x),
                    base_frequency_y: Au::from_f32_px(base_frequency_y),
                    num_octaves,
                    seed,
                }
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq, Hash, Eq)]
pub struct FilterGraphNodeKey {
    /// Indicates this graph node was marked as unnecessary by the DAG optimizer
    /// (for example SVGFEOffset can often be folded into downstream nodes)
    pub kept_by_optimizer: bool,
    /// True if color_interpolation_filter == LinearRgb; shader will convert
    /// sRGB texture pixel colors on load and convert back on store, for correct
    /// interpolation
    pub linear: bool,
    /// padding for output rect if we need a border to get correct clamping, or
    /// to account for larger final subregion than source rect (see bug 1869672)
    pub inflate: i16,
    /// virtualized picture input binding 1 (i.e. texture source), typically
    /// this is used, but certain filters do not use it
    pub inputs: Vec<FilterGraphPictureReferenceKey>,
    /// rect this node will render into, in filter space, does not account for
    /// inflate or device_pixel_scale
    pub subregion: [Au; 4],
}

impl From<FilterGraphNode> for FilterGraphNodeKey {
    fn from(node: FilterGraphNode) -> Self {
        FilterGraphNodeKey{
            kept_by_optimizer: node.kept_by_optimizer,
            linear: node.linear,
            inflate: node.inflate,
            inputs: node.inputs.into_iter().map(|node| {node.into()}).collect(),
            subregion: [
                Au::from_f32_px(node.subregion.min.x),
                Au::from_f32_px(node.subregion.min.y),
                Au::from_f32_px(node.subregion.max.x),
                Au::from_f32_px(node.subregion.max.y),
            ],
        }
    }
}

/// Represents a hashable description of how a picture primitive
/// will be composited into its parent.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, MallocSizeOf, PartialEq, Hash, Eq)]
pub enum PictureCompositeKey {
    // No visual compositing effect
    Identity,

    // FilterOp
    Blur(Au, Au, bool),
    Brightness(Au),
    Contrast(Au),
    Grayscale(Au),
    HueRotate(Au),
    Invert(Au),
    Opacity(Au),
    OpacityBinding(PropertyBindingId, Au),
    Saturate(Au),
    Sepia(Au),
    DropShadows(Vec<(VectorKey, Au, ColorU)>),
    ColorMatrix([Au; 20]),
    SrgbToLinear,
    LinearToSrgb,
    ComponentTransfer(ItemUid),
    Flood(ColorU),
    SvgFilter(Vec<FilterPrimitiveKey>),
    SVGFEGraph(Vec<(FilterGraphNodeKey, FilterGraphOpKey)>),

    // MixBlendMode
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
    PlusLighter,
}

impl From<Option<PictureCompositeMode>> for PictureCompositeKey {
    fn from(mode: Option<PictureCompositeMode>) -> Self {
        match mode {
            Some(PictureCompositeMode::MixBlend(mode)) => {
                match mode {
                    MixBlendMode::Normal => PictureCompositeKey::Identity,
                    MixBlendMode::Multiply => PictureCompositeKey::Multiply,
                    MixBlendMode::Screen => PictureCompositeKey::Screen,
                    MixBlendMode::Overlay => PictureCompositeKey::Overlay,
                    MixBlendMode::Darken => PictureCompositeKey::Darken,
                    MixBlendMode::Lighten => PictureCompositeKey::Lighten,
                    MixBlendMode::ColorDodge => PictureCompositeKey::ColorDodge,
                    MixBlendMode::ColorBurn => PictureCompositeKey::ColorBurn,
                    MixBlendMode::HardLight => PictureCompositeKey::HardLight,
                    MixBlendMode::SoftLight => PictureCompositeKey::SoftLight,
                    MixBlendMode::Difference => PictureCompositeKey::Difference,
                    MixBlendMode::Exclusion => PictureCompositeKey::Exclusion,
                    MixBlendMode::Hue => PictureCompositeKey::Hue,
                    MixBlendMode::Saturation => PictureCompositeKey::Saturation,
                    MixBlendMode::Color => PictureCompositeKey::Color,
                    MixBlendMode::Luminosity => PictureCompositeKey::Luminosity,
                    MixBlendMode::PlusLighter => PictureCompositeKey::PlusLighter,
                }
            }
            Some(PictureCompositeMode::Filter(op)) => {
                match op {
                    Filter::Blur { width, height, should_inflate } =>
                        PictureCompositeKey::Blur(Au::from_f32_px(width), Au::from_f32_px(height), should_inflate),
                    Filter::Brightness(value) => PictureCompositeKey::Brightness(Au::from_f32_px(value)),
                    Filter::Contrast(value) => PictureCompositeKey::Contrast(Au::from_f32_px(value)),
                    Filter::Grayscale(value) => PictureCompositeKey::Grayscale(Au::from_f32_px(value)),
                    Filter::HueRotate(value) => PictureCompositeKey::HueRotate(Au::from_f32_px(value)),
                    Filter::Invert(value) => PictureCompositeKey::Invert(Au::from_f32_px(value)),
                    Filter::Saturate(value) => PictureCompositeKey::Saturate(Au::from_f32_px(value)),
                    Filter::Sepia(value) => PictureCompositeKey::Sepia(Au::from_f32_px(value)),
                    Filter::SrgbToLinear => PictureCompositeKey::SrgbToLinear,
                    Filter::LinearToSrgb => PictureCompositeKey::LinearToSrgb,
                    Filter::Identity => PictureCompositeKey::Identity,
                    Filter::DropShadows(ref shadows) => {
                        PictureCompositeKey::DropShadows(
                            shadows.iter().map(|shadow| {
                                (shadow.offset.into(), Au::from_f32_px(shadow.blur_radius), shadow.color.into())
                            }).collect()
                        )
                    }
                    Filter::Opacity(binding, _) => {
                        match binding {
                            PropertyBinding::Value(value) => {
                                PictureCompositeKey::Opacity(Au::from_f32_px(value))
                            }
                            PropertyBinding::Binding(key, default) => {
                                PictureCompositeKey::OpacityBinding(key.id, Au::from_f32_px(default))
                            }
                        }
                    }
                    Filter::ColorMatrix(values) => {
                        let mut quantized_values: [Au; 20] = [Au(0); 20];
                        for (value, result) in values.iter().zip(quantized_values.iter_mut()) {
                            *result = Au::from_f32_px(*value);
                        }
                        PictureCompositeKey::ColorMatrix(quantized_values)
                    }
                    Filter::ComponentTransfer => unreachable!(),
                    Filter::Flood(color) => PictureCompositeKey::Flood(color.into()),
                    Filter::SVGGraphNode(_node, _op) => unreachable!(),
                }
            }
            Some(PictureCompositeMode::ComponentTransferFilter(handle)) => {
                PictureCompositeKey::ComponentTransfer(handle.uid())
            }
            Some(PictureCompositeMode::SvgFilter(filter_primitives, filter_data)) => {
                PictureCompositeKey::SvgFilter(filter_primitives.into_iter().map(|primitive| {
                    match primitive.kind {
                        FilterPrimitiveKind::Identity(identity) => FilterPrimitiveKey::Identity(primitive.color_space, identity.input),
                        FilterPrimitiveKind::Blend(blend) => FilterPrimitiveKey::Blend(primitive.color_space, blend.mode, blend.input1, blend.input2),
                        FilterPrimitiveKind::Flood(flood) => FilterPrimitiveKey::Flood(primitive.color_space, flood.color.into()),
                        FilterPrimitiveKind::Blur(blur) =>
                            FilterPrimitiveKey::Blur(primitive.color_space, Au::from_f32_px(blur.width), Au::from_f32_px(blur.height), blur.input),
                        FilterPrimitiveKind::Opacity(opacity) =>
                            FilterPrimitiveKey::Opacity(primitive.color_space, Au::from_f32_px(opacity.opacity), opacity.input),
                        FilterPrimitiveKind::ColorMatrix(color_matrix) => {
                            let mut quantized_values: [Au; 20] = [Au(0); 20];
                            for (value, result) in color_matrix.matrix.iter().zip(quantized_values.iter_mut()) {
                                *result = Au::from_f32_px(*value);
                            }
                            FilterPrimitiveKey::ColorMatrix(primitive.color_space, quantized_values, color_matrix.input)
                        }
                        FilterPrimitiveKind::DropShadow(drop_shadow) => {
                            FilterPrimitiveKey::DropShadow(
                                primitive.color_space,
                                (
                                    drop_shadow.shadow.offset.into(),
                                    Au::from_f32_px(drop_shadow.shadow.blur_radius),
                                    drop_shadow.shadow.color.into(),
                                ),
                                drop_shadow.input,
                            )
                        }
                        FilterPrimitiveKind::ComponentTransfer(component_transfer) =>
                            FilterPrimitiveKey::ComponentTransfer(primitive.color_space, component_transfer.input, filter_data.clone()),
                        FilterPrimitiveKind::Offset(info) =>
                            FilterPrimitiveKey::Offset(primitive.color_space, info.input, info.offset.into()),
                        FilterPrimitiveKind::Composite(info) =>
                            FilterPrimitiveKey::Composite(primitive.color_space, info.input1, info.input2, info.operator.into()),
                    }
                }).collect())
            }
            Some(PictureCompositeMode::SVGFEGraph(filter_nodes)) => {
                PictureCompositeKey::SVGFEGraph(
                    filter_nodes.into_iter().map(|(node, op)| {
                        (node.into(), op.into())
                    }).collect())
            }
            Some(PictureCompositeMode::Blit(_)) |
            Some(PictureCompositeMode::TileCache { .. }) |
            Some(PictureCompositeMode::IntermediateSurface) |
            None => {
                PictureCompositeKey::Identity
            }
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct Picture {
    pub composite_mode_key: PictureCompositeKey,
    pub raster_space: RasterSpace,
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash)]
pub struct PictureKey {
    pub composite_mode_key: PictureCompositeKey,
    pub raster_space: RasterSpace,
}

impl PictureKey {
    pub fn new(
        pic: Picture,
    ) -> Self {
        PictureKey {
            composite_mode_key: pic.composite_mode_key,
            raster_space: pic.raster_space,
        }
    }
}

impl InternDebug for PictureKey {}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PictureData;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(MallocSizeOf)]
pub struct PictureTemplate;

impl From<PictureKey> for PictureTemplate {
    fn from(_: PictureKey) -> Self {
        PictureTemplate
    }
}

pub type PictureDataHandle = InternHandle<Picture>;

impl Internable for Picture {
    type Key = PictureKey;
    type StoreData = PictureTemplate;
    type InternData = ();
    const PROFILE_COUNTER: usize = crate::profiler::INTERNED_PICTURES;
}

impl InternablePrimitive for Picture {
    fn into_key(
        self,
        _: &LayoutPrimitiveInfo,
    ) -> PictureKey {
        PictureKey::new(self)
    }

    fn make_instance_kind(
        _key: PictureKey,
        _: PictureDataHandle,
        _: &mut PrimitiveStore,
    ) -> PrimitiveInstanceKind {
        // Should never be hit as this method should not be
        // called for pictures.
        unreachable!();
    }
}

impl IsVisible for Picture {
    fn is_visible(&self) -> bool {
        true
    }
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
    assert_eq!(mem::size_of::<Picture>(), 96, "Picture size changed");
    assert_eq!(mem::size_of::<PictureTemplate>(), 0, "PictureTemplate size changed");
    assert_eq!(mem::size_of::<PictureKey>(), 96, "PictureKey size changed");
}

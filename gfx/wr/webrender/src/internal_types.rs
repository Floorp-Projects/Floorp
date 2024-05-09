/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ColorF, DocumentId, ExternalImageId, PrimitiveFlags, Parameter, RenderReasons};
use api::{ImageFormat, NotificationRequest, Shadow, FilterOpGraphPictureBufferId, FilterOpGraphPictureReference, FilterOpGraphNode, FilterOp, ImageBufferKind};
use api::FramePublishId;
use api::units::*;
use crate::render_api::DebugCommand;
use crate::composite::NativeSurfaceOperation;
use crate::device::TextureFilter;
use crate::renderer::{FullFrameStats, PipelineInfo};
use crate::gpu_cache::GpuCacheUpdateList;
use crate::frame_builder::Frame;
use crate::profiler::TransactionProfile;
use crate::spatial_tree::SpatialNodeIndex;
use crate::prim_store::PrimitiveInstanceIndex;
use crate::filterdata::FilterDataHandle;
use fxhash::FxHasher;
use plane_split::BspSplitter;
use smallvec::SmallVec;
use std::{usize, i32};
use std::collections::{HashMap, HashSet};
use std::f32;
use std::hash::BuildHasherDefault;
use std::path::PathBuf;
use std::sync::Arc;
use std::time::{UNIX_EPOCH, SystemTime};
use peek_poke::PeekPoke;

#[cfg(any(feature = "capture", feature = "replay"))]
use crate::capture::CaptureConfig;
#[cfg(feature = "capture")]
use crate::capture::ExternalCaptureImage;
#[cfg(feature = "replay")]
use crate::capture::PlainExternalImage;

pub type FastHashMap<K, V> = HashMap<K, V, BuildHasherDefault<FxHasher>>;
pub type FastHashSet<K> = HashSet<K, BuildHasherDefault<FxHasher>>;

#[derive(Copy, Clone, Hash, MallocSizeOf, PartialEq, PartialOrd, Debug, Eq, Ord, PeekPoke)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameId(u64);

impl FrameId {
    /// Returns a FrameId corresponding to the first frame.
    ///
    /// Note that we use 0 as the internal id here because the current code
    /// increments the frame id at the beginning of the frame, rather than
    /// at the end, and we want the first frame to be 1. It would probably
    /// be sensible to move the advance() call to after frame-building, and
    /// then make this method return FrameId(1).
    pub fn first() -> Self {
        FrameId(0)
    }

    /// Returns the backing u64 for this FrameId.
    pub fn as_u64(&self) -> u64 {
        self.0
    }

    /// Advances this FrameId to the next frame.
    pub fn advance(&mut self) {
        self.0 += 1;
    }

    /// An invalid sentinel FrameId, which will always compare less than
    /// any valid FrameId.
    pub const INVALID: FrameId = FrameId(0);
}

impl Default for FrameId {
    fn default() -> Self {
        FrameId::INVALID
    }
}

impl ::std::ops::Add<u64> for FrameId {
    type Output = Self;
    fn add(self, other: u64) -> FrameId {
        FrameId(self.0 + other)
    }
}

impl ::std::ops::Sub<u64> for FrameId {
    type Output = Self;
    fn sub(self, other: u64) -> FrameId {
        assert!(self.0 >= other, "Underflow subtracting FrameIds");
        FrameId(self.0 - other)
    }
}

/// Identifier to track a sequence of frames.
///
/// This is effectively a `FrameId` with a ridealong timestamp corresponding
/// to when advance() was called, which allows for more nuanced cache eviction
/// decisions. As such, we use the `FrameId` for equality and comparison, since
/// we should never have two `FrameStamps` with the same id but different
/// timestamps.
#[derive(Copy, Clone, Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameStamp {
    id: FrameId,
    time: SystemTime,
    document_id: DocumentId,
}

impl Eq for FrameStamp {}

impl PartialEq for FrameStamp {
    fn eq(&self, other: &Self) -> bool {
        // We should not be checking equality unless the documents are the same
        debug_assert!(self.document_id == other.document_id);
        self.id == other.id
    }
}

impl PartialOrd for FrameStamp {
    fn partial_cmp(&self, other: &Self) -> Option<::std::cmp::Ordering> {
        self.id.partial_cmp(&other.id)
    }
}

impl FrameStamp {
    /// Gets the FrameId in this stamp.
    pub fn frame_id(&self) -> FrameId {
        self.id
    }

    /// Gets the time associated with this FrameStamp.
    pub fn time(&self) -> SystemTime {
        self.time
    }

    /// Gets the DocumentId in this stamp.
    pub fn document_id(&self) -> DocumentId {
        self.document_id
    }

    pub fn is_valid(&self) -> bool {
        // If any fields are their default values, the whole struct should equal INVALID
        debug_assert!((self.time != UNIX_EPOCH && self.id != FrameId(0) && self.document_id != DocumentId::INVALID) ||
                      *self == Self::INVALID);
        self.document_id != DocumentId::INVALID
    }

    /// Returns a FrameStamp corresponding to the first frame.
    pub fn first(document_id: DocumentId) -> Self {
        FrameStamp {
            id: FrameId::first(),
            time: SystemTime::now(),
            document_id,
        }
    }

    /// Advances to a new frame.
    pub fn advance(&mut self) {
        self.id.advance();
        self.time = SystemTime::now();
    }

    /// An invalid sentinel FrameStamp.
    pub const INVALID: FrameStamp = FrameStamp {
        id: FrameId(0),
        time: UNIX_EPOCH,
        document_id: DocumentId::INVALID,
    };
}

/// Custom field embedded inside the Polygon struct of the plane-split crate.
#[derive(Copy, Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PlaneSplitAnchor {
    pub spatial_node_index: SpatialNodeIndex,
    pub instance_index: PrimitiveInstanceIndex,
}

impl PlaneSplitAnchor {
    pub fn new(
        spatial_node_index: SpatialNodeIndex,
        instance_index: PrimitiveInstanceIndex,
    ) -> Self {
        PlaneSplitAnchor {
            spatial_node_index,
            instance_index,
        }
    }
}

impl Default for PlaneSplitAnchor {
    fn default() -> Self {
        PlaneSplitAnchor {
            spatial_node_index: SpatialNodeIndex::INVALID,
            instance_index: PrimitiveInstanceIndex(!0),
        }
    }
}

/// A concrete plane splitter type used in WebRender.
pub type PlaneSplitter = BspSplitter<PlaneSplitAnchor>;

/// An index into the scene's list of plane splitters
#[derive(Debug, Copy, Clone)]
#[cfg_attr(feature = "capture", derive(Serialize))]
pub struct PlaneSplitterIndex(pub usize);

/// An arbitrary number which we assume opacity is invisible below.
const OPACITY_EPSILON: f32 = 0.001;

#[derive(Clone, Copy, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FilterGraphPictureReference {
    /// Id of the picture in question in a namespace unique to this filter DAG,
    /// some are special values like
    /// FilterPrimitiveDescription::kPrimitiveIndexSourceGraphic.
    pub buffer_id: FilterOpGraphPictureBufferId,
    /// Set by wrap_prim_with_filters to the subregion of the input node, may
    /// also have been offset for feDropShadow or feOffset
    pub subregion: LayoutRect,
    /// During scene build this is the offset to apply to the input subregion
    /// for feOffset, which can be optimized away by pushing its offset and
    /// subregion crop to downstream nodes.  This is always zero in render tasks
    /// where it has already been applied to subregion by that point.  Not used
    /// in get_coverage_svgfe because source_padding/target_padding represent
    /// the offset there.
    pub offset: LayoutVector2D,
    /// Equal to the inflate value of the referenced buffer, or 0
    pub inflate: i16,
    /// Padding on each side to represent how this input is read relative to the
    /// node's output subregion, this represents what the operation needs to
    /// read from ths input, which may be blurred or offset.
    pub source_padding: LayoutRect,
    /// Padding on each side to represent how this input affects the node's
    /// subregion, this can be used to calculate target subregion based on
    /// SourceGraphic subregion.  This is usually equal to source_padding except
    /// offset in the opposite direction, inflates typically do the same thing
    /// to both types of padding.
    pub target_padding: LayoutRect,
}

impl From<FilterOpGraphPictureReference> for FilterGraphPictureReference {
    fn from(pic: FilterOpGraphPictureReference) -> Self {
        FilterGraphPictureReference{
            buffer_id: pic.buffer_id,
            // All of these are set by wrap_prim_with_filters
            subregion: LayoutRect::zero(),
            offset: LayoutVector2D::zero(),
            inflate: 0,
            source_padding: LayoutRect::zero(),
            target_padding: LayoutRect::zero(),
        }
    }
}

pub const SVGFE_CONVOLVE_DIAMETER_LIMIT: usize = 5;
pub const SVGFE_CONVOLVE_VALUES_LIMIT: usize = SVGFE_CONVOLVE_DIAMETER_LIMIT *
    SVGFE_CONVOLVE_DIAMETER_LIMIT;

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum FilterGraphOp {
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
    /// Filter that does no transformation of the colors, used to implement a
    /// few things like SVGFEOffset, and this is the default value in
    /// impl_default_for_enums.
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input with offset
    SVGFEIdentity,
    /// represents CSS opacity property as a graph node like the rest of the
    /// SVGFE* filters
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    SVGFEOpacity{valuebinding: api::PropertyBinding<f32>, value: f32},
    /// convert a color image to an alpha channel - internal use; generated by
    /// SVGFilterInstance::GetOrCreateSourceAlphaIndex().
    SVGFEToAlpha,
    /// combine 2 images with SVG_FEBLEND_MODE_DARKEN
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendDarken,
    /// combine 2 images with SVG_FEBLEND_MODE_LIGHTEN
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendLighten,
    /// combine 2 images with SVG_FEBLEND_MODE_MULTIPLY
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendMultiply,
    /// combine 2 images with SVG_FEBLEND_MODE_NORMAL
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feBlendElement
    SVGFEBlendNormal,
    /// combine 2 images with SVG_FEBLEND_MODE_SCREEN
    /// parameters: FilterGraphNode
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
    /// parameters: FilterGraphNode, matrix[5][4]
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feColorMatrixElement
    SVGFEColorMatrix{values: [f32; 20]},
    /// transform colors of image through configurable gradients with component
    /// swizzle
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feComponentTransferElement
    SVGFEComponentTransfer,
    /// Processed version of SVGFEComponentTransfer with the FilterData
    /// replaced by an interned handle, this is made in wrap_prim_with_filters.
    /// Aside from the interned handle, creates_pixels indicates if the transfer
    /// parameters will probably fill the entire subregion with non-zero alpha.
    SVGFEComponentTransferInterned{handle: FilterDataHandle, creates_pixels: bool},
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterGraphNode, k1, k2, k3, k4
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeArithmetic{k1: f32, k2: f32, k3: f32, k4: f32},
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeATop,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterGraphNode
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
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeOut,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeOver,
    /// composite 2 images with chosen composite mode with parameters for that
    /// mode
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feCompositeElement
    SVGFECompositeXOR,
    /// transform image through convolution matrix of up to 25 values (spec
    /// allows more but for performance reasons we do not)
    /// parameters: FilterGraphNode, orderX, orderY, kernelValues[25], divisor,
    ///  bias, targetX, targetY, kernelUnitLengthX, kernelUnitLengthY,
    ///  preserveAlpha
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feConvolveMatrixElement
    SVGFEConvolveMatrixEdgeModeDuplicate{order_x: i32, order_y: i32,
        kernel: [f32; SVGFE_CONVOLVE_VALUES_LIMIT], divisor: f32, bias: f32,
        target_x: i32, target_y: i32, kernel_unit_length_x: f32,
        kernel_unit_length_y: f32, preserve_alpha: i32},
    /// transform image through convolution matrix of up to 25 values (spec
    /// allows more but for performance reasons we do not)
    /// parameters: FilterGraphNode, orderX, orderY, kernelValues[25], divisor,
    ///  bias, targetX, targetY, kernelUnitLengthX, kernelUnitLengthY,
    ///  preserveAlpha
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feConvolveMatrixElement
    SVGFEConvolveMatrixEdgeModeNone{order_x: i32, order_y: i32,
        kernel: [f32; SVGFE_CONVOLVE_VALUES_LIMIT], divisor: f32, bias: f32,
        target_x: i32, target_y: i32, kernel_unit_length_x: f32,
        kernel_unit_length_y: f32, preserve_alpha: i32},
    /// transform image through convolution matrix of up to 25 values (spec
    /// allows more but for performance reasons we do not)
    /// parameters: FilterGraphNode, orderX, orderY, kernelValues[25], divisor,
    ///  bias, targetX, targetY, kernelUnitLengthX, kernelUnitLengthY,
    ///  preserveAlpha
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#feConvolveMatrixElement
    SVGFEConvolveMatrixEdgeModeWrap{order_x: i32, order_y: i32,
        kernel: [f32; SVGFE_CONVOLVE_VALUES_LIMIT], divisor: f32, bias: f32,
        target_x: i32, target_y: i32, kernel_unit_length_x: f32,
        kernel_unit_length_y: f32, preserve_alpha: i32},
    /// calculate lighting based on heightmap image with provided values for a
    /// distant light source with specified direction
    /// parameters: FilterGraphNode, surfaceScale, diffuseConstant,
    ///  kernelUnitLengthX, kernelUnitLengthY, azimuth, elevation
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDiffuseLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDistantLightElement
    SVGFEDiffuseLightingDistant{surface_scale: f32, diffuse_constant: f32,
        kernel_unit_length_x: f32, kernel_unit_length_y: f32, azimuth: f32,
        elevation: f32},
    /// calculate lighting based on heightmap image with provided values for a
    /// point light source at specified location
    /// parameters: FilterGraphNode, surfaceScale, diffuseConstant,
    ///  kernelUnitLengthX, kernelUnitLengthY, x, y, z
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDiffuseLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEPointLightElement
    SVGFEDiffuseLightingPoint{surface_scale: f32, diffuse_constant: f32,
        kernel_unit_length_x: f32, kernel_unit_length_y: f32, x: f32, y: f32,
        z: f32},
    /// calculate lighting based on heightmap image with provided values for a
    /// spot light source at specified location pointing at specified target
    /// location with specified hotspot sharpness and cone angle
    /// parameters: FilterGraphNode, surfaceScale, diffuseConstant,
    ///  kernelUnitLengthX, kernelUnitLengthY, x, y, z, pointsAtX, pointsAtY,
    ///  pointsAtZ, specularExponent, limitingConeAngle
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDiffuseLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpotLightElement
    SVGFEDiffuseLightingSpot{surface_scale: f32, diffuse_constant: f32,
        kernel_unit_length_x: f32, kernel_unit_length_y: f32, x: f32, y: f32,
        z: f32, points_at_x: f32, points_at_y: f32, points_at_z: f32,
        cone_exponent: f32, limiting_cone_angle: f32},
    /// calculate a distorted version of first input image using offset values
    /// from second input image at specified intensity
    /// parameters: FilterGraphNode, scale, xChannelSelector, yChannelSelector
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDisplacementMapElement
    SVGFEDisplacementMap{scale: f32, x_channel_selector: u32,
        y_channel_selector: u32},
    /// create and merge a dropshadow version of the specified image's alpha
    /// channel with specified offset and blur radius
    /// parameters: FilterGraphNode, flood_color, flood_opacity, dx, dy,
    ///  stdDeviationX, stdDeviationY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDropShadowElement
    SVGFEDropShadow{color: ColorF, dx: f32, dy: f32, std_deviation_x: f32,
        std_deviation_y: f32},
    /// synthesize a new image of specified size containing a solid color
    /// parameters: FilterGraphNode, color
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEFloodElement
    SVGFEFlood{color: ColorF},
    /// create a blurred version of the input image
    /// parameters: FilterGraphNode, stdDeviationX, stdDeviationY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEGaussianBlurElement
    SVGFEGaussianBlur{std_deviation_x: f32, std_deviation_y: f32},
    /// synthesize a new image based on a url (i.e. blob image source)
    /// parameters: FilterGraphNode,
    ///  samplingFilter (see SamplingFilter in Types.h), transform
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEImageElement
    SVGFEImage{sampling_filter: u32, matrix: [f32; 6]},
    /// create a new image based on the input image with the contour stretched
    /// outward (dilate operator)
    /// parameters: FilterGraphNode, radiusX, radiusY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEMorphologyElement
    SVGFEMorphologyDilate{radius_x: f32, radius_y: f32},
    /// create a new image based on the input image with the contour shrunken
    /// inward (erode operator)
    /// parameters: FilterGraphNode, radiusX, radiusY
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEMorphologyElement
    SVGFEMorphologyErode{radius_x: f32, radius_y: f32},
    /// calculate lighting based on heightmap image with provided values for a
    /// distant light source with specified direction
    /// parameters: FilerData, surfaceScale, specularConstant, specularExponent,
    ///  kernelUnitLengthX, kernelUnitLengthY, azimuth, elevation
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpecularLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEDistantLightElement
    SVGFESpecularLightingDistant{surface_scale: f32, specular_constant: f32,
        specular_exponent: f32, kernel_unit_length_x: f32,
        kernel_unit_length_y: f32, azimuth: f32, elevation: f32},
    /// calculate lighting based on heightmap image with provided values for a
    /// point light source at specified location
    /// parameters: FilterGraphNode, surfaceScale, specularConstant,
    ///  specularExponent, kernelUnitLengthX, kernelUnitLengthY, x, y, z
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpecularLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFEPointLightElement
    SVGFESpecularLightingPoint{surface_scale: f32, specular_constant: f32,
        specular_exponent: f32, kernel_unit_length_x: f32,
        kernel_unit_length_y: f32, x: f32, y: f32, z: f32},
    /// calculate lighting based on heightmap image with provided values for a
    /// spot light source at specified location pointing at specified target
    /// location with specified hotspot sharpness and cone angle
    /// parameters: FilterGraphNode, surfaceScale, specularConstant,
    ///  specularExponent, kernelUnitLengthX, kernelUnitLengthY, x, y, z,
    ///  pointsAtX, pointsAtY, pointsAtZ, specularExponent, limitingConeAngle
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpecularLightingElement
    ///  https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFESpotLightElement
    SVGFESpecularLightingSpot{surface_scale: f32, specular_constant: f32,
        specular_exponent: f32, kernel_unit_length_x: f32,
        kernel_unit_length_y: f32, x: f32, y: f32, z: f32, points_at_x: f32,
        points_at_y: f32, points_at_z: f32, cone_exponent: f32,
        limiting_cone_angle: f32},
    /// create a new image based on the input image, repeated throughout the
    /// output rectangle
    /// parameters: FilterGraphNode
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETileElement
    SVGFETile,
    /// synthesize a new image based on Fractal Noise (Perlin) with the chosen
    /// stitching mode
    /// parameters: FilterGraphNode, baseFrequencyX, baseFrequencyY, numOctaves,
    ///  seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithFractalNoiseWithNoStitching{base_frequency_x: f32,
        base_frequency_y: f32, num_octaves: u32, seed: u32},
    /// synthesize a new image based on Fractal Noise (Perlin) with the chosen
    /// stitching mode
    /// parameters: FilterGraphNode, baseFrequencyX, baseFrequencyY, numOctaves,
    ///  seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithFractalNoiseWithStitching{base_frequency_x: f32,
        base_frequency_y: f32, num_octaves: u32, seed: u32},
    /// synthesize a new image based on Turbulence Noise (offset vectors)
    /// parameters: FilterGraphNode, baseFrequencyX, baseFrequencyY, numOctaves,
    ///  seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{base_frequency_x: f32,
        base_frequency_y: f32, num_octaves: u32, seed: u32},
    /// synthesize a new image based on Turbulence Noise (offset vectors)
    /// parameters: FilterGraphNode, baseFrequencyX, baseFrequencyY, numOctaves,
    ///  seed
    /// SVG filter semantics - selectable input(s), selectable between linear
    /// (default) and sRGB color space for calculations
    /// Spec: https://www.w3.org/TR/filter-effects-1/#InterfaceSVGFETurbulenceElement
    SVGFETurbulenceWithTurbulenceNoiseWithStitching{base_frequency_x: f32,
        base_frequency_y: f32, num_octaves: u32, seed: u32},
}

impl FilterGraphOp {
    pub fn kind(&self) -> &'static str {
        match *self {
            FilterGraphOp::SVGFEBlendColor => "SVGFEBlendColor",
            FilterGraphOp::SVGFEBlendColorBurn => "SVGFEBlendColorBurn",
            FilterGraphOp::SVGFEBlendColorDodge => "SVGFEBlendColorDodge",
            FilterGraphOp::SVGFEBlendDarken => "SVGFEBlendDarken",
            FilterGraphOp::SVGFEBlendDifference => "SVGFEBlendDifference",
            FilterGraphOp::SVGFEBlendExclusion => "SVGFEBlendExclusion",
            FilterGraphOp::SVGFEBlendHardLight => "SVGFEBlendHardLight",
            FilterGraphOp::SVGFEBlendHue => "SVGFEBlendHue",
            FilterGraphOp::SVGFEBlendLighten => "SVGFEBlendLighten",
            FilterGraphOp::SVGFEBlendLuminosity => "SVGFEBlendLuminosity",
            FilterGraphOp::SVGFEBlendMultiply => "SVGFEBlendMultiply",
            FilterGraphOp::SVGFEBlendNormal => "SVGFEBlendNormal",
            FilterGraphOp::SVGFEBlendOverlay => "SVGFEBlendOverlay",
            FilterGraphOp::SVGFEBlendSaturation => "SVGFEBlendSaturation",
            FilterGraphOp::SVGFEBlendScreen => "SVGFEBlendScreen",
            FilterGraphOp::SVGFEBlendSoftLight => "SVGFEBlendSoftLight",
            FilterGraphOp::SVGFEColorMatrix{..} => "SVGFEColorMatrix",
            FilterGraphOp::SVGFEComponentTransfer => "SVGFEComponentTransfer",
            FilterGraphOp::SVGFEComponentTransferInterned{..} => "SVGFEComponentTransferInterned",
            FilterGraphOp::SVGFECompositeArithmetic{..} => "SVGFECompositeArithmetic",
            FilterGraphOp::SVGFECompositeATop => "SVGFECompositeATop",
            FilterGraphOp::SVGFECompositeIn => "SVGFECompositeIn",
            FilterGraphOp::SVGFECompositeLighter => "SVGFECompositeLighter",
            FilterGraphOp::SVGFECompositeOut => "SVGFECompositeOut",
            FilterGraphOp::SVGFECompositeOver => "SVGFECompositeOver",
            FilterGraphOp::SVGFECompositeXOR => "SVGFECompositeXOR",
            FilterGraphOp::SVGFEConvolveMatrixEdgeModeDuplicate{..} => "SVGFEConvolveMatrixEdgeModeDuplicate",
            FilterGraphOp::SVGFEConvolveMatrixEdgeModeNone{..} => "SVGFEConvolveMatrixEdgeModeNone",
            FilterGraphOp::SVGFEConvolveMatrixEdgeModeWrap{..} => "SVGFEConvolveMatrixEdgeModeWrap",
            FilterGraphOp::SVGFEDiffuseLightingDistant{..} => "SVGFEDiffuseLightingDistant",
            FilterGraphOp::SVGFEDiffuseLightingPoint{..} => "SVGFEDiffuseLightingPoint",
            FilterGraphOp::SVGFEDiffuseLightingSpot{..} => "SVGFEDiffuseLightingSpot",
            FilterGraphOp::SVGFEDisplacementMap{..} => "SVGFEDisplacementMap",
            FilterGraphOp::SVGFEDropShadow{..} => "SVGFEDropShadow",
            FilterGraphOp::SVGFEFlood{..} => "SVGFEFlood",
            FilterGraphOp::SVGFEGaussianBlur{..} => "SVGFEGaussianBlur",
            FilterGraphOp::SVGFEIdentity => "SVGFEIdentity",
            FilterGraphOp::SVGFEImage{..} => "SVGFEImage",
            FilterGraphOp::SVGFEMorphologyDilate{..} => "SVGFEMorphologyDilate",
            FilterGraphOp::SVGFEMorphologyErode{..} => "SVGFEMorphologyErode",
            FilterGraphOp::SVGFEOpacity{..} => "SVGFEOpacity",
            FilterGraphOp::SVGFESourceAlpha => "SVGFESourceAlpha",
            FilterGraphOp::SVGFESourceGraphic => "SVGFESourceGraphic",
            FilterGraphOp::SVGFESpecularLightingDistant{..} => "SVGFESpecularLightingDistant",
            FilterGraphOp::SVGFESpecularLightingPoint{..} => "SVGFESpecularLightingPoint",
            FilterGraphOp::SVGFESpecularLightingSpot{..} => "SVGFESpecularLightingSpot",
            FilterGraphOp::SVGFETile => "SVGFETile",
            FilterGraphOp::SVGFEToAlpha => "SVGFEToAlpha",
            FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{..} => "SVGFETurbulenceWithFractalNoiseWithNoStitching",
            FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithStitching{..} => "SVGFETurbulenceWithFractalNoiseWithStitching",
            FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{..} => "SVGFETurbulenceWithTurbulenceNoiseWithNoStitching",
            FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching{..} => "SVGFETurbulenceWithTurbulenceNoiseWithStitching",
        }
    }
}

#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FilterGraphNode {
    /// Indicates this graph node was marked as necessary by the DAG optimizer
    pub kept_by_optimizer: bool,
    /// true if color_interpolation_filter == LinearRgb; shader will convert
    /// sRGB texture pixel colors on load and convert back on store, for correct
    /// interpolation
    pub linear: bool,
    /// padding for output rect if we need a border to get correct clamping, or
    /// to account for larger final subregion than source rect (see bug 1869672)
    pub inflate: i16,
    /// virtualized picture input bindings, these refer to other filter outputs
    /// by number within the graph, usually there is one element
    pub inputs: Vec<FilterGraphPictureReference>,
    /// clipping rect for filter node output
    pub subregion: LayoutRect,
}

impl From<FilterOpGraphNode> for FilterGraphNode {
    fn from(node: FilterOpGraphNode) -> Self {
        let mut inputs: Vec<FilterGraphPictureReference> = Vec::new();
        if node.input.buffer_id != FilterOpGraphPictureBufferId::None {
            inputs.push(node.input.into());
        }
        if node.input2.buffer_id != FilterOpGraphPictureBufferId::None {
            inputs.push(node.input2.into());
        }
        // If the op used by this node is a feMerge, it will add more inputs
        // after this invocation.
        FilterGraphNode{
            linear: node.linear,
            inputs,
            subregion: node.subregion,
            // These are computed later in scene_building
            kept_by_optimizer: true,
            inflate: 0,
        }
    }
}


/// Equivalent to api::FilterOp with added internal information
#[derive(Clone, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum Filter {
    Identity,
    Blur {
        width: f32,
        height: f32,
        should_inflate: bool,
    },
    Brightness(f32),
    Contrast(f32),
    Grayscale(f32),
    HueRotate(f32),
    Invert(f32),
    Opacity(api::PropertyBinding<f32>, f32),
    Saturate(f32),
    Sepia(f32),
    DropShadows(SmallVec<[Shadow; 1]>),
    ColorMatrix(Box<[f32; 20]>),
    SrgbToLinear,
    LinearToSrgb,
    ComponentTransfer,
    Flood(ColorF),
    SVGGraphNode(FilterGraphNode, FilterGraphOp),
}

impl Filter {
    pub fn is_visible(&self) -> bool {
        match *self {
            Filter::Identity |
            Filter::Blur { .. } |
            Filter::Brightness(..) |
            Filter::Contrast(..) |
            Filter::Grayscale(..) |
            Filter::HueRotate(..) |
            Filter::Invert(..) |
            Filter::Saturate(..) |
            Filter::Sepia(..) |
            Filter::DropShadows(..) |
            Filter::ColorMatrix(..) |
            Filter::SrgbToLinear |
            Filter::LinearToSrgb |
            Filter::ComponentTransfer  => true,
            Filter::Opacity(_, amount) => {
                amount > OPACITY_EPSILON
            },
            Filter::Flood(color) => {
                color.a > OPACITY_EPSILON
            }
            Filter::SVGGraphNode(..) => true,
        }
    }

    pub fn is_noop(&self) -> bool {
        match *self {
            Filter::Identity => false, // this is intentional
            Filter::Blur { width, height, .. } => width == 0.0 && height == 0.0,
            Filter::Brightness(amount) => amount == 1.0,
            Filter::Contrast(amount) => amount == 1.0,
            Filter::Grayscale(amount) => amount == 0.0,
            Filter::HueRotate(amount) => amount == 0.0,
            Filter::Invert(amount) => amount == 0.0,
            Filter::Opacity(api::PropertyBinding::Value(amount), _) => amount >= 1.0,
            Filter::Saturate(amount) => amount == 1.0,
            Filter::Sepia(amount) => amount == 0.0,
            Filter::DropShadows(ref shadows) => {
                for shadow in shadows {
                    if shadow.offset.x != 0.0 || shadow.offset.y != 0.0 || shadow.blur_radius != 0.0 {
                        return false;
                    }
                }

                true
            }
            Filter::ColorMatrix(ref matrix) => {
                **matrix == [
                    1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0,
                    0.0, 0.0, 0.0, 0.0
                ]
            }
            Filter::Opacity(api::PropertyBinding::Binding(..), _) |
            Filter::SrgbToLinear |
            Filter::LinearToSrgb |
            Filter::ComponentTransfer |
            Filter::Flood(..) => false,
            Filter::SVGGraphNode(..) => false,
        }
    }


    pub fn as_int(&self) -> i32 {
        // Must be kept in sync with brush_blend.glsl
        match *self {
            Filter::Identity => 0, // matches `Contrast(1)`
            Filter::Contrast(..) => 0,
            Filter::Grayscale(..) => 1,
            Filter::HueRotate(..) => 2,
            Filter::Invert(..) => 3,
            Filter::Saturate(..) => 4,
            Filter::Sepia(..) => 5,
            Filter::Brightness(..) => 6,
            Filter::ColorMatrix(..) => 7,
            Filter::SrgbToLinear => 8,
            Filter::LinearToSrgb => 9,
            Filter::Flood(..) => 10,
            Filter::ComponentTransfer => 11,
            Filter::Blur { .. } => 12,
            Filter::DropShadows(..) => 13,
            Filter::Opacity(..) => 14,
            Filter::SVGGraphNode(..) => unreachable!("SVGGraphNode handled elsewhere"),
        }
    }
}

impl From<FilterOp> for Filter {
    fn from(op: FilterOp) -> Self {
        match op {
            FilterOp::Identity => Filter::Identity,
            FilterOp::Blur(width, height) => Filter::Blur { width, height, should_inflate: true },
            FilterOp::Brightness(b) => Filter::Brightness(b),
            FilterOp::Contrast(c) => Filter::Contrast(c),
            FilterOp::Grayscale(g) => Filter::Grayscale(g),
            FilterOp::HueRotate(h) => Filter::HueRotate(h),
            FilterOp::Invert(i) => Filter::Invert(i),
            FilterOp::Opacity(binding, opacity) => Filter::Opacity(binding, opacity),
            FilterOp::Saturate(s) => Filter::Saturate(s),
            FilterOp::Sepia(s) => Filter::Sepia(s),
            FilterOp::ColorMatrix(mat) => Filter::ColorMatrix(Box::new(mat)),
            FilterOp::SrgbToLinear => Filter::SrgbToLinear,
            FilterOp::LinearToSrgb => Filter::LinearToSrgb,
            FilterOp::ComponentTransfer => Filter::ComponentTransfer,
            FilterOp::DropShadow(shadow) => Filter::DropShadows(smallvec![shadow]),
            FilterOp::Flood(color) => Filter::Flood(color),
            FilterOp::SVGFEBlendColor{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendColor),
            FilterOp::SVGFEBlendColorBurn{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendColorBurn),
            FilterOp::SVGFEBlendColorDodge{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendColorDodge),
            FilterOp::SVGFEBlendDarken{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendDarken),
            FilterOp::SVGFEBlendDifference{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendDifference),
            FilterOp::SVGFEBlendExclusion{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendExclusion),
            FilterOp::SVGFEBlendHardLight{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendHardLight),
            FilterOp::SVGFEBlendHue{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendHue),
            FilterOp::SVGFEBlendLighten{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendLighten),
            FilterOp::SVGFEBlendLuminosity{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendLuminosity),
            FilterOp::SVGFEBlendMultiply{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendMultiply),
            FilterOp::SVGFEBlendNormal{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendNormal),
            FilterOp::SVGFEBlendOverlay{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendOverlay),
            FilterOp::SVGFEBlendSaturation{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendSaturation),
            FilterOp::SVGFEBlendScreen{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendScreen),
            FilterOp::SVGFEBlendSoftLight{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEBlendSoftLight),
            FilterOp::SVGFEColorMatrix{node, values} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEColorMatrix{values}),
            FilterOp::SVGFEComponentTransfer{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEComponentTransfer),
            FilterOp::SVGFECompositeArithmetic{node, k1, k2, k3, k4} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFECompositeArithmetic{k1, k2, k3, k4}),
            FilterOp::SVGFECompositeATop{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFECompositeATop),
            FilterOp::SVGFECompositeIn{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFECompositeIn),
            FilterOp::SVGFECompositeLighter{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFECompositeLighter),
            FilterOp::SVGFECompositeOut{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFECompositeOut),
            FilterOp::SVGFECompositeOver{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFECompositeOver),
            FilterOp::SVGFECompositeXOR{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFECompositeXOR),
            FilterOp::SVGFEConvolveMatrixEdgeModeDuplicate{node, order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEConvolveMatrixEdgeModeDuplicate{order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha}),
            FilterOp::SVGFEConvolveMatrixEdgeModeNone{node, order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEConvolveMatrixEdgeModeNone{order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha}),
            FilterOp::SVGFEConvolveMatrixEdgeModeWrap{node, order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEConvolveMatrixEdgeModeWrap{order_x, order_y, kernel, divisor, bias, target_x, target_y, kernel_unit_length_x, kernel_unit_length_y, preserve_alpha}),
            FilterOp::SVGFEDiffuseLightingDistant{node, surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, azimuth, elevation} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEDiffuseLightingDistant{surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, azimuth, elevation}),
            FilterOp::SVGFEDiffuseLightingPoint{node, surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, x, y, z} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEDiffuseLightingPoint{surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, x, y, z}),
            FilterOp::SVGFEDiffuseLightingSpot{node, surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, x, y, z, points_at_x, points_at_y, points_at_z, cone_exponent, limiting_cone_angle} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEDiffuseLightingSpot{surface_scale, diffuse_constant, kernel_unit_length_x, kernel_unit_length_y, x, y, z, points_at_x, points_at_y, points_at_z, cone_exponent, limiting_cone_angle}),
            FilterOp::SVGFEDisplacementMap{node, scale, x_channel_selector, y_channel_selector} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEDisplacementMap{scale, x_channel_selector, y_channel_selector}),
            FilterOp::SVGFEDropShadow{node, color, dx, dy, std_deviation_x, std_deviation_y} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEDropShadow{color, dx, dy, std_deviation_x, std_deviation_y}),
            FilterOp::SVGFEFlood{node, color} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEFlood{color}),
            FilterOp::SVGFEGaussianBlur{node, std_deviation_x, std_deviation_y} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEGaussianBlur{std_deviation_x, std_deviation_y}),
            FilterOp::SVGFEIdentity{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEIdentity),
            FilterOp::SVGFEImage{node, sampling_filter, matrix} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEImage{sampling_filter, matrix}),
            FilterOp::SVGFEMorphologyDilate{node, radius_x, radius_y} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEMorphologyDilate{radius_x, radius_y}),
            FilterOp::SVGFEMorphologyErode{node, radius_x, radius_y} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEMorphologyErode{radius_x, radius_y}),
            FilterOp::SVGFEOffset{node, offset_x, offset_y} => {
                Filter::SVGGraphNode(
                    FilterGraphNode {
                        kept_by_optimizer: true, // computed later in scene_building
                        linear: node.linear,
                        inflate: 0, // computed later in scene_building
                        inputs: [FilterGraphPictureReference {
                            buffer_id: node.input.buffer_id,
                            offset: LayoutVector2D::new(offset_x, offset_y),
                            subregion: LayoutRect::zero(),
                            inflate: 0,
                            source_padding: LayoutRect::zero(),
                            target_padding: LayoutRect::zero(),
                        }].to_vec(),
                        subregion: node.subregion,
                    },
                    FilterGraphOp::SVGFEIdentity,
                )
            },
            FilterOp::SVGFEOpacity{node, valuebinding, value} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEOpacity{valuebinding, value}),
            FilterOp::SVGFESourceAlpha{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFESourceAlpha),
            FilterOp::SVGFESourceGraphic{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFESourceGraphic),
            FilterOp::SVGFESpecularLightingDistant{node, surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, azimuth, elevation} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFESpecularLightingDistant{surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, azimuth, elevation}),
            FilterOp::SVGFESpecularLightingPoint{node, surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, x, y, z} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFESpecularLightingPoint{surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, x, y, z}),
            FilterOp::SVGFESpecularLightingSpot{node, surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, x, y, z, points_at_x, points_at_y, points_at_z, cone_exponent, limiting_cone_angle} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFESpecularLightingSpot{surface_scale, specular_constant, specular_exponent, kernel_unit_length_x, kernel_unit_length_y, x, y, z, points_at_x, points_at_y, points_at_z, cone_exponent, limiting_cone_angle}),
            FilterOp::SVGFETile{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFETile),
            FilterOp::SVGFEToAlpha{node} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFEToAlpha),
            FilterOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{node, base_frequency_x, base_frequency_y, num_octaves, seed} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithNoStitching{base_frequency_x, base_frequency_y, num_octaves, seed}),
            FilterOp::SVGFETurbulenceWithFractalNoiseWithStitching{node, base_frequency_x, base_frequency_y, num_octaves, seed} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFETurbulenceWithFractalNoiseWithStitching{base_frequency_x, base_frequency_y, num_octaves, seed}),
            FilterOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{node, base_frequency_x, base_frequency_y, num_octaves, seed} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithNoStitching{base_frequency_x, base_frequency_y, num_octaves, seed}),
            FilterOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching{node, base_frequency_x, base_frequency_y, num_octaves, seed} => Filter::SVGGraphNode(node.into(), FilterGraphOp::SVGFETurbulenceWithTurbulenceNoiseWithStitching{base_frequency_x, base_frequency_y, num_octaves, seed}),
        }
    }
}

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Copy, Debug, Eq, Hash, MallocSizeOf, PartialEq)]
pub enum Swizzle {
    Rgba,
    Bgra,
}

impl Default for Swizzle {
    fn default() -> Self {
        Swizzle::Rgba
    }
}

/// Swizzle settings of the texture cache.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone, Copy, Debug, Eq, Hash, MallocSizeOf, PartialEq)]
pub struct SwizzleSettings {
    /// Swizzle required on sampling a texture with BGRA8 format.
    pub bgra8_sampling_swizzle: Swizzle,
}

/// An ID for a texture that is owned by the `texture_cache` module.
///
/// This can include atlases or standalone textures allocated via the texture
/// cache (e.g.  if an image is too large to be added to an atlas). The texture
/// cache manages the allocation and freeing of these IDs, and the rendering
/// thread maintains a map from cache texture ID to native texture.
///
/// We never reuse IDs, so we use a u64 here to be safe.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CacheTextureId(pub u32);

impl CacheTextureId {
    pub const INVALID: CacheTextureId = CacheTextureId(!0);
}

#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct DeferredResolveIndex(pub u32);

/// Identifies the source of an input texture to a shader.
#[derive(Copy, Clone, Debug, Eq, PartialEq, Hash)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TextureSource {
    /// Equivalent to `None`, allowing us to avoid using `Option`s everywhere.
    Invalid,
    /// An entry in the texture cache.
    TextureCache(CacheTextureId, Swizzle),
    /// An external image texture, mananged by the embedding.
    External(DeferredResolveIndex, ImageBufferKind),
    /// Select a dummy 1x1 white texture. This can be used by image
    /// shaders that want to draw a solid color.
    Dummy,
}

impl TextureSource {
    pub fn image_buffer_kind(&self) -> ImageBufferKind {
        match *self {
            TextureSource::TextureCache(..) => ImageBufferKind::Texture2D,

            TextureSource::External(_, image_buffer_kind) => image_buffer_kind,

            // Render tasks use texture arrays for now.
            TextureSource::Dummy => ImageBufferKind::Texture2D,

            TextureSource::Invalid => ImageBufferKind::Texture2D,
        }
    }

    #[inline]
    pub fn is_compatible(
        &self,
        other: &TextureSource,
    ) -> bool {
        *self == TextureSource::Invalid ||
        *other == TextureSource::Invalid ||
        self == other
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTargetInfo {
    pub has_depth: bool,
}

#[derive(Debug)]
pub enum TextureUpdateSource {
    External {
        id: ExternalImageId,
        channel_index: u8,
    },
    Bytes { data: Arc<Vec<u8>> },
    /// Clears the target area, rather than uploading any pixels. Used when the
    /// texture cache debug display is active.
    DebugClear,
}

/// Command to allocate, reallocate, or free a texture for the texture cache.
#[derive(Debug)]
pub struct TextureCacheAllocation {
    /// The virtual ID (i.e. distinct from device ID) of the texture.
    pub id: CacheTextureId,
    /// Details corresponding to the operation in question.
    pub kind: TextureCacheAllocationKind,
}

/// A little bit of extra information to make memory reports more useful
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TextureCacheCategory {
    Atlas,
    Standalone,
    PictureTile,
    RenderTarget,
}

/// Information used when allocating / reallocating.
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub struct TextureCacheAllocInfo {
    pub width: i32,
    pub height: i32,
    pub format: ImageFormat,
    pub filter: TextureFilter,
    pub target: ImageBufferKind,
    /// Indicates whether this corresponds to one of the shared texture caches.
    pub is_shared_cache: bool,
    /// If true, this texture requires a depth target.
    pub has_depth: bool,
    pub category: TextureCacheCategory
}

/// Sub-operation-specific information for allocation operations.
#[derive(Debug)]
pub enum TextureCacheAllocationKind {
    /// Performs an initial texture allocation.
    Alloc(TextureCacheAllocInfo),
    /// Reallocates the texture without preserving its contents.
    Reset(TextureCacheAllocInfo),
    /// Frees the texture and the corresponding cache ID.
    Free,
}

/// Command to update the contents of the texture cache.
#[derive(Debug)]
pub struct TextureCacheUpdate {
    pub rect: DeviceIntRect,
    pub stride: Option<i32>,
    pub offset: i32,
    pub format_override: Option<ImageFormat>,
    pub source: TextureUpdateSource,
}

/// Command to update the contents of the texture cache.
#[derive(Debug)]
pub struct TextureCacheCopy {
    pub src_rect: DeviceIntRect,
    pub dst_rect: DeviceIntRect,
}

/// Atomic set of commands to manipulate the texture cache, generated on the
/// RenderBackend thread and executed on the Renderer thread.
///
/// The list of allocation operations is processed before the updates. This is
/// important to allow coalescing of certain allocation operations.
#[derive(Default)]
pub struct TextureUpdateList {
    /// Indicates that there was some kind of cleanup clear operation. Used for
    /// sanity checks.
    pub clears_shared_cache: bool,
    /// Commands to alloc/realloc/free the textures. Processed first.
    pub allocations: Vec<TextureCacheAllocation>,
    /// Commands to update the contents of the textures. Processed second.
    pub updates: FastHashMap<CacheTextureId, Vec<TextureCacheUpdate>>,
    /// Commands to move items within the cache, these are applied before everything
    /// else in the update list.
    pub copies: FastHashMap<(CacheTextureId, CacheTextureId), Vec<TextureCacheCopy>>,
}

impl TextureUpdateList {
    /// Mints a new `TextureUpdateList`.
    pub fn new() -> Self {
        TextureUpdateList {
            clears_shared_cache: false,
            allocations: Vec::new(),
            updates: FastHashMap::default(),
            copies: FastHashMap::default(),
        }
    }

    /// Returns true if this is a no-op (no updates to be applied).
    pub fn is_nop(&self) -> bool {
        self.allocations.is_empty() && self.updates.is_empty()
    }

    /// Sets the clears_shared_cache flag for renderer-side sanity checks.
    #[inline]
    pub fn note_clear(&mut self) {
        self.clears_shared_cache = true;
    }

    /// Pushes an update operation onto the list.
    #[inline]
    pub fn push_update(&mut self, id: CacheTextureId, update: TextureCacheUpdate) {
        self.updates
            .entry(id)
            .or_default()
            .push(update);
    }

    /// Sends a command to the Renderer to clear the portion of the shared region
    /// we just freed. Used when the texture cache debugger is enabled.
    #[cold]
    pub fn push_debug_clear(
        &mut self,
        id: CacheTextureId,
        origin: DeviceIntPoint,
        width: i32,
        height: i32,
    ) {
        let size = DeviceIntSize::new(width, height);
        let rect = DeviceIntRect::from_origin_and_size(origin, size);
        self.push_update(id, TextureCacheUpdate {
            rect,
            stride: None,
            offset: 0,
            format_override: None,
            source: TextureUpdateSource::DebugClear,
        });
    }


    /// Pushes an allocation operation onto the list.
    pub fn push_alloc(&mut self, id: CacheTextureId, info: TextureCacheAllocInfo) {
        debug_assert!(!self.allocations.iter().any(|x| x.id == id));
        self.allocations.push(TextureCacheAllocation {
            id,
            kind: TextureCacheAllocationKind::Alloc(info),
        });
    }

    /// Pushes a reallocation operation onto the list, potentially coalescing
    /// with previous operations.
    pub fn push_reset(&mut self, id: CacheTextureId, info: TextureCacheAllocInfo) {
        self.debug_assert_coalesced(id);

        // Drop any unapplied updates to the to-be-freed texture.
        self.updates.remove(&id);

        // Coallesce this realloc into a previous alloc or realloc, if available.
        if let Some(cur) = self.allocations.iter_mut().find(|x| x.id == id) {
            match cur.kind {
                TextureCacheAllocationKind::Alloc(ref mut i) => *i = info,
                TextureCacheAllocationKind::Reset(ref mut i) => *i = info,
                TextureCacheAllocationKind::Free => panic!("Resetting freed texture"),
            }
            return
        }

        self.allocations.push(TextureCacheAllocation {
            id,
            kind: TextureCacheAllocationKind::Reset(info),
        });
    }

    /// Pushes a free operation onto the list, potentially coalescing with
    /// previous operations.
    pub fn push_free(&mut self, id: CacheTextureId) {
        self.debug_assert_coalesced(id);

        // Drop any unapplied updates to the to-be-freed texture.
        self.updates.remove(&id);

        // Drop any allocations for it as well. If we happen to be allocating and
        // freeing in the same batch, we can collapse them to a no-op.
        let idx = self.allocations.iter().position(|x| x.id == id);
        let removed_kind = idx.map(|i| self.allocations.remove(i).kind);
        match removed_kind {
            Some(TextureCacheAllocationKind::Alloc(..)) => { /* no-op! */ },
            Some(TextureCacheAllocationKind::Free) => panic!("Double free"),
            Some(TextureCacheAllocationKind::Reset(..)) |
            None => {
                self.allocations.push(TextureCacheAllocation {
                    id,
                    kind: TextureCacheAllocationKind::Free,
                });
            }
        };
    }

    /// Push a copy operation from a texture to another.
    ///
    /// The source and destination rectangles must have the same size.
    /// The copies are applied before every other operations in the
    /// texture update list.
    pub fn push_copy(
        &mut self,
        src_id: CacheTextureId, src_rect: &DeviceIntRect,
        dst_id: CacheTextureId, dst_rect: &DeviceIntRect,
    ) {
        debug_assert_eq!(src_rect.size(), dst_rect.size());
        self.copies.entry((src_id, dst_id))
            .or_insert_with(Vec::new)
            .push(TextureCacheCopy {
                src_rect: *src_rect,
                dst_rect: *dst_rect,
            });
    }

    fn debug_assert_coalesced(&self, id: CacheTextureId) {
        debug_assert!(
            self.allocations.iter().filter(|x| x.id == id).count() <= 1,
            "Allocations should have been coalesced",
        );
    }
}

/// A list of updates built by the render backend that should be applied
/// by the renderer thread.
pub struct ResourceUpdateList {
    /// List of OS native surface create / destroy operations to apply.
    pub native_surface_updates: Vec<NativeSurfaceOperation>,

    /// Atomic set of texture cache updates to apply.
    pub texture_updates: TextureUpdateList,
}

impl ResourceUpdateList {
    /// Returns true if this update list has no effect.
    pub fn is_nop(&self) -> bool {
        self.texture_updates.is_nop() && self.native_surface_updates.is_empty()
    }
}

/// Wraps a frame_builder::Frame, but conceptually could hold more information
pub struct RenderedDocument {
    pub frame: Frame,
    pub profile: TransactionProfile,
    pub render_reasons: RenderReasons,
    pub frame_stats: Option<FullFrameStats>
}

pub enum DebugOutput {
    #[cfg(feature = "capture")]
    SaveCapture(CaptureConfig, Vec<ExternalCaptureImage>),
    #[cfg(feature = "replay")]
    LoadCapture(CaptureConfig, Vec<PlainExternalImage>),
}

#[allow(dead_code)]
pub enum ResultMsg {
    DebugCommand(DebugCommand),
    DebugOutput(DebugOutput),
    RefreshShader(PathBuf),
    UpdateGpuCache(GpuCacheUpdateList),
    UpdateResources {
        resource_updates: ResourceUpdateList,
        memory_pressure: bool,
    },
    PublishPipelineInfo(PipelineInfo),
    PublishDocument(
        FramePublishId,
        DocumentId,
        RenderedDocument,
        ResourceUpdateList,
    ),
    AppendNotificationRequests(Vec<NotificationRequest>),
    SetParameter(Parameter),
    ForceRedraw,
}

/// Primitive metadata we pass around in a bunch of places
#[derive(Copy, Clone, Debug)]
pub struct LayoutPrimitiveInfo {
    /// NOTE: this is *ideally* redundant with the clip_rect
    /// but that's an ongoing project, so for now it exists and is used :(
    pub rect: LayoutRect,
    pub clip_rect: LayoutRect,
    pub flags: PrimitiveFlags,
}

impl LayoutPrimitiveInfo {
    pub fn with_clip_rect(rect: LayoutRect, clip_rect: LayoutRect) -> Self {
        Self {
            rect,
            clip_rect,
            flags: PrimitiveFlags::default(),
        }
    }
}

// In some cases (e.g. printing) a pipeline is referenced multiple times by
// a parent display list. This allows us to distinguish between them.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Copy, Clone, PartialEq, Debug, Eq, Hash)]
pub struct PipelineInstanceId(u32);

impl PipelineInstanceId {
    pub fn new(id: u32) -> Self {
        PipelineInstanceId(id)
    }
}

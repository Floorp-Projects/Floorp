/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! A collection of coordinate spaces and their corresponding Point, Size and Rect types.
//!
//! Physical pixels take into account the device pixel ratio and their dimensions tend
//! to correspond to the allocated size of resources in memory, while logical pixels
//! don't have the device pixel ratio applied which means they are agnostic to the usage
//! of hidpi screens and the like.
//!
//! The terms "layer" and "stacking context" can be used interchangeably
//! in the context of coordinate systems.

pub use app_units::Au;
use euclid::{Length, TypedRect, TypedScale, TypedSize2D, TypedTransform3D, TypedTranslation2D};
use euclid::{TypedPoint2D, TypedPoint3D, TypedVector2D, TypedVector3D, TypedSideOffsets2D};
use euclid::HomogeneousVector;
// local imports
use crate::image::DirtyRect;

/// Geometry in the coordinate system of the render target (screen or intermediate
/// surface) in physical pixels.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Serialize, Deserialize)]
pub struct DevicePixel;

pub type DeviceIntRect = TypedRect<i32, DevicePixel>;
pub type DeviceIntPoint = TypedPoint2D<i32, DevicePixel>;
pub type DeviceIntSize = TypedSize2D<i32, DevicePixel>;
pub type DeviceIntLength = Length<i32, DevicePixel>;
pub type DeviceIntSideOffsets = TypedSideOffsets2D<i32, DevicePixel>;

pub type DeviceRect = TypedRect<f32, DevicePixel>;
pub type DevicePoint = TypedPoint2D<f32, DevicePixel>;
pub type DeviceVector2D = TypedVector2D<f32, DevicePixel>;
pub type DeviceSize = TypedSize2D<f32, DevicePixel>;
pub type DeviceHomogeneousVector = HomogeneousVector<f32, DevicePixel>;

/// Geometry in the coordinate system of the framebuffer in physical pixels.
/// It's Y-flipped comparing to DevicePixel.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd, Serialize, Deserialize)]
pub struct FramebufferPixel;

pub type FramebufferIntPoint = TypedPoint2D<i32, FramebufferPixel>;
pub type FramebufferIntSize = TypedSize2D<i32, FramebufferPixel>;
pub type FramebufferIntRect = TypedRect<i32, FramebufferPixel>;

/// Geometry in the coordinate system of a Picture (intermediate
/// surface) in physical pixels.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct PicturePixel;

pub type PictureIntRect = TypedRect<i32, PicturePixel>;
pub type PictureIntPoint = TypedPoint2D<i32, PicturePixel>;
pub type PictureIntSize = TypedSize2D<i32, PicturePixel>;
pub type PictureRect = TypedRect<f32, PicturePixel>;
pub type PicturePoint = TypedPoint2D<f32, PicturePixel>;
pub type PictureSize = TypedSize2D<f32, PicturePixel>;
pub type PicturePoint3D = TypedPoint3D<f32, PicturePixel>;
pub type PictureVector2D = TypedVector2D<f32, PicturePixel>;
pub type PictureVector3D = TypedVector3D<f32, PicturePixel>;

/// Geometry gets rasterized in a given root coordinate space. This
/// is often the root spatial node (world space), but may be a local
/// space for a variety of reasons (e.g. perspective).
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct RasterPixel;

pub type RasterIntRect = TypedRect<i32, RasterPixel>;
pub type RasterIntPoint = TypedPoint2D<i32, RasterPixel>;
pub type RasterIntSize = TypedSize2D<i32, RasterPixel>;
pub type RasterRect = TypedRect<f32, RasterPixel>;
pub type RasterPoint = TypedPoint2D<f32, RasterPixel>;
pub type RasterSize = TypedSize2D<f32, RasterPixel>;
pub type RasterPoint3D = TypedPoint3D<f32, RasterPixel>;
pub type RasterVector2D = TypedVector2D<f32, RasterPixel>;
pub type RasterVector3D = TypedVector3D<f32, RasterPixel>;

/// Geometry in a stacking context's local coordinate space (logical pixels).
#[derive(Hash, Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq, Ord, PartialOrd, Deserialize, Serialize)]
pub struct LayoutPixel;

pub type LayoutRect = TypedRect<f32, LayoutPixel>;
pub type LayoutPoint = TypedPoint2D<f32, LayoutPixel>;
pub type LayoutPoint3D = TypedPoint3D<f32, LayoutPixel>;
pub type LayoutVector2D = TypedVector2D<f32, LayoutPixel>;
pub type LayoutVector3D = TypedVector3D<f32, LayoutPixel>;
pub type LayoutSize = TypedSize2D<f32, LayoutPixel>;
pub type LayoutSideOffsets = TypedSideOffsets2D<f32, LayoutPixel>;

pub type LayoutIntRect = TypedRect<i32, LayoutPixel>;
pub type LayoutIntPoint = TypedPoint2D<i32, LayoutPixel>;
pub type LayoutIntSize = TypedSize2D<i32, LayoutPixel>;

/// Geometry in the document's coordinate space (logical pixels).
#[derive(Hash, Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq, Ord, PartialOrd)]
pub struct WorldPixel;

pub type WorldRect = TypedRect<f32, WorldPixel>;
pub type WorldPoint = TypedPoint2D<f32, WorldPixel>;
pub type WorldSize = TypedSize2D<f32, WorldPixel>;
pub type WorldPoint3D = TypedPoint3D<f32, WorldPixel>;
pub type WorldVector2D = TypedVector2D<f32, WorldPixel>;
pub type WorldVector3D = TypedVector3D<f32, WorldPixel>;

/// Offset in number of tiles.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct Tiles;
pub type TileOffset = TypedPoint2D<i32, Tiles>;
pub type TileRange = TypedRect<i32, Tiles>;

/// Scaling ratio from world pixels to device pixels.
pub type DevicePixelScale = TypedScale<f32, WorldPixel, DevicePixel>;
/// Scaling ratio from layout to world. Used for cases where we know the layout
/// is in world space, or specifically want to treat it this way.
pub type LayoutToWorldScale = TypedScale<f32, LayoutPixel, WorldPixel>;
/// A complete scaling ratio from layout space to device pixel space.
pub type LayoutToDeviceScale = TypedScale<f32, LayoutPixel, DevicePixel>;

pub type LayoutTransform = TypedTransform3D<f32, LayoutPixel, LayoutPixel>;
pub type LayoutToWorldTransform = TypedTransform3D<f32, LayoutPixel, WorldPixel>;
pub type WorldToLayoutTransform = TypedTransform3D<f32, WorldPixel, LayoutPixel>;

pub type LayoutToPictureTransform = TypedTransform3D<f32, LayoutPixel, PicturePixel>;
pub type PictureToLayoutTransform = TypedTransform3D<f32, PicturePixel, LayoutPixel>;

pub type LayoutToRasterTransform = TypedTransform3D<f32, LayoutPixel, RasterPixel>;
pub type RasterToLayoutTransform = TypedTransform3D<f32, RasterPixel, LayoutPixel>;

pub type PictureToRasterTransform = TypedTransform3D<f32, PicturePixel, RasterPixel>;
pub type RasterToPictureTransform = TypedTransform3D<f32, RasterPixel, PicturePixel>;

// Fixed position coordinates, to avoid float precision errors.
pub type LayoutPointAu = TypedPoint2D<Au, LayoutPixel>;
pub type LayoutRectAu = TypedRect<Au, LayoutPixel>;
pub type LayoutSizeAu = TypedSize2D<Au, LayoutPixel>;
pub type LayoutVector2DAu = TypedVector2D<Au, LayoutPixel>;
pub type LayoutSideOffsetsAu = TypedSideOffsets2D<Au, LayoutPixel>;

pub type ImageDirtyRect = DirtyRect<i32, DevicePixel>;
pub type BlobDirtyRect = DirtyRect<i32, LayoutPixel>;

pub type BlobToDeviceTranslation = TypedTranslation2D<i32, LayoutPixel, DevicePixel>;

/// Stores two coordinates in texel space. The coordinates
/// are stored in texel coordinates because the texture atlas
/// may grow. Storing them as texel coords and normalizing
/// the UVs in the vertex shader means nothing needs to be
/// updated on the CPU when the texture size changes.
#[derive(Copy, Clone, Debug, Serialize, Deserialize)]
pub struct TexelRect {
    pub uv0: DevicePoint,
    pub uv1: DevicePoint,
}

impl TexelRect {
    pub fn new(u0: f32, v0: f32, u1: f32, v1: f32) -> Self {
        TexelRect {
            uv0: DevicePoint::new(u0, v0),
            uv1: DevicePoint::new(u1, v1),
        }
    }

    pub fn invalid() -> Self {
        TexelRect {
            uv0: DevicePoint::new(-1.0, -1.0),
            uv1: DevicePoint::new(-1.0, -1.0),
        }
    }
}

const MAX_AU_FLOAT: f32 = 1.0e6;

pub trait AuHelpers<T> {
    fn from_au(data: T) -> Self;
    fn to_au(&self) -> T;
}

impl AuHelpers<LayoutSizeAu> for LayoutSize {
    fn from_au(size: LayoutSizeAu) -> Self {
        LayoutSize::new(
            size.width.to_f32_px(),
            size.height.to_f32_px(),
        )
    }

    fn to_au(&self) -> LayoutSizeAu {
        let width = self.width.min(2.0 * MAX_AU_FLOAT);
        let height = self.height.min(2.0 * MAX_AU_FLOAT);

        LayoutSizeAu::new(
            Au::from_f32_px(width),
            Au::from_f32_px(height),
        )
    }
}

impl AuHelpers<LayoutVector2DAu> for LayoutVector2D {
    fn from_au(size: LayoutVector2DAu) -> Self {
        LayoutVector2D::new(
            size.x.to_f32_px(),
            size.y.to_f32_px(),
        )
    }

    fn to_au(&self) -> LayoutVector2DAu {
        LayoutVector2DAu::new(
            Au::from_f32_px(self.x),
            Au::from_f32_px(self.y),
        )
    }
}

impl AuHelpers<LayoutPointAu> for LayoutPoint {
    fn from_au(point: LayoutPointAu) -> Self {
        LayoutPoint::new(
            point.x.to_f32_px(),
            point.y.to_f32_px(),
        )
    }

    fn to_au(&self) -> LayoutPointAu {
        let x = self.x.min(MAX_AU_FLOAT).max(-MAX_AU_FLOAT);
        let y = self.y.min(MAX_AU_FLOAT).max(-MAX_AU_FLOAT);

        LayoutPointAu::new(
            Au::from_f32_px(x),
            Au::from_f32_px(y),
        )
    }
}

impl AuHelpers<LayoutRectAu> for LayoutRect {
    fn from_au(rect: LayoutRectAu) -> Self {
        LayoutRect::new(
            LayoutPoint::from_au(rect.origin),
            LayoutSize::from_au(rect.size),
        )
    }

    fn to_au(&self) -> LayoutRectAu {
        LayoutRectAu::new(
            self.origin.to_au(),
            self.size.to_au(),
        )
    }
}

impl AuHelpers<LayoutSideOffsetsAu> for LayoutSideOffsets {
    fn from_au(offsets: LayoutSideOffsetsAu) -> Self {
        LayoutSideOffsets::new(
            offsets.top.to_f32_px(),
            offsets.right.to_f32_px(),
            offsets.bottom.to_f32_px(),
            offsets.left.to_f32_px(),
        )
    }

    fn to_au(&self) -> LayoutSideOffsetsAu {
        LayoutSideOffsetsAu::new(
            Au::from_f32_px(self.top),
            Au::from_f32_px(self.right),
            Au::from_f32_px(self.bottom),
            Au::from_f32_px(self.left),
        )
    }
}

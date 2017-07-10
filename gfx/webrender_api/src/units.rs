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

use euclid::{Length, TypedTransform3D, TypedRect, TypedSize2D};
use euclid::{TypedPoint2D, TypedPoint3D, TypedVector2D, TypedVector3D};

/// Geometry in the coordinate system of the render target (screen or intermediate
/// surface) in physical pixels.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct DevicePixel;

pub type DeviceIntRect = TypedRect<i32, DevicePixel>;
pub type DeviceIntPoint = TypedPoint2D<i32, DevicePixel>;
pub type DeviceIntSize = TypedSize2D<i32, DevicePixel>;
pub type DeviceIntLength = Length<i32, DevicePixel>;

pub type DeviceUintRect = TypedRect<u32, DevicePixel>;
pub type DeviceUintPoint = TypedPoint2D<u32, DevicePixel>;
pub type DeviceUintSize = TypedSize2D<u32, DevicePixel>;

pub type DeviceRect = TypedRect<f32, DevicePixel>;
pub type DevicePoint = TypedPoint2D<f32, DevicePixel>;
pub type DeviceVector2D = TypedVector2D<f32, DevicePixel>;
pub type DeviceSize = TypedSize2D<f32, DevicePixel>;

/// Geometry in a stacking context's local coordinate space (logical pixels).
///
/// For now layout pixels are equivalent to layer pixels, but it may change.
pub type LayoutPixel = LayerPixel;

pub type LayoutRect = LayerRect;
pub type LayoutPoint = LayerPoint;
pub type LayoutVector2D = LayerVector2D;
pub type LayoutVector3D = LayerVector3D;
pub type LayoutSize = LayerSize;

/// Geometry in a layer's local coordinate space (logical pixels).
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct LayerPixel;

pub type LayerRect = TypedRect<f32, LayerPixel>;
pub type LayerPoint = TypedPoint2D<f32, LayerPixel>;
pub type LayerPoint3D = TypedPoint3D<f32, LayerPixel>;
pub type LayerVector2D = TypedVector2D<f32, LayerPixel>;
pub type LayerVector3D = TypedVector3D<f32, LayerPixel>;
pub type LayerSize = TypedSize2D<f32, LayerPixel>;

/// Geometry in a layer's scrollable parent coordinate space (logical pixels).
///
/// Some layers are scrollable while some are not. There is a distinction between
/// a layer's parent layer and a layer's scrollable parent layer (its closest parent
/// that is scrollable, but not necessarily its immediate parent). Most of the internal
/// transforms are expressed in terms of the scrollable parent and not the immediate
/// parent.
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
pub struct ScrollLayerPixel;

pub type ScrollLayerRect = TypedRect<f32, ScrollLayerPixel>;
pub type ScrollLayerPoint = TypedPoint2D<f32, ScrollLayerPixel>;
pub type ScrollLayerVector2D = TypedVector2D<f32, ScrollLayerPixel>;
pub type ScrollLayerSize = TypedSize2D<f32, ScrollLayerPixel>;

/// Geometry in the document's coordinate space (logical pixels).
#[derive(Hash, Clone, Copy, Debug, Eq, PartialEq, Ord, PartialOrd)]
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
pub type TileOffset = TypedPoint2D<u16, Tiles>;

pub type LayoutTransform = TypedTransform3D<f32, LayoutPixel, LayoutPixel>;
pub type LayerTransform = TypedTransform3D<f32, LayerPixel, LayerPixel>;
pub type LayerToScrollTransform = TypedTransform3D<f32, LayerPixel, ScrollLayerPixel>;
pub type ScrollToLayerTransform = TypedTransform3D<f32, ScrollLayerPixel, LayerPixel>;
pub type LayerToWorldTransform = TypedTransform3D<f32, LayerPixel, WorldPixel>;
pub type WorldToLayerTransform = TypedTransform3D<f32, WorldPixel, LayerPixel>;
pub type ScrollToWorldTransform = TypedTransform3D<f32, ScrollLayerPixel, WorldPixel>;


pub fn device_length(value: f32, device_pixel_ratio: f32) -> DeviceIntLength {
    DeviceIntLength::new((value * device_pixel_ratio).round() as i32)
}

pub fn as_scroll_parent_rect(rect: &LayerRect) -> ScrollLayerRect {
    ScrollLayerRect::from_untyped(&rect.to_untyped())
}

pub fn as_scroll_parent_vector(vector: &LayerVector2D) -> ScrollLayerVector2D {
    ScrollLayerVector2D::from_untyped(&vector.to_untyped())
}

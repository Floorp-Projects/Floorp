/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::f32::consts::{FRAC_1_SQRT_2};
use euclid::{Point2D, Rect, Size2D};
use euclid::{TypedRect, TypedPoint2D, TypedSize2D, TypedPoint4D, TypedMatrix4D};
use webrender_traits::{DeviceIntRect, DeviceIntPoint, DeviceIntSize};
use webrender_traits::{LayerRect, WorldPoint4D, LayerPoint4D, LayerToWorldTransform};
use webrender_traits::{BorderRadius, ComplexClipRegion, LayoutRect};
use num_traits::Zero;

// TODO: Implement these in euclid!
pub trait MatrixHelpers<Src, Dst> {
    fn transform_point_and_perspective_project(&self, point: &TypedPoint4D<f32, Src>) -> TypedPoint2D<f32, Dst>;
    fn transform_rect(&self, rect: &TypedRect<f32, Src>) -> TypedRect<f32, Dst>;

    /// Returns true if this matrix transforms an axis-aligned 2D rectangle to another axis-aligned
    /// 2D rectangle.
    fn can_losslessly_transform_a_2d_rect(&self) -> bool;

    /// Returns true if this matrix transforms an axis-aligned 2D rectangle to another axis-
    /// aligned 2D rectangle after perspective divide.
    fn can_losslessly_transform_and_perspective_project_a_2d_rect(&self) -> bool;

    /// Clears out the portions of the matrix that `transform_rect()` uses. This allows the use of
    /// `transform_rect()` while keeping the Z/W transform portions of the matrix intact.
    fn reset_after_transforming_rect(&self) -> TypedMatrix4D<f32, Src, Dst>;

    fn is_identity(&self) -> bool;
}

impl<Src, Dst> MatrixHelpers<Src, Dst> for TypedMatrix4D<f32, Src, Dst> {
    fn transform_point_and_perspective_project(&self, point: &TypedPoint4D<f32, Src>) -> TypedPoint2D<f32, Dst> {
        let point = self.transform_point4d(point);
        TypedPoint2D::new(point.x / point.w, point.y / point.w)
    }

    fn transform_rect(&self, rect: &TypedRect<f32, Src>) -> TypedRect<f32, Dst> {
        let top_left = self.transform_point(&rect.origin);
        let top_right = self.transform_point(&rect.top_right());
        let bottom_left = self.transform_point(&rect.bottom_left());
        let bottom_right = self.transform_point(&rect.bottom_right());
        TypedRect::from_points(&[top_left, top_right, bottom_right, bottom_left])
    }

    fn can_losslessly_transform_a_2d_rect(&self) -> bool {
        self.m12 == 0.0 && self.m14 == 0.0 && self.m21 == 0.0 && self.m24 == 0.0 && self.m44 == 1.0
    }

    fn can_losslessly_transform_and_perspective_project_a_2d_rect(&self) -> bool {
        self.m12 == 0.0 && self.m21 == 0.0
    }

    fn reset_after_transforming_rect(&self) -> TypedMatrix4D<f32, Src, Dst> {
        TypedMatrix4D::row_major(
            1.0,      0.0,      self.m13, 0.0,
            0.0,      1.0,      self.m23, 0.0,
            self.m31, self.m32, self.m33, self.m34,
            0.0,      0.0,      self.m43, 1.0,
        )
    }

    fn is_identity(&self) -> bool {
        *self == TypedMatrix4D::identity()
    }
}

pub trait RectHelpers<U> where Self: Sized {
    fn contains_rect(&self, other: &Self) -> bool;
    fn from_floats(x0: f32, y0: f32, x1: f32, y1: f32) -> Self;
    fn is_well_formed_and_nonempty(&self) -> bool;
}

impl<U> RectHelpers<U> for TypedRect<f32, U> {

    fn contains_rect(&self, other: &Self) -> bool {
        self.origin.x <= other.origin.x &&
        self.origin.y <= other.origin.y &&
        self.max_x() >= other.max_x() &&
        self.max_y() >= other.max_y()
    }

    fn from_floats(x0: f32, y0: f32, x1: f32, y1: f32) -> Self {
        TypedRect::new(TypedPoint2D::new(x0, y0),
                       TypedSize2D::new(x1 - x0, y1 - y0))
    }

    fn is_well_formed_and_nonempty(&self) -> bool {
        self.size.width > 0.0 && self.size.height > 0.0
    }
}

// Don't use `euclid`'s `is_empty` because that has effectively has an "and" in the conditional
// below instead of an "or".
pub fn rect_is_empty<N:PartialEq + Zero, U>(rect: &TypedRect<N, U>) -> bool {
    rect.size.width == Zero::zero() || rect.size.height == Zero::zero()
}

#[inline]
pub fn rect_from_points_f(x0: f32,
                          y0: f32,
                          x1: f32,
                          y1: f32) -> Rect<f32> {
    Rect::new(Point2D::new(x0, y0),
              Size2D::new(x1 - x0, y1 - y0))
}

pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    (b - a) * t + a
}

pub fn subtract_rect<U>(rect: &TypedRect<f32, U>,
                        other: &TypedRect<f32, U>,
                        results: &mut Vec<TypedRect<f32, U>>) {
    results.clear();

    let int = rect.intersection(other);
    match int {
        Some(int) => {
            let rx0 = rect.origin.x;
            let ry0 = rect.origin.y;
            let rx1 = rx0 + rect.size.width;
            let ry1 = ry0 + rect.size.height;

            let ox0 = int.origin.x;
            let oy0 = int.origin.y;
            let ox1 = ox0 + int.size.width;
            let oy1 = oy0 + int.size.height;

            let r = TypedRect::from_untyped(&rect_from_points_f(rx0, ry0, ox0, ry1));
            if r.size.width > 0.0 && r.size.height > 0.0 {
                results.push(r);
            }
            let r = TypedRect::from_untyped(&rect_from_points_f(ox0, ry0, ox1, oy0));
            if r.size.width > 0.0 && r.size.height > 0.0 {
                results.push(r);
            }
            let r = TypedRect::from_untyped(&rect_from_points_f(ox0, oy1, ox1, ry1));
            if r.size.width > 0.0 && r.size.height > 0.0 {
                results.push(r);
            }
            let r = TypedRect::from_untyped(&rect_from_points_f(ox1, ry0, rx1, ry1));
            if r.size.width > 0.0 && r.size.height > 0.0 {
                results.push(r);
            }
        }
        None => {
            results.push(*rect);
        }
    }
}
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
#[repr(u8)]
pub enum TransformedRectKind {
    AxisAligned = 0,
    Complex = 1,
}

#[derive(Debug, Clone)]
pub struct TransformedRect {
    pub local_rect: LayerRect,
    pub bounding_rect: DeviceIntRect,
    pub inner_rect: DeviceIntRect,
    pub vertices: [WorldPoint4D; 4],
    pub kind: TransformedRectKind,
}

impl TransformedRect {
    pub fn new(rect: &LayerRect,
               transform: &LayerToWorldTransform,
               device_pixel_ratio: f32) -> TransformedRect {

        let kind = if transform.can_losslessly_transform_and_perspective_project_a_2d_rect() {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        };

        // FIXME(gw): This code is meant to be a fast path for simple transforms.
        // However, it fails on transforms that translate Z but result in an
        // axis aligned rect.

/*
        match kind {
            TransformedRectKind::AxisAligned => {
                let v0 = transform.transform_point(&rect.origin);
                let v1 = transform.transform_point(&rect.top_right());
                let v2 = transform.transform_point(&rect.bottom_left());
                let v3 = transform.transform_point(&rect.bottom_right());

                let screen_min_dp = Point2D::new(DevicePixel((v0.x * device_pixel_ratio).floor() as i32),
                                                 DevicePixel((v0.y * device_pixel_ratio).floor() as i32));
                let screen_max_dp = Point2D::new(DevicePixel((v3.x * device_pixel_ratio).ceil() as i32),
                                                 DevicePixel((v3.y * device_pixel_ratio).ceil() as i32));

                let screen_rect_dp = Rect::new(screen_min_dp, Size2D::new(screen_max_dp.x - screen_min_dp.x,
                                                                          screen_max_dp.y - screen_min_dp.y));

                TransformedRect {
                    local_rect: *rect,
                    vertices: [
                        Point4D::new(v0.x, v0.y, 0.0, 1.0),
                        Point4D::new(v1.x, v1.y, 0.0, 1.0),
                        Point4D::new(v2.x, v2.y, 0.0, 1.0),
                        Point4D::new(v3.x, v3.y, 0.0, 1.0),
                    ],
                    bounding_rect: screen_rect_dp,
                    kind: kind,
                }
            }
            TransformedRectKind::Complex => {
                */
                let vertices = [
                    transform.transform_point4d(&LayerPoint4D::new(rect.origin.x,
                                                                   rect.origin.y,
                                                                   0.0,
                                                                   1.0)),
                    transform.transform_point4d(&LayerPoint4D::new(rect.bottom_left().x,
                                                                   rect.bottom_left().y,
                                                                   0.0,
                                                                   1.0)),
                    transform.transform_point4d(&LayerPoint4D::new(rect.bottom_right().x,
                                                                   rect.bottom_right().y,
                                                                   0.0,
                                                                   1.0)),
                    transform.transform_point4d(&LayerPoint4D::new(rect.top_right().x,
                                                                   rect.top_right().y,
                                                                   0.0,
                                                                   1.0)),
                ];

                let (mut xs, mut ys) = ([0.0; 4], [0.0; 4]);

                for (vertex, (x, y)) in vertices.iter().zip(xs.iter_mut().zip(ys.iter_mut())) {
                    let inv_w = 1.0 / vertex.w;
                    *x = vertex.x * inv_w;
                    *y = vertex.y * inv_w;
                }

                xs.sort_by(|a, b| a.partial_cmp(b).unwrap());
                ys.sort_by(|a, b| a.partial_cmp(b).unwrap());

                let outer_min_dp = DeviceIntPoint::new((xs[0] * device_pixel_ratio).floor() as i32,
                                                       (ys[0] * device_pixel_ratio).floor() as i32);
                let outer_max_dp = DeviceIntPoint::new((xs[3] * device_pixel_ratio).ceil() as i32,
                                                       (ys[3] * device_pixel_ratio).ceil() as i32);
                let inner_min_dp = DeviceIntPoint::new((xs[1] * device_pixel_ratio).ceil() as i32,
                                                       (ys[1] * device_pixel_ratio).ceil() as i32);
                let inner_max_dp = DeviceIntPoint::new((xs[2] * device_pixel_ratio).floor() as i32,
                                                       (ys[2] * device_pixel_ratio).floor() as i32);

                TransformedRect {
                    local_rect: *rect,
                    vertices: vertices,
                    bounding_rect: DeviceIntRect::new(outer_min_dp,
                                                      DeviceIntSize::new(outer_max_dp.x - outer_min_dp.x,
                                                                         outer_max_dp.y - outer_min_dp.y)),
                    inner_rect: DeviceIntRect::new(inner_min_dp,
                                                   DeviceIntSize::new(inner_max_dp.x - inner_min_dp.x,
                                                                      inner_max_dp.y - inner_min_dp.y)),
                    kind: kind,
                }
                /*
            }
        }*/
    }
}

#[inline(always)]
pub fn pack_as_float(value: u32) -> f32 {
    value as f32 + 0.5
}


pub trait ComplexClipRegionHelpers {
    /// Return an aligned rectangle that is inside the clip region and doesn't intersect
    /// any of the bounding rectangles of the rounded corners.
    fn get_inner_rect_safe(&self) -> Option<LayoutRect>;
    /// Return the approximately largest aligned rectangle that is fully inside
    /// the provided clip region.
    fn get_inner_rect_full(&self) -> Option<LayoutRect>;
}

impl ComplexClipRegionHelpers for ComplexClipRegion {
    fn get_inner_rect_safe(&self) -> Option<LayoutRect> {
        // value of `k==1.0` is used for extraction of the corner rectangles
        // see `SEGMENT_CORNER_*` in `clip_shared.glsl`
        extract_inner_rect_impl(&self.rect, &self.radii, 1.0)
    }

    fn get_inner_rect_full(&self) -> Option<LayoutRect> {
        // this `k` optimal for a simple case of all border radii being equal
        let k = 1.0 - 0.5 * FRAC_1_SQRT_2; // could be nicely approximated to `0.3`
        extract_inner_rect_impl(&self.rect, &self.radii, k)
    }
}

#[inline]
fn extract_inner_rect_impl<U>(rect: &TypedRect<f32, U>,
                              radii: &BorderRadius,
                              k: f32) -> Option<TypedRect<f32, U>> {
    // `k` defines how much border is taken into account
    // We enforce the offsets to be rounded to pixel boundaries
    // by `ceil`-ing and `floor`-ing them

    let xl = (k * radii.top_left.width.max(radii.bottom_left.width)).ceil();
    let xr = (rect.size.width - k * radii.top_right.width.max(radii.bottom_right.width)).floor();
    let yt = (k * radii.top_left.height.max(radii.top_right.height)).ceil();
    let yb = (rect.size.height - k * radii.bottom_left.height.max(radii.bottom_right.height)).floor();

    if xl <= xr && yt <= yb {
        Some(TypedRect::new(TypedPoint2D::new(rect.origin.x + xl, rect.origin.y + yt),
             TypedSize2D::new(xr-xl, yb-yt)))
    } else {
        None
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ComplexClipRegion, DeviceIntPoint, DeviceIntRect, DeviceIntSize};
use api::{DevicePoint, DeviceRect, DeviceSize, LayerPoint, LayerRect, LayerSize};
use api::{LayerToWorldTransform, LayoutPoint, LayoutRect, LayoutSize, WorldPoint3D};
use euclid::{Point2D, Rect, Size2D, TypedPoint2D, TypedRect, TypedSize2D, TypedTransform2D};
use euclid::TypedTransform3D;
use num_traits::Zero;
use std::f32::consts::FRAC_1_SQRT_2;
use std::i32;
use std::f32;

// Matches the definition of SK_ScalarNearlyZero in Skia.
const NEARLY_ZERO: f32 = 1.0 / 4096.0;

// TODO: Implement these in euclid!
pub trait MatrixHelpers<Src, Dst> {
    fn transform_rect(&self, rect: &TypedRect<f32, Src>) -> TypedRect<f32, Dst>;
    fn is_identity(&self) -> bool;
    fn preserves_2d_axis_alignment(&self) -> bool;
    fn has_perspective_component(&self) -> bool;
    fn has_2d_inverse(&self) -> bool;
    fn inverse_project(&self, target: &TypedPoint2D<f32, Dst>) -> Option<TypedPoint2D<f32, Src>>;
    fn inverse_rect_footprint(&self, rect: &TypedRect<f32, Dst>) -> TypedRect<f32, Src>;
    fn transform_kind(&self) -> TransformedRectKind;
}

impl<Src, Dst> MatrixHelpers<Src, Dst> for TypedTransform3D<f32, Src, Dst> {
    fn transform_rect(&self, rect: &TypedRect<f32, Src>) -> TypedRect<f32, Dst> {
        let top_left = self.transform_point2d(&rect.origin);
        let top_right = self.transform_point2d(&rect.top_right());
        let bottom_left = self.transform_point2d(&rect.bottom_left());
        let bottom_right = self.transform_point2d(&rect.bottom_right());
        TypedRect::from_points(&[top_left, top_right, bottom_right, bottom_left])
    }

    fn is_identity(&self) -> bool {
        *self == TypedTransform3D::identity()
    }

    // A port of the preserves2dAxisAlignment function in Skia.
    // Defined in the SkMatrix44 class.
    fn preserves_2d_axis_alignment(&self) -> bool {
        if self.m14 != 0.0 || self.m24 != 0.0 {
            return false;
        }

        let mut col0 = 0;
        let mut col1 = 0;
        let mut row0 = 0;
        let mut row1 = 0;

        if self.m11.abs() > NEARLY_ZERO {
            col0 += 1;
            row0 += 1;
        }
        if self.m12.abs() > NEARLY_ZERO {
            col1 += 1;
            row0 += 1;
        }
        if self.m21.abs() > NEARLY_ZERO {
            col0 += 1;
            row1 += 1;
        }
        if self.m22.abs() > NEARLY_ZERO {
            col1 += 1;
            row1 += 1;
        }

        col0 < 2 && col1 < 2 && row0 < 2 && row1 < 2
    }

    fn has_perspective_component(&self) -> bool {
         self.m14 != 0.0 || self.m24 != 0.0 || self.m34 != 0.0 || self.m44 != 1.0
    }

    fn has_2d_inverse(&self) -> bool {
        self.m11 * self.m22 - self.m12 * self.m21 != 0.0
    }

    fn inverse_project(&self, target: &TypedPoint2D<f32, Dst>) -> Option<TypedPoint2D<f32, Src>> {
        let m: TypedTransform2D<f32, Src, Dst>;
        m = TypedTransform2D::column_major(
            self.m11 - target.x * self.m14,
            self.m21 - target.x * self.m24,
            self.m41 - target.x * self.m44,
            self.m12 - target.y * self.m14,
            self.m22 - target.y * self.m24,
            self.m42 - target.y * self.m44,
        );
        m.inverse().map(|inv| TypedPoint2D::new(inv.m31, inv.m32))
    }

    fn inverse_rect_footprint(&self, rect: &TypedRect<f32, Dst>) -> TypedRect<f32, Src> {
        TypedRect::from_points(&[
            self.inverse_project(&rect.origin)
                .unwrap_or(TypedPoint2D::zero()),
            self.inverse_project(&rect.top_right())
                .unwrap_or(TypedPoint2D::zero()),
            self.inverse_project(&rect.bottom_left())
                .unwrap_or(TypedPoint2D::zero()),
            self.inverse_project(&rect.bottom_right())
                .unwrap_or(TypedPoint2D::zero()),
        ])
    }

    fn transform_kind(&self) -> TransformedRectKind {
        if self.preserves_2d_axis_alignment() {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        }
    }
}

pub trait RectHelpers<U>
where
    Self: Sized,
{
    fn contains_rect(&self, other: &Self) -> bool;
    fn from_floats(x0: f32, y0: f32, x1: f32, y1: f32) -> Self;
    fn is_well_formed_and_nonempty(&self) -> bool;
}

impl<U> RectHelpers<U> for TypedRect<f32, U> {
    fn contains_rect(&self, other: &Self) -> bool {
        self.origin.x <= other.origin.x && self.origin.y <= other.origin.y &&
            self.max_x() >= other.max_x() && self.max_y() >= other.max_y()
    }

    fn from_floats(x0: f32, y0: f32, x1: f32, y1: f32) -> Self {
        TypedRect::new(
            TypedPoint2D::new(x0, y0),
            TypedSize2D::new(x1 - x0, y1 - y0),
        )
    }

    fn is_well_formed_and_nonempty(&self) -> bool {
        self.size.width > 0.0 && self.size.height > 0.0
    }
}

// Don't use `euclid`'s `is_empty` because that has effectively has an "and" in the conditional
// below instead of an "or".
pub fn rect_is_empty<N: PartialEq + Zero, U>(rect: &TypedRect<N, U>) -> bool {
    rect.size.width == Zero::zero() || rect.size.height == Zero::zero()
}

#[allow(dead_code)]
#[inline]
pub fn rect_from_points_f(x0: f32, y0: f32, x1: f32, y1: f32) -> Rect<f32> {
    Rect::new(Point2D::new(x0, y0), Size2D::new(x1 - x0, y1 - y0))
}

pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    (b - a) * t + a
}

pub fn _subtract_rect<U>(
    rect: &TypedRect<f32, U>,
    other: &TypedRect<f32, U>,
    results: &mut Vec<TypedRect<f32, U>>,
) {
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

pub fn get_normal(x: f32) -> Option<f32> {
    if x.is_normal() {
        Some(x)
    } else {
        None
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[repr(u32)]
pub enum TransformedRectKind {
    AxisAligned = 0,
    Complex = 1,
}

#[derive(Debug, Clone)]
pub struct TransformedRect {
    pub local_rect: LayerRect,
    pub bounding_rect: DeviceIntRect,
    pub inner_rect: DeviceIntRect,
    pub vertices: [WorldPoint3D; 4],
    pub kind: TransformedRectKind,
}

impl TransformedRect {
    pub fn new(
        rect: &LayerRect,
        transform: &LayerToWorldTransform,
        device_pixel_ratio: f32,
    ) -> TransformedRect {
        let kind = if transform.preserves_2d_axis_alignment() {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        };


        let vertices = [
            transform.transform_point3d(&rect.origin.to_3d()),
            transform.transform_point3d(&rect.bottom_left().to_3d()),
            transform.transform_point3d(&rect.bottom_right().to_3d()),
            transform.transform_point3d(&rect.top_right().to_3d()),
        ];

        let (mut xs, mut ys) = ([0.0; 4], [0.0; 4]);

        for (vertex, (x, y)) in vertices.iter().zip(xs.iter_mut().zip(ys.iter_mut())) {
            *x = get_normal(vertex.x).unwrap_or(0.0);
            *y = get_normal(vertex.y).unwrap_or(0.0);
        }

        xs.sort_by(|a, b| a.partial_cmp(b).unwrap());
        ys.sort_by(|a, b| a.partial_cmp(b).unwrap());

        let outer_min_dp = (DevicePoint::new(xs[0], ys[0]) * device_pixel_ratio).floor();
        let outer_max_dp = (DevicePoint::new(xs[3], ys[3]) * device_pixel_ratio).ceil();
        let inner_min_dp = (DevicePoint::new(xs[1], ys[1]) * device_pixel_ratio).ceil();
        let inner_max_dp = (DevicePoint::new(xs[2], ys[2]) * device_pixel_ratio).floor();

        let max_rect = DeviceRect::max_rect();
        let bounding_rect = DeviceRect::new(outer_min_dp, (outer_max_dp - outer_min_dp).to_size())
            .intersection(&max_rect)
            .unwrap_or(max_rect)
            .to_i32();
        let inner_rect = DeviceRect::new(inner_min_dp, (inner_max_dp - inner_min_dp).to_size())
            .intersection(&max_rect)
            .unwrap_or(max_rect)
            .to_i32();

        TransformedRect {
            local_rect: *rect,
            vertices,
            bounding_rect,
            inner_rect,
            kind,
        }
    }
}

#[inline(always)]
pub fn pack_as_float(value: u32) -> f32 {
    value as f32 + 0.5
}


pub trait ComplexClipRegionHelpers {
    /// Return the approximately largest aligned rectangle that is fully inside
    /// the provided clip region.
    fn get_inner_rect_full(&self) -> Option<LayoutRect>;
    /// Split the clip region into 2 sets of rectangles: opaque and transparent.
    /// Guarantees no T-junctions in the produced split.
    /// Attempts to cover more space in opaque, where it reasonably makes sense.
    fn split_rectangles(
        &self,
        opaque: &mut Vec<LayoutRect>,
        transparent: &mut Vec<LayoutRect>,
    );
}

impl ComplexClipRegionHelpers for ComplexClipRegion {
    fn get_inner_rect_full(&self) -> Option<LayoutRect> {
        // this `k` is optimal for a simple case of all border radii being equal
        let k = 1.0 - 0.5 * FRAC_1_SQRT_2; // could be nicely approximated to `0.3`
        extract_inner_rect_impl(&self.rect, &self.radii, k)
    }

    fn split_rectangles(
        &self,
        opaque: &mut Vec<LayoutRect>,
        transparent: &mut Vec<LayoutRect>,
    ) {
        fn rect(p0: LayoutPoint, p1: LayoutPoint) -> Option<LayoutRect> {
            if p0.x != p1.x && p0.y != p1.y {
                Some(LayerRect::new(p0.min(p1), (p1 - p0).abs().to_size()))
            } else {
                None
            }
        }

        let inner = match extract_inner_rect_impl(&self.rect, &self.radii, 1.0) {
            Some(rect) => rect,
            None => {
                transparent.push(self.rect);
                return
            },
        };
        let left_top = inner.origin - self.rect.origin;
        let right_bot = self.rect.bottom_right() - inner.bottom_right();

        // fill in the opaque parts
        opaque.push(inner);
        if left_top.x > 0.0 {
            opaque.push(LayerRect::new(
                LayoutPoint::new(self.rect.origin.x, inner.origin.y),
                LayoutSize::new(left_top.x, inner.size.height),
            ));
        }
        if right_bot.y > 0.0 {
            opaque.push(LayerRect::new(
                LayoutPoint::new(inner.origin.x, inner.origin.y + inner.size.height),
                LayoutSize::new(inner.size.width, right_bot.y),
            ));
        }
        if right_bot.x > 0.0 {
            opaque.push(LayerRect::new(
                LayoutPoint::new(inner.origin.x + inner.size.width, inner.origin.y),
                LayoutSize::new(right_bot.x, inner.size.height),
            ));
        }
        if left_top.y > 0.0 {
            opaque.push(LayerRect::new(
                LayoutPoint::new(inner.origin.x, self.rect.origin.y),
                LayoutSize::new(inner.size.width, left_top.y),
            ));
        }

        // fill in the transparent parts
        transparent.extend(rect(self.rect.origin, inner.origin));
        transparent.extend(rect(self.rect.bottom_left(), inner.bottom_left()));
        transparent.extend(rect(self.rect.bottom_right(), inner.bottom_right()));
        transparent.extend(rect(self.rect.top_right(), inner.top_right()));
    }
}

#[inline]
fn extract_inner_rect_impl<U>(
    rect: &TypedRect<f32, U>,
    radii: &BorderRadius,
    k: f32,
) -> Option<TypedRect<f32, U>> {
    // `k` defines how much border is taken into account
    // We enforce the offsets to be rounded to pixel boundaries
    // by `ceil`-ing and `floor`-ing them

    let xl = (k * radii.top_left.width.max(radii.bottom_left.width)).ceil();
    let xr = (rect.size.width - k * radii.top_right.width.max(radii.bottom_right.width)).floor();
    let yt = (k * radii.top_left.height.max(radii.top_right.height)).ceil();
    let yb =
        (rect.size.height - k * radii.bottom_left.height.max(radii.bottom_right.height)).floor();

    if xl <= xr && yt <= yb {
        Some(TypedRect::new(
            TypedPoint2D::new(rect.origin.x + xl, rect.origin.y + yt),
            TypedSize2D::new(xr - xl, yb - yt),
        ))
    } else {
        None
    }
}

/// Return an aligned rectangle that is inside the clip region and doesn't intersect
/// any of the bounding rectangles of the rounded corners.
pub fn extract_inner_rect_safe<U>(
    rect: &TypedRect<f32, U>,
    radii: &BorderRadius,
) -> Option<TypedRect<f32, U>> {
    // value of `k==1.0` is used for extraction of the corner rectangles
    // see `SEGMENT_CORNER_*` in `clip_shared.glsl`
    extract_inner_rect_impl(rect, radii, 1.0)
}

/// Consumes the old vector and returns a new one that may reuse the old vector's allocated
/// memory.
pub fn recycle_vec<T>(mut old_vec: Vec<T>) -> Vec<T> {
    if old_vec.capacity() > 2 * old_vec.len() {
        // Avoid reusing the buffer if it is a lot larger than it needs to be. This prevents
        // a frame with exceptionally large allocations to cause subsequent frames to retain
        // more memory than they need.
        return Vec::with_capacity(old_vec.len());
    }

    old_vec.clear();

    return old_vec;
}


#[cfg(test)]
pub mod test {
    use super::*;
    use euclid::{Point2D, Radians, Transform3D};
    use std::f32::consts::PI;

    #[test]
    fn inverse_project() {
        let m0 = Transform3D::identity();
        let p0 = Point2D::new(1.0, 2.0);
        // an identical transform doesn't need any inverse projection
        assert_eq!(m0.inverse_project(&p0), Some(p0));
        let m1 = Transform3D::create_rotation(0.0, 1.0, 0.0, Radians::new(PI / 3.0));
        // rotation by 60 degrees would imply scaling of X component by a factor of 2
        assert_eq!(m1.inverse_project(&p0), Some(Point2D::new(2.0, 2.0)));
    }
}

pub trait MaxRect {
    fn max_rect() -> Self;
}

impl MaxRect for LayerRect {
    fn max_rect() -> Self {
        LayerRect::new(
            LayerPoint::new(f32::MIN / 2.0, f32::MIN / 2.0),
            LayerSize::new(f32::MAX, f32::MAX),
        )
    }
}

impl MaxRect for DeviceIntRect {
    fn max_rect() -> Self {
        DeviceIntRect::new(
            DeviceIntPoint::new(i32::MIN / 2, i32::MIN / 2),
            DeviceIntSize::new(i32::MAX, i32::MAX),
        )
    }
}

impl MaxRect for DeviceRect {
    fn max_rect() -> Self {
        // Having an unlimited bounding box is fine up until we try
        // to cast it to `i32`, where we get `-2147483648` for any
        // values larger than or equal to 2^31.
        //
        // Note: clamping to i32::MIN and i32::MAX is not a solution,
        // with explanation left as an exercise for the reader.
        const MAX_COORD: f32 = 1.0e9;

        DeviceRect::new(
            DevicePoint::new(-MAX_COORD, -MAX_COORD),
            DeviceSize::new(2.0 * MAX_COORD, 2.0 * MAX_COORD),
        )
    }
}

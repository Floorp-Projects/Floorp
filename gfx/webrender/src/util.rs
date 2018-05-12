/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, DeviceIntPoint, DeviceIntRect, DeviceIntSize, DevicePixelScale};
use api::{DevicePoint, DeviceRect, DeviceSize, LayoutPixel, LayoutPoint, LayoutRect, LayoutSize};
use api::{WorldPixel, WorldRect};
use euclid::{Point2D, Rect, Size2D, TypedPoint2D, TypedRect, TypedSize2D};
use euclid::{TypedTransform2D, TypedTransform3D, TypedVector2D, TypedVector3D};
use euclid::{HomogeneousVector};
use num_traits::Zero;
use plane_split::{Clipper, Plane, Polygon};
use std::{i32, f32};

// Matches the definition of SK_ScalarNearlyZero in Skia.
const NEARLY_ZERO: f32 = 1.0 / 4096.0;

// TODO: Implement these in euclid!
pub trait MatrixHelpers<Src, Dst> {
    fn preserves_2d_axis_alignment(&self) -> bool;
    fn has_perspective_component(&self) -> bool;
    fn has_2d_inverse(&self) -> bool;
    fn inverse_project(&self, target: &TypedPoint2D<f32, Dst>) -> Option<TypedPoint2D<f32, Src>>;
    fn inverse_rect_footprint(&self, rect: &TypedRect<f32, Dst>) -> TypedRect<f32, Src>;
    fn transform_kind(&self) -> TransformedRectKind;
    fn is_simple_translation(&self) -> bool;
    fn is_simple_2d_translation(&self) -> bool;
}

impl<Src, Dst> MatrixHelpers<Src, Dst> for TypedTransform3D<f32, Src, Dst> {
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
            self.inverse_project(&rect.origin).unwrap_or(TypedPoint2D::zero()),
            self.inverse_project(&rect.top_right()).unwrap_or(TypedPoint2D::zero()),
            self.inverse_project(&rect.bottom_left()).unwrap_or(TypedPoint2D::zero()),
            self.inverse_project(&rect.bottom_right()).unwrap_or(TypedPoint2D::zero()),
        ])
    }

    fn transform_kind(&self) -> TransformedRectKind {
        if self.preserves_2d_axis_alignment() {
            TransformedRectKind::AxisAligned
        } else {
            TransformedRectKind::Complex
        }
    }

    fn is_simple_translation(&self) -> bool {
        if (self.m11 - 1.0).abs() > NEARLY_ZERO ||
            (self.m22 - 1.0).abs() > NEARLY_ZERO ||
            (self.m33 - 1.0).abs() > NEARLY_ZERO {
            return false;
        }

        self.m12.abs() < NEARLY_ZERO && self.m13.abs() < NEARLY_ZERO &&
            self.m14.abs() < NEARLY_ZERO && self.m21.abs() < NEARLY_ZERO &&
            self.m23.abs() < NEARLY_ZERO && self.m24.abs() < NEARLY_ZERO &&
            self.m31.abs() < NEARLY_ZERO && self.m32.abs() < NEARLY_ZERO &&
            self.m34.abs() < NEARLY_ZERO
    }

    fn is_simple_2d_translation(&self) -> bool {
        if !self.is_simple_translation() {
            return false;
        }

        self.m43.abs() < NEARLY_ZERO
    }
}

pub trait RectHelpers<U>
where
    Self: Sized,
{
    fn from_floats(x0: f32, y0: f32, x1: f32, y1: f32) -> Self;
    fn is_well_formed_and_nonempty(&self) -> bool;
}

impl<U> RectHelpers<U> for TypedRect<f32, U> {
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

pub fn calculate_screen_bounding_rect(
    transform: &LayoutToWorldFastTransform,
    rect: &LayoutRect,
    device_pixel_scale: DevicePixelScale,
    screen_bounds: Option<&DeviceIntRect>,
) -> Option<DeviceIntRect> {
    debug!("rect {:?}", rect);
    debug!("transform {:?}", transform);
    debug!("screen_bounds: {:?}", screen_bounds);
    let homogens = [
        transform.transform_point2d_homogeneous(&rect.origin),
        transform.transform_point2d_homogeneous(&rect.top_right()),
        transform.transform_point2d_homogeneous(&rect.bottom_left()),
        transform.transform_point2d_homogeneous(&rect.bottom_right()),
    ];
    debug!("homogeneous points {:?}", homogens);
    let max_rect = match screen_bounds {
        Some(bounds) => bounds.to_f32(),
        None => DeviceRect::max_rect(),
    };

    // Note: we only do the full frustum collision when the polygon approaches the camera plane.
    // Otherwise, it will be clamped to the screen bounds anyway.
    let world_rect = if homogens.iter().any(|h| h.w <= 0.0) {
        let mut clipper = Clipper::new();
        // using inverse-transpose of the inversed transform
        let t = transform.to_transform();
        // camera plane
        {
            let normal = TypedVector3D::new(t.m14, t.m24, t.m34);
            let kf = 1.0 / normal.length();
            clipper.add(Plane {
                normal: normal * kf,
                offset: t.m44 * kf,
            });
        }

        // The equations for clip planes come from the following one:
        // (v * M).x < right
        // (v, Mx) < right
        // (v, Mx) - right = 0;

        // left/right planes
        if let Some(bounds) = screen_bounds {
            let normal = TypedVector3D::new(t.m11, t.m21, t.m31);
            let kf = 1.0 / normal.length();
            clipper.add(Plane {
                normal: normal * kf,
                offset: t.m41 * kf - (bounds.origin.x) as f32 / device_pixel_scale.0,
            });
            clipper.add(Plane {
                normal: normal * -kf,
                offset: t.m41 * -kf + (bounds.origin.x + bounds.size.width) as f32 / device_pixel_scale.0,
            });
        }
        // top/bottom planes
        if let Some(bounds) = screen_bounds {
            let normal = TypedVector3D::new(t.m12, t.m22, t.m32);
            let kf = 1.0 / normal.length();
            clipper.add(Plane {
                normal: normal * kf,
                offset: t.m42 * kf - (bounds.origin.y) as f32 / device_pixel_scale.0,
            });
            clipper.add(Plane {
                normal: normal * -kf,
                offset: t.m42 * -kf + (bounds.origin.y + bounds.size.height) as f32 / device_pixel_scale.0,
            });
        }

        let polygon = Polygon::from_points(
            [
                rect.origin.to_3d(),
                rect.top_right().to_3d(),
                rect.bottom_left().to_3d(),
                rect.bottom_right().to_3d(),
            ],
            1,
        );
        debug!("crossing detected for poly {:?}", polygon);
        let results = clipper.clip(polygon);
        debug!("clip results: {:?}", results);
        if results.is_empty() {
            return None
        }

        debug!("points:");
        WorldRect::from_points(results
            .into_iter()
            // filter out parts behind the view plane
            .flat_map(|poly| &poly.points)
            .map(|p| {
                debug!("\tpoint {:?} -> {:?} -> {:?}", p,
                    transform.transform_point2d_homogeneous(&p.to_2d()),
                    transform.transform_point2d(&p.to_2d())
                );
                transform.transform_point2d(&p.to_2d())
            })
        )
    } else {
        WorldRect::from_points(&[
            homogens[0].to_point2d(),
            homogens[1].to_point2d(),
            homogens[2].to_point2d(),
            homogens[3].to_point2d(),
        ])
    };

    debug!("world rect {:?}", world_rect);
    (world_rect * device_pixel_scale)
        .round_out()
        .intersection(&max_rect)
        .map(|r| r.to_i32())
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

#[repr(u32)]
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TransformedRectKind {
    AxisAligned = 0,
    Complex = 1,
}

#[inline(always)]
pub fn pack_as_float(value: u32) -> f32 {
    value as f32 + 0.5
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

    old_vec
}


#[cfg(test)]
pub mod test {
    use super::*;
    use euclid::{Point2D, Angle, Transform3D};
    use std::f32::consts::PI;

    #[test]
    fn inverse_project() {
        let m0 = Transform3D::identity();
        let p0 = Point2D::new(1.0, 2.0);
        // an identical transform doesn't need any inverse projection
        assert_eq!(m0.inverse_project(&p0), Some(p0));
        let m1 = Transform3D::create_rotation(0.0, 1.0, 0.0, Angle::radians(PI / 3.0));
        // rotation by 60 degrees would imply scaling of X component by a factor of 2
        assert_eq!(m1.inverse_project(&p0), Some(Point2D::new(2.0, 2.0)));
    }
}

pub trait MaxRect {
    fn max_rect() -> Self;
}

impl MaxRect for LayoutRect {
    fn max_rect() -> Self {
        LayoutRect::new(
            LayoutPoint::new(f32::MIN / 2.0, f32::MIN / 2.0),
            LayoutSize::new(f32::MAX, f32::MAX),
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

/// An enum that tries to avoid expensive transformation matrix calculations
/// when possible when dealing with non-perspective axis-aligned transformations.
#[derive(Debug, Clone, Copy)]
pub enum FastTransform<Src, Dst> {
    /// A simple offset, which can be used without doing any matrix math.
    Offset(TypedVector2D<f32, Src>),

    /// A 2D transformation with an inverse.
    Transform {
        transform: TypedTransform3D<f32, Src, Dst>,
        inverse: Option<TypedTransform3D<f32, Dst, Src>>,
        is_2d: bool,
    },
}

impl<Src, Dst> FastTransform<Src, Dst> {
    pub fn identity() -> Self {
        FastTransform::Offset(TypedVector2D::zero())
    }

    pub fn with_vector(offset: TypedVector2D<f32, Src>) -> Self {
        FastTransform::Offset(offset)
    }

    #[inline(always)]
    pub fn with_transform(transform: TypedTransform3D<f32, Src, Dst>) -> Self {
        if transform.is_simple_2d_translation() {
            return FastTransform::Offset(TypedVector2D::new(transform.m41, transform.m42));
        }
        let inverse = transform.inverse();
        let is_2d = transform.is_2d();
        FastTransform::Transform { transform, inverse, is_2d}
    }

    pub fn to_transform(&self) -> TypedTransform3D<f32, Src, Dst> {
        match *self {
            FastTransform::Offset(offset) =>
                TypedTransform3D::create_translation(offset.x, offset.y, 0.0),
            FastTransform::Transform { transform, .. } => transform
        }
    }

    pub fn is_invertible(&self) -> bool {
        match *self {
            FastTransform::Offset(..) => true,
            FastTransform::Transform { ref inverse, .. } => inverse.is_some(),
        }
    }

    #[inline(always)]
    pub fn pre_mul<NewSrc>(
        &self,
        other: &FastTransform<NewSrc, Src>
    ) -> FastTransform<NewSrc, Dst> {
        match (self, other) {
            (&FastTransform::Offset(ref offset), &FastTransform::Offset(ref other_offset)) => {
                let offset = TypedVector2D::from_untyped(&offset.to_untyped());
                FastTransform::Offset(offset + *other_offset)
            }
            _ => {
                let new_transform = self.to_transform().pre_mul(&other.to_transform());
                FastTransform::with_transform(new_transform)
            }
        }
    }

    #[inline(always)]
    pub fn pre_translate(&self, other_offset: &TypedVector2D<f32, Src>) -> Self {
        match *self {
            FastTransform::Offset(ref offset) =>
                FastTransform::Offset(*offset + *other_offset),
            FastTransform::Transform { transform, .. } =>
                FastTransform::with_transform(transform.pre_translate(other_offset.to_3d()))
        }
    }

    #[inline(always)]
    pub fn preserves_2d_axis_alignment(&self) -> bool {
        match *self {
            FastTransform::Offset(..) => true,
            FastTransform::Transform { ref transform, .. } =>
                transform.preserves_2d_axis_alignment(),
        }
    }

    #[inline(always)]
    pub fn has_perspective_component(&self) -> bool {
        match *self {
            FastTransform::Offset(..) => false,
            FastTransform::Transform { ref transform, .. } => transform.has_perspective_component(),
        }
    }

    #[inline(always)]
    pub fn is_backface_visible(&self) -> bool {
        match *self {
            FastTransform::Offset(..) => false,
            FastTransform::Transform { ref transform, .. } => transform.is_backface_visible(),
        }
    }

    #[inline(always)]
    pub fn transform_point2d(&self, point: &TypedPoint2D<f32, Src>) -> TypedPoint2D<f32, Dst> {
        match *self {
            FastTransform::Offset(offset) => {
                let new_point = *point + offset;
                TypedPoint2D::from_untyped(&new_point.to_untyped())
            }
            FastTransform::Transform { ref transform, .. } => transform.transform_point2d(point),
        }
    }

    #[inline(always)]
    pub fn transform_point2d_homogeneous(&self, point: &TypedPoint2D<f32, Src>) -> HomogeneousVector<f32, Dst> {
        match *self {
            FastTransform::Offset(offset) => {
                let new_point = *point + offset;
                HomogeneousVector::new(new_point.x, new_point.y, 0.0, 1.0)
            }
            FastTransform::Transform { ref transform, .. } => transform.transform_point2d_homogeneous(point),
        }
    }

    #[inline(always)]
    pub fn transform_rect(&self, rect: &TypedRect<f32, Src>) -> TypedRect<f32, Dst> {
        match *self {
            FastTransform::Offset(offset) =>
                TypedRect::from_untyped(&rect.to_untyped().translate(&offset.to_untyped())),
            FastTransform::Transform { ref transform, .. } => transform.transform_rect(rect),
        }
    }

    pub fn unapply(&self, rect: &TypedRect<f32, Dst>) -> Option<TypedRect<f32, Src>> {
        match *self {
            FastTransform::Offset(offset) =>
                Some(TypedRect::from_untyped(&rect.to_untyped().translate(&-offset.to_untyped()))),
            FastTransform::Transform { inverse: Some(ref inverse), is_2d: true, .. }  =>
                Some(inverse.transform_rect(rect)),
            FastTransform::Transform { ref transform, is_2d: false, .. } =>
                Some(transform.inverse_rect_footprint(rect)),
            FastTransform::Transform { inverse: None, .. }  => None,
        }
    }

    #[inline(always)]
    pub fn offset(&self, new_offset: TypedVector2D<f32, Src>) -> Self {
        match *self {
            FastTransform::Offset(offset) => FastTransform::Offset(offset + new_offset),
            FastTransform::Transform { ref transform, .. } => {
                let transform = transform.pre_translate(new_offset.to_3d());
                FastTransform::with_transform(transform)
            }
        }
    }

    pub fn post_translate(&self, new_offset: TypedVector2D<f32, Dst>) -> Self {
        match *self {
            FastTransform::Offset(offset) => {
                let offset = offset.to_untyped() + new_offset.to_untyped();
                FastTransform::Offset(TypedVector2D::from_untyped(&offset))
            }
            FastTransform::Transform { ref transform, .. } => {
                let transform = transform.post_translate(new_offset.to_3d());
                FastTransform::with_transform(transform)
            }
        }
    }

    #[inline(always)]
    pub fn inverse(&self) -> Option<FastTransform<Dst, Src>> {
        match *self {
            FastTransform::Offset(offset) =>
                Some(FastTransform::Offset(TypedVector2D::new(-offset.x, -offset.y))),
            FastTransform::Transform { transform, inverse: Some(inverse), is_2d, } =>
                Some(FastTransform::Transform {
                    transform: inverse,
                    inverse: Some(transform),
                    is_2d
                }),
            FastTransform::Transform { inverse: None, .. } => None,

        }
    }

    pub fn update(&self, transform: TypedTransform3D<f32, Src, Dst>) -> Option<Self> {
        if transform.is_simple_2d_translation() {
            Some(self.offset(TypedVector2D::new(transform.m41, transform.m42)))
        } else {
            // If we break 2D axis alignment or have a perspective component, we need to start a
            // new incompatible coordinate system with which we cannot share clips without masking.
            None
        }
    }
}

impl<Src, Dst> From<TypedTransform3D<f32, Src, Dst>> for FastTransform<Src, Dst> {
    fn from(transform: TypedTransform3D<f32, Src, Dst>) -> Self {
        FastTransform::with_transform(transform)
    }
}

impl<Src, Dst> Into<TypedTransform3D<f32, Src, Dst>> for FastTransform<Src, Dst> {
    fn into(self) -> TypedTransform3D<f32, Src, Dst> {
        self.to_transform()
    }
}

impl<Src, Dst> From<TypedVector2D<f32, Src>> for FastTransform<Src, Dst> {
    fn from(vector: TypedVector2D<f32, Src>) -> Self {
        FastTransform::with_vector(vector)
    }
}

pub type LayoutFastTransform = FastTransform<LayoutPixel, LayoutPixel>;
pub type LayoutToWorldFastTransform = FastTransform<LayoutPixel, WorldPixel>;
pub type WorldToLayoutFastTransform = FastTransform<WorldPixel, LayoutPixel>;

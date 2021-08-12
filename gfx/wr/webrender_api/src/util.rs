/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::{Point2D, Box2D, Size2D, Vector2D};
use euclid::{default, Transform3D};

// Matches the definition of SK_ScalarNearlyZero in Skia.
const NEARLY_ZERO: f32 = 1.0 / 4096.0;

// Represents an optimized transform where there is only
// a scale and translation (which are guaranteed to maintain
// an axis align rectangle under transformation). The
// scaling is applied first, followed by the translation.
// TODO(gw): We should try and incorporate F <-> T units here,
//           but it's a bit tricky to do that now with the
//           way the current spatial tree works.
#[derive(Debug, Clone, Copy, MallocSizeOf)]
#[derive(Serialize, Deserialize)]
pub struct ScaleOffset {
    pub scale: default::Vector2D<f32>,
    pub offset: default::Vector2D<f32>,
}

impl ScaleOffset {
    pub fn identity() -> Self {
        ScaleOffset {
            scale: Vector2D::new(1.0, 1.0),
            offset: Vector2D::zero(),
        }
    }

    // Construct a ScaleOffset from a transform. Returns
    // None if the matrix is not a pure scale / translation.
    pub fn from_transform<F, T>(
        m: &Transform3D<f32, F, T>,
    ) -> Option<ScaleOffset> {
        // To check that we have a pure scale / translation:
        // Every field must match an identity matrix, except:
        //  - Any value present in tx,ty
        //  - Any value present in sx,sy

        if m.m12.abs() > NEARLY_ZERO ||
           m.m13.abs() > NEARLY_ZERO ||
           m.m14.abs() > NEARLY_ZERO ||
           m.m21.abs() > NEARLY_ZERO ||
           m.m23.abs() > NEARLY_ZERO ||
           m.m24.abs() > NEARLY_ZERO ||
           m.m31.abs() > NEARLY_ZERO ||
           m.m32.abs() > NEARLY_ZERO ||
           (m.m33 - 1.0).abs() > NEARLY_ZERO ||
           m.m34.abs() > NEARLY_ZERO ||
           m.m43.abs() > NEARLY_ZERO ||
           (m.m44 - 1.0).abs() > NEARLY_ZERO {
            return None;
        }

        Some(ScaleOffset {
            scale: Vector2D::new(m.m11, m.m22),
            offset: Vector2D::new(m.m41, m.m42),
        })
    }

    pub fn from_offset(offset: default::Vector2D<f32>) -> Self {
        ScaleOffset {
            scale: Vector2D::new(1.0, 1.0),
            offset,
        }
    }

    pub fn from_scale(scale: default::Vector2D<f32>) -> Self {
        ScaleOffset {
            scale,
            offset: Vector2D::new(0.0, 0.0),
        }
    }

    pub fn inverse(&self) -> Self {
        ScaleOffset {
            scale: Vector2D::new(
                1.0 / self.scale.x,
                1.0 / self.scale.y,
            ),
            offset: Vector2D::new(
                -self.offset.x / self.scale.x,
                -self.offset.y / self.scale.y,
            ),
        }
    }

    pub fn offset(&self, offset: default::Vector2D<f32>) -> Self {
        self.accumulate(
            &ScaleOffset {
                scale: Vector2D::new(1.0, 1.0),
                offset,
            }
        )
    }

    pub fn scale(&self, scale: f32) -> Self {
        self.accumulate(
            &ScaleOffset {
                scale: Vector2D::new(scale, scale),
                offset: Vector2D::zero(),
            }
        )
    }

    /// Produce a ScaleOffset that includes both self and other.
    /// The 'self' ScaleOffset is applied after other.
    /// This is equivalent to `Transform3D::pre_transform`.
    pub fn accumulate(&self, other: &ScaleOffset) -> Self {
        ScaleOffset {
            scale: Vector2D::new(
                self.scale.x * other.scale.x,
                self.scale.y * other.scale.y,
            ),
            offset: Vector2D::new(
                self.offset.x + self.scale.x * other.offset.x,
                self.offset.y + self.scale.y * other.offset.y,
            ),
        }
    }

    pub fn map_rect<F, T>(&self, rect: &Box2D<f32, F>) -> Box2D<f32, T> {
        // TODO(gw): The logic below can return an unexpected result if the supplied
        //           rect is invalid (has size < 0). Since Gecko currently supplied
        //           invalid rects in some cases, adding a max(0) here ensures that
        //           mapping an invalid rect retains the property that rect.is_empty()
        //           will return true (the mapped rect output will have size 0 instead
        //           of a negative size). In future we could catch / assert / fix
        //           these invalid rects earlier, and assert here instead.

        let w = rect.width().max(0.0);
        let h = rect.height().max(0.0);

        let mut x0 = rect.min.x * self.scale.x + self.offset.x;
        let mut y0 = rect.min.y * self.scale.y + self.offset.y;

        let mut sx = w * self.scale.x;
        let mut sy = h * self.scale.y;
        // Handle negative scale. Previously, branchless float math was used to find the
        // min / max vertices and size. However, that sequence of operations was producind
        // additional floating point accuracy on android emulator builds, causing one test
        // to fail an assert. Instead, we retain the same math as previously, and adjust
        // the origin / size if required.

        if self.scale.x < 0.0 {
            x0 += sx;
            sx = -sx;
        }
        if self.scale.y < 0.0 {
            y0 += sy;
            sy = -sy;
        }

        Box2D::from_origin_and_size(
            Point2D::new(x0, y0),
            Size2D::new(sx, sy),
        )
    }

    pub fn unmap_rect<F, T>(&self, rect: &Box2D<f32, F>) -> Box2D<f32, T> {
        // TODO(gw): The logic below can return an unexpected result if the supplied
        //           rect is invalid (has size < 0). Since Gecko currently supplied
        //           invalid rects in some cases, adding a max(0) here ensures that
        //           mapping an invalid rect retains the property that rect.is_empty()
        //           will return true (the mapped rect output will have size 0 instead
        //           of a negative size). In future we could catch / assert / fix
        //           these invalid rects earlier, and assert here instead.

        let w = rect.width().max(0.0);
        let h = rect.height().max(0.0);

        let mut x0 = (rect.min.x - self.offset.x) / self.scale.x;
        let mut y0 = (rect.min.y - self.offset.y) / self.scale.y;

        let mut sx = w / self.scale.x;
        let mut sy = h / self.scale.y;

        // Handle negative scale. Previously, branchless float math was used to find the
        // min / max vertices and size. However, that sequence of operations was producind
        // additional floating point accuracy on android emulator builds, causing one test
        // to fail an assert. Instead, we retain the same math as previously, and adjust
        // the origin / size if required.

        if self.scale.x < 0.0 {
            x0 += sx;
            sx = -sx;
        }
        if self.scale.y < 0.0 {
            y0 += sy;
            sy = -sy;
        }

        Box2D::from_origin_and_size(
            Point2D::new(x0, y0),
            Size2D::new(sx, sy),
        )
    }

    pub fn map_vector<F, T>(&self, vector: &Vector2D<f32, F>) -> Vector2D<f32, T> {
        Vector2D::new(
            vector.x * self.scale.x,
            vector.y * self.scale.y,
        )
    }

    pub fn unmap_vector<F, T>(&self, vector: &Vector2D<f32, F>) -> Vector2D<f32, T> {
        Vector2D::new(
            vector.x / self.scale.x,
            vector.y / self.scale.y,
        )
    }

    pub fn map_point<F, T>(&self, point: &Point2D<f32, F>) -> Point2D<f32, T> {
        Point2D::new(
            point.x * self.scale.x + self.offset.x,
            point.y * self.scale.y + self.offset.y,
        )
    }

    pub fn unmap_point<F, T>(&self, point: &Point2D<f32, F>) -> Point2D<f32, T> {
        Point2D::new(
            (point.x - self.offset.x) / self.scale.x,
            (point.y - self.offset.y) / self.scale.y,
        )
    }

    pub fn to_transform<F, T>(&self) -> Transform3D<f32, F, T> {
        Transform3D::new(
            self.scale.x,
            0.0,
            0.0,
            0.0,

            0.0,
            self.scale.y,
            0.0,
            0.0,

            0.0,
            0.0,
            1.0,
            0.0,

            self.offset.x,
            self.offset.y,
            0.0,
            1.0,
        )
    }
}

pub trait RectHelpers<U>
where
    Self: Sized,
{
    fn snap(&self) -> Self;
}

impl<U> RectHelpers<U> for Box2D<f32, U> {
    fn snap(&self) -> Self {
        self.round()
    }
}

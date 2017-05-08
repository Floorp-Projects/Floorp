/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use webrender_traits::{LayerPoint, LayerSize};
use std::f32::consts::FRAC_PI_2;

/// Number of steps to integrate arc length over.
const STEP_COUNT: usize = 20;

/// Represents an ellipse centred at a local space origin.
#[derive(Debug, Clone)]
pub struct Ellipse {
    pub radius: LayerSize,
}

impl Ellipse {
    pub fn new(radius: LayerSize) -> Ellipse {
        Ellipse {
            radius: radius,
        }
    }

    /// Use Simpsons rule to approximate the arc length of
    /// part of an ellipse. Note that this only works over
    /// the range of [0, pi/2].
    // TODO(gw): This is a simplistic way to estimate the
    // arc length of an ellipse segment. We can probably use
    // a faster / more accurate method!
    fn get_simpson_length(&self, theta: f32) -> f32 {
        let df = theta / STEP_COUNT as f32;
        let mut sum = 0.0;

        for i in 0..(STEP_COUNT+1) {
            let (sin_theta, cos_theta) = (i as f32 * df).sin_cos();
            let a = self.radius.width * sin_theta;
            let b = self.radius.height * cos_theta;
            let y = (a*a + b*b).sqrt();
            let q = if i == 0 || i == STEP_COUNT {
                1.0
            } else if i % 2 == 0 {
                2.0
            } else {
                4.0
            };

            sum += q * y;
        }

        (df / 3.0) * sum
    }

    /// Binary search to estimate the angle of an ellipse
    /// for a given arc length. This only searches over the
    /// first quadrant of an ellipse.
    pub fn find_angle_for_arc_length(&self, arc_length: f32) -> f32 {
        let epsilon = 0.01;
        let mut low = 0.0;
        let mut high = FRAC_PI_2;
        let mut theta = 0.0;

        while low <= high {
            theta = 0.5 * (low + high);
            let length = self.get_simpson_length(theta);

            if (length - arc_length).abs() < epsilon {
                break;
            } else if length < arc_length {
                low = theta;
            } else {
                high = theta;
            }
        }

        theta
    }

    /// Approximate the total length of the first quadrant of
    /// this ellipse.
    pub fn get_quarter_arc_length(&self) -> f32 {
        self.get_simpson_length(FRAC_PI_2)
    }

    /// Get a point and tangent on this ellipse from a given angle.
    /// This only works for the first quadrant of the ellipse.
    pub fn get_point_and_tangent(&self, theta: f32) -> (LayerPoint, LayerPoint) {
        let (sin_theta, cos_theta) = theta.sin_cos();
        let point = LayerPoint::new(self.radius.width * cos_theta,
                                    self.radius.height * sin_theta);
        let tangent = LayerPoint::new(-self.radius.width * sin_theta,
                                       self.radius.height * cos_theta);
        (point, tangent)
    }
}

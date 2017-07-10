/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::LayerPoint;

/// Some arbitrarily small positive number used as threshold value.
pub const EPSILON: f32 = 0.1;

/// The default stiffness factor.
pub const STIFFNESS: f32 = 0.2;

/// The default damping factor.
pub const DAMPING: f32 = 1.0;

#[derive(Copy, Clone, Debug)]
pub struct Spring {
    /// The current position of spring.
    cur: LayerPoint,
    /// The position of spring at previous tick.
    prev: LayerPoint,
    /// The destination of spring.
    dest: LayerPoint,
    /// How hard it springs back.
    stiffness: f32,
    /// Friction. 1.0 means no bounce.
    damping: f32,
}

impl Spring {
    /// Create a new spring at location.
    pub fn at(pos: LayerPoint, stiffness: f32, damping: f32) -> Spring {
        Spring {
            cur: pos,
            prev: pos,
            dest: pos,
            stiffness,
            damping,
        }
    }

    /// Set coords on a spring, mutating spring
    pub fn coords(&mut self, cur: LayerPoint, prev: LayerPoint, dest: LayerPoint) {
        self.cur = cur;
        self.prev = prev;
        self.dest = dest
    }

    pub fn current(&self) -> LayerPoint {
        self.cur
    }

    /// Run one tick of the spring animation. Return true if the animation is complete.
    pub fn animate(&mut self) -> bool {
        if !is_resting(self.cur.x, self.prev.x, self.dest.x) ||
                !is_resting(self.cur.y, self.prev.y, self.dest.y) {
            let next = LayerPoint::new(next(self.cur.x,
                                            self.prev.x,
                                            self.dest.x,
                                            self.stiffness,
                                            self.damping),
                                       next(self.cur.y,
                                            self.prev.y,
                                            self.dest.y,
                                            self.stiffness,
                                            self.damping));
            let (cur, dest) = (self.cur, self.dest);
            self.coords(next, cur, dest);
            false
        } else {
            let dest = self.dest;
            self.coords(dest, dest, dest);
            true
        }
    }
}

/// Given numbers, calculate the next position for a spring.
fn next(cur: f32, prev: f32, dest: f32, stiffness: f32, damping: f32) -> f32 {
    // Calculate spring force
    let fspring = -stiffness * (cur - dest);

    // Calculate velocity
    let vel = cur - prev;

    // Calculate damping force.
    let fdamping = -damping * vel;

    // Calc acceleration by adjusting spring force to damping force
    let acc = fspring + fdamping;

    // Calculate new velocity after adding acceleration. Scale to framerate.
    let nextv = vel + acc;

    // Calculate next position by integrating velocity. Scale to framerate.
    let next = cur + nextv;
    next
}

/// Given numbers, calcluate if a spring is at rest.
fn is_resting(cur: f32, prev: f32, dest: f32) -> bool {
    (cur - prev).abs() < EPSILON && (cur - dest).abs() < EPSILON
}

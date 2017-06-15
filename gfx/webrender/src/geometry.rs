/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::{Rect, Point3D};

/*
 A naive port of "An Efficient and Robust Ray–Box Intersection Algorithm"
 from https://www.cs.utah.edu/~awilliam/box/box.pdf

 This should be cleaned up and extracted into more useful types!
 */

// Assumes rect is in the z=0 plane!
pub fn ray_intersects_rect(ray_origin: Point3D<f32>,
                           ray_end: Point3D<f32>,
                           rect: Rect<f32>) -> bool {
    let mut dir = ray_end - ray_origin;
    let len = ((dir.x*dir.x) + (dir.y*dir.y) + (dir.z*dir.z)).sqrt();
    dir.x = dir.x / len;
    dir.y = dir.y / len;
    dir.z = dir.z / len;
    let inv_direction = Point3D::new(1.0/dir.x, 1.0/dir.y, 1.0/dir.z);

    let sign = [
        if inv_direction.x < 0.0 {
            1
        } else {
            0
        },
        if inv_direction.y < 0.0 {
            1
        } else {
            0
        },
        if inv_direction.z < 0.0 {
            1
        } else {
            0
        },
    ];

    let parameters = [
        rect.origin.to_3d(),
        rect.bottom_right().to_3d(),
    ];

    let mut tmin = (parameters[sign[0]].x - ray_origin.x) * inv_direction.x;
    let mut tmax = (parameters[1-sign[0]].x - ray_origin.x) * inv_direction.x;
    let tymin = (parameters[sign[1]].y - ray_origin.y) * inv_direction.y;
    let tymax = (parameters[1-sign[1]].y - ray_origin.y) * inv_direction.y;
    if (tmin > tymax) || (tymin > tmax) {
        return false;
    }
    if tymin > tmin {
        tmin = tymin;
    }
    if tymax < tmax {
        tmax = tymax;
    }
    let tzmin = (parameters[sign[2]].z - ray_origin.z) * inv_direction.z;
    let tzmax = (parameters[1-sign[2]].z - ray_origin.z) * inv_direction.z;
    if (tmin > tzmax) || (tzmin > tmax) {
        return false;
    }

    // Don't care about where on the ray it hits...
    true

    /*
    if tzmin > tmin {
        tmin = tzmin;
    }
    if tzmax < tmax {
        tmax = tzmax;
    }

    let t0 = 0.0;
    let t1 = len;

    (tmin < t1) && (tmax > t0)
    */
}

/*
pub fn circle_contains_rect(circle_center: &Point2D<f32>,
                        radius: f32,
                        rect: &Rect<f32>) -> bool {
    let dx = (circle_center.x - rect.origin.x).max(rect.origin.x + rect.size.width - circle_center.x);
    let dy = (circle_center.y - rect.origin.y).max(rect.origin.y + rect.size.height - circle_center.y);
    radius * radius >= dx * dx + dy * dy
}

pub fn rect_intersects_circle(circle_center: &Point2D<f32>,
                          radius: f32,
                          rect: &Rect<f32>) -> bool {
    let circle_distance_x = (circle_center.x - (rect.origin.x + rect.size.width * 0.5)).abs();
    let circle_distance_y = (circle_center.y - (rect.origin.y + rect.size.height * 0.5)).abs();

    if circle_distance_x > rect.size.width * 0.5 + radius {
        return false
    }
    if circle_distance_y > rect.size.height * 0.5 + radius {
        return false
    }

    if circle_distance_x <= rect.size.width * 0.5 {
        return true;
    }
    if circle_distance_y <= rect.size.height * 0.5 {
        return true;
    }

    let corner_distance_sq = (circle_distance_x - rect.size.width * 0.5) * (circle_distance_x - rect.size.width * 0.5) +
                             (circle_distance_y - rect.size.height * 0.5) * (circle_distance_y - rect.size.height * 0.5);

    corner_distance_sq <= radius * radius
}
*/

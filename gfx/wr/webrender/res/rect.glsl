/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct RectWithSize {
    vec2 p0;
    vec2 size;
};

struct RectWithEndpoint {
    vec2 p0;
    vec2 p1;
};

float point_inside_rect(vec2 p, vec2 p0, vec2 p1) {
    vec2 s = step(p0, p) - step(p1, p);
    return s.x * s.y;
}

vec2 signed_distance_rect_xy(vec2 pos, vec2 p0, vec2 p1) {
    // Instead of using a true signed distance to rect here, we just use the
    // simpler approximation of the maximum distance on either axis from the
    // outside of the rectangle. This avoids expensive use of length() and only
    // causes mostly imperceptible differences at corner pixels.
    return max(p0 - pos, pos - p1);
}

float signed_distance_rect(vec2 pos, vec2 p0, vec2 p1) {
    // Collapse the per-axis distances to edges to a single approximate value.
    vec2 d = signed_distance_rect_xy(pos, p0, p1);
    return max(d.x, d.y);
}

vec2 rect_clamp(RectWithEndpoint rect, vec2 pt) {
    return clamp(pt, rect.p0, rect.p1);
}

vec2 rect_size(RectWithEndpoint rect) {
    return rect.p1 - rect.p0;
}

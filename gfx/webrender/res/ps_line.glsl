/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_FEATURE_CACHE
    #define PRIMITIVE_HAS_PICTURE_TASK
#endif

#include shared,prim_shared

varying vec4 vColor;
flat varying int vStyle;
flat varying float vAxisSelect;
flat varying vec4 vParams;
flat varying vec2 vLocalOrigin;

#ifdef WR_FEATURE_TRANSFORM
varying vec3 vLocalPos;
#else
varying vec2 vLocalPos;
#endif

#ifdef WR_VERTEX_SHADER
#define LINE_ORIENTATION_VERTICAL       0
#define LINE_ORIENTATION_HORIZONTAL     1

struct Line {
    vec4 color;
    float wavyLineThickness;
    float style;
    float orientation;
};

Line fetch_line(int address) {
    vec4 data[2] = fetch_from_resource_cache_2(address);
    return Line(data[0], data[1].x, data[1].y, data[1].z);
}

void main(void) {
    Primitive prim = load_primitive();
    Line line = fetch_line(prim.specific_prim_address);

    vec2 pos, size;

    switch (int(line.orientation)) {
        case LINE_ORIENTATION_HORIZONTAL:
            vAxisSelect = 0.0;
            pos = prim.local_rect.p0;
            size = prim.local_rect.size;
            break;
        case LINE_ORIENTATION_VERTICAL:
            vAxisSelect = 1.0;
            pos = prim.local_rect.p0.yx;
            size = prim.local_rect.size.yx;
            break;
    }

    vLocalOrigin = pos;
    vStyle = int(line.style);

    switch (vStyle) {
        case LINE_STYLE_SOLID: {
            break;
        }
        case LINE_STYLE_DASHED: {
            float dash_length = size.y * 3.0;
            vParams = vec4(2.0 * dash_length, // period
                           dash_length,       // dash length
                           0.0,
                           0.0);
            break;
        }
        case LINE_STYLE_DOTTED: {
            float diameter = size.y;
            float period = diameter * 2.0;
            float center_line = pos.y + 0.5 * size.y;
            float max_x = floor(size.x / period) * period;
            vParams = vec4(period,
                           diameter / 2.0, // radius
                           center_line,
                           max_x);
            break;
        }
        case LINE_STYLE_WAVY: {
            // This logic copied from gecko to get the same results
            float line_thickness = max(line.wavyLineThickness, 1.0);
            // Difference in height between peaks and troughs
            // (and since slopes are 45 degrees, the length of each slope)
            float slope_length = size.y - line_thickness;
            // Length of flat runs
            float flat_length = max((line_thickness - 1.0) * 2.0, 1.0);

            vParams = vec4(line_thickness / 2.0,
                           slope_length,
                           flat_length,
                           size.y);
            break;
        }
    }

#ifdef WR_FEATURE_CACHE
    vec2 device_origin = prim.task.target_rect.p0 +
                         uDevicePixelRatio * (prim.local_rect.p0 - prim.task.content_origin);
    vec2 device_size = uDevicePixelRatio * prim.local_rect.size;

    vec2 device_pos = mix(device_origin,
                          device_origin + device_size,
                          aPosition.xy);

    vColor = prim.task.color;
    vLocalPos = mix(prim.local_rect.p0,
                    prim.local_rect.p0 + prim.local_rect.size,
                    aPosition.xy);

    gl_Position = uTransform * vec4(device_pos, 0.0, 1.0);
#else
    vColor = line.color;

    #ifdef WR_FEATURE_TRANSFORM
        TransformVertexInfo vi = write_transform_vertex_primitive(prim);
    #else
        VertexInfo vi = write_vertex(prim.local_rect,
                                     prim.local_clip_rect,
                                     prim.z,
                                     prim.layer,
                                     prim.task,
                                     prim.local_rect);
    #endif

    vLocalPos = vi.local_pos;
    write_clip(vi.screen_pos, prim.clip_area);
#endif
}
#endif

#ifdef WR_FRAGMENT_SHADER

#define MAGIC_WAVY_LINE_AA_SNAP         0.7

float det(vec2 a, vec2 b) {
    return a.x * b.y - b.x * a.y;
}

// From: http://research.microsoft.com/en-us/um/people/hoppe/ravg.pdf
vec2 get_distance_vector(vec2 b0, vec2 b1, vec2 b2) {
    float a = det(b0, b2);
    float b = 2.0 * det(b1, b0);
    float d = 2.0 * det(b2, b1);

    float f = b * d - a * a;
    vec2 d21 = b2 - b1;
    vec2 d10 = b1 - b0;
    vec2 d20 = b2 - b0;

    vec2 gf = 2.0 * (b *d21 + d * d10 + a * d20);
    gf = vec2(gf.y,-gf.x);
    vec2 pp = -f * gf / dot(gf, gf);
    vec2 d0p = b0 - pp;
    float ap = det(d0p, d20);
    float bp = 2.0 * det(d10, d0p);

    float t = clamp((ap + bp) / (2.0 * a + b + d), 0.0, 1.0);
    return mix(mix(b0, b1, t), mix(b1,b2,t), t);
}

// Approximate distance from point to quadratic bezier.
float approx_distance(vec2 p, vec2 b0, vec2 b1, vec2 b2) {
    return length(get_distance_vector(b0 - p, b1 - p, b2 - p));
}

void main(void) {
    float alpha = 1.0;

#ifdef WR_FEATURE_CACHE
    vec2 local_pos = vLocalPos;
#else
    #ifdef WR_FEATURE_TRANSFORM
        alpha = 0.0;
        vec2 local_pos = init_transform_fs(vLocalPos, alpha);
    #else
        vec2 local_pos = vLocalPos;
    #endif

        alpha *= do_clip();
#endif

    // Find the appropriate distance to apply the step over.
    float aa_range = compute_aa_range(local_pos);

    // Select the x/y coord, depending on which axis this edge is.
    vec2 pos = mix(local_pos.xy, local_pos.yx, vAxisSelect);

    switch (vStyle) {
        case LINE_STYLE_SOLID: {
            break;
        }
        case LINE_STYLE_DASHED: {
            // Get the main-axis position relative to closest dot or dash.
            float x = mod(pos.x - vLocalOrigin.x, vParams.x);

            // Calculate dash alpha (on/off) based on dash length
            alpha = min(alpha, step(x, vParams.y));
            break;
        }
        case LINE_STYLE_DOTTED: {
            // Get the main-axis position relative to closest dot or dash.
            float x = mod(pos.x - vLocalOrigin.x, vParams.x);

            // Get the dot alpha
            vec2 dot_relative_pos = vec2(x, pos.y) - vParams.yz;
            float dot_distance = length(dot_relative_pos) - vParams.y;
            alpha = min(alpha, distance_aa(aa_range, dot_distance));
            // Clip off partial dots
            alpha *= step(pos.x - vLocalOrigin.x, vParams.w);
            break;
        }
        case LINE_STYLE_WAVY: {
            vec2 normalized_local_pos = pos - vLocalOrigin.xy;

            float half_line_thickness = vParams.x;
            float slope_length = vParams.y;
            float flat_length = vParams.z;
            float vertical_bounds = vParams.w;
            // Our pattern is just two slopes and two flats
            float half_period = slope_length + flat_length;

            float mid_height = vertical_bounds / 2.0;
            float peak_offset = mid_height - half_line_thickness;
            // Flip the wave every half period
            float flip = -2.0 * (step(mod(normalized_local_pos.x, 2.0 * half_period), half_period) - 0.5);
            // float flip = -1.0;
            peak_offset *= flip;
            float peak_height = mid_height + peak_offset;

            // Convert pos to a local position within one half period
            normalized_local_pos.x = mod(normalized_local_pos.x, half_period);

            // Compute signed distance to the 3 lines that make up an arc
            float dist1 = distance_to_line(vec2(0.0, peak_height),
                                           vec2(1.0, -flip),
                                           normalized_local_pos);
            float dist2 = distance_to_line(vec2(0.0, peak_height),
                                           vec2(0, -flip),
                                           normalized_local_pos);
            float dist3 = distance_to_line(vec2(flat_length, peak_height),
                                           vec2(-1.0, -flip),
                                           normalized_local_pos);
            float dist = abs(max(max(dist1, dist2), dist3));

            // Apply AA based on the thickness of the wave
            alpha = distance_aa(aa_range, dist - half_line_thickness);

            // Disable AA for thin lines
            if (half_line_thickness <= 1.0) {
                alpha = 1.0 - step(alpha, MAGIC_WAVY_LINE_AA_SNAP);
            }

            break;
        }
    }

    oFragColor = vColor * alpha;
}
#endif

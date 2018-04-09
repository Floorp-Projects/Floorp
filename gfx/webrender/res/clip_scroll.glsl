/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef WR_VERTEX_SHADER
#define VECS_PER_CLIP_SCROLL_NODE   9

uniform HIGHP_SAMPLER_FLOAT sampler2D sClipScrollNodes;

struct ClipScrollNode {
    mat4 transform;
    mat4 inv_transform;
    bool is_axis_aligned;
};

ClipScrollNode fetch_clip_scroll_node(int index) {
    ClipScrollNode node;

    // Create a UV base coord for each 8 texels.
    // This is required because trying to use an offset
    // of more than 8 texels doesn't work on some versions
    // of OSX.
    ivec2 uv = get_fetch_uv(index, VECS_PER_CLIP_SCROLL_NODE);
    ivec2 uv0 = ivec2(uv.x + 0, uv.y);
    ivec2 uv1 = ivec2(uv.x + 8, uv.y);

    node.transform[0] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(0, 0));
    node.transform[1] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(1, 0));
    node.transform[2] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(2, 0));
    node.transform[3] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(3, 0));

    node.inv_transform[0] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(4, 0));
    node.inv_transform[1] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(5, 0));
    node.inv_transform[2] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(6, 0));
    node.inv_transform[3] = TEXEL_FETCH(sClipScrollNodes, uv0, 0, ivec2(7, 0));

    vec4 misc = TEXEL_FETCH(sClipScrollNodes, uv1, 0, ivec2(0, 0));
    node.is_axis_aligned = misc.x == 0.0;

    return node;
}

// Return the intersection of the plane (set up by "normal" and "point")
// with the ray (set up by "ray_origin" and "ray_dir"),
// writing the resulting scaler into "t".
bool ray_plane(vec3 normal, vec3 pt, vec3 ray_origin, vec3 ray_dir, out float t)
{
    float denom = dot(normal, ray_dir);
    if (abs(denom) > 1e-6) {
        vec3 d = pt - ray_origin;
        t = dot(d, normal) / denom;
        return t >= 0.0;
    }

    return false;
}

// Apply the inverse transform "inv_transform"
// to the reference point "ref" in CSS space,
// producing a local point on a ClipScrollNode plane,
// set by a base point "a" and a normal "n".
vec4 untransform(vec2 ref, vec3 n, vec3 a, mat4 inv_transform) {
    vec3 p = vec3(ref, -10000.0);
    vec3 d = vec3(0, 0, 1.0);

    float t = 0.0;
    // get an intersection of the ClipScrollNode plane with Z axis vector,
    // originated from the "ref" point
    ray_plane(n, a, p, d, t);
    float z = p.z + d.z * t; // Z of the visible point on the ClipScrollNode

    vec4 r = inv_transform * vec4(ref, z, 1.0);
    return r;
}

// Given a CSS space position, transform it back into the ClipScrollNode space.
vec4 get_node_pos(vec2 pos, ClipScrollNode node) {
    // get a point on the scroll node plane
    vec4 ah = node.transform * vec4(0.0, 0.0, 0.0, 1.0);
    vec3 a = ah.xyz / ah.w;

    // get the normal to the scroll node plane
    vec3 n = transpose(mat3(node.inv_transform)) * vec3(0.0, 0.0, 1.0);
    return untransform(pos, n, a, node.inv_transform);
}

#endif //WR_VERTEX_SHADER


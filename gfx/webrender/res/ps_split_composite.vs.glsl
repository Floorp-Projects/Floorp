#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct SplitGeometry {
    vec3 points[4];
};

SplitGeometry fetch_split_geometry(int address) {
    ivec2 uv = get_resource_cache_uv(address);

    vec4 data0 = texelFetchOffset(sResourceCache, uv, 0, ivec2(0, 0));
    vec4 data1 = texelFetchOffset(sResourceCache, uv, 0, ivec2(1, 0));
    vec4 data2 = texelFetchOffset(sResourceCache, uv, 0, ivec2(2, 0));

    SplitGeometry geo;
    geo.points = vec3[4](
        data0.xyz, vec3(data0.w, data1.xy),
        vec3(data1.zw, data2.x), data2.yzw
    );
    return geo;
}

vec3 bilerp(vec3 a, vec3 b, vec3 c, vec3 d, float s, float t) {
    vec3 x = mix(a, b, t);
    vec3 y = mix(c, d, t);
    return mix(x, y, s);
}

void main(void) {
    CompositeInstance ci = fetch_composite_instance();
    SplitGeometry geometry = fetch_split_geometry(ci.user_data0);
    AlphaBatchTask src_task = fetch_alpha_batch_task(ci.src_task_index);

    vec3 world_pos = bilerp(geometry.points[0], geometry.points[1],
                            geometry.points[3], geometry.points[2],
                            aPosition.y, aPosition.x);
    vec4 final_pos = vec4(world_pos.xy * uDevicePixelRatio, ci.z, 1.0);

    gl_Position = uTransform * final_pos;

    vec2 uv_origin = src_task.render_target_origin;
    vec2 uv_pos = uv_origin + world_pos.xy - src_task.screen_space_origin;
    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0));
    vUv = vec3(uv_pos / texture_size, src_task.render_target_layer_index);
    vUvTaskBounds = vec4(uv_origin, uv_origin + src_task.size) / texture_size.xyxy;
    vUvSampleBounds = vec4(uv_origin + 0.5, uv_origin + src_task.size - 0.5) / texture_size.xyxy;
}

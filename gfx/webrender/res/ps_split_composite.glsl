/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vUvTaskBounds;
flat varying vec4 vUvSampleBounds;

#ifdef WR_VERTEX_SHADER
struct SplitGeometry {
    vec3 points[4];
};

SplitGeometry fetch_split_geometry(int address) {
    ivec2 uv = get_resource_cache_uv(address);

    vec4 data0 = TEXEL_FETCH(sResourceCache, uv, 0, ivec2(0, 0));
    vec4 data1 = TEXEL_FETCH(sResourceCache, uv, 0, ivec2(1, 0));
    vec4 data2 = TEXEL_FETCH(sResourceCache, uv, 0, ivec2(2, 0));

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

struct SplitCompositeInstance {
    int render_task_index;
    int src_task_index;
    int polygons_address;
    float z;
};

SplitCompositeInstance fetch_composite_instance() {
    SplitCompositeInstance ci;

    ci.render_task_index = aData0.x;
    ci.src_task_index = aData0.y;
    ci.polygons_address = aData0.z;
    ci.z = float(aData0.w);

    return ci;
}

void main(void) {
    SplitCompositeInstance ci = fetch_composite_instance();
    SplitGeometry geometry = fetch_split_geometry(ci.polygons_address);
    PictureTask src_task = fetch_picture_task(ci.src_task_index);
    PictureTask dest_task = fetch_picture_task(ci.render_task_index);

    vec2 dest_origin = dest_task.common_data.task_rect.p0 -
                       dest_task.content_origin;

    vec3 world_pos = bilerp(geometry.points[0], geometry.points[1],
                            geometry.points[3], geometry.points[2],
                            aPosition.y, aPosition.x);
    vec4 final_pos = vec4((world_pos.xy + dest_origin) * uDevicePixelRatio, ci.z, 1.0);

    gl_Position = uTransform * final_pos;

    vec2 uv_origin = src_task.common_data.task_rect.p0;
    vec2 uv_pos = uv_origin + world_pos.xy - src_task.content_origin;
    vec2 texture_size = vec2(textureSize(sCacheRGBA8, 0));
    vUv = vec3(uv_pos / texture_size, src_task.common_data.texture_layer_index);
    vUvTaskBounds = vec4(uv_origin, uv_origin + src_task.common_data.task_rect.size) / texture_size.xyxy;
    vUvSampleBounds = vec4(uv_origin + 0.5, uv_origin + src_task.common_data.task_rect.size - 0.5) / texture_size.xyxy;
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    bvec4 inside = lessThanEqual(vec4(vUvTaskBounds.xy, vUv.xy),
                                 vec4(vUv.xy, vUvTaskBounds.zw));
    if (all(inside)) {
        vec2 uv = clamp(vUv.xy, vUvSampleBounds.xy, vUvSampleBounds.zw);
        oFragColor = textureLod(sCacheRGBA8, vec3(uv, vUv.z), 0.0);
    } else {
        oFragColor = vec4(0.0);
    }
}
#endif

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include shared,prim_shared

varying vec3 vUv;
flat varying vec4 vUvSampleBounds;

#ifdef WR_VERTEX_SHADER
struct SplitGeometry {
    vec2 local[4];
};

SplitGeometry fetch_split_geometry(int address) {
    ivec2 uv = get_gpu_cache_uv(address);

    vec4 data0 = TEXEL_FETCH(sGpuCache, uv, 0, ivec2(0, 0));
    vec4 data1 = TEXEL_FETCH(sGpuCache, uv, 0, ivec2(1, 0));

    SplitGeometry geo;
    geo.local = vec2[4](
        data0.xy,
        data0.zw,
        data1.xy,
        data1.zw
    );

    return geo;
}

vec2 bilerp(vec2 a, vec2 b, vec2 c, vec2 d, float s, float t) {
    vec2 x = mix(a, b, t);
    vec2 y = mix(c, d, t);
    return mix(x, y, s);
}

struct SplitCompositeInstance {
    int prim_header_index;
    int polygons_address;
    float z;
};

SplitCompositeInstance fetch_composite_instance() {
    SplitCompositeInstance ci;

    ci.prim_header_index = aData.x;
    ci.polygons_address = aData.y;
    ci.z = float(aData.z);

    return ci;
}

void main(void) {
    SplitCompositeInstance ci = fetch_composite_instance();
    SplitGeometry geometry = fetch_split_geometry(ci.polygons_address);
    PrimitiveHeader ph = fetch_prim_header(ci.prim_header_index);
    PictureTask dest_task = fetch_picture_task(ph.render_task_index);
    Transform transform = fetch_transform(ph.transform_id);
    ImageResource res = fetch_image_resource(ph.user_data.x);
    ImageResourceExtra extra_data = fetch_image_resource_extra(ph.user_data.x);
    ClipArea clip_area = fetch_clip_area(ph.clip_task_index);

    vec2 dest_origin = dest_task.common_data.task_rect.p0 -
                       dest_task.content_origin;

    vec2 local_pos = bilerp(geometry.local[0], geometry.local[1],
                            geometry.local[3], geometry.local[2],
                            aPosition.y, aPosition.x);
    vec4 world_pos = transform.m * vec4(local_pos, 0.0, 1.0);

    vec4 final_pos = vec4(
        dest_origin * world_pos.w + world_pos.xy * dest_task.common_data.device_pixel_scale,
        world_pos.w * ci.z,
        world_pos.w
    );

    write_clip(
        world_pos,
        vec2(0.0),
        clip_area
    );

    gl_Position = uTransform * final_pos;

    vec2 texture_size = vec2(textureSize(sPrevPassColor, 0));
    vec2 uv0 = res.uv_rect.p0;
    vec2 uv1 = res.uv_rect.p1;

    vec2 min_uv = min(uv0, uv1);
    vec2 max_uv = max(uv0, uv1);

    vUvSampleBounds = vec4(
        min_uv + vec2(0.5),
        max_uv - vec2(0.5)
    ) / texture_size.xyxy;

    vec2 f = (local_pos - ph.local_rect.p0) / ph.local_rect.size;

    f = bilerp(
        extra_data.st_tl, extra_data.st_tr,
        extra_data.st_bl, extra_data.st_br,
        f.y, f.x
    );
    vec2 uv = mix(uv0, uv1, f);

    vUv = vec3(uv / texture_size, res.layer);
}
#endif

#ifdef WR_FRAGMENT_SHADER
void main(void) {
    float alpha = do_clip();
    vec2 uv = clamp(vUv.xy, vUvSampleBounds.xy, vUvSampleBounds.zw);
    oFragColor = alpha * textureLod(sPrevPassColor, vec3(uv, vUv.z), 0.0);
}
#endif

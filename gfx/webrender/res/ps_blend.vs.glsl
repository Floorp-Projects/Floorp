#line 1
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

struct Blend {
    ivec4 src_id_target_id_op_amount;
    int z;
};

Blend fetch_blend() {
    PrimitiveInstance pi = fetch_prim_instance();

    Blend blend;
    blend.src_id_target_id_op_amount = ivec4(pi.user_data.x,
                                             pi.render_task_index,
                                             pi.sub_index,
                                             pi.user_data.y);
    blend.z = pi.z;

    return blend;
}

void main(void) {
    Blend blend = fetch_blend();
    Tile src = fetch_tile(blend.src_id_target_id_op_amount.x);
    Tile dest = fetch_tile(blend.src_id_target_id_op_amount.y);

    vec2 dest_origin = dest.screen_origin_task_origin.zw -
                       dest.screen_origin_task_origin.xy +
                       src.screen_origin_task_origin.xy;

    vec2 local_pos = mix(dest_origin,
                         dest_origin + src.size_target_index.xy,
                         aPosition.xy);

    vec2 texture_size = vec2(textureSize(sCache, 0));
    vec2 st0 = src.screen_origin_task_origin.zw / texture_size;
    vec2 st1 = (src.screen_origin_task_origin.zw + src.size_target_index.xy) / texture_size;
    vUv = vec3(mix(st0, st1, aPosition.xy), src.size_target_index.z);

    vOp = blend.src_id_target_id_op_amount.z;
    vAmount = float(blend.src_id_target_id_op_amount.w) / 65535.0;

    gl_Position = uTransform * vec4(local_pos, blend.z, 1.0);
}

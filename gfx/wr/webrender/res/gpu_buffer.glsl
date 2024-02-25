/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

uniform HIGHP_SAMPLER_FLOAT sampler2D sGpuBufferF;
uniform HIGHP_SAMPLER_FLOAT isampler2D sGpuBufferI;

ivec2 get_gpu_buffer_uv(HIGHP_FS_ADDRESS int address) {
    return ivec2(uint(address) % WR_MAX_VERTEX_TEXTURE_WIDTH,
                 uint(address) / WR_MAX_VERTEX_TEXTURE_WIDTH);
}

vec4 fetch_from_gpu_buffer_1f(HIGHP_FS_ADDRESS int address) {
    ivec2 uv = get_gpu_buffer_uv(address);
    return texelFetch(sGpuBufferF, uv, 0);
}

vec4[2] fetch_from_gpu_buffer_2f(HIGHP_FS_ADDRESS int address) {
    ivec2 uv = get_gpu_buffer_uv(address);
    return vec4[2](
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(0, 0)),
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(1, 0))
    );
}

vec4[3] fetch_from_gpu_buffer_3f(HIGHP_FS_ADDRESS int address) {
    ivec2 uv = get_gpu_buffer_uv(address);
    return vec4[3](
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(0, 0)),
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(1, 0)),
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(2, 0))
    );
}

vec4[4] fetch_from_gpu_buffer_4f(HIGHP_FS_ADDRESS int address) {
    ivec2 uv = get_gpu_buffer_uv(address);
    return vec4[4](
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(0, 0)),
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(1, 0)),
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(2, 0)),
        TEXEL_FETCH(sGpuBufferF, uv, 0, ivec2(3, 0))
    );
}

ivec4 fetch_from_gpu_buffer_1i(HIGHP_FS_ADDRESS int address) {
    ivec2 uv = get_gpu_buffer_uv(address);
    return texelFetch(sGpuBufferI, uv, 0);
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The multi-brush shader is capable of rendering most types of brushes
// if they are enabled via WR_FEATURE_*_BRUSH defines.
// This type of uber-shader comes at a cost so the goal for this is to
// provide opportunities for aggressive batching when the number of draw
// calls so high that reducing the number of draw calls is worth the
// cost of this "über-shader".


#define WR_FEATURE_MULTI_BRUSH

// These constants must match the BrushShaderKind enum in gpu_types.rs.
#define BRUSH_KIND_SOLID            0x1000000
#define BRUSH_KIND_IMAGE            0x2000000
#define BRUSH_KIND_TEXT             0x3000000
#define BRUSH_KIND_LINEAR_GRADIENT  0x4000000
#define BRUSH_KIND_RADIAL_GRADIENT  0x5000000
#define BRUSH_KIND_BLEND            0x6000000
#define BRUSH_KIND_MIX_BLEND        0x7000000
#define BRUSH_KIND_YV               0x8000000

int vecs_per_brush(int brush_kind);

#include shared,prim_shared,brush

#ifdef WR_FEATURE_IMAGE_BRUSH
#include brush_image
#endif

#undef VECS_PER_SPECIFIC_BRUSH
#undef WR_BRUSH_VS_FUNCTION
#undef WR_BRUSH_FS_FUNCTION

#ifdef WR_FEATURE_SOLID_BRUSH
#include brush_solid
#endif

#undef VECS_PER_SPECIFIC_BRUSH
#undef WR_BRUSH_VS_FUNCTION
#undef WR_BRUSH_FS_FUNCTION

#ifdef WR_FEATURE_BLEND_BRUSH
#include brush_blend
#endif

#undef VECS_PER_SPECIFIC_BRUSH
#undef WR_BRUSH_VS_FUNCTION
#undef WR_BRUSH_FS_FUNCTION

#ifdef WR_FEATURE_MIX_BLEND_BRUSH
#include brush_mix_blend
#endif

#undef VECS_PER_SPECIFIC_BRUSH
#undef WR_BRUSH_VS_FUNCTION
#undef WR_BRUSH_FS_FUNCTION

#ifdef WR_FEATURE_LINEAR_GRADIENT_BRUSH
#include brush_linear_gradient
#endif

#undef VECS_PER_SPECIFIC_BRUSH
#undef WR_BRUSH_VS_FUNCTION
#undef WR_BRUSH_FS_FUNCTION

#ifdef WR_FEATURE_RADIAL_GRADIENT_BRUSH
#include brush_radial_gradient
#endif

int vecs_per_brush(int brush_kind) {
    switch (brush_kind) {
        // The default arm should never be taken, we let it point to whichever shader
        // is enabled first to satisfy ANGLE validation.
        default:

        #ifdef WR_FEATURE_IMAGE_BRUSH
        case BRUSH_KIND_IMAGE: return VECS_PER_IMAGE_BRUSH;
        #endif

        #ifdef WR_FEATURE_IMAGE_BRUSH
        case BRUSH_KIND_SOLID: return VECS_PER_SOLID_BRUSH;
        #endif

        #ifdef WR_FEATURE_BLEND_BRUSH
        case BRUSH_KIND_BLEND: return VECS_PER_BLEND_BRUSH;
        #endif

        #ifdef WR_FEATURE_MIX_BLEND_BRUSH
        case BRUSH_KIND_MIX_BLEND: return VECS_PER_MIX_BLEND_BRUSH;
        #endif

        #ifdef WR_FEATURE_LINEAR_GRADIENT_BRUSH
        case BRUSH_KIND_LINEAR_GRADIENT: return VECS_PER_LINEAR_GRADIENT_BRUSH;
        #endif

        #ifdef WR_FEATURE_RADIAL_GRADIENT_BRUSH
        case BRUSH_KIND_RADIAL_GRADIENT: return VECS_PER_RADIAL_GRADIENT_BRUSH;
        #endif
    }
}

#define BRUSH_VS_PARAMS vi, prim_address, local_rect, segment_rect, \
    prim_user_data, specific_resource_address, transform, pic_task, \
    brush_flags, texel_rect


#ifdef WR_VERTEX_SHADER
void multi_brush_vs(
    VertexInfo vi,
    int prim_address,
    RectWithSize local_rect,
    RectWithSize segment_rect,
    ivec4 prim_user_data,
    int specific_resource_address,
    mat4 transform,
    PictureTask pic_task,
    int brush_flags,
    vec4 texel_rect,
    int brush_kind
) {
    switch (brush_kind) {
        default:

        #ifdef WR_FEATURE_IMAGE_BRUSH
        case BRUSH_KIND_IMAGE:
            image_brush_vs(BRUSH_VS_PARAMS);
            break;
        #endif

        #ifdef WR_FEATURE_SOLID_BRUSH
        case BRUSH_KIND_SOLID:
            solid_brush_vs(BRUSH_VS_PARAMS);
            break;
        #endif

        #ifdef WR_FEATURE_BLEND_BRUSH
        case BRUSH_KIND_BLEND:
            blend_brush_vs(BRUSH_VS_PARAMS);
            break;
        #endif

        #ifdef WR_FEATURE_MIX_BLEND_BRUSH
        case BRUSH_KIND_MIX_BLEND:
            mix_blend_brush_vs(BRUSH_VS_PARAMS);
            break;
        #endif

        #ifdef WR_FEATURE_LINEAR_GRADIENT_BRUSH
        case BRUSH_KIND_LINEAR_GRADIENT:
            linear_gradient_brush_vs(BRUSH_VS_PARAMS);
            break;
        #endif

        #ifdef WR_FEATURE_RADIAL_GRADIENT_BRUSH
        case BRUSH_KIND_RADIAL_GRADIENT:
            radial_gradient_brush_vs(BRUSH_VS_PARAMS);
            break;
        #endif
    }
}

#endif // WR_VERTEX_SHADER

#ifdef WR_FRAGMENT_SHADER

Fragment multi_brush_fs(int brush_kind) {
    switch (brush_kind) {
        default:

        #ifdef WR_FEATURE_IMAGE_BRUSH
        case BRUSH_KIND_IMAGE: return image_brush_fs();
        #endif

        #ifdef WR_FEATURE_SOLID_BRUSH
        case BRUSH_KIND_SOLID: return solid_brush_fs();
        #endif

        #ifdef WR_FEATURE_BLEND_BRUSH
        case BRUSH_KIND_BLEND: return blend_brush_fs();
        #endif

        #ifdef WR_FEATURE_MIX_BLEND_BRUSH
        case BRUSH_KIND_MIX_BLEND: return mix_blend_brush_fs();
        #endif

        #ifdef WR_FEATURE_LINEAR_GRADIENT_BRUSH
        case BRUSH_KIND_LINEAR_GRADIENT: return linear_gradient_brush_fs();
        #endif

        #ifdef WR_FEATURE_RADIAL_GRADIENT_BRUSH
        case BRUSH_KIND_RADIAL_GRADIENT: return radial_gradient_brush_fs();
        #endif
    }
}

#endif

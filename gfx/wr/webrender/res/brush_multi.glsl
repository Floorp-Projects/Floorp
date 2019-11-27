/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define WR_FEATURE_MULTI_BRUSH

#include shared,prim_shared

int vecs_per_brush(int brush_kind);

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
    int result;
    switch (brush_kind) {
        #ifdef WR_FEATURE_IMAGE_BRUSH
        case BRUSH_KIND_IMAGE:
            result = VECS_PER_IMAGE_BRUSH;
            break;
        #endif

        #ifdef WR_FEATURE_IMAGE_BRUSH
        case BRUSH_KIND_SOLID:
            result = VECS_PER_SOLID_BRUSH;
            break;
        #endif

        #ifdef WR_FEATURE_BLEND_BRUSH
        case BRUSH_KIND_BLEND:
            result = VECS_PER_BLEND_BRUSH;
            break;
        #endif

        #ifdef WR_FEATURE_MIX_BLEND_BRUSH
        case BRUSH_KIND_MIX_BLEND:
            result = VECS_PER_MIX_BLEND_BRUSH;
            break;
        #endif

        #ifdef WR_FEATURE_LINEAR_GRADIENT_BRUSH
        case BRUSH_KIND_LINEAR_GRADIENT:
            result = VECS_PER_LINEAR_GRADIENT_BRUSH;
            break;
        #endif

        #ifdef WR_FEATURE_RADIAL_GRADIENT_BRUSH
        case BRUSH_KIND_RADIAL_GRADIENT:
            result = VECS_PER_RADIAL_GRADIENT_BRUSH;
            break;
        #endif
    }

    return result;
}

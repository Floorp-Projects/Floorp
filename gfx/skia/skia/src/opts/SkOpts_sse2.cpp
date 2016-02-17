/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkOpts.h"
#define SK_OPTS_NS sk_sse2
#include "SkBlitMask_opts.h"
#include "SkBlitRow_opts.h"
#include "SkBlurImageFilter_opts.h"
#include "SkColorCubeFilter_opts.h"
#include "SkMatrix_opts.h"
#include "SkMorphologyImageFilter_opts.h"
#include "SkSwizzler_opts.h"
#include "SkUtils_opts.h"
#include "SkXfermode_opts.h"

namespace SkOpts {
    void Init_sse2() {
        memset16 = sk_sse2::memset16;
        memset32 = sk_sse2::memset32;
        create_xfermode = sk_sse2::create_xfermode;
        color_cube_filter_span = sk_sse2::color_cube_filter_span;

        box_blur_xx = sk_sse2::box_blur_xx;
        box_blur_xy = sk_sse2::box_blur_xy;
        box_blur_yx = sk_sse2::box_blur_yx;

        dilate_x = sk_sse2::dilate_x;
        dilate_y = sk_sse2::dilate_y;
        erode_x = sk_sse2::erode_x;
        erode_y = sk_sse2::erode_y;

        blit_mask_d32_a8 = sk_sse2::blit_mask_d32_a8;

        blit_row_color32 = sk_sse2::blit_row_color32;

        matrix_translate = sk_sse2::matrix_translate;
        matrix_scale_translate = sk_sse2::matrix_scale_translate;
        matrix_affine = sk_sse2::matrix_affine;

        premul_xxxa = sk_sse2::premul_xxxa;
        swaprb_xxxa = sk_sse2::swaprb_xxxa;
        premul_swaprb_xxxa = sk_sse2::premul_swaprb_xxxa;
    }
}


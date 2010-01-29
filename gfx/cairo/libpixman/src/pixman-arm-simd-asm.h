/*
 * Copyright © 2008 Mozilla Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Mozilla Corporation not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Mozilla Corporation makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Author:  Jeff Muizelaar (jeff@infidigm.net)
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pixman-private.h"

void
arm_composite_add_8000_8000 (pixman_implementation_t * impl,
			     pixman_op_t               op,
			     pixman_image_t *          src_image,
			     pixman_image_t *          mask_image,
			     pixman_image_t *          dst_image,
			     int32_t                   src_x,
			     int32_t                   src_y,
			     int32_t                   mask_x,
			     int32_t                   mask_y,
			     int32_t                   dest_x,
			     int32_t                   dest_y,
			     int32_t                   width,
			     int32_t                   height);

void
arm_composite_over_8888_8888 (pixman_implementation_t * impl,
			      pixman_op_t               op,
			      pixman_image_t *          src_image,
			      pixman_image_t *          mask_image,
			      pixman_image_t *          dst_image,
			      int32_t                   src_x,
			      int32_t                   src_y,
			      int32_t                   mask_x,
			      int32_t                   mask_y,
			      int32_t                   dest_x,
			      int32_t                   dest_y,
			      int32_t                   width,
			      int32_t                   height);

void
arm_composite_over_8888_n_8888 (pixman_implementation_t * impl,
				pixman_op_t               op,
				pixman_image_t *          src_image,
				pixman_image_t *          mask_image,
				pixman_image_t *          dst_image,
				int32_t                   src_x,
				int32_t                   src_y,
				int32_t                   mask_x,
				int32_t                   mask_y,
				int32_t                   dest_x,
				int32_t                   dest_y,
				int32_t                   width,
				int32_t                   height);

void
arm_composite_over_n_8_8888 (pixman_implementation_t * impl,
			     pixman_op_t               op,
			     pixman_image_t *          src_image,
			     pixman_image_t *          mask_image,
			     pixman_image_t *          dst_image,
			     int32_t                   src_x,
			     int32_t                   src_y,
			     int32_t                   mask_x,
			     int32_t                   mask_y,
			     int32_t                   dest_x,
			     int32_t                   dest_y,
			     int32_t                   width,
			     int32_t                   height);
void
arm_composite_src_8888_0565 (pixman_implementation_t * impl,
			     pixman_op_t               op,
			     pixman_image_t *          src_image,
			     pixman_image_t *          mask_image,
			     pixman_image_t *          dst_image,
			     int32_t                   src_x,
			     int32_t                   src_y,
			     int32_t                   mask_x,
			     int32_t                   mask_y,
			     int32_t                   dest_x,
			     int32_t                   dest_y,
			     int32_t                   width,
			     int32_t                   height);

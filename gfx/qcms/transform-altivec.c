/* vim: set ts=8 sw=8 noexpandtab: */
//  qcms
//  Copyright (C) 2009 Mozilla Corporation
//  Copyright (C) 1998-2007 Marti Maria
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <altivec.h>

#include "qcmsint.h"

#define FLOATSCALE (float)(PRECACHE_OUTPUT_SIZE)
#define CLAMPMAXVAL (((float) (PRECACHE_OUTPUT_SIZE - 1)) / PRECACHE_OUTPUT_SIZE)
static const ALIGN float floatScaleX4 = FLOATSCALE;
static const ALIGN float clampMaxValueX4 = CLAMPMAXVAL;

inline vector float load_aligned_float(float *dataPtr)
{
	vector float data = vec_lde(0, dataPtr);
	vector unsigned char moveToStart = vec_lvsl(0, dataPtr);
	return vec_perm(data, data, moveToStart);
}

void qcms_transform_data_rgb_out_lut_altivec(qcms_transform *transform,
                                             unsigned char *src,
                                             unsigned char *dest,
                                             size_t length)
{
	unsigned int i;
	float (*mat)[4] = transform->matrix;
	char input_back[32];
	/* Ensure we have a buffer that's 16 byte aligned regardless of the original
	 * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
	 * because they don't work on stack variables. gcc 4.4 does do the right thing
	 * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
	float const *input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
	/* share input and output locations to save having to keep the
 	 * locations in separate registers */
	uint32_t const *output = (uint32_t*)input;

	/* deref *transform now to avoid it in loop */
	const float *igtbl_r = transform->input_gamma_table_r;
	const float *igtbl_g = transform->input_gamma_table_g;
	const float *igtbl_b = transform->input_gamma_table_b;

	/* deref *transform now to avoid it in loop */
	const uint8_t *otdata_r = &transform->output_table_r->data[0];
	const uint8_t *otdata_g = &transform->output_table_g->data[0];
	const uint8_t *otdata_b = &transform->output_table_b->data[0];

	/* input matrix values never change */
	const vector float mat0 = vec_ldl(0, (vector float*)mat[0]);
	const vector float mat1 = vec_ldl(0, (vector float*)mat[1]);
	const vector float mat2 = vec_ldl(0, (vector float*)mat[2]);

	/* these values don't change, either */
	const vector float max = vec_splat(vec_lde(0, (float*)&clampMaxValueX4), 0);
	const vector float min = (vector float)vec_splat_u32(0);
	const vector float scale = vec_splat(vec_lde(0, (float*)&floatScaleX4), 0);

	/* working variables */
	vector float vec_r, vec_g, vec_b, result;

	/* CYA */
	if (!length)
		return;

	/* one pixel is handled outside of the loop */
	length--;

	/* setup for transforming 1st pixel */
	vec_r = load_aligned_float((float*)&igtbl_r[src[0]]);
	vec_g = load_aligned_float((float*)&igtbl_r[src[1]]);
	vec_b = load_aligned_float((float*)&igtbl_r[src[2]]);
	src += 3;

	/* transform all but final pixel */

	for (i=0; i<length; i++)
	{
		/* position values from gamma tables */
		vec_r = vec_splat(vec_r, 0);
		vec_g = vec_splat(vec_g, 0);
		vec_b = vec_splat(vec_b, 0);

		/* gamma * matrix */
		vec_r = vec_madd(vec_r, mat0, min);
		vec_g = vec_madd(vec_g, mat1, min);
		vec_b = vec_madd(vec_b, mat2, min);

		/* crunch, crunch, crunch */
		vec_r = vec_add(vec_r, vec_add(vec_g, vec_b));
		vec_r = vec_max(min, vec_r);
		vec_r = vec_min(max, vec_r);
		result = vec_madd(vec_r, scale, min);

		/* store calc'd output tables indices */
		vec_st(vec_ctu(vec_round(result), 0), 0, (vector unsigned int*)output);

		/* load for next loop while store completes */
		vec_r = load_aligned_float((float*)&igtbl_r[src[0]]);
		vec_g = load_aligned_float((float*)&igtbl_r[src[1]]);
		vec_b = load_aligned_float((float*)&igtbl_r[src[2]]);
		src += 3;

		/* use calc'd indices to output RGB values */
		dest[0] = otdata_r[output[0]];
		dest[1] = otdata_g[output[1]];
		dest[2] = otdata_b[output[2]];
		dest += 3;
	}

	/* handle final (maybe only) pixel */

	vec_r = vec_splat(vec_r, 0);
	vec_g = vec_splat(vec_g, 0);
	vec_b = vec_splat(vec_b, 0);

	vec_r = vec_madd(vec_r, mat0, min);
	vec_g = vec_madd(vec_g, mat1, min);
	vec_b = vec_madd(vec_b, mat2, min);

	vec_r = vec_add(vec_r, vec_add(vec_g, vec_b));
	vec_r = vec_max(min, vec_r);
	vec_r = vec_min(max, vec_r);
	result = vec_madd(vec_r, scale, min);

	vec_st(vec_ctu(vec_round(result),0),0,(vector unsigned int*)output);

	dest[0] = otdata_r[output[0]];
	dest[1] = otdata_g[output[1]];
	dest[2] = otdata_b[output[2]];
}

void qcms_transform_data_rgba_out_lut_altivec(qcms_transform *transform,
                                              unsigned char *src,
                                              unsigned char *dest,
                                              size_t length)
{
	unsigned int i;
	float (*mat)[4] = transform->matrix;
	char input_back[32];
	/* Ensure we have a buffer that's 16 byte aligned regardless of the original
	 * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
	 * because they don't work on stack variables. gcc 4.4 does do the right thing
	 * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
	float const *input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
	/* share input and output locations to save having to keep the
	 * locations in separate registers */
	uint32_t const *output = (uint32_t*)input;

	/* deref *transform now to avoid it in loop */
	const float *igtbl_r = transform->input_gamma_table_r;
	const float *igtbl_g = transform->input_gamma_table_g;
	const float *igtbl_b = transform->input_gamma_table_b;

	/* deref *transform now to avoid it in loop */
	const uint8_t *otdata_r = &transform->output_table_r->data[0];
	const uint8_t *otdata_g = &transform->output_table_g->data[0];
	const uint8_t *otdata_b = &transform->output_table_b->data[0];

	/* input matrix values never change */
	const vector float mat0 = vec_ldl(0, (vector float*)mat[0]);
	const vector float mat1 = vec_ldl(0, (vector float*)mat[1]);
	const vector float mat2 = vec_ldl(0, (vector float*)mat[2]);

	/* these values don't change, either */
	const vector float max = vec_splat(vec_lde(0, (float*)&clampMaxValueX4), 0);
	const vector float min = (vector float)vec_splat_u32(0);
	const vector float scale = vec_splat(vec_lde(0, (float*)&floatScaleX4), 0);

	/* working variables */
	vector float vec_r, vec_g, vec_b, result;
	unsigned char alpha;

	/* CYA */
	if (!length)
		return;

	/* one pixel is handled outside of the loop */
	length--;

	/* setup for transforming 1st pixel */
	vec_r = load_aligned_float((float*)&igtbl_r[src[0]]);
	vec_g = load_aligned_float((float*)&igtbl_r[src[1]]);
	vec_b = load_aligned_float((float*)&igtbl_r[src[2]]);
	alpha = src[3];
	src += 4;

	/* transform all but final pixel */

	for (i=0; i<length; i++)
	{
		/* position values from gamma tables */
		vec_r = vec_splat(vec_r, 0);
		vec_g = vec_splat(vec_g, 0);
		vec_b = vec_splat(vec_b, 0);

		/* gamma * matrix */
		vec_r = vec_madd(vec_r, mat0, min);
		vec_g = vec_madd(vec_g, mat1, min);
		vec_b = vec_madd(vec_b, mat2, min);

		/* store alpha for this pixel; load alpha for next */
		dest[3] = alpha;
		alpha = src[3];

		/* crunch, crunch, crunch */
		vec_r = vec_add(vec_r, vec_add(vec_g, vec_b));
		vec_r = vec_max(min, vec_r);
		vec_r = vec_min(max, vec_r);
		result = vec_madd(vec_r, scale, min);

		/* store calc'd output tables indices */
		vec_st(vec_ctu(vec_round(result), 0), 0, (vector unsigned int*)output);

		/* load gamma values for next loop while store completes */
		vec_r = load_aligned_float((float*)&igtbl_r[src[0]]);
		vec_g = load_aligned_float((float*)&igtbl_r[src[1]]);
		vec_b = load_aligned_float((float*)&igtbl_r[src[2]]);
		src += 4;

		/* use calc'd indices to output RGB values */
		dest[0] = otdata_r[output[0]];
		dest[1] = otdata_g[output[1]];
		dest[2] = otdata_b[output[2]];
		dest += 4;
	}

	/* handle final (maybe only) pixel */

	vec_r = vec_splat(vec_r, 0);
	vec_g = vec_splat(vec_g, 0);
	vec_b = vec_splat(vec_b, 0);

	vec_r = vec_madd(vec_r, mat0, min);
	vec_g = vec_madd(vec_g, mat1, min);
	vec_b = vec_madd(vec_b, mat2, min);

	dest[3] = alpha;

	vec_r = vec_add(vec_r, vec_add(vec_g, vec_b));
	vec_r = vec_max(min, vec_r);
	vec_r = vec_min(max, vec_r);
	result = vec_madd(vec_r, scale, min);

	vec_st(vec_ctu(vec_round(result), 0), 0, (vector unsigned int*)output);

	dest[0] = otdata_r[output[0]];
	dest[1] = otdata_g[output[1]];
	dest[2] = otdata_b[output[2]];
}


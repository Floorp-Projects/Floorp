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

#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "qcmsint.h"

#if defined(_M_IX86) || defined(__i386__) || defined(__x86_64__) || defined(_M_AMD64)
#define X86
#endif

//XXX: could use a bettername
typedef uint16_t uint16_fract_t;

/* value must be a value between 0 and 1 */
//XXX: is the above a good restriction to have?
float lut_interp_linear(double value, uint16_t *table, int length)
{
	int upper, lower;
	value = value * (length - 1);
	upper = ceil(value);
	lower = floor(value);
	//XXX: can we be more performant here?
	value = table[upper]*(1. - (upper - value)) + table[lower]*(upper - value);
	/* scale the value */
	return value * (1./65535.);
}

/* same as above but takes and returns a uint16_t value representing a range from 0..1 */
uint16_t lut_interp_linear16(uint16_t input_value, uint16_t *table, int length)
{
	uint32_t value = (input_value * (length - 1));
	uint32_t upper = (value + 65534) / 65535; /* equivalent to ceil(value/65535) */
	uint32_t lower = value / 65535;           /* equivalent to floor(value/65535) */
	uint32_t interp = value % 65535;

	value = (table[upper]*(interp) + table[lower]*(65535 - interp))/65535;

	return value;
}

void compute_curve_gamma_table_type1(float gamma_table[256], double gamma)
{
	unsigned int i;
	for (i = 0; i < 256; i++) {
		gamma_table[i] = pow(i/255., gamma);
	}
}

void compute_curve_gamma_table_type2(float gamma_table[256], uint16_t *table, int length)
{
	unsigned int i;
	for (i = 0; i < 256; i++) {
		gamma_table[i] = lut_interp_linear(i/255., table, length);
	}
}

void compute_curve_gamma_table_type0(float gamma_table[256])
{
	unsigned int i;
	for (i = 0; i < 256; i++) {
		gamma_table[i] = i;
	}
}

unsigned char clamp_u8(float v)
{
	if (v > 255.)
		return 255;
	else if (v < 0)
		return 0;
	else
		return floor(v+.5);
}

struct vector {
	float v[3];
};

struct matrix {
	float m[3][3];
	bool invalid;
};

struct vector matrix_eval(struct matrix mat, struct vector v)
{
	struct vector result;
	result.v[0] = mat.m[0][0]*v.v[0] + mat.m[0][1]*v.v[1] + mat.m[0][2]*v.v[2];
	result.v[1] = mat.m[1][0]*v.v[0] + mat.m[1][1]*v.v[1] + mat.m[1][2]*v.v[2];
	result.v[2] = mat.m[2][0]*v.v[0] + mat.m[2][1]*v.v[1] + mat.m[2][2]*v.v[2];
	return result;
}

//XXX: should probably pass by reference and we could
//probably reuse this computation in matrix_invert
float matrix_det(struct matrix mat)
{
	float det;
	det = mat.m[0][0]*mat.m[1][1]*mat.m[2][2] +
		mat.m[0][1]*mat.m[1][2]*mat.m[2][0] +
		mat.m[0][2]*mat.m[1][0]*mat.m[2][1] -
		mat.m[0][0]*mat.m[1][2]*mat.m[2][1] -
		mat.m[0][1]*mat.m[1][0]*mat.m[2][2] -
		mat.m[0][2]*mat.m[1][1]*mat.m[2][0];
	return det;
}

/* from pixman and cairo and Mathematics for Game Programmers */
/* lcms uses gauss-jordan elimination with partial pivoting which is
 * less efficient and not as numerically stable. See Mathematics for
 * Game Programmers. */
struct matrix matrix_invert(struct matrix mat)
{
	struct matrix dest_mat;
	int i,j;
	static int a[3] = { 2, 2, 1 };
	static int b[3] = { 1, 0, 0 };

	/* inv  (A) = 1/det (A) * adj (A) */
	float det = matrix_det(mat);

	if (det == 0) {
		dest_mat.invalid = true;
	} else {
		dest_mat.invalid = false;
	}

	det = 1/det;

	for (j = 0; j < 3; j++) {
		for (i = 0; i < 3; i++) {
			double p;
			int ai = a[i];
			int aj = a[j];
			int bi = b[i];
			int bj = b[j];

			p = mat.m[ai][aj] * mat.m[bi][bj] -
				mat.m[ai][bj] * mat.m[bi][aj];
			if (((i + j) & 1) != 0)
				p = -p;

			dest_mat.m[j][i] = det * p;
		}
	}
	return dest_mat;
}

struct matrix matrix_identity(void)
{
	struct matrix i;
	i.m[0][0] = 1;
	i.m[0][1] = 0;
	i.m[0][2] = 0;
	i.m[1][0] = 0;
	i.m[1][1] = 1;
	i.m[1][2] = 0;
	i.m[2][0] = 0;
	i.m[2][1] = 0;
	i.m[2][2] = 1;
	i.invalid = false;
	return i;
}

static struct matrix matrix_invalid(void)
{
	struct matrix inv = matrix_identity();
	inv.invalid = true;
	return inv;
}


/* from pixman */
/* MAT3per... */
struct matrix matrix_multiply(struct matrix a, struct matrix b)
{
	struct matrix result;
	int dx, dy;
	int o;
	for (dy = 0; dy < 3; dy++) {
		for (dx = 0; dx < 3; dx++) {
			double v = 0;
			for (o = 0; o < 3; o++) {
				v += a.m[dy][o] * b.m[o][dx];
			}
			result.m[dy][dx] = v;
		}
	}
	result.invalid = a.invalid || b.invalid;
	return result;
}

float u8Fixed8Number_to_float(uint16_t x)
{
	// 0x0000 = 0.
	// 0x0100 = 1.
	// 0xffff = 255  + 255/256
	return x/256.;
}

float *build_input_gamma_table(struct curveType *TRC)
{
	float *gamma_table = malloc(sizeof(float)*256);
	if (gamma_table) {
		if (TRC->count == 0) {
			compute_curve_gamma_table_type0(gamma_table);
		} else if (TRC->count == 1) {
			compute_curve_gamma_table_type1(gamma_table, u8Fixed8Number_to_float(TRC->data[0]));
		} else {
			compute_curve_gamma_table_type2(gamma_table, TRC->data, TRC->count);
		}
	}
	return gamma_table;
}

struct matrix build_colorant_matrix(qcms_profile *p)
{
	struct matrix result;
	result.m[0][0] = s15Fixed16Number_to_float(p->redColorant.X);
	result.m[0][1] = s15Fixed16Number_to_float(p->greenColorant.X);
	result.m[0][2] = s15Fixed16Number_to_float(p->blueColorant.X);
	result.m[1][0] = s15Fixed16Number_to_float(p->redColorant.Y);
	result.m[1][1] = s15Fixed16Number_to_float(p->greenColorant.Y);
	result.m[1][2] = s15Fixed16Number_to_float(p->blueColorant.Y);
	result.m[2][0] = s15Fixed16Number_to_float(p->redColorant.Z);
	result.m[2][1] = s15Fixed16Number_to_float(p->greenColorant.Z);
	result.m[2][2] = s15Fixed16Number_to_float(p->blueColorant.Z);
	result.invalid = false;
	return result;
}

/* The following code is copied nearly directly from lcms.
 * I think it could be much better. For example, Argyll seems to have better code in
 * icmTable_lookup_bwd and icmTable_setup_bwd. However, for now this is a quick way
 * to a working solution and allows for easy comparing with lcms. */
uint16_fract_t lut_inverse_interp16(uint16_t Value, uint16_t LutTable[], int length)
{
        int l = 1;
        int r = 0x10000;
        int x = 0, res;       // 'int' Give spacing for negative values
        int NumZeroes, NumPoles;
        int cell0, cell1;
        double val2;
        double y0, y1, x0, x1;
        double a, b, f;

        // July/27 2001 - Expanded to handle degenerated curves with an arbitrary
        // number of elements containing 0 at the begining of the table (Zeroes)
        // and another arbitrary number of poles (FFFFh) at the end.
        // First the zero and pole extents are computed, then value is compared.

        NumZeroes = 0;
        while (LutTable[NumZeroes] == 0 && NumZeroes < length-1)
                        NumZeroes++;

        // There are no zeros at the beginning and we are trying to find a zero, so
        // return anything. It seems zero would be the less destructive choice
	/* I'm not sure that this makes sense, but oh well... */
        if (NumZeroes == 0 && Value == 0)
            return 0;

        NumPoles = 0;
        while (LutTable[length-1- NumPoles] == 0xFFFF && NumPoles < length-1)
                        NumPoles++;

        // Does the curve belong to this case?
        if (NumZeroes > 1 || NumPoles > 1)
        {               
                int a, b;

                // Identify if value fall downto 0 or FFFF zone             
                if (Value == 0) return 0;
               // if (Value == 0xFFFF) return 0xFFFF;

                // else restrict to valid zone

                a = ((NumZeroes-1) * 0xFFFF) / (length-1);               
                b = ((length-1 - NumPoles) * 0xFFFF) / (length-1);
                                                                
                l = a - 1;
                r = b + 1;
        }


        // Seems not a degenerated case... apply binary search

        while (r > l) {

                x = (l + r) / 2;

		res = (int) lut_interp_linear16((uint16_fract_t) (x-1), LutTable, length);

                if (res == Value) {

                    // Found exact match. 
                    
                    return (uint16_fract_t) (x - 1);
                }

                if (res > Value) r = x - 1;
                else l = x + 1;
        }

        // Not found, should we interpolate?

                
        // Get surrounding nodes
        
        val2 = (length-1) * ((double) (x - 1) / 65535.0);

        cell0 = (int) floor(val2);
        cell1 = (int) ceil(val2);
           
        if (cell0 == cell1) return (uint16_fract_t) x;

        y0 = LutTable[cell0] ;
        x0 = (65535.0 * cell0) / (length-1); 

        y1 = LutTable[cell1] ;
        x1 = (65535.0 * cell1) / (length-1);

        a = (y1 - y0) / (x1 - x0);
        b = y0 - a * x0;

        if (fabs(a) < 0.01) return (uint16_fract_t) x;

        f = ((Value - b) / a);

        if (f < 0.0) return (uint16_fract_t) 0;
        if (f >= 65535.0) return (uint16_fract_t) 0xFFFF;

        return (uint16_fract_t) floor(f + 0.5);                        
        
}

// Build a White point, primary chromas transfer matrix from RGB to CIE XYZ
// This is just an approximation, I am not handling all the non-linear
// aspects of the RGB to XYZ process, and assumming that the gamma correction
// has transitive property in the tranformation chain.
//
// the alghoritm:
//
//            - First I build the absolute conversion matrix using
//              primaries in XYZ. This matrix is next inverted
//            - Then I eval the source white point across this matrix
//              obtaining the coeficients of the transformation
//            - Then, I apply these coeficients to the original matrix
static struct matrix build_RGB_to_XYZ_transfer_matrix(qcms_CIE_xyY white, qcms_CIE_xyYTRIPLE primrs)
{
	struct matrix primaries;
	struct matrix primaries_invert;
	struct matrix result;
	struct vector white_point;
	struct vector coefs;

	double xn, yn;
	double xr, yr;
	double xg, yg;
	double xb, yb;

	xn = white.x;
	yn = white.y;

	if (yn == 0.0)
		return matrix_invalid();

	xr = primrs.red.x;
	yr = primrs.red.y;
	xg = primrs.green.x;
	yg = primrs.green.y;
	xb = primrs.blue.x;
	yb = primrs.blue.y;

	primaries.m[0][0] = xr;
	primaries.m[0][1] = xg;
	primaries.m[0][2] = xb;

	primaries.m[1][0] = yr;
	primaries.m[1][1] = yg;
	primaries.m[1][2] = yb;

	primaries.m[2][0] = 1 - xr - yr;
	primaries.m[2][1] = 1 - xg - yg;
	primaries.m[2][2] = 1 - xb - yb;
	primaries.invalid = false;

	white_point.v[0] = xn/yn;
	white_point.v[1] = 1.;
	white_point.v[2] = (1.0-xn-yn)/yn;

	primaries_invert = matrix_invert(primaries);

	coefs = matrix_eval(primaries_invert, white_point);

	result.m[0][0] = coefs.v[0]*xr;
	result.m[0][1] = coefs.v[1]*xg;
	result.m[0][2] = coefs.v[2]*xb;

	result.m[1][0] = coefs.v[0]*yr;
	result.m[1][1] = coefs.v[1]*yg;
	result.m[1][2] = coefs.v[2]*yb;

	result.m[2][0] = coefs.v[0]*(1.-xr-yr);
	result.m[2][1] = coefs.v[1]*(1.-xg-yg);
	result.m[2][2] = coefs.v[2]*(1.-xb-yb);
	result.invalid = primaries_invert.invalid;

	return result;
}

struct CIE_XYZ {
	double X;
	double Y;
	double Z;
};

/* CIE Illuminant D50 */
static const struct CIE_XYZ D50_XYZ = {
	0.9642,
	1.0000,
	0.8249
};

/* from lcms: xyY2XYZ()
 * corresponds to argyll: icmYxy2XYZ() */
static struct CIE_XYZ xyY2XYZ(qcms_CIE_xyY source)
{
	struct CIE_XYZ dest;
	dest.X = (source.x / source.y) * source.Y;
	dest.Y = source.Y;
	dest.Z = ((1 - source.x - source.y) / source.y) * source.Y;
	return dest;
}

/* from lcms: ComputeChromaticAdaption */
// Compute chromatic adaption matrix using chad as cone matrix
static struct matrix
compute_chromatic_adaption(struct CIE_XYZ source_white_point,
                           struct CIE_XYZ dest_white_point,
                           struct matrix chad)
{
	struct matrix chad_inv;
	struct vector cone_source_XYZ, cone_source_rgb;
	struct vector cone_dest_XYZ, cone_dest_rgb;
	struct matrix cone, tmp;

	tmp = chad;
	chad_inv = matrix_invert(tmp);

	cone_source_XYZ.v[0] = source_white_point.X;
	cone_source_XYZ.v[1] = source_white_point.Y;
	cone_source_XYZ.v[2] = source_white_point.Z;

	cone_dest_XYZ.v[0] = dest_white_point.X;
	cone_dest_XYZ.v[1] = dest_white_point.Y;
	cone_dest_XYZ.v[2] = dest_white_point.Z;

	cone_source_rgb = matrix_eval(chad, cone_source_XYZ);
	cone_dest_rgb   = matrix_eval(chad, cone_dest_XYZ);

	cone.m[0][0] = cone_dest_rgb.v[0]/cone_source_rgb.v[0];
	cone.m[0][1] = 0;
	cone.m[0][2] = 0;
	cone.m[1][0] = 0;
	cone.m[1][1] = cone_dest_rgb.v[1]/cone_source_rgb.v[1];
	cone.m[1][2] = 0;
	cone.m[2][0] = 0;
	cone.m[2][1] = 0;
	cone.m[2][2] = cone_dest_rgb.v[2]/cone_source_rgb.v[2];
	cone.invalid = false;

	// Normalize
	return matrix_multiply(chad_inv, matrix_multiply(cone, chad));
}

/* from lcms: cmsAdaptionMatrix */
// Returns the final chrmatic adaptation from illuminant FromIll to Illuminant ToIll
// Bradford is assumed
static struct matrix
adaption_matrix(struct CIE_XYZ source_illumination, struct CIE_XYZ target_illumination)
{
	struct matrix lam_rigg = {{ // Bradford matrix
	                         {  0.8951,  0.2664, -0.1614 },
	                         { -0.7502,  1.7135,  0.0367 },
	                         {  0.0389, -0.0685,  1.0296 }
	                         }};
	return compute_chromatic_adaption(source_illumination, target_illumination, lam_rigg);
}

/* from lcms: cmsAdaptMatrixToD50 */
static struct matrix adapt_matrix_to_D50(struct matrix r, qcms_CIE_xyY source_white_pt)
{
	struct CIE_XYZ Dn;
	struct matrix Bradford;

	if (source_white_pt.y == 0.0)
		return matrix_invalid();

	Dn = xyY2XYZ(source_white_pt);

	Bradford = adaption_matrix(Dn, D50_XYZ);
	return matrix_multiply(Bradford, r);
}

void set_rgb_colorants(qcms_profile *profile, qcms_CIE_xyY white_point, qcms_CIE_xyYTRIPLE primaries)
{
	struct matrix colorants;
	colorants = build_RGB_to_XYZ_transfer_matrix(white_point, primaries);
	colorants = adapt_matrix_to_D50(colorants, white_point);

	/* note: there's a transpose type of operation going on here */
	profile->redColorant.X = double_to_s15Fixed16Number(colorants.m[0][0]);
	profile->redColorant.Y = double_to_s15Fixed16Number(colorants.m[1][0]);
	profile->redColorant.Z = double_to_s15Fixed16Number(colorants.m[2][0]);

	profile->greenColorant.X = double_to_s15Fixed16Number(colorants.m[0][1]);
	profile->greenColorant.Y = double_to_s15Fixed16Number(colorants.m[1][1]);
	profile->greenColorant.Z = double_to_s15Fixed16Number(colorants.m[2][1]);

	profile->blueColorant.X = double_to_s15Fixed16Number(colorants.m[0][2]);
	profile->blueColorant.Y = double_to_s15Fixed16Number(colorants.m[1][2]);
	profile->blueColorant.Z = double_to_s15Fixed16Number(colorants.m[2][2]);
}

static uint16_t *invert_lut(uint16_t *table, int length)
{
	int i;
	/* for now we invert the lut by creating a lut of the same size
	 * and attempting to lookup a value for each entry using lut_inverse_interp16 */
	uint16_t *output = malloc(sizeof(uint16_t)*length);
	if (!output)
		return NULL;

	for (i = 0; i < length; i++) {
		double x = ((double) i * 65535.) / (double) (length - 1);
		uint16_fract_t input = floor(x + .5);
		output[i] = lut_inverse_interp16(input, table, length);
	}
	return output;
}

static uint16_t *build_linear_table(int length)
{
	int i;
	uint16_t *output = malloc(sizeof(uint16_t)*length);
	if (!output)
		return NULL;

	for (i = 0; i < length; i++) {
		double x = ((double) i * 65535.) / (double) (length - 1);
		uint16_fract_t input = floor(x + .5);
		output[i] = input;
	}
	return output;
}

static uint16_t *build_pow_table(float gamma, int length)
{
	int i;
	uint16_t *output = malloc(sizeof(uint16_t)*length);
	if (!output)
		return NULL;

	for (i = 0; i < length; i++) {
		uint16_fract_t result;
		double x = ((double) i) / (double) (length - 1);
		x = pow(x, gamma);
                //XXX turn this conversion into a function
		result = floor(x*65535. + .5);
		output[i] = result;
	}
	return output;
}

static float clamp_float(float a)
{
	if (a > 1.)
		return 1.;
	else if (a < 0)
		return 0;
	else
		return a;
}

#if 0
static void qcms_transform_data_rgb_out_pow(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
	for (i=0; i<length; i++) {
		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;

		float linear_r = transform->input_gamma_table_r[device_r];
		float linear_g = transform->input_gamma_table_g[device_g];
		float linear_b = transform->input_gamma_table_b[device_b];

		float out_linear_r = mat[0][0]*linear_r + mat[1][0]*linear_g + mat[2][0]*linear_b;
		float out_linear_g = mat[0][1]*linear_r + mat[1][1]*linear_g + mat[2][1]*linear_b;
		float out_linear_b = mat[0][2]*linear_r + mat[1][2]*linear_g + mat[2][2]*linear_b;

		float out_device_r = pow(out_linear_r, transform->out_gamma_r);
		float out_device_g = pow(out_linear_g, transform->out_gamma_g);
		float out_device_b = pow(out_linear_b, transform->out_gamma_b);

		*dest++ = clamp_u8(255*out_device_r);
		*dest++ = clamp_u8(255*out_device_g);
		*dest++ = clamp_u8(255*out_device_b);
	}
}
#endif

static void qcms_transform_data_gray_out_lut(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	for (i = 0; i < length; i++) {
		float out_device_r, out_device_g, out_device_b;
		unsigned char device = *src++;

		float linear = transform->input_gamma_table_gray[device];

                out_device_r = lut_interp_linear(linear, transform->output_gamma_lut_r, transform->output_gamma_lut_r_length);
		out_device_g = lut_interp_linear(linear, transform->output_gamma_lut_g, transform->output_gamma_lut_g_length);
		out_device_b = lut_interp_linear(linear, transform->output_gamma_lut_b, transform->output_gamma_lut_b_length);

		*dest++ = clamp_u8(out_device_r*255);
		*dest++ = clamp_u8(out_device_g*255);
		*dest++ = clamp_u8(out_device_b*255);
	}
}

static void qcms_transform_data_graya_out_lut(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	for (i = 0; i < length; i++) {
		float out_device_r, out_device_g, out_device_b;
		unsigned char device = *src++;
		unsigned char alpha = *src++;

		float linear = transform->input_gamma_table_gray[device];

                out_device_r = lut_interp_linear(linear, transform->output_gamma_lut_r, transform->output_gamma_lut_r_length);
		out_device_g = lut_interp_linear(linear, transform->output_gamma_lut_g, transform->output_gamma_lut_g_length);
		out_device_b = lut_interp_linear(linear, transform->output_gamma_lut_b, transform->output_gamma_lut_b_length);

		*dest++ = clamp_u8(out_device_r*255);
		*dest++ = clamp_u8(out_device_g*255);
		*dest++ = clamp_u8(out_device_b*255);
		*dest++ = alpha;
	}
}


static void qcms_transform_data_gray_out_precache(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	for (i = 0; i < length; i++) {
		unsigned char device = *src++;
		uint16_t gray;

		float linear = transform->input_gamma_table_gray[device];

		/* we could round here... */
		gray = linear * 65535.;

		*dest++ = transform->output_table_r->data[gray];
		*dest++ = transform->output_table_g->data[gray];
		*dest++ = transform->output_table_b->data[gray];
	}
}

static void qcms_transform_data_graya_out_precache(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	for (i = 0; i < length; i++) {
		unsigned char device = *src++;
		unsigned char alpha = *src++;
		uint16_t gray;

		float linear = transform->input_gamma_table_gray[device];

		/* we could round here... */
		gray = linear * 65535.;

		*dest++ = transform->output_table_r->data[gray];
		*dest++ = transform->output_table_g->data[gray];
		*dest++ = transform->output_table_b->data[gray];
		*dest++ = alpha;
	}
}

static const ALIGN float floatScale = 65536.0f;
static const ALIGN float * const floatScaleAddr = &floatScale; // Win32 ASM doesn't know how to take addressOf inline

static const ALIGN float clampMaxValue = ((float) (65536 - 1)) / 65536.0f;

#ifdef X86
#if 0
#include <emmintrin.h>
void qcms_transform_data_rgb_out_lut_sse_intrin(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
        char input_back[32];
	/* Ensure we have a buffer that's 16 byte aligned regardless of the original
	 * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
	 * because they don't work on stack variables. gcc 4.4 does do the right thing 
	 * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
        float *input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
        /* share input and output locations to save having to keep the
         * locations in separate registers */
        uint32_t* output = (uint32_t*)input;
	for (i=0; i<length; i++) {
		const float *clampMax = &clampMaxValue;

		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;

		__m128 xmm1 = _mm_load_ps(mat[0]);
		__m128 xmm2 = _mm_load_ps(mat[1]);
		__m128 xmm3 = _mm_load_ps(mat[2]);

		__m128 vec_r = _mm_load_ss(&transform->input_gamma_table_r[device_r]);
		vec_r = _mm_shuffle_ps(vec_r, vec_r, 0);
		__m128 vec_g = _mm_load_ss(&transform->input_gamma_table_r[device_g]);
		vec_g = _mm_shuffle_ps(vec_g, vec_g, 0);
		__m128 vec_b = _mm_load_ss(&transform->input_gamma_table_r[device_b]);
		vec_b = _mm_shuffle_ps(vec_b, vec_b, 0);

		vec_r = _mm_mul_ps(vec_r, xmm1);
		vec_g = _mm_mul_ps(vec_g, xmm2);
		vec_b = _mm_mul_ps(vec_b, xmm3);

		vec_r = _mm_add_ps(vec_r, _mm_add_ps(vec_g, vec_b));

		__m128 max = _mm_load_ss(&clampMax);
		max = _mm_shuffle_ps(max, max, 0);
		__m128 min = _mm_setzero_ps();

		vec_r = _mm_max_ps(min, vec_r);
		vec_r = _mm_min_ps(max, vec_r);

		__m128 scale = _mm_load_ss(&floatScale);
		scale = _mm_shuffle_ps(scale, scale, 0);
		__m128 result = _mm_mul_ps(vec_r, scale);

		__m128i out = _mm_cvtps_epi32(result);
		_mm_store_si128((__m128i*)input, out);

		*dest++ = transform->output_table_r->data[output[0]];
		*dest++ = transform->output_table_g->data[output[1]];
		*dest++ = transform->output_table_b->data[output[2]];
	}
}
#endif

#if defined(_MSC_VER) && defined(_M_AMD64)
#include <emmintrin.h>
#endif

static void qcms_transform_data_rgb_out_lut_sse(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
        char input_back[32];
	/* Ensure we have a buffer that's 16 byte aligned regardless of the original
	 * stack alignment. We can't use __attribute__((aligned(16))) or __declspec(align(32))
	 * because they don't work on stack variables. gcc 4.4 does do the right thing 
	 * on x86 but that's too new for us right now. For more info: gcc bug #16660 */
        float *input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
        /* share input and output locations to save having to keep the
         * locations in separate registers */
        uint32_t* output = (uint32_t*)input;
	for (i = 0; i < length; i++) {
		const float *clampMax = &clampMaxValue;

		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;

		input[0] = transform->input_gamma_table_r[device_r];
		input[1] = transform->input_gamma_table_g[device_g];
		input[2] = transform->input_gamma_table_b[device_b];

#ifdef __GNUC__
		__asm(
                      "movaps (%0), %%xmm1;\n\t"          // Move the first matrix column to xmm1
                      "movaps 16(%0), %%xmm2;\n\t"        // Move the second matrix column to xmm2
                      "movaps 32(%0), %%xmm3;\n\t"        // move the third matrix column to xmm3
                      "movaps (%3), %%xmm0;\n\t"        // Move the vector to xmm0

                                                          // Note - We have to copy and then shuffle because of the weird
                                                          // semantics of shufps
                                                          //
                      "movaps %%xmm0, %%xmm4;\n\t"        // Copy the vector to xmm4
                      "shufps $0, %%xmm4, %%xmm4;\n\t"    // Shuffle to repeat the first vector element repeated 4 times
                      "mulps %%xmm4, %%xmm1;\n\t"         // Multiply the first vector element by the first matrix column
                      "movaps %%xmm0, %%xmm5; \n\t"       // Copy the vector to xmm5
                      "shufps $0x55, %%xmm5, %%xmm5;\n\t" // Shuffle to repeat the second vector element repeated 4 times
                      "mulps %%xmm5, %%xmm2;\n\t"         // Multiply the second vector element by the seccond matrix column 
                      "movaps %%xmm0, %%xmm6;\n\t"        // Copy the vector to xmm6
                      "shufps $0xAA, %%xmm6, %%xmm6;\n\t" // Shuffle to repeat the third vector element repeated 4 times
                      "mulps %%xmm6, %%xmm3;\n\t"         // Multiply the third vector element by the third matrix column

                      "addps %%xmm3, %%xmm2;\n\t"         // Sum (second + third) columns
                      "addps %%xmm2, %%xmm1;\n\t"         // Sum ((second + third) + first) columns

                      "movss (%1), %%xmm7;\n\t"        // load the floating point representation of 65535/65536 
                      "shufps $0, %%xmm7, %%xmm7;\n\t" // move it into all of the four slots
                      "minps %%xmm7, %%xmm1;\n\t"      // clamp the vector to 1.0 max
                      "xorps %%xmm6, %%xmm6;\n\t"       // get us cleared bitpatern, which is 0.0f
                      "maxps %%xmm6, %%xmm1;\n\t"      // clamp the vector to 0.0 min
                      "movss (%2), %%xmm5;\n\t"        // load the floating point scale factor
                      "shufps $0, %%xmm5, %%xmm5;\n\t" // put it in all four slots
                      "mulps %%xmm5, %%xmm1;\n\t"      // multiply by the scale factor
                      "cvtps2dq %%xmm1, %%xmm1;\n\t"   // convert to integers
                      "movdqa %%xmm1, (%3);\n\t"       // store

                      : 
                      : "r" (mat), "r" (clampMax), "r" (&floatScale), "r" (input)
                      : "memory"
/* older versions of gcc don't know about these registers so only include them as constraints
   if gcc knows about them */
#ifdef __SSE2__
                        , "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                      );
#elif defined(_MSC_VER) && defined(_M_IX86)
                __asm {
                      mov      eax, mat
                      mov      ecx, clampMax
                      mov      edx, floatScaleAddr
		      mov      ebx, input

                      movaps   xmm1, [eax]
                      movaps   xmm2, [eax + 16]
                      movaps   xmm3, [eax + 32]
                      movaps   xmm0, [ebx]

                      movaps   xmm4, xmm0
                      shufps   xmm4, xmm4, 0
                      mulps    xmm1, xmm4
                      movaps   xmm5, xmm0
                      shufps   xmm5, xmm5, 0x55
                      mulps    xmm2, xmm5
                      movaps   xmm6, xmm0
                      shufps   xmm6, xmm6, 0xAA
                      mulps    xmm3, xmm6

                      addps    xmm2, xmm3
                      addps    xmm1, xmm2

                      movss    xmm7, [ecx]
                      shufps   xmm7, xmm7, 0
                      minps    xmm1, xmm7
                      xorps    xmm6, xmm6
                      maxps    xmm1, xmm6
                      movss    xmm5, [edx]
                      shufps   xmm5, xmm5, 0
                      mulps    xmm1, xmm5
                      cvtps2dq xmm1, xmm1
                      movdqa   [ebx], xmm1
                }
#elif defined(_MSC_VER) && defined(_M_AMD64)
                {
                        __m128 xmm0, xmm1, xmm2, xmm3, xmm5, xmm6, xmm7;

                        xmm1 = _mm_load_ps((__m128*)mat);
                        xmm2 = _mm_load_ps(((__m128*)mat) + 1);
                        xmm3 = _mm_load_ps(((__m128*)mat) + 2);
                        xmm0 = _mm_load_ps((__m128*)input);

                        xmm1 = _mm_mul_ps(xmm1, _mm_shuffle_ps(xmm0, xmm0, _MM_SHUFFLE(0,0,0,0)));
                        xmm2 = _mm_mul_ps(xmm2, _mm_shuffle_ps(xmm0, xmm0, _MM_SHUFFLE(1,1,1,1)));
                        xmm3 = _mm_mul_ps(xmm3, _mm_shuffle_ps(xmm0, xmm0, _MM_SHUFFLE(2,2,2,2)));

                        xmm1 = _mm_add_ps(xmm1, _mm_add_ps(xmm2, xmm3));

                        xmm7 = _mm_load_ss(clampMax);
                        xmm7 = _mm_shuffle_ps(xmm7, xmm7, _MM_SHUFFLE(0,0,0,0));
                        xmm1 = _mm_min_ps(xmm1, xmm7);
                        xmm6 = _mm_xor_ps(xmm6, xmm6);
                        xmm1 = _mm_max_ps(xmm1, xmm6);
                        xmm5 = _mm_load_ss(&floatScale);
                        xmm5 = _mm_shuffle_ps(xmm5, xmm5, _MM_SHUFFLE(0,0,0,0));
                        xmm1 = _mm_mul_ps(xmm1, xmm5);
                        _mm_store_si128((__m128i*)input, _mm_cvtps_epi32(xmm1));
                }
#else
#error "Unknown platform"
#endif

		*dest++ = transform->output_table_r->data[output[0]];
		*dest++ = transform->output_table_g->data[output[1]];
		*dest++ = transform->output_table_b->data[output[2]];
	}
}

static void qcms_transform_data_rgba_out_lut_sse(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
        char input_back[32];
	/* align input on 16 byte boundary */
        float *input = (float*)(((uintptr_t)&input_back[16]) & ~0xf);
        /* share input and output locations to save having to keep the
         * locations in separate registers */
        uint32_t* output = (uint32_t*)input;
	for (i = 0; i < length; i++) {
		const float *clampMax = &clampMaxValue;

		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;
		unsigned char alpha = *src++;

		input[0] = transform->input_gamma_table_r[device_r];
		input[1] = transform->input_gamma_table_g[device_g];
		input[2] = transform->input_gamma_table_b[device_b];

#ifdef __GNUC__
		__asm(
                      "movaps (%0), %%xmm1;\n\t"          // Move the first matrix column to xmm1
                      "movaps 16(%0), %%xmm2;\n\t"        // Move the second matrix column to xmm2
                      "movaps 32(%0), %%xmm3;\n\t"        // move the third matrix column to xmm3
                      "movaps (%3), %%xmm0;\n\t"        // Move the vector to xmm0

                                                          // Note - We have to copy and then shuffle because of the weird
                                                          // semantics of shufps
                                                          //
                      "movaps %%xmm0, %%xmm4;\n\t"        // Copy the vector to xmm4
                      "shufps $0, %%xmm4, %%xmm4;\n\t"    // Shuffle to repeat the first vector element repeated 4 times
                      "mulps %%xmm4, %%xmm1;\n\t"         // Multiply the first vector element by the first matrix column
                      "movaps %%xmm0, %%xmm5; \n\t"       // Copy the vector to xmm5
                      "shufps $0x55, %%xmm5, %%xmm5;\n\t" // Shuffle to repeat the second vector element repeated 4 times
                      "mulps %%xmm5, %%xmm2;\n\t"         // Multiply the second vector element by the seccond matrix column 
                      "movaps %%xmm0, %%xmm6;\n\t"        // Copy the vector to xmm6
                      "shufps $0xAA, %%xmm6, %%xmm6;\n\t" // Shuffle to repeat the third vector element repeated 4 times
                      "mulps %%xmm6, %%xmm3;\n\t"         // Multiply the third vector element by the third matrix column

                      "addps %%xmm3, %%xmm2;\n\t"         // Sum (second + third) columns
                      "addps %%xmm2, %%xmm1;\n\t"         // Sum ((second + third) + first) columns

                      "movss (%1), %%xmm7;\n\t"        // load the floating point representation of 65535/65536 
                      "shufps $0, %%xmm7, %%xmm7;\n\t" // move it into all of the four slots
                      "minps %%xmm7, %%xmm1;\n\t"      // clamp the vector to 1.0 max
                      "xorps %%xmm6, %%xmm6;\n\t"       // get us cleared bitpatern, which is 0.0f
                      "maxps %%xmm6, %%xmm1;\n\t"      // clamp the vector to 0.0 min
                      "movss (%2), %%xmm5;\n\t"        // load the floating point scale factor
                      "shufps $0, %%xmm5, %%xmm5;\n\t" // put it in all four slots
                      "mulps %%xmm5, %%xmm1;\n\t"      // multiply by the scale factor
                      "cvtps2dq %%xmm1, %%xmm1;\n\t"   // convert to integers
                      "movdqa %%xmm1, (%3);\n\t"       // store

                      : 
                      : "r" (mat), "r" (clampMax), "r" (&floatScale), "r" (input)
                      : "memory"
/* older versions of gcc don't know about these registers so only include them as constraints
   if gcc knows about them */
#ifdef __SSE2__
                        , "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7"
#endif
                      );
#elif defined(_MSC_VER) && defined(_M_IX86)
                __asm {
                      mov      eax, mat
                      mov      ecx, clampMax
                      mov      edx, floatScaleAddr
		      mov      ebx, input

                      movaps   xmm1, [eax]
                      movaps   xmm2, [eax + 16]
                      movaps   xmm3, [eax + 32]
                      movaps   xmm0, [ebx]

                      movaps   xmm4, xmm0
                      shufps   xmm4, xmm4, 0
                      mulps    xmm1, xmm4
                      movaps   xmm5, xmm0
                      shufps   xmm5, xmm5, 0x55
                      mulps    xmm2, xmm5
                      movaps   xmm6, xmm0
                      shufps   xmm6, xmm6, 0xAA
                      mulps    xmm3, xmm6

                      addps    xmm2, xmm3
                      addps    xmm1, xmm2

                      movss    xmm7, [ecx]
                      shufps   xmm7, xmm7, 0
                      minps    xmm1, xmm7
                      xorps    xmm6, xmm6
                      maxps    xmm1, xmm6
                      movss    xmm5, [edx]
                      shufps   xmm5, xmm5, 0
                      mulps    xmm1, xmm5
                      cvtps2dq xmm1, xmm1
                      movdqa   [ebx], xmm1
                }
#elif defined(_MSC_VER) && defined(_M_AMD64)
                {
                        __m128 xmm0, xmm1, xmm2, xmm3, xmm5, xmm6, xmm7;

                        xmm1 = _mm_load_ps((__m128*)mat);
                        xmm2 = _mm_load_ps(((__m128*)mat) + 1);
                        xmm3 = _mm_load_ps(((__m128*)mat) + 2);
                        xmm0 = _mm_load_ps((__m128*)input);

                        xmm1 = _mm_mul_ps(xmm1, _mm_shuffle_ps(xmm0, xmm0, _MM_SHUFFLE(0,0,0,0)));
                        xmm2 = _mm_mul_ps(xmm2, _mm_shuffle_ps(xmm0, xmm0, _MM_SHUFFLE(1,1,1,1)));
                        xmm3 = _mm_mul_ps(xmm3, _mm_shuffle_ps(xmm0, xmm0, _MM_SHUFFLE(2,2,2,2)));

                        xmm1 = _mm_add_ps(xmm1, _mm_add_ps(xmm2, xmm3));

                        xmm7 = _mm_load_ss(clampMax);
                        xmm7 = _mm_shuffle_ps(xmm7, xmm7, _MM_SHUFFLE(0,0,0,0));
                        xmm1 = _mm_min_ps(xmm1, xmm7);
                        xmm6 = _mm_xor_ps(xmm6, xmm6);
                        xmm1 = _mm_max_ps(xmm1, xmm6);
                        xmm5 = _mm_load_ss(&floatScale);
                        xmm5 = _mm_shuffle_ps(xmm5, xmm5, _MM_SHUFFLE(0,0,0,0));
                        xmm1 = _mm_mul_ps(xmm1, xmm5);
                        _mm_store_si128((__m128i*)input, _mm_cvtps_epi32(xmm1));
                }
#else
#error "Unknown platform"
#endif

		*dest++ = transform->output_table_r->data[output[0]];
		*dest++ = transform->output_table_g->data[output[1]];
		*dest++ = transform->output_table_b->data[output[2]];
		*dest++ = alpha;
	}
}
#endif

static void qcms_transform_data_rgb_out_lut_precache(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
	for (i = 0; i < length; i++) {
		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;
		uint16_t r, g, b;

		float linear_r = transform->input_gamma_table_r[device_r];
		float linear_g = transform->input_gamma_table_g[device_g];
		float linear_b = transform->input_gamma_table_b[device_b];

		float out_linear_r = mat[0][0]*linear_r + mat[1][0]*linear_g + mat[2][0]*linear_b;
		float out_linear_g = mat[0][1]*linear_r + mat[1][1]*linear_g + mat[2][1]*linear_b;
		float out_linear_b = mat[0][2]*linear_r + mat[1][2]*linear_g + mat[2][2]*linear_b;

		out_linear_r = clamp_float(out_linear_r);
		out_linear_g = clamp_float(out_linear_g);
		out_linear_b = clamp_float(out_linear_b);

		/* we could round here... */
		r = out_linear_r * 65535.;
		g = out_linear_g * 65535.;
		b = out_linear_b * 65535.;

		*dest++ = transform->output_table_r->data[r];
		*dest++ = transform->output_table_g->data[g];
		*dest++ = transform->output_table_b->data[b];
	}
}

static void qcms_transform_data_rgba_out_lut_precache(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
	for (i = 0; i < length; i++) {
		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;
		unsigned char alpha = *src++;
		uint16_t r, g, b;

		float linear_r = transform->input_gamma_table_r[device_r];
		float linear_g = transform->input_gamma_table_g[device_g];
		float linear_b = transform->input_gamma_table_b[device_b];

		float out_linear_r = mat[0][0]*linear_r + mat[1][0]*linear_g + mat[2][0]*linear_b;
		float out_linear_g = mat[0][1]*linear_r + mat[1][1]*linear_g + mat[2][1]*linear_b;
		float out_linear_b = mat[0][2]*linear_r + mat[1][2]*linear_g + mat[2][2]*linear_b;

		out_linear_r = clamp_float(out_linear_r);
		out_linear_g = clamp_float(out_linear_g);
		out_linear_b = clamp_float(out_linear_b);

		/* we could round here... */
		r = out_linear_r * 65535.;
		g = out_linear_g * 65535.;
		b = out_linear_b * 65535.;

		*dest++ = transform->output_table_r->data[r];
		*dest++ = transform->output_table_g->data[g];
		*dest++ = transform->output_table_b->data[b];
		*dest++ = alpha;
	}
}

static void qcms_transform_data_rgb_out_lut(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
	for (i = 0; i < length; i++) {
		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;
		float out_device_r, out_device_g, out_device_b;

		float linear_r = transform->input_gamma_table_r[device_r];
		float linear_g = transform->input_gamma_table_g[device_g];
		float linear_b = transform->input_gamma_table_b[device_b];

		float out_linear_r = mat[0][0]*linear_r + mat[1][0]*linear_g + mat[2][0]*linear_b;
		float out_linear_g = mat[0][1]*linear_r + mat[1][1]*linear_g + mat[2][1]*linear_b;
		float out_linear_b = mat[0][2]*linear_r + mat[1][2]*linear_g + mat[2][2]*linear_b;

		out_linear_r = clamp_float(out_linear_r);
		out_linear_g = clamp_float(out_linear_g);
		out_linear_b = clamp_float(out_linear_b);

		out_device_r = lut_interp_linear(out_linear_r, transform->output_gamma_lut_r, transform->output_gamma_lut_r_length);
		out_device_g = lut_interp_linear(out_linear_g, transform->output_gamma_lut_g, transform->output_gamma_lut_g_length);
		out_device_b = lut_interp_linear(out_linear_b, transform->output_gamma_lut_b, transform->output_gamma_lut_b_length);

		*dest++ = clamp_u8(out_device_r*255);
		*dest++ = clamp_u8(out_device_g*255);
		*dest++ = clamp_u8(out_device_b*255);
	}
}

static void qcms_transform_data_rgba_out_lut(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
	for (i = 0; i < length; i++) {
		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;
		unsigned char alpha = *src++;
		float out_device_r, out_device_g, out_device_b;

		float linear_r = transform->input_gamma_table_r[device_r];
		float linear_g = transform->input_gamma_table_g[device_g];
		float linear_b = transform->input_gamma_table_b[device_b];

		float out_linear_r = mat[0][0]*linear_r + mat[1][0]*linear_g + mat[2][0]*linear_b;
		float out_linear_g = mat[0][1]*linear_r + mat[1][1]*linear_g + mat[2][1]*linear_b;
		float out_linear_b = mat[0][2]*linear_r + mat[1][2]*linear_g + mat[2][2]*linear_b;

		out_linear_r = clamp_float(out_linear_r);
		out_linear_g = clamp_float(out_linear_g);
		out_linear_b = clamp_float(out_linear_b);

		out_device_r = lut_interp_linear(out_linear_r, transform->output_gamma_lut_r, transform->output_gamma_lut_r_length);
		out_device_g = lut_interp_linear(out_linear_g, transform->output_gamma_lut_g, transform->output_gamma_lut_g_length);
		out_device_b = lut_interp_linear(out_linear_b, transform->output_gamma_lut_b, transform->output_gamma_lut_b_length);

		*dest++ = clamp_u8(out_device_r*255);
		*dest++ = clamp_u8(out_device_g*255);
		*dest++ = clamp_u8(out_device_b*255);
		*dest++ = alpha;
	}
}

#if 0
static void qcms_transform_data_rgb_out_linear(qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length)
{
	int i;
	float (*mat)[4] = transform->matrix;
	for (i = 0; i < length; i++) {
		unsigned char device_r = *src++;
		unsigned char device_g = *src++;
		unsigned char device_b = *src++;

		float linear_r = transform->input_gamma_table_r[device_r];
		float linear_g = transform->input_gamma_table_g[device_g];
		float linear_b = transform->input_gamma_table_b[device_b];

		float out_linear_r = mat[0][0]*linear_r + mat[1][0]*linear_g + mat[2][0]*linear_b;
		float out_linear_g = mat[0][1]*linear_r + mat[1][1]*linear_g + mat[2][1]*linear_b;
		float out_linear_b = mat[0][2]*linear_r + mat[1][2]*linear_g + mat[2][2]*linear_b;

		*dest++ = clamp_u8(out_linear_r*255);
		*dest++ = clamp_u8(out_linear_g*255);
		*dest++ = clamp_u8(out_linear_b*255);
	}
}
#endif

static struct precache_output *precache_reference(struct precache_output *p)
{
	p->ref_count++;
	return p;
}

static struct precache_output *precache_create()
{
	struct precache_output *p = malloc(sizeof(struct precache_output));
	if (p)
		p->ref_count = 1;
	return p;
}

void precache_release(struct precache_output *p)
{
	if (--p->ref_count == 0) {
		free(p);
	}
}

#ifdef HAS_POSIX_MEMALIGN
static qcms_transform *transform_alloc(void)
{
	qcms_transform *t;
	if (!posix_memalign(&t, 16, sizeof(*t))) {
		return t;
	} else {
		return NULL;
	}
}
static void transform_free(qcms_transform *t)
{
	free(t);
}
#else
static qcms_transform *transform_alloc(void)
{
	/* transform needs to be aligned on a 16byte boundrary */
	char *original_block = calloc(sizeof(qcms_transform) + sizeof(void*) + 16, 1);
	/* make room for a pointer to the block returned by calloc */
	void *transform_start = original_block + sizeof(void*);
	/* align transform_start */
	qcms_transform *transform_aligned = (qcms_transform*)(((uintptr_t)transform_start + 15) & ~0xf);

	/* store a pointer to the block returned by calloc so that we can free it later */
	void **(original_block_ptr) = (void**)transform_aligned;
	if (!original_block)
		return NULL;
	original_block_ptr--;
	*original_block_ptr = original_block;

	return transform_aligned;
}
static void transform_free(qcms_transform *t)
{
	/* get at the pointer to the unaligned block returned by calloc */
	void **p = (void**)t;
	p--;
	free(*p);
}
#endif

void qcms_transform_release(qcms_transform *t)
{
	/* ensure we only free the gamma tables once even if there are
	 * multiple references to the same data */

	if (t->output_table_r)
		precache_release(t->output_table_r);
	if (t->output_table_g)
		precache_release(t->output_table_g);
	if (t->output_table_b)
		precache_release(t->output_table_b);

	free(t->input_gamma_table_r);
	if (t->input_gamma_table_g != t->input_gamma_table_r)
		free(t->input_gamma_table_g);
	if (t->input_gamma_table_g != t->input_gamma_table_r &&
	    t->input_gamma_table_g != t->input_gamma_table_b)
		free(t->input_gamma_table_b);

	free(t->input_gamma_table_gray);

	free(t->output_gamma_lut_r);
	free(t->output_gamma_lut_g);
	free(t->output_gamma_lut_b);

	transform_free(t);
}

static void compute_precache_pow(uint8_t *output, float gamma)
{
	uint32_t v = 0;
	for (v = 0; v <= 0xffff; v++) {
		//XXX: don't do integer/float conversion... and round?
		output[v] = 255. * pow(v/65535., gamma);
	}
}

void compute_precache_lut(uint8_t *output, uint16_t *table, int length)
{
	uint32_t v = 0;
	for (v = 0; v <= 0xffff; v++) {
		//XXX: don't do integer/float conversion... round?
		output[v] = lut_interp_linear16(v, table, length) >> 8;
	}
}

void compute_precache_linear(uint8_t *output)
{
	uint32_t v = 0;
	for (v = 0; v <= 0xffff; v++) {
		//XXX: round?
		output[v] = v >> 8;
	}
}

qcms_bool compute_precache(struct curveType *trc, uint8_t *output)
{
	if (trc->count == 0) {
		compute_precache_linear(output);
	} else if (trc->count == 1) {
		compute_precache_pow(output, 1./u8Fixed8Number_to_float(trc->data[0]));
	} else {
		uint16_t *inverted = invert_lut(trc->data, trc->count);
		if (!inverted)
			return false;
		compute_precache_lut(output, inverted, trc->count);
		free(inverted);
	}
	return true;
}


// Determine if we can build with SSE2 (this was partly copied from jmorecfg.h in
// mozilla/jpeg)
 // -------------------------------------------------------------------------
#if defined(_M_IX86) && defined(_MSC_VER)
#define HAS_CPUID
/* Get us a CPUID function. Avoid clobbering EBX because sometimes it's the PIC
   register - I'm not sure if that ever happens on windows, but cpuid isn't
   on the critical path so we just preserve the register to be safe and to be
   consistent with the non-windows version. */
static void cpuid(uint32_t fxn, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
       uint32_t a_, b_, c_, d_;
       __asm {
              xchg   ebx, esi
              mov    eax, fxn
              cpuid
              mov    a_, eax
              mov    b_, ebx
              mov    c_, ecx
              mov    d_, edx
              xchg   ebx, esi
       }
       *a = a_;
       *b = b_;
       *c = c_;
       *d = d_;
}
#elif defined(__GNUC__) && defined(__i386__)
#define HAS_CPUID
/* Get us a CPUID function. We can't use ebx because it's the PIC register on
   some platforms, so we use ESI instead and save ebx to avoid clobbering it. */
static void cpuid(uint32_t fxn, uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {

	uint32_t a_, b_, c_, d_;
       __asm__ __volatile__ ("xchgl %%ebx, %%esi; cpuid; xchgl %%ebx, %%esi;" 
                             : "=a" (a_), "=S" (b_), "=c" (c_), "=d" (d_) : "a" (fxn));
	   *a = a_;
	   *b = b_;
	   *c = c_;
	   *d = d_;
}
#endif

// -------------------------Runtime SSE2 Detection-----------------------------

#define SSE2_EDX_MASK (1UL << 26)
static qcms_bool sse2_available(void)
{
#if defined(__x86_64__) || defined(_M_AMD64)
       return true;
#elif defined(HAS_CPUID)
       static int has_sse2 = -1;
       uint32_t a, b, c, d;
       uint32_t function = 0x00000001;

       if (has_sse2 == -1) {
              has_sse2 = 0;
	      cpuid(function, &a, &b, &c, &d);
              if (d & SSE2_EDX_MASK)
                     has_sse2 = 1;
              else
                     has_sse2 = 0;
       }

       return has_sse2;
#endif
       return false;
}


void build_output_lut(struct curveType *trc,
		uint16_t **output_gamma_lut, size_t *output_gamma_lut_length)
{
	if (trc->count == 0) {
		*output_gamma_lut = build_linear_table(4096);
		*output_gamma_lut_length = 4096;
	} else if (trc->count == 1) {
		float gamma = 1./u8Fixed8Number_to_float(trc->data[0]);
		*output_gamma_lut = build_pow_table(gamma, 4096);
		*output_gamma_lut_length = 4096;
	} else {
		*output_gamma_lut = invert_lut(trc->data, trc->count);
		*output_gamma_lut_length = trc->count;
	}

}

void qcms_profile_precache_output_transform(qcms_profile *profile)
{
	/* we only support precaching on rgb profiles */
	if (profile->color_space != RGB_SIGNATURE)
		return;

	if (!profile->output_table_r) {
		profile->output_table_r = precache_create();
		if (profile->output_table_r &&
				!compute_precache(profile->redTRC, profile->output_table_r->data)) {
			precache_release(profile->output_table_r);
			profile->output_table_r = NULL;
		}
	}
	if (!profile->output_table_g) {
		profile->output_table_g = precache_create();
		if (profile->output_table_g &&
				!compute_precache(profile->greenTRC, profile->output_table_g->data)) {
			precache_release(profile->output_table_g);
			profile->output_table_g = NULL;
		}
	}
	if (!profile->output_table_b) {
		profile->output_table_b = precache_create();
		if (profile->output_table_b &&
				!compute_precache(profile->blueTRC, profile->output_table_b->data)) {
			precache_release(profile->output_table_g);
			profile->output_table_g = NULL;
		}
	}
}

#define NO_MEM_TRANSFORM NULL

qcms_transform* qcms_transform_create(
		qcms_profile *in, qcms_data_type in_type,
		qcms_profile* out, qcms_data_type out_type,
		qcms_intent intent)
{
	bool precache = false;

        qcms_transform *transform = transform_alloc();
        if (!transform) {
		return NULL;
	}
	if (out_type != QCMS_DATA_RGB_8 &&
                out_type != QCMS_DATA_RGBA_8) {
            assert(0 && "output type");
	    free(transform);
            return NULL;
        }

	if (out->output_table_r &&
			out->output_table_g &&
			out->output_table_b) {
		precache = true;
	}

	if (precache) {
		transform->output_table_r = precache_reference(out->output_table_r);
		transform->output_table_g = precache_reference(out->output_table_g);
		transform->output_table_b = precache_reference(out->output_table_b);
	} else {
		build_output_lut(out->redTRC, &transform->output_gamma_lut_r, &transform->output_gamma_lut_r_length);
		build_output_lut(out->greenTRC, &transform->output_gamma_lut_g, &transform->output_gamma_lut_g_length);
		build_output_lut(out->blueTRC, &transform->output_gamma_lut_b, &transform->output_gamma_lut_b_length);
		if (!transform->output_gamma_lut_r || !transform->output_gamma_lut_g || !transform->output_gamma_lut_b) {
			qcms_transform_release(transform);
			return NO_MEM_TRANSFORM;
		}
	}

        if (in->color_space == RGB_SIGNATURE) {
            struct matrix in_matrix, out_matrix, result;

            if (in_type != QCMS_DATA_RGB_8 &&
                    in_type != QCMS_DATA_RGBA_8){
                assert(0 && "input type");
		free(transform);
                return NULL;
            }
	    if (precache) {
#ifdef X86
		    if (sse2_available()) {
			    if (in_type == QCMS_DATA_RGB_8)
				    transform->transform_fn = qcms_transform_data_rgb_out_lut_sse;
			    else
				    transform->transform_fn = qcms_transform_data_rgba_out_lut_sse;

		    } else
#endif
		    {
			    if (in_type == QCMS_DATA_RGB_8)
				    transform->transform_fn = qcms_transform_data_rgb_out_lut_precache;
			    else
				    transform->transform_fn = qcms_transform_data_rgba_out_lut_precache;
		    }
	    } else {
		    if (in_type == QCMS_DATA_RGB_8)
			    transform->transform_fn = qcms_transform_data_rgb_out_lut;
		    else
			    transform->transform_fn = qcms_transform_data_rgba_out_lut;
	    }

            //XXX: avoid duplicating tables if we can
            transform->input_gamma_table_r = build_input_gamma_table(in->redTRC);
            transform->input_gamma_table_g = build_input_gamma_table(in->greenTRC);
            transform->input_gamma_table_b = build_input_gamma_table(in->blueTRC);

	    if (!transform->input_gamma_table_r || !transform->input_gamma_table_g || !transform->input_gamma_table_b) {
		    qcms_transform_release(transform);
		    return NO_MEM_TRANSFORM;
	    }

            /* build combined colorant matrix */
            in_matrix = build_colorant_matrix(in);
            out_matrix = build_colorant_matrix(out);
            out_matrix = matrix_invert(out_matrix);
            if (out_matrix.invalid) {
                qcms_transform_release(transform);
                return NULL;
            }
            result = matrix_multiply(out_matrix, in_matrix);

            /* store the results in column major mode
             * this makes doing the multiplication with sse easier */
            transform->matrix[0][0] = result.m[0][0];
            transform->matrix[1][0] = result.m[0][1];
            transform->matrix[2][0] = result.m[0][2];
            transform->matrix[0][1] = result.m[1][0];
            transform->matrix[1][1] = result.m[1][1];
            transform->matrix[2][1] = result.m[1][2];
            transform->matrix[0][2] = result.m[2][0];
            transform->matrix[1][2] = result.m[2][1];
            transform->matrix[2][2] = result.m[2][2];

        } else if (in->color_space == GRAY_SIGNATURE) {
            if (in_type != QCMS_DATA_GRAY_8 &&
                    in_type != QCMS_DATA_GRAYA_8){
                assert(0 && "input type");
		free(transform);
                return NULL;
            }

            transform->input_gamma_table_gray = build_input_gamma_table(in->grayTRC);
	    if (!transform->input_gamma_table_gray) {
		    qcms_transform_release(transform);
		    return NO_MEM_TRANSFORM;
	    }

	    if (precache) {
		    if (in_type == QCMS_DATA_GRAY_8) {
			    transform->transform_fn = qcms_transform_data_gray_out_precache;
		    } else {
			    transform->transform_fn = qcms_transform_data_graya_out_precache;
		    }
	    } else {
		    if (in_type == QCMS_DATA_GRAY_8) {
			    transform->transform_fn = qcms_transform_data_gray_out_lut;
		    } else {
			    transform->transform_fn = qcms_transform_data_graya_out_lut;
		    }
	    }
	} else {
		assert(0 && "unexpected colorspace");
	}
	return transform;
}

void qcms_transform_data(qcms_transform *transform, void *src, void *dest, size_t length)
{
	transform->transform_fn(transform, src, dest, length);
}

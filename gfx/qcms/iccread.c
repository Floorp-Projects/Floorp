//  qcms
//  Copyright (C) 2009 Mozilla Foundation
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

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include "qcmsint.h"

//XXX: use a better typename
typedef uint32_t __be32;
typedef uint16_t __be16;

#if 0
not used yet
/* __builtin_bswap isn't available in older gccs
 * so open code it for now */
static __be32 cpu_to_be32(int32_t v)
{
#ifdef IS_LITTLE_ENDIAN
	return ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | ((v & 0xff000000) >> 24);
	//return __builtin_bswap32(v);
	return v;
#endif
}
#endif

static uint32_t be32_to_cpu(__be32 v)
{
#ifdef IS_LITTLE_ENDIAN
	return ((v & 0xff) << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | ((v & 0xff000000) >> 24);
	//return __builtin_bswap32(v);
#else
	return v;
#endif
}

static uint32_t be16_to_cpu(__be16 v)
{
#ifdef IS_LITTLE_ENDIAN
	return ((v & 0xff) << 8) | ((v & 0xff00) >> 8);
#else
	return v;
#endif
}

/* a wrapper around the memory that we are going to parse
 * into a qcms_profile */
struct mem_source
{
	const unsigned char *buf;
	size_t size;
	qcms_bool valid;
	const char *invalid_reason;
};

static void invalid_source(struct mem_source *mem, const char *reason)
{
	mem->valid = false;
	mem->invalid_reason = reason;
}

static uint32_t read_u32(struct mem_source *mem, size_t offset)
{
	if (offset + 4 > mem->size) {
		invalid_source(mem, "Invalid offset");
		return 0;
	} else {
		return be32_to_cpu(*(__be32*)(mem->buf + offset));
	}
}

static uint16_t read_u16(struct mem_source *mem, size_t offset)
{
	if (offset + 2 > mem->size) {
		invalid_source(mem, "Invalid offset");
		return 0;
	} else {
		return be16_to_cpu(*(__be16*)(mem->buf + offset));
	}
}

static uint8_t read_u8(struct mem_source *mem, size_t offset)
{
	if (offset + 1 > mem->size) {
		invalid_source(mem, "Invalid offset");
		return 0;
	} else {
		return *(uint8_t*)(mem->buf + offset);
	}
}

static s15Fixed16Number read_s15Fixed16Number(struct mem_source *mem, size_t offset)
{
	return read_u32(mem, offset);
}

#if 0
static uInt16Number read_uInt16Number(struct mem_source *mem, size_t offset)
{
	return read_u16(mem, offset);
}
#endif

#define BAD_VALUE_PROFILE NULL
#define INVALID_PROFILE NULL
#define NO_MEM_PROFILE NULL

/* An arbitrary 4MB limit on profile size */
#define MAX_PROFILE_SIZE 1024*1024*4
#define MAX_TAG_COUNT 1024

static void check_CMM_type_signature(struct mem_source *src)
{
	//uint32_t CMM_type_signature = read_u32(src, 4);
	//TODO: do the check?

}

static void check_profile_version(struct mem_source *src)
{
	uint8_t major_revision = read_u8(src, 8 + 0);
	uint8_t minor_revision = read_u8(src, 8 + 1);
	uint8_t reserved1      = read_u8(src, 8 + 2);
	uint8_t reserved2      = read_u8(src, 8 + 3);
	if (major_revision > 0x2)
		invalid_source(src, "Unsupported major revision");
	if (minor_revision > 0x40)
		invalid_source(src, "Unsupported minor revision");
	if (reserved1 != 0 || reserved2 != 0)
		invalid_source(src, "Invalid reserved bytes");
}

#define INPUT_DEVICE_PROFILE   0x73636e72 // 'scnr'
#define DISPLAY_DEVICE_PROFILE 0x6d6e7472 // 'mntr'
#define OUTPUT_DEVICE_PROFILE  0x70727472 // 'prtr'
#define DEVICE_LINK_PROFILE    0x6c696e6b // 'link'
#define COLOR_SPACE_PROFILE    0x73706163 // 'spac'
#define ABSTRACT_PROFILE       0x61627374 // 'abst'
#define NAMED_COLOR_PROFILE    0x6e6d636c // 'nmcl'

static void read_class_signature(qcms_profile *profile, struct mem_source *mem)
{
	profile->class = read_u32(mem, 12);
	switch (profile->class) {
		case DISPLAY_DEVICE_PROFILE:
		case INPUT_DEVICE_PROFILE:
			break;
		case OUTPUT_DEVICE_PROFILE:
		default:
			invalid_source(mem, "Invalid  Profile/Device Class signature");
	}
}

static void read_color_space(qcms_profile *profile, struct mem_source *mem)
{
	profile->color_space = read_u32(mem, 16);
	switch (profile->color_space) {
		case RGB_SIGNATURE:
		case GRAY_SIGNATURE:
			break;
		default:
			invalid_source(mem, "Unsupported colorspace");
	}
}

struct tag
{
	uint32_t signature;
	uint32_t offset;
	uint32_t size;
};

struct tag_index {
	uint32_t count;
	struct tag *tags;
};

static struct tag_index read_tag_table(qcms_profile *profile, struct mem_source *mem)
{
	struct tag_index index = {0, NULL};
	int i;

	index.count = read_u32(mem, 128);
	if (index.count > MAX_TAG_COUNT) {
		invalid_source(mem, "max number of tags exceeded");
		return index;
	}

	index.tags = malloc(sizeof(struct tag)*index.count);
	if (index.tags) {
		for (i = 0; i < index.count; i++) {
			index.tags[i].signature = read_u32(mem, 128 + 4 + 4*i*3);
			index.tags[i].offset    = read_u32(mem, 128 + 4 + 4*i*3 + 4);
			index.tags[i].size      = read_u32(mem, 128 + 4 + 4*i*3 + 8);
		}
	}

	return index;
}

// Checks a profile for obvious inconsistencies and returns
// true if the profile looks bogus and should probably be
// ignored.
qcms_bool qcms_profile_is_bogus(qcms_profile *profile)
{
       float sum[3], target[3], tolerance[3];
       unsigned i;

       // Sum the values
       sum[0] = s15Fixed16Number_to_float(profile->redColorant.X) +
	       s15Fixed16Number_to_float(profile->greenColorant.X) +
	       s15Fixed16Number_to_float(profile->blueColorant.X);
       sum[1] = s15Fixed16Number_to_float(profile->redColorant.Y) +
	       s15Fixed16Number_to_float(profile->greenColorant.Y) +
	       s15Fixed16Number_to_float(profile->blueColorant.Y);
       sum[2] = s15Fixed16Number_to_float(profile->redColorant.Z) +
	       s15Fixed16Number_to_float(profile->greenColorant.Z) +
	       s15Fixed16Number_to_float(profile->blueColorant.Z);

       // Build our target vector (see mozilla bug 460629)
       target[0] = 0.96420;
       target[1] = 1.00000;
       target[2] = 0.82491;

       // Our tolerance vector - Recommended by Chris Murphy based on
       // conversion from the LAB space criterion of no more than 3 in any one
       // channel. This is similar to, but slightly more tolerant than Adobe's
       // criterion.
       tolerance[0] = 0.02;
       tolerance[1] = 0.02;
       tolerance[2] = 0.04;

       // Compare with our tolerance
       for (i = 0; i < 3; ++i) {
           if (!(((sum[i] - tolerance[i]) <= target[i]) &&
                 ((sum[i] + tolerance[i]) >= target[i])))
               return true;
       }

       // All Good
       return false;
}

#define TAG_bXYZ 0x6258595a
#define TAG_gXYZ 0x6758595a
#define TAG_rXYZ 0x7258595a
#define TAG_rTRC 0x72545243
#define TAG_bTRC 0x62545243
#define TAG_gTRC 0x67545243
#define TAG_kTRC 0x6b545243
#define TAG_A2B0 0x41324230

static struct tag *find_tag(struct tag_index index, uint32_t tag_id)
{
	int i;
	struct tag *tag = NULL;
	for (i = 0; i < index.count; i++) {
		if (index.tags[i].signature == tag_id) {
			return &index.tags[i];
		}
	}
	return tag;
}

#define XYZ_TYPE   0x58595a20 // 'XYZ '
#define CURVE_TYPE 0x63757276 // 'curv'
#define LUT16_TYPE 0x6d667432 // 'mft2'
#define LUT8_TYPE  0x6d667431 // 'mft1'

static struct XYZNumber read_tag_XYZType(struct mem_source *src, struct tag_index index, uint32_t tag_id)
{
	struct XYZNumber num = {0};
	struct tag *tag = find_tag(index, tag_id);
	if (tag) {
		uint32_t offset = tag->offset;

		uint32_t type = read_u32(src, offset);
		if (type != XYZ_TYPE)
			invalid_source(src, "unexpected type, expected XYZ");
		num.X = read_s15Fixed16Number(src, offset+8);
		num.Y = read_s15Fixed16Number(src, offset+12);
		num.Z = read_s15Fixed16Number(src, offset+16);
	} else {
		invalid_source(src, "missing xyztag");
	}
	return num;
}

static struct curveType *read_tag_curveType(struct mem_source *src, struct tag_index index, uint32_t tag_id)
{
	struct tag *tag = find_tag(index, tag_id);
	struct curveType *curve = NULL;
	if (tag) {
		uint32_t offset = tag->offset;
		uint32_t type = read_u32(src, offset);
		uint32_t count = read_u32(src, offset+8);
		int i;

		if (type != CURVE_TYPE) {
			invalid_source(src, "unexpected type, expected CURV");
			return NULL;
		}

#define MAX_CURVE_ENTRIES 40000 //arbitrary
		if (count > MAX_CURVE_ENTRIES) {
			invalid_source(src, "curve size too large");
			return NULL;
		}
		curve = malloc(sizeof(struct curveType) + sizeof(uInt16Number)*count);
		if (!curve)
			return NULL;

		curve->count = count;
		for (i=0; i<count; i++) {
			curve->data[i] = read_u16(src, offset + 12 + i *2);
		}
	} else {
		invalid_source(src, "missing curvetag");
	}

	return curve;
}

/* This function's not done yet */
static struct lutType *read_tag_lutType(struct mem_source *src, struct tag_index index, uint32_t tag_id)
{
	struct tag *tag = find_tag(index, tag_id);
	uint32_t offset = tag->offset;
	uint32_t type = read_u32(src, offset);
	uint16_t num_input_table_entries;
	uint16_t num_output_table_entries;
	uint8_t in_chan, grid_points, out_chan;
	uint32_t clut_size;
	struct lutType *lut;
	int i;

	num_input_table_entries  = read_u16(src, offset + 48);
	num_output_table_entries = read_u16(src, offset + 50);

	in_chan     = read_u8(src, offset + 8);
	out_chan    = read_u8(src, offset + 9);
	grid_points = read_u8(src, offset + 10);

	if (!src->valid)
		return NULL;

	clut_size = in_chan * grid_points * out_chan;
#define MAX_CLUT_SIZE 10000 // arbitrary
	if (clut_size > MAX_CLUT_SIZE) {
		return NULL;
	}

	if (type != LUT16_TYPE && type != LUT8_TYPE)
		return NULL;

	lut = malloc(sizeof(struct lutType) + (clut_size + num_input_table_entries + num_output_table_entries)*sizeof(uint8_t));
	if (!lut)
		return NULL;
	lut->num_input_channels   = read_u8(src, offset + 8);
	lut->num_output_channels  = read_u8(src, offset + 9);
	lut->num_clut_grid_points = read_u8(src, offset + 10);
	lut->e00 = read_s15Fixed16Number(src, offset+12);
	lut->e01 = read_s15Fixed16Number(src, offset+16);
	lut->e02 = read_s15Fixed16Number(src, offset+20);
	lut->e10 = read_s15Fixed16Number(src, offset+24);
	lut->e11 = read_s15Fixed16Number(src, offset+28);
	lut->e12 = read_s15Fixed16Number(src, offset+32);
	lut->e20 = read_s15Fixed16Number(src, offset+36);
	lut->e21 = read_s15Fixed16Number(src, offset+40);
	lut->e22 = read_s15Fixed16Number(src, offset+44);

	//TODO: finish up
	for (i = 0; i < lut->num_input_table_entries; i++) {
	}
	return lut;
}

static void read_rendering_intent(qcms_profile *profile, struct mem_source *src)
{
	profile->rendering_intent = read_u32(src, 64);
	switch (profile->rendering_intent) {
		case QCMS_INTENT_PERCEPTUAL:
		case QCMS_INTENT_SATURATION:
		case QCMS_INTENT_RELATIVE_COLORIMETRIC:
		case QCMS_INTENT_ABSOLUTE_COLORIMETRIC:
			break;
		default:
			invalid_source(src, "unknown rendering intent");
	}
}

qcms_profile *qcms_profile_create(void)
{
	return calloc(sizeof(qcms_profile), 1);
}

/* build sRGB gamma table */
/* based on cmsBuildParametricGamma() */
static uint16_t *build_sRGB_gamma_table(int num_entries)
{
	int i;
	/* taken from lcms: Build_sRGBGamma() */
	double gamma = 2.4;
	double a = 1./1.055;
	double b = 0.055/1.055;
	double c = 1./12.92;
	double d = 0.04045;

	uint16_t *table = malloc(sizeof(uint16_t) * num_entries);
	if (!table)
		return NULL;

	for (i=0; i<num_entries; i++) {
		double x = (double)i / (num_entries-1);
		double y, output;
		// IEC 61966-2.1 (sRGB)
		// Y = (aX + b)^Gamma | X >= d
		// Y = cX             | X < d
		if (x >= d) {
			double e = (a*x + b);
			if (e > 0)
				y = pow(e, gamma);
			else
				y = 0;
		} else {
			y = c*x;
		}

		// Saturate -- this could likely move to a separate function
		output = y * 65535. + .5;
		if (output > 65535.)
			output = 65535;
		if (output < 0)
			output = 0;
		table[i] = (uint16_t)floor(output);
	}
	return table;
}

static struct curveType *curve_from_table(uint16_t *table, int num_entries)
{
	struct curveType *curve;
	int i;
	curve = malloc(sizeof(struct curveType) + sizeof(uInt16Number)*num_entries);
	if (!curve)
		return NULL;
	curve->count = num_entries;
	for (i = 0; i < num_entries; i++) {
		curve->data[i] = table[i];
	}
	return curve;
}

static uint16_t float_to_u8Fixed8Number(float a)
{
	if (a > (255. + 255./256))
		return 0xffff;
	else if (a < 0.)
		return 0;
	else
		return floor(a*256. + .5);
}

static struct curveType *curve_from_gamma(float gamma)
{
	struct curveType *curve;
	int num_entries = 1;
	curve = malloc(sizeof(struct curveType) + sizeof(uInt16Number)*num_entries);
	if (!curve)
		return NULL;
	curve->count = num_entries;
	curve->data[0] = float_to_u8Fixed8Number(gamma);
	return curve;
}

static void qcms_profile_fini(qcms_profile *profile)
{
	free(profile->redTRC);
	free(profile->blueTRC);
	free(profile->greenTRC);
	free(profile->grayTRC);
	free(profile);
}

//XXX: it would be nice if we had a way of ensuring
// everything in a profile was initialized regardless of how it was created

//XXX: should this also be taking a black_point?
/* similar to CGColorSpaceCreateCalibratedRGB */
qcms_profile* qcms_profile_create_rgb_with_gamma(
		qcms_CIE_xyY white_point,
		qcms_CIE_xyYTRIPLE primaries,
		float gamma)
{
	qcms_profile* profile = qcms_profile_create();
	if (!profile)
		return NO_MEM_PROFILE;

	//XXX: should store the whitepoint
	if (!set_rgb_colorants(profile, white_point, primaries)) {
		qcms_profile_fini(profile);
		return INVALID_PROFILE;
	}

	profile->redTRC = curve_from_gamma(gamma);
	profile->blueTRC = curve_from_gamma(gamma);
	profile->greenTRC = curve_from_gamma(gamma);

	if (!profile->redTRC || !profile->blueTRC || !profile->greenTRC) {
		qcms_profile_fini(profile);
		return NO_MEM_PROFILE;
	}
	profile->class = DISPLAY_DEVICE_PROFILE;
	profile->rendering_intent = QCMS_INTENT_PERCEPTUAL;
	profile->color_space = RGB_SIGNATURE;
	return profile;
}

qcms_profile* qcms_profile_create_rgb_with_table(
		qcms_CIE_xyY white_point,
		qcms_CIE_xyYTRIPLE primaries,
		uint16_t *table, int num_entries)
{
	qcms_profile* profile = qcms_profile_create();
	if (!profile)
		return NO_MEM_PROFILE;

	//XXX: should store the whitepoint
	if (!set_rgb_colorants(profile, white_point, primaries)) {
		qcms_profile_fini(profile);
		return INVALID_PROFILE;
	}

	profile->redTRC = curve_from_table(table, num_entries);
	profile->blueTRC = curve_from_table(table, num_entries);
	profile->greenTRC = curve_from_table(table, num_entries);

	if (!profile->redTRC || !profile->blueTRC || !profile->greenTRC) {
		qcms_profile_fini(profile);
		return NO_MEM_PROFILE;
	}
	profile->class = DISPLAY_DEVICE_PROFILE;
	profile->rendering_intent = QCMS_INTENT_PERCEPTUAL;
	profile->color_space = RGB_SIGNATURE;
	return profile;
}

/* from lcms: cmsWhitePointFromTemp */
/* tempK must be >= 4000. and <= 25000.
 * similar to argyll: icx_DTEMP2XYZ() */
static qcms_CIE_xyY white_point_from_temp(int temp_K)
{
	qcms_CIE_xyY white_point;
	double x, y;
	double T, T2, T3;
	// double M1, M2;

	// No optimization provided.
	T = temp_K;
	T2 = T*T;            // Square
	T3 = T2*T;           // Cube

	// For correlated color temperature (T) between 4000K and 7000K:
	if (T >= 4000. && T <= 7000.) {
		x = -4.6070*(1E9/T3) + 2.9678*(1E6/T2) + 0.09911*(1E3/T) + 0.244063;
	} else {
		// or for correlated color temperature (T) between 7000K and 25000K:
		if (T > 7000.0 && T <= 25000.0) {
			x = -2.0064*(1E9/T3) + 1.9018*(1E6/T2) + 0.24748*(1E3/T) + 0.237040;
		} else {
			assert(0 && "invalid temp");
		}
	}

	// Obtain y(x)

	y = -3.000*(x*x) + 2.870*x - 0.275;

	// wave factors (not used, but here for futures extensions)

	// M1 = (-1.3515 - 1.7703*x + 5.9114 *y)/(0.0241 + 0.2562*x - 0.7341*y);
	// M2 = (0.0300 - 31.4424*x + 30.0717*y)/(0.0241 + 0.2562*x - 0.7341*y);

	// Fill white_point struct
	white_point.x = x;
	white_point.y = y;
	white_point.Y = 1.0;

	return white_point;
}

qcms_profile* qcms_profile_sRGB(void)
{
	qcms_profile *profile;
	uint16_t *table;

	qcms_CIE_xyYTRIPLE Rec709Primaries = {
		{0.6400, 0.3300, 1.0},
		{0.3000, 0.6000, 1.0},
		{0.1500, 0.0600, 1.0}
	};
	qcms_CIE_xyY D65;

	D65 = white_point_from_temp(6504);

	table = build_sRGB_gamma_table(1024);

	if (!table)
		return NO_MEM_PROFILE;

	profile = qcms_profile_create_rgb_with_table(D65, Rec709Primaries, table, 1024);
	free(table);
	return profile;
}


/* qcms_profile_from_memory does not hold a reference to the memory passed in */
qcms_profile* qcms_profile_from_memory(const void *mem, size_t size)
{
	uint32_t length;
	struct mem_source source;
	struct mem_source *src = &source;
	struct tag_index index;
	qcms_profile *profile;

	source.buf = mem;
	source.size = size;
	source.valid = true;
	length = read_u32(src, 0);
	if (length <= size) {
		// shrink the area that we can read if appropriate
		source.size = length;
	} else {
		return INVALID_PROFILE;
	}

	profile = qcms_profile_create();
	if (!profile)
		return NO_MEM_PROFILE;

	check_CMM_type_signature(src);
	check_profile_version(src);
	read_class_signature(profile, src);
	read_rendering_intent(profile, src);
	read_color_space(profile, src);
	//TODO read rest of profile stuff

	if (!src->valid)
		goto invalid_profile;

	index = read_tag_table(profile, src);
	if (!src->valid || !index.tags)
		goto invalid_tag_table;

	if (profile->class == DISPLAY_DEVICE_PROFILE || profile->class == INPUT_DEVICE_PROFILE) {
		if (profile->color_space == RGB_SIGNATURE) {

			profile->redColorant = read_tag_XYZType(src, index, TAG_rXYZ);
			profile->blueColorant = read_tag_XYZType(src, index, TAG_bXYZ);
			profile->greenColorant = read_tag_XYZType(src, index, TAG_gXYZ);

			if (!src->valid)
				goto invalid_tag_table;

			profile->redTRC = read_tag_curveType(src, index, TAG_rTRC);
			profile->blueTRC = read_tag_curveType(src, index, TAG_bTRC);
			profile->greenTRC = read_tag_curveType(src, index, TAG_gTRC);

			if (!profile->redTRC || !profile->blueTRC || !profile->greenTRC)
				goto invalid_tag_table;

		} else if (profile->color_space == GRAY_SIGNATURE) {

			profile->grayTRC = read_tag_curveType(src, index, TAG_kTRC);
			if (!profile->grayTRC)
				goto invalid_tag_table;

		} else {
			goto invalid_tag_table;
		}
	} else if (0 && profile->class == OUTPUT_DEVICE_PROFILE) {
		profile->A2B0 = read_tag_lutType(src, index, TAG_A2B0);
	} else {
		goto invalid_tag_table;
	}

	if (!src->valid)
		goto invalid_tag_table;

	free(index.tags);

	return profile;

invalid_tag_table:
	free(index.tags);
invalid_profile:
	qcms_profile_fini(profile);
	return INVALID_PROFILE;
}

qcms_intent qcms_profile_get_rendering_intent(qcms_profile *profile)
{
	return profile->rendering_intent;
}

icColorSpaceSignature
qcms_profile_get_color_space(qcms_profile *profile)
{
	return profile->color_space;
}

void qcms_profile_release(qcms_profile *profile)
{
	if (profile->output_table_r)
		precache_release(profile->output_table_r);
	if (profile->output_table_g)
		precache_release(profile->output_table_g);
	if (profile->output_table_b)
		precache_release(profile->output_table_b);

	qcms_profile_fini(profile);
}

#include <stdio.h>
qcms_profile* qcms_profile_from_file(FILE *file)
{
	uint32_t length, remaining_length;
	qcms_profile *profile;
	size_t read_length;
	__be32 length_be;
	void *data;

	fread(&length_be, sizeof(length), 1, file);
	length = be32_to_cpu(length_be);
	if (length > MAX_PROFILE_SIZE)
		return BAD_VALUE_PROFILE;

	/* allocate room for the entire profile */
	data = malloc(length);
	if (!data)
		return NO_MEM_PROFILE;

	/* copy in length to the front so that the buffer will contain the entire profile */
	*((__be32*)data) = length_be;
	remaining_length = length - sizeof(length_be);

	/* read the rest profile */
	read_length = fread((unsigned char*)data + sizeof(length_be), 1, remaining_length, file);
	if (read_length != remaining_length)
		return INVALID_PROFILE;

	profile = qcms_profile_from_memory(data, length);
	free(data);
	return profile;
}

qcms_profile* qcms_profile_from_path(const char *path)
{
	qcms_profile *profile = NULL;
	FILE *file = fopen(path, "r");
	if (file) {
		profile = qcms_profile_from_file(file);
		fclose(file);
	}
	return profile;
}

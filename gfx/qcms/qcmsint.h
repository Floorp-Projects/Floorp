#include "qcms.h"
#include "qcmstypes.h"

/* used as a 16bit lookup table for the output transformation.
 * we refcount them so we only need to have one around per output
 * profile, instead of duplicating them per transform */
struct precache_output
{
	int ref_count;
	uint8_t data[65535];
};

#ifdef _MSC_VER
#define ALIGN __declspec(align(16))
#else
#define ALIGN __attribute__(( aligned (16) ))
#endif

struct _qcms_transform {
	float ALIGN matrix[3][4];
	float *input_gamma_table_r;
	float *input_gamma_table_g;
	float *input_gamma_table_b;

	float *input_gamma_table_gray;

	float out_gamma_r;
	float out_gamma_g;
	float out_gamma_b;

	float out_gamma_gray;

	uint16_t *output_gamma_lut_r;
	uint16_t *output_gamma_lut_g;
	uint16_t *output_gamma_lut_b;

	uint16_t *output_gamma_lut_gray;

	size_t output_gamma_lut_r_length;
	size_t output_gamma_lut_g_length;
	size_t output_gamma_lut_b_length;

	size_t output_gamma_lut_gray_length;

	struct precache_output *output_table_r;
	struct precache_output *output_table_g;
	struct precache_output *output_table_b;

	void (*transform_fn)(struct _qcms_transform *transform, unsigned char *src, unsigned char *dest, size_t length);
};

typedef int32_t s15Fixed16Number;
typedef uint16_t uInt16Number;

struct XYZNumber {
	s15Fixed16Number X;
	s15Fixed16Number Y;
	s15Fixed16Number Z;
};

struct curveType {
	uint32_t count;
	uInt16Number data[0];
};

struct lutType {
	uint8_t num_input_channels;
	uint8_t num_output_channels;
	uint8_t num_clut_grid_points;

	s15Fixed16Number e00;
	s15Fixed16Number e01;
	s15Fixed16Number e02;
	s15Fixed16Number e10;
	s15Fixed16Number e11;
	s15Fixed16Number e12;
	s15Fixed16Number e20;
	s15Fixed16Number e21;
	s15Fixed16Number e22;

	uint16_t num_input_table_entries;
	uint16_t num_output_table_entries;

	uint16_t *input_table;
	uint16_t *clut_table;
	uint16_t *output_table;
};
#if 0
this is from an intial idea of having the struct correspond to the data in
the file. I decided that it wasn't a good idea.
struct tag_value {
	uint32_t type;
	union {
		struct {
			uint32_t reserved;
			struct {
				s15Fixed16Number X;
				s15Fixed16Number Y;
				s15Fixed16Number Z;
			} XYZNumber;
		} XYZType;
	};
}; // I guess we need to pack this?
#endif

#define RGB_SIGNATURE  0x52474220
#define GRAY_SIGNATURE 0x47524159

struct _qcms_profile {
	uint32_t class;
	uint32_t color_space;
	qcms_intent rendering_intent;
	struct XYZNumber redColorant;
	struct XYZNumber blueColorant;
	struct XYZNumber greenColorant;
	struct curveType *redTRC;
	struct curveType *blueTRC;
	struct curveType *greenTRC;
	struct curveType *grayTRC;
	struct lutType *A2B0;

	struct precache_output *output_table_r;
	struct precache_output *output_table_g;
	struct precache_output *output_table_b;
};

#ifdef _MSC_VER
#define inline _inline
#endif

static inline float s15Fixed16Number_to_float(s15Fixed16Number a)
{
	return ((int32_t)a)/65536.;
}

static inline s15Fixed16Number double_to_s15Fixed16Number(double v)
{
	return (int32_t)(v*65536);
}

void precache_release(struct precache_output *p);
qcms_bool set_rgb_colorants(qcms_profile *profile, qcms_CIE_xyY white_point, qcms_CIE_xyYTRIPLE primaries);

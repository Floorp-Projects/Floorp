#ifndef PACKAGE
#  error config.h must be included before pixman-private.h
#endif

#ifndef PIXMAN_PRIVATE_H
#define PIXMAN_PRIVATE_H

#include "pixman.h"
#include <time.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define MSBFirst 0
#define LSBFirst 1

#ifdef WORDS_BIGENDIAN
#  define IMAGE_BYTE_ORDER MSBFirst
#  define BITMAP_BIT_ORDER MSBFirst
#else
#  define IMAGE_BYTE_ORDER LSBFirst
#  define BITMAP_BIT_ORDER LSBFirst
#endif

#undef DEBUG
#define DEBUG 0

#if defined (__GNUC__)
#  define FUNC     ((const char*) (__PRETTY_FUNCTION__))
#elif defined (__sun) || (defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#  define FUNC     ((const char*) (__func__))
#else
#  define FUNC     ((const char*) ("???"))
#endif

#ifndef INT16_MIN
# define INT16_MIN              (-32767-1)
#endif

#ifndef INT16_MAX
# define INT16_MAX              (32767)
#endif

#ifndef INT32_MIN
# define INT32_MIN              (-2147483647-1)
#endif

#ifndef INT32_MAX
# define INT32_MAX              (2147483647)
#endif

#ifndef UINT32_MIN
# define UINT32_MIN             (0)
#endif

#ifndef UINT32_MAX
# define UINT32_MAX             (4294967295U)
#endif

#ifndef M_PI
# define M_PI			3.14159265358979323846
#endif

#ifdef _MSC_VER
/* 'inline' is available only in C++ in MSVC */
#   define inline __inline
#   define force_inline __forceinline
#elif defined __GNUC__
#   define inline __inline__
#   define force_inline __inline__ __attribute__ ((__always_inline__))
#else
# ifndef force_inline
#  define force_inline inline
# endif
#endif

#define FB_SHIFT    5
#define FB_UNIT     (1 << FB_SHIFT)
#define FB_HALFUNIT (1 << (FB_SHIFT-1))
#define FB_MASK     (FB_UNIT - 1)
#define FB_ALLONES  ((uint32_t) -1)

/* Memory allocation helpers */
void *pixman_malloc_ab (unsigned int n, unsigned int b);
void *pixman_malloc_abc (unsigned int a, unsigned int b, unsigned int c);
pixman_bool_t pixman_multiply_overflows_int (unsigned int a, unsigned int b);
pixman_bool_t pixman_addition_overflows_int (unsigned int a, unsigned int b);

#if DEBUG

#define return_if_fail(expr)						\
    do									\
    {									\
	if (!(expr))							\
	{								\
	    fprintf(stderr, "In %s: %s failed\n", FUNC, #expr);		\
	    return;							\
	}								\
    }									\
    while (0)

#define return_val_if_fail(expr, retval) 				\
    do									\
    {									\
	if (!(expr))							\
	{								\
	    fprintf(stderr, "In %s: %s failed\n", FUNC, #expr);		\
	    return (retval);						\
	}								\
    }									\
    while (0)

#else

#define return_if_fail(expr)						\
    do									\
    {									\
	if (!(expr))							\
	    return;							\
    }									\
    while (0)

#define return_val_if_fail(expr, retval)				\
    do									\
    {									\
	if (!(expr))							\
	    return (retval);						\
    }									\
    while (0)

#endif

typedef struct image_common image_common_t;
typedef struct source_image source_image_t;
typedef struct solid_fill solid_fill_t;
typedef struct gradient gradient_t;
typedef struct linear_gradient linear_gradient_t;
typedef struct horizontal_gradient horizontal_gradient_t;
typedef struct vertical_gradient vertical_gradient_t;
typedef struct conical_gradient conical_gradient_t;
typedef struct radial_gradient radial_gradient_t;
typedef struct bits_image bits_image_t;
typedef struct circle circle_t;
typedef struct point point_t;

/* FIXME - the types and structures below should be give proper names
 */

#define FASTCALL
typedef FASTCALL void (*CombineMaskU32) (uint32_t *src, const uint32_t *mask, int width);
typedef FASTCALL void (*CombineFuncU32) (uint32_t *dest, const uint32_t *src, int width);
typedef FASTCALL void (*CombineFuncC32) (uint32_t *dest, uint32_t *src, uint32_t *mask, int width);
typedef FASTCALL void (*fetchProc32)(bits_image_t *pict, int x, int y, int width,
                                     uint32_t *buffer);
typedef FASTCALL uint32_t (*fetchPixelProc32)(bits_image_t *pict, int offset, int line);
typedef FASTCALL void (*storeProc32)(pixman_image_t *, uint32_t *bits,
                                     const uint32_t *values, int x, int width,
                                     const pixman_indexed_t *);

typedef FASTCALL void (*CombineMaskU64) (uint64_t *src, const uint64_t *mask, int width);
typedef FASTCALL void (*CombineFuncU64) (uint64_t *dest, const uint64_t *src, int width);
typedef FASTCALL void (*CombineFuncC64) (uint64_t *dest, uint64_t *src, uint64_t *mask, int width);
typedef FASTCALL void (*fetchProc64)(bits_image_t *pict, int x, int y, int width,
                                     uint64_t *buffer);
typedef FASTCALL uint64_t (*fetchPixelProc64)(bits_image_t *pict, int offset, int line);
typedef FASTCALL void (*storeProc64)(pixman_image_t *, uint32_t *bits,
                                     const uint64_t *values, int x, int width,
                                     const pixman_indexed_t *);

typedef struct _FbComposeData {
    uint8_t	 op;
    pixman_image_t	*src;
    pixman_image_t	*mask;
    pixman_image_t	*dest;
    int16_t	 xSrc;
    int16_t	 ySrc;
    int16_t	 xMask;
    int16_t	 yMask;
    int16_t	 xDest;
    int16_t	 yDest;
    uint16_t	 width;
    uint16_t	 height;
} FbComposeData;

typedef struct _FbComposeFunctions32 {
    CombineFuncU32 *combineU;
    CombineFuncC32 *combineC;
    CombineMaskU32 combineMaskU;
} FbComposeFunctions32;

typedef struct _FbComposeFunctions64 {
    CombineFuncU64 *combineU;
    CombineFuncC64 *combineC;
    CombineMaskU64 combineMaskU;
} FbComposeFunctions64;

extern FbComposeFunctions32 pixman_composeFunctions;
extern FbComposeFunctions64 pixman_composeFunctions64;

void pixman_composite_rect_general_accessors (const FbComposeData *data,
                                              void *src_buffer,
                                              void *mask_buffer,
                                              void *dest_buffer,
                                              const int wide);
void pixman_composite_rect_general (const FbComposeData *data);

fetchProc32 pixman_fetchProcForPicture32 (bits_image_t *);
fetchPixelProc32 pixman_fetchPixelProcForPicture32 (bits_image_t *);
storeProc32 pixman_storeProcForPicture32 (bits_image_t *);
fetchProc32 pixman_fetchProcForPicture32_accessors (bits_image_t *);
fetchPixelProc32 pixman_fetchPixelProcForPicture32_accessors (bits_image_t *);
storeProc32 pixman_storeProcForPicture32_accessors (bits_image_t *);

fetchProc64 pixman_fetchProcForPicture64 (bits_image_t *);
fetchPixelProc64 pixman_fetchPixelProcForPicture64 (bits_image_t *);
storeProc64 pixman_storeProcForPicture64 (bits_image_t *);
fetchProc64 pixman_fetchProcForPicture64_accessors (bits_image_t *);
fetchPixelProc64 pixman_fetchPixelProcForPicture64_accessors (bits_image_t *);
storeProc64 pixman_storeProcForPicture64_accessors (bits_image_t *);

void pixman_expand(uint64_t *dst, const uint32_t *src, pixman_format_code_t, int width);
void pixman_contract(uint32_t *dst, const uint64_t *src, int width);

void pixmanFetchSourcePict(source_image_t *, int x, int y, int width,
                           uint32_t *buffer, uint32_t *mask, uint32_t maskBits);
void pixmanFetchSourcePict64(source_image_t *, int x, int y, int width,
                             uint64_t *buffer, uint64_t *mask, uint32_t maskBits);

void fbFetchTransformed(bits_image_t *, int x, int y, int width,
                        uint32_t *buffer, uint32_t *mask, uint32_t maskBits);
void fbStoreExternalAlpha(bits_image_t *, int x, int y, int width,
                          uint32_t *buffer);
void fbFetchExternalAlpha(bits_image_t *, int x, int y, int width,
                          uint32_t *buffer, uint32_t *mask, uint32_t maskBits);

void fbFetchTransformed_accessors(bits_image_t *, int x, int y, int width,
                                  uint32_t *buffer, uint32_t *mask,
                                  uint32_t maskBits);
void fbStoreExternalAlpha_accessors(bits_image_t *, int x, int y, int width,
                                    uint32_t *buffer);
void fbFetchExternalAlpha_accessors(bits_image_t *, int x, int y, int width,
                                    uint32_t *buffer, uint32_t *mask,
                                    uint32_t maskBits);

void fbFetchTransformed64(bits_image_t *, int x, int y, int width,
                          uint64_t *buffer, uint64_t *mask, uint32_t maskBits);
void fbStoreExternalAlpha64(bits_image_t *, int x, int y, int width,
                            uint64_t *buffer);
void fbFetchExternalAlpha64(bits_image_t *, int x, int y, int width,
                            uint64_t *buffer, uint64_t *mask, uint32_t maskBits);

void fbFetchTransformed64_accessors(bits_image_t *, int x, int y, int width,
                                    uint64_t *buffer, uint64_t *mask,
                                    uint32_t maskBits);
void fbStoreExternalAlpha64_accessors(bits_image_t *, int x, int y, int width,
                                      uint64_t *buffer);
void fbFetchExternalAlpha64_accessors(bits_image_t *, int x, int y, int width,
                                      uint64_t *buffer, uint64_t *mask,
                                      uint32_t maskBits);

/* end */

typedef enum
{
    BITS,
    LINEAR,
    CONICAL,
    RADIAL,
    SOLID
} image_type_t;

#define IS_SOURCE_IMAGE(img)     (((image_common_t *)img)->type > BITS)

typedef enum
{
    SOURCE_IMAGE_CLASS_UNKNOWN,
    SOURCE_IMAGE_CLASS_HORIZONTAL,
    SOURCE_IMAGE_CLASS_VERTICAL
} source_pict_class_t;

struct point
{
    int16_t x, y;
};

struct image_common
{
    image_type_t		type;
    int32_t			ref_count;
    pixman_region32_t		full_region;
    pixman_region32_t		clip_region;
    pixman_region32_t	       *src_clip;
    pixman_bool_t               has_client_clip;
    pixman_transform_t	       *transform;
    pixman_repeat_t		repeat;
    pixman_filter_t		filter;
    pixman_fixed_t	       *filter_params;
    int				n_filter_params;
    bits_image_t	       *alpha_map;
    point_t			alpha_origin;
    pixman_bool_t		component_alpha;
    pixman_read_memory_func_t	read_func;
    pixman_write_memory_func_t	write_func;
};

struct source_image
{
    image_common_t	common;
    source_pict_class_t class;
};

struct solid_fill
{
    source_image_t	common;
    uint32_t		color;		/* FIXME: shouldn't this be a pixman_color_t? */
};

struct gradient
{
    source_image_t		common;
    int				n_stops;
    pixman_gradient_stop_t *	stops;
    int				stop_range;
    uint32_t *			color_table;
    int				color_table_size;
};

struct linear_gradient
{
    gradient_t			common;
    pixman_point_fixed_t	p1;
    pixman_point_fixed_t	p2;
};

struct circle
{
    pixman_fixed_t x;
    pixman_fixed_t y;
    pixman_fixed_t radius;
};

struct radial_gradient
{
    gradient_t	common;

    circle_t	c1;
    circle_t	c2;
    double	cdx;
    double	cdy;
    double	dr;
    double	A;
};

struct conical_gradient
{
    gradient_t			common;
    pixman_point_fixed_t	center;
    pixman_fixed_t		angle;
};

struct bits_image
{
    image_common_t		common;
    pixman_format_code_t	format;
    const pixman_indexed_t     *indexed;
    int				width;
    int				height;
    uint32_t *			bits;
    uint32_t *			free_me;
    int				rowstride; /* in number of uint32_t's */
};

union pixman_image
{
    image_type_t		type;
    image_common_t		common;
    bits_image_t		bits;
    gradient_t			gradient;
    linear_gradient_t		linear;
    conical_gradient_t		conical;
    radial_gradient_t		radial;
    solid_fill_t		solid;
};


#define LOG2_BITMAP_PAD 5
#define FB_STIP_SHIFT	LOG2_BITMAP_PAD
#define FB_STIP_UNIT	(1 << FB_STIP_SHIFT)
#define FB_STIP_MASK	(FB_STIP_UNIT - 1)
#define FB_STIP_ALLONES	((uint32_t) -1)

#if BITMAP_BIT_ORDER == LSBFirst
#define FbScrLeft(x,n)	((x) >> (n))
#define FbScrRight(x,n)	((x) << (n))
#define FbLeftStipBits(x,n) ((x) & ((((uint32_t) 1) << (n)) - 1))
#else
#define FbScrLeft(x,n)	((x) << (n))
#define FbScrRight(x,n)	((x) >> (n))
#define FbLeftStipBits(x,n) ((x) >> (FB_STIP_UNIT - (n)))
#endif

#define FbStipLeft(x,n)	FbScrLeft(x,n)
#define FbStipRight(x,n) FbScrRight(x,n)
#define FbStipMask(x,w)	(FbStipRight(FB_STIP_ALLONES,(x) & FB_STIP_MASK) & \
			 FbStipLeft(FB_STIP_ALLONES,(FB_STIP_UNIT - ((x)+(w))) & FB_STIP_MASK))

#define FbLeftMask(x)       ( ((x) & FB_MASK) ? \
			      FbScrRight(FB_ALLONES,(x) & FB_MASK) : 0)
#define FbRightMask(x)      ( ((FB_UNIT - (x)) & FB_MASK) ? \
			      FbScrLeft(FB_ALLONES,(FB_UNIT - (x)) & FB_MASK) : 0)

#define FbMaskBits(x,w,l,n,r) {						\
	n = (w); \
	r = FbRightMask((x)+n); \
	l = FbLeftMask(x); \
	if (l) { \
	    n -= FB_UNIT - ((x) & FB_MASK); \
	    if (n < 0) { \
		n = 0; \
		l &= r; \
		r = 0; \
	    } \
	} \
	n >>= FB_SHIFT; \
    }

#if IMAGE_BYTE_ORDER == MSBFirst
#define Fetch24(img, a)  ((unsigned long) (a) & 1 ?	      \
    ((READ(img, a) << 16) | READ(img, (uint16_t *) ((a)+1))) : \
    ((READ(img, (uint16_t *) (a)) << 8) | READ(img, (a)+2)))
#define Store24(img,a,v) ((unsigned long) (a) & 1 ? \
    (WRITE(img, a, (uint8_t) ((v) >> 16)),		  \
     WRITE(img, (uint16_t *) ((a)+1), (uint16_t) (v))) :  \
    (WRITE(img, (uint16_t *) (a), (uint16_t) ((v) >> 8)), \
     WRITE(img, (a)+2, (uint8_t) (v))))
#else
#define Fetch24(img,a)  ((unsigned long) (a) & 1 ?			     \
    (READ(img, a) | (READ(img, (uint16_t *) ((a)+1)) << 8)) : \
    (READ(img, (uint16_t *) (a)) | (READ(img, (a)+2) << 16)))
#define Store24(img,a,v) ((unsigned long) (a) & 1 ? \
    (WRITE(img, a, (uint8_t) (v)),				\
     WRITE(img, (uint16_t *) ((a)+1), (uint16_t) ((v) >> 8))) : \
    (WRITE(img, (uint16_t *) (a), (uint16_t) (v)),		\
     WRITE(img, (a)+2, (uint8_t) ((v) >> 16))))
#endif

#define CvtR8G8B8toY15(s)       (((((s) >> 16) & 0xff) * 153 + \
                                  (((s) >>  8) & 0xff) * 301 +		\
                                  (((s)      ) & 0xff) * 58) >> 2)
#define miCvtR8G8B8to15(s) ((((s) >> 3) & 0x001f) |  \
			    (((s) >> 6) & 0x03e0) |  \
			    (((s) >> 9) & 0x7c00))
#define miIndexToEnt15(mif,rgb15) ((mif)->ent[rgb15])
#define miIndexToEnt24(mif,rgb24) miIndexToEnt15(mif,miCvtR8G8B8to15(rgb24))

#define miIndexToEntY24(mif,rgb24) ((mif)->ent[CvtR8G8B8toY15(rgb24)])


#define FbIntMult(a,b,t) ( (t) = (a) * (b) + 0x80, ( ( ( (t)>>8 ) + (t) )>>8 ) )
#define FbIntDiv(a,b)	 (((uint16_t) (a) * 255) / (b))

#define FbGet8(v,i)   ((uint16_t) (uint8_t) ((v) >> i))


#define cvt8888to0565(s)    ((((s) >> 3) & 0x001f) | \
			     (((s) >> 5) & 0x07e0) | \
			     (((s) >> 8) & 0xf800))
#define cvt0565to0888(s)    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) | \
			     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) | \
			     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

/*
 * There are two ways of handling alpha -- either as a single unified value or
 * a separate value for each component, hence each macro must have two
 * versions.  The unified alpha version has a 'U' at the end of the name,
 * the component version has a 'C'.  Similarly, functions which deal with
 * this difference will have two versions using the same convention.
 */

#define FbOverU(x,y,i,a,t) ((t) = FbIntMult(FbGet8(y,i),(a),(t)) + FbGet8(x,i),	\
			    (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

#define FbOverC(x,y,i,a,t) ((t) = FbIntMult(FbGet8(y,i),FbGet8(a,i),(t)) + FbGet8(x,i),	\
			    (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

#define FbInU(x,i,a,t) ((uint32_t) FbIntMult(FbGet8(x,i),(a),(t)) << (i))

#define FbInC(x,i,a,t) ((uint32_t) FbIntMult(FbGet8(x,i),FbGet8(a,i),(t)) << (i))

#define FbAdd(x,y,i,t)	((t) = FbGet8(x,i) + FbGet8(y,i),		\
			 (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

#define div_255(x) (((x) + 0x80 + (((x) + 0x80) >> 8)) >> 8)
#define div_65535(x) (((x) + 0x8000 + (((x) + 0x8000) >> 16)) >> 16)

#define MOD(a,b) ((a) < 0 ? ((b) - ((-(a) - 1) % (b))) - 1 : (a) % (b))

#define DIV(a,b) ((((a) < 0) == ((b) < 0)) ? (a) / (b) :		\
		  ((a) - (b) + 1 - (((b) < 0) << 1)) / (b))

#define CLIP(a,b,c) ((a) < (b) ? (b) : ((a) > (c) ? (c) : (a)))

#if 0
/* FIXME: the MOD macro above is equivalent, but faster I think */
#define mod(a,b) ((b) == 1 ? 0 : (a) >= 0 ? (a) % (b) : (b) - (-a) % (b))
#endif

/* FIXME: the (void)__read_func hides lots of warnings (which is what they
 * are supposed to do), but some of them are real. For example the one
 * where Fetch4 doesn't have a READ
 */

#if 0
/* Framebuffer access support macros */
#define ACCESS_MEM(code)						\
    do {								\
	const image_common_t *const com__ =				\
	    (image_common_t *)image;					\
									\
	if (!com__->read_func && !com__->write_func)			\
	{								\
	    const int do_access__ = 0;					\
	    const pixman_read_memory_func_t read_func__ = NULL;		\
	    const pixman_write_memory_func_t write_func__ = NULL;	\
	    (void)read_func__;						\
	    (void)write_func__;						\
	    (void)do_access__;						\
									\
	    {code}							\
	}								\
	else								\
	{								\
	    const int do_access__ = 1;					\
	    const pixman_read_memory_func_t read_func__ =		\
		com__->read_func;					\
	    const pixman_write_memory_func_t write_func__ =		\
		com__->write_func;					\
	    (void)read_func__;						\
	    (void)write_func__;						\
	    (void)do_access__;						\
	    								\
	    {code}							\
	}								\
    } while (0)
#endif

#ifdef PIXMAN_FB_ACCESSORS

#define ACCESS(sym) sym##_accessors

#define READ(img, ptr)							\
    ((img)->common.read_func ((ptr), sizeof(*(ptr))))
#define WRITE(img, ptr,val)						\
    ((img)->common.write_func ((ptr), (val), sizeof (*(ptr))))

#define MEMCPY_WRAPPED(img, dst, src, size)				\
    do {								\
	size_t _i;							\
	uint8_t *_dst = (uint8_t*)(dst), *_src = (uint8_t*)(src);	\
	for(_i = 0; _i < size; _i++) {					\
	    WRITE((img), _dst +_i, READ((img), _src + _i));		\
	}								\
    } while (0)

#define MEMSET_WRAPPED(img, dst, val, size)				\
    do {								\
	size_t _i;							\
	uint8_t *_dst = (uint8_t*)(dst);				\
	for(_i = 0; _i < (size_t) size; _i++) {				\
	    WRITE((img), _dst +_i, (val));				\
	}								\
    } while (0)

#else

#define ACCESS(sym) sym

#define READ(img, ptr)		(*(ptr))
#define WRITE(img, ptr, val)	(*(ptr) = (val))
#define MEMCPY_WRAPPED(img, dst, src, size)					\
    memcpy(dst, src, size)
#define MEMSET_WRAPPED(img, dst, val, size)					\
    memset(dst, val, size)

#endif

#define fbComposeGetSolid(img, res, fmt)				\
    do									\
    {									\
	pixman_format_code_t format__;					\
	if (img->type == SOLID)						\
	{								\
	    format__ = PIXMAN_a8r8g8b8;					\
	    (res) = img->solid.color;					\
	}								\
	else								\
	{								\
	    uint32_t	       *bits__   = (img)->bits.bits;		\
	    format__ = (img)->bits.format;				\
		  							\
	    switch (PIXMAN_FORMAT_BPP((img)->bits.format))		\
	    {								\
	    case 32:							\
		(res) = READ(img, (uint32_t *)bits__);			\
		break;							\
	    case 24:							\
		(res) = Fetch24(img, (uint8_t *) bits__);			\
		break;							\
	    case 16:							\
		(res) = READ(img, (uint16_t *) bits__);			\
		(res) = cvt0565to0888(res);				\
		break;							\
	    case 8:							\
		(res) = READ(img, (uint8_t *) bits__);			\
		(res) = (res) << 24;					\
		break;							\
	    case 1:							\
		(res) = READ(img, (uint32_t *) bits__);			\
		(res) = FbLeftStipBits((res),1) ? 0xff000000 : 0x00000000; \
		break;							\
	    default:							\
		return;							\
	    }								\
	    /* manage missing src alpha */				\
	    if (!PIXMAN_FORMAT_A((img)->bits.format))			\
		(res) |= 0xff000000;					\
	}								\
									\
	/* If necessary, convert RGB <--> BGR. */			\
	if (PIXMAN_FORMAT_TYPE (format__) != PIXMAN_FORMAT_TYPE(fmt))	\
	{								\
	    (res) = ((((res) & 0xff000000) >>  0) |			\
		     (((res) & 0x00ff0000) >> 16) |			\
		     (((res) & 0x0000ff00) >>  0) |			\
		     (((res) & 0x000000ff) << 16));			\
	}								\
    }									\
    while (0)

#define fbComposeGetStart(pict,x,y,type,out_stride,line,mul) do {	\
	uint32_t	*__bits__;					\
	int		__stride__;					\
	int		__bpp__;					\
									\
	__bits__ = pict->bits.bits;					\
	__stride__ = pict->bits.rowstride;				\
	__bpp__ = PIXMAN_FORMAT_BPP(pict->bits.format);			\
	(out_stride) = __stride__ * (int) sizeof (uint32_t) / (int) sizeof (type);	\
	(line) = ((type *) __bits__) +					\
	    (out_stride) * (y) + (mul) * (x);				\
    } while (0)


#define PIXMAN_FORMAT_16BPC(f)	(PIXMAN_FORMAT_A(f) > 8 || \
				 PIXMAN_FORMAT_R(f) > 8 || \
				 PIXMAN_FORMAT_G(f) > 8 || \
				 PIXMAN_FORMAT_B(f) > 8)
/*
 * Edges
 */

#define MAX_ALPHA(n)	((1 << (n)) - 1)
#define N_Y_FRAC(n)	((n) == 1 ? 1 : (1 << ((n)/2)) - 1)
#define N_X_FRAC(n)	((n) == 1 ? 1 : (1 << ((n)/2)) + 1)

#define STEP_Y_SMALL(n)	(pixman_fixed_1 / N_Y_FRAC(n))
#define STEP_Y_BIG(n)	(pixman_fixed_1 - (N_Y_FRAC(n) - 1) * STEP_Y_SMALL(n))

#define Y_FRAC_FIRST(n)	(STEP_Y_SMALL(n) / 2)
#define Y_FRAC_LAST(n)	(Y_FRAC_FIRST(n) + (N_Y_FRAC(n) - 1) * STEP_Y_SMALL(n))

#define STEP_X_SMALL(n)	(pixman_fixed_1 / N_X_FRAC(n))
#define STEP_X_BIG(n)	(pixman_fixed_1 - (N_X_FRAC(n) - 1) * STEP_X_SMALL(n))

#define X_FRAC_FIRST(n)	(STEP_X_SMALL(n) / 2)
#define X_FRAC_LAST(n)	(X_FRAC_FIRST(n) + (N_X_FRAC(n) - 1) * STEP_X_SMALL(n))

#define RenderSamplesX(x,n)	((n) == 1 ? 0 : (pixman_fixed_frac (x) + X_FRAC_FIRST(n)) / STEP_X_SMALL(n))

/*
 * Step across a small sample grid gap
 */
#define RenderEdgeStepSmall(edge) { \
    edge->x += edge->stepx_small;   \
    edge->e += edge->dx_small;	    \
    if (edge->e > 0)		    \
    {				    \
	edge->e -= edge->dy;	    \
	edge->x += edge->signdx;    \
    }				    \
}

/*
 * Step across a large sample grid gap
 */
#define RenderEdgeStepBig(edge) {   \
    edge->x += edge->stepx_big;	    \
    edge->e += edge->dx_big;	    \
    if (edge->e > 0)		    \
    {				    \
	edge->e -= edge->dy;	    \
	edge->x += edge->signdx;    \
    }				    \
}

void
pixman_rasterize_edges_accessors (pixman_image_t *image,
				  pixman_edge_t	*l,
				  pixman_edge_t	*r,
				  pixman_fixed_t	t,
				  pixman_fixed_t	b);

pixman_bool_t
pixman_image_is_opaque(pixman_image_t *image);

pixman_bool_t
pixman_image_can_get_solid (pixman_image_t *image);

pixman_bool_t
pixman_compute_composite_region32 (pixman_region32_t *	pRegion,
				   pixman_image_t *	pSrc,
				   pixman_image_t *	pMask,
				   pixman_image_t *	pDst,
				   int16_t		xSrc,
				   int16_t		ySrc,
				   int16_t		xMask,
				   int16_t		yMask,
				   int16_t		xDst,
				   int16_t		yDst,
				   uint16_t		width,
				   uint16_t		height);

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4
#define PIXMAN_EXPORT __attribute__ ((visibility("default")))
/* Sun Studio 8 visibility */
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
#define PIXMAN_EXPORT __global
#else
#define PIXMAN_EXPORT
#endif

/* Region Helpers */
pixman_bool_t pixman_region32_copy_from_region16 (pixman_region32_t *dst,
						  pixman_region16_t *src);
pixman_bool_t pixman_region16_copy_from_region32 (pixman_region16_t *dst,
						  pixman_region32_t *src);
void pixman_region_internal_set_static_pointers (pixman_box16_t *empty_box,
						 pixman_region16_data_t *empty_data,
						 pixman_region16_data_t *broken_data);

#ifdef PIXMAN_TIMING

/* Timing */
static inline uint64_t
oil_profile_stamp_rdtsc (void)
{
    uint64_t ts;
    __asm__ __volatile__("rdtsc\n" : "=A" (ts));
    return ts;
}
#define OIL_STAMP oil_profile_stamp_rdtsc

typedef struct PixmanTimer PixmanTimer;

struct PixmanTimer
{
    int initialized;
    const char *name;
    uint64_t n_times;
    uint64_t total;
    PixmanTimer *next;
};

extern int timer_defined;
void pixman_timer_register (PixmanTimer *timer);

#define TIMER_BEGIN(tname)						\
    {									\
	static PixmanTimer	timer##tname;				\
	uint64_t		begin##tname;				\
									\
	if (!timer##tname.initialized)					\
	{								\
	    timer##tname.initialized = 1;				\
	    timer##tname.name = #tname;					\
	    pixman_timer_register (&timer##tname);			\
	}								\
									\
	timer##tname.n_times++;						\
	begin##tname = OIL_STAMP();

#define TIMER_END(tname)						\
        timer##tname.total += OIL_STAMP() - begin##tname;		\
    }

#endif /* PIXMAN_TIMING */

#endif /* PIXMAN_PRIVATE_H */

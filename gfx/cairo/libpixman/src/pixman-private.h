#ifndef PACKAGE
#  error config.h must be included before pixman-private.h
#endif

#ifndef PIXMAN_PRIVATE_H
#define PIXMAN_PRIVATE_H

#define PIXMAN_DISABLE_DEPRECATED
#define PIXMAN_USE_INTERNAL_API

#include "pixman.h"
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pixman-compiler.h"

/*
 * Images
 */
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

typedef void (*fetch_scanline_t) (pixman_image_t *image,
				  int             x,
				  int             y,
				  int             width,
				  uint32_t       *buffer,
				  const uint32_t *mask,
				  uint32_t        mask_bits);

typedef uint32_t (*fetch_pixel_32_t) (bits_image_t *image,
				      int           x,
				      int           y);

typedef uint64_t (*fetch_pixel_64_t) (bits_image_t *image,
				      int           x,
				      int           y);

typedef void (*store_scanline_t) (bits_image_t *  image,
				  int             x,
				  int             y,
				  int             width,
				  const uint32_t *values);

typedef enum
{
    BITS,
    LINEAR,
    CONICAL,
    RADIAL,
    SOLID
} image_type_t;

typedef enum
{
    SOURCE_IMAGE_CLASS_UNKNOWN,
    SOURCE_IMAGE_CLASS_HORIZONTAL,
    SOURCE_IMAGE_CLASS_VERTICAL,
} source_image_class_t;

typedef source_image_class_t (*classify_func_t) (pixman_image_t *image,
						int             x,
						int             y,
						int             width,
						int             height);
typedef void (*property_changed_func_t) (pixman_image_t *image);

struct image_common
{
    image_type_t                type;
    int32_t                     ref_count;
    pixman_region32_t           clip_region;
    pixman_bool_t               have_clip_region;   /* FALSE if there is no clip */
    pixman_bool_t               client_clip;        /* Whether the source clip was
						       set by a client */
    pixman_bool_t               clip_sources;       /* Whether the clip applies when
						     * the image is used as a source
						     */
    pixman_bool_t		dirty;
    pixman_transform_t *        transform;
    pixman_repeat_t             repeat;
    pixman_filter_t             filter;
    pixman_fixed_t *            filter_params;
    int                         n_filter_params;
    bits_image_t *              alpha_map;
    int                         alpha_origin_x;
    int                         alpha_origin_y;
    pixman_bool_t               component_alpha;
    classify_func_t             classify;
    property_changed_func_t     property_changed;
    fetch_scanline_t            get_scanline_32;
    fetch_scanline_t            get_scanline_64;

    pixman_image_destroy_func_t destroy_func;
    void *                      destroy_data;

    uint32_t			flags;
    pixman_format_code_t	extended_format_code;
};

struct source_image
{
    image_common_t common;
    source_image_class_t class;
};

struct solid_fill
{
    source_image_t common;
    pixman_color_t color;
    
    uint32_t	   color_32;
    uint64_t	   color_64;
};

struct gradient
{
    source_image_t          common;
    int                     n_stops;
    pixman_gradient_stop_t *stops;
    int                     stop_range;
};

struct linear_gradient
{
    gradient_t           common;
    pixman_point_fixed_t p1;
    pixman_point_fixed_t p2;
};

struct circle
{
    pixman_fixed_t x;
    pixman_fixed_t y;
    pixman_fixed_t radius;
};

struct radial_gradient
{
    gradient_t common;

    circle_t   c1;
    circle_t   c2;
    double     cdx;
    double     cdy;
    double     dr;
    double     A;
};

struct conical_gradient
{
    gradient_t           common;
    pixman_point_fixed_t center;
    pixman_fixed_t       angle;
};

struct bits_image
{
    image_common_t             common;
    pixman_format_code_t       format;
    const pixman_indexed_t *   indexed;
    int                        width;
    int                        height;
    uint32_t *                 bits;
    uint32_t *                 free_me;
    int                        rowstride;  /* in number of uint32_t's */

    /* Fetch a pixel, disregarding alpha maps, transformations etc. */
    fetch_pixel_32_t	       fetch_pixel_raw_32;
    fetch_pixel_64_t	       fetch_pixel_raw_64;

    /* Fetch a pixel, taking alpha maps into account */
    fetch_pixel_32_t	       fetch_pixel_32;
    fetch_pixel_64_t	       fetch_pixel_64;

    /* Fetch raw scanlines, with no regard for transformations, alpha maps etc. */
    fetch_scanline_t           fetch_scanline_raw_32;
    fetch_scanline_t           fetch_scanline_raw_64;

    /* Store scanlines with no regard for alpha maps */
    store_scanline_t           store_scanline_raw_32;
    store_scanline_t           store_scanline_raw_64;

    /* Store a scanline, taking alpha maps into account */
    store_scanline_t           store_scanline_32;
    store_scanline_t           store_scanline_64;

    /* Used for indirect access to the bits */
    pixman_read_memory_func_t  read_func;
    pixman_write_memory_func_t write_func;
};

union pixman_image
{
    image_type_t       type;
    image_common_t     common;
    bits_image_t       bits;
    source_image_t     source;
    gradient_t         gradient;
    linear_gradient_t  linear;
    conical_gradient_t conical;
    radial_gradient_t  radial;
    solid_fill_t       solid;
};


void
_pixman_bits_image_setup_raw_accessors (bits_image_t *image);

void
_pixman_image_get_scanline_generic_64  (pixman_image_t *image,
                                        int             x,
                                        int             y,
                                        int             width,
                                        uint32_t *      buffer,
                                        const uint32_t *mask,
                                        uint32_t        mask_bits);

source_image_class_t
_pixman_image_classify (pixman_image_t *image,
                        int             x,
                        int             y,
                        int             width,
                        int             height);

void
_pixman_image_get_scanline_32 (pixman_image_t *image,
                               int             x,
                               int             y,
                               int             width,
                               uint32_t *      buffer,
                               const uint32_t *mask,
                               uint32_t        mask_bits);

/* Even thought the type of buffer is uint32_t *, the function actually expects
 * a uint64_t *buffer.
 */
void
_pixman_image_get_scanline_64 (pixman_image_t *image,
                               int             x,
                               int             y,
                               int             width,
                               uint32_t *      buffer,
                               const uint32_t *unused,
                               uint32_t        unused2);

void
_pixman_image_store_scanline_32 (bits_image_t *  image,
                                 int             x,
                                 int             y,
                                 int             width,
                                 const uint32_t *buffer);

/* Even though the type of buffer is uint32_t *, the function
 * actually expects a uint64_t *buffer.
 */
void
_pixman_image_store_scanline_64 (bits_image_t *  image,
                                 int             x,
                                 int             y,
                                 int             width,
                                 const uint32_t *buffer);

pixman_image_t *
_pixman_image_allocate (void);

pixman_bool_t
_pixman_init_gradient (gradient_t *                  gradient,
                       const pixman_gradient_stop_t *stops,
                       int                           n_stops);
void
_pixman_image_reset_clip_region (pixman_image_t *image);

void
_pixman_image_validate (pixman_image_t *image);

uint32_t
_pixman_image_get_solid (pixman_image_t *     image,
                         pixman_format_code_t format);

#define PIXMAN_IMAGE_GET_LINE(image, x, y, type, out_stride, line, mul)	\
    do									\
    {									\
	uint32_t *__bits__;						\
	int       __stride__;						\
        								\
	__bits__ = image->bits.bits;					\
	__stride__ = image->bits.rowstride;				\
	(out_stride) =							\
	    __stride__ * (int) sizeof (uint32_t) / (int) sizeof (type);	\
	(line) =							\
	    ((type *) __bits__) + (out_stride) * (y) + (mul) * (x);	\
    } while (0)

/*
 * Gradient walker
 */
typedef struct
{
    uint32_t                left_ag;
    uint32_t                left_rb;
    uint32_t                right_ag;
    uint32_t                right_rb;
    int32_t                 left_x;
    int32_t                 right_x;
    int32_t                 stepper;

    pixman_gradient_stop_t *stops;
    int                     num_stops;
    unsigned int            spread;

    int                     need_reset;
} pixman_gradient_walker_t;

void
_pixman_gradient_walker_init (pixman_gradient_walker_t *walker,
                              gradient_t *              gradient,
                              unsigned int              spread);

void
_pixman_gradient_walker_reset (pixman_gradient_walker_t *walker,
                               pixman_fixed_32_32_t      pos);

uint32_t
_pixman_gradient_walker_pixel (pixman_gradient_walker_t *walker,
                               pixman_fixed_32_32_t      x);

/*
 * Edges
 */

#define MAX_ALPHA(n)    ((1 << (n)) - 1)
#define N_Y_FRAC(n)     ((n) == 1 ? 1 : (1 << ((n) / 2)) - 1)
#define N_X_FRAC(n)     ((n) == 1 ? 1 : (1 << ((n) / 2)) + 1)

#define STEP_Y_SMALL(n) (pixman_fixed_1 / N_Y_FRAC (n))
#define STEP_Y_BIG(n)   (pixman_fixed_1 - (N_Y_FRAC (n) - 1) * STEP_Y_SMALL (n))

#define Y_FRAC_FIRST(n) (STEP_Y_BIG (n) / 2)
#define Y_FRAC_LAST(n)  (Y_FRAC_FIRST (n) + (N_Y_FRAC (n) - 1) * STEP_Y_SMALL (n))

#define STEP_X_SMALL(n) (pixman_fixed_1 / N_X_FRAC (n))
#define STEP_X_BIG(n)   (pixman_fixed_1 - (N_X_FRAC (n) - 1) * STEP_X_SMALL (n))

#define X_FRAC_FIRST(n) (STEP_X_BIG (n) / 2)
#define X_FRAC_LAST(n)  (X_FRAC_FIRST (n) + (N_X_FRAC (n) - 1) * STEP_X_SMALL (n))

#define RENDER_SAMPLES_X(x, n)						\
    ((n) == 1? 0 : (pixman_fixed_frac (x) +				\
		    X_FRAC_FIRST (n)) / STEP_X_SMALL (n))

void
pixman_rasterize_edges_accessors (pixman_image_t *image,
                                  pixman_edge_t * l,
                                  pixman_edge_t * r,
                                  pixman_fixed_t  t,
                                  pixman_fixed_t  b);

/*
 * Implementations
 */
typedef struct pixman_implementation_t pixman_implementation_t;

typedef void (*pixman_combine_32_func_t) (pixman_implementation_t *imp,
					  pixman_op_t              op,
					  uint32_t *               dest,
					  const uint32_t *         src,
					  const uint32_t *         mask,
					  int                      width);

typedef void (*pixman_combine_64_func_t) (pixman_implementation_t *imp,
					  pixman_op_t              op,
					  uint64_t *               dest,
					  const uint64_t *         src,
					  const uint64_t *         mask,
					  int                      width);

typedef void (*pixman_composite_func_t) (pixman_implementation_t *imp,
					 pixman_op_t              op,
					 pixman_image_t *         src,
					 pixman_image_t *         mask,
					 pixman_image_t *         dest,
					 int32_t                  src_x,
					 int32_t                  src_y,
					 int32_t                  mask_x,
					 int32_t                  mask_y,
					 int32_t                  dest_x,
					 int32_t                  dest_y,
					 int32_t                  width,
					 int32_t                  height);
typedef pixman_bool_t (*pixman_blt_func_t) (pixman_implementation_t *imp,
					    uint32_t *               src_bits,
					    uint32_t *               dst_bits,
					    int                      src_stride,
					    int                      dst_stride,
					    int                      src_bpp,
					    int                      dst_bpp,
					    int                      src_x,
					    int                      src_y,
					    int                      dst_x,
					    int                      dst_y,
					    int                      width,
					    int                      height);
typedef pixman_bool_t (*pixman_fill_func_t) (pixman_implementation_t *imp,
					     uint32_t *               bits,
					     int                      stride,
					     int                      bpp,
					     int                      x,
					     int                      y,
					     int                      width,
					     int                      height,
					     uint32_t                 xor);

void _pixman_setup_combiner_functions_32 (pixman_implementation_t *imp);
void _pixman_setup_combiner_functions_64 (pixman_implementation_t *imp);

typedef struct
{
    pixman_op_t             op;
    pixman_format_code_t    src_format;
    uint32_t		    src_flags;
    pixman_format_code_t    mask_format;
    uint32_t		    mask_flags;
    pixman_format_code_t    dest_format;
    uint32_t		    dest_flags;
    pixman_composite_func_t func;
} pixman_fast_path_t;

struct pixman_implementation_t
{
    pixman_implementation_t *	toplevel;
    pixman_implementation_t *	delegate;
    const pixman_fast_path_t *	fast_paths;
    
    pixman_blt_func_t		blt;
    pixman_fill_func_t		fill;

    pixman_combine_32_func_t	combine_32[PIXMAN_N_OPERATORS];
    pixman_combine_32_func_t	combine_32_ca[PIXMAN_N_OPERATORS];
    pixman_combine_64_func_t	combine_64[PIXMAN_N_OPERATORS];
    pixman_combine_64_func_t	combine_64_ca[PIXMAN_N_OPERATORS];
};

pixman_implementation_t *
_pixman_implementation_create (pixman_implementation_t *delegate,
			       const pixman_fast_path_t *fast_paths);

void
_pixman_implementation_combine_32 (pixman_implementation_t *imp,
                                   pixman_op_t              op,
                                   uint32_t *               dest,
                                   const uint32_t *         src,
                                   const uint32_t *         mask,
                                   int                      width);
void
_pixman_implementation_combine_64 (pixman_implementation_t *imp,
                                   pixman_op_t              op,
                                   uint64_t *               dest,
                                   const uint64_t *         src,
                                   const uint64_t *         mask,
                                   int                      width);
void
_pixman_implementation_combine_32_ca (pixman_implementation_t *imp,
                                      pixman_op_t              op,
                                      uint32_t *               dest,
                                      const uint32_t *         src,
                                      const uint32_t *         mask,
                                      int                      width);
void
_pixman_implementation_combine_64_ca (pixman_implementation_t *imp,
                                      pixman_op_t              op,
                                      uint64_t *               dest,
                                      const uint64_t *         src,
                                      const uint64_t *         mask,
                                      int                      width);

pixman_bool_t
_pixman_implementation_blt (pixman_implementation_t *imp,
                            uint32_t *               src_bits,
                            uint32_t *               dst_bits,
                            int                      src_stride,
                            int                      dst_stride,
                            int                      src_bpp,
                            int                      dst_bpp,
                            int                      src_x,
                            int                      src_y,
                            int                      dst_x,
                            int                      dst_y,
                            int                      width,
                            int                      height);

pixman_bool_t
_pixman_implementation_fill (pixman_implementation_t *imp,
                             uint32_t *               bits,
                             int                      stride,
                             int                      bpp,
                             int                      x,
                             int                      y,
                             int                      width,
                             int                      height,
                             uint32_t                 xor);

/* Specific implementations */
pixman_implementation_t *
_pixman_implementation_create_general (void);

pixman_implementation_t *
_pixman_implementation_create_fast_path (void);

#ifdef USE_MMX
pixman_implementation_t *
_pixman_implementation_create_mmx (void);
#endif

#ifdef USE_SSE2
pixman_implementation_t *
_pixman_implementation_create_sse2 (void);
#endif

#ifdef USE_ARM_SIMD
pixman_implementation_t *
_pixman_implementation_create_arm_simd (void);
#endif

#ifdef USE_ARM_NEON
pixman_implementation_t *
_pixman_implementation_create_arm_neon (void);
#endif

#ifdef USE_VMX
pixman_implementation_t *
_pixman_implementation_create_vmx (void);
#endif

pixman_implementation_t *
_pixman_choose_implementation (void);



/*
 * Utilities
 */

/* These "formats" all have depth 0, so they
 * will never clash with any real ones
 */
#define PIXMAN_null             PIXMAN_FORMAT (0, 0, 0, 0, 0, 0)
#define PIXMAN_solid            PIXMAN_FORMAT (0, 1, 0, 0, 0, 0)
#define PIXMAN_pixbuf		PIXMAN_FORMAT (0, 2, 0, 0, 0, 0)
#define PIXMAN_rpixbuf		PIXMAN_FORMAT (0, 3, 0, 0, 0, 0)
#define PIXMAN_unknown		PIXMAN_FORMAT (0, 4, 0, 0, 0, 0)
#define PIXMAN_any		PIXMAN_FORMAT (0, 5, 0, 0, 0, 0)

#define PIXMAN_OP_any		(PIXMAN_N_OPERATORS + 1)

#define FAST_PATH_ID_TRANSFORM			(1 <<  0)
#define FAST_PATH_NO_ALPHA_MAP			(1 <<  1)
#define FAST_PATH_NO_CONVOLUTION_FILTER		(1 <<  2)
#define FAST_PATH_NO_PAD_REPEAT			(1 <<  3)
#define FAST_PATH_NO_REFLECT_REPEAT		(1 <<  4)
#define FAST_PATH_NO_ACCESSORS			(1 <<  5)
#define FAST_PATH_NO_WIDE_FORMAT		(1 <<  6)
#define FAST_PATH_COVERS_CLIP			(1 <<  7)
#define FAST_PATH_COMPONENT_ALPHA		(1 <<  8)
#define FAST_PATH_UNIFIED_ALPHA			(1 <<  9)
#define FAST_PATH_SCALE_TRANSFORM		(1 << 10)
#define FAST_PATH_NEAREST_FILTER		(1 << 11)
#define FAST_PATH_SIMPLE_REPEAT			(1 << 12)
#define FAST_PATH_IS_OPAQUE			(1 << 13)
#define FAST_PATH_NEEDS_WORKAROUND		(1 << 14)
#define FAST_PATH_NO_NONE_REPEAT		(1 << 15)
#define FAST_PATH_SAMPLES_COVER_CLIP		(1 << 16)
#define FAST_PATH_16BIT_SAFE			(1 << 17)
#define FAST_PATH_X_UNIT_POSITIVE		(1 << 18)

#define _FAST_PATH_STANDARD_FLAGS					\
    (FAST_PATH_ID_TRANSFORM		|				\
     FAST_PATH_NO_ALPHA_MAP		|				\
     FAST_PATH_NO_CONVOLUTION_FILTER	|				\
     FAST_PATH_NO_PAD_REPEAT		|				\
     FAST_PATH_NO_REFLECT_REPEAT	|				\
     FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_NO_WIDE_FORMAT		|				\
     FAST_PATH_COVERS_CLIP)

#define FAST_PATH_STD_SRC_FLAGS						\
    _FAST_PATH_STANDARD_FLAGS
#define FAST_PATH_STD_MASK_U_FLAGS					\
    (_FAST_PATH_STANDARD_FLAGS		|				\
     FAST_PATH_UNIFIED_ALPHA)
#define FAST_PATH_STD_MASK_CA_FLAGS					\
    (_FAST_PATH_STANDARD_FLAGS		|				\
     FAST_PATH_COMPONENT_ALPHA)
#define FAST_PATH_STD_DEST_FLAGS					\
    (FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_NO_WIDE_FORMAT)

#define FAST_PATH(op, src, src_flags, mask, mask_flags, dest, dest_flags, func) \
    PIXMAN_OP_ ## op,							\
    PIXMAN_ ## src,							\
    src_flags,							        \
    PIXMAN_ ## mask,						        \
    mask_flags,							        \
    PIXMAN_ ## dest,	                                                \
    dest_flags,							        \
    func

#define PIXMAN_STD_FAST_PATH(op, src, mask, dest, func)			\
    { FAST_PATH (							\
	  op,								\
	  src, FAST_PATH_STD_SRC_FLAGS,					\
	  mask, (PIXMAN_ ## mask) ? FAST_PATH_STD_MASK_U_FLAGS : 0,	\
	  dest, FAST_PATH_STD_DEST_FLAGS,				\
	  func) }

#define PIXMAN_STD_FAST_PATH_CA(op, src, mask, dest, func)		\
    { FAST_PATH (							\
	  op,								\
	  src, FAST_PATH_STD_SRC_FLAGS,					\
	  mask, FAST_PATH_STD_MASK_CA_FLAGS,				\
	  dest, FAST_PATH_STD_DEST_FLAGS,				\
	  func) }

/* Memory allocation helpers */
void *
pixman_malloc_ab (unsigned int n, unsigned int b);

void *
pixman_malloc_abc (unsigned int a, unsigned int b, unsigned int c);

pixman_bool_t
pixman_multiply_overflows_int (unsigned int a, unsigned int b);

pixman_bool_t
pixman_addition_overflows_int (unsigned int a, unsigned int b);

/* Compositing utilities */
void
pixman_expand (uint64_t *           dst,
               const uint32_t *     src,
               pixman_format_code_t format,
               int                  width);

void
pixman_contract (uint32_t *      dst,
                 const uint64_t *src,
                 int             width);


/* Region Helpers */
pixman_bool_t
pixman_region32_copy_from_region16 (pixman_region32_t *dst,
                                    pixman_region16_t *src);

pixman_bool_t
pixman_region16_copy_from_region32 (pixman_region16_t *dst,
                                    pixman_region32_t *src);


/* Misc macros */

#ifndef FALSE
#   define FALSE 0
#endif

#ifndef TRUE
#   define TRUE 1
#endif

#ifndef MIN
#  define MIN(a, b) ((a < b) ? a : b)
#endif

#ifndef MAX
#  define MAX(a, b) ((a > b) ? a : b)
#endif

/* Integer division that rounds towards -infinity */
#define DIV(a, b)					   \
    ((((a) < 0) == ((b) < 0)) ? (a) / (b) :                \
     ((a) - (b) + 1 - (((b) < 0) << 1)) / (b))

/* Modulus that produces the remainder wrt. DIV */
#define MOD(a, b) ((a) < 0 ? ((b) - ((-(a) - 1) % (b))) - 1 : (a) % (b))

#define CLIP(v, low, high) ((v) < (low) ? (low) : ((v) > (high) ? (high) : (v)))

/* Conversion between 8888 and 0565 */

#define CONVERT_8888_TO_0565(s)						\
    ((((s) >> 3) & 0x001f) |						\
     (((s) >> 5) & 0x07e0) |						\
     (((s) >> 8) & 0xf800))

#define CONVERT_0565_TO_0888(s)						\
    (((((s) << 3) & 0xf8) | (((s) >> 2) & 0x7)) |			\
     ((((s) << 5) & 0xfc00) | (((s) >> 1) & 0x300)) |			\
     ((((s) << 8) & 0xf80000) | (((s) << 3) & 0x70000)))

#define CONVERT_0565_TO_8888(s) (CONVERT_0565_TO_0888(s) | 0xff000000)

/* Trivial versions that are useful in macros */
#define CONVERT_8888_TO_8888(s) (s)
#define CONVERT_0565_TO_0565(s) (s)

#define PIXMAN_FORMAT_IS_WIDE(f)					\
    (PIXMAN_FORMAT_A (f) > 8 ||						\
     PIXMAN_FORMAT_R (f) > 8 ||						\
     PIXMAN_FORMAT_G (f) > 8 ||						\
     PIXMAN_FORMAT_B (f) > 8)

#ifdef WORDS_BIGENDIAN
#   define SCREEN_SHIFT_LEFT(x,n)	((x) << (n))
#   define SCREEN_SHIFT_RIGHT(x,n)	((x) >> (n))
#else
#   define SCREEN_SHIFT_LEFT(x,n)	((x) >> (n))
#   define SCREEN_SHIFT_RIGHT(x,n)	((x) << (n))
#endif

/*
 * Various debugging code
 */

#undef DEBUG

/* Turn on debugging depending on what type of release this is
 */
#if (((PIXMAN_VERSION_MICRO % 2) == 0) && ((PIXMAN_VERSION_MINOR % 2) == 1))

/* Debugging gets turned on for development releases because these
 * are the things that end up in bleeding edge distributions such
 * as Rawhide etc.
 *
 * For performance reasons we don't turn it on for stable releases or
 * random git checkouts. (Random git checkouts are often used for
 * performance work).
 */

#    define DEBUG

#endif

#ifdef DEBUG

void
_pixman_log_error (const char *function, const char *message);

#define return_if_fail(expr)                                            \
    do                                                                  \
    {                                                                   \
	if (!(expr))							\
	{								\
	    _pixman_log_error (FUNC, "The expression " # expr " was false"); \
	    return;							\
	}								\
    }                                                                   \
    while (0)

#define return_val_if_fail(expr, retval)                                \
    do                                                                  \
    {                                                                   \
	if (!(expr))                                                    \
	{								\
	    _pixman_log_error (FUNC, "The expression " # expr " was false"); \
	    return (retval);						\
	}								\
    }                                                                   \
    while (0)

#define critical_if_fail(expr)						\
    do									\
    {									\
	if (!(expr))							\
	    _pixman_log_error (FUNC, "The expression " # expr " was false"); \
    }									\
    while (0)


#else

#define _pixman_log_error(f,m) do { } while (0)				\

#define return_if_fail(expr)						\
    do                                                                  \
    {                                                                   \
	if (!(expr))							\
	    return;							\
    }                                                                   \
    while (0)

#define return_val_if_fail(expr, retval)                                \
    do                                                                  \
    {                                                                   \
	if (!(expr))							\
	    return (retval);						\
    }                                                                   \
    while (0)

#define critical_if_fail(expr)						\
    do									\
    {									\
    }									\
    while (0)
#endif

/*
 * Timers
 */

#ifdef PIXMAN_TIMERS

static inline uint64_t
oil_profile_stamp_rdtsc (void)
{
    uint64_t ts;

    __asm__ __volatile__ ("rdtsc\n" : "=A" (ts));
    return ts;
}

#define OIL_STAMP oil_profile_stamp_rdtsc

typedef struct pixman_timer_t pixman_timer_t;

struct pixman_timer_t
{
    int             initialized;
    const char *    name;
    uint64_t        n_times;
    uint64_t        total;
    pixman_timer_t *next;
};

extern int timer_defined;

void pixman_timer_register (pixman_timer_t *timer);

#define TIMER_BEGIN(tname)                                              \
    {                                                                   \
	static pixman_timer_t timer ## tname;                           \
	uint64_t              begin ## tname;                           \
        								\
	if (!timer ## tname.initialized)				\
	{                                                               \
	    timer ## tname.initialized = 1;				\
	    timer ## tname.name = # tname;				\
	    pixman_timer_register (&timer ## tname);			\
	}                                                               \
									\
	timer ## tname.n_times++;					\
	begin ## tname = OIL_STAMP ();

#define TIMER_END(tname)                                                \
    timer ## tname.total += OIL_STAMP () - begin ## tname;		\
    }

#endif /* PIXMAN_TIMERS */

#endif /* PIXMAN_PRIVATE_H */

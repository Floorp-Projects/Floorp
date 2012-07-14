#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <string.h>

#include "pixman-private.h"

#include "pixman-combine32.h"

static force_inline uint32_t
combine_mask (const uint32_t src, const uint32_t mask)
{
    uint32_t s, m;

    m = mask >> A_SHIFT;

    if (!m)
	return 0;
    s = src;

    UN8x4_MUL_UN8 (s, m);

    return s;
}

static inline uint32_t convert_0565_to_8888(uint16_t color)
{
    return CONVERT_0565_TO_8888(color);
}

static inline uint16_t convert_8888_to_0565(uint32_t color)
{
    return CONVERT_8888_TO_0565(color);
}

static void
combine_src_u (pixman_implementation_t *imp,
               pixman_op_t              op,
               uint32_t *               dest,
               const uint32_t *         src,
               const uint32_t *         mask,
               int                      width)
{
    int i;

    if (!mask)
	memcpy (dest, src, width * sizeof (uint16_t));
    else
    {
	uint16_t *d = (uint16_t*)dest;
	uint16_t *src16 = (uint16_t*)src;
	for (i = 0; i < width; ++i)
	{
	    if ((*mask & 0xff000000) == 0xff000000) {
		// it's likely worth special casing
		// fully opaque because it avoids
		// the cost of conversion as well the multiplication
		*(d + i) = *src16;
	    } else {
		// the mask is still 32bits
		uint32_t s = combine_mask (convert_0565_to_8888(*src16), *mask);
		*(d + i) = convert_8888_to_0565(s);
	    }
	    mask++;
	    src16++;
	}
    }

}

static void
combine_over_u (pixman_implementation_t *imp,
               pixman_op_t              op,
               uint32_t *                dest,
               const uint32_t *          src,
               const uint32_t *          mask,
               int                      width)
{
    int i;

    if (!mask)
	memcpy (dest, src, width * sizeof (uint16_t));
    else
    {
	uint16_t *d = (uint16_t*)dest;
	uint16_t *src16 = (uint16_t*)src;
	for (i = 0; i < width; ++i)
	{
	    if ((*mask & 0xff000000) == 0xff000000) {
		// it's likely worth special casing
		// fully opaque because it avoids
		// the cost of conversion as well the multiplication
		*(d + i) = *src16;
	    } else if ((*mask & 0xff000000) == 0x00000000) {
		// keep the dest the same
	    } else {
		// the mask is still 32bits
		uint32_t s = combine_mask (convert_0565_to_8888(*src16), *mask);
		uint32_t ia = ALPHA_8 (~s);
		uint32_t d32 = convert_0565_to_8888(*(d + i));
		UN8x4_MUL_UN8_ADD_UN8x4 (d32, ia, s);
		*(d + i) = convert_8888_to_0565(d32);
	    }
	    mask++;
	    src16++;
	}
    }

}


void
_pixman_setup_combiner_functions_16 (pixman_implementation_t *imp)
{
    int i;
    for (i = 0; i < PIXMAN_N_OPERATORS; i++) {
	imp->combine_16[i] = NULL;
    }
    imp->combine_16[PIXMAN_OP_SRC] = combine_src_u;
    imp->combine_16[PIXMAN_OP_OVER] = combine_over_u;
}


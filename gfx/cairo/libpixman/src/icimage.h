/*
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* XXX: This whole file should be moved up into incint.h (and cleaned
   up considerably as well) */

#ifndef _ICIMAGE_H_
#define _ICIMAGE_H_

/* #include "glyphstr.h" */
/* #include "scrnintstr.h" */

/* XXX: Hmmm... what's needed from here?
#include "resource.h"
*/


#define IcIntMult(a,b,t) ( (t) = (a) * (b) + 0x80, ( ( ( (t)>>8 ) + (t) )>>8 ) )
#define IcIntDiv(a,b)	 (((uint16_t) (a) * 255) / (b))

#define IcGet8(v,i)   ((uint16_t) (uint8_t) ((v) >> i))

/*
 * There are two ways of handling alpha -- either as a single unified value or
 * a separate value for each component, hence each macro must have two
 * versions.  The unified alpha version has a 'U' at the end of the name,
 * the component version has a 'C'.  Similarly, functions which deal with
 * this difference will have two versions using the same convention.
 */

#define IcOverU(x,y,i,a,t) ((t) = IcIntMult(IcGet8(y,i),(a),(t)) + IcGet8(x,i),\
			   (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

#define IcOverC(x,y,i,a,t) ((t) = IcIntMult(IcGet8(y,i),IcGet8(a,i),(t)) + IcGet8(x,i),\
			    (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

#define IcInU(x,i,a,t) ((uint32_t) IcIntMult(IcGet8(x,i),(a),(t)) << (i))

#define IcInC(x,i,a,t) ((uint32_t) IcIntMult(IcGet8(x,i),IcGet8(a,i),(t)) << (i))

#define IcGen(x,y,i,ax,ay,t,u,v) ((t) = (IcIntMult(IcGet8(y,i),ay,(u)) + \
					 IcIntMult(IcGet8(x,i),ax,(v))),\
				  (uint32_t) ((uint8_t) ((t) | \
						     (0 - ((t) >> 8)))) << (i))

#define IcAdd(x,y,i,t)	((t) = IcGet8(x,i) + IcGet8(y,i), \
			 (uint32_t) ((uint8_t) ((t) | (0 - ((t) >> 8)))) << (i))

/*
typedef struct _IndexFormat {
    VisualPtr	    pVisual; 
    ColormapPtr	    pColormap;
    int		    nvalues;
    xIndexValue	    *pValues;
    void	    *devPrivate;
} IndexFormatRec;
*/

/*
typedef struct pixman_format {
    uint32_t	    id;
    uint32_t	    format;
    unsigned char   type;
    unsigned char   depth;
    DirectFormatRec direct;
    IndexFormatRec  index;
} pixman_format_t;
*/

struct pixman_image {
    IcPixels	    *pixels;
    pixman_format_t	    image_format;
    int		    format_code;
    int		    refcnt;
    
    unsigned int    repeat : 1;
    unsigned int    graphicsExposures : 1;
    unsigned int    subWindowMode : 1;
    unsigned int    polyEdge : 1;
    unsigned int    polyMode : 1;
    /* XXX: Do we need this field */
    unsigned int    freeCompClip : 1;
    unsigned int    clientClipType : 2;
    unsigned int    componentAlpha : 1;
    unsigned int    unused : 23;

    struct pixman_image *alphaMap;
    IcPoint	    alphaOrigin;

    IcPoint 	    clipOrigin;
    void	   *clientClip;

    unsigned long   dither;

    unsigned long   stateChanges;
    unsigned long   serialNumber;

    pixman_region16_t	    *pCompositeClip;
    
    pixman_transform_t     *transform;

    pixman_filter_t	    filter;
    pixman_fixed16_16_t    *filter_params;
    int		    filter_nparams;

    int		    owns_pixels;
};

#endif /* _ICIMAGE_H_ */

#ifndef _IC_MIPICT_H_
#define _IC_MIPICT_H_

#define IC_MAX_INDEXED	256 /* XXX depth must be <= 8 */

#if IC_MAX_INDEXED <= 256
typedef uint8_t IcIndexType;
#endif

/* XXX: We're not supporting indexed operations, right?
typedef struct _IcIndexed {
    int	color;
    uint32_t	rgba[IC_MAX_INDEXED];
    IcIndexType	ent[32768];
} IcIndexedRec, *IcIndexedPtr;
*/

#define IcCvtR8G8B8to15(s) ((((s) >> 3) & 0x001f) | \
			     (((s) >> 6) & 0x03e0) | \
			     (((s) >> 9) & 0x7c00))
#define IcIndexToEnt15(icf,rgb15) ((icf)->ent[rgb15])
#define IcIndexToEnt24(icf,rgb24) IcIndexToEnt15(icf,IcCvtR8G8B8to15(rgb24))

#define IcIndexToEntY24(icf,rgb24) ((icf)->ent[CvtR8G8B8toY15(rgb24)])

/*
pixman_private int
IcCreatePicture (PicturePtr pPicture);
*/

pixman_private void
pixman_image_init (pixman_image_t *image);

pixman_private void
pixman_image_destroyClip (pixman_image_t *image);

/*
pixman_private void
IcValidatePicture (PicturePtr pPicture,
		   Mask       mask);
*/


/* XXX: What should this be?
pixman_private int
IcClipPicture (pixman_region16_t    *region,
	       pixman_image_t	    *image,
	       int16_t	    xReg,
	       int16_t	    yReg,
	       int16_t	    xPict,
	       int16_t	    yPict);
*/

pixman_private int
IcComputeCompositeRegion (pixman_region16_t	*region,
			  pixman_image_t	*iSrc,
			  pixman_image_t	*iMask,
			  pixman_image_t	*iDst,
			  int16_t		xSrc,
			  int16_t		ySrc,
			  int16_t		xMask,
			  int16_t		yMask,
			  int16_t		xDst,
			  int16_t		yDst,
			  uint16_t	width,
			  uint16_t	height);

int
miIsSolidAlpha (pixman_image_t *src);

/*
pixman_private int
IcPictureInit (ScreenPtr pScreen, PictFormatPtr formats, int nformats);
*/

/*
pixman_private void
IcGlyphs (pixman_operator_t	op,
	  PicturePtr	pSrc,
	  PicturePtr	pDst,
	  PictFormatPtr	maskFormat,
	  int16_t		xSrc,
	  int16_t		ySrc,
	  int		nlist,
	  GlyphListPtr	list,
	  GlyphPtr	*glyphs);
*/

/*
pixman_private void
pixman_compositeRects (pixman_operator_t	op,
		  PicturePtr	pDst,
		  xRenderColor  *color,
		  int		nRect,
		  xRectangle    *rects);
*/

pixman_private pixman_image_t *
IcCreateAlphaPicture (pixman_image_t	*dst,
		      pixman_format_t	*format,
		      uint16_t	width,
		      uint16_t	height);

typedef void	(*CompositeFunc) (pixman_operator_t   op,
				  pixman_image_t    *iSrc,
				  pixman_image_t    *iMask,
				  pixman_image_t    *iDst,
				  int16_t      xSrc,
				  int16_t      ySrc,
				  int16_t      xMask,
				  int16_t      yMask,
				  int16_t      xDst,
				  int16_t      yDst,
				  uint16_t     width,
				  uint16_t     height);

typedef struct _pixman_compositeOperand pixman_compositeOperand;

typedef uint32_t (*pixman_compositeFetch)(pixman_compositeOperand *op);
typedef void (*pixman_compositeStore) (pixman_compositeOperand *op, uint32_t value);

typedef void (*pixman_compositeStep) (pixman_compositeOperand *op);
typedef void (*pixman_compositeSet) (pixman_compositeOperand *op, int x, int y);

struct _pixman_compositeOperand {
    union {
	struct {
	    pixman_bits_t		*top_line;
	    int			left_offset;
	    
	    int			start_offset;
	    pixman_bits_t		*line;
	    uint32_t		offset;
	    IcStride		stride;
	    int			bpp;
	} drawable;
	struct {
	    int			alpha_dx;
	    int			alpha_dy;
	} external;
	struct {
	    int			top_y;
	    int			left_x;
	    int			start_x;
	    int			x;
	    int			y;
	    pixman_transform_t		*transform;
	    pixman_filter_t		filter;
	    int                         repeat;
	    int                         width;
	    int                         height;
	} transform;
    } u;
    pixman_compositeFetch	fetch;
    pixman_compositeFetch	fetcha;
    pixman_compositeStore	store;
    pixman_compositeStep	over;
    pixman_compositeStep	down;
    pixman_compositeSet	set;
/* XXX: We're not supporting indexed operations, right?
    IcIndexedPtr	indexed;
*/
    pixman_region16_t		*clip;
};

typedef void (*IcCombineFunc) (pixman_compositeOperand	*src,
			       pixman_compositeOperand	*msk,
			       pixman_compositeOperand	*dst);

typedef struct _IcAccessMap {
    uint32_t		format_code;
    pixman_compositeFetch	fetch;
    pixman_compositeFetch	fetcha;
    pixman_compositeStore	store;
} IcAccessMap;

/* iccompose.c */

typedef struct _IcCompSrc {
    uint32_t	value;
    uint32_t	alpha;
} IcCompSrc;

pixman_private int
IcBuildCompositeOperand (pixman_image_t	    *image,
			 pixman_compositeOperand op[4],
			 int16_t		    x,
			 int16_t		    y,
			 int		    transform,
			 int		    alpha);

pixman_private void
pixman_compositeGeneral (pixman_operator_t	op,
			 pixman_image_t	*iSrc,
			 pixman_image_t	*iMask,
			 pixman_image_t	*iDst,
			 int16_t	xSrc,
			 int16_t	ySrc,
			 int16_t	xMask,
			 int16_t	yMask,
			 int16_t	xDst,
			 int16_t	yDst,
			 uint16_t	width,
			 uint16_t	height);

#endif /* _IC_MIPICT_H_ */

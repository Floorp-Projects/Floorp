/***********************************************************

Copyright 1987, 1988, 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1987, 1988, 1989 by
Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

#define PIXREGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
/* not a region */
#define PIXREGION_NAR(reg)	((reg)->data == pixman_brokendata)
#define PIXREGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define PIXREGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define PIXREGION_RECTS(reg) ((reg)->data ? (box_type_t *)((reg)->data + 1) \
			               : &(reg)->extents)
#define PIXREGION_BOXPTR(reg) ((box_type_t *)((reg)->data + 1))
#define PIXREGION_BOX(reg,i) (&PIXREGION_BOXPTR(reg)[i])
#define PIXREGION_TOP(reg) PIXREGION_BOX(reg, (reg)->data->numRects)
#define PIXREGION_END(reg) PIXREGION_BOX(reg, (reg)->data->numRects - 1)


#undef assert
#ifdef DEBUG_PIXREGION
#define assert(expr) {if (!(expr)) \
		FatalError("Assertion failed file %s, line %d: expr\n", \
			__FILE__, __LINE__); }
#else
#define assert(expr)
#endif

#define good(reg) assert(PREFIX(_selfcheck) (reg))

#undef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#undef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static const box_type_t PREFIX(_emptyBox_) = {0, 0, 0, 0};
static const region_data_type_t PREFIX(_emptyData_) = {0, 0};
static const region_data_type_t PREFIX(_brokendata_) = {0, 0};

static box_type_t *pixman_region_emptyBox = (box_type_t *)&PREFIX(_emptyBox_);
static region_data_type_t *pixman_region_emptyData = (region_data_type_t *)&PREFIX(_emptyData_);
static region_data_type_t *pixman_brokendata = (region_data_type_t *)&PREFIX(_brokendata_);

/* This function exists only to make it possible to preserve the X ABI - it should
 * go away at first opportunity.
 *
 * The problem is that the X ABI exports the three structs and has used
 * them through macros. So the X server calls this function with
 * the addresses of those structs which makes the existing code continue to
 * work.
 */
void
PREFIX(_internal_set_static_pointers) (box_type_t *empty_box,
				       region_data_type_t *empty_data,
				       region_data_type_t *broken_data)
{
    pixman_region_emptyBox = empty_box;
    pixman_region_emptyData = empty_data;
    pixman_brokendata = broken_data;
}

static pixman_bool_t
pixman_break (region_type_t *pReg);

/*
 * The functions in this file implement the Region abstraction used extensively
 * throughout the X11 sample server. A Region is simply a set of disjoint
 * (non-overlapping) rectangles, plus an "extent" rectangle which is the
 * smallest single rectangle that contains all the non-overlapping rectangles.
 *
 * A Region is implemented as a "y-x-banded" array of rectangles.  This array
 * imposes two degrees of order.  First, all rectangles are sorted by top side
 * y coordinate first (y1), and then by left side x coordinate (x1).
 *
 * Furthermore, the rectangles are grouped into "bands".  Each rectangle in a
 * band has the same top y coordinate (y1), and each has the same bottom y
 * coordinate (y2).  Thus all rectangles in a band differ only in their left
 * and right side (x1 and x2).  Bands are implicit in the array of rectangles:
 * there is no separate list of band start pointers.
 *
 * The y-x band representation does not minimize rectangles.  In particular,
 * if a rectangle vertically crosses a band (the rectangle has scanlines in
 * the y1 to y2 area spanned by the band), then the rectangle may be broken
 * down into two or more smaller rectangles stacked one atop the other.
 *
 *  -----------				    -----------
 *  |         |				    |         |		    band 0
 *  |         |  --------		    -----------  --------
 *  |         |  |      |  in y-x banded    |         |  |      |   band 1
 *  |         |  |      |  form is	    |         |  |      |
 *  -----------  |      |		    -----------  --------
 *               |      |				 |      |   band 2
 *               --------				 --------
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible: no two rectangles within a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course).
 *
 * Adam de Boor wrote most of the original region code.  Joel McCormack
 * substantially modified or rewrote most of the core arithmetic routines, and
 * added pixman_region_validate in order to support several speed improvements to
 * pixman_region_validateTree.  Bob Scheifler changed the representation to be more
 * compact when empty or a single rectangle, and did a bunch of gratuitous
 * reformatting. Carl Worth did further gratuitous reformatting while re-merging
 * the server and client region code into libpixregion.
 */

/*  true iff two Boxes overlap */
#define EXTENTCHECK(r1,r2) \
      (!( ((r1)->x2 <= (r2)->x1)  || \
          ((r1)->x1 >= (r2)->x2)  || \
          ((r1)->y2 <= (r2)->y1)  || \
          ((r1)->y1 >= (r2)->y2) ) )

/* true iff (x,y) is in Box */
#define INBOX(r,x,y) \
      ( ((r)->x2 >  x) && \
        ((r)->x1 <= x) && \
        ((r)->y2 >  y) && \
        ((r)->y1 <= y) )

/* true iff Box r1 contains Box r2 */
#define SUBSUMES(r1,r2) \
      ( ((r1)->x1 <= (r2)->x1) && \
        ((r1)->x2 >= (r2)->x2) && \
        ((r1)->y1 <= (r2)->y1) && \
        ((r1)->y2 >= (r2)->y2) )

static size_t
PIXREGION_SZOF(size_t n)
{
    size_t size = n * sizeof(box_type_t);
    if (n > UINT32_MAX / sizeof(box_type_t))
        return 0;

    if (sizeof(region_data_type_t) > UINT32_MAX - size)
        return 0;

    return size + sizeof(region_data_type_t);
}

static void *
allocData(size_t n)
{
    size_t sz = PIXREGION_SZOF(n);
    if (!sz)
	return NULL;

    return malloc(sz);
}

#define freeData(reg) if ((reg)->data && (reg)->data->size) free((reg)->data)

#define RECTALLOC_BAIL(pReg,n,bail) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!pixman_rect_alloc(pReg, n)) { goto bail; }

#define RECTALLOC(pReg,n) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    if (!pixman_rect_alloc(pReg, n)) { return FALSE; }

#define ADDRECT(pNextRect,nx1,ny1,nx2,ny2)	\
{						\
    pNextRect->x1 = nx1;			\
    pNextRect->y1 = ny1;			\
    pNextRect->x2 = nx2;			\
    pNextRect->y2 = ny2;			\
    pNextRect++;				\
}

#define NEWRECT(pReg,pNextRect,nx1,ny1,nx2,ny2)			\
{									\
    if (!(pReg)->data || ((pReg)->data->numRects == (pReg)->data->size))\
    {									\
	if (!pixman_rect_alloc(pReg, 1))					\
	    return FALSE;						\
	pNextRect = PIXREGION_TOP(pReg);					\
    }									\
    ADDRECT(pNextRect,nx1,ny1,nx2,ny2);					\
    pReg->data->numRects++;						\
    assert(pReg->data->numRects<=pReg->data->size);			\
}

#define DOWNSIZE(reg,numRects)						\
    if (((numRects) < ((reg)->data->size >> 1)) && ((reg)->data->size > 50)) \
    {									\
	region_data_type_t * NewData;				\
	size_t data_size = PIXREGION_SZOF(numRects);			\
	if (!data_size)							\
	    NewData = NULL;						\
	else								\
	    NewData = (region_data_type_t *)realloc((reg)->data, data_size); \
	if (NewData)							\
	{								\
	    NewData->size = (numRects);					\
	    (reg)->data = NewData;					\
	}								\
    }

PIXMAN_EXPORT pixman_bool_t
PREFIX(_equal) (reg1, reg2)
    region_type_t * reg1;
    region_type_t * reg2;
{
    int i;
    box_type_t *rects1;
    box_type_t *rects2;

    if (reg1->extents.x1 != reg2->extents.x1) return FALSE;
    if (reg1->extents.x2 != reg2->extents.x2) return FALSE;
    if (reg1->extents.y1 != reg2->extents.y1) return FALSE;
    if (reg1->extents.y2 != reg2->extents.y2) return FALSE;
    if (PIXREGION_NUM_RECTS(reg1) != PIXREGION_NUM_RECTS(reg2)) return FALSE;

    rects1 = PIXREGION_RECTS(reg1);
    rects2 = PIXREGION_RECTS(reg2);
    for (i = 0; i != PIXREGION_NUM_RECTS(reg1); i++) {
	if (rects1[i].x1 != rects2[i].x1) return FALSE;
	if (rects1[i].x2 != rects2[i].x2) return FALSE;
	if (rects1[i].y1 != rects2[i].y1) return FALSE;
	if (rects1[i].y2 != rects2[i].y2) return FALSE;
    }
    return TRUE;
}

int
PREFIX(_print) (rgn)
    region_type_t * rgn;
{
    int num, size;
    int i;
    box_type_t * rects;

    num = PIXREGION_NUM_RECTS(rgn);
    size = PIXREGION_SIZE(rgn);
    rects = PIXREGION_RECTS(rgn);
    fprintf(stderr, "num: %d size: %d\n", num, size);
    fprintf(stderr, "extents: %d %d %d %d\n",
	   rgn->extents.x1, rgn->extents.y1, rgn->extents.x2, rgn->extents.y2);
    for (i = 0; i < num; i++)
	fprintf(stderr, "%d %d %d %d \n",
		rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
    fprintf(stderr, "\n");
    return(num);
}


PIXMAN_EXPORT void
PREFIX(_init) (region_type_t *region)
{
    region->extents = *pixman_region_emptyBox;
    region->data = pixman_region_emptyData;
}

PIXMAN_EXPORT void
PREFIX(_init_rect) (region_type_t *region,
		    int x, int y, unsigned int width, unsigned int height)
{
    region->extents.x1 = x;
    region->extents.y1 = y;
    region->extents.x2 = x + width;
    region->extents.y2 = y + height;
    region->data = NULL;
}

PIXMAN_EXPORT void
PREFIX(_init_with_extents) (region_type_t *region, box_type_t *extents)
{
    region->extents = *extents;
    region->data = NULL;
}

PIXMAN_EXPORT void
PREFIX(_fini) (region_type_t *region)
{
    good (region);
    freeData (region);
}

PIXMAN_EXPORT int
PREFIX(_n_rects) (region_type_t *region)
{
    return PIXREGION_NUM_RECTS (region);
}

PIXMAN_EXPORT box_type_t *
PREFIX(_rectangles) (region_type_t *region,
				  int		    *n_rects)
{
    if (n_rects)
	*n_rects = PIXREGION_NUM_RECTS (region);

    return PIXREGION_RECTS (region);
}

static pixman_bool_t
pixman_break (region_type_t *region)
{
    freeData (region);
    region->extents = *pixman_region_emptyBox;
    region->data = pixman_brokendata;
    return FALSE;
}

static pixman_bool_t
pixman_rect_alloc (region_type_t * region, int n)
{
    region_data_type_t *data;

    if (!region->data)
    {
	n++;
	region->data = allocData(n);
	if (!region->data)
	    return pixman_break (region);
	region->data->numRects = 1;
	*PIXREGION_BOXPTR(region) = region->extents;
    }
    else if (!region->data->size)
    {
	region->data = allocData(n);
	if (!region->data)
	    return pixman_break (region);
	region->data->numRects = 0;
    }
    else
    {
	size_t data_size;
	if (n == 1)
	{
	    n = region->data->numRects;
	    if (n > 500) /* XXX pick numbers out of a hat */
		n = 250;
	}
	n += region->data->numRects;
	data_size = PIXREGION_SZOF(n);
	if (!data_size)
	    data = NULL;
	else
	    data = (region_data_type_t *)realloc(region->data, PIXREGION_SZOF(n));
	if (!data)
	    return pixman_break (region);
	region->data = data;
    }
    region->data->size = n;
    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
PREFIX(_copy) (region_type_t *dst, region_type_t *src)
{
    good(dst);
    good(src);
    if (dst == src)
	return TRUE;
    dst->extents = src->extents;
    if (!src->data || !src->data->size)
    {
	freeData(dst);
	dst->data = src->data;
	return TRUE;
    }
    if (!dst->data || (dst->data->size < src->data->numRects))
    {
	freeData(dst);
	dst->data = allocData(src->data->numRects);
	if (!dst->data)
	    return pixman_break (dst);
	dst->data->size = src->data->numRects;
    }
    dst->data->numRects = src->data->numRects;
    memmove((char *)PIXREGION_BOXPTR(dst),(char *)PIXREGION_BOXPTR(src),
	  dst->data->numRects * sizeof(box_type_t));
    return TRUE;
}

/*======================================================================
 *	    Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_coalesce --
 *	Attempt to merge the boxes in the current band with those in the
 *	previous one.  We are guaranteed that the current band extends to
 *      the end of the rects array.  Used only by pixman_op.
 *
 * Results:
 *	The new index for the previous band.
 *
 * Side Effects:
 *	If coalescing takes place:
 *	    - rectangles in the previous band will have their y2 fields
 *	      altered.
 *	    - region->data->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
static inline int
pixman_coalesce (
    region_type_t *	region,	    	/* Region to coalesce		     */
    int	    	  	prevStart,  	/* Index of start of previous band   */
    int	    	  	curStart)   	/* Index of start of current band    */
{
    box_type_t *	pPrevBox;   	/* Current box in previous band	     */
    box_type_t *	pCurBox;    	/* Current box in current band       */
    int  	numRects;	/* Number rectangles in both bands   */
    int	y2;		/* Bottom of current band	     */
    /*
     * Figure out how many rectangles are in the band.
     */
    numRects = curStart - prevStart;
    assert(numRects == region->data->numRects - curStart);

    if (!numRects) return curStart;

    /*
     * The bands may only be coalesced if the bottom of the previous
     * matches the top scanline of the current.
     */
    pPrevBox = PIXREGION_BOX(region, prevStart);
    pCurBox = PIXREGION_BOX(region, curStart);
    if (pPrevBox->y2 != pCurBox->y1) return curStart;

    /*
     * Make sure the bands have boxes in the same places. This
     * assumes that boxes have been added in such a way that they
     * cover the most area possible. I.e. two boxes in a band must
     * have some horizontal space between them.
     */
    y2 = pCurBox->y2;

    do {
	if ((pPrevBox->x1 != pCurBox->x1) || (pPrevBox->x2 != pCurBox->x2)) {
	    return (curStart);
	}
	pPrevBox++;
	pCurBox++;
	numRects--;
    } while (numRects);

    /*
     * The bands may be merged, so set the bottom y of each box
     * in the previous band to the bottom y of the current band.
     */
    numRects = curStart - prevStart;
    region->data->numRects -= numRects;
    do {
	pPrevBox--;
	pPrevBox->y2 = y2;
	numRects--;
    } while (numRects);
    return prevStart;
}

/* Quicky macro to avoid trivial reject procedure calls to pixman_coalesce */

#define Coalesce(newReg, prevBand, curBand)				\
    if (curBand - prevBand == newReg->data->numRects - curBand) {	\
	prevBand = pixman_coalesce(newReg, prevBand, curBand);		\
    } else {								\
	prevBand = curBand;						\
    }

/*-
 *-----------------------------------------------------------------------
 * pixman_region_appendNonO --
 *	Handle a non-overlapping band for the union and subtract operations.
 *      Just adds the (top/bottom-clipped) rectangles into the region.
 *      Doesn't have to check for subsumption or anything.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	region->data->numRects is incremented and the rectangles overwritten
 *	with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */

static inline pixman_bool_t
pixman_region_appendNonO (
    region_type_t *	region,
    box_type_t *	r,
    box_type_t *  	  	rEnd,
    int  	y1,
    int  	y2)
{
    box_type_t *	pNextRect;
    int	newRects;

    newRects = rEnd - r;

    assert(y1 < y2);
    assert(newRects != 0);

    /* Make sure we have enough space for all rectangles to be added */
    RECTALLOC(region, newRects);
    pNextRect = PIXREGION_TOP(region);
    region->data->numRects += newRects;
    do {
	assert(r->x1 < r->x2);
	ADDRECT(pNextRect, r->x1, y1, r->x2, y2);
	r++;
    } while (r != rEnd);

    return TRUE;
}

#define FindBand(r, rBandEnd, rEnd, ry1)		    \
{							    \
    ry1 = r->y1;					    \
    rBandEnd = r+1;					    \
    while ((rBandEnd != rEnd) && (rBandEnd->y1 == ry1)) {   \
	rBandEnd++;					    \
    }							    \
}

#define	AppendRegions(newReg, r, rEnd)					\
{									\
    int newRects;							\
    if ((newRects = rEnd - r)) {					\
	RECTALLOC(newReg, newRects);					\
	memmove((char *)PIXREGION_TOP(newReg),(char *)r, 			\
              newRects * sizeof(box_type_t));				\
	newReg->data->numRects += newRects;				\
    }									\
}

/*-
 *-----------------------------------------------------------------------
 * pixman_op --
 *	Apply an operation to two regions. Called by pixman_region_union, pixman_region_inverse,
 *	pixman_region_subtract, pixman_region_intersect....  Both regions MUST have at least one
 *      rectangle, and cannot be the same object.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	The new region is overwritten.
 *	pOverlap set to TRUE if overlapFunc ever returns TRUE.
 *
 * Notes:
 *	The idea behind this function is to view the two regions as sets.
 *	Together they cover a rectangle of area that this function divides
 *	into horizontal bands where points are covered only by one region
 *	or by both. For the first case, the nonOverlapFunc is called with
 *	each the band and the band's upper and lower extents. For the
 *	second, the overlapFunc is called to process the entire band. It
 *	is responsible for clipping the rectangles in the band, though
 *	this function provides the boundaries.
 *	At the end of each band, the new region is coalesced, if possible,
 *	to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */

typedef pixman_bool_t (*OverlapProcPtr)(
    region_type_t	 *region,
    box_type_t *r1,
    box_type_t *r1End,
    box_type_t *r2,
    box_type_t *r2End,
    int    	 y1,
    int    	 y2,
    int		 *pOverlap);

static pixman_bool_t
pixman_op(
    region_type_t *newReg,		    /* Place to store result	     */
    region_type_t *       reg1,		    /* First region in operation     */
    region_type_t *       reg2,		    /* 2d region in operation        */
    OverlapProcPtr  overlapFunc,            /* Function to call for over-
					     * lapping bands		     */
    int	    appendNon1,		    /* Append non-overlapping bands  */
					    /* in region 1 ? */
    int	    appendNon2,		    /* Append non-overlapping bands  */
					    /* in region 2 ? */
    int	    *pOverlap)
{
    box_type_t * r1;			    /* Pointer into first region     */
    box_type_t * r2;			    /* Pointer into 2d region	     */
    box_type_t *	    r1End;		    /* End of 1st region	     */
    box_type_t *	    r2End;		    /* End of 2d region		     */
    int	    ybot;		    /* Bottom of intersection	     */
    int	    ytop;		    /* Top of intersection	     */
    region_data_type_t *	    oldData;		    /* Old data for newReg	     */
    int		    prevBand;		    /* Index of start of
					     * previous band in newReg       */
    int		    curBand;		    /* Index of start of current
					     * band in newReg		     */
    box_type_t * r1BandEnd;		    /* End of current band in r1     */
    box_type_t * r2BandEnd;		    /* End of current band in r2     */
    int	    top;		    /* Top of non-overlapping band   */
    int	    bot;		    /* Bottom of non-overlapping band*/
    int    r1y1;		    /* Temps for r1->y1 and r2->y1   */
    int    r2y1;
    int		    newSize;
    int		    numRects;

    /*
     * Break any region computed from a broken region
     */
    if (PIXREGION_NAR (reg1) || PIXREGION_NAR(reg2))
	return pixman_break (newReg);

    /*
     * Initialization:
     *	set r1, r2, r1End and r2End appropriately, save the rectangles
     * of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */

    r1 = PIXREGION_RECTS(reg1);
    newSize = PIXREGION_NUM_RECTS(reg1);
    r1End = r1 + newSize;
    numRects = PIXREGION_NUM_RECTS(reg2);
    r2 = PIXREGION_RECTS(reg2);
    r2End = r2 + numRects;
    assert(r1 != r1End);
    assert(r2 != r2End);

    oldData = (region_data_type_t *)NULL;
    if (((newReg == reg1) && (newSize > 1)) ||
	((newReg == reg2) && (numRects > 1)))
    {
	oldData = newReg->data;
	newReg->data = pixman_region_emptyData;
    }
    /* guess at new size */
    if (numRects > newSize)
	newSize = numRects;
    newSize <<= 1;
    if (!newReg->data)
	newReg->data = pixman_region_emptyData;
    else if (newReg->data->size)
	newReg->data->numRects = 0;
    if (newSize > newReg->data->size) {
	if (!pixman_rect_alloc(newReg, newSize)) {
	    if (oldData)
		free (oldData);
	    return FALSE;
	}
    }

    /*
     * Initialize ybot.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     * 	In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *	For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */

    ybot = MIN(r1->y1, r2->y1);

    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. pixman_coalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;

    do {
	/*
	 * This algorithm proceeds one source-band (as opposed to a
	 * destination band, which is determined by where the two regions
	 * intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
	 * rectangle after the last one in the current band for their
	 * respective regions.
	 */
	assert(r1 != r1End);
	assert(r2 != r2End);

	FindBand(r1, r1BandEnd, r1End, r1y1);
	FindBand(r2, r2BandEnd, r2End, r2y1);

	/*
	 * First handle the band that doesn't intersect, if any.
	 *
	 * Note that attention is restricted to one band in the
	 * non-intersecting region at once, so if a region has n
	 * bands between the current position and the next place it overlaps
	 * the other, this entire loop will be passed through n times.
	 */
	if (r1y1 < r2y1) {
	    if (appendNon1) {
		top = MAX(r1y1, ybot);
		bot = MIN(r1->y2, r2y1);
		if (top != bot)	{
		    curBand = newReg->data->numRects;
		    pixman_region_appendNonO(newReg, r1, r1BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r2y1;
	} else if (r2y1 < r1y1) {
	    if (appendNon2) {
		top = MAX(r2y1, ybot);
		bot = MIN(r2->y2, r1y1);
		if (top != bot) {
		    curBand = newReg->data->numRects;
		    pixman_region_appendNonO(newReg, r2, r2BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r1y1;
	} else {
	    ytop = r1y1;
	}

	/*
	 * Now see if we've hit an intersecting band. The two bands only
	 * intersect if ybot > ytop
	 */
	ybot = MIN(r1->y2, r2->y2);
	if (ybot > ytop) {
	    curBand = newReg->data->numRects;
	    (* overlapFunc)(newReg, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot,
			    pOverlap);
	    Coalesce(newReg, prevBand, curBand);
	}

	/*
	 * If we've finished with a band (y2 == ybot) we skip forward
	 * in the region to the next band.
	 */
	if (r1->y2 == ybot) r1 = r1BandEnd;
	if (r2->y2 == ybot) r2 = r2BandEnd;

    } while (r1 != r1End && r2 != r2End);

    /*
     * Deal with whichever region (if any) still has rectangles left.
     *
     * We only need to worry about banding and coalescing for the very first
     * band left.  After that, we can just group all remaining boxes,
     * regardless of how many bands, into one final append to the list.
     */

    if ((r1 != r1End) && appendNon1) {
	/* Do first nonOverlap1Func call, which may be able to coalesce */
	FindBand(r1, r1BandEnd, r1End, r1y1);
	curBand = newReg->data->numRects;
	pixman_region_appendNonO(newReg, r1, r1BandEnd, MAX(r1y1, ybot), r1->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Just append the rest of the boxes  */
	AppendRegions(newReg, r1BandEnd, r1End);

    } else if ((r2 != r2End) && appendNon2) {
	/* Do first nonOverlap2Func call, which may be able to coalesce */
	FindBand(r2, r2BandEnd, r2End, r2y1);
	curBand = newReg->data->numRects;
	pixman_region_appendNonO(newReg, r2, r2BandEnd, MAX(r2y1, ybot), r2->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Append rest of boxes */
	AppendRegions(newReg, r2BandEnd, r2End);
    }

    if (oldData)
	free(oldData);

    if (!(numRects = newReg->data->numRects))
    {
	freeData(newReg);
	newReg->data = pixman_region_emptyData;
    }
    else if (numRects == 1)
    {
	newReg->extents = *PIXREGION_BOXPTR(newReg);
	freeData(newReg);
	newReg->data = (region_data_type_t *)NULL;
    }
    else
    {
	DOWNSIZE(newReg, numRects);
    }

    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * pixman_set_extents --
 *	Reset the extents of a region to what they should be. Called by
 *	pixman_region_subtract and pixman_region_intersect as they can't figure it out along the
 *	way or do so easily, as pixman_region_union can.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
static void
pixman_set_extents (region_type_t *region)
{
    box_type_t *box, *boxEnd;

    if (!region->data)
	return;
    if (!region->data->size)
    {
	region->extents.x2 = region->extents.x1;
	region->extents.y2 = region->extents.y1;
	return;
    }

    box = PIXREGION_BOXPTR(region);
    boxEnd = PIXREGION_END(region);

    /*
     * Since box is the first rectangle in the region, it must have the
     * smallest y1 and since boxEnd is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  box and boxEnd, resp., as good things to initialize them
     * to...
     */
    region->extents.x1 = box->x1;
    region->extents.y1 = box->y1;
    region->extents.x2 = boxEnd->x2;
    region->extents.y2 = boxEnd->y2;

    assert(region->extents.y1 < region->extents.y2);
    while (box <= boxEnd) {
	if (box->x1 < region->extents.x1)
	    region->extents.x1 = box->x1;
	if (box->x2 > region->extents.x2)
	    region->extents.x2 = box->x2;
	box++;
    };

    assert(region->extents.x1 < region->extents.x2);
}

/*======================================================================
 *	    Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * pixman_region_intersectO --
 *	Handle an overlapping band for pixman_region_intersect.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static pixman_bool_t
pixman_region_intersectO (region_type_t *region,
			  box_type_t    *r1,
			  box_type_t    *r1End,
			  box_type_t    *r2,
			  box_type_t    *r2End,
			  int    	     y1,
			  int    	     y2,
			  int		    *pOverlap)
{
    int  	x1;
    int  	x2;
    box_type_t *	pNextRect;

    pNextRect = PIXREGION_TOP(region);

    assert(y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    do {
	x1 = MAX(r1->x1, r2->x1);
	x2 = MIN(r1->x2, r2->x2);

	/*
	 * If there's any overlap between the two rectangles, add that
	 * overlap to the new region.
	 */
	if (x1 < x2)
	    NEWRECT(region, pNextRect, x1, y1, x2, y2);

	/*
	 * Advance the pointer(s) with the leftmost right side, since the next
	 * rectangle on that list may still overlap the other region's
	 * current rectangle.
	 */
	if (r1->x2 == x2) {
	    r1++;
	}
	if (r2->x2 == x2) {
	    r2++;
	}
    } while ((r1 != r1End) && (r2 != r2End));

    return TRUE;
}

PIXMAN_EXPORT pixman_bool_t
PREFIX(_intersect) (region_type_t * 	newReg,
			 region_type_t * 	reg1,
			 region_type_t *	reg2)
{
    good(reg1);
    good(reg2);
    good(newReg);
   /* check for trivial reject */
    if (PIXREGION_NIL(reg1)  || PIXREGION_NIL(reg2) ||
	!EXTENTCHECK(&reg1->extents, &reg2->extents))
    {
	/* Covers about 20% of all cases */
	freeData(newReg);
	newReg->extents.x2 = newReg->extents.x1;
	newReg->extents.y2 = newReg->extents.y1;
	if (PIXREGION_NAR(reg1) || PIXREGION_NAR(reg2))
	{
	    newReg->data = pixman_brokendata;
	    return FALSE;
	}
	else
	    newReg->data = pixman_region_emptyData;
    }
    else if (!reg1->data && !reg2->data)
    {
	/* Covers about 80% of cases that aren't trivially rejected */
	newReg->extents.x1 = MAX(reg1->extents.x1, reg2->extents.x1);
	newReg->extents.y1 = MAX(reg1->extents.y1, reg2->extents.y1);
	newReg->extents.x2 = MIN(reg1->extents.x2, reg2->extents.x2);
	newReg->extents.y2 = MIN(reg1->extents.y2, reg2->extents.y2);
	freeData(newReg);
	newReg->data = (region_data_type_t *)NULL;
    }
    else if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
	return PREFIX(_copy) (newReg, reg1);
    }
    else if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
	return PREFIX(_copy) (newReg, reg2);
    }
    else if (reg1 == reg2)
    {
	return PREFIX(_copy) (newReg, reg1);
    }
    else
    {
	/* General purpose intersection */
	int overlap; /* result ignored */
	if (!pixman_op(newReg, reg1, reg2, pixman_region_intersectO, FALSE, FALSE,
			&overlap))
	    return FALSE;
	pixman_set_extents(newReg);
    }

    good(newReg);
    return(TRUE);
}

#define MERGERECT(r)						\
{								\
    if (r->x1 <= x2) {						\
	/* Merge with current rectangle */			\
	if (r->x1 < x2) *pOverlap = TRUE;				\
	if (x2 < r->x2) x2 = r->x2;				\
    } else {							\
	/* Add current rectangle, start new one */		\
	NEWRECT(region, pNextRect, x1, y1, x2, y2);		\
	x1 = r->x1;						\
	x2 = r->x2;						\
    }								\
    r++;							\
}

/*======================================================================
 *	    Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_unionO --
 *	Handle an overlapping band for the union operation. Picks the
 *	left-most rectangle each time and merges it into the region.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	region is overwritten.
 *	pOverlap is set to TRUE if any boxes overlap.
 *
 *-----------------------------------------------------------------------
 */
static pixman_bool_t
pixman_region_unionO (
    region_type_t	 *region,
    box_type_t *r1,
    box_type_t *r1End,
    box_type_t *r2,
    box_type_t *r2End,
    int	  y1,
    int	  y2,
    int		  *pOverlap)
{
    box_type_t *     pNextRect;
    int        x1;     /* left and right side of current union */
    int        x2;

    assert (y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    pNextRect = PIXREGION_TOP(region);

    /* Start off current rectangle */
    if (r1->x1 < r2->x1)
    {
	x1 = r1->x1;
	x2 = r1->x2;
	r1++;
    }
    else
    {
	x1 = r2->x1;
	x2 = r2->x2;
	r2++;
    }
    while (r1 != r1End && r2 != r2End)
    {
	if (r1->x1 < r2->x1) MERGERECT(r1) else MERGERECT(r2);
    }

    /* Finish off whoever (if any) is left */
    if (r1 != r1End)
    {
	do
	{
	    MERGERECT(r1);
	} while (r1 != r1End);
    }
    else if (r2 != r2End)
    {
	do
	{
	    MERGERECT(r2);
	} while (r2 != r2End);
    }

    /* Add current rectangle */
    NEWRECT(region, pNextRect, x1, y1, x2, y2);

    return TRUE;
}

/* Convenience function for performing union of region with a
 * single rectangle
 */
PIXMAN_EXPORT pixman_bool_t
PREFIX(_union_rect) (region_type_t *dest,
			  region_type_t *source,
			  int x, int y,
			  unsigned int width, unsigned int height)
{
    region_type_t region;

    if (!width || !height)
	return PREFIX(_copy) (dest, source);
    region.data = NULL;
    region.extents.x1 = x;
    region.extents.y1 = y;
    region.extents.x2 = x + width;
    region.extents.y2 = y + height;

    return PREFIX(_union) (dest, source, &region);
}

PIXMAN_EXPORT pixman_bool_t
PREFIX(_union) (region_type_t *newReg,
		     region_type_t *reg1,
		     region_type_t *reg2)
{
    int overlap; /* result ignored */

    /* Return TRUE if some overlap
     * between reg1, reg2
     */
    good(reg1);
    good(reg2);
    good(newReg);
    /*  checks all the simple cases */

    /*
     * Region 1 and 2 are the same
     */
    if (reg1 == reg2)
    {
	return PREFIX(_copy) (newReg, reg1);
    }

    /*
     * Region 1 is empty
     */
    if (PIXREGION_NIL(reg1))
    {
	if (PIXREGION_NAR(reg1))
	    return pixman_break (newReg);
        if (newReg != reg2)
	    return PREFIX(_copy) (newReg, reg2);
        return TRUE;
    }

    /*
     * Region 2 is empty
     */
    if (PIXREGION_NIL(reg2))
    {
	if (PIXREGION_NAR(reg2))
	    return pixman_break (newReg);
        if (newReg != reg1)
	    return PREFIX(_copy) (newReg, reg1);
        return TRUE;
    }

    /*
     * Region 1 completely subsumes region 2
     */
    if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
        if (newReg != reg1)
	    return PREFIX(_copy) (newReg, reg1);
        return TRUE;
    }

    /*
     * Region 2 completely subsumes region 1
     */
    if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
        if (newReg != reg2)
	    return PREFIX(_copy) (newReg, reg2);
        return TRUE;
    }

    if (!pixman_op(newReg, reg1, reg2, pixman_region_unionO, TRUE, TRUE, &overlap))
	return FALSE;

    newReg->extents.x1 = MIN(reg1->extents.x1, reg2->extents.x1);
    newReg->extents.y1 = MIN(reg1->extents.y1, reg2->extents.y1);
    newReg->extents.x2 = MAX(reg1->extents.x2, reg2->extents.x2);
    newReg->extents.y2 = MAX(reg1->extents.y2, reg2->extents.y2);
    good(newReg);
    return TRUE;
}

/*======================================================================
 *	    Batch Rectangle Union
 *====================================================================*/

#define ExchangeRects(a, b) \
{			    \
    box_type_t     t;	    \
    t = rects[a];	    \
    rects[a] = rects[b];    \
    rects[b] = t;	    \
}

static void
QuickSortRects(
    box_type_t     rects[],
    int        numRects)
{
    int	y1;
    int	x1;
    int        i, j;
    box_type_t *r;

    /* Always called with numRects > 1 */

    do
    {
	if (numRects == 2)
	{
	    if (rects[0].y1 > rects[1].y1 ||
		    (rects[0].y1 == rects[1].y1 && rects[0].x1 > rects[1].x1))
		ExchangeRects(0, 1);
	    return;
	}

	/* Choose partition element, stick in location 0 */
        ExchangeRects(0, numRects >> 1);
	y1 = rects[0].y1;
	x1 = rects[0].x1;

        /* Partition array */
        i = 0;
        j = numRects;
        do
	{
	    r = &(rects[i]);
	    do
	    {
		r++;
		i++;
            } while (i != numRects &&
		     (r->y1 < y1 || (r->y1 == y1 && r->x1 < x1)));
	    r = &(rects[j]);
	    do
	    {
		r--;
		j--;
            } while (y1 < r->y1 || (y1 == r->y1 && x1 < r->x1));
            if (i < j)
		ExchangeRects(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeRects(0, j);

	/* Recurse */
        if (numRects-j-1 > 1)
	    QuickSortRects(&rects[j+1], numRects-j-1);
        numRects = j;
    } while (numRects > 1);
}

/*-
 *-----------------------------------------------------------------------
 * pixman_region_validate --
 *
 *      Take a ``region'' which is a non-y-x-banded random collection of
 *      rectangles, and compute a nice region which is the union of all the
 *      rectangles.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *      The passed-in ``region'' may be modified.
 *	pOverlap set to TRUE if any retangles overlapped,
 *      else FALSE;
 *
 * Strategy:
 *      Step 1. Sort the rectangles into ascending order with primary key y1
 *		and secondary key x1.
 *
 *      Step 2. Split the rectangles into the minimum number of proper y-x
 *		banded regions.  This may require horizontally merging
 *		rectangles, and vertically coalescing bands.  With any luck,
 *		this step in an identity transformation (ala the Box widget),
 *		or a coalescing into 1 box (ala Menus).
 *
 *	Step 3. Merge the separate regions down to a single region by calling
 *		pixman_region_union.  Maximize the work each pixman_region_union call does by using
 *		a binary merge.
 *
 *-----------------------------------------------------------------------
 */

static pixman_bool_t
validate (region_type_t * badreg,
	  int *pOverlap)
{
    /* Descriptor for regions under construction  in Step 2. */
    typedef struct {
	region_type_t   reg;
	int	    prevBand;
	int	    curBand;
    } RegionInfo;

	     int	numRects;   /* Original numRects for badreg	    */
	     RegionInfo *ri;	    /* Array of current regions		    */
    	     int	numRI;      /* Number of entries used in ri	    */
	     int	sizeRI;	    /* Number of entries available in ri    */
	     int	i;	    /* Index into rects			    */
    int	j;	    /* Index into ri			    */
    RegionInfo *rit;       /* &ri[j]				    */
    region_type_t *  reg;        /* ri[j].reg			    */
    box_type_t *	box;	    /* Current box in rects		    */
    box_type_t *	riBox;      /* Last box in ri[j].reg		    */
    region_type_t *  hreg;       /* ri[j_half].reg			    */
    pixman_bool_t ret = TRUE;

    *pOverlap = FALSE;
    if (!badreg->data)
    {
	good(badreg);
	return TRUE;
    }
    numRects = badreg->data->numRects;
    if (!numRects)
    {
	if (PIXREGION_NAR(badreg))
	    return FALSE;
	good(badreg);
	return TRUE;
    }
    if (badreg->extents.x1 < badreg->extents.x2)
    {
	if ((numRects) == 1)
	{
	    freeData(badreg);
	    badreg->data = (region_data_type_t *) NULL;
	}
	else
	{
	    DOWNSIZE(badreg, numRects);
	}
	good(badreg);
	return TRUE;
    }

    /* Step 1: Sort the rects array into ascending (y1, x1) order */
    QuickSortRects(PIXREGION_BOXPTR(badreg), numRects);

    /* Step 2: Scatter the sorted array into the minimum number of regions */

    /* Set up the first region to be the first rectangle in badreg */
    /* Note that step 2 code will never overflow the ri[0].reg rects array */
    ri = (RegionInfo *) pixman_malloc_ab (4, sizeof(RegionInfo));
    if (!ri)
	return pixman_break (badreg);
    sizeRI = 4;
    numRI = 1;
    ri[0].prevBand = 0;
    ri[0].curBand = 0;
    ri[0].reg = *badreg;
    box = PIXREGION_BOXPTR(&ri[0].reg);
    ri[0].reg.extents = *box;
    ri[0].reg.data->numRects = 1;
    badreg->extents = *pixman_region_emptyBox;
    badreg->data = pixman_region_emptyData;

    /* Now scatter rectangles into the minimum set of valid regions.  If the
       next rectangle to be added to a region would force an existing rectangle
       in the region to be split up in order to maintain y-x banding, just
       forget it.  Try the next region.  If it doesn't fit cleanly into any
       region, make a new one. */

    for (i = numRects; --i > 0;)
    {
	box++;
	/* Look for a region to append box to */
	for (j = numRI, rit = ri; --j >= 0; rit++)
	{
	    reg = &rit->reg;
	    riBox = PIXREGION_END(reg);

	    if (box->y1 == riBox->y1 && box->y2 == riBox->y2)
	    {
		/* box is in same band as riBox.  Merge or append it */
		if (box->x1 <= riBox->x2)
		{
		    /* Merge it with riBox */
		    if (box->x1 < riBox->x2) *pOverlap = TRUE;
		    if (box->x2 > riBox->x2) riBox->x2 = box->x2;
		}
		else
		{
		    RECTALLOC_BAIL(reg, 1, bail);
		    *PIXREGION_TOP(reg) = *box;
		    reg->data->numRects++;
		}
		goto NextRect;   /* So sue me */
	    }
	    else if (box->y1 >= riBox->y2)
	    {
		/* Put box into new band */
		if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
		if (reg->extents.x1 > box->x1)   reg->extents.x1 = box->x1;
		Coalesce(reg, rit->prevBand, rit->curBand);
		rit->curBand = reg->data->numRects;
		RECTALLOC_BAIL(reg, 1, bail);
		*PIXREGION_TOP(reg) = *box;
		reg->data->numRects++;
		goto NextRect;
	    }
	    /* Well, this region was inappropriate.  Try the next one. */
	} /* for j */

	/* Uh-oh.  No regions were appropriate.  Create a new one. */
	if (sizeRI == numRI)
	{
	    size_t data_size;
	    
	    /* Oops, allocate space for new region information */
	    sizeRI <<= 1;

            data_size = sizeRI * sizeof(RegionInfo);
            if (data_size / sizeRI != sizeof(RegionInfo))
                goto bail;
            rit = (RegionInfo *) realloc(ri, data_size);
	    if (!rit)
		goto bail;
	    ri = rit;
	    rit = &ri[numRI];
	}
	numRI++;
	rit->prevBand = 0;
	rit->curBand = 0;
	rit->reg.extents = *box;
	rit->reg.data = (region_data_type_t *)NULL;
	if (!pixman_rect_alloc(&rit->reg, (i+numRI) / numRI)) /* MUST force allocation */
	    goto bail;
NextRect: ;
    } /* for i */

    /* Make a final pass over each region in order to Coalesce and set
       extents.x2 and extents.y2 */

    for (j = numRI, rit = ri; --j >= 0; rit++)
    {
	reg = &rit->reg;
	riBox = PIXREGION_END(reg);
	reg->extents.y2 = riBox->y2;
	if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
	Coalesce(reg, rit->prevBand, rit->curBand);
	if (reg->data->numRects == 1) /* keep unions happy below */
	{
	    freeData(reg);
	    reg->data = (region_data_type_t *)NULL;
	}
    }

    /* Step 3: Union all regions into a single region */
    while (numRI > 1)
    {
	int half = numRI/2;
	for (j = numRI & 1; j < (half + (numRI & 1)); j++)
	{
	    reg = &ri[j].reg;
	    hreg = &ri[j+half].reg;
	    if (!pixman_op(reg, reg, hreg, pixman_region_unionO, TRUE, TRUE, pOverlap))
		ret = FALSE;
	    if (hreg->extents.x1 < reg->extents.x1)
		reg->extents.x1 = hreg->extents.x1;
	    if (hreg->extents.y1 < reg->extents.y1)
		reg->extents.y1 = hreg->extents.y1;
	    if (hreg->extents.x2 > reg->extents.x2)
		reg->extents.x2 = hreg->extents.x2;
	    if (hreg->extents.y2 > reg->extents.y2)
		reg->extents.y2 = hreg->extents.y2;
	    freeData(hreg);
	}
	numRI -= half;
	if (!ret)
	    goto bail;
    }
    *badreg = ri[0].reg;
    free(ri);
    good(badreg);
    return ret;
bail:
    for (i = 0; i < numRI; i++)
	freeData(&ri[i].reg);
    free (ri);

    return pixman_break (badreg);
}

/*======================================================================
 * 	    	  Region Subtraction
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_subtractO --
 *	Overlapping band subtraction. x1 is the left-most point not yet
 *	checked.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	region may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static pixman_bool_t
pixman_region_subtractO (
    region_type_t *	region,
    box_type_t *	r1,
    box_type_t *  	  	r1End,
    box_type_t *	r2,
    box_type_t *  	  	r2End,
    int  	y1,
    int  	y2,
    int		*pOverlap)
{
    box_type_t *	pNextRect;
    int  	x1;

    x1 = r1->x1;

    assert(y1<y2);
    assert(r1 != r1End && r2 != r2End);

    pNextRect = PIXREGION_TOP(region);

    do
    {
	if (r2->x2 <= x1)
	{
	    /*
	     * Subtrahend entirely to left of minuend: go to next subtrahend.
	     */
	    r2++;
	}
	else if (r2->x1 <= x1)
	{
	    /*
	     * Subtrahend preceeds minuend: nuke left edge of minuend.
	     */
	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend completely covered: advance to next minuend and
		 * reset left fence to edge of new minuend.
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend now used up since it doesn't extend beyond
		 * minuend
		 */
		r2++;
	    }
	}
	else if (r2->x1 < r1->x2)
	{
	    /*
	     * Left part of subtrahend covers part of minuend: add uncovered
	     * part of minuend to region and skip to next subtrahend.
	     */
	    assert(x1<r2->x1);
	    NEWRECT(region, pNextRect, x1, y1, r2->x1, y2);

	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend used up: advance to new...
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend used up
		 */
		r2++;
	    }
	}
	else
	{
	    /*
	     * Minuend used up: add any remaining piece before advancing.
	     */
	    if (r1->x2 > x1)
		NEWRECT(region, pNextRect, x1, y1, r1->x2, y2);
	    r1++;
	    if (r1 != r1End)
		x1 = r1->x1;
	}
    } while ((r1 != r1End) && (r2 != r2End));

    /*
     * Add remaining minuend rectangles to region.
     */
    while (r1 != r1End)
    {
	assert(x1<r1->x2);
	NEWRECT(region, pNextRect, x1, y1, r1->x2, y2);
	r1++;
	if (r1 != r1End)
	    x1 = r1->x1;
    }
    return TRUE;
}

/*-
 *-----------------------------------------------------------------------
 * pixman_region_subtract --
 *	Subtract regS from regM and leave the result in regD.
 *	S stands for subtrahend, M for minuend and D for difference.
 *
 * Results:
 *	TRUE if successful.
 *
 * Side Effects:
 *	regD is overwritten.
 *
 *-----------------------------------------------------------------------
 */
PIXMAN_EXPORT pixman_bool_t
PREFIX(_subtract) (region_type_t *	regD,
		       region_type_t * 	regM,
		       region_type_t *	regS)
{
    int overlap; /* result ignored */

    good(regM);
    good(regS);
    good(regD);
   /* check for trivial rejects */
    if (PIXREGION_NIL(regM) || PIXREGION_NIL(regS) ||
	!EXTENTCHECK(&regM->extents, &regS->extents))
    {
	if (PIXREGION_NAR (regS))
	    return pixman_break (regD);
	return PREFIX(_copy) (regD, regM);
    }
    else if (regM == regS)
    {
	freeData(regD);
	regD->extents.x2 = regD->extents.x1;
	regD->extents.y2 = regD->extents.y1;
	regD->data = pixman_region_emptyData;
	return TRUE;
    }

    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky substraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    if (!pixman_op(regD, regM, regS, pixman_region_subtractO, TRUE, FALSE, &overlap))
	return FALSE;

    /*
     * Can't alter RegD's extents before we call pixman_op because
     * it might be one of the source regions and pixman_op depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    pixman_set_extents(regD);
    good(regD);
    return TRUE;
}

/*======================================================================
 *	    Region Inversion
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * pixman_region_inverse --
 *	Take a region and a box and return a region that is everything
 *	in the box but not in the region. The careful reader will note
 *	that this is the same as subtracting the region from the box...
 *
 * Results:
 *	TRUE.
 *
 * Side Effects:
 *	newReg is overwritten.
 *
 *-----------------------------------------------------------------------
 */
pixman_bool_t
PIXMAN_EXPORT PREFIX(_inverse) (region_type_t * 	  newReg,       /* Destination region */
		      region_type_t * 	  reg1,         /* Region to invert */
		      box_type_t *     	  invRect) 	/* Bounding box for inversion */
{
    region_type_t	  invReg;   	/* Quick and dirty region made from the
				 * bounding box */
    int	  overlap;	/* result ignored */

    good(reg1);
    good(newReg);
   /* check for trivial rejects */
    if (PIXREGION_NIL(reg1) || !EXTENTCHECK(invRect, &reg1->extents))
    {
	if (PIXREGION_NAR(reg1))
	    return pixman_break (newReg);
	newReg->extents = *invRect;
	freeData(newReg);
	newReg->data = (region_data_type_t *)NULL;
        return TRUE;
    }

    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky substraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    invReg.extents = *invRect;
    invReg.data = (region_data_type_t *)NULL;
    if (!pixman_op(newReg, &invReg, reg1, pixman_region_subtractO, TRUE, FALSE, &overlap))
	return FALSE;

    /*
     * Can't alter newReg's extents before we call pixman_op because
     * it might be one of the source regions and pixman_op depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    pixman_set_extents(newReg);
    good(newReg);
    return TRUE;
}

/*
 *   RectIn(region, rect)
 *   This routine takes a pointer to a region and a pointer to a box
 *   and determines if the box is outside/inside/partly inside the region.
 *
 *   The idea is to travel through the list of rectangles trying to cover the
 *   passed box with them. Anytime a piece of the rectangle isn't covered
 *   by a band of rectangles, partOut is set TRUE. Any time a rectangle in
 *   the region covers part of the box, partIn is set TRUE. The process ends
 *   when either the box has been completely covered (we reached a band that
 *   doesn't overlap the box, partIn is TRUE and partOut is false), the
 *   box has been partially covered (partIn == partOut == TRUE -- because of
 *   the banding, the first time this is true we know the box is only
 *   partially in the region) or is outside the region (we reached a band
 *   that doesn't overlap the box at all and partIn is false)
 */

pixman_region_overlap_t
PIXMAN_EXPORT PREFIX(_contains_rectangle) (region_type_t *  region,
				 box_type_t *     prect)
{
    int	x;
    int	y;
    box_type_t *     pbox;
    box_type_t *     pboxEnd;
    int			partIn, partOut;
    int			numRects;

    good(region);
    numRects = PIXREGION_NUM_RECTS(region);
    /* useful optimization */
    if (!numRects || !EXTENTCHECK(&region->extents, prect))
        return(PIXMAN_REGION_OUT);

    if (numRects == 1)
    {
	/* We know that it must be PIXMAN_REGION_IN or PIXMAN_REGION_PART */
	if (SUBSUMES(&region->extents, prect))
	    return(PIXMAN_REGION_IN);
	else
	    return(PIXMAN_REGION_PART);
    }

    partOut = FALSE;
    partIn = FALSE;

    /* (x,y) starts at upper left of rect, moving to the right and down */
    x = prect->x1;
    y = prect->y1;

    /* can stop when both partOut and partIn are TRUE, or we reach prect->y2 */
    for (pbox = PIXREGION_BOXPTR(region), pboxEnd = pbox + numRects;
         pbox != pboxEnd;
         pbox++)
    {

        if (pbox->y2 <= y)
           continue;    /* getting up to speed or skipping remainder of band */

        if (pbox->y1 > y)
        {
           partOut = TRUE;      /* missed part of rectangle above */
           if (partIn || (pbox->y1 >= prect->y2))
              break;
           y = pbox->y1;        /* x guaranteed to be == prect->x1 */
        }

        if (pbox->x2 <= x)
           continue;            /* not far enough over yet */

        if (pbox->x1 > x)
        {
           partOut = TRUE;      /* missed part of rectangle to left */
           if (partIn)
              break;
        }

        if (pbox->x1 < prect->x2)
        {
            partIn = TRUE;      /* definitely overlap */
            if (partOut)
               break;
        }

        if (pbox->x2 >= prect->x2)
        {
           y = pbox->y2;        /* finished with this band */
           if (y >= prect->y2)
              break;
           x = prect->x1;       /* reset x out to left again */
        }
	else
	{
	    /*
	     * Because boxes in a band are maximal width, if the first box
	     * to overlap the rectangle doesn't completely cover it in that
	     * band, the rectangle must be partially out, since some of it
	     * will be uncovered in that band. partIn will have been set true
	     * by now...
	     */
	    partOut = TRUE;
	    break;
	}
    }

    if (partIn)
    {
	if (y < prect->y2)
	    return PIXMAN_REGION_PART;
	else
	    return PIXMAN_REGION_IN;
    }
    else
    {
	return PIXMAN_REGION_OUT;
    }
}

/* PREFIX(_translate) (region, x, y)
   translates in place
*/

PIXMAN_EXPORT void
PREFIX(_translate) (region_type_t * region, int x, int y)
{
    int x1, x2, y1, y2;
    int nbox;
    box_type_t * pbox;

    good(region);
    region->extents.x1 = x1 = region->extents.x1 + x;
    region->extents.y1 = y1 = region->extents.y1 + y;
    region->extents.x2 = x2 = region->extents.x2 + x;
    region->extents.y2 = y2 = region->extents.y2 + y;
    if (((x1 - SHRT_MIN)|(y1 - SHRT_MIN)|(SHRT_MAX - x2)|(SHRT_MAX - y2)) >= 0)
    {
	if (region->data && (nbox = region->data->numRects))
	{
	    for (pbox = PIXREGION_BOXPTR(region); nbox--; pbox++)
	    {
		pbox->x1 += x;
		pbox->y1 += y;
		pbox->x2 += x;
		pbox->y2 += y;
	    }
	}
	return;
    }
    if (((x2 - SHRT_MIN)|(y2 - SHRT_MIN)|(SHRT_MAX - x1)|(SHRT_MAX - y1)) <= 0)
    {
	region->extents.x2 = region->extents.x1;
	region->extents.y2 = region->extents.y1;
	freeData(region);
	region->data = pixman_region_emptyData;
	return;
    }
    if (x1 < SHRT_MIN)
	region->extents.x1 = SHRT_MIN;
    else if (x2 > SHRT_MAX)
	region->extents.x2 = SHRT_MAX;
    if (y1 < SHRT_MIN)
	region->extents.y1 = SHRT_MIN;
    else if (y2 > SHRT_MAX)
	region->extents.y2 = SHRT_MAX;
    if (region->data && (nbox = region->data->numRects))
    {
	box_type_t * pboxout;

	for (pboxout = pbox = PIXREGION_BOXPTR(region); nbox--; pbox++)
	{
	    pboxout->x1 = x1 = pbox->x1 + x;
	    pboxout->y1 = y1 = pbox->y1 + y;
	    pboxout->x2 = x2 = pbox->x2 + x;
	    pboxout->y2 = y2 = pbox->y2 + y;
	    if (((x2 - SHRT_MIN)|(y2 - SHRT_MIN)|
		 (SHRT_MAX - x1)|(SHRT_MAX - y1)) <= 0)
	    {
		region->data->numRects--;
		continue;
	    }
	    if (x1 < SHRT_MIN)
		pboxout->x1 = SHRT_MIN;
	    else if (x2 > SHRT_MAX)
		pboxout->x2 = SHRT_MAX;
	    if (y1 < SHRT_MIN)
		pboxout->y1 = SHRT_MIN;
	    else if (y2 > SHRT_MAX)
		pboxout->y2 = SHRT_MAX;
	    pboxout++;
	}
	if (pboxout != pbox)
	{
	    if (region->data->numRects == 1)
	    {
		region->extents = *PIXREGION_BOXPTR(region);
		freeData(region);
		region->data = (region_data_type_t *)NULL;
	    }
	    else
		pixman_set_extents(region);
	}
    }
}

PIXMAN_EXPORT void
PREFIX(_reset) (region_type_t *region, box_type_t *box)
{
    good(region);
    assert(box->x1<=box->x2);
    assert(box->y1<=box->y2);
    region->extents = *box;
    freeData(region);
    region->data = (region_data_type_t *)NULL;
}

/* box is "return" value */
PIXMAN_EXPORT int
PREFIX(_contains_point) (region_type_t * region,
			     int x, int y,
			     box_type_t * box)
{
    box_type_t *pbox, *pboxEnd;
    int numRects;

    good(region);
    numRects = PIXREGION_NUM_RECTS(region);
    if (!numRects || !INBOX(&region->extents, x, y))
        return(FALSE);
    if (numRects == 1)
    {
	*box = region->extents;
	return(TRUE);
    }
    for (pbox = PIXREGION_BOXPTR(region), pboxEnd = pbox + numRects;
	 pbox != pboxEnd;
	 pbox++)
    {
        if (y >= pbox->y2)
	   continue;		/* not there yet */
	if ((y < pbox->y1) || (x < pbox->x1))
	   break;		/* missed it */
	if (x >= pbox->x2)
	   continue;		/* not there yet */
	*box = *pbox;
	return(TRUE);
    }
    return(FALSE);
}

PIXMAN_EXPORT int
PREFIX(_not_empty) (region_type_t * region)
{
    good(region);
    return(!PIXREGION_NIL(region));
}

PIXMAN_EXPORT box_type_t *
PREFIX(_extents) (region_type_t * region)
{
    good(region);
    return(&region->extents);
}

/*
    Clip a list of scanlines to a region.  The caller has allocated the
    space.  FSorted is non-zero if the scanline origins are in ascending
    order.
    returns the number of new, clipped scanlines.
*/

PIXMAN_EXPORT pixman_bool_t
PREFIX(_selfcheck) (reg)
    region_type_t * reg;
{
    int i, numRects;

    if ((reg->extents.x1 > reg->extents.x2) ||
	(reg->extents.y1 > reg->extents.y2))
	return FALSE;
    numRects = PIXREGION_NUM_RECTS(reg);
    if (!numRects)
	return ((reg->extents.x1 == reg->extents.x2) &&
		(reg->extents.y1 == reg->extents.y2) &&
		(reg->data->size || (reg->data == pixman_region_emptyData)));
    else if (numRects == 1)
	return (!reg->data);
    else
    {
	box_type_t * pboxP, * pboxN;
	box_type_t box;

	pboxP = PIXREGION_RECTS(reg);
	box = *pboxP;
	box.y2 = pboxP[numRects-1].y2;
	pboxN = pboxP + 1;
	for (i = numRects; --i > 0; pboxP++, pboxN++)
	{
	    if ((pboxN->x1 >= pboxN->x2) ||
		(pboxN->y1 >= pboxN->y2))
		return FALSE;
	    if (pboxN->x1 < box.x1)
	        box.x1 = pboxN->x1;
	    if (pboxN->x2 > box.x2)
		box.x2 = pboxN->x2;
	    if ((pboxN->y1 < pboxP->y1) ||
		((pboxN->y1 == pboxP->y1) &&
		 ((pboxN->x1 < pboxP->x2) || (pboxN->y2 != pboxP->y2))))
		return FALSE;
	}
	return ((box.x1 == reg->extents.x1) &&
		(box.x2 == reg->extents.x2) &&
		(box.y1 == reg->extents.y1) &&
		(box.y2 == reg->extents.y2));
    }
}

PIXMAN_EXPORT pixman_bool_t
PREFIX(_init_rects) (region_type_t *region,
		     box_type_t *boxes, int count)
{
    int overlap;

    /* if it's 1, then we just want to set the extents, so call
     * the existing method. */
    if (count == 1) {
       PREFIX(_init_rect) (region,
                               boxes[0].x1,
                               boxes[0].y1,
                               boxes[0].x2 - boxes[0].x1,
                               boxes[0].y2 - boxes[0].y1);
       return TRUE;
    }

    PREFIX(_init) (region);

    /* if it's 0, don't call pixman_rect_alloc -- 0 rectangles is
     * a special case, and causing pixman_rect_alloc would cause
     * us to leak memory (because the 0-rect case should be the
     * static pixman_region_emptyData data).
     */
    if (count == 0)
        return TRUE;

    if (!pixman_rect_alloc(region, count))
	return FALSE;

    /* Copy in the rects */
    memcpy (PIXREGION_RECTS(region), boxes, sizeof(box_type_t) * count);
    region->data->numRects = count;

    /* Validate */
    region->extents.x1 = region->extents.x2 = 0;
    return validate (region, &overlap);
}

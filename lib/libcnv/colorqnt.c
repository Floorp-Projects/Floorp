#include "xp_core.h" //used to make library compile faster on win32 do not ifdef this or it wont work

/* taken from libppm3.c - ppm utility library part 3
**
** Colormap routines.
**
** Copyright (C) 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/
#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <process.h>
#include <search.h>        
#include <stdlib.h>        
#include "xp_core.h"/*defines of int32, etc.*/
#include "xp_mem.h"/*defines of XP_ALLOC, etc.*/
#include "xp_str.h"/*defines of XP_MEMCPY, etc.*/
#include "xpassert.h"
#include "xp_qsort.h"
#include "ntypes.h" /* for MWContext to include libcnv.h*/

#include "libcnv.h"
#include "pbmplus.h"
#include "ppm.h"
#include "ppmcmap.h"
#include "colorqnt.h"

#define LARGE_NORM 1
#define REP_AVERAGE_COLORS 1

#define MAXCOLORS 256

#define FLOYD_DEFAULT 1/*use floyd algorithm*/
#define FS_SCALE 1024

typedef struct box* box_vector;
struct box
    {
    int16 ind;
    int16 colors;
    int32 sum;
    };

static colorhist_vector mediancut ARGS(( colorhist_vector chv, int16 colors, int32 sum, pixval maxval, int16 newcolors ));
static int redcompare ARGS(( const colorhist_vector ch1, const colorhist_vector ch2 ));
static int greencompare ARGS(( const colorhist_vector ch1, const colorhist_vector ch2 ));
static int bluecompare ARGS(( const colorhist_vector ch1, const colorhist_vector ch2 ));
static int sumcompare ARGS(( const box_vector b1, const box_vector b2 ));

/*used for quick sort of color table*/
typedef int (* qsortprototype)(const void *, const void *);

/*EFFECTS:modifies imagearray to reflect colors in colorhistogram
 MODIFIES:imagearray, pchv,maxcolorvalue,colors
  RETURNS:CONV_OK if success
    INPUT:
    imagearray= RGB tripples
    imagewidth= width of image
    imageheight=height of image
    maxcolorvalue=passed in as maximum value for each R,G or B 
    colors= valid pointer to int16
    maxcolorvalue= valid pointer to int16
   OUTPUT:
    maxcolorvalue=leaves with what was assigned as the maximum value
    colors= number of colors in the mapping
    pchv= leaves pointing to the new color table. calling function reponsible for deleting!
SPECIAL NOTE!
the returned color table is in the range of 0 to maxcolorvalue.
you must divide my maxcolorvalue and multiply times the input maxcolorvalue to get 
colors in the range of 0-MAXCOLORS-1
*/
CONVERT_IMAGERESULT
quantize_colors(CONVERT_IMG_ARRAY imagearray,int16 imagewidth,int16 imageheight,int16 *maxcolorvalue,int16 *colors,colorhist_vector *pchv)
{
    pixval newmaxval=0;
    int16 row=0;
    colorhist_vector chv=NULL;
    colorhist_vector colormap=NULL;
    pixel *pP=NULL;
    pixval maxval=(pixval)(MAXCOLORS-1);
    colorhash_table cht=NULL;
    int32* thisrerr=NULL;
    int32* nextrerr=NULL;
    int32* thisgerr=NULL;
    int32* nextgerr=NULL;
    int32* thisberr=NULL;
    int32* nextberr=NULL;
    int32* temperr=NULL;
    int16 usehash,floyd=FLOYD_DEFAULT;
    int16 fs_direction;
    register int16 col, limitcol;
    register int32 sr, sg, sb, err;
    register int16 ind;
    pixel** copy = 0;

    *pchv=NULL;
    XP_ASSERT(colors);
    XP_ASSERT(maxcolorvalue);
    /*
    ** Step 1: attempt to make a histogram of the colors, unclustered.
    ** If at first we don't succeed, lower maxval to increase color
    ** coherence and try again.  This will eventually terminate, with
    ** maxval at worst 15, since 32^3 is approximately MAXCOLORS.
    */
    /* Make a copy of the image pixels */
    copy = XP_ALLOC(sizeof(pixel*) * imageheight);
    {
        int i;
        pixel* pixelRow;
        pixel* sourceRow;
        for(i = 0; i < imageheight; i++){
            pixelRow = XP_ALLOC(imagewidth*sizeof(pixel));
            copy[i] = pixelRow;
            sourceRow = (pixel*) imagearray[i];
            /* XP_MEMCPY(pixelRow, sourceRow, sizeof(pixel)*imagewidth); */
            {
                int j;
                for(j = 0; j < imagewidth; j++){
                    pixelRow[j] = sourceRow[j];
                }
            }
        }
    }

    for ( ; ; )
    {
        chv = ppm_computecolorhist(
            (pixel **)copy, imagewidth, imageheight, MAXCOLORS, colors );
        if ( chv != (colorhist_vector) 0 )
            break;
        newmaxval = maxval / 2;
        for ( row = 0; row < imageheight; ++row )
            for ( col = 0, pP = (pixel *)copy[row]; col < imagewidth; ++col, ++pP ){
                PPM_DEPTH( *pP, *pP, maxval, newmaxval );
            }
        maxval=newmaxval;
    }
    /* Free copy of image */
    {
        int i;
        for(i = 0; i < imageheight; i++){
            XP_FREE(copy[i]);
        }
        XP_FREE(copy);
    }

    /* we now have the maximum # of colors*/
    /*
    ** Step 2: apply median-cut to histogram, making the new colormap.
    */
    colormap = mediancut( chv, (int16)*colors, (int32)(imageheight * imagewidth), (pixval)maxval, (int16)*maxcolorvalue );
    free( chv );
    { /* Crank up colors in color map */
        int i;
        for(i = 0; i < *colors; i++ ) {
            PPM_DEPTH( colormap[i].color, colormap[i].color, maxval, MAXCOLORS-1 );
        }
        maxval = MAXCOLORS-1;
    }
    /*
    ** Step 3: map the colors in the image to their closest match in the
    ** new colormap, and write 'em out.
    */
    cht = ppm_alloccolorhash( );
    usehash = 1;
    if ( floyd )
    {
        /* Initialize Floyd-Steinberg error vectors. */
        thisrerr = (int32*) malloc( (imagewidth + 2)* sizeof(int32) );
        nextrerr = (int32*) malloc( (imagewidth + 2)* sizeof(int32) );
        thisgerr = (int32*) malloc( (imagewidth + 2)* sizeof(int32) );
        nextgerr = (int32*) malloc( (imagewidth + 2)* sizeof(int32) );
        thisberr = (int32*) malloc( (imagewidth + 2)* sizeof(int32) );
        nextberr = (int32*) malloc( (imagewidth + 2)* sizeof(int32) );
        if (!nextberr||!thisberr||!nextgerr||!thisgerr||!nextrerr||!thisrerr)
            return CONVERR_OUTOFMEMORY; /*memory allocation failed*/
        srandom( (int16) ( time( 0 ) ^ getpid( ) ) );
        for ( col = 0; col < imagewidth + 2; ++col )
        {
            thisrerr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
            thisgerr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
            thisberr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
            /* (random errors in [-1 .. 1]) */
        }
        fs_direction = 1;
    }
    for ( row = 0; row < imageheight; ++row )
    {
        if ( floyd )
            for ( col = 0; col < imagewidth + 2; ++col )
                nextrerr[col] = nextgerr[col] = nextberr[col] = 0;
        if ( ( ! floyd ) || fs_direction )
        {
            col = 0;
            limitcol = imagewidth;
            pP = (pixel *)imagearray[row];
        }
        else
        {
            col = imagewidth - 1;
            limitcol = -1;
            pP = &((pixel **)imagearray)[row][col]; /*(pixel *)&(imagearray[row][col*3]); times 3 because a CONV_IMAGE_ARRAY IS A char ** not a pixel ** */
        }
        do
        {
            if ( floyd )
            {
                /* Use Floyd-Steinberg errors to adjust actual color. */
                sr = PPM_GETR(*pP) + thisrerr[col + 1] / FS_SCALE;
                sg = PPM_GETG(*pP) + thisgerr[col + 1] / FS_SCALE;
                sb = PPM_GETB(*pP) + thisberr[col + 1] / FS_SCALE;
                if ( sr < 0 ) sr = 0;
                else if ( sr > maxval ) sr = maxval;
                if ( sg < 0 ) sg = 0;
                else if ( sg > maxval ) sg = maxval;
                if ( sb < 0 ) sb = 0;
                else if ( sb > maxval ) sb = maxval;
                PPM_ASSIGN( *pP, (pixval)sr, (pixval)sg, (pixval)sb );
            }

            /* Check hash table to see if we have already matched this color. */
            ind = ppm_lookupcolor( cht, pP );
            if ( ind == -1 )
            { /* No; search colormap for closest match. */
                register int16 i, r1, g1, b1, r2, g2, b2;
                register int32 dist, newdist;
                r1 = PPM_GETR( *pP );
                g1 = PPM_GETG( *pP );
                b1 = PPM_GETB( *pP );
                dist = 2000000000;
                for ( i = 0; i < *maxcolorvalue; ++i )
                {
                    r2 = PPM_GETR( colormap[i].color );
                    g2 = PPM_GETG( colormap[i].color );
                    b2 = PPM_GETB( colormap[i].color );
                    newdist = ( r1 - r2 ) * ( r1 - r2 ) +
                        ( g1 - g2 ) * ( g1 - g2 ) +
                        ( b1 - b2 ) * ( b1 - b2 );
                    if ( newdist < dist )
                    {
                        ind = i;
                        dist = newdist;
                    }
                }
                if ( usehash )
                {
                    if ( ppm_addtocolorhash( cht, pP, ind ) < 0 )
                    {
                        usehash = 0;
                    }
                }
            }

            if ( floyd )
            {
                /* Propagate Floyd-Steinberg error terms. */
                if ( fs_direction )
                {
                    err = ( sr - (int32) PPM_GETR( colormap[ind].color ) ) * FS_SCALE;
                    thisrerr[col + 2] += ( err * 7 ) / 16;
                    nextrerr[col    ] += ( err * 3 ) / 16;
                    nextrerr[col + 1] += ( err * 5 ) / 16;
                    nextrerr[col + 2] += ( err     ) / 16;
                    err = ( sg - (int32) PPM_GETG( colormap[ind].color ) ) * FS_SCALE;
                    thisgerr[col + 2] += ( err * 7 ) / 16;
                    nextgerr[col    ] += ( err * 3 ) / 16;
                    nextgerr[col + 1] += ( err * 5 ) / 16;
                    nextgerr[col + 2] += ( err     ) / 16;
                    err = ( sb - (int32) PPM_GETB( colormap[ind].color ) ) * FS_SCALE;
                    thisberr[col + 2] += ( err * 7 ) / 16;
                    nextberr[col    ] += ( err * 3 ) / 16;
                    nextberr[col + 1] += ( err * 5 ) / 16;
                    nextberr[col + 2] += ( err     ) / 16;
                }
                else
                {
                    err = ( sr - (int32) PPM_GETR( colormap[ind].color ) ) * FS_SCALE;
                    thisrerr[col    ] += ( err * 7 ) / 16;
                    nextrerr[col + 2] += ( err * 3 ) / 16;
                    nextrerr[col + 1] += ( err * 5 ) / 16;
                    nextrerr[col    ] += ( err     ) / 16;
                    err = ( sg - (int32) PPM_GETG( colormap[ind].color ) ) * FS_SCALE;
                    thisgerr[col    ] += ( err * 7 ) / 16;
                    nextgerr[col + 2] += ( err * 3 ) / 16;
                    nextgerr[col + 1] += ( err * 5 ) / 16;
                    nextgerr[col    ] += ( err     ) / 16;
                    err = ( sb - (int32) PPM_GETB( colormap[ind].color ) ) * FS_SCALE;
                    thisberr[col    ] += ( err * 7 ) / 16;
                    nextberr[col + 2] += ( err * 3 ) / 16;
                    nextberr[col + 1] += ( err * 5 ) / 16;
                    nextberr[col    ] += ( err     ) / 16;
                }
            }

            *pP = colormap[ind].color;

            if ( ( ! floyd ) || fs_direction )
            {
                ++col;
                ++pP;
            }
            else
            {
                --col;
                --pP;
            }
        }
        while ( col != limitcol );

        if ( floyd )
        {
            temperr = thisrerr;
            thisrerr = nextrerr;
            nextrerr = temperr;
            temperr = thisgerr;
            thisgerr = nextgerr;
            nextgerr = temperr;
            temperr = thisberr;
            thisberr = nextberr;
            nextberr = temperr;
            fs_direction = ! fs_direction;
        }

/*        ppm_writeppmrow( stdout, (pixel *)imagearray[row], imagewidth, maxval, 0 );*/
    }
    ppm_freecolorhash(cht);
    free(thisrerr);
    free(nextrerr);
    free(thisgerr);
    free(nextgerr);
    free(thisberr);
    free(nextberr);
    (*maxcolorvalue)=maxval;
    (*pchv)=colormap;
    return CONV_OK;
}

/*
** Here is the fun part, the median-cut colormap generator.  This is based
** on Paul Heckbert's paper "Color Image Quantization for Frame Buffer
** Display", SIGGRAPH '82 Proceedings, page 297.
*/

static colorhist_vector
mediancut( colorhist_vector chv, int16 colors, int32 sum, pixval maxval, int16 newcolors )
{
    colorhist_vector colormap;
    box_vector bv;
    register int16 bi, i;
    int16 boxes;

    bv = (box_vector) malloc( sizeof(struct box) * newcolors );
    colormap = (colorhist_vector) malloc( sizeof(struct colorhist_item) * newcolors );
    if ( bv == (box_vector) 0 || colormap == (colorhist_vector) 0 )
        return NULL;
    for ( i = 0; i < newcolors; ++i )
        PPM_ASSIGN( colormap[i].color, 0, 0, 0 );

        /*
        ** Set up the initial box.
    */
    bv[0].ind = 0;
    bv[0].colors = colors;
    bv[0].sum = sum;
    boxes = 1;

    /*
    ** Main loop: split boxes until we have enough.
    */
    while ( boxes < newcolors )
    {
        register int32 indx, clrs;
        int32 sm;
        register int32 minr, maxr, ming, maxg, minb, maxb, v;
        int32 halfsum, lowersum;

        /*
        ** Find the first splittable box.
        */
        for ( bi = 0; bi < boxes; ++bi )
            if ( bv[bi].colors >= 2 )
            break;
        if ( bi == boxes )
            break;	/* ran out of colors! */
        indx = bv[bi].ind;
        clrs = bv[bi].colors;
        sm = bv[bi].sum;

        /*
        ** Go through the box finding the minimum and maximum of each
        ** component - the boundaries of the box.
        */
        minr = maxr = PPM_GETR( chv[indx].color );
        ming = maxg = PPM_GETG( chv[indx].color );
        minb = maxb = PPM_GETB( chv[indx].color );
        for ( i = 1; i < clrs; ++i )
        {
            v = PPM_GETR( chv[indx + i].color );
            if ( v < minr ) minr = v;
            if ( v > maxr ) maxr = v;
            v = PPM_GETG( chv[indx + i].color );
            if ( v < ming ) ming = v;
            if ( v > maxg ) maxg = v;
            v = PPM_GETB( chv[indx + i].color );
            if ( v < minb ) minb = v;
            if ( v > maxb ) maxb = v;
        }

        /*
        ** Find the largest dimension, and sort by that component.  I have
        ** included two methods for determining the "largest" dimension;
        ** first by simply comparing the range in RGB space, and second
        ** by transforming into luminosities before the comparison.  You
        ** can switch which method is used by switching the commenting on
        ** the LARGE_ defines at the beginning of this source file.
        */
#ifdef LARGE_NORM
        if ( maxr - minr >= maxg - ming && maxr - minr >= maxb - minb )
            XP_QSORT(
            (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
            (qsortprototype)redcompare );
        else if ( maxg - ming >= maxb - minb )
            XP_QSORT(
            (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
            (qsortprototype)greencompare );
        else
            XP_QSORT(
            (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
            (qsortprototype)bluecompare );
#endif /*LARGE_NORM*/
#ifdef LARGE_LUM
        {
            pixel p;
            float rl, gl, bl;

            PPM_ASSIGN(p, maxr - minr, 0, 0);
            rl = PPM_LUMIN(p);
            PPM_ASSIGN(p, 0, maxg - ming, 0);
            gl = PPM_LUMIN(p);
            PPM_ASSIGN(p, 0, 0, maxb - minb);
            bl = PPM_LUMIN(p);

            if ( rl >= gl && rl >= bl )
                XP_QSORT(
                (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                redcompare );
            else if ( gl >= bl )
                XP_QSORT(
                (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                greencompare );
            else
                XP_QSORT(
                (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                bluecompare );
        }
#endif /*LARGE_LUM*/
        
        /*
        ** Now find the median based on the counts, so that about half the
        ** pixels (not colors, pixels) are in each subdivision.
        */
        lowersum = chv[indx].value;
        halfsum = sm / 2;
        for ( i = 1; i < clrs - 1; ++i )
        {
            if ( lowersum >= halfsum )
                break;
            lowersum += chv[indx + i].value;
        }

        /*
        ** Split the box, and sort to bring the biggest boxes to the top.
        */
        bv[bi].colors = i;
        bv[bi].sum = lowersum;
        bv[boxes].ind = indx + i;
        bv[boxes].colors = clrs - i;
        bv[boxes].sum = sm - lowersum;
        ++boxes;
        XP_QSORT( (char*) bv, boxes, sizeof(struct box), (qsortprototype)sumcompare );
    }

    /*
    ** Ok, we've got enough boxes.  Now choose a representative color for
    ** each box.  There are a number of possible ways to make this choice.
    ** One would be to choose the center of the box; this ignores any structure
    ** within the boxes.  Another method would be to average all the colors in
    ** the box - this is the method specified in Heckbert's paper.  A third
    ** method is to average all the pixels in the box.  You can switch which
    ** method is used by switching the commenting on the REP_ defines at
    ** the beginning of this source file.
    */
    for ( bi = 0; bi < boxes; ++bi )
    {
#ifdef REP_CENTER_BOX
        register int16 indx = bv[bi].ind;
        register int16 clrs = bv[bi].colors;
        register int16 minr, maxr, ming, maxg, minb, maxb, v;

        minr = maxr = PPM_GETR( chv[indx].color );
        ming = maxg = PPM_GETG( chv[indx].color );
        minb = maxb = PPM_GETB( chv[indx].color );
        for ( i = 1; i < clrs; ++i )
        {
            v = PPM_GETR( chv[indx + i].color );
            minr = min( minr, v );
            maxr = max( maxr, v );
            v = PPM_GETG( chv[indx + i].color );
            ming = min( ming, v );
            maxg = max( maxg, v );
            v = PPM_GETB( chv[indx + i].color );
            minb = min( minb, v );
            maxb = max( maxb, v );
        }
        PPM_ASSIGN(
            colormap[bi].color, ( minr + maxr ) / 2, ( ming + maxg ) / 2,
            ( minb + maxb ) / 2 );
#endif /*REP_CENTER_BOX*/
#ifdef REP_AVERAGE_COLORS
        register int16 indx = bv[bi].ind;
        register int16 clrs = bv[bi].colors;
        register int32 r = 0, g = 0, b = 0;

        for ( i = 0; i < clrs; ++i )
        {
            r += PPM_GETR( chv[indx + i].color );
            g += PPM_GETG( chv[indx + i].color );
            b += PPM_GETB( chv[indx + i].color );
        }
        r = r / clrs;
        g = g / clrs;
        b = b / clrs;
        PPM_ASSIGN( colormap[bi].color, (pixval)r, (pixval)g, (pixval)b );
#endif /*REP_AVERAGE_COLORS*/
#ifdef REP_AVERAGE_PIXELS
        register int16 indx = bv[bi].ind;
        register int16 clrs = bv[bi].colors;
        register int32 r = 0, g = 0, b = 0, sum = 0;

        for ( i = 0; i < clrs; ++i )
        {
            r += PPM_GETR( chv[indx + i].color ) * chv[indx + i].value;
            g += PPM_GETG( chv[indx + i].color ) * chv[indx + i].value;
            b += PPM_GETB( chv[indx + i].color ) * chv[indx + i].value;
            sum += chv[indx + i].value;
        }
        r = r / sum;
        if ( r > maxval ) r = maxval;	/* avoid math errors */
        g = g / sum;
        if ( g > maxval ) g = maxval;
        b = b / sum;
        if ( b > maxval ) b = maxval;
        PPM_ASSIGN( colormap[bi].color, r, g, b );
#endif /*REP_AVERAGE_PIXELS*/
    }

    /*
    ** All done.
    */
    free (bv);
    return colormap;
}

static int
redcompare( ch1, ch2 )
const colorhist_vector ch1, ch2;
{
    return (int16) PPM_GETR( ch1->color ) - (int16) PPM_GETR( ch2->color );
}

static int
greencompare( ch1, ch2 )
const colorhist_vector ch1, ch2;
{
    return (int16) PPM_GETG( ch1->color ) - (int16) PPM_GETG( ch2->color );
}

static int
bluecompare( ch1, ch2 )
const colorhist_vector ch1, ch2;
{
    return (int16) PPM_GETB( ch1->color ) - (int16) PPM_GETB( ch2->color );
}

static int
sumcompare( b1, b2 )
const box_vector b1, b2;
{
    return b2->sum - b1->sum;
}

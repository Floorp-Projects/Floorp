/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xp.h"


Bool
lo_is_location_in_poly(int32 x, int32 y, int32 *coords, int32 coord_cnt)
{
    int32 totalv = 0;
    int32 totalc = 0;
    int32 intersects = 0;
    int32 wherex = x;
    int32 wherey = y;
    int32 xval, yval;
    int32 *pointer;
    int32 *end;

    totalv = coord_cnt / 2;
    totalc = totalv * 2;

    xval = coords[totalc - 2];
    yval = coords[totalc - 1];
    end = &coords[totalc];

    pointer = coords + 1;

    if ((yval >= wherey) != (*pointer >= wherey)) 
        if ((xval >= wherex) == (*coords >= wherex)) 
	    intersects += (xval >= wherex);
        else 
            intersects += (xval - (yval - wherey) * 
                          (*coords - xval) /
                          (*pointer - yval)) >= wherex;

    while(pointer < end)  {
        yval = *pointer;
        pointer += 2;

        if(yval >= wherey)  {
	   while((pointer < end) && (*pointer >= wherey))
		pointer+=2;
	   if (pointer >= end)
		break;
           if( (*(pointer-3) >= wherex) == (*(pointer-1) >= wherex) )
	        intersects += (*(pointer-3) >= wherex);
 	    else
		intersects += (*(pointer-3) - (*(pointer-2) - wherey) *
                              (*(pointer-1) - *(pointer-3)) /
                              (*pointer - *(pointer - 2))) >= wherex;

        }  else  {
	   while((pointer < end) && (*pointer < wherey))
		pointer+=2;
	   if (pointer >= end)
		break;
           if( (*(pointer-3) >= wherex) == (*(pointer-1) >= wherex) )
		intersects += (*(pointer-3) >= wherex);
 	   else
	        intersects += (*(pointer-3) - (*(pointer-2) - wherey) *
                              (*(pointer-1) - *(pointer-3)) /
                              (*pointer - *(pointer - 2))) >= wherex;
        }  
    }
    if (intersects & 1)
        return (TRUE);
    else
        return (FALSE);
}


/* #ifndef NO_TAB_NAVIGATION  */
Bool
lo_find_location_in_poly(int32 *xx, int32 *yy, int32 *coords, int32 coord_cnt)
{
	int32  x1, y1, x2, y2, px1, py1, px2, py2;
	int32	ii;

	/* get a edge of the polygon */
	x1 =  y1 = x2 = y2 = 0;
	for( ii = 0; (ii <= coord_cnt-4) && (x1==x2) && (y1==y2); ii+=2 ) { 
		x1 = coords[ii];
		y1 = coords[ii+1];
		x2 = coords[ii+2];
		y2 = coords[ii+3];

		if( (x1!=x2) || (y1!=y2) ) {
			/* got a good edge, try it. */
			/* get a point on each side of the edge, 2 pixels away, not too close */
			px1 = ( x1 + x2 ) / 2 + 2;
			px2 = px1 - 4;
			py1 = ( y1 + y2 ) / 2 + 2;
			py2 = py1 - 4;

			if( lo_is_location_in_poly( px1, py1, coords, coord_cnt) ) {
				*xx = px1;
				*yy = py1;
				return( TRUE );
			}

			if( lo_is_location_in_poly( px2, py2, coords, coord_cnt) ) {
				*xx = px2;
				*yy = py2;
				return( TRUE );
			}
		}

	}


	return( FALSE );
}
/* NO_TAB_NAVIGATION */


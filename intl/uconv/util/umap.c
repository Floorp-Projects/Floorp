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
/* #include "PRIntlpriv.h" */
#include "unicpriv.h" 


typedef PRUint16 (* MapFormatFunc)(PRUint16 in,uTable *uT,uMapCell *cell);
typedef PRBool (* HitFormateFunc)(PRUint16 in,uMapCell *cell);


PRIVATE PRBool uHitFormate0(PRUint16 in,uMapCell *cell);
PRIVATE PRBool uHitFormate1(PRUint16 in,uMapCell *cell);
PRIVATE PRBool uHitFormate2(PRUint16 in,uMapCell *cell);
PRIVATE PRUint16 uMapFormate0(PRUint16 in,uTable *uT,uMapCell *cell);
PRIVATE PRUint16 uMapFormate1(PRUint16 in,uTable *uT,uMapCell *cell);
PRIVATE PRUint16 uMapFormate2(PRUint16 in,uTable *uT,uMapCell *cell);


PRIVATE uMapCell *uGetMapCell(uTable *uT, PRInt16 item);
PRIVATE char uGetFormat(uTable *uT, PRInt16 item);


/*=================================================================================

=================================================================================*/
PRIVATE MapFormatFunc m_map[uNumFormatTag] =
{
	uMapFormate0,
	uMapFormate1,
	uMapFormate2,
};

/*=================================================================================

=================================================================================*/
PRIVATE HitFormateFunc m_hit[uNumFormatTag] =
{
	uHitFormate0,
	uHitFormate1,
	uHitFormate2,
};

/*
	Need more work
*/
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uHit(unsigned char format, PRUint16 in,uMapCell *cell)
{
	return 	(* m_hit[format])((in),(cell));
}
/*=================================================================================

=================================================================================*/
/*	
	Switch to Macro later for performance
	
#define	uHit(format,in,cell) 		(* m_hit[format])((in),(cell))
*/
/*=================================================================================

=================================================================================*/
PRIVATE PRUint16 uMap(unsigned char format, PRUint16 in,uTable *uT,uMapCell *cell)
{
	return 	(* m_map[format])((in),(uT),(cell));
}
/* 	
	Switch to Macro later for performance
	
#define uMap(format,in,cell) 		(* m_map[format])((in),(cell))
*/

/*=================================================================================

=================================================================================*/
/*	
	Switch to Macro later for performance
*/
PRIVATE uMapCell *uGetMapCell(uTable *uT, PRInt16 item)
{
	return ((uMapCell *)(((PRUint16 *)uT) + uT->offsetToMapCellArray) + item) ;
}
/*=================================================================================

=================================================================================*/
/*	
	Switch to Macro later for performance
*/
PRIVATE char uGetFormat(uTable *uT, PRInt16 item)
{
	return (((((PRUint16 *)uT) + uT->offsetToFormatArray)[ item >> 2 ]
		>> (( item % 4 ) << 2)) & 0x0f);
}
/*=================================================================================

=================================================================================*/
MODULE_PRIVATE PRBool uMapCode(uTable *uT, PRUint16 in, PRUint16* out)
{
	PRBool done = PR_FALSE;
	PRUint16 itemOfList = uT->itemOfList;
	PRUint16 i;
	*out = NOMAPPING;
	for(i=0;i<itemOfList;i++)
	{
		uMapCell* uCell;
		char format = uGetFormat(uT,i);
		uCell = uGetMapCell(uT,i);
		if(uHit(format, in, uCell))
		{
			*out = uMap(format, in, uT,uCell);
			done = PR_TRUE;
			break;
		}
	}
	return ( done && (*out != NOMAPPING));
}


/*
	member function
*/
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uHitFormate0(PRUint16 in,uMapCell *cell)
{
	return ( (in >= cell->fmt.format0.srcBegin) &&
			     (in <= cell->fmt.format0.srcEnd) ) ;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uHitFormate1(PRUint16 in,uMapCell *cell)
{
	return  uHitFormate0(in,cell);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uHitFormate2(PRUint16 in,uMapCell *cell)
{
	return (in == cell->fmt.format2.srcBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRUint16 uMapFormate0(PRUint16 in,uTable *uT,uMapCell *cell)
{
	return ((in - cell->fmt.format0.srcBegin) + cell->fmt.format0.destBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRUint16 uMapFormate1(PRUint16 in,uTable *uT,uMapCell *cell)
{
	return (*(((PRUint16 *)uT) + uT->offsetToMappingTable
		+ cell->fmt.format1.mappingOffset + in - cell->fmt.format1.srcBegin));
}
/*=================================================================================

=================================================================================*/
PRIVATE PRUint16 uMapFormate2(PRUint16 in,uTable *uT,uMapCell *cell)
{
	return (cell->fmt.format2.destBegin);
}


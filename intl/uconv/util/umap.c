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


typedef PRUint16 (* MapFormatFunc)(PRUint16 in,const uTable *uT,const uMapCell *cell);
typedef PRBool (* HitFormateFunc)(PRUint16 in,const uMapCell *cell);


PRIVATE PRBool uHitFormate0(PRUint16 in,const uMapCell *cell);
PRIVATE PRBool uHitFormate1(PRUint16 in,const uMapCell *cell);
PRIVATE PRBool uHitFormate2(PRUint16 in,const uMapCell *cell);
PRIVATE PRUint16 uMapFormate0(PRUint16 in,const uTable *uT,const uMapCell *cell);
PRIVATE PRUint16 uMapFormate1(PRUint16 in,const uTable *uT,const uMapCell *cell);
PRIVATE PRUint16 uMapFormate2(PRUint16 in,const uTable *uT,const uMapCell *cell);


PRIVATE const uMapCell *uGetMapCell(const uTable *uT, PRInt16 item);
PRIVATE char uGetFormat(const uTable *uT, PRInt16 item);


/*=================================================================================

=================================================================================*/
PRIVATE const MapFormatFunc m_map[uNumFormatTag] =
{
	uMapFormate0,
	uMapFormate1,
	uMapFormate2,
};

/*=================================================================================

=================================================================================*/
PRIVATE const HitFormateFunc m_hit[uNumFormatTag] =
{
	uHitFormate0,
	uHitFormate1,
	uHitFormate2,
};

#define	uHit(format,in,cell) 		(* m_hit[(format)])((in),(cell))
#define uMap(format,in,uT,cell) 	(* m_map[(format)])((in),(uT),(cell))
#define uGetMapCell(uT, item) ((uMapCell *)(((PRUint16 *)uT) + (uT)->offsetToMapCellArray) + (item))
#define uGetFormat(uT, item) (((((PRUint16 *)uT) + (uT)->offsetToFormatArray)[(item)>> 2 ] >> (((item)% 4 ) << 2)) & 0x0f)

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
		const uMapCell* uCell;
		PRInt8 format = uGetFormat(uT,i);
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
PRIVATE PRBool uHitFormate0(PRUint16 in,const uMapCell *cell)
{
	return ( (in >= cell->fmt.format0.srcBegin) &&
			     (in <= cell->fmt.format0.srcEnd) ) ;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uHitFormate1(PRUint16 in,const uMapCell *cell)
{
	return  uHitFormate0(in,cell);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uHitFormate2(PRUint16 in,const uMapCell *cell)
{
	return (in == cell->fmt.format2.srcBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRUint16 uMapFormate0(PRUint16 in,const uTable *uT,const uMapCell *cell)
{
	return ((in - cell->fmt.format0.srcBegin) + cell->fmt.format0.destBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE PRUint16 uMapFormate1(PRUint16 in,const uTable *uT,const uMapCell *cell)
{
	return (*(((PRUint16 *)uT) + uT->offsetToMappingTable
		+ cell->fmt.format1.mappingOffset + in - cell->fmt.format1.srcBegin));
}
/*=================================================================================

=================================================================================*/
PRIVATE PRUint16 uMapFormate2(PRUint16 in,const uTable *uT,const uMapCell *cell)
{
	return (cell->fmt.format2.destBegin);
}


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
#include "intlpriv.h"
#include "unicpriv.h"

typedef uint16 (* MapFormatFunc)(uint16 in,uTable *uTable,uMapCell *cell);
typedef XP_Bool (* HitFormateFunc)(uint16 in,uMapCell *cell);
typedef void (* IterateFormatFunc)(uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context);

PRIVATE XP_Bool uHitFormate0(uint16 in,uMapCell *cell);
PRIVATE XP_Bool uHitFormate1(uint16 in,uMapCell *cell);
PRIVATE XP_Bool uHitFormate2(uint16 in,uMapCell *cell);
PRIVATE uint16 uMapFormate0(uint16 in,uTable *uTable,uMapCell *cell);
PRIVATE uint16 uMapFormate1(uint16 in,uTable *uTable,uMapCell *cell);
PRIVATE uint16 uMapFormate2(uint16 in,uTable *uTable,uMapCell *cell);
PRIVATE void uIterateFormate0(uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context);
PRIVATE void uIterateFormate1(uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context);
PRIVATE void uIterateFormate2(uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context);
PRIVATE uMapCell *uGetMapCell(uTable *uTable, int16 item);
PRIVATE char uGetFormat(uTable *uTable, int16 item);


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
PRIVATE IterateFormatFunc m_iterate[uNumFormatTag] =
{
	uIterateFormate0,
	uIterateFormate1,
	uIterateFormate2,
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
PRIVATE XP_Bool uHit(unsigned char format, uint16 in,uMapCell *cell)
{
	return 	(* m_hit[format])((in),(cell));
}
/*=================================================================================

=================================================================================*/
PRIVATE void uCellIterate(unsigned char format, uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context)
{
	(* m_iterate[format])((uTable),(cell),(callback),(context));
}
/*	
	Switch to Macro later for performance
	
#define	uHit(format,in,cell) 		(* m_hit[format])((in),(cell))
*/
/*=================================================================================

=================================================================================*/
PRIVATE uint16 uMap(unsigned char format, uint16 in,uTable *uTable,uMapCell *cell)
{
	return 	(* m_map[format])((in),(uTable),(cell));
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
PRIVATE uMapCell *uGetMapCell(uTable *uTable, int16 item)
{
	return ((uMapCell *)(((uint16 *)uTable) + uTable->offsetToMapCellArray) + item) ;
}
/*=================================================================================

=================================================================================*/
/*	
	Switch to Macro later for performance
*/
PRIVATE char uGetFormat(uTable *uTable, int16 item)
{
	return (((((uint16 *)uTable) + uTable->offsetToFormatArray)[ item >> 2 ]
		>> (( item % 4 ) << 2)) & 0x0f);
}
/*=================================================================================

=================================================================================*/
MODULE_PRIVATE XP_Bool uMapCode(uTable *uTable, uint16 in, uint16* out)
{
	XP_Bool done = FALSE;
	uint16 itemOfList = uTable->itemOfList;
	uint16 i;
	for(i=0;i<itemOfList;i++)
	{
		uMapCell* uCell;
		char format = uGetFormat(uTable,i);
		uCell = uGetMapCell(uTable,i);
		if(uHit(format, in, uCell))
		{
			*out = uMap(format, in, uTable,uCell);
			done = TRUE;
			break;
		}
	}
	return ( done && (*out != NOMAPPING));
}
/*=================================================================================

=================================================================================*/
MODULE_PRIVATE void		uMapIterate(uTable *uTable, uMapIterateFunc callback, uint16 context)
{
	uint16 itemOfList = uTable->itemOfList;
	uint16 i;
	for(i=0;i<itemOfList;i++)
	{
		uMapCell* uCell;
		char format = uGetFormat(uTable,i);
		uCell = uGetMapCell(uTable,i);
		uCellIterate(format, uTable ,uCell,callback, context);
	}
}

/*
	member function
*/
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uHitFormate0(uint16 in,uMapCell *cell)
{
	return ( (in >= cell->fmt.format0.srcBegin) &&
			     (in <= cell->fmt.format0.srcEnd) ) ;
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uHitFormate1(uint16 in,uMapCell *cell)
{
	return  uHitFormate0(in,cell);
}
/*=================================================================================

=================================================================================*/
PRIVATE XP_Bool uHitFormate2(uint16 in,uMapCell *cell)
{
	return (in == cell->fmt.format2.srcBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE uint16 uMapFormate0(uint16 in,uTable *uTable,uMapCell *cell)
{
	return ((in - cell->fmt.format0.srcBegin) + cell->fmt.format0.destBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE uint16 uMapFormate1(uint16 in,uTable *uTable,uMapCell *cell)
{
	return (*(((uint16 *)uTable) + uTable->offsetToMappingTable
		+ cell->fmt.format1.mappingOffset + in - cell->fmt.format1.srcBegin));
}
/*=================================================================================

=================================================================================*/
PRIVATE uint16 uMapFormate2(uint16 in,uTable *uTable,uMapCell *cell)
{
	return (cell->fmt.format2.destBegin);
}

/*=================================================================================

=================================================================================*/
PRIVATE void uIterateFormate0(uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context)
{
	uint16 ucs2;
	uint16 med;
	for(ucs2 = cell->fmt.format0.srcBegin, med = cell->fmt.format0.destBegin;
				ucs2 <= cell->fmt.format0.srcEnd ; ucs2++,med++)
		(*callback)(ucs2, med, context);
}
/*=================================================================================

=================================================================================*/
PRIVATE void uIterateFormate1(uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context)
{
	uint16 ucs2;
	uint16 *medpt;
	medpt = (((uint16 *)uTable) + uTable->offsetToMappingTable	+ cell->fmt.format1.mappingOffset);
	for(ucs2 = cell->fmt.format1.srcBegin;	ucs2 <= cell->fmt.format1.srcEnd ; ucs2++, medpt++)
		(*callback)(ucs2, *medpt, context);
}
/*=================================================================================

=================================================================================*/
PRIVATE void uIterateFormate2(uTable *uTable, uMapCell *cell,uMapIterateFunc callback, uint16 context)
{
	(*callback)(cell->fmt.format2.srcBegin, cell->fmt.format2.destBegin, context);
}

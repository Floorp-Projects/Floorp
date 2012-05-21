/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* #include "PRIntlpriv.h" */
#include "unicpriv.h" 


typedef PRUint16 (* MapFormatFunc)(PRUint16 in,const uTable *uT,const uMapCell *cell);
typedef PRBool (* HitFormateFunc)(PRUint16 in,const uMapCell *cell);
typedef void (* FillInfoFormateFunc)(const uTable *uT, const uMapCell *cell, PRUint32* info);


PRIVATE PRBool uHitFormate0(PRUint16 in,const uMapCell *cell);
PRIVATE PRBool uHitFormate2(PRUint16 in,const uMapCell *cell);
PRIVATE PRUint16 uMapFormate0(PRUint16 in,const uTable *uT,const uMapCell *cell);
PRIVATE PRUint16 uMapFormate1(PRUint16 in,const uTable *uT,const uMapCell *cell);
PRIVATE PRUint16 uMapFormate2(PRUint16 in,const uTable *uT,const uMapCell *cell);
PRIVATE void uFillInfoFormate0(const uTable *uT,const uMapCell *cell,PRUint32* aInfo);
PRIVATE void uFillInfoFormate1(const uTable *uT,const uMapCell *cell,PRUint32* aInfo);
PRIVATE void uFillInfoFormate2(const uTable *uT,const uMapCell *cell,PRUint32* aInfo);


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
PRIVATE const FillInfoFormateFunc m_fillinfo[uNumFormatTag] =
{
    uFillInfoFormate0,
    uFillInfoFormate1,
    uFillInfoFormate2,
};

/*=================================================================================

=================================================================================*/
PRIVATE const HitFormateFunc m_hit[uNumFormatTag] =
{
    uHitFormate0,
    uHitFormate0,
    uHitFormate2,
};

#define uHit(format,in,cell)   (* m_hit[(format)])((in),(cell))
#define uMap(format,in,uT,cell)  (* m_map[(format)])((in),(uT),(cell))
#define uGetMapCell(uT, item) ((uMapCell *)(((PRUint16 *)uT) + (uT)->offsetToMapCellArray + (item)*(UMAPCELL_SIZE/sizeof(PRUint16))))
#define uGetFormat(uT, item) (((((PRUint16 *)uT) + (uT)->offsetToFormatArray)[(item)>> 2 ] >> (((item)% 4 ) << 2)) & 0x0f)

/*=================================================================================

=================================================================================*/
MODULE_PRIVATE PRBool uMapCode(const uTable *uT, PRUint16 in, PRUint16* out)
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

#define SET_REPRESENTABLE(info, c)  (info)[(c) >> 5] |= (1L << ((c) & 0x1f))
/*=================================================================================

=================================================================================*/
PRIVATE void uFillInfoFormate0(const uTable *uT,const uMapCell *cell,PRUint32* info)
{
  PRUint16 begin, end, i;
  begin = cell->fmt.format0.srcBegin;
  end = cell->fmt.format0.srcEnd;
  if( (begin >> 5) == (end >> 5)) /* High 17 bits are the same */
  {
    for(i = begin; i <= end; i++)
      SET_REPRESENTABLE(info, i);
  } 
  else {
    PRUint32 b = begin >> 5;
    PRUint32 e = end >> 5;
    info[ b ] |= (0xFFFFFFFFL << ((begin) & 0x1f)); 
    info[ e ] |= (0xFFFFFFFFL >> (31 - ((end) & 0x1f)));
    for(b++ ; b < e ; b++)
      info[b] |= 0xFFFFFFFFL;
  }
}
/*=================================================================================

=================================================================================*/
PRIVATE void uFillInfoFormate1(const uTable *uT,const uMapCell *cell,PRUint32* info)
{
  PRUint16 begin, end, i;
  PRUint16 *base;
  begin = cell->fmt.format0.srcBegin;
  end = cell->fmt.format0.srcEnd;
  base = (((PRUint16 *)uT) + uT->offsetToMappingTable + cell->fmt.format1.mappingOffset);
  for(i = begin; i <= end; i++) 
  {
    if(0xFFFD != base[i - begin])  /* check every item */
      SET_REPRESENTABLE(info, i);
  }
}
/*=================================================================================

=================================================================================*/
PRIVATE void uFillInfoFormate2(const uTable *uT,const uMapCell *cell,PRUint32* info)
{
  SET_REPRESENTABLE(info, cell->fmt.format2.srcBegin);
}


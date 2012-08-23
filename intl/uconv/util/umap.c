/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* #include "PRIntlpriv.h" */
#include "unicpriv.h" 


typedef uint16_t (* MapFormatFunc)(uint16_t in,const uTable *uT,const uMapCell *cell);
typedef PRBool (* HitFormateFunc)(uint16_t in,const uMapCell *cell);
typedef void (* FillInfoFormateFunc)(const uTable *uT, const uMapCell *cell, uint32_t* info);


PRIVATE PRBool uHitFormate0(uint16_t in,const uMapCell *cell);
PRIVATE PRBool uHitFormate2(uint16_t in,const uMapCell *cell);
PRIVATE uint16_t uMapFormate0(uint16_t in,const uTable *uT,const uMapCell *cell);
PRIVATE uint16_t uMapFormate1(uint16_t in,const uTable *uT,const uMapCell *cell);
PRIVATE uint16_t uMapFormate2(uint16_t in,const uTable *uT,const uMapCell *cell);
PRIVATE void uFillInfoFormate0(const uTable *uT,const uMapCell *cell,uint32_t* aInfo);
PRIVATE void uFillInfoFormate1(const uTable *uT,const uMapCell *cell,uint32_t* aInfo);
PRIVATE void uFillInfoFormate2(const uTable *uT,const uMapCell *cell,uint32_t* aInfo);


PRIVATE const uMapCell *uGetMapCell(const uTable *uT, int16_t item);
PRIVATE char uGetFormat(const uTable *uT, int16_t item);


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
#define uGetMapCell(uT, item) ((uMapCell *)(((uint16_t *)uT) + (uT)->offsetToMapCellArray + (item)*(UMAPCELL_SIZE/sizeof(uint16_t))))
#define uGetFormat(uT, item) (((((uint16_t *)uT) + (uT)->offsetToFormatArray)[(item)>> 2 ] >> (((item)% 4 ) << 2)) & 0x0f)

/*=================================================================================

=================================================================================*/
MODULE_PRIVATE PRBool uMapCode(const uTable *uT, uint16_t in, uint16_t* out)
{
  PRBool done = PR_FALSE;
  uint16_t itemOfList = uT->itemOfList;
  uint16_t i;
  *out = NOMAPPING;
  for(i=0;i<itemOfList;i++)
  {
    const uMapCell* uCell;
    int8_t format = uGetFormat(uT,i);
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
PRIVATE PRBool uHitFormate0(uint16_t in,const uMapCell *cell)
{
  return ( (in >= cell->fmt.format0.srcBegin) &&
    (in <= cell->fmt.format0.srcEnd) ) ;
}
/*=================================================================================

=================================================================================*/
PRIVATE PRBool uHitFormate2(uint16_t in,const uMapCell *cell)
{
  return (in == cell->fmt.format2.srcBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE uint16_t uMapFormate0(uint16_t in,const uTable *uT,const uMapCell *cell)
{
  return ((in - cell->fmt.format0.srcBegin) + cell->fmt.format0.destBegin);
}
/*=================================================================================

=================================================================================*/
PRIVATE uint16_t uMapFormate1(uint16_t in,const uTable *uT,const uMapCell *cell)
{
  return (*(((uint16_t *)uT) + uT->offsetToMappingTable
    + cell->fmt.format1.mappingOffset + in - cell->fmt.format1.srcBegin));
}
/*=================================================================================

=================================================================================*/
PRIVATE uint16_t uMapFormate2(uint16_t in,const uTable *uT,const uMapCell *cell)
{
  return (cell->fmt.format2.destBegin);
}

#define SET_REPRESENTABLE(info, c)  (info)[(c) >> 5] |= (1L << ((c) & 0x1f))
/*=================================================================================

=================================================================================*/
PRIVATE void uFillInfoFormate0(const uTable *uT,const uMapCell *cell,uint32_t* info)
{
  uint16_t begin, end, i;
  begin = cell->fmt.format0.srcBegin;
  end = cell->fmt.format0.srcEnd;
  if( (begin >> 5) == (end >> 5)) /* High 17 bits are the same */
  {
    for(i = begin; i <= end; i++)
      SET_REPRESENTABLE(info, i);
  } 
  else {
    uint32_t b = begin >> 5;
    uint32_t e = end >> 5;
    info[ b ] |= (0xFFFFFFFFL << ((begin) & 0x1f)); 
    info[ e ] |= (0xFFFFFFFFL >> (31 - ((end) & 0x1f)));
    for(b++ ; b < e ; b++)
      info[b] |= 0xFFFFFFFFL;
  }
}
/*=================================================================================

=================================================================================*/
PRIVATE void uFillInfoFormate1(const uTable *uT,const uMapCell *cell,uint32_t* info)
{
  uint16_t begin, end, i;
  uint16_t *base;
  begin = cell->fmt.format0.srcBegin;
  end = cell->fmt.format0.srcEnd;
  base = (((uint16_t *)uT) + uT->offsetToMappingTable + cell->fmt.format1.mappingOffset);
  for(i = begin; i <= end; i++) 
  {
    if(0xFFFD != base[i - begin])  /* check every item */
      SET_REPRESENTABLE(info, i);
  }
}
/*=================================================================================

=================================================================================*/
PRIVATE void uFillInfoFormate2(const uTable *uT,const uMapCell *cell,uint32_t* info)
{
  SET_REPRESENTABLE(info, cell->fmt.format2.srcBegin);
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Stell <bstell@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include "prmem.h"
#include "nsCRT.h"
#include "nsICharRepresentable.h"
#include "nsCompressedCharMap.h"

void
FreeCCMap(PRUint16* &aMap)
{
  if (!aMap)
    return;
  PR_Free(aMap - CCMAP_EXTRA);
  aMap = nsnull;
}

PRUint16*
MapToCCMap(PRUint32* aMap)
{
  // put the data into a temp map
  nsCompressedCharMap ccmapObj;
  ccmapObj.SetChars(aMap);

  PRUint16 *ccmap = (PRUint16*)PR_Malloc((CCMAP_EXTRA + ccmapObj.GetSize()) * sizeof(PRUint16));
  NS_ASSERTION(ccmap, "failed to alloc new CCMap");

  if (!ccmap)
    return nsnull;

  ccmap += CCMAP_EXTRA;
  CCMAP_SIZE(ccmap) = ccmapObj.GetSize();
  CCMAP_FLAG(ccmap) = CCMAP_NONE_FLAG;

  ccmapObj.FillCCMap(ccmap);

#ifdef DEBUG
  for (int i=0; i<NUM_UNICODE_CHARS; i++) {
    PRBool oldb = IS_REPRESENTABLE(aMap, i);
    PRBool newb = CCMAP_HAS_CHAR(ccmap, i);
    if ((oldb) != (newb)) {
      NS_ASSERTION(oldb==newb,"failed to generate map correctly");
    }
  }
#endif
  return ccmap;
}

PRUint16* CreateEmptyCCMap()
{
  PRUint16 *ccmap = (PRUint16*)PR_Malloc((CCMAP_EXTRA + 16) * sizeof(PRUint16));
  NS_ASSERTION(ccmap, "failed to alloc new CCMap");

  if (!ccmap)
    return nsnull;

  memset(ccmap, '\0', CCMAP_EMPTY_SIZE_PER_INT16 * sizeof(PRUint16)+ CCMAP_EXTRA);
  ccmap += CCMAP_EXTRA;
  CCMAP_SIZE(ccmap) = CCMAP_EMPTY_SIZE_PER_INT16;
  CCMAP_FLAG(ccmap) = CCMAP_NONE_FLAG;
  return ccmap;
}

PRUint16*
MapperToCCMap(nsICharRepresentable *aMapper)
{
  PRUint32 map[UCS2_MAP_LEN];
  memset(map, 0, sizeof(map));
  nsresult res = aMapper->FillInfo(map);
  if (NS_FAILED(res))
    return nsnull;
  PRUint16* ccMap = MapToCCMap(map);

  return ccMap;
}

PRBool
NextNonEmptyCCMapPage(const PRUint16* aCCMap, PRUint32 *aPageStart)
{
  int i, j, l;
  int planeend = 0;
  int planestart = 0;
  unsigned int k;
  const PRUint16* ccmap;
  PRUint32 pagestart = *aPageStart;

  if(CCMAP_FLAG(aCCMap) & CCMAP_SURROGATE_FLAG) {
    // for SURROGATE
    planeend = EXTENDED_UNICODE_PLANES;
  }

  if(pagestart != CCMAP_BEGIN_AT_START_OF_MAP) {
    planestart = CCMAP_PLANE(pagestart);
  }

  // checking each plane
  for(l=planestart; l<=planeend; l++, pagestart = CCMAP_BEGIN_AT_START_OF_MAP) {

    if(l != 0 && CCMAP_FLAG(aCCMap) & CCMAP_SURROGATE_FLAG) {
      // for SURROGATE - get ccmap per plane
      ccmap = CCMAP_FOR_PLANE_EXT(aCCMap, l);
    } else {
      // only BMP
      ccmap = aCCMap;
    }
    //
    // Point to the next page
    //
    unsigned int upper_index;
    unsigned int mid_index;

    if (pagestart == CCMAP_BEGIN_AT_START_OF_MAP) {
      upper_index = 0;
      mid_index   = 0;
    } else {
      upper_index = CCMAP_UPPER_INDEX(pagestart & 0xffff);
      mid_index   = CCMAP_MID_INDEX(pagestart & 0xffff) + 1;
    }

    // walk thru the upper pointers
    const PRUint16 *upper = &ccmap[0];
    for (i=upper_index; i<CCMAP_NUM_UPPER_POINTERS; i++, mid_index=0) {
      if (upper[i] == CCMAP_EMPTY_MID) {
        continue;
      }

      // walk the mid array
      const PRUint16 *mid = &ccmap[upper[i]];
      for (j=mid_index; j<CCMAP_NUM_MID_POINTERS; j++) {
        if (mid[j] == CCMAP_EMPTY_PAGE)
          continue;
  
        // walk the page
        const ALU_TYPE *page = (ALU_TYPE*)&ccmap[mid[j]];
        for (k=0; k<CCMAP_NUM_ALUS_PER_PAGE; k++) {
          if (page[k] != 0) {
            PRUint32 base = (i*CCMAP_NUM_UCHARS_PER_MID) + (j*CCMAP_NUM_UCHARS_PER_PAGE);
            NS_ASSERTION(base<NUM_UNICODE_CHARS, "invalid page address");
            // return exact UCS4 code point, plane number + base
            *aPageStart = (((PRUint32)l)<<16)+base;
            return PR_TRUE;
          }
        }
      }
    }
  }
  return PR_FALSE;
}

#define CCMAP_MID_OFFSET(m, i) ((m)[i])
#define CCMAP_PAGE_OFFSET_FROM_MIDOFFSET(m, midoffset, i) ((m)[(i) + (midoffset)])

/***********************************************************************************
 *compare 2 ccmap and see if they are exactly the same
 * Here I assume both ccmap is generated by 
 * nsCompressedCharMap::SetChars(PRUint32* aMap)
 * This funtion rely on current implementation of above function. The that changed, 
 * we might need to revisit this implementation. 
 ***********************************************************************************/
PRBool IsSameCCMap(PRUint16* ccmap1, PRUint16* ccmap2)
{
  PRUint16 len1 = CCMAP_SIZE(ccmap1);
  PRUint16 len2 = CCMAP_SIZE(ccmap2);
  
  if (len1 != len2)
    return PR_FALSE;

  if (memcmp(ccmap1, ccmap2, sizeof(PRUint16)*len1))
    return PR_FALSE;
  return PR_TRUE;
}

PRUint16*
nsCompressedCharMap::NewCCMap()
{
  if (mExtended) {
    return MapToCCMapExt(mMap, mExtMap+1, EXTENDED_UNICODE_PLANES);
  } else {
    PRUint16 *newMap = (PRUint16*)PR_Malloc((CCMAP_EXTRA + mUsedLen) * sizeof(PRUint16));
    NS_ASSERTION(newMap, "failed to alloc new CCMap");
    if (!newMap)
      return nsnull;
  
    newMap += CCMAP_EXTRA;
    CCMAP_SIZE(newMap) = GetSize();
    CCMAP_FLAG(newMap) = CCMAP_NONE_FLAG;

    FillCCMap(newMap);
    return newMap;
  }
}

PRUint16*
nsCompressedCharMap::FillCCMap(PRUint16* aCCMap)
{
  // transfer the data
  for (int i=0; i<mUsedLen; i++)
    aCCMap[i] = u.mCCMap[i];

  return aCCMap;
}

nsCompressedCharMap::~nsCompressedCharMap()
{
  if(mExtended){
    int i;
    for (i = 1; i <= EXTENDED_UNICODE_PLANES; ++i) {
      if (mExtMap[i]) {
        PR_Free(mExtMap[i]);
      }
    }
  }
}

nsCompressedCharMap::nsCompressedCharMap()
{
  // initialize map to have:
  //    1 upper pointer array
  //    1 empty mid pointer array
  //    1 empty page

  int i;
  memset(u.mCCMap, 0, sizeof(u.mCCMap));
  mUsedLen = 0;
  mAllOnesPage = 0;

  // init the upper pointers
  PRUint16 *upper = &u.mCCMap[0];
  for (i=0; i<CCMAP_NUM_UPPER_POINTERS; i++) {
    upper[i] = CCMAP_EMPTY_MID;
  }
  mUsedLen += CCMAP_NUM_UPPER_POINTERS;

  // init the empty mid
  NS_ASSERTION(mUsedLen==CCMAP_EMPTY_MID, "empty mid offset misconfigured");
  PRUint16 *mid = &u.mCCMap[CCMAP_EMPTY_MID];
  for (i=0; i<CCMAP_NUM_MID_POINTERS; i++) {
    mid[i] = CCMAP_EMPTY_PAGE;
  }
  mUsedLen += CCMAP_NUM_MID_POINTERS;

  // init the empty page
  NS_ASSERTION(mUsedLen==CCMAP_EMPTY_PAGE, "empty page offset misconfigured");
  // the page was zero'd by the memset above
  mUsedLen += CCMAP_NUM_PRUINT16S_PER_PAGE;

  // init extended
  mExtended = PR_FALSE;
  memset(mExtMap+1, 0, sizeof(PRUint32*) * EXTENDED_UNICODE_PLANES);
  memset(mMap, 0, sizeof(mMap));
  mExtMap[0] = mMap;

}

void
nsCompressedCharMap::SetChar(PRUint32 aChar)
{
  if (mExtended) {
    PRUint32 plane_num = CCMAP_PLANE(aChar);
    NS_ASSERTION(plane_num <= EXTENDED_UNICODE_PLANES,"invalid plane");
    if (plane_num <= EXTENDED_UNICODE_PLANES) {
      if (mExtMap[plane_num] == 0) {
        mExtMap[plane_num] = (PRUint32*)PR_Malloc(sizeof(PRUint32)*UCS2_MAP_LEN);
        NS_ASSERTION(mExtMap[plane_num], "failed to alloc new mExtMap");
        if (!mExtMap[plane_num]) {
          return;
        }
        memset(mExtMap[plane_num], 0, sizeof(PRUint32)*UCS2_MAP_LEN);
      }
      SET_REPRESENTABLE(mExtMap[plane_num], aChar & 0xffff);
    }
  } else {
    NS_ASSERTION(aChar <= 0xffff, "extended char is passed");

    unsigned int i;
    unsigned int upper_index      = CCMAP_UPPER_INDEX(aChar);
    unsigned int mid_index        = CCMAP_MID_INDEX(aChar);

    PRUint16 mid_offset = u.mCCMap[upper_index];
    if (mid_offset == CCMAP_EMPTY_MID) {
      mid_offset = u.mCCMap[upper_index] = mUsedLen;
      mUsedLen += CCMAP_NUM_MID_POINTERS;
      NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
      // init the mid
      PRUint16 *mid = &u.mCCMap[mid_offset];
      for (i=0; i<CCMAP_NUM_MID_POINTERS; i++) {
        NS_ASSERTION(mid[i]==0, "this mid pointer should be unused");
        mid[i] = CCMAP_EMPTY_PAGE;
      }
    }

    PRUint16 page_offset = u.mCCMap[mid_offset+mid_index];
    if (page_offset == CCMAP_EMPTY_PAGE) {
      page_offset = u.mCCMap[mid_offset+mid_index] = mUsedLen;
      mUsedLen += CCMAP_NUM_PRUINT16S_PER_PAGE;
      NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
      // init the page
      PRUint16 *page = &u.mCCMap[page_offset];
      for (i=0; i<CCMAP_NUM_PRUINT16S_PER_PAGE; i++) {
        NS_ASSERTION(page[i]==0, "this page should be unused");
        page[i] = 0;
      }
    }
#undef CCMAP_SET_CHAR
#define CCMAP_SET_CHAR(m,c) (CCMAP_TO_ALU(m,c) |= (CCMAP_POW2(CCMAP_BIT_INDEX(c))))
    CCMAP_SET_CHAR(u.mCCMap,aChar);
#undef CCMAP_SET_CHAR
    NS_ASSERTION(CCMAP_HAS_CHAR(u.mCCMap,aChar), "failed to set bit");
  }
}

void
nsCompressedCharMap::SetChars(PRUint16 aBase, ALU_TYPE* aPage)
{
  unsigned int i;
  unsigned int upper_index = CCMAP_UPPER_INDEX(aBase);
  unsigned int mid_index   = CCMAP_MID_INDEX(aBase);
  NS_ASSERTION((aBase&CCMAP_PAGE_MASK)==0, "invalid page address");

  //
  // check of none/all bits set
  //
  PRUint16 num_none_set = 0;
  PRUint16 num_all_set = 0;
  for (i=0; i<CCMAP_NUM_ALUS_PER_PAGE; i++) {
    if (aPage[i] == 0)
      num_none_set++;
    else if (aPage[i] == CCMAP_ALU_MASK)
      num_all_set++;
  }
  if (num_none_set == CCMAP_NUM_ALUS_PER_PAGE) {
    return;
  }

  //
  // Alloc mid if necessary
  //
  PRUint16 mid_offset = u.mCCMap[upper_index];
  if (mid_offset == CCMAP_EMPTY_MID) {
    mid_offset = u.mCCMap[upper_index] = mUsedLen;
    mUsedLen += CCMAP_NUM_MID_POINTERS;
    NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
    // init the mid
    PRUint16 *mid = &u.mCCMap[mid_offset];
    for (i=0; i<CCMAP_NUM_MID_POINTERS; i++) {
      NS_ASSERTION(mid[i]==0, "this mid pointer should be unused");
      mid[i] = CCMAP_EMPTY_PAGE;
    }
  }

  //
  // if all bits set share an "all bits set" page
  //
  if (num_all_set == CCMAP_NUM_ALUS_PER_PAGE) {
    if (mAllOnesPage == 0) {
      mAllOnesPage = mUsedLen;
      mUsedLen += CCMAP_NUM_PRUINT16S_PER_PAGE;
      NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
      ALU_TYPE *all_ones_page = (ALU_TYPE*)&u.mCCMap[mAllOnesPage];
      for (i=0; i<CCMAP_NUM_ALUS_PER_PAGE; i++) {
        NS_ASSERTION(all_ones_page[i]==0, "this page should be unused");
        all_ones_page[i] = CCMAP_ALU_MASK;
      }
    }
    u.mCCMap[mid_offset+mid_index] = mAllOnesPage;
    return;
  }

  //
  // Alloc page if necessary
  //
  PRUint16 page_offset = u.mCCMap[mid_offset+mid_index];
  if (page_offset == CCMAP_EMPTY_PAGE) {
    page_offset = u.mCCMap[mid_offset+mid_index] = mUsedLen;
    mUsedLen += CCMAP_NUM_PRUINT16S_PER_PAGE;
    NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
  }

  // copy the page data
  ALU_TYPE *page = (ALU_TYPE*)&u.mCCMap[page_offset];
  for (i=0; i<CCMAP_NUM_ALUS_PER_PAGE; i++) {
    NS_ASSERTION(page[i]==0, "this page should be unused");
    page[i] = aPage[i];
  }
}

void
nsCompressedCharMap::SetChars(PRUint16* aCCMap)
{
  int i, j;
  if(mExtended){
    PRUint32 page = CCMAP_BEGIN_AT_START_OF_MAP;
    while (NextNonEmptyCCMapPage(aCCMap, &page)) {
      PRUint32 pagechar = page;
      for (i=0; i<(CCMAP_BITS_PER_PAGE/8); i++) {
        for (j=0; j<8; j++) {
          if (CCMAP_HAS_CHAR_EXT(aCCMap, pagechar)) {
            SetChar(pagechar);
          }
          pagechar++;
        }
      }
    }
  } else {
    //
    // Copy the input CCMap
    //
    // walk thru the upper pointers
    PRUint16 *upper = &aCCMap[0];
    for (i=0; i<CCMAP_NUM_UPPER_POINTERS; i++) {
      if (upper[i] == CCMAP_EMPTY_MID)
        continue;

      // walk the mid array
      PRUint16 *mid = &aCCMap[upper[i]];
      for (j=0; j<CCMAP_NUM_MID_POINTERS; j++) {
        if (mid[j] == CCMAP_EMPTY_PAGE)
          continue;

        PRUint32 base = (i*CCMAP_NUM_UCHARS_PER_MID) + (j*CCMAP_NUM_UCHARS_PER_PAGE);
        NS_ASSERTION(base<NUM_UNICODE_CHARS, "invalid page address");
        ALU_TYPE *page = (ALU_TYPE*)&aCCMap[mid[j]];
        SetChars((PRUint16)base, page);
      }
    }
  }
}

void
nsCompressedCharMap::SetChars(PRUint32* aMap)
{
  PRUint32* frommap_page;
  frommap_page = aMap;
  PRUint16 base = 0;

  for (int i=0; i<CCMAP_TOTAL_PAGES; i++) {

#if (CCMAP_BITS_PER_ALU == CCMAP_BITS_PER_PRUINT32)
    SetChars(base, (ALU_TYPE*)frommap_page);
    frommap_page += CCMAP_PRUINT32S_PER_PAGE;

#elif (CCMAP_BITS_PER_ALU > CCMAP_BITS_PER_PRUINT32)
    int j, k = CCMAP_BITS_PER_PRUINT32;
    ALU_TYPE page[CCMAP_NUM_ALUS_PER_PAGE];
    ALU_TYPE *p = page;
    for (j=0; j<CCMAP_ALUS_PER_PAGE; j++) {
      ALU_TYPE alu_val = 0;
      ALU_TYPE tmp;
      for (k=0; k<CCMAP_PRUINT32S_PER_ALU; k++) {
        tmp = *frommap_page;
        tmp <<= (k*CCMAP_BITS_PER_PRUINT32);
        //alu_val |= (*frommap_page)<<(k*CCMAP_BITS_PER_PRUINT32);
        alu_val |= tmp;
        frommap_page++;
      }
      *p++ = alu_val;
    }
    SetChars(base, page);
#elif (CCMAP_BITS_PER_ALU < CCMAP_BITS_PER_PRUINT32)
    int j, k;
    ALU_TYPE page[CCMAP_NUM_ALUS_PER_PAGE];
    int v = CCMAP_PRUINT32S_PER_PAGE;
    ALU_TYPE *p = page;
    for (j=0; j<CCMAP_PRUINT32S_PER_PAGE; j++) {
      PRUint32 pruint32_val = *frommap_page++;
      for (k=0; k<CCMAP_ALUS_PER_PRUINT32; k++) {
        *p++ = pruint32_val & CCMAP_ALU_MASK;
        pruint32_val >>= CCMAP_BITS_PER_ALU; 
      }
    }
    SetChars(base, page);
#endif

    base += CCMAP_NUM_UCHARS_PER_PAGE;
  }
}

#ifdef DEBUG
void
printCCMap(PRUint16* aCCMap)
{
  PRUint32 page = CCMAP_BEGIN_AT_START_OF_MAP;
  while (NextNonEmptyCCMapPage(aCCMap, &page)) {
    //FONT_SCAN_PRINTF(("page starting at 0x%04x has chars", page));
    int i;
    PRUint32 pagechar = page;
  
    printf("CCMap:0x%04lx=", (long)page);
    for (i=0; i<(CCMAP_BITS_PER_PAGE/8); i++) {
      unsigned char val = 0;
      for (int j=0; j<8; j++) {
        if (CCMAP_HAS_CHAR(aCCMap, pagechar)) {
          val |= 1 << j;
        }
        pagechar++;
      }
      printf("%02x", val);
    }
    printf("\n");
  }
}
#endif

// Non-BMP unicode support extension, create ccmap for both BMP and extended planes 
PRUint16*
MapToCCMapExt(PRUint32* aBmpPlaneMap, PRUint32** aOtherPlaneMaps, PRUint32 aOtherPlaneNum)
{
  nsCompressedCharMap* otherPlaneObj[EXTENDED_UNICODE_PLANES];
  PRUint32 totalSize;
  PRUint16 i;
  PRUint32 *planeCCMapOffsets;
  PRUint32 currOffset;

  NS_ASSERTION(aOtherPlaneNum <= EXTENDED_UNICODE_PLANES, "illegal argument value");
  if (aOtherPlaneNum > EXTENDED_UNICODE_PLANES)
    return nsnull;

  // Put the data into a temp map
  nsCompressedCharMap bmpCcmapObj;
  bmpCcmapObj.SetChars(aBmpPlaneMap);

  // Add bmp size
  totalSize = bmpCcmapObj.GetSize();

  // Add bmp length field
  totalSize += CCMAP_EXTRA;
  
  // Add Plane array 
  totalSize += EXTENDED_UNICODE_PLANES * sizeof(PRUint32)/sizeof(PRUint16);

  // Add an empty plane ccmap
  // A totally empty plane ccmap can be represented by 16 *(PRUint16)0. 
  totalSize += CCMAP_EMPTY_SIZE_PER_INT16;

  // Create ccmap for other planes
  for (i = 0; i < aOtherPlaneNum; i++) {
    if (aOtherPlaneMaps[i]) {
      otherPlaneObj[i] = new nsCompressedCharMap();
      NS_ASSERTION(otherPlaneObj, "unable to create new nsCompressedCharMap");
      if(otherPlaneObj) {
        otherPlaneObj[i]->SetChars(aOtherPlaneMaps[i]);
        totalSize += otherPlaneObj[i]->GetSize();
      }
    } else {
      otherPlaneObj[i] = nsnull;
    }
  }

  PRUint16 *ccmap = (PRUint16*)PR_Malloc(totalSize * sizeof(PRUint16));
  NS_ASSERTION(ccmap, "failed to alloc new CCMap");

  if (!ccmap)
    return nsnull;

  // Assign BMP ccmap size
  ccmap += CCMAP_EXTRA;
  CCMAP_SIZE(ccmap) = bmpCcmapObj.GetSize();
  CCMAP_FLAG(ccmap) = CCMAP_SURROGATE_FLAG;

  // Fill bmp plane ccmap 
  bmpCcmapObj.FillCCMap(ccmap);

  // Get pointer for plane ccmap offset array
  currOffset = bmpCcmapObj.GetSize();
  planeCCMapOffsets = (PRUint32*)(ccmap+currOffset);
  currOffset += sizeof(PRUint32)/sizeof(PRUint16)*EXTENDED_UNICODE_PLANES;

  // Put a empty ccmap there 
  memset(ccmap+currOffset, '\0', sizeof(PRUint16)*16);
  PRUint32 emptyCCMapOffset = currOffset;
  currOffset += CCMAP_EMPTY_SIZE_PER_INT16;

  // Now fill all rest of the planes' ccmap and put off in array
  for (i = 0; i <aOtherPlaneNum; i++) {
    if (aOtherPlaneMaps[i] && otherPlaneObj[i]) {
      *(planeCCMapOffsets+i) = currOffset;
      otherPlaneObj[i]->FillCCMap(ccmap+currOffset);
      currOffset += otherPlaneObj[i]->GetSize();
    }
    else 
      *(planeCCMapOffsets+i) = emptyCCMapOffset;
  }
  for (; i < EXTENDED_UNICODE_PLANES; i++) {
    *(planeCCMapOffsets+i) = emptyCCMapOffset;
  }

  // remove all nsCompressedCharMap objects allocated 
  for (i = 0; i < aOtherPlaneNum; i++) {
    if (otherPlaneObj[i]) 
      delete otherPlaneObj[i];
  }

#ifdef DEBUG
  PRUint32 k, h, l, plane, offset;
  PRBool oldb;
  PRBool newb;

  // testing for BMP plane
  for (k=0; k<NUM_UNICODE_CHARS; k++) {
    oldb = IS_REPRESENTABLE(aBmpPlaneMap, k);
    newb = CCMAP_HAS_CHAR_EXT(ccmap, k);
    NS_ASSERTION(oldb==newb,"failed to generate map correctly");
  }

  //testing for extension plane 
  for (k = 0x10000; k < 0x100000; k++) {
    plane = k/0x10000;
    if (plane > aOtherPlaneNum)
      break;
    if (aOtherPlaneMaps[plane-1])
      oldb = IS_REPRESENTABLE(aOtherPlaneMaps[plane-1], k&0xffff);
    else
      oldb = 0;
    newb = CCMAP_HAS_CHAR_EXT(ccmap, k);
    NS_ASSERTION(oldb==newb, "failed to generate extension map correctly");
  }

  
  // testing for non-BMP plane
    for (h = 0; h < 0x400; h++) {
      for (l = 0; l < 0x400; l++) {
        plane = h >> 6;
        offset = (h*0x400 + l) & 0xffff;
        if (aOtherPlaneMaps[plane])
          oldb = IS_REPRESENTABLE(aOtherPlaneMaps[plane], offset);
        else
          oldb = 0;
        newb = CCMAP_HAS_CHAR_EXT2(ccmap, h+0xd800, l+0xdc00);
        NS_ASSERTION(oldb==newb, "failed to generate extension map correctly");
      }
    }
#endif

  return ccmap;
}

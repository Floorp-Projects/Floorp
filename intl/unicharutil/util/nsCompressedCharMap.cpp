/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab:
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Brian Stell <bstell@netscape.com>
 */

#include "prmem.h"
#include "nsCompressedCharMap.h"
#include "nsCRT.h"
#include "nsICharRepresentable.h"

void
FreeCCMap(PRUint16* &aMap)
{
  if (!aMap)
    return;
  PR_Free(aMap);
  aMap = nsnull;
}

PRUint16*
MapToCCMap(PRUint32* aMap)
{
  // put the data into a temp map
  nsCompressedCharMap ccmapObj;
  ccmapObj.SetChars(aMap);

  // make a copy of the map
  PRUint16* ccmap = ccmapObj.NewCCMap();

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

PRUint16*
nsCompressedCharMap::NewCCMap()
{
  PRUint16 *newMap = (PRUint16*)PR_Malloc(mUsedLen * sizeof(PRUint16));
  NS_ASSERTION(newMap, "failed to alloc new CCMap");
  if (!newMap)
    return nsnull;

  // transfer the data
  for (int i=0; i<mUsedLen; i++)
    newMap[i] = mCCMap[i];

  return newMap;
}

nsCompressedCharMap::nsCompressedCharMap()
{
  // initialize map to have:
  //    1 upper pointer array
  //    1 empty mid pointer array
  //    1 empty page

  int i;
  memset(mCCMap, 0, sizeof(mCCMap));
  mUsedLen = 0;
  mAllOnesPage = 0;

  // init the upper pointers
  PRUint16 *upper = &mCCMap[0];
  for (i=0; i<CCMAP_NUM_UPPER_POINTERS; i++) {
    upper[i] = CCMAP_EMPTY_MID;
  }
  mUsedLen += CCMAP_NUM_UPPER_POINTERS;

  // init the empty mid
  NS_ASSERTION(mUsedLen==CCMAP_EMPTY_MID, "empty mid offset misconfigured");
  PRUint16 *mid = &mCCMap[CCMAP_EMPTY_MID];
  for (i=0; i<CCMAP_NUM_MID_POINTERS; i++) {
    mid[i] = CCMAP_EMPTY_PAGE;
  }
  mUsedLen += CCMAP_NUM_MID_POINTERS;

  // init the empty page
  NS_ASSERTION(mUsedLen==CCMAP_EMPTY_PAGE, "empty page offset misconfigured");
  // the page was zero'd by the memset above
  mUsedLen += CCMAP_NUM_PRUINT16S_PER_PAGE;
}

void
nsCompressedCharMap::SetChar(PRUint16 aChar)
{
  unsigned int i;
  unsigned int upper_index      = CCMAP_UPPER_INDEX(aChar);
  unsigned int mid_index        = CCMAP_MID_INDEX(aChar);

  PRUint16 mid_offset = mCCMap[upper_index];
  if (mid_offset == CCMAP_EMPTY_MID) {
    mid_offset = mCCMap[upper_index] = mUsedLen;
    mUsedLen += CCMAP_NUM_MID_POINTERS;
    NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
    // init the mid
    PRUint16 *mid = &mCCMap[mid_offset];
    for (i=0; i<CCMAP_NUM_MID_POINTERS; i++) {
      NS_ASSERTION(mid[i]==0, "this mid pointer should be unused");
      mid[i] = CCMAP_EMPTY_PAGE;
    }
  }

  PRUint16 page_offset = mCCMap[mid_offset+mid_index];
  if (page_offset == CCMAP_EMPTY_PAGE) {
    page_offset = mCCMap[mid_offset+mid_index] = mUsedLen;
    mUsedLen += CCMAP_NUM_PRUINT16S_PER_PAGE;
    NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
    // init the page
    PRUint16 *page = &mCCMap[page_offset];
    for (i=0; i<CCMAP_NUM_PRUINT16S_PER_PAGE; i++) {
      NS_ASSERTION(page[i]==0, "this page should be unused");
      page[i] = 0;
    }
  }
#undef CCMAP_SET_CHAR
#define CCMAP_SET_CHAR(m,c) (CCMAP_TO_ALU(m,c) |= (CCMAP_POW2(CCMAP_BIT_INDEX(c))))
  CCMAP_SET_CHAR(mCCMap,aChar);
#undef CCMAP_SET_CHAR
  NS_ASSERTION(CCMAP_HAS_CHAR(mCCMap,aChar), "failed to set bit");
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
  PRUint16 mid_offset = mCCMap[upper_index];
  if (mid_offset == CCMAP_EMPTY_MID) {
    mid_offset = mCCMap[upper_index] = mUsedLen;
    mUsedLen += CCMAP_NUM_MID_POINTERS;
    NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
    // init the mid
    PRUint16 *mid = &mCCMap[mid_offset];
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
      ALU_TYPE *all_ones_page = (ALU_TYPE*)&mCCMap[mAllOnesPage];
      for (i=0; i<CCMAP_NUM_ALUS_PER_PAGE; i++) {
        NS_ASSERTION(all_ones_page[i]==0, "this page should be unused");
        all_ones_page[i] = CCMAP_ALU_MASK;
      }
    }
    mCCMap[mid_offset+mid_index] = mAllOnesPage;
    return;
  }

  //
  // Alloc page if necessary
  //
  PRUint16 page_offset = mCCMap[mid_offset+mid_index];
  if (page_offset == CCMAP_EMPTY_PAGE) {
    page_offset = mCCMap[mid_offset+mid_index] = mUsedLen;
    mUsedLen += CCMAP_NUM_PRUINT16S_PER_PAGE;
    NS_ASSERTION(mUsedLen<=CCMAP_MAX_LEN,"length too long");
  }

  // copy the page data
  ALU_TYPE *page = (ALU_TYPE*)&mCCMap[page_offset];
  for (i=0; i<CCMAP_NUM_ALUS_PER_PAGE; i++) {
    NS_ASSERTION(page[i]==0, "this page should be unused");
    page[i] = aPage[i];
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


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSCOMPRESSEDCHARMAP_H
#define NSCOMPRESSEDCHARMAP_H
#include "prtypes.h"


#define ALU_SIZE PR_BITS_PER_LONG
//#define ALU_SIZE 16
//#define ALU_SIZE 32
//#define ALU_SIZE 64
#if (ALU_SIZE==32)
#   define ALU_TYPE                PRUint32
#   define CCMAP_POW2(n)           (1L<<(n))
#   define CCMAP_BITS_PER_ALU_LOG2 5
#elif (ALU_SIZE==64)
#   define ALU_TYPE                PRUint64
#   define CCMAP_POW2(n)           (1LL<<(n))
#   define CCMAP_BITS_PER_ALU_LOG2 6
#else
#   define ALU_TYPE                PRUint16
#   define CCMAP_POW2(n)           (1<<(n))
#   define CCMAP_BITS_PER_ALU_LOG2 4
#endif


class nsICharRepresentable;

extern PRUint16* CreateEmptyCCMap();
extern PRUint16* MapToCCMap(PRUint32* aMap);
extern PRUint16* MapperToCCMap(nsICharRepresentable *aMapper);
extern void FreeCCMap(PRUint16* &aMap);
extern PRBool IsSameCCMap(PRUint16* ccmap1, PRUint16* ccmap2);

// 
// nsCompressedCharMap
// 
// A Compressed Char Map (CCMap) saves memory by folding all
// the empty portions of the map on top of each other.
//
// Building a Compressed Char Map (CCMap) is more complex than
// accessing it.  We use the nsCompressedCharMap object to 
// build the CCMap. Once nsCompressedCharMap has built the CCMap
// we get a copy of the CCMap and discard the nsCompressedCharMap 
// object.  The CCMap is an array of PRUint16 and is accessed by 
// a macro.
//
// See "Character Map Compression" below for a discussion of
// what the array looks like.

//
// The maximum size a CCMap:
//  (16 upper pointers) + (16 empty mid pointers) +
//  (16 empty page)     + (16*16 max mid pointers) +
//  (256*16 max pages)  =  4400 PRUint16
#define CCMAP_MAX_LEN (16+16+16+256+4096)

class nsCompressedCharMap {
public:
  nsCompressedCharMap();

  PRUint16* NewCCMap();
  void      FreeCCMap(PRUint16*);
  void      SetChar(PRUint16);
  void      SetChars(PRUint16*);
  void      SetChars(PRUint16, ALU_TYPE*);
  void      SetChars(PRUint32*);

protected:
  PRUint16 mUsedLen;   // in PRUint16
  PRUint16 mAllOnesPage;
  PRUint16 mCCMap[CCMAP_MAX_LEN];

};

//
// Character Map Compression
//
// Each font requires its own 8k charmap.  On a system with 200 
// fonts this would take: 200 * 8K = 1600K memory.
//
// Since most char maps are mostly empty a significant amount
// of memory can be saved by not allocating the unused sections.
//
// If the map has one or more levels of indirection then the
// the empty sections of the map can all be folded to a single
// common empty element. In this way only the non-empty sections 
// need space. Because the empty sections actually point to a
// common empty section every entry in the map can be valid
// without requiring actually allocating space. 
// Some larger CJK fonts have large sections where every bit
// is set.  In the same way that the empty sections are folded
// onto one "empty page", the sections where all bits are set are 
// folded on to one "all bits set page" .
//
// Break up the Unicode range bits 0x0000 - 0xFFFF
// into 3 bit ranges:
//
// upper bits: bit15 - bit12
//   mid bits: bit11 -  bit8
//  page bits:  bit7 -  bit0
//
// within a page, (assumming a 4 byte ALU)
//    bits 7-5 select one of the 8 longs
//    bits 4-0 select one of the 32 bits within the long
//
// There is exactly one upper "pointers" array.
//
// The upper pointers each point to a mid array.  If there are no chars 
// in an upper pointer's block that pointer points to the empty mid.
// Thus all upper pointers are "valid" even if they do not have space
// allocated; eg: the accessor macro does not need to test if the 
// pointer is zero.
//
// Each mid pointer in the mid array points to a page. If there are no
// chars in a mid pointer's page that pointer points to the empty page.
// Thus all mid pointers are "valid" even if they do not have space
// allocated; eg: the accessor macro does not need to test if the 
// pointer is zero.
//
// Since the array will be less than 5K PRUint16 the "pointers" can
// be implemented as 2 byte offsets from the base instead of
// real pointers.
//
// the format of the CCMap is
//     the upper pointers     (16 PRUint16)
//     the empty mid pointers (16 PRUint16)
//     the empty page         (16 PRUint16)
//     non-empty mid pointers and pages as needed

// One minor note: for a completely empty map it is actually 
// possible to fold the upper, empty mid, and empty page
// on top of each other and make a map of only 32 bytes.

// offsets to the empty mid and empty page
#define CCMAP_EMPTY_MID  CCMAP_NUM_UPPER_POINTERS
#define CCMAP_EMPTY_PAGE CCMAP_EMPTY_MID+CCMAP_NUM_MID_POINTERS

// 
// Because the table is offset based the code can build the table in a
// temp space (max table size on the stack) and then do one alloc of 
// the actual needed size and simply copy over the data.
//



//
// Page bits
//
#define CCMAP_BITS_PER_PAGE_LOG2    8
#define CCMAP_BITS_PER_PAGE         CCMAP_POW2(CCMAP_BITS_PER_PAGE_LOG2)
#define CCMAP_BIT_INDEX(c)          ((c) & PR_BITMASK(CCMAP_BITS_PER_ALU_LOG2))
#define CCMAP_ALU_INDEX(c)          (((c)>>CCMAP_BITS_PER_ALU_LOG2) \
               & PR_BITMASK(CCMAP_BITS_PER_PAGE_LOG2 - CCMAP_BITS_PER_ALU_LOG2))

#define CCMAP_PAGE_MASK             PR_BITMASK(CCMAP_BITS_PER_PAGE_LOG2)
#define CCMAP_NUM_PRUINT16S_PER_PAGE \
                         (CCMAP_BITS_PER_PAGE/CCMAP_BITS_PER_PRUINT16)
// one bit per char
#define CCMAP_NUM_ALUS_PER_PAGE     (CCMAP_BITS_PER_PAGE/CCMAP_BITS_PER_ALU)
#define CCMAP_NUM_UCHARS_PER_PAGE   CCMAP_BITS_PER_PAGE

//
// Mid bits
//
#define CCMAP_BITS_PER_MID_LOG2     4
#define CCMAP_MID_INDEX(c)          \
      (((c)>>CCMAP_BITS_PER_PAGE_LOG2) & PR_BITMASK(CCMAP_BITS_PER_MID_LOG2))
#define CCMAP_NUM_MID_POINTERS    CCMAP_POW2(CCMAP_BITS_PER_MID_LOG2)
#define CCMAP_NUM_UCHARS_PER_MID    \
               CCMAP_POW2(CCMAP_BITS_PER_MID_LOG2+CCMAP_BITS_PER_PAGE_LOG2)

//
// Upper bits
//
#define CCMAP_BITS_PER_UPPER_LOG2   4
#define CCMAP_UPPER_INDEX(c)        \
      (((c)>>(CCMAP_BITS_PER_MID_LOG2+CCMAP_BITS_PER_PAGE_LOG2)) \
         & PR_BITMASK(CCMAP_BITS_PER_UPPER_LOG2))
#define CCMAP_NUM_UPPER_POINTERS    CCMAP_POW2(CCMAP_BITS_PER_UPPER_LOG2)

//
// Misc
//
#define CCMAP_BITS_PER_PRUINT16_LOG2 4
#define CCMAP_BITS_PER_PRUINT32_LOG2 5

#define CCMAP_BITS_PER_PRUINT16 CCMAP_POW2(CCMAP_BITS_PER_PRUINT16_LOG2)
#define CCMAP_BITS_PER_PRUINT32 CCMAP_POW2(CCMAP_BITS_PER_PRUINT32_LOG2)
#define CCMAP_BITS_PER_ALU      CCMAP_POW2(CCMAP_BITS_PER_ALU_LOG2)

#define CCMAP_ALUS_PER_PRUINT32  (CCMAP_BITS_PER_PRUINT32/CCMAP_BITS_PER_ALU)
#define CCMAP_PRUINT32S_PER_ALU  (CCMAP_BITS_PER_ALU/CCMAP_BITS_PER_PRUINT32)
#define CCMAP_PRUINT32S_PER_PAGE (CCMAP_BITS_PER_PAGE/CCMAP_BITS_PER_PRUINT32)

#define CCMAP_ALU_MASK       PR_BITMASK(CCMAP_BITS_PER_ALU)
#define CCMAP_ALUS_PER_PAGE  CCMAP_POW2(CCMAP_BITS_PER_PAGE_LOG2 \
                                       - CCMAP_BITS_PER_ALU_LOG2)
#define NUM_UNICODE_CHARS    CCMAP_POW2(CCMAP_BITS_PER_UPPER_LOG2 \
                                       +CCMAP_BITS_PER_MID_LOG2 \
                                       +CCMAP_BITS_PER_PAGE_LOG2)
#define CCMAP_TOTAL_PAGES    CCMAP_POW2(CCMAP_BITS_PER_UPPER_LOG2 \
                                       +CCMAP_BITS_PER_MID_LOG2)


//
// Finally, build up the macro to test the bit for a given char
//

// offset from base to mid array
#define CCMAP_TO_MID(m,c) ((m)[CCMAP_UPPER_INDEX(c)])

// offset from base to page
#define CCMAP_TO_PAGE(m,c) ((m)[CCMAP_TO_MID((m),(c)) + CCMAP_MID_INDEX(c)])

// offset from base to alu
#define CCMAP_TO_ALU(m,c) \
          (*((ALU_TYPE*)(&((m)[CCMAP_TO_PAGE((m),(c))])) + CCMAP_ALU_INDEX(c)))

// test the bit
#define CCMAP_HAS_CHAR(m,c) (((CCMAP_TO_ALU(m,c))>>CCMAP_BIT_INDEX(c)) & 1)

// unset the bit
#define CCMAP_UNSET_CHAR(m,c) (CCMAP_TO_ALU(m,c) &= ~(CCMAP_POW2(CCMAP_BIT_INDEX(c))))

#endif // NSCOMPRESSEDCHARMAP_H 

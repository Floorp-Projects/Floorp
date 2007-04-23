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

#ifndef NSCOMPRESSEDCHARMAP_H
#define NSCOMPRESSEDCHARMAP_H
#include "prtypes.h"
#include "nsICharRepresentable.h"

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
#   define CCMAP_POW2(n)           (1L<<(n))
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
extern PRBool NextNonEmptyCCMapPage(const PRUint16 *, PRUint32 *);
extern PRBool IsSameCCMap(PRUint16* ccmap1, PRUint16* ccmap2);
#ifdef DEBUG
void printCCMap(PRUint16* aCCMap);
#endif


// surrogate support extension
extern PRUint16*
MapToCCMapExt(PRUint32* aBmpPlaneMap, PRUint32** aOtherPlaneMaps, PRUint32 aOtherPlaneNum);

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

// non-bmp unicode support extension
#define EXTENDED_UNICODE_PLANES    16

class nsCompressedCharMap {
public:
  nsCompressedCharMap();
  ~nsCompressedCharMap();

  PRUint16* NewCCMap();
  PRUint16* FillCCMap(PRUint16* aCCMap);
  PRUint16  GetSize() {return mUsedLen;}
  void      SetChar(PRUint32);
  void      SetChars(PRUint16*);
  void      SetChars(PRUint16, ALU_TYPE*);
  void      SetChars(PRUint32*);
  void      Extend() {mExtended = PR_TRUE;} // enable surrogate area

protected:
  union {
    PRUint16 mCCMap[CCMAP_MAX_LEN];
    ALU_TYPE used_for_align; // do not use; only here to cause
                             // alignment
  } u;
  PRUint16 mUsedLen;   // in PRUint16
  PRUint16 mAllOnesPage;

  PRBool mExtended;

  // for surrogate support
  PRUint32* mExtMap[EXTENDED_UNICODE_PLANES+1];
  PRUint32  mMap[UCS2_MAP_LEN];
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
#define CCMAP_EMPTY_SIZE_PER_INT16    16

// offsets to the empty mid and empty page
#define CCMAP_EMPTY_MID  CCMAP_NUM_UPPER_POINTERS
#define CCMAP_EMPTY_PAGE CCMAP_EMPTY_MID+CCMAP_NUM_MID_POINTERS

// 
// Because the table is offset based the code can build the table in a
// temp space (max table size on the stack) and then do one alloc of 
// the actual needed size and simply copy over the data.
//

//
// Compressed Char map surrogate extension 
//
// The design goal of surrogate support extension is to keep efficiency 
// and compatibility of existing compressed charmap operations. Most of 
// existing operation are untouched. For BMP support, very little memory 
// overhead (4 bytes) is added. Runtime efficiency of BMP support is 
// unchanged. 
//
// structure of extended charmap:
//    ccmap flag      1 PRUint16 (PRUint32) , indicates if this is extended or not
//    bmp ccmap size  1 PRUint16 (PRUint32) , the size of BMP ccmap, 
//    BMP ccmap       size varies, 
//    plane index     16 PRUint32, use to index ccmap for non-BMP plane
//    empty ccmap     16 PRUint16, a total empty ccmap
//    non-BMP ccmaps  size varies, other non-empty, non-BMP ccmap
//   
// Changes to basic ccmap 
//  2 PRUint16 (PRUint32 on 64bit machines) are added in the very beginning. 
// One is used to specify the size 
// of the ccmap, the other is used as a flag. But these 2 fields are indexed 
// negatively so that all other operation remain unchanged to keep efficiency. 
// ccmap memory allocation is moved from nsCompressedCharMap::NewCCMap to 
// MapToCCMap.
//
// Extended ccmap 
//  A 16*PRUint32 array was put at the end of basic ccmap, each PRUint32 either 
// pointed to the empty ccmap or a independent plane ccmap. Directly after this 
// array is a empty ccmap. All planes that has no character will share this ccmap. 
// All non-empty plane will have a ccmap. 
//  "MapToCCMapExt" is added to created an extended ccmap, each plane ccmap is 
// created the same as basic one, but without 2 additional fields.
//  "HasGlyphExt" is used to access extended ccmap, it first need to locate the 
// plane ccmap, and then operated the same way as "HasGlyph". 
//
// Compatibility between old and new one
// Because extended ccmap include an exactly identical basic ccmap in its head, 
// basic ccmap operation (HasGlyph) can be applied on extended ccmap without 
// problem. 
// Because basic ccmap is now have a flag to indicate if it is a extended one, 
// Extended ccmap operation (HasGlyphExt) can check the flag before it does 
// extended ccmap specific operation. So HasGlyphExt can be applied to basic ccmap 
// too.  
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

#define CCMAP_ALU_MASK       PR_BITMASK(CCMAP_BITS_PER_ALU_LOG2)
#define CCMAP_ALUS_PER_PAGE  CCMAP_POW2(CCMAP_BITS_PER_PAGE_LOG2 \
                                       - CCMAP_BITS_PER_ALU_LOG2)
#define NUM_UNICODE_CHARS    CCMAP_POW2(CCMAP_BITS_PER_UPPER_LOG2 \
                                       +CCMAP_BITS_PER_MID_LOG2 \
                                       +CCMAP_BITS_PER_PAGE_LOG2)
#define CCMAP_TOTAL_PAGES    CCMAP_POW2(CCMAP_BITS_PER_UPPER_LOG2 \
                                       +CCMAP_BITS_PER_MID_LOG2)

#define CCMAP_BEGIN_AT_START_OF_MAP 0xFFFFFFFF

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

#define CCMAP_SIZE(m) (*((m)-1))
#define CCMAP_FLAG(m) (*((m)-2))
#define CCMAP_EXTRA    (sizeof(ALU_TYPE)/sizeof(PRUint16)>2? sizeof(ALU_TYPE)/sizeof(PRUint16): 2)
#define CCMAP_SURROGATE_FLAG         0x0001  
#define CCMAP_NONE_FLAG              0x0000

// get plane number from ccmap, bmp excluded, so plane 1's number is 0.
#define CCMAP_PLANE_FROM_SURROGATE(h)  ((((PRUint16)(h) - (PRUint16)0xd800) >> 6) + 1)

// same as above, but get plane number from a ucs4 char
#define CCMAP_PLANE(u)  ((((PRUint32)(u))>>16))

// scalar value inside the plane
#define CCMAP_INPLANE_OFFSET(h, l) (((((PRUint16)(h) - (PRUint16)0xd800) & 0x3f) << 10) + ((PRUint16)(l) - (PRUint16)0xdc00))

// get ccmap for that plane
#define CCMAP_FOR_PLANE_EXT(m, i)  ((m) + ((PRUint32*)((m) + CCMAP_SIZE(m)))[(i)-1])

// test the bit for surrogate pair
#define CCMAP_HAS_CHAR_EXT2(m, h, l)  (CCMAP_FLAG(m) & CCMAP_SURROGATE_FLAG && \
                                      CCMAP_HAS_CHAR(CCMAP_FOR_PLANE_EXT((m), CCMAP_PLANE_FROM_SURROGATE(h)), CCMAP_INPLANE_OFFSET(h, l)))
// test the bit for a character in UCS4
#define CCMAP_HAS_CHAR_EXT(m, ucs4)  (((ucs4)&0xffff0000) ?  \
                                      (CCMAP_FLAG(m) & CCMAP_SURROGATE_FLAG) && CCMAP_HAS_CHAR(CCMAP_FOR_PLANE_EXT((m), CCMAP_PLANE(ucs4)), (ucs4) & 0xffff) : \
                                      CCMAP_HAS_CHAR(m, (PRUnichar)(ucs4)) )

// macros to ensure that the array defining a pre-compiled CCMap starts
// at an ALU_TYPE boundary instead of just a PRUint16 boundary.
// When invoking the macro, 'typequal' should be either 'const' 
// or empty (NULL). see bug 224337.
     
#define DEFINE_ANY_CCMAP(var, extra, typequal)              \
static typequal union {                                     \
  PRUint16 array[var ## _SIZE];                             \
  ALU_TYPE align;                                           \
} var ## Union =                                            \
{                                                           \
  { var ## _INITIALIZER }                                   \
};                                                          \
static typequal PRUint16* var = var ## Union.array + extra

#define DEFINE_CCMAP(var, typequal)   DEFINE_ANY_CCMAP(var, 0, typequal)
#define DEFINE_X_CCMAP(var, typequal) DEFINE_ANY_CCMAP(var, CCMAP_EXTRA, typequal)

#endif // NSCOMPRESSEDCHARMAP_H 

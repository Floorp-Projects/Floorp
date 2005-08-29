/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *   Jungshik Shin <jshin@mailaps.org> 
 *   Frank Tang <ftang@netscape.com>
 *   Jin-Hwan Cho <chofchof@ktug.or.kr>
 *   Won-Kyu Park  <wkpark@chem.skku.ac.kr>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * - Purposes:
 *    1. Enable rendering  over 1.5 million Hangul syllables with 
 *       UnBatang and other fonts made available by UN KoaungHi
 *       and PARK Won-kyu.
 */
#include "nsUCvKODll.h"
#include "nsUnicodeToJamoTTF.h"
#include "prmem.h"
#include "nsXPIDLString.h"
#include "prtypes.h"
#include "nscore.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsIUnicodeDecoder.h"
#include "nsServiceManagerUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsICharRepresentable.h"
#include <string.h>

typedef struct { 
  PRUint8 seq[3];
  PRUint8 liga;
} JamoNormMap; 

// cluster maps 
#include "jamoclusters.h"

// Constants for Hangul Jamo/syllable handling taken from Unicode 3.0 
// section 3.11 

#define LBASE 0x1100
#define VBASE 0x1161
#define TBASE 0x11A7
#define TSTART 0x11A8
#define SBASE 0xAC00

#define LCOUNT 19
#define VCOUNT 21
#define TCOUNT 28
#define SCOUNT (LCOUNT * VCOUNT * TCOUNT)
#define SEND (SBASE + SCOUNT - 1)


#define LFILL 0x115F
#define VFILL 0x1160

#define IS_LC(wc) (LBASE <= (wc) && (wc) <  VFILL)
#define IS_VO(wc) (VFILL <= (wc) && (wc) <  TSTART)
#define IS_TC(wc) (TSTART <= (wc) && (wc) <= 0x11FF)
#define IS_JAMO(wc)   (IS_LC(wc) || IS_VO(wc) || IS_TC(wc))

// Jamos used in modern precomposed syllables 
#define IS_SYL_LC(wc) (LBASE <= (wc) && (wc) <  LBASE + LCOUNT)
#define IS_SYL_VO(wc) (VBASE <= (wc) && (wc) <  VBASE + VCOUNT)
#define IS_SYL_TC(wc) (TBASE <  (wc) && (wc) <= TBASE + TCOUNT)

// Modern precomposed syllables. 
#define IS_SYL(wc)   (SBASE <= (wc) && (wc) <= SEND)
#define IS_SYL_WO_TC(wc)  (((wc) - SBASE) % TCOUNT == 0)
#define IS_SYL_WITH_TC(wc)  (((wc) - SBASE) % TCOUNT)

// Compose precomposed syllables out of L, V, and T.
#define SYL_FROM_LVT(l,v,t) (SBASE + \
                             (((l) - LBASE) * VCOUNT + (v) - VBASE) * TCOUNT + \
                             (t) - TBASE)

// Hangul tone marks
#define HTONE1 0x302E
#define HTONE2 0x302F

#define IS_TONE(wc) ((wc) == HTONE1 || (wc) == HTONE2)

// Below are constants for rendering with UnBatang-like fonts.

#define LC_TMPPOS  0xF000 // temp. block for leading consonants
#define VO_TMPPOS  0xF100 // temp. block for vowels
#define TC_TMPPOS  0xF200 // temp. block for trailinng consonants
#define LC_OFFSET  (LC_TMPPOS-LBASE)
#define VO_OFFSET  (VO_TMPPOS-VFILL)
#define TC_OFFSET  (TC_TMPPOS-TSTART)

// Jamo class of *temporary* code points   in PUA for UnBatang-like fonts.
#define IS_LC_EXT(wc) ( ((wc) & 0xFF00) == LC_TMPPOS )
#define IS_VO_EXT(wc) ( ((wc) & 0xFF00) == VO_TMPPOS )
#define IS_TC_EXT(wc) ( ((wc) & 0xFF00) == TC_TMPPOS )

// Glyph code point bases for L,V, and T in  UnBatang-like fonts
#define UP_LBASE 0xE000  // 0xE000 = Lfill, 0xE006 = Kiyeok 
#define UP_VBASE 0xE300  // 0xE300 = Vfill, 0xE302 = Ah  
#define UP_TBASE 0xE404  // 0xE400 = Tfill, 0xE404 = Kiyeok

// EUC-KR decoder for FillInfo.
static nsCOMPtr<nsIUnicodeDecoder> gDecoder = 0;
  
static inline void FillInfoRange     (PRUint32* aInfo, PRUint32 aStart, 
                                      PRUint32 aEnd);
static nsresult     JamoNormalize    (const PRUnichar* aInSeq, 
                                      PRUnichar** aOutSeq, PRInt32* aLength);
static void         JamosToExtJamos  (PRUnichar* aInSeq,  PRInt32* aLength);
static const JamoNormMap* JamoClusterSearch(JamoNormMap aKey, 
                                            const JamoNormMap* aClusters,
                                            PRInt16 aClustersSize);
static nsresult     FillInfoEUCKR    (PRUint32 *aInfo, PRUint16 aHigh1, 
                                      PRUint16 aHigh2);

static PRInt32      JamoNormMapComp  (const JamoNormMap& p1, 
                                      const JamoNormMap& p2);
static PRInt16      JamoSrchReplace  (const JamoNormMap* aCluster, 
                                      PRUint16 aSize, PRUnichar *aIn, 
                                      PRInt32* aLength, PRUint16 aOffset);
static nsresult     GetDecoder       (nsIUnicodeDecoder** aDecoder);
static nsresult     ScanDecomposeSyllable (PRUnichar *aIn, PRInt32* aLength, 
                                           const PRInt32 aMaxLen);

//----------------------------------------------------------------------
// Class nsUnicodeToJamoTTF [implementation]
  
NS_IMPL_ISUPPORTS2(nsUnicodeToJamoTTF, nsIUnicodeEncoder, nsICharRepresentable)

NS_IMETHODIMP 
nsUnicodeToJamoTTF::SetOutputErrorBehavior(PRInt32 aBehavior, 
                                           nsIUnicharEncoder *aEncoder, 
                                           PRUnichar aChar)
{
  if (aBehavior == kOnError_CallBack && aEncoder == nsnull)
    return NS_ERROR_NULL_POINTER;
  NS_IF_RELEASE(mErrEncoder);
  mErrEncoder = aEncoder;
  NS_IF_ADDREF(mErrEncoder);
  
  mErrBehavior = aBehavior;
  mErrChar = aChar;
  return NS_OK;
}

// constructor and destructor

nsUnicodeToJamoTTF::nsUnicodeToJamoTTF() 
{
  mJamos = nsnull;
  Reset();
}

nsUnicodeToJamoTTF::~nsUnicodeToJamoTTF()
{
  if (mJamos != nsnull && mJamos != mJamosStatic)
    PR_Free(mJamos);
}

enum KoCharClass {
  KO_CHAR_CLASS_LC,
  KO_CHAR_CLASS_VO,  
  KO_CHAR_CLASS_TC,  
  KO_CHAR_CLASS_SYL1,   // modern precomposed syllable w/o TC (LV type syl.)
  KO_CHAR_CLASS_SYL2,   // modern precomposed syllable with TC (LVT type syl.)
  KO_CHAR_CLASS_TONE,   // Tone marks 
  KO_CHAR_CLASS_NOHANGUL, // Non-Hangul characters.
  KO_CHAR_CLASS_NUM
} ;

#define CHAR_CLASS(ch) \
  (IS_LC(ch) ? KO_CHAR_CLASS_LC   :  \
   IS_VO(ch) ? KO_CHAR_CLASS_VO   :  \
   IS_TC(ch) ? KO_CHAR_CLASS_TC   :  \
   IS_SYL(ch) ?                      \
    (IS_SYL_WITH_TC(ch) ? KO_CHAR_CLASS_SYL2 : KO_CHAR_CLASS_SYL1) : \
   IS_TONE(ch) ? KO_CHAR_CLASS_TONE : \
   KO_CHAR_CLASS_NOHANGUL)


// Grapheme boundary checker : See UTR #29 and Unicode 3.2 section 3.11
const static PRBool gIsBoundary[KO_CHAR_CLASS_NUM][KO_CHAR_CLASS_NUM] = 
{// L  V  T  S1 S2 M  X
  { 0, 0, 1, 0, 0, 0, 1 }, // L  
  { 1, 0, 0, 1, 1, 0, 1 }, // V
  { 1, 1, 0, 1, 1, 0, 1 }, // T
  { 1, 0, 0, 1, 1, 0, 1 }, // S1
  { 1, 1, 0, 1, 1, 0, 1 }, // S2
  { 1, 1, 1, 1, 1, 0, 1 }, // M
  { 1, 1, 1, 1, 1, 0, 1 }  // X
};


NS_IMETHODIMP 
nsUnicodeToJamoTTF::Convert(const PRUnichar * aSrc, 
                            PRInt32 * aSrcLength, char * aDest, 
                            PRInt32 * aDestLength)
{
  nsresult rv = NS_OK;
  mByteOff = 0;

  // This should never happen, but it happens under MS Windows, somehow...
  if (mJamoCount > mJamosMaxLength) 
  {
    NS_WARNING("mJamoCount > mJamoMaxLength on entering Convert()");
    Reset();
  }

  for (PRInt32 charOff = 0; charOff < *aSrcLength; charOff++)
  {
    PRUnichar ch = aSrc[charOff];

    // Syllable boundary check. Ref. : Unicode 3.2 section 3.11 
    if (mJamoCount != 0 &&
        gIsBoundary[CHAR_CLASS(mJamos[mJamoCount - 1])][CHAR_CLASS(ch)])
    {
      composeHangul(aDest);
      mJamoCount = 0;
    }
    // Ignore tone marks other than the first in a sequence of tone marks.
    else if (mJamoCount != 0 && IS_TONE(mJamos[mJamoCount - 1]) && IS_TONE(ch))
    {
      --mJamoCount; 
      composeHangul(aDest);
      mJamoCount = 0;

      // skip over tone marks from the second on in a series.
      while (IS_TONE(ch) && ++charOff < *aSrcLength)
        ch = aSrc[charOff]; 

      if (!IS_TONE(ch)) 
      {
        mJamos[mJamoCount++] = ch; 
        continue;
      }
      else
        break;
    }

    if (mJamoCount == mJamosMaxLength)
    {
      mJamosMaxLength++;
      if (mJamos == mJamosStatic)
      {
        mJamos = (PRUnichar *) PR_Malloc(sizeof(PRUnichar) * mJamosMaxLength);
        if (!mJamos)
          return  NS_ERROR_OUT_OF_MEMORY;
        memcpy(mJamos, mJamosStatic, sizeof(PRUnichar) * mJamoCount);
      }
      else
      {
        mJamos = (PRUnichar *) PR_Realloc(mJamos, 
                               sizeof(PRUnichar) * mJamosMaxLength);
        if (!mJamos)
          return  NS_ERROR_OUT_OF_MEMORY;
      }
    }

    mJamos[mJamoCount++] = ch;
  }
    
  if (mJamoCount != 0)
    composeHangul(aDest);
  mJamoCount = 0;
  *aDestLength = mByteOff;

  return rv;
}

NS_IMETHODIMP 
nsUnicodeToJamoTTF::Finish(char* aDest, PRInt32* aDestLength)
{
  mByteOff = 0;
  if (mJamoCount != 0)
    composeHangul(aDest);

  *aDestLength = mByteOff;

  mByteOff = 0;
  mJamoCount = 0;
  return NS_OK;
}

//================================================================
NS_IMETHODIMP 
nsUnicodeToJamoTTF::Reset()
{

  if (mJamos != nsnull && mJamos != mJamosStatic)
    PR_Free(mJamos);
  mJamos = mJamosStatic;
  mJamosMaxLength = sizeof(mJamosStatic) / sizeof(PRUnichar);
  memset(mJamos, 0, sizeof(mJamosStatic));
  mJamoCount = 0;
  mByteOff = 0;

  return NS_OK;
}

NS_IMETHODIMP 
nsUnicodeToJamoTTF::GetMaxLength(const PRUnichar * aSrc, PRInt32 aSrcLength,
                                 PRInt32 * aDestLength)
{
  // a precomposed Hangul syllable can be decomposed into 3 Jamos, each of
  // which takes 2bytes. 
  *aDestLength = aSrcLength *  6;
  return NS_OK;
}


NS_IMETHODIMP 
nsUnicodeToJamoTTF::FillInfo(PRUint32* aInfo)
{
  FillInfoRange(aInfo, SBASE, SEND);

  PRUnichar i;

  // Hangul Conjoining Jamos
  for(i = 0x1100; i<= 0x1159; i++)
     SET_REPRESENTABLE(aInfo, i);
  SET_REPRESENTABLE(aInfo, 0x115f);
  for(i = 0x1160; i <= 0x11a2; i++)
     SET_REPRESENTABLE(aInfo, i);
  for(i = 0x11a8; i <= 0x11f9; i++)
     SET_REPRESENTABLE(aInfo, i);

  // Hangul Tone marks
  SET_REPRESENTABLE(aInfo, HTONE1);
  SET_REPRESENTABLE(aInfo, HTONE2);

  // UnPark  fonts have US-ASCII chars.
  for(i=0x20; i < 0x7f; i++)
     SET_REPRESENTABLE(aInfo, i);

  nsresult rv;

  // UnPark fonts have Hanjas and symbols defined in KS X 1001 as well.
  
  // XXX: Do we need to exclude Cyrillic, Greek letters and some Latin letters 
  // included in KS X 1001 as 'symbol characters'? 
  // KS X 1001 has only a subset of Greek and Cyrillic alphabets and
  // Latin letters with diacritic marks so that including them may
  // result in ransom-note like effect if it is listed *before*
  // any genuine Greek/Russian/Latin fonts in CSS. 
    
  // Lead byte range for symbol chars. in EUC-KR : 0xA1 - 0xAF
  rv = FillInfoEUCKR(aInfo, 0xA1, 0xAF); 
  NS_ENSURE_SUCCESS(rv, rv);

  // Lead byte range for Hanja in EUC-KR : 0xCA - 0xFD.
  return FillInfoEUCKR(aInfo, 0xCA, 0xFD); 
}

/**
 * Copied from mslvt.otp by Jin-Hwan Cho <chofchof@ktug.or.kr>.
 * Extended by Jungshik Shin <jshin@mailaps.org> to support
 * additional Jamo clusters not encoded in U+1100 Jamo block
 * as precomposed Jamo clsuters. 
 * Corrected by Won-Kyu Park <wkpark@chem.skku.ac.kr>.
 * See http://www.ktug.or.kr for its use in Lambda and swindow/SFontTTF.cpp at  
 * http://www.yudit.org for its use in Yudit.
 * A patch with the same set of tables was submitted for
 * inclusion in Pango (http://www.pango.org).
 */

/**
 * Mapping from LC code points  to glyph indices in UnPark fonts.
 * UnPark fonts have the same glyph arrangement as Ogulim font, but
 * they have them in BMP PUA (beginning at U+E000) to be proper Unicode 
 * fonts unlike Ogulim font with Jamo glyphs in CJK ideograph code points.
 * Glyph indices for 90 LCs encoded in U+1100 block are followed by  6 reserved 
 * code points  and  glyph indices for 34 additional consonant  clusters 
 * (not assigned code points of their own)  for which separate glyphs exist in 
 * UnPark fonts.
 * The first element is for Kiyeok and UP_LBASE is set to Lfill glyph(0xe000)
 * so that the first element is '1' to map it to glyph for Kiyeok at 0xe006.
 * (there are six glyphs for each LC in UnPark fonts.)
 */
const static PRUint8 gUnParkLcGlyphMap[130] = {
  1,  2,  4, 12, 14, 20, 36, 42, 46, 62, 70, 85,100,102,108,113,
114,116,120,  5,  6,  7,  8, 13, 23, 26, 34, 35, 39, 41, 43, 44,
 45, 47, 48, 49, 50, 51, 52, 54, 55, 57, 58, 60, 61, 63, 64, 65,
 66, 67, 68, 69, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
 84, 86, 87, 89, 90, 91, 92, 93, 94, 95, 96, 97, 99,101,104,105,
106,107,109,110,111,112,117,119,122,123,  0,  0,  0,  0,  0,  0,
  3,  9, 10, 11, 15, 16, 17, 18, 19, 21, 22, 24, 25, 27, 28, 29,
 30, 31, 32, 33, 37, 38, 40, 53, 56, 59, 71, 88, 98,103,115,118,
121, 124
};

/**
 * Mapping from vowel code points  to glyph indices in UnPark/Oxxx font.
 * Glyphs for 28 additional vowel clusters (not given separate
 * code points in U+1100 block) are available in O*ttf fonts.
 * Total count: 95 = 1(Vfill) + 66 (in U+1100 block) + 28 (extra.)
 */
const static PRUint8 gUnParkVoGlyphMap[95] = {
   0,  1,  5,  6, 10, 11, 15, 16, 20, 21, 22, 23, 33, 34, 43, 46, 
  48, 52, 54, 64, 71, 73,  2,  3,  7,  8, 12, 13, 14, 18, 19, 26, 
  27, 29, 30, 32, 37, 38, 40, 41, 42, 44, 45, 47, 50, 51, 55, 57, 
  58, 59, 60, 62, 63, 69, 70, 72, 74, 75, 80, 83, 85, 87, 88, 90, 
  92, 93, 94,  4,  9, 17, 24, 25, 28, 31, 35, 36, 39, 49, 53, 56, 
  61, 65, 66, 67, 68, 76, 77, 78, 79, 81, 82, 84, 86, 89, 91
};

/**
 * Mapping from TC code points  to glyph indices in UnPark/Oxxx font.
 * glyphs for 59 additional trailing consonant clusters (not given separate
 * code points in U+1100 blocks) are available in O*ttf fonts.
 * Total count: 141 = 82 (in U+1100 block) + 59 (extra.)
 * The first element is Kiyeok and UP_TBASE is set to 0x5204 (Kiyeok).
 */
const static PRUint8 gUnParkTcGlyphMap[141] = {
   0,  1,  5, 10, 17, 20, 21, 32, 33, 42, 46, 52, 57, 58, 59, 63,
  78, 84, 91, 98,109,123,127,128,129,130,135,  3,  6, 11, 13, 15,
  16, 19, 22, 25, 35, 37, 38, 39, 40, 43, 44, 48, 50, 51, 53, 54,
  56, 60, 64, 67, 69, 71, 72, 73, 75, 76, 77, 80, 88, 89, 90, 92,
  93, 94, 96,106,110,111,114,115,117,119,120,131,134,136,137,138,
 139,140,  2,  4,  7,  8,  9, 12, 14, 18, 23, 24, 26, 27, 28, 29,
  30, 31, 34, 36, 41, 45, 47, 49, 55, 61, 62, 65, 66, 68, 70, 74,
  79, 81, 82, 83, 85, 86, 87, 95, 97, 99,100,101,102,103,104,105,
 107,108,112,113,116,118,121,122,124,125,126,132,133
};

/* Which of six glyphs to use for choseong(L) depends on 
   the following vowel and whether or not jongseong(T) is present
   in a syllable. Note that The first(0th) element is for Vfill.

   shape Number of choseong(L) w.r.t. jungseong(V) without jongseong(T)

   95 = 1(Vfill) + 66 + 28 (extra)
*/
 
const static PRUint8 gUnParkVo2LcMap[95] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 1, 2, 2, 1,
  1, 1, 2, 2, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 2, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1,
  1, 1, 1, 2, 1, 2, 2, 1, 0, 0, 1, 1, 1, 0, 2, 1,
  2, 1, 2, 1, 1, 0, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1,
  2, 1, 1, 1, 2, 1, 0, 0, 0, 1, 1, 1, 0, 2, 2
};

/* shape Number of choseong(L) w.r.t. jungseong(V) with jongseong(T) */

const static PRUint8 gUnParkVo2LcMap2[95] = {
  3, 3, 3, 3, 3, 3, 3, 3, 3, 5, 4, 4, 4, 5, 5, 4,
  4, 4, 5, 5, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  4, 4, 5, 5, 4, 4, 4, 5, 4, 4, 4, 4, 4, 5, 4, 4,
  4, 4, 4, 5, 4, 5, 5, 4, 3, 3, 4, 4, 4, 3, 5, 4,
  5, 4, 5, 4, 4, 3, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4,
  5, 4, 4, 4, 5, 4, 3, 3, 3, 4, 4, 4, 3, 5, 5
};

/* shape Number of jongseong(T) w.r.t. jungseong(V)
   Which of four glyphs to use for jongseong(T) depends on 
   the preceding vowel. */

const static PRUint8 gUnParkVo2TcMap[95] = {
  3, 0, 2, 0, 2, 1, 2, 1, 2, 3, 0, 2, 1, 3, 3, 1,
  2, 1, 3, 3, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
  2, 2, 3, 3, 0, 2, 1, 3, 1, 0, 2, 1, 2, 3, 0, 1,
  2, 1, 2, 3, 1, 3, 3, 1, 2, 2, 1, 1, 1, 1, 3, 1,
  3, 1, 3, 0, 1, 0, 0, 0, 2, 3, 0, 2, 1, 1, 2, 2,
  3, 0, 0, 0, 3, 0, 2, 2, 2, 1, 0, 1, 2, 1, 1
};

NS_IMETHODIMP 
nsUnicodeToJamoTTF::composeHangul(char* aResult)
{
  PRInt32 length = mJamoCount, i;
  nsresult rv = NS_OK;

  if (!length)
  {
    NS_WARNING("composeHangul() : zero length string comes in ! \n");
    return NS_ERROR_UNEXPECTED;
  }

  if (!aResult) 
    return NS_ERROR_NULL_POINTER;

  // Put Hangul tone mark first as it should be to the left of 
  // the character it follows.
  // XXX : What should we do when a tone mark come by itself?
  
  if (IS_TONE(mJamos[length - 1])) 
  {
    aResult[mByteOff++] = PRUint8(mJamos[length - 1] >> 8);
    aResult[mByteOff++] = PRUint8(mJamos[length - 1] & 0xff); 
    if (--length == 0)
      return rv;
  }

  // no more processing is necessary for precomposed modern Hangul syllables.
  if (length == 1 && IS_SYL(mJamos[0])) 
  {
    aResult[mByteOff++] = PRUint8(mJamos[0] >> 8);
    aResult[mByteOff++] = PRUint8(mJamos[0] & 0xff); 
    return rv;
  }

  if (CHAR_CLASS(mJamos[0]) == KO_CHAR_CLASS_NOHANGUL) 
  {
    NS_ASSERTION(length == 1, "A non-Hangul should come by itself !!\n");
    aResult[mByteOff++] = PRUint8(mJamos[0] >> 8);
    aResult[mByteOff++] = PRUint8(mJamos[0] & 0xff); 
    return rv;
  }

  nsXPIDLString buffer;

  rv =  JamoNormalize(mJamos, getter_Copies(buffer), &length);

  // safe to cast away const.
  PRUnichar* text = buffer.BeginWriting();
  NS_ENSURE_SUCCESS(rv, rv);

  text += RenderAsPrecompSyllable(text, &length, aResult);

  if (!length)
    return rv;

  // convert to extended Jamo sequence
  JamosToExtJamos(text, &length);


  // Check if not in LV or LVT form after the conversion
  if (length != 2 && length != 3 ||
      (!IS_LC_EXT(text[0]) || !IS_VO_EXT(text[1]) ||
       (length == 3 && !IS_TC_EXT(text[2]))))
    goto fallback;

//  Now that text[0..2] are identified as L,V, and T, it's safe to 
//  shift them back to U+1100 block although their ranges overlap each other.
  
  text[0] -= LC_OFFSET; 
  text[1] -= VO_OFFSET; 
  if (length == 3)
    text[2] -= TC_OFFSET;

  if (length != 3)
  {
    text[0] = gUnParkLcGlyphMap[text[0] - LBASE] * 6 + 
              gUnParkVo2LcMap[text[1] - VFILL] + UP_LBASE;
    text[1] = gUnParkVoGlyphMap[text[1] - VFILL] * 2 + UP_VBASE;
  }
  else 
  {
    text[0] = gUnParkLcGlyphMap[text[0] - LBASE] * 6 + 
              gUnParkVo2LcMap2[text[1] - VFILL] + UP_LBASE;
    text[2] = gUnParkTcGlyphMap[text[2] - TSTART] * 4 + 
              gUnParkVo2TcMap[text[1] - VFILL] + UP_TBASE; 
    text[1] = gUnParkVoGlyphMap[text[1] - VFILL] * 2 + UP_VBASE + 1; 
  }

  // Xft doesn't like blank glyphs at code points other than listed in 
  // the blank glyph list. Replace Lfill glyph code points of UnPark
  // fonts with standard LFILL code point (U+115F).
    
  if (UP_LBASE <= text[0] && text[0] < UP_LBASE + 6)
    text[0] = LFILL;

  // The same is true of glyph code points corresponding to VFILL
  // in UnBatang-like fonts. VFILL is not only blank but also non-advancing
  // so that we can just skip it. 
  if (UP_VBASE <= text[1] && text[1] < UP_VBASE + 2)
  {
    --length;
    if (length == 2) 
      text[1] = text[2]; 
  }

  for (i = 0 ; i < length; i++)
  {
    aResult[mByteOff++] = PRUint8(text[i] >> 8);
    aResult[mByteOff++] = PRUint8(text[i] & 0xff);
  }

  return rv;


  /* If jamo sequence is not convertible to a jamo cluster,
   * just enumerate stand-alone jamos. Prepend V and T with  Lf.
   *
   * XXX: It might be better to search for a sub-sequence (not just at the
   * beginning of a cluster but also in the middle or at the end.) 
   * that can be rendered as precomposed and render it as such and enumerate
   * jamos in the rest. This approach is useful when a simple Xkb-based input
   * is used. 
   */

fallback: 
  for (i = 0; i < length; i++)
  {
    PRUnichar wc=0, wc2=0;
    /* skip Lfill and Vfill if they're not the sole char. in a cluster */
    if (length > 1 && 
         (text[i] - LC_OFFSET == LFILL || text[i] - VO_OFFSET == VFILL))
      continue;
    else if (IS_LC_EXT (text[i]))
       wc = gUnParkLcGlyphMap[text[i] - LC_OFFSET - LBASE] * 6 + UP_LBASE;
    else 
    {
  /* insert Lfill glyph to advance cursor pos. for V and T */
      wc = LBASE;
  /* don't have to draw Vfill. Drawing Lfill is sufficient. */ 
      if (text[i] - VO_OFFSET != VFILL) 
        wc2 = IS_VO_EXT (text[i]) ? 
        gUnParkVoGlyphMap[text[i] - VO_OFFSET - VFILL] * 2 + UP_VBASE:
        gUnParkTcGlyphMap[text[i] - TC_OFFSET - TSTART] * 4 + UP_TBASE + 3;
    }
    aResult[mByteOff++] = PRUint8(wc >> 8);
    aResult[mByteOff++] = PRUint8(wc & 0xff);

    if (wc2) 
    {
      aResult[mByteOff++] = wc2 >> 8;
      aResult[mByteOff++] = wc2 & 0xff; 
    }
  }

  return rv;
}

int
nsUnicodeToJamoTTF::RenderAsPrecompSyllable (PRUnichar* aSrc, 
                                             PRInt32* aSrcLength, char* aResult)
{

  int composed = 0;

  if (*aSrcLength == 3 && IS_SYL_LC(aSrc[0]) && IS_SYL_VO(aSrc[1]) && 
      IS_SYL_TC(aSrc[2]))
    composed = 3;
  else if (*aSrcLength == 2 && IS_SYL_LC(aSrc[0]) && IS_SYL_VO(aSrc[1]))
    composed = 2;
  else
    composed = 0;

  if (composed)
  {
    PRUnichar wc;
    if (composed == 3)
      wc = SYL_FROM_LVT(aSrc[0], aSrc[1], aSrc[2]);
    else
      wc = SYL_FROM_LVT(aSrc[0], aSrc[1], TBASE);
    aResult[mByteOff++] = PRUint8(wc >> 8);
    aResult[mByteOff++] = PRUint8(wc & 0xff);
  }

  *aSrcLength -= composed;

  return composed;
}

// Fill up Cmap array quickly for a rather large range.
/* static */
inline void FillInfoRange(PRUint32* aInfo, PRUint32 aStart, PRUint32 aEnd)
{

  PRUint32 b = aStart >> 5; 
  PRUint32 e = aEnd >> 5;

  if (aStart & 0x1f)
    aInfo[b++] |= ~ (0xFFFFFFFFL >> (32 - ((aStart) & 0x1f)));

  for( ; b < e ; b++)
    aInfo[b] |= 0xFFFFFFFFL;

  aInfo[e] |= (0xFFFFFFFFL >> (31 - ((aEnd) & 0x1f)));
}


#define ROWLEN 94
#define IS_GR94(x) (0xA0 < (x) && (x) < 0xFF)

// Given a range [aHigh1, aHigh2] in high bytes of EUC-KR, convert 
// rows of 94 characters in the range (row by row) to Unicode and set 
// representability if the result is not 0xFFFD (Unicode replacement char.).
/* static */
nsresult FillInfoEUCKR (PRUint32 *aInfo, PRUint16 aHigh1, PRUint16 aHigh2)
{
  char row[ROWLEN * 2];
  PRUnichar dest[ROWLEN];
  nsresult rv = NS_OK;

  NS_ENSURE_TRUE(aInfo, NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(IS_GR94(aHigh1) && IS_GR94(aHigh2), NS_ERROR_INVALID_ARG);

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  rv = GetDecoder(getter_AddRefs(decoder));
  NS_ENSURE_SUCCESS(rv,rv);

  for (PRUint16 i = aHigh1 ; i <= aHigh2; i++)
  {
    PRUint16 j;
    // handle a row of 94 char. at a time.
    for (j = 0 ; j < ROWLEN; j++)
    {
      row[j * 2] = char(i);
      row[j * 2 + 1] = char(j + 0xa1);
    }
    PRInt32 srcLen = ROWLEN * 2;
    PRInt32 destLen = ROWLEN;
    rv = decoder->Convert(row, &srcLen, dest, &destLen);
    NS_ENSURE_SUCCESS(rv, rv);

    // set representability according to the conversion result.
    for (j = 0 ; j < ROWLEN; j++)
      if (dest[j] != 0xFFFD)
        SET_REPRESENTABLE(aInfo, dest[j]);
  }
  return rv;
}

/* static */
nsresult GetDecoder(nsIUnicodeDecoder** aDecoder)
{
  nsresult rv; 

  if (gDecoder) {
    *aDecoder = gDecoder.get();
    NS_ADDREF(*aDecoder);
    return NS_OK;
  }

  nsCOMPtr<nsICharsetConverterManager> charsetConverterManager;
  charsetConverterManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  rv = charsetConverterManager->GetUnicodeDecoderRaw("EUC-KR", getter_AddRefs(gDecoder));
  NS_ENSURE_SUCCESS(rv,rv);

  *aDecoder = gDecoder.get();
  NS_ADDREF(*aDecoder);
  return NS_OK;
}


/* static */
PRInt32 JamoNormMapComp (const JamoNormMap& p1, const JamoNormMap& p2)
{
  if (p1.seq[0] != p2.seq[0]) 
    return p1.seq[0] - p2.seq[0];
  if (p1.seq[1] != p2.seq[1]) 
    return p1.seq[1] - p2.seq[1];
  return p1.seq[2] - p2.seq[2];
}

/* static */
const JamoNormMap* JamoClusterSearch (JamoNormMap aKey, 
                                const JamoNormMap* aClusters, 
                                PRInt16 aClustersSize)
{

  if (aClustersSize <= 0 || !aClusters)
  {
    NS_WARNING("aClustersSize <= 0 || !aClusters");
    return nsnull;
  }

  if (aClustersSize < 9) 
  {
    PRInt16 i;
    for (i = 0; i < aClustersSize; i++)
      if (JamoNormMapComp (aKey, aClusters[i]) == 0) 
        return aClusters + i; 
    return nsnull;
  }
   
  PRUint16 l = 0, u = aClustersSize - 1;
  PRUint16 h = (l + u) / 2;

  if (JamoNormMapComp (aKey, aClusters[h]) < 0) 
    return JamoClusterSearch(aKey, &(aClusters[l]), h - l);   
  else if (JamoNormMapComp (aKey, aClusters[h]) > 0) 
    return JamoClusterSearch(aKey, &(aClusters[h + 1]), u - h);   
  else
    return aClusters + h;

}


/*
 *  look up cluster array for all possible matching Jamo sequences 
 *  in 'aIn' and  replace all matching substrings with match->liga in place. 
 *  returns the difference in aLength between before and after the replacement.
 *  XXX : 1. Do we need caching here? 
 **/

/* static */
PRInt16 JamoSrchReplace (const JamoNormMap* aClusters, 
                         PRUint16 aClustersSize, PRUnichar* aIn, 
                         PRInt32* aLength, PRUint16 aOffset)
{
  PRInt32 origLen = *aLength; 

  // non-zero third element => clusternLen = 3. otherwise, it's 2.
  PRUint16 clusterLen = aClusters[0].seq[2] ? 3 : 2; 

  PRInt32 start = 0, end;

  // identify the substring of aIn with values in [aOffset, aOffset + 0x100).
  while (start < origLen && (aIn[start] & 0xff00) != aOffset)
    ++start;
  for (end=start; end < origLen && (aIn[end] & 0xff00) == aOffset; ++end);

  // now process the substring aIn[start] .. aIn[end] 
  // we don't need a separate range check here because the one in 
  // for-loop is sufficient.
  for (PRInt32 i = start; i <= end - clusterLen; i++)
  {
    const JamoNormMap *match;
    JamoNormMap key;

    // cluster array is made up of PRUint8's to save memory
    // and we have to subtract aOffset from the input before looking it up.
    key.seq[0] = aIn[i] - aOffset;
    key.seq[1] = aIn[i + 1] - aOffset;
    key.seq[2] = clusterLen == 3 ? (aIn[i + 2] - aOffset) : 0;

    match = JamoClusterSearch (key, aClusters, aClustersSize);

    if (match) 
    {
      aIn[i] = match->liga + aOffset; // add back aOffset. 

      // move up the 'tail'
      for (PRInt32 j = i + clusterLen ; j < *aLength; j++)
        aIn[j - clusterLen + 1] = aIn[j];

      end -= (clusterLen - 1);
      *aLength -= (clusterLen - 1);
    }
  }

  return *aLength - origLen;
}

/* static */
nsresult ScanDecomposeSyllable(PRUnichar* aIn, PRInt32 *aLength, 
                               const PRInt32 maxLength)
{
  nsresult rv = NS_OK;

  if (!aIn || *aLength < 1 || maxLength < *aLength + 2)
    return NS_ERROR_INVALID_ARG;

  PRInt32 i = 0;
  while (i < *aLength && !IS_SYL(aIn[i]))
    i++;

  // Convert a precomposed syllable to an LV or LVT sequence.
  if (i < *aLength && IS_SYL(aIn[i]))
  {
    PRUint16 j = IS_SYL_WITH_TC(aIn[i]) ? 1 : 0; 
    aIn[i] -= SBASE;
    memmove(aIn + i + 2 + j, aIn + i + 1, *aLength - i - 1);
    if (j)
      aIn[i + 2] = aIn[i] % TCOUNT + TBASE;
    aIn[i + 1] = (aIn[i] / TCOUNT) % VCOUNT + VBASE;
    aIn[i] = aIn[i] / (TCOUNT * VCOUNT) + LBASE;
    *aLength += 1 + j;
  }

  return rv;
}

/*
 *  1. Normalize (regularize) a jamo sequence to the regular
 *     syllable form defined in Unicode 3.2 section 3.11 to the extent
 *     that it's useful in rendering by render_func's().
 *
 *  2. Replace a compatibly decomposed Jamo sequence (unicode 2.0 
 *     definition) with a 'precomposed' Jamo cluster (with codepoint
 *     of its own in U+1100 block). For instance, a seq.
 *     of U+1100, U+1100 is replaced by U+1101. It actually
 *     more than Unicode 2.0 decomposition map suggests.
 *     For a Jamo cluster made up of three basic Jamos
 *     (e.g. U+1133 : Sios, Piup, Kiyeok), not only
 *      a sequence of Sios(U+1109), Piup(U+1107) and 
 *     Kiyeok(U+1100) but also two more sequences,
 *     {U+1132(Sios-Pieup), U+1100(Kiyeok) and {Sios(U+1109),
 *      U+111E(Piup-Kiyeok)} are mapped to U+1133.
 *
 *  3. the result is returned in a newly malloced
 *     PRUnichar*. Callers have to delete it, which 
 *     is taken care of by using nsXPIDLString in caller.
 */

/* static */
nsresult JamoNormalize(const PRUnichar* aInSeq, PRUnichar** aOutSeq, 
                       PRInt32* aLength) 
{
  if (!aInSeq || !aOutSeq || *aLength <= 0)
    return NS_ERROR_INVALID_ARG;

  // 4 more slots : 2 for Lf and Vf, 2 for decomposing a modern precomposed 
  // syllable into a Jamo sequence of LVT?. 
  *aOutSeq = new PRUnichar[*aLength + 4]; 
  if (!*aOutSeq)
    return NS_ERROR_OUT_OF_MEMORY;
  memcpy(*aOutSeq, aInSeq, *aLength * sizeof(PRUnichar));

  nsresult rv = ScanDecomposeSyllable(*aOutSeq, aLength, *aLength + 4);
  NS_ENSURE_SUCCESS(rv, rv);

  // LV or LVT : no need to search for and replace jamo sequences 
  if ((*aLength == 2 && IS_LC((*aOutSeq)[0]) && IS_VO((*aOutSeq)[1])) || 
      (*aLength == 3 && IS_LC((*aOutSeq)[0]) && IS_VO((*aOutSeq)[1]) && 
      IS_TC((*aOutSeq)[2])))
    return NS_OK;

  // remove Lf in LfL sequence that may occur in an interim cluster during
  // a simple Xkb-based input. 
  if ((*aOutSeq)[0] == LFILL && *aLength > 1 && IS_LC((*aOutSeq)[1]))
  {
    memmove (*aOutSeq, *aOutSeq + 1, (*aLength - 1) * sizeof(PRUnichar)); 
    (*aLength)--;
  }

  if (*aLength > 1)
  {
    JamoSrchReplace (gJamoClustersGroup1,
        sizeof(gJamoClustersGroup1) / sizeof(gJamoClustersGroup1[0]), 
        *aOutSeq, aLength, LBASE);
    JamoSrchReplace (gJamoClustersGroup234,
        sizeof(gJamoClustersGroup234) / sizeof(gJamoClustersGroup234[0]), 
        *aOutSeq, aLength, LBASE);
  }

  // prepend a leading V with Lf 
  if (IS_VO((*aOutSeq)[0])) 
  {
     memmove(*aOutSeq + 1, *aOutSeq, *aLength * sizeof(PRUnichar));
    (*aOutSeq)[0] = LFILL;
    (*aLength)++;
  }
  /* prepend a leading T with LfVf */
  else if (IS_TC((*aOutSeq)[0])) 
  {
    memmove (*aOutSeq + 2, *aOutSeq, *aLength * sizeof(PRUnichar));
    (*aOutSeq)[0] = LFILL;
    (*aOutSeq)[1] = VFILL;
    *aLength += 2;
  }
  return NS_OK;
}


/*  JamosToExtJamos() :
 *  1. shift jamo sequences to three disjoint code blocks in
 *     PUA (0xF000 for LC, 0xF1000 for VO, 0xF200 for TC).
 *  2. replace a jamo sequence with a precomposed extended 
 *     cluster jamo code point in PUA
 *  3. this replacement is done 'in place' 
 */

/* static */
void JamosToExtJamos (PRUnichar* aInSeq,  PRInt32* aLength)
{
  // translate jamo code points to temporary code points in PUA
  for (PRInt32 i = 0; i < *aLength; i++)
  {
    if (IS_LC(aInSeq[i]))
      aInSeq[i] += LC_OFFSET;
    else if (IS_VO(aInSeq[i]))
      aInSeq[i] += VO_OFFSET;
    else if (IS_TC(aInSeq[i]))
      aInSeq[i] += TC_OFFSET;
  }

  // LV or LVT : no need to search for and replace jamo sequences 
  if ((*aLength == 2 && IS_LC_EXT(aInSeq[0]) && IS_VO_EXT(aInSeq[1])) || 
      (*aLength == 3 && IS_LC_EXT(aInSeq[0]) && IS_VO_EXT(aInSeq[1]) && 
       IS_TC_EXT(aInSeq[2])))
    return;

  // replace a sequence of Jamos with the corresponding precomposed 
  // Jamo cluster in PUA 
    
  JamoSrchReplace (gExtLcClustersGroup1, 
      sizeof (gExtLcClustersGroup1) / sizeof (gExtLcClustersGroup1[0]), 
      aInSeq, aLength, LC_TMPPOS); 
  JamoSrchReplace (gExtLcClustersGroup2,
       sizeof (gExtLcClustersGroup2) / sizeof (gExtLcClustersGroup2[0]), 
       aInSeq, aLength, LC_TMPPOS);
  JamoSrchReplace (gExtVoClustersGroup1,
       sizeof (gExtVoClustersGroup1) / sizeof (gExtVoClustersGroup1[0]), 
       aInSeq, aLength, VO_TMPPOS);
  JamoSrchReplace (gExtVoClustersGroup2, 
       sizeof (gExtVoClustersGroup2) / sizeof (gExtVoClustersGroup2[0]), 
       aInSeq, aLength, VO_TMPPOS);
  JamoSrchReplace (gExtTcClustersGroup1, 
       sizeof (gExtTcClustersGroup1) / sizeof (gExtTcClustersGroup1[0]), 
       aInSeq, aLength, TC_TMPPOS);
  JamoSrchReplace (gExtTcClustersGroup2, 
       sizeof (gExtTcClustersGroup2) / sizeof (gExtTcClustersGroup2[0]), 
       aInSeq, aLength, TC_TMPPOS);
    return;
}


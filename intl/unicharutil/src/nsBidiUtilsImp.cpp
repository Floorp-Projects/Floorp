/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.  Portions created by IBM are
 * Copyright (C) 2000 IBM Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCom.h"
#include "pratom.h"
#include "nsUUDll.h"
#include "nsISupports.h"
#include "nsBidiUtilsImp.h"
#include "bidicattable.h"
#include "symmtable.h"

#include "nsIServiceManager.h"
#include "nsIUBidiUtils.h"
#include "nsIBidi.h"
static NS_DEFINE_CID(kBidiCID, NS_BIDI_CID);
NS_IMPL_ISUPPORTS(nsBidiUtilsImp, NS_GET_IID(nsIUBidiUtils));

static nsCharType ebc2ucd[15] = {
    eCharType_OtherNeutral, /* Placeholder -- there will never be a 0 index value */
    eCharType_LeftToRight,
    eCharType_RightToLeft,
    eCharType_RightToLeftArabic,
    eCharType_ArabicNumber,
    eCharType_EuropeanNumber,
    eCharType_EuropeanNumberSeparator,
    eCharType_EuropeanNumberTerminator,
    eCharType_CommonNumberSeparator,
    eCharType_OtherNeutral,
    eCharType_DirNonSpacingMark,
    eCharType_BoundaryNeutral,
    eCharType_BlockSeparator,
    eCharType_SegmentSeparator,
    eCharType_WhiteSpaceNeutral
};

static nsCharType cc2ucd[5] = {
    eCharType_LeftToRightEmbedding,
    eCharType_RightToLeftEmbedding,
    eCharType_PopDirectionalFormat,
    eCharType_LeftToRightOverride,
    eCharType_RightToLeftOverride
};

// the Array Index = FE_CHAR - FE_TO_06_OFFSET

#define FE_TO_06_OFFSET 0xfe70

static PRUint16  FE_TO_06 [][2] = {
    {0x064a,0x0000},{0x064a,0x0640},{0x064c,0x0000},
    {0x0000,0x0000},{0x064d,0x0000},{0x0000,0x0000},
    {0x064e,0x0000},{0x064e,0x0640},{0x064f,0x0000},
    {0x064f,0x0640},{0x0650,0x0000},{0x0650,0x0640},
    {0x0651,0x0000},{0x0651,0x0640},{0x0652,0x0000},
    {0x0652,0x0640},{0x0621,0x0000},{0x0622,0x0000},
    {0x0622,0x0000},{0x0623,0x0000},{0x0623,0x0000},
    {0x0624,0x0000},{0x0624,0x0000},{0x0625,0x0000},
    {0x0625,0x0000},{0x0626,0x0000},{0x0626,0x0000},
    {0x0626,0x0000},{0x0626,0x0000},{0x0627,0x0000},
    {0x0627,0x0000},{0x0628,0x0000},{0x0628,0x0000},
    {0x0628,0x0000},{0x0628,0x0000},{0x0629,0x0000},
    {0x0629,0x0000},{0x062a,0x0000},{0x062a,0x0000},
    {0x062a,0x0000},{0x062a,0x0000},{0x062b,0x0000},
    {0x062b,0x0000},{0x062b,0x0000},{0x062b,0x0000},
    {0x062c,0x0000},{0x062c,0x0000},{0x062c,0x0000},
    {0x062c,0x0000},{0x062d,0x0000},{0x062d,0x0000},
    {0x062d,0x0000},{0x062d,0x0000},{0x062e,0x0000},
    {0x062e,0x0000},{0x062e,0x0000},{0x062e,0x0000},
    {0x062f,0x0000},{0x062f,0x0000},{0x0630,0x0000},
    {0x0630,0x0000},{0x0631,0x0000},{0x0631,0x0000},
    {0x0632,0x0000},{0x0632,0x0000},{0x0633,0x0000},
    {0x0633,0x0000},{0x0633,0x0000},{0x0633,0x0000},
    {0x0634,0x0000},{0x0634,0x0000},{0x0634,0x0000},
    {0x0634,0x0000},{0x0635,0x0000},{0x0635,0x0000},
    {0x0635,0x0000},{0x0635,0x0000},{0x0636,0x0000},
    {0x0636,0x0000},{0x0636,0x0000},{0x0636,0x0000},
    {0x0637,0x0000},{0x0637,0x0000},{0x0637,0x0000},
    {0x0637,0x0000},{0x0638,0x0000},{0x0638,0x0000},
    {0x0638,0x0000},{0x0638,0x0000},{0x0639,0x0000},
    {0x0639,0x0000},{0x0639,0x0000},{0x0639,0x0000},
    {0x063a,0x0000},{0x063a,0x0000},{0x063a,0x0000},
    {0x063a,0x0000},{0x0641,0x0000},{0x0641,0x0000},
    {0x0641,0x0000},{0x0641,0x0000},{0x0642,0x0000},
    {0x0642,0x0000},{0x0642,0x0000},{0x0642,0x0000},
    {0x0643,0x0000},{0x0643,0x0000},{0x0643,0x0000},
    {0x0643,0x0000},{0x0644,0x0000},{0x0644,0x0000},
    {0x0644,0x0000},{0x0644,0x0000},{0x0645,0x0000},
    {0x0645,0x0000},{0x0645,0x0000},{0x0645,0x0000},
    {0x0646,0x0000},{0x0646,0x0000},{0x0646,0x0000},
    {0x0646,0x0000},{0x0647,0x0000},{0x0647,0x0000},
    {0x0647,0x0000},{0x0647,0x0000},{0x0648,0x0000},
    {0x0648,0x0000},{0x0649,0x0000},{0x0649,0x0000},
    {0x064a,0x0000},{0x064a,0x0000},{0x064a,0x0000},
    {0x064a,0x0000},{0x0644,0x0622},{0x0644,0x0622},
    {0x0644,0x0623},{0x0644,0x0623},{0x0644,0x0625},
    {0x0644,0x0625},{0x0644,0x0627},{0x0644,0x0627},
    {0x0000,0x0000},{0x0000,0x0000},{0x0000,0x0000}
};

//============ Begin Arabic Basic to Presentation Form B Code ============
// Note: the following code are moved from gfx/src/windows/nsRenderingContextWin.cpp
static PRUint8 gArabicMap1[] = {
            0x81, 0x83, 0x85, 0x87, 0x89, 0x8D, // 0622-0627
0x8F, 0x93, 0x95, 0x99, 0x9D, 0xA1, 0xA5, 0xA9, // 0628-062F
0xAB, 0xAD, 0xAF, 0xB1, 0xB5, 0xB9, 0xBD, 0xC1, // 0630-0637
0xC5, 0xC9, 0xCD                                // 0638-063A
};

static PRUint8 gArabicMap2[] = {
      0xD1, 0xD5, 0xD9, 0xDD, 0xE1, 0xE5, 0xE9, // 0641-0647
0xED, 0xEF, 0xF1                                // 0648-064A
};

#define PresentationFormB(c, form)                           \
  (((0x0622<=(c)) && ((c)<=0x063A)) ?                        \
    (0xFE00|(gArabicMap1[(c)-0x0622] + (form))) :            \
     (((0x0641<=(c)) && ((c)<=0x064A)) ?                     \
      (0xFE00|(gArabicMap2[(c)-0x0641] + (form))) : (c)))

enum {
   eIsolated,  // or Char N
   eFinal,     // or Char R
   eInitial,   // or Char L
   eMedial     // or Char M
} eArabicForm;
enum {
   eTr = 0, // Transparent
   eRJ = 1, // Right-Joining
   eLJ = 2, // Left-Joining
   eDJ = 3, // Dual-Joining
   eNJ  = 4,// Non-Joining
   eJC = 7, // Joining Causing
   eRightJCMask = 2, // bit of Right-Join Causing 
   eLeftJCMask = 1   // bit of Left-Join Causing 
} eArabicJoiningClass;

#define RightJCClass(j) (eRightJCMask&(j))
#define LeftJCClass(j)  (eLeftJCMask&(j))

#define DecideForm(jl,j,jr)                                 \
  (((eRJ == (j)) && RightJCClass(jr)) ? eFinal              \
                                      :                     \
   ((eDJ == (j)) ?                                          \
    ((RightJCClass(jr)) ?                                   \
     (((LeftJCClass(jl)) ? eMedial                          \
                         : eFinal))                         \
                        :                                   \
     (((LeftJCClass(jl)) ? eInitial                         \
                         : eIsolated))                      \
    )                     : eIsolated))                     \


static PRInt8 gJoiningClass[] = {
          eRJ, eRJ, eRJ, eRJ, eDJ, eRJ, // 0620-0627
eDJ, eRJ, eDJ, eDJ, eDJ, eDJ, eDJ, eRJ, // 0628-062F
eRJ, eRJ, eRJ, eDJ, eDJ, eDJ, eDJ, eDJ, // 0630-0637
eDJ, eDJ, eDJ, eNJ, eNJ, eNJ, eNJ, eNJ, // 0638-063F
eJC, eDJ, eDJ, eDJ, eDJ, eDJ, eDJ, eDJ, // 0640-0647
eRJ, eRJ, eDJ, eTr, eTr, eTr, eTr, eTr, // 0648-064F
eTr, eTr, eTr                           // 0650-0652
};

#define GetJoiningClass(c)                   \
  (((0x0622 <= (c)) && ((c) <= 0x0652)) ?    \
       (gJoiningClass[(c) - 0x0622]) :       \
      ((0x200D == (c)) ? eJC : eTr))

static PRUint16 gArabicLigatureMap[] = 
{
0x82DF, // 0xFE82 0xFEDF -> 0xFEF5
0x82E0, // 0xFE82 0xFEE0 -> 0xFEF6
0x84DF, // 0xFE84 0xFEDF -> 0xFEF7
0x84E0, // 0xFE84 0xFEE0 -> 0xFEF8
0x88DF, // 0xFE88 0xFEDF -> 0xFEF9
0x88E0, // 0xFE88 0xFEE0 -> 0xFEFA
0x8EDF, // 0xFE8E 0xFEDF -> 0xFEFB
0x8EE0  // 0xFE8E 0xFEE0 -> 0xFEFC
};
#define CHAR_IS_HEBREW(c) ((0x0590 <= (c)) && ((c)<= 0x05FF))
#define CHAR_IS_ARABIC(c) ((0x0600 <= (c)) && ((c)<= 0x06FF))
// Note: The above code are moved from gfx/src/windows/nsRenderingContextWin.cpp

#define LRM_CHAR 0x200e
#define ARABIC_TO_HINDI_DIGIT_INCREMENT (START_HINDI_DIGITS - START_ARABIC_DIGITS)
#define NUM_TO_ARABIC(c) \
  ((((c)>=START_HINDI_DIGITS) && ((c)<=END_HINDI_DIGITS)) ? \
   ((c) - (PRUint16)ARABIC_TO_HINDI_DIGIT_INCREMENT) : \
   (c))
#define NUM_TO_HINDI(c) \
  ((((c)>=START_ARABIC_DIGITS) && ((c)<=END_ARABIC_DIGITS)) ? \
   ((c) + (PRUint16)ARABIC_TO_HINDI_DIGIT_INCREMENT): \
   (c))

nsBidiUtilsImp::nsBidiUtilsImp()
{
  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsBidiUtilsImp::~nsBidiUtilsImp()
{
  PR_AtomicDecrement(&g_InstanceCount);
}

NS_IMETHODIMP nsBidiUtilsImp::GetBidiCategory(PRUnichar aChar, eBidiCategory* oResult)
{
  *oResult = GetBidiCat(aChar);
  if (eBidiCat_CC == *oResult)
    *oResult = (eBidiCategory)(aChar & 0xFF); /* Control codes have special treatment to keep the tables smaller */
  return NS_OK;
}

NS_IMETHODIMP nsBidiUtilsImp::IsBidiCategory(PRUnichar aChar, eBidiCategory aBidiCategory, PRBool* oResult)
{
  eBidiCategory bCat = GetBidiCat(aChar);
  if (eBidiCat_CC == bCat)
    bCat = (eBidiCategory)(aChar & 0xFF);
  *oResult = (bCat == aBidiCategory);
  return NS_OK;
}

NS_IMETHODIMP nsBidiUtilsImp::IsBidiControl(PRUnichar aChar, PRBool* oResult)
{
  // This method is used when stripping Bidi control characters for
  // display, so it will return TRUE for LRM and RLM as well as the
  // characters with category eBidiCat_CC
  *oResult = (eBidiCat_CC == GetBidiCat(aChar) || ((aChar)&0xfffe)==LRM_CHAR);
  return NS_OK;
}

NS_IMETHODIMP nsBidiUtilsImp::GetCharType(PRUnichar aChar, nsCharType* oResult)
{
  eBidiCategory bCat = GetBidiCat(aChar);
  if (eBidiCat_CC != bCat) {
    NS_ASSERTION(bCat < (sizeof(ebc2ucd)/sizeof(nsCharType)), "size mismatch");
    if(bCat < (sizeof(ebc2ucd)/sizeof(nsCharType)))
      *oResult = ebc2ucd[bCat];
    else 
      *oResult = ebc2ucd[0]; // something is very wrong, but we need to return a value
  } else {
    NS_ASSERTION((aChar-0x202a) < (sizeof(cc2ucd)/sizeof(nsCharType)), "size mismatch");
    if((aChar-0x202a) < (sizeof(cc2ucd)/sizeof(nsCharType)))
      *oResult = cc2ucd[aChar - 0x202a];
    else 
      *oResult = ebc2ucd[0]; // something is very wrong, but we need to return a value
  }
  return NS_OK;
}

NS_IMETHODIMP nsBidiUtilsImp::SymmSwap(PRUnichar* aChar)
{
  *aChar = Mirrored(*aChar);
  return NS_OK;
}

NS_IMETHODIMP nsBidiUtilsImp::ArabicShaping(const PRUnichar* aString, PRUint32 aLen,
                                            PRUnichar* aBuf, PRUint32 *aBufLen)
{
  // Note: The real implementation is still under reviewing process 
  // will be check in soon. 
  // a stub routine which simply copy the data is now place here untill the
  // real code get check in.
  for(*aBufLen = 0;*aBufLen < aLen; *aBufLen++)
    aBuf[*aBufLen] = aString[*aBufLen];
  return NS_OK;
}


NS_IMETHODIMP nsBidiUtilsImp::HandleNumbers(PRUnichar* aBuffer, PRUint32 aSize, PRUint32 aNumFlag)
{
  PRUint32 i;
  // IBMBIDI_NUMERAL_REGULAR *
  // IBMBIDI_NUMERAL_HINDICONTEXT
  // IBMBIDI_NUMERAL_ARABIC
  // IBMBIDI_NUMERAL_HINDI

  switch (aNumFlag) {
    case IBMBIDI_NUMERAL_HINDI:
      for (i=0;i<aSize;i++)
        aBuffer[i] = NUM_TO_HINDI(aBuffer[i]);
      break;
    case IBMBIDI_NUMERAL_ARABIC:
      for (i=0;i<aSize;i++)
        aBuffer[i] = NUM_TO_ARABIC(aBuffer[i]);
      break;
    default : // IBMBIDI_NUMERAL_REGULAR, IBMBIDI_NUMERAL_HINDICONTEXT
      for (i=1;i<aSize;i++) {
        if (IS_ARABIC_CHAR(aBuffer[i-1])) 
          aBuffer[i] = NUM_TO_HINDI(aBuffer[i]);
        else 
          aBuffer[i] = NUM_TO_ARABIC(aBuffer[i]);
      }
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP nsBidiUtilsImp::HandleNumbers(const nsString aSrc, nsString & aDst)
{
  // Note: The real implementation is still under reviewing process 
  // will be check in soon. 
  // a stub routine which simply copy the data is now place here untill the
  // real code get check in.
  aDst = aSrc;
  return NS_OK;
}


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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Maha Abou El Rous <mahar@eg.ibm.com>
 *   Lina Kemmel <lkemmel@il.ibm.com>
 *   Simon Montagu <smontagu@netscape.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#ifndef nsBidiUtils_h__
#define nsBidiUtils_h__

#include "nsStringGlue.h"

   /**
    *  Read ftp://ftp.unicode.org/Public/UNIDATA/ReadMe-Latest.txt
    *  section BIDIRECTIONAL PROPERTIES
    *  for the detailed definition of the following categories
    *
    *  The values here must match the equivalents in %map in
    * mozilla/intl/unicharutil/tools/genbidicattable.pl
    */

typedef enum {
  eBidiCat_Undefined,
  eBidiCat_L,          /* Left-to-Right               */
  eBidiCat_R,          /* Right-to-Left               */
  eBidiCat_AL,         /* Right-to-Left Arabic        */
  eBidiCat_AN,         /* Arabic Number               */
  eBidiCat_EN,         /* European Number             */
  eBidiCat_ES,         /* European Number Separator   */
  eBidiCat_ET,         /* European Number Terminator  */
  eBidiCat_CS,         /* Common Number Separator     */
  eBidiCat_ON,         /* Other Neutrals              */
  eBidiCat_NSM,        /* Non-Spacing Mark            */
  eBidiCat_BN,         /* Boundary Neutral            */
  eBidiCat_B,          /* Paragraph Separator         */
  eBidiCat_S,          /* Segment Separator           */
  eBidiCat_WS,         /* Whitespace                  */
  eBidiCat_CC = 0xf,   /* Control Code                */
                       /* (internal use only - will never be outputed) */
  eBidiCat_LRE = 0x2a, /* Left-to-Right Embedding     */
  eBidiCat_RLE = 0x2b, /* Right-to-Left Embedding     */
  eBidiCat_PDF = 0x2c, /* Pop Directional Formatting  */
  eBidiCat_LRO = 0x2d, /* Left-to-Right Override      */
  eBidiCat_RLO = 0x2e  /* Right-to-Left Override      */
} eBidiCategory;

enum nsCharType   { 
  eCharType_LeftToRight              = 0, 
  eCharType_RightToLeft              = 1, 
  eCharType_EuropeanNumber           = 2,
  eCharType_EuropeanNumberSeparator  = 3,
  eCharType_EuropeanNumberTerminator = 4,
  eCharType_ArabicNumber             = 5,
  eCharType_CommonNumberSeparator    = 6,
  eCharType_BlockSeparator           = 7,
  eCharType_SegmentSeparator         = 8,
  eCharType_WhiteSpaceNeutral        = 9, 
  eCharType_OtherNeutral             = 10, 
  eCharType_LeftToRightEmbedding     = 11,
  eCharType_LeftToRightOverride      = 12,
  eCharType_RightToLeftArabic        = 13,
  eCharType_RightToLeftEmbedding     = 14,
  eCharType_RightToLeftOverride      = 15,
  eCharType_PopDirectionalFormat     = 16,
  eCharType_DirNonSpacingMark        = 17,
  eCharType_BoundaryNeutral          = 18,
  eCharType_CharTypeCount
};

/**
 * This specifies the language directional property of a character set.
 */
typedef enum nsCharType nsCharType;

/**
 * definitions of bidirection character types by category
 */

#define CHARTYPE_IS_RTL(val) ( ( (val) == eCharType_RightToLeft) || ( (val) == eCharType_RightToLeftArabic) )

#define CHARTYPE_IS_WEAK(val) ( ( (val) == eCharType_EuropeanNumberSeparator)    \
                           || ( (val) == eCharType_EuropeanNumberTerminator) \
                           || ( ( (val) > eCharType_ArabicNumber) && ( (val) != eCharType_RightToLeftArabic) ) )

  /**
   * Inspects a Unichar, converting numbers to Arabic or Hindi forms and returning them
   * @param aChar is the character
   * @param aPrevCharArabic is true if the previous character in the string is an Arabic char
   * @param aNumFlag specifies the conversion to perform:
   *        IBMBIDI_NUMERAL_NOMINAL:      don't do any conversion
   *        IBMBIDI_NUMERAL_HINDI:        convert to Hindi forms (Unicode 0660-0669)
   *        IBMBIDI_NUMERAL_ARABIC:       convert to Arabic forms (Unicode 0030-0039)
   *        IBMBIDI_NUMERAL_HINDICONTEXT: convert numbers in Arabic text to Hindi, otherwise to Arabic
   * @return the converted Unichar
   */
  PRUnichar HandleNumberInChar(PRUnichar aChar, bool aPrevCharArabic, PRUint32 aNumFlag);

  /**
   * Scan a Unichar string, converting numbers to Arabic or Hindi forms in place
   * @param aBuffer is the string
   * @param aSize is the size of aBuffer
   * @param aNumFlag specifies the conversion to perform:
   *        IBMBIDI_NUMERAL_NOMINAL:      don't do any conversion
   *        IBMBIDI_NUMERAL_HINDI:        convert to Hindi forms (Unicode 0660-0669)
   *        IBMBIDI_NUMERAL_ARABIC:       convert to Arabic forms (Unicode 0030-0039)
   *        IBMBIDI_NUMERAL_HINDICONTEXT: convert numbers in Arabic text to Hindi, otherwise to Arabic
   */
  nsresult HandleNumbers(PRUnichar* aBuffer, PRUint32 aSize, PRUint32  aNumFlag);

  /**
   * Give a UTF-32 codepoint, return a nsCharType (compatible with ICU)
   */
  nsCharType GetCharType(PRUint32 aChar);

  /**
   * Give a UTF-32 codepoint
   * return PR_TRUE if the codepoint is a Bidi control character (LRE, RLE, PDF, LRO, RLO, LRM, RLM)
   * return PR_FALSE, otherwise
   */
  bool IsBidiControl(PRUint32 aChar);

  /**
   * Give an nsString.
   * @return PR_TRUE if the string contains right-to-left characters
   */
  bool HasRTLChars(const nsAString& aString);

// --------------------------------------------------
// IBMBIDI 
// --------------------------------------------------
//
// These values are shared with Preferences dialog
//  ------------------
//  If Pref values are to be changed
//  in the XUL file of Prefs. the values
//  Must be changed here too..
//  ------------------
//
#define IBMBIDI_TEXTDIRECTION_STR       "bidi.direction"
#define IBMBIDI_TEXTTYPE_STR            "bidi.texttype"
#define IBMBIDI_NUMERAL_STR             "bidi.numeral"
#define IBMBIDI_SUPPORTMODE_STR         "bidi.support"

#define IBMBIDI_TEXTDIRECTION       1
#define IBMBIDI_TEXTTYPE            2
#define IBMBIDI_NUMERAL             4
#define IBMBIDI_SUPPORTMODE         5

//  ------------------
//  Text Direction
//  ------------------
//  bidi.direction
#define IBMBIDI_TEXTDIRECTION_LTR     1 //  1 = directionLTRBidi *
#define IBMBIDI_TEXTDIRECTION_RTL     2 //  2 = directionRTLBidi
//  ------------------
//  Text Type
//  ------------------
//  bidi.texttype
#define IBMBIDI_TEXTTYPE_CHARSET      1 //  1 = charsettexttypeBidi *
#define IBMBIDI_TEXTTYPE_LOGICAL      2 //  2 = logicaltexttypeBidi
#define IBMBIDI_TEXTTYPE_VISUAL       3 //  3 = visualtexttypeBidi
//  ------------------
//  Numeral Style
//  ------------------
//  bidi.numeral
#define IBMBIDI_NUMERAL_NOMINAL       0 //  0 = nominalnumeralBidi *
#define IBMBIDI_NUMERAL_REGULAR       1 //  1 = regularcontextnumeralBidi
#define IBMBIDI_NUMERAL_HINDICONTEXT  2 //  2 = hindicontextnumeralBidi
#define IBMBIDI_NUMERAL_ARABIC        3 //  3 = arabicnumeralBidi
#define IBMBIDI_NUMERAL_HINDI         4 //  4 = hindinumeralBidi
#define IBMBIDI_NUMERAL_PERSIANCONTEXT 5 // 5 = persiancontextnumeralBidi
#define IBMBIDI_NUMERAL_PERSIAN       6 //  6 = persiannumeralBidi
//  ------------------
//  Support Mode
//  ------------------
//  bidi.support
#define IBMBIDI_SUPPORTMODE_MOZILLA     1 //  1 = mozillaBidisupport *
#define IBMBIDI_SUPPORTMODE_OSBIDI      2 //  2 = OsBidisupport
#define IBMBIDI_SUPPORTMODE_DISABLE     3 //  3 = disableBidisupport

#define IBMBIDI_DEFAULT_BIDI_OPTIONS              \
        ((IBMBIDI_TEXTDIRECTION_LTR<<0)         | \
         (IBMBIDI_TEXTTYPE_CHARSET<<4)          | \
         (IBMBIDI_NUMERAL_NOMINAL<<8)          | \
         (IBMBIDI_SUPPORTMODE_MOZILLA<<12))

#define GET_BIDI_OPTION_DIRECTION(bo) (((bo)>>0) & 0x0000000F) /* 4 bits for DIRECTION */
#define GET_BIDI_OPTION_TEXTTYPE(bo) (((bo)>>4) & 0x0000000F) /* 4 bits for TEXTTYPE */
#define GET_BIDI_OPTION_NUMERAL(bo) (((bo)>>8) & 0x0000000F) /* 4 bits for NUMERAL */
#define GET_BIDI_OPTION_SUPPORT(bo) (((bo)>>12) & 0x0000000F) /* 4 bits for SUPPORT */

#define SET_BIDI_OPTION_DIRECTION(bo, dir) {(bo)=((bo) & 0xFFFFFFF0)|(((dir)& 0x0000000F)<<0);}
#define SET_BIDI_OPTION_TEXTTYPE(bo, tt) {(bo)=((bo) & 0xFFFFFF0F)|(((tt)& 0x0000000F)<<4);}
#define SET_BIDI_OPTION_NUMERAL(bo, num) {(bo)=((bo) & 0xFFFFF0FF)|(((num)& 0x0000000F)<<8);}
#define SET_BIDI_OPTION_SUPPORT(bo, sup) {(bo)=((bo) & 0xFFFF0FFF)|(((sup)& 0x0000000F)<<12);}

/* Constants related to the position of numerics in the codepage */
#define START_HINDI_DIGITS              0x0660
#define END_HINDI_DIGITS                0x0669
#define START_ARABIC_DIGITS             0x0030
#define END_ARABIC_DIGITS               0x0039
#define START_FARSI_DIGITS              0x06f0
#define END_FARSI_DIGITS                0x06f9
#define IS_HINDI_DIGIT(u)   ( ( (u) >= START_HINDI_DIGITS )  && ( (u) <= END_HINDI_DIGITS ) )
#define IS_ARABIC_DIGIT(u)  ( ( (u) >= START_ARABIC_DIGITS ) && ( (u) <= END_ARABIC_DIGITS ) )
#define IS_FARSI_DIGIT(u)  ( ( (u) >= START_FARSI_DIGITS ) && ( (u) <= END_FARSI_DIGITS ) )
/**
 * Arabic numeric separator and numeric formatting characters:
 *  U+0600;ARABIC NUMBER SIGN
 *  U+0601;ARABIC SIGN SANAH
 *  U+0602;ARABIC FOOTNOTE MARKER
 *  U+0603;ARABIC SIGN SAFHA
 *  U+066A;ARABIC PERCENT SIGN
 *  U+066B;ARABIC DECIMAL SEPARATOR
 *  U+066C;ARABIC THOUSANDS SEPARATOR
 *  U+06DD;ARABIC END OF AYAH
 */
#define IS_ARABIC_SEPARATOR(u) ( ( (u) == 0x0600 ) || ( (u) == 0x0601 ) || ( (u) == 0x0602 ) || ( (u) == 0x0603 ) || ( (u) == 0x066A ) || ( (u) == 0x066B ) || ( (u) == 0x066C ) || ( (u) == 0x06DD ) )

#define IS_BIDI_DIACRITIC(u) ( \
  ( (u) >= 0x0591 && (u) <= 0x05A1) || ( (u) >= 0x05A3 && (u) <= 0x05B9) \
    || ( (u) >= 0x05BB && (u) <= 0x05BD) || ( (u) == 0x05BF) || ( (u) == 0x05C1) \
    || ( (u) == 0x05C2) || ( (u) == 0x05C4) \
    || ( (u) >= 0x064B && (u) <= 0x0652) || ( (u) == 0x0670) \
    || ( (u) >= 0x06D7 && (u) <= 0x06E4) || ( (u) == 0x06E7) || ( (u) == 0x06E8) \
    || ( (u) >= 0x06EA && (u) <= 0x06ED) )

#define IS_HEBREW_CHAR(c) (((0x0590 <= (c)) && ((c)<= 0x05FF)) || (((c) >= 0xfb1d) && ((c) <= 0xfb4f)))
#define IS_ARABIC_CHAR(c) ((0x0600 <= (c)) && ((c)<= 0x06FF))
#define IS_ARABIC_ALPHABETIC(c) (IS_ARABIC_CHAR(c) && \
                                !(IS_HINDI_DIGIT(c) || IS_FARSI_DIGIT(c) || IS_ARABIC_SEPARATOR(c)))
#define IS_BIDI_CONTROL_CHAR(c) (((0x202a <= (c)) && ((c)<= 0x202e)) \
                                || ((c) == 0x200e) || ((c) == 0x200f))

/**
 * The codepoint ranges in the following macros are based on the blocks
 *  allocated, or planned to be allocated, to right-to-left characters in the
 *  BMP (Basic Multilingual Plane) and SMP (Supplementary Multilingual Plane)
 *  according to
 *  http://unicode.org/Public/UNIDATA/extracted/DerivedBidiClass.txt and
 *  http://www.unicode.org/roadmaps/
 */

#define IS_IN_BMP_RTL_BLOCK(c) ((0x590 <= (c)) && ((c) <= 0x8ff))
#define IS_RTL_PRESENTATION_FORM(c) (((0xfb1d <= (c)) && ((c) <= 0xfdff)) || \
                                     ((0xfe70 <= (c)) && ((c) <= 0xfefc)))
#define IS_IN_SMP_RTL_BLOCK(c) ((0x10800 <= (c)) && ((c) <= 0x10fff))
#define UCS2_CHAR_IS_BIDI(c) ((IS_IN_BMP_RTL_BLOCK(c)) || \
                              (IS_RTL_PRESENTATION_FORM(c)))
#define UTF32_CHAR_IS_BIDI(c)  ((IS_IN_BMP_RTL_BLOCK(c)) || \
                               (IS_RTL_PRESENTATION_FORM(c)) || \
                               (IS_IN_SMP_RTL_BLOCK(c)))
#endif  /* nsBidiUtils_h__ */

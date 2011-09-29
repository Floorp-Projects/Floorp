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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsIUGenDetailCategory_h__
#define nsIUGenDetailCategory_h__


#include "nsISupports.h"
#include "nscore.h"

// {E86B3372-BF89-11d2-B3AF-00805F8A6670}
#define NS_IUGENDETAILCATEGORY_IID \
{ 0xe86b3372, 0xbf89, 0x11d2, \
    { 0xb3, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } };



class nsIUGenDetailCategory : public nsISupports {

public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IUGENDETAILCATEGORY_IID)

   /**
    *  Read ftp://ftp.unicode.org/Public/UNIDATA/ReadMe-Latest.txt
    *  section GENERAL CATEGORY
    *  for the detail defintation of the following categories
    */
   typedef enum {
              
     kUGDC_M  = (kUGenCategory_Mark << 4) | 0, // Mark- return only when kUGDC_DetailMark bit is clear
     kUGDC_Mn = (kUGenCategory_Mark << 4) | 1, // Mark, Non-Spacing - return only when kUGDC_DetailMark bit is set
     kUGDC_Mc = (kUGenCategory_Mark << 4) | 2, // Mark, Spacing Combining - return only when kUGDC_DetailMark bit is set
     kUGDC_Me = (kUGenCategory_Mark << 4) | 3, // Mark, Enclosing - return only when kUGDC_DetailMark bit is set

     kUGDC_N  = (kUGenCategory_Number << 4) | 0, // Number- return only when kUGDC_DetailNumber bit is clear
     kUGDC_Nd = (kUGenCategory_Number << 4) | 1, // Number, Decimal Digit - return only when kUGDC_DetailNumber bit is set
     kUGDC_Nl = (kUGenCategory_Number << 4) | 2, // Number, Letter - return only when kUGDC_DetailNumber bit is set
     kUGDC_No = (kUGenCategory_Number << 4) | 3, // Number, Other - return only when kUGDC_DetailNumber bit is set

     kUGDC_Z  = (kUGenCategory_Separator << 4) | 0, // Separator- return only when kUGDC_DetailSeparator bit is clear
     kUGDC_Zs = (kUGenCategory_Separator << 4) | 1, // Separator, Space - return only when kUGDC_DetailSeparator bit is set
     kUGDC_Zl = (kUGenCategory_Separator << 4) | 2, // Separator, Line - return only when kUGDC_DetailSeparator bit is set
     kUGDC_Zp = (kUGenCategory_Separator << 4) | 3, // Separator, Paragraph - return only when kUGDC_DetailSeparator bit is set

     kUGDC_C  = (kUGenCategory_Other << 4) | 0, // Other- return only when kUGDC_DetailOther bit is clear
     kUGDC_Cc = (kUGenCategory_Other << 4) | 1, // Other, Control - return only when kUGDC_DetailOther bit is set
     kUGDC_Cf = (kUGenCategory_Other << 4) | 2, // Other, Format - return only when kUGDC_DetailOther bit is set
     kUGDC_Cs = (kUGenCategory_Other << 4) | 3, // Other, Surrogate - return only when kUGDC_DetailOther bit is set
     kUGDC_Co = (kUGenCategory_Other << 4) | 4, // Other, Private Use - return only when kUGDC_DetailOther bit is set
     kUGDC_Cn = (kUGenCategory_Other << 4) | 5, // Other, Not Assigned - return only when kUGDC_DetailOther bit is set

     kUGDC_L  = (kUGenCategory_Letter << 4) | 0, // Letter- return only when kUGDC_DetailLetter bit is clear
     kUGDC_Lu = (kUGenCategory_Letter << 4) | 1, // Letter, Uppercase - return only when kUGDC_DetailLetter bit is set
     kUGDC_Ll = (kUGenCategory_Letter << 4) | 2, // Letter, Lowercase - return only when kUGDC_DetailLetter bit is set
     kUGDC_Lt = (kUGenCategory_Letter << 4) | 3, // Letter, Titlecase - return only when kUGDC_DetailLetter bit is set
     kUGDC_Lm = (kUGenCategory_Letter << 4) | 4, // Letter, Modifier - return only when kUGDC_DetailLetter bit is set
     kUGDC_Lo = (kUGenCategory_Letter << 4) | 5, // Letter, Other - return only when kUGDC_DetailLetter bit is set

     kUGDC_P  = (kUGenCategory_Punctuation << 4) | 0, // Punctuation- return only when kUGDC_DetailPunctuation bit is clear
     kUGDC_Pc = (kUGenCategory_Punctuation << 4) | 1, // Punctuation, Connector - return only when kUGDC_DetailPunctuation bit is set
     kUGDC_Pd = (kUGenCategory_Punctuation << 4) | 2, // Punctuation, Dash - return only when kUGDC_DetailPunctuation bit is set
     kUGDC_Ps = (kUGenCategory_Punctuation << 4) | 3, // Punctuation, Open - return only when kUGDC_DetailPunctuation bit is set
     kUGDC_Pe = (kUGenCategory_Punctuation << 4) | 4, // Punctuation, Close - return only when kUGDC_DetailPunctuation bit is set
     kUGDC_Pi = (kUGenCategory_Punctuation << 4) | 5, // Punctuation, Initial quote - return only when kUGDC_DetailPunctuation bit is set
     kUGDC_Pf = (kUGenCategory_Punctuation << 4) | 6, // Punctuation, Final quote - return only when kUGDC_DetailPunctuation bit is set
     kUGDC_Po = (kUGenCategory_Punctuation << 4) | 7, // Punctuation, Other - return only when kUGDC_DetailPunctuation bit is set

     kUGDC_S  = (kUGenCategory_Symbol << 4) | 0, // Symbol- return only when kUGDC_DetailSymbol bit is clear
     kUGDC_Sm = (kUGenCategory_Symbol << 4) | 1, // Symbol, Math - return only when kUGDC_DetailSymbol bit is set
     kUGDC_Sc = (kUGenCategory_Symbol << 4) | 2, // Symbol, Currency - return only when kUGDC_DetailSymbol bit is set
     kUGDC_Sk = (kUGenCategory_Symbol << 4) | 3, // Symbol, Modifier - return only when kUGDC_DetailSymbol bit is set
     kUGDC_So = (kUGenCategory_Symbol << 4) | 4, // Symbol, Other - return only when kUGDC_DetailSymbol bit is set
   } nsUGDC;

   /**
    * Sometimes, we want the Get method only return detail in certain 
    * area. It help to ignore unnecessary detail to improve performance
    **/
   typedef enum {
     kUGDC_DetailNone          = 0,
     kUGDC_DetailMark          = ( 1 << 0),
     kUGDC_DetailNumber        = ( 1 << 1),
     kUGDC_DetailSeparator     = ( 1 << 2),
     kUGDC_DetailOther         = ( 1 << 3),
     kUGDC_DetailLetter        = ( 1 << 4),
     kUGDC_DetailPunctuation   = ( 1 << 5),
     kUGDC_DetailSymbol        = ( 1 << 6),
     kUGDC_DetailNormative     = ( kUGDC_DetailMark | 
                                   kUGDC_DetailNumber | 
                                   kUGDC_DetailSeparator | 
                                   kUGDC_DetailOther  ),
     kUGDC_DetailInformative   = ( kUGDC_DetailLetter | 
                                   kUGDC_DetailPunctuation | 
                                   kUGDC_DetailSymbol);
     kUGDC_DetailAll           = ( kUGDC_DetailNormative | kUGDC_Informative );
   } nsUGDC_Detail;

   /**
    * Give a Unichar, return a nsUGDC
    */
   NS_IMETHOD Get( PRUnichar aChar, nsUGDC_Detail aDetailSpec, nsUGDC* oResult) = 0 ;
    
   /**
    * Give a Unichar, and a nsUGenDetailCategory, 
    * return PR_TRUE if the Unichar is in that category, 
    * return PR_FALSE, otherwise
    */
   NS_IMETHOD Is( PRUnichar aChar, nsUGDC aDetailCategory, bool* oResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUGenDetailCategory,
                              NS_IUGENDETAILCATEGORY_IID)

#endif  /* nsIUGenDetailCategory_h__ */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIUGenCategory_h__
#define nsIUGenCategory_h__


#include "nsISupports.h"
#include "nscore.h"

// {E86B3371-BF89-11d2-B3AF-00805F8A6670}
#define NS_IUGENCATEGORY_IID \
{ 0xe86b3371, 0xbf89, 0x11d2, \
    { 0xb3, 0xaf, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


class nsIUGenCategory : public nsISupports {

public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IUGENCATEGORY_IID)

   /**
    *  Read ftp://ftp.unicode.org/Public/UNIDATA/ReadMe-Latest.txt
    *  section GENERAL CATEGORY
    *  for the detail defintation of the following categories
    */
   typedef enum {
     kUGenCategory_Mark         = 1, // Mn, Mc, and Me
     kUGenCategory_Number       = 2, // Nd, Nl, and No 
     kUGenCategory_Separator    = 3, // Zs, Zl, and Zp
     kUGenCategory_Other        = 4, // Cc, Cf, Cs, Co, and Cn
     kUGenCategory_Letter       = 5, // Lu, Ll, Lt, Lm, and Lo
     kUGenCategory_Punctuation  = 6, // Pc, Pd, Ps, Pe, Pi, Pf, and Po
     kUGenCategory_Symbol       = 7  // Sm, Sc, Sk, and So
   } nsUGenCategory;

   /**
    * Give a Unichar, return a nsUGenCategory
    */
   NS_IMETHOD Get( PRUnichar aChar, nsUGenCategory* oResult) = 0 ;
    
   /**
    * Give a Unichar, and a nsUGenCategory, 
    * return PR_TRUE if the Unichar is in that category, 
    * return PR_FALSE, otherwise
    */
   NS_IMETHOD Is( PRUnichar aChar, nsUGenCategory aCategory, PRBool* oResult) = 0;
};

#endif  /* nsIUGenCategory_h__ */

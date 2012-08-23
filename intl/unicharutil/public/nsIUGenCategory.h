/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIUGenCategory_h__
#define nsIUGenCategory_h__


#include "nsISupports.h"
#include "nscore.h"

// {671fea05-fcee-4b1c-82a3-6eb03eda8ddc}
#define NS_IUGENCATEGORY_IID \
{ 0x671fea05, 0xfcee, 0x4b1c, \
    { 0x82, 0xa3, 0x6e, 0xb0, 0x3e, 0xda, 0x8d, 0xdc } }


class nsIUGenCategory : public nsISupports {

public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IUGENCATEGORY_IID)

   /**
    *  Read http://unicode.org/reports/tr44/#General_Category_Values
    *  for the detailed definition of the following categories
    */
   typedef enum {
     kUndefined    = 0,
     kMark         = 1, // Mn, Mc, and Me
     kNumber       = 2, // Nd, Nl, and No 
     kSeparator    = 3, // Zs, Zl, and Zp
     kOther        = 4, // Cc, Cf, Cs, Co, and Cn
     kLetter       = 5, // Lu, Ll, Lt, Lm, and Lo
     kPunctuation  = 6, // Pc, Pd, Ps, Pe, Pi, Pf, and Po
     kSymbol       = 7  // Sm, Sc, Sk, and So
   } nsUGenCategory;

   /**
    * Give a Unichar, return a nsUGenCategory
    */
   virtual nsUGenCategory Get(uint32_t aChar) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIUGenCategory, NS_IUGENCATEGORY_IID)

#endif  /* nsIUGenCategory_h__ */

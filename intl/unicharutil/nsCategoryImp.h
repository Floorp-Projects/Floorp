/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsCategoryImp_h__
#define nsCategoryImp_h__

#include "nsIUGenCategory.h"

class nsCategoryImp : public nsIUGenCategory {
   NS_DECL_THREADSAFE_ISUPPORTS
   
public:
   static nsCategoryImp* GetInstance();
    
   /**
    * Give a Unichar, return a nsUGenCategory
    */
   virtual nsUGenCategory Get(uint32_t aChar) override;
};

#endif  /* nsCategoryImp_h__ */

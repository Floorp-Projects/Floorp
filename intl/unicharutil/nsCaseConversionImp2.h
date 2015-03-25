/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCaseConversionImp2_h__
#define nsCaseConversionImp2_h__

#include "nscore.h"
#include "nsISupports.h"

#include "nsICaseConversion.h"

class nsCaseConversionImp2 : public nsICaseConversion { 
  NS_DECL_THREADSAFE_ISUPPORTS 

public:
  virtual ~nsCaseConversionImp2() { }

  static nsCaseConversionImp2* GetInstance();

  NS_IMETHOD ToUpper(char16_t aChar, char16_t* aReturn) override;

  NS_IMETHOD ToLower(char16_t aChar, char16_t* aReturn) override;

  NS_IMETHOD ToTitle(char16_t aChar, char16_t* aReturn) override;

  NS_IMETHOD ToUpper(const char16_t* anArray, char16_t* aReturn, uint32_t aLen) override;

  NS_IMETHOD ToLower(const char16_t* anArray, char16_t* aReturn, uint32_t aLen) override;

  NS_IMETHOD CaseInsensitiveCompare(const char16_t* aLeft, const char16_t* aRight, uint32_t aLength, int32_t *aResult) override;
};

#endif

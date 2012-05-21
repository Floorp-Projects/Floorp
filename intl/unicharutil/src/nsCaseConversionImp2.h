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
  NS_DECL_ISUPPORTS 

public:
  virtual ~nsCaseConversionImp2() { }

  static nsCaseConversionImp2* GetInstance();

  NS_IMETHOD ToUpper(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToLower(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToTitle(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToUpper(const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen);

  NS_IMETHOD ToLower(const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen);

  NS_IMETHOD CaseInsensitiveCompare(const PRUnichar* aLeft, const PRUnichar* aRight, PRUint32 aLength, PRInt32 *aResult);
};

#endif

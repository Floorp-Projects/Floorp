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

#ifndef nsCaseConversionImp2_h__
#define nsCaseConversionImp2_h__

#include "nsCom.h"
#include "nsISupports.h"

#include "nsICaseConversion.h"

nsresult NS_NewCaseConversion(nsISupports** oResult);

class nsCaseConversionImp2 : public nsICaseConversion { 
  NS_DECL_ISUPPORTS 

public:
  nsCaseConversionImp2();
  virtual ~nsCaseConversionImp2();


  NS_IMETHOD ToUpper(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToLower(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToTitle(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToUpper(const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen);

  NS_IMETHOD ToLower(const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen);

  NS_IMETHOD ToTitle(const PRUnichar* anArray, PRUnichar* aReturn, 
                     PRUint32 aLen, PRBool aStartInWordBoundary = PR_TRUE);
   
  NS_IMETHOD ToUpper(const PRUnichar* anIn, PRUint32 aLen, nsString& anOut, const PRUnichar* aLocale=nsnull) ;
  NS_IMETHOD ToLower(const PRUnichar* anIn, PRUint32 aLen, nsString& anOut, const PRUnichar* aLocale=nsnull );
  NS_IMETHOD ToTitle(const PRUnichar* anIn, PRUint32 aLen, nsString& anOut, const PRUnichar* aLocale=nsnull, PRBool aStartInWordBoundary=PR_TRUE) ;
private:
  static nsrefcnt gInit;
};

#endif

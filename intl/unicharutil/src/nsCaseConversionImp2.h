/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsCaseConversionImp2_h__
#define nsCaseConversionImp2_h__

#include "nsCom.h"
#include "nsISupports.h"

#include "nsICaseConversion.h"

class nsCaseConversionImp2 : public nsICaseConversion { 
  NS_DECL_ISUPPORTS 


  nsCaseConversionImp2() {
    if(! gInit)
	Init();	
    NS_INIT_REFCNT();
  }

  NS_IMETHOD ToUpper(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToLower(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToTitle(PRUnichar aChar, PRUnichar* aReturn);

  NS_IMETHOD ToUpper(const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen);

  NS_IMETHOD ToLower(const PRUnichar* anArray, PRUnichar* aReturn, PRUint32 aLen);

  NS_IMETHOD ToTitle(const PRUnichar* anArray, PRUnichar* aReturn, 
                     PRUint32 aLen, PRBool aStartInWordBoundary = PR_TRUE);
   
  void Init();
private:
  static PRBool gInit;
};

#endif


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
#ifndef nsCollation_h__
#define nsCollation_h__


#include "nsICollation.h"
#include "nsICaseConversion.h"


// Create a collation interface for an input locale.
// 
class nsCollationFactory: public nsICollationFactory {

public: 
  NS_DECL_ISUPPORTS 

  NS_IMETHOD CreateCollation(const nsString& locale, nsICollation** instancePtr);

  nsCollationFactory() {NS_INIT_REFCNT();};
};


struct NS_EXPORT nsCollation {

public: 

  nsCollation();
  
  ~nsCollation();

  // compare two strings
  // result is same as strcmp
  nsresult CompareString(nsICollation *inst, const nsCollationStrength strength, 
                         const nsString& string1, const nsString& string2, PRInt32* result);

  // compare two sort keys
  // length is character length not byte length, result is same as strcmp
  PRInt32 CompareSortKey(const PRUint8* key1, const PRUint32 len1, 
                         const PRUint8* key2, const PRUint32 len2);

  // normalize string before collation key generation
  nsresult NormalizeString(nsString stringInOut);

protected:
  nsICaseConversion*  mCaseConversion;
};

#endif  /* nsCollation_h__ */

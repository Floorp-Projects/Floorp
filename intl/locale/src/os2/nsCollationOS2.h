/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _nscollationos2_h_
#define _nscollationos2_h_


#include "nsICollation.h"
#include "nsCollation.h"  // static library
#include "nsCRT.h"



class nsCollationOS2 : public nsICollation {

protected:
  nsCollation   *mCollation;
  nsString      mLocale;
  nsString      mSavedLocale;

public:
  NS_DECL_ISUPPORTS

  // compare two strings
  // result is same as strcmp
  NS_IMETHOD CompareString(const nsCollationStrength strength, 
                           const nsAString& string1, const nsAString& string2, PRInt32* result);

  // allocate sort key from input string
  // returns newly allocated key and its byte length
  NS_IMETHOD AllocateRawSortKey(const nsCollationStrength strength, 
                                const nsAString& stringIn, PRUint8** key, PRUint32 *outLen);

  // compare two sort keys
  // length is character length not byte length, result is same as strcmp
  NS_IMETHOD CompareRawSortKey(const PRUint8* key1, const PRUint32 len1, 
                               const PRUint8* key2, const PRUint32 len2, 
                               PRInt32* result) 
                               {*result = nsCRT::strcmp((PRUnichar *)key1, (PRUnichar *)key2); return NS_OK;}
   
  // init this interface to a specified locale (should only be called by collation factory)
  //
  NS_IMETHOD Initialize(nsILocale* locale);

  nsCollationOS2();

  virtual ~nsCollationOS2();
};

#endif /* nsCollationOS2_h__ */ 

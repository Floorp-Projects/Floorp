/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.  Portions created by IBM are
 * Copyright (C) 2000 IBM Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsBidiUtilsImp_h__
#define nsBidiUtilsImp_h__

#include "nsCom.h"
#include "nsISupports.h"
#include "nsIUBidiUtils.h"

class nsBidiUtilsImp : public nsIUBidiUtils {
   NS_DECL_ISUPPORTS
   
public:
   nsBidiUtilsImp();
   virtual ~nsBidiUtilsImp();


   /**
    * Give a Unichar, return an eBidiCategory
    */
   NS_IMETHOD GetBidiCategory(PRUnichar aChar, eBidiCategory* oResult);
    
   /**
    * Give a Unichar, and an eBidiCategory, 
    * return PR_TRUE if the Unichar is in that category, 
    * return PR_FALSE, otherwise
    */
   NS_IMETHOD IsBidiCategory(PRUnichar aChar, eBidiCategory aBidiCategory, PRBool* oResult);
   
   /**
    * Give a Unichar
    * return PR_TRUE if the Unichar is a Bidi control character (LRE, RLE, PDF, LRO, RLO, LRM, RLM)
    * return PR_FALSE, otherwise
    */
   NS_IMETHOD IsBidiControl(PRUnichar aChar, PRBool* oResult);

   /**
    * Give a Unichar, return a nsCharType (compatible with ICU)
    */
   NS_IMETHOD GetCharType(PRUnichar aChar, nsCharType* oResult);

   /**
    * Give a Unichar, return the symmetric equivalent
    */
   NS_IMETHOD SymmSwap(PRUnichar* aChar);


   NS_IMETHOD ArabicShaping(const PRUnichar* aString, PRUint32 aLen,
                            PRUnichar* aBuf, PRUint32 *aBufLen);

   NS_IMETHOD HandleNumbers(PRUnichar* aBuffer, PRUint32 aSize, PRUint32  aNumFlag);
   NS_IMETHOD HandleNumbers(const nsString aSrc, nsString & aDst );
};

#endif  /* nsBidiUtilsImp_h__ */


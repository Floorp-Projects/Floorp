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
#ifndef nsCategoryImp_h__
#define nsCategoryImp_h__

#include "nsCom.h"
#include "nsISupports.h"
#include "nsIUGenCategory.h"

class nsCategoryImp : public nsIUGenCategory {
   NS_DECL_ISUPPORTS
   
public: 
   nsCategoryImp();
   virtual ~nsCategoryImp();


   /**
    * Give a Unichar, return a nsUGenCategory
    */
   NS_IMETHOD Get( PRUnichar aChar, nsUGenCategory* oResult);
    
   /**
    * Give a Unichar, and a nsUGenCategory, 
    * return PR_TRUE if the Unichar is in that category, 
    * return PR_FALSE, otherwise
    */
   NS_IMETHOD Is( PRUnichar aChar, nsUGenCategory aCategory, PRBool* oResult);
};

#endif  /* nsCategoryImp_h__ */

/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCom.h"
#include "pratom.h"
#include "nsUUDll.h"
#include "nsISupports.h"
#include "nsCategoryImp.h"
#include "cattable.h"

NS_IMPL_ISUPPORTS1(nsCategoryImp, nsIUGenCategory);


nsCategoryImp::nsCategoryImp()
{
   NS_INIT_REFCNT();
}

nsCategoryImp::~nsCategoryImp()
{
}

nsresult nsCategoryImp::Get( PRUnichar aChar, nsUGenCategory* oResult)
{
   PRUint8 ret = GetCat(aChar);
   if( 0 == ret)
      *oResult = kUGenCategory_Other; // treat it as Cn - Other, Not Assigned
   else 
      *oResult = (nsUGenCategory)ret;
   return NS_OK;
}
    
nsresult nsCategoryImp::Is( PRUnichar aChar, nsUGenCategory aCategory, PRBool* oResult)

{
   nsUGenCategory cat ;
   PRUint8 ret = GetCat(aChar);
   if( 0 == ret)
      cat = kUGenCategory_Other; // treat it as Cn - Other, Not Assigned
   else 
      cat = (nsUGenCategory)ret;
   *oResult = (aCategory == cat );
   return NS_OK;
}

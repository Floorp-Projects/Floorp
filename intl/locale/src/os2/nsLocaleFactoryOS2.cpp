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
 *
 */

#include "nscore.h"
#include "nsLocaleFactoryOS2.h"

// ctor/dtor
nsLocaleFactoryOS2::nsLocaleFactoryOS2() : mSysLocale(nsnull),
                                           mAppLocale(nsnull)
{
   // don't need to init the ref-count because XP nsLocaleFactory does that.
}

nsLocaleFactoryOS2::~nsLocaleFactoryOS2()
{
   NS_IF_RELEASE(mSysLocale);
   NS_IF_RELEASE(mAppLocale);
}

// nsILocaleFactory
nsresult nsLocaleFactoryOS2::NewLocale( nsString  **aCatList,
                                        nsString  **aValList,
                                        PRUint8     aCount,
                                        nsILocale **aLocale)
{
   if( !aCatList || !aValList || !aLocale)
      return NS_ERROR_NULL_POINTER;

   *aLocale = nsnull;
   nsLocaleOS2 *aLoc = new nsLocaleOS2;
   if(nsnull == aLoc)
       return NS_ERROR_OUT_OF_MEMORY;

   nsresult rc = aLoc->Init( aCatList, aValList, aCount);

   if( NS_FAILED(rc))
      delete aLoc;
   else
   {
      NS_ADDREF(aLoc);
      *aLocale = aLoc;
   }

   return rc;
}

nsresult nsLocaleFactoryOS2::NewLocale( const nsString *aName,
                                        nsILocale     **aLocale)
{
   if( !aName || !aLocale)
      return NS_ERROR_NULL_POINTER;

   *aLocale = nsnull;
   nsLocaleOS2 *aLoc = new nsLocaleOS2;
   if(nsnull == aLoc)
       return NS_ERROR_OUT_OF_MEMORY;

   nsresult rc = aLoc->Init( *aName);

   if( NS_FAILED(rc))
      delete aLoc;
   else
   {
      NS_ADDREF(aLoc);
      *aLocale = aLoc;
   }

   return rc;
}

nsresult nsLocaleFactoryOS2::GetSystemLocale( nsILocale **aSysLocale)
{
   if( !aSysLocale)
      return NS_ERROR_NULL_POINTER;

   if( !mSysLocale)
      mSysLocale = new nsSystemLocale;

   NS_ADDREF(mSysLocale);

   return NS_OK;
}

nsresult nsLocaleFactoryOS2::GetApplicationLocale( nsILocale **aAppLocale)
{
   if( !aAppLocale)
      return NS_ERROR_NULL_POINTER;

   if( !mAppLocale)
      mAppLocale = new nsApplicationLocale;

   NS_ADDREF(mAppLocale);

   return NS_OK;
}

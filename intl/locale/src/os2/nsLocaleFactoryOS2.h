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

#ifndef _nslocalefactoryos2_h_
#define _nslocalefactoryos2_h_

#include "nsLocaleFactory.h"
#include "nsLocaleOS2.h" // for sys/app typedef (sorry!)

// Although we pretty-much do our own thing for locale on OS/2 due to the
// rich set of native functions, it is necessary to derive this class from
// the XP nsLocaleFactory to take advantage of the GetLocaleFromAcceptLanguage()
// method.  Doing this probably drags in a load of unused XP locale code :-(
//
class nsLocaleFactoryOS2 : public nsLocaleFactory
{
 public:
   nsLocaleFactoryOS2();
   virtual ~nsLocaleFactoryOS2();

   // nsILocaleFactory
   NS_IMETHOD NewLocale( nsString **aCatList, nsString **aValList,
                         PRUint8 aCount, nsILocale **aLocale);
   NS_IMETHOD NewLocale( const nsString *aLocaleName, nsILocale **aLocale);
   NS_IMETHOD GetSystemLocale( nsILocale **aSystemLocale);
   NS_IMETHOD GetApplicationLocale( nsILocale **aApplicationLocale);

 protected:
   nsSystemLocale      *mSysLocale;
   nsApplicationLocale *mAppLocale;
};

#endif

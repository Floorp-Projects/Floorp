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

#ifndef _nslocaleos2_h_
#define _nslocaleos2_h_

#include "nsILocale.h"
#include "nsILocaleOS2.h"

class nsLocaleOS2 : public nsILocale, public nsILocaleOS2
{
 public:
   nsLocaleOS2();
   virtual ~nsLocaleOS2();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsILocale
   NS_IMETHOD GetCategory( const nsString *aCat, nsString *aLocale);

   // nsILocaleOS2
   // Init a complex locale - categories should be magic nsLocale words
   NS_IMETHOD Init( nsString **aCatList,
                    nsString **aValList,
                    PRUint8    aLength);

   // Init a locale object from a xx-XX style name
   NS_IMETHOD Init( const nsString &aLocaleName);

   // Get the OS/2 locale object
   NS_IMETHOD GetLocaleObject( LocaleObject *aLocaleObject);

 protected:
   LocaleObject mLocaleObject;

   NS_IMETHOD Init( char *pszLocale);
};

class nsSystemLocale : public nsLocaleOS2
{
 public:
   nsSystemLocale();
};

// XXX for now, I guess
typedef nsSystemLocale nsApplicationLocale;

#endif

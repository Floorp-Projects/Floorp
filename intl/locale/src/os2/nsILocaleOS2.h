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

#ifndef _nsilocaleos2_h_
#define _nsilocaleos2_h_

#include "nsISupports.h"
#include "unidef.h"       // for LocaleObject

class nsString;

// XXX I made this IID up.  Get a legit one when we land the branch.

// {00932BE1-B65A-11d2-AF0B-aa60089FE59B}
#define NS_ILOCALEOS2_IID	                    \
{ 0x932be1, 0xb65a, 0x11d2,                    \
{ 0xaf, 0xb, 0xaa, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}

class nsILocaleOS2 : public nsISupports
{
 public:
   // Init a complex locale - categories should be magic nsLocale words
   NS_IMETHOD Init( nsString **aCatList,
                    nsString **aValList,
                    PRUint8    aLength) = 0;

   // Init a locale object from a xx-XX style name
   NS_IMETHOD Init( const nsString &aLocaleName) = 0;

   // Get the OS/2 locale object
   NS_IMETHOD GetLocaleObject( LocaleObject *aLocaleObject) = 0;
};

#endif

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsILocaleManager_h__
#define nsILocaleManager_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsILocale.h"


// {00932BE1-B65A-11d2-AF0B-0060089FE59B}
#define NS_ILOCALEFACTORY_IID						\
{	0x932be1, 0xb65a, 0x11d2,						\
{	0xaf, 0xb, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}


class nsILocaleFactory : public nsIFactory
{

public:

   NS_IMETHOD NewLocale(nsString** catagoryList,nsString**  
      valueList, PRUint8 count, nsILocale** locale) = 0;

   NS_IMETHOD NewLocale(const nsString* localeName, nsILocale** locale) = 0;

   NS_IMETHOD GetSystemLocale(nsILocale** systemLocale) = 0;

   NS_IMETHOD GetApplicationLocale(nsILocale** applicationLocale) = 0;

};

#endif /* nsILocaleManager_h__ */

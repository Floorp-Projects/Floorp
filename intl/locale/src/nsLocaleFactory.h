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

#ifndef nsLocaleManager_h__
#define nsLocaleManager_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#ifdef XP_PC
#include "nsIWin32Locale.h"
#endif

class nsLocaleFactory : public nsILocaleFactory
{
  NS_DECL_ISUPPORTS

private:

  nsString**	fCatagoryList;
  nsILocale*	fSystemLocale;
  nsILocale*	fApplicationLocale;
#ifdef XP_PC
  nsIWin32Locale*		fWin32LocaleInterface;
#endif

public:

   
	nsLocaleFactory(void);
	virtual ~nsLocaleFactory(void);
	
	NS_IMETHOD CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult);

	NS_IMETHOD LockFactory(PRBool aLock);

	NS_IMETHOD NewLocale(nsString** catagoryList,nsString** valueList,
		PRUint8 count, nsILocale** locale);

	NS_IMETHOD NewLocale(const nsString* localeName, nsILocale** locale);

	NS_IMETHOD GetSystemLocale(nsILocale** systemLocale);

	NS_IMETHOD GetApplicationLocale(nsILocale** applicationLocale);

};

#endif /* nsILocaleManager_h__ */

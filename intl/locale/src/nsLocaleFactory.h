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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsLocaleManager_h__
#define nsLocaleManager_h__

#include "nsString.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsILocale.h"
#include "nsILocaleFactory.h"
#ifdef XP_WIN
#include "nsIWin32Locale.h"
#elif defined(XP_OS2)
#include "nsIOS2Locale.h"
#elif defined(XP_UNIX) || defined(XP_BEOS)
#include "nsIPosixLocale.h"
#endif

class nsLocaleServiceFactory : public nsIFactory
{
public:
  nsLocaleServiceFactory();
  virtual ~nsLocaleServiceFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports* aOuter, REFNSIID aIID, void** aResult);
  NS_IMETHOD LockFactory(PRBool aLock);
};

class nsLocaleFactory : public nsILocaleFactory
{
  NS_DECL_ISUPPORTS

private:

  nsString**	fCategoryList;
  nsILocale*	fSystemLocale;
  nsILocale*	fApplicationLocale;
#ifdef XP_WIN
  nsIWin32Locale*   fWin32LocaleInterface;
#elif defined(XP_OS2)
  nsIOS2Locale*     fOS2LocaleInterface;
#elif defined(XP_UNIX) || defined(XP_BEOS)
  nsIPosixLocale*   fPosixLocaleInterface;
#endif

public:

   
	nsLocaleFactory(void);
	virtual ~nsLocaleFactory(void);
	
	NS_IMETHOD CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult);

	NS_IMETHOD LockFactory(PRBool aLock);

	NS_IMETHOD NewLocale(nsString** categoryList,nsString** valueList,
		PRUint8 count, nsILocale** locale);

	NS_IMETHOD NewLocale(const nsString* localeName, nsILocale** locale);

	NS_IMETHOD GetSystemLocale(nsILocale** systemLocale);

	NS_IMETHOD GetApplicationLocale(nsILocale** applicationLocale);

	NS_IMETHOD GetLocaleFromAcceptLanguage(const char* acceptLanguage, nsILocale** acceptLocale);

};

#endif /* nsILocaleManager_h__ */

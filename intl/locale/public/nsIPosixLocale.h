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
#ifndef nsIPosixLocale_h__
#define nsIPosixLocale_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"

// {21434951-EC71-11d2-9E89-0060089FE59B}
#define	NS_IPOSIXLOCALE_IID	  \
{ 0x21434951, 0xec71, 0x11d2,     \
{ 0x9e, 0x89, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}

#define MAX_LANGUAGE_CODE_LEN 3
#define MAX_COUNTRY_CODE_LEN  3
#define MAX_LOCALE_LEN        128
#define MAX_EXTRA_LEN         65

class nsIPosixLocale : public nsISupports {

public:
	
        NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPOSIXLOCALE_IID)
	NS_IMETHOD GetPlatformLocale(const nsString* locale,char* posixLocale,size_t length)=0;
	NS_IMETHOD GetXPLocale(const char* posixLocale, nsString* locale)=0;
};


#endif

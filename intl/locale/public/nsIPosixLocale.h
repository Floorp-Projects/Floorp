/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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


class nsIPosixLocale : public nsISupports {

public:
	
	NS_IMETHOD GetPlatformLocale(const nsString* locale,char* posixLocale,size_t length)=0;
	NS_IMETHOD GetXPLocale(const char* posixLocale, nsString* locale)=0;
};


#endif

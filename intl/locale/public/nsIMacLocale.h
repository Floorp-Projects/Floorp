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
#ifndef nsIMacLocale_h__
#define nsIMacLocale_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include <Script.h>

// {E58B24B2-FD1A-11d2-9E8E-0060089FE59B}
#define NS_IMACLOCALE_IID							\
{	0xe58b24b2, 0xfd1a, 0x11d2, 					\
{	0x9e, 0x8e, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}	


class nsIMacLocale : public nsISupports {

public:
	
        NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMACLOCALE_IID)
	NS_IMETHOD GetPlatformLocale(const nsString* locale,short* scriptCode,short* langCode, short* regionCode) = 0;
	NS_IMETHOD GetXPLocale(short scriptCode,short langCode, short regionCode, nsString* locale) = 0;
};


#endif

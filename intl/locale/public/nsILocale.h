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
#ifndef nsILocale_h__
#define nsILocale_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"

// {DDD50DD0-AB3F-11d2-AF09-0060089FE59B}
#define NS_ILOCALE_IID							\
{	0xddd50dd0,0xab3f,0x11d2,					\
{	0xaf,0x9,0x0,0x60,0x8,0x9f,0xe5,0x9b} }


class nsILocale : public nsISupports {

public:
	
	NS_IMETHOD GetCatagory(const nsString* catagory, nsString* result) = 0;

};


#endif

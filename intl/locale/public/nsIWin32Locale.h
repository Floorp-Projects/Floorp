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
#ifndef nsIWin32Locale_h__
#define nsIWin32Locale_h__


#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include <windows.h>

// {D92D57C2-BA1D-11d2-AF0C-0060089FE59B}
#define	NS_IWIN32LOCALE_IID							\
{	0xd92d57c2, 0xba1d, 0x11d2,						\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}


class nsIWin32Locale : public nsISupports {

public:
	
        NS_DEFINE_STATIC_IID_ACCESSOR(NS_IWIN32LOCALE_IID)
	NS_IMETHOD GetPlatformLocale(const nsString* locale,LCID* winLCID) = 0;
	NS_IMETHOD GetXPLocale(LCID winLCID,nsString* locale) = 0;
};

nsresult GetPlatformLocale(const PRUnichar* localeValue, LCID* winLCID);
nsresult GetXPLocale(LCID winLCID, PRUnichar* localeValue);

#endif

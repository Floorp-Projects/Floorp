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
 * Contributor(s): Henry Sobotka <sobotka@axess.com> January 2000 update
 *
 */

#ifndef _nsios2locale_h_
#define _nsios2locale_h_

#include "nsISupports.h"
#include "nscore.h"
#include "nsString.h"
#include "unidef.h"     


// {F25F74F0-FB59-11d3-A9F2-00203522A03C}
#define NS_IOS2LOCALE_IID	                    \
{ 0xf25f74f0, 0xfb59, 0x11d3,                     \
{ 0xa9, 0xf2, 0x0, 0x20, 0x35, 0x22, 0xa0, 0x3c }}


class nsIOS2Locale : public nsISupports
{
 public:

   NS_DEFINE_STATIC_IID_ACCESSOR(NS_IOS2LOCALE_IID)
   NS_IMETHOD GetPlatformLocale(PRUnichar* os2Locale,size_t length)=0;
   NS_IMETHOD GetXPLocale(const char* os2Locale, nsString* locale)=0;
};

#endif

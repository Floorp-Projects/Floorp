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

#ifndef nsIPlatformCharset_h__
#define nsIPlatformCharset_h__

#include "nsString.h"
#include "nsISupports.h"

// Interface ID for our nsIPlatformCharset interface

#define NS_IPLATFORMCHARSET_IID \
{ 0x84b0f181, 0xc6c7, 0x11d2, {0xb3, 0xb0, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 }}

#define NS_PLATFORMCHARSET_CID \
{ 0x84b0f182, 0xc6c7, 0x11d2, {0xb3, 0xb0, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 }}

#define NS_PLATFORMCHARSET_CONTRACTID "@mozilla.org/intl/platformcharset;1"

typedef enum {
     kPlatformCharsetSel_PlainTextInClipboard = 0,
     kPlatformCharsetSel_FileName = 1,
     kPlatformCharsetSel_Menu = 2,
     kPlatformCharsetSel_4xBookmarkFile = 3,
     kPlatformCharsetSel_KeyboardInput = 4,
     kPlatformCharsetSel_WindowManager = 5
} nsPlatformCharsetSel;

class nsIPlatformCharset : public nsISupports
{
public:
 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLATFORMCHARSET_IID)

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsString& oResult) = 0;

  NS_IMETHOD GetDefaultCharsetForLocale(const PRUnichar* localeName, PRUnichar** _retValue) = 0;

};

#endif /* nsIPlatformCharset_h__ */

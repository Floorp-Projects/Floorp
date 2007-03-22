/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIPlatformCharset_h__
#define nsIPlatformCharset_h__

#include "nsStringGlue.h"
#include "nsISupports.h"

// Interface ID for our nsIPlatformCharset interface

/* 778859d5-fc01-4f4b-bfaa-3c0d1b6c81d6 */
#define NS_IPLATFORMCHARSET_IID \
{   0x778859d5, \
    0xfc01, \
    0x4f4b, \
    {0xbf, 0xaa, 0x3c, 0x0d, 0x1b, 0x6c, 0x81, 0xd6} }

#define NS_PLATFORMCHARSET_CID \
{ 0x84b0f182, 0xc6c7, 0x11d2, {0xb3, 0xb0, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 }}

#define NS_PLATFORMCHARSET_CONTRACTID "@mozilla.org/intl/platformcharset;1"

typedef enum {
     kPlatformCharsetSel_PlainTextInClipboard = 0,
     kPlatformCharsetSel_FileName = 1,
     kPlatformCharsetSel_Menu = 2,
     kPlatformCharsetSel_4xBookmarkFile = 3,
     kPlatformCharsetSel_KeyboardInput = 4,
     kPlatformCharsetSel_WindowManager = 5,
     kPlatformCharsetSel_4xPrefsJS = 6,
     kPlatformCharsetSel_PlainTextInFile = 7
} nsPlatformCharsetSel;

class nsIPlatformCharset : public nsISupports
{
public:
 
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPLATFORMCHARSET_IID)

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsACString& oResult) = 0;

  NS_IMETHOD GetDefaultCharsetForLocale(const nsAString& localeName, nsACString& oResult) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPlatformCharset, NS_IPLATFORMCHARSET_IID)

#endif /* nsIPlatformCharset_h__ */

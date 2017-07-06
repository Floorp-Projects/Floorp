/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

/**
 * DO NOT ADD NEW USES OF THIS INTERFACE!
 * Removal is https://bugzilla.mozilla.org/show_bug.cgi?id=943272
 *
 * Instead, use UTF-16 APIs on Windows and UTF-8 APIs everywhere else.
 * Assume plain text files are UTF-8.
 */
class nsIPlatformCharset : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPLATFORMCHARSET_IID)

  NS_IMETHOD GetCharset(nsPlatformCharsetSel selector, nsACString& oResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPlatformCharset, NS_IPLATFORMCHARSET_IID)

#endif /* nsIPlatformCharset_h__ */

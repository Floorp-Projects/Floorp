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

#ifndef nsIFontMetrics_h___
#define nsIFontMetrics_h___

#include "nsISupports.h"
#include "nsCoord.h"

struct nsFont;
class nsString;
class nsIDeviceContext;

// IID for the nsIFontMetrics interface
#define NS_IFONT_METRICS_IID   \
{ 0xc74cb770, 0xa33e, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//----------------------------------------------------------------------

// Native font handle
typedef void* nsFontHandle;

// FontMetrics interface
class nsIFontMetrics : public nsISupports
{
  //what about encoding, where do we put that? MMP

public:
  //initializer
  NS_IMETHOD  Init(const nsFont& aFont, nsIDeviceContext *aContext) = 0;

  //get the width of an 8 bit char
  NS_IMETHOD  GetWidth(char aC, nscoord &aWidth) = 0;

  //get the width of a unicode char
  NS_IMETHOD  GetWidth(PRUnichar aC, nscoord &aWidth) = 0;

  //get the width of an nsString
  NS_IMETHOD  GetWidth(const nsString& aString, nscoord &aWidth) = 0;

  //get the width of 8 bit character string
  NS_IMETHOD  GetWidth(const char *aString, nscoord &aWidth) = 0;

  //get the width of a Unicode character string
  NS_IMETHOD  GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth) = 0;

  NS_IMETHOD  GetWidth(nsIDeviceContext *aContext, const nsString& aString, nscoord &aWidth) = 0;

  //get the height as this font
  NS_IMETHOD  GetHeight(nscoord &aHeight) = 0;

  //get height - (ascent + descent)
  NS_IMETHOD  GetLeading(nscoord &aLeading) = 0;

  //get the maximum character ascent
  NS_IMETHOD  GetMaxAscent(nscoord &aAscent) = 0;

  //get the maximum character descent
  NS_IMETHOD  GetMaxDescent(nscoord &aDescent) = 0;

  //get the maximum character advance for the font
  NS_IMETHOD  GetMaxAdvance(nscoord &aAdvance) = 0;

  //get the widths of the first 256 characters of the font
  NS_IMETHOD  GetWidths(const nscoord *&aWidths) = 0;

  //get the font associated width these metrics
  NS_IMETHOD  GetFont(const nsFont *&aFont) = 0;

  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle) = 0;
};

#endif /* nsIFontMetrics_h___ */

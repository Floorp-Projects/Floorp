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
  virtual nsresult Init(const nsFont& aFont, nsIDeviceContext *aContext) = 0;

  //get the width of an 8 bit char
  virtual nscoord GetWidth(char aC) = 0;

  //get the width of a unicode char
  virtual nscoord GetWidth(PRUnichar aC) = 0;

  //get the width of an nsString
  virtual nscoord GetWidth(const nsString& aString) = 0;

  //get the width of 8 bit character string
  virtual nscoord GetWidth(const char *aString) = 0;

  //get the width of a Unicode character string
  virtual nscoord GetWidth(const PRUnichar *aString, PRUint32 aLength) = 0;

  //get the height as this font
  virtual nscoord GetHeight() = 0;

  //get height - (ascent + descent)
  virtual nscoord GetLeading() = 0;

  //get the maximum character ascent
  virtual nscoord GetMaxAscent() = 0;

  //get the maximum character descent
  virtual nscoord GetMaxDescent() = 0;

  //get the maximum character advance for the font
  virtual nscoord GetMaxAdvance() = 0;

  //get the widths of the first 256 characters of the font
  virtual const nscoord *GetWidths() = 0;

  //get the font associated width these metrics
  virtual const nsFont& GetFont() = 0;

  virtual nsFontHandle GetFontHandle() = 0;
};

#endif /* nsIFontMetrics_h___ */

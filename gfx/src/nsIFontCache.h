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

#ifndef nsIFontCache_h___
#define nsIFontCache_h___

#include <stdio.h>
#include "nscore.h"
#include "nsISupports.h"

class nsIFontMetrics;
class nsIDeviceContext;
struct nsFont;

// IID for the nsIFontCache interface
#define NS_IFONT_CACHE_IID    \
{ 0x894758e0, 0xb49a, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

/**
 * Font cache interface. A font cache is bound to a particular device
 * context which it uses to realize the font metrics. The cache uses
 * the nsFont as a key.
 */
class nsIFontCache : public nsISupports
{
public:
  /**
   * Initialize the font cache. Call this after creating the font
   * cache and before trying to use it.
   */
  virtual nsresult Init(nsIDeviceContext* aContext) = 0;

  /**
   * Get the device context associated with this cache
   */
  virtual nsIDeviceContext* GetDeviceContext() const = 0;

  /**
   * Get metrics for a given font.
   */
  virtual nsIFontMetrics* GetMetricsFor(const nsFont& aFont) = 0;

  /**
   * Flush the cache.
   */
  virtual void Flush() = 0;
};

extern NS_GFX nsresult
  NS_NewFontCache(nsIFontCache **aInstancePtrResult);

#endif

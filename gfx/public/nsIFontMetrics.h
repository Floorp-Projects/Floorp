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

#ifndef nsIFontMetrics_h___
#define nsIFontMetrics_h___

#include "nsISupports.h"
#include "nsCoord.h"

struct nsFont;
class nsString;
class nsIDeviceContext;
class nsIAtom;

// IID for the nsIFontMetrics interface
#define NS_IFONT_METRICS_IID   \
{ 0xc74cb770, 0xa33e, 0x11d1, \
{ 0xa8, 0x24, 0x00, 0x40, 0x95, 0x9a, 0x28, 0xc9 } }

//----------------------------------------------------------------------

/**
 * A native font handle
 */
typedef void* nsFontHandle;

/**
 * Font metrics interface
 */
class nsIFontMetrics : public nsISupports
{
  // XXX what about encoding, where do we put that? MMP

public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFONT_METRICS_IID)

  /**
   * Initialize the font metrics. Call this after creating the font metrics.
   * Font metrics you get from the font cache do NOT need to be initialized
   *
   * @see nsIDeviceContext#GetMetricsFor()
   */
  NS_IMETHOD  Init(const nsFont& aFont, nsIAtom* aLangGroup,
                   nsIDeviceContext *aContext) = 0;

  /**
   * Destroy this font metrics. This breaks the association between
   * the font metrics and the device context.
   */
  NS_IMETHOD  Destroy() = 0;

#if defined(MOZ_MATHML) && defined(WIN32)
  // XXX currently implemented only on win32 -- other platforms should implement this
  /**
   * Return the font's italic slope, i.e., the tangent of the italic angle.
   * The slope = 0 for an upright font. It is > 0 for a forward-slanted
   * font (italic style) and is < 0 for a back-slanted font.
   */
  NS_IMETHOD  GetItalicSlope(float& aResult) = 0;
#endif

  /**
   * Return the font's xheight property, scaled into app-units.
   */
  NS_IMETHOD  GetXHeight(nscoord& aResult) = 0;

  /**
   * Return the font's superscript offset (the distance from the
   * baseline to where a superscript's baseline should be placed). The
   * value returned will be a positive value.
   */
  NS_IMETHOD  GetSuperscriptOffset(nscoord& aResult) = 0;

  /**
   * Return the font's subscript offset (the distance from the
   * baseline to where a subscript's baseline should be placed). The
   * value returned will be a positive value.
   */
  NS_IMETHOD  GetSubscriptOffset(nscoord& aResult) = 0;

  /**
   * Return the font's strikeout offset (the distance from the 
   * baseline to where a strikeout should be placed) and size
   * Positive values are above the baseline, negative below.
   */
  NS_IMETHOD  GetStrikeout(nscoord& aOffset, nscoord& aSize) = 0;

  /**
   * Return the font's underline offset (the distance from the 
   * baseline to where a underline should be placed) and size.
   * Positive values are above the baseline, negative below.
   */
  NS_IMETHOD  GetUnderline(nscoord& aOffset, nscoord& aSize) = 0;

  /**
   * Returns the height (in app units) of the font. This is ascent plus descent
   * plus any internal leading
   */
  NS_IMETHOD  GetHeight(nscoord &aHeight) = 0;

  /**
   * Returns the amount of internal leading (in app units) for the font. This
   * is computed as the "height  - (ascent + descent)"
   */
  NS_IMETHOD  GetLeading(nscoord &aLeading) = 0;

  /**
   * Returns, in app units, the maximum distance characters in this font extend
   * above the base line.
   */
  NS_IMETHOD  GetMaxAscent(nscoord &aAscent) = 0;

  /**
   * Returns, in app units, the maximum distance characters in this font extend
   * below the base line.
   */
  NS_IMETHOD  GetMaxDescent(nscoord &aDescent) = 0;

  /**
   * Returns, in app units, the maximum character advance for the font
   */
  NS_IMETHOD  GetMaxAdvance(nscoord &aAdvance) = 0;

  /**
   * Returns the font associated with these metrics
   */
  NS_IMETHOD  GetFont(const nsFont *&aFont) = 0;

  /**
   * Returns the language group associated with these metrics
   */
  NS_IMETHOD  GetLangGroup(nsIAtom** aLangGroup) = 0;

  /**
   * Returns the font handle associated with these metrics
   */
  NS_IMETHOD  GetFontHandle(nsFontHandle &aHandle) = 0;

#ifdef _WIN32
  /**
   * Returns the average character width
   */
  NS_IMETHOD  GetAveCharWidth(nscoord& aAveCharWidth) = 0;
#endif
};

#endif /* nsIFontMetrics_h___ */

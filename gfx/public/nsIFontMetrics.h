/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 *
 * This interface may be somewhat misnamed. A better name might be
 * nsIFontList. The style system uses the nsFont struct for various font
 * properties, one of which is font-family, which can contain a *list* of
 * font names. The nsFont struct is "realized" by asking the device context
 * to cough up an nsIFontMetrics object, which contains a list of real font
 * handles, one for each font mentioned in font-family (and for each fallback
 * when we fall off the end of that list).
 *
 * The style system needs to have access to certain metrics, such as the
 * em height (for the CSS "em" unit), and we use the first Western font's
 * metrics for that purpose. The platform-specific implementations are
 * expected to select non-Western fonts that "fit" reasonably well with the
 * Western font that is loaded at Init time.
 */
class nsIFontMetrics : public nsISupports
{
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
   *
   * This method will be removed once the callers have been moved over to the
   * new GetEmHeight (and possibly GetMaxHeight).
   */
  NS_IMETHOD  GetHeight(nscoord &aHeight) = 0;

#if defined(XP_UNIX) || defined(XP_PC) || defined(XP_MAC) || defined(XP_BEOS) 
#define NEW_FONT_HEIGHT_APIS 1
#endif
#ifdef NEW_FONT_HEIGHT_APIS
  /**
   * Returns the normal line height (em height + leading).
   */
  NS_IMETHOD  GetNormalLineHeight(nscoord &aHeight) = 0;
#endif /* NEW_FONT_HEIGHT_APIS */

  /**
   * Returns the amount of internal leading (in app units) for the font. This
   * is computed as the "height  - (ascent + descent)"
   */
  NS_IMETHOD  GetLeading(nscoord &aLeading) = 0;

#ifdef NEW_FONT_HEIGHT_APIS

  /**
   * Returns the height (in app units) of the Western font's em square. This is
   * em ascent plus em descent.
   */
  NS_IMETHOD  GetEmHeight(nscoord &aHeight) = 0;

  /**
   * Returns, in app units, the ascent part of the Western font's em square.
   */
  NS_IMETHOD  GetEmAscent(nscoord &aAscent) = 0;

  /**
   * Returns, in app units, the descent part of the Western font's em square.
   */
  NS_IMETHOD  GetEmDescent(nscoord &aDescent) = 0;

  /**
   * Returns the height (in app units) of the Western font's bounding box.
   * This is max ascent plus max descent.
   */
  NS_IMETHOD  GetMaxHeight(nscoord &aHeight) = 0;

#endif /* NEW_FONT_HEIGHT_APIS */

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

#if defined(_WIN32) || defined(XP_OS2)
  /**
   * Returns the average character width
   */
  NS_IMETHOD  GetAveCharWidth(nscoord& aAveCharWidth) = 0;
#endif
};

#endif /* nsIFontMetrics_h___ */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIStyleSheetLinkingElement_h__
#define nsIStyleSheetLinkingElement_h__


#include "nsISupports.h"

class nsICSSLoaderObserver;
class nsIURI;

#define NS_ISTYLESHEETLINKINGELEMENT_IID          \
{ 0xe5855604, 0x8a9a, 0x4181, \
 { 0xbe, 0x41, 0xdd, 0xf7, 0x08, 0x70, 0x3f, 0xbe } }

namespace mozilla {
class CSSStyleSheet;
} // namespace mozilla

class nsIStyleSheetLinkingElement : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISTYLESHEETLINKINGELEMENT_IID)

  /**
   * Used to make the association between a style sheet and
   * the element that linked it to the document.
   *
   * @param aStyleSheet the style sheet associated with this
   *                    element.
   */
  NS_IMETHOD SetStyleSheet(mozilla::CSSStyleSheet* aStyleSheet) = 0;

  /**
   * Used to obtain the style sheet linked in by this element.
   *
   * @return the style sheet associated with this element.
   */
  NS_IMETHOD_(mozilla::CSSStyleSheet*) GetStyleSheet() = 0;

  /**
   * Initialize the stylesheet linking element. If aDontLoadStyle is
   * true the element will ignore the first modification to the
   * element that would cause a stylesheet to be loaded. Subsequent
   * modifications to the element will not be ignored.
   */
  NS_IMETHOD InitStyleLinkElement(bool aDontLoadStyle) = 0;

  /**
   * Tells this element to update the stylesheet.
   *
   * @param aObserver    observer to notify once the stylesheet is loaded.
   *                     This will be passed to the CSSLoader
   * @param [out] aWillNotify whether aObserver will be notified when the sheet
   *                          loads.  If this is false, then either we didn't
   *                          start the sheet load at all, the load failed, or
   *                          this was an inline sheet that completely finished
   *                          loading.  In the case when the load failed the
   *                          failure code will be returned.
   * @param [out] whether the sheet is an alternate sheet.  This value is only
   *              meaningful if aWillNotify is true.
   * @param aForceUpdate whether we wand to force the update, flushing the
   *                     cached version if any.
   */
  NS_IMETHOD UpdateStyleSheet(nsICSSLoaderObserver* aObserver,
                              bool *aWillNotify,
                              bool *aIsAlternate,
                              bool aForceUpdate = false) = 0;

  /**
   * Tells this element whether to update the stylesheet when the
   * element's properties change.
   *
   * @param aEnableUpdates update on changes or not.
   */
  NS_IMETHOD SetEnableUpdates(bool aEnableUpdates) = 0;

  /**
   * Gets the charset that the element claims the style sheet is in
   *
   * @param aCharset the charset
   */
  NS_IMETHOD GetCharset(nsAString& aCharset) = 0;

  /**
   * Tells this element to use a different base URI. This is used for
   * proper loading of xml-stylesheet processing instructions in XUL overlays
   * and is only currently used by nsXMLStylesheetPI.
   *
   * @param aNewBaseURI the new base URI, nullptr to use the default base URI.
   */
  virtual void OverrideBaseURI(nsIURI* aNewBaseURI) = 0;

  // This doesn't entirely belong here since they only make sense for
  // some types of linking elements, but it's a better place than
  // anywhere else.
  virtual void SetLineNumber(uint32_t aLineNumber) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIStyleSheetLinkingElement,
                              NS_ISTYLESHEETLINKINGELEMENT_IID)

#endif // nsILinkingElement_h__

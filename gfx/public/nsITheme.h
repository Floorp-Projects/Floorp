/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
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

/* service providing platform-specific native rendering for widgets */

#ifndef nsITheme_h_
#define nsITheme_h_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsColor.h"

struct nsRect;
struct nsIntSize;
struct nsFont;
struct nsIntMargin;
class nsPresContext;
class nsIRenderingContext;
class nsIDeviceContext;
class nsIFrame;
class nsIContent;
class nsIAtom;

// IID for the nsITheme interface
// {887e8902-db6b-41b4-8481-a80f49c5a93a}
 #define NS_ITHEME_IID     \
{ 0x887e8902, 0xdb6b, 0x41b4, { 0x84, 0x81, 0xa8, 0x0f, 0x49, 0xc5, 0xa9, 0x3a } }

// {D930E29B-6909-44e5-AB4B-AF10D6923705}
#define NS_THEMERENDERER_CID \
{ 0xd930e29b, 0x6909, 0x44e5, { 0xab, 0x4b, 0xaf, 0x10, 0xd6, 0x92, 0x37, 0x5 } }

enum nsTransparencyMode {
  eTransparencyOpaque = 0,  // Fully opaque
  eTransparencyTransparent, // Parts of the window may be transparent
  eTransparencyGlass        // Transparent parts of the window have Vista AeroGlass effect applied
};

/**
 * nsITheme is a service that provides platform-specific native
 * rendering for widgets.  In other words, it provides the necessary
 * operations to draw a rendering object (an nsIFrame) as a native
 * widget.
 *
 * All the methods on nsITheme take a rendering context or device
 * context, a frame (the rendering object), and a widget type (one of
 * the constants in nsThemeConstants.h).
 */
class nsITheme: public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITHEME_IID)
  
  NS_IMETHOD DrawWidgetBackground(nsIRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect)=0;

  /**
   * Get the computed CSS border for the widget, in pixels.
   */
  NS_IMETHOD GetWidgetBorder(nsIDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType,
                             nsIntMargin* aResult)=0;

  /**
   * This method can return PR_FALSE to indicate that the CSS padding
   * value should be used.  Otherwise, it will fill in aResult with the
   * computed padding, in pixels, and return PR_TRUE.
   *
   * XXXldb This ought to be required to return true for non-containers
   * so that we don't let specified padding that has no effect change
   * the computed padding and potentially the size.
   */
  virtual PRBool GetWidgetPadding(nsIDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntMargin* aResult) = 0;

  /**
   * On entry, *aResult is positioned at 0,0 and sized to the new size
   * of aFrame (aFrame->GetSize() may be stale and should not be used).
   * This method can return PR_FALSE to indicate that no special
   * overflow area is required by the native widget. Otherwise it will
   * fill in aResult with the desired overflow area, in appunits, relative
   * to the frame origin, and return PR_TRUE.
   */
  virtual PRBool GetWidgetOverflow(nsIDeviceContext* aContext,
                                   nsIFrame* aFrame,
                                   PRUint8 aWidgetType,
                                   /*INOUT*/ nsRect* aOverflowRect)
  { return PR_FALSE; }

  /**
   * Get the minimum border-box size of a widget, in *pixels* (in
   * |aResult|).  If |aIsOverridable| is set to true, this size is a
   * minimum size; if false, this size is the only valid size for the
   * widget.
   */
  NS_IMETHOD GetMinimumWidgetSize(nsIRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntSize* aResult,
                                  PRBool* aIsOverridable)=0;

  virtual nsTransparencyMode GetWidgetTransparency(PRUint8 aWidgetType)=0;

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, PRBool* aShouldRepaint)=0;

  NS_IMETHOD ThemeChanged()=0;

  /**
   * Can the nsITheme implementation handle this widget?
   */
  virtual PRBool ThemeSupportsWidget(nsPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     PRUint8 aWidgetType)=0;

  virtual PRBool WidgetIsContainer(PRUint8 aWidgetType)=0;

  /**
   * Does the nsITheme implementation draw its own focus ring for this widget?
   */
  virtual PRBool ThemeDrawsFocusForWidget(nsPresContext* aPresContext,
                                          nsIFrame* aFrame,
                                          PRUint8 aWidgetType)=0;
  
  /**
    * Should we insert a dropmarker inside of combobox button?
   */
  virtual PRBool ThemeNeedsComboboxDropmarker()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITheme, NS_ITHEME_IID)

// Creator function
extern NS_METHOD NS_NewNativeTheme(nsISupports *aOuter, REFNSIID aIID, void **aResult);

#endif

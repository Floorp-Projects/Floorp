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
struct nsIntRect;
struct nsIntSize;
struct nsFont;
struct nsIntMargin;
class nsPresContext;
class nsRenderingContext;
class nsDeviceContext;
class nsIFrame;
class nsIContent;
class nsIAtom;
class nsIWidget;

// IID for the nsITheme interface
// {b0f3efe9-0bd4-4f6b-8daa-0ec7f6006822}
 #define NS_ITHEME_IID     \
{ 0xb0f3efe9, 0x0bd4, 0x4f6b, { 0x8d, 0xaa, 0x0e, 0xc7, 0xf6, 0x00, 0x68, 0x22 } }
// {D930E29B-6909-44e5-AB4B-AF10D6923705}
#define NS_THEMERENDERER_CID \
{ 0xd930e29b, 0x6909, 0x44e5, { 0xab, 0x4b, 0xaf, 0x10, 0xd6, 0x92, 0x37, 0x5 } }

enum nsTransparencyMode {
  eTransparencyOpaque = 0,  // Fully opaque
  eTransparencyTransparent, // Parts of the window may be transparent
  eTransparencyGlass,       // Transparent parts of the window have Vista AeroGlass effect applied
  eTransparencyBorderlessGlass // As above, but without a border around the opaque areas when there would otherwise be one with eTransparencyGlass
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

  /**
   * Draw the actual theme background.
   * @param aContext the context to draw into
   * @param aFrame the frame for the widget that we're drawing
   * @param aWidgetType the -moz-appearance value to draw
   * @param aRect the rectangle defining the area occupied by the widget
   * @param aDirtyRect the rectangle that needs to be drawn
   */
  NS_IMETHOD DrawWidgetBackground(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) = 0;

  /**
   * Get the computed CSS border for the widget, in pixels.
   */
  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, 
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
  virtual bool GetWidgetPadding(nsDeviceContext* aContext,
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
   *
   * This overflow area is used to determine what area needs to be
   * repainted when the widget changes.  However, it does not affect the
   * widget's size or what area is reachable by scrollbars.  (In other
   * words, in layout terms, it affects visual overflow but not
   * scrollable overflow.)
   */
  virtual bool GetWidgetOverflow(nsDeviceContext* aContext,
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
  NS_IMETHOD GetMinimumWidgetSize(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntSize* aResult,
                                  bool* aIsOverridable)=0;


  enum Transparency {
    eOpaque = 0,
    eTransparent,
    eUnknownTransparency
  };

  /**
   * Returns what we know about the transparency of the widget.
   */
  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, PRUint8 aWidgetType)
  { return eUnknownTransparency; }

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                nsIAtom* aAttribute, bool* aShouldRepaint)=0;

  NS_IMETHOD ThemeChanged()=0;

  /**
   * Can the nsITheme implementation handle this widget?
   */
  virtual bool ThemeSupportsWidget(nsPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     PRUint8 aWidgetType)=0;

  virtual bool WidgetIsContainer(PRUint8 aWidgetType)=0;

  /**
   * Does the nsITheme implementation draw its own focus ring for this widget?
   */
  virtual bool ThemeDrawsFocusForWidget(nsPresContext* aPresContext,
                                          nsIFrame* aFrame,
                                          PRUint8 aWidgetType)=0;
  
  /**
    * Should we insert a dropmarker inside of combobox button?
   */
  virtual bool ThemeNeedsComboboxDropmarker()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITheme, NS_ITHEME_IID)

// Creator function
extern nsresult NS_NewNativeTheme(nsISupports *aOuter, REFNSIID aIID, void **aResult);

#endif

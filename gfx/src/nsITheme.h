/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* service providing platform-specific native rendering for widgets */

#ifndef nsITheme_h_
#define nsITheme_h_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsColor.h"

struct nsRect;
struct nsIntRect;
struct nsIntSize;
class nsIntRegion;
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
{ 0x2e49c679, 0x2130, 0x432c, \
  { 0x92, 0xcb, 0xd4, 0x8e, 0x9a, 0xe2, 0x34, 0x75 } }
// {D930E29B-6909-44e5-AB4B-AF10D6923705}
#define NS_THEMERENDERER_CID \
{ 0x9020805b, 0x14a3, 0x4125, \
  { 0xa5, 0x63, 0x4a, 0x8c, 0x5d, 0xe0, 0xa9, 0xa3 } }

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
                                  uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect,
                                  nsIntRegion* aRegionToClear = nullptr) = 0;

  /**
   * Get the computed CSS border for the widget, in pixels.
   */
  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             uint8_t aWidgetType,
                             nsIntMargin* aResult)=0;

  /**
   * This method can return false to indicate that the CSS padding
   * value should be used.  Otherwise, it will fill in aResult with the
   * computed padding, in pixels, and return true.
   *
   * XXXldb This ought to be required to return true for non-containers
   * so that we don't let specified padding that has no effect change
   * the computed padding and potentially the size.
   */
  virtual bool GetWidgetPadding(nsDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  nsIntMargin* aResult) = 0;

  /**
   * On entry, *aResult is positioned at 0,0 and sized to the new size
   * of aFrame (aFrame->GetSize() may be stale and should not be used).
   * This method can return false to indicate that no special
   * overflow area is required by the native widget. Otherwise it will
   * fill in aResult with the desired overflow area, in appunits, relative
   * to the frame origin, and return true.
   *
   * This overflow area is used to determine what area needs to be
   * repainted when the widget changes.  However, it does not affect the
   * widget's size or what area is reachable by scrollbars.  (In other
   * words, in layout terms, it affects visual overflow but not
   * scrollable overflow.)
   */
  virtual bool GetWidgetOverflow(nsDeviceContext* aContext,
                                   nsIFrame* aFrame,
                                   uint8_t aWidgetType,
                                   /*INOUT*/ nsRect* aOverflowRect)
  { return false; }

  /**
   * Get the minimum border-box size of a widget, in *pixels* (in
   * |aResult|).  If |aIsOverridable| is set to true, this size is a
   * minimum size; if false, this size is the only valid size for the
   * widget.
   */
  NS_IMETHOD GetMinimumWidgetSize(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
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
  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType)
  { return eUnknownTransparency; }

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                nsIAtom* aAttribute, bool* aShouldRepaint)=0;

  NS_IMETHOD ThemeChanged()=0;

  virtual bool WidgetAppearanceDependsOnWindowFocus(uint8_t aWidgetType)
  { return false; }

  /**
   * Can the nsITheme implementation handle this widget?
   */
  virtual bool ThemeSupportsWidget(nsPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     uint8_t aWidgetType)=0;

  virtual bool WidgetIsContainer(uint8_t aWidgetType)=0;

  /**
   * Does the nsITheme implementation draw its own focus ring for this widget?
   */
  virtual bool ThemeDrawsFocusForWidget(uint8_t aWidgetType)=0;
  
  /**
    * Should we insert a dropmarker inside of combobox button?
   */
  virtual bool ThemeNeedsComboboxDropmarker()=0;

  /**
   * Should we hide scrollbars?
   */
  virtual bool ShouldHideScrollbars()
  { return false; }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITheme, NS_ITHEME_IID)

// Creator function
extern nsresult NS_NewNativeTheme(nsISupports *aOuter, REFNSIID aIID, void **aResult);

#endif

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
// {a21dd936-5960-46da-a724-7c114e421b41}
 #define NS_ITHEME_IID     \
{ 0xa21dd936, 0x5960, 0x46da, \
  { 0xa7, 0x24, 0x7c, 0x11, 0x4e, 0x42, 0x1b, 0x41 } }
// {0ae05515-cf7a-45a8-9e02-6556de7685b1}
#define NS_THEMERENDERER_CID \
{ 0x0ae05515, 0xcf7a, 0x45a8, \
  { 0x9e, 0x02, 0x65, 0x56, 0xde, 0x76, 0x85, 0xb1 } }

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
                                  const nsRect& aDirtyRect) = 0;

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
  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext,
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

  virtual bool NeedToClearBackgroundBehindWidget(uint8_t aWidgetType)
  { return false; }

  virtual bool WidgetProvidesFontSmoothingBackgroundColor(nsIFrame* aFrame,
                                      uint8_t aWidgetType, nscolor* aColor)
  { return false; }

  /**
   * ThemeGeometryType values are used for describing themed nsIFrames in
   * calls to nsIWidget::UpdateThemeGeometries. We don't simply pass the
   * -moz-appearance value ("widget type") of the frame because the widget may
   * want to treat different frames with the same -moz-appearance differently
   * based on other properties of the frame. So we give the theme a first look
   * at the frame in nsITheme::ThemeGeometryTypeForWidget and pass the
   * returned ThemeGeometryType along to the widget.
   * Each theme backend defines the ThemeGeometryType values it needs in its
   * own nsITheme subclass. eThemeGeometryTypeUnknown is the only value that's
   * shared between backends.
   */
  typedef uint8_t ThemeGeometryType;
  enum {
    eThemeGeometryTypeUnknown = 0
  };

  /**
   * Returns the theme geometry type that should be used in the ThemeGeometry
   * array that's passed to the widget using nsIWidget::UpdateThemeGeometries.
   * A return value of eThemeGeometryTypeUnknown means that this frame will
   * not be included in the ThemeGeometry array.
   */
  virtual ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame* aFrame,
                                                       uint8_t aWidgetType)
  { return eThemeGeometryTypeUnknown; }

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

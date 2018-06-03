/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* service providing platform-specific native rendering for widgets */

#ifndef nsITheme_h_
#define nsITheme_h_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "Units.h"

struct nsRect;
class gfxContext;
class nsAttrValue;
class nsPresContext;
class nsDeviceContext;
class nsIFrame;
class nsAtom;
class nsIWidget;

namespace mozilla {
class ComputedStyle;
namespace layers {
class StackingContextHelper;
class WebRenderLayerManager;
}
namespace wr {
class DisplayListBuilder;
class IpcResourceUpdateQueue;
}
}

// IID for the nsITheme interface
// {7329f760-08cb-450f-8225-dae729096dec}
 #define NS_ITHEME_IID     \
{ 0x7329f760, 0x08cb, 0x450f, \
  { 0x82, 0x25, 0xda, 0xe7, 0x29, 0x09, 0x6d, 0xec } }
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
protected:
  using LayoutDeviceIntMargin = mozilla::LayoutDeviceIntMargin;

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
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) = 0;

  /**
   * Get the used color of the given widget when it's specified as auto.
   * It's currently only used for scrollbar-*-color properties.
   */
  virtual nscolor GetWidgetAutoColor(mozilla::ComputedStyle* aStyle,
                                     uint8_t aWidgetType)
  { return NS_RGB(0, 0, 0); }

  /**
   * Create WebRender commands for the theme background.
   * @return true if the theme knows how to create WebRender commands for the
   *         given widget type, false if DrawWidgetBackground need sto be called
   *         instead.
   */
  virtual bool CreateWebRenderCommandsForWidget(mozilla::wr::DisplayListBuilder& aBuilder,
                                                mozilla::wr::IpcResourceUpdateQueue& aResources,
                                                const mozilla::layers::StackingContextHelper& aSc,
                                                mozilla::layers::WebRenderLayerManager* aManager,
                                                nsIFrame* aFrame,
                                                uint8_t aWidgetType,
                                                const nsRect& aRect) { return false; }

  /**
   * Return the border for the widget, in device pixels.
   */
  virtual MOZ_MUST_USE LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext* aContext,
                                                             nsIFrame* aFrame,
                                                             uint8_t aWidgetType) = 0;

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
                                  LayoutDeviceIntMargin* aResult) = 0;

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
                                  mozilla::LayoutDeviceIntSize* aResult,
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

  /**
   * Sets |*aShouldRepaint| to indicate whether an attribute or content state
   * change should trigger a repaint.  Call with null |aAttribute| (and
   * null |aOldValue|) for content state changes.
   */
  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                nsAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue)=0;

  NS_IMETHOD ThemeChanged()=0;

  virtual bool WidgetAppearanceDependsOnWindowFocus(uint8_t aWidgetType)
  { return false; }

  virtual bool NeedToClearBackgroundBehindWidget(nsIFrame* aFrame,
                                                 uint8_t aWidgetType)
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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITheme, NS_ITHEME_IID)

// Creator function
extern nsresult NS_NewNativeTheme(nsISupports *aOuter, REFNSIID aIID, void **aResult);

#endif

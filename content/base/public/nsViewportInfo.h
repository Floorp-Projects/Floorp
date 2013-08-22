/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewportInfo_h___
#define nsViewportInfo_h___

#include <stdint.h>
#include "nscore.h"

/**
 * Default values for the nsViewportInfo class.
 */
static const double   kViewportMinScale = 0.0;
static const double   kViewportMaxScale = 10.0;
static const uint32_t kViewportMinWidth = 200;
static const uint32_t kViewportMaxWidth = 10000;
static const uint32_t kViewportMinHeight = 223;
static const uint32_t kViewportMaxHeight = 10000;
static const int32_t  kViewportDefaultScreenWidth = 980;

/**
 * Information retrieved from the <meta name="viewport"> tag. See
 * nsContentUtils::GetViewportInfo for more information on this functionality.
 */
class MOZ_STACK_CLASS nsViewportInfo
{
  public:
    nsViewportInfo(uint32_t aDisplayWidth, uint32_t aDisplayHeight) :
      mDefaultZoom(1.0),
      mMinZoom(kViewportMinScale),
      mMaxZoom(kViewportMaxScale),
      mWidth(aDisplayWidth),
      mHeight(aDisplayHeight),
      mAutoSize(true),
      mAllowZoom(true)
    {
        ConstrainViewportValues();
    }

    nsViewportInfo(double aDefaultZoom,
                   double aMinZoom,
                   double aMaxZoom,
                   uint32_t aWidth,
                   uint32_t aHeight,
                   bool aAutoSize,
                   bool aAllowZoom) :
                     mDefaultZoom(aDefaultZoom),
                     mMinZoom(aMinZoom),
                     mMaxZoom(aMaxZoom),
                     mWidth(aWidth),
                     mHeight(aHeight),
                     mAutoSize(aAutoSize),
                     mAllowZoom(aAllowZoom)
    {
      ConstrainViewportValues();
    }

    double GetDefaultZoom() { return mDefaultZoom; }
    void SetDefaultZoom(const double aDefaultZoom);
    double GetMinZoom() { return mMinZoom; }
    double GetMaxZoom() { return mMaxZoom; }

    uint32_t GetWidth() { return mWidth; }
    uint32_t GetHeight() { return mHeight; }

    bool IsAutoSizeEnabled() { return mAutoSize; }
    bool IsZoomAllowed() { return mAllowZoom; }

  private:

    /**
     * Constrain the viewport calculations from the
     * nsContentUtils::GetViewportInfo() function in order to always return
     * sane minimum/maximum values.
     */
    void ConstrainViewportValues();

    // Default zoom indicates the level at which the display is 'zoomed in'
    // initially for the user, upon loading of the page.
    double mDefaultZoom;

    // The minimum zoom level permitted by the page.
    double mMinZoom;

    // The maximum zoom level permitted by the page.
    double mMaxZoom;

    // The width of the viewport, specified by the <meta name="viewport"> tag,
    // in CSS pixels.
    uint32_t mWidth;

    // The height of the viewport, specified by the <meta name="viewport"> tag,
    // in CSS pixels.
    uint32_t mHeight;

    // Whether or not we should automatically size the viewport to the device's
    // width. This is true if the document has been optimized for mobile, and
    // the width property of a specified <meta name="viewport"> tag is either
    // not specified, or is set to the special value 'device-width'.
    bool mAutoSize;

    // Whether or not the user can zoom in and out on the page. Default is true.
    bool mAllowZoom;
};

#endif


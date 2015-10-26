/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewportInfo_h___
#define nsViewportInfo_h___

#include <stdint.h>
#include "mozilla/Attributes.h"
#include "Units.h"

/**
 * Default values for the nsViewportInfo class.
 */
static const mozilla::LayoutDeviceToScreenScale kViewportMinScale(0.1f);
static const mozilla::LayoutDeviceToScreenScale kViewportMaxScale(10.0f);
static const mozilla::CSSIntSize kViewportMinSize(200, 40);
static const mozilla::CSSIntSize kViewportMaxSize(10000, 10000);

/**
 * Information retrieved from the <meta name="viewport"> tag. See
 * nsIDocument::GetViewportInfo for more information on this functionality.
 */
class MOZ_STACK_CLASS nsViewportInfo
{
  public:
    nsViewportInfo(const mozilla::ScreenIntSize& aDisplaySize,
                   const mozilla::CSSToScreenScale& aDefaultZoom,
                   bool aAllowZoom) :
      mDefaultZoomValid(true),
      mDefaultZoom(aDefaultZoom),
      mAutoSize(true),
      mAllowZoom(aAllowZoom)
    {
        mSize = mozilla::ScreenSize(aDisplaySize) / mDefaultZoom;
        mozilla::CSSToLayoutDeviceScale pixelRatio(1.0f);
        mMinZoom = pixelRatio * kViewportMinScale;
        mMaxZoom = pixelRatio * kViewportMaxScale;
        ConstrainViewportValues();
    }

    nsViewportInfo(const mozilla::CSSToScreenScale& aDefaultZoom,
                   const mozilla::CSSToScreenScale& aMinZoom,
                   const mozilla::CSSToScreenScale& aMaxZoom,
                   const mozilla::CSSSize& aSize,
                   bool aAutoSize,
                   bool aAllowZoom) :
                     mDefaultZoomValid(true),
                     mDefaultZoom(aDefaultZoom),
                     mMinZoom(aMinZoom),
                     mMaxZoom(aMaxZoom),
                     mSize(aSize),
                     mAutoSize(aAutoSize),
                     mAllowZoom(aAllowZoom)
    {
      ConstrainViewportValues();
    }

    bool IsDefaultZoomValid() const { return mDefaultZoomValid; }
    mozilla::CSSToScreenScale GetDefaultZoom() const { return mDefaultZoom; }
    mozilla::CSSToScreenScale GetMinZoom() const { return mMinZoom; }
    mozilla::CSSToScreenScale GetMaxZoom() const { return mMaxZoom; }

    mozilla::CSSSize GetSize() const { return mSize; }

    bool IsAutoSizeEnabled() const { return mAutoSize; }
    bool IsZoomAllowed() const { return mAllowZoom; }

  private:

    /**
     * Constrain the viewport calculations from the
     * nsIDocument::GetViewportInfo() function in order to always return
     * sane minimum/maximum values.
     */
    void ConstrainViewportValues();

    // If the default zoom was specified and was between the min and max
    // zoom values.
    bool mDefaultZoomValid;

    // Default zoom indicates the level at which the display is 'zoomed in'
    // initially for the user, upon loading of the page.
    mozilla::CSSToScreenScale mDefaultZoom;

    // The minimum zoom level permitted by the page.
    mozilla::CSSToScreenScale mMinZoom;

    // The maximum zoom level permitted by the page.
    mozilla::CSSToScreenScale mMaxZoom;

    // The size of the viewport, specified by the <meta name="viewport"> tag.
    mozilla::CSSSize mSize;

    // Whether or not we should automatically size the viewport to the device's
    // width. This is true if the document has been optimized for mobile, and
    // the width property of a specified <meta name="viewport"> tag is either
    // not specified, or is set to the special value 'device-width'.
    bool mAutoSize;

    // Whether or not the user can zoom in and out on the page. Default is true.
    bool mAllowZoom;
};

#endif


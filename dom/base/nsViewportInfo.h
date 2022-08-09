/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewportInfo_h___
#define nsViewportInfo_h___

#include <algorithm>
#include <stdint.h>
#include "mozilla/Attributes.h"
#include "mozilla/StaticPrefs_apz.h"
#include "Units.h"

namespace mozilla::dom {
enum class ViewportFitType : uint8_t {
  Auto,
  Contain,
  Cover,
};
}  // namespace mozilla::dom

/**
 * Default values for the nsViewportInfo class.
 */
static const mozilla::CSSIntSize kViewportMinSize(200, 40);
static const mozilla::CSSIntSize kViewportMaxSize(10000, 10000);

inline mozilla::LayoutDeviceToScreenScale ViewportMinScale() {
  return mozilla::LayoutDeviceToScreenScale(
      std::max(mozilla::StaticPrefs::apz_min_zoom(), 0.1f));
}

inline mozilla::LayoutDeviceToScreenScale ViewportMaxScale() {
  return mozilla::LayoutDeviceToScreenScale(
      std::min(mozilla::StaticPrefs::apz_max_zoom(), 100.0f));
}

/**
 * Information retrieved from the <meta name="viewport"> tag. See
 * Document::GetViewportInfo for more information on this functionality.
 */
class MOZ_STACK_CLASS nsViewportInfo {
 public:
  enum class AutoSizeFlag {
    AutoSize,
    FixedSize,
  };
  enum class AutoScaleFlag {
    AutoScale,
    FixedScale,
  };
  enum class ZoomFlag {
    AllowZoom,
    DisallowZoom,
  };
  enum class ZoomBehaviour {
    Mobile,
    Desktop,  // disallows zooming out past default zoom
  };
  nsViewportInfo(const mozilla::ScreenIntSize& aDisplaySize,
                 const mozilla::CSSToScreenScale& aDefaultZoom,
                 ZoomFlag aZoomFlag, ZoomBehaviour aBehaviour)
      : mDefaultZoom(aDefaultZoom),
        mViewportFit(mozilla::dom::ViewportFitType::Auto),
        mDefaultZoomValid(true),
        mAutoSize(true),
        mAllowZoom(aZoomFlag == ZoomFlag::AllowZoom) {
    mSize = mozilla::ScreenSize(aDisplaySize) / mDefaultZoom;
    mozilla::CSSToLayoutDeviceScale pixelRatio(1.0f);
    if (aBehaviour == ZoomBehaviour::Desktop) {
      mMinZoom = aDefaultZoom;
    } else {
      mMinZoom = pixelRatio * ViewportMinScale();
    }
    mMaxZoom = pixelRatio * ViewportMaxScale();
    ConstrainViewportValues();
  }

  nsViewportInfo(const mozilla::CSSToScreenScale& aDefaultZoom,
                 const mozilla::CSSToScreenScale& aMinZoom,
                 const mozilla::CSSToScreenScale& aMaxZoom,
                 const mozilla::CSSSize& aSize, AutoSizeFlag aAutoSizeFlag,
                 AutoScaleFlag aAutoScaleFlag, ZoomFlag aZoomFlag,
                 mozilla::dom::ViewportFitType aViewportFit)
      : mDefaultZoom(aDefaultZoom),
        mMinZoom(aMinZoom),
        mMaxZoom(aMaxZoom),
        mSize(aSize),
        mViewportFit(aViewportFit),
        mDefaultZoomValid(aAutoScaleFlag != AutoScaleFlag::AutoScale),
        mAutoSize(aAutoSizeFlag == AutoSizeFlag::AutoSize),
        mAllowZoom(aZoomFlag == ZoomFlag::AllowZoom) {
    ConstrainViewportValues();
  }

  bool IsDefaultZoomValid() const { return mDefaultZoomValid; }
  mozilla::CSSToScreenScale GetDefaultZoom() const { return mDefaultZoom; }
  mozilla::CSSToScreenScale GetMinZoom() const { return mMinZoom; }
  mozilla::CSSToScreenScale GetMaxZoom() const { return mMaxZoom; }

  mozilla::CSSSize GetSize() const { return mSize; }

  bool IsAutoSizeEnabled() const { return mAutoSize; }
  bool IsZoomAllowed() const { return mAllowZoom; }

  mozilla::dom::ViewportFitType GetViewportFit() const { return mViewportFit; }

  enum {
    Auto = -1,
    ExtendToZoom = -2,
    DeviceSize = -3,  // for device-width or device-height
  };
  // MIN/MAX computations where one of the arguments is auto resolve to the
  // other argument. For instance, MIN(0.25, auto) = 0.25, and
  // MAX(5, auto) = 5.
  // https://drafts.csswg.org/css-device-adapt/#constraining-defs
  static const float& Max(const float& aA, const float& aB);
  static const float& Min(const float& aA, const float& aB);

 private:
  /**
   * Constrain the viewport calculations from the
   * Document::GetViewportInfo() function in order to always return
   * sane minimum/maximum values.
   */
  void ConstrainViewportValues();

  // Default zoom indicates the level at which the display is 'zoomed in'
  // initially for the user, upon loading of the page.
  mozilla::CSSToScreenScale mDefaultZoom;

  // The minimum zoom level permitted by the page.
  mozilla::CSSToScreenScale mMinZoom;

  // The maximum zoom level permitted by the page.
  mozilla::CSSToScreenScale mMaxZoom;

  // The size of the viewport, specified by the <meta name="viewport"> tag.
  mozilla::CSSSize mSize;

  // The value of the viewport-fit.
  mozilla::dom::ViewportFitType mViewportFit;

  // If the default zoom was specified and was between the min and max
  // zoom values.
  // FIXME: Bug 1504362 - Unify this and mDefaultZoom into
  // Maybe<CSSToScreenScale>.
  bool mDefaultZoomValid;

  // Whether or not we should automatically size the viewport to the device's
  // width. This is true if the document has been optimized for mobile, and
  // the width property of a specified <meta name="viewport"> tag is either
  // not specified, or is set to the special value 'device-width'.
  bool mAutoSize;

  // Whether or not the user can zoom in and out on the page. Default is true.
  bool mAllowZoom;
};

#endif

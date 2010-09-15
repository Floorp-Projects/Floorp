/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is nsMediaFeatures.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* the features that media queries can test */

#include "nsMediaFeatures.h"
#include "nsGkAtoms.h"
#include "nsCSSKeywords.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDeviceContext.h"
#include "nsCSSValue.h"
#include "nsIDocShell.h"
#include "nsLayoutUtils.h"
#include "nsCSSRuleProcessor.h"

static const PRInt32 kOrientationKeywords[] = {
  eCSSKeyword_portrait,                 NS_STYLE_ORIENTATION_PORTRAIT,
  eCSSKeyword_landscape,                NS_STYLE_ORIENTATION_LANDSCAPE,
  eCSSKeyword_UNKNOWN,                  -1
};

static const PRInt32 kScanKeywords[] = {
  eCSSKeyword_progressive,              NS_STYLE_SCAN_PROGRESSIVE,
  eCSSKeyword_interlace,                NS_STYLE_SCAN_INTERLACE,
  eCSSKeyword_UNKNOWN,                  -1
};

// A helper for four features below
static nsSize
GetSize(nsPresContext* aPresContext)
{
    nsSize size;
    if (aPresContext->IsRootPaginatedDocument())
        // We want the page size, including unprintable areas and margins.
        size = aPresContext->GetPageSize();
    else
        size = aPresContext->GetVisibleArea().Size();
    return size;
}

static nsresult
GetWidth(nsPresContext* aPresContext, const nsMediaFeature*,
         nsCSSValue& aResult)
{
    nsSize size = GetSize(aPresContext);
    float pixelWidth = aPresContext->AppUnitsToFloatCSSPixels(size.width);
    aResult.SetFloatValue(pixelWidth, eCSSUnit_Pixel);
    return NS_OK;
}

static nsresult
GetHeight(nsPresContext* aPresContext, const nsMediaFeature*,
          nsCSSValue& aResult)
{
    nsSize size = GetSize(aPresContext);
    float pixelHeight = aPresContext->AppUnitsToFloatCSSPixels(size.height);
    aResult.SetFloatValue(pixelHeight, eCSSUnit_Pixel);
    return NS_OK;
}

inline static nsIDeviceContext*
GetDeviceContextFor(nsPresContext* aPresContext)
{
  // It would be nice to call
  // nsLayoutUtils::GetDeviceContextForScreenInfo here, except for two
  // things:  (1) it can flush, and flushing is bad here, and (2) it
  // doesn't really get us consistency in multi-monitor situations
  // *anyway*.
  return aPresContext->DeviceContext();
}

// A helper for three features below.
static nsSize
GetDeviceSize(nsPresContext* aPresContext)
{
    nsSize size;
    if (aPresContext->IsRootPaginatedDocument())
        // We want the page size, including unprintable areas and margins.
        // XXX The spec actually says we want the "page sheet size", but
        // how is that different?
        size = aPresContext->GetPageSize();
    else
        GetDeviceContextFor(aPresContext)->
            GetDeviceSurfaceDimensions(size.width, size.height);
    return size;
}

static nsresult
GetDeviceWidth(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
    nsSize size = GetDeviceSize(aPresContext);
    float pixelWidth = aPresContext->AppUnitsToFloatCSSPixels(size.width);
    aResult.SetFloatValue(pixelWidth, eCSSUnit_Pixel);
    return NS_OK;
}

static nsresult
GetDeviceHeight(nsPresContext* aPresContext, const nsMediaFeature*,
                nsCSSValue& aResult)
{
    nsSize size = GetDeviceSize(aPresContext);
    float pixelHeight = aPresContext->AppUnitsToFloatCSSPixels(size.height);
    aResult.SetFloatValue(pixelHeight, eCSSUnit_Pixel);
    return NS_OK;
}

static nsresult
GetOrientation(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
    nsSize size = GetSize(aPresContext);
    PRInt32 orientation;
    if (size.width > size.height) {
        orientation = NS_STYLE_ORIENTATION_LANDSCAPE;
    } else {
        // Per spec, square viewports should be 'portrait'
        orientation = NS_STYLE_ORIENTATION_PORTRAIT;
    }

    aResult.SetIntValue(orientation, eCSSUnit_Enumerated);
    return NS_OK;
}

// Helper for two features below
static nsresult
MakeArray(const nsSize& aSize, nsCSSValue& aResult)
{
    nsRefPtr<nsCSSValue::Array> a = nsCSSValue::Array::Create(2);

    a->Item(0).SetIntValue(aSize.width, eCSSUnit_Integer);
    a->Item(1).SetIntValue(aSize.height, eCSSUnit_Integer);

    aResult.SetArrayValue(a, eCSSUnit_Array);
    return NS_OK;
}

static nsresult
GetAspectRatio(nsPresContext* aPresContext, const nsMediaFeature*,
               nsCSSValue& aResult)
{
    return MakeArray(GetSize(aPresContext), aResult);
}

static nsresult
GetDeviceAspectRatio(nsPresContext* aPresContext, const nsMediaFeature*,
                     nsCSSValue& aResult)
{
    return MakeArray(GetDeviceSize(aPresContext), aResult);
}

static nsresult
GetColor(nsPresContext* aPresContext, const nsMediaFeature*,
         nsCSSValue& aResult)
{
    // FIXME:  This implementation is bogus.  nsThebesDeviceContext
    // doesn't provide reliable information (should be fixed in bug
    // 424386).
    // FIXME: On a monochrome device, return 0!
    nsIDeviceContext *dx = GetDeviceContextFor(aPresContext);
    PRUint32 depth;
    dx->GetDepth(depth);
    // The spec says to use bits *per color component*, so divide by 3,
    // and round down, since the spec says to use the smallest when the
    // color components differ.
    depth /= 3;
    aResult.SetIntValue(PRInt32(depth), eCSSUnit_Integer);
    return NS_OK;
}

static nsresult
GetColorIndex(nsPresContext* aPresContext, const nsMediaFeature*,
              nsCSSValue& aResult)
{
    // We should return zero if the device does not use a color lookup
    // table.  Stuart says that our handling of displays with 8-bit
    // color is bad enough that we never change the lookup table to
    // match what we're trying to display, so perhaps we should always
    // return zero.  Given that there isn't any better information
    // exposed, we don't have much other choice.
    aResult.SetIntValue(0, eCSSUnit_Integer);
    return NS_OK;
}

static nsresult
GetMonochrome(nsPresContext* aPresContext, const nsMediaFeature*,
              nsCSSValue& aResult)
{
    // For color devices we should return 0.
    // FIXME: On a monochrome device, return the actual color depth, not
    // 0!
    aResult.SetIntValue(0, eCSSUnit_Integer);
    return NS_OK;
}

static nsresult
GetResolution(nsPresContext* aPresContext, const nsMediaFeature*,
              nsCSSValue& aResult)
{
    // Resolution values are in device pixels, not CSS pixels.
    nsIDeviceContext *dx = GetDeviceContextFor(aPresContext);
    float dpi = float(dx->AppUnitsPerPhysicalInch()) / float(dx->AppUnitsPerDevPixel());
    aResult.SetFloatValue(dpi, eCSSUnit_Inch);
    return NS_OK;
}

static nsresult
GetScan(nsPresContext* aPresContext, const nsMediaFeature*,
        nsCSSValue& aResult)
{
    // Since Gecko doesn't support the 'tv' media type, the 'scan'
    // feature is never present.
    aResult.Reset();
    return NS_OK;
}

static nsresult
GetGrid(nsPresContext* aPresContext, const nsMediaFeature*,
        nsCSSValue& aResult)
{
    // Gecko doesn't support grid devices (e.g., ttys), so the 'grid'
    // feature is always 0.
    aResult.SetIntValue(0, eCSSUnit_Integer);
    return NS_OK;
}

static nsresult
GetDevicePixelRatio(nsPresContext* aPresContext, const nsMediaFeature*,
                    nsCSSValue& aResult)
{
  float ratio = aPresContext->CSSPixelsToDevPixels(1.0f);
  aResult.SetFloatValue(ratio, eCSSUnit_Number);
  return NS_OK;
}

static nsresult
GetSystemMetric(nsPresContext* aPresContext, const nsMediaFeature* aFeature,
                nsCSSValue& aResult)
{
    NS_ABORT_IF_FALSE(aFeature->mValueType == nsMediaFeature::eBoolInteger,
                      "unexpected type");
    nsIAtom *metricAtom = *aFeature->mData.mMetric;
    PRBool hasMetric = nsCSSRuleProcessor::HasSystemMetric(metricAtom);
    aResult.SetIntValue(hasMetric ? 1 : 0, eCSSUnit_Integer);
    return NS_OK;
}

/*
 * Adding new media features requires (1) adding the new feature to this
 * array, with appropriate entries (and potentially any new code needed
 * to support new types in these entries and (2) ensuring that either
 * nsPresContext::MediaFeatureValuesChanged or
 * nsPresContext::PostMediaFeatureValuesChangedEvent is called when the
 * value that would be returned by the entry's mGetter changes.
 */

/* static */ const nsMediaFeature
nsMediaFeatures::features[] = {
    {
        &nsGkAtoms::width,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        { nsnull },
        GetWidth
    },
    {
        &nsGkAtoms::height,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        { nsnull },
        GetHeight
    },
    {
        &nsGkAtoms::deviceWidth,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        { nsnull },
        GetDeviceWidth
    },
    {
        &nsGkAtoms::deviceHeight,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eLength,
        { nsnull },
        GetDeviceHeight
    },
    {
        &nsGkAtoms::orientation,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eEnumerated,
        { kOrientationKeywords },
        GetOrientation
    },
    {
        &nsGkAtoms::aspectRatio,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eIntRatio,
        { nsnull },
        GetAspectRatio
    },
    {
        &nsGkAtoms::deviceAspectRatio,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eIntRatio,
        { nsnull },
        GetDeviceAspectRatio
    },
    {
        &nsGkAtoms::color,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        { nsnull },
        GetColor
    },
    {
        &nsGkAtoms::colorIndex,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        { nsnull },
        GetColorIndex
    },
    {
        &nsGkAtoms::monochrome,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        { nsnull },
        GetMonochrome
    },
    {
        &nsGkAtoms::resolution,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eResolution,
        { nsnull },
        GetResolution
    },
    {
        &nsGkAtoms::scan,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eEnumerated,
        { kScanKeywords },
        GetScan
    },
    {
        &nsGkAtoms::grid,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { nsnull },
        GetGrid
    },

    // Mozilla extensions
    {
        &nsGkAtoms::_moz_device_pixel_ratio,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eFloat,
        { nsnull },
        GetDevicePixelRatio
    },
    {
        &nsGkAtoms::_moz_scrollbar_start_backward,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::scrollbar_start_backward },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_scrollbar_start_forward,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::scrollbar_start_forward },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_scrollbar_end_backward,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::scrollbar_end_backward },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_scrollbar_end_forward,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::scrollbar_end_forward },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_scrollbar_thumb_proportional,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::scrollbar_thumb_proportional },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_images_in_menus,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::images_in_menus },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_images_in_buttons,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::images_in_buttons },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_windows_default_theme,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::windows_default_theme },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_mac_graphite_theme,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::mac_graphite_theme },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_windows_compositor,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::windows_compositor },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_windows_classic,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::windows_classic },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_touch_enabled,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::touch_enabled },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_maemo_classic,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::maemo_classic },
        GetSystemMetric
    },
    {
        &nsGkAtoms::_moz_menubar_drag,
        nsMediaFeature::eMinMaxNotAllowed,
        nsMediaFeature::eBoolInteger,
        { &nsGkAtoms::menubar_drag },
        GetSystemMetric
    },

    // Null-mName terminator:
    {
        nsnull,
        nsMediaFeature::eMinMaxAllowed,
        nsMediaFeature::eInteger,
        { nsnull },
        nsnull
    },
};

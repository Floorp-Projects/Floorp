/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCCallbackHelper.h"
#include "gfxPrefs.h" // For gfxPrefs::LayersTilesEnabled, LayersTileWidth/Height
#include "mozilla/Preferences.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsIDOMElement.h"
#include "nsIInterfaceRequestorUtils.h"

namespace mozilla {
namespace layers {

bool
APZCCallbackHelper::HasValidPresShellId(nsIDOMWindowUtils* aUtils,
                                        const FrameMetrics& aMetrics)
{
    MOZ_ASSERT(aUtils);

    uint32_t presShellId;
    nsresult rv = aUtils->GetPresShellId(&presShellId);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return NS_SUCCEEDED(rv) && aMetrics.GetPresShellId() == presShellId;
}

static void
AdjustDisplayPortForScrollDelta(mozilla::layers::FrameMetrics& aFrameMetrics,
                                const CSSPoint& aActualScrollOffset)
{
    // Correct the display-port by the difference between the requested scroll
    // offset and the resulting scroll offset after setting the requested value.
    LayerPoint shift =
        (aFrameMetrics.GetScrollOffset() - aActualScrollOffset) *
        aFrameMetrics.LayersPixelsPerCSSPixel();
    LayerMargin margins = aFrameMetrics.GetDisplayPortMargins();
    margins.left -= shift.x;
    margins.right += shift.x;
    margins.top -= shift.y;
    margins.bottom += shift.y;
    aFrameMetrics.SetDisplayPortMargins(margins);
}

static void
RecenterDisplayPort(mozilla::layers::FrameMetrics& aFrameMetrics)
{
    LayerMargin margins = aFrameMetrics.GetDisplayPortMargins();
    margins.right = margins.left = margins.LeftRight() / 2;
    margins.top = margins.bottom = margins.TopBottom() / 2;
    aFrameMetrics.SetDisplayPortMargins(margins);
}

static CSSPoint
ScrollFrameTo(nsIScrollableFrame* aFrame, const CSSPoint& aPoint, bool& aSuccessOut)
{
  aSuccessOut = false;

  if (!aFrame) {
    return aPoint;
  }

  CSSPoint targetScrollPosition = aPoint;

  // If the frame is overflow:hidden on a particular axis, we don't want to allow
  // user-driven scroll on that axis. Simply set the scroll position on that axis
  // to whatever it already is. Note that this will leave the APZ's async scroll
  // position out of sync with the gecko scroll position, but APZ can deal with that
  // (by design). Note also that when we run into this case, even if both axes
  // have overflow:hidden, we want to set aSuccessOut to true, so that the displayport
  // follows the async scroll position rather than the gecko scroll position.
  CSSPoint geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
  if (aFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.y = geckoScrollPosition.y;
  }
  if (aFrame->GetScrollbarStyles().mHorizontal == NS_STYLE_OVERFLOW_HIDDEN) {
    targetScrollPosition.x = geckoScrollPosition.x;
  }

  // If the scrollable frame is currently in the middle of an async or smooth
  // scroll then we don't want to interrupt it (see bug 961280).
  // Also if the scrollable frame got a scroll request from something other than us
  // since the last layers update, then we don't want to push our scroll request
  // because we'll clobber that one, which is bad.
  if (!aFrame->IsProcessingAsyncScroll() &&
     (!aFrame->OriginOfLastScroll() || aFrame->OriginOfLastScroll() == nsGkAtoms::apz)) {
    aFrame->ScrollToCSSPixelsApproximate(targetScrollPosition, nsGkAtoms::apz);
    geckoScrollPosition = CSSPoint::FromAppUnits(aFrame->GetScrollPosition());
    aSuccessOut = true;
  }
  // Return the final scroll position after setting it so that anything that relies
  // on it can have an accurate value. Note that even if we set it above re-querying it
  // is a good idea because it may have gotten clamped or rounded.
  return geckoScrollPosition;
}

void
APZCCallbackHelper::UpdateRootFrame(nsIDOMWindowUtils* aUtils,
                                    FrameMetrics& aMetrics)
{
    // Precondition checks
    MOZ_ASSERT(aUtils);
    MOZ_ASSERT(aMetrics.GetUseDisplayPortMargins());
    if (aMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    // Set the scroll port size, which determines the scroll range. For example if
    // a 500-pixel document is shown in a 100-pixel frame, the scroll port length would
    // be 100, and gecko would limit the maximum scroll offset to 400 (so as to prevent
    // overscroll). Note that if the content here was zoomed to 2x, the document would
    // be 1000 pixels long but the frame would still be 100 pixels, and so the maximum
    // scroll range would be 900. Therefore this calculation depends on the zoom applied
    // to the content relative to the container.
    CSSSize scrollPort = aMetrics.CalculateCompositedSizeInCssPixels();
    aUtils->SetScrollPositionClampingScrollPortSize(scrollPort.width, scrollPort.height);

    // Scroll the window to the desired spot
    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aMetrics.GetScrollId());
    bool scrollUpdated = false;
    CSSPoint actualScrollOffset = ScrollFrameTo(sf, aMetrics.GetScrollOffset(), scrollUpdated);

    if (scrollUpdated) {
        // Correct the display port due to the difference between mScrollOffset and the
        // actual scroll offset.
        AdjustDisplayPortForScrollDelta(aMetrics, actualScrollOffset);
    } else {
        // For whatever reason we couldn't update the scroll offset on the scroll frame,
        // which means the data APZ used for its displayport calculation is stale. Fall
        // back to a sane default behaviour. Note that we don't tile-align the recentered
        // displayport because tile-alignment depends on the scroll position, and the
        // scroll position here is out of our control. See bug 966507 comment 21 for a
        // more detailed explanation.
        RecenterDisplayPort(aMetrics);
    }

    aMetrics.SetScrollOffset(actualScrollOffset);

    // The mZoom variable on the frame metrics stores the CSS-to-screen scale for this
    // frame. This scale includes all of the (cumulative) resolutions set on the presShells
    // from the root down to this frame. However, when setting the resolution, we only
    // want the piece of the resolution that corresponds to this presShell, rather than
    // all of the cumulative stuff, so we need to divide out the parent resolutions.
    // Finally, we multiply by a ScreenToLayerScale of 1.0f because the goal here is to
    // take the async zoom calculated by the APZC and tell gecko about it (turning it into
    // a "sync" zoom) which will update the resolution at which the layer is painted.
    ParentLayerToLayerScale presShellResolution =
        aMetrics.GetZoom()
        / aMetrics.mDevPixelsPerCSSPixel
        / aMetrics.GetParentResolution()
        * ScreenToLayerScale(1.0f);
    aUtils->SetResolution(presShellResolution.scale, presShellResolution.scale);

    // Finally, we set the displayport.
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aMetrics.GetScrollId());
    if (!content) {
        return;
    }
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(content);
    if (!element) {
        return;
    }

    gfx::IntSize alignment = gfxPrefs::LayersTilesEnabled()
        ? gfx::IntSize(gfxPrefs::LayersTileWidth(), gfxPrefs::LayersTileHeight()) :
          gfx::IntSize(0, 0);
    LayerMargin margins = aMetrics.GetDisplayPortMargins();
    aUtils->SetDisplayPortMarginsForElement(margins.left,
                                            margins.top,
                                            margins.right,
                                            margins.bottom,
                                            alignment.width,
                                            alignment.height,
                                            element, 0);
    CSSRect baseCSS = aMetrics.CalculateCompositedRectInCssPixels();
    nsRect base(baseCSS.x * nsPresContext::AppUnitsPerCSSPixel(),
                baseCSS.y * nsPresContext::AppUnitsPerCSSPixel(),
                baseCSS.width * nsPresContext::AppUnitsPerCSSPixel(),
                baseCSS.height * nsPresContext::AppUnitsPerCSSPixel());
    nsLayoutUtils::SetDisplayPortBaseIfNotSet(content, base);
}

void
APZCCallbackHelper::UpdateSubFrame(nsIContent* aContent,
                                   FrameMetrics& aMetrics)
{
    // Precondition checks
    MOZ_ASSERT(aContent);
    MOZ_ASSERT(aMetrics.GetUseDisplayPortMargins());
    if (aMetrics.GetScrollId() == FrameMetrics::NULL_SCROLL_ID) {
        return;
    }

    nsCOMPtr<nsIDOMWindowUtils> utils = GetDOMWindowUtils(aContent);
    if (!utils) {
        return;
    }

    // We currently do not support zooming arbitrary subframes. They can only
    // be scrolled, so here we only have to set the scroll position and displayport.

    nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(aMetrics.GetScrollId());
    bool scrollUpdated = false;
    CSSPoint actualScrollOffset = ScrollFrameTo(sf, aMetrics.GetScrollOffset(), scrollUpdated);

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aContent);
    if (element) {
        if (scrollUpdated) {
            AdjustDisplayPortForScrollDelta(aMetrics, actualScrollOffset);
        } else {
            RecenterDisplayPort(aMetrics);
        }
        gfx::IntSize alignment = gfxPrefs::LayersTilesEnabled()
            ? gfx::IntSize(gfxPrefs::LayersTileWidth(), gfxPrefs::LayersTileHeight()) :
              gfx::IntSize(0, 0);
        LayerMargin margins = aMetrics.GetDisplayPortMargins();
        utils->SetDisplayPortMarginsForElement(margins.left,
                                               margins.top,
                                               margins.right,
                                               margins.bottom,
                                               alignment.width,
                                               alignment.height,
                                               element, 0);
        CSSRect baseCSS = aMetrics.CalculateCompositedRectInCssPixels();
        nsRect base(baseCSS.x * nsPresContext::AppUnitsPerCSSPixel(),
                    baseCSS.y * nsPresContext::AppUnitsPerCSSPixel(),
                    baseCSS.width * nsPresContext::AppUnitsPerCSSPixel(),
                    baseCSS.height * nsPresContext::AppUnitsPerCSSPixel());
        nsLayoutUtils::SetDisplayPortBaseIfNotSet(aContent, base);
    }

    aMetrics.SetScrollOffset(actualScrollOffset);
}

already_AddRefed<nsIDOMWindowUtils>
APZCCallbackHelper::GetDOMWindowUtils(const nsIDocument* aDoc)
{
    nsCOMPtr<nsIDOMWindowUtils> utils;
    nsCOMPtr<nsIDOMWindow> window = aDoc->GetDefaultView();
    if (window) {
        utils = do_GetInterface(window);
    }
    return utils.forget();
}

already_AddRefed<nsIDOMWindowUtils>
APZCCallbackHelper::GetDOMWindowUtils(const nsIContent* aContent)
{
    nsCOMPtr<nsIDOMWindowUtils> utils;
    nsIDocument* doc = aContent->GetCurrentDoc();
    if (doc) {
        utils = GetDOMWindowUtils(doc);
    }
    return utils.forget();
}

bool
APZCCallbackHelper::GetOrCreateScrollIdentifiers(nsIContent* aContent,
                                                 uint32_t* aPresShellIdOut,
                                                 FrameMetrics::ViewID* aViewIdOut)
{
    if (!aContent) {
        return false;
    }
    *aViewIdOut = nsLayoutUtils::FindOrCreateIDFor(aContent);
    nsCOMPtr<nsIDOMWindowUtils> utils = GetDOMWindowUtils(aContent);
    return utils && (utils->GetPresShellId(aPresShellIdOut) == NS_OK);
}

class AcknowledgeScrollUpdateEvent : public nsRunnable
{
    typedef mozilla::layers::FrameMetrics::ViewID ViewID;

public:
    AcknowledgeScrollUpdateEvent(const ViewID& aScrollId, const uint32_t& aScrollGeneration)
        : mScrollId(aScrollId)
        , mScrollGeneration(aScrollGeneration)
    {
    }

    NS_IMETHOD Run() {
        MOZ_ASSERT(NS_IsMainThread());

        nsIScrollableFrame* sf = nsLayoutUtils::FindScrollableFrameFor(mScrollId);
        if (sf) {
            sf->ResetOriginIfScrollAtGeneration(mScrollGeneration);
        }

        return NS_OK;
    }

protected:
    ViewID mScrollId;
    uint32_t mScrollGeneration;
};

void
APZCCallbackHelper::AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                            const uint32_t& aScrollGeneration)
{
    nsCOMPtr<nsIRunnable> r1 = new AcknowledgeScrollUpdateEvent(aScrollId, aScrollGeneration);
    if (!NS_IsMainThread()) {
        NS_DispatchToMainThread(r1);
    } else {
        r1->Run();
    }
}

void
APZCCallbackHelper::UpdateCallbackTransform(const FrameMetrics& aApzcMetrics, const FrameMetrics& aActualMetrics)
{
    nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aApzcMetrics.GetScrollId());
    if (!content) {
        return;
    }
    CSSPoint scrollDelta = aApzcMetrics.GetScrollOffset() - aActualMetrics.GetScrollOffset();
    content->SetProperty(nsGkAtoms::apzCallbackTransform, new CSSPoint(scrollDelta),
                         nsINode::DeleteProperty<CSSPoint>);
}

CSSPoint
APZCCallbackHelper::ApplyCallbackTransform(const CSSPoint& aInput, const ScrollableLayerGuid& aGuid)
{
    // XXX: technically we need to walk all the way up the layer tree from the layer
    // represented by |aGuid.mScrollId| up to the root of the layer tree and apply
    // the input transforms at each level in turn. However, it is quite difficult
    // to do this given that the structure of the layer tree may be different from
    // the structure of the content tree. Also it may be impossible to do correctly
    // at this point because there are other CSS transforms and such interleaved in
    // between so applying the inputTransforms all in a row at the end may leave
    // some things transformed improperly. In practice we should rarely hit scenarios
    // where any of this matters, so I'm skipping it for now and just doing the single
    // transform for the layer that the input hit.

    if (aGuid.mScrollId != FrameMetrics::NULL_SCROLL_ID) {
        nsCOMPtr<nsIContent> content = nsLayoutUtils::FindContentFor(aGuid.mScrollId);
        if (content) {
            void* property = content->GetProperty(nsGkAtoms::apzCallbackTransform);
            if (property) {
                CSSPoint delta = (*static_cast<CSSPoint*>(property));
                return aInput + delta;
            }
        }
    }
    return aInput;
}

nsIntPoint
APZCCallbackHelper::ApplyCallbackTransform(const nsIntPoint& aPoint,
                                        const ScrollableLayerGuid& aGuid,
                                        const CSSToLayoutDeviceScale& aScale)
{
    LayoutDevicePoint point = LayoutDevicePoint(aPoint.x, aPoint.y);
    point = ApplyCallbackTransform(point / aScale, aGuid) * aScale;
    LayoutDeviceIntPoint ret = gfx::RoundedToInt(point);
    return nsIntPoint(ret.x, ret.y);
}

}
}

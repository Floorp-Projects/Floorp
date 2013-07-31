/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PositionedEventTargeting.h"

#include "mozilla/Preferences.h"
#include "nsGUIEvent.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "nsEventListenerManager.h"
#include "nsPrintfCString.h"
#include "mozilla/dom/Element.h"
#include <algorithm>

namespace mozilla {

/*
 * The basic goal of FindFrameTargetedByInputEvent() is to find a good
 * target element that can respond to mouse events. Both mouse events and touch
 * events are targeted at this element. Note that even for touch events, we
 * check responsiveness to mouse events. We assume Web authors
 * designing for touch events will take their own steps to account for
 * inaccurate touch events.
 *
 * IsElementClickable() encapsulates the heuristic that determines whether an
 * element is expected to respond to mouse events. An element is deemed
 * "clickable" if it has registered listeners for "click", "mousedown" or
 * "mouseup", or is on a whitelist of element tags (<a>, <button>, <input>,
 * <select>, <textarea>, <label>), or has role="button", or is a link, or
 * is a suitable XUL element.
 * Any descendant (in the same document) of a clickable element is also
 * deemed clickable since events will propagate to the clickable element from its
 * descendant.
 *
 * If the element directly under the event position is clickable (or
 * event radii are disabled), we always use that element. Otherwise we collect
 * all frames intersecting a rectangle around the event position (taking CSS
 * transforms into account) and choose the best candidate in GetClosest().
 * Only IsElementClickable() candidates are considered; if none are found,
 * then we revert to targeting the element under the event position.
 * We ignore candidates outside the document subtree rooted by the
 * document of the element directly under the event position. This ensures that
 * event listeners in ancestor documents don't make it completely impossible
 * to target a non-clickable element in a child document.
 *
 * When both a frame and its ancestor are in the candidate list, we ignore
 * the ancestor. Otherwise a large ancestor element with a mouse event listener
 * and some descendant elements that need to be individually targetable would
 * disable intelligent targeting of those descendants within its bounds.
 *
 * GetClosest() computes the transformed axis-aligned bounds of each
 * candidate frame, then computes the Manhattan distance from the event point
 * to the bounds rect (which can be zero). The frame with the
 * shortest distance is chosen. For visited links we multiply the distance
 * by a specified constant weight; this can be used to make visited links
 * more or less likely to be targeted than non-visited links.
 */

struct EventRadiusPrefs
{
  uint32_t mVisitedWeight; // in percent, i.e. default is 100
  uint32_t mSideRadii[4]; // TRBL order, in millimetres
  bool mEnabled;
  bool mRegistered;
  bool mTouchOnly;
};

static EventRadiusPrefs sMouseEventRadiusPrefs;
static EventRadiusPrefs sTouchEventRadiusPrefs;

static const EventRadiusPrefs*
GetPrefsFor(nsEventStructType aEventStructType)
{
  EventRadiusPrefs* prefs = nullptr;
  const char* prefBranch = nullptr;
  if (aEventStructType == NS_TOUCH_EVENT) {
    prefBranch = "touch";
    prefs = &sTouchEventRadiusPrefs;
  } else if (aEventStructType == NS_MOUSE_EVENT) {
    // Mostly for testing purposes
    prefBranch = "mouse";
    prefs = &sMouseEventRadiusPrefs;
  } else {
    return nullptr;
  }

  if (!prefs->mRegistered) {
    prefs->mRegistered = true;

    nsPrintfCString enabledPref("ui.%s.radius.enabled", prefBranch);
    Preferences::AddBoolVarCache(&prefs->mEnabled, enabledPref.get(), false);

    nsPrintfCString visitedWeightPref("ui.%s.radius.visitedWeight", prefBranch);
    Preferences::AddUintVarCache(&prefs->mVisitedWeight, visitedWeightPref.get(), 100);

    static const char prefNames[4][9] =
      { "topmm", "rightmm", "bottommm", "leftmm" };
    for (int32_t i = 0; i < 4; ++i) {
      nsPrintfCString radiusPref("ui.%s.radius.%s", prefBranch, prefNames[i]);
      Preferences::AddUintVarCache(&prefs->mSideRadii[i], radiusPref.get(), 0);
    }

    if (aEventStructType == NS_MOUSE_EVENT) {
      Preferences::AddBoolVarCache(&prefs->mTouchOnly,
          "ui.mouse.radius.inputSource.touchOnly", true);
    } else {
      prefs->mTouchOnly = false;
    }
  }

  return prefs;
}

static bool
HasMouseListener(nsIContent* aContent)
{
  nsEventListenerManager* elm = aContent->GetListenerManager(false);
  if (!elm) {
    return false;
  }
  return elm->HasListenersFor(nsGkAtoms::onclick) ||
         elm->HasListenersFor(nsGkAtoms::onmousedown) ||
         elm->HasListenersFor(nsGkAtoms::onmouseup);
}

static bool
IsElementClickable(nsIFrame* aFrame)
{
  // Input events propagate up the content tree so we'll follow the content
  // ancestors to look for elements accepting the click.
  for (nsIContent* content = aFrame->GetContent(); content;
       content = content->GetFlattenedTreeParent()) {
    if (HasMouseListener(content)) {
      return true;
    }
    if (content->IsHTML()) {
      nsIAtom* tag = content->Tag();
      if (tag == nsGkAtoms::button ||
          tag == nsGkAtoms::input ||
          tag == nsGkAtoms::select ||
          tag == nsGkAtoms::textarea ||
          tag == nsGkAtoms::label) {
        return true;
      }
    } else if (content->IsXUL()) {
      nsIAtom* tag = content->Tag();
      // See nsCSSFrameConstructor::FindXULTagData. This code is not
      // really intended to be used with XUL, though.
      if (tag == nsGkAtoms::button ||
          tag == nsGkAtoms::checkbox ||
          tag == nsGkAtoms::radio ||
          tag == nsGkAtoms::autorepeatbutton ||
          tag == nsGkAtoms::menu ||
          tag == nsGkAtoms::menubutton ||
          tag == nsGkAtoms::menuitem ||
          tag == nsGkAtoms::menulist ||
          tag == nsGkAtoms::scrollbarbutton ||
          tag == nsGkAtoms::resizer) {
        return true;
      }
    }
    if (content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::role,
                             nsGkAtoms::button, eIgnoreCase)) {
      return true;
    }
    nsCOMPtr<nsIURI> linkURI;
    if (content->IsLink(getter_AddRefs(linkURI))) {
      return true;
    }
  }
  return false;
}

static nscoord
AppUnitsFromMM(nsIFrame* aFrame, uint32_t aMM, bool aVertical)
{
  nsPresContext* pc = aFrame->PresContext();
  nsIPresShell* presShell = pc->PresShell();
  float result = float(aMM) *
    (pc->DeviceContext()->AppUnitsPerPhysicalInch() / MM_PER_INCH_FLOAT) *
    (aVertical ? presShell->GetYResolution() : presShell->GetXResolution());
  return NSToCoordRound(result);
}

static nsRect
GetTargetRect(nsIFrame* aRootFrame, const nsPoint& aPointRelativeToRootFrame,
              const EventRadiusPrefs* aPrefs)
{
  nsMargin m(AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[0], true),
             AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[1], false),
             AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[2], true),
             AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[3], false));
  nsRect r(aPointRelativeToRootFrame, nsSize(0,0));
  r.Inflate(m);
  return r;
}

static float
ComputeDistanceFromRect(const nsPoint& aPoint, const nsRect& aRect)
{
  nscoord dx = std::max(0, std::max(aRect.x - aPoint.x, aPoint.x - aRect.XMost()));
  nscoord dy = std::max(0, std::max(aRect.y - aPoint.y, aPoint.y - aRect.YMost()));
  return float(NS_hypot(dx, dy));
}

static float
ComputeDistanceFromRegion(const nsPoint& aPoint, const nsRegion& aRegion)
{
  nsRegionRectIterator iter(aRegion);
  const nsRect* r;
  float minDist = -1;
  while ((r = iter.Next()) != nullptr) {
    float dist = ComputeDistanceFromRect(aPoint, *r);
    if (dist < minDist || minDist < 0) {
      minDist = dist;
    }
  }
  return minDist;
}

// Subtract aRegion from aExposedRegion as long as that doesn't make the
// exposed region get too complex or removes a big chunk of the exposed region.
static void
SubtractFromExposedRegion(nsRegion* aExposedRegion, const nsRegion& aRegion)
{
  if (aRegion.IsEmpty())
    return;

  nsRegion tmp;
  tmp.Sub(*aExposedRegion, aRegion);
  // Don't let *aExposedRegion get too complex, but don't let it fluff out to
  // its bounds either. Do let aExposedRegion get more complex if by doing so
  // we reduce its area by at least half.
  if (tmp.GetNumRects() <= 15 || tmp.Area() <= aExposedRegion->Area()/2) {
    *aExposedRegion = tmp;
  }
}

static nsIFrame*
GetClosest(nsIFrame* aRoot, const nsPoint& aPointRelativeToRootFrame,
           const nsRect& aTargetRect, const EventRadiusPrefs* aPrefs,
           nsIFrame* aRestrictToDescendants, nsTArray<nsIFrame*>& aCandidates)
{
  nsIFrame* bestTarget = nullptr;
  // Lower is better; distance is in appunits
  float bestDistance = 1e6f;
  nsRegion exposedRegion(aTargetRect);
  for (uint32_t i = 0; i < aCandidates.Length(); ++i) {
    nsIFrame* f = aCandidates[i];

    bool preservesAxisAlignedRectangles = false;
    nsRect borderBox = nsLayoutUtils::TransformFrameRectToAncestor(f,
        nsRect(nsPoint(0, 0), f->GetSize()), aRoot, &preservesAxisAlignedRectangles);
    nsRegion region;
    region.And(exposedRegion, borderBox);
    if (preservesAxisAlignedRectangles) {
      // Subtract from the exposed region if we have a transform that won't make
      // the bounds include a bunch of area that we don't actually cover.
      SubtractFromExposedRegion(&exposedRegion, region);
    }

    if (!IsElementClickable(f)) {
      continue;
    }
    // If our current closest frame is a descendant of 'f', skip 'f' (prefer
    // the nested frame).
    if (bestTarget && nsLayoutUtils::IsProperAncestorFrameCrossDoc(f, bestTarget, aRoot)) {
      continue;
    }
    if (!nsLayoutUtils::IsAncestorFrameCrossDoc(aRestrictToDescendants, f, aRoot)) {
      continue;
    }

    // distance is in appunits
    float distance = ComputeDistanceFromRegion(aPointRelativeToRootFrame, region);
    nsIContent* content = f->GetContent();
    if (content && content->IsElement() &&
        content->AsElement()->State().HasState(nsEventStates(NS_EVENT_STATE_VISITED))) {
      distance *= aPrefs->mVisitedWeight / 100.0f;
    }
    if (distance < bestDistance) {
      bestDistance = distance;
      bestTarget = f;
    }
  }
  return bestTarget;
}

nsIFrame*
FindFrameTargetedByInputEvent(const nsGUIEvent *aEvent,
                              nsIFrame* aRootFrame,
                              const nsPoint& aPointRelativeToRootFrame,
                              uint32_t aFlags)
{
  uint32_t flags = (aFlags & INPUT_IGNORE_ROOT_SCROLL_FRAME) ?
     nsLayoutUtils::IGNORE_ROOT_SCROLL_FRAME : 0;
  nsIFrame* target =
    nsLayoutUtils::GetFrameForPoint(aRootFrame, aPointRelativeToRootFrame, flags);

  const EventRadiusPrefs* prefs = GetPrefsFor(aEvent->eventStructType);
  if (!prefs || !prefs->mEnabled || (target && IsElementClickable(target))) {
    return target;
  }

  // Do not modify targeting for actual mouse hardware; only for mouse
  // events generated by touch-screen hardware.
  if (aEvent->eventStructType == NS_MOUSE_EVENT &&
      prefs->mTouchOnly &&
      static_cast<const nsMouseEvent*>(aEvent)->inputSource !=
          nsIDOMMouseEvent::MOZ_SOURCE_TOUCH) {
      return target;
  }

  nsRect targetRect = GetTargetRect(aRootFrame, aPointRelativeToRootFrame, prefs);
  nsAutoTArray<nsIFrame*,8> candidates;
  nsresult rv = nsLayoutUtils::GetFramesForArea(aRootFrame, targetRect, candidates, flags);
  if (NS_FAILED(rv)) {
    return target;
  }

  // If the exact target is non-null, only consider candidate targets in the same
  // document as the exact target. Otherwise, if an ancestor document has
  // a mouse event handler for example, targets that are !IsElementClickable can
  // never be targeted --- something nsSubDocumentFrame in an ancestor document
  // would be targeted instead.
  nsIFrame* restrictToDescendants = target ?
    target->PresContext()->PresShell()->GetRootFrame() : aRootFrame;
  nsIFrame* closestClickable =
    GetClosest(aRootFrame, aPointRelativeToRootFrame, targetRect, prefs,
               restrictToDescendants, candidates);
  return closestClickable ? closestClickable : target;
}

}

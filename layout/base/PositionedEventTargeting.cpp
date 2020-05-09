/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PositionedEventTargeting.h"

#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/MouseEventBinding.h"
#include "nsFrameList.h"  // for DEBUG_FRAME_DUMP
#include "nsHTMLParts.h"
#include "nsLayoutUtils.h"
#include "nsGkAtoms.h"
#include "nsFontMetrics.h"
#include "nsPrintfCString.h"
#include "mozilla/dom/Element.h"
#include "nsRegion.h"
#include "nsDeviceContext.h"
#include "nsIContentInlines.h"
#include "nsIFrame.h"
#include <algorithm>
#include "LayersLogging.h"

using namespace mozilla;
using namespace mozilla::dom;

// If debugging this code you may wish to enable this logging, via
// the env var MOZ_LOG="event.retarget:4". For extra logging (getting
// frame dumps, use MOZ_LOG="event.retarget:5".
static mozilla::LazyLogModule sEvtTgtLog("event.retarget");
#define PET_LOG(...) MOZ_LOG(sEvtTgtLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {

/*
 * The basic goal of FindFrameTargetedByInputEvent() is to find a good
 * target element that can respond to mouse events. Both mouse events and touch
 * events are targeted at this element. Note that even for touch events, we
 * check responsiveness to mouse events. We assume Web authors
 * designing for touch events will take their own steps to account for
 * inaccurate touch events.
 *
 * GetClickableAncestor() encapsulates the heuristic that determines whether an
 * element is expected to respond to mouse events. An element is deemed
 * "clickable" if it has registered listeners for "click", "mousedown" or
 * "mouseup", or is on a whitelist of element tags (<a>, <button>, <input>,
 * <select>, <textarea>, <label>), or has role="button", or is a link, or
 * is a suitable XUL element.
 * Any descendant (in the same document) of a clickable element is also
 * deemed clickable since events will propagate to the clickable element from
 * its descendant.
 *
 * If the element directly under the event position is clickable (or
 * event radii are disabled), we always use that element. Otherwise we collect
 * all frames intersecting a rectangle around the event position (taking CSS
 * transforms into account) and choose the best candidate in GetClosest().
 * Only GetClickableAncestor() candidates are considered; if none are found,
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

struct EventRadiusPrefs {
  uint32_t mVisitedWeight;  // in percent, i.e. default is 100
  uint32_t mSideRadii[4];   // TRBL order, in millimetres
  bool mEnabled;
  bool mRegistered;
  bool mTouchOnly;
  bool mRepositionEventCoords;
};

static EventRadiusPrefs sMouseEventRadiusPrefs;
static EventRadiusPrefs sTouchEventRadiusPrefs;

static const EventRadiusPrefs* GetPrefsFor(EventClassID aEventClassID) {
  EventRadiusPrefs* prefs = nullptr;
  const char* prefBranch = nullptr;
  if (aEventClassID == eTouchEventClass) {
    prefBranch = "touch";
    prefs = &sTouchEventRadiusPrefs;
  } else if (aEventClassID == eMouseEventClass) {
    // Mostly for testing purposes
    prefBranch = "mouse";
    prefs = &sMouseEventRadiusPrefs;
  } else {
    return nullptr;
  }

  if (!prefs->mRegistered) {
    prefs->mRegistered = true;

    nsPrintfCString enabledPref("ui.%s.radius.enabled", prefBranch);
    Preferences::AddBoolVarCache(&prefs->mEnabled, enabledPref, false);

    nsPrintfCString visitedWeightPref("ui.%s.radius.visitedWeight", prefBranch);
    Preferences::AddUintVarCache(&prefs->mVisitedWeight, visitedWeightPref,
                                 100);

    static const char prefNames[4][9] = {"topmm", "rightmm", "bottommm",
                                         "leftmm"};
    for (int32_t i = 0; i < 4; ++i) {
      nsPrintfCString radiusPref("ui.%s.radius.%s", prefBranch, prefNames[i]);
      Preferences::AddUintVarCache(&prefs->mSideRadii[i], radiusPref, 0);
    }

    if (aEventClassID == eMouseEventClass) {
      Preferences::AddBoolVarCache(
          &prefs->mTouchOnly, "ui.mouse.radius.inputSource.touchOnly", true);
    } else {
      prefs->mTouchOnly = false;
    }

    nsPrintfCString repositionPref("ui.%s.radius.reposition", prefBranch);
    Preferences::AddBoolVarCache(&prefs->mRepositionEventCoords, repositionPref,
                                 false);
  }

  return prefs;
}

static bool HasMouseListener(nsIContent* aContent) {
  if (EventListenerManager* elm = aContent->GetExistingListenerManager()) {
    return elm->HasListenersFor(nsGkAtoms::onclick) ||
           elm->HasListenersFor(nsGkAtoms::onmousedown) ||
           elm->HasListenersFor(nsGkAtoms::onmouseup);
  }

  return false;
}

static bool HasTouchListener(nsIContent* aContent) {
  EventListenerManager* elm = aContent->GetExistingListenerManager();
  if (!elm) {
    return false;
  }

  // FIXME: Should this really use the pref rather than TouchEvent::PrefEnabled
  // or such?
  if (!StaticPrefs::dom_w3c_touch_events_enabled()) {
    return false;
  }

  return elm->HasListenersFor(nsGkAtoms::ontouchstart) ||
         elm->HasListenersFor(nsGkAtoms::ontouchend);
}

static bool IsDescendant(nsIFrame* aFrame, nsIContent* aAncestor,
                         nsAutoString* aLabelTargetId) {
  for (nsIContent* content = aFrame->GetContent(); content;
       content = content->GetFlattenedTreeParent()) {
    if (aLabelTargetId && content->IsHTMLElement(nsGkAtoms::label)) {
      content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::_for,
                                    *aLabelTargetId);
    }
    if (content == aAncestor) {
      return true;
    }
  }
  return false;
}

static nsIContent* GetClickableAncestor(
    nsIFrame* aFrame, nsAtom* stopAt = nullptr,
    nsAutoString* aLabelTargetId = nullptr) {
  // If the frame is `cursor:pointer` or inherits `cursor:pointer` from an
  // ancestor, treat it as clickable. This is a heuristic to deal with pages
  // where the click event listener is on the <body> or <html> element but it
  // triggers an action on some specific element. We want the specific element
  // to be considered clickable, and at least some pages that do this indicate
  // the clickability by setting `cursor:pointer`, so we use that here.
  // Note that descendants of `cursor:pointer` elements that override the
  // inherited `pointer` to `auto` or any other value are NOT treated as
  // clickable, because it seems like the content author is trying to express
  // non-clickability on that sub-element.
  // In the future depending on real-world cases it might make sense to expand
  // this check to any non-auto cursor. Such a change would also pick up things
  // like contenteditable or input fields, which can then be removed from the
  // loop below, and would have better performance.
  if (aFrame->StyleUI()->mCursor.keyword == StyleCursorKind::Pointer) {
    return aFrame->GetContent();
  }

  // Input events propagate up the content tree so we'll follow the content
  // ancestors to look for elements accepting the click.
  for (nsIContent* content = aFrame->GetContent(); content;
       content = content->GetFlattenedTreeParent()) {
    if (stopAt && content->IsHTMLElement(stopAt)) {
      break;
    }
    if (HasTouchListener(content) || HasMouseListener(content)) {
      return content;
    }
    if (content->IsAnyOfHTMLElements(nsGkAtoms::button, nsGkAtoms::input,
                                     nsGkAtoms::select, nsGkAtoms::textarea)) {
      return content;
    }
    if (content->IsHTMLElement(nsGkAtoms::label)) {
      if (aLabelTargetId) {
        content->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::_for,
                                      *aLabelTargetId);
      }
      return content;
    }

    // Bug 921928: we don't have access to the content of remote iframe.
    // So fluffing won't go there. We do an optimistic assumption here:
    // that the content of the remote iframe needs to be a target.
    if (content->IsHTMLElement(nsGkAtoms::iframe) &&
        content->AsElement()->AttrValueIs(kNameSpaceID_None,
                                          nsGkAtoms::mozbrowser,
                                          nsGkAtoms::_true, eIgnoreCase) &&
        content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::remote,
                                          nsGkAtoms::_true, eIgnoreCase)) {
      return content;
    }

    // See nsCSSFrameConstructor::FindXULTagData. This code is not
    // really intended to be used with XUL, though.
    if (content->IsAnyOfXULElements(
            nsGkAtoms::button, nsGkAtoms::checkbox, nsGkAtoms::radio,
            nsGkAtoms::menu, nsGkAtoms::menuitem, nsGkAtoms::menulist,
            nsGkAtoms::scrollbarbutton, nsGkAtoms::resizer)) {
      return content;
    }

    static Element::AttrValuesArray clickableRoles[] = {
        nsGkAtoms::button, nsGkAtoms::key, nullptr};
    if (content->IsElement() && content->AsElement()->FindAttrValueIn(
                                    kNameSpaceID_None, nsGkAtoms::role,
                                    clickableRoles, eIgnoreCase) >= 0) {
      return content;
    }
    if (content->IsEditable()) {
      return content;
    }
    nsCOMPtr<nsIURI> linkURI;
    if (content->IsLink(getter_AddRefs(linkURI))) {
      return content;
    }
  }
  return nullptr;
}

static nscoord AppUnitsFromMM(RelativeTo aFrame, uint32_t aMM) {
  nsPresContext* pc = aFrame.mFrame->PresContext();
  float result = float(aMM) * (pc->DeviceContext()->AppUnitsPerPhysicalInch() /
                               MM_PER_INCH_FLOAT);
  if (aFrame.mViewportType == ViewportType::Layout) {
    PresShell* presShell = pc->PresShell();
    result = result / presShell->GetResolution();
  }
  return NSToCoordRound(result);
}

/**
 * Clip aRect with the bounds of aFrame in the coordinate system of
 * aRootFrame. aRootFrame is an ancestor of aFrame.
 */
static nsRect ClipToFrame(const nsIFrame* aRootFrame, const nsIFrame* aFrame,
                          nsRect& aRect) {
  nsRect bound = nsLayoutUtils::TransformFrameRectToAncestor(
      aFrame, nsRect(nsPoint(0, 0), aFrame->GetSize()),
      RelativeTo{aRootFrame, ViewportType::Visual});
  nsRect result = bound.Intersect(aRect);
  return result;
}

static nsRect GetTargetRect(RelativeTo aRootFrame,
                            const nsPoint& aPointRelativeToRootFrame,
                            const nsIFrame* aRestrictToDescendants,
                            const EventRadiusPrefs* aPrefs, uint32_t aFlags) {
  nsMargin m(AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[0]),
             AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[1]),
             AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[2]),
             AppUnitsFromMM(aRootFrame, aPrefs->mSideRadii[3]));
  nsRect r(aPointRelativeToRootFrame, nsSize(0, 0));
  r.Inflate(m);
  if (!(aFlags & INPUT_IGNORE_ROOT_SCROLL_FRAME)) {
    // Don't clip this rect to the root scroll frame if the flag to ignore the
    // root scroll frame is set. Note that the GetClosest code will still
    // enforce that the target found is a descendant of aRestrictToDescendants.
    r = ClipToFrame(aRootFrame.mFrame, aRestrictToDescendants, r);
  }
  return r;
}

static float ComputeDistanceFromRect(const nsPoint& aPoint,
                                     const nsRect& aRect) {
  nscoord dx =
      std::max(0, std::max(aRect.x - aPoint.x, aPoint.x - aRect.XMost()));
  nscoord dy =
      std::max(0, std::max(aRect.y - aPoint.y, aPoint.y - aRect.YMost()));
  return float(NS_hypot(dx, dy));
}

static float ComputeDistanceFromRegion(const nsPoint& aPoint,
                                       const nsRegion& aRegion) {
  MOZ_ASSERT(!aRegion.IsEmpty(),
             "can't compute distance between point and empty region");
  float minDist = -1;
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    float dist = ComputeDistanceFromRect(aPoint, iter.Get());
    if (dist < minDist || minDist < 0) {
      minDist = dist;
    }
  }
  return minDist;
}

// Subtract aRegion from aExposedRegion as long as that doesn't make the
// exposed region get too complex or removes a big chunk of the exposed region.
static void SubtractFromExposedRegion(nsRegion* aExposedRegion,
                                      const nsRegion& aRegion) {
  if (aRegion.IsEmpty()) return;

  nsRegion tmp;
  tmp.Sub(*aExposedRegion, aRegion);
  // Don't let *aExposedRegion get too complex, but don't let it fluff out to
  // its bounds either. Do let aExposedRegion get more complex if by doing so
  // we reduce its area by at least half.
  if (tmp.GetNumRects() <= 15 || tmp.Area() <= aExposedRegion->Area() / 2) {
    *aExposedRegion = tmp;
  }
}

static nsIFrame* GetClosest(const nsIFrame* aRoot,
                            const nsPoint& aPointRelativeToRootFrame,
                            const nsRect& aTargetRect,
                            const EventRadiusPrefs* aPrefs,
                            const nsIFrame* aRestrictToDescendants,
                            nsIContent* aClickableAncestor,
                            nsTArray<nsIFrame*>& aCandidates) {
  nsIFrame* bestTarget = nullptr;
  // Lower is better; distance is in appunits
  float bestDistance = 1e6f;
  nsRegion exposedRegion(aTargetRect);
  for (uint32_t i = 0; i < aCandidates.Length(); ++i) {
    nsIFrame* f = aCandidates[i];

    bool preservesAxisAlignedRectangles = false;
    nsRect borderBox = nsLayoutUtils::TransformFrameRectToAncestor(
        f, nsRect(nsPoint(0, 0), f->GetSize()),
        RelativeTo{aRoot, ViewportType::Visual},
        &preservesAxisAlignedRectangles);
    PET_LOG("Checking candidate %p with border box %s\n", f,
            mozilla::layers::Stringify(borderBox).c_str());
    nsRegion region;
    region.And(exposedRegion, borderBox);
    if (region.IsEmpty()) {
      PET_LOG("  candidate %p had empty hit region\n", f);
      continue;
    }

    if (preservesAxisAlignedRectangles) {
      // Subtract from the exposed region if we have a transform that won't make
      // the bounds include a bunch of area that we don't actually cover.
      SubtractFromExposedRegion(&exposedRegion, region);
    }

    nsAutoString labelTargetId;
    if (aClickableAncestor &&
        !IsDescendant(f, aClickableAncestor, &labelTargetId)) {
      PET_LOG("  candidate %p is not a descendant of required ancestor\n", f);
      continue;
    }

    nsIContent* clickableContent =
        GetClickableAncestor(f, nsGkAtoms::body, &labelTargetId);
    if (!aClickableAncestor && !clickableContent) {
      PET_LOG("  candidate %p was not clickable\n", f);
      continue;
    }
    // If our current closest frame is a descendant of 'f', skip 'f' (prefer
    // the nested frame).
    if (bestTarget &&
        nsLayoutUtils::IsProperAncestorFrameCrossDoc(f, bestTarget, aRoot)) {
      PET_LOG("  candidate %p was ancestor for bestTarget %p\n", f, bestTarget);
      continue;
    }
    if (!aClickableAncestor && !nsLayoutUtils::IsAncestorFrameCrossDoc(
                                   aRestrictToDescendants, f, aRoot)) {
      PET_LOG("  candidate %p was not descendant of restrictroot %p\n", f,
              aRestrictToDescendants);
      continue;
    }

    // distance is in appunits
    float distance =
        ComputeDistanceFromRegion(aPointRelativeToRootFrame, region);
    nsIContent* content = f->GetContent();
    if (content && content->IsElement() &&
        content->AsElement()->State().HasState(
            EventStates(NS_EVENT_STATE_VISITED))) {
      distance *= aPrefs->mVisitedWeight / 100.0f;
    }
    if (distance < bestDistance) {
      PET_LOG("  candidate %p is the new best\n", f);
      bestDistance = distance;
      bestTarget = f;
    }
  }
  return bestTarget;
}

nsIFrame* FindFrameTargetedByInputEvent(
    WidgetGUIEvent* aEvent, RelativeTo aRootFrame,
    const nsPoint& aPointRelativeToRootFrame, uint32_t aFlags) {
  using FrameForPointOption = nsLayoutUtils::FrameForPointOption;
  EnumSet<FrameForPointOption> options;
  if (aFlags & INPUT_IGNORE_ROOT_SCROLL_FRAME) {
    options += FrameForPointOption::IgnoreRootScrollFrame;
  }
  nsIFrame* target = nsLayoutUtils::GetFrameForPoint(
      aRootFrame, aPointRelativeToRootFrame, options);
  PET_LOG(
      "Found initial target %p for event class %s message %s point %s "
      "relative to root frame %p\n",
      target, ToChar(aEvent->mClass), ToChar(aEvent->mMessage),
      mozilla::layers::Stringify(aPointRelativeToRootFrame).c_str(),
      aRootFrame.mFrame);

  const EventRadiusPrefs* prefs = GetPrefsFor(aEvent->mClass);
  if (!prefs || !prefs->mEnabled || EventRetargetSuppression::IsActive()) {
    PET_LOG("Retargeting disabled\n");
    return target;
  }
  nsIContent* clickableAncestor = nullptr;
  if (target) {
    clickableAncestor = GetClickableAncestor(target, nsGkAtoms::body);
    if (clickableAncestor) {
      PET_LOG("Target %p is clickable\n", target);
      // If the target that was directly hit has a clickable ancestor, that
      // means it too is clickable. And since it is the same as or a descendant
      // of clickableAncestor, it should become the root for the GetClosest
      // search.
      clickableAncestor = target->GetContent();
    }
  }

  // Do not modify targeting for actual mouse hardware; only for mouse
  // events generated by touch-screen hardware.
  if (aEvent->mClass == eMouseEventClass && prefs->mTouchOnly &&
      aEvent->AsMouseEvent()->mInputSource !=
          MouseEvent_Binding::MOZ_SOURCE_TOUCH) {
    PET_LOG("Mouse input event is not from a touch source\n");
    return target;
  }

  // If the exact target is non-null, only consider candidate targets in the
  // same document as the exact target. Otherwise, if an ancestor document has
  // a mouse event handler for example, targets that are !GetClickableAncestor
  // can never be targeted --- something nsSubDocumentFrame in an ancestor
  // document would be targeted instead.
  const nsIFrame* restrictToDescendants =
      target ? target->PresShell()->GetRootFrame() : aRootFrame.mFrame;

  nsRect targetRect = GetTargetRect(aRootFrame, aPointRelativeToRootFrame,
                                    restrictToDescendants, prefs, aFlags);
  PET_LOG("Expanded point to target rect %s\n",
          mozilla::layers::Stringify(targetRect).c_str());
  AutoTArray<nsIFrame*, 8> candidates;
  nsresult rv = nsLayoutUtils::GetFramesForArea(aRootFrame, targetRect,
                                                candidates, options);
  if (NS_FAILED(rv)) {
    return target;
  }

  nsIFrame* closestClickable =
      GetClosest(aRootFrame.mFrame, aPointRelativeToRootFrame, targetRect,
                 prefs, restrictToDescendants, clickableAncestor, candidates);
  if (closestClickable) {
    target = closestClickable;
  }
  PET_LOG("Final target is %p\n", target);

#ifdef DEBUG_FRAME_DUMP
  // At verbose logging level, dump the frame tree to help with debugging.
  // Note that dumping the frame tree at the top of the function may flood
  // logcat on Android devices and cause the PET_LOGs to get dropped.
  if (MOZ_LOG_TEST(sEvtTgtLog, LogLevel::Verbose)) {
    aRootFrame.mFrame->DumpFrameTree();
  }
#endif

  if (!target || !prefs->mRepositionEventCoords) {
    // No repositioning required for this event
    return target;
  }

  // Take the point relative to the root frame, make it relative to the target,
  // clamp it to the bounds, and then make it relative to the root frame again.
  nsPoint point = aPointRelativeToRootFrame;
  if (nsLayoutUtils::TRANSFORM_SUCCEEDED !=
      nsLayoutUtils::TransformPoint(aRootFrame, RelativeTo{target}, point)) {
    return target;
  }
  point = target->GetRectRelativeToSelf().ClampPoint(point);
  if (nsLayoutUtils::TRANSFORM_SUCCEEDED !=
      nsLayoutUtils::TransformPoint(RelativeTo{target}, aRootFrame, point)) {
    return target;
  }
  // Now we basically undo the operations in GetEventCoordinatesRelativeTo, to
  // get back the (now-clamped) coordinates in the event's widget's space.
  nsView* view = aRootFrame.mFrame->GetView();
  if (!view) {
    return target;
  }
  LayoutDeviceIntPoint widgetPoint = nsLayoutUtils::TranslateViewToWidget(
      aRootFrame.mFrame->PresContext(), view, point, aRootFrame.mViewportType,
      aEvent->mWidget);
  if (widgetPoint.x != NS_UNCONSTRAINEDSIZE) {
    // If that succeeded, we update the point in the event
    aEvent->mRefPoint = widgetPoint;
  }
  return target;
}

uint32_t EventRetargetSuppression::sSuppressionCount = 0;

EventRetargetSuppression::EventRetargetSuppression() { sSuppressionCount++; }

EventRetargetSuppression::~EventRetargetSuppression() { sSuppressionCount--; }

bool EventRetargetSuppression::IsActive() { return sSuppressionCount > 0; }

}  // namespace mozilla
